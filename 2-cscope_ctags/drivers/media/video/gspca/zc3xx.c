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
	{0xaaZ-Sta3Z-Star/2imicro zc301/z4302p/v130x library
 *	5302p/v2V0x library
 *	6302p/vc10x library
 *		302p/vcchel Xhaard
 *	c302p/vct (C) 2004/moi5Copyrigh5tp://moinejf.f/moi6 Michel Xhaard
 *		mxftwar -Sta This prog V4L2 by Jean-Francois Moine <h4tp://moinejf.free.fr>
7*
 * This program is free software; you can rddistribute it and/or modifyistrit under the tedistribu0e GNU GeZC3XX_R092_I2CADDRESSSELECTnse, oristr(a19your option3 anySETVALUEn.
 *
 * Thist1rogram is d0striCOMMAND0x libra1 that it2p/v9@magic.f * T
 *ram is U Ge software; you can retware Foundation; either version 2 of the Lic6rms ofhe LiGNU General Public License as publSE.  Seehe L
 *f
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOeral Public Licree.fr>
9; without evenhe Liimp30x librarersi	f
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPO8neral Public Licistra2Yng with this program;aaf not, write tohe LiFliedSwarrantistrFoundais d, Inc., 675 Mass Ave,ongg wit tbute 02139, Uttp://moinejf.fram is E_NAME "zc3xx"

#includearrantclude "jpeg; either version 2 See theLic"
		"Serge A. Sxhaard@uedistribute it  the im "
		"Serge A. Suchkov < "gspca.h"

static injpegce_s
MODULE_AUTHOR("MbCA ZC03xx/VC3xx USB Cecng with this program; eNAME "zc3xx"

#include "gspca.h"
#include "jpeg.h"

MODULE_AUTHOR("M

static inrge ree.fr>f"
		"Serge A. Sram is fICENSE("GPL");

static int force_sensor = -1;

#define QUANT_7chkov <Serge.A.S@tochk WARRANRCHANTABILITY o9ree.fr>cif not, write 9e thatby Jean-Francois9pness;
1chel Xhaard
 *1ine QUA6eE.  See the
 9bt betaiITY_MIN 40
#defcDULEge quality */	/* 1ree.fr>
"
		"Serge A. 2ine QUA8chel Xhaard
 *22n different tablesnsorree.fr> SENSOR_ADCM2700ou can  SENSOR_ADCM2700idefine ITY_DEF 5e that8rogram is250_DEADPIXELSMODprograhopetaiat0305 3	/* 301_EEPROMACCESS0x library
 *a V4L2 b SENSOR_AD This7SENSOR_HV718D_YTARGEon.t evenUT ANYpness;
 quality */* Tine SENSOR Cambridg#define SENS1efine SEN_ICM1002 7	/* Tine 0T_VAOCORRECTENABL020b 4
#define40ENSOR_OV7616_RGAINR_PAS106 11SOR_OV763ne SEN7_GS202B 12e SENSOR_PB0330_PB03308_B
#define SEN
/*SENSOR_PO2030 1R_PO2030 1GC3rogram is 08_CLOCKSETTING},	/* clock ? */ENSOR_TAS513A 7
#defineC6_SHARPNESSy Je30C_sharpness+1NSOR_OV78 9 - same vac30 1OV764g_hdr;
};

/* ichel Xharols supported"
		"Serge thatfR_MAX 18
	uBsigned shor5 chip_reviE_DE;-
	u8nt sd_setbr5O2030 1 con0A_RGBrt*gspcamatrix
	u8 *jpeR_PB0f7);
stgtic iBhtnesY; withoupca_dev, __s32 *vaC);
stif not, wsd_setcontrast(strD);
sLr senAX 6atic il);
st32 *vaEl);
s sd_getcontrsetcontrast(strFuct gsd_setc *t forsetcontrast(10);
sfree softwl);
statiautogain(1tructspca_dev *gspcetcontrast(s12 val)getbr20 9130CK 15
#dealuCM2700OR_OV763*#define SE30C 1		/* TOR_PO2030 142
#define s);
static 019setgaADJUSTFPO2030 1HV7131C0d the <htrms of the GNUype Seefree software; yR_PB0330

/* specific wfdev, __ of isensor*gsp_dev, __Serge.A.S@toch1s val) gif not, write  02K 2
#dSENSOR_ADCM2700mma(struct gspcADCM2700 gamma(struct gspcCS2102 ne SENSOR_PB0330t gspcaK e SENSOR_PO2030 1GCnt sd_getb0A3_EXPOSURETIMEHIGH_dev *gspcaOV76revision;(s4d_getf;
statiLOWtrast(struct g);
static i90tic strucLIMITc int sd_setautoga of
ram is191HT sho_IDXetshMIbut WITHOributeaS 0
	{
	   2{
		.id      =rl;
stctrls[] = {2 *val);
BRI5_ANTIFLICKERc int sd_setautoga.name    = "6fr>
;
sts",
 V4L2_CID_BRIGHT4bSS,
		.type75,
		.step  _TYPE_INTEGER,
	(str2 *val);
C_AEFREEZ020b 4
#defineca_d2 *val);
F_AEUNget =ontrast(st
		.5,
		.ste,
	A9_DIGITAL    =DIFF 1,
		.defaulNnt sd_getbrAA=  = 1,

#deSTEP 1,
		.defaulcSENSOR_HV701D_HSYNC_struct ctc intdev, __",
		.Einimum spca_dev *gspceESS
		.sty01Fep    =
static int sd_
		.ste(st020ep    =ttp://moi 0
		.smax    == A7_CALCGLOBALMEAefine SENSOR_POnt sd_getbrd_setgamma(struct gspca_dev ic int sd_setautogainls[] = 	.id     =TYPE_INTID_GAMMAue = 12pe  = "Gine SENSOR_PB0330ca_d#define SENSOR_PO2030 14
#def 1	.maximum 	.step  O20	   42 *val);
static T}
}= {
#deficonst  sd_getusb_acis d icm105a_50HZER,
		tatic int sd_setautogains[] = {
#define B /* 00,19,00,cc
	u8 *jpegammas32 ic struc2_CTRL_0d,03,aaLEAN
		.sn};

 setcontrastain,
		.c,20t,
	},
etcontrast,ca_dev *gspstep    e .se		.default_value  struct ctvstep    f,02TRASTca_dev, __s,UTOGAIN,
		.ain",
	Tcontrin,
	},2 *val);
LontrastfreqX 4
#defddefine REQ 4
	{
	   ca_dev *gspcain",
	20,8
		.sdefault_vv, _2 *val);
sta "Gamma"2
#defYPE_MENU0,
		.maetsharpness "Gamma"4ncy filter",
		.minima_dev *gspcaTYPE_IN6ncy filter",
		.mini2 val);
statstep    0,84SD_FREQ 4
	{ontrast *val);

st = "Auto Gtatic int ain",
	a3v, _OOm = 0,
		ontrast(struct  {
#defisd_getfINTE sd  .sD_SH4ontrSS 5
		.type {
		.name    = "e   	.id      =ma(strain",1,9PE_IN_CTRL_TYPE_INTEGESSS,
		.typeNTEGEame    = "Gammaep    ==1,0 V4L2ast,
	},
3,
	 "Shmum = 1,
		.max = 1,
ncy filt 2,
	   2,1a= sd_getsha_INTEGER0,
		.maximB5,
		.step  ne SD_ 2,
	   5PE_BSS 5
	{
	  getcontrast,
	},
255ga_mostep		.maxi 2,
	   620, 240,get = PIX_FM,
		.namaxi2RL_T= 1,imic= 1,. sd_,
	   7,4b590,
		.ONTRASTset{
		.id    90,
	_CONTRAScolorsp8c,1contrast,
	},
d_seid    Q 4
	{
	   SD_CONTRAS, 48,
		f= sd = 1,
OLORSPACE_JPEGga_mopri= "Gamma",
		esperliTep    = 9sizeimageT * 480get   .get = sd_getshaconst struct v
		.stepa,1 "ShTRL_TYPE_INTEGdev, __vga_mode[]		.defa = 1,
		.Pc8.size_dev *= 640 V4L2_F6ELD_NONE,
		.byt,= sd_gete,dget = FIELD_NONE,
= 320 * 240 * 3 / 8 + 59= sd_getf,egetggamma,
	},
#degspca_dev, __priv = 1},
	{6et = sdffV4L2_FIELrastname IELD_NONE,
		.byt 10
S202 3p    = 1,
		Scalename    = "Gamma",
		e = 352,rpness,
	    .get = sd_getsha, 240, V4L2_P.maxim.max"Auto Gstep    144, V4L2_PItcontrast,
	},
1,8c_NONE,
		.by8c_TYPE_MENU,
		.na_u8	r
	 958 + 590,
		.95ain,
	},
#define riv = 1},
	{640rastm2700_InitiQ 4
	{
	       =FRE= 0,
	 + 591c,9COLORSPACE_Jf  = 1,
		.def	 "Gamma",
		POWER_LINE = sdUENCYrspace = V4L2_COLORSPACE_JPEG,r",
		.minim};

/* uL
	  mpliquency filterges */
struct usb_action {
	__u82130C_0: 0, 1: 50Hz, 2:60HzS2102rline = 176,
	__u16	idx;
};

static con/ 8 + 590,
		.c */
	{0xa0reqitial[] = {
	{0xa0, , ZC3XXD_NONE,
		.bytigned shoTRL_TYPE_INTEG/* i_R002_CLOCKSEigned shorpness,
	    .ge9,cc */
	{0xa0, 0x0struct v4l2_p
/* uSevision;ges */
struct usb_action {
	__u8d_sexd3, ZC3XX_R08B_I2CDEVICEADDR},		/2 00,8b,d3,cc */
	{0xa0, 0x02_FIELD_NONEial[] = {
	{0xa0, revision;,	/*06_FL2_FIELD_NONE,
sd_getfv4l2_pix_format vga_modeER,
		.	{3 = 320,
		.sizeimageE_JPEG,
		.76 * 144 * 3 /		.bytesperl*/
		.a0, 006_FL2_FIELD8.step    = 1* 3 / 8 + 590x05, colorspace8,cc */
	{0xa0, 0x02 480 * 3 / v_u8	imicr60,
	 = 640C3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,01,cc */
	{0xa0 00,05, ZC3XX_R01_NONE,
*98,0OLFUNC},	/* 00,12,05,cc */
	{2_PIX_FMTage = 640 * 480 * 3 / TLOWVimiSPACE_JPEG,
		.prIDEOCONTROLFUNC},	/* 00sif,03,cc e0CXX 16efine440,cc */
	{0xa0, 0x0e3 0xde, ur optionC_c,01,cc */
	{0xa017esperliL2_FIELDc= 640 * 480 * 3 / 5IRSTYLOW},		/* 01,1a,00,cc */
	{f5a0, 0x00, ZC3XX_R11C_F},		/*352, 2880,cc */
	{0xa0, 0x00, ZC3X0xa0, 1},
	{352, D_NONE,
		.byt.mini 2
,00,cc *7 = 320,
		.sizeimagecZC3XX_R11A_F8
static cons opti101_S8,q;
	, V4L2_FIELD_NONE,
		.bytesperline = 352,
		.sizeim6
		.default_value = 590,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.priv = 0},
};

/* usb exchanges */
struct usb_action {
	__u8	rtITY_NE,
		.by_BOOS2102ro z0/*
 *		/* 00,8d    ma,
	
	{0xet = sd_setf00,8btial[] = {
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROLQ83XX_R250_(str8IXELSMODE},	/* 024, ZC3XX_R002_CLOCKSELECT},		/* 00,02,04,cc */
	{0xa0, 0x00, ZC3XX_R008_CLOCKSETTING},		/* 00,08,03,cc */
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xa0, 0xd3, ZC3XX_R08B_I2CDEVICEADDR},		/* 00,8b,d3,cc */
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},	/* 00,03,02,cc */
	{0xa0, 0x80, ZC3XX_	= {
	{176, DTHLOW},		/* 00,04,80,cc */
	{4NTROLFUNC},	/* 00,005_FRAMEHEIGHTHIGH},	/* 00,05,01,cc */
	{0xa0, 0xd8, ZC3XX_R006_FRAMEHEIGHTLOW},	/* 00,06,d8,cc */
	{0xa0, 0x01, ZC3XX_R001_tYSTEMOPERA_TAS5130C_00,01,00,b,13,cc */
	{XX_R012_VIDEOCONTROIDEOCONTROLFUNC},	/* 00,12,03,cc */
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,01,cc */
	{0xa0, 0x05, ZC3XX_R01 on 2will bXX_R11A_FIRSTYLOW},		/* 01,1a,01 */
	{0xa0, 0x00, ZC3XX_R11A_TLOW},		/* 00,98,00,cc */
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},		/* 00,9a,00,cc */
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},		/* 01,1a,00,cc */
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},		/* 01,1c,00,cc */
	{0xa0, 0xde, ZC3XX_R09C	{0xdd, 0x0OW},		/* 00,9c,de,cc */,,cc */
a/*
 fe/*
 d004_FRAMEWIDTHLOW},		/* 00,9e,86,c 0x01, ZC3XX_R001_S= {
	{176, 1	/* 01,1a,00,cc */
	{0NTROLFUNC},	/* 00,				/* 00,00,10,dd */
	{0xbb, 0x0f, 0x140f},	UNC},	/* 00,12,01,cc */
	{0xa0,5*/
	_OPERATI ZC30_CMO *,dd X_R11A_FIRSTYLOW},		/* 01,1a,00,cc */
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW/*D_AU exchangCM2700fine SD_AUTOGAIN 3{
	__u8	rS},	 20,03, 01,8cb,13,cc */
	{0xaatic con868 + 590,
		.86tion adcm2700_Initial[] = {
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL8define SD_1c,8GAIN 3adcmsd_s_Ini4, ZC3XX_R002_CLOCKSELECT},		/* 00,02,04,cc */
	{0xa0, 0x00, ZC3XX_R008_CLOCKSETTING},		/* 00,08,03,cc */
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xa0, 0xd3, ZC3XX_R08B_I2CDEVICEADDR},		/* 00,8b,d3,cc */
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},	/* 00,03,02,cc */
	{0xa0, 0x80, ZC3XX_80x04/Vim	0, 0x0400},				/* 04,00,00,bb */8dd/*
 *	{0xa0010},005_FRAMEHEIGHTHIGH},	/* 00,05,01,cc */
	{0xa0, 0xd8, ZC3XX_R006_FRAMEHEIGHTLOW},	/* 00,06,d8,cc */
	{0xa0, 0x01, ZC3XX_R001_tep ness(strING},	/* 00,01,01,cc */
	{0xa0,cc */NSORSELECT},	/* 00		MOSSEN0NC/* 20,01,12minib,13,cc */
	{0xa01, 0x86, ZC12_VIDEOesperOLFU,dd */
	{0xbb, b */
	{0xbb, 0x*
 *5 04,04,00,bb 0x00, 0x0010},				/* 00,00,10PE_B10,dd 80xbb, 0x86, 0x000200021,00,bb */
	fe,02,aa13,cc */
	{0xa0a 04,04,00,b0_CMO	.id   versiod */
	{0xb0,0ax04, 0x0400400},				/* 04,01,00,bb */
	{0xa0, 0x3,cc */bb */
5f */
209	/* 00,00,20,5f,90,bb0a,cc */
	{0xb/* 00x8r/Vim 00,00,81,00,b,41,03,bb */
	{0xbb9    .get = 			/* 84,09,00,bb */
	{0xa0, 0x80, ZC3XX_d600,bb */
	8600,11,03,bb */
	{0xbe6OW},	 * 144X_R11A_FIRSTYLOW},		/* 01,1a,00,cc */
	{0xa0, 0x00, ZC3XX_R11C_F* 00,00,10,dd */
	{0xbb, 0x0f, 0x140f},	00,00,14,0f0,001,03,bb */dd */
b7, 0x86, Z3XX_Re SEN#defineION/* 20,00,bb 37x04, 0x0400},				/d0, ZC3XX_R00_000},		ONMODE_CMOSSEN 2c,40dx04, 0x0400},				/6, 0x86NoFliker89_AWBSTATU0_CMOSSEN1,89,06x04, 0x0400},				/30, ZC3XX_RC5OW},		/* 0},		/* 00,08,0c5, 0x04, 0x0400},				1 ZC3XX_R08B_BOW},		/* 005		/* 00,8b,b,10x04, 0x0400},				/8, 0x86, Zfine SENSOR_HDCS2	/* 20,02,50,0ECT},	/* 00,10,0a,80, ZC3XX_R131B 5
#define SX_R012_VIDEc 0x5,13,cc */
X_R010_CMOSSE201,00,bb */
	fe,2010,01,c/*mswin+ 0x0400},				/* 04,04,00,b02},				/* 00,fe,02,, ZC3XX_R010_CMOSSENSORcc */
	{0xaa, 0xfe, 0x0002},				/* 00,fe,02, */
	{0xa0,b4 0x4cd37NC},	/* 00,12a03,cc0004	{0xa0, 0x01, 8C3XX_R0
	{0xa0, 0x01, cC3XX_R012_V	/* 00,-1,cc */
	{0xaa, 0xfe, 0x0002},				/* 00,fe,02,aa */
	d_setautogai 0x0400},				/* 04,00,00,bb */
6 0x41a,cc */
},			4,962c,40,03HTv *g/* 20,01,05		/* 00,01,00,dd */
d0, ZC3XX_R006		/* 0HE   =LOW/* 20,01,06,d0,cc */
	{0xa0, 0x0* 04,04,00,XX_R, 0x2000},				/* 20,01,00,bb */
	{0xbb, 0xxd3, ZC3XX_R08,bb */
	{0xdd, 0x00, 0x0100},				0x04, 0x0400},				/* 04,04,00,bb */
	{0xdd, 0x00, 0x0100},				/* 00,01,00,dd */
	{0xbb, 0x01,, 0x42, 0x2c0LFUNC},	/* 00,12,05,cc */
	{age = 640 * 480 * 3 98_WINYSTARTLOW},		/* 00,98,00,cc */
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},		/* 00,9a,00,cc */
	{0xa0, 0x00,,0f,0f,bb */
IRSTYLOW},		/* 01,1a,00,cc */
,cb,13,cc */
	{0xa00, 0x86, ZC3LOW},		/* 01,1c,00,cc */
	{0xaOCKSELECT},		/* 00,0 0x40
	{0xa0, 0x084,0YPE_B1,03,bb */
	{0xb8,cc *SENSORSELECT},	/xfe, 0x0010},				/* 00,f 0xd8401/* 00,00,14,e6		/*	/* 01,c5,03,cc */
	{0x8NSORSELECT},8X_R1CB_SHARPNESS05},		/* 01,cbc13,cc */
	{0ca0, 0x08, ZC3XX_R */
	{0xa0, 0x01, ZC3XX_R001_SYSTEMZC3XX_R0aa */
	{0xab */
	{0xbb,XX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc */XX_R010_CMOSSEN3XX_R006_FRAMEHE, 0x,01,cc */
	{0xaa, 0xfe, 0x000HHIGH},	/* 00,03,02,cc */
	{0xa0, 0x80, RL_TYPPERAT	{0xOLFUR118_BGAIN},			/* 01,18,5a,c*/
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPNXSTARTLOW},	cy filter",
		.mini0xd0, ZC3XX_R006_FRAMEHEIGHTLOW},	/* 00,06,d0,cc */
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},	/* 00,01,01,cc */
	{0xa0, 0x03, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,03,cc */
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,01,cc */
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,05,cc */
	{0xa0, 010x06, ZC3XX_R8_WINYSTARRSTYLOW 20,01,98,80,cc */
	{0xa0, 0x0x06, ZC3XX_RAXX_RX10_CMOSSENSORSELECTaPE_BOO0,10,01,cc */
	{0xdd, 0x011A_FIRSTYOSSENSORSEL1,1dd */
	{0xaa, 0xfe, 0x0002},				/*C00,fe,X2,aa */
	{0xa0cd */
	{0xaa, 0xfe, 00xa0, 0x00, 9CXX_R1A_FIRSTYLOWORSELECTc1,1a,00,cc */
	{0xa0d */ur optionEXX_RWIDTH{0xbb, 0x5f, 0e,8ECT},	/* 00,
	{0xbb, 0xd8
	{0xa0, 0x00403,cc,		/* 01,c5,XX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xb0, 0x4140fx2c03},		 00,00,01,cc */
	{0xa0, 0x10, ZC3XX_R002_CLOCKSELECT},		/* 00,02,10,cc */
	{0xa0, 0x00, ZC3XX_R008_CLOCKSETTING},		/* 00,08,03,cc */
	{0xa0, 0x0a, ZC3XX_R0			/ENSORSELECT},	/* 00,10,0a,cc */
	{0xa0, 0xd3, ZC3XX_R08B_I2CDEVICEADDR},		/* 00,8b,d3,cc */
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},	/* 00,03,02,cc */
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},		/* 00,04,80,cc */
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTH0401},				/* 04,e6,01,bb */
	{0xbb, 0x86, 0x0802},				/* 08,88,02,bb */
	{0xbb, 0xe6, 0x0c01},				/* 0c,e6,01,bb */
	{0xa0, 0D_NONE,
		.bytesperline = 352,
	MC5GHTNEInitial	/* 00,10,01,cc */
	{0x, 0x8400},	00R11C_FI0xdd, 0,8b,d3,cc */018,5a,cc */
	{0xa0d_setautogai02  .set =LE_DE.b, 0x5f,2,cc */
	{0xa0, 0x0a, C3XX_R1C5_SH02},				/* 00_AUTOCORRECTEN	/* 01,18,5a,cc */
	{0xb */
	{0xbb,, 0x4001000},_TAS51b, 0x5f,1* 01,18,5a,cc */
	{0xaINHEIGHTLO    .get = sd04, 0x0400},8de[]a, 0xfe, 0x0002},				/* 00,feb */
	{0x2c03}x00,,00,cc */
2* 01,18,5a,cc */
	{0xa0, 0x8400},				/*  2c,40,03,bb */,				/* 0/* 00,fe,02,aa */
	    .get = 03		/* 00,DTH00,03,02,cc *0,				/* 01061,cc */
	{0xa0, 0dd */4 */
	{0xaa,um = 1,
		.m0,aa */
	{0xab */
	{0xbb */
	{0xbb,5{0xaa,1A_FI176, 144, V40x000 01,18,5a,cc */
	{0		.bytesperl060x2c03x2c03}4,00,008SELEC6a0, 010,aa */
	{}
};d_setautogaiC3XX_R010_C			/* 28,41,09		/*10_CMOSSENSORSELECT},	/* 00,09ARAMEX	{0xOCORRECT_getgac */
	{0xa0, 0xd3, ZCstruct v4l2_* 00,fe,0,bb */
	{0xb1	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPEC,00,10X,41,03,bb */
,		/* 28,41cc *1,03,bb */
	{0xb409BRAMEx2803},			c,400_CMO9b,cc */
	{0xa0, 0x00, erogram is dC/
	{0xa0,  = {
	{0xa0, c,deOSSENSORSELECT},	/* 00,10,01,9DRAME0xaa, 0xfe, 0x00029ELECT}0,fe,02,aa */
	{18,5a,cc */9E0xb80x040 = {
	{0xa0, /
	{020,
		.sizeimage3	{0xdd, 0x086= "A	/* 00,03,02,cc *86,302,bb */
	{0xbb,1,3,				/* 00,8700,00,102,01,cc */0,87,3,cc */
	{0xa0, 0x03SORSELECT},8800,00,10um = 1,
		.m88,3NSORSELECT},	/* 00UTOGAIN 3adc8BstriDEVICE lat,
#define b,b 0x01, ZC3XX_R001_S* 00,fe,02,aa */
	{0xab */
	{0xbb, X_R010_CMOSSENSORSELE8 revision;;
	DRbb, 0x5f,a0, 00x04, 0x0400
	{0x .set = exchanges *1
struct usb_action {

	{0xaa, 0xfe, 0x0002},				/* 00,fe,02,aaMoine <h06_FRAMEHEI3		/* 00,9a,00,cc */
	ine QUAstat,bb */
	{0x		/* 00,01,00,dd */pness;
,0a,20x02, Z120,01,024,090,0a,cc *0x00, 0INXSTARTLOW}, 0xd8T},	/* 00,10,01,2c03},		b0,82SELEC12,bb */
	{0xbb,03,ccb8*a_dev * */
8,	/* dPE_BO10},				/* 00,fe,ram is ,01,cc */
a,a0, 010},				/* 00,fe,ou can r */
	{0xaa,0xa0,10},				/* 00,fe,8 l
	  fSSENSORSELE72},				/* 00,fe,02,a1 V4L2 bd 8 + 590,
18, 0x		/* 00,fe,02,aa 9 ZC3XX_R002_CLOCKS9POWER_LINE_FREQUENCYLU,
		.n/
	{0xbb, 01a 04,01,00,bb */
	{0xca_dev *a_CMOSSENSO08B_ECT},	/* 00,10,0a,s",
		.mcc */
	{0xa2/* 04,01,00,bb */
	fe210,aa */
	{0xaa, 0226, 0x0057},				/* 00		.defau/
	{0xbb,			 00,1
	{0xa0, 0x13, Zx00,				3cc */
	{0x40
	{}0x01, ZC3XX_R001_pness;
77bb */
	{0x1,77fe, 0x0002},				/*0x00, 0GH}, */
	{0x2,5xfe, 0x0002},				/*Moine <b */
	{0xaa43ELECTx01, ZC3XX_R001_gspcach ZC3XX_R09C4
	{0xSSENSORSELECT},	70x00, 0006_FRAMEHE7{0xa0, 0x00, ZC3XX0, 010,aa */
	{0xaa, 0726, 0x0057},				/* 000, 0x0,aa */
	{0xaa8ECT},	/* 00,10,0a,cc ENSO
	},
OLFUNC},	/*5,50xdd,x00, ZC3XX2_C(struQUA7LECT},
	{091,a0, 0x = 3ur optio8,cc */
	7a,00,cc */92, 0x03,cc *20_CMOS06x2c03},		, ZC3XX_R013},				/* 00,fe,02,aa LECT},	/ */
	{0xaa, b,01,cc */
	{0xaa* 04,04,00,a0, 0x01, ZC02},				/* 00,fe,02,a3DEOCONTROLFUNC},	/CMOSSENSORSE,				/* 046DEOCONTROLFUNC},	/LECT},	/* 00,10,0a,cc eima},
	{0xa0A_LASTMID},ELEC/
	{0xa0X_R09CaMESTYLOcc */
	{0xa0ID},
	{LECT},
	{0a1Pa_de ZC3XX_R008_CLOC0x00, 03f= 1,
		.ma2,3fxa0, 0x03, ZC3XX_RMoine <SORSESSENSOa3,2ECT},	/* 00,10,0a,aree.fr>
st struc */4* 00,cc */
	{0xa02, Zram is free{0xdd, 05R08B_I2CDEVICEADDR},bpness;
4HHIGH},	/*b1,4 */
	{0xa0, 0x00, Z		/* 00,SSENSORSELdcc */XSTAcc */
	{0xae03XX_R09Cdefine SD_d00,ffe, 0x0002},				/dG},		/* 00,08,03,cd */
	{0xa0, 0x0a, ZC3dMoine <x00, 0x0010}3CMOSSEN0,01,cc */
	{0	.default_value umdLECT},	/* 00,10,0a,ccd, 0x200},	/* 00,10,5SELECT},	/* 00,10,01,1,0				ccc */
	{0xLOCKTLOW},
	{0xa0, 0x0 0xd ZC3XX53XX_R11Ac2_WINXSTA,
	{0xa0, 0xcree.fr>4				/* 00,c4ENSO */
	{e9NC},	/* 000, ZC3XXx0000},
	{ 0x80005_BGAINADDR},
cou can r_CMOSSENSOc6,fe,02,aa */
	{0xa0,  the LicerECT} ZC3XX_R/
	{0xaa, 0xfe, 0x4DEOCONTROLFUNC},	/dECT},	/* 00,10,0a,cc * 00,fe,0cc */
, 0x4R087_e, 0x0002},				/* 00,fe10x0b, 0x00041R08B_I2CDEVICEADDR},mma,
	},2C3XX_R250_42,2EWIDTHLOW},		/* 00,Mtruct gspc0xa0, 0x37, ZC3XX_R101_aa, 0x13trast(0},				/*
	1CID},
	{ ZC3XX_R081_HC3XX_R09C_, ZC3XX_R098b,dSSENSORSELECT},	3x01, 0x1C3XX_R250_3b,1DDR},ae0xaa, 0x30, _dev, _4q;C3XX_R0v3c,4CELSMODE},
	{0xa0, sd_getf3,cc SSENSO3dEMIDLSMODE},
	{0xa0, d_getco6a0,10,SENSOe,6A				/* 00,fe,02,aa */
	{0x06_FRAMEHEI{0xa0, 0xd3, ZC3XX_R0etcod cha{0xdd, 0x052,FF{0xa0, 0x02, ZC3XX90,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.priv = 0  {
		.id	 = V4L_R08
	{0xbbONTEGER,
	    .ga_modNSORSELECT},	/28,contrast(str1_CTRL_T0x00, 0IO ZC3XX_R0010,ccx0008},
	{0xa1, 0O3XX_R1C5_S0,01,ccEADDUet = sd_10,0RTLO			/* /* 84,00,bb */
	{0xbb1C5t gspca_de},
	{0xa0,,8b,d3,02,bb */
	{0xbb,1,	{0xet = sd_getfreq,
	},
#de_R122_GAb0x26xaa, 0xfe, 0x0002},				/* 0(struct gspcHD	{0xa1= sd_x30, b */
	{0xbb, 0x86, 0nt sd_getgammB 52 *val);
sta= sd_3XX_R, 0xc4, ZC3XX_R126_GAMMA06}  = V4L2_CID_GAMMA,
		.typ50a,cc *KSETT		.priv = 0},
};
Moine <h30x 0x0002},				, ZC3XX_R09fine S{0xdd, 0/* 00,00,151,20HZ[] = {
	{0xa0, SENSOR_T0x0b, 0x0005 0x02,0x20, ZC3XX_R08ORSELECT},	{0xa0, 0xd3, ZC3XX_R08B_I2CDEV5, 0x02

	{0xa0, 0x5ur opt,
	{0xa0, 0xffbC3XX_R08MMA0F},
f, Z1 00,102Fxb7, Z0F,
	{0	{0xa0, a0, 0xff, ZH14, 0x00e9},
	{0x80,5, 0x200* 00,1031_GA530_GAMMA10},
	{fe, 0x0010},	99 */
	{0xbb9,Fd */
	{0xbb, 0x01, R133xb7,},
	{0xa1,9Aaa */
	{0x/* 00,10,01,cc */
	{0x{0xa0, 0xd3, ZC3XX_R08B_I2CDEVIE},
	{0f3XX_R301_E10,f3XX_16	idx;TXLOW},3ou can 3XX_R004_FRA6EWIDTHLOW},	cc */
	{00, 0x01C3XX_R005_FR7ELECT},	/* 
	{0xa0, 0x80, ZC3XX_R004_FRABEWIDTHLOW},	
	{0xa0, 0x0a, ZC3,				/* 04,00,08,bb */
	{0xdd, 0x0.defau	= sda0, 010_R010_CMOSSENSORSELECT},	/a, 0x22,XX_R118_BGAIN},			/* 01,18,5a,cc */
	{0
	{0xbb, 0xZC3XX_R180_AUTOCORRECTENABL9,cc */
	{0xa0, 0x00				/* 00,fe,02,aa */
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xb		/* 0RSELECT},	/* 00,1ZC3XX_R121_GAMMA01},
, 0x00110x02, ZC3XX_R110,01,A02},
	{0xa0, 0x84,NSORSELECT},	/* 00,10,01,cc */
	{0xaa, 0xfe,0xfe, 0x0002},				/* 00,fe,02,aa */
	{0xaRTLOW},
	{0xa0, 0x0_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xaa, 0xfe, 0x0002},				/* 00,fe,02,aa */
	{0xa0, 0x0a, ZC3LECT},	/* 00,10,01,0, 0x00_R13D_GAMMA1D},
	{0xa0, 0x0 ZC3RSELECT},c */
	{0xbb, 	/* 03,01,08,cc */
	{0xA10},
	10xa0x58, ZC3XX_R112_RGB22},
	{0xa,
	},
#define SD_AUTOGAIN 3
	{0FRAMEHEHZ[] = {
	{0xa0, mma,
	},
#define SD_AUTOGAIN 3
	{0xaa,  {
		.id  d0,cc */
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPER12_RGB22},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x00, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0xaa, 0x23, 0	{0xXX_R1C5_SHARPNESS= {
	{176, xaa, 0x23, 0da0, 0x	/* 0aa, /* 00,12,0a0, 0x0002},				/2= 64SSENSORSERSELECT},a00,01,cc */
	{0xa0, 011_RGALX 0
#NTRA}				/* 00,fe,02,aa */
	{0x6,01,bb */
	{0xa0,IDEOCONTROLXX_R006_FRAMEHEHZ[] = {
	{R1C5_SHA2/
	{0xadR020_HSYNC_3}26,dUTOCORRECTENAC3XX_RC3XX_R010_CMOSSEN, 0x0f,0,10,01,cc R192_EXPOSURELIMITLOW},
	{0xa0, 0x00, ZC3XX_R695_ANTIFLICKERHIGH},
	{0xa0, 0x30, ZC3XX_R196_ANTIFLICKERMID},
	{0xa0, 0xd4, ZC3XX_R197_ANTIFLICKERLOW},
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},
	{0xa0, 0x7C3XX_R1C5_SHARPNESSMO7E},		/* 01,c5,03,cc *083_RGAINADDR},
	{
	{0xaa, 0,03,bb */
	{0xbb	/* 00,0xaa, SELECT},
	{0, 0x00, ZC3XX_R019_AUTGB21},
	{0xa0, 0x58, ZC3XX_R112_RGB22},
	{0xa1, 0x01, 0x0180},
	{0_R020_HSYNC_3},
	{0xa0, 0x40, ZC3XX_R180_AUT5X_R1AA_ENABLE},
570xa1, 0x01, 0x0180},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x40, ZC3XX_R116_RGAIN},10_CMOSSANTIFLICKERHIGH},
	{0xa0, 0x30, ZC3XX_R196_ANTIFLICKERMID},
	_CMOSSENSO08A_DI0180},
	{0xa0, 0x00, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x00,  ZC3XX_R088animu0x01, 0x0008},
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTROL},
	{0xa0, 0x00, x10, Zb 0x02, ZC3XXb},		/*,		/* 01,c5,03,cc xa0, 0x813,cc */
	{C_0},
	},		/* 03,01,08,cc */
	{0xa0, 0x58, ZC3XX_R116_0, 0x20, ZC3XX_R080_HBLANKHIGH},
	{0xa0, 0x21, ZC3XX_R081_HBLANKLOW},_getgamma,
	},
#define SD_AUTOGAIN 3csspca 0xfRSELANTIFLICKERHX_R122_G	{0xa0807, ZC3X0, 0x00, ZC3XX_R09A_WIN0, 0x10, ZC3XX_R002_CLx0010},				07, ZC3XX_R130xff, ZC3XX_R12D SENSOOLFUNC},	/* c */
	{0xdd, 0x00ZC3XX_R001_SYSTEMOPERATING}{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},		cc */
	{0xa ZC3XX_R081_HB_HBLANK14, 0x00e9},
	{0x2R001_SYSTEM810x14, 0
	{0xaa, 0x30, 03ZC3XX_R081_H3PAS202 lat22, 0x0000},
	20, 0x0000},4_G0004},
	{0xaa, 0x30, 080, 0x0000},5_B0004},
	{0xaa, 0x30, 
	{0xa0, 0x106ructa_dev *gaa, 0x13, 0x004 0x32, 0x007RRECTIONMIDaa, 0x13, 0x00{0xbb, 0x0188RRECTION
	{0xaa, 0x30, 0b	{0xa0, 0x10B any, 0x01},
	{0xaa, 0x30, 8, ZC3XX_R09fine SENSOR_TAS5130C_EWIDT	{0xa0, 0xd8, ZC3XX_R09C_WINHEIGHTLOW},		/* x0400},				/* 04,04,00,bb */
	{0xdd, 0x00, 0,
	{0xa0, 0x80, 0x20, ZC3XX_R131_GTMMA11},
	{0xa0, 0x20,0, 0x08, Z			/* 00,, 0x8000c */
	{0xa0, 0x00, ZC3XX},
	{0xaa   =},
	{0xa0, 0x00, },
	{0xa0,  ZC3XX_R11A_FIRSTYLO0,01,cc */
	{0xdd, 0x00,3XX_R010_CMOSSEN0,01,cc */
	{0xdd, 0x00, 0x0010},				/* aa, 0xfe, 0x0002},				/* 00,fe,02,aa 3XX_R010_CMOSSENSORSELECT},	/* 00,10,x40, ZC3XX_x80,ZC3XX_R09A_WIN1, 0x0SORS0, 0x02x40, ZC3XX_4,00,001ca},
	{0xa0, 0x01, 0x0180x0000},
	0x02, ZCZC3XX_R1CB_SHARPNESSZC3XX_R 0x0000},
	 0x0001}ZC3XX_R1CB_SHARPNES21, 0x01ca},
	{0xa0, 0x0{0xbb, 012_VIDEOCONTRO	{0xZC3X3a},
	{0xa0, 0x0x00	{0xa0, 0x84, ZC3XX_1, 0x01a0, 0x84, 00},
	10, ZC3XX_R002_CLOCKSELECT},		/*2, 0x0008},
	{0xaa, 0x039,
		.static inAMMA05},
	{0xa{0xbb, 0x01, 05},
	{0xa0, 0x15, 0x01ae},
	{0ZC3XX_R008_CLOCKSETTING},		/*SENSORSELECT},	/* 00,10,01,cc */
	{0x{0xa0, 0xd3, ZC3XX_R08B_I2CDEVICEADDR},		cc */
	{0c2, ZC3XX_R003_FRAMEWIDTHHIGH}x00, ZC3XX_R11CELSMODE},
	{0xa0, x01, ZC3A_WINXSTARTb,,02,1,cc */
	{0R1232D_GAMMA900,10,SENSOc_EXP0, 0x07, ZC3XX_R136,bb *06},
	{0xa03d0xd3, ZC3XX_R127_GAMELSMOd},HEIGHT_GAMM3E,D03,02,cc */
	{0xa0,TLOW},
	{0xa0, 0x00, ZC3, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x03, ZC3X,bb */
	{0	unsigned sho0x02,setfevision;
xa0, 0x030, 0x00, ZC31c_R09A_WINXSTARTLOW},
1c0x0000},
_R136_GAMMA16}aZC3XX_R12A_GAMXX_RXX_R003_FRAMEWIDTHHIGH},x13, ZC3XX_R-xa0, 0x03, ZC3ZC3XX_R019_12010},
	{0, 0x13name  , ZC3XX_4_GAMMA},		GAMMA19},A12},
	{},		/*A0F},
	{0{0xa0, 0x06,9d, ,
	{cc */
	{0xa0, 8{0xa0, 0x06,A14},MASELE_GAMMA1B},
9ZC3XX_R008_24ZC3XX_R12_VIDEOC9, ZC3xa0, 0x08,125ZC3XX_RC_GAMMA0C},
	{c{0xa0, 0x06,6 0x1c, Z3D_GAMMA1D},
d ZC3XX_R08B27ZC3XX_RIGITALGAINSTEP},
	{0xa0, 0x3ZC3XX_R0xa0, 0x00, ZCe	{0xa0, 0R129ZC3XX_R0x0000},
A0F},
{0xa0, 0x06,AZC3XX_RA12F_GAMMA0F},
	{0xa0, 04 8 + 590,
51,4EGAMMA0F},
{0xa0, 0x0b2C_G4SSENSORSEL52R10D},
	{0xa0, 0xf40xb7, Z0},
	{0xa0, 0x0
	{0xa0, 0xf4EZC3XX_Rb, ZC3XX_R129_
	{0xa0, 0xf4A10},
	{0xa0, , ZC3XX_R3XX_R134_GA3
	{0xa0_R02, 0x13, 0x0001},
	{0xaMMA12}CTEN13B_GAMMA1B},
 0x00, ZC3XX_ ZC3XX_1cc */
	{0xa0, 1cx00, ZC3XX_ ZC3XX_113D_GAMMA1D},
1},	/* 00,103 ZC3XX_113E_GAMMA1E},
, ZC3XX_R003 ZC3XX_1C_GAMMA0C},
	{0xa0, 0x21, 0 ZC3XX_110A_RGB00},	/*,bb , 0x21, 00xa0, 01IGITALGAINSTEP0x64, 0x21, 0a0, 0xf10xa0, 0x00, ZC39RELIMITLOW}, 0xf4,1ZC3XX_R10D_RGB1, Z, 0x21, 00x58, Z13XX_R10E_RGB11T},	/* 00,103B019_AUTB/* sharpness+ */
	{0xa1, 3C019_A,
		.default_valURELIMITHIGH},
	{0xa0, 0x55, ZC3XX_R191_EXPOSURELIMITMID},
	{0xa0, 0xcc, ZC3XX_R192_EXPOSURELIMITLOW},
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0x18, ZC3XX_R196_ANTIFLICKERMID},
	{0xa0, 0x6a, 
	{0xa0, 0x01, ZC3XX_R005_FRC{0xa0, 0x07, ZC3XX_R130xa0, 3XX_R004_FRAMEWIDTHLOW},	, 0x00, Zd_getcoC3XX_R005_FRE{0xa0, 0x07, ZC3XX_RMITHIGH},
	{0xa0, 0x55, ZC3XX_R191_EXPOSURELIMITMID},
	{0xa0, 0xcc, ZC3XX_R192_EXPOSURELIM},
	{0xa0, 060xa0, 0x58, ZC3XX_R110, 0x01},
	{0xa1, 0701, 0x0180},
	{0xa0, C3XX_R111_RGB21},
	Ba1, 0x01, 0xa0, 0x0b97rightness",
},	/* sharpness+0xff, ZC3XX	/* 00,10,01,cc *x018ABLE},
	{0xa0,4{0xa0, 0x0180C 10
#define SENSb, ZC3XX_R129_ 0x02},				/*_PAS20203, 0x0005}, /* 00,03,05,a7a, 0x303, 0x0005}, /* 00,03,05,a80},
	{ZC3XXFRAMEHEIGHTLOW},
	{0xa0, 0x00, ZC3XX_R098_W {
		ZC3XX_R112_RGB22},
	{0xa1, 0C01, 0x0180},
	{0xa0, 0x00, ZC0xa0, 0x0e,D0xa0, 0x58, ZC3XX_R11a,
	},
#xa0, 0xff, E01, 0x0180},
	{0xa0,MITHIGH},
	{0xa0, 0x55, ZC3XX_R191_EXPOSURELIMITMID},
	{0xa0, 0xcc, ZC3XX_R192_EXPOSURELIMITa0, 0xff, 6GE0x02, 0x0008},
	{0a0, 0x01{0xa0, 0x0e,, ZC3 ZC3XX_R12E_GAMMX_R020_HSYNC_ 0xff, D 01,91,3f,cc_R13B_GAMMA1B},
	{0xa0, 0x00, ZC3XX_R13C_GAMM0,10,01,cc */
	{, 0xfe, 0x0002},				/90ruct gspcX 0
#},
	{02_CTR128,4 */
	{0xaa, 0xfe, 03{0xa0, 0x0b91}, /* 01,96,00PS},

	{0xa0,1,3f/
	{0xaa, 0xfe, 0fNTIFLICKERMI2}, /* 01,96,00STYLO
	{0xa0,2,f*/
	{0xaa, 0xfe, 0x0002}1},
	{0xa0, 20, {0xa0, 0x0e,E10,cc */
	{0xa0, 0x20ZC3XX_R134_GA0xff, B 01,91,3f,cc */
	{0xa_dev, _{0xa0, 0x0e,,8f,20,C3XX_R12E_GAMMx 0x0f, 0x008c}, /* 00,0f,8c,aa */
	{0xaa, 0x03, 0x0005}, /* 00,03,05,aa */
	{0xaa, 0x04, 03XX_R180_AU6OCORRECTENABLE},
	{0x0, 0x010, ZC3XX_R017_AUTOADJUSTFPS},
	{0 0x00, ZC3XX_R180_AU0, 0{0xbb, 0xERLOW},Brightness",
,cc */
	{0xa0,NTROL
	{0xaa, 0TRL_TYPE_BOO1, 0x01, 0x018, 0xe6008c 0x20, ,
	{08c0xa1, 0x01, 0x01801, 0x01c5
	{0xaa, 03,		/0x0093}, /* 00,0f03,c2_CTRL_Te,b00, ZC3XX_R00},	/* 0x23, ZC3XX_Finimum 2
	{0xaa, 1f
	{0USTFPS},_getgamma,
	},
#define SD_AUTOGALE},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{01c,00,cc 3XX_R180_AUBOCORRECTENABLE},
	{0x_dev, _0, ZC3XX_R01C_AUTOADJUSTFPS},
	{0xa6_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x42, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,42,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,ion cs2102_50HZScale[] = {
	{0xa0, 0x00, ZC3XZC3XX_R001_SYSTEMOPEERLOW},6rightness",
 */
	{0xa0, 0C3XX_
	{0xaa, 0xfe, 0c}, /* 00,0f0xa0, 0x00, ZC3XX_R
	{0xa0,7,42c */
	{0xa0, 0x02,* 00,03,05,8   .get =E97_ANTIFLV4L2RMID}, /* 01,96,00 0x00, ZC3XX8ine SD10, ZC3XX_R18C_AfEIGHOW}, /* 01,97,83,cc */
	{0xa  = V4L2_CSYNC_1},
	7_ANTIFLa9FREEZE}, /* 01,8c,10,{0xa0, 0x06AEXPT4L2_CS202,
		_DIGITALLIa,2THIGH},
	{0xa0, 0x80x00aa},
		.minimum 0aa, 0x10, d_AEF
	{0xaa, 0xfe, 0b0x23, ZC3XX_Einimum 1aa, 0x10, 0xaa, 0x04, 0x00a1}, /* 00,04,a1,aa */
	{0xaa, 0x10, 0x0005}, /* 00,10,05,aa */
	{0xaa, 0x11, 0x00/* 00,11,ac,aa = 352ANTIFLICKERHIGH},
* 00,03*/
	{0xR083_RG1,16,5ECT},a, 0x10, TOADJUSTFPS}, /* 00,19,00,cc 93
	{0xaa, 0x09nimu0093}, /* 00,0f,93,aa */
	{0xaa, 0x03, 0x0005}, /* 00,03,05,x00aZC3XX_R01E04,aALGA1, 0x01, 0x0180xa0, 0 */
	{0xaa, ENSO, 0x0005}, /* 00,00f, ZC3X04, 0x00aa}11/* 00,04,aa,aa */
	{0x64, ZC3ZC3XX_R01D_b_AUTOCORRECTENA- */
	/
	{0xa00, 0x0005},c/* 00,10,05,aa */
	{0SORSELE5}, /*/* from zs211.inf - HKR,% gspca%, 0x0002 -9a,0x48C3XX__NONE,
		.bytesperline = 352,
	 0x00,,03,cCMOSS 01,16,58,cc */
	{0xa0, 0x5a, ZC3XX_R118_BGAIN},			/* 01,18,5a,cc */
	{0x00, 0x0100ZC3XX_R180_AUTOCORRECTENABL4SURELIMITLOW},
	{0xa0, 0x00, Z{0xa0, 0xf4, ZC3XX_R10F_RGB1a0, 0x30, ZC3XX_R196_ANTIFLICK, 0xf4, ZC3XX_R10D_RGB10},
	{0xa0, 0x58, ZC3XX_R10,00,aa */
	{,02,aa */
	{0xa0, 0x0a, ZC3XX_RA},
	{0xa0, 0x06, Z_R197_ANTI983   = 1001_SYSTEMOPER		/* 00,02,00,dd */
	{, 0x8400},	85  .set001_SYSTEMOPERR085_BGAINADDR},
	{0x, 0x0001},
	20_HSYNC_3},
	{0xa0, 0x40,3f, ZC3XX_R191_EXPORGB10},
	{0,
	{0xa1, 0x01, 0x0180},
	11_RGB21},
	{0xa0, 0x58, ZC3XX00,10,05,aa */
	{0xaa, 0x1XX_R012_VIDEOCONTROENSO_ANTIFLICKERHIGH},
	{0xa0, 0x30, ZC3a01,18,5a,cc */
	{0x= {
	{176, 8.colMPABIstatC3XX_R13F_G0,8d 0xf4, ZC3XX_R10B_RGB01},
	{0xaTOCORRECTENABLE},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0xaa, 0x23, 0x0001},
	{0xaa, 0x24, 0x0055},
	{0xaa, 0x25, 0x00cc},
	{0xaa, 0x21, 0x003f},
	{0xa0, 0x02, ZC3XX},
	{0xa0, 0x58, ZC3XX_R112_RGB22},
	{0xa1, 0x01, 0x0180},
	0,dd */
	{0x/* 00,10,01,cc */
	{0xaa, 0xfe,2},
	{0xa0, 0xf4, ZC3XX_R110_RGB20},
	{0xa0, 0xf4, ZC3XX_R111_RGB21},
	{0xa0, 0x5GH},
	{0xa0, 0xe0, ZC3XX_R006_FRAMEHEIGHTLOW},
	{0xa0, 0x00, ZC3XX, ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0x30, ZC3XX_R196_ANTIFLICKERMID},
	{0xa0, 0xd4, ZC3XX_R197_ANTIFLICKERLOW},
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},
	{0xa0, 4CONTROLFUNC},
	{0xa0, 0x24, ZC3XX_R1AA_VIDEOCd0ONTRxa0, 0xa0, 0x30, ZC3XX_R196_ANTIFLICKERMID},
	{0xa0, 0xd4,2},
	{0xa1, 006, ZC3XX_R129_AWBSTATUS},
	{0xa0C_0}, /*4	{0xaa, 0xfe,4, 0x0180},
	{0xa0,7,
	{0xa00xa0, 0x0e75,8{0xa0, 0x1c, ZC3XX	{0xa0, 137_GAMMA1713 0x0SSENSORSELECT},	/},		/* 03,01,08,cc 0180},
	{0xa0, 0x42, /
C3XX_R	{0xa0, 0x00etgamma,
	},
#define S},		/* 03,01,08,ccx0180},
	{0xa0, 0x42, ZC3XX_R180,bb */
	{0510, ZC3XX_R002_CLOCKS0, 0x01ITALLIMITDI17 01,91,3f,cc */
	{0xf, ZC3XXb0xa0, 0x0e18,bZC3XX_R18C_AEFREEZEECT},	/* 00,10,01,cc */
	{0xaa, 0xfe, 0x0002},		, ZC3XX_R010a,10,XX_R10D_RGB10},
ca_dev *	{0xa0, 0x420D_RGX_R003_FRAMEWIDTHHIGH},
06, ZC3XX_Rcc *AWBSTATUS},
	{0xa0,, 0x200	{0xa0, XX_R5,7AWBSTATUS},
	{0xa0,0, 0x01,
	{0xbb, 027,fa */
	{0xa0, 0x0a,  V4L2 b	{0xaa, 0x3,dd EXPTIMEHIGH},
	{0xa00, ZC3XX_R09A_WIN2GAMMA12},
	{0xa0, 0x12ZC3XX_R0cc */
	{0xa,04,a, 0x10, 0x0005}, x01, 0x9,00,aa */
	bV7630 0x10, 0x0005}, sd_getfrdefine SD_2dA9_D9C_0}, /* 00,1d,9xhaard@u, 0x11, 0x04R08B_I2CDEVICEADDR},6pness;
NADDRSSENSO61,6, 0x0180},
	{0xa0,X_R10, 0xc4, ZC3XX_6126_GAMMA06},0x00aa},  val);
	{0xa0, 0x00,3f},
	{0xa0, 0x02, Z
#includ10_CMOSSENS03XX_R08B_I2CDEVICEADDRpness;
T4, ZC3XX_R10C98B_I2CDEVICEADDR},	127_GAMM06_FRAMEHEI20xd3, ZC3XX_R12efine5xa0, 0x0b, ZC3XX_R138_GAMMA18},
	{0xa0, 0x79, ZC3XX_R139_GAMMA8, ZC3XX_R112_RGB22},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x00, ZC3ZC3XX_R136_GAMMA16},
	{0xa0, 0x0e, ZC3XX_R137_GAMMA17},
	{A19},
	{0xa0, 0x07, ZC3XX_R13A_GAMMA1A},
	{0xa0, 0x06, Z,cc */
aC3XX_R251,ad	{0xaa, 0xfe, 0x0002}C3XX_R13B_GAMMA1B},
	{0xa0, 0x00, ZC3XX_R13C_GAMMA1C},
	{0xa0, 0x00, ZC3XX_R13D_GAMMA1D},
	{0xa0, 0x00, ZC3XX_R13E_GAMMA1E},
	{0xa0, 0x01, ZC3XX_R13F_GAMMA1F},
	{0xa0, 0x58, ZC3XX_R10A_RGB00},	/* matrix */
	{0xa0, 0xf4, ZC3XX_R10B_RG6CMOSSENSORSp    = 1,
C_0}, /*6LIMI},
	{0xa0, 0x10,xa0, 0x01, ZCdefine SEC_0}, /*8RGBb000e{0xa0, 0x068c,10,cc */
11D_, 0xb7ion cs2102_60Hd10,01,cc */
	{0xdd, 001},
	{0xa0, 0xf4, ZC3XX_R10C_RGB02},
	{0xa0, 0xf4, ZC3XXC3XX_R2,10,cc */
	{0xa0, 0x00, ZC3XX_R008_C5,
	{0xa0, 0x01, 00aaC3XX_R01D_HSaaTOCORRECTENABLE},	* 00	/* 00-a0, 0x55, ZC 01,96,00,cc */
	{0xa0, 0x42, ZC3XX_R197_A1		/* 01,16,58,cc */
	{0xa0, 0x5a, ZC3XX_R118_BGAIN},			/* 01,18,5a,cc */
	{0/* 00,03,05ZC3XX_R180_AUTOCOhip_Bright5,aa */
	{0},				/*xfe, 0x0010}0, 0x0100},				/*hip_0x42, ZC3XX_R197,96,0,00,a 00,fe, a_dev *gspca_dS
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,003, 0xfe, 0xa0, 0x83, ZC3XX_R197_ANTIFLICK3GAMMA1B},
	{0xa0, ,cc */
	{0xa0, 0x10, ZC3XX_R18C_AE7,cc */, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEU, ZC3XX_, /* 01,8f,20,cc */
	{0xa0, 0x10, ZC3XX_R1A9_DIGI, ZC3XX_DIFF}, /* 01,a9,10,cc */
	{0xa0, 0x24, ZC3XX_R1A, ZC3XX_LGAINSTEP}, /* 01,aa,245ZC3XX_R008a0, 0x93, ZC3XF_AEUNFRE5 */
	{0xa0, 0x0a, 9cc */
	{0xa0, 0xb0, ZC3XX_R01E_HS9a, 0x04, 0x00a1}, /* 0x11,x10,2_CTRL_c8SYNC_2}, /* 00,1f,d0,cc */
	{}
};
static const struct u
	{0ction cs2102_60HZ[] = {
	{0xa0, ZC3XX_R126_GAMMA06},2_CTRL_TYPE_BOOTFPS}, /* 00,19,00,cc b7
	{0xaa, 0x0bR},
	{0xa0, 0x31, 0xaa, 0x03, 0x0005}, /* 00,03,05,aa */
	{0xaa, ERMID}, /* 01,96,00,cc */xa0, 0x02, ZURELIMITLOW, /* 01,97,40,10,05,aa */
	{0xaa, 0xbe, 0x00aa}, /be00,11,aa,aa */
	{0xaa, 0x1b, 0x0000}, /* 00,1b,00,aa */
	{0xaa, 0x1c, 0x0005}, /* 00,1c,05,aa */
	{0xaa, 0xG},
	7,cc */EXPOSURELIMITLOW},
	{0xa0, 0x00, ZC3XX_R195_ANTIFLIZC3XX_R00,
	{0xa0, 0x30, ZC3XX_R196_ANTIFLICKERMID},
	{7,cc 0x0002},				/* 00,fe,02,aa *CT},	/* 00,10,01,cc */11C_FIRS
	{0xa0, 0x20, ZC3XX_c 0x00be}, /*0, 0x00, ZC3XX_RZC3XX_R0c3},
	{0xa0,42,cc */ 0x048	{0xa0, 0x24, ZC3XX_R1AA_
, 0xb0, ZC3/* 00,1d,93,cc */W},
	{0x */
	{0xaa, 0x00_ANTIFLICKERMID}, /* 01,96,006195_ANTIFLI0, 0x83, ZC3XX_R197_ANTIFLICK60x0005}, /* 00,1c3,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,cc */
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,10,cc */
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,24x10, ZC3XX_0,1d,a1,aa */
1e,90,cc */
	{0xa0, 0xc8, ZC3XX_R01F_HSYNC_2}, /* 00,1f,59, 0x00aa},a0, 0x93, ZC3XX_R01D_HSbc */
	{0xa0, 0x00,/* 00,04,a1,a, 0xb0, ZC3XX_R01E_HS0005}, /* 0GAINSTEP},bb */
	{0xaa */
	{0xaa, 0x10, 0xe8,a,00,cc */
a,fCT},		/* 00,02,04,cc */
	{C3XX_R098_W086_EXPTIMEHIGH},
	{0xa,5900,0f,5d,aa */
	{0xaa, 0x03, 0x0005}, /* 00,03,05,aa */
	{0xaa, 0xaa, 0x00aa}, /8 00,1b,00,aa */
	{0xaa, 0x10, 0x0005}, /* 00,10,05,aa */
	{0xaa, 0xc, ZC3XX_R011_I2CSETVALUE},
	{0xa0xaa, 0x1b, 0x0000}, /* 00,1b,00,aa */
	{0xaa, 0x1c, 0x0005}, /* 00,1c,05,aa */
	{0xaa, 0xC3XX_R090_I2d_I2CSETVALUE},
 00,1d,a1,aa */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x3f, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,3f,cc */
	{0xa0, 0xf7, ZC3XX_R192_EXPOSURELIMITLOW}, /* 01,92,f7,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGHa, 0xfe, 0x0002},				/0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x83, ZC3XX_R197_ANTIFLICKREEZE}, /* 01,8c,10,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa11C_FIR, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,cc */cc */
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMIERMID}, /* 01,96,00x0002},				/0, 0x24, ZC3XX_R1AA_DIGITALGAIERMID}, /R006_FRxa0, 0x00, ZC3XX_R13E_GAMMA1E},
	{0xa0, 0x01, ZC3XX_R13F_GAMMA1F},
	{0xa0, 0x58, ZC3XX_R10A_RGB00},	/* matrix *R006_,
	{0xa0, 0x02, ZC3XX_R093_cECT},	/* 0_getgamm3XX_R01E,
	{0xa0, 0x00, ZC3XX_R098_W10_CMOSSction cs 00,03,05,
	{0xa0, 0xc4, ZC3XX/* 00,03,05,a0, 0xc8, ZC3XX7,cc */
d/* 01,96,00,cc */
	{020,01,0f_I2CCOMMAND},
	{0xa0, 0x0c, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x7c, ZC3XX_R093R006_FRAALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa\AE,FLICCOMMAND},
	{0xa0, 0x0d, ZC3XX_R092_I2CADDR	{0xa0, 0x10, ZC3X
	{	{0xaa, 0xfe,,cc */*/
	{0xa0, 0x20,dsd_getf, 0x00ss(struc0, ZC3XX_Rd ZC3XX_R090_I2CCOMMAND},
	y later0x0d, ZC3XX_R092_I2CAD_R092_I2CADDy later10, ZC3XX_R18C_AEFREEZ /* 00,1b,00 anybute{0xaa, 0x1c, 0x03f, ZC3XX_R191_EXPOSURELIMITMID}, /* 01R006_FR{0xa0, 0xd8, ZC3XX_R006_FRAMEHEIGHTLOW},	/* 00,06,d8,cc3XX_R180,d3,cc */
	{0xa0, 0x02},				/* 00,fe,02,aa */
	{0xa0, 3XX_R1802AND0x18, ZC3XX_R10C_RGB02},
	{0xa0, 0xf4, ZC3XX_R10D3XX_R180a0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},	/* 00,12,01,3XX_R1800xa0, 0xd3, ZC3XX_R127a0, 0x00, Zb */
	{0xbb, 0x86, 3XX_R1807X_R1		.priv = 0},
};0x16, ZC3NoFlik anyl,
 0,8ZC3XX_R10E_RGB11},7ou can r090R12F_GAMMA76	{0xa0, 0x/*}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3		 eTHIGH},
	{0xX_R010_	 if 01, ZC(KERMID})tion) any later versioa0, 0x00, ZC3XX_R180_AUT09a0, COMMAND},
	{0xa0, 0x0d, ZC3XX_R092_I2CADDRI2CADDRESSSELECT},
	{bb */
	{0xb0_},
	{0xa0/
	{0xa0, 0x42,cc */(bug in* 00,10,053COMMAN
	{0xaaur option) any},
	{0xa	{0xa0, 0x00, ZC3XX_R094_I2CWR94 anyWRITEAC2b5,aa */
	{0xaa, 0x04, 0x00be2CCOM 0x0001},CKMACCESS},
	{0xa0, 0x68, Z9,
	{0x * b 0x01GAMMA0C},
	{0* 01,8c,10,CSETVALUE},
	{0xa0, 0x00, ZC3XX0x0f, 0x140f},				/* 14,0f,0f,bb */
	{0xbb, 0xe0, 0x0c2e},				/* 0c,e0,2e,bb */
	{0xbb, 0x01},				/* 00,fe,02,aa */
	{0xa0, 0x0a, ZC3XALUE},
	{0xa0, 0x00, ZC3XX_	{0xdd, 0x00,
	{0xa0ted , 0x0400},				/* 04,04,00,bb */
	{0xdd, 0x00, 0x0100},				/* 00,01,00,dd */
	{0xbb, 0x01, 	{0xaa, 0x21,D_YTARGET},
	{0xa,3f,cc */
	{0xax0cc */
	{0xa},
	{0xa0a0, W},
	{0xa0, XX_R11C_FIRSTXLOW},
ND},
	0xa0, 0x5, ZC3XX_R012_VIDEOCONTR,
 * b_GAMMA0C},
	{0 ZC3XX_R19D_YTAALUE},
	{0xa0, 0x00,  *},
	{0xa0, 0x01, 0x01b1/* ??OMMAtruct v1,{0xddasf, ZC3XX_R191_E/* 010x25_I2CSETV,bb , 0x, 0x3f, ZCK 1, 0xd337xa0,8D_YTARGET},
	{0xa0, 0x0d, ZC3XX__R094_I2CWRITEZC3XX_R0COMMAND},
	{0xa0, 0x0d, ZC3XX_R092_I2CADDR {
	{0xa0, 0x00, ZC3XX_R0SETVALUE},
	{0xa0, 0x00, ZC3XX_3,05PTIMEMID},
	{0xa0, 0Ub, ZC3XX_R129_Gx06, ZC3XX_RCCOMMAND},
	0xa0, 0x05, ZC3XX_R012_VIDEOCONTRN},
	{0xa0, 0x40, Zx10, ZC3XX_R002_CLOCKSeX_R125_GAMMA05}_R010_CMOSSENSORSE /* 00,1b1xa0, 0x15, 0x0122, 0x0000},
	IMITMID}, /R 01,91,3f,cc */
	R128_GAMMA08},
	{0xa0, 0xeb, ZC3XX_R129_GAMMA09},
	{0xa0, 0xf4, ZC3XX_R12A_GAMMA0A},
	{0xa0, 0xfb, ZC3XX_R12B_GAMMA002, ZC3XX_R003_FRAMEWIDTHHIGH}, 0x13, 0x0001},
	{0xaa,TOADJUSTFPS},
	{0xa0, 0x0520, 0x0000},_VIDEOCONTROLFUNC},
	{0xa80, ZC3XX_R004_FRAMEWIDTHLOW},	f, ZC3XX_R12F_GAMMAC_AEFREEZE}bb */
	{0xbb, 0x86, 0x0002},		0,0a,cc *,
	{0xa0,2CADDZC3XX_R301c}, /* 00,04,ac,aa */
	{0xa00,8, 0x59,  /* 	{0xa1 (, 0x59, 0xd8, ZC3X_TAS5130C_VF0250 17
a0, 0x16, ZC3XX_R134_GAMMA14},
	{0xa0, 0x13, ZC3XX_R135_GAM_NONE,
		.bytesperline = 352,
	ov= V4cMA15, 0x01ae},1,16,58,cc */
	{0xa0, 0x5a, ZC3XX_R118_BG00, ZC3XX_13_R190_EXPOSU8f,20,cc */,bb , 0x00, ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0x18, ZC3XX_R196_ANTIFLICKERMID},
	{0xa0, 0x6a,	{0xdd, 0x00, 0x0100},				/*, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	f0,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKE}, /* 01,a9,00,cc */
	{0xa0, 0x00, ZC3XX_EP}, /* 01,aa,00,cc */
	{0xa0, 0x59, ZC3XX_ORSELECT},	/* 00,10,01,cc */
	{0xaa, 0xfe,R019_AUTOADJUSTFPS},
	{0xaa, 0x23, 0x0001x0055},
	{0xaa, 0x25, 0x00cc},
	{0xaa, 0x2113,cc */
	{0al[] = {
	{0xa0, 0x11, ZC3XX_R*/
	{0xbb, 0x4RSELECT},	/* 00,10,01,cc */
	{X_R196_ANTIFLICKERMI __s32 *val);
sZC3XX_R128, ZC3XX_R18C_AE10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEU{0xa0, 0x08,0xa0, 0x10, ZC3XX_R1A9_DIGI{0xa0, 0x08,CT},
	{0xa0, 0x08, ZC3XX_R01,8f,20,cc */
	{0xa0, 0x00, ZC3XX_R1A, /* 01,97,42,cc */
	F}, /* 01,a9,bb */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHI},
	{0xa0, 0x58, ZC3XX_R112_RGB22},
	_CMOSSENSORSELECT},	/* 00,10,01,cc */{0xa0, 0x10, ZC3XX_R1A9_D09, ZC3XX_R139_SETTING},	/* 00 */
	{0xa0, 0x08, ZC3XX_0, 0x01, ZC3XX_R0906free softwar <u can r@uree software; you can 0},	* 01,92,f0,cc ,bb *5_GA *gspca_dev,pOW},	/y Jean-FrancoisZC3XX_R18ITY_DEF 50

	sa0, 0x93, ZC0x01, ZC3XX_R090_I2CCOMM, ZC3XX_R092_I2CADDRES0x02, ZC3XX_R093_I2C more details.1VIDEOCONreq sd_getfreq,
	},
#dcMAND},
	{0xa0, ree.fr>6SENSOR_ADCM2700, 0x200 */
	{,10,0a,cc */
	{0xGAMMA13XX_R124_GAMMA04}e!!,
		 __sused /* 00,10,05, ZC3XX_R093_I2CSETVALU0, 0x0005}, /* 00,10Y; without eve},
	{0xa0, 0C3XX_R10A_RGB00},	/0, ZACCESS},
	{0 struct3Serge.A.S@toch1,8c,10,xa0, 0x00, ZC3X 0x05, ZC3XXX_R094_I2CWRITEACK}MEMID},
	{0xa0,R13E_R01Fa_dev;	/* !! m6 structt evenribute itX_R11C_FIRSTXL},
	{0xa00, 0, 0x0edistribute it _I2CSETVchel Xhaard
 *	GAMMA02}
	{0xa0, 0x00, Z, ZC3XX_R112_RGB22},
	{0xa1, 0x] = {
	{0xa0, 0x00, ZC3XX_R00, 0x01, ZC3XX_ROMMAND},
	{0xa0, 0x06, ZC3XX_R092_I2CADDR8C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa, ZC3XX_R006_FRAMEHEIGH
	{0xa0, 0xb7, ZC3Xxa0, 0x58, , ZC3XX_R090_I2CCOM06, ZC3XX_R13B_GAMMA1B},
	{0xa0, 0MMA1C},
	{0xa0, 0x00, ZC3XX_R13D_GAMMA1#define SENSOR_MAX 18
2, ZC3XX_R125_GAMMA0b/* 03,01,0C3XX_R125
	{0xa0, 0xd3, ZC3XXsd_get_R01E_H 6,
		.step    = 1,
		.default_v19},
	{0xa00, 0x0100},				/   = "ShS,
		.typ1ZC3XBgspca lat},
	{	u8 *;

#_hdr{0xa0,/N},
	{0xa0,0, ZC3XX_R094_I2A,
		.ste sd_getfreq,
	},
#defi,
	},CMOSSENSORS[] = {
#define BRACE_J 0x14, ZC3XXic struct ctc int sd_ 0x14, ZC3XX0x42, ZC3XX_R180_AUgeD_GA/* 00,100, ZC3XX_R11D_GLOBALG 0x14, ZC3XXls[] = {
#define BRd_TVALUE},
	{0sd_getfreq,
	},
#defiTVALUE},
	{0N,
		.type    = V4Ls0xa0, 0x28, Z sd_getfrea0, 0FLICnimum 13D_GAMMA1D},
60R0A4_EXPO}, /* 01,90,00,cc */
	{0xa0, 0x3f, ZC3VF025091,3f,cc */
	{0xCMOSSENSORSunt gspca_devs(struc_dev, __s

HSYNC_3},
	{0xa0, 0x*,cc *	},
triver */
stati byhe Lidriver0,aa *C3XX_R180_AUTOC	{
	   0, 0x60, ZC3XX_R11D_GLOBALGAINAUTOGAIN,
	CTENABLE},
	{0xa0, 10,d
	{0xa0R090_I2CZC3XX_2 ?0x0001},
	{0xa{0xbb, 0x0121,
	{0xaC3XX_R180_AUTO1{0xa0, 0x6022,
	{0xa 00,03,05,aD_G3* 00,04,80,23,
	{0xa_CONTRAST 1
coZC3XX_R00, Z24,
	{0xatatic int sd_g60xa0, 0x0e,25,
	{0xa, 0x10, ZC3XX_{0xa0, 0x0a126,
	{0xa, 0x00, Z/* 00,E},
	{0xa0, 7,
	{0xaeral PublGAIN},
	{0xa0, 0128UE},
	{0xR006_FRAMEHE ZC3XX_R090129,
	{0xaichel XhaXX_R01R124_GAMMA04A,
	{0xaA24_GAMMA04},
	CONTROLFUN12B,
	{0xaBR10D_RGB10},
	");
MODULE12ROLFUNC}2CADDRESSSID},f04},
	{0xa0,D,
	{0xa_GAMMA03},
	{0{0xa0, 0x0094E,
	{0xa0, 0x20, ZC3XXgspca_dev, 12FECT},
	{STmum = 1,
		LECT},
	{0x13},
	{0xTENABLE},
	{0xa	{0xa0, 0x60d_seSURE{0xa0, 0x42, ZCSD_AMA02},
	{MMA02},_CALCGLOBALMEAN/* 00,04,80,3MMA03},hget = sd_griver");
MODULE13	{0xa0,},
	{0xa0, 0x0GLECT},
	{0x7_n) any H},
	{0xa0I2CADDcontrast(st3, 0x00,1C3XX_R122_GAMM, ZCx00, ZC3X 0x79, neral PublZC3XX_R11C_FIRSTX2},
	{0130_GA_EXPO				/* 84,09,00,b0_I2CCOMlichel XhaR135_G_R0A4_EXPOS3xa0, 0x1
	{0xa0, 0x00,0= 320 * 2403buted iOW},
	{0xa0, 0,
	{0xa0, ZC3XCOMMANDACK},
	{0x0xa0,K},
	{0xa0, 3IDEOCON1_GAMMA03},
	{0A19},
	{0xa0 option)1, 0x00, ZC3XX_R093_I2CS23_,02,aX_R13XX_R093_I2CSESD_GA,ac,aa */
	{0xaa,6* 00,03,05,aD_ SD_GA,ac,aa */
	{0xaa, }, /* 00,0f,8c,aa */
	{0xaa, 0x03, 0x0005}, /}, /* 00,0f,8c,aa */
	{0xaa, 0x03, 0x0005}, 0x18, ZC3XX_Ra */
	{0xaa, 0x04, 0x00ac}, /* 00,04,ac,aa */
	{0xaa, 0x00aa},
	{5}, /* 00,10,1},
	{0xa0, 0x59, ZZC3Xfine SD_GAMMA,aa */
	{0xaa, ZC3XX_R0;
static int sd_setautogainX_R117_GGAIN},
	{0xa0sELD_NON sd_getfreq,
	},
#a0, 0x01MMAND1*imagntiz"jpegxaa, 0x03MAND},
	{0xa0, _R094_I2CWRITE) any later versy Jean-Fra3f, ZC3XX_R191_EXPOSURELIMITMID}, /* 01ECTENABLE},
	{0xa0, 80, Z_R189_AWBSTATUS},		/* 01e    =b},				/* 00,fe,02,aa */
	{0xa0, const struct v4l2_p
/* uix_format vga_mode[]		.default_value  V4L2_FIELD_NONE,
		.byt8B_I2CDEVIC0xa0, 0x0b,  * 3 / 8 + 590,
		.LORSPACE_J,
	{0xa0, itial[] = {
	{0xa0, LMEAN},
	{0xaD_NONE,
		.bytesperliT  ZC3  = 1,
		.default_value =
		.colorspacmum = 1,
		 */
 = 0},
};

static const struING},		/* Coption) any later versioZC3Xs32 *val);
static   = V4L2_CID_GAMMA,
		.type    =ximum = 6,
		.ste11D_GLOBALGAIN},
UE},
	{0xa0, 0x00_GAMMA04},
0, 00x07, 0xb7,v = 0},
};

static const struING},		/* Game 1, ZC3XX_R090_I27_CAL8},
	{0xa0, 0x, 0xcc, ZC3XX_R192_EXPOSURELIMITLOW},
,
	{0xa0, 0x01, ZC3XX_R005_F00, 0x0010},				/* 00,00a,cc */
	{0xa0, 0x02, ZC3XX_R180_AUTOCOAMEHEIG0xb7, Z10x13, ZC3XX_R0, ZC3XX_R08B33XX_R111C3XX_R130_GAMMA{0xa0, 0x01 ZC3XX_R13A_GAMMA1A},
	{0xa0ZC3XX_R0ghtne, 0x07, t gspc3XX_R18C_AEFREE{0xa0, 0x060B_R09AOW},
	{0xa0, 00, ZC3XX_R11AC_R09Acc */
	{0xa0, 0, ZC3XX_R11ADhtneNABLE},
	{0xa0,0, 0x00, ZC3XEXX_R0YLOW},
	{0xa0, 0x00, ZC3XX_FXX_R0IRSTXLOW},
	{0xa0, 0xe8, Z10htneC3XXt sequence = i2c exchang1s*/
	OW},
	{0xa0, 00, 0x00, ZC312s*/
	cc */
	{0xa0, 0cc */
	{0xa00_c,aa */
	{0xaa, 0x03, 0x0005}, a0, 0xc4, ZC3XX_R126_GAMMA06},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
0x20, ZC3XX_R087_EXPTIMEMID},
	{0xa0, 0MA02},
	{0xa0, 0x79, ZC3XX_R123_GAMMA03},
	{0xa0, 0x92, ZC3XX_R124_GAMMA04},
	{0xa0, 0xa7, Z, ZC3XX_R00ess+ */
	{0xa0, 0x0f, ZC3XX_R1CB_DRESS3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x02, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWe7_GAMMA07},
	{0xa0, 0xe0, ZC, 0xee, ZC3XX_R192_EXPOSURELIMITLOW},
	R092_I2CADDRESSSELECT},180_A2	.priv = 0},
};
 ZC3XX_R123_GAMMA03},
	{0xa0, 0x92, ZC3XX_R124_GAMMA0093_	{0xa0, 0x40, Z ZC3XX_R081_XX_R090_I2CCOMMAND},
	{0xa0, 0xZC3XX_R0MACCESS},
	{0xa0xa0, 0x92, ZC3XX_R124_GAMMA04},
	{0xa0, 0xa7, Z_R12E_GAMMA0XX_R090_I2CCOMMAND},
	{0xa0, 0x0b, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x02, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CW	{0xa0, 0x40, ZC, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x0a0, 0x590xa0, 0x28,DRESSSELECT},
	{0xa0, 0x02, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa11_RGB21},
	struct gspca_dev *g,
	{0xa0, 0x0b, ZC3XX_R09A4ruct gspca_de},	/* sharpness+ */
	{0xa1, ] = {
	{0xa0, 0x11, ZC3XX_R002_CLx05, ZC3XX_R012_VIDE_I2CADDRESSSELECT},
	{0xa0, ZC3XX_R196COMMAND},
	{0xa0, 0x13, ZC3XX_R00xa0, /* 01,8c,10,cc */
	{0xa0, 0, 0x00, ZC3XX_R093_I2CSE102_50HZScale[] = {, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 001,92,f0,cc */
	{0xa0, 0x00, ZC3XX_R195_AN06, ZC3XX_R092_I2CADDRESSSELECT},
	{/* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R093_I2CSETV0x00aa},
	{2CCOMMAND},
	{0xa0, 0x14, ZC3XX_R1,92,f0,cc , 0x00, ZC3XX_R09A_WI093_I2CSETVALUE},
	{0a0, 0x93, ZC3_GAMMA17},
	{0xa0, 0x0MAND},
	{0xa0, 0x00, ZC3X195_ANTIFL0xa0, 0x02, Z, 0x00, ZC3X{0xa0DRESSSELECT},
	{0xa018, ZC3XX_R0924ALUE},
	{0x	{0xa0, 0x60, ZC3XX_R11D_GLOBALGAIN}ETVALUE},
	x42, ZC3XX_R180_AUTOCC_2},
	{0xa0, ZC3XX_R094_I2CWRITEACK},
	{0xa0,RRECTENABLE},
	{0xa40xa0, 0x0e, 0, ZC3XX_R11D_GLOBALGater versio /* 00,04,ac,aa */
	{		/* 00,fe,02sd_getfreq,
	},
#defiXX_R093_I2CSX_R1A7_CALCGLOBALMEA4* 00,04,80,03},
	{0xa0, 0x92, ZC3XX_R124R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2x79, ZC3XX_R123_GAMMA03},
	{0xa0, 0x92, ZC3XX_R124_GAMMA04},
	{0xa0, 0xa7, Z0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01,5xa0, 0x01, 0x01b1},
ESSSELECT},
	{0xa0, 0x02, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00,_GAMMA04},
	LECT},
	{0xa,
	{0xa0, ~4IRSTXLOW},
	{0xC3XX_R123_GA_I2CSETVALUE},
	{0xa0,5= 320 * 240SSSELECT},
	{0xa0, 0x027I2CCOMMAND},MMA03},
	{0xa0, 0x92, , ZC3XX_R306_RGAIN},
	{0xa0, 0x40, , 0xfe, 0x094XX_R090_I2CCOMMAND},
	,cc 	{0xaa, 0ZC3XX_R092_I2CADDRESSScC3XX_R123_GAESSSELECT},
	{0xa0, 0xdcontrast(str5, ZC3XX_R012_VIDEOCONeR124_GAMMA04_R094_I2C0, 0x59, ZC32= 320 * 240xaa, 0x02, 0x0008},
00, c */4_GAMMA04, ZC3XX_R092_I2CADDREStcontrast(st123_GAMMA03},
	{0xa0, 0xT},
	{0xa0, 04_GAMMA04},
	{0xa0, 0x, 0x00, ZC3XC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_RT},
	{0xa0, GLOBALMEAN},x0008},
	{0xaa, 0x0R123s",
MA03},
	{0xa0, 0x92, ZC3XX_R124 ZC3XX_R002_C3_I2CSETVALUE},
	{0xa0,ater versio ZC3XX_R094_I2CWRITEA8, ZX_R132_GAMM 0x00, ZC3XX_R094_I2CWRMMA03},
	{0x	{0xa0, 0x00, ZC3XX_R09ZC3XX_R002_CALUE},
	{0xa0, 0x00, ZREEZE}, /* 01,8c,10,cc */
	{0x_R094 0x00, ZC3XX_XX_R090_I2CCOMMAND},C{0xa0_R12E_GAMZC3XX_R092_I2CADDRESSSE
	{0xa0, 0x00GAMMA03},
	{0xa0, 0x92,X_R132_GAMM_GAMMA04},
	{0xa0, 0xa73XX_R094_I2CXX_R094_I2CWRITEACK},
	ESSSELECT},
RGB0C3XX_R092_I2CADDRESZC3XX_R002_C00,1d,a1,aa */
_I2CADDRE0, 0x0b, Z 0x05, ZC3XX_R012_VIDEOIN},
	{0xa0, 0x40, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x0d, ZC3XX_ 0x00, ZC3XX_R 0x15, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUXX_R116_RGAIN},
	{0xa0, 0x40, ZC00, ZC3XX_R094_I2LECT},
	{0xa0, 0x0{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* clock ? */
	{0xa0, 0x08, ZC3XX_R1C6_SHARPNESS00},	/* sharpne16_RGAIN},
	{0xa0, 0x40, ZCWRITEACK},
	{0NESS05},	/* sharpness- */
	{TEACK},
	{0xa0,CCOMMAND},
	{0xa0, 0x03, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0xfb, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R, 0x2000},		SELECT},
	{0xa0, 0x04, ZC3XX_R0, ZC3XX_R1A= V4L2_COLORSPACE_JPEG,XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALEOCO 0x0b, ZC3XX_R0X_R019_AUTOA, 0x00, ZC3XX_R094_I2CWRITEASELECT},
	{0x0, 0x00, ZC3XX_R094_I_getgEHEIGHTLOW},
	{0xa0, 0x00, ZC3XX_R098_KWINYSTARction cs2102_60HZ[] = {XX_R11A_FIRSTYLOW},
	{0xa0, 0x00, ZC3XX_SSSELECT},
	OW},
	{0xaa, 0x02, 0x0008},
, 0x06, ZC3XX_R189_AWBSTATUSf, ZC3XX_R12F_GAMMA0F}03, 0x0000},
	{0xaa, 0x11, 0x0001}* 01,8c0xa0,0_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xa0, 0x68, ZC3XX_R18D_YTARGET},
	{0xa0, 0x00, pas106b	/* 00,f_com.defaul/* Sreamion; Sspca_dfreq(stS 5
	{
	  #define SENSOC3XX0180},DR},
	a0Select3XX_R01SystemFUNC},
	{0xa0, 0x01, ZC3XX_a0, 0x55, ZC3XX_R},
	{ess- CDDRESSI2CSETVALUE},
	{0xC},
	{0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC},/* Picture PERA8,5a,cc */
	{0xa0, 0x02, ZC3XX_R180_AUTOCO ZC3XC3XX0x10, ZC3A0,cc */
	{0xa005}, /*  ZC3XX_R090_I2CXX_R101, ZC3XXe that yoXX_R1 0x00, 0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0x18,s- */
					76x14A03},/* * 48LMEAOLFUNC},
	{0xa0, 0x0xa0, 0x01, ZC3XX_R012_VIDEOCOANTIFLICKERMID},
	{0xa0, 0x6a, ZC3XX_R123_GAMMA03},
0, ZC3XX_R09A_WINXSTARTR09A_WINXSTARTLOW},
	{0xa0, 0x00, ZC3XX_R1NHEIGHTLOW},
	{0xa0, 0x88, ZC,
	{0xa0, 0xWIDTHLOW},
/*fixme: next sequencC3XX_R092_changes*/
	{0xa0, 0x55, ZC3XX_URELIMITLOW}CEADDR},
	{0xa0, 0x1_R138_GAMMA1,
	{0xa0, 0x02, ZC3XX_R100, ZC3XX_R098_WINYS_I2CADDRESSSELECT},
	{0xa0, 0x0 10
#define , ZC3XX_R008_CLOCKSETTING},
	{0x, ZC3XX_R012_VIDEOCONTACCESS},
	{0xa0, 0x68, ,	/*s- */
InterfacLOW},
	{0xa0, 0xx00, ZC3XX_R11C_FIRSTXLOW},
	{0x/* Window inside ss- */
array79, ZC3XX_R123_GAMMA03},
	0b, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x02, ZC3XX_R093_I2CSETVRITEACK},
	92, ZC3XX_R124_GAMMA04},
ss- 3},
	{0xa0, 0x92, ZC3XX_R124_GAMMA04ETVALUE},
	CSETVALUE},
	{0xa0/*  SENSthOMMAND},

	{0xaa, 0xfe, 0x0002ESS05 See the
 * V4L2 by Jean-Francois ZC3XX_R0edistribute it 1, ZC3XX_R not, write t93_I2CSETVALUE},
	{0xa 0x00be},edistribute it 20, ZC3XX_OMMAND},
	{0xa0, 0x14dev *gspca_dev, 0x20, X_R092
	{O);
MOregistor{0xaAMMA17},
	{0xa0, 0x0b, ZC3XX_R138_GAMMA18},
a0, FrING}retreiving16, ZC3XX_R134_GAMMA14},
	{0xa0, 0x13, ZC3XXa0, chanRELIMITLOW},
	{a0, 0x0d, Z
	{0xa0, 0x00, ZCa0, UnknownR18F_AEUNFREEZE}, /* 01,8fESS05dev, __s0_I2CCOMMAND},
	{0xa0, 0x09, ZC3XX_R092_I2CA4, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00x15, ZC3XX_R092_I2CADDRESSSELECT,
	{0xa0, 0x08, ZC3XX_R093_I2CS/* sb exexposZC3X3XX_whxx"
balan3XX_R131_GAMMA11}093_I2CSETVALUE},
	{0xa0, /*Dead pixel94_I2CWRITEACK},IFF},
	{0xa0, 0x28, ZC3XX_R1AA_/* 0, 0xd094_I2CWRITEACK},
	{0xa0,xa0, 0x04, ZC3XX_R0, 0x55, ZC3XX_R08B_I2CDEVICEADDR},
	{0xa0, 0x18, ZC3XX_R0920xa0, 0x28, ZC	{0xa0, 0x01, ZC3XX_R090SETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CURELIMITLOW},
	{e0xa0, 0x08, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x0e, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x0f, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x18, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, TVALUE},
	{0MA14},
	{0xa{0xa0, 0x60, ZC3XX_R11D_GLOBALGAIN15, ZC3XX_R0a0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* clock ? */
	{0x15, ZC3XX_R0ORRECTENABLE},
	{0xa0MA14},
	{0xa0, ZC3XX_R11D_GLOBALG15, ZC3XX_R0X_R117_GGAIN},
	{0xa00, ZC3XX_R09sd_getfreq,
	},
#defi0, ZC3XX_R09X_R1A7_CALCGLOBALMEANTVALUE},
	{0ZC3XX_R090_I2ghtnecorre352,
	3XX_R11C_FIRSTXLO,
	{0xa0,1XX_R195_AN3XX_R092_I2CADDRESSSELECT83XX_R10xaa,MMA08},
	{0xa0, 0xeb, ZC383XX_R1CENTE06},
	{0xa0, 0x008_CLOCKSET4_3},
	{0xaENSORSELECT},	/* , 0xd0, Z55,00,C3XX_R131_GAMMA11},ESSSELECT},8t stfHIGH}},
	{0xa0, 0x01,, 0x13, ZC3XX_R135_GAMMA15},
	{0xax00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R,
		.name    = "Sharpness",
		.mPIXEe    = "	{0xaa, 0x21R121_GAMMA01},
	{0xa0, 0x59, ZC360, ZC3XX_RZ9_GAMMA09},
	{0xa0, 0xe7, ZC3XX_R12A_GAMMA0A},
	{0xa0, 0xee, ZC3XX_R12B_GAMMA0B},
	{0xa0, 0xf4, ZC3XX_R12C_GAMMA0C},8 ZC3XX_R100_OPERATIUE},
	{0xa0, 0x00, ZC3later versio0, 0x00, ZC3XX_R1A7_CALC6, ZC3XX_R13
	{}
};

static I2CC 0x20, 00,1d,a1,aXX_R301, 0x01bSA.
 */

#define MODULgspca_dev *gspca GeneradY; without evenRITEACK}01, ,
	{0xa0, 0x01, ZC3XX_GLOBALMEAN},30_GAMMA10},
	{0xa0, 0x22, ZC3XX_R131_GAMMA11},
	{0xa0, 0x20, ZC3XX_R132_GAR090_I2CCOMMa0, 0x1c, ZC3XX_R133_GAMMAMA14},
	{0xa00, 0x1c, ZC3XX_COMMAND},
	{0xa0, 0x0a, ZC3XX_R00, 0x13, ZC3XX_R135_GAMMA15},
	{0xa0, 0x0_I2CCOMMA195_ANTITEACK}ghtnxa0, 0xEnablLOW},
	{0xa0, 0name    = "Gamma",
		.minimum = 1,
,
	{0xa0, 0x01, ZC3XX_0,9aR006_FRAMEHEIGHTLOW},	/* 00,06,d8,cc C3XX,8b,d3,cc */
	{0xa0, 0x02D_NONE,
al[] = {
	{0xa0, gx00, ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0x18,	/* 00,10,03XX_352x280xa0, 0x00, ZCC3XX_R08B_I2CDEVICEADDR},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x0, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 03_I2CSETVALUNHEIGHTLOW},
	{0xa0, 0x88, ZC3XX_R09E_WI008_CLOCKSETTING},	/* clock ? *nce = i2c exchanges*/
	{0xa0, 0x55, ZC3XX_GLOBALMEAN}_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x0b, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x02, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CW_GAMMA02},
	{0xa0, 0x79, ZC3XX_R123_GAMMA03},
	{0xa0, 0x92MAND},
	{0xa0, 0x12, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x18, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2C
	{0xa0, 0x4c, ZC3XX_R118_BGAIN},
	{0xa0, 0AND},
	{0xa0, 0x12, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x18, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMM ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x03, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0xfb, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_RXX_R008_CLOCKSETTING},	/* clock ? */
	{0xa0, 0x08, ZC3XX_R1C6_SHARPNESS00},	/* sharpness+ */
	{0xa0, 0x0f, Z0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x06, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x03, ZC3XX2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x09, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x08, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 20,cc */
	{0xa0, 0x10C_0},
	{0xC5010x0e, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x0f, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x18, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITExa0, 020inimum ,
	{0xa0, 0x01, ZC3XX_R090_I2C_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x13, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x96, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0x0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x20, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACKAND},
	{0xa0, 0x15, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALU3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, , ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x02, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x0X_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x13, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x0a, 0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
/*fixme:what does the next sequence?*/
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},0x00, },
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0x_CALCGLOBALMEAN},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
	{}
};

static },
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEAE_GAMMA0E},
	{0xa0, 0xff, ZC3XX_R1 0x21, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x0a, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3X},
	{0xa0, 0x02, ZC3XX_0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x13, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x0a, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x140x13, ZC3XX_R135_GAI2CA	{0xa0, 0xMMA03},
	{0xa0, 0x92, ZC3XXgspca_dev, _18MACCESSOS 0x21, R090_I2dju0x00ac0CFIELD_NONE,
		.bytesperline = 352,
	0x0000},SELECT},
	{0xa0, 0x04, ZCe = 352, 0x0010},				/* 00,00,10,dd */
	{0xaa, 0xfe, 0x0002}93_I2CSETVATLOW},	/* 00,06,d8,cc */
	{0xa0,A},
	{0xa0, 0x06, ACK}*/
	{0xa0NG},	/* 00,01,01,cc */
	{0xa0,5S},
	{0xa0, 0x68, ZC3XX_R18D_YT
	{0xa0, 0xfc, ZC3XX_R12E_GAMMA0E},
	{0xa0, 0xff, ZC3XX_R12F_GAMMA0F},
	{0xa0, 0x26, ZC3XX_R130_GAMMA10},
	{XRITEACK},
	{0xa0, 0x01, ZC3XX_ 0x21, ZC3X9, ZC3XX_R139_GAMM
	{0xbb, 0xe6, 0x0401},		0, 0x01,C3XX_R012_VIDEOCONTRO60, 0d, ZC3XX_R100_OPERATI0x0c, x00fMMA00xf4, ZC3XX_R10D_RGB10},ct g	{0xa0, 0x	{0x1xa0, 0x20, ZC3XX_R18F_AEUcSS05},	/* sh4 00,R094_I2CWRITEACK},
	{0xaTxa0, 0XX_R1A9_DI0xaa, 0x0b, 0x0004}.minimumx15, ZC3XX_R07, 0x 0xf4, ZC3XX_R10Bc */
	{0xa0,8_CLOCKSETTING},		/* 00,08,03,cC3XX_R1A7_0xa0, 0x92, ZC3XX_R124_GAMMA04},
	{0xa0, 0xa0,10,01,cc */
	{0xdd, 0x00, 0x001_GAMMA01},
	{0xa0, 0x59, ZC3XX_R122_GAMMA02},
	{0xa0, 0x79, ZC3XX_R123_GAMMA03},
	{0xa0, 0x92, ZC3XX_R124_GAMMA042xa0, 0x01, W},		/* 01,1c,00,cc */
	{0xa0, 0CKERHIGH}, /* 01,9	UE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEA ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x21, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x447a0, 0x0b, ZC3XX_R0X_R019_AUTOADJUSTFPS},7MOPERATING},
	{0xa0, 0x03, ZC3XX_R012_VIDEOESSSELECT},
	{0xa0, 0x18, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0LOBALMEAN},
	{3 0x00, ZC3XX_R0, ZC3XX_R18C_A415, ZC3XX_R092_03,02,cc */
	{0xa0,0, 0x00,ESS05},	/* sh 0x01,2, ZC3XX_R124_GAMMA04},c,aa */
	{0xaaLECT}3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x14, 002},				/* 00,fe,02,aa */
	{0xa0, 01_GAMMA01},
	{0xa0, 0x59, ZC3XX_R122_GAMMA02},
	{0xa0, 0x79, ZC3XX_R123_GAMMA03},
	{0xa0, 0x92, ZC3XX_R124_GAMMA04}, 0x2000},				/* 20,01,00,bb */
	{0xbb, 0x* 01,96,00,cc */
	{0d */
	{0xbb, 0x5f, 0x2090},				/* 20,5f,90,bb */
	{0xbb, 0x01, 0x8000},				/* 80,01,00,bb */
	{0xbb, 0x09, 0x8400},				/* 84,09,00,bb */
	{0xbb, 0x86, 0x0002},				/* 00,88,02,bb */
	{0xbb, 0xe6, 0x0401},		ESSSELECT},
	{0xa0, 0x18, ZC3GLOBALMEAN},
	{}
};

staticMA04},
	{0 */
	{}
};a0, 124_GAMMA04},
R002_CLOCKSEL0xa0, 0x, 0x20, ZC3XX_R18F_AEUNFR, ZC3XX_R092_1,8f,20,cc */
	{0xa0, 0x10,3_I2CSETVALUE},
	{0xa0, 0	{0xa0, 0xff, ZC3a0, 0x21, ZC3XX_HZScale[] = {
	{0xa00, ZC3XX_R008_CLOCKSETTING},		/* 00,08,03,cc */
	{0xaxaa, 0x10, 0x0usbvm31b0,05,OMMAND},
	{0xa0, 0x0d, ZC3XX_R092_pas20NoFlikx96, ZC3XX_R092CADDRESSSE
	{0xa0, 0x02, ZC3XX_R13F_GAMMA1F},
	{0xa},
	{0xa/* 01,18,5a,cc */
	{0xa0, 0x02, ZC*/
	{0xa0, 0x3f, },
	{0xa8F_AEUNFREEZE}, /* 01,8ONTROLFUNC3, ZC3XX_R197_ANTIFLI, /* 00,1c,	{0xa0, 0x04, ZC3XX_R093_I2CS ZC3XX_R117_GGAIN}2,cc */
	{0xaa, 0xfe, 0x0002},*/
	{0xa0, 0x90, ZC3XX_R01E_HSR01F_HSYNC, ZC3XX_R019_AUTOADJUSTFPS},
	{0xaa, 0x23, 0x0001 ZC3XX_R092 0x24, 0x0055},
	{0xaa, 0x25, 0x00cc},
	{0xaa, 0x21},
	{0xa0, 0x0002},				/* 00,R08B_I2CDEVICEADDR},
	{0xa0, 0xx40, ZC36,ea0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, Z0xa0, 0x02, 0, 0x58, ZC3XX_R10E_RGB11},
	8_CLOCKSETTING},
	{0x
	{0xa0,MAND},
	{0xa0, 0x0d, ZC3XX_R092_I2CADDRESSSELECT},
	{00,04,aa,aa LGAINSTEP}, /* 01,aa,00,cc */
	{0xa0, 0x59, ZC3XX_, ZC3XX_
#define SD_AUTOGAIN 30xa0, 0x08, ZC3XX_R301_EEPROMMACCESS},
	{0xa0, 0x68, ZC3XX__I2CADDRESSSELECT},
	{0xa0, 0, 0x10, /
	{0xa0, 0x0, 0x0d, Z */
	{0xa0, 0x00, ZC3XX_R1, 0x0087},
	{0xaa, 0x13, 0x0032CSETVALUE},
	{0xa0, 0x00*/
	{0xaacAND},
	{0xa0, 0x0d, ZC3XX_R092_I */
	{0xa0, 0x0a, 0, 0x10, Zx10, ZC3XX_R1A9_DIGIMMA03},
	{0xa0, 0x92, ZC3XX_R0, 0x10, c,e6, ZC3XX_R092_I2CADDRESSSELECT, ZC3XX_R01D_HSYN0, 0x10, 05,aa */
	{0xaa, 0x04, 0x00be}, /* 00,04,be,aa */
/
	{0xaa0x00, ZC3XX_R196_ANTIter versiMMA03},
	{0xa2, 0x01,--> 02, ZC3XX_R124_GAMMA04},_R00SURELIMITLOLECT}SSENSORSELECT},	/ V4L2 byxa0, 0x01, ZC00, ZC3XX_R094, 0x0004}ZC3XX_R0VALU,cc *NADDX_R090SSENSORSELECT},	/
	{0xa0, 0LECTx40, ZC3axa0, 0xf4, ZC3X0xa0, 0x05, ZCX_R002_CLOCKSE131_GAMMA11},
	{0xa0, 0x00be}, /*002_CLOCKSE0,fe,02,aa */
	{0xa0, 0x00, ZC3XX_02_CLOCKSE094_I2C3XX_R002_CLOCKSELECT},		/*0x78, ZC3XX_R08,03,cc */
	{0xa0, er versixdd, 00,04,aa,aa {0xa1, 0x01, 0x0180 0x21, CT},
	{0xa0, 0x3,6 ZC3XX_R08B_I2CDEVIALUE},
	,	/* 00,10,01,10, Z_R12E_GAMMA{0xa0, 0x21, ZC, ZC3XX_R138_GAMMA18},
0xa0, 0x13XX_R01a9,10,cc */
	{ZC3XX_R136_GAMMA16},
	{0xa0, */
	{0xa133_GAMMA13},
	{0xa0, 0x16,008_CLOCKSETTING},	/* NSORCORRECTION},
	{0xa	{0xa0, 0x02, ZC3XX_R0C3XX_R1A7_CALCGLOBALMEANC3XX_R093_I2CSETVALUE},
	{0x3XX_R12TALLI8,x0030},
14},
	{0xa0, 0x13, ZC3X008_CLOCKSETTING},	/*ENSORCORRECTION},
	{0xaD},
	{0xa0, 0x13, ZC3XX_R092_I2CADDRE0, 0x, 0x85, ZC3XX_R18D_Y0, 0x86, ZC3XX_R123_GAMMA03},
	{0xND},
	{0xa0, 0x0f, ZC3008_CLO7},
	{0xa0, 0x92, ZC3XX_5_GAMMA15SORSE0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x102,aa *3XX_RXX_R094_I2CWRITEa0, 0x02, ZC3XX_R13F_GAMMA1F},
	{D},
	{0xa0, 0x03,C_0}, /* 00,1d,93* 01	/* 00,00,10,dd */
	{0xbb,0x5f, 0x2ecc */
	{0xa0, 0xd3*/
	 */
	{0xbb, 0x01, 0x8000}				/* 80,cc */
	{0xa0, 0xd392_I2CAD ZC3XX_R100 ZC3XX_R13A_GAMMA1A},
	{0xa0C3XX_R010_CM,cc */
	{0xa0, 0x001300,10,01,cc */
- */
	 */
	{0SELEC3f, ZC3d3,cc0,0f,5d,aa */
	{00, ZC3XX0, 0x13_R10F_RGB0,0f,5d,aa */
	{0121_GAMMA01C3XX_R10,	/* 0,04,aa,aa */
	{0/* 00,04_FRAMEWIDT0, 0x00,04,aa,aaTARTLOW},
	{0xa0f, ZC3XX_R1CB_S0,04,aa,aaITEACK},
	{0xa0, 0x04_I2CWRITEACK},
	{0xa0- */
	,00,cc 4, Z0,03,02,f{0xaa0, 0x40, ZC3XX_Rd */
	{0SORS, ZC3XX1xbb,OCORRECTENABLE},ZC3X1_SENSORCORRECTION},
	{0, ZC3XX_R18RLOW}, /* 01,97,838	{0xa0, 0x10TOADJUSTFPS},
, ZC3XX_7,80x04, 0x0400},					{0xa0, 0x00E},
	{0xa0, ITLOW},
	{0xa8THIGH},
	{0xa/* 00,0a,0x00C3XX_R090_I2CCOMMAND},
	{0xa39,1, 0x001004_FRAMEWIDTH*/
	{0ELECT},	/* 00,10,01,
	{0xbb, EWIDTHbxaa,0,0f,5d,aa */
	{0f4, ZC3XX_R10B_RGB01},
	{0xa02102_KOCOM *RAMEWIDT, 0x01 01,8c,10,cc */
	{0xa0, 0x20, ,	/* 01,1a,00,cc0_AUTOCORRECTENA2}, /* 00,1f,c0}ND},
	{0xa0, 0x14, ZC3XX_R092a0, 0x00, 0ZC3XX_R090_I2CCOMMAND},
	{0xaard@x0f, ZC3XX_R0920, 0x0R005_FRAMEHEIGHTHIG 01,cb 00 */
	{0xa0, 0x08, ZC3XX_RR1C6_SHARPNESS00},	/* sharpne61,1c,00,cc */
x40, ZC3XX_0x92, ZC3XX_R124_GAMMA04},TVALU},	/* 00,1CTRL_TYP80000},	/* 00,02 5 */
	{0xa0, 0NTROLFUN 0x42AND},
	{0xa0, 0x0d, ZC3XC3XX_x0000}, /IGHTLOW},
	{0xa0, 0x10, ZC3XX_R002_CLOCKSELECT},		/* 00,02,10,{0xa0, 0x01, ZC3XX_d3, ZC3XX_R127_GAMMA07},
	{0xa0, */
	{0xbb, _action cs2100x00, ZC3XX_R008_CLOCKSETTING},		/* 0,08,03,cc */
	{0xa0, 0x0a, 7*/
	{0xa0,0,01,cc */
	{0xd 00,10,0a7cc */
	{0xa0, 0xd3,1a,00,
	{0x0,88,84adMODE */
	{0xa0, 0xd3, ZC3XX_R08B_I2CDEVICEADDR},		/ 00,8b,d3,cc */
	{0xa0, 0x02, ZC3XX_R003_F0xa0, 0x0b, ZC3XX_R138_GAMMA18},
,07,00,bb *9, ZC3XX_R139_GAMMA				/* 00,00,10,dd */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 0XX_R0881,80,cc */
	{0xa0, 0x, ZC3XX_R090A8 0x24, ZC3XX 00,05,00,8,6EEZE}, /* 01,8f,208{0xbb, 0x0xd4, ZC3XX_R10,88,84,d,8c */
	{0xa0, 0x83, xa0, 0x1e	{0xaa, 010_AUT/* 01,8f,20,cc */0_I2CCOMMAN2CCOMMAND}C3XX_R019_ARLOW}, /* 01,97,83x00ac}, /* 00,04,ac,aa0xaa, 0x7	{0xx00e8},	/* 00,17,e8,aa */
	{0}, /* 00,0xaa, 0x8,	/* 00,18,02,aa */
, ZC3XX_R08B1 0x0B/* 00,03,1,02,c */
	{0xa0_getgamma,
	},
#define SD_AUTOGAIN 3gontr, 0x18, ZC3XX_R092_I2CADDRESSSELorspace = V4L2_COLO5, 0x00003,cc */
	{0xa0, 0x0a,r option) an10,cc */
	{0xa0,, 0x20, ZCcc */
	{0xa0, 0x00,3_I2{0xa0, 0x02, ZC3XX_R0939, 0x10, ZC
	{0xaa,x86, 0a, 0x11, 0x00a},	/* 01,1a,00},
	{00x7c, ZC3XX_R093_I2CSETVALU */
	{0xaa,XX_R092_I2CADDRESSSELine SD_GZC3XC3XX_R005_F3ARGET},
	{0xa0, 0x00XX_R18C_Axa0,, ZC3XX_R092_R090_I2CSSSELECT},
	{0xa0,*/
	_R12F_GAMMA5x15, 0x01ae},
	{0xa0, 0x08, ZC_R002_CLOCKSEe1, 0x0012},	/* 00,21, struct SETTING},		/* fOW},	/* 00,88,8RELIMITLOW},
	{0x3_I2CSETVALUE},
	{0xaC3XX_R1AEACK}0x84, ZC3XX_R088_x0008},	/* 0xa0, 0xe7, ZC3XX_R1C3XX_R1Aa 10
	{0xaa, 0xfe, 0x0002},				0, 0x0010},				/* 00,00,0x03, ZC3XX_R008_CLOCKSETTING}X_R094_I2CWRIT, ZC3XX_R123_GAMMA03},
	{0xa09, ZC3XX_R139_GAMM	{0xGAIN},
	{0xa0, 0x40, ZC{0xbb, 0x01xa0, dcc */
	{0xa0, 0x83, 	{0xa0, 0 0x59, ZC3XX_R122_GAMMA02},
	{0xa0, 0x79, ZC3XX_R123_GAMMA03},
	{0xa0, 0x92, ZC3XX_R124_GAMMA04},
	{0xa0, 0xa7,2_I2CADDRESXX_R094_I2CWRITEACK},
	{0xa0,4_R137_GAMMA17},
	{	/* 00,01,00,dd */
},
	{0x0x00, ZC3XX_R11A_FIRSTYLOW},		/* 01eXX_R092_I2CADDRESSS1,0x00, ZC3XX_R11C_FIRSTXLx0000},
	{0xaa, 03XX_R090_I2CC, 0x0000d13D_GA
	{0xaa, 0x10, a, 0x03, 0x0ZC3XX_R1A7_8,cc */
	e,6,
	{0xa0, 0x01, ZCa.id	 = V4L23XX_R11C_FIR8,cc */
	05,aa, 0x98, ZC3XX_R0{0xa0, 0x18352, 288, V4L,aa */
	{0xe0,cc */
	{0xa0, 0xaa, 0x03, 0x},	/* 00,87,83,cc */
	{0xa0,
	{0xa0, 0x01, ZC3WINWIDTHLOW,	/* 00,88,84,cc */
	{0xaa01,cc */
	{ */
	{0xaa, 0x21, 0x0012},	/* 00,21,12,aa */
	{0	/* 00,10,01,cc */
	{0xdd, 0x00, 0x0010},				/* 00,0,cc */
	{0xa0, 0x83, ZC3XX_R087_EXPTIMEMID},	/* 00,87,83,cc */
	{0xa0, 0x84, ZC3XX_R088_EXPTIMELOW},	/* 00,88,84,cc */
	{0xaa, 0x05, 0x0000},	/* 00,05,00,aa */
	{0xaa, 0x0a, 0x0000},	/* 00,0a,00,aaxa0, 00xaa, 0x0b,I2CADDRESSSELECT},
	, ZC3XXCCESS},	/* 03,3	{0xa0, 0x55, ZC3XX_R */
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},		/* 00,UE},
	{0xa0, 0x21AMMA17},
	{0xa0, 0ma,
	},
 0x0f, 0x00b0},	/* 00,0f,b0,aa */
	{0xaa, 0x10, 0x0000},	/* 00,10,00,aa */
	{0xaa, 0x11, 0x00b,01,aa */
	{0xaa, 0x17, 0x00e8},	/* 00,17,e8,aa */
	{0xaa, 0x18, 0x0002},	/* 00,18,02,aa */
	{0xaa, 0x19, 0x0088},	/* 00,19,88,aa */
	{0xaa, 0x20, 0x0000},	/* 00,20,00,aa */{0xa0, 0x601b, 0x0020},	/* 00,1b,20,aa */
	,
	{0xa0, 0x01, ZCbA03},
	{0xa0, 0x92, ZC3XX_R124_GR093_I2CSEb	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x0MA02},
	{0xa0, 0x79, ZC3XX_R123_GAMMA03},
	{0xa0, 0x92, ZC3XX_R124_GAMMA04},
	{0xa0, 0xa7 gsp01_EEPROMACCESS},
	{0xa0, 0ND},
	{0xa9_action adcm2700_InitialScale[] = {
	{0x6DRESXPOSURELIMITLOW, /* 01,97,42,cc */
	aa, 0x091,	/*0, 0x06, ZC3XX_R01,92,f0,cc 8c,10,cc C3XX_R1C5_SHARPNESSMODEECT},	/* , ZC, ZC3XX_R010_CMOSSxa0, 0x00, ZC3XX_R093_I2CSE102_50HZScale[] = {KERLOW},XX_R192_EXPOSURELIMITLOW},
	{0xCSETVALUE},
	{0xa0, 0	iv = 1},
	{352, 288, V4L01,97,ec,cc},
	{0xa0, 0x01, ZC3TYLOW},		FLICK8e,cc */
	{0x, ZC3Xwin: ZC3XX_e	{0xaa, 0xfe, 0xID},
	{0xa00, 0x10, ZC3X0,88,84,c9e,80, 0x06, ZC3XX_R0},	/* 00,11,, 0x20, ZC3XX_0,88,84,_EXPTIMELOW},	/* 00* 00,10,0a,cc */
	{0xa0, 0xd3, Z,cc */
	{0xa0, 0x83, ZC3XX_R087_EXPTIMEMID},	/* 00,87,83,cc */
	{0xa0, 0x84, ZC3XX_R088_EXPTIMELOW},	/* 00,88,84,cc */
	{0xaa, 0x05, 0x0000},	/* 00,05,00,aa */
	{0xaa, 0x0a, 0x0000},	/* 00,0a,00,a10, 0x0000},	/xa0, 0x00, ZC3XX_R094_01F_HSYNC, 0x05, 0x000,a9,,cc */
	{0xaa, 0xftR092_I2C04,80,cc */
	{00,fe,1,20,cc */
	{0xa0, 0x10, ZC1CWRI	{0xaa, {0xbb, 0x04, 0x0400}	{0xaa, 0x0f, 0x00b0},	/* 00,0f,b0,aa */
	{0xaa, 0x10, 0x0000},	/* 00,10,00,aa */
	{0xaa, 0x11, 0x00b,01,aa */
	{0xaa, 0x17, 0x00e8},	/* 00,17,e8,aa */
	{0xaa, 0x18, 0x0002},	/* 00,18,02,aa */
	{0xaa, 0x19, 0x0088},	/* 00,19,88,aa */
	{0xaa, 0x20, 0x0000},	/* 00,20,00,aa */
	{0xaa, 0x1b, 0x0020},	/* 00,1b,20,aa */
	{0xa0, 0xb7, ZC3XX02,10,cc */
0xa0, 0x42, ZC3XX_R197_ANTSURELLOCKSETTINGFLICKERH/* 00ZC3XX_* 00,05,01,cc0xaa1d, 0x0080},	/* 00cc *{0xa0, 0x10,cc * ZC3XX_R003_FRAMEWIDT	{0xd, ZC01,01,cc *84,30xa0, 0/* 00 84,09,00,bb */
	{0xbb, 0x86, 92_I2CADDREc */
	{0xa0, 0x00, ZC3XX_R11A_ANTIFLICKERMID}, /* 01,96,00e0x00aa},
	{0xa0, 0x00, ZC3XX_R62, ZC3XX_eNC_0}, /* 00,1d,93 ZC3XX_R01E_HRGB22},
	{0x00,1e,90,cc */
	{0xa0, NSORSELECT},	/* 008 0x01, 0x800},	/* 01,8f,15,cc */
	8CKERHIGH}, /* 01,95,00,ccB_I2CDIMITDIFF},	/* 01,a9,10c5,03,cc */
	{0xa0,_RGB22},
	{0 0x24, ZC3XX_R1AA_aa, 0x/* 00,fe,02,aa */
	
	{0x	/* 01,1d,60, 0x93, ZC3
	{0xaa,d,6RLOW}, /* 01,97,839,00,cc */
	{0xa0, 0x00,
	{0xaa,1AA_DIGITALGAINSTEP},cELECT},
	{0xa0, 0x02, Z01,cc */
	ALUE},
	{EZE}, /* 01,8f,20,cc */
	{0xa0, 0x10, ZC3XX_R1A9_D0,05,00,MITDIFF}, /* 01,a9,10,cc */
	{0xa0, 0x24, ZC3XX_R1AA_0,05,00,AINSTEP}, /* 01,aa,24,cc */
	{}
};

static const struct usb_action hdcs2020xb_Initial[] = {
	{0xa0, 0x01, ZC3XX_R000_SYSTEMCONTRO_R094_I SD_AUTOGAI	{0xaa, 0x11, 0x00aa},d,a14	/* 01,97,10,c0_I2C_EXPTIMEMID},	/* 00,87,83,cc *NC_0}, /* 00,KERLOW},	/* 01,97,10,cc */PNESS05},	/* 00, 0xa7, ZC3XX_R125_G0,0f,8c,aa */
	{0xaa, 0x030,88,84,0KERLOW}, //ALCGLOBAL0b0},	/* 00,11,b0,aa */
	{0 0x0 0x16, 0x000NC_0},	spcafXX_R1908F_AEUN01_SENSORCORRECTION},	/* 01,01,b7,cc */
	5,
	{0xa0, 0x40, ZKERLOW},	/* 01,97,10,cc */
	{0xa0, 0x0e, ZC3XX_R18C_AEF,97,10,cc *3
	{0xa0, 0x0e, ZC3XX__GAMMA00c3XX_R18F_AEUe, 0x0093}, /* 00,1d,a1,aa */
	{0xa0, 0x00, 9x0b, 0x0006},01,96,00,cc */
	{0xa0, 0x10, 9,
	{0xa0, 0x01, ZC3XX_R005_FRA0, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
	{C3XX_R090_I2CCOMMAND},
	{0xa0,ND},
	{0xa03XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R */
	{0xa0, 0xec, ZC3XX_R197_ANTIFLICKERLOW},	/* 01,97,ec,cc */
	{0xa0, 0x0e, ZC3XX_R18
	{0xa0, 0x01, ZC3XX_R0e,cc */
	{0x	{0xa0, 0x00, ZC3X8F_AEUNFREEZE},	/* 01,8f,15,cc */
	{0xa0, 0x10, ZC3XX_Ra, 0x03, 0x0IMITDIFF},	/* 01,a9,10,,
	{0xa0, 0x01, ZC010_CMOSSENSORSELECT},
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0, ZC3XL},
	{0xa0, 0x11, ZC3XX_R002_CLOCKSEL002},				/* 00,fe,02,aa */
	{0xa0, 0x0a, ZC30},	/* matr,cc */
	{0xa0, 0x83, ZC3XX_R087_EXPTIMEMID},	/* 00,87,83,cc */
	{0xa0, 0x84, ZC3XX_R088_EXPTIMELOW},	/* 00,88,84,cc */
	{0xaa, 0x05, 0x0000},	/* 00,05,00,aa */
	{0xaa, 0x0a, 0x0000},	/* 00,0a,00,a5, 0x000SD_AUTOGA	{0xa1, 0x01, 0x01ZC3XX_R11,
	{011C_FIRSTXLOW8B_I2CDEVICEADDR},	ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x01, ZC3XX_R{0xa0, 0x10, ZC3XX8B_I2CDEVICEADDR},		/* 00,80x0f, 0x00b0},	/* 00,0f,b0,aa */
	{0xaa, 0x10, 0x0000},	/* 00,10,00,aa */
	{0xa00,10,00,aa 19,88,aa */
	{0xaa, 0x20, 0x0000},	/* 00,20,00,aa */
	{0xaa, 0x1b, 0x0020},	/* 00,1b,20,aa */
	{0xa0, 0xb7, ZC3XXMANDNXSTARTLOW01,96,00,cc */
	{0xa0, 0x10, f0xaa, 0x0x01, ZC3XX_THIGH},
	{0xa0, 0xe	/* 01,1d,6MA02},
	{0xa0, 0x79, ZC3XX_R123_GAMMA03},
	{0xa0, 0x92, ZC3XX_R124_GAMMA04},
	{0xa0, 0xa7 ZC3XX_R092_0xa0, 0_R093_FRAMEWIDTHHIGH},/* 20,5f,90,bb */
		{0xa0, 0x24xa0, 0x90, ZC3XX_R01E_HSYNC_1},	/* 00,1e,90,cc */
	{0xa0, 0xc8, ZC3XX_R01F_HSYNC_2},	/* 00,1f,c8,cc 08},
	{0xa0, 0xdf, ZC3XX_R129_GAMx17, 0x00eR010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xdd, 000,1b,00,aaSYNC_2},	/* 00,1f,c8,ccf2,cccxa1, 0x01, 0x, ZC3XX_R1A7 0x00be}, /* 00,04aa, 0x82, 0x0},	/* 01,8f,15,cc */
	{7,cc */
	{0xa0, 0x00, 0x0d, ZC3IMITDIFF},	/* 01,a9,109 0x00be}, /* 00,04_GAMMA0C},
	{0xa0, 0xf9, ZC
	{0xa0, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, OSSENSORSELECT},	/* 9A_WINXSTARTLOW},
	{* 01,aa,24,cc 00, ZC3XX_R09A_WINXSTARTLOW},
xaa, 0x0f, 0x00800, ZC3XX_R001, ZC3XX_R090_I2CCOMM,
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* qtable 0x05 */
	{0xa0, 0x08, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0, ZC3X	{0xa, 0x10, ZC3X, 0x0180},
	{0xa0, 0xa0, 0x0e, ZC3XX_R137_GAMMA17},
	{0xa0, 0x0b, ZC3XX_R138_GAMMA18},
	{0xa0, 0x09, ZC3XX_R, ZC3XXanges*/
	{0xa0a0, 0xc 0x24, 0x0, ZC3XX_R090_I2CCC_AEFREEZE},
	{0xa0, 0x20, ZC00, ZC3X0x00, ZC3XX_R13C_GAMMA1C},
7195_ANTIFLI, ZC3XX_R13D_GAMMA1D},
	{0xa0, 0x01, ZC3XX_R13E_GAMMA1E},
a ZC3Xx0b, 0x0006},
	{0xaa, 0x0c, 0x007b},
	{0xaa, 0x0d, 0x00a7},
	ONTROL},
	{0xa	{0xa0, 0xf4, ZC3XX_R10Bd{0xa0, 0x06,a0, 0xf4, ZC3XX_R10C_Rd	{0xa0, 0xf4, 0xf4, ZC3XX_R10D_RGBe ZC3XX_R196 0x58, ZC3XX_R10E_RGB11MAND},
	{0xa2X_R197_0NTIFLICKERLOW}10},
	{0xa0, C3XX_R105, 0x01ae},
	{f0e, 0x0004},ZC3XX_R111_RGB21},
	{0x0x00aa},
	{C3XX_R112_RGB22},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x00, ZC3R180180_AUTOCORRECTENABLE},
	{0xa0, 0_I2CCOMMAND},
	{0xa0, 0x18, ZC3
	{0xaa, 0x23, 0x0000},
	{0xaa, 0x24, 0x00aa},
	{0xaa, 0x25, 0x00e6},
	{0xaa, 0x21, 0x003f},
	{0xa0, 0x01, ZC3XX_R190_1EXPOSURELIMITHIGH},
	{0xa0, 0x55, ZC3XX_R191_EXPOSURELIMITM05_FRAMEHEIGHTHIGH},
	{0xa0, 0xe0, ZC3XX_R006_FRA	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0x18, ZC3XX_R196_ANTIFLICKERMID},
	{0xa0, 0x6a, ZC3XX_R197_ANTIFLICKERLOW},},	/* 00,11ZC3XX_R13, ZC3XX_R092_I2CADDRESSSELECT},
	{0I2CWRITEACK	{0xaa, 0xfe, 0x0002},				/* 00,fb#def3xa0, 0RRECTENABLE},
	{0xa0, 0x40, ZC3XX_R116_RGAIN},
	{0xa0, 0x40, {0xa0, 0x01, ZC3XX_R012_VIDEOCONTROLFUNC} 320 * 240 * ZC3XX_R09A_WINXSTARTLOW},
	{0xC_GAMMA1C},
	{0xa0, 0x04, ZC3XX_R13D_GAMM3XX_R09C_WINHEIGHTLOW},
	{0xa0, 0x88, ZC3XX_R09E_WINWIDTHLOW},
/*fixme: next sequence = i2c exchanges*/
	{0xa0, 0x55, ZC3XX_R08B_I2CDEVICEADDR},
	{0xa0, 0x18, ZC3XX_Rd{0xbb, 0x01093_I2CSETVALUE},
3XX_Rb -> d},
	{0xa0, 0x04, ZC3XX_R010x00, ZC3XX_R098_WINYSTARTLOW},
	},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSECADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOxa0, 0x13, ZC3XX_R1CB_SHARPNE ZC3XX_R000_SYSTEMCONTRME "zc3xx"

#includechel Xhaard
 *	AMMA114,84,38,aa */	/* win: 00,94_I2CWRITEACK},
	{0xa0 more details.
ENSOR_T{0xa0, 0x4ncois Moine 1eeneral Public Lic
	{028eral Public Lic	{0xa30x00, ZC3XX_R094_I2CWR ZC3I2{0xa0, 0x01,, 0x2000},	N},
	{0xa0, 0x00, ZCedistribute it{0xa004},
	{0xa0, 0xa7,0xa03XX_R09A_struct gspcENSOR_R001_S,
	{3XX_or},
	{0xa*/
	R09C_WINHEIG3XX_R09CLOBALMEAN},
	{0s- */
	{0xaR09C_WINHEIGd_getcox15, 0x01a20,cc */
	{0xa0, 0 ZC3XX_R123_GAMMA03},
	{0
	{0xa0, 0xMMA03},
	{0xa0, 0x92, Z,
	{0xa0, 0xd0, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R, 0x08, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
a0, 0x0e, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x0f, ZC3XX_R092_I2CADDRESSSELEAND},
	{0xa0, 0x0*/
#define5AENSOR_OV763 0x01, ZC3XXCB 8
0x01, 60, ZC3XX_Rp    = 1,
		.default_63XX_R094_I2CWRITEA ZC3XX_R09D},
	{0xa0, 0x18, ZC3XX_R092 0x0d, X_R01E_HSYNC_1},C3XX_R11ad_setfreq12D_GAMMAa0, 0x00, ZC3XX_R11A127_GAMM_GAMMA1A},
	{0xa135_GAMxa0, 0xeb, ZC3, 0xf7, ZC3XX_R101_SENxa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x0a, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x02, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x0R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x13, ZC3XX_R092_I2CADDRER093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_IX_R092_I2CADDRESSSELECT},
	{0xa0, 0x18, ZC3XXD},
	{0xa0, 0x13, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x0a,  usb_action gc0390_I2CCOMMAND},
	{0xa0, 0x0a, ZC3XX_R092_I2CADDRESSSEwin: 00,84,eETVALUE},
	{0xa0, 0x0A03},
	{0xa0, 0xECT},	/* 00	{0xaa, 0x0a, 0x0000},	/* LE},
	{0xaR093_I2CSETVALUE},
	{0xZC3XX_R131_G	{0xa0, 0x01, 0x01b1},
04},
	{0xa0,52, ZC3XX_R116_RGAIN},	/
	{0xa0, 0xALMEAN},
	{0xa0, 0x04, ZC3XX_R131_G0xa0, 0x40, ZC3XX_R117_180_AUTOCORRCCOMMAND},
	{0xa0, 0x0a0xa0, 0x01, 5, ZC3XX_R012_VIDEOCONdxa0, 0x00, 0a0, 0x03, ZC3XX_R1C5_SH/
	{0xa0, 0xALCGLOBALMEAN},
	{0xa0exa0, 0x01, ZC3XX_R090_I2CCOMMAND},_I2CADDRESSSN},	/* 01,16,52,cc */
	{ZC3XX_R131_G
	{0xa0, 0x00, ZC3XX_R0I2CCOMMAND},},
	{0xa0, 0x92, ZC3XX_R124_GAMMA04},
	{0xa0, 0xa7, Zf ZC3{0xa0, 0x08,, ZC3XX_R125_GAMMA05},
	{0xa3, ZC3XX_R127_GAMMA07},
	{0xa0, 0xe0, ZC3XX71,92,f0,cc */
	{03XX_R128_GAMMA08},
	{0xa0, 0xdf, ZC3XX_R129_GAMMA09},
	{0xa0, 0xe7, ZC3XX_R12A_GAMMA0A},
	{0xa0, 0xee, ZC3XX_R12B_GAMMA0B},
	{0xa0, 0xf4, ZC3XX_R12C_GAMMA0C},
	{0xa0, 0xf9, ZC3XX_R12D_GAMMA0D},
	{0xa0, 0xfc, ZC3XX_R12E_GAMMA0E},
	{0xa0, 0xff, ZC3XX_R12F_GAMMA0F},
	{0xa0, 0x26, ZC3XX_R130_GAMMA10},
	{_GAMMA1B},
	{0xa0, 0x05, ZC0x01ad},
	{0
	{0xa0, 0x10,0a,cc *x01bOMMAND},
	{0xa0	/* 01,1d,MA03},
	{0xRECTENABLE},
	{0xa0, 0x */
	{0xbb, 0, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x04, ZC3XX_R00},	/* gamma 4 */
	{0xa0, 0x38, ZC3XX_R121_GAMMA01},
	{0xa0, 0x59, ZC3XX_R122_GAMMA02},
	{0xa0, 0x79, ZC3XX_R123_GAMMA03},
	{0xa0, 0x92, ZC3XX_R124_GAMMA04},
	{0xa0, 0xa7, ZC3XX_R125_GAMMA05},
	{0xa0, 0xb9, ZC3XX_R126_GAMMA06},
	{0xa0, 0xc8, ZC3XX_R127_GAMMA07},
	{0xa0, 0xd4, ZC3XX_R128RESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xND},
	{0xa0, 0x14, ZC3XX_R092_I2CADDRESSSELECV4L2_CID_AUTOGAIN,
		.type    = V4Lxa0, 0xame  0, 0x00,ichel Xhaard <me ***/
3tatic int sd_ge01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC
	{0xaa, 0x1b, 0x0020},	/* 00,1b,cc */
	{0xa0, 0x01, ZZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC0x78, ZC3XX_xa0, 0x20, ZC3XX_R18F_AEUNFREEZxa0, 0x01, 012_VIDEOCONTROLFUNC},	/*LOBALMEAN},
	{}
};

static const struca0, 0x00, ZC3XX_Raa */
	{0xaa, 00,17,e8,aa */
	{0xaa, 0x18, 0x0002},	/* 00,3_I2CSETVALUE},
	{0xa0*/
struct usb_action {
	_I2CADDRESS, ZC3XX_R1A7_CALCGLOBALtt struct us0, 0x04, ZC3XX_R1A7_CALCGLOBA1},
	{352, 288, V4L2_P			/* 14,_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX   .set {0xa0, 0x10, ZC3XX_R136_GAMMA16},
	{0xa0, 0x0eA05},
	{0xa0, 0xb9, ZC3XX_R126_GAMMA06},
	{0xa0, 0xc8, ZC 0x0180},
	{0xa0, 0x42, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x40, ZC3XX_R116_RGAIN}00,1d,a1,aa */
, 0x01, 0x0002},
	{0xa1, ,
	{0xaa, 0x12, 0x0087},
	{0xaa, 0x13, },
	{0xa0, 0x13, ZC3XX_R135_GAMMA15},
	{0xa0, 0x10, ZC3XX_R1308, ZC3XX_R250_DEADPIXEZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13MACCESS},
	{0xa0, 0x68, ZC3XX_R18D_YTARGET},
	{0xa0, 0x00, 0x01ad},
	{0xa1, 0x01, 0x0002},
	{0xa1, ,
	{0xaa, 0x12, 0x0087},
	{0xaa, 0x13, 0x0XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x15, 0x01ae},
	{00x01, 0x0008},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* 00 */
	{0xa0, 0x08, ZC3XX_R1C6_SHARPNESS00},	/* sharpness+ */
	{0xa1, 0x01, 0x01c8},
	{0xaDRESSSELECT},
	{00,10,dd */
	{0xbb	/* 20,5f,90,bb */
	{0xbb, 0x01, 0x800000,aa */
	{0xaa, 0x1A01},
	{0xa0, 0OW},	/* XX_R093_Ixa0, 0x64, ZC310A_RGB00/* 00,/
	{0xa7x01, ZC3X	/* 00,9c,e8,a
	{0xa0, 0x01,01, 0x01fC3XX_R19100,9a,00,cc *{0xaa, 0x13, 0x0/
	{0xa013D_GAMMA/* 00,_R195_Ac9},
	{0xa1, 0x0,00,cc R012_VIDEOiginal settingTVALUE},
	{0xa0, 0x00,0, 0x00, Z*/
	{0xaa, 0x_R191_EXPOSURELI1, 0x01_R191_EXPOSURELI00,cc *4ECT},
	{0*/
	{0xaa, 0x1GAMMA15},
	{0xx02, 	{0xa0, 0xb2, ZC3XX_R125_GAMMA05},
	{0xa_R10A_RGB00},	/* matrix */
	{0xa0, 0xed, ZC3cc */
	{0xa04, ZC3XX_R128_GAMMA08},
	{0xa0, 0xdf, ZC3XX_R129_GAMMA09},
	{0xa0, 0xe7, ZC3XX_R12A_GAMMA0A},
	{0xa0, 0xee, ZC3XX_R12B_GAMMA0B},
	{0xa0, 0xf4, ZC3XX_R12C_GAMMA0C},
	{0xa0, 0xf9, ZC,
	{0xa0, 0x00, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x00, ZC3XX_R019_AUTOA3XX_R13C_GAMMA1C},
	{0xa0, 0x04, ZC3XX_R13D_GAMMA1D},
	{0xa0, 0x03, ZC3XX_R13E_GAMMA1E},
	{0xa0, 0x02, ZC3XX, 0x0005}, /* 00,03,05,aa */
	{0xaa, 0x04, 0x00ac}, /* 00,04,ac,aa */
	{0xaa, 0x10, 0x0005}, /* 00,10,WINXSTARTLOW},
	{0xa0, 0x00, ZC3, 0x06, ZC3XX_R189_AWBSTATUS},
	C3XX_R135_GAMMA15},
	{0xa0, 0x10, ZC3XX_R136_GAMMA16},
	{0xa0, 0x0d0,1d,a1,aa */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURE{0xa0, 0x0e, ZC3XX_R137_GAMMA17},
	{0xa0, 0x0b, ZC3XX_R138_GAMMA18},
	{0xa0, 0x09, ZC3XX_R, ZC3XX_R003,
	{0xa0, 0x07, ZC3XX_4/* 01,92,f0,cc C3XX_R192_EX,
	{0xaa, 0x13, 0x0031},
	{0xaa, 0x14, 0x0001},
	{0xaa, 0x0e, 0x0004},
	{0xaa, 0x19, 0x00cd},
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITH, 0x01, ZC3XX_R13F_GAMMA1F},
b0e, 0x0004}, ZC3XX_R10A_RGB00},	/*ZC3XX_R192_EXPOSURELIMITLOW},
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0x00, ZC3XX_R00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x21, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x5c, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0,] = {
#defiDIFF},	/* 0x14 */
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
	{0xa0, 0x01, 3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x00, ZC3XX_R098_WINYSTARTLOW},
	{0xa6TALGAINSTEPXX_R09A_WINXSTARTLOW},
	{0xa0, 0x0eZC3XX_R008_C_FIRSTYLOW},
	{0xa0,},
	{0xa0, 0xR11C_FIRSTXLOW},
	{0},
	{0xa0, 0x3XX_R09C_WINHEIGHTLONC},
	{0xa0, x88, ZC3XX_R09E_WINW},
	{0xa0, 0xixme: next sequence },
	{0xa0, 0ges*/
	{0xa0, 0x55, ZLOW},
	{0xa0,DEVICEADDR},
	{0xa0,NC},
	{0xa0,_R092_I2CADDRESSMMA1D},
	{0xa0, 0x03, ZC3XX_R
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x14, ZC3XX_R092_I2CADDRESSS /****0,00LICKER	/* **STEMOPERATING}R18C_AE3/* 00,13,31,aa0_GAMMA0	/* 00,13,31,aa _CMOSSEN12_VIDEOCONTROX_R195_Ac*** set exposurxa0, 0x13, ZC3XX_R092_I2CADDRESSSELECT},
	{0xcc */
	{0xa0OMMAND},
	{0xa0, 0x13, ZC3XX_R0,cc */
	{}
a0, 0x09, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x08, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	ZC3XX_R008_0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /*C3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x0f, ZC3XX_R092_I2CADDRESSSELEC_R100_OPERATIx18, ZC3XX_R093_I2CSET2cc */
	{0xa00, 0x00, ZC3XX_R094_I2x0000, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLO0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0MMA1D},
	{0xa0, 0x03, ZC3XX_c}, /* 00,0f,8c,aa */
	{0xaa, 0x03, 0x0005}, /* 00,03,05,aa */
	{0xaa, 0x04, 0x00ac}, /* 00,04,ac,aa */
	{0xaa, 0x10, 0x0005}, /* 00,10,05,aa */
	{0xaa, 0x11, 0x00ac}, /* 0hdcTYLO0bR195_ANTIFLICKERHIGH},
92_I2CADDRESSSELECT},
	{0xa0, 0x02, ZC3XX_R093_I2CSETVALUEaa, 0x13R012_ ZC3XX_R13*/
	0,04,aa,aa */
	{0xaa, 0x13, PERATING},0,10,0,0f,5d,aa */
	{0_CMOSSENGH},	/* 00,9e,88 00,10,05,aa */
	{0_R195_A110,0a,	{0xaa, 01faa */
	{0xaa, 0x11, 0xLICKERMID}, /* 01,96,00,cc */
	{0xa0, 0x42, ZC3XX_R197_ANT,		/* 00,17,88,aa */
	{0xaa, 0x31{0xa0, 0x10RLOW}, /* 01,97,833XX_R088_EXP8c,10,cc */
	{0xa0, 0x20, Z92c */
	{0xaa, 0x05, 0x/* 01,8f,15,ccx1C_FIRSTXLOW},
	{0xaa130CK 15
#define S controls supporte_DIGITALLIMITDIFa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEAW},
	{0xa0, 0x00, ZC3, 0x01ae},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xa0, 0x68, ZC3XX_R18D_YTARGET},
	{0xa0, 0x00, 0x01ad},
	{0xa1, 0x01, 0x0002},
	{0xa1, ALLIMITDIFF}, /* 01,a9,0c,cc */
	{0xa0, 0x28, ZC3XX_R1AA_DIGITALGAIN, 0x01ae},
	{0xa0, 0x08,098_WINYSTARTLOW},
	, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,ccX_R001_SYS0xa0, 0x20, ZC3XX_R18F_AEUNFREEZ},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x0b, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x02, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWCT},
	{0xa0, 0x00, ZC3XX_R010_CMOSSENSOx00cd},			/* 00,19,ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x02, ZC3XX_R191_EXPOS0xaa, 0x14,X_R191_EXPOSURELIMITMID}, /* 01,91,02,cc */
	{0xa0, 0x62,SSELECf7LICKEXPOSURELIMITLOW}, /* 01,92,62,cc */
	{0xa0, 0x00, ZC3XX_ 0xf_OPERATIOxa0, 0x62, ZC3XX_RID}, /* 01,96,0X_R020_68x00, 0x0004},	X_R116_Rour opti ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
CKERLOW}, /* 01,97,3d,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,cc */
	{0xa0, 0x0c, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,0c,cc */
	{0xa0, 0x28, ZC3XX_R1AA_DIGITALGAINSTEP}, /* 01,aa,28,cc */
	{0xa0, 0x04, ZC3XX_R01D_HSYNC_0}, /* 00,1d,04,cc */
	{0xa0, 0x18, ZC3XX_R01E_HSYNC_1}, /* 00,1e,18,cc */
	{0xa0, 0x2c, ZC3XX_R01F_HSYNC_2}, /* 00,1f,2c,cc */
	{}
};
static6LOBALMEAN},
0x03, ZC3XX_R008_INYSTARTLOW},
	{0xaca_d5},
	{0xa0, 0x10, ZC3XX_R136_GAMMA16},
	{0xa0, 0x0e,, ZC3XX_R190)stri, ZC
MODULE_DE01CB 8
 0x040x01, ZC3XX_RusefulSELE 4L2_CID_B#define SENSOTSA.
 */

ICKERMID},	/* 94_I2CWRITCKERMID},	/* ,1e,00,cc */
	_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x13, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x96, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x13, ZC0xa0, 0xc8, ZC3XXXPOSURELIMITHIGH},	/0x00aa},
	{HSYNC_1},	/* 00,1e,90,ccNC_0}, /* 00,1d,930xa0, 0x0e, , ZC3XX_R134_GAMMA14},
	{0ECT},	/* 00,10,0a,0xa0, 0x0e, OCKSETTING},	/* qtable 0x05 */
	ECT},	/* 00,10,0a,_R010_CMOSSENSORSELECT},
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHH0, 0x00, ZC3X

static const struct, 0x00e8},	/* 00,17,* wincontrast(str
	{0xa0, 0x28, ZC3XX_R1AA_DIGITALGAINSTEP},
	{0xa0, 0x04, ZC3XX_R01D_HSYNC_0},
	{0xa0, 0x180, ZC3XX more details.

	{0xa0b},	/* 01,16,52,00,05,00,aa 3XX_R126_GA{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
a0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x08,8ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xaa, 0x02, 0x0090},			/* 00,02,80,aa */
	{}
};

static co8* 00,04,80,c, 0x04, ZC3XX_R1A7_CALCGLOBALMEAN},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALMEAN},
	{}
};

static const struc, ZC3XX_R002_CLOCKSELECT},
	{0xa0, 0x00, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xa0, 0x55, ZC3XX_R191_EXPOSUac,aa */
	{0xaa,, 0x01	{0xaa, 0x0e, 0x0005},			/* 00,0e,05,aa */
	{0xaa, 0x19, 0x0012_VIDEOCONTROLFUNC},
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMEHEIGHTHIGH},
	008},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTeral{0xa0, 0x08,ZC3XX_R25RCORa, 0x23, 0x0000C3XX_R12D_GAic const struct usb_action cs2102_60HZ[] = 92_I2CADDRESSSEL	/* 00,10,01,cc *},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* 00 */
	{0xa0, 0x08, ZC3XX_R1C6_SHARPNESS00},	/* sharpness+ */
	{0xa1, 0x01, 0x01c8},
	{0xa1, 0x2},
	{0x2{0xa0, 0x80, ZC3XX_R0040xa0, 0xfx86, 0, ZC3XX_R11A_FIRSTYLOW},
	{0x123, 0x004c},			/*3XX_R09C_WINHEIGHTLOW},
	{0xa0, 0x88, ZC3XX_R09E_WINWIDTHLOW},
/*fixme: next sequence = i2c exchanges*/
	{0xa0, 0x55, ZC3XX_R08B_I2CDEVICEADDR},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRES, ZC3Xx40, ZC3XX_5x64, ZC3OMMAND},
	{0xa0,, 0xc4, ZC3XX_R126_GAMMA06},
	{0xa0, 0xd3, ZC3XX_R127_GAMMA07},
	{0xa0, 0xe0, ZC3XX_R128_GAMMA08},
	{0xa0, 0xeb, ZC3XX_R129_FLICKERMID}, /* 01,96,00,cc */
	{0xxaa, 0x10, 0x0000},	/* 00,10_AUTOADJUSTFPS},
 /**** set exposuFLICKERH9C3XX_R191_EXPOX_R09A_W9c 0x03, ZC3XX_R13E_GAMMA1E}8enso,ac,aa */
	{0xaa,01,97,3d,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE}, /* 01,8c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20,cc */
	{0xa0, 0x0c, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,0c,cc */
	{0xa0,0,cc */
	{0xa 0x28t struct usb_05},	/* , ZCC3XX_R11C_FIRSTXLOW},
	{0xa0, 0xe8, ZC3XXhvgamm002_CLOCKSEL*/
	 0x00,  ZC3XX_R117_GGAIN},	/* 013XX_R126_GAMMA06},DEOCONTROL, 0x00e8},	/* /* 00,xaa, 0x1X_R1AA, ZC3XX5ONTRANKLOW},
	{0xa0, 0x30, ZCELECT}3_RGAINADD*/
	{0xa0, 0x80, Z2_EXPOSURR020_H, ZC3XXTROL}0x0020},			/* 00,1c,17,aa */
	{0xaa, 0 0x1d, 0x0080},	/* 00R012_VID5 */
	{0xaa, 01,04,0x00a0},			/* 00, */
	{0xb*/
	{0xaa, 02}, /0x00a0},			/* 00,		/* 00, 0x0	{0xaa, 03,faa, 0x0c, 0x007b},2FLICKERLOW},D}, /* 01,96,00,cc */3XX_R12E0,2, ZC3XX_R18C_AEFRE9{0xa0, 0xec, ZC3XX_R197_ANTIFLICKERLOW},	/9 01,97,ec,cc */
	{0xa0, 0x08,005_FRAMEHEIGHTHIGH},
3XX_R12E20, 0IGITALLIMITDIFF},	/* 01,a9,10cc */
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF}e*/
	{0xa0, 0,cc */
	{0xa0, 0x24ANTIFLICKe6_RGAIN},
	{0xa0, , ZC3XX_R090,90,cc */
	{0xa0, 0xc8, ZC3XX,	/* 00,19,00,cc */
	{0xaa, 0x25, 0x0007},			/* 00,25,07,aa */
	{0xaa, 0x26, 0x0053},			/* 00,26,53,aa */
	{0xaa, 0x27, 0x0000},			/* 00,27,00,aa */
	{0xaa, 0x20, 0x0000},			/* 00,20,00,aa */
	{0xaa, 0x21, 0x0050},			/* 00,21,50,aa */
	{0xaa, 0x22, 0x0012},			/* 00,22,12,aa */
	{0xaa, 0x23, 0x0080},			/* 00,23itial[] = {
	{0xa0, 0x01, ZC5EEZE}, /* 01,8f,20,URELIMITLO0, 0x11, ZC3XX_R002_CLO1 01,97,ec,cc */
	{ ZC3XX_R1A9, ZC3XX_R1A7_{0xaa, 0x2fNC_0}, /* sb_action hv7131b_60HZScale[] = {	/* 320x240 */
ction cs210{0xa3IONM00, X_R019_AUTOADJUSTFPS},	/* 00,19,00,cc */
	{0xaa, 0x25, 0x0007},			/* 00,25,07,aa */
	{0xaa, 0x26, 0x00a1},			/* 00,26,a1,aa */
	{0xaa, 0x27, 0x0020},			/* 00,27,20,aa */
	{0xaa, 0x20, 0x0000},			/* 00,20,00,aa */
	{0xaa, 0x21, 0x00a0},			/* 00,21,a0,aa */
	{0xaa, 0x22, 0x0016},			/* 00,22,16,aaSORSEL0xaa, 0x233XX_R003_FRAMEWIDT,23,40,at st
	{0xa0, 0x2TEACK},
	{0xa0, 0x0URELIMITHIGH},	/* 01,90,2f,cc */
	{0xa0, 0x4d, ZC3XX_R191_EXPOSURELIMITMID},	/* 01,91,4d,cc */
	{0xa0, 0x60, ZC3XX_R192_EXPOSURELIMITLOW},	/*01,92,60,cc */
	{0xa0, 0x01, ZC3XX_R195_ANTIFL0xa0, 0x07,	/* 01,95,01,cc */
	{0xa0, 0x86 00,9a,00,cc */
	{0 ZC3XX_R019_* 01,96,86,cc */
	{0xa0, 0xadTHIGH},
	{0xa0, 0xFLICERLOW},	/* 01,97,a0,cc */
	{0xa0, 0x0c, ZC3XX_R195_ANTIFL ZC3XX_R196HSYNC_1},	/* 00,1e,90,ccc */
	{0xa0, 0x00, {0xa0, 0x0b, ZC3XX_R134_GAMMA14},
	{0, ZC3XX_R18C_AEFREALLIMITDIFF},	/* 01,a9,18,cc */
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINSTEP},	/* 01,aa,24,cc */
	{0xa0, 0x00, ZC3XX_R01D_HSYNC_0},	/* 00,1d,00,cc */
	{0xa0, 0xa0, ZC3XX_R01E_HSYNC_1},	/* 00,1e,a0,cc */
	{0xa0, 0x16, ZC3XX_R01F_HSYNC_	/* 01,1d,60,cc,cc */
	{0xa0, 0x40UNC},	/* 00,12,01,c96,c3,cc */0,40,cc */
	{}
};
stat1, ZC3XX_R1sb_action hv7131b_60HZScale[] = {	/* 320x240
	{0xa0, 0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},	/* 00,19,00,cc */
	{0xaa, 0x25, 0x0007},			/* 00,25,07,aa */
	{0xaa, 0x26, 0x00a1},			/* 00,26,a1,a04, 
	{0xaa, 0x2{0xa0, 0x03, ZC3XX,27,20,aC3XX_R0	{0xaaSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAN,
	{0xa0, 0x60, ZC3XX_R11D_GLOBAL*/
	{0xa0,0,00,aa */
	{0xaa,C3XX_R196_ANa0, 0x00, ZC3XX_R1A7_CALCGLOBALf, ZC3XX_R12D_ ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX ZC3XX_R18D_ 0x15, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETV3XX_R093_I2CS_R11A_FIRSTYLOW},		/* 01,1a,0A},
	{0xa0, 0x06, 
	{0xbb, 0xe6, 0x0401},				/* 04,e6,01,bb */
	{0xbb, 0x86, 0x0802},				/* 08,88,02,bb */
	{0xbb, 0xe6, 0x0c01},	{0xaa, 0x020, ZC3XX_R087_EXPTIMEMID},
	{0x00,bb */
	{0xbb, 0x},
	{0xaa, GAMMA0F},
	{0xa0, 0x26,TROLFUN1, 0x	{0xa0, 0x06, FLICKERHIGH}W},		/* 00,9c,de,cc */{0xa0, 0x00, ZC3XX_0, 0x18, ZC3XTHLOW},		/* 00,9e,86, ZC3XX_R18D_YTARGET = {
	{176, 1	/* 01,1a,00,cc */
	{SYNC_2},
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE},
	0, 0x0000}	/* 00,10,01,cc */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xaa, 0xfe,,00,aa */
	{0xaa, 0x21, 0x00a0},			/* 00,21,a0,aa */
	{0xaa, 0x2TEACx0016},			/* 00,22,16,aaNADDR}0xaa, 0x231c, 0x0017},	/* 00,3EACK},
I2CCOMMa0, 0x00, ZC3XX_R1A7_CALCGLOBALa3XX_R128_GAMMA08},
	{0xa0, 0xdf, ZC3XX_R129_GAMMA0ZC3XX_R250_DEADPIXEC3XX_R10 ZC3XZC3XX_R192_EXPOSURELIMITLOW ZC3XX_R301_EEPROMAC3XX_R104,87X_R18F_AEUNFREEZE},	/* 01,8f,2ITALLIMITDITXLOW},		/* 01,1c,/
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF},{0xaa, 0x32,* 01,96,86,cc */
	{0xa0, 0xaX_R010_CMOSSENSORSEMEHEIGHTLOW},90,cc */
	{0xa0, 0xc8, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x0HSYNC_1},	/* 00,1e,90,cREEZE}, /* 01,8c,10,cc */
	{0xa0, eX_R020_HSYNC_{0xa0, 0x0			/R010_CM00, 0x0010},				/* CKSETTING},	/* qtable 0x05 */
0
	{0xa0, XX_R195_ANTIa0, 0x16, ZC3XX_R01F_HSYOSURELIMITMETVALUE},
	{0 0x22, 0x0012},			/* 00,22,12,aa */
	{0x */
	{0xa0, 0xff, ZC3XX_R020_HSYNC_3},	/* 00,20,ff,C3XX_R08B_I2CDEVICEADDR}ZC3XX_R01F_HSYNC_2},	/* 00,1f,16,cc */
	{0xa0, 0x40, ZC3XX_R020_HSYNC_3},	/* 00,20,40,cc */
	{}
};

static const struct usb_actionZC3XD},
	{0xa0, 0x00, ZC3XX_R1A7_CALCGLOBALd,
	{0xa0, 0x80, ZC3XX_R004_FRAM
	{0xa0, 0xfc, ZC3XX_R12E_GAMMA0E},
	{0xa0, 0xff, ZC3XX_R12F_GAMMA0F},
	{0xa0, 0x26, ZC3XX_R130_GAMMA10},
	{X_R020X_R01D_HSYNC_0},
	{0xa0, 0x18, ZC3X3/* 01,90,2f,cc */
	{0xa0, 0x4, 0x00e8},	/* 0TING},
	{0xa0, 0x05, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x07, ZC3XX_R012_VIDEOCONTROLFUNC},
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004_FRAMEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FRAMTART_R134_GAMMA14},
	{REEZE}, /* 07,69,cc ZC3XX_R13E_, ZC3XX_R190x00, ZC3XX_R098_WINYST* 01,96,00,cc */
	{XX_R192_EXPOSURELIMITLOWTEP},	/* 010d, ZC3XX_R100_OPEB10,dd */
	GET},
	{0xa0, 0x00xa0, 0x00, Z},
	{0xa0, 0x02, Zxb_Initial[] = {
	{0xa0, 0x01, ZCx0003},	/* 006},
	{0xaa, 0x21,1f,16,cc */
	{0xa0, 0x40cc */
	{0xa0, 0xd3/* 00,03,050,03,cc */
	{}
};
stati88,aa */
	{2F_GAMMA0F},
	{0xa0, 0x26, ZC3XX_R130 320x2cxxWINYSTARTLOW},
	{0xa0, ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},
	{0xaa, 0x02, 0x0008},
xa0, 0x01, ZC3XX_R001_SYSTEMOPERATING},
	{0xaa, 0x17, 0x0086},
	{0xaa, 0x31, 0x0038},
74},
	{0xa0, 0xb2, ZC3XX_R125_GAMMA05},
	{0xa0xa0, 0x16, ZC3XX_R134_GAMMA14},
	{0xa0,3, ZC3XX_R127_GAMMA07},
	{0xa0, 0xe0, ZC3XX_3XX_R117_GG27_GAMMA07},
	{0xa0, 0xe0, ZC3XX_XX_R002_CLOCKSELECT},
	{0xa0, LSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xa0, 0x68, ZC3XX_R1HSYNC_1},	/* 00,1e,90,ccSS05},	/* 0, ZC3XX_R094_I2CWRITEACK},
	{0a0, 0x0c, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,0c,cc */
	{0xa0, 0x28, ZC3XX_R1AA_DIGIT117_GGAIN},	/* 01,XX_R123_GAMMA03},
	{0xaENABLE},
	{0xa0, 0x40, ZC3XX_R116_RGAIN},
	{0xa0, 0x40,0d, ZC3XX_R100_OPERATIONZC3XX_R01F_HSYNC_2},	/* 00,1f,16,cc */
	{0xa0, 0x40, ZC3XX_R020_HSYNC_3},	/* 00,20,40,cc */
	{}
};

static const struct usb_actiona0, 0xf0, Z	/* sharpness+ */
	{0xa1, 0x01, 00, ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0x00, ZC3X/* 20,5f,90,bb */
	{0xbb, 0x01, 0x8000},				/* 80,01,00,bb */
	{0xbb, 0x09, 0x8400},				/* 84,09,00,bb */
	{0xbb, 0x86, 0x0002},				/* 00,88,02,bb */
	{0xbb, 0xe6, 0x0401},				/* 04,e6,01,bb */
	{0xbb, 0x86, 0x0802},				/* 08,88,02,bb */
	{0xbb, 0xe6, 0x0c01},				/* 0c,e6,01,bb */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,a0, 0x13, Z 0x13, ZC3XX_R1CB_SHARP09a, 0x21, 0x003f},
	{0xa0, 0x01THLOW},		/* 00,9e,86,LIMITHIGH},
	{0xa0,00,25,07,aa */1},			/* 00,26,a1,aa */0},			/* 00,27,20,aa */0xa0, 0ACK},
	{0xa0, 0x01D}, /* 01,9, 0x02, ZC3XX_R090_I2CCOMMAND},
	{0xa1, 0x01, 0x0091},
	{0xa1, 0x01, 0x0095},
	{0xa1, 0x01, 0x0096},

	{0xa1, 0x01, 0x0008},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING},	/* clock ? */
	{0xa0, 0x08, ZC3XX_R1C6_SH_R094_I2CWRITEACK},
ICKERLOW},	/* 01,97,00,cc */
	{,
	{0xa1, 0x01, 0x0002},
	{0xa1, 0x01, 0x0008},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x02, E},	/* 01,8f,18,cc */
	{0xa0, 0x18, ZC3,cc */
	{0xa0, 0x10, ZC3XX_R1A9_DWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
a0, 0x04, Z, 0x0b, ZC3XX_R191_EXPOSURELIMITMID},	/* 01,91,0b,cc */
	x00, ZC3XX_R11C_FIRSTXLaa */
	{0xaa, 0x19ELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xaa, 0x02, 0x0090},			/* 00,02,80,aa */
ock ? */
	{0xa0, 0x08, ZC3XX_R1C6_SHARPNE,cc */
	{0{0xa0, 0x22, ZC3XX_R131_GAMMA11},
	{0xa0, 0,	/* 01,950_CMOSSENSORSE},
	{0xa0, 0x1c, ZC3XX_R133_	RECTENABLE},
	{0xa0, xaa, 0x10, 0x0oem90,05,aa */
	d_setg00,1d,a1,aa */
	{0xa- (clos"

#i gspca{0xa0_NONE,
		.bytesperline = 352,
	 01,cb197_ANTIFLICKERLOW}, /* 01,97,42,cc */
	{0xa0, 0x10, ZC3XX_R18C_AEFRE, ZC3XX_R019_, ZCX 16
#defx007b},
	{0xaa, 0x0d, 0x0	{0xaa, 0xfe, 0x0002
	{0xa0, 0xf4, ZC3XX_R10B_RGB01},
	{0xa0, 0xf4, ZC3XX_R10C_RGB02},
	{0xa0, 0xf4, ZC3XX_R10D_RGB10},
	{0xa0, 0x58, ZC3XX_R10E	{0xaa, 0x2 0x14, 0},
	{0xa0, 0x43XX_R3XX_R196_ANTIFLICKERMID}, /*8_SYSTEX_
	},
#define 4_I2CWRITEACK},
	{0xa0, 0xeb, ZC, ZC3XX_R18C_AEFREEZE}, /1FLICKERHIGH},	/* 01a,00,cc */
	C3XX_R18F_AEUNFREEZE}, SS05},	/* sharpness/* 00,04,80, ZC3XX_R1A9_DIGITALLIMITDISTXLOW},		/* 01,1c,_GAMMA1B},
	
	{0xa1, 0x01, 0x0180},
	a1, 0x01, 0e = 640 *0x15, 0x01ae}0, 0x93, ZC3XX_R01D_HS5C3XX_R1A9_x10, ZC3XX_R18C_AEFA, 0x0f, ZC3XX_R1CB_SHARPNESS03XX_R012_VIDEOCONTROE_RGB11},
	{0xa0, 0xf4, ZC3XX_R10F_RGB12},
	{0xa0, 0xf4, Z, ZC3XX_R008_CLOCKSETTING},
	{0xa0, 0x08, ZC3XX_R010_CMOSSENSORSELECT},
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTHHIGH},
	{0xa0, 0x*/
	{0xa0, 0x90, ZC3XX_R01E_HSYNC_1}, /* 00,1e,90,cc */
	{0xa0, 0xc8, ZC3XX_R01F_HSYNC_2}, /* 00,1f,c8,cc */
	{}
};

/* CS2102_KOCOM */
static const struct usb_action cs2102K_Initaa, 0x1c, 0x0017},	/* 00,1c,17,aa */
94_I2a0, 0x00, ZC3XX_Rusb_action cs2102K_Ini.minimum = 1,NWIDTHHIGHXX_R008_CLOCKSEXX_R1sd_getfr04,80,ma,
	},
#defELIMITLOW},
	{0xa0, 0x00, ZC0x13, 0x0000},	/* {0xaa, 0x13, 0x0001}; */
	{0xaa, 0x14, 0x0001},
	{0xaa, 0x15, 0x00e6},
	{0xaa, 0x16, 0x0002},
	{0xaa, 0x17, 0x0086},
	{0xaa, 0x31, 0x0038},
	{0xaa, 0x32, 0x0038},
	{00xa0, 0x13, ZC3XX_R1CB_SHARPNE01_SYSTEMf0x0001},	/* 00,15,011c,17,aa */
	{0xaa 0x00YNC_3ENSORCO	{0xaa,, 0x0 0x09, 0x01a, ZC3XXMMA00 */
	{0xaa,7,e6_ANTIFLc 8 + 590,
	9,cORSPACE_Jm2700_Init03},
	{0define SD_AU,	/* 00,05,00,aa */
008_CLOCRL_T	{0xa0, 0AMMASSENSORSELECT},	/	/* 01,eb
	{0xa0, 0x0eb{0xaa, 0x17, 0x0080, 0x01,	{0xa0, 0x0ACK},0xaa, 0x17, 0x008 V4L2 by 0x01, ZC3X8cEAN},
	SYNC_2},	/* 8ZC3XX_R0	{0xa0, 0x0{0xaa01, ZC3XX_R090_I2
	{0xa0,define SD_8a,	/* 00,05,00,aa */
	{0xaa, xa0, 0x0a, GITAINWIDTHHIGH},
	{0xaou can 	{0xa0, 0x4168F_AEUNFREEZE},	/* 0f, ZC3XX	{0xaa, 0x0383D_GAMMA1D},
	{0xa0, , ZC3XX_R002_CLOCKSELECT},		/* 00,02,04,c_ANTIFL ZC3XX_aa, 09A1C}},
	{0xa0, 0xdf, R01D_HSYdefine SD_45R008_CLOCKSETTING},	2_RGB22}eC3XX_R250_50,xa0, LSMODE},	/* 02ZC3XX_R092_fine SD_NTIFa0, 0x00, ZC3XX_R11{0xa0, 0x0xa0, 0x0/
	{ZC3XX_R10E_RGB11},
093_I2CSTUS},
	{0xa3,0x10, 0x0005}EIGHTLI2CADDREfine SENSOR79AMMA14},
	{0xa0,0x00703},
	{0xa0LUE},
	{00xa0, 0x10, ZC3XX_R00_CLOCKSEXX_R019_AUTe_EEPROMACCESS},
	{0xa structad},
	{0xa1,fDTHLOW},
	{0xa0, 0x03_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_180}			/*},	/* 00,2330x00, ZC3XX_R192_EXPOSURELIMX_R019_AU0xaa,60, 0x01, ZC3XX_R090_I2CCOM, ZC3XX_R1237,fe,02,aa */
	{0xa0,1c,00,cc DR},		/* 00,805,aa */
		{0xa0, 0},
	ELSMO{0xdd, 0x04HHIGHx0180},
	{0xa0, 0/* 00,1c,17				/* 0, 0x21, 0x00a0},			/*6ou can 12_VIDEOE},
6EXPO/* 00 */
	{0xa0, NTIFLICK0xa0, 0x2067R092_I2CADDRESSSELEC6x01, 0x	{0xaa, 0x36bxa0, 0x7c, ZC3XX_R09,cc CADDRESSSELECT}6c},	/* 01,8f,20,cc */27_G 0x26, 0x 00,10,6NC_3, 0x24, 0x00aa},
xa0, 0x0b, ZC3XX_R138_GAMMA18},
	{0xa0, 0xf
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x06, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x03, ZC3XX_R093_I2CSETVALUE},
	{0x0, 0x20, ZC3XX_R18F_AEUNFREEZE}, /* 01,8f,20US},	/* 01,89,I2CCOMMAND},
	{0xa0, 0x09, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x08, ZC3XX_R093_I2CSETVALUE},
	{013, 0x0031},
	{0xaa,  0x0008},
	{0xa0, 0x03, */
	{0xaa, 0x0a, 09,00,cc */
	{0xa0, 0x00, ZC3XX_R1AA_DIGITALGAINSTEP},},
	{0xa0, 0x02, ZC3XX_RETVALUE,
	{0xaa, 0xtruct usb_action hv7XX_R125_GAMMA05},
	},	/* 01,16,53XX_R090_IFLICKERHI4TXLOW},		/MEWIDTHHIGH},	ec3, ZC3XX_R196,aa */
MA1C},
	{ ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x0d, ZC3XX_R092_{0xa0,W},
	{0ECT},
	{0xa0, 0xa3, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWR98_WINYSTARTLOW},
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},
	{0xa0, 0xe8, ZC3XX_R09C_WINHEIGHTLOW},
	{0xa0, 0W},
	{0xa0, 0x00, ZC3X18F_AEUNFREEZE}, /* 01,8f,20,cc */
	{0xa0, 0x0c, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,0c,cc */
	{0xa0, 0x28, ZC3XX_R1AA_DIGITx27, 0x0000},

	{0xa0, 0x10, ZC3XX_R190_0},	/* 0rXX_Rogram2c*gspcs idENSO, 0xdf, ZC3XX_R129_GAMMA0_GAMMA04},
	{0xa0, 0xa0, 0x04, ZC39XX_R093_I,
	{0xaa, 0x060xa0, 0xf,
	{0xaa, 0x0610A_cc */
	{}
};
static const struct usb_action hdcs2020b_NoFliker[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{0xaaAMMA15},
	{0xa0, 0x10, ZC3XX_R136_GAMMA16},
	{0xa0, 0x0e, ZC3XX_R137_GAMMA17},
	{0xa0, 0x0b, ZC3XX_R138_GAMMA18},
	{0xa0, 0x00, 0xe0, ZC3XX_R006_FRAXX_R09A_WINXSTARTLOW},
	{0xa0, 0x003XX_R093_I2C_FIRSTYLOW},
	{0xa0, 3XX_R093_I2CSETVALUE},
	{0xa0, 0x3XX_R093_I2CHLOW},
	{0xa0, 0x01, 3XX_R093_I2Cx88, ZC3XX_R09E_WINWI3XX_R093_I2Cixme: next sequence = ZC3XX_R090_es*/
	{0xa0, 0x55, ZC ZC3XX_R090_	{0xa0, 0x00, ZC3XX_R ZC3XX_R090_TLOW},
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},,cc */
	{0xa01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x14, ZC3XX_R092_I2CADX_R192_EXPOSURELIMITLOW},
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH},
	{0xa0, 0x30, ZC3XX_R196_ANTIFLICKERMID},
	{0xa0, 0xd4, ZC3XX_R197_ANTIFLICKERLOW},
	{0xa0, 0x10, ZC3XX_R18C_AEFREEZE},
	{0xa0, 0xa0, 0x08, a0, 0x00, ZC3XX_R195_ANTIFLODE},	/* 02,50,08,cNXSTARTLOW},
	{0xa0, 0x00, Z 0x01, ZC3XX_R195_ANTIFL00, ZC3XX_R1963XX_R301_Ea0xa02_VIDEOCONTROLFUNC},
	{0xa0,,
	{0xa1, 0x01, 0x0002},
	{0xa1, 0x01, 2},				/* 0LFUNCa0, 0x04, ZC3XX_R093_I2CSHSYNC_3},
	fR133_GAMMA13},
	{0xaZC3XX_R004_FRAMEWIDTHLOWZC3XX_R098_WINYSTARTLOW},
	{0xa0, 0x00, ZC3XX_R09A_WINXSTARTLOW},
	{0xa0, 0x01, Z001{0xa0, 0x10, ZC0x01, 0xECT},
	{00xa1, 0x01, 0xCADDRESSS0xa1, 0x01, 0	{0xa0, 0x55, ZR09D_WINWIDTHHIGH},
	{0xa0, 0x88, ZC3XX_R09E_WINWIDTHLOW},
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},
	{0xa0, 0
	{
	  0},	/* sharpness+ */
	{0xa1, 0x01, 0x01c8},
	{0xa1, 0x01, 0x01c9},
	{0xa1, 0x01, 0x01ca},
	{0xa0, 0x0f, ZC3X0xa0, 0x16, ZC3XX_R134_GAMMA14},
	{0xa0,cc */
	{0xa0	{0xa0, 0x46, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,46,cc */
	{x11, 0x0001},
	{0xaa, 0x13, 0x0000},	/* {0xaa, 0x13, 0x0001}; */
	{0xaa, 0x14, 0x0001},
	{0xaa, 0x15, 0x00e6},
	{0xaa, 0x16, 0x0002},
	{0xaa, 0x17, 0x0086},
	{0xaa, 0x31, 0x0038},
	{0xaa, 0x32, 0x0038},
	{00xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x15, 0x01ae},
	{_SHAXPOSURELIMITMID},	/* 9b *, ZC3XX_R008_CLOCKSETTING7XX_R010_CMDRESSSELECT},
	{0x00, ZC3XX_R0},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSETTING90x0010},		SURETIMEHIGH},
	{0xa0, 0x94, Z},	/* 00 */
	{0xa0, 0x08, ZC0xa0, 0x07,ARPNESS00},	/* sharpness+ ITMID},
	{0xtatic const struct usb_a0xa0, 0x94, Z0, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0,20b_60HZ[] = {
	{0xa0, 0x00,xf8, ZC3XX_R1D, 0x01, 0MA11},
	{0xa0, 0x2ZC3XX_R019_AUTOADJUSTFPS}, /* 00,GAMMA04},
	{0xa0, 0xb2, ZC3XX_R125_GAMMA05},
	{0xa
	{0xa0, 0xdf, ZC3XX_R129_GAMMA09},
	{0xa0, 0xe7, ZC3XX_R12A_GAMMA0A},
	{0xa0, 0xee, ZC3XX_R12B_GAMMA0B},
	{0xa0, 0xf4, ZC3XX_R12C_GAMMA0C},
	{0xa0, 0xf80, ZC3XX_R00NABLE},
	{1, 0x01, 0x01ca},
	{0xa0, 0x0xaa, 0x	/* 00,13,31,aa 00,cc *	{0xa0, 0x},			/* 00,19,/* 00,13,31,aa C3XX_R0120_HSYNC_3},
	{R18C_AEF/* 00,13,31,aa  0x15, ZC3XX_R09E00,9a,00,cc *x40,EAN},
	{0xa0, 0xc0, ZC3XX_R1A8_DIGITALGAIN},
	{0xa0, 0xc0, ZC3XX_R ZC3XX_R0LGAIN},
	{0xa0
	{0xa0, 0x01,_CALCGLOBALMEAN},
	{0xa0, 0xc0, ZC3XX_R1A8_DIGITALGAIN},
	{0xa0, 0xc0, ZC3X},			/* 00,19,5, ZC3XX_R01F_HTALGAIN},_OPERATIONMODE},
	{0xa0,
	{0xa0, mma,
	},
#def	{0xaa, 0xfe, 0x0002},		001_SYSTEMOP21, 0x0012},	/* 00,21_GAMMA04}a0, 0x18, Zb#defC3XX_R08B_I2CDEVICEADDR},
	{0xa0, 01c0, ZC3XX_R09A_W0, 0x5c, ZC3XX_R0 0x0010},				/* 00,00,10,dd */
	{NSORSELECT},	/* 00R180_AUTOCOxa0, 0x03, ZC3XX_R1C5_SHARPNESSMx00, ZC3XX_R190_EXP, ZC3XX_R008a0, 0x00, ZC3XX_R1A7_CALCGLOBALR196_ANTIFLI1ca},
	{0xa0, 0x0{0xa0, 0x24, ZC3XX_R120_GAMMA0R11D_GLOBALGAINX_R1920x00, ZC3XX_bb */
	{0xa0, 0x01, ZC3XX_R01	/* 00,fe,02,aa */
0x00, ZC3XX_x01, ZC3XX_R001_SYSTEMOPERATI5S05},	/* sharpness-C3XX_R093_I2CSETV3XX_R18,98,00,cc 9b, 0x00f0},
	{0xaaESSSELECT},
	{0xa0, 0x024C3XX_R0a,00,SS05},	/* sharpness	{0xa0, 0x96, ZC3XX_R093_I0xa0, 0x07,ZC3XX_R01, 0xcc, ZC3 ZC3XX_R11A_FIRSTYLOW},		/* 01,1a,00,cc */
	{0xa0, 0x00, ZC3XXd */
	{0xbb,004_FRAMEWIDTHLOW},
	{0xa0, 0x00,03,02,cc {0xa0, 0x03, ZC3XXa0, 0x60, ZC3, ZC3XX_d 0x00, ZC3XX_R094_I2C,bb */
	{0xb0, 0xc8, ZC3XX_R01F_HSYN_I2CSETVALUE},
	{0x0xaa, 0x91, 0x001f},
	{0xaa, 0x10, 0x0064},
	{0xaa, 0x9GAMMA11},
	{0xa0, 0x20, ZC3XX_R132_GAM0x0000},
	{0002},
	{0xaa, 00, 0x22, ZC3XX_R131_GAMMA11},
	{0xa0, 0x20, ZC3XX_ale[] = {
	0095},
	{0xa1OW},
	{0xaa, 0x0, ZC3XX_R196b},				/* 00,fe,02,aa ,20,cc * 0x52, ZC3XX_F_AEUNFREEZE},C3XX_R135_GAMMA15} ZC3XX_R1A9_DIGITALLIMITDIFF},
	LECT},
	{0xa0, 0x0},	/4/
	{0xa00,40,cc */
	{}
};

static constcale[] = {
	{0xa0, ,
	{0xaa_R192_EXPOSURELIMITLOW}, /* 01,92,80,cc */
	{0xa0, 0x00, ZC3XX/*
 5_ANTIFLICKERHIGHc301/zc3025,0c30x library
 *	Co6fright (C) 2604 2005ean-6MIDhel Xhaard6,6f	mxhaard@magic.fr2y*
 * V4L2 b7 Jean-Francoo haar Xhaard7,2		mxhaard@magic.frpc*
 * V4L2 8C_AEFREEZEftware; y8c *		mxhaard@magic.fr18nd/or modifF_AEUN it under the tef,18	mxhaard@magic.fr
*
 * This pA8_DIGITALGAINftware; ya8,6 of the GNU Generalndation; eit9er versiimicrDIFF ofd by
Li9,1		mxhaard@magic.fr22ption) any Aater veron 2STEP},	This pra,22	mxhaard@magic.fr8 Public LiceD_YTARGET},	ul,
 * 8d,8e Free SoMERCHA/* win: warrant0haard@magic.fr5Y; without 1D_GLOBwill b oliedY or1d,5y of
 * Mftware Fo4nd by
hope 80_AUTOCORRECTENABLEefied warr0,4WITHOUT AN}
};

static const struct usb_acion) PO2030_NoFliker[] = {stribute it  more details.
 * warYou should hed by
 *0,0d a copy oY WaRRANTde it y0duoption0r FI0d,aa75 Mass Ave, C1e MOD0000MA 02139,1a *		 lib
#define MODUbE_NAME 2zc3xx"

#ibnc.,e "gspca.h"
#inclcbridge78.h"

MODULc,78THOR("Michel Xhaa46de "jpe".h"

MODU46clude "gspca.h"
#incl5rge A. Suchkov <Sigic.de "gspca.d by
/* TESThaarGNU GeneralY; without ense waratas5130CK_Initialprogram; if not, w1*
 * V4L2000_SYSTEMCONTROL},nsor = -1;gspca "jp3bntiztion) table lib#inalude "zc3xx-reg.h"

/*8lude "zc3xx-re.A.S@jp39lude "zc3xx-rete it yiptor libublic Lsd {
	ublic Ltochk_dev pca.h"
#QUANT_VAL 1		/* qu specific webca3u8 brightne8_CLOCKSETTINGlude "zc3xx-reau8 brightn10_CMOSSENSORSELECTlude "zc3xx-r opion)) an002ogain;
		u8 coimageontrliwrite to th003_FRAMEWIDTH net>,Mass AAe, Cay.h"

ca.h"
4u8 brightnL soft iteme "gs	__u8 b*
 * ne5ca.h"
HEIGHTITY_DEF 50

	signed char senso6values used * T
#defiseizatidaPublic Li08B_I2CDEVICEADDR 0
#define SEchip lib/* !!1s;
	__uOPERAhip l
 * freq;r ch78 sharpness2_VIDEO8 contrFUNCst item */

	_gsp_CS2102 98_WINYSTAR2700 0spca.h"
#SEp*
 * This 09Aa.h"X qualit_ICM105A 7M105A 7
#dealit11A_FIRSTY7620 9
/*#de_OV7620 9
/*r senC 9
/*#XOR_ICM105A 7
#de 9 - 131C 6
1_OV7630C 10
#define SEK 2ar s5b 4_OV7630C 10
#defHV7131B 5_OV7630C 10
#de_u8 brightn92r sealitESSY_MIN 4CM105A 7
#QUA0
#deflong w3r seSETVALUEne SENSOR_OV7620 9
/*#de_M4r seWRITEACK0 13_OV7630C 10
#deflong w0r seCOMMAND_CS2102K 2
#de6AX 18
	unsi 102B 12
#define SETAS
stati 15
ENSOR_PC 6
#controls suXX 16
};

/* V4L2 controls su_VF0250 1ine SENSOR_TAS5130AX 18
	untablede
 *rtdefin_reviwill;
Y; without_hdr;
};

/* V4L2 controls suppor8maCS202u8 arive/* !! mU Genin be _setbr4ghtness(struct gspca_dev *gspca_dev, __s32 val);
static int sd_getbrightnessMAX 18
	unsidr;
};

/* V4L2 controls supportev *tem */

_setcontrast(struct gspca_d
/* !!ss(first item */

	_uct gspcav, __s32 val);trast(struct gsgval);
static int sd_getautogain(struct gspca_dev* *gspc

	u8 *jpeg_hiver */
static int sd_setbrightness(struct gspca_dev *gspca_dev, __s32 val);
static int sd_getbrightnessLin d_s3260*val);
static int sd_setautogain1};

/* V4L2 controls sutic int sd_setbrightness(struct gspca_dev *gspca_dev, __s32 val);
static int sd_getbrightnessast(struct gdr;
};

/* V4L2 controls supporE0S202B 12
#pca_dev, __s32 *valgamma intMAX 18
	unsiuct gspca_dev *gspca_dev, __s32 val);
static int sd_getbrightnessgain(struct mma(struct gspca_dev *gspca_devct gspca_dev *gspca_dev,rast(stublic Lctin(struct gsuct gspca_dev *gspca_dev, __s32 val);
static int sd_getbrightnesstruct gspca_dr;
};

/* V4L2 controls supportin(struct gspca_dev *gspca_dev, __s32 3ightness(struct gspca_dev *gspca_dev, __s32 val);
static int sd_getbrightnese SoftThis _hdr; the /* V4L2neratrols supported byd by
dd_setcontrast(struct gspca_5rlt gsctrlsprograspca.h"
#BRsed NESS_IDXICM105A 7
#dD_t_value =  0
	{
	    {3AS202B 12
#ntrast},ntrast.set =_s32 val);
s7Fca_dev *gspca_dev, __s32 *valstruct ctsd_setgamma(struct gspca_dev *gspca_devrast(struct gspcaR_HD int sd_getautog,    },
	    dr;
};

/* V4L2 controls supportntrastg
	},
ast,
		.id 4,
  =R,
		_CID_GAMMA,    type    .set = sTRL_TYPE_INTEGERma,
	nam .get ="Gtruc"ma,
	minimum dev, __s32 *val);
static int sd_setautogain(sst",
		.minimum = 0,
		.maximum = 256);
static int sd_getautogain(struct gspca_dev *gspca_dev, __s32 *val);
staticICULAR PURP,
#degetname ast,
	} = 1,
		.maxiG_getcontrastpca_dev *gspca_dev, __s32 *val);
static int sd_setsharpness(struct gspca_dev *gspca_dev, __s32 val);
st6in(struct gspca_dev *gspca_dev, __s32 *val "Gamma",
		.minimumstruct gspca_dev *gspgain(struct gspca_dev *gspca_dev, __s32 *valsd_setau int sd_getautogain(stru2Bgetcontrast,
	},
#definautQUALIcontrastlue e Soft,
		 = sd_setain,
	    .Lsed FREQ128,
02B 12
#defD__SHA 4tcontrast,    },	 .set = sd_sPOWER_LINE{
		.UENCY,
	2#define SEN_getcontrast,
	},
#definR_HDsetfreq,
	   .set = 	.minimain,
	    .getSHARP sd_s5	 = V4L2_CID_SHARPNESS,
		.ty,
		.defama,
	    .get = sd_getgamma,
	}D
#define SD_AUTOGAIN S
#define	    {
		.id  = 0    {
axl2_pix_3ma,
	step    .s1ma,
	default_SENSO = 2contrastcontrast,
	},
#defin4
#definenimum = 0,
		.maximusearpness,
	},
};

static const struct v4l2_pix_format vga_mode[] = {
	{320, 240, V4L2_PIX_FMT_JPEG, V4L2_FIELD_NONE,
		.bytesperline = 320,
		.sizeimage = 320 td char sens87	Z-STIMEis Mord@magic.frbpca_dev, _1_GC0e QUtoftwareIONSENSOR_MAX 18
	    .get = SENSOR_MAX 18
B03 *vaine SENSOR_det = sOLORS0_ structONMODic int sd_setbr __s32 val189_AWBSTATUS_CS2102K 2
#de9    =rada_dev *gspca_dev, __s32 1C5et = sd_seX_FMT_JPEG,set = 1 3 / 8 + 590B    colors05*val);
static int sd_set250_DEADPIXELspacG, V {
	{176, c int sd_ge301_EEPROMACCESlh"
#= 176ma,
MA 0213) any hat it will b o= 352 * 288 SOR_CS2102ifve more impe = V4L2_COLS5130CK 15116_RorIELD_NONE,
		.by	    .get 118_B/
struct usb_acti.size
#defi352 * * 144 
inclLE1a
sta0},_INTEGE4ORSPACE_JPE0A_RGB SucNTABmatrixhaard@magic.frfusb exchang0B1,
		1static const *PACE_JPEG,
	C/* 002_action adcm2f 240	{0xa010D1,
	10 vga_mode[25* 288
	{320,10E,04,c,00,0130x libr 1		/* quant	F8_CLOR002ogain;
	LEeORSPACE_JPE101,
	2x library
 *	C{0xa0, 0x04,110
#deCKSETTING},		/opyright (_R120
#de0xa0, 0x0a, ZCmtatic int st = sd_tic int SENSOR_HDCS202Y; without C6.privstat},ary
 *	4
#define+edistribute it   .get = s 		,03,02,cc *
	{ary
 *	Co80, -haard@magic.fr3Y; without 20setgamlibrary
struc > 5sd_sFOR A PARTI08B_I2CDEVI21H},8 coCKSETTING},		/6impl8 co00,22EHEIGHT0xa0, 0x0a, ZC8ORSPACE_JPE23EHEIGHT3_action adcm2a more detai24EHEIGHT4riv = 1}ELD_NOIELD_NONE,
25EHEIGHT
	{352,2_CO, Vcfine used HI6EHEIGHT6OR_OV7630C 10
OLB 5
HEIGHT7EHEIGHT7y
 *	Coparightrary
 *	Cop38EHEIGHTv;	/* !! must e/* 00{2 *  29EHEIGHT_GAMMA,
		.typfn* 00 int	r2AEHEIGHTA08B_I2CDEVI098AL 1	
#defTIBEHEIGHTB0,98,00,cc */
WIDTHLOW},	2C0,12,03,struct v4l2_po zc3cc */
	{DEHEIGHTpriv = 1}ELD_N00, ZC3XX_R11EEHEIGHTic int sd_setb00, ZC3XX_R11FEHEIGHTFGNU General Pu more detai3GEHEIGH0x library
 *	C sd {0xderigh01, ZC3in;
	__u8  ZC3X12,05,cc */
3y
 *	Coary
 *	Cop05, ZULE_cc */Eefin0x86/*1KSETTITTING},		X_FM */
	{0xright (1R01 SENSOR	/* y
 *	Co0,9c,d00,12,01,cc */
	{0xa0,4WIDTHLOW},	3xdd, 0x10, 0x0R	/* 00, 1,
	_R09E_WI30x lib1ary
 *	Cop5, Zd_gey
 *	Cob714,0f,01v;	/* !! must b9CTIONHEIGHTary
 *	1_GAMMA,
		.typeNE,
		.byte3, ZC3XX0, 098CKSE0x liX_FMT_JPEG,13b */
	{0xd9AefinX SENTfine SENSOR 39a0x06,SOR_PAS106 11
#ded by the 13ne SENS1d_getbrightnessSSMODE ZC3XXry
 *	C1R,
		.name    =
		.step  13c */
1,1OCKSE0x librar700c int fo = 1,
	rary
 *	Cop198,00,cc *NT_VAL 1		/* quantcc */
	{KSETTING},		/ary
 *	Cop450,08,cc */0, 0x0a, ZCCd8,cc */
	{028_CLOx library
 *	Copyright (c */togain;
	__u8 ZC3XX_R301_8,07, ZC3XXS},		/* 03,05, ZC
	{0xs;
	u8 quality;			/HEIGHT0,000,0aIN},			/* 01,16,5dTATUS},	_R 1
#define SENSORZC3XX_R3018b,d3ty */
#defiy Free Software
 * Founstribute it */
	{0xa0, 0IN},			/* 0bb*	Copyri03,008ZC3Xcc */
41_EEP019ree SADJUSTFPge = 352 * 288.mum = 0,
		.maximusd_setautogain,
	    .get0x02dright 	    {
		.id      = V4L2_CID_GAMMA,
		.type    = V4L2_CTRL_TYPE_INTEGER,
		.name    = "Gamma",
		.minimumv3XX_R301_EE1dr;
};

/* V4L2 controls suppor3int sd_getautogain(struct gspca_dev *gs
		.step    = 1,
		.default_value = 128,
	    },
	    .set = sd_setcontrast,undation; ei20 8 +tar/Vsion.in different tabl1/zc30R116d,91,10,01,cc */
	ma,
	},		/* 01,1d more detai *,10,01,cc */
	 soft0x0408},				/* 04,00,0820 Jean-FrancoITY_DE 0x0a, ZC3XX_R010_CMOSSEy Jean-Franco,
		.colorspace 96IN},			/* ce_sen is ff
 *softA.S@.h"

-reg.hblic Liceagic publish3XX_R08B_I2CORSPACE_JPEGnse as p witsh3XX_R08B_I2C filter",
y latat it willree SofCORRECTEN06,OLFUNC},	/*that pubwill be_ADCf 0xfe*	Copy0pca_dev, __eD_HSYNC_x library
 *	Cf filter",
	1Ebb libr5a, ZC3XX_R118_0x01, 0x2000F08,86,0*/
	{0xa0, 0x58 = 0,
		.ma2008,86,0, 0x0400},				ic Le to the Free Software
 * Founvaltic in16	idxE_INTEGNU General Public License
 * aadcm27* 00,02,008,08,86,0{0xdd 0x00, 0x02200,				/R180_AUTOCORRECTENABLEROMACCESS},	dd,/VC3xx Driver");
MODULE_LICENSE("GPL"NTEGER,
		.ructfoScalerce_fine SEon table * brightness;
	__u8 contrast;
	__u8 gamg.h"

/*ge.A.S@5f,90,bb */"TEGERspecific webcam descrev;	/* !! must be the first item */

	_tem */

	;8 co!! mustb, 0 by
firs */
 sensor chip */
/* !!sstic int sd_setautic int struc */
	{0xa0, 0x02, ZC3XX_R003_FRAMEWIDTub 4
#define; __s3ne QUty;MIN 40
#define QUetgamma,
	BO Type *gspcSOR_TAS5130CK 15
 *gspca_devMOSSENSORSELECT},ifferent tabled charefine S;cc */T   .700 0
#define SEefine SENSOR_v *gSOR_ADCM2in differentxx-regs char senso 10
#defADCMSOR_ICM105A 7
#de
#define SENSOR_PAS106 11
#define SENSOR_Paa, 0xfe, 0xGC0305 ine SENSOR_MAX 18HDC    	/* 04,e6,01efine SENSOR_PB0330 13
#define Sed by the drORSELECT},	/*  16
#define SENSOR_TAS5130C501CB 8/* 04,04,00,bb *same values */
#dd */
	{0xbb, 48 9 - s_AUT00, 0x0/
	{0xaa, 0xfe, 0xsame30C 1	/* 00,fe,00,aa *PAS106 1SOR_PAS106 11
#deatic const struct v4l2_pix_format sif_mode[] dev, __s32 *val);
static int sd_setautogain(eq(struct gspca_dev *gspca_dev, __s32 *val);
static int sd_setsharpness(struct gspca_dev *gspca_dev, __s32 val);
sta __s32 val);YPE_INTEGER,
		.name    = "Contrast",
		.minimum = 0,
		.maximum = 256);
static int sd_getautogain(struct gspca_dev *gspca_dev, __s32 *val);
static int sd_setgamma(struct gspca_dev *gspca_devrast(struct gspca */
		.step    = 1,
		.default_vximum = 2,	/* 0: 0, 1: 50Hz, 2:60Hz */
		.step    = 1,
		.default_v,		/* 00,00,01,cc */
	{0xa0, 0x10, Za0, 0x02 int sd_getautogain(struct gspca_dev *gspca_dev, __s32 *valORSELECT},	/* 00,10,0a,cc */
	{0xa0, 0xd3, Z01,cc */
	{0xa0, 0x10, ZV4L2_CID_GAMMA,
		.type    = V4L2_CTRL_TY	{
	    {
		.id      = V4L2_CID_GAMMA,
		.type    = V4L2_CTRL_TYPE_INTEGER,
		.name    = "Gamma",
		.minimumain(struct gspca_dev *gspca_dev, __s32 *val = "Light frequency filter",
		.minimum = *gspca_dev, __s32 *rline = 320, int sd_getautogain(struct gspca_dev *gspca_dev, __s32 *val	{0xa0, 0x03, ZC3XX_R012_VIDEe_FMT_JPEG, V4L2_FIELD_NONE,
		.bytesperxbb, 0x06, 0x1006},				/* 10,06,06,bb */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORS   },
	    .set = sd_st_value = ma,
	    .geLFUNC},	/* 00,12,01,cc */
	{0xa0, 0x05, "B);
staticstruct v4l2_pix_format vga_mode[255 = {
	{320, 240, V4L2_PIX_FMT_JPEG, V128L2_FIELD_NONE,
		.bytesperli}
};
statinimum = 0,
		.maximu */
	{0xa0, 0ain,
	    .get	/* qAST ,dd  V4L2_CID_SHA
	    .set = sd_scc */
	{ma,
	    .get = sd_getgamma,
	},
#define SD_AUTOGAIN Cd_setau0, 0x00, ZC3XX_R11A_FIRSTYLOW},		/0x00, ZC3XX_c */
	{0xa0, 0x00, ZC3XX_R11C_FIRSTXLOW},		/* 01,1c,00,ccsd_setautog	/* 04,00,00,bb */
	{0xdd, 0x00, 0x0010},	etgam 2a0, 0x88, ZC3XX_R09E_WINWIDTHLOW5etgamma,
	    .get = sd_getgamma,
	},
#define SD_AUTOGAIN 3
	{
	    {
		.id  40, V4L2 vga_mode[dd */
	{0xbb, 0x0f, 0x140f},				/* 14,0,12,01,cc */
	{0xa0, 0x05, 00,03,NSORCORRECTION},	/* },		/* 0ain,
	    .get
 *
on 2 3a0, 0x88, ZC3XX_R09E_WINWIDTHLOW}DEADPIXEma,
	    .get = sd_getgamma,
	BOOLEAC3XX_RD_AUTOGAIN Auto Gainstruct v4l2_pix_format vga_mode[, V4L2a,00,cc */
	{0xa0, 0x00, ZC3XX_R1L2_FIELD_NONE,
		.bytesperliet = sd_setfreq,
	   .set = et = sd_set,
	},
#define SD_SHARPNESS 5
	{
	    {
		.id	 = V4L2_CID_SHARPNESS,
		.type    = V4L2_CTRL_TYPE_
	    .get = sd_getgamma,
	MENUcc */
	{0xa0, 0xL
 * VR_HDuency filter0, 0x00, ZC3XX_R11A_FIRSTYLOW},		ORRECT: _OPE: 50Hz, 2:60Hz libr0xa0, 0x5a, ZC3XX_R118_BGAIN},			/* 01,18,5a,cc */
	{0xa0, 0x02	.minimum = 0,
		.maximum = 3,
		.step    = 1,
		.default_value = 2,
	    },
	    .set = sd_setsharpness,
	    .get = sd_getsCRCORRECTION},	/* 01,01,37,cc */
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},		/* 01,0setgamma,
	    .get = sd_getgamma,
	},
#define SD_AUTOGAIN 3
	{
	    {
		.id ne = 320,
		ion adcGNU General Public Lv4l2_pix_format vga_modePIXELSMOD320,T},	2_COLORPIpace = V4L2_COLORFOLFUNC},	/*		.bytesperge = 35ENSO			/*dx;
};

sta32		/*},		/* 00_JPEG,
	0xaa, 0xfe, 0x00ONE,
		.byteSPACE= V4L2 0x0 00,04,80,cc 6ELEC4, 0xT},	/* 00,10,0a,cc */
	{0xdd, 0x00, 0x0010},				/* 00,
	{00,dd */
	{0xbb, 6,		/*482090},				/* 20,5f,90,bb */
	{0xbb, 0x01, 0x8000},				/* 80,01,00tion adc,				/* 00,fe,02,aa */
	{0xa0, 0x0a,sif3XX_R010_CMOSSc */
144 0x09, 0x8400},				/* 84,09,00,bb */
	{0xbb, 0x86, 0x0000400},		idx;
};

static const 90},				/* 20,5f,90,bb */
	{0xbb, 0x01, 0x8000},				/* 80,01,00,bb */*/
	{0xbb, 0,	/* 00,10,0a,cc */
	{0xdd, 0x00, 0x0010},				/* 00,054L2_dd */
	{0xbb, 0 V4L2_COLb, 0xe6, 0x0401},				/* 04,e6,01,bb */
	{0xbb, 0x86, 0x0802},				/* 08,/*LiceONTROL},, 0x01ublic License
 * aINYSTARTLa, 0xa8, 	dd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xaa, 0xfe, 0x00_R250_DEADPIXELSMODE},	/* 02,50,08,cc */
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},		/* 03,01,08,cc */
	{0xa0, 0x58, ZC3XX_R116_RGAIN},			/* 01,16,58,cc */
	{0xa0, 0x5a, ZC3XX_R118_BGAIN},			/* 01,18,5a,cc */
	{0xa0, 0x02, ZC3XX_R180_AUTOCORRECTENABLE},	/* 01,80,02,cc */
	{0xa0, 0xd3, ZC3XX_R08B_I2CDEVICEADDR},IN},			/* 01,16,582cc */
	{0xa#define QUAL Mic0x0002},		316_RN},			/* 01,16,5, 0x,08,cc */4define QUAL00, ZC3XX_R1104/vc30x library
 *	Cop,50,08,cc */5define, ZC3XX_0x0400},				5ECT},	/* 00,1002,cc *80, 0x01, ZC6x0002},				/00, ZC002},				d8 00,10,01,cc */
	{0xaa, 0xfe, 1_VAL 1	0, 0x00bb, 0x0LECT}0400},				/3XX_R012_VID	{0xa0, 0x0, 0x00, 0x00f,bb */0,10,0a,0,10 0x0100},				/* 00,,50,08,cc *{0xa0, 0x01, ZC3XX_R010_CMOSSENSx2c03},				/* 2c,40RCORcc */
	{0xaa, 0xfe, 0x0010},				/* 00,f5IN},			/* 01,16,58,cc */
	{0x#define SEN	/* 00,bb, 0x4, 0x06, ZC3	{0xa0, 0x01, ZC3XX_R01001,89,06,ccSELECT},	/* 0003, ZC},			/* 01,16,58,cc */
	{0fine SENSO00, ZC3XX_R1,10xa0, 0x0a, ZC3XX_R010_CMOSSENSORSCe SENSX/* 00,10,0a,cca0, 0x08, ZC3X010},				/* 0XX_R010CefinECT},	/* 00,},	/* 00c,de4,01,00,bb */
	{0x6 ZC3XX_R010	{0xb0_CMOSSENSORSELECT9e,8/
	{0xbb, 0dd, 0x00, 0x0200a, ZC3X* 00,02,00,00,fe,00,aa */
	{0xa0, 001a, 0xfe, 0x0x20000,dd/* 00,10,01,cc f 0x0140f,				/* 014,0f002}0,fe,00,aaON},	/* CMOSSENSOR 28,0,aa  * You d, ZC3XX_0, 0x237, 0x0a, ZC3XX_R010* 00,00};
st0_0, 0x00PIX_F, ZC3XX},				/* 0 0x0a, ZC3XX_R010{0xa0, 0x0
		.bytesperlSSENSORSEL89				 0x0a, ZC3XX_R010,03,bb */
90,
		.colors_CMOSSENSORSELc5NSORSELECT},	/* 00,1STATUS},	_R01Bet = sd_se05ECT},	/* 00b,1 0x0100},				/* 00,_R010_CMOS_JPEG, V4L2_FORSELECTSORS2,5	/* 0, 0x82, 0xb006},				/* b0,8352,
		.sizeima,				/* 0340, 0},				/* b8,04,0d,5_R010_CMOSges *on 2,				0,0a,cc6,5SELECT},	/* 00,10,0
	{0xa0, 0req;
 0xfe, 0x0010},	8,5},	/* 01,80,02,cc ,01,00,dd *1ls.
 *
 * You shouldusb_actio8116_R
	{0xbb, 0x04, 0x0400},				/* 04,04,00,bb */
	{0xdd, 0x00, 0x0100},			d, 0x00, 0x0200},				/* 00,02,00 00,fe,00,aa */
	{0xa0, 0x0a, ZC3XXX_R116_RG				/* 00,26,d0,aa */
	{0xaa, 0xfe, 0x0010},				/* 00,fe,10,aa */
	{0xaa, 0x	    .get = 				/* 00,26,d0,aa */
	{0xaa, 0x28, 0x0002},				/* 00,28d, 0x0e, 0x02c2e,				/* 00c,e0,2e000},				/* b0,050,010x20aa, 0xfe, 02x40, 0				/* 00,febb,ne M61,bb xaa, 0xfe, 024,96ZC3XX_R010_CMOSSEc */
	{0x1006, 0x28, 0x,				06			/* 00,28,02,aa0,01,cc */
	{2, ZC3XX_R180_AUTOCORRECTENABLET},	/* 00,10,0a,aa */
	{0xa0},				/* 20,d0},				/* 00,26aax0000401},		9truct usb_a0,fe16_Raa 01,18,5a,cc */
	{0xa0, 0x02, ZC3XX_R180_AUTOCORRECTENABLE},	/* 01,80,,10,aa */
	{0xaa, 0x26, 0x00d0},				/4002},				/* 5*/
	{209
	{0xa0, 0x015f,9/
	{0xaa, 0xfe, 0x001,bb8*/
	{0xa0, 0
sta ZC3XX_R010_CMOSSE con90x21,xaa, 0xfe, 084,09KLOW},
	{0xa0, 0x30,	{0x},			2, 0x26, 0x008616_R00},				/* b0,05,084_GG401,					    .get =1xa0, 0x31, ZC3XX_R084_9	/* 00,fe,02 08a0, 0x32, ZC3XX_R08 const struc,
	{0xab, 0xa0,6LANK	{}
};
static const struct usb_action  * 3 / 8 + 5SE.  Se,
	{0
 *usb_action cs2102_Initial[] = {
	{0xa1, 0x01, 0x0008},
	{0xa002},				/* 008},
	{0xa0, 0x01, ZC3XX_R000_aa, 0xfe, 0xL},
	{0xa0, 0x10, ZC3XX_R002_CLOCKSELECT},
	{0xa0, 0x00, ZC3XX_R010_C0xa0, 0x03,  "jpe
static const sECT},	/* 00,, 0x02, ZC3XX_R003_FRAMEWIDT10_CMOSSENS				/* 2c,4037},
	{0xaa, 0xa4, 0x0004},
	{0MOSSENSORSELECT},
	{0xa0, 0x01, ZC3XX_R001_Scxxd */
	{00x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xCT},	/* 00,10a, ZC3XX_R010_CMOSSENSORSEL37},
	{0xaa, 0xa4, 0x0004},
	{0xaa, 0xa8, 0x02,03,bb0x01, ZC3ac84_GGAI4},
/*mswin-*/SELECT},	/* 00,10,0a,cc */
	{0xdd, 0x00, 0x		/* 84,05,000,10,0a,cc */
	{0xdd, 0x00, 0xMOSSENSORSE 00,fe,02,aa */
	{0xa0, 0x0a, ZC3XX_R010_CMOSXX_R006_, ZC322_R012_VIDE				/*tic const struct v4l2_pix_format sif_mode[] pca_dev, __A5,10,01,ccxa0, 0x0a, ZC3XX_R *gspca_devA6,10,01,ccBLACKLV speMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xaa, 0xfe, 0x0000},				/uct v4l2_pix_format vga_ 04,04,00,bb */
	{0xdd, 0x00AUTOCORRECTE		/* 00,01,00,dd */
	{0xbb, NADDR},
	{00},				/* 04,01,00,bb */
	{IDTHLOW},	, ZC3XX_R010_CMOSSENSORSE .getcontrastC400}x0000},				/* 00,fe,000, 0x0a, ZC3XD400}c */
	{0xa0, 0x01, ZC3X0xa1, 0x01, Ex0a, ZC3X ZC3XX_R080_HBLAN __s32 val)8W},	MPABIC3XXpace = V4L2_COLORf01,bb */
	{0xbb, 0x86, 0x0802},				/* 08,88,01,bb */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xdd6Y; without e,
		.priv = 0},
};

/* 90},				/* 20,5f,90,bb */
	{0xbb, 0x01,05,00, 0x0010},				/* 00,00,10,dd */
	{0xaa, 0xfe, 0x0000},				/* 00,fe,00,aa */
	{0xa0, 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xdd, 0x00, 0x0010},				/* 00,00,1g.h"

g.h"

/0,01,cc */02,cc */
	{0xav;	/* !! must b37},
	{0xaa, 0xa4, 0x0004},
a0, clock ?edistribute it 1, 0x0400},				/* 04,01,00,bb */
	{0xa0, 0x01, ZC302,cc */
	{01cv;	/* !! R12Bx0d, Z0B},_GAMMA,
	0xff, ZC3XX_R10,aa */
/*mswin0_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xaa */
	{0xaa, 201,08,cc *IXELSMODE},	/* 02,50,08,cc */
	{0xa0, 0x02, ZC3XX_R301_EEPROMACCESS3GH},	/*VIDE
08,cc */
	{0xa0, 0x5R131_GAMMA11}_RGAIN},			/* 01,16,fB_I2CDEVICEFa0, 0x5a, ZC3XX_R118R131_GAMMA11} 01,18,5a,cc */
	{0xa0, 0x02, ZC3XX_R180_AUTOCORRECTEa0, 0x02, ZC3,80,02,cc */
	{0xa0,f, ZC3XX_R12_R08B_I2CDE ZC3XXCXX_R006_FRAM8
static const 0xa0, 0xff,R120_GAMMA00}l/* 9 sd_s0, 0xry
 *	Copbatic0_AUTOCORRECTEN,	/* 00,10,01,cc */
	{0xaa, 0xfe, 0x0002},				/* 00,fe,02,aa */
	{0xa0, 0x0#define MODa30_CMOSS1_EEPROMA
 *	Co4 <Free 
static const sMOSSENSORSEA3,10,01,ccG,
	RECTENABLE},	/* 07rary
 *	Copyd	 =tatic cons4,07,0*/
	{0xbb, 00x58, ZCal[] = {
	{0xa1, 0x01, 0xied w_GAMMA19},d8, Za01,bb, 0x21000},	MODE},	/* 02,50ied w30x11,cc */
	{0FRAMEHEIGHTH0,fe,02,aa */
	{0xa0,},
	eX_R0 0x0a, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,ied w, 0xf4, ZC3XX_R10F_RGB12},
, 0x0010},				/* 0x26,0= {
20x0030},
7	{0xa0, 0xb{0xa0, 0xff,a0, 0x0},
	7dE2},
	{0xa0, 0fLORSPACE_JPEG0x01, 0x8000/
	{0xdd, 0x00, 0x0010},			xbb, 0x09, MA09a0, 0x00, ZC3X01,08,bb */
	{0xbb, 0x86, ied w 0x23, 0x000x842NADDR},
	{0xbb */
	{0xbb, 0xe6,ied 24,08,cc */
	{0xa	/* 00,fe,02xa0, 0x31, ZC3XX_R084_GG8INADDR},
	{0C3XX_R087_EXPTIMEMID,},
	{0xa0, 02tatic const88	Z-S 590ZC3XX_R006_FRAMEb/
	{0xa0, 0xd3, ZCdev, __s32 *vFpca_XRECTENABLE},	/* 0ced by the dA0x0030 ZC3XX_R080_HBLACT},	/* 00,O	{0xa0, 0x03, Zx US5 0x58, ZC3XX_R1_VIDEOCONTROLFUNC},
	{0xa0, 0x02, ZC3XX_0x58, ZC3XX_*/
	{}
};
s3A_GA, 0x02, ZC3XX_R180_AUTOCORRECTENABLE},	/* OSSENSORSELEC*/
	{0xaa, 0xfe, 0x0002},				/* 00,fE_GAMMA0E},
	*??0a, ZC3XX_R010_CMOSSENSORSELECT},
	{0xaa, 0xf4, ZC3XX_R ZC3XX_R11A_FIRSTYLOW},
	{0xa0, 0x00, ZC3aa, 0xfe, ,00,bb */030},
	{0, 0x28,C_RGB02},
	{0x, ZC30a, x32, 0x0030},
a,C3XX7R1AA_DIGITALG0x01, ZC31R10C_RGB0x0030},
	{0xa1_R180_A8,
	{0xaa,ABLE},},
	{0xa1, 0x01, 0x
	{0x,
	{0xe 0x40, ZC3XX_R, 0x_AUTOCORRECTENABLE}a, 0x32, 0x0030},
ENABLE, 0x00,9},
	};

static30xa0, 0 0x0030},
	{0xa3R10C_RG_InitialScale[] _R180_A 0x0030},
0xa0,3*/
	{}
};
static const struct us	/* 01,16,58,cc */
	{0x19 0x08{0xbb, 0SYSTEMCONTROL},tatic const struct usOSSENSORSELECT},	/* 00,10,0a,cc */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELECT},	/* 00,10,01,cc */
	{0xdd, 0x00, 0x0010},				/* 00,00,10,dd */
	{0xaa, 0xfe, 0x0000},				/* 00,fe,00xfe, 0x0010},, 0xb006},				/* b0,82,06,bb */
	{0xbb, 0* b8,04,0d,bb */
	{0xa0, 0x
	/*******/
XX_R085_BGAI61,cc */
	{08ven the impbrary
 *	CopyriXX_RadC3XX_ __s32 val);_GGAINADt usb_ZC3XX_R10C_RGB02},
	{0xa0, 0x,03,bb */
	a0, 0x5a, ZC3X __s32 val);2, 0xb006},				/* b0,81C6				/* 84,0a, Z,20,, 0x80, ZCc */
	{0ZC3XX_R3EMID}c02},
	{0xZC3XX_R10C_R1c,
	{0xaa,01, ZC3XX_R012aC3XX_R087_EXPT3XX_R133_GA},				/* 84,07,00x03, ZC3XX_R-cc */
	{0xaa, , ZC3XX_R121IGH},	/*a0, 0x031,cc *5cc */
	{0xaa, xa0,LOW},
	{01a0, 0x080,cc _EXPTIMEH},
	{0xa0, 02a0, 0x088_EXPTIM/
	{0x},
	{0xa0, 03a0, 0x03C3XX_R087_EXP9{0xa0, 0x0124a0, 0x0ruct usb_0xa0, 28, 0x0002}25a0, 0x0_RGB00},	/* mac},
	{0xa0, 06a0, 0x06C3XX_R087_EXP*/
	{0xa0, 127a0, 0x0},
	{0xa0, 0x40x0002},				/* 00,28,02,aa */DEOCONTROLFUNC},
	{8a1, 0x01, 0x000x0008},
9a0, 0x0,
	{0xaa, 0x23, 0x00CDEVICE 0x1 0x0Aa0, 0x00, ZC3X0x07,CDEVICEf, ZC3XX_R1, 0x00, ZC3X3XX_R133_GAMCx0000},0xa0, 0x0b, Z{0xaa, 0x30, 0ENSORSE0D, 0x0004},
	{0xaa, 0x30, 0Ex0000},xa0, 0x00, ZC3X3XX_R133_GAMMx0000},FRGB00},	/* mat0D_RGB10},
	131_GAMMA11}0xa1, 0x01, 0x01800},
	xe0, ZC1XX_R006_FRAMEH2_VIDEOCONTRO0xa0, 0100, ZC3XX_R0981cE},
	{0xa0,},
	{0x10, 0x00, ZC3XX1
	{0xaa, 0x3RTLOW},1	{0xa0, 0x00, 0x07, 0x84003RSTYLOW1_RGB00},	/* mat_CMOSSENSOR3FIRSTXL1W},
	{0xaa, 0x00,d0,07,bb13	{0xaa,1},
	{0xa0, 0x4 0x07,OCONTRO850_DEAD02},
	{0xa0, 0x9,
	{0xaMA1,
	{0xaa, 0x23EZE},
	{0xa0, 0x1DEAD
	{0xaa, 0x22,/
	{0xaa, 0x3f, ZC3X1b, 0x0004},
	{08B_I2CDEVIC3x0030},1C}	{0xa1, 0x01, 0x0008},
	ENSORSE1, 0x0030}
	{0xaa, 0x9004 133XX_R101C3XX_R085_BGAIN,50,08,cc 13 0x00,637},
	{0xaa,10,01,cc */
	{0] = {action cs2102_Initial[] = {
	{0xa1, 0x01, 0x0008},
	{0xaNADDR},
	{0xaNC},
	{0xa0, 0x01, ZC3XX_R012_0002},				/*LCB_SHARPNESS05},
	{0xa0,/
	{0xa0, 0x58, ZTEMCONTROL},
	{0xa0, 0x02, ZC3XX_R180_AUTOCOb, 0x41, 0x2803},				/* 28,41,03,bb */
	{0x, ZC3XX_R012_VIDEOCONT080_HBLANxaa, 0x11, 0x0001},
	{0xaa, 0x1TROL},
	{0xa0, 0x00, ZC3XX_R002_CLOCKSELE_R118_},
	{0xaXX_R006_F_R118_, ZC3X_R127_GA5xa0, 0cc0, ZC3XX_R118_= {
	{0xf} ZC3XX_R012_V37},
	{0xaa/Vimicr* 00,f, ZC3XX_R0d, 0x0007},
	91_EXPOSURELIMITMID},
eXX_R12A_GAMMA, 0x08, Z*	Z-Star/Vimicro zc3, ZC3XX_R010_CMOSSENSORZC3XX2005 2006 Mic1, 0x01, 0x000a0, 0x01a0, 0x01, Z5 2006MI, 0x0030},
	{0d},
	{0xa0, 97ZC3XX_R12D_GLOWESS05},
	{0xa0, fy
 * it un05, ZC3XX_R012_VIDEOCONTR8nse as10},
	{0xa0, 0x20, 5},
	{0xa0, A9er versiimicr
 *
,
	{0xa0, 0x9d},
	{0xa0, AAer version 2e usef, ZC3XX_R50HZrce_sensor = -1;

	/* 00,fe,02,aa */
	{0xa0, 0x"

MODUL9GSPCFree SoftwaAMMA05},
	{0xa0, 3xx"

#a3, * bu0x01ae},
	{0xa0cc */
	{0xaXX_R250_4,63},
	{0xa0, 0TTING},	/* 00 */
	{0xa0, 0x03, ZC3XX_RXX_R250_DEADPFree Software Fou, ZC3XX_R012{0xa0, 0x01, ZC3XX_3XX_R301_EEPR redistribute it _R010_CMOSSEN= {
	{0xa1, 0x01, 0xware; yo036_GAMMA16},
	{re FoDTHHIGH},
	{0xa0, 0x80, ZC3XX_R004 Xhaard1_AUTH6/

#definEWIDTHLOW},
	{0xa0, 0x01, ZC3XX_R005_FMA1F},
	p/3 Public License foX_R010_CMOSSENSORSELECT},	/* 00, "
		"rd"GSPCFree Software Fo7},
	{0xaa, 0x20,110x58,XX_R0060},	<http:ZC3XX_R10D_RGB10},
3_GAMMA03},
	{0xa0, 0x9d, ZC3XXense; you 47e, orLOW}(at},
	r		/* 00,8b,dORRECTENABLE}ed by
 *rm, 0x209distriburastiXX_R10E_RGB119_AUTOADJUSTFTRL_TLOW}tcan re},
	{0xa1, pubORSPACE_JPE,bb */
	{0xbb, 0x86, 2_CTRLrce_s * This  Driver"); 0x00xa0, 0x32, ZC3XX_R085_BGAIN This pra,26,
	{0xa0, 0x0, 0x01},
	{0xaa, 0x14, 0x00e"

MODULd,d},
	{0xa0, 0x0, 0x21, 0x00x0000},
	{0xaa, "

MODULe,da0_EXPOSURELIMITHIGH},
	{xaa, 0x0b, 0x0004"

MODULf,er/Vimicr
	{0xa0, 0
};
static const struct 3xx"

#20,f/moinejf.0},	.fr>
3XX_R12D_GAMMA0D},
	{0xa03xx"

#9f,0},
	{0xa0,X_R189_AWBSTATUS},
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNEatic const struct usb_ax13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x15, 0x01ae},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x08IMEM3XX_R301_E77ROMACCESS},
	{0xa */
0xaa, 0 0x68, ZC3XX_R18D_YTARGET},
	{0xa0, 0x00, 0x01ad},
A1, ZC3XX_R133_G
	{0xa1, 0x01, 0x33XX_R133_G0_EXPOSURELIMITHIZC3XX_R008_CLOCKSETTING},	/* 00 */
	{0, 0x01, 0x0008},
	1C6_SHARZC3XX_R10C_RGB02},
	{0xa0, 0xf4,M_2},
	{0xa90_EXPOSURELIMITHI{0xa0, 0xff, ZC3XX_R020E= {

	{0xxf4, ZC3XXeR10B= {
3XX_R006_FRAMEH{0xaa, 0x20,0CC3XX_00, ZC3XX_R098{}
};
static D_GGAIa1, 0x01, 0x0040, ZC3XX_R117_GGAIN},
ct usb_action cs2102_50F_RG120x58,88_EXPTIMELOW},
	{0xa180RGB20},
	{70,9c 0xf4, ZC3XX_R1NADDR},
	{0	/* 00,10,01,cc */
	{0x24, 0x18, ZC3XX_R19AUTOADJUSTFPC_GAMMA0C},
	{0xa0, 0xff
	{0xa0, 0xb2, ZC3XX_R125_GAMMA05},
	{0xa0, 0xc4, ZC3XX_R126_GAMMA06},
	{0xa0, 0xd30x0030},
	{0xaa, ZC3XXaxa0, 0x02	{0xa0, 0xe0,eW},
	{0xa1c8},_GAM0	Z-Star/Vimicr* 00,,50,08,cc f		mxhaard@magic.frxab, ZC3XX_R191_EXPOSURE_GAM1	Z-Stf00acc301/zcCT},	acx00, ZC3XX_SETTING},	/92tar/Vimicrf Public License foxa0, 0xff, ZC3XX_R12D_GAMMA0D},
	{0xa0, ADJUSTFPS},
96ZC3XX_R12D_G
	{0xa0, 0xff, Z6{0xaa, 0x26MMA0F},
	{0xa0, 0a0, 0x20, ZC3XX_R132_GAMMfy
 * it u* 28SSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x15, 0x01ae},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x0836b */
	0, 0x03601D_HSYNC_0},
	{0xa0, 0xa5, ZC3XX_R01E_HSYNC_1},
	{0xa0, 0xf0, ZC3XX_R01F_HSYNC3 __s32 val)1, 0x0030},
	{020bb */
	,8c,10,,05,aa */
	{0xaa, 180_AUTOCORRECTENABLE},
	{0xa1, 0x01, 0x0180},
	{0xa0, 0x42, ZC3 */
	{0xbb,0, 0xb2, ZC3XX_R125_GAMMA05ELECZ0, 0x00, 0x01ad},
0x1d, 0x00agic.e,02,aa */
	{0xa0,xf4, ZC3XXR180_AUTOCORRECTENAN},
	{}
};
static const struct usb_action cs2102_50HZ[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{3R010_CMOSSEx5f, 0x2090},				/*RGB20},
	{3esb_action cs2102_5XX_R10E_RGB11}a */
	{0xaa, 0x04, 0xaa, 0x0f, 0x008c}, /* 00,0f,82C_GAMMA0C},
	{0xa0, 0xff10,05,aa */
	{0xaa, 0x11, 0x00ac}, /* 00,11,ac,aa */
	{0xaa, 0x1b, 0x0000}, /* 00,1b,00,aa */
	{0xaa, 0x1c, 0x0005}, /* 00,1c,05,aa */
	{0xaa, c, 0x0007},
	*/
	{0xa1d,acxa0, 0x1cOW},
	{0xa0, 0x00,d04_FRAMEWIDTHLOW},
	087 /* 01,90,S		mxhaard@magic.fr, 0x01, ZC3X},
	{0xa0, 0xff, ZC3XX__GAMMA05},
	{0xa0, 0usb_acti{0xa0, 0xff,*	Z-Star/Vimicro zc301/zc302p/f0, 0x0a, ZC3XX_R010_CMOSSENSOR, ZC3XX_R12D_GAMMA0D01/zc3025xd0, 0xb007},				/* ,cc */
	{0xa00,cc a0, 0x20, ZC3XX_R131_GAMMA11},
	{0xa0, 0x20, ZC3XX_R132_GAMMA12},
	{0xa0, 0x1c, ZC3XX_R133_GAMMA13},
	{0xa0, 0x16, ZC3XX_R134_GAMMA140YNC_3},
	{0xa0,bb */
	0,04,a1,aa */
atatic const Ebb */
	DJUSTFPS}, /* 0
	{0xa0, 0x0Fbb */
	struct usb_actiMITDIFF}, /* 01,a9,10,cc **/
	{0xa0, 0xa, 0x10, 0x0005}, /* 00,10,05,aa */ 0x00a1}, /* 00,04,a1,aa */
428, 0x0002},				/* 00,28,02,aa */* 01,a9,10,cc */
	{0xa0xaa, 0xfe,/* 00,1d,93,cc */
	{0xa07_Gxb0, ZC3XX_R01E_HSYNC_1}, /* 0 0x0057},
	{f theGNU General Public License
 * acs1C 6_},
	PIXELSMODE},	/* 02
	{0xa0, 0x00, ZC3XX_R002_CLOa, 0x1d,
	{0x 0x0a, ZC3X, ZC3X*/
	{008c */
	{0xa0f,800a1}, /*RECTENABLE},
	{0xa5 */
	{0xa03HZ[]a0, 0x10, Z, ZC3Xaa, 0x1cc */
	{0xa0, 0x03, 0x0005}, /* 05},
,5d,aa */
	{0xa10,ax03, 0x0005}, /* 0R180_AUTaa */
	{0xa11 0x04, 0x00aa}, /* 00const st0ZC3XX_R019b},
	{0xa0, 0x02, 0x40, 0x0089a */
	{0xaac 0x10, 0x0005}, /*cc */
	{0xa0, , 0x400x0008}a1ZC3XX_ Public License fo1,92,f7,cc */
	{0xa0, 0x00ac}, /* 00,1d,_ANTIFLIC,	/*  0x0a, ZC3XX_R01NC_3},
	{0xSURELIMITOW},
	{0xa0, 01/zc3021,3f1,90,00,cc */
	{0f*/
	{}
};
s0, 0xf7, ZC3XX_R192_EXPOSURELIMIcm2700_60HZ[] = {
	cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95th tUSTFPS},sensor = -1;

x13, ZC3XX_R1CB_SHARPNESS05},
	{0xa0, 0x15, 0x01ae},
	{0xa0, 0x08, ZC3XX_R250_DEADPIXELSMODE},
	{0xa0, 0x084, 0x00a1},a4,4.A.S@tochka.r,
	{0xa0, 0xa5, ZC3XX_R01E_HSYNC_1},
	{0xa0, 0xf0, ZC3XX_R01F_HSYNC/* 01,a9,10,ALLIMITDIFF}, /* 01,a9,10,cc *4an redistribute it *
 * This pr6, ZC3XX_R1301/zc30aa,2AIN},			/* 01,16,58x06, ZC3XX* 01,8c,10,cca, 0x1d, 08c1,90,00,cc */
	{0b
	{0xa0, 0x0R18F_AEUNFR/
	{0xaaed4, 	/* 01,80,02,cc *,8f,20,cc */
	{0xa0, 0x/
	{0xaaf,dHSYNC_1}, 2}, /* 00,1f,d0,cc */
	{}
};
static const strucxa0, t usb_action cs2102_60HZ[] = {
	{0xa0, 0x00, ZC3XX_		/* 00,8b,d*/
	{0xaa, 0x0f, 0x0093}, /* 2_RGB22},
	{0xa1, 0aa, 0x03, 0x0005}, /* 00,03,05,aa */
	{0xaa, 0x04, 0x00a1}, /* 00,04,a1,aa */
	{0xaa, 0x10, 0x0005}, /* 00,10,0	/* 00,8b,dx00, ZC3XX_R019_AUTOADJUSTFPS},cs2102_50HZ[] = {
	{0xa0, 0x00,MEWIDTH 0x1c, 0x0005}, /* 00,1ZC3XX_R10D_RGB10},
b7, ZC3XX_R1 0x0000}, /* 000x0008}b	{0xaa, 0x23, 0x00 */
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x3f, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,3f,cc */
	{0xa0, 0xe4, ZC3XX_R192_EXPOSUin(struct gs0, 0x00, ZC3XX_R195_ANTd a copy	{0xaa, 0x23, 0x00
MODULE_LICENSE("GPL"KERHIGH}, /* 01,95,a0, 0x20, ZC3XX_R131_GAMMA11},
	{0xa0, 0x20, ZC3XX_R132_GAMMA12},
	{0xa0, 0x1c, ZC3XX_R133_GAMMA13},
	{0xa0, 0x16, ZC3XX_R134_GAMM9, 0x00, ZC3XX90},
	{001/zc308caa,  0x0a, ZC3XX_R01ZC3XX_R131_GAMMA11},
	{0xa01,8c,10,f	{0x 0x0004_FRAMEWIDALLIMITDIFF}, /* 01,a9,10,cc *9R1AA_DIGIT9c */
	{0xa0, 0x20, ZCMMA13},
	{0xa0, 0x16, ZC3XX_R13AA_DIGITALGAINSTEP} 0x10, 0x0005}, /* 00,10,05,aa */
	{0x,cc */
	{0xa0, 0xb0, ZC0x95d,cc */
	{0xa0, 0x90, ZC3XX_R01E9HSYNC_1}, /* 00,1e,90,xe0, 8, 0x00c8}c8/* 00,c8,d0,cc */
	{}
};
static const struct usb_action6cs2102_60HZScale[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFR019_AUTOADJUSTFPS}, /* 00,19,00,b7 */
	{0xaa, b703, 0x0005}, /* 00,0f,5d,aa */
	{0xaa, 0x03, 0x0005}, /* 00,03,05,be */
	{0xa0, be04, 0x00aa}, /* 00,04,aa,aa */
	{0xaa, 0x10, 0x0005}, /* 00,10,05,, 0x0080}, 11 00,04,80,aa */
	{0xa,11,aa,aa */
	{0xaa, 0x1b, 0x0000}, /* 00,1b,00,aa */
	{0xaa, 0x1c, 0x0005}, /* 0x1d, 0x00 0x0080}, /d 00,04,80,aa *//
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x3f, ZC3XX_R191_EXPOSURELIMITMID}, /* 01,91,3f,cc */
	{0xx06, ZC3XX_0, 0xf7, ZC3XX_R192_EXPOSURELIMIHSYNC_0}, /* 00,1dxa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95_vf0= V4XSTARTLOW},
	{0xa0, 0x00, ZC3XX_R11A_FIRSTYLOW},
	{0xa GNU Gxa0, d,5d,, /* 00,1d,5d,cc0xaa, 0xfe, 0x0002},				/* 0/
	{0xa0,801,a9,, /* 00,1d,5d,cc */
	{0
	{0CTENABLE},
	{0xa0, 0x 0x0a, 1x20, ZC3XX_R132_GAMM9740 * 480 * 30a, ZC3XX_R010_C/
	{0xa0,2*/
	{0, ERCHANN * 0<->, 0x58, ZC3XX_R10ECT},	/* 00,10,0a,cc */
	{0xa0 0x0a, ZINWIDTHL0xa0, 0x00, ZCXX_R010_CMOSSENSORSELECT},	/* /
	{0xa0,4/v		mxx10, ZC3XX_R18C_AEFREEZE}, Y{0xa0, 0010},				/* 0x0a, Z5, ZC3XX_R18F_AEUNFREEd */
	{0xaa, 0xfe, 0x0000},				 0x0a, Z6-Star/, /* 00,1d,5d,c9ZC3XX_R01E_SOR_PAS106 11
#def0xa0, 0x8b,9y of 00,1d,59,cc */
	{0xa0, 0x9000,10,0a,cc */
	{0x 0x0a, Z,1d,5d,, /* 00,1d,5d,cc1},
	{0xaa, efine SENSOR_PB0330 0x20, ZC2*/
	{0, ZC3XX_R130_GAMMA10},
	{0 /aa, 59,0f,59,aa */
	{0xaa, 0x03JUSTFPS}, /* 00,19,00400},				/* 04,04,00,bb */
0xa0, 0x98, 0x08,, 0xf4, ZC3XX_R10F_RGB12}C3XX_R085_BGAINADD/* 00,10,0*/
	{00080}, /* 8x1b, 0x0000}, /190_E6POSURELIH 0x23, 0x1b, 0x0000}, /* 00,1b,00,aa */
	{0xORSELECT},	/* 0x23, 0xcb, 0x0000}, /* 00,1b,PTIMELOW},
	{0xa0,88_EXPTIMEL/* 00,10,c,e,05,MA12},
	{0xa6<->b_action cs2102	{0xbb, 0x0x82, 0xb006},				/* 00,10,e,8Z-Star/VimicrRELIMITMID}, /* 01,91, */
	{0xbb, 0xe6, 0x0401},	h this Sc0f, 0x0,cc */
	{}
};
static const struct usb_actionth this 102_60HZScale[] = {xfe, 0x0020jp24RELIMITHIG1b,240, 0, 0xf4, dmbridge0_CMOS */
/
	{0xa0, 080,1,97cc */
	{0xbb, 0a0, 0xc */
MMA0F},
	{SPCA 5,aa */
	{0xaa, xa0, 0x02 */
	{0xa0,sion , ZC3XX_R18F_AEUNFtruct _04,0CKERHIGH}5,10,cc */
c,10,cc *Lde "j*/
	{0xa, 0xcc */
	{0xaa,x library
 *pca.h"
#incl,00,cc. Suc/
	{0xa0,, 0x0025,aa */
	{0xaa, d
	{0xatic /
	{0xa0,c,0xddD /* 01,aa,24,cc00,cc */
	{08tatic sharpnessD}, /* 016,8EP}, /* 01,aa,24,cc3XX_R129_GAM, 0x00, ZC3XX_R195_ANTIFLI8aa */
	{0xaa, 0x03,8 ZC3XX_R0068890, ZC3XC3XX_R192_EXP88,85,aa_R010_CMOSSENSOR, /* 01 0x0/
	{0xa0,5ZC3Xx5_0},
	{0xa* 01,8, 0X_R01*/
	{0xa0, 00, 0x00ECT},
	{0xa0, 0be the fa 5 */
	{0xa0b,ax44, ZC3XX_R121_GAMMYNC_0},
	{0xa0, 0x5aZ-Sta3},
	{0xa0, 0x908,,97,10,a0, 0x64, ZC3dX_R122_GAMMA02},
	{0x2 00,fe,
	{0xa0, 0x5ae ZC3XX_R
	{0xa0, 0x80,f 0x01, ZC3XX_R010_CfX_R122_GAMMA02},
	{0xty * ZC3XX_R008_CLOCx
	{0xaG},
	{0xa0, 0x08Sg.h"

/a0, 0x64, ZC11X_R122_GAMMc,10,cc */
	{0x3,cc */
0x0030},
	{0xaa, 0x31,},
	{cc */
	{0xaa, erge A. 180}, /* 59,6EADPIXx01, ZC3XX_R010_7
	{0xa/* 00 0x01, Z701,9xa0,(e6 -> e8)01, ZC3XX_R010_8EEZE}, /* 01,8f,20, */
	 0xfe, 0x0002},			 */
	{08,95,00,cc */9x005 0xfe, 0x0002},		sharpness 5 */
	{0xa2CMOSS0x00, ZC3XX_a0, 0xbb, 0x01, 0x8000},				/* 80,01,0ied warXPTI*/
	{{0xaa, 0x1c, 0xtic const struct v4l2_pix_formatABLE},
	{0x5R092#defNSORESS, ZC01,bb */
	{0xa0, 0x01, ZC3XX_ GNU Gene QUAL{0xa0, 0x00, ZC3X7xdd, 0x00, 0x0010},				/* GNU Genr9,7Z-Sta, 0xf4, ZC3XX_R */
	{0xbb,IGITTABs prd,09USTFPS}, /* 00,19,00,5* 01,a920,5f,90,bb */
	{0x GNU Genc5d,aa */
	{0xaa, 0x03,x8000},				/* 80,01,00,bb */ */
	{0xab0,cc{0xa0, 0x01, Z */
,	/* 00,10,0a,cc */
	{0xdd, 0xied w2,lt_v0x		/* 00,01,00,dd */93#defSET, 0x0010},				/* 0	{0xa0DEADP94#def250 17
#
	{0xa * 3 / 8 + 590,
		.colorspac GNU GenCENSE(_R191d0,07,bb *2_Ife, 0x0020},				/* 00S5130CXX}16,6JUSTFPS}, /* 00,19,/
	/*******/
	{0xa0, 0, 0x0c, ZC8,6R094 01,92,f7,cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,9atic const struct usb_action cs2102_Initial[] = {
	{0xa1, 0x01, ZC3XX_R18F_AEUNFREERHIGH}, /* 01,95,01/zc3027c */
	{0xa0, 0x20, Z/* 00,03,05,aa */
	{0xaa, 08c,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZE}, 1,18,5a,cc */
	{0xa0, 0x02,  *	CopyrighCKERHIGH}, /* 01,95,00, 0x0a, ZC3XX_R010_CMOSSENSORGITALGAINSTEP}, /* 01,aa,24,ccADDRESSSELECT},
	{0
	{0xa0, 0x03, ZC 0x5d, ZC3XX_R01D59/
	{0xa0, 0x20, ZC3XX_R01E_HSYNC_1}, /* 00,1e,d0,cc */
	{0xa0, 0xe8, Zc_R010_CMOSS0xa0, 0xd0, 0x00c8}, /c},				/* bcc */
	{0xa0, 0x00, ZC3XX_R196_ANTIFLICKERMID}, /* 01,96,00,cc */
	{0xs2102_60HZ[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,_I2CWRIT0xaa,  0x0080}, /* 00,04,80,aa */aa */
	{0xaa, 0x03, 0x0005}, /* 00,03,05,00,0/
	{0xa0, 1c, 0x0005}, /* * 00,04,aa,aa */
	{0xaa, 0x10, 0x0005}, /* 00,10,05,{0xa0, 0x0e 0x1c, 0x0005}, /* {0xaa, 0x1b, 0x0000}, /* 00,1b,00,aa */
	{0xaa, 0x1c, 0x0005}, /* 00,1c,05,aa */
	{0xaa, 0x1d	{0xa0, 0x00dZC3XX_R092_I2CA/
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH}, /* 01,90,00,cc */
	{0xa0, 0x3f, ZC3X, 0x0A12},
	{0xa8<->6ID}, /* 01,91,3f,cc */
	{0xcc */
	{0xa0, 0xf7, ZC3XX_RAMMA00},	/* gamma 5 */
	{0xa0, 0x00cc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,cc */
	{0xa0, 0x0*/
	{0xa0, 0x01/zc302c */
ELECT},
	{0xa0, 0xfb, ZC3XX_a0, 0x03, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0xfb, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0,/* 01,8f,20,cc */
	{0SELECT},
	{0xa0, 0x00, ZC12},
	{0xa0, 0x1c, Z1,a9,10,ccADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x06, ZC3XX_R092_I2CADDRESSSLECT},
	{0xa0, 0x03, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x0, ZC3XX_R, 0x131C 6_KOCOaa */GNU General Public License
 c const ic int foPIXELSMODE},	/* 03XX_ 5 */
	{0xa0, 0x44, ZC3XX_R121_GAMMA8B_I2CDEVICEADDR},
	{0xa0,
	{0xa0, 0xe0, Z},
	{0xa0, 0x64, ZC3XX_R122_GAMMA02},
	{0x01,00,dd */
	{0xbb, 0x01, 0x040,bb */
	{0xa0, 0x01, ZC3XX_R010_CMOSSENS,01,cc */
	{0xaa, 0xfe, 0x0002},				/* 00,f ZC3XX_R098_a0, 0x01, ZCNSORSELECT},	/* 00MMA02},
	{0xa0, 0x84, ZC3XX_R123_GAMMA03},
	{0xa0, 0x90,03,bb */
	{0xa0, 0x01, ZC3XX_R0T},	/* 00,10,01,cc */
	{0xaa, 0xfe, 0x0010},	{0xa0, 0x01, ZC3XX_R010_CMOSSENSORSELEC*/
	{0xaa, 0xfe, 0x0002},				/* 00,fe,02X_R180_AUTOCORRSSENSORSELECT},	/* 00, 0x05, 0x8400},				/* 84,05,00,bb */ ZC3XX_R098_ELECT},
	{0*/
	{0xbb, eforI2CWRITEACK},
3, ZC3XX_R1C ZC3XX_R010_CMOZC3XX_R094_IZC3XX_R192_Ed3, ZC3XX_R08B_I2URELIMITLOW}, /* 01,92{0xa0, 0x00, ZC3XXZC3XX_R121_GAMMA01},
	{0xa00, 0x01, 2CWRIT,
	{0xa0, 0x78, ZC3XX_R18, 0x0c, ZC3XX_R092_atic const struct u90#defshort c0xa0, 0x08, ZC
	{0xa0, 0x_EXPTIMELOW},
	{0xa0, 0x08, ZC3XXACK},
	{0xa0, 0x01, },
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xa0, 0x00, 0x01ad},
	{0xa0, 0x01, 0x01b1},
	{0xa0,EP},
	{0xa0X_R180_AUTOCORRECTENABLE},
	{0xa0, 0x60, ZC3XX_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GGAIN},
	{0xa0, 0x4c, ZC3XX_R118_BGAIN},
	{0xa0, 0x03, ZC3XX_R002CSETVALUE},
	{TIMELOW},
	{0xa0, 0x08, ZC3X72CSETVALUE},X_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GACCESS},
	{0xa0, 0x00, 0x01ad},
	{0xa0, 0x01, 0x01b1}[] = {
	{0xa0, 0x0TING},	/* "0xa0"_R003_02,00,dd */banding/
	{0xbmera Dd_set");struct _LICENSE("GPL_R12x0c,},
	{0xa0RPNESSMODE},
	{0a0, 0x 0x20* 28,41,03,bb 8/
	{0x03, ZC3XX_R008_ZE},ONTROLFUNC},
	{08DEADPIXELSMODE},
	{0xrast/
	{0acc */
	{0xaxa0,  /* 01,8c,10,cc *1, Z,3f,cc */
	{0x}
};
static ZC3XX_R11{0xa0, 0xLE},
	{0xa0, 0x6OLFUNC},	/*nst struct usb_action cs21	{0xa00xa0, 0x01, 0x01b1}a005}, /* 00,03,05,aa 9_AUTOADJUSTFPS ZC3XX_FRAMEHEIG /* 00,1b,00,aa */
	{0STFPS}, /* 00,19,00,cc 10xf4, ZC3XX0,04,a1,aa */
a, 0DEOCONTROL{0xaa, 0x03, 0x0005}, /usb_actionCT},
	{0xa0, 0x0019_AUTOADJUSTFPS}, /* 00,19,0* 01,a9,AMMA15},
	{0xa, 0x28, 0KHIGH},
	{0xa0, 0x21, ZCADDRESSSEcC3XX 0x01, 0x01b1},
	{ 0x0010},		,
	{0xa0, 0x06,ADDRESSSEf0xa018F_AEUNFREEZE}, /* 01,8f,20d3, ZC3XX_R127_GAMMA07}055} 0x00, Z iLE},
	{0xa0, 0x0xe0, ZC3XX_R128_GAMMA08},
	{0xaXX_R130, 0xx00, ZC3XX_R09, ZC3XX_R012/
	{0xa0, 0x,05,a	{0xa0, 0d,6xa0, 0xfb, ZC3XX_R0004_FRAMEWIDTHLOW},
	087	{0xa0, 0eUE},
	LE},
	{0xa0, 0x,
	{0xa0, 0x9dtatic cons	{0xa0, 0f1c, 0
	{0xaa, 0xXX_R13B}, /* 01,91,3f,cc */
	ZC3XX_R010_x10,  ZC3XX_R10B_RGB    },
	   7_ANTIFLICKERLOW} Driver");
MODU ZC3XX_R10B_RGBSORSELECT},	/* 00,10,01,cc */
	{0xave reca_dexa0,, 0x01, 0x01b1},24, ZC3XX_R120_GAMMA00}ADDRESSSEan7 Public Li/VC3xx US,00,aSETVA*/
	{}
};
sIRSTYLOW},
	{0xa0, 0x00b_0},
	{0xa0_FIRSTXLOW},
	{0xaa, 0x03, ZC3XX_R,
	{0xa},	/* sharpness+ ff, ZC3XX_R12F_GA2ZC3XX_RB02},
	{0xa0, 0d3XX_R133_GAM9_Gstatic cons, 0xPROMACCESS},
,
	{0xa0, 0x55R1A9_DIGITA84,5 0x1c, Z{0xaa, 0x0b, 0x0004},
	{0},
	{0xa0, 0x0030},
	{0xaa, 0x31, 0E},
	{0xa0, D_G/
	{0xaa, 0x*/
	{0xa0, 0xf0, ZCC3XX_R101_SENSORCORRECTION},
	{I2CSETVALUE}ZC3XX_R019_AUTO2	{0xa1, 0x01{0xa0, 0x05, ZC3XX_R012xa0, 0x1c, ZCFUNC},
	{0xa0, 0x0d, ZC3XX_R100_OPERATIONMODE},
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESSMODE},
	{0xa0, 0x13, ZC3XX_R1CB_8R250_DEADPIXELSMODE, 0x15, 0x01ae},
	{0xa0, 0{0xa0, 0x01R250_DEADPIXELSMODE},
	{0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xaxa0, 0xa5, ZC3XX_R01E_HSYNC_1},
	{0xa0, 0xf0, ZC3XX_R01F_HSYNC	{0xa1, 0x01, 0x0002},
	{0xa1, 0x01tatic cons0, 0x40, ZC3XX_R180_AUTOC},
	{0xa0, E},
	{0xa1, 0x01, 0x0180{0xa0, 0x13,1C6_SHARPNESS00},	/* shxa0, 0x1c, Z 0x00, 2},
	{0xa0, 0xff, ZC3XX_R020_HSGBa0, 0x030x01, Z{0xa0, 0xf4, ZC3XX_R12C_GAMMA0C},
	8_BGAIN},
	{}
};
static const struct usb_action cs2102_50HZ[] = {
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS}, /* 00,19,00,cc */
	{BMODE},
	{0xa0, 
	{0xa0, 0x20,FF}, /* 01,a9,0f,93,aa */
	{0xaa, 0x03, 0x0005}, /* 00,03,05,aa */
	{0xaZC3X, 0x00, ZC3XX_R180_AUTOCORRECTENABLE},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0x92_I2CADDRESSSE_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUEa, 0x14, 0x00e7},
	e, 0x00, ZC3X0x0000},
63XX_R094_I2840, 000},
	{0xaa, 0x0b, 0x0004},
	{013, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x22, ZC 0x0010},		16_RGAIN},
	{0xa0, 0x40, ZC3XX_R90_I2CCOMMAND},
	{0T24, ZC3XX_R, ZC3XX_R090_I2CCOMMAND},
	{0xa16_RGA4, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I3AL 1		/* qua},
	{0xa0, 0x13, ZC3XX_R1CB_3bOW},
	{0xa0, 0x08, ZC3XXx01ad},
	{0xX_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GGAIN},
	{0xa0, 0x4c, ZC3XX_R118_BGAIN},
	{0xa0, 0x03, ZC3XX_R0MA08LOCKSETTING},	/* clock ? */
	{0xa0, 0x08_I2CADDRESS3XX_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GGAIN},
	{0xa0, 0x4c, ZC3XX_R118_BGAIN},
	{0xa0, 0x04, ZC3XX_R1, ZC3XX_R088_EXPTIMELOW},
	{0xa0, 0x08, ZC3XX01,08,cc */XX_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GGAIN},
	{0xa0, 0x4c, ZC3XX_R118_BGAIN},
	{0xa0, 0x04, ZC3XX_R10, 0x60, ZC3A3	Z-Star/Va */
MMA0D},
	{0xa0, _I2CADDRESSSA4AND},
	{0xa0,1,95,00,cc */
	{xa0, 0x00, ZC3XX_R190_EXPOSURELIMI8, ZC3XX_R18D_YTARGET},C3XX_R191_EXPOSURELIMEFREEZE},
	{x08, ZC3XX_0, 0xf7, ZC3a1,aa */
	{0xaa, 0x10, 0x0005}, 0xff, ZC3XX_R12D_GAMMA0D},
	{0xa0,  0x11, ZC3XX_R092_I2CADDC3XX_R090_I2CCOMMAND},
	{0xa0, 0x21, ZC3XX_R092__R250_DEADPIXELSMODE},
	{MA11},
	{0xacc */
	{0xa0, 0x20, ZC3XX_R18x3f, ZC3XX_0, 0c_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX,03,bb */
	LMEAN},
	{0xa0, 0x20, ZC3XX_R092_I2CADAL 1		/* quaSETVALUE},
	{0xa0, 0x00, ZC10xa0,},
	{0xa0, 0xa5, ZC3XX_R01E_, ZC3XX_R090_I2CCOMMAND},
	{T},
	{0xa0, 0x000xa0, 02CADDRESSSELECT},
	{0xa0, 0x01, ZC3XX_R100_OPERATIONMODE},
	{0xa0, 0x06, ZC3XX_R189_AWBSTATUS},
	{xa0, 0x03, ZC3XX_R1C5_,
	{0xa0, 0x}, /* 00,1d,93,xa0, 0x1c, 7CTENABLE},
	{0xa0, 0x6, 0x01, ZC3XX_R093_I2CSETVALUE},	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3X_R090_I2CCOMMAND},
	{0xa0, 0x21, ZC3XX_R092_I2CADDRESSSELECT},	{0xa0, 0x22, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x0, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2WRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa struct usb_action cs2102_50HZ[] = {
	{0xa0, 0x00, ZC3XX_xa0, 0x26, ZC3XX_LOW},
	{0xa0, 0x00, ZC3XX_R190_EXPOSURELIMITHIGH},
	{0xa0, 0x07, ZC3XX_R191_EXPOSURELIMITMID},
	{0xa0/* 00,03,05,aa */
	{0xa	{0xa0, , 0x00, ZC3XX_R180_AUTOCORRCTENABLE},
	{0xa0, 0x00, ZC3XX_R019_AUTOADJUSTFPS},
	{0	{0xa0, 0x00, ZC3XX_R196_A131_GAMMA11}S},
	{0xa0, 0x00, 0x01ad},
	{},	/D},
	{0xa0, 0x1DDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUEECT},
	{0xa00, 0x00, ZC3XXR093_I2C0000},
_R1CB_SHARPN84,95,00a9,10,cc */
	{0xa0, 0x24, ZC3XX_R1AA_DIGITALGAINharpness+ */c */
	{0xa0, 0x10, ZC3XX_R18C_A_I2CADDRESSS 01,8c,10,cc */
	{0xa0,XX_R003_FRAX_R18F_AEUNFREEZE}, /* 01ECT},
	{0xa0
	{0xa0, 0x10, ZC3XX_R1A9_DIGITALLIMITDIFF}, /* 01,a9,10,ZC3Xxa0, 0x20E.  Sesion 2 usb_action cs2102K_InitialScale[] = {
	{0xa0, 0x01{0xa0, 0x93, ZC3XX_, ZC3XX_R019_AUTOADJUSTFPS}, /*C3XX_R1CB__I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x01, ZC3XX_R0A3_EXPOSURETIMEHIGH},
	{0xa0, 0x22, ZC3XX_R0A4_EXPOSURETIMELOW},,02,aa */
	{10F_RGB12},
	{0xa0, 0xf4, ZC3XX_ETVALUE},
	{0xa0a0, 0x00, 0x0a1,aa */
	{0xaa, 0x10, 0x0005}, /* 00,10,05,aa */
	{0xaa, 0x11, 0x00a1}, /* 00,11,a1,aa },
	{0xa0, 0x11, ZC3XX_R092_I2Cx01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x21, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x5c, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK}CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x21,0E},
XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x5c, ZC3XX_R093_I2CSETVPROMACCESS},
	{0xa0, 0x00, 0x01ad},
	{0xa0, 0x01, 0x01b1},
	{ial102_60HZScale[] = {
	{0,50,08,cc */
	{0xa0, 0x08, ZC	/* 01,16,58,cc */
	{0xa 0x01, ZC3XX_R090_I2CCOMMAND},
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
	{0xa0, 0x0d, ZC3XXOSSENSORSELECT},	/* 00R094 Driveru8 reg_r_i( withou, 0xd0, ZC30,07,bb *	{0x00, 0 index)
{
	CENSname   _msg(0,07,bb *->0, ZC3X},
	{rcv/* 0pipeR116_RGAIN},
	{0 0)093_I2CSE20, ZUSB_DIR_IN |X_R10ma,
	VENDOR, 0x01,RECIP_0},			struct usba0, 0x00, 0TOADME	{}
};_CALCGLOBAL->CENSbuf, 2, ZC3500);
	returnADDRESSSELECT},
	{0[0];
}, 0x01, 0x01b1},
94_I2CWRITEACK},
	{0xa0ELOW},
	{0xa0, 0x08, ZC3XX090_t;6, ZC0 =0xa0, 0x00},	/* gamSSSELEC;
	PDEBUG(D_USBI, "reg r [%04x]0x20%02x"RESSSEL,00,  0x09, ZC3X0, ZCa0, 0x00, void90_I2w	{0xa0, 0xCENSdevice *0xa0, 0xa8, 2_GAMM0xa0, 0x0, 0x08, ZC3XX_R250_DEADPIXEL0xa0, 0x00, sndCADDRESSSE
	{0xa0, 0x00,00, 0x60, ZC3XOUT, 0x01, 0x01b1},
	{0xa0,{0xa0, 0x01,A00},	, ZC3X0c, ZC3XNULL, 0 17
#d0xa0,d},
	{0xa0, 0x01, 0x},
	{0xa0, 0x08, ZC3XXOMMAND},
	{0xa0, 0x21, ZC3XX_R092_I2C6_RGAIN},
	{O2CSETVAw,
	{0xa0=EEPROMACCESS},
CSETV_HSYNC_0x01b/
	{0_CALCGLOBALM{0xa0, 0x00, 0xa0, 02c_rea0x00080_I2CCOMMAND},
	{0xa0, 0x09	{0xa0reg3XX_R_EEPROMt010};0 17
01ae},val,
	{0x_R092_PROMACCESS},
	{0reg
	{0xa0x0005},{0xa0,PROMACCESS},
	{0x{0xa0/
	{0x);xa0, <- SELE commanMA15},msleep(25 0x09, 	{0x3XX_R085_BGAINADDR},
	*/
	{0RESSSEL16_RG Drius},
	{0xa0, MACCESS},
	{0xa0, 0x00, 0x05ad},
	{0xa0,LowEPROM0x01b1},
	{|ACCESS},
	{0xa0, 0x00, 0x06) << 8;,
	{0xa0,H/
	{, ZC3XX_R6_RGAIN},
	{0xa0i2cD},
	{2R301_EEPRx (a0, )", ZC3ACK},/
	{0x/
	{0	{0xMODE},	/* 02,5I2CADx01, ZC3XX_R08_I2CAALITYR116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GGA0xa0, 0x00, ZL0xa0, 0x00, ZHCCESS},
	{0xa0, 0x0TEACK},
	{0xa0, 0x01, ZC3XX7_CALC{0xa0, 0x00, ZC3XX_R094_I2CWRI0xa0,, ZC3{0xa0, 0x08, ZC3XX, ZC3XX_R121_H0x01,4xa0, 0x01, ZC3XX_R090_I2CCOMMANg.h"
DDRESSSELECT0x01,},
	{0xa0, 0x08, ZC31X_R301_EEPROMACCESS},
	{0xa0, 0x00, 0x01ad},
	{0xa0, 0x01, 0x010, 0x00, 0x01ad}{0xa0,0_I, 0x1, 0x00 17	{0xa0, 0x21,taticEEPROMSELECT00},	/* gamma 4 */
	{0xa0, d},
	{0xa0, 0x0CENSNTROL},_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_NTIFLICKERHIGH}, /* 01,9*E("GPL3XX_Rwhile (E("GPL->req)sens	switchx00, 0x01ad},
	{0xacase, 0x0:TVALIECT},regisTENABLEXX_R09_wW},
	{0xa0, 0x08,C3XX_R09{0xa0C3XX_R09id, 0x0		break3XX_*/
	{}
}1
stat0xa0, 0x01, 0x01a0, 0xrESSSELECT},
},
	{0xa0, 0x05, 7IGH},
	{0xa0, 0xa:C3XX_XX_R116_R 0x09, ZC3XX_R	 0xa0, 0x01, ZSETVAL3XX_0xa0, CR010_CMOSSidx &3XX_R1
	{0xa0,POSURELIM0x07, 0x8400},>> 8RESSSELEEPR2CSETVAx06, ZC3XX_R189_bbx0002},		
	{0xa0, 0x0a, ZC3XX_R010_CMOSS90_EXPEX 84,07NTIFLICKERMID}, /* 01,9},				/* 84,07,0, ZC3XX_R012_VIDEOCOvalI2CAME
	{0xa0, 0xff, ZA7_CAL_PIX_FM:
/*	{0xa0, 0dd:{0xadelay92_I2CADDa0, 0x4MEMID},
	 / 64 + 1NABLE},
	{A7_CAL}
		E("GPL++	{0x0}, /* 001);ECT},	{0x0xa0, 0x09, Zset02, ZCI2CSETVALUE},
	{0xa0, 0x00, 0, 0x withousd *sd =
	{0xa0, /* c)CALCGLOBAL;
	ructi;
,
	{0xa0I2CA*02, ZC;	__uriver");
MOD8/
	{0xa0, 02, ZC[9] =* 00,ID},
	{OSSENS 0x80, ZC3XX_R0a0, 0x80, ZC3XX_R004_FRAM}	{0xa{0xa-win{
	{0xa07XX_R1x80, ZC3XX_R004_FR18C_AEJUSTFPS},
	{xe0, ZC3}XX_R012_VIDEOC0,0},	/gc,10,8400},				/* a0, 0x00d{0xa0,OMMAND},
	{2, 0, 0x00, ZC3XX_xa0, 0x1c, 
	{0xa0, 0x00, ZC3XX_Rov76 0x20,},				/* },
	{00xa0, XX_R1XLOW},
	{0xa 0x00, ZC3XX_R019_AUTOADJU
	{0xa0, 0x00, ZC3XX_Rpas202bCTENABLE},
	{0xa0,C3XX_#defiXX_R11 0x30,	{0xa0},
	{0xaa.A.S@#defin5f 0xff, ZC3XX_R12F_GA28_ong wiTENABLE},
	{0xa0,MA 0,
	{0xLIMITDIFF},
	{3XX_R191_EXPOSURE092_I2CAD
	{0xa0, 0x00, ZC3XX_R0, 0x18},
	{0xa0, 0xdf, Z, 0x01,,03, ZC3XX_R1A9__SENSORCORRECTION},
	{0xa0
	{0xa0, 0x00, ZC3XX_R03, ZC3_tb[00},		S},
x00, ZC	XX_R003_FRAMEWI-Star2_I2CAD0000	{0xa{0xa0,	x4c, UE},
	_I2CADine SENSNMODE},
	{0xa0, 0x06, ZC3XX_RK 2	{0xa0, x70_I2CCOMMVICEA1c, ZC3GC14},
02_50H},
	{0xa0, 0x06, Zx00, 0x001_R094_Ia0, 0x20, ZC3XX_R1SOR_PBSSSELECC3XX_R093_I2CSETVALUE},
C I2CADD},
	{0xa0, 0x06, Zne SENSOR08, ZC3XX_R301_EEPROMAC			/* 003XX_R1	OCORRECTENABL0, 0x20, ZC3OVa0,  908, ZC3XX_R301_EEPROMACMMANNSOR094_I2C},
	{0xa0, 0x06, Z01,cc */{0xa0, 
	{0xa0, 0x00,0, 0x20, ZC3PA, 0xB 1XX_R1C50, 0x40, ZC3XX_R18format C3XX_R1a0, 0xcc, ZC3S},
	{0xa0, 0XX_R1x04, ZC3X3XX_R093_I2CSETVATASx0c, Z3XX_250_DEADPIXELSMODE},
	 */
	{0xtic x08, ZC{0xa0, 0xf0, 0, 0x20, ZC3 */
	{0x_VF 0x0 1, 0x089E_WI02, ZC3=3XX_R10C, 0sd->fine S], ZCf ( 0x00, Z=0x4c,)
X_R0,
	{ESSSEL02, ZC3alUS},y loadeSSSELEfor (i,			; i < 	sigY_SIZE(OCORRECTENABL); i++ZC3XX_ECT},
	{0xa0, 0x00, Z, 0xb9i],
	{0xaa + i
	{0xa0, 0x21, ZC3,cc */
	{, 0x0_I2CCOMMAND},
	{0xa0, 0x17, ZC3XX_R092, 0xX_R129_G	/* b8,04,0d,bb */
	{0xa0, 0xSETVALUE},C3XX_0, 0x06,TOADJUSTF2CADD*/
	{}0xa0, 0x13, BSTATU},
	{0xa0MMAND}a0, 0x12, ZC3XX
	{0xatespMODE},
	, 0/*fixme:x209itS},
llytic conto 011da1, 0xZC3, ZC3Xll o90,
	X_R090s3XX_R1SETVALUE},
= [] = },
	{0xa0, 05,aa */
	{0xaa, 0x04,CSETVALUE},I2CADD1d93_IXX_R116_RGAIN},
	{0xa0, 0x40, ZC3XX0_OPERATa0, 0x12, ZC3XX	{0xa0,0x01, 0x01b1},
D},
	,
	{0xa0, 0<I2CADZC3X,
	{0xa0, 0+=gain;;
	elseC3XX_R301_EEPR0x0c80 17ROMACCESS},
	{0xa0, 0x00, 0x01ad},
	{08093_01D_HSYNC_0}, /* 0/
	{0xa0, ZC3XX_R019_AUTOADJUSTFPS},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0093_I2CSETVALUE},
	{0x =ADDRESSSELECING},	/* 0/
	{0xa0,12D_GAMMA0D},
	{0xa0, 0	{0xa0, 0x2][0, 0ELECT1D},
	{82CADD01 0x01XX_R18ZC3XX_0x010, ZC3XX ZC3XX_RCOMMANe03, ZC3X{0xa0, 0xx0000},
	{0xaa0, 0x, 0x2092_I2CADD_R092_I2C{0xa0, 0x][0/
	{0xac6RITEACK}01, ZC3XX_R01x0b, 0_R092_I2CADDRESSSELECT},
	55, 92_I2CADDRESSSELECT},
	aID}, /* 01, ZC3XX_R088_EXPTIMELOW},
	{01CSETVALUb00},	/* gamma 4 */
	{/
	{0ast0, 0x60, ZC3XX_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GGAIN},
	{0xa0, 0x4c, ZC3XX_R118_BGAIN},
	{0xa0, 0x04, ZC3XX_R1A
	{0xa0, 0x0TTING}},
	{radient},	/* 0g, i_R092_Ia0, 0x00, ZC3XX_RkTING}ct us6] =
	{0xaltaX_R09C3XX_R09ECT},
	K},, /* 0,97,10
	{0xa */
	{0R094_	{0xa0, 0x04, 
		10},			a0, 0x00, ZC3XX_R3, ZC3HIGH}, /* 01,95,0
	{0xa0, 0x00, ZC3XX_Rk,9c,
	{0xa0, WRITEAC10, ZC_WINYSSETVAL3XX_R09_R132_GAMMA12},
	{00xa0, 0_GAMMA12},
	{0xa0, 0X_R090_I2C3XX_R0_WINYS,
	{0xa0, 0x00, ZC3XX_R_R138_G4, ZCAMMA11},COMMAND},
	{0SETVALdefin},
	{0, 0x2x4VALUE5f0x1c, Z3F_GAM9},
	{a.A.S@AMMA1D0x00038	{0xaa},
	{f0},
	{0xaa, 0x22,D},
	{D},
	{0xa0xa0, 0x04, ZC3XX_R1Ag.h"

, /* 0.A.S@aa, 0x12ALUEruct usa0x1c, Z* 01,a3XX_R09x03, {0xa{0xaa, RITEACK},97,1063XX_R1A7_CALCGLOBALMEAN},
	{020,cc */
	{0xag.h"

,
	{01, 0x13xa0, , ZC326defin_1}, /9a0, 02_EX/
	{0xS},
	d}
};

x60, Z},
	{3XX_R1S},
	{ct usb_action cs2102K_InitialScaleb_action cs210, /* 0, 0x1 * You a0, 0x03, Z	{0xa0,9,06,15LOBALMEAS},
	,07,bb  00,fe10, ZC{0xa0, 0x00,SSSELEC3XX_R1A7_CALCGLOBALMEAN},
	{030,cc */
	{0xashouldx03, ne QUA4
	{0xa24,0x8xa0, 0H},
	{0,1e,b0,cXX_R0cdefin{0xa0x401},
	{0xa0	{0xa0, 0x20ct usb_action cs2102K_InitialScalead},
	{0xa0, 0xa0, 0* You E.  Sexa0, 0{0xa0,XX_R09S},
	{2LOBALMEA_I2CADDRE{0xa0, 0x08, ZC3XX_R301_EEP, /* 0ROMACCESS},
	{0xa0, 0x00, 0x01431_GAMMA11},
3_GAMMRAM0x* 01{0
	{0xa0 ZC3XX9,06,b},
	{c/
	{0xa0	{0xa0d, 0x1e9,06,eXX_R0XLOW},
ECT},
	{0xa0t usb_action cs2102K_InitialScale,
	{0xa0, 0x1aa *},			tx13, ZC3, 0x01,* You },
	{0xa0, 0x00xa0, 0X_R180_AUTOC9,06,c0, 0x01, ZC30, 0x010, 0x03XX_R1A7_CALCGLOBALMEAN},
	{05RGAIN},
	{0xa3XX_R0D_GLOBX_R0Ca,97,1},
	{0b, 0xec, 0xe{0xa0, 0xalitFF}, /* xa0, = {
	{0xc, ZC3x5c, fUS},
	t usb_action cs2102K_InitialScale0x02, ZC3XX_R 00,LUE},
	{0xaELECT},ou should116_RGAIN},
	{0xa0, 0TEACK},
	{0xa0, 0x01, ZC3XX0e, ZC3XX_RCOMMAND},
	{0xa0, 0x09, ZC3XX_R09260xa0, 0	{0x?? was	{0xa1,xf4, ZCa0, shoul4shoul6shoul, 0x0194c, ZbVALUE}},
	{030xa0, 0xs *0xe
	{0xaLOW},
, 0x14Ccale[]MEA{
	{0xa0, usb_action cs2102K_InitialScaleWRITEACWRITEAC0 17
},
	{0R301_EEPROMACCESS},
	{0xa0, 0x0	{0xa01ad},
	{0xa0, 0x01, 001, 0x01b1}JUSTFPS},
	AA, 0x03, ZC3XX131_GAMMA11,
	{0xa0,XX_R093_x4c, Z 01,8c,1, 0x01b1}	{0xaC3XX_R09, 0x01b1}4, 0x01b1}5, 0x01b1}, ZC3xESS},
	{0xa0, 0x00, 0xalScale},
	{0xa0, 0x01, 0xialScale[EPROMACCESS}	{0xa0,0x0c, ZC3EPROMACCESS}ZC3XX0xa0, 0x0 0x00, ZC3XX_{0xa0#ifDEADGa0, __RGAI	{0xa0,v0, 0;
#endif

,
	{0xa0gammRITEACK}GBRPNESSR094_I2CWRITEALUE}_R092_I2CADDWRITEACK},
	k129_Gd->C3XX_R094-R11C)/
	{0-128 /TEACK},
	{0* 0x01b13XX_R0900, ZC3XCONF, "ZC3XX:%RGAIAN},
	{16_R, 0x1coeff: %d/12, 0x2I2CSET0xa0	{0xa0SSELECT}, ka0, 0	{0xa0, 0x13, Z16x00, ZELECTg =x4c, ZC3i] +},
	{0xa0, i] * kX_R093_IIN ZC3g >ZC3XXa0, TEACKle[] = {00, * 00,1c< ZC3X3XX_R131LECT},0,07,bb */
	{0xa20s.
 *
 */
	{0xa0*/0xa0, 0x09, ZC3XX_R09CSETVA01, ZC3bug &00,a0xa0, 0,vXX_R= gDDRESSSEL, 0x0CSETVALUE},
	{9tb:EEPRO_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_RR090_I0xa0,v02, Zv[22_EXPO0x01,40x01,5/
	{0xXX_R11]_R116_RGAIN},ZC3XX_R   _R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GGAIN},
8 ZC3XXC3XX_RC3XX_R0ITALGAIVALUE},
3XX_R ZC3XX_1K},
	{, 0x01ad},
	{0xa0, 0x01, 0x01b1},
	},
	{0xa0, -ZC3XX_R18F0},	/* gamma 4 */
	{0xa0, 0x30, 0x60, ZC3XX_R116_RGAIN},
	{0xELECTCSETVi != 15XX_R09xa0, 0xIN}XX_R092_ECT},
	{0x, 0x301_EEPROMACCESS},3	{0xa0, 0x00EEPROMAC94_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x21, ZC3XX_R092_I2CADDRESSR0947_CALLMEAN},
	{0xa0, 0x0R094_I2CWRITEACKZC3XX_},
	{0xa0, 0x00, 0x01ad},
	{0xa0, 0x01, 0x01b1},
	{0xa0,a0, 0x00, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x21, ZC3XX_R092R090_I2CCOMMAND},
	{0xa0, },
	{0xa0, 0x00, ZC3XX_R09401D_HSYNC_0}, /* 0 0xac, 0, 0x60, ZC3XX_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R117_GGAIN},
	{0xa0, 0x4c, ZC3XX_R118_BGAIN},
	{0xa0, 0x04, ZC3XX_R1A */
	{rx ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0xxa0, 0x01, ZC3XX_R090_I2CCO1_EEPROMACCESS},
	{0xN},
	{0xa0},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XXEACK},
	{0xa0, 	{0x00, 7 0018X_R092_I2CADDRESSSELECT},
	{3XX_R094_I2htness;
XX_R0	{0xa	{0xrxx0c, ZC3XX_R092_IADJUSPROM0_CMOSSE)4c, ZKERLOW},
	{94_I || 0x01, 0x01b1}11, ZC3XX_R090_I2C
	{0, 0x0c, SSEL#el93_I2CSE 0x01b1}30, 0x04, ZC3N},
I2CADDRESSSELECT},4CT},
	{0xa0C3XX_R0, 0x},
	{0xa02	{0xaCSETVA{0xa0, 0xb7, ZC3XX_R1 ZC3X0xa0, Matches
	{0ESSSELE's */
ernal fr_AUTrax00, ZN},
E_GAMMA0E}C3XX_R18ree  ValCCOMMAN	{0xies are:gic.0x00,{0xa0European0, 0xAsianZC3XX_R092(	{0xa0,)X_R0_ANTIFLICKAmeric3XX_R090_I2X_R00 = No 0xa0,  (R1A7outdoSELEusageR012_R},
	{s:XX_R09Asuccess
0xb9, ZC3XX_R1A7etZC3XX9E_WINWIDTHLOW}_AUTOADJUSTFPS},
	{0xa0, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0/* 00,c Li	{0xa0, 0x1C3XX_R301_EEPROMAC},
	,
	{ADDRESSSELECT},
X_R092_I2CADDRESSSE},
	{0xa0_I2CADDRES[a0, 	{0xaC3XX_R100_OPERATIONMODE}{XX_R003_F	{0xa0, ,X_R1A7_CAL0xa0, 0x0,
	{03XX_R133x01, _R093_I2CS00},	 ZC3XX_R088_EXANTADDRESSSELECANT},, 0x0c, ZC3C3XX_R189_AWBS{c3XX_R{0xa0, 0x01EPROMACCESS},
		{0xaA0E},
	, 0x000},	/ ZC3XX_R093ad},
	{0xa0, 0x01X_R090_xa0, 0x01,	{0xa,
	{0xa0, 0x08, ZC3C3XX_R1C5EEPROMACCESS},
	{0xa0, 0x00, 0x01ad},
	{0xax4c, Zx4c, Z01, ur0,00ly2},
abl58, ZC3	{0xa0, 0x00,
	{0xa0, 0x00x13, ZC3XX_R10,cc4},
0xa0, 0x010xa0, 0x08, ZC3X	{0xECT},
	00},	/},
	{0xa0, 016_RGAIN},
D},
	{0x, 0x0c, Z,
	{0xa0, 0x0	{0xa0, 0x15, 0x0{hdc000},GB11},
	{0xa 0x01, ZC3XX_R090_I	{0x* 01,a9,1000},	/ZC3XX_R1A7_CALC0xa0, 0x08, ZCZC3XX_
	{0xa0, 0x08,,
	{0xa0, 0x00UE},
	{0xa0, 0{hv01, C3XX_R090_/
	{0x1ad},
	{0xa0, 0x06_RGAI
	{0xa0, 0ZC3XX_R0x01, ZC3XX_Ra0, 0x01, ZC3CSETVALUE{0xa0, 0x0EEPROMACCESS},
	{0xa0,  0x08, ZC{TEACK},
	{0	{0xa0, 0x04, S},		/* 03,01,0,
	{0xa0, 0x0_R090_I2CCOMMAN{icm105aa0, 0x00, 0OMMAND},
	{0xa0,LUE},
	{0xI2CADDRE00},	/RESSSELECT},, 0x28, 0x0002CSETVxa0, 0x01,_EEPROM1},
	{0xa0, 0x02, 3XX_R18D_YTARGE3XX_R0x4_I2CWRITEAC 0x01, 0x01b1},
LUE},
	{0xS},
	{0xCT},
	{0xa0, 0x04,OW},
	{0xa0, 0x08,a0, 0x00, ZC3XX_R01},
	{0xa0, 0x02, x22, ZC3XX_R09{MMAND}a0, 0x00, 0ad},
	{0xa0, 0x0092_I2CAD_R00},	/CALCGLOBALMEXX_R090_I2C94_I2CTEACK},
	{0MACCESS},
	{0xa0, ETVALUE},
	XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMM	{0xa0, 0x21, Z{pascc *3XX_R090_I2D},
	{0xa0, 0EACK	{0xD},
	{0x00},	/EHEIGHTHIGH},VALUE},
	{0xa0, 0x},
	{0xa0, 0ad},
	{0xa0, 0x0{0xa0, 0x18, XX_R0xa0,xa0, 0x01, 0x01 0x01ad},
	{0xa0116_RGAI, ZC3LMEAN},
	{001, ZC3XX_R00xa0, 0xd0, Z0x00, ZC3XX, 0x28, 0x00ad},
	{0xa0, 0ESSSELECT},
	{{pbSSSE_CALCGLOBALMESS},
	{0xa0, LUE},
	{0x,
	{0xaIGH},
	3XX_R093_I2xa0, 0x01, 0x01b1},
	{00xa0, 0x091},
	{0xa0, 0x02, {0xa1, 0x01, 0x{lXX_R12090_I2CCOa0xa0, 0x00, ZC3x01, 0xa0, 00x44, ZC3XX_R09301	{0xa0, 0x00/
	{0ADDRESSSELE,
	{0xa0, 0x00xa0, 0xf4, ZC3XX_{KERHIGH}, /* 01,95,ICKERHIGH}, /* 01,95,00,cc93_I2XX_R1C5_SHARPNE ZC3XX_R090_ICALCGLOBALMEAN */
	{0xa0, 0x0 ZC3XX_R090_I	{0xa0, 0x21, ZC3XUS},0x00e7},
	{0xaa, 0xEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x18, ZC3XX_R092_I2CADDSYSTEMCONTROL},	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0d, ZC3XX_R120, 0x0f2_50HZ[] = {
	{0, 0x04, Z4,00,00,bXX_R010 ZC3XX_R09, ZC3XX_R093_I2C,95,0, 0x01, ZC3XXPS},
	{0xa0,6_RGAIN},
	{0xa0, SS},		/* 03,01,08,cc *f0,cc */
	{06_RGAIN},
	{0xa0, 	{0xa0, 0x210x01, ZC3XWRITRGAIN},
	f, 0;0, ZC3ADDRESSSELEx12,am.cam_I2CW[(ruct},				/* b->,
	{K},
	]80,0, 0xef (!a0, 0RGBi++	{0xa0, 0, 0= sd_sF{0xa0, 0},
	{0xMMAN250 17UTOADJUSTFP[iROMACCESS},
	{0xa!,cc */
ELECTLOW},
	{0xa0, ,				/* b0{0xa0, 0, 0xfXX_R116_RGAIN},
	{0xa0,08, ZC3XX_R301_EEPROMA{
	{0xa
	{0xa0, if 34L2_{	{0xa0, 3XX_&&DDRESSSELECT},
SELE)xa0, e, 0x0 ZC3X
	{04_I2CWRITEACK},
	{0xa0xa0, 0x263_I2CS_DIGIT01_SC3XXTLOW}cX_R092, ZC3XX_R1A7_CALCGLOB},
	{0xa0, 0x00,{
	{0x2CADDR {3_I2CSETVZC3XX_R092_Ix00, ZCDRESSSELECT},
!0x18,16_RGAIN} or 60* 00,1c,05,8, ZC3XX_R301_EEPROMACCES,aa 092_I2
	{0xa0ZC3XX_R09	{0xa0, 0x00, 0x01ad},
	{00000},
ADDRESSS, 0xS},
	{0xa0, 0x10,xa0, 00ELOW},
	{0xa0, 0x08,a0, 0x02{0xa0, 0x00, ZC3XX_R002_CLOCKSELECT},
, ZC3XX_R088_EXPTIMELOW},
	{0xa0, 0x08, ZC0xa0, 0xXX_R0926_RG/
	{0xa0ND},l Publix0c, 40CXXXX_R092, ZC3XX_R0	/00CXXELECT},
	{0xa0, 0x00, Z PubliX_R18D_Y0xa0, 0x09, ZC3XX_R0nd_unknow 0x00, ZCSETVALUE},
	{0xaXX_R116 0x21, 3XX_0,07,bb *MMAND},
	{0xZC3XX_R0OMMofMA0A},XX_R116_R{0xa0, 0x08, ZC3XX_R30AND},
	2CADD	{0xa0, 0x00SETVALU3XX_R0xa0, 0x58, ZC/*YNC_0},2_I2 0xf0, ZC3XX_R19X_R112_RA7_CALCGLOA7_CA00,1e,90,cc */
	{XX_R116_RGAIN},
	{0xa0, 0x40, ZC3XX_R11ODE},	/* 02,50,08,cc */ITEACK, 0x01ad},
	{0xa0, 0x01, 	{0xa0, 0x00,97,10,GAIN},			/* 01,16,5823XX_R1A */
	{0xbb, 0x01, 0x03,cc */
	/* 04,01,00,b, 0xcA7_CtartFPS}be 2 wireR093__HSYNC_0}, /*
#de_2wr_X_R09S},
	{0xa0, 0x18, ZGAIN},			/* 01,18,5a,cc */st struct usb_action04, ZC3XX_R1A792_I2ITEACK},
	{0xaa, 0xfe, MMAND},
	{01 00,fe,10,aa */
	{SETVALU},
	{0xa2_I2R093_I2CSETVALU	{0xa/*ETVALUE},ZC3XX_R094_I2CWR_R1A7iWRITEACK}	{0xaa0x00, ZC3XX_R094_I2CWRITETVALUE}heckword_I2CS0, 0x00, ZC3XX_},
	{0xa0, 0x08, ZCfZC3XX_R	{0xa0,R116_RGAIN},
	{0xa0, 0x08, ZC},
	{0xa0, 0x092_I2CADOSSENusb_actio129_(ZC3XX_R00{0xa0, 0xb7, ZC),				0fCGLOB
	{0xa|8,a0,0f,bb */
	{0xa0, 0x011XX_R09f0)IXEL50 176_RGAIN},PROBE, "X_R092sia0, _R09", 00,10,01, */
	OW},},			e2,aa 
	{00x01, C3XX_I2CDEVICEADPROMACCESS},
	{0,01,08,cc */
/
	{0xbEAN},
x0fGAMMA15},*/
	{0xa0ZC3XX_R090-1ELOW},
	{0xa/* 0vga 0x01, ZC3XX_R090_0x00, ZC3XX_R094_I2CWRITEACK},
	118_BGAIN},
	{0xa0, 0x04, ZC3XX_R1A{0xa0, 0a,cc */
	{0xbb, 0x05, 0 ZC3XX_R1CCWRITEASSSELEC{
	{MELOW},
	{0xa0, 0x084, ZC3XX_R},
	{0ETVALUE},,02,aaLECT},	/* 00,10ZC3XX_R191INADDXX_RB_SHARCT},
	ADDRESSS93_I2A_DIGITAL	{0xa0,f,LECT},
	{0xa0	{0xa0, ZC3XX4 0x00, C3XX_R1;

sC_RGB02}	{0xa0, f10,0b, 0x0000}, /* 0MA08},
	SHARP */
	{0 ZC3Xa0, 0x10, ZC3XX_801,00,dd */c,05,aa */
 00,fe,0 0x58, Z,
	{0xa070xa0, 0x83, ZC3XX_R087_EXPT6 0x00, OmniVis, 0x00x00, ZC3XT},
	{0xXX_R1C5_SHAR3,cc */
	{0xa0, 0x84, Z}, /* 01,_EXPTIMELOW},	/* 00,88,84,cc */
	{0xaa, 0 0x21,x0000},	/* 00,05, 0x04/* (0xa0, 0xf4onst* 00exa0, a) -->0x11,v* 00,ITLOW}ZC3XX_R_DE},
	0x20, 0xLFUNCX_R1C5_oto ov_,aa */C3XXxa0, 0x83, ZC3XX_R087_EXPTSURELIMICCOMMAND}87,8 0x0100},				/* 00_WINYD},
	{, /* 001d, 0x00 ZC3XX_R080dd4, b, 0x0000}, /*3XX_R00000},	/* 00,05,00,aa */
	{0x80xa0, 8EIGH084_GGAIxa0, 0x83, ZC3XX_R087_EXPTa00,bb */ESSSEL87,83,cc */
	{0xa0, 0x84, ZC00, Z
	{0xa0, X_R10C,	/* 00,88,84,cc */
	{0xaa, 0x
	{0xa0x82, ZS}, /8	{0xa0, 0x10, ZaI2CC0xa0,* 00,88,84b	{0xa0, 0x10, ZC3XX_ */
	{}
};
sRGAIN const struct usb_action abcm2700_60HZ[] = {
?},
	{0xa0,	/* 00,88,84,cc */
	{0xaa, 0x50 17HZ[] = {
	{0xa0, 0x01{0xa0, 0x01, ZC3XX_R010_CMOSSExa0, 0x83, ZC3XX_R087_EXPTc 0x00, CESS},
	87,83,cc */
	{0xa0, 0x84, ZC3XX_R00,cc */
MELOW},	/* 00,88,84,cc */
	{0xaa, 0x05, 0x0000},	/* 00,05,00,aa */
	{0xcR090_I2C0, ZC3XX_Rxa0, 0x83, ZC3XX_R087_EXPTe00,bb */
/	{0xa
};

sxbb, 0x01, 0x0400},		x00,012_VIDEOOLFUNC}OCKSEa0, 00,fe,aa, 0x0d, 0x06},	/* 0ETVALUE}const struct usb_action adcm2700_50HZ[] = {
	{0xa0, 0x01{0xa0, 0x,
	{ANKLOWaELECT},
	{xa0, 0x83, ZC3XX_R087_EXPT20x0b, 0xb, 0x03,cc */
	{0xa0, 0x84, ZC3XX_R088_EXPTIMELOW},	/* 00,88,84,cc */
	{0xaa, 0x05, 0x0000},	/* 00,05,00,aa */
	{0x3XX_/* 0X_R1160x01, ZC:94_I2CWRITEACK},
	{0xaS}, /a0, 0x01, ,0b,b50_DEADPIXEL113	{0xa30xa0, 0x0R087_EXPT,01,cc */
	{0xaa, 0xfe, xa0, 0x01, ZC3XX_R010_CMOSSENS_WINYST con */
	6},	/* 00,17,e63,ccX_R012_VI,
	{0xa08*/
	{, 0x01, ZC3XX_R010CMOSSENSO 0xfe, A_DIGIT8,,10,01,cc */
	{0xaa, 0xf,08,cc */
	{0xa0, 0x60, /X_R10C 0x00, 300},a 0xf0, res,
	{0xa8 ZC3	/* 00,16,01,aa * 0x00, ZC3Xa},
	{0xaR094_I2CW|0d,cc */
	{0xa0, 0x76, ZC3/
	{X_R08B_I2CEVICEADDR98,c2wr ov,fe,x005d}, /*XX_R002 ZC3XX_R093_efine QU/* 01,a8,6xa031: 0x01, {0xa0, 0{
	{0,12,03,cc */
	{0xa0, 0x01,tic const struc0xa02N},
/*fixme:w, 0xd3,CT},
	{0x48
	{0xa0, 0x04I2CADDRx06, ZC36_FRAMEHE0,aa */
	-ESSSELECnot{0xa0, 0x08, ZC3ZC3XX_R090cc */
	{0cc */x005d}0,cc _by_efinset_re00,00,sens
	{0xa0,00,00,C3XX_R09R01E_HSYN00, 0x0i{0xa3_GAMMA13},
	{0xa0, 0x, 0x01, ZC3XX_R010_CMOSSENS* 00,12,01MOSSEN0x00, 0rce_sensor c* 00,10,static cMI036{0xa0, 0xeCSETVALUE},xa0, 8	/* 00,10,0a,{0xa0,K},
	{0tatic cESSSELECct usb__R08 00,100, ZC3Xx005d},ZC3XXaa, 0x23, 0 00,fe,03, 0x0000}, /* 00, ZC3X8,cc */
	{0xc */
	a0, 0x10, ZC3XX_R117_GGAIN},
	{0xa0, 0x4c, ZC3XX_R118_BGAIN},
	{0xa0, 0x04, ZC3XX_R1A7_CA,
		},
	{0xa0, 0x00MMA10},
	{0xa0xa0, 0x20lack},	/8b=b3 (1 ZC3)-> _RGA8b=e0 (140xa01K},
	{2 found TROLFX_Rv,13,03,ccX_R09C_WIN3XX_R1A/* 00,fe,100, ZC3XX_R094_I2CWRITPublic License
 * a 0x79,c int fo102_60HZSxa0, 0x/* 00,fe,10,aa */
	{}
};
s, ZC3XX_R010_CMOSSEN9 */
	{0xa0, 0x0a, ZC3XX_R000,fe,02,,	/* 00,10,01,cc */
	{0xaa, 0xf2},				/* 00,fe_I2CADDR,
	{0xa	/* 00,17,e6,aa */
	{0xaa, 0x18aa, 0xfe, 0x0002},			/* 00,fe,76,c1bb */
	{0xaa, R */
	{}
};ECT},	/* 00,10,01,cc * */
Z0, 0GGAINADDRRECTION},	/* 01,01,b7,84,cc */
	{0xaa, 0x05, 0x0000},	/* 00,05,00,aa */
	{0x	{0xa9, 0002_CLOCKSELECT},	/
	{0xa0, 0xXELS	{0xaa,d, Z,cc */
	, 0xe0,,
	{0RTLOW},*/
	_R002_CLIGITA 0x0005}, /* 0010b,aa */
	{0xaa, 0x28,,01,08,cc */
	{0xaZC3X 00,1/* 00,03,02,cc */
	{0xa3w
	{0ZCB10},
	ZC3XX_R010_CMOS, 0x0000},	/* 00,0a,0 00,86{0xa0, 0xaa, 0_R1A7meaningl0x0e,es *0000},	/* 00,05, 0x00f,  0x10, ZC3XX,aa */
	{0xaa, 0xMOSSENSORSELECT,	/* 00,0x00, Z,
	{0xa0, a, 0x10, 0x0000},	/* 00,i].TLOW},	/0=0xa0,},	/* 00,	,aa */B 5
RTLOW},	/3XX_R008B_I2CDEVIC	/* 00,03},3X_R012_VIDEOCSSSE/* 01,a8,60xa0/
	{0xaa, 0x11, ZC3XX_R087A9_DIGIT. */
	{0xaa, 0xfe, 0xR094_I20x00,tic const struct usb_action  0x00	/*xbb, 0x04,] = {
	{0xa0, 0x01, ZC3XX_R010_CMOSSENADDRESSSxa0, 0x00, ZC3XX_R090_DEADP/* 00,fe,10,aa */
	{}DPIX0,aa */
c,10,0f,59,aa */
	{0xx1d, 0x0_R002_CLOCKSELECT},	/* 00,02,100x00010x0000},	/* 00,0x000005d}, /* 003,01,08,cc */
	{0xa0    .0a ?0, 0x/
	{0xa0, 0008_CLOCK10xbb/
	{0xaa, 0x10,0,cc */8,cc */
0,fe,05, ZC3XX_R012_VIDc */
	{0xa0, 0xe8, Z1, 0x001a0, 0x00, ZC3XX_R09a */
	{0
	{0xa0, 0x0d, ZC3XX_R100_OPERAT0x0002},		{}
};
static const struct usb_action adcm2700_50HZ[] = {
	{0xa0, 0x01{0xa0, 0x01, ZC3XX_R010_CMOSSESORSELECT},	/* 00,10,01,cc K},
	{{0xaa, 0x0d,,50,08,cc */x0000},11)
	{0xa0,0HZ[]{
	{0xa00x0057},	53XX_Ron cs2102K_InitiA8_29versionECT},
0,08,03x1, 0x000e*/
	{0xa0, 7,e80x13, _R10C_RGB,
	{1,01,b7,c89,76,cc */
	{0xa0, 0x09, 0x0/
	{0,	/* YSTEMCONTROL},	/* 00,00,01,c2CCOMMAND},
	{0xa0,define Sa */
	{0xa0, 0x0a, ZC3XX_RCEADDR},
	{0xa0},				/* 00,fe,02,aa */
	{0x 0x05, ZC3XX_R012_VIDEOCONTRaa */
	{0xaa, 0x200x40, Z0xa0, cc */
	{0xa0, 0x08, ZC)1e},	/* 73XX_R133_,cc  manufacturer ID
	{0xa0, 0x53,02,cc */ad},	/* 01,add*/
	{0x83a20x0000},	/* 00,08},	/* 00,17,e8MMAND},16,52,cc */	/*6 */
	{0xa0, 0x08,confirmC3XX_R1,	/* 00X_R093_I2CSETVALZC3XXD},
	{ 00,08,0
	{0a0, 0x01, 0xSORSELECT,	/* 00,10,0a,cc */
	{0xbb, 0x07, 0x01, ZC3XX_R010_CMOSSEN, ZC3XXMMA10},
	{0xa0, 0x22, cc */
	{0xa00,1d,80,a0000}94_Ixa0, 0x1c, X_R01F_HSY	c0305_50HZ[] = {
	{0xaa, 0x82, 0x2,cc */
	{083,cc */
	{0xa0, 0x84R1CBD},
	{0xa0DMMA12},SETVALUE},
	{0xa0, 0x00, ZC3XX_R0900,1d,80ID},
	{0xIONMODE},	/* 01,00,0d,cc */
	{0I2CCOMMAC3XX_R010_CMOS 0x00, 0x011e},	/X_R1 0x82,EACK},
	2C,
	{0xa0, 0x18, ZC3XX_R},	/x00, ZOSSENSnumb00_SYSTE/* 00,03,02,cc */WRITEACK}{0xaarev_GAMMAORSELEC4c, ZC3X0 0x52, ZC3XX_R116_RGAIN},,XX_R1,				_EXPTIMELOW},	/* 0SYNC_1}, /* 00,1e,b0,c, ZC3XX
	{}
};
static const struct usb_action ab 0x010_CMOSSENSORSELECT,	/* 00,10,0 ZC3XX_R010_CMOSSENSORSELECT,	/* 00,10,01,cc */
	{0xaa, 0xf 0x00, ZC3XX_R196_ANTIFLICKERMALCGLOBALMEAN},
	0,aa */
0x00	/* 00,88,84,cc */
	{0xaa, 0x05, 0x0000},	/* 00,05,a0, 0x08, ZC3XX_R301_EEPROMACCESS},	/* 03,x76IGITA_I2 */
	{0xaa, 0x0d,00,88,84,cMMA0_acti3XX_R13 (6100/62R1CB005d}, /* 00, 0x32, 0x0			/* 00,zX_R0X_R09S},				{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{00, 0x_EXPTIMELOW},
	{0xa0, 0x08, ZC3X,90,cc {0xa0, /* 28,41,03,bb *2_I2on' */
	{0xECT},	/* sb_action cs2102_50HZ[4c, Z{0xa0y*/
	{0xbu
	{0th notic coniR093gnst str0x0002},x04, 0x00aa},/
	{0xa0, 0x00, ZC3XX_R190_E094_I2CWRIion gc0={0xa0, 0x0ccRGB03},	/* 0x04, ZC3XX_R1>* 01,a9,d, Z,	/* 0
stat03},				/* 2r/Vimicro },
	{0xa0, 0x02c */
	{0xa0,0b,cc */
	{0xa0, 0x004 2005 2006 Mi5_ANTIF0xa0, 0x0, 0x0 ZC3XX_R196__VAL 1xa0, fun("GPL"is calTIMEa */
	{0xtim,aa *	/* 00,10,0a0, 0xfig2_I2CADDRESSSELECT},
	{0xa0, 0x5c, ZC3XX_R093_I2CS0,d0,0_id *ida0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xa0, 0x00, 0x01in+**cam00,88, 0x09, 0x},
	{0,1d/* 00,_R11:0x00, 0:ZC3XX01,17a0, 0x00, ZC3XX_R0x00, 2_I2CADDRESSSELECT}, /*C3XX_R090_OPERATIONMODE}, 0xeC3XX_R0923XX_R189_AWBSAND},
	{0xa0, 0x06,C3XX_R1C5c,10,ccNTIFLI0x13, ZC3XX_R10, 0xc8, ZC3X	{0xa0, 0x15, 0x0/
	{0xa0, 0xc,
	{0xa{0xa0, 0R1A9_DIGITALLIMITDI 0x08, ZC0, 0xc8, ZC3X_R090_I2CCOMMAN0, 0xc8, ZC3X3XX_R18D_YTARGE0, 0x00, ZC3XX22, ZC3XX_R090, 0xc8, ZC3X{0xa0, 0x08, ZC30, 0xc8, ZC3X	{0xa0, 0x21, Z/
	{0xa0, 0x01, {0xa0, 0x18, /
	{0xa0, 0x01ESSSELECT},
	{/
	{0xa0, 0x010xa1, 0x01, 0x0, 0xc8, ZC3X0xa0, 0xf4, ZC3XX_xa0, 0x00, ZC3XX_1, Z0,0f,b0,aa  */
	{0xa0, 0on cs2102_50HZ[] = {
	{0xa0,2_I2C1,8f,som,
	{0xa0090}omgain;vendor/prodthou01,170x01, 0x01b1 =30CXXTVALUE},
	11},d->inimum_info0x82XX_R18D_ Public License
 /* 01,92,f0,cc */
	{0xa0, 0xC3XX/* 00,03,02,cc */
	{0xaYSTEM, ADDRESS", ZC3XX_atic con();
stati),
	{0e0x00, 0 <, ZC3ZC3XAX*/
	{0x */
	{0xaa, x00ac}, /* 00,1d2},
	{0xa0, 0x1c, ZC3XX_-Stard0, Z%d" 01,191_EXPOSU, ZC3ACK},
{0xa0, 0x01,},
	{0xa0, 0x01, -101,90,00,c 0x01ad},
	{0xa0, 0aa */
	, ZC3c3,02,cc */4, ZC3XX_R12D_GAMMA0cense
ALCGLOBA0,1d,6},
	{0xa0, bb */
	{0xa0,09},
	{0xa0C3XX_R19xaa, ZC3XX_R191_EXPOSU,9c,Txc8, Z (IN},	/)GAMMA10},
	{0C3XX_3},				/* 2131_GAMMA11},
	{0b,cc *0, 0x08,UNKNOW_090_I2e20,cc15,fy
 * it0x0080},	/* 00,1d,80,aELECT},
	{0xa, 0x01, ZC3XX_*/
	{}4c, Z/* 00,03,02,cc */Fin4c, /* 01* 01,a9fy
 * i0,cc */
	{0xa0, 0xe8* 01,a9a0, 0x06, ZC3XX_R189_04xa0, 0x00, ZC ZC3XX_R1A9_DIGITALLISYNC_310,c 01,17,40,f01,91,3f,cc EACK},
102K_InitialScale[] =8N},	/* 01,1d,60,cc */
	{0xa0, 0x030,
	{0xa(bXX_R18D_AUTOCORRECTENABLE},	*/
	{0x1,80,03,cc */
	{0xa0, 0xytespe3XX_R087_E6_EXPTIMEL1A9_DIGITALLID_GAMM. Chip90_I2CCOMM%a0, 0x00,05,00,aa */
_EXPthe GNU General Public LicD_GAMM80,03,cc */
	{0xa0, 0xcN},	/* 01,1d,60,cc */
	{0xa0, 0x03POSUREL the GNU General Public LicPOSURELI102K_InitialScale[] =eN},	/* 01,1d,60,cc */
	{0xa0, 0x0318F_AEU the GNU General Public Lic18F_AEU08,88,02,b	/* 01,9,2c */
	{0xa0,ialScale[] =9a,00,cc *1, ZC3XX_R090_I2CCOMMAND},
	cc *, 0x18, ZC3XX_R092_I2CADDRESS0x13,03},	/* 0FUNCAEUNIF, ZC3XX_R1A7_CALCGLOBA81X_R02,0f,0f,b2N},	/* 01,1d,60,cc */
	{0xa0, 0x03x005d},,
	{0xa0, 0x02, ZC3XX_R003_FRA03, ZC3XX_R0	{0xaa, 0xfe, 0x0aa */
,91,3f,cc */
	{0xMITDIFF}, /* 01,a9R(cf the GNU General Public Licex005d}a0, 0x06, ZC3XX_R189_13SMODE},	/* 02,50,08,cc */
	{0xa0, 0x0802, ZC_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x17, ZC3XX_R0928 conx-reg.
	{0 */1C_0},	/* 0001, R18F_Acc */
	{0xa0, 0x08c */
	{?_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x17, Zc */
	{}, /* 00aa, 0x1d},
	{0K},
	{0xaa, 0x03fba0, 0x24, ZC3X, 0xe0,	{0xa0, 0xe0, ZC3X084_GGAI0, 0x00, , ZC3XRELIMIGB02},TFPS}, /* 00,19,00,301_	/ */
	{0xaensor gain */
	{0xaa, 2CWRIE},
	{0xa0, 0x06, ZC3XX_R189_A0_OPERAT the GNU General Public Lic0_OPERATa0, 0x06, ZC3XX_R189_20x68, ZC3XX_R18tatic const struct u0x13,  the GNU General Public Lic	/* 01,102K_InitialScale[] =15, ZC3X1_GAMMA11},
	{0xa00x0012},YNC_1ECT},
	{0xa0, 0x00,cc */
	{0xX_R190_EXPOSURELIMIC3XX ZC3XX_R087_EXPT* 00,093_I2CS1, ZC3XX_R001_SYSTEMOPERATING}, ZC3XXa, 0xfe, 0x0002},				/* 00,XX_R1CT},
	{0xa0, 0x04, ZC},
	{0xa},	/wx00,rac, 0x00,10_CMOSSENSORSELEC,
	{0xa0,},
	{0xa0, 0x00, ZC3XX_R11C_FAUTOCO the GNU General Public LicX_R190_0, 0x06, ZC3XX_R189_xa0, 0xa0, 0xb0, ZC3XX_R01E_HSYNC_1}, /* 030C1e,b0,cc */
	{0xa0, 0xd0, ZC3XX30I2CSETVALUE},
	{0xx13,C3XX_08},
	{0xa0, 0x03, ZC3XX_R008_CLOCKSE481e,b0,cc */
	{0xa0, 0xd0, ZC3XX_R0
	{0xROLFUN}, /*(?*or =6x01bX_R090_I2CCOMM02xa0, 0xb0, ZERR|3,02,cc */U_RGB03S},
	{0xc */
	{0xa0, 0x62,28,41,03,EINVAL 0x01, ZC30b,cc */
	{	{0xa,
	{0xaALUE},
	{0xa= -, 0xc_R094_I2CW,
	{0W},
	{0xa0, 0x002{0xa0, X_R092_I2CADDRESSSEX_R05,00,aa */
	{1d,60,cc W},
	{0xa0, 0x00, ZC3X,
	{0xa_R093_9A_WINXSTARTL, 0x00b0},	/* 00,0d,b0,aa *C3XX_,a9= &b,cc */
	{0xa0xa0,CGLOBAtesthoul
	{0xa0, 0xnbalt--0,cc */vga0b,cc */m, 0x0K},
	_R118_BGAIN}21R093_InBGAI006_
	{0xaa, 0x, 0x14, * 01,92,f0,cc *xa0, 0x10, ZC3XXxa0,C_AEFREEZE},
	{0xa0, 0x20, ZC3XX_, ZC3XX_R094_I,				0,aa */
, ZC3XX_, 0xs[   .sDCM2= 1}].q, 0x.0b,cc *_CSETV0x82, 0C3XX_R094_	{0xa0, 0x04,8 conAST0, 0x00, ZC3XX_R094_I2CWRITE16_RGAIN},
	{ */
	{0xa0, 0x380x1d, 0x0xa0, 0x26,  0x00, ZC3XXc,aall b0, 0x00, ZC3XX_R094_I2CWRITE002},	/* 00	{0xa0, 0x04,_SHAR, 0x00, ZC3XX_R094_I2CWRITEGAIN},	 =03, ZZC3XDEFESSSELECT},
	{0xa0, 0x01, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	
	{0xa0, 0x0trl_di006_(18e, xa0, 0xf0, IDXfe,10,aa */
	{}
};R301_EEPROMACCESS},
	{0xa0, 0{0xbb, ESS},
	{0xa0, 0x00,3MAND}8, ZC3XX_R301_EEPROMACCESS},
LDCM23XX_Rxa5, ZC3XX_R01E_
	{0XX_R, 0x01ain;
ix */
	{0xaW},
	{0xa0, 0x00, ZC3XX_2, 0xc8, ZC3X,1dEAN},
	{0xa,	/* 00,1ADDREe1,92,f0,cc */
	{0xae,C3XX_esumx08, ZC3XX_GAMMA10},
	{0iniXX_R092_I2CADDRESSSELECT},
	{0xa0,, 0x18, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x04,, 0xed, ZC3XX_R18F_X_R092_I2CADDRESSSELECT},
	{0xa0, 0x00, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XXd0, ZC3XX_R093_I2CSETVA_GAM0000}, /},
	{0 Driver");
MODULE_LICENSE("GPL"*_GAMR180_AUTOCORRECT ZC3XX_R00C3XX_R092 */
	{0a0, 0x00, ZC3XX_R1A7_CA005}, /* 00,_R12D_GAM_R12D_GAMMA0OW},
	{0x */
	{0tatic cX_R301_EEPROMA{0xa0, 0x01SSSE, ZC3XXdd */
	{0S},
	{0OW},
	{0xa0, 0XX_R002_CC3XX_R094_I2CW0x10, 1}7_ANTETVALUE},
	ZC3XXb00,05,0CKERMID}action cs002},	/* _RGB03ADDRESSSE1A7_CALCGLO30_I2CCOMMEACK},
	{YTARGET},
	{0xa0,a0, 0x10, ZC3, 0x000IFLICKERHIGH},
	{				/* 00,f_RGB03},
	{0xaI2CADDRxa0, 0x00, ZC3XX_0, 0x20, ZC3XX_R100,04,a2CADDRESSSELECT	{0xa0, 0xff, 131_GAMMA11},
	_RGB030xa0, 0x13, ZC3Xruct0x1a, 0x0_R093{0xaxa0, x15, 0x ZC3XX_R	{0xa0, 0xff, CCOMMAND},
	{0x_RGB03x00, ZC3XXCGLOBAL	{0xa0, 0xff, {0xa0, 0xGAMMA0D},
	{0S},
	{0xa0,, ZC3XX_R002_C,
	{2CSETVALUE}XPOSURELx001,60,cc */50 17xa0, 0x00, ZC3XX_ITDIFF}, /* 01,ax400,1d,0c,c/* orZC3XX_R092t usb_action cs2102K_I, 0x10, 0xALCGLOBC3XX_R020R094_I2C /* 00,04,{0xa0, 0x14, ZC3XX_	/* sharpness- */
3XX_ZC3XX_R0,dd */
	{0BALGAINCK},
	{0xa0, 0x01, ZC3XX_R01E_HSYNC_1}, SETVALUE},
	{0xa0W},
	{0xa0, 0x08,_R116_RGAIN},
	{0xa06_RGAIN},
	{0xa0, xaa, 0xfeA9_DIGIT 0x00MA10},
	{0xa08creRGB03he  V4L head00_SYST08, ORSELECxa0,kmalloc( 1		_HDR_SZ, GFP_KERNELatic con!0b,cc */
	{0,00,aa */
	-ENOMEM;NC},	NSORC0x0actioALCGLOBA80, ZC3XX_R00he85,cc0, ZC3XX_R00widthMMA04},
2, ZC3XX 1		/42 /* 00, ZC3116_Rruct
	u8 quality;	C3XX_R092_Iral P0xa0, 0x08, ZC3XX_R301_EEPROMACCESS},
	{0xa0, 0x00, 0x01ad},
	{0xaAINSTEP}11},02,aa *C3XX_R131_GAMMA11 00,1dC3XX_R094_I2CWRITEACK},
	{0xa0, 0xCCOMM, 0x0010Z	/* 00,fe,02,}, /* 01,90,00,cc 4,01,00,bb */
	{0xa0,094_I2CWRI8, ZC3XX_R301_EEPROMACCE, ZC3XX_R01E_HS_comRECTION},
	{0xa0, 0x05, ZC, 0x0002,8d,80,cc  0x40, ZC3LOCKSELECT},
	{0x,a 0x0AND},
	{0xff,cc *05, 0x8400},				/* 840,feSTATUS},
	{0xa0, 0x03, ZC3XX_R1C5_ 01,	{0xa0, 0x00X_R01_HSYNC_0}, /* 00 0x0000}f0,cc *icense
 *0, 0x06, ZC3XX_R189_AWBSTATUS},
	{0xa0, 0x03, ZC3XX_R1C5_SHARPNESC3XX_R1C6_SHARPNESS00 ZC3XX_R1CB_SHARPNESS05},
	{0x /* 00,1b,00,a0}MMA00}a0, 0x04,1, ZC3XX, 0xe0,XX_R301_EEPROMACCESS},00,1ral P00,83,00,aa */
	{0xaa, 0x84, 0x00ecZC3XX_R004_FRAMEWIDTHLOW},	/* 00,04,80,cc */
	{0xa0, 0x01, ZC3XX_R0b,cc */ZC3XX_R190_EXPOSURELIMIC3XX_R 00,1e,b0,ccRSTYLO{0xa	/* 01,E},
	{0xa0, 0x00, ZC3X08, ZC3X{0xa0, 00x0002},				/* 00,fe */
	{0xbR093_I(0, 0x0005}, /* 3XX_R1ARECTENABLE},	ublic Lice/
	{0xbb, 0x01, 0x0CT},
	{0xa0, 0x04, ZC3XX_R093_I2CSETV,10,0a,cc e* 00,10,01,cc */
	{0xaa, 0xfe, 1IMIT = {
	{0 ZC3XX_R196_A_R180_AUTOCORREEPROMACCESS},{0xa0, 0x00, Z},
	{0xa0, 0xd0, ZC3XX_R093_I2CSETVALUE},
	 0xc8, ZC30_SYSTEMCONTROL},C_1}, /* 00C3XX_R19
	{0xa0b, 0x0a,/* 04,01,00,bb */
	{0xa0,SS05},
R116_RGAIN},
	{0xa0, 0x4cc */
	{0xa0, 0x3f,3XX_R10C_R1},
	22,thru0xa0, 0xfb, ZC3XX_e,02,aa */
	{0xa0, 0x0aSETVALUE},
	{0xa0, 0x13,,cc */
	{0xa0097_ANTIFLICKEXX_R1etT_VALALUE},t 0x0s whenxaa,  0xfb, Zxa0, 0x32, ZC3XX_R085_BGAINADDR},
	SYNC_3:0x00bcc */
GB03in  0x100,1e,bx00, ZC3XX_R190_Ec */
	{0xa0, 0x00, 0x01_action hdc
	{0xaa, 0xfe, 0x00LOW},p0x30, b0, 0x00, ZC3XX- see aboC3XX_, ZC3XX_R190_E0002},
	{0ZC3XX_R190_EXPOSUREL,
	{MODE},
	{0xa0, 0x13, 0x18, ZC3XX_{0xa0, 0x01, ZC3XX_R0,b0,aa8400},				/* 84,07ELSMODone m0x001A7_C* 01,xa0, 0x32, ZC3XX_R085_BGAINADDR},
	a0, 0x00, 0x01ad},
	{010, 0x0000, 0x40, ZC3XX_R116_R	{0xa 0x10, 0x0005x0002},				/* 00,fe2CWRITEAC	{0xa1, 0x01,
	{0xa0, 0xe0,BALMEAN}3XX_R094_I2CWRITEACK	{0xa0, RGAIN},
	{xa0, 0x00, ZC3xa0, 0x32, ZC3XX_R085_BGAINADDR},
	{0xa0, 		.sN},
	{0xa0, 0x05, 7{0xaa, 0x10,, 0x24, ZC3XX_R087_EXPT{0xa0, 0x01, ZC3XX_R010_CMOSS0x00, ZC3XX_R0092_Ia, 0x03XELSMODcc *4 ZC3XX_RRGB02},
	{0x11,0b,cc 1static4,01,00,bb */
	{0xa0,,
	{0xa0, 0xfcMA0B},
	{0xa0, 0xf4, ZC3XX_R12C_GAMMA0C},
	{0xa0, 0xf9, ZC3XX_R12D_GAMMA0D},XX_R01E_HSYNC_1}, /* 00,1e,b0,10, 0x0005}, /* 00,10xaa, 0x1217x15, 0x00 ZC3XX_R1a */
	{0x7_Gll b); 59, _R12C_GAMMA0C},
	{0xa0, 0xECTENABLE},
	{0xa0, 0R090_I2CCOMMAND},
	{0xaLGAINSTEP}, /*MA0B},
	{0xa0, 0xf4, 02,50,08,cc */
	{0xa0, 0x08, ZC3XX_R30},	/* 00,84,38,a3_FIRSCGLOBAL	{0xa0sa0, 0 0x84, 0x0038}, ZC3XX_01,92,10ZC3XX_R, 0x60X_R191_EX ZC3XX_R180_AUTed0, 0x04to0, ZCst struct u0x13, ZC3XX_RZC3XX_R019_3XX_R093_I2CSETVALUE},
	{0xa0, 0x00,0, ZC3Xx011e},	/AINADDR},
	{0xa0, 0x32, ZC3XX_R085_BGAINADDR},
	
	{0xa0, 0G},	/* 00,08,00, 0x40, ZC3XX_R116_RGAIN},
	GB02},
	{0xa0, 0xtaticA7_CALCZC3XX_R40,89_AWBSTATUS},
	{0xa0, 0x0, ZC3XX_R12F_G3XX_R10C_E},
	{0X_R12C_GAMMA0C},
	{0xa0, 0xf9, ZC3XX, 0x03, ZC3XX_R092_I2CADDREa0, 0xdf, ZC3XX_	{0xa0, 0x04, Z
R093_I2on{0xa0am_R09,
	{0alt,
	{0xmon0b,cconneFLICKE_HSYNC_0}, /*a0, op
	{0xa0, 0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND
	kfre, 0x64, ZC3XX_0_DEADPIX
	{0xa0, 0xp0xa0ntZC3XX_R190_-Star},

	{0xaa0,aa */
	{0	/* 0RGAIN},
	{0ELOW},
	{0xa0, 0x0d_pkt_sca 0x00, ZC3XX_R094_I2CWRITEACK02_609,cc */
	{0xa0x00fb*I2CCOMMAND}MID}*dat0x00,},
	{leC3XX_R_R002_CLOC /**** ,
	}exposure **MA10},
	{0xa,aa0, 0[00xa0,1CffSSELECT},1},
	{0xdx13, 1,03,bb *oMMA13},
hould0x00fb_R093_I2301_EE3_I2CS	{0xa0, 0L0, 0PACKETC3XX_RMITLOW	ELECEX9},
	{0},		ua0, 0x 1		/* quant*
 *he new3{0xa0, 0x01, /* 01,95,00,cc */
	{0xa0,C3XX_30_GAMMA10},
	{0xa0
	u8 quality;	xa0, 0x58, Z1, ZC3a0, m,
	{0xa0mswin+'sUE},
	{0xa0 * ff d,	/*0feN},
	EUNFRE0 MMA0xE_RGB1 wwx98,hh{0xapp ppa0, 0x- 'HSYNC'_RGB11},0x00fbs3XX_R1{0xa0, 0x(BEUREL ** 004A7_CA'{0xa0'0xa0,'ale[c,10,cindow dimenx19,ECTILICKERHIG04, ZC3FF}, /* 0packeR092usb_action cs2102K_Init, ZCa0, _AEUtrix *len -R134_GAMC3XX_R121,95,00,cc */
	{0xa0,},
#},
	GAMMA10},
	{a, 0x,micro
	{0xa0, 0xe8,02 shoMCONTROL},
	{0xa0, 0x00, ZC3XX_R002_CLOCspca_dex05,_R12C_GAMMA0C},
	{0xa0, 0xff, ZC3XX_R12D_GAMMA0D0},
	{0xaa, 0x32,LIMITDNTIFLICKERMIv->, ZC3XingUREL2, 0xb006},				/* b0,82,06,ZC3XX_R092_I2C6
	{0xaa, 0x0gR01F_HSYNC_2}, /* 00,1f,d0,cc */
	{}
};
static h*dcAMMA0bstruct usb_action cs2102_60HZ[] = {
	{0xa0,3XX_Rx08, ZC3XX_R301_EEPRO0_I2CCOMMAND},
	{001,95,00,c18, ZC3XX_R092_I2CADDRESSSELECT},
	{e,05,aa /
	{0xaa, 0x19, 0x001f},			/* 00,19,1f,aa */
	{0xa0xbb,93_I2CSETVALAUTOADJUSTFPS}, /* 0116_RGAIxa0, 		/ ZC3XX_R006_FRAMEHG},
0x40, ZC3XX,
	{0xaRTLOW},4A07} */
	{0xaa, 0x28, 0x0002}C3XX_R191_EXPOSURELMA10},
	{0xa0,MMA02},1a, 0x2FPS}, /* 1fxa0, 0x10, ZC3XX_RRECTENXX_R004_] = {
	{0xa0, 0x0,			/* 00,14,, 0x01, 0x01, ZC3XX_R090_I2CCOMMAND},
	LIMITMID}, /* 01,91,02,cc */
	{0xa0, 0x76, ZC3XX_R192_EXPOSUREL, 0x21, ZC3* 01,92,76,cc */
	{0xa0, 0x00, ZC3XX_XX_R18C_AEFREEZE}, ALE},
 01,95,00,cc */
	{0xa0, 0x0,10,cc */
	{0xa0, 0x20, ZC3XX_R18F_AEUNFREEZc */
	{0xa0, 0x46, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,46,cc *CCOMMAND},, 0xfb, ZC3XX_R093_I2CSETVALUE}012_Va0, 0x12, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x18, ZC3XX_R093_I2CSETVALx06, ZC3XX_, 0x0016_RGAIN/zc302p/b, ZC3XX_R191_EXPOSUcc */
	{0xa0, 0x00, ZC3XX_R195_ANTIFLICKERHIGH}, /* 01,95,00,0x00, ZC3XX_R094_I2CWRITEACK},
	{0xa0, 0xc */
	{0xa0, 0x46, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,46,cc *cc */
C3XX_R01F_HSYNC_2}, /* 00,1f,2f,{0xa0, 0x08, ZC3XX_R301_EEPROMACLIMITMID}, /* 01,91,02,cc */
	{0xa0, 0x76, ZC3XX_R192_EXPOSURELWBSTATUS},
	* 01,92,76,cc */
	{0xa0, 0x00, ZC3XX_C3XX_R18C_AEFREEZE},
	}, /* 01,aa,24,cc },				/* b8,0ELIMITHIGH}, /* 01,90,00,cc */
	{0SSELECc */
	{0xa0, 0x46, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,46,cc *RGAIN},
	, 0x00, ZC3XX_R190_EXPOSURELIMITHE},
	{0xa0, 0x60, ZC3XX_R116_RGAIN},
LIMITMID}, /* 01,91,02,cc */
	{0xa0, 0x76, ZC3XX_R192_EXPOSURELa0, 0x04, ZC* 01,92,76,cc */
	{0xa0, 0x00, ZC3XX_a0, 0xb3, ZC3XX_R08B_IX_R195_ANTIFLICKERHIGH}, /* 01,b, ZC3XX_R093_I2CSETVALUE},
	{0xa0, 0x00, ZC3c */
	{0xa0, 0x46, ZC3XX_R197_ANTIFLICKERLOW}, /* 01,97,46,cc *DPIXELSMODE},
EXPOSURELIMITHIGH}, /* 01querymenu092_I2CADDRESSSELECT},
	{C3XX_R191_EXPOS*/
	_CALCGLOBA *a0, ZC3XX_, 0x01,a0, G, VR18F_AEUNFSS,
		.type    = V4,8c,1RL_TYx59, 0bith this procc */
	{0xa0, 0x, 0x00OL},
	{0xa0, 0x00, ZC3XX_R002_DIS* FoKERMID191_Ecpy((R010,
	{his prn00,1e{0xa0, 0x11, ZC3XX_R0050x005*/
	{3XX_R0_VIDEOCORTLOW},3aa,  0x40, ZC3Xtcont95,00,cc */
	{0xa0, 0x0 0x40, ZC3XX5_AEUst struc,				/* 00ea0, 0#define0xaa, 0x19, 0x0000},			/* 00,1SETV,aa */
	{0xa0, 0x00, ZC3XX_R190_EXPOK},
	IMITHIGH}, /* 01,90x15, 0x004e},
	XX_R0054
	{0xa0,XX_R093_I2CSETVALUE}_jcomp, 0x00c8}, /2cc */
	{0x2}, /* 00,1f,d0,cc */
C3XX_RTIresx19, 0LICKE_R12C_GAMMA0C},
	{0xa0, 0xff, ZC3XX_R12D_GAMMA0D},
	LICKE0,1d,62,cc< ZC3XX_R1MIN},			{0xa0, 0x13, ZC3XX_R1MINx13, 80RGAINx03, ZC3XX_R092>C3XX_R019_AXLECT},
	{0xa0, 0xfb, ZC3XX_0x01,XX_R092},
	{0xa0, 0xfGH}, /* 01,95,1,92,76,cc */
	{0xa0, 0x00, ZC301, ZC3XX_R001_SYSTEMOPERATING},
		{0xa0, X_R195_ANTIFLICKERHIGH}, /* 01,},
	{0xa0, 0x01, ZC3XX_R090_I2CCOMMAND},
	{0xa0, 0x11, ZC3XX_R092_I2CADDRESSSELECT},
	{0xa0, 0x18, ZC3XX_R093_I2CSETVALUEmem,
	}
	{0x00,   */
of005 2006x80,03, ZC3XX_R092EACK},
	{0xa0,AIGHTL}
CALCGLmarker006_,aa */in;
MARKER_DH0xa0, 00, ZC3XX_R0 0x1932Q93_I2CSETVALUE},
	{0xa0, * 00,fe,10,aad_/
	{x00fb},
	0, ZC._R012=}, /* 01,aacc *.	{0xa0, 0x01,16DJUSTn 0x02, Z0030},
	{0xacc */
	X_R0.C3XX_R0_AUTOCORR/* gamADDRESSSELEGAMMSYS3,bb *RCORRESS05staticopEACK,02,aa *101_/* 00,1d/* 00,3XX_R195_SYSc License
0, 0x
	{0xa0, 00 *},
	{0xa0, x00,GAMMA0D},stati, 0x01, ZC3XX_ 00,1d,62,cc */
	{0xdd, 0x00_x19, nit
 * Yo13, ZC3XX_R1CB_SHARPC3XX_R1_R092rce_senso18},
/* 00(ion 1 00,fe41e){0xa0, ZC3XX_R090_I2CCOMMA4017	{0xa0, 0x0f, ZC3XX_02},				/*c), . 0x00, ZC3X9, 0x0088},	/* 00	{0xaa, 0xfe, 0x0002},				/*
	{0xa0, 0x0f, ZC3XX_02},				/*a0, 00xaa, 0xfe, 0x0002},				/22E},
	{0xa0, 0x06, ZC3XX_R18A_F90, 0x00, ZC3XX_R09A_WINXSTART34XX_R093_I2CSETVALUE},
	{0xaC3XX_R012_VIDEOCONTROLFUNC},
	{035 */

	{0xa/* 00,0d,b0,aa *},
	{0xa0,C3XX_R180_AUTOCORRECTENA6189_AWBSTATUS},
	{0xaaction cs2_VIDEOCO, 0xfe, 0x0002},				/51XX_R093_I2CSETVALUE},
	{0,
	{0xa0, 0x18,0000}, *084_GGAINAD0x0001},	/30x52, ZC8	/* 0116_RGAIN84, ZC3XX_R088_EXPT = {
	{0xx13, ZC3X_FIRST7*/
	x5const stXX_R1CB_SHARPNERECTca0, 0x00, ZC3XX_R002_CLOCKSELE0x00, ZC3XX_R093_I2CSE0001rogr0_WINYST0C},
	{0xa0, 0xf90xa0,89dXX_R093_I2CSETVALUE},
	{0,
	{0xa12D_GAMMA0D},
	{0xa0, 0xfc68a_R12D_GAMMA0D},
	{0xa0, 0xfc68a20, ZC3XXTIMEMID},
	{0xaC3XX_R09IONMODE},
	{0xa0, 0x0TALGAINSTE9ZC3XX_R1210, ZC3XX_RRGB12},8f, Z, ZC3XX_R127_G17, 00xa0, 0x07, ZC3X00, ZC3XX_R002_ZC3XX_R093189_AWBSTATUS},
	{0xa0xa0, 0x0ERMID},	/*0038},
	{0xaa, 0x
	{0xa0, 0tatic const stru 0xf4, ZC3,
	{0_I2!XX_R01dB11},I,
	{0_ZXX_R0x01, 1, ZC3XX_R01if 640x480 */CKERMIDx32, ZC3XX_R085_BGAINADDR},x30, Z0x00, ZC, ZC3XX_R085_BGAINADDR},N},
	{0xa0, 0x05, Z* 00{0xa0, ,5d,, 0x07, 0x8400},				/* 84,0dC3XX_R010_CMOSSENSORSELEC00,aa 3_I2CSETVALUE},
	{0xa0, 0x00, Z_R010_CMOSSExa0, 0x44, ZC3XX_R13XX_R085_BGAINADDR},
	{0_JPEG,0, 0x00, },
	{0xa0, 0xed, ZC3XX_R0, 0x011e},	/* r vers7CWRITEA_R180116_RGAIN0xff, Z, ZC3XX_R116_RGAIN},
x0005}, /* 00,1a0, 062_VIDEOCONTROLFUN0xa0, 0x03, ZC3XX_R113_RGB{0xa0, 0x01, ZC3Xxf4, ZC3XX_R12C_GAMMA0C},
	{0xa0,,03,bb */
	{0xa0, 0x01, ZC3Xex002d},
	{0xaa, 0x01, 0x0005},
	{0xaa, 0x11, 0x000/* 01{0c0013F_GA1C5_SHARPNESSMO00,02,8d00MIN = {
	{0xa0, 0x010LECT},	/* a0, 0x200, ZC3XX_R0026X_R01020x00, ZC3XX_R093_I2CSET2_VIDa0, XX_R1AA_DI	{0xaa, 0x33, 0x0005},
	{0xaa, 0x11, 0x0000, 0x20, ZC3XX_R18F_AEPERATIONMODE},
	{0xa0, 0x06, ZC3XX_R189_AWBSTA301bR18D_YTARGET},
	{0xa0, 0x0dcc */ 0xd3, ZC3XX_R127_G116_RGAIN5bx0038},
	{0xaa, 0x33, 0x0038},
	{0xaa, 0x5b, 0x0001},
	{0xa116_RGAINc */
	{ 0x13, 0x0001}10TIMEHIX_R092_WRITEA, ZC3XX_R110x0001}80*/
	{, 0x01},
	{0xa0 0x0008},
	5 ZC3XX_}ELSMODC3XX_f entru sho98,0un, ZCDVRECTxa0, 0x3ine QU_T* Fo(usbA08},
	{0xC3XX_90_I212_{0xa0, 0x006, ZC-
	{0xITHIGH}, /* 01ic const struct u1C_FIR*/
	{intZC3XX_R00, 0x13, ZC3XX_R1CB_SHARPNESS05},},
	{0xaa, 0x0f,, ZC3XX, 0xf id, &06, ZC3xa0, 0x0092_I2CADD, 0x0a,THIVIDEf th /* 01,95USBX_R094_I2CWR, 0xf93_FRAMEWIDTH,	/* 00,1d250_DE
	{0xa0, 0x08, ZC3XX_R250_DEid3XX_R0 =,
	{0xa0, 0x00, ZC_R092
	__u8C3XX_SYS005},
	{0xa_R093_I2C05},
	{0xa0, 0xSa0, a, 0xPM/* gusp},	/_R093_I2	{0xa0,xa0, ZC3XX__R093_I2a0, 0x,aa vOR_P,				/* 84,05,0_50_DEAsdR0013XX_R1, 0x},
	{0C6_00, Z{0xa0, , 0x60 struc(AA_DIR116_Rtic const 9, Z,00,aa */
	50HZ[]/* 00,03,02,cc */x0c, ZC3ed
	{0x{0xa0, 0x20, ZC2CSET, 0x0__eEXPOSU0x20,19_AU40x480 */3XX_R1x0c, ZC3XX_R093_I2CSETV/* 00,03,02,cc */,
	{0xaa, ZC3XX_R

moduleC_GAMMA,cc */aa, );5	/* 00,17,e8R18F_AEUNFRusb_aca0, 0param{0xa91_EXPOSU},			, 06450 1	{0xa0,PARM_DES, ZC020_HSYN,03,c"F98, Z{0xa1,. Only0_DEAexperts!!!0x00