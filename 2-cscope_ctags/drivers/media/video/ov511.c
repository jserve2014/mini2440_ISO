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
		unsigned int fnum = *(( Copyright (c*) argra-t	struct Omni1n OV51 *mpres
 *  McCrc;
e Cof () 199>= OVdecoNUMFRAMES) *
 *	err("idge Driver invalid mpress(%d)",c000 dde C	r/es
  -EINVAL.com}t 19t Wall= &ov->mpres[000 ]hixesPDEBUG(4, "syncing toet Wall%d, grabstatby O%d<bwalla,
		 sion 7mpres->shot code)<olawswitch (20 fixes by Charlies
 SB Brchnol_UNUSED:@san.rr.com>.comColor ngieee.orlaREADYatsuiva.com>
 *GRABBINGinal SAA7111A cERROR:
redoinalo98-2!ion dev)tsuooka <claudiO<olaw	rc = wait_event_interruptiblepbotha@iwqore
.comO code freee.org>
 ==com>
 *DONEsava.c* O||v.cee.oGerd Knorr and Alan Cks@ib)l P. BURB erc* Memofrom pw W. M199RB e0ement (rvmalloc) Charnorr abttv* Cha*r, by(y Neml deVisror pres1 Camellac) < 0* Memoalloc) coothJohagoto net>elt.
}e se/* Fall through */ve Perks <dperox
 .comr, byror snap_enabled && !* Basedsites byr writtenee.oPeter Pregler,.comScott J. Bertinc) cot.
 nnes Erdfse seies
  PleB Brsee e sealpha.d on the Li * foClaudio Mtherse fiReset it
 hardware yndns.or buttonation/e filIXME - Is thie Soe best place for Soft?ished b * fo webyndn at:  ht)tp:/ alpha.dLicense gg/ov511
urrediopshed) a = 0ou can Preglclear_License (ovc1tsuoitc) c/* Decot Wallion,; eimay Chnverseful,etc...ersion  Preglpostprocessott J.pressdvemenbreaknectf /* eErdfela <c*/thatka <clas pro-Unges  improGFBUFersi
 *Originavideo_buffer *vb =llan<olawlor@acm) anRPOSE.  See" drivememset(vb, 0, sizeof(U General Public Livanty oFOR A PARTICULAR PU
 *
 * UNIT it
 GNof the GNU Geue th*vueredi eitmore detailscan rediRTICshould have recuived a copy ofgram; if noto tPublic vu->GNU Gmore ->vdev->minorse sde <lbi =URPOEO_NO_RTICnux/inuxradix/moh>
#incluinux/x/vm/vmauloc.
#include <linux/slslabteletex Thi
#include <linueral censree Salong wegler,IOC_WI2e the GNU Generl sioni2c_U Gener*wree Softwarka <clae <lw_slavcott Jw->i386_	nclureg<asm/valu <asm/maskd
 *CULAR PUorincludeRde <linux/slmminclude <linux/ctypedrree Softw Mark W. M19Petere <lr_lude _)
	#rlude <liarm/cpuc1@sa,ement is free nnes Erdfelersrture.h> =; you cLicense
 * alodefaultinalre deta3, "Unsupported IOCtl: 0x%X", cmdc1@saka <claudNOIOCTLCMD * aBILITYrsionr FITNESSLicense
 *}

 Chaic proc
 Preglv4l1_ioctlA 02139,filusefile vid 3 Mark W. McCcmd,om>"
#defirg>  * fd
{
if not, write device *incl =dio@c->private_data;erarovemeusb_
#inc *o_I2CGNU Geget_drvfine(incl#inclION "v1.648-2mutex_lockthe icatsuok() rror NIT_)* Meka <claudioTR
#inPeter USA.
 sere, M(ectivDRIVERarg,re infoterm
 * U_VIDEO ne: DP. _MAX_Uunountixel count ; or \
	<c W. by Gerd Knsdge,_t&r the termareadr FIthe te@con MAX_ char _*/
#d *bufridge,_t cnt, loff_t *ppost J.NABLE_Y_r"
SC "ovne * Sco#def_RETRIES 3ety */
#dEN McCnobountMAX_RAW_Df_flags&cludNBLOCK;
o@coark W.ScotUE(w,e <lcnt ENABLE_Y_QUANT !=  1SIZE(w, h)  != -UVPERM)

/*****SC "i, Peter0ed wmx = -1rc) != -EP
#incl be usefunux CPefine ine MountVIDEO 16
e fiPA_SIZE(w, h* bytes per YUV420 pire details.%ldions o,) * (h) = Kfinetin *********

#de, by ine M|| !bufg/ov51Peter-EFAULinux/ rediic_iinux tha, by rftwaessThese variabnty d e: D codic globa//three : Only USBch \s tw000)
 *s
 itYou  if a00)
 * is ll blude ,blicn use itANTY;  it
 webSky La0].s by Char >code froxt
 * inttxt
 arl  driveished *******ARTIelbrigps by;
tobrig1t (ccams			= 1ix;
statht ( Module  1;
static i testpapix;1ters/* If non*****/26/we f y */
#mmediatelyntbsitesh********t your********ny lato */
staAGAIN
statiautobk W.  			sorix;
stOmni1.press;
neok, butaov511.txdeee.oD x14;
int sn	= 0x06;exp 			= 1;
sinnapshoim (s bybing)ebugbridge;
stati 0x1f;phuvd 			0x* Chax1f;ppix;
stat		= 0t led 		 code fro4;
stati* Memnt dumpint bandingdumppixc int banding htt		int bandingfNIT_div 			-= 1;
****		= 0pvint li			= 1;
sactiIL "startify q int = -1;
stati W. freq
stator more info.
 *
 * This pr-1;
sta)x05;
str Manyeval:re info.
 *
 * Ttobrig 675can rediobright	 alo		= 0bynit_Oxel cstaticwrmx <olrestati:als);
stdynd to zero */ int bandingx06;
static in 			Wait whx siwe'restati banblic imageishedre details.Omniaram;
statule_para OmniPeterosoftint gVIDEO 16

/* Piux CPiA drvidagement (rvmalloc) code froc int Memanag Based on the Linux CPiA driveranty dyndns (rvdevusedmarkini + o0);
MODGotM_DESC****arrodify itomatun*****notha@ieee.org>
  Pdule_param(a*****_recvdexp, i(x03;
st,haiva.cexpombrigally/or modify it
 * uux CPiAanty of/ov51modautobrigm(deaosurt bandT_VIstseck! ** Ebriger00)
 * *****l: 0cur, 0);ant markitt phnges ic Knorr a0 to (See ov511.txt f;
static i Copyrighproce= 0x0_es gain");
m bandn rediststatiastU GetatiRepeat untilintpget ainclude  a be use;
statippixnclude ,arl ( more details.ULE_PARMltaneousgfiler");
mapshot;
License, or at y/alpha.option) an/Omniveon S=none, 1=inits, 2=wark/26/the Lhot, int, 0);
MODULE_PAARM_DESC(snapshot, "Enable snapshot mode");
module_param(cams, iule_param(aDULE_PAR 			Cistrblic can rediTs");
vel: 0=none,  Modulumpp: 0=none, c int d,umppix, inumppix, "hiARTICam is dRM_Dibuot, iy it
 ho, 0);
Mwitic estat ANYas pint WITHOUT ANY WARRint snthout 
 * 		= 0x0plint, 0);anty re details. ov5=SnapMODULE_PAR=%ld, lengthdge,);
momx vidt, 0);
MODULE_PAR vid != 03;
s_Dump taram(aomaticILITfinSC(dum/200ese)
space;er othe wt qhypartialsE_PARs"Dum//c int rtin a+ raw pixel datdead)tobrory >of sp_sensor 0);
MO 02139,*
 * Module P)_param(tobr	rtin a &nt, 0);
canr, int -umppix, int, 0);
M 0);
MOstati Frrtin aNU Geifuncto bes, inmppix open)")am(phy, cketh, "Print framparam raw pixel dataCexp,_DESC(phuv"Du:**********int, 0);, "T
 * (nit.RM_Dxtra YUV(****xmodule_*/
#h 0);
MODULEt, 0);
M"Predict1_MAX_UNaram (horiz. UVfailed! %int, 0) no, 2=piebridiT_VIaw*/
stales () cot autobright		= lt, 0);
MODULE_PARM+xp, u (rc)re details.{RM_D}LE_PARYuse imp, "newESC(dup_ge (ge," vidint, 0)mppix, int, 0);
MO, "Pri
stae: De (vess AvFoenump r open)")ix, int, 0)
module_pa
re");pE_PARure");c in, "Predictt, 0);
MODULEt, 0);
MODUxp, i	= 1;
static imirrogain 			= 1;
statiLE_PMark intas avail, orutobrig. Y) av, iint snw[s
 * (See ov51 int bandingnder it
 terms ed, int, 
  "Reation;obrig with !vram cal baric int"Enable snapshotka <cledy working)");
module_param(dumpp org> & B) an_PARMfinish debka <clone,int,(sweet <bw_param(ay */
#dine DATA_SIZE(w, h) (ine FRA, "Quan
cams,:  "Enable banding filter (to reduceMEng filter (tinof(Originatimmmap)) detaMax a coptionof the Gm_meva/
#definevmac frame for safety */
#define MAX_RAW_DATA_SIZE(w, h) _ks@ibuant ((rstatic= vma->vm_, "Prediction rE_PARM_D, "Feta co, "Fo & C-ze");
modrce a sp != -EPERM)

/***********************************DULE_PARM_DESCp 0);
evenof t * fault tt frNULLtions of thesevemenre details.
M_Done, p(%lX)pvy 0);
Mcketws pic, int, 0) pic > (((gler,
 Teby Clva.co OV76*ble banding fing");maxwidthmaxette, he W. ss, * OV76+ PAGErce_p - 1) & ~(palettm(framed****ons of these)@coners
 * (See ov511.txt for detailed descriptions of these)
 ****posThis3timek W.511 )[OV51buf;
	_cely" */
ake 0mask mp resize")he L, "Apfn((void *)isoct, 0);
Mremapobri_0);
M(vmae
stati,ont ngi palette SC(to t_HARED1_MAX_UN"Enable banding filter (toons of thes;
styd 		aramstatic+=o,
  "F);
m 0 objeam(remove_zeros,gsove_c uns palette * Memo tak- 0);
MODULE_PARdumpDESC(
MOD_DESC(, 0)"Enable banding filter (to reduce.c by Gerd KnconsteictiSIZE4l2_ MAX_opes, 2PARM/TY
 heopject{
	.owner =	THISorce a ,p rapen =M_DESLEDt tim 0);ely"releges =, int, 0);
Mclosiva.._PARM_x, int, 0);
M0=none,.RM_D(experiBaseal)"RM_Dt, 0* (h));
st, "Et:  h* (h),
};filter (t+e for safety */
#deincl_templ4;
stamp rnt_vid		"gler, LAR c fram + o.VER_ly		&ULE_PoVER_re");Omni8");
safety */
#d_ Miscelt, 0)ude  =	-1xp, i)stpat;
s*************************************/
 int banOriginaQUAN  "DebuOmni
 1E_UV0x06h a& Ch "PriLE_Pfigutfreq,driverSC(p I2C transpaleon. Im/cpB BrtC(leifnt p<asm/re gett/26/"FESC(pove_eval stpat Toftwint,terizaramhOmni7610,sM_DE20,
staM_DEBEbr ovf . TPARMes = Docusor sESC(dzompregistPrinetic isqvuv_PARM_DESCi2sinceB_DEy neravery similar.
duleicalmppix, in7xx0_ime_DESCeme startERM)

/******c fratpat;
stuhtfre********/
#de/*MAX_UeMNIVGla;
mo [<lg@jfm.bc.ca>_DES, in:
	 * 0); R_t* Term0x0fqvy	_PAR_par hUSVISIVfo senone,effeceshold (DEVI0x85 (AEC method 1): Boundove GNl, goodLE_Ptrast C(qv0nT;
mo541[] niVi
modYQ2): VD_OVstatebug(com0)le51a5 (spec shband_param(): Ok,as pblic Flack level iopyri	shif\
	&resulic ison loss ofrightes ppyrighc05 (oldanty o ] = O{
	):ROBLE;
stable sna, too muchyrigh_UVERM)

/**/EVICt, 0);
LICEN
#inclregTHOU"aR******Norm_par[]******	{egler,
MAX_BUS, 0xVEND0xff }t, 0);
MODU
/* Known OV56
/* 01-bas*****/

s/*vmalw
 * S28igina4ompresast, "Enable Originaeivexac cameras */
static struct12-base0 cameras */
static struct3symbo81c_list camlist[] = {
	{    "D-Llic_l2CAM0cnt testpat;
s5, "Puretek 0f"D-Li5511_/OVlg's********10" },
	{   5qvy,uretek P15 },
Tnk DSB-C30 USB Life T.orgGe5, "P1IDEO ******* (no ID)USB Li23 TV (a2, "Lifeview*****)" }AL D/45, "Pu+B/G)" },
	{  36, "Koala-C{  218K+B/Gife TV {  36, "Koala-C1  1,a2+B/G)" },
	{  36, "Koala-C7 3" cPC-Mw USB Life 4ret Mtekvisa  21,OV51isngfil{  7********fe Tneric e+B/G)" },
	{  36, "Koala-Cd 3" 93Mus" },WCam 3XUSB Life T3,5, "P7 "Creative Labs WebCam 3" 31 3" 6"oboCam" },
	{ 102, "AverMe   1,2dc_list camlist[] = {
	{  3PAL D/rte MV300" },	/* or OV7110 am" }482, "Lifeview USB Life TV fe T1, OV51  70, "Lifeview USB Li1int F, "Crght,ve Labs Web},
	{" 0oala-phaCam SE" },
	{  -1, NULL 0},
	{phaCam SE" },
	{xt
   70io Ma. Al rte MV**** rediSeric OV NamwrittHI240" },
	{ VIDEO_PA2*****************************0,	"GREYUSB L Life TV (NTSC)" },
T
};

8  41, 8B/G)" },
	{  36, "K0h. AlPALETTE_RGB32,	"E_RGBUSB Lifs EZCc_PALETTE_RGB555,	"RGB555" }ycam h \
 MVve Labs	/harl OV71100ParaZEY" },
m" },
	{ 102, "AverM },
	{   4, "Generic OV51TTE_GREY,Y" },
	{ VIDEO_PALETTE_YUV420,	"YpLifeview USB L.txt _  -1, _par }
};
statVl Pu4ESC(d1 Ppalettcamlch. Ale_p},
	SEUSB Life PALETTE_RAs EZCAWUSB Life{ VIDEO_PALETTse)
00B" 8UV422P" },
	{ VIDEO_PALETTE TV (/* Video4Linux1 Palettes */1ycam eRoboC00B"  Tech10+B/GAverM1V" },ETTE_RGB565,a (no ID)" },
	{ "D-Lneric Camerist[] = {
	{ BRG_9  1, tion kit */
	{ 134, "Ezoni1eON" f;

sc Camera (no ID)" },
	{0 3" 
	{ VIDEO_PALETTE_UYVY,	"UYB Life  Tech25ret ,"YUV ect immed2P" },
	{ VIDEE_RGB555,	"RGB555" 211P" },
	{ VIDERGB555555,	"555" (s EZCTechPALETTE_RAW,	"Rst[] = {
ycam Mca MV402" },
	{  46, "Suma V" },eAL)" },
	{  41, "Samsung Anneric ALETTE_RGB32,	"RGB32" },
	eUS,	"ON_OV6630AE,	"OV6630AE" },
	V518"{  41, 30AE,	"OV6630AE" },
	+};

stat0" },
	{ SEN_OV7620AE,	"oala-8PALETTE_YUV42senLifeview US},
	{  "Creative Labs WebCam 3" }eOVint i	"RB err};

statSEN_RB e2-60074haCam SE" },
	{  -1, NULL 6EO_PA27TE_YUV42urb_errLifeview USW,	"RAca MV402" },
	{  46, "Suma6h. Al5		"OV511" },
	{ BRG_OV511P6s EZCd

statBRG_,
	{8PL{ SEN_V51600B" 5OSR,	"Bic Licdefaul(
star < 0V (PNULL }
};

static struct s6ycam 5b Ter(efine Osend_DES******V" },9"KS0EPIPE,	"S****edmeout (bV511_,	oo muc1};

statdevice s1P6{ SEN7tion kit */
	{ 134, "Ezoni6V518"2IME,	"Device does not respo0127,	"KS0127" },
	{ SEN_KS0127B6oala-	"KS0	"Device does not respo},
	{ETTE_YUV42fer error (overrunr, by faul14;
s:amlist[] = {
	{ 6 symb1dble (device sends too muc7EO_PA8b**************/

*******voW,	"RA8600,	"OV8600" },
	{ SEN_K7h. Al1haCam SE" },
	{  -1, NULL 7s EZC5ze =_pale_ALIGN(a cora-tmem00B" arraymem;
* Copyrighproceadr errorPAL/SECAMsion Tech10+" }Li7{ -1, n NULL;

	memset(mem, 0, siV" },***********/

static struct7 6007"n NULL;

	memset(mem, 0, si{ SENageGNU rved);
Mhe L_to_page(V518" *)adr));
		adr += PAGE_SIZE+" },eca MV402" },
	{  46, "Suma7HI240"8600,	"OV8600" O_PALETTPALEEO_PALETTE_RV411Pre details.>****,
	{VISION, retry 675 Mor ID...dumpDumpdunda1" }ERM)is nautomaryt qhyalettes en)ump _DATA
		Cde <liude  regl_OMNISIesho();
modul,e recarra_idump_brdr)ra-t	adm is freeka <clauh, "Packe" */_ov_;

sta it
t, 0pyrightre detaALETSIZE;
5;

stat*********d (511_UVQUAov511} dumpp1" }am; if not, w76xxnt testpat;d (_ott Je)
 2P,"80m is free );
} (si
MODULE_ov5118 qhyitV)")****(snapshohed msleep(15Y)"). BRM_DARTICEND_MATb_= 0x0 ump t qv <rDESC(detect_triesg/ov511ULE_PASC(d32(siSIZ610_REG_ID_HIGH)dule0x7F(at");
eodev.

	lor@acmV (N0x%02X:0x%LOW,_DES, A21_MAX_UNI* Cam Copynsor of MERCHANTthe GN*******	i++bute itth			= 1Wasuf[0=Copyrightes pre.he)
previously bands ob= BCL_ODocuesto alwayDumpgain	= 1*ov, un. WheER_A anym(phactu**** dependodifint,aqvuvgze >unkc st, 0);
MOi**********  (ov->bclasint 	rc = usbV511_ writtIT_VIFRM_DE/,
	_PARM*******ID. You mc isitionsholdaopyri ov511.t>cbuf_/char valt maybrigtionresponding.C(caortYUV4, rcZE;
her wao " EMAIL*/
/* returg) me"s oram(a waULE_P"/* r,can a*****DESCsus_DES"T(tesnint phfe TV (P_DE    hou/***
  aissuefineer */
 mucxnowtobrimeras */
sta>devore
	 msg(HI240" },
	{ VIDEOter */
sta)
		Rructt2, %dxdingi+1*/
/*le_para. 0=otes faul%d: (sub)typams, in eitESC(d(&ov->cbuf_lock)COM_I, int, 0) c is f			   *ov,, 4=f nlock(ic is****** REG */
/*;
by NemQUANev,
			estpat,
&10 ereg3RECIP_MODULEfoSC(mir
#inccont "Stpat;
staaEVICE(VE\ror (, autob******=tic s>cbuf_ut, para
	ifQuanin a)****	tmask manti don't 0],  what'ED pral asedaboRM)

/*t usbymodul sRECIP_nsor ****1.txtce t1* Memgread :*******8)?1%satic ieneric OV(fer er20AE*/
/* reom uncomes bits at positions specified by mask to Bov->c rf_lockgler,+ =offph
statitionzerosunitparareunl= uswits se
 MSION, Pe_i****me stvC(qvoSS F SomeVICEneed nege sgnedin*
 * Mexacom ag.G)" },
	{uf[0&cab_dtwarftfUNIT_[0];
dr +ge ( num=TIMEDgler,PLUog writtes bits at positions sp	   u "Ee, ok(&obrid/ are O workarBLE;*/
/* retra-to else "0x%y Nemo2ARTIControl_msg(o
{
	int ret;
	unsigneBng from uf[0];
		PDEBUG(5, "0x%_rr("reieeeits at posiimage yQuaif_briby if

ove_a */
/* retur	int ret;
	unsignenlockev,
			     *ov,U[ov->1_PARM_D'sm un"uto (on:, 5=maBUG(5, ->cbuf	/* En,ut,  stting ");int ret;if


	oll = umppix, int, 0);
W timeRMl = ceE_TABLEst, rc)ackeov51,	"HI240"ott JYUV411P" },
	{ Vpecifl | value;	/* f[0];
(reg_w1 Cambuf[0newval));1***/
), 2=rndns multipTimen)cificv->bcl= (~m single r1->deer. int aby Brewlass =Seice se)?-yQuaifsizearel: 0 (0x;eDEVICE( = 64dvalreshold (vert = 48uf_lock)%inck(&ov->cb,, un, n);, n=%daticet(memc int ban zero donorr mar FI_PARtrol_mview USB Lyet_lock)brval)n snapsha co<< 8f_lock)unlock(&ucontrol_msg(oQUANsndl Cope(ov->dev, 0),
			hhat antrol_msg(o (MAX_FRA code long) meter */
stac* WIst 6 char6)reint vdval AEar valu;	/* F5_OV7620An rc;TablCE(VEND6);
	VIosition******T,	"LUS);

stat);

staIC "v1.6DEO_PALETTE_HI240,	"HI240" },
	{ VIDEO_P6xOV7610" }6
	{ SEN_65P,"YUV411PHI240" } },oorerif no  36, "Koala-CNTSCife TV W,	"RAW" },
	{ VIDEO_PALETTE_YUV
0,	"OV7621P" },
	{ VIDEO_YV,	"YUY TV (d *meooreFor wqvuvx06;adjuv, "F>offw USB Life TV (NTSC)" },
TV" },ooreoliclong)e ) {
	e sele = Numb411, _unlocoluf_locwhicenpo McCl
	int i, rc, reg = R511_e)
 *P,"YUV411P" },
	{ VIDEO_PAto t"BLE_Y_P,"YUV411P" },
	{ VIDERT******;

sooreCOMStruct usb_dgned c*ov)
{
	udia Inal199-2p= OVEle sParaaist c");V411P" },
	{ VIDEO_PA*****se)
 ooreyrighe AGCle++;
			val0 &= 0x0f;
				r/*
 TE_YUV4/OV7x16V51106 helpcompres;
stbilse v "Ligmovcharobjtes le++;
			val0 &= 0x0f;
			{ -1, tion obri,
	{ SEN_OV7620AE,	"igina3_quan_taApram aramerr  0, vale, or(iThis; i <OV511_UERM)

/ code2bca MooreBLCLEg fi/2ock(&)

/*28 "0x%5 Sele++;
RGBfault), ifn OVqblersionC300" },
	{   4, "Generic0bble (device sends too muc V518"g adrooreDisEBUG(03;
srloc) (ov->bcutobr70, "Lifeview USB Lifn mem;IDEOe511;= 0x0f;
;ar re2a[7] firpyrigh/
static serks <dpice rks <d9 OV5k theoid *13.orgEzonics EZCag_w(ov, conectP	= 0x0charPaeg wTermble5118

	memset(metes p000B" d
 * ightckdiA/Duvfect_w(otiztic i _TABLDump 
	f	SetPaadr += (memset(mem, 0,a co3{ SEN4Tech.RM)

/**, "0x%	val099-2oala-3al1;	
		PDEBUG(5niViodens ong,
	{  36, "Koala-C1 *ov)
{
	uTable+
	memsetC;
	}v= 0x&ight00 |=, un1 << 4;
	}unsigntain
 para	0x0f;		val0 			if (rchindTable++| 0)
				return rc;
		}

,
	{ *)adr))N	if Psed, xt			= 10xc4 - 0 (0x4V518"4b. Bee LE;
cuBaseed] = {yless
  po+ OVto_le	   r balarc <uantization tables");

	al0 =  *)adr));
		adr += PAGE_SIZ4+" },ENABLE0; i <		}

	uneg OV5	}},
	{ val0);
	 u321ile c"QuanisMAX_b it
g++;
	} (ENABLE_UDEBUG(5r, byc "Creative Labs WebCam 3" 4 symb******// Do 50-53ask to ryles");
?lus Toggntiz1_u[2])
			dingon here?E) {
			val0 = f;
			emset(mem, 0);
	END MARKEtatic 	unsigned char *pHI24ror ESET,P,"YUV411P" },
6x3cameras */*OK*/****aram(dumpp
);
moduploadg_w(ov, +) {;
MODULE		val0 &= 0x0f;
			Copyri  41, SYS_RESETPredic;
		PDEBUG(		res

	memse/*0A?***********************QUANTAable51**********_unlock(&ov-0ock(&****E(VENspecifreg = RreseCOMP_LUT_BEGIval0 &or@acm.orgU
		er/26/BLESIZE / 2; i++) {
		if (	"Bi, va***************************st[] = {
	{ ***************************,	"YUV411" }/*A***********************r, byc str6//

/*am" },
 Note that this
 _PALETT8600,	"OV8600" },
	{ SEN_K"KS0127" },
rr ai************************;
		}

		if* regis +		if (rc < 0)


		reg++;
	}// 21 & 22?_locksugges uvQua.h>LL;

	 wro****Go);
			_param(ion. M_DESC(lee > ordr +y o20,	"O1P" },
(ov->bclas{******ight***** TV (9AL)" // Check
 */
wrb_e stiza] = OECIPit
 , "Pri
	in Wal	mutend w rc;rn 0;
}

static int
ov51-to-
 (rc < 00*****/
OV5118**/

/* NOTE: Do not aer th2c_w(). NotrlpiaGeneis fu8)t, "Enableking.
 */
stRniViMAX_andiock(&u(!mem4= 0x0f/ e det: TribrightUV bused chR51x_I2C_SADDR_3, reg);
	set: command failed");

	return rc;
}

/s */
	RscelMAX_CTL****01(ENA{
	intization tables");

	*pUVable5, 1, * Low-levelation tables");

	
 *
 *******E: Do value)
{
	int rc;

	PD1_COMP_tion0xce if );
	NOTEBitsegist/O fundETTE_RGB565,	"RGB565" 
	for (iETTE_R;
			if 8518_i2c_wriigned8bbuf_lock);
	02 Lawbuf[0v	SetPawB_RECIPth40	for (retries = OV511_I2{ SEN.txt 0xceval  addx0f;
h7l0 = *pU0)
				return rcal0 = *pE) {
			val0 = BLE) {
			val0 = *pUC_SADDR_) {
		(ENAB		PDEBUG(5, registeset_type)
{
	int rcC data port of OV511 */
		rc  (*******************= *pYTable++;
	pUable5ov51x0);
			le */511_i2ov51xO functionsbuf[rol_m0x%02X", n 518/* Clerc < 0)
			break;

		/* Write "value" to I20)
				return rc;
		}

		if/
static int
oyte write cy	reg+igned k);

	retu0x%02X", reg, v)if (r(rc  *pUVTa_DESCeseBUG(5,reg_ MERC	/* TTE_Ack?t i, rite "value" to  0);

	if (rc <x_R_3,t{ -ETIILIUe: e.563u, Vdif
	714v oduleic1	break;
		}
#if 0
		/* Ir, byrc = reg_
#enV ;
sttgeif (rngfiERM)killer:mbolon	rc 	if (rset_typ2X", reg, val

		/*" },
/
	unsign 0)
		return 500B" 2

TTE_Three bGC  */
: 18dB/* NOTE: Do not call this5V" },i "Cr;
#e( = reg_Once,ATA, val {
	ar value)
{ SEN_ rc;

	P difdxp, cur
	muttic : +1ed from i2c_r(). Note that)
{
	unrc;
}

(		return rc;s noorr ag_w(rv, R51x_I2C_D0127,	w-level AWB chromin		if ble sel: 0=noom i2c_r(). Note thatoala-V8600,	"OV8600" et: o no=ries = OV51E: Do not	/* ly!
 * The OV11 */!mem
			(rc < ;*******

	adr = (unsigned ***********************SBet;

	oror %"aticov51es at positions specified by mask to r_MAX_x0,k" aeg. Geno to I2CRad sorr a
	  ask" av->deeither wageo4Linis*****1 *ov, unsign"Ligcer qvytting},
	{ *	/* 0;
			res");

d I2/O
 ed cha /* REGg, u321:3BILI02X:0ot, trol_msg(o);

sIR_IN |
	retTYPE_
	ifORlue;
}
	rc = reg_wEtrol_msg(o0, (__u1v->cn "vion al | value;	/* ipshot 
	if (rc <= (u	/* ****	int ret;
	unsigegatsb_ces bits at positions specified by mask tegatsk);		/ | va regck(&ov->cbuf_lock);
	0);

	if (rc < 0 int
s");

ternalmmand failed");

	ret = usb_ NOTE: rif

);		/{
	inandig_r(ov, n 0;
}2_lock(&o byte write cycle *eadenericetries = OV511_I2C_RETRIES; ; ) {
		/*  UYV,c = reuf[0];
		PDEBUG(5, "0x% Manyrereturn rc;

	data p2rt of OV511 */
		rc =n rc;
		rc =alue)TTE_In, pos o2Fnt
ovwr_bri		if (rc  u32ck(&ovhtfreX", reg, value);

&ov->352, val, n7dbuf_lock)28 0),
			  int
rMAX_UNIT_(s: negatitatic int
*((__le32 *): negati****__cpuPAGE Ack((0x3v, R51x_Iad_inontrol_msg(ion e(ov->dev, 0),
			  ctrlpip11 C0x10); 0)trol_msg(o10x%02X", reg, value);

	ret
/* NOTE: Doit
 desiI2C_returist[	r* Senra rin
 * registers (0x3ite d 0xc4 - 0xce).
 */
static int
ov518_reg_w32(strucite b_ov511 *ov, unsign)
		return rc;

	g_w(ovs0f;
3rc >_DAT; ; , "0x%y until idle -nt
ovead scyclet i, R51x311_I2C_RETRIES; ; ) {
	o not call this function directly!
 *KS0127ightf
	int B(1.5) #incldeC(packeB_DEVICE(VEksDEBUgister *_w(ov, , R518_.
 */
stas at positions /* Ack? * OV511_I: neghowot c> (2r re: commifailed#if 0 +c_32(siSIg) memksI2C ab{
			rc = ly!
 * The OV518 I8_ue;
	 */
stV511 */
		: negative is error******k;

		/* Ack? *
	intx(B)g_n thturn rc;

	/* I);#endif;

	PDEBUG(5,V511 */ sub02X", reg, value);

	ret (siMODULE_P not call this function directly!
 * returns: negative is error**********BUG(50x08e_zeros;
stunsi read re3rert. Ux%02X:0xe MAX_Chis function directly!
 * returns: negatatieg_w(ov, R511_I2Cosarl zeroericis data */
srra realue= OV511_UMAX_RAW_DATc;

		/* R rc;iateSelec", reg, vta *rc;

	int gX", reg, value)		return rc;

9>g_w(atic int
{
	i			ebc;

	PDEBUG(5, "0x%
	unsignB Rev. AMiscelDR_2, reg);
		if (* regis)B of OV5mera rel this functio: ified by mask		  ogain
	&
	int2:gard2C_CTL, 0x05)r, p	rc = reg_r(ov, R511_I2C_CTL);
		} while (rc > cbuf_lock)& 1) == 0));g_w(ov, ov->c= ov511_i2c_read_		if ((rc&2) == 0) /* Ack? */
			break;

		/* I2C abort */
		reg_w(ov, R511_I2C_CTL, 0x10);

		if (--retries < 0) {
			err("i2c write retries exhausted");
			return -1;
		}
	}
failed")ROD_ME2e > ot(qvuv, ied	if ((Bail _unlnowUV)");
modobje
	mu_upload_qTRIErol_msg(o
			brht rc,*****/

/* Wv, R511_I2C_CTL, 0x05);
	if (rc < 0)
erks <dp0;
}

sV511 */		if ((rc&2) == saa{
	ia
			break;

		/* I2C abort */
		rc = reg_w(ov"	/* (rc < 0)****lue,

/*
 * W[: commc __cn(cloU Gen		if (rcmov, bow-levstatten, o regwCopygivesain"rd Kn Low-l reg_wDEO_PALETTE_HI240,	"HI240" },
	{ VIDEO_P reg);
mturn rc;
}

static int
ov511Dycam c* Clear the ram out, no juYVP,"YUV411P" },
	{ VIDEUYVsignUY);
			c OV5;
#ese)
 str240/286 line	val0 = *p			break;

		/* nt aca2c_r(). N1 *o Mr res EZBGHI2, val0);
			if (rc < 0)
L610,	"OV7610" 24555,	"24P,"YUV411P" },lue */
		newval = oldval | Vs EZCaNULL }
};

static struct s0 rc;

		_par valuave rememived axc4 -Ldval &= (~maI2C tniti Bl = &ov->1,UT_B;
MODu2000iel regeqinternal(ov, reg, newval))rom i2c_r(). NC	val.tranpov, reAPER=0.25internal(ov, reg, newval))1 *ov(rc&2****BRIG=128internal(ov, reg, newval))+" },_CTL,le *YTNT=1.0internal(ov, reg, newval));if ((ov, R51xSATNc_w_if

ies = OV511_I2C_RETRIEVTablensor of HUE=g,
	   unsigned char value,

		/*u	rc = rrs (0xigneld);
	| vg);
		i
	{ 253, "Alpha Vision Tech. Al
	{ VIDEO_PALETTE_UYVY,	"UYNABLE_YSelect ov511_iunrn rc;

		iied by mask to an I2C reg. Bits " },
	{asm/p     succeeds*/
};rdle OV51 adeg);
	2C abi386e IDs.)ov-on value */
		newval = oldval | Vh. Al.txt ULE_Pompos, vainput g,
	   unsignedsize)
{
	unsigned long adr;
ta */
static this 1 (ENAB< 0)
			return rc;

		/* Retrf) {
-);

	if G(5, saa	eak;

	i2c
		do );

	if exhaboCadump  ((rc&2rnic insk);		/Writ518_re
 * Th0x%02X:0x%02X", reg, 
 I2C );		/afor (retries = OV511_I2iate 3-roce6_i2c_0ue,
			      un	/* Wc_wrDump raw pixells data */
h511 DEBU> 0			rc DEBU&orce== 0)
{
40; rc;
Evene distr"LED  * Thv,
			    L_OV518)
		rc = ov5188_i2c_write_internal(g);
f (rc/Odint elvel: 0 "0x%)
			return rc;g_w32(c;

		if ((rc&2) IDeg);
f (rc < , u(str
	/* 
			   as_ > 0)ded, R511dr +num_ t, 0Copy 0),
			 or999-
#inclMODE_AUTs, inTwo top_duringZE;
val0  int=off)
{
gua****ees2C_CTl06;
stati, r)s == ff)
{
doeev, 0		val;

	rse0)
			b, soegisc&2) _w(oread PARMof ) {
acut_msg(ULE_ (horiut/* Write "vwhicha */
y Nec;
}*/
stat11_I2C_CTL, 0x10);

		if (--retries < 0) {
		4ol_ms92c write retries eC75 Meg,  */,
	;

	r3276TE: Dretries = OV511_I2C_
		retur 0xc4 - 0xce). Initiate 2-byte read cycle */
		rc
		retupecific valueh, "Pri; /* REGuto (on&1) 0)
	, "0x0x%02reg);
	ernae afreg)statestore  ) {
ter */
***/_brid__u1mask)
{
/
	rtatic upint sn(ov, unsi read retries)
		return the i2c I/O
 05v511 *ov,			returnreuto (on(ov, R51x_I2C_W_SID, slave);
  unsign1);
		if (rc <   unecified by maskurn rc;
}(ailed");0x%x)\rs (0x;		/* Clear the maskedter */
stAX_UN).c int banFix, R518 qhygler8(+)E1/OV) /* I2* Plgpalet e
	  ed")ount. Octions.
,;

stasiin (rc < = BCLtati	 masd jernalue = remub_contropyraber ");
module_tex_
		retBCLailed"reg.);_cont* Setsg);
		i= ov. Bits &= ma */(&ov->i2c_lock);

v511_		reture,
	ycharunction dv, slve w /e_ids+ask"are g not call this function directly!
 * r511;
MOD1+ */,
as 0ask on cbuf_lock);

	ifsion< 0)
		err("reg write multiple: errDEO_PALETTE_HI240,	"HI240" },
	{ VIDntil511"RGB565" },
	{ VID2X:* SetRregl**********	 Low-ley? char r1_I2C_CTL, (stru-bytINIT,		E_Y_val0);
11Pblestex_unlock(&ov->i21******_NOREGS);
out:return rc;
}

/* Set ernal(ov,  rc;

		/* R	reg_w(ov, R51he re_****alomman(ov, R51x_ },
	{v,		rc = r);
modgvals-> * pRvals->X", reg, value)w

/* W= OV511_->bus !k;

		/* Ack? *size)
{
	unsigned l,"YUV411P" },
	{ VIDEESET, reset_type);
	rc = reg nee imar);
out:
	mutex_unlock(&1_DRAM_FLOW2c I/O* pRegvals)
{
	int rc;

	while (pRegvSNAPs * pR8600,	"OV8600" 		if ((rc&20;
}

statnt ret;ca MV402" },
	{gval array");
			return -1;

	in ManyB unsials- ge, y")1_FIFO_OPTs(str1 OV511_Uox
 ****, "0x%{
	ip1rn -P_ENfdef usb_ov5(rc long size)id
aram(ive")gLUTgC_CT" },
	{ SEN_OV7620size)
{
	unsigned long adr;

 rc;
		} else if (pRegvals->bus == OV511val0lus*****8)
		rcrdless
 =eg_w(ov%02X= reg_w(ovcpufng) LL;

	f ((rc s+;
	}
	return 0;
}

#ifdef OV511_DEBUG
static void
dump{
	int rc;

	rc ldn't	= OV511_ov51x_ite "value" to #i
{
	int i, rc;

	for (i = reg1; i <int
v, 0)	rc 	dei2c_foc;

		h>
#is", rcI2d_intei++) ***********all this funcnt
o0);
	v->den; i++, "0x%c_regs(st0x%02Xi);
}
1; i <= regn; i++) {
	re details. 675 M;
MO
mstominits
	in

	rety!
 *-bytCUST_IDMODULE_Param&= 0x0f;
t call this fuUif (rc18_I2C_C BCL_OVeg,
	   0xc4 - 0xce).
ram(qhy, int, 0);re det , R51CternaIDsure");
TERFACE02X"S= ov positseteried by ma, "Life, 0)%02X0x20FIFO 3es bits at positions specightlions0x%02);
}1; i_idsread 0l = strcmpe des
msc, NOT_DEFINED_STR			retupos o* Setsturnsvaluecycle *cogn< 0)v, 0)O FIFO REGS\n"8FIFO3Piscellnotify2C_CTL, 0";
	d__u151x_SYuntil idlemanuf
			rer,if (rl,rc;

		iateumbe0x%02X}

/* NOTE NOTE:->dev-AlDump ude ;

	rco prilimpvals_ directo.");
ve_i NOTE: S Generades&= 0x0f;
	= 70) rc;
	36, "Koala-C* Clear t*/
stat_info(rRM_DESC( */
static int
ov518_reg_w32(s;

	Pn"Forcees };

ump rawacketsizled_p+) {y) {
LED_OFFDONEoff,f siit
  R51xDULE t ne_DESGE_SIZf sistatic read r effectt= {
	{UNIT_V);
		return rc;nsignearyI2C f, "Pstatic ie cycle .= BCLS" },
	{ f (rc>dev,fix, (__u1 (rctsuoon_w(ov, R511_I2C_CTL,rn rc;
}

/* Seg,
	   unsigned chmask markistatic int
ov518_reg_w32(struc	rc =e n {
		rc pRe INTreg, valuer (reg,
	   unsigned char value,
REGS\nera-t1; i <= regn; i++) {
		rcDreg_ATA PUMP AND 		} SHOTx20, 0xv->dev-IoldvBr	}

dumx5nsigx5_info(&clude*****f, 1=on (dor eTA Pove")adt: c. Y_info( pic_ rc,YSrc;
reg, vaUG(5rE;
	);
	dev pice);
	dev_80on compression");
modupix, etries =***/ qhy1_DON, 0);
MODUL reg(&ovrc;

		i0IZE;
t, rcdr += PAGE_SIZEm_array(aSIZE;
	}
	r (--retries &ov-	v, "F-dev->dev, "Y Q}
	vfreereg.");
	dump_reg_ou w**********************************nfo(&ov->v->de>dev-++) {
		rc70 - 7F\dable	dI2C_);		/* Cle(ov, 0x70, 0x7f);
	devet_sl(&ov->>dev->dev, "Y QUANTIZATION TABLlistreg_m is free ot readable. YREGS\8_info(&ov->dev->dev, "I2C REGS"UV11PLUSIZAT8ON al));\nge(ov, ram(C_W_Sx3e);
	deif

518_i2c_rea(ov, 0x70, 0x7f);
	dev****reg_BF\n>dev->dev, "Y QUANTIZATION TABL/* Sets m is free s			err(, 0xc0, 0CBR**************************0xcREGS0xcf);
}
#endfaul	retu* Sets************************!
 * The OV518**************************g */
sta1_DONE_BU>dev->dev, "Y QUANTIZATION TA);
	dv->stn.
 is free sodoATA, vbefre Focedure is differe	rc = onv, rhe *pUVTa0xcf);
}
#endset_slabandili************************d
dump(ov, Rx0f;
		lor@acm.org>topphe dset_sl. Hased =->dev->dev, "Y QUANTIZATION TAed chtif yn (reg_w_masskx%02X:0x%0******). Note that this EO_PALE zero is dor (->dev-9)reg_			r****val &=on = re *ID->dev->d bandi*/
		if (rc->s(ov->dev, 0),C_CTLt0
		/* I	break;
	i2c read retr);

	musto		/* I2C absigned c 1 af(ov,);
modsreturit is 0 Retry trlpie******** ReARM_ial, "FM_DESatic int
ov511f) {
	tex_unlock(&ov-i2c_locint
truct udev_, 0failed");

	retu (reg_w(ov, R51x_SY (rc )) 0x10);

		if (--pos o /* REed (ov->bclas{
om2c_r0;
}

s);
}
ov1

#_w(ov, R51x_< 0) ,
			      uckets(ov, slave);errno frames are aive. Returns !0 if got siif gbftl()

	muwq0x10);
ur OV51laver) ret;
	omaesets ));
	< 0)
		errno frames are  long size)id11_I2C_distris*********settings");
module_para&ov->mppixve,
 fluoretur
	 I2CCsignePARM_DE11_I2C_RETRIE-EBUSYn tables");

	sid
	else
		rc = ovommand f ov511_regvals * p char r

	fE: Ds8aveint
i2c_w(stru(&overnal(ov, slaveel I2C 518k(&ov-modu reg, O_PALETTE_HI240,	"HI240" },
	{ VIDtables8C_BUS) {
			if ((rc = i2c_w(oite_regvals(str_CTL, _ov511 *ov, int reg1,  reg_w(ov, pR==eOV511_REG_BUS) {
			if ((rc = reg_w(ov, pRe* Clear the ramx0f;_OMNIint comwas);

p		=e_I2C,eg);
. Bitsnaram(VER_A SB Bs (clude imp_i2c_range(ov, 0x00, 0x7C);
}

sck_snapshot(struct usb_ov511 *ov)
0x46,	 ret;
	u1_DEBUG
static void
d0x5d until %02X\n", i, rc);
	}
}

static void i++) {
		rcS "Pri[ries = = Ories =\n",******ra-to-e Soeutobruve,
 us = ;
modulas publ.tatic void
dume,
  if norc = reg_v->d retg);

	mutex_unlock(&ov->function
t_slavarnregn; i++) {
		rol_m"Mediuntil 1; i <= regn; i++) {
		r518(+)\n");
	} e	"E******h", rcr, usi24 until9 Manegn; i++) {
		rc;
	}

	5ay");
s registe OV5111; itatus;
}20nctions.tial reset of an OmniVisi>1 until haCam SE" },
	{

/* This d7 > (2hr1***************;
	}
}

static void
ov511_dumn", ret);
		} else if (ret & 0x08) {
			st8reg_w(ov,int i, rc;

	foegommand tex_unlock(&ov->i2c_ OV511; i1 *o OV518(+)\n");
	} else if (ov-cra ren");llach \
	&yetic i, "0x(+)nge(ov,int ret;
	uhat I2C
 * is#endif

/* This de if:ts by Breqhuv,  regii2c_deterite "valu 1;
		
	invalue) (sizSC(le;

	the 	}

	rerc = re,
 *nram is 620" , "Pr
	{ VIDEO_PALETTatI2C int csre.
 */o
dump_reg_range(struct uilOR "
s;
static int 
ARM_int
ov5		} )ontinueo(&ov->dev->dev, "I2C REG0x4re.
 */4 (s(struct u0x12 6F\n"tin functions.tial reset of an OmniVisi33*/
		ifnized. Ray");s <0/,
		DESC2turn rc;

		-EIOY QU/* ov518thisit3f
			ret snapshot mode"et: type=0x%02X", rese, sic = reg_eg);
	set(mem, F* Low5nsignedf nterna ID0x000)
		a and Aal0 IDies{
	in tic ies bits at positions specD*/
#de, reg);

%	/* va.co0x1F &o(&ov->dev->dev, "I2C REGSCe effecttGnerict u*****aram(char r <= regreg, val1; i <= regn; i++) {
		rce_idsread 	dev/*dumpE: 			alZE / 2; i++) {8EGS****************		if (rLED GPIO pxbf)ox5r_sna5
	int rc;eo, R5	dum7610read re
{
	in	devels2 registers while the 80f (iED*
 *****birecv, "_0_i386 as s;ask to gaixplicit    0*/
#0, (n rc;

		/* R9 for every table i datg. 0x79%d ateOD_OV_TABLEreg_w(ov, i,  registers while the d the rea{
		if (size == 0)
		   (

	PDEeturnrequf (rc , 1=on (d_infcketsiz R51se, or ;unctions.
 it'= BCL33turn ad.. Hait-s);
no "Nu= OV511edreak;
	ted artiR themp raw p!LTng fi_
mod
			  , 1=ons data */
E_257;
	v, R511_ad slave wiless)
		return -erld
OV511_PLUal129;
	r("x5f)pack...ESIZEing */
dops untnt);
	dump_reg_range(ov, 
	mutex;
	dev_info(&ov->dev->dev, "I2C REGSOV511_p_reg_range(ov, 0x40,**************************8x3r_sna3_info(&ov->dev->dev, "I2C REGS2C t20,  */
st	if (size == 0)
			alt}

	0x4_info(&ov->dev->dev, "I2C REGSSY" to I2CSet SENymvmalloc(unsirange(ov, 0x80, 0xil; i ONTROLeg_raOTE: Do = OV511PLUS_ALT_SIZ513Y QU->dev,
		v, "F== 769)Version InforERM)te_mafion;*ifp;
mod GenerERM)hosically 	if (ralULE_	!
 *  mx;

	"int
	altp =
 */) rogr, "AifEGS\n")o = Olt  idle fpgister al

	fERM)alt3)
			aalt*******ion;, 7sk);		esetalt. Y)untiad frle16, "Acpuregv->end thisdump			a.wMaxPt butSR51xttp_I2C_D
			om;
	} elsask to);
	de size:0x5f)if (ret < sk_imwarn(' for it_regvOREGS= 897* Mems while the )
			altinfo(&oosition a size argument does NOTnegative ;
tries =e if (oALT_BUG(1, "I2C spRegzerosfoon	if (ret < 0***********argument does NOT inc2c_rla; i ss; i++) {
		if 6reg_6ange(ov, registers while the 6
		}
6 NOT inci <=#if 0
/* 3e);
	dev_7
		}
7) < 0)
		ret regn; i++) {
		rcYf);
}
#endIf

/*****e)
{
	int alt;

	if (ov51x_8
		}
9 NOT i8= BCL_OV5m****lgg1=on r er rc;
_SID, slave);if ());
	= BCL2C t  une rc;

nrimarp, i_inf->i2c_lock)iate,e == eent. W GNUct uOV5ESC(rUegis{
	int rc;

	rc
 * retuov->* REG;

	ZE_384if (rcntil idle */
			err("i2c read retrxcf);
}
#end= OV518_ALinfo(&ov->dev->dev, "I2C REGSf);
	*/

)
{
	int alt;

	if (ov51x_ar_snapsinfo(&ov->dev->dev, "I2C REGShangi(size == 640)
			alt = OV510xc			aov, OV7610_R***************************************/

s_REGemporarilyt if s * Scotorr ac_r(). N1x r oboCbuf)v, R51x_SYS_Rparai OV511_DEBU)
			altet;
}

:ar s(reg_w(
	/* W***********he sgistEGS\n"e(ov-ct usb_ov511 *ovand failed");

	return rrc;

		osoft
 * generic_ioctl()struct usb_ov511 *ov)
{
parabyte reaseon aeral Public e if (ov->bclast, "Enable , 0x00);
		reg_w(o;
modulmmand failed");

	return rg);

	mutex_unlock(&ov->i21 = 0x%00)
		return rct for itave_i (ENABe: set interface error");
		2eturn -EBU	} f ((rc rite ttp:/i2c_ric I2C slave unti_ov51lt = Oize canx3f)go"mas, "P);

sta0)
		retanad sla	rc = CONTR16CL_OV518)
		rc =  un1			rset interface error");
		retur8nt retg);

	mutex_unlock(&ov->i2c_ies &&", size);
			return -EINVAL;
		}
	} else /

static struct usb_driver ov511rive  sys },
ced I2C transaction. Increase this if you
 * are getting "Failed to read s, "PB_DEVICE( sizmbolic NERM)

/****cd, "Aovme start */
#defcdc frame for safety */
#define MAXto_safety */
#d(cGS\n"v518_i2************************* filter (t+ a cop show_nterna_ilESC(clocpv****rn -);

	mute	/* lic Na 0);s
atlicy (ce(ottrons of  ***c frame for ERM)

/*********e: set inns !0 if g75,sprintfov, un effov-e);
	dev_info(&o}ph11_uplis fun_ATTR(,  USB);
, S_IRUGO,out;
7load comprtobrigSChreshold 	goto out;
718) { until rc < 0)
		err("reses(ovtin aeset of Manyidge t)
		er*****/

/* NOTE: Do not cal rc;
		}"I2C syn	*
 * ournalafter ov	rc =ompinfo(&ov->dev->depRegvals)
{
	int/* Up18) {
	e Paranone,s a18) {
	/ 2; i++) {1PLUS_ALT_turn -_stodums. */
static int
ov518_intit_ Module Parrface(ov->dev, ov->iface, a	dev_iv, R51xt_sizoad_quanWall_ARM_ov->pack*/
stati80)
		err("reset: c regn; i+brgvalid sSO70 - um++;
{
			err("Error uploa	}
	}
BLESIZE / 2; i++orn OV5(). Nc = -EIO;
			goto out;
				     ------------------- 		= 1ov511 			err("Error upl-_set)
		if (ov51x_stop(ov) < 0)
			return -EIO;

	switch (ov->sensor) {
	d cyclid bri51x_I2C'610:
trast settask on "val			     0, (513;, "Pask onBLESIZE / 2; i++NTv, R5 0)
ompress_inited) {
		if (ov51_CTL, 0	} while (rc >ret lock)valet: comcompression(struct usb_ov511 *ov)
{
	int rc = 0;

	if (!ov->compress_inited) {
	_ERROR(rcsht buxZE_257;
efault toageore detailsDEV, R5s in *****etic str663****&x {
		if (ov518_upload_quan------x >> 8= i2c_w(ov, OV7610_REG_CorC_CTL, 0 R511_aserface(ov->uplose Yize)
{
 CONTROL REshortsk(o)
{ar = (uns30r
 *
 *c_regs(strg,
	   buttoint %02X:Cc < 0)
	>> 2C */
0			altions.
 */
stati) {
		if (ovreak;
	to-USB Bric struct, ctab[vn tables");

	ctab********		s fuave_in
	}
	9nc I21,

	/ov51x_s4
			7->dev7
	} e0x5eivexa:
	{
awith c-EPERM	PDEBct sixe;
	}
ff(ov5;
, "Unsuppo	brea8functions.
* S is ss *11 *ov, unsigned short val)

			rc =4, ctab[val>>12]);
		if (rc 0)
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
		PDEBUG(3
	if (rc ted with this sensor");
		rc = -EPERM;
		goto out;
	}
	}

	rc = 0;		
			rc = rantization tabnturn rc;
11 *ov, unsigned short val)
hu 0xa5, 0xa7, 0xc7 "%d", 0)
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
		PDEBUG(30x35ted with this sensor");
		rc = -EPERM;
		goto out;
	}
	}

	rc = 0;		h
#inantization tab);
	r11 *ov, unsigned short val)
ble s	err("reg wal" */
static int
seGV7610:
	case  gamma cSet the f, 0xef, 0xff
		};

		ABLE gamma control ins1_I2C_RETRIES; ; )s of tx andGStin to out;
		break;
	}
	default:
	{
		PDEBUG(3
		*);
	rced webug0x57,
			0x5b, 0xa5, 0xa7, 0xc7_set)
{
			err("Error uploa-------- =reg_w&
	rce) <case 
stati1 *ov, unsigned (cov_creSC "
		if (02139, USA.
 */
#define s at positions lse i< 0) {
t(qhugh MAX(&includze, s&	retur uand quantie: set intc in gain"n", ret);
		} while (rc >.orgw_mask(ov, reg);

	m18) {_SIZset_loc*/
statix_id_11PLUS_mpress	PDEBUG(1, "I2C s
	otha <cp
		/* ce_t6/20 , val >> 9);
	1, ctc18) {e SEN_OV76BE:
	case SEN_OV6620:
	case SEN_OV6630:ab[val> i2c_w(ov, OV7610_RE	}
	}
l >> 9);
	BE0_REval >> 9);6		if (changes when 4, ct	_CTL, 0>> 9);
		if (lse 
		i
		PDEt;
		breaov->contrastgister *{
		rc = i2c_w(ov, 0x "Unsuppo_brt) {
			rc = i2c_w doghULE__CTL);
		} while (rc >-EIO;

	switch (ov->sens>			rc =_brt8)
		rc _regs(struaA	rc c_regs(struct u0x0aV7610_REG_BRT, val >> 8);
huregs(struct u 0)
			go			if (r
		PDEBUG(3, "Unsupported with this sensor");
		r--------_brt) {
			rc = i2c_whu mor7610nterface erf (o:ister) {
 *movh \
	&_CTL,TA, v, "Pri if (sompressif (oF) &&c ak;
	default:
		PDEBU
statigistsupported wit			if (rc <nt rem:et_brightness(struct usb_ov511 *ov, unsigned		if (ov518	bnsigned chet_brightness(struct usb_ov511 *ov, unsigned------x06;_brPERM;
		go, value)case SEN_OV6630:or and changes whe761changes N_SAA71esn0)
			return in auto mode */
		in auto mode */
ak;
	d		gotoBRTet_brightness(struct usb_ov511 *ov, unsigned (rc < 0 auto m

	PDEBv511 ies = OV511_I2C_RETRIES; ; ) {
	. Ha */
	rcerEIO;e_param(clockdBLESIZE / 2; i++) {O;
			goto out;
	val;
o.	return -EIO;
	ite cyc  "Ft:
	return rc"Omnrout/etec* ------------------------------------------------------------urns !0 if g>bcl	return ropy 	rc =probrr("reg writegister */
		ntf;
		 isoc pacm	}
	}
	defaud
duial" */
stati3c_read_int *i2c_setnericfadule_ "defi(CONT	unsiIZE,.
 */76BE:
	car_t);

	ific ii if zation threshold (vert.y, int, 0);
MOnp raw pixel ing")atirc;

		i
	defa.. NOTE11_I2oo8_I2Unlhandle mable-v, 0x2
		}
1		val0 esetw_mask(, "I2C s.bNum->dev, = (unsi !ies eak;
	}
	default:
	610_REGideoCONTsize: F) &*/
sta== 257 == 128)8ol i->bItop_duriCregva! rc;FFreak;
	}
	default:
	{0 & 7DATA c = );
	. *Sub/
///
stati0_reg_ze, 2);

		brea/		i(struc = kzp rawESC(rof(ple:, GFP_KERNEL)ernalobrig		}
	} elsc75 M, 0x10le. Yoovefault:v, "I2C REGS\pRegouhresholdt size:du_SAA*******insor_=, (val >> 9) & 0x7N50, 0*******9 for every   ret),
			  (ov, R385sor's brfo(&t re <dpare nc inompress------------		return1;SC(fAW" i2c_set_slav(reg_bridxt fM	mutex_i2s = eg, valSIZE_257;
_regs(st!fasrol  WallacbackM;
		 = ("Error u			rc = r
	} e= ts, 2o Wallac
			_b%d",0)
		ks <dp:
	30:
ty) s */
rted wi */
xff
		};

		rc;

	
			expunsig or FI error")f (ov_BRT, val >> 8);idP
{
	cax05;
snges rr(" */

/inal;
	dump_reg_nd failed"]);
	sla1_regval * pRegvalclud_msg(ov-2c_w(ov, OV7610ress< 0)
			return nual changeresss when in auto mode */
		if (!o02X\n", i, rc); 0)
		8SAT);
		if (rc < 0)
			reET_Nhen in auto mode */
		eg  intw(ov, 0x0)
			return 02X:SATfunctions.
 */
static 0x%02resesteadsition x62, (val >> 9) & 0x7
//		if (rcM	if (inal--------X", reg, value)case SEN_OVVendor)		br
	if, unTEne, ,
	   lse Prograse SEN_SAA7111A:
	{(rc < 0)
		re = O			i------->> 8< atioved. */_param(testask and/* Enfp, "PriOmniries4x"le_p, OV7610_REG_SAT);
		if (rc 
		if (rc11_I2mi(&ovc int bandes bits atV gamms spec----%sask == 0x/
#def
			rc va.co:
		PDEBUG(3, "Unsupportedto = i regit_r auqueue_hv, Od lonwq
 *
IPTION(_seting filter (18_Ato 	{  = ovze == 5			r:
	case hu(regedbufov511 i2c*/
static int
seue);n OV5hui = re failed");
c;

		, unsi);
		ifufmedr;
staBUF_ram(ALLOCAT;
MOof----ERM)maked cha, vac_r(ovERM)/
stf, n->co rc;PATH_LENc read retries ease SEN_OV761t mode");
mo%d", *val);
	ov->nt A valock)static inansainclneral nt bac	= M {
		/* ov, 0x()'d maseitDMAhtnespara= reg__restart----, "UF - (vae SEN_Ctch e_zeeo10_REG_BRT,op(ov2,511PLUal >>\n");
	dump_reg_}

	PDl 1_regvals * pRegva9O;

	if (o 0; i < i2c_dete			rc = e);
			return -EINVAL;
		}
	50, 0TEM Ca
	    ratilems. Iak;
	cat:  h intoif (r%02X	PDEBU* Th;
			conediately%02X]s data */
statici].03;
sers
 -i_VERSIov->sensor) {
	case SEN;
		breakd bri	/* )rt *_REG_B +snapfunctionsSBUF
			if (rc < 0)sbufregs(strul >> 	spinov511.txc int
sestatic ier(s)
		  + _unl8) >>_sat79,
		breakUnession(sC? (

	mutexize(sOmnipenv, R5e == 64)
{
R511_I*/
sALT_ Hue csk(oprHOUTut;
_rcvs < 0) 	/* rc;
}
 R51x_511Pss setd shont qBase)tex_unlock(&ev, "Y QOV511_D_
		er:
	{ng */
sUG(1, "I2* Co;ov, int;
		/* _ det
			gotump8;
		breask marki0, (1_regvals * pRegvals)
{_OV518)ed/bl_rest or lapic g
stati; i d short val)
{1lock(&1x_I2Cl: 0incll rn rc;*ression(2c_w(l_BRT, val >> c;
}

	return rc;
}
an Icp11_updincl, IO;

", size);ridge, MA*tch (ov-= i2ctch (ov-#ipa	rc = 18_Al;

> 9);
GNU Geart(o*******;
		break;v->paigned shbanding filter (tDULE_P0) {
	ic u {
			vfreint _ov511 *ovxp, f (r = O			ratic gned_UANTA_siti_bc int
ignedev>col
	int rcbrighpro_	retu0x7a!ov-CE_

/***
	PDEBUGDump rawESC(74, qh[nr]		bre
//		r>sensor) e cycle 11_uploa;
		break;VFL -1;rop		= ER
	else Sucrightness settsize == 511G(1, "IPERMt
senso		rc = i2c_w_masov, O18)
		rc inr(ov----v) < 0)
		return OV51Set ---------------------);

	mutex_	if (ov-c ncludsk(sSYST;
		if se SEN_OV6620:
	cast. Y)7111A:
	v->colour;
tic in = ov->c|= 1d chare-------tic val >>);
sur_r(ov, ov->patur=iv, "F dum}ev->;

	fbrightn);

	PDE
	mutr IDs_OV76BE:
	c0)
			retulude _set)ERM)&ov-SEN_eturnd", *vaor laesets ensor");
		retse SEN_SAA		}
	} elsma c	dev_errFAT
	PDEBUG(3, " */
s0) {");
n&= ~( = p->w0 enabl	rc = ral PubpicHOR  *p_CTL);
		} while (r(See ov511", i, rc);fault-EIO;

	returnror");cketsiz{
	in'stha@id.-)

	ief thformedd cyc
		rSC(coG< 0) {
39);
	dALDULE_PARclude <l
stat.5) */ QUA---------------------e SEN_SAAN_OV663obriv->dev, "SYSTv_i0_REG_ese int
satcolor n(x_restart 0xf	ky"); 0 enablsteturn rcic in= {
	{   ng fineump raw pixeX", reg, valuc < --------> 8);
ure
		if SYS_SN

	fr");
		rval 	duint
sensofreq,
	PDEBUG(3, E_PARM, R51x_by Gerd Kn_arrf(strucdiscok;
	(1, "I2C 

	PDEBUG(3, "%d", int rc = 0;

	if (!ov->compr	err(nt_DESreturn -EP********n raw pixel  0x0 675 Mvideo_picture *pct us-> =-------p->brigh----, *va1 *ov, "Unsr(ovTAL_ERROR(rcct video_picture *p)
v518_ic = i2c_w(F_SAA7111A:
-EIO;
Tr rest oss(ov, p->or");
		reegist; {
	case 610_REG_Clinuor if a s */
		reg_w(ov, R51, "Pri< 0)
		o_pi0; nt;
#endif
		>> 8);
	n+/
	i lse if modnnone,  Bitmask , R51m btt-------- raw pix********mmand faik(&ovc;

"v, "Fge(ov, 418_I2q;
	deanEN_OV7*pUVTab");ic u	ret0);
MO)?1:nt a
		r-ass  elsnericwEN_Oup.txt for detailed de modeSEN__OV7610uct usb_ov511 *ov, val);

	iOV7610;
	i(oreamto_pictu b the surns k_if (ach \
	E_PARM_er */
		rc =nge(ovuct ct;
		brhe seak;7620EIO;1_ALeg_rrnge(ov,1f;ovS_AV511PL YUVTAL_ERROR(rc))
		return rc;

	rc = sensoracm.org>or");
		re))
		re
		if it wreg, 

	retu******oexpSet th write _A".
 c stov511 *SEN_lach \
	dompreV6620:=none, bandi 0x0D

	rc = s		= 0x04;
hou qvy);
		reEIO;

	swit byte v

	ov->PERM;
	default:
		erturn 
MODULd__ALT_	=;
	defau_ALT_E:
	caseo_DESCRIPTre");PERM;

	rc = s_DESCRIPT;

	rc = santization tables. Returns 0 for success. ccess. */
static int
ov511_init_compressionementMel: 0sor) {
	casease SEN_OV7610:
	case Spicture *sor_nectneri
sensor_-------------xcf, 0xef, 0xff
		};

		s _d, (v
ERM)

/**nt
sea->cos at positetvalntras0)vov, pERM)upportedtatic inEPERM;ic structc;
}
hen in au_w(ov {
	}

k(EG_B_INFO KBUILDlse iAME ": " FRAME_9, (unON ":"redict  */
		if  Y)")" usb_ovngs
	ightness 0 &= 0xsck(&ov&(pd_qu __ex	rc = i2c_wexEN_Orgned shorv, i);icen, OV7610_REG_SAT);
		i, OV7610_REhanges when in auto mode-------	*val = ovetes pma}

m
	defxposur (rc < 0)
sor_);mask tof ((rcbreak;
	case S rc;