/*
 * HD audio interface patch for AD1882, AD1884, AD1981HD, AD1983, AD1984,
 *   AD1986A, AD1988
 *
 * Copyright (c) 2005-2007 Takashi Iwai <tiwai@suse.de>
 *
 *  This driver is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This driver is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <linux/init.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/pci.h>

#include <sound/core.h>
#include "hda_codec.h"
#include "hda_local.h"
#include "hda_beep.h"

struct ad198x_spec {
	struct snd_kcontrol_new *mixers[5];
	int num_mixers;
	unsigned int beep_amp;	/* beep amp value, set via set_beep_amp() */
	const struct hda_verb *init_verbs[5];	/* initialization verbs
						 * don't forget NULL termination!
						 */
	unsigned int num_init_verbs;

	/* playback */
	struct hda_multi_out multiout;	/* playback set-up
					 * max_channels, dacs must be set
					 * dig_out_nid and hp_nid are optional
					 */
	unsigned int cur_eapd;
	unsigned int need_dac_fix;

	/* capture */
	unsigned int num_adc_nids;
	hda_nid_t *adc_nids;
	hda_nid_t dig_in_nid;		/* digital-in NID; optional */

	/* capture source */
	const struct hda_input_mux *input_mux;
	hda_nid_t *capsrc_nids;
	unsigned int cur_mux[3];

	/* channel model */
	const struct hda_channel_mode *channel_mode;
	int num_channel_mode;

	/* PCM information */
	struct hda_pcm pcm_rec[3];	/* used in alc_build_pcms() */

	unsigned int spdif_route;

	/* dynamic controls, init_verbs and input_mux */
	struct auto_pin_cfg autocfg;
	struct snd_array kctls;
	struct hda_input_mux private_imux;
	hda_nid_t private_dac_nids[AUTO_CFG_MAX_OUTS];

	unsigned int jack_present :1;
	unsigned int inv_jack_detect:1;

#ifdef CONFIG_SND_HDA_POWER_SAVE
	struct hda_loopback_check loopback;
#endif
	/* for virtual master */
	hda_nid_t vmaster_nid;
	const char **slave_vols;
	const char **slave_sws;
};

/*
 * input MUX handling (common part)
 */
static int ad198x_mux_enum_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct ad198x_spec *spec = codec->spec;

	return snd_hda_input_mux_info(spec->input_mux, uinfo);
}

static int ad198x_mux_enum_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct ad198x_spec *spec = codec->spec;
	unsigned int adc_idx = snd_ctl_get_ioffidx(kcontrol, &ucontrol->id);

	ucontrol->value.enumerated.item[0] = spec->cur_mux[adc_idx];
	return 0;
}

static int ad198x_mux_enum_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct ad198x_spec *spec = codec->spec;
	unsigned int adc_idx = snd_ctl_get_ioffidx(kcontrol, &ucontrol->id);

	return snd_hda_input_mux_put(codec, spec->input_mux, ucontrol,
				     spec->capsrc_nids[adc_idx],
				     &spec->cur_mux[adc_idx]);
}

/*
 * initialization (common callbacks)
 */
static int ad198x_init(struct hda_codec *codec)
{
	struct ad198x_spec *spec = codec->spec;
	int i;

	for (i = 0; i < spec->num_init_verbs; i++)
		snd_hda_sequence_write(codec, spec->init_verbs[i]);
	return 0;
}

static const char *ad_slave_vols[] = {
	"Front Playback Volume",
	"Surround Playback Volume",
	"Center Playback Volume",
	"LFE Playback Volume",
	"Side Playback Volume",
	"Headphone Playback Volume",
	"Mono Playback Volume",
	"Speaker Playback Volume",
	"IEC958 Playback Volume",
	NULL
};

static const char *ad_slave_sws[] = {
	"Front Playback Switch",
	"Surround Playback Switch",
	"Center Playback Switch",
	"LFE Playback Switch",
	"Side Playback Switch",
	"Headphone Playback Switch",
	"Mono Playback Switch",
	"Speaker Playback Switch",
	"IEC958 Playback Switch",
	NULL
};

static void ad198x_free_kctls(struct hda_codec *codec);

/* additional beep mixers; the actual parameters are overwritten at build */
static struct snd_kcontrol_new ad_beep_mixer[] = {
	HDA_CODEC_VOLUME("Beep Playback Volume", 0, 0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Beep Playback Switch", 0, 0, HDA_OUTPUT),
	{ } /* end */
};

#define set_beep_amp(spec, nid, idx, dir) \
	((spec)->beep_amp = HDA_COMPOSE_AMP_VAL(nid, 1, idx, dir)) /* mono */

static int ad198x_build_controls(struct hda_codec *codec)
{
	struct ad198x_spec *spec = codec->spec;
	unsigned int i;
	int err;

	for (i = 0; i < spec->num_mixers; i++) {
		err = snd_hda_add_new_ctls(codec, spec->mixers[i]);
		if (err < 0)
			return err;
	}
	if (spec->multiout.dig_out_nid) {
		err = snd_hda_create_spdif_out_ctls(codec, spec->multiout.dig_out_nid);
		if (err < 0)
			return err;
		err = snd_hda_create_spdif_share_sw(codec,
						    &spec->multiout);
		if (err < 0)
			return err;
		spec->multiout.share_spdif = 1;
	} 
	if (spec->dig_in_nid) {
		err = snd_hda_create_spdif_in_ctls(codec, spec->dig_in_nid);
		if (err < 0)
			return err;
	}

	/* create beep controls if needed */
	if (spec->beep_amp) {
		struct snd_kcontrol_new *knew;
		for (knew = ad_beep_mixer; knew->name; knew++) {
			struct snd_kcontrol *kctl;
			kctl = snd_ctl_new1(knew, codec);
			if (!kctl)
				return -ENOMEM;
			kctl->private_value = spec->beep_amp;
			err = snd_hda_ctl_add(codec, kctl);
			if (err < 0)
				return err;
		}
	}

	/* if we have no master control, let's create it */
	if (!snd_hda_find_mixer_ctl(codec, "Master Playback Volume")) {
		unsigned int vmaster_tlv[4];
		snd_hda_set_vmaster_tlv(codec, spec->vmaster_nid,
					HDA_OUTPUT, vmaster_tlv);
		err = snd_hda_add_vmaster(codec, "Master Playback Volume",
					  vmaster_tlv,
					  (spec->slave_vols ?
					   spec->slave_vols : ad_slave_vols));
		if (err < 0)
			return err;
	}
	if (!snd_hda_find_mixer_ctl(codec, "Master Playback Switch")) {
		err = snd_hda_add_vmaster(codec, "Master Playback Switch",
					  NULL,
					  (spec->slave_sws ?
					   spec->slave_sws : ad_slave_sws));
		if (err < 0)
			return err;
	}

	ad198x_free_kctls(codec); /* no longer needed */
	return 0;
}

#ifdef CONFIG_SND_HDA_POWER_SAVE
static int ad198x_check_power_status(struct hda_codec *codec, hda_nid_t nid)
{
	struct ad198x_spec *spec = codec->spec;
	return snd_hda_check_amp_list_power(codec, &spec->loopback, nid);
}
#endif

/*
 * Analog playback callbacks
 */
static int ad198x_playback_pcm_open(struct hda_pcm_stream *hinfo,
				    struct hda_codec *codec,
				    struct snd_pcm_substream *substream)
{
	struct ad198x_spec *spec = codec->spec;
	return snd_hda_multi_out_analog_open(codec, &spec->multiout, substream,
					     hinfo);
}

static int ad198x_playback_pcm_prepare(struct hda_pcm_stream *hinfo,
				       struct hda_codec *codec,
				       unsigned int stream_tag,
				       unsigned int format,
				       struct snd_pcm_substream *substream)
{
	struct ad198x_spec *spec = codec->spec;
	return snd_hda_multi_out_analog_prepare(codec, &spec->multiout, stream_tag,
						format, substream);
}

static int ad198x_playback_pcm_cleanup(struct hda_pcm_stream *hinfo,
				       struct hda_codec *codec,
				       struct snd_pcm_substream *substream)
{
	struct ad198x_spec *spec = codec->spec;
	return snd_hda_multi_out_analog_cleanup(codec, &spec->multiout);
}

/*
 * Digital out
 */
static int ad198x_dig_playback_pcm_open(struct hda_pcm_stream *hinfo,
					struct hda_codec *codec,
					struct snd_pcm_substream *substream)
{
	struct ad198x_spec *spec = codec->spec;
	return snd_hda_multi_out_dig_open(codec, &spec->multiout);
}

static int ad198x_dig_playback_pcm_close(struct hda_pcm_stream *hinfo,
					 struct hda_codec *codec,
					 struct snd_pcm_substream *substream)
{
	struct ad198x_spec *spec = codec->spec;
	return snd_hda_multi_out_dig_close(codec, &spec->multiout);
}

static int ad198x_dig_playback_pcm_prepare(struct hda_pcm_stream *hinfo,
					   struct hda_codec *codec,
					   unsigned int stream_tag,
					   unsigned int format,
					   struct snd_pcm_substream *substream)
{
	struct ad198x_spec *spec = codec->spec;
	return snd_hda_multi_out_dig_prepare(codec, &spec->multiout, stream_tag,
					     format, substream);
}

static int ad198x_dig_playback_pcm_cleanup(struct hda_pcm_stream *hinfo,
					   struct hda_codec *codec,
					   struct snd_pcm_substream *substream)
{
	struct ad198x_spec *spec = codec->spec;
	return snd_hda_multi_out_dig_cleanup(codec, &spec->multiout);
}

/*
 * Analog capture
 */
static int ad198x_capture_pcm_prepare(struct hda_pcm_stream *hinfo,
				      struct hda_codec *codec,
				      unsigned int stream_tag,
				      unsigned int format,
				      struct snd_pcm_substream *substream)
{
	struct ad198x_spec *spec = codec->spec;
	snd_hda_codec_setup_stream(codec, spec->adc_nids[substream->number],
				   stream_tag, 0, format);
	return 0;
}

static int ad198x_capture_pcm_cleanup(struct hda_pcm_stream *hinfo,
				      struct hda_codec *codec,
				      struct snd_pcm_substream *substream)
{
	struct ad198x_spec *spec = codec->spec;
	snd_hda_codec_cleanup_stream(codec, spec->adc_nids[substream->number]);
	return 0;
}


/*
 */
static struct hda_pcm_stream ad198x_pcm_analog_playback = {
	.substreams = 1,
	.channels_min = 2,
	.channels_max = 6, /* changed later */
	.nid = 0, /* fill later */
	.ops = {
		.open = ad198x_playback_pcm_open,
		.prepare = ad198x_playback_pcm_prepare,
		.cleanup = ad198x_playback_pcm_cleanup
	},
};

static struct hda_pcm_stream ad198x_pcm_analog_capture = {
	.substreams = 1,
	.channels_min = 2,
	.channels_max = 2,
	.nid = 0, /* fill later */
	.ops = {
		.prepare = ad198x_capture_pcm_prepare,
		.cleanup = ad198x_capture_pcm_cleanup
	},
};

static struct hda_pcm_stream ad198x_pcm_digital_playback = {
	.substreams = 1,
	.channels_min = 2,
	.channels_max = 2,
	.nid = 0, /* fill later */
	.ops = {
		.open = ad198x_dig_playback_pcm_open,
		.close = ad198x_dig_playback_pcm_close,
		.prepare = ad198x_dig_playback_pcm_prepare,
		.cleanup = ad198x_dig_playback_pcm_cleanup
	},
};

static struct hda_pcm_stream ad198x_pcm_digital_capture = {
	.substreams = 1,
	.channels_min = 2,
	.channels_max = 2,
	/* NID is set in alc_build_pcms */
};

static int ad198x_build_pcms(struct hda_codec *codec)
{
	struct ad198x_spec *spec = codec->spec;
	struct hda_pcm *info = spec->pcm_rec;

	codec->num_pcms = 1;
	codec->pcm_info = info;

	info->name = "AD198x Analog";
	info->stream[SNDRV_PCM_STREAM_PLAYBACK] = ad198x_pcm_analog_playback;
	info->stream[SNDRV_PCM_STREAM_PLAYBACK].channels_max = spec->multiout.max_channels;
	info->stream[SNDRV_PCM_STREAM_PLAYBACK].nid = spec->multiout.dac_nids[0];
	info->stream[SNDRV_PCM_STREAM_CAPTURE] = ad198x_pcm_analog_capture;
	info->stream[SNDRV_PCM_STREAM_CAPTURE].substreams = spec->num_adc_nids;
	info->stream[SNDRV_PCM_STREAM_CAPTURE].nid = spec->adc_nids[0];

	if (spec->multiout.dig_out_nid) {
		info++;
		codec->num_pcms++;
		info->name = "AD198x Digital";
		info->pcm_type = HDA_PCM_TYPE_SPDIF;
		info->stream[SNDRV_PCM_STREAM_PLAYBACK] = ad198x_pcm_digital_playback;
		info->stream[SNDRV_PCM_STREAM_PLAYBACK].nid = spec->multiout.dig_out_nid;
		if (spec->dig_in_nid) {
			info->stream[SNDRV_PCM_STREAM_CAPTURE] = ad198x_pcm_digital_capture;
			info->stream[SNDRV_PCM_STREAM_CAPTURE].nid = spec->dig_in_nid;
		}
	}

	return 0;
}

static void ad198x_free_kctls(struct hda_codec *codec)
{
	struct ad198x_spec *spec = codec->spec;

	if (spec->kctls.list) {
		struct snd_kcontrol_new *kctl = spec->kctls.list;
		int i;
		for (i = 0; i < spec->kctls.used; i++)
			kfree(kctl[i].name);
	}
	snd_array_free(&spec->kctls);
}

static void ad198x_free(struct hda_codec *codec)
{
	struct ad198x_spec *spec = codec->spec;

	if (!spec)
		return;

	ad198x_free_kctls(codec);
	kfree(spec);
	snd_hda_detach_beep_device(codec);
}

static struct hda_codec_ops ad198x_patch_ops = {
	.build_controls = ad198x_build_controls,
	.build_pcms = ad198x_build_pcms,
	.init = ad198x_init,
	.free = ad198x_free,
#ifdef CONFIG_SND_HDA_POWER_SAVE
	.check_power_status = ad198x_check_power_status,
#endif
};


/*
 * EAPD control
 * the private value = nid | (invert << 8)
 */
#define ad198x_eapd_info	snd_ctl_boolean_mono_info

static int ad198x_eapd_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct ad198x_spec *spec = codec->spec;
	int invert = (kcontrol->private_value >> 8) & 1;
	if (invert)
		ucontrol->value.integer.value[0] = ! spec->cur_eapd;
	else
		ucontrol->value.integer.value[0] = spec->cur_eapd;
	return 0;
}

static int ad198x_eapd_put(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct ad198x_spec *spec = codec->spec;
	int invert = (kcontrol->private_value >> 8) & 1;
	hda_nid_t nid = kcontrol->private_value & 0xff;
	unsigned int eapd;
	eapd = !!ucontrol->value.integer.value[0];
	if (invert)
		eapd = !eapd;
	if (eapd == spec->cur_eapd)
		return 0;
	spec->cur_eapd = eapd;
	snd_hda_codec_write_cache(codec, nid,
				  0, AC_VERB_SET_EAPD_BTLENABLE,
				  eapd ? 0x02 : 0x00);
	return 1;
}

static int ad198x_ch_mode_info(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_info *uinfo);
static int ad198x_ch_mode_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol);
static int ad198x_ch_mode_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol);


/*
 * AD1986A specific
 */

#define AD1986A_SPDIF_OUT	0x02
#define AD1986A_FRONT_DAC	0x03
#define AD1986A_SURR_DAC	0x04
#define AD1986A_CLFE_DAC	0x05
#define AD1986A_ADC		0x06

static hda_nid_t ad1986a_dac_nids[3] = {
	AD1986A_FRONT_DAC, AD1986A_SURR_DAC, AD1986A_CLFE_DAC
};
static hda_nid_t ad1986a_adc_nids[1] = { AD1986A_ADC };
static hda_nid_t ad1986a_capsrc_nids[1] = { 0x12 };

static struct hda_input_mux ad1986a_capture_source = {
	.num_items = 7,
	.items = {
		{ "Mic", 0x0 },
		{ "CD", 0x1 },
		{ "Aux", 0x3 },
		{ "Line", 0x4 },
		{ "Mix", 0x5 },
		{ "Mono", 0x6 },
		{ "Phone", 0x7 },
	},
};


static struct hda_bind_ctls ad1986a_bind_pcm_vol = {
	.ops = &snd_hda_bind_vol,
	.values = {
		HDA_COMPOSE_AMP_VAL(AD1986A_FRONT_DAC, 3, 0, HDA_OUTPUT),
		HDA_COMPOSE_AMP_VAL(AD1986A_SURR_DAC, 3, 0, HDA_OUTPUT),
		HDA_COMPOSE_AMP_VAL(AD1986A_CLFE_DAC, 3, 0, HDA_OUTPUT),
		0
	},
};

static struct hda_bind_ctls ad1986a_bind_pcm_sw = {
	.ops = &snd_hda_bind_sw,
	.values = {
		HDA_COMPOSE_AMP_VAL(AD1986A_FRONT_DAC, 3, 0, HDA_OUTPUT),
		HDA_COMPOSE_AMP_VAL(AD1986A_SURR_DAC, 3, 0, HDA_OUTPUT),
		HDA_COMPOSE_AMP_VAL(AD1986A_CLFE_DAC, 3, 0, HDA_OUTPUT),
		0
	},
};

/*
 * mixers
 */
static struct snd_kcontrol_new ad1986a_mixers[] = {
	/*
	 * bind volumes/mutes of 3 DACs as a single PCM control for simplicity
	 */
	HDA_BIND_VOL("PCM Playback Volume", &ad1986a_bind_pcm_vol),
	HDA_BIND_SW("PCM Playback Switch", &ad1986a_bind_pcm_sw),
	HDA_CODEC_VOLUME("Front Playback Volume", 0x1b, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Front Playback Switch", 0x1b, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Surround Playback Volume", 0x1c, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Surround Playback Switch", 0x1c, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("Center Playback Volume", 0x1d, 1, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("LFE Playback Volume", 0x1d, 2, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE_MONO("Center Playback Switch", 0x1d, 1, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE_MONO("LFE Playback Switch", 0x1d, 2, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Headphone Playback Volume", 0x1a, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Headphone Playback Switch", 0x1a, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("CD Playback Volume", 0x15, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("CD Playback Switch", 0x15, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Line Playback Volume", 0x17, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Line Playback Switch", 0x17, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Aux Playback Volume", 0x16, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Aux Playback Switch", 0x16, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x13, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Mic Playback Switch", 0x13, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Mic Boost", 0x0f, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Mono Playback Volume", 0x1e, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Mono Playback Switch", 0x1e, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Capture Volume", 0x12, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Capture Switch", 0x12, 0x0, HDA_OUTPUT),
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Capture Source",
		.info = ad198x_mux_enum_info,
		.get = ad198x_mux_enum_get,
		.put = ad198x_mux_enum_put,
	},
	HDA_CODEC_MUTE("Stereo Downmix Switch", 0x09, 0x0, HDA_OUTPUT),
	{ } /* end */
};

/* additional mixers for 3stack mode */
static struct snd_kcontrol_new ad1986a_3st_mixers[] = {
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Channel Mode",
		.info = ad198x_ch_mode_info,
		.get = ad198x_ch_mode_get,
		.put = ad198x_ch_mode_put,
	},
	{ } /* end */
};

/* laptop model - 2ch only */
static hda_nid_t ad1986a_laptop_dac_nids[1] = { AD1986A_FRONT_DAC };

/* master controls both pins 0x1a and 0x1b */
static struct hda_bind_ctls ad1986a_laptop_master_vol = {
	.ops = &snd_hda_bind_vol,
	.values = {
		HDA_COMPOSE_AMP_VAL(0x1a, 3, 0, HDA_OUTPUT),
		HDA_COMPOSE_AMP_VAL(0x1b, 3, 0, HDA_OUTPUT),
		0,
	},
};

static struct hda_bind_ctls ad1986a_laptop_master_sw = {
	.ops = &snd_hda_bind_sw,
	.values = {
		HDA_COMPOSE_AMP_VAL(0x1a, 3, 0, HDA_OUTPUT),
		HDA_COMPOSE_AMP_VAL(0x1b, 3, 0, HDA_OUTPUT),
		0,
	},
};

static struct snd_kcontrol_new ad1986a_laptop_mixers[] = {
	HDA_CODEC_VOLUME("PCM Playback Volume", 0x03, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("PCM Playback Switch", 0x03, 0x0, HDA_OUTPUT),
	HDA_BIND_VOL("Master Playback Volume", &ad1986a_laptop_master_vol),
	HDA_BIND_SW("Master Playback Switch", &ad1986a_laptop_master_sw),
	HDA_CODEC_VOLUME("CD Playback Volume", 0x15, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("CD Playback Switch", 0x15, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Line Playback Volume", 0x17, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Line Playback Switch", 0x17, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Aux Playback Volume", 0x16, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Aux Playback Switch", 0x16, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x13, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Mic Playback Switch", 0x13, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Mic Boost", 0x0f, 0x0, HDA_OUTPUT),
	/* 
	   HDA_CODEC_VOLUME("Mono Playback Volume", 0x1e, 0x0, HDA_OUTPUT),
	   HDA_CODEC_MUTE("Mono Playback Switch", 0x1e, 0x0, HDA_OUTPUT), */
	HDA_CODEC_VOLUME("Capture Volume", 0x12, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Capture Switch", 0x12, 0x0, HDA_OUTPUT),
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Capture Source",
		.info = ad198x_mux_enum_info,
		.get = ad198x_mux_enum_get,
		.put = ad198x_mux_enum_put,
	},
	{ } /* end */
};

/* laptop-eapd model - 2ch only */

static struct hda_input_mux ad1986a_laptop_eapd_capture_source = {
	.num_items = 3,
	.items = {
		{ "Mic", 0x0 },
		{ "Internal Mic", 0x4 },
		{ "Mix", 0x5 },
	},
};

static struct hda_input_mux ad1986a_automic_capture_source = {
	.num_items = 2,
	.items = {
		{ "Mic", 0x0 },
		{ "Mix", 0x5 },
	},
};

static struct snd_kcontrol_new ad1986a_laptop_master_mixers[] = {
	HDA_BIND_VOL("Master Playback Volume", &ad1986a_laptop_master_vol),
	HDA_BIND_SW("Master Playback Switch", &ad1986a_laptop_master_sw),
	{ } /* end */
};

static struct snd_kcontrol_new ad1986a_laptop_eapd_mixers[] = {
	HDA_CODEC_VOLUME("PCM Playback Volume", 0x03, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("PCM Playback Switch", 0x03, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x13, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Mic Playback Switch", 0x13, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Mic Boost", 0x0f, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Capture Volume", 0x12, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Capture Switch", 0x12, 0x0, HDA_OUTPUT),
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Capture Source",
		.info = ad198x_mux_enum_info,
		.get = ad198x_mux_enum_get,
		.put = ad198x_mux_enum_put,
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "External Amplifier",
		.info = ad198x_eapd_info,
		.get = ad198x_eapd_get,
		.put = ad198x_eapd_put,
		.private_value = 0x1b | (1 << 8), /* port-D, inversed */
	},
	{ } /* end */
};

static struct snd_kcontrol_new ad1986a_laptop_intmic_mixers[] = {
	HDA_CODEC_VOLUME("Internal Mic Playback Volume", 0x17, 0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Internal Mic Playback Switch", 0x17, 0, HDA_OUTPUT),
	{ } /* end */
};

/* re-connect the mic boost input according to the jack sensing */
static void ad1986a_automic(struct hda_codec *codec)
{
	unsigned int present;
	present = snd_hda_codec_read(codec, 0x1f, 0, AC_VERB_GET_PIN_SENSE, 0);
	/* 0 = 0x1f, 2 = 0x1d, 4 = mixed */
	snd_hda_codec_write(codec, 0x0f, 0, AC_VERB_SET_CONNECT_SEL,
			    (present & AC_PINSENSE_PRESENCE) ? 0 : 2);
}

#define AD1986A_MIC_EVENT		0x36

static void ad1986a_automic_unsol_event(struct hda_codec *codec,
					    unsigned int res)
{
	if ((res >> 26) != AD1986A_MIC_EVENT)
		return;
	ad1986a_automic(codec);
}

static int ad1986a_automic_init(struct hda_codec *codec)
{
	ad198x_init(codec);
	ad1986a_automic(codec);
	return 0;
}

/* laptop-automute - 2ch only */

static void ad1986a_update_hp(struct hda_codec *codec)
{
	struct ad198x_spec *spec = codec->spec;
	unsigned int mute;

	if (spec->jack_present)
		mute = HDA_AMP_MUTE; /* mute internal speaker */
	else
		/* unmute internal speaker if necessary */
		mute = snd_hda_codec_amp_read(codec, 0x1a, 0, HDA_OUTPUT, 0);
	snd_hda_codec_amp_stereo(codec, 0x1b, HDA_OUTPUT, 0,
				 HDA_AMP_MUTE, mute);
}

static void ad1986a_hp_automute(struct hda_codec *codec)
{
	struct ad198x_spec *spec = codec->spec;
	unsigned int present;

	present = snd_hda_codec_read(codec, 0x1a, 0, AC_VERB_GET_PIN_SENSE, 0);
	spec->jack_present = !!(present & 0x80000000);
	if (spec->inv_jack_detect)
		spec->jack_present = !spec->jack_present;
	ad1986a_update_hp(codec);
}

#define AD1986A_HP_EVENT		0x37

static void ad1986a_hp_unsol_event(struct hda_codec *codec, unsigned int res)
{
	if ((res >> 26) != AD1986A_HP_EVENT)
		return;
	ad1986a_hp_automute(codec);
}

static int ad1986a_hp_init(struct hda_codec *codec)
{
	ad198x_init(codec);
	ad1986a_hp_automute(codec);
	return 0;
}

/* bind hp and internal speaker mute (with plug check) */
static int ad1986a_hp_master_sw_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	long *valp = ucontrol->value.integer.value;
	int change;

	change = snd_hda_codec_amp_update(codec, 0x1a, 0, HDA_OUTPUT, 0,
					  HDA_AMP_MUTE,
					  valp[0] ? 0 : HDA_AMP_MUTE);
	change |= snd_hda_codec_amp_update(codec, 0x1a, 1, HDA_OUTPUT, 0,
					   HDA_AMP_MUTE,
					   valp[1] ? 0 : HDA_AMP_MUTE);
	if (change)
		ad1986a_update_hp(codec);
	return change;
}

static struct snd_kcontrol_new ad1986a_automute_master_mixers[] = {
	HDA_BIND_VOL("Master Playback Volume", &ad1986a_laptop_master_vol),
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Master Playback Switch",
		.info = snd_hda_mixer_amp_switch_info,
		.get = snd_hda_mixer_amp_switch_get,
		.put = ad1986a_hp_master_sw_put,
		.private_value = HDA_COMPOSE_AMP_VAL(0x1a, 3, 0, HDA_OUTPUT),
	},
	{ } /* end */
};


/*
 * initialization verbs
 */
static struct hda_verb ad1986a_init_verbs[] = {
	/* Front, Surround, CLFE DAC; mute as default */
	{0x03, AC_VERB_SET_AMP_GAIN_MUTE, 0xb080},
	{0x04, AC_VERB_SET_AMP_GAIN_MUTE, 0xb080},
	{0x05, AC_VERB_SET_AMP_GAIN_MUTE, 0xb080},
	/* Downmix - off */
	{0x09, AC_VERB_SET_AMP_GAIN_MUTE, 0xb080},
	/* HP, Line-Out, Surround, CLFE selectors */
	{0x0a, AC_VERB_SET_CONNECT_SEL, 0x0},
	{0x0b, AC_VERB_SET_CONNECT_SEL, 0x0},
	{0x0c, AC_VERB_SET_CONNECT_SEL, 0x0},
	{0x0d, AC_VERB_SET_CONNECT_SEL, 0x0},
	/* Mono selector */
	{0x0e, AC_VERB_SET_CONNECT_SEL, 0x0},
	/* Mic selector: Mic 1/2 pin */
	{0x0f, AC_VERB_SET_CONNECT_SEL, 0x0},
	/* Line-in selector: Line-in */
	{0x10, AC_VERB_SET_CONNECT_SEL, 0x0},
	/* Mic 1/2 swap */
	{0x11, AC_VERB_SET_CONNECT_SEL, 0x0},
	/* Record selector: mic */
	{0x12, AC_VERB_SET_CONNECT_SEL, 0x0},
	/* Mic, Phone, CD, Aux, Line-In amp; mute as default */
	{0x13, AC_VERB_SET_AMP_GAIN_MUTE, 0xb080},
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, 0xb080},
	{0x15, AC_VERB_SET_AMP_GAIN_MUTE, 0xb080},
	{0x16, AC_VERB_SET_AMP_GAIN_MUTE, 0xb080},
	{0x17, AC_VERB_SET_AMP_GAIN_MUTE, 0xb080},
	/* PC beep */
	{0x18, AC_VERB_SET_CONNECT_SEL, 0x0},
	/* HP, Line-Out, Surround, CLFE, Mono pins; mute as default */
	{0x1a, AC_VERB_SET_AMP_GAIN_MUTE, 0xb080},
	{0x1b, AC_VERB_SET_AMP_GAIN_MUTE, 0xb080},
	{0x1c, AC_VERB_SET_AMP_GAIN_MUTE, 0xb080},
	{0x1d, AC_VERB_SET_AMP_GAIN_MUTE, 0xb080},
	{0x1e, AC_VERB_SET_AMP_GAIN_MUTE, 0xb080},
	/* HP Pin */
	{0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, 0xc0 },
	/* Front, Surround, CLFE Pins */
	{0x1b, AC_VERB_SET_PIN_WIDGET_CONTROL, 0x40 },
	{0x1c, AC_VERB_SET_PIN_WIDGET_CONTROL, 0x40 },
	{0x1d, AC_VERB_SET_PIN_WIDGET_CONTROL, 0x40 },
	/* Mono Pin */
	{0x1e, AC_VERB_SET_PIN_WIDGET_CONTROL, 0x40 },
	/* Mic Pin */
	{0x1f, AC_VERB_SET_PIN_WIDGET_CONTROL, 0x24 },
	/* Line, Aux, CD, Beep-In Pin */
	{0x20, AC_VERB_SET_PIN_WIDGET_CONTROL, 0x20 },
	{0x21, AC_VERB_SET_PIN_WIDGET_CONTROL, 0x20 },
	{0x22, AC_VERB_SET_PIN_WIDGET_CONTROL, 0x20 },
	{0x23, AC_VERB_SET_PIN_WIDGET_CONTROL, 0x20 },
	{0x24, AC_VERB_SET_PIN_WIDGET_CONTROL, 0x20 },
	{ } /* end */
};

static struct hda_verb ad1986a_ch2_init[] = {
	/* Surround out -> Line In */
	{ 0x1c, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN },
 	/* Line-in selectors */
	{ 0x10, AC_VERB_SET_CONNECT_SEL, 0x1 },
	/* CLFE -> Mic in */
	{ 0x1d, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80 },
	/* Mic selector, mix C/LFE (backmic) and Mic (frontmic) */
	{ 0x0f, AC_VERB_SET_CONNECT_SEL, 0x4 },
	{ } /* end */
};

static struct hda_verb ad1986a_ch4_init[] = {
	/* Surround out -> Surround */
	{ 0x1c, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x10, AC_VERB_SET_CONNECT_SEL, 0x0 },
	/* CLFE -> Mic in */
	{ 0x1d, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80 },
	{ 0x0f, AC_VERB_SET_CONNECT_SEL, 0x4 },
	{ } /* end */
};

static struct hda_verb ad1986a_ch6_init[] = {
	/* Surround out -> Surround out */
	{ 0x1c, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x10, AC_VERB_SET_CONNECT_SEL, 0x0 },
	/* CLFE -> CLFE */
	{ 0x1d, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x0f, AC_VERB_SET_CONNECT_SEL, 0x0 },
	{ } /* end */
};

static struct hda_channel_mode ad1986a_modes[3] = {
	{ 2, ad1986a_ch2_init },
	{ 4, ad1986a_ch4_init },
	{ 6, ad1986a_ch6_init },
};

/* eapd initialization */
static struct hda_verb ad1986a_eapd_init_verbs[] = {
	{0x1b, AC_VERB_SET_EAPD_BTLENABLE, 0x00 },
	{}
};

static struct hda_verb ad1986a_automic_verbs[] = {
	{0x1d, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x1f, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	/*{0x20, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},*/
	{0x0f, AC_VERB_SET_CONNECT_SEL, 0x0},
	{0x1f, AC_VERB_SET_UNSOLICITED_ENABLE, AC_USRSP_EN | AD1986A_MIC_EVENT},
	{}
};

/* Ultra initialization */
static struct hda_verb ad1986a_ultra_init[] = {
	/* eapd initialization */
	{ 0x1b, AC_VERB_SET_EAPD_BTLENABLE, 0x00 },
	/* CLFE -> Mic in */
	{ 0x0f, AC_VERB_SET_CONNECT_SEL, 0x2 },
	{ 0x1d, AC_VERB_SET_PIN_WIDGET_CONTROL, 0x24 },
	{ 0x1d, AC_VERB_SET_AMP_GAIN_MUTE, 0xb080 },
	{ } /* end */
};

/* pin sensing on HP jack */
static struct hda_verb ad1986a_hp_init_verbs[] = {
	{0x1a, AC_VERB_SET_UNSOLICITED_ENABLE, AC_USRSP_EN | AD1986A_HP_EVENT},
	{}
};

static void ad1986a_samsung_p50_unsol_event(struct hda_codec *codec,
					    unsigned int res)
{
	switch (res >> 26) {
	case AD1986A_HP_EVENT:
		ad1986a_hp_automute(codec);
		break;
	case AD1986A_MIC_EVENT:
		ad1986a_automic(codec);
		break;
	}
}

static int ad1986a_samsung_p50_init(struct hda_codec *codec)
{
	ad198x_init(codec);
	ad1986a_hp_automute(codec);
	ad1986a_automic(codec);
	return 0;
}


/* models */
enum {
	AD1986A_6STACK,
	AD1986A_3STACK,
	AD1986A_LAPTOP,
	AD1986A_LAPTOP_EAPD,
	AD1986A_LAPTOP_AUTOMUTE,
	AD1986A_ULTRA,
	AD1986A_SAMSUNG,
	AD1986A_SAMSUNG_P50,
	AD1986A_MODELS
};

static const char *ad1986a_models[AD1986A_MODELS] = {
	[AD1986A_6STACK]	= "6stack",
	[AD1986A_3STACK]	= "3stack",
	[AD1986A_LAPTOP]	= "laptop",
	[AD1986A_LAPTOP_EAPD]	= "laptop-eapd",
	[AD1986A_LAPTOP_AUTOMUTE] = "laptop-automute",
	[AD1986A_ULTRA]		= "ultra",
	[AD1986A_SAMSUNG]	= "samsung",
	[AD1986A_SAMSUNG_P50]	= "samsung-p50",
};

static struct snd_pci_quirk ad1986a_cfg_tbl[] = {
	SND_PCI_QUIRK(0x103c, 0x30af, "HP B2800", AD1986A_LAPTOP_EAPD),
	SND_PCI_QUIRK(0x1043, 0x1153, "ASUS M9", AD1986A_LAPTOP_EAPD),
	SND_PCI_QUIRK(0x1043, 0x11f7, "ASUS U5A", AD1986A_LAPTOP_EAPD),
	SND_PCI_QUIRK(0x1043, 0x1213, "ASUS A6J", AD1986A_LAPTOP_EAPD),
	SND_PCI_QUIRK(0x1043, 0x1263, "ASUS U5F", AD1986A_LAPTOP_EAPD),
	SND_PCI_QUIRK(0x1043, 0x1297, "ASUS Z62F", AD1986A_LAPTOP_EAPD),
	SND_PCI_QUIRK(0x1043, 0x12b3, "ASUS V1j", AD1986A_LAPTOP_EAPD),
	SND_PCI_QUIRK(0x1043, 0x1302, "ASUS W3j", AD1986A_LAPTOP_EAPD),
	SND_PCI_QUIRK(0x1043, 0x1443, "ASUS VX1", AD1986A_LAPTOP),
	SND_PCI_QUIRK(0x1043, 0x1447, "ASUS A8J", AD1986A_3STACK),
	SND_PCI_QUIRK(0x1043, 0x817f, "ASUS P5", AD1986A_3STACK),
	SND_PCI_QUIRK(0x1043, 0x818f, "ASUS P5", AD1986A_LAPTOP),
	SND_PCI_QUIRK(0x1043, 0x81b3, "ASUS P5", AD1986A_3STACK),
	SND_PCI_QUIRK(0x1043, 0x81cb, "ASUS M2N", AD1986A_3STACK),
	SND_PCI_QUIRK(0x1043, 0x8234, "ASUS M2N", AD1986A_3STACK),
	SND_PCI_QUIRK(0x10de, 0xcb84, "ASUS A8N-VM", AD1986A_3STACK),
	SND_PCI_QUIRK(0x1179, 0xff40, "Toshiba", AD1986A_LAPTOP_EAPD),
	SND_PCI_QUIRK(0x144d, 0xb03c, "Samsung R55", AD1986A_3STACK),
	SND_PCI_QUIRK(0x144d, 0xc01e, "FSC V2060", AD1986A_LAPTOP),
	SND_PCI_QUIRK(0x144d, 0xc024, "Samsung P50", AD1986A_SAMSUNG_P50),
	SND_PCI_QUIRK(0x144d, 0xc027, "Samsung Q1", AD1986A_ULTRA),
	SND_PCI_QUIRK_MASK(0x144d, 0xff00, 0xc000, "Samsung", AD1986A_SAMSUNG),
	SND_PCI_QUIRK(0x144d, 0xc504, "Samsung Q35", AD1986A_3STACK),
	SND_PCI_QUIRK(0x17aa, 0x1011, "Lenovo M55", AD1986A_LAPTOP),
	SND_PCI_QUIRK(0x17aa, 0x1017, "Lenovo A60", AD1986A_3STACK),
	SND_PCI_QUIRK(0x17aa, 0x2066, "Lenovo N100", AD1986A_LAPTOP_AUTOMUTE),
	SND_PCI_QUIRK(0x17c0, 0x2017, "Samsung M50", AD1986A_LAPTOP),
	{}
};

#ifdef CONFIG_SND_HDA_POWER_SAVE
static struct hda_amp_list ad1986a_loopbacks[] = {
	{ 0x13, HDA_OUTPUT, 0 }, /* Mic */
	{ 0x14, HDA_OUTPUT, 0 }, /* Phone */
	{ 0x15, HDA_OUTPUT, 0 }, /* CD */
	{ 0x16, HDA_OUTPUT, 0 }, /* Aux */
	{ 0x17, HDA_OUTPUT, 0 }, /* Line */
	{ } /* end */
};
#endif

static int is_jack_available(struct hda_codec *codec, hda_nid_t nid)
{
	unsigned int conf = snd_hda_codec_get_pincfg(codec, nid);
	return get_defcfg_connect(conf) != AC_JACK_PORT_NONE;
}

static int patch_ad1986a(struct hda_codec *codec)
{
	struct ad198x_spec *spec;
	int err, board_config;

	spec = kzalloc(sizeof(*spec), GFP_KERNEL);
	if (spec == NULL)
		return -ENOMEM;

	codec->spec = spec;

	err = snd_hda_attach_beep_device(codec, 0x19);
	if (err < 0) {
		ad198x_free(codec);
		return err;
	}
	set_beep_amp(spec, 0x18, 0, HDA_OUTPUT);

	spec->multiout.max_channels = 6;
	spec->multiout.num_dacs = ARRAY_SIZE(ad1986a_dac_nids);
	spec->multiout.dac_nids = ad1986a_dac_nids;
	spec->multiout.dig_out_nid = AD1986A_SPDIF_OUT;
	spec->num_adc_nids = 1;
	spec->adc_nids = ad1986a_adc_nids;
	spec->capsrc_nids = ad1986a_capsrc_nids;
	spec->input_mux = &ad1986a_capture_source;
	spec->num_mixers = 1;
	spec->mixers[0] = ad1986a_mixers;
	spec->num_init_verbs = 1;
	spec->init_verbs[0] = ad1986a_init_verbs;
#ifdef CONFIG_SND_HDA_POWER_SAVE
	spec->loopback.amplist = ad1986a_loopbacks;
#endif
	spec->vmaster_nid = 0x1b;

	codec->patch_ops = ad198x_patch_ops;

	/* override some parameters */
	board_config = snd_hda_check_board_config(codec, AD1986A_MODELS,
						  ad1986a_models,
						  ad1986a_cfg_tbl);
	switch (board_config) {
	case AD1986A_3STACK:
		spec->num_mixers = 2;
		spec->mixers[1] = ad1986a_3st_mixers;
		spec->num_init_verbs = 2;
		spec->init_verbs[1] = ad1986a_ch2_init;
		spec->channel_mode = ad1986a_modes;
		spec->num_channel_mode = ARRAY_SIZE(ad1986a_modes);
		spec->need_dac_fix = 1;
		spec->multiout.max_channels = 2;
		spec->multiout.num_dacs = 1;
		break;
	case AD1986A_LAPTOP:
		spec->mixers[0] = ad1986a_laptop_mixers;
		spec->multiout.max_channels = 2;
		spec->multiout.num_dacs = 1;
		spec->multiout.dac_nids = ad1986a_laptop_dac_nids;
		break;
	case AD1986A_LAPTOP_EAPD:
		spec->num_mixers = 3;
		spec->mixers[0] = ad1986a_laptop_master_mixers;
		spec->mixers[1] = ad1986a_laptop_eapd_mixers;
		spec->mixers[2] = ad1986a_laptop_intmic_mixers;
		spec->num_init_verbs = 2;
		spec->init_verbs[1] = ad1986a_eapd_init_verbs;
		spec->multiout.max_channels = 2;
		spec->multiout.num_dacs = 1;
		spec->multiout.dac_nids = ad1986a_laptop_dac_nids;
		if (!is_jack_available(codec, 0x25))
			spec->multiout.dig_out_nid = 0;
		spec->input_mux = &ad1986a_laptop_eapd_capture_source;
		break;
	case AD1986A_SAMSUNG:
		spec->num_mixers = 2;
		spec->mixers[0] = ad1986a_laptop_master_mixers;
		spec->mixers[1] = ad1986a_laptop_eapd_mixers;
		spec->num_init_verbs = 3;
		spec->init_verbs[1] = ad1986a_eapd_init_verbs;
		spec->init_verbs[2] = ad1986a_automic_verbs;
		spec->multiout.max_channels = 2;
		spec->multiout.num_dacs = 1;
		spec->multiout.dac_nids = ad1986a_laptop_dac_nids;
		if (!is_jack_available(codec, 0x25))
			spec->multiout.dig_out_nid = 0;
		spec->input_mux = &ad1986a_automic_capture_source;
		codec->patch_ops.unsol_event = ad1986a_automic_unsol_event;
		codec->patch_ops.init = ad1986a_automic_init;
		break;
	case AD1986A_SAMSUNG_P50:
		spec->num_mixers = 2;
		spec->mixers[0] = ad1986a_automute_master_mixers;
		spec->mixers[1] = ad1986a_laptop_eapd_mixers;
		spec->num_init_verbs = 4;
		spec->init_verbs[1] = ad1986a_eapd_init_verbs;
		spec->init_verbs[2] = ad1986a_automic_verbs;
		spec->init_verbs[3] = ad1986a_hp_init_verbs;
		spec->multiout.max_channels = 2;
		spec->multiout.num_dacs = 1;
		spec->multiout.dac_nids = ad1986a_laptop_dac_nids;
		if (!is_jack_available(codec, 0x25))
			spec->multiout.dig_out_nid = 0;
		spec->input_mux = &ad1986a_automic_capture_source;
		codec->patch_ops.unsol_event = ad1986a_samsung_p50_unsol_event;
		codec->patch_ops.init = ad1986a_samsung_p50_init;
		break;
	case AD1986A_LAPTOP_AUTOMUTE:
		spec->num_mixers = 3;
		spec->mixers[0] = ad1986a_automute_master_mixers;
		spec->mixers[1] = ad1986a_laptop_eapd_mixers;
		spec->mixers[2] = ad1986a_laptop_intmic_mixers;
		spec->num_init_verbs = 3;
		spec->init_verbs[1] = ad1986a_eapd_init_verbs;
		spec->init_verbs[2] = ad1986a_hp_init_verbs;
		spec->multiout.max_channels = 2;
		spec->multiout.num_dacs = 1;
		spec->multiout.dac_nids = ad1986a_laptop_dac_nids;
		if (!is_jack_available(codec, 0x25))
			spec->multiout.dig_out_nid = 0;
		spec->input_mux = &ad1986a_laptop_eapd_capture_source;
		codec->patch_ops.unsol_event = ad1986a_hp_unsol_event;
		codec->patch_ops.init = ad1986a_hp_init;
		/* Lenovo N100 seems to report the reversed bit
		 * for HP jack-sensing
		 */
		spec->inv_jack_detect = 1;
		break;
	case AD1986A_ULTRA:
		spec->mixers[0] = ad1986a_laptop_eapd_mixers;
		spec->num_init_verbs = 2;
		spec->init_verbs[1] = ad1986a_ultra_init;
		spec->multiout.max_channels = 2;
		spec->multiout.num_dacs = 1;
		spec->multiout.dac_nids = ad1986a_laptop_dac_nids;
		spec->multiout.dig_out_nid = 0;
		break;
	}

	/* AD1986A has a hardware problem that it can't share a stream
	 * with multiple output pins.  The copy of front to surrounds
	 * causes noisy or silent outputs at a certain timing, e.g.
	 * changing the volume.
	 * So, let's disable the shared stream.
	 */
	spec->multiout.no_share_stream = 1;

	return 0;
}

/*
 * AD1983 specific
 */

#define AD1983_SPDIF_OUT	0x02
#define AD1983_DAC		0x03
#define AD1983_ADC		0x04

static hda_nid_t ad1983_dac_nids[1] = { AD1983_DAC };
static hda_nid_t ad1983_adc_nids[1] = { AD1983_ADC };
static hda_nid_t ad1983_capsrc_nids[1] = { 0x15 };

static struct hda_input_mux ad1983_capture_source = {
	.num_items = 4,
	.items = {
		{ "Mic", 0x0 },
		{ "Line", 0x1 },
		{ "Mix", 0x2 },
		{ "Mix Mono", 0x3 },
	},
};

/*
 * SPDIF playback route
 */
static int ad1983_spdif_route_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	static char *texts[] = { "PCM", "ADC" };

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = 2;
	if (uinfo->value.enumerated.item > 1)
		uinfo->value.enumerated.item = 1;
	strcpy(uinfo->value.enumerated.name, texts[uinfo->value.enumerated.item]);
	return 0;
}

static int ad1983_spdif_route_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct ad198x_spec *spec = codec->spec;

	ucontrol->value.enumerated.item[0] = spec->spdif_route;
	return 0;
}

static int ad1983_spdif_route_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct ad198x_spec *spec = codec->spec;

	if (ucontrol->value.enumerated.item[0] > 1)
		return -EINVAL;
	if (spec->spdif_route != ucontrol->value.enumerated.item[0]) {
		spec->spdif_route = ucontrol->value.enumerated.item[0];
		snd_hda_codec_write_cache(codec, spec->multiout.dig_out_nid, 0,
					  AC_VERB_SET_CONNECT_SEL,
					  spec->spdif_route);
		return 1;
	}
	return 0;
}

static struct snd_kcontrol_new ad1983_mixers[] = {
	HDA_CODEC_VOLUME("Front Playback Volume", 0x05, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Front Playback Switch", 0x05, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Headphone Playback Volume", 0x06, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Headphone Playback Switch", 0x06, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("Mono Playback Volume", 0x07, 1, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE_MONO("Mono Playback Switch", 0x07, 1, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("PCM Playback Volume", 0x11, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("PCM Playback Switch", 0x11, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x12, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Mic Playback Switch", 0x12, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Line Playback Volume", 0x13, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Line Playback Switch", 0x13, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Mic Boost", 0x0c, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Capture Volume", 0x15, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Capture Switch", 0x15, 0x0, HDA_OUTPUT),
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Capture Source",
		.info = ad198x_mux_enum_info,
		.get = ad198x_mux_enum_get,
		.put = ad198x_mux_enum_put,
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = SNDRV_CTL_NAME_IEC958("",PLAYBACK,NONE) "Source",
		.info = ad1983_spdif_route_info,
		.get = ad1983_spdif_route_get,
		.put = ad1983_spdif_route_put,
	},
	{ } /* end */
};

static struct hda_verb ad1983_init_verbs[] = {
	/* Front, HP, Mono; mute as default */
	{0x05, AC_VERB_SET_AMP_GAIN_MUTE, 0xb080},
	{0x06, AC_VERB_SET_AMP_GAIN_MUTE, 0xb080},
	{0x07, AC_VERB_SET_AMP_GAIN_MUTE, 0xb080},
	/* Beep, PCM, Mic, Line-In: mute */
	{0x10, AC_VERB_SET_AMP_GAIN_MUTE, 0xb080},
	{0x11, AC_VERB_SET_AMP_GAIN_MUTE, 0xb080},
	{0x12, AC_VERB_SET_AMP_GAIN_MUTE, 0xb080},
	{0x13, AC_VERB_SET_AMP_GAIN_MUTE, 0xb080},
	/* Front, HP selectors; from Mix */
	{0x05, AC_VERB_SET_CONNECT_SEL, 0x01},
	{0x06, AC_VERB_SET_CONNECT_SEL, 0x01},
	/* Mono selector; from Mix */
	{0x0b, AC_VERB_SET_CONNECT_SEL, 0x03},
	/* Mic selector; Mic */
	{0x0c, AC_VERB_SET_CONNECT_SEL, 0x0},
	/* Line-in selector: Line-in */
	{0x0d, AC_VERB_SET_CONNECT_SEL, 0x0},
	/* Mic boost: 0dB */
	{0x0c, AC_VERB_SET_AMP_GAIN_MUTE, 0xb000},
	/* Record selector: mic */
	{0x15, AC_VERB_SET_CONNECT_SEL, 0x0},
	{0x15, AC_VERB_SET_AMP_GAIN_MUTE, 0xb080},
	/* SPDIF route: PCM */
	{0x02, AC_VERB_SET_CONNECT_SEL, 0x0},
	/* Front Pin */
	{0x05, AC_VERB_SET_PIN_WIDGET_CONTROL, 0x40 },
	/* HP Pin */
	{0x06, AC_VERB_SET_PIN_WIDGET_CONTROL, 0xc0 },
	/* Mono Pin */
	{0x07, AC_VERB_SET_PIN_WIDGET_CONTROL, 0x40 },
	/* Mic Pin */
	{0x08, AC_VERB_SET_PIN_WIDGET_CONTROL, 0x24 },
	/* Line Pin */
	{0x09, AC_VERB_SET_PIN_WIDGET_CONTROL, 0x20 },
	{ } /* end */
};

#ifdef CONFIG_SND_HDA_POWER_SAVE
static struct hda_amp_list ad1983_loopbacks[] = {
	{ 0x12, HDA_OUTPUT, 0 }, /* Mic */
	{ 0x13, HDA_OUTPUT, 0 }, /* Line */
	{ } /* end */
};
#endif

static int patch_ad1983(struct hda_codec *codec)
{
	struct ad198x_spec *spec;
	int err;

	spec = kzalloc(sizeof(*spec), GFP_KERNEL);
	if (spec == NULL)
		return -ENOMEM;

	codec->spec = spec;

	err = snd_hda_attach_beep_device(codec, 0x10);
	if (err < 0) {
		ad198x_free(codec);
		return err;
	}
	set_beep_amp(spec, 0x10, 0, HDA_OUTPUT);

	spec->multiout.max_channels = 2;
	spec->multiout.num_dacs = ARRAY_SIZE(ad1983_dac_nids);
	spec->multiout.dac_nids = ad1983_dac_nids;
	spec->multiout.dig_out_nid = AD1983_SPDIF_OUT;
	spec->num_adc_nids = 1;
	spec->adc_nids = ad1983_adc_nids;
	spec->capsrc_nids = ad1983_capsrc_nids;
	spec->input_mux = &ad1983_capture_source;
	spec->num_mixers = 1;
	spec->mixers[0] = ad1983_mixers;
	spec->num_init_verbs = 1;
	spec->init_verbs[0] = ad1983_init_verbs;
	spec->spdif_route = 0;
#ifdef CONFIG_SND_HDA_POWER_SAVE
	spec->loopback.amplist = ad1983_loopbacks;
#endif
	spec->vmaster_nid = 0x05;

	codec->patch_ops = ad198x_patch_ops;

	return 0;
}


/*
 * AD1981 HD specific
 */

#define AD1981_SPDIF_OUT	0x02
#define AD1981_DAC		0x03
#define AD1981_ADC		0x04

static hda_nid_t ad1981_dac_nids[1] = { AD1981_DAC };
static hda_nid_t ad1981_adc_nids[1] = { AD1981_ADC };
static hda_nid_t ad1981_capsrc_nids[1] = { 0x15 };

/* 0x0c, 0x09, 0x0e, 0x0f, 0x19, 0x05, 0x18, 0x17 */
static struct hda_input_mux ad1981_capture_source = {
	.num_items = 7,
	.items = {
		{ "Front Mic", 0x0 },
		{ "Line", 0x1 },
		{ "Mix", 0x2 },
		{ "Mix Mono", 0x3 },
		{ "CD", 0x4 },
		{ "Mic", 0x6 },
		{ "Aux", 0x7 },
	},
};

static struct snd_kcontrol_new ad1981_mixers[] = {
	HDA_CODEC_VOLUME("Front Playback Volume", 0x05, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Front Playback Switch", 0x05, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Headphone Playback Volume", 0x06, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Headphone Playback Switch", 0x06, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("Mono Playback Volume", 0x07, 1, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE_MONO("Mono Playback Switch", 0x07, 1, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("PCM Playback Volume", 0x11, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("PCM Playback Switch", 0x11, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Front Mic Playback Volume", 0x12, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Front Mic Playback Switch", 0x12, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Line Playback Volume", 0x13, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Line Playback Switch", 0x13, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Aux Playback Volume", 0x1b, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Aux Playback Switch", 0x1b, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x1c, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Mic Playback Switch", 0x1c, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("CD Playback Volume", 0x1d, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("CD Playback Switch", 0x1d, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Front Mic Boost", 0x08, 0x0, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Boost", 0x18, 0x0, HDA_INPUT),
	HDA_CODEC_VOLUME("Capture Volume", 0x15, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Capture Switch", 0x15, 0x0, HDA_OUTPUT),
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Capture Source",
		.info = ad198x_mux_enum_info,
		.get = ad198x_mux_enum_get,
		.put = ad198x_mux_enum_put,
	},
	/* identical with AD1983 */
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = SNDRV_CTL_NAME_IEC958("",PLAYBACK,NONE) "Source",
		.info = ad1983_spdif_route_info,
		.get = ad1983_spdif_route_get,
		.put = ad1983_spdif_route_put,
	},
	{ } /* end */
};

static struct hda_verb ad1981_init_verbs[] = {
	/* Front, HP, Mono; mute as default */
	{0x05, AC_VERB_SET_AMP_GAIN_MUTE, 0xb080},
	{0x06, AC_VERB_SET_AMP_GAIN_MUTE, 0xb080},
	{0x07, AC_VERB_SET_AMP_GAIN_MUTE, 0xb080},
	/* Beep, PCM, Front Mic, Line, Rear Mic, Aux, CD-In: mute */
	{0x0d, AC_VERB_SET_AMP_GAIN_MUTE, 0xb080},
	{0x11, AC_VERB_SET_AMP_GAIN_MUTE, 0xb080},
	{0x12, AC_VERB_SET_AMP_GAIN_MUTE, 0xb080},
	{0x13, AC_VERB_SET_AMP_GAIN_MUTE, 0xb080},
	{0x1b, AC_VERB_SET_AMP_GAIN_MUTE, 0xb080},
	{0x1c, AC_VERB_SET_AMP_GAIN_MUTE, 0xb080},
	{0x1d, AC_VERB_SET_AMP_GAIN_MUTE, 0xb080},
	/* Front, HP selectors; from Mix */
	{0x05, AC_VERB_SET_CONNECT_SEL, 0x01},
	{0x06, AC_VERB_SET_CONNECT_SEL, 0x01},
	/* Mono selector; from Mix */
	{0x0b, AC_VERB_SET_CONNECT_SEL, 0x03},
	/* Mic Mixer; select Front Mic */
	{0x1e, AC_VERB_SET_AMP_GAIN_MUTE, 0xb000},
	{0x1f, AC_VERB_SET_AMP_GAIN_MUTE, 0xb080},
	/* Mic boost: 0dB */
	{0x08, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_UNMUTE(0)},
	{0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_UNMUTE(0)},
	/* Record selector: Front mic */
	{0x15, AC_VERB_SET_CONNECT_SEL, 0x0},
	{0x15, AC_VERB_SET_AMP_GAIN_MUTE, 0xb080},
	/* SPDIF route: PCM */
	{0x02, AC_VERB_SET_CONNECT_SEL, 0x0},
	/* Front Pin */
	{0x05, AC_VERB_SET_PIN_WIDGET_CONTROL, 0x40 },
	/* HP Pin */
	{0x06, AC_VERB_SET_PIN_WIDGET_CONTROL, 0xc0 },
	/* Mono Pin */
	{0x07, AC_VERB_SET_PIN_WIDGET_CONTROL, 0x40 },
	/* Front & Rear Mic Pins */
	{0x08, AC_VERB_SET_PIN_WIDGET_CONTROL, 0x24 },
	{0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, 0x24 },
	/* Line Pin */
	{0x09, AC_VERB_SET_PIN_WIDGET_CONTROL, 0x20 },
	/* Digital Beep */
	{0x0d, AC_VERB_SET_CONNECT_SEL, 0x00},
	/* Line-Out as Input: disabled */
	{0x1a, AC_VERB_SET_AMP_GAIN_MUTE, 0xb080},
	{ } /* end */
};

#ifdef CONFIG_SND_HDA_POWER_SAVE
static struct hda_amp_list ad1981_loopbacks[] = {
	{ 0x12, HDA_OUTPUT, 0 }, /* Front Mic */
	{ 0x13, HDA_OUTPUT, 0 }, /* Line */
	{ 0x1b, HDA_OUTPUT, 0 }, /* Aux */
	{ 0x1c, HDA_OUTPUT, 0 }, /* Mic */
	{ 0x1d, HDA_OUTPUT, 0 }, /* CD */
	{ } /* end */
};
#endif

/*
 * Patch for HP nx6320
 *
 * nx6320 uses EAPD in the reverse way - EAPD-on means the internal
 * speaker output enabled _and_ mute-LED off.
 */

#define AD1981_HP_EVENT		0x37
#define AD1981_MIC_EVENT	0x38

static struct hda_verb ad1981_hp_init_verbs[] = {
	{0x05, AC_VERB_SET_EAPD_BTLENABLE, 0x00 }, /* default off */
	/* pin sensing on HP and Mic jacks */
	{0x06, AC_VERB_SET_UNSOLICITED_ENABLE, AC_USRSP_EN | AD1981_HP_EVENT},
	{0x08, AC_VERB_SET_UNSOLICITED_ENABLE, AC_USRSP_EN | AD1981_MIC_EVENT},
	{}
};

/* turn on/off EAPD (+ mute HP) as a master switch */
static int ad1981_hp_master_sw_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct ad198x_spec *spec = codec->spec;

	if (! ad198x_eapd_put(kcontrol, ucontrol))
		return 0;
	/* change speaker pin appropriately */
	snd_hda_codec_write(codec, 0x05, 0,
			    AC_VERB_SET_PIN_WIDGET_CONTROL,
			    spec->cur_eapd ? PIN_OUT : 0);
	/* toggle HP mute appropriately */
	snd_hda_codec_amp_stereo(codec, 0x06, HDA_OUTPUT, 0,
				 HDA_AMP_MUTE,
				 spec->cur_eapd ? 0 : HDA_AMP_MUTE);
	return 1;
}

/* bind volumes of both NID 0x05 and 0x06 */
static struct hda_bind_ctls ad1981_hp_bind_master_vol = {
	.ops = &snd_hda_bind_vol,
	.values = {
		HDA_COMPOSE_AMP_VAL(0x05, 3, 0, HDA_OUTPUT),
		HDA_COMPOSE_AMP_VAL(0x06, 3, 0, HDA_OUTPUT),
		0
	},
};

/* mute internal speaker if HP is plugged */
static void ad1981_hp_automute(struct hda_codec *codec)
{
	unsigned int present;

	present = snd_hda_codec_read(codec, 0x06, 0,
				     AC_VERB_GET_PIN_SENSE, 0) & 0x80000000;
	snd_hda_codec_amp_stereo(codec, 0x05, HDA_OUTPUT, 0,
				 HDA_AMP_MUTE, present ? HDA_AMP_MUTE : 0);
}

/* toggle input of built-in and mic jack appropriately */
static void ad1981_hp_automic(struct hda_codec *codec)
{
	static struct hda_verb mic_jack_on[] = {
		{0x1f, AC_VERB_SET_AMP_GAIN_MUTE, 0xb080},
		{0x1e, AC_VERB_SET_AMP_GAIN_MUTE, 0xb000},
		{}
	};
	static struct hda_verb mic_jack_off[] = {
		{0x1e, AC_VERB_SET_AMP_GAIN_MUTE, 0xb080},
		{0x1f, AC_VERB_SET_AMP_GAIN_MUTE, 0xb000},
		{}
	};
	unsigned int present;

	present = snd_hda_codec_read(codec, 0x08, 0,
			    	 AC_VERB_GET_PIN_SENSE, 0) & 0x80000000;
	if (present)
		snd_hda_sequence_write(codec, mic_jack_on);
	else
		snd_hda_sequence_write(codec, mic_jack_off);
}

/* unsolicited event for HP jack sensing */
static void ad1981_hp_unsol_event(struct hda_codec *codec,
				  unsigned int res)
{
	res >>= 26;
	switch (res) {
	case AD1981_HP_EVENT:
		ad1981_hp_automute(codec);
		break;
	case AD1981_MIC_EVENT:
		ad1981_hp_automic(codec);
		break;
	}
}

static struct hda_input_mux ad1981_hp_capture_source = {
	.num_items = 3,
	.items = {
		{ "Mic", 0x0 },
		{ "Docking-Station", 0x1 },
		{ "Mix", 0x2 },
	},
};

static struct snd_kcontrol_new ad1981_hp_mixers[] = {
	HDA_BIND_VOL("Master Playback Volume", &ad1981_hp_bind_master_vol),
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Master Playback Switch",
		.info = ad198x_eapd_info,
		.get = ad198x_eapd_get,
		.put = ad1981_hp_master_sw_put,
		.private_value = 0x05,
	},
	HDA_CODEC_VOLUME("PCM Playback Volume", 0x11, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("PCM Playback Switch", 0x11, 0x0, HDA_OUTPUT),
#if 0
	/* FIXME: analog mic/line loopback doesn't work with my tests...
	 *        (although recording is OK)
	 */
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x12, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Mic Playback Switch", 0x12, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Docking-Station Playback Volume", 0x13, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Docking-Station Playback Switch", 0x13, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Internal Mic Playback Volume", 0x1c, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Internal Mic Playback Switch", 0x1c, 0x0, HDA_OUTPUT),
	/* FIXME: does this laptop have analog CD connection? */
	HDA_CODEC_VOLUME("CD Playback Volume", 0x1d, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("CD Playback Switch", 0x1d, 0x0, HDA_OUTPUT),
#endif
	HDA_CODEC_VOLUME("Mic Boost", 0x08, 0x0, HDA_INPUT),
	HDA_CODEC_VOLUME("Internal Mic Boost", 0x18, 0x0, HDA_INPUT),
	HDA_CODEC_VOLUME("Capture Volume", 0x15, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Capture Switch", 0x15, 0x0, HDA_OUTPUT),
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Capture Source",
		.info = ad198x_mux_enum_info,
		.get = ad198x_mux_enum_get,
		.put = ad198x_mux_enum_put,
	},
	{ } /* end */
};

/* initialize jack-sensing, too */
static int ad1981_hp_init(struct hda_codec *codec)
{
	ad198x_init(codec);
	ad1981_hp_automute(codec);
	ad1981_hp_automic(codec);
	return 0;
}

/* configuration for Toshiba Laptops */
static struct hda_verb ad1981_toshiba_init_verbs[] = {
	{0x05, AC_VERB_SET_EAPD_BTLENABLE, 0x01 }, /* default on */
	/* pin sensing on HP and Mic jacks */
	{0x06, AC_VERB_SET_UNSOLICITED_ENABLE, AC_USRSP_EN | AD1981_HP_EVENT},
	{0x08, AC_VERB_SET_UNSOLICITED_ENABLE, AC_USRSP_EN | AD1981_MIC_EVENT},
	{}
};

static struct snd_kcontrol_new ad1981_toshiba_mixers[] = {
	HDA_CODEC_VOLUME("Amp Volume", 0x1a, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Amp Switch", 0x1a, 0x0, HDA_OUTPUT),
	{ }
};

/* configuration for Lenovo Thinkpad T60 */
static struct snd_kcontrol_new ad1981_thinkpad_mixers[] = {
	HDA_CODEC_VOLUME("Master Playback Volume", 0x05, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Master Playback Switch", 0x05, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("PCM Playback Volume", 0x11, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("PCM Playback Switch", 0x11, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x12, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Mic Playback Switch", 0x12, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("CD Playback Volume", 0x1d, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("CD Playback Switch", 0x1d, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Mic Boost", 0x08, 0x0, HDA_INPUT),
	HDA_CODEC_VOLUME("Capture Volume", 0x15, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Capture Switch", 0x15, 0x0, HDA_OUTPUT),
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Capture Source",
		.info = ad198x_mux_enum_info,
		.get = ad198x_mux_enum_get,
		.put = ad198x_mux_enum_put,
	},
	/* identical with AD1983 */
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = SNDRV_CTL_NAME_IEC958("",PLAYBACK,NONE) "Source",
		.info = ad1983_spdif_route_info,
		.get = ad1983_spdif_route_get,
		.put = ad1983_spdif_route_put,
	},
	{ } /* end */
};

static struct hda_input_mux ad1981_thinkpad_capture_source = {
	.num_items = 3,
	.items = {
		{ "Mic", 0x0 },
		{ "Mix", 0x2 },
		{ "CD", 0x4 },
	},
};

/* models */
enum {
	AD1981_BASIC,
	AD1981_HP,
	AD1981_THINKPAD,
	AD1981_TOSHIBA,
	AD1981_MODELS
};

static const char *ad1981_models[AD1981_MODELS] = {
	[AD1981_HP]		= "hp",
	[AD1981_THINKPAD]	= "thinkpad",
	[AD1981_BASIC]		= "basic",
	[AD1981_TOSHIBA]	= "toshiba"
};

static struct snd_pci_quirk ad1981_cfg_tbl[] = {
	SND_PCI_QUIRK(0x1014, 0x0597, "Lenovo Z60", AD1981_THINKPAD),
	SND_PCI_QUIRK(0x1014, 0x05b7, "Lenovo Z60m", AD1981_THINKPAD),
	/* All HP models */
	SND_PCI_QUIRK_VENDOR(0x103c, "HP nx", AD1981_HP),
	SND_PCI_QUIRK(0x1179, 0x0001, "Toshiba U205", AD1981_TOSHIBA),
	/* Lenovo Thinkpad T60/X60/Z6xx */
	SND_PCI_QUIRK_VENDOR(0x17aa, "Lenovo Thinkpad", AD1981_THINKPAD),
	/* HP nx6320 (reversed SSID, H/W bug) */
	SND_PCI_QUIRK(0x30b0, 0x103c, "HP nx6320", AD1981_HP),
	{}
};

static int patch_ad1981(struct hda_codec *codec)
{
	struct ad198x_spec *spec;
	int err, board_config;

	spec = kzalloc(sizeof(*spec), GFP_KERNEL);
	if (spec == NULL)
		return -ENOMEM;

	codec->spec = spec;

	err = snd_hda_attach_beep_device(codec, 0x10);
	if (err < 0) {
		ad198x_free(codec);
		return err;
	}
	set_beep_amp(spec, 0x0d, 0, HDA_OUTPUT);

	spec->multiout.max_channels = 2;
	spec->multiout.num_dacs = ARRAY_SIZE(ad1981_dac_nids);
	spec->multiout.dac_nids = ad1981_dac_nids;
	spec->multiout.dig_out_nid = AD1981_SPDIF_OUT;
	spec->num_adc_nids = 1;
	spec->adc_nids = ad1981_adc_nids;
	spec->capsrc_nids = ad1981_capsrc_nids;
	spec->input_mux = &ad1981_capture_source;
	spec->num_mixers = 1;
	spec->mixers[0] = ad1981_mixers;
	spec->num_init_verbs = 1;
	spec->init_verbs[0] = ad1981_init_verbs;
	spec->spdif_route = 0;
#ifdef CONFIG_SND_HDA_POWER_SAVE
	spec->loopback.amplist = ad1981_loopbacks;
#endif
	spec->vmaster_nid = 0x05;

	codec->patch_ops = ad198x_patch_ops;

	/* override some parameters */
	board_config = snd_hda_check_board_config(codec, AD1981_MODELS,
						  ad1981_models,
						  ad1981_cfg_tbl);
	switch (board_config) {
	case AD1981_HP:
		spec->mixers[0] = ad1981_hp_mixers;
		spec->num_init_verbs = 2;
		spec->init_verbs[1] = ad1981_hp_init_verbs;
		spec->multiout.dig_out_nid = 0;
		spec->input_mux = &ad1981_hp_capture_source;

		codec->patch_ops.init = ad1981_hp_init;
		codec->patch_ops.unsol_event = ad1981_hp_unsol_event;
		break;
	case AD1981_THINKPAD:
		spec->mixers[0] = ad1981_thinkpad_mixers;
		spec->input_mux = &ad1981_thinkpad_capture_source;
		break;
	case AD1981_TOSHIBA:
		spec->mixers[0] = ad1981_hp_mixers;
		spec->mixers[1] = ad1981_toshiba_mixers;
		spec->num_init_verbs = 2;
		spec->init_verbs[1] = ad1981_toshiba_init_verbs;
		spec->multiout.dig_out_nid = 0;
		spec->input_mux = &ad1981_hp_capture_source;
		codec->patch_ops.init = ad1981_hp_init;
		codec->patch_ops.unsol_event = ad1981_hp_unsol_event;
		break;
	}
	return 0;
}


/*
 * AD1988
 *
 * Output pins and routes
 *
 *        Pin               Mix     Sel     DAC (*)
 * port-A 0x11 (mute/hp) <- 0x22 <- 0x37 <- 03/04/06
 * port-B 0x14 (mute/hp) <- 0x2b <- 0x30 <- 03/04/06
 * port-C 0x15 (mute)    <- 0x2c <- 0x31 <- 05/0a
 * port-D 0x12 (mute/hp) <- 0x29         <- 04
 * port-E 0x17 (mute/hp) <- 0x26 <- 0x32 <- 05/0a
 * port-F 0x16 (mute)    <- 0x2a         <- 06
 * port-G 0x24 (mute)    <- 0x27         <- 05
 * port-H 0x25 (mute)    <- 0x28         <- 0a
 * mono   0x13 (mute/amp)<- 0x1e <- 0x36 <- 03/04/06
 *
 * DAC0 = 03h, DAC1 = 04h, DAC2 = 05h, DAC3 = 06h, DAC4 = 0ah
 * (*) DAC2/3/4 are swapped to DAC3/4/2 on AD198A rev.2 due to a h/w bug.
 *
 * Input pins and routes
 *
 *        pin     boost   mix input # / adc input #
 * port-A 0x11 -> 0x38 -> mix 2, ADC 0
 * port-B 0x14 -> 0x39 -> mix 0, ADC 1
 * port-C 0x15 -> 0x3a -> 33:0 - mix 1, ADC 2
 * port-D 0x12 -> 0x3d -> mix 3, ADC 8
 * port-E 0x17 -> 0x3c -> 34:0 - mix 4, ADC 4
 * port-F 0x16 -> 0x3b -> mix 5, ADC 3
 * port-G 0x24 -> N/A  -> 33:1 - mix 1, 34:1 - mix 4, ADC 6
 * port-H 0x25 -> N/A  -> 33:2 - mix 1, 34:2 - mix 4, ADC 7
 *
 *
 * DAC assignment
 *   6stack - front/surr/CLFE/side/opt DACs - 04/06/05/0a/03
 *   3stack - front/surr/CLFE/opt DACs - 04/05/0a/03
 *
 * Inputs of Analog Mix (0x20)
 *   0:Port-B (front mic)
 *   1:Port-C/G/H (line-in)
 *   2:Port-A
 *   3:Port-D (line-in/2)
 *   4:Port-E/G/H (mic-in)
 *   5:Port-F (mic2-in)
 *   6:CD
 *   7:Beep
 *
 * ADC selection
 *   0:Port-A
 *   1:Port-B (front mic-in)
 *   2:Port-C (line-in)
 *   3:Port-F (mic2-in)
 *   4:Port-E (mic-in)
 *   5:CD
 *   6:Port-G
 *   7:Port-H
 *   8:Port-D (line-in/2)
 *   9:Mix
 *
 * Proposed pin assignments by the datasheet
 *
 * 6-stack
 * Port-A front headphone
 *      B front mic-in
 *      C rear line-in
 *      D rear front-out
 *      E rear mic-in
 *      F rear surround
 *      G rear CLFE
 *      H rear side
 *
 * 3-stack
 * Port-A front headphone
 *      B front mic
 *      C rear line-in/surround
 *      D rear front-out
 *      E rear mic-in/CLFE
 *
 * laptop
 * Port-A headphone
 *      B mic-in
 *      C docking station
 *      D internal speaker (with EAPD)
 *      E/F quad mic array
 */


/* models */
enum {
	AD1988_6STACK,
	AD1988_6STACK_DIG,
	AD1988_3STACK,
	AD1988_3STACK_DIG,
	AD1988_LAPTOP,
	AD1988_LAPTOP_DIG,
	AD1988_AUTO,
	AD1988_MODEL_LAST,
};

/* reivision id to check workarounds */
#define AD1988A_REV2		0x100200

#define is_rev2(codec) \
	((codec)->vendor_id == 0x11d41988 && \
	 (codec)->revision_id == AD1988A_REV2)

/*
 * mixers
 */

static hda_nid_t ad1988_6stack_dac_nids[4] = {
	0x04, 0x06, 0x05, 0x0a
};

static hda_nid_t ad1988_3stack_dac_nids[3] = {
	0x04, 0x05, 0x0a
};

/* for AD1988A revision-2, DAC2-4 are swapped */
static hda_nid_t ad1988_6stack_dac_nids_rev2[4] = {
	0x04, 0x05, 0x0a, 0x06
};

static hda_nid_t ad1988_3stack_dac_nids_rev2[3] = {
	0x04, 0x0a, 0x06
};

static hda_nid_t ad1988_adc_nids[3] = {
	0x08, 0x09, 0x0f
};

static hda_nid_t ad1988_capsrc_nids[3] = {
	0x0c, 0x0d, 0x0e
};

#define AD1988_SPDIF_OUT		0x02
#define AD1988_SPDIF_OUT_HDMI	0x0b
#define AD1988_SPDIF_IN		0x07

static hda_nid_t ad1989b_slave_dig_outs[] = {
	AD1988_SPDIF_OUT, AD1988_SPDIF_OUT_HDMI, 0
};

static struct hda_input_mux ad1988_6stack_capture_source = {
	.num_items = 5,
	.items = {
		{ "Front Mic", 0x1 },	/* port-B */
		{ "Line", 0x2 },	/* port-C */
		{ "Mic", 0x4 },		/* port-E */
		{ "CD", 0x5 },
		{ "Mix", 0x9 },
	},
};

static struct hda_input_mux ad1988_laptop_capture_source = {
	.num_items = 3,
	.items = {
		{ "Mic/Line", 0x1 },	/* port-B */
		{ "CD", 0x5 },
		{ "Mix", 0x9 },
	},
};

/*
 */
static int ad198x_ch_mode_info(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_info *uinfo)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct ad198x_spec *spec = codec->spec;
	return snd_hda_ch_mode_info(codec, uinfo, spec->channel_mode,
				    spec->num_channel_mode);
}

static int ad198x_ch_mode_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct ad198x_spec *spec = codec->spec;
	return snd_hda_ch_mode_get(codec, ucontrol, spec->channel_mode,
				   spec->num_channel_mode, spec->multiout.max_channels);
}

static int ad198x_ch_mode_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct ad198x_spec *spec = codec->spec;
	int err = snd_hda_ch_mode_put(codec, ucontrol, spec->channel_mode,
				      spec->num_channel_mode,
				      &spec->multiout.max_channels);
	if (err >= 0 && spec->need_dac_fix)
		spec->multiout.num_dacs = spec->multiout.max_channels / 2;
	return err;
}

/* 6-stack mode */
static struct snd_kcontrol_new ad1988_6stack_mixers1[] = {
	HDA_CODEC_VOLUME("Front Playback Volume", 0x04, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Surround Playback Volume", 0x06, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("Center Playback Volume", 0x05, 1, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("LFE Playback Volume", 0x05, 2, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Side Playback Volume", 0x0a, 0x0, HDA_OUTPUT),
	{ } /* end */
};

static struct snd_kcontrol_new ad1988_6stack_mixers1_rev2[] = {
	HDA_CODEC_VOLUME("Front Playback Volume", 0x04, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Surround Playback Volume", 0x05, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("Center Playback Volume", 0x0a, 1, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("LFE Playback Volume", 0x0a, 2, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Side Playback Volume", 0x06, 0x0, HDA_OUTPUT),
	{ } /* end */
};

static struct snd_kcontrol_new ad1988_6stack_mixers2[] = {
	HDA_BIND_MUTE("Front Playback Switch", 0x29, 2, HDA_INPUT),
	HDA_BIND_MUTE("Surround Playback Switch", 0x2a, 2, HDA_INPUT),
	HDA_BIND_MUTE_MONO("Center Playback Switch", 0x27, 1, 2, HDA_INPUT),
	HDA_BIND_MUTE_MONO("LFE Playback Switch", 0x27, 2, 2, HDA_INPUT),
	HDA_BIND_MUTE("Side Playback Switch", 0x28, 2, HDA_INPUT),
	HDA_BIND_MUTE("Headphone Playback Switch", 0x22, 2, HDA_INPUT),
	HDA_BIND_MUTE("Mono Playback Switch", 0x1e, 2, HDA_INPUT),

	HDA_CODEC_VOLUME("CD Playback Volume", 0x20, 0x6, HDA_INPUT),
	HDA_CODEC_MUTE("CD Playback Switch", 0x20, 0x6, HDA_INPUT),
	HDA_CODEC_VOLUME("Front Mic Playback Volume", 0x20, 0x0, HDA_INPUT),
	HDA_CODEC_MUTE("Front Mic Playback Switch", 0x20, 0x0, HDA_INPUT),
	HDA_CODEC_VOLUME("Line Playback Volume", 0x20, 0x1, HDA_INPUT),
	HDA_CODEC_MUTE("Line Playback Switch", 0x20, 0x1, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x20, 0x4, HDA_INPUT),
	HDA_CODEC_MUTE("Mic Playback Switch", 0x20, 0x4, HDA_INPUT),

	HDA_CODEC_VOLUME("Analog Mix Playback Volume", 0x21, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Analog Mix Playback Switch", 0x21, 0x0, HDA_OUTPUT),

	HDA_CODEC_VOLUME("Front Mic Boost", 0x39, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Mic Boost", 0x3c, 0x0, HDA_OUTPUT),

	{ } /* end */
};

/* 3-stack mode */
static struct snd_kcontrol_new ad1988_3stack_mixers1[] = {
	HDA_CODEC_VOLUME("Front Playback Volume", 0x04, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Surround Playback Volume", 0x0a, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("Center Playback Volume", 0x05, 1, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("LFE Playback Volume", 0x05, 2, 0x0, HDA_OUTPUT),
	{ } /* end */
};

static struct snd_kcontrol_new ad1988_3stack_mixers1_rev2[] = {
	HDA_CODEC_VOLUME("Front Playback Volume", 0x04, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Surround Playback Volume", 0x0a, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("Center Playback Volume", 0x06, 1, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("LFE Playback Volume", 0x06, 2, 0x0, HDA_OUTPUT),
	{ } /* end */
};

static struct snd_kcontrol_new ad1988_3stack_mixers2[] = {
	HDA_BIND_MUTE("Front Playback Switch", 0x29, 2, HDA_INPUT),
	HDA_BIND_MUTE("Surround Playback Switch", 0x2c, 2, HDA_INPUT),
	HDA_BIND_MUTE_MONO("Center Playback Switch", 0x26, 1, 2, HDA_INPUT),
	HDA_BIND_MUTE_MONO("LFE Playback Switch", 0x26, 2, 2, HDA_INPUT),
	HDA_BIND_MUTE("Headphone Playback Switch", 0x22, 2, HDA_INPUT),
	HDA_BIND_MUTE("Mono Playback Switch", 0x1e, 2, HDA_INPUT),

	HDA_CODEC_VOLUME("CD Playback Volume", 0x20, 0x6, HDA_INPUT),
	HDA_CODEC_MUTE("CD Playback Switch", 0x20, 0x6, HDA_INPUT),
	HDA_CODEC_VOLUME("Front Mic Playback Volume", 0x20, 0x0, HDA_INPUT),
	HDA_CODEC_MUTE("Front Mic Playback Switch", 0x20, 0x0, HDA_INPUT),
	HDA_CODEC_VOLUME("Line Playback Volume", 0x20, 0x1, HDA_INPUT),
	HDA_CODEC_MUTE("Line Playback Switch", 0x20, 0x1, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x20, 0x4, HDA_INPUT),
	HDA_CODEC_MUTE("Mic Playback Switch", 0x20, 0x4, HDA_INPUT),

	HDA_CODEC_VOLUME("Analog Mix Playback Volume", 0x21, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Analog Mix Playback Switch", 0x21, 0x0, HDA_OUTPUT),

	HDA_CODEC_VOLUME("Front Mic Boost", 0x39, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Mic Boost", 0x3c, 0x0, HDA_OUTPUT),
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Channel Mode",
		.info = ad198x_ch_mode_info,
		.get = ad198x_ch_mode_get,
		.put = ad198x_ch_mode_put,
	},

	{ } /* end */
};

/* laptop mode */
static struct snd_kcontrol_new ad1988_laptop_mixers[] = {
	HDA_CODEC_VOLUME("PCM Playback Volume", 0x04, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("PCM Playback Switch", 0x29, 0x0, HDA_INPUT),
	HDA_BIND_MUTE("Mono Playback Switch", 0x1e, 2, HDA_INPUT),

	HDA_CODEC_VOLUME("CD Playback Volume", 0x20, 0x6, HDA_INPUT),
	HDA_CODEC_MUTE("CD Playback Switch", 0x20, 0x6, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x20, 0x0, HDA_INPUT),
	HDA_CODEC_MUTE("Mic Playback Switch", 0x20, 0x0, HDA_INPUT),
	HDA_CODEC_VOLUME("Line Playback Volume", 0x20, 0x1, HDA_INPUT),
	HDA_CODEC_MUTE("Line Playback Switch", 0x20, 0x1, HDA_INPUT),

	HDA_CODEC_VOLUME("Analog Mix Playback Volume", 0x21, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Analog Mix Playback Switch", 0x21, 0x0, HDA_OUTPUT),

	HDA_CODEC_VOLUME("Mic Boost", 0x39, 0x0, HDA_OUTPUT),

	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "External Amplifier",
		.info = ad198x_eapd_info,
		.get = ad198x_eapd_get,
		.put = ad198x_eapd_put,
		.private_value = 0x12 | (1 << 8), /* port-D, inversed */
	},

	{ } /* end */
};

/* capture */
static struct snd_kcontrol_new ad1988_capture_mixers[] = {
	HDA_CODEC_VOLUME("Capture Volume", 0x0c, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Capture Switch", 0x0c, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME_IDX("Capture Volume", 1, 0x0d, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE_IDX("Capture Switch", 1, 0x0d, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME_IDX("Capture Volume", 2, 0x0e, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE_IDX("Capture Switch", 2, 0x0e, 0x0, HDA_OUTPUT),
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		/* The multiple "Capture Source" controls confuse alsamixer
		 * So call somewhat different..
		 */
		/* .name = "Capture Source", */
		.name = "Input Source",
		.count = 3,
		.info = ad198x_mux_enum_info,
		.get = ad198x_mux_enum_get,
		.put = ad198x_mux_enum_put,
	},
	{ } /* end */
};

static int ad1988_spdif_playback_source_info(struct snd_kcontrol *kcontrol,
					     struct snd_ctl_elem_info *uinfo)
{
	static char *texts[] = {
		"PCM", "ADC1", "ADC2", "ADC3"
	};
	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = 4;
	if (uinfo->value.enumerated.item >= 4)
		uinfo->value.enumerated.item = 3;
	strcpy(uinfo->value.enumerated.name, texts[uinfo->value.enumerated.item]);
	return 0;
}

static int ad1988_spdif_playback_source_get(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	unsigned int sel;

	sel = snd_hda_codec_read(codec, 0x1d, 0, AC_VERB_GET_AMP_GAIN_MUTE,
				 AC_AMP_GET_INPUT);
	if (!(sel & 0x80))
		ucontrol->value.enumerated.item[0] = 0;
	else {
		sel = snd_hda_codec_read(codec, 0x0b, 0,
					 AC_VERB_GET_CONNECT_SEL, 0);
		if (sel < 3)
			sel++;
		else
			sel = 0;
		ucontrol->value.enumerated.item[0] = sel;
	}
	return 0;
}

static int ad1988_spdif_playback_source_put(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	unsigned int val, sel;
	int change;

	val = ucontrol->value.enumerated.item[0];
	if (val > 3)
		return -EINVAL;
	if (!val) {
		sel = snd_hda_codec_read(codec, 0x1d, 0,
					 AC_VERB_GET_AMP_GAIN_MUTE,
					 AC_AMP_GET_INPUT);
		change = sel & 0x80;
		if (change) {
			snd_hda_codec_write_cache(codec, 0x1d, 0,
						  AC_VERB_SET_AMP_GAIN_MUTE,
						  AMP_IN_UNMUTE(0));
			snd_hda_codec_write_cache(codec, 0x1d, 0,
						  AC_VERB_SET_AMP_GAIN_MUTE,
						  AMP_IN_MUTE(1));
		}
	} else {
		sel = snd_hda_codec_read(codec, 0x1d, 0,
					 AC_VERB_GET_AMP_GAIN_MUTE,
					 AC_AMP_GET_INPUT | 0x01);
		change = sel & 0x80;
		if (change) {
			snd_hda_codec_write_cache(codec, 0x1d, 0,
						  AC_VERB_SET_AMP_GAIN_MUTE,
						  AMP_IN_MUTE(0));
			snd_hda_codec_write_cache(codec, 0x1d, 0,
						  AC_VERB_SET_AMP_GAIN_MUTE,
						  AMP_IN_UNMUTE(1));
		}
		sel = snd_hda_codec_read(codec, 0x0b, 0,
					 AC_VERB_GET_CONNECT_SEL, 0) + 1;
		change |= sel != val;
		if (change)
			snd_hda_codec_write_cache(codec, 0x0b, 0,
						  AC_VERB_SET_CONNECT_SEL,
						  val - 1);
	}
	return change;
}

static struct snd_kcontrol_new ad1988_spdif_out_mixers[] = {
	HDA_CODEC_VOLUME("IEC958 Playback Volume", 0x1b, 0x0, HDA_OUTPUT),
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "IEC958 Playback Source",
		.info = ad1988_spdif_playback_source_info,
		.get = ad1988_spdif_playback_source_get,
		.put = ad1988_spdif_playback_source_put,
	},
	{ } /* end */
};

static struct snd_kcontrol_new ad1988_spdif_in_mixers[] = {
	HDA_CODEC_VOLUME("IEC958 Capture Volume", 0x1c, 0x0, HDA_INPUT),
	{ } /* end */
};

static struct snd_kcontrol_new ad1989_spdif_out_mixers[] = {
	HDA_CODEC_VOLUME("IEC958 Playback Volume", 0x1b, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("HDMI Playback Volume", 0x1d, 0x0, HDA_OUTPUT),
	{ } /* end */
};

/*
 * initialization verbs
 */

/*
 * for 6-stack (+dig)
 */
static struct hda_verb ad1988_6stack_init_verbs[] = {
	/* Front, Surround, CLFE, side DAC; unmute as default */
	{0x04, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x06, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x05, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x0a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	/* Port-A front headphon path */
	{0x37, AC_VERB_SET_CONNECT_SEL, 0x01}, /* DAC1:04h */
	{0x22, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(0)},
	{0x22, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(1)},
	{0x11, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x11, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	/* Port-D line-out path */
	{0x29, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(0)},
	{0x29, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(1)},
	{0x12, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x12, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	/* Port-F surround path */
	{0x2a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(0)},
	{0x2a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(1)},
	{0x16, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x16, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	/* Port-G CLFE path */
	{0x27, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(0)},
	{0x27, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(1)},
	{0x24, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x24, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	/* Port-H side path */
	{0x28, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(0)},
	{0x28, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(1)},
	{0x25, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x25, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	/* Mono out path */
	{0x36, AC_VERB_SET_CONNECT_SEL, 0x1}, /* DAC1:04h */
	{0x1e, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(0)},
	{0x1e, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(1)},
	{0x13, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x13, AC_VERB_SET_AMP_GAIN_MUTE, 0xb01f}, /* unmute, 0dB */
	/* Port-B front mic-in path */
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x39, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_ZERO},
	/* Port-C line-in path */
	{0x15, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	{0x15, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},
	{0x3a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_ZERO},
	{0x33, AC_VERB_SET_CONNECT_SEL, 0x0},
	/* Port-E mic-in path */
	{0x17, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	{0x17, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x3c, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_ZERO},
	{0x34, AC_VERB_SET_CONNECT_SEL, 0x0},
	/* Analog CD Input */
	{0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},
	/* Analog Mix output amp */
	{0x21, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE | 0x1f}, /* 0dB */

	{ }
};

static struct hda_verb ad1988_capture_init_verbs[] = {
	/* mute analog mix */
	{0x20, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(0)},
	{0x20, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(1)},
	{0x20, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(2)},
	{0x20, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(3)},
	{0x20, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(4)},
	{0x20, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(5)},
	{0x20, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(6)},
	{0x20, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(7)},
	/* select ADCs - front-mic */
	{0x0c, AC_VERB_SET_CONNECT_SEL, 0x1},
	{0x0d, AC_VERB_SET_CONNECT_SEL, 0x1},
	{0x0e, AC_VERB_SET_CONNECT_SEL, 0x1},

	{ }
};

static struct hda_verb ad1988_spdif_init_verbs[] = {
	/* SPDIF out sel */
	{0x02, AC_VERB_SET_CONNECT_SEL, 0x0}, /* PCM */
	{0x0b, AC_VERB_SET_CONNECT_SEL, 0x0}, /* ADC1 */
	{0x1d, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_UNMUTE(0)},
	{0x1d, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(1)},
	/* SPDIF out pin */
	{0x1b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE | 0x27}, /* 0dB */

	{ }
};

/* AD1989 has no ADC -> SPDIF route */
static struct hda_verb ad1989_spdif_init_verbs[] = {
	/* SPDIF-1 out pin */
	{0x1b, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{0x1b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE | 0x27}, /* 0dB */
	/* SPDIF-2/HDMI out pin */
	{0x1d, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{0x1d, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE | 0x27}, /* 0dB */
	{ }
};

/*
 * verbs for 3stack (+dig)
 */
static struct hda_verb ad1988_3stack_ch2_init[] = {
	/* set port-C to line-in */
	{ 0x15, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE },
	{ 0x15, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN },
	/* set port-E to mic-in */
	{ 0x17, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE },
	{ 0x17, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80 },
	{ } /* end */
};

static struct hda_verb ad1988_3stack_ch6_init[] = {
	/* set port-C to surround out */
	{ 0x15, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x15, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE },
	/* set port-E to CLFE out */
	{ 0x17, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x17, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE },
	{ } /* end */
};

static struct hda_channel_mode ad1988_3stack_modes[2] = {
	{ 2, ad1988_3stack_ch2_init },
	{ 6, ad1988_3stack_ch6_init },
};

static struct hda_verb ad1988_3stack_init_verbs[] = {
	/* Front, Surround, CLFE, side DAC; unmute as default */
	{0x04, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x06, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x05, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x0a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	/* Port-A front headphon path */
	{0x37, AC_VERB_SET_CONNECT_SEL, 0x01}, /* DAC1:04h */
	{0x22, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(0)},
	{0x22, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(1)},
	{0x11, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x11, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	/* Port-D line-out path */
	{0x29, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(0)},
	{0x29, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(1)},
	{0x12, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x12, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	/* Mono out path */
	{0x36, AC_VERB_SET_CONNECT_SEL, 0x1}, /* DAC1:04h */
	{0x1e, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(0)},
	{0x1e, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(1)},
	{0x13, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x13, AC_VERB_SET_AMP_GAIN_MUTE, 0xb01f}, /* unmute, 0dB */
	/* Port-B front mic-in path */
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x39, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_ZERO},
	/* Port-C line-in/surround path - 6ch mode as default */
	{0x15, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x15, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x3a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_ZERO},
	{0x31, AC_VERB_SET_CONNECT_SEL, 0x0}, /* output sel: DAC 0x05 */
	{0x33, AC_VERB_SET_CONNECT_SEL, 0x0},
	/* Port-E mic-in/CLFE path - 6ch mode as default */
	{0x17, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x17, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x3c, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_ZERO},
	{0x32, AC_VERB_SET_CONNECT_SEL, 0x1}, /* output sel: DAC 0x0a */
	{0x34, AC_VERB_SET_CONNECT_SEL, 0x0},
	/* mute analog mix */
	{0x20, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(0)},
	{0x20, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(1)},
	{0x20, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(2)},
	{0x20, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(3)},
	{0x20, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(4)},
	{0x20, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(5)},
	{0x20, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(6)},
	{0x20, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(7)},
	/* select ADCs - front-mic */
	{0x0c, AC_VERB_SET_CONNECT_SEL, 0x1},
	{0x0d, AC_VERB_SET_CONNECT_SEL, 0x1},
	{0x0e, AC_VERB_SET_CONNECT_SEL, 0x1},
	/* Analog Mix output amp */
	{0x21, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE | 0x1f}, /* 0dB */
	{ }
};

/*
 * verbs for laptop mode (+dig)
 */
static struct hda_verb ad1988_laptop_hp_on[] = {
	/* unmute port-A and mute port-D */
	{ 0x11, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE },
	{ 0x12, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE },
	{ } /* end */
};
static struct hda_verb ad1988_laptop_hp_off[] = {
	/* mute port-A and unmute port-D */
	{ 0x11, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE },
	{ 0x12, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE },
	{ } /* end */
};

#define AD1988_HP_EVENT	0x01

static struct hda_verb ad1988_laptop_init_verbs[] = {
	/* Front, Surround, CLFE, side DAC; unmute as default */
	{0x04, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x06, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x05, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x0a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	/* Port-A front headphon path */
	{0x37, AC_VERB_SET_CONNECT_SEL, 0x01}, /* DAC1:04h */
	{0x22, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(0)},
	{0x22, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(1)},
	{0x11, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x11, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	/* unsolicited event for pin-sense */
	{0x11, AC_VERB_SET_UNSOLICITED_ENABLE, AC_USRSP_EN | AD1988_HP_EVENT },
	/* Port-D line-out path + EAPD */
	{0x29, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(0)},
	{0x29, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(1)},
	{0x12, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x12, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x12, AC_VERB_SET_EAPD_BTLENABLE, 0x00}, /* EAPD-off */
	/* Mono out path */
	{0x36, AC_VERB_SET_CONNECT_SEL, 0x1}, /* DAC1:04h */
	{0x1e, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(0)},
	{0x1e, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(1)},
	{0x13, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x13, AC_VERB_SET_AMP_GAIN_MUTE, 0xb01f}, /* unmute, 0dB */
	/* Port-B mic-in path */
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x39, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_ZERO},
	/* Port-C docking station - try to output */
	{0x15, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x15, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x3a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_ZERO},
	{0x33, AC_VERB_SET_CONNECT_SEL, 0x0},
	/* mute analog mix */
	{0x20, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(0)},
	{0x20, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(1)},
	{0x20, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(2)},
	{0x20, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(3)},
	{0x20, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(4)},
	{0x20, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(5)},
	{0x20, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(6)},
	{0x20, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(7)},
	/* select ADCs - mic */
	{0x0c, AC_VERB_SET_CONNECT_SEL, 0x1},
	{0x0d, AC_VERB_SET_CONNECT_SEL, 0x1},
	{0x0e, AC_VERB_SET_CONNECT_SEL, 0x1},
	/* Analog Mix output amp */
	{0x21, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE | 0x1f}, /* 0dB */
	{ }
};

static void ad1988_laptop_unsol_event(struct hda_codec *codec, unsigned int res)
{
	if ((res >> 26) != AD1988_HP_EVENT)
		return;
	if (snd_hda_codec_read(codec, 0x11, 0, AC_VERB_GET_PIN_SENSE, 0) & (1 << 31))
		snd_hda_sequence_write(codec, ad1988_laptop_hp_on);
	else
		snd_hda_sequence_write(codec, ad1988_laptop_hp_off);
} 

#ifdef CONFIG_SND_HDA_POWER_SAVE
static struct hda_amp_list ad1988_loopbacks[] = {
	{ 0x20, HDA_INPUT, 0 }, /* Front Mic */
	{ 0x20, HDA_INPUT, 1 }, /* Line */
	{ 0x20, HDA_INPUT, 4 }, /* Mic */
	{ 0x20, HDA_INPUT, 6 }, /* CD */
	{ } /* end */
};
#endif

/*
 * Automatic parse of I/O pins from the BIOS configuration
 */

enum {
	AD_CTL_WIDGET_VOL,
	AD_CTL_WIDGET_MUTE,
	AD_CTL_BIND_MUTE,
};
static struct snd_kcontrol_new ad1988_control_templates[] = {
	HDA_CODEC_VOLUME(NULL, 0, 0, 0),
	HDA_CODEC_MUTE(NULL, 0, 0, 0),
	HDA_BIND_MUTE(NULL, 0, 0, 0),
};

/* add dynamic controls */
static int add_control(struct ad198x_spec *spec, int type, const char *name,
		       unsigned long val)
{
	struct snd_kcontrol_new *knew;

	snd_array_init(&spec->kctls, sizeof(*knew), 32);
	knew = snd_array_new(&spec->kctls);
	if (!knew)
		return -ENOMEM;
	*knew = ad1988_control_templates[type];
	knew->name = kstrdup(name, GFP_KERNEL);
	if (! knew->name)
		return -ENOMEM;
	knew->private_value = val;
	return 0;
}

#define AD1988_PIN_CD_NID		0x18
#define AD1988_PIN_BEEP_NID		0x10

static hda_nid_t ad1988_mixer_nids[8] = {
	/* A     B     C     D     E     F     G     H */
	0x22, 0x2b, 0x2c, 0x29, 0x26, 0x2a, 0x27, 0x28
};

static inline hda_nid_t ad1988_idx_to_dac(struct hda_codec *codec, int idx)
{
	static hda_nid_t idx_to_dac[8] = {
		/* A     B     C     D     E     F     G     H */
		0x04, 0x06, 0x05, 0x04, 0x0a, 0x06, 0x05, 0x0a
	};
	static hda_nid_t idx_to_dac_rev2[8] = {
		/* A     B     C     D     E     F     G     H */
		0x04, 0x05, 0x0a, 0x04, 0x06, 0x05, 0x0a, 0x06
	};
	if (is_rev2(codec))
		return idx_to_dac_rev2[idx];
	else
		return idx_to_dac[idx];
}

static hda_nid_t ad1988_boost_nids[8] = {
	0x38, 0x39, 0x3a, 0x3d, 0x3c, 0x3b, 0, 0
};

static int ad1988_pin_idx(hda_nid_t nid)
{
	static hda_nid_t ad1988_io_pins[8] = {
		0x11, 0x14, 0x15, 0x12, 0x17, 0x16, 0x24, 0x25
	};
	int i;
	for (i = 0; i < ARRAY_SIZE(ad1988_io_pins); i++)
		if (ad1988_io_pins[i] == nid)
			return i;
	return 0; /* should be -1 */
}

static int ad1988_pin_to_loopback_idx(hda_nid_t nid)
{
	static int loopback_idx[8] = {
		2, 0, 1, 3, 4, 5, 1, 4
	};
	switch (nid) {
	case AD1988_PIN_CD_NID:
		return 6;
	default:
		return loopback_idx[ad1988_pin_idx(nid)];
	}
}

static int ad1988_pin_to_adc_idx(hda_nid_t nid)
{
	static int adc_idx[8] = {
		0, 1, 2, 8, 4, 3, 6, 7
	};
	switch (nid) {
	case AD1988_PIN_CD_NID:
		return 5;
	default:
		return adc_idx[ad1988_pin_idx(nid)];
	}
}

/* fill in the dac_nids table from the parsed pin configuration */
static int ad1988_auto_fill_dac_nids(struct hda_codec *codec,
				     const struct auto_pin_cfg *cfg)
{
	struct ad198x_spec *spec = codec->spec;
	int i, idx;

	spec->multiout.dac_nids = spec->private_dac_nids;

	/* check the pins hardwired to audio widget */
	for (i = 0; i < cfg->line_outs; i++) {
		idx = ad1988_pin_idx(cfg->line_out_pins[i]);
		spec->multiout.dac_nids[i] = ad1988_idx_to_dac(codec, idx);
	}
	spec->multiout.num_dacs = cfg->line_outs;
	return 0;
}

/* add playback controls from the parsed DAC table */
static int ad1988_auto_create_multi_out_ctls(struct ad198x_spec *spec,
					     const struct auto_pin_cfg *cfg)
{
	char name[32];
	static const char *chname[4] = { "Front", "Surround", NULL /*CLFE*/, "Side" };
	hda_nid_t nid;
	int i, err;

	for (i = 0; i < cfg->line_outs; i++) {
		hda_nid_t dac = spec->multiout.dac_nids[i];
		if (! dac)
			continue;
		nid = ad1988_mixer_nids[ad1988_pin_idx(cfg->line_out_pins[i])];
		if (i == 2) {
			/* Center/LFE */
			err = add_control(spec, AD_CTL_WIDGET_VOL,
					  "Center Playback Volume",
					  HDA_COMPOSE_AMP_VAL(dac, 1, 0, HDA_OUTPUT));
			if (err < 0)
				return err;
			err = add_control(spec, AD_CTL_WIDGET_VOL,
					  "LFE Playback Volume",
					  HDA_COMPOSE_AMP_VAL(dac, 2, 0, HDA_OUTPUT));
			if (err < 0)
				return err;
			err = add_control(spec, AD_CTL_BIND_MUTE,
					  "Center Playback Switch",
					  HDA_COMPOSE_AMP_VAL(nid, 1, 2, HDA_INPUT));
			if (err < 0)
				return err;
			err = add_control(spec, AD_CTL_BIND_MUTE,
					  "LFE Playback Switch",
					  HDA_COMPOSE_AMP_VAL(nid, 2, 2, HDA_INPUT));
			if (err < 0)
				return err;
		} else {
			sprintf(name, "%s Playback Volume", chname[i]);
			err = add_control(spec, AD_CTL_WIDGET_VOL, name,
					  HDA_COMPOSE_AMP_VAL(dac, 3, 0, HDA_OUTPUT));
			if (err < 0)
				return err;
			sprintf(name, "%s Playback Switch", chname[i]);
			err = add_control(spec, AD_CTL_BIND_MUTE, name,
					  HDA_COMPOSE_AMP_VAL(nid, 3, 2, HDA_INPUT));
			if (err < 0)
				return err;
		}
	}
	return 0;
}

/* add playback controls for speaker and HP outputs */
static int ad1988_auto_create_extra_out(struct hda_codec *codec, hda_nid_t pin,
					const char *pfx)
{
	struct ad198x_spec *spec = codec->spec;
	hda_nid_t nid;
	int i, idx, err;
	char name[32];

	if (! pin)
		return 0;

	idx = ad1988_pin_idx(pin);
	nid = ad1988_idx_to_dac(codec, idx);
	/* check whether the corresponding DAC was already taken */
	for (i = 0; i < spec->autocfg.line_outs; i++) {
		hda_nid_t pin = spec->autocfg.line_out_pins[i];
		hda_nid_t dac = ad1988_idx_to_dac(codec, ad1988_pin_idx(pin));
		if (dac == nid)
			break;
	}
	if (i >= spec->autocfg.line_outs) {
		/* specify the DAC as the extra output */
		if (!spec->multiout.hp_nid)
			spec->multiout.hp_nid = nid;
		else
			spec->multiout.extra_out_nid[0] = nid;
		/* control HP volume/switch on the output mixer amp */
		sprintf(name, "%s Playback Volume", pfx);
		err = add_control(spec, AD_CTL_WIDGET_VOL, name,
				  HDA_COMPOSE_AMP_VAL(nid, 3, 0, HDA_OUTPUT));
		if (err < 0)
			return err;
	}
	nid = ad1988_mixer_nids[idx];
	sprintf(name, "%s Playback Switch", pfx);
	if ((err = add_control(spec, AD_CTL_BIND_MUTE, name,
			       HDA_COMPOSE_AMP_VAL(nid, 3, 2, HDA_INPUT))) < 0)
		return err;
	return 0;
}

/* create input playback/capture controls for the given pin */
static int new_analog_input(struct ad198x_spec *spec, hda_nid_t pin,
			    const char *ctlname, int boost)
{
	char name[32];
	int err, idx;

	sprintf(name, "%s Playback Volume", ctlname);
	idx = ad1988_pin_to_loopback_idx(pin);
	if ((err = add_control(spec, AD_CTL_WIDGET_VOL, name,
			       HDA_COMPOSE_AMP_VAL(0x20, 3, idx, HDA_INPUT))) < 0)
		return err;
	sprintf(name, "%s Playback Switch", ctlname);
	if ((err = add_control(spec, AD_CTL_WIDGET_MUTE, name,
			       HDA_COMPOSE_AMP_VAL(0x20, 3, idx, HDA_INPUT))) < 0)
		return err;
	if (boost) {
		hda_nid_t bnid;
		idx = ad1988_pin_idx(pin);
		bnid = ad1988_boost_nids[idx];
		if (bnid) {
			sprintf(name, "%s Boost", ctlname);
			return add_control(spec, AD_CTL_WIDGET_VOL, name,
					   HDA_COMPOSE_AMP_VAL(bnid, 3, idx, HDA_OUTPUT));

		}
	}
	return 0;
}

/* create playback/capture controls for input pins */
static int ad1988_auto_create_analog_input_ctls(struct ad198x_spec *spec,
						const struct auto_pin_cfg *cfg)
{
	struct hda_input_mux *imux = &spec->private_imux;
	int i, err;

	for (i = 0; i < AUTO_PIN_LAST; i++) {
		err = new_analog_input(spec, cfg->input_pins[i],
				       auto_pin_cfg_labels[i],
				       i <= AUTO_PIN_FRONT_MIC);
		if (err < 0)
			return err;
		imux->items[imux->num_items].label = auto_pin_cfg_labels[i];
		imux->items[imux->num_items].index = ad1988_pin_to_adc_idx(cfg->input_pins[i]);
		imux->num_items++;
	}
	imux->items[imux->num_items].label = "Mix";
	imux->items[imux->num_items].index = 9;
	imux->num_items++;

	if ((err = add_control(spec, AD_CTL_WIDGET_VOL,
			       "Analog Mix Playback Volume",
			       HDA_COMPOSE_AMP_VAL(0x21, 3, 0x0, HDA_OUTPUT))) < 0)
		return err;
	if ((err = add_control(spec, AD_CTL_WIDGET_MUTE,
			       "Analog Mix Playback Switch",
			       HDA_COMPOSE_AMP_VAL(0x21, 3, 0x0, HDA_OUTPUT))) < 0)
		return err;

	return 0;
}

static void ad1988_auto_set_output_and_unmute(struct hda_codec *codec,
					      hda_nid_t nid, int pin_type,
					      int dac_idx)
{
	/* set as output */
	snd_hda_codec_write(codec, nid, 0, AC_VERB_SET_PIN_WIDGET_CONTROL, pin_type);
	snd_hda_codec_write(codec, nid, 0, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE);
	switch (nid) {
	case 0x11: /* port-A - DAC 04 */
		snd_hda_codec_write(codec, 0x37, 0, AC_VERB_SET_CONNECT_SEL, 0x01);
		break;
	case 0x14: /* port-B - DAC 06 */
		snd_hda_codec_write(codec, 0x30, 0, AC_VERB_SET_CONNECT_SEL, 0x02);
		break;
	case 0x15: /* port-C - DAC 05 */
		snd_hda_codec_write(codec, 0x31, 0, AC_VERB_SET_CONNECT_SEL, 0x00);
		break;
	case 0x17: /* port-E - DAC 0a */
		snd_hda_codec_write(codec, 0x32, 0, AC_VERB_SET_CONNECT_SEL, 0x01);
		break;
	case 0x13: /* mono - DAC 04 */
		snd_hda_codec_write(codec, 0x36, 0, AC_VERB_SET_CONNECT_SEL, 0x01);
		break;
	}
}

static void ad1988_auto_init_multi_out(struct hda_codec *codec)
{
	struct ad198x_spec *spec = codec->spec;
	int i;

	for (i = 0; i < spec->autocfg.line_outs; i++) {
		hda_nid_t nid = spec->autocfg.line_out_pins[i];
		ad1988_auto_set_output_and_unmute(codec, nid, PIN_OUT, i);
	}
}

static void ad1988_auto_init_extra_out(struct hda_codec *codec)
{
	struct ad198x_spec *spec = codec->spec;
	hda_nid_t pin;

	pin = spec->autocfg.speaker_pins[0];
	if (pin) /* connect to front */
		ad1988_auto_set_output_and_unmute(codec, pin, PIN_OUT, 0);
	pin = spec->autocfg.hp_pins[0];
	if (pin) /* connect to front */
		ad1988_auto_set_output_and_unmute(codec, pin, PIN_HP, 0);
}

static void ad1988_auto_init_analog_input(struct hda_codec *codec)
{
	struct ad198x_spec *spec = codec->spec;
	int i, idx;

	for (i = 0; i < AUTO_PIN_LAST; i++) {
		hda_nid_t nid = spec->autocfg.input_pins[i];
		if (! nid)
			continue;
		switch (nid) {
		case 0x15: /* port-C */
			snd_hda_codec_write(codec, 0x33, 0, AC_VERB_SET_CONNECT_SEL, 0x0);
			break;
		case 0x17: /* port-E */
			snd_hda_codec_write(codec, 0x34, 0, AC_VERB_SET_CONNECT_SEL, 0x0);
			break;
		}
		snd_hda_codec_write(codec, nid, 0, AC_VERB_SET_PIN_WIDGET_CONTROL,
				    i <= AUTO_PIN_FRONT_MIC ? PIN_VREF80 : PIN_IN);
		if (nid != AD1988_PIN_CD_NID)
			snd_hda_codec_write(codec, nid, 0, AC_VERB_SET_AMP_GAIN_MUTE,
					    AMP_OUT_MUTE);
		idx = ad1988_pin_idx(nid);
		if (ad1988_boost_nids[idx])
			snd_hda_codec_write(codec, ad1988_boost_nids[idx], 0,
					    AC_VERB_SET_AMP_GAIN_MUTE,
					    AMP_OUT_ZERO);
	}
}

/* parse the BIOS configuration and set up the alc_spec */
/* return 1 if successful, 0 if the proper config is not found, or a negative error code */
static int ad1988_parse_auto_config(struct hda_codec *codec)
{
	struct ad198x_spec *spec = codec->spec;
	int err;

	if ((err = snd_hda_parse_pin_def_config(codec, &spec->autocfg, NULL)) < 0)
		return err;
	if ((err = ad1988_auto_fill_dac_nids(codec, &spec->autocfg)) < 0)
		return err;
	if (! spec->autocfg.line_outs)
		return 0; /* can't find valid BIOS pin config */
	if ((err = ad1988_auto_create_multi_out_ctls(spec, &spec->autocfg)) < 0 ||
	    (err = ad1988_auto_create_extra_out(codec,
						spec->autocfg.speaker_pins[0],
						"Speaker")) < 0 ||
	    (err = ad1988_auto_create_extra_out(codec, spec->autocfg.hp_pins[0],
						"Headphone")) < 0 ||
	    (err = ad1988_auto_create_analog_input_ctls(spec, &spec->autocfg)) < 0)
		return err;

	spec->multiout.max_channels = spec->multiout.num_dacs * 2;

	if (spec->autocfg.dig_outs)
		spec->multiout.dig_out_nid = AD1988_SPDIF_OUT;
	if (spec->autocfg.dig_in_pin)
		spec->dig_in_nid = AD1988_SPDIF_IN;

	if (spec->kctls.list)
		spec->mixers[spec->num_mixers++] = spec->kctls.list;

	spec->init_verbs[spec->num_init_verbs++] = ad1988_6stack_init_verbs;

	spec->input_mux = &spec->private_imux;

	return 1;
}

/* init callback for auto-configuration model -- overriding the default init */
static int ad1988_auto_init(struct hda_codec *codec)
{
	ad198x_init(codec);
	ad1988_auto_init_multi_out(codec);
	ad1988_auto_init_extra_out(codec);
	ad1988_auto_init_analog_input(codec);
	return 0;
}


/*
 */

static const char *ad1988_models[AD1988_MODEL_LAST] = {
	[AD1988_6STACK]		= "6stack",
	[AD1988_6STACK_DIG]	= "6stack-dig",
	[AD1988_3STACK]		= "3stack",
	[AD1988_3STACK_DIG]	= "3stack-dig",
	[AD1988_LAPTOP]		= "laptop",
	[AD1988_LAPTOP_DIG]	= "laptop-dig",
	[AD1988_AUTO]		= "auto",
};

static struct snd_pci_quirk ad1988_cfg_tbl[] = {
	SND_PCI_QUIRK(0x1043, 0x81ec, "Asus P5B-DLX", AD1988_6STACK_DIG),
	SND_PCI_QUIRK(0x1043, 0x81f6, "Asus M2N-SLI", AD1988_6STACK_DIG),
	SND_PCI_QUIRK(0x1043, 0x8277, "Asus P5K-E/WIFI-AP", AD1988_6STACK_DIG),
	SND_PCI_QUIRK(0x1043, 0x8311, "Asus P5Q-Premium/Pro", AD1988_6STACK_DIG),
	{}
};

static int patch_ad1988(struct hda_codec *codec)
{
	struct ad198x_spec *spec;
	int err, board_config;

	spec = kzalloc(sizeof(*spec), GFP_KERNEL);
	if (spec == NULL)
		return -ENOMEM;

	codec->spec = spec;

	if (is_rev2(codec))
		snd_printk(KERN_INFO "patch_analog: AD1988A rev.2 is detected, enable workarounds\n");

	board_config = snd_hda_check_board_config(codec, AD1988_MODEL_LAST,
						  ad1988_models, ad1988_cfg_tbl);
	if (board_config < 0) {
		printk(KERN_INFO "hda_codec: %s: BIOS auto-probing.\n",
		       codec->chip_name);
		board_config = AD1988_AUTO;
	}

	if (board_config == AD1988_AUTO) {
		/* automatic parse from the BIOS config */
		err = ad1988_parse_auto_config(codec);
		if (err < 0) {
			ad198x_free(codec);
			return err;
		} else if (! err) {
			printk(KERN_INFO "hda_codec: Cannot set up configuration from BIOS.  Using 6-stack mode...\n");
			board_config = AD1988_6STACK;
		}
	}

	err = snd_hda_attach_beep_device(codec, 0x10);
	if (err < 0) {
		ad198x_free(codec);
		return err;
	}
	set_beep_amp(spec, 0x10, 0, HDA_OUTPUT);

	switch (board_config) {
	case AD1988_6STACK:
	case AD1988_6STACK_DIG:
		spec->multiout.max_channels = 8;
		spec->multiout.num_dacs = 4;
		if (is_rev2(codec))
			spec->multiout.dac_nids = ad1988_6stack_dac_nids_rev2;
		else
			spec->multiout.dac_nids = ad1988_6stack_dac_nids;
		spec->input_mux = &ad1988_6stack_capture_source;
		spec->num_mixers = 2;
		if (is_rev2(codec))
			spec->mixers[0] = ad1988_6stack_mixers1_rev2;
		else
			spec->mixers[0] = ad1988_6stack_mixers1;
		spec->mixers[1] = ad1988_6stack_mixers2;
		spec->num_init_verbs = 1;
		spec->init_verbs[0] = ad1988_6stack_init_verbs;
		if (board_config == AD1988_6STACK_DIG) {
			spec->multiout.dig_out_nid = AD1988_SPDIF_OUT;
			spec->dig_in_nid = AD1988_SPDIF_IN;
		}
		break;
	case AD1988_3STACK:
	case AD1988_3STACK_DIG:
		spec->multiout.max_channels = 6;
		spec->multiout.num_dacs = 3;
		if (is_rev2(codec))
			spec->multiout.dac_nids = ad1988_3stack_dac_nids_rev2;
		else
			spec->multiout.dac_nids = ad1988_3stack_dac_nids;
		spec->input_mux = &ad1988_6stack_capture_source;
		spec->channel_mode = ad1988_3stack_modes;
		spec->num_channel_mode = ARRAY_SIZE(ad1988_3stack_modes);
		spec->num_mixers = 2;
		if (is_rev2(codec))
			spec->mixers[0] = ad1988_3stack_mixers1_rev2;
		else
			spec->mixers[0] = ad1988_3stack_mixers1;
		spec->mixers[1] = ad1988_3stack_mixers2;
		spec->num_init_verbs = 1;
		spec->init_verbs[0] = ad1988_3stack_init_verbs;
		if (board_config == AD1988_3STACK_DIG)
			spec->multiout.dig_out_nid = AD1988_SPDIF_OUT;
		break;
	case AD1988_LAPTOP:
	case AD1988_LAPTOP_DIG:
		spec->multiout.max_channels = 2;
		spec->multiout.num_dacs = 1;
		spec->multiout.dac_nids = ad1988_3stack_dac_nids;
		spec->input_mux = &ad1988_laptop_capture_source;
		spec->num_mixers = 1;
		spec->mixers[0] = ad1988_laptop_mixers;
		spec->num_init_verbs = 1;
		spec->init_verbs[0] = ad1988_laptop_init_verbs;
		if (board_config == AD1988_LAPTOP_DIG)
			spec->multiout.dig_out_nid = AD1988_SPDIF_OUT;
		break;
	}

	spec->num_adc_nids = ARRAY_SIZE(ad1988_adc_nids);
	spec->adc_nids = ad1988_adc_nids;
	spec->capsrc_nids = ad1988_capsrc_nids;
	spec->mixers[spec->num_mixers++] = ad1988_capture_mixers;
	spec->init_verbs[spec->num_init_verbs++] = ad1988_capture_init_verbs;
	if (spec->multiout.dig_out_nid) {
		if (codec->vendor_id >= 0x11d4989a) {
			spec->mixers[spec->num_mixers++] =
				ad1989_spdif_out_mixers;
			spec->init_verbs[spec->num_init_verbs++] =
				ad1989_spdif_init_verbs;
			codec->slave_dig_outs = ad1989b_slave_dig_outs;
		} else {
			spec->mixers[spec->num_mixers++] =
				ad1988_spdif_out_mixers;
			spec->init_verbs[spec->num_init_verbs++] =
				ad1988_spdif_init_verbs;
		}
	}
	if (spec->dig_in_nid && codec->vendor_id < 0x11d4989a)
		spec->mixers[spec->num_mixers++] = ad1988_spdif_in_mixers;

	codec->patch_ops = ad198x_patch_ops;
	switch (board_config) {
	case AD1988_AUTO:
		codec->patch_ops.init = ad1988_auto_init;
		break;
	case AD1988_LAPTOP:
	case AD1988_LAPTOP_DIG:
		codec->patch_ops.unsol_event = ad1988_laptop_unsol_event;
		break;
	}
#ifdef CONFIG_SND_HDA_POWER_SAVE
	spec->loopback.amplist = ad1988_loopbacks;
#endif
	spec->vmaster_nid = 0x04;

	return 0;
}


/*
 * AD1884 / AD1984
 *
 * port-B - front line/mic-in
 * port-E - aux in/out
 * port-F - aux in/out
 * port-C - rear line/mic-in
 * port-D - rear line/hp-out
 * port-A - front line/hp-out
 *
 * AD1984 = AD1884 + two digital mic-ins
 *
 * FIXME:
 * For simplicity, we share the single DAC for both HP and line-outs
 * right now.  The inidividual playbacks could be easily implemented,
 * but no build-up framework is given, so far.
 */

static hda_nid_t ad1884_dac_nids[1] = {
	0x04,
};

static hda_nid_t ad1884_adc_nids[2] = {
	0x08, 0x09,
};

static hda_nid_t ad1884_capsrc_nids[2] = {
	0x0c, 0x0d,
};

#define AD1884_SPDIF_OUT	0x02

static struct hda_input_mux ad1884_capture_source = {
	.num_items = 4,
	.items = {
		{ "Front Mic", 0x0 },
		{ "Mic", 0x1 },
		{ "CD", 0x2 },
		{ "Mix", 0x3 },
	},
};

static struct snd_kcontrol_new ad1884_base_mixers[] = {
	HDA_CODEC_VOLUME("PCM Playback Volume", 0x04, 0x0, HDA_OUTPUT),
	/* HDA_CODEC_VOLUME_IDX("PCM Playback Volume", 1, 0x03, 0x0, HDA_OUTPUT), */
	HDA_CODEC_MUTE("Headphone Playback Switch", 0x11, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Front Playback Switch", 0x12, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("Mono Playback Volume", 0x13, 1, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE_MONO("Mono Playback Switch", 0x13, 1, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Front Mic Playback Volume", 0x20, 0x00, HDA_INPUT),
	HDA_CODEC_MUTE("Front Mic Playback Switch", 0x20, 0x00, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x20, 0x01, HDA_INPUT),
	HDA_CODEC_MUTE("Mic Playback Switch", 0x20, 0x01, HDA_INPUT),
	HDA_CODEC_VOLUME("CD Playback Volume", 0x20, 0x02, HDA_INPUT),
	HDA_CODEC_MUTE("CD Playback Switch", 0x20, 0x02, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Boost", 0x15, 0x0, HDA_INPUT),
	HDA_CODEC_VOLUME("Front Mic Boost", 0x14, 0x0, HDA_INPUT),
	HDA_CODEC_VOLUME("Capture Volume", 0x0c, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Capture Switch", 0x0c, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME_IDX("Capture Volume", 1, 0x0d, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE_IDX("Capture Switch", 1, 0x0d, 0x0, HDA_OUTPUT),
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		/* The multiple "Capture Source" controls confuse alsamixer
		 * So call somewhat different..
		 */
		/* .name = "Capture Source", */
		.name = "Input Source",
		.count = 2,
		.info = ad198x_mux_enum_info,
		.get = ad198x_mux_enum_get,
		.put = ad198x_mux_enum_put,
	},
	/* SPDIF controls */
	HDA_CODEC_VOLUME("IEC958 Playback Volume", 0x1b, 0x0, HDA_OUTPUT),
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = SNDRV_CTL_NAME_IEC958("",PLAYBACK,NONE) "Source",
		/* identical with ad1983 */
		.info = ad1983_spdif_route_info,
		.get = ad1983_spdif_route_get,
		.put = ad1983_spdif_route_put,
	},
	{ } /* end */
};

static struct snd_kcontrol_new ad1984_dmic_mixers[] = {
	HDA_CODEC_VOLUME("Digital Mic Capture Volume", 0x05, 0x0, HDA_INPUT),
	HDA_CODEC_MUTE("Digital Mic Capture Switch", 0x05, 0x0, HDA_INPUT),
	HDA_CODEC_VOLUME_IDX("Digital Mic Capture Volume", 1, 0x06, 0x0,
			     HDA_INPUT),
	HDA_CODEC_MUTE_IDX("Digital Mic Capture Switch", 1, 0x06, 0x0,
			   HDA_INPUT),
	{ } /* end */
};

/*
 * initialization verbs
 */
static struct hda_verb ad1884_init_verbs[] = {
	/* DACs; mute as default */
	{0x03, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_ZERO},
	{0x04, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_ZERO},
	/* Port-A (HP) mixer */
	{0x07, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_UNMUTE(0)},
	{0x07, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_UNMUTE(1)},
	/* Port-A pin */
	{0x11, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	{0x11, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	/* HP selector - select DAC2 */
	{0x22, AC_VERB_SET_CONNECT_SEL, 0x1},
	/* Port-D (Line-out) mixer */
	{0x0a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_UNMUTE(0)},
	{0x0a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_UNMUTE(1)},
	/* Port-D pin */
	{0x12, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	{0x12, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	/* Mono-out mixer */
	{0x1e, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_UNMUTE(0)},
	{0x1e, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_UNMUTE(1)},
	/* Mono-out pin */
	{0x13, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	{0x13, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	/* Mono selector */
	{0x0e, AC_VERB_SET_CONNECT_SEL, 0x1},
	/* Port-B (front mic) pin */
	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_UNMUTE(0)},
	/* Port-C (rear mic) pin */
	{0x15, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x15, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_UNMUTE(0)},
	/* Analog mixer; mute as default */
	{0x20, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(0)},
	{0x20, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(1)},
	{0x20, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(2)},
	{0x20, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(3)},
	/* Analog Mix output amp */
	{0x21, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE | 0x1f}, /* 0dB */
	/* SPDIF output selector */
	{0x02, AC_VERB_SET_CONNECT_SEL, 0x0}, /* PCM */
	{0x1b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE | 0x27}, /* 0dB */
	{ } /* end */
};

#ifdef CONFIG_SND_HDA_POWER_SAVE
static struct hda_amp_list ad1884_loopbacks[] = {
	{ 0x20, HDA_INPUT, 0 }, /* Front Mic */
	{ 0x20, HDA_INPUT, 1 }, /* Mic */
	{ 0x20, HDA_INPUT, 2 }, /* CD */
	{ 0x20, HDA_INPUT, 4 }, /* Docking */
	{ } /* end */
};
#endif

static const char *ad1884_slave_vols[] = {
	"PCM Playback Volume",
	"Mic Playback Volume",
	"Mono Playback Volume",
	"Front Mic Playback Volume",
	"Mic Playback Volume",
	"CD Playback Volume",
	"Internal Mic Playback Volume",
	"Docking Mic Playback Volume",
	/* "Beep Playback Volume", */
	"IEC958 Playback Volume",
	NULL
};

static int patch_ad1884(struct hda_codec *81HD,)
{
	D1884, ad198x_spD, A86A,;
	tch err;

	86A, = kzalloc(sizeof(AD198), GFP_KERNEL)8
 *f (ght (c= dio )
		return -ENOMEMpyri81HD,->ght (c)D1988

	erredisnd_AD19attach_beep_device(81HD,, 0x10se.de>
  it < 0) {
		   AD19frethe terse.der is fr Copy	}
	set  it uamp *
 *ms of , 0, HDA_OUTPUT)pyright ->multiout.max_channels = 2; the License, or
num_dacr opARRAY_SIZE(1882, n.
 _nidsse.dhe License, or
istribut =*   is distributd in the hope that ig_outribu
 * DsefulSPDIFrsiod in the sionadt will be  This driver is d *  MERCed in the  *  MERCHAN FITNESS FOR A d in the capsrt will be usefulPublic Liced in the input_mux = & for more ture_source warranty of
mixerl be1d in the hl Pub[0].  See thebaseral Pub warranty of
init_verbblic License
 o the Freeng with this o the Freed in the spdif_route = 0;
#ifdef CONFIG_SND_ verPOWER_SAVE in the loopback.amplist.  See the.h>
#incs;
#endif in the vmasterithout 0x04;
	/* we need to cover all play#inc v
 * Hs */, Suite 3lave_vour op useful
#include ware; you cfor ADopl be usAD19p.h"

strbuter is fr0;
}

/*
 * Lenovo Thinkpad T61/X61
da_cface paD1884, AD19ou should  ad194_t[5];
	iived a copy of  =Publrsioniteml be4,
	./
	constubli{ "Mic"ms o0 },rb *inInternal it_verbs15];	/* inMixverbs35];	/* inDocking-Sace onverbs45];	/},interrol_newDell Precision T3400ixers;
	unsigned int beep_amp;	/* beep adell_desktop, set via set_beep_amp() */
	const3struct hda_verb *inFronttion verb[5];	/* inLine-I						s
						 * don't forget nsigned ;
	unsigned ind/orkcontrol_new* beep amp value,*  alon wit{
	 verCODEC_VOLUME("PCM P.h>
#inc*
 * HD inux/hda_ner version 2 ,ci.h> num_adc_nids;
	_IDXhda_nid_t *adc_nids;
	hd1hda_n3d_t dig_in_nid;		/*da_co num_adc_nMUTE("Headphoneid_t *adc_Switchptiona*/
	coig_in_nid;		/* d*input_mux;
	hdaSpeakerapsrc_nids;
	unsigned 2nt cur_mux[3];

	/* channel modids;
	hdMicid_t *adc_nids;
	hda_2eithx0ur_mux[IN

	/* channel model */

	/* PCM info;
	unsigned	struct hda_pcm pcm_rec[3];	/* usids;
	hditialization* PCM information */
	struct1hda_pcm pcm_rec[3];	/* used in rols, init_verbs and is() */

	unsigned t auto_pin_cfg autocfg;
	sids;
	hd terminit_verbs and input_mux */
	struc4hda_pcm pcm_rec[3];	/* used in c_nids[AUTO_CFG_MAX_Os() */

	unsigned nt jack_present :1;
	unsigl_mode;

	/*Boostptionaid_t dig_in_if_route;

	/* dynamic controls, init_veheck loopba5k;
#endif
	/* for virtual master */
	hc_nids[AUTO_heck loopb2onst char **ode;
	int num_channel_mode;
Ced a c_nids;
	hda_ntion;ur_mux[3];

	/* channel model */
 int ad1;
	unsignedm_info(struct snd_kcontrol *kcont; optional * int ad198x_mux_e*/
	codinfo(struct snd_kcontrol *kcontrol, = snd_kcontro;
	unsignontrol);
	struct ad198x_specubli.ifa_beepSNDRV_CTL_ELEM_IFACE_MIXER;	/*/* The ense,ple struct sndy of " eed_dacsol, fuse alsa*  al
		 * So c/corsomewhat different..ue *u/num_ge.nambeepkcontrol *kcontrt_mux 	_kcontrolIu shcontrol);truccouninux2 codeinfouct ad198xmux_ete to fo codegeinux/dedc_idx = snd_get codepec dx(kcontrol, &uconpuol->];	//* e impol, structmux *input_muxmic contrEC958id_t *adc_nids;
	hda_1b_mux_info(spec->input_mux, uinfo);
}

static int ad198x_mux_enum_kcontro
}

staticNAtion 0;
}("",PLAYBACK,NONE) "*spec = cod/* identical with* beep3
	strucgned int adc3_330, Boston_ctl_get_ioffidx(kconec->spec;
	unstrol->id);

	ucontrec->spec;
	unsnumerated.{ } /* endur_mnter/* addin!
	al  Free ers;
	unsigned int be Fre* beep amp value, Foundationned int/* Port-E (d_nids[Aface on mic) pinur_mu{0x1c, AC_VERB_SET_PIN_WIDGET_CONTROL,  calVREF80n sndialization (commonAMP_GAINx;
	h, ec *OUTx;
	hted.iter_mux[admic beck * initiaput ruct hda_codec *codec)
{
	struct adZEROted.iteAnalo codxer -*spec = code; mton,as defaulec;
	int i0

	for (i = 0; i < spec->num_initdec)
{
(4)ted.iteenable EAPD biec;
	int de *tion (commonnt P_BTLENABLEtruct2n snd_hda_input_mux_put(t num_init_verbs;

	/* playback */
	struct int need_dac_fix;

	/* ct;	/* playbac
	unsigned int num_adc_nids;
	hda_nid_t *adc_nids;
	hda_nid_t dig_in_nid;		/* d*input_mux;
	hda_nid_t *capsrc_nids;
	unsigned int cur_mux[3];

	/* channel model */
	const struct hda_channel_mode *channel_mode;
	int num_channel_mode_MONO("Mono

static int ad198x_mu3hda_inpuruct ad198x_spec *spec = codec-,
	"Side Playback Sw;
	unsigned dphone Playback Switch",
	"Mono Plaids;
	hd * dig_ou* PCM information */
	struct hda_pcm pcm_rec[3];	/* used in static void ad198x_s() */

	unsigned int spdif_route;

	/* dynamic contd are oerbs and input_mux */
	struct auto_pin_cfg autocfg;
	struct  struct snd_kcontct hda_input_mux private_imux;
	hda_nid_t private_da struct ter_nid;
	const char **slave_vols;
	const char **sstatic voiheck loopback;
#endif
	/* for virtual master */
	h int ad198x_mux_enum_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct ad198x_spec *spec = codec->spec;

	return snd_hda_input_mux_info(spec->input_mux, uinfo);
}

static int ad198x_mux_enum_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct ad198x_spec *spec = codec->spec;
	unsigned int adc_idx = snd_ctl_get_ioffidx(kcontrol, &ucontrol->id);

	ucontrol->value.enumerated.d_hda_input_mux_put(cDigizatiIC ADC NIDem_i5 +em_i6t_mux, ucontch  beep apcm_dmic_prepareAD1884, AD19d_hdstream *hctl_get_		  igned int be81HD, AD1983n_nid);
	unsigned{
		e, spec_tag err;
	}

	/* create bformaol->id);
		if (erd/orodec,ub, spec->kcontrol_, AD19/or mod81HD,_setupc, speche terms o(speckcontrol_ty ofbereep_amp) {
	 controltherpec->bse.dct snd_kcontr_nid) {
		err = snd_hda_crecleanupif_in_ctls(codec, spec->dig_in_nid);
		if (err < 0)
			return err;
	}
{
		struct snd_kcontrol_new *knew;
		for (knew = ad_beeturn -xer; knew->name; knew++) {
			struct snd_ctl_new1(knew, codec);f_in_ctls(codec, spec-if (!kctl)
				reint ad1ep_ampkcontrol_r optstru(at your_minec;
	unaster_tlv[4ad hat vmade <linux5nd_hstruct_mux,ate_spd
	ucontrsnd_hda_create_spd codecturn -A_OUTPUT, vmaster_t snd_hdint cur_e_nid) {
		err = snbuilt sndsAD1884, AD1981HD, AD1983, AD1984,
 *   AD1986A, AD198 = ; you can rd in_in_ctls(code *ctl_8
 *
 * Copyri it an(kcontr vmaster_tl publisheGNU General hed by
 *  the 
 *
ed inhe Licd_hdrec + : ad_slue.encmce, add_vmaster(cod++8
 *
fo->kcontrolADPUT,re_sptzation  Switch",l *kct[
}

stPCM_STREAM_CAPTURE with taster Playback Volum_ctl_new1(knew, /* modour */
 snd_nidNULL,
_BASIC,kctls(codTHINKPAD/* no longDELL_DESKTOP/* no longMODELSinterface paconst char *ave_sws}

	ad[ CONFIG_SND_Hned int_power_sec); ]		= "bast_vet hda_codeer neede]c, hmp valued_t nid)
{
	rn 0;
}

#ifd198xt;	/* playba"Playback Volu{
		struct sni_quirk, "Mastecfg_tbl
				     & *mixers[5];
	int num_mur_mu
 */PCI_QUIRK_VENDOR(0x17aa, " *mixers[5];
	i", eve
{
	struct a/* d*/
static int_pla028e",
	"ack;"m_ini	/* p hda_pcm_srn 0;
}

#ifnput_}interface patch for AD1889, AD1884, AD1981HD, AD1983, AD1984,
 *   AD1986A, AD1988
 *
 *board_ sndig,urn err;
	}
	ifor AD1882, Aixer_ctl(codec, "Master Playback Swiight (c): ad_slave_volpen(codec, & and/or modcheck_pen(codec, &he terms CONFIG_SND_Heep_amd);
 ad198x_check,ack, nid);
}
#eed in
	uns (pen(codec, & Publcase hda_codec); :hip(kcodec, spec-d			  (smiccur_mucense
 *  alon GNU General Pub++d_slave_swsa_cream; if no"hda_beep.h"

str. vmaster_t_slave_sws vmaster_td_hdbreakec, at,
				   er needetrucITHOUT ANY WARRANTY; without even the implied wa*
 *  You should have rep amp value, set via set_bup(struct *  along with t/* capture */
	unsiup(struct hdoundationt, write to the Free codec->spec;psrc_nids[adc_idx],ut, stream_tag,
						forn 0;
}

#ifbstream);
}

static int ad198x_pl0up(struct hda_pcm_stream *hinft;	/* playback set-up
					a_codec *codec,
				       stack Volume",
	"Monout, stream_t}ctl_new1(knew, e",
	"Leven 3 / even tAtream)9{
	struct adB
 ,
	"Lppec-B _pla4) - f* digmic-inc = codecE>specc
	rerearsnd_hda_multi_ouF>spec6
	reCD / ext outc = codecC>spec5pen(codecl areda_multi_ouD>spec2pcm_close(strud198x_dig_plA>spec1
	return shp structream *s ad19t even tA +stream *-micream *subst= equivalent	strucpcm_subsnd_pcm_suBstream ad19+int raem[0] struct snd_FIXME:_newWe sh	HDAthe single DACtl = both HP and					 strs (se,
			884/a_mu).ixersack VoluAD19nid_	err 884adistribut[1ned int	consux_pu#define struct h *  MERCic LTNESS FOR A nsigned int streaPublic Lice				   uPublic Liceunsigned ieam *suhe implied
			2eapd;
	unsigned t beep_amp;	/* be					   t via set_beep_amp() */
	constc->vmst be set
					 * dig_out_nid and hp_niit_verbs */
	up_nid arptional
					 *CD * inpl
					 */
	unsigned int cur_epd;
	unsigned int need_dac_fix;

	uct hprogram; ifgned int num_adc_nids;
	hdM#incl* PCM information */
	int cur_mux[3];

	/* channel model */
 *substream)
{
	s() */

	unsint cur_mux[3];

	/* channel model */
_nid_t *capsrc_nids;
	unsigned int cur_mux[3];

	/* channel model */
 * digk Switch",
	"Center Playback Switch",
	"LFE Playback Switch",
	"Side Playback Switch",
	"Headphone Playback Switch",
	"Mono Playback Switch",
	"Speaker Playback Switch",
	"IEC958 Playback Switch",
	NULL
};

a_nid_t *adc_nids;
	hda_	struc5hda_pcm pcm_rec[3];	/* used in a_nid_t *adc_s() */

	unsignedc->adc_nids[substream->numULL
};

static void ad198x_free_kctls(struct hda_codec *codec);

/* additional beep mixers; the actual parameters are overwritten at build */
static str snd_kcontrol_new ad_beep_mixer[] = {
	HDA_CODEC_VOLUME("Beep Pltls;
	struct hda_input_mux private_imux;
	hda_nid_t private_daUTO_CFG_MAX_OUTS];

	unsigned int jack_present :1;
	unsigned inck_detect:1;

#ifdef CONFIG_SND_HDA_POWER_SAVE
	struct hda_loopbaCD* PCM information */
	struct2hda_pcm pcm_rec[3];	/* used in later */
	.ns() */

	unsigned ter */
	.ops = {
		.open =efine set_beep_amp(spec, nid, idx, dir) \
	((spec)->beep_amp = HDA_Ccodecter_nid;
	const char **slave_vols;
	const char **s;
};

/*
 * input MUX handling (common part)
 */
static int ad198x_mux_enum_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct ad198x_spec *spec = codec->spec;

	return snd_hda_input_mux_info(spec->input_mux, uinfo);
}

static int ad198x_mux_enum_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct ad198x_spec *spec = codec->spec;
	unsigned int adc_idx = snd_ctl_get_ioffidx(kcontrol, &ucontrol->id);

	ucontrol->value.enumerated.item[0] = spec->cur_mux[adc_idx];
	return 0;
}

static int ad198x_mux_enum_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct ad198x_spec *spec = codec->spec;
	unsigned int adc_idx = snd_ctl_get_ioffidx(kcontrol, &ucontrol->id);

	return snd_hda_input_mux_put(_newubstializidx]); Freeixers;
	unsigned int be    specuct h[adc_idx],
				     &DACs; un, spec->init_verbs[i]);onstruct hda_codec *codec)
{
	s0x27},da_i0dBstream[SN4RV_PCM_STREAM_PLAYBACK].nid = spec->multiout.da  &spec-A (HP)a_sequencoston,onlyeturm and_hda_sequetream[SN7return 0;
}

static const char *ad_slave_0ols[]AM_CAPTURE].substreams = spec->num_adc_niUN;
	hd1ols[] = 198x_pc*
 * initial1zation (common callbacks)
 */
static iHP98x_initut.dig_out_nid)ec *codec)
{
	struct ad198x_spec *spec-D (d areoutnalog_capture;
	info->stream[SNDRV_PCM_STREAM_CaPTURE].substreams = spec->num_adc_nids;
	info->stre] = ad198x_pcm_digital_playback;
		inec->adc_nids[0];

	ifD(spec->multioe",
	"Surround  {
		info++;
		codec->num_pcms++;
e",
	"Surround ec *codec)
{
	struct ad198x_spec *de PYPE_alog_capture;
	info->stream[SNDRV_PCM_STREAM_1ePTURE].substreams = spec->num_adc_nids;
	info->str}
	}

	return 0;
}

static void ad198xec->adc_nids[0];[SNDRV_PC*
 * initialDRV_PCM_STREAM_ {
		info++;
		codec->num_pcms++;
DRV_PCM_STREAM_PLAYBACK].nid =";
		info->pcm_type = HDB (turn snd_
/*
 * initialds[0];
	info->s callbacks)
 */
static int ad198x_initds[0];
	info->stream[SNDRV_PCMt ad198x_spec info->ype = HDC (close(struct
/*
 * initial;

	for (i = 0; callbacks)
 */
static iINctls(str;

	for (i = 0; i < spec->num_initodec *codec)
{
	struct aE198x_sp
}

/*
 * initialization (common callbacks)
 */
static int ad198x_init(struct hda_codec *codec)
{
	struct ad198x_spent i;

	for (i = 0; i < spec->num_init_verbs; i+da_inoc->spec;
	iype = HDF (CD
/*
 * initial6;

	if (!spec)
		return;

	ad198x_free_kctls(codstatus = ad198xec *codec)
{
	struct ad198x_spec *snd_hda_sequec, spec->init_verbs[i]);
	return 0;
}

static const char *ad_slave_info->str
	return 0;
}

static const char *ad_slave__nids[eapd_get(struct snd_kcontrol *kcontrol,
			   2truct snd_ctl_elem_value *ucontrol)
{
	struct hda_c3truct snd_ctl_elem_value *ucontrol)
{
	struct ec->adcvolsda_iauxrbs[i]);
	return 0;
}

static const char *ad_slave_5ols[] = snd_hdaMixad19pec ampc;
	int i		info->name = "AD198x Digital";
		info->pcm_typek Volumepy of cur_mum[SNization (commonCONNECT_SELtructfo->streld_controls,
	.build_pcms = ad198x_build_pcms,
	.l);
nt ad198x_eapd_put(struct snd_kcontrntrol)
{
	strucec *codec)
{
	struct ad198x_spec *m[0] =ec->cur_eapd;
	elsux_eontrol_chip(kcontrol);
	struct ad19ec->ad |= spec->multiout.dac_hda_input_mux_pu02111-1307 USA
 */

#include <linsnd_hda_find_mixeramp_ <lin.channelh>
#inclu
				   {odec, sif
	/* fo, [5];da_istatic voicontro	eapd = !eapd;
	is
		da_ic->cur_eapd)
		return 0;
	spcm_da_iCDcur_eapd)
		return 0;
	s */
da_ic_nids[Acontrol->private_valu <linuxrol_new aptop	}

	act snd_spec A: _nid_t *cajack
}

statiB:f = 1ch_mode_info(C: itializatiIC
}

statiD:B_SET codecOut (if {
	"Frd)
}

statiEct snd_ctl_eIn_info *uinfo);
static Fkcontrol,
	sconst am[SNDack Volume",
		uct hmobile_
#inclusw
	reAD1884, int need_dac *need_dacdec,
		atic int adctl_elem_value *ueed_dac, AD1984,
 *AD1981HD, AD1983 and/orneed_dac_chip(need_dacse.dent rffidx (knew =_sequalue.		    );
stde_put(st      stru8
 *
 *, spe= (!     str->l,
		.integer.l,
		ng w&&ue *    AD1986A_SURR_DAC	0x04
#define 1]DAC	/* tog intGPIO1 accordds[Ato

sta, spefaceeda_cod(knew = ad_bewrite_cachthe terms opriv	return 0;
}

s_nid_DATAeep_aCLFE, spe?epare :D198d_ctl_new1(retif (!snd_hda_find_m		   struct hda_codec *col0);
	
					   struct snd_pcm_substream *substream)
{
	struct ad198x_spec *spec = codec->kcontrol *kcontrol, struct snd_ctl_elem_value *uconnd_hda_multi_out_dig_cleaspec *spec e AD1986A_SPDIF_OUT	0x02
ctl_get_ioffidxe AD1986A_SPDIF_OUT	0x02
trol->id);

	ucon_elem_value *ucontrol);
sl->id)rivateol,
			=ital-inMPOSE(kconVAL(lse
		nst ig_in_nid;		/* d];	/:1;
	unsigned int in *hinfo,
				      struct hda_codec *codec,
				      unsignda_codec_setup_stream(codec, spec->adc_nids[substream->number],
				   stream_tag, 0, format);
	return 0;
}

static int ad198x_c
	/* PCM information */
	struct hda_pcm pcm_rec[3];	/* used in alc_build_pcms() */

	unsigned int spdif_route;

	/* dynamic controls, init_verbs and input_mux */
	struct auto_pin_cfg autocfg;
	struct snd_array kctls;
	struct hda_input_mux private_imux;
	hda_nid_t private_dac_niAUTO_CFG_MAX_OUTS];

	unsigned int jack_present :1;
	unsigned int inack_detect:1;

#ifdef CONFIG_SND_HDA_POWER_SAVE
	struct hda_loopback_check loopback;
#endif
	/* for virtual master */
	hda_nid_t vmaster_nid;
	const char **slave_vols;
	const char **slaves;
};

/*
 * input MUX handling (common part)
 */
static int ad198x_mux_enum_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	struct rol->private_valueam *hinfo,
					   struct hda_codec *co_value *				   struct snd_pcm_substream *substream)
{
	struct ad198x_spec *spec = codec->/*spec;
	return snd_hda_multi_out_dig_cleanup(codec, &spec->multio}

stms = {
		{ "Mic", 0x0 },
		{ "CD", 0x1 },
		{ "Aux", 0x3 },
		{ "Line", 0x4 },
		{ "Mix", 0x5 },
		{ "Mono", 0x6 },
		{ "Phone", 0x7 },
	},
};


static struct hda_bind_ctls ad1986a_bind_pcm_vol = {
	.ops = &snd_hda_bind_vol,
	.values = {
		HDA_COMPOSE_AMP_VAL(AD1986A_FRONT_3, 0, HDA_OUTPUT),
		HDA_COMPOSE_AMP_VAL(AD1986A_CLFE_DAC, 3, 0, HDA_OUTPUT),
		0
	},
};

static struct hda_bind_ctls ad1986a_bind_pcm_sw  int ad198x_mux_enuack;
#endif
	/* for virtual master */
	hda_nid_t vmas 0x0, HDA_OUTPUT),
	onst char **slave_vols;
	const char **s("Front Playback Switch", 0x1b, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Surround Playback Volume", 0x1c, 0x0, HDA_OUTPUT);
	}ton,	0x0kcontrol,
		 ifplayis pluggeut_muface pavoidger.valuehp_auto 0x1AD1884, AD1981HD, AD1983, AD1
	/* create bpresentwitcback Swpcm_stream *ad_bereadhe terms of6A_CLep_amp)  rt = (kcocks) calSENSd = ) & 0x80A_CODE98x_(knew = ad_beF_OUTtereohe terms of6er version 2LUME("Mic  verec *)
{
	s", 0x13,?_CODEC_MUTE("s[1]ed in_FRONT_DAC, AD1986me", 0x1e, 0x0	return 0;
}

sPlayback Volumed_t ad19no Playbacuct s[1] =2)n err;
			     toint x0, HDodecifCODEC_VOLUME("Mic Playback Volume", 0x13icx0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Mic Playback Switch", 0x13, 0x0, HDA_OUTPUT),
	HDA_CODEC_Vid_tE("Mic Boost", 0x0f, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Mono Pla
	HDA_CODEC_VOLm_inftrol)
{
	struct hda_codec DA_OUTPUT),
	HDA_CO : 1Capturstream)
{
	strucHP_EVENT		0x37for 3stack mode *MICstatic stru6tch",unsolicited evx13,98x_HP snd_ sentic LUME("Mic Playback Volume",[] = _.ifacAD1884, AD1981HD, AD1983,

	/* create bres, AD19	      res >> 26t format,
			ode */
statictrucck Volume", 0x13, 0x publishedstream_tag,
				ol_new ad1986a},
	{ } /* end */
};ic* laptop model - 2c} err;
	ayback;
	x_ch_m-_ELEM_I, toog_in_nid) {
		err Volume",ubstAD1884, AD1981HD, AD1983, AD1(kcontr
stat publishe{ } /* end */
};

/* laptop m ad1986a_laptop_dac_nids[1] 			return err;
	}0x16, 0x0, HDA_OUTPUT),
	HDor*spec = c	HDA_CODEC_VOLUME("Mic Playback Volum= { 0x1 0x13, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Mic Playback Switch", 0x13, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUMEst", 0x0f, 0x0, HDA_OUTPUT;ch", 0x13,&=rt =PINDA_OU_PRESENCEtl(code!back Sw Publi", 0x13, 0x0, HDA_OUTPUT),
	HDA_CODEC_Ve *cdec,
		 Boost", 0x0f, 0x0, HDA_OUTPUTup(sUT),
		HDA_COMPOSE_AMP_VAL(0x1b, 3ree UME("Mono Playback Volume", 0x1e, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Mono Playback Switch", 0x1e, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Capture Volume", 0x12, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Capture Switch", 0x12, 0x0, HDA_OUTPUT),
	{
		.iface = SNDRV_CTL_E;

static stE_MIXER,
		.name = "Capture Source",
		.infoidxwitch>
 *o,
		.get = ad198x_mux_enum_get,
st", 0x0f, 0x0, HDA_OUTPUT),
6a_laCOMPOSE_AMP_VAL(0x1bter idd haC_VOelse_OUT, HDA_OUTPUT),
	HDA_CODEC_VOLU0x0, HDA_OUTPUack Volume", 0x17, 0x	HDA_OUTPUT),
	HDA_CODEC_MUTE("L/pciPlayC_MUTE("L Licetereo Downmix Switch", 0x09, 0x0, HDA_OUTPUT),
	{ } /* end , 0xCapture S[] = {
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "C= { 0x1nel Mode",
		.info = ad198x_ch_mode_("Mic Boos}

	/* create b ad198x_ch_mode_get,
		.put = ad198x_ch_mode_put,
	},
	{ } /* e;

static struct laptop model - 2ch only */
static hda_nid_t ad1986a 0x0, HDA_OUTPU_nids[1] = { AD1986A_FRONT_DAC };

/* master controls both pins 0x1a and 0x1b= { 0x1
static struct hda_bind_ctls ad1986a_laptop_master_vol = {
	.ock Volume", 0x1e, 0x0, HD Playback Switch", 0x1e, 0x0, HD			return err;
	odec, spec->input98x_= { 0x	}

	a[SNDRV_PCM_STREAM_PLAYBACK].channel= { 0x1idx],
				     &spec-pcm_ana*
 *- always_channeut_mus++;
		info->name = "AD198x Digital";
		infec->ad
{
	struct aF (te beconst nalog_capture;
	info->stream[SNDRV_PCM_STREAM_Cvert = (kcontrol->private_value >>_nids;
	info->stre 0x0 },
		{ "Internal Mic", 0x4 },
		ec->adc_nids[0];

	ifpd_capture_sourccheck_power_status = ad198x_check_power_status,
#enOUTf
};


/*
 * EAPD control
 * the private value = nid | (irpec redm_infcompaq 6530s/6531sDA_OUTPUTec->cur initialization (common callbacks)
 */
static iix", 0xstruct ad1};

/*, 0x0, HDnd_hdaodec->spec;

	if (!spec)
		return;

	ad198x_free>kctls);
}

static void ad198x_free(struct h0x70	"Ceda_iraise HDA_c->init_verbs[i]);dec);
	kfree(spec);
	snd_hda_deop_eapd_mixers[] = {
	HDA_CODEC_VOLUMype = HDA_Pr_mux[ad				 str/
};

/*init_ver-eapd model - 2cRE] = ad198x_pcm_digital_capture;
			i ad1986a_laptam[SNDRV_Prbs[i]);
	return 0;
}

static const char *ad_slave_vols[] = [] = {
	{
		.iface = Spinter cAD1986++;
		info->name = UNSOLICITED_ Volume"AC_USRSP_EN |ck mode */
staticAM_CAPTURE] = ad198x_pcVOLUME("Capture Volume", 0x12, 0x0, HDA_OUTPUT),
	HDA_CODEds[0];
	info->sVOLUME("Capture Volume", 0x12, 0x0, HDA_OUw ad1986a98x_init(struct hda_codR,
		.name = "Capture Source",
		.info = ad198x_mux_enuHDA_llowdac_noucha_nid_t(98x_3, 0, specifi_STREAM_Cut.dig_out_nid)tic hMASKe",
	"Cente= "External Amplifier",DIRECTIONinfo = ad198x_eapd_info,
		.get = aa_ni",
	"Ceda_ifirs03
#demodel - 0x0, HDA_OUTPUT),
	HDA_CODEC_MPLAYBACK].channel_value  = spec->multiout.max_channels;
	info->stream[SNDRV_PCM_STREAM_PLAYBACK].nid = spec->multiout.dac_nids[0];
	info->stream[SNDRV_PCM_STREAM_CAPTURE] = ad198x_pcm_analog_capture;
	info->stream[SNDRV_PCM_STREAM_CAPTURE].substreams = spec->num_adc_nids;
	info->stream[SNDRV_PCM_STREAM_CAPTURE].nid = spec->adc_nids[0];

	if (spec->multiout.dig_out_nid) {
		info++;
		codec->num_pcms	{ } /* end */
};

/* laptop-eapd model - 2ch only */

static struct hda_input_mux ad1986a_laptop_eaB (odecch_m++)
			kfree(kctl[i].name);
	}
	snd_array_free(&spec->kctls);
}

static void ad198x_free(struct h, HDA_OUTPUT),
	HDA_CODEC_MUTE("PCM Playback Cd_capt; i++)
			kfree(ka_laptop_master_sw),
	{ } /* end */
};

static structCM Playback Volume", 0x03, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("PCM Playback pd_capture_source = {
	.num_items = 3,
	.items = {
		{ "Mic", 0x0 },
		{ "Internal Mic", 0x4 },
		{ "Mix", 0x5 },
	},
};

static struct hda_input_mux ad1986a_automic_capturecheck_power_status = ad198x_check_power_status,
#enm_pcms++;
*
 * EAPD control
 * the private value = nid | (invert << 8)
 */
#define ad198x_eapd_info	snd_ctl_boolean_mono_info

static int ad198x_eapd_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct ad198x_spec *spec = codec->spec;
	int invert = (lave_vols[] >> 8) & 1;
	if (invert)
		ucontrol->value.integer.value[0] = ! spec->cur_eapd;
	else
		ucontrol->value.integer.value[0] = spec->cur_eapd;
	return 0;
}

s/* tatic int ad198x_eapd_put(struct snd_ */da_iset via_VOLUM
}

static int ad198x_ea		   struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct ad198x_spec *spVOLUME("Mic Boost", 0x0f, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Capture Volume", 0x12, 0x0, HDA_OUTPUT),
	HDA_CODEELEM_IFACE_MIXER,
		.name = "Capture Source",
		.info = ad198x_mux_enu
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "External Amplifier",
		.info = ad198x_eapd_info,
		.get = ad198x_eapd_get,
		.put = ad198x_eapd_put,
		.private_value = 0x1b | (1 << 8), /* port-D, invlog_pls[5];
	inX3 playEC_VO - HP			    2 -ture_sou			    4 -snd_hda_mulplay -  vmatch",	struc   struct trol,
				     spec->caec->spec;
idx],
				     &HP_channeldel - 2ch only */

static struct hda_input_mux ad1986a_lapt_CODEC_MUTE("Mic Playback Switch", 0x13, 0x0, HDA_OUTPUT),
	HDA_CODEC_is fronont Plck Volume",
	"Surround Playback Volume",
	"Cent7

static void ad1986a_hp_unsol_event(struct hda_codec *codec, unsigned int res)
{
	if ((res >> 26) != AD1986A_HONT_D, 0x0, HDA_- a_cr	change |APTURE].substre
		return;

	ad198x_free_kctls = !!(pmag0, HOEFum_infnge;
}

stat"External Amplif = {_INDEXk Switf7fo->streut.dig_out_nid) ROColumerivat8rr;
		spec->multiout.spd;
	unsigned int need_dac_fix;

	/* g *valp = u0x1c, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("Center Playback Volume", 0x1d, 1, spec;
	return snd_hda_multi_out_dig_cleanup(codec, &spec->multiout);
}

/*
 3, 0, HDA_OUTPUT),
		HDA_COMPOSE_AMP_VAL(AD1986A_CLFE_DAC, 3, 0, HDA_OUTPUT),
		0
	},
};

static struct hda_bind_ctls ad1986a_bind_pcm_sw = {
	.ops = &snd_hda_bind_sw,
	.values = {
		HDA_COMPOSE_AMP_VAL(AD1986A_FRONT_DAC, 3, 0, HDA_OUTPUT),
		HDA_COMPOSE_AMP_VALck_check loopback;
#endif
	/* for virtual master */
	hda_nid_t vmaster_nid;
	c7EC_MUTE("Line Playback Switch", 0x17, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Aux Playback Volume", 0x16, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Ams = {
		{ "Mic", 0x0 },
		{ "CD", 0x1 },
		{ "Aux"_chip(kcontrol); = snd_hda_create_spdif_share_sw(codec,
						    &spec->multiout);
		if (err < 0)
			return err;
		spec->multiout.s;
	unsigned int beep_amp;	/* beep g *valp = uk set-up
					 * max_channels, dacs must be set
					it_verbs[5];	/* initialization verb5l
					 */
	unsigned int cur_eh", 0x16, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Mic Playback /* Line-in selic struct hda_bind_ctls ad1986a_laptop_master_sw = {
	.ops = &snd_hda_bind_sw,
	.values = {
		HDA_COMPOSE_AMP_VAL(0x1a, 3, 0, HDA_O
		&_COMPOSE_AMP_VAL(0x1b, 30, HDA_OUTPUT),
	HDA_CODEC_MUTE("PCter */
A_OUTPUT),
	HDA_CODEC_MUTE("Mono Playback Switch", 0x1e, 0xPlayback Volume", 0x13, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Mic/* Line-in selitch", 0x13, 0x0, HDA_OUTPUT),
	HDA_CODEC	E("Mic Boost", 0x0f, 0ack _get,
		.put!tream *su*/
staticter PlaybaM_IFAC0},
	/* Mic, Phone, CD, publish_FRONT_DAC };

/* master controls both pins 0x1a and/* Line-in sel
static struct hda_bind_ctls ad1986a_laptop_master_vol = P_GAIN_MUTE, 0xb080},
	{0x1c, Auct snd_kcontrol_newHP TNDRVsmarruct hda_codec *cod, CLFec,
					 struct  codec->spec;
	 CLFE Punused8x_dig_playback_pc_WIDGET_CONTROL, 0x40 stream *h CLFE Pclose(strad198x_dig_plt_dig_ope CLFE Pins */, &spec->multiout);
}

s CLFE Pol *kcontrol,
			   tream)
{
	st>spec7
	reitializatec = snd_kcontrol_chip(kcontrol);
	long *xc0 },
	/c struct snd_kcontrol_new ad1986a_laptop_intmic_mixers[] = {
	HDA_CODEC_VOLUME("Internal Mic Playback Volume", 0x17, 0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Internal Mic Playback Switch", 0x17, 0, HDA_OUTPUT),
	{ } /* end */
};

/* re-connect the mic boost input according to the jack sensing */
static void ad1986a_automic(struct hda_codec *codec)
{
	unsigned int present;
	present = snd_hda_codec_read(codec, 0x1f, 0, AC_VERB_GET_PIN_SENSE, 0);
	/* 0 = 0x1f, 2 = 0x1d, 4 = mixed */
	Ed_capture_source = {
	.num_items = 3,
	.items = {
		{ "Mic"i;

	for (i = 0; i < spec->num_[SND_SET_CONNECT_S*
 * initialization (common callbacks)
 */
static i_kctls(cod0);
	if (spec->inv_jack_detect)
		spec ad1986a_ltialization (common callbacks)
 */
static int ad198x_a_automic(codec);
}

static int ad1986a_automic_init(struct hda_codec *codec)
{
	ad198x_init(codec);
	ad1986a_automic(codec);
	return 0;
}

/* laptop-automute - 2ch only */

static void ad1986a_update_hp(struct hda_codec *codec)
{
	struct ad198x_spec *spec = codec->spec;
	unsigned int mute;

	if (spec->jack_present)
		mute = HDA_AMP_MUTE; /* mute internal speaker */
	else
		/* unmute internal speaker if necessary */
		mute = snd_hda_codec_amp_read(codec, 0x1a, 0, HDA_OUTPUT, 0);
	snd_hda_codec_amp_stereo(codec, 0x1b, HDA_OUTPUT, 0,
				 HDA_AMP_MUTE, mute);
}

static void ad1986a_hp_automute(struct hda_codec *codec)
{
	struct ad198x_spec *spec = codec->spec;
	unsigned int present;

	present = snd_hda_codec_read(codec, 0x1a, 0, AC_VERB_GET_PIN_SENSE, 0);
	spec->jack_present = !!(present & 0x80000000);
	if (spec->inv_jack_detect)
		spec->jack_present = !spec->jack_present;
	ad1986a_update_hp(codec);
}

#define AD1986A_HP_EVENT		0x37

static void ad1986a_hp_unsol_event(struct hda_codec *codec, unsigned int res)
{
	if ((res >> 26) != AD1986A_HP_EVfo,
		.get = ad198x_mux_enum_get,
		.put = ad198x_mux_enum_put,
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "External Amplifier",
		.info = ad198x_eapd_info,
		.get = ad198x_eapd_get,
		.put = ad198x_eapd_put,
		.private_value = 0x1b | (1 << odec);
	return change;
}

static struct snd_kcontrol_new ad1986a_automute_master_mixers[] = {
	HDA_BIND_VOL("Master Playback Volume", &ad1986a_laptop_master_vol),
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Master Playback Switch",
		.iOL, 0x24 }0x1c, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("Center Playback Volume", 0x1d, 1,/*86a_hp_master_sw_put,
		.private_value = HDA_COMPOSE_AMP_VAL(0x1a1d, 2, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE_MONO("Center Playback Switch", 0x1d, 1, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE_MONO("LFE Playback Switch", 0x1d, 2, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Headphone Playback Volume", 0x1a, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Headphone Playback Switch", 0x1a, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("CD Playback Volume", 0x15, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("CD Playback Switch", 0 int ad198x_mux_enum_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	struct hda_codec *codectreams = 1,
	.channels_min = 2,
	.channels_max = 2,
	.nid{0x09, AC_VERB_SET_AMP_GAIN_MUTE, 0xb080},
	/d_hda_input_mux_put(cwitch", 0x12, 0x0, HDA_OUTPUT),
	{
		.iface = SNDRV_] = {
	{0x1a, ACDA_OUTPUT),
	HDA_CODEC_MUTE("CD Playack Switch", 0x17, 0x0, HDA_OUTPUT),
	("Mic Boost", 0x0f, 0x0, HDA_OUTPUT),
	HDA_CODEC Publix Playback Switch", 0x16, 0x0, HDA_("Mic Boost", 0x0fx_eapd_put(struct s4B_SE} Playbng-p50",
};

static struct snd_pci_quirk ad1986a_cfg_tbl[] = {
	SND_PCI_QUIR5(0x10m_subst,
	/* PC beep */
	{0x18, AC_VERB_SET_CONNECT_SEL, 0x0},
	/* HP,OL, 0x24 }itch", 0x13, 0x0, HDA_OUTPUT),
	HDA_COnfo,
		.get = ad198x_ch_mode_get,
		.put = ad198x_ch_mode_put,
	},
	{ } /* end */
};

/* laptop model - 2ch only */
static hda_nid_t ad	[AD1986A_LAPTOP_AUTOMUT_nids[1] = { AD1986A_FRONT_DAC };

/* master controls both pins 0x1a and AD1986A_LAPTOP_
static struct hda_bind_ctls ad1986a_laptop_master_vol = {
	.ops = &snd_hda_bind_vol,
 Z62F", AD1986A_LAPTOP_EAPD),
	SNct snd_pcm_substrea/
x_free_kctlsb080}
}

#ifdef COb080}LAPTACK),
	SND_PCMOBI, HDA
	SND_PCer needed */
	retA_TOUCHSMARTK(0x1043, 0x8ND_HDA_POWER_SAVE
static int ad/
};

sheck_pow1043, 0x818struct hda_D1986A_3STACrn snd_heck_amp3, "ASUS PI_QUIRd198x= { 0x,
	SND_PCI_QUx817f,d198x_value,
	SND_PCI_QUstruct ad198x_spec *spec = codec
	SND_PCI_QUd198x_xc0 },
	/amp_list_power(codec, &spec->loopback,					 ;
}
#endif

/*,
				    struct h3tion;3030, "HP hda_p M2N", AD19info,
				    struct h0xff40, "7oshib 2230sa", AD1986AI_QUIRP_EAPD),
	SND_PCI_QUIRK(0x144d56oshiba", AD1986A_LAPTOP_EAPD),
	SND_PCI,
		._QUIRK(0x14fffstruc307Toshiba", AD1986A_LAPTOP_EAPD),
	SND_PCI,
	SND_PCI_QUIRK(0x144d, 0dToshibfo,
		.msung R55", AD1986A_3STACK),
	SND_,
	SND_PCI_QUIRK(0x144d, 0exc027, "Samsung Q1", AD1986A_ULTRA),
	SND_PCI_QUIRK_MASK(0x144d, t hdst_mt hd27, "Samsung Q1", AD1986A_ULTRA),
	SND_PCI_QUIRK_MASK(0x144d, 0xff0070 eit27, "Samsung Q1", AD1_LAPTOP_EAPD),
	SND_PCI_QUIybackapd_ac, " *kcontrol,
	a", AD1986Atream *hinfo,
				    struct h0xff402a8286A_SND_PCI_QUIec->spec	SND_PCI_QUream *substream)
{
	struct ad19uct AD1884, AD1981HD, AD1983, AD1984,
 *   AD1986A, AD1988
 *
 * Co,open(codec, &pyright (c) 2005-2007 Takashi Iwai <tiwai@suse.de>
 *
 *  This driver is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This driver is  hda_codeced in the hope that it will be usefu hda_codect WITHOUT ANY WARRANTY; without even tuct ad198x_ warranty of
 *  MERCHANTABILITY or FITNEeam_tag,
	PARTICULAR PURPOSE.  See theam_tag,
	 General Public License for m		   struct s
 *
 *  You should have rece_multi_out_dig_pthe GNU General Public License
 *  along with thiscodec,
					  not, write to the Free Software
 *  Foundation, Inc., 59 els_max = spe, Suite 330, Boston, MA  02111-1307 USA
 */

#include <linux/init.h>
#include <linux/delayue[0];
	if de <linux/hda_beep.h"

struct ad198x_spec {
	stru/* sounridretume parametPubldel (struct hda_pcm_stream *hinfo,
				       struct hda1043, 0x818dec,
				    P),
	SND_PCpec->adc_nids = adeam_tag,
				       unsigned int format,
			R55", AD198bstream);
}da_attach_beep_devi= { 0x12 };

bstream *substream)
{
	struct ad198x_spec *spec = ad198x_mux_enum_pa_codec *coltiout);
}

/*
 * Digital oa_multi_out_analonel Mode",
ce;
	spec->num_mixnel Mode",
d_hda_multi_out_analoubstce;
	spec->num_mixubstup(s = !!(p
staupper-limice = S_seque_eaptoltiou98x_alayb6a_dahCODE, AC_ssi"Frodamage byd1986loaverr*codec = x Playba1986a_daaybacPublhe terms oapd = !eapd;
	ad1986a__play <<ume"AMPCAP_OFFmmonSHIFT) |d1986a_models,
						  ad19NUc->sEPSg_tbl);
	switch (boa05,
						  ad19D198s drig_tbl);
	switch (b1,
						  ad19odec-_tbl);op model - 2ch only */
stat817f,d1986a_capture_source;
	spec->itch", 0x1c, bstream *substream)
{init;
		spec->channe1;
	spec->init_verbs[0] = ad1986a_init_verbs;
#ifdef CONFIG_SND_HDA_POWER_SAVE
	sphannel Mode",
st = ad1986a_loopbacks;
#endif
	spec */
staid = 0x1b;

	codec->patch_ops = ad198x_patch_ops;

	/* override some parameters */
	board_config = snd_hda_check_board_config(codec, AD1986A_MODELS,
						  ad1986a_models,
						  ad1986a_cfg_tbl);
	switch (board_config) {
	case AD1986A_3STACK:
		spec->num_mixers = 2;
		spec->mixers[1] = ad1986a_3st_mixers;
		spec->num_init_verbs = 2;
		spec->init_rmat, substream);
}dec,
				       s	.info = snd_hda_bstream *substream)
{
	struct ad198x_spec *speep_a;
	long *valp = ucontrpec->init_verbs[0] = ad1986a_init_verbtruct hda_pcm_stream *hinLine-in selector: Line-inverbs;
#ifdef CONFIG_SND_HDA_POWER_SAV/* HP, Line-Out, Surroundst = ad1986a_loopbacks;
#endif
ET_AMP_GAIN_MUTE, ut, stream_tag,
						f_PCI_QUIRK(086a_laptop_intmic_mixers;
		spe	{0x1a, AC_VERB_ = ad1986a_modes;
		spec->num_CONTROL, 0x24 },
	/*pec->init_verbs[0] = ad1986a_init_verbs;
#ifdef CONFIG_SND_HDA_POWER_SAV AD1986A_LAPTOP_EAPD),
	SND->multiout.dig_out_nid = 0;
		spec->iI_QUIRK(0x1043id = 0x1b;

	codec->patch_ops = ad198x_patch_ops;

	/* override some parameters */
	board_config = snd_hda_check_board_config(codec, AD1986A_MODELS,
						  ad1986a_models,
						  ad1986a_cfg_tbl);
	switch (board_config) {
	case AD1986A_3STACK:
		spec->num_mixers = 2;
		spec->mixers[1] = ad1986a_3st_mixers;
		spec->num_init_verbs = 2;
}truct snd_kcontrbstream *sub2tream)
{2c;
	uct hda_cod Pins */
	{0x1b, AC_VERB_return snd_hda_multi_ouCx40 },
	{0x1-in,ut);
}d surrRV_PC(3stcode_PIN_WIDGETinfo,
					 struct hda_cEen(codec, &spemixers[0]clfTYPE_6a_automute_masterFmixers;
 = ad1986a6automute_masterGmixers;
ixers;
		sit_verbs[1eam *hinfo,
					   struc2hda_codec 3codec,
			id_t dnst st5interface pabs[2] = ad1986a_a *  MERC[2codec,
			a_code9m_stream *hinbs[2] = ad1986a_aPublic Liceultiout.max_0x09, d	   unsigned i6a_autct ad198x_spec *s/*  <li[1]  int c39"Sams60", 1a_cod0xff40,x_enude *ch20xers;
	unsigned int beep_amp;	/* bem_dacs i_out_dig_prepare(codec, &spec->multiout, stream_tag,
					     s
						 * dtream);
}

static int ad1pcm_cleanuayback_orget NULL don't f7L, 0x0},
	/* Re_jack_available(codec, 0x20xff405))
		1fultiout.dig_out_nid = 0;
		spec->input_mux = &ad19_multi_out_dig_prepare(codec, &spec->multiout, stream_tag,
					     _samsung_p50_unsol_vent;
		codec->patch_ops.in				  (spec-
	/* 6986a_samsung_p50_init;
		breakam *hinfo,
					   struct hda_codec 2odec,
					   struct snd_pcm_substreamtream *hinfo,
	olume",
	"IEC958 Playback Volume",
	NULL
};
ids;
	hdSurroundid_t *adc_nids;
	hda_nnst struct hda_inputFE Playback Switch",
	"SiCe);
	id_t *adc_nids;
	hda_n5ch",
	"IEC958 Playback Switch",
	NULL
};k SwitcLFEc, 0x25))
			spec->multioe *channel_mode;
	int num_channe* Analog capture
 */
static int ad198x_capture_pcm_prepare(struct hda_pcm_stream *hinfo,
				      struct hda_codec *codec,
				      unsigned int stream_tag,
				      unsigned int format,
				      struct snd_pcm_substream *substream)
{
	struct ad198x_spec *spec = coDELS
};

static const char *ad19860xff40IEC958 Playback Switch",
	NULL
};

static voic->init_ver(code2;
		spec->multiout.num_dacs = 1;
	 Switch", 0, 0, HDc, 0x2X handling (common part)
 */
static int ad198x_mux_enum_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct ad198x_spec *spec = codec->spec;

	return snd_hda_input_mux_info(spec->input_mux, uinfo);
}

static int ad198x_mux_enum_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct ad198x_spec *spec = codec->spec;
	unsigned int adc_idx = snd_ctl_get_ioffidx(kcontrol, &ucontrol->id);

	ucontrol->value.enumerated.item[0] = spec->cur_mux[adc_idx];
	return 0;
}

static int ad198x_mux_enum_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct ad198x_spec *spec = codec->spec;
	unsigned int adc_idx = snd_ctl_get_ioffidx(kcontrol, &ucontrol->id);

	return snd_hda_input_mux_pu = ad1986a_eapd_init_verbs;
		spec->in.h>
#incerbs[2] = ad1986a_hp_init_verbs;
		spec void ad198x_free_kctls(struct hda_codec *codec);

/* additional beep mixers; the actual parameters are overwritten at build */
statict_verbs and input_mux */
	struct auto_pin_cfg autocfg;
	struct  kctls;
	struct hda_input_mux private_imux;
	hda_nid_t private_da ad198x_spec *spec = codec->specnt jack_present :1;
	unsigned incodec, spec->adc_nids[substream-2,
	.channels_max = 6, /* changed later */
	.nid = 0, /* fill la0x0, HD	.ops = {
		.open = ad198x_playback_pcm_open,
		.prepar_spec *spec = co8), /* port-D, inversed */
	},
	d_init_verbs;
		spec->iue[0];
	if { "PCM", "ADC" };

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = 2;
	if (uinfo->value.enumerated.item > 1)
		uinfo->value.enumerated.item = 1;
	strcpy98x_pcm_analog_playback = {
	.substreams = 1,
	.channels_min = 2,
	.channels_max = 6, /* changed  ad198x_spec *spec = codec->spec;
	snd_hda_codec_cleanup_stream(codec, spec->adc_nids[substream->number]);
	return 0;
}


/*
 */
schip(kcontrol);
	struct ad198x_spec *spec = codec->spec;

	ucontrol->value.enumerated.item[0] = spec->spdif_s;
	const char **sl			  (speca_pcm_streamum_mipd = !eapd;
dif_route;
	return 0;
}

static int ad1983_spdif_route_put(__automC_VERB_SET_UNSOLICITED_ENel */
	spec->multiout.da;
	unsigned ut MUX handling (common part)
 *yback Switce(codec, 0x25))
;
	unsigned 7phone Playback Switch",
	"Mono Playback Switc86a_laptop_eaack Switch", 0xe *channel_mode;
	int 0},
	{0x0c, AC_VERB_SET_CONNECT_SEL, 0x0},
	{0x0d, ACat you ModNNECT_SEL, 0x0},
	/* ch	SND__ctl_get_ioffidx(kcontrC_VOLUMEtrol->id);

	ucontrolC_VOLUMEtrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	static char *tit_verx0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Headphone Playback VolumeME("x06, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Headphone Playback Switch"240x06, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("Mono Playback Volume" Pla07, 1, 0x0, HDA_OUTPUT) pin */
	{0x0f, AC_VERB_SET_CONNECTBACK].chan2_ch2ter_n(invert)
spec;

	if (!spec)
		return;

	ad198x_free_kctls(co20);
	if (spec->inv_jack_detect)
		ic int ad198x_eapdUTPUT),
	HDA_CODEC_MUTE("Capture Switch", struct snic struct snd_kcontrol_new ad1986a_automnt ad198x_ini2*
 * EAPD control
 * the private vic int ad198x_eapdenum_info,
		.get = ad198x_mux_enum_get,
	struct OUTPUT),
	HDA_CODEC_VOLUME("Mic Boost", 0x0c, 0x0 TemplOUTPUT),
	HDA_CODEC_VOLUME("Capture Volume", 0x15, ix", 0x5 }, HDA_OUTPUT),
	{
		.iface = SNDRV_CTLc *codec)
{
	.get = ad1983_spdif_route_get,
		.put = ad1983_s_IFACE_MIXER,
		.name = "Capture Source",
		.info = ad198x_mux_enum_info,
		.get = ad198x_mux_enum_get,
		.put = ad198x_mux_enum_put,
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = SNDRV_CTL_NAME_IEC958("6,PLAYBACK,NONE) "Source",
		.info = ad1983_spdif_route_info,
		.get = ad1983_spdif_route_get,
		.put = ad1983_spdif_route_put,
	},
	{ } /* end */
};

static struct hda_verb ad1983_init_verbs[] = {
	/* Front, HP, Monfo,
		.getenum_info,
		.get = ad198x_mux_enum_gd1983_spdif_routes; from Mix */
	{0x05, AC_VERB_SET_CONNECT_S_CTL_ELEM_IFACE_MIXER,
		.name = SNDRV_CTL(at you	SND_ 0x0c, 0SND_verbs;
		s{Swit0x0c, 0x0, HDA_5];	/{ 4ector; Mic  Templx0c, AC6ector; Mic  PCM, 5];	_analog_playback;
	info->stream[SNDRV_PCM_STREAM_PLAYBACK].chan, HDA_c struct snd_kcontrol_nannels;
	info->stream[SNDRV_PCM_STREAM_PLAYBACK].nid =init_verbs; i++)c_nids[0];
	info->stream[SNDRV_PCM/
	{0x15, AC_VERB_SE;

	for (i = 0; i < spec->num_init_verbs; i++)
		198x_pcm_analog_caET_PIN_WRE] = ad198x_pcm_digital_capture;
_CONNECT_SEL, 0x01},},
	/* Front Pin */
	{0x05, AC_VERB_SET_PIN__nids[0];

	if (spec->multiout.dig_out_nid) {
		info++;
		codec->num_pcms++;
		info->name = "AD198x Digital";
		info->pcm_typeHP selectouencT_PIN_ut.m2_SEL, 0x, 0xnt ad198x_eapd_put(struct s1cm_type = HDA_PCM_TYPE_SPDIF;
	ET_PIN_W9,
	/* Front Pin */
	{0x05, AC_VERB_SET_PIN_WIDGET_CON end */
};

#ifdef CONFIG_SND_HDA_POWER_SAVEdig_out_nid;
		if (spec->dig_in_nid) {
			info->stream[SNDRV_PCM_STREAM_CAPTURE] = ad198x_pcm_digital_capture;
			info->stream[SNDRV_PCM_STREnid;
		}
	}

	return 0;
}

static void ad198xd1983_spdif_routruct hda_codec *codec)
{
	struct ad198x_spec *spec = codec->spec;

	if (spec->kctls.list) {
		struct snd_kcontrol_new *kctl = spec->kctls.list;
		int i;
		for (i = 0; i < spec->kctls.used; i++)
			kfree(kctl[i].name);
	}
	snd_array_free(&spec->kctls);
}

static void ad198x_free(struct hda_c_build_pcms,
	.2;
	 = ad198x_init,
	.free = ad198x_free,
#ifdef FIG_SND_HDA_POWER_C (ec *spec = codec->spec;

	if (!spec)
		return;

	ad198x_free_kctls(codec);
	kfree(spec);
	snd_hda_detach = ARRAY_SIZE(ad] = ad198x_pcm_digital_playback;
	c_nids = ad1983_dac_nids;
	spec->m_sequenc, spec->ou sh,
	{ } /*UTPUT),
	HDA_CODEC_MUTE("Capture Switch", 0x15, 0x0, HDA_OUTPUT),
	{
		.iface = SNDRV_CTL_ELEM_IFACE  &spec->cund_hda	.num_items = ER,
		.name = "Capture Source",
		.info = ad198x_muxND_HDA_POWER_SAV->multiout.num_dacs = ARRAY_SIZE(ad0);
	if (spec->inv_jack_detect)
		specds = ad1983_dac_nids;
	spec-Ec->num_mixers = 1;
	spec->mixerenum_info,
		.get = ad198x_mux_enum_get,
		.put = ad198x_mux_enum_put,
	},
	{
		.iface = SNDRV_CTL_Ea_automic(c = ac->m	.name =  2,
	.items = {
		{ "Mic", 0x0 },
		{ "Mix", 0x5 },
	},
};

static struct snd_kcontrol_new ad1986a_lspec-G (CLFE	.name =  Pla_AMP_GAIN_MUTE, 0xb080},
	/* Front, HP selectorT_CONNECT_SEL, 0x0},
	{0x15, AC_VERB_S = nid | (invert << 8)
 */
#define ad198x_eap;
	case AD1e(codec, 0x25*/
	cde *ch	spec->multiopec->namute = HDA_AMP_MUTE; /* mute internal speaker */
	else
		/* unmute internal speaker if necessary */
		mute = snd_hda_codec_amp_read(codec, 0x1a, 0, HDA_OUTPUT, 0);
	snd_hda_codec_amp_stereo(codec, 0x1b, HDA_OUTPUT, 0,
				 HDA_AMP_MUTE, mute);
}

static void ad1986a_hp_automute(struct hda_codec *codec)
{
	struct ad1eapd_get(struct snd_kcontrol *kcontrol,
			   6UT),
	HDA_CODEC_MUTE("Headphone Playback Switch", 07ger.value[0] = ! spec->cur_eapd;
	else
		ucontrol->value.integer.value[0] = & 1;
	hda_n1fEAM_CAPTURE] = adec = codec->sT_PIN_WIDVOL("Mas"Mic Playback Volume", 0x13, 0x0, HDA_OUTPUThda_nid_t nid = kcontr("PCM Playback Vol);
	spec->jack_pres0c, CM;
	int invert = (kcontrol->private_value >> 8) & 1;
	hda_nid_t nid = kcontrol->private_value & 0xff;
	unsigned int eapd;
	eapd = !!ucontrol->value.integer.va*texts[] = (invert)
		eapd = !eapd;
	if (eapd == spec->cur_eapd)
		return 0;
	spec->cur_eapd = eapd;
	snd_hda_code AC_VERBcodecur_eapd)
		return 0;
	serbsite_cache(codNABLE,
				  eapd ? 0x02 	}

	ad198x_free_kctls", 0xSTec =itch", 0x6b, 0x0, HDA_OUT 0x818f, "ASUS P5", AD1986A_LAPTOP) 0x03}ck_power6(0x1043, 0x81b3, "ASU0x1b, 0xdec, h_autom,
	SND_PCIUTPUT),
dec, hit_veramp_lisrface patch for AD1882,26A_LAPTOP),
	{}
};

#ifdef CONFIG_SND_HDA_POWER_SAVE
static struct hda_amp_list ad1986a_loopbacks[] = {
	{ 0x13, HDA_OUTPUT, 0 }, /* Mic */
	{ 0x14, HDA_OUTPUT, 0 }, /* Phone */
	{ 0x15, HDA_OUTPUT, 0 }, /* CD */
	{ 0x16, HDA_OUTPUT, 0 }, /* Aux */
	{ 0x17, HDA_OUTPUT, 0 }, /* Line */
	{ } /* end */
};
#endif

static int is_jack_available(struct hda_codec *co6ion) any later version.
 *
 *3d in the hope that it will be usef_automic_vt WITHOUT ANY WARRANTY; without even p_dac_nids; warranty of
 *  MERCHANTABILITY or FITN
		spec->mPARTICULAR PURPOSE.  See t
		spec->m General Public License for dacs = 1;
		stl(code; you cvendor_houtlinu11d4,PLA80},
 *  You should have rec986a_automic_cap
	HDA_CODEfo,
		.get = ad1983_spdifturn -ENOMEM;

	codec->spec = spec;

	tion) any l  along with thiinit_verbs[2]"Source",
		.info = ad1983_spdif_route_info,
		init_ve*codechar *texts[] = { "PCMput = ad1983_spdAMP_GAIN_MUTE, 0xbstruct snd_kcontr not, write to the Free Software
 *  Foundation, Inc., 59oost: 0dB */e, Suite 330, Boston, MA  02111-1307 USA
 */

#include <linux/init.h>
#include <linux/dela"Line Playbde <linux/slab.h>
#include <linux/pc86a_dac_nids);
	spec->multiout.dac_nids = ad1986a_dac_nids;
	spec->multiout.dig_out_nid = AD1986A_SPDIF_OUT;
	spec->num_adc_nidEC_VOLUMpec->adc_nids =, 0x0, H,his dr
				       unsigned int forinit_ve:	spec->input0x1b, 0xbstream);
eneral Public",
	_AMP_GAIN_MUTEltioutch", 0x05, 0x0, HDAET_CONNECTB_SET_CONNECT},
	{0x06x03},ET_CONNECTsionEL, 0x01},
	/*  AD1983 */
	{
		.ix03},DEC_Vrom MixeeddistrfiMUTE("Authe License, or
 *  (at your optionn) any later version.
 *
 *ront del - 2ch only */
UTPUT),
Mix */
	{0x05, AC_VERB_SET_CONNECT_SEL, 0x01},
	{0x06UME("Mic Playodec,
					struct snd_pcm_substreamfor A entrieam[SNDRV_PCM_STREAM_PLAYOUTPUTback t ad19ls(coselec_am[SND(invert)
	.e <linudif_ro4a,d_kcontrolinput_m", .P_GAINt, substream,
ax0c, ACB_SET_CONNECT_A_LA 0x0},
	{0x15,2C_VERB_SET_AMP_GAIN_MUTpcm_cl80},
	/* SPDIF ro3te: PCM */
	{0x03C_VERB_SET_AMP_GAIN_MUTE, 0xb080},
	/* SPDIF ro4L, 0x0},
	{0x15, C_VERB_SET_AMP_GAIN_MUTEL, 0x0},
	/* Front P9SEL, 0x0},
	{0x1captC_VERB_SET_AMP_GAIN_MUTE, 0xb080},
	/* SPDIF 94b	{0x07, AC_VERB_SBT_PIN_WIDGET_CONTROL, 0x40 },
	/* Front & Rear81	{0x07, AC_VERB_1C_VERB_SET_AMP_GAIN_M98s
				L, 0x24 },
	{0x18 */
	{0x05, AC_9ERB_SET_PIN_WIDGET_CONTd198/* Line Pin */
	{0x09
	{0x06, AC_VER9_SET_PIN_WIDGET_CONTROLL,
	/* Line Pin */
	{0x096x0d, AC_VERB_SET6ET_PIN_WIDGET_CONTROL,986CONTROL, 0x24 },
	{0x188x0d, AC_VERB_SET8_VERB_SET_AMP_GAIN_MUTE8Line-Out as Input: disMic Pins */
	{0x08, AC_VERB_SET_PIN_WIDGstatic struct hda_amp_},
	te: PCM */
	{0x02ET_PIN_WIDGET_CONTROL, 0EL, 0x0},
	/* Front 989
	{0x07, AC_VERB_9C_VERB_SET_AMP_GAIN_MUTEatic struct hda_amp_989Mic Pins */
	{0x09[] = {
	{ 0x12, HDA_OUTPUT, 0 }hda_iterminaEC_VOLUnterMODULE_ALIAS("snd-hda-; you id:dif_*" of * nx632LICA_OU("GPLy - * nx632DESCRIPx_ea("snd_hdaDder ts HD-audie <sdecy - EN_UNMUTE(0)},
	/* Record selec.integend_hd.integep_amp selectne", 0x7 }t mic */
	{0x->vmawnet anTHISC_VOU, HDnterface patch _in selfor AD1 AD198
statlayb, AD1r is frd/or modidcodecord selec(&e AD1981_MI, AC_V("Mic Playba__exET_EAPD_BTLENABL_VERx00 }, /* x Playbadele86A_in sensing on HP and Mic jacksmodulME("it(EAPD_BTLENABLE, 0)_SET_UNSTED_E_SET_UNSOLICITED_)
