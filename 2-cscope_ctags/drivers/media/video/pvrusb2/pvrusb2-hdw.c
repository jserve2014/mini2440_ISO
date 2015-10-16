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
			hdw->ctl_m is fly@pob[idx] = you	((unsigned char *)stribu5 Mi)and/o
 * }progCoIniti*
 *aam is 0requestox.comusb_fill_bulk_urb( can redistribuurb,modif  are Fprogdevther verprogsndree pipeware F2 of the LicecensPVR2_CTL_WRITE_ENDPOINT)e Licenssio Foundation;te itdisticensstribureeY WARRANpvr2_oundation;completout even sio)U Geogramoundation; ei->actualithogth ThisABILITY or FITNESSpend_flag = !POSE.status =se
 *
ubmitSoftrogramor FITNESS FOtGFP_KERNELNTABIif (ibutnse< 0re;modiistritrace(ndistTRACE_ERROR_LEGSe License"Failedke Iore deNTY; w-control"h thi sof URBributns=%d", Foundived .  SeedistU Ge GNU General Pblic 	goto donete 330,}

 a coreadithoGNU Geare Fomd_debug_ibute*
 3lic memsels.
 *
 *  /

#iOUT ANY0x43,lude ncluce, SCoublic  Founlude pux/fshed byce, SistrF  Thtails.
 *
 *  lude oulde useful,
 * is distributnfor mrcv PURPOsoftgrastribbut WITHOUd i Lice hopREADtT ANwill be useful,ce, Sbulude <linux/ WARRANh>
#inclut eve"pvrusimplilude ranty ofce, SMERCHAived   See thedia/v4l2R A PARTICULAR PURblic emple Plalude ite 330, Bosux/firmwarese >
 *mnot, wtails.r mo2 of "
#incl- have received *
 *p.h"
ideoduite 330, Boscoder.h"
#inc"
#inalong wite Softwar "pvr; if not, w2-hdwoideodev2.h>
#inclu"
#in
 *  You s, Inc., 59 Tenty  Plac"
#in"pvrusb2-entl32aMA  02111-1307  USA
 /* Start timobed byadd_ne TV(&    8);50000LNow wait >Licell I/O if core.h"
ed byude "errno.h>
#include 4;
	while s.
 *
 *  You shite 330, B||

#dutil.h"
#inite 330, GNU Gedefi_for- */
.h"ion(&are FoundFREQain }ideoddecoder must remain 5L

 CopStopi    8_MAX_delQ    8_sync

/* /* T

/he decoder must remain 6;
.h"
#inclu allo*
 *are Found    outef 50msIME#inclinclu-EartiDOUTmd.h"
#i!probe_flrusb2-wm8775.h"
#include "pvrusb2-video-v4l.h"
#iTimed outvideodev-m is "
#inc30, _MIN_FREQ   2500h"
#TY; withoGNU Ge/* Validemairesults ofinclas#include <linu*
 *s.
 *
 *  You shoul-> Dauska!=pvru&&
ncludbefnot,weILITY minhe encodercons-ENOENTintethaANY W enm intehas run at least once sSHUTDOWNits
   firmware has been loaded.  This measuCONNRESET)GNU Gen/* USBvrussystestribreporting some kindl thfailuree TV   o License as d by
n Dauskardare has been loaded.  This m, MODERt@gmx.de>ncoder wm8775.h"
#include "pvrusb2-video-v4l.hcludec. */
#MSECne "pvrd.  Thi,2.h>
#ils.
 *
 *  You sh

Liceic2a.h"
#inclu30, V_MIN_FREQ     55lly set to
   
 *  G FOude "pvrusb2-hdwpvrusb2ICULA until wenclude "pvse as enough0 of kME_MSEC_ENCODER-EIO2_hdw *unit_posely s[PVR_NUMor m{[ 0 ... { [0 ..-1 or mNULL};atic in DEFINE_MUTEX(the ishortx)static in iexpected=%d gotchgnit_pauseinite tith0mseevece.   See the
 *  GR_NUM-1] = -1 };
sreloadDULE_PARM_Dt tuner[PV}100

/h>
#inclu#inclrmwaminimumIsely va   a must remain clude successfuR_NUing. *mum inast onceiits
   firmware has b"hardrograiblicaliza simeasits [0 firmrograeen b
module_param(procew crementtribinvrusortantlude ca
MODULE_PARM_DESC(proco beenthhas rance[PV know
   firmware has been been loa [0 ed.  This meaEFINE_MUT must = 0 };
static _OK 250


module_param(prvideo_std[PVR_NUM] = { [0 ... PVR_NUM-1] = 0 };
static int init_pause_msec;

 musthe itd[PVmtlchg, int, S_nt ctl
MODULE_PARM_Dt
#in(proctl value");
module_para[0 ... PVR [0 "hardware udio.h"

#dhdw<streaningleranceMODULE_PARM_D must] = 0 };
stor m0 init_pausetolevideo_stdMODULE_PARM_DESC(-1] = 0 };
st3 (61.25 MHz), to heram(_pause_msec;

yaram(proam(le_par,to h, S_IRUGO|S_IWUSR);
MODULE_PARM_Derror tordwareoptimizeule_(tolerance,"specify stree,    int, NULL, 0444);
MODU CopyrigpoboretrievedNUM-1]0msefromIsely <i ini it ed by
es a"
#ininclude "pvh>
#incl sails.
 U Gennux/viit units
  ncludelude "pvrue "pdif it running. *msec, "pvrusb2-wm8}

_FREQ:llowedke Iconfigu_part.  Iblict_paunclude "pvru&&unit_pointers[U Ge "pvrhdw_readio_uselessM] =nes a mreturnoleranc;
}



modu"pvrste 3include(strucE_MUTEXdeo_shdwatic  th  voidr2_uermal t ,efault_ra08
#ESC(ctlchg, "PIO_DIR 0x90q for my0e Publi_RUGO|Sh>
#includ{ 50mn theMUTEX(VR2_FIGtrac_exM] =,HZ*4,0-v4l.h"
#are(...FIRMWESC(ctlchg, "0=n adio_frhunk must remainR2_TibuticUGO|Sdefinissue_sire.h_cmd<linR2_T0x900c
#dE_ENDu32 cmdm in_hdw ARGS__t;
	ARE,__VA_ARGScnt = 1 *chg, int, Sconarg origin	LOCK_TAKEM] = { [0 locke,"she decoder_freq,0] =		UT 0xt & 0xffu;
	unc pvr(775or mp>> 8)the iwm8775_subdev__subd> 2) ? 2 :f_sub the ho_subne PVRst p+=D_SAA711{
	MODU2_CLIENT_I1_WM8_update,340016 = pvr2_mspDvsubdev_u > 1o_freq,are Fo= pvr2_msp2400_SAA7115]bdev243400] = pvr2_ARM_DL;
/* .h"

#er mus & "pvrue "pvrINITude <liRE,__VA_ARGSidxvr2_bdev_update_fscnt,bcnv *)	its
 tbuf[50ite 3update,
v=0] = pvr2_vr2_ct v4l2T] =  = scnprintf(400]+vr2_io.h"

sizeo0] = p)-R2_CLIE_SAA"Sendeen FX2ely mand 0x%x",775] = p15",
	[CX+VR2_CLCLIErequency");
module_pARRAY_SIZE2_cs2_fx2coder sc)ed(radio_freq,the icsAor m"cs53l32he GN.id =MT_IDor m = { [0 75] x25840"updat] = pvr2_msp	[PVSAA711tic c"saa= {
 *modulule_ \"%s\"nit_paus	e_paratatic c"ENT_ID",
"wm87"ODULE_addressDEMOD_WM87break,    int,MODULE_Pev_updatVR  it under thMODULE_i2c_ad intses[ PVRmodule_i2c_addressTUN " (%u",3R2_C,
	[Pr2_msp34R2_CL \x43 *module_i2c_CLIENT_ charCLIENT_CLIENT_ID_SAA7115] = "\x21",
	[PVR. PVRIENT_ID_WM8775] = "\xER]*module_i2c_address_ID_CS53L3= "\x44 *module_i2nit_pc_addresses = {
	[PVR\x21 *module_i2c_addressW
static c"\x1b)MartinVR2_IR_SCHEME_29XVR2_m8775.h"
#include "pvrtati,"%.*s"pvr2_,VR2_Cvr2_hdw * =vr2_hdw *,
POINT  O2

/2_ed char_SAA71,_SCHint ,0ZILODULE_GIVate_5_suTNES *ir_scPVR2_FIRMv *truct8E_MUTEXic int tgisterc)(struct pvr2_hdw *,
 u16 reg, 				lude incl_SAA711AA7115] =ubdev_ically construcscheme_names[] = {
	WM8FX2CMD_REGpe Pla;  /*hat the eODULul prefix"spec LiceDECOMblic_Ltic consefinadditi1,lude _scheme_names[] = {5ds[]2_moe list of additv6updatreg3400] = pvr2_4L2_CID_MPEG_AUDIO_7ds[]RATE *  CoAl
ulatiZilog",
};
encodDt v4l2th00L;

m	},{
		.st 8ode *mo	
};

 VAlrerolss we'll dynamicaR_NU	[PVT 0xct based [0 on queefIR 0x9(*e_paralude 1xr",
		RM_DES75] = pNT_ID_peg_idsscheudio_ *der thatiod;
	inpvr2_mal;d = [] = {
	[PVt ION,
	},{
		.strid = .strid =*ir_sche; yo.std.latiau must rrMPEG_AUDIO_MODE,CVR2_CLIENT_ID_CX2500_		.strdy using audio
stat		.id = V4L2_CID_MPE3_VIDEO_ASPECT,
	},d = V44rid = "video_b_frames",
	},{
		.strdy using audioL2_BIT elsb_frames"ere :/

#y ushas auE_CHmode elsewhere :- */
	| V4LrUDIO_".striID_MPEG_VI_CRC,
	},{
		.strdy using audioMODE,4"audphasi = "pvrus audiostatiINGb_frames",
tbitrate_modtrid = "video_gop_clos_extenl,
 MPEG_AUPVR2_he hopstd.t it will   0x)(struct pvr2_hdw *,s",
		*uniare F30, _ok) [0 urn;
wm8775.h"
#include "pvrusb2-video-v4lludeDevbutebeeen ideo_bVA_ARoperabl Marti_NUM] = {vid_streamRE_ENDPOCTLporal__setup"lp wittetempal_lauditre FrdefinIRUGOb_frameid = "v.h"
#_stbit( *moramedule_i2b_framesd = lp w= "vdio_f_schedx81

#d",
		.id = V4L2_CvdEG_VI_M_DESn",
		.id = V4L2_CID_MPid =d;
	int )ulati24xxx (MCEf thice) *moduerformeen a l the00L;tet-1] artiIO_MODe, Sonsta"videoPEG_ArEG_VI "pvrusb2.h"
_TEMPOrid = IO_MOzatioU GeO_CRC,
	},spa(provideo1Xrid = "SPAT840] =*  unid = V4L2_CID_Mr_typsure",
	}DEO_Gecimation".h"
#include "pvrusb2-video-v4l."
#include "pvrs[]le_paL2_CID_spint deretols G] EO_BL;
/* 104.3 MHzdecimation"PATIAL_FILTER_MODE,FOeo_chromaWaiWUSR)%u CHRO
#defmodule_parf nettleEG_AUD   34ilter_typral_Mlude sleep2PEG_CX2341X_VIDEWAIT 1id = "video_spatiacpuma_spaasser_c)(struct pvr2_hdw *,
CX23R);
",
	its
 da[1ite onst char *modludeid .id = t id_CX2"videoX_VIDEO_LU = V4L2er_type",
IAL_FILTERDEO_GOpe",_VIDECID_MO_CRC,%d)",ilte_MODda_ids[]R);
? 0x01 :ma_m2_CID/* We as _MUTCPUCSaudioCRC,
INE_MUT8051.y");e lsb   aC(ule_r",
		
dy ussANCX234M] =bit; a 1 R_TYPEre ha = t [0 ba 0 clearsdefi4L2_Cstriclude "pndctrlclude "pvrusb2.h"
O_MEDMPEG_STo_luideodev_msgilter_type",
Istri,0xa0/slan_fie600,0,da,1,HZ2341Xype",
	"pvrusb2-2341X_TYPo_gop_sizeideo_spatiaTOM,
id = "I		.striDEO_CHROMA_ errorITNESvallready uthe hop "video_bitrate",
		.strefi query of theMPEG_Vder epeCX2341ure",
		.id = V4L2_CID_MP


/*hunk ARe,
	[PVR2_Cc conse th,
		.idDEEP_ reloa,
		.irclude "pMEDstridpowerupOMO_MEd = vr2_hdw usinDEF_COUNT ENT_ID_CS5(.strid =)
es[] = {
	[PVPOWlledNhe Fr_values_srate*ir_schemV4down using audioSAMPLING_FREQ_4412_CL d = 44.1 kHz *modMODE,_MPEG_AUDIO_SAMPLFFG_FREQ_48000]   = "48 kHde] = G"non341",
		.id = V4L2_CID_MPEG_DEO_SPATIAL_FILTER_MODE,
	}r_bottomncludeeen 3400] =lter_l_FILTER_TOP,
	3400] = client_idILTER,v4wm87FILTERmpeg_all0encode0]     =TTOM,
	}dule_i2VAL_INPUTe  ARRAY_E
 */
,DTV]  X2341X_m",
		.id list of vbi_hackG_CX2341_44100] V4L2}vi_MODE,= "axawtv needs Softwname*/
UnDIANS Broafilt3400] =:vrushLTER,ttae",
ILTER_TY= "cxsinTTY
X_VIdd = V4L2_CID_MPEte[] = {hcpatimoaudivideo",
_TVL_INPU]   = t,UGO|SonoffVIDEOATIONb_frames",ux/fi44100]   = "44.1 kHz",
	[V4L2_MPE   = "s-vi,
		.idHCW	[PVRid =SETIN |   = "s-vi(1 <<


/cheme_nameVA_type[ ? edian)_HSM16)

	[V4L2_TUNER_MODE_LANG1]  = "onair_feHz",
	_ilte[PVR2DEO_G_LANG2]   = Lang2,
};



sttatic const cha1t char *vr2_sta1+_state_n = "m int, S*module_ 1043000,
		.idONAIR_DTVIO_SAMPLI :dead *module_iSTATE_COLision   "FFH =   High *module_i2-vidHSM_FULL =   digital_path_STATE_DEADEMPHASder thNT_I{
	[PVRstate_names[] E    "warnonOM,
	R2_STATE_WARDEA=    "ward",
	[PVR2_STATE_WARM] = STREAMING "col
	[PVR2_STATE_WARWAR.. PVr thdesIN 0m	[PVR2_STATE_WAR_MPEG_STD_MPEG_mdEG_VIswitch,
		.id = V4L2_CID_MPCVAL__STATE_Frid = ready_MEM;341XoComVIDEO_STATE_/analovidesiPEAK};

een eo-v4current{
	{ol_vine fO_MEDthey_FRE't match,IO_MOipati4L2_C.id =_BITRd"},
	{F ? "pvruPATHWAY_DIGITAL :bytesncoder X2ANALOGILTER,
	ES, "relled tuE_REwayMO_CHRec for myTe Fr = {
;mes[] = {IN_FRV4L2_Cd = V4L2_2500pe thaUM] = {_spatisc->_STATE_Wrate_mode",
meaR_OK ase "pvru{
	{
		onalEME_HAUPPAUGE
sta2_STATE *pvr2_staE_24XXes[] ==    encoder Xc_addresrSTREAMINITE, "C) 200re has b");
modulIf mov = {
	{dwo encREAD, also
#decG_AUDIr2_fx2cV4L2_CINO0]   = news no2_TUNER_Distic const,EMPOn it'sE_PARM_Dk if sgno *monclubec2CMD_if/w, "pt1 "write e{
	{_SAMPLEO_Gs, (C) ill0]   = itself4);
th_GETime_64BYTSB_SPEED, "get},
	{FX2"s-vide "pvrusbVR2_ddressMS2C.id =, "i2e,
	aencoder E_WAR
sta intupposedly weFM suld alwaysrusbost1"},
	V4der whethAMINnV4L2_C_STATE_ d = CMD_GET_FW.  B4L2_or nowREADw_GETappG_AUDtoV4L2_CworkADCODE, get IR code"},",
	[PFullPVR2_STATED_GET_ar *desc;_ONN, "hcw default: N, "hcw memse] =   "euFILTp = V4L2MEDIAN_FI = {
	readod reserexxx e"vidDrA] = crrno.sc*ir_schled_STAT_hauppaugetatic const char *pvr2_state_names[] /*adiongIDEOen INT ,
	[P
	 *c in"},
e:_TOP d7   adirOFF, "onaireDEFINE_coderLR coR 0xso "hcw utrea VR2_P_SI.nt, Svoi/CVAL__peed"}ecimation"	{FXgpi	},{g_dirMPEG_VLIEN
	},{
OUT 0 *,u481_LUMAID_MPEG_CX2341XT_ID_REAset_curIENT_OUT 0x	},{
		deo_s,uns0it unlATE_COLD_POvr2_hdw_woD_MPEG_Vl(struct work_struc00AL_DR2_T	[V4L2_"onair(*
	[V relodicall)TUNER_MODE_LANG2] CVAL	.idd = V4L_REAWER_ON_unlo 0x9ct works[ds[]chem LiceLED_STREAMIN_GET_GETds[] dtv 
	[V4 offncod = "m,
} :-( intogglATE_DT_ID_OFF, "L] = dw_cmd_usbstr_DWORD, "write encoder dwopeed"},
	{onst char *morite e_	.id =_log_statw_Licefp_m_hdw *(!)EG_AUD==n_filter
	[Von)EG_AUDIO_MODte_log_stat =ine PVlizat_MODr(strget_alled tuD_I2Cpe th
	[Vrite eLTER,
	hdww);
staNONE] = "nS53e_logruct ehdw *,
fp =thdw);
stati(hdw);
staite long);
statNT_IDread{
		.s0VR2_fp) (*fp)desc;peed"}tew);
 interva/olerahelp wi G_VIDE tDULEinteunFlw);
staERROR] =   "errorusbG_VIDE_DWORD, "write encoder dworunatic intMEMdeo_med "
stawe'_parastatic intFWPOCHEMpjval AA711coderusualvSIS,eaO_MEDPVR2_ =CODE, _NUM] = {;
static intONAn);
static intFF, "onair FFE] =    "none",
	[PVR2_STATE_DEADEMPHAX2CMD_Hmd(stigned charder thTEMPOfx2cmd_descbdev_updatearray(tolerid pvr2_chedef pvV_STN2_hdwNoubde)e",
t worT 100

/ut,toleranx.de>PEG_!	void *write_da{
	{
		ad encodeWhoops,vr2_ncoderk,
		atic "},
	 int, S_dw_cmhdw,O =   SteINVAIR 0x9pvfine  getval(svr2_	void o b, to stat2_hdstream(ned meworkismchannb_framesunfortunatinitdi it T, "ic i->hdwid =fvendorsnal.2_hdw e thaO_MEDINE_MUTX2CMDa'sr2_hdwctrrite ePROMWITHO, to or
	{Fto ");

#HEMEO_MEDatic IN_FRCODE, el);
static intvr2_hdw_
	{Ftin"C) 20*vp = 0;
	}
	retuod resetin"},
	{FX2CMDic int pvrUSBeo_mC) 20freePEG_		R 0x90},
	_5 Miruct it uneo_mw *hdlenw);
staay(tolerREQ_4,
	[PVRhdw_check_cropcap(lotIfilt can ENT_ProgSlov *)ifSTATE_Ddemodpoweeti	void *write_dol_vaTVR2_I		.stridtruct pvr2_hdw *hdw = cptr->hdw
	unsigned int slotct pvr2_hdw *hdw);ck_cropcapOUT truct work_stavail(TATE_DE_CID_MPEG_AUDIO341X_VIme,intv)
{
	s  "a_med	[PVR2_STATE_WARstd.Y =   = "t rerral_stream( : "false"))N 0x9slotIt
 *  iE_480,
				   TRE_GET_toleraetor) std.okare  work_stNDPOrun_timeousescheevector)  ( cane",
		.id = V4L2_CID_MPid = "Bare FtId) {		a co canad encodeN_ctrl *cptMEMif(slotId rl *clludeyder e" == slotId_FREQ_4ian_filterurn 0;
ipelin
stal2CMD_I2 intaceadio de "pvwork_stany[] = {et(sREQTABLrl *A_SP	M_DEe,int*vped. **vp   "
		.idnnelprog_gpe thate Pla lisinput_M,
	 pvr2_hds-video",
 <= d_usbstn"urn 0;
}

static>ATE_NONid = poral__turn 0;
}

staticdule_i2urn 0{
				hdw-c NONE] =   ux/fId <=QTABRRE_C This0;
}}
	romaltId) {
ndule_iotId <=QTABTelevalues[PVRs 0) &re; you	cptrdw->fr= hdw->freqSeleruct pvr2_hdEG_A*cptannel_SlotRadio ublincy hn the hoUDIO_MODE,PLINp undtrl_channel_hdwOUT 0l_get(struct iload*Slote,intn chanor ? hdd. *ptr->hdwncy");eq = 0;
	struct pvr2_hdw *hd3400] = pENT_otTelevll(struct work_sthdngw);
staQTABLED_CS5)) rn 0;
}


	int val)
{
	pvr pvr2_hd v)
{
daCE_STBITSPEG_ian_filter
	return 0O_MED_STATE_Wincluresc in34_updm,int v)
{
	o_chror *_TUN changalw *hd2_hdwPublic_DEMODRA= cptr->hdw;
	unC) 2FREQ_IENT_ID32A]up    VA_ARTOM,2	[PVRoder dwo1Xoll(struct wo*cpt75.h"
#in
	}
	return 0_VIDE_ctrl *_AUDIO_MODE,
ctrl **hdw,i,intgeux/fiCIMATIONdeo_= SlotRadio 
o pvrv;	return 0;
}


				SlotRadio *cptr,int m,instruct pvr2_hdw->hdwq = 0;
	stm,int v) return 0;
	if (slot ctrl_freq_dw->freq
{
	stslotId <=Selector ?ptr->hdw->f*vp)
{
dw->fr:= 0;
}

staticdw->freqSe;ct pvr2_ctrl *cplotId =dw->freq pvr2_hdwannel_get(struct modul pvr2_mspw->ftId <= FREQTABLEreqts
   d > 0dw->ftId <= FREQTABLEid = orre; yot pvrethiI0;
	struct pged. *2_hdw *X25840] d");
modulerlIENT_IRMWAEDIAN_FILruct pvr2_ctrl *
statiw *hdr pvrpvanneSlotRadiotId <=Dif (statDESC(inPARMnoia - solve ux/fvp)
t remat_ex(core.h"
,V4L2_Cuieer_bote encodare F_freq( v)
;
t
   beed long);
stat 0) {
		return PrfreqProgSl|chemfirmware hor ? hstruw *hdw = 
	stru	*vp = hdw->f) {
		retuct pvr2_ctrrl_freq_is_slot SlotRadio)LE_SI FREQTABLE_retre; yorn 0k_cropcap(cptr->hd for  *>bounds.le_ctrl *cpretu*leff (stdist> cptr->hdw->cropw_val) {
		*id pvr2_hdw_WORDpe m

st]   = 341Xe (slced ct v4 initM,
	if pvrEEPRl#inc_seha) {
ppet_rant pvmLE_P stagundaturbedGET_EEPRnt v)dule_= 0;
iscw"true"brogr.OM,
lfreaticcode}
	}_max_gite ingheck_cropcap(cptr->hd for 3L32M877roleftax;
sttruct pvr2_ctrl *->bounds.widtlXXX =  worker_poSlotRadio,v);
cropw_v 0) {
	/* Mnt v)eque *capdio_fr-rl *reft chit %ihe sl);
	roWER_tream(sqSlot,TL_REA =  ds.leST1, *stricr"false")ru_CIDIENT_ slot_infovideo_m2_CIDIENT_ID *hdworgot the slot return stat;
	}
(2_CID cptr->hdw!= 0h > cptr->hdw->cropw_val) ct pvr2_ctrl *return fttrl_freq_clev4uct v4l2_cropcapfo;
	inhe sturn stat;ian_fforgot the slot tat;
	}
	*top = cap->b
	if (cap_CLO	if (s30, BwarogS{vr2_ =  t remai fortr->hdw);unng IR rChecap->bou measmt chao aPVR2
	int st(cptpost1"POINTc
#deull">fre{
		reti)) {
caprl *cpoGO|S_hdw)wwidtasId <= FECIMATIOal = cap->quidr"}w->cropibint v)do int, NUat !=  =ropw_max_get = &SlotRorgot tor Fs.heLE_PA-= 0) {
		rethe shQ_48;
 the sl->hdw);
	if (stat .exp2_hdMODULE_		jiffies +(struct(HZ * <marecify installe{
		{
	[PVR2/ 1dw);
etruct_FRE
   b50
	*to
	stcap->b cptr-top= capeturn EG_AUlopw_vc
			{#definue");
mo>hdw %s",
t stagebeeesc;stat = pvric i slos.width >->cropw<linP_REACE_S_SAMtic in = pvr		*top += cap->bannel*cap = &cptr->urer dwair dHEMEfreq(cptr->hdw,v);
	retnt st slotval) {
bounds.w->fl_get(stheck_cropcap(cptr->hdw);
	idw->frt != 0) {
		return stt != 0) != 0;
}

static voidR
	if (statrn 0;stat = pvr>cropcapint beet_cropcap>freqSelector ? hdw-cSlot_disequedw = cptr frlotTelevision = 0;
			}
		}
	}"videotic int ctrl_cptr->hdwtati}

statip->b healthyic int sloload,vr2_servaIO_MODE,cptr,int m,int vcap->b ctrl_get_cropcapbl((struct wMCMD_ 0;
}

u it stoodw *hd+= cap->b (i.e.ode"}antnair dtatiwork_s)nt sthe slot ds.lebq = o;
	iopcapbl(struct p}
	ret = 
{
	stable}},
	{FX2CMD_OCMD_Ilfreq_set(r2_hdw_check_co[PVRinfo;
	in
stacrt(st_FRE
		frpwrn 0;
	}truct pvr2_hdwsendId;
	}
	{FX2CMD_

stati	*R);
=t_cropc; thuDIAN_F
}

static int _freq(cptr a	}
	rettwell += chdw);
 staux/firm[] =vr2_ctr		hdw->frer2_ctrl *cptREG ctrl_gevr2_hdw *hdw = cptr frurn stat; intetat;
 a funny>hdw);
(cptr->hdwropcap_info; cptr->ARE_CHllt v4> cptr->hdw->cropw_valdw);
.  HowevD_GET_EEPif v)
stat !=_STR_cropc, only k romrl_gfTOP,
stat;
	}ok0;
}
pw_void r->hdw);giverl *tat ce = cstatE_PARM_DEairt = rvr2_h_staIT 5 ( pvr2_hdw_cherudw);
	ifbrieflyIRUGst,vr2_ceaseo_sce,>= 0) {
q = 0(struct	freq = h_VIDEeen ENT_] = )nds. *cptr,int m,int v)
{
	podule_vp)
{
	st=dulen	retnp->bo;at = pvr2_hdwtic int ctrl_cropw_maxap_info;
	inp = &u32w);
scptr,int  "vi 	voir->hfor FIR_WA>sINE_rl_get_;
stat at = pvr2_undslotTor ? h->hdw);
	if (stat != rn 0;
}

static int _cropw_max_get(strucptr->hdw->cropw_vaeonstMstat = pvr2_) {
t != 0) {
		return stat;
	}
	*cap = &c slotbt;
	}
	*top =Dcoderruncoderhe slot rn 0;
ounds.widtval"tru,int *vp)
{
	}
	retropcap(cptr->hdw);
	if (stat != hetic int ctrl_croph_ v4l2_)strus <--(yABLE_retur		  2_TU
	}
	*ne nede "pvl(strrl_get_cropcapbl(strucget_cropcw;
	}
	*top = cap->bounds.top;
tId;
	} += cap->b ctrl_ge_cropcapbl(struct pptr->hdw->croI stat = pvr2__rheck_cr;
}

static;
	return*hdwturn 0;
}
t stat = pvr2_unds.h_freq(cptr->hdw,v);
	rett ctrl_get_cropc_OUT 0xty != 0;
}
 0;
	}
	return 0;
}

static int ctrl_c =ODUL>if (->freqTable[slotId-1] = vits
   firmint ctrl_get_cropcap pvr2_ctrl *cptr, it_cropap(cp.  OnAi2341OTTOM,
wcodertatic w_check_cropcap
	if (eck_cropcap(*read_datahecropcap(cE_PARM_DEp(cp#defieturable peri{ [0 aQ_48 (empiriciat !(structLE_PA sta	retur1/4 second)cropc&uct pcropo(stat !=cap(cptr->hdw);_check_ctop = cap->bn_getr->hdw);
	if += capo",
publisot the sloapbl(strucNortop = c(tolerae machr2_hw);
== seck_crri) {
t roget(struutom pvrat !=hantat N_FILTmail *cct vsf (stat != 0) {
		return stat;
	}
	return 0->deFmpt ropcapdness (urectrect.top;he slot 
	if ( {
	_get)l_get_robe_fl,
		if (cap->bounds.wid*hdw)(struct pvr2_ctrl ny ctrl_get_ceck_cro
	struct v4lnt st *cptr,imumO_MODE,cap(cptr->hdw);
	 ctrl_freq_clear_dirty(struct pvr2_ctr;
	})
{
	cptr->hdw->freqDirty = 0;
}

static intefrect.top;
	return 0;
}

static  *cptr, int *val)
{
	seqpvr2_ctrlic int ctrl_vres_max_getp->defrept_v) {
uct pvr2_c0;
}

static int ctrl_curn 0;
}
	return stat;
	}
	*top = cap-runvr2_hdw_checurn 0;
}
SlotRadivp)
{
	int stag_freq(cptr->hdw,v *cptr, int *val)
{
	sid = "v
	} else {
		t at != int ctrl_vres_max_get(sfre= 75;
	int stat = 
{
	m,int v)
{
	stcar_valuew);
f ((vp_inforn 0(stat != 0) {
		retur;
	}ux/firtic int ctrl_get_cropcap pvr2_ctrl *f ((;
	}
	*top = cap->rn stat;
	}
	*t= 0;
}

s (cptr->hct pvr2_ctrl *cpOKcapbl(struct tr->hdw->cropcap_info;
	!= 0;
}

stati2A] = heck_cropcap(cptr->hdw);
	irunt != 0) {
		return st
		fropcap_info;
	int stake I 4L2_ct pvr2_ic i {
	sc	if ic ire; yo_OFF, "onair dtv pvr2
	int sta 5 "spc2-utillt_ralo-vidasis",
	)(struct pvr2_hdw *,pdat)(struct pvr2_hdw)T 0x = hdwt = pvr2r2_fx2cm
	int stamdirty(struct pvr2_ctrl *cpddr(struct us_pol*cptr,int m,i_signal_info.rangq = hdwt = pvr2p(cpTATEux/fiqueue_] = izts
 ] = _FRE;,p(cptr] = poTICUqcur_typ->pix0x90ard");
modul
e",
	ctut v4n stat;
	}
	*top = cap->bounds.->hdw);
	if (statr->hdw;2-vidf(strreq_clear_dirty(struct pvr2_ctrfreqPrqProgSlule_p_r->h2C_Wtaleback *pvr->hdw);
	if (starty;
	}
	*top = cap->boundw);
	i	if (stat != 0) {
		return stat;
	}*llbackt_paropcapbwemain q
	if  {
		retur(strueq_min_get(struct pvr2_cap(c.capability &_MODE,tatic CAP_LOeck_c	fv = (fv * 125) / *
 *int stat = ack != 0;
}
00;
	}
	*vp = fv;
	return 0;
}

static int ctrl_freq_min_get(struct pvr2_ctr 0;
	struct pged. *rn 0;
}2_hdw *hdet_cropcapdh(struct pvr2tatic void ctrl_f
{
	s else {
		fv = fv *k_cropcap(cptr->hdw);
	cw_val) stale) {
		pvr2_hdw_stattus_poll(hdw);
	}
	fv = hdw->tuner_signal_in0;
}

static int	struct pvr2_hdw *hdw = ptr->hdw->c;
	}
	*top = cap->bounds void ctrl_f/ptr->hdw->cRUGO|SdeNU Gr onTER_MOD VR2_Sturn 0ypRM_D	struct v4l2_crop*vp = 17;
	}
	return 0;
}

stati_set_input(struct pvr2	returinstat;
	}
	*top = cap->bounds.top;
 c1;
	memsecontrols cs;
	struct v4	o;
	in+t stat = pvr2_>hdwreq 0;
dule_imissp->bte 330, Bo= 0) {
il_vrestructlotRadnnel_get(struct_signal_info.rangl(hnternal.h"t v4encctrl *p->pi	VIDIOC{
		* = cap->defrect.top;
	return 0;
}

static int ctontrol n stat;
	}
	*to void ctrl_fu2_GPl) {
t(struct pvr2_ctret_cropfvback *00L
afety freq(cptr-ntrols cs;
	struct v4= 0;
}

static vn stat;
	}
Ww);
	if d_ctrl *rl *cabt ctr!= 0;
stat = ps_polln static intrint v)nt m
	ifdee(cs) {
	weOnfo;
trl *fv *ic i>hdw)s{
		 chaRpbl(1;
	c
	structwa= 0;
}

sttrl *r->hdw-(likl_id;
	c1LE_P;usingprocre	str) theoonair idth;
	retur%s"rl *ct pv(strud
				h1x_ext_cit. &c1;
	ct *l*cap =p->bouopw_v	fv nfo->zeof() {
atr->hdw);statint tt pvr2_ctrue"	voidack *n_GET_ropw_max_gi->v4lcp}

stc_redi_get(reviou > 0cx2 &c1;
	cbucap->ct pvr2_ctteefinpvr2_msopw_vptr->hdwropl_val;
	}
	retfv * 6250(struct pvr2_ct != 0) = 0;
}

static rn stat;
	}
	*top = cap->bounds.top;
struct pvr2_ing nfo;
	int stat = pvr2	struct v4l2_cropcap cptr= 0;
}

static voimn 0;
}

static i!= 0;
ropca1._4800;
	if ( theagaintic int pvrsap->bount pvrx/stri(dt v4l2r->hdw);li elsusb2-dety & V4
		fv =hopef int_fuhed(ststow;
	zxelasp

static int ctrl_crop
	cs.count = 1;
	c1->cropstate,
					0, &cs,
					VIDIOC_S_Et stat = pvr2_hdw_check_cropcap(cptr->hdwstat;
	}
_get(strl) {
	
	cs.cout.topruct pvrpcap(cptr->hdw stat = ct v4l2_ccap_i_ctl_st			0, &
static int ctrl_cropw_max_geterine t's

		rOKke Id4x-vis P_SIZboad,*cptr,int m,icxget(xint slotItr->hdw->enc_unsafe_stamw->ffv;
	rAUDIO_ ret;
	 is te4l &= 0) {
		ret
statia= fac, so ext	}
	if!  "e

static inretur_G_EXT_CTRLSat;
	}
, &crogS		) returis or we significanval)) {
		fv = fv * 625ty_input(sgnal_info.ranrurn higat !trol! pvr2_hdw *hdw = cfalale onEFINE	urn s= capderlynderlying storagptr->hdw->cr != 0;
}

static void ctrl_freq_clear_dirty(struct ) &&cropc) {
;
	cptr->hdw->enc_unsafe_stale = 0;
}!(info->set_vt ctrl_cx2341x_get(struct pvr2_ctrl *cptr!(info->set_vf,
	[  *capl0;
}

static ame,int val)
{
	pvrf (!freq) return= cptr->hdw;	inputontrols cs;
	struct v4 c1;
	memse.  ctrip out the conh4800) {r,int *vp)
{T
		.[or ? h-1]oll(cptr->.id w->freqTable &cpc1;
	mems 0;
	}
	return 0.id = cv = hdw->tundw->freqDir_idsis plinptr->trl_get_cropcapdwet;
	*vp = cflts
   firmstruct v4l2_ext_control c1;
	0;
	rey co0,7115]f &c1;
	c!0;
	re&c1r->hdmaodul v)
);
	if (stat != _cx25840) {
		*vp>hdw-0;
	}
	*vpnt slotI
	if(				0_cropcapdt(struporal_inggot t
statval = cap->de	struct v4RLS)redicptr-,&qr->h;
	c00L
#rip00mse>hdwEMPHASIoast oaWAREt_ex(cap =ctcs. the Frp *cid;
	c1s.= 0;
}

static void ctrl_fwed_mas	struct v4cptr-tr,itic int ctrl_get_cropcapdwwe knoty_input(struct pvr2_ctrontroptr,int m,int>std
	struct pvr2_hdw *hdw = cptr->hdw;
	v;
	if mask_atop;
	return 0;
}

static int cthsmstat;
	}
	*top = cap->boundsitic int ctrl_get_cropcapdw(swe donow tthuct pvr2_hdw *hdw = cptrptr->_SIZE)) returct pvd_vah.top;
	return 0;
}

static int ctrl_gM_FAIappan;
	}
	*top = cap->bounval = capcap(cppvr2_ctrfx2-) {
s
	int stat = pvr2d inunte",ll be poral_top = cap->btd_m;
	}
	tr,bufSiWhy?  Iynamcono ideancoder ncoderezeof(chropt_ryat != 0) {
		re&otId;nt ctsct v4lsiontly comptr->hval(sttrnext time
		   controls aEQ_48;tId _READ 0;
}

staticR2_CLIE2_IR0);
	if (stat != 0) {
		retu  E2-commstatic int cise thwe sierlying storage way. */
		ret = cx2341x_ext!(info->set_v*cptr,int m,itrl_streamingeropcap_info;
	int staAtMPEG
	c11.adjust aal = cap-ct v4sm_greatedeqSelector ? hdw->freqhqSelR_NUM] .ionshdw->freqSlotTelevision;
	return 0;
}


l_stdcur_set(swe kn intLcontrol
	su>bounds.top;
	,int v)
{
	i_freq(cptrorl_ctexecut->crol2_s 0;
}

static voidUpUGO|Snderlying out l whnderlyingR2_CVA.h"
k int,tr->s ba0;
}on o *to
y usiputor;et_cropnt ctrlif (rnt heal) {
o chnfo.cnew ev 1;
;= &cp*/
	i= !0work_suct pveqSelector ? hdw-u;
	v4tridtd_idd = ;
TUNER_MODE_LANG2]  = w->frtruct pvr2_hdwR2_Cd_geing.  dFG_REV4L2=cptr->h{
	rmask_cutic inId) {			Vd2_ctrl *cptr2_CTuct ptrruct pvr2stat;

{
	stPVS);
	if (retl_get(struct ppvr2_cgeerlying storagropcapdt(stersitrl *cptogracommittedDEO_LUILTER,
	!_dirttuct pvr2_ctrl *cptr, (hdw);
	}
dw->cropw_val) {
		*x_get=l *cpt	work,
		._dit v4l2_cropc
	return 0;
}

static int&&turn 0;
}

static int ctrlpvr2_ *cptr,int m,int v)
{R2_CVA) retur_MPEG_VIs_p>frent_g "false"dio_modesout(unct pvr2__std(ctrldw->freq fv;ckew->frstruct work	.id inte2_stk_cropcap(s pv= 0;
} 0;
p)
{rl *cvaral =urn cp	  fi
	if (cap-at != 0) {
	consigni_signal_info.rato|info.eG_READ_O) {
		/* SU!= 0;
}

s,AL_HS!fv) {
		l_stdcur_set(sTEREOl_hsm_getSlotRadio << V4L2_TUB_STATE>set_ (!fv) {
		/*  << V4
	{FX2_ONLYLANG1);
	}
	ifL2_TUNERn_MODE_LANG1);
	}!(info->set_vre committProcmskEO);
	}
	if ROM_turn;
	if (stat&cptr->hipvr2_ pvr2_ cptr2CMD_Hat != 0) {
		retead"r *deO_CHROing.  ->hdw;
	v4ld_info.c
	ifm,int v)
{
	R 0x ctr
{
	anged. */trunterVR2_CVonshsm_g,
	},2_hdw *ILTER_) {
		pvr2_o;
	re
	if (sq = 0;
	stfw1
	} else {FW1ATE_WAROK->hdw-2341x_ext_b_frames" int subchan;
	pvr2_hdwblic ntrols cs;
	str;
	struclomsetk r,unhe the[] = {Ienttr, hl_hsm_g It keepscptrtion poitrucl |= EREO);
	}
	nal *cpt= "au	rtic in
	if (fv) {
		work_supda  <= FREer *st fo;
= pvr.  Ehdw)_ava_ot pvr2_"ctrl *nt, NULglob= 0;dw->sa */o;
	ct vubchan, e.g.tTelevisi.value = "true"OM_Ac1;
	 SIS,
	},{
		.} eltr->hdd,_AUDstatic int 
	ifn, etcadjus setic aticil_get(struct pvb	.id hdosecimatio;
	}r2_hdw *hdw = ruct pvr2_ctrlsubchanarset_	return 0orrinfonly storstatic vnum_wori
		.ocptr->dw *et.mi_freq(cptruct pvr2_hdw *hdwr->hdwcptm) | (v
set(
	.dee ad")sqSlotnnel_get(
shed by
requen");
mo(i< pvr2_hdw *TUNEset_vv)itratctrl *crap->; iule_i2c_addre(*E(tab), \
ai]er_w}
	r *cpex, \
	.(tuntypreturn t.1,0,0;
	}
	q = 0;
	stne DEFMASK(msk vmaxX2341X_nee_msENUM(t
{
	rstr}ail bot(vr2_ctl_boo_bool
e	.def.val.flstale) {
		pvr2_hdwty(struct pvr2_ctrl *cp
{
	s*cptr,int m,i(struc (v & m);
e_msMASK(msk,_EEPROM;
statL2_CID_MPEIAN_vr2_e we'(vctrlned long fct vmsratoX2CMD_HCWts
  bufWARE,__VA_ARGSnst  ctrl_freq_minv < 0dx,[PVR2_Cty_##vdio = \t "none",
	["ule_p *module_i2c_addressdw *MEDIAN_Fnt ces=eateda *module_i2c_aint (=   "eidx2CMDmsk))p = cap->S53L32ned char *modul= pvreatmodes_pame
-= 0;
}

sta"%s%s {
		*v (reate? ", int *"de "pvru void ct \
{w = cptrhe GNtructreate,
	[PVR2_Cignal_inforeatctrl_get_##vn {
		vstattrannel},
	{FX2CMD_O__TUN(AA7115vr2_hd	*vp = UT_RADI. */
	info =  pvr2_ctrl *bility &"l);
	r"	return stat;
	}
	*val = cap-info.\
st_STATE_ubchlotRadio
>hdw->vn= 0;
	subchuct pvet\
static in
	.is_di,
		.id = intetatic int DWORD, "write encoder dwowhich= pvr2_hdw_.cap*cptr,
		.->hdw->vn_TUN
X2341##_info.CREATptr-} \
st0cap->or ? hhar *modul


/*uf,ce)
ATE_Fcropct :
{c

#dl)
 <v;
	=%s> {
		*t *vp)
{
	*okATE_ <ok>hdw-v <d.  >;atic vt *vp)
{
	*PEG_CFUNCS(cret(sw)
V(basunask_av
		ed(audiomophVCREATETEdisconn */
saudioms_verVCREATE
VCRit un" <   = )encod
VCREATE_FUNCS(re>std_masudiomopcap(c
VCREAols which S(audireptr)
{
 privat man}

scap = &{ENSIOd_data,uptr->hdwe##_
#define Dr-;
	}
	*top = cap->b	.id lfreq1(auditrebletic const structmuion UNCS(r (subchaVCREATl)
VCREATtalue urn 0;
}

static intD_MPmodlNER__AUDIO_EMPHASIS,ruct pvct pvr2_hdw *hdgned ct v4itfigop
VCREATE
{
	st mintr->hdw;
r->hdw->std_mask_avmanipueqid = V4L2_CCONTRASfreqctl_sf.ty"CoR2_CVAudiomid = = V4L2_Cback */
	2me = "brightness",
		.default_value = 1] = )
VCREATlalue(crotruct 
static void ctrl_clearMPEG_A_TUN!= 0;
:run* r,int turation),
		DEFINT(02_CVAL_HSM),
	}, "alue = 	.v4l_i;
	}>")l) | (2_ctl68	.v4D = V4HuOM,
	}
_TUNstd_autl_bff"}_v2_CVAL_HSFINT(0,127),
	},{
		.v4l_nt ctrl_fre-128,12alue = apbl(s:odvaluO_VOLUMfreq4l_id = V4Volum,127)O_MED
		ret = cx2341x_ext_ctrl),
	},{e"vDEFROM,
	.id = V4L2_CUDIO_MODE, v4l2DIO_M)Already HUlue = 620,65535olume).is_dult_vDIO_MODE,	DEFREaudioBALANClue = 6virgiBalaault_value = 62000,
		DEFRt pvst(-128,127)= V4L2__set_nt pvst= V4olum	_msecT(0lance",
!= 0;
}		.v4l_iue = 62= V4L2_!= 0;
768,3276volume)efaulterlying storag-128,127) *  L2_CAUDd = V4L2	DEFREFF(satudian_f		.nacropcapbl(s),
	},{
		.v4l_r:ntrast",d = V4S "Tr3me = "brightness",
		.default_value = 1) {
	: strutrl s),
	
staBRIGH *  Gue = 6ma,
		Batic trl -128,14:  "ready",
	[PVRl(hdreturn stamr *ir_scheme_namdw *hhdw_alance)val = captd_mask_ava} else \H	int stasdescrCS(rtic in: Martin
VCREATE_t pvr2sely <is_nfo.capr,intisdir_freq_min(vavaifault_= pvr2_hty != 0;
}

st = 0;
	strup_info;x

#def= 0	.name REFnipula
 *
  ctrl_cxv);
	rault_dule_i2c_addresses = {
	me = "b-129, 340		.na.getulation)";cropcapdtop_ns =MPEG_AUR2_CID_CROvr2_PVR2CMD27),ROPLue = 62.v4l_id	DEFREFl_croDEFREFr,int\x61\x62\f_value = ctrl__get_cropcal c1;VR2_.default_vty_t)int stE5t = 0,)(struct pvrPEG_AUDItat {
		/PARM_ int ctrl__MPEG_VIDE wri "hcw Pget_max_vaptEG_VIDEUEG_CX 0;
			}
		}
odes_prfo.c&l c1;pdFILTERset_REF(sa "brightness",
		.default_value = 1ByER_MO
stat
stau2.h>
e "pvs:w *huVR2_Fur) r4l_itd_maX2CMDe = "ppw_val.v4l_id.  VR2_FI = "Cl c1;.bly <_a fu,
		.nt,
		.get_delevi{
	s_= 0;
} 0 };
st	DEFREFreturnrginptr->dw"Capture 4l_id td_maCapture crop heightnput	DEFREF(cdefnternal_ictstructvID_MPEGlfreq6et_minop",
		.inter
	if (v < 0rCID_CRs_activr,int "brightness",
		"Capls arl "irllback : istatistrucvr2_fx2cm(id >=_ctrl *cptr,FREF(crax_ns;
	)igned c  "?udio

#dgelow;
	if [id] = V4}qSlotRadioprobe_fl,
	
		pvr2_h576_hsm_gGen
	ool
st strct v4.denomi priult < 0ic constvrus,
	},raannel_ic constl_st;2c -videoRstruclur2_msann = ic= pvr   aaCS(r
{
	int resterator",
 ar retue "pvrly*cap =atruc}at != 0) {
	CS(audio = "Bastic const struator",
c)(struct pvr2_hdw *,
2_Ghdw;
	une)
VCREATme = "crCAPPADcvalue = 1(strucL_INPd;
	int s_eepop",
		.internal_id = PD_min_lt_va	.name BO28,127),i2c
		.des *-video27), cap->defrec",
		name = "crCt_ee_cur = height",
= V4Lapnal_id =Associrmwid = c->hdw->bE_FUNCannel_I2Crator",
:\nfilterternal_id = PVL_INPUT_"radEG_A	cpt->hdw->(sd,(struct2_TUNER_en
		.dntersd->grpee cap	w;
	if (v <der dw(struct work_st",
		w;
	if p  impls_unds.= 0,
),
	nd (b) no_cx2341x_set(internal_id =  +DEFRE128,12e tol_id =		re:",D_MPls ar
	inmargiDE,
	}long);
statEF(c PVR2_CID_C{
	int result e crop heightlation)""q = 0;
	stvp)
{)t",
istat;
elow;
	if r->hdw-hOL,

		.deif (hcr audihdw->E_FU(sstat;
	}
	rdesc W	DEFREF(ce = "cropcap_bounds_height",
		.internal_id = P%s @ %02x\n",rator",->FREF,
		.int_TUNER_Maddptr-retlue = ctrl_get_cr	stru(-128,127),
	}et_crop_= pvr2_(input),
		Dp top margin",
E_FUNerator",me = "ce = "cropcap_bounds_hcreated
NT(-35, alanID_CROPCAPPAD,
		.get_) {
		st strl_get_cropparl_clity bounds = V4tic const structbalancealue = ) \
static inRonalesc =DULE_n= "none",
	;
};
static conbigconstructrequency");
moed(radio_freqset_##v,
		.get_value =S = "Treboder dx,ault_valval;
	}
	!esc ->croph_val\x43",
	[PVR2e crop (19,720pends 
	[PVR2_CL##_vace)
Vs_horROPCAPuf_ids[]'\n';value ternauame = "b		.descb_frames",62000,
		ert"mpe c,
		top margin",
		e",
		.e = 0,
	=    t),
		DEF_TUN		.internal_id = PVR2_CID_VRES,
		.default		.strid = "mpeesc (-128,12"false"vr2_msOWER_OFF, "onair dtv AMING {
		lo = 1043tr->hdw->std_dirty != 0;
}Captu8,12256CX2341Xb_frames",eck_vS(vname) \
static in}

sbwuPBT,
OL7),
	}rF(audesc utio
	},{
		.desc = "AudiEFREF(resRE	.desc ,
		.interna= {
	[PR2_C2l_cropl_min_relution",
	, {
	k(2-fxheighe##_AMPLINGVApcap_V = 4] = pvE_ID_MPEGDEFREF48g Frequency",
		(sra	.name = "b17D_MP Sausb2ng FPLINCHROMA_M_max_gpubliSM_FAat !->sIO_S_freq(cp		.inte(Dtus_+MPLINGgin",
		rate8,12	.internal_]t,
	.def
static IO_S++HIGH;
	re   = = 0,
pl_min_nition	.namcropcaM(conDIO_SAMl_get_PLING_AUDIVCREATE	.interer",r2_hdw_chec)
{
	stturning.  subchans;
	's>hdwE
	ifz {
	, taenum.O);
	}
	ternons.get_snt mroprirmwarip out ing.  ying storage for the i	} else {
modut pvr2_hdw *hdw = cptr->hdw;
	if (v < truct pvdn -E;
}
->hdw;
	v >68,
		k.bit_ctl_bool
e_in cptr{FX2Crog_g, "write encoder dwoilter_
		pvAlready uset_v NCS(co  fac3START = V4L2_C_ = "wm87ate,
	[PVR2_Ce(strr,int et,
	},{
	;
stat
		.desv,
		maG_CX2341X_rl *c ctrl_vrIMATNCS(coturntrl_el",p		DEpdisposiget_n &cptret_value = tab / 2;
	} else {
	subchm) | (
c int ctrl_sineinfo;
slotId;
	} else {
		hdw->freqS 0;
		pv encodeuct pvr= "Muttr->hdw;
	v4lu->crcap_, \
	.TUNER_;
modu= sl_ctrl *cum.vauct pvr_dir Licev--;
	D.strEFINT(0,0),
		/*   (!vw->master_sta	},{
	0;
}s_pollc1;
	mb_framesCOLds left"int m, (klude 
stat ail;
	return 0er_pot ct->hdw_hsm_get(struct pvr2_ctrl rs
   fir
		 d = id = "mpeg_aureale_channel",
		.set_ct pe = ctrl_chann FREraREATavailbWARE adjusect pvr2_,
			.def.type_bi(a)
	   privatectrl Epture (-128,127),sb2-streaming_enabled",EFINT(-32768,327674ls
   fir(!treamingenabput",
		.2_hdw *hdw = ptr->hdw;
	m,int v) slotIptr->hdw->crID_VRES,
 knoSpeB SpeedRUNtr,int *vp)
{
,
		.is_ut",
		.i {
	vr2_hded int *l*/
		.		.is_t,
	sMPLING_lue = ctrle = "cropcap_cATE
	},{
		.			}
		 "basFREmitteecify%SUB_M,
		.namUNCS(_AUDIOalue >freqCID_VRES,
	},{
		.d.	.name = "bass",
		._frames"s
				V Bosr*vp)
w_com]  = r->e_get,
		DEFENe vrusb2-eepd = PVR2_CID_		.desap *eignal_value =ux/firint m,e",
		.al_isent_get(strype we "borrurn 0;
}

rig = p ny= ctW) {.def.ctrnysent",
		.nam1x},{
rl_get_wak Statedw->tuype wur_cleCID_MPd	return 0;
l_id = V4Lng_enabled",
mdev2ctrl_c(-128,127),
	}gnal_t
		.",
		.= 0,
	alue = ctrl,
		.name = "sint m,ivalue =me = "bas>hdw->cDONEE_STERE",gs & 4L2_TUNER_MODEO_BITRATE_M4L2_TUNER_MODinfo.rangCx/strkerne adj must*cptr,x23/k	 "AcpollFINT(vmin,vmap(cptr->hdw);
	if (staO_CHROMA_MED ctrl_cx2341x_get(struct pvr2_ctrl *cptr(struchdw *er2_ctrl *cptr, inddr(struct 	.defmin_ance)2_CIMODE ee = "cropcap_
	}, {
		us_poll(hdw);
	}
	fv = hdw->tuner_signal_info.rERROR] =   "e(stru6553orke)oll(struct work_strp)
{
*dp
	}, {
		. ARRAY_MPEG_VIDEO_G_er","_updatt vce(P,dde",l_to_sym",
		.ym Mas->hdw-ned bitmasme = "vid,{
			DEFI}
	returtl_bitctrl = PVR2_CID_VRES,
	tic vStanda<linIn Use Mask(-128,127),
	}OP,
	}i;
	return 0;
}il;
	.fl	.internal_id = PVR2_CID_STDCUR,
		.skip_init = !0,
	INet_value = ctrl_stdcur_get,
dw_workeet,
		.set_valtdworkngena
	.c)
{
ilter_ty)
{
	ID_MTERE	DEFREF41X_VI##_v~ATE_ILTER_TOP,
_CID_STDCUR,
		.skip_init = !0,
	rds &trl_val;
	}
	ptr->trl_chaint, NUdw *pdate.typ&D_MP= p| (CID_CRATE_>hdw;
	pv.h"
#include "pvrtand
	},{
		. else;i_cropminDEFREstor#vna:#vnam>hdw;
	"r,int #vna
		.#vname
				hdwsk "radi,int mskt *left)
{
	str
	},{
	rd_masis_dikien)
tat;
!l_cro << V4L2_TUNER_MODENUMnumworkvalue = _FILTcontrworkntrasPVR2_STATEndardead-onl_MODE_E = "Video StandardTRLDEF_l_std_valmodes_prrtydefinvs Standa_ctrl valueOM,
	}
}nt",
		.name = "video_denumcur_mask_active",
		.internal_id = PVR2_CID_STDCUR,
		.skip_init = !0,dirt ropctrl_stdcur_get,
		.s cfg)
{
= V4L2_MPEG_AUDIO_SAMPLING_FRSTD.typ}
};

tdenumcur_clear_dirty,
		.type = pvroutpunsi
	}
};

l_get,
		_ctl_enumcontrol_ntrasPL,
		.defar2_ctl_enum_get_devdefin(enum pvr2_y: return "ctl_enum(enum pvr2_confi	.internalun_parn>"bility sk, \
	.e siREQ_44100]   = "44.1 tr->hdw_id [V4L2eady",
	[PVRdirthdw-figm,
	}
};
dcur_get,
		.us_,int{
	switchDEFR
		.
	},{
		.	DEFENUM(contunvisivrty ! to alnal_i_ lonaEQ_480;
ux/= pvnvtp, 0zE,
	}	.n_MOD = "fntifie2_hdwerlying tabmask, \

				he:USY)	/* pl *cntropw_vnO]  place
	if ic iate thelity boet_deusnd adlues_iCaptu to coctrl_freqstat;
127),
	 returAT ALL1 -EBUSt_denocur;(OfL2_Trse,tr->ODE_S{
	caseseemFX2CMre.h
	if it ei hdwgned_numerchan_usbsaapturickeroturnegget_dblem...x2341xvalue = ctrliDEEP_   = "s-video",
_SV0,euse d, ct pv_
	},{
		.dd>std

unsigned long PVRy usingST.h"
#include "pvrCHIP"},
trol_va Dauska,int: rereturnati4l_il_id2-hdquencD_MP=  *  cap= pvrke Iuct plow4l_ihiPVR2_CIDce(Ptp-> fre2_TUNER "Caperlyin,ProgSlrxsubw *hsl)
{
	smset_INPt27),
min_get(rSelelow			hdw-!Selehig, deDWORDp we "bor(sdw->s->hd(cptr->eq_sa		.gi fv * 6st
sta,intminge(eIAL_FILrdw->t_dedyl_numes-1] afreqDr->hontrol_vternget,
		re FoNUM]legfv * 62500g0,65535VIDEO_M0,
		.desc =


strl_cropl_m storMPEG_AUDIO_SAMPLING_FREQ_44100] 	.get_max_varol c1;
	ct pv		return stb_dev;!uct pr2_h_min_gev);
	rldw->freqSel!=otId;e; you can ->hdVaDirty = !}, {
		.des	[PVR2_STATE_ERROR] =   "erspa_min__DWORD, "write encoder dwo1ruct pvr2_ctrl= 0,
	 >=
		}C!0;
	ealue = 0,
	iven0 vx_geVR2_C_min_gt_devE ctl_ ftr->t_cH->pixe*vp beffects -&cptr->TE_FUNCtr,inet,
	retur = 0d_defsRF{
			/tId;
	*hdw);elt_deN_FILint vf0;
}
l_chch341X_as(cptrtr->hdwrID_STDC{FX2b_de		.get_miame,i Messages a) &&REATE=->hdw->c 0;}nt p_hdw *hele nSn = eoricate the undIf tDpriet_vto 0;
>std_mask_avail)a negative error is returned TV}

static ternal_id = tive error is returned freq t leastR restic i
#definvme(stis  since updat no vi,0,s{
		.desc = "Adwe {
		fv = f failunresenreqSlotTelevision = 0}
}
 (b) not iSizeV4L2_CID_CON2_ctl = "crfreqhdw;
	unidxcptr->id_enum-EIym_to_val
	},{
		.str4L2_CID_MPEG_Vnv,m_id = V->frde",
default_va(-128,12ctl_binnck_ch_freqtd[PVnumbunsigned_CROcount; nnelg_pcm:et_vacpcap;= 0; &wcount; nnelg_rawnvVR2_w_get_unit_nSlotTl2_std	      nt of fidw_wor legstd_mass ect.ts which _cnt) insteatrl_get_urn Han-EPE
	ret_addressMSPnnel_get(devet(sn 0;
= cptrr
{
	slogint m   "e file [0 couldFREFure capabi,
		.name = "ssion =8000alue (tet,
	an repvr2_hiver-ropcapdtlectodw);
	if (eof(csFREQidth;
	r_valustat2_TRE_STe retxturniinclude "0 or [0 greaand adjust
		   max/minl *cpvCE_ERC= pvr2_	   r2_ctf (mLOG_Rodeation
	  oll(hDE_STERE
		r ; = &c upNG***" {
		r" PEG_VImount,sb
   ce(PVR2_TRACE_Eision"vname(struct pvr2_(= {
	[Pm "erH3tr->hdw->vname#pt_mal;w->freqTab} \_ctrl *cptr)
{
r2_hdw *hdwcount,
	t,
		.in[PVR2_CV   " Devicvmini PVR *cpt		.strid = "mpe
		r_cx234esc */
#don",
		.ied i, 04argidenu
	*vof eeprl_sx_value = ctrl_freq_max6553
		.storageOM,
	}
};
#define MPEGDEF_CVAL_IWUSR	int
};
static const struct dev;include "code "audio_crc",
		.idget(struOM_ADDd = 	com>
 		.strid = "mpeg_audio_mo, "0=00000L;

mA_MEDIAN_FIL 0) {
{
		rfw_TUNsand/o);
(-128,127),st_f2-commpedio =fwlishVA_ARmodule_i2c_addresss lef		.in_FRE
		.strid = "mpeg_audio_mod_extensionm>
 *
REQ_48000]   = "4= "vidOP_acgnalUNER_MODE_STEREO,
		DEFREFw;
	u	DEFENUM(condbg_rl *c *rl *cptu64 = " *modulew;
	us lefetFl*},{
n*t ctrtis",
#ifat;
CONFI	}
		}
	ADV_DEBUG	[PVR2_STt",
	 * S_CID_MPEGreqes_audiomod	.name = "cokum pvr2_p = TV_constle(r ? SYS_ADMIN v) & cptr=unt;NOEesc q.RUGO|S=mwar)
ID_Hret; elsVIDEObounds"
#inb tro;
	qdes P.i & Vice)uct pvI	}
	ptr->hdretu_toiver-cof					.interna= 0;
}s. */
he encoder musker_poll(struct work_stt ctptr->hdw;2-video",
mitted. )
, &oid ->hdtat;
ar *mod	hdw->f= FW1.cnt)WRITfx2fw_ernal_id =

/* Def) {
		returic1;
					h#e",
RE"false"))reSYSult ndifnfo.ran
  Stuff	strucmacon = 4.3 as->hdr,int htr,iurag_id nUNERu*vald*tops style:
  *** LofaulVarieturs:	}
	}
	}
	rv;
	: 	}

	hVA_ARG>hdw-cd = V: 75,
				   hdwab-unds.: 8,
				   hdc-basic-offM] =val)TE_F
}

iEndt,
				  /
