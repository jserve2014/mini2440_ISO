/*
 *	Z-Star/Vimicro zc301/zc302p/vc30x library
 *	Copyright (C) 2004 2005 2006 Michel Xhaard
 *		mxhaard@magic.fr
 *
 * V4L2 by Jean-Francois Moine <http://moinejf.free.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#define MODULE_NAME "zc3xx"

#include "gspca.h"
#include "jpeg.h"

MODULE_AUTHOR("Michel Xhaard <mxhaard@users.sourceforge.net>, "
		"Serge A. Suchkov <Serge.A.S@tochka.ru>");
MODULE_DESCRIPTION("GSPCA ZC03xx/VC3xx USB Camera Driver");
MODULE_LICENSE("GPL");

static int force_sensor = -1;

#define QUANT_VAL 1		/* quantization table */
#include "zc3xx-reg.h"

/* specific webcam descriptor */
struct sd {
	struct gspca_dev gspca_dev;	/* !! must be the first item */

	__u8 brightness;
	__u8 contrast;
	__u8 gamma;
	__u8 autogain;
	__u8 lightfreq;
	__u8 sharpness;
	u8 quality;			/* image quality */
#define QUALITY_MIN 40
#define QUALITY_MAX 60
#define QUALITY_DEF 50

	signed char sensor;		/* Type of image sensor chip */
/* !! values used in different tables */
#define SENSOR_ADCM2700 0
#define SENSOR_CS2102 1
#define SENSOR_CS2102K 2
#define SENSOR_GC0305 3
#define SENSOR_HDCS2020b 4
#define SENSOR_HV7131B 5
#define SENSOR_HV7131C 6
#define SENSOR_ICM105A 7
#define SENSOR_MC501CB 8
#define SENSOR_OV7620 9
/*#define SENSOR_OV7648 9 - same values */
#define SENSOR_OV7630C 10
#define SENSOR_PAS106 11
#define SENSOR_PAS202B 12
#define SENSOR_PB0330 13
#define SENSOR_PO2030 14
#define SENSOR_TAS5130CK 15
#define SENSOR_TAS5130CXX 16
#define SENSOR_TAS5130C_VF0250 17
#define SENSOR_MAX 18
	unsigned short chip_revision;

	u8 *jpeg_hdr;
};

/* V4L2 controls supported by the driver */
static int sd_setbrightness(struct gspca_dev *gspca_dev, __s32 val);
static int sd_getbrightness(struct gspca_dev *gspca_dev, __s32 *val);
static int sd_setcontrast(struct gspca_dev *gspca_dev, __s32 val);
static int sd_getcontrast(struct gspca_dev *gspca_dev, __s32 *val);
static int sd_setautogain(struct gspca_dev *gspca_dev, __s32 val);
static int sd_getautogain(struct gspca_dev *gspca_dev, __s32 *val);
static int sd_setgamma(struct gspca_dev *gspca_dev, __s32 val);
static int sd_getgamma(struct gspca_dev *gspca_dev, __s32 *val);
static int sd_setfreq(struct gspca_dev *gspca_dev, __s32 val);
static int sd_getfreq(struct gspca_dev *gspca_dev, __s32 *val);
static int sd_setsharpness(struct gspca_dev *gspca_dev, __s32 val);
static int sd_getsharpness(struct gspca_dev *gspca_dev, __s32 *val);

static struct ctrl sd_ctrls[] = {
#define BRIGHTNESS_IDX 0
#define SD_BRIGHTNESS 0
	{
	    {
		.id      = V4L2_CID_BRIGHTNESS,
		.type    = V4L2_CTRL_TYPE_INTEGER,
		.name    = "Brightness",
		.minimum = 0,
		.maximum = 255,
		.step    = 1,
		.default_value = 128,
	    },
	    .set = sd_setbrightness,
	    .get = sd_getbrightness,
	},
#define SD_CONTRAST 1
	{
	    {
		.id      = V4L2_CID_CONTRAST,
		.type    = V4L2_CTRL_TYPE_INTEGER,
		.name    = "Contrast",
		.minimum = 0,
		.maximum = 256,
		.step    = 1,
		.default_value = 128,
	    },
	    .set = sd_setcontrast,
	    .get = sd_getcontrast,
	},
#define SD_GAMMA 2
	{
	    {
		.id      = V4L2_CID_GAMMA,
		.type    = V4L2_CTRL_TYPE_INTEGER,
		.name    = "Gamma",
		.minimum = 1,
		.maximum = 6,
		.step    = 1,
		.default_value = 4,
	    },
	    .set = sd_setgamma,
	    .get = sd_getgamma,
	},
#define SD_AUTOGAIN 3
	{
	    {
		.id      = V4L2_CID_AUTOGAIN,
		.type    = V4L2_CTRL_TYPE_BOOLEAN,
		.name    = "Auto Gain",
		.minimum = 0,
		.maximum = 1,
		.step    = 1,
		.default_value = 1,
	    },
	    .set = sd_setautogain,
	    .get = sd_getautogain,
	},
#define LIGHTFREQ_IDX 4
#define SD_FREQ 4
	{
	    {
		.id	 = V4L2_CID_POWER_LINE_FREQUENCY,
		.type    = V4L2_CTRL_TYPE_MENU,
		.name    = "Light frequency filter",
		.minimum = 0,
		.maximum = 2,	/* 0: 0, 1: 50Hz, 2:60Hz */
		.step    = 1,
		.default_value = 1,
	    },
	    .set = sd_setfreq,
	    .get = sd_getfreq,
	},
#define SD_SHARPNESS 5
	{
	    {
		.id	 = V4L2_CID_SHARPNESS,
		.type    = V4L2_CTRL_TYPE_INTEGER,
		.name    = "Sharpness",
		.minimum = 0,
		.maximum = 3,
		.step    = 1,
		.default_value = 2,
	    },
	    .set = sd_setsharpness,
	    .get = sd_getsharpness,
	},
};

static const struct v4l2_pix_format vga_mode[] = {
	{320, 240, V4L2_PIX_FMT_JPEG, V4L2_FIELD_NONE,
		.bytesperline = 320,
		.sizeimage = 320 * 240 * 3 / 8 + 590,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.priv = 1},
	{640, 480, V4L2_PIX_FMT_JPEG, V4L2_FIELD_NONE,
		.bytesperline = 640,
		.sizeimage = 640 * 480 * 3 / 8 + 590,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.priv = 0},
};

static const struct v4l2_pix_format sif_mode[] = {
	{176, 144, V4L2_PIX_FMT_JPEG, V4L2_FIELD_NONE,
		.bytesperline = 176,
		.sizeimage = 176 * 144 * 3 / 8 + 590,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.priv = 1},
	{352, 288, V4L2_PIX_FMT_JPEG, V4L2_FIELD_NONE,
		.bytesperline = 352,
		.sizeimage = 352 * 288 * 3 / 8 + 590,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.priv = 0},
};

/* usb exchanges */
struct usb_action {
	__u8	req;
	__u8	val;
	__u16	idx;
};

static const struct usb_action adcm2700_Initial[] = {
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},		/* 00,00,01,cc */
	{0xa0, 0x04, ZC3XX_R002_CLOCKSELECT},		/* 00,02,04,cc */
	{0xa0, 0x00, ZC3XX_R008_CLOCKSETTING},		/* 00,08,03,cc */
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xa0, 0xd3, ZC3XX_R08B_I2CDEVICEADDR},		/* 00,8b,d3,cc */
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},	/* 00,03,02,cc */
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},		/* 00,04,80,cc */
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},	/* 00,05,01,cc */
	{0xa0, 0xd8, ZC3XX_R006_FRAMEHEIGHTLOW},	/* 00,06,d8,cc */
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},	/* 00,01,01,cc */
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,03,cc */
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,01,cc */
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,05,cc */
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},		/* 00,98,00,cc */
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},		/* 00,9a,00,cc */
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},		/* 01,1a,00,cc */
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},		/* 01,1c,00,cc */
	{0xa0, 0xde, ZC3XX_R09C_WINHEIGHTLOW},		/* 00,9c,de,cc */
	{0xa0, 0x86, ZC3XX_R09E_WINWIDTHLOW},		/* 00,9e,86,cc */
	{0xbb, 0x00, 0x0400},				/* 04,00,00,bb */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xbb, 0x0f, 0x140f},				/* 14,0f,0f,bb */
	{0xa0, 0xb7, ZC3XX_R101_SENSORCORRECTION},	/* 01,01,37,cc */
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},		/* 01,00,0d,cc */
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},		/* 01,89,06,cc */
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},		/* 01,c5,03,cc */
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},		/* 01,cb,13,cc */
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},	/* 02,50,08,cc */
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},		/* 03,01,08,cc */
	{0xa0, 0x58, ZC3XX_R116_RGAIN},			/* 01,16,58,cc */
	{0xa0, 0x5a, ZC3XX_R118_BGAIN},			/* 01,18,5a,cc */
	{0xa0, 0x02, ZC3XX_R180_AUTOCORRECTENABLE},	/* 01,80,02,cc */
	{0xa0, 0xd3, ZC3XX_R08B_I2CDEVICEADDR},		/* 00,8b,d3,cc */
	{0xbb, 0x00, 0x0408},				/* 04,00,08,bb */
	{0xdd, 0x00, 0x0200},				/* 00,02,00,dd */
	{0xbb, 0x00, 0x0400},				/* 04,00,00,bb */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xbb, 0x0f, 0x140f},				/* 14,0f,0f,bb */
	{0xbb, 0xe0, 0x0c2e},				/* 0c,e0,2e,bb */
	{0xbb, 0x01, 0x2000},				/* 20,01,00,bb */
	{0xbb, 0x96, 0x2400},				/* 24,96,00,bb */
	{0xbb, 0x06, 0x1006},				/* 10,06,06,bb */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xaa, 0xfe, 0x0002},				/* 00,fe,02,aa */
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xbb, 0x5f, 0x2090},				/* 20,5f,90,bb */
	{0xbb, 0x01, 0x8000},				/* 80,01,00,bb */
	{0xbb, 0x09, 0x8400},				/* 84,09,00,bb */
	{0xbb, 0x86, 0x0002},				/* 00,86,02,bb */
	{0xbb, 0xe6, 0x0401},				/* 04,e6,01,bb */
	{0xbb, 0x86, 0x0802},				/* 08,86,02,bb */
	{0xbb, 0xe6, 0x0c01},				/* 0c,e6,01,bb */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xaa, 0xfe, 0x0000},				/* 00,fe,00,aa */
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xaa, 0xfe, 0x0020},				/* 00,fe,20,aa */
/*mswin+*/
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},
	{0xaa, 0xfe, 0x0002},
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},
	{0xaa, 0xb4, 0xcd37},
	{0xaa, 0xa4, 0x0004},
	{0xaa, 0xa8, 0x0007},
	{0xaa, 0xac, 0x0004},
/*mswin-*/
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xaa, 0xfe, 0x0000},				/* 00,fe,00,aa */
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xbb, 0x04, 0x0400},				/* 04,04,00,bb */
	{0xdd, 0x00, 0x0100},				/* 00,01,00,dd */
	{0xbb, 0x01, 0x0400},				/* 04,01,00,bb */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xaa, 0xfe, 0x0002},				/* 00,fe,02,aa */
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xbb, 0x41, 0x2803},				/* 28,41,03,bb */
	{0xbb, 0x40, 0x2c03},				/* 2c,40,03,bb */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xaa, 0xfe, 0x0010},				/* 00,fe,10,aa */
	{}
};
static const struct usb_action adcm2700_InitialScale[] = {
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},		/* 00,00,01,cc */
	{0xa0, 0x10, ZC3XX_R002_CLOCKSELECT},		/* 00,02,10,cc */
	{0xa0, 0x00, ZC3XX_R008_CLOCKSETTING},		/* 00,08,03,cc */
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xa0, 0xd3, ZC3XX_R08B_I2CDEVICEADDR},		/* 00,8b,d3,cc */
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},	/* 00,03,02,cc */
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},		/* 00,04,80,cc */
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},	/* 00,05,01,cc */
	{0xa0, 0xd0, ZC3XX_R006_FRAMEHEIGHTLOW},	/* 00,06,d0,cc */
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},	/* 00,01,01,cc */
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,03,cc */
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,01,cc */
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,05,cc */
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},		/* 00,98,00,cc */
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},		/* 00,9a,00,cc */
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},		/* 01,1a,00,cc */
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},		/* 01,1c,00,cc */
	{0xa0, 0xd8, ZC3XX_R09C_WINHEIGHTLOW},		/* 00,9c,d8,cc */
	{0xa0, 0x88, ZC3XX_R09E_WINWIDTHLOW},		/* 00,9e,88,cc */
	{0xbb, 0x00, 0x0400},				/* 04,00,00,bb */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xbb, 0x0f, 0x140f},				/* 14,0f,0f,bb */
	{0xa0, 0xb7, ZC3XX_R101_SENSORCORRECTION},	/* 01,01,37,cc */
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},		/* 01,00,0d,cc */
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},		/* 01,89,06,cc */
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},		/* 01,c5,03,cc */
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},		/* 01,cb,13,cc */
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},	/* 02,50,08,cc */
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},		/* 03,01,08,cc */
	{0xa0, 0x58, ZC3XX_R116_RGAIN},			/* 01,16,58,cc */
	{0xa0, 0x5a, ZC3XX_R118_BGAIN},			/* 01,18,5a,cc */
	{0xa0, 0x02, ZC3XX_R180_AUTOCORRECTENABLE},	/* 01,80,02,cc */
	{0xa0, 0xd3, ZC3XX_R08B_I2CDEVICEADDR},		/* 00,8b,d3,cc */
	{0xbb, 0x00, 0x0408},				/* 04,00,08,bb */
	{0xdd, 0x00, 0x0200},				/* 00,02,00,dd */
	{0xbb, 0x00, 0x0400},				/* 04,00,00,bb */
	{0xdd, 0x00, 0x0050},				/* 00,00,50,dd */
	{0xbb, 0x0f, 0x140f},				/* 14,0f,0f,bb */
	{0xbb, 0xe0, 0x0c2e},				/* 0c,e0,2e,bb */
	{0xbb, 0x01, 0x2000},				/* 20,01,00,bb */
	{0xbb, 0x96, 0x2400},				/* 24,96,00,bb */
	{0xbb, 0x06, 0x1006},				/* 10,06,06,bb */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xaa, 0xfe, 0x0002},				/* 00,fe,02,aa */
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xbb, 0x5f, 0x2090},				/* 20,5f,90,bb */
	{0xbb, 0x01, 0x8000},				/* 80,01,00,bb */
	{0xbb, 0x09, 0x8400},				/* 84,09,00,bb */
	{0xbb, 0x86, 0x0002},				/* 00,88,02,bb */
	{0xbb, 0xe6, 0x0401},				/* 04,e6,01,bb */
	{0xbb, 0x86, 0x0802},				/* 08,88,02,bb */
	{0xbb, 0xe6, 0x0c01},				/* 0c,e6,01,bb */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xaa, 0xfe, 0x0000},				/* 00,fe,00,aa */
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xaa, 0xfe, 0x0020},				/* 00,fe,20,aa */
	/*******/
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xaa, 0xfe, 0x0000},				/* 00,fe,00,aa */
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xbb, 0x04, 0x0400},				/* 04,04,00,bb */
	{0xdd, 0x00, 0x0100},				/* 00,01,00,dd */
	{0xbb, 0x01, 0x0400},				/* 04,01,00,bb */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xaa, 0xfe, 0x0002},				/* 00,fe,02,aa */
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xbb, 0x41, 0x2803},				/* 28,41,03,bb */
	{0xbb, 0x40, 0x2c03},				/* 2c,40,03,bb */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xaa, 0xfe, 0x0010},				/* 00,fe,10,aa */
	{}
};
static const struct usb_action adcm2700_50HZ[] = {
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xaa, 0xfe, 0x0002},				/* 00,fe,02,aa */
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xbb, 0x05, 0x8400},				/* 84,05,00,bb */
	{0xbb, 0xd0, 0xb007},				/* b0,d0,07,bb */
	{0xbb, 0xa0, 0xb80f},				/* b8,a0,0f,bb */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xaa, 0xfe, 0x0010},				/* 00,fe,10,aa */
	{0xaa, 0x26, 0x00d0},				/* 00,26,d0,aa */
	{0xaa, 0x28, 0x0002},				/* 00,28,02,aa */
	{}
};
static const struct usb_action adcm2700_60HZ[] = {
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xaa, 0xfe, 0x0002},				/* 00,fe,02,aa */
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xbb, 0x07, 0x8400},				/* 84,07,00,bb */
	{0xbb, 0x82, 0xb006},				/* b0,82,06,bb */
	{0xbb, 0x04, 0xb80d},				/* b8,04,0d,bb */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xaa, 0xfe, 0x0010},				/* 00,fe,10,aa */
	{0xaa, 0x26, 0x0057},				/* 00,26,57,aa */
	{0xaa, 0x28, 0x0002},				/* 00,28,02,aa */
	{}
};
static const struct usb_action adcm2700_NoFliker[] = {
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xaa, 0xfe, 0x0002},				/* 00,fe,02,aa */
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xbb, 0x07, 0x8400},				/* 84,07,00,bb */
	{0xbb, 0x05, 0xb000},				/* b0,05,00,bb */
	{0xbb, 0xa0, 0xb801},				/* b8,a0,01,bb */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xaa, 0xfe, 0x0010},				/* 00,fe,10,aa */
	{}
};
static const struct usb_action cs2102_Initial[] = {
	{0xa1, 0x01, 0x0008},
	{0xa1, 0x01, 0x0008},
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},
	{0xa0, 0x10, ZC3XX_R002_CLOCKSELECT},
	{0xa0, 0x00, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x20, ZC3XX_R080_HBLANKHIGH},
	{0xa0, 0x21, ZC3XX_R081_HBLANKLOW},
	{0xa0, 0x30, ZC3XX_R083_RGAINADDR},
	{0xa0, 0x31, ZC3XX_R084_GGAINADDR},
	{0xa0, 0x32, ZC3XX_R085_BGAINADDR},
	{0xa0, 0x23, ZC3XX_R086_EXPTIMEHIGH},
	{0xa0, 0x24, ZC3XX_R087_EXPTIMEMID},
	{0xa0, 0x25, ZC3XX_R088_EXPTIMELOW},
	{0xa0, 0xb3, ZC3XX_R08B_I2CDEVICEADDR},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* 00 */
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},
	{0xaa, 0x02, 0x0008},
	{0xaa, 0x03, 0x0000},
	{0xaa, 0x11, 0x0000},
	{0xaa, 0x12, 0x0089},
	{0xaa, 0x13, 0x0000},
	{0xaa, 0x14, 0x00e9},
	{0xaa, 0x20, 0x0000},
	{0xaa, 0x22, 0x0000},
	{0xaa, 0x0b, 0x0004},
	{0xaa, 0x30, 0x0030},
	{0xaa, 0x31, 0x0030},
	{0xaa, 0x32, 0x0030},
	{0xa0, 0x37, ZC3XX_R101_SENSORCORRECTION},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x10, 0x01ae},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xa0, 0x68, ZC3XX_R18D_YTARGET},
	{0xa0, 0x00, 0x01ad},
	{0xa1, 0x01, 0x0002},
	{0xa1, 0x01, 0x0008},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* 00 */
	{0xa0, 0x08, ZC3XX_R1C6_SHARPNESS00},	/* sharpness+ */
	{0xa1, 0x01, 0x01c8},
	{0xa1, 0x01, 0x01c9},
	{0xa1, 0x01, 0x01ca},
	{0xa0, 0x0f, ZC3XX_R1CB_SHARPNESS05},	/* sharpness- */
	{0xa0, 0x24, ZC3XX_R120_GAMMA00},	/* gamma 5 */
	{0xa0, 0x44, ZC3XX_R121_GAMMA01},
	{0xa0, 0x64, ZC3XX_R122_GAMMA02},
	{0xa0, 0x84, ZC3XX_R123_GAMMA03},
	{0xa0, 0x9d, ZC3XX_R124_GAMMA04},
	{0xa0, 0xb2, ZC3XX_R125_GAMMA05},
	{0xa0, 0xc4, ZC3XX_R126_GAMMA06},
	{0xa0, 0xd3, ZC3XX_R127_GAMMA07},
	{0xa0, 0xe0, ZC3XX_R128_GAMMA08},
	{0xa0, 0xeb, ZC3XX_R129_GAMMA09},
	{0xa0, 0xf4, ZC3XX_R12A_GAMMA0A},
	{0xa0, 0xfb, ZC3XX_R12B_GAMMA0B},
	{0xa0, 0xff, ZC3XX_R12C_GAMMA0C},
	{0xa0, 0xff, ZC3XX_R12D_GAMMA0D},
	{0xa0, 0xff, ZC3XX_R12E_GAMMA0E},
	{0xa0, 0xff, ZC3XX_R12F_GAMMA0F},
	{0xa0, 0x18, ZC3XX_R130_GAMMA10},
	{0xa0, 0x20, ZC3XX_R131_GAMMA11},
	{0xa0, 0x20, ZC3XX_R132_GAMMA12},
	{0xa0, 0x1c, ZC3XX_R133_GAMMA13},
	{0xa0, 0x16, ZC3XX_R134_GAMMA14},
	{0xa0, 0x13, ZC3XX_R135_GAMMA15},
	{0xa0, 0x10, ZC3XX_R136_GAMMA16},
	{0xa0, 0x0e, ZC3XX_R137_GAMMA17},
	{0xa0, 0x0b, ZC3XX_R138_GAMMA18},
	{0xa0, 0x09, ZC3XX_R139_GAMMA19},
	{0xa0, 0x07, ZC3XX_R13A_GAMMA1A},
	{0xa0, 0x06, ZC3XX_R13B_GAMMA1B},
	{0xa0, 0x00, ZC3XX_R13C_GAMMA1C},
	{0xa0, 0x00, ZC3XX_R13D_GAMMA1D},
	{0xa0, 0x00, ZC3XX_R13E_GAMMA1E},
	{0xa0, 0x01, ZC3XX_R13F_GAMMA1F},
	{0xa0, 0x58, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xf4, ZC3XX_R10B_RGB01},
	{0xa0, 0xf4, ZC3XX_R10C_RGB02},
	{0xa0, 0xf4, ZC3XX_R10D_RGB10},
	{0xa0, 0x58, ZC3XX_R10E_RGB11},
	{0xa0, 0xf4, ZC3XX_R10F_RGB12},
	{0xa0, 0xf4, ZC3XX_R110_RGB20},
	{0xa0, 0xf4, ZC3XX_R111_RGB21},
	{0xa0, 0x58, ZC3XX_R112_RGB22},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x00, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0xaa, 0x23, 0x0001},
	{0xaa, 0x24, 0x0055},
	{0xaa, 0x25, 0x00cc},
	{0xaa, 0x21, 0x003f},
	{0xa0, 0x02, ZC3XX_R190_EXPOSURELIMITHIGH},
	{0xa0, 0xab, ZC3XX_R191_EXPOSURELIMITMID},
	{0xa0, 0x98, ZC3XX_R192_EXPOSURELIMITLOW},
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0x30, ZC3XX_R196_ANTIFLICKERMID},
	{0xa0, 0xd4, ZC3XX_R197_ANTIFLICKERLOW},
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF},
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP},
	{0xa0, 0x39, ZC3XX_R01D_HSYNC_0},
	{0xa0, 0x70, ZC3XX_R01E_HSYNC_1},
	{0xa0, 0xb0, ZC3XX_R01F_HSYNC_2},
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3},
	{0xa0, 0x40, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x40, ZC3XX_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GGAIN},
	{0xa0, 0x40, ZC3XX_R118_BGAIN},
	{}
};

static const struct usb_action cs2102_InitialScale[] = {
	{0xa1, 0x01, 0x0008},
	{0xa1, 0x01, 0x0008},
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},
	{0xa0, 0x00, ZC3XX_R002_CLOCKSELECT},
	{0xa0, 0x00, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x20, ZC3XX_R080_HBLANKHIGH},
	{0xa0, 0x21, ZC3XX_R081_HBLANKLOW},
	{0xa0, 0x30, ZC3XX_R083_RGAINADDR},
	{0xa0, 0x31, ZC3XX_R084_GGAINADDR},
	{0xa0, 0x32, ZC3XX_R085_BGAINADDR},
	{0xa0, 0x23, ZC3XX_R086_EXPTIMEHIGH},
	{0xa0, 0x24, ZC3XX_R087_EXPTIMEMID},
	{0xa0, 0x25, ZC3XX_R088_EXPTIMELOW},
	{0xa0, 0xb3, ZC3XX_R08B_I2CDEVICEADDR},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* 00 */
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},
	{0xaa, 0x02, 0x0008},
	{0xaa, 0x03, 0x0000},
	{0xaa, 0x11, 0x0001},
	{0xaa, 0x12, 0x0087},
	{0xaa, 0x13, 0x0001},
	{0xaa, 0x14, 0x00e7},
	{0xaa, 0x20, 0x0000},
	{0xaa, 0x22, 0x0000},
	{0xaa, 0x0b, 0x0004},
	{0xaa, 0x30, 0x0030},
	{0xaa, 0x31, 0x0030},
	{0xaa, 0x32, 0x0030},
	{0xa0, 0x77, ZC3XX_R101_SENSORCORRECTION},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x15, 0x01ae},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xa0, 0x68, ZC3XX_R18D_YTARGET},
	{0xa0, 0x00, 0x01ad},
	{0xa1, 0x01, 0x0002},
	{0xa1, 0x01, 0x0008},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* 00 */
	{0xa0, 0x08, ZC3XX_R1C6_SHARPNESS00},	/* sharpness+ */
	{0xa1, 0x01, 0x01c8},
	{0xa1, 0x01, 0x01c9},
	{0xa1, 0x01, 0x01ca},
	{0xa0, 0x0f, ZC3XX_R1CB_SHARPNESS05},	/* sharpness- */
	{0xa0, 0x24, ZC3XX_R120_GAMMA00},	/* gamma 5 */
	{0xa0, 0x44, ZC3XX_R121_GAMMA01},
	{0xa0, 0x64, ZC3XX_R122_GAMMA02},
	{0xa0, 0x84, ZC3XX_R123_GAMMA03},
	{0xa0, 0x9d, ZC3XX_R124_GAMMA04},
	{0xa0, 0xb2, ZC3XX_R125_GAMMA05},
	{0xa0, 0xc4, ZC3XX_R126_GAMMA06},
	{0xa0, 0xd3, ZC3XX_R127_GAMMA07},
	{0xa0, 0xe0, ZC3XX_R128_GAMMA08},
	{0xa0, 0xeb, ZC3XX_R129_GAMMA09},
	{0xa0, 0xf4, ZC3XX_R12A_GAMMA0A},
	{0xa0, 0xfb, ZC3XX_R12B_GAMMA0B},
	{0xa0, 0xff, ZC3XX_R12C_GAMMA0C},
	{0xa0, 0xff, ZC3XX_R12D_GAMMA0D},
	{0xa0, 0xff, ZC3XX_R12E_GAMMA0E},
	{0xa0, 0xff, ZC3XX_R12F_GAMMA0F},
	{0xa0, 0x18, ZC3XX_R130_GAMMA10},
	{0xa0, 0x20, ZC3XX_R131_GAMMA11},
	{0xa0, 0x20, ZC3XX_R132_GAMMA12},
	{0xa0, 0x1c, ZC3XX_R133_GAMMA13},
	{0xa0, 0x16, ZC3XX_R134_GAMMA14},
	{0xa0, 0x13, ZC3XX_R135_GAMMA15},
	{0xa0, 0x10, ZC3XX_R136_GAMMA16},
	{0xa0, 0x0e, ZC3XX_R137_GAMMA17},
	{0xa0, 0x0b, ZC3XX_R138_GAMMA18},
	{0xa0, 0x09, ZC3XX_R139_GAMMA19},
	{0xa0, 0x07, ZC3XX_R13A_GAMMA1A},
	{0xa0, 0x06, ZC3XX_R13B_GAMMA1B},
	{0xa0, 0x00, ZC3XX_R13C_GAMMA1C},
	{0xa0, 0x00, ZC3XX_R13D_GAMMA1D},
	{0xa0, 0x00, ZC3XX_R13E_GAMMA1E},
	{0xa0, 0x01, ZC3XX_R13F_GAMMA1F},
	{0xa0, 0x58, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xf4, ZC3XX_R10B_RGB01},
	{0xa0, 0xf4, ZC3XX_R10C_RGB02},
	{0xa0, 0xf4, ZC3XX_R10D_RGB10},
	{0xa0, 0x58, ZC3XX_R10E_RGB11},
	{0xa0, 0xf4, ZC3XX_R10F_RGB12},
	{0xa0, 0xf4, ZC3XX_R110_RGB20},
	{0xa0, 0xf4, ZC3XX_R111_RGB21},
	{0xa0, 0x58, ZC3XX_R112_RGB22},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x00, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0xaa, 0x23, 0x0000},
	{0xaa, 0x24, 0x00aa},
	{0xaa, 0x25, 0x00e6},
	{0xaa, 0x21, 0x003f},
	{0xa0, 0x01, ZC3XX_R190_EXPOSURELIMITHIGH},
	{0xa0, 0x55, ZC3XX_R191_EXPOSURELIMITMID},
	{0xa0, 0xcc, ZC3XX_R192_EXPOSURELIMITLOW},
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0x18, ZC3XX_R196_ANTIFLICKERMID},
	{0xa0, 0x6a, ZC3XX_R197_ANTIFLICKERLOW},
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF},
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP},
	{0xa0, 0x3f, ZC3XX_R01D_HSYNC_0},
	{0xa0, 0xa5, ZC3XX_R01E_HSYNC_1},
	{0xa0, 0xf0, ZC3XX_R01F_HSYNC_2},
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3},
	{0xa0, 0x40, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x40, ZC3XX_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GGAIN},
	{0xa0, 0x40, ZC3XX_R118_BGAIN},
	{}
};
static const struct usb_action cs2102_50HZ[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xaa, 0x0f, 0x008c}, /* 00,0f,8c,aa */
	{0xaa, 0x03, 0x0005}, /* 00,03,05,aa */
	{0xaa, 0x04, 0x00ac}, /* 00,04,ac,aa */
	{0xaa, 0x10, 0x0005}, /* 00,10,05,aa */
	{0xaa, 0x11, 0x00ac}, /* 00,11,ac,aa */
	{0xaa, 0x1b, 0x0000}, /* 00,1b,00,aa */
	{0xaa, 0x1c, 0x0005}, /* 00,1c,05,aa */
	{0xaa, 0x1d, 0x00ac}, /* 00,1d,ac,aa */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x3f, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,3f,cc */
	{0xa0, 0xf0, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,f0,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x42, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,42,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,cc */
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,10,cc */
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,24,cc */
	{0xa0, 0x8c, ZC3XX_R01D_HSYNC_0}, /* 00,1d,8c,cc */
	{0xa0, 0xb0, ZC3XX_R01E_HSYNC_1}, /* 00,1e,b0,cc */
	{0xa0, 0xd0, ZC3XX_R01F_HSYNC_2}, /* 00,1f,d0,cc */
	{}
};
static const struct usb_action cs2102_50HZScale[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xaa, 0x0f, 0x0093}, /* 00,0f,93,aa */
	{0xaa, 0x03, 0x0005}, /* 00,03,05,aa */
	{0xaa, 0x04, 0x00a1}, /* 00,04,a1,aa */
	{0xaa, 0x10, 0x0005}, /* 00,10,05,aa */
	{0xaa, 0x11, 0x00a1}, /* 00,11,a1,aa */
	{0xaa, 0x1b, 0x0000}, /* 00,1b,00,aa */
	{0xaa, 0x1c, 0x0005}, /* 00,1c,05,aa */
	{0xaa, 0x1d, 0x00a1}, /* 00,1d,a1,aa */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x3f, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,3f,cc */
	{0xa0, 0xf7, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,f7,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x83, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,83,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,cc */
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,10,cc */
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,24,cc */
	{0xa0, 0x93, ZC3XX_R01D_HSYNC_0}, /* 00,1d,93,cc */
	{0xa0, 0xb0, ZC3XX_R01E_HSYNC_1}, /* 00,1e,b0,cc */
	{0xa0, 0xd0, ZC3XX_R01F_HSYNC_2}, /* 00,1f,d0,cc */
	{}
};
static const struct usb_action cs2102_60HZ[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xaa, 0x0f, 0x005d}, /* 00,0f,5d,aa */
	{0xaa, 0x03, 0x0005}, /* 00,03,05,aa */
	{0xaa, 0x04, 0x00aa}, /* 00,04,aa,aa */
	{0xaa, 0x10, 0x0005}, /* 00,10,05,aa */
	{0xaa, 0x11, 0x00aa}, /* 00,11,aa,aa */
	{0xaa, 0x1b, 0x0000}, /* 00,1b,00,aa */
	{0xaa, 0x1c, 0x0005}, /* 00,1c,05,aa */
	{0xaa, 0x1d, 0x00aa}, /* 00,1d,aa,aa */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x3f, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,3f,cc */
	{0xa0, 0xe4, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,e4,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x3a, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,3a,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,cc */
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,10,cc */
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,24,cc */
	{0xa0, 0x5d, ZC3XX_R01D_HSYNC_0}, /* 00,1d,5d,cc */
	{0xa0, 0x90, ZC3XX_R01E_HSYNC_1}, /* 00,1e,90,cc */
	{0xa0, 0xd0, 0x00c8}, /* 00,c8,d0,cc */
	{}
};
static const struct usb_action cs2102_60HZScale[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xaa, 0x0f, 0x00b7}, /* 00,0f,b7,aa */
	{0xaa, 0x03, 0x0005}, /* 00,03,05,aa */
	{0xaa, 0x04, 0x00be}, /* 00,04,be,aa */
	{0xaa, 0x10, 0x0005}, /* 00,10,05,aa */
	{0xaa, 0x11, 0x00be}, /* 00,11,be,aa */
	{0xaa, 0x1b, 0x0000}, /* 00,1b,00,aa */
	{0xaa, 0x1c, 0x0005}, /* 00,1c,05,aa */
	{0xaa, 0x1d, 0x00be}, /* 00,1d,be,aa */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x3f, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,3f,cc */
	{0xa0, 0xfc, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,fc,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x69, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,69,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,cc */
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,10,cc */
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,24,cc */
	{0xa0, 0xb7, ZC3XX_R01D_HSYNC_0}, /* 00,1d,b7,cc */
	{0xa0, 0xd0, ZC3XX_R01E_HSYNC_1}, /* 00,1e,d0,cc */
	{0xa0, 0xe8, ZC3XX_R01F_HSYNC_2}, /* 00,1f,e8,cc */
	{}
};
static const struct usb_action cs2102_NoFliker[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xaa, 0x0f, 0x0059}, /* 00,0f,59,aa */
	{0xaa, 0x03, 0x0005}, /* 00,03,05,aa */
	{0xaa, 0x04, 0x0080}, /* 00,04,80,aa */
	{0xaa, 0x10, 0x0005}, /* 00,10,05,aa */
	{0xaa, 0x11, 0x0080}, /* 00,11,80,aa */
	{0xaa, 0x1b, 0x0000}, /* 00,1b,00,aa */
	{0xaa, 0x1c, 0x0005}, /* 00,1c,05,aa */
	{0xaa, 0x1d, 0x0080}, /* 00,1d,80,aa */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x3f, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,3f,cc */
	{0xa0, 0xf0, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,f0,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x10, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,10,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,cc */
	{0xa0, 0x00, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,00,cc */
	{0xa0, 0x00, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,00,cc */
	{0xa0, 0x59, ZC3XX_R01D_HSYNC_0}, /* 00,1d,59,cc */
	{0xa0, 0x90, ZC3XX_R01E_HSYNC_1}, /* 00,1e,90,cc */
	{0xa0, 0xc8, ZC3XX_R01F_HSYNC_2}, /* 00,1f,c8,cc */
	{}
};
static const struct usb_action cs2102_NoFlikerScale[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xaa, 0x0f, 0x0059}, /* 00,0f,59,aa */
	{0xaa, 0x03, 0x0005}, /* 00,03,05,aa */
	{0xaa, 0x04, 0x0080}, /* 00,04,80,aa */
	{0xaa, 0x10, 0x0005}, /* 00,10,05,aa */
	{0xaa, 0x11, 0x0080}, /* 00,11,80,aa */
	{0xaa, 0x1b, 0x0000}, /* 00,1b,00,aa */
	{0xaa, 0x1c, 0x0005}, /* 00,1c,05,aa */
	{0xaa, 0x1d, 0x0080}, /* 00,1d,80,aa */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x3f, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,3f,cc */
	{0xa0, 0xf0, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,f0,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x10, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,10,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,cc */
	{0xa0, 0x00, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,00,cc */
	{0xa0, 0x00, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,00,cc */
	{0xa0, 0x59, ZC3XX_R01D_HSYNC_0}, /* 00,1d,59,cc */
	{0xa0, 0x90, ZC3XX_R01E_HSYNC_1}, /* 00,1e,90,cc */
	{0xa0, 0xc8, ZC3XX_R01F_HSYNC_2}, /* 00,1f,c8,cc */
	{}
};

/* CS2102_KOCOM */
static const struct usb_action cs2102K_Initial[] = {
	{0xa0, 0x11, ZC3XX_R002_CLOCKSELECT},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},
	{0xa0, 0x08, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},
	{0xa0, 0xe8, ZC3XX_R09C_WINHEIGHTLOW},
	{0xa0, 0x88, ZC3XX_R09E_WINWIDTHLOW},
	{0xa0, 0x55, ZC3XX_R08B_I2CDEVICEADDR},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x0a, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x02, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x0b, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x02, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x0c, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x7c, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x0d, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0xa3, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x03, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0xfb, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x05, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x06, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x03, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x09, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x08, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x0e, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x0f, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x18, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x10, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x18, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x11, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x18, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x12, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x18, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x15, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x16, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x0c, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x17, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x0c, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0xb7, ZC3XX_R101_SENSORCORRECTION},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x78, ZC3XX_R18D_YTARGET},
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x20, ZC3XX_R087_EXPTIMEMID},
	{0xa0, 0x21, ZC3XX_R088_EXPTIMELOW},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xa0, 0x00, 0x01ad},
	{0xa0, 0x01, 0x01b1},
	{0xa0, 0x02, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x60, ZC3XX_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GGAIN},
	{0xa0, 0x4c, ZC3XX_R118_BGAIN},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* clock ? */
	{0xa0, 0x08, ZC3XX_R1C6_SHARPNESS00},	/* sharpness+ */
	{0xa0, 0x0f, ZC3XX_R1CB_SHARPNESS05},	/* sharpness- */
	{0xa0, 0x13, ZC3XX_R120_GAMMA00},	/* gamma 4 */
	{0xa0, 0x38, ZC3XX_R121_GAMMA01},
	{0xa0, 0x59, ZC3XX_R122_GAMMA02},
	{0xa0, 0x79, ZC3XX_R123_GAMMA03},
	{0xa0, 0x92, ZC3XX_R124_GAMMA04},
	{0xa0, 0xa7, ZC3XX_R125_GAMMA05},
	{0xa0, 0xb9, ZC3XX_R126_GAMMA06},
	{0xa0, 0xc8, ZC3XX_R127_GAMMA07},
	{0xa0, 0xd4, ZC3XX_R128_GAMMA08},
	{0xa0, 0xdf, ZC3XX_R129_GAMMA09},
	{0xa0, 0xe7, ZC3XX_R12A_GAMMA0A},
	{0xa0, 0xee, ZC3XX_R12B_GAMMA0B},
	{0xa0, 0xf4, ZC3XX_R12C_GAMMA0C},
	{0xa0, 0xf9, ZC3XX_R12D_GAMMA0D},
	{0xa0, 0xfc, ZC3XX_R12E_GAMMA0E},
	{0xa0, 0xff, ZC3XX_R12F_GAMMA0F},
	{0xa0, 0x26, ZC3XX_R130_GAMMA10},
	{0xa0, 0x22, ZC3XX_R131_GAMMA11},
	{0xa0, 0x20, ZC3XX_R132_GAMMA12},
	{0xa0, 0x1c, ZC3XX_R133_GAMMA13},
	{0xa0, 0x16, ZC3XX_R134_GAMMA14},
	{0xa0, 0x13, ZC3XX_R135_GAMMA15},
	{0xa0, 0x10, ZC3XX_R136_GAMMA16},
	{0xa0, 0x0d, ZC3XX_R137_GAMMA17},
	{0xa0, 0x0b, ZC3XX_R138_GAMMA18},
	{0xa0, 0x09, ZC3XX_R139_GAMMA19},
	{0xa0, 0x07, ZC3XX_R13A_GAMMA1A},
	{0xa0, 0x06, ZC3XX_R13B_GAMMA1B},
	{0xa0, 0x05, ZC3XX_R13C_GAMMA1C},
	{0xa0, 0x04, ZC3XX_R13D_GAMMA1D},
	{0xa0, 0x03, ZC3XX_R13E_GAMMA1E},
	{0xa0, 0x02, ZC3XX_R13F_GAMMA1F},
	{0xa0, 0x58, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xf4, ZC3XX_R10B_RGB01},
	{0xa0, 0xf4, ZC3XX_R10C_RGB02},
	{0xa0, 0xf4, ZC3XX_R10D_RGB10},
	{0xa0, 0x58, ZC3XX_R10E_RGB11},
	{0xa0, 0xf4, ZC3XX_R10F_RGB12},
	{0xa0, 0xf4, ZC3XX_R110_RGB20},
	{0xa0, 0xf4, ZC3XX_R111_RGB21},
	{0xa0, 0x58, ZC3XX_R112_RGB22},
	{0xa0, 0x00, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x13, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x22, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x14, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x20, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x21, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x22, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x01, ZC3XX_R0A3_EXPOSURETIMEHIGH},
	{0xa0, 0x22, ZC3XX_R0A4_EXPOSURETIMELOW},
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},
	{0xa0, 0x07, ZC3XX_R191_EXPOSURELIMITMID},
	{0xa0, 0xee, ZC3XX_R192_EXPOSURELIMITLOW},
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},
	{0xa0, 0x3a, ZC3XX_R197_ANTIFLICKERLOW},
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},
	{0xa0, 0x0c, ZC3XX_R1A9_DIGITALLIMITDIFF},
	{0xa0, 0x28, ZC3XX_R1AA_DIGITALGAINSTEP},
	{0xa0, 0x04, ZC3XX_R01D_HSYNC_0},
	{0xa0, 0x0f, ZC3XX_R01E_HSYNC_1},
	{0xa0, 0x19, ZC3XX_R01F_HSYNC_2},
	{0xa0, 0x1f, ZC3XX_R020_HSYNC_3},
	{0xa0, 0x60, ZC3XX_R11D_GLOBALGAIN},
	{0xa0, 0x60, ZC3XX_R11D_GLOBALGAIN},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x60, ZC3XX_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GGAIN},
	{0xa0, 0x4c, ZC3XX_R118_BGAIN},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x20, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x21, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x5c, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x13, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x5c, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x14, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x20, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x21, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x96, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x13, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x96, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x14, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
	{}
};

static const struct usb_action cs2102K_InitialScale[] = {
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},
	{0xa0, 0x00, ZC3XX_R002_CLOCKSELECT},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},
	{0xa0, 0x08, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},
	{0xa0, 0xe8, ZC3XX_R09C_WINHEIGHTLOW},
	{0xa0, 0x88, ZC3XX_R09E_WINWIDTHLOW},
/*fixme: next sequence = i2c exchanges*/
	{0xa0, 0x55, ZC3XX_R08B_I2CDEVICEADDR},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x0a, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x02, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x0b, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x02, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x0c, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x7b, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x0d, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0xa3, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x03, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0xfb, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x05, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x06, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x03, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x09, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x08, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x0e, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x0f, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x18, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x10, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x18, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x11, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x18, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x12, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x18, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x15, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x16, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x0c, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x17, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x0c, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0xf7, ZC3XX_R101_SENSORCORRECTION},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x78, ZC3XX_R18D_YTARGET},
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x20, ZC3XX_R087_EXPTIMEMID},
	{0xa0, 0x21, ZC3XX_R088_EXPTIMELOW},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xa0, 0x00, 0x01ad},
	{0xa0, 0x01, 0x01b1},
	{0xa0, 0x02, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x60, ZC3XX_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GGAIN},
	{0xa0, 0x4c, ZC3XX_R118_BGAIN},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* clock ? */
	{0xa0, 0x08, ZC3XX_R1C6_SHARPNESS00},	/* sharpness+ */
	{0xa0, 0x0f, ZC3XX_R1CB_SHARPNESS05},	/* sharpness- */
	{0xa0, 0x13, ZC3XX_R120_GAMMA00},	/* gamma 4 */
	{0xa0, 0x38, ZC3XX_R121_GAMMA01},
	{0xa0, 0x59, ZC3XX_R122_GAMMA02},
	{0xa0, 0x79, ZC3XX_R123_GAMMA03},
	{0xa0, 0x92, ZC3XX_R124_GAMMA04},
	{0xa0, 0xa7, ZC3XX_R125_GAMMA05},
	{0xa0, 0xb9, ZC3XX_R126_GAMMA06},
	{0xa0, 0xc8, ZC3XX_R127_GAMMA07},
	{0xa0, 0xd4, ZC3XX_R128_GAMMA08},
	{0xa0, 0xdf, ZC3XX_R129_GAMMA09},
	{0xa0, 0xe7, ZC3XX_R12A_GAMMA0A},
	{0xa0, 0xee, ZC3XX_R12B_GAMMA0B},
	{0xa0, 0xf4, ZC3XX_R12C_GAMMA0C},
	{0xa0, 0xf9, ZC3XX_R12D_GAMMA0D},
	{0xa0, 0xfc, ZC3XX_R12E_GAMMA0E},
	{0xa0, 0xff, ZC3XX_R12F_GAMMA0F},
	{0xa0, 0x26, ZC3XX_R130_GAMMA10},
	{0xa0, 0x22, ZC3XX_R131_GAMMA11},
	{0xa0, 0x20, ZC3XX_R132_GAMMA12},
	{0xa0, 0x1c, ZC3XX_R133_GAMMA13},
	{0xa0, 0x16, ZC3XX_R134_GAMMA14},
	{0xa0, 0x13, ZC3XX_R135_GAMMA15},
	{0xa0, 0x10, ZC3XX_R136_GAMMA16},
	{0xa0, 0x0d, ZC3XX_R137_GAMMA17},
	{0xa0, 0x0b, ZC3XX_R138_GAMMA18},
	{0xa0, 0x09, ZC3XX_R139_GAMMA19},
	{0xa0, 0x07, ZC3XX_R13A_GAMMA1A},
	{0xa0, 0x06, ZC3XX_R13B_GAMMA1B},
	{0xa0, 0x05, ZC3XX_R13C_GAMMA1C},
	{0xa0, 0x04, ZC3XX_R13D_GAMMA1D},
	{0xa0, 0x03, ZC3XX_R13E_GAMMA1E},
	{0xa0, 0x02, ZC3XX_R13F_GAMMA1F},
	{0xa0, 0x58, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xf4, ZC3XX_R10B_RGB01},
	{0xa0, 0xf4, ZC3XX_R10C_RGB02},
	{0xa0, 0xf4, ZC3XX_R10D_RGB10},
	{0xa0, 0x58, ZC3XX_R10E_RGB11},
	{0xa0, 0xf4, ZC3XX_R10F_RGB12},
	{0xa0, 0xf4, ZC3XX_R110_RGB20},
	{0xa0, 0xf4, ZC3XX_R111_RGB21},
	{0xa0, 0x58, ZC3XX_R112_RGB22},
	{0xa0, 0x00, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x13, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x22, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x14, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x20, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x21, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x22, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x01, ZC3XX_R0A3_EXPOSURETIMEHIGH},
	{0xa0, 0x22, ZC3XX_R0A4_EXPOSURETIMELOW},
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},
	{0xa0, 0x07, ZC3XX_R191_EXPOSURELIMITMID},
	{0xa0, 0xee, ZC3XX_R192_EXPOSURELIMITLOW},
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},
	{0xa0, 0x3a, ZC3XX_R197_ANTIFLICKERLOW},
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},
	{0xa0, 0x0c, ZC3XX_R1A9_DIGITALLIMITDIFF},
	{0xa0, 0x28, ZC3XX_R1AA_DIGITALGAINSTEP},
	{0xa0, 0x04, ZC3XX_R01D_HSYNC_0},
	{0xa0, 0x0f, ZC3XX_R01E_HSYNC_1},
	{0xa0, 0x19, ZC3XX_R01F_HSYNC_2},
	{0xa0, 0x1f, ZC3XX_R020_HSYNC_3},
	{0xa0, 0x60, ZC3XX_R11D_GLOBALGAIN},
	{0xa0, 0x60, ZC3XX_R11D_GLOBALGAIN},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x60, ZC3XX_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GGAIN},
	{0xa0, 0x4c, ZC3XX_R118_BGAIN},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x20, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x21, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x5c, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x13, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x5c, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x14, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x20, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x21, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x96, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x13, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x96, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x14, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
/*fixme:what does the next sequence?*/
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x20, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x21, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0xd0, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x13, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0xd0, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x14, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x02, ZC3XX_R008_CLOCKSETTING},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x20, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x02, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x21, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x0a, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x13, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x0a, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x14, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x02, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x20, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x02, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x21, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x44, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x13, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x44, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x14, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x02, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x20, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x02, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x21, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x7e, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x13, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x7e, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x14, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x02, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{}
};

static const struct usb_action gc0305_Initial[] = {	/* 640x480 */
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},	/* 00,00,01,cc */
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* 00,08,03,cc */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xa0, 0x04, ZC3XX_R002_CLOCKSELECT},	/* 00,02,04,cc */
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},	/* 00,03,02,cc */
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},	/* 00,04,80,cc */
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},	/* 00,05,01,cc */
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},	/* 00,06,e0,cc */
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},	/* 00,01,01,cc */
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,03,cc */
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,01,cc */
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},	/* 00,98,00,cc */
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},	/* 00,9a,00,cc */
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},	/* 01,1a,00,cc */
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},	/* 01,1c,00,cc */
	{0xa0, 0xe6, ZC3XX_R09C_WINHEIGHTLOW},	/* 00,9c,e6,cc */
	{0xa0, 0x86, ZC3XX_R09E_WINWIDTHLOW},	/* 00,9e,86,cc */
	{0xa0, 0x98, ZC3XX_R08B_I2CDEVICEADDR},	/* 00,8b,98,cc */
	{0xaa, 0x13, 0x0002},	/* 00,13,02,aa */
	{0xaa, 0x15, 0x0003},	/* 00,15,03,aa */
	{0xaa, 0x01, 0x0000},	/* 00,01,00,aa */
	{0xaa, 0x02, 0x0000},	/* 00,02,00,aa */
	{0xaa, 0x1a, 0x0000},	/* 00,1a,00,aa */
	{0xaa, 0x1c, 0x0017},	/* 00,1c,17,aa */
	{0xaa, 0x1d, 0x0080},	/* 00,1d,80,aa */
	{0xaa, 0x1f, 0x0008},	/* 00,1f,08,aa */
	{0xaa, 0x21, 0x0012},	/* 00,21,12,aa */
	{0xa0, 0x82, ZC3XX_R086_EXPTIMEHIGH},	/* 00,86,82,cc */
	{0xa0, 0x83, ZC3XX_R087_EXPTIMEMID},	/* 00,87,83,cc */
	{0xa0, 0x84, ZC3XX_R088_EXPTIMELOW},	/* 00,88,84,cc */
	{0xaa, 0x05, 0x0000},	/* 00,05,00,aa */
	{0xaa, 0x0a, 0x0000},	/* 00,0a,00,aa */
	{0xaa, 0x0b, 0x00b0},	/* 00,0b,b0,aa */
	{0xaa, 0x0c, 0x0000},	/* 00,0c,00,aa */
	{0xaa, 0x0d, 0x00b0},	/* 00,0d,b0,aa */
	{0xaa, 0x0e, 0x0000},	/* 00,0e,00,aa */
	{0xaa, 0x0f, 0x00b0},	/* 00,0f,b0,aa */
	{0xaa, 0x10, 0x0000},	/* 00,10,00,aa */
	{0xaa, 0x11, 0x00b0},	/* 00,11,b0,aa */
	{0xaa, 0x16, 0x0001},	/* 00,16,01,aa */
	{0xaa, 0x17, 0x00e6},	/* 00,17,e6,aa */
	{0xaa, 0x18, 0x0002},	/* 00,18,02,aa */
	{0xaa, 0x19, 0x0086},	/* 00,19,86,aa */
	{0xaa, 0x20, 0x0000},	/* 00,20,00,aa */
	{0xaa, 0x1b, 0x0020},	/* 00,1b,20,aa */
	{0xa0, 0xb7, ZC3XX_R101_SENSORCORRECTION},	/* 01,01,b7,cc */
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,05,cc */
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},	/* 01,00,0d,cc */
	{0xa0, 0x76, ZC3XX_R189_AWBSTATUS},	/* 01,89,76,cc */
	{0xa0, 0x09, 0x01ad},	/* 01,ad,09,cc */
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},	/* 01,c5,03,cc */
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},	/* 01,cb,13,cc */
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},	/* 02,50,08,cc */
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},	/* 03,01,08,cc */
	{0xa0, 0x60, ZC3XX_R1A8_DIGITALGAIN},	/* 01,a8,60,cc */
	{0xa0, 0x85, ZC3XX_R18D_YTARGET},	/* 01,8d,85,cc */
	{0xa0, 0x00, 0x011e},	/* 01,1e,00,cc */
	{0xa0, 0x52, ZC3XX_R116_RGAIN},	/* 01,16,52,cc */
	{0xa0, 0x40, ZC3XX_R117_GGAIN},	/* 01,17,40,cc */
	{0xa0, 0x52, ZC3XX_R118_BGAIN},	/* 01,18,52,cc */
	{0xa0, 0x03, ZC3XX_R113_RGB03},	/* 01,13,03,cc */
	{}
};
static const struct usb_action gc0305_InitialScale[] = { /* 320x240 */
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},	/* 00,00,01,cc */
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* 00,08,03,cc */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xa0, 0x10, ZC3XX_R002_CLOCKSELECT},	/* 00,02,10,cc */
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},	/* 00,03,02,cc */
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},	/* 00,04,80,cc */
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},	/* 00,05,01,cc */
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},	/* 00,06,e0,cc */
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},	/* 00,01,01,cc */
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,03,cc */
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,01,cc */
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},	/* 00,98,00,cc */
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},	/* 00,9a,00,cc */
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},	/* 01,1a,00,cc */
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},	/* 01,1c,00,cc */
	{0xa0, 0xe8, ZC3XX_R09C_WINHEIGHTLOW},	/* 00,9c,e8,cc */
	{0xa0, 0x88, ZC3XX_R09E_WINWIDTHLOW},	/* 00,9e,88,cc */
	{0xa0, 0x98, ZC3XX_R08B_I2CDEVICEADDR},	/* 00,8b,98,cc */
	{0xaa, 0x13, 0x0000},	/* 00,13,00,aa */
	{0xaa, 0x15, 0x0001},	/* 00,15,01,aa */
	{0xaa, 0x01, 0x0000},	/* 00,01,00,aa */
	{0xaa, 0x02, 0x0000},	/* 00,02,00,aa */
	{0xaa, 0x1a, 0x0000},	/* 00,1a,00,aa */
	{0xaa, 0x1c, 0x0017},	/* 00,1c,17,aa */
	{0xaa, 0x1d, 0x0080},	/* 00,1d,80,aa */
	{0xaa, 0x1f, 0x0008},	/* 00,1f,08,aa */
	{0xaa, 0x21, 0x0012},	/* 00,21,12,aa */
	{0xa0, 0x82, ZC3XX_R086_EXPTIMEHIGH},	/* 00,86,82,cc */
	{0xa0, 0x83, ZC3XX_R087_EXPTIMEMID},	/* 00,87,83,cc */
	{0xa0, 0x84, ZC3XX_R088_EXPTIMELOW},	/* 00,88,84,cc */
	{0xaa, 0x05, 0x0000},	/* 00,05,00,aa */
	{0xaa, 0x0a, 0x0000},	/* 00,0a,00,aa */
	{0xaa, 0x0b, 0x00b0},	/* 00,0b,b0,aa */
	{0xaa, 0x0c, 0x0000},	/* 00,0c,00,aa */
	{0xaa, 0x0d, 0x00b0},	/* 00,0d,b0,aa */
	{0xaa, 0x0e, 0x0000},	/* 00,0e,00,aa */
	{0xaa, 0x0f, 0x00b0},	/* 00,0f,b0,aa */
	{0xaa, 0x10, 0x0000},	/* 00,10,00,aa */
	{0xaa, 0x11, 0x00b0},	/* 00,11,b0,aa */
	{0xaa, 0x16, 0x0001},	/* 00,16,01,aa */
	{0xaa, 0x17, 0x00e8},	/* 00,17,e8,aa */
	{0xaa, 0x18, 0x0002},	/* 00,18,02,aa */
	{0xaa, 0x19, 0x0088},	/* 00,19,88,aa */
	{0xaa, 0x20, 0x0000},	/* 00,20,00,aa */
	{0xaa, 0x1b, 0x0020},	/* 00,1b,20,aa */
	{0xa0, 0xb7, ZC3XX_R101_SENSORCORRECTION},	/* 01,01,b7,cc */
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,05,cc */
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},	/* 01,00,0d,cc */
	{0xa0, 0x76, ZC3XX_R189_AWBSTATUS},	/* 01,89,76,cc */
	{0xa0, 0x09, 0x01ad},	/* 01,ad,09,cc */
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},	/* 01,c5,03,cc */
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},	/* 01,cb,13,cc */
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},	/* 02,50,08,cc */
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},	/* 03,01,08,cc */
	{0xa0, 0x60, ZC3XX_R1A8_DIGITALGAIN},	/* 01,a8,60,cc */
	{0xa0, 0x00, 0x011e},	/* 01,1e,00,cc */
	{0xa0, 0x52, ZC3XX_R116_RGAIN},	/* 01,16,52,cc */
	{0xa0, 0x40, ZC3XX_R117_GGAIN},	/* 01,17,40,cc */
	{0xa0, 0x52, ZC3XX_R118_BGAIN},	/* 01,18,52,cc */
	{0xa0, 0x03, ZC3XX_R113_RGB03},	/* 01,13,03,cc */
	{}
};
static const struct usb_action gc0305_50HZ[] = {
	{0xaa, 0x82, 0x0000},	/* 00,82,00,aa */
	{0xaa, 0x83, 0x0002},	/* 00,83,02,aa */
	{0xaa, 0x84, 0x0038},	/* 00,84,38,aa */	/* win: 00,84,ec */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},	/* 01,90,00,cc */
	{0xa0, 0x0b, ZC3XX_R191_EXPOSURELIMITMID},	/* 01,91,0b,cc */
	{0xa0, 0x18, ZC3XX_R192_EXPOSURELIMITLOW},	/* 01,92,18,cc */
							/* win: 01,92,10 */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},	/* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},	/* 01,96,00,cc */
	{0xa0, 0x8e, ZC3XX_R197_ANTIFLICKERLOW},	/* 01,97,8e,cc */
							/* win: 01,97,ec */
	{0xa0, 0x0e, ZC3XX_R18C_AEFREEZE},	/* 01,8c,0e,cc */
	{0xa0, 0x15, ZC3XX_R18F_AEUNFREEZE},	/* 01,8f,15,cc */
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF},	/* 01,a9,10,cc */
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP},	/* 01,aa,24,cc */
	{0xa0, 0x62, ZC3XX_R01D_HSYNC_0},	/* 00,1d,62,cc */
	{0xa0, 0x90, ZC3XX_R01E_HSYNC_1},	/* 00,1e,90,cc */
	{0xa0, 0xc8, ZC3XX_R01F_HSYNC_2},	/* 00,1f,c8,cc */
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3},	/* 00,20,ff,cc */
	{0xa0, 0x60, ZC3XX_R11D_GLOBALGAIN},	/* 01,1d,60,cc */
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},	/* 01,80,42,cc */
/*	{0xa0, 0x85, ZC3XX_R18D_YTARGET},	 * 01,8d,85,cc *
						 * if 640x480 */
	{}
};
static const struct usb_action gc0305_60HZ[] = {
	{0xaa, 0x82, 0x0000},	/* 00,82,00,aa */
	{0xaa, 0x83, 0x0000},	/* 00,83,00,aa */
	{0xaa, 0x84, 0x00ec},	/* 00,84,ec,aa */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},	/* 01,90,00,cc */
	{0xa0, 0x0b, ZC3XX_R191_EXPOSURELIMITMID},	/* 01,91,0b,cc */
	{0xa0, 0x10, ZC3XX_R192_EXPOSURELIMITLOW},	/* 01,92,10,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},	/* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},	/* 01,96,00,cc */
	{0xa0, 0xec, ZC3XX_R197_ANTIFLICKERLOW},	/* 01,97,ec,cc */
	{0xa0, 0x0e, ZC3XX_R18C_AEFREEZE},	/* 01,8c,0e,cc */
	{0xa0, 0x15, ZC3XX_R18F_AEUNFREEZE},	/* 01,8f,15,cc */
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF},	/* 01,a9,10,cc */
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP},	/* 01,aa,24,cc */
	{0xa0, 0x62, ZC3XX_R01D_HSYNC_0},	/* 00,1d,62,cc */
	{0xa0, 0x90, ZC3XX_R01E_HSYNC_1},	/* 00,1e,90,cc */
	{0xa0, 0xc8, ZC3XX_R01F_HSYNC_2},	/* 00,1f,c8,cc */
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3},	/* 00,20,ff,cc */
	{0xa0, 0x60, ZC3XX_R11D_GLOBALGAIN},	/* 01,1d,60,cc */
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},	/* 01,80,42,cc */
	{0xa0, 0x80, ZC3XX_R18D_YTARGET},	/* 01,8d,80,cc */
	{}
};

static const struct usb_action gc0305_NoFliker[] = {
	{0xa0, 0x0c, ZC3XX_R100_OPERATIONMODE},	/* 01,00,0c,cc */
	{0xaa, 0x82, 0x0000},	/* 00,82,00,aa */
	{0xaa, 0x83, 0x0000},	/* 00,83,00,aa */
	{0xaa, 0x84, 0x0020},	/* 00,84,20,aa */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},	/* 01,90,00,cc */
	{0xa0, 0x00, ZC3XX_R191_EXPOSURELIMITMID},	/* 01,91,00,cc */
	{0xa0, 0x48, ZC3XX_R192_EXPOSURELIMITLOW},	/* 01,92,48,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},	/* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},	/* 01,96,00,cc */
	{0xa0, 0x10, ZC3XX_R197_ANTIFLICKERLOW},	/* 01,97,10,cc */
	{0xa0, 0x0e, ZC3XX_R18C_AEFREEZE},	/* 01,8c,0e,cc */
	{0xa0, 0x15, ZC3XX_R18F_AEUNFREEZE},	/* 01,8f,15,cc */
	{0xa0, 0x62, ZC3XX_R01D_HSYNC_0},	/* 00,1d,62,cc */
	{0xa0, 0x90, ZC3XX_R01E_HSYNC_1},	/* 00,1e,90,cc */
	{0xa0, 0xc8, ZC3XX_R01F_HSYNC_2},	/* 00,1f,c8,cc */
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3},	/* 00,20,ff,cc */
	{0xa0, 0x60, ZC3XX_R11D_GLOBALGAIN},	/* 01,1d,60,cc */
	{0xa0, 0x03, ZC3XX_R180_AUTOCORRECTENABLE},	/* 01,80,03,cc */
	{0xa0, 0x80, ZC3XX_R18D_YTARGET},	/* 01,8d,80,cc */
	{}
};

static const struct usb_action hdcs2020xb_Initial[] = {
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},
	{0xa0, 0x11, ZC3XX_R002_CLOCKSELECT},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* qtable 0x05 */
	{0xa0, 0x08, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},
	{0xa0, 0xe8, ZC3XX_R09C_WINHEIGHTLOW},
	{0xa0, 0x88, ZC3XX_R09E_WINWIDTHLOW},
	{0xaa, 0x1c, 0x0000},
	{0xaa, 0x0a, 0x0001},
	{0xaa, 0x0b, 0x0006},
	{0xaa, 0x0c, 0x007b},
	{0xaa, 0x0d, 0x00a7},
	{0xaa, 0x03, 0x00fb},
	{0xaa, 0x05, 0x0000},
	{0xaa, 0x06, 0x0003},
	{0xaa, 0x09, 0x0008},

	{0xaa, 0x0f, 0x0018},	/* set sensor gain */
	{0xaa, 0x10, 0x0018},
	{0xaa, 0x11, 0x0018},
	{0xaa, 0x12, 0x0018},

	{0xaa, 0x15, 0x004e},
	{0xaa, 0x1c, 0x0004},
	{0xa0, 0xb7, ZC3XX_R101_SENSORCORRECTION},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x70, ZC3XX_R18D_YTARGET},
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xa1, 0x01, 0x0002},
	{0xa1, 0x01, 0x0008},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x02, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x40, ZC3XX_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GGAIN},
	{0xa0, 0x40, ZC3XX_R118_BGAIN},
	{0xa1, 0x01, 0x0008},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* clock ? */
	{0xa0, 0x08, ZC3XX_R1C6_SHARPNESS00},	/* sharpness+ */
	{0xa1, 0x01, 0x01c8},
	{0xa1, 0x01, 0x01c9},
	{0xa1, 0x01, 0x01ca},
	{0xa0, 0x0f, ZC3XX_R1CB_SHARPNESS05},	/* sharpness- */
	{0xa0, 0x13, ZC3XX_R120_GAMMA00},	/* gamma 4 */
	{0xa0, 0x38, ZC3XX_R121_GAMMA01},
	{0xa0, 0x59, ZC3XX_R122_GAMMA02},
	{0xa0, 0x79, ZC3XX_R123_GAMMA03},
	{0xa0, 0x92, ZC3XX_R124_GAMMA04},
	{0xa0, 0xa7, ZC3XX_R125_GAMMA05},
	{0xa0, 0xb9, ZC3XX_R126_GAMMA06},
	{0xa0, 0xc8, ZC3XX_R127_GAMMA07},
	{0xa0, 0xd4, ZC3XX_R128_GAMMA08},
	{0xa0, 0xdf, ZC3XX_R129_GAMMA09},
	{0xa0, 0xe7, ZC3XX_R12A_GAMMA0A},
	{0xa0, 0xee, ZC3XX_R12B_GAMMA0B},
	{0xa0, 0xf4, ZC3XX_R12C_GAMMA0C},
	{0xa0, 0xf9, ZC3XX_R12D_GAMMA0D},
	{0xa0, 0xfc, ZC3XX_R12E_GAMMA0E},
	{0xa0, 0xff, ZC3XX_R12F_GAMMA0F},
	{0xa0, 0x26, ZC3XX_R130_GAMMA10},
	{0xa0, 0x22, ZC3XX_R131_GAMMA11},
	{0xa0, 0x20, ZC3XX_R132_GAMMA12},
	{0xa0, 0x1c, ZC3XX_R133_GAMMA13},
	{0xa0, 0x16, ZC3XX_R134_GAMMA14},
	{0xa0, 0x13, ZC3XX_R135_GAMMA15},
	{0xa0, 0x10, ZC3XX_R136_GAMMA16},
	{0xa0, 0x0d, ZC3XX_R137_GAMMA17},
	{0xa0, 0x0b, ZC3XX_R138_GAMMA18},
	{0xa0, 0x09, ZC3XX_R139_GAMMA19},
	{0xa0, 0x07, ZC3XX_R13A_GAMMA1A},
	{0xa0, 0x06, ZC3XX_R13B_GAMMA1B},
	{0xa0, 0x05, ZC3XX_R13C_GAMMA1C},
	{0xa0, 0x04, ZC3XX_R13D_GAMMA1D},
	{0xa0, 0x03, ZC3XX_R13E_GAMMA1E},
	{0xa0, 0x02, ZC3XX_R13F_GAMMA1F},

	{0xa0, 0x66, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xed, ZC3XX_R10B_RGB01},
	{0xa0, 0xed, ZC3XX_R10C_RGB02},
	{0xa0, 0xed, ZC3XX_R10D_RGB10},
	{0xa0, 0x66, ZC3XX_R10E_RGB11},
	{0xa0, 0xed, ZC3XX_R10F_RGB12},
	{0xa0, 0xed, ZC3XX_R110_RGB20},
	{0xa0, 0xed, ZC3XX_R111_RGB21},
	{0xa0, 0x66, ZC3XX_R112_RGB22},

	{0xa1, 0x01, 0x0180},
	{0xa0, 0x00, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0xaa, 0x13, 0x0031},
	{0xaa, 0x14, 0x0001},
	{0xaa, 0x0e, 0x0004},
	{0xaa, 0x19, 0x00cd},
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},
	{0xa0, 0x02, ZC3XX_R191_EXPOSURELIMITMID},
	{0xa0, 0x62, ZC3XX_R192_EXPOSURELIMITLOW},
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},
	{0xa0, 0x3d, ZC3XX_R197_ANTIFLICKERLOW},
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},

	{0xa0, 0x0c, ZC3XX_R1A9_DIGITALLIMITDIFF},	/* 0x14 */
	{0xa0, 0x28, ZC3XX_R1AA_DIGITALGAINSTEP},
	{0xa0, 0x04, ZC3XX_R01D_HSYNC_0},
	{0xa0, 0x18, ZC3XX_R01E_HSYNC_1},
	{0xa0, 0x2c, ZC3XX_R01F_HSYNC_2},
	{0xa0, 0x41, ZC3XX_R020_HSYNC_3},
	{0xa0, 0x60, ZC3XX_R11D_GLOBALGAIN},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x40, ZC3XX_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GGAIN},
	{0xa0, 0x40, ZC3XX_R118_BGAIN},
	{}
};
static const struct usb_action hdcs2020xb_InitialScale[] = {
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},
	{0xa0, 0x00, ZC3XX_R002_CLOCKSELECT},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},
	{0xa0, 0x08, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},
	{0xa0, 0xe8, ZC3XX_R09C_WINHEIGHTLOW},
	{0xa0, 0x88, ZC3XX_R09E_WINWIDTHLOW},
	{0xaa, 0x1c, 0x0000},
	{0xaa, 0x0a, 0x0001},
	{0xaa, 0x0b, 0x0006},
	{0xaa, 0x0c, 0x007a},
	{0xaa, 0x0d, 0x00a7},
	{0xaa, 0x03, 0x00fb},
	{0xaa, 0x05, 0x0000},
	{0xaa, 0x06, 0x0003},
	{0xaa, 0x09, 0x0008},
	{0xaa, 0x0f, 0x0018},	/* original setting */
	{0xaa, 0x10, 0x0018},
	{0xaa, 0x11, 0x0018},
	{0xaa, 0x12, 0x0018},
	{0xaa, 0x15, 0x004e},
	{0xaa, 0x1c, 0x0004},
	{0xa0, 0xf7, ZC3XX_R101_SENSORCORRECTION},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x70, ZC3XX_R18D_YTARGET},
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xa1, 0x01, 0x0002},
	{0xa1, 0x01, 0x0008},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x02, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x40, ZC3XX_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GGAIN},
	{0xa0, 0x40, ZC3XX_R118_BGAIN},
	{0xa1, 0x01, 0x0008},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* clock ? */
	{0xa0, 0x08, ZC3XX_R1C6_SHARPNESS00},	/* sharpness+ */
	{0xa1, 0x01, 0x01c8},
	{0xa1, 0x01, 0x01c9},
	{0xa1, 0x01, 0x01ca},
	{0xa0, 0x0f, ZC3XX_R1CB_SHARPNESS05},	/* sharpness- */
	{0xa0, 0x13, ZC3XX_R120_GAMMA00},	/* gamma 4 */
	{0xa0, 0x38, ZC3XX_R121_GAMMA01},
	{0xa0, 0x59, ZC3XX_R122_GAMMA02},
	{0xa0, 0x79, ZC3XX_R123_GAMMA03},
	{0xa0, 0x92, ZC3XX_R124_GAMMA04},
	{0xa0, 0xa7, ZC3XX_R125_GAMMA05},
	{0xa0, 0xb9, ZC3XX_R126_GAMMA06},
	{0xa0, 0xc8, ZC3XX_R127_GAMMA07},
	{0xa0, 0xd4, ZC3XX_R128_GAMMA08},
	{0xa0, 0xdf, ZC3XX_R129_GAMMA09},
	{0xa0, 0xe7, ZC3XX_R12A_GAMMA0A},
	{0xa0, 0xee, ZC3XX_R12B_GAMMA0B},
	{0xa0, 0xf4, ZC3XX_R12C_GAMMA0C},
	{0xa0, 0xf9, ZC3XX_R12D_GAMMA0D},
	{0xa0, 0xfc, ZC3XX_R12E_GAMMA0E},
	{0xa0, 0xff, ZC3XX_R12F_GAMMA0F},
	{0xa0, 0x26, ZC3XX_R130_GAMMA10},
	{0xa0, 0x22, ZC3XX_R131_GAMMA11},
	{0xa0, 0x20, ZC3XX_R132_GAMMA12},
	{0xa0, 0x1c, ZC3XX_R133_GAMMA13},
	{0xa0, 0x16, ZC3XX_R134_GAMMA14},
	{0xa0, 0x13, ZC3XX_R135_GAMMA15},
	{0xa0, 0x10, ZC3XX_R136_GAMMA16},
	{0xa0, 0x0d, ZC3XX_R137_GAMMA17},
	{0xa0, 0x0b, ZC3XX_R138_GAMMA18},
	{0xa0, 0x09, ZC3XX_R139_GAMMA19},
	{0xa0, 0x07, ZC3XX_R13A_GAMMA1A},
	{0xa0, 0x06, ZC3XX_R13B_GAMMA1B},
	{0xa0, 0x05, ZC3XX_R13C_GAMMA1C},
	{0xa0, 0x04, ZC3XX_R13D_GAMMA1D},
	{0xa0, 0x03, ZC3XX_R13E_GAMMA1E},
	{0xa0, 0x02, ZC3XX_R13F_GAMMA1F},
	{0xa0, 0x66, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xed, ZC3XX_R10B_RGB01},
	{0xa0, 0xed, ZC3XX_R10C_RGB02},
	{0xa0, 0xed, ZC3XX_R10D_RGB10},
	{0xa0, 0x66, ZC3XX_R10E_RGB11},
	{0xa0, 0xed, ZC3XX_R10F_RGB12},
	{0xa0, 0xed, ZC3XX_R110_RGB20},
	{0xa0, 0xed, ZC3XX_R111_RGB21},
	{0xa0, 0x66, ZC3XX_R112_RGB22},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x00, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
 /**** set exposure ***/
	{0xaa, 0x13, 0x0031},
	{0xaa, 0x14, 0x0001},
	{0xaa, 0x0e, 0x0004},
	{0xaa, 0x19, 0x00cd},
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},
	{0xa0, 0x02, ZC3XX_R191_EXPOSURELIMITMID},
	{0xa0, 0x62, ZC3XX_R192_EXPOSURELIMITLOW},
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},
	{0xa0, 0x3d, ZC3XX_R197_ANTIFLICKERLOW},
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},
	{0xa0, 0x0c, ZC3XX_R1A9_DIGITALLIMITDIFF},
	{0xa0, 0x28, ZC3XX_R1AA_DIGITALGAINSTEP},
	{0xa0, 0x04, ZC3XX_R01D_HSYNC_0},
	{0xa0, 0x18, ZC3XX_R01E_HSYNC_1},
	{0xa0, 0x2c, ZC3XX_R01F_HSYNC_2},
	{0xa0, 0x41, ZC3XX_R020_HSYNC_3},
	{0xa0, 0x60, ZC3XX_R11D_GLOBALGAIN},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x40, ZC3XX_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GGAIN},
	{0xa0, 0x40, ZC3XX_R118_BGAIN},
	{}
};
static const struct usb_action hdcs2020b_50HZ[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xaa, 0x13, 0x0018},			/* 00,13,18,aa */
	{0xaa, 0x14, 0x0001},			/* 00,14,01,aa */
	{0xaa, 0x0e, 0x0005},			/* 00,0e,05,aa */
	{0xaa, 0x19, 0x001f},			/* 00,19,1f,aa */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x02, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,02,cc */
	{0xa0, 0x76, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,76,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x46, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,46,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,cc */
	{0xa0, 0x0c, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,0c,cc */
	{0xa0, 0x28, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,28,cc */
	{0xa0, 0x05, ZC3XX_R01D_HSYNC_0}, /* 00,1d,05,cc */
	{0xa0, 0x1a, ZC3XX_R01E_HSYNC_1}, /* 00,1e,1a,cc */
	{0xa0, 0x2f, ZC3XX_R01F_HSYNC_2}, /* 00,1f,2f,cc */
	{}
};
static const struct usb_action hdcs2020b_60HZ[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xaa, 0x13, 0x0031},			/* 00,13,31,aa */
	{0xaa, 0x14, 0x0001},			/* 00,14,01,aa */
	{0xaa, 0x0e, 0x0004},			/* 00,0e,04,aa */
	{0xaa, 0x19, 0x00cd},			/* 00,19,cd,aa */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x02, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,02,cc */
	{0xa0, 0x62, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,62,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x3d, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,3d,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,cc */
	{0xa0, 0x0c, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,0c,cc */
	{0xa0, 0x28, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,28,cc */
	{0xa0, 0x04, ZC3XX_R01D_HSYNC_0}, /* 00,1d,04,cc */
	{0xa0, 0x18, ZC3XX_R01E_HSYNC_1}, /* 00,1e,18,cc */
	{0xa0, 0x2c, ZC3XX_R01F_HSYNC_2}, /* 00,1f,2c,cc */
	{}
};
static const struct usb_action hdcs2020b_NoFliker[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xaa, 0x13, 0x0010},			/* 00,13,10,aa */
	{0xaa, 0x14, 0x0001},			/* 00,14,01,aa */
	{0xaa, 0x0e, 0x0004},			/* 00,0e,04,aa */
	{0xaa, 0x19, 0x0000},			/* 00,19,00,aa */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x02, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,02,cc */
	{0xa0, 0x70, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,70,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x10, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,10,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,cc */
	{0xa0, 0x00, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,00,cc */
	{0xa0, 0x00, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,00,cc */
	{0xa0, 0x04, ZC3XX_R01D_HSYNC_0}, /* 00,1d,04,cc */
	{0xa0, 0x17, ZC3XX_R01E_HSYNC_1}, /* 00,1e,17,cc */
	{0xa0, 0x2a, ZC3XX_R01F_HSYNC_2}, /* 00,1f,2a,cc */
	{}
};

static const struct usb_action hv7131bxx_Initial[] = {		/* 320x240 */
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},
	{0xa0, 0x10, ZC3XX_R002_CLOCKSELECT},
	{0xa0, 0x00, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x77, ZC3XX_R101_SENSORCORRECTION},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* 00 */
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},
	{0xaa, 0x30, 0x002d},
	{0xaa, 0x01, 0x0005},
	{0xaa, 0x11, 0x0000},
	{0xaa, 0x13, 0x0001},	/* {0xaa, 0x13, 0x0000}, */
	{0xaa, 0x14, 0x0001},
	{0xaa, 0x15, 0x00e8},
	{0xaa, 0x16, 0x0002},
	{0xaa, 0x17, 0x0086},		/* 00,17,88,aa */
	{0xaa, 0x31, 0x0038},
	{0xaa, 0x32, 0x0038},
	{0xaa, 0x33, 0x0038},
	{0xaa, 0x5b, 0x0001},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
	{0xa0, 0x68, ZC3XX_R18D_YTARGET},
	{0xa0, 0x60, ZC3XX_R1A8_DIGITALGAIN},
	{0xa0, 0x00, 0x01ad},
	{0xa0, 0xc0, 0x019b},
	{0xa0, 0xa0, 0x019c},
	{0xa0, 0x02, ZC3XX_R188_MINGAIN},
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xaa, 0x02, 0x0090},			/* 00,02,80,aa */
	{}
};

static const struct usb_action hv7131bxx_InitialScale[] = {	/* 640x480*/
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},
	{0xa0, 0x00, ZC3XX_R002_CLOCKSELECT},
	{0xa0, 0x00, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x37, ZC3XX_R101_SENSORCORRECTION},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* 00 */
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},
	{0xaa, 0x30, 0x002d},
	{0xaa, 0x01, 0x0005},
	{0xaa, 0x11, 0x0001},
	{0xaa, 0x13, 0x0000},	/* {0xaa, 0x13, 0x0001}; */
	{0xaa, 0x14, 0x0001},
	{0xaa, 0x15, 0x00e6},
	{0xaa, 0x16, 0x0002},
	{0xaa, 0x17, 0x0086},
	{0xaa, 0x31, 0x0038},
	{0xaa, 0x32, 0x0038},
	{0xaa, 0x33, 0x0038},
	{0xaa, 0x5b, 0x0001},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
	{0xa0, 0x70, ZC3XX_R18D_YTARGET},
	{0xa0, 0x60, ZC3XX_R1A8_DIGITALGAIN},
	{0xa0, 0x00, 0x01ad},
	{0xa0, 0xc0, 0x019b},
	{0xa0, 0xa0, 0x019c},
	{0xa0, 0x02, ZC3XX_R188_MINGAIN},
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xaa, 0x02, 0x0090},	/* {0xaa, 0x02, 0x0080}, */
	{}
};
static const struct usb_action hv7131b_50HZ[] = {	/* 640x480*/
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},	/* 00,19,00,cc */
	{0xaa, 0x25, 0x0007},			/* 00,25,07,aa */
	{0xaa, 0x26, 0x0053},			/* 00,26,53,aa */
	{0xaa, 0x27, 0x0000},			/* 00,27,00,aa */
	{0xaa, 0x20, 0x0000},			/* 00,20,00,aa */
	{0xaa, 0x21, 0x0050},			/* 00,21,50,aa */
	{0xaa, 0x22, 0x001b},			/* 00,22,1b,aa */
	{0xaa, 0x23, 0x00fc},			/* 00,23,fc,aa */
	{0xa0, 0x2f, ZC3XX_R190_EXPOSURELIMITHIGH},	/* 01,90,2f,cc */
	{0xa0, 0x9b, ZC3XX_R191_EXPOSURELIMITMID},	/* 01,91,9b,cc */
	{0xa0, 0x80, ZC3XX_R192_EXPOSURELIMITLOW},	/* 01,92,80,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},	/* 01,95,00,cc */
	{0xa0, 0xea, ZC3XX_R196_ANTIFLICKERMID},	/* 01,96,ea,cc */
	{0xa0, 0x60, ZC3XX_R197_ANTIFLICKERLOW},	/* 01,97,60,cc */
	{0xa0, 0x0c, ZC3XX_R18C_AEFREEZE},	/* 01,8c,0c,cc */
	{0xa0, 0x18, ZC3XX_R18F_AEUNFREEZE},	/* 01,8f,18,cc */
	{0xa0, 0x18, ZC3XX_R1A9_DIGITALLIMITDIFF},	/* 01,a9,18,cc */
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP},	/* 01,aa,24,cc */
	{0xa0, 0x00, ZC3XX_R01D_HSYNC_0},	/* 00,1d,00,cc */
	{0xa0, 0x50, ZC3XX_R01E_HSYNC_1},	/* 00,1e,50,cc */
	{0xa0, 0x1b, ZC3XX_R01F_HSYNC_2},	/* 00,1f,1b,cc */
	{0xa0, 0xfc, ZC3XX_R020_HSYNC_3},	/* 00,20,fc,cc */
	{}
};
static const struct usb_action hv7131b_50HZScale[] = {	/* 320x240 */
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},	/* 00,19,00,cc */
	{0xaa, 0x25, 0x0007},			/* 00,25,07,aa */
	{0xaa, 0x26, 0x0053},			/* 00,26,53,aa */
	{0xaa, 0x27, 0x0000},			/* 00,27,00,aa */
	{0xaa, 0x20, 0x0000},			/* 00,20,00,aa */
	{0xaa, 0x21, 0x0050},			/* 00,21,50,aa */
	{0xaa, 0x22, 0x0012},			/* 00,22,12,aa */
	{0xaa, 0x23, 0x0080},			/* 00,23,80,aa */
	{0xa0, 0x2f, ZC3XX_R190_EXPOSURELIMITHIGH},	/* 01,90,2f,cc */
	{0xa0, 0x9b, ZC3XX_R191_EXPOSURELIMITMID},	/* 01,91,9b,cc */
	{0xa0, 0x80, ZC3XX_R192_EXPOSURELIMITLOW},	/* 01,92,80,cc */
	{0xa0, 0x01, ZC3XX_R195_ANTIFLICKERHIGH},	/* 01,95,01,cc */
	{0xa0, 0xd4, ZC3XX_R196_ANTIFLICKERMID},	/* 01,96,d4,cc */
	{0xa0, 0xc0, ZC3XX_R197_ANTIFLICKERLOW},	/* 01,97,c0,cc */
	{0xa0, 0x07, ZC3XX_R18C_AEFREEZE},	/* 01,8c,07,cc */
	{0xa0, 0x0f, ZC3XX_R18F_AEUNFREEZE},	/* 01,8f,0f,cc */
	{0xa0, 0x18, ZC3XX_R1A9_DIGITALLIMITDIFF},	/* 01,a9,18,cc */
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP},	/* 01,aa,24,cc */
	{0xa0, 0x00, ZC3XX_R01D_HSYNC_0},	/* 00,1d,00,cc */
	{0xa0, 0x50, ZC3XX_R01E_HSYNC_1},	/* 00,1e,50,cc */
	{0xa0, 0x12, ZC3XX_R01F_HSYNC_2},	/* 00,1f,12,cc */
	{0xa0, 0x80, ZC3XX_R020_HSYNC_3},	/* 00,20,80,cc */
	{}
};
static const struct usb_action hv7131b_60HZ[] = {	/* 640x480*/
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},	/* 00,19,00,cc */
	{0xaa, 0x25, 0x0007},			/* 00,25,07,aa */
	{0xaa, 0x26, 0x00a1},			/* 00,26,a1,aa */
	{0xaa, 0x27, 0x0020},			/* 00,27,20,aa */
	{0xaa, 0x20, 0x0000},			/* 00,20,00,aa */
	{0xaa, 0x21, 0x0040},			/* 00,21,40,aa */
	{0xaa, 0x22, 0x0013},			/* 00,22,13,aa */
	{0xaa, 0x23, 0x004c},			/* 00,23,4c,aa */
	{0xa0, 0x2f, ZC3XX_R190_EXPOSURELIMITHIGH},	/* 01,90,2f,cc */
	{0xa0, 0x4d, ZC3XX_R191_EXPOSURELIMITMID},	/* 01,91,4d,cc */
	{0xa0, 0x60, ZC3XX_R192_EXPOSURELIMITLOW},	/* 01,92,60,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},	/* 01,95,00,cc */
	{0xa0, 0xc3, ZC3XX_R196_ANTIFLICKERMID},	/* 01,96,c3,cc */
	{0xa0, 0x50, ZC3XX_R197_ANTIFLICKERLOW},	/* 01,97,50,cc */
	{0xa0, 0x0c, ZC3XX_R18C_AEFREEZE},	/* 01,8c,0c,cc */
	{0xa0, 0x18, ZC3XX_R18F_AEUNFREEZE},	/* 01,8f,18,cc */
	{0xa0, 0x18, ZC3XX_R1A9_DIGITALLIMITDIFF},	/* 01,a9,18,cc */
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP},	/* 01,aa,24,cc */
	{0xa0, 0x00, ZC3XX_R01D_HSYNC_0},	/* 00,1d,00,cc */
	{0xa0, 0x40, ZC3XX_R01E_HSYNC_1},	/* 00,1e,40,cc */
	{0xa0, 0x13, ZC3XX_R01F_HSYNC_2},	/* 00,1f,13,cc */
	{0xa0, 0x4c, ZC3XX_R020_HSYNC_3},	/* 00,20,4c,cc */
	{}
};
static const struct usb_action hv7131b_60HZScale[] = {	/* 320x240 */
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},	/* 00,19,00,cc */
	{0xaa, 0x25, 0x0007},			/* 00,25,07,aa */
	{0xaa, 0x26, 0x00a1},			/* 00,26,a1,aa */
	{0xaa, 0x27, 0x0020},			/* 00,27,20,aa */
	{0xaa, 0x20, 0x0000},			/* 00,20,00,aa */
	{0xaa, 0x21, 0x00a0},			/* 00,21,a0,aa */
	{0xaa, 0x22, 0x0016},			/* 00,22,16,aa */
	{0xaa, 0x23, 0x0040},			/* 00,23,40,aa */
	{0xa0, 0x2f, ZC3XX_R190_EXPOSURELIMITHIGH},	/* 01,90,2f,cc */
	{0xa0, 0x4d, ZC3XX_R191_EXPOSURELIMITMID},	/* 01,91,4d,cc */
	{0xa0, 0x60, ZC3XX_R192_EXPOSURELIMITLOW},	/* 01,92,60,cc */
	{0xa0, 0x01, ZC3XX_R195_ANTIFLICKERHIGH},	/* 01,95,01,cc */
	{0xa0, 0x86, ZC3XX_R196_ANTIFLICKERMID},	/* 01,96,86,cc */
	{0xa0, 0xa0, ZC3XX_R197_ANTIFLICKERLOW},	/* 01,97,a0,cc */
	{0xa0, 0x07, ZC3XX_R18C_AEFREEZE},	/* 01,8c,07,cc */
	{0xa0, 0x0f, ZC3XX_R18F_AEUNFREEZE},	/* 01,8f,0f,cc */
	{0xa0, 0x18, ZC3XX_R1A9_DIGITALLIMITDIFF},	/* 01,a9,18,cc */
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP},	/* 01,aa,24,cc */
	{0xa0, 0x00, ZC3XX_R01D_HSYNC_0},	/* 00,1d,00,cc */
	{0xa0, 0xa0, ZC3XX_R01E_HSYNC_1},	/* 00,1e,a0,cc */
	{0xa0, 0x16, ZC3XX_R01F_HSYNC_2},	/* 00,1f,16,cc */
	{0xa0, 0x40, ZC3XX_R020_HSYNC_3},	/* 00,20,40,cc */
	{}
};
static const struct usb_action hv7131b_NoFliker[] = {	/* 640x480*/
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},	/* 00,19,00,cc */
	{0xaa, 0x25, 0x0003},			/* 00,25,03,aa */
	{0xaa, 0x26, 0x0000},			/* 00,26,00,aa */
	{0xaa, 0x27, 0x0000},			/* 00,27,00,aa */
	{0xaa, 0x20, 0x0000},			/* 00,20,00,aa */
	{0xaa, 0x21, 0x0010},			/* 00,21,10,aa */
	{0xaa, 0x22, 0x0000},			/* 00,22,00,aa */
	{0xaa, 0x23, 0x0003},			/* 00,23,03,aa */
	{0xa0, 0x2f, ZC3XX_R190_EXPOSURELIMITHIGH},	/* 01,90,2f,cc */
	{0xa0, 0xf8, ZC3XX_R191_EXPOSURELIMITMID},	/* 01,91,f8,cc */
	{0xa0, 0x00, ZC3XX_R192_EXPOSURELIMITLOW},	/* 01,92,00,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},	/* 01,95,00,cc */
	{0xa0, 0x02, ZC3XX_R196_ANTIFLICKERMID},	/* 01,96,02,cc */
	{0xa0, 0x00, ZC3XX_R197_ANTIFLICKERLOW},	/* 01,97,00,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},	/* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},	/* 01,8f,20,cc */
	{0xa0, 0x00, ZC3XX_R1A9_DIGITALLIMITDIFF},	/* 01,a9,00,cc */
	{0xa0, 0x00, ZC3XX_R1AA_DIGITALGAINSTEP},	/* 01,aa,00,cc */
	{0xa0, 0x00, ZC3XX_R01D_HSYNC_0},	/* 00,1d,00,cc */
	{0xa0, 0x10, ZC3XX_R01E_HSYNC_1},	/* 00,1e,10,cc */
	{0xa0, 0x00, ZC3XX_R01F_HSYNC_2},	/* 00,1f,00,cc */
	{0xa0, 0x03, ZC3XX_R020_HSYNC_3},	/* 00,20,03,cc */
	{}
};
static const struct usb_action hv7131b_NoFlikerScale[] = { /* 320x240 */
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},	/* 00,19,00,cc */
	{0xaa, 0x25, 0x0003},			/* 00,25,03,aa */
	{0xaa, 0x26, 0x0000},			/* 00,26,00,aa */
	{0xaa, 0x27, 0x0000},			/* 00,27,00,aa */
	{0xaa, 0x20, 0x0000},			/* 00,20,00,aa */
	{0xaa, 0x21, 0x00a0},			/* 00,21,a0,aa */
	{0xaa, 0x22, 0x0016},			/* 00,22,16,aa */
	{0xaa, 0x23, 0x0040},			/* 00,23,40,aa */
	{0xa0, 0x2f, ZC3XX_R190_EXPOSURELIMITHIGH},	/* 01,90,2f,cc */
	{0xa0, 0xf8, ZC3XX_R191_EXPOSURELIMITMID},	/* 01,91,f8,cc */
	{0xa0, 0x00, ZC3XX_R192_EXPOSURELIMITLOW},	/* 01,92,00,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},	/* 01,95,00,cc */
	{0xa0, 0x02, ZC3XX_R196_ANTIFLICKERMID},	/* 01,96,02,cc */
	{0xa0, 0x00, ZC3XX_R197_ANTIFLICKERLOW},	/* 01,97,00,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},	/* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},	/* 01,8f,20,cc */
	{0xa0, 0x00, ZC3XX_R1A9_DIGITALLIMITDIFF},	/* 01,a9,00,cc */
	{0xa0, 0x00, ZC3XX_R1AA_DIGITALGAINSTEP},	/* 01,aa,00,cc */
	{0xa0, 0x00, ZC3XX_R01D_HSYNC_0},	/* 00,1d,00,cc */
	{0xa0, 0xa0, ZC3XX_R01E_HSYNC_1},	/* 00,1e,a0,cc */
	{0xa0, 0x16, ZC3XX_R01F_HSYNC_2},	/* 00,1f,16,cc */
	{0xa0, 0x40, ZC3XX_R020_HSYNC_3},	/* 00,20,40,cc */
	{}
};

static const struct usb_action hv7131cxx_Initial[] = {
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},
	{0xa0, 0x10, ZC3XX_R002_CLOCKSELECT},
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x77, ZC3XX_R101_SENSORCORRECTION},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x07, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x01, ZC3XX_R09B_WINHEIGHTHIGH},
	{0xa0, 0xe8, ZC3XX_R09C_WINHEIGHTLOW},
	{0xa0, 0x02, ZC3XX_R09D_WINWIDTHHIGH},
	{0xa0, 0x88, ZC3XX_R09E_WINWIDTHLOW},
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xaa, 0x01, 0x000c},
	{0xaa, 0x11, 0x0000},
	{0xaa, 0x13, 0x0000},
	{0xaa, 0x14, 0x0001},
	{0xaa, 0x15, 0x00e8},
	{0xaa, 0x16, 0x0002},
	{0xaa, 0x17, 0x0088},

	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
	{0xa0, 0x89, ZC3XX_R18D_YTARGET},
	{0xa0, 0x50, ZC3XX_R1A8_DIGITALGAIN},
	{0xa0, 0x00, 0x01ad},
	{0xa0, 0xc0, 0x019b},
	{0xa0, 0xa0, 0x019c},
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xa1, 0x01, 0x0002},
	{0xa0, 0x00, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x02, ZC3XX_R090_I2CCOMMAND},
	{0xa1, 0x01, 0x0091},
	{0xa1, 0x01, 0x0095},
	{0xa1, 0x01, 0x0096},

	{0xa1, 0x01, 0x0008},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* clock ? */
	{0xa0, 0x08, ZC3XX_R1C6_SHARPNESS00},	/* sharpness+ */
	{0xa1, 0x01, 0x01c8},
	{0xa1, 0x01, 0x01c9},
	{0xa1, 0x01, 0x01ca},
	{0xa0, 0x0f, ZC3XX_R1CB_SHARPNESS05},	/* sharpness- */

	{0xa0, 0x60, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xf0, ZC3XX_R10B_RGB01},
	{0xa0, 0xf0, ZC3XX_R10C_RGB02},
	{0xa0, 0xf0, ZC3XX_R10D_RGB10},
	{0xa0, 0x60, ZC3XX_R10E_RGB11},
	{0xa0, 0xf0, ZC3XX_R10F_RGB12},
	{0xa0, 0xf0, ZC3XX_R110_RGB20},
	{0xa0, 0xf0, ZC3XX_R111_RGB21},
	{0xa0, 0x60, ZC3XX_R112_RGB22},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x10, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0xaa, 0x25, 0x0007},
	{0xaa, 0x26, 0x0053},
	{0xaa, 0x27, 0x0000},

	{0xa0, 0x10, ZC3XX_R190_EXPOSURELIMITHIGH},	/* 2f */
	{0xa0, 0x04, ZC3XX_R191_EXPOSURELIMITMID},	/* 9b */
	{0xa0, 0x60, ZC3XX_R192_EXPOSURELIMITLOW},	/* 80 */
	{0xa0, 0x01, ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0xd4, ZC3XX_R196_ANTIFLICKERMID},
	{0xa0, 0xc0, ZC3XX_R197_ANTIFLICKERLOW},
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},
	{0xa0, 0x60, ZC3XX_R1A8_DIGITALGAIN},
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF},
	{0xa0, 0x13, ZC3XX_R1AA_DIGITALGAINSTEP},
	{0xa1, 0x01, 0x001d},
	{0xa1, 0x01, 0x001e},
	{0xa1, 0x01, 0x001f},
	{0xa1, 0x01, 0x0020},
	{0xa0, 0x40, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{}
};

static const struct usb_action hv7131cxx_InitialScale[] = {
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},

	{0xa0, 0x00, ZC3XX_R002_CLOCKSELECT},	/* diff */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x77, ZC3XX_R101_SENSORCORRECTION},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},

	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x07, ZC3XX_R012_VIDEOCONTROLFUNC},

	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},	/* 1e0 */

	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x01, ZC3XX_R09B_WINHEIGHTHIGH},
	{0xa0, 0xe8, ZC3XX_R09C_WINHEIGHTLOW},
	{0xa0, 0x02, ZC3XX_R09D_WINWIDTHHIGH},
	{0xa0, 0x88, ZC3XX_R09E_WINWIDTHLOW},
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xaa, 0x01, 0x000c},
	{0xaa, 0x11, 0x0000},
	{0xaa, 0x13, 0x0000},
	{0xaa, 0x14, 0x0001},
	{0xaa, 0x15, 0x00e8},
	{0xaa, 0x16, 0x0002},
	{0xaa, 0x17, 0x0088},

	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},	/* 00 */

	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
	{0xa0, 0x89, ZC3XX_R18D_YTARGET},
	{0xa0, 0x50, ZC3XX_R1A8_DIGITALGAIN},
	{0xa0, 0x00, 0x01ad},
	{0xa0, 0xc0, 0x019b},
	{0xa0, 0xa0, 0x019c},
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xa1, 0x01, 0x0002},
	{0xa0, 0x00, ZC3XX_R092_I2CADDRESSSELECT},
						/* read the i2c chips ident */
	{0xa0, 0x02, ZC3XX_R090_I2CCOMMAND},
	{0xa1, 0x01, 0x0091},
	{0xa1, 0x01, 0x0095},
	{0xa1, 0x01, 0x0096},

	{0xa1, 0x01, 0x0008},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* clock ? */
	{0xa0, 0x08, ZC3XX_R1C6_SHARPNESS00},	/* sharpness+ */
	{0xa1, 0x01, 0x01c8},
	{0xa1, 0x01, 0x01c9},
	{0xa1, 0x01, 0x01ca},
	{0xa0, 0x0f, ZC3XX_R1CB_SHARPNESS05},	/* sharpness- */

	{0xa0, 0x60, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xf0, ZC3XX_R10B_RGB01},
	{0xa0, 0xf0, ZC3XX_R10C_RGB02},
	{0xa0, 0xf0, ZC3XX_R10D_RGB10},
	{0xa0, 0x60, ZC3XX_R10E_RGB11},
	{0xa0, 0xf0, ZC3XX_R10F_RGB12},
	{0xa0, 0xf0, ZC3XX_R110_RGB20},
	{0xa0, 0xf0, ZC3XX_R111_RGB21},
	{0xa0, 0x60, ZC3XX_R112_RGB22},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x10, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0xaa, 0x25, 0x0007},
	{0xaa, 0x26, 0x0053},
	{0xaa, 0x27, 0x0000},

	{0xa0, 0x10, ZC3XX_R190_EXPOSURELIMITHIGH},	/* 2f */
	{0xa0, 0x04, ZC3XX_R191_EXPOSURELIMITMID},	/* 9b */
	{0xa0, 0x60, ZC3XX_R192_EXPOSURELIMITLOW},	/* 80 */

	{0xa0, 0x01, ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0xd4, ZC3XX_R196_ANTIFLICKERMID},
	{0xa0, 0xc0, ZC3XX_R197_ANTIFLICKERLOW},

	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},
	{0xa0, 0x60, ZC3XX_R1A8_DIGITALGAIN},
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF},
	{0xa0, 0x13, ZC3XX_R1AA_DIGITALGAINSTEP},
	{0xa1, 0x01, 0x001d},
	{0xa1, 0x01, 0x001e},
	{0xa1, 0x01, 0x001f},
	{0xa1, 0x01, 0x0020},
	{0xa0, 0x40, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{}
};

static const struct usb_action icm105axx_Initial[] = {
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},
	{0xa0, 0x10, ZC3XX_R002_CLOCKSELECT},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},
	{0xa0, 0x0c, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0xa1, ZC3XX_R08B_I2CDEVICEADDR},
	{0xa0, 0x00, ZC3XX_R097_WINYSTARTHIGH},
	{0xa0, 0x01, ZC3XX_R098_WINYSTARTLOW},
	{0xa0, 0x00, ZC3XX_R099_WINXSTARTHIGH},
	{0xa0, 0x01, ZC3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x01, ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x01, ZC3XX_R11C_FIRSTXLOW},
	{0xa0, 0x01, ZC3XX_R09B_WINHEIGHTHIGH},
	{0xa0, 0xe8, ZC3XX_R09C_WINHEIGHTLOW},
	{0xa0, 0x02, ZC3XX_R09D_WINWIDTHHIGH},
	{0xa0, 0x88, ZC3XX_R09E_WINWIDTHLOW},
	{0xa0, 0x37, ZC3XX_R101_SENSORCORRECTION},
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xaa, 0x01, 0x0010},
	{0xaa, 0x03, 0x0000},
	{0xaa, 0x04, 0x0001},
	{0xaa, 0x05, 0x0020},
	{0xaa, 0x06, 0x0001},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x03, 0x0001},
	{0xaa, 0x04, 0x0011},
	{0xaa, 0x05, 0x00a0},
	{0xaa, 0x06, 0x0001},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x03, 0x0002},
	{0xaa, 0x04, 0x0013},
	{0xaa, 0x05, 0x0020},
	{0xaa, 0x06, 0x0001},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x03, 0x0003},
	{0xaa, 0x04, 0x0015},
	{0xaa, 0x05, 0x0020},
	{0xaa, 0x06, 0x0005},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x03, 0x0004},
	{0xaa, 0x04, 0x0017},
	{0xaa, 0x05, 0x0020},
	{0xaa, 0x06, 0x000d},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x03, 0x0005},
	{0xaa, 0x04, 0x0019},
	{0xaa, 0x05, 0x0020},
	{0xaa, 0x06, 0x0005},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x03, 0x0006},
	{0xaa, 0x04, 0x0017},
	{0xaa, 0x05, 0x0026},
	{0xaa, 0x06, 0x0005},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x03, 0x0007},
	{0xaa, 0x04, 0x0019},
	{0xaa, 0x05, 0x0022},
	{0xaa, 0x06, 0x0005},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x03, 0x0008},
	{0xaa, 0x04, 0x0021},
	{0xaa, 0x05, 0x00aa},
	{0xaa, 0x06, 0x0005},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x03, 0x0009},
	{0xaa, 0x04, 0x0023},
	{0xaa, 0x05, 0x00aa},
	{0xaa, 0x06, 0x000d},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x03, 0x000a},
	{0xaa, 0x04, 0x0025},
	{0xaa, 0x05, 0x00aa},
	{0xaa, 0x06, 0x0005},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x03, 0x000b},
	{0xaa, 0x04, 0x00ec},
	{0xaa, 0x05, 0x002e},
	{0xaa, 0x06, 0x0005},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x03, 0x000c},
	{0xaa, 0x04, 0x00fa},
	{0xaa, 0x05, 0x002a},
	{0xaa, 0x06, 0x0005},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x07, 0x000d},
	{0xaa, 0x01, 0x0005},
	{0xaa, 0x94, 0x0002},
	{0xaa, 0x90, 0x0000},
	{0xaa, 0x91, 0x001f},
	{0xaa, 0x10, 0x0064},
	{0xaa, 0x9b, 0x00f0},
	{0xaa, 0x9c, 0x0002},
	{0xaa, 0x14, 0x001a},
	{0xaa, 0x20, 0x0080},
	{0xaa, 0x22, 0x0080},
	{0xaa, 0x24, 0x0080},
	{0xaa, 0x26, 0x0080},
	{0xaa, 0x00, 0x0084},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xaa, 0xa8, 0x00c0},
	{0xa1, 0x01, 0x0002},
	{0xa1, 0x01, 0x0008},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x02, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x40, ZC3XX_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GGAIN},
	{0xa0, 0x40, ZC3XX_R118_BGAIN},
	{0xa1, 0x01, 0x0008},

	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* clock ? */
	{0xa0, 0x08, ZC3XX_R1C6_SHARPNESS00},	/* sharpness+ */
	{0xa1, 0x01, 0x01c8},
	{0xa1, 0x01, 0x01c9},
	{0xa1, 0x01, 0x01ca},
	{0xa0, 0x0f, ZC3XX_R1CB_SHARPNESS05},	/* sharpness- */
	{0xa0, 0x52, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xf7, ZC3XX_R10B_RGB01},
	{0xa0, 0xf7, ZC3XX_R10C_RGB02},
	{0xa0, 0xf7, ZC3XX_R10D_RGB10},
	{0xa0, 0x52, ZC3XX_R10E_RGB11},
	{0xa0, 0xf7, ZC3XX_R10F_RGB12},
	{0xa0, 0xf7, ZC3XX_R110_RGB20},
	{0xa0, 0xf7, ZC3XX_R111_RGB21},
	{0xa0, 0x52, ZC3XX_R112_RGB22},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x00, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0xaa, 0x0d, 0x0003},
	{0xaa, 0x0c, 0x008c},
	{0xaa, 0x0e, 0x0095},
	{0xaa, 0x0f, 0x0002},
	{0xaa, 0x1c, 0x0094},
	{0xaa, 0x1d, 0x0002},
	{0xaa, 0x20, 0x0080},
	{0xaa, 0x22, 0x0080},
	{0xaa, 0x24, 0x0080},
	{0xaa, 0x26, 0x0080},
	{0xaa, 0x00, 0x0084},
	{0xa0, 0x02, ZC3XX_R0A3_EXPOSURETIMEHIGH},
	{0xa0, 0x94, ZC3XX_R0A4_EXPOSURETIMELOW},
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},
	{0xa0, 0x04, ZC3XX_R191_EXPOSURELIMITMID},
	{0xa0, 0x20, ZC3XX_R192_EXPOSURELIMITLOW},
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},
	{0xa0, 0x84, ZC3XX_R197_ANTIFLICKERLOW},
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF},
	{0xa0, 0x12, ZC3XX_R1AA_DIGITALGAINSTEP},
	{0xa0, 0xe3, ZC3XX_R01D_HSYNC_0},
	{0xa0, 0xec, ZC3XX_R01E_HSYNC_1},
	{0xa0, 0xf5, ZC3XX_R01F_HSYNC_2},
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0xc0, ZC3XX_R1A8_DIGITALGAIN},
	{0xa0, 0xc0, ZC3XX_R11D_GLOBALGAIN},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x40, ZC3XX_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GGAIN},
	{0xa0, 0x40, ZC3XX_R118_BGAIN},
	{}
};

static const struct usb_action icm105axx_InitialScale[] = {
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},
	{0xa0, 0x00, ZC3XX_R002_CLOCKSELECT},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},
	{0xa0, 0x0c, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0xa1, ZC3XX_R08B_I2CDEVICEADDR},
	{0xa0, 0x00, ZC3XX_R097_WINYSTARTHIGH},
	{0xa0, 0x02, ZC3XX_R098_WINYSTARTLOW},
	{0xa0, 0x00, ZC3XX_R099_WINXSTARTHIGH},
	{0xa0, 0x02, ZC3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x02, ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x02, ZC3XX_R11C_FIRSTXLOW},
	{0xa0, 0x01, ZC3XX_R09B_WINHEIGHTHIGH},
	{0xa0, 0xe6, ZC3XX_R09C_WINHEIGHTLOW},
	{0xa0, 0x02, ZC3XX_R09D_WINWIDTHHIGH},
	{0xa0, 0x86, ZC3XX_R09E_WINWIDTHLOW},
	{0xa0, 0x77, ZC3XX_R101_SENSORCORRECTION},
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xaa, 0x01, 0x0010},
	{0xaa, 0x03, 0x0000},
	{0xaa, 0x04, 0x0001},
	{0xaa, 0x05, 0x0020},
	{0xaa, 0x06, 0x0001},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x03, 0x0001},
	{0xaa, 0x04, 0x0011},
	{0xaa, 0x05, 0x00a0},
	{0xaa, 0x06, 0x0001},
	{0xaa, 0x08/*
 *000},
	{0xaa/*
 *3Z-Star/2imicro zc301/z4Z-Star13imicro zc301/z5Z-Star2Vimicro zc301/z6Z-Star/1imicro zc301/z	Z-Star/Vimicro zc301/zc302p/vct (C) 2004 2005Copyrigh5 (C) 2004 2005 2006 Michel Xhaard
 *		mxhaard *
 * This prog	Z-Star/Vimicro zc301/zc302p/vc4tp://moinejf.free.fr>
7 (C) 2004 2005 2006 Michel Xhaard
 *		mxhaardddistribute it and/or modify
 * it under the t *
 * Thi0ejf.freeZC3XX_R092_I2CADDRESSSELECTnse, or
 * (a19your option3 anySETVALUEnse, or
 * (at1your option0 anyCOMMANDimicro z1 that itStar9@magic.fr
 *
 *ram is free software; you can redistribute it and/or modify
 * it under the t6rms of the GNU General Public License as publSE.  See the
 *ou can redistribute it and/or modify
 * it under the t Public LicenseCopyrigh9; without even the imp30x library
 *	ou can redistribute it and/or modify
 * it under the t8l Public License
 * a2Y; without even the imaaf not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave,ong with this p02139, Ut (C) 2004 2005 2006 ME_NAME "zc3xx"

#includeftware Foundation; either version 2 of the Lic_NAME "zc3xx"

02139, U *
 * This program is E_NAME "zc3xx"

#include "gspca.h"
#include "jpeg.h"

MODULE_AUTHOR("MbCA ZC03xx/VC3xx USB Cec; without even the impef not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave,
#include "zc3xCopyrigf_NAME "zc3xx"

 2006 Mi_NAME "zc3xx"

#include "gspca.h"
#include "jpeg.h"

MODULE_A7chkov <Serge.A.S@tochk WARRANredistribute it9Copyrigc30x library
 *9 * (atr/Vimicro zc301/9 WARRAN1Vimicro zc301/1ine QUA6erms of the GN9bt be thITY_MIN 40
#defc image quality */
#de1Copyrigh_NAME "zc3xx"
2ine QUA8Vimicro zc301/22n different tables */
Copyrigerent tables */
	mxhaarerent tables */0in diffeerms of t * (at8your opti250_DEADPIXELSMODn the hope that0305 3
#de301_EEPROMACCESSimicro zc301/a	Z-Starerent tabl * (a70305 3
#de18D_YTARGEon.
 *
 *UT ANY WARRANc30x libra#define SENSOR CambridgUT ANY WARRA1ne SENSOR_ICM1002 7
#define 0_AUTOCORRECTENABLn the hope tha40 7
#define16_RGAINR_PAS106 11
#define SENSOR7_GS202B 12
#define SENSOR_PB03308_BS202B 12
#de
/*#define SENSOR_ne SENSOR_GC3your optio08_CLOCKSETTING},	/* clock ? */ne SENSOR_GC0305 3
#de1C6_SHARPNESS/Vim30C_sharpness+17
#defin8 9 - same vacSOR_OV7648 9 - same vacong with rols supported_NAME "zc * (atfR_MAX 18
	uBsigned shor5 chip_revision;-17
#nt sd_setbr5SENSOR_OV760A_RGBrt chip_matrix17
#define SENf7t sd_getbriBhtnes@magic.frpca_dev, __s32 *vaChtnes30x librapca_dev, __s32 *vaDhtneLITY_MAX 6atic int sd_getbriEl);
satic int sd_setcontrast(strFl);
spca_dev *gspca_dev, __s32 10htneichel Xhaant sd_setautogain(1tructatic int sd_seint sd_getbr12truct_OV7620 9
/*#define Salues */
#define efine SENSO30C 10
#define SENSOR_PAS106 11
#static int 019C 10
ADJUSTFPSENSOR_HV7131C0doine <http://moinejf.fype of ichel Xhaard
 *	e SENSOR

/* specific wfpe of image sensor chiype of iftware Foundat1struct g30x library
 * in different tables */
#define SENSOR_ADCM2700 0
#define SENSOR_CS2102 1
#define SENSOR_CS2102K 2
#define SENSOR_GCSENSOR_OV70A3_EXPOSURETIMEHIGH int sd_setbridsharpness(s4ruct gspca_deLOW__s32 *val);
static int s90ruct gspcLIMITv *gspca_dev, __s3 your opti191HTNESS_IDX 0
#MIbut WITHO This aS 0
	{
	   2HTNESS_IDX 0
#rl sd_ctrls[] = {
#define BRI5_ANTIFLICKERv *gspca_dev, __s3
#define BRI6rightness",
 V4L2_CID_BRIGHT4bS 0
	{
	   7rightness",
rl sd_ctrls[] = fine
#define C_AEFREEZn the hope tha in 
#define F_AEUNget = sd_getbrightnrightness,
	A9_DIGITALX 0
#DIFFL2_CID_BRIGHTNSENSOR_OV76AA= V4L2_CS202STEPL2_CID_BRIGHTc0305 3
#de01D_HSYNC_ gspca_dev *gsdntrast",
		.Einimum atic int sd_seeESS,
		.ty01Finimum pca_dev *gspca_ghtness(st020inimum t (C) 200 0,
		.maximum = A7_CALCGLOBALMEA2B 12
#define SSENSOR_OV7630C 10
#define SENSOR_PAS106ev *gspca_dev, __s32 *val);
	.id      = V4L2_CID_GAMMA,
		.type    = 11
#define SENSOR_PAS202B 12
#define SENSOR_PB0330 13
#define SENSOR_PO2030 14
#define SENSOR_T}
};
static const struct usb_action icm105a_50HZ[] = {a_dev *gspca_dev, __s32 val);
static int  /* 00,19,00,cc17
#defingamma(struct gspc2_CTRL_0d,03,aaLEAN,
		.name  _dev, __s32ain",
		c,20imum = 0,
		.maximic int sd_sain",
		e .semum = 0,
		.maximt gspca_devain",
		f,02= sd_setautogain,, __s32 val)2_CTRL_Tc		.min,
	},
#define L sd_getfreq2_CTRL_Tdtogain,
	},
#define  in differen2_CTRL_20,8,
		.default_value
#define SEN = V4L2_2TRL_TYPE_MENU,
		.nam 0
#define  = V4L2_4TRL_TYPE_MENU,
		.nam2 1
#define = V4L2_6TRL_TYPE_MENU,
		.na102K 2
#defiain",
		0,84SD_FREQ 4
	{ sd_getsharpness(struct gspca_dev *gsp2_CTRL_a3togaOOLEAN,
		., __s32 *val);

static struct ctrl sdne SD_SH4		.mSS 5
	{
	    {
		
#define BRIGHTNESS_IDX 0
#define2_CTR1,9	    SS 5
	{
	    {
		SS 0
	{
	    {
		.id      = V4L2inimum =1,04		.maximum = 3,
	NESS,
		.type    = V4L2_CTRL_TYPEinimum =2,1a2_CTRL_TYPE_INTEGER,
		.name  Brightness",
		.mininimum =5PE_BOOLEAN,
		. 0,
		.maximum = 255,
		.step    = 1inimum =620, 240, V4L2_PIX_FMt_value = 128,
	    },
	    .seinimum =7,4b	    .set = sd_setightness,
	    .get = sdinimum 8c,1
		.maximum = 3,
	ess,
	},
#define SD_CONTRAS, 480, Vf2_CTV4L2_COLORSPACE_JPEG,
		.pri  = V4L2_CID_CONTRASTinimum a9L2_PIX_FMT_JPEG, V4L  = V4L2_CTRL_TYPE_INTEGER,
		.ne = V4L2a,1NESS 5
	{
	    {
	ontrast",
		.minimum = 0V4L2_CID_Pc8.sizeimage = 640 m = 256,
		.step    = 1,2_CTRL_Te,d V4L2_FIELD_NONE,
_value = 128,
	    },
	 2_CTRL_Tf,e
};

static const d_setcontrast,
	    .get = V4L2_CTff.sizeimag_getgamma,
	},
#define SD_AUTOGAIN 3
	{
	    {
	Scale	.id      = V4L2_CID_AUTOGAIN,
		.type    = V4L2_CTRL_TYPE_BOOLEAN,
		.name    = "Auto Gain",
		.minimum = 0,
		.maximum = 1,8c	.step    = 8c
		.default_value = 1,
	 95 },
	    .se95= sd_setautogain,
	    .get = sd_getautogain,
	},
#define LIGHTFRE9	    },
	 1c,9set = sd_setf
	    {
		.id	 = V4L2_CID_POWER_LINE_FREQUENCY,
		.type    = V4L2_CTRL_TYPE_MENU,
		.name    = "Light frequency filter",
		.minimum = 0,
		.maximum = 2,	/* 0: 0, 1: 50Hz, 2:60Hz */
		.step    = 1,
		.default_value = 1,
	    },
	    .set = sd_setfreq,
	    .get = sd_getfreq,
	},
#define SD_SHARPNESS 5
	{
	    {
	/* i	 = V4L2_CID_SHARPNESS,
		.type    = V9
	    .set = sd_seER,
		.name    = "Sharpness",
		.minimum = 0,
		.maximum = 3,
		.step    = 1,
		.default_value = 2,
	    },
	    .set = sd_seess,
	},
#de	    .get = sd_getsharpness,
	}		.sizeimage = 640 struct v4l2_pix_format vga_mode[] = {
	{320, 240, V4L2_PIX_FMT_JPEG, V4L2_FIELD_NONE,
		.bytesperline = 320,
		.sizeimage8SS 0
	{
	   * 3 / 8 + 590,
		.colorspace8
	    .set = sd_setPEG,
		.priv = 1},
	{640, 480, V4L2_PIX_FMT_JPEG, V4L2_FIELD_NONE,
		.bytesperline = 640,
		.sizeimage = 640 * 480 * 3 / 8 + 590,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.priv = 0},
};

static const struct v4l2_pix_format sif_mode[]e0CXX 16
#de144, V4L2_PIX_FMT_JPEGe3 0xde, ZC3XX_R09C_c.bytesperline = 176,
		.sizeimagecSPACE_JPEG,
		.pri5 + 590,
		.colorspace = V4L2_COLf5SPACE_JPEG,
		.priv = 1},
	{352, 288, V4L2_PIX_FMT_JPEG, V4L2_sd_getcontrast,
	},
#define SD_GAMMA 2
e = V4L2720, 240, V4L2_PIX_FMc 480 * 3 / 88TYPE_INTEGER3XX_R101_S8,c	.sizeimag_getgamma,
	},
#define SD_AUTOGAIN 3
	{
	   6{
		.id      = V4L2_CID_AUTOGAIN,
		.type    = V4L2_CTRL_TYPE_BOOLEAN,
		.name    = "Auto Gain",
		.minimum = 0,
		.maximum = 1,termstep    = 0,cc */
	{0xa0, 0x0 = 1,
	  _IDX 4
#de.set SD_FREQ 4
	{
	   
	    .get = sd_getautogain,
	},
#define LIGHTFREQ8IDX 4
#define8SD_FREQ 4
	{
	    {
		.id	 = V4L2_CID_POWER_LINE_FREQUENCY,
		.type    = V4L2_CTRL_TYPE_MENU,
		.name    = "Light frequency filter",
		.minimum = 0,
		.maximum = 2,	/* 0: 0, 1: 50Hz, 2:60Hz */
		.step    = 1,
		.default_value = 1,
	    },
	    .set = sd_setfreq,
	    .get = sd_getfreq,
	},
#define SD_SHARPNESS 5
	{
	    {
		ntrast",
		_CID_SHARPNESS,
		.type    = V4 V4L2_FIELD_NONE,
ER,
		.name    = "Sharpness",
		.minimum = 0,
		.maximum = 3,
		.step    = 1,
		.default_value = 2,
	    },
	    .set = sd_setYSTEMOPERATING},	/* 00,01,01,cc */
	{0xa0,2_PIX_FMT_JPEG, V4Lstruct v4l2_pix_format vga_mode[] = {
	{320, 240, V4L2_PIX_FMT_JPEG, V4L2_FIELD_NONE,
		.bytesperline = 320,
		.sizeimage  it will b0 * 3 / 8 + 590,
		.colorspace 1.sizeimage = 640 * 480 * 3 / v = 1},
	{640, 480, V4L2_PIX_FMT_JPEG, V4L2_FIELD_NONE,
		.bytesperline = 640,
		.sizeimage = 640 * 480 * 3 / 8 + 590,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.priv = 0},
};

static const struct v4l2_pix_format sif_mode[]  it will be144, V4L2_PIX_FMT_JPEG,/
	{0xaa, 0xfe, 0xd004_FRAMEWIine = 176,
		.sizeimage
	    .set = sd_seentrast",
		.colorspace = V4L2_COLO V4L2_FIELD_NONE,
iv = 1},
	{352, 288, V4L2_PIX_FMT_JPEG, V4L2_FIELD_NONE,
		.bytesperline = 352,
		.sizeimX_R1= 352 * 288 * 3 / 8 + 590,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.priv = 0},
};

/* usb exchanges */
struct usb_action {
	__u8	rS},		/* 03, 01,8,cc */
	{0xa0, 0x = 1,
	 86 },
	    .se86= sd_setautogain,
	    .get = sd_getautogain,
	},
#define LIGHTFRE8t struct u1c,8ction adcm2700_Ini {
		.id	 = V4L2_CID_POWER_LINE_FREQUENCY,
		.type    = V4L2_CTRL_TYPE_MENU,
		.name    = "Light frequency filter",
		.minimum = 0,
		.maximum = 2,	/* 0: 0, 1: 50Hz, 2:60Hz */
		.step    = 1,
		.default_value = 1,
	    },
	    .set = sd_setfreq,
	    .get = sd_getfreq,
	},
#define SD_SHARPNESS 5
	{
	    {
	80x0400},			_CID_SHARPNESS,
		.type    = V8dd, 0x00, 0x0010},ER,
		.name    = "Sharpness",
		.minimum = 0,
		.maximum = 3,
		.step    = 1,
		.default_value = 2,
	    },
	    .set = sd_seSOR_MAX 18
		    .get = sd_getsharpness,
	}
	{0xdd, 0x00, 0x0010},				/* 00NC},	/* 00,12,03,cc */
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,01,cc */
	{0xa0, 0x05, ZC3XX_R012{0xdd, 0x00, 0x0010},				/* 00,00,10,dd 8/
	{0xaa, 0xfe, 0x0002},				/* 00,fe,02,aa */
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xbb, 0x5f, 0x2090},				/* 20,5f,90,bb */
	{0xbb, 0x01, 0x8000},				/* 80,01,00,bb */
	{0xbb, 0x09tsharpness(144, V4L2_PIX_FMT_JPEG,NESS 5
	{
	    {
	d6				/* 00,86,02,bb */
	{0xbb, 0xe6= 176 * 144 * 3 / 8 + 590,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.priv = 1},
	{352, 288, V4L2_PIX_FMT_JPEG, V4L2_			/* 14,0f,0f,bb */
	{0xa0, 0xb7, ZC3XX_R101_SENSORCORRECTION},	/* 01,01,37,cc */
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},		/* 01,00,0d,cc */
	{0xa0, 0x06, ZC3XNoFliker89_AWBSTATUS},		/* 01,89,06,cc */
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},		/* 01,c5,03,cc */
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},		/* 01,cb,13,cc */
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},	/* 02,50,08,cc */
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS_PIX_FMT_JPc    c */
	{0xaa, 0xfe, 0x0020},				/* 00,fe,20,aa */
/*mswin+*/
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},
	{0xaa, 0xfe, 0x0002},
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},
	{0xaa, 0xb4, 0xcd37},
	{0xaa, 0xa4, 0x0004},
	{0xaa, 0xa8, 0x0007},
	{0xaa, 0xac, 0x0004},
/*mswin-*/
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,a_dev, __s32_CID_SHARPNESS,
		.type    = V46, 0x2400},				/* 24,96,00,bb *HTHIGH},	/* 00,05,01,cc */
	{0xa0, 0xd8, ZC3XX_R006_FRAMEHEIGHTLOW},	/* 00,06,d8,cc */
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},	/* 00,01,01,cc */
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,03,cc */
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,01,cc */
	{0xa0, 0x05, ZC3XX_R011, 0x2000},		* 3 / 8 + 590,
		.colorspaceOLORSPACE_JPEG,
		.pPEG,
		.priv = 1},
	{640, 480, V4L2_PIX_FMT_JPEG, V4L2_FIELD_NONE,
		.bytesperline = 640,
		.sizeimage = 640 ntrast,
	},
# + 590,
		.colorspace = V4L2_C0,cc */
	{0xa0, 0x00, ZC3XX_R09;

static const struct v4l2_piSORCORRECTION},	/* 0, 0x8400},				/* 84,09,00,bb */
	{0xbb, 0x86, 0x0002},				/* 00,86,02,bb */
	{0xbb, 0xe6, 0x0401},				/* 04,e6,01,bb */
	{0xbb, 0x86, 0x0802},				/* 08,86,02,bb */
	{0xbb, 0xe6, 0x0c01},				/* 0c,e6,01,bb */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELE10_CMOSS	/* 00,10,01,cc */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xaa, 0xfe, 0x0000},				/* 00,fe,00,aa */
	{0xa0, 0x0a, ZC3XX_R010SS05},		/* 01,cb,13,cc */
	{0xa0, 0x08, 8,
		.sizei 0x0l2_p,				/* 00,00,10,dd */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECTIGH},	/* 00,0RL_TYPE_MENU,
		.nae, 0x0020},				/* 00,fe,20,aa */
/*mswin+*/
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},
	{0xaa, 0xfe, 0x0002},
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},
	{0xaa, 0xb4, 0xcd37},
	{0xaa, 0xa4, 0x0004},
	{0xaa, 0xa8, 0x0007},
	{0xaa, 0xac, 0x0004},
/*mswin-*/
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,100, ZC3XX_R098_WINYSTARTLOW},		/* 00,98,08,cc */
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},		/* 00,9a,00,cc */
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},		/* 01,1a,00,cc */
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},		/* 01,1c,00,cc */
	{0xa0, 0xd8, ZC3XX_R09C_WINHEIGHTLOW},		/* 00,9c,d8,cc */
	{0xa0, 0x88, ZC3XX_R09E_WINWIDTHLOW},		/* 00,9e,88,cc */
	{0xbb, 0x00, 0x0400},				/* 04,00,00,bb */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xbb, 0x0f, 0x140f},				/* 14,0f,0f,bb */
	{0xa0, 0xb7, ZC3XX_R101_SENSORCORRECTION},	/* 01,01,37,cc */
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},		/* 01,00,0d,cc */
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},		/* 01,89,06,cc */
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},		/* 01,c5,03,cc */
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},		/* 01,cb,13,cc */
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},	/* 02,50,08,cc */
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},				/* 14,0f,0f,bb */
	{0xa0, 0xb7, ZC3XX_R101_SENSORCORRECTION},	/* 01,01,37,cc */
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE}gamma,
	},
#define SD_AUTOGAIN 3MC50ructInitial= 352 * 288 * 3 / 8 + 5 it will be00_SYSTEMCONTROL    },
	    0/
	{0xaa, 0xfe, 0xa_dev, __s3202ne SENSOrsion.,		/* 0020,0d,cc */
	{0xa0, 0x, 0x8400},		0_CMOSSENSORe,00,aa */
	{0x10,dd */
	{0xaa, 0xfe, 
	{0xdd, 0x01, 0x001OPERA_TAS51,		/* 001,dd */
	{0xaa, 0xfe, 0CXX 16
#define SENSOR_TAS51,		/* 008mini0a, ZC3XX_R010_CMOSSENSORSELE2_VIDEO0},				FUNCe = V4L2_C2,dd */
	{0xaa, 0xfe, 0x0400},				/* 04,01,00,bb */
	{0xa0, 0x01,NSORSELECT},	/* 00,tsharpness(03_FRAMEWIDTH
#define SD_S0	/* 10,06,06,bb */
	{0xa0, 0x01, 04{0xa0, 0x0a
		.type    0,	/* 00,10,01,cc */
	{
	{0xdd, 0x05{0xa0,HEIGH",
		.minimu0e, 0dd */
	{0xaa, 0xfe,m = 256,
		.060x2c03},				41, 0x2803},	6ge = 176 * 144 * 3 /a_dev, __s3298_WINYSTAR,	/* 00,10,01980,0d,cc */
	{0xa0, 0x06, ZC3XX_09A00,fX,10,aa */
	{}
};
s,cc */
	{0xa0, 0x03, ER,
		.name 1A_FIRSTY,
		.colorsp10x01, ZC3XX_R010_CMOSSENSORSELECT}C	/* 00X10,01,cc */
	,05,0 28,41,03,bb */
	{0xbb, 0x409B00,f},				/* 2c,40,03,b9b*/
	{0xa0, 0x01, ZC3Xeyour optionC00,10,0a,ca */
	{}
};
sc,de0x0002},				/* 00,fe,02,aa */9D00,f 0x0a, ZC3XX_R010_9_POWERSELECT},	/* 00,1/
	{0xaa, 09E0xb80f},	a */
	{}
};
s0x001240, V4L2_PIX_FM30CXX 16
#de86ruct
	},
#define SD_S86,3d */
	{0xbb, 0x01,3004_FRAMEWI87			/* 00	.bytesper0,87,3
	    .set = sd_se30x0400},			88			/* 00
		.type    88,3dd, 0x00, 0x0010},b_action adc8B anyDEVICE latonst strucb,bC3XX_R010_CMOSSENSOSORSELECT},	/* 00,10,01,cc */
	{0xaa, 0xfe, 0x0002},				8 sharpness;
	DR},		/* 00				/3,cc */
	{0xbb, 0e SENSORo Gain",
		1minimum = 0,
		.maxima0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/c302p/vc		.step    305,01,cc */
	{0xa0, 0ine QUALITY*/
	{0xdd, ,01,cc */
	{0xa0, 0 WARRAN 0x0200},			1/* 00,02,00,dd */
	{0#defineHIGH},	/* 000, 0x0,02,00,dd */
	{0,				/* b0,82,06,b1 */
	{0xbb, 0x04, 0xb8* image /* b8,04,0d,00,c0,02,00,dd */
	{0 2006 Ma */
	{0xaa,320, 0,02,00,dd */
	{0	mxhaard/* b8,04,0de = 30,02,00,dd */
	{08 lightf,
		.sizeim7_CMOSSENSORSELECT},	1	Z-Stard  },
	    18		/*SENSORSELECT},	/*9
		.id	 = V4L2_CID9togain,
	},
#define Llt_valu0xdd, 0x00,1ax0010},				/* 00,00, in diffa},		/* 03,1,
	8,cc */
	{0xa0, 0x
#definetion adcm272 0x0010},				/* 00,fe2,				/* b0,82,06,b2 */
	{0xbb, 0x04, 0xbmum = 0,0xbb, 0xa0,  0x28, 0x0002},				/*defiOSSE3V4L2_PIX_F400xaa3XX_R010_CMOSSENS WARRAN77,	/* 00,101,77cc */
	{0xaa, 0xfe#define5},	/* 00,102,5,cc */
	{0xaa, 0xfec302p/vb/* b8,04,043OSSE3XX_R010_CMOSSENSgned ch, ZC3XX_R014x05, 3,cc */
	{0xbb, 7#define
		.step   70, 0x01, ZC3XX_R000_SY,				/* b0,82,06,b7 */
	{0xbb, 0x04, 0xb0xa0,				/* b8,04,08,bb */
	{0xa0, 0x01, 10,0c conLECT},
	{0x5,50x10, ZC3XX_R002_Cfine QUA7/* b8,04,091,70, 0x20, ZC3XX_R080#define7ce = V4L2_92xa0,bb, 0x82, 0xb006},				/* DR},		/* 003CMOSSENSORSELECT},	/*{0xbb, 0/* b8,04,0d,ba */
	{0xa0, 0x01, ZC3XX_R0, ZC3XX_R0100_CMOSSENSORSELECT},	3SENSORSELECT},
	{03,bb */
	{0xa0, 0x01, 6SENSORSELECT},
	{06,bb */
	{0xa0, 0x01, PIX_01, ZC3XXA_LAST0xa0,STATrline = XX_R01aMELOW},
	{0xa0, 0x0xa0, 0x/* b8,04,0a1PTIMELOW},
	{0xa0, 0x#define3f.type    =2,3fMELOW},
	{0xa0, 0xc302p/v2},		/* 03,a3,28,cc */
	{0xa0, 0xaCopyrigh},	/* 00 */4C3XX_
	{0xa0, 0x02, Z 2006 MicheOCONTROL5 1,
		.default_valueb WARRAN4SS05},		/*b1,4,cc */
	{0xa0, 0x0dSENSORSE,
		.sizeid
	{0xGH},
	{0xa0, 0xe0C3XX_R01t struct ud101,cc */
	{0xaa, 0xfde    = "Light freqdency filter",
		.minidc302p/v_R09A_WINXST3RTLOW},
	{0xa0, 0x00,um = 0,
		.maximumd= 2,	/* 0: 0, 1: 50HzdYSTEMOP_FIRSTXLOW},5/* 00,02,00,dd */
	{01,0OSSEcV4L2_PIX_F0_OP,cc */
	{0xaa, 0xf, 0xXX_R005_FRAMEHEc2HTHIGH},
	{0xa0, 0xecCopyrig404_FRAMEWIc410,0, 0x00e9},
	{0xaaZC3XX_R004_FRAMEWIcTHLOW},
	{0xa0, 0x01,c	mxhaard},		/* 03,c6ELECT},	/* 00,10,0a,cer the termX_R010_CMOS,cc */
	{0xa0, 0x04SENSORSELECT},
	{0d,bb */
	{0xa0, 0x01, SORSELECT
	{0xa1, 0xR087_c */
	{0xaa, 0xfe, 0x001004_FRAMEWI41 1,
		.default_valuetatic co2_IDX 4
#de42,2PIXELSMODE},	/* 02,Moine <httpX_R010_CMOS,cc */
	{0xaa, 0xf, __s32 ERATING},
	1Cxa0, 0x20, ZC3XX_R08ZC3XX_R0123, ZC3XX_R08b,d3,cc */
	{0xbb, 3gned ch1_IDX 4
#de3b,1D0x01ae},
	{0xa0, 0ype of 4q;
	__u8	v3c,4C0x01ae},
	{0xa0, 0struct 1},		/* 03,3d{0xax01ae},
	{0xa0, 0ic int 6aX_R18D_YTAe,6AOSSENSORSELECT},	/* 00,10,		.step    
	{0xa0, 0x03, ZC3XX_int be thOCONTROLFU52,FFet = sd_setfreq,
	ID_AUTOGAIN,
		.type    = V4L2_CTRL_TYPE_BOOLEAN,
		., __s32 *val);

100_xbb, 0xONCS2020inimum ,
		.m02},				/* 00,28,v, __s32 *va1_* 00,1#defineIO0d, ZC3XX_01,373XX_R010_CMOSSENSO/
	{0xaa, 189_AWBEADDUV4L2_CTR1,891,cc* 04,04,00,bb */
	{0xdd, 0x1C5signed sho	{0xa1, 0x01,c5,dd */
	{0xbb, 0x01,,10,ness(struct gspca_dev *g_R122_GAb0x260x0a, ZC3XX_R010_CMOSSENSORfine SENSOR_HDCS20202_CTR2xa0,
	{0xdd, 0x00, 0x001SENSOR_HV7131B 5
#define SEN2_CTR ZC3X
	{0xdd, 0x00, 0x001SENSOR_OV7630C 10
#define SENSOR_5 */
	{00xa0,OOLEAN,
		.name  c302p/vc30xX_R010_CMOSSE3, ZC3XX_R008_CLOOCONTROL},				/* 051,2,10,aa */
	{}
};
sOCKSETTI004_FRAMEWI50, 0x10, ZC3XX_R002_CAWBSTATUS},
	{0xa0, 0x03, ZC3XX_R1C5_SHAR50000},
03_FRAMEWID5ZC3XX_},
	{0xa0, 0xfb, ZC3XX_xa0, 0xff, Z13XX_R12F_GAMMA0F},
	{C3XX_R003_FRAMEWID5HHIGH},
	{0xa0, 0x80,5YSTEMOPC3XX_R131_GA53XX_R12F_GAMMA06,02,bb */
	{99	.colorspa9,F0, 0x05, ZC3XX_R012R133_GAM	{0xa1, 1,9A	/* 00,10,ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x03, ZC3XX_R1C5_SHARP0000},
fq;
	__u8	v10,f	__u16	idx;
};

st3	mxhaar3XX_R250_DEA6PIXELSMODE},
	{0xa0, 8 lightC3XX_R301_EE7ROMACCESS},
	{0xa0, 0x08, ZC3XX_R250_DEABPIXELSMODE},0,10,0a,cc */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSEL	.id  	2_CT320,0108,41,03,bb */
	{0xbb, 0x40,0, 0x0010},				/* 00,00,10,dd */
	{0xaa, 0xfe,xbb, 0x0f, 		/* 00,fe,00,aa */
	{0xa0,96, 0x2400},				/* 2OSSENSORSELECT},	/* 00,10,0a,cc */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xbb, 0x04, 0x0400},				/* 04,04,00,bb */
	{0xdd, 0x00, 0x0100},				/* 00,01,00,dd */
	{0xbb, 0x01, 0x0400},				/* 04,01,00,bb */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xaa, 0xfe, 0x0002},				/* 00,fe,02,aa */
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xbb, 0x41, 0x2803},				/* 28,41,03,bb */
	{0xbb, 0x40, 0x2c03},				/* 2c,40,03,bb */
	{0xa0, 0x01, ZC3XF_GAMMA1F},
ENSORSELECT},	/* 00,10,01,cc ic const struct usb_action adcm 00,fe,10,aa */
	{}
};
static const struct usb_action adcm2700_50HZ[] = {
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xaa, 0xfe, 0x0002},				/* 00,fe,02,aa */
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xbb, 0x05, 0x8400},				/* 84ntrast",
		/
	{0xbb, 0xd0, 0xb007},				07},
	{0xa0, 0xe0, ZC3XX_R1280, 0xb80f},				/* b8,a0,0f,bb */
	{0xa0, 0x01, ZALLIMITDIFF}OSSENSORSELECT},	/* 00,10, V4L2_FIELD_NONE,
xfe, 0x0010},				/* 00,fe,10,aa */
	{0xaa, 0x26, 0x00d0},				/* 00,26,d0,aa */
	{0xaa, 0x28, 0x0002},				/* 00,28,02,aa */
	{}
};
static const struct usb_action adcm2700_60HZ[] = {
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xaa, 0xfe, 0x0002},				/* 00,fe,02,aa */
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xbb, 0x07, 0x8400},				/* 84,07,00,bb */
	{0xbb, 0x82, 0xb006},				/* b0,82,06,bb */
	{0xbb, 0x04, 0xb80d},				/* b8,04,0d,bb */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xaa, 0xfe, 0x0010},				/* 00,fe,10,aa */
	{0xaa, 0x26, 0x0057},				/* 00,26,57,aa */
	{0xaa, 0x28, 0x0002},				/* 00,28,02,aa */
	{}
};
static const struct usb_action adcm2700_NoFliker[] = {
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0},		/* 03,08A_DI0xfe, 0x0002},				/* 00,fe,02,aa */
	{0xa0, 0x0a, ZC3XX_R010_CMOSS},		/* 03,0aHSYN 00,10,0a,cc */
	{0xbb, 0x07, 0x8400},				/* 84,07,00,bb */
	{0xbb, 0x05, 0xb000},				/* b0,05,00,bb */
	{0xbb, 0xa0, 0xb801},				/* b8,a0,01,bb */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xaa, 0xfe, 0x0010},				/* 00,fe,10,aa */
	{}
};
static const struct usb_action cs2102_Initial[] = {
	{0xa1, 0x01, 0x0008},
	{0xa1, 0x01, 0x0008},
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},
	{0xa0, 0x10, ZC3XX_R002_CLOCKSELECT},
	{0xa0, 0x00, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x20, ZC3XX_R080_HBLANKHIGH},
	{0xa0, 0x21, ZC3XX_R081_HBLANKLOW},
	{0xa0, 0x30, ZC3XX_R083_RGAINADDR},
	{0xa0, 0x31, ZC3XX_R084_GGAINADDR},
	{0xa0, 0x32, ZC3XX_R085_BGAINADDR},
	{0xa0, 0x23, ZC3XX_R086_EXPTIMEHIGH},
	{0xa0, 0x24, ZC3XX_R087_EXPTIMEMID},
	{0xa0, 0x25, ZC3XX_R088_EXPTIMELOW},
	{0xa0, 0xb3, ZC3XX_R08B_I2CDEVICEADDR},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* 00 */
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},
	{0xaa, 0x02, 0x0008},
	{0xaa, 0x03, 0x0000},
	{0xaa, 0x11, 0x0000},
	{0xaa, 0x12, 0x0089},
	{0xaa, 0x13, 0x0000},
	{0xaa, 0x14, 0x00e9},
	{0xaa, 0x20, 0x0000},
	{0xaa, 0x22, 0x0000},
	{0xaa, 0x0b, 0x0004},
	{0xaa, 0x30, 0x0030},
	{0xaa, 0x31, 0x0030},
	{0xaa, 0x32, 0x0030},
	{0xa0, 0x37, ZC3XX_R101_SENSORCORRECTION},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, cx13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x10, 0x01ae},
	{0xa0, 0x08, ZC3	{0xa1, 0x0b,30x0002},
	{0xa1, 0 0x08, Z9XX_R18D_YTAc
staCCESS},
	{0xa0, 0x68, ZCPS},
	{0xa03d0x05, ZC3XX_R012_VID0x01ad},d
	{0xa0, 03E,Dcb,13,cc */
	{0xa0,01, 0x0008},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* 00 */
	{0xa0, 0x08, ZC3XX_R1C6_SHARPNESS00},	/* sharpness+ */
	{0xa1, 0x01, 0x01c8},
	{0xa1, 0x01, 0x01c9},
	{0xa1, 0x01, 0x01ca},
	{0xa0, 0x0f, ZC3XX_R1CB_SHARPNESS05},	/* sharpness- */
	{0xa0, 0x24, ZC3XX_R120_GAMMA00},	/* gamma 5 */
	{0xa0, 0x44, ZC3XX_R121_GAMMA01},
	{0xa0, 0x64, ZC3XX_R122_GAMMA02},
	{0xa0, 0x84, ZC3XX_R123_GAMMA03},
	{0xa0, 0x9d, ZC3XX_R124_GAMMA04},
	{0xa0, 0xb2, ZC3XX_R125_GAMMA05},
	{0xa0, 0xc4, ZC3XX_R126_GAMMA06},
	{0xa0, 0xd3, ZC3XX_R127_GAMMA07},
	{0xa0, 0xe0, ZC3XX_R128_GAMMA08},
	{0xa0, 0xeb, ZC3XX_R129_GAMMA09},
	{0xa0, 0xf4, ZC3XX_R12A_GAMMA0A},
	{0xa0, 0xfb, ZC3XX_4  },
	    51,4E{0xa0, 0xff, ZC3XX_R12C_G4,
		.sizei520xa00xff, ZC3XX_R12D_GAMMA0D},
	{0xa0, 0xff, ZC3XX_R12E_GAMMA0E},
	{0xa0, 0xff, ZC3XX_R12F_GAMMA0F},
	{0xa0, 0x18, ZC3XX_R130_GAMMA10},
	{0xa0, 0x20, ZC3XX_R131_GAMMA11},
	{0xa0, 0x20, ZC3XX_R132_GAMMA12},
	{0xa0, 0x1c, ZC3XX_R133_GAMMA13},
	{0xa0, 0x16, ZC3XX_R134_GAMMA14},
	{0xa0, 0x13, ZC3XX_R35_GAMMA15},
	{0xa0, 0x10, ZC3XX_R136_GAMMA16},
	{0xa0, 0x0e, ZC3XX_R137_GAMMA17},
	{0xa0, 0x0b, ZC3XX_R138_GAMMA18},
	{0xa0, 0x09, ZC3XX_R139_GAMMA19},
	{0xa0, 0x07, ZC3XX_R13A_GAMMA1A},
	{0xa0, 0x06, ZC3XX_R13B_GAMMA1B},
	{0xa0, 0x00, ZC3XX_R13C_GAMM {
		.id      = MMA15},
	{0xa0, 0x10, ZC3XX_R136_GAMMA16},
	{0xa0, 0x0e, ZC3XX_R137_GAMMA17},
	{0xa0, 0x0b, ZC3XX_R138_GAMMA18},
	{0xa0, 0x09, ZC3XX_R139_GAMMA19},
	{0xa0, 0x07, ZC3XX_R13A_GAMMA1A},
	{0xa0, 0x06, ZC3XX
	{0xa0, 0x08, ZC3XX_R301_EECROMACCESS},
	{0xa0, 0x68, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, ic int C3XX_R301_EEEROMACCESS},
	{0xa0, },
	{0xa0, 0x10, ZC3XX_R136_GAMMA16},
	{0xa0, 0x0e, ZC3XX_R137_GAMMA17},
	{0xa0, 0x0b, ZC3D},
	{0xa0, 6xff, ZC3XX_R12E_GAMMA8 light0xa0, 0xff, 7C3XX_R12F_GAMMA0F},
	_GAMMA0D},
	{0xa0, Bxff, ZC3XX_RZC3XX_R197_ANTIFLICKERLOW},
	{0xa0, 0x10, ZC3XX_R= 352 * 288 * 3 /x0180},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x40, ZC3XX_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GGAIN},
	{0xa0, 0x40, ZC3XX_R118_BGAIN},
	{}
};
static const struct usb_action cs2102_50HZ[12E_GAMMA0E},
	{0xa0, 0xff, CC3XX_R12F_GAMMA0F},
	{0xa0, 0	{0xa1, 0x0Dxff, ZC3XX_R12E_GAMMAtic consXX_R18D_YTAEC3XX_R12F_GAMMA0F},
},
	{0xa0, 0x10, ZC3XX_R136_GAMMA16},
	{0xa0, 0x0e, ZC3XX_R137_GAMMA17},
	{0xa0, 0x0b, ZC3XXX_R18D_YTA6GET},
	{0xa0, 0x00, 08 light
	{0xa1, 0x07, 0x0002},
	{0xa1, 00x68, ZC3XX_R18D_YTADGET},
	{0xa00,10,0a,cc */
	{0xa0, 0x01, ZC3XX_R010_CMOSSEX_R189_AWBSTATUS{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x3f, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,3f,cc */
	{0xa0, 0xf0, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,f0,cc */
	{0xa0, 0x00, ZC, 0x00, 0x01ad},
	{0xa1, 0x0EEXPOSURELIMITLOW}, /*x08, ZC3XX_R18D_YTABGET},
	{0xa0, 0x00, 0ype of 
	{0xa1, 0x0C, 0x0002},
	{0xa1, 0x,
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x40, ZC3XX_R116_RGAIN},
	{0xa0, 0xx18, ZC3XX_6130_GAMMA10},
	{0xa0,8 lightZC3XX_R131_G7MMA11},
	{0xa0, 0x20	{0xa0, 0x18, ZC3XX_D0x05, ZC3XX_3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0x 00,19,00,cc */
	{0xaa, 0x0f, 0x008c}, /* 00,0f,8c,aa */
	{0xaa, 0x03, 0x0005}, /* 00,03,05,aa */
	{0xaa, 0x04, 0 /* 00,1e,b0,cc */
	{0xa0, 0xd0, ZC3XX_R01F_HSYNC_2}, /* 00,1f,d0,cc */
	{}
};
static const struct usb_act,
	{0xa0, 0x20, ZC3XX_R131_GAMMA11},
	{0xa0, 0x20st structx18, ZC3XX_B130_GAMMA10},
	{0xa0,ype of ZC3XX_R131_GCMMA11},
	{0xa0, 0x20, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x3f, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,3f,cc */
	{0xa0, 0xf0, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,f0,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
10_CMOSSENSORSELECT}3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x42, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,42,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,cc */
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,10,cc */
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,24,cc */
	{0xa0, 0x8c, ZC3XX_R01D_HSYNC_0}, /* 00,1d,8c,cc */
	{0xa0, 0xb0, ZC3XX_R01E_HSYNC_1}, /* 00,1e,b0,cc */
	{0xa0, 0xd0, ZC3XX_R01F_HSYNC_2}, /* 00,1f,d0,cc */
	{}
};
static const struct usb_action cs2102_50HZScale[] = {
	{0xa0, 0x00, ZC3XRGAIN},			/* 01,16,58,cc , /* 00,19,00,cc */
	{0xaa, 0x0f, 0x0093}, /* 00,0f,93,aa */
	{0xaa, 0x03, 0x0005}, /* 00,03,05,aa */
	{0xaa, 0x04, 0x00a1}, /* 00,04,a1,aa */
	{0xaa, 0x10, 0x0005}, /* 00,10,05,aa */
	{0xaa, 0x11, 0x00a1}, /* 00,11,a1,aa */
	{0xaa, 0x1b, 0x0000}, /* 00,1b,00,aa */
	{0xaa, 0x1c, 0x0005}, /* 00,1c,05,aa */
	{0xaa, 0x1d, 0x000,10,0/* from zs211.inf - HKR,%OV7620%,NSORSEL - 640x48X_R13amma,
	},
#define SD_AUTOGAIN 3	{0xa0_mode0	/* 00,10,01,cc */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xaa, 0xfe,define SENS		/* 00,fe,00,aa */
	{0xa0,4ic const struct usb_action adc0, 0x0100},				/* 00,01,00,dC3XX_R010_CMOSSENSORSELECT},	/dd */
	{0xbb, 0x04, 0x0400},				/* 04,04,00,bb */
/
	{0xaa, 0xCT},	/* 00,10,0a,cc */
	{0xdd, , 0x44, ZC3XX_R121_{0xa0, 0x3983PAS202, ZC3XX_R010_CARPNESS 5
	{
	    {
		 it will be85ne SEN, ZC3XX_R010_C */
	{0xa0, 0x01, ZC30xa0, 0x01, ,				/* 00,fe,10,aa */
	{0/* 00,10,01,cc */
	 0x0400},		6,d0,aa */
	{0xaa, 0x28, 001, ZC3XX_R010_CMOSSENSORSELEC{}
};
static const struct 2_PIX_FMT_JPEG, V4LCKSEZ[] = {
	{0xa0, 0x01, ZC3XX_R010_CMOad */
	{0xaa, 0xfe, ntrast",
		8D_COMPABILITYGAMMA05},
	0,8dMA07},
	{0xa0, 0xe0, ZC3XX_R128/
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xbb, 0x41, 0x2803},				/* 28,41,03,bb */
	{0xbb, 0x40, 0x2c03},				/* 2c,40,03,bb */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xaa, 0xfe, 0xWINHEIGHTLOW/* 04,01,00,bb */
	{0xa0, 0x01,d */
	{0xbb, 0x01, 0x0400},				/* 04,01,00,bb */
	{0xa0, 0x01, ZC3XX_R010_CMOSSEN0010},				/* 00,fe,10,aa */
	{}
};
static const struct usb_action adcm2700_50HZ[] = {
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xaa, 0xfe, 0x0002},				/* 00,fe,02,aa */
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENS4,05,00,bb */
	{0xbb, 0xd0, 0xb007},				/* b0,d0,07,bb */
ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0x10,01,cc */
	C3XX_R008_CL2CKSETTING},	/* 00 */,cc */
	4c */
	{0xa0, 4XX_R12F_GAMMA0F},
7,
	{0xaa	{0xa1, 0x75,803, ZC3XX_R008_CLO,10,aa *a},
	{0xa0,13OLFU3,cc */
	{0xbb, 01,bb */
	{0xa0, 0x0 0x28, 0x0002},				/*/

	__u8008},
	{0xa0};
static const struct1,bb */
	{0xa0, 0x, 0x28, 0x0002},				/* 00,28,02,		/* 00,0057, ZC3XX_R101_SENSORC8 light* 01,a9,10,17GET},
	{0xa0, 0x00, * 00,10,b	{0xa1, 0x18,bZE}, /* 01,8c,10,cc	/* 00,fe,02,aa */
	{0xa0, 0x0a, ZC3XX_R010_CMOSf,
		.sizeima,f},
	{0xa0, 0xf4, Z in diff 0x0000},
	2a0, 01},				/* b8,a0,01,bb */C3XX_R008_C = 2SETTING},	/* 00 */
YSTEMOP73XX_R019_AU5,7SETTING},	/* 00 */
8 lightfxdd, 0x00,27,f/* 00,10,0a,cc */
		Z-Star0, 0x32, ZCNC},iker[] = {
	{0xa0, 01, 0x0008},
	{0xa2
	{0xa0, 0x03, ZC3XX_210_CMOSSV4L2_PIX_FMa */
0x0005}, /* 00,10gned ch9/
	{0xaa, 0bine 0x0005}, /* 00,10struct gt struct u2dFF},9,cc */
	{0xa0, 002139, UT},
	{0xa0,4 1,
		.default_value6 WARRAN6},		/* 03,61,6XX_R12F_GAMMA0F},
60x00, ZC3XX_R019_A6TOADJUSTFPS}, /* 00,1102K 2
#008},
	{0xa0,bb */
	{0xa0, 0x01, to the F 0x0200},		0   = 1,
		.default_val WARRANT08},
	{0xa0, 9,
		.default_value 012_VIDE		.step    20x05, ZC3XX_R01ICM105 ZC3XX_R1CB_SHARPNESS05},	/* sharpness- */7	{0xa0, 0x24, ZC3XXSORSELECT},	/* 00,10,01,cc */
	{0xaa, 0xfe, 0x0002},				/* 00,	{0xa1, 0x01, 0x01c9},
	{0xa1, 0x01, 0x01ca},
	{0xa0, 0x0fX_R120_GAMMA00},	/* gamma 5 */
	{0xa0, 0x44, ZC3XX_R121_ZC3XX_Ra_IDX 4
#1,adc */
	{0xa0, 0x00, ZCGAMMA01},
	{0xa0, 0x64, ZC3XX_R122_GAMMA02},
	{0xa0, 0x84, ZC3XX_R123_GAMMA03},
	{0xa0, 0x9d, ZC3XX_R124_GAMMA04},
	{0xa0, 0xb2, ZC3XX_R125_GAMMA05},
	{0xa0, 0xc4, ZC3XX_R126_GAMMA06},
	{0xa0, 0xd3, ZC3XX_R127_GAMMA07},
	{0xa0, 0xe0,6OR_MAX 18
	R_PAS202B ,cc */
	600, , 0x01, 0x01c8},d_getautogainine SENSO,cc */
	8RGB2 0xe4, ZC3XX_R192_EXPOSUREL11D_ SD_GAale[] = {
	{0xd/
	{0xa0, 0x00, ZC3XXZC3XX_R128_GAMMA08},
	{0xa0, 0xeb, ZC3XX_R129_GAMMA09},
	ion cs1,01,37,cc */
	{0xa0, 0x0d, ZC3XX_R105ECT},	/* 00,10,000aa}, /* 00,1d,aa,aa */
	{0xa0, 0x00, Z= 352 -0, ZC3XX_R13SURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 1	/* 00,10,01,cc */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xaa, 0xfe,xaa, 0x04, 		/* 00,fe,00,aa 30C_5_ANTI	{0xaa, 0x1OR_TAS5130CXX 16
#define SENSOR_TAS5130C_,00,cc */
	{0xa0ELIMIructx change 17
#define SENS3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x3a, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,3a,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c ZC3XX_*/
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8FLICKERM*/
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF}, /*FLICKERM0,cc */
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEPFLICKERM,aa,24,cc */
	{0xa0, 0x5d, ZC3XX_R01D_HSYNC_0}, FLICKERM,5d,cc */
	{0xa0, 0x90, ZC3XX_R01E_HSYNC_1}, /* 00,1e,90,cc */
	{0xa0, 0xd0, 0x00c8}, /* 00,c8,d0,cc */
	{}
};
static const struct usb_action cs2102_60HZScale[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xaa, 0x0f, 0x00b7}, /* 00,0f,b7,aa */
	{0xaa, 0x03, 0x0005}, /* 00,03,05,aa */
	{0xaa, 0x04, 0x_R190_EXPOSURELIMITHIGH},
	{0xa0, 0xab, ZC3XX_R191_EXPOSURELaa */
	{0xaa, 0x11, 0x00be}, /* 00,11,be,aa */
	{0xaa, 0x1b, 0x0000}, /* 00,1b,00,aa */
	{0xaa, 0x1c, 0x0005}, /* 00,1c,05,aa */
	{0xaa, 0x1d, 0x00be},  ZC3XX_static const struct usb_action adcm2700_50HZ[] = {
},
	{0xa0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01 ZC3X/
	{0xaa, 0xfe, 0x0002},				/* 00,fe,02,aa */
	{0xSYSTEMOPLIMITLOW}, /* 01,92,fcC3XX_R010_CM
	{0xbb, 0xd0, 0},
	{0xac0,fe,10,aa LIMITMID	{0x48XX_R1AA_DIGITALGAINSTEP},
E_HSYNC_1},
	{0xa0, 0xb0, ZC},
	{0xa_HSYNC_2},
	{0xa /* 01,96,00,cc */
	{0xa0, 0x69, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,69,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,cc */
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,10,cc */
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,24,cc */
	{0xa0, 0xb7, ZC3XX_R 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xaa, 0x0f, 0x0059}, /* 00,001D_HSYNC_0}, /* 00,1d,b7,cc */
	{0xa0, 0xd0, ZC3XX_R01E_HSYNC_1}, /* 00,1e,d0,cc */
	{0xa0, 0xe8, ZC3XX_R01F_HSYNC_2}, /* 00,1f,e8,ce = V4L2_Ca,fWER_LINE_FREQUENCY,
		.typion cs2102_NoFliker[] = {
	{0xa0, ,59,aa */
	{0xaa, 0x03, 0x0005}, /* 00,03,05,aa */
	{0xaa, 0x04, 0x0080}, /* 00,04,80,aa */
	{0xaa, 0x10, 0x0005}, /* 00,10,05,aa */
	{0xaa, 0x11, 0x0080}, /* 00,11,80,aa */
	{0xaa, 0x1b, 0x0000}, /* 00,1b,00,aa */
	{0xaa, 0x1c, 0x0005}, /* 00,1c,05,aa */
	{0xaa, 0x1d, 0x0080}, /* 00,1d,80,aa */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x3f, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,3f,cc */
	{0xa0, 0xf0, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,f0,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x10, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,10,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R1SYSTEMOFREEZE}, /* 01,8f,20,cc */
	{0xa0,MITMID0, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,00,cc */
	{0xa0, 0x00, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,00,cc */
0x01, Z ZC3XX_R124_GAMMA04},
	{0xa0, 0xb2, ZC3XX_R125_GAMMA05},
	{0xa0, 0xc4, ZC3XX_R126_GAMMA06},
	{0xa0, 0xd3, ZC3XX0x01,XX_R01F_HSYNC_2}, /* 00,1f,c8,cc */
	{}
};
sta 0x01, Zst struct usb_action cs2102_NoFlikerScale[] , ZC3XX_Ra0, 0x00, ZC3XX_R019_xaa, 0x04, 0S}, /* 00,19,00 ZC3XX_RdPOSURELIMITHIGH}, /* /* 00,0f,59,aa */
	{0xaa, 0x03, 0x0005}, /* 00,03,05,aa */
	{0xaa, 0x04, 0x0080}, /* 00,040x01, ZC/
	{0xaa, 0x10, 0x0005}, /* 00,10,05,aa */
	{0xaa, \AE, {
	,80,aa */
	{0xaa, 0x1b, 0x0000}, /* 00,1b,XX_R18C_AEFREEZE},
	{c */
	{0xa0, ZC3XX_ZC3XX_R18F_AEUNFdstruct 
	{0xart chip_0xa0, 0x0dd0080}, /* 00,11,80,aa */
	CADDRES0x1b, 0x0000}, /* 00,10x10, ZC3XX_CADDRESFREEZE}, /* 01,8c,10,cb,00,aa */
	_I2CSETVx1c, 0x0005}, /*/* 00,10,01,cc */
	{0xdd, 0x00, 0x0010}0x01, Z= 0,
		.maximum = 3,
		.step    = 1,
		.default_value =x18, ZC3 },
	    .set = sd_setCMOSSENSORSELECT},	/* 00,10,0a,cx18, ZC32AND}0x00, 0x0010},				/* 00,00,10,dd */
	{0xbb, 0x04x18, ZC3320, 240, V4L2_PIX_FMT_JPEG, V4L2_FIELD_NONE,
		.bytx18, ZC30xa0, 0x05, ZC3XX_R012ZC3XX_R123_
	{0xdd, 0x00, 0x00x18, ZC37RMIDOOLEAN,
		.name xa0, 0x082a0, 0_I2CCOMM0,8A0A},
	{0xa0, 0xfb7	mxhaard090_},
	{0xa076x03, ZC3XX/* ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,9		 e4,cc */
	{0xWINYSTA	 if a0, 0 (X_R190_)_R092_I2CADDRESSSELECT},
	{0xa0, 0x18, ZC3XX_R09X_R1,80,aa */
	{0xaa, 0x1b, 0x0000}, /* 00,1b,	{0xa0, 0x00, ZC3XX_R, ZC3XX_R090_,
	{0xa0,C3XX_R18F_AELIMITMID(bug in/* 00,1d,a3_I2CSE 0x11, ZC3XX_R092_I2C,
	{0xa0SSELECT},
	{0xa0, 0x18, ZC3XX_94_I2CWRITEAC2bbb */
	{0xa0, 0x01, ZC3XX_R094_I2xa0, 0x01CK},
	{0xa0, 0x01, ZC3XX_R090_I2CCMAND},
	{,
	{0xa0, 0x12, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0inimum = 0,
		.maximum = 3,
		.step    = 1,
		.default_value = 2,
	    },
	    .set = sd_setCMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xdCADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVAL */
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,01,cc */
	{0xa0, 0x05, ZC3XX_R0120x16, ZC3XX_R092_I2CADDRESSSEL	{0xa0, 0x00, 0x0c, ZC3XX_R093_I2CSET*/
	},
	{0xa0, 0 0x10, ZC3XX_R002_CLRITEACK},
	{0x0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x17, ZC3XX_R092_ICADDRESSSELECT},
	{0x *0, 0x0c, ZC3XX_R093_I2C/* ??I2CSgspca v1, it was* 00,10,01,cc *0, 0xMA13CADDRES08, ZT},
R_TAS5130CK 15
#def370xa0R092_I2CADDRESSSELECT},
	{0xa0, 0x18, ZC3XX_R0910_CMOSS,80,aa */
	{0xaa, 0x1b, 0x0000}, /* 00,1b,}, /* 01,95,00,cc */
	{0x2_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0xb7, ZC3XX_R101_SENSORCeRRECTION},
	{0x, 0xfe, 0x0002},		b,00,aa *1NTROLFUNC},
	{0},
	{0xa0, 0x3XX_R18D_YTARGET},
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x20, ZC3XX_R087_EXPTIMEMID},
	{0xa0, 0x21, ZC3XX_R088_EXPTIMELOW},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3Xxa0, 0xf0, /
	{0xdd, 0x00, 0x0010},				/*dd */
	{00_I2CCOMMAND}004_FRAMEWIZC3XX_R117_GGAIN},
	{0xa0, 
	  , ZC3XX_-0, 0x0c,1 (, ZC3XX, 0x03, ZC3TING},	/* clock ? */
	{0xa0, 0x08, ZC3XX_R1C6_SHARPNESS00},	/* sharpness+ */
	{amma,
	},
#define SD_AUTOGAIN 3ov7630cMA1C},
	{0xa0,,10,01,cc */
	{0xdd, 0x00, 0x0010},				/*1, ZC3XX_R13F_GAMMA1F},
	{0xa0, 0x58, Z8},
	{0xa0, 0x09, ZC3XX_R139_GAMMA19},
	{0xa0, 0x07, ZC3XX_R13A_GAMMA1A},
	{0xa0, 0x06, ZC3X0CXX 16
#define SENSOR_TAS51f,cc */
	{0xa0, 0xf0, ZC3XX_R192_EXPOSURELI0, 0x3a, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,9	{0xa0, 0x90, ZC3XX_R01E_HSYNC_1}, /* 00,, 0xd0, 0x00c8}, /* 00,c8,d0,cc */
	{}
};
s},				/* 00,fe,02,aa */
	{0xa0, 0x0a, ZC3XELECT},	/* 00,10,0a,cc */
	{0xbb, 0x41, 01,03,bb */
	{0xbb, 0x40, 0x2c03},				/* 2c,01},				/* 0_R190_EXPOSURELIMITHIGH},
	{0x0xbb, 0x01, 0x0400},				/* 04,01,00,bb */
	{RMID}, /* 01,96,00,cues */
#define SENSOR_OV7EEZE}, /* 01,8c0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8x80, ZC3XX_R_R1A9_DIGITALLIMITDIFF}, /*x80, ZC3XX_RZC3XX_R1AA_DIGITALGAINSTEP*/
	{0xa0, 0x5d, ZC3XX_R01D_HSYNC_0}, 91_EXPOSURELIMITMID},
	{0xa0, 0x98, ZC3Xt struct usb_action adcm2700_50HZ[] = {
X_R010_CMOSSENSORSELECT},	/* 00,10,01e, 0x0002},				/* 00,fe,02,aa */
	{0xX_R1A9_DIGITALLIMITDIFF},
	{0xa0, 0x24, 3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x00, Z3XX_R197_ANTIFLICKE6ichel Xhaard <mxhaard@uchel Xhaard
 *		mxhaarERAT*/
	{0xa0, 0x38, ZC*/
	e sensor chip */
/*/Vimicro zc301/ 00,28,02erms of the GN01D_HSYNC_0}/
	{0xa0, 0xd0, ZC3XX_R0,1e,d0,cc */
	{0xa0, 0YNC_2}, /* 00,1f,e8,SE.  See the
 1_R090_I2req(struct gspca_dev *creq(struct gspcCopyrig6rent tables */
YSTEMOP 0x0 0, 1: 50Hz, 2:60HzC3XX, ZC3XX_R090_I2CCOMe!! values used ,05,aa */
	0}, /* 00,04,80,aa */
	5}, /* 00,10,05,aa *@magic.fr
 *
 1,80,aa */
	GAMMA06},
	{0xa0, 0},		R092_I2CADDRt gspca3ftware Foundatxa0, 0x2LECT},
	{0xa0, , 0x25, ZC3X0}, /* 00,1d,80,aa XX_R093_I2CSETV1f,c,cc *
#include "zc36t gspca
 *
 * This pro0x10, ZC3XX_R093_I2CSET,95, ZC3X *
 * This prog,80,aa *Vimicro zc301/zVALUE},
ues */
#define SORSELECT},	/* 00,10,01,cc */
	{IGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_, /* 01,97,10,cc */
	{0xa0, 0x10, ZC3XX_R01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R1imum = 3,
		.step    = #define SD_GAMMA 2
	{
	    {
	c */
	{0xa0, ZC3XX_R121_GAMMA01},
	{0xa0, 0x64, ZC3XX0xa0, 0x84, ZC3XX_R123_GAMMA03},
	{0xa0ne SENSOR_GC0305 3
#define SENSOR_HDCS2020b 4
#define SENSOR_HV7131B 5
#define SENSOR_HV* 00,1efine SENSOR_PAS202B 12
#define S_R120_GAMMAfine SENSOR_TAS5IGHTNESS 0
	{
	  1ZE},Bhttp:ADDR93_I2	u8 *jpeg_hdr;
};

/OMMAND},
	{, ZC3XX_R092_I2CAightness(struct gspca_dev *gspca_deOR_MAX 18
	al);
static int sd_set},
	{0xa0, 0uct gspca_dev *gspca_},
	{0xa0, 0al);
static int sd_geOBALGAIN},
	ruct gspca_dev *gspca},
	{0xa0, 0*val);
static int sd_f,c8,cc */
	struct gspca_dev *gspf,c8,cc */
	2 val);
static int sdefine SENSOR(struct gs3XX_RMMA1HSYNC_3},
	{0xa0, 0x60XX_R093_ITAS5130CXX 16
#define SENSOR_TAS5130C_VF0250 17
#define SENSOR_MAX 18
	unsigned short chip_revision;

	u8 *jpeg_hdr;
};

/* V4L2 controls supported by the driver */
static int sd_setbrightness(struct gspca_dev *gspca_dev, __s32 vaMMA10},
	{0xa0, 0x22, 20_GAMMArt chip_gamma 2 ?a0, 0x01, ZC3X_R09E_WINW121MAND},
atic int sd_se1ghtness(str22MAND},
, ZC3XX_R11D_G3ESS,
		.typ23MAND},
get = sd_getco5ZC3XX_R123_24MAND},
efine SENSOR_G6	{0xa1, 0x025MAND},
9_DIGITALLIMIT10,0a,cc */126MAND},
T},
	{0xaC3XX_RDRESSSELECT}7MAND},
 Public Lm2700_60HZ[] = {128MAND},
, 0x01, ZC3XX_, 0x01, ZC3129MAND},
ong with ,92,fcX_R090_I2CCOAMAND},
AR090_I2CCOMMAN,05,00,bb 12BMAND},
B0xa0, 0xf4, ZCprogram is12CMAND},
{0xa0, 0xee, ZfCOMMAND},
	{DMAND},
2CWRITEACK},
	f, ZC3XX_R094EMAND},
8F_AEUNFREEZE}d_setcontra12FMAND},
AST,
		.type  C3XX_R191_E13MMAND},static int sd_gightness(str3,
	{0xagspca_dev *gspct usUE},
	{0xLUE},
	;
static int sdNESS,
		.typ3RITEACKht (C) 200 This program is13I2CCOMM1efine SENSOR_G, ZC3XX_R1A7_92_I2CA
 *
 * Thi	{0xa0v, __s32 *v3 ZC3XX_1093_I2CSETVALU 00,3XX_R1A7_x00, ZCl Public L,
		.priv = 0},
}30, 0x013XX_R093_I/* 04,00,00,bb */30xa0, 0long with  ? */
3XX_R093_I23LECT},
1{0xa0, 0x01, Z0_value = 123SETVALU1},
	{0xa0, 0x00 ZC3XX_R1A7__I2CWRI1{0xa0, 0xee, ZSOR_MAX 18
	3R090_I212CWRITEACK},
	X_R120_GAMMA3XX_R0921
	{0xa0, 0x00, ZC3XX_R123_30x04, Z1AST,
		.type  LOBALGAIN},
	{0xa0, 0x60, ZC3XX_R11D_GLOBALGAIN},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x60, ZC3XX_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GGAIN},
	{0xa0, 0x4c, ZC3XX_R118_BGAIN},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x20, ZC3XXspca_dev *gspca_dev, __s32 *val);
static int sd_setgamma(struct gspca_dev X_R093_I2CSET1* quantizationTENABLE},req(struct gspcx18, ZC3XX_R092_I2CADDRESSSELE/Vimicro z/* 00,10,01,cc */
	{0xdd, 0x00, 0x0010}AMMA10},
	{0xa0, 0x22, Z {
		.id      = V4L2_CID_BRIGHTbCMOSSENSORSELECT},	/* 00,10,0a,c_INTEGER,
		.name    = "Brightness",
		.minimum = 0,
		.maximum = 255,
		.step    = 1,
		.defaul, ZC3XX_R1CB8,
	    },
	    .set = sd_setbrightness,
	    .get = sd_getbrightness,
	},
#define SD_CONTRAST 1
	{
	    {
		.id      = V4L2_CID_CONTRAST,
		.type  , 2:V4L2_CTRL_TYPE_INTEGER,
		.name    = "CXX_R092_I2CADDRESSSELECT},
 */
#define SENSOR_OV7630C 10
#define SENSOR_PAS106 11
#define SENSO, ZC3XX_R092_I2CADDRESSSELECT},
	{90_I2CCOMMAa0, XX_R13D_GAM V4L2_CTRL_TYPE_INTEGER,
		.name    = "Gamma",
		.minimum = 1,
		0x0d, ZC3XX_R137_GAMMA17},
	{0xa0, 0x0b, ZC3XX_R138_ECT},	/* 00,10,01,cc */
	{0xdd, 0x00, 0x0010},				/*0xaa, 0xfe, 0x0000},				/* 00,fe,00,aa 3XX_R13D_GAMMA1D},
	{0xa0, 0x03, ZC3XX_R13E_GAMMA1E},
	{0xa0, 0x02, ZC3XX_R13F_GAMMA1F},
	{0xa0, 0x58, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xf4, ZC3XX_R10B_RGB01},
	{0xa0, 0xf4, ZC3XX_R10C_RGB02},
	{0xa0, 0xf4, ZC3XX_R10D_RGB10},
	{0xa0, 0x58, ZC3XX_R10E_RGB11},
	{0xa0, 0xf4, ZC3XX_R10F_RGB12},
	{0xa0, 0xf4, ZC3XX_R110_RGB20},
	{0xa0, 0xf4, ZC3XX_R111_RGB21},
	{0xa0, 0x58, ZC3XX_R112_RGB22},
	{0xa0, 0x00, ZC3XX_R180__AUTOCORRECTENABLE},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x13, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x22, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAe2_VIDEOCONTROLFUNC},
	{0xa0,H}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3_R093_I2CSETVALUE},
	{0a0, i2OLEAN,
		.name  C3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CC	{0xND},
	{0xa0, 0x20, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0	/* 00,f},
	{0xa0, 0x01	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x21, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x22, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3Xdefine SENSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x01, ZC3XX_R0A3_EXPOSURETIMEHIGH},
	{0xa0, 0x22, ZC3XX_R0A4_EXPOSURETIMELOW},
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},
	{0xa0, 0xxa0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x07, ZC3XX_R191_EXPOSURELIMITMID},
	{0xa0, 0xee, ZC3XX_R192_EXPOSURELIMITLOW},
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},
	{0xa0, 0x3a, ZC3XX_R197_ANTIFLICKERLOW},
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},
	{0xa0, 0x0c, ZC3XX_R1A9_DIGITALLIMITDIFF},
	{0xa0, 0x28, ZC3XX_R1AA_DIGITALGAINSTEP},
	{0xa0, 0x04, ZC3XX_R01D_HSYNC_0},
	{0xa0, 0x0f, ZC3XX_R01E_HSYNC_1},
	{0xa0, 0x19, ZC3XX_R01F_HSYNC_2},
	{0xa0, 0x1f, ZCYNC_3},
	{0xa0, 0x60, ZC3XX_R11D_G43XX_R093_I2ightness(struct gspca_dev *gspca_deOMMAND},
	{0l);
static int sd_setSS 0
	{
	  ruct gspca_dev *gspca_dev, __s32 val);
static int sd_g4	{0xa1, 0x01ruct gspca_dev *gspcaDRESSSELECTXX_R117_GGAIN},
	{0xaSENSORSELECT}struct gspca_dev *gsp1},
	{0xa0, 2 val);
static int s4ESS,
		.typEACK},
	{0xa0, 0x01, ZC3XX_R0a0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x21, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x5c, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC390_I2CCOMMANOMMAND},
	{0xa0, 0x18,~42},
	{0xa0, 0xfXX_R094_I2CW,
	{0xa0, 0x00, ZC3XX_5_value = 12ALUE},
	{0xa0, 0x00, ZC7DRESSSELECT}RITEACK},
	{0xa0, 0x01R004_FRAMEW0_I2CCOMMAND},
	{0xa0, 0a, ZC3XX_R09492_I2CADDRESSSELECT},
b0, 0x16, ZC3 ZC3XX_R093_I2CSETVALUcXX_R094_I2CWx00, ZC3XX_R094_I2CWRIdv, __s32 *va0, 0x01, ZC3XX_R090_I2eX_R090_I2CCO0xa0, 0x14, ZC3XX_R092_value = 12SELECT},
	{0xa0, 0x01, ZfX_R090_I2CCOSETVALUE},
	{0xa0, 0x0ev, __s32 *v4_I2CWRITEACK},
	{0xa0, DRESSSELECT}R090_I2CCOMMAND},
	{0xaT},
	{0xa0, XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSEess,
	},
#dexa0, 0x00, ZC3XX_R094_I
#deITEACK},
	{0xa0, 0x01, ZC3XX_R0ZC3XX_R101_SE},
	{0xa0, 0x00, ZC3XX_DRESSSELECTOBALMEAN},
	{0xa0, 0x04,0_I2CCOMMANDCALCGLOBALMEAN},
	{0xa0RITEACK},
	{X_R1A7_CALCGLOBALMEAN},C3XX_R101_SE4, ZC3XX_R1A7_CALCGLOB10,cc */
	{0xa0, 0x20, ZC3XX_R092_Ix01, ZC3XX_R0CT},
	{0xa0, 0x01, ZC3, 0x21, ZC3XXTVALUE},
	{0xa0, 0x00, 
	{0xa0, 0x02CWRITEACK},
	{0xa0, 0x0_I2CCOMMAND90_I2CCOMMAND},
	{0xa0,LMEAN},
	{0x_R092_I2CADDRESSSELECT}VALUE},
	{0x96, ZC3XX_R093_I2CSETVAC3XX_R101_SE, 0x00, ZC3XX_R094_I2CWxa0, 0x22, {0xa0, 0x01, ZC3XX_R090COMMAND},
	{0xa0, 0x10, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x18, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x11, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x18, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I90_I2CCOMMAND},
	{0xa0, 0x14, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0val);
static in_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R09YSTEMOPERATI {
		.id      = V4L2_CID_BRIGHTTALLIMITDIFpe    = V4L2_CTRL_TYPE_INTEGER,
		.name    = "Brightness",
		.minimum = 0,
		.maximum = 255,
		.step    = 1,
		.defaul90_I, 0x22, ZC3XX_R131_GAMMA11}_CALCGLOBALMEAN},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
	{}
};

static const struct usb_action cs2102K_InitialScale[] = {
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},
	{0xa0, 0x00, ZC3XX_R002_CLOCKSELECT},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},
	{0xa0, 0x08, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0x02, ZC3XK},
	IDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0,pas106bENSORSEL_com	.id   /* Sream and Sensor specifiOLEAN,
		.UT ANY WARRAN03_Fa0, },	/*{0xa0Select 0x03, System_R13D_GAMMA1D},
	{0xa0, 0x00, ZC3XX_R13E_GAMa0, ,
	{0xControla0, 0x04, ZC3XX_R13D_GAMMA1D},
	{0xa0, 0x03, ZC3XX_R13E_GAMMA1E},/* Picture size
	{0xaa, 0xfe, 0x0000},				/* 00,fe,00,aa , 0x0F025R13C_GAMMATHIGH}, /* 01,0, 0x40,X_R092_I2CADDRE,
	{0* quantiz * (at yo,
	{0XX_R09A18},
	{0xa0, 0x09, ZC3XX_R139_GAMMA19},
	{0xa0, 0x0{0xa0, /* 176x14TEACK/* JPEG	},
AMMA1E},
	{0xa0, 0xMA1D},
	{0xa0, 0x03, ZC3XX_R13A_GAMMA1A},
	{0xa0, 0x06, ZC3XX_R13B094_I2CWRITEACK}3XX_R10A_RGB00},	/* matRGB00},	/* matrix */
	{0xa0, 0xf4, ZC3XX_R{0xa0, 0x58, ZC3XX_R10E_RGB11_60HZ[] = {
xf4, ZC3XX_R10F_RGB12},
	{0xa0, ZC3XX_R093X_R110_RGB20},
	{0xa0, 0xf4, Z0xa0, 0x00, B21},
	{0xa0, 0x58, 1C},
	{0xa0, 0x04, ZC3XX_R13D_GAMMA102, ZC3XX_R13F_GAMMAB22},
	{0xa0, 0x00, ZC3XX_R180_AUTOCORRECTEaa */
	{0xaa, 0x11, 0x00be}, /* , 0x01, ZC3XX_R090_I2C,
	{0xa0, 0x01, ZC3XX_RZC3X{0xa0,Interfacrix */
	{0xa0, 0, ZC3XX_R10C_RGB02},
	{0xa0, 0xf/* Window inside s{0xa0,array, ZC3XX_R094_I2CWRITEACK},x22, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACKZC3XX_R123_0x01, ZC3XX_R090_I2CCOMMA	{0xACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCO1f,c8,cc */xa0, 0x0c, ZC3XX_R/* NSOR thI2CWRITEAcc */
	{0xa0, 0x00, Zterms of the GNU	Z-Star/Vimicro zc301/z	/* 00,f *
 * This progxa0, 0xc30x library
 *	0_I2CCOMMAND},
	{0xa0,C3XX_R010 *
 * This progstruct gs32 *val);
static int smage sensor chip */
/*3XX_R,
	{Other registors,
	{0xa0, 0x0f, ZC3XX_R1CB_SHARPNESS05},	/* sha,
	{Frame retreiving0, 0x08, ZC3XX_R1C6_SHARPNESS00},	/* sharpne,
	{Gainxa0, 0x00, ZC3Xxb3, ZC3XX_ */
	{0xa0, 0x0d,,
	{Unknown /* 01,8f,20,cc */
	{0xa0,2CADDevision;A9_DIGITALLIMITDIFF}, /* 01,a9,00,cc */
	{0xAEUNFREEZE},
	{0xa0, 0x0c, ZC3XX_R1A9_DII2CSETVALUE},
	{0xa0, 0x00, ZC3XC3XX_R195_ANTIFLICKERHIGH},
	{0/* Auto expos	/* A},
white balan	{0xa0, 0x01, ZC3ZC3XX_R196_ANTIFLICKERMID}/*Dead pixel, 0x07, ZC3XX_R10305 3
#define SENSOR_HDCS2020b/*  5
#de17
#define SENSOR_MAX 18
131B 5
#define SENS{0xa0, 0xf4, ZC3XX_R111_RGB21},
	{0xa0, 0x58, ZC3XX_R112_RGdefine SENSOR_MAX 18
	unsigned short cnt sd_setbrightness(struct gspca_dev *g0xa0, 0x00, ZC3Xe_R195_ANTIFLICKERHIGH},
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},
	{0xa0, 0x3a, ZC3XX_R197_ANTIFLICKERLOW},
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},
	{0xa0, 0x0c, ZC3XX_R1A9_DIGITALLIMITDIFF},
	{0xa0, 0x28, ZC3XX_R1AA_DIGITALGAINSTEP},
	{0xa0, 0x04, ZC3XX_R01D_HSYNC_0},
	{0xa0, 0x0f, ZC3XX_R01E_HSYNC_1},
	{0xa0, 0x19, ZC3XX_R01F_HSYNC_2MAND},
	{0xa},
	{0xa0, 0ghtness(struct gspca_dev *gspca_de2CSETVALUE},0, 0x18, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_2CSETVALUE},al);
static int sd_ge},
	{0xa0, 0ruct gspca_dev *gspca2CSETVALUE},*val);
static int sd_C_2},
	{0xa0struct gspca_dev *gspC_2},
	{0xa02 val);
static int sdf,c8,cc */
	x20, ZC3XX_R0NTIFLcorreGAIN 3_R10C_RGB02},
	{020_GAMMA01700_50HZ[]01D_HSYNC_0},
	{0xa0, 0x082700_5 0x0aX_R100_OPERATIONMODE},
	{83700_5CENTEPS},
	{0xa0, 0x93_I2CSETVAL400,fe,10,a01_EEPROMACCESS},
	{0xa0,8500,fe
	{0xa0, 0x01, ZC3XVALUE},
	{08600,fe,
	{0xa0, 0x13, ZC3X08, ZC3XX_R010_CMOSSENSORSELECT},
96_ANTIFLICKERMID},
	{0xa0, 0x3a, ZC3XX_R197_ANTIF
#define BRIGHTNESS_IDX 0
#define SD_BRIGHTNE0x16, ZC3XX_SELECT},
	{0xa0, 0x04, ZC3XX_R09xa0, 0x22, Z    = V4L2_CTRL_TYPE_INTEGER,
		.name    = "Brightness",
		.minimum = 0,
		.maximum = 255,
		.step    = 1,
		.defaul8EAN},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALDDRESSSELECT    .get = sd_getbrightnSMODE},
	{0xfine SD_CONTRASTip_r{0xa0,0, 0x00, ZC;
	__u8 lightbY; without even the imhttp://moinejf.free.fr>d@magic.fr
 *
 *	{0xa0, * quGH},
	{0xa0, 0x22, ZC3ess,
	},
#den cs2102K_InitialScale[] = {
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},
	{0xaXX_R0A4_EXPOXX_R002_CLOCKSELECT},
	{0x},
	{0xa0, 0xX_R002_CLOCKSELSSSELECT},
	{0xa0, 0x00, ZC3XX_Rx08, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa{0xa0, 0x59, ZC3X{0xa0, NTIFC,
	{0xEnablrix */
	{0xa0, 	.id      = V4L2_CID_GAMMA,
		.typeGH},
	{0xa0, 0x22, ZC3 = 6,
		.step    = 1,
		.default_value = 4,
	    },
	    .set = sd_setgamma,
	    .get = sd_getg
	{0xa0, 0x09, ZC3XX_R139_GAMMA19},
	{0xa0, 0x0= 352 * 288_R11352x288},
	{0xa0, 0xf4, ZC3XX_R111_RGB21},
	{0xa0, 0x58, ZC3XX_R112_RGB22},
	{0xa0, 0x00, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0xa0, 0x18, 3XX_R196_ANT{0xa0, 0x58, ZC3XX_R10E_RGB11},
	{0xa0, 93_I2CSETVALUE},
	{0xa0, 0x00, , 0xf4, ZC3XX_R110_RGB20},
	{0xa0, 0xf4, Zess,
	},
#dCOMMAND},
	{0xa0, 0x13, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x22, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x14, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x20, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x21, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x22, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x01, ZC3XX_R0A3_EXPOSURETIMEHIGH},
	{0xa0, 0x22, ZC3XX_R0A4_EXPOSURETIMELOW},
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},
	{0xa0, 0x07, ZC3XX_R191_EXPOSURELIMITMID},
	{0xa0, 0xee, ZC3XX_R192_EXPOSURELIMITLOW},
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},
	{0xa0, 0x3a, ZC3XX_R197_ANTIFLICKERLOW},
	{0xa0, 0x10, 0x10, ZC3XX_R1A9_DIGIe SENSOR_MC501ZC3XX_R18C_AEFREEZE},
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},
	{0xa0, 0x0c, ZC3XX_R1A9_DIGITALLIMITDIFF},
	{0xa0, 0x28, ZC3XX_R1AA_DIGITALGAINSTEP},
	{0xa0, 0x04, ZC3XX_R01D_HSYNC_0},
	{0xa0, 0x0f, ZC3XX_R01E_HSYNC_1},
	{0xa0, 0x19, ZC3XX_R01F_HSYNC_2},
	{0xa0, 0x1f, ZC3XX_R020_HSYNC_3},
	{0xa0, 0x60, ZC3XX_R11D_GLOBALGAIN},
	{0xa0, 0x60, ZC3XX_R11D_GLOBALGAIN},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x60, ZC3XX_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GGAIN},
	{0xa0, 0x4c, ZC3XX_R118_BGAIN},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x20, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x21, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x5c, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x13, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x5c, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x14, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZCC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_Rt = sd_setbrightness,
	    .get = sd_getbrightness,
	},
#define SD_CONTRASTC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x20, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x21, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x96, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0G},	/* clock ? */
	3XX__R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3d_setcontras18{0xa0,LOS	{0xa0,3XX_R0adjusXX_R10C_getgamma,
	},
#define SD_AUTOGAIN 39},
	{0x {
		.id      = V4L2_CID_AUTOGAI9A_WINXSTARTLOW},		/* 00,9a,00,cc */
	{0xa0, 0x00, ZCa0, 0x01, Z1,
		.default_value = 2,
	    },, 0x44, ZC3XX_R121 0x2rpness,
	    .get = sd_getsharpness,
	}5xa0, 0x01, ZC3XX_R005_FRAMEHEIGTIMEMID},
	{0xa0, 0x21, ZC3XX_R088_EXPTIMELOW},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XXEAN},
	{0xa0, 0x04, ZC3XX_R1A7	{0xa0, 0x0	{0xa0, 0x24, ZC3Xxbb, 0x0f, 0x140f},				/*a0, 0x004L2_PIX_FMT_JPEG, V4Lxa0,ALMEAN},
	{0xa0, 0x04, a0, 0x00f087_GAMMA09},
	{0xa0, 0xf4, );
s2CWRITEACK3 0x1R18F_AEUNFREEZE}, /* 01,8cCADDRESSSELE4ORSE0, 0x10, ZC3XX_R1A9_DIGITALUE},MITDIFF}, , ZC3XX_R083_RGAINA1D_HSYNCI2CSETVALUE},7
	{0MA07},
	{0xa0, 0xx3a, ZC3XX_RR100_OPERATIONMODE},		/* 01,00,
	    .set	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0X_R189_AWBSTATUS},		/* 01,89,06,ECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCO23XX_R093_I2ING},	/* 00,01,01,cc */
	{0xa0, /* b0,d0,07,bb */
	 ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0,7xa0, 0x22, ZC3XX_R131_GAMMA11},
	{0xa0, 7/
	{0xaa, 0xfe, 0x0002},				/* 00,fe,02,aa x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x21, ZC3XX_R092_AND},
	{0xa0, 3x18, ZC3XX_R092EEZE}, /* 01,842CSETVALUE},
	{cb,13,cc */
	{0xa0,7_CALCGL2CADDRESSSELE */
	0x01, ZC3XX_R090_I2CCOMMcN},
	{0xa0, 07	{0x ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa10_CMOSSENSORSELECT},	/* 00,10,0a,ccECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMYSTEMOPERATING},	/* 00,01,01,cc */
	{0xa0,OSURELIMITHIGH}, /* HEIGHTLOW},		/* 00,9c,d8,cc */
	{0xa0, 0x88, ZC3XX_R09E_WINWIDTHLOW},		/* 00,9e,88,cc */
	{0xbb, 0x00, 0x0400},				/* 04,00,00,bb */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xbb, 0x0f, 0x140f},				/*x00, ZC3XX_R094_I2CWRITEACK},ess,
	},
#define SD_CONTRAS2CCOMMAND}		.sizeimage =_R090_I2CCOMMAESSSELECT},
 0x0X_R18F_AEUNFREEZE}, /* 01,8f,2SETVALUE},
	{xa0, 0x10, ZC3XX_R1A9_DIGIT},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEANC3XX{0xa0, 0x04,FLICKERHIGH}, /* 01 0x0d, ZC3XX_R100_OPERATIONMODE},		/* 01,00,0d,cc */
	0, 0x0005}, /*usbvm31b1d,aa80,aa */
	{0xaa, 0x1b, 0x0000}, /*pas202a0, 0xf4, ZC3XX_R11X_R190_EXPO0x04, ZC3XX_R13D_GAMMA1D},
	{0xa0, 0x03, CK},
	{00,dd */
	{0xaa, 0xfe, 0x0000},				ine SENSOR_TAS513CK},
	{0cc */
	{0xa0, 0x00, ZC,05,00,bb *IFLICKERLOW}, /* 01,9
	{0xa0, 00},
	{0xa0, 0x14, ZC3XX_R092_I0x17, ZC3XX_R092_I2/
	{0xa0, 0x0a, ZC3XX_R010_CMcs2102_60HZScale[] = {
	{0xa0,cc */
	{0xSSENSORSELECT},	/* 00,10,0a,cc */
	{0xbb, 0x41, 0ETVALUE},
		/* 28,41,03,bb */
	{0xbb, 0x40, 0x2c03},				/* 2c,CK},
	{0xa0/
	{0xaa, 0xfe, 0xC3XX_R111_RGB21},
	{0xa0, 0x58,
	{0xaa,6,e*/
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01WRITEACK},
	/* 04,04,00,bb */
	{0xdd, 0x0a, 0x11, 0x00be}, /* _I2CCOMM,aa */
	{0xaa, 0x1b, 0x0000}, /* 00,1b,00,aa */
	{0xaaaa */
	{0xa
	{0xa0, 0xd0, 0x00c8}, /* 00,c8,d0,cc */
	{}
};
sFLICKERMnst struct usb_action 80, ZC3XX_R004_FRAMEWIDTHLOW}},
	{0xa0, 0x01, ZC3XX_R005_FRC3XX_R090_I2CCOMMAND},
	{0xa, 0x05, 0aaa */
	{0xaa, 0x1b, 0	{0xa0, 0x01, ZC3XX_R001_SSYSTEMOPERATING},
	{0xa0, 0x0301, ZC3XX_R093_I2CSETVALU/
	{0xaa,caa */
	{0xaa, 0x1b, 0x0000}, /* * 00,10,0a,cc */
	, 0x05, 0x05, 0x8400},				/* 8RITEACK},
	{0xa0, 0x01, ZC3XX, 0x05, 0c,e	{0xa0, 0x10, ZC3XX_R18C_AEFRE, 0xb80f},				/* , 0x05, 0,bb */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT}, 0x05, 0ANTIFLICKERMID}, /* 0RESSSELECRITEACK},
	{0204, ZC3--> 0201, ZC3XX_R090_I2CCOMMR093{0xa0, 0x007	{0x3,cc */
	{0xbb, 0	Z-Star/ALUE},
	{0xa00xa0, ZC3XX_R083_RGAINA	/* 00,f19, 0x0086},	/xa0, 3,cc */
	{0xbb, 0X_R094_I2CADDR
	{0xaa,a_GAMMA07},
	{0xK},
	{0xa0, 0x_R101_SENSORCO0, 0x01, ZC3XX_R000_SC3XX_R010_CM01_SENSORCOSELECT},	/* 00,10,0a,c18, ZC3XX_R01_SENSORCOC3XX_C3XX_R101_SENSORCORRECTION},E},
	{0xa0, 0 01,00,0d,cc */
	{0xESSSELECONTROLaa */
	{0xa,cc */
	{0xaa, 0xfe	{0xa0,6,
	{0xa0, 0x013,63, ZC3XX_R1C5_SHARP,00,cc *, ZC3XX_R189_A5, 0x21, ZC3XX_R, 0x00 ZC3XX_R1CB_SHARPNESS05},	/* sha0x01, Z01,b	{0xa0, 0x24, ZC3XX	{0xa1, 0x01, 0x01c9},
	{0xa1/
	{0xaaCT},
	{0xa0, 0x03, ZC3XX_R093_I2CSETVALUE},
	{0xaa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 00x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x09, ZC3XX_R092_I2CA 01,a8,6ELECT},
	{0xa0, 0x08, ZC3XX_R093_I2CSETVALUE},
	{0xxa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090
	{0xOMMAND},
	{0xa0, 0x0e, ZC3XX_R092_I2CADDRESSSELECT},
		{0xa0, 0x04, ZC3XX_R093_I2CS7CK},
	{0xa0, 0x01, ZC3X*/
	{0xa0d, 0xVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
YLOW},	/* 01_R092_I2CADDRESS ZC3XX_R13D_GAMMA1D},
	{0xa0, 0x00, ZC3XX_R13E_GAM,cc */
	{0xa0, 0xe6, ZC3XX_R09C_WINHEIGHTLOW},	/* 00,9c,e6,cc */
	{0xa0, 0x86, ZC3XX_R09E_WINWIDTHLOW},	/* 00,9e,86,cc */
	{0xa0, 0x98, ZC3XX_R08B_I2CD3F_GAMMA1F},
	{0xa0, 0x58, Z/
	{0xaa, 096, 0x2400},				/* 213,02,aa */
	{0xaa, 0x15, 0x0003},	/* 00,15,03,aa */
	{0xaa, 0x01, 0x0000},	/* 00,01,00,aa */
	{0xaa, 0x02, 0x0000},	/* 00,02,00,aa */
	{0xaa, 0x1a, 0x0000},	/* 00,1a,00,aa */
	{0xial[] = {
	{0xa0, 0x11, ZC3XX_Raa */
	{0xZC3XX_R191_EXPOSUREL* 00,1d,80,aa */
	{0xaa, 0x1f, 0x0008},	/* 00,1f,08,aa */
	{0xaa, 0x21, 0x0012},	/* 00,21,12,aa */
	{0xa0, 0x82, ZC3XX_R086_EXPTIMEHIGH},	/* 00,86,82,cc */
	{0xa0, 0x83, ZC3XX_R087_EXPTIMEMID},	/* 00,87,83,cc */
	{0xa0, 0x84, ZC3XX_R088_EXPTIMELO0x00, ZC3XX_,84,cc */
	{0xaa, 0x05, 0x00DIGITALGAINSTEP},
	{0xa0, 0x39,0x0a, 0x0000},	/* 00,0a,00,a* 00,02,00,dd */
	{0xbb, 0x00* 00,0b,b0,aa */
	{0xaa, 0x07},
	{0xa0, 0xe0, ZC3XX_R128_
	{0xaa, 0x0d, 0x00b0},	/*{0xa0, 0x20, ZC3XX_R18F_AEUNFR0x0000},	/* 00,0e,00,aa */
	{0xaa, 0x0f, 0x00b0}TALLIMITDIFF},
	{0xa0, 0x24, x10, 0x0000_DIGITALGAINSTEP},
	{0xa0, 0x39, ZC3XX_R01D_HSYN0,11,b0,aa */
	{0xaa, 0x16, 0x0XSTARTLOW},
	{0xa0, 0x00, ZCC3XX_R11A_FIRSTYLOW},
	{0xa0,6,aa */
	{0xaa,
	{0xaa, 0x, 0x01, ZC3XX_R090_I2CCOMMx19, 0x0086},	/* 00,19,86,aa */
	{0xaa, 0x20, 0x0000},	/* 00,20,00,aa */
	{0xaa, 0x1b, 0x0020},	/* 00,1b,20,aa */
	{0xa0, 0xb7, ZC3XX_R101_SENSORCORRECTION},	/* 01,01,b7,cc */
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,05,cc */
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},	/* 01,00,0d,cc */
	{0xa0, 0x76, ZC3XX_R189_AWBSTATUS},	/* 01,89,76,cc */
	{0xa0, 0x09, 0x01ad},	/* 01,ad,09,cc */
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},	/* 01,c5,03,cc */
	{0xa0, 0x13, ZC3XX_R1CB, ZC3XX_R1CB_SHARPNESS05},	/* sha	{0xa0, 0x
	{0xa0, 0x24, ZC3XXIXELSMODE},	/* 02,50,08,cc */
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},	/* 03,01,08,cc */
	{0xa0, 0x60, ZC3XX_R1A8_DIGITALGAIN},	/* 01,a8,60,cc */
	{0xa0, 0x85, ZC3XX_R18D_YTARGET},	/* 01,8d,85,cc */
	{0xa0, 0x00, 0x011e},	/* 01,1e,00,cc */
	{0xa0, 0x52, ZC3XX_R116_RGAIN},	/* 01,16,52,cc */
	{0xa0, 0x40, ZC3XX_R117_GGAIN},	/* 01,17,40,cc */
	{0xa0, 0x52, ZC3XX_R118_BGAIN},	/* 01,18,52,cc */
	{0xa0, 0x03, ZC3XX_R113_RGB03},	/* 01,13,03,cc */
	{}
};
static const struct usb_action gc030 {
		.id      = V4L2_CID_AUTOGAIN,
		.type    = V4L, 0x09, 000,0d,cc */
	{0xa0, 0C3XX_R092_I20x20, ZC3XX_R18FF_AEUNFREE		.sizeimage = 640 4,80	{0xa0, 0x00, ZC3XX_R1A99_DIGITALL2/
	{0xaa, 0xftruct usb_act, 0x0000},	/* 28, 0x0x0080}, /* 00,04,80,aa */
	09,cc */
	{x00, ZC3XX_R190_EXPOSALCGLOBA005_,02,00,aa *3HTHIGH},
	{0xa0, 0xe, /* 01,8T},
SETVALUE},
	{9xa0, 0x00, ZC3XX_R094_I2CVIDE 0x08, ZC3X5UNC},
	{0xa0, 0x02,  = 1,
	  _R101_SENSORCOeRECTION},	/* 01,01,b7t gspca_ERATIONMODE},	fBSTATUS},	/* 01xa0, 0x00, ZC3XX_},
	{0xa0, 0x00, ZC3X0x01, ZC9X_R10xa0, 0x76, ZC3XX*/
	{0xa0, 0_TYPE_INTEGER,
		.n0x01, ZCaAUTOc */
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},		/* 0x18, ZC3XX_R093_I2CSETVALUE},
EAN},
	{0xa0, ZC3XX_R094_I2CWRITEACK},
	{0x	{0xa0, 0x24, ZC3X0, Z2CCOMMAND},
	{0xa0, 0x15, ZC3XX_R092_I2d05,cc */
	{0xa0, 0x00, ZC3XX_ ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, ZC3XX_R090__R092_I2CADDRESSSELECT},
	{0x41ca},
	{0xa0, 0x0f01,cc */
	{0xa0, 0xe0, ZC33XX_R006_FRAMEHEIGHTLOW},	/* 00,06,e0,cc */
	{0xa0, 0x01,, ZC3XX_R001_SYSTEMOPERA_GAMMA12},
	{0xa0.minimum = 0, 0x09, 0d3},
	{YNC_2}, /* 00,1NABLE},
	{0xtep    = 1,, 0x09, 0e,6ECT},	/* 00,10,01,a2 *val);

s*/
	{0xbb, 0, 0x09, 0f ZC3a, 0x0f, 0x00b0},	{0xa0, 0xst,
	    .get,cc */
	{0xe= V4L2_COLORSPACE_ENABLE},
	{0ONMODE},	/* 01,00,0d,cc */0ECT},	/* 00,10,01,c,05,00,bb *ATUS},	/* 01,89,76,cc */
	98, ZC3XX_R_R101_SENSORCORRECTION},	/* 01,01,b7,cc */
	{0xa= 352 * 288 * 3 / 8 + 590,
		.colorspace = V4L2_COLO,05,cc */
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},	/* 01,00,0d,cc */
	{0xa0, 0x76, ZC3XX_R189_AWBSTATUS},	/* 01,89,76,cc */
	{0xa0, 0x09, 0x01ad},	/* 01,ad,09,cc */
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNEI2CSET},	/* 01,c50xa0, 0x00, ZC3XX_Rx13, ZC3_R101_SENSORCO320},
	{0xa0, 0xf4, ZC */
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},	/* 02, ZC3XX_R01F_H{0xa	__u16	idx;
};

static conACCESS},	/* 03,01,08,cc */
	{0xa0, 0x60, ZC3XX_R1A8_DIGITALGAIN},	/* 01,a8,60,cc */
	{0xa0, 0x00, 0x011e},	/* 01,1e,00,cc */
	{0xa0, 0x52, ZC3XX_R116_RGAIN},	/* 01,16,52,cc */
	{0xa0, 0x40, ZC3XX_R117_GGAIN},	/* 01,17,40,cc */
	{0xa0, 0x52, ZC3XX_R118_BGAIN},	/* 01,1ghtness(str	{0xa0, 0x03, ZC3XX_R113_RGB03},ECT},	/* 00,10,01,bTEACK},
	{0xa0, 0x01, ZC3XX_R090XX_R092_I2b},
	{0xa0, 0x14, ZC3XX_R092_I2CADDRESSSELECT},
	{0UE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0,igne6, ZC3XX_R092_I2CADDRESSSELECT},
	{0x9= V4L2_COLORSPACE_JPEG,
		.priv = 1},
	{6400, 0x0b, ZC3XX_R191_EXPOSURELIMITMID},	/* 01,91,0b,cc */
	{0xa0, 0x18, ZC3XX_R192_EXPOSU, 0x8400},				/* 84,09,8,cc */
				/
	{0xaa, 0xfe, 0x0 */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},	/* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},	d_setcontrast,
	    .get,cc */
	{0xLECT},	/* 00,10,01,cOW},	/* 01,97,8e,cc */
							/* win: 01,97,ec */
	{0xa0, 0x0e, ZC3XX_R18C_AEFREEZE},	/* 01,8c,0e,cc */
	{0xa0, 0x15, ZC3XX_R18F_AEUNFREEZE},	/* 01,8X_R189_AWBSTATUS},		/* 01,89,06,cc */
	{0xa0, 0x03,,05,cc */
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},	/* 01,00,0d,cc */
	{0xa0, 0x76, ZC3XX_R189_AWBSTATUS},	/* 01,89,76,cc */
	{0xa0, 0x09, 0x01ad},	/* 01,ad,09,cc */
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNXX_R1A8_DIGITA0x10, 0x0005}, /* 00,1c */
	{0x{0xa0, 0x09, XX_Rction adcm2700_Inita0, 0x0450,08,cc */
	{*/
	{0, 0x10, ZC3XX_R1A9_DIGITALL1,1d,60,cc */00,12,03,cc */
	{0xa_EEPROMACCESS},	/* 03,01,08,cc */
	{0xa0, 0x60, ZC3XX_R1A8_DIGITALGAIN},	/* 01,a8,60,cc */
	{0xa0, 0x00, 0x011e},	/* 01,1e,00,cc */
	{0xa0, 0x52, ZC3XX_R116_RGAIN},	/* 01,16,52,cc */
	{0xa0, 0x40, ZC3XX_R117_GGAIN},	/* 01,17,40,cc */
	{0xa0, 0x52, ZC3XX_R118_BGAIN},	/* 01,18,52,cc */
	{0xa0, 0x03, ZC3XX_R113_RGB03},	/* 01,13,03,cc */01,01,37,cc  01,90,00,cc */
	{0xa0, 0x0b, Z0_OPERATION {
	{0xaa, 0x82, 0x0000},	/* 00,82,00,aa */
	{0xaa, 0x83, 0x0002},	/* 00,83,02,aa */
	{0xaa, 0x84, 0x0038},	/* 00,84,38,aa */	/* w00,00,bb */
	{0xdd, 0x00, 0x00ZC3XX_R190_	.sizeimage = 640 * 480 * 3 / /* 01,96,00,cc */
	{0xa0, 0xec, ZC3XX_R197_ANTIFLICKERLOW},	/* 01,97,ec,cc */
	{0xa0, 0x0e, ZC3XX_R180x0400},					/* 01,8c,0e,cc */
	{0xdd, 0x00, 0x0010},8WINWIDTHLOW}0x00, ZC3XX_R195_ANTIF8/* b0,d0,07,bb */
ZC3XX_R1C5_SH{0xa0, 0x00, ZC3XX_R19{0xbb, 0x86, 0x0002 0x0400},			_DIGITALGAINSTEP},	/* 01NSORSELECT},	/* 00,, 0x62, ZC3XX_R01D_HSYNC_0},	/* 00,1d,62,cc */
	{0xa0, 0x90, ZC3XX_R01E_HSYNC_1},	/* 00,1e,90,cc */
	{0xa0, 0xc8, ZC3XX_R01F_HSYNC_2},	/* 00,1f,c8,cc */
,cc */
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF},	/* 01,a9,10,cc */
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP},	/* 01,aa,24,cc */
	{0xa0, 0x62, ZC3XX_R01D_HSYNC_0},	/* 00,1d,62,cc */
	{0xa0, 0x90, ZC3XX_R01E_HSYNC_1},	/* 00,1e,90,cc */
	{0xa0, 0xc8AN},
	{ct usb_acti0x1d, 0x00a1}, /* 00,1d,a14 0x0000},	/* 030xa0100_OPERATIONMODE},	/* 01,00,0c,cc */
	{0xaa, 0x82, 0x0000},	/* 00,82XX_R1CB_SHARPN	{0xa0, 0xb7, ZC3XX_RX_R180_AUTOCORRECTENABLE},	/* 01,80,42,cc */
/*	{0xa0, 0x85, ZC3XX_R18D_YTARGET},	 * 01,8d,85,cc *
						 * if 640x480 */
	{}
};
static const struct usb_action gc0305_60HZ[] = {
	{0xaa, 0x82, 0x0000},	/* 00,82,00,aa */
	{0xaa, 0x83, 0x0000},	/* 00,83,00,aa */
	{0xaa, 0x84, 0x00ec},	/* 00,84,ec,aa */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELI9x00ec},	/* 001,90,00,cc */
	{0xa0, 0x0b, Z9ECT},	/* 00,10,01,cc */
	{0xaaZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, xa0, 0x22, ZC3XX_R131_GAMMA11}ECT},
	{0xaX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3X, 0x0b, ZC3XX_R191_EXPOSURELIMITMID},	/* 01,91,0b,cc */
	{0xa0, 0x18, ZC3XX_R192_EXPOSU 0x01, ZC3XX_R010_CMOSS8,cc */
				 0x00, ZC3XX_R019_0 */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},	/* 01,9NABLE},
	{0x{0xa0, 0x00, ZC3XX_R196ECT},	/* 00,10,01,, ZC3XX_R1AA_DIGITALGAINSTEP},	/* 01,aa,24,cc */
	{0xa0, 0x62, ZC3XX_R01D_HSYNC_0},	/* 00,1d,62,cc */
	{0xa0, 0x90, ZC3XX_R01E_HSYNC_1},	/* 00,1e,90,cc */
	{0xa0, 0xc8, ZC3XX_R01F_HSYNC_2},	/* 00,1f,c8,cc10_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xa0, 0xd3, Z,05,cc */
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},	/* 01,00,0d,cc */
	{0xa0, 0x76, ZC3XX_R189_AWBSTATUS},	/* 01,89,76,cc */
	{0xa0, 0x09, 0x01ad},	/* 01,ad,09,cc */
	{0xa0, 0x03, ZC3XX_R1C5_SHARPN	{0xa0,ct usb_act0, 0xff, ZC3XX_R12D_GAMMA0D 0x0_SYSTEMOPERATI,
		.default_value },	/* 01,00,0c,cc */
	{0xaa, 0x82, 0x0000},	/* 00,801, 0x01c8},
	{0xa,
		.default_value = 1,
	  CCESS},	/* 03,01,08,cc */
	{0xa0, 0x60, ZC3XX_R1A8_DIGITALGAIN},	/* 01,a8,60,ccAIN},	/* 01,17,40,cc */
	{0xa0, 0x52, ZC3XX_R118_BGAIN},	/* 01,18,52,cc */
	{0xa0, 0x03, ZC3XX_R113_RGB03},	/* 01,13,03,cc */_R13IGH},	/* 01,90,00,cc */
	{0xa0, 0x0b, ZfKSELECT},	/* 00,02,04,cc */
	{0xa0, 0x02, ZC3XX_R0UE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0,ETVALUE},
	{, 0x0018},

	{0xaa, 0x15, 0x0	{0xa0, 0x88, ZC3XXLICKERMID},	/* 01,96,00,cc */
	{0xa0, 0xec, ZC3XX_R197_ANTIFLICKERLOW},	/* 01,97,ec,cc */
	{0xa0, 0x0e, ZC3XX_R1 0x0d, ZC3XX_R100_OPERATIONMODE},1e,00,cc *,0d,cc */
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},		/* /
	{0xa0, 18F_AEUNFREEZE},	/* 01,8f,15,c0xff, ZC3XX_R020_HSYNC_3}C3XX_R010_CMOSSENS0x00, ZC3XX_ 0x00, ZC3XX_R195_ANTIFL ZC3XX_R195_ANTIFLIb3, ZC3XX_R0{0xa0, 0x00, ZC3XX_R199C3XX_R010_CMOSSENSCB_SHARPNESS05},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xa1, 0x01, 0x0002},
	{0xa1, 0x01, 0x0008},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x02, ZC3XX_R180_AUTOCORRECTENABLE},,cc */
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF},	/* 01,a9,10,cc */
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP},	/* 01,aa,24,cc */
	{0xa0, 0x62, ZC3XX_R01D_HSYNC_0},	/* 00,1d,62,cc */
	{0xa0, 0x90, ZC3XX_R01E_HSYNC_1},	/* 00,1e,90,cc */
	{0xa0, 0xc80xa0,x01c8},
	{0xXX_R12F_GAMMA0F},
		{0xa1, 0x01, 0x01ca},
	{0xa0, 0x0f, ZC3XX_R1CB_SHARPNESS05},	/* sharpness- */
	{0xa0, 0x13, ZC3R110_RGB20},
	0,
	{0xa0, 0x1c, 
	{0xa0, 0x38, ZC3XX_R121_GAMMA01},
	{0xa0, 0x59, ZC3XX_R122_GAMMA02},
	{0xa0, 0x79, ZC3XX_R123_GAMMA03},
	{0xa0, 0x92, ZC3XX_R124_GAMMA04},
	{0xa0, 0xa7, ZCx00ec},	/* 00,84,ec,aa */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELI 0xc8, ZC3XX_R127_GAMMA07},
	{0xa0, 0xd4, ZC3XX_R128_GAMMA08},
	{0xa0, 0xdf, ZC3XX_R129_GAMMA09},
	{0xa0, 0xe7, ZC3XX_R12A_GAMMA0A},
	{0xa0, 0xee, ZC3XX_R12B_GAMMA0B},
	{0xa0, 0xf4, ZC3XX_R12C_GAMMA0C},
	{0xa0, 0xf9, ZC3XX_R12D_GAMMA0D},
	{0xa0, 0xfc, ZC3XX_R12E_GAMMA0E},
	{0xa0, 0xff, ZC3XX_R12F_GAMMA0F},
	{0xa0, 0x26, ZC3XX_R130_GAMMA10},
	{0xa0, 0x22, ZC3XX_R131_GAMMA11},
	{0xa0, 0x20, ZC3XX_R132_GAMMA12},
	{0xa0, 0x1c, ZC3XX_R133_GAMMA13},
	{0xa0, 0x16, ZC3XX_R134_GAMMA14},
	{0xa0, 0x13, ZC3XX_R135_GAMMA15},
	{0xa0, 0x10, ZC3XX_R136_GAMMA16},
	{0xa0, 0x0d, ZC3XX_R137_GAMMA17},
	{0xa0, 0x0b, ZC3XX_R138_GAMMA18},
	{0xa0, 0x09, ZC3XX_R139_GAMMA19},
	{0xa0, 0x07, ZC3XX_R13A_GAMMA1A},
	{0xa0, 0x06, ZC3XX_R13B_GAMMA1B},
	{0xa0, 0x05, ZC3XX_R13C_GAMMA1C},
	{0xa0, 0x04, ZC3XX_R13D_GAMMA1D},
	{0xa0,c */
	{0xa0, 0x00, ZC3XX_R11A_FIRb03303xXX_R138_GAMMA18},
	{0xa0, 0x09, ZC3XX_R139_GAMMA19},
	{0xa0, 0x07,MMA1D},
	{0xa0, 0x03, ZC3XX_R13E_GAMMA1E}value = 128,X_R10A_RGB00},	/* matrix */
	{0 ZC3XX_R13A_GAMMA1A},
	{0xa0, 0x06, ZC3X0D_RGB10},
	{0xa0, 0x58, ZC3XX_R10E_RGB11},
	{0xa0, 0xf4, ZC3XX_R10F_RGB12},
	{0xa0, 0xf4, ZC3XX_R110_RGB20},
	{0xa0, 0xf4, ZC3XX_R111_RGB21},
	{0xa0, 0x58, ZC3XX_R112d_R09E_WINWI	{0xa0, 0x01, ZC3X
	{08b -> d30_GAMMA10},
	{0xa0, 0x22,0x02, ZC3XX_R13F_GAMMA1F},
	{0xa0CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x14, ZC3XX_R092_I2OMMAND},
	{0xa0, 0x13, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x22, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMANFUNC},
	{0xa0, 0x01, ZC3XX_R0, 0x8400},				/* 84,07,not, write to the FrVimicro zc301/z01, ZC4OMMAND},
	{0xa0, ZC3XX_RT},
	{0xa0, 0x22, ZC3XXSE.  See the
 *CKSETTIgspca_dev c301/zc302p/1eal Public License
 * 28 Public LicenseMELOW3OMMAND},
	{0xa0, 0x21,192_I2x00a1}, /* 0YSTEMOPERATEHIGH},
	{0xa0, 0x24 *
 * This pro3_I2CCOMMAND},
	{0xa0, 18, ,cc */
	0A3_EXPOSURECKSETT4 0x0018},	/* or1,80,aa VIDE_R012_VIDEOCD_RGB10}AND},
	{0xa0, 0{0xa0, 0x18_R012_VIDEOCic int UNC},
	{0xxa0, 0x5d, ZC3XX_RC3XX_R094_I2CWRITEACK},
	X_R094_I2CWRITEACK},
	{0xa0, 0x01,0, 0x07, ZC3XX_R191_EXPOSURELIMITMID},
	{0xa0, 0xee, ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},
	{0xa0, 0x0c, ZC3XX_R1A9_DIGITALLIMITDIFF},
	{0xa0, 0x28, ZC3XX_R1AA_DIGITALGAINSTEP},
	{0xa0, 0x04, ZC3XX_R01D_HSYNC_0},
	{0xa0, 0SURETIMELOW},
	{0NSOR_ICM105A 7
#define SENSOR_MC501CB 8
x10, Zxa0, 0x22, R_PAS202B 12
#define 6LMEAN},
	{0xa0, 0x14, ZC3XX_,
	{0xa0, 0x60, ZC3XX_R11D_G 0x1b, C3XX_R18C_AEFREE,	/* 011a

/* spec, 0x08, Zxa0, 0xf4, ZC3XX_R10012_VIDE,
	{0xa0, 0x58, ? */
	{ATIONMODE},
	{_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x13, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x96, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R0a0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0xD_HSYNC_0},
	{0xa0, 0x0f, ZC3XX_R01E_HSYNC_1}2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x15, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE ZC3XX_R090_MMAND},
	{0xa0, 0x18,TEACK},
	{0xa0, 11C_FIRSTXL_R090_I2CCOMMAND},
	{0xa, ZC3XX_R094XX_R092_I2CADDRESSSELEC, ZC3XX_R0940x0c, ZC3XX_R093_I2CSETCOMMAND},
	{a0, 0x00, ZC3XX_R094_I2SELECT},
	{0	{0xa0, 0x01, ZC3XX_R09, ZC3XX_R094},
	{0xa0, 0x17, ZC3XX_ZC3XX_R130_GESSSELECT},
	{0xa0, 0x0VALUE},
	{0x0, 0x01, ZC3XX_R090_I2dT},
	{0xa0, _R094_I2CWRITEACK},
	{0SELECT},
	{0LECT},
	{0xa0, 0x01, Ze3XX_R093_I2CSETVALUE},
	{0xa0, 0x0C3XX_R090_I24_I2CWRITEACK},
	{0xa0, , ZC3XX_R094R090_I2CCOMMAND},
	{0xaDRESSSELECT}CK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0xf7, Z0_I2CCOMMANDNSORCORRECTION},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x78, ZC3XX_R18D_YTARGET},
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x20, ZC3XX_R087_EXPTIMEMID},
	{0xa0, 0x21, ZC3XX_R088_EXPTIMELOW},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xa0, 0x00, 0x01ad},
	{0xa0, 0x01, 0x01b1},
	{0xa0, 0x02, ZC3XX_RITEACK},
	{, 0x00, ZC3XX_R094_I2CW, 0x05, ZC3X0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x13, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x96, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x14, ZC3XX_R092_I2CADDRESSSELECal);
static int sd_setgamma(struct gspca_dev *gspca_dev, __s32 val);
static int sd_getgamma(7_CALCGLong with this p1, 0x013efine SENSOR_GC
#define BRIGHTNESS_IDX 0
#define SD_BRIGHTNE8,52,cc */
	{0xa0, 0x03, ZC3XX_R 0x86, ZC3XX_R09E_WINWMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CE},
	{0xa0, {0xa0, 0x01, ZC3XX_R090_I2CCOMM3XX_R093_I20, 0x00, ZC3XX_R1A7_CALCGAND},
	{0xa0ine SD_CONTRAST 1
	{
	    */
	{0xa0, 0x00, 0x011e},	/* 01,, 0x52, ZC3XX_R116_RGAIN},	/* 01,16,52,cc *
	{0xa0, 0x0c, ZC3XX_R	.minimum = 0,
		.maximuC3XX_R090_I.step    = 1,
		.default ZC3XX_R13C8,
	    },
	    .set = sd_setcontrast,
	    .get = sd_getconame    = "Gamma",
		.minimum = 1,
		.maximum01, 0x01c8},
	{0xa1, 0x01, 0x01c9},
	{0xa1, 0x	{0xa0, 0x96, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3X3XX_R137_GAMMA17},
	{0xa0, 0x0b, ZC3XX_R138_GAMMA18},
	{0xa0, 0x09, ZC3XX_R139_GAMMA19, 0x00, ZC3XX_RMEHEIGHTLOW},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0,{0xa0, 0x08, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH}, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},
	{},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},
	{0xa0, 0xe8, ZC3XX_R09C_WINHEIGHTLOW},
	{0xa0, 0x88, ZC3XX_R09E_WINWIDTHLOW},
	{0xaa, 0x1c, 0x0000},
	{0xaa, 0x0a, 0x0001},
	{0xaa, 0x0b, 0x0006},
	{0xaa, 0x0c, 0x007a},
	{0xaa, 0x0d, 0x00a7},
	{0xaa, 0x03, 0x00fb},
	{0xaa, 0x05, 0x0000},
	{0xaa, 0x06, 0x0003},
	{0xaa, 0x09, 0x0008},
	{0xaa, 0x0f, 0x0018},	/* original setting */
	{0xaa, 0x10, 0x0018},
	{0xaa, 0x11, 0x0018},
	{0xaa, 0x12, 0x0018},
	{0xaa, 0x15, 0x004e},
	{0xaa, 0x1c, 0x0004},
	{0xa0, 0xf7, ZC3XX_R101_SENSORCORRECTION},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x70, ZC3XX_R18D_YTARGET},
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xa1, 0x01, 0x0002},
	{0xa1, 0x01, 0x0008},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x02, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x40, ZC3XX_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GGAIN},
	{0xa0, 0x40, ZC3XX_R118_BGAIN},
	{0xa1, 0x01, 0x0008},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* clock ? */
	{0xa0, 0x08, ZC3XX_R1C6_SHARPNESS00},	/* sharpness+ */
 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,
	{0xa1, 0x01, 0x01ca},
	{0xa0, 0x0f, ZC3XX_R1CB_SHARPNESS05},	/* sharpness- */
	{0xa0, 0x13, ZC3XX_R120_GAMMA00},	/* gamma 4 */
	{0xa0, 0x38, ZC3XX_R121_GAMMA01},
	{0xa0, 0x59, ZC3XX_R122_GAMMA02},
	{0xa0, 0x79, ZC3XX_R123_GAMMA03},
	{0xa0, 0x92, ZC3XX_R124_GAMMA04},
	{0xa0, 0xa7, ZC3XX_R125_GAMMA05},
	{0xa0, 0xb9, ZC3XX_R126_GAMMA06},
	{0xa0, 0xc8, ZC3XX_R127_GAMMA07},
	{0xa0, 0xd4, ZC3XX_R128_GAMMA08},
	{0xa0, 0xdf, ZC3XX_R129_GAMMA09},30C_VF0250 17
#define SENSOR_MAX 18
	unsigned short chip_revision;

	u8 *jpeg_hdr;
};

/* V4L2 controls supported by the driver */
static int sd_setbrightness(struct gspca_dev *gspca_dev, __s32 val);
static 
	{0xa0, 0xff, ZC3XX_R12F_GAMMA0F},
	{0xa0, 0x26, ZC3XX_R130_GAMMA10},
	{0xa0, 0x22, ZC3XX_R131_GAMMA11},
	{0xa0, 0x20, ZC3XX_R132_GAMMA12},
	{0xa0, 0x1c, ZC3XX_R133_GAMMA13},
	{0xa0, 0x16, ZC3XX_R134_GAMMA14},
	{0xa0, 0x13, ZC3XX_R135_GAMMA15},
	{0xa0, 0x10, ZC3XX_R136_GAMMA16},
	{0xa0, 0x0d, ZC3XX_R137_GAMMA17},
	{0xa0, 0x0b, ZC3XX_R138_GAMMA18},
	{0xa0, 0x09, ZC3XX_R139_GAMMA19},
	{0xa0, 0x07, ZC3XX_R13A_GAMMA1A},
	{0xa0, 0x06, ZC3XX_R13B_GAMMA1B},
	{0xa0, 0x05, ZC3XX_R13C_GAMMA1C},
	{0xa0, 0x04, ZC3XX_R13D_GAMMA1D},
	{0xa0, 0x03, ZC3XX_R13E_GAMMA1E},
	{0xa0, 0x02, ZC3XX_R13F_GAMMA1F},
	{0xa0, 0x66, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xed, ZC3XX_R10B_RGB01},
	{0xa0, 0xed, ZC3XX_R10C_RGB02},
	{0xa0, 0xed, ZC3XX_R10D_RGB10},
	{0xa0, 0x66, ZC3XX_R10E_RGB11},
	{0xa0, 0xed, ZC3XX_R10F_RGB12},
	{0xa0, 0xed, ZC3XX_R110_RGB20},
	{0xa0, 0xed, ZC3XX_R111_RGB21},
	{0xa0, 0x66, ZC3XX_R112_RGB22},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x00, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
 /**** set exposure ***/
	{0xaa, 0x13, 0x0031},
	{0xaa, 0x14, 0x0001},
	{0xaa, 0x0e, 0x0004},
	{0xaa, 0x19, 0x00cd},
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},
	{0xa0, 0x02, ZC3XX_R191_EXPOSURELIMITMID},
	{0xa0, 0x62, ZC3XX_R192_EXPOSURELIMITLOW},
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID},
	{0xa0, 0x3d, ZC3XX_R197_ANTIFLICKERLOW},
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},
	{0xa0, 0x0c, ZC3XX_R1A9_DIGITALLIMITDIFF},
	{0xa0, 0x28, ZC3XX_R1AA_DIGITALGAINSTEP},
	{0xa0, 0x04, ZC3XX_R01D_HSYNC_0},
	{0xa0, 0x18, ZC3XX_R01E_HSYNC_1},
	{0xa0, 0x2c, ZC3XX_R01F_HSYNC_2},
	{0xa0, 0x41, ZC3XX_R020_HSYNC_3},
	{0xa0, 0x60, ZC3XX_R11D_GLOBALGAIN},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x40, ZC3XX_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GGAIN},
	{0xa0, 0x40, ZC3XX_R118_BGAIN},
	{}
};
static const struct usb_action hdcs2020b_50HZ[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xaa, 0x13, 0x0018},			/* 00,13,18,aa */
	{0xaa, 0x14, 0x0001},			/* 00,14,01,aa */
	{0xaa, 0x0e, 0x0005},			/* 00,0e,05,aa */
	{0xaa, 0x19, 0x001f},			/* 00,19,1f,aa */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x02, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,02,cc */
	{0xa0, 0x76, ZC3XX_R192_EXPOSURELIMITLOW}, /* 0192,76,cc */
	{0xa0, 0x00, ZC3XX_R195_x, ZC3XX_R002_CLOCKSEL
/*#define SENSOR_OV7648 9 - same va0, 0x01, ZC3XX_R*/
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0
	{0xa 0x00, ZC3XX_R0},
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTRO},
	{0xa0, 0x02, ZC3XX_R13F_GAMMA1F},
	{0xa0 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x08, 0x13, 0x00	{0xa0, 0x01, ZC3XX_R090_I2CCOMM0xa0, 0x13, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x22, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMA	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}	{0xaa, 0x06, 0x0008},
	{0xaa, 0x0f, 0x0018},	/* original setting */
	{0xaa, 0x10, 0x0018},
	{0xaaRITEACK},
	018},
	{0xaa, 0x11, 0x0018},
	{0xaa, 0x12, 0x0018},
	{0xaLUE},
f7{
	{04e},
	{0xaa, 0x1c, 0x0004},
	{0xa0, 0xf7, ZC3XX_R101_SEN				_R092_I2018},
	{0xaa, 0x15,90_EXPOSURELIMI0x68, Z68f/* {0xaa, 0x0SORSELE1eZC3XX_R101_SENSORCORRECTION},
	{0xa0, 0x05, ZC3XX_R012_},
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xa1, 0x01, 0x0002},
	{0xa1, 0x01, 0x0008},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x02, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x40, ZC3XX_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GGAIN},
	{0xa0, 0x40, ZC3XX_R118_BGAIN},
	{0xa1, 0x01, 0x006AND},
	{0xa0SENSOR_MC501CB 8
#define SENSOR_OV7620 91, 0x01, 0x01c8},
	{0xa1, 0x01, 0x01c9},
	{0xa1, 0x0_action adcm) any later version.
 *
 * This/
	{0xaa, 0x1useful,
 * but WITHOUT ANY WARRANTY; withouUT ANY WARRANT9_DIGITALUT ANY WARRANT093_I2CSETVALULOBALGAIN},
	{0xa0, 0x60, ZC3XX_R11D_GLOBALGAIN},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x60, ZC3XX_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GGAIN},
	{0xa0, 0x4c, ZC3XX_R118_BGAIN},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x20, ZC3XXRESSSELECT},
	{0xa0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x21, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x5c, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0,ERLOW},	/* 01,97,60,cc */
	{0xa0, 0x0c, ZC3XX_R18C_AEFREEZE},	/* 01,8c,0c,cc */
	{0xa0, 0x18, ZC3XX_R18F_AEUNFREEZE},	/* 01,8f,18,cc */
	{0xa0, 0x18, ZC3XX_R1A9_DIGITALLIMITDIFF},	/* 01,a9,18,cc */
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP},	/* 01,aa,24,cc */
	{0xa0, 0x00, ZC3XX_R01D_HSYNC_0},	/* 00,1d,00,cc */
	{0xa0, 0x50, ZCv, __s32 *val);
static int sd_setgamma(struct gspca_dev *gspca_dev, __s32 val);
static int sd_getgamma(, 0x0080SE.  See the
 *	/* 002b_I2CWRITEACK},
,	/* 01,ad,019_AUTOADJU90_I2CCOMMAND},
	{0xa0, 0x21, ZC3XX_R01F_HSYNC_2},
	{0xa0, 0x41, ZC3XX_R020_HSYNC_3},
	{0xa0, 0x60, ZC3XX_R11D_GLOBALGAIN},
	{08a0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x40, ZC3XX8ESS,
		.type,
	    },
	    .set = sd_setbrightness,
	    .get = sd_getbrightness,
	},
#define SD_CONTRAST 1
	{
	     hdcs2020b_50HZ[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xaa, 0x13, 0x0018},			/* 00,13,18,aa *C3XX_R136_GAMMA16},
	{0AIN},
	{0xa0, 0x04, ZC8,
	    },
	    .set = sd_setcontrast,
	    .get = sd_getcon_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x02, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,02,cc */
	{0xa0, 0x76, ZC3XX_R192_EXPOSURELIMITLOW}, R098_WINYSTARTLOW},
	{0xa0, 0x00, ZC3XX_ Pub0_I2CCOMMANDxa0, 0x24},		ZC3XX_R132_GAMMI2CADDRESSSEsb_action cs2102_50HZScale[] = {
	{0xa0, 0xOW},
	{0xa0, 0x0= 352 * 288 * 3 /8_WINYSTARTLOW},
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},
	{0xaa, 0x30, 0x002d},
	{0xaa, 0x01, 0x0005},
	{0xaa, 0xf4, ZC3XX_R10B_RGB01},
	{0xa0,1OW},
	{0xaa, 0x30D_RGB10},
	{0xa0, 0x58, ZC3XX_R10E_RGB11},
	{0xa0, 0xf4, ZC3XX_R10F_RGB12},
	{0xa0, 0xf4, ZC3XX_R110_RGB20},
	{0xa0, 0xf4, ZC3XX_R111_RGB21},
	{0xa0, 0x58, ZC3XX_R112_RGB22},
	{0x0038},
	{0xaa, 0x5b, 0x0001},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
	{0xa0, 0x70, ZC3XX_R18D_YTARGET},
	{0xa0, 0x60, ZC3XX_R1A8_DIGITALGAIN},
	{0xa0, 0x00, 0x01ad},
	{0xa0, 0xc0, 0x019b},
	{0xa0, 0xa0, 0x019c},
	{0xa0, 0x02, ZC3XX_R188_MINGAIN},
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xaa, 0x02, 0x0090},	/* {0xaa, 0x02, 0x0080}, */
	{}
};
static const struct usb_action hv7131b_50HZ[] = {	/* 640x480*/
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},	/* 00,19,00,cc */
	{0xaa, 0x25, 0x0007},			/* 00,25,07,aa */
	{0xaa, 0x26, 0x0053},			/* 00,26,53,aa */
	{0xaa, 0x27, 0x0000},			/* 00,27,00,aa */
	{0xaa, 0x20, 0x0000},			/* 00,20,00,aa */
	{0xaa, 0x21, 0x0050},			/* 00,21,50,aa */
	{0xaa, 0x22, 0x001b},			/* 00,22,1b,aa */
	{0xaa, 0x23, 0x00fc},			/* 00,23,fc,aa */
	{0xa0, 0x2f, ZC3XX_R190_EXPOSURELIMITHIGH},	/* 01,90,2f,cc */
	{0xa0, 0x9b, ZC3XX_R191_EXPOSURELIMITMID},	/* 01,91,9b,cc */
	{0xa0, 0x80, ZC3XX_R192_EXPOSURELIMITLOW},	/* 01,92,80,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},	/* 01,95,00,cc */
	{0xa0, 0xea, ZC3XX_R196_ANTIFLICKERMID},	/* 01,96,ea,cc */
	{0xa0, 0x60, ZC3XX_R197_ANTIFLICKERLOW},	/* 01,97,60,cc */
	{0xa0, 0x0c, ZC3XX_R18C_AEFREEZE},	/* 01,8c,0c,cc */
	{0xa0, 0x18, ZC3XX_R18F_AEUNFREEZE},	/* 01,8f,18,cc */
	{0xa0, 0x18, ZC3XX_R1A9_DIGITALLIMITDIFF},	/* 01,a9,18,cc */
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP},	/* 01,aa,24,cc */
	{0xa0, 0x00, ZC3XX_R01D_HSYNC_0},	/* 00,1d,00,cc */
	{0xa0, 0x50, ZC3XX_R01E_HSYNC_1},	/* 00,1e,50,cc */
	{0xa0, 0x1b, ZC3XX_R01F_HSYNC_2},	/* 00,1f,1b,cc */
	{0xa0, 0xfc, ZC3XX_R020_HSYNC_3},	/* 00,20,fc,cc */
	{}
};
static const struct usb_action hv7131b_50HZScale[] = {	/* 320x240 */
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},	/* 00,19,00,cc */
	{0xaa, 0x25, 0x0007},			/* 00,25,07,aa */
	{0xaa, 0x26, 0x0053},			/* 00,26,53,aa */
	{0xaa, 0x27, 0x0000},			/* 00,27,00,aa */
	{0xaa, 0x20, 0x0000},			/* 00,20,00,aa */
	{0xaa, 0x21, 0x0050},			/* 00,21,50,aa */
	{0xaa, 0x22, 0x0012},			/* 00,22,12,aa */
	{0xaa, 0x23, 0x0080},			/* 00,23,80,aa */
	{0xa0, 0x2f, ZC3XX_R190_EXPOSURELIMITHIGH},	/* 01,90,2f,cc */
	{0xa0, 0x9b, ZC3XX_R191_EXPOSURELIMITMID},	/* 01,91,9b,cc */
	{0xa0, 0x80, ZC3XX_R192_EPOSURELIMITLOW},	/* 01,92,80,cc */
	{0xa0, 0x01, ZC3XX_R195_ANTIFLICKERHIGH},	/* 01,95,01,cc */
	{0xa0, 0xd4, ZC3XX_R196_ANTIFLICKERMID},	/* 01,96,d4,cc */
	{0xa0, 0xc0, ZC3XX_R197_ANTIFLICKERLOW},	/* 01,97,c0,cc */
	{0xa0, 0x07, ZC3XX_R18C_AEFREEZE},	/* 01,8c,07,cc */
	{0xa0, 0x0f, ZC3XX_R18F_AEUNFREEZE},	/* 01,8f,0f,cc */
	{0xa0, 0x18, ZC3XX_R1A9_DIGITALLIMITDIFF},	/* 01,a9,18,cc */
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP},	/* 01,aa,24,cc */
	{0xa0, 0x00, ZC3XX_R01D_HSYNC_0},	/* 00,1d,00,cc */
	{0xa0, 0x50, ZC3XX_R01E_HSYNC_1},	/* 00,1e,50,cc */
	{0xa0, 0x12, ZC3XX_R01F_HSYNC_2},	/* 00,1f,12,cc */
	{0xa0, 0x80, ZC3XX_R020_HSYNC_3},	/* 00,20,80,cc */
	{}
};
static const struct usb_action hv7131b_60HZ[] = {	/* 640x480*/
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},	/* 00,19,00,cc */
	{0xaa, 0x25, 0x0007},			/* 00,25,07,aa */
	{0xaa, 0x26, 0x00a1},			/* 00,26,1,aa */
	{0xaa, 0x27, 0x0020},			/* 00, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 8,52,cc */
	{0xa0, 0x03, ZC3XX_R 2,
	    },	{0xa0, 0x24, ZC3X ZC3XX_R13A_	    .get = sd_getsharpness,
	}e},
	{0xa0, 0x14, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x02, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, _1},
	{0xa0, * 3 / 8 + 590,
		.colorspace , 0x44, ZC3XX_R121xbb, 0x0f, 0x140f},				/* 14,0f,0f,bb */
	{0xa0, 0xb7, ZC3XX_R101_SENSORCORRECTION},	/* 01,01,37,cc */
	{0xa0, a0, 0x42, Z},
	{0xa0, 0x00, ZC3XX_R093_I2CSc */
	{0xbb, 0x00, ZC3XX_R13F_;

static const struct v4l2_pi2, 0x44, ZC3XX_R1210x01, ZC3XX_144, V4L2_PIX_FMT_JPEGstruct usb_action c,
	{0xa0, 0x0ne = 176,
		.sizeimag,
	{0xa0, 0x09, ZC3ontrast",
		.colorspace = V4L2_COL, V4L2_FIEL	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},00,aa */
	= 352 * 288 * 3 / 8 + 590,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.priv = 0 0x0000},			/* 00,20,00,aa */
	{0xaa, 0x21, 0x00a0},			/* 00,21,a0,aa */
	{0xaa, 0x22, 0x0016},			/* 00,22,16,aa */
	{0xaa, 0x23, XX_R0A4_EXPO	    .get = sd_getsharpness,
	}aKSELECT},	/* 00,02,04,cc */
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},	/* 00,03,02,cc */
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},	/* 00,04,87*/
	{0xa0, 0x0f, ZC3XX_R18F_AE 0x18, ZC3X
};

static const TIFLICKERHIGH},	/* 01,95,00,cc */
	{0xa0, 0x02, ZC3XX_R196_ANTIFLICKERMID},	/* 01,96,02,cc */
	{0xa0, 0x00, ZC3XX_R197_ANTIFLICKERLOW},	/* 01,97,00,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},	/* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_e/
	{0xa0, 0xff, ZC3XX_Rc,de,cc */
dd, 0x00, 0x0010},	9_DIGITALLIMITDIFF},	/* 01,a9,00,C3XX_R128_GAMMA08}, 00,1e,50,cc */
	{0xa0, ,bb */
	{0x,aa,00,cc */
	{0xa0, 0x00, ZC3XX_R01D_HSYNC_0},	/* 00X_R189_AWBSTATUS},		/* 01,89,06,cc */
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMOD 0x0000},			/* 00,20,00,aa */
	{0xaa, 0x21, 0x00a0},			/* 00,21,a0,aa */
	{0xaa, 0x22, 0x0016},			/* 00,22,16,aa */
	{0xaa, 0x23, x11,rpness,
	    .get = sd_getsharpness,
	}dL2_CTRL_TYPE_INTEGER,
		.name  TIMEMID},
	{0xa0, 0x21, ZC3XX_R088_EXPTIMELOW},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3X0x68, 0x22, ZC3XX_R131_GAMMA11},
	{0xa0, 3EXPOSURELIMITHIGH},	/* 01,90,00,cc */
	{0xa00,cc */
	{0xa0, 0x02, ZC3XX_R196_ANTIFLICKERMID},	/* 01,96,02,cc */
	{0xa0, 0x00, ZC3XX_R197_ANTIFLICKERLOW},	/* 01,97,00,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},	/* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_ial[FREEZE},	/* 01,8f,20,cc */
	{04XX_R124_GAMMA04},
,80,aa */
	{MITDIFF},	/* 01,a9,00,OSURELIMITHIGH}, /*,
	{0xa0, 0x0b, ZC3XX_R1= V4L2_COL,
	{0xa0, 0C3XX_R09B_WINHEIGHTHIGH},
	{0xa0, 0xe8, ZC3XX_R00,cc */
	{0xa0, 0xa0, ZC3XX_R01E_HSYNC_1},	/* 00,1e,a0,cc */
	{0xa0, 0x16, ZC3XX_R01F_HSYNC_2},	/* 00,1f,16,cc */
	{0xa0, 0x40, ZC3XX_R020_HSYNC_3},	/* 00,20,40,cc */
	{}
};

static const struct usb_action hv7131cxx_Initial[] = {
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},
	{0xa0, 0x10, ZC3XX_R002_CLOCKSELECT},
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x77, ZC3XX_R101_SENSORCORRECTION},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x07, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAM8C_AEFREEZE},	/* 01,8c,0R1CB_SHARPN 0x0080}, /* 00,1d,80,aa */
	{50_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xa1, 0x01, 0x0002},
	{0xa0, 0x00, ZC3XX_R092_I2CADDRESSSELECT},
	{10_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xa0, 0xd3, ZC3XX_R08B_I2CDEVICEADDR} 0x0000},			/* 00,20,00,aa */
	{0xaa, 0x21, 0x00a0},			/* 00,21,a0,aa */
	{0xaa, 0x22, 0x0016},			/* 00,22,16,aa */
	{0xaa, 0x23, },
	{0xa0, Initial[] = {
	{0xa0, 0x01, ZC3XC3XX_R128_GAMMA08},
	{0xa0, 0xdf, ZC3XX_R129_GAMMA0	{0xa0, 0x88, ZC3XX_R09E_WINWIDTHLOW},		/* 00,9e,88,cc */
	{0xbb, 0x00, 0x0400},				/* 04,00,00,bb */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xbb, 0x0f, 0x140f},				/* 14,0f,0f,bb */
	{0xa0, 0xb7, ZC3XX_R101_SENSORCORRECTION},	/* 01,01,37,cc */
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},		/* 01,00,0d,cc */
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},		/* 01,89,06,cc */
	{0xa0, 0x03, , 0x21, ZC3,	/* 01,8f,20,cc */
	{009 ZC3XX_R134_GAMMA14},
	{0xa0, ne = 176,
		.sizeimag15},
	{0xa0, 0x10, 0x25, 0x0007},
	{0xaa, 0x26, 0x0053},
	{0xaa, 0x27, 0x0000},

	{0xa0, 0x10, ZC3XX_R190_EXPOSUREL00,cc */
	{0xa0, 0xa0, ZC3XX_R01E_HSYNC_1},	/* 00,1e,a0,cc */
	{0xa0, 0x16, ZC3XX_R01F_HSYNC_2},	/* 00,1f,16,cc */
	{0xa0, 0x40, ZC3XX_R020_HSYNC_3},	/* 00,20,40,cc */
	{}
};

static const struct usb_actionCKERMID},
	{0xa0, 0xc0, ZC3XX_R197_ANTIFLICKERLOW},
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},
	{0xa0, 0x60, ZC3XX_R1A8_DIGITALGAIN},
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF},
	{0xa0, 0x13, ZC3XX_R1AA_DIGITALGAINSTEP},
	{0xa1, 0x01, 0x3XX_R006_FRAMEHEIGHTLOW},	/* 00,06,e0,cc */
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERAa0, 0x40, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{}
};

static const struct usb_action hv71,cc */
			ialScale[] = {
	{0xa0, 0x01, ZC3XX_R000_SYSR195_ANTIF},

	{0xa0, 0x00, ZC3XX_R002_CLOCKSELECT},	8_GAMMA18},
	{0xa0, 00, 0x0005}, /*oem91d,aa,aa */
PO2030, 0x00, ZC3XX_R190_E- (close to CS21023_I2Camma,
	},
#define SD_AUTOGAIN 36, 0x0xa0, 0x3f, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,3f,cc */
	{0xa0, 0xe4, ZC3XX_R19t your optio0xa0, 0x00, ZC3XX_R190_EXP0, 0x0a, ZC3XX_R010_OSSENSORSELECT},	/* 00,10,0a,cc */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xbb, 0x04, 0x0400},				/* 04,04,00,bb */
			/* 00,26,0_HBLANK,fe,10,aa */
	/
	{0KERMID}, /* 01,96,00,cc */
	81 ZC3XX_c const struc1e, 0x0002},				/* 00IONMODE},
	EEZE}, /* 01,8c,10,cc */
1x00, ZC3XX_R195_ANTc8}, /* 00,cEZE}, /* 01,8f,20,cc */CADDRESSSELECT},
	{NESS,
		.tyTALLIMITDIFF}, /* 01,a9,10,,
};

static const 
	{0xa0, 0x6,d0,aa */
	{0xaa, 0x28, 01= V4L2_COLORSPACE_JFUNC},
	{0xa01D_HSYNC_0}, /* 00,1d,5dc */
	{0xbb, 0x00, ZC3XX_R13A
	{0xa0, 0x01, ZC3XX_R010_CMOZC3XX_R190_EXPOSUREL	{0xdd, 0x00, 0x0100},				/* 00,01,00,dd */
	{0xbb, 0x01, aa */
	{0xaa, 0x11, 0x00be}, /* 00,11,be,aa */
	{0xaa, 0x1b, 0x0000}, /* 00,1b,00,aa */
	{0xaa, 0x1c, 0x0005}, /* 00,1c,05,acs2102_60HZScale[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xaa, 0x0f, 0x00b7}, /* 00,0f,b7,aa */
	{0xaa, 0x03, 0x0005}, /* 00,03,05,aa */
	{0xaa, 0x04, 0C3XX_R111_RGB21},
	{0xa0, 0x58,
	{0xa0, 0aa, 0x1d, 0x0080},		.id      = V4L2_CID_GAMMA,
		.typ, ZC3XX_R1ZC3XX_R100_OPER001_Sstruct g50,08,atic const sMA07},
	{0xa0, 0xd4, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x01, ZC3XX_R0,	/* 00,0f,b0,aa */
	{0xaa, 0x0, 0x0000},	/* 00,10,00,aa *0001},	/* 00,16,01,aa */
	{0xa, 0x17, 0x00e6},	/* 00,17,e6	/* 00,c  },
	    .9,c = sd_setautogain,
EACK},
	t struct usb01ad},	/* 01,ad,09,cX_R100_O5
	{0xa0, 0xdLMEA3,cc */
	{0xbb, 0x00, 0xeb = sd_getauteb0x01, ZC3XX_R001_S8 lightfLECT},
	{0xENSORx01, ZC3XX_R001_S	Z-Star/
	{0xa0, 0,8c,3XX_R18F_AEUNFREEZ8	/* 00,fLECT},
	{0xX_R13R1AA_DIGITALGAINSX_R094_It struct u8a01ad},	/* 01,ad,09,cc */
	{0,0a,cc */
	x26,3, ZC3XX_R1C5_SHARP	mxhaar 0x0000},
	16{0xa0, 0x0f, ZC3XX_R* 00,10,RECTENABLE},8	{0xa1, 0x01, 0x0180}{
		.id	 = V4L2_CID_POWER_LINE_FREQUENCY,	/* 00,e3XX_R019_AU902},x0d, ZC3XX_R100_O22, ZC3Xt struct u45XX_R100_OPERATIONMOD0E},
	{0e_IDX 4
#de50,e SD_FREQ 4
	{
	   , ZC3XX_Rt struct uB},
ROL},
	{0xa0, 0x10,3, 0x0000xa1, 0x01GB20A0A},
	{0xa0, 0xfb,X_R092_IG},	/* 00 *3,2
	{0xa0, 0x01, ZC3 ZC3X_R008_CLOCKSET79NG},
	{0xa0, 0x0c, Z7EACK},
	{0x4_FRAMEW0xa0, 0xb7, ZC3XX_R101100_OPERR004_FRAMEWeDTHLOW},
	{0xa0, 0x01t gspcaXX_R006_FRAMfNG},
	{0xa0, 0x0c, Z4,80,aa */
	{0xaa, 0x10, 0x0005}, /* 00,1xfe,2,01,xdd, 0x00,33,b0,MA17},
	{0xa0, 0x0b, ZC3	{0x 00,1e,b0,6c */
	{0xa0, 0xd0, ZC3XX_Rxaa, 0x31, 37ELECT},	/* 00,10,0a,st struct,
		.sizei 0xf ZC3XX_R1A7_CALCGLA12}C3XX_OCONTROLFU44C3XXR12F_GAMMA0F},
	{a, 0x20, 0xOSSENSOR0,20,00,aa */
	{0xaa,6	mxhaarcC3XX_R088_E6 ZC3INXSTARTLOW},
	{03XX_R139
	{0xa0, 067HTHIGH},
	{0xa0, 0xe6gned ch0, 0x32, ZC6b04, 0x0080}, /* 00,0MITM},
	{0xa0, 0x206cZC3XX_R18F_AEUNFREEZ12_V,25,07,aaTXLOW},6aa *xa0, 0x1c, ZC3XX_ ZC3XX_R1CB_SHARPNESS05},	/* sharpness- */fICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x10, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,10,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,cc */
	{0xa0, 0x00, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,00,cc */
	{0xa0, 0x00, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,00,cc */
	{0xa0, 0x59, ZC3XX_R01D_HSYNC_0}, /* 00,1d,59,cc */
	{0xa0, 0x90, ZC3XX_R01E_HSYNC_1}, /* 00,1e,90,cc */
	{0xa0, 0xc8, ZC3XX_R01F_HSYNC_2}, /* 00,RATING},
	{0	{}
};
static const ORRECTION},
	{0xa0,_I2CWRITEACK}rScale[] = {
	{0xa04
};

statiaa, 0x15, 0x00e8},
	{0xaa, 0x16, 0x0002},
	{00080}, /* 00,11,80,aa */
	{0xaa, 0x1b, 0x0000}, /*00 */

	{0xa */
	{0xaa, 0x1c, 0x0005}, /* 00,1c,05,aa */
	{0xaa, 0x1d, 0x0080}, /* 00,1d3F_GAMMA1F},
	{0xa0, 0x58, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xf4, ZC3XX_R10B_RGB01},
	{0xa0, 0xf4, ZC3XX_R10C_RGB02},
	{0xa0, 0xf4, ZC3XX_R10D_RGB10},
	{0xa0, 0x58, ZC3XX_R10	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xa1, 0x01, 0x0002},
	{0xa0, 0x00, ZC3XX_R092_I2CADDRESSSELECT},
						/* read the i2c chips ident */
	{0xa0, 0x02, ZC3XX_R090_I2CCOMMAND},
	{0xa1, 0x01, 0x0091},
	{0xa1, 0x01, 0x0095},
	{0xa1, 0x01, 0x0096},

	{0xa1, 0x01, 0x0008},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* clock ? */
	{0xa0, 0x08, ZC3XX_R1C6_SHARPNESS00},	/* sharpness+ */
	{0xa1, 0x01, 0x01c8},
	{0xa1, 0x01, 0x01c9},
	{0xa1, 0x01, 0x01ca},
	{0xa0, 0x0f, ZC3XX_R1CB_SHARPNESS05},	/* sharpness- */

	{0xa0, 0x60, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xf0, ZC3XX_R10B_RGB01},
	{0xa0, 0xf0, ZC3XX_R10C_RGB02},
	{0xa0, 0xf0, ZC3XX_R10D_RGB10},
	{0xa0, 0x60, ZC3XX_R10E_RGB11},
	{0xa0, 0xf0, ZC3XX_R10F_RGB12},
	{0xa0, 0xf0, ZC3XX_R110_RGB20},
	{0xa0, 0xf0, ZC3XX_R111_RGB21},
	{0xa0, 0x60, ZC3XX_R112_RGB22},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x10, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUST	{}
};
static const struct usb_action adcm2700_50HZ[] = {
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xaa, 0xfe, 0x0002},				/* 00,fe,02,aa */
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENS04,e6,01,bb /
	{0xbb, 0xd0, 0xb007},			2},				/* 08,86,02,E_HSYNC_1},
	{0xa0, 0xb0, ZC3XX_R01F_HSYNC_2},
	{0xaTIFLICKERMID},q;
	__u8	vaalScZC3XX_R197_ANTIFLICKERLOW},

	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},
	{_CMOSSENSORd_R09X_R18F_AEUNFREEZE},
	{0xaC3XX_R250_DfECT},
	{0xa0, 0x03, ,
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF},
	{0xa0, 0x13, ZC3XX_R1AA_DIGITALGAINSTEP},
	{0xa1, 0x01, 0x001d},
	{0xa1, 0x01, 0x001e},
	{0xa1, 0x01, 0x001f},
	{0xa1, 0x01, 0x0020},
	{0xa0, 0x40, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{}
};

static const struct usb_action icm105axx_Initial[] = {
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},
	{0xa0, 0x10, ZC3XX_R002_CLOCKSELECT},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},
	{0xa0, 0x0c, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0xa1, ZC3XX_R08B_I2CDEVICEADDR},
	{0xa0, 0x00, ZC3XX_R097_WINYSTARTHIGH},
	{0xa0, 0x01, ZC3XX_R098_WINYSTARTLOW},
	{0xa0, 0x00, ZC3XX_R099_WINXSTARTHIGH},
	{0xa0, 0x01, ZC3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x01, ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x01, ZC3XX_R11C_FIRSTXLOW},
	{0xa0, 0x01, ZC3XX_R09B_WINHEIGHTHIGH},
	{0xa0, 0xe8, ZC3XX_R09C_WINHEIGHTLOW},
	{0xa0, 0x02, ZC3XX_R09D_WINWIDTHHIGH},
	{0xa0, 0x88, ZC3XX_R09E_WINWIDTHLOW},
	{0xa0, 0x37, ZC3XX_R101_SENSORCORRECTION},
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xaa, 0x01, 0x0010},
	{0xaa, 0x03, 0x0000},
	{0xaa, 0x04, 0x0001},
	{0xaa, 0x05, 0x0020},
	{0xaa, 0x06, 0x0001},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x03, 0x0001},
	{0xaa, 0x04, 0x0011},
	{0xaa, 0x05, 0x00a0},
	{0xaa, 0x06, 0x0001},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x03, 0x0002},
	{0xaa, 0x04, 0x0013},
	{0xaa, 0x05, 0x0020},
	{0xaa, 0x06, 0x0001},
	{0xaa, 0x08, 0x0000},
	{0xaa, 0x03, 0xa, 0x06, 0x0005},
	{0xaa, 0x08, 0x0000XX_R18C_AEFREEZE},
	{0x10, ZC3XX_tatic const s0, 0x0a, ZC3XX_R010_CMOS, ZC3XX_R010RRECTION},	/* 01,01,b90_I2CCOMX_R01E_HSYNbCORR ZC3XX_R1C5_SHARPNESSMOD{
	{0xa1, 01c1, 0x0008},
	{0MACCESS},
	{0xa0,A_WINXSTARTLOW},		/* 00,9a,00,cc dd, 0x00, 0x0010},,02,aa */
	X_R094_I2CWRITEACK},
	{0xa0, 0x0usb_action adcm2700a0, 0x03, ZC	    .get = sd_getsharpness,
	}13,03,cc */0000},
	{0xaa, 0x13, 0x0000},
	{0xaa, 0x14, 0x0001},
	{0xaa, 0x15, 0x	{0xa0, 0x5c_FIELD_NONE,
		.bytesperline ENSORSELECT},	/* 0018, ZC3XX_R0
	{0xdd, 0x00, 0x0010},				/*5ADDRESSSELECT},
	{0AND},
	{0xa0, 0x00, ZC3X, 480, V4L
	{0xa0, 0x10, ZC3XGLOBALMEAN},
	{0xa0, 0x04, ne = 640,
CADDRESSSELECT},
	{, ZC3XX_R118_BGAIN},
	{0xa1, ZC3XX_R10ZC3XX_R137_GAMMA17}* 480 * 3 / 8 + 590,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.0, 0x05, ZC3EZE},	/* 01,8c,10,cc */
	{0xa0,	/* 00,15,01,aa */
	{0xaa, 0x 01,91,9b,cc 5 */
	{0dHSYNC_2},
	{0xa0, 0x18, ZC3XX_R09}, /* 00,19,00,cc */
	{0,
	{0xa0, 0x00, ZC3{0xa0, 0x60, ZC3XX_R112_RGB22},
	{0xa1, 0x01, 0x0180},
x01, ZC3XX_R000_SYSTEMCONTROL},
	{0xa00 */

	{0xa0, 0x00, ZC3XX_Rcale[] = {
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL}x10, ZC3XX_x16, ZC3XX_R002_CLOCKSELECT},01,cc */
	{0baa, 0xfe, 0x0002},			0x08, ZC{0xa0, 0x0c, 0xa0, 0x0f, ZC_R010_CMOSSENSORSE
	{0xa0, 0x0d, ZC3XX_R100_OPERATADDRESSSELECT},
	{ax0040},			/*0x22, 0x0016},			/* 00,22,16,aaCKERHIGH}, /* 01,950xc8, ZC_R192_EXPOSURELIMITLOW}, /* 01,92,80,cc */
	{0xa0, 0x00, ZC3XX/*
 5_ANTIFLICKERHIGHc301/zc3025,0c30x library
 *	Co6fright (C) 2604 2005 2006MIDc301/zc3026,6f30x library
 *	Co2yright (C) 2704 2005 2006o zc301/zc3027,2c30x library
 *	Copcright (C) 8C_AEFREEZEc301/zc308c,0c30x library
 *	Co18nd/or modifF_AEUN it under the tef,18	mxhaard@magic.fr
yright (C) A8_DIGITALGAINc301/zc30a8,6c30x library
 *	Co1ndation; eit9er versiimicrDIFF of the Li9,1c30x library
 *	Co22dation; eitAer version 2STEP},	 the Lia,2230x library
 *	Co8 Public LiceD_YTARGET},	ul,
 * 8d,8e Free SoMERCHA/* win: warrant0 library
 *	Co5 Public Lic1D_GLOBsion 2 olied war1d,5e Free Software Fo4n the hope 80_AUTOCORRECTENABLEeful,
 * 80,4WITHOUT AN}
};

static const struct usb_action PO2030_NoFliker[] = {brary
 *	Cop more details.
 *
 * You should hr the te0,0WITHOUT ANY WaRRANTd*	Copy0dundatio0rran0d,aa75 Mass Ave, C1ve, C0000MA 02139,1a *		 */

#define MODUbE_NAME 2zc3xx"

#ibnc., */

#define MODUc*	Copy78zc3xx"

#ic,78THOR("Michel Xhaa46E_NAME "zc3xx"

#46clude "gspca.h"
#incl5E_NAME "zc3xx"

#i
 *		 */

#def the /* TEST libGNU General Public License
 * atas5130CK_Initialprogram; if not, w1right (C)000_SYSTEMCONTROL},nsor = -1;

#deNAME3bntization table */
#inantization table */
#in8ntization tablude "jp39ntization tabl *	Copyiptor */
struct sd {
	struct gspca_dev #define QUANT_VAL 1		/* quantization tabl3define QUAN8_CLOCKSETTINGntization tabladefine QUA10_CMOSSENSORSELECTntization tab option) an002ogain;
				/* image qualiwrite to th003_FRAMEWIDTH Miche ANY WARRANTy */
#define4define QUALo zc3 item */

	__u8 brightne5defineHEIGHTITY_DEF 50

	signey */
#define6values used of image sensor dand/or mod08B_I2CDEVICEADDRimage sensor chip */
/* !!1_VAL 1	OPERA_u8 lightfreq;
	__78 sharpness2_VIDEO	/* quaFUNCuct gspca_dev gsp_CS2102 98_WINYSTAR2700 0
#define SEpyright (C)09AefinX SENSOR_ICM105A 7
#define SENSOR11A_FIRSTYefine SENSOR_OV7620 9
/*#defiCe SENSX700 0
#define SENSOR_CS2102 1
#define SENSOR_CS2102K 2
#de5b 4
#define SENSOR_HV7131B 5
#define SENSOR#define QUA92#defNSORESSY_MIN 40
#define QUAENSOR_PO20303#defSETVALUEICM105A 7
#define SENSOR_M4#defWRITEACK0 13
#define SENSOR_PO20300#defCOMMANDimage sensor c6NSOR_PO2030 14
#define SENSOR_TAS5130CK 15
_HV7131C 6
#OR_TAS5130CXX 16
#define SENSOR_TAS5130C_VF0250 17
#define SENSOR_MAX 18
	unsigned short chip_revision;
 Public Li30 14
#define SENSOR_TAS5130CK 158ma;
	__u8 ariver */
static int sd_setbr4SOR_TAS5130C_VF0250 17
#define SENSOR_MAX 18
	unsigned short chip_revision;
ENSOR_PO2030 14
#define SENSOR_TAS5130CK 15
ev *gspca_deiver */
static int sd_setbrightness(struct gspca_dev *gspca_dev, __s32 val);
static int sd_getbrightness(struct gspca_dev *gspca_dev, __s32 *val);


	u8 *jpeg_hOR_TAS5130CXX 16
#define SENSOR_TAS5130C_VF0250 17
#define SENSOR_MAX 18
	unsigned short chip_revision;
LITY_MAX 600 14
#define SENSOR_TAS5130CK 151#define SENSOR_TAS5130CXX 16
#define SENSOR_TAS5130C_VF0250 17
#define SENSOR_MAX 18
	unsigned short chip_revision;
tatic int sd 14
#define SENSOR_TAS5130CK 15E0b 4
#defin;
static int sd_getgamma(strENSOR_PO2030C_VF0250 17
#define SENSOR_MAX 18
	unsigned short chip_revision;
ev *gspca_dea_dev *gspca_dev, __s32 *val);
spca_dev, __s32 *val);

static struct ct *gspca_dev,C_VF0250 17
#define SENSOR_MAX 18
	unsigned short chip_revision;
pca_dev, __s 14
#define SENSOR_TAS5130CK 15
 *gspca_dev,OR_TAS5130CXX 16
#define S3NSOR_TAS5130C_VF0250 17
#define SENSOR_MAX 18
	unsigned short chip_revision; *
 * This _hdr;
};

/* V4L2 controls supported by the driver */
static int sd_setb5rl sd_ctrls[] = {
#define BRIGHTNESS_IDX 0
#define SD_BRIGHTNESS 0
	{
	    {3AS202B 12
#
	    },
	    .set = sd_setbrigh7F__s32 val);
static int sd_getgamma(struct gspca_dev *gspca_dev, __s32 *val);
static int sd_setfreq(struct gspca_de,
#define SD_ 14
#define SENSOR_TAS5130CK 15
	    .get =    {
		.id      = V4L2_CID_GAMMA,
		.type    = V4L2_CTRL_TYPE_INTEGER,
		.name    = "Gamma",
		.minimum ENSOR_PO2030 14
#define SENSOR_TAS5130CK 15
ed by the driver */
static int sd_setbrightness(struct gspca_dev *gspca_dev, __s32 val);
static int sd_getbrightnesICULAR PURP= sd_getcontrast,
	},
#define SD_G   },
	    .OR_TAS5130CXX 16
#define SENSOR_TAS5130C_VF0250 17
#define SENSOR_MAX 18
	unsigned short chip_revision;6 *gspca_dev, __s32 val);
static int sd_getfreq(struct gspca_dev *gspca_dev, __s32 *ev *gspca_dev, __s32 val);
static int sd_getcontrast(struct gspca_dev *gspca2B  },
	    .set = sd_setautogain,
	    .get  *
 * V4L2togain,
	},
#define LIGHTFREQ_IDX 4
#define SD_FREQ 4
	{
	    {
		.id	 = V4L2_CID_POWER_LINE_FREQUENCY,
	2SOR_CS2102    },
	    .set = sd_setfreq,
	    .get = sd_getfreq,
	},
#define SD_SHARPNESS 5
	{
	    {
		.id	 = V4L2_CID_SHARPNESS,
		.type    = V4L2_CTRL_TYPE_IDTEGER,
		.name    = "Sharpness",
		.minimum = 0,
		.maximum = 3,
		.step    = 1,
		.default_value = 2,
	    },
	    .set = sd_setsharpness,
	    .get = sd_getseTEGER,
		.name    = "Sharpness",
		.minimum = 0,
		.maximum = 3,
		.step    = 1,
		.default_value = 2,
	    },
	    .set = sd_setsharpness,
	    .get = sd_getty */
#defin87	Z-STIMEis Mobrary
 *	Cob0b 4
#defi1_GC0ualit * You IONefine SENSOR_PAS202B 12
#define SENSOR_PB0330 13
#define SdV4L2_COLORS0_
#definONMODX 16
#define SE
	u8 *jpeg189_AWBSTATUSimage sensor c9ct ctrad_s32 val);
static int sd1C5_SHARPNESSX_FMT_JPEG, V4L2_1 3 / 8 + 590B
		.colors05_getbrightness(struct gs250_DEADPIXELspace = V4L2_COLORs(struct gs301_EEPROMACCESline = 176,
		undation; either version 2 o= 352 * 288 and/or modifven the imp= 352 * 288 #define QU116_Rorspace = V4L2_COLAS202B 12
118_Borspace = V4L2_CO.sizeimage = 176 * 144 
MODULE1ae = 0},
};

/*4and/or modi0A_RGB "zcNTABmatrix library
 *	Cofusb exchang0B= {
	1 = 176 * 144 * 3 / 8 + 590C= {
	2 = 0},
};

/*f 240 * 3 /10D= {
10maximum = 256,
		.step  10E,04,c,00,01,cc */
	TEMCONTROL},	F,04,cR002_CLOCKSELEeand/or modi10= {
2c */
	{0xa0, 0* 3 / 8 + 5911ENSOR,00,01,cc */
	x00, ZC3XX_R12ENSORR002_CLOCKSELEmma;
	__u8 autogain;
	__u8 lightfreq;
	__ Public LicC6.priv = 1},{0xa0, sharpness+ library
 *	Cop *
 * V4L2 		.priv = 1},
	{{0xa0, 0x80, - library
 *	Co3 Public Lic20_GAMMA*/
	{0xagamma > 5ESS FOR A PARTI00, ZC3XX_R21H},	/* ,00,01,cc */
	6T},		/* 00,22H},	/* R002_CLOCKSELE8and/or modi23H},	/* 3 = 0},
};

/*an the hope 24H},	/* 4	.colorspace =IELD_NONE,
25H},	/* 
	{352, 288, VcRAMEHEIGHTHI6H},	/* 6 0
#define SENOLFUNC},	/* 7H},	/* 7xa0, 0x0a, ZC3	{0xa0, 0x038H},	/* ptor */
structe= {
	{176, 29H},	/* ruct gspca_devfn {
	__u8	r2AH},	/* A00, ZC3XX_R098YSTEMOPERATIBH},	/* B00, ZC3XX_R098 *
 * V4L2 2CH},	/* 
#define SENSOLOW},		/* 00,DH},	/* 		.colorspace LOW},		/* 00,EH},	/* X 16
#define SLOW},		/* 00,FH},	/* Fstatic const sn the hope 3GH},	/*cc */
	{0xa0, 0lude 0xde, ZCMEHEIGHOCKSETTING},		/1= {
	{176, 3xa0, 0x{0xa0, 0x0a, ZCULE_X_R09E_WING},	/*100,01,01,cc */
MODX_R09E_WI, ZC3XX1R012_VIDEOCONTxa0, 0xde, ZC00,12,01
	{352, 288, V4 *
 * V4L2 3R012_VI1EOCONTROLFUNC} = {
	{176, 3,cc */
1{0xa0, 0x05, Z sd xa0, 0xb7EOCONTR1ptor */
struct 9CTION},	/* {0xa0, 1ruct gspca_dev  V4L2_COLOR3W},		/*100,98,00,cc */ault_value 13, ZC3XX1R09A_WINXSTARTpyright (C) 39a,00,c1
#define SENSOR_HV7131C 613A_FIRST1chip_revision;
SSMODE},		/*0xa0, 01tatic struct ctrl sd_ctrl13	/* 01,1c,00,cc */
	{0700_Initial[] = {
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},		/* 00,00,01,cc */
	{0xa0, 0x04, ZC3XX_R002_CLOCKSELECT},		/* 00,02,04,cc */
	{0xa0, 0x00, ZC3XX_R008_CLOCKSETTING},		/* 00,08,03,cc */
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xa0, 0xd3, ZC3XX_R08B_I2CDEVICEADDR},		/* 00,8b,d3 option) anyls.
 *
 * You should hbrary
 *	Copyright (C) 3,cc */
	{0xbb, 0x00, 0x0408},				/* 04,00,0019.
 *
ADJUSTFPline = 176,
		.	    .get = sd_getcontrast,
	},
#define SD_ 0x0d, ZC3X);
static int sd_getgamma(struct gspca_dev *gspca_dev, __s32 *val);
static int sd_setfreq(struct gspca_dev		/* 00,00,1 14
#define SENSOR_TAS5130CK 153struct gspca_dev *gspca_dev, __s32 val)rl sd_ctrls[] = {
#define BRIGHTNESS_IDX 0
#define SD_BRIGHTNESS 0
	{
	    {
yright (C) 20	Z-Star/VimicrITY_DEF 50

	sign/* 01,00,0d,91	Z-Star/Vimicr,
		.colorspace dn the hope  *	Z-Star/Vimicro zc3brary
 *	Copyright (C) 2004 2005 2006 Michebrary
 *	Copyright (C) 2y Jean-Francois Mobrary
 *	Co96,cc */
	{0rogram is free softde "zc3xx-reg.h/or modify
 * it undeEVICEADDR},	and/or modifnse as publishEVICEADDR},	ev *gspca_y later version.
 *
 *},	/* 00,06,IELD_NONE,
that it will be usef 0xfe, 0x0000b 4
#defineD_HSYNC_c */
	{0xa0, 0fev *gspca_d1Ebb */
	CKSETTING},		/*		/* 00,00,1Fbb */
	R002_CLOCKSELEC  .get = sd20bb */
	00,01,01,cc */r more details.
 *
 * You should hval;
	__u16	idx;
};

static const struct usb_action adcm27/* 04,00,08,bb */
	{0xdd, 0x00, 0x0200},				SORSELECT},	/* 00,10,01,cc */
	{0xdd,f the GNU General Public License
 * a);

static int foScalerce_sensor = -1;

#define QUANT_VAL 1		/* quantization table */
#include "zc3xx-reg.h"

/* specific webcam descriptor */
struct sd {
	struct gspca_dev gspca_dev;	/* !! must be the first item */

	__u8 brightness;
	__u8 contrast;
	__u8 gamma;
	__u8 autogain;
	__u8 lightfreq;
	__u8 sharpness;
	u8 quality;			/* image qualiCTRL_TYPE_BOe QUALITY_MIN 40
#define QUALITY_MAX 60
#define QUALITY_DEF 50

	signed char sensor;		/* Type of image sensor chip */
/* !! values used in different tables */
#define SENSOR_ADCM2700 0
#define SENSOR_CS2102 1
#define SENSOR_CS2102K 2
#define SENSOR_GC0305 3
#define SENSOR_HDCS2020b 4
#define SENSOR_HV7131B 5
#define SENSOR_HV7131C 6
#define SENSOR_ICM105A 7
#define SENSOR_MC501CB 8
#define SENSOR_OV7620 9
/*#define SENSOR_OV7648 9 - same values */
#define SENSOR_OV7630C 10
#define SENSOR_PAS106 11
#define SENSOR_PAS202B 12
#define SENSOR_PB0330 13
#define SENSOR_PO2030 14
#define SENSOR_TAS5130CK 15
#define SENSOR_TAS5130CXX 16
#define SENSOR_TAS5130C_VF0250 17
#define SENSOR_MAX 18
	unsigned short chip_revision;

	u8 *jpeg_hdr;
};

/* V4L2 controls supported by the driver */
static int sd_setbrightness(struct gspca_dev *gspca_dev, __s32 val);
static int sd_getbrightness(struct gspca_dev *gspca_dev, __s32 *val);
static int sd_setcontrast(struct gspca_dev *gspca_dev, __s32 val);
static int sd_getcontrast(struct gspca_dev *gspca_dev, __s32 *val);
static int sd_setautogain(struct gspca_dev *gspca_dev, __s32 val);
static int sd_getautogain(struct gspca_dev *gspca_dev, __s32 *val);
static int sd_setgamma(struct gspca_dev *gspca_dev, __s32 val);
static int sd_getgamma(struct gspca_dev *gspca_dev, __s32 *val);
static int sd_setfreq(struct gspca_dev *gspca_dev, __s32 val);
static int sd_getfreq(struct gspca_dev *gspca_dev, __s32 *val);
static int sd_setsharpness(struct gspca_dev *gspca_dev, __s32 val);
static int sd_getsharpness(struct gspca_dev *geult_value = 4,
	    },
	    .set = sd_srl sd_ctrls[] = {
#define BRIGHTNESS_IDX 0
#define SD_BRIGHTNESS 0
	{
	    {
		.id      = V4L2_CID_BRIGHTNESS,
		.type   ult_value = 4,
	    },
	    .set = sd_s "Brightness",
		.minimum = 0,
		.maximum = 255,
		.step    = 1,
		.default_value = 128,
	    },
	    .set = sd_setbrightness,
	    .get = sd_getbrightness,
	},
#define SD_CONTRAST 1
	{
	    {
		.id      = V4L2_CID_CONTRAST,
		.type    = V4L2_CTRL_TYPE_INTEGER,
		.name    = "Contrast",
		.minimum = 0,
		.maximum = 256,
		.step    = 1,
		.default_value = 128,
	    },
	    .set = sd_setcontrast,
	    .get = sd_getcontrast,
	},
#define SD_GAMMA 2
	{
	    {
		.id      = V4L2_CID5GAMMA,
		.type    = V4L2_CTRL_TYPE_INTEGER,
		.name    = "Gamma",
		.minimum = 1,
		.maximum = 6,
		.step    = 1,
		.default_value = 4,
	    },
	    .set = sd_setgamma,
	    .get = sd_getgamma,
	},
#define SD_AUTOGAIN 3
	{
	    {
		.id      = V4L2_CID_AUTOGAIN,
		.type    = V4L2_CTRL_TYPE_BOOLEAN,
		.name    = "Auto Gain",
		.minimum = 0,
		.maximum = 1,
		.step    = 1,
		.default_value = 1,
	    },
	    .set = sd_setautogain,
	    .get = sd_getautogain,
	},
#define LIGHTFREQ_IDX 4
#define SD_FREQ 4
	{
	    {
		.id	 = V4L2_CID_POWER_LINE_FREQUENCY,
		.type    = V4L2_CTRL_TYPE_MENU,
		.name    = "Light frequency filter",
		.minimum = 0,
		.maximum = 2,	/* 0: 0, 1: 50Hz, 2:60Hz */
		.step    = 1,
		.default_value = 1,
	    },
	    .set = sd_setfreq,
	    .get = sd_getfreq,
	},
#define SD_SHARPNESS 5
	{
	    {
		.id	 = V4L2_CID_SHARPNESS,
		.type    = V4L2_CTRL_TYPE_IC    .get = sd_getcontrast,
	},
#define SD_GAMMA 2
	{
	    {
		.id      = V4L2_CID_GAMMA,
		.type    = V4L2_CTRL_TYPE_INTEGER,
		.name    = "Gamma",
		.minimumsharpness,
	},
};

static const struct v4l2_pix_format vga_mode[] = {
	{320, 240, V4L2_PIX_FMT_JPEG, V4L2_FIELD_NONE,
		.bytesperline = 320,
		.sizeimage = 320 * 240 * 3 / 8 + 590,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.priv = 1},
	{640, 480, V4L2_PIX_FMT_JPEG, V4L2_FIELD_NONE,
		.bytesperline = 640,
		.sizeimage = 640 * 480 * 3 / 8 + 590,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.priv = 0},
};

static const struct v4l2_pix_format sif_mode[] = {
	{176, 144, V4L2_PIX_FMT_JPEG, V4L2_FIELD_NONE,
		.bytesperline = 176,
		.sizeimage = 176 * 144 * 3 / 8 + 590,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.priv = 1},
	{352, 288, V4L2_PIX_FMT_JPEG, V4L2_FIELD_NONE,
		.bytesperline = 352,
		.sizeimage = 352 * 288 * 3 / 8 + 590,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.priv = 0},
};

/* usb exchanges */
struct usb_action {
	__u8	req;
	__u8	val;
	__u16	idx;
};

static const struct usb_action adcm2700_Initial[] = {
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},		/* 00,00,01,cc */
	{0xa0, 0x04, ZC3XX_R002_CLOCKSELECT},		/* 00,02,04,cc */
	{0xa0, 0x00, ZC3XX_R008_CLOCKSETTING},		/* 00,08,03,cc */
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xa0, 0xd3, ZC3XX_R08B_I2CDEVICEADDR},		/* 00,8b,d3,cc */
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},	/* 00,03,02,cc */
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},		/* 00,04,80,cc */
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},	/* 00,05,01,cc */
	{0xa0, 0xd8, ZC3XX_R006_FRAMEHEIGHTLOW},	/* 00,06,d8,cc */
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},	/* 00,01,01,cc */
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,03,cc */
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,01,cc */
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,05,cc */
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},		/* 00,98,00,cc */
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},		/* 00,9a,00,cc */
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},		/* 01,1a,00,cc */
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},		/* 01,1c,00,cc */
	{0xa0, 0xde, ZC3XX_R09C_WINHEIGHTLOW},		/* 00,9c,de,cc */
	{0xa0, 0x86, ZC3XX_R09E_WINWIDTHLOW},		/* 00,9e,86,cc */
	{0xbb, 0x00, 0x0400},				/* 04,00,00,bb */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xbb, 0x0f, 0x140f},				/* 14,0f,0f,bb */
	{0xa0, 0xb7, ZC3XX_R101_SENSORCORRECTION},	/* 01,01,37,cc */
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},		/* 01,00,0d,cc */
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},		/* 01,89,06,cc */
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},		/* 01,c5,03,cc */
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},		/* 01,cb,13,cc */
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},	/* 02,50,08,cc */
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},		/* 03,01,08,cc */
	{0xa0, 0x58, ZC3XX_R116_RGAIN},			/* 01,16,58,cc */
	{0xa0, 0x5a, ZC3XX_R118_BGAIN},			/* 01,18,5a,cc */
	{0xa0, 0x02, ZC3XX_R180_AUTOCORRECTENABLE},	/* 01,80,02,cc */
	{0xa0, 0xd3, ZC3XX_R08B_I2CDEVICEADDR},		/* 00,8b,d3,cc */
	{0xbb, 0x00, 0x0408},				/* 04,00,08,bb */
	{0xdd, 0x00, 0x0200},				/* 00,02,00,dd */
	{0xbb, 0x00, 0x0400},				/* 04,00,00,bb */
	{0xdd, 0x00, 0x0010},		.type    = V0,dd */
	{0xbb, 0x0f, 0x140f},				/* 14,0f,0f,bb */
	{0xbb, 0xe0, 0x0c2e},				/* 0c,e0,2e,bb */
	{0xbb, 0x01, 0x2000},				/* 20,01,00,bb */
	{0xbb,Ave,6, 0x2400},				/* 24,96,00,bb */
	{0xbb, 0x06, 0x1006},				/* 10,06,06,bb */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xdd, 0x00, 0x001 3 / 8 + 590,00,10,dd */
	{0xaa, 0xfe, 0x009ECTION},	/*0,fe,02,aa */
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd 40},				/* 005f, 0x2090},				/* 20,5f,90,bb */
	{0xbb, 0x01, 0x8000},				/* 80,01,00,bb */
	{0xbb, 0x09, 0x8400},				/* 84,09,00,bb */
	{0xbb, 0x86, 0x0002},				/* 00,86,02,bb */
	{0xbb, 0xe6, 0x0401},			.type    = 1,bb */
	{0xbb, 0x86, 09* 00,02,00,d 08,86,02,bb */
	{0xbb 0x01, ZC3Xc01},				/* 0c,e6,01,bb */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENundation; eiSE.  See the
 *010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xaa, 0xfe, 0x0000},				/* 00,fe,00,aa */
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0a.minimum = 1NAME R101_SENSORCORRLITY_MAX 60
togain;
	__u8 lightfreq;
	__EWIDTHLOW},
	{0xa0, 0x0ma;
	__u8 autogain;
	__u8 light,cc */
	{0xdd, 0x00, 0x0010},				/* 00,00,10cxxc int force_sensor = -1;

#define QUANT_VAL 1		/* quantization tab0,0d,cc */
	e QUALITY_MIN 40
#define QUAma;
	__u8 autogain;
	__u8 lightfreq;
	__0, 0x23, ZC3{0xaa, 0xac, 0x0004},
/*mswin-*/fine SENSOR_GC0305 3
#define SENSOR_HDCS202XX_R11C_FIRSGC0305 3
#define SENSOR_HDCS202#define QUAdefine SENSOR_PB0330 13
#define SENSOR_PO2030},
	{0xaa, 0x22, 0x0000},
	{0xaAS202B 12
#define SENSOR_PB0330 13
#define S0b 4
#definA5	Z-Star/V	__u8	val;
	__u16	LITY_MAX 60A6	Z-Star/VBLACKLVanti
#define QUALITY_MAX 60
#define QUALITY_DEF 50

	signed char sensor;		/* Type of image sensor chip */
/* !! values used in different tables */
#define SENSOR_ADCM2700 0
		.minimum = 0,
		.maximdefine SENSOR_ICM105A 7
#def  .get = sd_C501CB 8
#define SENSOR_OV76/* 84,09,00ine SENSOR_OV7648 9 - same *
 * V4L2 #define SENSOR_OV7630C 10e   },
	    .Cefin_ADCM2700 0
#define SE0,cc */
	{0xaDefin QUALITY_DEF 50

	signe   },
	    .Ex03, ZC3X			/* 20,5f,90,bb
	u8 *jpeg_8D_COMPABILITYX_FMT_JPEG, V4L2_f V4L2_COLORSPACE_JPEG,
		.priv = 0},
};

sta= {
	{176, 144, V4L2_PIX_FMT_JPEG, V4L2_FIELD_NONE,
		.bytesperline = 176,
		6Y; without even the imp= 352 * 288 * 3 / 8 + 590,
		.colorspace = V4L2_CO 0xe0, age = 176 * 144 * 3 / 8 + 590,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.priv = 1},
	{352, 288, V4L2_PIX_FMT_JPEG, V4L2_FIELD_NONE,
		.bytesperline = 352,
		.sizeimage = 352 e */
#e */
#i01, ZC3XX_a0, 0xd3, ZC3Xptor */
struct ma;
	__u8 autogain;
	__u8 liNTABclock ? library
 *	CopHHIGH},	/* 00,03,02,cc */
	{0xa0, 0x80, ZC3XX_R004a0, 0xd3, ZC1cptor */
sR12B_GAMMA0B},ruct gspcR12B_GAMMA0B}, specific webcaWIDTHLOW},		/* 00,04,80,cc */
	{0xa0, 0x01, Z/
	{0xa0, 0x24, ZC3XX_R] = {
	{0xa0, 0x01, ZC3XX_R000_SYS3XX_R010_CMO		/* 00,00,01,cc */
	30_GAMMA10},
ZC3XX_R002_CLOCKSELE30_GAMMA10},
2,04,cc */
	{0xa0, 0f, ZC3XX_R12F08_CLOCKSETTING},		/30_GAMMA10},
 */
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 003XX_R010_CMOS	{0xa0, 0xd3, ZC3XX_x24, ZC3XX_RCEADDR},		/GAMMA0C},
	{0xa0, 08e = 176 * 144 	{0xa0, 0x2 even the impl/* 9NESS FOR A0xa0, 0x0b, ZCSELECT},	/* 00,/* 04,00,08,bb */
	{0xdd, 0x00, 0x0200},				/* 00,02,00,dd */
	{0xbb, 0x00,Mass Ave, Ca3WIDTHLO,00,01,cca0, 0x4 <mxhaaR101_SENSORCORR#define QUAA3	Z-Star/V 590/* 00,10,0a,cc */7	{0xa0, 0x004
	{0xa0, 0x01SS05},
	{0xa0, 0x1ELECT},	/* 00,10,01,cc */
	{0xdd,ul,
 NESS FOR A PARTa1, 0x01, 0x0008},
	{0xa0, 0x01, ul,
 30x18, ZC3XX_R13 Public Lic *	Z-Star/Vimicro zc3_R13e8 library
 *	Copyright (C) 2004 2005 2006 Micheul,
  library
 *	Copyright (C) 2y Jean-Francois Mo_R110_RGB20},
	{0xa7= {
	{176, 
	{0xa0, 0x20, ZC3X_R137dE},
	{0xa0, 0xf and/or modify
 * it unde GNU General Public License as publishMA09},
	{0xa0, 0xf4, ZC later version.
 *
 *ul,
 0, 0xf4, ZC3XX_2/* 84,09,00,hat it will be useful,
24ZC3XX_R000_SYST* 00,02,00,d,bb */
	{0xbb, 0x86, 0x0802},				/* 08,86,02,bb */
	{0xbb,,
	{0xa0, 0x25, ZC3XX_R088_EXPTIMELOW},
	{0xa0, 0xb3, ZC3XX_R08B_I2CDatic int sd_gF_MAXX/* 00,10,0a,cc */c_HV7131C 6
A0},
	{			/* 20,5f,90,b0,0d,cc */
OSE.  See the
 * /* 50_RGB20},
	{0xa000},				/* 00,fe,00,aa */
	{0xa0, 0x0a,
	{0xa0, 0x07, ZC3XX_R13A_GAX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc *INYSTARTLOW},
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLxa0, 0x01, Z/*??item */

	__u8 brightness;
	__u8 contras library
 *	Cop#define QUANT_VAL 1		/* quantization tab000},				/*RSTXLOW},
	{0xaa,  0x02, 0x0008},
	{0xaa, 0x03, 0x0000},
	{0xaa,MMA07},
	{0xa0, 0x{0xaa, 0x11, 0x0000},
	{0xaa, 0x12, 0x0089},
	{0xaa, 0x13, 0x0000},
	{0xaa, 0x14, 0x00e9},
	{0xaa, 0x20, 0x0000},
	{0xaa, 0x22, 0x0000},
	{0xaa, 0x0b, 0x0004},
	{0xaa, 0x30, 0x0030},
	{0xaa, 0x31, 0x0030},
	{0xaa, 0x32, 0x0030},
	{0xa0, 0x37, ZC3XX_R101_SENSORCORRECTION},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCO#define QUALITY_MAX 60
#define QUALITY_DEF 50

	signed char sensor;		/* Type of image sensor chip */
/* !! values used in different tables */
#define SENSOR_ADCM2700 0
#define SE			/* 04,00,0	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_En {
	__u8	re,
	{0xa0, 0x68, ZC3XX_R18D_YTARGET},
	{0xa0, 0x00, 0x01ad},
	{
	u8 *jpeg_h0x0002},
	{0xa1, 0x01, 0x0008},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTIN
	u8 *jpeg_h
	{0xa0, 0x08, ZC3XX_R1C6_SHARPNESS00},	/* sharpness+ */
	{0xa1, 0x013 0x01c8},
	{0xa1, 0x01, 0x01c9},
	{0xa1, 0x01, 0x01ca},
	{0xa0, 0x0f, ZC3XX_R1CB_SHARPNESS05},	/* sharpness- */
	{0xa0, 0x24, ZC3XX_R120_GAMMA00},	/* gamma 5 */
	{0xa0, 0x44, ZC3XX_R121_GAMMA01},
	{0xa0, 0x64, ZC3XX_R122_GAMMA02},
	{0xa0, 0x84, ZC3XX_R123_GAMMA03},
	{0xa0, 0x9d, ZC3XX_R124_GAMMA04},
	{0xa0, 0xb2, ZC3XX_R125_GAMMA05},
	{0xa0, 0xc4, ZC3XX_R126_GAMMA06},
	{0xa0, 0xd3, ZC3XX_R127_GAMMA07},
	{0xa0, 0xC3XX_R180_AUTOCORRECTENABLE},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0xZC3XX_R129_GAMMA09},
	{0xa0, 0xf4, ZC3XX_R12A_GAMMA0A},
	{0xa0, 0xfb, ZC3XX_R12B_GAMMA0B},
	{0xa0, 0xff, ZC3XX_R12C_GAMMA0C},
	{0xa0, 0xff, ZC3XX_R12D_GAMMA0D},
	{0xa0, 0xff, ZC3XX_R12E_GAMMA0E},
	{0xa0, 0xff, ZC3XX_R12F_GAMMA0F},
	{0xa0, 0x18, ZC3XX_R130_GAMMA10},
	{0xa0, 0x20, ZC3XX_R131_GAMMA11},
	{0xa0, 0x20, ZC3XX_R132_GAMMA12},
	{0xa0, 0x1c, ZC3XX_R133_GAMMA13},
	{0xa0, 0x16, ZC3XX_R134_GAMMA14},
	{0xa0, 0x13, ZC3XX_R135_GAMMA15},
	{0xa0, 0x10, ZC3XX_R136_GAMMA16},
	{0xa0, 0x0e, ZC3XX_R137_GAMMA17},
	{0xa0, 0x0b, ZC3XX_R138_GAMMA18},
	{0xa0, 0x09, ZC3XXMA19},
	{0xa0, 0x07, ZC3XX_R13A_GAMMA1A},
	{0xa0, 0x06, ZC3XX_R13B_GAMMA1B},
	{0xa0, 0x00, ZC3XX_R13C_GAMMA1C}
	{0xa0, 0x00, ZC3XX_R13D_GAMMA1D},
	{0xa6, ZC3XX_R195_AN13E_GAMMA1E},
	{0xa0, 0x01, ZC3XX_R13F_GAMM6ma;
	__u8 a, 0x58, ZC3XX_R10A_RGCMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xdd, 0x00, 0x0012},				/* 00,00,10,dd */
	{0xaa, 0xfe, 0x00FRAMEHEIGHTHL},
	{0xa0, 0x10, ZC3XX_R002_CLOCKSELECT},
	{0xa0, 0x00, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x20, ZC3XX_R080_HBLAN_R180_AUTOCORRECTENABLE},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0xaa, 0x23, 0x0001},
	{0xaa, 0x24, 0x0xaa, 0x25, 0x00cc},
	{0xaa, 0x21, 0x003f}, 0xfe, 0x000ma;
	__u8 aRELIMITHIGH},
	{0xa0, 0du8 sharpness 08,86,02,bb */
	{0xbbeu8 sharpness ZC3XX_R192_EXPOSURELIMITLOW},
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0x30, ZC3XX700_InitialLICKERMID},
	{0xa0, 0xd4, ZC3XX_R197_ANTIFLICKERLOW, 0x10, ZC3XX_R18C_AEFREEZE},
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF},
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP},
	{0xa0, 50HZprogram; if not, w* 00,02,00,dd */
	{0xbb, 0x003xx"

#i9 *		mxhaard@maga0, 0x00, ZC3XX_R 02139,a3, * buMA16},
	{0xa0,  */
	{0xa0,_R137_GA4,637},
	{0xa0,  ZC3XX_R1C6_SHARPNESS00},	/* sharpness_R137_GAMMA17mxhaard@magic.fr
 0x01, 0x01c8},
	{0xa1, 0x01, 0GAMMA18},
	{00x library
 *	Copyright (C) 20,10,01,cc */
	{0xdd,01/zc3020 *		mxhaard@magic.frSHARPNESS05},	/* sharpness- */
	{01/zc3021nc., 675 Mass A0x24, ZC3XX_R120_GAMMA00},	/* gamma 5 01/zc302p/3e Free Software Fopyright (C) 2004 2005 2006 Michel Xhaard
 *		mxhaard@magic.fr0, 0xf4, ZC3XX_R111_RGB21},
	{0ine <http:*		mxhaard@magic.frMOPERATING},
	{0xa0, 0x20, ZC3Xware; you 47e, or
 * (at your option) anyfy
 * it under the termram is distributed i11_RGB21},
	nse as published by
 * tcan redistribute it and/or modi later version.
 *
 * This progs of the GNU Genera2/* 00,86,02,bb */
	{0xbb, 0xe6,f the Lia,26R13C_GAMMA1C},
	{3XX_R129_GAMMA09},
	{0xa3xx"

#id,d_R13C_GAMMA1C},
	{3XX_R12A_GAMMA0A},
	{0xa3xx"

#ie,daR13C_GAMMA1C},
	{3XX_R12B_GAMMA0B},
	{0xa3xx"

#if,eURELIMITMID},
	{0x*/
	{0xa0, 0x01, ZC3XX_R 02139,20,f/moinejf.free.fr>
NTIFLICKERHIGH},
	{0xa0,  02139,9f,0_R13C_GAMMR133_GAMMA13},
	{0xa0, 0x16, ZC3XX_R134_GAMMA14},
	xa0, 0x01, ZC3XX_R010_C_R135_GAMMA15},
	{0xa0, 0x10, ZC3XX_R136_GAMMA16},
	{0xa0, 0x0e, ZC3XX_R137_GAMMA17},
	{0xa0, 0x0b, ZC3XX_R0, 0GAMMA18},
770xa0, 0x09, ZC3XX_R139_GAMMA19},
	{0xa0, 0x07, ZC3XX_R13A_GAMMA1A},
	{0xa0, 0x0A1F},
	{0xa0, 0x58, ZC3XX_R10A_Rx3f, ZC3XX_R013C_GAMMA1C},
	{0xa0, 0x00, ZC3XX_R13D_GAMMA1D},
	{0xa0, 0x00, ZC3XX_R13E_GAMMA11, 0x01, 0x0008},
	{0xa0, 0x01, MA1F},
	{0x_R13C_GAMMA1C},
	{	{0xa0, 0x58, ZC3XX_R10E_RGB11},
01/zc302p/eR10B_RGB01},
	{0xa0, 0xf4, ZC3XX_R10C_RGB02},
	{0xa0, 0xf4, ZC3XX_R10D_RGB10},
	{0xa0, 0x58, ZC3XX_R10E_RGB11},
	{0xa0, 0xf4, ZC3XX_R10F_RG12_RGB22},
	{0xa1, 0x01, 0x0180ware; you 7de, or
 * (at your /* 84,09,00	{0xa0, 0x58, ZC3XX_R1124moinejf.free.fr>
 *
 * This p,
	{0xa0, 0x00, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0xaa, 0x23, 0x0000},
	{0xaa, 0x24, 0x00aa},
	{0xaa, 0x25, 0x00e6},
	{0xaa, 0x_R190_EXPOSURELIMITHIGH}1, ZC3XX_Rfc30x library
 *	Cox0802},				/* 08,86,02,b_R191_EXPOf00ac}, /* 00,04,ac	{0xa0, 0x98, ZC3XX_R192OSURELIMITfe Free Software Fo ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0x18, ZC3XX_R196_ANTIFLICKERMID},
	{0xa0, 0x6a, ZC3XX_R197_ANTIFLICKERLOW},
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZ6,
	{0xa0, 0x13, ZC3XX_R135_GAMMA15},
	{0xa0, 0x10, ZC3XX_R136_GAMMA16},
	{0xa0, 0x0e, ZC3XX_R137_GAMMA17},
	{0xa0, 0x0b, ZC3XX_R36HSYNC_3},
	{360xa0, 0x09, ZC3XX_R139_GAMMA19},
	{0xa0, 0x07, ZC3XX_R13A_GAMMA1A},
	{0xa0, 0x03
	u8 *jpeg_ 0xff, ZC3XX_R020_HSYNC_3},
	{3x00e6},
	{0xaa, 0x0xa0, 0x00, ZC3XX_R13D_GAMMA1D},
	{0xa0, 0x00, ZC3XX_R13E_GAMMA1 01,cb,13,cOCORRECTENABLE},
	{0xa0, 0x40, ZMA1A},
	{0xa0, 0x0_R190_EXPOS
 *	Z-Star/Vimicro zc301/zc302p/{0xa0, 0x00, ZC3XX_0, 0xf4, ZC3XX_R10C_RGB02},
	{0xa0, 0xf4, ZC3XX_R10D_RGB10},
	{0xa0, 0x58, ZC3XX_R10E_RGB11},
	{0xa0, 0xf4, ZC3XX_R10F_RG3 */
	{0xbb,rogram is free software; you 3ea0, 0xf4, ZC3XX_R111_RGB21},
	{0xa0, 0x58, ZC3XX_R112_RGB22},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x00, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0xaa, 0x23, 0x0000},
	{0xaa, 0x24, 0x00aa},
	{0xaa, 0x25, 0x00e6},
	{0xaa, 0xcu8 sharpness, /* 00,1d,ac,aa */
	cURELIMITMID},
	{0xd{0xa0, 0x24, ZC3XX_R087_R191_EXPOSc30x library
 *	Coes */
#defin3XX_R192_EXPOSURELIMITL0xa0, 0x00, ZC3XX_R0xa0, 0xf0, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,f0,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANT	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF},
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP},
	{0xa0, 0x3f, ZC3XX_R01D_HSYNC_0},
	{0xa0, 0xa5, ZC3XX_R01E_HSYNC_1},
	{0xa0, 0xf0, ZC3XX_R01F_HSYNC_2},
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3},
	{0xa0, 0x40, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x40, ZC3XX_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GGAIN},
	{0xa0, 0x40, ZC3XX_R118_BGAIN},
	{}
};
static const struct usb_action cs2102_50HZ[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xaa, 0x0f, 0x008c}, /* 00,0f,8c,aa */
	{0xaa, 0x03, 0x0005}, /* 00,03,05,aa */
	{0xaa, 0x04, 0x00ac}, /* 00,04,ac,aa */
	{0xaa, 0x10, 0x0005}, /* 00,10,05,aa */
	{0xaa, 0x11, 0x00ac}, /* 00,11,ac,aa */
	{0xaa, 0x1b, 0x0000}, /* 00,1b,00,aa */
	{0xaa, 0x1c, 0x0005}, /* 00,1c,05,aa */
	{0xaa, },
	{0xa0, 0xaa, 0x1d, 0x00a1}, /* e Free Software Foxa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x3f, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,3f,cc */
	{0xa0, 0xf7, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,f7,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0,th this program; if not, w_R135_GAMMA15},
	{0xa0, 0x10, ZC3XX_R136_GAMMA16},
	{0xa0, 0x0e, ZC3XX_R137_GAMMA17},
	{0xa0, 0x0b, ZC3XX_R41d,ac,aa *a4,4ude "gspca.h"ZC3XX_R139_GAMMA19},
	{0xa0, 0x07, ZC3XX_R13A_GAMMA1A},
	{0xa0, 0x0020_HSYNC_3} 0xff, ZC3XX_R020_HSYNC_3},
	{4c30x library
 *	Copyright (C) 2ALGAINSTEP}, /* 01,aa,24,cc */
	{0xa0, 0x8c, ZC3XX_R01D_HSYNC_0}, /* 00,1d,8c,cc */
	{0xa0, 0xb0, ZC3XX_R01E_HSYNC_1}, /* 00,1e,b0,cc */
	{0xa0, 0xd0, ZC3XX_R01F_HSYNC_2}, /* 00,1f,d0,cc */
	{}
};
static const struct usb_action cs2102_50HZScale[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /*  option) anyrogram is free software; you ram is distributed 111_RGB21},
	{0xa0, 0x58, ZC3XX_R112_RGB22},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x00, ZC3XX_R180_AUTOCORRECTENABLE}option) any later version.
 *
 * This progZC3XX_R10D_RGB10},
	{0xa0, 0x58 0x24, 0x00aa},
	{0xaa, 0x25, *		mxhaard@magic.frbX_R12E_GAMM */
	{0xaa, 0x1d, 0x00b of the GNU Generaxa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x3f, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,3f,cc */
	{0xa0, 0xf7, ZC3XX_R192_EXPOSURELIMIT *gspca_dev,MID},
	{0xa0, 0x6a, ZC3d a copy of the GNU General Public License
 * a5,00,cc */
	{0xa0, 	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF},
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP},
	{0xa9ZC3XX_R18C_AE9REEZE}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,cc *	{0xa0, 0x24 0xff, ZC3XX_R020_HSYNC_3},
	{9}, /* 01,a9,10,cc */
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,24,cc */
XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x40, ZC3XX_R116_RGAIN},
0x90, ZC3XX_R01E_HSYNC_1}, /* 00,1e,90,cc */
	{0xa0, 0xd0, 0x00c8}, /* 00,c8,d0,cc */
	{}
};
static const struct usb_action cs2102_60HZScale[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xaa, 0x0f, 0x00b7}, /* 00,0f,b7,aa */
	{0xaa, 0x03, 0x0005}, /* 00,03,05,aa */
	{0xaa, 0x04, 0x00be}, /* 00,04,be,aa */
	{0xaa, 0x10, 0x0005}, /* 00,10,05,aa */
	{0xaa, 0x11, 0x00be}, /* 00,11,be,aa */
	{0xaa, 0x1b, 0x0000}, /* 00,1b,00,aa */
	{0xaa, 0x1c, 0x0005}, /* 00,1c,05,aa */
	{0xaa, 0x1d, 0x00be}, /* 00,1d,be,aa */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x3f, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,3f,cc */
	{0xa0, 0xfc, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,fc,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc _vf0_JPE int force_sensor = -1;

#define QUANT_VAL 1		/* quantlied wx00, xb0, 	{0xa0, 0xb0, ZC1, ZC3XX_R005_FRAMEHEIGHTHIG,cc */
	{8{0xa0,	{0xa0, 0xb0, ZC3XX_R01F_HS{0xaa, 0x12, 0x0089},cc */
	10xa0, 0x10, ZC3XX_R197ty */
#define QUALITY_MIN 40,cc */
	{2aa, 0x, MERCHAN * 0<->10_RGB20},
	{0xa0LITY_MAX 60
#define QUALITY_DEcc */
	{3xa0, 0x10, ZC3XX_R18Ced char sensor;		/* Type of im,cc */
	{4/vc30x	{0xa0, 0xb0, ZC3XX_R01F_HSY values used in difcc */
	{5xa0, 0x10, ZC3XX_R197es */
#define SENSOR_ADCM2700 0cc */
	{6XPOSUR	{0xa0, 0xb0, Z9,
	{0xa0, 01
#define SENSOR_C,cc */
	8b,9e Fr	{0xa0, 0xb0, ZC3XX_R01F_HSYGC0305 3
#define SEcc */
	{ 0xb0, 	{0xa0, 0xb0, ZCXX_R129_GAMM SENSOR_HV7131B 5
#dxa0, 0x22 ZC3XXx10, ZC3XX_R18C_AEFREEZE}, /,0f,59,aa */
	{0xaa, 0x03, 0x00 */
	{0xaa, 0x0f, 0x_HV7131C 6
#define SENSOR_IC,cc */
	98C3XX_R1 library
 *	Copyright (C)E},
	{0xa0, 0x08, , 0x11, 0xaa, 0x /* 00,11,80,aa */
	{0xaa,X_R086_EXPTIMEH GNU Gene,aa */
	{0xaa, 0x1c, 0x0005}, /* 00define SENSOR GNU Genecaa */
	{0xaa, 0x1c, 0{0xa1, 0x01, 0x0002},
	{0xa1,, 0x11, 0c,ex00e1A9_DIGITALL6<->0, 0xf4, ZC3XX_NG},	/* 00 */
	{0xa0, 0x08, , 0x11, 0e,8EXPOSURELIMITMID}, /* 01,91,3f,cc *40 * 480 * 3 / 8 + 590,
		.oFlikerSc00b7}, {}
};
static const struct usb_action cs2102_NoFlikerScale[] = {
	{0xa0,
#include "jp24HIGH}, /* 1b,2425,  librarydmbridgeWIDTHL, ZC,cc */
	{0x80,1,97 */
	{0xa0, 0x10, ZC3*/
	{97_ANTIFLISPCA R18C_AEFREEZE}, 00, ZC3X210,cc */
	{ALGAI 0x20, ZC3XX_R18F_MODULE_DX_R197_ANTIF5NC_0},
	{0YNC_0},
	LE_NAMd3, ZC3X1d,a */
	{0xa0, 0c */
	{0xa0,#define MODULE_NAME "zc,cc */
	{nclude R18C_AEFREEZE}, d <mxha_R10,cc */
	{c,1R01DTEP}, /* 01,aa,0LITY_MAX 6085, ZC0x01, ZC3XoFlikerSc6,8AINSTEP}, /* 01,aa,0ma;
	__u8 a5_ANTIFLICKERHIGH}, /* 01,805}, /* 00,03,05,aa8ev *gspca_d88_1}, /* 192_EXPOSUREL88,800ac, 0x00, ZC3XX_R1MODULE_cc *,cc */
	{5x20,x59, ZC3XX_R01D_H_u8 ,aa,00,cc */
	{00a0, 0x59, ZC3XX_R01D_H sd {
	saZC3XX_R002_Cb,aKSELECT},
	{0xa0, 0xd <mxhaXX_R008_CLOCKEXPOSNG},
	{0xa0, 0x08,mbridgeR010_CMOSSENSdRSELECT},
	{0xa0, 0x02g.h"

/XX_R008_CLOCKeETTING},
	{0xa0, 0x08,fZC3XX_R004_FRAMEWIDfRSELECT},
	{0xa0, 0x0 opt,aa,00,cc */
	{0xx00, x59, ZC3XX_R01D_HSe */
#iR010_CMOSSEN11RSELECT},
	YNC_0},
	{0xa0,gspca_de2C_GAMMA0C},
	{0xa0, 03, 0x */
	{0xa0, 0xerge A. 1* 00,1d,59,6MA17},0x00, ZC3XX_R0987XX_R00e 0x0EOCONTRO71_EXx00,(e6 -> e8)x00, ZC3XX_R0988EUNFREEZE}, /* 01,80xa0,x00, ZC3XX_R09A_WIsizeim08OW},
	{0xa0,9/
	{x00, ZC3XX_R09A_Wx01, ZC3XZC3XX_R002_2DTHLO
	{0xa0, 0x90, ZC= V4L2_COLORSPACE_JPEG,
		.priv =ul,
 * 01,bxa0, /* 00,11,80,aa AS202B 12
#define SENSOR_PB0330  0x03, 0x005R092_I2CADDRESSSELE= {
	{176, 144, V4L2_PIX_FMT_lied warEWIDTH092_I2CADDRESSSEL7IELD_NONE,
		.bytesperlinlied warr9,7EXPOS library
 *	Copsizeimage =CHANTABe Lid,09*/
	{0xaa, 0x0f, 0x0059}, /* 90,
		.colorspace =lied warc50005}, /* 00,03,05,aaSPACE_JPEG,
		.priv = 1},
	{, ZC3XX_Rb,8f,,
	{0xa0, 0x02, ZCL2_PIX_FMT_JPEG, V4L2_FIELD_NOul,
 2,5
	{0xxa0, 0x02, ZC3XX_R093_I2CSET352,
		.sizeimage ,
	{0xMMA1794_I2CWRITEACK},
	{0undation; either version 2 olied waricense, 0c, ZC3XX_R092_Iusb exchanges */
struSETVALUE}16,6 */
	{0xaa, 0x0f, 0on {
	__u8	req;
	__u8	4_I2CWRITE8,6
	{0{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xa0, 0x10, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,10,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREE/
	{0xa0, 0x0a, ZC3XX_R010_C0, 0x00, ZC95,00,cc */
	{0xa0, 00,cc */
	{0xa0, 0x00, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,00,cc */
	{0xa0, 0x59, ZC3XX_R01D_HSYNC_0}, /* 00,1d,59,cc */
	{0xa0, 0x90, ZC3XX_R01E_HSYNC_1}, /* 00,1e,90,cc */
	{0xa0, 0xc8, ZC3XX_R01F_HSYNC_2}, /* 00,1f,c8,cc */
	{}
};
static const struct usb_action cs2102_NoFlikerScale[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xaa, 0x0f, 0x0059}, /* 00,0f,59,aa */
	{0xaa, 0x03, 0x0005}, /* 00,03,05,aa */
	{0xaa, 0x04, 0x0080}, /* 00,04,80,aa */
	{0xaa, 0x10, 0x0005}, /* 00,10,05,aa */
	{0xaa, 0x11, 0x0080}, /* 00,11,80,aa */
	{0xaa, 0x1b, 0x0000}, /* 00,1b,00,aa */
	{0xaa, 0x1c, 0x0005}, /* 00,1c,05,aa */
	{0xaa, 0x1d, 0x0080}, /* 00,1d,80,aa */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x3f, ZC3XX_R191_4_I2CA9_DIGITALL8<->6 01,91,3f,cc */
	{0xa0, 0xf0, ZC3XX_R192_EXPOSURELIMIT},
	{0xa0, 0x10, ZC3XX_R092_I2CADDR0, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x10, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,10,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,cc */
	{0xa0, 0x00, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,00,cc */
	{0xa0, 0x00, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,00,cc */
	{0xa0, 0x59, ZC3XX_R01D_HSYNC_0}, /* 00,1d,59,cc */
	{0xa0, 0x90, ZC3XX_R01E_HSYNC_1}, /* 00,1e,9,cc */
	{0xa0, 0xc8, ZC3XX_R01F_HSYNC_2}, /* 00,1f,c8,cc /
	{}
};

/* CS2102_KOCOM */
static const struct usb_actin cs2102K_Initial[] = {
	{0xa0, 0x11, ZC3XX_R002_CLOCKSELECT},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},
	{0xa0, 0x08, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x00, C3XX_R11A_FIRSTYLOW},
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},
	{0xa0, 0xe8, ZC3XX_R09C_WINHEIGHefor},
	{0xa0, 0x88, ZC3XX_R09E_WINWIDTHLOW},
	{0xa0, 0x55, ZC3XX_R08B_I2CDEVICEADDR},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x0a, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x02, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x0b, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x02, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x0c, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x7c, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{xa0, 0x0d, ZC3XX_R092_I2C/* "},
	" light frequency banding filtermera Driver");
MODULE_LICENSE("GPL");

sta,00,cc */},
	{0xa0, 0x13,ve, Ca2XX_R001_SYSTEMOPER8ZC3XX},
	{0xa0, 0x0b,1F_HNYSTARTLOW},
	{08MMA17},
	{0xa0, 0x0b,stat_R012ae,90,cc */
KERLOEZE}, /* 01,8c,10rix */
	{0xa0, 0xf4, ZC3XX_R10B_RGB01}, 0x00, ZC	{0xa0, 0x02, ZCIELD_NONE,
RGB02},
	{0xa0, 0xf4, ZC3X,
	{0x090_I2CCOMMAND},
	{a{0xa0, 0x58, ZC3XX_R10E_RGB11},
	{0xc302p/5 */
	{0xaa, 0x1c, 0x0005}, /* 0
	{0xa0, 0xf4, ZC3XX_R110ard
 *		mx0},
	{0xa0, 0x22, ZC3XX_R131X_R111_RGB21},
	{0xa0, xa0, 0xf4,S2102_KOCOM */
s0,19,00,cc */
	{0xaa, 0x0f, 0x
	{0xa0,	{0xa0,	{0xa0, 0x02, ZC */
	{0xbb, 0x01, 0x8000lied warrcGHTH2CCOMMAND},
	{0xa0* 04,00,00,,
	{0xa0, 0x00,lied warrfALLI3XX_R18F_AEUNFREEZE}, /* 01,001},
	{0xaa, 0x24, 0x0055} program i	{0xa0, 0x02, Z 0x00cc},
	{0xaa, 0x21, 0x003f},
	{00x25, 0Initial[] = {

	{0xa0, 0x23, ZC3XX_R086_EXP,
	{0xa0,d,60, 0x10, ZC3XX_R18C	{0xa0, 0x24, ZC3XX_R087,
	{0xa0,e/* 01,	{0xa0, 0x02, Z},
	{0xa0, 0x25, ZC3XX_R,
	{0xa0,f0aa}, 	{0xa0, 0x02, Z 01,91,3f,cc */
	{0xa0, _WINWIDTHLOa0, 0	{0xa0, 0x02, ZICULAR PURPOSE.  See the
 * GNU General Pub	{0xa0, 0x02, Zr more details.
 *
 * You should have received a I2CCOMMAND},
	{0Y; without even the implied warran7e Free Sof/VC3xx US, 0xaxa0, 7, ZC3XX_R125_GAMMA05},
	{0xa0, 0xb9, ZC3XX_R126_GAMMA06},
	{0xa0, 0xc8, ZC3XX_R127_GAMxa0, 0x01, ZC3XX_0, 0xd4, ZC3XX_R128_GAMMA08},
	{0xa0, 0xdf, ZC3XX_R129_G3XX_R10C_RG0xa0{0xa0, 0x09, _R12A_GAMMA0A5X_R197_ANTI84,5ITDIFF},12B_GAMMA0B},
	{0xa0, 0xf4, ZC3XX_R12C_GAMMA0C},
	{0xa0, 0xf9, ZC3XX_R12D_G2_RGB22},
	{xa0, 0xfc, ZC3XX_R12E_GAMMA0E},
	{0xa0, 0xff, ZC3X, ZC3XX_R1970F},
	{0xa0, 0x26, ZC3XX_R130_GAMMA10},
	{0xa0, 0x22, ZC3XX_R131_GAMMA11},
	{0xa0, 0x20, ZC3XX_R132_GAMMA12},
	{0xa0, 0x1c, ZC3XX_R133_GAMMA13},
	{0xa0, 0x16, ZC3XX_R134_GAMMA14},
	{0xa0, 0x13, ZC3XX_R135_GAMMA15},
837_GAMMA17},
	{0xa0R136_GAMMA16},
	{0xa0, 0x0d, ZC3XX_R137_GAMMA17},
	{0xa0, 0x0b, ZC3XX_R138_GAMMA18},
	{0xa0, 0x09, ZC3XX_R139_GAMMA19},
	{0xa0, 0x07, ZC3XX_R13A_GAMMA1A},
	{0xa0, 0x06, ZC3XX_R13B_GAMMA1B},
	{0xa0, 0x05, ZC3XX_R13C_GAMMA1C},
	{0xa0, 0x04, ZC3XX_R13D_GAMMA1D},
	{0xa0, 0x03, ZC3XX_R13E_GAMMA1E},
	{0xa0, 0x02, ZC3XX_R13F_GAMMA1F},
	{0xa0, 0x58, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xf4, ZC3XX_R10B_RGB01},
	{0xa0, 0xf4, ZC3XX_R10C_RGB02},
	{0xa0, 0xf4, ZC3XX_R10D_RGB10},
	{0xa0, 0x58, ZC3XX_R10E_RGB11},
	{0xa0, 0xf4, ZC3XX_R10F_RGB12},
	{0xa0, 0xf4, ZC3XX_R110_RGB20},
	{0xa0, 0xf4, ZC3XX_R111_RGB21},
	{0xa0, 0x58, ZC3XX_R112_RGB22_ANT7, ZC3XX_R125_GAMMA05},
	{0xa0, 0xb9, ZC3XX_R126_GAMMA06},
	{0xa0, 0xc8, ZC3XX_R127_ANTIFLICKERMID}0, 0xd4, ZC3XX_R128_GAMMA08},
	{0xa0, 0xdf, ZC3XX_R129_GAMMA09},
	{0xa0, 0xe7, ZC3XX_R12A_GAMMA0A6ix */
	{0xa84 ZC3C3XX_R12B_GAMMA0B},
	{0xa0, 0xf4, ZC3XX_R12C_GAMMA0C},
	{0xa0, 0xf9, ZC3XX_R12D_G* 04,00,00,CSETVALUE},
	{0xa0, 0x00, ZC3XX_09, ZC3XX_R139_GAMMTY; without 0F},
	{0xa0, 0x26, ZC3XX_R130_GCSETVA},
	{0xa0, 0x22, ZC3XX_R131_GAMMA11},
	{0xa0, 0x20, ZC3XX_R132_GAMMA12},
	{0xa0, 0x1c, ZC3XX_R133_GAMMA13},
	{0xa0, 0x16, ZC3XX_R134_GAMM3YSTEMCONTROL 0x13, ZC3XX_R135_GAMMA15},
3bRESSSELECT},
	{0xa0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x21, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x22, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x01, ZC3XX_R0A3_EXPOSURETIMEHIGH},
	{0xa0, 0x22, ZC3XX_R0A4_EXPOSURETIMELOW},
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},
	{0xa0, 0x07, ZC3XX_R191_EXPOSURELIMITMID},
	{0xa0, 0xee, ZC3XX_R192_EXPOSUREL{0xa0, 0x00, ZC3XX_R180_AUTOCORXX_R195_ANTIFLICKERHIGH},
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICK 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE_AEUNFREEZE}f,20,cce7, ZC3XX_R12A_GAMMA0Ac	{0xa0, 0x01, cC3XX_R090_I2CCOMMAND},
	{0xa0, 0x13, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x22, ZCYSTEMCONTROLxa0, 0xfc, ZC3XX_R12E_GAMMA1NSOR, ZC3XX_R139_GAMMA19},
	{0xa00F},
	{0xa0, 0x26, ZC3XX_R13, 0x05, ZC3XX_R092_I2CA, ZC3XX_R131_GAMMA11},
	{0xa0, 0x20, Z3XX_R132_GAMMA12},
	{0xa0, 0x1c, ZC3XX_R133_GAMMA13},
	{0xa0 0x16, ZC3XX_R134_GAMM0xa0, 0x0a, E},
	{0xa0, 0x42, ZC3XX_R17ECT},
	{0xa0, 0x02, ZC136_GAMMA16},
	{0xa0, 0x0d, ZC3X_R137_GAMMA17},
	{0xa0, 0x0b, ZC3XX_R138_GAMMA18},
	{0xa0 0x09, ZC3XX_R139_GAMMA19},
	{0xa0, 0x07, ZC3XX_R13A_GAMMA1A},	{0xa0, 0x06, ZC3XX_R13B_GAMMA1B},
	{0xa0, 0x05, ZC3XX_R13C_GAMA1C},
	{0xa0, 0x04, ZC3XX_R13D_GAMMA1D},
	{0xa0, 0x0, ZC3XX_R13E_GAMMA1E},
	{0xa0, 0x02, ZC3XX_R13F_GAMMA1},
	{0xa0, 0x58, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa, 0xf4, ZC3XX_R10B_RGB01},
	{0xa0, 0xf4, ZC3XX_R10C_RG02},
	{0xa0, 0xf4, ZC3XX_R10D_RGB10},
	{0xa0, 0x58, ZC3XXR10E_RGB11},
	{0xa0, 0xf4, ZC3XX_R10F_RGB12},
	{0xa0, 0xf4, ZC3XX_R110_RGB20},
	{0xa0, 0xf4, ZC3XX_R111_RGB21},
	{0xa0 0x58, ZC3XX_R112_RGB22th this 7, ZC3XX_R125_GAMMA05},
	{0a0, 0xb9, ZC3XX_R126_GAMMA06},
	{0xa0, 0xc8, ZC3XX_R127th this program; if not, w0_GAMMA10},
TEACK},
	{0xa0, 0x01, ZC3XX_R of 0x00, ZC3XX_R09d4, ZC3XX_R128_GAMMA08},
	{0xa0, 0xdf, ZC3XX_R129_G_R128_GAMMA03},
	{0xa0, 0xdf, ZC3XGAMMA0AR09E_WINWIDT84OW},
	* 01,a9,10,cc */
	{0xa0, 0x24, ZC3XX_R1AA_DIGITA01, ZC3XX_R1AA_DIGITALGAINSTEP},
	{0xa0, 0x04, ZC3XX_R01D_HSYNC_0},
	{0xa0, 0x0f, ZC3XX_R01E_HSYNC_1},
	{0xa0, 0x19, ZC3XX_R01F_HSYNC_2},
	{0xa0, 0x1f, ZC3XX_R020_HSYNC_3},
	{0xa0, 0x60, ZC3XX_R11D_GLOBALGAIN},
	{0xa0, 0x60, ZC3XX_R11D_GLOBALGAIN},
	{0xa0, 0x42, ZC3XX_R180_AUTOPS}, /* 00,19,00,cc */
	{0xaa, GAMMA15},
0x05, ZC3XX_R092_I2CA136_GAMMA16},
	{0xa0, 0x0d, ZC3XX_R137_GAMMA17},
	{0xa0, 0x0b, ZC3XX_R138_GAMMA18},
	{0xa0, 0x09, ZC3XX_R139_GAMMZC3XX_R13D_GAMMA1D},
	{0xa0, 0x03, ZC3XX_R13E_GAMMA1E},
	{0xa0, 0x02, ZC3XX_R13F_GAMMA1F},
	{0xa0, 0x58, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xf4, ZC3XX_R10B_RGB01},
	{0xa0, 0xf4, ZC3XX_R10C_RGB02},
	{0xa0, 0xf4, ZC3XX_R10D_RGB10},
	{0xa0, 0x58, ZC3XX_R10E_RGB11},
	{0xa0, 0a0, 0xb7, ZCls.
 *
 * You should have receiv3XX_R197_ANTIFLI	{0xa0, 0x01,{0xa0, 0x00, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0	{0xa0, 0x00, ZC3XX_R196_ANTIFL	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},	{0x0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R0994_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0ialScale[] = {
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},
	{0xa0, 0x00, ZC3XX_R002_CLOCKSELECT},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},
	{0xa0, 0x08, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},
	{0GNU Genu8 reg_r_i(ublic Lgspca_dev *ZC3XX_R09{0xa__u16 index)
{
	icencontrol_msg(ZC3XX_R09->RESSSELx00, rcvctrlpipeI2CSETVALUE},
	{ 0){0xa0 0x010xa0USB_DIR_IN | C3XXTYPE_VENDORI2CCOMMRECIP_ine SE{0xa0, 01
	{0xa0value
 * MER	{0xa, ZC3XX_R09->icenbuf, x01, Z500);
	returnZC3XX_R093_I2CSETVA[0];
}I2CCOMMAND},
	{0x, 0x05, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x0},
	t;
 0x00 =,
	{0xa0,I2CADDRESS
	{0xa0;
	PDEBUG(D_USBI, "reg r [%04x]_R11%02x"XX_R093,DRES0, 0x00, ZCRESSS	{0xa0, 0xvoid,
	{0wa0, 0x05, icendevice *,
	{0xa0__u8CT},
	92_I2CAD},
	{0xa0, 0x00, ZC3XX_R093_I,
	{0xa0, 0xsnd ZC3XX_R0ACK},
	{0xa0, 0001, ZC3XX_R09OUTI2CCOMMAND},
	{0xa0, 0x06, ZC3XX_R092_I2CAESSSELCWRITEACNULL, ITEACK0xa0, C3XX_R090_I2CCOMMAND
	{0xa0, 0x0e, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, SETVALUE},
	O0xa0, 0w00, ZC3X=R094_I2CWRITEACT},
	0, 0x0AND},
ZC3XX, ZC3XX_R092},
	{0xa0, 0x,
	{0xa2c_read, 0x05, ZC3XX_R092_I2CADDRESSSEL2CADDRreg0, 0x_R094_Itbyte;ITEAC16},
	valSSELECAND},
94_I2CWRITEACK},reg, ZC3XX2 ZC3XX_R092_94_I2CWRITEACK},
CADDRZC3XX_);2, ZC<- 093_ comman0xa0,	msleep(250, 0x00{0xa},
	{0xa0, 0x08, ZC3XXZC3XX1XX_R093CSETVGNU usx00, ZC3XX_I2CWRITEACK},
	{0xa0, 0x01,5ZC3XX_R090_ILow094_IND},
	{0xa0|2CWRITEACK},
	{0xa0, 0x01,6) << 8;XX_R090_IHZC3Xa0, 0x18,SETVALUE},
	{0xai2c0x00, 2C3XX_R094x (0_I2)"01, Z11, ZZC3XX_ZC3XX{0xa
	{0xa0, 0x01,XX_R0{0xa0, 0x18, 83XX_RwriteI2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I292_I2CADDRESSL92_I2CADDRESSHCWRITEACK},
	{0xa0,90_I2CCOMMAND},
	{0xa0, 0x11, ZC3_R092_I2CADDRESSSELECT},
	{0xaxa0, ESSS3LECT},
	{0xa0, 0x0c, ZC3XX_R093HI2CSE4092_I2CADDRESSSELECT},
	{0xa0, e */
C3XX_R093_I2I2CSEVALUE},
	{0xa0, 0x001 ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},{0xa0, 0x01, ZC33XX_090_II2CCMMAND}ITEA,
	{0xa0, 0x15, ZCR094_I093_I2_I2CADDRESSSELECT},
	{0xa0,C3XX_R090_I2CCOicenexchangETVALUE},
	{0xa0, 0x00, ZC3XX_R094eral Public License
 * a*se
 * 0, 0xwhile (se
 * ->req)ram;	switch0, 0x01, ZC3XX_R090caseC3XX0:NTABI0xa0,regis0, 0xb90x15, _wESSSELECT},
	{0xa 0x01, Z_R092 0x01, Zid3_I2C		break0, 07, ZC3XX1R101_090_I2CCOMMAND},	{0xa0r 0x08, ZC3XXFUNC},
	{0xa0, 0x78, ZC3XX_R18D_YTa:a0, 093_I2CSET0, 0x00, ZC3XX	 2_VIDEOCONTROHANTAB0, 0 * MERCR1C5_SHARPidx &RGB01}SELECT},L0xa0, 0x13, ZC3XX_R1CB>> 8XX_R093R0940xa0, 078, ZC3XX_R18D_YbbBSTATUS},
	{0xa0, 0x03, ZC3XX_R1C5_SHARP_R087_EXNESS05
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x20, ZC3XXvalXX_RMEMID},
	{0xa0, 0x21, ZCdefault:
/*XX_R18D_Ydd:TALLdelay0xa0, 00, 0x0000, 0x01ad} / 64 + 1a0, 00, 0x21, ZC}
		se
 * ++{0xaLE},
	{0x1);0xa0,}I2CADDRESSSELECTset0x01, , 0x05, ZC3XX_R092_I2CADDRES0, 0xublic Lsd *sd = , 0x05, /* c) ZC3XX_R09;
	int i;
0xa0, 03XX_R*0x01, ;ETTIU General Pu8 adcm2700_0x01, [9] =R117_{			/* 0,12,0sharpness- */
	/* sharpness- */
	{0xa0, }{0xa0/*ms-winN},
	{0x7GAMMAarpness- */
	{0xa00, 0x38, ZC3XX_R121_GAMMA0}ness+ */
	{0xa0,3XX_Rgc0305X_R1CB_SHARPN	{0xa0,0d0x3f, 	{0xa0, 0x92, 3},
	{0xa0, 0x92, ZC3XX_R1R122_GAMMA02},
	{0xa0,ov762XX_R1CB_SHARPNAMMA03xa0, 0GAMMAMA06},
	{0xa26_GAMMA06},
	{0xa0, 0xc8,R122_GAMMA02},
	{0xa0,pas202ba0, 0xb9, ZC3XX_R1700_098_WIGB01},bb, 0x ZC3XXX_R129_GAude "arpnes5f0xa0, 0xd4, ZC3XX_R128_ong wi0, 0xb9, ZC3XX_R1unda0, ZC3xf4, ZC3XX_R120, 0xf4, ZC3XX_R12C_GAMMA0R122_GAMMA02},
	{0xa0,0,cc */0, 0xb9, ZC3XX_R173XX_R103, Z0xa0, 0xff, E},
	{0xa0, 0xff, ZC3XX_R1R122_GAMMA02},
	{0xa0,* sharp_tb[CE_JPE},
	rogram;	0f, ZC3XX_R1CB_XPOSUx22, ZCADCMZC3X10_RGB2	2CADD{0xa0x22, ZCCS2102 1A12},
	{0xa0, 0x1c, ZC3XX_R13K 20xa0,  0x79, ZC3XX__R008x22, ZCGCx79, X_R10D
	{0xa0, 0x1c, ZC3HDCS2020b a0, 0x5},
	{0xa0, 0x10, ZV7131B
	{0xa016},
	{0xa0, 0x0d, ZC3XXC XX_R09
	{0xa0, 0x1c, ZC3ICM105A 7_R138_GAMMA18},
	{0xa0,MC501CB 0, 0xf	,
	{0xa0, 0xb, 0x13, ZC3XOV	{0x 9_R138_GAMMA18},
	{0xa0,x06,30C DIFF}, 
	{0xa0, 0x1c, ZC3PAS106 1_GAMMA1_GAMMA08},
	{0, 0x13, ZC3XPAXX_RB 1134_GAM13C_GAMMA1C},
	{0xB0330 1GAMMA153XX_R12B_GAMM},
	{0xa0, 0xong w 1_GAMMA16},
	{0xa0, 0x0d,TAS
stati 1137_GAMMA17},
	{0xa0, 0x/
	{0xa0XX 1X_R138_ 0xfc, ZC3XX_, 0x13, ZC3X/
	{0xa0_VFcc * 1C3XX_R
	{0	0x01, Z= 0x01, 0, 0sd->sensor]X_R1f (, 0x58, =I2CAD)
x15,0, ZX_R0930x01, Zal093_y loade
	{0xafor (i = 0; i < ARRAY_SIZE(,
	{0xa0, 0xb); i++_RGB12a0, 0x05, ZC3XX_R012_R1CB_Si]XX_R090a + i},
	{0xa0, 0x18, ZsetbrZC3X80, 	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* clock ? */
	{0xa0, 0x08, ZC3XX_R3XX_Rxa0, 0x00,0xa0_I2CCOMMRGB11},
	{X_R097, ZC3, ZC3XX_R135:a0, 0x00, ZC3Xx06, Z_I2CWRITEACK},
, ZC3XBSTA12},
	{0, 0/*fixme: is it},
	lly_SENSORto 011d },
	 ZC3 	{0xall other 1},
	{s_GAMMAxa0, 0x00, = RGB10x00, ZC3XX_3XX_R112_RGB22},
	{0xa0xa0, 0x00,XX_R091d_I2C93_I2CSETVALUE},
	{0xa0, 0x00, ZC3X_R132_GA_I2CWRITEACK},
 ZC3XX_CCOMMAND},
	{0x0, 0xALUE},
	{0x<XX_R0_RGBALUE},
	{0x+=CLOCK;
	else00, ZC3XX_R094I2CW8ITEA4_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R080_I2ORRECTENABLE},
	{00, 0x80, 	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* clock ? */
	{0xa0, 0x08, ZC3XX_R	{0xa0, 0x0e, ZC3XX_R0 =ZC3XX_R093_IC3XX_R1C6_0, 0x80, 122_GAMMA02},
	{0xa0,0, 0x80, 0, 0][2_R131_GA{0, 0x18, 00,01_I2CSGAMMA0{0xa0_I2CSa0, 0xf ZC3XX_RC3XX_R1e}
	{0xa00, 0x80, xa0, 0xC3XX_R092_I2XX_R11ZC3XX{0xa0, 0x22, 0, 0x80, ][0ZC3XX_Rc6 ZC3XX_RR100_OPERATIOA0B},
0, 0x04, ZC3XX_R093_I2CSET90, 0x04, ZC3XX_R093_I2CSETa ZC3XX_R0x18, ZC3XX_R092_I2CADDRESSSELE1T},
	{0xb_I2CADDRESSSELECT},
	ZC3XXast01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x2PNESS00},	/*T1,cc ,
	{0radientX_R1C6_g, i, ZC3X_GAMMA02},
	{0xa0,k1,cc 0, 016] =, ZCdeltaa0, 0H},
	{0x0xa0, CK},MODULEmbridg03, ZCsizeima0, 0xD},
	{0xa0, 0x
		ytesperRMID},
	{0xa0, 0x3a, ZCANTIFLICKERLOW},
R122_GAMMA02},
	{0xa0,ke, Z3XX_R195_ 0x00, ude "jerge AX_R1290x18, Z, ZC3XX_R1A9_DIGITA3XX_R19XX_R1A9_DIGITALLIMIT1},
	{0xa00x18, erge A_R122_GAMMA02},
	{0xa0,{0xa0,_1X_R18F_AEUNFXX_R1AA_DIGITX_R129rpnesREEZE}* 240x4129_G5fTDIFF},7,
	{09X_R12aude "ca0, 0d, 0x38129_GAX_R12fMA0A},
	{0xa0, 0xee, ZCee, ZC3XXNC_0},
	{0xa0, 0x0f, e */
#MODULEude ",
	{0xa129_Gxa0, 01aTDIFF},ULE_NA0xa0, 0/* sh, /*x0010}, ZC3XX_Rmbridg6	{0xa0, 0x04, ZC3XX_R01D_HSYN2_0},
	{0xa0, e */
#C3XX_1MMA09303, Z5X_R126rpnes, ZC0x9	{0xaR088ZC3XX_},
	{d 0x1f,xa0, e,
	{0f},
	{},
	{0_3},
	{0xa0, 0x60, ZC3XX_R11D_GLOB	{0xa0, 0x60, MODULEMMA09CORRECTa0, 0xe8, ZELECT},NXSTA1542, ZC3X},
	{C3XX_R0g.h"

ude "jCKERMID},
	{
	{0xa0	{0xa0, 0x04, ZC3XX_R01D_HSYN3_0},
	{0xa0, ENABLE/* shMEWIDT4xa0, x24,0x890_I2CZC3XXa7_GGAIN},xa0, crpnes3XX_0xe, 0x38, ZC3xe7, ZC3XX_R_3},
	{0xa0, 0x60, ZC3XX_R11D_GLOBZC3XX_R090_I2CR116_RORRECTD_GLOBR116_RLECT},xa0, 0},
	{0242, ZC3X22, ZC3XXE},
	{0xa0, 0x00, ZC3XX_R09MODULE4_I2CWRITEACK},
	{0xa0, 0x01, 4X_R18F_AEUNFRX_R12FRAM0x5,
	{0F_HSYNC},
	{aNXSTAb,
	{0cZC3XX_R1, ZC3XdMMA09eNXSTAexa0, MA06},
0xa0, C3XX_R3},
	{0xa0, 0x60, ZC3XX_R11D_GLOB00, ZC3XX_R0900},d in t88, ZC3XI2CCOMMORRECTX_R12R092_I2CA3XX_R19
	{0xa0, 0x0NXSTAR 0x00, ZC3XXGAMMA00, 0x0c	{0xa0, 0x04, ZC3XX_R01D_HSYN5TVALUE},
	{0x0xa0, ude ",d8, Cambridx00, ZbXX_R1cXX_R1d, ZC3XX_NSOR3XX_R0203XX_GAIN},
	{a0, 0fx5c, f093_I23},
	{0xa0, 0x60, ZC3XX_R11D_GLOB, 0x01, ZC3XXUNC}{0xa0, 0x0,9e,86,cECTENABLE2CSETVALUE},
	3XX_R1990_I2CCOMMAND},
	{0xa0, 0x14, ZC3XX_RC3XX_R092_I2CADDRESSSELECT},
	{0xa06_R195_A, ZC?? was01,cc *137_GAM	{0xENABL4ENABL6ENABL3_I2CS9CADDRbx01, c,
	{0x3a0, 0x0es *0xeLUE},
A06},
MMA09}CGLOBALMEAN},
	{0xa},
	{0xa0, 0x60, ZC3XX_R11D_GLOB 0x00,  0x00, ITEAC4, ZC3C3XX_R094_I2CWRITEACK},
	{0xa0,ELECT, ZC3XX_R090_I2CCOMMAN	{0xa0, 0x28, ZC3XX_R1AA16, ZC3XX_R130_GAMMA10},0, ZC3XX__R131_GA2CADDR1D_HSYNCMAND},
	{, ZC3 0x01, ZMAND},
	{4MAND},
	{5MAND},
	{6
	{0xRITEACK},
	{0xa0, 0x011D_GLOB3XX_R090_I2CCOMMAND11D_GLOBA094_I2CWRITE, ZC3XXI2CWRITEA094_I2CWRITESSELE92_I2CADD,
	{0xa0, 0x96, ZC#ifdef GSPCA_ETVAL{0xa0, v_R19;
#endif

3XX_R09OMMA, ZC3XX_RGB1E},
	0xa0	{0xa0, 0OMMA,
	{0xa0, 00, 0x00, ZC3
	kck ? d->H},
	{0xa- 128)ZC3XX-128 /,
	{0xa0, 0*AND},
	CK},
, 0x00, ZCCONF, "E},
	:%TVAL,
	{0x3XX_1,cc *coeff: %d/1280, 0x0, 0x00, ,0, 0xR093_I2C, k_I2C	{0xa0, 0xf4, ZC160x58, 31_GAg =2CADDRESi] +x00, ZC3XX_i] * k, ZC3XGAIN, 0xg >E},
	_RGB,
	{0OBALGAINCK}, },
	{0<= 0x00,,
	{01GAIN, ZC3XX_R09, ZC3X1200_AUTO05,01,cc **/ADDRESSSELECT},
	{0xaT},
	{C3XX_R0bug & 0xa0, 0xa0,v_I2C= gC3XX_R0933_I2CT},
	{0xa0, 0x9tb:R094__I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_0, 0xvECT},v0x01,v[2	{0xa3	{0xa4	{0xa5	{0xa6	{0xa7]_I2CSETVALUE}a0, 0x9   _I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRIT8	{0xa9ACK},
EACK},

	{0xa10, 0x011, ZC31XX_R0915,
	{00x01, ZC3XX_R090_I2CCOMMAND},
	{0xa, ZC3XX_I2C-x20, ZC3XXI2CADDRESSSELECT},
	{0xa0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{031_GAT},
	i != 150xa0, ZC3XXGAIN}CK},
	{0, 0x00, ZC 0x43XX_R094_I2CWRITEA3K},
	{0xa0, R094_I2C ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x04, ZC3XX_R1A7_CAEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAORRECTENABLE},
	{0quality01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x23XX_RfrxSSSELCOMMAND},
	{0xa0, 0x14, ZC3XX_R092_I2CADDRESSSELECT},
	{0xX_R094_I2CWRITEACK},
a0, 0x01, CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x13, ZC3XX_R092_Ixa0,DRES7 0018a0, 0x22, ZC3XX_R093_I2CSETV, ZC3XX_R09QUANT_VA_I2CSxa0,MEAN}rx2CWR
	{0{0xa0, 0x18, Z94_IWIDTHLOW)CADDR	{0xa0, 0x3XX_0 ||2CCOMMAND},
	{1xa0, 0x18, ZC3XX_2R094_I2CWRIR093#el_I2CCOMMAND},
	{3CT},
	{0xa0,00x04, ZC3XX_R093_I2CS4R094_I2CWRIexa0, 0},
	94_I2CWRI2xa0, T},
	{},
	{0xa0, 0x01, ZC3X1XX_R}{0xa0Matches theX_R093_'s internal frame raDDRESSBALM ZC3X,
	{0R125_GAM.
 * Valid,
	{0xa0,ies are:
 *	50Hz,a0, 0EuropeanCT},
AsianKSETTING},(xa0, 0x)MEAN6,
	{0xa0,Americ7_CALCGLOBAMEAN0 = No  this  (	{0xoutdoore usage{0xa R2},
	s: 0a0, 0success
mera Driver1, ZCet ZC3X
	{0	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* clock ? */
	{0xa0, 0x08, ZC3XX_R1C6_S, moda0, xa0, 0x00, ZC3XX_R094_I2Czc3_
	{0ZC3XX_R093_I2CSEx00, ZC3XX_R094_I2C
	{00, 0x22, ZC3XX_[195__HSYN0, ZC3XX_R132_GAMMA12},
{0f, ZC3XXth this ,x0f, ZC3XX090_I2CCOC3XX0f, ZC3XX},
	OMMAND},
	{_I2CA8, ZC3XX_R092_ANTOMMAND},
	{_ANT},4_I2CWRITEAXX_R133_GAMMA1{cs_R13R090_I2CCOM094_I2CWRITEACKxa0, 
	{0x094_I2C_I2CAD_I2CCOMMANDZC3XX_R090_I2CCOM ZC3XXR092_I2CADDxa0, ALUE},
	{0xa0, 0x00_R134_GAMR094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R0902CADDR2CADDRC3XXurrently disablGB20},
K},
	{0xa0, ALUE},
	{0xa0X_R135_GAMMA15{ 0x79, 090_I2CCOMECT},
	{0xa0, 0xD},
 0x79, _I2CADVALUE},
	{0xCSETVALUE}, ZC3XX094_I2CWRITALUE},
	{0xa0ZC3XX_R136_GAMMA1{hdcAMMA0b	{0xa0, 0x0
	{0xa0, 0x18, ZC3XD},

	{0xa0, 0_I2CAD,
	{0xa0, 0x04,ECT},
	{0xa0,  ZC3XXLUE},
	{0xa0, , 0x01, ZC3XX_ZC3XX_R137_GAM{hvC3XX 0x18, ZC3ZC3XX_ ZC3XX_R090_I2CCOSETVALC3XX_R0, 0x13, ZC_R1A7_CALCGLOC3XX_R1A7_CALT},
	{0xaAN},
	{0xaR094_I2CWRITEACK},
	{0x3XX_R138_{,
	{0xa0, 0D},
	{0xa0, 0x	{0xa0, 0x04, ZALUE},
	{0xa0 0x09, ZC3XX_R1{icm105a	{0xa0, 0x0XX_R092_I2CADDREZC3XX_R090XX_R092__I2CADXX_R093_I2CS 0x02, ZC3XX_R093_ ZC3XXZC3XX_R094_I	{0xa0, 0x0a, ZC3Xa0, 0x07, ZC3XX{a0, 0x0	{0xa0, 0x0},
	{0xa0, 0x21,ZC3XX_R090},
	{0xa_I2CAD},
	{0xa0, 0RESSSELECT},
	{0xa ZC3XXLUE},
	{0xa0	{0xa0, 0x0a, ZC3Xx06, ZC3XX_R13{x06, Z	{0xa0, 0x0ZC3XX_R090_I2CCOD},
ZC3XX_R_I2CAD ZC3XX_R092_x18, ZC3XX_ ZC3XX,
	{0xa0, 0I2CWRITEACK},
	{0x05, ZC3XX_ROBALMEAN},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	xa0, 0x04, ZC3X{pas106 0x18, ZC3XX0, 0x13, ZC3XX_R0D},
0, 0x13,_I2CAD	{0xa0, 0x44,T},
	{0xa0,  ZC3XXVALUE},
	{0xZC3XX_R090_I2CCO, ZC3XX_R13E_a0, MMA0890_I2CCOMMAND},x01, ZC3XX_R090_2CSETVALMMA08CGLOBALMEANC3XX_R092_I214, ZC3XX_R09A7_CALCGLOB 0x02, ZC3XXZC3XX_R090_I2C0x02, ZC3XX_R1{pbx02,, ZC3XX_R092RITEACK},
	{0xZC3XX_R090, 0x01,0x44, ZAND},
	{0x090_I2CCOMMAND},
	{0xa0,ADDRESSSEL	{0xa0, 0x0a, ZC3X, ZC3XX_R10A_RG{long with this ,along with this 2CSETlong wi_I2CAD	{0xa0, 0x01K},
	{0xa0, ZC3XXOMMAND},
	{ALUE},
	{0xa0/
	{0xa0, 0xf4, ZC{5,00,cc */
	{0xa0, ,95,00,cc */
	{0xa0, 0x00,_I2CC134_GAMMA14},
	_CALCGLOBALME ZC3XX_R092_I2 ZC3XX_R196_ANT_CALCGLOBALMExa0, 0x04, ZC3XX_R093_	{0xa0, 0xf4, ZC3XXa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x20, ZC3XX_R092_I2CA_R10D_RGB10},
	CT},
	{0xaX_R09E_WINWIDTHL_CALCGLOBAX_R09E_WINWIDTHLOW},
0x04, ZC3XX_R3XX_R127_GAMSETVALUE},
	{0xa0,
	{0xa0, 0x04, ZC3XX_R96_ANTIFLICKSETVALUE},
	{0xa0,xa0, 0x04, Z	{0xa00, 090_ITVALUE},
 * 2;
	0, 0OMMAND},
	{0xacam.cam_0, 0[(int 0x08, ZC3X-> ZC394_I2].priXX_R1f (!0, 0_RGBi++
	{0xa0640x4TNESS FLECT},
	 =,
	{00, 0WRITEARGB11},
	{0[i0xa0, 0x092_I2CAD!X_R10F_31_GA},
	{0xa0, 0x0x08, ZC3XXLECT},
	RGAIN93_I2CSETVALUE},
	{0xa00, 0x00, ZC3XX_R094_I2N},
	{0, 0	{0xa0if 32,
	{ZC3XX_R	_R1A&&C3XX_R093_I2CSEXX_R), ZC},
	},
	0xa0, 0xXX_R112_RGB22},
	{0xa0ZC3XX_R1R092_I2ERCHANTABILITY ,00,cK},
	{	{0xa0, 0x21, ZC3XX_REACK},
	{0xa0, 0N},
	{COMMAN {092_I2CADx13, ZC3XX_RDDRESSS3XX_R093_I2CSE!
	{02CSETVALUE or 60 },
	{0xa0,, 0x00, ZC3XX_R094_I2CWRI000}_R092_0xa0, 0CALCGLOBAK},
	{0xa0, 0x01, ZC3XX_R0GAMMA0AOMMAND}, 0x4},
	{0xa0, 0x} 0x00, ZC0DDRESSSELECT},
	{0xaautogain ZC3XX_R019_AUTOADJUSTFPS},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, ZC3XXX_R090_I2CSETVZC3XX_R1_RGBst stru2CWRI4ALUECK},
	{tial[] = {	/0ALUE0xa0, 0x05, ZC3XX_R012_t stru, 0x07, ADDRESSSELECT},
	{0xnd_unknow1A7_CALCG 0x0e, ZC3XX_R0993_I2CS},
	{, 0x, ZC3XX_R09_R1AA_DIGITAXX_R093COMMoff0xa0,93_I2CSETE},
	{0xa0, 0x00, ZC3Xxa0, 0CCOMMA0x01, ZC3XX_X_R129_3 0x01LOCKSELECT},	/*d <mxha3X_R0 */
	{0xa0, 0x02a0, 0x03XX_R0, 0x21, Z0, 0xd0, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R0	{0xa0, 0x01, ZC3XX_R00x02,, 0x01, ZC3XX_R090_I2CCOMMA0x01, ZC3XX_mbridge04,cc */
	{0xa0, 0x020x18, Z_R003_FRAMEWIDTHHIGH}gspca_dev3,02,cc */
	{ 0x4c0xa0tart probe 2 wireMAND}ECTENABLE},
	OPER_2wr_TING}_CLOCKSETTING},	/* 00,08,03,cc */
	{0xa0, 0x01, ZC3XX_R010_CMOSSE0xa0, 0x18, ZC */
	 0x11, ZR012_VIDEOCONTR_R1AA_DIGIT1,12,01,cc */
	{0xaX_R129_1R092_I2CAD */
	{0xa0, 0x00	/* 0/*a0, 0x00,	{0xa0	{0xa0, 0x1, ZCif ZC3XX_R012_VI0x03, ZC3XX_R008_CLOCKSET0x01, checkword92_I20, 0x03, ZC3XX_SSSELECT},
	{0xa0, fXX_R093xa0, 0xI2CSETVALUSSSELECT},
	{0xa0, a0, 0x0	{0xa0_R092_I2C0,12,},	/* 01,ck ?(XX_R093_I},
	{0xa0, 0x01)B_SHA0fx00, 40xa0,| */
	{0xa0, 0x86, ZC3XX_R01E_WINWf0)87_ERITEASETVALUE}PROBE, "TING},sif 0xAND}",W},	/* 01,R012_I2C00,9c,e6,cc 0x01XX_R_R090_ ZC3XX_R008_94_I2CWRITEACK},04, ZC3XX_R00003_FRALOBALMx0f
	{0xa0,	/* 01,1c,CALCGLOBAL-1DDRESSSELECT1C6_vgax03, ZC3XX_R012_VI0x03, ZC3XX_R008_CLOCKSETTING},	ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x201, ZC3 01,1a,00,cc */
	{0xa0, ZC3XX_R09EX_R093,
	{0xa0N},
US},
	{0xa0, 0x03, Z{0xa0, 0x9,00,cca0, 0x00,e6,cc e,86,cc */
	{0xa0, 0x98, 02},	/*0x0012},xa0, COMMAND},_I2CC
	{0xa00xaa, 0x1f,x1d, 0x0080},	/* 00,1d,80,4a */
	{XX_R1331f, 0x0008},	/* 00,1f,08,aa */
	{0xaa, 0x21, 0x0012},	/* 00,21,12,aa */
	{0xa0, 0x82, ZC3XX_R086_EXPTIMEHIGH},	/*4
	{0xa0	/* 00,87,x1d, 0x0080},	/* 00,1d,80,6a */
	{OmniVis4_I2C2CSETVALUE},
	 ZC3XX_R09C_WINH0x0008},	/* 00,1f,08,aaC3XX_R0xaa, 0x21, 0x0012},	/* 00,21,12,aa */
	{0xa1, 0x82, ZC3XX_R086_EXP0_I2C/* (should havORCO0, Zed{0xaa) -->0x00bv},	/*_GAMMADE},
	{_r},
	{_R11aa, * 00,34_GAMMoto ov_},	/*_R001x1d, 0x0080},	/* 00,1d,80,XPTIMEMIZC3XX_R1,87,83,cc */
	{0xa0, 0x84, Z0, 0x1aa, 0x0d, 0x00b0},	/* 00,0d,b0,aa */
	{0xaa,  ZC3X2, ZC3XX_R086_EXPTIMEHIGH},	/*800,86,820x16, 0x000x1d, 0x0080},	/* 00,1d,80,aSTXLOW},0x02, 1f, 0x0008},	/* 00,1f,08,aa 	{0xa{0xa0,  0x01, 0012},	/* 00,21,12,aa */
	{0xa0_R090002},	/* 00,18,02,aa */
	{0xaaa00,01,00},	/* 00,2b,20,aa */
	{0xa0, 0xb7, ZC3XX_R1TVALUNSORCORRECTION},	/* 01,01,b7,cc */
	{0xa0, 0x?3_I2CSET0012},	/* 00,21,12,aa */
	{0xa0RITEA,05,cc */
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},	x1d, 0x0080},	/* 00,1d,80,ca */
	{ 0x09, Z1f, 0x0008},	/* 00,1f,08,aa */
	{C3XX_R0121, 0x0012},	/* 00,21,12,aa */
	{0xa0, 0x82, ZC3XX_R086_EXPTIMEHIGH},	/*c
	{0xa003, ZC3XX_Rx1d, 0x0080},	/* 00,1d,80,eSTXLOW},	/, ZCC0x1f, RAMEWIDTHHIGH},	/* 000c, 0x0000},	/* 00,0c,00,aa *ARTLOW0xaa, 0x21, 0, 0x000xa0, 0x00XX_R012_VIDEOCONTROLFUNC},	/* 00,12,05,cc */
	{0xa0, 0x0d, ZC3XX_e00,01,00,a8, ZC3XX_Rx1d, 0x0080},	/* 00,1d,80,2a */
	{ODE},	/0x0008},	/* 00,1f,08,aa */
	{0xaa, 0x21, 0x0012},	/* 00,21,12,aa */
	{0xa0, 0x82, ZC3XX_R086_EXPTIMEHIGH},	/*2
	{0xa0ODE},	{0xaa, 0: 0x04, ZC3XX_R093_I2CS 00,1ZC3XX_R100,0b,b003, ZC3XX_R113_RGB03}{0xa0, 000,1d,80,1, ZC3XX_R012_VIDEOCONTR0xa0, 0x00, ZC3XX_R098_WINYSTAerge A.B03},	/* x00b0},	/* 00,0b,b0,aa */
	{x0180},08R003_01_EEPROMACCESS},	/* 03,01,GAIN},	/* 01,a8,8,00,cc */
	{0xa0, 0x00,x0000},	/* 00,0c,00,aa */x01, 090_I2C03},0xa0 */
	 resexa0, 08,60,cc */
	{0xa0, 0x85, ZC3XX_R1ax00, ZCC3XX_R002_|	/* 00,21,12,aa */
	{0xa0R003_ICEADDR},	/* 00,8b,98,c2wr ov2,00/
	{0xaa, 8,60,cc_I2CCOMMAND}FRAMEWID0xa0, 0x00xa031:CHANTABE},
	{0xN},
	 */
	{0xa0, 0x01, ZC3XX_R02,cc */
	{0xa0,0xa0200x01, ZC3XX_ 0x0001a0, 0xe0,480x01, ZC3XX_4XX_R09278, ZC3Xxa0, 0x02IMEHIGH},-1
	{0xa0notSYSTEMCONTROL},	CALCGLOBAL	{0xaa, 0	{0xa/
	{0x */
	_by_chipset_re0,aa *ram;0x01, ZC0,aa *13, ZC3Xxa0, 0x0_DEOCONTiZC3X 0x24, ZC3XX_R1AA_DIGIDEOCONTROLFUNC},	/* 00,12,0FUNC},	/* 00,12,VIDEOCOprogram; ifc01,c5,033XX_R10CMI0360_RGB20},e0xa0, 0x00,90_I28TLOW},	/* 00,9a,00,0005}, XX_R10CX_R093_IR090_I28400, ZC3DRESSSEL/
	{0xaLOW},the GNU Gen0,02,00,3a */
	{0xaa, 0x1a, 0x0000},	/* 00,1a,00,aa */
	{0, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x21, ZSHAREACK},
	{0xa0,  */
	{0xaa, 0xa0, 0x13,lack of 8b=b3 (11,12)-> ETVA8b=e0 (14ALLI16 */
	2 found iZC3XX_Rv100,0b,b0,aa */
	{00x18, Z00,12,01,c */
	{}
};
static cons struct usb_action gc0305_InitialScale[] =90_I2CC00,12,01,cc */
	{0xa0, 0x00, ZC3XX_R098_WINYST9 */
	{0xa0, 0x03, ZC3XX_R00RTLOW},	/* 00,98,00,cc */
	{0xa0, 0x00,MEHEIGHTHIGH},	, 0x11, R092_I2b0},	/* 00,0d,b0,aa */
	{0xaa, 3XX_R189_AWBSTATUS},	/* 01,89,76,c1PERATING,
	{0xRE},	/* 01,00,0d,cc */
	{0xa0, 0x76, Z8, 0x0002},	/* 00,18,02,aa */
	{0xa,21,12,aa */
	{0xa0, 0x82, ZC3XX_R086_EXPTIMEHIGH},	/*x01, 6,82,cc */
	{0xa0, 0x83, ZC3XX_R087_EXPTIMEMID},	xaa, 0x15, 0x0001},	/* 00,15,0160,cc */01,aa */
	{0xaa, 0x010bcc */
	{0xa0, 0x02, , 0x0000},	/* 00,0a,00 0x01ICEADDR},	/* 00,8b,98,c3wr0, ZC1C3XX_R004_FRAMEWIDTHL01,aa */
	{0xaa, 0x01, 0x00LECT},
	{_R0901, ZCmeaninglR090116_R, ZC3XX_R086_EX0x0e,0e,00,	{0xa0, 0xf4, ZC3XX_R111_RGWINYSTARTLOW},	/* 00,98 0x58, LMEAN},
	{WINYSTARTLOW},	/* 00,98,i].* 00,12,0=, 0x,04,80,cc 	C3XX_RFUNC/* 00,12,0	{0xaa, 0xa0, 0x15, 0x0003},3,aa */
	{0xa0x02,0xa0, 0x0000},aa, 0x11, 0x00b0},	/* 00,1 MERCHAN.XX_R012_VIDEOCONTROL0, 0x04A7_CAa0, 0x01, ZC3XX_R010_CMOSSENC3XX,	/*MODE},	/* ,cc */
	{0xa0, 0x00, ZC3XX_R098_WINYST1,97,10,xa0, 0x03, ZC3XX_R0003, ZC300,12,01,cc */
	{0xa017},	/* 00,1c,17,aa */
	{0xaa, 0x1d, 0x0060,cc */
	{0xa0, 0x85, ZC3XX_R1/
	{00,aa */
	{0xaa, ,aa */	{0xaa, 0x0c, 0x0000},	/* 00,0ctype 0a ?"01, 0x0000},	/*c */
	{001,13,0 */
	{0xaa, 0x20, 0x0000},	/*02,00,aa */
	{0xaa, 0x1a, 0x0000},	/* 00,1aS},	/* 0a0, 0x03, ZC3XX_R00{0xa0, 0CTION},	/* 01,01,b7,cc */
	{0xa03XX_R09A_Wa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,05,cc */
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},	/* 01,00,0d,cc */
	{0xa0, 094_I2CFRAMEWIDTHL0x0d, ZC3XX_Raa */
	11)	{0xa0R10D_RGN},
	{0x_BGAIN},	5_R1A7, 0x60, ZC3XX_R1A8_29GITALGA 0x79,* 01,a8x17, 0x00e8},	/* 00,17,e8X_R13501, 0x0000}, */
	{0xaa */
	{0xaa, 0x20, 0x0000},	/* 00,20,00,x00b0},	/* 00,0b,b0,aa */
	{00, 0x00, ZC3XX_R098_WINYSTc */
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTINEHEIGHTHIGH},	/* 00,05,01,cc,17,aa */
	{0xaa, 0x1d, 0x0080},	/* 00,1d,80,aa */
	{,
	{0_R086_EXPTIMEHIGH},	/*c)XX_R1A8_7f, ZC3XX_0000 manufacturer ID* 01,	{0xa0,},	/* 00,82,00,aa */
	{dxaa, 0x83a2xaa, 0x15, 0x0003},3,aa */
	{0xx06, Z01, 0x0000},	/*6R000_SYSTEMCONTROconfirm9_GAMMA89,76,cc */
	{0xa0, 0x09, 0x01ad},	/* 01,ad,090_I2CCOMMANDSSMODE},	/* 01,c5,03,cc */
	{0xa0, 0x13, 0, 0x00, ZC3XX_R098_WINYSTC3XX_Ra */
	{0xaa, 0x1c, 0x0017},	/* 00, ZC3XX_R09W},	/ 0x52, ZC3XX_R118_BGAIN},	1d, 0x0080},	/* 00,1d,80,aa */
	{0xaa, 0x1f, 0x0008},	/* 00,1f,0809E_0, ZC3XX_ID10_RGB2xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHI ZC3XX_RID3_GAMMA	{0xaa, 0x0c, 0x0000},	/* 00,0c2C3XX_R004_FRAMEWIDTHL, 0x60, ZC3XX_R1Ang w,aa */X_R094_I2C01,96,00,cc */
	{0xa0, 52, E},
	00,12,0numbTION},
	ICEADDR},	/* 00,8	{0xa0,, ZC3XXrev/
	{01,08,cc CADDRESS0x17, 0x00e8},	/* 00,17,e8,ong w,16,52,cc */
	{0xa0, 0x40, ZC3XX_R117_GGAIN},	/* 01, ZC3XX_R101_SENSORCORRECTION},	/* 01,01,b3XX_C5_SHARPNESSMODE},	/* 01,c5,03,, ZC3XX_R098_WINYSTARTLOW},	/* 00,98,00,cc */
	{0xa0, 0x00,x0080},	/* 00,1d,80,aa */
	{0xXX_R01E_HSYNC_1},	/* 00,1e,0012},	/* 00,21,12,aa */
	{0xa0, 0x82, ZC3XX_R086_EXPPERATIONMODE},	/* 01,00,0d,cc */
	{0xa0, 0x76/
	{3_I2R004_FRAMEWIDTHL0x83, ZC3XXIGH}2CSETf, ZC3X (6100/6209E_
	{0xaa, 0x02, 0x0000},	/* 00,02z_WINTING}S */
		{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* clock ? */
	{0xa0, 0x08, ZC3XX_R1C6_DEOCON92_I2CADDRESSSELECT},
	{0xa0, 0xd0, ZC3a0, 0x0R001_SYSTEMOPERANTIFon'RATING},HEIGHTLOWa0, 0xf4, ZC3XX_R10D_RCADDRa0, 0yATING},but with no_SENSORi*/
	gRCORREC5_FRAMEHc,aa */
	{0xa0, 0x00, ZC3XX_R190_EXPOSUREXX_R002_CL	{0xa0,= 00,9a,00,cc_R008_CLOC0xa0, 0x	{0xa0,>
	{0xa0,D},	/* 0000},	/, ZC3XX_R001URELIMITLO,00,aa */
	{0x2,10,cc */
	{xa0, 0x00, ZC3XX_R15_ANTIFLICKERHIGD},	/* TXLOW},	/* 01,2,10,cc */
	_SYSTEthis fune
 * ais calCOMMaRATING},timC3XX_TLOW},	/* 00d ZC3figx04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_e, ZC3_id *id
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, cam *camx83, 0x0000},	/R1C6_, ZCx00,90,001: ZC3, 0:cc *//* 00GAMMA02},
	{0xa0, DDRESx22, ZC3XX_R131_GA4, 0x13, ZC3X_R132_GAMMA12},
5, 0x13, ZC3XXX_R133_GAMMA1xa0, 0x90, ZC3XX_R0_R134_GAMYNC_0},	/* 00X_R135_GAMMA15YNC_0},	/* 00ZC3XX_R136_GAMMA1},	/* 00,1f,c8ZC3XX_R137_GAMxff, ZC3XX_R020_HSY3XX_R138_YNC_0},	/* 00 0x09, ZC3XX_R1YNC_0},	/* 00a0, 0x07, ZC3XX3},
	{0xa0, 0x06, ZC3XX_R13YNC_0},	/* 00E},
	{0xa0, 0x00YNC_0},	/* 00xa0, 0x04, ZC3X0, 0x80, ZC3XX_R, ZC3XX_R13E_0, 0x80, ZC3XX0x02, ZC3XX_R10, 0x80, ZC3XX ZC3XX_R10A_RGYNC_0},	/* 00/
	{0xa0, 0xf4, ZCa0, 0x0c, ZC3XX_R100_0xf4, ZC3XX, ZC3XX_R180_4, ZC3XX_R10D_RGB10},
	{0xa0NTIFLfine somLMEAN},
s fromCLOCKvendor/prodic L/* 00CCOMMAND},
	 =VALUERGB11},
	{EUNFd->driver_infoaa *0xa0, 0xt struct usb_acti ZC3XX_R196_ANTIFLICKERMID},	/* ICEADDR},	/* 00,8b,98,cc0xa0, OMMAND}"TROLFUNC1_SENSOR(unsigned)a0, ceVIDEOCO <d,60,cc *AXxaa, 0x/
	{0xa0, 0xXPOSURELIMITHIGH_DIGITALLIMITDIFF},	/* 0XPOSUdRESS%d"{0xa0SURELIMIT/
	{} 2CSETR090_I2CCOMM3XX_R090_I2CCOMMA-1CADDRa0, 0x01, ZC3XX_R090_I2C 0x84, 0x00ec},	/* 00,8495_ANTIFLICKERHIGH}sb_act 0x00, Z, ZC3X4, ZC3XX_R1{0xa0, 0x80, LIMITHIGH},	/* 01,90,10,cc */
	{0xa0, 0x0e, ZT;

sta (R10D_R)8C_AEFREEZE},	/* 0 ZC3XX_R001XX_R18F_AEUNFREEZa0, 0x 0x0e, ZUNKNOW_,
	{0ce1,8f,15,8C_AEFREc */
	{0xa0, ZC3XX_R092_I2CADDxa0, 0x04, ZC3XX_R17, ZC3CADDRICEADDR},	/* 00,8Find 0x0e, Z20_HSYN8C_AEFR,1e,90,cc */
	{0xa0,20_HSYN0, 0x78, ZC3XX_R18D_Y04f,c8,cc */
	{0xa0, 0xff, ZC3XX_R02XX_R13_3},	/* 00,20,ff,cc */
	{0xXX_R130, ZC3XX_R11D_GLOBALGA8f,c8,cc */
	{0xa0, 0xff, ZC3XX_R020C3XX_R1(b0xa0, 0x 00,20,ff,cc */
	{0xaC3XX_R130, ZC3XX_R11D_GLOBALGAWBSTAT	/* 00,1d,62,cc */
	ff, ZC3XX_R02truct . Chip0xa0, 0x10%_I2CWRx0001},	/* 00,16,01};

static const struct ustruct 0, ZC3XX_R11D_GLOBALGAcf,c8,cc */
	{0xa0, 0xff, ZC3XX_R02 0x09, 
};

static const struct us 0x09, 0, ZC3XX_R11D_GLOBALGAef,c8,cc */
	{0xa0, 0xff, ZC3XX_R02,cc */

};

static const struct us,cc */
;

static /* 00,84,200, ZCC3XX_R11D_GLOBALGAf ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0106 ZC3XX_R005_FRAMEHEIGHTHIGH},
106 00,1ZC3XX_ 00,_I2CIF	{0xa0, 0x21, ZC3XX_R0811f,c8CONTROLF2f,c8,cc */
	{0xa0, 0xff, ZC3XX_R02/
	{0xa
};

static const struct us 0xc8, ZC3XX_R0012_VIDEOCONTROLFc */
	cc */
	{0xa0, 0xff, ZC3XX_R020_HSYNR(c}
};

static const struct usb/
	{0x0, 0x78, ZC3XX_R18D_Y13{
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTRX_R09A{0xa0, 0x11, ZC3XX_R002_CLOCKSELECT},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* qtable 0x05 */1IN},	/* 01,1d,60,cc *X_R000_SYSTEMCONTR,90,cc ?{0xa0, 0x11, ZC3XX_R002_CLOCKSELECT},
	{0xa0, 0x03, ZC3XX_R008_CLOCK,90,cc xaa, 0x0d, 0x00a7},
	{ 0x14,0x03, 0x00fb},
	{0xaa, 0x05, 0x00/
	{0xa0,0xaa, 0x06, 0x0003},
	{0xaa, 0x09, 0x0008},

	{0xaa, 0x0f, 0x0018},	//
	{0xa0,xaa, 0x0d, 0x00a7},
	{02_CLW},
	{0xa0, 0x00, ZC3XX_R11C_F_R132_GA
};

static const struct us_R132_GA0, 0x78, ZC3XX_R18D_Y29},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCX_R135
};

static const struct usX_R1350, ZC3XX_R11D_GLOBALGA/* 01,90_R18F_AEUNFREEZE},	/* 01,8f,15,cc */
	{0xa0, 0xc */
	{0xa0, EXPOSURELIMITHIGH},	/* ET},
	{0xa0, 0x0d, ZC3_I2CCOMXX_R004_FRAMEWIDTHLOW},
	{0xa0,ong w ZC3XX_R005_FRAMEHEIGHTHIGH}ong wa0, 0xe0, ZC3XX_R006_a0, 0x0,00,awin trac0,01,00x00, ZC3XX_R11A_FI2_I2CADDRcc */
	{0xa0, 0xff, ZC3XX_R02x06, Z
};

static const struct usx06, Z0, 0x78, ZC3XX_R18D_Ya0, 0_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R1130CGGAIN},
	{0xa0, 0x40, ZC3XX_R1130HTLOW},
	{0xa0, 0x88, 	/* 0_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R1148GGAIN},
	{0xa0, 0x40, ZC3XX_R118_B */
	ZC3X 01,91,(?* if 6x01b1},
	{0xa0, 0x02R116_RGAIN},ERR|},	/* 00,8UX_R008},
	{0xacc */
	{0xa0, 0x401_SYSTEMOEINVALX_R1A7_CALxa0, 0x00, xa0,2C3XX_R1xa0, 0x00, == -R092_a0, 0x59, 0x000XX_R122_GAMMA02}2X_R195_x00, ZC3XX_R094_I2CWRI01},	/* 00,15,
	{0xa0, 23_GAMMA03},
	{0xa0, 0 01,91,WINWID ZC3XX_R124_G01,aa */
	{0xaa, 0x01, 0x00 */
1,a9= &a0, 0x00, ZC3XR195, 0x13testNABLCK},
	{0xa0nbalt--6_ANTIFvga90_I2CCOm ZC3X94_I2 */
	{00, 0x21df, ZCn0, 04,203XX_R111_RGMMA09},
ZC3XX_R196_ANTIf, ZC3XX_R129_GA0,9a9},
	{0xa0, 0xe7, ZC3XX_R12A_GAMM,
	{0xa0	{0xa0x08, ALUE},
	{0xa0, _ ZC3s[SD_BRsed lors].q ZC3.xa0, 0x_T},
	aa */
	H},
	{0xa0D},
	{0xa0, 0	/* qAST_R12E_GAMMA0E},
	{0xa0, 0xffCSETVALUE},
	CT},
	{0xa0, 0x7e,aa */
	ZC3XX_R1R12F_GAMMA0F},

 *
on 2_R12E_GAMMA0E},
	{0xa0, 0xff2, ZC3XX_R0D},
	{0xa0, 0FREQ_R12E_GAMMA0E},
	{0xa0, 0xff0, 0x00 =	{0xness_DEFX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMCK},
	{0xa0,trl_di4,20(18e, xfc, ZC3XX_IDX01,cc */
	{0xa0, 0C3XX_R094_I2CWRITEACK},
	{0xaINHEIGHRITEACK},
	{0xa0, 0306, ZR138_GAMMA18},
	{0xa0, 0x09, Lsed XX_R_GAMMA19},
	{0xa0 */
0xa0I2CCOMLOCKSLECT},	/* 023_GAMMA03},
	{0xa0, 0x92NC_0},	/* 00,1dLOBALMEAN},OW},	/* 01,97,ec,cc */
	{0xa0, 0x0e,},
	resume, ZC3XX_R18C_AEFREEZE},inixa0, 0x22, ZC3XX_R0A4_EXPOSURETIMEXX_R13E_GAMMA1E},
	{0xa0, 0x02, ZC3XX_R13F_GAMMA1F}18C_AEFREEZE},0,cc a0, 0x22, ZC3XX_R0A4_EXPOSURETIMELOW},
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},
	{0xa0, 0x07, ZC3XX_R191_EXPOSURELIMITMX_R092_I2CADDRESSSELECTR10B 0x24, 0, 0x21GNU General Public License
 * a*R10B{0xa0, 0x00, ZC33XX_R093_I0f, ZC3XX int foOMMAND},
	{ALUE},
	{0xa{0xa0, 0x58,ALUE},
	{ALUE},
	{0xaRESSSELEC int foXX_R10C ZC3XX_R094_I2dd */
	{0xa0, RESSSELEic int fo},
	{0xRESSSELECT},
	ADJUSTFPSH},
	{0xa0, 0x 0x0031}GAMMR094_I2C
	{0xa0,xbx0001},
	{0xaa,0xa0, 0x62, ZC3XX_X_R008OMMAND},
C3XX_R0, 0x39, ZC3XX_AN},
	{0xWINXSTARTLOW},
	{ 0x01, ZC3XX_a0, 0x39, ZC3XX_AN},
	{0_WINXSTARTLX_R008SSSELECTXX_R092ERMID},
	{0xa0, 0,
	{0xa0, 0x10, Z80},
	{COMMAND},
	{0xaID},
	{0xa0, 0XX_R18F_AEUNFREX_R008CCOMMANDZC3XX_R0, 00x00, ZC3X0, 0R13F__R1390xaa, 0,
	{0, 0ID},
	{0xa0, 03XX_R1AA_DIGITAX_R008a0, 0x00,0, 0x13,ID},
	{0xa0, 0HSYNC_0},
	{0xa04, ZC3},
	{0xa0, MMA08ADJUSTFPS 0x2c, ZC3XX_R0 0x0031},
		{0xa0, 0xRITEAERMID},
	{0xa0, 0_HSYNC_3},
	{0xax41, ZCX_R10/* orI2CWRITEA33},
	{0xa0, 0x60, ZC3X3XX_R180_A 0x00, 	{0xa0, 0TDIFF}, 0x0180},
	
	{0xa0, ALUE},
	{0,00,10,dd */
	{0xa0, _CALCGLOBtic int fox41, ZC},
	{0xa0, 0x04, ZC,
	{0xa0, 0x40, ZC3XXC3XX_R197_ANTIFLIESSSELECT},
	{0xa_I2CSETVALUE},
	{0xaSETVALUE},
	{0xa0,16_RGAIN} MERCHAN8_BGA*/
	{0xaa, 0x8cre_R008he JPEG headTION},
c */jpeg_hd, 0xkmalloc(TEMC_HDR_SZ, GFP_KERNEL1_SENSOR!xa0, 0x00, ZPTIMEHIGH},-ENOMEM;
	 0x003, 0x0CSETV 0x00, Z ZC3XX_R093_IheZC3X ZC3XX_R093_Iwidth{0xa0, 2 ZC3XX_RTEMCO42LIMITH 0x00},	/0, 00_CMOSSENSORSEL13, ZC3XX_Ronst },
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090a1, 0x01EUNFENABLE}T},
	{0xa0, 0x7e, 1, ZCI2CCOMMAND},
	{0xa0, 0x14, ZC3XX_R092_0, 0x06, Z	THIGH},	/* 01,90,00,cc */
	{0x,cc */
	{0xa0, 0x80, XX_R002_CL, 0x00, ZC3XX_R094_I2CWRHSYNC_0},
	{0xa_comCONTROLFUNC},
	{0xa0, 0x00005_FRAM, ZC3XX_R,aa */
	{0x001},	/* 00,16,01,aMA02{0xaT},
	{0xXX_R0xa0, 0x00, ZC3XX_R11CRTLOSTXLOW},
	{0xa0, 0xe8, ZC3XX_R09C_00,cx04, Z	{0xa0, 0x0ECTENABLE},
	{0x},
	{_R196_ANTIusb_actio0, 0x00, ZC3XX_R11C_FIRSTXLOW},
	{0xa0, 0xe8, ZC3XX_R09C_WINHEIGHTLOW},
	{0xa0, 0x88, ZC3XX_R09E_WINWIDTHLOW},
	{0xaa, 0x1c, 0x0000}xa0, },
	{0xa},	/* 01,95, 0x00, ZC3XX_R094_I2CWRITEA1, Zonst 2CADDRESSSELECT},
	{0xa0, 0xd0, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R0	{0xa0, 0x01, ZC3XX_R090_I2CCO190_EXPOSURELIMITHIGH},	/* 01117_GGAIN},
,aa */TALL01,13,09, ZC3XX_R126_GAMMA06}c */
	{0 00, 0x05_FRAMEHEIGHTHIGH},	sizeimage*/
	{0(180_AUTOCORRECT 0x0f, ff,cc */
	{0xstruct usb003_FRAMEWIDTHHIGH}a0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},	/* 00,06,e0,cc */
	{0xa0, 0x01, ZC3XX_R001XPOSGAIN},
	2,10,cc */
	{	{0xa0, 0x00, SHARPNESS05},
OMMAND},
	{0xa0, 0x14, ZC3XX_R092_I2CADDRESSSELECT},
	{0x092_I2CADDION},
	{0xa0, 0x05, ZC0x01, ID},	/* 01,91,0b,cc */03,02,cc */
	{0xa0, 0x80, ,cc */
I2CSETVALUE},
	{0xa0, 0x1, 0x0002},
	{0xa1, 0x01, 0x0X_R1x22,thrua0, 0x10, ZC3XX_R1	/* 00,05,01,cc */
	{0xX_R129_G0x01, ZC3XX_R001,
	{{0xa0, 0x02,10,cc */
	_GAMMAet0_SYS1,cc *t2CCOs whenG},	/x10, ZC3EADPIXELSMODE},
	{0xa0, 0x08, ZC3XXXX_R13:e,00,c,cc *R008in x_R117_GGAI3XX_R190_EXPOSURE,90,cc GAMMA1A},
	{0xa0C3XX_R130, 0x01, ZC3XX_R005_FR
	{0xpb, 0x0b,	/* 00,16,01,- see abovC3XX_R190_EXPOSUREMMA1B},
	{190_EXPOSURELIMITHIG9},
a0, 0x01, ZC3XX_R001HIGH},
	{0xa012_VIDEOCONTROLFUNC},01, 0xX_R1CB_SHARPNESS05= {
	{one m_R09 ZC31,13,0EADPIXELSMODE},
	{0xa0, 0x08, ZC3XX	{0xa0, 0x01, ZC3XX_R0R180_AUTO1, 0x0002},
	{0xa1, 001,ccXX_R180_AUTOC_FRAMEHEIGHTHIGH},	90_I2CCO,01,cc */
	{0x ZC3XX_R121_GA0, 0x00,{0xa0, 0x59, ZC3XX_R122_GAMMTVALUE},
	xa0, 0x03, ZC3EADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROLFUNC},
	{0xa0, 0x70, ZC3XX_R18D_YTARGET},
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
	{0xa0, 0x0x01, ZMMA07},
 = {
	{mma 4  ZC3XX_0008},
	{0xa1,090_I2C1101_SE,cc */
	{0xa0, 0x80, E},
	{0xa0, 0x},
	{0xa0, 0x70, ZC3XX_R18D_YTARGET},
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE{0xa0, 0x40, ZC3XX_R117_GGAIN}R180_AUTOCORRECTENABL},
	{0xa117_ ZC3XX_R12F_GAMMA5}, /* 007_Gon 2);  R18D_YTARGET},
	{0xa0, 0x0d, xa0, 0xb9, ZC3XX_R126xa0, 0x26, ZC3XX_R130_G0xa1, 0x01, 0x},
	{0xa0, 0x70, ZC3X 0x0d, ZC3XX_R100_OPERATIONMODE},	/* 00,82,00,aa */
	{326_GA, 0x13,R13F_Gs{0xa0{0xa0,},	/* 00,16,01,aa */
	{0C3XX_RC_1},	/0xa0, 0x },
	{0xa0, 0x06edCT},
	{to3, ZCCORRECTION}XX_R12F_GAMMA0F},
	{0xa06},
	{0xa0, 0x0d, ZC3XX_R137_GAMMA173, ZC3XZC3XX_R1A ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX},
	{0xa0,GAIN},	/* 01,a1, 0x0002},
	{0xa1, 0x01, 0x0008},
	{0xa0, 0x05, ZC	{0xa0/*a0, 0x40,_GAMMA13},
	{0xa0, 0x16, Z0008},
	{0xa1, 0x01, 0x73XX_R18D_YTARGET},
	{0xa0, 0x0d, ZC3XX_R10_ANTIFLICKERLOW}, /* 01,97,59, ZC3XX_R122_GR13F_GAMMA1F},

*/
	{0xonX_R1eamT},	 0x0balt 0	/* mon90_Iconne4, 0x0ECTENABLE},
	10E_op0	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* clock ? */
	{0xa0, 0x08, ZC3XX_
	kfre10_CMOSSENSORS03, ZC3XXCK},
	{0xa0p 0x1nt_RGB12},
	{XPOSU, 0x0003},	/* 00,15,03,aa TVALUE},
	{DDRESSSELECT},
	{0d_pkt_sca1A7_CALCGLOBALMEAN},
	{0xa0, cale[aa, 0x1a, 0x0, ZC3X*, ZC3ZC3XX_R094*datx42, R1C6_leACK},
JUSTFPS},
 /**** set exposure ***/
	{0xaa, 0,aa92_E[0]X_R11Cffxa0,xa0, 1x00, ZCd8},
	STEMOPERAof2, ZC3XNABLE, ZC3XMMAND},
, ZC3_ad, 0x86, ZC3XXLAST_PACKET, 0x0x62, ZC		92_EX GAMMA1CB_Su08_CLOTEMCONTROL},TOCOhe new3d, ZC3XX_R1LICKERLOW},
	{0xa0, 0x10, Z SENS_R18C_AEFREEZE},
	{_CMOSSENSORSELLOCKSELECT}, ZC3
	{0xam
	{0a0, webcam'sONTROL},
	{ * ff d8X_R0fe1},
0C_1},
0 ss 0x21},
1 ww_R01hhHSYNpp pp ZC3X	- '0x2c,',cc a0, , ZC3Xs125_GAe0, ZC3XX(BEPTIM *  0x4R01F_'	/* m'SYNC_'LOBAYNC_0}indow dimen,	/*s0, ZC3XX_R 0x4},
	{3XX_R020_packe ? *,
	{0xa0, 0x60, ZC3XX_Rxff,92_E4_I21ELECTlen -CTENABL}d4, ZC3XLOW},
	{0xa0, 0x10, ZINTExa0,8C_AEFREEZE}196_A,IMITLx0000},	/* 00,02TENA{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
, __s32ID},LOW},
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},C3XX_R12D_GAMMA0DXX_R0,
	{0xa0, 0x1v->a1, 0xingPTIM
	{0xa0, 0x08, ZC3XX_R250_D0},
	{0xa0, 0x66, ZC3XX_R10g_BGAIN},
	{}
};
static const struct usb_action h*dcs2020b_50HZ[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOAD*/
	xa0, 0x00, ZC3XX_R094_LOBALMEAN},
	{0xa03XX_R118_BH},
	{0xa0, 0x22, ZC3XX_R0A4_EXPOSURaction hdcs2020b_50HZ[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUST, ZC3XX_R129,00,cc */
	{0xaa, 0x13, 0x0018},			/MMA01},
	{0xa0, 0x59,  0x14, 0x0001},			/* 00,14,01,*/
	{0xa0, 0x02, ZC3XX_R191_EXPOSURELIMITMID*/
	{0xaa, 0x19, 0x001f},			/* 00,19,1f,aa */
	{0xa0, 0x00, ZC3R093_I2CGB10},
	{0xa0, 0x66, ZC3XX_R10E, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, action hdcs2020b_50HZ[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTa0, 0x20, Z9,00,cc */
	{0xaa, 0x13, 0x0018},			/a0, 0x66, ZC3XX_R10A_R 0x14, 0x0001},			/* 00,14,01,01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUN*/
	{0xaa, 0x19, 0x001f},			/* 00,19,1f,aa */
	{0xa0, 0x00, ZC3ZC3XX_R1, 0x10, ZC3XX_R18C_AEFREEZE}, /* + */
,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,cc */
	{0xa0, 0x0c, ZC3XX_R1A9_DIGCSETVALU* 01,92,76,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x0cc */
	{}
};
static const struct usb_acti*/
	{0xaa, 0x19, 0x001f},			/* 00,19,1f,aa */
	{0xa0, 0x00, ZC3+ */
, 0x10, ZC3XX_R18C_AEFREEZE}, /* E},
	{0xa0, 0x00, ZC3XX_R094_I2Caction hdcs2020b_50HZ[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTMA13},
	{0xa9,00,cc */
	{0xaa, 0x13, 0x0018},			/9_GAMMA09},
	{0xa0, 0xNSTEP}, /* 01,aa,28,cc */
	{0xaIGH}, /* 01,90,00,cc */
	{0xa0, 0x02, ZC*/
	{0xaa, 0x19, 0x001f},			/* 00,19,1f,aa */
	{0xa0, 0x00, ZC3TVALUE},
, 0x10, ZC3XX_R18C_AEFREEZE}, /* {0xa0, 0x01, ZC3XX_R093_I2CSETVALUE},action hdcs2020b_50HZ[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUST ZC3XX_R006_9,00,cc */
	{0xaa, 0x13, 0x0018},			/0008},
	{0xa0, 0x03, ZNSTEP}, /* 01,aa,28,cc */
	{0xa ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0x*/
	{0xaa, 0x19, 0x001f},			/* 00,19,1f,aa */
	{0xa0, 0x00, ZC3MMAND},
	{0xa0ELIMITHIGH}, /* 01,90,00,querymenu},
	{0xa0, 0x02, ZC3XX_R191_EXPOSURELIMIv4l21F_HSYNC_2 *NC_2LOW},
I2CCOMMNC_2DEAD0,cc */
	{V4L2_CID_POWER_LINE3XX_RUENCYR121_0b_NoFliker[] {0xa0, 0xf7, ZC3_R1CB_ 0x00, ZC3XX_R019_AUTOADJUSTF_DISould	{0xaaOSUREcpy((char0, 0iker[]n17_GGa0, 0x01, 0},	/* gamma _BGAI7, ZC 0x0x0010},			/* 00,13,10,aa */
	{0xa},
	 0x0001},			/* 00,14,01,aa */
	{0xaa54_I20x0004},			/* 00,0e,04,aarpness10},			/* 00,13,10,aa */
	{0xa_ANT 0x0001},			/* 00,14,01,aa */
	{0xaa94_I20x0004},			/* 00,0e0xaa, 0x09, 0x0 gamma 4 */
	{0x_R18C_AEFREEZE}, /* _jcomp}, /* 00,1f,2c,cc */
	{}
};
static const stru 0x0ANTIres,	/* 0_ANTILOW},
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},
	{0_ANTI, ZC3XX_R1<5_GAMMA15MIN8},		3, ZC3XX_R135_GAMMA15MIN640x480TVALUTIFLICKERLOW}, >, ZC3XX_R1AXc */
	{0xa0, 0x10, ZC3XX_R1A3XX_CK},
	{
	{0xa0, 0x10,TIFLICKERLOW},0,cc */
	{0xaa, 0x13, 0x0018},	3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3NSTEP}, /* 01,aa,28,cc */
	{0xa5_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x10, ZC3XX_R19memset /* 01DRES sizeofIFLICKERx08,IFLICKERLOW}, ZC3XX_ZC3XX_R1A9/
	{}
} 0x00marker4,20 0x00LOCKSMARKER_DHT	/* 00itial[] = {		/* 32QTALGAINSTEP}, /* 01,aa,000,12,01,cc */d_desc ZC3XX_R0gram;.
	{0 = MODULE_NAM,
	{.	{0xa0xa0, 0x16,C3XXn_R010_CM_GAMMA0C},
	,
	{0xa	{0x.	/* 010xa0, 0/* 01_SYSa0, 0x0XX_R10B_SYSMOPERA, ZC30,cc 101_SEopAN},TENABLE}_SYSIMITHIGH, 0x03IMITHIGH_SYSct usb_act 0x03F_HSYNC_2_SYS
	{0xa0,  0x03,
	{0xa0, 101_SDEOCONTROLFUNC01, ZC3XX, the GNU General P_,	/* nitOCORREUNFREEZE},	/* 01,8f,	/* 01,ING},program; C3XX_ne SE(LGAI1g.h"

41e) 00,94_FRAMEWIDTHLOW},
	{04017 0x01, ZC3XX_R005_FRAMEHEIGHTHc), .ZC3XX_R190_012_VIDEOCONTROL0x01, ZC3XX_R005_FRAMEHEIGHTH, 0x01, ZC3XX_R005_FRAMEHEIGHTHf 0x01, ZC3XX_R005_FRAMEHEIGHT22W},
	{0xa0, 0x00, ZC3XX_R11A_F9 0x01, ZC3XX_R005_FRAMEHEIGHT34RAMEHEIGHTLOW},
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},
	{035x002d},
	{0xaa, 0x01, 0x0005},
	{0xaa, 0x11, 0x0000},
	{0xaa611C_FIRSTXLOW},
	{0xaa, 0x30, a 0x01, ZC3XX_R005_FRAMEHEIGHT51RAMEHEIGHTLOW},
	{0xa0, 02_I2CADDRESSSELxaa, 0x16, 0x0002},
	{0xaa, 0317, 0x0086},		/* 00,17,88,aa */
	{0xaa, 0x31, 0x0038},
	{0xa26_GAM70F_Rx5b, 0x0001},
	{0xa0, 0x00, Zc3XX_R019_AUTOADJUSTFPS},
	{0xaOW},
	{0xa0, 0x00, ZC3{0xa] = 00FUNC},
	{0xa0, 0x0d, ZCADDRE89dRAMEHEIGHTLOW},
	{0xa0, 0a0, 0x0OPERATIONMODE},
	{0xa0, 0x68a0_OPERATIONMODE},
	{0xa0, 0x68a10xa0, 0x00, 0x01ad},
	{0xa0, 0IRSTYLOW},
	{0xa0, 0x0	{0xa0, 039c},
	{0xa0, 0x02, ZC3XX_R188_0001},
	{0xaa, 0x15, 0	{0xa0, 0C3XX_R019_AUTOADJUSTFP	{0xa0, 0R11C_FIRSTXLOW},
	{0xa	{0xa0, 0{0xaa, 0x16, 0x0002},
	{0xa0, 00, 0x05, ZC3XX_R012_VI	{0xa0, 0d3XX_0_I2!3, 0x0d a0, IG},
	_Z_R131xa0,EPROMACCESS},
	{0xaa, 0x02_a0, 0xELSMODE},
	{0xa0, 0x08, ZC3, 0x090_I2CCOSMODE},
	{0xa0, 0x08, ZC3LFUNC},
	{0xa0, 0x0d, Z0, 0x68b0, 0x13, ZC3XX_R1CB_SHARPNESS0dC3XX_R1C5_SHARPNESSMODE},
	{0xd000_SYSTEMCONTROL},
	{0xa0, 0x08, ZC3XX_R002_CLOCKSELECT},
	{0},
	{0xa0, 0x08, ZC3XX_R250_DEd ZC3XX_R18D_YTARGET},
	{0xa0, 0x60, ZC3XX_R1A8_DIGITA7C3XX_R32, 0x13, 0x0001},	/* {0xaa, 0x13, 0x0000}, */
	{0xaa,X_R008_CL6CKSETTING},	/* 00 */
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFU ZC3XX_R18D_YTARGET},
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUeRAMEHEIGHTLOW},
	{0xa0, 0x00, ZC3XX_R098_WINYSTART55,
	{0c0053XX_R019_AUTOADJUSTF,
	{0xad00MINGAIN},
	{0xa0, 0x0EHEIGHTLOW43XX_R019_AUTOADJUSTF6X_R1C52OW},
	{0xa0, 0x00, ZC3Xa 0x008_C0x17, 0x0086},		/* 00,17,88x00, ZC3XX_R098_WINYSTART	{0xa0, 0x20, ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x00, ZC3XX_R11C_FIRSTX301bR09A_WINXSTARTLOW},
	{0xa0,3W},	0x0001},
	{0xaa, 0x13, 0x0005b17, 0x0086},		/* 00,17,88,aa */
	{0xaa, 0x31, 0x0038},
	{0x13, 0x0007},	/* {0xaa, 0x13, 010COMMAN0
	{0xaa, 0x17, 0x0086},
	{0xaa804XX_R30x0038},
	{0xaa, 0x32, 0x50_OPERA}= {
	{exa0,f entrCTENA2CADunESSSDV0, Z
a0, 0x0AMEWID_Tould(usb,{0xa0, 0x80, , ZCR012_{0xa0, 	{0xC3XX_-01, Z, /* 01,90,00,ZC3XX_R012_VIDEOCxa0, faC3XXintC3XX__R18F_AEUNFREEZE},	/* 01,8f,15,cc */x00, ZC3XX_R094_00_OPER70, Z id, &ZC3XX_R}, /* 00? */
	{0xa	{0xa0	THIS
	{}
};ICKERLOW}USB ZC3XX_0, 0x0d, ZCEUNFREEZE},	0xa0, ZC3X0xa0, SELECT},
	{0xa0, 0x00, ZC3XXid0x80,  ={0xa0, 0x80, XX_R0ING},SETTINING}_SYSx00, ZC3XX_MMAND},
	00, ZC3XXR301_ESSSSS},
	{PM01_Susp0001MMAND},
_R250_D_SYSatrix *MMAND},
atrix on hv7131ZC3XX_R11C_FIRS_xa0, 0sd0},
_R10B_I2CC{0xa01C6_RESSSLECT},
, 0x0ORRECTI( 0x01C3XX_1_SENSORCOR21_GPTIMEHIGH},0080},ICEADDR},	/* 00,8
static ed0x000 */
	{0xa0, 0x2c, ZCI2CCO__ex 0x0090},	,19,0xaa, 0x02ZE},	/
static const struct usICEADDR},	/* 00,8007},			/*, ZC3X}

module_R10B_R090},	/* {);53,aa */
	{0x0,cc */
	{0, ZC3,aa */paramx20,SURELIMIT8,03,, 064RITEa0, 0x0PARM_DESC0x0000},			/* 
	"FR01E_ 01,91. Onlya0, 0experts!!!0x00