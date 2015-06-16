/*
 * OmniVision OV511 Camera-to-USB Bridge Driver
 *
 * Copyright (c) 1999-2003 Mark W. McClelland
 * Original decompression code Copyright 1998-2000 OmniVision Technologies
 * Many improvements by Bret Wallach <bwallac1@san.rr.com>
 * Color fixes by by Orion Sky Lawlor <olawlor@acm.org> (2/26/2000)
 * Snapshot code by Kevin Moore
 * OV7620 fixes by Charl P. Botha <cpbotha@ieee.org>
 * Changes by Claudio Matsuoka <claudio@conectiva.com>
 * Original SAA7111A code by Dave Perks <dperks@ibm.net>
 * URB error messages from pwc driver by Nemosoft
 * generic_ioctl() code from videodev.c by Gerd Knorr and Alan Cox
 * Memory management (rvmalloc) code from bttv driver, by Gerd Knorr and others
 *
 * Based on the Linux CPiA driver written by Peter Pregler,
 * Scott J. Bertin and Johannes Erdfelt.
 *
 * Please see the file: Documentation/usb/ov511.txt
 * and the website at:  http://alpha.dyndns.org/ov511
 * for more info.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/pagemap.h>
#include <asm/processor.h>
#include <linux/mm.h>
#include <linux/device.h>

#if defined (__i386__)
	#include <asm/cpufeature.h>
#endif

#include "ov511.h"

/*
 * Version Information
 */
#define DRIVER_VERSION "v1.64 for Linux 2.5"
#define EMAIL "mark@alpha.dyndns.org"
#define DRIVER_AUTHOR "Mark McClelland <mark@alpha.dyndns.org> & Bret Wallach \
	& Orion Sky Lawlor <olawlor@acm.org> & Kevin Moore & Charl P. Botha \
	<cpbotha@ieee.org> & Claudio Matsuoka <claudio@conectiva.com>"
#define DRIVER_DESC "ov511 USB Camera Driver"

#define OV511_I2C_RETRIES 3
#define ENABLE_Y_QUANTABLE 1
#define ENABLE_UV_QUANTABLE 1

#define OV511_MAX_UNIT_VIDEO 16

/* Pixel count * bytes per YUV420 pixel (1.5) */
#define MAX_FRAME_SIZE(w, h) ((w) * (h) * 3 / 2)

#define MAX_DATA_SIZE(w, h) (MAX_FRAME_SIZE(w, h) + sizeof(struct timeval))

/* Max size * bytes per YUV420 pixel (1.5) + one extra isoc frame for safety */
#define MAX_RAW_DATA_SIZE(w, h) ((w) * (h) * 3 / 2 + 1024)

#define FATAL_ERROR(rc) ((rc) < 0 && (rc) != -EPERM)

/**********************************************************************
 * Module Parameters
 * (See ov511.txt for detailed descriptions of these)
 **********************************************************************/

/* These variables (and all static globals) default to zero */
static int autobright		= 1;
static int autogain		= 1;
static int autoexp		= 1;
static int debug;
static int snapshot;
static int cams			= 1;
static int compress;
static int testpat;
static int dumppix;
static int led 			= 1;
static int dump_bridge;
static int dump_sensor;
static int printph;
static int phy			= 0x1f;
static int phuv			= 0x05;
static int pvy			= 0x06;
static int pvuv			= 0x06;
static int qhy			= 0x14;
static int qhuv			= 0x03;
static int qvy			= 0x04;
static int qvuv			= 0x04;
static int lightfreq;
static int bandingfilter;
static int clockdiv		= -1;
static int packetsize		= -1;
static int framedrop		= -1;
static int fastset;
static int force_palette;
static int backlight;
/* Bitmask marking allocated devices from 0 to OV511_MAX_UNIT_VIDEO */
static unsigned long ov511_devused;
static int unit_video[OV511_MAX_UNIT_VIDEO];
static int remove_zeros;
static int mirror;
static int ov518_color;

module_param(autobright, int, 0);
MODULE_PARM_DESC(autobright, "Sensor automatically changes brightness");
module_param(autogain, int, 0);
MODULE_PARM_DESC(autogain, "Sensor automatically changes gain");
module_param(autoexp, int, 0);
MODULE_PARM_DESC(autoexp, "Sensor automatically changes exposure");
module_param(debug, int, 0);
MODULE_PARM_DESC(debug,
  "Debug level: 0=none, 1=inits, 2=warning, 3=config, 4=functions, 5=max");
module_param(snapshot, int, 0);
MODULE_PARM_DESC(snapshot, "Enable snapshot mode");
module_param(cams, int, 0);
MODULE_PARM_DESC(cams, "Number of simultaneous cameras");
module_param(compress, int, 0);
MODULE_PARM_DESC(compress, "Turn on compression");
module_param(testpat, int, 0);
MODULE_PARM_DESC(testpat,
  "Replace image with vertical bar testpattern (only partially working)");
module_param(dumppix, int, 0);
MODULE_PARM_DESC(dumppix, "Dump raw pixel data");
module_param(led, int, 0);
MODULE_PARM_DESC(led,
  "LED policy (OV511+ or later). 0=off, 1=on (default), 2=auto (on when open)");
module_param(dump_bridge, int, 0);
MODULE_PARM_DESC(dump_bridge, "Dump the bridge registers");
module_param(dump_sensor, int, 0);
MODULE_PARM_DESC(dump_sensor, "Dump the sensor registers");
module_param(printph, int, 0);
MODULE_PARM_DESC(printph, "Print frame start/end headers");
module_param(phy, int, 0);
MODULE_PARM_DESC(phy, "Prediction range (horiz. Y)");
module_param(phuv, int, 0);
MODULE_PARM_DESC(phuv, "Prediction range (horiz. UV)");
module_param(pvy, int, 0);
MODULE_PARM_DESC(pvy, "Prediction range (vert. Y)");
module_param(pvuv, int, 0);
MODULE_PARM_DESC(pvuv, "Prediction range (vert. UV)");
module_param(qhy, int, 0);
MODULE_PARM_DESC(qhy, "Quantization threshold (horiz. Y)");
module_param(qhuv, int, 0);
MODULE_PARM_DESC(qhuv, "Quantization threshold (horiz. UV)");
module_param(qvy, int, 0);
MODULE_PARM_DESC(qvy, "Quantization threshold (vert. Y)");
module_param(qvuv, int, 0);
MODULE_PARM_DESC(qvuv, "Quantization threshold (vert. UV)");
module_param(lightfreq, int, 0);
MODULE_PARM_DESC(lightfreq,
  "Light frequency. Set to 50 or 60 Hz, or zero for default settings");
module_param(bandingfilter, int, 0);
MODULE_PARM_DESC(bandingfilter,
  "Enable banding filter (to reduce effects of fluorescent lighting)");
module_param(clockdiv, int, 0);
MODULE_PARM_DESC(clockdiv, "Force pixel clock divisor to a specific value");
module_param(packetsize, int, 0);
MODULE_PARM_DESC(packetsize, "Force a specific isoc packet size");
module_param(framedrop, int, 0);
MODULE_PARM_DESC(framedrop, "Force a specific frame drop register setting");
module_param(fastset, int, 0);
MODULE_PARM_DESC(fastset, "Allows picture settings to take effect immediately");
module_param(force_palette, int, 0);
MODULE_PARM_DESC(force_palette, "Force the palette to a specific value");
module_param(backlight, int, 0);
MODULE_PARM_DESC(backlight, "For objects that are lit from behind");
static unsigned int num_uv;
module_param_array(unit_video, int, &num_uv, 0);
MODULE_PARM_DESC(unit_video,
  "Force use of specific minor number(s). 0 is not allowed.");
module_param(remove_zeros, int, 0);
MODULE_PARM_DESC(remove_zeros,
  "Remove zero-padding from uncompressed incoming data");
module_param(mirror, int, 0);
MODULE_PARM_DESC(mirror, "Reverse image horizontally");
module_param(ov518_color, int, 0);
MODULE_PARM_DESC(ov518_color, "Enable OV518 color (experimental)");

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

/**********************************************************************
 * Miscellaneous Globals
 **********************************************************************/

static struct usb_driver ov511_driver;

/* Number of times to retry a failed I2C transaction. Increase this if you
 * are getting "Failed to read sensor ID..." */
static const int i2c_detect_tries = 5;

static struct usb_device_id device_table [] = {
	{ USB_DEVICE(VEND_OMNIVISION, PROD_OV511) },
	{ USB_DEVICE(VEND_OMNIVISION, PROD_OV511PLUS) },
	{ USB_DEVICE(VEND_MATTEL, PROD_ME2CAM) },
	{ }  /* Terminating entry */
};

MODULE_DEVICE_TABLE (usb, device_table);

static unsigned char yQuanTable511[] = OV511_YQUANTABLE;
static unsigned char uvQuanTable511[] = OV511_UVQUANTABLE;
static unsigned char yQuanTable518[] = OV518_YQUANTABLE;
static unsigned char uvQuanTable518[] = OV518_UVQUANTABLE;

/**********************************************************************
 * Symbolic Names
 **********************************************************************/

/* Known OV511-based cameras */
static struct symbolic_list camlist[] = {
	{   0, "Generic Camera (no ID)" },
	{   1, "Mustek WCam 3X" },
	{   3, "D-Link DSB-C300" },
	{   4, "Generic OV511/OV7610" },
	{   5, "Puretek PT-6007" },
	{   6, "Lifeview USB Life TV (NTSC)" },
	{  21, "Creative Labs WebCam 3" },
	{  22, "Lifeview USB Life TV (PAL D/K+B/G)" },
	{  36, "Koala-Cam" },
	{  38, "Lifeview USB Life TV (PAL)" },
	{  41, "Samsung Anycam MPC-M10" },
	{  43, "Mtekvision Zeca MV402" },
	{  46, "Suma eON" },
	{  70, "Lifeview USB Life TV (PAL/SECAM)" },
	{ 100, "Lifeview RoboCam" },
	{ 102, "AverMedia InterCam Elite" },
	{ 112, "MediaForte MV300" },	/* or OV7110 evaluation kit */
	{ 134, "Ezonics EZCam II" },
	{ 192, "Webeye 2000B" },
	{ 253, "Alpha Vision Tech. AlphaCam SE" },
	{  -1, NULL }
};

/* Video4Linux1 Palettes */
static struct symbolic_list v4l1_plist[] = {
	{ VIDEO_PALETTE_GREY,	"GREY" },
	{ VIDEO_PALETTE_HI240,	"HI240" },
	{ VIDEO_PALETTE_RGB565,	"RGB565" },
	{ VIDEO_PALETTE_RGB24,	"RGB24" },
	{ VIDEO_PALETTE_RGB32,	"RGB32" },
	{ VIDEO_PALETTE_RGB555,	"RGB555" },
	{ VIDEO_PALETTE_YUV422,	"YUV422" },
	{ VIDEO_PALETTE_YUYV,	"YUYV" },
	{ VIDEO_PALETTE_UYVY,	"UYVY" },
	{ VIDEO_PALETTE_YUV420,	"YUV420" },
	{ VIDEO_PALETTE_YUV411,	"YUV411" },
	{ VIDEO_PALETTE_RAW,	"RAW" },
	{ VIDEO_PALETTE_YUV422P,"YUV422P" },
	{ VIDEO_PALETTE_YUV411P,"YUV411P" },
	{ VIDEO_PALETTE_YUV420P,"YUV420P" },
	{ VIDEO_PALETTE_YUV410P,"YUV410P" },
	{ -1, NULL }
};

static struct symbolic_list brglist[] = {
	{ BRG_OV511,		"OV511" },
	{ BRG_OV511PLUS,	"OV511+" },
	{ BRG_OV518,		"OV518" },
	{ BRG_OV518PLUS,	"OV518+" },
	{ -1, NULL }
};

static struct symbolic_list senlist[] = {
	{ SEN_OV76BE,	"OV76BE" },
	{ SEN_OV7610,	"OV7610" },
	{ SEN_OV7620,	"OV7620" },
	{ SEN_OV7620AE,	"OV7620AE" },
	{ SEN_OV6620,	"OV6620" },
	{ SEN_OV6630,	"OV6630" },
	{ SEN_OV6630AE,	"OV6630AE" },
	{ SEN_OV6630AF,	"OV6630AF" },
	{ SEN_OV8600,	"OV8600" },
	{ SEN_KS0127,	"KS0127" },
	{ SEN_KS0127B,	"KS0127B" },
	{ SEN_SAA7111A,	"SAA7111A" },
	{ -1, NULL }
};

/* URB error codes: */
static struct symbolic_list urb_errlist[] = {
	{ -ENOSR,	"Buffer error (overrun)" },
	{ -EPIPE,	"Stalled (device not responding)" },
	{ -EOVERFLOW,	"Babble (device sends too much data)" },
	{ -EPROTO,	"Bit-stuff error (bad cable?)" },
	{ -EILSEQ,	"CRC/Timeout (bad cable?)" },
	{ -ETIME,	"Device does not respond to token" },
	{ -ETIMEDOUT,	"Device does not respond to command" },
	{ -1, NULL }
};

/**********************************************************************
 * Memory management
 **********************************************************************/
static void *
rvmalloc(unsigned long size)
{
	void *mem;
	unsigned long adr;

	size = PAGE_ALIGN(size);
	mem = vmalloc_32(size);
	if (!mem)
		return NULL;

	memset(mem, 0, size); /* Clear the ram out, no junk to the user */
	adr = (unsigned long) mem;
	while (size > 0) {
		SetPageReserved(vmalloc_to_page((void *)adr));
		adr += PAGE_SIZE;
		size -= PAGE_SIZE;
	}

	return mem;
}

static void
rvfree(void *mem, unsigned long size)
{
	unsigned long adr;

	if (!mem)
		return;

	adr = (unsigned long) mem;
	while ((long) size > 0) {
		ClearPageReserved(vmalloc_to_page((void *)adr));
		adr += PAGE_SIZE;
		size -= PAGE_SIZE;
	}
	vfree(mem);
}

/**********************************************************************
 *
 * Register I/O
 *
 **********************************************************************/

/* Write an OV51x register */
static int
reg_w(struct usb_ov511 *ov, unsigned char reg, unsigned char value)
{
	int rc;

	PDEBUG(5, "0x%02X:0x%02X", reg, value);

	mutex_lock(&ov->cbuf_lock);
	ov->cbuf[0] = value;
	rc = usb_control_msg(ov->dev,
			     usb_sndctrlpipe(ov->dev, 0),
			     (ov->bclass == BCL_OV518)?1:2 /* REG_IO */,
			     USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			     0, (__u16)reg, &ov->cbuf[0], 1, 1000);
	mutex_unlock(&ov->cbuf_lock);

	if (rc < 0)
		err("reg write: error %d: %s", rc, symbolic(urb_errlist, rc));

	return rc;
}

/* Read from an OV51x register */
/* returns: negative is error, pos or zero is data */
static int
reg_r(struct usb_ov511 *ov, unsigned char reg)
{
	int rc;

	mutex_lock(&ov->cbuf_lock);
	rc = usb_control_msg(ov->dev,
			     usb_rcvctrlpipe(ov->dev, 0),
			     (ov->bclass == BCL_OV518)?1:3 /* REG_IO */,
			     USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			     0, (__u16)reg, &ov->cbuf[0], 1, 1000);

	if (rc < 0) {
		err("reg read: error %d: %s", rc, symbolic(urb_errlist, rc));
	} else {
		rc = ov->cbuf[0];
		PDEBUG(5, "0x%02X:0x%02X", reg, ov->cbuf[0]);
	}

	mutex_unlock(&ov->cbuf_lock);

	return rc;
}

/*
 * Writes bits at positions specified by mask to an OV51x reg. Bits that are in
 * the same position as 1's in "mask" are cleared and set to "value". Bits
 * that are in the same position as 0's in "mask" are preserved, regardless
 * of their respective state in "value".
 */
static int
reg_w_mask(struct usb_ov511 *ov,
	   unsigned char reg,
	   unsigned char value,
	   unsigned char mask)
{
	int ret;
	unsigned char oldval, newval;

	ret = reg_r(ov, reg);
	if (ret < 0)
		return ret;

	oldval = (unsigned char) ret;
	oldval &= (~mask);		/* Clear the masked bits */
	value &= mask;			/* Enforce mask on value */
	newval = oldval | value;	/* Set the desired bits */

	return (reg_w(ov, reg, newval));
}

/*
 * Writes multiple (n) byte value to a single register. Only valid with certain
 * registers (0x30 and 0xc4 - 0xce).
 */
static int
ov518_reg_w32(struct usb_ov511 *ov, unsigned char reg, u32 val, int n)
{
	int rc;

	PDEBUG(5, "0x%02X:%7d, n=%d", reg, val, n);

	mutex_lock(&ov->cbuf_lock);

	*((__le32 *)ov->cbuf) = __cpu_to_le32(val);

	rc = usb_control_msg(ov->dev,
			     usb_sndctrlpipe(ov->dev, 0),
			     1 /* REG_IO */,
			     USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			     0, (__u16)reg, ov->cbuf, n, 1000);
	mutex_unlock(&ov->cbuf_lock);

	if (rc < 0)
		err("reg write multiple: error %d: %s", rc,
		    symbolic(urb_errlist, rc));

	return rc;
}

static int
ov511_upload_quan_tables(struct usb_ov511 *ov)
{
	unsigned char *pYTable = yQuanTable511;
	unsigned char *pUVTable = uvQuanTable511;
	unsigned char val0, val1;
	int i, rc, reg = R511_COMP_LUT_BEGIN;

	PDEBUG(4, "Uploading quantization tables");

	for (i = 0; i < OV511_QUANTABLESIZE / 2; i++) {
		if (ENABLE_Y_QUANTABLE)	{
			val0 = *pYTable++;
			val1 = *pYTable++;
			val0 &= 0x0f;
			val1 &= 0x0f;
			val0 |= val1 << 4;
			rc = reg_w(ov, reg, val0);
			if (rc < 0)
				return rc;
		}

		if (ENABLE_UV_QUANTABLE) {
			val0 = *pUVTable++;
			val1 = *pUVTable++;
			val0 &= 0x0f;
			val1 &= 0x0f;
			val0 |= val1 << 4;
			rc = reg_w(ov, reg + OV511_QUANTABLESIZE/2, val0);
			if (rc < 0)
				return rc;
		}

		reg++;
	}

	return 0;
}

/* OV518 quantization tables are 8x4 (instead of 8x8) */
static int
ov518_upload_quan_tables(struct usb_ov511 *ov)
{
	unsigned char *pYTable = yQuanTable518;
	unsigned char *pUVTable = uvQuanTable518;
	unsigned char val0, val1;
	int i, rc, reg = R511_COMP_LUT_BEGIN;

	PDEBUG(4, "Uploading quantization tables");

	for (i = 0; i < OV518_QUANTABLESIZE / 2; i++) {
		if (ENABLE_Y_QUANTABLE) {
			val0 = *pYTable++;
			val1 = *pYTable++;
			val0 &= 0x0f;
			val1 &= 0x0f;
			val0 |= val1 << 4;
			rc = reg_w(ov, reg, val0);
			if (rc < 0)
				return rc;
		}

		if (ENABLE_UV_QUANTABLE) {
			val0 = *pUVTable++;
			val1 = *pUVTable++;
			val0 &= 0x0f;
			val1 &= 0x0f;
			val0 |= val1 << 4;
			rc = reg_w(ov, reg + OV518_QUANTABLESIZE/2, val0);
			if (rc < 0)
				return rc;
		}

		reg++;
	}

	return 0;
}

static int
ov51x_reset(struct usb_ov511 *ov, unsigned char reset_type)
{
	int rc;

	/* Setting bit 0 not allowed on 518/518Plus */
	if (ov->bclass == BCL_OV518)
		reset_type &= 0xfe;

	PDEBUG(4, "Reset: type=0x%02X", reset_type);

	rc = reg_w(ov, R51x_SYS_RESET, reset_type);
	rc = reg_w(ov, R51x_SYS_RESET, 0);

	if (rc < 0)
		err("reset: command failed");

	return rc;
}

/**********************************************************************
 *
 * Low-level I2C I/O functions
 *
 **********************************************************************/

/* NOTE: Do not call this function directly!
 * The OV518 I2C I/O procedure is different, hence, this function.
 * This is normally only called from i2c_w(). Note that this function
 * always succeeds regardless of whether the sensor is present and working.
 */
static int
ov518_i2c_write_internal(struct usb_ov511 *ov,
			 unsigned char reg,
			 unsigned char value)
{
	int rc;

	PDEBUG(5, "0x%02X:0x%02X", reg, value);

	/* Select camera register */
	rc = reg_w(ov, R51x_I2C_SADDR_3, reg);
	if (rc < 0)
		return rc;

	/* Write "value" to I2C data port of OV511 */
	rc = reg_w(ov, R51x_I2C_DATA, value);
	if (rc < 0)
		return rc;

	/* Initiate 3-byte write cycle */
	rc = reg_w(ov, R518_I2C_CTL, 0x01);
	if (rc < 0)
		return rc;

	return 0;
}

/* NOTE: Do not call this function directly! */
static int
ov511_i2c_write_internal(struct usb_ov511 *ov,
			 unsigned char reg,
			 unsigned char value)
{
	int rc, retries;

	PDEBUG(5, "0x%02X:0x%02X", reg, value);

	/* Three byte write cycle */
	for (retries = OV511_I2C_RETRIES; ; ) {
		/* Select camera register */
		rc = reg_w(ov, R51x_I2C_SADDR_3, reg);
		if (rc < 0)
			break;

		/* Write "value" to I2C data port of OV511 */
		rc = reg_w(ov, R51x_I2C_DATA, value);
		if (rc < 0)
			break;

		/* Initiate 3-byte write cycle */
		rc = reg_w(ov, R511_I2C_CTL, 0x01);
		if (rc < 0)
			break;

		/* Retry until idle */
		do {
			rc = reg_r(ov, R511_I2C_CTL);
		} while (rc > 0 && ((rc&1) == 0));
		if (rc < 0)
			break;

		/* Ack? */
		if ((rc&2) == 0) {
			rc = 0;
			break;
		}
#if 0
		/* I2C abort */
		reg_w(ov, R511_I2C_CTL, 0x10);
#endif
		if (--retries < 0) {
			err("i2c write retries exhausted");
			rc = -1;
			break;
		}
	}

	return rc;
}

/* NOTE: Do not call this function directly!
 * The OV518 I2C I/O procedure is different, hence, this function.
 * This is normally only called from i2c_r(). Note that this function
 * always succeeds regardless of whether the sensor is present and working.
 */
static int
ov518_i2c_read_internal(struct usb_ov511 *ov, unsigned char reg)
{
	int rc, value;

	/* Select camera register */
	rc = reg_w(ov, R51x_I2C_SADDR_2, reg);
	if (rc < 0)
		return rc;

	/* Initiate 2-byte write cycle */
	rc = reg_w(ov, R518_I2C_CTL, 0x03);
	if (rc < 0)
		return rc;

	/* Initiate 2-byte read cycle */
	rc = reg_w(ov, R518_I2C_CTL, 0x05);
	if (rc < 0)
		return rc;

	value = reg_r(ov, R51x_I2C_DATA);

	PDEBUG(5, "0x%02X:0x%02X", reg, value);

	return value;
}

/* NOTE: Do not call this function directly!
 * returns: negative is error, pos or zero is data */
static int
ov511_i2c_read_internal(struct usb_ov511 *ov, unsigned char reg)
{
	int rc, value, retries;

	/* Two byte write cycle */
	for (retries = OV511_I2C_RETRIES; ; ) {
		/* Select camera register */
		rc = reg_w(ov, R51x_I2C_SADDR_2, reg);
		if (rc < 0)
			return rc;

		/* Initiate 2-byte write cycle */
		rc = reg_w(ov, R511_I2C_CTL, 0x03);
		if (rc < 0)
			return rc;

		/* Retry until idle */
		do {
			rc = reg_r(ov, R511_I2C_CTL);
		} while (rc > 0 && ((rc & 1) == 0));
		if (rc < 0)
			return rc;

		if ((rc&2) == 0) /* Ack? */
			break;

		/* I2C abort */
		reg_w(ov, R511_I2C_CTL, 0x10);

		if (--retries < 0) {
			err("i2c write retries exhausted");
			return -1;
		}
	}

	/* Two byte read cycle */
	for (retries = OV511_I2C_RETRIES; ; ) {
		/* Initiate 2-byte read cycle */
		rc = reg_w(ov, R511_I2C_CTL, 0x05);
		if (rc < 0)
			return rc;

		/* Retry until idle */
		do {
			rc = reg_r(ov, R511_I2C_CTL);
		} while (rc > 0 && ((rc&1) == 0));
		if (rc < 0)
			return rc;

		if ((rc&2) == 0) /* Ack? */
			break;

		/* I2C abort */
		rc = reg_w(ov, R511_I2C_CTL, 0x10);
		if (rc < 0)
			return rc;

		if (--retries < 0) {
			err("i2c read retries exhausted");
			return -1;
		}
	}

	value = reg_r(ov, R51x_I2C_DATA);

	PDEBUG(5, "0x%02X:0x%02X", reg, value);

	/* This is needed to make i2c_w() work */
	rc = reg_w(ov, R511_I2C_CTL, 0x05);
	if (rc < 0)
		return rc;

	return value;
}

/* returns: negative is error, pos or zero is data */
static int
i2c_r(struct usb_ov511 *ov, unsigned char reg)
{
	int rc;

	mutex_lock(&ov->i2c_lock);

	if (ov->bclass == BCL_OV518)
		rc = ov518_i2c_read_internal(ov, reg);
	else
		rc = ov511_i2c_read_internal(ov, reg);

	mutex_unlock(&ov->i2c_lock);

	return rc;
}

static int
i2c_w(struct usb_ov511 *ov, unsigned char reg, unsigned char value)
{
	int rc;

	mutex_lock(&ov->i2c_lock);

	if (ov->bclass == BCL_OV518)
		rc = ov518_i2c_write_internal(ov, reg, value);
	else
		rc = ov511_i2c_write_internal(ov, reg, value);

	mutex_unlock(&ov->i2c_lock);

	return rc;
}

/* Do not call this function directly! */
static int
ov51x_i2c_write_mask_internal(struct usb_ov511 *ov,
			      unsigned char reg,
			      unsigned char value,
			      unsigned char mask)
{
	int rc;
	unsigned char oldval, newval;

	if (mask == 0xff) {
		newval = value;
	} else {
		if (ov->bclass == BCL_OV518)
			rc = ov518_i2c_read_internal(ov, reg);
		else
			rc = ov511_i2c_read_internal(ov, reg);
		if (rc < 0)
			return rc;

		oldval = (unsigned char) rc;
		oldval &= (~mask);		/* Clear the masked bits */
		value &= mask;			/* Enforce mask on value */
		newval = oldval | value;	/* Set the desired bits */
	}

	if (ov->bclass == BCL_OV518)
		return (ov518_i2c_write_internal(ov, reg, newval));
	else
		return (ov511_i2c_write_internal(ov, reg, newval));
}

/* Writes bits at positions specified by mask to an I2C reg. Bits that are in
 * the same position as 1's in "mask" are cleared and set to "value". Bits
 * that are in the same position as 0's in "mask" are preserved, regardless
 * of their respective state in "value".
 */
static int
i2c_w_mask(struct usb_ov511 *ov,
	   unsigned char reg,
	   unsigned char value,
	   unsigned char mask)
{
	int rc;

	mutex_lock(&ov->i2c_lock);
	rc = ov51x_i2c_write_mask_internal(ov, reg, value, mask);
	mutex_unlock(&ov->i2c_lock);

	return rc;
}

/* Set the read and write slave IDs. The "slave" argument is the write slave,
 * and the read slave will be set to (slave + 1). ov->i2c_lock should be held
 * when calling this. This should not be called from outside the i2c I/O
 * functions.
 */
static int
i2c_set_slave_internal(struct usb_ov511 *ov, unsigned char slave)
{
	int rc;

	rc = reg_w(ov, R51x_I2C_W_SID, slave);
	if (rc < 0)
		return rc;

	rc = reg_w(ov, R51x_I2C_R_SID, slave + 1);
	if (rc < 0)
		return rc;

	return 0;
}

/* Write to a specific I2C slave ID and register, using the specified mask */
static int
i2c_w_slave(struct usb_ov511 *ov,
	    unsigned char slave,
	    unsigned char reg,
	    unsigned char value,
	    unsigned char mask)
{
	int rc = 0;

	mutex_lock(&ov->i2c_lock);

	/* Set new slave IDs */
	rc = i2c_set_slave_internal(ov, slave);
	if (rc < 0)
		goto out;

	rc = ov51x_i2c_write_mask_internal(ov, reg, value, mask);

out:
	/* Restore primary IDs */
	if (i2c_set_slave_internal(ov, ov->primary_i2c_slave) < 0)
		err("Couldn't restore primary I2C slave");

	mutex_unlock(&ov->i2c_lock);
	return rc;
}

/* Read from a specific I2C slave ID and register */
static int
i2c_r_slave(struct usb_ov511 *ov,
	    unsigned char slave,
	    unsigned char reg)
{
	int rc;

	mutex_lock(&ov->i2c_lock);

	/* Set new slave IDs */
	rc = i2c_set_slave_internal(ov, slave);
	if (rc < 0)
		goto out;

	if (ov->bclass == BCL_OV518)
		rc = ov518_i2c_read_internal(ov, reg);
	else
		rc = ov511_i2c_read_internal(ov, reg);

out:
	/* Restore primary IDs */
	if (i2c_set_slave_internal(ov, ov->primary_i2c_slave) < 0)
		err("Couldn't restore primary I2C slave");

	mutex_unlock(&ov->i2c_lock);
	return rc;
}

/* Sets I2C read and write slave IDs. Returns <0 for error */
static int
ov51x_set_slave_ids(struct usb_ov511 *ov, unsigned char sid)
{
	int rc;

	mutex_lock(&ov->i2c_lock);

	rc = i2c_set_slave_internal(ov, sid);
	if (rc < 0)
		goto out;

	// FIXME: Is this actually necessary?
	rc = ov51x_reset(ov, OV511_RESET_NOREGS);
out:
	mutex_unlock(&ov->i2c_lock);
	return rc;
}

static int
write_regvals(struct usb_ov511 *ov, struct ov511_regvals * pRegvals)
{
	int rc;

	while (pRegvals->bus != OV511_DONE_BUS) {
		if (pRegvals->bus == OV511_REG_BUS) {
			if ((rc = reg_w(ov, pRegvals->reg, pRegvals->val)) < 0)
				return rc;
		} else if (pRegvals->bus == OV511_I2C_BUS) {
			if ((rc = i2c_w(ov, pRegvals->reg, pRegvals->val)) < 0)
				return rc;
		} else {
			err("Bad regval array");
			return -1;
		}
		pRegvals++;
	}
	return 0;
}

#ifdef OV511_DEBUG
static void
dump_i2c_range(struct usb_ov511 *ov, int reg1, int regn)
{
	int i, rc;

	for (i = reg1; i <= regn; i++) {
		rc = i2c_r(ov, i);
		dev_info(&ov->dev->dev, "Sensor[0x%02X] = 0x%02X\n", i, rc);
	}
}

static void
dump_i2c_regs(struct usb_ov511 *ov)
{
	dev_info(&ov->dev->dev, "I2C REGS\n");
	dump_i2c_range(ov, 0x00, 0x7C);
}

static void
dump_reg_range(struct usb_ov511 *ov, int reg1, int regn)
{
	int i, rc;

	for (i = reg1; i <= regn; i++) {
		rc = reg_r(ov, i);
		dev_info(&ov->dev->dev, "OV511[0x%02X] = 0x%02X\n", i, rc);
	}
}

static void
ov511_dump_regs(struct usb_ov511 *ov)
{
	dev_info(&ov->dev->dev, "CAMERA INTERFACE REGS\n");
	dump_reg_range(ov, 0x10, 0x1f);
	dev_info(&ov->dev->dev, "DRAM INTERFACE REGS\n");
	dump_reg_range(ov, 0x20, 0x23);
	dev_info(&ov->dev->dev, "ISO FIFO REGS\n");
	dump_reg_range(ov, 0x30, 0x31);
	dev_info(&ov->dev->dev, "PIO REGS\n");
	dump_reg_range(ov, 0x38, 0x39);
	dump_reg_range(ov, 0x3e, 0x3e);
	dev_info(&ov->dev->dev, "I2C REGS\n");
	dump_reg_range(ov, 0x40, 0x49);
	dev_info(&ov->dev->dev, "SYSTEM CONTROL REGS\n");
	dump_reg_range(ov, 0x50, 0x55);
	dump_reg_range(ov, 0x5e, 0x5f);
	dev_info(&ov->dev->dev, "OmniCE REGS\n");
	dump_reg_range(ov, 0x70, 0x79);
	/* NOTE: Quantization tables are not readable. You will get the value
	 * in reg. 0x79 for every table register */
	dump_reg_range(ov, 0x80, 0x9f);
	dump_reg_range(ov, 0xa0, 0xbf);

}

static void
ov518_dump_regs(struct usb_ov511 *ov)
{
	dev_info(&ov->dev->dev, "VIDEO MODE REGS\n");
	dump_reg_range(ov, 0x20, 0x2f);
	dev_info(&ov->dev->dev, "DATA PUMP AND SNAPSHOT REGS\n");
	dump_reg_range(ov, 0x30, 0x3f);
	dev_info(&ov->dev->dev, "I2C REGS\n");
	dump_reg_range(ov, 0x40, 0x4f);
	dev_info(&ov->dev->dev, "SYSTEM CONTROL AND VENDOR REGS\n");
	dump_reg_range(ov, 0x50, 0x5f);
	dev_info(&ov->dev->dev, "60 - 6F\n");
	dump_reg_range(ov, 0x60, 0x6f);
	dev_info(&ov->dev->dev, "70 - 7F\n");
	dump_reg_range(ov, 0x70, 0x7f);
	dev_info(&ov->dev->dev, "Y QUANTIZATION TABLE\n");
	dump_reg_range(ov, 0x80, 0x8f);
	dev_info(&ov->dev->dev, "UV QUANTIZATION TABLE\n");
	dump_reg_range(ov, 0x90, 0x9f);
	dev_info(&ov->dev->dev, "A0 - BF\n");
	dump_reg_range(ov, 0xa0, 0xbf);
	dev_info(&ov->dev->dev, "CBR\n");
	dump_reg_range(ov, 0xc0, 0xcf);
}
#endif

/*****************************************************************************/

/* Temporarily stops OV511 from functioning. Must do this before changing
 * registers while the camera is streaming */
static inline int
ov51x_stop(struct usb_ov511 *ov)
{
	PDEBUG(4, "stopping");
	ov->stopped = 1;
	if (ov->bclass == BCL_OV518)
		return (reg_w_mask(ov, R51x_SYS_RESET, 0x3a, 0x3a));
	else
		return (reg_w(ov, R51x_SYS_RESET, 0x3d));
}

/* Restarts OV511 after ov511_stop() is called. Has no effect if it is not
 * actually stopped (for performance). */
static inline int
ov51x_restart(struct usb_ov511 *ov)
{
	if (ov->stopped) {
		PDEBUG(4, "restarting");
		ov->stopped = 0;

		/* Reinitialize the stream */
		if (ov->bclass == BCL_OV518)
			reg_w(ov, 0x2f, 0x80);

		return (reg_w(ov, R51x_SYS_RESET, 0x00));
	}

	return 0;
}

/* Sleeps until no frames are active. Returns !0 if got signal */
static int
ov51x_wait_frames_inactive(struct usb_ov511 *ov)
{
	return wait_event_interruptible(ov->wq, ov->curframe < 0);
}

/* Resets the hardware snapshot button */
static void
ov51x_clear_snapshot(struct usb_ov511 *ov)
{
	if (ov->bclass == BCL_OV511) {
		reg_w(ov, R51x_SYS_SNAP, 0x00);
		reg_w(ov, R51x_SYS_SNAP, 0x02);
		reg_w(ov, R51x_SYS_SNAP, 0x00);
	} else if (ov->bclass == BCL_OV518) {
		dev_warn(&ov->dev->dev,
			 "snapshot reset not supported yet on OV518(+)\n");
	} else {
		dev_err(&ov->dev->dev, "clear snap: invalid bridge type\n");
	}
}

#if 0
/* Checks the status of the snapshot button. Returns 1 if it was pressed since
 * it was last cleared, and zero in all other cases (including errors) */
static int
ov51x_check_snapshot(struct usb_ov511 *ov)
{
	int ret, status = 0;

	if (ov->bclass == BCL_OV511) {
		ret = reg_r(ov, R51x_SYS_SNAP);
		if (ret < 0) {
			dev_err(&ov->dev->dev,
				"Error checking snspshot status (%d)\n", ret);
		} else if (ret & 0x08) {
			status = 1;
		}
	} else if (ov->bclass == BCL_OV518) {
		dev_warn(&ov->dev->dev,
			 "snapshot check not supported yet on OV518(+)\n");
	} else {
		dev_err(&ov->dev->dev, "clear snap: invalid bridge type\n");
	}

	return status;
}
#endif

/* This does an initial reset of an OmniVision sensor and ensures that I2C
 * is synchronized. Returns <0 for failure.
 */
static int
init_ov_sensor(struct usb_ov511 *ov)
{
	int i, success;

	/* Reset the sensor */
	if (i2c_w(ov, 0x12, 0x80) < 0)
		return -EIO;

	/* Wait for it to initialize */
	msleep(150);

	for (i = 0, success = 0; i < i2c_detect_tries && !success; i++) {
		if ((i2c_r(ov, OV7610_REG_ID_HIGH) == 0x7F) &&
		    (i2c_r(ov, OV7610_REG_ID_LOW) == 0xA2)) {
			success = 1;
			continue;
		}

		/* Reset the sensor */
		if (i2c_w(ov, 0x12, 0x80) < 0)
			return -EIO;
		/* Wait for it to initialize */
		msleep(150);
		/* Dummy read to sync I2C */
		if (i2c_r(ov, 0x00) < 0)
			return -EIO;
	}

	if (!success)
		return -EIO;

	PDEBUG(1, "I2C synced in %d attempt(s)", i);

	return 0;
}

static int
ov511_set_packet_size(struct usb_ov511 *ov, int size)
{
	int alt, mult;

	if (ov51x_stop(ov) < 0)
		return -EIO;

	mult = size >> 5;

	if (ov->bridge == BRG_OV511) {
		if (size == 0)
			alt = OV511_ALT_SIZE_0;
		else if (size == 257)
			alt = OV511_ALT_SIZE_257;
		else if (size == 513)
			alt = OV511_ALT_SIZE_513;
		else if (size == 769)
			alt = OV511_ALT_SIZE_769;
		else if (size == 993)
			alt = OV511_ALT_SIZE_993;
		else {
			err("Set packet size: invalid size (%d)", size);
			return -EINVAL;
		}
	} else if (ov->bridge == BRG_OV511PLUS) {
		if (size == 0)
			alt = OV511PLUS_ALT_SIZE_0;
		else if (size == 33)
			alt = OV511PLUS_ALT_SIZE_33;
		else if (size == 129)
			alt = OV511PLUS_ALT_SIZE_129;
		else if (size == 257)
			alt = OV511PLUS_ALT_SIZE_257;
		else if (size == 385)
			alt = OV511PLUS_ALT_SIZE_385;
		else if (size == 513)
			alt = OV511PLUS_ALT_SIZE_513;
		else if (size == 769)
			alt = OV511PLUS_ALT_SIZE_769;
		else if (size == 961)
			alt = OV511PLUS_ALT_SIZE_961;
		else {
			err("Set packet size: invalid size (%d)", size);
			return -EINVAL;
		}
	} else {
		err("Set packet size: Invalid bridge type");
		return -EINVAL;
	}

	PDEBUG(3, "%d, mult=%d, alt=%d", size, mult, alt);

	if (reg_w(ov, R51x_FIFO_PSIZE, mult) < 0)
		return -EIO;

	if (usb_set_interface(ov->dev, ov->iface, alt) < 0) {
		err("Set packet size: set interface error");
		return -EBUSY;
	}

	if (ov51x_reset(ov, OV511_RESET_NOREGS) < 0)
		return -EIO;

	ov->packet_size = size;

	if (ov51x_restart(ov) < 0)
		return -EIO;

	return 0;
}

/* Note: Unlike the OV511/OV511+, the size argument does NOT include the
 * optional packet number byte. The actual size *is* stored in ov->packet_size,
 * though. */
static int
ov518_set_packet_size(struct usb_ov511 *ov, int size)
{
	int alt;

	if (ov51x_stop(ov) < 0)
		return -EIO;

	if (ov->bclass == BCL_OV518) {
		if (size == 0)
			alt = OV518_ALT_SIZE_0;
		else if (size == 128)
			alt = OV518_ALT_SIZE_128;
		else if (size == 256)
			alt = OV518_ALT_SIZE_256;
		else if (size == 384)
			alt = OV518_ALT_SIZE_384;
		else if (size == 512)
			alt = OV518_ALT_SIZE_512;
		else if (size == 640)
			alt = OV518_ALT_SIZE_640;
		else if (size == 768)
			alt = OV518_ALT_SIZE_768;
		else if (size == 896)
			alt = OV518_ALT_SIZE_896;
		else {
			err("Set packet size: invalid size (%d)", size);
			return -EINVAL;
		}
	} else {
		err("Set packet size: Invalid bridge type");
		return -EINVAL;
	}

	PDEBUG(3, "%d, alt=%d", size, alt);

	ov->packet_size = size;
	if (size > 0) {
		/* Program ISO FIFO size reg (packet number isn't included) */
		ov518_reg_w32(ov, 0x30, size, 2);

		if (ov->packet_numbering)
			++ov->packet_size;
	}

	if (usb_set_interface(ov->dev, ov->iface, alt) < 0) {
		err("Set packet size: set interface error");
		return -EBUSY;
	}

	/* Initialize the stream */
	if (reg_w(ov, 0x2f, 0x80) < 0)
		return -EIO;

	if (ov51x_restart(ov) < 0)
		return -EIO;

	if (ov51x_reset(ov, OV511_RESET_NOREGS) < 0)
		return -EIO;

	return 0;
}

/* Upload compression params and quantization tables. Returns 0 for success. */
static int
ov511_init_compression(struct usb_ov511 *ov)
{
	int rc = 0;

	if (!ov->compress_inited) {
		reg_w(ov, 0x70, phy);
		reg_w(ov, 0x71, phuv);
		reg_w(ov, 0x72, pvy);
		reg_w(ov, 0x73, pvuv);
		reg_w(ov, 0x74, qhy);
		reg_w(ov, 0x75, qhuv);
		reg_w(ov, 0x76, qvy);
		reg_w(ov, 0x77, qvuv);

		if (ov511_upload_quan_tables(ov) < 0) {
			err("Error uploading quantization tables");
			rc = -EIO;
			goto out;
		}
	}

	ov->compress_inited = 1;
out:
	return rc;
}

/* Upload compression params and quantization tables. Returns 0 for success. */
static int
ov518_init_compression(struct usb_ov511 *ov)
{
	int rc = 0;

	if (!ov->compress_inited) {
		if (ov518_upload_quan_tables(ov) < 0) {
			err("Error uploading quantization tables");
			rc = -EIO;
			goto out;
		}
	}

	ov->compress_inited = 1;
out:
	return rc;
}

/* -------------------------------------------------------------------------- */

/* Sets sensor's contrast setting to "val" */
static int
sensor_set_contrast(struct usb_ov511 *ov, unsigned short val)
{
	int rc;

	PDEBUG(3, "%d", val);

	if (ov->stop_during_set)
		if (ov51x_stop(ov) < 0)
			return -EIO;

	switch (ov->sensor) {
	case SEN_OV7610:
	case SEN_OV6620:
	{
		rc = i2c_w(ov, OV7610_REG_CNT, val >> 8);
		if (rc < 0)
			goto out;
		break;
	}
	case SEN_OV6630:
	{
		rc = i2c_w_mask(ov, OV7610_REG_CNT, val >> 12, 0x0f);
		if (rc < 0)
			goto out;
		break;
	}
	case SEN_OV7620:
	{
		unsigned char ctab[] = {
			0x01, 0x05, 0x09, 0x11, 0x15, 0x35, 0x37, 0x57,
			0x5b, 0xa5, 0xa7, 0xc7, 0xc9, 0xcf, 0xef, 0xff
		};

		/* Use Y gamma control instead. Bit 0 enables it. */
		rc = i2c_w(ov, 0x64, ctab[val>>12]);
		if (rc < 0)
			goto out;
		break;
	}
	case SEN_SAA7111A:
	{
		rc = i2c_w(ov, 0x0b, val >> 9);
		if (rc < 0)
			goto out;
		break;
	}
	default:
	{
		PDEBUG(3, "Unsupported with this sensor");
		rc = -EPERM;
		goto out;
	}
	}

	rc = 0;		/* Success */
	ov->contrast = val;
out:
	if (ov51x_restart(ov) < 0)
		return -EIO;

	return rc;
}

/* Gets sensor's contrast setting */
static int
sensor_get_contrast(struct usb_ov511 *ov, unsigned short *val)
{
	int rc;

	switch (ov->sensor) {
	case SEN_OV7610:
	case SEN_OV6620:
		rc = i2c_r(ov, OV7610_REG_CNT);
		if (rc < 0)
			return rc;
		else
			*val = rc << 8;
		break;
	case SEN_OV6630:
		rc = i2c_r(ov, OV7610_REG_CNT);
		if (rc < 0)
			return rc;
		else
			*val = rc << 12;
		break;
	case SEN_OV7620:
		/* Use Y gamma reg instead. Bit 0 is the enable bit. */
		rc = i2c_r(ov, 0x64);
		if (rc < 0)
			return rc;
		else
			*val = (rc & 0xfe) << 8;
		break;
	case SEN_SAA7111A:
		*val = ov->contrast;
		break;
	default:
		PDEBUG(3, "Unsupported with this sensor");
		return -EPERM;
	}

	PDEBUG(3, "%d", *val);
	ov->contrast = *val;

	return 0;
}

/* -------------------------------------------------------------------------- */

/* Sets sensor's brightness setting to "val" */
static int
sensor_set_brightness(struct usb_ov511 *ov, unsigned short val)
{
	int rc;

	PDEBUG(4, "%d", val);

	if (ov->stop_during_set)
		if (ov51x_stop(ov) < 0)
			return -EIO;

	switch (ov->sensor) {
	case SEN_OV7610:
	case SEN_OV76BE:
	case SEN_OV6620:
	case SEN_OV6630:
		rc = i2c_w(ov, OV7610_REG_BRT, val >> 8);
		if (rc < 0)
			goto out;
		break;
	case SEN_OV7620:
		/* 7620 doesn't like manual changes when in auto mode */
		if (!ov->auto_brt) {
			rc = i2c_w(ov, OV7610_REG_BRT, val >> 8);
			if (rc < 0)
				goto out;
		}
		break;
	case SEN_SAA7111A:
		rc = i2c_w(ov, 0x0a, val >> 8);
		if (rc < 0)
			goto out;
		break;
	default:
		PDEBUG(3, "Unsupported with this sensor");
		rc = -EPERM;
		goto out;
	}

	rc = 0;		/* Success */
	ov->brightness = val;
out:
	if (ov51x_restart(ov) < 0)
		return -EIO;

	return rc;
}

/* Gets sensor's brightness setting */
static int
sensor_get_brightness(struct usb_ov511 *ov, unsigned short *val)
{
	int rc;

	switch (ov->sensor) {
	case SEN_OV7610:
	case SEN_OV76BE:
	case SEN_OV7620:
	case SEN_OV6620:
	case SEN_OV6630:
		rc = i2c_r(ov, OV7610_REG_BRT);
		if (rc < 0)
			return rc;
		else
			*val = rc << 8;
		break;
	case SEN_SAA7111A:
		*val = ov->brightness;
		break;
	default:
		PDEBUG(3, "Unsupported with this sensor");
		return -EPERM;
	}

	PDEBUG(3, "%d", *val);
	ov->brightness = *val;

	return 0;
}

/* -------------------------------------------------------------------------- */

/* Sets sensor's saturation (color intensity) setting to "val" */
static int
sensor_set_saturation(struct usb_ov511 *ov, unsigned short val)
{
	int rc;

	PDEBUG(3, "%d", val);

	if (ov->stop_during_set)
		if (ov51x_stop(ov) < 0)
			return -EIO;

	switch (ov->sensor) {
	case SEN_OV7610:
	case SEN_OV76BE:
	case SEN_OV6620:
	case SEN_OV6630:
		rc = i2c_w(ov, OV7610_REG_SAT, val >> 8);
		if (rc < 0)
			goto out;
		break;
	case SEN_OV7620:
//		/* Use UV gamma control instead. Bits 0 & 7 are reserved. */
//		rc = ov_i2c_write(ov->dev, 0x62, (val >> 9) & 0x7e);
//		if (rc < 0)
//			goto out;
		rc = i2c_w(ov, OV7610_REG_SAT, val >> 8);
		if (rc < 0)
			goto out;
		break;
	case SEN_SAA7111A:
		rc = i2c_w(ov, 0x0c, val >> 9);
		if (rc < 0)
			goto out;
		break;
	default:
		PDEBUG(3, "Unsupported with this sensor");
		rc = -EPERM;
		goto out;
	}

	rc = 0;		/* Success */
	ov->colour = val;
out:
	if (ov51x_restart(ov) < 0)
		return -EIO;

	return rc;
}

/* Gets sensor's saturation (color intensity) setting */
static int
sensor_get_saturation(struct usb_ov511 *ov, unsigned short *val)
{
	int rc;

	switch (ov->sensor) {
	case SEN_OV7610:
	case SEN_OV76BE:
	case SEN_OV6620:
	case SEN_OV6630:
		rc = i2c_r(ov, OV7610_REG_SAT);
		if (rc < 0)
			return rc;
		else
			*val = rc << 8;
		break;
	case SEN_OV7620:
//		/* Use UV gamma reg instead. Bits 0 & 7 are reserved. */
//		rc = i2c_r(ov, 0x62);
//		if (rc < 0)
//			return rc;
//		else
//			*val = (rc & 0x7e) << 9;
		rc = i2c_r(ov, OV7610_REG_SAT);
		if (rc < 0)
			return rc;
		else
			*val = rc << 8;
		break;
	case SEN_SAA7111A:
		*val = ov->colour;
		break;
	default:
		PDEBUG(3, "Unsupported with this sensor");
		return -EPERM;
	}

	PDEBUG(3, "%d", *val);
	ov->colour = *val;

	return 0;
}

/* -------------------------------------------------------------------------- */

/* Sets sensor's hue (red/blue balance) setting to "val" */
static int
sensor_set_hue(struct usb_ov511 *ov, unsigned short val)
{
	int rc;

	PDEBUG(3, "%d", val);

	if (ov->stop_during_set)
		if (ov51x_stop(ov) < 0)
			return -EIO;

	switch (ov->sensor) {
	case SEN_OV7610:
	case SEN_OV6620:
	case SEN_OV6630:
		rc = i2c_w(ov, OV7610_REG_RED, 0xFF - (val >> 8));
		if (rc < 0)
			goto out;

		rc = i2c_w(ov, OV7610_REG_BLUE, val >> 8);
		if (rc < 0)
			goto out;
		break;
	case SEN_OV7620:
// Hue control is causing problems. I will enable it once it's fixed.
#if 0
		rc = i2c_w(ov, 0x7a, (unsigned char)(val >> 8) + 0xb);
		if (rc < 0)
			goto out;

		rc = i2c_w(ov, 0x79, (unsigned char)(val >> 8) + 0xb);
		if (rc < 0)
			goto out;
#endif
		break;
	case SEN_SAA7111A:
		rc = i2c_w(ov, 0x0d, (val + 32768) >> 8);
		if (rc < 0)
			goto out;
		break;
	default:
		PDEBUG(3, "Unsupported with this sensor");
		rc = -EPERM;
		goto out;
	}

	rc = 0;		/* Success */
	ov->hue = val;
out:
	if (ov51x_restart(ov) < 0)
		return -EIO;

	return rc;
}

/* Gets sensor's hue (red/blue balance) setting */
static int
sensor_get_hue(struct usb_ov511 *ov, unsigned short *val)
{
	int rc;

	switch (ov->sensor) {
	case SEN_OV7610:
	case SEN_OV6620:
	case SEN_OV6630:
		rc = i2c_r(ov, OV7610_REG_BLUE);
		if (rc < 0)
			return rc;
		else
			*val = rc << 8;
		break;
	case SEN_OV7620:
		rc = i2c_r(ov, 0x7a);
		if (rc < 0)
			return rc;
		else
			*val = rc << 8;
		break;
	case SEN_SAA7111A:
		*val = ov->hue;
		break;
	default:
		PDEBUG(3, "Unsupported with this sensor");
		return -EPERM;
	}

	PDEBUG(3, "%d", *val);
	ov->hue = *val;

	return 0;
}

/* -------------------------------------------------------------------------- */

static int
sensor_set_picture(struct usb_ov511 *ov, struct video_picture *p)
{
	int rc;

	PDEBUG(4, "sensor_set_picture");

	ov->whiteness = p->whiteness;

	/* Don't return error if a setting is unsupported, or rest of settings
	 * will not be performed */

	rc = sensor_set_contrast(ov, p->contrast);
	if (FATAL_ERROR(rc))
		return rc;

	rc = sensor_set_brightness(ov, p->brightness);
	if (FATAL_ERROR(rc))
		return rc;

	rc = sensor_set_saturation(ov, p->colour);
	if (FATAL_ERROR(rc))
		return rc;

	rc = sensor_set_hue(ov, p->hue);
	if (FATAL_ERROR(rc))
		return rc;

	return 0;
}

static int
sensor_get_picture(struct usb_ov511 *ov, struct video_picture *p)
{
	int rc;

	PDEBUG(4, "sensor_get_picture");

	/* Don't return error if a setting is unsupported, or rest of settings
	 * will not be performed */

	rc = sensor_get_contrast(ov, &(p->contrast));
	if (FATAL_ERROR(rc))
		return rc;

	rc = sensor_get_brightness(ov, &(p->brightness));
	if (FATAL_ERROR(rc))
		return rc;

	rc = sensor_get_saturation(ov, &(p->colour));
	if (FATAL_ERROR(rc))
		return rc;

	rc = sensor_get_hue(ov, &(p->hue));
	if (FATAL_ERROR(rc))
		return rc;

	p->whiteness = 105 << 8;

	return 0;
}

#if 0
// FIXME: Exposure range is only 0x00-0x7f in interlace mode
/* Sets current exposure for sensor. This only has an effect if auto-exposure
 * is off */
static inline int
sensor_set_exposure(struct usb_ov511 *ov, unsigned char val)
{
	int rc;

	PDEBUG(3, "%d", val);

	if (ov->stop_during_set)
		if (ov51x_stop(ov) < 0)
			return -EIO;

	switch (ov->sensor) {
	case SEN_OV6620:
	case SEN_OV6630:
	case SEN_OV7610:
	case SEN_OV7620:
	case SEN_OV76BE:
	case SEN_OV8600:
		rc = i2c_w(ov, 0x10, val);
		if (rc < 0)
			goto out;

		break;
	case SEN_KS0127:
	case SEN_KS0127B:
	case SEN_SAA7111A:
		PDEBUG(3, "Unsupported with this sensor");
		return -EPERM;
	default:
		err("Sensor not supported for set_exposure");
		return -EINVAL;
	}

	rc = 0;		/* Success */
	ov->exposure = val;
out:
	if (ov51x_restart(ov) < 0)
		return -EIO;

	return rc;
}
#endif

/* Gets current exposure level from sensor, regardless of whether it is under
 * manual control. */
static int
sensor_get_exposure(struct usb_ov511 *ov, unsigned char *val)
{
	int rc;

	switch (ov->sensor) {
	case SEN_OV7610:
	case SEN_OV6620:
	case SEN_OV6630:
	case SEN_OV7620:
	case SEN_OV76BE:
	case SEN_OV8600:
		rc = i2c_r(ov, 0x10);
		if (rc < 0)
			return rc;
		else
			*val = rc;
		break;
	case SEN_KS0127:
	case SEN_KS0127B:
	case SEN_SAA7111A:
		val = NULL;
		PDEBUG(3, "Unsupported with this sensor");
		return -EPERM;
	default:
		err("Sensor not supported for get_exposure");
		return -EINVAL;
	}

	PDEBUG(3, "%d", *val);
	ov->exposure = *val;

	return 0;
}

/* Turns on or off the LED. Only has an effect with OV511+/OV518(+) */
static void
ov51x_led_control(struct usb_ov511 *ov, int enable)
{
	PDEBUG(4, " (%s)", enable ? "turn on" : "turn off");

	if (ov->bridge == BRG_OV511PLUS)
		reg_w(ov, R511_SYS_LED_CTL, enable ? 1 : 0);
	else if (ov->bclass == BCL_OV518)
		reg_w_mask(ov, R518_GPIO_OUT, enable ? 0x02 : 0x00, 0x02);

	return;
}

/* Matches the sensor's internal frame rate to the lighting frequency.
 * Valid frequencies are:
 *	50 - 50Hz, for European and Asian lighting
 *	60 - 60Hz, for American lighting
 *
 * Tested with: OV7610, OV7620, OV76BE, OV6620
 * Unsupported: KS0127, KS0127B, SAA7111A
 * Returns: 0 for success
 */
static int
sensor_set_light_freq(struct usb_ov511 *ov, int freq)
{
	int sixty;

	PDEBUG(4, "%d Hz", freq);

	if (freq == 60)
		sixty = 1;
	else if (freq == 50)
		sixty = 0;
	else {
		err("Invalid light freq (%d Hz)", freq);
		return -EINVAL;
	}

	switch (ov->sensor) {
	case SEN_OV7610:
		i2c_w_mask(ov, 0x2a, sixty?0x00:0x80, 0x80);
		i2c_w(ov, 0x2b, sixty?0x00:0xac);
		i2c_w_mask(ov, 0x13, 0x10, 0x10);
		i2c_w_mask(ov, 0x13, 0x00, 0x10);
		break;
	case SEN_OV7620:
	case SEN_OV76BE:
	case SEN_OV8600:
		i2c_w_mask(ov, 0x2a, sixty?0x00:0x80, 0x80);
		i2c_w(ov, 0x2b, sixty?0x00:0xac);
		i2c_w_mask(ov, 0x76, 0x01, 0x01);
		break;
	case SEN_OV6620:
	case SEN_OV6630:
		i2c_w(ov, 0x2b, sixty?0xa8:0x28);
		i2c_w(ov, 0x2a, sixty?0x84:0xa4);
		break;
	case SEN_KS0127:
	case SEN_KS0127B:
	case SEN_SAA7111A:
		PDEBUG(5, "Unsupported with this sensor");
		return -EPERM;
	default:
		err("Sensor not supported for set_light_freq");
		return -EINVAL;
	}

	ov->lightfreq = freq;

	return 0;
}

/* If enable is true, turn on the sensor's banding filter, otherwise turn it
 * off. This filter tries to reduce the pattern of horizontal light/dark bands
 * caused by some (usually fluorescent) lighting. The light frequency must be
 * set either before or after enabling it with ov51x_set_light_freq().
 *
 * Tested with: OV7610, OV7620, OV76BE, OV6620.
 * Unsupported: KS0127, KS0127B, SAA7111A
 * Returns: 0 for success
 */
static int
sensor_set_banding_filter(struct usb_ov511 *ov, int enable)
{
	int rc;

	PDEBUG(4, " (%s)", enable ? "turn on" : "turn off");

	if (ov->sensor == SEN_KS0127 || ov->sensor == SEN_KS0127B
		|| ov->sensor == SEN_SAA7111A) {
		PDEBUG(5, "Unsupported with this sensor");
		return -EPERM;
	}

	rc = i2c_w_mask(ov, 0x2d, enable?0x04:0x00, 0x04);
	if (rc < 0)
		return rc;

	ov->bandfilt = enable;

	return 0;
}

/* If enable is true, turn on the sensor's auto brightness control, otherwise
 * turn it off.
 *
 * Unsupported: KS0127, KS0127B, SAA7111A
 * Returns: 0 for success
 */
static int
sensor_set_auto_brightness(struct usb_ov511 *ov, int enable)
{
	int rc;

	PDEBUG(4, " (%s)", enable ? "turn on" : "turn off");

	if (ov->sensor == SEN_KS0127 || ov->sensor == SEN_KS0127B
		|| ov->sensor == SEN_SAA7111A) {
		PDEBUG(5, "Unsupported with this sensor");
		return -EPERM;
	}

	rc = i2c_w_mask(ov, 0x2d, enable?0x10:0x00, 0x10);
	if (rc < 0)
		return rc;

	ov->auto_brt = enable;

	return 0;
}

/* If enable is true, turn on the sensor's auto exposure control, otherwise
 * turn it off.
 *
 * Unsupported: KS0127, KS0127B, SAA7111A
 * Returns: 0 for success
 */
static int
sensor_set_auto_exposure(struct usb_ov511 *ov, int enable)
{
	PDEBUG(4, " (%s)", enable ? "turn on" : "turn off");

	switch (ov->sensor) {
	case SEN_OV7610:
		i2c_w_mask(ov, 0x29, enable?0x00:0x80, 0x80);
		break;
	case SEN_OV6620:
	case SEN_OV7620:
	case SEN_OV76BE:
	case SEN_OV8600:
		i2c_w_mask(ov, 0x13, enable?0x01:0x00, 0x01);
		break;
	case SEN_OV6630:
		i2c_w_mask(ov, 0x28, enable?0x00:0x10, 0x10);
		break;
	case SEN_KS0127:
	case SEN_KS0127B:
	case SEN_SAA7111A:
		PDEBUG(5, "Unsupported with this sensor");
		return -EPERM;
	default:
		err("Sensor not supported for set_auto_exposure");
		return -EINVAL;
	}

	ov->auto_exp = enable;

	return 0;
}

/* Modifies the sensor's exposure algorithm to allow proper exposure of objects
 * that are illuminated from behind.
 *
 * Tested with: OV6620, OV7620
 * Unsupported: OV7610, OV76BE, KS0127, KS0127B, SAA7111A
 * Returns: 0 for success
 */
static int
sensor_set_backlight(struct usb_ov511 *ov, int enable)
{
	PDEBUG(4, " (%s)", enable ? "turn on" : "turn off");

	switch (ov->sensor) {
	case SEN_OV7620:
	case SEN_OV8600:
		i2c_w_mask(ov, 0x68, enable?0xe0:0xc0, 0xe0);
		i2c_w_mask(ov, 0x29, enable?0x08:0x00, 0x08);
		i2c_w_mask(ov, 0x28, enable?0x02:0x00, 0x02);
		break;
	case SEN_OV6620:
		i2c_w_mask(ov, 0x4e, enable?0xe0:0xc0, 0xe0);
		i2c_w_mask(ov, 0x29, enable?0x08:0x00, 0x08);
		i2c_w_mask(ov, 0x0e, enable?0x80:0x00, 0x80);
		break;
	case SEN_OV6630:
		i2c_w_mask(ov, 0x4e, enable?0x80:0x60, 0xe0);
		i2c_w_mask(ov, 0x29, enable?0x08:0x00, 0x08);
		i2c_w_mask(ov, 0x28, enable?0x02:0x00, 0x02);
		break;
	case SEN_OV7610:
	case SEN_OV76BE:
	case SEN_KS0127:
	case SEN_KS0127B:
	case SEN_SAA7111A:
		PDEBUG(5, "Unsupported with this sensor");
		return -EPERM;
	default:
		err("Sensor not supported for set_backlight");
		return -EINVAL;
	}

	ov->backlight = enable;

	return 0;
}

static int
sensor_set_mirror(struct usb_ov511 *ov, int enable)
{
	PDEBUG(4, " (%s)", enable ? "turn on" : "turn off");

	switch (ov->sensor) {
	case SEN_OV6620:
	case SEN_OV6630:
	case SEN_OV7610:
	case SEN_OV7620:
	case SEN_OV76BE:
	case SEN_OV8600:
		i2c_w_mask(ov, 0x12, enable?0x40:0x00, 0x40);
		break;
	case SEN_KS0127:
	case SEN_KS0127B:
	case SEN_SAA7111A:
		PDEBUG(5, "Unsupported with this sensor");
		return -EPERM;
	default:
		err("Sensor not supported for set_mirror");
		return -EINVAL;
	}

	ov->mirror = enable;

	return 0;
}

/* Returns number of bits per pixel (regardless of where they are located;
 * planar or not), or zero for unsupported format.
 */
static inline int
get_depth(int palette)
{
	switch (palette) {
	case VIDEO_PALETTE_GREY:    return 8;
	case VIDEO_PALETTE_YUV420:  return 12;
	case VIDEO_PALETTE_YUV420P: return 12; /* Planar */
	default:		    return 0;  /* Invalid format */
	}
}

/* Bytes per frame. Used by read(). Return of 0 indicates error */
static inline long int
get_frame_length(struct ov511_frame *frame)
{
	if (!frame)
		return 0;
	else
		return ((frame->width * frame->height
			 * get_depth(frame->format)) >> 3);
}

static int
mode_init_ov_sensor_regs(struct usb_ov511 *ov, int width, int height,
			 int mode, int sub_flag, int qvga)
{
	int clock;

	/******** Mode (VGA/QVGA) and sensor specific regs ********/

	switch (ov->sensor) {
	case SEN_OV7610:
		i2c_w(ov, 0x14, qvga?0x24:0x04);
// FIXME: Does this improve the image quality or frame rate?
#if 0
		i2c_w_mask(ov, 0x28, qvga?0x00:0x20, 0x20);
		i2c_w(ov, 0x24, 0x10);
		i2c_w(ov, 0x25, qvga?0x40:0x8a);
		i2c_w(ov, 0x2f, qvga?0x30:0xb0);
		i2c_w(ov, 0x35, qvga?0x1c:0x9c);
#endif
		break;
	case SEN_OV7620:
//		i2c_w(ov, 0x2b, 0x00);
		i2c_w(ov, 0x14, qvga?0xa4:0x84);
		i2c_w_mask(ov, 0x28, qvga?0x00:0x20, 0x20);
		i2c_w(ov, 0x24, qvga?0x20:0x3a);
		i2c_w(ov, 0x25, qvga?0x30:0x60);
		i2c_w_mask(ov, 0x2d, qvga?0x40:0x00, 0x40);
		i2c_w_mask(ov, 0x67, qvga?0xf0:0x90, 0xf0);
		i2c_w_mask(ov, 0x74, qvga?0x20:0x00, 0x20);
		break;
	case SEN_OV76BE:
//		i2c_w(ov, 0x2b, 0x00);
		i2c_w(ov, 0x14, qvga?0xa4:0x84);
// FIXME: Enable this once 7620AE uses 7620 initial settings
#if 0
		i2c_w_mask(ov, 0x28, qvga?0x00:0x20, 0x20);
		i2c_w(ov, 0x24, qvga?0x20:0x3a);
		i2c_w(ov, 0x25, qvga?0x30:0x60);
		i2c_w_mask(ov, 0x2d, qvga?0x40:0x00, 0x40);
		i2c_w_mask(ov, 0x67, qvga?0xb0:0x90, 0xf0);
		i2c_w_mask(ov, 0x74, qvga?0x20:0x00, 0x20);
#endif
		break;
	case SEN_OV6620:
		i2c_w(ov, 0x14, qvga?0x24:0x04);
		break;
	case SEN_OV6630:
		i2c_w(ov, 0x14, qvga?0xa0:0x80);
		break;
	default:
		err("Invalid sensor");
		return -EINVAL;
	}

	/******** Palette-specific regs ********/

	if (mode == VIDEO_PALETTE_GREY) {
		if (ov->sensor == SEN_OV7610 || ov->sensor == SEN_OV76BE) {
			/* these aren't valid on the OV6620/OV7620/6630? */
			i2c_w_mask(ov, 0x0e, 0x40, 0x40);
		}

		if (ov->sensor == SEN_OV6630 && ov->bridge == BRG_OV518
		    && ov518_color) {
			i2c_w_mask(ov, 0x12, 0x00, 0x10);
			i2c_w_mask(ov, 0x13, 0x00, 0x20);
		} else {
			i2c_w_mask(ov, 0x13, 0x20, 0x20);
		}
	} else {
		if (ov->sensor == SEN_OV7610 || ov->sensor == SEN_OV76BE) {
			/* not valid on the OV6620/OV7620/6630? */
			i2c_w_mask(ov, 0x0e, 0x00, 0x40);
		}

		/* The OV518 needs special treatment. Although both the OV518
		 * and the OV6630 support a 16-bit video bus, only the 8 bit Y
		 * bus is actually used. The UV bus is tied to ground.
		 * Therefore, the OV6630 needs to be in 8-bit multiplexed
		 * output mode */

		if (ov->sensor == SEN_OV6630 && ov->bridge == BRG_OV518
		    && ov518_color) {
			i2c_w_mask(ov, 0x12, 0x10, 0x10);
			i2c_w_mask(ov, 0x13, 0x20, 0x20);
		} else {
			i2c_w_mask(ov, 0x13, 0x00, 0x20);
		}
	}

	/******** Clock programming ********/

	/* The OV6620 needs special handling. This prevents the
	 * severe banding that normally occurs */
	if (ov->sensor == SEN_OV6620 || ov->sensor == SEN_OV6630)
	{
		/* Clock down */

		i2c_w(ov, 0x2a, 0x04);

		if (ov->compress) {
//			clock = 0;    /* This ensures the highest frame rate */
			clock = 3;
		} else if (clockdiv == -1) {   /* If user didn't override it */
			clock = 3;    /* Gives better exposure time */
		} else {
			clock = clockdiv;
		}

		PDEBUG(4, "Setting clock divisor to %d", clock);

		i2c_w(ov, 0x11, clock);

		i2c_w(ov, 0x2a, 0x84);
		/* This next setting is critical. It seems to improve
		 * the gain or the contrast. The "reserved" bits seem
		 * to have some effect in this case. */
		i2c_w(ov, 0x2d, 0x85);
	}
	else
	{
		if (ov->compress) {
			clock = 1;    /* This ensures the highest frame rate */
		} else if (clockdiv == -1) {   /* If user didn't override it */
			/* Calculate and set the clock divisor */
			clock = ((sub_flag ? ov->subw * ov->subh
				  : width * height)
				 * (mode == VIDEO_PALETTE_GREY ? 2 : 3) / 2)
				 / 66000;
		} else {
			clock = clockdiv;
		}

		PDEBUG(4, "Setting clock divisor to %d", clock);

		i2c_w(ov, 0x11, clock);
	}

	/******** Special Features ********/

	if (framedrop >= 0)
		i2c_w(ov, 0x16, framedrop);

	/* Test Pattern */
	i2c_w_mask(ov, 0x12, (testpat?0x02:0x00), 0x02);

	/* Enable auto white balance */
	i2c_w_mask(ov, 0x12, 0x04, 0x04);

	// This will go away as soon as ov51x_mode_init_sensor_regs()
	// is fully tested.
	/* 7620/6620/6630? don't have register 0x35, so play it safe */
	if (ov->sensor == SEN_OV7610 || ov->sensor == SEN_OV76BE) {
		if (width == 640 && height == 480)
			i2c_w(ov, 0x35, 0x9e);
		else
			i2c_w(ov, 0x35, 0x1e);
	}

	return 0;
}

static int
set_ov_sensor_window(struct usb_ov511 *ov, int width, int height, int mode,
		     int sub_flag)
{
	int ret;
	int hwsbase, hwebase, vwsbase, vwebase, hwsize, vwsize;
	int hoffset, voffset, hwscale = 0, vwscale = 0;

	/* The different sensor ICs handle setting up of window differently.
	 * IF YOU SET IT WRONG, YOU WILL GET ALL ZERO ISOC DATA FROM OV51x!!! */
	switch (ov->sensor) {
	case SEN_OV7610:
	case SEN_OV76BE:
		hwsbase = 0x38;
		hwebase = 0x3a;
		vwsbase = vwebase = 0x05;
		break;
	case SEN_OV6620:
	case SEN_OV6630:
		hwsbase = 0x38;
		hwebase = 0x3a;
		vwsbase = 0x05;
		vwebase = 0x06;
		break;
	case SEN_OV7620:
		hwsbase = 0x2f;		/* From 7620.SET (spec is wrong) */
		hwebase = 0x2f;
		vwsbase = vwebase = 0x05;
		break;
	default:
		err("Invalid sensor");
		return -EINVAL;
	}

	if (ov->sensor == SEN_OV6620 || ov->sensor == SEN_OV6630) {
		/* Note: OV518(+) does downsample on its own) */
		if ((width > 176 && height > 144)
		    || ov->bclass == BCL_OV518) {  /* CIF */
			ret = mode_init_ov_sensor_regs(ov, width, height,
				mode, sub_flag, 0);
			if (ret < 0)
				return ret;
			hwscale = 1;
			vwscale = 1;  /* The datasheet says 0; it's wrong */
			hwsize = 352;
			vwsize = 288;
		} else if (width > 176 || height > 144) {
			err("Illegal dimensions");
			return -EINVAL;
		} else {			    /* QCIF */
			ret = mode_init_ov_sensor_regs(ov, width, height,
				mode, sub_flag, 1);
			if (ret < 0)
				return ret;
			hwsize = 176;
			vwsize = 144;
		}
	} else {
		if (width > 320 && height > 240) {  /* VGA */
			ret = mode_init_ov_sensor_regs(ov, width, height,
				mode, sub_flag, 0);
			if (ret < 0)
				return ret;
			hwscale = 2;
			vwscale = 1;
			hwsize = 640;
			vwsize = 480;
		} else if (width > 320 || height > 240) {
			err("Illegal dimensions");
			return -EINVAL;
		} else {			    /* QVGA */
			ret = mode_init_ov_sensor_regs(ov, width, height,
				mode, sub_flag, 1);
			if (ret < 0)
				return ret;
			hwscale = 1;
			hwsize = 320;
			vwsize = 240;
		}
	}

	/* Center the window */
	hoffset = ((hwsize - width) / 2) >> hwscale;
	voffset = ((vwsize - height) / 2) >> vwscale;

	/* FIXME! - This needs to be changed to support 160x120 and 6620!!! */
	if (sub_flag) {
		i2c_w(ov, 0x17, hwsbase+(ov->subx>>hwscale));
		i2c_w(ov, 0x18,	hwebase+((ov->subx+ov->subw)>>hwscale));
		i2c_w(ov, 0x19, vwsbase+(ov->suby>>vwscale));
		i2c_w(ov, 0x1a, vwebase+((ov->suby+ov->subh)>>vwscale));
	} else {
		i2c_w(ov, 0x17, hwsbase + hoffset);
		i2c_w(ov, 0x18, hwebase + hoffset + (hwsize>>hwscale));
		i2c_w(ov, 0x19, vwsbase + voffset);
		i2c_w(ov, 0x1a, vwebase + voffset + (vwsize>>vwscale));
	}

#ifdef OV511_DEBUG
	if (dump_sensor)
		dump_i2c_regs(ov);
#endif

	return 0;
}

/* Set up the OV511/OV511+ with the given image parameters.
 *
 * Do not put any sensor-specific code in here (including I2C I/O functions)
 */
static int
ov511_mode_init_regs(struct usb_ov511 *ov,
		     int width, int height, int mode, int sub_flag)
{
	int hsegs, vsegs;

	if (sub_flag) {
		width = ov->subw;
		height = ov->subh;
	}

	PDEBUG(3, "width:%d, height:%d, mode:%d, sub:%d",
	       width, height, mode, sub_flag);

	// FIXME: This should be moved to a 7111a-specific function once
	// subcapture is dealt with properly
	if (ov->sensor == SEN_SAA7111A) {
		if (width == 320 && height == 240) {
			/* No need to do anything special */
		} else if (width == 640 && height == 480) {
			/* Set the OV511 up as 320x480, but keep the
			 * V4L resolution as 640x480 */
			width = 320;
		} else {
			err("SAA7111A only allows 320x240 or 640x480");
			return -EINVAL;
		}
	}

	/* Make sure width and height are a multiple of 8 */
	if (width % 8 || height % 8) {
		err("Invalid size (%d, %d) (mode = %d)", width, height, mode);
		return -EINVAL;
	}

	if (width < ov->minwidth || height < ov->minheight) {
		err("Requested dimensions are too small");
		return -EINVAL;
	}

	if (ov51x_stop(ov) < 0)
		return -EIO;

	if (mode == VIDEO_PALETTE_GREY) {
		reg_w(ov, R511_CAM_UV_EN, 0x00);
		reg_w(ov, R511_SNAP_UV_EN, 0x00);
		reg_w(ov, R511_SNAP_OPTS, 0x01);
	} else {
		reg_w(ov, R511_CAM_UV_EN, 0x01);
		reg_w(ov, R511_SNAP_UV_EN, 0x01);
		reg_w(ov, R511_SNAP_OPTS, 0x03);
	}

	/* Here I'm assuming that snapshot size == image size.
	 * I hope that's always true. --claudio
	 */
	hsegs = (width >> 3) - 1;
	vsegs = (height >> 3) - 1;

	reg_w(ov, R511_CAM_PXCNT, hsegs);
	reg_w(ov, R511_CAM_LNCNT, vsegs);
	reg_w(ov, R511_CAM_PXDIV, 0x00);
	reg_w(ov, R511_CAM_LNDIV, 0x00);

	/* YUV420, low pass filter on */
	reg_w(ov, R511_CAM_OPTS, 0x03);

	/* Snapshot additions */
	reg_w(ov, R511_SNAP_PXCNT, hsegs);
	reg_w(ov, R511_SNAP_LNCNT, vsegs);
	reg_w(ov, R511_SNAP_PXDIV, 0x00);
	reg_w(ov, R511_SNAP_LNDIV, 0x00);

	if (ov->compress) {
		/* Enable Y and UV quantization and compression */
		reg_w(ov, R511_COMP_EN, 0x07);
		reg_w(ov, R511_COMP_LUT_EN, 0x03);
		ov51x_reset(ov, OV511_RESET_OMNICE);
	}

	if (ov51x_restart(ov) < 0)
		return -EIO;

	return 0;
}

/* Sets up the OV518/OV518+ with the given image parameters
 *
 * OV518 needs a completely different approach, until we can figure out what
 * the individual registers do. Also, only 15 FPS is supported now.
 *
 * Do not put any sensor-specific code in here (including I2C I/O functions)
 */
static int
ov518_mode_init_regs(struct usb_ov511 *ov,
		     int width, int height, int mode, int sub_flag)
{
	int hsegs, vsegs, hi_res;

	if (sub_flag) {
		width = ov->subw;
		height = ov->subh;
	}

	PDEBUG(3, "width:%d, height:%d, mode:%d, sub:%d",
	       width, height, mode, sub_flag);

	if (width % 16 || height % 8) {
		err("Invalid size (%d, %d)", width, height);
		return -EINVAL;
	}

	if (width < ov->minwidth || height < ov->minheight) {
		err("Requested dimensions are too small");
		return -EINVAL;
	}

	if (width >= 320 && height >= 240) {
		hi_res = 1;
	} else if (width >= 320 || height >= 240) {
		err("Invalid width/height combination (%d, %d)", width, height);
		return -EINVAL;
	} else {
		hi_res = 0;
	}

	if (ov51x_stop(ov) < 0)
		return -EIO;

	/******** Set the mode ********/

	reg_w(ov, 0x2b, 0);
	reg_w(ov, 0x2c, 0);
	reg_w(ov, 0x2d, 0);
	reg_w(ov, 0x2e, 0);
	reg_w(ov, 0x3b, 0);
	reg_w(ov, 0x3c, 0);
	reg_w(ov, 0x3d, 0);
	reg_w(ov, 0x3e, 0);

	if (ov->bridge == BRG_OV518 && ov518_color) {
		/* OV518 needs U and V swapped */
		i2c_w_mask(ov, 0x15, 0x00, 0x01);

		if (mode == VIDEO_PALETTE_GREY) {
			/* Set 16-bit input format (UV data are ignored) */
			reg_w_mask(ov, 0x20, 0x00, 0x08);

			/* Set 8-bit (4:0:0) output format */
			reg_w_mask(ov, 0x28, 0x00, 0xf0);
			reg_w_mask(ov, 0x38, 0x00, 0xf0);
		} else {
			/* Set 8-bit (YVYU) input format */
			reg_w_mask(ov, 0x20, 0x08, 0x08);

			/* Set 12-bit (4:2:0) output format */
			reg_w_mask(ov, 0x28, 0x80, 0xf0);
			reg_w_mask(ov, 0x38, 0x80, 0xf0);
		}
	} else {
		reg_w(ov, 0x28, (mode == VIDEO_PALETTE_GREY) ? 0x00:0x80);
		reg_w(ov, 0x38, (mode == VIDEO_PALETTE_GREY) ? 0x00:0x80);
	}

	hsegs = width / 16;
	vsegs = height / 4;

	reg_w(ov, 0x29, hsegs);
	reg_w(ov, 0x2a, vsegs);

	reg_w(ov, 0x39, hsegs);
	reg_w(ov, 0x3a, vsegs);

	/* Windows driver does this here; who knows why */
	reg_w(ov, 0x2f, 0x80);

	/******** Set the framerate (to 15 FPS) ********/

	/* Mode independent, but framerate dependent, regs */
	reg_w(ov, 0x51, 0x02);	/* Clock divider; lower==faster */
	reg_w(ov, 0x22, 0x18);
	reg_w(ov, 0x23, 0xff);

	if (ov->bridge == BRG_OV518PLUS)
		reg_w(ov, 0x21, 0x19);
	else
		reg_w(ov, 0x71, 0x19);	/* Compression-related? */

	// FIXME: Sensor-specific
	/* Bit 5 is what matters here. Of course, it is "reserved" */
	i2c_w(ov, 0x54, 0x23);

	reg_w(ov, 0x2f, 0x80);

	if (ov->bridge == BRG_OV518PLUS) {
		reg_w(ov, 0x24, 0x94);
		reg_w(ov, 0x25, 0x90);
		ov518_reg_w32(ov, 0xc4,    400, 2);	/* 190h   */
		ov518_reg_w32(ov, 0xc6,    540, 2);	/* 21ch   */
		ov518_reg_w32(ov, 0xc7,    540, 2);	/* 21ch   */
		ov518_reg_w32(ov, 0xc8,    108, 2);	/* 6ch    */
		ov518_reg_w32(ov, 0xca, 131098, 3);	/* 2001ah */
		ov518_reg_w32(ov, 0xcb,    532, 2);	/* 214h   */
		ov518_reg_w32(ov, 0xcc,   2400, 2);	/* 960h   */
		ov518_reg_w32(ov, 0xcd,     32, 2);	/* 20h    */
		ov518_reg_w32(ov, 0xce,    608, 2);	/* 260h   */
	} else {
		reg_w(ov, 0x24, 0x9f);
		reg_w(ov, 0x25, 0x90);
		ov518_reg_w32(ov, 0xc4,    400, 2);	/* 190h   */
		ov518_reg_w32(ov, 0xc6,    500, 2);	/* 1f4h   */
		ov518_reg_w32(ov, 0xc7,    500, 2);	/* 1f4h   */
		ov518_reg_w32(ov, 0xc8,    142, 2);	/* 8eh    */
		ov518_reg_w32(ov, 0xca, 131098, 3);	/* 2001ah */
		ov518_reg_w32(ov, 0xcb,    532, 2);	/* 214h   */
		ov518_reg_w32(ov, 0xcc,   2000, 2);	/* 7d0h   */
		ov518_reg_w32(ov, 0xcd,     32, 2);	/* 20h    */
		ov518_reg_w32(ov, 0xce,    608, 2);	/* 260h   */
	}

	reg_w(ov, 0x2f, 0x80);

	if (ov51x_restart(ov) < 0)
		return -EIO;

	/* Reset it just for good measure */
	if (ov51x_reset(ov, OV511_RESET_NOREGS) < 0)
		return -EIO;

	return 0;
}

/* This is a wrapper around the OV511, OV518, and sensor specific functions */
static int
mode_init_regs(struct usb_ov511 *ov,
	       int width, int height, int mode, int sub_flag)
{
	int rc = 0;

	if (!ov || !ov->dev)
		return -EFAULT;

	if (ov->bclass == BCL_OV518) {
		rc = ov518_mode_init_regs(ov, width, height, mode, sub_flag);
	} else {
		rc = ov511_mode_init_regs(ov, width, height, mode, sub_flag);
	}

	if (FATAL_ERROR(rc))
		return rc;

	switch (ov->sensor) {
	case SEN_OV7610:
	case SEN_OV7620:
	case SEN_OV76BE:
	case SEN_OV8600:
	case SEN_OV6620:
	case SEN_OV6630:
		rc = set_ov_sensor_window(ov, width, height, mode, sub_flag);
		break;
	case SEN_KS0127:
	case SEN_KS0127B:
		err("KS0127-series decoders not supported yet");
		rc = -EINVAL;
		break;
	case SEN_SAA7111A:
//		rc = mode_init_saa_sensor_regs(ov, width, height, mode,
//					       sub_flag);

		PDEBUG(1, "SAA status = 0x%02X", i2c_r(ov, 0x1f));
		break;
	default:
		err("Unknown sensor");
		rc = -EINVAL;
	}

	if (FATAL_ERROR(rc))
		return rc;

	/* Sensor-independent settings */
	rc = sensor_set_auto_brightness(ov, ov->auto_brt);
	if (FATAL_ERROR(rc))
		return rc;

	rc = sensor_set_auto_exposure(ov, ov->auto_exp);
	if (FATAL_ERROR(rc))
		return rc;

	rc = sensor_set_banding_filter(ov, bandingfilter);
	if (FATAL_ERROR(rc))
		return rc;

	if (ov->lightfreq) {
		rc = sensor_set_light_freq(ov, lightfreq);
		if (FATAL_ERROR(rc))
			return rc;
	}

	rc = sensor_set_backlight(ov, ov->backlight);
	if (FATAL_ERROR(rc))
		return rc;

	rc = sensor_set_mirror(ov, ov->mirror);
	if (FATAL_ERROR(rc))
		return rc;

	return 0;
}

/* This sets the default image parameters. This is useful for apps that use
 * read() and do not set these.
 */
static int
ov51x_set_default_params(struct usb_ov511 *ov)
{
	int i;

	/* Set default sizes in case IOCTL (VIDIOCMCAPTURE) is not used
	 * (using read() instead). */
	for (i = 0; i < OV511_NUMFRAMES; i++) {
		ov->frame[i].width = ov->maxwidth;
		ov->frame[i].height = ov->maxheight;
		ov->frame[i].bytes_read = 0;
		if (force_palette)
			ov->frame[i].format = force_palette;
		else
			ov->frame[i].format = VIDEO_PALETTE_YUV420;

		ov->frame[i].depth = get_depth(ov->frame[i].format);
	}

	PDEBUG(3, "%dx%d, %s", ov->maxwidth, ov->maxheight,
	       symbolic(v4l1_plist, ov->frame[0].format));

	/* Initialize to max width/height, YUV420 or RGB24 (if supported) */
	if (mode_init_regs(ov, ov->maxwidth, ov->maxheight,
			   ov->frame[0].format, 0) < 0)
		return -EINVAL;

	return 0;
}

/**********************************************************************
 *
 * Video decoder stuff
 *
 **********************************************************************/

/* Set analog input port of decoder */
static int
decoder_set_input(struct usb_ov511 *ov, int input)
{
	PDEBUG(4, "port %d", input);

	switch (ov->sensor) {
	case SEN_SAA7111A:
	{
		/* Select mode */
		i2c_w_mask(ov, 0x02, input, 0x07);
		/* Bypass chrominance trap for modes 4..7 */
		i2c_w_mask(ov, 0x09, (input > 3) ? 0x80:0x00, 0x80);
		break;
	}
	default:
		return -EINVAL;
	}

	return 0;
}

/* Get ASCII name of video input */
static int
decoder_get_input_name(struct usb_ov511 *ov, int input, char *name)
{
	switch (ov->sensor) {
	case SEN_SAA7111A:
	{
		if (input < 0 || input > 7)
			return -EINVAL;
		else if (input < 4)
			sprintf(name, "CVBS-%d", input);
		else // if (input < 8)
			sprintf(name, "S-Video-%d", input - 4);
		break;
	}
	default:
		sprintf(name, "%s", "Camera");
	}

	return 0;
}

/* Set norm (NTSC, PAL, SECAM, AUTO) */
static int
decoder_set_norm(struct usb_ov511 *ov, int norm)
{
	PDEBUG(4, "%d", norm);

	switch (ov->sensor) {
	case SEN_SAA7111A:
	{
		int reg_8, reg_e;

		if (norm == VIDEO_MODE_NTSC) {
			reg_8 = 0x40;	/* 60 Hz */
			reg_e = 0x00;	/* NTSC M / PAL BGHI */
		} else if (norm == VIDEO_MODE_PAL) {
			reg_8 = 0x00;	/* 50 Hz */
			reg_e = 0x00;	/* NTSC M / PAL BGHI */
		} else if (norm == VIDEO_MODE_AUTO) {
			reg_8 = 0x80;	/* Auto field detect */
			reg_e = 0x00;	/* NTSC M / PAL BGHI */
		} else if (norm == VIDEO_MODE_SECAM) {
			reg_8 = 0x00;	/* 50 Hz */
			reg_e = 0x50;	/* SECAM / PAL 4.43 */
		} else {
			return -EINVAL;
		}

		i2c_w_mask(ov, 0x08, reg_8, 0xc0);
		i2c_w_mask(ov, 0x0e, reg_e, 0x70);
		break;
	}
	default:
		return -EINVAL;
	}

	return 0;
}

/**********************************************************************
 *
 * Raw data parsing
 *
 **********************************************************************/

/* Copies a 64-byte segment at pIn to an 8x8 block at pOut. The width of the
 * image at pOut is specified by w.
 */
static inline void
make_8x8(unsigned char *pIn, unsigned char *pOut, int w)
{
	unsigned char *pOut1 = pOut;
	int x, y;

	for (y = 0; y < 8; y++) {
		pOut1 = pOut;
		for (x = 0; x < 8; x++) {
			*pOut1++ = *pIn++;
		}
		pOut += w;
	}
}

/*
 * For RAW BW (YUV 4:0:0) images, data show up in 256 byte segments.
 * The segments represent 4 squares of 8x8 pixels as follows:
 *
 *      0  1 ...  7    64  65 ...  71   ...  192 193 ... 199
 *      8  9 ... 15    72  73 ...  79        200 201 ... 207
 *           ...              ...                    ...
 *     56 57 ... 63   120 121 ... 127        248 249 ... 255
 *
 */
static void
yuv400raw_to_yuv400p(struct ov511_frame *frame,
		     unsigned char *pIn0, unsigned char *pOut0)
{
	int x, y;
	unsigned char *pIn, *pOut, *pOutLine;

	/* Copy Y */
	pIn = pIn0;
	pOutLine = pOut0;
	for (y = 0; y < frame->rawheight - 1; y += 8) {
		pOut = pOutLine;
		for (x = 0; x < frame->rawwidth - 1; x += 8) {
			make_8x8(pIn, pOut, frame->rawwidth);
			pIn += 64;
			pOut += 8;
		}
		pOutLine += 8 * frame->rawwidth;
	}
}

/*
 * For YUV 4:2:0 images, the data show up in 384 byte segments.
 * The first 64 bytes of each segment are U, the next 64 are V.  The U and
 * V are arranged as follows:
 *
 *      0  1 ...  7
 *      8  9 ... 15
 *           ...
 *     56 57 ... 63
 *
 * U and V are shipped at half resolution (1 U,V sample -> one 2x2 block).
 *
 * The next 256 bytes are full resolution Y data and represent 4 squares
 * of 8x8 pixels as follows:
 *
 *      0  1 ...  7    64  65 ...  71   ...  192 193 ... 199
 *      8  9 ... 15    72  73 ...  79        200 201 ... 207
 *           ...              ...                    ...
 *     56 57 ... 63   120 121 ... 127   ...  248 249 ... 255
 *
 * Note that the U and V data in one segment represent a 16 x 16 pixel
 * area, but the Y data represent a 32 x 8 pixel area. If the width is not an
 * even multiple of 32, the extra 8x8 blocks within a 32x8 block belong to the
 * next horizontal stripe.
 *
 * If dumppix module param is set, _parse_data just dumps the incoming segments,
 * verbatim, in order, into the frame. When used with vidcat -f ppm -s 640x480
 * this puts the data on the standard output and can be analyzed with the
 * parseppm.c utility I wrote.  That's a much faster way for figuring out how
 * these data are scrambled.
 */

/* Converts from raw, uncompressed segments at pIn0 to a YUV420P frame at pOut0.
 *
 * FIXME: Currently only handles width and height that are multiples of 16
 */
static void
yuv420raw_to_yuv420p(struct ov511_frame *frame,
		     unsigned char *pIn0, unsigned char *pOut0)
{
	int k, x, y;
	unsigned char *pIn, *pOut, *pOutLine;
	const unsigned int a = frame->rawwidth * frame->rawheight;
	const unsigned int w = frame->rawwidth / 2;

	/* Copy U and V */
	pIn = pIn0;
	pOutLine = pOut0 + a;
	for (y = 0; y < frame->rawheight - 1; y += 16) {
		pOut = pOutLine;
		for (x = 0; x < frame->rawwidth - 1; x += 16) {
			make_8x8(pIn, pOut, w);
			make_8x8(pIn + 64, pOut + a/4, w);
			pIn += 384;
			pOut += 8;
		}
		pOutLine += 8 * w;
	}

	/* Copy Y */
	pIn = pIn0 + 128;
	pOutLine = pOut0;
	k = 0;
	for (y = 0; y < frame->rawheight - 1; y += 8) {
		pOut = pOutLine;
		for (x = 0; x < frame->rawwidth - 1; x += 8) {
			make_8x8(pIn, pOut, frame->rawwidth);
			pIn += 64;
			pOut += 8;
			if ((++k) > 3) {
				k = 0;
				pIn += 128;
			}
		}
		pOutLine += 8 * frame->rawwidth;
	}
}

/**********************************************************************
 *
 * Decompression
 *
 **********************************************************************/

static int
request_decompressor(struct usb_ov511 *ov)
{
	if (ov->bclass == BCL_OV511 || ov->bclass == BCL_OV518) {
		err("No decompressor available");
	} else {
		err("Unknown bridge");
	}

	return -ENOSYS;
}

static void
decompress(struct usb_ov511 *ov, struct ov511_frame *frame,
	   unsigned char *pIn0, unsigned char *pOut0)
{
	if (!ov->decomp_ops)
		if (request_decompressor(ov))
			return;

}

/**********************************************************************
 *
 * Format conversion
 *
 **********************************************************************/

/* Fuses even and odd fields together, and doubles width.
 * INPUT: an odd field followed by an even field at pIn0, in YUV planar format
 * OUTPUT: a normal YUV planar image, with correct aspect ratio
 */
static void
deinterlace(struct ov511_frame *frame, int rawformat,
	    unsigned char *pIn0, unsigned char *pOut0)
{
	const int fieldheight = frame->rawheight / 2;
	const int fieldpix = fieldheight * frame->rawwidth;
	const int w = frame->width;
	int x, y;
	unsigned char *pInEven, *pInOdd, *pOut;

	PDEBUG(5, "fieldheight=%d", fieldheight);

	if (frame->rawheight != frame->height) {
		err("invalid height");
		return;
	}

	if ((frame->rawwidth * 2) != frame->width) {
		err("invalid width");
		return;
	}

	/* Y */
	pInOdd = pIn0;
	pInEven = pInOdd + fieldpix;
	pOut = pOut0;
	for (y = 0; y < fieldheight; y++) {
		for (x = 0; x < frame->rawwidth; x++) {
			*pOut = *pInEven;
			*(pOut+1) = *pInEven++;
			*(pOut+w) = *pInOdd;
			*(pOut+w+1) = *pInOdd++;
			pOut += 2;
		}
		pOut += w;
	}

	if (rawformat == RAWFMT_YUV420) {
	/* U */
		pInOdd = pIn0 + fieldpix * 2;
		pInEven = pInOdd + fieldpix / 4;
		for (y = 0; y < fieldheight / 2; y++) {
			for (x = 0; x < frame->rawwidth / 2; x++) {
				*pOut = *pInEven;
				*(pOut+1) = *pInEven++;
				*(pOut+w/2) = *pInOdd;
				*(pOut+w/2+1) = *pInOdd++;
				pOut += 2;
			}
			pOut += w/2;
		}
	/* V */
		pInOdd = pIn0 + fieldpix * 2 + fieldpix / 2;
		pInEven = pInOdd + fieldpix / 4;
		for (y = 0; y < fieldheight / 2; y++) {
			for (x = 0; x < frame->rawwidth / 2; x++) {
				*pOut = *pInEven;
				*(pOut+1) = *pInEven++;
				*(pOut+w/2) = *pInOdd;
				*(pOut+w/2+1) = *pInOdd++;
				pOut += 2;
			}
			pOut += w/2;
		}
	}
}

static void
ov51x_postprocess_grey(struct usb_ov511 *ov, struct ov511_frame *frame)
{
		/* Deinterlace frame, if necessary */
		if (ov->sensor == SEN_SAA7111A && frame->rawheight >= 480) {
			if (frame->compressed)
				decompress(ov, frame, frame->rawdata,
						 frame->tempdata);
			else
				yuv400raw_to_yuv400p(frame, frame->rawdata,
						     frame->tempdata);

			deinterlace(frame, RAWFMT_YUV400, frame->tempdata,
				    frame->data);
		} else {
			if (frame->compressed)
				decompress(ov, frame, frame->rawdata,
						 frame->data);
			else
				yuv400raw_to_yuv400p(frame, frame->rawdata,
						     frame->data);
		}
}

/* Process raw YUV420 data into standard YUV420P */
static void
ov51x_postprocess_yuv420(struct usb_ov511 *ov, struct ov511_frame *frame)
{
	/* Deinterlace frame, if necessary */
	if (ov->sensor == SEN_SAA7111A && frame->rawheight >= 480) {
		if (frame->compressed)
			decompress(ov, frame, frame->rawdata, frame->tempdata);
		else
			yuv420raw_to_yuv420p(frame, frame->rawdata,
					     frame->tempdata);

		deinterlace(frame, RAWFMT_YUV420, frame->tempdata,
			    frame->data);
	} else {
		if (frame->compressed)
			decompress(ov, frame, frame->rawdata, frame->data);
		else
			yuv420raw_to_yuv420p(frame, frame->rawdata,
					     frame->data);
	}
}

/* Post-processes the specified frame. This consists of:
 * 	1. Decompress frame, if necessary
 *	2. Deinterlace frame and scale to proper size, if necessary
 * 	3. Convert from YUV planar to destination format, if necessary
 * 	4. Fix the RGB offset, if necessary
 */
static void
ov51x_postprocess(struct usb_ov511 *ov, struct ov511_frame *frame)
{
	if (dumppix) {
		memset(frame->data, 0,
			MAX_DATA_SIZE(ov->maxwidth, ov->maxheight));
		PDEBUG(4, "Dumping %d bytes", frame->bytes_recvd);
		memcpy(frame->data, frame->rawdata, frame->bytes_recvd);
	} else {
		switch (frame->format) {
		case VIDEO_PALETTE_GREY:
			ov51x_postprocess_grey(ov, frame);
			break;
		case VIDEO_PALETTE_YUV420:
		case VIDEO_PALETTE_YUV420P:
			ov51x_postprocess_yuv420(ov, frame);
			break;
		default:
			err("Cannot convert data to %s",
			    symbolic(v4l1_plist, frame->format));
		}
	}
}

/**********************************************************************
 *
 * OV51x data transfer, IRQ handler
 *
 **********************************************************************/

static inline void
ov511_move_data(struct usb_ov511 *ov, unsigned char *in, int n)
{
	int num, offset;
	int pnum = in[ov->packet_size - 1];		/* Get packet number */
	int max_raw = MAX_RAW_DATA_SIZE(ov->maxwidth, ov->maxheight);
	struct ov511_frame *frame = &ov->frame[ov->curframe];
	struct timeval *ts;

	/* SOF/EOF packets have 1st to 8th bytes zeroed and the 9th
	 * byte non-zero. The EOF packet has image width/height in the
	 * 10th and 11th bytes. The 9th byte is given as follows:
	 *
	 * bit 7: EOF
	 *     6: compression enabled
	 *     5: 422/420/400 modes
	 *     4: 422/420/400 modes
	 *     3: 1
	 *     2: snapshot button on
	 *     1: snapshot frame
	 *     0: even/odd field
	 */

	if (printph) {
		dev_info(&ov->dev->dev,
			 "ph(%3d): %2x %2x %2x %2x %2x %2x %2x %2x %2x %2x %2x %2x\n",
			 pnum, in[0], in[1], in[2], in[3], in[4], in[5], in[6],
			 in[7], in[8], in[9], in[10], in[11]);
	}

	/* Check for SOF/EOF packet */
	if ((in[0] | in[1] | in[2] | in[3] | in[4] | in[5] | in[6] | in[7]) ||
	    (~in[8] & 0x08))
		goto check_middle;

	/* Frame end */
	if (in[8] & 0x80) {
		ts = (struct timeval *)(frame->data
		      + MAX_FRAME_SIZE(ov->maxwidth, ov->maxheight));
		do_gettimeofday(ts);

		/* Get the actual frame size from the EOF header */
		frame->rawwidth = ((int)(in[9]) + 1) * 8;
		frame->rawheight = ((int)(in[10]) + 1) * 8;

		PDEBUG(4, "Frame end, frame=%d, pnum=%d, w=%d, h=%d, recvd=%d",
			ov->curframe, pnum, frame->rawwidth, frame->rawheight,
			frame->bytes_recvd);

		/* Validate the header data */
		RESTRICT_TO_RANGE(frame->rawwidth, ov->minwidth, ov->maxwidth);
		RESTRICT_TO_RANGE(frame->rawheight, ov->minheight,
				  ov->maxheight);

		/* Don't allow byte count to exceed buffer size */
		RESTRICT_TO_RANGE(frame->bytes_recvd, 8, max_raw);

		if (frame->scanstate == STATE_LINES) {
			int nextf;

			frame->grabstate = FRAME_DONE;
			wake_up_interruptible(&frame->wq);

			/* If next frame is ready or grabbing,
			 * point to it */
			nextf = (ov->curframe + 1) % OV511_NUMFRAMES;
			if (ov->frame[nextf].grabstate == FRAME_READY
			    || ov->frame[nextf].grabstate == FRAME_GRABBING) {
				ov->curframe = nextf;
				ov->frame[nextf].scanstate = STATE_SCANNING;
			} else {
				if (frame->grabstate == FRAME_DONE) {
					PDEBUG(4, "** Frame done **");
				} else {
					PDEBUG(4, "Frame not ready? state = %d",
						ov->frame[nextf].grabstate);
				}

				ov->curframe = -1;
			}
		} else {
			PDEBUG(5, "Frame done, but not scanning");
		}
		/* Image corruption caused by misplaced frame->segment = 0
		 * fixed by carlosf@conectiva.com.br
		 */
	} else {
		/* Frame start */
		PDEBUG(4, "Frame start, framenum = %d", ov->curframe);

		/* Check to see if it's a snapshot frame */
		/* FIXME?? Should the snapshot reset go here? Performance? */
		if (in[8] & 0x02) {
			frame->snapshot = 1;
			PDEBUG(3, "snapshot detected");
		}

		frame->scanstate = STATE_LINES;
		frame->bytes_recvd = 0;
		frame->compressed = in[8] & 0x40;
	}

check_middle:
	/* Are we in a frame? */
	if (frame->scanstate != STATE_LINES) {
		PDEBUG(5, "Not in a frame; packet skipped");
		return;
	}

	/* If frame start, skip header */
	if (frame->bytes_recvd == 0)
		offset = 9;
	else
		offset = 0;

	num = n - offset - 1;

	/* Dump all data exactly as received */
	if (dumppix == 2) {
		frame->bytes_recvd += n - 1;
		if (frame->bytes_recvd <= max_raw)
			memcpy(frame->rawdata + frame->bytes_recvd - (n - 1),
				in, n - 1);
		else
			PDEBUG(3, "Raw data buffer overrun!! (%d)",
				frame->bytes_recvd - max_raw);
	} else if (!frame->compressed && !remove_zeros) {
		frame->bytes_recvd += num;
		if (frame->bytes_recvd <= max_raw)
			memcpy(frame->rawdata + frame->bytes_recvd - num,
				in + offset, num);
		else
			PDEBUG(3, "Raw data buffer overrun!! (%d)",
				frame->bytes_recvd - max_raw);
	} else { /* Remove all-zero FIFO lines (aligned 32-byte blocks) */
		int b, read = 0, allzero, copied = 0;
		if (offset) {
			frame->bytes_recvd += 32 - offset;	// Bytes out
			memcpy(frame->rawdata,	in + offset, 32 - offset);
			read += 32;
		}

		while (read < n - 1) {
			allzero = 1;
			for (b = 0; b < 32; b++) {
				if (in[read + b]) {
					allzero = 0;
					break;
				}
			}

			if (allzero) {
				/* Don't copy it */
			} else {
				if (frame->bytes_recvd + copied + 32 <= max_raw)
				{
					memcpy(frame->rawdata
						+ frame->bytes_recvd + copied,
						in + read, 32);
					copied += 32;
				} else {
					PDEBUG(3, "Raw data buffer overrun!!");
				}
			}
			read += 32;
		}

		frame->bytes_recvd += copied;
	}
}

static inline void
ov518_move_data(struct usb_ov511 *ov, unsigned char *in, int n)
{
	int max_raw = MAX_RAW_DATA_SIZE(ov->maxwidth, ov->maxheight);
	struct ov511_frame *frame = &ov->frame[ov->curframe];
	struct timeval *ts;

	/* Don't copy the packet number byte */
	if (ov->packet_numbering)
		--n;

	/* A false positive here is likely, until OVT gives me
	 * the definitive SOF/EOF format */
	if ((!(in[0] | in[1] | in[2] | in[3] | in[5])) && in[6]) {
		if (printph) {
			dev_info(&ov->dev->dev,
				 "ph: %2x %2x %2x %2x %2x %2x %2x %2x\n",
				 in[0], in[1], in[2], in[3], in[4], in[5],
				 in[6], in[7]);
		}

		if (frame->scanstate == STATE_LINES) {
			PDEBUG(4, "Detected frame end/start");
			goto eof;
		} else { //scanstate == STATE_SCANNING
			/* Frame start */
			PDEBUG(4, "Frame start, framenum = %d", ov->curframe);
			goto sof;
		}
	} else {
		goto check_middle;
	}

eof:
	ts = (struct timeval *)(frame->data
	      + MAX_FRAME_SIZE(ov->maxwidth, ov->maxheight));
	do_gettimeofday(ts);

	PDEBUG(4, "Frame end, curframe = %d, hw=%d, vw=%d, recvd=%d",
		ov->curframe,
		(int)(in[9]), (int)(in[10]), frame->bytes_recvd);

	// FIXME: Since we don't know the header formats yet,
	// there is no way to know what the actual image size is
	frame->rawwidth = frame->width;
	frame->rawheight = frame->height;

	/* Validate the header data */
	RESTRICT_TO_RANGE(frame->rawwidth, ov->minwidth, ov->maxwidth);
	RESTRICT_TO_RANGE(frame->rawheight, ov->minheight, ov->maxheight);

	/* Don't allow byte count to exceed buffer size */
	RESTRICT_TO_RANGE(frame->bytes_recvd, 8, max_raw);

	if (frame->scanstate == STATE_LINES) {
		int nextf;

		frame->grabstate = FRAME_DONE;
		wake_up_interruptible(&frame->wq);

		/* If next frame is ready or grabbing,
		 * point to it */
		nextf = (ov->curframe + 1) % OV511_NUMFRAMES;
		if (ov->frame[nextf].grabstate == FRAME_READY
		    || ov->frame[nextf].grabstate == FRAME_GRABBING) {
			ov->curframe = nextf;
			ov->frame[nextf].scanstate = STATE_SCANNING;
			frame = &ov->frame[nextf];
		} else {
			if (frame->grabstate == FRAME_DONE) {
				PDEBUG(4, "** Frame done **");
			} else {
				PDEBUG(4, "Frame not ready? state = %d",
				       ov->frame[nextf].grabstate);
			}

			ov->curframe = -1;
			PDEBUG(4, "SOF dropped (no active frame)");
			return;  /* Nowhere to store this frame */
		}
	}
sof:
	PDEBUG(4, "Starting capture on frame %d", frame->framenum);

// Snapshot not reverse-engineered yet.
#if 0
	/* Check to see if it's a snapshot frame */
	/* FIXME?? Should the snapshot reset go here? Performance? */
	if (in[8] & 0x02) {
		frame->snapshot = 1;
		PDEBUG(3, "snapshot detected");
	}
#endif
	frame->scanstate = STATE_LINES;
	frame->bytes_recvd = 0;
	frame->compressed = 1;

check_middle:
	/* Are we in a frame? */
	if (frame->scanstate != STATE_LINES) {
		PDEBUG(4, "scanstate: no SOF yet");
		return;
	}

	/* Dump all data exactly as received */
	if (dumppix == 2) {
		frame->bytes_recvd += n;
		if (frame->bytes_recvd <= max_raw)
			memcpy(frame->rawdata + frame->bytes_recvd - n, in, n);
		else
			PDEBUG(3, "Raw data buffer overrun!! (%d)",
				frame->bytes_recvd - max_raw);
	} else {
		/* All incoming data are divided into 8-byte segments. If the
		 * segment contains all zero bytes, it must be skipped. These
		 * zero-segments allow the OV518 to mainain a constant data rate
		 * regardless of the effectiveness of the compression. Segments
		 * are aligned relative to the beginning of each isochronous
		 * packet. The first segment in each image is a header (the
		 * decompressor skips it later).
		 */

		int b, read = 0, allzero, copied = 0;

		while (read < n) {
			allzero = 1;
			for (b = 0; b < 8; b++) {
				if (in[read + b]) {
					allzero = 0;
					break;
				}
			}

			if (allzero) {
			/* Don't copy it */
			} else {
				if (frame->bytes_recvd + copied + 8 <= max_raw)
				{
					memcpy(frame->rawdata
						+ frame->bytes_recvd + copied,
						in + read, 8);
					copied += 8;
				} else {
					PDEBUG(3, "Raw data buffer overrun!!");
				}
			}
			read += 8;
		}
		frame->bytes_recvd += copied;
	}
}

static void
ov51x_isoc_irq(struct urb *urb)
{
	int i;
	struct usb_ov511 *ov;
	struct ov511_sbuf *sbuf;

	if (!urb->context) {
		PDEBUG(4, "no context");
		return;
	}

	sbuf = urb->context;
	ov = sbuf->ov;

	if (!ov || !ov->dev || !ov->user) {
		PDEBUG(4, "no device, or not open");
		return;
	}

	if (!ov->streaming) {
		PDEBUG(4, "hmmm... not streaming, but got interrupt");
		return;
	}

	if (urb->status == -ENOENT || urb->status == -ECONNRESET) {
		PDEBUG(4, "URB unlinked");
		return;
	}

	if (urb->status != -EINPROGRESS && urb->status != 0) {
		err("ERROR: urb->status=%d: %s", urb->status,
		    symbolic(urb_errlist, urb->status));
	}

	/* Copy the data received into our frame buffer */
	PDEBUG(5, "sbuf[%d]: Moving %d packets", sbuf->n,
	       urb->number_of_packets);
	for (i = 0; i < urb->number_of_packets; i++) {
		/* Warning: Don't call *_move_data() if no frame active! */
		if (ov->curframe >= 0) {
			int n = urb->iso_frame_desc[i].actual_length;
			int st = urb->iso_frame_desc[i].status;
			unsigned char *cdata;

			urb->iso_frame_desc[i].actual_length = 0;
			urb->iso_frame_desc[i].status = 0;

			cdata = urb->transfer_buffer
				+ urb->iso_frame_desc[i].offset;

			if (!n) {
				PDEBUG(4, "Zero-length packet");
				continue;
			}

			if (st)
				PDEBUG(2, "data error: [%d] len=%d, status=%d",
				       i, n, st);

			if (ov->bclass == BCL_OV511)
				ov511_move_data(ov, cdata, n);
			else if (ov->bclass == BCL_OV518)
				ov518_move_data(ov, cdata, n);
			else
				err("Unknown bridge device (%d)", ov->bridge);

		} else if (waitqueue_active(&ov->wq)) {
			wake_up_interruptible(&ov->wq);
		}
	}

	/* Resubmit this URB */
	urb->dev = ov->dev;
	if ((i = usb_submit_urb(urb, GFP_ATOMIC)) != 0)
		err("usb_submit_urb() ret %d", i);

	return;
}

/****************************************************************************
 *
 * Stream initialization and termination
 *
 ***************************************************************************/

static int
ov51x_init_isoc(struct usb_ov511 *ov)
{
	struct urb *urb;
	int fx, err, n, i, size;

	PDEBUG(3, "*** Initializing capture ***");

	ov->curframe = -1;

	if (ov->bridge == BRG_OV511) {
		if (cams == 1)
			size = 993;
		else if (cams == 2)
			size = 513;
		else if (cams == 3 || cams == 4)
			size = 257;
		else {
			err("\"cams\" parameter too high!");
			return -1;
		}
	} else if (ov->bridge == BRG_OV511PLUS) {
		if (cams == 1)
			size = 961;
		else if (cams == 2)
			size = 513;
		else if (cams == 3 || cams == 4)
			size = 257;
		else if (cams >= 5 && cams <= 8)
			size = 129;
		else if (cams >= 9 && cams <= 31)
			size = 33;
		else {
			err("\"cams\" parameter too high!");
			return -1;
		}
	} else if (ov->bclass == BCL_OV518) {
		if (cams == 1)
			size = 896;
		else if (cams == 2)
			size = 512;
		else if (cams == 3 || cams == 4)
			size = 256;
		else if (cams >= 5 && cams <= 8)
			size = 128;
		else {
			err("\"cams\" parameter too high!");
			return -1;
		}
	} else {
		err("invalid bridge type");
		return -1;
	}

	// FIXME: OV518 is hardcoded to 15 FPS (alternate 5) for now
	if (ov->bclass == BCL_OV518) {
		if (packetsize == -1) {
			ov518_set_packet_size(ov, 640);
		} else {
			dev_info(&ov->dev->dev, "Forcing packet size to %d\n",
				 packetsize);
			ov518_set_packet_size(ov, packetsize);
		}
	} else {
		if (packetsize == -1) {
			ov511_set_packet_size(ov, size);
		} else {
			dev_info(&ov->dev->dev, "Forcing packet size to %d\n",
				 packetsize);
			ov511_set_packet_size(ov, packetsize);
		}
	}

	for (n = 0; n < OV511_NUMSBUF; n++) {
		urb = usb_alloc_urb(FRAMES_PER_DESC, GFP_KERNEL);
		if (!urb) {
			err("init isoc: usb_alloc_urb ret. NULL");
			for (i = 0; i < n; i++)
				usb_free_urb(ov->sbuf[i].urb);
			return -ENOMEM;
		}
		ov->sbuf[n].urb = urb;
		urb->dev = ov->dev;
		urb->context = &ov->sbuf[n];
		urb->pipe = usb_rcvisocpipe(ov->dev, OV511_ENDPOINT_ADDRESS);
		urb->transfer_flags = URB_ISO_ASAP;
		urb->transfer_buffer = ov->sbuf[n].data;
		urb->complete = ov51x_isoc_irq;
		urb->number_of_packets = FRAMES_PER_DESC;
		urb->transfer_buffer_length = ov->packet_size * FRAMES_PER_DESC;
		urb->interval = 1;
		for (fx = 0; fx < FRAMES_PER_DESC; fx++) {
			urb->iso_frame_desc[fx].offset = ov->packet_size * fx;
			urb->iso_frame_desc[fx].length = ov->packet_size;
		}
	}

	ov->streaming = 1;

	for (n = 0; n < OV511_NUMSBUF; n++) {
		ov->sbuf[n].urb->dev = ov->dev;
		err = usb_submit_urb(ov->sbuf[n].urb, GFP_KERNEL);
		if (err) {
			err("init isoc: usb_submit_urb(%d) ret %d", n, err);
			return err;
		}
	}

	return 0;
}

static void
ov51x_unlink_isoc(struct usb_ov511 *ov)
{
	int n;

	/* Unschedule all of the iso td's */
	for (n = OV511_NUMSBUF - 1; n >= 0; n--) {
		if (ov->sbuf[n].urb) {
			usb_kill_urb(ov->sbuf[n].urb);
			usb_free_urb(ov->sbuf[n].urb);
			ov->sbuf[n].urb = NULL;
		}
	}
}

static void
ov51x_stop_isoc(struct usb_ov511 *ov)
{
	if (!ov->streaming || !ov->dev)
		return;

	PDEBUG(3, "*** Stopping capture ***");

	if (ov->bclass == BCL_OV518)
		ov518_set_packet_size(ov, 0);
	else
		ov511_set_packet_size(ov, 0);

	ov->streaming = 0;

	ov51x_unlink_isoc(ov);
}

static int
ov51x_new_frame(struct usb_ov511 *ov, int framenum)
{
	struct ov511_frame *frame;
	int newnum;

	PDEBUG(4, "ov->curframe = %d, framenum = %d", ov->curframe, framenum);

	if (!ov->dev)
		return -1;

	/* If we're not grabbing a frame right now and the other frame is */
	/* ready to be grabbed into, then use it instead */
	if (ov->curframe == -1) {
		newnum = (framenum - 1 + OV511_NUMFRAMES) % OV511_NUMFRAMES;
		if (ov->frame[newnum].grabstate == FRAME_READY)
			framenum = newnum;
	} else
		return 0;

	frame = &ov->frame[framenum];

	PDEBUG(4, "framenum = %d, width = %d, height = %d", framenum,
	       frame->width, frame->height);

	frame->grabstate = FRAME_GRABBING;
	frame->scanstate = STATE_SCANNING;
	frame->snapshot = 0;

	ov->curframe = framenum;

	/* Make sure it's not too big */
	if (frame->width > ov->maxwidth)
		frame->width = ov->maxwidth;

	frame->width &= ~7L;		/* Multiple of 8 */

	if (frame->height > ov->maxheight)
		frame->height = ov->maxheight;

	frame->height &= ~3L;		/* Multiple of 4 */

	return 0;
}

/****************************************************************************
 *
 * Buffer management
 *
 ***************************************************************************/

/*
 * - You must acquire buf_lock before entering this function.
 * - Because this code will free any non-null pointer, you must be sure to null
 *   them if you explicitly free them somewhere else!
 */
static void
ov51x_do_dealloc(struct usb_ov511 *ov)
{
	int i;
	PDEBUG(4, "entered");

	if (ov->fbuf) {
		rvfree(ov->fbuf, OV511_NUMFRAMES
		       * MAX_DATA_SIZE(ov->maxwidth, ov->maxheight));
		ov->fbuf = NULL;
	}

	vfree(ov->rawfbuf);
	ov->rawfbuf = NULL;

	vfree(ov->tempfbuf);
	ov->tempfbuf = NULL;

	for (i = 0; i < OV511_NUMSBUF; i++) {
		kfree(ov->sbuf[i].data);
		ov->sbuf[i].data = NULL;
	}

	for (i = 0; i < OV511_NUMFRAMES; i++) {
		ov->frame[i].data = NULL;
		ov->frame[i].rawdata = NULL;
		ov->frame[i].tempdata = NULL;
		if (ov->frame[i].compbuf) {
			free_page((unsigned long) ov->frame[i].compbuf);
			ov->frame[i].compbuf = NULL;
		}
	}

	PDEBUG(4, "buffer memory deallocated");
	ov->buf_state = BUF_NOT_ALLOCATED;
	PDEBUG(4, "leaving");
}

static int
ov51x_alloc(struct usb_ov511 *ov)
{
	int i;
	const int w = ov->maxwidth;
	const int h = ov->maxheight;
	const int data_bufsize = OV511_NUMFRAMES * MAX_DATA_SIZE(w, h);
	const int raw_bufsize = OV511_NUMFRAMES * MAX_RAW_DATA_SIZE(w, h);

	PDEBUG(4, "entered");
	mutex_lock(&ov->buf_lock);

	if (ov->buf_state == BUF_ALLOCATED)
		goto out;

	ov->fbuf = rvmalloc(data_bufsize);
	if (!ov->fbuf)
		goto error;

	ov->rawfbuf = vmalloc(raw_bufsize);
	if (!ov->rawfbuf)
		goto error;

	memset(ov->rawfbuf, 0, raw_bufsize);

	ov->tempfbuf = vmalloc(raw_bufsize);
	if (!ov->tempfbuf)
		goto error;

	memset(ov->tempfbuf, 0, raw_bufsize);

	for (i = 0; i < OV511_NUMSBUF; i++) {
		ov->sbuf[i].data = kmalloc(FRAMES_PER_DESC *
			MAX_FRAME_SIZE_PER_DESC, GFP_KERNEL);
		if (!ov->sbuf[i].data)
			goto error;

		PDEBUG(4, "sbuf[%d] @ %p", i, ov->sbuf[i].data);
	}

	for (i = 0; i < OV511_NUMFRAMES; i++) {
		ov->frame[i].data = ov->fbuf + i * MAX_DATA_SIZE(w, h);
		ov->frame[i].rawdata = ov->rawfbuf
		 + i * MAX_RAW_DATA_SIZE(w, h);
		ov->frame[i].tempdata = ov->tempfbuf
		 + i * MAX_RAW_DATA_SIZE(w, h);

		ov->frame[i].compbuf =
		 (unsigned char *) __get_free_page(GFP_KERNEL);
		if (!ov->frame[i].compbuf)
			goto error;

		PDEBUG(4, "frame[%d] @ %p", i, ov->frame[i].data);
	}

	ov->buf_state = BUF_ALLOCATED;
out:
	mutex_unlock(&ov->buf_lock);
	PDEBUG(4, "leaving");
	return 0;
error:
	ov51x_do_dealloc(ov);
	mutex_unlock(&ov->buf_lock);
	PDEBUG(4, "errored");
	return -ENOMEM;
}

static void
ov51x_dealloc(struct usb_ov511 *ov)
{
	PDEBUG(4, "entered");
	mutex_lock(&ov->buf_lock);
	ov51x_do_dealloc(ov);
	mutex_unlock(&ov->buf_lock);
	PDEBUG(4, "leaving");
}

/****************************************************************************
 *
 * V4L 1 API
 *
 ***************************************************************************/

static int
ov51x_v4l1_open(struct file *file)
{
	struct video_device *vdev = video_devdata(file);
	struct usb_ov511 *ov = video_get_drvdata(vdev);
	int err, i;

	PDEBUG(4, "opening");

	mutex_lock(&ov->lock);

	err = -EBUSY;
	if (ov->user)
		goto out;

	ov->sub_flag = 0;

	/* In case app doesn't set them... */
	err = ov51x_set_default_params(ov);
	if (err < 0)
		goto out;

	/* Make sure frames are reset */
	for (i = 0; i < OV511_NUMFRAMES; i++) {
		ov->frame[i].grabstate = FRAME_UNUSED;
		ov->frame[i].bytes_read = 0;
	}

	/* If compression is on, make sure now that a
	 * decompressor can be loaded */
	if (ov->compress && !ov->decomp_ops) {
		err = request_decompressor(ov);
		if (err && !dumppix)
			goto out;
	}

	err = ov51x_alloc(ov);
	if (err < 0)
		goto out;

	err = ov51x_init_isoc(ov);
	if (err) {
		ov51x_dealloc(ov);
		goto out;
	}

	ov->user++;
	file->private_data = vdev;

	if (ov->led_policy == LED_AUTO)
		ov51x_led_control(ov, 1);

out:
	mutex_unlock(&ov->lock);
	return err;
}

static int
ov51x_v4l1_close(struct file *file)
{
	struct video_device *vdev = file->private_data;
	struct usb_ov511 *ov = video_get_drvdata(vdev);

	PDEBUG(4, "ov511_close");

	mutex_lock(&ov->lock);

	ov->user--;
	ov51x_stop_isoc(ov);

	if (ov->led_policy == LED_AUTO)
		ov51x_led_control(ov, 0);

	if (ov->dev)
		ov51x_dealloc(ov);

	mutex_unlock(&ov->lock);

	/* Device unplugged while open. Only a minimum of unregistration is done
	 * here; the disconnect callback already did the rest. */
	if (!ov->dev) {
		mutex_lock(&ov->cbuf_lock);
		kfree(ov->cbuf);
		ov->cbuf = NULL;
		mutex_unlock(&ov->cbuf_lock);

		ov51x_dealloc(ov);
		kfree(ov);
		ov = NULL;
	}

	file->private_data = NULL;
	return 0;
}

/* Do not call this function directly! */
static long
ov51x_v4l1_ioctl_internal(struct file *file, unsigned int cmd, void *arg)
{
	struct video_device *vdev = file->private_data;
	struct usb_ov511 *ov = video_get_drvdata(vdev);
	PDEBUG(5, "IOCtl: 0x%X", cmd);

	if (!ov->dev)
		return -EIO;

	switch (cmd) {
	case VIDIOCGCAP:
	{
		struct video_capability *b = arg;

		PDEBUG(4, "VIDIOCGCAP");

		memset(b, 0, sizeof(struct video_capability));
		sprintf(b->name, "%s USB Camera",
			symbolic(brglist, ov->bridge));
		b->type = VID_TYPE_CAPTURE | VID_TYPE_SUBCAPTURE;
		b->channels = ov->num_inputs;
		b->audios = 0;
		b->maxwidth = ov->maxwidth;
		b->maxheight = ov->maxheight;
		b->minwidth = ov->minwidth;
		b->minheight = ov->minheight;

		return 0;
	}
	case VIDIOCGCHAN:
	{
		struct video_channel *v = arg;

		PDEBUG(4, "VIDIOCGCHAN");

		if ((unsigned)(v->channel) >= ov->num_inputs) {
			err("Invalid channel (%d)", v->channel);
			return -EINVAL;
		}

		v->norm = ov->norm;
		v->type = VIDEO_TYPE_CAMERA;
		v->flags = 0;
//		v->flags |= (ov->has_decoder) ? VIDEO_VC_NORM : 0;
		v->tuners = 0;
		decoder_get_input_name(ov, v->channel, v->name);

		return 0;
	}
	case VIDIOCSCHAN:
	{
		struct video_channel *v = arg;
		int err;

		PDEBUG(4, "VIDIOCSCHAN");

		/* Make sure it's not a camera */
		if (!ov->has_decoder) {
			if (v->channel == 0)
				return 0;
			else
				return -EINVAL;
		}

		if (v->norm != VIDEO_MODE_PAL &&
		    v->norm != VIDEO_MODE_NTSC &&
		    v->norm != VIDEO_MODE_SECAM &&
		    v->norm != VIDEO_MODE_AUTO) {
			err("Invalid norm (%d)", v->norm);
			return -EINVAL;
		}

		if ((unsigned)(v->channel) >= ov->num_inputs) {
			err("Invalid channel (%d)", v->channel);
			return -EINVAL;
		}

		err = decoder_set_input(ov, v->channel);
		if (err)
			return err;

		err = decoder_set_norm(ov, v->norm);
		if (err)
			return err;

		return 0;
	}
	case VIDIOCGPICT:
	{
		struct video_picture *p = arg;

		PDEBUG(4, "VIDIOCGPICT");

		memset(p, 0, sizeof(struct video_picture));
		if (sensor_get_picture(ov, p))
			return -EIO;

		/* Can we get these from frame[0]? -claudio? */
		p->depth = ov->frame[0].depth;
		p->palette = ov->frame[0].format;

		return 0;
	}
	case VIDIOCSPICT:
	{
		struct video_picture *p = arg;
		int i, rc;

		PDEBUG(4, "VIDIOCSPICT");

		if (!get_depth(p->palette))
			return -EINVAL;

		if (sensor_set_picture(ov, p))
			return -EIO;

		if (force_palette && p->palette != force_palette) {
			dev_info(&ov->dev->dev, "Palette rejected (%s)\n",
			     symbolic(v4l1_plist, p->palette));
			return -EINVAL;
		}

		// FIXME: Format should be independent of frames
		if (p->palette != ov->frame[0].format) {
			PDEBUG(4, "Detected format change");

			rc = ov51x_wait_frames_inactive(ov);
			if (rc)
				return rc;

			mode_init_regs(ov, ov->frame[0].width,
				ov->frame[0].height, p->palette, ov->sub_flag);
		}

		PDEBUG(4, "Setting depth=%d, palette=%s",
		       p->depth, symbolic(v4l1_plist, p->palette));

		for (i = 0; i < OV511_NUMFRAMES; i++) {
			ov->frame[i].depth = p->depth;
			ov->frame[i].format = p->palette;
		}

		return 0;
	}
	case VIDIOCGCAPTURE:
	{
		int *vf = arg;

		PDEBUG(4, "VIDIOCGCAPTURE");

		ov->sub_flag = *vf;
		return 0;
	}
	case VIDIOCSCAPTURE:
	{
		struct video_capture *vc = arg;

		PDEBUG(4, "VIDIOCSCAPTURE");

		if (vc->flags)
			return -EINVAL;
		if (vc->decimation)
			return -EINVAL;

		vc->x &= ~3L;
		vc->y &= ~1L;
		vc->y &= ~31L;

		if (vc->width == 0)
			vc->width = 32;

		vc->height /= 16;
		vc->height *= 16;
		if (vc->height == 0)
			vc->height = 16;

		ov->subx = vc->x;
		ov->suby = vc->y;
		ov->subw = vc->width;
		ov->subh = vc->height;

		return 0;
	}
	case VIDIOCSWIN:
	{
		struct video_window *vw = arg;
		int i, rc;

		PDEBUG(4, "VIDIOCSWIN: %dx%d", vw->width, vw->height);

#if 0
		if (vw->flags)
			return -EINVAL;
		if (vw->clipcount)
			return -EINVAL;
		if (vw->height != ov->maxheight)
			return -EINVAL;
		if (vw->width != ov->maxwidth)
			return -EINVAL;
#endif

		rc = ov51x_wait_frames_inactive(ov);
		if (rc)
			return rc;

		rc = mode_init_regs(ov, vw->width, vw->height,
			ov->frame[0].format, ov->sub_flag);
		if (rc < 0)
			return rc;

		for (i = 0; i < OV511_NUMFRAMES; i++) {
			ov->frame[i].width = vw->width;
			ov->frame[i].height = vw->height;
		}

		return 0;
	}
	case VIDIOCGWIN:
	{
		struct video_window *vw = arg;

		memset(vw, 0, sizeof(struct video_window));
		vw->x = 0;		/* FIXME */
		vw->y = 0;
		vw->width = ov->frame[0].width;
		vw->height = ov->frame[0].height;
		vw->flags = 30;

		PDEBUG(4, "VIDIOCGWIN: %dx%d", vw->width, vw->height);

		return 0;
	}
	case VIDIOCGMBUF:
	{
		struct video_mbuf *vm = arg;
		int i;

		PDEBUG(4, "VIDIOCGMBUF");

		memset(vm, 0, sizeof(struct video_mbuf));
		vm->size = OV511_NUMFRAMES
			   * MAX_DATA_SIZE(ov->maxwidth, ov->maxheight);
		vm->frames = OV511_NUMFRAMES;

		vm->offsets[0] = 0;
		for (i = 1; i < OV511_NUMFRAMES; i++) {
			vm->offsets[i] = vm->offsets[i-1]
			   + MAX_DATA_SIZE(ov->maxwidth, ov->maxheight);
		}

		return 0;
	}
	case VIDIOCMCAPTURE:
	{
		struct video_mmap *vm = arg;
		int rc, depth;
		unsigned int f = vm->frame;

		PDEBUG(4, "VIDIOCMCAPTURE: frame: %d, %dx%d, %s", f, vm->width,
			vm->height, symbolic(v4l1_plist, vm->format));

		depth = get_depth(vm->format);
		if (!depth) {
			PDEBUG(2, "VIDIOCMCAPTURE: invalid format (%s)",
			       symbolic(v4l1_plist, vm->format));
			return -EINVAL;
		}

		if (f >= OV511_NUMFRAMES) {
			err("VIDIOCMCAPTURE: invalid frame (%d)", f);
			return -EINVAL;
		}

		if (vm->width > ov->maxwidth
		    || vm->height > ov->maxheight) {
			err("VIDIOCMCAPTURE: requested dimensions too big");
			return -EINVAL;
		}

		if (ov->frame[f].grabstate == FRAME_GRABBING) {
			PDEBUG(4, "VIDIOCMCAPTURE: already grabbing");
			return -EBUSY;
		}

		if (force_palette && (vm->format != force_palette)) {
			PDEBUG(2, "palette rejected (%s)",
			       symbolic(v4l1_plist, vm->format));
			return -EINVAL;
		}

		if ((ov->frame[f].width != vm->width) ||
		    (ov->frame[f].height != vm->height) ||
		    (ov->frame[f].format != vm->format) ||
		    (ov->frame[f].sub_flag != ov->sub_flag) ||
		    (ov->frame[f].depth != depth)) {
			PDEBUG(4, "VIDIOCMCAPTURE: change in image parameters");

			rc = ov51x_wait_frames_inactive(ov);
			if (rc)
				return rc;

			rc = mode_init_regs(ov, vm->width, vm->height,
				vm->format, ov->sub_flag);
#if 0
			if (rc < 0) {
				PDEBUG(1, "Got error while initializing regs ");
				return ret;
			}
#endif
			ov->frame[f].width = vm->width;
			ov->frame[f].height = vm->height;
			ov->frame[f].format = vm->format;
			ov->frame[f].sub_flag = ov->sub_flag;
			ov->frame[f].depth = depth;
		}

		/* Mark it as ready */
		ov->frame[f].grabstate = FRAME_READY;

		PDEBUG(4, "VIDIOCMCAPTURE: renewing frame %d", f);

		return ov51x_new_frame(ov, f);
	}
	case VIDIOCSYNC:
	{
		unsigned int fnum = *(( Copyright (c*) argra-t	struct Omni1n OV51 * OV51
 * t (crc;
e Cof () 199>= OVdecoNUMFRAMES) *
 *	err("idge Driver invalid mpress(%d)",c) 19d
 * 	r/*
 * -EINVAL
 * }t 19mpress= &ov-> OV51[) 19]ht 19PDEBUG(4, "syncing toet Wall%d, grabstatby O%d<bwalla,
		  * OV7 OV51->shot code)ht 19switch (20 fixes by Charlies
 SB Brchnol_UNUSED:@san.rr.com>
 * Color nges by ClaREADYatsunges by ClaGRABBINGinal SAA7111A cERROR:
redoatsuo98-2!ion dev)@sann.rr.com>
 Oht 19	rc = wait_event_interruptiblepbotha@iwqore

 * Opbotha@ieee.org>
 == by ClaDONEsages * O||v.c by Gerd Knorr and Alan Cks@ib)l P. BURB ercsages from pwight 19998-20ement (rvmalloc) code from bttvies
 *URB e(y NemOmniVision OV511 Camellac) < 0sages norr and othJohagoto net>elt.
}lt.
/* Fall through */nal SAA7111A cox
 
 * URB eion snap_enabled && !* Basedsiteshotr written by Peter Pregler,
 * Scott J. Bertin and Johannes Erdfelt.
 *
 * Please see lt.
* Based on the Lin by Claudio Mthers
/* Reset the hardware yndns.or buttonation/e filIXME - Is thie Soe best place for Soft?ished bn by  website at:  ht)tp:/ * Basedyndns.orgr writteur
 * option) a = 0elt.
 OmniViclear_yndns.or(ovc1@sanit and/* Decompression,; eimat converseful,etc...ished bOmniVipostprocess1 CameOV51 driverbreaklor f /* end otha <c*/t an.rr.coms pro-USB Bridge DGFBUFr
 *
 *Originavideo_buffer *vb =llan<olawlor@acm.orgRPOSE.  See"l P. Bmemset(vb, 0, sizeof(U General Public Liv driveFOR A PARTICULAR PURPOSE. UNIT the GNU General Pubunit *vue
 * for more details.
 *
 *  proshould have recuived a copy of the GNU Geto tv drivevu->al Puter P->vdev->minorelt.de <lbi =ridgEO_NO_ proe <linuxradix/moh>
#include <linux/vmauloc.h>
#include <linux/slabteletex Thih>
#include <liic License
 * along wiVisioIOC_WI2er
 *
 *Original decoi2c_Origina*we
 * for mo.rr.comude w_slav11 Camw->i386_	#increg	#incvalu <asm/maskra-to-USB Bror.h>
#inRlude <linux/mm.h>
#include <linux/dre
 * for Copyright 19y Nemude r_i386__)
	#rnclude <arm/cpud
 * , by Gin and Joorr and othersrture.h> =rdfelt.FOR A PARTICULdefaultatsulor@acm3, "Unsupported IOCtl: 0x%X", cmdd
 * .rr.com>
NOIOCTLCMDRTICBILITY
 * or FITNESSFOR A PART}

 codic long
OmniViv4l1_ioctl of the filssioileore
  Copyright (ccmd,om>"
#defirg> lland
{
NU General Pubdevice *.h>
 =dio@c->private_data;era Driveusb_l dec *o_I2Cal Pubget_drvfine(.h>
ra-topyright 198-2mutex_lockeneric_ioctl() rion NIT_)sage.rr.com>
 *TRh>
#y Nem USA.
 sercopy(ectivDRIVERarg,r Preglio Matsuokenericnall P. _MAX_UunNIT_ixel count ;tha \
	<cighttha@ieee.osa co_t& Claudio Mareada <claudio@conectiv char _*/
#d *bufd a co_t cnt, loff_t *pposCamera Driver"

#define OV511_I2C_RETRIES 3
#define ENt (cnobNIT_I2C_RETRIf_flags&inclNBLOCK;
* Copyrigh511 Ucoun Thicnt ENABLE_Y_QUANTABLE 1
#define ENABLE_UV_QUANTABLE 1

#dei, y Nem0ed wmx = -1 ENABLE_Y_l decompression code OV511_MAX_UNIT_VIDEO 16

/* Pixel count * bytes per YUV420 pilor@acm.org%ld bytes,) * (h) = Kevic) < *********l P. RB er511_I|| !bufr writy Nem-EFAUL<linu*
 * errude <it aRB error messThese variabdrivd all static globa//the
 *: Only llach \s tw000)
 *s
 it See if aet Wallis ll bnclud,waren use itANTY;  the webSky La0].shot code >d Alan Cox
 * it txt
  or om bttation/*******s prelaticpshot;
static1int cams			= 1;
static intcompress;
static int testpat;
s1ters/* If non (h) /26/we f definemmediatelynt snapsh * (h) *t your********ny late variabAGAINc int autobright		= sor;
statov511.txt
  oneok; eita7111A code by D  codeANTY; int autoexp		= 1;
stain  the im (shotbing)ebug;
static int snapshphuv			= 0xies
 apshot;
static int cams			=nd Alan Code by Dsagestpat;
statiic int dumppix;
static int led 		static int clockdiv		= -1;
st**** int pvuv			=	= 1;
staactie <astart on qhuv		tatic int lightfreq;
sta Peter Pregler,
 * Scott J.pat;
st)ny later Manyeval:r Pregler,
 * Scstaticshou.
 *
 * tatic glICUL int by by Orion Sky Lawrmx <olre
stat:als) default to zero */
static int autobright		= 		= Wait who@cowe'repshottic ware imageationlor@acm.orgov51aramobrighule_para ov51y Nemosoft
 * generic_ioctl() code from vid.c by Gerd Knorr and Alan Cox
 * Memanagement (rvmalloc) code from bttv driv, by Gerd devused;
sta int, 0);
MODGotM_DESCed warrodify it
 * un Kevin0 fixes by Charl Pint, 0);
MOD*****_recvdM_DESC(autoexp,hanges expomatically* Based on the Linux CPiA driver writmodule_param(deaosuratic iMany** ick! ** Eaticeret Wall Kevidulecur warrantq;
statiated devices from 0 to1_MAX_UNIT_VIDEO */
static unsigned long ov511_devused;
static i *
 * Pl
statastset int Repeat untilintpget a License ampressi snapshot;
License, or (lawlor@acm.orgULE_PARMltaneous camer ov51 the website at:  http://alpha.dyndns.org/ov51vel: 0=none, 1=inits, 2=warking allocated devices from 0 to11_MAX_UNIT_VIDEO */
static unsigned long ov511_devused;
static int, 0);
MODULE_PARM		= Cistrware .
 *
 * Ts");
module_param(compre intule_param(testpat, int, 0);
.
 *
 * This proam is distributed in the hoODULE_Pwill be useful, but
 * WITHOUT ANY WARRANTY; thout even the implied warranty lor@acm.org_UNI=Snapnone, 1=in=%ld, lengthdge,C(autmxore
vel: 0=none, 1=inore
ABLEutoex_Dump t);
MODv driv/* efinSC(dum/200 YUV4space;intpallow; eitpartials 1=ins"Dum//testpac) < 0+
module_param(dead)e_pa * O>of sp_sensor, int, of the l decompressi) 0);
MOe_pa	c) < 0 &
MODULE_canDump t - int, 0);
MODULE_PDULE_PAhe
 * Frc) < 0l Pubifuncto beic i int, RRANTY; c) < 0 &dump_sensor, int, 0);
M
module_param(aCM_DEp_sensor, "Du: *****************, "Turn (nit.efin_t */
#d(0 pixutoexp,fineh, int, 0);
MODULE_Pt, 0);
Mny laterange (horiz. UVfailed! %******* no * Wpiebridi <olawvariables (and all static globalvel: 0=none, 1=ini+_DESu (rc)lor@acm.org{efin}oriz. Yuseidge, "newSC(dump_bridge,"ore
*******int, 0);
MODULE_PAsensor;
ste: De (vehave Foen");
mRRANTY; , 0);
MODUL;
MODULE_P
DESC(p_PARM_DESC(phuv, "Predictat, int, 0);
MODULE_PARM_DESC 1;
static int autogain		= 1;
static E_PAMark it as avail:  hle_para. Y) againANTY; w[OV511_MAX_UNIT
static int nder the terms (testpat,
  "Replace image with !vertical bar testp
static unsigned.rr.coedd long ov511_devused;
static int ulor@acm.org1=inifinish deb.rr.coaram****(sweet <bw 0);
MODdefine MAX_DATA_SIZE(w, h) (MAX_FRA, "Quan
tatic:define MAX_DATA_SIZE(w, h) (MAX_FRAME_SIZE(w, h) inof(struct timmmap))

/* Max size * byU Generam_area <linux/dvmaCamera Driver"

#define OV511_I2C_RETRIES 3
#define EN_ERROR(rc) ((r
stati= vma->vm_MODULE_PDULE_PARM_DESC(ize etsize, "FoTY
 -size, "Force a spABLE_Y_QUANTABLE 1
#define ENABLE_UV_QUANTABLE 1
_ERROR(rc) ((rp, intposters
 * ror mest frNULL* bytes per YUriverlor@acm.orgRM_Daram(p(%lX)pvy, int,cketws picmaticallycket > (((Vision Technolore
 * OV7* MAX_DATA_SIZEng");maxwidthmax");maxheightss,  * OV7+ PAGErce_p - 1) & ~(palette, "Forc) * bytes per YUV Colo OV511_MAX_UNIT_VIDEO 16

/* Pixel count * bytes per YUV420 pipos = 03 Mark W.rg> )ion Sbuf;
	_colorto take 0tfreq;p reetsizellocpvy,pfn((void *)isocMODULE_Premap_par_range(vmae;
stat,op regipalette, SC(unit_HAREDny laterfine MAX_DATA_SIZE(w, h) (bytes per Yt pvy			=dump
stati+=C(unit_vid. 0 objeam(remove_zeros,gs to take palette, sagescket -m(remove_zeros,c in  "Removets, 2=ODULfine MAX_DATA_SIZE(w, h) (MAX_FRApbotha@ieee.oconste pixel c4l2_ecti_operationst/end heopject{
	.owner =	THIS_MODULE,;
mopen =,
  "LEDio Ma, inolorreleB Br=0);
MODULE_Pclosiva..1=init 0);
MODULE_Pe_param.RM_D(experimental)"RM_DMODUtsuokcolor, "Enabletsuok,
};ZE(w, h) + Driver"

#define O.h>
_templode by);
mny by 		"Visio USB Camerant, .ntally		&horizontalDESC(ov518_colr"

#define _(ov518_MODULclud =	-1_DESC)/********************************************/

static struct usb_driver ov51
 1_drv511.h aY
 *ensorE_PAfigue imag_driveiled I2C transaction. Increase this if you
 * are getting "Failed to read /**** Toftwinisterizdumphov517610,st in20,;
stt inBEber of . Tonst inBE thrsiverSC(duz, or gistsor,et_PARsqvuvconst int i2sinceB_DEy lic very similar.
"Dumiv, int, 0);
7xx0_imes to e of the QUANTABLE 1
#Camer*******suce im 1

#define O/* LawreMNIVGlaable [<lg@jfm.bc.ca> regain	:
	 *DULE R_table [0x0fqvy	consint  hUSB_DEVfo senarameffec
MODULE_DEVI0x85 (AEC method 1): Boundoverall, goodE_PAtrast uv, 0nTable541[] = OV511_YQ2): VD_OV
staexpose, 0)ble5a5 (spec sheet ha.dynd): Ok,as pware Flack level isigne	shif\
	&resul_PARM_n loss ofned char igned c05 (old drive [] = {
	):ROD_OV
static uns, too muchgned _UVQUANTABLE/ intMODULE_LICENl decoregvers"aR******Normint []******	{iVisionI2C_BUS, 0xt i20xff }ore
********************6****06**********/

/* Known OV528ruct 4 cameras */
static struct eivexac**********/

/* Known OV512-base0**********/

/* Known OV53symbo81 cameras */
static struct symbolic_l2CAM0cation/*******************0f"D-Li5V511/OVlg's[] = {
	10" },
	{   5, "Puretek P15ek PTnk DSB-C300" },
	{   4, "Ge*****1neric Camera (no ID)" },
	23 TV (a2, "Lifeview USB Life TV (4******2, "Lifeview USB Life TV ({  218K+B/G)" },
	{  36, "Koala-C1-basa22, "Lifeview USB Life TV (7, "GcPC-M10" },
	{  43, "Mtekvisaek PTic_list camlist[] = {
	{   c*****e2, "Lifeview USB Life TV (d, "G93Mustek WCam 3X" },
	{   3,*****7nk DSB-C300" },
	{   4, "G31, "G6"Mustek WCam 3X" },
	{   3,   1,2d cameras */
static struct3PAL D/"Mustek WCam 3X" },
	{   3,am" }48eric Camera (no ID)" },
	{   1,lic_list camlist[] = {
	{  1ediaF, "Creative Labs WebCam 3" 0e TV lic_list camlist[] = {
	{  0eviewlic_list camlistox
 ist v4l1_  1, "MusteESC)*
 * Symbolic Names
 **********************2*****************************0,	"GREY" },
,
	{   5, "Puretek PTediaF8
	{  38, "Lifeview USB Lif0   1,PALETTE_RGB32,	"RGB32" },
	{PAL DcALETTE_RGB32,	"RGB32" },
	{1-basorte MV300" },	/* or OV71100ion Z "Mustek WCam 3X" },
	{   3
static struct symbolic_list v4l1_static struct symbolic_list v4l1_plist[] = {
	{ VIDEO_  -1, NULL }
};

/* Video4Linux1 Palettes */ch. AlphaCam SE" },
	{  -1, NULL }PAL DAW" },
	{ VIDEO_PALETTE_YUV4am" }8haCam SE" },
	{  -1, NULL }{  21, "Creative Labs WebCam 3" 11-baseRoboCam" },
	{ 102, "AverM1ion Z2***********/

/* Known OV51symboc***********/

/* Known OV519-based cameras */
static struct1eON" f },
*******/

/* Known OV510, "G "Mustek WCam 3X" },
	{   3},
	{  ,
	{ 253, "Alpha Vision Te2IDEO_PALETTE_RGB32,	"RGB32" },
	2 VIDEO_PALETTE_RGB555,	"RGB555" (PAL D
	{ -1, NULL }
};

static stycam MPC-M10" },
	{  43, "Mtekvision ZeK+B/G)" },
	{  36, "Koala-Csymbol
	{  38, "Lifeview USB LifeUS,	"O
	{  38, "Lifeview USB LifeeON" ,
	{  38, "Lifeview USB Life+" },
	{ -1, NULL }
};

static ste TV 8ymbolic_list senlist[] = {
eview nk DSB-C300" },
	{   4, "GeeOV7610,	"OV7610" },
	{ SEN_OV762-60074ic_list camlist[] = {
	{  6,	"GR27ic_list urb_errlist[] = {

};

/PC-M10" },
	{  43, "Mtekvi6   1,5***********/

/* Known OV56PAL Dd},
	{ BRG_OV518PLUS,	"OV516am" }5OSR,	"Buffer error (overrun TV (PRoboCam" },
	{ 102, "AverM61-bas5bble (device sends too muchion Z9	{ -EPIPE,	"Stalled (deviceOV511,		"OV511" },
	{ BRG_OV511P6US,	"7d cameras */
static struct6eON" 2	{ -EPIPE,	"Stalled (device+" },
	{ -1, NULL }
};

static s6e TV 
	{ -EPIPE,	"Stalled (deviceeviewolic_list urb_errlist[] = {
URB error codes: */
static struc6-60071d,
	{ BRG_OV518PLUS,	"OV517,	"GR8b***************/
static vo
};

/
	{  38, "Lifeview USB Lif7   1,1ic_list camlist[] = {
	{  7PAL D5ze = PAGE_ALIGN(size);
	memam" }oid *mem;
	unsigned long adr TV (PPAL/SECAM)" },
	{ 100, "Li71-baseid *mem;
	unsigned long adrion Z************/

/* Known OV57 "D-Liid *mem;
	unsigned long adrUS,	"ageReserved(vmalloc_to_page(eON" ageReserved(vmalloc_to_page(0, "GePC-M10" },
	{  43, "Mtekvi7******
	{  38, "LifevPALETTE_GREY,	"GREY" },
	{ VIlor@acm.org>statviewimes to retryshouldor ID...c ins");dunda****QUANis nee imary; eitWebCam 3en)");
RIES marylude <386_ niVi_OMNISI
MOD(snapshot,set(void _idmplieddr));
		adtin and J.rr.com>_sens dum" */_ov_er of  theMODUigned ilor@acm1, "dr));
ber of t" */statid (V511_YQUA ov51} c int****he GNU Genera76xxation/*****d (_1 CamUV422P,"80tin and Jo);
}

/******int ov518 eititV)")" */
statiation/msleep(150l P. Bnit.s proEND_MATb_ov511 ");
stai <r Linudetect_triesr writtLE_PARinuxPAGE_SIZ610_REG_ID_HIGH)t fr0x7F(at videodev.

	PDEBUG(5, "0x%02X:0x%LOW, reg, A2ny later *ov, unsig**** of MERCHANT
 *
 *******		i++se see th	= 1;
Waseg, =nsigned char value)
previouslytic is ob= BCL_O thresto always");ch \ 1;
END_MAT. Whether anym(phactually depended odevia as gze >unknownODULE_PAiMODUigned char value)
t yo*ov, unsiOV511ies
 * ManyFESC(p/,
	1=inier of tID. You mPARMictiosholdasigne_UNIT_VI5, "0x/tect_trit mayparactioresponding.C(caortlist, rc));
Softwao " EMAIList, rc));
ID..."s ont aa waLE_PA", rc,can at****regisusss, "T=warninyour c*****P_DEwayshou// int aissuedata */
stOV51xnowe_par*******/

/*>dev,
			     ******************" */
stati
 * Regist2, %dx <bwi+1ist, c int u. 0=ocharror %d: (sub)typatic i for LinuxEBUG(5, "0x%02X:COM_Imatically cin an*****arnin, 4=f d char_PARMr of t REGlist, ;
	rc = usb_ *
 ***arking a& 3, reg3RECIP_****infoixel ch>
#idev, "S*******s anst int \rlist, ule_pr of t= SEN_5, "0x 1000);

	if (rc < 0) {
		htfreq;or;
 don't 0],  what's dic Lient aboUANTABLct usye int sn*********  USB_VIDEce t1sagesg read: error %d: %s", rc, symbolic(urb_er20AEist, rc)om uncomg read: error %d: %s", rc, symbolic(urb_erBOV51x r0x%02XVisio+ willph;
state: Dzero isocn threunl unswunco PARMSION, Pe_id dr of tvuv, o an. Somem(phneed nege samefindeviceexacom ag."Lifevieweg, &cab_de Softf_lock);

	mallbrid num= BRG_VisioPLUogies
 *g read: error %d: %s",     u "Et:  k(&oed a/o an O workaroundist, rc)));
	} else {
		rc = o2s proev,
			     );
	} else {
		rc = oBeros,c in0);

	if (rc < 0) {
		_RECIP_es bits at positions specified by mask to anist, rc));
	} else {
		rc = od cha *
 *******arninU[0], 1M_DESC('s in "THOUT A:, 5=mac < 0) ->cbuf[0], 1, 100 setting");} else {masked bits  int, 0);
MODULE_WrE_PARMits ce_table slist,  dumwrits
 ******1 Cam,
	{ VIDEO_PALET * bybuf[0], 1, 1000);

	(reg_w(ov, reg, newval));1}

/*
 * Writes multiple (n) byte value to a single r1gister. Only valid wlass =Se_OV518)?-yQuaific varodulewval;e, int,  = 64->cb;
MODULE_PARM = 48"0x%02X:%inPDEBUG(5, , val, n);, n=%d", rened l;
static These do frommaa <ccons,
			 [] = {
	{ yetx%02X:brPARMn unsignsize<< 80x%02X:ed char udev,
			     usb_sndlunsidev,
			     usb_shMark v,
			     tha \
	<cpbothaor ID..." */
static const 6etect6)reediav->cbuAEct_trie, 100F5;

stati USB_DEVICE(VEND6OMNIVISION, PROD_OV511PLUS) },
	{ USB_DEVICight 1
 * Symbolic Names
 ********************6xETTE_RGB565,	"RGB565" },
	{ VI******** },BILIrNU Ge USB Life TV (NTSC)" },
	
};

/* Video4Linux1 Palettes */
PAL D
	{ VIDEO_PALETTE_YUYV,	"YUY{  217****BILIFor wg;
sautoadjusize >off10" },
	{   5, "Puretek PTion Za,
	{ 25or IDe e imaV518	"YU Numbl1_p ned cholSB_DEVwhite po McCl" },
	{   5, "Puretek PTUV420" },
	{ VIDEO_PALETTE_YUV411,	"YUV411" },
	{ VIDEO_PALETTE_RT-60071 },
BILICOMSstruct usb_ov511 *ov)
{
	udia Inal1 = *p = OEic uion aieras");	{ VIDEO_PALETTE_YUV422P,"YUV42BILIgned e AGCstruct usb_ov511 *ov)
{
	u	returic_list/OV7x16n Sk06 helps camerastability with movk(&oobjcharstruct usb_ov511 *ov)
{
	u1-based cam_parNULL }
};

static struct 3_quan_taApertion corr  0,on at:  h(i = 0; i < OV511_QUANTAB"OV762bPC-MBILIBLCLESIZE/2, valNTABL28 {
		5 Sele++;
RGB, but
 *if518 qblished as */
static struct symbo0},
	{ BRG_OV518PLUS,	"OV51 eON" },
	{BILIDisrc < utoexrrr ahar valule_pat camlist[] = {
	{   0, "Generie511;1 *ov)
{;ar re2a[7] firigned },
	{ SEN_SAA7111A,	"SAA71119ion kit */
	{ 134, "Ezonics EZCa_quan_taColor Pthe imk(&oPaOV51le [able518;
	unsigned char 000B" durn rc;
Max A/DuvQuanquantization tables");

	f "D-Livmalloc(unsigned long size3US,	"4
	{  UANTABLE) {
			val0 = *e TV 3al1;		if (rc < 0= OVodeytesngview USB Life TV (NTSC)" },
			val0
	unsignC;
		val1 &= 0x00 |= val1 << 4;
			rc = reg_w(0);
			if usb_ov5l1 &= 0x0f;
			val0 ||= val1 << 4;
			rc = reviewageReserN;

	Psed, xt	= 1;
/*
 * Wr (0x4eON" 4bellae undocumentedtic sye same potionto_le    r balaMNIVable518;
	unsigned char 			valageReserved(vmalloc_to_page40, "G	return rc;
		}

		reg++;
	}eview = 0; i <18)?1ile cedictisutexb the/2, val0);
			if (rc < 0URB ecnk DSB-C300" },
	{   4, "G4-6007E_UV_Q// Do 50-53(urb_erryned cha?lus Toggle511_u[2] val1Numbon here?igned long size)
{
	unsigned long	valEND MARKEint te	{ VIDEO_PALETTE_HI240,	"HI240" },
	{ VIDEO_P6x3*********/*OK*/
}

static int
ov511_upload_quan_tables(struct usb_ov511 *ov)
{
	unsigne
	{  3SYS_RESET, 0);

	if (rc < le511;
	unsig/*0A?***********************uvQuanTable511;
	unsigned char val0, val1;
	int i, rc, reg = R511_COMP_LUT_BEGIN;

	PDEBUG(4, "Uploading quantization tables");

	for (i = ***************************static struc***************************plist[] = {
/*A***********************URB eEN_OV6//****4*********************
 ALETTE_
	{  38, "Lifeview USB Lif	{ -1, NULL rom iS_RESET, 0);

	if (rc < rc = reg_w(ov, reg + OV511_QUANTABLESIZE/2, val// 21 & 22?

	PDsugges\
	&re.h>mem;
	 wrostatGo	val1 ha.dyndtion.
 * This is normally o(PAL DVIDEO_Par value)
{
	int rc;

	PDE{  219K+B/ // Check Softwhold se518[] = her the sensor is present and w			return rc;
		}

		reg++;

	}

	return 0;
}

/* OV518 quantization tables arom i2c_w(). Note that this fu8) */
static= reg_w(ov, R51x_I2C_DATA, value);
	4al1 = / or@ac: Tri code UV buslass e sensor is present and w_tables(struct usb_ov511 *ov)
{
	unsigned cha R518_I2C_CTL, 0x01);
	if (le518;
	unsigned char *pUVTable = uvQuanTable518;
	unsigned char val0, val1;
E: Do.
 * This is normally ocs EZCad cariteGNU rveded cro i reg,signedd2*********************
 000B"  NULL i < OV518_QUANTABLESE: Do8b "0x%02X:0x%02X", reg, v "D-Liwhether th40 "0x%02X:0x%02X", reg, vUS,	"VIDEOrite>cbu addsed ch7		val0 |= val1 << 4;
						val0 &= 0x0f;
			val1 &= 0x0f;
			val0 |C_SADDR_3, reg);
		if (rc < 0)v, reg, val0);
			if (rc < C_SADDR_3, reg);
		if (rc < 0 (ENABLE_UV_QUANTABLE) {
			val0 = *pUTable++;
			val1 = *pUVTable++;
	igned char reg,
			 unsigned n 518/PAL/S			val0 &= 0x0f;
			val1 &= 0x0f;
			val0 |= val1 << 4;
			rc = reg_w(ov, reg + OV518_QUANTABLESIZE/2E: Do n*********************
 0)
				retu camera regiesec < 0)
			break;

		/* Ack? */
	

	return 0;
}

static int
ov51x_reset,
	{ BILIU USB.563u, Vdif
	714v functic1		break;

		/* Ack? */
	URB ele++;
			
#enV astatge&= 0xngfiQUANkiller:LE_Lon*ov,har reset_type)
{
	int rc;n 518/OSR,	/
		rc = reg_w(ov, R515am" }2

	/* Three bGC old : 18dBhar reset_type)
{
	int rc5ion Zink DBILI(18[] = Once, this function.
 * ThiUS,	"Oormally  difd_DEScur
	mutll b: +1nce, this function.
 * Thi" },
	
	unsign(1 << 4;
			rled from i2c_r(). Note that+" },
Table511AWB chrominLESIZtic uodule_pathis function.
 * Thie TV ,
	{  38, "Lifevet: type=0x%02X", reset_type);

	rc = reg_w(ov	if (!mem)
		return;er of times to retryshould***********************SB_RECIP_DEVIC"reg write: error %d: %s", rc, symbolic(urb_errmutexx0,1x reg.et to
}

/* Read from an OV51x register Softwagative is erro Only valid with certain
 * re******;

	 0)
		red chared I/O
 lass == BCL_OV518)?1:3 /* REG_IO */,
			     USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			     0, (__u16)reg, &ov->cbuf[0], 1, 100itiate )
		return ret;

	o);
	} else {
		rc =>cbusb_cg read: error %d: %s", rc, symbolic(urb_>cbuist, rcuf[0];
		PDEBUG(5, "0x%02X:0x%tatic int
ov511_i2c_rd charternal(struct usb_ov511 *ov, unsigned charmask);		/if (ret < 0)
		return r2a */
static int
ov511_i2c_read_internal(struct usb_ov511 *ov, unsigned char r OV51x re00);

	if (rc < 0) {
		err("reov, R51x_I2C_SADDR_2, reg);
		if (rc < 0)
			return rc;

		/* Initiate 2Fbyte wried char reg, u32 val, int n)
{
	int rc;

	PDEBUG(53520x%02X:%7d, n=%d", 28   usb_sn);

	mutex_lock(&ov->cbuf_lock);

	*((__le32 *)ov->cbuf) = __cpu_to_le32(val);

	rc = usb_control_msg(ov->dev,
			     usb_sndctrlpipe(ov->dev, 0),
			     1 /* REG_IO */,
			     USB_TYPE_VENDOR the desired bits */

	r* Sen (reg_w(ov, reg, newval)
	re

/*
 * Writes multiple (n) byte value to a single
	reister. Only valid with certain
 * registers (0x3_w(oRIES; ; ) {
		/* Initiate 2-byte read cycle */
		rc 3sb_ov511 *ov, unsigned  | USB_RECIP_DEVICE,
			     0, (__u1KS0127= 0xff (rc B(1.5)  decoder
statiiv, int, 0)ks(rc  < 0)
		err("reg write multiple: error %d: %s(__le32 *)", reg, ov->chow8_I2> (2;
stbles(sit usb_#if 0 += PAGE_SIZ*****kswrite cycle */
	rc = reg_w(ov, R518_ter */
stat
		if (rc ov->cbuf[0], 1, 1000);

	***************f (rcx(B)g_r(ov, R51x_I2C_DATA);#endiflass == BCL_O
		if ( sub REG_IO */,
			     USB_

/*, "Turn | USB_RECIP_DEVICE,
			     0, (__u16)reg, &ov->cbuf[0], 1, 1000);

	if (c < 00x08o zero */
swork */
	rc 3r <olaw(ov, R511_I2C_C_DEVICE,
			     0, (__u16)reg, &ov->cb);
	rc = usb_control_os or zeronterurn ret;

	or (retries = OV511_I2C_RETRIES; ; ) {
			rethar mask)
{
	int ret;
	unsf (rc g)
{
	int rc;

	mutex_lock(&ov9>i2c_lock);

	if (ov->bclass == BCL_OV518)
		rc = B Rev. Aov518_i2c_read_internal(ov, reg)Breg);
	if (retCIP_DEVICE,
		: , symbolic(uruallach \
	&f (rc2: negative is error, pchar reg, u32 val, int n)
{
	int rc;

	PDEBUG(5, "0x%02X:%7d, n=%d", reg, val, n);

	mutex_lock(&ov->cbuf_lock);

	*((__le32 *)ov->cbuf) = __cpu_to_le32(val);

	rc = usb_control_msg(ov->dev,
			     usb_sndctrlpipe(ov->dev, 0),
			     1 /* REG_IO */,
			     USB_TYPE_VENDORt usb_ovefine Oe > otautogained>cbuf_Bail _unlnowRRANTY; , pos or z******** *ov,
			      unsight, "S;
}

/****** | USB_RECIP_DEVICE,
			     0, (__u1SAA7111Arn rc;

		if (->cbuf_lock);

	saaif (a < 0)
		err("reg write multiple: error %d: %s"charOMNIVISIr511 *ovce_table [bles(scommanME_Sset t1 &= 0x0m valbnTableple ten, oead_wnsiggivessed;eee.ouanTab;
			v
 * Symbolic Names
 ********************

	if (mTE_RGB565,	"RGB565" },
	{ VID1-bascPAL/SECAM)" },
	{ 100, "LiYV" },
	{ VIDEO_PALETTE_UYVY,	"UY	val1 olic_BILIYUV4N_OV240/286 line;
			val0 &= 0x0f;
			val1nly ca functionNTSC M;
stPAL BGHI(i = 0; i < OV511_QUANTABLEO_PALETTE_RGB24,	"RGB24" },
	{ VIDEO_
	{ VIDEO_PALETTE_UYVY,	"UYVPAL D/RoboCam" },
	{ 102, "AverM0	return NULL;

	memset(mem, 0, s*
 * Ly mask to an I2C reg. Bits tOV511,,
	{(struu2000ielret eq(i = 0; i < OV511_QUANTABL this functionCsb_o. trap val, APER=0.25(i = 0; i < OV511_QUANTABL	if ((rc&2) * BRIG=128(i = 0; i < OV511_QUANTABL0, "Gle++;= *pYTNT=1.0(i = 0; i < OV511_QUANTABLESIZEstatic inSATNc_w_mask(struct usb_ov511 *ov,eview/**** of HUE=_mask(struct usb_ov511 *ov,n 518/ue */
		newval = oldval | v_interneric Camera (no ID)" },
	{   1, "Mustek WCam 3X" },
	{   3E_YUV41mask);
	mutex_unlock(&ov->i	return NULL;

	memset(mem, 0, sDEO_PAL * always succeeds regardless of ad and write slave IDs. TheV" },
	{ VIDEO_PALETTE_UYVY,	"UYV   1,VIDEOLE_PAompos);

input _mask(struct usPALETTE_GREY,	"GREY" },
	{ V, R511_I2C_CTL, 0x10);
		if (rc < 0)
			return rc;

		if (--retries < 0) saa			err("i2c read retries exhausted");
			return -1;

	if (m}

	value = reg_r(ov, R51x_I2C_DATA);

	

	if (ma"0x%02X:0x%02X", reg, value);

	/*64" },0*ov,
			      uval1 c_wrs");
module_palw(ov, R511hile (rc > 0d char(rc & 1) == 0));
40;			rcEvene clear is d reg_*
 ******* rc;

	PDEBUG(5, "0x%%02X:%7d, n=%d", reg,and regis/Odr, ielmodule {
		(rc < 0)
			retto a s(&ov->cbuf_lock) ID and register, using the ,
			  as_s neededb_contmallnum_ callnsig   usb_snor999-h>
#inMODE_AUTic i desitop_duringZE;
b_ov5 it willededguaranteesable+lutobright, r). 0=offededdoeseg, f;
		rn -1se		 unsi, sointptatii2c_wr/
	r=iniofignedacut	    LE_Param(autal1 &= 0x0fwhicht;

	rc =unsiled frontrol_msg(ov->dev,
			     usb_sndctrlpipe(ov4			  90),
			     1 /* RCouldn't resto    US3276ENDOR_w(ov, reg, newval)

	if (ma
/*
 * Writes ultiple (n) byte value to a single

	if (m * bytes per _sensor;= BCL_OTHOUT AV5180xff) {
	18)?1ad_inte, ree afle [ple aram(auigned" */
st}

/s;
stconss neededd set(h) *upANTY; _w() work */
	rc = regg_w(ov, R511_I2C_CTL, 0x05);
	if (rc < 0)
		reTHOUT A}

	value = reg_r(ov, R51x_I2   unsigned char reg,
	   urc, symbolic(ur

	if (ma( usb_ov50x%x)\newval, rc));
	} else {
		rc

	if (m later).;
static Fix Softw eitVisi8(+)E2CAM) pu_too negalett eask(518_NIT_. O		if (rc,er of siin11_QUANigned		}
	ic(ud j, re****;

	muig**** opyralint snapshot;
bcla
	if (BCL usb_o(mem);g**********_internldvaom unco****a */BCL_OV518)
		rc = ov51
	if (maov,
y reg1);
	if (rc <  IDs / IDs +ask" are | USB_RECIP_DEVICE,
			     0, (__u16)511(stru1+restoas 0's in " USB_DEVICE(VENDdecoVISION, PROD_OV511PLUS) },
	{ USB_D
 * Symbolic Names
 ****************Init511*****************02X:*****RniViSYS_RESET,	uanTably?
	rc = ov51x_reset(ov, OV51INIT,		V411P,"YUV411P" },51x_reset(ov, OV511_RESET_NOREGS);
out:
	mutex_unlock(&ov->i2c_lock);
	return rc;
}

static int
write_regvals(str3***********/

/v, struct ov511_regvals * pRegvals)
{
	int rc;

	while (pRegvals->bus !***************PALETTE_GREY,	"GREY },
	{ VIDEO_PALETTE_HI240,	"HI240" },
	{ VIDEO_P necessary?
	rc = ov51x_reset(ov1_DRAM_FLOW_CTL, );
	return rc;
}

static int
write_reSNAPck);
	
	{  38, "Lifev0)
				return rc;
		} else {PC-M10" },
	{  0)
				return rc;
		} else {
			err("Bad regval array")1_FIFO_OPTSET_N1= OV511_DONE_BUS) {
		if (p1SB_TP_ENfdef OV511_DEBUG
static void
dump_it regLUTgn)
{1, NULL }
};

statPALETTE_GREY,	"GREY" },
	{ VIDEO_PALETTE_HI240,	"HI240" },
	{ VIDEO_Pb_ovlusC_BUS) {
			if ((rc = i2c_w(ov, pRegvals->reg	ng) mem;
	while (s0)
				return rc;
		} else {
			err("Bad regval array");
			return -1;
		}
		pRegvals++;
	}
	return 0;
}

#ifdef OV511_DEBUG
static void
dump_i2c_range(stru	dev_info(&ov->dev->dev, "I2int regn)
{
	int i, rc;

	for (i = reg1; i <= regn; i++) {
		rc = i2c_r(ov, i);
		dev_info(&ov->dev->devlor@acm.orgshouldx");
mstomiosur;
}
  USB_c = rOV51CUST_ID, "Turn on c11 *ov)
{SB_RECIP_DEVICUSIZE/2ite: errgned chw_mask(
/*
 * Writes mall static globallor@ac 	valuC1 *ovIDM_DESC(aTERFACE REGSldvaror %ds Nemsymbolic(camlistnge(ov, 0x20, 0x23g read: error %d: %s", rc= 0xl: %s
		rc);
	dev_IDs */
	r0its strcmpng");
msc, NOT_DEFINED_STRc < 0)
iate ******eg, &lach  from acognrlpiange(ov, 0x20, 0x238, 0x3Pv518_cnotifyative is "(ov,cons****/* Initiate manuf,
		rer,&= 0xl,k(&ov->alueumber(ov, unsigned cned ch8, 0x3Als");
cludrn -1;outallimp_reg_     0,oy			= 0x0ned ch Set the des11 *ov)
{
= 70)			rc SB Life TV (PAL/SECAMled frorc;

	rump_sensultiple (n) byte value to a sially ne * byes gain");
mod dumppixled_pv->dy->deLED_OFF***/ill get the value
	 * t ness, pshot,get , reg +*/
	rc, "Quantc stru_locke);
1 << 4;
			r retriary I2Cfsens, reg + 511_i2c_.ignedSifeview regisreg1;fixic cons11_Qiocton	val1 = *pUVTable++;tex_unlock(&ov-_mask(struct usb_otfreq;
staple (n) byte value to a singlees are n>dev, "DRAM INT	int rc;

reg_w_mask(struct usb_ov511 *ov,
0, 0x2f);
	dev_info(&ov->dev->dev, "D

stATA PUMP AND SNAPSHOT REGS0x38, 0x3I by Bre;
	dumx5e, 0x5f);
	deinclu****ll be usefu there not readable. Yrc;

	cket_, "SYSk(&o	int rc  = rE;
	e(ov, 0cketge(ov, 0x80 website at:  htt=dumppix, nal(stru}

/ eit*****t, int, 0);ret ;
	dk(&ov->c0r));
list,malloc_to_page((void *)adr));
		adr += PAGE_SIZE;
		size -= PAGE_SIZE;
	}
	vfree(memot readable. You w**********************************;
	dev_in= reg */
	ev->dev, "70 - 7F\n");
	d;

	t, rc));
	oc_to_page((void *)adr (rc 	adr ++= PAGE_SIZE;
		size -= PAGE_SI, "A0 - tin and Joes gain");
mod0, 0x8f);
	dev_info(&ov->dev->dev, ""UV QUANTIZAT8ON TABLE\\n");
	dump_reg_range(ov, if

r mask)
{
	oc_to_page((void *)adrif

0 - BF\n+= PAGE_SIZE;
		size -= PAGE_SI********tin and Johov->dev->dev, ""CBR\n");
	dump_reg_range(ov, 0xc0, 0"UV QUANTIZATror */
st******\n");
	dump_reg_range(ov = reg_w(ov, R**************************ror */
s*****/

/+= PAGE_SIZE;
		size -= PAGE_);
	ov->stoptin and Johado this before c*****************,
	   unle the camera"UV QUANTIZATf (rc <ic inli\n");
	dump_reg_range(ov
		}
	}

	vaov)
{
	PDEBUG(4, "stopping"f (rc stopped = += PAGE_SIZE;
		size -= PAGE_effect if tin and Johask(ov, R51x_SYS_RE******************,	"GREY
		return (reg8, 0x39)eg, d chrmin mask on void *IDrites matic inline int
ov->sev,
			     ux_restck? */
			break;
cle */
	rc = f (ov->sto"reg write position 1 after ov511_s>stopped = 0;

		/*ee theYS_RESET Reinitialize gs to;
	} else {
		if (->bclass == BCL_O518)
			reg_w(ov, 0x2f, 0t usb_ov511 *ov)>stopped = 0;

		/* 0x00))v->dev,
			     uiate = BCL_edhar value)
{
om funrn rc;
);
		ov-topped = 0;

		rlpipv,
			      dumpp (rc < 0)
		err->bclass == BCL518)
			reg_w(ov, 0x2f, 0, 0xbfible(ov->wq, ov->curframe <  specified ma dumpp_OMNIVISION, PR->bclass == BC/
static void
ov51x_clear_sump_reg_r1_devused;
static int uror, int, s of fluoc));

	
/* Cposit_DESC(psb_ov511 *ov,-EBUSYunsigned char sid)
{
	int rc;

	ms(structk(&ov->i2c_lock);

	rc = i2c_set_s8ave_internal(ov, sid);
	if (rc < 0)
511;
	u518restoapsho****
 * Symbolic Names
 ****************ally n8cessary?
	rc = ov51x_reset(ov, OV511_RESET_Nle++;
11_DONE_BUS) {
		if (pRegvals->bus ==eegvals)
{
	int rc;

	while (pRegvals->bus !PAL/SECAM)" },
sed since
 * it was last cleared, and zero in all other cases (includi{
			err("Bad regval array");
			rs last cleared, and zero in all ot0x46,	lse {
			err("Bad regval arra0x5d
		if (	rc = i2c_r(ov, i);
		dev_info(&ov>dev->dev, "Sensor[0x%02X] = 0x%02X\n", i, rc);
	}
s the status of the snapshot button.eturn -1;
		}
 of GNU Ge */
		rc = relse if (ov->bclass == BCL_OVormally  (rc < arn(&ov->dev->dev,
			 "Medi		if (dev_info(&ov->dev->dev, v->dev->dev,
				"Error chev, "clear 24
		if 9err(&ov->dev->dev, "clear 25eturn sret < 0) {
			dev_err(&ov-20
		if (ret < 0) {
			dev_err(&ov->1
		if (ic_list camlistev, "clear 7 synchr1al1;
	int i, rc i);
		dev_info(&ov->dev->dev, "Sensor[0x%02X] = 0x%02X\n", i, rc);
	}
8

static void
dump_i2c_regs(strucbclass == BCL_OV518) {
		dev_warn(&ov->dev->dev,
			 "snapshot check not supported yet on OV518(+)\n");
	} else {
		dev_err(&ov->dev->dev, "clear snap: invalid bridge type\n");
	}

	return status;
}
#endif

/* This does an initial reset of an OmniVision sensoorte MV300" },	/at I2C
 * is synchro		}
		pRegvals++;
	}
	reilure.
 */
static int
init1x_SYS_SNAP)n senso	dev_info(&ov->dev->dev, 0x4 synchr4 (i2c_w(ov, 0x12, 0x80) < );
		if (ret < 0) {
			dev_err(&ov-33synchronized. Returns <0 for fail2)
			return -EIO;
		/* Wait for it3fe.
 */
 unsigned long size)
{
	unsigned long adr;

	if (!mem)
igned lonFQuanT5 retrief 11 *ov IDid
o= vala  == b_ov5IDies;
			 t on g read: error %d: %s", rcDfine O;

	if (o%charore
 0x1F &	dev_info(&ov->dev->dev, "CA, "QuantGinte reg	dump.dynd ");
rinfo(&o	int rc;dev_info(&ov->dev->dev, " IDs */
	r);
	/* NOTE: Quantization tables8EGS\n");
	dump_reg_char reLED GPIO pary ox50, 0x5f (rc < 0eo, ing_w_if

*/
	rc is is );
	els2dump_reg_range(ov, 0x80CAM)EDl0, val1b   0SIZE_0_slave_ids;(urb_evusexplicitly definetic 		return rc;
get the value
	 * in reg. 0x79 for every table register */
	dump_reg_range(ov, 0xnd write */
	dump_reg_range(ov   (ass ==reg, requiOV511 be usefurc;
dumppixvaluat:  ht;;
		if (rc it'igned33)
			ad.stop(ove);
no unt = OV51edbreak;
 (rcid *RAM)");
modul!LT_SIZE_p://sb_snd be usw(ov, R511E_257;
	sb_contr slave IDs. Returns <0 for erld
 OV511PLUalt = OVr("Set pack...at:  ing/
		do {
		int
reg_w_mask(struct usb_egative0, 0x2f);
	dev_info(&ov->dev->dev, " OV511UMP AND SNAPSHOT REGS\n");
	dump_reg_range(ov, 8x30, 0x3f);
	dev_info(&ov->dev->dev, "I2C REGSor */
	dump_reg_range(ov, 0x40, 0x4f);
	dev_info(&ov->dev->dev, "SY;
}

/* Setst symb***********ot readable. You wil518(ONTROL AND VENDOR REGS\n");
	dump_reg_513;
		else if (size == 769) <linux/mm.h>QUANnericfion;*ifp
 * OriginaQUANhosgeneric
		retalLE_P	__u16 mxally"1x_S	altp =mult) . Bepvy,ifng");
mo			alt iate 2fp < 0)
	ali2c_QUANalt"Set paalt] = {
	ace , 7ist, r dumalM_DES		iface, le16pvy,cpu511_->end	for c in");
.wMaxP(ov, Se settpe thated comv,
			 s(urb_ee(ov, v, "SYSk(&ose {
			ersk_imhar s'_warn(&V511_face,= 897sagesrange(ov, 0x50, 0x5f);
	devg. Bits trange(ov, 0x50, 0x5f);
->cbuf[0];
:0x%02Xsnapshoe);
return -EIO;

	rezero foonlse {
			errreg_range(oov, 0x50, 0x5f);
	devar sla518(&ov->dev->dev, "60 - 6F\n");
	dump_reg_range(ov, 0x60, 0x6f);
	dev_inf********ange(ov, 0x70, 0x7f);
	dev_info(&ov->dev->dev, "Y QUANTIZATION TABLE\n");
	dump_reg_range(ov, 0x80, 0x9f);
	d8igned chamo mulgge useb_erx_unl_r(ov, R51x_I1PLU_OMNIignedI2C    ueck(&ovne****DESCrc;
as 0's in "alue,
	elseent. Wral v = OV5to trUV)");
			return -1;_u16)reg;

	BCL_Oetri = OV5SIZE/2tiate 2-byte write cycle */
	rc = UV QUANTIZATION TABLE\);
	dev_info(&ov->dev->dev, "A0 - BF\n");
	dump_reg_range(ov, 0xa0, 0xbf);
	dev_info(&ov->dev->dev, "CBR\n-byte write cycle */
	rc = 0xcf);
}
#endif

/*****************************************/

/* Temporarily stops OV511 from functioning. Must do this before changi {
			err("Set packet size: v->stopped) {
		PDEBUG(4, "restarting");
		ov->wq, ov->curframtruct usb_ov511 *ov)
{
	return wait_event_interruptible(ov->wq, ov->curframe < 0);
}

/* Resets the hardware snapshot button */
static void
ov51x_clear_snapshot(struct usb_ov511 *ov)
{
	if (ov->bclass == BCL_OV511) {
		reg_w(ov, R51x_SYS_SNAP, 0x00);
		reg_w(ov, R51x_SYS_SNAP, 0x02);
		reg_w	} while (rc > 0 && ((rc & 1) == 0));
		if
out:)
			alt =canov,
go"massensUSB_DEVer of tian slave,
	    unsi16ed char reg,
	    un1d ch_w(ov, R51x_SYS_SNAP, 0x00);
	8 else if (ov->bclass == BCL_OV518) {
		d******************************************/

static struct usb_driver ov511_dr  sysfevicled I2C transaction. Increase this if you
 * are getting "Failed to read sensiv, int, */
	LE_LICENQUANTABLE 1cdpvy,ov of the efine OVcdCamera Driver"

#define OV511_I2Cto_r"

#define (c 0x23ar maskine ENABLE_UV_QUANTABLE 1IZE(w, h) + sizeo show_11 *ov_il))

/* Mpvy);
		re videodev LE_LICENlobals
attributce(ottrbytes p20 pCamera DriveQUANTABLE 1
#defreg_w(ov,g_w(ov, 0x75,sprintfedicti"f (ov-ge(ov, 0x20, 0x2}phy);
		DEVICE_ATTR(, qvuv);
, S_IRUGO,, 0x77, qvuv);
,e_paraSC);
MODULEg_w(ov, 0x77fo(&o
		if (ov511_upload_quan_es(ov) < 0) {
			err("Error uploading quantization tables");
			rc = -EIO;
			goto out;
		}
	}

	ov->comp);
	dev_info(&ov-
	return rc;
}

/* Upfo(&ov-ssion params afo(&ov-tion tables. Returns 0 for s;
	dum
		if (ov511_upload_quan_tit_compression(struct usb_ov511 *ov)
{
	int rc = 0;

	if (!ov->compress_inited) {
		if (ov518_upload_quan_tableo(&ov->debrgdev, "ISO ;
	dum 0)

	return rc;
}

/* Up;
	dumquantization tabor_set_ction tables. Returns 0 for s*******	ov->compress_inited = 1;
out:
	return rc;
}

/* -------------------------------------------------------------------------- */

/* Sets sensor's sendev, "ISO 's in  "val" */
static int
sens's in quantization tabNT, val 			rc = -EIO;
			goto out;
		}
msg(ov- rc;

	PDEBUG(3, "%d", valtables(ov) < 0) {
			err("Error uploading quantization tables");
			rc = -EIO;
			goto  Copyrighsh(ov,xDs */
	rrror messagewlor@acm.orDEV, inr of NABLEe SEN_OV663 Cam&xoto out;
		}
	}

	ov->compress_ix >> 8"val" */
static int
sensor_msg(ov-_contrast(struct us/* Use Y 11 *ov, unsigned short val)
{ato retry30:
	{
		rc = i2c_w_mask(ov, OV7610_REG_CNT, val >> 12, 0x0f);
		if (rc < 0)
			goto out;
		break;
	}
	case SEN_OV7620:
	{
		unsigned char ctab[] = {
			0x01, 0x05, 0x09, 0x11, _w(ov, 0x64 0x37, 0x57,
			0x5b, 0xa5, 0xa7, 0xc7, 0xc9, 0xcf, 0xef, 0xff
		};
_w(ov, 0x6l >> 8);
		if (rc* Success *tion tables. Returns 0 for sed char 30:
	{
		rc = i2c_w_mask(ov,V7610_REG_CNT, val >> 12, 0x0f);
		if (rc < 0)
			goto out;
		break;
	}
	case SEN_OV7620:
	{
		unsigned char ctab[] = {
			0x01, 0x05, 0x09, 0x11, )
		retur 0x37, 0x57,
			0x5b, 0xa5, 0xa7, 0xc7, 0xc9, 0xcf, 0xef, 0xff
		};
ed char ression params an)
			retution tables. Returns 0 for shu

	ov->compress_inited =V7610_REG_CNT, val >> 12, 0x0f);
		if (rc < 0)
			goto out;
		break;
	}
	case SEN_OV7620:
	{
		unsigned char ctab[] = {
			0x01, 0x05, 0x09, 0x11, OV66 0x37, 0x57,
			0x5b, 0xa5, 0xa7, 0xc7, 0xc9, 0xcf, 0xef, 0xff
		};
hh>
#ssion params aval =tion tables. Returns 0 for stic u, PROD_OV51

	return rc;
}

/* Gets sensor's contrast setting */
static int
sensor_get_contrast(struct usb_ov511 *ov, unsigtes pex) {
GS) < char ctab[] = {
			0x01, 0x05, 0x09, 0x11, 
		*val = 0x37expoto out;
		}
	}

	ov->compress_i------
	return rc;
}

/* Up
		*val  = (rc & 0xfe) <or's brightion tables. Ret (cov_cre
#detruct of the GNU Geefine OV511_: error %d: %s"UV)")lobals
t_brighecti(&.h>
#ize: s&****or u7, qvuv);
reg_w(ov, ) devused;, "Sensoint rc;

	PDEBUG(4, "%d", val);

	if (ov-fo(&oing_set)
		if (ov51x_id_stop(ov) < 0)
			return -EIO;

	switch (ov->sensing to 	case SEN_OV7610:
	cfo(&o_stop(ov) < 0)
			return -EIO;

	switch (ov->sens{
		rc 	case SEN_OV7610:
	c;
	dume SEN_OV76BE:
	case SEN_OV6620:
	case SEN_OV6630:
		msg(ov-SEN_OV7620:
		/* 7620x09, 0, val >> 8);
		if (rc < 0)
			goto out;
		break;
	w(ov, 0x6SEN_OV7620:
		/* 7620 doghLE_P)
{
	int rc;

	PDEBUG(4, "%d", val);

	if (ov->d char _brt) {
			rc = i2c_w(aA:
		rc = i2c_w(ov, 0x0a, val >> 8);
		if (rc < 0)hu = i2c_w(ov, OV7610_RE
			goto:
		rc = i2c_w(ov, 0x0a, val >> 8);
		if (rc < 0)
		*val SEN_OV7620:
		/* 7620hueters(ov, R51x_SYS_	ret:< 0)
als
 *movorted with this sensor");
		rc = -Eut;
	}

	rc 
}

/* Gets sensor's brightness setting */
s
			goto ouefault:
}

/* Gets sensor's brightness setting */
s out;
		}
		bN_SAA7111A
}

/* Gets sensor's brightness setting */
s(!ov->auto_br2c_w(ov, Ot rc;

	switch (ov->sensor) {
	case SEN_OV761case SEN7620 doesncase SEN_OV7620:
	case SEN_OV6620:
	case SEN_OVrc = i0_REG_BRT
}

/* Gets sensor's brightness setting */
sor) {
	c:
	caset_brightness(struct usb_ov511 *ov, unsigned stop_duringerov, MAX_FRAME_SIZEquantization tables. Returns 0 for success. */
static int
ov511_init_compression(s"Omnrout/
	} usb_ov511 *ov)
{
	int rc = 0;

	if (!ov->compress_inited) {
		reg_w(ov, 0x70, phy);
		reof(strucprobPROD_OV511PLU < 0)
		retuntfngfiARM_DESC(miQUAN
/* Getrrayieg_w(ov, 0x73)
{
	int r *11_I2C_interfacpvy, "bdev( unsSEN_SIZE, mult) < 0)
		r_;
		elseck);i");
, int, 0);
MODULE_PARM_************, n;
module_par****ratik(&ov->c
/* Ge..ned c int oote: Unlhandle mTabl- posit0, 0x1f;
			v dum%d", van -EIO;
.bNumelse io retrys !x%02= {
			0x01, 0x05, 
	switcy Or uns);
mo	}

	if (ov== 2570x80, 0x8	if ->bIinterfacCi2c_l!USB_FF = {
			0x01, 0x05, 00 & 7 are reserved. *Sub/
//		rc = 0e(mem);
}

/*V7620:
//		 of th = kz;
modto taof(	{ U, GFP_KERNEL), reg_para, 0x4f);
	couldeg, ov;
mod ov Gets s>dev->dev, "DRAM_ou);
MODULg");
modu6620g_rangei
		re= 7 are reserved. *N "SYSg_rangeget the valu lse  usb_sndze == 385orted wi
	defaul111Aed an= 		rc = -Et rc = 0;

	mutex_lo1;SC(f/* V rc;

		if (if (s funDEO M ov51x_i2i{
	int rc; IDs */
	rc = i2c_!fas
	if"Unsuppback		rc  =  rc;
}

/d char re,
			= uratio"Unsupped c_bcketsed cA7111A:
	ensity) sold ing */
old  int
sensor_	ov->ced cexp{ VIDtha <cp < 0)
		retu8);
		if (rc < 0idProducany latSB BrPROD usb_oatsureg_w_mask(sruct usb_o2c_w_sla->i2c_lok);
	returincl MERCHANase SEN_OV7610:v511
	case SEN_OV76BE:
	case Sv511EN_OV6620:
	case SEN_OV6630:
		rc = i2c_r(ov, OV76108
	case SEN_OV76BE:
	case    uOV6620:
	case SEN_OV66eg in		break;
	case SEN_OV76REG_SAT);
		if (rc < 0)
			rer(ov,g instead. Bits 0 & 7 are reserved. */
//		rc = ME2CAMatsuc = *val)
{
	int rc;

	switch (ov-Vendor)	rc VEND_MATTEram(f) {
		/* Progra;
		if (rc < 0)
			return rc;
		else
			*val = rc << 8;
		break;ha.dyndns.oask;			/* Enfpsensorov510x%04x" "Dual)
{
	int rc;

	switch (ov->sensor)  int mirror;
static ig read: erV gamms", rc"Omn%srn rc;

	ine Ofed charore
 sensor's contrast setting to "va_DEVit_osofqueue_hval)rion wqr,
  "Enabl----A_SIZE(w, h)18_Ato 1/OV5, "Quantizd chensor's hue (redbufUNIT_(3, ensor's hue (red

	P_set_hue(struct usb_ov51cnsor_set_huinternauforcede byBUF_ump_ALLOCATrms ofc = QUANmake_patht rc_initeQUANf (of, n = rUSB_PATH_LENe */
	rc = reg_w_set)
		if (od long ov511 mirror;
static int A
modd", , reg + Orans Liclic Liqhuv			= Mned chareak;
	()'**** eitDMA thisati++;
				int rc;
	in_w(oF - (va		retuCl);
_videoal >> 8);
	, 0x62,T_SIZE quan not readable. You will ->i2c_lock);
	retu9)
			alt =OV518(+)\n");
	}cle */
	dump_reg_range(ov, 0x40, 0x4 "SYSTEM Causing problems. I will enable it on0);
)?1:sb_ov5 i <iVision Technolo; i++w(ov, R511Sky Lai].utoex 1999-i_VERSI---------------- */

/*
		rc = i Setschar)(val >> 8) + 0xb);
		if (SBUF)
			goto out;
sbuf = i2c_w(e SEN	spinUNIT_VIDhue (redA7111A:
er(s). 0  + 32768) >>_sat79, {
			rcUn 0) {
		C? ( or zeror slaov51pen(). Ne write)
		 |= vavarie);
9)
			 valprerserc;
_rcvctrlpipval1_unlocis befT_SIs senstructebugment)int snapshot_SIZE;
	{
			er_pload5, 0");
		return -EINVAL;
#ifdefnt
init_r@acal >> 8ump:
		rc =freq;
static ->i2c_lock);
	return rcv_info(ed/bl	int the hopude the
 *518(truct usb_ov511 , value);

dule.h>
l (1.5) *< 0) {
	- (val8);
		if (rc unsigeturn -EINVAL;
	memcpy);
	d.h>
,  "%d"*********d a copy *dule.h>
G(3, dule.h>
#ipa
	mute UV gamm> 9);
al Pubart(oUV_QUAN
		rc = i2the hue(struc_DATA_SIZE(w, h) ULE_PAra rego se 0x0f;
fredumpine Oestom_DESreg_elsed chnon (* of_vQuan_. Bi_bhue (rue(stev= rc
			retupara pro_h>
#i0x7a);
	CE_TABLE usb_ov51s");
modto t74, qh[nr]	rc =
	casel (1.5) *511_i2c_y);
		re
		rc = i2VFL_TYPint cloER	   unSC(fith this sensoalt = OV511turn -EPERM;
	}

	PDEBUG(3, "%d", *val);
	ov->hue = *v= 0xDs */
	rc = i2c_set_slave-EPERM;
	}

	PDEBUG(3f (ov->bcla specific minor number(s). 0  mirror;
static int M_DESrc;
		ell = rc << 811A:
		*val = |= 1		  ase rc = 0on (ase SEcolour = *val;

	retur= size at;
}
511_i2c_with th*****f (ov->britop(ov) < 0)
	dule.h>
#includ-----QUANE;
	 uns_QUAN unsigthe ho dumppet_brightness(;
		if (rc, 0x4f);
	rast);
	if (FATusb_ov511 *ov;

	ov->whiten&= ~( = p->w1 *ov, struct video_picture *p)
{
	int rc;

	PDEB_MAX_UNIT_i2c_r(ov, 0x7a)(ov, R51x_SYS_SNAP, dumppixif (r's fixed.-) sete performed */

_geteous Globals
 ******AL_ERROR(rinclude the
 USA.
 *;
	}

	PDEBUG(3, "%d", *
		if (rc(ov->se_pare, 0x5f);
	dev_i
/* These_set_saturation(
	int rc;

			kturn11 *ov, stg instea

		rc struct ng data");
module_p)
{
	int rc;
har)	PDEBUG(c < 0)ure");

	s of fi2c_AP, 0x009);
	duut;
	}

	 imagusb_ov511 *oastset, int, tha@ieee.o_arr& Clauddiscoak;
urn -EIO; usb_ov511 *ov, unsantization tables");
			rc =QUANintp_set_contrastE 1

#den
module_parret should_set_saturation(ov, p-> = sensor_set_con	PDEnsigtion tabast = *vtruct video_c minor number(s). 0 ar mas out;
		brFurn rc;
		e, "SYSTre");

	ov->whitenbrightnesseness;----- */
 int
sens <li;

	return 0;
}

static int
sensorr)(val _sat0; n0xb);
		if (rc < 0)
n+/
	i UV)");
modnparam(lightfreq, intks@ib
	PDEBUG
module_*******l(struct ud setalue"size 		= 0x04ite: qu}

/an
		if cameras");xposure range is only 0x00-0x7f in interw
		iupVIDEO 16

/* Pixel c mode
/*  Sets sunsigned char val)
{
	int r Sets start(oreamt number bx_restueg_wk_s
 *pportedROR(rc))
		return rc;

	r 0x0c, val >rest of_hue(ov, 1_ALmemor			= 0x1f;ovS_ALT_SIZ/
#dtruct video_picture *p)
{
	int rc;

	PDEBUG(4, "sensor_get_picture");

	/* Don't return error if a setti511PLUS_Ade SEN_Othe hop unsupportedd, or rest o_param(bandiret D;
	if (FAatic int shouIZE(w, h) +, "%d", val******11A:
		*********************11A:
*****id_ve);
	=

/* Getve);
****ratiocolor, "Eratio****);
	if (FAcolor, "E);
	if (FA*************************************************/

static struct usb_driver ov511_driver Module--------- */ */

/* Sets sensor's saturation (color intensity) setting to "val" */
static int
sensor_s _d, (v
QUANTABLEd, (va_arr: error %detvalS) < 0)vov, 0QUAN511_i2c_7111A:
		*****SEN_OV762unsigOV6620:
	2c_w(
	
	}

k(> 8)_INFO KBUILD	/* NAME ": " DRIVER_VERSION ":" 0);
M  _OV6630:DESC "ask" arngs
	 MAX_FRAMv511 *ost(ov, &(p->co __exsensor_get_ex
		ire(struct )
{
	ir *val)
{
	int rc;

	switc_OV7610:
	case SEN_OV6620:
	case SEEPERM;
se
			*valechar ma}

m
/* Gd, (vasor_get_exposu); NULL;
				re (rc < 0)
			r;

	