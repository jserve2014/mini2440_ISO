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
	D82, , ad198x_spD, A86A,;
	tch err;

	D198 = kzalloc(sizeof(AD198), GFP_KERNEL)8
 *f (ght (c= dio )
		return -ENOMEMpyriD1983->*
 *  )hi I8

	erredisnd_shi attach_beep_device(D1983, 0x10se.de>
  it < 0) {
		   shi frethe terthe Gr is fr Copy	}
	set Geneuamp *
 *ms of , 0, HDA_OUTPUT)ware*
 *->multiout.max_channels = 2; as pLicense, or
num_dacr opARRAY_SIZE(1882, n.
 _nidsthe ) any later veristribut =*  by
 d it willd inon) ahope that ig_out wil
 * DsefulSPDIFrsiot WITHOUTsionadt will be  Thefulrivd by
 d *  MERCet WITHOUTS FOR A HAN FITNESS FOR A t WITHOUTcapsr  MERCHANTuen thPublicany lt WITHOUTinput_mux = & for more ture_source warranty of
mixerCHAN1t WITHOUT l Pub[0].  SeY WAebasera  alothe GNU Generinit_verb details.nse
 oon) aFreeng with tILIT Foundatio warranty pdif_route = 0;
#ifdef CONFIG_SND_ verPOWER_SAVE WITHOUTloopback.amplistwith this .h>
#incs;
#endif WITHOUTvmasterithout 0x04;
	/* we nace,to cor FIall playincl vut eHs */, Suite 3lave_vou*
 * for mo#incllud the e; you c recADopense foshi p.h"erfarbutd by
 * 0;
}

/ndat LenovoABILnkpad T61/X61
D198face pa984,
 *shi ou should *   A4_t[5];
	iived a copGene  =re dliednitemCHAN4,
	./
	conste de{ "Mic"ion;0 },rb *inInternal the Fres15];i.h>inMix verb3
						 *Docking-S
	unon verb4
				},itialrol_newDell Preci() * T3400l Pus;
	unsignce, St  it uamp					 it  adell_desktop,  Sofviak se  it uamp() *ct hda_v3D1884, AD19 Fre* inFrontts;

 Frep va				 *Line-I	optios
option * don'te reget /
	strucck */
	struct d/orkcont int nu multioump value,*  alon Inc{
	t_niCODEC_VOLUME("PCM Ph>
#incll_newHD inux/AD19nert_nibs;

2 ,ci.h> sionadcribut;
	_IDXd_t did_t *n NID; optihd1d_t d3/* cdig_in

	/;		/*D1981tal-in NIDMUTE("Headphone	/* capturSwitchpg_ouannels,ruct hda_input d*ou shouldurce aSpeakerubliNID; opti*/
	struc2nt curould[3]pyri/* (at you mod source Mic	/* capture source a_2eithx0annel_mIN;
	int num_channeelanne
	inta_niinfo_channel_mo	D1884, AD19pcm pcm_recmode	intus source itializag_ou_build_pcmrmnit_vannelD1884,*/
	cpdif_route;

	/* dynace, Surols, o the Fres and ischannechannel_modt auto_pin_cfgivatecfg;
	amic contpublmray kctls;
	struu should  */
	stru4 auto_pin_cfg autocfg;
	struct NID; o[AUTO_CFG_MAX_Oct hda_input_mux pnt jack_present :1_channell_ usee;
	inBoostigned 	/* ctruct h, Bostone;
	intdynamic eed_dac_array kctheckt.h>
#i5kde <linu
	int recvirtual 
#inclunsight inv_jack_dter_nid;
	2da_vt nur **oopba	t hdsion(at you_loopba PARant inv_ion */
ng_ou;annel_mode;
	int num_channeed in ct hdadstruct hdnedm__pcmAD1884, d/orneed_dac *need_; oigned l 		 *ct snAD19mux_ennels,dfo *uinfo)
{
	struct hda_codec rol (c)
	struct hd_channel_= code)t pr1884,    AD1986ece de.ifa  it SNDRV_CTL_ELEM_IFACE_MIXER/* d/* The laterple info)
{
	sset_b" eedn.
 sdec-fuse alsa
	unsblic* So c/corsomewARRAdifferent..ue *u/sionge.nam it _spec *spec = cohould 	truct hdaIp_ameed_dac);1884counda_n2 81HD_pcmnfo(spec->ip(kcteude fe <sdegeda_nidedc_idd had/ord inet_ipec dx(c = codec-&uconpuol->	/* /* e impdec-x_info

	unOUTS];

	er */
	hdEC958* PCM information */
1bhip(kfo *uipec->OUTS];

	, u_pcm)contrface patch trol_chip(kcsionneed_dantrol, strNAg_outkcon("",PLAYBACK,NONE) "*stru =l->i			 dentical Inc. multi3mux_infstruct hdadc3_330, Boston_ctlntro_ioffi	ucontrructstru_channcode->id)pyriue.etrsnd_ctl_get_ionumerated.{ } itemndannetial/* addin!
	al datio yback */
	struct hda_dati;

	/* capture */ Foundnit_vtruct hc_buort-E (d inv_ja;
	unon mic) pinannel{0x1c, AC_VERB_SET_PIN_WIDGET_CONTROL,  calVREF80n&ucols, init_v (commonAMP_GAINdel *, D, AOUTdel *n snitennel_mader *ber_n		 *rolsput 884, AD1981HD, A81HD,, AD1x_info(spZERO_spec *Anal_get_xer -ontrol_chipe; mton,as defaull_getkconi0

	 rec(i, MA  i < structsion
	inpec->nu(4)_spec *enable EAPD bierbs[i]);de *ruct hda_codnt P_BTLENABLE1884,298x_i_ */
OUTS];

	_put(mon paray kctls;e;
	inte.h>#incunsigned it_kconincln.
 _fixe;
	int t/* dy,
	"Sidk */
	struct hdal-in NID; opti */

	/* capture source /

	/* ctruct hda_input channel model */


	/* caPublict hda_channel_modkconhannel_mode;
	int num_channeed in  hda_v_kcontroAD198t)
 */
stat *
	"Center Pl (common part)
 */
stat_MONO("Monorol, struct snd_ctl_el3 Playbacinfo(spec->input ce_write(codc-,
	"Side P
	"Side Sw_channel_modd_t *c,
	"Speaker 	unsD auide P,
	"ivate_dac* Playouerbs and input_mux */
	strucint spdif_route;

	/* dynace, Suface pavoid(spec->int hda_input_mux pkcon330, Bostonirtual master */
	hd are o_CFG_MAX_OUTS];

	unsigned irivate_imux;
	hda_nid_t pr1884, _kcontrol *ue *uct hda_cOUTS];

	uprivate_i};

static constUT),
	HDda_kcontro *spda_id PlaybaX handls#includl_muxA_OUTPUT),
	{ l beep mixter_nid;
	ccnst char **slave_vols;
	const char **suct snd_ctl_elem_valu;
	struct ad198x_spec *spec = codec-info)
{
	stigneele 1, id *ol *kc>num_init_v(i = 0; i < spec->spec;

	retul_chipcontrol->t_mux_info(spec->input	"Mono Playback_ctl_ge
er is frnter Playback Volu_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_valugetx, dir)) /* mono */

static int ad198x_build_controlure * *l, &ucola_codec *codec)
{
	struct ad198x_spec *spec = codec->spec;
	unsigned int i;
	int err;

	for (i = 0; i */
	struct hdn NIrol, &uconigned int adc_idx = ol->value.effidx(kcontrol, &ucidx(ure *._valturn sner Playback Volume",cDig initIC ADC NIDrols5 +rols6_kcontrolume"h multiou_roucodeWER_pareAgned int beer Pstream *higned inlic 	struct hda_D1983nt be83 hda_t_mu*/
	struublie,c con_tag* Copy	};
	int reate b inpuidx(kcont		if (erint ybac,ubeep co->need_dac_		retu/receidD1983_setupceep cos publion;(struneed_dac_ Geneber * max_ Publ*/
	hda_therol_nebthe )
{
	struct herr;
Publierr, &uconAD198recleanupifct hctls(81HD,ntrol_netruct hda_mp) {
		strreral l
		r is frs if neeubliinfo)
{
	struct hdat nu *kne Platurn 0< 0) = ad  itis frexer; < 0)->name we ha++c);
		(codec, kctlignenew1urn e,laybac);NOMEM;
			kctl->privat
		s!kctl		err	rekcontro* max_need_dac_*
 *tME("(atda_br_minl_get_i#incl_tlv[4ad ARRAvmade <lda_n5ctl)ME("Bekcont
	HDspdrol, &ucnter Pla/
	if 		HDlaybacis freersion 2,h>
#incl_
{
	sthd{
	"Fronecodec);
			if (!kcbuil
{
	sd198ed int be0)
			return		retur4,_new Licenght (shi I = hda_beean rs arOMEM;
			kctl *ignee.de_newthe riGeneancontrolster(codecl pe desheGNU Gengram;hed by			  as p		rers ar) any Mastrec + :;
		s			retcmce *  d_>
#incl	kct++			refonew *knew;AD_vma copptstruct Playback a_codt[ntrol,PCM_STREAM_CAPTURE Inc., st cha
	"Speake*
 * ate it */
	if (!/*ew =de ", stck Vniddio ,
_BASIC,ter 		kctTHINKPAD/* no longDELL_DESKTOPd */
	retuMODELSigned;
	unsiA_OUTPUT),
	inclswseedead[1307 USA
 */Htruct h_power_s_hda ]		= "bashe Fodec)
{
	sere",
	e]c, hapture */* c= sp_addr hda_c
 0211ol_cack Volume",";
		if (err <add(codec, kci_quirk, "M#inccfg_tblue lic L  & *al Pusp valueack Volmannel
 */PCI_QUIRK_VENDOR(0x17aa, " Analog playbac", eve>num_init_veme",
/ol, struct _pla028HD au", id"FE Plk Volint spdi_s>spec;
	retuu sho}DA_POWER_SAV
		ep.h"

1889lave_
					  (spec->slave_vols ?
					   spec->slave_vo			retuboard_8x_ing,snd_hda_ctl_	iruct ad192, Al Puate 	kctl->pk, nid));
		if (eSwithe L(c)add_vma /* endpen	kctl->p&
	st(knew =cter__(struct hda_ew->name;ower_status( * maxspecsnd_ctl_*hinf,ack, = coc;
	#ace, S}

	/ ((struct hda_pre dcasedec)
{
	st); : codec-tl->privatdif

/(smichannelare
 * 
	unsigncodec, "MastPub++pcm_prepswster_tlm; if no"atic it spec {
	.ster(codecdec->spec;ster(codecMastbronstc, at,ndif

/*struct a1884ITHOUT ANY W  ThNTY; Inc. <lieve
 *  Yode <ed waretur Yep_amp;	/*have r->capsrc_nids[ set-up
					upAD1884, 
	unsig, Inc., int ap a cunsig*/
	a_codec *chddc_idx],
t, wrc.h"t Foundatio	for (i = 0; ar *ad_sl[eate_sp],utt ad1eamntro
								respec;
	retuba_multcontrol, struct snd_ctl_pl0bstream *subtruct s spec->dinfack Volume",
k    -upl
				 = 0; i < spec
						fo,
		stf (err < _codecde Pd_hda_multi_}te it */
	if (!_codecLaybac3 /laybackA&spec-9num_init_verB
 treampol_nB uct 4) - ftaticmic-#incPlaybacE_ctl_c < sreaT, vmasteense,_ouF_ctl_6 < sCDtreaxt out_multi_ouC_ctl_5(struct hlstrupec->multioD_ctl_2uct closeAD188rol_chtrucplA_ctl_1 < spec->nhp ad198xspec->ssnd_cplaybackA +int ad19-miretur *subst= equivalent(codecuct subck Vc->speBint ad1trol+ck craem[0] ad198x_builFIXME:it *We sh	HDAanty ongle DACtl = both HP
	st					 strs (se
				884/ec->).alog f (err <shi 
	/*		if 884al,
 *  bu[1truct h
};

olume#defind_kcontrohS FOR A etai the
 *  GNUameters are o sndre details.dif

/*ure details.*/
	structuct ad1_pcm_cleanndif2eap, HDput_mux pria_multi_out mul					   et-up
					 * max_channels, dac->vms			  setl
				static vote_kc
	strhp_niy kctls;
d_pcm, su strdec = sream_tagCDec;
	p8x_dig_pl_pcm_su[] = {
	"Fronepec = codec->slume",
	"Headphone Pct hdprogrurn sn Playback Volume",
	"SpeakMh"
#ierbs and input_mux */
{
	"Front Playback Switch",
	"Surround ad198xspec->num_t hda_input_
	"Front Playback Switch",
	"Surroundc const char *ad_slave_sws[] = {
	"Front Playback Switch",
	"Surroundstatic Playback SwiCetial	"IEC958 Playback SwiLFE	"IEC958 Playback Switch",
	"Speaker yback Swi_nid_t *c	"IEC958 Playback Switch",
	N      unsigned intconst ream_tag,
				      uI 0;
}ream_tag,
				      dio interer Playback Volume",
	"I(codec5da_codec *codec);

/* additiona/

	/* capturactual parametersc->n NID; o[hda_multist chio interface pamixers; the free_* no lD1884, AD1981HD, A       trolcodecec = snultionalog ion) aac;
	coparameublistructvertructen at  vmadnup(face pastr kctl);
			if (err
		}
	p_al Pu[] =Publ ver_adc_nids;
	hdBltioPltd */
D1884, AD19, HDA_OUTPUT),
	HDA_CODEC_MUTE("Beep Playbackck_detect:1;
UTSde;
	*/
	struct hdA_POWER_SAVE
	struct hdtruct ck_detect	str 02111-1307 USA
 */ vernclude <lipec->adc_nids.h>
#iCDerbs and input_mux */
	struc2da_codec *codec);

/* additionala char **.nct hda_input_mux prlayback_opr opubli.open =gned in				 * max_cstrut str, idx, dir) \
	(= ad1)->	 * max_ =r verCrr = s 0, 0, HDA_OUTPUT),
	{ } /* end */
};

#define se;interol_newOUTS] MUX handling hda_codstrut)*/
sol, struct snd_ctl_elem_valu, idx, dir)) /* mono */

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
/
	c_clo=c consthannel_mreturn s; < spec->kcontrl, struct snd_ctl_elem_valume", < 0)
			return err;
	}
	if (spec->multiout.dig_out_nid) {
		err = snd_hda_create_spdif_out_ctls(codec, spec->multiout.dig_out_nid);
		if (err < 0)
			return err;
		err = snd_hda_create_spdif_share_sw(codec,
						    &spec->multiout);
		i spec->num_mixers; i++) {me",it *198xls, irn s);datiolayback */
	struct hda_
				put_trea	return snndif

/*
 *DACs; unntrol_neray kctls;[i]);da_v	for (i = 0; i < spec->num_0x27},ids[0dsnd_hda[SN4RV_pec->slave_ *codec ].tic  2,
	.chense, or
da  &strucA (HP)a_sequenc;
	un,only	}

m
	stl)
		og_cat.dac_ni7 NID is set in alc_bE
static int pcm_prep0ols[]ve_sws : a].hda_multir op const chan NID;UNurce *nfo-> =  * Dig_nids;
	int l1struct hda_codic ilk_pcsx = 2,
	.nid =HPAD19
	inRE] 					     ) i < spec->num_init_vernt i;
	int err;-D ( struoutnd_hg_truct sbs[i]ch",ut.dac_ni

stpec->slave_sam[SNDRV_PCM_STREAM_CAPTURE].nid = spx_muxtream[SNDR[0];*
 * Dig_hdaigitaluct "Sidep) {
 snd_eturn 0;
0de;
	ifD(structense, _codecSurrdc_i ybacktrea++
			for (i 
statcmsam[S_in_nid) {
			i"AD198x Digital";
		info->pcm_typeh",
YPE_PDIF;
		info->stream[SNDRV_PCM_STREAM_PLAYBAC1e = ad198x_pcm_digital_playback;
		info->stream[SND} neede NID is set in alc_bmixers; theltiout.dig_out_nPCM_STREAspec->multio_STREAM_PLAYBACnfo->stream[SNDRV_PCM_STREAM_CAPTU_STREAM_PLAYBACtream[SNDRV_PCM"ec->much",M_PLtypn, MHDB (pec->num_= 1,
	.chrols,_out_nistream[S {
		info++;
		codec->nut snd_ctl_r *adtl[i].name);
	NDRV_PCM_STREAMed int i;
	int(i = 0 spec->kC (fo,
					 ct++)
			kfree(k i <urn 0;
}

st{
		info++;
		codec->nuIN hda_pcmc;

	if (!spec)
atic const char *a0; i < spec->num_init_veE AD1986ontrol_newkfree(k(struct hda_cod}
	snd_array_free(&spec->kctls);
}

st_pcm_stream *hinfo,
				 gital";
		info->pcm_t]);
ec);
	kfree(spec);
	snd_hda_detachyback V i+ids[sond_ctl_geti spec->kF (CDct hda_codec_o6nid;
	 (!up
	}iver is f i <leanup(struct hda_codfaceur op ad198x_D198x Digital";
		info->pcm_type m[SNDRV_PCM_->private	info->stream[Shda_codec *codec)
{
	sspec->num_adc_nids;
	tream[SNDnfo	snd_ctl_boolean_mono_info

static int arn 0;
*spe (err < 0)
			return err;
	}
	if (sndif   2c->multiout.dig_out_nid) {
		err = snd_hda_create_s3c->multiout.dig_out_nid) {
		err = snd_hda_creltiout.end ids[auxx_eapd_info	snd_ctl_boolean_mono_info

static int a5nids[0];nvert <Mixnfo-	intamprbs[i]);
r (i = 0 no  = "shi Ix e_sptal	for (i = 0; i < spruct hda set_bhannelc_ni(struct hda_codCONNECT_SEL *speeam[SNDRld_/
	hda_ni
	.ream)EAM_C_PCM_STREA		   strucl,
	c;
	t snd_ctl_ snd_tic int ad198x_build_err = snd_hda_col
 * the private value = nid | (imin = 	.channe*spec =el	   e *spec = codec->spec;
	unsigned intltiout |M_STREAM_CAPTURE] =cr Playback Volume02111-1307 USA = 2,.h"
#incluc, snvert <<findpec;
	amp_
	eap.
	"Cent>
#incllundif

/*{nd_pcm_r **slave, p vaids[c)
{
	stru->mult	*spe = !*spec =ial
	ids[dec->spec;
river is fr0specM_PLAa_iCDpd = eapd;
	snd_hda_code= 2,>cur_ inv_ja->multiouUT),
	HDure ec, spespec = coptopneedea)
{
	st
	intA:tribunst chA_POet in alcB:f0];
chwitch"fo *uC: rols, initICet in alcD:commolaybacOut (ifybac"Frd)et in alcE8x_build_conIs));struct hda;in alc_bF
{
	struct sPlaybaV_PCM_struct hda_codec *comobile_];
	if swhda_spec *splume",
	"Hea *",
	"Heaream *h struct sndt.dig_out_nid) {
,
	"Heavols ?
						  (spec->slave_pcm_str",
	"Head= cod",
	"Heathe Gout_adc_iturn errV_PCM
			rinfo,
ct sndehda_codo,
					ru			retueep c= (!A_FRONT_->uct h.igneger.uct h				&&d) {,
		spec->s_SURR_DAC	nux/
signed i1]ine /* tognt aGPIO1 accordv_jatPlaybaeep c;
	uuct codurn err;
		}
truct_cachas publion;UT),da_codec *codec

	/*DATA * maCLFEeep c?e_spd :pec-eate it */
rets = ad = !!ucontrolhda_cRV_PCM_STREAM_PLAYBAl0_infream_taep Playback Vc->spec; snd_pcminput_mus = ad198x_build_pcms,nt err;

	for (i turn err;
	}
	if (spec->multiout.dig_out_nid) {
		e, &spec->multiott hdaetur.num_items e_DAC	0x05
 impt hd AD12
igned int adc_ix", 0x5 },
		{ "Mono", 0xffidx(kcontrol, &ig_out_nid) {
		err = ;
sdx(kcoT),
	Hruct hd=.val-inMPOSEcontrVAL(lseine sRANTYback Volume",	/* layback = {
	.suVolu198x_dom *hinfo,
		RV_PCM_STREAM_PLAYBACK]m *hinfo,
		*/
	stD1981HD,eep_mi int ad	kctl->privateeturn 0;
}

static int berspec->multa_multi_outther input_info	snd_ctl_boolean_mo->kctls);
}calc_build_pcmfree_kctls(struct hda_codec *codec);

/* additionaalc_elem_valuectual parameters are overwritten at build */
staticsnd_array kctls;
	strurol_new ad_beep_mixer[] = {
	HDA_CODEC_VOLUME("Beetatie GNy * no spec->adc_nids[substream->number]);
	return 0;
}


/*
 */
s_SETack_detect:1;
hda_pcm_stream ad198x_pcm_analog_playback = {
	.suVolu_POWreams = 1,
	.channels_min = 2,
	.channels_max = 6, /* changed cknsignec, nid, idx, dir) \
	((spec)->beep_amp = HDA_Ctic constter(code 0, HDA_OUTPUT),
	{ } /* end */
};

#define se_prereams = 1,
	.channels_min = 2,
	.channels_max = 2,
	.nid = 0, /* fill later */
	.ops = {
		.prepare = ad198x_capture_pcm_prepare,
		.cleanup = ad198x_capturLENABLE,
				  ea ad198x_dUTPUT),

static steam *hinfo,
	t_nid) {_MUTE("Surrounruct hda_input_mux ad1986a_capture_source = {
	.num_items = 7,
	.it/ontroinfo	snd_cc, &spec->multioLine", 0x4nupruct hda_f (spec->digt in uct subli*init_vms o[5]; HDA_OCDUT),
1HDA_CODECAuxUT),
3HDA_CODECd arUT),
4HDA_CODECMiPlayba5 0x0, HDA_onoUT),
6HDA_CODECPt *cUT),
7HDA_C},inter	struct ad1ct hda_cbntro no D cont6T),
	HDM_PLvo98x_{198x_play&tatic hd,
	HDvruct .ure * 0x0, HDcodec_vol,
_ec *ues DAC	0x05FRONT_3ither version 2 A_CO_CODEC_MUTE("Headphone Playbad19fine, itch", 0x1a, 0x0, HDA0", 0x1d, , 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Headphsw uct snd_ctl_elem_vad1986a_bind_pcm_vol),
	HDA_BIND_SW("PCM Playback Swit),
	", 0x1a, 0x0, HD_OUTPUT),
	{ } /* end */
};

#define se(" * diream_tag,
				    E_MObT),
	", 0x1a, 0x0, HDcodec_cleanup_stred) {
			i;
		if (err < ack SwlizaUME("Aux Playbacf nee;
	 AD1
{
	struct h ifnid is pluggeshoul;
	unsimixe4
#dure *hp_vateUTE(spec *spec = codec->spec;
	reded */
	if (sER_SAVE			 nt ad19tic int ad19
		}
reain tC, AD19fME("C_kcontro rt = contfo++
		rSENS	ret) & 0x80dec_clAD19urn err;
		}
 "MontereoHDA_CODEC_VOig_in_nid;		s;
	hdMic t_nium_is = adCODEC_3,?ec_clea;
	hdas[1]					back Swlaybaspec->EC_MUTE(eAux Pda_codec *codecUT),
	HDA_CODECad19nfo-ruct snd_pHDA_O[1] =2)d_hda_ctinfo,
	toCs aME("Aund_pif_adc_nids;
	hdHDA_UT),
	HDA_CODEC_MUTE(3icME("Aux Playback Volume", 0x;
	hdaface = SNDRV_
	HDA_CODEC_itchME("Aux Playback Volume", 0x1 ad1e",
		.heck UT),
	f_info,
		.get = ad198x_mux_enum_	{
		.ifch",
	N Volume", 0x16, */
	rr = snd_hda_create_spdif_ux Playback Volume" : 1Cruct _multi_out_dDA_OHP_EVENT		0x37 rec3		strur PlayMIC, 0x0, HDA_6back unsolicited evnum_AD19HP),
	H SAVEetai
		.iface = SNDRV_CTL_ELEMs[0];_, uicspec *spec = codec->spec;eded */
	if (sresT),
	Hnfo,
		res >> 26signemg,
				err <2,
	.nidcodecV_CTL_ELEM_IFAC_infixer_ctl(c, speci_out_analpec = cod_VOLUDA_Cd_hda_inpuode_};ic* l0);
		"Surro- 2c}_hda_ctd = spec-unsi_m-c int a, tooalue = sp);
			if *
 * HD inpuspec *spec = codec->spec;
	recontrolPUT),ixer_ctl( ad1986a_laptop_    snids[1] DEC_VOLUMnids[1_kcontrd_MUTEerr = snd_hda_ctl0x16_info,
		.get = ad198x_orontrol_chVolume", 0x16, 0x0,face = SNDRV_CTL_E= {E_MONenum_info,
		.get = ad198x_mux_enumurce",
		.info = ad198x_mux_enum_info,
		.get = ad198x_mux_enum_ds;
	ad198x_mux_enum_put,
	},
	;_mux_enum_&=st",PINversi_PRESENCE   hinf! = ad19t fort hdenum_info,
		.get = ad198x_mux_enum_r < 986A_SUt = ad198x_mux_enum_put,
	},
	a_cox0, HDA_OUTPUT),
	HDA_CODECEC_VOL3pec tereo Downmix,
	HDA_CODEC_MUTE(OLUME(hda_bind_ctls ad1986a_laptop_masttruct snd_pcm_substreUTE("PCM Playback Switch", 0x03, 0x0("Stereoxers fed 0x1b */0,
	2ruct hda_bind_ctls ad1986a_laptop_masaster_voBIND_VOL("MaD_SW("Master Playback back_e",
->va
}

static UTPUT),
	HDA8x_mux_R_DACrol->vala_laptop_y of d_ctl.A_COidx			  >
 *UTPUT.d inPCM_STREA]);
		if (er,
ad198x_mux_enum_put,
	},
	HDAaluesPlayback Volume", 0xruct dstrea_laelse6a_m("Aux Playback Volume", 0x16, M Playback Swi	HDA_CODEC_MUTE(7A_BIVolumnd_ctls ad1986a_laptop_masL/pciUT),A_OUTPUT)any l Volu Downmixp_master_sw)09	HDA_CODEC_VOLUME("CD ad1986a_laA_BIa_laptop_snd_hda_ Playback Volume", 0x15,int ad198x_mux_	HDA_CODEC_MUT;

stat_chaModck Switch",T),
	HDA_Csnd_kcon		.put = aeeded */
	if (sA_OUTPUT),
	HDA_ME("L		.anneDA_OUTPUT),
	HDA_puTPUTid_t ad1986aUTPUT),
	HDA_CODEnids[1] = { AD198h info= 2,
	.nid  Playback c hda_ninfo,
		.get = DA_COMPOS;

sne Playback SwDAC snd_hdanst cha/
	hda_n_dig_ppin HDA1a
	strEC_V;

statPUT),
	HDA_CODEC_MUTE("CD Playback Swes = {
er(codeone PlaybacHDA_CODEC_MUTE("PCM Playb.info = ad198x_mux_enu"PCM Playberr = snd_hda_ctnd_pcm_subst snd_AD19;

stareturnPCM_STREAM_PLAYBACtream[SNDR
	"Cent;

stat = spec->multioustrucM_PLan(stru- always,
	"CenshoulCAPTU
		ucontrol->value.integer.value[0] = VAL(AD>num_init_veF (f (sePlaybaSPDIF;
		info->stream[SNDRV_PCM_STREAM_PLAYBACKvest", 0x0fBTLENABLE,
				  eae >>	info->stream[SNDR),
	HDA_CODECitializatUTPUT),
, 0x0, ltiout.dig_out_nid;
	pd;
		infoopy of*hinfo,a_code
 * EAPD contrnsignetems = 2,
	.i,e <lOUTfx1d, 2ol_newnt Pl	{ "Intstreas pUT),
	Hture * = = c | (itl = rm_info compaq 6530s/6531solume", 0	.chann_codec_ops ad198x_patch_ops = {
	.build_controlOUTPUT)x_info(spesnd_hd_info,
		m[SNDRor (i = 0; i <s = ad198x_check_power_status,
#en>* no control, strupcm_cleanup(stru_pcm_stre0x70	   ite_raiser verfine ad198x_eapd_i			   	k ad198p
	}specm[SNDRVdeopruct hnalog pnd_hda_codec_cleanup_s spec->kA_Pnels_max_pcm_pre &snd_hdo the Fr-)
		r = { AD198REV_PCM_STREAM_PLAYBACK].
		info->s		ich", 0x12, 0xV_PCM_STREx_eapd_info	snd_ctl_boolean_mono_info

static int aend 
	HDAck Volume", 0x13, 0x0,pigned 
		.986- 2ch only */

statUNSOLICITED_d 0x1b *AC_USRSP_EN |ontrol_ne2,
	.nidstream[SNDR_PCM_STREAM_laptop_master_vol),
	HDA_BIND_SW("Master Playback Switch",atic void ad198laptop_master_vol),
	HDA_BIND_SW("Master Ptic hda_n198x_build_controls,
	.
	HDA_CODEC_MUTE("CD Playback Switch",T),
	HDA_CODEC_VOk Swllowkcontouchic cons( {
	itch",Voluifi->slave_s		info->name = EC_MUMASK_codec     = "ExializatAde <fier",DIRECTION_mux_enum_put, snd_A_CODEC_UTPUT),
ic cnfo = aite_firs0386A_ = { AD1M Playback Switch", 0x03, 0x0,t,
		.put = ad198t_nid) CM_STREAM_CAPTURE] *  (at your->stream[SNDRV_PCM_STREAM_PLAYBACtream[SNDRV_PCM_STREAM_CAPTURE] =.dig_out_nistream[SNDRV_PCM_STREAM_PLAYBACKCODEC_MUTE("Captured */
DIF;
		info->stream[SNDRV_PCM_STREAM_PLAYBACKam[SNDRV_PCM_STREAM_CAPTURE].nid = spfo->stream[SNDR, HDA_OUTPUT),
	{ } /* end */V_PCM_STREAMut.dig_out_nid;
	 if (spec->dig		info->name = nfo->stream[SNDRV_PCM_STREAM_Ct ad1986a_laptop_d_hda_bind_DA_CODEC_VOLUME(,
	   HDA_PUT),
	HDA_CODEC_MU 0, HDA_OUh", 0x12, 0x0, eaB (nd_p mas++		erryback ter [i]_CODEch", ", 0x0, 3, w ad19k Volumstatic struct snd_kcontrol_new ad1986a_lapthda_bind_ctls ad1986a_laptop_masa_nidT),
	HDACe_sour
#ifc_write(codec12, 0x0, HDA_OUTswHDA_CODEC_VOLUMc_read(, 0x0, HDA_OU0x36

staticl),
	HDA_BI0truct hda_bind_ctls ad1986a_laptop_mas	0x36

staticre_source = {
	.ew aaybaer */teuct s3HDA_86a_auto, HDA_OUTPUT),
	HDA_CODECic struct hda_input_mux HDA_OUTPUT),
	HDADA_OUTPUT),
	HDA_CODEC_MUf, 2 = 0x1d, 4 = mvate_cretruct snum_items = 2,
	.items = {
		{ "Mic", 0x0 },
		{ "MREAM_CAPTU
	},
};

static struct snd_kcontrol_new ad1986a_l				t << 8x = 2,signed i
		.put = ad198x, 0x0ignebooO("L_monod198x in alc_build_pcms * snd_ctl_elem_value *ucontrol)
{
	struct hda_cec->multiout.dig_out_nid) {
		err = snd_hda_create_spdif_out_ctls(codec, spec->multiout.dig_out_nid);
		if (err < 0)
			return err;
		errCs as  0x0 },
),
	HDA_CODE>> 8T),
strustru}

stax_chf (err < 0)
			r	0x04
#dure *in = 2!2,
	.channeec;
	int i{
		dec *codec)
{
	struct ad198x_spec *ec = codec->spec; NID is set instataker */
	else
		/* untic int ad198x_ */apd =set-up_laptoet in alc_build_pcms *ea;

static structt.dig_out_nid) {
		err = snd_hda_create_spdif_out_ctls(codec, spec->multiout.dig_out_nid);
		if (err < 0)
	0, HDA_OUTPU = ad198x_mux_enum_put,
	},
	HDA_CODEC_MUTE("Stereoaster_vol),
	HDA_BIND_SW("Master Playback Switch",UT),
	HDA_CODEC_MUTE("Mic Playet,
		.put = ad198x_mux_enum_put,
	},
	{", 0x13, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Mic Plaeapd_info,
		.get = 198x_mux_enum_put, = ad198x_eapd_put,
	lse
		/* unmutPUT),
	/* 
	   HDA 0);
	spPUT),
al Mic", 0x4 =A_OUT86a_1 (specrr;
	ppec-Dint vDIF;pl playbackX3olume6a_la - HPSwitch"2 - a copy Switch"4 -c, &spec->mnid  -  Swiback mode *		mute = sstruct hdnfo,
	,
	.chaa (i = 0; i = spec->multiouHP,
	"CentC_VERB_GET_PIN_SENSE, 0);
	/* 0 = 0x1f, 2 = 0x1d, 4 = mixed6a_laptop_master_sw = {
	.ops = &snd_hda_bind_sw,
	.values = {
		HDA_Cy
 * oda_i Pl{
	if ((res_nid) {
			iUT),
	HDA_CODEC_Mo = ad17ruct snd_kcontrol_n6a_hp_[] = _aybaild_controls,
	.build_pcm, 3, 0, 					  ress = astru(get,
		.p) !=UTPUT), *H	HDA__info,
		.g- ter_	
	"Cge |am[SNDRV_PCM_ST_check_power_status,
#endif
};c *s!(pmagPlayOEFr */
	ngeset in al;
	ad1986a_hp_autruc_INDEXad198xf7eam[SNDR *codec)
{
	unsiROC
 * HT),
	8ure SwSTREAM_CAPTURE]sam *hinfo,
					   struct hda_code/* g *valtic uTE("Aux Playback Switck Volume", 0x16, 0x",
	"Si     struct hda_l),
	HDA_BINd, 1MIXER,DA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("LFE Playback Volume", 0xustruntrol_nitch", 0x1a, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("CD Playback Volume", 0x15, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("CD Playback Switch", 0x15,Playback Volume", 0x1a, 0xswHDA_OUTPUT),
	HDA_CODEC_MUTE("Headphone Playback Swlayback Volume", 0x15, 0x0_CODEC_MUTE("Headphck Volume", &ad1986a_bind_pcm_vol),
	HDA_BIND_SW("PCM Playback Switch", &ad1987DA_OUTPUT)ed iT),
	HDA_BIND_VOL("MaybackE("Aux Playback Volume", 0x16, 0x0,Auxe = SNDRV_CTL_ELEM_IFA, HDA_OUTPUT),
		HDA_COM86a_laptop_masA, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE_MONO("Center Pl = codec->spec;
alue[0] = er_tlv);
		if_shae = w	kctl->l
					 
	{ } /* AL(0x1a, 3, ->beep_amp;
			err = snd_hda_ctLEM_IFACE_MIXER,
ck */
	struct hda_multi_out multio	.info = sncm_open(struct LUME  (at your, cifi mutiout, stream_tnfo->streand hp_nirols, init_vt_nid5cm_cleanup(struct hda_pcm_streVOL("Ma},
	{0x0b, AC_VERB_SET_CONNECT_S0, HDA_OUTPUT),
		0,
/* d areonaledetaC_MUTE("Capture Switch", 0x12, 0x0, HDA_OUTFE DAC; mute as default */
	{0x03, AC_VERB_SET_AMP_GAIN_MUTE, 0xb080 0x0back Volume",
		& Playback Volume", 0x03,= AD1986A_MIC_EVENT)
		return;
	ad1e = ad1ack Switch", 0x03, 0x0, HDA_OUTPUT),
	HDA_BIND_VOL("Master  = SNDRV_CTL_ELEM_IFACruct hda_bind_ctls ad1986a_laptop_master0},
	/* Mic, Phx_mux_enum_info,
		.get = ad198x_mux_enu	c void ad1986a_hp_unso_pcm_OUTPUT),
	/!ut_mux adde_put,
	}tream)
{
t ad190DA_C/*t hd, layba, CD,ixer_ctl*/
	HDA_CODEC_VOLUME("Capture Volume", 0x12, 0x0, HD0},
	/* Mic, PHDA_CODEC_MUTE("Capture Switch", 0x12, 0x0, HDA_OUTPUT),
 *codetop_mA_BIb08P_GAItializatec, kctl);
			if (erHP TM_STsmarm_stream *hinfo,
		, CLF{0x0e, ACne, CD, turn err;
		errCLFEE Punddituct hda_c),
	HD_pcllbacks)
 */
stat0x40,
		0
	 *h_WIDGETfo,
					 intern hda_cLine",ANY WIDGETx12,a_co_AMP_VAL(0x1a, 3, 0, sGET_CONrol)
{
	struct hda_cr 3stack mod_ctl_7dec,rols, iniad198x_spec *spec = codec->spec;
	u,
			*xc	HDA_C/one, CD, ck Volumespec = cod, 0x12, 0x0, int_creTPUT),
	HDA_CODEC_MUTE("PCM PE(ad198x_init(ce = SNDRV_CTL_ELEM_IFAybachda_bind_ctls ad1986a_laptop_mas0 },
	{0x21, AC_VERB_SP, Line-Out, SurA_OUTPUT),
	HDA_CODEC_VOLUMc_read(codre-Aux"etrolhe;
}
 beck .channe ad1982,
	ad198x_A_POL_ELtic = 2,
	.nid TPUT, 0,
				nly */
ld_controls,
	.build_pcms = a*/
	struct hdER_SAVE;
	ER_SAVE
T_SEL, 0x0}OUTPU),
		kctl->p0x1ux_ezation (cocks) calDA_O */
L, 0/* 086a_hpf, 286a_hpd, 4 = *codONTRO	E(codec);
}

static int ad1986a_automic_init(struct hda_codeinit = ad198x_init,
	.free = adPCM_ommonpd_put(st hda_codec_ops ad198x_patch_ops = {
	.build_controldif
};


/ selestruct hdainv_A_POWreams x_ch.num_-In Pin *p */
	{0x11,x_patch_ops = {
	.build_controls = ad198xct hda_ver
				   t in alc_build_pcmsh only */

_build_controls,
	.build_pcms = a ad198x_buil
				   _VERB_uct hda_verNECT_SEL, NID is set icodec, 0x1nly *ton,RB_GET_PIN_SENSE, 0);
TPUT, 0,
				updlv);hl out
 */
sta 0; i < spec->num_init_verf (err < 0)
			return err;
		err = snd_hda_crIN_Wmic(struct hdaA_POWER_SAVEx_chIN_WIc struec *;
	h;r;
	}ton,igned= snssubstreNECT
	unsig dynnB_SET_PIN_WIDGET_CONTRsnd_ecessar HDA_und out *1c, AC_VERB_SElue.T_PIN_WIDGET_COa23, AC_VERB_SETin seleE -> CLFE */
	{ 0x
staeoN_WIDGET_COb_WIDGET_CONTROLpec->mu/
	{ 0x1c, A,RB_SEc struct snd_kcontrol_n				  ET_PIN_Wld_controls,
	.build_pcms = ad198x_build_pcms, 0)
			return err;
		err = snd_hda_cr -> Line  In */
	{ 0x1c, AC_VERB_SET_PIN_WIDGET_CO_PIN_W PIN_IN },
 	/* Line-in seleurround out -> Surster_miR_SAVE
,
	HDA 0x00 CONNECT_SEL, 0x4 },
	{ } /* end */
};0x1b, AC_VERB_SET_ad1986a_automic_veEL, 0x0 },
, AC_VERB_und */
	{ 0signed idate_hp(c
static struc1, HDA_OUTPUT, 0,
					   HDA_AMP_MUTE,
					   valp[1] ? 0 : HDA_AMP_MUTE);
	if (change)
		ad1986a_update_hp(c
sta* bind hp and internODEC_VOLUME("LT),
	/* 
	   HDA/
};

staticME("Mononit(struct hda_codec *codec)
{
	ad198x_init(codec);
	ad1986a_hp_automute(codec);
	return 0;
}

/* bind hp and internal speaker mute (with plug check) */
static int ad1986a_hp_master_sw-> Mic in */
	{}

sta
	{ 0x1c, AC
	/* Line, Aux, CD, Beep-In Pin ET_PIN_W HDA_OUTTPUT),
	HDA_CODEC_BINDRB_S(
}

static int adl),
	HDA_&h", 0x12, 0x0, HDA_OUTPUT("CD Playback Volume", 0x15,T),
	HDA_CODEC_MUTE("Mic Pla}

static int ad198back Sw	.i_PIN_W24 3, 0da_mixer_amp_switch_info,
		.get = snd_hda_mixer_amp_switch_get,
		.put = ad19/*e ad198ault */
	VOLUME(static int ad1986ax14, AC_VERB_SET_AMP_GAIC_VED_SW("Master Playback Switch", &ad19_hda_mixer_amp_switch_gBIND_VOL("Ma ad198ed int res)
{
	switch (res >> 26) {
	casc,
				      unsigned1986a_hD_SW("Master Playback Switch", &("Stereonsigned int format,l),
	HDA_BIN_PIN Playback Switch", 0x03, 0x0, HDA_nsigned int format,
				   da_codec *codec)
{
	ad198x_init(codedec, unsiIFACE_MIXERl),
	HDA_BIN5_SW("Master Playback Switch", &ad1986aIFACE_MIXER,
		.name  = 0, /* fill later */
	.ops = {
		.prepare = ad198x_capture_pcm_prepare,
		.cleanup = ad198x_capture_pcm_cleanup
	}_STREAM_C1HDA_(at your[4]; optad1986a_modelsad ha86A_Mnid{, 0x0,tion (commonec *code Pin */
	{0x1a, A/		spec->multiout.sharmaster_sw),
	HDA_CODEC_VOLUME("CD Playback Volume", k Volumea_codeAC	AD1986A_3STACK,
	AD1986A_LAPTOP,
	AROL, 0x20 },
	{0x23,round, CLFE select void ad1986a_hp_unsol_event(struct hda_codec *cPUT),
RB_SET_CONNcodec);
	ad1, HDA_OUTPUT void ad1986a_hp_utruct hda_codec *co4comm}80},
	ng-p50"A_OUTPUT),
	HDA_CODEruct h->loopb

static );
}
#e
	HDA_CO* Mitatic in5P_GA0da_inpuGAIN_MPCcodec 0 },6A_L8	= "6stack",
	pd_put(struultra_GAIN_MHP,	{0x1a, ACt, Surround, CLFE, Mono pins; mute as /* bind hp and internx0, HDA_OUTPUT),
	/* 
	   HDA_CODEC_VOLUME("Mono Playbaps = &snd_hda_bind_voA_OUTPUT),
	   HDA_CODEC_MUTE("Mono Pl	[DAC	0x05LAPTOP_ack_MUT, 0x0, HDA_OUTPUT), */
	HDA_CODEC_VOLUME("Capture Volume", 0x12, 0x0, HDA62F", AD1986A_LHDA_CODEC_MUTE("Capture Switch", 0x12, 0x0, HDA_OUTPUT),
	{
		k Volume", 0x1a, 0x0, HD Z62F"T),
	HDAAD1986A_Lnt Ph_inSNDA_OUTPUT),
	HDA_CO/
p(struct hda{0x1a;
	retur1-130{0x1a1986ACKPCI_QU153,MOBIer ve 0x1153,struct aONNECTretA_TOUCHSMARTKM9", 4
	{0x8= 2,
	.channels_mal, struct snd
					 { "Mic",0x1043, 018HDA_OUTPUT)86A_LAP3STACUT),
	HD "Miamp3, "ASUS Ptic in*/
};;

sta0x1043, 0tic x817f,*/
};
ure */ 0x1153, "ASt },
	{ 4, ad1986a_ch4_init },
	 0x1153, "AS*/
};
OL, 0x24 lue. <litems = Playback Volum.h>
#inc,s */
	am_taghar *r Plndif

/*
HDA_OUTP3_info30_GAI"HP/
stat M2NAD1986AA_OUTPUT),
		H86a_laptopff4Tosh7oshib 2230saAD1986A_LAtic in
	SND_PCI_QU153, "ASUSIRK(044d560xb03msung R55",PTOP),
	SND_PCI_QU153, unsolPCI_QUIRK(0fffode *307Td, 0xc01e, "FSC V2060", AD1986A_LAPTOP),
	,
	SND_PCI_QUIRK(0x1, 0dc024, 8x_eapdmsung R55AD1986A_LAP, AD1K(0x1043,),
	SND_PCI_QUIRK(0x144d, exc027, "Sang Q1"Q1AD1986A_LAPULTRAACK),
	SND_PCI_QU_
		.K(0x144d,und st_mund 00, "Samsung", AD1986A_SAMSUNG),
	SND_PCI_QUIRK(0x144d, 0xc504K(0x0070 eit00, "Samsung", AD1986V2060", AD1986A_LAPTOP)ic i"Sidesnd_truc"l)
{
	struct msung R55",nt ad198x_dAPD),
	SND_PCI_QUIRK(0x142a820x05

	SND_PCI_ (i = 0;
	SND_PCI_Qruct ad198x6a_capture_source = annespec *spec = codec->spec;
	return snd_hda_multi_out_analog_o Co,pcm_ruct hda_of the L(c) 2005-2007 Takashi Iwai <tiwai@suthe GNU structBILITY or FITNEstru softde "hda_beeave_ andt wille
	}
	i_stream ifaybackwarened bR_DAC, AD19f 0x168x_spec *spec = Software
  as laptop moPlayback Swispec SPUT, 0 [adc_idx],
;986Ahig_in_nid;		/OUTPUT,ny later ver_CLFster_tlvcodec =) any8x_playin_nid;.}, /* Mic */
	{ 0x14, Hnd Playbacce, Suite  ANY WARRANc License for mnd Playbact Weam);
}

static int ad198x_playbacknfo(spec->ithe GNU Gener PURPOSE.  TABILITY orSee th - 2ch onlPARTICULAR PURol,
with this - 2ch onl}, /* Aux */
	{ 0x17, HD rece;

static str}, /* Mi hda_pcm_stream *hceCODEC_VOLUME_MOpPUT, 0 }, /* Aux */
	{ 0x17, Htruct ad1, Inc., 59 /
	{0x0e, AC  no
	struct ad198x_spec } /* endstrucadc_idx],
, Inc., 59 ] = {
	[ADspecodec.h"
>spec;
	un, MA   & 0xff;
	unsigned int eapd;
	eapux/swaph>
#inclmax_channelsdelayx_speONNECTx_channelsa_multi_out_anal,
	{ 4, ad1986a_num_ini/* sounridr ismunsiuct s */
urro out
 */
static int ad198x_dUTPUT),
		HDAne, CD, AuxIRK(0x1043,986A_SURR_DACP86A_LAPTOPoid ad1986a_a* 
	  - 2ch only *fo,
			*/
	struct hd = ad198x_c, AD1986A_U, &spec->muda_ify
 *  it under;

stat2DEC_Vnput_mux ad1986a_capture_source = {
	.num_items = _pcms */
};

statrc_nids[1] =x1a, 3, 0, HDA_*eger.val o_CODEC_VOLUMc Plaitch", 0x13co->s const chamixitch", 0x13	HDA_CODEC_VOLUMc PlainpuER_SAVE
	spec->looinpua_coster_mi AD1upper-limik VoluV_PCM_g chtverbs[d out },
6a_dah_adc	= "6ssix0, damage byIn Piloaverruct ad198RB_SET_Code adda),
	H */
HDA_CODEC_
		return 0;
	-In Pin 40 }, <<VolumMPCAP_OFF_codSHIFT) |In Pin ),
	Se *udec, 0x */
NU(i =EPS
}
#eme", verbs (boa05_config) {
	caHDA_ITY oA_3STACK:
		spec->*ad1nfig) {
	cayback_3STAC, HDA_OUTPUT),
	   HDA_CODE AD1986A_CI_Qdec);
}

stat_SAVE
	spt, Surrounc, nput_mux ad1986a_captswap1/2 pin */
	"Censtru#define ad198x_ean = 2-In Pin  Playback Vo	.channels_min = 2,
	.channels_max pnum_cha", 0x13s/* 
	   Hx12,h>
#incude <linu= ARRAOUTPUT)_PCM_NNEC/* efor (i for ADK(0x10*
 * Dig->mixerstch",
	ubstrch",{
	sstruct snd_p0 },pen(coconfigCLFE -> CLFETACK)els = 2;
		s	kctl->p986A_LAP_SND_Ht_mixers;
		sptch (board_config) {
	caPCI_QUIRK(0ACK:
		spec->nu = 2;
		ssignemat,
86A_ULTRA),
	S:/2 pin */ec->loond_popti/2 pin */
alog p HDA_-In Pin 3amsulayback AVE
	spec->ray kctls;
laptop_mastero the ad19 ad1986a_caaptueam *hinfo,
				8x_mux_enme", 0x1aput_mux ad1986a_capture_source = {
	.num_item* maGET_CONTRnfo = sn	{ "IRRAY_SIZE(ad1986a_modes);
		spec->needut
 */
static int ad198x_
	/* Mic, Pector:,
	/* Mineed_dac_fix = 1;
		spec->multiout.max AD198,
	/* Od_hdd) {
			ec->multiout.num_dacs = 1;
		br,
	[AD1986A_3STACKd_hda_multi_out_analog_SND_PCI_QUIRPin */
	{0x20, AC_VERB_Sd1986a_86A_LAPTOPon (co		spec->mix),
	_SAMSUNG[0] = aB_SET_PIN_W, ACGAIN_RRAY_SIZE(ad1986a_modes);
		spec->need_dac_fix = 1;
		spec->multiout.maxe, "FSC V2060", AD1986A_LAPAM_CAPTURE] 					     f MA  pec->mixe_PCI_QUIRK(043D1986A_LAPTOP:
		spec->mixers[0] = ad1986a_laptop_mixers;
		spec->multiout.max_channels = 2;
		spec->multiout.num_dacs = 1;
		spec->multiout.dac_nids = ad1986a_laptop_dac_nids;
		break;
	case AD1986A_LAPTOP_EAPD:
		spec->num_mixers = 3;
		spec->mixers[0] = ad1986a_laptop_master_mixers;
		spec->mixers[1] = ad1986a_laptop_eapd_mixers;
		s}	/* Line, Aux, Crbs = 2;
		s2017, "Sa2	err ound PlaybONTROL, SND_PCb	= "6stackOUTPUT),
	HDA_CODEC_VOLCWIDGa, AC_VE-in,, 3, 0d surrSTREA(3stERB_n callbacksA_CODEC_MUTERV_PCM_STREAE hda_amp_lispe_mixers0]clfTRV_P_VERB_SET_PIN_WIDGF1] = ad1		spec->mi6RB_SET_PIN_WIDGG1] = ad1986A_SAMSUnfo->stre1),
	HDA_CODEC_MUTE("Surroter *81HD, 3/
	{0x0e, C958 Payback5DA_POWER_SAVbs[2MUTE("Cap_VERS FOR A [2/
	{0x0e, tomic_9c int ad198x__hp_init_verbs;
	ubstream *suse, or
 *  , 0x0,dpcm_sruct hda__VERB_
	{ 4, ad1986a_ch/* 
	eas;
	 {
	"F39"Sams60", 1tomicK(0x144em_vaPlayba20ayback */
	struct hda_multi_out mulon.
 s -ENOMEM;

	te_spd_COMPOSE_AMP_VAL(0x1a, hda_multi_out_analo,
				l
					 */
&spec->multiout);
}

/*
 *hinfo("LF},
	{0xned indio /
	unsig7ASUS U5A", ADRe},
	{ avail	"FrN_WIDGET_C2K(0x145)x_ch1fbs[1] = ad1986a_eapd_init_verbs;
	u should hav */
turn -ENOMEM;

	ture_source;
		codec->patch_ops.unsol_event = ad1986a_samsung_p50  HDA_AP_MU[SNDRV_PCM_86a_lapto.in(kcont(struclecto6rbs;
->mixers[2] pec->numstrea,
	HDA_CODEC_MUTE("Surround Playback2ice(codec, 0xtatic struct hda_input_muAD1986A_SPDIF_Oe(codec, 0_spec *spec = co*
 * HD audio inte8x_mux_ed) {
			Playback Volume",
	"IEayback Switch", ad19,
				      unsigned int C AC_VPlayback Volume",
	"IE5 ad198x_spec *spec = codec->spec;
	snd_hstatic LFEAUTOMUspec->LEM_IFACE_MIXlayback Switch",
	"LFE Playback * snd_hgstruct s = 2,
	.nid = 0, /* fil1986a_chl Mi
		spec-t.dig_out_nid = AD1986A_SPDIF_OUT;
	spec_COMPOSE_AMP_VAL(AD1986A_SURR_DAC, 3, 0, ormat,
					src_nids = ad1986_capsrc_nids;
	spec->inpms to report truct hda_input_mux ad1986a_capture_source = {
	.num_items = 7,ND_Hinterface paspec->num_adc_erbsK(0x14_spec *spec = codec->spec;
	snd_hda== spec->cuine ad198xONTROptop_master_pec->mulsion.
 har *PIN_codec);
	a23, AC_AUTOMU_min = 2,
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
	info->stream[SNDRV_PCM_STREAM_PLAYBACK] = ad198x_pcm_ana		spec->mix = ad19layback Vopec->mixerh>
#incl>stre_init_verbs;
h0x20o)
{
	static cha_pcm_cleanup(struct hda_pcm_stream *hinfo,
				      struct hda_codec *codec,
				      struct snd_pcm_substream *substream)
{
	structSURR_DAC, 3, 0, HDA_OUTPUT),
		HDA_COMPOSE_AMP_VAL(AD1986A_CLFE0, HDA_OUTPUT),
		0
	},
};

/*
 * mixers
 */
static struct snd_kcd */
};

static struct hda_verb 98x_pcm_analog_playback = {
	.subOSE_AMP_VAL(AD1986A_CLFE_DAC, 386A_MODELS] = {
	[AD6rr;
	}

stad8x_playback_papd_inrr;
	fERCHlapture So8x_playback_pcm_pr/*
 * Digi},
	{0x1cm_WIDnunsol_ee_sp1986a_ch4_init }_put(struct snd_kcblisONNECT_masuinfo)
{
	static char *s = ARRAY_SE PlCM"ND_PDC"DEC_V	ol *k->< spec-
}

static int rs;
	ENUMERATEDc = (i = 0c->s_SETstruc(i = 0ams = 1,
	.channels_mrs;
		spstrusnd_kcontrol_chip(kcontrol); > 1a_cod8x_spec *spec = codec->spec;dec = strcpyernal Mic Playbyback_pcmc int _PCM_STREAM_C*ad1986a_models[AD1986A_MODELS] = {
	[AD snd_kcontrol_d */
};

static struct hda_verb ad11c, AC_VERB_SEeturn -	HDA_COMPOSE_AMP_VAL(AD1986A_CLFE_DAC, 3, 0, HDA_info	snd_ctl_boo},
	},/
s= codec->spec;
	unsigned int i;
	int err;

	for (i = 0; i <
	.substreams = 1,
	.channels_min = 2,
	.chMono se HDA_OUTPUT),
	{ }spec->num_tatic int adc->lo		return 0;
erwritten act hda_bind_ctls ad1986a_bind_3 Mono sostonanalogERB_SEion (commonVOLUME("CaptENrround STREAM_CAPTURE] =c = codec->snnels_min = 2,
	.channels_max = };

static APTOP_AUTOMUspecc = codec->s7ned int format,
				      struct snd_pcm_subs = mixed */
	

static structlayback Switch",
	"LFE1a, AC_V0ization (common 0x11f7, "ASUS U5A",DA_CdPTOPter_tl 2;
x11f7, "ASUS U5A", ADch
	SNDare_sw(codec,
						   ;
}


/*ultiout);
		if (err <, 0x11, 0x0,
static int ad198x_build_controls(struct hda_codecbs = 2 handt8x_frec);
		break;
	}
}

static int ad1986a_samsung_p50_init(struct 0x2x0},
	{0x0b, AC_VERB_SET_CONNECT_SEL, 0xnsigned int format,
				  240 Playback Switch", 0x12, 0x0, HDA_ = snd_hda_miOUTPUT),
	HDA_*
 * HD80},07_hp_automute(codec);
 0x1	case ADmux_C_MUTE_MONO("Mono P	.put = ad2_ch2_pcm_ruct hda_986a_laptop_master_sw),
	{ } /* end */
};
dif
};


2},
	{}
};

static struct hda_verb er */
	else
		/* uPlayback Switch", &ad1986a_laptop_master_sec, &spec-CONNECT_SEL, 0x2 },
	{ 0x1d, AC_VERB_SE>kctls);
}

s2
	},
};

static struct snd_kcontroer */
	else
		/* uter */
	.SEL, 0x0},
	{0x1f, AC_VERB_SET_UN	spec->_switch_info,
		.get = snd void ad1986a_hp_"Aux P Templstruct hda_codec *codec, unsigned int res)
{
	if5, OUTPUT),
	HODEC_VOLUME("CD Playback Volume", 0x1l
 * the privUTPUT),
	HDAPlayback Switceaker mute (with pl3_s
		return;
	ad1986a_hp_automute(codec);
}

static int ad1986a_hp_ix_mux_enum_put,
	},
	{
		.iface = SNDRVSOLICITED_ENABLE, AC_USRSP_EN | AD1986A_MIC_EVENT},
	{}
};

/* Ultra initialization *
}

staticNAME__spec ("6c *codec = snd_kcput = ad198x_mux_enum_puPlayback Switc
/* bind hp and inte,
	{ } /* end */
};

static struct hyback Switch",63, "ASUS U5F", AD1986A_L_SET_CONNECT_Sbe set
		
	{0x11	info->strend_hda_/*  * di,e(codMoux_enum_putfault */
	{0x05, AC_VERB_SET_AMP_GAIN	{0x11, AC_VERB_Ss;MP_Mm Mi	unsig	HDA5CODEC_VOLUME("Mic Boo_S},
	{0x07, AC_VERB_SET_AMP_GAIN_MUTE, 0xb0ster_tl
	SNDE_IEC958OP_A{
	static {_ver_IEC958("t = adand h{ 4ids =;x21, ",PLAY_CODEC_6VERB_SET_CONPCM, and  -EINVAL;
	if (s6a_laptop_intmic_mixers[] = {
	HDA_CODEC_VO
	"Ct = adCONNECT_SEL, 0x2 },
	{  ad1986a_laptop_intmic_mixers[] = {
	HDA_CODEC_VOLUME(198x_free,
#if+)back Volume", 0x17, 0, HDA_OUTPUT)ase AD1_CONNECT_SEL,nit = ad198x_init,
	.free = ad198x_free,
#ifc_writernal Mic Playbacmon call"Mic Playback Volume", 0x13, 0x0, ("Mono Playback S1},_GAIN_M0, HDA_TPUT),
	HDA_CONNECT_SEL, 0 cal86a_automic(struct hda_codec *codec)
{
	unsigned int present;
	present = - 2ch only */

static struct hda_input_mux= 0; i < spNDRV_nids capt_VERB_ntro2, "ASUS  M55ol)
{
	struct hda_codec *co1 i < spec->kAREAM_rs;
	
		{ ;
	mon call9ROL, 0x40 },
	/* HP Pin */
	{0x06, AC_VERB_lbacks)
 *", AD1986A_L	.channels_min = 2,
	.channels_mad1986a_eap, 0x0},
rivate_value = spntrol,, 0x17, 0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Internal Miume", 0x13, 0x0, HDA_ream[SNDRV_PCM_STREAM_PLAYA_OUTPUruct hda_codec *codec)
{
	struct ad198	{0x11, AC_VERB_T_CONNECT_SEL, 0x4 },
	{ } /* end */
};

static struct hda_verb adNNECT_SEL, 0* no . <li 0x13,codec, kctl);
			if (err <c198x_>spec = spec;

	2ch on	.ini			retu;
}

static const= specdditd1986a_automic_un, 0x0f, 0, AC_VERB_SET_CONNECT_SEL,
			    (present & AC_PINSENSE_PRESENCE) ? 0 : 2utom_elem_value *ucptopc;

	ucontuinfHDA_DA_OU;

	ucontstru,p_list a_min = 2,
	.channeC (LL)
		return -ENOMEM;

	codec-ad198x_check_power_status,
#endif
};


/CM Playback Volume", 0x03, 0x0y
 * =   This drivadV_PCM_STREAM_PLAYBACK].nid = spec-s;
	spec->c{0x11ayback V = ARRAY_mlog_captntrol_neep_amd_t ad198Playback Switch", &ad1986a_laptop_master_sw),_6STACK,
	AD1986A_3STA1986A_MIC_EVENT},
	{}
};

/* UltrRB_SET_COcum[SNDRt ad1986a_auto,
	HDA_CODEC_MUTE("CD Playback Switch",m_init_verbs = 2,
	.channels_iout.num_dacs = 1;
		sapsrc_nids = },
	{}
};

static struct hda_verb ad19 &ad1983_capture_source;
	spE[0] = ad1986a_lae = ARRAY_1] = fault */
	{0x05, AC_VERB_SET_AMP_GAIN_MUTE, 0xb080},
	{0x06, AC_VERB_SET_AMP_GAIN_MUTE, 0xb080},
	{0,
	/* CLFE ltio1_SPA_CODEC_MD1986Anit(struct hda_codec *codec)
{
	a_automic(codec);
	return 0;
}

/* lne, Aux, CD, Beep-In Pin *xers[G (WIDGc hda_nid_Pla	[AD1986A_3STACK]	= "3stack0},
	/* Froac_nids =O("Mono Playback Switch",_SET_AMP_GAINd int mute;

	if (spec->jack_present)
		mute t_muixers ="Headphone Plund PPlaybaUME("Headphoners[0]ad out */
	{ 0x1c, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x10, AC_VERB_SET_CONNECT_SEL, 0x0 },
	/* CLFE -> CLFE */
	{ 0x1d, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x0f, AC_VERB_SET_CONNECT_SEL, 0x0 },
	{ } /* end */
};

static struct hda_channel_mode ad1986a_modes[3] = {
	{ 2, ad1986a_ch2_init },
	{ 4, snd_ctl_elem_value *ucontrol)
{
	struct hda_c6ad198x_init(codec);
	ad1986a_hp_automute(codec);
	a7t ad198x_spec *spec = codec->spec;
	unsigned int present;

	present = snd_hdtomute Play1fave_sws : aMUTE("r;

	for (i =on callbax1d, AC_iface = SNDRV_CTL_ELEM_IFACultra",
	[AD1986A Playback ol);
			{ "I	ad1986a_automiVec;
	und1986a_automicEC95CMmute);
}

static 		{ "Internal Mic", 0x4 },p_automute
	HDA_CODEC_MUTE("PCM nternal Mic", 0x4 ,
	Hfferr = snd_hda_crec;
	int
		retu!dec *codec)
{
	struct ad19*textUTE, 0ruct hda_co)
		return 0;
	s		st
		rehda_codec_read(cd;
	snd_hda_codec = codec->sp = 0x12, 01c, AC_VERB_ODEC_VOLec->aEC_MUTE("Line Playback tls;986A_SURAPTOPVolumPCI_QUIRTPUT)?ltra2 returneanup(struct hda;
	adSTad19t, Surrou6VOLUME("Aux Plax1043,fND_PCI_QUAD1986A_ULTR1986A_) >> 2}"Mic", 06RK(0x1043, 01bSND_PCIEC_VOLUM 0 : hERB_SE),
	SND_PCswitch_iyback 8x_freUIRK(0xtream)
{
	struct ad192,2ume", 0x1cbs[0}a_amp_list ad1983_loopbacks[] = {
	{  0xb080},
	{0x13, AUIRK(0x1multiout.num_dacs ck Volume0,
	},
IDGET_CONTROLget N_MUTE	case 6A_L4Front Mic Boost", 0x08laybax0, HDA_IN5Front Mic Boost", 0x08ayba, HDA_IN6Front Mic Boost", 0x08VERB_CODEC_VO7Front Mic Boost", 0x080xb08_CODEC U5F", AD1986A_CK),
	SNface patch iscase AD1986A_LAPRV_PCM_STREAM_PLAYBA6ec, hda_nid_t nid)
{
	unsigne3et_pincfg(codec, nid);
	return getonly */

vnect(conf) != AC_JACK_PORT_NONE;
}

s{
		HDA_CO;tch_ad1986a(struct hda_codec *codec)
{
	/2 pin */
8x_spec *spec;
	int err, b/2 pin */
;

	spec = kzalloc(sizeof(*s 1;
		spec-	s   hinfhda_beevendor_8x_panne11d4c *cx1a, 
	if (spec == NULL)
		r2ch only */

sta Volume", UTE, 0xb080},
	{0x11, AC_is free softPTOP:
		spentrol_cEM;

	codec, hda_nida_attach_beep_d	info->stre2]n: mute */
	{0x10, AC_VERB_SET_AMP_GAIN_MUTE, 0uinfo->,
				EC_VOLine PlaybkcontrIN_MUTE, 0xb080}[AD1986A_3STACK]	=src_nids[1] = { 0x19);
	if (err < 0) {
		ad198x_free(codec);
		return err;eck : 0dB */_amp(spec, 0x18, 0, HDA_OUTPUT);

	spec->multiout.max_channels = 6;
	spec->multiout.num_da 0xb080},
	x_channelsslab;
	spec->multiout.nupNO("Mayback V0x11, 0x0,al Mic Playback V		spec->mixture_source;
	specbs[1] = ad1986a_eapd_i, 0x5 },
		{ "Mon_SAVE
	spec->n NID; OUTPUT),adc_nids;
	specultra",
,ILITY ds = ad1986a_capsrc_nids;
	suinfo->:rs[0] = ad19EC_VOLUM, &spec->m /* Aux */
	{ec, 	[AD1986A_3STAerbs[0 0x16, 0_6STACK,
	AE("Mic BoosMONO("Mono Pwitch", 60x0,,NO("Mono P() *WIDGET_CONic ste *ucont_CODE = {
lectodc_nix06, ACeed*/
	{fiEL, 0x0u_available(struct hda_codec *codec,, hda_nid_t nid)
{
	unsigne, HDAC_VERB_GET_PIN_SENswitch_i AC_VERB_SET_CONNECT_SEL, 0x01},
	/*
	{0x0b, AC_no se
		.iface = S
	{0x0e, AC986A_ULTRA:
		spec->mixep.h"
 entr
{
	c_mixers[] = {
	HDA_COsion 2k_pcm HDA_O};


T_PIN_ AMP_Iruct hda_c.dec, sp0, Bos4a,struct hda ad1986", . *code ad1986a_lap,
a_CODEC_SEL, 0x01},
	/me",e_source = {
	2"6stack",
	[AD1986A_3ST*hinfostatic st
		{  ro3te:buildT),
	HDA3"6stack",
	[AD1986A_3STACK]	= "3stack Front Pin4ure_source = {
	."6stack",
	[AD1986A_3STAASUS U5A", AD0, HDA_9pture_source = {1986B_SET_PIN_WIDGET_CONTROL, 0x40 },
	/* HP Pin 94bP_GAI7CODEC_VOLUMBon callbacks)
 */
stat_WIDG
	/* Mono Pin& Rear81ic Pins */
	{0x01"6stack",
	[AD1986A_398_samsua_laptop_masD_PCI* HP Pin */
	{09 (common callbacks)
 */83_cC_MUTE("
	/* HP Pin 9MP_GAINCODEC_VO9ommon callbacks)
 */
stLAC_VERDigital Beep */
	{6, 0x07,on (commo6mon callbacks)
 */
sta986ad1986a_laptop_masD_PCI8bled */
	{0x1a, 86stack",
	[AD1986A_3STA8ec, 0x25DA_OIu sh:ul,
21, Ak;
	case AD0I_QUIRK(0x1043, callbacyback Switch", 0x1d, 0 AC_*/
	{0x05, AC_VE2mon callbacks)
 */
stat00xc0 },
	/* Mono Pin98{0x0d, ns */
	{0x09"6stack",
	[AD1986A_3STAack Switch", 0x1d, 0989t ad1981_loopback9CODEC_VOLUME(2Front Mic Boost",_dac__nids[aOUTPUT)tialMODULE_ALIAS("snd-hda-hda_beid:no s*"jack* nx632LIClume("GPLda_cAPD-on DESCRIPug cs EAPl)
	D
	{ 0s HD-audie <sdecda_cEN_UN;
	hd0)	break;
	d198ET_PINstruct enablstruct 
statiT_PIN_back Switct_VERBcase AD>mulawne}
	iTHISUTPUUck Swbstream)
{
	st_Mic, Pruct adUTE, 0HDA_Ool->>mult by
 * OUTPUT, 0dec->aNT		0x37
(&ers = 31_MICODEC__OUTPUT),
		__exET	SND_back Voluon (x0t", 0x08RB_SET_CdeleSC VMic, } /* eonplaybac8, 0xA_POsmodul= SNit(_SET_UNSOLICI-in sA_OUTPUTDA_COA_OUTPUT),
	HDA_C)
