/*

    bttv - Bt848 frame grabber driver

    Copyright (C) 1996,97,98 Ralph  Metzler <rjkm@thp.uni-koeln.de>
			   & Marcus Metzler <mocm@thp.uni-koeln.de>
    (c) 1999-2002 Gerd Knorr <kraxel@bytesex.org>

    some v4l2 code lines are taken from Justin's bttv2 driver which is
    (c) 2000 Justin Schoeman <justin@suntiger.ee.up.ac.za>

    V4L1 removal from:
    (c) 2005-2006 Nickolay V. Shmyrev <nshmyrev@yandex.ru>

    Fixes to be fully V4L2 compliant by
    (c) 2006 Mauro Carvalho Chehab <mchehab@infradead.org>

    Cropping and overscan support
    Copyright (C) 2005, 2006 Michael H. Schimek <mschimek@gmx.at>
    Sponsored by OPQ Systems AB

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/smp_lock.h>
#include <linux/interrupt.h>
#include <linux/kdev_t.h>
#include "bttvp.h"
#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>
#include <media/tvaudio.h>
#include <media/msp3400.h>

#include <linux/dma-mapping.h>

#include <asm/io.h>
#include <asm/byteorder.h>

#include <media/rds.h>


unsigned int bttv_num;			/* number of Bt848s in use */
struct bttv *bttvs[BTTV_MAX];

unsigned int bttv_debug;
unsigned int bttv_verbose = 1;
unsigned int bttv_gpio;

/* config variables */
#ifdef __BIG_ENDIAN
static unsigned int bigendian=1;
#else
static unsigned int bigendian;
#endif
static unsigned int radio[BTTV_MAX];
static unsigned int irq_debug;
static unsigned int gbuffers = 8;
static unsigned int gbufsize = 0x208000;
static unsigned int reset_crop = 1;

static int video_nr[BTTV_MAX] = { [0 ... (BTTV_MAX-1)] = -1 };
static int radio_nr[BTTV_MAX] = { [0 ... (BTTV_MAX-1)] = -1 };
static int vbi_nr[BTTV_MAX] = { [0 ... (BTTV_MAX-1)] = -1 };
static int debug_latency;

static unsigned int fdsr;

/* options */
static unsigned int combfilter;
static unsigned int lumafilter;
static unsigned int automute    = 1;
static unsigned int chroma_agc;
static unsigned int adc_crush   = 1;
static unsigned int whitecrush_upper = 0xCF;
static unsigned int whitecrush_lower = 0x7F;
static unsigned int vcr_hack;
static unsigned int irq_iswitch;
static unsigned int uv_ratio    = 50;
static unsigned int full_luma_range;
static unsigned int coring;

/* API features (turn on/off stuff for testing) */
static unsigned int v4l2        = 1;

/* insmod args */
module_param(bttv_verbose,      int, 0644);
module_param(bttv_gpio,         int, 0644);
module_param(bttv_debug,        int, 0644);
module_param(irq_debug,         int, 0644);
module_param(debug_latency,     int, 0644);

module_param(fdsr,              int, 0444);
module_param(gbuffers,          int, 0444);
module_param(gbufsize,          int, 0444);
module_param(reset_crop,        int, 0444);

module_param(v4l2,              int, 0644);
module_param(bigendian,         int, 0644);
module_param(irq_iswitch,       int, 0644);
module_param(combfilter,        int, 0444);
module_param(lumafilter,        int, 0444);
module_param(automute,          int, 0444);
module_param(chroma_agc,        int, 0444);
module_param(adc_crush,         int, 0444);
module_param(whitecrush_upper,  int, 0444);
module_param(whitecrush_lower,  int, 0444);
module_param(vcr_hack,          int, 0444);
module_param(uv_ratio,          int, 0444);
module_param(full_luma_range,   int, 0444);
module_param(coring,            int, 0444);

module_param_array(radio,       int, NULL, 0444);
module_param_array(video_nr,    int, NULL, 0444);
module_param_array(radio_nr,    int, NULL, 0444);
module_param_array(vbi_nr,      int, NULL, 0444);

MODULE_PARM_DESC(radio,"The TV card supports radio, default is 0 (no)");
MODULE_PARM_DESC(bigendian,"byte order of the framebuffer, default is native endian");
MODULE_PARM_DESC(bttv_verbose,"verbose startup messages, default is 1 (yes)");
MODULE_PARM_DESC(bttv_gpio,"log gpio changes, default is 0 (no)");
MODULE_PARM_DESC(bttv_debug,"debug messages, default is 0 (no)");
MODULE_PARM_DESC(irq_debug,"irq handler debug messages, default is 0 (no)");
MODULE_PARM_DESC(gbuffers,"number of capture buffers. range 2-32, default 8");
MODULE_PARM_DESC(gbufsize,"size of the capture buffers, default is 0x208000");
MODULE_PARM_DESC(reset_crop,"reset cropping parameters at open(), default "
		 "is 1 (yes) for compatibility with older applications");
MODULE_PARM_DESC(automute,"mute audio on bad/missing video signal, default is 1 (yes)");
MODULE_PARM_DESC(chroma_agc,"enables the AGC of chroma signal, default is 0 (no)");
MODULE_PARM_DESC(adc_crush,"enables the luminance ADC crush, default is 1 (yes)");
MODULE_PARM_DESC(whitecrush_upper,"sets the white crush upper value, default is 207");
MODULE_PARM_DESC(whitecrush_lower,"sets the white crush lower value, default is 127");
MODULE_PARM_DESC(vcr_hack,"enables the VCR hack (improves synch on poor VCR tapes), default is 0 (no)");
MODULE_PARM_DESC(irq_iswitch,"switch inputs in irq handler");
MODULE_PARM_DESC(uv_ratio,"ratio between u and v gains, default is 50");
MODULE_PARM_DESC(full_luma_range,"use the full luma range, default is 0 (no)");
MODULE_PARM_DESC(coring,"set the luma coring level, default is 0 (no)");
MODULE_PARM_DESC(video_nr, "video device numbers");
MODULE_PARM_DESC(vbi_nr, "vbi device numbers");
MODULE_PARM_DESC(radio_nr, "radio device numbers");

MODULE_DESCRIPTION("bttv - v4l/v4l2 driver module for bt848/878 based cards");
MODULE_AUTHOR("Ralph Metzler & Marcus Metzler & Gerd Knorr");
MODULE_LICENSE("GPL");

/* ----------------------------------------------------------------------- */
/* sysfs                                                                   */

static ssize_t show_card(struct device *cd,
			 struct device_attribute *attr, char *buf)
{
	struct video_device *vfd = container_of(cd, struct video_device, dev);
	struct bttv *btv = video_get_drvdata(vfd);
	return sprintf(buf, "%d\n", btv ? btv->c.type : UNSET);
}
static DEVICE_ATTR(card, S_IRUGO, show_card, NULL);

/* ----------------------------------------------------------------------- */
/* dvb auto-load setup                                                     */
#if defined(CONFIG_MODULES) && defined(MODULE)
static void request_module_async(struct work_struct *work)
{
	request_module("dvb-bt8xx");
}

static void request_modules(struct bttv *dev)
{
	INIT_WORK(&dev->request_module_wk, request_module_async);
	schedule_work(&dev->request_module_wk);
}
#else
#define request_modules(dev)
#endif /* CONFIG_MODULES */


/* ----------------------------------------------------------------------- */
/* static data                                                             */

/* special timing tables from conexant... */
static u8 SRAM_Table[][60] =
{
	/* PAL digital input over GPIO[7:0] */
	{
		45, // 45 bytes following
		0x36,0x11,0x01,0x00,0x90,0x02,0x05,0x10,0x04,0x16,
		0x12,0x05,0x11,0x00,0x04,0x12,0xC0,0x00,0x31,0x00,
		0x06,0x51,0x08,0x03,0x89,0x08,0x07,0xC0,0x44,0x00,
		0x81,0x01,0x01,0xA9,0x0D,0x02,0x02,0x50,0x03,0x37,
		0x37,0x00,0xAF,0x21,0x00
	},
	/* NTSC digital input over GPIO[7:0] */
	{
		51, // 51 bytes following
		0x0C,0xC0,0x00,0x00,0x90,0x02,0x03,0x10,0x03,0x06,
		0x10,0x04,0x12,0x12,0x05,0x02,0x13,0x04,0x19,0x00,
		0x04,0x39,0x00,0x06,0x59,0x08,0x03,0x83,0x08,0x07,
		0x03,0x50,0x00,0xC0,0x40,0x00,0x86,0x01,0x01,0xA6,
		0x0D,0x02,0x03,0x11,0x01,0x05,0x37,0x00,0xAC,0x21,
		0x00,
	},
	// TGB_NTSC392 // quartzsight
	// This table has been modified to be used for Fusion Rev D
	{
		0x2A, // size of table = 42
		0x06, 0x08, 0x04, 0x0a, 0xc0, 0x00, 0x18, 0x08, 0x03, 0x24,
		0x08, 0x07, 0x02, 0x90, 0x02, 0x08, 0x10, 0x04, 0x0c, 0x10,
		0x05, 0x2c, 0x11, 0x04, 0x55, 0x48, 0x00, 0x05, 0x50, 0x00,
		0xbf, 0x0c, 0x02, 0x2f, 0x3d, 0x00, 0x2f, 0x3f, 0x00, 0xc3,
		0x20, 0x00
	}
};

/* minhdelayx1	first video pixel we can capture on a line and
   hdelayx1	start of active video, both relative to rising edge of
		/HRESET pulse (0H) in 1 / fCLKx1.
   swidth	width of active video and
   totalwidth	total line width, both in 1 / fCLKx1.
   sqwidth	total line width in square pixels.
   vdelay	start of active video in 2 * field lines relative to
		trailing edge of /VRESET pulse (VDELAY register).
   sheight	height of active video in 2 * field lines.
   videostart0	ITU-R frame line number of the line corresponding
		to vdelay in the first field. */
#define CROPCAP(minhdelayx1, hdelayx1, swidth, totalwidth, sqwidth,	 \
		vdelay, sheight, videostart0)				 \
	.cropcap.bounds.left = minhdelayx1,				 \
	/* * 2 because vertically we count field lines times two, */	 \
	/* e.g. 23 * 2 to 23 * 2 + 576 in PAL-BGHI defrect. */		 \
	.cropcap.bounds.top = (videostart0) * 2 - (vdelay) + MIN_VDELAY, \
	/* 4 is a safety margin at the end of the line. */		 \
	.cropcap.bounds.width = (totalwidth) - (minhdelayx1) - 4,	 \
	.cropcap.bounds.height = (sheight) + (vdelay) - MIN_VDELAY,	 \
	.cropcap.defrect.left = hdelayx1,				 \
	.cropcap.defrect.top = (videostart0) * 2,			 \
	.cropcap.defrect.width = swidth,				 \
	.cropcap.defrect.height = sheight,				 \
	.cropcap.pixelaspect.numerator = totalwidth,			 \
	.cropcap.pixelaspect.denominator = sqwidth,

const struct bttv_tvnorm bttv_tvnorms[] = {
	/* PAL-BDGHI */
	/* max. active video is actually 922, but 924 is divisible by 4 and 3! */
	/* actually, max active PAL with HSCALE=0 is 948, NTSC is 768 - nil */
	{
		.v4l2_id        = V4L2_STD_PAL,
		.name           = "PAL",
		.Fsc            = 35468950,
		.swidth         = 924,
		.sheight        = 576,
		.totalwidth     = 1135,
		.adelay         = 0x7f,
		.bdelay         = 0x72,
		.iform          = (BT848_IFORM_PAL_BDGHI|BT848_IFORM_XT1),
		.scaledtwidth   = 1135,
		.hdelayx1       = 186,
		.hactivex1      = 924,
		.vdelay         = 0x20,
		.vbipack        = 255, /* min (2048 / 4, 0x1ff) & 0xff */
		.sram           = 0,
		/* ITU-R frame line number of the first VBI line
		   we can capture, of the first and second field.
		   The last line is determined by cropcap.bounds. */
		.vbistart       = { 7, 320 },
		CROPCAP(/* minhdelayx1 */ 68,
			/* hdelayx1 */ 186,
			/* Should be (768 * 1135 + 944 / 2) / 944.
			   cropcap.defrect is used for image width
			   checks, so we keep the old value 924. */
			/* swidth */ 924,
			/* totalwidth */ 1135,
			/* sqwidth */ 944,
			/* vdelay */ 0x20,
			/* sheight */ 576,
			/* videostart0 */ 23)
		/* bt878 (and bt848?) can capture another
		   line below active video. */
		.cropcap.bounds.height = (576 + 2) + 0x20 - 2,
	},{
		.v4l2_id        = V4L2_STD_NTSC_M | V4L2_STD_NTSC_M_KR,
		.name           = "NTSC",
		.Fsc            = 28636363,
		.swidth         = 768,
		.sheight        = 480,
		.totalwidth     = 910,
		.adelay         = 0x68,
		.bdelay         = 0x5d,
		.iform          = (BT848_IFORM_NTSC|BT848_IFORM_XT0),
		.scaledtwidth   = 910,
		.hdelayx1       = 128,
		.hactivex1      = 910,
		.vdelay         = 0x1a,
		.vbipack        = 144, /* min (1600 / 4, 0x1ff) & 0xff */
		.sram           = 1,
		.vbistart	= { 10, 273 },
		CROPCAP(/* minhdelayx1 */ 68,
			/* hdelayx1 */ 128,
			/* Should be (640 * 910 + 780 / 2) / 780? */
			/* swidth */ 768,
			/* totalwidth */ 910,
			/* sqwidth */ 780,
			/* vdelay */ 0x1a,
			/* sheight */ 480,
			/* videostart0 */ 23)
	},{
		.v4l2_id        = V4L2_STD_SECAM,
		.name           = "SECAM",
		.Fsc            = 35468950,
		.swidth         = 924,
		.sheight        = 576,
		.totalwidth     = 1135,
		.adelay         = 0x7f,
		.bdelay         = 0xb0,
		.iform          = (BT848_IFORM_SECAM|BT848_IFORM_XT1),
		.scaledtwidth   = 1135,
		.hdelayx1       = 186,
		.hactivex1      = 922,
		.vdelay         = 0x20,
		.vbipack        = 255,
		.sram           = 0, /* like PAL, correct? */
		.vbistart	= { 7, 320 },
		CROPCAP(/* minhdelayx1 */ 68,
			/* hdelayx1 */ 186,
			/* swidth */ 924,
			/* totalwidth */ 1135,
			/* sqwidth */ 944,
			/* vdelay */ 0x20,
			/* sheight */ 576,
			/* videostart0 */ 23)
	},{
		.v4l2_id        = V4L2_STD_PAL_Nc,
		.name           = "PAL-Nc",
		.Fsc            = 28636363,
		.swidth         = 640,
		.sheight        = 576,
		.totalwidth     = 910,
		.adelay         = 0x68,
		.bdelay         = 0x5d,
		.iform          = (BT848_IFORM_PAL_NC|BT848_IFORM_XT0),
		.scaledtwidth   = 780,
		.hdelayx1       = 130,
		.hactivex1      = 734,
		.vdelay         = 0x1a,
		.vbipack        = 144,
		.sram           = -1,
		.vbistart	= { 7, 320 },
		CROPCAP(/* minhdelayx1 */ 68,
			/* hdelayx1 */ 130,
			/* swidth */ (640 * 910 + 780 / 2) / 780,
			/* totalwidth */ 910,
			/* sqwidth */ 780,
			/* vdelay */ 0x1a,
			/* sheight */ 576,
			/* videostart0 */ 23)
	},{
		.v4l2_id        = V4L2_STD_PAL_M,
		.name           = "PAL-M",
		.Fsc            = 28636363,
		.swidth         = 640,
		.sheight        = 480,
		.totalwidth     = 910,
		.adelay         = 0x68,
		.bdelay         = 0x5d,
		.iform          = (BT848_IFORM_PAL_M|BT848_IFORM_XT0),
		.scaledtwidth   = 780,
		.hdelayx1       = 135,
		.hactivex1      = 754,
		.vdelay         = 0x1a,
		.vbipack        = 144,
		.sram           = -1,
		.vbistart	= { 10, 273 },
		CROPCAP(/* minhdelayx1 */ 68,
			/* hdelayx1 */ 135,
			/* swidth */ (640 * 910 + 780 / 2) / 780,
			/* totalwidth */ 910,
			/* sqwidth */ 780,
			/* vdelay */ 0x1a,
			/* sheight */ 480,
			/* videostart0 */ 23)
	},{
		.v4l2_id        = V4L2_STD_PAL_N,
		.name           = "PAL-N",
		.Fsc            = 35468950,
		.swidth         = 768,
		.sheight        = 576,
		.totalwidth     = 1135,
		.adelay         = 0x7f,
		.bdelay         = 0x72,
		.iform          = (BT848_IFORM_PAL_N|BT848_IFORM_XT1),
		.scaledtwidth   = 944,
		.hdelayx1       = 186,
		.hactivex1      = 922,
		.vdelay         = 0x20,
		.vbipack        = 144,
		.sram           = -1,
		.vbistart       = { 7, 320 },
		CROPCAP(/* minhdelayx1 */ 68,
			/* hdelayx1 */ 186,
			/* swidth */ (768 * 1135 + 944 / 2) / 944,
			/* totalwidth */ 1135,
			/* sqwidth */ 944,
			/* vdelay */ 0x20,
			/* sheight */ 576,
			/* videostart0 */ 23)
	},{
		.v4l2_id        = V4L2_STD_NTSC_M_JP,
		.name           = "NTSC-JP",
		.Fsc            = 28636363,
		.swidth         = 640,
		.sheight        = 480,
		.totalwidth     = 910,
		.adelay         = 0x68,
		.bdelay         = 0x5d,
		.iform          = (BT848_IFORM_NTSC_J|BT848_IFORM_XT0),
		.scaledtwidth   = 780,
		.hdelayx1       = 135,
		.hactivex1      = 754,
		.vdelay         = 0x16,
		.vbipack        = 144,
		.sram           = -1,
		.vbistart       = { 10, 273 },
		CROPCAP(/* minhdelayx1 */ 68,
			/* hdelayx1 */ 135,
			/* swidth */ (640 * 910 + 780 / 2) / 780,
			/* totalwidth */ 910,
			/* sqwidth */ 780,
			/* vdelay */ 0x16,
			/* sheight */ 480,
			/* videostart0 */ 23)
	},{
		/* that one hopefully works with the strange timing
		 * which video recorders produce when playing a NTSC
		 * tape on a PAL TV ... */
		.v4l2_id        = V4L2_STD_PAL_60,
		.name           = "PAL-60",
		.Fsc            = 35468950,
		.swidth         = 924,
		.sheight        = 480,
		.totalwidth     = 1135,
		.adelay         = 0x7f,
		.bdelay         = 0x72,
		.iform          = (BT848_IFORM_PAL_BDGHI|BT848_IFORM_XT1),
		.scaledtwidth   = 1135,
		.hdelayx1       = 186,
		.hactivex1      = 924,
		.vdelay         = 0x1a,
		.vbipack        = 255,
		.vtotal         = 524,
		.sram           = -1,
		.vbistart	= { 10, 273 },
		CROPCAP(/* minhdelayx1 */ 68,
			/* hdelayx1 */ 186,
			/* swidth */ 924,
			/* totalwidth */ 1135,
			/* sqwidth */ 944,
			/* vdelay */ 0x1a,
			/* sheight */ 480,
			/* videostart0 */ 23)
	}
};
static const unsigned int BTTV_TVNORMS = ARRAY_SIZE(bttv_tvnorms);

/* ----------------------------------------------------------------------- */
/* bttv format list
   packed pixel formats must come first */
static const struct bttv_format formats[] = {
	{
		.name     = "8 bpp, gray",
		.fourcc   = V4L2_PIX_FMT_GREY,
		.btformat = BT848_COLOR_FMT_Y8,
		.depth    = 8,
		.flags    = FORMAT_FLAGS_PACKED,
	},{
		.name     = "8 bpp, dithered color",
		.fourcc   = V4L2_PIX_FMT_HI240,
		.btformat = BT848_COLOR_FMT_RGB8,
		.depth    = 8,
		.flags    = FORMAT_FLAGS_PACKED | FORMAT_FLAGS_DITHER,
	},{
		.name     = "15 bpp RGB, le",
		.fourcc   = V4L2_PIX_FMT_RGB555,
		.btformat = BT848_COLOR_FMT_RGB15,
		.depth    = 16,
		.flags    = FORMAT_FLAGS_PACKED,
	},{
		.name     = "15 bpp RGB, be",
		.fourcc   = V4L2_PIX_FMT_RGB555X,
		.btformat = BT848_COLOR_FMT_RGB15,
		.btswap   = 0x03, /* byteswap */
		.depth    = 16,
		.flags    = FORMAT_FLAGS_PACKED,
	},{
		.name     = "16 bpp RGB, le",
		.fourcc   = V4L2_PIX_FMT_RGB565,
		.btformat = BT848_COLOR_FMT_RGB16,
		.depth    = 16,
		.flags    = FORMAT_FLAGS_PACKED,
	},{
		.name     = "16 bpp RGB, be",
		.fourcc   = V4L2_PIX_FMT_RGB565X,
		.btformat = BT848_COLOR_FMT_RGB16,
		.btswap   = 0x03, /* byteswap */
		.depth    = 16,
		.flags    = FORMAT_FLAGS_PACKED,
	},{
		.name     = "24 bpp RGB, le",
		.fourcc   = V4L2_PIX_FMT_BGR24,
		.btformat = BT848_COLOR_FMT_RGB24,
		.depth    = 24,
		.flags    = FORMAT_FLAGS_PACKED,
	},{
		.name     = "32 bpp RGB, le",
		.fourcc   = V4L2_PIX_FMT_BGR32,
		.btformat = BT848_COLOR_FMT_RGB32,
		.depth    = 32,
		.flags    = FORMAT_FLAGS_PACKED,
	},{
		.name     = "32 bpp RGB, be",
		.fourcc   = V4L2_PIX_FMT_RGB32,
		.btformat = BT848_COLOR_FMT_RGB32,
		.btswap   = 0x0f, /* byte+word swap */
		.depth    = 32,
		.flags    = FORMAT_FLAGS_PACKED,
	},{
		.name     = "4:2:2, packed, YUYV",
		.fourcc   = V4L2_PIX_FMT_YUYV,
		.btformat = BT848_COLOR_FMT_YUY2,
		.depth    = 16,
		.flags    = FORMAT_FLAGS_PACKED,
	},{
		.name     = "4:2:2, packed, YUYV",
		.fourcc   = V4L2_PIX_FMT_YUYV,
		.btformat = BT848_COLOR_FMT_YUY2,
		.depth    = 16,
		.flags    = FORMAT_FLAGS_PACKED,
	},{
		.name     = "4:2:2, packed, UYVY",
		.fourcc   = V4L2_PIX_FMT_UYVY,
		.btformat = BT848_COLOR_FMT_YUY2,
		.btswap   = 0x03, /* byteswap */
		.depth    = 16,
		.flags    = FORMAT_FLAGS_PACKED,
	},{
		.name     = "4:2:2, planar, Y-Cb-Cr",
		.fourcc   = V4L2_PIX_FMT_YUV422P,
		.btformat = BT848_COLOR_FMT_YCrCb422,
		.depth    = 16,
		.flags    = FORMAT_FLAGS_PLANAR,
		.hshift   = 1,
		.vshift   = 0,
	},{
		.name     = "4:2:0, planar, Y-Cb-Cr",
		.fourcc   = V4L2_PIX_FMT_YUV420,
		.btformat = BT848_COLOR_FMT_YCrCb422,
		.depth    = 12,
		.flags    = FORMAT_FLAGS_PLANAR,
		.hshift   = 1,
		.vshift   = 1,
	},{
		.name     = "4:2:0, planar, Y-Cr-Cb",
		.fourcc   = V4L2_PIX_FMT_YVU420,
		.btformat = BT848_COLOR_FMT_YCrCb422,
		.depth    = 12,
		.flags    = FORMAT_FLAGS_PLANAR | FORMAT_FLAGS_CrCb,
		.hshift   = 1,
		.vshift   = 1,
	},{
		.name     = "4:1:1, planar, Y-Cb-Cr",
		.fourcc   = V4L2_PIX_FMT_YUV411P,
		.btformat = BT848_COLOR_FMT_YCrCb411,
		.depth    = 12,
		.flags    = FORMAT_FLAGS_PLANAR,
		.hshift   = 2,
		.vshift   = 0,
	},{
		.name     = "4:1:0, planar, Y-Cb-Cr",
		.fourcc   = V4L2_PIX_FMT_YUV410,
		.btformat = BT848_COLOR_FMT_YCrCb411,
		.depth    = 9,
		.flags    = FORMAT_FLAGS_PLANAR,
		.hshift   = 2,
		.vshift   = 2,
	},{
		.name     = "4:1:0, planar, Y-Cr-Cb",
		.fourcc   = V4L2_PIX_FMT_YVU410,
		.btformat = BT848_COLOR_FMT_YCrCb411,
		.depth    = 9,
		.flags    = FORMAT_FLAGS_PLANAR | FORMAT_FLAGS_CrCb,
		.hshift   = 2,
		.vshift   = 2,
	},{
		.name     = "raw scanlines",
		.fourcc   = -1,
		.btformat = BT848_COLOR_FMT_RAW,
		.depth    = 8,
		.flags    = FORMAT_FLAGS_RAW,
	}
};
static const unsigned int FORMATS = ARRAY_SIZE(formats);

/* ----------------------------------------------------------------------- */

#define V4L2_CID_PRIVATE_CHROMA_AGC  (V4L2_CID_PRIVATE_BASE + 0)
#define V4L2_CID_PRIVATE_COMBFILTER  (V4L2_CID_PRIVATE_BASE + 1)
#define V4L2_CID_PRIVATE_AUTOMUTE    (V4L2_CID_PRIVATE_BASE + 2)
#define V4L2_CID_PRIVATE_LUMAFILTER  (V4L2_CID_PRIVATE_BASE + 3)
#define V4L2_CID_PRIVATE_AGC_CRUSH   (V4L2_CID_PRIVATE_BASE + 4)
#define V4L2_CID_PRIVATE_VCR_HACK    (V4L2_CID_PRIVATE_BASE + 5)
#define V4L2_CID_PRIVATE_WHITECRUSH_UPPER   (V4L2_CID_PRIVATE_BASE + 6)
#define V4L2_CID_PRIVATE_WHITECRUSH_LOWER   (V4L2_CID_PRIVATE_BASE + 7)
#define V4L2_CID_PRIVATE_UV_RATIO    (V4L2_CID_PRIVATE_BASE + 8)
#define V4L2_CID_PRIVATE_FULL_LUMA_RANGE    (V4L2_CID_PRIVATE_BASE + 9)
#define V4L2_CID_PRIVATE_CORING      (V4L2_CID_PRIVATE_BASE + 10)
#define V4L2_CID_PRIVATE_LASTP1      (V4L2_CID_PRIVATE_BASE + 11)

static const struct v4l2_queryctrl no_ctl = {
	.name  = "42",
	.flags = V4L2_CTRL_FLAG_DISABLED,
};
static const struct v4l2_queryctrl bttv_ctls[] = {
	/* --- video --- */
	{
		.id            = V4L2_CID_BRIGHTNESS,
		.name          = "Brightness",
		.minimum       = 0,
		.maximum       = 65535,
		.step          = 256,
		.default_value = 32768,
		.type          = V4L2_CTRL_TYPE_INTEGER,
	},{
		.id            = V4L2_CID_CONTRAST,
		.name          = "Contrast",
		.minimum       = 0,
		.maximum       = 65535,
		.step          = 128,
		.default_value = 32768,
		.type          = V4L2_CTRL_TYPE_INTEGER,
	},{
		.id            = V4L2_CID_SATURATION,
		.name          = "Saturation",
		.minimum       = 0,
		.maximum       = 65535,
		.step          = 128,
		.default_value = 32768,
		.type          = V4L2_CTRL_TYPE_INTEGER,
	},{
		.id            = V4L2_CID_HUE,
		.name          = "Hue",
		.minimum       = 0,
		.maximum       = 65535,
		.step          = 256,
		.default_value = 32768,
		.type          = V4L2_CTRL_TYPE_INTEGER,
	},
	/* --- audio --- */
	{
		.id            = V4L2_CID_AUDIO_MUTE,
		.name          = "Mute",
		.minimum       = 0,
		.maximum       = 1,
		.type          = V4L2_CTRL_TYPE_BOOLEAN,
	},{
		.id            = V4L2_CID_AUDIO_VOLUME,
		.name          = "Volume",
		.minimum       = 0,
		.maximum       = 65535,
		.step          = 65535/100,
		.default_value = 65535,
		.type          = V4L2_CTRL_TYPE_INTEGER,
	},{
		.id            = V4L2_CID_AUDIO_BALANCE,
		.name          = "Balance",
		.minimum       = 0,
		.maximum       = 65535,
		.step          = 65535/100,
		.default_value = 32768,
		.type          = V4L2_CTRL_TYPE_INTEGER,
	},{
		.id            = V4L2_CID_AUDIO_BASS,
		.name          = "Bass",
		.minimum       = 0,
		.maximum       = 65535,
		.step          = 65535/100,
		.default_value = 32768,
		.type          = V4L2_CTRL_TYPE_INTEGER,
	},{
		.id            = V4L2_CID_AUDIO_TREBLE,
		.name          = "Treble",
		.minimum       = 0,
		.maximum       = 65535,
		.step          = 65535/100,
		.default_value = 32768,
		.type          = V4L2_CTRL_TYPE_INTEGER,
	},
	/* --- private --- */
	{
		.id            = V4L2_CID_PRIVATE_CHROMA_AGC,
		.name          = "chroma agc",
		.minimum       = 0,
		.maximum       = 1,
		.type          = V4L2_CTRL_TYPE_BOOLEAN,
	},{
		.id            = V4L2_CID_PRIVATE_COMBFILTER,
		.name          = "combfilter",
		.minimum       = 0,
		.maximum       = 1,
		.type          = V4L2_CTRL_TYPE_BOOLEAN,
	},{
		.id            = V4L2_CID_PRIVATE_AUTOMUTE,
		.name          = "automute",
		.minimum       = 0,
		.maximum       = 1,
		.type          = V4L2_CTRL_TYPE_BOOLEAN,
	},{
		.id            = V4L2_CID_PRIVATE_LUMAFILTER,
		.name          = "luma decimation filter",
		.minimum       = 0,
		.maximum       = 1,
		.type          = V4L2_CTRL_TYPE_BOOLEAN,
	},{
		.id            = V4L2_CID_PRIVATE_AGC_CRUSH,
		.name          = "agc crush",
		.minimum       = 0,
		.maximum       = 1,
		.type          = V4L2_CTRL_TYPE_BOOLEAN,
	},{
		.id            = V4L2_CID_PRIVATE_VCR_HACK,
		.name          = "vcr hack",
		.minimum       = 0,
		.maximum       = 1,
		.type          = V4L2_CTRL_TYPE_BOOLEAN,
	},{
		.id            = V4L2_CID_PRIVATE_WHITECRUSH_UPPER,
		.name          = "whitecrush upper",
		.minimum       = 0,
		.maximum       = 255,
		.step          = 1,
		.default_value = 0xCF,
		.type          = V4L2_CTRL_TYPE_INTEGER,
	},{
		.id            = V4L2_CID_PRIVATE_WHITECRUSH_LOWER,
		.name          = "whitecrush lower",
		.minimum       = 0,
		.maximum       = 255,
		.step          = 1,
		.default_value = 0x7F,
		.type          = V4L2_CTRL_TYPE_INTEGER,
	},{
		.id            = V4L2_CID_PRIVATE_UV_RATIO,
		.name          = "uv ratio",
		.minimum       = 0,
		.maximum       = 100,
		.step          = 1,
		.default_value = 50,
		.type          = V4L2_CTRL_TYPE_INTEGER,
	},{
		.id            = V4L2_CID_PRIVATE_FULL_LUMA_RANGE,
		.name          = "full luma range",
		.minimum       = 0,
		.maximum       = 1,
		.type          = V4L2_CTRL_TYPE_BOOLEAN,
	},{
		.id            = V4L2_CID_PRIVATE_CORING,
		.name          = "coring",
		.minimum       = 0,
		.maximum       = 3,
		.step          = 1,
		.default_value = 0,
		.type          = V4L2_CTRL_TYPE_INTEGER,
	}



};

static const struct v4l2_queryctrl *ctrl_by_id(int id)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(bttv_ctls); i++)
		if (bttv_ctls[i].id == id)
			return bttv_ctls+i;

	return NULL;
}

/* ----------------------------------------------------------------------- */
/* resource management                                                     */

/*
   RESOURCE_    allocated by                freed by

   VIDEO_READ   bttv_read 1)                bttv_read 2)

   VIDEO_STREAM VIDIOC_STREAMON             VIDIOC_STREAMOFF
		 VIDIOC_QBUF 1)              bttv_release
		 VIDIOCMCAPTURE 1)

   OVERLAY	 VIDIOCCAPTURE on            VIDIOCCAPTURE off
		 VIDIOC_OVERLAY on           VIDIOC_OVERLAY off
		 3)                          bttv_release

   VBI		 VIDIOC_STREAMON             VIDIOC_STREAMOFF
		 VIDIOC_QBUF 1)              bttv_release
		 bttv_read, bttv_poll 1) 4)

   1) The resource must be allocated when we enter buffer prepare functions
      and remain allocated while buffers are in the DMA queue.
   2) This is a single frame read.
   3) VIDIOC_S_FBUF and VIDIOC_S_FMT (OVERLAY) still work when
      RESOURCE_OVERLAY is allocated.
   4) This is a continuous read, implies VIDIOC_STREAMON.

   Note this driver permits video input and standard changes regardless if
   resources are allocated.
*/

#define VBI_RESOURCES (RESOURCE_VBI)
#define VIDEO_RESOURCES (RESOURCE_VIDEO_READ | \
			 RESOURCE_VIDEO_STREAM | \
			 RESOURCE_OVERLAY)

static
int check_alloc_btres(struct bttv *btv, struct bttv_fh *fh, int bit)
{
	int xbits; /* mutual exclusive resources */

	if (fh->resources & bit)
		/* have it already allocated */
		return 1;

	xbits = bit;
	if (bit & (RESOURCE_VIDEO_READ | RESOURCE_VIDEO_STREAM))
		xbits |= RESOURCE_VIDEO_READ | RESOURCE_VIDEO_STREAM;

	/* is it free? */
	mutex_lock(&btv->lock);
	if (btv->resources & xbits) {
		/* no, someone else uses it */
		goto fail;
	}

	if ((bit & VIDEO_RESOURCES)
	    && 0 == (btv->resources & VIDEO_RESOURCES)) {
		/* Do crop - use current, don't - use default parameters. */
		__s32 top = btv->crop[!!fh->do_crop].rect.top;

		if (btv->vbi_end > top)
			goto fail;

		/* We cannot capture the same line as video and VBI data.
		   Claim scan lines crop[].rect.top to bottom. */
		btv->crop_start = top;
	} else if (bit & VBI_RESOURCES) {
		__s32 end = fh->vbi_fmt.end;

		if (end > btv->crop_start)
			goto fail;

		/* Claim scan lines above fh->vbi_fmt.end. */
		btv->vbi_end = end;
	}

	/* it's free, grab it */
	fh->resources  |= bit;
	btv->resources |= bit;
	mutex_unlock(&btv->lock);
	return 1;

 fail:
	mutex_unlock(&btv->lock);
	return 0;
}

static
int check_btres(struct bttv_fh *fh, int bit)
{
	return (fh->resources & bit);
}

static
int locked_btres(struct bttv *btv, int bit)
{
	return (btv->resources & bit);
}

/* Call with btv->lock down. */
static void
disclaim_vbi_lines(struct bttv *btv)
{
	btv->vbi_end = 0;
}

/* Call with btv->lock down. */
static void
disclaim_video_lines(struct bttv *btv)
{
	const struct bttv_tvnorm *tvnorm;
	u8 crop;

	tvnorm = &bttv_tvnorms[btv->tvnorm];
	btv->crop_start = tvnorm->cropcap.bounds.top
		+ tvnorm->cropcap.bounds.height;

	/* VBI capturing ends at VDELAY, start of video capturing, no
	   matter how many lines the VBI RISC program expects. When video
	   capturing is off, it shall no longer "preempt" VBI capturing,
	   so we set VDELAY to maximum. */
	crop = btread(BT848_E_CROP) | 0xc0;
	btwrite(crop, BT848_E_CROP);
	btwrite(0xfe, BT848_E_VDELAY_LO);
	btwrite(crop, BT848_O_CROP);
	btwrite(0xfe, BT848_O_VDELAY_LO);
}

static
void free_btres(struct bttv *btv, struct bttv_fh *fh, int bits)
{
	if ((fh->resources & bits) != bits) {
		/* trying to free ressources not allocated by us ... */
		printk("bttv: BUG! (btres)\n");
	}
	mutex_lock(&btv->lock);
	fh->resources  &= ~bits;
	btv->resources &= ~bits;

	bits = btv->resources;

	if (0 == (bits & VIDEO_RESOURCES))
		disclaim_video_lines(btv);

	if (0 == (bits & VBI_RESOURCES))
		disclaim_vbi_lines(btv);

	mutex_unlock(&btv->lock);
}

/* ----------------------------------------------------------------------- */
/* If Bt848a or Bt849, use PLL for PAL/SECAM and crystal for NTSC          */

/* Frequency = (F_input / PLL_X) * PLL_I.PLL_F/PLL_C
   PLL_X = Reference pre-divider (0=1, 1=2)
   PLL_C = Post divider (0=6, 1=4)
   PLL_I = Integer input
   PLL_F = Fractional input

   F_input = 28.636363 MHz:
   PAL (CLKx2 = 35.46895 MHz): PLL_X = 1, PLL_I = 0x0E, PLL_F = 0xDCF9, PLL_C = 0
*/

static void set_pll_freq(struct bttv *btv, unsigned int fin, unsigned int fout)
{
	unsigned char fl, fh, fi;

	/* prevent overflows */
	fin/=4;
	fout/=4;

	fout*=12;
	fi=fout/fin;

	fout=(fout%fin)*256;
	fh=fout/fin;

	fout=(fout%fin)*256;
	fl=fout/fin;

	btwrite(fl, BT848_PLL_F_LO);
	btwrite(fh, BT848_PLL_F_HI);
	btwrite(fi|BT848_PLL_X, BT848_PLL_XCI);
}

static void set_pll(struct bttv *btv)
{
	int i;

	if (!btv->pll.pll_crystal)
		return;

	if (btv->pll.pll_ofreq == btv->pll.pll_current) {
		dprintk("bttv%d: PLL: no change required\n",btv->c.nr);
		return;
	}

	if (btv->pll.pll_ifreq == btv->pll.pll_ofreq) {
		/* no PLL needed */
		if (btv->pll.pll_current == 0)
			return;
		bttv_printk(KERN_INFO "bttv%d: PLL can sleep, using XTAL (%d).\n",
			btv->c.nr,btv->pll.pll_ifreq);
		btwrite(0x00,BT848_TGCTRL);
		btwrite(0x00,BT848_PLL_XCI);
		btv->pll.pll_current = 0;
		return;
	}

	bttv_printk(KERN_INFO "bttv%d: PLL: %d => %d ",btv->c.nr,
		btv->pll.pll_ifreq, btv->pll.pll_ofreq);
	set_pll_freq(btv, btv->pll.pll_ifreq, btv->pll.pll_ofreq);

	for (i=0; i<10; i++) {
		/*  Let other people run while the PLL stabilizes */
		bttv_printk(".");
		msleep(10);

		if (btread(BT848_DSTATUS) & BT848_DSTATUS_PLOCK) {
			btwrite(0,BT848_DSTATUS);
		} else {
			btwrite(0x08,BT848_TGCTRL);
			btv->pll.pll_current = btv->pll.pll_ofreq;
			bttv_printk(" ok\n");
			return;
		}
	}
	btv->pll.pll_current = -1;
	bttv_printk("failed\n");
	return;
}

/* used to switch between the bt848's analog/digital video capture modes */
static void bt848A_set_timing(struct bttv *btv)
{
	int i, len;
	int table_idx = bttv_tvnorms[btv->tvnorm].sram;
	int fsc       = bttv_tvnorms[btv->tvnorm].Fsc;

	if (btv->input == btv->dig) {
		dprintk("bttv%d: load digital timing table (table_idx=%d)\n",
			btv->c.nr,table_idx);

		/* timing change...reset timing generator address */
		btwrite(0x00, BT848_TGCTRL);
		btwrite(0x02, BT848_TGCTRL);
		btwrite(0x00, BT848_TGCTRL);

		len=SRAM_Table[table_idx][0];
		for(i = 1; i <= len; i++)
			btwrite(SRAM_Table[table_idx][i],BT848_TGLB);
		btv->pll.pll_ofreq = 27000000;

		set_pll(btv);
		btwrite(0x11, BT848_TGCTRL);
		btwrite(0x41, BT848_DVSIF);
	} else {
		btv->pll.pll_ofreq = fsc;
		set_pll(btv);
		btwrite(0x0, BT848_DVSIF);
	}
}

/* ----------------------------------------------------------------------- */

static void bt848_bright(struct bttv *btv, int bright)
{
	int value;

	// printk("bttv: set bright: %d\n",bright); // DEBUG
	btv->bright = bright;

	/* We want -128 to 127 we get 0-65535 */
	value = (bright >> 8) - 128;
	btwrite(value & 0xff, BT848_BRIGHT);
}

static void bt848_hue(struct bttv *btv, int hue)
{
	int value;

	btv->hue = hue;

	/* -128 to 127 */
	value = (hue >> 8) - 128;
	btwrite(value & 0xff, BT848_HUE);
}

static void bt848_contrast(struct bttv *btv, int cont)
{
	int value,hibit;

	btv->contrast = cont;

	/* 0-511 */
	value = (cont  >> 7);
	hibit = (value >> 6) & 4;
	btwrite(value & 0xff, BT848_CONTRAST_LO);
	btaor(hibit, ~4, BT848_E_CONTROL);
	btaor(hibit, ~4, BT848_O_CONTROL);
}

static void bt848_sat(struct bttv *btv, int color)
{
	int val_u,val_v,hibits;

	btv->saturation = color;

	/* 0-511 for the color */
	val_u   = ((color * btv->opt_uv_ratio) / 50) >> 7;
	val_v   = (((color * (100 - btv->opt_uv_ratio) / 50) >>7)*180L)/254;
	hibits  = (val_u >> 7) & 2;
	hibits |= (val_v >> 8) & 1;
	btwrite(val_u & 0xff, BT848_SAT_U_LO);
	btwrite(val_v & 0xff, BT848_SAT_V_LO);
	btaor(hibits, ~3, BT848_E_CONTROL);
	btaor(hibits, ~3, BT848_O_CONTROL);
}

/* ----------------------------------------------------------------------- */

static int
video_mux(struct bttv *btv, unsigned int input)
{
	int mux,mask2;

	if (input >= bttv_tvcards[btv->c.type].video_inputs)
		return -EINVAL;

	/* needed by RemoteVideo MX */
	mask2 = bttv_tvcards[btv->c.type].gpiomask2;
	if (mask2)
		gpio_inout(mask2,mask2);

	if (input == btv->svhs)  {
		btor(BT848_CONTROL_COMP, BT848_E_CONTROL);
		btor(BT848_CONTROL_COMP, BT848_O_CONTROL);
	} else {
		btand(~BT848_CONTROL_COMP, BT848_E_CONTROL);
		btand(~BT848_CONTROL_COMP, BT848_O_CONTROL);
	}
	mux = bttv_muxsel(btv, input);
	btaor(mux<<5, ~(3<<5), BT848_IFORM);
	dprintk(KERN_DEBUG "bttv%d: video mux: input=%d mux=%d\n",
		btv->c.nr,input,mux);

	/* card specific hook */
	if(bttv_tvcards[btv->c.type].muxsel_hook)
		bttv_tvcards[btv->c.type].muxsel_hook (btv, input);
	return 0;
}

static char *audio_modes[] = {
	"audio: tuner", "audio: radio", "audio: extern",
	"audio: intern", "audio: mute"
};

static int
audio_mux(struct bttv *btv, int input, int mute)
{
	int gpio_val, signal;
	struct v4l2_control ctrl;

	gpio_inout(bttv_tvcards[btv->c.type].gpiomask,
		   bttv_tvcards[btv->c.type].gpiomask);
	signal = btread(BT848_DSTATUS) & BT848_DSTATUS_HLOC;

	btv->mute = mute;
	btv->audio = input;

	/* automute */
	mute = mute || (btv->opt_automute && !signal && !btv->radio_user);

	if (mute)
		gpio_val = bttv_tvcards[btv->c.type].gpiomute;
	else
		gpio_val = bttv_tvcards[btv->c.type].gpiomux[input];

	switch (btv->c.type) {
	case BTTV_BOARD_VOODOOTV_FM:
	case BTTV_BOARD_VOODOOTV_200:
		gpio_val = bttv_tda9880_setnorm(btv, gpio_val);
		break;

	default:
		gpio_bits(bttv_tvcards[btv->c.type].gpiomask, gpio_val);
	}

	if (bttv_gpio)
		bttv_gpio_tracking(btv, audio_modes[mute ? 4 : input]);
	if (in_interrupt())
		return 0;

	ctrl.id = V4L2_CID_AUDIO_MUTE;
	ctrl.value = btv->mute;
	bttv_call_all(btv, core, s_ctrl, &ctrl);
	if (btv->sd_msp34xx) {
		u32 in;

		/* Note: the inputs tuner/radio/extern/intern are translated
		   to msp routings. This assumes common behavior for all msp3400
		   based TV cards. When this assumption fails, then the
		   specific MSP routing must be added to the card table.
		   For now this is sufficient. */
		switch (input) {
		case TVAUDIO_INPUT_RADIO:
			in = MSP_INPUT(MSP_IN_SCART2, MSP_IN_TUNER1,
				    MSP_DSP_IN_SCART, MSP_DSP_IN_SCART);
			break;
		case TVAUDIO_INPUT_EXTERN:
			in = MSP_INPUT(MSP_IN_SCART1, MSP_IN_TUNER1,
				    MSP_DSP_IN_SCART, MSP_DSP_IN_SCART);
			break;
		case TVAUDIO_INPUT_INTERN:
			/* Yes, this is the same input as for RADIO. I doubt
			   if this is ever used. The only board with an INTERN
			   input is the BTTV_BOARD_AVERMEDIA98. I wonder how
			   that was tested. My guess is that the whole INTERN
			   input does not work. */
			in = MSP_INPUT(MSP_IN_SCART2, MSP_IN_TUNER1,
				    MSP_DSP_IN_SCART, MSP_DSP_IN_SCART);
			break;
		case TVAUDIO_INPUT_TUNER:
		default:
			/* This is the only card that uses TUNER2, and afaik,
			   is the only difference between the VOODOOTV_FM
			   and VOODOOTV_200 */
			if (btv->c.type == BTTV_BOARD_VOODOOTV_200)
				in = MSP_INPUT(MSP_IN_SCART1, MSP_IN_TUNER2, \
					MSP_DSP_IN_TUNER, MSP_DSP_IN_TUNER);
			else
				in = MSP_INPUT_DEFAULT;
			break;
		}
		v4l2_subdev_call(btv->sd_msp34xx, audio, s_routing,
			       in, MSP_OUTPUT_DEFAULT, 0);
	}
	if (btv->sd_tvaudio) {
		v4l2_subdev_call(btv->sd_tvaudio, audio, s_routing,
				input, 0, 0);
	}
	return 0;
}

static inline int
audio_mute(struct bttv *btv, int mute)
{
	return audio_mux(btv, btv->audio, mute);
}

static inline int
audio_input(struct bttv *btv, int input)
{
	return audio_mux(btv, input, btv->mute);
}

static void
bttv_crop_calc_limits(struct bttv_crop *c)
{
	/* Scale factor min. 1:1, max. 16:1. Min. image size
	   48 x 32. Scaled width must be a multiple of 4. */

	if (1) {
		/* For bug compatibility with VIDIOCGCAP and image
		   size checks in earlier driver versions. */
		c->min_scaled_width = 48;
		c->min_scaled_height = 32;
	} else {
		c->min_scaled_width =
			(max(48, c->rect.width >> 4) + 3) & ~3;
		c->min_scaled_height =
			max(32, c->rect.height >> 4);
	}

	c->max_scaled_width  = c->rect.width & ~3;
	c->max_scaled_height = c->rect.height;
}

static void
bttv_crop_reset(struct bttv_crop *c, unsigned int norm)
{
	c->rect = bttv_tvnorms[norm].cropcap.defrect;
	bttv_crop_calc_limits(c);
}

/* Call with btv->lock down. */
static int
set_tvnorm(struct bttv *btv, unsigned int norm)
{
	const struct bttv_tvnorm *tvnorm;
	v4l2_std_id id;

	BUG_ON(norm >= BTTV_TVNORMS);
	BUG_ON(btv->tvnorm >= BTTV_TVNORMS);

	tvnorm = &bttv_tvnorms[norm];

	if (memcmp(&bttv_tvnorms[btv->tvnorm].cropcap, &tvnorm->cropcap,
		    sizeof (tvnorm->cropcap))) {
		bttv_crop_reset(&btv->crop[0], norm);
		btv->crop[1] = btv->crop[0]; /* current = default */

		if (0 == (btv->resources & VIDEO_RESOURCES)) {
			btv->crop_start = tvnorm->cropcap.bounds.top
				+ tvnorm->cropcap.bounds.height;
		}
	}

	btv->tvnorm = norm;

	btwrite(tvnorm->adelay, BT848_ADELAY);
	btwrite(tvnorm->bdelay, BT848_BDELAY);
	btaor(tvnorm->iform,~(BT848_IFORM_NORM|BT848_IFORM_XTBOTH),
	      BT848_IFORM);
	btwrite(tvnorm->vbipack, BT848_VBI_PACK_SIZE);
	btwrite(1, BT848_VBI_PACK_DEL);
	bt848A_set_timing(btv);

	switch (btv->c.type) {
	case BTTV_BOARD_VOODOOTV_FM:
	case BTTV_BOARD_VOODOOTV_200:
		bttv_tda9880_setnorm(btv, gpio_read());
		break;
	}
	id = tvnorm->v4l2_id;
	bttv_call_all(btv, core, s_std, id);

	return 0;
}

/* Call with btv->lock down. */
static void
set_input(struct bttv *btv, unsigned int input, unsigned int norm)
{
	unsigned long flags;

	btv->input = input;
	if (irq_iswitch) {
		spin_lock_irqsave(&btv->s_lock,flags);
		if (btv->curr.frame_irq) {
			/* active capture -> delayed input switch */
			btv->new_input = input;
		} else {
			video_mux(btv,input);
		}
		spin_unlock_irqrestore(&btv->s_lock,flags);
	} else {
		video_mux(btv,input);
	}
	audio_input(btv, (btv->tuner_type != TUNER_ABSENT && input == 0) ?
			 TVAUDIO_INPUT_TUNER : TVAUDIO_INPUT_EXTERN);
	set_tvnorm(btv, norm);
}

static void init_irqreg(struct bttv *btv)
{
	/* clear status */
	btwrite(0xfffffUL, BT848_INT_STAT);

	if (bttv_tvcards[btv->c.type].no_video) {
		/* i2c only */
		btwrite(BT848_INT_I2CDONE,
			BT848_INT_MASK);
	} else {
		/* full video */
		btwrite((btv->triton1)  |
			(btv->gpioirq ? BT848_INT_GPINT : 0) |
			BT848_INT_SCERR |
			(fdsr ? BT848_INT_FDSR : 0) |
			BT848_INT_RISCI | BT848_INT_OCERR |
			BT848_INT_FMTCHG|BT848_INT_HLOCK|
			BT848_INT_I2CDONE,
			BT848_INT_MASK);
	}
}

static void init_bt848(struct bttv *btv)
{
	int val;

	if (bttv_tvcards[btv->c.type].no_video) {
		/* very basic init only */
		init_irqreg(btv);
		return;
	}

	btwrite(0x00, BT848_CAP_CTL);
	btwrite(BT848_COLOR_CTL_GAMMA, BT848_COLOR_CTL);
	btwrite(BT848_IFORM_XTAUTO | BT848_IFORM_AUTO, BT848_IFORM);

	/* set planar and packed mode trigger points and         */
	/* set rising edge of inverted GPINTR pin as irq trigger */
	btwrite(BT848_GPIO_DMA_CTL_PKTP_32|
		BT848_GPIO_DMA_CTL_PLTP1_16|
		BT848_GPIO_DMA_CTL_PLTP23_16|
		BT848_GPIO_DMA_CTL_GPINTC|
		BT848_GPIO_DMA_CTL_GPINTI,
		BT848_GPIO_DMA_CTL);

	val = btv->opt_chroma_agc ? BT848_SCLOOP_CAGC : 0;
	btwrite(val, BT848_E_SCLOOP);
	btwrite(val, BT848_O_SCLOOP);

	btwrite(0x20, BT848_E_VSCALE_HI);
	btwrite(0x20, BT848_O_VSCALE_HI);
	btwrite(BT848_ADC_RESERVED | (btv->opt_adc_crush ? BT848_ADC_CRUSH : 0),
		BT848_ADC);

	btwrite(whitecrush_upper, BT848_WC_UP);
	btwrite(whitecrush_lower, BT848_WC_DOWN);

	if (btv->opt_lumafilter) {
		btwrite(0, BT848_E_CONTROL);
		btwrite(0, BT848_O_CONTROL);
	} else {
		btwrite(BT848_CONTROL_LDEC, BT848_E_CONTROL);
		btwrite(BT848_CONTROL_LDEC, BT848_O_CONTROL);
	}

	bt848_bright(btv,   btv->bright);
	bt848_hue(btv,      btv->hue);
	bt848_contrast(btv, btv->contrast);
	bt848_sat(btv,      btv->saturation);

	/* interrupt */
	init_irqreg(btv);
}

static void bttv_reinit_bt848(struct bttv *btv)
{
	unsigned long flags;

	if (bttv_verbose)
		printk(KERN_INFO "bttv%d: reset, reinitialize\n",btv->c.nr);
	spin_lock_irqsave(&btv->s_lock,flags);
	btv->errors=0;
	bttv_set_dma(btv,0);
	spin_unlock_irqrestore(&btv->s_lock,flags);

	init_bt848(btv);
	btv->pll.pll_current = -1;
	set_input(btv, btv->input, btv->tvnorm);
}

static int bttv_g_ctrl(struct file *file, void *priv,
					struct v4l2_control *c)
{
	struct bttv_fh *fh = priv;
	struct bttv *btv = fh->btv;

	switch (c->id) {
	case V4L2_CID_BRIGHTNESS:
		c->value = btv->bright;
		break;
	case V4L2_CID_HUE:
		c->value = btv->hue;
		break;
	case V4L2_CID_CONTRAST:
		c->value = btv->contrast;
		break;
	case V4L2_CID_SATURATION:
		c->value = btv->saturation;
		break;

	case V4L2_CID_AUDIO_MUTE:
	case V4L2_CID_AUDIO_VOLUME:
	case V4L2_CID_AUDIO_BALANCE:
	case V4L2_CID_AUDIO_BASS:
	case V4L2_CID_AUDIO_TREBLE:
		bttv_call_all(btv, core, g_ctrl, c);
		break;

	case V4L2_CID_PRIVATE_CHROMA_AGC:
		c->value = btv->opt_chroma_agc;
		break;
	case V4L2_CID_PRIVATE_COMBFILTER:
		c->value = btv->opt_combfilter;
		break;
	case V4L2_CID_PRIVATE_LUMAFILTER:
		c->value = btv->opt_lumafilter;
		break;
	case V4L2_CID_PRIVATE_AUTOMUTE:
		c->value = btv->opt_automute;
		break;
	case V4L2_CID_PRIVATE_AGC_CRUSH:
		c->value = btv->opt_adc_crush;
		break;
	case V4L2_CID_PRIVATE_VCR_HACK:
		c->value = btv->opt_vcr_hack;
		break;
	case V4L2_CID_PRIVATE_WHITECRUSH_UPPER:
		c->value = btv->opt_whitecrush_upper;
		break;
	case V4L2_CID_PRIVATE_WHITECRUSH_LOWER:
		c->value = btv->opt_whitecrush_lower;
		break;
	case V4L2_CID_PRIVATE_UV_RATIO:
		c->value = btv->opt_uv_ratio;
		break;
	case V4L2_CID_PRIVATE_FULL_LUMA_RANGE:
		c->value = btv->opt_full_luma_range;
		break;
	case V4L2_CID_PRIVATE_CORING:
		c->value = btv->opt_coring;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int bttv_s_ctrl(struct file *file, void *f,
					struct v4l2_control *c)
{
	int err;
	int val;
	struct bttv_fh *fh = f;
	struct bttv *btv = fh->btv;

	err = v4l2_prio_check(&btv->prio, &fh->prio);
	if (0 != err)
		return err;

	switch (c->id) {
	case V4L2_CID_BRIGHTNESS:
		bt848_bright(btv, c->value);
		break;
	case V4L2_CID_HUE:
		bt848_hue(btv, c->value);
		break;
	case V4L2_CID_CONTRAST:
		bt848_contrast(btv, c->value);
		break;
	case V4L2_CID_SATURATION:
		bt848_sat(btv, c->value);
		break;
	case V4L2_CID_AUDIO_MUTE:
		audio_mute(btv, c->value);
		/* fall through */
	case V4L2_CID_AUDIO_VOLUME:
		if (btv->volume_gpio)
			btv->volume_gpio(btv, c->value);

		bttv_call_all(btv, core, s_ctrl, c);
		break;
	case V4L2_CID_AUDIO_BALANCE:
	case V4L2_CID_AUDIO_BASS:
	case V4L2_CID_AUDIO_TREBLE:
		bttv_call_all(btv, core, s_ctrl, c);
		break;

	case V4L2_CID_PRIVATE_CHROMA_AGC:
		btv->opt_chroma_agc = c->value;
		val = btv->opt_chroma_agc ? BT848_SCLOOP_CAGC : 0;
		btwrite(val, BT848_E_SCLOOP);
		btwrite(val, BT848_O_SCLOOP);
		break;
	case V4L2_CID_PRIVATE_COMBFILTER:
		btv->opt_combfilter = c->value;
		break;
	case V4L2_CID_PRIVATE_LUMAFILTER:
		btv->opt_lumafilter = c->value;
		if (btv->opt_lumafilter) {
			btand(~BT848_CONTROL_LDEC, BT848_E_CONTROL);
			btand(~BT848_CONTROL_LDEC, BT848_O_CONTROL);
		} else {
			btor(BT848_CONTROL_LDEC, BT848_E_CONTROL);
			btor(BT848_CONTROL_LDEC, BT848_O_CONTROL);
		}
		break;
	case V4L2_CID_PRIVATE_AUTOMUTE:
		btv->opt_automute = c->value;
		break;
	case V4L2_CID_PRIVATE_AGC_CRUSH:
		btv->opt_adc_crush = c->value;
		btwrite(BT848_ADC_RESERVED |
				(btv->opt_adc_crush ? BT848_ADC_CRUSH : 0),
				BT848_ADC);
		break;
	case V4L2_CID_PRIVATE_VCR_HACK:
		btv->opt_vcr_hack = c->value;
		break;
	case V4L2_CID_PRIVATE_WHITECRUSH_UPPER:
		btv->opt_whitecrush_upper = c->value;
		btwrite(c->value, BT848_WC_UP);
		break;
	case V4L2_CID_PRIVATE_WHITECRUSH_LOWER:
		btv->opt_whitecrush_lower = c->value;
		btwrite(c->value, BT848_WC_DOWN);
		break;
	case V4L2_CID_PRIVATE_UV_RATIO:
		btv->opt_uv_ratio = c->value;
		bt848_sat(btv, btv->saturation);
		break;
	case V4L2_CID_PRIVATE_FULL_LUMA_RANGE:
		btv->opt_full_luma_range = c->value;
		btaor((c->value<<7), ~BT848_OFORM_RANGE, BT848_OFORM);
		break;
	case V4L2_CID_PRIVATE_CORING:
		btv->opt_coring = c->value;
		btaor((c->value<<5), ~BT848_OFORM_CORE32, BT848_OFORM);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

/* ----------------------------------------------------------------------- */

void bttv_gpio_tracking(struct bttv *btv, char *comment)
{
	unsigned int outbits, data;
	outbits = btread(BT848_GPIO_OUT_EN);
	data    = btread(BT848_GPIO_DATA);
	printk(KERN_DEBUG "bttv%d: gpio: en=%08x, out=%08x in=%08x [%s]\n",
	       btv->c.nr,outbits,data & outbits, data & ~outbits, comment);
}

static void bttv_field_count(struct bttv *btv)
{
	int need_count = 0;

	if (btv->users)
		need_count++;

	if (need_count) {
		/* start field counter */
		btor(BT848_INT_VSYNC,BT848_INT_MASK);
	} else {
		/* stop field counter */
		btand(~BT848_INT_VSYNC,BT848_INT_MASK);
		btv->field_count = 0;
	}
}

static const struct bttv_format*
format_by_fourcc(int fourcc)
{
	unsigned int i;

	for (i = 0; i < FORMATS; i++) {
		if (-1 == formats[i].fourcc)
			continue;
		if (formats[i].fourcc == fourcc)
			return formats+i;
	}
	return NULL;
}

/* ----------------------------------------------------------------------- */
/* misc helpers                                                            */

static int
bttv_switch_overlay(struct bttv *btv, struct bttv_fh *fh,
		    struct bttv_buffer *new)
{
	struct bttv_buffer *old;
	unsigned long flags;
	int retval = 0;

	dprintk("switch_overlay: enter [new=%p]\n",new);
	if (new)
		new->vb.state = VIDEOBUF_DONE;
	spin_lock_irqsave(&btv->s_lock,flags);
	old = btv->screen;
	btv->screen = new;
	btv->loop_irq |= 1;
	bttv_set_dma(btv, 0x03);
	spin_unlock_irqrestore(&btv->s_lock,flags);
	if (NULL != old) {
		dprintk("switch_overlay: old=%p state is %d\n",old,old->vb.state);
		bttv_dma_free(&fh->cap,btv, old);
		kfree(old);
	}
	if (NULL == new)
		free_btres(btv,fh,RESOURCE_OVERLAY);
	dprintk("switch_overlay: done\n");
	return retval;
}

/* ----------------------------------------------------------------------- */
/* video4linux (1) interface                                               */

static int bttv_prepare_buffer(struct videobuf_queue *q,struct bttv *btv,
			       struct bttv_buffer *buf,
			       const struct bttv_format *fmt,
			       unsigned int width, unsigned int height,
			       enum v4l2_field field)
{
	struct bttv_fh *fh = q->priv_data;
	int redo_dma_risc = 0;
	struct bttv_crop c;
	int norm;
	int rc;

	/* check settings */
	if (NULL == fmt)
		return -EINVAL;
	if (fmt->btformat == BT848_COLOR_FMT_RAW) {
		width  = RAW_BPL;
		height = RAW_LINES*2;
		if (width*height > buf->vb.bsize)
			return -EINVAL;
		buf->vb.size = buf->vb.bsize;

		/* Make sure tvnorm and vbi_end remain consistent
		   until we're done. */
		mutex_lock(&btv->lock);

		norm = btv->tvnorm;

		/* In this mode capturing always starts at defrect.top
		   (default VDELAY), ignoring cropping parameters. */
		if (btv->vbi_end > bttv_tvnorms[norm].cropcap.defrect.top) {
			mutex_unlock(&btv->lock);
			return -EINVAL;
		}

		mutex_unlock(&btv->lock);

		c.rect = bttv_tvnorms[norm].cropcap.defrect;
	} else {
		mutex_lock(&btv->lock);

		norm = btv->tvnorm;
		c = btv->crop[!!fh->do_crop];

		mutex_unlock(&btv->lock);

		if (width < c.min_scaled_width ||
		    width > c.max_scaled_width ||
		    height < c.min_scaled_height)
			return -EINVAL;

		switch (field) {
		case V4L2_FIELD_TOP:
		case V4L2_FIELD_BOTTOM:
		case V4L2_FIELD_ALTERNATE:
			/* btv->crop counts frame lines. Max. scale
			   factor is 16:1 for frames, 8:1 for fields. */
			if (height * 2 > c.max_scaled_height)
				return -EINVAL;
			break;

		default:
			if (height > c.max_scaled_height)
				return -EINVAL;
			break;
		}

		buf->vb.size = (width * height * fmt->depth) >> 3;
		if (0 != buf->vb.baddr  &&  buf->vb.bsize < buf->vb.size)
			return -EINVAL;
	}

	/* alloc + fill struct bttv_buffer (if changed) */
	if (buf->vb.width != width || buf->vb.height != height ||
	    buf->vb.field != field ||
	    buf->tvnorm != norm || buf->fmt != fmt ||
	    buf->crop.top != c.rect.top ||
	    buf->crop.left != c.rect.left ||
	    buf->crop.width != c.rect.width ||
	    buf->crop.height != c.rect.height) {
		buf->vb.width  = width;
		buf->vb.height = height;
		buf->vb.field  = field;
		buf->tvnorm    = norm;
		buf->fmt       = fmt;
		buf->crop      = c.rect;
		redo_dma_risc = 1;
	}

	/* alloc risc memory */
	if (VIDEOBUF_NEEDS_INIT == buf->vb.state) {
		redo_dma_risc = 1;
		if (0 != (rc = videobuf_iolock(q,&buf->vb,&btv->fbuf)))
			goto fail;
	}

	if (redo_dma_risc)
		if (0 != (rc = bttv_buffer_risc(btv,buf)))
			goto fail;

	buf->vb.state = VIDEOBUF_PREPARED;
	return 0;

 fail:
	bttv_dma_free(q,btv,buf);
	return rc;
}

static int
buffer_setup(struct videobuf_queue *q, unsigned int *count, unsigned int *size)
{
	struct bttv_fh *fh = q->priv_data;

	*size = fh->fmt->depth*fh->width*fh->height >> 3;
	if (0 == *count)
		*count = gbuffers;
	while (*size * *count > gbuffers * gbufsize)
		(*count)--;
	return 0;
}

static int
buffer_prepare(struct videobuf_queue *q, struct videobuf_buffer *vb,
	       enum v4l2_field field)
{
	struct bttv_buffer *buf = container_of(vb,struct bttv_buffer,vb);
	struct bttv_fh *fh = q->priv_data;

	return bttv_prepare_buffer(q,fh->btv, buf, fh->fmt,
				   fh->width, fh->height, field);
}

static void
buffer_queue(struct videobuf_queue *q, struct videobuf_buffer *vb)
{
	struct bttv_buffer *buf = container_of(vb,struct bttv_buffer,vb);
	struct bttv_fh *fh = q->priv_data;
	struct bttv    *btv = fh->btv;

	buf->vb.state = VIDEOBUF_QUEUED;
	list_add_tail(&buf->vb.queue,&btv->capture);
	if (!btv->curr.frame_irq) {
		btv->loop_irq |= 1;
		bttv_set_dma(btv, 0x03);
	}
}

static void buffer_release(struct videobuf_queue *q, struct videobuf_buffer *vb)
{
	struct bttv_buffer *buf = container_of(vb,struct bttv_buffer,vb);
	struct bttv_fh *fh = q->priv_data;

	bttv_dma_free(q,fh->btv,buf);
}

static struct videobuf_queue_ops bttv_video_qops = {
	.buf_setup    = buffer_setup,
	.buf_prepare  = buffer_prepare,
	.buf_queue    = buffer_queue,
	.buf_release  = buffer_release,
};

static int bttv_s_std(struct file *file, void *priv, v4l2_std_id *id)
{
	struct bttv_fh *fh  = priv;
	struct bttv *btv = fh->btv;
	unsigned int i;
	int err;

	err = v4l2_prio_check(&btv->prio, &fh->prio);
	if (0 != err)
		return err;

	for (i = 0; i < BTTV_TVNORMS; i++)
		if (*id & bttv_tvnorms[i].v4l2_id)
			break;
	if (i == BTTV_TVNORMS)
		return -EINVAL;

	mutex_lock(&btv->lock);
	set_tvnorm(btv, i);
	mutex_unlock(&btv->lock);

	return 0;
}

static int bttv_querystd(struct file *file, void *f, v4l2_std_id *id)
{
	struct bttv_fh *fh = f;
	struct bttv *btv = fh->btv;

	if (btread(BT848_DSTATUS) & BT848_DSTATUS_NUML)
		*id = V4L2_STD_625_50;
	else
		*id = V4L2_STD_525_60;
	return 0;
}

static int bttv_enum_input(struct file *file, void *priv,
					struct v4l2_input *i)
{
	struct bttv_fh *fh = priv;
	struct bttv *btv = fh->btv;
	int n;

	if (i->index >= bttv_tvcards[btv->c.type].video_inputs)
		return -EINVAL;

	i->type     = V4L2_INPUT_TYPE_CAMERA;
	i->audioset = 1;

	if (btv->tuner_type != TUNER_ABSENT && i->index == 0) {
		sprintf(i->name, "Television");
		i->type  = V4L2_INPUT_TYPE_TUNER;
		i->tuner = 0;
	} else if (i->index == btv->svhs) {
		sprintf(i->name, "S-Video");
	} else {
		sprintf(i->name, "Composite%d", i->index);
	}

	if (i->index == btv->input) {
		__u32 dstatus = btread(BT848_DSTATUS);
		if (0 == (dstatus & BT848_DSTATUS_PRES))
			i->status |= V4L2_IN_ST_NO_SIGNAL;
		if (0 == (dstatus & BT848_DSTATUS_HLOC))
			i->status |= V4L2_IN_ST_NO_H_LOCK;
	}

	for (n = 0; n < BTTV_TVNORMS; n++)
		i->std |= bttv_tvnorms[n].v4l2_id;

	return 0;
}

static int bttv_g_input(struct file *file, void *priv, unsigned int *i)
{
	struct bttv_fh *fh = priv;
	struct bttv *btv = fh->btv;

	*i = btv->input;
	return 0;
}

static int bttv_s_input(struct file *file, void *priv, unsigned int i)
{
	struct bttv_fh *fh  = priv;
	struct bttv *btv = fh->btv;

	int err;

	err = v4l2_prio_check(&btv->prio, &fh->prio);
	if (0 != err)
		return err;

	if (i > bttv_tvcards[btv->c.type].video_inputs)
		return -EINVAL;

	mutex_lock(&btv->lock);
	set_input(btv, i, btv->tvnorm);
	mutex_unlock(&btv->lock);
	return 0;
}

static int bttv_s_tuner(struct file *file, void *priv,
					struct v4l2_tuner *t)
{
	struct bttv_fh *fh  = priv;
	struct bttv *btv = fh->btv;
	int err;

	err = v4l2_prio_check(&btv->prio, &fh->prio);
	if (0 != err)
		return err;

	if (btv->tuner_type == TUNER_ABSENT)
		return -EINVAL;

	if (0 != t->index)
		return -EINVAL;

	mutex_lock(&btv->lock);
	bttv_call_all(btv, tuner, s_tuner, t);

	if (btv->audio_mode_gpio)
		btv->audio_mode_gpio(btv, t, 1);

	mutex_unlock(&btv->lock);

	return 0;
}

static int bttv_g_frequency(struct file *file, void *priv,
					struct v4l2_frequency *f)
{
	struct bttv_fh *fh  = priv;
	struct bttv *btv = fh->btv;
	int err;

	err = v4l2_prio_check(&btv->prio, &fh->prio);
	if (0 != err)
		return err;

	f->type = btv->radio_user ? V4L2_TUNER_RADIO : V4L2_TUNER_ANALOG_TV;
	f->frequency = btv->freq;

	return 0;
}

static int bttv_s_frequency(struct file *file, void *priv,
					struct v4l2_frequency *f)
{
	struct bttv_fh *fh  = priv;
	struct bttv *btv = fh->btv;
	int err;

	err = v4l2_prio_check(&btv->prio, &fh->prio);
	if (0 != err)
		return err;

	if (unlikely(f->tuner != 0))
		return -EINVAL;
	if (unlikely(f->type != (btv->radio_user
		? V4L2_TUNER_RADIO : V4L2_TUNER_ANALOG_TV)))
		return -EINVAL;
	mutex_lock(&btv->lock);
	btv->freq = f->frequency;
	bttv_call_all(btv, tuner, s_frequency, f);
	if (btv->has_matchbox && btv->radio_user)
		tea5757_set_freq(btv, btv->freq);
	mutex_unlock(&btv->lock);
	return 0;
}

static int bttv_log_status(struct file *file, void *f)
{
	struct bttv_fh *fh  = f;
	struct bttv *btv = fh->btv;

	printk(KERN_INFO "bttv%d: ========  START STATUS CARD #%d  ========\n",
			btv->c.nr, btv->c.nr);
	bttv_call_all(btv, core, log_status);
	printk(KERN_INFO "bttv%d: ========  END STATUS CARD   #%d  ========\n",
			btv->c.nr, btv->c.nr);
	return 0;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int bttv_g_register(struct file *file, void *f,
					struct v4l2_dbg_register *reg)
{
	struct bttv_fh *fh = f;
	struct bttv *btv = fh->btv;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	if (!v4l2_chip_match_host(&reg->match))
		return -EINVAL;

	/* bt848 has a 12-bit register space */
	reg->reg &= 0xfff;
	reg->val = btread(reg->reg);
	reg->size = 1;

	return 0;
}

static int bttv_s_register(struct file *file, void *f,
					struct v4l2_dbg_register *reg)
{
	struct bttv_fh *fh = f;
	struct bttv *btv = fh->btv;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	if (!v4l2_chip_match_host(&reg->match))
		return -EINVAL;

	/* bt848 has a 12-bit register space */
	reg->reg &= 0xfff;
	btwrite(reg->val, reg->reg);

	return 0;
}
#endif

/* Given cropping boundaries b and the scaled width and height of a
   single field or frame, which must not exceed hardware limits, this
   function adjusts the cropping parameters c. */
static void
bttv_crop_adjust	(struct bttv_crop *             c,
			 const struct v4l2_rect *	b,
			 __s32                          width,
			 __s32                          height,
			 enum v4l2_field                field)
{
	__s32 frame_height = height << !V4L2_FIELD_HAS_BOTH(field);
	__s32 max_left;
	__s32 max_top;

	if (width < c->min_scaled_width) {
		/* Max. hor. scale factor 16:1. */
		c->rect.width = width * 16;
	} else if (width > c->max_scaled_width) {
		/* Min. hor. scale factor 1:1. */
		c->rect.width = width;

		max_left = b->left + b->width - width;
		max_left = min(max_left, (__s32) MAX_HDELAY);
		if (c->rect.left > max_left)
			c->rect.left = max_left;
	}

	if (height < c->min_scaled_height) {
		/* Max. vert. scale factor 16:1, single fields 8:1. */
		c->rect.height = height * 16;
	} else if (frame_height > c->max_scaled_height) {
		/* Min. vert. scale factor 1:1.
		   Top and height count field lines times two. */
		c->rect.height = (frame_height + 1) & ~1;

		max_top = b->top + b->height - c->rect.height;
		if (c->rect.top > max_top)
			c->rect.top = max_top;
	}

	bttv_crop_calc_limits(c);
}

/* Returns an error if scaling to a frame or single field with the given
   width and height is not possible with the current cropping parameters
   and width aligned according to width_mask. If adjust_size is TRUE the
   function may adjust the width and/or height instead, rounding width
   to (width + width_bias) & width_mask. If adjust_crop is TRUE it may
   also adjust the current cropping parameters to get closer to the
   desired image size. */
static int
limit_scaled_size       (struct bttv_fh *               fh,
			 __s32 *                        width,
			 __s32 *                        height,
			 enum v4l2_field                field,
			 unsigned int			width_mask,
			 unsigned int			width_bias,
			 int                            adjust_size,
			 int                            adjust_crop)
{
	struct bttv *btv = fh->btv;
	const struct v4l2_rect *b;
	struct bttv_crop *c;
	__s32 min_width;
	__s32 min_height;
	__s32 max_width;
	__s32 max_height;
	int rc;

	BUG_ON((int) width_mask >= 0 ||
	       width_bias >= (unsigned int) -width_mask);

	/* Make sure tvnorm, vbi_end and the current cropping parameters
	   remain consistent until we're done. */
	mutex_lock(&btv->lock);

	b = &bttv_tvnorms[btv->tvnorm].cropcap.bounds;

	/* Do crop - use current, don't - use default parameters. */
	c = &btv->crop[!!fh->do_crop];

	if (fh->do_crop
	    && adjust_size
	    && adjust_crop
	    && !locked_btres(btv, VIDEO_RESOURCES)) {
		min_width = 48;
		min_height = 32;

		/* We cannot scale up. When the scaled image is larger
		   than crop.rect we adjust the crop.rect as required
		   by the V4L2 spec, hence cropcap.bounds are our limit. */
		max_width = min(b->width, (__s32) MAX_HACTIVE);
		max_height = b->height;

		/* We cannot capture the same line as video and VBI data.
		   Note btv->vbi_end is really a minimum, see
		   bttv_vbi_try_fmt(). */
		if (btv->vbi_end > b->top) {
			max_height -= btv->vbi_end - b->top;
			rc = -EBUSY;
			if (min_height > max_height)
				goto fail;
		}
	} else {
		rc = -EBUSY;
		if (btv->vbi_end > c->rect.top)
			goto fail;

		min_width  = c->min_scaled_width;
		min_height = c->min_scaled_height;
		max_width  = c->max_scaled_width;
		max_height = c->max_scaled_height;

		adjust_crop = 0;
	}

	min_width = (min_width - width_mask - 1) & width_mask;
	max_width = max_width & width_mask;

	/* Max. scale factor is 16:1 for frames, 8:1 for fields. */
	min_height = min_height;
	/* Min. scale factor is 1:1. */
	max_height >>= !V4L2_FIELD_HAS_BOTH(field);

	if (adjust_size) {
		*width = clamp(*width, min_width, max_width);
		*height = clamp(*height, min_height, max_height);

		/* Round after clamping to avoid overflow. */
		*width = (*width + width_bias) & width_mask;

		if (adjust_crop) {
			bttv_crop_adjust(c, b, *width, *height, field);

			if (btv->vbi_end > c->rect.top) {
				/* Move the crop window out of the way. */
				c->rect.top = btv->vbi_end;
			}
		}
	} else {
		rc = -EINVAL;
		if (*width  < min_width ||
		    *height < min_height ||
		    *width  > max_width ||
		    *height > max_height ||
		    0 != (*width & ~width_mask))
			goto fail;
	}

	rc = 0; /* success */

 fail:
	mutex_unlock(&btv->lock);

	return rc;
}

/* Returns an error if the given overlay window dimensions are not
   possible with the current cropping parameters. If adjust_size is
   TRUE the function may adjust the window width and/or height
   instead, however it always rounds the horizontal position and
   width as btcx_align() does. If adjust_crop is TRUE the function
   may also adjust the current cropping parameters to get closer
   to the desired window size. */
static int
verify_window		(struct bttv_fh *               fh,
			 struct v4l2_window *           win,
			 int                            adjust_size,
			 int                            adjust_crop)
{
	enum v4l2_field field;
	unsigned int width_mask;
	int rc;

	if (win->w.width  < 48 || win->w.height < 32)
		return -EINVAL;
	if (win->clipcount > 2048)
		return -EINVAL;

	field = win->field;

	if (V4L2_FIELD_ANY == field) {
		__s32 height2;

		height2 = fh->btv->crop[!!fh->do_crop].rect.height >> 1;
		field = (win->w.height > height2)
			? V4L2_FIELD_INTERLACED
			: V4L2_FIELD_TOP;
	}
	switch (field) {
	case V4L2_FIELD_TOP:
	case V4L2_FIELD_BOTTOM:
	case V4L2_FIELD_INTERLACED:
		break;
	default:
		return -EINVAL;
	}

	/* 4-byte alignment. */
	if (NULL == fh->ovfmt)
		return -EINVAL;
	width_mask = ~0;
	switch (fh->ovfmt->depth) {
	case 8:
	case 24:
		width_mask = ~3;
		break;
	case 16:
		width_mask = ~1;
		break;
	case 32:
		break;
	default:
		BUG();
	}

	win->w.width -= win->w.left & ~width_mask;
	win->w.left = (win->w.left - width_mask - 1) & width_mask;

	rc = limit_scaled_size(fh, &win->w.width, &win->w.height,
			       field, width_mask,
			       /* width_bias: round down */ 0,
			       adjust_size, adjust_crop);
	if (0 != rc)
		return rc;

	win->field = field;
	return 0;
}

static int setup_window(struct bttv_fh *fh, struct bttv *btv,
			struct v4l2_window *win, int fixup)
{
	struct v4l2_clip *clips = NULL;
	int n,size,retval = 0;

	if (NULL == fh->ovfmt)
		return -EINVAL;
	if (!(fh->ovfmt->flags & FORMAT_FLAGS_PACKED))
		return -EINVAL;
	retval = verify_window(fh, win,
			       /* adjust_size */ fixup,
			       /* adjust_crop */ fixup);
	if (0 != retval)
		return retval;

	/* copy clips  --  luckily v4l1 + v4l2 are binary
	   compatible here ...*/
	n = win->clipcount;
	size = sizeof(*clips)*(n+4);
	clips = kmalloc(size,GFP_KERNEL);
	if (NULL == clips)
		return -ENOMEM;
	if (n > 0) {
		if (copy_from_user(clips,win->clips,sizeof(struct v4l2_clip)*n)) {
			kfree(clips);
			return -EFAULT;
		}
	}
	/* clip against screen */
	if (NULL != btv->fbuf.base)
		n = btcx_screen_clips(btv->fbuf.fmt.width, btv->fbuf.fmt.height,
				      &win->w, clips, n);
	btcx_sort_clips(clips,n);

	/* 4-byte alignments */
	switch (fh->ovfmt->depth) {
	case 8:
	case 24:
		btcx_align(&win->w, clips, n, 3);
		break;
	case 16:
		btcx_align(&win->w, clips, n, 1);
		break;
	case 32:
		/* no alignment fixups needed */
		break;
	default:
		BUG();
	}

	mutex_lock(&fh->cap.vb_lock);
	kfree(fh->ov.clips);
	fh->ov.clips    = clips;
	fh->ov.nclips   = n;

	fh->ov.w        = win->w;
	fh->ov.field    = win->field;
	fh->ov.setup_ok = 1;
	btv->init.ov.w.width   = win->w.width;
	btv->init.ov.w.height  = win->w.height;
	btv->init.ov.field     = win->field;

	/* update overlay if needed */
	retval = 0;
	if (check_btres(fh, RESOURCE_OVERLAY)) {
		struct bttv_buffer *new;

		new = videobuf_sg_alloc(sizeof(*new));
		new->crop = btv->crop[!!fh->do_crop].rect;
		bttv_overlay_risc(btv, &fh->ov, fh->ovfmt, new);
		retval = bttv_switch_overlay(btv,fh,new);
	}
	mutex_unlock(&fh->cap.vb_lock);
	return retval;
}

/* ----------------------------------------------------------------------- */

static struct videobuf_queue* bttv_queue(struct bttv_fh *fh)
{
	struct videobuf_queue* q = NULL;

	switch (fh->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		q = &fh->cap;
		break;
	case V4L2_BUF_TYPE_VBI_CAPTURE:
		q = &fh->vbi;
		break;
	default:
		BUG();
	}
	return q;
}

static int bttv_resource(struct bttv_fh *fh)
{
	int res = 0;

	switch (fh->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		res = RESOURCE_VIDEO_STREAM;
		break;
	case V4L2_BUF_TYPE_VBI_CAPTURE:
		res = RESOURCE_VBI;
		break;
	default:
		BUG();
	}
	return res;
}

static int bttv_switch_type(struct bttv_fh *fh, enum v4l2_buf_type type)
{
	struct videobuf_queue *q = bttv_queue(fh);
	int res = bttv_resource(fh);

	if (check_btres(fh,res))
		return -EBUSY;
	if (videobuf_queue_is_busy(q))
		return -EBUSY;
	fh->type = type;
	return 0;
}

static void
pix_format_set_size     (struct v4l2_pix_format *       f,
			 const struct bttv_format *     fmt,
			 unsigned int                   width,
			 unsigned int                   height)
{
	f->width = width;
	f->height = height;

	if (fmt->flags & FORMAT_FLAGS_PLANAR) {
		f->bytesperline = width; /* Y plane */
		f->sizeimage = (width * height * fmt->depth) >> 3;
	} else {
		f->bytesperline = (width * fmt->depth) >> 3;
		f->sizeimage = height * f->bytesperline;
	}
}

static int bttv_g_fmt_vid_cap(struct file *file, void *priv,
					struct v4l2_format *f)
{
	struct bttv_fh *fh  = priv;

	pix_format_set_size(&f->fmt.pix, fh->fmt,
				fh->width, fh->height);
	f->fmt.pix.field        = fh->cap.field;
	f->fmt.pix.pixelformat  = fh->fmt->fourcc;

	return 0;
}

static int bttv_g_fmt_vid_overlay(struct file *file, void *priv,
					struct v4l2_format *f)
{
	struct bttv_fh *fh  = priv;

	f->fmt.win.w     = fh->ov.w;
	f->fmt.win.field = fh->ov.field;

	return 0;
}

static int bttv_try_fmt_vid_cap(struct file *file, void *priv,
						struct v4l2_format *f)
{
	const struct bttv_format *fmt;
	struct bttv_fh *fh = priv;
	struct bttv *btv = fh->btv;
	enum v4l2_field field;
	__s32 width, height;
	int rc;

	fmt = format_by_fourcc(f->fmt.pix.pixelformat);
	if (NULL == fmt)
		return -EINVAL;

	field = f->fmt.pix.field;

	if (V4L2_FIELD_ANY == field) {
		__s32 height2;

		height2 = btv->crop[!!fh->do_crop].rect.height >> 1;
		field = (f->fmt.pix.height > height2)
			? V4L2_FIELD_INTERLACED
			: V4L2_FIELD_BOTTOM;
	}

	if (V4L2_FIELD_SEQ_BT == field)
		field = V4L2_FIELD_SEQ_TB;

	switch (field) {
	case V4L2_FIELD_TOP:
	case V4L2_FIELD_BOTTOM:
	case V4L2_FIELD_ALTERNATE:
	case V4L2_FIELD_INTERLACED:
		break;
	case V4L2_FIELD_SEQ_TB:
		if (fmt->flags & FORMAT_FLAGS_PLANAR)
			return -EINVAL;
		break;
	default:
		return -EINVAL;
	}

	width = f->fmt.pix.width;
	height = f->fmt.pix.height;

	rc = limit_scaled_size(fh, &width, &height, field,
			       /* width_mask: 4 pixels */ ~3,
			       /* width_bias: nearest */ 2,
			       /* adjust_size */ 1,
			       /* adjust_crop */ 0);
	if (0 != rc)
		return rc;

	/* update data for the application */
	f->fmt.pix.field = field;
	pix_format_set_size(&f->fmt.pix, fmt, width, height);

	return 0;
}

static int bttv_try_fmt_vid_overlay(struct file *file, void *priv,
						struct v4l2_format *f)
{
	struct bttv_fh *fh = priv;

	return verify_window(fh, &f->fmt.win,
			/* adjust_size */ 1,
			/* adjust_crop */ 0);
}

static int bttv_s_fmt_vid_cap(struct file *file, void *priv,
				struct v4l2_format *f)
{
	int retval;
	const struct bttv_format *fmt;
	struct bttv_fh *fh = priv;
	struct bttv *btv = fh->btv;
	__s32 width, height;
	enum v4l2_field field;

	retval = bttv_switch_type(fh, f->type);
	if (0 != retval)
		return retval;

	retval = bttv_try_fmt_vid_cap(file, priv, f);
	if (0 != retval)
		return retval;

	width = f->fmt.pix.width;
	height = f->fmt.pix.height;
	field = f->fmt.pix.field;

	retval = limit_scaled_size(fh, &width, &height, f->fmt.pix.field,
			       /* width_mask: 4 pixels */ ~3,
			       /* width_bias: nearest */ 2,
			       /* adjust_size */ 1,
			       /* adjust_crop */ 1);
	if (0 != retval)
		return retval;

	f->fmt.pix.field = field;

	fmt = format_by_fourcc(f->fmt.pix.pixelformat);

	/* update our state informations */
	mutex_lock(&fh->cap.vb_lock);
	fh->fmt              = fmt;
	fh->cap.field        = f->fmt.pix.field;
	fh->cap.last         = V4L2_FIELD_NONE;
	fh->width            = f->fmt.pix.width;
	fh->height           = f->fmt.pix.height;
	btv->init.fmt        = fmt;
	btv->init.width      = f->fmt.pix.width;
	btv->init.height     = f->fmt.pix.height;
	mutex_unlock(&fh->cap.vb_lock);

	return 0;
}

static int bttv_s_fmt_vid_overlay(struct file *file, void *priv,
				struct v4l2_format *f)
{
	struct bttv_fh *fh = priv;
	struct bttv *btv = fh->btv;

	if (no_overlay > 0) {
		printk(KERN_ERR "V4L2_BUF_TYPE_VIDEO_OVERLAY: no_overlay\n");
		return -EINVAL;
	}

	return setup_window(fh, btv, &f->fmt.win, 1);
}

#ifdef CONFIG_VIDEO_V4L1_COMPAT
static int vidiocgmbuf(struct file *file, void *priv, struct video_mbuf *mbuf)
{
	int retval;
	unsigned int i;
	struct bttv_fh *fh = priv;

	mutex_lock(&fh->cap.vb_lock);
	retval = __videobuf_mmap_setup(&fh->cap, gbuffers, gbufsize,
				     V4L2_MEMORY_MMAP);
	if (retval < 0) {
		mutex_unlock(&fh->cap.vb_lock);
		return retval;
	}

	gbuffers = retval;
	memset(mbuf, 0, sizeof(*mbuf));
	mbuf->frames = gbuffers;
	mbuf->size   = gbuffers * gbufsize;

	for (i = 0; i < gbuffers; i++)
		mbuf->offsets[i] = i * gbufsize;

	mutex_unlock(&fh->cap.vb_lock);
	return 0;
}
#endif

static int bttv_querycap(struct file *file, void  *priv,
				struct v4l2_capability *cap)
{
	struct bttv_fh *fh = priv;
	struct bttv *btv = fh->btv;

	if (0 == v4l2)
		return -EINVAL;

	strlcpy(cap->driver, "bttv", sizeof(cap->driver));
	strlcpy(cap->card, btv->video_dev->name, sizeof(cap->card));
	snprintf(cap->bus_info, sizeof(cap->bus_info),
		 "PCI:%s", pci_name(btv->c.pci));
	cap->version = BTTV_VERSION_CODE;
	cap->capabilities =
		V4L2_CAP_VIDEO_CAPTURE |
		V4L2_CAP_VBI_CAPTURE |
		V4L2_CAP_READWRITE |
		V4L2_CAP_STREAMING;
	if (btv->has_saa6588)
		cap->capabilities |= V4L2_CAP_RDS_CAPTURE;
	if (no_overlay <= 0)
		cap->capabilities |= V4L2_CAP_VIDEO_OVERLAY;

	if (btv->tuner_type != TUNER_ABSENT)
		cap->capabilities |= V4L2_CAP_TUNER;
	return 0;
}

static int bttv_enum_fmt_cap_ovr(struct v4l2_fmtdesc *f)
{
	int index = -1, i;

	for (i = 0; i < FORMATS; i++) {
		if (formats[i].fourcc != -1)
			index++;
		if ((unsigned int)index == f->index)
			break;
	}
	if (FORMATS == i)
		return -EINVAL;

	f->pixelformat = formats[i].fourcc;
	strlcpy(f->description, formats[i].name, sizeof(f->description));

	return i;
}

static int bttv_enum_fmt_vid_cap(struct file *file, void  *priv,
				struct v4l2_fmtdesc *f)
{
	int rc = bttv_enum_fmt_cap_ovr(f);

	if (rc < 0)
		return rc;

	return 0;
}

static int bttv_enum_fmt_vid_overlay(struct file *file, void  *priv,
					struct v4l2_fmtdesc *f)
{
	int rc;

	if (no_overlay > 0) {
		printk(KERN_ERR "V4L2_BUF_TYPE_VIDEO_OVERLAY: no_overlay\n");
		return -EINVAL;
	}

	rc = bttv_enum_fmt_cap_ovr(f);

	if (rc < 0)
		return rc;

	if (!(formats[rc].flags & FORMAT_FLAGS_PACKED))
		return -EINVAL;

	return 0;
}

static int bttv_g_fbuf(struct file *file, void *f,
				struct v4l2_framebuffer *fb)
{
	struct bttv_fh *fh = f;
	struct bttv *btv = fh->btv;

	*fb = btv->fbuf;
	fb->capability = V4L2_FBUF_CAP_LIST_CLIPPING;
	if (fh->ovfmt)
		fb->fmt.pixelformat  = fh->ovfmt->fourcc;
	return 0;
}

static int bttv_overlay(struct file *file, void *f, unsigned int on)
{
	struct bttv_fh *fh = f;
	struct bttv *btv = fh->btv;
	struct bttv_buffer *new;
	int retval;

	if (on) {
		/* verify args */
		if (NULL == btv->fbuf.base)
			return -EINVAL;
		if (!fh->ov.setup_ok) {
			dprintk("bttv%d: overlay: !setup_ok\n", btv->c.nr);
			return -EINVAL;
		}
	}

	if (!check_alloc_btres(btv, fh, RESOURCE_OVERLAY))
		return -EBUSY;

	mutex_lock(&fh->cap.vb_lock);
	if (on) {
		fh->ov.tvnorm = btv->tvnorm;
		new = videobuf_sg_alloc(sizeof(*new));
		new->crop = btv->crop[!!fh->do_crop].rect;
		bttv_overlay_risc(btv, &fh->ov, fh->ovfmt, new);
	} else {
		new = NULL;
	}

	/* switch over */
	retval = bttv_switch_overlay(btv, fh, new);
	mutex_unlock(&fh->cap.vb_lock);
	return retval;
}

static int bttv_s_fbuf(struct file *file, void *f,
				struct v4l2_framebuffer *fb)
{
	struct bttv_fh *fh = f;
	struct bttv *btv = fh->btv;
	const struct bttv_format *fmt;
	int retval;

	if (!capable(CAP_SYS_ADMIN) &&
		!capable(CAP_SYS_RAWIO))
		return -EPERM;

	/* check args */
	fmt = format_by_fourcc(fb->fmt.pixelformat);
	if (NULL == fmt)
		return -EINVAL;
	if (0 == (fmt->flags & FORMAT_FLAGS_PACKED))
		return -EINVAL;

	retval = -EINVAL;
	if (fb->flags & V4L2_FBUF_FLAG_OVERLAY) {
		__s32 width = fb->fmt.width;
		__s32 height = fb->fmt.height;

		retval = limit_scaled_size(fh, &width, &height,
					   V4L2_FIELD_INTERLACED,
					   /* width_mask */ ~3,
					   /* width_bias */ 2,
					   /* adjust_size */ 0,
					   /* adjust_crop */ 0);
		if (0 != retval)
			return retval;
	}

	/* ok, accept it */
	mutex_lock(&fh->cap.vb_lock);
	btv->fbuf.base       = fb->base;
	btv->fbuf.fmt.width  = fb->fmt.width;
	btv->fbuf.fmt.height = fb->fmt.height;
	if (0 != fb->fmt.bytesperline)
		btv->fbuf.fmt.bytesperline = fb->fmt.bytesperline;
	else
		btv->fbuf.fmt.bytesperline = btv->fbuf.fmt.width*fmt->depth/8;

	retval = 0;
	fh->ovfmt = fmt;
	btv->init.ovfmt = fmt;
	if (fb->flags & V4L2_FBUF_FLAG_OVERLAY) {
		fh->ov.w.left   = 0;
		fh->ov.w.top    = 0;
		fh->ov.w.width  = fb->fmt.width;
		fh->ov.w.height = fb->fmt.height;
		btv->init.ov.w.width  = fb->fmt.width;
		btv->init.ov.w.height = fb->fmt.height;
			kfree(fh->ov.clips);
		fh->ov.clips = NULL;
		fh->ov.nclips = 0;

		if (check_btres(fh, RESOURCE_OVERLAY)) {
			struct bttv_buffer *new;

			new = videobuf_sg_alloc(sizeof(*new));
			new->crop = btv->crop[!!fh->do_crop].rect;
			bttv_overlay_risc(btv, &fh->ov, fh->ovfmt, new);
			retval = bttv_switch_overlay(btv, fh, new);
		}
	}
	mutex_unlock(&fh->cap.vb_lock);
	return retval;
}

static int bttv_reqbufs(struct file *file, void *priv,
				struct v4l2_requestbuffers *p)
{
	struct bttv_fh *fh = priv;
	return videobuf_reqbufs(bttv_queue(fh), p);
}

static int bttv_querybuf(struct file *file, void *priv,
				struct v4l2_buffer *b)
{
	struct bttv_fh *fh = priv;
	return videobuf_querybuf(bttv_queue(fh), b);
}

static int bttv_qbuf(struct file *file, void *priv, struct v4l2_buffer *b)
{
	struct bttv_fh *fh = priv;
	struct bttv *btv = fh->btv;
	int res = bttv_resource(fh);

	if (!check_alloc_btres(btv, fh, res))
		return -EBUSY;

	return videobuf_qbuf(bttv_queue(fh), b);
}

static int bttv_dqbuf(struct file *file, void *priv, struct v4l2_buffer *b)
{
	struct bttv_fh *fh = priv;
	return videobuf_dqbuf(bttv_queue(fh), b,
			file->f_flags & O_NONBLOCK);
}

static int bttv_streamon(struct file *file, void *priv,
					enum v4l2_buf_type type)
{
	struct bttv_fh *fh = priv;
	struct bttv *btv = fh->btv;
	int res = bttv_resource(fh);

	if (!check_alloc_btres(btv, fh, res))
		return -EBUSY;
	return videobuf_streamon(bttv_queue(fh));
}


static int bttv_streamoff(struct file *file, void *priv,
					enum v4l2_buf_type type)
{
	struct bttv_fh *fh = priv;
	struct bttv *btv = fh->btv;
	int retval;
	int res = bttv_resource(fh);


	retval = videobuf_streamoff(bttv_queue(fh));
	if (retval < 0)
		return retval;
	free_btres(btv, fh, res);
	return 0;
}

static int bttv_queryctrl(struct file *file, void *priv,
					struct v4l2_queryctrl *c)
{
	struct bttv_fh *fh = priv;
	struct bttv *btv = fh->btv;
	const struct v4l2_queryctrl *ctrl;

	if ((c->id <  V4L2_CID_BASE ||
	     c->id >= V4L2_CID_LASTP1) &&
	    (c->id <  V4L2_CID_PRIVATE_BASE ||
	     c->id >= V4L2_CID_PRIVATE_LASTP1))
		return -EINVAL;

	if (!btv->volume_gpio && (c->id == V4L2_CID_AUDIO_VOLUME))
		*c = no_ctl;
	else {
		ctrl = ctrl_by_id(c->id);

		*c = (NULL != ctrl) ? *ctrl : no_ctl;
	}

	return 0;
}

static int bttv_g_parm(struct file *file, void *f,
				struct v4l2_streamparm *parm)
{
	struct bttv_fh *fh = f;
	struct bttv *btv = fh->btv;

	v4l2_video_std_frame_period(bttv_tvnorms[btv->tvnorm].v4l2_id,
				    &parm->parm.capture.timeperframe);
	return 0;
}

static int bttv_g_tuner(struct file *file, void *priv,
				struct v4l2_tuner *t)
{
	struct bttv_fh *fh = priv;
	struct bttv *btv = fh->btv;

	if (btv->tuner_type == TUNER_ABSENT)
		return -EINVAL;
	if (0 != t->index)
		return -EINVAL;

	mutex_lock(&btv->lock);
	t->rxsubchans = V4L2_TUNER_SUB_MONO;
	bttv_call_all(btv, tuner, g_tuner, t);
	strcpy(t->name, "Television");
	t->capability = V4L2_TUNER_CAP_NORM;
	t->type       = V4L2_TUNER_ANALOG_TV;
	if (btread(BT848_DSTATUS)&BT848_DSTATUS_HLOC)
		t->signal = 0xffff;

	if (btv->audio_mode_gpio)
		btv->audio_mode_gpio(btv, t, 0);

	mutex_unlock(&btv->lock);
	return 0;
}

static int bttv_g_priority(struct file *file, void *f, enum v4l2_priority *p)
{
	struct bttv_fh *fh = f;
	struct bttv *btv = fh->btv;

	*p = v4l2_prio_max(&btv->prio);

	return 0;
}

static int bttv_s_priority(struct file *file, void *f,
					enum v4l2_priority prio)
{
	struct bttv_fh *fh = f;
	struct bttv *btv = fh->btv;

	return v4l2_prio_change(&btv->prio, &fh->prio, prio);
}

static int bttv_cropcap(struct file *file, void *priv,
				struct v4l2_cropcap *cap)
{
	struct bttv_fh *fh = priv;
	struct bttv *btv = fh->btv;

	if (cap->type != V4L2_BUF_TYPE_VIDEO_CAPTURE &&
	    cap->type != V4L2_BUF_TYPE_VIDEO_OVERLAY)
		return -EINVAL;

	*cap = bttv_tvnorms[btv->tvnorm].cropcap;

	return 0;
}

static int bttv_g_crop(struct file *file, void *f, struct v4l2_crop *crop)
{
	struct bttv_fh *fh = f;
	struct bttv *btv = fh->btv;

	if (crop->type != V4L2_BUF_TYPE_VIDEO_CAPTURE &&
	    crop->type != V4L2_BUF_TYPE_VIDEO_OVERLAY)
		return -EINVAL;

	/* No fh->do_crop = 1; because btv->crop[1] may be
	   inconsistent with fh->width or fh->height and apps
	   do not expect a change here. */

	crop->c = btv->crop[!!fh->do_crop].rect;

	return 0;
}

static int bttv_s_crop(struct file *file, void *f, struct v4l2_crop *crop)
{
	struct bttv_fh *fh = f;
	struct bttv *btv = fh->btv;
	const struct v4l2_rect *b;
	int retval;
	struct bttv_crop c;
	__s32 b_left;
	__s32 b_top;
	__s32 b_right;
	__s32 b_bottom;

	if (crop->type != V4L2_BUF_TYPE_VIDEO_CAPTURE &&
	    crop->type != V4L2_BUF_TYPE_VIDEO_OVERLAY)
		return -EINVAL;

	retval = v4l2_prio_check(&btv->prio, &fh->prio);
	if (0 != retval)
		return retval;

	/* Make sure tvnorm, vbi_end and the current cropping
	   parameters remain consistent until we're done. Note
	   read() may change vbi_end in check_alloc_btres(). */
	mutex_lock(&btv->lock);

	retval = -EBUSY;

	if (locked_btres(fh->btv, VIDEO_RESOURCES)) {
		mutex_unlock(&btv->lock);
		return retval;
	}

	b = &bttv_tvnorms[btv->tvnorm].cropcap.bounds;

	b_left = b->left;
	b_right = b_left + b->width;
	b_bottom = b->top + b->height;

	b_top = max(b->top, btv->vbi_end);
	if (b_top + 32 >= b_bottom) {
		mutex_unlock(&btv->lock);
		return retval;
	}

	/* Min. scaled size 48 x 32. */
	c.rect.left = clamp(crop->c.left, b_left, b_right - 48);
	c.rect.left = min(c.rect.left, (__s32) MAX_HDELAY);

	c.rect.width = clamp(crop->c.width,
			     48, b_right - c.rect.left);

	c.rect.top = clamp(crop->c.top, b_top, b_bottom - 32);
	/* Top and height must be a multiple of two. */
	c.rect.top = (c.rect.top + 1) & ~1;

	c.rect.height = clamp(crop->c.height,
			      32, b_bottom - c.rect.top);
	c.rect.height = (c.rect.height + 1) & ~1;

	bttv_crop_calc_limits(&c);

	btv->crop[1] = c;

	mutex_unlock(&btv->lock);

	fh->do_crop = 1;

	mutex_lock(&fh->cap.vb_lock);

	if (fh->width < c.min_scaled_width) {
		fh->width = c.min_scaled_width;
		btv->init.width = c.min_scaled_width;
	} else if (fh->width > c.max_scaled_width) {
		fh->width = c.max_scaled_width;
		btv->init.width = c.max_scaled_width;
	}

	if (fh->height < c.min_scaled_height) {
		fh->height = c.min_scaled_height;
		btv->init.height = c.min_scaled_height;
	} else if (fh->height > c.max_scaled_height) {
		fh->height = c.max_scaled_height;
		btv->init.height = c.max_scaled_height;
	}

	mutex_unlock(&fh->cap.vb_lock);

	return 0;
}

static int bttv_g_audio(struct file *file, void *priv, struct v4l2_audio *a)
{
	if (unlikely(a->index))
		return -EINVAL;

	strcpy(a->name, "audio");
	return 0;
}

static int bttv_s_audio(struct file *file, void *priv, struct v4l2_audio *a)
{
	if (unlikely(a->index))
		return -EINVAL;

	return 0;
}

static ssize_t bttv_read(struct file *file, char __user *data,
			 size_t count, loff_t *ppos)
{
	struct bttv_fh *fh = file->private_data;
	int retval = 0;

	if (fh->btv->errors)
		bttv_reinit_bt848(fh->btv);
	dprintk("bttv%d: read count=%d type=%s\n",
		fh->btv->c.nr,(int)count,v4l2_type_names[fh->type]);

	switch (fh->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		if (!check_alloc_btres(fh->btv, fh, RESOURCE_VIDEO_READ)) {
			/* VIDEO_READ in use by another fh,
			   or VIDEO_STREAM by any fh. */
			return -EBUSY;
		}
		retval = videobuf_read_one(&fh->cap, data, count, ppos,
					   file->f_flags & O_NONBLOCK);
		free_btres(fh->btv, fh, RESOURCE_VIDEO_READ);
		break;
	case V4L2_BUF_TYPE_VBI_CAPTURE:
		if (!check_alloc_btres(fh->btv,fh,RESOURCE_VBI))
			return -EBUSY;
		retval = videobuf_read_stream(&fh->vbi, data, count, ppos, 1,
					      file->f_flags & O_NONBLOCK);
		break;
	default:
		BUG();
	}
	return retval;
}

static unsigned int bttv_poll(struct file *file, poll_table *wait)
{
	struct bttv_fh *fh = file->private_data;
	struct bttv_buffer *buf;
	enum v4l2_field field;
	unsigned int rc = POLLERR;

	if (V4L2_BUF_TYPE_VBI_CAPTURE == fh->type) {
		if (!check_alloc_btres(fh->btv,fh,RESOURCE_VBI))
			return POLLERR;
		return videobuf_poll_stream(file, &fh->vbi, wait);
	}

	if (check_btres(fh,RESOURCE_VIDEO_STREAM)) {
		mutex_lock(&fh->cap.vb_lock);
		/* streaming capture */
		if (list_empty(&fh->cap.stream))
			goto err;
		buf = list_entry(fh->cap.stream.next,struct bttv_buffer,vb.stream);
	} else {
		/* read() capture */
		mutex_lock(&fh->cap.vb_lock);
		if (NULL == fh->cap.read_buf) {
			/* need to capture a new frame */
			if (locked_btres(fh->btv,RESOURCE_VIDEO_STREAM))
				goto err;
			fh->cap.read_buf = videobuf_sg_alloc(fh->cap.msize);
			if (NULL == fh->cap.read_buf)
				goto err;
			fh->cap.read_buf->memory = V4L2_MEMORY_USERPTR;
			field = videobuf_next_field(&fh->cap);
			if (0 != fh->cap.ops->buf_prepare(&fh->cap,fh->cap.read_buf,field)) {
				kfree (fh->cap.read_buf);
				fh->cap.read_buf = NULL;
				goto err;
			}
			fh->cap.ops->buf_queue(&fh->cap,fh->cap.read_buf);
			fh->cap.read_off = 0;
		}
		mutex_unlock(&fh->cap.vb_lock);
		buf = (struct bttv_buffer*)fh->cap.read_buf;
	}

	poll_wait(file, &buf->vb.done, wait);
	if (buf->vb.state == VIDEOBUF_DONE ||
	    buf->vb.state == VIDEOBUF_ERROR)
		rc =  POLLIN|POLLRDNORM;
	else
		rc = 0;
err:
	mutex_unlock(&fh->cap.vb_lock);
	return rc;
}

static int bttv_open(struct file *file)
{
	int minor = video_devdata(file)->minor;
	struct bttv *btv = video_drvdata(file);
	struct bttv_fh *fh;
	enum v4l2_buf_type type = 0;

	dprintk(KERN_DEBUG "bttv: open minor=%d\n",minor);

	lock_kernel();
	if (btv->video_dev->minor == minor) {
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	} else if (btv->vbi_dev->minor == minor) {
		type = V4L2_BUF_TYPE_VBI_CAPTURE;
	} else {
		WARN_ON(1);
		unlock_kernel();
		return -ENODEV;
	}

	dprintk(KERN_DEBUG "bttv%d: open called (type=%s)\n",
		btv->c.nr,v4l2_type_names[type]);

	/* allocate per filehandle data */
	fh = kmalloc(sizeof(*fh),GFP_KERNEL);
	if (NULL == fh) {
		unlock_kernel();
		return -ENOMEM;
	}
	file->private_data = fh;
	*fh = btv->init;
	fh->type = type;
	fh->ov.setup_ok = 0;
	v4l2_prio_open(&btv->prio,&fh->prio);

	videobuf_queue_sg_init(&fh->cap, &bttv_video_qops,
			    &btv->c.pci->dev, &btv->s_lock,
			    V4L2_BUF_TYPE_VIDEO_CAPTURE,
			    V4L2_FIELD_INTERLACED,
			    sizeof(struct bttv_buffer),
			    fh);
	videobuf_queue_sg_init(&fh->vbi, &bttv_vbi_qops,
			    &btv->c.pci->dev, &btv->s_lock,
			    V4L2_BUF_TYPE_VBI_CAPTURE,
			    V4L2_FIELD_SEQ_TB,
			    sizeof(struct bttv_buffer),
			    fh);
	set_tvnorm(btv,btv->tvnorm);
	set_input(btv, btv->input, btv->tvnorm);

	btv->users++;

	/* The V4L2 spec requires one global set of cropping parameters
	   which only change on request. These are stored in btv->crop[1].
	   However for compatibility with V4L apps and cropping unaware
	   V4L2 apps we now reset the cropping parameters as seen through
	   this fh, which is to say VIDIOC_G_CROP and scaling limit checks
	   will use btv->crop[0], the default cropping parameters for the
	   current video standard, and VIDIOC_S_FMT will not implicitely
	   change the cropping parameters until VIDIOC_S_CROP has been
	   called. */
	fh->do_crop = !reset_crop; /* module parameter */

	/* Likewise there should be one global set of VBI capture
	   parameters, but for compatibility with V4L apps and earlier
	   driver versions each fh has its own parameters. */
	bttv_vbi_fmt_reset(&fh->vbi_fmt, btv->tvnorm);

	bttv_field_count(btv);
	unlock_kernel();
	return 0;
}

static int bttv_release(struct file *file)
{
	struct bttv_fh *fh = file->private_data;
	struct bttv *btv = fh->btv;

	/* turn off overlay */
	if (check_btres(fh, RESOURCE_OVERLAY))
		bttv_switch_overlay(btv,fh,NULL);

	/* stop video capture */
	if (check_btres(fh, RESOURCE_VIDEO_STREAM)) {
		videobuf_streamoff(&fh->cap);
		free_btres(btv,fh,RESOURCE_VIDEO_STREAM);
	}
	if (fh->cap.read_buf) {
		buffer_release(&fh->cap,fh->cap.read_buf);
		kfree(fh->cap.read_buf);
	}
	if (check_btres(fh, RESOURCE_VIDEO_READ)) {
		free_btres(btv, fh, RESOURCE_VIDEO_READ);
	}

	/* stop vbi capture */
	if (check_btres(fh, RESOURCE_VBI)) {
		videobuf_stop(&fh->vbi);
		free_btres(btv,fh,RESOURCE_VBI);
	}

	/* free stuff */
	videobuf_mmap_free(&fh->cap);
	videobuf_mmap_free(&fh->vbi);
	v4l2_prio_close(&btv->prio,&fh->prio);
	file->private_data = NULL;
	kfree(fh);

	btv->users--;
	bttv_field_count(btv);

	if (!btv->users)
		audio_mute(btv, 1);

	return 0;
}

static int
bttv_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct bttv_fh *fh = file->private_data;

	dprintk("bttv%d: mmap type=%s 0x%lx+%ld\n",
		fh->btv->c.nr, v4l2_type_names[fh->type],
		vma->vm_start, vma->vm_end - vma->vm_start);
	return videobuf_mmap_mapper(bttv_queue(fh),vma);
}

static const struct v4l2_file_operations bttv_fops =
{
	.owner	  = THIS_MODULE,
	.open	  = bttv_open,
	.release  = bttv_release,
	.ioctl	  = video_ioctl2,
	.read	  = bttv_read,
	.mmap	  = bttv_mmap,
	.poll     = bttv_poll,
};

static const struct v4l2_ioctl_ops bttv_ioctl_ops = {
	.vidioc_querycap                = bttv_querycap,
	.vidioc_enum_fmt_vid_cap        = bttv_enum_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap           = bttv_g_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap         = bttv_try_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap           = bttv_s_fmt_vid_cap,
	.vidioc_enum_fmt_vid_overlay    = bttv_enum_fmt_vid_overlay,
	.vidioc_g_fmt_vid_overlay       = bttv_g_fmt_vid_overlay,
	.vidioc_try_fmt_vid_overlay     = bttv_try_fmt_vid_overlay,
	.vidioc_s_fmt_vid_overlay       = bttv_s_fmt_vid_overlay,
	.vidioc_g_fmt_vbi_cap           = bttv_g_fmt_vbi_cap,
	.vidioc_try_fmt_vbi_cap         = bttv_try_fmt_vbi_cap,
	.vidioc_s_fmt_vbi_cap           = bttv_s_fmt_vbi_cap,
	.vidioc_g_audio                 = bttv_g_audio,
	.vidioc_s_audio                 = bttv_s_audio,
	.vidioc_cropcap                 = bttv_cropcap,
	.vidioc_reqbufs                 = bttv_reqbufs,
	.vidioc_querybuf                = bttv_querybuf,
	.vidioc_qbuf                    = bttv_qbuf,
	.vidioc_dqbuf                   = bttv_dqbuf,
	.vidioc_s_std                   = bttv_s_std,
	.vidioc_enum_input              = bttv_enum_input,
	.vidioc_g_input                 = bttv_g_input,
	.vidioc_s_input                 = bttv_s_input,
	.vidioc_queryctrl               = bttv_queryctrl,
	.vidioc_g_ctrl                  = bttv_g_ctrl,
	.vidioc_s_ctrl                  = bttv_s_ctrl,
	.vidioc_streamon                = bttv_streamon,
	.vidioc_streamoff               = bttv_streamoff,
	.vidioc_g_tuner                 = bttv_g_tuner,
	.vidioc_s_tuner                 = bttv_s_tuner,
#ifdef CONFIG_VIDEO_V4L1_COMPAT
	.vidiocgmbuf                    = vidiocgmbuf,
#endif
	.vidioc_g_crop                  = bttv_g_crop,
	.vidioc_s_crop                  = bttv_s_crop,
	.vidioc_g_fbuf                  = bttv_g_fbuf,
	.vidioc_s_fbuf                  = bttv_s_fbuf,
	.vidioc_overlay                 = bttv_overlay,
	.vidioc_g_priority              = bttv_g_priority,
	.vidioc_s_priority              = bttv_s_priority,
	.vidioc_g_parm                  = bttv_g_parm,
	.vidioc_g_frequency             = bttv_g_frequency,
	.vidioc_s_frequency             = bttv_s_frequency,
	.vidioc_log_status		= bttv_log_status,
	.vidioc_querystd		= bttv_querystd,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.vidioc_g_register		= bttv_g_register,
	.vidioc_s_register		= bttv_s_register,
#endif
};

static struct video_device bttv_video_template = {
	.fops         = &bttv_fops,
	.minor        = -1,
	.ioctl_ops    = &bttv_ioctl_ops,
	.tvnorms      = BTTV_NORMS,
	.current_norm = V4L2_STD_PAL,
};

/* ----------------------------------------------------------------------- */
/* radio interface                                                         */

static int radio_open(struct file *file)
{
	int minor = video_devdata(file)->minor;
	struct bttv *btv = video_drvdata(file);
	struct bttv_fh *fh;

	dprintk("bttv: open minor=%d\n",minor);

	lock_kernel();
	WARN_ON(btv->radio_dev && btv->radio_dev->minor != minor);
	if (!btv->radio_dev || btv->radio_dev->minor != minor) {
		unlock_kernel();
		return -ENODEV;
	}

	dprintk("bttv%d: open called (radio)\n",btv->c.nr);

	/* allocate per filehandle data */
	fh = kmalloc(sizeof(*fh), GFP_KERNEL);
	if (NULL == fh) {
		unlock_kernel();
		return -ENOMEM;
	}
	file->private_data = fh;
	*fh = btv->init;
	v4l2_prio_open(&btv->prio, &fh->prio);

	mutex_lock(&btv->lock);

	btv->radio_user++;

	bttv_call_all(btv, tuner, s_radio);
	audio_input(btv,TVAUDIO_INPUT_RADIO);

	mutex_unlock(&btv->lock);
	unlock_kernel();
	return 0;
}

static int radio_release(struct file *file)
{
	struct bttv_fh *fh = file->private_data;
	struct bttv *btv = fh->btv;
	struct rds_command cmd;

	v4l2_prio_close(&btv->prio,&fh->prio);
	file->private_data = NULL;
	kfree(fh);

	btv->radio_user--;

	bttv_call_all(btv, core, ioctl, RDS_CMD_CLOSE, &cmd);

	return 0;
}

static int radio_querycap(struct file *file, void *priv,
					struct v4l2_capability *cap)
{
	struct bttv_fh *fh = priv;
	struct bttv *btv = fh->btv;

	strcpy(cap->driver, "bttv");
	strlcpy(cap->card, btv->radio_dev->name, sizeof(cap->card));
	sprintf(cap->bus_info, "PCI:%s", pci_name(btv->c.pci));
	cap->version = BTTV_VERSION_CODE;
	cap->capabilities = V4L2_CAP_TUNER;

	return 0;
}

static int radio_g_tuner(struct file *file, void *priv, struct v4l2_tuner *t)
{
	struct bttv_fh *fh = priv;
	struct bttv *btv = fh->btv;

	if (btv->tuner_type == TUNER_ABSENT)
		return -EINVAL;
	if (0 != t->index)
		return -EINVAL;
	mutex_lock(&btv->lock);
	strcpy(t->name, "Radio");
	t->type = V4L2_TUNER_RADIO;

	bttv_call_all(btv, tuner, g_tuner, t);

	if (btv->audio_mode_gpio)
		btv->audio_mode_gpio(btv, t, 0);

	mutex_unlock(&btv->lock);

	return 0;
}

static int radio_enum_input(struct file *file, void *priv,
				struct v4l2_input *i)
{
	if (i->index != 0)
		return -EINVAL;

	strcpy(i->name, "Radio");
	i->type = V4L2_INPUT_TYPE_TUNER;

	return 0;
}

static int radio_g_audio(struct file *file, void *priv,
					struct v4l2_audio *a)
{
	if (unlikely(a->index))
		return -EINVAL;

	strcpy(a->name, "Radio");

	return 0;
}

static int radio_s_tuner(struct file *file, void *priv,
					struct v4l2_tuner *t)
{
	struct bttv_fh *fh = priv;
	struct bttv *btv = fh->btv;

	if (0 != t->index)
		return -EINVAL;

	bttv_call_all(btv, tuner, g_tuner, t);
	return 0;
}

static int radio_s_audio(struct file *file, void *priv,
					struct v4l2_audio *a)
{
	if (unlikely(a->index))
		return -EINVAL;

	return 0;
}

static int radio_s_input(struct file *filp, void *priv, unsigned int i)
{
	if (unlikely(i))
		return -EINVAL;

	return 0;
}

static int radio_s_std(struct file *file, void *fh, v4l2_std_id *norm)
{
	return 0;
}

static int radio_queryctrl(struct file *file, void *priv,
					struct v4l2_queryctrl *c)
{
	const struct v4l2_queryctrl *ctrl;

	if (c->id <  V4L2_CID_BASE ||
			c->id >= V4L2_CID_LASTP1)
		return -EINVAL;

	if (c->id == V4L2_CID_AUDIO_MUTE) {
		ctrl = ctrl_by_id(c->id);
		*c = *ctrl;
	} else
		*c = no_ctl;

	return 0;
}

static int radio_g_input(struct file *filp, void *priv, unsigned int *i)
{
	*i = 0;
	return 0;
}

static ssize_t radio_read(struct file *file, char __user *data,
			 size_t count, loff_t *ppos)
{
	struct bttv_fh *fh = file->private_data;
	struct bttv *btv = fh->btv;
	struct rds_command cmd;
	cmd.block_count = count/3;
	cmd.buffer = data;
	cmd.instance = file;
	cmd.result = -ENODEV;

	bttv_call_all(btv, core, ioctl, RDS_CMD_READ, &cmd);

	return cmd.result;
}

static unsigned int radio_poll(struct file *file, poll_table *wait)
{
	struct bttv_fh *fh = file->private_data;
	struct bttv *btv = fh->btv;
	struct rds_command cmd;
	cmd.instance = file;
	cmd.event_list = wait;
	cmd.result = -ENODEV;
	bttv_call_all(btv, core, ioctl, RDS_CMD_POLL, &cmd);

	return cmd.result;
}

static const struct v4l2_file_operations radio_fops =
{
	.owner	  = THIS_MODULE,
	.open	  = radio_open,
	.read     = radio_read,
	.release  = radio_release,
	.ioctl	  = video_ioctl2,
	.poll     = radio_poll,
};

static const struct v4l2_ioctl_ops radio_ioctl_ops = {
	.vidioc_querycap        = radio_querycap,
	.vidioc_g_tuner         = radio_g_tuner,
	.vidioc_enum_input      = radio_enum_input,
	.vidioc_g_audio         = radio_g_audio,
	.vidioc_s_tuner         = radio_s_tuner,
	.vidioc_s_audio         = radio_s_audio,
	.vidioc_s_input         = radio_s_input,
	.vidioc_s_std           = radio_s_std,
	.vidioc_queryctrl       = radio_queryctrl,
	.vidioc_g_input         = radio_g_input,
	.vidioc_g_ctrl          = bttv_g_ctrl,
	.vidioc_s_ctrl          = bttv_s_ctrl,
	.vidioc_g_frequency     = bttv_g_frequency,
	.vidioc_s_frequency     = bttv_s_frequency,
};

static struct video_device radio_template = {
	.fops      = &radio_fops,
	.minor     = -1,
	.ioctl_ops = &radio_ioctl_ops,
};

/* ----------------------------------------------------------------------- */
/* some debug code                                                         */

static int bttv_risc_decode(u32 risc)
{
	static char *instr[16] = {
		[ BT848_RISC_WRITE     >> 28 ] = "write",
		[ BT848_RISC_SKIP      >> 28 ] = "skip",
		[ BT848_RISC_WRITEC    >> 28 ] = "writec",
		[ BT848_RISC_JUMP      >> 28 ] = "jump",
		[ BT848_RISC_SYNC      >> 28 ] = "sync",
		[ BT848_RISC_WRITE123  >> 28 ] = "write123",
		[ BT848_RISC_SKIP123   >> 28 ] = "skip123",
		[ BT848_RISC_WRITE1S23 >> 28 ] = "write1s23",
	};
	static int incr[16] = {
		[ BT848_RISC_WRITE     >> 28 ] = 2,
		[ BT848_RISC_JUMP      >> 28 ] = 2,
		[ BT848_RISC_SYNC      >> 28 ] = 2,
		[ BT848_RISC_WRITE123  >> 28 ] = 5,
		[ BT848_RISC_SKIP123   >> 28 ] = 2,
		[ BT848_RISC_WRITE1S23 >> 28 ] = 3,
	};
	static char *bits[] = {
		"be0",  "be1",  "be2",  "be3/resync",
		"set0", "set1", "set2", "set3",
		"clr0", "clr1", "clr2", "clr3",
		"irq",  "res",  "eol",  "sol",
	};
	int i;

	printk("0x%08x [ %s", risc,
	       instr[risc >> 28] ? instr[risc >> 28] : "INVALID");
	for (i = ARRAY_SIZE(bits)-1; i >= 0; i--)
		if (risc & (1 << (i + 12)))
			printk(" %s",bits[i]);
	printk(" count=%d ]\n", risc & 0xfff);
	return incr[risc >> 28] ? incr[risc >> 28] : 1;
}

static void bttv_risc_disasm(struct bttv *btv,
			     struct btcx_riscmem *risc)
{
	unsigned int i,j,n;

	printk("%s: risc disasm: %p [dma=0x%08lx]\n",
	       btv->c.v4l2_dev.name, risc->cpu, (unsigned long)risc->dma);
	for (i = 0; i < (risc->size >> 2); i += n) {
		printk("%s:   0x%lx: ", btv->c.v4l2_dev.name,
		       (unsigned long)(risc->dma + (i<<2)));
		n = bttv_risc_decode(le32_to_cpu(risc->cpu[i]));
		for (j = 1; j < n; j++)
			printk("%s:   0x%lx: 0x%08x [ arg #%d ]\n",
			       btv->c.v4l2_dev.name, (unsigned long)(risc->dma + ((i+j)<<2)),
			       risc->cpu[i+j], j);
		if (0 == risc->cpu[i])
			break;
	}
}

static void bttv_print_riscaddr(struct bttv *btv)
{
	printk("  main: %08Lx\n",
	       (unsigned long long)btv->main.dma);
	printk("  vbi : o=%08Lx e=%08Lx\n",
	       btv->cvbi ? (unsigned long long)btv->cvbi->top.dma : 0,
	       btv->cvbi ? (unsigned long long)btv->cvbi->bottom.dma : 0);
	printk("  cap : o=%08Lx e=%08Lx\n",
	       btv->curr.top    ? (unsigned long long)btv->curr.top->top.dma : 0,
	       btv->curr.bottom ? (unsigned long long)btv->curr.bottom->bottom.dma : 0);
	printk("  scr : o=%08Lx e=%08Lx\n",
	       btv->screen ? (unsigned long long)btv->screen->top.dma : 0,
	       btv->screen ? (unsigned long long)btv->screen->bottom.dma : 0);
	bttv_risc_disasm(btv, &btv->main);
}

/* ----------------------------------------------------------------------- */
/* irq handler                                                             */

static char *irq_name[] = {
	"FMTCHG",  // format change detected (525 vs. 625)
	"VSYNC",   // vertical sync (new field)
	"HSYNC",   // horizontal sync
	"OFLOW",   // chroma/luma AGC overflow
	"HLOCK",   // horizontal lock changed
	"VPRES",   // video presence changed
	"6", "7",
	"I2CDONE", // hw irc operation finished
	"GPINT",   // gpio port triggered irq
	"10",
	"RISCI",   // risc instruction triggered irq
	"FBUS",    // pixel data fifo dropped data (high pci bus latencies)
	"FTRGT",   // pixel data fifo overrun
	"FDSR",    // fifo data stream resyncronisation
	"PPERR",   // parity error (data transfer)
	"RIPERR",  // parity error (read risc instructions)
	"PABORT",  // pci abort
	"OCERR",   // risc instruction error
	"SCERR",   // syncronisation error
};

static void bttv_print_irqbits(u32 print, u32 mark)
{
	unsigned int i;

	printk("bits:");
	for (i = 0; i < ARRAY_SIZE(irq_name); i++) {
		if (print & (1 << i))
			printk(" %s",irq_name[i]);
		if (mark & (1 << i))
			printk("*");
	}
}

static void bttv_irq_debug_low_latency(struct bttv *btv, u32 rc)
{
	printk("bttv%d: irq: skipped frame [main=%lx,o_vbi=%lx,o_field=%lx,rc=%lx]\n",
	       btv->c.nr,
	       (unsigned long)btv->main.dma,
	       (unsigned long)le32_to_cpu(btv->main.cpu[RISC_SLOT_O_VBI+1]),
	       (unsigned long)le32_to_cpu(btv->main.cpu[RISC_SLOT_O_FIELD+1]),
	       (unsigned long)rc);

	if (0 == (btread(BT848_DSTATUS) & BT848_DSTATUS_HLOC)) {
		printk("bttv%d: Oh, there (temporarely?) is no input signal. "
		       "Ok, then this is harmless, don't worry ;)\n",
		       btv->c.nr);
		return;
	}
	printk("bttv%d: Uhm. Looks like we have unusual high IRQ latencies.\n",
	       btv->c.nr);
	printk("bttv%d: Lets try to catch the culpit red-handed ...\n",
	       btv->c.nr);
	dump_stack();
}

static int
bttv_irq_next_video(struct bttv *btv, struct bttv_buffer_set *set)
{
	struct bttv_buffer *item;

	memset(set,0,sizeof(*set));

	/* capture request ? */
	if (!list_empty(&btv->capture)) {
		set->frame_irq = 1;
		item = list_entry(btv->capture.next, struct bttv_buffer, vb.queue);
		if (V4L2_FIELD_HAS_TOP(item->vb.field))
			set->top    = item;
		if (V4L2_FIELD_HAS_BOTTOM(item->vb.field))
			set->bottom = item;

		/* capture request for other field ? */
		if (!V4L2_FIELD_HAS_BOTH(item->vb.field) &&
		    (item->vb.queue.next != &btv->capture)) {
			item = list_entry(item->vb.queue.next, struct bttv_buffer, vb.queue);
			/* Mike Isely <isely@pobox.com> - Only check
			 * and set up the bottom field in the logic
			 * below.  Don't ever do the top field.  This
			 * of course means that if we set up the
			 * bottom field in the above code that we'll
			 * actually skip a field.  But that's OK.
			 * Having processed only a single buffer this
			 * time, then the next time around the first
			 * available buffer should be for a top field.
			 * That will then cause us here to set up a
			 * top then a bottom field in the normal way.
			 * The alternative to this understanding is
			 * that we set up the second available buffer
			 * as a top field, but that's out of order
			 * since this driver always processes the top
			 * field first - the effect will be the two
			 * buffers being returned in the wrong order,
			 * with the second buffer also being delayed
			 * by one field time (owing to the fifo nature
			 * of videobuf).  Worse still, we'll be stuck
			 * doing fields out of order now every time
			 * until something else causes a field to be
			 * dropped.  By effectively forcing a field to
			 * drop this way then we always get back into
			 * sync within a single frame time.  (Out of
			 * order fields can screw up deinterlacing
			 * algorithms.) */
			if (!V4L2_FIELD_HAS_BOTH(item->vb.field)) {
				if (NULL == set->bottom &&
				    V4L2_FIELD_BOTTOM == item->vb.field) {
					set->bottom = item;
				}
				if (NULL != set->top  &&  NULL != set->bottom)
					set->top_irq = 2;
			}
		}
	}

	/* screen overlay ? */
	if (NULL != btv->screen) {
		if (V4L2_FIELD_HAS_BOTH(btv->screen->vb.field)) {
			if (NULL == set->top && NULL == set->bottom) {
				set->top    = btv->screen;
				set->bottom = btv->screen;
			}
		} else {
			if (V4L2_FIELD_TOP == btv->screen->vb.field &&
			    NULL == set->top) {
				set->top = btv->screen;
			}
			if (V4L2_FIELD_BOTTOM == btv->screen->vb.field &&
			    NULL == set->bottom) {
				set->bottom = btv->screen;
			}
		}
	}

	dprintk("bttv%d: next set: top=%p bottom=%p [screen=%p,irq=%d,%d]\n",
		btv->c.nr,set->top, set->bottom,
		btv->screen,set->frame_irq,set->top_irq);
	return 0;
}

static void
bttv_irq_wakeup_video(struct bttv *btv, struct bttv_buffer_set *wakeup,
		      struct bttv_buffer_set *curr, unsigned int state)
{
	struct timeval ts;

	do_gettimeofday(&ts);

	if (wakeup->top == wakeup->bottom) {
		if (NULL != wakeup->top && curr->top != wakeup->top) {
			if (irq_debug > 1)
				printk("bttv%d: wakeup: both=%p\n",btv->c.nr,wakeup->top);
			wakeup->top->vb.ts = ts;
			wakeup->top->vb.field_count = btv->field_count;
			wakeup->top->vb.state = state;
			wake_up(&wakeup->top->vb.done);
		}
	} else {
		if (NULL != wakeup->top && curr->top != wakeup->top) {
			if (irq_debug > 1)
				printk("bttv%d: wakeup: top=%p\n",btv->c.nr,v - Bt->top);
			er

    Cop->vb.ts = tsright (C) 1996,97,98 field_count = abber-koeln.de>
Metzler <rjkm@thp.unistate = us MeMetzler <_up(&er <rjkm@thp.unidoneyMetz}
		if (NULL !=ter

    bottom && currde lines e v4l2 coen from ) {etzlC) 1irq_debug > 1)etzl	print/*C) 199
   r

  84 from rame gr   &  driv (C) 199v2
    Metzler <rjkm from p.uniRalph  Metzler <rjkm005-2006 NiMarcus Metz			   & Marcus V. Shmyremocm@thpshmyrev@yanus Metz 199(c<rjkm9yrev2 Gerd Knorrshmyrev@yan@bytesex.org}
}

us Mic voidC) 19_s
  tin@su_vbi(struct 6 Mau*btv, pesex.99-200buffer *tin@su,
 Fixe unsigned int Mauro 
{
	2006 Mitimevalay V.
>C) 199som=stin's b0 Jureturn6 Mado_getysteofday(&tsrom:in's btt Nickolay V.  and/or moddex.ru>

    Fixes to be fully V4Lunder the c) 2006 Mauro Carlho Chehab <mcheadead
   >roppingicen ovichael supn redut(gmx.at>
 long datared by OPQ Ser <r2005 = o2006 Michae 2) you;ption) any lal H. Sc_set old,newram is distribut>
    *ovbiat it will be useful,
 item;
	se, or6 MauratflagC) 199Thi be uverbosever whin SchoKERN_INFO eman <jushe Lice: dr fradis
 =%d/%d, risc=%08x, "schimek@  Fixes 
    Fixes te.upPURP there  supme6 MauYou shtotaibutPublicICULread(BT848_RISC_COUNT)e it
 be uBILIT of bits(ARTIGNU GenerINT_STAT),0icensBILITY e\n"e it}

	spin_lockg wisave(&   & sounda,plied)6 Ma/* deactiv Metstuff */
	memset(&new,0,sizeof(newCULAnold   Fixes  takncluvbi<linux/mo  but ude <ldul =  that ude ncludincl sonux/errxloopg wi = 0nux/ee useful,
MA 02139_video(005, nux/e ite.h>inux/errx/kernel.hbisched.h.h>
#inclschedset_dmasched.te tridgetin@ up
*/

rscan support6 Ma>
#x/smp_lohopenux/e, VIDEOBUF_ERROR
#includeev_t.h>
#inc Cocludeludeed.h>media/tin'-com.h>
#cancel all outstandithecapture / errnrequests <linwhile (!list_emptyc., 675ia/msp3)CHANTA; wi = ma eitntryinclched.h>as.next,ter 6 Mion 2 ted in, vb.queuorg>

de <adel(&
#inre Fo>
    Sbtt* numa>

c) 2006 l.h>
#include Metzthe Free in use */ad.org>
}clude dma-de <asgck.hude "bvr.h>

#m/iock.h>x/smp_lo1;
und.orordese>
 1;
int bttv_>
#incrdsbttv_
gmx.at>
    S
   _num;			/in use */of@sunt8sul,
use
*/
pyrightjusti*
   s[BTTV_MAX];_ENDIAN
static unsig  (ch>
#incerrors++at i  Founass m isrestorn

#incl Mass Ave, Cambn; either verseux/kdev_t.h>
#intopion.

    This005r oam is distributed in Publ <mnclude <ldul.top warrants progat it wfree software; <mesigndag;
static uns
#inclg var
ln.diux/f__BIG int int radio_nigned int btv_   G_hookinclud a coSLOT_O_FIELD,ck.h>errupt.hyou /kdereill be icense as pife WITunder the termsn=1;is program; i a copy of thense as popy shes */
#ifdeDONE clude FX] =S{ [0 ..TTV_MAXtioc@gmx.at>
  -1)] = -1 };radin; either infrom <meSisncludeeion.

    cxtic umem * -1 , u32 rcr optC) 1rc < <meG->dmaX] = { [0 V_MAX
    Sa>c_crush <me+t whitex/ing varadio_t lumstatic 1nt g H. Scalph8
static unsiwitchsched.hitecx/ins.h>x208000
static unsi.at>
   mbfillude <WITHOUT ANY WARRANmbfilhop;
	dma_addr_t rc... (;
#endif;
static unsignedh>
#new ned intbfil>
#includev_te.h>gned int bttv_clude rc  Fixunsigned int fdsr;

/*gned in
/* cor[0 ic un    ...static und int b radio_996,9ound_parrc)) ||
Publix.at>
    Spfrom Jc unig var
/* insmod argshmyrev@yfrom ,am(btCHANTA   Yodetails.
atic ed inauro _latencyree sonTV_MAX] <meintownt, 0644includrcicens lumafilteratic unsigned in = { [0 ..dio[B/*eln.dic ovANTY/ched.hrop = 1;

se.helayPI features (turn oturn onfs&= ~1features (turn onude <lin features (turn onon.h>
turn onint [0 ... >
#;

1;

inpu
    SC) 1UNSETJusttati{new_rese);
moduched._muxinclu 0444)_param(leoptio
     int, 06no.h>(irq_d,     )x/smp_lofinatic nsignedinux/ecoring;

ule_param(gbu

   p.h"d int bttv_>
#inc   Sp   S;
module_param(debug_latenigned iutolowercr_ha7F  = 1;

/*96,9bufsize = 0x208
static unsigned int irt 

moigned intWITHOUT ANY WARRANTY odul ic uns    int, 0444);
full_;
mo_rangeC) 1
static unsig    MERig varun
		t, 064nsiggpio_paramconfig variables
*/
#ifdef __BIG_ENDIANstatic unsfdsr,endiann <juton/off  USA.
for testing) unsigtic unss proe se = &&l2  ule_paramold/nt, 064_prcodule_ Public,          itv_gnge,        0444_paraar0644)t radiogaram(vcr_
module_param(coring,    s
    (c)aram(vcr_h
module_param(coring,      (c)nt, 0644aram(v
module_paraaram(rrq_isincl.h>
o.am(coring,    itecrush, 4e,      ule_param(gbufsize,  mp_lint varam(vcr_hnge,   int, 0444dule_paraaram(combfilttin'dule(combdule_pt, 0444);

moduint, 0444oring,    ;
module_tis
  [0 _t lumafirq(int, rq, ;
sti*ng;
idr optmoduus M,aus Ms 0 (node endia = 1 V. Shmyule_param(automuLE_PARMhandle_par0Uose sv=ion.d warrants padiod warranty = 1;
stomned hitertup messa_DESC      nt, M_DE      Metz=_MAX]
unsig1CHANTA/* get/clearLL,  [0 . Sponsus th t;
MOD	us M=ideo_nr[BT; if not, wr0644vMODU=us M&MODULE_PARM_DESC(MASKelse
sttfram enaram(creak0644g gpio chaLL, 	btwrit@gmxat, stanges, s
    (       in devic9, Uefaultit w0 (no
MODUM_DESose sta_DDirq_US.     ch i supauro CHANTABBILITY or FIDEBUGS FOR A PAuffeturnOSE.fceset"h,         "_crus=%xbuffeGsigned int fddsr;

/* optiont, 0 Metzl    Yodarcus Metz, d8");
MO"
t 8">>28or cunsigned int fdsr;

/* optisuld haa  (atwit ish nati (no) 0644)it wplic & PARM_DESC(HLOCK);
motthe Free mess)" => %s", (M_DES");
MODULzetaticyC(ch;
MODUring,  ? "yes" : "no{ [0 ed ialith older is 1 (yVPRES;C(gbufsize,"siES,"videoa_agcce Aam(whibfilAGCn=1;chr, default ARM_DESC(adc_E_PAR)")ables the luminanC(adt whitFMTCHGables the luminanNUMLDC crush, default is 1 (yes)");
07")ts the white whi625E_PAR525per,"sebfilter;
statict, 06ULE_P (no)&e crush uphSYN DULE_ixes to be fullyparaC(gbufsse staDESC(adc_crusGPINT)uffenges,remotRCHANTAdif
staticnges,nt, qomute,"iswimodul
MODULE_PAR7pper,"sets the DESC(adc_crusI2Cadio,HANTAB have 2c_ad.o06 Maur Carvalho Cheht, 04betwstatic unsper,"setsrpper,"sets the whi a cI defa _PARM_DES(4<<28)white the white  int, 0444)rush_uphite crush,         ,"tic bfilcrus   ine cape2_DESC(whitecrush_uppx.at>
    Sture bufcoring,"the bfildefauideo d levePARM_DESC(adc1_DESC(whitecrush_upper,"setsed int br, "video device numbers");
MC(ch);vel, dnges,opt_iltemusore			audil2, tee TV cnges,IPTIO;ridgetrigger ESCR mod;
MOrtup mfull_luma_PARM_DESC(iCERR|PARM_DESC(OV. Saram(coAt is 0x20800TNESMODULE_PAR%s%s @ gram;ac.za>

    th older apfull_luma_range,"us V. S);
MO V. Srush_uith older apfull_luma_range,"us fully;
MO full-                *pper,"sets the white cSCRIPwitch,"s ttv o on bad/missODULh>
#i siRM_DESC(adc_c27"_PARM_Dof
NULL, 
MODU"mute audio   G 50;ute
stairq ightdele_pull pper,"sets the whiFDSRhmyr&hehab <mchepper,"sets LICENS, stPLpper
p.acth older ap		 "dc_cru                         */

s_devvice_at be use *>c.t, char *bufred bpyrightvid
	pper,"     int, >

   > 4odulyrightdecard, NU8 || !ynch_t spoor VCR tapes),nt, 0,"rs," use *0,SC(adc_crusith old
MODULE_PARM20800ERR     ,"sieman <jusIRQ _MAXup, "  (c06 MauSmask ["
    V4
    0644)} else       to-load se gpi(MODULE)
static void request_module_ombf------froc un_struct *wotecrusdeviin             #if  (c) o chagesith o_AUT-1 ^-----------------deo_g			);
}

static vo);
M	}est_motatic ssize_t show_card(struct de video_dev"]			 stru);(&tvp.ave receivedparam(is g gpio pe : have recouparame_paramIRQ_RETVAL#NFIG
#deam(luaram-param                -------------------------------------------------;
MOram(bitialiODULonring,  (MODULE)
static void request_module_a              ----either ule_parm(tin'sWORK(&*vper,"nitdule_param(automu,s the whiteconss, dpecial timODULtam(wtng.h, 0c un8 SRAM_Tam(w[]
}
statype_na   Yoption) an{
	/* PAL digisfbufsivf_par{
	/* PAL di_alloc(g,    uv_rati== vsh_line_paramle_para0x11 =gital rese;x01,0->tin'ODUL = d int b.x00schi00,0x31,0releas u a,0x90x8902,0x84,0x7,0x89,0x0auro C    intv->c.ty0x89 timinturnrvprog(vfd------4);
nBILITf0x37->// 4, t, 0it.00
	},
	/), "BT%d%s %s (%s)nt fdse requd,ut eL, 0d==848M_DESC(adc_vision==0x12     A           	45, ,
	/
   tv_tv----s[.za>

 03,0].,
	/5ules(dev)
x11,0n; eitherramebuon 2 unregister@gmx.at>
    Svcr_hack  = 1;apture bu{
	/* PALnsignedf 
#int is 0 ({
	/* PAL->minorpe : x50x894x89,94,0x0ng tab	0x81,x03,x03,0xct vNFIG,0xA61,0x002,0x1,0x0581,14,0x TGB_5x89,74,0x4		0x00,0x86,0oring,    }4,0x		0x81,
bi,0x37,
		0ht
	/C
		0x		0x0ied toB_NTSC3 TGB0x21,
	x0D4,0x00,
,0x3713,02A, v D
	{  used foAC,0x211,0x05,0,
	l inp/ TGB_NT8, uart24,
42
	_hac4,
	nal,am(w has beenatic ratin@ to be used for Fusion R1024,
		0
		0x2A, // size of table = 42
		0x06, 0x0x2c, 0x14, 0x0a, 0xc0, 0x00, 0x18, 0x08, 0x03, 00,
	},
bf0x08,20, 0x30, 0x0, 0x90, 0x02}Srk)
D,0x02,0r Fusi4cludeith oldecrushafiltemut _e of module_paD,0x02,0x6, 06to bx02,0		0x80x3780x3708,no_paralay > 0 usethe Freeman : O0x04risv_t.h>
willam(wd.
statile_pa be u;
MODULE_Pht08, 0xes)"_MAXom;
moched.hvice_  timi0xC0,0x0t "e   w.h>
wi0x0D,1x21,
	,0x01,0x05,0x37
		goto e     	stan 1 /itable = 42
		0x06, 0x0SC392 //, VFL_TYPE_GRABBERt over Gls.
  nr1start4,nr]) <ng edg irqsquc unerd Knorr");
MODULE_LICENSD,0x02,0edith olden 1 /%druct dault "
		 "is 1 (y   Yoht
	// Thx0nu tot:      x00,
cre <lifilram(bttvstart0	ITU-eo ihe luminan&per,>c.t_x03,)<0o)");
t is 0x20800ERR          eigendian=1;numbei 'x03,',"rq_d cn; eifail	to nork)
{
	requesnt irqpulse (dule_pa.h>
n=1;MA 02e24,
	2, 0combivedwidth	nhdel  int ayx1,, both inbi fCLKx1.buffsqayx1,				 8, 0x04de verostart0)pixettv_remod int	gnedtopcap.bouv D
	{i	0x0*i-koeVBI relssingbi		trailODULedg of c/VRESET pulse (VDELAY 0x39,0x0)e counhe JustELAY, opcap.bounbi
		vdeI defrecd  int,. 23 *>
#isv D
	{
	RRPOSm, 0444)nges,has_0x2c,CF;
static unsinhdex2c,ropcap.bou	hack24,
	0 23 nhdelayx1,			0x2c,  because ve intx{ [0 we count field li00,
		0xbfs two, */	 \
	/* e.g. 23 * 2 to 23 * 2 + 576 0x2c, 0x1rgin at theRADIO * 2 to 0x2c, pcap.bounds.top = (videostart0) * 2 - (vdelay) + MIN_VDELAY, \
	/* 4 is a safe) -  margin at the end of the line.0x2cx07,11,.qwidcap/*] = { en u    e_param(uv
puls:size,"sx0D,0x02,0xtart o0x0a, 0e_param-tNTABt    on OpenFirmic unmachrom J(P,   Mac at, 0x7t), PCIoma_ory cycl.denonhdels def-- *Pze of on bano fGHI
*/

isSC(i enally ptionF/kdeia/msp3amebupcideo, commandt>
    SpNTS, 0x. Sc5 by sheight, v(__ping pc__)t_mo even 0x08,0mbufsi{
		 pro_
modul_dwordne nally _COMMAND, &cm_DES	cmx00, of 9|950,
	}.swid_MEMORY      NTS use            = 35468height      tecrus  #DULEfm(lumafiltemun a
	/* *and 23 hprob@gmx.at>
{ht  tin'_id * 2 t7:0] *[60] =ed b48_I00,
ebuf{
		.WORK(&1135resulULE_ne, or
  /	 \
l v gadule_param,"    MEE_PARM_Dof
numx03,;
#endifve video11,-ENOMEM) * 2 - (vdelay) + MIN_VD:@sunxxr ve3 fV_MA (%d)we cdeo, b0.cropce0x37 0x2ork_st*/
	ngm(whkzx20,
	NTSC dire on, GFP_2080DULE_Papture x03,kodul/G_MOht,  CROPCAP(miivisia:ESC( of922, buwe coun	.croM_XT1bipackremo->
#incnds.am  tablenuithoFc0, 0x0x0x5t     video, b12,*totaC drati0444 7, 320 l in	 int beman <jworkred b
#inclu_-rk)
44);

zult Metzs / fOUT in = Vtiveecrushv4l/xotalwiC(adc_unsigne Sposigned ip.defrectic unsignett, 0imag * 2 be   Fixuffechecks, s    _waiseful,_hprogture buffersiswt bi      =*/ 9 0x02nt bit50pper,"sets INIT_LIST_HEADam(bttv_.sub4);
d	.crop*/_hackqwidth *sttv_gpi
			57x21,
t biline. taram(whitec		0x00,
prio the old valx19,) - 4,tartn reridth */RTICULA_param(inRTICULA.funcct yonds. */, swidt* opV_MAs.
	/* 4 = (prog= 1135,nut even the i full  =,
			/*44     ms[]v4l2_iduner_03,0_STD		.hd, the oring,     i is dFsc            == (to=ap.piap.bounds.t 1135,
pci, USA.
(ve v,2-32,irq/mmio,t v4      = 28_.480,=, hd, NULL, 0
#inclng68,
oic used0,0totaing PA42
		0x0RM_Prt ev swidtfirst aWARNINC(gbufsize,Can'.boun PAx68,ela/M_XT fdsr;

/* optionfost
	/* *t wietIO 0x02, 0x08OPCAPct iigendi
		.adDMA_BIShould(32param(coOPCA| GenerIFORM_XT0)ght   caNo s		.vy   DMA availally		.910,
yx1  = 1135,128 is dhp.boun
		.vbis= 910,
!d int b_ma_a * 2orush.vresource_23 * 
		.a0 int bi78 (a  = 1,
	len	= { 1t biSho* d second,
		*/ 68,
n (160 144, /* min (1600 / 4, 0x1ffcedtwid int b iC cru(0x%llx*/

hdelayx1       = 128,rbecant fie_NTSC_M | V4L2tV4L2_uld be (640 */* st	= { 1	
		.hactivex1 BUSY bytes		.vdelamC0,00Gener		.nhdedelay768 - am(i= "    5,
	4,	 8,0x03,0video0x02,0his p  = 0  of a = { 7, 320 param(uv_5,
		. = (bistart44,idgemin (1600 
	/* 0x1ffelay         = 0x7f  ) ,	 \
		* 2 to,VDELAY, ,and bt8= 0x_MAX----63,
		.swidth   byt Geneadelay LASS_REVISION	.DELAY,x0xc0,C0= "SECAM, /* min (1600 1 4, 0x1fflLATENCY_TIMER, &luct deerd Knorr");
MODULE_LICENSBt%d "SEv %d)	to %s int ftruct *work)
himek@brid/  { 10, */
			,(1600,
	//* min     /* vi"irq: he
         780? P(/*:924,t birgin at the end of pcive rec10,
,
		CR35,
videostarnd bt848?t0/* s23), 0xBT848_IFORM_P vdelcludeulerail8_IFORM_tm; iP(/* = ioremaphdelay3)
	},{
		.v4l2_id 0)    =00ruptowe count field li	/* 4 
		/ = 0x7= 1135,gbufsize,"878 (an
		CRObe is diforork_struct 5,
		.ad		.vbist   = Gene  =        identify   = 0alwidthitcSECA0eo_nr, "vdge,/cally = 6s, */
				fiuffeg gpiog,    --------------------hould bd        = 80? */
	MODULE_/ 1135,
			/* hack - ,0x0/* tIRQF_SHARED |twidt7DISABLED	.iform  elayx1 */ 68,CR(a#deff,
			/* bt87SECAMlayx1,e           y   and secondistahdelayh   yx1, margin 	/* tota likh   L,);
M1135,
			/    = 640,
		.DELAY, ostarbttv_40tv_g gpio_chipched		/*x1a,
		.bdelay        = 64e is d2ayx1 */full lndop+ 2)sct x1 *_ttv_gpiodio,  isgbufsiDpports ,
		=* hdelayvdth     = 780,;
mo0x1a,
L_NC|
		/* bt878 (aDELAY,78 based ,
		78 basedart0 */ 23)
chro0 + g    _PAL_Mack 			/* sqwidthC(vcr_haIFORM63,
		.sart0 */ 23)
08,0x03,_IFOR       =art0 */ 23)
] = e
		.s_uppeb & 0xOPCAP(/*   = 
	ght   / 780,
			/*th   ldth         = 0x1a,
0x68art0 */ 23)
,0x1ic d_IFOR 2863636art0 */ 23)
crus,
		.     8_IFOR= 186,
		.hacti0 + tart0STD_oasync80,
		
		.vb 1135,
944 /ule_param(auke P/atio, WARRA) KR,
	.   Fix7, 32rC die c       =    	ty   b,
		ermiov.w.         32ndif
inhdetalwidth ELAY,	.ad24,
				nhde/ 57fm3636    = 11 286at_by_f
			c(tart0PIXpper_BGRAP(/*nds.he 10, 2-0x00,,
		.vbbi(/* minhdelayx1     O,
		.vbAP(,
		.a924,8636getaticShould ANY(768hard 754,qwid    ostaruffef the   = 9		_trackingyx1,	"pre- 10, 27swi
static unalwiimainn2863rangTD_PLAY, CAP(/* k   duffe       0x5d,
	x0---------GPIO_REG_INP              5,
		.adelay   OUT_ENparam(uvtal li   MERC);
moound			/* sqwidth */ 9			/* vdel  = 1eds0x04bepect.dbefoy, m2c max, \
	/* 4 itotalwidth0 */    1  = "PAL-N",      / 2) c +363,
		.swth     scal2c  = "PAL-N",		/*     -s0] =fld be     	.cropworwidtx1ff)40,
		.in (160e   N2,
		.nam = 0x20,
	     PAL-Nn.up.oundt vith */
		.hactivex1    848_uct depi+(modul_eight */!ram  size ofrailing },
		0xove vido)");
MO start0 */ 23 * 2 struct vyx1 */br    yx1,	3276835 +R,
	 /;
motras94sqwidth */     = 0x1huTV caould bidth */ 944sa,
			/* sqwidth ustin@ crotiming1			 \
	tstatic, 0444)ct? 754tvnorframe
t0 */qwidostahed.  = 0xrop[0]
			/* vdela   = 780        1U-.crop         ; she_gpi
    Fi     =ose staisclaim 044_ine this00, 0x10,
		.ade timinlwidth     =dule_paad>
#ibxel wk   nd	/* v&& dedvb-b
	/* ifam    640,
	y        x1 */ 68,
			/* hdela 0x1dvb8ostart0 */sub= 5 42
		0x06am        vba
		.hac		.vbisoCROP = "PAL-N",elch,"static i   = 0x1a      s 94yt		.vbis_par	.adeltart	= {     ,0x02,0= sq/is p:
	AX] N4, /* min (1600 / 4,.6,
			 */ 7:sp34ano       x0D,0x02,0tart0 */    = 910,57, 2730:s been modi_Ncram      bttoun0x1aart0 */135,
			/640,
0x81,			/* swidtvideostart0 */ 23)
	} = 0x20,
780 / 2)  (1600/ 0x6448_I910*/ 7eostart/*SC392 /x00,bipack  x04,0x19,0x00,
 0x72,, 0xiformdemnt lumafiltBT848_IFORM_XT1)5M_XTes folloight       *x00,
	},
v@yaTSg0x37301,0x0      tiFORM_X0, 273 },
		Colayosqwid(/ 910,
			/*ay         = 0x1a,
		e of
		/HRES<jusram(aasynp.ac.za>

   		.vbistahutdow0 + 780    16(DMA+IRQay  NTSC-J- n~15
		.adelay   1a,
CTed int          = 28636363,
		.sw(        ~0x		.iform     s
    (cNTSC-J7),
				.adelay         delay        idth */ 1135,
			/* sqwidth */ 9    ned(MO/ 2) /tell363,
	    35,0x68ke Plea6,
		ctivexay     = 768,
	gendi"sets the white alwidth1      = 75pararmined      =t   delrst an/8,
		= 78aledtwidx0D,0x02,0x1ff_buPAL_first an/*parasqwid   = 1,
		.vbist.ee.u    = { 7, 320 },
ck       t igned int b_     	.na{ 10, reeodulocated		   Thtrminednt");
C cr_* viayx1 */*/ 4,
		.sn	.vb/ 1135,
			/*re
		.ayxecrushatic  is dete23 * 2      = dela			/* sh +ght       ight */ 480,ay */ 0x20,
		/* "PAL-t0 */t fielRMS nsigned int
	.crop			/* sqwidt0 */ 23)
 */ 944"PAL- */ 910.vbista 780? */
			/* swidtvideostarttwidth 
	bounds.toring,    kIZE(bttv    = 2 [0 ..}lwidth_loCO0,0x_PMdia/msp34o      255spe- nilayx1 th the strange t, pm_
}

sta_s, def45 bytes follhich, 320 }recgned deo_nduce when o riither		.vd PAL*-----v_foa= {  TVt v4l vdela_IFORM_P         int, 0444);
ur, d AB
 e        he imve, CULE_d60,
		.name      hdela     = -
		 "is 1 (yus Me.    e_dela_FMTc unult is bytes0x1a,
signed inn, Iug;
static unsigned intd int btt    = FORNTSC di    /
/* , 273 dela 	 2 beca RGB, le",
		 is 0      bi = 113
	    int,	/* s.cc   =am(gbuffe.hnr,    int, NLL, 0444);

MODth    =hed.h>
#inclBTTV_MAX]    int, 0444);
module_param(gbuth */444);
module_param_array(vbi_nr, aptureatures (turn on 0444);
my         = 0x1a,
		.vbipack  @gmx.at>
    Ss
    (c)          int, 0444);COLOR_quesbt878 hdela	 * tape on   = ,
		.pth   =      int, 0444)iform         AGS_PACKED8, 0x{  = 780=alwid= 186videos 10,plie------ORMAT_FL		.vdav{
		.teOR_FMT_Y8,
		 */
/* b		.vdela
		.,.dep0x1a,
= 16* minhoad s	GS_PACKED,
	},so w  =       =  10,lay   42
		0x0R_FMT_Y8,
	felay.swi= V + 94L2_itecrus}ominato    s
		.hactivex1= */ 113sum*/ 91s on bad/eelay a NTSC* PA,
		.dw2_PIX_FMT_GREY,
		.btformat = BT848_COLOR_FMT_Y8,
		.depth    = 8,
		.flags    = FORMAT_F= "8 bpp, d0x19,ed colD,
	}ulse orsram  	.btformat ing
	wht   ra
		.+ 780 / 2) 
    (c*/ 76,
		.FMT_Rapture bube",
=  GenerC,
			/err=     = 1135,
		.BT_RGB565X0x03,0x8errruct video_device,  (1600 / 4, 0x1ffl		/* 
		.nam = "PAL-N924,
=ODULM= BT8IX_FMT_	0x20,
		.ulse (line..bB, be",
32 bpp RGOLOin (16_RGB24,
	OR_FMT_Rourcc   = V4L2_P
eighD      = 2= "32 bppformat =			/* s	},{
	_FMT_BGR32RGB, be",
		
	},{
	GB32alwidS_PACKED,
	pth    _FLAGS_PACKORMAX_FMT_RGB32,
	= "32 bpp RGB, be",
		
	},	.btform      RM_    = 1radioGS_PACKED,
	},,
	ORMAIX_FMT_RGRMAT_FLAGS_IX_FMT_opefuth     = 1135,"PA	FMT_,in opt0xf_FMT_HI280 /32,
		.btform        = FORe(
		.-btformat =.btswap  .sraB, be",
an 1 ma_PACKED,
	,
		.s= FORMAT_FLAGS_IX_FMT_RGB32,
 int, 0444);        16,
15 ANYturn onarray(4,
		.depth V4		 * t= FORMAT_FLAGalwid "32 bpp RGB,         int, 0444);
module_param(gbut	= { [0 
		. RGB, be_RGB24,
		.depth UYV",p   = 0x0  = 0x0f5_BGR32 packed, UYVwap   = 0x03, /* byteswap */
		.depth    = 1RGB sqwid.bts	.namedork)
ld be0] =
activex1      = 2863ci_tbl[ked {
	{eighVDEVICE(BROOKTREE2 bpp R2P,
	_ID_ram;  FOR},},{
YUV428_COL, packed, UYVY",PACKED,
	},{
Y9rCb42th    = 32,
		.fl
		.namormat = BT848_COLOR_7CS_PLANAR,
		.hshift   = 1,
		.vshift   = 0,
	},{
	urccLANAR,0,}
by c(gbufs   = FORT,
		wap , 0x1ff		.btsw = 12:2,48_Cnar, Y-Cb-Criing,swap */
	1th    ap   },
		,
		.vbC) 19		.h.id_A.
*/

eostar		.btswf RGB, le",
_PIX_28636363	},{crusy 
		.d	.crat on_
staopARRAly ),le_param(abledelay	.UYV",
		.vshift she2_PI
		.de = 2x02,0tableS_PLANAf   = "4e_PLANAR,
	68950,,
		.iform
		.PA,
		.    3  = 32,
113,
		.fourc   = 1		.Fsc ormat fo35,
		.ade2048= FORMAverayx1 %d.epth Cb-Crb0,
		d int BTTVCb-CrVER,
		_CODE >> 16oidLAGS__YUY2,
		.btswapwap   = .dep11P,8= 16,
		.flags    COLOR_FMT_YCrCb411cc   =x7f,640 * SNAPSHOT48_CD,
	}d, YUYV",
		.fcalenapshott yoe %04d-%02 "4:1:B24,
		.depth	.hT848_/RM_P0, (_RGB24,
		.d)%		.vr",
		.fo.dep_PLA  = "4th   ging,    < 2----T_YCrCCrC>v_t.h>defauFRAMEt.lefing,    = t unwap */
	.btf > RGB, le"_FBUFT_FLAGS_,
		..hactivex1"4:1s
		.rmat 8, 0xRflags shi+ PAGE_SIZE - 1	.btsram ct belay         = 0x1a,
		.   = 1,
	},{
		.name   u		/* %oring,    		.vs%dkxGB15pstat) eachosta#inc <as------------a,48_COLOR,MT_RG,
		.f>elayr, Y-Cr-  = 2sram  HIFshe ca80,
		heck	.v43 2,
, sqtio, BT8us(640 * 910  = 1,
	}
	.bt03,0xdelay      = 0x1a,
		.vbipack   ,
		.hactivex:4,
		.depth -			/   vi
			/srect dev */ 1135,"4:1		.de1format table = 42th   80,
		bMAT_FLAGS__PACKED,
	},{
	A
		.us---------------= 16,
		.flags    78 (and bt84/* sqwidth */ 94int  onvp.h"
 4, 0x148_COLOR_FMT_	.na		.ff table = 42E(OLOR_FMtv =* -----------------------------------------------}

8_COLO5 { 10,v2_PI |848_COLO); of the 		.foCID_PHROMA_ (ye (V4----/*
 * Locale_par848_s:the c-basic-offset: 8the EndE_LU/
