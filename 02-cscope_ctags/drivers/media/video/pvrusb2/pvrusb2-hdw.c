/*
 *
 *
 *  Copyright (C) 2005 Mike Isely <isely@pobox.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/errno.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/videodev2.h>
#include <media/v4l2-common.h>
#include <media/tuner.h>
#include "pvrusb2.h"
#include "pvrusb2-std.h"
#include "pvrusb2-util.h"
#include "pvrusb2-hdw.h"
#include "pvrusb2-i2c-core.h"
#include "pvrusb2-eeprom.h"
#include "pvrusb2-hdw-internal.h"
#include "pvrusb2-encoder.h"
#include "pvrusb2-debug.h"
#include "pvrusb2-fx2-cmd.h"
#include "pvrusb2-wm8775.h"
#include "pvrusb2-video-v4l.h"
#include "pvrusb2-cx2584x-v4l.h"
#include "pvrusb2-cs53l32a.h"
#include "pvrusb2-audio.h"

#define TV_MIN_FREQ     55250000L
#define TV_MAX_FREQ    850000000L

/* This defines a minimum interval that the decoder must remain quiet
   before we are allowed to start it running. */
#define TIME_MSEC_DECODER_WAIT 50

/* This defines a minimum interval that the encoder must remain quiet
   before we are allowed to configure it.  I had this originally set to
   50msec, but Martin Dauskardt <martin.dauskardt@gmx.de> reports that
   things work better when it's set to 100msec. */
#define TIME_MSEC_ENCODER_WAIT 100

/* This defines the minimum interval that the encoder must successfully run
   before we consider that the encoder has run at least once since its
   firmware has been loaded.  This measurement is in important for cases
   where we can't do something until we know that the encoder has been run
   at least once. */
#define TIME_MSEC_ENCODER_OK 250

static struct pvr2_hdw *unit_pointers[PVR_NUM] = {[ 0 ... PVR_NUM-1 ] = NULL};
static DEFINE_MUTEX(pvr2_unit_mtx);

static int ctlchg;
static int procreload;
static int tuner[PVR_NUM] = { [0 ... PVR_NUM-1] = -1 };
static int tolerance[PVR_NUM] = { [0 ... PVR_NUM-1] = 0 };
static int video_std[PVR_NUM] = { [0 ... PVR_NUM-1] = 0 };
static int init_pause_msec;

module_param(ctlchg, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(ctlchg, "0=optimize ctl change 1=always accept new ctl value");
module_param(init_pause_msec, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(init_pause_msec, "hardware initialization settling delay");
module_param(procreload, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(procreload,
		 "Attempt init failure recovery with firmware reload");
module_param_array(tuner,    int, NULL, 0444);
MODULE_PARM_DESC(tuner,"specify installed tuner type");
module_param_array(video_std,    int, NULL, 0444);
MODULE_PARM_DESC(video_std,"specify initial video standard");
module_param_array(tolerance,    int, NULL, 0444);
MODULE_PARM_DESC(tolerance,"specify stream error tolerance");

/* US Broadcast channel 3 (61.25 MHz), to help with testing */
static int default_tv_freq    = 61250000L;
/* 104.3 MHz, a usable FM station for my area */
static int default_radio_freq = 104300000L;

module_param_named(tv_freq, default_tv_freq, int, 0444);
MODULE_PARM_DESC(tv_freq, "specify initial television frequency");
module_param_named(radio_freq, default_radio_freq, int, 0444);
MODULE_PARM_DESC(radio_freq, "specify initial radio frequency");

#define PVR2_CTL_WRITE_ENDPOINT  0x01
#define PVR2_CTL_READ_ENDPOINT   0x81

#define PVR2_GPIO_IN 0x9008
#define PVR2_GPIO_OUT 0x900c
#define PVR2_GPIO_DIR 0x9020

#define trace_firmware(...) pvr2_trace(PVR2_TRACE_FIRMWARE,__VA_ARGS__)

#define PVR2_FIRMWARE_ENDPOINT   0x02

/* size of a firmware chunk */
#define FIRMWARE_CHUNK_SIZE 0x2000

typedef void (*pvr2_subdev_update_func)(struct pvr2_hdw *,
					struct v4l2_subdev *);

static const pvr2_subdev_update_func pvr2_module_update_functions[] = {
	[PVR2_CLIENT_ID_WM8775] = pvr2_wm8775_subdev_update,
	[PVR2_CLIENT_ID_SAA7115] = pvr2_saa7115_subdev_update,
	[PVR2_CLIENT_ID_MSP3400] = pvr2_msp3400_subdev_update,
	[PVR2_CLIENT_ID_CX25840] = pvr2_cx25840_subdev_update,
	[PVR2_CLIENT_ID_CS53L32A] = pvr2_cs53l32a_subdev_update,
};

static const char *module_names[] = {
	[PVR2_CLIENT_ID_MSP3400] = "msp3400",
	[PVR2_CLIENT_ID_CX25840] = "cx25840",
	[PVR2_CLIENT_ID_SAA7115] = "saa7115",
	[PVR2_CLIENT_ID_TUNER] = "tuner",
	[PVR2_CLIENT_ID_DEMOD] = "tuner",
	[PVR2_CLIENT_ID_CS53L32A] = "cs53l32a",
	[PVR2_CLIENT_ID_WM8775] = "wm8775",
};


static const unsigned char *module_i2c_addresses[] = {
	[PVR2_CLIENT_ID_TUNER] = "\x60\x61\x62\x63",
	[PVR2_CLIENT_ID_DEMOD] = "\x43",
	[PVR2_CLIENT_ID_MSP3400] = "\x40",
	[PVR2_CLIENT_ID_SAA7115] = "\x21",
	[PVR2_CLIENT_ID_WM8775] = "\x1b",
	[PVR2_CLIENT_ID_CX25840] = "\x44",
	[PVR2_CLIENT_ID_CS53L32A] = "\x11",
};


static const char *ir_scheme_names[] = {
	[PVR2_IR_SCHEME_NONE] = "none",
	[PVR2_IR_SCHEME_29XXX] = "29xxx",
	[PVR2_IR_SCHEME_24XXX] = "24xxx (29xxx emulation)",
	[PVR2_IR_SCHEME_24XXX_MCE] = "24xxx (MCE device)",
	[PVR2_IR_SCHEME_ZILOG] = "Zilog",
};


/* Define the list of additional controls we'll dynamically construct based
   on query of the cx2341x module. */
struct pvr2_mpeg_ids {
	const char *strid;
	int id;
};
static const struct pvr2_mpeg_ids mpeg_ids[] = {
	{
		.strid = "audio_layer",
		.id = V4L2_CID_MPEG_AUDIO_ENCODING,
	},{
		.strid = "audio_bitrate",
		.id = V4L2_CID_MPEG_AUDIO_L2_BITRATE,
	},{
		/* Already using audio_mode elsewhere :-( */
		.strid = "mpeg_audio_mode",
		.id = V4L2_CID_MPEG_AUDIO_MODE,
	},{
		.strid = "mpeg_audio_mode_extension",
		.id = V4L2_CID_MPEG_AUDIO_MODE_EXTENSION,
	},{
		.strid = "audio_emphasis",
		.id = V4L2_CID_MPEG_AUDIO_EMPHASIS,
	},{
		.strid = "audio_crc",
		.id = V4L2_CID_MPEG_AUDIO_CRC,
	},{
		.strid = "video_aspect",
		.id = V4L2_CID_MPEG_VIDEO_ASPECT,
	},{
		.strid = "video_b_frames",
		.id = V4L2_CID_MPEG_VIDEO_B_FRAMES,
	},{
		.strid = "video_gop_size",
		.id = V4L2_CID_MPEG_VIDEO_GOP_SIZE,
	},{
		.strid = "video_gop_closure",
		.id = V4L2_CID_MPEG_VIDEO_GOP_CLOSURE,
	},{
		.strid = "video_bitrate_mode",
		.id = V4L2_CID_MPEG_VIDEO_BITRATE_MODE,
	},{
		.strid = "video_bitrate",
		.id = V4L2_CID_MPEG_VIDEO_BITRATE,
	},{
		.strid = "video_bitrate_peak",
		.id = V4L2_CID_MPEG_VIDEO_BITRATE_PEAK,
	},{
		.strid = "video_temporal_decimation",
		.id = V4L2_CID_MPEG_VIDEO_TEMPORAL_DECIMATION,
	},{
		.strid = "stream_type",
		.id = V4L2_CID_MPEG_STREAM_TYPE,
	},{
		.strid = "video_spatial_filter_mode",
		.id = V4L2_CID_MPEG_CX2341X_VIDEO_SPATIAL_FILTER_MODE,
	},{
		.strid = "video_spatial_filter",
		.id = V4L2_CID_MPEG_CX2341X_VIDEO_SPATIAL_FILTER,
	},{
		.strid = "video_luma_spatial_filter_type",
		.id = V4L2_CID_MPEG_CX2341X_VIDEO_LUMA_SPATIAL_FILTER_TYPE,
	},{
		.strid = "video_chroma_spatial_filter_type",
		.id = V4L2_CID_MPEG_CX2341X_VIDEO_CHROMA_SPATIAL_FILTER_TYPE,
	},{
		.strid = "video_temporal_filter_mode",
		.id = V4L2_CID_MPEG_CX2341X_VIDEO_TEMPORAL_FILTER_MODE,
	},{
		.strid = "video_temporal_filter",
		.id = V4L2_CID_MPEG_CX2341X_VIDEO_TEMPORAL_FILTER,
	},{
		.strid = "video_median_filter_type",
		.id = V4L2_CID_MPEG_CX2341X_VIDEO_MEDIAN_FILTER_TYPE,
	},{
		.strid = "video_luma_median_filter_top",
		.id = V4L2_CID_MPEG_CX2341X_VIDEO_LUMA_MEDIAN_FILTER_TOP,
	},{
		.strid = "video_luma_median_filter_bottom",
		.id = V4L2_CID_MPEG_CX2341X_VIDEO_LUMA_MEDIAN_FILTER_BOTTOM,
	},{
		.strid = "video_chroma_median_filter_top",
		.id = V4L2_CID_MPEG_CX2341X_VIDEO_CHROMA_MEDIAN_FILTER_TOP,
	},{
		.strid = "video_chroma_median_filter_bottom",
		.id = V4L2_CID_MPEG_CX2341X_VIDEO_CHROMA_MEDIAN_FILTER_BOTTOM,
	}
};
#define MPEGDEF_COUNT ARRAY_SIZE(mpeg_ids)


static const char *control_values_srate[] = {
	[V4L2_MPEG_AUDIO_SAMPLING_FREQ_44100]   = "44.1 kHz",
	[V4L2_MPEG_AUDIO_SAMPLING_FREQ_48000]   = "48 kHz",
	[V4L2_MPEG_AUDIO_SAMPLING_FREQ_32000]   = "32 kHz",
};



static const char *control_values_input[] = {
	[PVR2_CVAL_INPUT_TV]        = "television",  /*xawtv needs this name*/
	[PVR2_CVAL_INPUT_DTV]       = "dtv",
	[PVR2_CVAL_INPUT_RADIO]     = "radio",
	[PVR2_CVAL_INPUT_SVIDEO]    = "s-video",
	[PVR2_CVAL_INPUT_COMPOSITE] = "composite",
};


static const char *control_values_audiomode[] = {
	[V4L2_TUNER_MODE_MONO]   = "Mono",
	[V4L2_TUNER_MODE_STEREO] = "Stereo",
	[V4L2_TUNER_MODE_LANG1]  = "Lang1",
	[V4L2_TUNER_MODE_LANG2]  = "Lang2",
	[V4L2_TUNER_MODE_LANG1_LANG2] = "Lang1+Lang2",
};


static const char *control_values_hsm[] = {
	[PVR2_CVAL_HSM_FAIL] = "Fail",
	[PVR2_CVAL_HSM_HIGH] = "High",
	[PVR2_CVAL_HSM_FULL] = "Full",
};


static const char *pvr2_state_names[] = {
	[PVR2_STATE_NONE] =    "none",
	[PVR2_STATE_DEAD] =    "dead",
	[PVR2_STATE_COLD] =    "cold",
	[PVR2_STATE_WARM] =    "warm",
	[PVR2_STATE_ERROR] =   "error",
	[PVR2_STATE_READY] =   "ready",
	[PVR2_STATE_RUN] =     "run",
};


struct pvr2_fx2cmd_descdef {
	unsigned char id;
	unsigned char *desc;
};

static const struct pvr2_fx2cmd_descdef pvr2_fx2cmd_desc[] = {
	{FX2CMD_MEM_WRITE_DWORD, "write encoder dword"},
	{FX2CMD_MEM_READ_DWORD, "read encoder dword"},
	{FX2CMD_HCW_ZILOG_RESET, "zilog IR reset control"},
	{FX2CMD_MEM_READ_64BYTES, "read encoder 64bytes"},
	{FX2CMD_REG_WRITE, "write encoder register"},
	{FX2CMD_REG_READ, "read encoder register"},
	{FX2CMD_MEMSEL, "encoder memsel"},
	{FX2CMD_I2C_WRITE, "i2c write"},
	{FX2CMD_I2C_READ, "i2c read"},
	{FX2CMD_GET_USB_SPEED, "get USB speed"},
	{FX2CMD_STREAMING_ON, "stream on"},
	{FX2CMD_STREAMING_OFF, "stream off"},
	{FX2CMD_FWPOST1, "fwpost1"},
	{FX2CMD_POWER_OFF, "power off"},
	{FX2CMD_POWER_ON, "power on"},
	{FX2CMD_DEEP_RESET, "deep reset"},
	{FX2CMD_GET_EEPROM_ADDR, "get rom addr"},
	{FX2CMD_GET_IR_CODE, "get IR code"},
	{FX2CMD_HCW_DEMOD_RESETIN, "hcw demod resetin"},
	{FX2CMD_HCW_DTV_STREAMING_ON, "hcw dtv stream on"},
	{FX2CMD_HCW_DTV_STREAMING_OFF, "hcw dtv stream off"},
	{FX2CMD_ONAIR_DTV_STREAMING_ON, "onair dtv stream on"},
	{FX2CMD_ONAIR_DTV_STREAMING_OFF, "onair dtv stream off"},
	{FX2CMD_ONAIR_DTV_POWER_ON, "onair dtv power on"},
	{FX2CMD_ONAIR_DTV_POWER_OFF, "onair dtv power off"},
};


static int pvr2_hdw_set_input(struct pvr2_hdw *hdw,int v);
static void pvr2_hdw_state_sched(struct pvr2_hdw *);
static int pvr2_hdw_state_eval(struct pvr2_hdw *);
static void pvr2_hdw_set_cur_freq(struct pvr2_hdw *,unsigned long);
static void pvr2_hdw_worker_poll(struct work_struct *work);
static int pvr2_hdw_wait(struct pvr2_hdw *,int state);
static int pvr2_hdw_untrip_unlocked(struct pvr2_hdw *);
static void pvr2_hdw_state_log_state(struct pvr2_hdw *);
static int pvr2_hdw_cmd_usbstream(struct pvr2_hdw *hdw,int runFl);
static int pvr2_hdw_commit_setup(struct pvr2_hdw *hdw);
static int pvr2_hdw_get_eeprom_addr(struct pvr2_hdw *hdw);
static void pvr2_hdw_internal_find_stdenum(struct pvr2_hdw *hdw);
static void pvr2_hdw_internal_set_std_avail(struct pvr2_hdw *hdw);
static void pvr2_hdw_quiescent_timeout(unsigned long);
static void pvr2_hdw_encoder_wait_timeout(unsigned long);
static void pvr2_hdw_encoder_run_timeout(unsigned long);
static int pvr2_issue_simple_cmd(struct pvr2_hdw *,u32);
static int pvr2_send_request_ex(struct pvr2_hdw *hdw,
				unsigned int timeout,int probe_fl,
				void *write_data,unsigned int write_len,
				void *read_data,unsigned int read_len);
static int pvr2_hdw_check_cropcap(struct pvr2_hdw *hdw);


static void trace_stbit(const char *name,int val)
{
	pvr2_trace(PVR2_TRACE_STBITS,
		   "State bit %s <-- %s",
		   name,(val ? "true" : "false"));
}

static int ctrl_channelfreq_get(struct pvr2_ctrl *cptr,int *vp)
{
	struct pvr2_hdw *hdw = cptr->hdw;
	if ((hdw->freqProgSlot > 0) && (hdw->freqProgSlot <= FREQTABLE_SIZE)) {
		*vp = hdw->freqTable[hdw->freqProgSlot-1];
	} else {
		*vp = 0;
	}
	return 0;
}

static int ctrl_channelfreq_set(struct pvr2_ctrl *cptr,int m,int v)
{
	struct pvr2_hdw *hdw = cptr->hdw;
	unsigned int slotId = hdw->freqProgSlot;
	if ((slotId > 0) && (slotId <= FREQTABLE_SIZE)) {
		hdw->freqTable[slotId-1] = v;
		/* Handle side effects correctly - if we're tuned to this
		   slot, then forgot the slot id relation since the stored
		   frequency has been changed. */
		if (hdw->freqSelector) {
			if (hdw->freqSlotRadio == slotId) {
				hdw->freqSlotRadio = 0;
			}
		} else {
			if (hdw->freqSlotTelevision == slotId) {
				hdw->freqSlotTelevision = 0;
			}
		}
	}
	return 0;
}

static int ctrl_channelprog_get(struct pvr2_ctrl *cptr,int *vp)
{
	*vp = cptr->hdw->freqProgSlot;
	return 0;
}

static int ctrl_channelprog_set(struct pvr2_ctrl *cptr,int m,int v)
{
	struct pvr2_hdw *hdw = cptr->hdw;
	if ((v >= 0) && (v <= FREQTABLE_SIZE)) {
		hdw->freqProgSlot = v;
	}
	return 0;
}

static int ctrl_channel_get(struct pvr2_ctrl *cptr,int *vp)
{
	struct pvr2_hdw *hdw = cptr->hdw;
	*vp = hdw->freqSelector ? hdw->freqSlotRadio : hdw->freqSlotTelevision;
	return 0;
}

static int ctrl_channel_set(struct pvr2_ctrl *cptr,int m,int slotId)
{
	unsigned freq = 0;
	struct pvr2_hdw *hdw = cptr->hdw;
	if ((slotId < 0) || (slotId > FREQTABLE_SIZE)) return 0;
	if (slotId > 0) {
		freq = hdw->freqTable[slotId-1];
		if (!freq) return 0;
		pvr2_hdw_set_cur_freq(hdw,freq);
	}
	if (hdw->freqSelector) {
		hdw->freqSlotRadio = slotId;
	} else {
		hdw->freqSlotTelevision = slotId;
	}
	return 0;
}

static int ctrl_freq_get(struct pvr2_ctrl *cptr,int *vp)
{
	*vp = pvr2_hdw_get_cur_freq(cptr->hdw);
	return 0;
}

static int ctrl_freq_is_dirty(struct pvr2_ctrl *cptr)
{
	return cptr->hdw->freqDirty != 0;
}

static void ctrl_freq_clear_dirty(struct pvr2_ctrl *cptr)
{
	cptr->hdw->freqDirty = 0;
}

static int ctrl_freq_set(struct pvr2_ctrl *cptr,int m,int v)
{
	pvr2_hdw_set_cur_freq(cptr->hdw,v);
	return 0;
}

static int ctrl_cropl_min_get(struct pvr2_ctrl *cptr, int *left)
{
	struct v4l2_cropcap *cap = &cptr->hdw->cropcap_info;
	int stat = pvr2_hdw_check_cropcap(cptr->hdw);
	if (stat != 0) {
		return stat;
	}
	*left = cap->bounds.left;
	return 0;
}

static int ctrl_cropl_max_get(struct pvr2_ctrl *cptr, int *left)
{
	struct v4l2_cropcap *cap = &cptr->hdw->cropcap_info;
	int stat = pvr2_hdw_check_cropcap(cptr->hdw);
	if (stat != 0) {
		return stat;
	}
	*left = cap->bounds.left;
	if (cap->bounds.width > cptr->hdw->cropw_val) {
		*left += cap->bounds.width - cptr->hdw->cropw_val;
	}
	return 0;
}

static int ctrl_cropt_min_get(struct pvr2_ctrl *cptr, int *top)
{
	struct v4l2_cropcap *cap = &cptr->hdw->cropcap_info;
	int stat = pvr2_hdw_check_cropcap(cptr->hdw);
	if (stat != 0) {
		return stat;
	}
	*top = cap->bounds.top;
	return 0;
}

static int ctrl_cropt_max_get(struct pvr2_ctrl *cptr, int *top)
{
	struct v4l2_cropcap *cap = &cptr->hdw->cropcap_info;
	int stat = pvr2_hdw_check_cropcap(cptr->hdw);
	if (stat != 0) {
		return stat;
	}
	*top = cap->bounds.top;
	if (cap->bounds.height > cptr->hdw->croph_val) {
		*top += cap->bounds.height - cptr->hdw->croph_val;
	}
	return 0;
}

static int ctrl_cropw_max_get(struct pvr2_ctrl *cptr, int *val)
{
	struct v4l2_cropcap *cap = &cptr->hdw->cropcap_info;
	int stat = pvr2_hdw_check_cropcap(cptr->hdw);
	if (stat != 0) {
		return stat;
	}
	*val = 0;
	if (cap->bounds.width > cptr->hdw->cropl_val) {
		*val = cap->bounds.width - cptr->hdw->cropl_val;
	}
	return 0;
}

static int ctrl_croph_max_get(struct pvr2_ctrl *cptr, int *val)
{
	struct v4l2_cropcap *cap = &cptr->hdw->cropcap_info;
	int stat = pvr2_hdw_check_cropcap(cptr->hdw);
	if (stat != 0) {
		return stat;
	}
	*val = 0;
	if (cap->bounds.height > cptr->hdw->cropt_val) {
		*val = cap->bounds.height - cptr->hdw->cropt_val;
	}
	return 0;
}

static int ctrl_get_cropcapbl(struct pvr2_ctrl *cptr, int *val)
{
	struct v4l2_cropcap *cap = &cptr->hdw->cropcap_info;
	int stat = pvr2_hdw_check_cropcap(cptr->hdw);
	if (stat != 0) {
		return stat;
	}
	*val = cap->bounds.left;
	return 0;
}

static int ctrl_get_cropcapbt(struct pvr2_ctrl *cptr, int *val)
{
	struct v4l2_cropcap *cap = &cptr->hdw->cropcap_info;
	int stat = pvr2_hdw_check_cropcap(cptr->hdw);
	if (stat != 0) {
		return stat;
	}
	*val = cap->bounds.top;
	return 0;
}

static int ctrl_get_cropcapbw(struct pvr2_ctrl *cptr, int *val)
{
	struct v4l2_cropcap *cap = &cptr->hdw->cropcap_info;
	int stat = pvr2_hdw_check_cropcap(cptr->hdw);
	if (stat != 0) {
		return stat;
	}
	*val = cap->bounds.width;
	return 0;
}

static int ctrl_get_cropcapbh(struct pvr2_ctrl *cptr, int *val)
{
	struct v4l2_cropcap *cap = &cptr->hdw->cropcap_info;
	int stat = pvr2_hdw_check_cropcap(cptr->hdw);
	if (stat != 0) {
		return stat;
	}
	*val = cap->bounds.height;
	return 0;
}

static int ctrl_get_cropcapdl(struct pvr2_ctrl *cptr, int *val)
{
	struct v4l2_cropcap *cap = &cptr->hdw->cropcap_info;
	int stat = pvr2_hdw_check_cropcap(cptr->hdw);
	if (stat != 0) {
		return stat;
	}
	*val = cap->defrect.left;
	return 0;
}

static int ctrl_get_cropcapdt(struct pvr2_ctrl *cptr, int *val)
{
	struct v4l2_cropcap *cap = &cptr->hdw->cropcap_info;
	int stat = pvr2_hdw_check_cropcap(cptr->hdw);
	if (stat != 0) {
		return stat;
	}
	*val = cap->defrect.top;
	return 0;
}

static int ctrl_get_cropcapdw(struct pvr2_ctrl *cptr, int *val)
{
	struct v4l2_cropcap *cap = &cptr->hdw->cropcap_info;
	int stat = pvr2_hdw_check_cropcap(cptr->hdw);
	if (stat != 0) {
		return stat;
	}
	*val = cap->defrect.width;
	return 0;
}

static int ctrl_get_cropcapdh(struct pvr2_ctrl *cptr, int *val)
{
	struct v4l2_cropcap *cap = &cptr->hdw->cropcap_info;
	int stat = pvr2_hdw_check_cropcap(cptr->hdw);
	if (stat != 0) {
		return stat;
	}
	*val = cap->defrect.height;
	return 0;
}

static int ctrl_get_cropcappan(struct pvr2_ctrl *cptr, int *val)
{
	struct v4l2_cropcap *cap = &cptr->hdw->cropcap_info;
	int stat = pvr2_hdw_check_cropcap(cptr->hdw);
	if (stat != 0) {
		return stat;
	}
	*val = cap->pixelaspect.numerator;
	return 0;
}

static int ctrl_get_cropcappad(struct pvr2_ctrl *cptr, int *val)
{
	struct v4l2_cropcap *cap = &cptr->hdw->cropcap_info;
	int stat = pvr2_hdw_check_cropcap(cptr->hdw);
	if (stat != 0) {
		return stat;
	}
	*val = cap->pixelaspect.denominator;
	return 0;
}

static int ctrl_vres_max_get(struct pvr2_ctrl *cptr,int *vp)
{
	/* Actual maximum depends on the video standard in effect. */
	if (cptr->hdw->std_mask_cur & V4L2_STD_525_60) {
		*vp = 480;
	} else {
		*vp = 576;
	}
	return 0;
}

static int ctrl_vres_min_get(struct pvr2_ctrl *cptr,int *vp)
{
	/* Actual minimum depends on device digitizer type. */
	if (cptr->hdw->hdw_desc->flag_has_cx25840) {
		*vp = 75;
	} else {
		*vp = 17;
	}
	return 0;
}

static int ctrl_get_input(struct pvr2_ctrl *cptr,int *vp)
{
	*vp = cptr->hdw->input_val;
	return 0;
}

static int ctrl_check_input(struct pvr2_ctrl *cptr,int v)
{
	return ((1 << v) & cptr->hdw->input_allowed_mask) != 0;
}

static int ctrl_set_input(struct pvr2_ctrl *cptr,int m,int v)
{
	return pvr2_hdw_set_input(cptr->hdw,v);
}

static int ctrl_isdirty_input(struct pvr2_ctrl *cptr)
{
	return cptr->hdw->input_dirty != 0;
}

static void ctrl_cleardirty_input(struct pvr2_ctrl *cptr)
{
	cptr->hdw->input_dirty = 0;
}


static int ctrl_freq_max_get(struct pvr2_ctrl *cptr, int *vp)
{
	unsigned long fv;
	struct pvr2_hdw *hdw = cptr->hdw;
	if (hdw->tuner_signal_stale) {
		pvr2_hdw_status_poll(hdw);
	}
	fv = hdw->tuner_signal_info.rangehigh;
	if (!fv) {
		/* Safety fallback */
		*vp = TV_MAX_FREQ;
		return 0;
	}
	if (hdw->tuner_signal_info.capability & V4L2_TUNER_CAP_LOW) {
		fv = (fv * 125) / 2;
	} else {
		fv = fv * 62500;
	}
	*vp = fv;
	return 0;
}

static int ctrl_freq_min_get(struct pvr2_ctrl *cptr, int *vp)
{
	unsigned long fv;
	struct pvr2_hdw *hdw = cptr->hdw;
	if (hdw->tuner_signal_stale) {
		pvr2_hdw_status_poll(hdw);
	}
	fv = hdw->tuner_signal_info.rangelow;
	if (!fv) {
		/* Safety fallback */
		*vp = TV_MIN_FREQ;
		return 0;
	}
	if (hdw->tuner_signal_info.capability & V4L2_TUNER_CAP_LOW) {
		fv = (fv * 125) / 2;
	} else {
		fv = fv * 62500;
	}
	*vp = fv;
	return 0;
}

static int ctrl_cx2341x_is_dirty(struct pvr2_ctrl *cptr)
{
	return cptr->hdw->enc_stale != 0;
}

static void ctrl_cx2341x_clear_dirty(struct pvr2_ctrl *cptr)
{
	cptr->hdw->enc_stale = 0;
	cptr->hdw->enc_unsafe_stale = 0;
}

static int ctrl_cx2341x_get(struct pvr2_ctrl *cptr,int *vp)
{
	int ret;
	struct v4l2_ext_controls cs;
	struct v4l2_ext_control c1;
	memset(&cs,0,sizeof(cs));
	memset(&c1,0,sizeof(c1));
	cs.controls = &c1;
	cs.count = 1;
	c1.id = cptr->info->v4l_id;
	ret = cx2341x_ext_ctrls(&cptr->hdw->enc_ctl_state, 0, &cs,
				VIDIOC_G_EXT_CTRLS);
	if (ret) return ret;
	*vp = c1.value;
	return 0;
}

static int ctrl_cx2341x_set(struct pvr2_ctrl *cptr,int m,int v)
{
	int ret;
	struct pvr2_hdw *hdw = cptr->hdw;
	struct v4l2_ext_controls cs;
	struct v4l2_ext_control c1;
	memset(&cs,0,sizeof(cs));
	memset(&c1,0,sizeof(c1));
	cs.controls = &c1;
	cs.count = 1;
	c1.id = cptr->info->v4l_id;
	c1.value = v;
	ret = cx2341x_ext_ctrls(&hdw->enc_ctl_state,
				hdw->state_encoder_run, &cs,
				VIDIOC_S_EXT_CTRLS);
	if (ret == -EBUSY) {
		/* Oops.  cx2341x is telling us it's not safe to change
		   this control while we're capturing.  Make a note of this
		   fact so that the pipeline will be stopped the next time
		   controls are committed.  Then go on ahead and store this
		   change anyway. */
		ret = cx2341x_ext_ctrls(&hdw->enc_ctl_state,
					0, &cs,
					VIDIOC_S_EXT_CTRLS);
		if (!ret) hdw->enc_unsafe_stale = !0;
	}
	if (ret) return ret;
	hdw->enc_stale = !0;
	return 0;
}

static unsigned int ctrl_cx2341x_getv4lflags(struct pvr2_ctrl *cptr)
{
	struct v4l2_queryctrl qctrl;
	struct pvr2_ctl_info *info;
	qctrl.id = cptr->info->v4l_id;
	cx2341x_ctrl_query(&cptr->hdw->enc_ctl_state,&qctrl);
	/* Strip out the const so we can adjust a function pointer.  It's
	   OK to do this here because we know this is a dynamically created
	   control, so the underlying storage for the info pointer is (a)
	   private to us, and (b) not in read-only storage.  Either we do
	   this or we significantly complicate the underlying control
	   implementation. */
	info = (struct pvr2_ctl_info *)(cptr->info);
	if (qctrl.flags & V4L2_CTRL_FLAG_READ_ONLY) {
		if (info->set_value) {
			info->set_value = NULL;
		}
	} else {
		if (!(info->set_value)) {
			info->set_value = ctrl_cx2341x_set;
		}
	}
	return qctrl.flags;
}

static int ctrl_streamingenabled_get(struct pvr2_ctrl *cptr,int *vp)
{
	*vp = cptr->hdw->state_pipeline_req;
	return 0;
}

static int ctrl_masterstate_get(struct pvr2_ctrl *cptr,int *vp)
{
	*vp = cptr->hdw->master_state;
	return 0;
}

static int ctrl_hsm_get(struct pvr2_ctrl *cptr,int *vp)
{
	int result = pvr2_hdw_is_hsm(cptr->hdw);
	*vp = PVR2_CVAL_HSM_FULL;
	if (result < 0) *vp = PVR2_CVAL_HSM_FAIL;
	if (result) *vp = PVR2_CVAL_HSM_HIGH;
	return 0;
}

static int ctrl_stdavail_get(struct pvr2_ctrl *cptr,int *vp)
{
	*vp = cptr->hdw->std_mask_avail;
	return 0;
}

static int ctrl_stdavail_set(struct pvr2_ctrl *cptr,int m,int v)
{
	struct pvr2_hdw *hdw = cptr->hdw;
	v4l2_std_id ns;
	ns = hdw->std_mask_avail;
	ns = (ns & ~m) | (v & m);
	if (ns == hdw->std_mask_avail) return 0;
	hdw->std_mask_avail = ns;
	pvr2_hdw_internal_set_std_avail(hdw);
	pvr2_hdw_internal_find_stdenum(hdw);
	return 0;
}

static int ctrl_std_val_to_sym(struct pvr2_ctrl *cptr,int msk,int val,
			       char *bufPtr,unsigned int bufSize,
			       unsigned int *len)
{
	*len = pvr2_std_id_to_str(bufPtr,bufSize,msk & val);
	return 0;
}

static int ctrl_std_sym_to_val(struct pvr2_ctrl *cptr,
			       const char *bufPtr,unsigned int bufSize,
			       int *mskp,int *valp)
{
	int ret;
	v4l2_std_id id;
	ret = pvr2_std_str_to_id(&id,bufPtr,bufSize);
	if (ret < 0) return ret;
	if (mskp) *mskp = id;
	if (valp) *valp = id;
	return 0;
}

static int ctrl_stdcur_get(struct pvr2_ctrl *cptr,int *vp)
{
	*vp = cptr->hdw->std_mask_cur;
	return 0;
}

static int ctrl_stdcur_set(struct pvr2_ctrl *cptr,int m,int v)
{
	struct pvr2_hdw *hdw = cptr->hdw;
	v4l2_std_id ns;
	ns = hdw->std_mask_cur;
	ns = (ns & ~m) | (v & m);
	if (ns == hdw->std_mask_cur) return 0;
	hdw->std_mask_cur = ns;
	hdw->std_dirty = !0;
	pvr2_hdw_internal_find_stdenum(hdw);
	return 0;
}

static int ctrl_stdcur_is_dirty(struct pvr2_ctrl *cptr)
{
	return cptr->hdw->std_dirty != 0;
}

static void ctrl_stdcur_clear_dirty(struct pvr2_ctrl *cptr)
{
	cptr->hdw->std_dirty = 0;
}

static int ctrl_signal_get(struct pvr2_ctrl *cptr,int *vp)
{
	struct pvr2_hdw *hdw = cptr->hdw;
	pvr2_hdw_status_poll(hdw);
	*vp = hdw->tuner_signal_info.signal;
	return 0;
}

static int ctrl_audio_modes_present_get(struct pvr2_ctrl *cptr,int *vp)
{
	int val = 0;
	unsigned int subchan;
	struct pvr2_hdw *hdw = cptr->hdw;
	pvr2_hdw_status_poll(hdw);
	subchan = hdw->tuner_signal_info.rxsubchans;
	if (subchan & V4L2_TUNER_SUB_MONO) {
		val |= (1 << V4L2_TUNER_MODE_MONO);
	}
	if (subchan & V4L2_TUNER_SUB_STEREO) {
		val |= (1 << V4L2_TUNER_MODE_STEREO);
	}
	if (subchan & V4L2_TUNER_SUB_LANG1) {
		val |= (1 << V4L2_TUNER_MODE_LANG1);
	}
	if (subchan & V4L2_TUNER_SUB_LANG2) {
		val |= (1 << V4L2_TUNER_MODE_LANG2);
	}
	*vp = val;
	return 0;
}


static int ctrl_stdenumcur_set(struct pvr2_ctrl *cptr,int m,int v)
{
	struct pvr2_hdw *hdw = cptr->hdw;
	if (v < 0) return -EINVAL;
	if (v > hdw->std_enum_cnt) return -EINVAL;
	hdw->std_enum_cur = v;
	if (!v) return 0;
	v--;
	if (hdw->std_mask_cur == hdw->std_defs[v].id) return 0;
	hdw->std_mask_cur = hdw->std_defs[v].id;
	hdw->std_dirty = !0;
	return 0;
}


static int ctrl_stdenumcur_get(struct pvr2_ctrl *cptr,int *vp)
{
	*vp = cptr->hdw->std_enum_cur;
	return 0;
}


static int ctrl_stdenumcur_is_dirty(struct pvr2_ctrl *cptr)
{
	return cptr->hdw->std_dirty != 0;
}


static void ctrl_stdenumcur_clear_dirty(struct pvr2_ctrl *cptr)
{
	cptr->hdw->std_dirty = 0;
}


#define DEFINT(vmin,vmax) \
	.type = pvr2_ctl_int, \
	.def.type_int.min_value = vmin, \
	.def.type_int.max_value = vmax

#define DEFENUM(tab) \
	.type = pvr2_ctl_enum, \
	.def.type_enum.count = ARRAY_SIZE(tab), \
	.def.type_enum.value_names = tab

#define DEFBOOL \
	.type = pvr2_ctl_bool

#define DEFMASK(msk,tab) \
	.type = pvr2_ctl_bitmask, \
	.def.type_bitmask.valid_bits = msk, \
	.def.type_bitmask.bit_names = tab

#define DEFREF(vname) \
	.set_value = ctrl_set_##vname, \
	.get_value = ctrl_get_##vname, \
	.is_dirty = ctrl_isdirty_##vname, \
	.clear_dirty = ctrl_cleardirty_##vname


#define VCREATE_FUNCS(vname) \
static int ctrl_get_##vname(struct pvr2_ctrl *cptr,int *vp) \
{*vp = cptr->hdw->vname##_val; return 0;} \
static int ctrl_set_##vname(struct pvr2_ctrl *cptr,int m,int v) \
{cptr->hdw->vname##_val = v; cptr->hdw->vname##_dirty = !0; return 0;} \
static int ctrl_isdirty_##vname(struct pvr2_ctrl *cptr) \
{return cptr->hdw->vname##_dirty != 0;} \
static void ctrl_cleardirty_##vname(struct pvr2_ctrl *cptr) \
{cptr->hdw->vname##_dirty = 0;}

VCREATE_FUNCS(brightness)
VCREATE_FUNCS(contrast)
VCREATE_FUNCS(saturation)
VCREATE_FUNCS(hue)
VCREATE_FUNCS(volume)
VCREATE_FUNCS(balance)
VCREATE_FUNCS(bass)
VCREATE_FUNCS(treble)
VCREATE_FUNCS(mute)
VCREATE_FUNCS(cropl)
VCREATE_FUNCS(cropt)
VCREATE_FUNCS(cropw)
VCREATE_FUNCS(croph)
VCREATE_FUNCS(audiomode)
VCREATE_FUNCS(res_hor)
VCREATE_FUNCS(res_ver)
VCREATE_FUNCS(srate)

/* Table definition of all controls which can be manipulated */
static const struct pvr2_ctl_info control_defs[] = {
	{
		.v4l_id = V4L2_CID_BRIGHTNESS,
		.desc = "Brightness",
		.name = "brightness",
		.default_value = 128,
		DEFREF(brightness),
		DEFINT(0,255),
	},{
		.v4l_id = V4L2_CID_CONTRAST,
		.desc = "Contrast",
		.name = "contrast",
		.default_value = 68,
		DEFREF(contrast),
		DEFINT(0,127),
	},{
		.v4l_id = V4L2_CID_SATURATION,
		.desc = "Saturation",
		.name = "saturation",
		.default_value = 64,
		DEFREF(saturation),
		DEFINT(0,127),
	},{
		.v4l_id = V4L2_CID_HUE,
		.desc = "Hue",
		.name = "hue",
		.default_value = 0,
		DEFREF(hue),
		DEFINT(-128,127),
	},{
		.v4l_id = V4L2_CID_AUDIO_VOLUME,
		.desc = "Volume",
		.name = "volume",
		.default_value = 62000,
		DEFREF(volume),
		DEFINT(0,65535),
	},{
		.v4l_id = V4L2_CID_AUDIO_BALANCE,
		.desc = "Balance",
		.name = "balance",
		.default_value = 0,
		DEFREF(balance),
		DEFINT(-32768,32767),
	},{
		.v4l_id = V4L2_CID_AUDIO_BASS,
		.desc = "Bass",
		.name = "bass",
		.default_value = 0,
		DEFREF(bass),
		DEFINT(-32768,32767),
	},{
		.v4l_id = V4L2_CID_AUDIO_TREBLE,
		.desc = "Treble",
		.name = "treble",
		.default_value = 0,
		DEFREF(treble),
		DEFINT(-32768,32767),
	},{
		.v4l_id = V4L2_CID_AUDIO_MUTE,
		.desc = "Mute",
		.name = "mute",
		.default_value = 0,
		DEFREF(mute),
		DEFBOOL,
	}, {
		.desc = "Capture crop left margin",
		.name = "crop_left",
		.internal_id = PVR2_CID_CROPL,
		.default_value = 0,
		DEFREF(cropl),
		DEFINT(-129, 340),
		.get_min_value = ctrl_cropl_min_get,
		.get_max_value = ctrl_cropl_max_get,
		.get_def_value = ctrl_get_cropcapdl,
	}, {
		.desc = "Capture crop top margin",
		.name = "crop_top",
		.internal_id = PVR2_CID_CROPT,
		.default_value = 0,
		DEFREF(cropt),
		DEFINT(-35, 544),
		.get_min_value = ctrl_cropt_min_get,
		.get_max_value = ctrl_cropt_max_get,
		.get_def_value = ctrl_get_cropcapdt,
	}, {
		.desc = "Capture crop width",
		.name = "crop_width",
		.internal_id = PVR2_CID_CROPW,
		.default_value = 720,
		DEFREF(cropw),
		.get_max_value = ctrl_cropw_max_get,
		.get_def_value = ctrl_get_cropcapdw,
	}, {
		.desc = "Capture crop height",
		.name = "crop_height",
		.internal_id = PVR2_CID_CROPH,
		.default_value = 480,
		DEFREF(croph),
		.get_max_value = ctrl_croph_max_get,
		.get_def_value = ctrl_get_cropcapdh,
	}, {
		.desc = "Capture capability pixel aspect numerator",
		.name = "cropcap_pixel_numerator",
		.internal_id = PVR2_CID_CROPCAPPAN,
		.get_value = ctrl_get_cropcappan,
	}, {
		.desc = "Capture capability pixel aspect denominator",
		.name = "cropcap_pixel_denominator",
		.internal_id = PVR2_CID_CROPCAPPAD,
		.get_value = ctrl_get_cropcappad,
	}, {
		.desc = "Capture capability bounds top",
		.name = "cropcap_bounds_top",
		.internal_id = PVR2_CID_CROPCAPBT,
		.get_value = ctrl_get_cropcapbt,
	}, {
		.desc = "Capture capability bounds left",
		.name = "cropcap_bounds_left",
		.internal_id = PVR2_CID_CROPCAPBL,
		.get_value = ctrl_get_cropcapbl,
	}, {
		.desc = "Capture capability bounds width",
		.name = "cropcap_bounds_width",
		.internal_id = PVR2_CID_CROPCAPBW,
		.get_value = ctrl_get_cropcapbw,
	}, {
		.desc = "Capture capability bounds height",
		.name = "cropcap_bounds_height",
		.internal_id = PVR2_CID_CROPCAPBH,
		.get_value = ctrl_get_cropcapbh,
	},{
		.desc = "Video Source",
		.name = "input",
		.internal_id = PVR2_CID_INPUT,
		.default_value = PVR2_CVAL_INPUT_TV,
		.check_value = ctrl_check_input,
		DEFREF(input),
		DEFENUM(control_values_input),
	},{
		.desc = "Audio Mode",
		.name = "audio_mode",
		.internal_id = PVR2_CID_AUDIOMODE,
		.default_value = V4L2_TUNER_MODE_STEREO,
		DEFREF(audiomode),
		DEFENUM(control_values_audiomode),
	},{
		.desc = "Horizontal capture resolution",
		.name = "resolution_hor",
		.internal_id = PVR2_CID_HRES,
		.default_value = 720,
		DEFREF(res_hor),
		DEFINT(19,720),
	},{
		.desc = "Vertical capture resolution",
		.name = "resolution_ver",
		.internal_id = PVR2_CID_VRES,
		.default_value = 480,
		DEFREF(res_ver),
		DEFINT(17,576),
		/* Hook in check for video standard and adjust maximum
		   depending on the standard. */
		.get_max_value = ctrl_vres_max_get,
		.get_min_value = ctrl_vres_min_get,
	},{
		.v4l_id = V4L2_CID_MPEG_AUDIO_SAMPLING_FREQ,
		.default_value = V4L2_MPEG_AUDIO_SAMPLING_FREQ_48000,
		.desc = "Audio Sampling Frequency",
		.name = "srate",
		DEFREF(srate),
		DEFENUM(control_values_srate),
	},{
		.desc = "Tuner Frequency (Hz)",
		.name = "frequency",
		.internal_id = PVR2_CID_FREQUENCY,
		.default_value = 0,
		.set_value = ctrl_freq_set,
		.get_value = ctrl_freq_get,
		.is_dirty = ctrl_freq_is_dirty,
		.clear_dirty = ctrl_freq_clear_dirty,
		DEFINT(0,0),
		/* Hook in check for input value (tv/radio) and adjust
		   max/min values accordingly */
		.get_max_value = ctrl_freq_max_get,
		.get_min_value = ctrl_freq_min_get,
	},{
		.desc = "Channel",
		.name = "channel",
		.set_value = ctrl_channel_set,
		.get_value = ctrl_channel_get,
		DEFINT(0,FREQTABLE_SIZE),
	},{
		.desc = "Channel Program Frequency",
		.name = "freq_table_value",
		.set_value = ctrl_channelfreq_set,
		.get_value = ctrl_channelfreq_get,
		DEFINT(0,0),
		/* Hook in check for input value (tv/radio) and adjust
		   max/min values accordingly */
		.get_max_value = ctrl_freq_max_get,
		.get_min_value = ctrl_freq_min_get,
	},{
		.desc = "Channel Program ID",
		.name = "freq_table_channel",
		.set_value = ctrl_channelprog_set,
		.get_value = ctrl_channelprog_get,
		DEFINT(0,FREQTABLE_SIZE),
	},{
		.desc = "Streaming Enabled",
		.name = "streaming_enabled",
		.get_value = ctrl_streamingenabled_get,
		DEFBOOL,
	},{
		.desc = "USB Speed",
		.name = "usb_speed",
		.get_value = ctrl_hsm_get,
		DEFENUM(control_values_hsm),
	},{
		.desc = "Master State",
		.name = "master_state",
		.get_value = ctrl_masterstate_get,
		DEFENUM(pvr2_state_names),
	},{
		.desc = "Signal Present",
		.name = "signal_present",
		.get_value = ctrl_signal_get,
		DEFINT(0,65535),
	},{
		.desc = "Audio Modes Present",
		.name = "audio_modes_present",
		.get_value = ctrl_audio_modes_present_get,
		/* For this type we "borrow" the V4L2_TUNER_MODE enum from
		   v4l.  Nothing outside of this module cares about this,
		   but I reuse it in order to also reuse the
		   control_values_audiomode string table. */
		DEFMASK(((1 << V4L2_TUNER_MODE_MONO)|
			 (1 << V4L2_TUNER_MODE_STEREO)|
			 (1 << V4L2_TUNER_MODE_LANG1)|
			 (1 << V4L2_TUNER_MODE_LANG2)),
			control_values_audiomode),
	},{
		.desc = "Video Standards Available Mask",
		.name = "video_standard_mask_available",
		.internal_id = PVR2_CID_STDAVAIL,
		.skip_init = !0,
		.get_value = ctrl_stdavail_get,
		.set_value = ctrl_stdavail_set,
		.val_to_sym = ctrl_std_val_to_sym,
		.sym_to_val = ctrl_std_sym_to_val,
		.type = pvr2_ctl_bitmask,
	},{
		.desc = "Video Standards In Use Mask",
		.name = "video_standard_mask_active",
		.internal_id = PVR2_CID_STDCUR,
		.skip_init = !0,
		.get_value = ctrl_stdcur_get,
		.set_value = ctrl_stdcur_set,
		.is_dirty = ctrl_stdcur_is_dirty,
		.clear_dirty = ctrl_stdcur_clear_dirty,
		.val_to_sym = ctrl_std_val_to_sym,
		.sym_to_val = ctrl_std_sym_to_val,
		.type = pvr2_ctl_bitmask,
	},{
		.desc = "Video Standard Name",
		.name = "video_standard",
		.internal_id = PVR2_CID_STDENUM,
		.skip_init = !0,
		.get_value = ctrl_stdenumcur_get,
		.set_value = ctrl_stdenumcur_set,
		.is_dirty = ctrl_stdenumcur_is_dirty,
		.clear_dirty = ctrl_stdenumcur_clear_dirty,
		.type = pvr2_ctl_enum,
	}
};

#define CTRLDEF_COUNT ARRAY_SIZE(control_defs)


const char *pvr2_config_get_name(enum pvr2_config cfg)
{
	switch (cfg) {
	case pvr2_config_empty: return "empty";
	case pvr2_config_mpeg: return "mpeg";
	case pvr2_config_vbi: return "vbi";
	case pvr2_config_pcm: return "pcm";
	case pvr2_config_rawvideo: return "raw video";
	}
	return "<unknown>";
}


struct usb_device *pvr2_hdw_get_dev(struct pvr2_hdw *hdw)
{
	return hdw->usb_dev;
}


unsigned long pvr2_hdw_get_sn(struct pvr2_hdw *hdw)
{
	return hdw->serial_number;
}


const char *pvr2_hdw_get_bus_info(struct pvr2_hdw *hdw)
{
	return hdw->bus_info;
}


const char *pvr2_hdw_get_device_identifier(struct pvr2_hdw *hdw)
{
	return hdw->identifier;
}


unsigned long pvr2_hdw_get_cur_freq(struct pvr2_hdw *hdw)
{
	return hdw->freqSelector ? hdw->freqValTelevision : hdw->freqValRadio;
}

/* Set the currently tuned frequency and account for all possible
   driver-core side effects of this action. */
static void pvr2_hdw_set_cur_freq(struct pvr2_hdw *hdw,unsigned long val)
{
	if (hdw->input_val == PVR2_CVAL_INPUT_RADIO) {
		if (hdw->freqSelector) {
			/* Swing over to radio frequency selection */
			hdw->freqSelector = 0;
			hdw->freqDirty = !0;
		}
		if (hdw->freqValRadio != val) {
			hdw->freqValRadio = val;
			hdw->freqSlotRadio = 0;
			hdw->freqDirty = !0;
		}
	} else {
		if (!(hdw->freqSelector)) {
			/* Swing over to television frequency selection */
			hdw->freqSelector = 1;
			hdw->freqDirty = !0;
		}
		if (hdw->freqValTelevision != val) {
			hdw->freqValTelevision = val;
			hdw->freqSlotTelevision = 0;
			hdw->freqDirty = !0;
		}
	}
}

int pvr2_hdw_get_unit_number(struct pvr2_hdw *hdw)
{
	return hdw->unit_number;
}


/* Attempt to locate one of the given set of files.  Messages are logged
   appropriate to what has been found.  The return value will be 0 or
   greater on success (it will be the index of the file name found) and
   fw_entry will be filled in.  Otherwise a negative error is returned on
   failure.  If the return value is -ENOENT then no viable firmware file
   could be located. */
static int pvr2_locate_firmware(struct pvr2_hdw *hdw,
				const struct firmware **fw_entry,
				const char *fwtypename,
				unsigned int fwcount,
				const char *fwnames[])
{
	unsigned int idx;
	int ret = -EINVAL;
	for (idx = 0; idx < fwcount; idx++) {
		ret = request_firmware(fw_entry,
				       fwnames[idx],
				       &hdw->usb_dev->dev);
		if (!ret) {
			trace_firmware("Located %s firmware: %s;"
				       " uploading...",
				       fwtypename,
				       fwnames[idx]);
			return idx;
		}
		if (ret == -ENOENT) continue;
		pvr2_trace(PVR2_TRACE_ERROR_LEGS,
			   "request_firmware fatal error with code=%d",ret);
		return ret;
	}
	pvr2_trace(PVR2_TRACE_ERROR_LEGS,
		   "***WARNING***"
		   " Device %s firmware"
		   " seems to be missing.",
		   fwtypename);
	pvr2_trace(PVR2_TRACE_ERROR_LEGS,
		   "Did you install the pvrusb2 firmware files"
		   " in their proper location?");
	if (fwcount == 1) {
		pvr2_trace(PVR2_TRACE_ERROR_LEGS,
			   "request_firmware unable to locate %s file %s",
			   fwtypename,fwnames[0]);
	} else {
		pvr2_trace(PVR2_TRACE_ERROR_LEGS,
			   "request_firmware unable to locate"
			   " one of the following %s files:",
			   fwtypename);
		for (idx = 0; idx < fwcount; idx++) {
			pvr2_trace(PVR2_TRACE_ERROR_LEGS,
				   "request_firmware: Failed to find %s",
				   fwnames[idx]);
		}
	}
	return ret;
}


/*
 * pvr2_upload_firmware1().
 *
 * Send the 8051 firmware to the device.  After the upload, arrange for
 * device to re-enumerate.
 *
 * NOTE : the pointer to the firmware data given by request_firmware()
 * is not suitable for an usb transaction.
 *
 */
static int pvr2_upload_firmware1(struct pvr2_hdw *hdw)
{
	const struct firmware *fw_entry = NULL;
	void  *fw_ptr;
	unsigned int pipe;
	int ret;
	u16 address;

	if (!hdw->hdw_desc->fx2_firmware.cnt) {
		hdw->fw1_state = FW1_STATE_OK;
		pvr2_trace(PVR2_TRACE_ERROR_LEGS,
			   "Connected device type defines"
			   " no firmware to upload; ignoring firmware");
		return -ENOTTY;
	}

	hdw->fw1_state = FW1_STATE_FAILED; // default result

	trace_firmware("pvr2_upload_firmware1");

	ret = pvr2_locate_firmware(hdw,&fw_entry,"fx2 controller",
				   hdw->hdw_desc->fx2_firmware.cnt,
				   hdw->hdw_desc->fx2_firmware.lst);
	if (ret < 0) {
		if (ret == -ENOENT) hdw->fw1_state = FW1_STATE_MISSING;
		return ret;
	}

	usb_clear_halt(hdw->usb_dev, usb_sndbulkpipe(hdw->usb_dev, 0 & 0x7f));

	pipe = usb_sndctrlpipe(hdw->usb_dev, 0);

	if (fw_entry->size != 0x2000){
		pvr2_trace(PVR2_TRACE_ERROR_LEGS,"wrong fx2 firmware size");
		release_firmware(fw_entry);
		return -ENOMEM;
	}

	fw_ptr = kmalloc(0x800, GFP_KERNEL);
	if (fw_ptr == NULL){
		release_firmware(fw_entry);
		return -ENOMEM;
	}

	/* We have to hold the CPU during firmware upload. */
	pvr2_hdw_cpureset_assert(hdw,1);

	/* upload the firmware to address 0000-1fff in 2048 (=0x800) bytes
	   chunk. */

	ret = 0;
	for(address = 0; address < fw_entry->size; address += 0x800) {
		memcpy(fw_ptr, fw_entry->data + address, 0x800);
		ret += usb_control_msg(hdw->usb_dev, pipe, 0xa0, 0x40, address,
				       0, fw_ptr, 0x800, HZ);
	}

	trace_firmware("Upload done, releasing device's CPU");

	/* Now release the CPU.  It will disconnect and reconnect later. */
	pvr2_hdw_cpureset_assert(hdw,0);

	kfree(fw_ptr);
	release_firmware(fw_entry);

	trace_firmware("Upload done (%d bytes sent)",ret);

	/* We should have written 8192 bytes */
	if (ret == 8192) {
		hdw->fw1_state = FW1_STATE_RELOAD;
		return 0;
	}

	return -EIO;
}


/*
 * pvr2_upload_firmware2()
 *
 * This uploads encoder firmware on endpoint 2.
 *
 */

int pvr2_upload_firmware2(struct pvr2_hdw *hdw)
{
	const struct firmware *fw_entry = NULL;
	void  *fw_ptr;
	unsigned int pipe, fw_len, fw_done, bcnt, icnt;
	int actual_length;
	int ret = 0;
	int fwidx;
	static const char *fw_files[] = {
		CX2341X_FIRM_ENC_FILENAME,
	};

	if (hdw->hdw_desc->flag_skip_cx23416_firmware) {
		return 0;
	}

	trace_firmware("pvr2_upload_firmware2");

	ret = pvr2_locate_firmware(hdw,&fw_entry,"encoder",
				   ARRAY_SIZE(fw_files), fw_files);
	if (ret < 0) return ret;
	fwidx = ret;
	ret = 0;
	/* Since we're about to completely reinitialize the encoder,
	   invalidate our cached copy of its configuration state.  Next
	   time we configure the encoder, then we'll fully configure it. */
	hdw->enc_cur_valid = 0;

	/* Encoder is about to be reset so note that as far as we're
	   concerned now, the encoder has never been run. */
	del_timer_sync(&hdw->encoder_run_timer);
	if (hdw->state_encoder_runok) {
		hdw->state_encoder_runok = 0;
		trace_stbit("state_encoder_runok",hdw->state_encoder_runok);
	}

	/* First prepare firmware loading */
	ret |= pvr2_write_register(hdw, 0x0048, 0xffffffff); /*interrupt mask*/
	ret |= pvr2_hdw_gpio_chg_dir(hdw,0xffffffff,0x00000088); /*gpio dir*/
	ret |= pvr2_hdw_gpio_chg_out(hdw,0xffffffff,0x00000008); /*gpio output state*/
	ret |= pvr2_hdw_cmd_deep_reset(hdw);
	ret |= pvr2_write_register(hdw, 0xa064, 0x00000000); /*APU command*/
	ret |= pvr2_hdw_gpio_chg_dir(hdw,0xffffffff,0x00000408); /*gpio dir*/
	ret |= pvr2_hdw_gpio_chg_out(hdw,0xffffffff,0x00000008); /*gpio output state*/
	ret |= pvr2_write_register(hdw, 0x9058, 0xffffffed); /*VPU ctrl*/
	ret |= pvr2_write_register(hdw, 0x9054, 0xfffffffd); /*reset hw blocks*/
	ret |= pvr2_write_register(hdw, 0x07f8, 0x80000800); /*encoder SDRAM refresh*/
	ret |= pvr2_write_register(hdw, 0x07fc, 0x0000001a); /*encoder SDRAM pre-charge*/
	ret |= pvr2_write_register(hdw, 0x0700, 0x00000000); /*I2C clock*/
	ret |= pvr2_write_register(hdw, 0xaa00, 0x00000000); /*unknown*/
	ret |= pvr2_write_register(hdw, 0xaa04, 0x00057810); /*unknown*/
	ret |= pvr2_write_register(hdw, 0xaa10, 0x00148500); /*unknown*/
	ret |= pvr2_write_register(hdw, 0xaa18, 0x00840000); /*unknown*/
	ret |= pvr2_issue_simple_cmd(hdw,FX2CMD_FWPOST1);
	ret |= pvr2_issue_simple_cmd(hdw,FX2CMD_MEMSEL | (1 << 8) | (0 << 16));

	if (ret) {
		pvr2_trace(PVR2_TRACE_ERROR_LEGS,
			   "firmware2 upload prep failed, ret=%d",ret);
		release_firmware(fw_entry);
		goto done;
	}

	/* Now send firmware */

	fw_len = fw_entry->size;

	if (fw_len % sizeof(u32)) {
		pvr2_trace(PVR2_TRACE_ERROR_LEGS,
			   "size of %s firmware"
			   " must be a multiple of %zu bytes",
			   fw_files[fwidx],sizeof(u32));
		release_firmware(fw_entry);
		ret = -EINVAL;
		goto done;
	}

	fw_ptr = kmalloc(FIRMWARE_CHUNK_SIZE, GFP_KERNEL);
	if (fw_ptr == NULL){
		release_firmware(fw_entry);
		pvr2_trace(PVR2_TRACE_ERROR_LEGS,
			   "failed to allocate memory for firmware2 upload");
		ret = -ENOMEM;
		goto done;
	}

	pipe = usb_sndbulkpipe(hdw->usb_dev, PVR2_FIRMWARE_ENDPOINT);

	fw_done = 0;
	for (fw_done = 0; fw_done < fw_len;) {
		bcnt = fw_len - fw_done;
		if (bcnt > FIRMWARE_CHUNK_SIZE) bcnt = FIRMWARE_CHUNK_SIZE;
		memcpy(fw_ptr, fw_entry->data + fw_done, bcnt);
		/* Usbsnoop log shows that we must swap bytes... */
		/* Some background info: The data being swapped here is a
		   firmware image destined for the mpeg encoder chip that
		   lives at the other end of a USB endpoint.  The encoder
		   chip always talks in 32 bit chunks and its storage is
		   organized into 32 bit words.  However from the file
		   system to the encoder chip everything is purely a byte
		   stream.  The firmware file's contents are always 32 bit
		   swapped from what the encoder expects.  Thus the need
		   always exists to swap the bytes regardless of the endian
		   type of the host processor and therefore swab32() makes
		   the most sense. */
		for (icnt = 0; icnt < bcnt/4 ; icnt++)
			((u32 *)fw_ptr)[icnt] = swab32(((u32 *)fw_ptr)[icnt]);

		ret |= usb_bulk_msg(hdw->usb_dev, pipe, fw_ptr,bcnt,
				    &actual_length, HZ);
		ret |= (actual_length != bcnt);
		if (ret) break;
		fw_done += bcnt;
	}

	trace_firmware("upload of %s : %i / %i ",
		       fw_files[fwidx],fw_done,fw_len);

	kfree(fw_ptr);
	release_firmware(fw_entry);

	if (ret) {
		pvr2_trace(PVR2_TRACE_ERROR_LEGS,
			   "firmware2 upload transfer failure");
		goto done;
	}

	/* Finish upload */

	ret |= pvr2_write_register(hdw, 0x9054, 0xffffffff); /*reset hw blocks*/
	ret |= pvr2_write_register(hdw, 0x9058, 0xffffffe8); /*VPU ctrl*/
	ret |= pvr2_issue_simple_cmd(hdw,FX2CMD_MEMSEL | (1 << 8) | (0 << 16));

	if (ret) {
		pvr2_trace(PVR2_TRACE_ERROR_LEGS,
			   "firmware2 upload post-proc failure");
	}

 done:
	if (hdw->hdw_desc->signal_routing_scheme ==
	    PVR2_ROUTING_SCHEME_GOTVIEW) {
		/* Ensure that GPIO 11 is set to output for GOTVIEW
		   hardware. */
		pvr2_hdw_gpio_chg_dir(hdw,(1 << 11),~0);
	}
	return ret;
}


static const char *pvr2_get_state_name(unsigned int st)
{
	if (st < ARRAY_SIZE(pvr2_state_names)) {
		return pvr2_state_names[st];
	}
	return "???";
}

static int pvr2_decoder_enable(struct pvr2_hdw *hdw,int enablefl)
{
	/* Even though we really only care about the video decoder chip at
	   this point, we'll broadcast stream on/off to all sub-devices
	   anyway, just in case somebody else wants to hear the
	   command... */
	pvr2_trace(PVR2_TRACE_CHIPS, "subdev v4l2 stream=%s",
		   (enablefl ? "on" : "off"));
	v4l2_device_call_all(&hdw->v4l2_dev, 0, video, s_stream, enablefl);
	if (hdw->decoder_client_id) {
		/* We get here if the encoder has been noticed.  Otherwise
		   we'll issue a warning to the user (which should
		   normally never happen). */
		return 0;
	}
	if (!hdw->flag_decoder_missed) {
		pvr2_trace(PVR2_TRACE_ERROR_LEGS,
			   "WARNING: No decoder present");
		hdw->flag_decoder_missed = !0;
		trace_stbit("flag_decoder_missed",
			    hdw->flag_decoder_missed);
	}
	return -EIO;
}


int pvr2_hdw_get_state(struct pvr2_hdw *hdw)
{
	return hdw->master_state;
}


static int pvr2_hdw_untrip_unlocked(struct pvr2_hdw *hdw)
{
	if (!hdw->flag_tripped) return 0;
	hdw->flag_tripped = 0;
	pvr2_trace(PVR2_TRACE_ERROR_LEGS,
		   "Clearing driver error statuss");
	return !0;
}


int pvr2_hdw_untrip(struct pvr2_hdw *hdw)
{
	int fl;
	LOCK_TAKE(hdw->big_lock); do {
		fl = pvr2_hdw_untrip_unlocked(hdw);
	} while (0); LOCK_GIVE(hdw->big_lock);
	if (fl) pvr2_hdw_state_sched(hdw);
	return 0;
}




int pvr2_hdw_get_streaming(struct pvr2_hdw *hdw)
{
	return hdw->state_pipeline_req != 0;
}


int pvr2_hdw_set_streaming(struct pvr2_hdw *hdw,int enable_flag)
{
	int ret,st;
	LOCK_TAKE(hdw->big_lock); do {
		pvr2_hdw_untrip_unlocked(hdw);
		if ((!enable_flag) != !(hdw->state_pipeline_req)) {
			hdw->state_pipeline_req = enable_flag != 0;
			pvr2_trace(PVR2_TRACE_START_STOP,
				   "/*--TRACE_STREAM--*/ %s",
				   enable_flag ? "enable" : "disable");
		}
		pvr2_hdw_state_sched(hdw);
	} while (0); LOCK_GIVE(hdw->big_lock);
	if ((ret = pvr2_hdw_wait(hdw,0)) < 0) return ret;
	if (enable_flag) {
		while ((st = hdw->master_state) != PVR2_STATE_RUN) {
			if (st != PVR2_STATE_READY) return -EIO;
			if ((ret = pvr2_hdw_wait(hdw,st)) < 0) return ret;
		}
	}
	return 0;
}


int pvr2_hdw_set_stream_type(struct pvr2_hdw *hdw,enum pvr2_config config)
{
	int fl;
	LOCK_TAKE(hdw->big_lock);
	if ((fl = (hdw->desired_stream_type != config)) != 0) {
		hdw->desired_stream_type = config;
		hdw->state_pipeline_config = 0;
		trace_stbit("state_pipeline_config",
			    hdw->state_pipeline_config);
		pvr2_hdw_state_sched(hdw);
	}
	LOCK_GIVE(hdw->big_lock);
	if (fl) return 0;
	return pvr2_hdw_wait(hdw,0);
}


static int get_default_tuner_type(struct pvr2_hdw *hdw)
{
	int unit_number = hdw->unit_number;
	int tp = -1;
	if ((unit_number >= 0) && (unit_number < PVR_NUM)) {
		tp = tuner[unit_number];
	}
	if (tp < 0) return -EINVAL;
	hdw->tuner_type = tp;
	hdw->tuner_updated = !0;
	return 0;
}


static v4l2_std_id get_default_standard(struct pvr2_hdw *hdw)
{
	int unit_number = hdw->unit_number;
	int tp = 0;
	if ((unit_number >= 0) && (unit_number < PVR_NUM)) {
		tp = video_std[unit_number];
		if (tp) return tp;
	}
	return 0;
}


static unsigned int get_default_error_tolerance(struct pvr2_hdw *hdw)
{
	int unit_number = hdw->unit_number;
	int tp = 0;
	if ((unit_number >= 0) && (unit_number < PVR_NUM)) {
		tp = tolerance[unit_number];
	}
	return tp;
}


static int pvr2_hdw_check_firmware(struct pvr2_hdw *hdw)
{
	/* Try a harmless request to fetch the eeprom's address over
	   endpoint 1.  See what happens.  Only the full FX2 image can
	   respond to this.  If this probe fails then likely the FX2
	   firmware needs be loaded. */
	int result;
	LOCK_TAKE(hdw->ctl_lock); do {
		hdw->cmd_buffer[0] = FX2CMD_GET_EEPROM_ADDR;
		result = pvr2_send_request_ex(hdw,HZ*1,!0,
					   hdw->cmd_buffer,1,
					   hdw->cmd_buffer,1);
		if (result < 0) break;
	} while(0); LOCK_GIVE(hdw->ctl_lock);
	if (result) {
		pvr2_trace(PVR2_TRACE_INIT,
			   "Probe of device endpoint 1 result status %d",
			   result);
	} else {
		pvr2_trace(PVR2_TRACE_INIT,
			   "Probe of device endpoint 1 succeeded");
	}
	return result == 0;
}

struct pvr2_std_hack {
	v4l2_std_id pat;  /* Pattern to match */
	v4l2_std_id msk;  /* Which bits we care about */
	v4l2_std_id std;  /* What additional standards or default to set */
};

/* This data structure labels specific combinations of standards from
   tveeprom that we'll try to recognize.  If we recognize one, then assume
   a specified default standard to use.  This is here because tveeprom only
   tells us about available standards not the intended default standard (if
   any) for the device in question.  We guess the default based on what has
   been reported as available.  Note that this is only for guessing a
   default - which can always be overridden explicitly - and if the user
   has otherwise named a default then that default will always be used in
   place of this table. */
static const struct pvr2_std_hack std_eeprom_maps[] = {
	{	/* PAL(B/G) */
		.pat = V4L2_STD_B|V4L2_STD_GH,
		.std = V4L2_STD_PAL_B|V4L2_STD_PAL_B1|V4L2_STD_PAL_G,
	},
	{	/* NTSC(M) */
		.pat = V4L2_STD_MN,
		.std = V4L2_STD_NTSC_M,
	},
	{	/* PAL(I) */
		.pat = V4L2_STD_PAL_I,
		.std = V4L2_STD_PAL_I,
	},
	{	/* SECAM(L/L') */
		.pat = V4L2_STD_SECAM_L|V4L2_STD_SECAM_LC,
		.std = V4L2_STD_SECAM_L|V4L2_STD_SECAM_LC,
	},
	{	/* PAL(D/D1/K) */
		.pat = V4L2_STD_DK,
		.std = V4L2_STD_PAL_D|V4L2_STD_PAL_D1|V4L2_STD_PAL_K,
	},
};

static void pvr2_hdw_setup_std(struct pvr2_hdw *hdw)
{
	char buf[40];
	unsigned int bcnt;
	v4l2_std_id std1,std2,std3;

	std1 = get_default_standard(hdw);
	std3 = std1 ? 0 : hdw->hdw_desc->default_std_mask;

	bcnt = pvr2_std_id_to_str(buf,sizeof(buf),hdw->std_mask_eeprom);
	pvr2_trace(PVR2_TRACE_STD,
		   "Supported video standard(s) reported available"
		   " in hardware: %.*s",
		   bcnt,buf);

	hdw->std_mask_avail = hdw->std_mask_eeprom;

	std2 = (std1|std3) & ~hdw->std_mask_avail;
	if (std2) {
		bcnt = pvr2_std_id_to_str(buf,sizeof(buf),std2);
		pvr2_trace(PVR2_TRACE_STD,
			   "Expanding supported video standards"
			   " to include: %.*s",
			   bcnt,buf);
		hdw->std_mask_avail |= std2;
	}

	pvr2_hdw_internal_set_std_avail(hdw);

	if (std1) {
		bcnt = pvr2_std_id_to_str(buf,sizeof(buf),std1);
		pvr2_trace(PVR2_TRACE_STD,
			   "Initial video standard forced to %.*s",
			   bcnt,buf);
		hdw->std_mask_cur = std1;
		hdw->std_dirty = !0;
		pvr2_hdw_internal_find_stdenum(hdw);
		return;
	}
	if (std3) {
		bcnt = pvr2_std_id_to_str(buf,sizeof(buf),std3);
		pvr2_trace(PVR2_TRACE_STD,
			   "Initial video standard"
			   " (determined by device type): %.*s",bcnt,buf);
		hdw->std_mask_cur = std3;
		hdw->std_dirty = !0;
		pvr2_hdw_internal_find_stdenum(hdw);
		return;
	}

	{
		unsigned int idx;
		for (idx = 0; idx < ARRAY_SIZE(std_eeprom_maps); idx++) {
			if (std_eeprom_maps[idx].msk ?
			    ((std_eeprom_maps[idx].pat ^
			     hdw->std_mask_eeprom) &
			     std_eeprom_maps[idx].msk) :
			    (std_eeprom_maps[idx].pat !=
			     hdw->std_mask_eeprom)) continue;
			bcnt = pvr2_std_id_to_str(buf,sizeof(buf),
						  std_eeprom_maps[idx].std);
			pvr2_trace(PVR2_TRACE_STD,
				   "Initial video standard guessed as %.*s",
				   bcnt,buf);
			hdw->std_mask_cur = std_eeprom_maps[idx].std;
			hdw->std_dirty = !0;
			pvr2_hdw_internal_find_stdenum(hdw);
			return;
		}
	}

	if (hdw->std_enum_cnt > 1) {
		// Autoselect the first listed standard
		hdw->std_enum_cur = 1;
		hdw->std_mask_cur = hdw->std_defs[hdw->std_enum_cur-1].id;
		hdw->std_dirty = !0;
		pvr2_trace(PVR2_TRACE_STD,
			   "Initial video standard auto-selected to %s",
			   hdw->std_defs[hdw->std_enum_cur-1].name);
		return;
	}

	pvr2_trace(PVR2_TRACE_ERROR_LEGS,
		   "Unable to select a viable initial video standard");
}


static unsigned int pvr2_copy_i2c_addr_list(
	unsigned short *dst, const unsigned char *src,
	unsigned int dst_max)
{
	unsigned int cnt = 0;
	if (!src) return 0;
	while (src[cnt] && (cnt + 1) < dst_max) {
		dst[cnt] = src[cnt];
		cnt++;
	}
	dst[cnt] = I2C_CLIENT_END;
	return cnt;
}


static void pvr2_hdw_cx25840_vbi_hack(struct pvr2_hdw *hdw)
{
	/*
	  Mike Isely <isely@pobox.com> 19-Nov-2006 - This bit of nuttiness
	  for cx25840 causes that module to correctly set up its video
	  scaling.  This is really a problem in the cx25840 module itself,
	  but we work around it here.  The problem has not been seen in
	  ivtv because there VBI is supported and set up.  We don't do VBI
	  here (at least not yet) and thus we never attempted to even set
	  it up.
	*/
	struct v4l2_format fmt;
	if (hdw->decoder_client_id != PVR2_CLIENT_ID_CX25840) {
		/* We're not using a cx25840 so don't enable the hack */
		return;
	}

	pvr2_trace(PVR2_TRACE_INIT,
		   "Module ID %u:"
		   " Executing cx25840 VBI hack",
		   hdw->decoder_client_id);
	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_SLICED_VBI_CAPTURE;
	v4l2_device_call_all(&hdw->v4l2_dev, hdw->decoder_client_id,
			     video, s_fmt, &fmt);
}


static int pvr2_hdw_load_subdev(struct pvr2_hdw *hdw,
				const struct pvr2_device_client_desc *cd)
{
	const char *fname;
	unsigned char mid;
	struct v4l2_subdev *sd;
	unsigned int i2ccnt;
	const unsigned char *p;
	/* Arbitrary count - max # i2c addresses we will probe */
	unsigned short i2caddr[25];

	mid = cd->module_id;
	fname = (mid < ARRAY_SIZE(module_names)) ? module_names[mid] : NULL;
	if (!fname) {
		pvr2_trace(PVR2_TRACE_ERROR_LEGS,
			   "Module ID %u for device %s has no name",
			   mid,
			   hdw->hdw_desc->description);
		return -EINVAL;
	}
	pvr2_trace(PVR2_TRACE_INIT,
		   "Module ID %u (%s) for device %s being loaded...",
		   mid, fname,
		   hdw->hdw_desc->description);

	i2ccnt = pvr2_copy_i2c_addr_list(i2caddr, cd->i2c_address_list,
					 ARRAY_SIZE(i2caddr));
	if (!i2ccnt && ((p = (mid < ARRAY_SIZE(module_i2c_addresses)) ?
			 module_i2c_addresses[mid] : NULL) != NULL)) {
		/* Second chance: Try default i2c address list */
		i2ccnt = pvr2_copy_i2c_addr_list(i2caddr, p,
						 ARRAY_SIZE(i2caddr));
		if (i2ccnt) {
			pvr2_trace(PVR2_TRACE_INIT,
				   "Module ID %u:"
				   " Using default i2c address list",
				   mid);
		}
	}

	if (!i2ccnt) {
		pvr2_trace(PVR2_TRACE_ERROR_LEGS,
			   "Module ID %u (%s) for device %s:"
			   " No i2c addresses",
			   mid, fname, hdw->hdw_desc->description);
		return -EINVAL;
	}

	/* Note how the 2nd and 3rd arguments are the same for
	 * v4l2_i2c_new_subdev().  Why?
	 * Well the 2nd argument is the module name to load, while the 3rd
	 * argument is documented in the framework as being the "chipid" -
	 * and every other place where I can find examples of this, the
	 * "chipid" appears to just be the module name again.  So here we
	 * just do the same thing. */
	if (i2ccnt == 1) {
		pvr2_trace(PVR2_TRACE_INIT,
			   "Module ID %u:"
			   " Setting up with specified i2c address 0x%x",
			   mid, i2caddr[0]);
		sd = v4l2_i2c_new_subdev(&hdw->v4l2_dev, &hdw->i2c_adap,
					 fname, fname,
					 i2caddr[0], NULL);
	} else {
		pvr2_trace(PVR2_TRACE_INIT,
			   "Module ID %u:"
			   " Setting up with address probe list",
			   mid);
		sd = v4l2_i2c_new_subdev(&hdw->v4l2_dev, &hdw->i2c_adap,
						fname, fname,
						0, i2caddr);
	}

	if (!sd) {
		pvr2_trace(PVR2_TRACE_ERROR_LEGS,
			   "Module ID %u (%s) for device %s failed to load",
			   mid, fname, hdw->hdw_desc->description);
		return -EIO;
	}

	/* Tag this sub-device instance with the module ID we know about.
	   In other places we'll use that tag to determine if the instance
	   requires special handling. */
	sd->grp_id = mid;

	pvr2_trace(PVR2_TRACE_INFO, "Attached sub-driver %s", fname);


	/* client-specific setup... */
	switch (mid) {
	case PVR2_CLIENT_ID_CX25840:
	case PVR2_CLIENT_ID_SAA7115:
		hdw->decoder_client_id = mid;
		break;
	default: break;
	}

	return 0;
}


static void pvr2_hdw_load_modules(struct pvr2_hdw *hdw)
{
	unsigned int idx;
	const struct pvr2_string_table *cm;
	const struct pvr2_device_client_table *ct;
	int okFl = !0;

	cm = &hdw->hdw_desc->client_modules;
	for (idx = 0; idx < cm->cnt; idx++) {
		request_module(cm->lst[idx]);
	}

	ct = &hdw->hdw_desc->client_table;
	for (idx = 0; idx < ct->cnt; idx++) {
		if (pvr2_hdw_load_subdev(hdw, &ct->lst[idx]) < 0) okFl = 0;
	}
	if (!okFl) pvr2_hdw_render_useless(hdw);
}


static void pvr2_hdw_setup_low(struct pvr2_hdw *hdw)
{
	int ret;
	unsigned int idx;
	struct pvr2_ctrl *cptr;
	int reloadFl = 0;
	if (hdw->hdw_desc->fx2_firmware.cnt) {
		if (!reloadFl) {
			reloadFl =
				(hdw->usb_intf->cur_altsetting->desc.bNumEndpoints
				 == 0);
			if (reloadFl) {
				pvr2_trace(PVR2_TRACE_INIT,
					   "USB endpoint config looks strange"
					   "; possibly firmware needs to be"
					   " loaded");
			}
		}
		if (!reloadFl) {
			reloadFl = !pvr2_hdw_check_firmware(hdw);
			if (reloadFl) {
				pvr2_trace(PVR2_TRACE_INIT,
					   "Check for FX2 firmware failed"
					   "; possibly firmware needs to be"
					   " loaded");
			}
		}
		if (reloadFl) {
			if (pvr2_upload_firmware1(hdw) != 0) {
				pvr2_trace(PVR2_TRACE_ERROR_LEGS,
					   "Failure uploading firmware1");
			}
			return;
		}
	}
	hdw->fw1_state = FW1_STATE_OK;

	if (!pvr2_hdw_dev_ok(hdw)) return;

	hdw->force_dirty = !0;

	if (!hdw->hdw_desc->flag_no_powerup) {
		pvr2_hdw_cmd_powerup(hdw);
		if (!pvr2_hdw_dev_ok(hdw)) return;
	}

	/* Take the IR chip out of reset, if appropriate */
	if (hdw->ir_scheme_active == PVR2_IR_SCHEME_ZILOG) {
		pvr2_issue_simple_cmd(hdw,
				      FX2CMD_HCW_ZILOG_RESET |
				      (1 << 8) |
				      ((0) << 16));
	}

	// This step MUST happen after the earlier powerup step.
	pvr2_i2c_core_init(hdw);
	if (!pvr2_hdw_dev_ok(hdw)) return;

	pvr2_hdw_load_modules(hdw);
	if (!pvr2_hdw_dev_ok(hdw)) return;

	v4l2_device_call_all(&hdw->v4l2_dev, 0, core, load_fw);

	for (idx = 0; idx < CTRLDEF_COUNT; idx++) {
		cptr = hdw->controls + idx;
		if (cptr->info->skip_init) continue;
		if (!cptr->info->set_value) continue;
		cptr->info->set_value(cptr,~0,cptr->info->default_value);
	}

	pvr2_hdw_cx25840_vbi_hack(hdw);

	/* Set up special default values for the television and radio
	   frequencies here.  It's not really important what these defaults
	   are, but I set them to something usable in the Chicago area just
	   to make driver testing a little easier. */

	hdw->freqValTelevision = default_tv_freq;
	hdw->freqValRadio = default_radio_freq;

	// Do not use pvr2_reset_ctl_endpoints() here.  It is not
	// thread-safe against the normal pvr2_send_request() mechanism.
	// (We should make it thread safe).

	if (hdw->hdw_desc->flag_has_hauppauge_rom) {
		ret = pvr2_hdw_get_eeprom_addr(hdw);
		if (!pvr2_hdw_dev_ok(hdw)) return;
		if (ret < 0) {
			pvr2_trace(PVR2_TRACE_ERROR_LEGS,
				   "Unable to determine location of eeprom,"
				   " skipping");
		} else {
			hdw->eeprom_addr = ret;
			pvr2_eeprom_analyze(hdw);
			if (!pvr2_hdw_dev_ok(hdw)) return;
		}
	} else {
		hdw->tuner_type = hdw->hdw_desc->default_tuner_type;
		hdw->tuner_updated = !0;
		hdw->std_mask_eeprom = V4L2_STD_ALL;
	}

	if (hdw->serial_number) {
		idx = scnprintf(hdw->identifier, sizeof(hdw->identifier) - 1,
				"sn-%lu", hdw->serial_number);
	} else if (hdw->unit_number >= 0) {
		idx = scnprintf(hdw->identifier, sizeof(hdw->identifier) - 1,
				"unit-%c",
				hdw->unit_number + 'a');
	} else {
		idx = scnprintf(hdw->identifier, sizeof(hdw->identifier) - 1,
				"unit-??");
	}
	hdw->identifier[idx] = 0;

	pvr2_hdw_setup_std(hdw);

	if (!get_default_tuner_type(hdw)) {
		pvr2_trace(PVR2_TRACE_INIT,
			   "pvr2_hdw_setup: Tuner type overridden to %d",
			   hdw->tuner_type);
	}


	if (!pvr2_hdw_dev_ok(hdw)) return;

	if (hdw->hdw_desc->signal_routing_scheme ==
	    PVR2_ROUTING_SCHEME_GOTVIEW) {
		/* Ensure that GPIO 11 is set to output for GOTVIEW
		   hardware. */
		pvr2_hdw_gpio_chg_dir(hdw,(1 << 11),~0);
	}

	pvr2_hdw_commit_setup(hdw);

	hdw->vid_stream = pvr2_stream_create();
	if (!pvr2_hdw_dev_ok(hdw)) return;
	pvr2_trace(PVR2_TRACE_INIT,
		   "pvr2_hdw_setup: video stream is %p",hdw->vid_stream);
	if (hdw->vid_stream) {
		idx = get_default_error_tolerance(hdw);
		if (idx) {
			pvr2_trace(PVR2_TRACE_INIT,
				   "pvr2_hdw_setup: video stream %p"
				   " setting tolerance %u",
				   hdw->vid_stream,idx);
		}
		pvr2_stream_setup(hdw->vid_stream,hdw->usb_dev,
				  PVR2_VID_ENDPOINT,idx);
	}

	if (!pvr2_hdw_dev_ok(hdw)) return;

	hdw->flag_init_ok = !0;

	pvr2_hdw_state_sched(hdw);
}


/* Set up the structure and attempt to put the device into a usable state.
   This can be a time-consuming operation, which is why it is not done
   internally as part of the create() step. */
static void pvr2_hdw_setup(struct pvr2_hdw *hdw)
{
	pvr2_trace(PVR2_TRACE_INIT,"pvr2_hdw_setup(hdw=%p) begin",hdw);
	do {
		pvr2_hdw_setup_low(hdw);
		pvr2_trace(PVR2_TRACE_INIT,
			   "pvr2_hdw_setup(hdw=%p) done, ok=%d init_ok=%d",
			   hdw,pvr2_hdw_dev_ok(hdw),hdw->flag_init_ok);
		if (pvr2_hdw_dev_ok(hdw)) {
			if (hdw->flag_init_ok) {
				pvr2_trace(
					PVR2_TRACE_INFO,
					"Device initialization"
					" completed successfully.");
				break;
			}
			if (hdw->fw1_state == FW1_STATE_RELOAD) {
				pvr2_trace(
					PVR2_TRACE_INFO,
					"Device microcontroller firmware"
					" (re)loaded; it should now reset"
					" and reconnect.");
				break;
			}
			pvr2_trace(
				PVR2_TRACE_ERROR_LEGS,
				"Device initialization was not successful.");
			if (hdw->fw1_state == FW1_STATE_MISSING) {
				pvr2_trace(
					PVR2_TRACE_ERROR_LEGS,
					"Giving up since device"
					" microcontroller firmware"
					" appears to be missing.");
				break;
			}
		}
		if (procreload) {
			pvr2_trace(
				PVR2_TRACE_ERROR_LEGS,
				"Attempting pvrusb2 recovery by reloading"
				" primary firmware.");
			pvr2_trace(
				PVR2_TRACE_ERROR_LEGS,
				"If this works, device should disconnect"
				" and reconnect in a sane state.");
			hdw->fw1_state = FW1_STATE_UNKNOWN;
			pvr2_upload_firmware1(hdw);
		} else {
			pvr2_trace(
				PVR2_TRACE_ERROR_LEGS,
				"***WARNING*** pvrusb2 device hardware"
				" appears to be jammed"
				" and I can't clear it.");
			pvr2_trace(
				PVR2_TRACE_ERROR_LEGS,
				"You might need to power cycle"
				" the pvrusb2 device"
				" in order to recover.");
		}
	} while (0);
	pvr2_trace(PVR2_TRACE_INIT,"pvr2_hdw_setup(hdw=%p) end",hdw);
}


/* Perform second stage initialization.  Set callback pointer first so that
   we can avoid a possible initialization race (if the kernel thread runs
   before the callback has been set). */
int pvr2_hdw_initialize(struct pvr2_hdw *hdw,
			void (*callback_func)(void *),
			void *callback_data)
{
	LOCK_TAKE(hdw->big_lock); do {
		if (hdw->flag_disconnected) {
			/* Handle a race here: If we're already
			   disconnected by this point, then give up.  If we
			   get past this then we'll remain connected for
			   the duration of initialization since the entire
			   initialization sequence is now protected by the
			   big_lock. */
			break;
		}
		hdw->state_data = callback_data;
		hdw->state_func = callback_func;
		pvr2_hdw_setup(hdw);
	} while (0); LOCK_GIVE(hdw->big_lock);
	return hdw->flag_init_ok;
}


/* Create, set up, and return a structure for interacting with the
   underlying hardware.  */
struct pvr2_hdw *pvr2_hdw_create(struct usb_interface *intf,
				 const struct usb_device_id *devid)
{
	unsigned int idx,cnt1,cnt2,m;
	struct pvr2_hdw *hdw = NULL;
	int valid_std_mask;
	struct pvr2_ctrl *cptr;
	struct usb_device *usb_dev;
	const struct pvr2_device_desc *hdw_desc;
	__u8 ifnum;
	struct v4l2_queryctrl qctrl;
	struct pvr2_ctl_info *ciptr;

	usb_dev = interface_to_usbdev(intf);

	hdw_desc = (const struct pvr2_device_desc *)(devid->driver_info);

	if (hdw_desc == NULL) {
		pvr2_trace(PVR2_TRACE_INIT, "pvr2_hdw_create:"
			   " No device description pointer,"
			   " unable to continue.");
		pvr2_trace(PVR2_TRACE_INIT, "If you have a new device type,"
			   " please contact Mike Isely <isely@pobox.com>"
			   " to get it included in the driver\n");
		goto fail;
	}

	hdw = kzalloc(sizeof(*hdw),GFP_KERNEL);
	pvr2_trace(PVR2_TRACE_INIT,"pvr2_hdw_create: hdw=%p, type \"%s\"",
		   hdw,hdw_desc->description);
	if (!hdw) goto fail;

	init_timer(&hdw->quiescent_timer);
	hdw->quiescent_timer.data = (unsigned long)hdw;
	hdw->quiescent_timer.function = pvr2_hdw_quiescent_timeout;

	init_timer(&hdw->encoder_wait_timer);
	hdw->encoder_wait_timer.data = (unsigned long)hdw;
	hdw->encoder_wait_timer.function = pvr2_hdw_encoder_wait_timeout;

	init_timer(&hdw->encoder_run_timer);
	hdw->encoder_run_timer.data = (unsigned long)hdw;
	hdw->encoder_run_timer.function = pvr2_hdw_encoder_run_timeout;

	hdw->master_state = PVR2_STATE_DEAD;

	init_waitqueue_head(&hdw->state_wait_data);

	hdw->tuner_signal_stale = !0;
	cx2341x_fill_defaults(&hdw->enc_ctl_state);

	/* Calculate which inputs are OK */
	m = 0;
	if (hdw_desc->flag_has_analogtuner) m |= 1 << PVR2_CVAL_INPUT_TV;
	if (hdw_desc->digital_control_scheme != PVR2_DIGITAL_SCHEME_NONE) {
		m |= 1 << PVR2_CVAL_INPUT_DTV;
	}
	if (hdw_desc->flag_has_svideo) m |= 1 << PVR2_CVAL_INPUT_SVIDEO;
	if (hdw_desc->flag_has_composite) m |= 1 << PVR2_CVAL_INPUT_COMPOSITE;
	if (hdw_desc->flag_has_fmradio) m |= 1 << PVR2_CVAL_INPUT_RADIO;
	hdw->input_avail_mask = m;
	hdw->input_allowed_mask = hdw->input_avail_mask;

	/* If not a hybrid device, pathway_state never changes.  So
	   initialize it here to what it should forever be. */
	if (!(hdw->input_avail_mask & (1 << PVR2_CVAL_INPUT_DTV))) {
		hdw->pathway_state = PVR2_PATHWAY_ANALOG;
	} else if (!(hdw->input_avail_mask & (1 << PVR2_CVAL_INPUT_TV))) {
		hdw->pathway_state = PVR2_PATHWAY_DIGITAL;
	}

	hdw->control_cnt = CTRLDEF_COUNT;
	hdw->control_cnt += MPEGDEF_COUNT;
	hdw->controls = kzalloc(sizeof(struct pvr2_ctrl) * hdw->control_cnt,
				GFP_KERNEL);
	if (!hdw->controls) goto fail;
	hdw->hdw_desc = hdw_desc;
	hdw->ir_scheme_active = hdw->hdw_desc->ir_scheme;
	for (idx = 0; idx < hdw->control_cnt; idx++) {
		cptr = hdw->controls + idx;
		cptr->hdw = hdw;
	}
	for (idx = 0; idx < 32; idx++) {
		hdw->std_mask_ptrs[idx] = hdw->std_mask_names[idx];
	}
	for (idx = 0; idx < CTRLDEF_COUNT; idx++) {
		cptr = hdw->controls + idx;
		cptr->info = control_defs+idx;
	}

	/* Ensure that default input choice is a valid one. */
	m = hdw->input_avail_mask;
	if (m) for (idx = 0; idx < (sizeof(m) << 3); idx++) {
		if (!((1 << idx) & m)) continue;
		hdw->input_val = idx;
		break;
	}

	/* Define and configure additional controls from cx2341x module. */
	hdw->mpeg_ctrl_info = kzalloc(
		sizeof(*(hdw->mpeg_ctrl_info)) * MPEGDEF_COUNT, GFP_KERNEL);
	if (!hdw->mpeg_ctrl_info) goto fail;
	for (idx = 0; idx < MPEGDEF_COUNT; idx++) {
		cptr = hdw->controls + idx + CTRLDEF_COUNT;
		ciptr = &(hdw->mpeg_ctrl_info[idx].info);
		ciptr->desc = hdw->mpeg_ctrl_info[idx].desc;
		ciptr->name = mpeg_ids[idx].strid;
		ciptr->v4l_id = mpeg_ids[idx].id;
		ciptr->skip_init = !0;
		ciptr->get_value = ctrl_cx2341x_get;
		ciptr->get_v4lflags = ctrl_cx2341x_getv4lflags;
		ciptr->is_dirty = ctrl_cx2341x_is_dirty;
		if (!idx) ciptr->clear_dirty = ctrl_cx2341x_clear_dirty;
		qctrl.id = ciptr->v4l_id;
		cx2341x_ctrl_query(&hdw->enc_ctl_state,&qctrl);
		if (!(qctrl.flags & V4L2_CTRL_FLAG_READ_ONLY)) {
			ciptr->set_value = ctrl_cx2341x_set;
		}
		strncpy(hdw->mpeg_ctrl_info[idx].desc,qctrl.name,
			PVR2_CTLD_INFO_DESC_SIZE);
		hdw->mpeg_ctrl_info[idx].desc[PVR2_CTLD_INFO_DESC_SIZE-1] = 0;
		ciptr->default_value = qctrl.default_value;
		switch (qctrl.type) {
		default:
		case V4L2_CTRL_TYPE_INTEGER:
			ciptr->type = pvr2_ctl_int;
			ciptr->def.type_int.min_value = qctrl.minimum;
			ciptr->def.type_int.max_value = qctrl.maximum;
			break;
		case V4L2_CTRL_TYPE_BOOLEAN:
			ciptr->type = pvr2_ctl_bool;
			break;
		case V4L2_CTRL_TYPE_MENU:
			ciptr->type = pvr2_ctl_enum;
			ciptr->def.type_enum.value_names =
				cx2341x_ctrl_get_menu(&hdw->enc_ctl_state,
								ciptr->v4l_id);
			for (cnt1 = 0;
			     ciptr->def.type_enum.value_names[cnt1] != NULL;
			     cnt1++) { }
			ciptr->def.type_enum.count = cnt1;
			break;
		}
		cptr->info = ciptr;
	}

	// Initialize video standard enum dynamic control
	cptr = pvr2_hdw_get_ctrl_by_id(hdw,PVR2_CID_STDENUM);
	if (cptr) {
		memcpy(&hdw->std_info_enum,cptr->info,
		       sizeof(hdw->std_info_enum));
		cptr->info = &hdw->std_info_enum;

	}
	// Initialize control data regarding video standard masks
	valid_std_mask = pvr2_std_get_usable();
	for (idx = 0; idx < 32; idx++) {
		if (!(valid_std_mask & (1 << idx))) continue;
		cnt1 = pvr2_std_id_to_str(
			hdw->std_mask_names[idx],
			sizeof(hdw->std_mask_names[idx])-1,
			1 << idx);
		hdw->std_mask_names[idx][cnt1] = 0;
	}
	cptr = pvr2_hdw_get_ctrl_by_id(hdw,PVR2_CID_STDAVAIL);
	if (cptr) {
		memcpy(&hdw->std_info_avail,cptr->info,
		       sizeof(hdw->std_info_avail));
		cptr->info = &hdw->std_info_avail;
		hdw->std_info_avail.def.type_bitmask.bit_names =
			hdw->std_mask_ptrs;
		hdw->std_info_avail.def.type_bitmask.valid_bits =
			valid_std_mask;
	}
	cptr = pvr2_hdw_get_ctrl_by_id(hdw,PVR2_CID_STDCUR);
	if (cptr) {
		memcpy(&hdw->std_info_cur,cptr->info,
		       sizeof(hdw->std_info_cur));
		cptr->info = &hdw->std_info_cur;
		hdw->std_info_cur.def.type_bitmask.bit_names =
			hdw->std_mask_ptrs;
		hdw->std_info_avail.def.type_bitmask.valid_bits =
			valid_std_mask;
	}

	hdw->cropcap_stale = !0;
	hdw->eeprom_addr = -1;
	hdw->unit_number = -1;
	hdw->v4l_minor_number_video = -1;
	hdw->v4l_minor_number_vbi = -1;
	hdw->v4l_minor_number_radio = -1;
	hdw->ctl_write_buffer = kmalloc(PVR2_CTL_BUFFSIZE,GFP_KERNEL);
	if (!hdw->ctl_write_buffer) goto fail;
	hdw->ctl_read_buffer = kmalloc(PVR2_CTL_BUFFSIZE,GFP_KERNEL);
	if (!hdw->ctl_read_buffer) goto fail;
	hdw->ctl_write_urb = usb_alloc_urb(0,GFP_KERNEL);
	if (!hdw->ctl_write_urb) goto fail;
	hdw->ctl_read_urb = usb_alloc_urb(0,GFP_KERNEL);
	if (!hdw->ctl_read_urb) goto fail;

	if (v4l2_device_register(&intf->dev, &hdw->v4l2_dev) != 0) {
		pvr2_trace(PVR2_TRACE_ERROR_LEGS,
			   "Error registering with v4l core, giving up");
		goto fail;
	}
	mutex_lock(&pvr2_unit_mtx); do {
		for (idx = 0; idx < PVR_NUM; idx++) {
			if (unit_pointers[idx]) continue;
			hdw->unit_number = idx;
			unit_pointers[idx] = hdw;
			break;
		}
	} while (0); mutex_unlock(&pvr2_unit_mtx);

	cnt1 = 0;
	cnt2 = scnprintf(hdw->name+cnt1,sizeof(hdw->name)-cnt1,"pvrusb2");
	cnt1 += cnt2;
	if (hdw->unit_number >= 0) {
		cnt2 = scnprintf(hdw->name+cnt1,sizeof(hdw->name)-cnt1,"_%c",
				 ('a' + hdw->unit_number));
		cnt1 += cnt2;
	}
	if (cnt1 >= sizeof(hdw->name)) cnt1 = sizeof(hdw->name)-1;
	hdw->name[cnt1] = 0;

	hdw->workqueue = create_singlethread_workqueue(hdw->name);
	INIT_WORK(&hdw->workpoll,pvr2_hdw_worker_poll);

	pvr2_trace(PVR2_TRACE_INIT,"Driver unit number is %d, name is %s",
		   hdw->unit_number,hdw->name);

	hdw->tuner_type = -1;
	hdw->flag_ok = !0;

	hdw->usb_intf = intf;
	hdw->usb_dev = usb_dev;

	usb_make_path(hdw->usb_dev, hdw->bus_info, sizeof(hdw->bus_info));

	ifnum = hdw->usb_intf->cur_altsetting->desc.bInterfaceNumber;
	usb_set_interface(hdw->usb_dev,ifnum,0);

	mutex_init(&hdw->ctl_lock_mutex);
	mutex_init(&hdw->big_lock_mutex);

	return hdw;
 fail:
	if (hdw) {
		del_timer_sync(&hdw->quiescent_timer);
		del_timer_sync(&hdw->encoder_run_timer);
		del_timer_sync(&hdw->encoder_wait_timer);
		if (hdw->workqueue) {
			flush_workqueue(hdw->workqueue);
			destroy_workqueue(hdw->workqueue);
			hdw->workqueue = NULL;
		}
		usb_free_urb(hdw->ctl_read_urb);
		usb_free_urb(hdw->ctl_write_urb);
		kfree(hdw->ctl_read_buffer);
		kfree(hdw->ctl_write_buffer);
		kfree(hdw->controls);
		kfree(hdw->mpeg_ctrl_info);
		kfree(hdw->std_defs);
		kfree(hdw->std_enum_names);
		kfree(hdw);
	}
	return NULL;
}


/* Remove _all_ associations between this driver and the underlying USB
   layer. */
static void pvr2_hdw_remove_usb_stuff(struct pvr2_hdw *hdw)
{
	if (hdw->flag_disconnected) return;
	pvr2_trace(PVR2_TRACE_INIT,"pvr2_hdw_remove_usb_stuff: hdw=%p",hdw);
	if (hdw->ctl_read_urb) {
		usb_kill_urb(hdw->ctl_read_urb);
		usb_free_urb(hdw->ctl_read_urb);
		hdw->ctl_read_urb = NULL;
	}
	if (hdw->ctl_write_urb) {
		usb_kill_urb(hdw->ctl_write_urb);
		usb_free_urb(hdw->ctl_write_urb);
		hdw->ctl_write_urb = NULL;
	}
	if (hdw->ctl_read_buffer) {
		kfree(hdw->ctl_read_buffer);
		hdw->ctl_read_buffer = NULL;
	}
	if (hdw->ctl_write_buffer) {
		kfree(hdw->ctl_write_buffer);
		hdw->ctl_write_buffer = NULL;
	}
	hdw->flag_disconnected = !0;
	/* If we don't do this, then there will be a dangling struct device
	   reference to our disappearing device persisting inside the V4L
	   core... */
	v4l2_device_disconnect(&hdw->v4l2_dev);
	hdw->usb_dev = NULL;
	hdw->usb_intf = NULL;
	pvr2_hdw_render_useless(hdw);
}


/* Destroy hardware interaction structure */
void pvr2_hdw_destroy(struct pvr2_hdw *hdw)
{
	if (!hdw) return;
	pvr2_trace(PVR2_TRACE_INIT,"pvr2_hdw_destroy: hdw=%p",hdw);
	if (hdw->workqueue) {
		flush_workqueue(hdw->workqueue);
		destroy_workqueue(hdw->workqueue);
		hdw->workqueue = NULL;
	}
	del_timer_sync(&hdw->quiescent_timer);
	del_timer_sync(&hdw->encoder_run_timer);
	del_timer_sync(&hdw->encoder_wait_timer);
	if (hdw->fw_buffer) {
		kfree(hdw->fw_buffer);
		hdw->fw_buffer = NULL;
	}
	if (hdw->vid_stream) {
		pvr2_stream_destroy(hdw->vid_stream);
		hdw->vid_stream = NULL;
	}
	pvr2_i2c_core_done(hdw);
	v4l2_device_unregister(&hdw->v4l2_dev);
	pvr2_hdw_remove_usb_stuff(hdw);
	mutex_lock(&pvr2_unit_mtx); do {
		if ((hdw->unit_number >= 0) &&
		    (hdw->unit_number < PVR_NUM) &&
		    (unit_pointers[hdw->unit_number] == hdw)) {
			unit_pointers[hdw->unit_number] = NULL;
		}
	} while (0); mutex_unlock(&pvr2_unit_mtx);
	kfree(hdw->controls);
	kfree(hdw->mpeg_ctrl_info);
	kfree(hdw->std_defs);
	kfree(hdw->std_enum_names);
	kfree(hdw);
}


int pvr2_hdw_dev_ok(struct pvr2_hdw *hdw)
{
	return (hdw && hdw->flag_ok);
}


/* Called when hardware has been unplugged */
void pvr2_hdw_disconnect(struct pvr2_hdw *hdw)
{
	pvr2_trace(PVR2_TRACE_INIT,"pvr2_hdw_disconnect(hdw=%p)",hdw);
	LOCK_TAKE(hdw->big_lock);
	LOCK_TAKE(hdw->ctl_lock);
	pvr2_hdw_remove_usb_stuff(hdw);
	LOCK_GIVE(hdw->ctl_lock);
	LOCK_GIVE(hdw->big_lock);
}


// Attempt to autoselect an appropriate value for std_enum_cur given
// whatever is currently in std_mask_cur
static void pvr2_hdw_internal_find_stdenum(struct pvr2_hdw *hdw)
{
	unsigned int idx;
	for (idx = 1; idx < hdw->std_enum_cnt; idx++) {
		if (hdw->std_defs[idx-1].id == hdw->std_mask_cur) {
			hdw->std_enum_cur = idx;
			return;
		}
	}
	hdw->std_enum_cur = 0;
}


// Calculate correct set of enumerated standards based on currently known
// set of available standards bits.
static void pvr2_hdw_internal_set_std_avail(struct pvr2_hdw *hdw)
{
	struct v4l2_standard *newstd;
	unsigned int std_cnt;
	unsigned int idx;

	newstd = pvr2_std_create_enum(&std_cnt,hdw->std_mask_avail);

	if (hdw->std_defs) {
		kfree(hdw->std_defs);
		hdw->std_defs = NULL;
	}
	hdw->std_enum_cnt = 0;
	if (hdw->std_enum_names) {
		kfree(hdw->std_enum_names);
		hdw->std_enum_names = NULL;
	}

	if (!std_cnt) {
		pvr2_trace(
			PVR2_TRACE_ERROR_LEGS,
			"WARNING: Failed to identify any viable standards");
	}
	hdw->std_enum_names = kmalloc(sizeof(char *)*(std_cnt+1),GFP_KERNEL);
	hdw->std_enum_names[0] = "none";
	for (idx = 0; idx < std_cnt; idx++) {
		hdw->std_enum_names[idx+1] =
			newstd[idx].name;
	}
	// Set up the dynamic control for this standard
	hdw->std_info_enum.def.type_enum.value_names = hdw->std_enum_names;
	hdw->std_info_enum.def.type_enum.count = std_cnt+1;
	hdw->std_defs = newstd;
	hdw->std_enum_cnt = std_cnt+1;
	hdw->std_enum_cur = 0;
	hdw->std_info_cur.def.type_bitmask.valid_bits = hdw->std_mask_avail;
}


int pvr2_hdw_get_stdenum_value(struct pvr2_hdw *hdw,
			       struct v4l2_standard *std,
			       unsigned int idx)
{
	int ret = -EINVAL;
	if (!idx) return ret;
	LOCK_TAKE(hdw->big_lock); do {
		if (idx >= hdw->std_enum_cnt) break;
		idx--;
		memcpy(std,hdw->std_defs+idx,sizeof(*std));
		ret = 0;
	} while (0); LOCK_GIVE(hdw->big_lock);
	return ret;
}


/* Get the number of defined controls */
unsigned int pvr2_hdw_get_ctrl_count(struct pvr2_hdw *hdw)
{
	return hdw->control_cnt;
}


/* Retrieve a control handle given its index (0..count-1) */
struct pvr2_ctrl *pvr2_hdw_get_ctrl_by_index(struct pvr2_hdw *hdw,
					     unsigned int idx)
{
	if (idx >= hdw->control_cnt) return NULL;
	return hdw->controls + idx;
}


/* Retrieve a control handle given its index (0..count-1) */
struct pvr2_ctrl *pvr2_hdw_get_ctrl_by_id(struct pvr2_hdw *hdw,
					  unsigned int ctl_id)
{
	struct pvr2_ctrl *cptr;
	unsigned int idx;
	int i;

	/* This could be made a lot more efficient, but for now... */
	for (idx = 0; idx < hdw->control_cnt; idx++) {
		cptr = hdw->controls + idx;
		i = cptr->info->internal_id;
		if (i && (i == ctl_id)) return cptr;
	}
	return NULL;
}


/* Given a V4L ID, retrieve the control structure associated with it. */
struct pvr2_ctrl *pvr2_hdw_get_ctrl_v4l(struct pvr2_hdw *hdw,unsigned int ctl_id)
{
	struct pvr2_ctrl *cptr;
	unsigned int idx;
	int i;

	/* This could be made a lot more efficient, but for now... */
	for (idx = 0; idx < hdw->control_cnt; idx++) {
		cptr = hdw->controls + idx;
		i = cptr->info->v4l_id;
		if (i && (i == ctl_id)) return cptr;
	}
	return NULL;
}


/* Given a V4L ID for its immediate predecessor, retrieve the control
   structure associated with it. */
struct pvr2_ctrl *pvr2_hdw_get_ctrl_nextv4l(struct pvr2_hdw *hdw,
					    unsigned int ctl_id)
{
	struct pvr2_ctrl *cptr,*cp2;
	unsigned int idx;
	int i;

	/* This could be made a lot more efficient, but for now... */
	cp2 = NULL;
	for (idx = 0; idx < hdw->control_cnt; idx++) {
		cptr = hdw->controls + idx;
		i = cptr->info->v4l_id;
		if (!i) continue;
		if (i <= ctl_id) continue;
		if (cp2 && (cp2->info->v4l_id < i)) continue;
		cp2 = cptr;
	}
	return cp2;
	return NULL;
}


static const char *get_ctrl_typename(enum pvr2_ctl_type tp)
{
	switch (tp) {
	case pvr2_ctl_int: return "integer";
	case pvr2_ctl_enum: return "enum";
	case pvr2_ctl_bool: return "boolean";
	case pvr2_ctl_bitmask: return "bitmask";
	}
	return "";
}


static void pvr2_subdev_set_control(struct pvr2_hdw *hdw, int id,
				    const char *name, int val)
{
	struct v4l2_control ctrl;
	pvr2_trace(PVR2_TRACE_CHIPS, "subdev v4l2 %s=%d", name, val);
	memset(&ctrl, 0, sizeof(ctrl));
	ctrl.id = id;
	ctrl.value = val;
	v4l2_device_call_all(&hdw->v4l2_dev, 0, core, s_ctrl, &ctrl);
}

#define PVR2_SUBDEV_SET_CONTROL(hdw, id, lab) \
	if ((hdw)->lab##_dirty || (hdw)->force_dirty) {		\
		pvr2_subdev_set_control(hdw, id, #lab, (hdw)->lab##_val); \
	}

/* Execute whatever commands are required to update the state of all the
   sub-devices so that they match our current control values. */
static void pvr2_subdev_update(struct pvr2_hdw *hdw)
{
	struct v4l2_subdev *sd;
	unsigned int id;
	pvr2_subdev_update_func fp;

	pvr2_trace(PVR2_TRACE_CHIPS, "subdev update...");

	if (hdw->tuner_updated || hdw->force_dirty) {
		struct tuner_setup setup;
		pvr2_trace(PVR2_TRACE_CHIPS, "subdev tuner set_type(%d)",
			   hdw->tuner_type);
		if (((int)(hdw->tuner_type)) >= 0) {
			memset(&setup, 0, sizeof(setup));
			setup.addr = ADDR_UNSET;
			setup.type = hdw->tuner_type;
			setup.mode_mask = T_RADIO | T_ANALOG_TV;
			v4l2_device_call_all(&hdw->v4l2_dev, 0,
					     tuner, s_type_addr, &setup);
		}
	}

	if (hdw->input_dirty || hdw->std_dirty || hdw->force_dirty) {
		pvr2_trace(PVR2_TRACE_CHIPS, "subdev v4l2 set_standard");
		if (hdw->input_val == PVR2_CVAL_INPUT_RADIO) {
			v4l2_device_call_all(&hdw->v4l2_dev, 0,
					     tuner, s_radio);
		} else {
			v4l2_std_id vs;
			vs = hdw->std_mask_cur;
			v4l2_device_call_all(&hdw->v4l2_dev, 0,
					     core, s_std, vs);
			pvr2_hdw_cx25840_vbi_hack(hdw);
		}
		hdw->tuner_signal_stale = !0;
		hdw->cropcap_stale = !0;
	}

	PVR2_SUBDEV_SET_CONTROL(hdw, V4L2_CID_BRIGHTNESS, brightness);
	PVR2_SUBDEV_SET_CONTROL(hdw, V4L2_CID_CONTRAST, contrast);
	PVR2_SUBDEV_SET_CONTROL(hdw, V4L2_CID_SATURATION, saturation);
	PVR2_SUBDEV_SET_CONTROL(hdw, V4L2_CID_HUE, hue);
	PVR2_SUBDEV_SET_CONTROL(hdw, V4L2_CID_AUDIO_MUTE, mute);
	PVR2_SUBDEV_SET_CONTROL(hdw, V4L2_CID_AUDIO_VOLUME, volume);
	PVR2_SUBDEV_SET_CONTROL(hdw, V4L2_CID_AUDIO_BALANCE, balance);
	PVR2_SUBDEV_SET_CONTROL(hdw, V4L2_CID_AUDIO_BASS, bass);
	PVR2_SUBDEV_SET_CONTROL(hdw, V4L2_CID_AUDIO_TREBLE, treble);

	if (hdw->input_dirty || hdw->audiomode_dirty || hdw->force_dirty) {
		struct v4l2_tuner vt;
		memset(&vt, 0, sizeof(vt));
		vt.audmode = hdw->audiomode_val;
		v4l2_device_call_all(&hdw->v4l2_dev, 0, tuner, s_tuner, &vt);
	}

	if (hdw->freqDirty || hdw->force_dirty) {
		unsigned long fv;
		struct v4l2_frequency freq;
		fv = pvr2_hdw_get_cur_freq(hdw);
		pvr2_trace(PVR2_TRACE_CHIPS, "subdev v4l2 set_freq(%lu)", fv);
		if (hdw->tuner_signal_stale) pvr2_hdw_status_poll(hdw);
		memset(&freq, 0, sizeof(freq));
		if (hdw->tuner_signal_info.capability & V4L2_TUNER_CAP_LOW) {
			/* ((fv * 1000) / 62500) */
			freq.frequency = (fv * 2) / 125;
		} else {
			freq.frequency = fv / 62500;
		}
		/* tuner-core currently doesn't seem to care about this, but
		   let's set it anyway for completeness. */
		if (hdw->input_val == PVR2_CVAL_INPUT_RADIO) {
			freq.type = V4L2_TUNER_RADIO;
		} else {
			freq.type = V4L2_TUNER_ANALOG_TV;
		}
		freq.tuner = 0;
		v4l2_device_call_all(&hdw->v4l2_dev, 0, tuner,
				     s_frequency, &freq);
	}

	if (hdw->res_hor_dirty || hdw->res_ver_dirty || hdw->force_dirty) {
		struct v4l2_format fmt;
		memset(&fmt, 0, sizeof(fmt));
		fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		fmt.fmt.pix.width = hdw->res_hor_val;
		fmt.fmt.pix.height = hdw->res_ver_val;
		pvr2_trace(PVR2_TRACE_CHIPS, "subdev v4l2 set_size(%dx%d)",
			   fmt.fmt.pix.width, fmt.fmt.pix.height);
		v4l2_device_call_all(&hdw->v4l2_dev, 0, video, s_fmt, &fmt);
	}

	if (hdw->srate_dirty || hdw->force_dirty) {
		u32 val;
		pvr2_trace(PVR2_TRACE_CHIPS, "subdev v4l2 set_audio %d",
			   hdw->srate_val);
		switch (hdw->srate_val) {
		default:
		case V4L2_MPEG_AUDIO_SAMPLING_FREQ_48000:
			val = 48000;
			break;
		case V4L2_MPEG_AUDIO_SAMPLING_FREQ_44100:
			val = 44100;
			break;
		case V4L2_MPEG_AUDIO_SAMPLING_FREQ_32000:
			val = 32000;
			break;
		}
		v4l2_device_call_all(&hdw->v4l2_dev, 0,
				     audio, s_clock_freq, val);
	}

	/* Unable to set crop parameters; there is apparently no equivalent
	   for VIDIOC_S_CROP */

	v4l2_device_for_each_subdev(sd, &hdw->v4l2_dev) {
		id = sd->grp_id;
		if (id >= ARRAY_SIZE(pvr2_module_update_functions)) continue;
		fp = pvr2_module_update_functions[id];
		if (!fp) continue;
		(*fp)(hdw, sd);
	}

	if (hdw->tuner_signal_stale || hdw->cropcap_stale) {
		pvr2_hdw_status_poll(hdw);
	}
}


/* Figure out if we need to commit control changes.  If so, mark internal
   state flags to indicate this fact and return true.  Otherwise do nothing
   else and return false. */
static int pvr2_hdw_commit_setup(struct pvr2_hdw *hdw)
{
	unsigned int idx;
	struct pvr2_ctrl *cptr;
	int value;
	int commit_flag = hdw->force_dirty;
	char buf[100];
	unsigned int bcnt,ccnt;

	for (idx = 0; idx < hdw->control_cnt; idx++) {
		cptr = hdw->controls + idx;
		if (!cptr->info->is_dirty) continue;
		if (!cptr->info->is_dirty(cptr)) continue;
		commit_flag = !0;

		if (!(pvrusb2_debug & PVR2_TRACE_CTL)) continue;
		bcnt = scnprintf(buf,sizeof(buf),"\"%s\" <-- ",
				 cptr->info->name);
		value = 0;
		cptr->info->get_value(cptr,&value);
		pvr2_ctrl_value_to_sym_internal(cptr,~0,value,
						buf+bcnt,
						sizeof(buf)-bcnt,&ccnt);
		bcnt += ccnt;
		bcnt += scnprintf(buf+bcnt,sizeof(buf)-bcnt," <%s>",
				  get_ctrl_typename(cptr->info->type));
		pvr2_trace(PVR2_TRACE_CTL,
			   "/*--TRACE_COMMIT--*/ %.*s",
			   bcnt,buf);
	}

	if (!commit_flag) {
		/* Nothing has changed */
		return 0;
	}

	hdw->state_pipeline_config = 0;
	trace_stbit("state_pipeline_config",hdw->state_pipeline_config);
	pvr2_hdw_state_sched(hdw);

	return !0;
}


/* Perform all operations needed to commit all control changes.  This must
   be performed in synchronization with the pipeline state and is thus
   expected to be called as part of the driver's worker thread.  Return
   true if commit successful, otherwise return false to indicate that
   commit isn't possible at this time. */
static int pvr2_hdw_commit_execute(struct pvr2_hdw *hdw)
{
	unsigned int idx;
	struct pvr2_ctrl *cptr;
	int disruptive_change;

	/* Handle some required side effects when the video standard is
	   changed.... */
	if (hdw->std_dirty) {
		int nvres;
		int gop_size;
		if (hdw->std_mask_cur & V4L2_STD_525_60) {
			nvres = 480;
			gop_size = 15;
		} else {
			nvres = 576;
			gop_size = 12;
		}
		/* Rewrite the vertical resolution to be appropriate to the
		   video standard that has been selected. */
		if (nvres != hdw->res_ver_val) {
			hdw->res_ver_val = nvres;
			hdw->res_ver_dirty = !0;
		}
		/* Rewrite the GOP size to be appropriate to the video
		   standard that has been selected. */
		if (gop_size != hdw->enc_ctl_state.video_gop_size) {
			struct v4l2_ext_controls cs;
			struct v4l2_ext_control c1;
			memset(&cs, 0, sizeof(cs));
			memset(&c1, 0, sizeof(c1));
			cs.controls = &c1;
			cs.count = 1;
			c1.id = V4L2_CID_MPEG_VIDEO_GOP_SIZE;
			c1.value = gop_size;
			cx2341x_ext_ctrls(&hdw->enc_ctl_state, 0, &cs,
					  VIDIOC_S_EXT_CTRLS);
		}
	}

	if (hdw->input_dirty && hdw->state_pathway_ok &&
	    (((hdw->input_val == PVR2_CVAL_INPUT_DTV) ?
	      PVR2_PATHWAY_DIGITAL : PVR2_PATHWAY_ANALOG) !=
	     hdw->pathway_state)) {
		/* Change of mode being asked for... */
		hdw->state_pathway_ok = 0;
		trace_stbit("state_pathway_ok",hdw->state_pathway_ok);
	}
	if (!hdw->state_pathway_ok) {
		/* Can't commit anything until pathway is ok. */
		return 0;
	}
	/* The broadcast decoder can only scale down, so if
	 * res_*_dirty && crop window < output format ==> enlarge crop.
	 *
	 * The mpeg encoder receives fields of res_hor_val dots and
	 * res_ver_val halflines.  Limits: hor<=720, ver<=576.
	 */
	if (hdw->res_hor_dirty && hdw->cropw_val < hdw->res_hor_val) {
		hdw->cropw_val = hdw->res_hor_val;
		hdw->cropw_dirty = !0;
	} else if (hdw->cropw_dirty) {
		hdw->res_hor_dirty = !0;           /* must rescale */
		hdw->res_hor_val = min(720, hdw->cropw_val);
	}
	if (hdw->res_ver_dirty && hdw->croph_val < hdw->res_ver_val) {
		hdw->croph_val = hdw->res_ver_val;
		hdw->croph_dirty = !0;
	} else if (hdw->croph_dirty) {
		int nvres = hdw->std_mask_cur & V4L2_STD_525_60 ? 480 : 576;
		hdw->res_ver_dirty = !0;
		hdw->res_ver_val = min(nvres, hdw->croph_val);
	}

	/* If any of the below has changed, then we can't do the update
	   while the pipeline is running.  Pipeline must be paused first
	   and decoder -> encoder connection be made quiescent before we
	   can proceed. */
	disruptive_change =
		(hdw->std_dirty ||
		 hdw->enc_unsafe_stale ||
		 hdw->srate_dirty ||
		 hdw->res_ver_dirty ||
		 hdw->res_hor_dirty ||
		 hdw->cropw_dirty ||
		 hdw->croph_dirty ||
		 hdw->input_dirty ||
		 (hdw->active_stream_type != hdw->desired_stream_type));
	if (disruptive_change && !hdw->state_pipeline_idle) {
		/* Pipeline is not idle; we can't proceed.  Arrange to
		   cause pipeline to stop so that we can try this again
		   later.... */
		hdw->state_pipeline_pause = !0;
		return 0;
	}

	if (hdw->srate_dirty) {
		/* Write new sample rate into control structure since
		 * the master copy is stale.  We must track srate
		 * separate from the mpeg control structure because
		 * other logic also uses this value. */
		struct v4l2_ext_controls cs;
		struct v4l2_ext_control c1;
		memset(&cs,0,sizeof(cs));
		memset(&c1,0,sizeof(c1));
		cs.controls = &c1;
		cs.count = 1;
		c1.id = V4L2_CID_MPEG_AUDIO_SAMPLING_FREQ;
		c1.value = hdw->srate_val;
		cx2341x_ext_ctrls(&hdw->enc_ctl_state, 0, &cs,VIDIOC_S_EXT_CTRLS);
	}

	if (hdw->active_stream_type != hdw->desired_stream_type) {
		/* Handle any side effects of stream config here */
		hdw->active_stream_type = hdw->desired_stream_type;
	}

	if (hdw->hdw_desc->signal_routing_scheme ==
	    PVR2_ROUTING_SCHEME_GOTVIEW) {
		u32 b;
		/* Handle GOTVIEW audio switching */
		pvr2_hdw_gpio_get_out(hdw,&b);
		if (hdw->input_val == PVR2_CVAL_INPUT_RADIO) {
			/* Set GPIO 11 */
			pvr2_hdw_gpio_chg_out(hdw,(1 << 11),~0);
		} else {
			/* Clear GPIO 11 */
			pvr2_hdw_gpio_chg_out(hdw,(1 << 11),0);
		}
	}

	/* Check and update state for all sub-devices. */
	pvr2_subdev_update(hdw);

	hdw->tuner_updated = 0;
	hdw->force_dirty = 0;
	for (idx = 0; idx < hdw->control_cnt; idx++) {
		cptr = hdw->controls + idx;
		if (!cptr->info->clear_dirty) continue;
		cptr->info->clear_dirty(cptr);
	}

	if ((hdw->pathway_state == PVR2_PATHWAY_ANALOG) &&
	    hdw->state_encoder_run) {
		/* If encoder isn't running or it can't be touched, then
		   this will get worked out later when we start the
		   encoder. */
		if (pvr2_encoder_adjust(hdw) < 0) return !0;
	}

	hdw->state_pipeline_config = !0;
	/* Hardware state may have changed in a way to cause the cropping
	   capabilities to have changed.  So mark it stale, which will
	   cause a later re-fetch. */
	trace_stbit("state_pipeline_config",hdw->state_pipeline_config);
	return !0;
}


int pvr2_hdw_commit_ctl(struct pvr2_hdw *hdw)
{
	int fl;
	LOCK_TAKE(hdw->big_lock);
	fl = pvr2_hdw_commit_setup(hdw);
	LOCK_GIVE(hdw->big_lock);
	if (!fl) return 0;
	return pvr2_hdw_wait(hdw,0);
}


static void pvr2_hdw_worker_poll(struct work_struct *work)
{
	int fl = 0;
	struct pvr2_hdw *hdw = container_of(work,struct pvr2_hdw,workpoll);
	LOCK_TAKE(hdw->big_lock); do {
		fl = pvr2_hdw_state_eval(hdw);
	} while (0); LOCK_GIVE(hdw->big_lock);
	if (fl && hdw->state_func) {
		hdw->state_func(hdw->state_data);
	}
}


static int pvr2_hdw_wait(struct pvr2_hdw *hdw,int state)
{
	return wait_event_interruptible(
		hdw->state_wait_data,
		(hdw->state_stale == 0) &&
		(!state || (hdw->master_state != state)));
}


/* Return name for this driver instance */
const char *pvr2_hdw_get_driver_name(struct pvr2_hdw *hdw)
{
	return hdw->name;
}


const char *pvr2_hdw_get_desc(struct pvr2_hdw *hdw)
{
	return hdw->hdw_desc->description;
}


const char *pvr2_hdw_get_type(struct pvr2_hdw *hdw)
{
	return hdw->hdw_desc->shortname;
}


int pvr2_hdw_is_hsm(struct pvr2_hdw *hdw)
{
	int result;
	LOCK_TAKE(hdw->ctl_lock); do {
		hdw->cmd_buffer[0] = FX2CMD_GET_USB_SPEED;
		result = pvr2_send_request(hdw,
					   hdw->cmd_buffer,1,
					   hdw->cmd_buffer,1);
		if (result < 0) break;
		result = (hdw->cmd_buffer[0] != 0);
	} while(0); LOCK_GIVE(hdw->ctl_lock);
	return result;
}


/* Execute poll of tuner status */
void pvr2_hdw_execute_tuner_poll(struct pvr2_hdw *hdw)
{
	LOCK_TAKE(hdw->big_lock); do {
		pvr2_hdw_status_poll(hdw);
	} while (0); LOCK_GIVE(hdw->big_lock);
}


static int pvr2_hdw_check_cropcap(struct pvr2_hdw *hdw)
{
	if (!hdw->cropcap_stale) {
		return 0;
	}
	pvr2_hdw_status_poll(hdw);
	if (hdw->cropcap_stale) {
		return -EIO;
	}
	return 0;
}


/* Return information about cropping capabilities */
int pvr2_hdw_get_cropcap(struct pvr2_hdw *hdw, struct v4l2_cropcap *pp)
{
	int stat = 0;
	LOCK_TAKE(hdw->big_lock);
	stat = pvr2_hdw_check_cropcap(hdw);
	if (!stat) {
		memcpy(pp, &hdw->cropcap_info, sizeof(hdw->cropcap_info));
	}
	LOCK_GIVE(hdw->big_lock);
	return stat;
}


/* Return information about the tuner */
int pvr2_hdw_get_tuner_status(struct pvr2_hdw *hdw,struct v4l2_tuner *vtp)
{
	LOCK_TAKE(hdw->big_lock); do {
		if (hdw->tuner_signal_stale) {
			pvr2_hdw_status_poll(hdw);
		}
		memcpy(vtp,&hdw->tuner_signal_info,sizeof(struct v4l2_tuner));
	} while (0); LOCK_GIVE(hdw->big_lock);
	return 0;
}


/* Get handle to video output stream */
struct pvr2_stream *pvr2_hdw_get_video_stream(struct pvr2_hdw *hp)
{
	return hp->vid_stream;
}


void pvr2_hdw_trigger_module_log(struct pvr2_hdw *hdw)
{
	int nr = pvr2_hdw_get_unit_number(hdw);
	LOCK_TAKE(hdw->big_lock); do {
		printk(KERN_INFO "pvrusb2: =================  START STATUS CARD #%d  =================\n", nr);
		v4l2_device_call_all(&hdw->v4l2_dev, 0, core, log_status);
		pvr2_trace(PVR2_TRACE_INFO,"cx2341x config:");
		cx2341x_log_status(&hdw->enc_ctl_state, "pvrusb2");
		pvr2_hdw_state_log_state(hdw);
		printk(KERN_INFO "pvrusb2: ==================  END STATUS CARD #%d  ==================\n", nr);
	} while (0); LOCK_GIVE(hdw->big_lock);
}


/* Grab EEPROM contents, needed for direct method. */
#define EEPROM_SIZE 8192
#define trace_eeprom(...) pvr2_trace(PVR2_TRACE_EEPROM,__VA_ARGS__)
static u8 *pvr2_full_eeprom_fetch(struct pvr2_hdw *hdw)
{
	struct i2c_msg msg[2];
	u8 *eeprom;
	u8 iadd[2];
	u8 addr;
	u16 eepromSize;
	unsigned int offs;
	int ret;
	int mode16 = 0;
	unsigned pcnt,tcnt;
	eeprom = kmalloc(EEPROM_SIZE,GFP_KERNEL);
	if (!eeprom) {
		pvr2_trace(PVR2_TRACE_ERROR_LEGS,
			   "Failed to allocate memory"
			   " required to read eeprom");
		return NULL;
	}

	trace_eeprom("Value for eeprom addr from controller was 0x%x",
		     hdw->eeprom_addr);
	addr = hdw->eeprom_addr;
	/* Seems that if the high bit is set, then the *real* eeprom
	   address is shifted right now bit position (noticed this in
	   newer PVR USB2 hardware) */
	if (addr & 0x80) addr >>= 1;

	/* FX2 documentation states that a 16bit-addressed eeprom is
	   expected if the I2C address is an odd number (yeah, this is
	   strange but it's what they do) */
	mode16 = (addr & 1);
	eepromSize = (mode16 ? EEPROM_SIZE : 256);
	trace_eeprom("Examining %d byte eeprom at location 0x%x"
		     " using %d bit addressing",eepromSize,addr,
		     mode16 ? 16 : 8);

	msg[0].addr = addr;
	msg[0].flags = 0;
	msg[0].len = mode16 ? 2 : 1;
	msg[0].buf = iadd;
	msg[1].addr = addr;
	msg[1].flags = I2C_M_RD;

	/* We have to do the actual eeprom data fetch ourselves, because
	   (1) we're only fetching part of the eeprom, and (2) if we were
	   getting the whole thing our I2C driver can't grab it in one
	   pass - which is what tveeprom is otherwise going to attempt */
	memset(eeprom,0,EEPROM_SIZE);
	for (tcnt = 0; tcnt < EEPROM_SIZE; tcnt += pcnt) {
		pcnt = 16;
		if (pcnt + tcnt > EEPROM_SIZE) pcnt = EEPROM_SIZE-tcnt;
		offs = tcnt + (eepromSize - EEPROM_SIZE);
		if (mode16) {
			iadd[0] = offs >> 8;
			iadd[1] = offs;
		} else {
			iadd[0] = offs;
		}
		msg[1].len = pcnt;
		msg[1].buf = eeprom+tcnt;
		if ((ret = i2c_transfer(&hdw->i2c_adap,
					msg,ARRAY_SIZE(msg))) != 2) {
			pvr2_trace(PVR2_TRACE_ERROR_LEGS,
				   "eeprom fetch set offs err=%d",ret);
			kfree(eeprom);
			return NULL;
		}
	}
	return eeprom;
}


void pvr2_hdw_cpufw_set_enabled(struct pvr2_hdw *hdw,
				int prom_flag,
				int enable_flag)
{
	int ret;
	u16 address;
	unsigned int pipe;
	LOCK_TAKE(hdw->big_lock); do {
		if ((hdw->fw_buffer == NULL) == !enable_flag) break;

		if (!enable_flag) {
			pvr2_trace(PVR2_TRACE_FIRMWARE,
				   "Cleaning up after CPU firmware fetch");
			kfree(hdw->fw_buffer);
			hdw->fw_buffer = NULL;
			hdw->fw_size = 0;
			if (hdw->fw_cpu_flag) {
				/* Now release the CPU.  It will disconnect
				   and reconnect later. */
				pvr2_hdw_cpureset_assert(hdw,0);
			}
			break;
		}

		hdw->fw_cpu_flag = (prom_flag == 0);
		if (hdw->fw_cpu_flag) {
			pvr2_trace(PVR2_TRACE_FIRMWARE,
				   "Preparing to suck out CPU firmware");
			hdw->fw_size = 0x2000;
			hdw->fw_buffer = kzalloc(hdw->fw_size,GFP_KERNEL);
			if (!hdw->fw_buffer) {
				hdw->fw_size = 0;
				break;
			}

			/* We have to hold the CPU during firmware upload. */
			pvr2_hdw_cpureset_assert(hdw,1);

			/* download the firmware from address 0000-1fff in 2048
			   (=0x800) bytes chunk. */

			pvr2_trace(PVR2_TRACE_FIRMWARE,
				   "Grabbing CPU firmware");
			pipe = usb_rcvctrlpipe(hdw->usb_dev, 0);
			for(address = 0; address < hdw->fw_size;
			    address += 0x800) {
				ret = usb_control_msg(hdw->usb_dev,pipe,
						      0xa0,0xc0,
						      address,0,
						      hdw->fw_buffer+address,
						      0x800,HZ);
				if (ret < 0) break;
			}

			pvr2_trace(PVR2_TRACE_FIRMWARE,
				   "Done grabbing CPU firmware");
		} else {
			pvr2_trace(PVR2_TRACE_FIRMWARE,
				   "Sucking down EEPROM contents");
			hdw->fw_buffer = pvr2_full_eeprom_fetch(hdw);
			if (!hdw->fw_buffer) {
				pvr2_trace(PVR2_TRACE_FIRMWARE,
					   "EEPROM content suck failed.");
				break;
			}
			hdw->fw_size = EEPROM_SIZE;
			pvr2_trace(PVR2_TRACE_FIRMWARE,
				   "Done sucking down EEPROM contents");
		}

	} while (0); LOCK_GIVE(hdw->big_lock);
}


/* Return true if we're in a mode for retrieval CPU firmware */
int pvr2_hdw_cpufw_get_enabled(struct pvr2_hdw *hdw)
{
	return hdw->fw_buffer != NULL;
}


int pvr2_hdw_cpufw_get(struct pvr2_hdw *hdw,unsigned int offs,
		       char *buf,unsigned int cnt)
{
	int ret = -EINVAL;
	LOCK_TAKE(hdw->big_lock); do {
		if (!buf) break;
		if (!cnt) break;

		if (!hdw->fw_buffer) {
			ret = -EIO;
			break;
		}

		if (offs >= hdw->fw_size) {
			pvr2_trace(PVR2_TRACE_FIRMWARE,
				   "Read firmware data offs=%d EOF",
				   offs);
			ret = 0;
			break;
		}

		if (offs + cnt > hdw->fw_size) cnt = hdw->fw_size - offs;

		memcpy(buf,hdw->fw_buffer+offs,cnt);

		pvr2_trace(PVR2_TRACE_FIRMWARE,
			   "Read firmware data offs=%d cnt=%d",
			   offs,cnt);
		ret = cnt;
	} while (0); LOCK_GIVE(hdw->big_lock);

	return ret;
}


int pvr2_hdw_v4l_get_minor_number(struct pvr2_hdw *hdw,
				  enum pvr2_v4l_type index)
{
	switch (index) {
	case pvr2_v4l_type_video: return hdw->v4l_minor_number_video;
	case pvr2_v4l_type_vbi: return hdw->v4l_minor_number_vbi;
	case pvr2_v4l_type_radio: return hdw->v4l_minor_number_radio;
	default: return -1;
	}
}


/* Store a v4l minor device number */
void pvr2_hdw_v4l_store_minor_number(struct pvr2_hdw *hdw,
				     enum pvr2_v4l_type index,int v)
{
	switch (index) {
	case pvr2_v4l_type_video: hdw->v4l_minor_number_video = v;
	case pvr2_v4l_type_vbi: hdw->v4l_minor_number_vbi = v;
	case pvr2_v4l_type_radio: hdw->v4l_minor_number_radio = v;
	default: break;
	}
}


static void pvr2_ctl_write_complete(struct urb *urb)
{
	struct pvr2_hdw *hdw = urb->context;
	hdw->ctl_write_pend_flag = 0;
	if (hdw->ctl_read_pend_flag) return;
	complete(&hdw->ctl_done);
}


static void pvr2_ctl_read_complete(struct urb *urb)
{
	struct pvr2_hdw *hdw = urb->context;
	hdw->ctl_read_pend_flag = 0;
	if (hdw->ctl_write_pend_flag) return;
	complete(&hdw->ctl_done);
}


static void pvr2_ctl_timeout(unsigned long data)
{
	struct pvr2_hdw *hdw = (struct pvr2_hdw *)data;
	if (hdw->ctl_write_pend_flag || hdw->ctl_read_pend_flag) {
		hdw->ctl_timeout_flag = !0;
		if (hdw->ctl_write_pend_flag)
			usb_unlink_urb(hdw->ctl_write_urb);
		if (hdw->ctl_read_pend_flag)
			usb_unlink_urb(hdw->ctl_read_urb);
	}
}


/* Issue a command and get a response from the device.  This extended
   version includes a probe flag (which if set means that device errors
   should not be logged or treated as fatal) and a timeout in jiffies.
   This can be used to non-lethally probe the health of endpoint 1. */
static int pvr2_send_request_ex(struct pvr2_hdw *hdw,
				unsigned int timeout,int probe_fl,
				void *write_data,unsigned int write_len,
				void *read_data,unsigned int read_len)
{
	unsigned int idx;
	int status = 0;
	struct timer_list timer;
	if (!hdw->ctl_lock_held) {
		pvr2_trace(PVR2_TRACE_ERROR_LEGS,
			   "Attempted to execute control transfer"
			   " without lock!!");
		return -EDEADLK;
	}
	if (!hdw->flag_ok && !probe_fl) {
		pvr2_trace(PVR2_TRACE_ERROR_LEGS,
			   "Attempted to execute control transfer"
			   " when device not ok");
		return -EIO;
	}
	if (!(hdw->ctl_read_urb && hdw->ctl_write_urb)) {
		if (!probe_fl) {
			pvr2_trace(PVR2_TRACE_ERROR_LEGS,
				   "Attempted to execute control transfer"
				   " when USB is disconnected");
		}
		return -ENOTTY;
	}

	/* Ensure that we have sane parameters */
	if (!write_data) write_len = 0;
	if (!read_data) read_len = 0;
	if (write_len > PVR2_CTL_BUFFSIZE) {
		pvr2_trace(
			PVR2_TRACE_ERROR_LEGS,
			"Attempted to execute %d byte"
			" control-write transfer (limit=%d)",
			write_len,PVR2_CTL_BUFFSIZE);
		return -EINVAL;
	}
	if (read_len > PVR2_CTL_BUFFSIZE) {
		pvr2_trace(
			PVR2_TRACE_ERROR_LEGS,
			"Attempted to execute %d byte"
			" control-read transfer (limit=%d)",
			write_len,PVR2_CTL_BUFFSIZE);
		return -EINVAL;
	}
	if ((!write_len) && (!read_len)) {
		pvr2_trace(
			PVR2_TRACE_ERROR_LEGS,
			"Attempted to execute null control transfer?");
		return -EINVAL;
	}


	hdw->cmd_debug_state = 1;
	if (write_len) {
		hdw->cmd_debug_code = ((unsigned char *)write_data)[0];
	} else {
		hdw->cmd_debug_code = 0;
	}
	hdw->cmd_debug_write_len = write_len;
	hdw->cmd_debug_read_len = read_len;

	/* Initialize common stuff */
	init_completion(&hdw->ctl_done);
	hdw->ctl_timeout_flag = 0;
	hdw->ctl_write_pend_flag = 0;
	hdw->ctl_read_pend_flag = 0;
	init_timer(&timer);
	timer.expires = jiffies + timeout;
	timer.data = (unsigned long)hdw;
	timer.function = pvr2_ctl_timeout;

	if (write_len) {
		hdw->cmd_debug_state = 2;
		/* Transfer write data to internal bufht (*/
		for (idx = 0;  *  <(C) 20_lens pro++) {
			hdw->ctl_m is fly@pob[idx] = you	((unsigned char *)m is f5 Mi)and/o
 * } *  CoIniti*
 *a(C) 200requestox.comusb_fill_bulk_urb( can redistribuurb,modif   can  *  devther ver *  sndree pipeware F2 of the Lice verPVR2_CTL_WRITE_ENDPOINT)ther version redistribute it ther verm is freether verpvr2_redistribucompletether versio)
 * are Foundation; ei->actualfreegth ThisABILITY or FITNESSpend_flag = !POSE.status =se
 *
ubmitSoftware Foundation; eitGFP_KERNELNTABIif (License< 0re; youthe itrace(n theTRACE_ERROR_LEGSther ver "Failedke Iore deNTY; w-control"h this pr URB Licens=%d",LicensNTABI.  See the
 *  GNU General PPOSE.	goto doneU Gener}

 a coreadfreere; yo can rmd_debug_Lice *
 3OSE.memsetware Found/

#iOUT ANY0x43,/

#inclu
 *  Coublic Licen/

# published by
 *  the Free Software Found/

#i either version 2 of the License
 *rcv  This program is distributed in the hopREADt it will be useful,
 *  bulude <linux/her ver/

#inclut even the impli/

#iranty of
 *  MERCHANTABILITY or Fdia/v4l2R A PARTICULAR PURPOSE.  See the/

#iNU General Public License for more details.
 *
 *  dia/v4l2- have received a copy of the GNU General Public License
 *  along with this program; if not, wr/

#o the Free Software
 *  Foundation, Inc., 59 Temple Placlude "pvrusb2-enton, MA  02111-1307  USA
 /* Start timobox.coadd_ne TV(&ne TV);50000LNow wait >
 *all I/Oke Iranty ofox.coinux/errno.h>
#include 4;
	while ware Foundation;NU General||sb2-util.h"
#inNU Generare; yodefi_for-core.h"ion(& can redi111-ude } the decoder must remain 5L

/* TStopine TV_MAX_delQ    8_sync50000000L

/inux/errno.h>
#include 6;
 License f0L

/a co can redine Toutefine TIME_License f-ETIMEDOUTved a co!probe_flGNU General Public License
 *  along with this prTimed out  the Fr-C) 20"ude <neraA  02111-130SA
 *
 *m is freere; yo/* Validcludresults ofnse as published by
a coware Foundation; ei->License!=e GN&&
his pbefore we are all before we cons-ENOENTder that the encoder has run at least once sSHUTDOWNder that the encoder has run at least once sCONNRESET)re; you/* USB notsystem is reporting some kindl thfailuren, M   on the(C) 200x.comn Dauskard encoder has run at least on, Mkardt@gmx.de> reportneral Public License
 *  along with thiis prc. */
#definee
 * t least,ee Sofftware Foundation;

static, Inc., 59 TneraMA  02111-1307  Ully set to
   ITNESS FOR A PARTICULAR Pram is freeg until weogram; if C) 200enough05 MikME_MSEC_ENCODER-EIO2_hdw *unit_pointers[PVR_NUM] = {[ 0 ... PVR_NUM-1 ] = NULL};
static DEFINE_MUTEX(pvr2_shortx);

static iexpected=%d gotchg;
static inTY; without evece. LITY or FITNESS FOR A PARTICULAR reload;
static int tuner[PV}
 *
 */

#include <lthe minimum interval th.h>
#include <linu successfully h"
#includre we consider that the encoder "hardware initializa since its
   firmware has b
module_param(procrelorement is in important for ca
module_param(procreloo something until we know that the encoder has been run
   at least once. */
#def.h>
#ME_MSEC_ENCODER_OK 250

"hardware initia2_hdw *unit_pointers[PVR_NUM] = {[ 0 ... PVR_NUM-1 ] = NULL};
static DEFINE_M.h>
#vr2_unit_mtx);

static int ctlchg;
static int procreload;
static int tuner[PVR_NUM] = { [0 h"
#include "pvrusb2-hdw<2-hdw.h"
lerance[PVR_NUM] = {.h>
#. PVR_NUM-1] = 0 };
static int video_std[PVR_NUM] = { [0 ... PVR_NUM-1] = 0 };
static int init_pause_msec;

y initialam(ctlchg, int, S_IRUGO|S_IWUSR);
MODULE_PARM_D-hdw.h"
#incluoptimize ctl h"
#include "pvrusb2-hdreload;
static int tuner[PVR Copyright (retrieved05 Mik0msefromIsely <isely@pobox.com>
 *
 *  This progra/

#incl software; youy
 *  it under the/

#i of the GNr modifb2-util.h"
#include he GNU Gener}

2111-:llowed to configure it.  IPOSE succpy of the GN&&rdt@gmx.de> re; yothe ihdw_render_uselessset nes a mreturnnt proc;
}


int the isU Gepublish(struc
#definhdw *hdw
stat th  void * terms of , *  it un08
#TY; without ePIO_DIR 0x90q, int, 0e trace_firmwa/

#inclu
{ine PVR2_define PVR2_GPIO__exset ,HZ*4,0th this pare(...FIRMWTY; without even adio_frFIRMWh>
#include0x90Liceicirmwathe iissue_sinty _cmdOUT 0x900c
#define PVRu32 cmdcodefine ARGS__t;
	 trace_firmwacnt = 1 *);

static conarg origin	LOCK_TAKEset to
   lockude inux/errno_freq,0] =		struct & 0xffu;
	unc pvr(775] = p>> 8)pvr2_wm8775_subdev_unc p> 2) ? 2 :func VR2_CTLunc re; yost p+=5_subdev{
	[PVR2_CLIENT_I1_WM8_update,
	[P16R2_CLIENT_IDv_update, > 1re; you can r2_CLIENT_I2400_subdev_updat24
	[PVR2_CLIENaram(init_pvrusb2no.h>
 &in thense
 *INITre; yo trace_firmwaidxLIEN;

static conscnt,bcnv *)	der ttbuf[50NU Ge775] = pv=PVR2_CLIENLIENdefine TVR2_ = scnprintf(400]+LIEN"pvrusbsizeoPVR2_C)-IENT_ID_SAA"Sendhas FX2ntermand 0x%x",	struct ENT_ID_CX+=PVR2_T_ID>
 *
 *  This prograARRAY_SIZE2_cs2_fx2errno.sc) software; youpvr2_csA] = "cs53l32and/o.id =M8775] = rs[PVR_N= "cx25840",
	[PVR2_CLIENT_ID_SASAA7115] = "saa7115",
	[PVPVR2 \"%s\";
static	pvr2_s775] = "wm8775",
3l32a"PVR_NIENT_ID_DEMOD] =		breakeload;
st[PVR_NUMate,
	[PVRsigned char *module_i2c_addresses[ = {
	[PVR2_CLIENT_ID_TUN " (%u",3400] = pvr2_msp34OD] = \x43",
	[PVR2_CLIT_ID_CX25840] = pvrsigned char *module_i2c_addresses[] = {
	[PVR2_CLIENT_ID_TUNER],
	[PVR2_CLIENT_ID_CX25840] == "\x44",
	[PVR2_C;
staLIENT_ID_SAA7115] = "\x21",
	[PVR2_CLIENT_ID_WM8775] = "\x1b) TIME_= "\x44",
	[PVR2_[PVReral Public License
 *

st,"%.*s"CLIEN, "saa#define P =#define PVR2_GPIO_O2

/2_cx25840_subde,_SCHNULL,0ZILOdule_GIVate_functions[] = {e PVR2_dev *0x9008
#definm is fregisterOUT 0x900c
#define PVR u16 reg, 				 of tv4l2_subdev *module_update_functions[] =  {
	[PVR2_CLIENT_ID_WM8FX2CMD_REGpe tha;  /*nse as pu modul prefix"specn theDECOMPOSE_Late_func of additi1, of t= {
	[PVR2_CLIENT_I5_WM82_mo2_cx25840_subdev6400_sreg
	[PVR2_CLIEN4L2_CID_MPEG_AUDIO_7_WM8RATE
		/* Al
] = "Zilog",
};


/* Define thimize c of additi 8ode",
		.id = V4L2_rolss we'll dynamically construcct based
   on queef void (*pvr2_s/

#i1x module. */
struct pvr2_mpeg_ids {
	const *char *strid;
	inoriginal;
};
static const struct pvr2_mpeg_ids mpeg_ids[] = {
	{
		.std. = "au.h>
#inr",
		.id = V4L2_C3400] = pvr2_msp3400_V4L2_CID_MPEG_AUDIO__updaV4L2_CID_MPEG_AUDIO_3_VIDEO_ASPECT,
	},{
		.s4_VIDEO_ASPECT,
	},{
		.sid = V4L2_CID_MPEG_AUDIO_L2_BITRATE,
	},{
		/* Already using audio_mode elsewhere :- = "Z|	.strid = "mpeg_audio_mode",
		.id = V4L2_CID_MPEG_AUDIO_MODE,4"audphasi =in the AUDIO_ENCODING,
	},{
		.st
	},{
		.strid = "mpeg_audio_mode_extension",
		.iR 0x92_CTL_READ_ENDPOINT   0xUT 0x900c
#define PV *striardt can nera_ok)
   urn;
neral Public License
 *  along with ts prDevice behas D_ENDP_firmoperable TIMEly set tovid_streamne PVR2_CTLporal__setup"video_temporal_l contrtrols a minimu,
	},{
VIDEO_APubli_stbit(",
	},{
[PVR2_C,
	},{
	 = "vide_REAinclu_schedx81

#d	},{
		.strid = "vdEG_VI_retrin		.id = V4L2_CID_MPEG_VIDEsubdev *)] = "24xxx (MCE device)",
	[Performhas a l_filt intet... TIME = V4L *  ns[]al_filteDECOr_modeogram is dist contrid = " = V4nside; yor",
		.id spatial_filt1X_VIDEO_SPATOD] = *  un= V4L2_CID_ilter_type",
		.i} elsee PVR2_CTLPublic License
 *  along with ths program; if ns[]e kno"video_sp;
MODUretZILOG] EO_Binit_pause_msecne PVR2_CTL24xxx (MCE device)"FOeo_chromaWai has %u CHROnes ahardware if nettle;
stat   341X_VIDEO_CHROMlinuxsleep2341X_VIDEO_CHROMWAIT 1	},{
		.strid = "vcpuma_spaasser_OUT 0x900c
#define PVRCX23val *strder tda[1NU G trace_firmwais prid d;
	int idEO_BITRATEtype",
		.strid =VIDEO_SPATIAL_FILTER_MODE,
	},oral_filter",
		%d)",1X_V V4LdaID_WM8val ? 0x01 :ma_mginal/* W) 200#defCPUCSUDIO_CRC,
*/
#def8051.  The lsbl thC(tune modul
ID_MPsAN_FILTset bit; a 1 er",
	encod = t
   ba 0 clears it."specis pe for mondctrlis program is dist,
	},= "video_lu the Fr_msg1X_VIDEO_SPATIis p,0xa0/slan_fie600,0,da,1,HZILTER,
	},{
he GNU GeILTER_TYPE,
	},{
		.strid = "video_chromaIAN_FILTER_TYPE,
	} errorationvalL2_CID_MR2_CTL_READ_ENDPOINT   0x81

#defi0x9008
#defintemporrno.eper_mode",
		.id = V4L2_CID_MPEG_PVR2_FIRMWARubdev_update_func2

/ = {
	{DEEP_methinon query of theMEDIAN_FpowerupOM,
	}
};
#define MPEGDEF_COUNT ARRAY_SIZE(mpeg_ids)


static consPOWER_ONntrol_values_srate[] = {
	[V4down_MPEG_AUDIO_SAMPLING_FREQ_44100]   = "44.1 kHz",
	[V4L2_MPEG_AUDIO_SAMPLIFFntrol_values_srate[] = {deructG_CX2341		.id = V4L2_CID_MPEG_VID] = "24xxx (MCE device)",
	2_CID_MRublishhas 
	[PVR2an_filstrid = "video
	[PVR2_client_idtrid =v4l32a_filtecall_all0

/* T]     = ode",
		[PVR2_CVAL_INPUTe FIRMWARE core,an_filFILTER_R2_CTL_REAcx25840_vbi_hackx81

#de_COUNT A2_mo}vision",  /*xawtv needs this name*/
Un
		.S Broad = 
	[PVR2: nothrid =tta	},{filter",VR2_ sinTTY
typedef void (*pvr2_sMEDIAN_Fhcial_moUDIOAL_INPUT_TV]        = "t,irmwaonoff *strATION,
	},{
		.ublicCOUNT ARRAY_SIZE(mpeg_ids)


stat[PVR2_CVAL = {
	{HCW_DEMO	{
	SETIN |[PVR2_CVAL(1 <<PVR2{
	[PVR2_CVA(",
	[ ? edian)_HSM16)

typedef void (*pvr2_sMEDIAN_Fonair_fe{
	[V4_1X_VTUNER_MODE_LANG2]  = "Lang2",
	[V4L2_TUNER_MODE_LANG1_LANG2] = "Lang1+Lang2",
};


static ,
	[PVR2 1043000 = {
	{ONAIR_DTVO_SAMPLIN :dead",
	[PVR2_STATE_COLD] =    "FFH] = "High",
	[PVR2_CVAL_HSM_FULL] = "digital_path;


static const char *pvr2ID_TUNEang2",
	[V4L2_E] =    "none",
	[PVR2_STATE_DEAD] =    "dead",
	[PVR2_STATE_COLD]STREAMING "cold",
	[PVR2_STATE_WARM] = ar *desc;
}m",
	[PVR2_STATE_ = "video_tempomd_modeswitch	.id = V4L2_CID_MPEG_CX234VR2_STAF_VIDEO cons_MEM;er_toCompe",
VR2_STA/analoAL_IsiPEAKid =has with currentCMD_HCW_.  If,
	},they2111't match, = V4iial_"spec_READ00_srd"},
	{F ?in thePATHWAY_DIGITAL :bytes"},
	{FX2ANALOGtrid = "ES, "reER_OK 2E_REwayM_TYPEec, int, TtrolFX2CM;[V4L2_TUN 0211"speci.strid = SA
 _WRITE set toatial_sc->VR2_STAT,{
		.st
	},mead encasein theCMD_REG_SCHEME_HAUPPAUGEld",E_LANG1]  = "Lang1",
	[V4L2_T = "rd"},
	{FXLIENT_IDr"},
	{FXITE, "write encoder  until weIf movFX2CMD_dword"}_MEM, alsones c		.id 
	[PVR22_CID_MNO]   = R res noL_INPUT_DisNER_MODE_,ontrn it's once. *kke Isgno",
	his becIDEO if/w, "pt1"},
	{FX2CMD_POWER_MODEs,  wriill]   = "itself at th2CMDime_64BYTSB_SPEED, "get 
	[PVR2_CVAL_ICHANTABI[PVRNT_ID_MS2C_READ, "i2c read"},
	{FTATE_ld",oderupposedly weFM suld always hav		.id 
	[V4G_CXwheth	{FXn2_CID_VR2_STA s a {FX2CMD_FW.  B"speor now_MEMw_GETapp		.idto2_CID_workAD_64BYTB_SPEED, "get L] = "Full",
};


stX2CMD_STREAMING_ONNT_ID_MSdefault: NT_ID_MSSA
 _CVAL_HSMuntripV4L2_CI,{
		.str_TUNERREG_READ, "reWM877EAD_Dr2_fx2cmd_desc[] = {
led;


s_hauppaugeTUNER_MODE_LANG2]  = "Lang2",
	[V4L2_/*ndernge been GPIO char
	 *atic[V4Le: "vi d7l thdir_STREAMING_ec. */
#},
	{LED, voidso "hcw ur"}, [PVRhere.atic voi/edian_",
	[Ve PVR2_CTL_REAgpio_chg_dirio_mod2_wmct pvr(str0 *,u481_LUMA_SPATIAL_FILTERvr2_hdw_set_cur_freq(struct pvr2_hdw *,uns0gned lNAIR_DTV_POhdw_set_coudio_modtruct pvr2_hdw *,un00rols0x90typedefdesc[](*powemethod_func)INPUT_TV]        =CX23.stref voidhdw_untrip_unlooid pvr2_hds[_WM8{
	[n theLEDd"},
	{FX2CMD_GET_WM8 dtv power off"},
};


,
} :-(oderoggl
statvr2__OFF, "onair dtv power of	.id = V4L2_CID_MPEG_CX234",
	[V4L2_ trace_firmwa
	{FX2_id;
	id pvr2_hdw_statfp_median_(!);
stat==_BITRATEpoweon)
		.id = V4Loid pvr2_hd =w *hdwconsi V4L_hdw_get_DER_OK 2D_I2C_WRITpowe
	{FX2rid = "hdw);
statLIENT_ID_CS53e_log_stateine PVRfp =te_log_state(_hdw_get_NU GA_SPATIAL_Fpvr2_ conWAIT 100

/fp) (*fp)AMING",
	[Vte);
soder mus/nt prt video poral_ tyrigder unFl);
statPVR2_CVAL_HSM_FULusbporal_	.id = V4L2_CID_MPEG_CX234run{FX2CMD_MEM	int id "strewe're in
	{FX2CMD_FWPON, "pjust ubdev},
	{usualv strea,
	},UNER] =_64BYTly set to},
	{FX2CMD_ONAn"},
	{FX2CMD_STREAMING_OFFG2] = "Lang1+Lang2",
};


static constVR2_CVAmd(st "dead",
	char *contrar *desc;
};

static ctatic int pvr2_hdw_chem",
	[V_STNpvr2_NoGS__)	},{vr2_hSA
 *
 *ut,int probe_fl,
		!n"},
	{FX2CMD_SCMD_REGec, int, Whoops, "hc"},
	{kD_ON_DTV_S, "rstatic i dtv streO] = "SteINVA void pvdw *h getval(s "hcn"},
	o bic inF, "hcw  off"},_LUMme2_hdismval(s,
	},{
	unfortunately di@pobT, "X2CM->hdw;
	ifvendors.  Spvr2_hWRITE,
	},*/
#def = "ra's pvr2_ctr
	{FX2PROMributic inorUT_Dto figure, "s,
	},_DTV_ 0211_64BYTel"},
	{FX2CMD_I2C_WRITE, "i2c write"},
	{FX2CMD_I2C_READ, "i2c read"},
	{FX2CMD_GET_USBint write_len,
				void *read_data,unsigned int read_len);
static int pol_va char *desc;
};

static clotId = hdw->freqProgSlot;
	if


statdemod resetin"},
	{FX2CMD_HCW_DTV_= "Zilog",
n,
				void *read_data,unsignedint read_len);
statc int pvr2_hdw_check_cropcap(strct pvr2_hdw *hdw);


statiR,
	},{
		.id =bdev *)m,int v)
{
	s  "error",
	[PVR2_STATE_READY] = = "md(strream off"}, : "false"));
}

statit(unsigEvaluD_ONDTV_STRE2CMD_tnt preTE_REREADok canr2_hdw *der_run_timeouse {
	evTATE_RE (hdw-
		.id = V4L2_CID_MPEG_VIDEO_B can se {
			if (hdw-ec, int, N
	{FX2CMD_MEMif hdw->frFX2CMl/

#y
	{Fe" : "false"trol_vaEO_BITRATEse {
		ipelinget_lead encoderaceallowm; if 2_hdw *anyL2_TUNet(sogSlot;FX2C} el	retr,int *vp)
{
	*vp =  = {
	{FX2CMD_MEM_WRITE_e the lisinput_eo_l		void *CVAL_INPUTreqPpower on"se {
			if (hdw->NG1_LANd = "stream_tse {
			if (hdw-[PVR2_Curn 0;
}

static LANG2] = "ubli>freqSlotRadio = 0;
			}
		} else {
	en[PVR2_w->freqSlotTelevision == slotId) {
				cptr->hdw;lotTelevision = 0;
			}
		}
	}
	return 0;cptr->hdw;equency hPVR2_CTLid = V4L2_R_ONped ctrl_channel_set(stru0;
}

static irun *cptr,int m,int slotId)
{
	unsignedconfig *cptr,int m,int slotId)
{
	
	[PVR2_Cfreq = 0;
	struct pvr2_hdw *hdng);
statQTABLE_SIZE)) return 0;
	it probe_fl,
				void *write_daCE_STBITS,
		EO_BITRATED_I2C_WRIT,
	},VR2_STATpubliresite"34e,
	*vp)
{
	*vp _SPATIr *name,int val)
{
	pvr2_trace(PVR2_TRAta,unsigned int writrol_v= pvr2_cs2_upload_firmde",2_DEMOEG_CX2341X(struct pvr2_ctrll Public d = "stream_type",r2_ctrl	.id = V4L2_r2_ctrl been changeublica minimuhdw = cptr->hdw;
ot = v;
	}
	return 0;
}

scptr->hdw;ctrl_channel_static int pvr2_ctrl *cptr,int *vp)
{
	struct pvr2_hdw *hdw = cptr->hd->hdw;
	*vp = hdw->freqSelector ? hdw->freqSlotRadi->hdw;: hdw->freqSlotTelevision;
	return 0;
}

static i->hdw;
	2_hdw_seturn 0;
}

static int c_CLIENT_I>hdw->freqProgSlot;
reqer thad > 0->hdw->freqProgSlot;
VIDEOor) {
		hdw->ESETIr,int m,int v)
{
	pvr2_hddefine Tatic int ctrl_freq_defi{
		.stristruct pvr2_ctrl *cptr)
{
	rt = pvurn cptr->hdw->freqDit = pvude <linparanoia - solve ubliotRane TV_t_ex(ranty of,"speciuiet
   before w can eturn stat;
Q    8_LUMA_SPATIAL_Fcptr->hdw->freqPr	if (hdw->|{
	[t the encoslotId;
	} else {
		hdw->freqSlotTelevtr->hdw->->hdw->freqPcptr->hdw;
pcap(cptr->hdw);
	if rogSlot;
	ret) {
		retuct pvr2_ctrl *cptr, int *) {
		return stat;
	}
	*left = pl_m {
		return stat;
	}
	*left = vr2_hdw_set_r_tope m_ex(   = "#defew *hced definsely eo_lifGET_EEPRlprog_seha on"ppet unD_GETmightreq_gedisturbedCMD_POWE

sta[PVR2ptr,iiscw dtv sbware.e",
2C_RCODE, "gEO_B
   beNU Gingtruct pvr2_ctrl *cptr, int3L32A] =ropl_max_get(struct pvr2_ctrl *cptr, int *lXXX] = cur_freq(cptr->hdw,v);
at;
	}
L32A] =/* M

sta
		. *capinclud- *carefDEEPit %icropcc intountroff"},
	cptr, ihdw_] = 	retuST1, v4l2_crO] = "Strutat = pvropcap_info;
	int stat = pvr2_hdw_c_check_cropcap(cptr->hdw);
	if (stat a,unsigned!= 0) {
		return stat;
	}
	*le
	return 0;
}
XXX] = ft)
{
	struct v4cptr->hdw->freqPt ctrl_cropt_max_get(EO_BIw_check_cropcap(cptr->hdw);
	if (stat != 0) {
	"aud = pvrneralwas,
	{d = ] = ne TV_i, in
	int staunng IR rChectrl *cponce mDEEP_o aR 0x v4l2_crfo;
post1"2_GPIefinhe{
	strr->hdw->ir on"cap *cappormwarw_chwnt *as>freqPr a minimalif (statquidr"}cropt_mibp)
{
	dohas beenp *cap =if (stat !=  = &cptr-_check_ounds.height - cptr->hdw->croph_val;
k_cropcvr2_ctrl *cptr, in.exp->frr modif		jiffies +ruct pv(HZ * <mar_MSEC_ENCODER_WAITID_TUNER/ 1 statect pvFREQ    850
	*top = cap->bounds.top;
	reXX] = ;
stal;
	}
ca,
	{ theinue untilurn  %s",
freq_gebeeMINGcap->boundX2CMopcas.width >stat;
	d byP_RESCMD_POWE(cptr-DE, "gft)
{
	struct v4urn 0 v)
{
	pvr2_hdur_CX23NG_ON, "sturn 0;
}

static int cl2_cropcap *cap = &cptr->hdw0;
}

sttruct pvr2_ctrl *cptr)
{
	r->hdw;urn cptr->hdw->freqDi->hdw;
pvr2_ctrl *cptr,int *Rt = pvr2_hreturcap->boundcw dtv s} elbecap *cap =evision == slotId) {ccptr_dis
		.	unsigned fr
		.id = V4L2_CID_MPEG_VIDEO_BITRATEcptr->hdw,v);
	return qSloropcap(ct v4 healthyX2CMD_opcament,t pvs mus = V4L2_	return 0;
}

stuct v4l2_cropcap *cap = &cruct pvr2M, "r2_ctrl uENDPstood)
{
	struct v4 (i.e. "getantING_ON, "o2_hdw )l2_crcropcap(	retub*cptoctrlcap = &cptr->hdw->cro{
		*vp = 0;
	}REG_READ, "read en2C_READ, "w->freqSlotTelo == int ctrl_get_crd > FREQTABLEpw_val;
	}atic int pvr2_sendal)
{
1"},
	{FX2opcap(c	*val =ap *cap; thu
		.stcropcap(cptr->heturn 0;
} aw->croptwell {
		*val = capublic SETIN, "hcw demod rese},
	{FX2CMD_REGl2_cropcslotId)
{
	unsigned fr_max_get(oder w);
	 a funnynt statfo;
	int spvr2_ctrl *c o;
	inARE_CHllr->h{
		return stat;
	}
	*val =.  HowevX2CMD_POWifstatr2_hdw_ encp *cap, only k romw->cfTOP,
dw);
	ifok_ctrl
	}
t *vr->hdw->give *ca2_hdceif (cap- once. */air->bor{FX2CunloIT 5 (ounds.top;
	rru
	*val =briefly first,X2CMleasdw *ce,> cptr->*cptr
	returBLE_SIZE)oral_has freqnair)idth;
	return 0;
}

static int ctlotRadio == we n %s"nrl *c;p->bounds.top(cptr->hdw);
	if (static int ctrl_get_cu32);
sttrl_chann bit n"},r, ifoundare.h>s*/
#cropcappvr2_  p->bounds.widtn = slotIdint stat = pvr2_hdw_check_cropcap(cptr->h
	if (stat != 0) {
		return stat;
	}
	eODE_Mcap->bounds.left;
	return 0;
}

static int ctrl_get_cropcapbt(struct pvr2D},
	{ru"},
	{cropcap(returnptr, int *val"true" : "false"w->cropcap_info;
	int stat = pvr2_hdw_checvr2_ctrl *cptr, int *val)it %s <--(yfreq %s",
		   namON, "one nem; if tructcropcap *cap = &cptr->_cropcapbw(struct pvr2_ctrl *cptr, int *val)
{
	struct v4l2_cropcp *cap = &cptr->hdw->cropcap_infIint pvr2_send_r_check__cropcap(cp	*val = cdw_c
	return 0= cap->bounds.width eturn 0;
}

static int ctrl_get_cropcapbh(struct pvr2_ctrl{FX2CMD_I2C_WRITE, "i2c write"},
	{FX2 = mod> 0)  resetin"},
	{FX2CMD_HCW_der that th ctrl_get_cropcapbt(
	struct v4l2_cropcap *ca int .  OnAiter_mode",
w},
	{ap(cpthdw_check_cropc!= 0) check_cropca

static checcap_info once. */int es a l = 0;
	 perio
   a_val (empirici_hdw
	returight > cpval = 1/4 second)> 0) &->hdwaticovr2_hdw__info;
	int stadw_check;
	if (stat n != ;
	}
	*val = {
		reall_requeseck_cropca = &cptr->Nor;
	if ( int pre mach pvrlog_timecheck_riap =t roGET_EEPRutom voi_hdw_hanap->C(tunemai*caphdw-s(stat != 0) {
		return stat;
	}
	*val = cap->deFortatic intness (urect.left;
	rcropcap(t = pvchec != )et_cro	{FX2CMD_Ovr2_ctrl *cptr, int dw_ch*val)
{
	struct v4nyl2_cropcap check_ccptr->hdw->cl2_cr"},
	{chec = V4L2__info;
	int stat vp)
{
	struct pvr2_hdw *hdw = cptr->hdchec	*vp = hdw->freqSelector ? hdw->freqSlotRadids.left;
	return 0;
}

static intslotId)
{
	unsigned freq	struct vn stat;
	}
	*val = cap->bounds.lpt_vleft)
{
	struccur_freq(cptr->hdw,v);
	return 0ropl_max_get(struct pvr2_ctrlrunnds.top;
	re	return 0cptr->hdrn 0ctrl_freq_geturn 0;
}

statislotId)
{
	unsigned frVIDEO_Aeft)
{
	struct vurn stat;
	}
	*val = cap->defre= 75;
	} else {
		*vp *vp)
{
	*vp = carW_DEMOw->input_val;
	retu;
}

static int ctrl_checublic  int ctrl_get_cropcapbt(
	struct v4lnput(struct pvr2_ctrl max_get(struct r2_ctrl * nt *val)
{
	struct v4l2_OKp = &cptr->hd>cropcap_info;
	int statvr2_ctrl *cptraram(itruct pvr2_ctrl *cptr)
{
	rrunurn cptr->hdw->freqDiTABLpvr2_ctrl *cptr,int * to  "spe
{
	struX2CM>bousc= 0)_val) {
		x2cmd_desc[] = {
	{FXtrl_freq_ 50msec,
 *  it unloVAL_har *strUT 0x900c
#define PV00_sUT 0x900c
#define)trucIZE)) {
		hdw-
	[PVR2_trl_freq_mot = v;
	}
	return 0;
}

sr2_hdw_status_polctrl_channel_r2_hdw_status_polSIZE)) {
		hdw-stalONAIublicqueue_nairizer tnairFREQ;,nfo;
	nairpolfreq_dirty = 0;
}


static int c
	/* Actuefinax_get(struct pvr2_ctrl *cptr, vr2_ctrl *cptr, isigned long fv;
	struct pvr2_hdw *hdw = cptr->hdw;
	if (hdw->tuner_signal_stale) {
		pv	int stat = pvr2_rty(struct pvr2_ctrl *cptr)
{
	rf (stat != 0) {
		return stat;
	}
	*llback */
		*vp = TV_MAX_FREQ;
		return 0;
	}
	if (hdw->tuner_signal_info.capability & V4L2_TUNER_CAP_LOcheck	fv = (fv * 125) / 2;
	} else {
		fv =vr2_ctrlsigned long fv;
	struct pvr2_hdw *hdw = cptr->hdw;
	if (hdw->tuner_signal_sttr,int m,int v)
{
	return pvr2_hdw_ *cptr, int *val)
{
	strcptr,int *vp)
{
	*vp =nt v)
{
	return pvrct pvr2_ctrl *cptr)
{
	c	}
	*lelback */
		*vp = TV_MAX__FREQ;
		return 0;
	}
	if (hdw->tuner_signaldw->freqSlotRadio = 0;
			}
		} else {
	d > FREQTAB(struct pvr2_ctrl *cptr,int *vp)
{
	/d > FREQTABnimum depends on device digitizer type. *ptr->hdw->cropcapcur_freq(cptr->hdw,v);
	return 0;
}

static int ctrl_cropl_min_get(struct pvr2_ctrl *cptr, int *min_get(str
	return 0;
}

static i		*top += cap->bounds.heigreq);
	[PVR2_missrl *U General  cptr->i
	*valtr->hptr->hrn 0;
}

staticr2_hdw_status_poll(hPOSE.  Seedw->enc_stale = 0;	VIDIOCft = cap->bounds.left;
	return 0;
}

static int ctrl_cropl_max_get(struct pint *vp)
{
	unnt *left)
{
	struct v4l2_cropcapfv) {
		/* Safety turn 0;
}
return 0;
}

static ir2_ctrl *cptr,inax_get(struWt %s <--doropcap *capab
	*vpvr2_ctcap->boufreq_max_ge(cptr->rp)
{
	anneT, "dee(cs));
	weOST1, rn st pvrX2CMnt sts		reEEP_R= &c1;
	c_cropcapwar2_ctrl *c} el	*val = (lik= &c1;
	cight;MPEG_ializaIT 5) as oMING_O {
		*val = %s"FX2CM_GET*val)d
}

st	*val = it.(cs));
	_LUMopcapprl *cp;
	}

	}
nfo->vpvr2ap =a
	int star2_hnt *t{
	strucdtv sn"},
 {
		n2CMD_if (stat !isrl *cpt_ex(c_ctl_ != 0revious.  cx2(cs));
	bu	ret)
{
	structte of CLIENT_;
	}
s.width - cptr->hdw->crop pvr2_hdw *hdw = cptr->hrn cptrr2_ctrl *cptr,imax_get(struct pvr2_ctrl *cptr, int *val)
{
	struEG_A4l2_cropcap *cap = &cptr->hdw->cropcap_info;
	r2_ctrl *cptr,int meck_cropcap(cptrvr2_cttic i1.value =ap *ck_cragain(cptr->i{
	sw->croptatic memset(ddw->crr->hdw->li = V detail

stati
	returhopefuhdw_fur_STREstabilizxelaspopcap(cptr->hdw);
	if += cap->bounds.height - c *cap = &cptr->hdw->cropcap_info;
	int stat = pvr2_hdw_check_cropcap(cptr->hdw);
	if al) {
		*left += cap->ft;
	if (cap->bounds.width > cptr->hdw->cropw_val) {
		*cptr->hpcap(cptr->hdw);
	if (stat !=ter.  It's
	   OK to do this here bent ctrl_channel_cx2341x_set(struct pvr2_ctrl *cptr,int m ret;
	str.id = cptr->info->v4l & cptr->hdw->input_a= cx2341x_ext_ctrls!_HSM>input_val;IDIOC_G_EXT_CTRLS);
	if , &cs,
				VIDIOC_G_EXT_CTRLS);
	if (ret) vr2_ctrl *cptr, int *val)
{_signal_info.rangehigh;
	if (!fv) {
		/* Safety faltation. */
	info = (struct truct pvr2_ctrl d > FREQTABLpvr2_ctrl *cptr,int *vp)
{
	struct pvr2_hdw *hdw = tId > 0) {
		(struct pvr2_ctrl *cptr,int *vp)
{
	/tId > 0) {
		nimum depends on device digitizer type. *tId > 0) {
		frint r	if l_ctrl *cptr,iut,int probe_fl,
				void *write_data,unsigned 	t(str	return 0;
}

static imin_get(strp;
	if (cap->bounds.halue) {>freqSlotRadTable[slotId-1];
		if (!freq) return 0;
		pvrin_get(st{FX2CMD_I2C_WRITreq);
	}
	if (hdw->freqSelectoate_pipelinectrl_get_cropcapbt(st= cap->boundfler that th
static int ctrl_cropl_min_geemset(&cs,0,sizeof(cs));
	memset(&c1ctrl_masterstattat = pvr2_hdw_cal = cap->bounds.heighigned long);
statitrls(&cptrtatic int ctrl_streamingheck_input(struct pvr2_ptr->hdw->enc_ctl_state,&qctrl);
	/* Strip out the const so we can adjust a functcs.controls = &c1;
	cs.r2_ctrl *cptr,int *vp)
{
	*vp = cptr->hdw->state_pip int ctrl_get_cropcapbt(stIt's
	nt *val)
{
	struct v4l2_cropls = &c1;
	cs.c_ctrl *cptr,int *vp)
{
	*vp = cptr->hdw->master_state;
	return 0;
}

static int ctrl_hsm_get(struct pvr2_ctrl *cptr,i int ctrl_get_cropcapbt(struwe do
	   th,int slotId)
{
	unsigned freq = 0;
	structrl_std_vaht;
	return 0;
}

static int ctrl_get_M_FAIappan(struct pvr2_ctrl *cpt(struct p_info;F, "hcw rece{
		sat = pvr2_hdw_cheM_FAun   0te of stream;
	if (stat trl rn 0;
tr,bufSiWhy?  Ihis cono idea"},
	{F"},
	{evpvr2_h_POWEry != 0) {
		retu& val);
	resicropc_run, &cs,
	pcap is contrs.width - cptr->hdw->cropl_val;
	
	struc2_ctrl *cptr,400] = "\x40eturn 0;
}

static int ctrl  Either we do
	   this or we siuct pvr2_ctrl *cvr2_ctrl *cptr, int *val)
{tId > 0) {
		ctrl_channel_tId > 0) {
		fpvr2_ctrl *cptr,int *Attemp;
	c1.>hdw->crstruct pvhdw->	}
	t ctrlsion == slotId) {
				hion pointer.  lotTelevision = 0;
			}
		}
	}
	return 0;
ion pointer.  It's
_FAIL;
	if (resu *cptr, int *left)
{
	strueturn 0;
}om detexecutropt_val;2_ctrl *cptr,int *Upimum truct pvr2ap->l whtruct pvr	memsePublkhas bcptrs bancodon o *to
D_MPEputor;cropcap;
	retul = int he*cap =o chirty  relev 1;
;
	pvrint *= !02_hdw _get_csion == slotId) {udw->sl2_std_id  ns;
INPUT_TV]        = "televiatic int pvr2_OD] d_gel *cptdF(str2_mo== hdw->std_mask_cu(cptr-e {
	ll(hdizer type. */
	if (cptrdw = cptrdw);
	*vp = PVc_stale = 0;tic int ctrl_signal_geuct pvr2_ctrl tic int ctr   controls are committed.  Thetrid = "!str2_t->hdw->freqProgSlot;
	return 0;n stat;
	}
	*left = cap->=d ctrl	cur_clear_didw->cropcap_n stat;
	}
	*left = cap->&&
	/* Strip out the const signal;
	return 0;
}

stati	memse	VIDIOCdio_modes_present_geO] = "Scur_clearte);
static ind_ge(*tId) {
				unlocked(struct pvr2_hd.strodereate_

static ct pv
	if (	if 
			dio =vars
		  ns;
	hat != 0) {
		 != 0) {
		rconRLS);ner_signal_inval |al_ine(struct L2_TUNER_SUvr2_ctrl *,(1 << V4L2_TUNion pointer.  TEREO);
	}
	ifcptr->hdw;2_TUNER_SUB_LANG1) {
	 & V4L2_TUNER_SUB_LAG_READ_ONLY2_TUNER_SUB_LANG1) {
	n & V4L2_TUNER_SUtId > 0) {
		 pvr2_hdw Procmsk << V4L2_TUNER_Ml what = pvr2_hr->hdw);i>std_rog_setely R2_CVA != 0) {
		returEG_STREAM_TYPE,l *cpttr->hdw->std_dirty != 0;
}

static voiditrl_stdt v)
{
	stru*cptPVR2_cons
	}
	nera_median_filter */
		*vp =d;
	ret = pvr*cptr,int fw1
{
	pvr2_tFW1_STATE_OKropcapt *val)
{
,
	},{
		nal;
	return 0;*vp = TVPOSE.return 0;
}

st2_cropcalocap k & vahek_crDIAN_FIentir->h);
	}
	 It keeps  coal) {
		tr->l |= (1 << V4L2_natoate_s {
		r(cptr-!= 0) V4L2_TUN2_hdw sint reqProge tv4l iter		hdw.  Each "hdw_ofpvr2_c"mask_cs been glob
	if>encsUGO|d;
	hdw-truct , e.g.= 0;
			}_cropcapw dtv sR_MOin_ge  struct pvr2_c = cptr->d,	.idcap(cptr->his on, etc>hdw- se= ca cptatic int ctrl_stbid;
	hdose PVR2_CR_SUdw->std_dirty struct pvr2_ctstatic ar {
	)
{
	strcorrdirt1x_ext_c = cptr-num_curin quo	[PVR2std_enum_eturn 0;r2_ctrl *cptr)
{
	return cptpt_val;
ap *d_enue ng us	cptrrn 0;
}


stox.com>
 *
  This (i<ENT_ID_CS53REO) {
		v)OINT mask_cur == ; iPVR2_CLIENT_I(*REO) {
		vai]er_wat v4l2_ex, \
	.def.typroph_vat.max_value =*cptr,int nt.max_value = vmax

#define DEFENUM(t_std_str}ter_bot(, \
	.def.tef.type_enum.va fallback */
		*vp = TV= v;
	}
	return 0;
}

s*vp =ctrl_channel_
	hdw- (v & m);
 DEFMASK(msk,_POWER_OFF, "TER,
	},{
		.s,
	[_if ((vmask
 *  it unhdw-msval VR2_CVAL_er thbufe trace_firmwaacnttr->hdw;
	if (v < 0dx,DEMOD] ty_##vname, \tD_CX25840] "tuner",
	[PVR2_CLIENT_ID_CS53,{
		.stl |=es= ctrla",
	[PVR2_CLIEO_BI(AL_HSMidxR2_Cmsk))	if (stat40] = "cx25840",
	[PV2_CLt ctclear_dame
-r2_ctrl *cp"%s%s>bounds (t ctr? ", " : "" be usefint *vp) \
{*vp = cpand/otr->ht ctrID_DEMOD] _hdw_statut ct_POWER_OFF, "n & V4 = ctrurn 0REG_READ, "re_name( *modut pvr2l"},
	{UT_RADI, int *val)
{
	struct v4l;
}


st" strea" ctrl_get_cropcapbh(struct pvdirty_##vVR2_STAtrucm off"},
irty_##vptr,inttrucw->freet_##vname, \
	.is_di_CTL_READ_der , "onair d.id = V4L2_CID_MPEG_CX234whichbounds.top;y = ctrl_cleardirty_##vname


#de##_dirtyCREAT= 0;} \
st0o == slotId840",
	[PVPVR2_uf,ame
clear"truct :
{cpropl)
 <_DTV=%s>>boundt(struct pvoke##_ <ok>l = v <t le>; cptr-t(struct pv341X_FUNCS(cr341Xw)
VCREAunstate,
		edUNCS(croph)
VCREATEdisconnGO|S_NCS(crs_ver)
VCREAw)
Vgned " <srate)

/* CS(croph)
VCREATE_ctrl *cCS(crr2_ctrlw)
VCR)
VCREATE_FUNCS(rescptr->info->v manip off"},
	{*/
static conl *cptr) \
{return cptr-(struct pvr2_ctrl *.stri2C_RE1UNCS(treble)
VCREATE_FUNCS(mute)
VCREAT (subchaS(cropl)
S(cropt)
VCRt;
	}
	*left = cap->udiomodle*/
static const stra function pointer.  I"deaddefinitfigopw)
VCREA*vp = all controls hdw->enc_ctl_state,CS(crreq= V4L2_CID_CONTRAST,
		.desc = "Co	memseCS(crVIDEO*/
stati) {
		hdw2UNCS(treble)
VCREATE_FUNCS(mute)
VCREATnairNCS(cropl)
VC
{cptr->hdint *val)
{
	struct v4l",
		.namevr2_ct:run* Table dint *val)
{
	struct memset(&c1,
		.n ")
VCREA	.v4l_irn 0>")lt_value = 68,
		Dc = "Hue",
		.name = "hu.default_vmemset(&c= V4L2_CID_CONTRAST,
		.dcptr->hdw;
,
		.na)
VCREA = &cp:ode)
VO_VOLUME,
		.desc = "Volum,127),
	},l *cptr, int *val)
{
	stre = "hue"volume",
d = V4L2_CID_id = V4L2_w->crd = V)4L2_CID_HUE,
		.des,65535),
	},{
		.v4l_id = V4L2_alue =UDIO_BALANCE,
		.dvirgiBalaO_VOLUME,
		.desc = "Volumntrast",
		.namelume",
 = "contrastlume),
		DEFINT(0,65535),vr2_ctr = "hue",
		.delume",
vr2_ct768,32767),
	},{
		.vuct pvr2_ctrl ,
		.nameusb_CID_AUD V4L2_CIalue = F(saturation),
		 *cap = &cp,
		.namevr2_ctr:opw)
VCRsc = "Satur3UNCS(treble)
VCREATE_FUNCS(mute)
VCREATL2_TU: ightnessst)
Vget_BRIGHTNESS,
		.dmaodulBrightness,
		.n4:tic const char *mot ctrl_get_ames[] = {
	[PVR2_ V4L_set_##vname(struct ptrl *cptr,iint v) \H_mode",
sAMINrREAT;
	pvr:  TIME_ 0;} \
static iinternal_irty = ctrl_isdirdw;
	if ((vavail_isdibounds.tt pvr2_ctrl *ccptr,int m,val;
	}
	retur= 0,
		DEFREF(crr2_te = ctrl_croic int_isdi[PVR2_CLIENT_ID_SAA7115]DEFINT(-129, 340),
		.get] = "\x1b;atic int op_left",
		.iinternal_id = PVRR2_CID_CROPL,
		.default_value = 0,
		lue = ctrl_\x61\x62\DEFINT(-129, 3440),
		.get_min_[PVRctrl_isdirty_t),
2C_RE5te",
	UT 0x900c
#d,
		.id tat2_TUNEparamEO_BITRATE_temporal_deNT_ID_MSP= ctrl_croptAUDIO_MU0x81
ID_MPEG_VIDEOlear_dirty &_min_pdt,
	}, {
	ptr->h(treble)
VCREATE_FUNCS(mute)
VCREATBy;
	hap(cptS_IWuee Sore
 *s:td_du PVR2ur) rult_trl *VR2_CID_CRp
	}
	*fault_t le PVR2_pdt,
_min_.btern_ropw),
		 = ctrl_cropy@pobvp =_
	if (_MSEC_ENalue = ctrl_g_id opcapdw,
	}, {
		.desctrl *opcapdw,
	}, {
		.d_get,
		.get_def_value = ctr_max_vZILOG] 2C_RE6te",
		.default_valtatic voidirernal_s_activctrl_(treble)
VCREATE_"Capcrop l "irE)) {
	: i_IWUSight ,
	[PVR2_(id >=IENT_ID_CS53	.get_max_cptrs) "dead"  "?DIO_ure capability [id]c = "}eam off"},
	{FX2CMD_O
		*vp = 576;
	}
	Gen
	.typTE_FUNhdw->.denomiinfos,0,sizER_MODE_ not- = "raurn 0;ER_MODE_= !0;2c VAL_INR, "ncluIENT_an indic		hdwl thaREATrl_get_cropture capab arrl_stA PARTlyopcappan,
	} != 0) {
		r_FUNCS(contrast)
VCREATE_FUNCe capabOUT 0x900c
#define PVR2_Gned int = ctrl_clCID_CROPCAPPADce)
VCREATT 0x90]    subdev *s_eep	.default_value = 0,
		DFREF(mute),
		DEFBO		.name i2c= ctrl_ *VAL_IName ct pvr2_ctrl= PVR2_CID_CROPCt_ee
,
	}, {
		.desc = "Capget_def_Assocc Lid = "c-opcap_btruct urn 0;I2Cre capab:\n TIME 0;} \
static i]     = "radDECOstbiropcap_(sd, R2_CVAL_INPUT_enabled*cptsd->grpet_eep	
static voiG_CX23ruct pvr2_hdw *modulbility p) vr2_s_width",
		e = >hdw->inpcropl_max_get,
		.get_def_v +lue =
		.nae tot_def_  %s:", pe crop top margin",
		A_SPATIAL_Fget_value = ctrl_get_cropcapbw,
	}, {
		.d = "\x1b"*cptr,int l_geu)"Capidw);
	apability bounds hOL,
= ctrl= = "crAUDIOpcap_truc(sdw);
	, "stAL_INW,
		.get_value = ctrl_get_cropcapbw,
	}, {
		.d = "\x1b"%s @ %02x\n",re capa->cptrefault_v_INPUT_Taddf (!retpability bounds height",
		.name = "cropcap_bounds_height",
		.internal_id = Ptructure capaCID_CROvalue = ctrl_get_cropnt ctrl_isdirty_##vn_FUNCS(contrast)
VCREAL2_TUNTE_FUNet_cropcappad,
	}, {
		.desolume)
VCREATE_FUNCS(balance)
VCREAty_##vname, \R_SCHVR2_Cdule_nID_CX25840]dule_update_funbigons[] = {>
 *
 *  This  software; yo= "cx25st)
VCREATE_FUNCS(saturatEG_CXdx,S(mute)
NG_ON, "s!VR2_ropt_max_geIENT_ID_DEMODw,
	}, (19,720)et_crD_DEMOD] =EO_BIame

s_hor),
		DufID_WM8'\n';PVR2_pvr2_su	DEFINT(19,720),
	},{
		.desc = "Vertical cefauternal_id = PVR2_CID_H ctrl_ge = "Lt",
		.nanameDEFINT(19,720),
	},{
		.desc = "Vertical cwe'll dynamicaltion",
		.naO] = "SLIENT_r2_fx2cmd_desc[] = {
	{FXL2_TUNlo
#incluINPUT_TV]        = "televi = ct	.na256_FILTER,
	},{
		.lt_v_DEMOD] ty_##vname, \lcapbwuDEFBOOLme = "resolution_hor",
		.internal_id = PVR2_CID_HRES,
		.default_value7115] ="saa20,
		DEFREF(res_hor),
		irty k(e re{
		. \
{ PVR2_CVActrl_V,
		VR2_CLE_ZILOG] lue = 480,
		DEFREF(res_ver),
		DEFINT(17udio Sampling FLING
#defint
   bereque<PVR2_hdw->sO_SAeturn 0;t
   be(D_FRE+PLING_id = PVRNT  	.nault_value =]r2_t"res *cptr,iO_SA++>bounds.hsrate",
		DEFREF(srate),
		D		.getV,
		IO_SAMPet_croLING		.id 0;} \
sult_val2_suw->freqSlotRadio =l whl *cptstruct pvr2's_RESET, "z2_TU, tacur =<< V4L2_valuonsenomis_STRroprc Licf (cap->l *cpt pvr2_ctrl *cptr,int m,int v)
{int tr->hdw->std_dirty != 0;
}

static void ctrl_stdn -EINVAL;
	if (v > hdw->allba
	.def.type_inoundsword"D_MEM V4L2_CID_MPEG_CX2341X_VIDSTBIT4L2_CID_MPtype , \
	.  cx23STARTstrid = "_cs53l32a_subdev_update,
};_channatic void pvr2_ = ctrl_vres_max81

#defisk_cu;
	}
	*v min, \
	.l whrl_cel",pe = pdisposinomin
	cptrEFMASK(msk,tab,int m,int v)
{
	strucpt_val
	ctrl_channeine_req;
nt val)
{
	pvr2_trace(PVR2_TRACE_STBITL

/* Thdw->st= "Mutcptr->hdw->stuponctrl_efine  ns;
	 int timeo_mask_cur == hdw->str->hn thev--;
	DL2_C->freqSlotRadio =  (!v) return 0;
	v--;
	if (freq_min_get,
	},{
	COLesc = "Channel (k for input &qctrl);
	/* Sfreq(hdw,freq);
	}
	if (hdw->freqSelectorr that tis is a dynamically creafreq_min_get,
	},{
	WARMsc = "Channel Prograhich can be tr->hdw-elprog_set,
	ef.type_enum. cptr->info->v4ming Enabled",
		.name  alosc = "Channel Progr,32767),
	},{
		.v4lr that t(!lprog_set,
		.get_valw->std_dirty = 0;
}

sta*vp)
{
	int vad > FREQTABL.desc = "USB Speed",
		RUN_input(struct ate",
		.get_valuEADYent_get(struct = "Mute",
		r2_tsPVR2_CIt,
		.get_value = ctrl_cATEeo_chromaPEG_VIDNT(0,FREhdw *ecify%ct pvightnessirtyCID_AUDIO_MUTE,
		.desc = "Mute",
		.,
		DEFINT(0,65535),
	},{
		spoll(hal Pre2_hdw_com = "tr->",
		.get_value NTABILITY ),
	},{
		.de ctrl_aunnelfreq_get,
		ublic hannel",
		.set_dw->cropcap_nnelfreq_get,ad encoderrigg us nyr->hW) {xt_c ctrny.get_value = 1x is get_crowak
{
	info;
	nnelfr *cptd = "aud
	}
	return.desc = "Channel Program Frequency",
		.name = "freq_table_value",
		.et,
		.get_value = ctrl_channel_get,
		DEFINT(0,FREQTABDONEchannel",ationhannel",
		.smode_extensihannel",
		.sal_info.cCmemsekerne->hd.h>
#ctrl_cx23/k in chetruct pvr2_ctstruct pvr2_ctrl *cptr, _TYPE,
	},{
imum depends on device digitizer type. *
	hdw->std_enal_stale) {
		pvr2_hdw_state DEFREF(vname) \
	.set_value = ctrl_set_##vnaFREQ;
		return 0;
	}
	if (hdw->tuner_signal_infPVR2_CVAL_HSMhdw_sAUDIur_f)(struct pvr2_hdw *,
				*dpfine PVR2_FIRMWARG_AUDIO_MODE_E = "n thent v_DIR,dturel_to_sym,
		.sym_to_val = sign_std_sym_to_val,
		.type = pvr2_ctl_bitmask,
	},{
		.desc = "Video StandaOUT In Use Mask",
		.name = "video_i	}
	}
	return qctrl.fltype = pvr2_ctl_bitmask,
	},{
		.desc = "Video StandaIN In Use Mask",
		.name = "viet_cur_fvalue = ctrl_stdcur_set,

	.c				1X_VIDEO					iltenvalalue = dev *)EO_B~\
strid = "videask,
	},{
		.desc = "Video Standards &ctrlNG_ON, "s frequency has beenstd_00_su,
		& pe = p| (ernal_\
stdes_presePublic License
 *nt veo_chromant v);i_intominlue =xt_c"tun:"tunegned in"ctrl_s"tun		.d"tuner
}

statskfiltectrl_std__LUMA_SPATIAL_F",
		.itd_sym
		.skip_init = !0,
		.get_value = ctrl_stdenumcur_get,
		.st,
		.is_cur_is_di",
};


struct cx2341x module "Video Standards cur_is_l_stdcur_clear_dirty,
		.vstandard_mask_active",
		.intym,
		.sym_to_val = ctrl_std_sym_to_val,
		.type = pvr2_ctl_bitmask,
	},{
		.desc = "Video Stan	.ge Name",
		.name = "video_standard",
		.internal_id = PVR2_CID_STDENUM,
		.skip_init = !0,
		.get_value = ctrl_soutpmsecet,
		.set_value denumcur_set,
		.is_dirty = ctrl_stdenumcur_is_dirty,
		.clear_dirty = ctrl_stdenumcur_clear_dirty,
		.type = pvrunknown>";
}


s#define CTRLDEF_COUNT ARRAY_SIZE(control_defs)


const char *	.ge_config_get,
		.name = "videous__sigrd_mask_available",
		.int	.name = "crtunoboxvtvr2_nfo;
	get_d_  ital_valu;
ux/strinvtp, 0z)",
		.nviceing Fntifier(struct pvr tab

#defin
}

ste:USY){
	spreadntt;
	}
nBroaplace!= 0)X2CMVIDIOC_CROPCAP_dirtusnd adounds_left"nt *top)
{
	strdw);
	name = VIDIOCAT ALL1));
	_dirtnow.  (Of		  rse,hdw)pcapp
		.intseemke Isnty != 0)it ei *totruc},
	{FD_ONower apcapdickerol wheggcropblem...pends ]     = "radio",
	[PVR2_CVAL_INPUT_SV0, get_d, g_get_d_dirty = dentifier(struct pvr2_hdD_MPEG_STPublic License
 *CHIPS, "opcap_bLicense_sig= ctl_stdtatiult_ap(cLAR ult_audio= usb_cap		hdw to radiolowult_hialue = c_DIRtp->tati name*/= !0;uct pv,(hdw->rxsub>";
slRadio cap pvr2tame =f (hdw->rhdw lowlRadio !hdw hig, deer_topfreq_get(sdocapturnfo;
	ineq_sares in pvr2_st 1;
_sigg_set(et contrrEQTAnobody"},
	{sMD_HaSelec0;
}cropcap_valu_value can ry seleg pvr2_hdw_gD_AUDIOMODE,
		.default_va		.set0,
		DEFRext_cM,
	}
};
#define MPEGDEF_COUNT Ae = ctrl_cropl_min_ge;
			hdw->freqDirty = !0;
		}
		if (hdwic intlTelevision != val) {
			hdw->freqValTelevisilue = ctrl_ = "High",
	[PVR2_CVAL_HSM_spaif (h	.id = V4L2_CID_MPEG_CX2341ce digitizer tif ((v >= != Capturedw;
	if ((v >= 0 v_MSP3400]if (hddirtyE enum froget_cH->pixeint beffects -r->hdw)##_dirt = &cnput IDIOC	}
	k & vaRFet conval)
{dw_cheeldirtC(tunp)
{
fpublincy chodeo_asnds.w>cropcark,
	},{wordrty int timeout,inif ((v >= 0) && (v <= FREQTABRADIOhdw->std_dele nS indeor		VIDIOC_G_EXle nDpriate to wha_ctrl *cptr,int if ((v >= 0) && (v <= FREQTABTVropcap(cptvalue = 0,
	 >= 0) && (v <= FREQTABLE_S failure.  If the return ver",
is -ENOENT then no viab
		.internal_id dw)
{
	return hdw->unidw->freqDirty = !0;
		}
	}
}
dw->input_Size
static consue = D_CROPT,
	ned int idx;
	int ret = -EI1X_VIDEO	.id = V4L2_C_CID_MPEG_AUDInv,mdesc =  capture resolution",
		.na = vminnvroph),
		unit_number(strucal_it = -EINVALdard",{
		(count; idx &et = -EINVALNUM,
nv[PVRqValTelevision = val;
	capturenCaptureet_cur legctrl *cs left;VCREATE_a_med instea_get_cro	/* Han-EPE "strIENT_ID_MSPrn 0;
}

dev->dev);
		if (!r= ne loghanneL_HSM file
   couldR2_C	.get_max_value = ctrl_cropl_mluesESET, "nput w->ctunds.t != 0tic int _CROP stat = pvpvr2_c111- {
		*va continue;
		chan indexl whi will be 0 or
   grea
	pvr2_trace(PVR2_TRACEio = vf (!rC(init_pa = cptrror with code=%d",ret);
		r_channel	    ;et_cr upNG***"
		   " Devicemdw->usb_dev->dev);
		if (!D] = "tuner",
	[PVR2_CLI(7115] =mHSM_H3a",
	[PVR2_CLIENT_Ial; return 0;} \tatic int ctrl_,
		.name =hdw->unitefault_	[PVR2_continue;
	valid_bitrols we'll dynamical	   depending on th   on quet(stun
 l_idctrlmsk of eepcifyvr2_ctrl *cptr,int m,inAUDIable t_ctrle",
		.id = V4L2_CID_MPEG_CX2341terv_module_update_functions[] =y = e <linux/errns mpeg_ids[] = {
	{GET_EEPROM_ADDe = 	
		forZilog",
};


/* Define th, "0=optimize c	},{
		.stris",
				   fwnames[idx]);
",
		.name st_fither pename,fwuest_firm
	[PVR2_CLIENT_ID_sc = t
   	} e we'll dynamically construct based
  	for (ol_values_srate[]LTER_TOP_ac}
	*et_cropcappad,
	}, {
		.ded int	.name = "crdbg_FX2CM *FX2CMD_u64ILTE",
	[PVRd intsc = etFl* is n*				htr *st#ifc inCONFIG_VIDEO_ADV_DEBUGhar *pvr2_hdw_est_DIO_CRC,
req.desc = "Chf (v > hdw-okar_dirty125) / >freqle(CAP_SYS_ADMIN5;
	} else== -ENOEing q.firmwa=are()
 ding q.RATEde elet_eepa copb trd;
	q.,
		.iion.
 *
_get_cIt w->hdw);
ndeo_to != 0)if &cpcappan,
	televis, ints published by_freq(struct pvr2_hdw *hdw,unsigned long_INPUThdw *hdw)
, &reqll(hdw);
	firmwaion.
 *
w->hd.cnt)sc->fx2fw_e pvr2_ctlVR2_GPIOc int ctrl_is));
}

st#SPATREO] = "StereSYSult ndifl_info.
  Stuffhdw->Emac
		}
se_masfreqTable[h = &urag2_ctnsmodunt edcares style:
  *** Local Variid  s:
			",
				_DTV: c->fx2_firmwthe -column: 75->fx2_firmwtab-width: 8->fx2_firmwc-basic-offset (ret < 0) {
	Endc->fx2_fi/
