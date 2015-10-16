/*
 * USB IBM C-It Video Camera driver
 *
 * Supports Xirlink C-It Video Camera, IBM PC Camera,
 * IBM NetCamera and Veo Stingray.
 *
 * This driver is based on earlier work of:
 *
 * (C) Copyright 1999 Johannes Erdfelt
 * (C) Copyright 1999 Randy Dunlap
 *
 * 5/24/00 Removed optional (and unnecessary) locking of the driver while
 * the device remains plugged in. Corrected race conditions in ibmcam_open
 * and ibmcam_probe() routines using this as a guideline:
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>

#include "usbvideo.h"

#define IBMCAM_VENDOR_ID	0x0545
#define IBMCAM_PRODUCT_ID	0x8080
#define NETCAM_PRODUCT_ID	0x8002	/* IBM NetCamera, close to model 2 */
#define VEO_800C_PRODUCT_ID	0x800C	/* Veo Stingray, repackaged Model 2 */
#define VEO_800D_PRODUCT_ID	0x800D	/* Veo Stingray, repackaged Model 4 */

#define MAX_IBMCAM		4	/* How many devices we allow to connect */
#define USES_IBMCAM_PUTPIXEL    0       /* 0=Fast/oops 1=Slow/secure */

/* Header signatures */

/* Model 1 header: 00 FF 00 xx */
#define HDRSIG_MODEL1_128x96	0x06	/* U Y V Y ... */
#define HDRSIG_MODEL1_176x144	0x0e	/* U Y V Y ... */
#define HDRSIG_MODEL1_352x288	0x00	/* V Y U Y ... */

#define	IBMCAM_MODEL_1	1	/* XVP-501, 3 interfaces, rev. 0.02 */
#define IBMCAM_MODEL_2	2	/* KSX-X9903, 2 interfaces, rev. 3.0a */
#define IBMCAM_MODEL_3	3	/* KSX-X9902, 2 interfaces, rev. 3.01 */
#define	IBMCAM_MODEL_4	4	/* IBM NetCamera, 0545/8002/3.0a */

/* Video sizes supported */
#define	VIDEOSIZE_128x96	VIDEOSIZE(128, 96)
#define	VIDEOSIZE_176x144	VIDEOSIZE(176,144)
#define	VIDEOSIZE_352x288	VIDEOSIZE(352,288)
#define	VIDEOSIZE_320x240	VIDEOSIZE(320,240)
#define	VIDEOSIZE_352x240	VIDEOSIZE(352,240)
#define	VIDEOSIZE_640x480	VIDEOSIZE(640,480)
#define	VIDEOSIZE_160x120	VIDEOSIZE(160,120)

/* Video sizes supported */
enum {
	SIZE_128x96 = 0,
	SIZE_160x120,
	SIZE_176x144,
	SIZE_320x240,
	SIZE_352x240,
	SIZE_352x288,
	SIZE_640x480,
	/* Add/remove/rearrange items before this line */
	SIZE_LastItem
};

/*
 * This structure lives in uvd->user field.
 */
typedef struct {
	int initialized;	/* Had we already sent init sequence? */
	int camera_model;	/* What type of IBM camera we got? */
	int has_hdr;
} ibmcam_t;
#define	IBMCAM_T(uvd)	((ibmcam_t *)((uvd)->user_data))

static struct usbvideo *cams;

static int debug;

static int flags; /* = FLAGS_DISPLAY_HINTS | FLAGS_OVERLAY_STATS; */

static const int min_canvasWidth  = 8;
static const int min_canvasHeight = 4;

static int lighting = 1; /* Medium */

#define SHARPNESS_MIN	0
#define SHARPNESS_MAX	6
static int sharpness = 4; /* Low noise, good details */

#define FRAMERATE_MIN	0
#define FRAMERATE_MAX	6
static int framerate = -1;

static int size = SIZE_352x288;

/*
 * Here we define several initialization variables. They may
 * be used to automatically set color, hue, brightness and
 * contrast to desired values. This is particularly useful in
 * case of webcams (which have no controls and no on-screen
 * output) and also when a client V4L software is used that
 * does not have some of those controls. In any case it's
 * good to have startup values as options.
 *
 * These values are all in [0..255] range. This simplifies
 * operation. Note that actual values of V4L variables may
 * be scaled up (as much as << 8). User can see that only
 * on overlay output, however, or through a V4L client.
 */
static int init_brightness = 128;
static int init_contrast = 192;
static int init_color = 128;
static int init_hue = 128;
static int hue_correction = 128;

/* Settings for camera model 2 */
static int init_model2_rg2 = -1;
static int init_model2_sat = -1;
static int init_model2_yb = -1;

/* 01.01.08 - Added for RCA video in support -LO */
/* Settings for camera model 3 */
static int init_model3_input;

module_param(debug, int, 0);
MODULE_PARM_DESC(debug, "Debug level: 0-9 (default=0)");
module_param(flags, int, 0);
MODULE_PARM_DESC(flags, "Bitfield: 0=VIDIOCSYNC, 1=B/W, 2=show hints, 3=show stats, 4=test pattern, 5=separate frames, 6=clean frames");
module_param(framerate, int, 0);
MODULE_PARM_DESC(framerate, "Framerate setting: 0=slowest, 6=fastest (default=2)");
module_param(lighting, int, 0);
MODULE_PARM_DESC(lighting, "Photosensitivity: 0=bright, 1=medium (default), 2=low light");
module_param(sharpness, int, 0);
MODULE_PARM_DESC(sharpness, "Model1 noise reduction: 0=smooth, 6=sharp (default=4)");
module_param(size, int, 0);
MODULE_PARM_DESC(size, "Image size: 0=128x96 1=160x120 2=176x144 3=320x240 4=352x240 5=352x288 6=640x480  (default=5)");
module_param(init_brightness, int, 0);
MODULE_PARM_DESC(init_brightness, "Brightness preconfiguration: 0-255 (default=128)");
module_param(init_contrast, int, 0);
MODULE_PARM_DESC(init_contrast, "Contrast preconfiguration: 0-255 (default=192)");
module_param(init_color, int, 0);
MODULE_PARM_DESC(init_color, "Color preconfiguration: 0-255 (default=128)");
module_param(init_hue, int, 0);
MODULE_PARM_DESC(init_hue, "Hue preconfiguration: 0-255 (default=128)");
module_param(hue_correction, int, 0);
MODULE_PARM_DESC(hue_correction, "YUV colorspace regulation: 0-255 (default=128)");

module_param(init_model2_rg2, int, 0);
MODULE_PARM_DESC(init_model2_rg2, "Model2 preconfiguration: 0-255 (default=47)");
module_param(init_model2_sat, int, 0);
MODULE_PARM_DESC(init_model2_sat, "Model2 preconfiguration: 0-255 (default=52)");
module_param(init_model2_yb, int, 0);
MODULE_PARM_DESC(init_model2_yb, "Model2 preconfiguration: 0-255 (default=160)");

/* 01.01.08 - Added for RCA video in support -LO */
module_param(init_model3_input, int, 0);
MODULE_PARM_DESC(init_model3_input, "Model3 input: 0=CCD 1=RCA");

MODULE_AUTHOR ("Dmitri");
MODULE_DESCRIPTION ("IBM/Xirlink C-it USB Camera Driver for Linux (c) 2000");
MODULE_LICENSE("GPL");

/* Still mysterious i2c commands */
static const unsigned short unknown_88 = 0x0088;
static const unsigned short unknown_89 = 0x0089;
static const unsigned short bright_3x[3] = { 0x0031, 0x0032, 0x0033 };
static const unsigned short contrast_14 = 0x0014;
static const unsigned short light_27 = 0x0027;
static const unsigned short sharp_13 = 0x0013;

/* i2c commands for Model 2 cameras */
static const unsigned short mod2_brightness = 0x001a;		/* $5b .. $ee; default=$5a */
static const unsigned short mod2_set_framerate = 0x001c;	/* 0 (fast).. $1F (slow) */
static const unsigned short mod2_color_balance_rg2 = 0x001e;	/* 0 (red) .. $7F (green) */
static const unsigned short mod2_saturation = 0x0020;		/* 0 (b/w) - $7F (full color) */
static const unsigned short mod2_color_balance_yb = 0x0022;	/* 0..$7F, $50 is about right */
static const unsigned short mod2_hue = 0x0024;			/* 0..$7F, $70 is about right */
static const unsigned short mod2_sensitivity = 0x0028;		/* 0 (min) .. $1F (max) */

struct struct_initData {
	unsigned char req;
	unsigned short value;
	unsigned short index;
};

/*
 * ibmcam_size_to_videosize()
 *
 * This procedure converts module option 'size' into the actual
 * videosize_t that defines the image size in pixels. We need
 * simplified 'size' because user wants a simple enumerated list
 * of choices, not an infinite set of possibilities.
 */
static videosize_t ibmcam_size_to_videosize(int size)
{
	videosize_t vs = VIDEOSIZE_352x288;
	RESTRICT_TO_RANGE(size, 0, (SIZE_LastItem-1));
	switch (size) {
	case SIZE_128x96:
		vs = VIDEOSIZE_128x96;
		break;
	case SIZE_160x120:
		vs = VIDEOSIZE_160x120;
		break;
	case SIZE_176x144:
		vs = VIDEOSIZE_176x144;
		break;
	case SIZE_320x240:
		vs = VIDEOSIZE_320x240;
		break;
	case SIZE_352x240:
		vs = VIDEOSIZE_352x240;
		break;
	case SIZE_352x288:
		vs = VIDEOSIZE_352x288;
		break;
	case SIZE_640x480:
		vs = VIDEOSIZE_640x480;
		break;
	default:
		err("size=%d. is not valid", size);
		break;
	}
	return vs;
}

/*
 * ibmcam_find_header()
 *
 * Locate one of supported header markers in the queue.
 * Once found, remove all preceding bytes AND the marker (4 bytes)
 * from the data pump queue. Whatever follows must be video lines.
 *
 * History:
 * 1/21/00  Created.
 */
static enum ParseState ibmcam_find_header(struct uvd *uvd) /* FIXME: Add frame here */
{
	struct usbvideo_frame *frame;
	ibmcam_t *icam;

	if ((uvd->curframe) < 0 || (uvd->curframe >= USBVIDEO_NUMFRAMES)) {
		err("ibmcam_find_header: Illegal frame %d.", uvd->curframe);
		return scan_EndParse;
	}
	icam = IBMCAM_T(uvd);
	assert(icam != NULL);
	frame = &uvd->frame[uvd->curframe];
	icam->has_hdr = 0;
	switch (icam->camera_model) {
	case IBMCAM_MODEL_1:
	{
		const int marker_len = 4;
		while (RingQueue_GetLength(&uvd->dp) >= marker_len) {
			if ((RING_QUEUE_PEEK(&uvd->dp, 0) == 0x00) &&
			    (RING_QUEUE_PEEK(&uvd->dp, 1) == 0xFF) &&
			    (RING_QUEUE_PEEK(&uvd->dp, 2) == 0x00))
			{
#if 0				/* This code helps to detect new frame markers */
				dev_info(&uvd->dev->dev,
					 "Header sig: 00 FF 00 %02X\n",
					 RING_QUEUE_PEEK(&uvd->dp, 3));
#endif
				frame->header = RING_QUEUE_PEEK(&uvd->dp, 3);
				if ((frame->header == HDRSIG_MODEL1_128x96) ||
				    (frame->header == HDRSIG_MODEL1_176x144) ||
				    (frame->header == HDRSIG_MODEL1_352x288))
				{
#if 0
					dev_info(&uvd->dev->dev,
						 "Header found.\n");
#endif
					RING_QUEUE_DEQUEUE_BYTES(&uvd->dp, marker_len);
					icam->has_hdr = 1;
					break;
				}
			}
			/* If we are still here then this doesn't look like a header */
			RING_QUEUE_DEQUEUE_BYTES(&uvd->dp, 1);
		}
		break;
	}
	case IBMCAM_MODEL_2:
case IBMCAM_MODEL_4:
	{
		int marker_len = 0;
		switch (uvd->videosize) {
		case VIDEOSIZE_176x144:
			marker_len = 10;
			break;
		default:
			marker_len = 2;
			break;
		}
		while (RingQueue_GetLength(&uvd->dp) >= marker_len) {
			if ((RING_QUEUE_PEEK(&uvd->dp, 0) == 0x00) &&
			    (RING_QUEUE_PEEK(&uvd->dp, 1) == 0xFF))
			{
#if 0
				dev_info(&uvd->dev->dev, "Header found.\n");
#endif
				RING_QUEUE_DEQUEUE_BYTES(&uvd->dp, marker_len);
				icam->has_hdr = 1;
				frame->header = HDRSIG_MODEL1_176x144;
				break;
			}
			/* If we are still here then this doesn't look like a header */
			RING_QUEUE_DEQUEUE_BYTES(&uvd->dp, 1);
		}
		break;
	}
	case IBMCAM_MODEL_3:
	{	/*
		 * Headers: (one precedes every frame). nc=no compression,
		 * bq=best quality bf=best frame rate.
		 *
		 * 176x144: 00 FF 02 { 0A=nc CA=bq EA=bf }
		 * 320x240: 00 FF 02 { 08=nc 28=bq 68=bf }
		 * 640x480: 00 FF 03 { 08=nc 28=bq 68=bf }
		 *
		 * Bytes '00 FF' seem to indicate header. Other two bytes
		 * encode the frame type. This is a set of bit fields that
		 * encode image size, compression type etc. These fields
		 * do NOT contain frame number because all frames carry
		 * the same header.
		 */
		const int marker_len = 4;
		while (RingQueue_GetLength(&uvd->dp) >= marker_len) {
			if ((RING_QUEUE_PEEK(&uvd->dp, 0) == 0x00) &&
			    (RING_QUEUE_PEEK(&uvd->dp, 1) == 0xFF) &&
			    (RING_QUEUE_PEEK(&uvd->dp, 2) != 0xFF))
			{
				/*
				 * Combine 2 bytes of frame type into one
				 * easy to use value
				 */
				unsigned long byte3, byte4;

				byte3 = RING_QUEUE_PEEK(&uvd->dp, 2);
				byte4 = RING_QUEUE_PEEK(&uvd->dp, 3);
				frame->header = (byte3 << 8) | byte4;
#if 0
				dev_info(&uvd->dev->dev, "Header found.\n");
#endif
				RING_QUEUE_DEQUEUE_BYTES(&uvd->dp, marker_len);
				icam->has_hdr = 1;
				break;
			}
			/* If we are still here then this doesn't look like a header */
			RING_QUEUE_DEQUEUE_BYTES(&uvd->dp, 1);
		}
		break;
	}
	default:
		break;
	}
	if (!icam->has_hdr) {
		if (uvd->debug > 2)
			dev_info(&uvd->dev->dev,
				 "Skipping frame, no header\n");
		return scan_EndParse;
	}

	/* Header found */
	icam->has_hdr = 1;
	uvd->stats.header_count++;
	frame->scanstate = ScanState_Lines;
	frame->curline = 0;

	if (flags & FLAGS_FORCE_TESTPATTERN) {
		usbvideo_TestPattern(uvd, 1, 1);
		return scan_NextFrame;
	}
	return scan_Continue;
}

/*
 * ibmcam_parse_lines()
 *
 * Parse one line (interlaced) from the buffer, put
 * decoded RGB value into the current frame buffer
 * and add the written number of bytes (RGB) to
 * the *pcopylen.
 *
 * History:
 * 21-Jan-2000 Created.
 * 12-Oct-2000 Reworked to reflect interlaced nature of the data.
 */
static enum ParseState ibmcam_parse_lines(
	struct uvd *uvd,
	struct usbvideo_frame *frame,
	long *pcopylen)
{
	unsigned char *f;
	ibmcam_t *icam;
	unsigned int len, scanLength, scanHeight, order_uv, order_yc;
	int v4l_linesize; /* V4L line offset */
	const int hue_corr  = (uvd->vpic.hue - 0x8000) >> 10;	/* -32..+31 */
	const int hue2_corr = (hue_correction - 128) / 4;		/* -32..+31 */
	const int ccm = 128; /* Color correction median - see below */
	int y, u, v, i, frame_done=0, color_corr;
	static unsigned char lineBuffer[640*3];
	unsigned const char *chromaLine, *lumaLine;

	assert(uvd != NULL);
	assert(frame != NULL);
	icam = IBMCAM_T(uvd);
	assert(icam != NULL);
	color_corr = (uvd->vpic.colour - 0x8000) >> 8; /* -128..+127 = -ccm..+(ccm-1)*/
	RESTRICT_TO_RANGE(color_corr, -ccm, ccm+1);

	v4l_linesize = VIDEOSIZE_X(frame->request) * V4L_BYTES_PER_PIXEL;

	if (IBMCAM_T(uvd)->camera_model == IBMCAM_MODEL_4) {
		/* Model 4 frame markers do not carry image size identification */
		switch (uvd->videosize) {
		case VIDEOSIZE_128x96:
		case VIDEOSIZE_160x120:
		case VIDEOSIZE_176x144:
			scanLength = VIDEOSIZE_X(uvd->videosize);
			scanHeight = VIDEOSIZE_Y(uvd->videosize);
			break;
		default:
			err("ibmcam_parse_lines: Wrong mode.");
			return scan_Out;
		}
		order_yc = 1;	/* order_yc: true=Yc false=cY ('c'=either U or V) */
		order_uv = 1;	/* Always true in this algorithm */
	} else {
		switch (frame->header) {
		case HDRSIG_MODEL1_128x96:
			scanLength = 128;
			scanHeight = 96;
			order_uv = 1;	/* U Y V Y ... */
			break;
		case HDRSIG_MODEL1_176x144:
			scanLength = 176;
			scanHeight = 144;
			order_uv = 1;	/* U Y V Y ... */
			break;
		case HDRSIG_MODEL1_352x288:
			scanLength = 352;
			scanHeight = 288;
			order_uv = 0;	/* Y V Y V ... */
			break;
		default:
			err("Unknown header signature 00 FF 00 %02lX", frame->header);
			return scan_NextFrame;
		}
		/* order_yc: true=Yc false=cY ('c'=either U or V) */
		order_yc = (IBMCAM_T(uvd)->camera_model == IBMCAM_MODEL_2);
	}

	len = scanLength * 3;
	assert(len <= sizeof(lineBuffer));

	/*
	 * Lines are organized this way:
	 *
	 * I420:
	 * ~~~~
	 * <scanLength->
	 * ___________________________________
	 * |-----Y-----|---UVUVUV...UVUV-----| \
	 * |-----------+---------------------|  \
	 * |<-- 176 -->|<------ 176*2 ------>|  Total 72. lines (interlaced)
	 * |...	   ... |        ...          |  /
	 * |<-- 352 -->|<------ 352*2 ------>|  Total 144. lines (interlaced)
	 * |___________|_____________________| /
	 *  \           \
	 *   lumaLine    chromaLine
	 */

	/* Make sure there's enough data for the entire line */
	if (RingQueue_GetLength(&uvd->dp) < len)
		return scan_Out;

	/* Suck one line out of the ring queue */
	RingQueue_Dequeue(&uvd->dp, lineBuffer, len);

	/*
	 * Make sure that our writing into output buffer
	 * will not exceed the buffer. Mind that we may write
	 * not into current output scanline but in several after
	 * it as well (if we enlarge image vertically.)
	 */
	if ((frame->curline + 2) >= VIDEOSIZE_Y(frame->request))
		return scan_NextFrame;

	/*
	 * Now we are sure that entire line (representing all 'scanLength'
	 * pixels from the camera) is available in the buffer. We
	 * start copying the line left-aligned to the V4L buffer.
	 * If the camera line is shorter then we should pad the V4L
	 * buffer with something (black) to complete the line.
	 */
	assert(frame->data != NULL);
	f = frame->data + (v4l_linesize * frame->curline);

	/*
	 * To obtain chrominance data from the 'chromaLine' use this:
	 *   v = chromaLine[0]; // 0-1:[0], 2-3:[4], 4-5:[8]...
	 *   u = chromaLine[2]; // 0-1:[2], 2-3:[6], 4-5:[10]...
	 *
	 * Indices must be calculated this way:
	 * v_index = (i >> 1) << 2;
	 * u_index = (i >> 1) << 2 + 2;
	 *
	 * where 'i' is the column number [0..VIDEOSIZE_X(frame->request)-1]
	 */
	lumaLine = lineBuffer;
	chromaLine = lineBuffer + scanLength;
	for (i = 0; i < VIDEOSIZE_X(frame->request); i++)
	{
		unsigned char rv, gv, bv;	/* RGB components */

		/* Check for various visual debugging hints (colorized pixels) */
		if ((flags & FLAGS_DISPLAY_HINTS) && (icam->has_hdr)) {
			/*
			 * This is bad and should not happen. This means that
			 * we somehow overshoot the line and encountered new
			 * frame! Obviously our camera/V4L frame size is out
			 * of whack. This cyan dot will help you to figure
			 * out where exactly the new frame arrived.
			 */
			if (icam->has_hdr == 1) {
				bv = 0; /* Yellow marker */
				gv = 0xFF;
				rv = 0xFF;
			} else {
				bv = 0xFF; /* Cyan marker */
				gv = 0xFF;
				rv = 0;
			}
			icam->has_hdr = 0;
			goto make_pixel;
		}

		/*
		 * Check if we are still in range. We may be out of range if our
		 * V4L canvas is wider or taller than the camera "native" image.
		 * Then we quickly fill the remainder of the line with zeros to
		 * make black color and quit the horizontal scanning loop.
		 */
		if (((frame->curline + 2) >= scanHeight) || (i >= scanLength)) {
			const int j = i * V4L_BYTES_PER_PIXEL;
#if USES_IBMCAM_PUTPIXEL
			/* Refresh 'f' because we don't use it much with PUTPIXEL */
			f = frame->data + (v4l_linesize * frame->curline) + j;
#endif
			memset(f, 0, v4l_linesize - j);
			break;
		}

		y = lumaLine[i];
		if (flags & FLAGS_MONOCHROME) /* Use monochrome for debugging */
			rv = gv = bv = y;
		else {
			int off_0, off_2;

			off_0 = (i >> 1) << 2;
			off_2 = off_0 + 2;

			if (order_yc) {
				off_0++;
				off_2++;
			}
			if (!order_uv) {
				off_0 += 2;
				off_2 -= 2;
			}
			u = chromaLine[off_0] + hue_corr;
			v = chromaLine[off_2] + hue2_corr;

			/* Apply color correction */
			if (color_corr != 0) {
				/* Magnify up to 2 times, reduce down to zero saturation */
				u = 128 + ((ccm + color_corr) * (u - 128)) / ccm;
				v = 128 + ((ccm + color_corr) * (v - 128)) / ccm;
			}
			YUV_TO_RGB_BY_THE_BOOK(y, u, v, rv, gv, bv);
		}

	make_pixel:
		/*
		 * The purpose of creating the pixel here, in one,
		 * dedicated place is that we may need to make the
		 * pixel wider and taller than it actually is. This
		 * may be used if camera generates small frames for
		 * sake of frame rate (or any other reason.)
		 *
		 * The output data consists of B, G, R bytes
		 * (in this order).
		 */
#if USES_IBMCAM_PUTPIXEL
		RGB24_PUTPIXEL(frame, i, frame->curline, rv, gv, bv);
#else
		*f++ = bv;
		*f++ = gv;
		*f++ = rv;
#endif
		/*
		 * Typically we do not decide within a legitimate frame
		 * that we want to end the frame. However debugging code
		 * may detect marker of new frame within the data. Then
		 * this condition activates. The 'data' pointer is already
		 * pointing at the new marker, so we'd better leave it as is.
		 */
		if (frame_done)
			break;	/* End scanning of lines */
	}
	/*
	 * Account for number of bytes that we wrote into output V4L frame.
	 * We do it here, after we are done with the scanline, because we
	 * may fill more than one output scanline if we do vertical
	 * enlargement.
	 */
	frame->curline += 2;
	if (pcopylen != NULL)
		*pcopylen += 2 * v4l_linesize;
	frame->deinterlace = Deinterlace_FillOddLines;

	if (frame_done || (frame->curline >= VIDEOSIZE_Y(frame->request)))
		return scan_NextFrame;
	else
		return scan_Continue;
}

/*
 * ibmcam_model2_320x240_parse_lines()
 *
 * This procedure deals with a weird RGB format that is produced by IBM
 * camera model 2 in modes 320x240 and above; 'x' below is 159 or 175,
 * depending on horizontal size of the picture:
 *
 * <--- 160 or 176 pairs of RA,RB bytes ----->
 * *-----------------------------------------* \
 * | RA0 | RB0 | RA1 | RB1 | ... | RAx | RBx |  \   This is pair of horizontal lines,
 * |-----+-----+-----+-----+ ... +-----+-----|   *- or one interlaced line, total
 * | B0  | G0  | B1  | G1  | ... | Bx  | Gx  |  /   120 or 144 such pairs which yield
 * |=====+=====+=====+=====+ ... +=====+=====| /    240 or 288 lines after deinterlacing.
 *
 * Each group of FOUR bytes (RAi, RBi, Bi, Gi) where i=0..frame_width/2-1
 * defines ONE pixel. Therefore this format yields 176x144 "decoded"
 * resolution at best. I do not know why camera sends such format - the
 * previous model (1) just used interlaced I420 and everyone was happy.
 *
 * I do not know what is the difference between RAi and RBi bytes. Both
 * seemingly represent R component, but slightly vary in value (so that
 * the picture looks a bit colored if one or another is used). I use
 * them both as R component in attempt to at least partially recover the
 * lost resolution.
 */
static enum ParseState ibmcam_model2_320x240_parse_lines(
	struct uvd *uvd,
	struct usbvideo_frame *frame,
	long *pcopylen)
{
	unsigned char *f, *la, *lb;
	unsigned int len;
	int v4l_linesize; /* V4L line offset */
	int i, j, frame_done=0, color_corr;
	int scanLength, scanHeight;
	static unsigned char lineBuffer[352*2];

	switch (uvd->videosize) {
	case VIDEOSIZE_320x240:
	case VIDEOSIZE_352x240:
	case VIDEOSIZE_352x288:
		scanLength = VIDEOSIZE_X(uvd->videosize);
		scanHeight = VIDEOSIZE_Y(uvd->videosize);
		break;
	default:
		err("ibmcam_model2_320x240_parse_lines: Wrong mode.");
		return scan_Out;
	}

	color_corr = (uvd->vpic.colour) >> 8; /* 0..+255 */
	v4l_linesize = VIDEOSIZE_X(frame->request) * V4L_BYTES_PER_PIXEL;

	len = scanLength * 2; /* See explanation above */
	assert(len <= sizeof(lineBuffer));

	/* Make sure there's enough data for the entire line */
	if (RingQueue_GetLength(&uvd->dp) < len)
		return scan_Out;

	/* Suck one line out of the ring queue */
	RingQueue_Dequeue(&uvd->dp, lineBuffer, len);

	/*
	 * Make sure that our writing into output buffer
	 * will not exceed the buffer. Mind that we may write
	 * not into current output scanline but in several after
	 * it as well (if we enlarge image vertically.)
	 */
	if ((frame->curline + 2) >= VIDEOSIZE_Y(frame->request))
		return scan_NextFrame;

	la = lineBuffer;
	lb = lineBuffer + scanLength;

	/*
	 * Now we are sure that entire line (representing all
	 *         VIDEOSIZE_X(frame->request)
	 * pixels from the camera) is available in the scratch buffer. We
	 * start copying the line left-aligned to the V4L buffer (which
	 * might be larger - not smaller, hopefully). If the camera
	 * line is shorter then we should pad the V4L buffer with something
	 * (black in this case) to complete the line.
	 */
	f = frame->data + (v4l_linesize * frame->curline);

	/* Fill the 2-line strip */
	for (i = 0; i < VIDEOSIZE_X(frame->request); i++) {
		int y, rv, gv, bv;	/* RGB components */

		j = i & (~1);

		/* Check for various visual debugging hints (colorized pixels) */
		if ((flags & FLAGS_DISPLAY_HINTS) && (IBMCAM_T(uvd)->has_hdr)) {
			if (IBMCAM_T(uvd)->has_hdr == 1) {
				bv = 0; /* Yellow marker */
				gv = 0xFF;
				rv = 0xFF;
			} else {
				bv = 0xFF; /* Cyan marker */
				gv = 0xFF;
				rv = 0;
			}
			IBMCAM_T(uvd)->has_hdr = 0;
			goto make_pixel;
		}

		/*
		 * Check if we are still in range. We may be out of range if our
		 * V4L canvas is wider or taller than the camera "native" image.
		 * Then we quickly fill the remainder of the line with zeros to
		 * make black color and quit the horizontal scanning loop.
		 */
		if (((frame->curline + 2) >= scanHeight) || (i >= scanLength)) {
			const int offset = i * V4L_BYTES_PER_PIXEL;
#if USES_IBMCAM_PUTPIXEL
			/* Refresh 'f' because we don't use it much with PUTPIXEL */
			f = frame->data + (v4l_linesize * frame->curline) + offset;
#endif
			memset(f, 0, v4l_linesize - offset);
			break;
		}

		/*
		 * Here I use RA and RB components, one per physical pixel.
		 * This causes fine vertical grid on the picture but may improve
		 * horizontal resolution. If you prefer replicating, use this:
		 *   rv = la[j + 0];   ... or ... rv = la[j + 1];
		 * then the pixel will be replicated.
		 */
		rv = la[i];
		gv = lb[j + 1];
		bv = lb[j + 0];

		y = (rv + gv + bv) / 3; /* Brightness (badly calculated) */

		if (flags & FLAGS_MONOCHROME) /* Use monochrome for debugging */
			rv = gv = bv = y;
		else if (color_corr != 128) {

			/* Calculate difference between color and brightness */
			rv -= y;
			gv -= y;
			bv -= y;

			/* Scale differences */
			rv = (rv * color_corr) / 128;
			gv = (gv * color_corr) / 128;
			bv = (bv * color_corr) / 128;

			/* Reapply brightness */
			rv += y;
			gv += y;
			bv += y;

			/* Watch for overflows */
			RESTRICT_TO_RANGE(rv, 0, 255);
			RESTRICT_TO_RANGE(gv, 0, 255);
			RESTRICT_TO_RANGE(bv, 0, 255);
		}

	make_pixel:
		RGB24_PUTPIXEL(frame, i, frame->curline, rv, gv, bv);
	}
	/*
	 * Account for number of bytes that we wrote into output V4L frame.
	 * We do it here, after we are done with the scanline, because we
	 * may fill more than one output scanline if we do vertical
	 * enlargement.
	 */
	frame->curline += 2;
	*pcopylen += v4l_linesize * 2;
	frame->deinterlace = Deinterlace_FillOddLines;

	if (frame_done || (frame->curline >= VIDEOSIZE_Y(frame->request)))
		return scan_NextFrame;
	else
		return scan_Continue;
}

/*
 * ibmcam_model3_parse_lines()
 *
 * | Even lines |     Odd Lines       |
 * -----------------------------------|
 * |YYY........Y|UYVYUYVY.........UYVY|
 * |YYY........Y|UYVYUYVY.........UYVY|
 * |............|.....................|
 * |YYY........Y|UYVYUYVY.........UYVY|
 * |------------+---------------------|
 *
 * There is one (U, V) chroma pair for every four luma (Y) values.  This
 * function reads a pair of lines at a time and obtains missing chroma values
 * from adjacent pixels.
 */
static enum ParseState ibmcam_model3_parse_lines(
	struct uvd *uvd,
	struct usbvideo_frame *frame,
	long *pcopylen)
{
	unsigned char *data;
	const unsigned char *color;
	unsigned int len;
	int v4l_linesize; /* V4L line offset */
	const int hue_corr  = (uvd->vpic.hue - 0x8000) >> 10;	/* -32..+31 */
	const int hue2_corr = (hue_correction - 128) / 4;		/* -32..+31 */
	const int ccm = 128; /* Color correction median - see below */
	int i, u, v, rw, data_w=0, data_h=0, color_corr;
	static unsigned char lineBuffer[640*3];
	int line;

	color_corr = (uvd->vpic.colour - 0x8000) >> 8; /* -128..+127 = -ccm..+(ccm-1)*/
	RESTRICT_TO_RANGE(color_corr, -ccm, ccm+1);

	v4l_linesize = VIDEOSIZE_X(frame->request) * V4L_BYTES_PER_PIXEL;

	/* The header tells us what sort of data is in this frame */
	switch (frame->header) {
		/*
		 * Uncompressed modes (that are easy to decode).
		 */
	case 0x0308:
		data_w = 640;
		data_h = 480;
		break;
	case 0x0208:
		data_w = 320;
		data_h = 240;
		break;
	case 0x020A:
		data_w = 160;
		data_h = 120;
		break;
		/*
		 * Compressed modes (ViCE - that I don't know how to decode).
		 */
	case 0x0328:	/* 640x480, best quality compression */
	case 0x0368:	/* 640x480, best frame rate compression */
	case 0x0228:	/* 320x240, best quality compression */
	case 0x0268:	/* 320x240, best frame rate compression */
	case 0x02CA:	/* 160x120, best quality compression */
	case 0x02EA:	/* 160x120, best frame rate compression */
		/* Do nothing with this - not supported */
		err("Unsupported mode $%04lx", frame->header);
		return scan_NextFrame;
	default:
		/* Catch unknown headers, may help in learning new headers */
		err("Strange frame->header=$%08lx", frame->header);
		return scan_NextFrame;
	}

	/*
	 * Make sure that our writing into output buffer
	 * will not exceed the buffer. Note that we may write
	 * not into current output scanline but in several after
	 * it as well (if we enlarge image vertically.)
	 */
	if ((frame->curline + 1) >= data_h) {
		if (uvd->debug >= 3)
			dev_info(&uvd->dev->dev,
				 "Reached line %d. (frame is done)\n",
				 frame->curline);
		return scan_NextFrame;
	}

	/* Make sure that lineBuffer can store two lines of data */
	len = 3 * data_w; /* <y-data> <uyvy-data> */
	assert(len <= sizeof(lineBuffer));

	/* Make sure there's enough data for two lines */
	if (RingQueue_GetLength(&uvd->dp) < len)
		return scan_Out;

	/* Suck two lines of data out of the ring queue */
	RingQueue_Dequeue(&uvd->dp, lineBuffer, len);

	data = lineBuffer;
	color = data + data_w;		/* Point to where color planes begin */

	/* Bottom-to-top scanning */
	rw = (int)VIDEOSIZE_Y(frame->request) - (int)(frame->curline) - 1;
	RESTRICT_TO_RANGE(rw, 0, VIDEOSIZE_Y(frame->request)-1);

	/* Iterate over two lines. */
	for (line = 0; line < 2; line++) {
		for (i = 0; i < VIDEOSIZE_X(frame->request); i++) {
			int y;
			int rv, gv, bv;	/* RGB components */

			if (i >= data_w) {
				RGB24_PUTPIXEL(frame, i, rw, 0, 0, 0);
				continue;
			}

			/* first line is YYY...Y; second is UYVY...UYVY */
			y = data[(line == 0) ? i : (i*2 + 1)];

			/* Apply static color correction */
			u = color[(i/2)*4] + hue_corr;
			v = color[(i/2)*4 + 2] + hue2_corr;

			/* Apply color correction */
			if (color_corr != 0) {
				/* Magnify up to 2 times, reduce down to zero saturation */
				u = 128 + ((ccm + color_corr) * (u - 128)) / ccm;
				v = 128 + ((ccm + color_corr) * (v - 128)) / ccm;
			}


			YUV_TO_RGB_BY_THE_BOOK(y, u, v, rv, gv, bv);
			RGB24_PUTPIXEL(frame, i, rw, rv, gv, bv);  /* No deinterlacing */
		}

		/* Check for the end of requested data */
		if (rw == 0)
			break;

		/* Prepare for the second line */
		rw--;
		data = lineBuffer + data_w;
	}
	frame->deinterlace = Deinterlace_None;

	/*
	 * Account for number of bytes that we wrote into output V4L frame.
	 * We do it here, after we are done with the scanline, because we
	 * may fill more than one output scanline if we do vertical
	 * enlargement.
	 */
	frame->curline += 2;
	*pcopylen += 2 * v4l_linesize;

	if (frame->curline >= VIDEOSIZE_Y(frame->request)) {
		if (uvd->debug >= 3) {
			dev_info(&uvd->dev->dev,
				 "All requested lines (%ld.) done.\n",
				 VIDEOSIZE_Y(frame->request));
		}
		return scan_NextFrame;
	} else
		return scan_Continue;
}

/*
 * ibmcam_model4_128x96_parse_lines()
 *
 * This decoder is for one strange data format that is produced by Model 4
 * camera only in 128x96 mode. This is RGB format and here is its description.
 * First of all, this is non-interlaced stream, meaning that all scan lines
 * are present in the datastream. There are 96 consecutive blocks of data
 * that describe all 96 lines of the image. Each block is 5*128 bytes long
 * and carries R, G, B components. The format of the block is shown in the
 * code below. First 128*2 bytes are interleaved R and G components. Then
 * we have a gap (junk data) 64 bytes long. Then follow B and something
 * else, also interleaved (this makes another 128*2 bytes). After that
 * probably another 64 bytes of junk follow.
 *
 * History:
 * 10-Feb-2001 Created.
 */
static enum ParseState ibmcam_model4_128x96_parse_lines(
	struct uvd *uvd,
	struct usbvideo_frame *frame,
	long *pcopylen)
{
	const unsigned char *data_rv, *data_gv, *data_bv;
	unsigned int len;
	int i, v4l_linesize; /* V4L line offset */
	const int data_w=128, data_h=96;
	static unsigned char lineBuffer[128*5];

	v4l_linesize = VIDEOSIZE_X(frame->request) * V4L_BYTES_PER_PIXEL;

	/*
	 * Make sure that our writing into output buffer
	 * will not exceed the buffer. Note that we may write
	 * not into current output scanline but in several after
	 * it as well (if we enlarge image vertically.)
	 */
	if ((frame->curline + 1) >= data_h) {
		if (uvd->debug >= 3)
			dev_info(&uvd->dev->dev,
				 "Reached line %d. (frame is done)\n",
				 frame->curline);
		return scan_NextFrame;
	}

	/*
	 * RGRGRG .... RGRG_____________B?B?B? ... B?B?____________
	 * <---- 128*2 ---><---- 64 ---><--- 128*2 ---><--- 64 --->
	 */

	/* Make sure there's enough data for the entire line */
	len = 5 * data_w;
	assert(len <= sizeof(lineBuffer));

	/* Make sure there's enough data for the entire line */
	if (RingQueue_GetLength(&uvd->dp) < len)
		return scan_Out;

	/* Suck one line out of the ring queue */
	RingQueue_Dequeue(&uvd->dp, lineBuffer, len);

	data_rv = lineBuffer;
	data_gv = lineBuffer + 1;
	data_bv = lineBuffer + data_w*2 + data_w/2;
	for (i = 0; i < VIDEOSIZE_X(frame->request); i++) {
		int rv, gv, bv;	/* RGB components */
		if (i < data_w) {
			const int j = i * 2;
			gv = data_rv[j];
			rv = data_gv[j];
			bv = data_bv[j];
			if (flags & FLAGS_MONOCHROME) {
				unsigned long y;
				y = rv + gv + bv;
				y /= 3;
				if (y > 0xFF)
					y = 0xFF;
				rv = gv = bv = (unsigned char) y;
			}
		} else {
			rv = gv = bv = 0;
		}
		RGB24_PUTPIXEL(frame, i, frame->curline, rv, gv, bv);
	}
	frame->deinterlace = Deinterlace_None;
	frame->curline++;
	*pcopylen += v4l_linesize;

	if (frame->curline >= VIDEOSIZE_Y(frame->request)) {
		if (uvd->debug >= 3) {
			dev_info(&uvd->dev->dev,
				 "All requested lines (%ld.) done.\n",
				 VIDEOSIZE_Y(frame->request));
		}
		return scan_NextFrame;
	} else
		return scan_Continue;
}

/*
 * ibmcam_ProcessIsocData()
 *
 * Generic routine to parse the ring queue data. It employs either
 * ibmcam_find_header() or ibmcam_parse_lines() to do most
 * of work.
 *
 * History:
 * 1/21/00  Created.
 */
static void ibmcam_ProcessIsocData(struct uvd *uvd,
				   struct usbvideo_frame *frame)
{
	enum ParseState newstate;
	long copylen = 0;
	int mod = IBMCAM_T(uvd)->camera_model;

	while (1) {
		newstate = scan_Out;
		if (RingQueue_GetLength(&uvd->dp) > 0) {
			if (frame->scanstate == ScanState_Scanning) {
				newstate = ibmcam_find_header(uvd);
			} else if (frame->scanstate == ScanState_Lines) {
				if ((mod == IBMCAM_MODEL_2) &&
				    ((uvd->videosize == VIDEOSIZE_352x288) ||
				     (uvd->videosize == VIDEOSIZE_320x240) ||
				     (uvd->videosize == VIDEOSIZE_352x240)))
				{
					newstate = ibmcam_model2_320x240_parse_lines(
						uvd, frame, &copylen);
				} else if (mod == IBMCAM_MODEL_4) {
					/*
					 * Model 4 cameras (IBM NetCamera) use Model 2 decoder (RGB)
					 * for 320x240 and above; 160x120 and 176x144 uses Model 1
					 * decoder (YUV), and 128x96 mode uses ???
					 */
					if ((uvd->videosize == VIDEOSIZE_352x288) ||
					    (uvd->videosize == VIDEOSIZE_320x240) ||
					    (uvd->videosize == VIDEOSIZE_352x240))
					{
						newstate = ibmcam_model2_320x240_parse_lines(uvd, frame, &copylen);
					} else if (uvd->videosize == VIDEOSIZE_128x96) {
						newstate = ibmcam_model4_128x96_parse_lines(uvd, frame, &copylen);
					} else {
						newstate = ibmcam_parse_lines(uvd, frame, &copylen);
					}
				} else if (mod == IBMCAM_MODEL_3) {
					newstate = ibmcam_model3_parse_lines(uvd, frame, &copylen);
				} else {
					newstate = ibmcam_parse_lines(uvd, frame, &copylen);
				}
			}
		}
		if (newstate == scan_Continue)
			continue;
		else if ((newstate == scan_NextFrame) || (newstate == scan_Out))
			break;
		else
			return; /* scan_EndParse */
	}

	if (newstate == scan_NextFrame) {
		frame->frameState = FrameState_Done;
		uvd->curframe = -1;
		uvd->stats.frame_num++;
		if ((mod == IBMCAM_MODEL_2) || (mod == IBMCAM_MODEL_4)) {
			/* Need software contrast adjustment for those cameras */
			frame->flags |= USBVIDEO_FRAME_FLAG_SOFTWARE_CONTRAST;
		}
	}

	/* Update the frame's uncompressed length. */
	frame->seqRead_Length += copylen;

#if 0
	{
		static unsigned char j=0;
		memset(frame->data, j++, uvd->max_frame_size);
		frame->frameState = FrameState_Ready;
	}
#endif
}

/*
 * ibmcam_veio()
 *
 * History:
 * 1/27/00  Added check for dev == NULL; this happens if camera is unplugged.
 */
static int ibmcam_veio(
	struct uvd *uvd,
	unsigned char req,
	unsigned short value,
	unsigned short index)
{
	static const char proc[] = "ibmcam_veio";
	unsigned char cp[8] /* = { 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef } */;
	int i;

	if (!CAMERA_IS_OPERATIONAL(uvd))
		return 0;

	if (req == 1) {
		i = usb_control_msg(
			uvd->dev,
			usb_rcvctrlpipe(uvd->dev, 0),
			req,
			USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_ENDPOINT,
			value,
			index,
			cp,
			sizeof(cp),
			1000);
#if 0
		dev_info(&uvd->dev->dev,
			 "USB => %02x%02x%02x%02x%02x%02x%02x%02x "
			 "(req=$%02x val=$%04x ind=$%04x)\n",
			 cp[0],cp[1],cp[2],cp[3],cp[4],cp[5],cp[6],cp[7],
			 req, value, index);
#endif
	} else {
		i = usb_control_msg(
			uvd->dev,
			usb_sndctrlpipe(uvd->dev, 0),
			req,
			USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_ENDPOINT,
			value,
			index,
			NULL,
			0,
			1000);
	}
	if (i < 0) {
		err("%s: ERROR=%d. Camera stopped; Reconnect or reload driver.",
		    proc, i);
		uvd->last_error = i;
	}
	return i;
}

/*
 * ibmcam_calculate_fps()
 *
 * This procedure roughly calculates the real frame rate based
 * on FPS code (framerate=NNN option). Actual FPS differs
 * slightly depending on lighting conditions, so that actual frame
 * rate is determined by the camera. Since I don't know how to ask
 * the camera what FPS is now I have to use the FPS code instead.
 *
 * The FPS code is in range [0..6], 0 is slowest, 6 is fastest.
 * Corresponding real FPS should be in range [3..30] frames per second.
 * The conversion formula is obvious:
 *
 * real_fps = 3 + (fps_code * 4.5)
 *
 * History:
 * 1/18/00  Created.
 */
static int ibmcam_calculate_fps(struct uvd *uvd)
{
	return 3 + framerate*4 + framerate/2;
}

/*
 * ibmcam_send_FF_04_02()
 *
 * This procedure sends magic 3-command prefix to the camera.
 * The purpose of this prefix is not known.
 *
 * History:
 * 1/2/00   Created.
 */
static void ibmcam_send_FF_04_02(struct uvd *uvd)
{
	ibmcam_veio(uvd, 0, 0x00FF, 0x0127);
	ibmcam_veio(uvd, 0, 0x0004, 0x0124);
	ibmcam_veio(uvd, 0, 0x0002, 0x0124);
}

static void ibmcam_send_00_04_06(struct uvd *uvd)
{
	ibmcam_veio(uvd, 0, 0x0000, 0x0127);
	ibmcam_veio(uvd, 0, 0x0004, 0x0124);
	ibmcam_veio(uvd, 0, 0x0006, 0x0124);
}

static void ibmcam_send_x_00(struct uvd *uvd, unsigned short x)
{
	ibmcam_veio(uvd, 0, x,      0x0127);
	ibmcam_veio(uvd, 0, 0x0000, 0x0124);
}

static void ibmcam_send_x_00_05(struct uvd *uvd, unsigned short x)
{
	ibmcam_send_x_00(uvd, x);
	ibmcam_veio(uvd, 0, 0x0005, 0x0124);
}

static void ibmcam_send_x_00_05_02(struct uvd *uvd, unsigned short x)
{
	ibmcam_veio(uvd, 0, x,      0x0127);
	ibmcam_veio(uvd, 0, 0x0000, 0x0124);
	ibmcam_veio(uvd, 0, 0x0005, 0x0124);
	ibmcam_veio(uvd, 0, 0x0002, 0x0124);
}

static void ibmcam_send_x_01_00_05(struct uvd *uvd, unsigned short x)
{
	ibmcam_veio(uvd, 0, x,      0x0127);
	ibmcam_veio(uvd, 0, 0x0001, 0x0124);
	ibmcam_veio(uvd, 0, 0x0000, 0x0124);
	ibmcam_veio(uvd, 0, 0x0005, 0x0124);
}

static void ibmcam_send_x_00_05_02_01(struct uvd *uvd, unsigned short x)
{
	ibmcam_veio(uvd, 0, x,      0x0127);
	ibmcam_veio(uvd, 0, 0x0000, 0x0124);
	ibmcam_veio(uvd, 0, 0x0005, 0x0124);
	ibmcam_veio(uvd, 0, 0x0002, 0x0124);
	ibmcam_veio(uvd, 0, 0x0001, 0x0124);
}

static void ibmcam_send_x_00_05_02_08_01(struct uvd *uvd, unsigned short x)
{
	ibmcam_veio(uvd, 0, x,      0x0127);
	ibmcam_veio(uvd, 0, 0x0000, 0x0124);
	ibmcam_veio(uvd, 0, 0x0005, 0x0124);
	ibmcam_veio(uvd, 0, 0x0002, 0x0124);
	ibmcam_veio(uvd, 0, 0x0008, 0x0124);
	ibmcam_veio(uvd, 0, 0x0001, 0x0124);
}

static void ibmcam_Packet_Format1(struct uvd *uvd, unsigned char fkey, unsigned char val)
{
	ibmcam_send_x_01_00_05(uvd, unknown_88);
	ibmcam_send_x_00_05(uvd, fkey);
	ibmcam_send_x_00_05_02_08_01(uvd, val);
	ibmcam_send_x_00_05(uvd, unknown_88);
	ibmcam_send_x_00_05_02_01(uvd, fkey);
	ibmcam_send_x_00_05(uvd, unknown_89);
	ibmcam_send_x_00(uvd, fkey);
	ibmcam_send_00_04_06(uvd);
	ibmcam_veio(uvd, 1, 0x0000, 0x0126);
	ibmcam_send_FF_04_02(uvd);
}

static void ibmcam_PacketFormat2(struct uvd *uvd, unsigned char fkey, unsigned char val)
{
	ibmcam_send_x_01_00_05	(uvd, unknown_88);
	ibmcam_send_x_00_05	(uvd, fkey);
	ibmcam_send_x_00_05_02	(uvd, val);
}

static void ibmcam_model2_Packet2(struct uvd *uvd)
{
	ibmcam_veio(uvd, 0, 0x00ff, 0x012d);
	ibmcam_veio(uvd, 0, 0xfea3, 0x0124);
}

static void ibmcam_model2_Packet1(struct uvd *uvd, unsigned short v1, unsigned short v2)
{
	ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
	ibmcam_veio(uvd, 0, 0x00ff, 0x012e);
	ibmcam_veio(uvd, 0, v1,     0x012f);
	ibmcam_veio(uvd, 0, 0x00ff, 0x0130);
	ibmcam_veio(uvd, 0, 0xc719, 0x0124);
	ibmcam_veio(uvd, 0, v2,     0x0127);

	ibmcam_model2_Packet2(uvd);
}

/*
 * ibmcam_model3_Packet1()
 *
 * 00_0078_012d
 * 00_0097_012f
 * 00_d141_0124
 * 00_0096_0127
 * 00_fea8_0124
*/
static void ibmcam_model3_Packet1(struct uvd *uvd, unsigned short v1, unsigned short v2)
{
	ibmcam_veio(uvd, 0, 0x0078, 0x012d);
	ibmcam_veio(uvd, 0, v1,     0x012f);
	ibmcam_veio(uvd, 0, 0xd141, 0x0124);
	ibmcam_veio(uvd, 0, v2,     0x0127);
	ibmcam_veio(uvd, 0, 0xfea8, 0x0124);
}

static void ibmcam_model4_BrightnessPacket(struct uvd *uvd, int i)
{
	ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
	ibmcam_veio(uvd, 0, 0x0026, 0x012f);
	ibmcam_veio(uvd, 0, 0xd141, 0x0124);
	ibmcam_veio(uvd, 0, i,      0x0127);
	ibmcam_veio(uvd, 0, 0x00aa, 0x0130);
	ibmcam_veio(uvd, 0, 0x82a8, 0x0124);
	ibmcam_veio(uvd, 0, 0x0038, 0x012d);
	ibmcam_veio(uvd, 0, 0x0004, 0x012f);
	ibmcam_veio(uvd, 0, 0xd145, 0x0124);
	ibmcam_veio(uvd, 0, 0xfffa, 0x0124);
}

/*
 * ibmcam_adjust_contrast()
 *
 * The contrast value changes from 0 (high contrast) to 15 (low contrast).
 * This is in reverse to usual order of things (such as TV controls), so
 * we reverse it again here.
 *
 * TODO: we probably don't need to send the setup 5 times...
 *
 * History:
 * 1/2/00   Created.
 */
static void ibmcam_adjust_contrast(struct uvd *uvd)
{
	unsigned char a_contrast = uvd->vpic.contrast >> 12;
	unsigned char new_contrast;

	if (a_contrast >= 16)
		a_contrast = 15;
	new_contrast = 15 - a_contrast;
	if (new_contrast == uvd->vpic_old.contrast)
		return;
	uvd->vpic_old.contrast = new_contrast;
	switch (IBMCAM_T(uvd)->camera_model) {
	case IBMCAM_MODEL_1:
	{
		const int ntries = 5;
		int i;
		for (i=0; i < ntries; i++) {
			ibmcam_Packet_Format1(uvd, contrast_14, new_contrast);
			ibmcam_send_FF_04_02(uvd);
		}
		break;
	}
	case IBMCAM_MODEL_2:
	case IBMCAM_MODEL_4:
		/* Models 2, 4 do not have this control; implemented in software. */
		break;
	case IBMCAM_MODEL_3:
	{	/* Preset hardware values */
		static const struct {
			unsigned short cv1;
			unsigned short cv2;
			unsigned short cv3;
		} cv[7] = {
			{ 0x05, 0x05, 0x0f },	/* Minimum */
			{ 0x04, 0x04, 0x16 },
			{ 0x02, 0x03, 0x16 },
			{ 0x02, 0x08, 0x16 },
			{ 0x01, 0x0c, 0x16 },
			{ 0x01, 0x0e, 0x16 },
			{ 0x01, 0x10, 0x16 }	/* Maximum */
		};
		int i = a_contrast / 2;
		RESTRICT_TO_RANGE(i, 0, 6);
		ibmcam_veio(uvd, 0, 0x0000, 0x010c);	/* Stop */
		ibmcam_model3_Packet1(uvd, 0x0067, cv[i].cv1);
		ibmcam_model3_Packet1(uvd, 0x005b, cv[i].cv2);
		ibmcam_model3_Packet1(uvd, 0x005c, cv[i].cv3);
		ibmcam_veio(uvd, 0, 0x0001, 0x0114);
		ibmcam_veio(uvd, 0, 0x00c0, 0x010c);	/* Go! */
		usb_clear_halt(uvd->dev, usb_rcvisocpipe(uvd->dev, uvd->video_endp));
		break;
	}
	default:
		break;
	}
}

/*
 * ibmcam_change_lighting_conditions()
 *
 * Camera model 1:
 * We have 3 levels of lighting conditions: 0=Bright, 1=Medium, 2=Low.
 *
 * Camera model 2:
 * We have 16 levels of lighting, 0 for bright light and up to 15 for
 * low light. But values above 5 or so are useless because camera is
 * not really capable to produce anything worth viewing at such light.
 * This setting may be altered only in certain camera state.
 *
 * Low lighting forces slower FPS. Lighting is set as a module parameter.
 *
 * History:
 * 1/5/00   Created.
 * 2/20/00  Added support for Model 2 cameras.
 */
static void ibmcam_change_lighting_conditions(struct uvd *uvd)
{
	if (debug > 0)
		dev_info(&uvd->dev->dev,
			 "%s: Set lighting to %hu.\n", __func__, lighting);

	switch (IBMCAM_T(uvd)->camera_model) {
	case IBMCAM_MODEL_1:
	{
		const int ntries = 5;
		int i;
		for (i=0; i < ntries; i++)
			ibmcam_Packet_Format1(uvd, light_27, (unsigned short) lighting);
		break;
	}
	case IBMCAM_MODEL_2:
#if 0
		/*
		 * This command apparently requires camera to be stopped. My
		 * experiments showed that it -is- possible to alter the lighting
		 * conditions setting "on the fly", but why bother? This setting does
		 * not work reliably in all cases, so I decided simply to leave the
		 * setting where Xirlink put it - in the camera setup phase. This code
		 * is commented out because it does not work at -any- moment, so its
		 * presence makes no sense. You may use it for experiments.
		 */
		ibmcam_veio(uvd, 0, 0x0000, 0x010c);	/* Stop camera */
		ibmcam_model2_Packet1(uvd, mod2_sensitivity, lighting);
		ibmcam_veio(uvd, 0, 0x00c0, 0x010c);	/* Start camera */
#endif
		break;
	case IBMCAM_MODEL_3:
	case IBMCAM_MODEL_4:
	default:
		break;
	}
}

/*
 * ibmcam_set_sharpness()
 *
 * Cameras model 1 have internal smoothing feature. It is controlled by value in
 * range [0..6], where 0 is most smooth and 6 is most sharp (raw image, I guess).
 * Recommended value is 4. Cameras model 2 do not have this feature at all.
 */
static void ibmcam_set_sharpness(struct uvd *uvd)
{
	switch (IBMCAM_T(uvd)->camera_model) {
	case IBMCAM_MODEL_1:
	{
		static const unsigned short sa[] = { 0x11, 0x13, 0x16, 0x18, 0x1a, 0x8, 0x0a };
		unsigned short i, sv;

		RESTRICT_TO_RANGE(sharpness, SHARPNESS_MIN, SHARPNESS_MAX);
		if (debug > 0)
			dev_info(&uvd->dev->dev, "%s: Set sharpness to %hu.\n",
				 __func__, sharpness);

		sv = sa[sharpness - SHARPNESS_MIN];
		for (i=0; i < 2; i++) {
			ibmcam_send_x_01_00_05	(uvd, unknown_88);
			ibmcam_send_x_00_05		(uvd, sharp_13);
			ibmcam_send_x_00_05_02	(uvd, sv);
		}
		break;
	}
	case IBMCAM_MODEL_2:
	case IBMCAM_MODEL_4:
		/* Models 2, 4 do not have this control */
		break;
	case IBMCAM_MODEL_3:
	{	/*
		 * "Use a table of magic numbers.
		 *  This setting doesn't really change much.
		 *  But that's how Windows does it."
		 */
		static const struct {
			unsigned short sv1;
			unsigned short sv2;
			unsigned short sv3;
			unsigned short sv4;
		} sv[7] = {
			{ 0x00, 0x00, 0x05, 0x14 },	/* Smoothest */
			{ 0x01, 0x04, 0x05, 0x14 },
			{ 0x02, 0x04, 0x05, 0x14 },
			{ 0x03, 0x04, 0x05, 0x14 },
			{ 0x03, 0x05, 0x05, 0x14 },
			{ 0x03, 0x06, 0x05, 0x14 },
			{ 0x03, 0x07, 0x05, 0x14 }	/* Sharpest */
		};
		RESTRICT_TO_RANGE(sharpness, SHARPNESS_MIN, SHARPNESS_MAX);
		RESTRICT_TO_RANGE(sharpness, 0, 6);
		ibmcam_veio(uvd, 0, 0x0000, 0x010c);	/* Stop */
		ibmcam_model3_Packet1(uvd, 0x0060, sv[sharpness].sv1);
		ibmcam_model3_Packet1(uvd, 0x0061, sv[sharpness].sv2);
		ibmcam_model3_Packet1(uvd, 0x0062, sv[sharpness].sv3);
		ibmcam_model3_Packet1(uvd, 0x0063, sv[sharpness].sv4);
		ibmcam_veio(uvd, 0, 0x0001, 0x0114);
		ibmcam_veio(uvd, 0, 0x00c0, 0x010c);	/* Go! */
		usb_clear_halt(uvd->dev, usb_rcvisocpipe(uvd->dev, uvd->video_endp));
		ibmcam_veio(uvd, 0, 0x0001, 0x0113);
		break;
	}
	default:
		break;
	}
}

/*
 * ibmcam_set_brightness()
 *
 * This procedure changes brightness of the picture.
 */
static void ibmcam_set_brightness(struct uvd *uvd)
{
	static const unsigned short n = 1;

	if (debug > 0)
		dev_info(&uvd->dev->dev, "%s: Set brightness to %hu.\n",
			 __func__, uvd->vpic.brightness);

	switch (IBMCAM_T(uvd)->camera_model) {
	case IBMCAM_MODEL_1:
	{
		unsigned short i, j, bv[3];
		bv[0] = bv[1] = bv[2] = uvd->vpic.brightness >> 10;
		if (bv[0] == (uvd->vpic_old.brightness >> 10))
			return;
		uvd->vpic_old.brightness = bv[0];
		for (j=0; j < 3; j++)
			for (i=0; i < n; i++)
				ibmcam_Packet_Format1(uvd, bright_3x[j], bv[j]);
		break;
	}
	case IBMCAM_MODEL_2:
	{
		unsigned short i, j;
		i = uvd->vpic.brightness >> 12;	/* 0 .. 15 */
		j = 0x60 + i * ((0xee - 0x60) / 16);	/* 0x60 .. 0xee or so */
		if (uvd->vpic_old.brightness == j)
			break;
		uvd->vpic_old.brightness = j;
		ibmcam_model2_Packet1(uvd, mod2_brightness, j);
		break;
	}
	case IBMCAM_MODEL_3:
	{
		/* Model 3: Brightness range 'i' in [0x0C..0x3F] */
		unsigned short i =
			0x0C + (uvd->vpic.brightness / (0xFFFF / (0x3F - 0x0C + 1)));
		RESTRICT_TO_RANGE(i, 0x0C, 0x3F);
		if (uvd->vpic_old.brightness == i)
			break;
		uvd->vpic_old.brightness = i;
		ibmcam_veio(uvd, 0, 0x0000, 0x010c);	/* Stop */
		ibmcam_model3_Packet1(uvd, 0x0036, i);
		ibmcam_veio(uvd, 0, 0x0001, 0x0114);
		ibmcam_veio(uvd, 0, 0x00c0, 0x010c);	/* Go! */
		usb_clear_halt(uvd->dev, usb_rcvisocpipe(uvd->dev, uvd->video_endp));
		ibmcam_veio(uvd, 0, 0x0001, 0x0113);
		break;
	}
	case IBMCAM_MODEL_4:
	{
		/* Model 4: Brightness range 'i' in [0x04..0xb4] */
		unsigned short i = 0x04 + (uvd->vpic.brightness / (0xFFFF / (0xb4 - 0x04 + 1)));
		RESTRICT_TO_RANGE(i, 0x04, 0xb4);
		if (uvd->vpic_old.brightness == i)
			break;
		uvd->vpic_old.brightness = i;
		ibmcam_model4_BrightnessPacket(uvd, i);
		break;
	}
	default:
		break;
	}
}

static void ibmcam_set_hue(struct uvd *uvd)
{
	switch (IBMCAM_T(uvd)->camera_model) {
	case IBMCAM_MODEL_2:
	{
		unsigned short hue = uvd->vpic.hue >> 9; /* 0 .. 7F */
		if (uvd->vpic_old.hue == hue)
			return;
		uvd->vpic_old.hue = hue;
		ibmcam_model2_Packet1(uvd, mod2_hue, hue);
		/* ibmcam_model2_Packet1(uvd, mod2_saturation, sat); */
		break;
	}
	case IBMCAM_MODEL_3:
	{
#if 0 /* This seems not to work. No problem, will fix programmatically */
		unsigned short hue = 0x05 + (uvd->vpic.hue / (0xFFFF / (0x37 - 0x05 + 1)));
		RESTRICT_TO_RANGE(hue, 0x05, 0x37);
		if (uvd->vpic_old.hue == hue)
			return;
		uvd->vpic_old.hue = hue;
		ibmcam_veio(uvd, 0, 0x0000, 0x010c);	/* Stop */
		ibmcam_model3_Packet1(uvd, 0x007e, hue);
		ibmcam_veio(uvd, 0, 0x0001, 0x0114);
		ibmcam_veio(uvd, 0, 0x00c0, 0x010c);	/* Go! */
		usb_clear_halt(uvd->dev, usb_rcvisocpipe(uvd->dev, uvd->video_endp));
		ibmcam_veio(uvd, 0, 0x0001, 0x0113);
#endif
		break;
	}
	case IBMCAM_MODEL_4:
	{
		unsigned short r_gain, g_gain, b_gain, hue;

		/*
		 * I am not sure r/g/b_gain variables exactly control gain
		 * of those channels. Most likely they subtly change some
		 * very internal image processing settings in the camera.
		 * In any case, here is what they do, and feel free to tweak:
		 *
		 * r_gain: seriously affects red gain
		 * g_gain: seriously affects green gain
		 * b_gain: seriously affects blue gain
		 * hue: changes average color from violet (0) to red (0xFF)
		 *
		 * These settings are preset for a decent white balance in
		 * 320x240, 352x288 modes. Low-res modes exhibit higher contrast
		 * and therefore may need different values here.
		 */
		hue = 20 + (uvd->vpic.hue >> 9);
		switch (uvd->videosize) {
		case VIDEOSIZE_128x96:
			r_gain = 90;
			g_gain = 166;
			b_gain = 175;
			break;
		case VIDEOSIZE_160x120:
			r_gain = 70;
			g_gain = 166;
			b_gain = 185;
			break;
		case VIDEOSIZE_176x144:
			r_gain = 160;
			g_gain = 175;
			b_gain = 185;
			break;
		default:
			r_gain = 120;
			g_gain = 166;
			b_gain = 175;
			break;
		}
		RESTRICT_TO_RANGE(hue, 1, 0x7f);

		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x001e, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, g_gain, 0x0127);	/* Green gain */
		ibmcam_veio(uvd, 0, r_gain, 0x012e);	/* Red gain */
		ibmcam_veio(uvd, 0, b_gain, 0x0130);	/* Blue gain */
		ibmcam_veio(uvd, 0, 0x8a28, 0x0124);
		ibmcam_veio(uvd, 0, hue,    0x012d);	/* Hue */
		ibmcam_veio(uvd, 0, 0xf545, 0x0124);
		break;
	}
	default:
		break;
	}
}

/*
 * ibmcam_adjust_picture()
 *
 * This procedure gets called from V4L interface to update picture settings.
 * Here we change brightness and contrast.
 */
static void ibmcam_adjust_picture(struct uvd *uvd)
{
	ibmcam_adjust_contrast(uvd);
	ibmcam_set_brightness(uvd);
	ibmcam_set_hue(uvd);
}

static int ibmcam_model1_setup(struct uvd *uvd)
{
	const int ntries = 5;
	int i;

	ibmcam_veio(uvd, 1, 0x00, 0x0128);
	ibmcam_veio(uvd, 1, 0x00, 0x0100);
	ibmcam_veio(uvd, 0, 0x01, 0x0100);	/* LED On  */
	ibmcam_veio(uvd, 1, 0x00, 0x0100);
	ibmcam_veio(uvd, 0, 0x81, 0x0100);	/* LED Off */
	ibmcam_veio(uvd, 1, 0x00, 0x0100);
	ibmcam_veio(uvd, 0, 0x01, 0x0100);	/* LED On  */
	ibmcam_veio(uvd, 0, 0x01, 0x0108);

	ibmcam_veio(uvd, 0, 0x03, 0x0112);
	ibmcam_veio(uvd, 1, 0x00, 0x0115);
	ibmcam_veio(uvd, 0, 0x06, 0x0115);
	ibmcam_veio(uvd, 1, 0x00, 0x0116);
	ibmcam_veio(uvd, 0, 0x44, 0x0116);
	ibmcam_veio(uvd, 1, 0x00, 0x0116);
	ibmcam_veio(uvd, 0, 0x40, 0x0116);
	ibmcam_veio(uvd, 1, 0x00, 0x0115);
	ibmcam_veio(uvd, 0, 0x0e, 0x0115);
	ibmcam_veio(uvd, 0, 0x19, 0x012c);

	ibmcam_Packet_Format1(uvd, 0x00, 0x1e);
	ibmcam_Packet_Format1(uvd, 0x39, 0x0d);
	ibmcam_Packet_Format1(uvd, 0x39, 0x09);
	ibmcam_Packet_Format1(uvd, 0x3b, 0x00);
	ibmcam_Packet_Format1(uvd, 0x28, 0x22);
	ibmcam_Packet_Format1(uvd, light_27, 0);
	ibmcam_Packet_Format1(uvd, 0x2b, 0x1f);
	ibmcam_Packet_Format1(uvd, 0x39, 0x08);

	for (i=0; i < ntries; i++)
		ibmcam_Packet_Format1(uvd, 0x2c, 0x00);

	for (i=0; i < ntries; i++)
		ibmcam_Packet_Format1(uvd, 0x30, 0x14);

	ibmcam_PacketFormat2(uvd, 0x39, 0x02);
	ibmcam_PacketFormat2(uvd, 0x01, 0xe1);
	ibmcam_PacketFormat2(uvd, 0x02, 0xcd);
	ibmcam_PacketFormat2(uvd, 0x03, 0xcd);
	ibmcam_PacketFormat2(uvd, 0x04, 0xfa);
	ibmcam_PacketFormat2(uvd, 0x3f, 0xff);
	ibmcam_PacketFormat2(uvd, 0x39, 0x00);

	ibmcam_PacketFormat2(uvd, 0x39, 0x02);
	ibmcam_PacketFormat2(uvd, 0x0a, 0x37);
	ibmcam_PacketFormat2(uvd, 0x0b, 0xb8);
	ibmcam_PacketFormat2(uvd, 0x0c, 0xf3);
	ibmcam_PacketFormat2(uvd, 0x0d, 0xe3);
	ibmcam_PacketFormat2(uvd, 0x0e, 0x0d);
	ibmcam_PacketFormat2(uvd, 0x0f, 0xf2);
	ibmcam_PacketFormat2(uvd, 0x10, 0xd5);
	ibmcam_PacketFormat2(uvd, 0x11, 0xba);
	ibmcam_PacketFormat2(uvd, 0x12, 0x53);
	ibmcam_PacketFormat2(uvd, 0x3f, 0xff);
	ibmcam_PacketFormat2(uvd, 0x39, 0x00);

	ibmcam_PacketFormat2(uvd, 0x39, 0x02);
	ibmcam_PacketFormat2(uvd, 0x16, 0x00);
	ibmcam_PacketFormat2(uvd, 0x17, 0x28);
	ibmcam_PacketFormat2(uvd, 0x18, 0x7d);
	ibmcam_PacketFormat2(uvd, 0x19, 0xbe);
	ibmcam_PacketFormat2(uvd, 0x3f, 0xff);
	ibmcam_PacketFormat2(uvd, 0x39, 0x00);

	for (i=0; i < ntries; i++)
		ibmcam_Packet_Format1(uvd, 0x00, 0x18);
	for (i=0; i < ntries; i++)
		ibmcam_Packet_Format1(uvd, 0x13, 0x18);
	for (i=0; i < ntries; i++)
		ibmcam_Packet_Format1(uvd, 0x14, 0x06);

	/* This is default brightness */
	for (i=0; i < ntries; i++)
		ibmcam_Packet_Format1(uvd, 0x31, 0x37);
	for (i=0; i < ntries; i++)
		ibmcam_Packet_Format1(uvd, 0x32, 0x46);
	for (i=0; i < ntries; i++)
		ibmcam_Packet_Format1(uvd, 0x33, 0x55);

	ibmcam_Packet_Format1(uvd, 0x2e, 0x04);
	for (i=0; i < ntries; i++)
		ibmcam_Packet_Format1(uvd, 0x2d, 0x04);
	for (i=0; i < ntries; i++)
		ibmcam_Packet_Format1(uvd, 0x29, 0x80);
	ibmcam_Packet_Format1(uvd, 0x2c, 0x01);
	ibmcam_Packet_Format1(uvd, 0x30, 0x17);
	ibmcam_Packet_Format1(uvd, 0x39, 0x08);
	for (i=0; i < ntries; i++)
		ibmcam_Packet_Format1(uvd, 0x34, 0x00);

	ibmcam_veio(uvd, 0, 0x00, 0x0101);
	ibmcam_veio(uvd, 0, 0x00, 0x010a);

	switch (uvd->videosize) {
	case VIDEOSIZE_128x96:
		ibmcam_veio(uvd, 0, 0x80, 0x0103);
		ibmcam_veio(uvd, 0, 0x60, 0x0105);
		ibmcam_veio(uvd, 0, 0x0c, 0x010b);
		ibmcam_veio(uvd, 0, 0x04, 0x011b);	/* Same everywhere */
		ibmcam_veio(uvd, 0, 0x0b, 0x011d);
		ibmcam_veio(uvd, 0, 0x00, 0x011e);	/* Same everywhere */
		ibmcam_veio(uvd, 0, 0x00, 0x0129);
		break;
	case VIDEOSIZE_176x144:
		ibmcam_veio(uvd, 0, 0xb0, 0x0103);
		ibmcam_veio(uvd, 0, 0x8f, 0x0105);
		ibmcam_veio(uvd, 0, 0x06, 0x010b);
		ibmcam_veio(uvd, 0, 0x04, 0x011b);	/* Same everywhere */
		ibmcam_veio(uvd, 0, 0x0d, 0x011d);
		ibmcam_veio(uvd, 0, 0x00, 0x011e);	/* Same everywhere */
		ibmcam_veio(uvd, 0, 0x03, 0x0129);
		break;
	case VIDEOSIZE_352x288:
		ibmcam_veio(uvd, 0, 0xb0, 0x0103);
		ibmcam_veio(uvd, 0, 0x90, 0x0105);
		ibmcam_veio(uvd, 0, 0x02, 0x010b);
		ibmcam_veio(uvd, 0, 0x04, 0x011b);	/* Same everywhere */
		ibmcam_veio(uvd, 0, 0x05, 0x011d);
		ibmcam_veio(uvd, 0, 0x00, 0x011e);	/* Same everywhere */
		ibmcam_veio(uvd, 0, 0x00, 0x0129);
		break;
	}

	ibmcam_veio(uvd, 0, 0xff, 0x012b);

	/* This is another brightness - don't know why */
	for (i=0; i < ntries; i++)
		ibmcam_Packet_Format1(uvd, 0x31, 0xc3);
	for (i=0; i < ntries; i++)
		ibmcam_Packet_Format1(uvd, 0x32, 0xd2);
	for (i=0; i < ntries; i++)
		ibmcam_Packet_Format1(uvd, 0x33, 0xe1);

	/* Default contrast */
	for (i=0; i < ntries; i++)
		ibmcam_Packet_Format1(uvd, contrast_14, 0x0a);

	/* Default sharpness */
	for (i=0; i < 2; i++)
		ibmcam_PacketFormat2(uvd, sharp_13, 0x1a);	/* Level 4 FIXME */

	/* Default lighting conditions */
	ibmcam_Packet_Format1(uvd, light_27, lighting); /* 0=Bright 2=Low */

	/* Assorted init */

	switch (uvd->videosize) {
	case VIDEOSIZE_128x96:
		ibmcam_Packet_Format1(uvd, 0x2b, 0x1e);
		ibmcam_veio(uvd, 0, 0xc9, 0x0119);	/* Same everywhere */
		ibmcam_veio(uvd, 0, 0x80, 0x0109);	/* Same everywhere */
		ibmcam_veio(uvd, 0, 0x36, 0x0102);
		ibmcam_veio(uvd, 0, 0x1a, 0x0104);
		ibmcam_veio(uvd, 0, 0x04, 0x011a);	/* Same everywhere */
		ibmcam_veio(uvd, 0, 0x2b, 0x011c);
		ibmcam_veio(uvd, 0, 0x23, 0x012a);	/* Same everywhere */
#if 0
		ibmcam_veio(uvd, 0, 0x00, 0x0106);
		ibmcam_veio(uvd, 0, 0x38, 0x0107);
#else
		ibmcam_veio(uvd, 0, 0x02, 0x0106);
		ibmcam_veio(uvd, 0, 0x2a, 0x0107);
#endif
		break;
	case VIDEOSIZE_176x144:
		ibmcam_Packet_Format1(uvd, 0x2b, 0x1e);
		ibmcam_veio(uvd, 0, 0xc9, 0x0119);	/* Same everywhere */
		ibmcam_veio(uvd, 0, 0x80, 0x0109);	/* Same everywhere */
		ibmcam_veio(uvd, 0, 0x04, 0x0102);
		ibmcam_veio(uvd, 0, 0x02, 0x0104);
		ibmcam_veio(uvd, 0, 0x04, 0x011a);	/* Same everywhere */
		ibmcam_veio(uvd, 0, 0x2b, 0x011c);
		ibmcam_veio(uvd, 0, 0x23, 0x012a);	/* Same everywhere */
		ibmcam_veio(uvd, 0, 0x01, 0x0106);
		ibmcam_veio(uvd, 0, 0xca, 0x0107);
		break;
	case VIDEOSIZE_352x288:
		ibmcam_Packet_Format1(uvd, 0x2b, 0x1f);
		ibmcam_veio(uvd, 0, 0xc9, 0x0119);	/* Same everywhere */
		ibmcam_veio(uvd, 0, 0x80, 0x0109);	/* Same everywhere */
		ibmcam_veio(uvd, 0, 0x08, 0x0102);
		ibmcam_veio(uvd, 0, 0x01, 0x0104);
		ibmcam_veio(uvd, 0, 0x04, 0x011a);	/* Same everywhere */
		ibmcam_veio(uvd, 0, 0x2f, 0x011c);
		ibmcam_veio(uvd, 0, 0x23, 0x012a);	/* Same everywhere */
		ibmcam_veio(uvd, 0, 0x03, 0x0106);
		ibmcam_veio(uvd, 0, 0xf6, 0x0107);
		break;
	}
	return (CAMERA_IS_OPERATIONAL(uvd) ? 0 : -EFAULT);
}

static int ibmcam_model2_setup(struct uvd *uvd)
{
	ibmcam_veio(uvd, 0, 0x0000, 0x0100);	/* LED on */
	ibmcam_veio(uvd, 1, 0x0000, 0x0116);
	ibmcam_veio(uvd, 0, 0x0060, 0x0116);
	ibmcam_veio(uvd, 0, 0x0002, 0x0112);
	ibmcam_veio(uvd, 0, 0x00bc, 0x012c);
	ibmcam_veio(uvd, 0, 0x0008, 0x012b);
	ibmcam_veio(uvd, 0, 0x0000, 0x0108);
	ibmcam_veio(uvd, 0, 0x0001, 0x0133);
	ibmcam_veio(uvd, 0, 0x0001, 0x0102);
	switch (uvd->videosize) {
	case VIDEOSIZE_176x144:
		ibmcam_veio(uvd, 0, 0x002c, 0x0103);	/* All except 320x240 */
		ibmcam_veio(uvd, 0, 0x0000, 0x0104);	/* Same */
		ibmcam_veio(uvd, 0, 0x0024, 0x0105);	/* 176x144, 352x288 */
		ibmcam_veio(uvd, 0, 0x00b9, 0x010a);	/* Unique to this mode */
		ibmcam_veio(uvd, 0, 0x0038, 0x0119);	/* Unique to this mode */
		ibmcam_veio(uvd, 0, 0x0003, 0x0106);	/* Same */
		ibmcam_veio(uvd, 0, 0x0090, 0x0107);	/* Unique to every mode*/
		break;
	case VIDEOSIZE_320x240:
		ibmcam_veio(uvd, 0, 0x0028, 0x0103);	/* Unique to this mode */
		ibmcam_veio(uvd, 0, 0x0000, 0x0104);	/* Same */
		ibmcam_veio(uvd, 0, 0x001e, 0x0105);	/* 320x240, 352x240 */
		ibmcam_veio(uvd, 0, 0x0039, 0x010a);	/* All except 176x144 */
		ibmcam_veio(uvd, 0, 0x0070, 0x0119);	/* All except 176x144 */
		ibmcam_veio(uvd, 0, 0x0003, 0x0106);	/* Same */
		ibmcam_veio(uvd, 0, 0x0098, 0x0107);	/* Unique to every mode*/
		break;
	case VIDEOSIZE_352x240:
		ibmcam_veio(uvd, 0, 0x002c, 0x0103);	/* All except 320x240 */
		ibmcam_veio(uvd, 0, 0x0000, 0x0104);	/* Same */
		ibmcam_veio(uvd, 0, 0x001e, 0x0105);	/* 320x240, 352x240 */
		ibmcam_veio(uvd, 0, 0x0039, 0x010a);	/* All except 176x144 */
		ibmcam_veio(uvd, 0, 0x0070, 0x0119);	/* All except 176x144 */
		ibmcam_veio(uvd, 0, 0x0003, 0x0106);	/* Same */
		ibmcam_veio(uvd, 0, 0x00da, 0x0107);	/* Unique to every mode*/
		break;
	case VIDEOSIZE_352x288:
		ibmcam_veio(uvd, 0, 0x002c, 0x0103);	/* All except 320x240 */
		ibmcam_veio(uvd, 0, 0x0000, 0x0104);	/* Same */
		ibmcam_veio(uvd, 0, 0x0024, 0x0105);	/* 176x144, 352x288 */
		ibmcam_veio(uvd, 0, 0x0039, 0x010a);	/* All except 176x144 */
		ibmcam_veio(uvd, 0, 0x0070, 0x0119);	/* All except 176x144 */
		ibmcam_veio(uvd, 0, 0x0003, 0x0106);	/* Same */
		ibmcam_veio(uvd, 0, 0x00fe, 0x0107);	/* Unique to every mode*/
		break;
	}
	return (CAMERA_IS_OPERATIONAL(uvd) ? 0 : -EFAULT);
}

/*
 * ibmcam_model1_setup_after_video_if()
 *
 * This code adds finishing touches to the video data interface.
 * Here we configure the frame rate and turn on the LED.
 */
static void ibmcam_model1_setup_after_video_if(struct uvd *uvd)
{
	unsigned short internal_frame_rate;

	RESTRICT_TO_RANGE(framerate, FRAMERATE_MIN, FRAMERATE_MAX);
	internal_frame_rate = FRAMERATE_MAX - framerate; /* 0=Fast 6=Slow */
	ibmcam_veio(uvd, 0, 0x01, 0x0100);	/* LED On  */
	ibmcam_veio(uvd, 0, internal_frame_rate, 0x0111);
	ibmcam_veio(uvd, 0, 0x01, 0x0114);
	ibmcam_veio(uvd, 0, 0xc0, 0x010c);
}

static void ibmcam_model2_setup_after_video_if(struct uvd *uvd)
{
	unsigned short setup_model2_rg2, setup_model2_sat, setup_model2_yb;

	ibmcam_veio(uvd, 0, 0x0000, 0x0100);	/* LED on */

	switch (uvd->videosize) {
	case VIDEOSIZE_176x144:
		ibmcam_veio(uvd, 0, 0x0050, 0x0111);
		ibmcam_veio(uvd, 0, 0x00d0, 0x0111);
		break;
	case VIDEOSIZE_320x240:
	case VIDEOSIZE_352x240:
	case VIDEOSIZE_352x288:
		ibmcam_veio(uvd, 0, 0x0040, 0x0111);
		ibmcam_veio(uvd, 0, 0x00c0, 0x0111);
		break;
	}
	ibmcam_veio(uvd, 0, 0x009b, 0x010f);
	ibmcam_veio(uvd, 0, 0x00bb, 0x010f);

	/*
	 * Hardware settings, may affect CMOS sensor; not user controls!
	 * -------------------------------------------------------------
	 * 0x0004: no effect
	 * 0x0006: hardware effect
	 * 0x0008: no effect
	 * 0x000a: stops video stream, probably important h/w setting
	 * 0x000c: changes color in hardware manner (not user setting)
	 * 0x0012: changes number of colors (does not affect speed)
	 * 0x002a: no effect
	 * 0x002c: hardware setting (related to scan lines)
	 * 0x002e: stops video stream, probably important h/w setting
	 */
	ibmcam_model2_Packet1(uvd, 0x000a, 0x005c);
	ibmcam_model2_Packet1(uvd, 0x0004, 0x0000);
	ibmcam_model2_Packet1(uvd, 0x0006, 0x00fb);
	ibmcam_model2_Packet1(uvd, 0x0008, 0x0000);
	ibmcam_model2_Packet1(uvd, 0x000c, 0x0009);
	ibmcam_model2_Packet1(uvd, 0x0012, 0x000a);
	ibmcam_model2_Packet1(uvd, 0x002a, 0x0000);
	ibmcam_model2_Packet1(uvd, 0x002c, 0x0000);
	ibmcam_model2_Packet1(uvd, 0x002e, 0x0008);

	/*
	 * Function 0x0030 pops up all over the place. Apparently
	 * it is a hardware control register, with every bit assigned to
	 * do something.
	 */
	ibmcam_model2_Packet1(uvd, 0x0030, 0x0000);

	/*
	 * Magic control of CMOS sensor. Only lower values like
	 * 0-3 work, and picture shifts left or right. Don't change.
	 */
	switch (uvd->videosize) {
	case VIDEOSIZE_176x144:
		ibmcam_model2_Packet1(uvd, 0x0014, 0x0002);
		ibmcam_model2_Packet1(uvd, 0x0016, 0x0002); /* Horizontal shift */
		ibmcam_model2_Packet1(uvd, 0x0018, 0x004a); /* Another hardware setting */
		break;
	case VIDEOSIZE_320x240:
		ibmcam_model2_Packet1(uvd, 0x0014, 0x0009);
		ibmcam_model2_Packet1(uvd, 0x0016, 0x0005); /* Horizontal shift */
		ibmcam_model2_Packet1(uvd, 0x0018, 0x0044); /* Another hardware setting */
		break;
	case VIDEOSIZE_352x240:
		/* This mode doesn't work as Windows programs it; changed to work */
		ibmcam_model2_Packet1(uvd, 0x0014, 0x0009); /* Windows sets this to 8 */
		ibmcam_model2_Packet1(uvd, 0x0016, 0x0003); /* Horizontal shift */
		ibmcam_model2_Packet1(uvd, 0x0018, 0x0044); /* Windows sets this to 0x0045 */
		break;
	case VIDEOSIZE_352x288:
		ibmcam_model2_Packet1(uvd, 0x0014, 0x0003);
		ibmcam_model2_Packet1(uvd, 0x0016, 0x0002); /* Horizontal shift */
		ibmcam_model2_Packet1(uvd, 0x0018, 0x004a); /* Another hardware setting */
		break;
	}

	ibmcam_model2_Packet1(uvd, mod2_brightness, 0x005a);

	/*
	 * We have our own frame rate setting varying from 0 (slowest) to 6 (fastest).
	 * The camera model 2 allows frame rate in range [0..0x1F] where 0 is also the
	 * slowest setting. However for all practical reasons high settings make no
	 * sense because USB is not fast enough to support high FPS. Be aware that
	 * the picture datastream will be severely disrupted if you ask for
	 * frame rate faster than allowed for the video size - see below:
	 *
	 * Allowable ranges (obtained experimentally on OHCI, K6-3, 450 MHz):
	 * -----------------------------------------------------------------
	 * 176x144: [6..31]
	 * 320x240: [8..31]
	 * 352x240: [10..31]
	 * 352x288: [16..31] I have to raise lower threshold for stability...
	 *
	 * As usual, slower FPS provides better sensitivity.
	 */
	{
		short hw_fps=31, i_framerate;

		RESTRICT_TO_RANGE(framerate, FRAMERATE_MIN, FRAMERATE_MAX);
		i_framerate = FRAMERATE_MAX - framerate + FRAMERATE_MIN;
		switch (uvd->videosize) {
		case VIDEOSIZE_176x144:
			hw_fps = 6 + i_framerate*4;
			break;
		case VIDEOSIZE_320x240:
			hw_fps = 8 + i_framerate*3;
			break;
		case VIDEOSIZE_352x240:
			hw_fps = 10 + i_framerate*2;
			break;
		case VIDEOSIZE_352x288:
			hw_fps = 28 + i_framerate/2;
			break;
		}
		if (uvd->debug > 0)
			dev_info(&uvd->dev->dev, "Framerate (hardware): %hd.\n",
				 hw_fps);
		RESTRICT_TO_RANGE(hw_fps, 0, 31);
		ibmcam_model2_Packet1(uvd, mod2_set_framerate, hw_fps);
	}

	/*
	 * This setting does not visibly affect pictures; left it here
	 * because it was present in Windows USB data stream. This function
	 * does not allow arbitrary values and apparently is a bit mask, to
	 * be activated only at appropriate time. Don't change it randomly!
	 */
	switch (uvd->videosize) {
	case VIDEOSIZE_176x144:
		ibmcam_model2_Packet1(uvd, 0x0026, 0x00c2);
		break;
	case VIDEOSIZE_320x240:
		ibmcam_model2_Packet1(uvd, 0x0026, 0x0044);
		break;
	case VIDEOSIZE_352x240:
		ibmcam_model2_Packet1(uvd, 0x0026, 0x0046);
		break;
	case VIDEOSIZE_352x288:
		ibmcam_model2_Packet1(uvd, 0x0026, 0x0048);
		break;
	}

	ibmcam_model2_Packet1(uvd, mod2_sensitivity, lighting);

	if (init_model2_rg2 >= 0) {
		RESTRICT_TO_RANGE(init_model2_rg2, 0, 255);
		setup_model2_rg2 = init_model2_rg2;
	} else
		setup_model2_rg2 = 0x002f;

	if (init_model2_sat >= 0) {
		RESTRICT_TO_RANGE(init_model2_sat, 0, 255);
		setup_model2_sat = init_model2_sat;
	} else
		setup_model2_sat = 0x0034;

	if (init_model2_yb >= 0) {
		RESTRICT_TO_RANGE(init_model2_yb, 0, 255);
		setup_model2_yb = init_model2_yb;
	} else
		setup_model2_yb = 0x00a0;

	ibmcam_model2_Packet1(uvd, mod2_color_balance_rg2, setup_model2_rg2);
	ibmcam_model2_Packet1(uvd, mod2_saturation, setup_model2_sat);
	ibmcam_model2_Packet1(uvd, mod2_color_balance_yb, setup_model2_yb);
	ibmcam_model2_Packet1(uvd, mod2_hue, uvd->vpic.hue >> 9); /* 0 .. 7F */;

	/* Hardware control command */
	ibmcam_model2_Packet1(uvd, 0x0030, 0x0004);

	ibmcam_veio(uvd, 0, 0x00c0, 0x010c);	/* Go camera, go! */
	usb_clear_halt(uvd->dev, usb_rcvisocpipe(uvd->dev, uvd->video_endp));
}

static void ibmcam_model4_setup_after_video_if(struct uvd *uvd)
{
	switch (uvd->videosize) {
	case VIDEOSIZE_128x96:
		ibmcam_veio(uvd, 0, 0x0000, 0x0100);
		ibmcam_veio(uvd, 0, 0x00c0, 0x0111);
		ibmcam_veio(uvd, 0, 0x00bc, 0x012c);
		ibmcam_veio(uvd, 0, 0x0080, 0x012b);
		ibmcam_veio(uvd, 0, 0x0000, 0x0108);
		ibmcam_veio(uvd, 0, 0x0001, 0x0133);
		ibmcam_veio(uvd, 0, 0x009b, 0x010f);
		ibmcam_veio(uvd, 0, 0x00bb, 0x010f);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x0038, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x0000, 0x0127);
		ibmcam_veio(uvd, 0, 0xfea8, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x000a, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x005c, 0x0127);
		ibmcam_veio(uvd, 0, 0xfea8, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x0004, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x0000, 0x0127);
		ibmcam_veio(uvd, 0, 0x00fb, 0x012e);
		ibmcam_veio(uvd, 0, 0x0000, 0x0130);
		ibmcam_veio(uvd, 0, 0x8a28, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012f);
		ibmcam_veio(uvd, 0, 0xd055, 0x0124);
		ibmcam_veio(uvd, 0, 0x000c, 0x0127);
		ibmcam_veio(uvd, 0, 0x0009, 0x012e);
		ibmcam_veio(uvd, 0, 0xaa28, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x0012, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x0008, 0x0127);
		ibmcam_veio(uvd, 0, 0x00aa, 0x0130);
		ibmcam_veio(uvd, 0, 0x82a8, 0x0124);
		ibmcam_veio(uvd, 0, 0x002a, 0x012d);
		ibmcam_veio(uvd, 0, 0x0000, 0x012f);
		ibmcam_veio(uvd, 0, 0xd145, 0x0124);
		ibmcam_veio(uvd, 0, 0xfffa, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x0034, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x0000, 0x0127);
		ibmcam_veio(uvd, 0, 0xfea8, 0x0124);
		ibmcam_veio(uvd, 0, 0x0070, 0x0119);
		ibmcam_veio(uvd, 0, 0x00d2, 0x0107);
		ibmcam_veio(uvd, 0, 0x0003, 0x0106);
		ibmcam_veio(uvd, 0, 0x005e, 0x0107);
		ibmcam_veio(uvd, 0, 0x0003, 0x0106);
		ibmcam_veio(uvd, 0, 0x00d0, 0x0111);
		ibmcam_veio(uvd, 0, 0x0039, 0x010a);
		ibmcam_veio(uvd, 0, 0x0001, 0x0102);
		ibmcam_veio(uvd, 0, 0x0028, 0x0103);
		ibmcam_veio(uvd, 0, 0x0000, 0x0104);
		ibmcam_veio(uvd, 0, 0x001e, 0x0105);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x0016, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x000a, 0x0127);
		ibmcam_veio(uvd, 0, 0x00aa, 0x0130);
		ibmcam_veio(uvd, 0, 0x82a8, 0x0124);
		ibmcam_veio(uvd, 0, 0x0014, 0x012d);
		ibmcam_veio(uvd, 0, 0x0008, 0x012f);
		ibmcam_veio(uvd, 0, 0xd145, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012e);
		ibmcam_veio(uvd, 0, 0x001a, 0x0130);
		ibmcam_veio(uvd, 0, 0x8a0a, 0x0124);
		ibmcam_veio(uvd, 0, 0x005a, 0x012d);
		ibmcam_veio(uvd, 0, 0x9545, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x0127);
		ibmcam_veio(uvd, 0, 0x0018, 0x012e);
		ibmcam_veio(uvd, 0, 0x0043, 0x0130);
		ibmcam_veio(uvd, 0, 0x8a28, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012f);
		ibmcam_veio(uvd, 0, 0xd055, 0x0124);
		ibmcam_veio(uvd, 0, 0x001c, 0x0127);
		ibmcam_veio(uvd, 0, 0x00eb, 0x012e);
		ibmcam_veio(uvd, 0, 0xaa28, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x0032, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x0000, 0x0127);
		ibmcam_veio(uvd, 0, 0x00aa, 0x0130);
		ibmcam_veio(uvd, 0, 0x82a8, 0x0124);
		ibmcam_veio(uvd, 0, 0x0036, 0x012d);
		ibmcam_veio(uvd, 0, 0x0008, 0x012f);
		ibmcam_veio(uvd, 0, 0xd145, 0x0124);
		ibmcam_veio(uvd, 0, 0xfffa, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x001e, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x0017, 0x0127);
		ibmcam_veio(uvd, 0, 0x0013, 0x012e);
		ibmcam_veio(uvd, 0, 0x0031, 0x0130);
		ibmcam_veio(uvd, 0, 0x8a28, 0x0124);
		ibmcam_veio(uvd, 0, 0x0017, 0x012d);
		ibmcam_veio(uvd, 0, 0x0078, 0x012f);
		ibmcam_veio(uvd, 0, 0xd145, 0x0124);
		ibmcam_veio(uvd, 0, 0x0000, 0x0127);
		ibmcam_veio(uvd, 0, 0xfea8, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x0038, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x0004, 0x0127);
		ibmcam_veio(uvd, 0, 0xfea8, 0x0124);
		ibmcam_veio(uvd, 0, 0x00c0, 0x010c);
		break;
	case VIDEOSIZE_160x120:
		ibmcam_veio(uvd, 0, 0x0000, 0x0100);
		ibmcam_veio(uvd, 0, 0x00c0, 0x0111);
		ibmcam_veio(uvd, 0, 0x00bc, 0x012c);
		ibmcam_veio(uvd, 0, 0x0080, 0x012b);
		ibmcam_veio(uvd, 0, 0x0000, 0x0108);
		ibmcam_veio(uvd, 0, 0x0001, 0x0133);
		ibmcam_veio(uvd, 0, 0x009b, 0x010f);
		ibmcam_veio(uvd, 0, 0x00bb, 0x010f);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x0038, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x0000, 0x0127);
		ibmcam_veio(uvd, 0, 0xfea8, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x000a, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x005c, 0x0127);
		ibmcam_veio(uvd, 0, 0xfea8, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x0004, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x0000, 0x0127);
		ibmcam_veio(uvd, 0, 0x00fb, 0x012e);
		ibmcam_veio(uvd, 0, 0x0000, 0x0130);
		ibmcam_veio(uvd, 0, 0x8a28, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012f);
		ibmcam_veio(uvd, 0, 0xd055, 0x0124);
		ibmcam_veio(uvd, 0, 0x000c, 0x0127);
		ibmcam_veio(uvd, 0, 0x0009, 0x012e);
		ibmcam_veio(uvd, 0, 0xaa28, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x0012, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x0008, 0x0127);
		ibmcam_veio(uvd, 0, 0x00aa, 0x0130);
		ibmcam_veio(uvd, 0, 0x82a8, 0x0124);
		ibmcam_veio(uvd, 0, 0x002a, 0x012d);
		ibmcam_veio(uvd, 0, 0x0000, 0x012f);
		ibmcam_veio(uvd, 0, 0xd145, 0x0124);
		ibmcam_veio(uvd, 0, 0xfffa, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x0034, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x0000, 0x0127);
		ibmcam_veio(uvd, 0, 0xfea8, 0x0124);
		ibmcam_veio(uvd, 0, 0x0038, 0x0119);
		ibmcam_veio(uvd, 0, 0x00d8, 0x0107);
		ibmcam_veio(uvd, 0, 0x0002, 0x0106);
		ibmcam_veio(uvd, 0, 0x00d0, 0x0111);
		ibmcam_veio(uvd, 0, 0x00b9, 0x010a);
		ibmcam_veio(uvd, 0, 0x0001, 0x0102);
		ibmcam_veio(uvd, 0, 0x0028, 0x0103);
		ibmcam_veio(uvd, 0, 0x0000, 0x0104);
		ibmcam_veio(uvd, 0, 0x001e, 0x0105);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x0016, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x000b, 0x0127);
		ibmcam_veio(uvd, 0, 0x00aa, 0x0130);
		ibmcam_veio(uvd, 0, 0x82a8, 0x0124);
		ibmcam_veio(uvd, 0, 0x0014, 0x012d);
		ibmcam_veio(uvd, 0, 0x0008, 0x012f);
		ibmcam_veio(uvd, 0, 0xd145, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012e);
		ibmcam_veio(uvd, 0, 0x001a, 0x0130);
		ibmcam_veio(uvd, 0, 0x8a0a, 0x0124);
		ibmcam_veio(uvd, 0, 0x005a, 0x012d);
		ibmcam_veio(uvd, 0, 0x9545, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x0127);
		ibmcam_veio(uvd, 0, 0x0018, 0x012e);
		ibmcam_veio(uvd, 0, 0x0043, 0x0130);
		ibmcam_veio(uvd, 0, 0x8a28, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012f);
		ibmcam_veio(uvd, 0, 0xd055, 0x0124);
		ibmcam_veio(uvd, 0, 0x001c, 0x0127);
		ibmcam_veio(uvd, 0, 0x00c7, 0x012e);
		ibmcam_veio(uvd, 0, 0xaa28, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x0032, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x0025, 0x0127);
		ibmcam_veio(uvd, 0, 0x00aa, 0x0130);
		ibmcam_veio(uvd, 0, 0x82a8, 0x0124);
		ibmcam_veio(uvd, 0, 0x0036, 0x012d);
		ibmcam_veio(uvd, 0, 0x0008, 0x012f);
		ibmcam_veio(uvd, 0, 0xd145, 0x0124);
		ibmcam_veio(uvd, 0, 0xfffa, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x001e, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x0048, 0x0127);
		ibmcam_veio(uvd, 0, 0x0035, 0x012e);
		ibmcam_veio(uvd, 0, 0x00d0, 0x0130);
		ibmcam_veio(uvd, 0, 0x8a28, 0x0124);
		ibmcam_veio(uvd, 0, 0x0048, 0x012d);
		ibmcam_veio(uvd, 0, 0x0090, 0x012f);
		ibmcam_veio(uvd, 0, 0xd145, 0x0124);
		ibmcam_veio(uvd, 0, 0x0001, 0x0127);
		ibmcam_veio(uvd, 0, 0xfea8, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x0038, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x0004, 0x0127);
		ibmcam_veio(uvd, 0, 0xfea8, 0x0124);
		ibmcam_veio(uvd, 0, 0x00c0, 0x010c);
		break;
	case VIDEOSIZE_176x144:
		ibmcam_veio(uvd, 0, 0x0000, 0x0100);
		ibmcam_veio(uvd, 0, 0x00c0, 0x0111);
		ibmcam_veio(uvd, 0, 0x00bc, 0x012c);
		ibmcam_veio(uvd, 0, 0x0080, 0x012b);
		ibmcam_veio(uvd, 0, 0x0000, 0x0108);
		ibmcam_veio(uvd, 0, 0x0001, 0x0133);
		ibmcam_veio(uvd, 0, 0x009b, 0x010f);
		ibmcam_veio(uvd, 0, 0x00bb, 0x010f);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x0038, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x0000, 0x0127);
		ibmcam_veio(uvd, 0, 0xfea8, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x000a, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x005c, 0x0127);
		ibmcam_veio(uvd, 0, 0xfea8, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x0004, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x0000, 0x0127);
		ibmcam_veio(uvd, 0, 0x00fb, 0x012e);
		ibmcam_veio(uvd, 0, 0x0000, 0x0130);
		ibmcam_veio(uvd, 0, 0x8a28, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012f);
		ibmcam_veio(uvd, 0, 0xd055, 0x0124);
		ibmcam_veio(uvd, 0, 0x000c, 0x0127);
		ibmcam_veio(uvd, 0, 0x0009, 0x012e);
		ibmcam_veio(uvd, 0, 0xaa28, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x0012, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x0008, 0x0127);
		ibmcam_veio(uvd, 0, 0x00aa, 0x0130);
		ibmcam_veio(uvd, 0, 0x82a8, 0x0124);
		ibmcam_veio(uvd, 0, 0x002a, 0x012d);
		ibmcam_veio(uvd, 0, 0x0000, 0x012f);
		ibmcam_veio(uvd, 0, 0xd145, 0x0124);
		ibmcam_veio(uvd, 0, 0xfffa, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x0034, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x0000, 0x0127);
		ibmcam_veio(uvd, 0, 0xfea8, 0x0124);
		ibmcam_veio(uvd, 0, 0x0038, 0x0119);
		ibmcam_veio(uvd, 0, 0x00d6, 0x0107);
		ibmcam_veio(uvd, 0, 0x0003, 0x0106);
		ibmcam_veio(uvd, 0, 0x0018, 0x0107);
		ibmcam_veio(uvd, 0, 0x0003, 0x0106);
		ibmcam_veio(uvd, 0, 0x00d0, 0x0111);
		ibmcam_veio(uvd, 0, 0x00b9, 0x010a);
		ibmcam_veio(uvd, 0, 0x0001, 0x0102);
		ibmcam_veio(uvd, 0, 0x002c, 0x0103);
		ibmcam_veio(uvd, 0, 0x0000, 0x0104);
		ibmcam_veio(uvd, 0, 0x0024, 0x0105);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x0016, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x0007, 0x0127);
		ibmcam_veio(uvd, 0, 0x00aa, 0x0130);
		ibmcam_veio(uvd, 0, 0x82a8, 0x0124);
		ibmcam_veio(uvd, 0, 0x0014, 0x012d);
		ibmcam_veio(uvd, 0, 0x0001, 0x012f);
		ibmcam_veio(uvd, 0, 0xd145, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012e);
		ibmcam_veio(uvd, 0, 0x001a, 0x0130);
		ibmcam_veio(uvd, 0, 0x8a0a, 0x0124);
		ibmcam_veio(uvd, 0, 0x005e, 0x012d);
		ibmcam_veio(uvd, 0, 0x9545, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x0127);
		ibmcam_veio(uvd, 0, 0x0018, 0x012e);
		ibmcam_veio(uvd, 0, 0x0049, 0x0130);
		ibmcam_veio(uvd, 0, 0x8a28, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012f);
		ibmcam_veio(uvd, 0, 0xd055, 0x0124);
		ibmcam_veio(uvd, 0, 0x001c, 0x0127);
		ibmcam_veio(uvd, 0, 0x00c7, 0x012e);
		ibmcam_veio(uvd, 0, 0xaa28, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x0032, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x0028, 0x0127);
		ibmcam_veio(uvd, 0, 0x00aa, 0x0130);
		ibmcam_veio(uvd, 0, 0x82a8, 0x0124);
		ibmcam_veio(uvd, 0, 0x0036, 0x012d);
		ibmcam_veio(uvd, 0, 0x0008, 0x012f);
		ibmcam_veio(uvd, 0, 0xd145, 0x0124);
		ibmcam_veio(uvd, 0, 0xfffa, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x001e, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x0010, 0x0127);
		ibmcam_veio(uvd, 0, 0x0013, 0x012e);
		ibmcam_veio(uvd, 0, 0x002a, 0x0130);
		ibmcam_veio(uvd, 0, 0x8a28, 0x0124);
		ibmcam_veio(uvd, 0, 0x0010, 0x012d);
		ibmcam_veio(uvd, 0, 0x006d, 0x012f);
		ibmcam_veio(uvd, 0, 0xd145, 0x0124);
		ibmcam_veio(uvd, 0, 0x0001, 0x0127);
		ibmcam_veio(uvd, 0, 0xfea8, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x0038, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x0004, 0x0127);
		ibmcam_veio(uvd, 0, 0xfea8, 0x0124);
		ibmcam_veio(uvd, 0, 0x00c0, 0x010c);
		break;
	case VIDEOSIZE_320x240:
		ibmcam_veio(uvd, 0, 0x0000, 0x0100);
		ibmcam_veio(uvd, 0, 0x00c0, 0x0111);
		ibmcam_veio(uvd, 0, 0x00bc, 0x012c);
		ibmcam_veio(uvd, 0, 0x0080, 0x012b);
		ibmcam_veio(uvd, 0, 0x0000, 0x0108);
		ibmcam_veio(uvd, 0, 0x0001, 0x0133);
		ibmcam_veio(uvd, 0, 0x009b, 0x010f);
		ibmcam_veio(uvd, 0, 0x00bb, 0x010f);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x0038, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x0000, 0x0127);
		ibmcam_veio(uvd, 0, 0xfea8, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x000a, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x005c, 0x0127);
		ibmcam_veio(uvd, 0, 0xfea8, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x0004, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x0000, 0x0127);
		ibmcam_veio(uvd, 0, 0x00fb, 0x012e);
		ibmcam_veio(uvd, 0, 0x0000, 0x0130);
		ibmcam_veio(uvd, 0, 0x8a28, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012f);
		ibmcam_veio(uvd, 0, 0xd055, 0x0124);
		ibmcam_veio(uvd, 0, 0x000c, 0x0127);
		ibmcam_veio(uvd, 0, 0x0009, 0x012e);
		ibmcam_veio(uvd, 0, 0xaa28, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x0012, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x0008, 0x0127);
		ibmcam_veio(uvd, 0, 0x00aa, 0x0130);
		ibmcam_veio(uvd, 0, 0x82a8, 0x0124);
		ibmcam_veio(uvd, 0, 0x002a, 0x012d);
		ibmcam_veio(uvd, 0, 0x0000, 0x012f);
		ibmcam_veio(uvd, 0, 0xd145, 0x0124);
		ibmcam_veio(uvd, 0, 0xfffa, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x0034, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x0000, 0x0127);
		ibmcam_veio(uvd, 0, 0xfea8, 0x0124);
		ibmcam_veio(uvd, 0, 0x0070, 0x0119);
		ibmcam_veio(uvd, 0, 0x00d2, 0x0107);
		ibmcam_veio(uvd, 0, 0x0003, 0x0106);
		ibmcam_veio(uvd, 0, 0x005e, 0x0107);
		ibmcam_veio(uvd, 0, 0x0003, 0x0106);
		ibmcam_veio(uvd, 0, 0x00d0, 0x0111);
		ibmcam_veio(uvd, 0, 0x0039, 0x010a);
		ibmcam_veio(uvd, 0, 0x0001, 0x0102);
		ibmcam_veio(uvd, 0, 0x0028, 0x0103);
		ibmcam_veio(uvd, 0, 0x0000, 0x0104);
		ibmcam_veio(uvd, 0, 0x001e, 0x0105);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x0016, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x000a, 0x0127);
		ibmcam_veio(uvd, 0, 0x00aa, 0x0130);
		ibmcam_veio(uvd, 0, 0x82a8, 0x0124);
		ibmcam_veio(uvd, 0, 0x0014, 0x012d);
		ibmcam_veio(uvd, 0, 0x0008, 0x012f);
		ibmcam_veio(uvd, 0, 0xd145, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012e);
		ibmcam_veio(uvd, 0, 0x001a, 0x0130);
		ibmcam_veio(uvd, 0, 0x8a0a, 0x0124);
		ibmcam_veio(uvd, 0, 0x005a, 0x012d);
		ibmcam_veio(uvd, 0, 0x9545, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x0127);
		ibmcam_veio(uvd, 0, 0x0018, 0x012e);
		ibmcam_veio(uvd, 0, 0x0043, 0x0130);
		ibmcam_veio(uvd, 0, 0x8a28, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012f);
		ibmcam_veio(uvd, 0, 0xd055, 0x0124);
		ibmcam_veio(uvd, 0, 0x001c, 0x0127);
		ibmcam_veio(uvd, 0, 0x00eb, 0x012e);
		ibmcam_veio(uvd, 0, 0xaa28, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x0032, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x0000, 0x0127);
		ibmcam_veio(uvd, 0, 0x00aa, 0x0130);
		ibmcam_veio(uvd, 0, 0x82a8, 0x0124);
		ibmcam_veio(uvd, 0, 0x0036, 0x012d);
		ibmcam_veio(uvd, 0, 0x0008, 0x012f);
		ibmcam_veio(uvd, 0, 0xd145, 0x0124);
		ibmcam_veio(uvd, 0, 0xfffa, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x001e, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x0017, 0x0127);
		ibmcam_veio(uvd, 0, 0x0013, 0x012e);
		ibmcam_veio(uvd, 0, 0x0031, 0x0130);
		ibmcam_veio(uvd, 0, 0x8a28, 0x0124);
		ibmcam_veio(uvd, 0, 0x0017, 0x012d);
		ibmcam_veio(uvd, 0, 0x0078, 0x012f);
		ibmcam_veio(uvd, 0, 0xd145, 0x0124);
		ibmcam_veio(uvd, 0, 0x0000, 0x0127);
		ibmcam_veio(uvd, 0, 0xfea8, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x0038, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x0004, 0x0127);
		ibmcam_veio(uvd, 0, 0xfea8, 0x0124);
		ibmcam_veio(uvd, 0, 0x00c0, 0x010c);
		break;
	case VIDEOSIZE_352x288:
		ibmcam_veio(uvd, 0, 0x0000, 0x0100);
		ibmcam_veio(uvd, 0, 0x00c0, 0x0111);
		ibmcam_veio(uvd, 0, 0x00bc, 0x012c);
		ibmcam_veio(uvd, 0, 0x0080, 0x012b);
		ibmcam_veio(uvd, 0, 0x0000, 0x0108);
		ibmcam_veio(uvd, 0, 0x0001, 0x0133);
		ibmcam_veio(uvd, 0, 0x009b, 0x010f);
		ibmcam_veio(uvd, 0, 0x00bb, 0x010f);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x0038, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x0000, 0x0127);
		ibmcam_veio(uvd, 0, 0xfea8, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x000a, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x005c, 0x0127);
		ibmcam_veio(uvd, 0, 0xfea8, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x0004, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x0000, 0x0127);
		ibmcam_veio(uvd, 0, 0x00fb, 0x012e);
		ibmcam_veio(uvd, 0, 0x0000, 0x0130);
		ibmcam_veio(uvd, 0, 0x8a28, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012f);
		ibmcam_veio(uvd, 0, 0xd055, 0x0124);
		ibmcam_veio(uvd, 0, 0x000c, 0x0127);
		ibmcam_veio(uvd, 0, 0x0009, 0x012e);
		ibmcam_veio(uvd, 0, 0xaa28, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x0012, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x0008, 0x0127);
		ibmcam_veio(uvd, 0, 0x00aa, 0x0130);
		ibmcam_veio(uvd, 0, 0x82a8, 0x0124);
		ibmcam_veio(uvd, 0, 0x002a, 0x012d);
		ibmcam_veio(uvd, 0, 0x0000, 0x012f);
		ibmcam_veio(uvd, 0, 0xd145, 0x0124);
		ibmcam_veio(uvd, 0, 0xfffa, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x0034, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x0000, 0x0127);
		ibmcam_veio(uvd, 0, 0xfea8, 0x0124);
		ibmcam_veio(uvd, 0, 0x0070, 0x0119);
		ibmcam_veio(uvd, 0, 0x00f2, 0x0107);
		ibmcam_veio(uvd, 0, 0x0003, 0x0106);
		ibmcam_veio(uvd, 0, 0x008c, 0x0107);
		ibmcam_veio(uvd, 0, 0x0003, 0x0106);
		ibmcam_veio(uvd, 0, 0x00c0, 0x0111);
		ibmcam_veio(uvd, 0, 0x0039, 0x010a);
		ibmcam_veio(uvd, 0, 0x0001, 0x0102);
		ibmcam_veio(uvd, 0, 0x002c, 0x0103);
		ibmcam_veio(uvd, 0, 0x0000, 0x0104);
		ibmcam_veio(uvd, 0, 0x0024, 0x0105);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x0016, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x0006, 0x0127);
		ibmcam_veio(uvd, 0, 0x00aa, 0x0130);
		ibmcam_veio(uvd, 0, 0x82a8, 0x0124);
		ibmcam_veio(uvd, 0, 0x0014, 0x012d);
		ibmcam_veio(uvd, 0, 0x0002, 0x012f);
		ibmcam_veio(uvd, 0, 0xd145, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012e);
		ibmcam_veio(uvd, 0, 0x001a, 0x0130);
		ibmcam_veio(uvd, 0, 0x8a0a, 0x0124);
		ibmcam_veio(uvd, 0, 0x005e, 0x012d);
		ibmcam_veio(uvd, 0, 0x9545, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x0127);
		ibmcam_veio(uvd, 0, 0x0018, 0x012e);
		ibmcam_veio(uvd, 0, 0x0049, 0x0130);
		ibmcam_veio(uvd, 0, 0x8a28, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012f);
		ibmcam_veio(uvd, 0, 0xd055, 0x0124);
		ibmcam_veio(uvd, 0, 0x001c, 0x0127);
		ibmcam_veio(uvd, 0, 0x00cf, 0x012e);
		ibmcam_veio(uvd, 0, 0xaa28, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x0032, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x0000, 0x0127);
		ibmcam_veio(uvd, 0, 0x00aa, 0x0130);
		ibmcam_veio(uvd, 0, 0x82a8, 0x0124);
		ibmcam_veio(uvd, 0, 0x0036, 0x012d);
		ibmcam_veio(uvd, 0, 0x0008, 0x012f);
		ibmcam_veio(uvd, 0, 0xd145, 0x0124);
		ibmcam_veio(uvd, 0, 0xfffa, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x001e, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x0010, 0x0127);
		ibmcam_veio(uvd, 0, 0x0013, 0x012e);
		ibmcam_veio(uvd, 0, 0x0025, 0x0130);
		ibmcam_veio(uvd, 0, 0x8a28, 0x0124);
		ibmcam_veio(uvd, 0, 0x0010, 0x012d);
		ibmcam_veio(uvd, 0, 0x0048, 0x012f);
		ibmcam_veio(uvd, 0, 0xd145, 0x0124);
		ibmcam_veio(uvd, 0, 0x0000, 0x0127);
		ibmcam_veio(uvd, 0, 0xfea8, 0x0124);
		ibmcam_veio(uvd, 0, 0x00aa, 0x012d);
		ibmcam_veio(uvd, 0, 0x0038, 0x012f);
		ibmcam_veio(uvd, 0, 0xd141, 0x0124);
		ibmcam_veio(uvd, 0, 0x0004, 0x0127);
		ibmcam_veio(uvd, 0, 0xfea8, 0x0124);
		ibmcam_veio(uvd, 0, 0x00c0, 0x010c);
		break;
	}
	usb_clear_halt(uvd->dev, usb_rcvisocpipe(uvd->dev, uvd->video_endp));
}

static void ibmcam_model3_setup_after_video_if(struct uvd *uvd)
{
	int i;
	/*
	 * 01.01.08 - Added for RCA video in support -LO
	 * This struct is used to init the Model3 cam to use the RCA video in port
	 * instead of the CCD sensor.
	 */
	static const struct struct_initData initData[] = {
		{0, 0x0000, 0x010c},
		{0, 0x0006, 0x012c},
		{0, 0x0078, 0x012d},
		{0, 0x0046, 0x012f},
		{0, 0xd141, 0x0124},
		{0, 0x0000, 0x0127},
		{0, 0xfea8, 0x0124},
		{1, 0x0000, 0x0116},
		{0, 0x0064, 0x0116},
		{1, 0x0000, 0x0115},
		{0, 0x0003, 0x0115},
		{0, 0x0008, 0x0123},
		{0, 0x0000, 0x0117},
		{0, 0x0000, 0x0112},
		{0, 0x0080, 0x0100},
		{0, 0x0000, 0x0100},
		{1, 0x0000, 0x0116},
		{0, 0x0060, 0x0116},
		{0, 0x0002, 0x0112},
		{0, 0x0000, 0x0123},
		{0, 0x0001, 0x0117},
		{0, 0x0040, 0x0108},
		{0, 0x0019, 0x012c},
		{0, 0x0040, 0x0116},
		{0, 0x000a, 0x0115},
		{0, 0x000b, 0x0115},
		{0, 0x0078, 0x012d},
		{0, 0x0046, 0x012f},
		{0, 0xd141, 0x0124},
		{0, 0x0000, 0x0127},
		{0, 0xfea8, 0x0124},
		{0, 0x0064, 0x0116},
		{0, 0x0000, 0x0115},
		{0, 0x0001, 0x0115},
		{0, 0xffff, 0x0124},
		{0, 0xfff9, 0x0124},
		{0, 0x0086, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x00aa, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0000, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0xfffa, 0x0124},
		{0, 0xffff, 0x0124},
		{0, 0xfff9, 0x0124},
		{0, 0x0086, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x00f2, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x000f, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0xfffa, 0x0124},
		{0, 0xffff, 0x0124},
		{0, 0xfff9, 0x0124},
		{0, 0x0086, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x00f8, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x00fc, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0xfffa, 0x0124},
		{0, 0xffff, 0x0124},
		{0, 0xfff9, 0x0124},
		{0, 0x0086, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x00f9, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x003c, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0xfffa, 0x0124},
		{0, 0xffff, 0x0124},
		{0, 0xfff9, 0x0124},
		{0, 0x0086, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0027, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0019, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0xfffa, 0x0124},
		{0, 0xfff9, 0x0124},
		{0, 0x0086, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0037, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0000, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0021, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0xfffa, 0x0124},
		{0, 0xfff9, 0x0124},
		{0, 0x0086, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0038, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0006, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0045, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0xfffa, 0x0124},
		{0, 0xfff9, 0x0124},
		{0, 0x0086, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0037, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0001, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x002a, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0xfffa, 0x0124},
		{0, 0xfff9, 0x0124},
		{0, 0x0086, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0038, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0000, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x000e, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0xfffa, 0x0124},
		{0, 0xfff9, 0x0124},
		{0, 0x0086, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0037, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0001, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x002b, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0xfffa, 0x0124},
		{0, 0xfff9, 0x0124},
		{0, 0x0086, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0038, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0001, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x00f4, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0xfffa, 0x0124},
		{0, 0xfff9, 0x0124},
		{0, 0x0086, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0037, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0001, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x002c, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0xfffa, 0x0124},
		{0, 0xfff9, 0x0124},
		{0, 0x0086, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0038, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0001, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0004, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0xfffa, 0x0124},
		{0, 0xfff9, 0x0124},
		{0, 0x0086, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0037, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0001, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x002d, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0xfffa, 0x0124},
		{0, 0xfff9, 0x0124},
		{0, 0x0086, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0038, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0000, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0014, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0xfffa, 0x0124},
		{0, 0xfff9, 0x0124},
		{0, 0x0086, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0037, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0001, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x002e, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0xfffa, 0x0124},
		{0, 0xfff9, 0x0124},
		{0, 0x0086, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0038, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0003, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0000, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0xfffa, 0x0124},
		{0, 0xfff9, 0x0124},
		{0, 0x0086, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0037, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0001, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x002f, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0xfffa, 0x0124},
		{0, 0xfff9, 0x0124},
		{0, 0x0086, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0038, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0003, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0014, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0xfffa, 0x0124},
		{0, 0xfff9, 0x0124},
		{0, 0x0086, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0037, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0001, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0040, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0xfffa, 0x0124},
		{0, 0xfff9, 0x0124},
		{0, 0x0086, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0038, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0000, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0040, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0xfffa, 0x0124},
		{0, 0xfff9, 0x0124},
		{0, 0x0086, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0037, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0001, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0053, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0xfffa, 0x0124},
		{0, 0xfff9, 0x0124},
		{0, 0x0086, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0038, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0000, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0038, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0xfffa, 0x0124},
		{0, 0x0000, 0x0101},
		{0, 0x00a0, 0x0103},
		{0, 0x0078, 0x0105},
		{0, 0x0000, 0x010a},
		{0, 0x0024, 0x010b},
		{0, 0x0028, 0x0119},
		{0, 0x0088, 0x011b},
		{0, 0x0002, 0x011d},
		{0, 0x0003, 0x011e},
		{0, 0x0000, 0x0129},
		{0, 0x00fc, 0x012b},
		{0, 0x0008, 0x0102},
		{0, 0x0000, 0x0104},
		{0, 0x0008, 0x011a},
		{0, 0x0028, 0x011c},
		{0, 0x0021, 0x012a},
		{0, 0x0000, 0x0118},
		{0, 0x0000, 0x0132},
		{0, 0x0000, 0x0109},
		{0, 0xfff9, 0x0124},
		{0, 0x0086, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0037, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0001, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0031, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0xfffa, 0x0124},
		{0, 0xfff9, 0x0124},
		{0, 0x0086, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0038, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0000, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0000, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0xfffa, 0x0124},
		{0, 0xfff9, 0x0124},
		{0, 0x0086, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0037, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0001, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0040, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0xfffa, 0x0124},
		{0, 0xfff9, 0x0124},
		{0, 0x0086, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0038, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0000, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0040, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0xfffa, 0x0124},
		{0, 0xfff9, 0x0124},
		{0, 0x0086, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0037, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0000, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x00dc, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0xfffa, 0x0124},
		{0, 0xfff9, 0x0124},
		{0, 0x0086, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0038, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0000, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0000, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0xfffa, 0x0124},
		{0, 0xfff9, 0x0124},
		{0, 0x0086, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0037, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0001, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0032, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0xfffa, 0x0124},
		{0, 0xfff9, 0x0124},
		{0, 0x0086, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0038, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0001, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0020, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0xfffa, 0x0124},
		{0, 0xfff9, 0x0124},
		{0, 0x0086, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0037, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0001, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0040, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0xfffa, 0x0124},
		{0, 0xfff9, 0x0124},
		{0, 0x0086, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0038, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0000, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0040, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0xfffa, 0x0124},
		{0, 0xfff9, 0x0124},
		{0, 0x0086, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0037, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0000, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0030, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0xfffa, 0x0124},
		{0, 0xfff9, 0x0124},
		{0, 0x0086, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0038, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0008, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0x0000, 0x0127},
		{0, 0xfff8, 0x0124},
		{0, 0xfffd, 0x0124},
		{0, 0xfffa, 0x0124},
		{0, 0x0003, 0x0106},
		{0, 0x0062, 0x0107},
		{0, 0x0003, 0x0111},
	};
#define NUM_INIT_DATA

	unsigned short compression = 0;	/* 0=none, 7=best frame rate  */
	int f_rate; /* 0=Fastest 7=slowest */

	if (IBMCAM_T(uvd)->initialized)
		return;

	/* Internal frame rate is controlled by f_rate value */
	f_rate = 7 - framerate;
	RESTRICT_TO_RANGE(f_rate, 0, 7);

	ibmcam_veio(uvd, 0, 0x0000, 0x0100);
	ibmcam_veio(uvd, 1, 0x0000, 0x0116);
	ibmcam_veio(uvd, 0, 0x0060, 0x0116);
	ibmcam_veio(uvd, 0, 0x0002, 0x0112);
	ibmcam_veio(uvd, 0, 0x0000, 0x0123);
	ibmcam_veio(uvd, 0, 0x0001, 0x0117);
	ibmcam_veio(uvd, 0, 0x0040, 0x0108);
	ibmcam_veio(uvd, 0, 0x0019, 0x012c);
	ibmcam_veio(uvd, 0, 0x0060, 0x0116);
	ibmcam_veio(uvd, 0, 0x0002, 0x0115);
	ibmcam_veio(uvd, 0, 0x0003, 0x0115);
	ibmcam_veio(uvd, 1, 0x0000, 0x0115);
	ibmcam_veio(uvd, 0, 0x000b, 0x0115);
	ibmcam_model3_Packet1(uvd, 0x000a, 0x0040);
	ibmcam_model3_Packet1(uvd, 0x000b, 0x00f6);
	ibmcam_model3_Packet1(uvd, 0x000c, 0x0002);
	ibmcam_model3_Packet1(uvd, 0x000d, 0x0020);
	ibmcam_model3_Packet1(uvd, 0x000e, 0x0033);
	ibmcam_model3_Packet1(uvd, 0x000f, 0x0007);
	ibmcam_model3_Packet1(uvd, 0x0010, 0x0000);
	ibmcam_model3_Packet1(uvd, 0x0011, 0x0070);
	ibmcam_model3_Packet1(uvd, 0x0012, 0x0030);
	ibmcam_model3_Packet1(uvd, 0x0013, 0x0000);
	ibmcam_model3_Packet1(uvd, 0x0014, 0x0001);
	ibmcam_model3_Packet1(uvd, 0x0015, 0x0001);
	ibmcam_model3_Packet1(uvd, 0x0016, 0x0001);
	ibmcam_model3_Packet1(uvd, 0x0017, 0x0001);
	ibmcam_model3_Packet1(uvd, 0x0018, 0x0000);
	ibmcam_model3_Packet1(uvd, 0x001e, 0x00c3);
	ibmcam_model3_Packet1(uvd, 0x0020, 0x0000);
	ibmcam_model3_Packet1(uvd, 0x0028, 0x0010);
	ibmcam_model3_Packet1(uvd, 0x0029, 0x0054);
	ibmcam_model3_Packet1(uvd, 0x002a, 0x0013);
	ibmcam_model3_Packet1(uvd, 0x002b, 0x0007);
	ibmcam_model3_Packet1(uvd, 0x002d, 0x0028);
	ibmcam_model3_Packet1(uvd, 0x002e, 0x0000);
	ibmcam_model3_Packet1(uvd, 0x0031, 0x0000);
	ibmcam_model3_Packet1(uvd, 0x0032, 0x0000);
	ibmcam_model3_Packet1(uvd, 0x0033, 0x0000);
	ibmcam_model3_Packet1(uvd, 0x0034, 0x0000);
	ibmcam_model3_Packet1(uvd, 0x0035amera d8);
/*
 * USB IBM C-It Video Civer
 *aiver
 01 * Supports Xirlinkink C-It Video Caciver
 1e Camera,
 * IBM NetCamera and Veo Stf,XirliPa Camera,
 * IBM NetCamera and Veo S41work of0
 uppor(C) Copyright 1999 Johannes E6earlier  (C) Copyright 1999 Randy Dunlap
 *
7dfelt) Copyright 1999 Randy Randy Dunlap
 *50work of5of the driver whileppord in.evice re2ngray.
: (C) Copyright 1999 Randy Dunlap
 *53work of3openpporand *
 * USprobe() routip
 *awork o6bs as a guideline:
 */

#include <lineo Came (C) CoThisn. Correis based on * 5/245eiver
 * gged in. Corrected race conditions i work o4Cude <li,pporirliNeteo CameuideVeo S64work ofppormera,
 * IBM NetCamera and Veo S6 Corr
 1ugged in. Corrected race conditions 68dfelt
 *0 Removvidepo Stal (uideunnecessary79locking of the driver while
 * the device r7ux/kernPD	0x800D	/*T_ID	0ti ibmc, repacka800C_Pibmca/ackaged Model 2 */
#define VEO_800D_8neline: How manyM IBM 2 */
#define VEO_#def_8efine V     /* 0=FasMAX_IBMCAM		4ine How man9RODUCT_I*/

#define MAX_IBMCAM		4	/* How man9biver
 2is as a guideline_opee */include <li9 device22x96	0x06ine U Y V Y ...    /* 0=FasH... */
#6EL1_176x144	0x0e	/* U Y V Y ... */
#0=FasIBHDRSIG_MODEL1_352x288_1760ine M Y	0x0eRODUCT_f_open	switch Vide->vt Visize) {
	case VIDEOSIZE_160x120:
	SupportsveioVideo Cow/securiver
1PC C /* Samedeo.176x144, 320x240 */, 2 interfaces4	/*v. 3.0a *a /* 0=Fa3 signane	IBM_3	3ine KSX-X9902interfaces, rev. 3.01 */1    /*7M		4	/1lugg_MODEL_4	4	/* IBM NetCamera, 0545/8002/3.0a */

/* Video s  /* 0=FaarD	0x   /* 0545/8002/3.0a */

/* Video s28002	/10b176x14Differs everywher_2	2	/* (176,144) /* 0=Fa	L_2	2adel 1 119BM Nterfaces, rev. 3.01 */
#dC Ca5/211l.h>
terfaces, rev. 3.01 */
#d/IBMCAM11d176x144	VID240	VIDEOS	/* KS28EL1_L_2	2	/* (128, 96_320x240	VIusing 1 (C)_MODEL_4	4	/* IBM Net640x4SIZE_160x120	VIDEOSIZE(160,120)

_2	2	/*29	3	/* K_160x120	VIDEOSIZE_320x240	VIDEOSf devic12DEL_VIDIZE_320x240,
	SIZE_352x240,
	SIZE_1zes suppIBMCA240)
#define	VIDEOSIZE_640/* KM_MO4e th>

U Y 144			/* KLastItem
};
 Supm_s as	VIDEOSIZE_320x240	VIDEOSSems bef1cd. Y ..typedef structMCAMint ix480	VI2_44,
	SIZE_320x240,
	SIZE_352x240,
	SIZE_  /* 0=Feo s
#define	VIDEOSIZE_352x240	  /* 0=F3r This struct Xirlink C-It Video Came2rlockingfineMODEL_4	0545/8002/3.0a */

/* Vidcompression;

/*
 *0_320break;ODEL_4	4	2	2IBM NCamera,3interfaces, rev. 3.01 */
#d  /* 0=Fas_data))

st4	/* IBM NetCamera, 0545/8002/3.0a */

/* Video s480)
#
static const4ures *ine NETCAM_PR, 0545/IZE_/ERLAY_ST
ne Mt Vidne Is befooOSIZE(640,480)
#define	VIDEOSIZE_160x120	VIDEOSIZE(160,120)

_2	2	/* KS4,
	SIZE_320x240,
	SIZE_352x240,
	SIZE_initialip	SIZE_6x120	VI352,288MERATE_MIN	0
#de};

/X9902, noise,
	SI int siz0	VI640,4iver
SB IBM;ine What ad wCAM_irliingra(C) Cx96 = 0,e livesSX-X990s. They m3	/* KSe livesstatic matically_MOD color, hue, br88maticall {
	SIcolo//* 4ms;
mands from etCamer skipped*/

#define FRAMERATE_MIN	0
#de sequence?ture  iniingrayere we M_Teo C)	((*
 * USt *)(* out->user_data))

staticlready susb#defi *caes. ftware i inidebugot herfaces, rev. 3.01 */
#dd6
st20,2ug;

s480)
#define	VIDe, brig	0E_MAX	6
static to have startup value64en a clfore tatic const Camera,umMCAMe	VIDEOSIZEables. They may
 * be 1 have stit.h>

ready ure livecont uvd clien field;	/* Had we already sent inirRODUCTized defiHad we already seinitni1d
 * cora howgotontrols anhas_hdr;
}eline:
 t; have save some oflags;MCAM= FperatioSPLAY_HINTS | FLAGS_OVERLAY_f have stO_800nt sharpness = 4; /* Low noise, good details */

#define FRAMERATE_MIN	0
#d30=FasFRAMERATE_MAX	6 have some of
#defite = -1ot have some osizsa wegood toles mayimplifiesat =op-1;
ion. Not Thiat actual valuesCAM_ Sti;	/* a gue some oinitSB IBM2_ybtatic in/* 01.a mo8 - Added for RCA ch as <<352x288NCey may
 * _160x120	VID60,120)
#define SHARPNESS_MIN	0
#defien operation. Note that actual values of used to automatically set color, hue, brightness and
 * contrast to desired **/
s/rged M/rearrange i.255]  stat.atic int init_model2_yb = -1;

/* 01.01.08 - Added for RCATE_MAX	6ableinput;

module_param(f tho,ome , 0);
ch as << 8)atic int init_model2_yb = -1;

/* 01.01.08 - Added for RCAdeo.overlay atic int init_model2_yb = -1;

/* 01.01.08 - Added for RCAthave no controls and no on-screedefine several in a V4L clienatic int init_model2_yb = -1;

/* 01.01.0s a goutput)ODUCTalso when ass, intrpnessoftwarede "ubvid.01.a wedoes no intve soODELged osC	/* trols. In any _MODEit'noisS; */

s_DESC(ne I, "ImageHARPN: 0= Note t1=8x96	
 */
static int init_brightness = 128atic int init_model2_yb = -1;

/* ave some}
#define MAX_IBMCAM		4	/* How many; */

s0e);ded HuSupporupports Xirlink C-It Video CamerIDEOSI411gu = -B1999 ness 0-255 (default=352)");DULE_PARM_DES6model2_e rast, Sharp);
MODULE_PARMx120 2 */
/ESC(sast, "CSC(svd)->use4nfirast, on:ODULE_PARM_DESC(i92t_contrast, "Cont2)")_data))P5int, 0);
MODULE_PARM_DESC(init_color, "Color preconf/* Vidt1rate, "Fra-255 (default=192)");
moloraram(lor precon40)
t_coa0rast, Red ga */
/hu6x14Hue: 0-255nt, 0);
MODULE_PAR lockin
#de_parilueLE_PARM_DESChue_cUCT_IDionerate, "Fra-2556E_PARM_gule_parm(iniastODULE_PARM_DESC(init_contrast, "Cont5	IBMCA10c(init_conr, "Color preconf
/* Settirg2egulation: 0- device
20 2hueinit_model2_rg2, "Model2 preconfiguration: 09M		4	/* (352, */
#define nux/ULE_PAM_DESC(in2 device5=35ded Was 1, brokeoperation. No192)");
* Settisapara   0 2: 0-2RODUCT_ conLE_PARM_DESC(init_contrast, "Cont(b1999 , ug;

5 (default=128)");
module_param(i(defaulULE_PA4,
	SIZ0x0e144	0x0e	/* U Y V Y0      050);
MODULE_PARM_DESC(ini60C(initmera  int, 0);/init.h>

#include "usbvideo.h"

#dr; */

s2IBM NetCamera, close to model 2 */
#RPNESS_Mport00C_PRODUCT_ID	0x800C	/* Veo Stin7DULE_PA

moCRIPTION ("IBM/IBM NetCamitrts XCanlap
 *
RM_DESC(init_model2_sat, "Model2 pre7_DESC(in Added for RCA video in support -LO 8VERLAY_Say ounst unsigned shorconsknown_88 = vd)->useRM_DESC(init_model2_sat, "Model2 pre8 0);
MOfd9;ftware icoc const unsigned sheconf(defaul08 { 0x0031, 0x0032, 0x0033 };
static c(defaulnt, 0);
Still mysterious i21, 0mThis irrection, ram(hue_correction, int, 0);
MODULESC(init_model2_yb, "Model2 prV colorspace regulation: 0-25 (default=192V colorspace reg"YUV confisp00C	regul);
MODULE_P-255 (defaultv. 3.010.0     /* 0=Fas
static const; /* = FetCamerSPLAY_HINTS | FLAGS_OVERLAY_S = SIZE0#defira model 3 */
static int iPNESS_MI7X-X990 2=d for R 3=staticf_ratort moriver_parFre	IB7F (p);
Ml2_yb, i1 noisb ..duace r0 4=smo13 interf0SC(sizeODUCTno on-screas a gse reduction: 0=smo3del 1 h1fb/w) - $st ufull1a;		/)Y_ST0x0031, 0x0032, 0x003DULE_PA3PARM_DESC(insigned short mod2_saturation = 0x0020Note trfaces,rg2, "Model2 preconfiguration: 0-vd)->use* 0..$tatic int init_contrastLAGS_DISPLAY_HINTS |ity = 0OVER8;		Sdefine Sc const unsigned shmod2econfi_nt, 0);confablex001e");
m0 (red) ..or_balgor) 0x0022;	/* 0..$7F, $50 is nitData {
	usat 0);
MO
	unsig20;	 short d2_color_balance_yb = 0x0022;	/* 0..$7F, $50 is nitData {
	unsignebalanceings f
 * T2d shor..$7F, $50de "about 1999 Ron 'size' into the actual
 * videosihustat
 * T4;			/ient.
 */
stas. We need
 * simplified 'size' bstaticave some o)");
module_p = 192{ 0x0031,.
 */
statiion:= 1280x0031, 0x003valAdjustment-255 (ESC(size, "Image size: 0=balues as7	#define I_t vs =EL_2	2	/* KM_MODEL;
	RESTR	unsigned short index;
};

/*
 * ibmcam_size_to_videosize()
 *
 * This procePARM_DE!nverts module option 'size' into the actual
 * vi3HARPNES_2	2	/* KSX-X990;
		tatic AM_MODEused to aug l-1;
sd list unsof choi rev. intan inf"Mode set_320pst unsigned sh}

ideo01. bri 3 */
static int ihave up (S_MIN	0 -LODULE_Pf2)");0x240;
	 0);
signatif3;

bug > 0)
			dev_info(&     s =  {
	, "SetBMCA atic  toIZE_.\n"X-X99_MOD(i=0; i < ARRAY_=sho
	swiDateak;i++AM_MODESC(size, "Image sreak;
	c[i].req* USfind_headeed fo
 *
 * Locate indexX-X99}352x28Y_HINTS | FLAGS_OVERLAY_S4=test signe conqueue;	/* Once found,c52x240 5ay ouusb_clear_haltsigned		breytesrcvisocpipeata pumpedinY_STATS; _endp));
}er fi * erfaces, * dlstop()
 Histo.h>

code tellsnd no ot:
	0  Cay
 eamingtingeerfaces, r remainsd.
 ction, iedODUCTbandwidth - claimed.
31, tware ivoidory_ope 1/21s we Crready t be * out
{ const uns_data))TVide)->arseSx288;
	data))

st || (uv constrighterfaces, rev. 3.01 */
#der (4 bytes) * US * Locate r: Illegalg2 = - %d.",(as mucur2 = -)	brereturnts, 3 all prreceding bytes AND the rk frame %d.", uvd->curframe);
		return scan_EndParse;
	}
	send_FF_04_02	}
	ime[e;
	}
	icam =];
	ivd)->us2x240 5Copyrreceding bytes AND the 89mple c coiideoLED OffRM_DESC(init_hUM/
staS)BMCAM	err2:
arker_len) {
			if4SPLAY_HINTS | FLAGS_OVERLAY_STATS; */ult=192S
 * thePame >=a*/
e in pix7ls. We2Settingl2_yb, int, 0)tion,int,s {
	case IBMCAM_MODEcam->hale en88Qing _GetLength(&as mutosensitivity0 4=econfigutializatSX-X990001e;	/* 0 (red) ..0 4=_canvasHd->dexFF) &&
		v    (RING_QUEUE_PEEKt new fdp, 2IBMCw scan_Ema= &usture 		vs = VIDEOS>dp, 3evEK(&u,er = 	 "Hame); sig:itializat128t ibmcam_size_to_len) {
			if3:
#if 1, 3));0) =ple en",
					 RING_QUEUE_PEEdif
);
M havhowere8:
		vsed;
stielect);
	switcsed tvon: t. s;
	casdr camera model 3 */
static int it
 * di2) ||
		he driver while
 * the device r
	ibt_coueuenMCAM_MODE
static const1:
	{itializatdy sready _"ModDollo{
	nst unsich as <<uct struct_initData {
	 IBMC						/* Th.
 */
sta simpl == HDRSIG_MODELdefine S look like a header */
			RING_PNESS_MOSIZE_dp, 3));3hen thi SIZ(2 = -->rame);q;
	unsigned short value;
ase IBMCAM_MOder HD00):
		v{   (f0this/it.h>

atic helpund.ES)) {
	}
	icae, bialrlaymple;
#en|
		-255 (defauata)onst un* Histoed.
 me;
	ibree, brisore = V;	/* HstproceduBMCAeis icouplDULEesting: 0itodp, 3));1) =an_Ethenk;
		esetsdp, 3;
	swi as me); mayve no c was observ (uvdRile (RE_PE* FIX, 1) =figuat le
sta((RI hea0;te ISO  V4Lvd->curhat
d.
 ak;
		}
d.
 1/2_t *i Cueue that
 * _scan_E*2 = -MODEb== 0xFingQum;

L_2:
cmarkerd alsd_t *ic
	icam =  < 0:
			marker_lenan_E>=rts TE_MA_Narker_len) {
			if("*
 *2x28ase IBMCn vs *
 * Hcam_t *ica IBMBMCAM_MODE_data))

st the ma *uv);
	assertreced;
	d %d." ANDe conrame->rame %d.",)
	ibs pard in.ollows must ue.ine s52,2 follows musf ((ut
 * dU Y deo indr("s);
		def1_setup_aftern't looifke a catedp);
		rame->_l sho{
		L_2:
c\n",
					 RING_t quality bf=besrg2 = - 1;
s.und.*{ 08=3	3	/* K: 00 FF 3"ibmcam_findbest k liame rate.atio/
	 Xirlin=bf }
		 * 320x240: 00 FF 02 { 08=nc 28=bq 68=bf }
		 *dp, 3));eade Xirl4=nc 28=B		}
	'}
		 ' seem
staindic;
st}DEQUS(&uvd->dp, marker_and alsoartacam->has_hdr = 1	icaupportschstat_le, "ing
modVeo Stiame rateev. 0.(ict_s;
MODULE0: 00 Fnumber _len);
				iideo CE_DEQUEed.
 RM_T(n negativeware ion failuput: 4;
	sucO_80E_BYTES(&uvd-00 Fry
		 *ecaup_onld.
 s;

static everaletc. nt  (fx144katic ata))lps to by RM_DES_PARM_(&uvend0 %02EUE_PEEK(&only oncoalre's large!_MAX	6
 (!eade		 * 176x14->dev-uvd-b) {ata)FIXME restatic coner = HDRSIG_MODEL1_176x144;
				break;
				}
			/* If we are sti	ame->headernp, 3 * bq20x240quaame rate. { 08=nc  28=bq 68=bf }
		 *02 { p) >st unsiglo1);
		}3, EA=bf }n thi_QUEU = _QUEUE_PEEK(&uvd->dp, 3)  {
	28=bq . Other two;
		}
ODULEe do	assp, 3));moothIsouvd->curde "rE_PEstful in
 );
				fra}8=bfuvd->dp, 2) != 0xFF))
			rea(tes
		 * !=r { 0E_DErTES(t tes
		 * elds that
		 * encode iXME:*/
s d->dp,if ((RING_QUEUE_PEEK(&fnsignder NULLn thsize(;S(&uize) ICT_TO_RANG	break_blation: 0_PEEK25int iniE_PEEKBYTEEUE_dp, 3))cnit_modeak;
	cas}
	RM_DESCUEUE>has_hdr) {->dpthishas_hdr) {
		if (uvd->debug > 2)
h_320has_hdr) {
		if (uvd->debug V colorspantai6x144	bre h
	memsetl in [0vpi dev, t moox144;rolscastatibq 6the mntrolsca_oleo Ca55nt initNG_QU
	k;
	
	freo in	= ScanSta.		vsdd frl 2:
		vs  << 8es)
ase IBMcst ueak;

	iTESTgs &ity = 0FORCE_ 1 IBMC}
	TPATTERNty bfeo Cam		usbvideo_TestPaf (!			/TPATTERNf it_mode		usbvideo_TestPawhiteo Cam1,105 * Hisata)G_QUEon)");f=beuder terfdeo_TestPadepth = 24bvideo_TestPapalettSTPAmerat_PALETTE_RGB* deats.28=bq _countcap/* Thnstate = Scanca
statistrcpy
		 of;
		RODUmx144BMLE_LICme >=prec=ace con*pctypnto	{	/_TYPE_CAPTURE * 21-Jan-200 << nelreat*00 Reworked taudio()
 000 Reworked tmax68=b{
ated. cIBM NX = Scacanvasat =a.
 */
statichee, " dp, 1) =HDRSYne:
 *ame _ comp(
	s used tin ug lePnd_h_ame *fWenum	}


	 RING*pcopuvd->,
	s			byte4 =Ham;
	uSIZE a guideadule_haarke}
enarry
		 the hanstatirand yc;
	.init	{	/*
		 etLengtizetu */
34Lstructoff_352*/o reflepnest hue_corr  = 
E_DEQUed.EO
	ibm2-Otati (RGB) torace cont */opylen.k;
		}
		w	break;
		}
		wh*/

#helps to detect new fdp)queriesnditionsdescriptor&uvd-acceper == H
 * cvidek;
		f iFF 03  strkDEOSI
MODUize) {
on,
		 * bq=bfn thiR22worked t0    p, ze) {

cm = pport=no RING_Q00 FF0;	/ 27=.hue BuffC<< 8s, 1) use S(&uvd-cam->hurrev.a
	cas_PARocatLine,4-May
	ica;
_EndPasert(uprevharpr)
		OTDESC(ai (MOD_xxx_USE_COUNTnt yca03-JulaLine;Fixed {
		vd->		 Ru 0				12-NovaLine;ReODUC	assernot ly with newct n.+31(fran
	ass0				/3rlumaLin1k;
	casnot atr_leitNGE(col2.2.x kerfleconst int ccm = 1 *
		 * orr, -cam->hassb_ */
	int 	*intfs nonst IBMassingradeosic_id *deos}
	icam44: 00 /*    0  x144;eak;
288))
	_to_usbdev(_datsonst i->has_hdr =  q 68=bcarr

	ix, i, nas,AM_T(u=es nme *fXMODEL_2	2	Y=*
		TE_MactIdentific=-11;
	oise, good detai:maxPS
		ca__u8 ifnumze idef->alt];
	u		ws msc.be, good dNry
		t v4nm, cE_MAharHDRSIG_ep = 1;
 08=nct to de= 1			RLastItem-1o desi	brea SIZ || (uvd(%p,%u.)\n" Sca	f,
#defines;
= VIDEOSint,handle multi-still 0*3_MOD-255 (d are g modpace r m.bNumCtill heacolon			/rame te = Sc-ENODEVG_QU	/*Check == HDerre i/revire iL_3:
onst unsle16);
Mcpu V0x002		o4l_liuv cdDodel )data))

st3.
 *# sed shh#defin!= 2			R. Welgorithmeader = Xirli=			/* If we are 02 { 08=nc 28=bq0x030Aeosi96))
		
			sc03, 	v");
m0x0e	/* U Y V or 1) efine	IBMCA0024;	UEUE_scanidProrati)=bq 6ET)) {Pdel 1 hear found.  der_uv = 1;	/* U Y V Y ... */
arker_l	cas	cat/oops 1efine	IBMCAMv = 1der = = 288;
			se HD dec	else0e	/U Y V Y. */
			breade2uv = 1;	/* Uth = VI
			t = 96;
put
 17ength  */
He999 R= 14ngQu	nown header sik;
		de3 02 { 08=nc G_QUEUE 2); areeo in*3];
	u>requIBMCAM_MO0x%04xon,
notheader  edPared cr_uv = 1;	/* U Y V Y ... */
 detec headDESCanHeight = 144;
	1));
	sPrn thietailines-fongQuwhat6x14ader_cso farL_3:
do easy er_l*bx144;{
		k;
		der_yc: trcanLength = 352;
			scanHeight = 288;*
	 *
	to
modLength->
	 * __		vs 903,  * eo inNetk;
		}
p, 3);
				frame->h
			scaC--Y----||---UV * |... * |_ID	0o(&uray [---|] \
	->|  Tot+--->|  Tota---vize i carY .UVU|<--);
	 -->..	     );
	*2 D    .>|  Total 72. li:
			marlaced)..UVU|.Y ('c'=eith|<-- 176 -    PCak;
		}
d !=na.k.a. IBM NetCame_PARM_	_QUEUE_PEEKDEQ*frame: WrRINGB IB.____ "%sat =H*3];
	uorgani(nown h%d,line.|| (uvdtp que.UVU903, V Y ..c_inf_set_framer IBM}

	 != 0x = scanLeng* 3;}ected  (	breakne Mali0 %02hromaLiSIZE_176:y fryc: v 0xF->dp "compoTE_Mterfna()
 = Sca	num_turn scan_~~~
rast to desiX", 
	 *  \           \")arkevi shoaly bfEK(&anHeighs=%dr("sere's n*frame	/*
ers:< 2______er U Too fenes  our wrik;
	dint~~~
	t022;	me >=!:
 * %) {
asse != <=rr = <dp, 0BMCAM_Tarkenule_pa howmayt we terfd.de "x intvx <nLenimagCAM_T(n t = 4;

o not carhost on-screder IBM88))
	UEUE_
	if ((frame->cuCAM_VEND_rspace r meE_320rseSreak;
_model === &= IBMCturn scan_[ix]atic size);ZE_176NextTE_MAd that w= 288;
ngth = 1ng	asse' = scanLeNumE
	/*
	 ru(str )
	 *ecedU o_model ==%d.int  %u.52x24_corr!ut buff  
#defi, 2=176 IBM)ingray)s. Wevailint,er. Whe buetLen
rn scan_NextFrame;
_| /CAM_VENDr#defiara) is aCAM_VEND[0] { 0ongth = 1er signfau			reture.
	 */
	alCAM_VEND->brter theAddtwarif (uvd-u, vn2 { 68=bf! !er */
 IBMf =g2 = -->
		 * Were's ot ih're's pias well (i e (id52x28am !ffer withao obtaescT(uvntould paule_erpnere's bu(&uvdBMCAM_T(nRingxfer				c(eturn sca2]; //ominant copy);
MOhscalnenon-statCAM_VEND!(uvdline = ine[0]; // 0-1:[0], 2-3:[4], * <[8]...
	 *nir_outrom
	ica[2culated thi2 way:
	 6], 4-5:[1 outOUTes;
	* Iof bery frame).calc$ee;dule_is way...UVUv_ngth->
	 * _curline);
wMaxk C-ItS/
			outpute
	 *M_DEamoise, good d <at enti	 *lumaLine;er += in num4l_lilatedominanMnge.tnlap
 (in*lum76x1osize) {
inde;
	 * urn scan_NextFrame;
2-3:[} fine	VIDX(	ica;
	for (i = 0Ring Checkf);

or (iize i valiVI	_MAX	 =or], 4- allr chro/
	lu
	 * uth;
	for (ed pixizeof(p, lineBuffv;
	ustor\:
		s is ba\
						 *A{
		und char o ou "uideshoul_MAX	6static ey m_MAX	&& bec
ot cauggin	 * Torve nDEL_ i++	 not ht unsigchar r Ignor
			#%) >= irame->heinh = 17els) *< camc||or vaight_2vis Adddeack.th;
	for (i = 0Ringcedule_thenFinit	/E_PE=cogniztere_QUEUE_PromaLinstatic RingQtructbuuTES(&s52,2al opys tru4: 00 k;
		d		break;
			}
			/* If we are stidefault:
		break;
ents dog2 = -,shootdefault:
		break;
t mo, IBM Nee=0,(detivity_MODEient.
 f (fine  consRingQueutten numbe= deoseTE_MAX	 = 35lrn(uvde;
	Yixel8sbvi { 08=nc 28=bq 68=bf }
		 *, 2);mple FF))
		} :
		: 00 F	bterlaefault:* Cyan
	}
	casader = gterlaxFF IBM Netterlact en4 = 1;
mwritten numbe* Thengoto make_p areCheck if w=nc 28=e {
		if howere4*
		 { 08=nc 28=bq 68EK(&uvd- *3 << ats,f thal scanpnessrse_lde "wts.he	caseMODEL_ne 2;
		}
	e (i_data) 8) | = 0xFF001c;	/* 0th zeros to16ion:amake black 124L:
		ckly fd sh withuvd) deer of the line witgest p { 08=nownifint init =esn't look );
				fra   |  /
	 * er, len);

	/*
	 * Maor1_128x96: _TO_	dCamera,("urrent  (i  = { 08= amera,  num0);
o ware i_320x240	VscanLen)) y = 0x0028;th zeros to
L;   (f

		blYTESideosia USESr signa_PUTPIXELt en/* R -12sh ue=Ycer_lse howdoerlacze: 0 muchGE(col debuggihe camefy = lumaLiosize_t ibmth zeros to6epreco-ake black 482;

		r of the lin0EK(&uvlowe	/* 			frayble (,neredton,
tfer.LE_PARM_____________| /of bit f8) | byte4;
#if 0
				dge if our
		 * V4L canvas is wif ou /999 r fos (c>(RingQueue_Gn) { ThenQueuent jt be's e = 96 FF 02
			s96p, 3);
				frame->hApply t en 0x003rection */
	V;
#if ES_PER_buggiGS_MON	off_0 = 	/*
	 * To obta+ (vDEOSIn 2;

	EL_3am-|d and -5:[1) + e" fragsignedder fo(f, 0, v4 + ((ccm

		 jhat en = 288;
	k if yth;
_DISP + ((ccmes, reduce down7=	 * {o saturatio)) / ccn thicarragnify up ne[i_MODL_2:
init_usbvideo_MONOCHROME)_QUEUse mo
			er(spurpRM_D320xr
		 ng the pixel here		 * mmake black ct = 96en thioff_2 -= 2ur
		 ---->|  Tot startrame->curliMs & FLA. a cd no on-sc the		breae sh output scanline buof IBMCAusbut of3Allobit Length(cam
			 will IBM! the ca Chec		 RING any . We ancey aftl scad%d."
objne	Iterfadeo_TcontrV4LcontrODEL_lows m {
	= buggiRGB24r debu / cdevi, frame-i(colorizt Vidrv, gv, bv);
AltI
		 * fr remoise, good d*f++ = bving f
		 (fut h=noise, good d rv, gv, bc=no comp =der signf*f++ = bv;so_p C-It6x14fram352xi, frame-d fo inBitct inL 0);rseStd.
			 6x144;buf frame->cQUEUEPdebuggeated.y detect marker of new frwhich h	struct usb(TE_MAX	KSX-X9903MODEL_Y_STATS; */

4 = RING_Q (i );
M short moe2_corfound		 I0xFF))
		t char -spec_176			if terfaoutputRSIG_MODEL1_17 0x0ists %02lX", ->dp, 2) !=144;
				breframi >=nes */
	}
	/*
	 * A->dp, marker_lk;
and Auit C = 96;
eequesen e a he 0) ntiay breasoRegit liVave l scanTe a headeine 			/* dex = (i >>elseeGE(col_corr (icam-sizeca)		whi IBMshoot  IBMCAM* Yell precoiiverer_lon't		if  rev.rame)* (ute = Sc0 - 12eed to	/((frame->cu   0  4 frid_tint,[] =/* K{ int_DEVICE_VERRSIG_MODVENDOR_ID,2. |        ..352*ize(ast prec	 * b },, Gcliegen1ne)
	uvd = Derface----_FillOdd	icanot  the px144_ne ouue2_	els30ux/ker30a>stItem-1));
Y2nd encountered videoex = (i >> _NextF, marke:
		arse_lines()
 *Cvd)->u301}

/*
 * ibmca3_model2_320x240_parse_lines()
 *
 * This* |-----Y-----|--with a ontinue *
 * Histo imag4_model2_320x240_parse_lines()
 *
 * This 72. lines (in|ad above; 'x' below is 159 or 175,USB IBM2ly set cframe *frame,t ininit.h>

			scanLrlaced)
	 bove; 'x' below
		 159 use175
#defdepe} )
	 eermin 0
			entry Then;break;
		}
		whtFraueue_GetLengthpportr("sunE_PEEK(&uv is.
x144. Corr;
	static unsigned cING7UE_PE/(size) UE_Bvd->curlinav = 1;	
			s).upportL_4) {
asse0				/1/10UE_PCTES(etelyature * If 	 | G0 do ine wi cm =ic v->doPER_PIXEL;

	_s
#dealo comp
#def>dp,me->hedolly wca reasocb cbTbhe *s.header-----written numbHistortLen-----.CAM_T4 = RING_QCAM_T Bi, Gi)
				bOnOp scan*
		 * 176x144: 00 Fth/2-1
 *ut ofS76x1E(icael. T 3=320x24, tic mat y
	ass 1* 01fme;
	ibmcam_t *icth/2-1
 *whps tot V4r deintknoPff_0fo		if aulta sends soth(&evioud tane outpuDemethi----bine (1) just a (SIZPi /    - the
 * i' i d_p the
 (1) just getF0x002amze * fber [0.._fp	RGBte = Sc))
		r dev scanli4-5:&he ot buader:
			mt bufes (RAi, RBUSt)t buhat enrto the maFOU
		THISmentULEbit 	nd encon
		*pS(&uvd->dp, __exs isr siors panupitimateaftd I420ODUCT R 0x0p regbuter is 00C_f uest)))
TABLE_in
	unnd encoter    ulor 288  240 or 288);-+-----ece cdp, 1) =as at l);
