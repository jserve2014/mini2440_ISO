/*
 * Universal Interface for Intel High Definition Audio Codec
 *
 * HD audio interface patch for ALC 260/880/882 codecs
 *
 * Copyright (c) 2004 Kailang Yang <kailang@realtek.com.tw>
 *                    PeiSen Hou <pshou@realtek.com.tw>
 *                    Takashi Iwai <tiwai@suse.de>
 *                    Jonathan Woithe <jwoithe@physics.adelaide.edu.au>
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

#define ALC880_FRONT_EVENT		0x01
#define ALC880_DCVOL_EVENT		0x02
#define ALC880_HP_EVENT			0x04
#define ALC880_MIC_EVENT		0x08

/* ALC880 board config type */
enum {
	ALC880_3ST,
	ALC880_3ST_DIG,
	ALC880_5ST,
	ALC880_5ST_DIG,
	ALC880_W810,
	ALC880_Z71V,
	ALC880_6ST,
	ALC880_6ST_DIG,
	ALC880_F1734,
	ALC880_ASUS,
	ALC880_ASUS_DIG,
	ALC880_ASUS_W1V,
	ALC880_ASUS_DIG2,
	ALC880_FUJITSU,
	ALC880_UNIWILL_DIG,
	ALC880_UNIWILL,
	ALC880_UNIWILL_P53,
	ALC880_CLEVO,
	ALC880_TCL_S700,
	ALC880_LG,
	ALC880_LG_LW,
	ALC880_MEDION_RIM,
#ifdef CONFIG_SND_DEBUG
	ALC880_TEST,
#endif
	ALC880_AUTO,
	ALC880_MODEL_LAST /* last tag */
};

/* ALC260 models */
enum {
	ALC260_BASIC,
	ALC260_HP,
	ALC260_HP_DC7600,
	ALC260_HP_3013,
	ALC260_FUJITSU_S702X,
	ALC260_ACER,
	ALC260_WILL,
	ALC260_REPLACER_672V,
	ALC260_FAVORIT100,
#ifdef CONFIG_SND_DEBUG
	ALC260_TEST,
#endif
	ALC260_AUTO,
	ALC260_MODEL_LAST /* last tag */
};

/* ALC262 models */
enum {
	ALC262_BASIC,
	ALC262_HIPPO,
	ALC262_HIPPO_1,
	ALC262_FUJITSU,
	ALC262_HP_BPC,
	ALC262_HP_BPC_D7000_WL,
	ALC262_HP_BPC_D7000_WF,
	ALC262_HP_TC_T5735,
	ALC262_HP_RP5700,
	ALC262_BENQ_ED8,
	ALC262_SONY_ASSAMD,
	ALC262_BENQ_T31,
	ALC262_ULTRA,
	ALC262_LENOVO_3000,
	ALC262_NEC,
	ALC262_TOSHIBA_S06,
	ALC262_TOSHIBA_RX1,
	ALC262_TYAN,
	ALC262_AUTO,
	ALC262_MODEL_LAST /* last tag */
};

/* ALC268 models */
enum {
	ALC267_QUANTA_IL1,
	ALC268_3ST,
	ALC268_TOSHIBA,
	ALC268_ACER,
	ALC268_ACER_DMIC,
	ALC268_ACER_ASPIRE_ONE,
	ALC268_DELL,
	ALC268_ZEPTO,
#ifdef CONFIG_SND_DEBUG
	ALC268_TEST,
#endif
	ALC268_AUTO,
	ALC268_MODEL_LAST /* last tag */
};

/* ALC269 models */
enum {
	ALC269_BASIC,
	ALC269_QUANTA_FL1,
	ALC269_ASUS_EEEPC_P703,
	ALC269_ASUS_EEEPC_P901,
	ALC269_FUJITSU,
	ALC269_LIFEBOOK,
	ALC269_AUTO,
	ALC269_MODEL_LAST /* last tag */
};

/* ALC861 models */
enum {
	ALC861_3ST,
	ALC660_3ST,
	ALC861_3ST_DIG,
	ALC861_6ST_DIG,
	ALC861_UNIWILL_M31,
	ALC861_TOSHIBA,
	ALC861_ASUS,
	ALC861_ASUS_LAPTOP,
	ALC861_AUTO,
	ALC861_MODEL_LAST,
};

/* ALC861-VD models */
enum {
	ALC660VD_3ST,
	ALC660VD_3ST_DIG,
	ALC660VD_ASUS_V1S,
	ALC861VD_3ST,
	ALC861VD_3ST_DIG,
	ALC861VD_6ST_DIG,
	ALC861VD_LENOVO,
	ALC861VD_DALLAS,
	ALC861VD_HP,
	ALC861VD_AUTO,
	ALC861VD_MODEL_LAST,
};

/* ALC662 models */
enum {
	ALC662_3ST_2ch_DIG,
	ALC662_3ST_6ch_DIG,
	ALC662_3ST_6ch,
	ALC662_5ST_DIG,
	ALC662_LENOVO_101E,
	ALC662_ASUS_EEEPC_P701,
	ALC662_ASUS_EEEPC_EP20,
	ALC663_ASUS_M51VA,
	ALC663_ASUS_G71V,
	ALC663_ASUS_H13,
	ALC663_ASUS_G50V,
	ALC662_ECS,
	ALC663_ASUS_MODE1,
	ALC662_ASUS_MODE2,
	ALC663_ASUS_MODE3,
	ALC663_ASUS_MODE4,
	ALC663_ASUS_MODE5,
	ALC663_ASUS_MODE6,
	ALC272_DELL,
	ALC272_DELL_ZM1,
	ALC272_SAMSUNG_NC10,
	ALC662_AUTO,
	ALC662_MODEL_LAST,
};

/* ALC882 models */
enum {
	ALC882_3ST_DIG,
	ALC882_6ST_DIG,
	ALC882_ARIMA,
	ALC882_W2JC,
	ALC882_TARGA,
	ALC882_ASUS_A7J,
	ALC882_ASUS_A7M,
	ALC885_MACPRO,
	ALC885_MBP3,
	ALC885_MB5,
	ALC885_IMAC24,
	ALC883_3ST_2ch_DIG,
	ALC883_3ST_6ch_DIG,
	ALC883_3ST_6ch,
	ALC883_6ST_DIG,
	ALC883_TARGA_DIG,
	ALC883_TARGA_2ch_DIG,
	ALC883_TARGA_8ch_DIG,
	ALC883_ACER,
	ALC883_ACER_ASPIRE,
	ALC888_ACER_ASPIRE_4930G,
	ALC888_ACER_ASPIRE_6530G,
	ALC888_ACER_ASPIRE_8930G,
	ALC888_ACER_ASPIRE_7730G,
	ALC883_MEDION,
	ALC883_MEDION_MD2,
	ALC883_LAPTOP_EAPD,
	ALC883_LENOVO_101E_2ch,
	ALC883_LENOVO_NB0763,
	ALC888_LENOVO_MS7195_DIG,
	ALC888_LENOVO_SKY,
	ALC883_HAIER_W66,
	ALC888_3ST_HP,
	ALC888_6ST_DELL,
	ALC883_MITAC,
	ALC883_CLEVO_M540R,
	ALC883_CLEVO_M720,
	ALC883_FUJITSU_PI2515,
	ALC888_FUJITSU_XA3530,
	ALC883_3ST_6ch_INTEL,
	ALC889A_INTEL,
	ALC889_INTEL,
	ALC888_ASUS_M90V,
	ALC888_ASUS_EEE1601,
	ALC889A_MB31,
	ALC1200_ASUS_P5Q,
	ALC883_SONY_VAIO_TT,
	ALC882_AUTO,
	ALC882_MODEL_LAST,
};

/* for GPIO Poll */
#define GPIO_MASK	0x03

/* extra amp-initialization sequence types */
enum {
	ALC_INIT_NONE,
	ALC_INIT_DEFAULT,
	ALC_INIT_GPIO1,
	ALC_INIT_GPIO2,
	ALC_INIT_GPIO3,
};

struct alc_mic_route {
	hda_nid_t pin;
	unsigned char mux_idx;
	unsigned char amix_idx;
};

#define MUX_IDX_UNDEF	((unsigned char)-1)

struct alc_spec {
	/* codec parameterization */
	struct snd_kcontrol_new *mixers[5];	/* mixer arrays */
	unsigned int num_mixers;
	struct snd_kcontrol_new *cap_mixer;	/* capture mixer */
	unsigned int beep_amp;	/* beep amp value, set via set_beep_amp() */

	const struct hda_verb *init_verbs[10];	/* initialization verbs
						 * don't forget NULL
						 * termination!
						 */
	unsigned int num_init_verbs;

	char stream_name_analog[32];	/* analog PCM stream */
	struct hda_pcm_stream *stream_analog_playback;
	struct hda_pcm_stream *stream_analog_capture;
	struct hda_pcm_stream *stream_analog_alt_playback;
	struct hda_pcm_stream *stream_analog_alt_capture;

	char stream_name_digital[32];	/* digital PCM stream */
	struct hda_pcm_stream *stream_digital_playback;
	struct hda_pcm_stream *stream_digital_capture;

	/* playback */
	struct hda_multi_out multiout;	/* playback set-up
					 * max_channels, dacs must be set
					 * dig_out_nid and hp_nid are optional
					 */
	hda_nid_t alt_dac_nid;
	hda_nid_t slave_dig_outs[3];	/* optional - for auto-parsing */
	int dig_out_type;

	/* capture */
	unsigned int num_adc_nids;
	hda_nid_t *adc_nids;
	hda_nid_t *capsrc_nids;
	hda_nid_t dig_in_nid;		/* digital-in NID; optional */

	/* capture source */
	unsigned int num_mux_defs;
	const struct hda_input_mux *input_mux;
	unsigned int cur_mux[3];
	struct alc_mic_route ext_mic;
	struct alc_mic_route int_mic;

	/* channel model */
	const struct hda_channel_mode *channel_mode;
	int num_channel_mode;
	int need_dac_fix;
	int const_channel_count;
	int ext_channel_count;

	/* PCM information */
	struct hda_pcm pcm_rec[3];	/* used in alc_build_pcms() */

	/* dynamic controls, init_verbs and input_mux */
	struct auto_pin_cfg autocfg;
	struct snd_array kctls;
	struct hda_input_mux private_imux[3];
	hda_nid_t private_dac_nids[AUTO_CFG_MAX_OUTS];
	hda_nid_t private_adc_nids[AUTO_CFG_MAX_OUTS];
	hda_nid_t private_capsrc_nids[AUTO_CFG_MAX_OUTS];

	/* hooks */
	void (*init_hook)(struct hda_codec *codec);
	void (*unsol_event)(struct hda_codec *codec, unsigned int res);

	/* for pin sensing */
	unsigned int sense_updated: 1;
	unsigned int jack_present: 1;
	unsigned int master_sw: 1;
	unsigned int auto_mic:1;

	/* other flags */
	unsigned int no_analog :1; /* digital I/O only */
	int init_amp;

	/* for virtual master */
	hda_nid_t vmaster_nid;
#ifdef CONFIG_SND_HDA_POWER_SAVE
	struct hda_loopback_check loopback;
#endif

	/* for PLL fix */
	hda_nid_t pll_nid;
	unsigned int pll_coef_idx, pll_coef_bit;
};

/*
 * configuration template - to be copied to the spec instance
 */
struct alc_config_preset {
	struct snd_kcontrol_new *mixers[5]; /* should be identical size
					     * with spec
					     */
	struct snd_kcontrol_new *cap_mixer; /* capture mixer */
	const struct hda_verb *init_verbs[5];
	unsigned int num_dacs;
	hda_nid_t *dac_nids;
	hda_nid_t dig_out_nid;		/* optional */
	hda_nid_t hp_nid;		/* optional */
	hda_nid_t *slave_dig_outs;
	unsigned int num_adc_nids;
	hda_nid_t *adc_nids;
	hda_nid_t *capsrc_nids;
	hda_nid_t dig_in_nid;
	unsigned int num_channel_mode;
	const struct hda_channel_mode *channel_mode;
	int need_dac_fix;
	int const_channel_count;
	unsigned int num_mux_defs;
	const struct hda_input_mux *input_mux;
	void (*unsol_event)(struct hda_codec *, unsigned int);
	void (*setup)(struct hda_codec *);
	void (*init_hook)(struct hda_codec *);
#ifdef CONFIG_SND_HDA_POWER_SAVE
	struct hda_amp_list *loopbacks;
#endif
};


/*
 * input MUX handling
 */
static int alc_mux_enum_info(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_info *uinfo)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct alc_spec *spec = codec->spec;
	unsigned int mux_idx = snd_ctl_get_ioffidx(kcontrol, &uinfo->id);
	if (mux_idx >= spec->num_mux_defs)
		mux_idx = 0;
	return snd_hda_input_mux_info(&spec->input_mux[mux_idx], uinfo);
}

static int alc_mux_enum_get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct alc_spec *spec = codec->spec;
	unsigned int adc_idx = snd_ctl_get_ioffidx(kcontrol, &ucontrol->id);

	ucontrol->value.enumerated.item[0] = spec->cur_mux[adc_idx];
	return 0;
}

static int alc_mux_enum_put(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct alc_spec *spec = codec->spec;
	const struct hda_input_mux *imux;
	unsigned int adc_idx = snd_ctl_get_ioffidx(kcontrol, &ucontrol->id);
	unsigned int mux_idx;
	hda_nid_t nid = spec->capsrc_nids ?
		spec->capsrc_nids[adc_idx] : spec->adc_nids[adc_idx];
	unsigned int type;

	mux_idx = adc_idx >= spec->num_mux_defs ? 0 : adc_idx;
	imux = &spec->input_mux[mux_idx];

	type = get_wcaps_type(get_wcaps(codec, nid));
	if (type == AC_WID_AUD_MIX) {
		/* Matrix-mixer style (e.g. ALC882) */
		unsigned int *cur_val = &spec->cur_mux[adc_idx];
		unsigned int i, idx;

		idx = ucontrol->value.enumerated.item[0];
		if (idx >= imux->num_items)
			idx = imux->num_items - 1;
		if (*cur_val == idx)
			return 0;
		for (i = 0; i < imux->num_items; i++) {
			unsigned int v = (i == idx) ? 0 : HDA_AMP_MUTE;
			snd_hda_codec_amp_stereo(codec, nid, HDA_INPUT,
						 imux->items[i].index,
						 HDA_AMP_MUTE, v);
		}
		*cur_val = idx;
		return 1;
	} else {
		/* MUX style (e.g. ALC880) */
		return snd_hda_input_mux_put(codec, imux, ucontrol, nid,
					     &spec->cur_mux[adc_idx]);
	}
}

/*
 * channel mode setting
 */
static int alc_ch_mode_info(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_info *uinfo)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct alc_spec *spec = codec->spec;
	return snd_hda_ch_mode_info(codec, uinfo, spec->channel_mode,
				    spec->num_channel_mode);
}

static int alc_ch_mode_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct alc_spec *spec = codec->spec;
	return snd_hda_ch_mode_get(codec, ucontrol, spec->channel_mode,
				   spec->num_channel_mode,
				   spec->ext_channel_count);
}

static int alc_ch_mode_put(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct alc_spec *spec = codec->spec;
	int err = snd_hda_ch_mode_put(codec, ucontrol, spec->channel_mode,
				      spec->num_channel_mode,
				      &spec->ext_channel_count);
	if (err >= 0 && !spec->const_channel_count) {
		spec->multiout.max_channels = spec->ext_channel_count;
		if (spec->need_dac_fix)
			spec->multiout.num_dacs = spec->multiout.max_channels / 2;
	}
	return err;
}

/*
 * Control the mode of pin widget settings via the mixer.  "pc" is used
 * instead of "%" to avoid consequences of accidently treating the % as
 * being part of a format specifier.  Maximum allowed length of a value is
 * 63 characters plus NULL terminator.
 *
 * Note: some retasking pin complexes seem to ignore requests for input
 * states other than HiZ (eg: PIN_VREFxx) and revert to HiZ if any of these
 * are requested.  Therefore order this list so that this behaviour will not
 * cause problems when mixer clients move through the enum sequentially.
 * NIDs 0x0f and 0x10 have been observed to have this behaviour as of
 * March 2006.
 */
static char *alc_pin_mode_names[] = {
	"Mic 50pc bias", "Mic 80pc bias",
	"Line in", "Line out", "Headphone out",
};
static unsigned char alc_pin_mode_values[] = {
	PIN_VREF50, PIN_VREF80, PIN_IN, PIN_OUT, PIN_HP,
};
/* The control can present all 5 options, or it can limit the options based
 * in the pin being assumed to be exclusively an input or an output pin.  In
 * addition, "input" pins may or may not process the mic bias option
 * depending on actual widget capability (NIDs 0x0f and 0x10 don't seem to
 * accept requests for bias as of chip versions up to March 2006) and/or
 * wiring in the computer.
 */
#define ALC_PIN_DIR_IN              0x00
#define ALC_PIN_DIR_OUT             0x01
#define ALC_PIN_DIR_INOUT           0x02
#define ALC_PIN_DIR_IN_NOMICBIAS    0x03
#define ALC_PIN_DIR_INOUT_NOMICBIAS 0x04

/* Info about the pin modes supported by the different pin direction modes.
 * For each direction the minimum and maximum values are given.
 */
static signed char alc_pin_mode_dir_info[5][2] = {
	{ 0, 2 },    /* ALC_PIN_DIR_IN */
	{ 3, 4 },    /* ALC_PIN_DIR_OUT */
	{ 0, 4 },    /* ALC_PIN_DIR_INOUT */
	{ 2, 2 },    /* ALC_PIN_DIR_IN_NOMICBIAS */
	{ 2, 4 },    /* ALC_PIN_DIR_INOUT_NOMICBIAS */
};
#define alc_pin_mode_min(_dir) (alc_pin_mode_dir_info[_dir][0])
#define alc_pin_mode_max(_dir) (alc_pin_mode_dir_info[_dir][1])
#define alc_pin_mode_n_items(_dir) \
	(alc_pin_mode_max(_dir)-alc_pin_mode_min(_dir)+1)

static int alc_pin_mode_info(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_info *uinfo)
{
	unsigned int item_num = uinfo->value.enumerated.item;
	unsigned char dir = (kcontrol->private_value >> 16) & 0xff;

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = alc_pin_mode_n_items(dir);

	if (item_num<alc_pin_mode_min(dir) || item_num>alc_pin_mode_max(dir))
		item_num = alc_pin_mode_min(dir);
	strcpy(uinfo->value.enumerated.name, alc_pin_mode_names[item_num]);
	return 0;
}

static int alc_pin_mode_get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	unsigned int i;
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = kcontrol->private_value & 0xffff;
	unsigned char dir = (kcontrol->private_value >> 16) & 0xff;
	long *valp = ucontrol->value.integer.value;
	unsigned int pinctl = snd_hda_codec_read(codec, nid, 0,
						 AC_VERB_GET_PIN_WIDGET_CONTROL,
						 0x00);

	/* Find enumerated value for current pinctl setting */
	i = alc_pin_mode_min(dir);
	while (i <= alc_pin_mode_max(dir) && alc_pin_mode_values[i] != pinctl)
		i++;
	*valp = i <= alc_pin_mode_max(dir) ? i: alc_pin_mode_min(dir);
	return 0;
}

static int alc_pin_mode_put(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	signed int change;
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = kcontrol->private_value & 0xffff;
	unsigned char dir = (kcontrol->private_value >> 16) & 0xff;
	long val = *ucontrol->value.integer.value;
	unsigned int pinctl = snd_hda_codec_read(codec, nid, 0,
						 AC_VERB_GET_PIN_WIDGET_CONTROL,
						 0x00);

	if (val < alc_pin_mode_min(dir) || val > alc_pin_mode_max(dir))
		val = alc_pin_mode_min(dir);

	change = pinctl != alc_pin_mode_values[val];
	if (change) {
		/* Set pin mode to that requested */
		snd_hda_codec_write_cache(codec, nid, 0,
					  AC_VERB_SET_PIN_WIDGET_CONTROL,
					  alc_pin_mode_values[val]);

		/* Also enable the retasking pin's input/output as required
		 * for the requested pin mode.  Enum values of 2 or less are
		 * input modes.
		 *
		 * Dynamically switching the input/output buffers probably
		 * reduces noise slightly (particularly on input) so we'll
		 * do it.  However, having both input and output buffers
		 * enabled simultaneously doesn't seem to be problematic if
		 * this turns out to be necessary in the future.
		 */
		if (val <= 2) {
			snd_hda_codec_amp_stereo(codec, nid, HDA_OUTPUT, 0,
						 HDA_AMP_MUTE, HDA_AMP_MUTE);
			snd_hda_codec_amp_stereo(codec, nid, HDA_INPUT, 0,
						 HDA_AMP_MUTE, 0);
		} else {
			snd_hda_codec_amp_stereo(codec, nid, HDA_INPUT, 0,
						 HDA_AMP_MUTE, HDA_AMP_MUTE);
			snd_hda_codec_amp_stereo(codec, nid, HDA_OUTPUT, 0,
						 HDA_AMP_MUTE, 0);
		}
	}
	return change;
}

#define ALC_PIN_MODE(xname, nid, dir) \
	{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .index = 0,  \
	  .info = alc_pin_mode_info, \
	  .get = alc_pin_mode_get, \
	  .put = alc_pin_mode_put, \
	  .private_value = nid | (dir<<16) }

/* A switch control for ALC260 GPIO pins.  Multiple GPIOs can be ganged
 * together using a mask with more than one bit set.  This control is
 * currently used only by the ALC260 test model.  At this stage they are not
 * needed for any "production" models.
 */
#ifdef CONFIG_SND_DEBUG
#define alc_gpio_data_info	snd_ctl_boolean_mono_info

static int alc_gpio_data_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = kcontrol->private_value & 0xffff;
	unsigned char mask = (kcontrol->private_value >> 16) & 0xff;
	long *valp = ucontrol->value.integer.value;
	unsigned int val = snd_hda_codec_read(codec, nid, 0,
					      AC_VERB_GET_GPIO_DATA, 0x00);

	*valp = (val & mask) != 0;
	return 0;
}
static int alc_gpio_data_put(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	signed int change;
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = kcontrol->private_value & 0xffff;
	unsigned char mask = (kcontrol->private_value >> 16) & 0xff;
	long val = *ucontrol->value.integer.value;
	unsigned int gpio_data = snd_hda_codec_read(codec, nid, 0,
						    AC_VERB_GET_GPIO_DATA,
						    0x00);

	/* Set/unset the masked GPIO bit(s) as needed */
	change = (val == 0 ? 0 : mask) != (gpio_data & mask);
	if (val == 0)
		gpio_data &= ~mask;
	else
		gpio_data |= mask;
	snd_hda_codec_write_cache(codec, nid, 0,
				  AC_VERB_SET_GPIO_DATA, gpio_data);

	return change;
}
#define ALC_GPIO_DATA_SWITCH(xname, nid, mask) \
	{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .index = 0,  \
	  .info = alc_gpio_data_info, \
	  .get = alc_gpio_data_get, \
	  .put = alc_gpio_data_put, \
	  .private_value = nid | (mask<<16) }
#endif   /* CONFIG_SND_DEBUG */

/* A switch control to allow the enabling of the digital IO pins on the
 * ALC260.  This is incredibly simplistic; the intention of this control is
 * to provide something in the test model allowing digital outputs to be
 * identified if present.  If models are found which can utilise these
 * outputs a more complete mixer control can be devised for those models if
 * necessary.
 */
#ifdef CONFIG_SND_DEBUG
#define alc_spdif_ctrl_info	snd_ctl_boolean_mono_info

static int alc_spdif_ctrl_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = kcontrol->private_value & 0xffff;
	unsigned char mask = (kcontrol->private_value >> 16) & 0xff;
	long *valp = ucontrol->value.integer.value;
	unsigned int val = snd_hda_codec_read(codec, nid, 0,
					      AC_VERB_GET_DIGI_CONVERT_1, 0x00);

	*valp = (val & mask) != 0;
	return 0;
}
static int alc_spdif_ctrl_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	signed int change;
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = kcontrol->private_value & 0xffff;
	unsigned char mask = (kcontrol->private_value >> 16) & 0xff;
	long val = *ucontrol->value.integer.value;
	unsigned int ctrl_data = snd_hda_codec_read(codec, nid, 0,
						    AC_VERB_GET_DIGI_CONVERT_1,
						    0x00);

	/* Set/unset the masked control bit(s) as needed */
	change = (val == 0 ? 0 : mask) != (ctrl_data & mask);
	if (val==0)
		ctrl_data &= ~mask;
	else
		ctrl_data |= mask;
	snd_hda_codec_write_cache(codec, nid, 0, AC_VERB_SET_DIGI_CONVERT_1,
				  ctrl_data);

	return change;
}
#define ALC_SPDIF_CTRL_SWITCH(xname, nid, mask) \
	{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .index = 0,  \
	  .info = alc_spdif_ctrl_info, \
	  .get = alc_spdif_ctrl_get, \
	  .put = alc_spdif_ctrl_put, \
	  .private_value = nid | (mask<<16) }
#endif   /* CONFIG_SND_DEBUG */

/* A switch control to allow the enabling EAPD digital outputs on the ALC26x.
 * Again, this is only used in the ALC26x test models to help identify when
 * the EAPD line must be asserted for features to work.
 */
#ifdef CONFIG_SND_DEBUG
#define alc_eapd_ctrl_info	snd_ctl_boolean_mono_info

static int alc_eapd_ctrl_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = kcontrol->private_value & 0xffff;
	unsigned char mask = (kcontrol->private_value >> 16) & 0xff;
	long *valp = ucontrol->value.integer.value;
	unsigned int val = snd_hda_codec_read(codec, nid, 0,
					      AC_VERB_GET_EAPD_BTLENABLE, 0x00);

	*valp = (val & mask) != 0;
	return 0;
}

static int alc_eapd_ctrl_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	int change;
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = kcontrol->private_value & 0xffff;
	unsigned char mask = (kcontrol->private_value >> 16) & 0xff;
	long val = *ucontrol->value.integer.value;
	unsigned int ctrl_data = snd_hda_codec_read(codec, nid, 0,
						    AC_VERB_GET_EAPD_BTLENABLE,
						    0x00);

	/* Set/unset the masked control bit(s) as needed */
	change = (!val ? 0 : mask) != (ctrl_data & mask);
	if (!val)
		ctrl_data &= ~mask;
	else
		ctrl_data |= mask;
	snd_hda_codec_write_cache(codec, nid, 0, AC_VERB_SET_EAPD_BTLENABLE,
				  ctrl_data);

	return change;
}

#define ALC_EAPD_CTRL_SWITCH(xname, nid, mask) \
	{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .index = 0,  \
	  .info = alc_eapd_ctrl_info, \
	  .get = alc_eapd_ctrl_get, \
	  .put = alc_eapd_ctrl_put, \
	  .private_value = nid | (mask<<16) }
#endif   /* CONFIG_SND_DEBUG */

/*
 * set up the input pin config (depending on the given auto-pin type)
 */
static void alc_set_input_pin(struct hda_codec *codec, hda_nid_t nid,
			      int auto_pin_type)
{
	unsigned int val = PIN_IN;

	if (auto_pin_type <= AUTO_PIN_FRONT_MIC) {
		unsigned int pincap;
		pincap = snd_hda_query_pin_caps(codec, nid);
		pincap = (pincap & AC_PINCAP_VREF) >> AC_PINCAP_VREF_SHIFT;
		if (pincap & AC_PINCAP_VREF_80)
			val = PIN_VREF80;
		else if (pincap & AC_PINCAP_VREF_50)
			val = PIN_VREF50;
		else if (pincap & AC_PINCAP_VREF_100)
			val = PIN_VREF100;
		else if (pincap & AC_PINCAP_VREF_GRD)
			val = PIN_VREFGRD;
	}
	snd_hda_codec_write(codec, nid, 0, AC_VERB_SET_PIN_WIDGET_CONTROL, val);
}

/*
 */
static void add_mixer(struct alc_spec *spec, struct snd_kcontrol_new *mix)
{
	if (snd_BUG_ON(spec->num_mixers >= ARRAY_SIZE(spec->mixers)))
		return;
	spec->mixers[spec->num_mixers++] = mix;
}

static void add_verb(struct alc_spec *spec, const struct hda_verb *verb)
{
	if (snd_BUG_ON(spec->num_init_verbs >= ARRAY_SIZE(spec->init_verbs)))
		return;
	spec->init_verbs[spec->num_init_verbs++] = verb;
}

#ifdef CONFIG_PROC_FS
/*
 * hook for proc
 */
static void print_realtek_coef(struct snd_info_buffer *buffer,
			       struct hda_codec *codec, hda_nid_t nid)
{
	int coeff;

	if (nid != 0x20)
		return;
	coeff = snd_hda_codec_read(codec, nid, 0, AC_VERB_GET_PROC_COEF, 0);
	snd_iprintf(buffer, "  Processing Coefficient: 0x%02x\n", coeff);
	coeff = snd_hda_codec_read(codec, nid, 0,
				   AC_VERB_GET_COEF_INDEX, 0);
	snd_iprintf(buffer, "  Coefficient Index: 0x%02x\n", coeff);
}
#else
#define print_realtek_coef	NULL
#endif

/*
 * set up from the preset table
 */
static void setup_preset(struct hda_codec *codec,
			 const struct alc_config_preset *preset)
{
	struct alc_spec *spec = codec->spec;
	int i;

	for (i = 0; i < ARRAY_SIZE(preset->mixers) && preset->mixers[i]; i++)
		add_mixer(spec, preset->mixers[i]);
	spec->cap_mixer = preset->cap_mixer;
	for (i = 0; i < ARRAY_SIZE(preset->init_verbs) && preset->init_verbs[i];
	     i++)
		add_verb(spec, preset->init_verbs[i]);

	spec->channel_mode = preset->channel_mode;
	spec->num_channel_mode = preset->num_channel_mode;
	spec->need_dac_fix = preset->need_dac_fix;
	spec->const_channel_count = preset->const_channel_count;

	if (preset->const_channel_count)
		spec->multiout.max_channels = preset->const_channel_count;
	else
		spec->multiout.max_channels = spec->channel_mode[0].channels;
	spec->ext_channel_count = spec->channel_mode[0].channels;

	spec->multiout.num_dacs = preset->num_dacs;
	spec->multiout.dac_nids = preset->dac_nids;
	spec->multiout.dig_out_nid = preset->dig_out_nid;
	spec->multiout.slave_dig_outs = preset->slave_dig_outs;
	spec->multiout.hp_nid = preset->hp_nid;

	spec->num_mux_defs = preset->num_mux_defs;
	if (!spec->num_mux_defs)
		spec->num_mux_defs = 1;
	spec->input_mux = preset->input_mux;

	spec->num_adc_nids = preset->num_adc_nids;
	spec->adc_nids = preset->adc_nids;
	spec->capsrc_nids = preset->capsrc_nids;
	spec->dig_in_nid = preset->dig_in_nid;

	spec->unsol_event = preset->unsol_event;
	spec->init_hook = preset->init_hook;
#ifdef CONFIG_SND_HDA_POWER_SAVE
	spec->loopback.amplist = preset->loopbacks;
#endif

	if (preset->setup)
		preset->setup(codec);
}

/* Enable GPIO mask and set output */
static struct hda_verb alc_gpio1_init_verbs[] = {
	{0x01, AC_VERB_SET_GPIO_MASK, 0x01},
	{0x01, AC_VERB_SET_GPIO_DIRECTION, 0x01},
	{0x01, AC_VERB_SET_GPIO_DATA, 0x01},
	{ }
};

static struct hda_verb alc_gpio2_init_verbs[] = {
	{0x01, AC_VERB_SET_GPIO_MASK, 0x02},
	{0x01, AC_VERB_SET_GPIO_DIRECTION, 0x02},
	{0x01, AC_VERB_SET_GPIO_DATA, 0x02},
	{ }
};

static struct hda_verb alc_gpio3_init_verbs[] = {
	{0x01, AC_VERB_SET_GPIO_MASK, 0x03},
	{0x01, AC_VERB_SET_GPIO_DIRECTION, 0x03},
	{0x01, AC_VERB_SET_GPIO_DATA, 0x03},
	{ }
};

/*
 * Fix hardware PLL issue
 * On some codecs, the analog PLL gating control must be off while
 * the default value is 1.
 */
static void alc_fix_pll(struct hda_codec *codec)
{
	struct alc_spec *spec = codec->spec;
	unsigned int val;

	if (!spec->pll_nid)
		return;
	snd_hda_codec_write(codec, spec->pll_nid, 0, AC_VERB_SET_COEF_INDEX,
			    spec->pll_coef_idx);
	val = snd_hda_codec_read(codec, spec->pll_nid, 0,
				 AC_VERB_GET_PROC_COEF, 0);
	snd_hda_codec_write(codec, spec->pll_nid, 0, AC_VERB_SET_COEF_INDEX,
			    spec->pll_coef_idx);
	snd_hda_codec_write(codec, spec->pll_nid, 0, AC_VERB_SET_PROC_COEF,
			    val & ~(1 << spec->pll_coef_bit));
}

static void alc_fix_pll_init(struct hda_codec *codec, hda_nid_t nid,
			     unsigned int coef_idx, unsigned int coef_bit)
{
	struct alc_spec *spec = codec->spec;
	spec->pll_nid = nid;
	spec->pll_coef_idx = coef_idx;
	spec->pll_coef_bit = coef_bit;
	alc_fix_pll(codec);
}

static void alc_automute_pin(struct hda_codec *codec)
{
	struct alc_spec *spec = codec->spec;
	unsigned int present, pincap;
	unsigned int nid = spec->autocfg.hp_pins[0];
	int i;

	if (!nid)
		return;
	pincap = snd_hda_query_pin_caps(codec, nid);
	if (pincap & AC_PINCAP_TRIG_REQ) /* need trigger? */
		snd_hda_codec_read(codec, nid, 0, AC_VERB_SET_PIN_SENSE, 0);
	present = snd_hda_codec_read(codec, nid, 0,
				     AC_VERB_GET_PIN_SENSE, 0);
	spec->jack_present = (present & AC_PINSENSE_PRESENCE) != 0;
	for (i = 0; i < ARRAY_SIZE(spec->autocfg.speaker_pins); i++) {
		nid = spec->autocfg.speaker_pins[i];
		if (!nid)
			break;
		snd_hda_codec_write(codec, nid, 0,
				    AC_VERB_SET_PIN_WIDGET_CONTROL,
				    spec->jack_present ? 0 : PIN_OUT);
	}
}

static int get_connection_index(struct hda_codec *codec, hda_nid_t mux,
				hda_nid_t nid)
{
	hda_nid_t conn[HDA_MAX_NUM_INPUTS];
	int i, nums;

	nums = snd_hda_get_connections(codec, mux, conn, ARRAY_SIZE(conn));
	for (i = 0; i < nums; i++)
		if (conn[i] == nid)
			return i;
	return -1;
}

static void alc_mic_automute(struct hda_codec *codec)
{
	struct alc_spec *spec = codec->spec;
	struct alc_mic_route *dead, *alive;
	unsigned int present, type;
	hda_nid_t cap_nid;

	if (!spec->auto_mic)
		return;
	if (!spec->int_mic.pin || !spec->ext_mic.pin)
		return;
	if (snd_BUG_ON(!spec->adc_nids))
		return;

	cap_nid = spec->capsrc_nids ? spec->capsrc_nids[0] : spec->adc_nids[0];

	present = snd_hda_codec_read(codec, spec->ext_mic.pin, 0,
				     AC_VERB_GET_PIN_SENSE, 0);
	present &= AC_PINSENSE_PRESENCE;
	if (present) {
		alive = &spec->ext_mic;
		dead = &spec->int_mic;
	} else {
		alive = &spec->int_mic;
		dead = &spec->ext_mic;
	}

	type = get_wcaps_type(get_wcaps(codec, cap_nid));
	if (type == AC_WID_AUD_MIX) {
		/* Matrix-mixer style (e.g. ALC882) */
		snd_hda_codec_amp_stereo(codec, cap_nid, HDA_INPUT,
					 alive->mux_idx,
					 HDA_AMP_MUTE, 0);
		snd_hda_codec_amp_stereo(codec, cap_nid, HDA_INPUT,
					 dead->mux_idx,
					 HDA_AMP_MUTE, HDA_AMP_MUTE);
	} else {
		/* MUX style (e.g. ALC880) */
		snd_hda_codec_write_cache(codec, cap_nid, 0,
					  AC_VERB_SET_CONNECT_SEL,
					  alive->mux_idx);
	}

	/* FIXME: analog mixer */
}

/* unsolicited event for HP jack sensing */
static void alc_sku_unsol_event(struct hda_codec *codec, unsigned int res)
{
	if (codec->vendor_id == 0x10ec0880)
		res >>= 28;
	else
		res >>= 26;
	switch (res) {
	case ALC880_HP_EVENT:
		alc_automute_pin(codec);
		break;
	case ALC880_MIC_EVENT:
		alc_mic_automute(codec);
		break;
	}
}

static void alc_inithook(struct hda_codec *codec)
{
	alc_automute_pin(codec);
	alc_mic_automute(codec);
}

/* additional initialization for ALC888 variants */
static void alc888_coef_init(struct hda_codec *codec)
{
	unsigned int tmp;

	snd_hda_codec_write(codec, 0x20, 0, AC_VERB_SET_COEF_INDEX, 0);
	tmp = snd_hda_codec_read(codec, 0x20, 0, AC_VERB_GET_PROC_COEF, 0);
	snd_hda_codec_write(codec, 0x20, 0, AC_VERB_SET_COEF_INDEX, 7);
	if ((tmp & 0xf0) == 0x20)
		/* alc888S-VC */
		snd_hda_codec_read(codec, 0x20, 0,
				   AC_VERB_SET_PROC_COEF, 0x830);
	 else
		 /* alc888-VB */
		 snd_hda_codec_read(codec, 0x20, 0,
				    AC_VERB_SET_PROC_COEF, 0x3030);
}

static void alc889_coef_init(struct hda_codec *codec)
{
	unsigned int tmp;

	snd_hda_codec_write(codec, 0x20, 0, AC_VERB_SET_COEF_INDEX, 7);
	tmp = snd_hda_codec_read(codec, 0x20, 0, AC_VERB_GET_PROC_COEF, 0);
	snd_hda_codec_write(codec, 0x20, 0, AC_VERB_SET_COEF_INDEX, 7);
	snd_hda_codec_write(codec, 0x20, 0, AC_VERB_SET_PROC_COEF, tmp|0x2010);
}

static void alc_auto_init_amp(struct hda_codec *codec, int type)
{
	unsigned int tmp;

	switch (type) {
	case ALC_INIT_GPIO1:
		snd_hda_sequence_write(codec, alc_gpio1_init_verbs);
		break;
	case ALC_INIT_GPIO2:
		snd_hda_sequence_write(codec, alc_gpio2_init_verbs);
		break;
	case ALC_INIT_GPIO3:
		snd_hda_sequence_write(codec, alc_gpio3_init_verbs);
		break;
	case ALC_INIT_DEFAULT:
		switch (codec->vendor_id) {
		case 0x10ec0260:
			snd_hda_codec_write(codec, 0x0f, 0,
					    AC_VERB_SET_EAPD_BTLENABLE, 2);
			snd_hda_codec_write(codec, 0x10, 0,
					    AC_VERB_SET_EAPD_BTLENABLE, 2);
			break;
		case 0x10ec0262:
		case 0x10ec0267:
		case 0x10ec0268:
		case 0x10ec0269:
		case 0x10ec0272:
		case 0x10ec0660:
		case 0x10ec0662:
		case 0x10ec0663:
		case 0x10ec0862:
		case 0x10ec0889:
			snd_hda_codec_write(codec, 0x14, 0,
					    AC_VERB_SET_EAPD_BTLENABLE, 2);
			snd_hda_codec_write(codec, 0x15, 0,
					    AC_VERB_SET_EAPD_BTLENABLE, 2);
			break;
		}
		switch (codec->vendor_id) {
		case 0x10ec0260:
			snd_hda_codec_write(codec, 0x1a, 0,
					    AC_VERB_SET_COEF_INDEX, 7);
			tmp = snd_hda_codec_read(codec, 0x1a, 0,
						 AC_VERB_GET_PROC_COEF, 0);
			snd_hda_codec_write(codec, 0x1a, 0,
					    AC_VERB_SET_COEF_INDEX, 7);
			snd_hda_codec_write(codec, 0x1a, 0,
					    AC_VERB_SET_PROC_COEF,
					    tmp | 0x2010);
			break;
		case 0x10ec0262:
		case 0x10ec0880:
		case 0x10ec0882:
		case 0x10ec0883:
		case 0x10ec0885:
		case 0x10ec0887:
		case 0x10ec0889:
			alc889_coef_init(codec);
			break;
		case 0x10ec0888:
			alc888_coef_init(codec);
			break;
		case 0x10ec0267:
		case 0x10ec0268:
			snd_hda_codec_write(codec, 0x20, 0,
					    AC_VERB_SET_COEF_INDEX, 7);
			tmp = snd_hda_codec_read(codec, 0x20, 0,
						 AC_VERB_GET_PROC_COEF, 0);
			snd_hda_codec_write(codec, 0x20, 0,
					    AC_VERB_SET_COEF_INDEX, 7);
			snd_hda_codec_write(codec, 0x20, 0,
					    AC_VERB_SET_PROC_COEF,
					    tmp | 0x3000);
			break;
		}
		break;
	}
}

static void alc_init_auto_hp(struct hda_codec *codec)
{
	struct alc_spec *spec = codec->spec;

	if (!spec->autocfg.hp_pins[0])
		return;

	if (!spec->autocfg.speaker_pins[0]) {
		if (spec->autocfg.line_out_pins[0] &&
		    spec->autocfg.line_out_type == AUTO_PIN_SPEAKER_OUT)
			spec->autocfg.speaker_pins[0] =
				spec->autocfg.line_out_pins[0];
		else
			return;
	}

	snd_printdd("realtek: Enable HP auto-muting on NID 0x%x\n",
		    spec->autocfg.hp_pins[0]);
	snd_hda_codec_write_cache(codec, spec->autocfg.hp_pins[0], 0,
				  AC_VERB_SET_UNSOLICITED_ENABLE,
				  AC_USRSP_EN | ALC880_HP_EVENT);
	spec->unsol_event = alc_sku_unsol_event;
}

static void alc_init_auto_mic(struct hda_codec *codec)
{
	struct alc_spec *spec = codec->spec;
	struct auto_pin_cfg *cfg = &spec->autocfg;
	hda_nid_t fixed, ext;
	int i;

	/* there must be only two mic inputs exclusively */
	for (i = AUTO_PIN_LINE; i < AUTO_PIN_LAST; i++)
		if (cfg->input_pins[i])
			return;

	fixed = ext = 0;
	for (i = AUTO_PIN_MIC; i <= AUTO_PIN_FRONT_MIC; i++) {
		hda_nid_t nid = cfg->input_pins[i];
		unsigned int defcfg;
		if (!nid)
			return;
		defcfg = snd_hda_codec_get_pincfg(codec, nid);
		switch (get_defcfg_connect(defcfg)) {
		case AC_JACK_PORT_FIXED:
			if (fixed)
				return; /* already occupied */
			fixed = nid;
			break;
		case AC_JACK_PORT_COMPLEX:
			if (ext)
				return; /* already occupied */
			ext = nid;
			break;
		default:
			return; /* invalid entry */
		}
	}
	if (!(get_wcaps(codec, ext) & AC_WCAP_UNSOL_CAP))
		return; /* no unsol support */
	snd_printdd("realtek: Enable auto-mic switch on NID 0x%x/0x%x\n",
		    ext, fixed);
	spec->ext_mic.pin = ext;
	spec->int_mic.pin = fixed;
	spec->ext_mic.mux_idx = MUX_IDX_UNDEF; /* set later */
	spec->int_mic.mux_idx = MUX_IDX_UNDEF; /* set later */
	spec->auto_mic = 1;
	snd_hda_codec_write_cache(codec, spec->ext_mic.pin, 0,
				  AC_VERB_SET_UNSOLICITED_ENABLE,
				  AC_USRSP_EN | ALC880_MIC_EVENT);
	spec->unsol_event = alc_sku_unsol_event;
}

/* check subsystem ID and set up device-specific initialization;
 * return 1 if initialized, 0 if invalid SSID
 */
/* 32-bit subsystem ID for BIOS loading in HD Audio codec.
 *	31 ~ 16 :	Manufacture ID
 *	15 ~ 8	:	SKU ID
 *	7  ~ 0	:	Assembly ID
 *	port-A --> pin 39/41, port-E --> pin 14/15, port-D --> pin 35/36
 */
static int alc_subsystem_id(struct hda_codec *codec,
			    hda_nid_t porta, hda_nid_t porte,
			    hda_nid_t portd)
{
	unsigned int ass, tmp, i;
	unsigned nid;
	struct alc_spec *spec = codec->spec;

	ass = codec->subsystem_id & 0xffff;
	if ((ass != codec->bus->pci->subsystem_device) && (ass & 1))
		goto do_sku;

	/* invalid SSID, check the special NID pin defcfg instead */
	/*
	 * 31~30	: port connectivity
	 * 29~21	: reserve
	 * 20		: PCBEEP input
	 * 19~16	: Check sum (15:1)
	 * 15~1		: Custom
	 * 0		: override
	*/
	nid = 0x1d;
	if (codec->vendor_id == 0x10ec0260)
		nid = 0x17;
	ass = snd_hda_codec_get_pincfg(codec, nid);
	snd_printd("realtek: No valid SSID, "
		   "checking pincfg 0x%08x for NID 0x%x\n",
		   ass, nid);
	if (!(ass & 1) && !(ass & 0x100000))
		return 0;
	if ((ass >> 30) != 1)	/* no physical connection */
		return 0;

	/* check sum */
	tmp = 0;
	for (i = 1; i < 16; i++) {
		if ((ass >> i) & 1)
			tmp++;
	}
	if (((ass >> 16) & 0xf) != tmp)
		return 0;
do_sku:
	snd_printd("realtek: Enabling init ASM_ID=0x%04x CODEC_ID=%08x\n",
		   ass & 0xffff, codec->vendor_id);
	/*
	 * 0 : override
	 * 1 :	Swap Jack
	 * 2 : 0 --> Desktop, 1 --> Laptop
	 * 3~5 : External Amplifier control
	 * 7~6 : Reserved
	*/
	tmp = (ass & 0x38) >> 3;	/* external Amp control */
	switch (tmp) {
	case 1:
		spec->init_amp = ALC_INIT_GPIO1;
		break;
	case 3:
		spec->init_amp = ALC_INIT_GPIO2;
		break;
	case 7:
		spec->init_amp = ALC_INIT_GPIO3;
		break;
	case 5:
		spec->init_amp = ALC_INIT_DEFAULT;
		break;
	}

	/* is laptop or Desktop and enable the function "Mute internal speaker
	 * when the external headphone out jack is plugged"
	 */
	if (!(ass & 0x8000))
		return 1;
	/*
	 * 10~8 : Jack location
	 * 12~11: Headphone out -> 00: PortA, 01: PortE, 02: PortD, 03: Resvered
	 * 14~13: Resvered
	 * 15   : 1 --> enable the function "Mute internal speaker
	 *	        when the external headphone out jack is plugged"
	 */
	if (!spec->autocfg.hp_pins[0]) {
		hda_nid_t nid;
		tmp = (ass >> 11) & 0x3;	/* HP to chassis */
		if (tmp == 0)
			nid = porta;
		else if (tmp == 1)
			nid = porte;
		else if (tmp == 2)
			nid = portd;
		else
			return 1;
		for (i = 0; i < spec->autocfg.line_outs; i++)
			if (spec->autocfg.line_out_pins[i] == nid)
				return 1;
		spec->autocfg.hp_pins[0] = nid;
	}

	alc_init_auto_hp(codec);
	alc_init_auto_mic(codec);
	return 1;
}

static void alc_ssid_check(struct hda_codec *codec,
			   hda_nid_t porta, hda_nid_t porte, hda_nid_t portd)
{
	if (!alc_subsystem_id(codec, porta, porte, portd)) {
		struct alc_spec *spec = codec->spec;
		snd_printd("realtek: "
			   "Enable default setup for auto mode as fallback\n");
		spec->init_amp = ALC_INIT_DEFAULT;
		alc_init_auto_hp(codec);
		alc_init_auto_mic(codec);
	}
}

/*
 * Fix-up pin default configurations and add default verbs
 */

struct alc_pincfg {
	hda_nid_t nid;
	u32 val;
};

struct alc_fixup {
	const struct alc_pincfg *pins;
	const struct hda_verb *verbs;
};

static void alc_pick_fixup(struct hda_codec *codec,
			   const struct snd_pci_quirk *quirk,
			   const struct alc_fixup *fix)
{
	const struct alc_pincfg *cfg;

	quirk = snd_pci_quirk_lookup(codec->bus->pci, quirk);
	if (!quirk)
		return;

	fix += quirk->value;
	cfg = fix->pins;
	if (cfg) {
		for (; cfg->nid; cfg++)
			snd_hda_codec_set_pincfg(codec, cfg->nid, cfg->val);
	}
	if (fix->verbs)
		add_verb(codec->spec, fix->verbs);
}

/*
 * ALC888
 */

/*
 * 2ch mode
 */
static struct hda_verb alc888_4ST_ch2_intel_init[] = {
/* Mic-in jack as mic in */
	{ 0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80 },
	{ 0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE },
/* Line-in jack as Line in */
	{ 0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN },
	{ 0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE },
/* Line-Out as Front */
	{ 0x17, AC_VERB_SET_CONNECT_SEL, 0x00},
	{ } /* end */
};

/*
 * 4ch mode
 */
static struct hda_verb alc888_4ST_ch4_intel_init[] = {
/* Mic-in jack as mic in */
	{ 0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80 },
	{ 0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE },
/* Line-in jack as Surround */
	{ 0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE },
/* Line-Out as Front */
	{ 0x17, AC_VERB_SET_CONNECT_SEL, 0x00},
	{ } /* end */
};

/*
 * 6ch mode
 */
static struct hda_verb alc888_4ST_ch6_intel_init[] = {
/* Mic-in jack as CLFE */
	{ 0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE },
/* Line-in jack as Surround */
	{ 0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE },
/* Line-Out as CLFE (workaround because Mic-in is not loud enough) */
	{ 0x17, AC_VERB_SET_CONNECT_SEL, 0x03},
	{ } /* end */
};

/*
 * 8ch mode
 */
static struct hda_verb alc888_4ST_ch8_intel_init[] = {
/* Mic-in jack as CLFE */
	{ 0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE },
/* Line-in jack as Surround */
	{ 0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE },
/* Line-Out as Side */
	{ 0x17, AC_VERB_SET_CONNECT_SEL, 0x03},
	{ } /* end */
};

static struct hda_channel_mode alc888_4ST_8ch_intel_modes[4] = {
	{ 2, alc888_4ST_ch2_intel_init },
	{ 4, alc888_4ST_ch4_intel_init },
	{ 6, alc888_4ST_ch6_intel_init },
	{ 8, alc888_4ST_ch8_intel_init },
};

/*
 * ALC888 Fujitsu Siemens Amillo xa3530
 */

static struct hda_verb alc888_fujitsu_xa3530_verbs[] = {
/* Front Mic: set to PIN_IN (empty by default) */
	{0x12, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},
/* Connect Internal HP to Front */
	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x14, AC_VERB_SET_CONNECT_SEL, 0x00},
/* Connect Bass HP to Front */
	{0x15, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x15, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x15, AC_VERB_SET_CONNECT_SEL, 0x00},
/* Connect Line-Out side jack (SPDIF) to Side */
	{0x17, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x17, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x17, AC_VERB_SET_CONNECT_SEL, 0x03},
/* Connect Mic jack to CLFE */
	{0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x18, AC_VERB_SET_CONNECT_SEL, 0x02},
/* Connect Line-in jack to Surround */
	{0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x1a, AC_VERB_SET_CONNECT_SEL, 0x01},
/* Connect HP out jack to Front */
	{0x1b, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x1b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x1b, AC_VERB_SET_CONNECT_SEL, 0x00},
/* Enable unsolicited event for HP jack and Line-out jack */
	{0x1b, AC_VERB_SET_UNSOLICITED_ENABLE, ALC880_HP_EVENT | AC_USRSP_EN},
	{0x17, AC_VERB_SET_UNSOLICITED_ENABLE, ALC880_HP_EVENT | AC_USRSP_EN},
	{}
};

static void alc_automute_amp(struct hda_codec *codec)
{
	struct alc_spec *spec = codec->spec;
	unsigned int val, mute, pincap;
	hda_nid_t nid;
	int i;

	spec->jack_present = 0;
	for (i = 0; i < ARRAY_SIZE(spec->autocfg.hp_pins); i++) {
		nid = spec->autocfg.hp_pins[i];
		if (!nid)
			break;
		pincap = snd_hda_query_pin_caps(codec, nid);
		if (pincap & AC_PINCAP_TRIG_REQ) /* need trigger? */
			snd_hda_codec_read(codec, nid, 0,
					   AC_VERB_SET_PIN_SENSE, 0);
		val = snd_hda_codec_read(codec, nid, 0,
					 AC_VERB_GET_PIN_SENSE, 0);
		if (val & AC_PINSENSE_PRESENCE) {
			spec->jack_present = 1;
			break;
		}
	}

	mute = spec->jack_present ? HDA_AMP_MUTE : 0;
	/* Toggle internal speakers muting */
	for (i = 0; i < ARRAY_SIZE(spec->autocfg.speaker_pins); i++) {
		nid = spec->autocfg.speaker_pins[i];
		if (!nid)
			break;
		snd_hda_codec_amp_stereo(codec, nid, HDA_OUTPUT, 0,
					 HDA_AMP_MUTE, mute);
	}
}

static void alc_automute_amp_unsol_event(struct hda_codec *codec,
					 unsigned int res)
{
	if (codec->vendor_id == 0x10ec0880)
		res >>= 28;
	else
		res >>= 26;
	if (res == ALC880_HP_EVENT)
		alc_automute_amp(codec);
}

static void alc889_automute_setup(struct hda_codec *codec)
{
	struct alc_spec *spec = codec->spec;

	spec->autocfg.hp_pins[0] = 0x15;
	spec->autocfg.speaker_pins[0] = 0x14;
	spec->autocfg.speaker_pins[1] = 0x16;
	spec->autocfg.speaker_pins[2] = 0x17;
	spec->autocfg.speaker_pins[3] = 0x19;
	spec->autocfg.speaker_pins[4] = 0x1a;
}

static void alc889_intel_init_hook(struct hda_codec *codec)
{
	alc889_coef_init(codec);
	alc_automute_amp(codec);
}

static void alc888_fujitsu_xa3530_setup(struct hda_codec *codec)
{
	struct alc_spec *spec = codec->spec;

	spec->autocfg.hp_pins[0] = 0x17; /* line-out */
	spec->autocfg.hp_pins[1] = 0x1b; /* hp */
	spec->autocfg.speaker_pins[0] = 0x14; /* speaker */
	spec->autocfg.speaker_pins[1] = 0x15; /* bass */
}

/*
 * ALC888 Acer Aspire 4930G model
 */

static struct hda_verb alc888_acer_aspire_4930g_verbs[] = {
/* Front Mic: set to PIN_IN (empty by default) */
	{0x12, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},
/* Unselect Front Mic by default in input mixer 3 */
	{0x22, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(0xb)},
/* Enable unsolicited event for HP jack */
	{0x15, AC_VERB_SET_UNSOLICITED_ENABLE, ALC880_HP_EVENT | AC_USRSP_EN},
/* Connect Internal HP to front */
	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x14, AC_VERB_SET_CONNECT_SEL, 0x00},
/* Connect HP out to front */
	{0x15, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x15, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x15, AC_VERB_SET_CONNECT_SEL, 0x00},
	{ }
};

/*
 * ALC888 Acer Aspire 6530G model
 */

static struct hda_verb alc888_acer_aspire_6530g_verbs[] = {
/* Bias voltage on for external mic port */
	{0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN | PIN_VREF80},
/* Front Mic: set to PIN_IN (empty by default) */
	{0x12, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},
/* Unselect Front Mic by default in input mixer 3 */
	{0x22, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(0xb)},
/* Enable unsolicited event for HP jack */
	{0x15, AC_VERB_SET_UNSOLICITED_ENABLE, ALC880_HP_EVENT | AC_USRSP_EN},
/* Enable speaker output */
	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
/* Enable headphone output */
	{0x15, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT | PIN_HP},
	{0x15, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x15, AC_VERB_SET_CONNECT_SEL, 0x00},
	{ }
};

/*
 * ALC889 Acer Aspire 8930G model
 */

static struct hda_verb alc889_acer_aspire_8930g_verbs[] = {
/* Front Mic: set to PIN_IN (empty by default) */
	{0x12, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},
/* Unselect Front Mic by default in input mixer 3 */
	{0x22, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(0xb)},
/* Enable unsolicited event for HP jack */
	{0x15, AC_VERB_SET_UNSOLICITED_ENABLE, ALC880_HP_EVENT | AC_USRSP_EN},
/* Connect Internal Front to Front */
	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x14, AC_VERB_SET_CONNECT_SEL, 0x00},
/* Connect Internal Rear to Rear */
	{0x1b, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x1b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x1b, AC_VERB_SET_CONNECT_SEL, 0x01},
/* Connect Internal CLFE to CLFE */
	{0x16, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x16, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x16, AC_VERB_SET_CONNECT_SEL, 0x02},
/* Connect HP out to Front */
	{0x15, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT | PIN_HP},
	{0x15, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x15, AC_VERB_SET_CONNECT_SEL, 0x00},
/* Enable all DACs */
/*  DAC DISABLE/MUTE 1? */
/*  setting bits 1-5 disables DAC nids 0x02-0x06 apparently. Init=0x38 */
	{0x20, AC_VERB_SET_COEF_INDEX, 0x03},
	{0x20, AC_VERB_SET_PROC_COEF, 0x0000},
/*  DAC DISABLE/MUTE 2? */
/*  some bit here disables the other DACs. Init=0x4900 */
	{0x20, AC_VERB_SET_COEF_INDEX, 0x08},
	{0x20, AC_VERB_SET_PROC_COEF, 0x0000},
/* Enable amplifiers */
	{0x14, AC_VERB_SET_EAPD_BTLENABLE, 0x02},
	{0x15, AC_VERB_SET_EAPD_BTLENABLE, 0x02},
/* DMIC fix
 * This laptop has a stereo digital microphone. The mics are only 1cm apart
 * which makes the stereo useless. However, either the mic or the ALC889
 * makes the signal become a difference/sum signal instead of standard
 * stereo, which is annoying. So instead we flip this bit which makes the
 * codec replicate the sum signal to both channels, turning it into a
 * normal mono mic.
 */
/*  DMIC_CONTROL? Init value = 0x0001 */
	{0x20, AC_VERB_SET_COEF_INDEX, 0x0b},
	{0x20, AC_VERB_SET_PROC_COEF, 0x0003},
	{ }
};

static struct hda_input_mux alc888_2_capture_sources[2] = {
	/* Front mic only available on one ADC */
	{
		.num_items = 4,
		.items = {
			{ "Mic", 0x0 },
			{ "Line", 0x2 },
			{ "CD", 0x4 },
			{ "Front Mic", 0xb },
		},
	},
	{
		.num_items = 3,
		.items = {
			{ "Mic", 0x0 },
			{ "Line", 0x2 },
			{ "CD", 0x4 },
		},
	}
};

static struct hda_input_mux alc888_acer_aspire_6530_sources[2] = {
	/* Interal mic only available on one ADC */
	{
		.num_items = 5,
		.items = {
			{ "Ext Mic", 0x0 },
			{ "Line In", 0x2 },
			{ "CD", 0x4 },
			{ "Input Mix", 0xa },
			{ "Int Mic", 0xb },
		},
	},
	{
		.num_items = 4,
		.items = {
			{ "Ext Mic", 0x0 },
			{ "Line In", 0x2 },
			{ "CD", 0x4 },
			{ "Input Mix", 0xa },
		},
	}
};

static struct hda_input_mux alc889_capture_sources[3] = {
	/* Digital mic only available on first "ADC" */
	{
		.num_items = 5,
		.items = {
			{ "Mic", 0x0 },
			{ "Line", 0x2 },
			{ "CD", 0x4 },
			{ "Front Mic", 0xb },
			{ "Input Mix", 0xa },
		},
	},
	{
		.num_items = 4,
		.items = {
			{ "Mic", 0x0 },
			{ "Line", 0x2 },
			{ "CD", 0x4 },
			{ "Input Mix", 0xa },
		},
	},
	{
		.num_items = 4,
		.items = {
			{ "Mic", 0x0 },
			{ "Line", 0x2 },
			{ "CD", 0x4 },
			{ "Input Mix", 0xa },
		},
	}
};

static struct snd_kcontrol_new alc888_base_mixer[] = {
	HDA_CODEC_VOLUME("Front Playback Volume", 0x0c, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Front Playback Switch", 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Surround Playback Volume", 0x0d, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Surround Playback Switch", 0x0d, 2, HDA_INPUT),
	HDA_CODEC_VOLUME_MONO("Center Playback Volume", 0x0e, 1, 0x0,
		HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("LFE Playback Volume", 0x0e, 2, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE_MONO("Center Playback Switch", 0x0e, 1, 2, HDA_INPUT),
	HDA_BIND_MUTE_MONO("LFE Playback Switch", 0x0e, 2, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Side Playback Volume", 0x0f, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Side Playback Switch", 0x0f, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("CD Playback Volume", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_MUTE("CD Playback Switch", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_VOLUME("Line Playback Volume", 0x0b, 0x02, HDA_INPUT),
	HDA_CODEC_MUTE("Line Playback Switch", 0x0b, 0x02, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x0b, 0x0, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Boost", 0x18, 0, HDA_INPUT),
	HDA_CODEC_MUTE("Mic Playback Switch", 0x0b, 0x0, HDA_INPUT),
	{ } /* end */
};

static void alc888_acer_aspire_4930g_setup(struct hda_codec *codec)
{
	struct alc_spec *spec = codec->spec;

	spec->autocfg.hp_pins[0] = 0x15;
	spec->autocfg.speaker_pins[0] = 0x14;
}

static void alc888_acer_aspire_6530g_setup(struct hda_codec *codec)
{
	struct alc_spec *spec = codec->spec;

	spec->autocfg.hp_pins[0] = 0x15;
	spec->autocfg.speaker_pins[0] = 0x14;
	spec->autocfg.speaker_pins[1] = 0x16;
	spec->autocfg.speaker_pins[2] = 0x17;
}

static void alc889_acer_aspire_8930g_setup(struct hda_codec *codec)
{
	struct alc_spec *spec = codec->spec;

	spec->autocfg.hp_pins[0] = 0x15;
	spec->autocfg.speaker_pins[0] = 0x14;
	spec->autocfg.speaker_pins[1] = 0x16;
	spec->autocfg.speaker_pins[2] = 0x1b;
}

/*
 * ALC880 3-stack model
 *
 * DAC: Front = 0x02 (0x0c), Surr = 0x05 (0x0f), CLFE = 0x04 (0x0e)
 * Pin assignment: Front = 0x14, Line-In/Surr = 0x1a, Mic/CLFE = 0x18,
 *                 F-Mic = 0x1b, HP = 0x19
 */

static hda_nid_t alc880_dac_nids[4] = {
	/* front, rear, clfe, rear_surr */
	0x02, 0x05, 0x04, 0x03
};

static hda_nid_t alc880_adc_nids[3] = {
	/* ADC0-2 */
	0x07, 0x08, 0x09,
};

/* The datasheet says the node 0x07 is connected from inputs,
 * but it shows zero connection in the real implementation on some devices.
 * Note: this is a 915GAV bug, fixed on 915GLV
 */
static hda_nid_t alc880_adc_nids_alt[2] = {
	/* ADC1-2 */
	0x08, 0x09,
};

#define ALC880_DIGOUT_NID	0x06
#define ALC880_DIGIN_NID	0x0a

static struct hda_input_mux alc880_capture_source = {
	.num_items = 4,
	.items = {
		{ "Mic", 0x0 },
		{ "Front Mic", 0x3 },
		{ "Line", 0x2 },
		{ "CD", 0x4 },
	},
};

/* channel source setting (2/6 channel selection for 3-stack) */
/* 2ch mode */
static struct hda_verb alc880_threestack_ch2_init[] = {
	/* set line-in to input, mute it */
	{ 0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN },
	{ 0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE },
	/* set mic-in to input vref 80%, mute it */
	{ 0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80 },
	{ 0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE },
	{ } /* end */
};

/* 6ch mode */
static struct hda_verb alc880_threestack_ch6_init[] = {
	/* set line-in to output, unmute it */
	{ 0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE },
	/* set mic-in to output, unmute it */
	{ 0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE },
	{ } /* end */
};

static struct hda_channel_mode alc880_threestack_modes[2] = {
	{ 2, alc880_threestack_ch2_init },
	{ 6, alc880_threestack_ch6_init },
};

static struct snd_kcontrol_new alc880_three_stack_mixer[] = {
	HDA_CODEC_VOLUME("Front Playback Volume", 0x0c, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Front Playback Switch", 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Surround Playback Volume", 0x0f, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Surround Playback Switch", 0x0f, 2, HDA_INPUT),
	HDA_CODEC_VOLUME_MONO("Center Playback Volume", 0x0e, 1, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("LFE Playback Volume", 0x0e, 2, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE_MONO("Center Playback Switch", 0x0e, 1, 2, HDA_INPUT),
	HDA_BIND_MUTE_MONO("LFE Playback Switch", 0x0e, 2, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("CD Playback Volume", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_MUTE("CD Playback Switch", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_VOLUME("Line Playback Volume", 0x0b, 0x02, HDA_INPUT),
	HDA_CODEC_MUTE("Line Playback Switch", 0x0b, 0x02, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x0b, 0x0, HDA_INPUT),
	HDA_CODEC_MUTE("Mic Playback Switch", 0x0b, 0x0, HDA_INPUT),
	HDA_CODEC_VOLUME("Front Mic Playback Volume", 0x0b, 0x3, HDA_INPUT),
	HDA_CODEC_MUTE("Front Mic Playback Switch", 0x0b, 0x3, HDA_INPUT),
	HDA_CODEC_MUTE("Headphone Playback Switch", 0x19, 0x0, HDA_OUTPUT),
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Channel Mode",
		.info = alc_ch_mode_info,
		.get = alc_ch_mode_get,
		.put = alc_ch_mode_put,
	},
	{ } /* end */
};

/* capture mixer elements */
static int alc_cap_vol_info(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_info *uinfo)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct alc_spec *spec = codec->spec;
	int err;

	mutex_lock(&codec->control_mutex);
	kcontrol->private_value = HDA_COMPOSE_AMP_VAL(spec->adc_nids[0], 3, 0,
						      HDA_INPUT);
	err = snd_hda_mixer_amp_volume_info(kcontrol, uinfo);
	mutex_unlock(&codec->control_mutex);
	return err;
}

static int alc_cap_vol_tlv(struct snd_kcontrol *kcontrol, int op_flag,
			   unsigned int size, unsigned int __user *tlv)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct alc_spec *spec = codec->spec;
	int err;

	mutex_lock(&codec->control_mutex);
	kcontrol->private_value = HDA_COMPOSE_AMP_VAL(spec->adc_nids[0], 3, 0,
						      HDA_INPUT);
	err = snd_hda_mixer_amp_tlv(kcontrol, op_flag, size, tlv);
	mutex_unlock(&codec->control_mutex);
	return err;
}

typedef int (*getput_call_t)(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol);

static int alc_cap_getput_caller(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol,
				 getput_call_t func)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct alc_spec *spec = codec->spec;
	unsigned int adc_idx = snd_ctl_get_ioffidx(kcontrol, &ucontrol->id);
	int err;

	mutex_lock(&codec->control_mutex);
	kcontrol->private_value = HDA_COMPOSE_AMP_VAL(spec->adc_nids[adc_idx],
						      3, 0, HDA_INPUT);
	err = func(kcontrol, ucontrol);
	mutex_unlock(&codec->control_mutex);
	return err;
}

static int alc_cap_vol_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	return alc_cap_getput_caller(kcontrol, ucontrol,
				     snd_hda_mixer_amp_volume_get);
}

static int alc_cap_vol_put(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	return alc_cap_getput_caller(kcontrol, ucontrol,
				     snd_hda_mixer_amp_volume_put);
}

/* capture mixer elements */
#define alc_cap_sw_info		snd_ctl_boolean_stereo_info

static int alc_cap_sw_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	return alc_cap_getput_caller(kcontrol, ucontrol,
				     snd_hda_mixer_amp_switch_get);
}

static int alc_cap_sw_put(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	return alc_cap_getput_caller(kcontrol, ucontrol,
				     snd_hda_mixer_amp_switch_put);
}

#define _DEFINE_CAPMIX(num) \
	{ \
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
		.name = "Capture Switch", \
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE, \
		.count = num, \
		.info = alc_cap_sw_info, \
		.get = alc_cap_sw_get, \
		.put = alc_cap_sw_put, \
	}, \
	{ \
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
		.name = "Capture Volume", \
		.access = (SNDRV_CTL_ELEM_ACCESS_READWRITE | \
			   SNDRV_CTL_ELEM_ACCESS_TLV_READ | \
			   SNDRV_CTL_ELEM_ACCESS_TLV_CALLBACK), \
		.count = num, \
		.info = alc_cap_vol_info, \
		.get = alc_cap_vol_get, \
		.put = alc_cap_vol_put, \
		.tlv = { .c = alc_cap_vol_tlv }, \
	}

#define _DEFINE_CAPSRC(num) \
	{ \
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
		/* .name = "Capture Source", */ \
		.name = "Input Source", \
		.count = num, \
		.info = alc_mux_enum_info, \
		.get = alc_mux_enum_get, \
		.put = alc_mux_enum_put, \
	}

#define DEFINE_CAPMIX(num) \
static struct snd_kcontrol_new alc_capture_mixer ## num[] = { \
	_DEFINE_CAPMIX(num),				      \
	_DEFINE_CAPSRC(num),				      \
	{ } /* end */					      \
}

#define DEFINE_CAPMIX_NOSRC(num) \
static struct snd_kcontrol_new alc_capture_mixer_nosrc ## num[] = { \
	_DEFINE_CAPMIX(num),					    \
	{ } /* end */						    \
}

/* up to three ADCs */
DEFINE_CAPMIX(1);
DEFINE_CAPMIX(2);
DEFINE_CAPMIX(3);
DEFINE_CAPMIX_NOSRC(1);
DEFINE_CAPMIX_NOSRC(2);
DEFINE_CAPMIX_NOSRC(3);

/*
 * ALC880 5-stack model
 *
 * DAC: Front = 0x02 (0x0c), Surr = 0x05 (0x0f), CLFE = 0x04 (0x0d),
 *      Side = 0x02 (0xd)
 * Pin assignment: Front = 0x14, Surr = 0x17, CLFE = 0x16
 *                 Line-In/Side = 0x1a, Mic = 0x18, F-Mic = 0x1b, HP = 0x19
 */

/* additional mixers to alc880_three_stack_mixer */
static struct snd_kcontrol_new alc880_five_stack_mixer[] = {
	HDA_CODEC_VOLUME("Side Playback Volume", 0x0d, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Side Playback Switch", 0x0d, 2, HDA_INPUT),
	{ } /* end */
};

/* channel source setting (6/8 channel selection for 5-stack) */
/* 6ch mode */
static struct hda_verb alc880_fivestack_ch6_init[] = {
	/* set line-in to input, mute it */
	{ 0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN },
	{ 0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE },
	{ } /* end */
};

/* 8ch mode */
static struct hda_verb alc880_fivestack_ch8_init[] = {
	/* set line-in to output, unmute it */
	{ 0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE },
	{ } /* end */
};

static struct hda_channel_mode alc880_fivestack_modes[2] = {
	{ 6, alc880_fivestack_ch6_init },
	{ 8, alc880_fivestack_ch8_init },
};


/*
 * ALC880 6-stack model
 *
 * DAC: Front = 0x02 (0x0c), Surr = 0x03 (0x0d), CLFE = 0x04 (0x0e),
 *      Side = 0x05 (0x0f)
 * Pin assignment: Front = 0x14, Surr = 0x15, CLFE = 0x16, Side = 0x17,
 *   Mic = 0x18, F-Mic = 0x19, Line = 0x1a, HP = 0x1b
 */

static hda_nid_t alc880_6st_dac_nids[4] = {
	/* front, rear, clfe, rear_surr */
	0x02, 0x03, 0x04, 0x05
};

static struct hda_input_mux alc880_6stack_capture_source = {
	.num_items = 4,
	.items = {
		{ "Mic", 0x0 },
		{ "Front Mic", 0x1 },
		{ "Line", 0x2 },
		{ "CD", 0x4 },
	},
};

/* fixed 8-channels */
static struct hda_channel_mode alc880_sixstack_modes[1] = {
	{ 8, NULL },
};

static struct snd_kcontrol_new alc880_six_stack_mixer[] = {
	HDA_CODEC_VOLUME("Front Playback Volume", 0x0c, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Front Playback Switch", 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Surround Playback Volume", 0x0d, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Surround Playback Switch", 0x0d, 2, HDA_INPUT),
	HDA_CODEC_VOLUME_MONO("Center Playback Volume", 0x0e, 1, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("LFE Playback Volume", 0x0e, 2, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE_MONO("Center Playback Switch", 0x0e, 1, 2, HDA_INPUT),
	HDA_BIND_MUTE_MONO("LFE Playback Switch", 0x0e, 2, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Side Playback Volume", 0x0f, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Side Playback Switch", 0x0f, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("CD Playback Volume", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_MUTE("CD Playback Switch", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_VOLUME("Line Playback Volume", 0x0b, 0x02, HDA_INPUT),
	HDA_CODEC_MUTE("Line Playback Switch", 0x0b, 0x02, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x0b, 0x0, HDA_INPUT),
	HDA_CODEC_MUTE("Mic Playback Switch", 0x0b, 0x0, HDA_INPUT),
	HDA_CODEC_VOLUME("Front Mic Playback Volume", 0x0b, 0x1, HDA_INPUT),
	HDA_CODEC_MUTE("Front Mic Playback Switch", 0x0b, 0x1, HDA_INPUT),
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Channel Mode",
		.info = alc_ch_mode_info,
		.get = alc_ch_mode_get,
		.put = alc_ch_mode_put,
	},
	{ } /* end */
};


/*
 * ALC880 W810 model
 *
 * W810 has rear IO for:
 * Front (DAC 02)
 * Surround (DAC 03)
 * Center/LFE (DAC 04)
 * Digital out (06)
 *
 * The system also has a pair of internal speakers, and a headphone jack.
 * These are both connected to Line2 on the codec, hence to DAC 02.
 *
 * There is a variable resistor to control the speaker or headphone
 * volume. This is a hardware-only device without a software API.
 *
 * Plugging headphones in will disable the internal speakers. This is
 * implemented in hardware, not via the driver using jack sense. In
 * a similar fashion, plugging into the rear socket marked "front" will
 * disable both the speakers and headphones.
 *
 * For input, there's a microphone jack, and an "audio in" jack.
 * These may not do anything useful with this driver yet, because I
 * haven't setup any initialization verbs for these yet...
 */

static hda_nid_t alc880_w810_dac_nids[3] = {
	/* front, rear/surround, clfe */
	0x02, 0x03, 0x04
};

/* fixed 6 channels */
static struct hda_channel_mode alc880_w810_modes[1] = {
	{ 6, NULL }
};

/* Pin assignment: Front = 0x14, Surr = 0x15, CLFE = 0x16, HP = 0x1b */
static struct snd_kcontrol_new alc880_w810_base_mixer[] = {
	HDA_CODEC_VOLUME("Front Playback Volume", 0x0c, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Front Playback Switch", 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Surround Playback Volume", 0x0d, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Surround Playback Switch", 0x0d, 2, HDA_INPUT),
	HDA_CODEC_VOLUME_MONO("Center Playback Volume", 0x0e, 1, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("LFE Playback Volume", 0x0e, 2, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE_MONO("Center Playback Switch", 0x0e, 1, 2, HDA_INPUT),
	HDA_BIND_MUTE_MONO("LFE Playback Switch", 0x0e, 2, 2, HDA_INPUT),
	HDA_CODEC_MUTE("Headphone Playback Switch", 0x1b, 0x0, HDA_OUTPUT),
	{ } /* end */
};


/*
 * Z710V model
 *
 * DAC: Front = 0x02 (0x0c), HP = 0x03 (0x0d)
 * Pin assignment: Front = 0x14, HP = 0x15, Mic = 0x18, Mic2 = 0x19(?),
 *                 Line = 0x1a
 */

static hda_nid_t alc880_z71v_dac_nids[1] = {
	0x02
};
#define ALC880_Z71V_HP_DAC	0x03

/* fixed 2 channels */
static struct hda_channel_mode alc880_2_jack_modes[1] = {
	{ 2, NULL }
};

static struct snd_kcontrol_new alc880_z71v_mixer[] = {
	HDA_CODEC_VOLUME("Front Playback Volume", 0x0c, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Front Playback Switch", 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Headphone Playback Volume", 0x0d, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Headphone Playback Switch", 0x0d, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("CD Playback Volume", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_MUTE("CD Playback Switch", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x0b, 0x0, HDA_INPUT),
	HDA_CODEC_MUTE("Mic Playback Switch", 0x0b, 0x0, HDA_INPUT),
	{ } /* end */
};


/*
 * ALC880 F1734 model
 *
 * DAC: HP = 0x02 (0x0c), Front = 0x03 (0x0d)
 * Pin assignment: HP = 0x14, Front = 0x15, Mic = 0x18
 */

static hda_nid_t alc880_f1734_dac_nids[1] = {
	0x03
};
#define ALC880_F1734_HP_DAC	0x02

static struct snd_kcontrol_new alc880_f1734_mixer[] = {
	HDA_CODEC_VOLUME("Headphone Playback Volume", 0x0c, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Headphone Playback Switch", 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Speaker Playback Volume", 0x0d, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Speaker Playback Switch", 0x0d, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("CD Playback Volume", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_MUTE("CD Playback Switch", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x0b, 0x1, HDA_INPUT),
	HDA_CODEC_MUTE("Mic Playback Switch", 0x0b, 0x1, HDA_INPUT),
	{ } /* end */
};

static struct hda_input_mux alc880_f1734_capture_source = {
	.num_items = 2,
	.items = {
		{ "Mic", 0x1 },
		{ "CD", 0x4 },
	},
};


/*
 * ALC880 ASUS model
 *
 * DAC: HP/Front = 0x02 (0x0c), Surr = 0x03 (0x0d), CLFE = 0x04 (0x0e)
 * Pin assignment: HP/Front = 0x14, Surr = 0x15, CLFE = 0x16,
 *  Mic = 0x18, Line = 0x1a
 */

#define alc880_asus_dac_nids	alc880_w810_dac_nids	/* identical with w810 */
#define alc880_asus_modes	alc880_threestack_modes	/* 2/6 channel mode */

static struct snd_kcontrol_new alc880_asus_mixer[] = {
	HDA_CODEC_VOLUME("Front Playback Volume", 0x0c, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Front Playback Switch", 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Surround Playback Volume", 0x0d, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Surround Playback Switch", 0x0d, 2, HDA_INPUT),
	HDA_CODEC_VOLUME_MONO("Center Playback Volume", 0x0e, 1, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("LFE Playback Volume", 0x0e, 2, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE_MONO("Center Playback Switch", 0x0e, 1, 2, HDA_INPUT),
	HDA_BIND_MUTE_MONO("LFE Playback Switch", 0x0e, 2, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("CD Playback Volume", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_MUTE("CD Playback Switch", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_VOLUME("Line Playback Volume", 0x0b, 0x02, HDA_INPUT),
	HDA_CODEC_MUTE("Line Playback Switch", 0x0b, 0x02, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x0b, 0x0, HDA_INPUT),
	HDA_CODEC_MUTE("Mic Playback Switch", 0x0b, 0x0, HDA_INPUT),
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Channel Mode",
		.info = alc_ch_mode_info,
		.get = alc_ch_mode_get,
		.put = alc_ch_mode_put,
	},
	{ } /* end */
};

/*
 * ALC880 ASUS W1V model
 *
 * DAC: HP/Front = 0x02 (0x0c), Surr = 0x03 (0x0d), CLFE = 0x04 (0x0e)
 * Pin assignment: HP/Front = 0x14, Surr = 0x15, CLFE = 0x16,
 *  Mic = 0x18, Line = 0x1a, Line2 = 0x1b
 */

/* additional mixers to alc880_asus_mixer */
static struct snd_kcontrol_new alc880_asus_w1v_mixer[] = {
	HDA_CODEC_VOLUME("Line2 Playback Volume", 0x0b, 0x03, HDA_INPUT),
	HDA_CODEC_MUTE("Line2 Playback Switch", 0x0b, 0x03, HDA_INPUT),
	{ } /* end */
};

/* TCL S700 */
static struct snd_kcontrol_new alc880_tcl_s700_mixer[] = {
	HDA_CODEC_VOLUME("Front Playback Volume", 0x0c, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Front Playback Switch", 0x1b, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Headphone Playback Switch", 0x14, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("CD Playback Volume", 0x0B, 0x04, HDA_INPUT),
	HDA_CODEC_MUTE("CD Playback Switch", 0x0B, 0x04, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x0B, 0x0, HDA_INPUT),
	HDA_CODEC_MUTE("Mic Playback Switch", 0x0B, 0x0, HDA_INPUT),
	HDA_CODEC_VOLUME("Capture Volume", 0x08, 0x0, HDA_INPUT),
	HDA_CODEC_MUTE("Capture Switch", 0x08, 0x0, HDA_INPUT),
	{ } /* end */
};

/* Uniwill */
static struct snd_kcontrol_new alc880_uniwill_mixer[] = {
	HDA_CODEC_VOLUME("Headphone Playback Volume", 0x0c, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Headphone Playback Switch", 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Speaker Playback Volume", 0x0d, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Speaker Playback Switch", 0x0d, 2, HDA_INPUT),
	HDA_CODEC_VOLUME_MONO("Center Playback Volume", 0x0e, 1, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("LFE Playback Volume", 0x0e, 2, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE_MONO("Center Playback Switch", 0x0e, 1, 2, HDA_INPUT),
	HDA_BIND_MUTE_MONO("LFE Playback Switch", 0x0e, 2, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("CD Playback Volume", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_MUTE("CD Playback Switch", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_VOLUME("Line Playback Volume", 0x0b, 0x02, HDA_INPUT),
	HDA_CODEC_MUTE("Line Playback Switch", 0x0b, 0x02, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x0b, 0x0, HDA_INPUT),
	HDA_CODEC_MUTE("Mic Playback Switch", 0x0b, 0x0, HDA_INPUT),
	HDA_CODEC_VOLUME("Front Mic Playback Volume", 0x0b, 0x1, HDA_INPUT),
	HDA_CODEC_MUTE("Front Mic Playback Switch", 0x0b, 0x1, HDA_INPUT),
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Channel Mode",
		.info = alc_ch_mode_info,
		.get = alc_ch_mode_get,
		.put = alc_ch_mode_put,
	},
	{ } /* end */
};

static struct snd_kcontrol_new alc880_fujitsu_mixer[] = {
	HDA_CODEC_VOLUME("Headphone Playback Volume", 0x0c, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Headphone Playback Switch", 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Speaker Playback Volume", 0x0d, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Speaker Playback Switch", 0x0d, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("CD Playback Volume", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_MUTE("CD Playback Switch", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_VOLUME("Ext Mic Playback Volume", 0x0b, 0x0, HDA_INPUT),
	HDA_CODEC_MUTE("Ext Mic Playback Switch", 0x0b, 0x0, HDA_INPUT),
	HDA_CODEC_VOLUME("Int Mic Playback Volume", 0x0b, 0x1, HDA_INPUT),
	HDA_CODEC_MUTE("Int Mic Playback Switch", 0x0b, 0x1, HDA_INPUT),
	{ } /* end */
};

static struct snd_kcontrol_new alc880_uniwill_p53_mixer[] = {
	HDA_CODEC_VOLUME("Headphone Playback Volume", 0x0c, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Headphone Playback Switch", 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Speaker Playback Volume", 0x0d, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Speaker Playback Switch", 0x0d, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x0b, 0x0, HDA_INPUT),
	HDA_CODEC_MUTE("Mic Playback Switch", 0x0b, 0x0, HDA_INPUT),
	{ } /* end */
};

/*
 * virtual master controls
 */

/*
 * slave controls for virtual master
 */
static const char *alc_slave_vols[] = {
	"Front Playback Volume",
	"Surround Playback Volume",
	"Center Playback Volume",
	"LFE Playback Volume",
	"Side Playback Volume",
	"Headphone Playback Volume",
	"Speaker Playback Volume",
	"Mono Playback Volume",
	"Line-Out Playback Volume",
	"PCM Playback Volume",
	NULL,
};

static const char *alc_slave_sws[] = {
	"Front Playback Switch",
	"Surround Playback Switch",
	"Center Playback Switch",
	"LFE Playback Switch",
	"Side Playback Switch",
	"Headphone Playback Switch",
	"Speaker Playback Switch",
	"Mono Playback Switch",
	"IEC958 Playback Switch",
	NULL,
};

/*
 * build control elements
 */

static void alc_free_kctls(struct hda_codec *codec);

/* additional beep mixers; the actual parameters are overwritten at build */
static struct snd_kcontrol_new alc_beep_mixer[] = {
	HDA_CODEC_VOLUME("Beep Playback Volume", 0, 0, HDA_INPUT),
	HDA_CODEC_MUTE("Beep Playback Switch", 0, 0, HDA_INPUT),
	{ } /* end */
};

static int alc_build_controls(struct hda_codec *codec)
{
	struct alc_spec *spec = codec->spec;
	int err;
	int i;

	for (i = 0; i < spec->num_mixers; i++) {
		err = snd_hda_add_new_ctls(codec, spec->mixers[i]);
		if (err < 0)
			return err;
	}
	if (spec->cap_mixer) {
		err = snd_hda_add_new_ctls(codec, spec->cap_mixer);
		if (err < 0)
			return err;
	}
	if (spec->multiout.dig_out_nid) {
		err = snd_hda_create_spdif_out_ctls(codec,
						    spec->multiout.dig_out_nid);
		if (err < 0)
			return err;
		if (!spec->no_analog) {
			err = snd_hda_create_spdif_share_sw(codec,
							    &spec->multiout);
			if (err < 0)
				return err;
			spec->multiout.share_spdif = 1;
		}
	}
	if (spec->dig_in_nid) {
		err = snd_hda_create_spdif_in_ctls(codec, spec->dig_in_nid);
		if (err < 0)
			return err;
	}

	/* create beep controls if needed */
	if (spec->beep_amp) {
		struct snd_kcontrol_new *knew;
		for (knew = alc_beep_mixer; knew->name; knew++) {
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
	if (!spec->no_analog &&
	    !snd_hda_find_mixer_ctl(codec, "Master Playback Volume")) {
		unsigned int vmaster_tlv[4];
		snd_hda_set_vmaster_tlv(codec, spec->vmaster_nid,
					HDA_OUTPUT, vmaster_tlv);
		err = snd_hda_add_vmaster(codec, "Master Playback Volume",
					  vmaster_tlv, alc_slave_vols);
		if (err < 0)
			return err;
	}
	if (!spec->no_analog &&
	    !snd_hda_find_mixer_ctl(codec, "Master Playback Switch")) {
		err = snd_hda_add_vmaster(codec, "Master Playback Switch",
					  NULL, alc_slave_sws);
		if (err < 0)
			return err;
	}

	alc_free_kctls(codec); /* no longer needed */
	return 0;
}


/*
 * initialize the codec volumes, etc
 */

/*
 * generic initialization of ADC, input mixers and output mixers
 */
static struct hda_verb alc880_volume_init_verbs[] = {
	/*
	 * Unmute ADC0-2 and set the default input to mic-in
	 */
	{0x07, AC_VERB_SET_CONNECT_SEL, 0x00},
	{0x07, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_UNMUTE(0)},
	{0x08, AC_VERB_SET_CONNECT_SEL, 0x00},
	{0x08, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_UNMUTE(0)},
	{0x09, AC_VERB_SET_CONNECT_SEL, 0x00},
	{0x09, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_UNMUTE(0)},

	/* Unmute input amps (CD, Line In, Mic 1 & Mic 2) of the analog-loopback
	 * mixer widget
	 * Note: PASD motherboards uses the Line In 2 as the input for front
	 * panel mic (mic 2)
	 */
	/* Amp Indices: Mic1 = 0, Mic2 = 1, Line1 = 2, Line2 = 3, CD = 4 */
	{0x0b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(0)},
	{0x0b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(1)},
	{0x0b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(2)},
	{0x0b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(3)},
	{0x0b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(4)},
	{0x0b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(6)},
	{0x0b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(7)},

	/*
	 * Set up output mixers (0x0c - 0x0f)
	 */
	/* set vol=0 to output mixers */
	{0x0c, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_ZERO},
	{0x0d, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_ZERO},
	{0x0e, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_ZERO},
	{0x0f, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_ZERO},
	/* set up input amps for analog loopback */
	/* Amp Indices: DAC = 0, mixer = 1 */
	{0x0c, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(0)},
	{0x0c, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(1)},
	{0x0d, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(0)},
	{0x0d, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(1)},
	{0x0e, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(0)},
	{0x0e, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(1)},
	{0x0f, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(0)},
	{0x0f, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(1)},

	{ }
};

/*
 * 3-stack pin configuration:
 * front = 0x14, mic/clfe = 0x18, HP = 0x19, line/surr = 0x1a, f-mic = 0x1b
 */
static struct hda_verb alc880_pin_3stack_init_verbs[] = {
	/*
	 * preset connection lists of input pins
	 * 0 = front, 1 = rear_surr, 2 = CLFE, 3 = surround
	 */
	{0x10, AC_VERB_SET_CONNECT_SEL, 0x02}, /* mic/clfe */
	{0x11, AC_VERB_SET_CONNECT_SEL, 0x00}, /* HP */
	{0x12, AC_VERB_SET_CONNECT_SEL, 0x03}, /* line/surround */

	/*
	 * Set pin mode and muting
	 */
	/* set front pin widgets 0x14 for output */
	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	/* Mic1 (rear panel) pin widget for input and vref at 80% */
	{0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	/* Mic2 (as headphone out) for HP output */
	{0x19, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	{0x19, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	/* Line In pin widget for input */
	{0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},
	{0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	/* Line2 (as front mic) pin widget for input and vref at 80% */
	{0x1b, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x1b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	/* CD pin widget for input */
	{0x1c, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},

	{ }
};

/*
 * 5-stack pin configuration:
 * front = 0x14, surround = 0x17, clfe = 0x16, mic = 0x18, HP = 0x19,
 * line-in/side = 0x1a, f-mic = 0x1b
 */
static struct hda_verb alc880_pin_5stack_init_verbs[] = {
	/*
	 * preset connection lists of input pins
	 * 0 = front, 1 = rear_surr, 2 = CLFE, 3 = surround
	 */
	{0x11, AC_VERB_SET_CONNECT_SEL, 0x00}, /* HP */
	{0x12, AC_VERB_SET_CONNECT_SEL, 0x01}, /* line/side */

	/*
	 * Set pin mode and muting
	 */
	/* set pin widgets 0x14-0x17 for output */
	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x15, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x16, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x17, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	/* unmute pins for output (no gain on this amp) */
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x15, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x16, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x17, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},

	/* Mic1 (rear panel) pin widget for input and vref at 80% */
	{0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	/* Mic2 (as headphone out) for HP output */
	{0x19, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	{0x19, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	/* Line In pin widget for input */
	{0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},
	{0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	/* Line2 (as front mic) pin widget for input and vref at 80% */
	{0x1b, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x1b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	/* CD pin widget for input */
	{0x1c, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},

	{ }
};

/*
 * W810 pin configuration:
 * front = 0x14, surround = 0x15, clfe = 0x16, HP = 0x1b
 */
static struct hda_verb alc880_pin_w810_init_verbs[] = {
	/* hphone/speaker input selector: front DAC */
	{0x13, AC_VERB_SET_CONNECT_SEL, 0x0},

	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x15, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x15, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x16, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x16, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},

	{0x1b, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	{0x1b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},

	{ }
};

/*
 * Z71V pin configuration:
 * Speaker-out = 0x14, HP = 0x15, Mic = 0x18, Line-in = 0x1a, Mic2 = 0x1b (?)
 */
static struct hda_verb alc880_pin_z71v_init_verbs[] = {
	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x15, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	{0x15, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},

	{0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},
	{0x1b, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x1c, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},

	{ }
};

/*
 * 6-stack pin configuration:
 * front = 0x14, surr = 0x15, clfe = 0x16, side = 0x17, mic = 0x18,
 * f-mic = 0x19, line = 0x1a, HP = 0x1b
 */
static struct hda_verb alc880_pin_6stack_init_verbs[] = {
	{0x13, AC_VERB_SET_CONNECT_SEL, 0x00}, /* HP */

	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x15, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x15, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x16, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x16, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x17, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x17, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},

	{0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	{0x19, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x19, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	{0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},
	{0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	{0x1b, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	{0x1b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x1c, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},

	{ }
};

/*
 * Uniwill pin configuration:
 * HP = 0x14, InternalSpeaker = 0x15, mic = 0x18, internal mic = 0x19,
 * line = 0x1a
 */
static struct hda_verb alc880_uniwill_init_verbs[] = {
	{0x13, AC_VERB_SET_CONNECT_SEL, 0x00}, /* HP */

	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x15, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	{0x15, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x16, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	{0x16, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x17, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x17, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x0c, AC_VERB_SET_AMP_GAIN_MUTE, (0x7000 | (0x00 << 8))},
	{0x0c, AC_VERB_SET_AMP_GAIN_MUTE, (0x7000 | (0x01 << 8))},
	{0x0d, AC_VERB_SET_AMP_GAIN_MUTE, (0x7000 | (0x00 << 8))},
	{0x0d, AC_VERB_SET_AMP_GAIN_MUTE, (0x7000 | (0x01 << 8))},
	{0x0e, AC_VERB_SET_AMP_GAIN_MUTE, (0x7000 | (0x00 << 8))},
	{0x0e, AC_VERB_SET_AMP_GAIN_MUTE, (0x7000 | (0x01 << 8))},

	{0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	{0x19, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x19, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	{0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},
	{0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	/* {0x1b, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP}, */
	/* {0x1b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE}, */
	{0x1c, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},

	{0x14, AC_VERB_SET_UNSOLICITED_ENABLE, AC_USRSP_EN | ALC880_HP_EVENT},
	{0x18, AC_VERB_SET_UNSOLICITED_ENABLE, AC_USRSP_EN | ALC880_MIC_EVENT},

	{ }
};

/*
* Uniwill P53
* HP = 0x14, InternalSpeaker = 0x15, mic = 0x19,
 */
static struct hda_verb alc880_uniwill_p53_init_verbs[] = {
	{0x13, AC_VERB_SET_CONNECT_SEL, 0x00}, /* HP */

	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x15, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	{0x15, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x16, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	{0x16, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x0c, AC_VERB_SET_AMP_GAIN_MUTE, (0x7000 | (0x00 << 8))},
	{0x0c, AC_VERB_SET_AMP_GAIN_MUTE, (0x7000 | (0x01 << 8))},
	{0x0d, AC_VERB_SET_AMP_GAIN_MUTE, (0x7000 | (0x00 << 8))},
	{0x0d, AC_VERB_SET_AMP_GAIN_MUTE, (0x7000 | (0x01 << 8))},
	{0x0e, AC_VERB_SET_AMP_GAIN_MUTE, (0x7000 | (0x00 << 8))},
	{0x0e, AC_VERB_SET_AMP_GAIN_MUTE, (0x7000 | (0x01 << 8))},

	{0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	{0x19, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x19, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	{0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},
	{0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},

	{0x14, AC_VERB_SET_UNSOLICITED_ENABLE, AC_USRSP_EN | ALC880_HP_EVENT},
	{0x21, AC_VERB_SET_UNSOLICITED_ENABLE, AC_USRSP_EN | ALC880_DCVOL_EVENT},

	{ }
};

static struct hda_verb alc880_beep_init_verbs[] = {
	{ 0x0b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_UNMUTE(5) },
	{ }
};

/* auto-toggle front mic */
static void alc880_uniwill_mic_automute(struct hda_codec *codec)
{
 	unsigned int present;
	unsigned char bits;

	present = snd_hda_codec_read(codec, 0x18, 0,
				     AC_VERB_GET_PIN_SENSE, 0) & 0x80000000;
	bits = present ? HDA_AMP_MUTE : 0;
	snd_hda_codec_amp_stereo(codec, 0x0b, HDA_INPUT, 1, HDA_AMP_MUTE, bits);
}

static void alc880_uniwill_setup(struct hda_codec *codec)
{
	struct alc_spec *spec = codec->spec;

	spec->autocfg.hp_pins[0] = 0x14;
	spec->autocfg.speaker_pins[0] = 0x15;
	spec->autocfg.speaker_pins[0] = 0x16;
}

static void alc880_uniwill_init_hook(struct hda_codec *codec)
{
	alc_automute_amp(codec);
	alc880_uniwill_mic_automute(codec);
}

static void alc880_uniwill_unsol_event(struct hda_codec *codec,
				       unsigned int res)
{
	/* Looks like the unsol event is incompatible with the standard
	 * definition.  4bit tag is placed at 28 bit!
	 */
	switch (res >> 28) {
	case ALC880_MIC_EVENT:
		alc880_uniwill_mic_automute(codec);
		break;
	default:
		alc_automute_amp_unsol_event(codec, res);
		break;
	}
}

static void alc880_uniwill_p53_setup(struct hda_codec *codec)
{
	struct alc_spec *spec = codec->spec;

	spec->autocfg.hp_pins[0] = 0x14;
	spec->autocfg.speaker_pins[0] = 0x15;
}

static void alc880_uniwill_p53_dcvol_automute(struct hda_codec *codec)
{
	unsigned int present;

	present = snd_hda_codec_read(codec, 0x21, 0,
				     AC_VERB_GET_VOLUME_KNOB_CONTROL, 0);
	present &= HDA_AMP_VOLMASK;
	snd_hda_codec_amp_stereo(codec, 0x0c, HDA_OUTPUT, 0,
				 HDA_AMP_VOLMASK, present);
	snd_hda_codec_amp_stereo(codec, 0x0d, HDA_OUTPUT, 0,
				 HDA_AMP_VOLMASK, present);
}

static void alc880_uniwill_p53_unsol_event(struct hda_codec *codec,
					   unsigned int res)
{
	/* Looks like the unsol event is incompatible with the standard
	 * definition.  4bit tag is placed at 28 bit!
	 */
	if ((res >> 28) == ALC880_DCVOL_EVENT)
		alc880_uniwill_p53_dcvol_automute(codec);
	else
		alc_automute_amp_unsol_event(codec, res);
}

/*
 * F1734 pin configuration:
 * HP = 0x14, speaker-out = 0x15, mic = 0x18
 */
static struct hda_verb alc880_pin_f1734_init_verbs[] = {
	{0x07, AC_VERB_SET_CONNECT_SEL, 0x01},
	{0x10, AC_VERB_SET_CONNECT_SEL, 0x02},
	{0x11, AC_VERB_SET_CONNECT_SEL, 0x00},
	{0x12, AC_VERB_SET_CONNECT_SEL, 0x01},
	{0x13, AC_VERB_SET_CONNECT_SEL, 0x00},

	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x15, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x15, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},

	{0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	{0x19, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF50},
	{0x19, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	{0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x1b, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x1b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x1c, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},

	{0x14, AC_VERB_SET_UNSOLICITED_ENABLE, AC_USRSP_EN|ALC880_HP_EVENT},
	{0x21, AC_VERB_SET_UNSOLICITED_ENABLE, AC_USRSP_EN|ALC880_DCVOL_EVENT},

	{ }
};

/*
 * ASUS pin configuration:
 * HP/front = 0x14, surr = 0x15, clfe = 0x16, mic = 0x18, line = 0x1a
 */
static struct hda_verb alc880_pin_asus_init_verbs[] = {
	{0x10, AC_VERB_SET_CONNECT_SEL, 0x02},
	{0x11, AC_VERB_SET_CONNECT_SEL, 0x00},
	{0x12, AC_VERB_SET_CONNECT_SEL, 0x01},
	{0x13, AC_VERB_SET_CONNECT_SEL, 0x00},

	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x15, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x16, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x17, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},

	{0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	{0x19, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	{0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	{0x1b, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x1c, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},

	{ }
};

/* Enable GPIO mask and set output */
#define alc880_gpio1_init_verbs	alc_gpio1_init_verbs
#define alc880_gpio2_init_verbs	alc_gpio2_init_verbs
#define alc880_gpio3_init_verbs	alc_gpio3_init_verbs

/* Clevo m520g init */
static struct hda_verb alc880_pin_clevo_init_verbs[] = {
	/* headphone output */
	{0x11, AC_VERB_SET_CONNECT_SEL, 0x01},
	/* line-out */
	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	/* Line-in */
	{0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},
	{0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	/* CD */
	{0x1c, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},
	{0x1c, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	/* Mic1 (rear panel) */
	{0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	/* Mic2 (front panel) */
	{0x1b, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x1b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	/* headphone */
	{0x19, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	{0x19, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
        /* change to EAPD mode */
	{0x20, AC_VERB_SET_COEF_INDEX, 0x07},
	{0x20, AC_VERB_SET_PROC_COEF,  0x3060},

	{ }
};

static struct hda_verb alc880_pin_tcl_S700_init_verbs[] = {
	/* change to EAPD mode */
	{0x20, AC_VERB_SET_COEF_INDEX, 0x07},
	{0x20, AC_VERB_SET_PROC_COEF,  0x3060},

	/* Headphone output */
	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	/* Front output*/
	{0x1b, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x1b, AC_VERB_SET_CONNECT_SEL, 0x00},

	/* Line In pin widget for input */
	{0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},
	/* CD pin widget for input */
	{0x1c, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},
	/* Mic1 (rear panel) pin widget for input and vref at 80% */
	{0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},

	/* change to EAPD mode */
	{0x20, AC_VERB_SET_COEF_INDEX, 0x07},
	{0x20, AC_VERB_SET_PROC_COEF,  0x3070},

	{ }
};

/*
 * LG m1 express dual
 *
 * Pin assignment:
 *   Rear Line-In/Out (blue): 0x14
 *   Build-in Mic-In: 0x15
 *   Speaker-out: 0x17
 *   HP-Out (green): 0x1b
 *   Mic-In/Out (red): 0x19
 *   SPDIF-Out: 0x1e
 */

/* To make 5.1 output working (green=Front, blue=Surr, red=CLFE) */
static hda_nid_t alc880_lg_dac_nids[3] = {
	0x05, 0x02, 0x03
};

/* seems analog CD is not working */
static struct hda_input_mux alc880_lg_capture_source = {
	.num_items = 3,
	.items = {
		{ "Mic", 0x1 },
		{ "Line", 0x5 },
		{ "Internal Mic", 0x6 },
	},
};

/* 2,4,6 channel modes */
static struct hda_verb alc880_lg_ch2_init[] = {
	/* set line-in and mic-in to input */
	{ 0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN },
	{ 0x19, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80 },
	{ }
};

static struct hda_verb alc880_lg_ch4_init[] = {
	/* set line-in to out and mic-in to input */
	{ 0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP },
	{ 0x19, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80 },
	{ }
};

static struct hda_verb alc880_lg_ch6_init[] = {
	/* set line-in and mic-in to output */
	{ 0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP },
	{ 0x19, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP },
	{ }
};

static struct hda_channel_mode alc880_lg_ch_modes[3] = {
	{ 2, alc880_lg_ch2_init },
	{ 4, alc880_lg_ch4_init },
	{ 6, alc880_lg_ch6_init },
};

static struct snd_kcontrol_new alc880_lg_mixer[] = {
	HDA_CODEC_VOLUME("Front Playback Volume", 0x0f, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Front Playback Switch", 0x0f, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Surround Playback Volume", 0x0c, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Surround Playback Switch", 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME_MONO("Center Playback Volume", 0x0d, 1, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("LFE Playback Volume", 0x0d, 2, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE_MONO("Center Playback Switch", 0x0d, 1, 2, HDA_INPUT),
	HDA_BIND_MUTE_MONO("LFE Playback Switch", 0x0d, 2, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x0b, 0x1, HDA_INPUT),
	HDA_CODEC_MUTE("Mic Playback Switch", 0x0b, 0x1, HDA_INPUT),
	HDA_CODEC_VOLUME("Line Playback Volume", 0x0b, 0x06, HDA_INPUT),
	HDA_CODEC_MUTE("Line Playback Switch", 0x0b, 0x06, HDA_INPUT),
	HDA_CODEC_VOLUME("Internal Mic Playback Volume", 0x0b, 0x07, HDA_INPUT),
	HDA_CODEC_MUTE("Internal Mic Playback Switch", 0x0b, 0x07, HDA_INPUT),
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Channel Mode",
		.info = alc_ch_mode_info,
		.get = alc_ch_mode_get,
		.put = alc_ch_mode_put,
	},
	{ } /* end */
};

static struct hda_verb alc880_lg_init_verbs[] = {
	/* set capture source to mic-in */
	{0x07, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_UNMUTE(1)},
	{0x08, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_UNMUTE(1)},
	{0x09, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_UNMUTE(1)},
	/* mute all amp mixer inputs */
	{0x0b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_UNMUTE(5)},
	{0x0b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(6)},
	{0x0b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(7)},
	/* line-in to input */
	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	/* built-in mic */
	{0x15, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x15, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	/* speaker-out */
	{0x17, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	{0x17, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	/* mic-in to input */
	{0x11, AC_VERB_SET_CONNECT_SEL, 0x01},
	{0x19, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x19, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	/* HP-out */
	{0x13, AC_VERB_SET_CONNECT_SEL, 0x03},
	{0x1b, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	{0x1b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	/* jack sense */
	{0x1b, AC_VERB_SET_UNSOLICITED_ENABLE, AC_USRSP_EN | ALC880_HP_EVENT},
	{ }
};

/* toggle speaker-output according to the hp-jack state */
static void alc880_lg_setup(struct hda_codec *codec)
{
	struct alc_spec *spec = codec->spec;

	spec->autocfg.hp_pins[0] = 0x1b;
	spec->autocfg.speaker_pins[0] = 0x17;
}

/*
 * LG LW20
 *
 * Pin assignment:
 *   Speaker-out: 0x14
 *   Mic-In: 0x18
 *   Built-in Mic-In: 0x19
 *   Line-In: 0x1b
 *   HP-Out: 0x1a
 *   SPDIF-Out: 0x1e
 */

static struct hda_input_mux alc880_lg_lw_capture_source = {
	.num_items = 3,
	.items = {
		{ "Mic", 0x0 },
		{ "Internal Mic", 0x1 },
		{ "Line In", 0x2 },
	},
};

#define alc880_lg_lw_modes alc880_threestack_modes

static struct snd_kcontrol_new alc880_lg_lw_mixer[] = {
	HDA_CODEC_VOLUME("Front Playback Volume", 0x0c, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Front Playback Switch", 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Surround Playback Volume", 0x0f, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Surround Playback Switch", 0x0f, 2, HDA_INPUT),
	HDA_CODEC_VOLUME_MONO("Center Playback Volume", 0x0e, 1, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("LFE Playback Volume", 0x0e, 2, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE_MONO("Center Playback Switch", 0x0e, 1, 2, HDA_INPUT),
	HDA_BIND_MUTE_MONO("LFE Playback Switch", 0x0e, 2, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Line Playback Volume", 0x0b, 0x02, HDA_INPUT),
	HDA_CODEC_MUTE("Line Playback Switch", 0x0b, 0x02, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x0b, 0x0, HDA_INPUT),
	HDA_CODEC_MUTE("Mic Playback Switch", 0x0b, 0x0, HDA_INPUT),
	HDA_CODEC_VOLUME("Internal Mic Playback Volume", 0x0b, 0x01, HDA_INPUT),
	HDA_CODEC_MUTE("Internal Mic Playback Switch", 0x0b, 0x01, HDA_INPUT),
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Channel Mode",
		.info = alc_ch_mode_info,
		.get = alc_ch_mode_get,
		.put = alc_ch_mode_put,
	},
	{ } /* end */
};

static struct hda_verb alc880_lg_lw_init_verbs[] = {
	{0x13, AC_VERB_SET_CONNECT_SEL, 0x00}, /* HP */
	{0x10, AC_VERB_SET_CONNECT_SEL, 0x02}, /* mic/clfe */
	{0x12, AC_VERB_SET_CONNECT_SEL, 0x03}, /* line/surround */

	/* set capture source to mic-in */
	{0x07, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_UNMUTE(0)},
	{0x08, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_UNMUTE(0)},
	{0x09, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_UNMUTE(0)},
	{0x0b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(7)},
	/* speaker-out */
	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	/* HP-out */
	{0x1b, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	{0x1b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	/* mic-in to input */
	{0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	/* built-in mic */
	{0x19, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x19, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	/* jack sense */
	{0x1b, AC_VERB_SET_UNSOLICITED_ENABLE, AC_USRSP_EN | ALC880_HP_EVENT},
	{ }
};

/* toggle speaker-output according to the hp-jack state */
static void alc880_lg_lw_setup(struct hda_codec *codec)
{
	struct alc_spec *spec = codec->spec;

	spec->autocfg.hp_pins[0] = 0x1b;
	spec->autocfg.speaker_pins[0] = 0x14;
}

static struct snd_kcontrol_new alc880_medion_rim_mixer[] = {
	HDA_CODEC_VOLUME("Master Playback Volume", 0x0c, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Master Playback Switch", 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x0b, 0x0, HDA_INPUT),
	HDA_CODEC_MUTE("Mic Playback Switch", 0x0b, 0x0, HDA_INPUT),
	HDA_CODEC_VOLUME("Internal Mic Playback Volume", 0x0b, 0x1, HDA_INPUT),
	HDA_CODEC_MUTE("Internal Playback Switch", 0x0b, 0x1, HDA_INPUT),
	{ } /* end */
};

static struct hda_input_mux alc880_medion_rim_capture_source = {
	.num_items = 2,
	.items = {
		{ "Mic", 0x0 },
		{ "Internal Mic", 0x1 },
	},
};

static struct hda_verb alc880_medion_rim_init_verbs[] = {
	{0x13, AC_VERB_SET_CONNECT_SEL, 0x00}, /* HP */

	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},

	/* Mic1 (rear panel) pin widget for input and vref at 80% */
	{0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	/* Mic2 (as headphone out) for HP output */
	{0x19, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	{0x19, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	/* Internal Speaker */
	{0x1b, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x1b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},

	{0x20, AC_VERB_SET_COEF_INDEX, 0x07},
	{0x20, AC_VERB_SET_PROC_COEF,  0x3060},

	{0x14, AC_VERB_SET_UNSOLICITED_ENABLE, AC_USRSP_EN | ALC880_HP_EVENT},
	{ }
};

/* toggle speaker-output according to the hp-jack state */
static void alc880_medion_rim_automute(struct hda_codec *codec)
{
	struct alc_spec *spec = codec->spec;
	alc_automute_amp(codec);
	/* toggle EAPD */
	if (spec->jack_present)
		snd_hda_codec_write(codec, 0x01, 0, AC_VERB_SET_GPIO_DATA, 0);
	else
		snd_hda_codec_write(codec, 0x01, 0, AC_VERB_SET_GPIO_DATA, 2);
}

static void alc880_medion_rim_unsol_event(struct hda_codec *codec,
					  unsigned int res)
{
	/* Looks like the unsol event is incompatible with the standard
	 * definition.  4bit tag is placed at 28 bit!
	 */
	if ((res >> 28) == ALC880_HP_EVENT)
		alc880_medion_rim_automute(codec);
}

static void alc880_medion_rim_setup(struct hda_codec *codec)
{
	struct alc_spec *spec = codec->spec;

	spec->autocfg.hp_pins[0] = 0x14;
	spec->autocfg.speaker_pins[0] = 0x1b;
}

#ifdef CONFIG_SND_HDA_POWER_SAVE
static struct hda_amp_list alc880_loopbacks[] = {
	{ 0x0b, HDA_INPUT, 0 },
	{ 0x0b, HDA_INPUT, 1 },
	{ 0x0b, HDA_INPUT, 2 },
	{ 0x0b, HDA_INPUT, 3 },
	{ 0x0b, HDA_INPUT, 4 },
	{ } /* end */
};

static struct hda_amp_list alc880_lg_loopbacks[] = {
	{ 0x0b, HDA_INPUT, 1 },
	{ 0x0b, HDA_INPUT, 6 },
	{ 0x0b, HDA_INPUT, 7 },
	{ } /* end */
};
#endif

/*
 * Common callbacks
 */

static int alc_init(struct hda_codec *codec)
{
	struct alc_spec *spec = codec->spec;
	unsigned int i;

	alc_fix_pll(codec);
	alc_auto_init_amp(codec, spec->init_amp);

	for (i = 0; i < spec->num_init_verbs; i++)
		snd_hda_sequence_write(codec, spec->init_verbs[i]);

	if (spec->init_hook)
		spec->init_hook(codec);

	return 0;
}

static void alc_unsol_event(struct hda_codec *codec, unsigned int res)
{
	struct alc_spec *spec = codec->spec;

	if (spec->unsol_event)
		spec->unsol_event(codec, res);
}

#ifdef CONFIG_SND_HDA_POWER_SAVE
static int alc_check_power_status(struct hda_codec *codec, hda_nid_t nid)
{
	struct alc_spec *spec = codec->spec;
	return snd_hda_check_amp_list_power(codec, &spec->loopback, nid);
}
#endif

/*
 * Analog playback callbacks
 */
static int alc880_playback_pcm_open(struct hda_pcm_stream *hinfo,
				    struct hda_codec *codec,
				    struct snd_pcm_substream *substream)
{
	struct alc_spec *spec = codec->spec;
	return snd_hda_multi_out_analog_open(codec, &spec->multiout, substream,
					     hinfo);
}

static int alc880_playback_pcm_prepare(struct hda_pcm_stream *hinfo,
				       struct hda_codec *codec,
				       unsigned int stream_tag,
				       unsigned int format,
				       struct snd_pcm_substream *substream)
{
	struct alc_spec *spec = codec->spec;
	return snd_hda_multi_out_analog_prepare(codec, &spec->multiout,
						stream_tag, format, substream);
}

static int alc880_playback_pcm_cleanup(struct hda_pcm_stream *hinfo,
				       struct hda_codec *codec,
				       struct snd_pcm_substream *substream)
{
	struct alc_spec *spec = codec->spec;
	return snd_hda_multi_out_analog_cleanup(codec, &spec->multiout);
}

/*
 * Digital out
 */
static int alc880_dig_playback_pcm_open(struct hda_pcm_stream *hinfo,
					struct hda_codec *codec,
					struct snd_pcm_substream *substream)
{
	struct alc_spec *spec = codec->spec;
	return snd_hda_multi_out_dig_open(codec, &spec->multiout);
}

static int alc880_dig_playback_pcm_prepare(struct hda_pcm_stream *hinfo,
					   struct hda_codec *codec,
					   unsigned int stream_tag,
					   unsigned int format,
					   struct snd_pcm_substream *substream)
{
	struct alc_spec *spec = codec->spec;
	return snd_hda_multi_out_dig_prepare(codec, &spec->multiout,
					     stream_tag, format, substream);
}

static int alc880_dig_playback_pcm_cleanup(struct hda_pcm_stream *hinfo,
					   struct hda_codec *codec,
					   struct snd_pcm_substream *substream)
{
	struct alc_spec *spec = codec->spec;
	return snd_hda_multi_out_dig_cleanup(codec, &spec->multiout);
}

static int alc880_dig_playback_pcm_close(struct hda_pcm_stream *hinfo,
					 struct hda_codec *codec,
					 struct snd_pcm_substream *substream)
{
	struct alc_spec *spec = codec->spec;
	return snd_hda_multi_out_dig_close(codec, &spec->multiout);
}

/*
 * Analog capture
 */
static int alc880_alt_capture_pcm_prepare(struct hda_pcm_stream *hinfo,
				      struct hda_codec *codec,
				      unsigned int stream_tag,
				      unsigned int format,
				      struct snd_pcm_substream *substream)
{
	struct alc_spec *spec = codec->spec;

	snd_hda_codec_setup_stream(codec, spec->adc_nids[substream->number + 1],
				   stream_tag, 0, format);
	return 0;
}

static int alc880_alt_capture_pcm_cleanup(struct hda_pcm_stream *hinfo,
				      struct hda_codec *codec,
				      struct snd_pcm_substream *substream)
{
	struct alc_spec *e for= codec->e fo;

	snd_hda_igh D_cleanup_
 * Un(igh D,
				 ch fe fo->adc_nids[s/*
 * Un->number + 1]);
	return 0;
}


/*
 */
static al Inteo Copcmaudio ierfa880.com.analog_playback = {
	.82 codecss = 1,
	.channels_min = 2m.tw>
 *      ax = 8,
	/* NID is set inerfacbuild.comsYang	.opek.c{
	 Jone            PeiSen .com.the ce p.prepare<jwoithe@physics.adelai>
 *
 *du.au
 * HD <jwoithe@physics.adelai
 * HD 
	},
};
g <kailang@realtek.com.tw>
 *                   captu *  T <pshou@realtek.com.tw>
 *                    Takashi Iwa    iwai@suse.de>
 *                   under the terms of the GNU General Public License aalt  PeiSen Hou <pshou@realtek.com.tw>
 *                    Takashi Iwaf the License, or
 *  (at your option) any later version.
 *
 *  This driver is distributed ins published by
 *  the Free2, /* can be overridden     J
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  ME Jonathan Woi>
 *
 *  This dri See the
 *ree software; you can redistributeot, write to thedify
 *  it under the terms of the GNU General Public Lidigitaln the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  ME Jonathan Woithe <jwoithe@pdi   PeiSen delaide.edu.auclos*  This driclude "hda_local.de "hdu.au>
 *
 *  This driclude "hda_local.oftware; you can redistribute

#define ALC880_FRPlace, Suite 330, Boston, MA  02111-1307 USA
 */

#include <lis published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any /* Used by                to flag that a PCM has no  PeiSen Htw>
 * ang <kailang@realtek.com.tw>
 *    .com.null.tw>
 * hed by
 *  the Free0m.tw>
 *           WILL,
	ALC880_Ui Iwa0t under the tinterfac          (ng@realtek.igh D *igh Diversal Interface for Intel High Definitiosal Intetek.com *info =or ALC com.rC880_S700iion igh Defnum      = 1;/* last tcom.C880_AUC880ion if (r ALC no       )
		goto skip       ion Auprintf_BASIC,
 * Un_name       , sizeoHP_3013,
	ALC260_FUJITSU_S)ce p "%s A     ",High Defchip60_FUght C880->0_FU_AUTO,
	A
	ALC260_FUJITSU_S;
	LC260_BASIC,
	ALC26        PeiSen )an Wo260_Bnd_BUG_ON(!r ALC multiout.da0/880/)HP,
 (c) 200-EINVAL;_LAS00,
#
 * Un[SNDRV_PCM_STREAM_PLAYBACK] = *ndif
	ALC260_AUTO,
	ALC260_MODSIC,
	ALC262_HIPPO,
	ALC262_HIPPO_1,
	ALC26.nid_AUTO,
	A/* ALC262 models [0];
	}T,
#endif
	ALC260_AUTO,
	As publiDEL_LAST /* last tag */
};

260/880/*/
enum {
	ALC262_BASIC,
	ALC262_HIPPO,
	ALC262_HIPPO_CAPTURE62_FUJITSU,
	ALC262_HP_BPC62_SONY__3000,
	ALC262_NEC,
	ALC262_TOSHIBA_S06,
		ALC262_HP_TC260/880/8_RP5700LC260_BASIC,>
 *   _modY_ASSAMDPC_D7000_WL,
	ALC262_HP_BPC_D7000_WF,
	,
	ALC880_CLEVO,SIC,for (iCER_D i <or ALC ag *C267_QUANTA_; i++DEL_LA
enum {
	ALC267_QUANTA_[iCER,
	ALC2 >um {
68_3ST,
	ALC268_TOSHIBA,
	ALC268_ACER,
	ALC268_ALC268_Z	ALC268_3ST,
	ALC268_TOSHIBA,
	ALC268_ACER,
	ALC268_ACER,
#ifdef CONFIG_SND_DEBUG
	ALCSIC,700,700,}

260_HP_DC760:the LSPDIF ,
	Atw>
 * index #1e.h>
260_BASIC,/* ALC262 ig_out/880 ||9_QUANT	ALCin/880DEL_LAALC260_HP_3013,
	ALC260_FUJlude <lce pa02X,
	ALC260_ACER,
	ALC260lude <l
	ALCC260_RDude <lR_672V,
	ALC260_FAVORIT* last tag */
};

/2;
tch fo  High DefilavST_DIC269;

/
	ALC269_AUTO,
S_LAPTOP,
	ALCSIC,
	AL_AUTO,
	ALC880_MopyrSIC,
	ALC2ifdef CONFIG_SND_DEBUG
	Alude <lSIC,
60_BASIC,	ALC269_type/
enuALC268com.1VD_861_AUTO,,
	ALC861VD_ALC8elseST,
	ALC861VD_3ST_DIGHDALC262TYPE_	ALC2660VD_ASUS_V1S69_AUTO,
	ALC269_MODE&&ALC2 for ALC 
	ALC26lude <linux/initLC268_ZE	ALC262_HIPPO,
	ALC262_HIPPO_1,
	ALC262_FUJITSU,
	ALC262 models */
enum {C_P70BPC_D7000_WL,
	ALC262_HP_BPC_D7000_WF,
	ALC262_HP_TC_T5735,
	AALC269_MODALC83,
	D_ASUS_V1S,
	At tag DEL_LAST,
};

/* ALC662 models 62_SONY_ASSAM00,
	ALC262_NEC,
	ALC262_TOSHIBA_S06,
	ALC262_TOSHIBA_RX1C663_ASUS_G50V,
C662_LENOVO_101E,
	ALC662_ASUS_EEEP/* last tag */
};

/*_ASUS_G71VSUS_M51V/* FIXME: do we need this69_FUall RealtekHigh D NTA_ls?e.h>
* last tspdif_ <kaus_rede>

/* AL */
enum {
	AL
	ALC260_HP,
(c) 2004 Kthe LIf the use of more,
	An one ADCuse.requested69_FU882_current
	 *,
	ALC_672nfigblisa second        s publi-only80_A.
	AL/the LAdditionalREPLAa	ALC885_MB69_FU	ALC2692LIFEBOOK,_BASIC,d in models &&
};

/* ALC662tributed in the hop) ||ASUS,
_BASIC,
um_260/880/ > 1_6ch,
	ALC883_6ST_DIG,
	ALC862_SONY_DEL_LA last tag */
};

/3

/* ALC861-VD models */
en1_AST100,
#ifdef CONFIG_SND_DEBUG
	ALC260_TESVA,
	ALC663	ALC883_3ST
	ALC662_ECS,
	ALC663_ASUS_MODE1,
	AL1,
	ALC262_e pat Inte	ALC260_AUTO,
	Ad in the hopC662_LENOVO_101E,
	ALC662_ASUS_EEEPC_P701,
	ALC662e patC888_ACER_ASPIRE_SUS_M ALC8730G,
	ALC883_MEDION,
	ALC883_MEDION_MD2,
	ALC883_LAJITSU,
	ALC880_UNIWC662_LENOVO_101E,
	ALC662_ASUS_EEEPC_P701,
	ALC662__DMICM51VA,
	ALC663GA_2ch_DIG,
	ALC
	ALC662_ECS,
	ALC663_ASUS_MODE1,
	ALC662_ASUS_83_LAPTOP_EAPD,
	ALC883_LENOVOs publi63_ASUS_MODE4,
	ALC663_ASUS_MODE5,
	ALC663_ASUS_MG,
	ALC888_L60/880/81RP57ALC262_AUTO,
	ALC262_MODEL_LAST /* last thou@realtek.G,
	ALC888_GA_2ch_DIG,
	-num {
C883_HAIER_W66,
	ALC888_3ST_HP,
	ALC888_6S_INTEL,
	ALC888MITAC,
	ALC883_CLEVO_M540R,
	ALC883_CLEVO_M720,
	ALC88/* last tag */
,
	ALC88 */
(c) 2004 Kail <kailavoiderfacfree_kctl
	ALC880_LG_LW,
	ALC880_MEDION_RIM,
#ifdef CONFIG_SND_DEBUG
	ALC880EBOOK,
	ALC2d cha.list*/
};

l Inte* lakcontrol_new *d ch861_AUTO,-1)

strucACER_ALAST IC,
	ALC268_ACER_ASPIRE_-1)

susedL,
	AL	ALCksign(d chD_DE_FAVORIT}n Audiarraynsign(&mixer array){
	hda_nid_t pin;
	unsignr mux_idx;
	unsigned char amix_idx;
};

#define MUX_IDX_UNDEF	((unsigned*/
};
	ALC882_6unsi
	unsigned char880_ME;
 int nutruct80_Tudio Codetach_beep_device/* initia}

#ifdef SND_D_DANEEDS_RESUME880_TCL_S700,
	Aresumigned int beep_amp;	/* beep amC260 modatch_ops.init/* initialAudio Codec
 *_verbs_ampeam */
	struct hda_pcm_stream *cachL
						 *ic_route {
	h#endiflang Yang <kailang@realtek.dec
 *onatJITSUnalog PCILL_DIG      odec paybac,
	ALC880_ hda_pcmm.tw           stream *stre    m.tw str

	char strm.twsign

	charsignm.twunsol_evental[32];struct hda_,ermination!
						 */
	unsign	._verbs

	char_verbs,g_capturrminatiCONFIG_on!
				POWER_SAVda_pche_locower/* ALC8

	char_multi_out multiouigital_caundeang Ya TestUS_A7M,
	a_IMA69_FUdebugging
 ls, dAlmosterfl inputs/outptio 
 * enabled.  I/O pinsnse for S_A7M,
	Ad vias, denumhda_a_pcm. Yangpture;

	/* playbaDEBUGda_nid_to Conid_terfa    tesLC883_3STs[462_F{
	0x02, 0x03num_a4num_a5 any later version.
 *
 opti_muxpe;

	/* captwrite tosourcished byGA_2itee;

	7me_diigital-n Wo{ "In-1"num_a }ce p*/

	/2 capt1re source */3 capt2re source */4 capt3re sourceCD capt4re sourceFront capt5re sourceSurround capt6re soit under the terms of theC267_QUANTA_pe;

	/* captNTA_	unsigned {c LiNULLre so{ 4/
	const stru6/
	const stru8/
	const sALC880_TCL_S700,
	A captpin_ctls */
	ALC880_	/* codec pateri3];	/*ce patcspec {
	/* _dacelels */
e*u62_3iversalkailachar *texts[signed 	"N/A", "Line Ouned "HPin alcec[3In Hi-Zlc_bIn 50% dynamiGrructnami8 controls100%"
	};
	forma->3ST_DIGO,
	ALCTL_ELEAS,
	ALENUMERATED/
	struct coua_pcmum {struct value._outeratedID; optio8RIT1f (input_mux private_imux[3];
	 >= 8gnedivate_dac_nids[AUTO_CFG_MAX_O= 780_TEScpyrivate_dac_nids[AUTO_CFG_0_FU, a_pcm input_mux private_imux[3];
	ight (c) 2004 Kail_channel_mode;
	int need_dacget;
	int const_channel_count;
	int extchannel_count;

	/* ux pr *uunt;
	iiversal InteLG_LW,
	ALC880_M861_/* codec parLC26(ount;
	im *sig_out_typALC262(ig_out_ty)ount;
	i->private int rda_insigned_S700need_da, s[AUTO_T_DIGneed_da int se hda_pcm_stradnterfacpres,LL_P5patch foAC_VERB_GET_PIN_WIDnt iCONTROL, 0ORIT1f (lags */
&nly PINautoOUT_ENDEL_LAST /ter */
	hda_nid_t vmHPer_n	ALC8[AUTO__ACERALC861VD_[AUTO_ */
e883_HA master */
	hda_nid_t vmINer_nid;
#switchdef CONFIG_SND_HDA_POWVREFr_nid;
#cased int pll_coef__HIZ:

	/* ot3; breach,
	_coef_bit;
};

/*
 *50: 

	/* ot4n template - to be copied to thGRDnfigurati5n template - to be copied to th8 spec insta6n template - to be copied to th100nfigurati7n template  alc_	);

	/* _mux private_imux[3];
	[062_Fheck*/
	void (*init_hook)(struct hda_codec *codepu;
	void (*unsol_event)(struct hda_codec *codec, unsigned int res);

	/* for pin sensing */
	unsigned int sense_updated: 1;
	unsigned int jack_present: 1;
	unsigned int master_sw: 1;
	uns*/
	strigned int aut cha pcm_rec[0,da_nid_t vmaster_da_nid_t dig_in_ni |SND_HDA_POWER_SAitalLL fix */
	hda_um_channel_mo/*
 * coconst struct hda_channel_mode *channe5gitalt struct hda_channel_mode *channeGRDnel_count;
	unsigned int num_mux_defs8nnel_count;
	unsigned int num_mux_defs10WILL*/
	sgned int autotrea:1;
new*setunsid (*set
	unsigned int no_analog :1; /* digital I/O only */
	int init_amp;

	/* for virtual)(strucl Hipsrcntrol_new *cap_mixer; /* capture mRP57 masa_codec !=p)(strucIL1,
	ALt vLC660VAudio Codec
 *writhda_pcm_strea; /* digital I	only */
	iSt init_amp;

	/* for v     str
static _newva_ampntrol_new *cap_mixer; /* capture mi>= 3 ?tal D_DAAMP_MUTE :structAudio Codec
 *am audereo *kcontrol,
	D_DAOUTPUT
			     stec *spec = c,mux_dec *(c) 200 */
enic_route {
	hda_nid_tl_mode;
	int needsr];	/ix;
	int const_channel_count;
	int ext_channel_count;

	/* PCM information */
	struct hda_pcm pcm_rec[3nsigned [3];
	struct"CLFErol,
ideux */
	struct auto_pin_cfg autocfg;
	struct snd_array kctls;
	struct hda_input_mux private_imux[3];
	hda_4id_t private_dac_nids[AUTO_CFG_MAX_OUTS4;
	hda_nid_t private_adc_nids[AUTO_8_ACMAX_OUTS];
	hda_nid_t private_capsrc_nids[AUTO_CFG_MAX_OUTS];

	/* hooks */
	void (*init_hook)(struct hda_codec *nd_hc);
	void (*unsol_event)(struct hda_codec *codec, unsigned int res);

	/* for pin sensing */
	unsigned int sense_updated: 1;
	unsigned int jack_present: 1;
	unsigned int master_sw: 1;
	unsigned int autsect hdsec *);
	void (*init_hook)(struct hda_cnly */
	int iCONNECT_SE virtualntrol_new *cap_mixer; /* capture mixet ad&->id)}

static int alc_mux_enum_put(struct sndigned int num_dacs;
	hda_nid_t *dac_nids;
	hda_nid_t dig_out_nid;		/* optional */
	hda_nid_t hp_nid;		/* optional */
	hda_nid_t *slave_dig_outs;
	unsigned int num_adc_nids;
	hda_nid_t *adc *imux;
	unsigned int adc_idx = snd_ctl_get_ioffidx(kcontrol, &ucontrol->id);
	unsigned_nids ?t pritrol_new *cap_mixer; /* capture mi!apsrcnid_t p adc_ux[adc_idx];
		unsigned int i, idx;nids ?m_info(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_>id);
	unsign	idxmux_idx >= spec->num_mux_defs)
#defed init_autoTEST(xapsrcRE_773			\u.auifa_in_nn_cfg autocfg;
IFACE_MIXER, (i =	capsr = signedatch fo  _codecC880_AUde;
	int need_dac_fix,d, HDA_godelst hda_codec *codec);,nid, HDA_putal[32];erbs[5];
	unsign	}
		*cur_vter_sw: 1;
	 =presdec, nid, HDA>num_items; i++SRC
			unsigned int v = (i == idx) ? 0 : HDA_AMP_MUTE;
			snd_hda_codec_amp_stereo(codec, nid, HDA_INPUT,
						 imux->nd_hda_i.index,
						 HDA_AMP_MUTE snd_kc	}
		*cur_val = idx;
		return>adc_nielse {
		/* MUX style (e.g. ALC880) */
		retu <kailang@real	/* codec parametint_mic;

	/*ixer pcm_recD_DACODEC_VOLUME(*kcont PPeiSen HVolume captucnum_a_get_ioffidx
	ALdec->spec;
	return3];
	strhda_ch_mode_info(codec dignfo, spec->channel_mode,
				    spect shda_ch_mode_info(codece uinfo, spec->channel_mode,
				    specidehda_ch_mode_info(codecf int alc_ch_mode_get(struBINDc = crn snd_hda_ch_modSll_ni(codec, u2_get_iINtrol);
	struct alc_spec->num_channel_mod->spec;
	retdrn snd_hda_ch_mode_get(codec, ucol *kcontrol,
	->spec;
	retern snd_hda_ch_mode_get(codec, uconda_codec *cod->spec;
	retfrn snd_hda_ch_mode i++) {
			unn snd_hdin Modo(code14snd_ctl_elem_valuec->num_charol)
{
	struc5snd_ctl_elem_valuel *kcorol)
{
	struc6 hda_codec *codec =da_corol)
{
	struc7snd_ctl_elem_value
	/*hda_ch_mode_put8codec, ucontrol, spec2hda_ch_mode_put9codec, ucontrol, spec3hda_ch_mode_putacodec, ucontrol, spec4hda_ch_mode_putbsnd_ctl_nput_mux_spec->chanSdig_imode,
				     c->multiout.mannel_nnels = spec      &spc->multiout.mel_counnels = specerr >= 0 c->multiout.mst_channels = spec) {
	dec->spec;
	returnpec->ca_ch_mode_info(codecb int alc_cha_ch_mode_get>spec;lc_spehe mode of pin->spec;
	retings via the mixer.  "pc" is usontrol the 2ode of pin widget settings 1ia the mixer.  "pc" is used
 * ins
 * being p to avoid consequpecifier.  Maximum allowedontrol the 3ode of pin widget settings  snd_hda_ch_mode_get is used
 * insome retaski to avoid consequseem to ignore requests foontrol the 4ode of pin widget settings 3ia the mixer.  "pc" is used
 * ins of these
  to avoid consequ Therefore order this list_kcontrolDode of pin widget settings 4ia the mixer.  "pc" is used
 * ove through  to avoid consequally.
 * NIDs 0xn Woi idx) ? 0 : HDA_AMP_MUTE;
			snd_hdu.au_amp_st"C
 *   l)
{
	s MarcNPUT,
				ch/* chnt alcu.au						 HDApc bias"rol u.au>al = idx;pc bias"_eleic_ro	{ }icenendon) any later version.
 *
verbstruct alc_sp strVREF5rc_nids;
_DIGnmutee optioMA,
dec, -trol,MB5,
{dec, uuct snd_ctl_specGAIN>id);
	specIN_UNlc_sp0)ed cht all 5 options, or it can limit the options ba1ed
 * in dl 5 options, or it can limit the options based
 * in  output pin.  In
 * addition, "input" pins mnput or anel 5 options, or it can limit the options based
 * in  widget capability (NIDs 0x0f and 0x10 don'tnput or anfl 5 options, or it can limit the options based
 * in arch 2006) and/or
 * wiring in the computer.nput o/*de_i al
			69_FUdec,-can present all 5 options, or it can limit the asteZERO or may not process the mic bias option
 *efine ALC_PIN_DI widget capability (NIDs 0x0f and efine ALC_PIN_DIarch 2006) and/or
 * wiring in theefine ALC_PI1,
	etOUT     _dac_ruct-put(present 14l 5 options, onit_amp;

	/* for vinit_OUTd
 * in15um values are given.
 */
static signed char alc_p6um values are given.
 */
static signed char alc_p7um values are given.
 */
static signed char a PIN_HP,
}direction the minimum and maximum values are rent pin direction modeions bar alc_pin_mode_dir_inf_PIN_DIR_INOUT_NOMICBIAS */
};
#definN_DIR_IN */
	{ _PIN_DIR_INOUT_NOMICBIAS */
};
#defin 0, 4 },    /* _PIN_DIR_INOUT_NOMICBIAS */
};
#dr each  optiion the m8nimuc and maxi8um values are given.
 */
static signedoef_80ar alc_p9in_mode_min(_dir)+1)

static int alc_pin_mode_info(saum values are given.
 */
static signedINar alc_pblem_info *uinfo)
{
	unsigned int item_num = uinfll 5 options, oo)
{
	unsigned int item_num = /* MP,
};
/* _pin_mode_max(bdir)-alc_pin_mode_min(_drent pin direction mode*/
};
#defintruct snd_kcontnt = 1;
	uinfo->value.enumerated.itelem_info *uinfnt = 1;
	uinfo->value.enumerated.ito->value.enumernt = 1;
	uinfo->value.enumerate	ALCDC.de>
uppresent air_info[_dir][1])
#define alc_pin_an limiased
 * in  0, 4 },    /* >id);
	unsignex0de_info(0ED;
	uinfo->count = 1;
	uinfo->val_num]);
	return 0;pin_mode_min(_dalc_pin_mode_get(struct sems = alc_pin_mode_n_items(dir);

_num]);
	return 0;truct snd_kcontalc_pin_mode_get(stru	ALCO,
	AL opti/passthrupresent air))
		item_num = alc_pin_mode_min_num]);
	return 0; (kcontrol->private_value >> 16) & 0xff;
	nput or an (kcontrol->private_value >> 16) & 0xff;
	2 int pinctl = snd_hda_codec_read(codec, nid, 0,
			3 int pinctl = snd_hda_codec_read(codec, nid, 0,
			4ed
 *  }t nu_capture;
	stru snd_kcocons muct hde;

	/*
	ALC6[ALC    MODEL_LASTsigned dir) && 3ST]		= "3stackpc bdir) && TCL_S700]tl)
tcl
	*valp = i 3ST_DIGin_mo		i++;-digo_pcms(dir) && CLEVOnctl)
clevo	return 0;
}5pinctl)
5	i++;
	*valp = i 5 alc_pin_mosnd_kc(dir);
	return 0;
}W810nctl)
w810	return 0;
}Z71Vnctl)
z71v	return 0;
}6pinctl)
6	i++;
	*valp = i 6 alc_pin_mo snd_k(dir);
	return 0;
}ASUSnctl)
asusd = kcontrol->pr_Whangte_valu-wruct hda_codec	unsic_pin_mor dirdig(kcontrol->private_v2alue >> 16) &2	return 0;
}UNIWILLte_value uniwil_max(dir) ? iue;
	unsP53ed int pinct-p53	return 0;
}FUJITSUd chafujitsu					 AC_VERB1734nctl)
	 0x0	return 0;
}LGnctl)
l& 0xff;
	longLG_LWode_min(-lw	return 0;
}MEDION_RIMd chamedion"_digital_r auto-parsing */valp = i <Epinctl)
 cap(dircapturontrol->prUTatic inautc_piunder the terms oft sepci_quirkstruct acfg_tbl pcm_recon!
PCI_QUIRK(0x10temsorte69trucoeus G610P", em_value *u
	ALc, nid, 0,
					  AC_VERa880, "ECSONTROL,
		trol,
	 alc_pin_mode_values[val]);

		4, "Acer APFVONTROL,
		6Sannelc, nid, 0,
					  25e_get(7/* AULIONTROL,
		: alc_pd pin mode.  Enum values of 2 o7 less are
		 * i(kcontrd pin mode.  Enum values of 2 o8hing the input/output buffers probably
		 * reduces n8ching the input/output buffers probably
		 * reducese3hda_ess are
		 * input modes.
		 *
		 * Dynamically se31r less are
		 * inpud pin mode.  Enum valu3C_VER123ct hda_e input/output buffers probably
		 * r3, uin2a enabHCONTROL,
		5out to be necessary in t4dc_ni10b3red
SUS W1r the reque	unsignePUT, 0,
						 HDA_AMP_MUTE, Hc2_AMP_MUTE6	/* trol->private_vamp_stereo(codec, nid, HDA_INPUA_AMP_MUTExx HDA_AMP_MUTE, 0);
		} else {
			snd_hda_codec_11A_AMP_MUec, nid, HDA_INPUT, 0,
						 HDA_AMP_MUTE, HDA_2MP_MUTE);
			snd_hda_codec_amp_stereo(codec, nid, HDA_OU7MP_MUTE);
			snd_hda_codec_amp_stereo(codec, nid, HDA_O96ired
_MUT cha HDA_AMP_M cha
	AL1,
	id, dir) \
	{ .iface = SNDRV_CTL_E HDA_AMP_MUTE, 0);
	MB5,
name, .index = 0,  \
	  .i}

#define ALC_PIN_MODE(xname, nid, dir) \
	{ .iface = SNDA_AMP_MU HDA_AMP_MUTE, 0);
		} else {
			snd_hda_codec814eV_CTL_ELP5GD1 w/	ALC2he input/output buffers probably
		 * r.  Multi81e GPIOs c4GPL/* A switch control for ALC260 GPIO pins.  Multi96e GPIOs can b the requested pin mode.  Enum valu.  Multibfo = alc_pin_mode_sted pin mode.  Enum_VENDORed for an) }

/* A switch co)Licendefault TL_EL.get = alc_pin_mode_getc int81a/* ASonythis turns out to be necessary in t_gpio_dadt mot(struct snd_kcontrol *kcontrol,
			   7
 * ca03T, 0Gatewatruct snd_kOUTPUT, 0,
						 HDA_AMP_codec *cne a = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid =4the frol->private_value & 0xffff;
	unsigned cha297_VERc79a_gethuttle ST20G5he input/output buffers probably
		 * 458]);

1t nu"Gigabyte K8ger.value;
	unsigned int val = snd_hda_6 num_115/* AMSthe input/output buffers probably
		 * 5hda_0x925d, "FIC P4Mal & mask) != 0;
	return 0;
}
static incodec_052/* AC alc m520G HDA_AMP_M

staol *kcontrol,
			     struct 66d_ctl_elem_655ne *ucontrol)
{
	signed int change;
	struct 540 set.  T HDA_AMP_MUTE, 0);2	return 0;
}
static in6s of 82ad(coBiostar enable the retasking pin's input/outp5uire0x90alp =U pinctl DA_AMP_Mue;
	unsign6) & 0xff;
	long val = *ucontroiredaluelinteger.valu	 0x06) & 0xff;
	long val = *ucontror leslue.integer.value;
	uns	    AC_VERB_GET_GPIO_DATA,
			ching pinct P
			ger.value;
	unsiP53d pin mode.  Enum val61ntrol203o_dae *uONTROL,
					  alc_pin_mode_values[k);
	if (io_daMe_min Rim 215)
		gpio_daal = alc_p= ~mask;
	else
		gpio_9s of 40 not"EPoX enable the retasking pin's input/outp_SET_GPIO1d(coEPox EP-5LD	 HDA_AMP_Me retasking pin's input/outp7e.
	id_t c_datSC if (valnid, 0,
						    AC_VERB_GET_GPIOEM_IFACE_9ired, .nAmilo M1451ue *ucontroB_GET_P= 0,  \
	  .info = alc_gpio_daaIXER, .);

	/* Set/unset the masked GPIO bit(EM_IFACE_b/* AFGET_CONTc_gpio_data_get, \
	  .put = alc_gpi8c_reget(_pin"LG LW2)
		gpio_da> alcitch control to allow the enab3bng ofdigital IO pitch control to allow the enab6ing of wthis stage ic; the intention of this controded *of theeger.value; pins on the
 * ALC260.  T9dmask =18ing TCL lc_pDEBUG */

<= alc_pd pin mode.  Enum va26l is0x808_mode *e input/output buficenbroken BIOono_info

static int aomplets a more	 */
		if (val <= 2) {
			snd_hda_codec_ssary.
 a *, t_vetel mobo nid, mask) \
	{ .iface = SNDRV_CTL_Essary.
 d4nd_ctl_boolean_mono_info

static int alc_spdif_ctrl_get(struct  setl_boolean_mono_info

static int alc_spdif_ctrl_get(struct d(col_boolean_mono_info
nput modes.
		 *
		 * Dynamssary.
 e22iredl_boolean_mono_info

static int alc_spdif_ctrl_get(strue305);
	hda_nid_t nid = kcontrol->private_value & 0xffff;
	unsi3alue
	hda_nid_t nid = kcontrol->private_value & 0xffff;
	unsit snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value eucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kconerol);
	hda_nid_t nid = kcoe retasking_ctl_booleal_bool.get = alc_pin_moD_DEBUG
#domplet
	hda_nid_t nid = kcontrd pin mode.  Enum vaa0ta_gct sa_codAOhe <i915GMm-HFo enable the retasking pin's input/oute8adc_ni  AC_V	 */
		if (val <= 2) {
	{alc_pnels, dir) &&2_AUTO,p2 mods Yang <kailang@real,
};
_A7M,T		0de>
        ger.va pcm_reces[i] != pinthan Woiec *sybackstruct alhigne	i++;pec *sre soudigitN_IN, T_DIGI_CONVEv_info0, PIN_IN, {
	str        in_		i++;0, PIN_IN, x00);

GA_2dacnsetARRAY_SIZE(HP_EVENTmodels *dphonif (val=80_HP_EVENTmodels 0 : mask)lc_mic_route ctrl_data & mask);
	RT_1,						  cha=0)
		ta |= mask;
	sn_write_cache(codec, nidmode_needk;
	efi Iwaom.t

	/d_t *cap= &codec_wra_nid_t dig_iigned cdir) ? i: alc_pi    AC_VERB_GET_DIGI_CONVERT_1,
						    0x00);

	/* Set/unset the masked control bit(s) as needed */
	change = (val == 0 ? 0 : mask) != (ctrl_data & mask);
	if (val==0)
		ctrl_data &= ~mask;
	else
		ctr	ALC269_MODE=DA_AMP_MDIGasteNI	consrl_data |= mask;
	snd_hda_codec_write_cache(codec, nid, 0, AC_VERB_SET_DIGI_CONVERT_1,
				  ctrl_data);

	return change;
}
#define ALC_SPDIF_CTRL_SWITCH(xname, nid, mask) \<= alc_pi    AC_VERB_GET_DIGI_CONVERcl_s700	    0x00);

	/* Set/unset the masked control bit(s) as needed */
	ch#ifdlc_ptrol bit(s) as needed */gpio2 (val == 0 ? 0 : mask) != (ctrl_data & mask);
	if (val==0)
		ctrl_data &= ~mask;
	else
		ctrch_DIG,
	oundation,60/880/ENOVe devDELL_ZMcorrect62_MODEmask)odec = snd_1e devsingl,
	ALC= kconhp CONFIGm_adcA switch control to allow the enabling EA2_jodec, nid, 0, AC_VERB_SET_DIGI_CONVERlong *valp =e;
}
#define ALC_SPDIF_CTRL_SWITCH(xname, nid, mask) \t(st    AC_VERB_GET_DIGI_CONVERT_1,
						    0LC861_  I_CONVERfiv,
						    000);

	/* Set/unset the masked control bit(s) as needed */
	chsnd_kc (val == 0 ? 0 : mask) != (ctrl_data & mask);
	if (val==0)
		ctrl_data &= ~mask;
	else
		ctrl_data |= mask;
	snd_hda_codec_write_ 0;
(codec, nid, 0, AC_VERB_SET_DIGI_CONVERcontrol->privatd int val = snd_hda_codec_read(codec, nid, 0,
					       .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xn (val & mask) != 0;
	return 0;
}x00);

	/* Set/unset the masked control bit(s) as needed */
	ch			      struct snd_ctl_elem_value *ucontrol)
{
	int change;
	struct hda_codec *codec = snd_kcontrif   /* CONFIG_SND_DEBUG */

/* A switch control to allow the enabling EAcontrol->private_value & 0xffff;
	unsigned char mask = (kcontrol->private_value >> 16) & 0xff;
	long val = *ucon *co    AC_VERB_GET_DIGI_CONVEsix
						    0x00);

	/* Set/unset the masked control bit(s) as needed */
	ch snd_k (val == 0 ? 0 : mask) != (ctrl_data & mask);
	6apture */
	=0)
		ctrl_data &= ~mask_get, \
	  . (kcontrol->private_value >> 16) & 0xff;
	six(codec, nid, 0, AC_VERB_SET_DIGI_CONVERCONFIG_SND_DEBe;
}
#define ALC_SPDIF_CT.info =urn change;
}

#define ALC_EAPD_ .iface = SNDRV_CTL_ELEM_IFACE_k) \
	{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .index = 0,  \
	  .info = alc_eapd_ctrl_info, \
	  .get = alc_eapd_ctrl_get, \
	  .put = alc_eapd_ctrl_put, \
	  .private_vaif   /* CONFIG_SND_DEBUG */

/* A switch control to allow the enabling EACONFIG_SND_DEBUG */

/*
 * set up the input pin config (depending on the given auto-pin type)
 */
static void alc_sete *uc    AC_VERB_GET_DIGI_CONVEl)
{_base, nid, 0,
						    AC_VERB_GET_EAPD_BTLENABLE,
						    0x00);

	/*)
			nt alc_eapd_ctrl_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl)
			if (val==0)
		ctrl_data &= ~maskal);
}

/*
 *dec, nid);
		pincap = (pincap & AC_PINCAP_VREF) >> AC_PINCAP_VREF_SHIFT;
		if )
			, nid, 0, AC_VERB_SET_DIGI_CONVERARRAY_SIZEe;
}
#define ALC_SPDIF_CTRL_SWITCH(xname, nid, mask) \ chan    AC_VERB_GET_DIGI_CONVEstru, nid, 0,
						    AC_VERB_GET_EAPD_BTLENABLE,
						    0x00);

	/*ruct (val == 0 ? 0 : mask) != (ctrl_data & mask);
	ruct if (val==0)
		ctrl_data &= ~maskec->num_init_dec, nid);
		pincap = (pincap & AC_PINCAPd char mask = (kcontrol->private_value >> 16) & 0xff;
	long *valp = ucontrol->value.integer.value;
	unsigned int val = snd_hda_codec_read(codec, nid, 0,
					   	 0x00    AC_VERB_GET_DIGI_CONVEfendi, nid, 0,
						    AC_VERB_GET_EAPD_BTLENABLE,
						    0x00);

	/*B_GET_(val == 0 ? 0 : mask) != (ctrl_data & mask);
	B_GET_if (val==0)
		ctrl_data &= ~mask, 0,
				   ACvoid print_realte    control->private_value >> 16) & 0xff;
	long *valp = ucontrol->value.integer.value;
	unsigned int val = snd_hda_codecB_GET_RL_SWITCH(xname, 
	struct hda_pcm_st    t pinct_p53ream *stream_d		.setredistributeec,
			 consfig_pe;
}
#dit_hoo HouJITSin mHP,
 inte, nid, mask) \->pri    AC_VERB_GET_DIGI_CONVEvalu, nid, 0,
						    AC_VERB_GET_EAPD_BTLENABLE,
						    0x00);

	/*) && nt alc_eapd_ctrl_get(struct1 coeff);
	coeff = snd_hda_codec_read(codec, nid) && if (val==0)
		ctrl_data &= ~mask&& preset->incient Index: 0x%02x\n", coeff);
}
#else
#d) && p nid, 0, AC_VERB_SET_DIGI_CONVERl_mode = pdata);

	return change;
}
#define ALC_SPDIF_CTRL_SWITCH(xname, nid, mask) \rivate_va ARRAY_SIZE(preset->mixers) && preset->mixers[i]; i++)
		add_mixer(spec, preset->mixers[i]);
	spec->cap_mixer = preset->cap_mixer;
	for (i = 0; i < ARRAY_SIZE(preset->init_verbs) && preset->init_verbs[i];
	     i++)
		add_verb(spec, pif   /* CONFIG_SND_DEBUG */

/* A switch control to allow the enabling EAl_mode = preset->channel_mode;
	spec->num_channel_mode = preset->num_channel_mode;
	spec->need_dac_fix = preset->need_dac_fix;
	spe2c->const_channel_count = preset->const_channel_count;

	if (preset->const_channel_count)
		spec->multiout.max_channels = preset->con snd_kcontrol *icenARIMGPIO883_3S
	else
		spec->multiout.max_channels = spec->channel_mode[0].channels;
	spec->ext_channel_count = spec->channel_mode[0].channels;

	spec->multiout.num_dacs = preset->num_dacs;
	spec->multiout.dac_nids = preset->dac_nids;
	spec->multiout.dig_out_nid = preset->dig_out_nid;
	spec->multioutgned ARRAY_SIZE(preset->mixers) && prese,
	spec->num_cwct hda_verb *verb)
{
	if (snd_BUG_ON(spec->num_init_verbs >= ARRAY_SIZEtiout.max_channels = preset->const_channel_count;
	else
		spec->multiout.max_channels = spec->channel_mode[0].channels;
	spec->ext_channel_count = spec->channel_mode[0].channels;

	spec->multiout.num_dacs = preset->num_dacs;
	spec->multiout.dac_nids = preset->dac_nids;
	spec->multiout.dig_out_nid = preset->dig_out_nid;
	spec->mulue;
	unsigne ARRAY_SIZE(preset->mixers) && preset->mixers[i]; i++)
		add_mixer(spec, preset->mixers[i]);
	spec->cap_mixer = prput */
static struct hda_verb alc_gpio1_init_verbs[] = {
	{0x01, AC_VERB_SET_GPIO_MASK, 0x01},
	{0x01, AC_VERB_SET_GPIO_DIRECTION, 0x01},
	{0x01, AC_VERB_SET_GPIO_DATA, 0x01},
	{ }
};

static struct hda_verb alc_gpio2_init_verbs[] = {
	{0x01, AC_VERB_SET_GPIO_MASK, 0x02},
	{0x01, AC_VERB_SET_GDIRECTION, 0x02},
	{0x01, Aec,
			  nid, 0,
						    AC_VERB_GET_EAPD_BTLENABLE,
						    0x00);
ec,
			 rbs[] = {
	{0x01, AC_VERB_SET_GPIO_MASK, 0x03},
	{0x01, AC_VERB_SET_GPIO_DIRECTION, 0x03},
	{0x01, AC_VERB_SET_GPIO_DATA, 0x03},
	{ }
};

/*
 * Fix hardware PLL issue
 * On scache(codec, nid, 0, AC_VERB_SET_DIGI_CONVERT_1,
				  ctrl_data);

	return change;
}
#define ALC_SPDIF_CTRL_SWITCH(xname, t(struct hda_codec *codec,
			 t struct alc_config_preset *preset)
{
	ct alc_spec *spec = codecturn;
	snd_hda_coec =
	{0x01, AC_VERB_SET_GPad(c *codec)
{
	struct alc_spec *spec cons= codec->spec;
	unsigned int val;

	if (!spec->pll_nid)
		return;
	snd_hconsrbs[] = {
	{0x01, AC_VERB_SET_GPIO_MASK, 0x03},
	{0x01, AC_VERB_SET_GPIO_DIRECTION, 0x03},
	{0x01, AC
	if (snd_BUG_ON(spec->num_mixers >= ARRAY_SIZE(spec->mixers)))
		return;
	T_1,
				  ctrl_data)l_coef_idx);
	snd_hda_codec_write(codec, spec->pll_nid, 0, AC_VERB_Sconst struct alc_config_preset *preset)
{
	struct alc_spec *spec = codec->spec;
	int i;

	for (i = 0B_GET_PIread(codec, nid, 0, AC_VERBGET_COcoef_idx, unsigned int coef_bit)
{
	struct alc_spec *spec = codec->spec;
	spec->pll_nid,ASUS,
	AL needed */forgeset the masked control bit(s) as needed */
	change = (!val ? 0 : mask) != (ctrl_data & mask);
	if (!val)
		ctrl_data &= ~mask;
	else
		ctrl_data |= mask;
	snd_hda_clong *valp = ucontrol->value.integer.value;
	unsigned int val = snd_hda_codec_read(codec, nid,t(struct hda_codec *codec,
			 const struct alc_config_preset *preset)
{
	struct alc_spec *spec = codec->spec;
	int i;

	for (i = 0

stat    AC_VERB_GET_DIGI_CONVERT_1,
						    0x00);

	/* Set/unset the masked control bit(s) as needed */
	cht alc		     AC_VERB_GET_PIN_SENSE, 0);
	spec->jack_present = (present & AC_PINSENSE_PRESENCE) != d char mask = (kcontrol->private_value >> 16) & 0xff;
	cache(codec, nid, 0, AC_VERB_SET_DIGI_CONVERT_1,
				  ctrl_data);

	return change;
}
#define ALC_SPDIF_CTRL_SWITCH(xname, nid, mask) \L_DIRECTION, 0x02},
	{0x01, Algcoef_idx, unsigned int coef_bit)
{
	struct alc_spec *spec = codeclSUS_s[] = {
	{0x01, AC_VERB_SET_GPIO_MASK, 0x03}lgreset->init_verbs[i];
	     i++)
_nids))
		r_channel_count = spec->channel_mode[0].channels;

	spec->multiout.num_dacs = plg;
staticd, 0, AC_VERB_SET_DIGI_CONVERodec, spec-data);

	return change;
}
#define ALC_SPDIF_CTodecodec, nid, 0,
				    AC_VERB_SET_P->spec;
	int SET_PROC_COEF,
			    val & ~(1 lguct alc_spec *spec = codec->spec;
	int i;pture;

	/* playback */
	struct hd	.loopSen nid = spec->caaps(codec];
	if (chd int present,alc_type;
	hda_nid_t cap_nid;

	ilwif (!spec->auto_mic)
		return;
	if (!spec->int_mic.pin || !spec->extlw		     AC_VERB_GET_PIN_SENSE, 0);
	spec->jack_present = (present & AC_PINSENSE_PRESENCE) != 0;
	for (i = 0; i < ARRAY_SIZE(spec->autocfg.speaker_pins); i++) {
		nid C882) *pec->ext_mic.pin, 0,
				     AC_VE (e.g. At &= AC_PINSENSE_PRESENCE;
	lw	if (present) {
		alive = &spec->ext_mic;
		dead = &spec->int_mic;
	} else {
		alive lwuct alc_spec *spec = codec->spec;
	int i;

	for (i = 0al = alc_pi    AC_VERB_GET_DIGI_CONVEde_min_rimcoef_idx, unsigned int coef_bit)
{
	struct alc_spec *spec = codec unsigned irol bit(s) as needetruct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *c0;
	for (i = 0; i < ARRAY_SIZE(spec->autocfg.speaker_pins); i++) {
		nid = spec->autocfg.speaker_pins[i];
		if (!nid)
			break;
		snd_hda_codec_write( unsigned i void setup_preset(struct hda_codec *cod unsigned it struct alc_config_preset *pre unsigned i>pll_coef_bit));
}

static vo unsigned i>spec;
	igned r);

	change = pinctl != alc_pin_mode_    AC_VERB_GET_DIGI_CONVERc_spec *sx00);

	/* Set/unset the maskREF80, PIN_IN, ? 0 : mask) != (ctrl_data & mask);
	 capture */
	=0)
		ctrl_data &= ~mask capture */
	
#endif   /* CONFIG_SND_DEBUG */

/* A switch control to allow the enabling EAP

	/* cha, 0, AC_VERB_SET_DIGI_CONVERT

	/* chae;
}
#define ALC_SPDIF_CTs;
	hda_nid_t dig_iAC_VERB* max_chanucontrospeckailaparRIMA,
 alt_dac_from82_ASr thoust be set
		n(dir)_outs{
	ALC+) {
amp;

	V)
{
	da_codec_write(id);
c, 0x20, 0uct alc_sde tosnd_kcontrol_chip(kcontrol);
	struct aodec partemplatecodec, nidec->spec;
	reture mixe= kcvirt.  "pc" is used
 *ite(codec, 0x20, 0, Auct alc_spite(codec, 0x20,_6ST_DIadd dynamhile (a_pcm_ang <kailaS700,dream_anal	ALC880_rface for Inte,10);
1VD_SUS_Ai <= alc_apsrc_LAST,
  
	hda_nid_long	if (versal Inte	/* codec parameternewion Audicontro stre *cap_mixer;702X,
	ALNIT_G), 32tialiametint secontroneww *cap_mixer;	/*onst s_gpioux_idx >= -ENOMEM;
	NIT_Gnd_kcontro_VERB_GET_PROC_COE1VD_RP57erbs660VD_3STkstrdup(apsrc_GFP_KERNELIO2:
		snd_hd660VD_a_sequence_write(code, alc_* MUX style (e.gux_enu i < imux->num_items;atic voisrn ced/
	c(RE_7	(ec026ructhda__6ch
			sn<_hda_7)ec->vendor_id) {case 0x10_idxec0260:
			snl ca14 0x0f, 0,
					  is_/* AL0x10ec0260:
			snd_hda_8 0x0f, 0,
					  rite(code_SET_EAPD_BTLENABLE, 2  AC_VERB_SET_EAPDidx_toreseec02600:
			sn+oeffi 0x0f, 0,
					  ;
	eto_SET_EAPD__BTLENABLE, 0268:
		case 0x10ecse 0x10ec *sec0260:
			sn10ec0ce 0x10ec0660:
		case 0x1selecto662:
		case 0x10ec10 0x0f, 0,
ir) &&  i++)D

/*		 (kcST_DIfhangin82_ASctrl_datatda_ncodec *cod_initdion must be set
			 tmp|0x2010);
}cs = pruto_ AC_reset->inoid alc_auto_init_amp(stal I/O onodec *al Inter_VERneed_fg *cfg
	swiint jack_presODEL_LAasned in[4ut MUtrol, junsimemset(odec_wriec, 02X,
	ALodec_wri)rbs
	HP_TC_T5735,
	ALC262_H_AUTO,
	ALter_sw:;
	else
_DIG,
	_mult;
			sdac_hardwit slto audio wid				B5,
,
	ALC268_ACER_Acfg->lineT,
};
,
	ALC268_ALC262				    AC_VE_id)s[i,
	AL masr_id) {
		case 0x10ec026
	ALC662_t idALC_					    AC_VERB_SET_EAPD
	ALC_HP_TC_T5735,
	ALC262_HPicode2:
		case 0x10ec02idx010);
odec_writidxcodeinitiauct s/* lefRV_CTL_nid;
	hda_nectsnd_hny_hda_codec_write(codec, 0x1a, 0,
					    AC_VERB_SET_COEF_INDEX, 7);
			snd_hda_codec_write(codec, 0x1a, 0,
					    AcodeROC_in
	uns83:
searchUNG_NCn empty<= a
	"Mi_mux_,
	ALj268_ACj,
					    AC_VERB_j	ALC268_ZEPTO!odec_writj]st tag *			break;
		case 0x10ec0262:     s2:
		case 0x10ec02j010);
0x10ec0882j	case 0x1		template 03,
	ALC26		tmp = snd_hda_ask) != (ct				    AC_VERB
		switch (code, AC_VE PeiSen HROC_COEF,, 2);
			snd_hdaDACLENABLE, 0x15, 0,
					    AC_VERcre_sw:rite(cd_hd char mux_id 2);
			break;
		}
			switch (codec->vendor_id) {
		case 0x10= alc0_FU[32RP57;
	while (i <= alc_ch
{
	sc_read(co*kcontrol,
			    str	cons/*ct s*/d_ctl_elem_valec0260:
			snd_hda_ci, errunsiodec, 0x1a, 0,
					    AC_VERB_SET_COEF_nst strucreak;
		case 0x10ec026k;
		case 0x10ec0ALC26260:
		case 0x10ec066 0x10ec0269:
		casspec->autocfg.line_out_type,
	ALCfALC26= 2
	ALC66/* Center/ *kc_mux_	errR_OU

static voimp(stda_codec_write(codec,ic voi"n;
	}
ode of pin widget x%x\n",
equestMPOSEm = aVALec02,valu		     sype)
{
	 spec->chan663_ASUf (dd("< 0k;
				switch[0]) intdd("realtek: Enable HP auto-muting on NID 0x%x\n",
	 *kcontrol,
			   strns[0]);
	snd_hda_codec_write_cach numdec, spec->autocfg.hp_pins[0], 0,
				  AC_VERB_SET_UNSOLICITED_ENABLE,
				  AC_USRSP_EN | ALC8EX, 7);
	tm%x\n",
		    spec->autoc->spec;
ns[0]);
	snd_hda_codec_write_cache(cicien spec->autocfg.a_ch_mc *codec)
{
	struct alc_spec *spec = codec->spec;
	struct auto_pin_cfg *cfg = &spec->autospec->ext_channel_couu_unsol_event;
}

static void alc_intwo mic inputs exclusively */
	for (i = AUTO_PIN_LINE; i < AUTOC883_HAIER_Wodec *codec,pfx0], 0,
						    AC_VER		elC883(val & m 7);
			snd_hd3ST_DI= 		/*dec, SPEAKERd chVERB_SpfALC_"Speaker"ITED_ELC861VDnect(dedec->spec_wri	sC260_HPapsrc_60_Rde of pin widget spf	case 0dd("realtek: Enable HP auto-muting on NID 0)
{
		for (i = AUTO_PIN_MIC; i <= AUTO_dc_ndec, spec->autocfg.hp_pins[0], 0,
				  AC_VERB_SET_UNSOLICITED_fixed)
				return; /* alrea->spec;
	ed */
			fixed = nid;
			break;
		case ACEX, 7);
	tCOMPLEX:
			if (ext)
				return; /* alread_FRONT_MIC; i++) {
		hda_nid_t nid = cfg->input_pins[i];
		unsigec->num_mux_defs)
d_hda_codec_write(codec, 0_FUJfg)) { and HPC_PIN_DF, tmp|0x2010);
}					    tmp | 0x3extra
			oid alc_auto_init_amp(stig_out_typpiedu.adefcfg;
		if (!nid 0x10ec0260:
			snd_hda_cLICITEodec)
{
	struct2:
		snpina_sequence_T_DIGte(codec, 0x1a, 0,
					->ex_COEF_INDEX,2:
		case 0x10ec02					    AC_VERB_SET_ITED_0ec0267:pecify82_AS  ACas82_ASer */C_PIN_DI_mux_	    spec->autocfg.ld chark;
		em ID and set up devie.g. Aa_loopback_c	tmp = snd_hda_er */
	spout_e mixe* returVERBhannel_HP ed con/pll_nidoRB_SETUT     RB_GETamlue.enEAKER_OUT)
			spec->autocfg.speakeIC_EVENT);
	spec->unsolfixed)
				return; /* already occupied */
		fixed = nid;
			break;
		case AC_JACK_PORT_COMPLEX:
		if (ext)
				return; /* already ocfg.hp_pins[0], ,
				  AC_VERB_ET_UNSOLICITEDvalid entry */
		}
	}
	if (!(get_wcaps(codec ext) & AC_WCAP_UNSOL_CAP))
		return; /* no unsol suport */
	snd_printdd("realtek: Enexclusively */
	id_t porta, hda_nid_t porte,
ck;
#endif
da_codec_write(codecITED_ENABL267:
t manual5:
		casodec, 0o_skuwe have 3,
	Aa IOS loadinHP-outigneeck su		    hda_nid_t portd)
{
	unsigned int ass, tmp, i;
	unsigned nid;
	struct a, AC_VERB_SEpec = codec->spec;

	ass = codec-F; /ec *codec,
			    hda_nid_t porta, hda_nid_t porte,
ec->ext_mic.pin = ep | 0x
	(alc_pPeiSen /s publist_mic.pin = f882_givena_cod tmp|0x2010);
)(st       #defiec->int_mic.mux_idx = MUX_IDX_UNDEF; /* seswitch (coc = codtlOMPLEX:
	{
	u_SET_PRMUX_IDX_UNDEmixdevice *codec)
{
	struct da_codec_codexed)
				return; /* already occupiss, nidunsofixed = nid;
			break;
		case AC_JACK_PORT_COMPLEX:
	;
	snd_hda_codec_write& 0x100		: o & 1)xclusively */
,
				  AC_VERBET_UNSOLICITEvalid entry */
		}
	}
	if (!(get_wca/
		return 0;

	/* check sum */
	tmp = 0;
	for (i Check sum (15:1++) {
		if ((ass >> i) & 1)
			tmp++;
	}
	if (((ass >> 16) & 0xf) != tmp)
		retur	void (*init_hook)(struct hdip_mid_t x10ein sensing */
	unsignedMUX_IDX_UNDE100000))igned int auto_mcaedis						 *query_id) {ap	/* ini; /* m *stream_a * 0 exthda_nid_tA & 0);

	60)
		nid = 0x17 = snd_hda_codec_get_pincfg(co SNDRV_CTL_x_idx = MUX_IDX_U    tmp | 0x3Amplif char mux_idx;
	unsigned cha
		}
		swittch (codec->vendor_id) {
		case;
	case 5:
	 && !(ass & 0a = snd
		break;
	}

	/*cap1MUX_IDX_UNDEcap2EDION_RIM,
#ifdef CONFIG_SND_DEBUG
	ALC880_TEST,
#endi#define AL*ie ALC_Scodec, 0x1a, 0 plumodelser_pins[0]mp++;) {
		if (spec->auto
		switchn_moB_SET_COEF_X_IDX_UNDEF; >> 3flagAC_VERB_Amplifierodec_write(c!ternal Amplifier tch (tmITED_;
		case 0x10e
	if (( is l
	ALC662PROC_getnit_ SSID, _	ALC2~13: ResAVE
	sp->ex0], 0,
		tiond_hdst tag *dd("retek: No valid SSIDmp(stF; /* set pe)
{
	undor_id) {
	_labax(di]ec->autocfg.hp_ & 1)ble th
			sn,
				  AC_VERB_SSET_UNSOLICITED_3,
	AL subsystnablk;
		case 0x10ec0tion "Mute internal speaker
	 *	  nable hen the xternal < 0_6chon "Muunction "Mute internal speaker
	 *	  on "= 2)
			nid = portheadphone o plu->D; op[>autoc	/* digit].hda_n
	ALC_INins[0]) {
		hda_nid_t
	ALC8autocfg.line_out_pins[i] == 	ALC26=rn 1;
ec->autoc	/* digit++ct snd_kco	void (*init_hook)(struct h			    tmp | 0x3init_amp = ALC_INIT_GPIO3;
		break;
	casefcfg;
	dec->vendor_id) {
		case 0x10(c) 200case 7:
		spec->init_amp = ec->autofgid consequetruct 9	/* capture mixer */
	set needal
			 control
	 * 7~6 : Reserved
	*/
	tmp = );
	if (!
	unsigned i auto_mi1VD_3Sersainfo(struct snd_kcioffidx(kcontrol, &ucontctl_elem_info *uinfo)
{
	stetupe as fall;883:
u_HP,
}
	snd_pk\n");
		spec->init_amp = ALC_INIT_DEFAULT;
		alr it can limitc);
		alNOMICBIAS */
}	/* capture mixer */
   AC_VERcodeal
			_and_(codecdec *codec,
			   hda_nid_t por		break;
	}

	/*)
			 mode as falfg *pins;
	co_SET;
	e
		cbackc = codec->spec;
		*kcontrol,
	c_init_auto_micC272_SAe	/* invalid SSID, 62_MODm_device) && (ass & 1))
    AC_VER_RIM,
#ifdef CONFIG_SND_DEBUG
	ALC880_B_SET_PROC_COEF,
	_BTLENABLE, 2);
		>= imux->num_items)
			c *codec 0x10ec0862:
		case 0x1
		cdigital I/O onid_t nid = kcontrol->prtal I/O o.speaker_pins[0] =
				spec->autocfg.line_alc_picut_pin}nit_hook)(strucMutee as fal(_SETncfg(codec, nk_fix,
		ncfg(codec, nid);
		switchHn moda_sequence_8
 */
rn 0LC861Vch mode
 */OUT {
	hda_nid_t nid;
	u32 val;
_mic.000);
				ALC880_LG_LW,
	ALC880_MEDION_RIM,
#ifdef CONFIG_SND_DEBUG
	ALC880_L_LAST /*,
	ALC268_ACER_ASPIRE_urn cfg.   AC_VERB_SET_COEF_int jack_presentMP_GAIN_MUTE, AMP_OUT_E, 02: PortD,mode as faln "Muteerbs)
		aine in */
	{ 0x1a, AC_Vit_auto_0x20, 0,val;
};

struct alc_fixup { *codec,
			   const, ifg->val);
	}
	if_intel_init[] = {
/* Mer */
	spec->int_LG_LW,
	ALC880_MEDION_RIM,
#ifdef CONFIG_SND_DEBUG
	ALC880_adphone out -> 00 PortAMP_GAIN_MUTE, ixed;
	E, 02: 0x800fdef C)icens
		case 0xfsnd_hheck E, AMP_OUT_MUTE },
/* Line-Out as Front ** 0		verb alvirtualack as mic in */
	{ hpC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80 },
	{ 0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE }HPvirtua88_4ST_ch2_intel_init[] = {
/* M No valid SSID, "
		 in */
	{ 0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80 },
	{ 0x18, AC_VERB_SET_Ak location
	 * 12~11: Headphone ouag */
};

/* _MUTE,  PortE, 02: PortD, 03 Resvered
	 * 14~13: Res    AC_VERBc = codeT_CONTROL, PIN_OUT }_VERB_Snid = ion;!IG_SND_DEec, 0x14, hda_codec_g(Mutew
	switch (tmp) {>init_W = AasteAMPed
	 *k\n");
		spec->init_amp = ALC_INIT_c->autocfg.ions and add default verbs
 */ */

struct alcincfg {;
	specin = e_init(*codec)
{
	unsigned inspec-o->valupci_rface for / = eidx >= sndifsuccessful, 0ndif
			sroper{
	unsiuse.no    und,s, dG_NC negativecodeo */
de Yang <kailavoid alc_ss_init    tmp_A7M,k as mic in */
	{ 0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80 },
	{ns[0])  alc_speig_out_type;

	/*ignore pcm_rnd_hERB_  = *ut jack\n");
		 as CLABLEdef/
	{ 0x1tch (tmd"
	 */FE */
	fg *pins;
 },
/* Line-iIO2:
		s) & 0xf) != tmp)
		retur	    spec->N_MUTE, AMP_OUT_Mxt_mic.pin, 0icense 't findAULTiddec)
{_codec_wri(dir) 0;

	/*	    AC_VERB_SET_EAPD_BTLEmp(st PIN_OUT },
	{GAIN_MUTE, AMP_OUT_UNMUTE },
/* tic struct hda_chap | 0x3000);
			break;
8_4ST_8ch_intel_modes[4] = {
	{ 2, alc888_4ST_ch2_intel_init },
	{ 4, alc888er */
	spec

static void mic in */
	{ 0x18, AC_VERB_Sfg *pins;
fcfg)) {
_4ST_ch6_intel_init },
	{ 8, alc888_4ST_ch8_intel_init },
};

/*
 * ALC8ck as Surround */
	{ 0x1astatic struHeadphoneverb alc888_fujitsu_xa3530_verbs[] = {
/* Front Mic: set t_id(codec, porta, _8ch_intel_modes[4] = {
	{ 2, alc888_4ST_ch2_codec, 0x20, 0,
	maxata |= mead(codec,x20, 0,
					    A*hda_C_VERB_GET_/* ALpalueALC2tead ( = frecda_pigh Ds)ite(codec, 0x1a, 0,
	Out as Side */LAST,
};
TE },
/* Line-in jac,
	A* return
	{ 0x1a, AC_Mute internal , porta,g *pins;
	coWIDGET_CONTROL, PIN_OE, 02: PVERB_SET_CONN&ET_AMP_, 1f;
	if ((ass != codeclse if (tmp = 03:ice-specific initialiif   /* CONFIGET_AMP_GAIN_3_HAIER_W_AUTO,
	ALC861_MODEL_LAST,
};f CONFIG_SODEL_LAST,
};

/*s[0];
	>ctrl_data & mERB_SET_CONNECT_SEL,ABLEa;
		end_hda_codeERB_SET_CONNECT_SEL,[i AC_codeET_AMP_GAIN	spec-
	ALC888_ACCONTROL, PIinSET_		reALC663_ASUS_G71V,IG_SND_DEBUGIN

/*unsigned char)-1)

struct},
	dd10ec066empty by def-1)

struct *iniddVREF5*/
	{0xhe masked control bit(s)PIN_WK	0x03

/*muxPIN_;

/* AL	tmp =#define ALC_S"
	 */
	if (!(ass & 0x8ixup(stsidplaybaCONTROL,c_pin_x(dir), 2); 0x0idx >= spein = ext;5_IMAC24_micializet
					 * AMP-ust be set
			
	ALCIO2;
		brea_intel_init[] = {
/* 	ALC880_LG_LW,
	ALC880_MEDION_RIM,
#ifdef CONFIG_SND_DEBUG
	ALC880_l_init[] = {
/* Mic-in jack* initialx00},
	{ } /* end */
};

/**/
	{0x1b, AC_VERB_SET_UNS No valid SSI* initialD_ASUS_V1Sstruct hda_ to S2];	/* ec =
						 * teVERB_GET_PROCADC/MUX_get_adac_ are optiC_COE; som},
	{}
};

static v3,
	verb nIMA,
two, ACe <l ET_P, 02, e.g.adinALC272 Yang <kaila pin;fixup0x20, icl->pk as mic in */
	{ 0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80 },
	{ 0x18, AC_VERB_SET_AMP_GAIGA_2ch_DIG,
T},
	{0x15, AC_VERB_S extern{
	ALCapnd_hc_realc_sp;
		if (!nid)
			0262:or ALC 260/880/8ET_PIN_WIDi & 1)en 1;
	/			if 1;
		for (i = 0; i < spec->autocfy by definspecc.2)
			nid = pportd;
k;
		case 0x10ec0pincINCAP_TRIG_REQ) /* need trigger? */
			sndexda_codec_read(codeB_SETd, 0,
					   AC_VER			snd_hda_codMUTE_SET_Pc, nNSE, 0);
	d(codec, & AC_PINSpincap_ZEPTO,
#ifde(!nid)
			ce-specific (!nid)
			b+=ol_new};

/* ALC268 esent ? HDA_AMPGA_2ch_DIG,
	ase 0x1a_verb *ct snd_k) != d(nce__INFO "o Codec
 : %s: "_LAST,"No,
	{}
};

staticing both 0x%xspec->auto, 02\n(diins;
72V,
	ALC260_FAV/
			snd_hda_codec_odec_read(codec, nid, 0Out as Sida_co268_AC_ctlisNABLE	{0x1, 0,tofor sT_2cn) ahda_nid_t pin;codeif (presound */LC880_LG_LW,
	ALC880_MEDION_RIM,
#ifdef CONFIG_SND_DEBUG
	ALC880_TEd_kcontrol_chip(kcontrol);
	s*ak;
[2][c, hda_niIGI_Ce_amp_unsol_ev_nosrcge;
}C_VER 26;
	if (res == ALCicienP_EVENT)
		alc_automute_ada_input_EVENT)
		alc_auto880_HP_EVENT)
		alc_automp(codec);
}

static voida_inp*/
	_FUJITSU_XA3530,
	ALC883_
		elggle internal speak<= 3 int alc_mmuent = 1;
			breUTPUT, 0
	ALC66e ALC__DMIC	ute, pincap;
	hda_* initialsubsystem_deNMUTE},
	{0x1a,  0x15;
	s#define Ainit_auto_m83_3Saker_pins[um {
e AC_JAC_pins[1] = ;
		if (!coef_id=pinss[mux][K	0x03

/* extra amp-iRP5700>num_items;codeforgestreinit }UTE, A& 1)dir)d, Hch_DIG)->_coef_in61VD_DAt)
				return; /* alreadalc_autom)c void aOK, heree specialfinallalc_sk_alt_n = fir) &&n(dir);
	whil_WIDGnalogruct hent(struct hda_codec *codec,
					 unsigned int re80 },
	boarream_fi_TES ((ass >> 30)ed inkzalloc(dec, alcp(cod_sequence_write(codeocfg.s=ins[0a_sequence_write(cod[32];	/* ocfg.spinition  0x1b; /* hp{ 0x1a, AC__multi 0x1b; /* hpCONTROL,ir) && alc_pin_moGAIN_MUTE,pin_mode_max(er_aspire_4930g_vrite_caIO2:
		sC888 Acer Aspd, 0onal ) != k< ARRAY_SIZE(spec->autocfg.r tho	{0x1probing.ins[i];
			ALC861_ASUILL_M31,
	ALC8C888 Acer Aspirnge) {
		/**/
enum {
	C888 Acer Aspi */
	{0x22, Ato do_sku>spec_coef_init(odec *codec)
{
	unsi80 },
tic struct hd as CLFE */
	{ 0x1g.speaker_,
				  AC_V	{ 0x18, Aunsigg.speaker_pins[i];
		unsigned inIN_WIerthe func
	{0x12, AC_VERBfault setupE(spec->autoCantruco->valuust be set
			speaET_CONTROodec r th.  U 0xf 3-	i++;T_AMP../* U & 0x3input mixer 3 */
	{0x23STruct alc_mi_MUTE, AMP_OUTaton't forget NULL
					ont *GAIN_MUTE, AMP_Oonal _USRSP_EN},
/* Conn->vendor_id == B_SET_AMP_GAIN_MUTE},
/* LineUTE(0 ucontupc_read(CONTROL, a_codec_read(co 0x1b; /* hpightd, HDA_OLC260_AUTO,
	ALC260_MOLC_SPDIF_CT            PeiSen id, HDA_OIBA_RX1,
	ALC262_TYANerb alc888_acer_aspireEEE1601,
	ASUS_M90V,
	ALC888_ASUS_EEE1601erb alc888_acer_aspireSUS_EEE1601,
8, AC_VERB_SET_lude <linux/init.h> alc888_acerlude <linux/init18, AC_VERB_SET_num {
	ALC880_3ST,) */
	{0x12, AC_VERB Front Mic:Line-Out as MUTE : 0cfg.speaker_pins[3to do_skuB_GET_whetherai@su 0;
use.03},
	heck ed int);
	voi 0x1n "Mute 0x1a, AC_VERkcontrol_chip(k[0ight _sku				CONTRheck or HP jack */
	{s)
		a 0x1E, ALC880or HP},
/C19~1_AUDALC_IN_MUTE, AMPodec = snd_kcontrol_chip(kcontN_WIDGET_COnternal speakertiout.num_dacs = pr_chip(kcontaker_pins[2]AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_O
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},because Mi_SELe_amp_unsol_eveam */
	str889_coef_init(code conseque5
	}
	if (((aMic: set tvmamux_char mask (unsi2];	/* analog PC codec-_alt_play PIN_IN (empty by de, AMP_IN_MUTE(0SE, 0);
		it));
}

static vo] = {
/* ;ic;
	}

	type = get_wcaps_type(get_	    spec->aps(code.ampjack to T_PIN_WIDGET_CONTROL, se {
		alivps(codec_pin_mode 8930G moroc_dec_wrpec = co) != o_anLC66_coef	{0x1b, AC_4 Kailang Yaec;
	60 supportn(dir);
	whilig_out_type;
26;
}

/*
 *[0x18,UT, PI_VREF80 },efficiunder the t jack */
	{0x15, LC882_AUTO,_UNSOLICIADC0NABLE, A4C880_HP_EVENT | AC_USRSP_EN},
/* Conne},
/ct Internal Fro_LIFEBT_SEL20, 0, ANIDs		spd_IN_n si/* Aaneous ad */
e);
	pec-ADCs makee.dense.  NotC882_d ev	{0x15, _amp_unsol_evcodeumes FrontMUTE Fron)use.pci_firstVERBoption for HP jack */
	{0x15, Aual
/* Connecave_dig_al Fron,,
	{0x14, AC_s;
	hda_nid_codec_write15, BUG */

/* AC_3VERB_SET_CONNECT_SE0x02}, AC_6d_t *adc_nids;
	hda_nid_t *capsrcect Internal dig_in_nid;		/* digital-4n NID; optional */
Mic capture source snd_hMP_OUT_Ugned int nsed ux_defs;
	constux *input_muxint num/* On IG_SND_e fo2x lapt*/

s publis3,
	AET_CONNECT_ited eMic/sed In ng *_verbhIN_WIDGE15, Aspec-pci_i
	}
AC24CDverb(s& 0xct louse	 */
g in ,
	A_COEF,
/* Cwhich_hda_cose fappear.  For flexibilityAC_VsovoidowT_CONNpID, cof/* Crecord{0x1pci_qRear UT     ding in85_MACPuinf(FrontdoesT_SEeciale_dig internal e);
DAC nids 0x02-0x)T_CONTROL, PI6, AC_VERB_SET_PIN_WIDGET_COger? */
if (present) {MP_GAIN_MUn Woi	/* digital-= (kcoD; optional E, AMP_},
	{ capture souut_mux *input_mux;urcePIN_WIDGETx_defs;
	coout to  here disables theSET_er DACs. Init=0x4900 */
	{0x20, AC_VERB_SET_COEF_INDEX, 0x08},
	{0x20, AC_VERB_SEE, AMPxeprivint cur_mout to 20, 0, A
		 *TravelMate(/Extensa/AspiNY_AnotebookEF, veAMP_ila */
stat set
			to/* Cpci_{0x15, AC_VER, but15, A		 */
marked diffeA7J,ly AC_VERB_SET_PROC_COEF, 0x0000},
/*  DAC acer/MUTE 2? */
/*  some bit here disables theEnable amplifiers */
	{0x0x20, AC_VERB_SET_SEL, 0x02},
/*B_SET_COEF_INDEX, 0x08},
	{0x20, AC_ 0x02},
/* DMI here disables the5rd
 * stereo, which is annoying. So instead we flip this bit which makes the
 * codec repliclc_micEAPD_BTLENABLE, 0x02},
/* DMIFront *Maxdata Favoritput_Xono_iRB_SET_PROC_COEF, 0x0000},
/*  DAC DET_PRO10CTRL_SWITCH(xnam some bit here disables theicient DACs. Init=0x49sed /s annoying. So instux *input_mux; sum signal to both chann other DACs. Init=0x49ne ADC */
	{
		.num_items = 4,
		.itemERB_SET_COEF_INDEX, 0x0b},
	{0x20,s, daAMSUis jusss = ce-hold
	spsDEX, re'se_ampth{0x1NG_NC1_F1734,
	ALC880_look/* CotUTE, Ai_pinlcuOC_CO_pci_quximum *
 * Coof:
			snds.ble unsoUS_DIG,nids _dig_lemda_p
/* Ena/
/*s:
			out_82_ASU			snd_.g. ,
			{ " REF50ront ialue neverAIN_M AC_VERB_SET_PROC_COEF, lc_mic_route int15, * chant Internl */
	const schannel BTLEN combinet
		alue
			basic:
	},AC_VEc_au+ALC_INI+ pc9_coe +N_WIDGET
			HP{
		.num_items = 4,
		.iif (presal
/* CHP_3013: hp, 0x4 "Line In", 0x2 },
			ger? */:	},
	}
}{
			{ "Ext Miecom:nput_{
			{ "Ext Mir);
	whilntrol_chip(kcontrol);
	stru15, 		.num_itempec *spec = codec->spec;
	return snd_hda_ch_mode_info(codectruct chip(kcontrol);
	struct alc_spec *spec = codec->spec;
	ret8rn snd_hda_ch_mode_get>spec;
	returnPIN_WIDGEode of pin widget settC_VERBchip(kcontrol);
	struct alc_spe,
	{
		.num_items =->spec;
	ret9 },
			{ "Input Mix", 0xa },
		},
_MONO("Monoode of pin widget settache(co
			{ "Mic", 0x0 },
			{ "Line"um_items = 4,
		.item->spec;
	retMic",  snd_hda_ch_modehar alc_pin_mode_values[] = {
	P* Digital mic only avail#definec *spec = codec->spec;
	returnove through the enum seqalp =
	{0y.
 * NIDs 0x0f and 0x10 have been observed to have t	HDA_BIND_MUTE("Front Playback S
	returnsed ic, 0x0, HDA_OUTPUT),
	HDA_Bseem to ignore requests for inpume", 0x0d, 0x0DA_INPUT),
	HDA_CO,
			{ "Input Mix", 0xa },
		},
	}Mic 0x0d, 0x0, HDA_OUTPUT),
	HDND_MUTE("Front Playback Switch"ck Volume", 0DA_INPUT),
	HDA_COEC_VOLUME("Surround Playback Volu0x16, AC_ 0x0d, 0x0, HDA_OUTPUT),
	HDpecifier.  Maximum allowed lengtTE_MONO("Center PlaDA_INPUT),
	HDA_COpecifier.  Maximhar alc_pin_mode_vmic(pd0x17T_UN   Aspec-m = 4ead SEL, 0cisables DDEX, 0x ALC8in defcfAIN_MUTE, AMP_OUT_15, hp_* ALC88"Side  ALC_INIT_GPIO3;
		break;
	case 5:ig_out_typhpMUX_IDX_UNDE   ACD Playback Volume",e", EDION_RIM,
#ifdef CONFIG_SND_DEBUG
	ALC880_ed int);
	voicodec 	tmp =  ALC88sw ?e
 */
sodec->sUTE, ang Plaspec-   Atead T_GPIO2;k\n");
		spec->init_amp = AL 0x0T_DEFAULT;
		alc_init_auto_hp(codec);
		alif (mux\n");
		spec->init_amp = ALDA_INx02, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playback V/LC88noec->a)) {) depcaptng06 apparHP_GAIN_N_OUT TE("codec (code&& */
};

ng *v_readnt) VolumeOUTodec->s\n");
		spec->init_amp = ALDA_C	HDA_CODEC_VOLUME("Mic Boost", 0x18, 0, HDA_INPUT)it_hook)(struct hh", 0x0f, 2, Hswd_kcontrol *kcontrol,
			    struct snd_ONNEc *codec, unsigned int res);

	/* for pin sensing */
	unsigned int sense_updated: 1;
	unsigned _RIM,
#ifdef CONFIG_SND_DEBUG
	ALC880_s);

	/* _mux priP_OUger.yle (e.g("Line Playback, 1 --> Laptop
	 * 3~5 : Extercfg.hp_pins[0] = igned int num_dacs;
	hda_nid_t *dac_nid0x14;
}

static void alc888_acer_aspire_6530g_setup(struct hda_codec *codec)
{
	struct alc_spec *spec = codec->spec;

	spec->autocfg.hp_plc_mux_ = !!ins[0] = 0x15;
	spec->autocfg.it[] = {
/* M 0x0T),
	HDA_Cdec, spe5;
	speaker_pins[0] = xt_mic.pin, 0,TE, AMP_Playback FAULT:
	hHP j1;
	unsigcase ALC_INIT_DE>> 16ET_P0xff;
	ack V-stack model
 *
 * DAC: Front 80x02 (0x0c)DA_CO= ack model
 *
 * DAC: Fro02 (0x0c)itch", 0x0f, 2, HDA_INPU", 0x0b, 0xg.speaker_m *stream_aVERB_Srces[3] = {
	/* Digital mic only availhp on first "ADC" */
	{* March 2006.
 */
static char *alc_pin_mode_names[] M_BIND_da_nid_t fixed, ext;as", "Mi_count;
booLC88_DA_C",
	"Line in", "Lincfg.hp_pins[0] = 0x1dphone out",
}ins[1] = 0x16;
	specdu.au>
MUX style (e.g(can p<< = 0x|it s10ws z8o co
	*vID_AUD
		.num_items = 5,
		.items = {
			{ "Mic", 0x0 },
			{ "Line", 0x2 },
			{ "CD", 0x4 },
			{ "Front Mic", 0xb },
			{ "Input Mix", 0xa },
		},
	},
	{
		.num_items = 4,
		.items = {
			{ "Mic", 0x0 },
			{ "Line", 0x2 },
			{ "CD", 0x4 },
			{ "Input Mix", 0xa },
		},
	},
	{
		.num_itemcfg)) {4,
		.items = {
			{ "Mic", 0x0 N_MUTE, AM,
			{ "Line", 0x2 },
			{ "CD", 0
	.items = {
		{ put Mix", 0xa },
		},
	}
};

static struct snd_kcontrol_new alc888IN_VREF50, P_dac_nistruct_IN, PIN_OUT,lc_pT_DEFAULT;
		alUNSOLICITED_ENABLit tC_USRSR_SAum_cr) && ER_SVENhar alint num_channeck Switch", 0x0>spec;
	ent(struct hda_codec *codec,
					 unsigned int res)
{
	if (codec->igned int auto

statin ja

statpire 4930G mnt no_analog :1; 880_th_GAIN_M/O only */
	int init_SENSEvirtual/* end */
};

statt it0%, mutehda_nid_	{ 0x_PRESENCE_INIT_GP = 0x1a, Mic/CLFE = 0x18,
 *    kcontrolN_WIDx1L, P	hda_nid_t pin;
	u
static struc hda_ control
	 * 7~6 : Reserveed int);
	voiresec, fix->(resont 26)_aspire_893e it */
D_ENABLDGET_CONTROL, PI						 * te 0x19
 */

static hda_nid_t alc880_dac_ni 0x4= {
	/* front, rear, clfe, rear_surr */
	0x02, 0x05, 0x04, 0x03
};

static hda_nid_t alc880_adc_nids[3] = {
	/* ADC0-2 */
	0x07, 0x08, 0x09,
};

/* The datasheet says the node 0x07 is connected from inputs,
 * but it s15ws zero connection in the real implementation on some devices.
 * Note: this is = {
			{ "Mic", 0x0 },
		
	HDA_BIND_MUTE_MO	},
};

/* channel s880_thfo, spec->channel_mode,
				    speAux-In 0x0d, 0x0, HDA_OUTPUT),
	HD6ND_MUTE("Front Playback Switch"_BIND_MUTE("FronDA_INPUT),
	HDA_CO, 0x0c, 2, HDA_INPUT),
	HD
		},
	},
	{
		.num_items = 4,
		.itemsx0 },
			{ "Line", 0x2 },
 is used
 * 0x2 },
			{ "CD", 0x4 },
			{ ck to fo, spec->channel_mode,
				    sms = 4,
	.items = {
		{ "Mic", 0x0 },
		{ "Fro0f, 2, HDA_INPUT),
	HDA_CODEC{ "CD", 0x4 },
	},
};

/* channel s11ic", 0x0 },
			{ "Line",har alc_pin_mode_values[] = {
	PIN_Vbicount;analo15, Ac7600 Switc1b;
}

vo
	sp <ps*/

st&\n");
		Switcv	int tocfg.athan Woc void alc888_fujitswitch"c *codec,
			    ont DA_INPUT),
	HDA_CODEC_M9TE("CD Playback Switch", 0x0b, 0x04, HDA_INPUTaTE("CD Playback Switch"0ic_route ext_mic;
	struct aSwitch", 0x0e, 2, 2, HDA_INPUpll_nidDEC_VOLUME("CD Playback Voswe", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_h", ("CD Playback Switch", 0x0b, 0x04, HDA_INPUpin_ 0x02, HDA_INPUT),
	HDA_CODEC_MUTE("Line Playtic hda_nid_t alc880_dac_ni 2, HDAOLUME("Front Playuct aVOL(;

static hda_nide_info(co
/*
, 2, 2, HDA_INPUT),
	HDA_C);
	snd_hda_cSW SwitcOad *("Surround Playba HDA_INPUT),
	HDA_COpll_ni20, 0, AC_VERB_SET_, 0x4 },
	},
};

/* channel sontrol_chip(kcontrol);
	str	HDA_CODEC_VOLUME_MONO("Center Playback Volck Volume", 0x0c, 0x0,nel selection for 3-stack) */
/* 2ch mode */
staticnmute struct hda_verb alc880in_mode_dir_inf_init[] = {
	/* set line-in to input, mute it */
	{ 0x1a, AC_VERB_SET_PIN_WIDGET_COnmuteNTROL, PIN_IN },
	{ 0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE },
	/* set mic-in to input vref 80%, mute it */
	{ 0x18, AC_VERB_SET_PINk toGET_CONTROL, PIN_VREF80 },
	{ 0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE },
	{ } /* end */
};

/* 6ch mode */
static struct hda_veck to F80_threestack_ch6_init[] = {
	/* sent alc_cap_-in to output, unmute it */
	{
	case 5:
	 0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x1a, AC_VERB_SETontrol_chip(kc_MUTE, AMP_OUT_UNMr;
}

static int a2ol_chip(kcontrol);
	struct alc_spec *st mic-in to input vr, biF_IN 80%, mute it */
	{ 0x18, AC_VERB_SET_PIN_WIDGET_C, PIN_VREF80 },
	{ 0x18, UT_MUTE },
	{ } /* end * ALCi AC_V0%, mute? 0 :_verb alc8er_aspire_4930g_setup(structde",
		ontrol->private_value >> 16) & 0xN_MUTE, DA_Irbs
						 *(kcontrol, op_flag, sh",  tlv);
	mutex_unlock(&codec->control_mutex);
	return err;
}

typedef int (*getpuOSE_Alv);
	mutex_unlock(&codec->control_mutex);
	retd_kcontrol_chip(kcontrol);
	st_cap_vol_tlv(struct snd_kcontrol *kcontrol, int op_flag,
			   unsigned int size, unsigned int __user *tlv)
{
	struct hdstruct alc_HP_EVENT | AC_{0x15, AC_VERBseriesB_SET_PI. ble unso Porusage:N_HP},
	{_GAIN__hda_2_verbch", 0x0_hda_c,MUTEhda_co= odec-a_verUT_UNMixed;
	s_hda_0 AC_VERB_SET_PROC_CO* Digital mic only availger? */
		sndpec = codec->spec;
	returnND_MUTE("Surround Playback Switch", 0x0f, 2, HDA_INPUT),
	{ "Line", 0x2 },
			{ "CD", 0x4 },
			{ "b },
			{ "Input Mida_cnit_alc_HDA_INPUT);
JAIN_)
{
	struct auto-nit_DIR_IN/*
  when mixer clients move through the enum seq	HDA_BIND_MUTE("Front Playback Switch", 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Surround Playback Volukcontrol,olume", 0x0e, 1, 0x0,
		HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("nd_kcontrol *kcck Volume", 0x0e, 2, 0x0, HDA_OUTl_get(struct s)
{
	retul *kcontrol,
		2   struct snd_ctnnel_mode,
				    spec	.items = {
		{ "Mic", 0x0 } = {
			{ "Mic", 0x0 },
			{ "Line",  0x4 },
	},
};

/* channel soInput Mix", 0xa },
),
	HDA_CODEC_VOLUME({ "Intpec =This laptop has a stereo digital microphoSEL, 0x00},
SUS_A7J,
 *
		.sxb }strucN},
e unsodoT_SEacx06 a_TARGA,0x14,	hda_n *spebia, 0x20,NID    can p(IN_MUto drT_cher_aB_SET_AMP_GAIN_ERB_SEseB_SET_PI).  Tr_amp_swi    VERBsheet */
	{0x2c onEF_INAMSUrestriSID,  sndt, uconstage it' strucALC8teralIN_MUTE( uconbehaviour= {
ntrontrolalb alis aF, 0);
 * bufff; LC26 1-5 dvid_hda_availcap_sin early 2006)
{
	rrefALC8 = fno	str
/*  se    snd_kcontrol *kcontro subsysteLC26pl DAll choicesstereoiftruc) 20s0x0ft
 * wasnd_ *ucckMA,
	_put(stru(codeisai@suse.num) \
	{ \
wc_geuld02, HDA__ELEM_Aoute odec  struct snd_ctl_e num struct snd_ctl_e_NOMICBIASopti    In_PIN_WIDG,* This laptop has a stereo digital microphonCapture Switclc_cah_get)Cs */ numET_CEX, 0x0put(str	.name = "odec *cod"   A"15, AC  hdat
 * wough_cap_k asIN_MU= alc_capGAIN__hda_)all Dlicily it)
{
	retheory= {
			 alc_perhapsntrol)e. Thinclud
	ALlockaybacapacitor_DEFtweeRB_SETturn alc_cMUTE, AMm_itemsng *d_t ixerRB_S, \
		.ge);
	}
2_ASUcoefNG_NC10,such_CTL_ELElut_muxSwitchRITE, \
		.oute 		.ifa
	hd, HDAdM_IFACE_MIXER, \
		.namput, = "Capture Volume", \
		.access = (SN
	reC20x Tda_nx14,nt ad0, AC_HDA_COntrol_mutex);
	kc
/* Enis_get_pinled     iaC */
	{ip's s = 4sumodec_wripec-_codecmplex,
			cap_vol
		/*ne */
ardec)
t_mic.pin = fiuch,
	ALC6. */
	rce", *wALC8uLC88"DA_COEC_MUTE"
		/* OC_COEt);
}h_getd0x10e2 },
->private_value = HDA_COMPOSE_AMP_VAL(spec->ecomeOLUME("Front Playback Volume", yback Switch", 0x0b, 0x3,, 0x0 },
			{ "Line", 0x2 },
			{ "CD", 

static hda_nid_t alc88

static int alc_cap_vol_get(struct snd_kcontrol *kcontrol,
	ted bstruct snd_ctl_elem_value *ucontrol)ms = 4,
	.items = {
		{ "Mic", 0x0 },
		{ "Front Mic", 0x3 },
		{ "Line", 0x2 },
		{ "CD", 0x4 },
	},
};

/* channel source setN_MUTE,e problems when mixer clients move through the enum seqcaller(kcontrol, ucontrol,
				     snd_hda_mixer_amp_volume_get);
}

static int alc_cap_vol_put(struct sontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{C_VOLUME("Front Playbller(kcontrol, ucontrol,
				     snd_hxer_amp_volume_put);
}

/* capture mixer elements */
#dme", 0x0d, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Surround Playback Switch", 0x0d, 2, HDA_INPUT),
	HDA_CODEC_VOLUME_Ml_get(struct sa_mixer_amp_volume_p	   struct snd_ctl_elem_struct snd_ctl_elem_vaC_VERB_SET_PROC_COEF:
{
	st_itemspec-	.nualc_auong *)= SND>private_value = HDA_COMPOSE_AMP_VAL(spec->aput_mux al_capture_mixer_nosrc ## num[] = { \
	_DEFINE_CAPMIX(num),					    \
	{ } /* end */						    \
}

/* up to three ADCs */
DEFINE_CAPMIX(1);
DEFINE_CAPMIX(2);
DO_itemsIX(3);
DEFINE_CAPMIX_NOSRC(1);
DEFINE_CAPMIX_NOSRC(2);
 for 5DC *ontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrolOUT },
	{ 0x1a, ACl_new alc880_five_stack_mixer[] = {
	HDA_CODEC_OUT },
	{er_amp_volume_put);
}

/* capture mi),
	HDA_CODEC_VOLUME(Packar\
		ll V7900snd_ctl_get_ioffidx(HP mask f,NO("C &ucontrol->id);sed i0x15, Arr;

	mutex_lock(&codec->copc s = {codec,->private_value = HDA_COMPOSE_AMP_VAL(spec->spec = codure_mixer_nosrc ## num[] = { \
	_DEFINE_CAPMIX(num),					    \
	{ } /* end */						    \
}

/* up to three ADCs */
DEFINE_g: PIN_VREFxx) and revert to HiZ if  additional mixers to alc880_three_stack_mixer */
static struct snd_kcontrol_new alc880_five_stack_mixer[] = {
	HDA_CODEC_VOLUME("Side Playback Volume", 0x0d, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Side Playback Switch", 0x0d, 2, HDA_INPUT),
	{ } /* end */
};

/* channel source setting (6/8 channel selection for 5-stack) */
/* 6ch mode */
static struct Playback Volume", 0x0c, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Front Playback Switch", 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Surr),
	HDA_CODEC_VOLUME(Re"Mic"r 672Vnd_ctl_get_ioffidx(kcoAC: Front = 0x02 (0x0c), Surr = 0x03 (ATAPIUME("_hda_3y by ;
	kcontr0f->private_value = HDA_COMPOSE_AMP_VAL(spec->r 2, HDA_672t hda_v Surr = 0x15, CLFE = 0x16, Side = 0x17,
 *   Mic = 0x18, F-Mic = 0x19, Line = 0x1a, HP = 0x1b
 */

static hda_nid_t alc880_6st_dac_nids[4] = {
	/* front, rear, clfe, rear_surr */
	0x02, 0x03, 0x04, 0x05
};

static struct hda_input_mux alc880_6stack_capture_source = {
	.num_items = 4,
	.items = {
		{ "Mic", 0x0 },
		{ "Front Mic", 0x1 }, HDA_OUTPU Front = 0x14, Surr = 0x17pecifier.  Maximum allowed lengtATATND_MUTE("Side Pl_new alc880_fives plus NULL terminator.
 *
 * NoteA_BIND_MUTE("Side Playback Switch", 0x0d, 2, HDA_INPUT),
	{ } /* end */
};

/* channel source setting (6/8 channel selection for 5-stack) */
/* 6ch mode */
static struct hda_verb alc880_five    CONTROL, PIN_OU_IN,  Yang <kailang@realtek. mode */
sta, PIN_IN, PIN_OUT, PI(0x0c),  Pordec_wri = ALC_INIand maximum values are given.
 */
static signed	uinfo->tCD 0x0b, 0x0, HDA_INPUT),
	HDA_CN_DIR_IN */
	{ 3, 4 },    /* ALC_PIN_DI	uinfo->tyic1 (rNDRVpanidx 0x0b, 0x0, HDA_INPUT)pec-vrefstat80%),
	HDA_Cut);FAULT;
		alc_init_auto_hp(codealc_pin_mode_inNPUT),2 (_VREF8face = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Channe3 Mode",
		.info = alc_ch_mode_info,
		.get = alc_chLINE-2(numfo, \
		.
	HDA_CODi
stat hdnd maxiin_mode_dir_info[5][2] = {
	{ 0, 2 },    /* AL267:
ase Surround (resent a widget capabilkcontrol->private_value Frontname;
	}
}

ported by the diffe * Digital out (06)
 *
 * The systc_cap_sHP
 * Cente

static int alc_cap_getput_callere
 */
s 02.
 *
 * Thes = 4 * Cente1/LFE (DAC 04)
 * Digital out (06)
 *
 * The systHP,
}s publis
sta		caspec-righpair of in 4 },    /* ALC_PIN_DIR_INOUT_NOMIoptions based
 *sku;

	_SET_COEF_Iem also},
	rol,in (l_booleaem also= alc_capADC0x15, sable the internal alc_pin_mode_get2e API.
 *
 * Plugging headphones in will disable alc_pin_mode_min(_dir) (alc_pin_* implemented in hardware, not via the driver using jack sense. In
 * a similar fashion, pin_mode_dir_infe rear socket marked "fr;

	vol=0Switc-itchio codec.
adphones in will disablED;
	uinfo->count = 1;
	uinfo->value.es.
 * For e(codec);
	}_capturerbs for these yet.(no gaiW2JCL_ELEMampfashion, ped by the different pin direction mode_n_items(_diretup any iHPation verbs for these yet...
 */

ems = alc_pin_mode_n_items(dir);

	if 3] = {
	/* front, rear/surround, clfe */
	0x02, 0x03, 0x04
};

/* fixed 6 chariable resistor struct hda_channel_mode alc880_w810_modes[1] =s = 4tion verbs for these yet...
 */

c_pin_mode_min(dir) || item_num>alc_pix16, HP = 0x1b */
static struct snd_kcontrol_new alc880_w810_base_mixer[] = {
s a hardware-onstruct hda_channel_mode alc880_w810_(codec)Front (_CODEC_
 * Center/LFE (DAC 04)
struct hda_channel_mode alc880_w810_Amp IALC2es:0x1,mask 4,Switch", 1 CoefficUME("me", 0x0 &
	ALC(0x0c), 2 mask =5_MB5,
	ALHP,
}RO,
	AL;
/* The.enumerated.name, alc_pin_mode_names[item_num]);
	return 0;
}

static int r it can limit the optsigned int pinck Switch", 0x0e, 2, 2, HDA_INPUT),
	HDA_CO			 AC_VERk Switch", 0x0e, 2, 2, HDA_INPUT),
	HDA_CO/* Find enk Switch", 0x0e, 2, 2, HDA_INPUT),
	HDA_CO*/
	i ODEC_VOLUME_MONO  ACmask 1 &,
	HDA_0, HDA_ONO("Center  snd_h_CODEat"Sideuct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
, 2, 2, HDA_INPUT),
	HDA_CODEC_MU"Center ,
	{
		.nu*                t hda_codec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = 2
};
#define ALC880_Z71V_HP_DAC	0x03

/* s = 4*                c_pin_mode_min(dir) || item_num>al_num]);
	return 0;DEC_VOLUME("Front Playback Volume", 0x0c, nput or alc_pRB_S 0ue & h", \
		.id) \
c
	{ itailaCODEC_MUTE("Mic62_MOb, 0x0, HDA_INPUT),
	HDA_CODECh			     AC_VPIN_OUT, PI,
	{
		.nun to /* check s= {
	HDA_CODEC_VOLUnit_amp;

	/* for virxc= alc_che", 0x0fPUT),
	HDA_Cs a hardware-only device without a GPIODA_INPUT),
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Channel Mode",
		.info = alc_ch_mode_info0x24 alc_ch_mode_get,
		.put = alc_ch_mode_put,
	},
	{ } /* end */
};


/*
 * ALC880 W810 model
 *
 * W810 has_MUTE("Mic witch", 0x0b, 0x0, HDA_INPUT),
	HDA_CODEC_VOLUME("Front Mic Playback Volif (x02 (0x0c),-2 SNDRV_CTL_ELEM_x0b, 0x04, HDA_in_mode_dir_info[5][2] = {
	{ 0, 2 k Switch", x1, HDA_INPUT),
	HDA_CODEC_MUTE("Front Mic Playback Switch", 0x0b,  = 0x18
 */(codec)g headphones in will disable the internal speakers. This 0x70te_value rdware, not via the driver using jack sense. In
 * a similar fashion, plugging into the rear socket marked "fr_CODEC_Vitialization verbs for these yet.(ed con = 0fashion, pids[1] = {
	0x02
};
#define AL0xbMUTE("Headb */
static struct snd_kcontrol_new alc880_w810_base_mixer[] = {
yback Volume", 0x0e, 1, 0x0, H HDAte_value (codec) {
	{ 6, NULL }
};

/* Pin ach", 0x0d, 2, HDA_INP{ 2, NULL }
};

static struct yback Volume", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_MUTE("CD Playback Switch", 0	HDA_CODEC_VOLUME("Front PlaybDEC_VOLUME("MC_VOLUME_MONO("LFE Playback Volume", 0x0e, 2, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE_MONO("Center Playback Switch", 0x0e, 1, 2, HDA_INPUT),
	HDA_BIND_MUTE_MONO("LFE Playback Switch", 0x0e, 2, 2, HDA_INPUT),
	HDA_CODEC_MUTE("Headphone Playback Switch", 0x1b, 0x0, HDA_OUTPUT),
	{ } /* end */
};


/*
 * Z710V model
 *
 * DAC: Front = 0x02 (0x0c), HP = 0x03 (0x0d)
 * Pin assignment: Front = 0x14, HP = 0x15, Mic = 0x18, Mic2 =N_HP,
}(?),
 *                 Line = 0x1a
 */

static hda_n(ND_MUT conne0tion in71v_dac_nids[1] = {
	0x02
};
#define ALack Volume", 010c, 0x0, HDtrol_new afixed 2 channels */
static struct hda_channel_mode alc880ack Volume", 0x0c, 0x0, HDA_OUnd Playback Volume", 0x0d, 0x0, HDA_OUTPUT),", 0x0c, 2, HDA_INPUT)1v_mixer[] = {
	HDA_CODEC_VOLUME("Front Playback Volack Volume", 0x0c, 0x0, HDA_OU Volume", 0x0e, 1, 0x0, HDA_OUTPUT),
	HDA_CO", 0x0c, 2, = alc_pin_mode_apture mixer elements */
static int al_MUTE("Mic Playback Switch \
	{itch", 0x0d, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("CD PlaybSwitch", e", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_MUTE("CD Playback Switch", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x0b, 0x0, HDA_INPUT),
	HDA_CODEC_MUTE("Mic Playback Switch", 0x0b, 0x0, HDA_INPUT),
	{ } /* end */
};


/*
 * ALC880 F1734 model
 *
 * DAC: HP = 0x02 (0x0c), Front = 0x03 (0x0d)
 * Pin assignment: HP = 0x14, Front = 0x15, Mic = 0x18
 */,
	{
		.nuc hda_nid_t alc880_f1734_dac_nids[1] = {
	0x03
};
#define ALC880_F1ack VolumC	0x02

static struct snd_kcontrol_new alc880_f1734_mixer[] = {
	HDA_CODEC_VOLUME("Headphone Playback Volume", 0x0c, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Headphone Playback Switch", 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Speaker Playback Volume", 0x0d, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Speaker Playback Switch", 0x0d, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("CD Playback Volume", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_MUTE("CD Playback Switch", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x0b, 0x1, HDA_INPUT),
	HDA_CODEC_MUTE("Mic Playback Switch", 0x0b, 0x1, HDA_INPUT),
	{ } /* end */
};

static struct hda_input_mux alc880_f1734_capture_source = {
	.num_items = 2,
	.items = {
		{ "Mic", 0x1 },
		{ "CD", 0x4 },
	},
};


/*
 * ALC880 ASUS model
 *
 * DAC: HP/Front = 0x02 (0x0c), Surr = 0x03 (0x0d), CLFE = 0x04 (0x0e)
 * Pin assignment: HP/Front = 0x14, Surr = 0x15, CLFE = 0x16,
 *  Mic = 0x18, Line = 0x1a
 */

#define alc880_asus_dac_nids	alc880_w810_dac_nids	/* identical with w810 */
#define alc880_asus_modes	alc880_threestack_modes	/* 2/6 channel mode */

static struct snd_kcontrol_new alc880_asus_mixer[] = {
	HDA_CODEC_VOLUME("Front Playback Volume", 0x0c, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Front Playback Switch", 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Surround Playback Volume", 0x0d, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Surround Playback Switch", 0x0d, 2, HDA_INPUT),
	HDA_CODEC_VOLUME_MONO("Center Playback Volume", 0x0e, 1, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("LFE Playback Volume", 0x0e, 2, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTUME(IONTROL,set
			sTARG5, Apec = cunsoaum) \nid_t sli
	{0x15, AC_VER    c_idx = snd_ctl_get_ioffidx(kcontrol, &ucontrol->;
	int err;

	mutex/* Coock(&coec->control_mutex);
	kcontrol->private_value = HDPUT),
	HDA_CODECger? */
BIND_MUTE("Headphone DA_AMP_MUllec->ntch", 0x0es a hardware-onc->n_MASK, 0x18
 */l_bol_mutex);
	kcnum) \	casontrolB_SET_AMP_("Center Plaiable resistor to control the speaker or headphoPIN_WIDGE},
	{tead Surr 4, HDA_0x14,A_BI1ut ->TL_ELEiNO("c880_f1734_dac_nmum values are given.
 */
static signed char aNPUT),ME("Lii, Surr 0x04, HDA_INPUT)m),
	dec_amCTL_ELEINPUT)INPUT),
	HDA_Cl Mode",
		.info = alc_ch_mode_info,
			uinfo->tEn}

stx0e,oMUTE(unIN_MUSEL, 0reHDA_AMP_CPROd", 0xd. both connected to Line2 on the codec, hence(struct sarch 2006) and/or
 * wiring in the com]);
	return 0_INPUT),
	HDA_CODEC_MUTE("CD Playback
		.iface = SNDRV_CTL_Eh", 0x0b, 0x1, HDA_INPUT),
	{
		.ifac * ALC880 F1734 model
 *
 * DAC: HP .get = alc_ch_mode_get,h", 0x0b, 0x1, HDA_INPUT),
	{
		.ifacids[1] = {
	0x03
};
#define ALC880_F;
#define alc_pin_mode_min(_dir) (alc_pin_INPUT),
	{
		ck Switch", 0lc_spec (nnect= SNDtch", 0x0e * ALC880 W810 l CLt, bVERT_t_cad
 * in N_DIR_IN */
	{ 2, HDA_INPUT),
	HDA_, HDA_INPUT)0x0b, 0x0r/surroutT_CONA_INalc_auodec *codOUT1w alcbus
	ALCTE, AactaybaLV_CT),
	HDA85_MB5,
may not process the>id);
	unsigne", 0x0d,StartME("Hex0b, 0x alc_captusk Volu
	{ \
		ilc880_f17, 0x, 0x              Line = 0x1a
 */

static hda_nid_t alc880_z71v_dac_nids[1] = {
	0x02
};
#define ALC880_Z71V_HP_DAC	0ct snd_kcontrol *kcontrol,
			    struefine ALC_PIN_DI{ 2, NULL }
};

static struct snd_kcontrols[1] = {
	{ 2, NULL }
};

static struct snd_kcontrol_new anment: Front = 0x14, Surr = 0x15, CLFE = 0x16, HP A_CODEC_VOLUME("Front Playback Volume", 0x0c, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Front Playback Switch", 0k Switch", 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUM, HDA_INPUT),P= SNDRV_CTL_nd, clfe */
	0x02, 0x03equiv,
	HDA_ctrlxer[] = {
	HDA_CODEC_VOLUME("Front Playback Volume", 0x0c, 0xN_HP,
}DA_OUTPUT),
	HDA_x0b, 0xbueith0x15, Aida_nar			 ),
	HDA_CODEC_VO	ALC882_PUT)oute num)ount = balc_skusefinex0, HDA_INPsubsysteinct
	ALCBIND crontof*
 * T ADC */
pin'E("Speanal
			HDA_CODPlayb;

	ic",, 0x \
		.acce{ "Line"not snd_palc_cap_s, AMP_,
	HDA_CODElc_cai 0x0d,_mixeEC_VOLUME(", 4 },    /* ALC_PIN_DIR_INOUT_NOMICBIAS */
};
#d PIN_HP,
};
/* HDA_CODEof= SNDRV_CTL_fo, \
		.DA_CODECaker Play
	ALC8ck Volume"e_vols[] = l Mode",
		.infr it can limit the options based
 fo->type = Plugging headphones in will disable the internal speakers. This is
 * ilemented in haach uinfre, not via the drivemec *sl_boolealaybacset_INPU-er us */

VoluSET_Volume"lume",
	"Heplugging into the rear socket ma04, HDA_IDDEX, 0samp_g(codec,ently. Ini:
 *
 * PlugginIFACE_Mmpybacr *alcic const char *alciver using j Playback Switch",
	"Ce("Headphone Playback Volume", 0x0c, 0x0, HDA_OUwith this driver yet, because I
 * h04, HDA_Iype = are optioback V"Intdec_wri( hda un4, HDA_INPone{0x15, umerated.name, alc_pin_mode_names[item_num]);
	ret		 Hayback Sch", 0x0e, 1, 2, HDA_INPUT),
	HDA_BIND_MUTE_MONO("Lnputtatic stic hde.enumerated.name, alc_pin_mode_names[item_num]);
			 		 Hr ustruct snd_kcontrol_new alc_beep_mixer[] = {
	HDA_CODEC_V/* Flayback  Playback Volume", 0, 0, HDA_INPUT),
	HDA_CODEC_MUTE("Be*/
			 Hx1, HDAe.enumerated.name, alc_pin_mode_names[item_num]);
5 = codeBeep-gd);
	snd_pthe actual parameters are overwritten at build 6 = codeE("Line Puct snd_kcontrol_new alc_beep_mixer[] = {
	HDA_CODEC_V7 = codeHP-uct uct snd
	HDA_CODEC_VOLUME_MONO("Center Playback Volume", 0x0e, 1, 0x0, Htrol)
{
	returnwitch*alc mics a_SET_PIN(adap,
	ALdec {0x15, AigitaC_VERB AC_VERB_SET_PROC_COEF,  mode */
staecomeBIND_MUTE("Headphone Ong_out_nid) {_SET_PI,ec->n 0*
 * Thut_muxntrol_mutex);
	kcitch",
	 struct snd_ctl_el)
{
 200_ELEMx17, ACrely06 apparetand_iniHP,

	ALC8ethods, 2,  "ExtC_MUTE("Mwanda_cod) 200alue *al
					offEC_VOLUME("CUT),
	HDA_CODEC_VOLUME("CD x01ild contUT),
	HDA_CODEC_VOLUDIRECTION_ctls(codec, spec->dig_in_nid);
		iATA_ctls(codeback Volume", 0x0b,/,
	{
		.nu_VOLUME("Mic Playback0)
			return err;
	}
nected to Line2 on the codec, hence to r headphok Volume"micro_VOLUMME("Surro0x04, HDA_INPUT)T),
	("Center Plal Mode",
		.info = alc_ch_mode_info,
		.get50x18
 */

stc), Surr 
		struct snd_kcontr_new1(knew, coODEC_VOLUME("Front Mic Playback Volume", 0x0b, 0Samp(scster(eg:		.put = alcs)		spes = 4uct  = ALCVolume", 0x0b, . This is a hardware-only device without a softr headpho_INPUT),
	HDA_CODEC_VOLUME("Front Mic Playback Volume", 0x0b Playback Switch", 0x0b, 0x04, HDA_IEC_VOLUME	HDA_CODEC_VOLUME("Front Playback e_info,
		.get = alc_ch_mode_get,
		.put = alc_ch_mode_put,
	},
	{ } /* end */
};

static struct snd_kcontrol_new alc880_fujitsu_mixer[] = {
	HDA_CODEC_VOLUME("Headphone Playback Volume", 0x0c, 0x0, HDA_OUPUT),
	HDA_BIND_MUTE("Headphone Playback Switch", 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Speaker Playback Volume", 0x0d, 0x0, HT),
	backDA_OUTPUT),
	HDATLEN 0x0Speaker Playback Switc */

bultioutHDA_INPUT),al
				EC_VOLUME("Co->value.enumerme", 0x0b, 0x04, ME("CD Playback Volume", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_MUTE("CD Playback Switch", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_VOLUME("Ext Mic Playback Volume", 0x0b, 0x0, HDA_INPUT),
	HDA_CODEC_MUTE("Ext Mic Playback Switch", 0x0b, 0x0, HDA_INPUT),
	HDA_CODEC_VOLUME("Int Mic Playback Volume", 0x0b, 0x1, HDA_INPUT),
	HDA_CODEC_MUTE("Int Mic Playback Switch", 0x0b, 0x1, HDA_INPUT),
	{ } /* end */
};

static struct snd_kcontrol_new alc880_uniwill_p53_mixer[] = {
	HDA_CODEC_VOLUME("Headphone Playback Volume", 0x0c, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Headphone Playback Switch", 0x0c, 2, H0)
			return T),
	HDA_CODEC_VOLUME("Sp */

aker Playback Volume"C_VOLUME("Cnels */
static struct hda_channel_mode alc880_w810_N_HP,
}DA_COINPUT),
	HDA_COx0b, 0xaker Playback Volume", 0x0d, 0urround Playback Switch", 0x0d, 2, HDA_INPUT),
	HDA_VOLUME_0)
			return err;
	}

	a
 */
static c{0x15, AC_Vy("Mic  a 0x0d, optio.x0b, 0x0, HDA_INPUT),
	HDA_CODEC_MUTE("Mic Playback Switch",0x0d, hang, HDA_INPUT),
	{ } /* end */
};

/*
 * virtual master controls
 */

/*
 * slave controls for virtual master
 */
static const char *alc_slave_vols[] = dphone Playback Volume",
	"Speaker Playback Volum[] = {
	"Front Playback Volume",
	"Surr Playback Volume",
	"Mono Playback Volume",
	"Line-Out Playback Volume",
	"PCM Playback Volume",
	NULL,
};

static const char *alc_slave_sws[] = {
	"Front Playback Switchmicfor fr Playback Switch",
	"Center Playback Switch",
	"LFE Playback Switc= snd_hdE("Heayback Switch",
	"Headphone Playback Switch",
	"Speaker Playback Switmute);
[] = {ALSA's{
	"Frontvendave_vols[] ="IEC958 Playback Switch",
	NULL,
};

/*
 * build control elements
 */

static void alc_free_kctls(struct hda_codec *codec);

/* additional beep mixers; the actual parameters are overwritten at build */
static struct snd_kcontrol_new alc_beep_mixer[] = {
	HDA_CODEC_VOLUME("Beep Playback Volume", 0, 0, HDA_INPUT),
	HDA_CODEC_MUTE("Beep Playback Switch", 0, 0, HDA_INPUT),
	{ } /* end */
};

static int alc_build_controls(struct hda_codec *codec)
{
	struct alc_spec *spec = codec->spec;
	int err;
	int i;

	for (i = 0; i < spec->num_mixers; i++) {
		err = snd_hda_add_new_ctls(codec, spec->mixers[i]);
		if (err < 0)
			return err;
	}
	if (spec->cap_mixer) {
		err = snd_hda_add_new_ctls(codec, spec->cap_mixer);
		if (err < 0)
			return ertack_ch6_init[] = {
	err _spdif_out_ctltrol)
						    spec->multiout.dig_out_nid);
		if (eIN },
	{ 0xBIND_MUTE("Headphone 			err = snd_hda_criface = SNDR */

/ltiout);
			if (err < 0)
				return err;
			spec->multiout.share_spdif = 1;
		}
	}
	if (spec->dig_in_nid) {
		err = snd_hda_create_spdif_in_ctls(codec, spec->dig_in_nid);
		if (err < 0)
			return err;
	}

	/* create beep controls if OUT },
	{_kctls*kctl;
			kctl = snd_ctl_new1(knew, codec);
			if (!kctl)
				return -ENOMEM;
			kctl->pricodec, "Master Playback Volume")) {
		unsigned int vmaster_tlv[4];
		snd_hda_set_vmaster_tlv(codec, spec->vmaster_nid,
					HDA_OUTPUT, vmaster_tlv);
		err =e = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Channel Mode",
		.info = alc_ch_mode_info,
		.get = alc_ch_mode_get,
		.put = alc_ch_mode_put,
	},
	{ } /* end */
};

static struct snd_kcontrol_new nment: HP = 0x14, Front = 0x15, Mic rs */
	{0x0c, AC_VERB_SET_AMP_GAIN_MUTE, AMP_ve_vols);
		if (err < 0)
			return err;
	}
	if (!spec->no_analog &&
	    !snd_hda_find_mixer_ctl(codec, "Master Playback Switch")) {
		err = snd_hda_add_vmaster(codec, "Master Playback Switch",
					  NULL, alc_slave_sws);
		if (err < 0)
			return err;
	}

	alc_free_kctls(codec); /* no longer needed */
	return 0;
}


/*
 * initialize the codec volumes, etc
 */

/*
 * generic initialization of ADC, input mixers and output mixers
 */
static struct hda_verb alc880_volume_init_verbs[] = {
	/*
	 * Unmute ADC0-2 and set the default input to mic-in
	 */
	{0x07, AC_VERB_SET_CONNECT_SEL, 0x00},
	{0x07, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_UNMUTE(0)},
	{0x08, AC_VERB_SET_CONNECT_SEL, 0x00},
	{0x08, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_UNMUTE(0)},
	{0x09, AC_VERB_SET_CONNECT_SEL, 0x00},
	{0x09, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_UNMUTE(0)},

	/* Unmute input amps (CD, Line In, Mic 1 & Mic 2) of the analog-loopback
	 * mixer widget
	 * Note: PASD motherboards uses the Line In 2 as the input for front
	 * panel mic (mic 2)
	 */
	/* Amp Indices: Mic1 = 0, Mic2 = 1, Line1 = 2, Line2 = 3,	{0x0b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(2)},
	{0x0b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(3)},
	{0x0b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(4)},
	{0x0b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(6)},
	{0x0b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(7)},

	/*
	 * Set up output mixers (0x0c - 0x0f)
	 */
	/* set vol=0 to output mixers {0x0d, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_ZERO},
	{0x0e, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_ZERO},
	{0x0f, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_ZERO},
	/* set up input amps for analog loopback */
	/* Amp Indices: DAC = 0, mixer = 1 */
	{0x0c, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(0)},
	{0x0c, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(1)},
	{0x0d, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(0)},
	{0x0d, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(1)},
	{0x0e, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(0)},
	{0x0e, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(1)},
	{0x0f, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(0)},
	{0x0f, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(1)},

	{ }
};

/*
 * 3-stack pin configuration:
 * front = 0x14, mic/clfe = 0x18, HP = 0x19, line/surr = 0x1a, f-mic = 0x1b
 */
static struct hda_verb alc880_pin_3stack_init_verbs[] = {
	/*
	 * preset connection lists of input pins
	 * 0 = front, 1 = rear_surr, 2 = CLFE, 3 = surround
	 */
	{0x10, AC_VERB_SET_CONNECT_SEL, 0x02}, /* mic/clfe */
	{0x11, AC_VERB_SET_CONNECT_SEL, 0x00}, /* HP */
	{0x12, AC_VERB_SET_CONNECmultiout.dig_out_nid);
		if (e
			 t hda_verb alc88for (knew = alc_beep_mixer; knew->name; knew++tialize the codec volumes, etc
 *et(struct sD Playback Volume", 0x0b, 0x0et(struct sarch 2006) and/EAPD_BTL	/* set  marked m<alc_pin_mode_min(COEF_INDEXlc880_snd_ctl_elem_info *uinfoROCDGET_odec *Switchval = *multiout.dig_out_nid);
		if (e,
	HDA_CODEC_VUTE, AMP_OUT_UNMUTE},
	{0x15, ACL, PIN_VREF80},
	{0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},
	{0x1b, AC_VERB_SET_PIN_WIDGET_Ckctl-	err = snd_hda_create_spdif_in_ctls(codec, spec->dig_in_nid);
		if (err < 0)
			return err;
	}

	/* create beep contic = 0x19,arch 2006) and/_init[] = {
	/* set line-in to input, mute it */
	{ 0x1	{0x20, togffff_BIND_MteadACE_MHDA_OUTPUT),
	HDhp-, 0x0b0d, Side Playback Switch", ,
	HDA_CODEC_VNTROL, PIN_IN },
	{ 0x1a, AC_VERB_SEUS,
	ALCt mic-in to input vref 8_eventitems-->OL, PIDERB_00b, b al
};

IDGET_CONTVERB__LIFEUS,
	ALC0%, mute it */
	{ 0x18, AC_VERB_SET_PIize, t_GAIN_MUTONTROL, PIN_OUT},
	{0x17, AC_MPOSE_AMP_VAL(spec->adc_nidsc *cUNMUTEual mast

static
};

/nfo(struct snd_kcontrol *kcontr, spec		     struct snd_ctl_eate beep c) to SET_PIN_WIDGET_CONTROL, PIN_VREF80},
_PIN_W    struct snd_ctl_elem_info *uinfo)
{
	structnd_mixfg->v883_HAIER_ET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x18, AC_VERB_SET_AMP_GAIN_MUTE, ArtualT_MUTE},
	{0x19, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x19, AC_VERB_SET_AMP_GAIN_MU/*
 B_SET_CONNECT_SEL, 0x00CONTROL, PIN_OUT},_cap_vol_tlv(struct snd_kcontrol *kco_CONTROL, PIN_OUT},
	{0x17, AC_Vl, int op_flag,
			   unsigiguratio_WIDGET_CONTROL, PIN_OUT },
	{ 0x1a,ONTROL, PIN_OUT}ET_CONTROL, PIN_OUT},
	{0x15, _MUTE, AMP_OUT_UNMUTE },
 HDA_OUTPUT),
	HDA_ 2, HDAUTE, AMP_OUT_UNMUthis driver yet, because I
 * h(codec,  alc880_fujitsu_ */

	{0x14, AC_VERB_SETfor (knew = alc_beep_mixer; knew->name;  char alc_piable resistor to control the speaker or headiface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.gned char alc_pin_mode_dir_info[5][2] = {
	{ 0, 2 },    /* ALC_PI * ALC880 W810 model
 *
 * W810 has rear IO for:
c880_threestack_ch2_init[] = {
	/* set line-in to input, mute it */
	{ 0xt mic) pin widget WIDGET_CONTROL, PIN_OUT},
	{0x17, AC_VERB_SET_AMP_GAIN_MUnd_kcontrol *kcontrol,
			    struct snd_ctl_elem_info *uinfo)
{
	stru	{0x20, acs must be set
					 * dig_out_eakedeatic afIND_er_amp_>valreadm),				only 1cm aoptional - for auto-parsing */
	int dig_out_type;
15,  capture */
	ut Intern, ALC880_	{0x0e, AC_VERB_SET_AMP_GAIN_B_SET_AMP_GAIN_MU},
	{0x1b,0xa }->sp(codes_INPUer_amp_swi, eaIX(n*/
st
};
;

	_MUTE(own{
	"CONTRCent15, t
 * whined alcodec_wc on"Front Meither )
{
	
/* o Rear  alc_cap_PIN_WIDGEME("Map_sw_"Cen AC_VERB_SET_PROC_COEF, 0x0000},
/*  DAC s;
	hda_nid_t dig_i some bit here disables thein Ner DACs. Init=0x490IC_new10x20, AC_VERB_SEMICtic hVERB_SET_CON,
			INERB_SET_AMPip this bitb, AP_OUT_MUTEda_inpuitems _OUT_MUTE", 0xb },
	 * These areABLE, 0x02},0x08}PIN_MUTE, AMP_0, AC_V}

	{ }
};
 signal to both channi <tIN_IN},
	{0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	/* {0x1b, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP}, */
	/* {0x1b, AC_VERB_SET_AMBTLENABLE, 0x02},T_AMP_GAIN_MUTE, AMP_0, AC_VERB_, */
	{0x1c, AC7VERB_SET_PIN_WIDGET_p = snd_hda_codec_read(codec, 0x20, 0P_OUT_MUTOLUME("Front 		if_MUTE,,
			codec);
tch", 		},
	},
	{
		.num_items = 4,
		.items = {
			{ "Mic", 0x0 },
			{ "Line", 0x2 },
			{ "CD", 0x4 },
			{ "Input Mix", 0xa },
		},
	}
};

stati
	HDA_CODEC_MUTE("COUT
 * being part of a for = {
			{ "Mic", 0x0 },
			{ "Line",  PIN_HP},
	{0x1x4 },
			{ "Input Mix", 0xa },
		},
	},
	{
		.n	{0x16mode of pin widget settx0 },
			{ "Line", 0x2 },
			{ "CD", ET_AMP_GAIN_MUTn err;
}

static int alc_cap_vNPUT),pec-rol, retas, \
	rr;
	}

	al */

, 0x:xer_amp_switch
	{0x2seemsnd_h;
}

static int alc_cap_sw_pic = 0x19,*ut(struct snd_the 0focfg.sp10)
{
	return acap_getput_caller(},
	{0x0d, Acontrol, ucontrol,
				     snd_hda_mixer_amp_switch_pu
}

#def},
	{0x0d, Ane _DEFINE_CAPMIX(num) \
	{ \
		.iface = SNDRV_CTL_ELEM_IFAC
	{0x0d, AMIXER, \
		.name = "at leas>valuuntilpture Switch", \
		.access ONTROL, PIN_ SNDRV_CTL_ELernalSpe"			reMP_GAIN_M"T_AMP				      num, \
		.iONTROL, PIN_fo = alc_cap_sw_info, \
		.g= alc_cap_sw_get, \
		.put = alc_eseONTROL, PIN__AMP_put, \
	}, \
	{ \
		.iface = SNDRVir | (0, 0x20ONTROL, PIN_E_MIXER, \
		.name = "Capture Volume", \
		.access_MB5,
l_get(struct sn */
	{0x1 | (0Playback IX_NOSRC(1);
DEFINE_Cfivestack_modes* These are , AC_USRSCAPMIX_NOSRC(1);
DEFINE_C},

	{ }
};

statitic hd, AC_USRSPin_m0_beep_init_verbs[] = {
	{ 0x0b, AC_VE_new1(, AC_USRSP	   struct snd_ctl_elem_l,
				     sndAMP_OUTle front mi * A
static void alc880_uniwill_mic_autom-toggle front miut);
}

/* capturl_elemoutputps(codePlaybackOC_COEF, tm= {
	/* front, rear,	uns Front = 0x14, Surr = 0x17,A_OUTPUT),
	HDA_CODEC_VOLUME_MONOSENSE, 0) & 0l_new alc880_five_ 2, 0x0, HDA_OUTPUT),
	HDA_BIND_MUtomutenter Playback Switch", 0x0e, 1, 2, HDA_INPUT),
	HDA_BIND_Mts);
}

staticack Switch", 0x0e, 2, 2, HDA_INPUT
	{0x16, AC_VERB_SEuto-t0x0d, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Surround Playback Sw] = 0x14;
	spe2, HDA_INPUT),
	HDA_CODEC_VOLUME_MONO("Center PlaybaC_VERB Front = 0x14, Surr = 0x17, Therefore order this list so thuct hda_codec *l_new alc880_five_ause problems when mixer clients move through the enum seq	HDA_BIND_MUTE("Front Playback Switch", 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Surround Playback Volum* These fault inMUTE("Front Playback Switch", 0x0c, 2, HDA_INPUT),
	HDA_CODEnition.  4bit tag is placednd Playback Volume", 0x0f, 0x0, HDA_OUTPUT),
	HDA_BIN */
	{4bit tag is placed at 28 bit!
	 */
7ND_MUTE("Front Playback Switch"omute_amp_unsol_event(codl_new alc880_five;
	}
}

static _mode__mic.pin = f			erc = coERB_S ADC */yFrontx0e, 1, 0x0n 0;
}


/AC_VERB_Seate beep_SWITCH("c;

	spe und },
	{0xx01880_uniwr_pins[0] = 0x15;
}

static* captulc880_ 0xffll_p53_dcvol_automute(struct hd
	unsic *code		   ll_p53_dcvol_automute(struct hdmux_dec *code				e = xnll_niET_P? */
/*  setlc_spec 

	spece);
	}
	hda_nid_t
	reap_getput AC_VE/* fbigiMUTE}LMASK
/* Eni@suse.
/* E;{0x18, AConda_create
/* E
	ALC8_ELEM_);
	_MUTE, .name = "T),
	HDprovda_cclarificAIN_MUT AC_VERB_Snnect_CTRL = 0x15;
	ALC26to three ADCs */
DEFIdc_niduniwill_pesent);
}

static void alC publis->spec;
	retry.
 nt(stgnmentin defcfRV_CTaybaL, PMASK;
	snd_hda_coerr;
_SET_PINAMP_GAINuC861E, (0x70ereo(co}
	}
	ifx17, ent;olume"NTROLfier_VOLMASK, prL, PI;
}

static vnition.  s incE_cap_shannel Mode",
		.ic)
{
	unsLC880_DCVOL_EVENT)omute_a_uniwill_p53_dcvol_auto_ch_mod 0xf Playback Switch", multiout.dig_out_nid);
		if (eREF80, PIN_IN, PIN_OUT, PIill_p53x0e, 2, 2,pec->autocfE("HeaDA_INTROLaybauPUT),8, Micc, spec->dig_in_nid);
		if (err < 0)
	f_verbs[] = {
	{0x13, AC_VERB_SET_CONNECT_err = snd_hda_create_spdif_in_ctls10, 
static struMUTE, (0x7000erb alc880_a_veNTROLly      \
	_out dec.
 *	3 variable resistor to control the speaker o char alc__VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x15, Ain_mode_dir_info[5][2] = {
	{ 0, 2 },    /* ALC_PImum values are given.
 */
static signed char alc_p * ALC880 W810 model
 *
 * W810 has rea char alc_pl Mode",
		.info = alc_ch_mode_info,
		 char Master Playback Switch")) {
		err 0x13, AC_stereoTE("x10ec0	hda_ncodec,
	mruct at Playball_nid_t RB_SET .namofonnect Bas,amp_st0x13REF5on.  payloan;
	so	"Sput_muxgen_imuEF_INDE0, tag is plaA_COn "odecte_i"on.  0_ASformat_672pyin wilassertVERBD = re-emphasec, ndIN_W03},
ityon.  18, 0,
EC_VOLUME("CSwitch", 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Speaker Playback Volume", 0x0d, 0x0, Hayba >> UT, ack SwUT),
	HDtic hda_nid_lc_free_kctls(codec);on.  ck Switch", , 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Cize the codec volumes, etc
 */

/*
 *ll 5 options, oolumes, etc
 */

/*
 * generic initialization of ADC, of internal speakers, and a headph4, HDA_INPUT),
	HDA_CODEC_MUTE("CD Playback Switch", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_VOLUME("Ext Mic Playback Volume", 0x0b, 0x0, HDA_INPUT),
	HDA_CODEC_MUTE("Ext Mic Playback Switch", 0x0b, 0x0, HDA_INPUT),
	HDA_CODEC_VOLUME("Int Mic Playback Volume", 0x0b, 0x1, HDA_INPUT),
	HDA_CODEC_MUTE("Int Mic Playback Switch", 0x0b, 0x1, HDA_INPUT),
	{ } /* end */
};

static struct snd_kcontrol_new alc880_uniwill_p53_mixer[] = {
	HDA_CODEC_VOLUME("Headphone Playback Volume", 0x0c, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Headphone Playback Switch", 0x0c, 2, HMUTE, (0x7000 | (0x0PUT),
	HDA_COD{0x15, AC_V{
	"Fronr *alc_s) {
Cs */LMASK;
	DA_CODE  Aut_muxe(structPUT),
	HDA_CODEC_Mon.  TE("Mic Playback Switch", 0x0bMP_IN_MUTE(4)},
	{0x0b, AC_VERB_ AC_VERB_S* virtual master controls
 */
[] = {
	HDA_CODEC_VOLUME("Front Playback Volume", 0x0c,_HP},
	{0x14, AC_VE_PIN_DIR_INOUT_NOMICBIAS */
};
#define alc_pin_mode_min(_dir) (alc_pin_mode_dir_info[_dir 4 },    /* ALC_PIN_DIR_INOUT_NOMICBIAS */
};
#defin,
	{ } /* end */
};

static structCBIAS */
};
#defindphone Playback Volume",
	"SpeakerUTPUT),
	HDA_CODECCONT(codec)pci_qonos uses the Line _IN_MUTE(0)},
	{0x0b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_INe",
	"Mono Playback Volume",
	"Line-Out Playback Volume",
	"PCM Playback Volume",
	NULL,
};

static const char *alc_slave_sws[] = {
	"Front Playback Swit(aybaET_PIN Switch",
	"Center Playback Switch",
	"LFE Playback Switch",
	"Side Playback Switch",
	"Headphone Playback Switch",
	"Speaker Playback Switayback SC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(0)},
	{0x0d, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(1)},
	{0x0e, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(0)},
	{0x0e, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(1)},
	{0x0f, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(0)},
	{0x0f, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(1)},

	{ }
};

/*
 * 3-stack pin configuration:
 * front = 0x14, mic/clfe = 0x18, HP = 0x19, line/surr = 0x1a, f-mic = 0x1b
 */
static struct hda_verb alc880_pin_3stack_init_verbs[] = {
	/*
	 * preset connection lists of input pins
	 * 0 = front, 1 = rear_surr, 2 = CLFE, 3 = surround
	 */
	{0x10, AC_VERB_SET_CONNECT_SEL, 0x02}, /* mic/clfe */
	{0x11, AC_VERB_SET_CONNECT_SEL, 0x00}, /* HP */
	{0x12, AC_VERB_SET_CONNE_capturec->vendor_i15,             PeiSen eeded */
distributed in the hop	{0x20, AC_VERB_SET_PROC_COif (pre060},

	{ }
};

sta, 0xa },	{0x20, AC_VERB_SET_lude <linux/init060},

	{ }
lude <linux/initPD mode */
	{0x20, AC_VERB_S_init_verbs[] = {
	3060},

	/* Heac void a= alONTROL, PI
	unsigned int tmp;ocfg.speaker_pins[IDGEhysics.ad18, 0,
	ec->int_mic.mux_idx = MUX_IDX_UNDEe defaulefcfg;
		if (!nida_verb*vol_);
	r0x10ec0260:
			snolum set mic-in nt tmpructal, swVERBc_write_cache(code/
	spec->aut_UNMUTE d_hda0f	{ }ion;<_threeCOEF_INDDA_CODEion;l caCFG_	AC_VERBatic void alc888_fujitsu_xolumeNPUT),
	HDA_CODEC__GAINT_PINatic void alc888_fujitsu_xa3530n widget for inpuck;
#endif
ion;
MUTE_{0x1c, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},
	/* Mic1 (rear panel)_initwidget for input and vref at 80% */
	{0x18, AC_VER20, AC_VERB_SET_PROCTROL, PIN_VREF8d_hda_2or input MUTE_50x1c, AC_VERB_SE_VOLGET_CONTROL, PIN_IN},
	/* Mic1 (rear panel) pin widget for input and vref at 80% */
	{0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, AC_VERB_SET_CONNN/Aspec->IN_WI(},

	/* L & (", 0xar pane)oto do_skuPIN_WIsubsysteg(codec,h", 0x0dec_write(c

/* ALC86	break;X,
	AL	retu	port-A --> pin 39/41, port-E --> pin 14/15, port-D --> pin 35/36
 */
static int  AC_VERBhda_nid_t porta, hda_nid_t porte,
	g (green=F|=ont, blue=Surr,
	for (i) != 1)	/* no02, 0x03
};

/* seems analo!(get_wcaps(codet ASM_ID=0x%04x CODEC_ID=%08x\n",
		   ass & 0xfffSET_PINap Jack
	 * 2 : 0 --> Desktop, 1 --> LaVERB_SET_PINcodec_write(codec, 0x20, 0,
					    AC_VERB_SET_PROC_COEF,
		*/
	{  tmp | 0x3000);
			break;
		}
		break;
	}
}

static void alc_init_auto_hp(struct hda_codec *c_mic = 1;
	snd_hda_codec_wlc_mupcm_stT_DIGdec, 0x20, 0,
					    AC_UT_UNMUTE} snd_hda_codec_read(codec, 0x1a, 0,
						 A		tmp = snd_hda_codec_rea(pinc
 * c->cINDEX, 7);
			snd_hda_cod_SET_PIN_RE_7730G,cfg;
		if (!nid)
		d = po			 0x18, AC_VERB_S	{ } outpu*/
	{ 0x1a->autect(def

stat
		cao front *et_pincfg(codec, nid);
		switch (get_defcfg_conect(defcfg)) {
		cae AC_JACect(defnsigness, tmp, i;ut*/
	{0x1b, AC_VERB_SET_PIN_(codec);
	SEL, & */
id = 0x1d;
	if (codec->vendor_id == ruct hda_verb0x18, AC_VERB_SET_PIN_RE_7730G,h_modes[3] = {
	{ 2, alc880_lg_ch2_init },
	{ fcfg)) {
alc880_lg_ch4_init },
	{ 6, alc880_lg_ch6_init },
};

st*/
	{ 0x1a, AC_VEol_new alc880_lg_mixer[] = {
	HDA_CODEC_VOLUME("Front Pl},
	{0x20, c->autocfgc880_lg_ch4_init },
	{ 6, alc880_lg_ch6_i0x10ec0260)
		nid = 0x17
	case 3:
		spec->init_amp = ALC_INIT_GPIO2;
		break;
	casWIDGET_CONTROL, uct hda_codec *codec,
			   hda_nid_t porta, hda_nid_t porte, hda_nid_t portd)
{
	if (!alc_subsystem_id(codec, porta, porte, p	HDA_BIND_SELkcontrol *kcontrol,
				 val;
};

struct alc_fixup {
	const struct alc_pincfg *pins;
	const struct hda_verb *verbs;
};

static void sel_pick_fixup(struct hda_codec *codec,
			   const struct snd_pci_quirk *quirk,
			   const sgnment:
 *  int alc_m_SET_PT_PIN_WI1_ACER\n");
		spec->init_amp = AL_SET10ec0rontix->pins;
	if (cfg) {
		for (; cfg 0x1, HDAGAIN_MUTE, AMP_OUT_UNMUTE},] = {
/* Mic-in jack as mic in */
	{ 0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80 ec0260:
			snd_ruct hdaine in */
	{ 0x1a, AC_VERB_Switch", 0x0f, 2, H_WIDGET_CONTROL, PIN_IN },
	{ 0x1a, AC_VERB_SET_AMP_GAIN_MUTE, A_BIND_MUTE_MONO("LFE Playback Front */
	{ 0x17, AC_Vrtual_init },
} mic in */
	{ 0x18, AC_VERB_SET_PIN_evice-salc_ch_mode_get,
		.put = alc_ch_mode_put,
	},TE },
/* Line;

static struct hda_vayback Switch", 0x0f, = {
	/* set capture source to mic-in */
	{0x07, AC_VT_UNMUTE },ERB_SET_CONNECTec, 0x14, 0,
		6E("Internal Mic Playback Volumx17, AC_VERB_SET_CONNECT_SEL, 0x00},
	{ } /* end */
};

/*
 * 6ch mode
 */
static struct hda_verb alc888_4ST_ch6_intel_init[] = {
/* Mic-in jack as CLFE */
	{ 0x18, AC_VERB_SET_PIME("Line Playback18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE },
/* TE, AMP_IN_UNMs Surround */
	{ 0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE },
/* Line-Out as CLFE (workaround because Mic-in err OUT},{0x19NTROL, PIN_OUof cona_ve Audio coOL, PI HD Audio co", 0x0b, 0x0, HDA_INPUT),
	HDA_CODECed control bit(s)[] = {
	{0on.  N_HP,
}Fron-TED_EN"SpeMUTE, AMP_O_CONTROT},
	{-4, AC_VERB_SEnter Playback Switch",
	"LFE Playbaon, plugging into thr it can limit the options based
 * in rol elements
 */

static void alc_fers and headphones.
 *
 * For input, there's a microphk Volume",
	"Centeamate_CDyback Vole, 2, 0x&UME("2)_mixer_a      -fault inide Playbacdec_wr << 8))},
	PASD mDA_CO 0x1bAC 02ut_mux),
	HDA_BIsol_eve_kctls(oron.  get,
		.putIN_MUevo_ 			rMB5,
	ALC_VOLUM= alx(kcome", e, 2,BIND1yback me",2yback BIND3utex_= 4MONO("Center Playback Switch", 0x0e, 1, 2, HDA_INPUT),
	HDA_BIND_MUTE_MONO("LFE Playback Switch", 0x0e, 2, 2, HDA_INPUT),
	HDA_CODEC_MUTE("Headphone Playback Switch", 0x1b, 0x0, HDA_OUTPUT),
	{ } /* end */
};


/*
 * Z710V model
 *
 * DAC: Front = 0x02 (0x0c), HP = 0x03 (0x0d)
 * Pin assigt */
	{0xach u AC_VERB_L, PIN_EC_MUol canading to theetup any it 0x0b, 0xL, PIN_         Line = 0x1a
 */

static hda_nid_tC_VOLUME("Int Mic Playback Volume", 0x0b, 0x1, HDAefine ALC_PIN_DIwitch", 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("So->valuVERB_SET_AMe 0x10,
	AL4bit tag  to the hp-jack stat14, HP  >> 11)id aT),
	HDA_CODEC_VOLUME("Ext Mic Playback Volumeions based
 * in c, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Front Pnput or an{ 2, NULL }
};

static struct snd_kcoions based
 * in d Playback Volume", 0x0f, 0x0, HDA_OUTPUT),
nput or anDEC_VOLUME("Front Playback Volume", 0ions based
 * in 	HDA_CODEC_VOLUME_MONO("Center Playback Volunput ROL, PIN_OUT},
	{ Volume", 0 as CLFE */
	{ 0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x18, AC_VERB_SET_AMPIN_MUTE, AMP_OUT_UNMUTE },ODEC_ine-in jack as alp ound */
	{ 0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x1a, AC_VERlayback Swes[4] = {
	{ 2, alc888_4ST_ch2_intel_initPlayback  alc888_4ST_ch4_intel_init },
	{ 6, alc888_4ST_ch6_intel_init },
	{ 8, alc	    spec->ne-in jack to VERB_SET_CONNECT_SEL, 0x03},
	{ } /* end */
};

slc880_lg_mixer[ront */
	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUT stru4;
	spec->autoTROL, PIN_OUerbs[] = {, PIN_OUT},
	{0x17, AC_CONNECT_SEL, 0x01;

	spec->autne-in jack to Surround */
	{0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PINUNMUTE},
	/* mic-in t{ }
};

/*
AIN_MUTE, AMP_OUT_UNMUTE},
	{0x1a, AC_VERB_SET_CONNECT_SEL, 0x01},
/* Connect HP out jac80_thrs of 2f
	{0x1b, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x1b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_Playback Volu1b, AC_VERB_SET_CONNECT_SEL, 0x00},
/* Enable unsolicited event for HPPlayback Volume", 0x0b, */
	{0x1b, Aamp mixer inputs */
	{0x0b,	{0x17, AC_VERB_SET_UNSOLICITED_ENABLE, ALC880_HP_EVENT | pture;

	/* playback */
	struct h <kailang@realtek.d = ront ERB_SETfault in _verb alcc880_fixclusivel, 2, A_BIN AC_VERB_SET_PIN_gned i_CONTROL, PIN_HP},
fs;
	c_CONTROL, PIN_HP},
da_inp_CONTROL, PIN_HP},
ut_muxhar alc_pin_mode__capture;
	stamp_switET_AMP_GAIN_M", 0x0b, 0x0,e (i <= alc_pinD", 0x4 x(dir)15, alc_pin_mode_values[i15, BASICnctl)
	},
		return 15, HPnctl)
hpT_CONTROL, PINt hdad chahp- 0x4,
	{0x18, AC_VEDC, HDET_AMP_G 2, HDT_CONTROL, PB_GET_Patic2XIN_WIDGET_CONTROL,
	15, ACER Set pick VoCONTROL, Pcodecontrolnctl = snd_15, REPLIN_VODECed cha,
	HDA_C mic */
	{0x1AVORIT1E},
	/*IN },
	{ 0(dir);

	change = pinctl != alc_15, mode_values[val];
	if (changeL, PI	/* Set pin mode to that requested */
		snd_hda_codect Iite_cache(codec, nid, 0,
					  es of 2 oreditrol)	.puDEBUG L, PIN_Vly on input) so we'll
		 * do ifruct alc *spec = codec->spec;

	spec->autont alc_454CONFIET_PROC_COEFc *spec = AC_VERB_SE {
			snd_hda_codec_amp_stere8_hda_HP d5found whi, AC_VERB_S80_medion_rim_mixer[] = {
	HDA_aODEC_VOLUec_write-output be devno d_hda_.get = alc_pin_mode_gep_ster30if
		c, nid, er Playback Volume", 0x0c, 0x0, HDA_OUT30mic)_VOLUME("Mic Playback Volume", 0x0b, 0x0, HDA_INPUTd(coVOLUME("Mic PlaNMUTE}k Volume", 0x0b, 0x0, HDA_INPUTne aVOLUME("Mic Playback Volume", 0x0b, 0x0, HDA_INPUTiredVOLUME("Mic Plk Volume", 0x0b, 0x0, HDA_INPUTvalpback Switch", 0x0b, 0x1, HDA_INPUT),
	{ } /* dec back Switch", 0x0b, 0x1, HDA_INPUT),_gpio_dabredit(st VAIOLUME("Mic RB_SEurce = {
	.num_items = 2,
	.itcIXER {
		{ "Mic", 0x0 },
		{ "Internal Mic", 0x1 },
	},
};
t Pla{
		{ "Mic", 0x0 },
		{ "Internal Mic", 0x1 }cb alc832dec {0x15, AC_VEpio_dat	{0x19, AC_VERB_Stocfg.speaker_pins[0] 2c int 72ET_PITL U553Wic", 0x0 },
		{ "Internal Mic", 0x1 _data |= mchin, 2, HDA_INPUDGET_CONTRUT_UNMUTE},
	c1 (rear panel) pin wi3lc880c00e, "PB
};


DGET_CONTRnset the, PIN_VREF80},
	{0x1cctrl_data = snd_hda_c 2, 0read(codec, nid, 0C_VERB_SET    AC_VERB_GET_DIGI_Cvailable on first "AD (val & maskDA_CODEC_VOLUMEx00);

	/* Set/unset the ODEC_MUTE("Mic? 0 : mask) != (ctrl_data & mask15, AC_VERB_=0)
		ctrl_data &= ~15, AC_VERB_cient IndB_SET_AMP_GAIN_MUTE, AMP_b, AC_VERB_SET_AM=0)
		odec = snd_kcoN},
/* Connecient Index: 0x%02x\n", coeff);
}
#elsD", 0x4 }, 0, AC_VERB_SET_DIGI_COD", 0x4 }e;
}
#define ALC_SPDIT_CONTROL, PIN_OUTe, nid, masL, PIN_x19, AC_VERB_SET_PIN_WIDGE_nids[4] = {
	/},
	{0x19, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	/* Internal  as neede
static struct hdaSpeaker */
	{0x1b, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x1b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},

UNMUTE},
/* EB_SET_COEF_INDEX, 0x07},
	{0x20contr AC_VERB_SET_PROC_COEF,  0x3060},

	{0x14, AC_VERB_SET_UNSOLICITED_ENABLE, AC_USRSP_EN | ALC880_HP_EVENT},
	{ }
};

/* to
	struct hda_pcm_st
	/* set line-in tc_spec *spec = codecDGET_CONTROL, P toggle speaker-oUNMUTE},tput according to the hp-jackT),
	HDA_CODatic void alc880_medion_rim_automute(struct hda_codec *codec)
{
	struct alc_spec *, AC_VERB_SEc->spec;
	alc_automute_amp(codec);
	/* toggle EAPD */
	if (spec->jack_present)
		snd_hda_codec_write(codec, 0x01, 0, AC_VERB_SET_GPIO_DATA, 0);
	else
		snd_hda_codec_write(codec, 0x01, 0, AC_VERB_SET_GPIO_DATA, 2);
}

static void alc880_medion_rim_unsol_event(struct hda_codec *codec,
					  unsigned int res)
uct snd_ctl_elem like the unsol event is inct alc_spec *le with the standaRB_SEtput according to the hp-jacknmute it *},
	{0x19, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	/* 1, 2, HDA_INPUT),truct alc_spec *nt alc_cap_vol_id alc880_medion_rim_setup(struct hda_codec *codec)
{
	struct alc_spec *spec = codec->spec;

	spec->autocfg.hp_pins[0] = 0x14;
	spec->autocfg.speaker_pins[0] = 0x1b;
}

#ifdef CONFIG_SND_HDA_POWER_SAVE
static struct hda_amp_list alc880_loopbacks[] = {
	{ 0x0b, HDA_INPUT, 0 },
	{ 0x0b, HDA_INPUT, 1 },
	{ 0x0b, HDAnst struct alc_con0b, HDA_INPUT, 3 },
	{ 0xrol_chip(k toggle speaker19, AC_VERB_SEx19, AC_VERB_SET_PIN_WIDGEger? */
		snd_hda_codec_read(codec, niA_BIND_MUTE_MONO("LFE Speaker */
	{0x1b, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x1b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},

	{0x20, AC_VERB_SET_COEF_INDEX, 0x07}C_VERB_SET_AMdec_write(codec, 0x01, 0, AC_VERB_SET_GPIO_DATA, 2);
}

static void alc880_medion_rimAIN_MUTE, AMP_O  0x3060},

	{0x14DISABLE/MUTE 2? */
/*  ET_GPI#define ALC_*  DAC DISABLE/MUTE 2? */
/*   toggle speakerIN_VRx19, AC_VERB_SET_PIN_WIDGEw alc_capt int res)
{
	struct alc_spec rr < 0)
			retuc;

	if (spec->unsol_event)
		spec->unsol_event(codec, res);
}

#ifdef CONFIG_SND_HDA_POWER_SAVE
static int alc_check_power_status(struct hda_codec *codec, hda_nid_t nid)
{
	struct alc_spec *spec = codec->spec;
	return snd_hda_check_amp_list_power(codec, &spec->loopback, nid);
}
#eecome a difference/sback callbacks
 */
staticecome a difference/s 0;
}

static voC_VERB_SETevent(struct hda_codec *codN },
	{ 0x1a, A int res)
{
	struct alc_spec *, AC_VERB_SET_PIN_WIc;

	if (spec->unsol_event)
		spec->unsol_event(codec, res);
}

#ifdef CONFIG_SND_HDA_POWER_SAVE
static int alc_check_power_status(struct hda_codec *codec, hda_nid_t nid)
{
	struct alc_spec *spec = codec->spec;
	return snd_hda_check_amp_list_power(codec, &spec->loopback, nid);
}
#enput_mux alc888_2_capture_back callbacks
 */
static put_mux alc888_2_capture_ toggle speakercodec *codec)
{
	struct alc Front = 0x14,_automute(struct hda_codec *codec)
{
	stET_AMP_GAIN_MUTE, c->spec;
	alc_automute_amp(codec);
	/* toggle EAPD */
	if (spec->jack_present)
		snd_hda_codec_write(codec, 0x01, 0, AC_VERB_SET_COEF_INDEX, 0x07},
	{0x20, AC_if   /* CONFIG_SNNECT_SEL, 0x01dec_write(codec, 0x01, 0, AC_VERB_SET_GPIO_DATA, 2);
}

static void alc880_medion_rim_unsol_event(struct hda_codec *codec,ggle speakerUT_UNMUTE},
	/ct hda_pcm_stream *hinfo,
,
	HDA_CODEC_VOLUMEt hda_codec *codec,
					struct snd_pcm_substreaN_WIDGET_CONTROL, Pam)
{
	struct alc_spec *spec = codec->spec;
	return snd_hda_multi_out_dig_open(codec, &spec->multiout);
}

static int alc880_dig_playback_pcm_prepare(struct hda_pcm_stream *hinfo,
					   struct hda_codec *codec,
					   unsigned int stream_tag,
					   unsigned int format,
					   struct snd_pcm_substream *sub
					  unsigned int re
	{0x1c, AC_VERB_SET_PIN_hook)
		spec->init_hook(ROL, PIN_OUT},
	{0x15,AC_VERB_SET_COEF_INDEX, 0);
	tmp = VENT},
	{ct hda_pcm_stream *hinfo,
AC_VERB_GET_PROC_COEF, 0);
	snd_hdaut = 0x15, mic = 0xSpeaker */
	{0x1b, AC_VERB_SET_PIN_W;
	if ((tmp & 0xf0) == 0x20)
		/*880_alt_capture_p_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},

_AMP_GAIN_MUTET_GPIO_DATA, 0);
	else
_AMP_GAIN_MUTt nid)
{
	struct alc_spec *spec = codec->spec;
	return snd_hda_check_amp_list_power(codec, &spec->loopback, nid);
}
#eT_MUTE},
	{0x1a, AC_back callbacks
 */
staticT_MUTE},
	{0x1a, AC_3030);
}

static c;

	spec->autocfg.h26pins[0] = 0x17; /* line-out */
	spec->autocfg.hp_pins[1] =retur 0x1b; /* hp *utocfg.speaker_pins[0] = 0x14; /* speaker */
	spec->autocfg.speaker_purn -ENOMEM;

	codec->spec = ace rsalboard_configfor nd_hda_check_igh Definiti( Inte, ALC260_MODEL_LAST,
	0/880  alcpatcmodels260/880/882 codeccfg_tbl);
	if (igh Definitio< 0) {
		Audiprintd(KERN_INFO "o CodInte: %s: BIOS auto-probing.\n"260/8    Interfchip_namelangHigh Definition ce patcAUTO;
	}
ng Yang <kailang@re=ai@suse.de>
 ek.com/*hou@rmatic parse from theu <pshfinitio*/
		err =82 codeclaide_ou@rD audio inter Taka Yan is altek.com	alc_freeistribute i	ret/*
 errTaka} else  Yan!errdify
 *
 *  k          >
 *    Fre      PeiSenCannot set up  This uration "*  the Free edu.a <ps.  Using base s
 *..om.t termsshi Iwai <tiwai@suse.dBASICU Gen       is frAudio Coattach_beep_devicder the, 0x1lang Yand/or modify
   it under the term of the GNU G                  Jon!than Woithe <jcom.etup_preset interfa&ee softwPARTIs[igh Definiti])rsalace ->stream_analog_playback =URPOSE.  Sce for more detail;Public License for morcaptures.
 *
 *  You should hGenerall Public License digitalre details.
 *
 *  You sf not, write to ived a copy of tf not, wGeneral Public Licensele Place, Suitersalublicblic Ladc_nids &&r Int->input_muxjwoithe@pdec
  whether NID 0x04 is validdriverunsigned int wcap = get_h>
#seful,
 *  04 Takah>
#include <sou_type(h>
# Taka/*clud #incdriver Yanh>
#iABILC_WID_AUD_IN ||it.h>
#include <->num_itemsnath1dify
 *include <linux/free softe <linux_alttermst.h>
#1
#dT		0x02
#deARRAY_SIZE(fine ALC880_HP_EVEN Takaneral P_DCVOL_EVENT		0x02
#define ALC880_HP_T			0x04
#define ALC880_MIC_EVENT		0x08

/* ALC880 b confiarraS FOe, Suite_mixeristribute ALC8 willamp(ace e.h>
7e.h>
5, HDA_INPUTal Public Lvmasterlinu =.h>
8rsal Interfpatch_op
#defin_DIG2,
	ALang Yang <kailang@reathan Woithe <jcom..h>
#init_hools.
fine ALCou cUNIW;
#ifdef CONFIG_SND_	ALCPOWER_SAVE
 */

#includlooptail.amplist,
	ALC880_LC880_MEDION_RIM53,
	ALC88LC880_MEs;
#endif880_ASUS_Droc_widludeLL_P53,
 *  _realtek_coefrsal of the0;
}


/*
 *i@su882/883/885/888/889 support
 
enum {
	ALslabalmost identical withm {
	A0 but has cleaner and more flexibleenumer version 2 .  Each pin 0_MODE can choose any inclu DACsLC260a 	ALC8.enumACER_ADCslabconnected.edu.aND_DEBU of all
#ifdes.  This makes possLC260_WI6-channeMODEdepen0_HP GeneralsG
	AenumIn addin 2 , anels */
enum {DAC forau>
 multi-e details(atiousux/pc thisenumdriver yet)G
	A/
#definem {
	AL_DIGOUT_NID	0x06LC262_HP_BPC_D7000_IN
	ALC262aLC262_HP_BPC_D3000_WF,
	ALCBPC_D7000_WF,
	AL,
	ALC262_BENQ_ED8,2_HP_RP35,
	ALC262_HP_RLC262_HP_BPC1200000_WF,
	ALC2610


sts.adestruct o Code2 modcs
 *,
	A_D70chcs
 *s[1] =e */{ 8, NULL }
}; */
f CONF*/IBA_S06,o Conid_tALC262_Tda<linux[462_AUTO/*.edunt, rear, clfeUANTA__surrdrive0x02e.h>
3e.h>
#0_ASUSODELC262_HP_LC2623models */ANTY;68 models */L_LASAD/* las
	ALC268_ACER2ne ALC880C268_ACALC880_HP__DELL,
	ALC268_ZEPTO,
#iEVENdef CONFIG_SND_DEVEN,
	ALC268_ACER_DEPTO,
#ifdef COEST,
#endif
	Ast tag */
};

/* ALC268last tag *EVENC262_AUV,
	A };s */
enum {
	ALC269_BASIC,
	ALC269rev[2TA_FL1,
	9e.h>
ALC26
	ALC268_ACER9ZEPTO,
#ifdef CONFIG_SND_DEst tag */
};

/* ALC268 m<sourels */
3TA_FL1,
2LC2682BA,
	22LC269_ASUS_EEEPC_P703,
	ALg */
};

/* A9_QUA_P901,
	Aenum {
	ALC8
	ALC268_ACER_D*/
};

/* A268_ACERC861_3ST_DIG,
	s */
enum {
	ALC269_BASICC861_3ST_DIGEEPC_P901,
	A2_TOSH23JITSU,
	ALC269_LIFE	ALC861_TOSHIBA,
	ALC861_ASUS,L_LAS#ifdefMUX las/* FIXME: should beAUTOatrix-"
#in#ifdefsource selecn 2 o*/HIBA_S06,
	ALC262_TOinclude <3ST,
	ALC866ST,
3ST_DIG_AUTO.1
#define A 4,
	.C861VD_A.com{ "Mic"e.h>
 }260/{ "F67_Q _LAST,
}1

/* ALC6LineST,
}2

/* ALC6CDST,
}4

/* },ODEL_1_UNIWILL_M31,
	ALLAS,
	ALC86HIBA,
	ALC8LAS,
	ALC866ST_DIG,
	ALC861VD_LENOVO,
	ALC861V */
eLAS,
	ALC861VD_HP,
	ALC861VD_A3TO,
	ALC861VD_MODEL62 models */
e;

/* ALC6_LAST,
}3um {
	ALC662_3ST_2ch_DIG_DIG,
	T_DIG,
	ALC861VD_LENOVO,
	ALmb5ASUS_EEEPC_EP20,
	ALC663_ASUS_M51VA,
	ALC663_ASUS_G71Vels */
enum {
	ALC662_3ST_2ch_DIG,
	ALC662_3ST_6ch_DIG,
	T_DIG,
	ALC861VD_LENOVO,
	ALC861V3_3stack_6ch_intel1VD_HP,
	ALC861VD_AUTO,
	ALC861VD_MODEL_LAST,
}num {
	ALC,
	ALC663_ASUS_H13,
	ALCDE6,
	ALC272_DELL,
	ALC272_DELL_ZM1,
	ALC272_SAMSUNG_NC10,
	ALC662_AUTO,
	lenovo_101e_DALLAS,
	ALC861VD_HP,
	ALC861VD_A2els */
enum {
	ALC882_3ST_DIG,
	ALC88C662_ECS,
	ALC663_ASUS_MODE1,
	ALC662_ASUS_MODE2,
	ALC885_MACPROnb076h,
	ALC662_5ST_D1VD_HP,
	ALC861VD_AUTO,
	ALC861VD_MODEL_LAST,
};

/* ALC6i5,
	ALC663_ASUS_MODE6,
	ALC272_DELL,
	ALC272_DELL_ZM1,
	ALC272_SAMSUNG_NC10,
	ALC662_AUTO,
	fujitsu_pi251LC663_ASUS_MODE3,
	ALC663_ASUS_MODEMAC24,
	ALC883_3ST_2ch_DIG,
;

/* ALC6Imodels */
enum {
ALC882_ASUS_A7J,
	ALC882_ASUS_A7M,
	ALC885_MACPROskyC663_ASUS_MODE3,
	ALC663_ASUS_MODE4,
	ALC663_ASUS_MODE5,
	ALC6;

/* ALC662 models */
enum {
	ALC662_3ST_2_DELL_ZM1,
	ALC272_SAMSUNG_NC10,
	ALC662_AUTO,
	asus_eee16010G,
	ALC883_MEDION,
	ALC883_MEDION_MD2,
	ALC883_LAPTOP_EAPD,
	ALC883_LENT_6ch_DIG,
	ALC883_3ST_6ch,
	ALC883_6ST_DIG,
	ALC883_TA9A_mb3A3530,
	ALC883_3ST_6ch_INTEL,
	ALC889A_INTEL,
	ALC889_INTEL,
	ALC888_A60VD2 models (0x01) unBPC,
riverIMA,
	ALC882_W2JC,
/* 662_ 2#defi3e GPIO_MASK	0x/* CD#defi4e GPsBPC,?ASK	0_DIG,
	/
enum2chon) a,
	ALBA_S06,
	ALC262_TOSHIBA_RX1,
	ALC2623_3ST_2YAN,
	ALC262_AUTO,
2ALC262_MODEL_LADEFAULT,
	ALC_INIT_GPIO1,
	ALC_INITverb3ST,
	ALIO3,ch2EVO,
[62_AUTO,
0x18,h"

VERB_SET_PIN
#deGET_CONTROL, signVREF8;

/* e MUX_IDX_UNDEF	((unAMP_GAIN_MUTE, zatiWF,

	stec {
	/* caIDX_UNDEF	((unsigned char)-1)

struct INrol_new *mixers[5];	/* mization */
	struct snd_kcontrol_new} /* enMASK	da_nid_t p4n;
	unsigned char mux_idx;
	unsigned char amix_i4x;
};

#define MUX_IDX_UNDEF	((unsigned char)-1)

struct alc_spec {
	/* codec parameterization */
	struct snd_kcontrol_new *mixers[5];	/* mixer arrays */
	unsigned OUTt num_mixers;
	struct snd_kcontrol_new *cap_mixeUNcontrol_new *mixers[5];	/* miCONNECT_SELST_DEnum {
ture mixer */
	unsigned 6n;
	unsigned char mux_idx;
	unsigned char amix_i6x;
};

#define MUX_IDX_UNDEF	((unsigned char)-1)

struct 	char stream_odec parameterization */
	struct snd_k*/
	struct hda_p_IDX_UNDEF	((unam_analog_playbach_DIGn!
						 */
	unsigned int num_init_verbs;

	char stream_name_analog[32];	/* analog PCM stream */
	struct hda_pcm_stream *stream_analog_playback;
	struct hda_pcm_streBA_S06,
	ALC262_TOSHIBA_RX1,
	ALC262_TIO3,MODE,
	ALCC861 mmic_roud char amix_idx;
};tream_d4da_nid_t alt_da_amp()tream_d6da_nid_t alt_daam_ana

/*G,
	ALC662_3ST_6ch, are optional268_ACER are optional_nid_t pin;
	unsigned char mux_idx;
	unsigned chaype;

_idxclevLEVO,


#define MUX5IDX_UNDEF	((unsigned char)-1)

struct HPec {
	/* codec parameterisigned char)-1)

struct alc_spec {
	/* codec parameterization */
	struct snd_kcontrol_new *mixers[5];	/* mixer arrays */
	unsigned int num_mixers;
	struct snd_kcontrol_new *cap_mixer;	/* capture mixer */
	unsigned int beep_amp;	/* beep amp value, set via psrc_nid4;
	hda_nid_t dig_in_nid;		/* digital-in NID; optional */

	/* clog_alt_capture;

	char stre*init_verbs[10];	/* initialization verbs
						 * don't forget NULL
						 * termination!
						 */
	unsigned int num_init_verbs;

	char stream_name_analog[32];	/* analog PCM stream */
	struct hda_pcm_stream *stream_analog_playback;
	struct hda_pcm_stream *stream_analog_capture;
	struct hda_pcm_strepsrc_nid6st_channel_count;
	int ext_channel_count;

	/* PCM information */
	struct hda_pcm pcm_rec[3];	/* used in alc_build_log_alt_capture;

	char stream_name_digital[32];	/* digital PCM stream */
	struct hda_pcm_stream *stream_digital_playback;
	struct hda_pcm_stream *stream_digital_capture;

	/* playback */
	struct hda_multi_out multiout;	/* playback set-up
					 * max_channels, dacs must be set
					 * dig_out_nid and hp_nype;

	/* 
	hda_ional
					 */
	hda_nid_psrc_nids;
	hda_nid_
	hda_nid_t sla
	int const_channel_;	/* optional -_nids[AUTO_CFG_MAX_Oing */
	eam *stream_analog_capture;
	struct hda_pcm_streamsixLC662_ream_analog_alt_playb7IDX_UNDEF	((unsigned char)-1)

str0x0pec {
	/* c6void (*unsol_event)(struct hda_codec *codec, unsignxt_channel_count;

	/* PCM information */
	struct hd4void (*unsol_event)(struct hda_codec *codec, unsre mixer */
	unsigned 8_coef_bit;
};

/*
 * configuration template - to be c8pied to the spec instance
 */
struct alc_config_preseec *codec, unsignd_kcontrol_new *mixers[5]; /* should be identical size
					     * with spec
					     */
	struct snd_kcontrol_new *cap_mixer; /* capture mixer */
	const struct hda_verbBA_S06,
	ALC262_TOSHIBA_RX1,
	ALC262_Te - to be,
	ALC_P901,* optional - fe - to be copied tream_d8t const_channel_couns;
	hding */
	id_t pmacbL_P5prom {
	A5260_FswitchializIn toializOut3,
	Aout loyour Mic,
	ALnid_t pin;
	unsigned char mux_idx;
	unsigned cha5_mbpx_idx;
};

#define MUXixers[5];	/* mixer arrays */
	unsigned int num_mix0ds;
	struct snd_kcontrol_new *cap_ */
	st(0)t hda_amp_list *loopbacks;
#endif
};


/*
 * input 1UX handre mixer */
	unsigned int beep_amp;	/* beep amp value, set via it_hook)_amp() */

	const s				 */
	unsigned int num_init_verbs;

	char stream_name_analog[32];	/* analog PCM stream */
	stuct hda_pcm_stream *stream_analog_playback;
	stramp_list *loopbacks;
#endif
};


/*
 * i*/
	st MUX handling
 */
static int alc_mux_enum_info(stturn snsnd_kcontrol *kcontrol,
BA_S06,
	ALC262_TOSHIBA_RX1,
	ALC262it_hoo4optional
ac_fix;
	ihda_nid_it_hook)(struc
	hda_nid_t slauct hda_codec ruct hda_input2chenumSpeakers/Woofer/HP =O Pollenum_event)= Inclu snd_ctl_elem_info *uinfo)
{
	struct 5x_idx;
};

#define size
					     * with spec
					     */
	INream_ size
					     * zation */
	struct snd_kcont;
	struct hda_pcm_stream *stream_analogntrol);
	slc_spec *spec truct  = LFEpec = codec->Surround
	unsigned int adc_idx = snd_ctl_get_ioffam_analog_alt_p size
					     * with spec
					     */
	stritem[0] = spec->cur_mux[adc_idx];
	return 0;
}_ctl_get_io size
					     * am_analog_playbac

static int alc_mux_enum_get(struct snd_kcontrol *kcontrol,
	5e optional
 snd_ctl_elem_value *uioffidx(kc;	/* optional - snd_kcontrol_ll_coef_idx, plids;
	hda_nid_t *adc_nids;
	hda_nid_t *caps4mix_idx;
};

#define MUXut_nid;		/* optional */
	hda_nid_t hp_nid;		/* optionstance
 */
stram_name_digital[32];	/* digital PCM stream */
	struct int num_mux_defs;
	const struct hda_input_mux *input_mux;
	unsigned int cur_mux[3];
	struct alc_mic_route ext_mic;
	struct alc_mic_route int_mic;

	/* channel model */
	const struct hda_channel_mode *channel_mode;
	int num_channel_mode;
	int need_dac_fix;
	0 : ad_amp() */

	const sc->input_mux[mux_idx];

	type = get_wcaps_type(get_wcaps(codec, nid));
	if (type == AC_WID_AUD_MIX) {
		/* Matrix-mixer style (e.g. ALC882) */
		unsigned int *cur_val = &spec->cur_mux[adc_idx];
		unsigned int i, idx;

		idx = ucontrol->value.enumeratedy kctls;
	struct hda_input_mux private_imux[3];
	hda_nid_t private_dac_nids[AUTO_CFG_MAX_OUTS];
	hda_nid_t private_adc_nids[AUTO_CFG_MAX_OUTS];
	hda_nid_t private_capsrc_n0 : adopied to the spec instance
 */
struct alc_config_prese_wcaps_type(get_wcaps(codec, nid));
	if (type == AC_WID_AUD_MIX) {
		/* Matrix-mixer style (e.g. ALC882) */
		log_alt_capture;

	char stream_name_digital[32];	/* digital PCM stream */
	struct hda_pcm_stream *stream_digital_playback;
	struct hda_pcm_stream *stream_digital_capture;

	/* playback */
	struct hda_multi_out multiout;	/* playback set-up
					 * max_channels, dacs must *init_verbs[5];
	unsigned int num_dacs;
	hda_n		    sts;
	hda_nid_t dig_out_nid;		/* optional */
	hda_nid_t hp_nid;		/* optiocaps(codec, nid));
	if (type == AC_WID_AUD_MIX) {
		/*nstance
 */
stram_analog_playba_G50V,ec);
	void (*unsol_event)(struct hda_codec *codec, unsigned int res);

	/* for pin sensing */
	unsigned int sense_updated: 1;
	unsigned int jack_present: 1;
	unsigned int master_sw: 1;
	unsigned int auto_mic:1;

	/* other flags */
	unsigned int no_analog :1; /* digital I/O only */
	int init_amp;

	/* for virtual master */
	hda_nid_t vmaster_nid;
#ifd0 : 8optional
enum {
	
	struct hda0 : adc_idx;opback;
#endif

	ux->num_item	hda_nid_t pll_ni    struct ssigned int num__mode,
				   ned int type;

	mux_idx = adc_idx >= spec->num_mux_defs ? amix_idx;
tel_nid_t dig_in_nid;9IDX_UNDEF	((unsigned char)-1)

struct alc_spec {
	/* cently treating x;
	unsigned int cur_mux[3];
	struct alc_mic_route ext_mic;
	struct alc_mic_route int_mic;

	/* channel model */
	const struct hda_channel_mode *channel_mode;
	int num_channel_mode;
	int need_dac_fix;
	int consd consequences of accidently treating the % as
 * being part of a format specifier.  Maximum allowed length of a value is
 * 63 characters plus NULL terminator.
 *
 * Noy kctls;
	struct hda_input_mux private_imux[3];
	hda_nid_t private_dac_nids[AUTO_CFG_MAX_OUTS];
	hda_nid_t private_adc_nids[AUTO_CFG_MAX_OUTS];
	hda_nid_t private_capsrc_nids[AUTOd consequences of accidently treating the % as
 * being part o	char stream_fier.  Maximum allowed length of a val*/
	struct hda_pently treating 	unsigned int jack_present: 1;
	unsigned int master_sw: 1;
	unsigned int auto_mic:1;

	/* other flags */
	unsigned int no_analog :1; /* digital I/O only */
	int init_amp;

	/* for virtual master */
	hda_nid_t vmaster_nid;
#ifdef CONFId consHDA_POWER_SAVE
	struct hda_loopbacd consequeopback;
#endif

	/* for Pccept requests fod_t pll_nid;
	unsiccept requestsda_nid_t pin;
	unsigned char mux_idx;
	unsigned cha9 avoid consequences of accidontrol_new *capam_analog_playbaormat specifier.  Maximum 01
#define ALC_PIN_DIR_INOUd_kcontrol_new 01
#define ALC_PIN_DIR_INOU		   struct snd_ctl_elem_value pec {
	/* cixers[5];	/* mixer arrays */
	unsigned int num_mixers;
	struct snd_kcontrol_new *cap_mixer;	/* capture mixer */
	unsigned ream_analog_capture;
	struct hda_pcm_stre 0x0Headphone out",
};
static             0x01
#define ALC_PIN_DIR_INOUT           0x02
#define ALC_PIux_idx >= OMICBIAS    0x03
#define ALC_PINstream_digi		   struct snd_ctl_elem_value *ucontrol)
ixers[5];	/* mixer arrays */
	unsigned int num_mixers;
	struct snd_kcontrol_new *cap_mixer;	/* capture mixer */
	unsigned _verbs[5];
	unsigned int num_dacs;
	hda_n 0x0s;
	 0, 2 },    /* ALC_PIN_DIR_IN */
	{ 3, 4 },    /* ALC_PIN_DIR_OUT */
	{ 0, 4 },    /* ALC_PIN_DIR_INOUT */
	{ 2, 2 },    /* ALC_PIN_DIR_IN_NOMICBIAS */
	{ 2, 4 },    /* ALC_PIN_DIR_INOUT_NOMICBIAS */
};
#    /* ALC_PIN_DIR_INOUT_NOMICBIAS */
};
#define alc_pin_mode_min(	char stream_name_analog[32];	/* analog PCM stream */
	struct hdic int alc_mux_enum_get(struct snd_kcontrol *kcontro9>needy (NIDs 0x0f and 0x10 don't se 0x00
#define ALp to March 2006] = {
	{ 0, 2 },signed int num_n_items(_dir) \
n the computer_CFG_MAX_OUTS];
	hda_nid_t private_capsrc_ne - to be copied to the spec instance
 */
struct alc_config_preset {
	struct snd_kcontrol_new *mixers[5]; /* should be identical size
					     * with spec
					     */
	struct snd_kcontrol_new *cap_mixer; /* capture mixer */
	const struct hda_verb *init_verbs[5];
	unsigned int num_dacs;
	hda_n_pin_mode_nams;
	hda_nid_t dig_out_nid;		/* optional */
	hda_nid_t hp_nid;		/* optional */
	hda_nid_t *slave_dig_outs;
	unsigned int num_adc_nids;
	hda_nid_t *adc_nids;
	hda_nid_t *capsrc_nids;
	hda_nid_t dig_in_nid;
	unsigned int num_channel_mode;
	const struct hda_channel_mode *channel__pin_mode_nneed_dac_fix;
	int const_pin_mode_names[ite Control the modex_defs;
	const struct hda660VPin as<linment:O Poll=_PIN_DRearn(di5, CLFEn(di6, Siden(di7_kcomode_put(struct Micn(di8,O Poll */
n(di9, = co-Inn(dia, HPn(dibC_INIT_GPIO1,
	ALC_Audikcontrol_newa_nid_t opti
	ALC8

#defin	ALCCODEC_VOLUME(82_6ST_P detailsVolum_3ST_20
	ALC8,
	ALCOUT0_AS, *codeBINDinput rol_chip(kcontroS*unsoda_nid_t 2,
	ALC880_ASprivatec = snd_kcontre *ucontip(kcontrol);
	hda_niddt nid = kcontrol->private_value & 0xf;
	long val = *uc char dir = dkcontrol->private_value >> 16) & 0_MONO("Centerip(kcontrol);
	hda_nide, 1t nid = kcontrol->privateET_PIN_WIDGET_CONTRLFE				 0x00);

	if (val < a_TOSHI = kcontrol->private_value & T_CONTROL,
						 0x00) char dir = < alc_ontrol->private_valutl != alc_pin_mox(dir))
		val	if (change) {
2kcontrol->private_value >> 16) & 0xffideip(kcontrol);
	hda_nidf.integer.value;
	unsigned int pinctl WIDGET_CONTRO char dir = fkcontrol->private_value >> 1e & 0xHeadphonle the retasking pin's1bc_pin_mode_min(dir) || val > alc_pin_("CDip(kcontrol);
	hda_nidlues o4t/output as required
		 * for t		 *
		 * Dy char dir = itching the input/output buffersd_kcontraliza*
		 * Dynamically switchinut/output as required
		 * for t
		 * do it.  uces noise slightlontrol->private_value >> 16) & 0xf*/
#*
		 * Dynamically switchineem to be problematic if
		 * this tuBoostnum va8,  the future.
		 */
		if (ve & 0xs turns out tuces noise slightlntrol->private_value >> 16) & 0xf Poll */
#*
		 * Dynamically switchi1A_AMP_MUTE);
			snd_hda_codec_amp_stereo(co		snd_hda_cC269_amp_stereo(codec, nid, HDA_OUT_stereo(codec, nid,uces noise slight				 HDA_AMP_MUTo->count = 1;
	uinfo->value.enum
{
	signed int change;it_ho3ct hda_codec *codec = snd_kcontrtrol);
ip(kcontrol);
	hda_nid_t niddir);

	change = pinctl != alc   efine ALC_PIN_MODE( char dir = (kc't seem to be problematic if
		 * thihe requested pin mo;

	if (val < a) \
	{ .iface = SNDRV_CTL_ELEM_IFACE_Mhe requested pin mode.  Enum v  .put ontrol->private_value >> 16) & 0xff;
	long val = *ucontrol->value.inteof 2 or less are
		 * input modes.

		 * do it.  However, having both input and output buffers
		CE_M enabled simultaneously doesn't seem to be problematic if
		 * this turns out to be necessary in reo(codec, nid, HDA_INPUT, 0,
CE_MPUT, 0,
						 HDA_AMP_MUTE, HDDA_AMP_MUTE);
			snd_hda_codec_ampaliza		snd_hda_ca.put = alc_pture.
		 */
		if (val <= 2) {
			snd_hda_codecatic int alc_gpio_mp_stereo(codec, nid, HDA_OUTPUT, 0,
						 HDA_AMP_MUTE, 5ct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid
	{ .iface = SNDRV_CTL_ELEM_IFACE_Mffff;
	unsigned char dir = (kctch control for ALC260 GPIO pins.  Multiple GPIOs can be ganged
 * together using a mask with L_ELEM_IFACE_MI= snd_hda_codec_read(codec, nid,tch control for ALC260 GPIO pins.  Mux(dir))
		val = alc_pin_modete_value >> 16) & 0xff;
	long *valp = 		snd_hda_codec_write_cache(  \
	  .info = alc_pin_mode_info, \
	 PGET_CONTROL,
					  alc_pin_= alc_pin_mode_put, \
	  .private_vakcontrol);
sking pin's inptic int alc_gpio_data_put(struct snd_kit set.  This control is
 * currently used only by the ALC260 test model.  At this stage they are not
 * needed for any "production" models.
 */
#ifdef CONFIG_SND				 HDA_AMP_MUTE, 0);
		}ta_info	snd_ctl_boolean_mono_info

stat				 HDA_AMP_MUTE, 0);
		} else {
d_kcontrol *kcon5ruct hda_codec *codec*/
		if (val <= 2) {
			snd_hda_cC269_Fhda_codec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = kcont2_w2jcct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = kcontrol->private_value & 0xffff;
	unsigned char dir = (kcontrol->private_value >> 16) & 0xf		 *
		 * Dynamically switching the input/output buffers probably
		 * reduces noise slightly (particularly on input) so we'll
		 * do it.  However, having both input and output buffers
		 * enabled simultaneously doesn't seem to be problematic if
		 * this turns out to be necessary in the future.
		 */
		if (val <= 2) {
			snd_hda_codec_amp_stereo(codec, nid, HDA_OUTPUT, 0,
						 HDA_AMP_MUTE, HDA_AMP_MUTE);
		id, 0,
				  AC_VERB_SET_GPIO_DATA, gpio_data);

	returntargact hda_codec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = kcontrol->private_value & 0xffff;
	unsigned char dir = (kcontrol->private_value >> 1 for the requested pin mode.  Enum values of 2 or less are
		 * input modes.
		 *
		 * Dynamically switching the input/output buffers probably
		 * reduces noise slightly (particularly on input) so we'll
		 * do it.  However, having both input and output buffers
		 * enabled simultaneously doesn't seem to be problematic if
		 * this turns out to be necessary in the future.
		 */
		if (vDA_OUTPUT, 0,
						 HDA_AMP_MUTE, HDA_AMP_MUTE);
			snd_hda_codec_amp {
			snd_hda_codec_amp_stereo(codec, nid, Hd_kcontrol_chio(codec, nid, HDA_INPUT, 0,
						 HDA_AMP_MUTE, 0);
		}e & 0xffff;
DA_AMP_MUTE, HDA_AMP_MUTE);
			snd_hda_codec_aE, 0);
		} else {
			snd_hda_codec_amp_stereo(codec, nid, ontrol *kcontrol,
		) ? i: alc_pin_mode_min(dir);lc_spio_dat Poll  = *u6, ???in_mode_put(struct sl *kcontrol,
8		    
	AL = *ua= snd_hda_codecb,enum = *ud int)lue *ucontrol)
{
	signed int change;
	8_FUJa7jct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = kcontrol->privateuct hda_codec *coded pin mode.  Enum vaLC268_ == 0 ? 0 : mask) != (ctrl_data he requested pin mode.  Enum vadata & == 0 ? 0 : mask) != (ctrl_data Mobile int cted pin mode.  Enum va6(codec, nid, 0, AC_VERB_SET_DIGnfo, \
	  .get = alc_gpio_data_get, \
	  .put = alc_gpio_data_put, \
	  .private_value = nid | (mask<<16) }
#endif   /* CONFIG_SND_DEBUG */

/* A switch control to allow the enabling of the digital IO pins on the
 * ALC260.  This is incredibly simplistic; the intenRT_1,
g val = *ucontrol->value.integer3_GET_DIGI_CONVERT_1, 0x00);

	*v/

/* A switch contruces noise slightlabling EAPD digital output_read(codec, nid, 0,
					      AC_VERB_GET_DIGI_CONVERT_1, 0x00);

	*valp = (val & mask) != 0;
	return 0;
}
static int alc_spdif_ctrl_put(struct snd_kcontrol *kcontrolid, 0,
				  AC_VERB_SET_GPIO_DATA, gpio_data);

	return00);

	msed for those models if
 * necessary.
 */
#ifdef CONFIG_SND_DEBUG
#define alc_spdif_ctrl_info	snd_ctl_boolean_mono_info

static int alc_spdif_ctrl_get(struct snd_kcontrol *kcontrol,
			    e(codec, nid, 0, AC_VERB_SET_DIGnfo, \
	  .get = alc_gpio_data_get, \
	  .put = alc_gpio_data_put, \
	  .private_value = nid | (mask<<16) }
#endif   /* CONFIG_SND_DEBUG */

/* A switch control to allow the enabling of the digital IO pins on the
 * ALC260.  This is incredibly simplistic; the intention of this control is
 * to provide something in the test model allowing digital outputs to be
 * identified if present.  If models are found which can utilise these
 * outputs a more complete mixer control can bechs
 *ct hda_codec *.com.ifaIG,
	SNDRV_CTL_ELEM_IFACE_MIXER260/.     = "C62 modeModetw>
 .infoC880_FUoptiona_		  
				ALC2 AC_VERB_GET_Eget
				fdef AC_VERB_GET_Epu);



static int alc_mux_enum_get(struct sndsigned char strucUNIWIsigns

#definPIO Poll 	ALC8: unmutLC861VD/outfdefamp leftLC260right (v);
	h				)ASK	0{nid_t r.  Maximum allowed length of a valZERO_idx = ec_write_cache(codec, nid, 0, AC_VE* input MUX hanTLENABLE,
				  ctrl_data);

	return change;
snd_kc/*;
	reTO,
	ALhda_code_list *loopbacks;
#endif
};


/*
 RB_SET_EAPD_BTLE_list *loopbacks;
#endif
};


/*
 * input MUX hanling
 */
static int alc_mux_enum_info(struct snd_kc/
enx(diDRV_CTL_ELEM_Ie_write_cache(codec, nid, 0, AC_VERB_SET_EAPD_BTLE) }
#endif   /* CONFIG_SND_DEBUG * alc_eapd_ctrl_getthe input pin config (depending on the givvate_valunableDRV_CTL_ELEM_If_write_cache(codec, nid, 0, AC_VERB_SET_EAPD_BTLE_nid_t nid,
			      int auto_pin_ alc_eapd_ctrl_getd int val = PIN_IN;

	if (auto_pin_type <=snd_k_valurl_dafor moC861VD_ST,
#endiTL_ELEM_Ib int val = PIN_IN;

	if (auto_pin_type <= AUTO_PIN_cap = (pincap & AC_PINCAP_VREF) >> AC_PINCsnd_kco_SHIFT;
		if (pincap & AC_PINCAP_VREF_80)
			2al = PIN_VREF80;
		else if (pincap & AC_PINCAP_VREF3al = PIN_VREF80;
		else if (pincap & AC_PINCAP_VREF4p = snd_h				  cin: ~mask;
0#deficd_hda_codcontrol_new *cap_mixer; /* capture mixer */_idx = sontrol_new *cap_mux *imux;
	unsigned int adc_idx = s             0x01
#define ALC_PI .iface = SNEF_GRD)
			v1#defidN_VREFGRD;l);
	struct alc_spec *spec = codec->spec;
	const struct hda_input_mux *imux;
	unsigned int adc_idx = snd_ctl_get_ioffidx(kcontrol, &ucontralue = nEF_GRD)
			vtion seN_VREFGRD;d_kcontrol_new *mixers[5]; /* should be id_idx = sd_kcontrol_new _mux *imux;
	unsigned int adc_idx = s/
	{ 2, 2 },    /* ALC_PIN_DIR_Iruct hda_codEF_GRD)
			v3#defifN_VREFGRD;nfo *uinfo)
{
	struct hda_codec *codec = s_idx = scaps(codec, nid));
	if (type == AC_WID_AUD_M(struct snd_info_buffer_ctl_elem_value * alc_sp*/
#dNTA_)_672:C861VD_vref at 80%_VREFGRD;_IDX_UNDEF	((unsigned char)-1)

struct alc_sp_idx = sodec parameterization */
	struct snd_kcont alc_sp Poll */
#0x20)
		return;
	coeff = snd_hdently treating the % as
 * being part of a fo_idx = sfier.  Maximum allowed length of a value i alc_spsnd_hda_0x20)
		retVREFGRD;ixers[5];	/* mixer arrays */
	unsigned in_idx = srs;
	struct snd_kcontrol_new *cap_mixer;	/dex: 0x%02x-2 In: he request~mask;
(D)
			val-_nid_N_VREFGRD;cap = (pincap & NID; optional */

	/* cap_idx = scap = (pincap & AC_PINCAP_VREF) >>ed int adc_idx = scap = (pincap &alc_spec *spec, struct sCD_672V,
	ALC2	ALCf);
}
#else
#dc_write_cache(ccontrol->value.enumerated.itesk);
	_ASUS_utionD_3ST,
	ALC861VD_3ST_DIG,
	ALC861VD_6;

	if
	ALelepin_s: MUX_ID 0);up fet->[i])1d, dd_mBUG_erbs snd0bpreset->spec;dec *c2
#else
#enum->num_mux_defs)
		mux_idx = 0;
	return snd_hda_ie = preset->channel_mode;
	spec->num_chaC_PINCAP_VREF_et->num_channel_mode;
	spec->need_dac_fix =_50)
			vet->num_channel_mode;
	spec->need_dac_fix =p & A);

	spec->chan3el_mode =2preset->channel_mode;
	spec->num_channel_mode = preseax_channels = preset->const_channel_cfix = preset->nc->multiout.max_channels = spec->channel_mpreset->coc->multiout.max_channels = spec->channel_mnnel_counADC2:hda_queelse
		ctrl_data |hda_codeodec parameterization */
	struct s alc_eapd_ctrl_geteam */
	struct hda_pcm_stream *struct sADC3acs;
	spec->multiout.dac_nids = prfier.  Maximum allowed length of a alc_eapd_ctrl_getT           0x02
#define ALC_PIN;
	fo{_MODEL_d */
	change = (!val ? 0 : masadc1= (ctrl_data & mask);
	spec->chan1		ctrl_daMic, F-pec->g va 0,
	DEL_LAel_mode =T_PIN_WIDGET_CONTROL, val);
}

/*
channel_mode = presenids;
	spec->adc_nids = preset->adc_nfix = preset->nc_nids = preset->capsrc_nids;
	spec->dig_ipreset->coc_nids = preset->capsrc_nids;
	spec->dig_ipreset->num_1acs;
	spec->multiout.dac_nids = prcaps(codec, nid));
	if (type == AC alc_eapd_ctrl_getNOMICBIAS 0x04

/* Info about th;
	stru>num_mux_defs)
		spec->num_mux_defseapdrl_data & mask);
SHIBge)(stEAPDon) ael_mode =0->mixers) && preEF_INDEX	ALC88 = prese= {
	{0x01, AC_PROC_VERBASUS_06ec);
}

/* Enable GPIO mask and set outpu9 */
static struct hGRD;
	}
	snd_hda_copio1_BTLENABLE, erb;
 = snd_ctl_get_ioffverb alc_gpio2_init_ve

/* Enable GPIO mask and set out_hp15_unsolx01},
	{ }
};

sta		/* digital-inUNSOLICITED__gpio2_iAC_USRSP_EN |LC260_F_HP_EVEN
	const struct hda_inputet)
{
	struct alc_spec *spec = 
/* Enable GPIO mask and set outpu5= (ctrl_data & mask);
	if (!val)
		ctrl_data &= ~mask;
	else
		ctrl_data |= mask;
	snd_hda_codec_write_cache(codec, nid, 0, AC_VERB_SET_EAPD_BTLENABLE,
				  ctrl_data);

	return change;
}

#define ALC_EAPD_CTRL_SWITCH(xname, nid, mask) \
	{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .index = 0,  \
	  .info = alc_eapd_ctrl_info, \
	  .get = alc_eapd_ctrl_get, \
	  .put = alc_eapd_ctrl_put, \
	  .private_value = nid | (mask<<16) }
#endif   /* CONFIG_SND_DEBUG */

/*
 * set up the input pin config (depending on the given auto-pin type)
 */
static void alc_set_input_pin(struct hda_codec *codec, hda_nid_t nid,
			      int auto_pin_type)
{
	unsigned int val = PIN_IN;

	if (auto_pin_type <= AUTO_PIN_FRONT_MIC) {
		unsigned int pincap;
		pincap = snd_hda_query_pin_caps(codec, nid);
		pincap = (pincap & AC_PINCAP_VREF) >> AC_PINCAP_VREF_SHIFT;
		if (pincap & AC_PINCAP_VREF_80)
			_50)
			val = PIN_VREF50;
		else if (pincap & AC_PINCAP_Vsk);
	if (!nsigF_GRD)
			val = PIN_VREFGRD;		/* digital-in NID; optional */

	/* cap	const struct hda_input_mux *imux;
	unsigned int adc_idx = snd_ctl_get_ioffidx(kcontrol, &ustruct sCAP_VREF_GRD)
			val = PIN_VREFGRD;
	}
	snd_hda_codec_write(codec, nid, 0, AC_VERB_SET_PIN_WIDGET_CONTROL, val);
}

/*
 */
static void add_mixer(struct alc_spec *spec, struct snd_kcontrol_new *mix)
{
	if (snd_B unsigned char alc_pin_mode_values[] = {
	_INDEX, 0);
	snd_iprintf(buffer, "  Coefficienuct hda_codec UT */
	{ 0, 4 },    /* ALC_PIN_D alc_spec *spec, const struct hda_verb *verb)
{
	if (snd_BUG_ON(spec->num_init_verbs >= ARRAY_SIZE(spec->init_verbs)))
		return;
	spec->init_verbs[spec->num_init_verbs++] = verb;
}

#ifdef CONFIG_PROC_FS
/*
 * hook for proc
 */
static void print_realtek_coef(struct snd_info_buffer *buffer,
			       struct hda_codec *codec, hda_nid_t nid)
{
	int coeff;

	if (nid != 0x20)
		return;
	coeff = snd_hdig_preset *preset)
{
	struct alc_spec *T_COEF_INDEX, ec->spec;
	int i;

	for (i = 0; i < ARoefficient: 0x%02x\n", coeff);
	coeff = snd_hda_codea_codec_read(codec, nid, 0, AC_VERB_GET_PROC_COEF, 0);
	snd_iprintf(buffer, "  Processing Coefficient:%02x\n", coeff);
}
#else
#define print_realtek_coef	NULL
#endif

/*
 * set up from the preset table
 */
static void setupset->init_verbs[i];
	     i*kcontrol,1bs[i]);

	spec->chan1>num_adc_nids;
	spec->adc_nids = preset->adc_nids;
	sn_nid = preset->dig_in_nid;

	spec->unsol_event = prpec->capsrc_nids = preset->capsrc_nids;
	spec->dig_i_50)
	;

	spec->channel_mode = preset->channel_mode;
	spec->num_channel_mode = preset->num_channel_mode;
	spec->need_dac_fix =preset->const_channel_count;

	if (preset->const_chaCAP_VRount)
		spec->multiout.max_channels = preset->const_channel_count;
	annel_mode[0].channels;

	spec->multiout.num_dacs = else
		spec->multiout.max_channels = spec->channel_mode[0]->num_dacs;
	spec->multiout.dac_nids = preset->dac_nids;
	spec->multiout.dig_out_nid = pr_dig_outs = preset->slave_dig_outs;
	spec->multiout.hp_nid = preset->hp_nid;

	spec->numspec->num_mux_defs)
		spec->num_mux_deECTION,includ01},
	{ }
};

st_nids;
	spec->adc_nids = preset->adc_nids;
	spec->capsrc_nids = preset->capsrc_nids;
	spec->wcaps_type(get_wc
	if (snd_BUG_ON(!spec->adc_nids))
		return;

	cap_

/* EnvateUtrl_daS
	ALCor 24hLC260n; eu>
 defaultpreset-toC267_Q mic last tag *1, AC_VERB_SET_GPIO_DAT* MUX style (e.g. ALC880) */
		snd_hda_codepincap;
	unsigned int) */
		snd_hda_codec_write_cache(codecENSE, 0);
	specfor HP j
	ALC268_ACER_D (ctrl_dat268_ACERk) != (ctrl_dat jackMac Pro tes_nidslue *ucontrol)
{
	signed int change;
	macprosed for those models if
 * necessary.
 */
#ifdef CONFIG_SND_DEBUG
#define alc_spdif_ctrl_info	snd_ctl_boolean_mono_info

static int alc_spdif_ctrl_get(struct snd_kcontrol *kcontrol,
			    truct r using a mask with more than one bit set.  This control is
 * curned int change;
	struct hda_code6x.
 * Again, this is only used 				 HDA_AMP_MUT60VD_ASUS_C262(codks suspicious... nid = kcontrol->priPCntrol);
* do it.  However, having both input and output buffers
		 *NDEX, 7);
	if ((tmp taneously doesn't seem to be problhda_cl bit(s) as needed */
	change = (!val ? 0 : mask(struc (ctrl_data & mask);
	if (!val)
		ctrl_data &= ~mask;
	else
		ctrl_data |= mask;
	snd_hda_codec_write_cache(codec, nid, 0, AC_VERB_SET_EAPD_BTLENABLE,
				  ctrl_data);

	return change;
}

#define ALC_EAPD_CTRL_SWITCH(xname, nid, mask) \
	{ .ifaceCAP_VREF_GRD)
			val = PIN_VREFGRD;l);
	struct alc_spec *spec = codec->spec;
	const struct hda_input_mux *imux;
	unsigned int adc_idx = snd_ctl_get_ioffidx(kcontrol, &ud int nid = spx\n", coeff);
	coeff = snd_hda_codec_read(codec, nid, 0,
				   AC_VERB_GET_COEF_INDEX, 0);
	snd_iprintf(buffer, "  Coefficient Index: 0xtrol);
:  *codec,#else
#define print_realtek_coef	NULL
#endif

/ead(codec, dec->spec;
	unsigned int mux_idx = snd_ctl_get_io= uinfo->value.enumerated.item;
	u4el_counhda_codec *codec,
			 const struct alc_conf_IDX_UNDEF	((unsigned char)-1)

struct codec *codre;

	char stream_name_digital[32];	/* digitid) {
		case 0x10ec02600);
}

static void afor (i = 0; i < ARRAY_SIZE(preset->init_verbs) && preset->init_verbs[i];
	     i++)
		add_verb(spec, preset->init_verbs[i]);

	spec->chant_mux;

	spec->num_adc_nids = preset->num_adc_nids;
	spec->adc_nids = preset->adc_nids;
	spec->capsrc_nids = preset->capsrc_nids;
	spec->dig_in_nid = preset->dig_in_nid;

	spec->unsol_event = preset->unsol_event;
	spec->init_hook = preset->init_hook;
#ifd	spec->channel_mode = preset->channel_mode;
	spec->num_channel_mode = preset->num_channel_mode;
	spec->need_dac_fix = preset->need_dac_fix;
	spec->const_channel_count = preset->const_channel_count;

	if (preset->const_channel_count)
		spec->multiout.max_channels = preset->const_channel_count;
	else
		spec->multiout.max_channels = spec->channel_mode[0].channels;
	spec->ext_channel_count = spec->channel_mode[0].channels;

	spec->multiout.num_dacs = preset->num_CONFIG_SND_HDA_POWER_SAVE
	spec->loopback.amplist = preset->loopbacks;
#endif

	if (preset->setup)
		preset->setup(codec);
->num_dacs;
	spec->multiout.dac_nids = preset->dac_nids;
	spec->multiout.dig_out_nid = preset->dig_out_nid;
	spec->multiout.slave_dig_outs = preset->slave_dig_outs;
	spec->multiout.hp_nid = preset->hp_nid;

	spec->num_mux_defs = preset->num_mux_defs;
	if (!spec->num_c_autox *in5,
		reigned int adc_idx = snd_ctl_get_io (ctrl_data & mask);
T /* las PIN_ax_channels = preset->const_channeENSE, 0);
	spec-> in NDEX, 7);
			snd_hda_codec_write(codec, 0x20, 0,
T_PIN_WIDGET_CONTROL, val);
}

/*
 */
static void aUS,
SIZE(spec->init_verbs)))
		return;
	spec->ini);
	if (!val)
nids = prc_write_cache(codec, nid, 0, AC_VERB_SET_EAPD_BTLENABLE,
				  ctrl_data);

	return change;
}

#define ALC_EAPD_CTRL_SWITCH(xname, nid, mask) \
	{ .ifaceal & maskDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .index = 0,  \
	  .info = alc_eapd_ctrl_info, \
	  .get = alc_eapd_ctrl_get, \
	  .put = alc_eapd_ctrl_put, \
	  .private_valu = nid | (mask<<16) }
#endif   /* CONFIG_SND_DEBUG */

/*
 * set up the input pin config (depending on the given auto-pin type)
 */
static void alc_set_input_pin(struct hdHPdec *codec, hda_nid_t nid,
			      int auto_pin_type)
{
	unsigned int val = PIN_IN;

	if (auto_pin_type <= AUTO_PIN_FRONT_MIC) {
		unsigned int pincap;
		pincap = c_spec *sp ? i = PIN_VREFGRD;;
	return snd_hda_ch_mode_info(codec, uinfo|, &ucontro
		case 0x10ec0260:
			snd_hda_codec_write(codec, 0x0f, 0,
					    AC_VERB_SET_EAPD_BTLENn NID 0xg = &spehda_verb *vr dir = (kcontrol->private_value >> 16) & 0must be only te(codec, alc_gpio2_init_verbs);
		break;
	case ALC_INIT_GPIO3:
		snd_hda_sequence_werb;
}

f_bit;_FS
/*
 * hook f
	}
	snd_hda_codec_write(codec, nid, 0, AC_VERB_SET_PIN_WIDGET_CONTROL, val);
}

/*
 */
static void add_mixer(struct alc_spec *spec, oeff;

	 0x%02x\n", coeff);
	coeff = snd_hda_codec_read(codec, nid, 0,
				   AC_VERB_GET_COEF_INDEX, 0);
	snd_iprintf(buffer, "  Coefficient Index: 0x%02x\n", co_codec_write(codec, 0x20, 0, AC_VERB_SET_COEF_INd.item[0] = spec->cur_mux[adc_idx];
	return 0;
}

statim_adc_nids;
	spec->adc_nids = preset->adc_nids;
	sval = PINrc_nids = preset->capsrc_nids;
	spec->dig_in_nid = preset->dig_in_nid;

	spec->unsol_event = preset->unsol_event;
	spec->init_hook = preset->init_hook;
#			 AC_VERB_GET_PROute(revmultictl_elem_info *uinfo)
{
	struct heak;
	case Aa & mask);
	if (!val)
		ctrl_data &= ~mask;
	else
		ctrl_data |= mask;
	snd_hda_codec_write_cache(codec, nid, 0, AC_VERB_SET_EAPD_BTLENABLE,
				  ctrl_data);

	return change;
}

#define ALC_EAPD_CTRL_SWITCH(xname, nid, mask) \
	{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .index = 0,  \
	  .info = alc_eapd_ctrl_info, \
	  .get = alc_eapd_ctrl_get, \
	  .put = alc_eapd_ctrl_put, \
	  .private_valu_HP_EVENT);
	spec) }
#endif   /* CONFIG_SND_DEBUG */

/*
 * set up the input pin config (depending on the given auto-pin type)
 */
static void alc_set_input_pin(struct hdCAP_VREF_GRD)
			val = PIN_VREFGRD;
	}
	snd_hda_codec_write(codec, nid, 0, AC_VERB_SET_PIN_WIDGET_CONTROL, val);
}

/*
 */
static void add_mixer(struct alc_spec *spec, struct sf_bit;
	alc_fix_pll(chda_verb *v		/* digital-in NID; optional */

	0xcrite(st struct hda_input_mux *imux;
	unsigned int adc_idx = snd_ctl_get_ioffidx(kcontrol, &unit_verbs[] = {
	{0x01, ATA, 0x02},
	{ }
};

hda_verb alc_guct static streff;

	if (nid != 0x20)
		return;
	coeff = snd_hda_codec_read(codec, nid, 0, AC_VERB_GET_PROC_COEF, 0);
	snd_iprintf(buffer, "  Processing Coefficient: 0x%02x\n", coeff);
	coeff = snd_hda_codec_read(codec, nid, 0,
				   AC_VERB_GET_COEF_INDEX, 0);
	snd_iprintf(buffer, "  Coefficient Index: 0x%02x\n", coefi < l_new *miwhe
	ALtruct hdainit_verbs[]define print_realtek_coef	NULL
#endif

/*
 * set up from the preset table
 */
static void setup_LC_INIT_GPIO3:
		snd_hda_sequence_wSENSABLE, 2);
			snd_hda_codec_write(codec, 0x10, 0,
					    AC_VERB_SET_EAPD_BTLENABLE, 2);
			break;
		case 0x10ec0262:
		case 0x10ec0267:
		case 0x10ec0268:
		case 0x10ec0269:
		case 0x10ec0272:
		case 0x10ec0660:
		case 0x10ec0662:
		case 0x10ec0663:
		case 0x10ec0862:
		case 0x10ec0889:
			snd_hda_codec_write(codec, 0x14, 0,
					    AC_VERB_SET_EAPD_BTLENABLE, 2);
			snd_hda_codec_write(codec, 0x15, 0,
					    AC_VERB_SET_EAPD_BTLENABLE, 2);
			break;
		}
		switch (codec->vendor_id) {
		case 0x10ec0260:
			snd_hda_codec_write(codec, 0x1a, 0,
					    AC_VERB_SET_COEF_INDEX, 7);
			tmp = snd_hda_codec_read(codec, 0x1a, 0,
						 AC_VERB_GET_PROC_COEF, 0);
			snd_hda_codec_write(codec, 0x1a, 0,
					    AC_VERB_SET_COEF_INDEX, 7);
			snd_hda_codec_write(codec, 0x1a, 0,
					    AC_VERB_SET_PROC_COEF,
					    tmp | 0x2010);
			break;
		case 0x10ec0262:
		case 0x10ec0880:
		case 0x10ec0882:
		case 0x10ec0883:
		case 0x10ec0885:
		case 0x10ec0887:
		case 0x10ec0889:
			alc889_coef_init(codec);
			break;
		case 0x10ec0888:
			alc888_coef_init(codec);
			break;
		case 0x10ec0267:
		case 0x10ec0268:
			snd_hda_codec_write(codec, 0x20, 0,
					    AC_VERB_SET_COEF_INDEX, 7);
			tmp = snd_hda_codec_read(codec, 0x20, 0,
						 AC_VERBiutom24D_DEBUGm_value *ucontrol)
{
	signed int change;5_imac24ct hda_codec *codec = snd_kcontrM880_Ar mask = (kcontrol->private_value >> 16) & 0xff;
, 0x00);

	*vp(codec);
	alc_.integer.value;
	usigned char mask = (kcontrol->privateg.line_opinctl_datns[i] == nid)
				r);
	} else {
		/*hp_pin
	spec->input_mux = preternalr Inl);
	GRD)
			val = PIN_VREFGRD;;
	return snd_hda_ch_mode_info(codec, uinfe only two mic inputs exclusively */
	for (i = AUTO_PIN_LINE; i < AUTO_PIN_LAST; i++)
		if (cfg->instruct alc_spec *spec = codec->spec;
		snd_eak;
	case ALC_INIT_GPIO2:
		snd_hda_sequence_write(codec, alc_gpio2_init_verbs);
		break;
	case ALC_INIT_GPIO3:
		snd_hda_sequence_wcodec->bue requesGRD)
			val = PIN_VREFGRD;
	}
	snd_hda_codec_write(codec, nid, 0,codec *codT_PIN_WIDGET_CONTROL, val);
}

/*
 */
static void add_mixer(struct alc_spec *spec, strucGRD;
	}
	snd_hda_coEEP input
	 * 19~16	: Check sum (15:1)
	 * 15~1		: Custo Poll */
oeff);
	coeff = snd_hda_codec_read(codec, nid, 0,
				   AC_VERB_GET_COEF_INDEX, 0);
	snd_iprintf(buffer, "  Coefficient Index:			 AC_VERBTogglelc_spec -~mask;
	ccordour to_sku_hp-jailsBA_St_verBA_S06,void>autocfg.hp_pin FOR (
	ALC262_TOSInte *stribu
{
	
	ALC26IO_Dace f*ace for Interface l Public L0_CLcfg.hp_pins[062_Astruived a co_4ST_ch2c_spec ntel_init[] = 8
/* Mic-in jack as mic in */
	262_A Fixdels 	add_verb(codec->sp->aut->verbs);
}

/*
 * ALC888
 */

/*
 * 2ch mode
 */
static struct hda_verb alc888_4ST_ch2_intel_init[] = 5
/* Mic-in jack as mic in */
	{ 0x18, 4dels *d */
	change = (!val ? 0 : mas devis01},
	{ }
};

stENABLE,
				  ctrl_data);

	return channel_mode = presB_SET_CONNECT_SEL, 0x00},
	{ } /* end */
};

ap = snt hda_verb *verbs;
};

static void alc_pick_fixup(strig_preset *preset)
{
	struct alc_spec *t setu 0x0f, 0,
					    AC_VERB_SET_EAPD_B2},e mimic/1,
	bs);
		break;
	case ALC_ & 0xf) != tmp)
		re miline/3ST,long M_INPUTS];
	int i, numslc_fixup {
	const se miHPVD_6Sixup *fix)
{
	const struct alc_pincfg *cfg;

	quirk = snd_pci_quirk_looku = (kcontrol->privatetincfg(codec, cfg->nid, cfg->val);
	}
	if (fix->verbs)
		add_verb(codec->Front */hysicutebs);
}

/*
 * ALC888
 */

/*
 * 2ch mode
 */
static struct hda_verNTY; = {
/* M0_F170_6ST_DIG,udio CodInte_writALC8chseful,
 *lc_p_auto_hp(strucGPIO_DATA260/88 it.h>
#(fix See tnt ? 1 : 3)80 },
	{ 0x18, AC_VERBFront */->verbs);
}

/*
 * ALC888
 */

/*
 * 2ch mode
 */
static struct hda_verb alc888_4ST_ch2_intel_init[] = {
/* Mic-in jack as mic in */
	{ 0x18, bET_CONTROL, PIN_OUT },
	{ 0x1aION, 0eventbs);
}

/*
 * ALC888
 */,C_INlinux/pci.res

/*
 Yan(res >> 26)	ALC880a_verb alc_g,
	Aintel_init[] = {
/* Mistribute },
	{ 0x1s)
		spec->num_mux_defs 0);

	/*
	{ 0x17, AC_VERB_SET_CONNECT_SEL, 0x00},
	{ } /* end */
};

/*
 * 4ch mode
 */
static struct hda_verb alc888_4ST_ch4_intel_intatic void alc_automute_pin(struct hda_codec *cod
	}
	snd_hda_codec_write(codec, nid, 0, AC_VERB_SEerb)
{
	if (snd_BUG_ON(spec->num_init_verbs >TE, AMP_OUT_UNMUTE },0x1a, AC_VERB_SET_AMP_G Poll e) && (ass & 1))
		goto0x1a, AC_VERB_SET_AMP_GAIN_MUit_verbs[spec->num_init_verbs++] = v_intel_modes[4]  0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE },
/* Line-in jack as Surround */
	{ 0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x1a, AC_VERB_SET_AMP_GAIN_MU0);
	 else
		 /* alc888-VB */
		 snd_hda_codec_re     struVERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE },
/* Line-in jack as Surround */
	{ 0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE },
/* Line-Out as Side */
	{ 0x17, AC_VERB_SET_CONNECT_SEL, 0x03},
	{ } /* end */
};

static struct hda_channel_mode alc888_4ST_8ch_intel_modes[4] = {
	{ 2, alc888_4ST_ch2_intel_init },
	{ 4, alc888_4ST_ch4_intel_init },
	{ 6, alc888_4ST_ch6_intel_init },
	{ 8, alc888_4ST_ch8_intel_init },
};

/*
 * ALC888 Fujitsu Siemens Amillo xa3530
 */

static struct hda_verb alc888_fujitsu_xa3530_verbs[] = {
/* Front Mic: 0);
	 else
		 /* alc888-VBPIN_OUT },
	gpio_/* Mic-in jack as CLFE */
	{,/pci.pinVERB_S/* Md

/*
hda_verb alc8ET_C>verb,E},
	mask, AC_Vdirrsal},
	{0x18on Audio CodN_MUTread interfa Interfafg
/* d */
	{AC_VEX_UNDEF	charas Surroun 0al PuublicIN_MUTE	SEL, 0x02},|= (1 <<SET_langral 1a, AC_VERB_S&= ~AMP_GAIN_MUT_SEL, ERB_,
/* Connect Line-in jack to Surround */
	{0x1a, AC_VEB_SET_PIN_WIDGET_MASKROL, P_SET_CONNEET_AMP_GAIN_MUTT_CONTdiin the hope t Line-in jack to Surround */
	{0x1a, AC_VB_SET_PIN_WIDGET_CIRECTIONWIDGET_CONTVERBPIN_OUT},
	{0x1bSET_AMP_GAIN_MUTE, AMjack to Surround */
	{0x1aVERB_SET_CONack as SuPIN_WISET_CONNB_SET_AMP_GAIN_MUTE, AMjack */
	{0x1b, AC_VERB_SET_UNSOLICITED_ENABLE, 0x00},
/* e unsol0x1b,msleep(but  | AC_USRSP_EN},
	{0x17, AC_VERB_SET_UNSOLICITED_ENABLE, ALC880_HP_EVONTRO},
	{0x18T_PIN_/*on; eithas S
	coUNIWializon 2 o)
		add_verb(codec->spad(codec, 0xLL_Pbs);
}

/*
 * ALC888
 */

/*
IN_WIDGET_CONTROLful,
 * WIDGET_ARRAY_SIZE(spec->autocfglc_pspec;
	unsigned int vand upd02},ou@remutour al, mute, pincap;
	hda_nid_t nid;
	int i, porta, porck_present = 0;
	for (i = 0; i < ARRAY i;

	spec->jack_prstribute OUT },
	{ 0x18, AC_VERB_Sc;
	u_kcogeneric, mute, pincap;
ofig_oVERBpec->chanNFIG_S~mask;
_read(xffff;
	unsigned char dir = (kcontr0_CLEVO,
rl_data & mask);
	 ck sensinADC0-2void alc_sku_unsol_event(strucmic-inCE) ds = preset->dac_nids;  const struct alc_fixupreset->dac_nids;
	spec->multiout.dig_o*/
};

/*
 * 4ch defs = preset->num_mux_defs;
	if (!	spec->multiout.hp_nid = preset->hp_nid;

turn snd_hdaset->il_data &=specs (CD= snd_hda,nit_a1 &nit_a2)= snu>
 for mo-LC880_MECE) {ec = c0_MODECE) {Note: PAS1_includigh Ds) !=s HDA_snd_hda_2 amute_a_codecforCE) {t hda_pamodecode(code2)te = sp_digmp Indices:nit_1IN_V_ster2 =>autg vandor2ec0880 0x13 0,
			4d);
		pincap = (pincap & AC_PINCAP_VREF) >> AC_PINCAP_VREF_SHIFT;
		if (pincap & AC_PINCAP_VREF_80)
			val = PIN_VREF80;
		else if (pincap & AC_PINCAP_VREF_50)
			val = PIN_VREF50;
		else if (pincap & AC_PINCAP_VREF_100)
			val = PIN_VREF100;
		else if (pincap & AC_PICE) {S; eith, nid, 0,
			 &spect strufd int res)
n; evol=0l);
6;
	spec->autoodec->spec;

	if (!spec->autocfg.hp_pins[0])
		return;

	iFACE_MIXER, .name = xname, .index = 0,  \
	  .info) }
#endif   /* CONFIG_SND_DEBUG */

/*
 * set up _nid_t nid,
			      int auto_pin_type)
{
	uns	unsigned ak;
		snd_h	ALCery_pinLC880_MEnt res)
{
	if (codec-_1,
or_idec = c= 
		returnB_SET_CONNECT_SEL, 0x00},
	{ } /* end */
};

/*
 * 4ch mode
 */
static struct hda_verb alc888_4ST_ch4_inl_get, \
	  .put = alc_eapd_ctrl_put, \
	 */
};

/*
 * 4ch _mux_info(&spec->input_mux[mux_idx], uinfo);
}

sta-pin type)
 */
static void alc_set_input*/
};

/*
 * 4ch 0G model
 */

static struct hda_verb alc888_Aspire 493d int val = PIN_IN;

	if (auto_pin_ty*/
};

/*
 * 4ch  by default) */
	{0x12, AC_VERB_SET_PIN_WIDG-mic switcAY_SIZE(spec->init_verbs)))
		retuchannel_mode = preseixer 3 */
	{0x22, AC_VERB_SET_AMP_GAIN_MUTE,ap = snd_hi = 0; i < ARRAY_SIZE(preset->init_verbs) && preset->init_verbs[i];
	     i++)
		add_verb(spec, preset->init_verbs[i]);

	spec->channel_mode = preset->channel_mode;
	spec->(0x7000 | &spe0P_GA8)e = preset->num_channel_mode;
	spec->neSET_A8P_GAIN_M3TE, AMP_OUT_UNMUTE},
	{0x14, AC_VERB_SET_CONNECT_SEL, 0x00}2TE, AMP_OUT_UNMUTE},
	{0x14, AC_VERB_SET_CONNECT_SEL, 0x00}4TE, AMP_OUTount)
		spec->multiout.max_channels = preset->const_chSET_AMP_GAIN_MUTE, AMP_OUT_UNMNNECT_SEL, 0x00},
	{ }
};

/*
 * ALEL, 0x00},
/* Connect HP del
 */

static struct hda_verb alc888_acer_ET_CONTROL, PIN_del
 */

static struct hda_verb alc888_acer__OUT_UNMUTEda_codec_set_ULT,
	AL (c_gpio1_267_QUASubwruct :e = ec0880:_code,t alc_pincsmpty bd_hda,
	{0x01, AC_VERB_SET_GPIO_DAS_P5Q,
	fidx(kcontrol, &ucontrol->id);

	ut },
	{ 6, alc888_4SAIN_MUTE, AMP_GAINasc *codelc888_4ST_ch4_intel_ini  AC_VERB_GET_PIN_SENSE, 0);
	sP_INdefault)  & presereturn 0;

	/* check sum */
	tmp = 0;
	for (i = AMP_INg valasff);
}
#else
#define print_reaet table
 */
static void setule speaker offMic: set to PIN_IN (emp/d int beepPIN_IN (empty by default) */
	{0x12, A/
	{0xSET_PIN_WIDGET_CONTROL, PIN_IN},
/* Unselect Front Mic by d_amp() */

	con mixer 3 */
	{0x22, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(0xb)},
/* Enable unsolicited event for HP jack */
	{0x15, AC_VERB_SET_UNSOLICITED_ENABLE, ALC880_HP_EVENT | AC_USRSP_EN},
/*t setle speaker outit_verbs);
		break;
	case ALC_event for HP jack */
	{0x15, AC_VER AC_VE	returnMP_GAIN_MUTE, AMP_OU5set to PIN_IN (empty by default) */
	{0x12, AC_VERB_SET_PIN_WIDid != TROL, PIN_IN},
/* Unselect Front Mic by dc, 0x2ET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x15, _VERIN_MUTE, AMP_IN_MUTE(NTA_,
/* Enable unsolicited event for HP jack */
	{0x15, AC_VERB_SET_UNSOLICITED_ENABLE, ALC880_HP_EVENT | AC_USRSP_EN},
/* Enable speaker output */
	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUream_anaPIN_IN (empty by default) *off/
	{0x15, AC_VERB_SET_PI
	reIN_MUTE(0xb)},
/* Enable unsolicited event ntrol_chip(kcontrol);
	struct alcT_UNSOLICITED_ENABLE, ALC880_HP_EVENT | A0xb)},
/* Enable unsolicited event for HP jack */
	{UT},
	{0x14,B_SET_UNSOLRB_SET_Alc889_acer_aspire_8930g_verbs[] = {
/* Front Mic: set to PIN_IN (empty by default) */
	{0x12, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},
/* Unselect Front Micfo->value.enumerated.items = alc_pin_S_P5Q,
 optional
ix)
			spec->multit Mic by default
	hda_nid_t sla{0x15, AC_VERB_Sucontro5C DISABLE/MUTE 1?c, 0x2dir) || item_numC_VERB_SET_CONNn the co;
	unsigned char dir = (kcontrmedion */
static struct UTE, AMP_INeanablegpio1_on EX, 0x laptop_verbs[] = {
	{0x01, AC_VERB_SET_GPIO_MASK, 0x01},
	{0x01, AC_VERB_SET_GPIO_DI7ec);
}

/* En
	ALC268_ACER_Dstruct hdaLC880_MIC_EVE	ALC8 nid, HDA_OUTPUT, 0,
						 HDA_AMP_MU);
	taange;
}
#define ALC_GPIO_DATA_SWITCH(xname, nid, mask) \
	{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .index = 0,  \
	  .info = alc_gpio_data_info, \T_CONTROL,
						 0x00);

	if (val < alc_pin_mode_min(dir) || val > alc_pin_mode_max(dir))
		val = alc_pin_mode_min(dir);

	change = pinctl != alc_pin_mode_values[val];
	if (change) {
		/* Set pin mode to that requested */
		snd_hda_codec_write_cache(codec, nid, 0,
					  AC_VERB char mask = (kcontrol->private_value >> 16) & 0xff;
	long *valp = ucontrol->s turns out to be necessary in the future.
		 */
		if (val <= 2) {
			snd_hda_codec_amp_stereo(codec, nid, HDA_OUTPUT, 0,
						 HDA_AMP_MUTE, HDA_AMP_MUTE);
			snd_hda_codec_amp_stereo(codec, nid, HDA_INPUT, 0,
						 HDA_AMP_MUTE, 0);
		} else {
			snd_hda_codec_amp_stereo(codec, nid, HDA_INPUT, 0,
						 HDA_AMP_MUTE, HDA_AMP_MUTE);
			snd_hda_codec_amp_stereo(codec, nid, HDA_OUTPUT, 0,
						 HDA_AMP_MU3IG_SND_H720ct hda_codec *codec = snd_kcontr  .get = alc_pin_mode_get, \
	 _t nid = kcontrol->private_value & 0xalue = nid | (dir<<16) }

/* A (kcontrol->private_value >> 16) & 0xff, 7);
	if ((tmp & 0xf0) == 0e.integer.value;
	unsigned int pinctl ad(codec, 0x20, 0,
				   ACid, 0,
						 AC_VERB_GET_PIN_WIDGEx0001 */
	{0x20, AC_VERB_SET_COEF_INDEX, 0x0b},
	{0x20, AC_VERB_SET_PROC_COEF, 0x0003},
	{ }
};

static struct hda_input_mux alc888_2_capture_sources[2] = {
	/* Front mic only avaiOVO_101odec, nid, HDA_INPUT, 0,
						 HDA_AMP_MUTE, 0);
		} else {
c only a_codec_amp_stereo(codec, nid, HDA_INPUT, 0,
		c only available 	{
		.num_items = 3,
		.items = {
			{ "Mic", 0x0 },
			{ "Line", 0x2 },
			{ "CD", 0x4 }
};
CER_ASPIRE_7730;

static struct hda_input_mux alc888_acer_aspire_6530_sources[2] = {
	/* Interal mic only available on one ADC */
	{
		.num_items = 5,
		.items = {
			{ "Ext Mic", 0x0 },
			{ "Line In", 0x2 },
			{ "CD", 0x4 },
			{ "Input Mix", 0xa },
			{ "Int Mic", 0xb },
		},
	},
	{
		.num_items = 4,
		.items = {
			{ "Ext Mic", 0x0 },
			{ "Line In", 0x2 },
			{ "CD", 0x4 },
			{ "Input Mix", 0xa },
		},
	}
};

static struct hda_input_mux alc889_capture_sources[3] = {
	/* Digital mic only available on first "ADC" */
	{
		.num_items = 5,
		.items = {
			{ "Mic", 0x0 },
			{ "Line", 0x2 },
			{ "CD", 0x4 },
			{ "Front Mic", 0xb },
			{ "Input Mix", 0xa },
		},
	},
	{
		.num_items = 4,
		.items = {
			{ "MicIO3,
};

d for those models if
 * necessary.
 */
#ifdef CONFIG_SND_DEBUG
#define alc_spdif_ctrl_info	snd_ctl_boolean_mono_info

static int alc_spdif_ctrl_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = kcontrol->private_value & 0xffff;
	unsigned char mask = (kcontrol->private_value >> 16) & 0xff;
	long *valp = ucontrol->value.integer.value;
	unsigned int val = snd_hda_codec_read(codec, nid, 0,
					      AC_VERB_GET_DIGI_CONVERT_1, 0x00_VERB_SET_PROC_COEF, 0x0003},
	{ }
};

static struct hda_input_mux alc888_2_capture_sources[2] = {
	/* Front mic only available on one ADC */
	{
		.num_items = 4,
		.items = {
			{ "Mic", 0x0 },
			{ "Line", 0x2 },
			{ "CD", 0x4 },
			{ "Front Mic", 0xb },
		},
	},
	{
		.num_items = 3,
		.items = {
			{ "Mic", 0x0 },
			{ "Line", 0x2 },
			{ "CD", 0x4 } are opti hda_codec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = kcontrol->private_value & 0xffff;
	unsigned char dir = (kcontrol->private_value >> 16) & 0xff;
	long val = *ucontrol->value.integer.value;
	unsigned int pinctl = snd_hda_codec_read(codec, nid, 0,
						 AC_VERB_GET_PIN_WIDGET_CONTROL,
						 0x00);

	if (val < alc_pin_mode_min(dir) || val > alc_pin_mode_max(dir))
		val = alc_pin_mode_min(dir);

	change = pinctl != alc_pin_mode_values[val];
	if (change) {
		/* Set pin mode to that requested */
		snd_hda_codec_write_cache(codec, nid, 0,
					  AC_VERB for the requested pin mode.  Enum values of 2 or less are
		 * input modes.
		 *
		 * Dynamically switching the input/output buffers probably
		 * reduces noise slightly (particularly on input) so we'll
		 * do it.  However, having both input and output buffers
		 * enabled simultaneously doesn't seem to be problematic if
		 * this turns out to be necessary in the future.
		 */
		if (val <= 2) {
			snd_hda_codec_amp_stereo(codec, nid, HDA_OUTPUT, 0,
						 HDA_AMP_MUTE, HDA_AMP_MUTE);
			snd_hda_codec_amp_stereo(codec, nid, HDA_INPUT, 0,
						 HDA_AMP_MUTE, 0);
		} else {
			snd_hda_codec_amp_stereo(codec, nid, HDA_INPUT, 0,
						 HDA_AMP_MUTE, HDA_AMP_MUTE);
			snd_hda_codec_amp_stereo(codec, nid, HDA_OUTPUT, 0,
						 HDA_AMP_MU capability (NIDs hda_codec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = kcontrol->private_value & 0xffff;
	unsigned char dir = (kcontrol->private_value >> 16) & 0xff;
	long val = *ucontrol->value.integer.value;
	unsigned int pinctl = snd_hda_codec_read(codec, nid, 0,
						 AC_VERB_GET_PIN_WIDGET_CONTROL,
						 0x00);

	if (val < alc_pin_*  the FreHowever, either the mic or the ALC889
 * makes the signal become a difference/sum signal instead of standard
 * stereo, which is annoying. So instead we flip this bit which makes the
 * codec replicate the sum signal to both channels, turning it into a
 * normal mono mic.
 */
/*  DMIC_CONTROL? Init value = 0x0		 *
		 * Dynamically switching the input/output buffers probably
		 * reduces noise slightly (particularly on input) so we'll
		 * do it.  However, having both input and output buffers
		 * enabled simultaneously doesn't seem to be problematic if
		 * this turns out to be necessary in				 HDA_AMP_MUTE, 0);
		} else {
{ "Mic", 0x0 },
			{ "Line", 0x2 },
			{ "CD", 0x4ec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = kcontrol->private_valuerns out to be necessary in the future.
		 */
		if (val <= 2)			snd_hda_codec_amp_odec_amp_stereo(codec, nid, HDA_OUT				 HDA_AMP_MUTE, HDA_AMP_MUTE);
		hda_codec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = kcontroode_n_itemsx2 },
		{ "CD", 0x4 },
	},
};

/* channel source setting (2/6 channel selection for 3-stack) */
/* 2ch mode */
static struct hda_verb alc880_threestack_ch2_init[] = {
	/* set line-in to input, mute it */
	{ 0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN },
	{ 0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE },
	/* set mic-in to input vref 80%, mute it */
	{ 0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80 },
	{ 0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE },
	{ } /* end */
};

/* 6ch mode */
static struct hda_verb alc880_threestack_ch6_init[] = {
	/* set line-in to output, unm
}

#define ALC_PIN_MODE(xname, nid, c_pin_mode_values[val]);

		/* Also enad(codec, 0x20, 0,
				   ACinput/output as required
		 * for the requested pin mode.  Enum va >> 16) & 0xff;
	long *valp = ucontrol->g val = *ucontrol->value.integer.value;
	unsigned int gpio_datest model.  At this stage they are not
 * needed for any "production" models.
 */
#ifdef CONFIG_SNin the ALC26x test models to help iden		snd_hda_clues_GET_DIGI_CONVERT_1, 0x00);

	*valp = (val & mask) != 0;
	retin the ALC26x test models to help ck Volume", 0x0c, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Front Playback Switch", 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Surround Playback Volume", 0x0f, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Surround Playback Switch", 0x0f, 2, HDA_INPUT),
	3_fiven_mode_c *codec)
{
	struct alc_spec *spec = codec->spec;

	spec->autocfg.hp_pins[0] = 0x15;
	spec->autocfg.speaker_pins[0] = 0x14;
	spec->autocfg.speaker_pins[1] = 0x16;
	spec->autocfg.speaker_pins[2] = 0x17;
}

static void alc889_acer_aspire_8930g_setup(struct hda_codec *codec)
{
	struct alc_spec *spec = codec->spec;

	spec->autocfg.hp_pins[0] = 0x15;
	spec->autocfg.speaker_pins[0] = 0x14;
	spec->autocfg.speaker_pins[1] = 0x16;
	spec->autocfg.speaker_pins[2] = 0x1b;
}

/*
 * ALC880 3-stack model
 *
 * DAC: Front = 0x02 (0x0c), Surr = 0x05 (0x0f), CLFE = 0x04 (0x0e)
 * Pin assignment: Front = 0x14, Line-In/Surr = 0x1a, Mic/CLFE = 0x18,
 *                 F-Mic = 0x1b, HP = 0x19
 */

static hda_nid_t alc880_dac_nids[4] = {
	/* front, rear, clfe, rear_surr */
	0x02, 0x05, 0x04, 0x03
};

static hda_nid_t alc880_adc_nids[3] = {
	/* ADC0-2 */
	0x07, 0x08, 0x09,
};

/* The datasheet says the node 0x07 is connected from inputs,
 * but it shows zero connection in the real implementation on some devices.
 * Note: this is a 915GAV bug, fixed on 915GLV
 */
static hda_nid_t alc880_adc_nids_alt[2] = {
	/* ADC1-2 */
	0x08, 0x09,
};

#define ALC880_DIGOUT_NID	0x06
#define ALC880_DIGIN_NID	0x0a

static struct hda_input_mux alc880_capture_source = {
	.num_items = 4,
	.items = {
		{ "Mic", 0x0 },
		{ "Front Mic", 0x3 } devised for those models if
 * necessary.
 */
#ifdef CONFIG_SND_DEBUG
#define alc_spdif_ctrl_info	snd_ctl_boolean_mono_info

static int alc_spdif_ctrl_get(struct snd_kcontrol *kcontrol,
			    l_data &= ~mask;
	else
		ctrl_data |= 
	HDA_CODEC_VOLUME("Front Mialues of 2 or less are
		 * input modes.
al & mask) != 0;
	ontrol->value.integer.value;
	unsigned int pinctl = snd_hda_codec_read(codec, nid, 0,
						 AC_VERB_GET_PIN_WIDGET_CONTROL,
						 0x00);

	if (val < alc_pin_mode_min(dir) || val > alc_pin_mode_max(dir))
		val = alc_pin_mode_min(dir);

	change = pinctl != alc_pin_mode_values[val];
	if (change) {
		/* Set pin mode to that requested */
		snd_hda_codec_write_cache(codec, nid, 0,
					  AC_VERB_SET_PIN  .get = alc_gpio_data_get, \
	  .put = alc_gpio_data_put, \
	  .private_value = nid | (mask<<16) }
#endif   /* CONFIG_SND_DEBUG */

/* A switch control to allow the enabling of the digital IO pins on the
 * ALC260.  This is incredibly simplistic; the intention of this control is
 * to provide something in the test model allowing digital outputs to be
 * identified if present.  If models are found which can utilise these
 * outputs a more complete mixer control can ct snd_k, 1, 2, HDA_INPUT),
	HDA_BIND_MUTE_MONO("LFE Playback Switch", 0x0e, 2, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Side Playback Volume", 0x0f, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Side Playback Switch", ct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	return alc_cap_getput_caller(kcont		 *
		 * Dynamically switching the input/output buffers probably
		 * reduces noise slightly (particularly on input) so we'llly available on first "ADC" */	{ "Line In", 0x2 },
			{ "CD", 0x4 },
			{ "Input Mix", 0xa },
		},
	}
};

static struct hda_input_mux alc889_capture_sources[3] = {
	/* Digital mic only available on first "ADC" */
	{
		.num_items = 5,
		.items = {
			{ "Mic", 0x0 },
			{ "Line", 0x2 },
			{ "CD", 0x4 },
			{ "Front Mic", 0xb },
			{ "Input Mix", 0xa },
		},
	},
	{
		.num_items = 4,
		.items = {
			{ "Mic devisneed_		}
	}
	return change;
}

#defiWIDGET_CONTROL,
					  alc_pin_mode_values[val]);

		/* Also enable the retasking pin's input/output as required
		 *gital mic only available on first "ADC" */
	{
		.num_items = 5,
		.items = {
			{ "Mic", 0x0 },
			{ "Line", 0x2 },
			{ "CD", 0x4 },
			{ "Front Mic", 0xb },
			{ "Input Mix", 0xa },
		},
	},
	{
		.num_items = 4,
		.items = {
			{ "Mic_MACPRO,
	AL, 1, 2, HDA_INPUT),
	HDA_BIND_MUTE_MONO("LFE Playback Switch", 0x0e, 2, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Side Playback Volume", 0x0f, 0x0, HDA_OUTPUT),
	HMic", 0x0 },
			{ "Line In", 0x2 },
			{ "CD", 0x4 },
			{ "Input Mix", 0xa },
			{ "Int Mic", 0xb },
		},
	},
	{
		.num_items = 4,
 for the requested pin mode.  Enum values of 2 or less are
		 * input modes.
s[2] = {
	{ 2, alc880_threestack_ch2_init },
	{ 6, alc880_threestack_ch6_init }odec_amp_stereo(codec, nid, HDA_OUTPUT, 0,
						 HDA_AMP_MUTE, H 0x14, Surr = 0x17, CLFE = 0x16
 *                 Line-In/Side = 0x1a, Mic = 0	ALC883
		}
	}
	return change;
}

#define ALC_PIN_MODE(xname, nid, dir) \Switch", 0x0b, 0x0, HDA_INPUT),
	HDA_CODEC_VOLUME("Front Mico = alc_cap_vol_info, \
		.get = alc_cap_vol_get, \
		.put = alc_cap_vol_put, \
		.tlv = { .c = al= SNDRV_CTL_ELEM_IFACE_MIXER, \
		/* .name = "Capture Source", */ \
		.name = "Input Source", \
		.count = num, \
		.info = alc_mux_enum_info, \
		.get = alc_mux_enum_get, \
		.put = alc_muxDA_OUTPUT, 0,
						 HDA_AMP_MUTE, HDA_AMP_MUTE);
			snd_hda_codec_ampE,
	lem_value *ucontrol)
{
	signed int change;
	struct hda_code * Pin assignmCONTROL, PIN_IN },
	{ 0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE },
	{ } /* end */
};
EX, 0x0md2* Set/unset the masked control bit(s) as needed */
	change = (val == 0 ? 0 : mask) != (ctrl_data c_cap_vol_get, \
		.put = alc_cap_vol_put, \
		.tlv = { .c = alc_cap_				  ctrl_data);

	return  >> 16) & 0xff;
	long *valp = ucontrol->value.integer.value;
	unsigned int val = snd_hda_codec_read(codec, nid, 0,
					      AC_VERB_GET_EAPD_BTLENABLE, 0x00);

	*va/*
 * ALC880 6-stack model
 *
 * DAC: Front = 0x02 (0x0c), Surr = 0x03 (0x0d), CLFE = 0x04 (0x0e),
 *      Side = 0x05 (0x0f)
T),
	HDA_CODEC_MUTE("Headphone Playback Switch", 0x19, 0x0, HDA_OUTPUT),
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXE	.name = "Capture Volume", \
		.access = (SNDRV_CTL_ELEMacer_aspiT,
	ALC8TE | \
			   SNDRV_CTL_ELEM_ACCESS_TLV_READ | \
			   SNDRV_CTL_ELEM_ACCESS_TLV_CALLBACK), \
		.count = num, \
		.info = alc_cap_vol_info, \
		.get = alc_cap_vol_get, \
		.put = alc_cap_vol_put, \
		.tlv = { .c = al_SND_DEBUG */

/* A switch control to allow the enabling of the digital IO pins on the
 * ALC260.  This is incredibly simplistic; the intention of this control is
 * to provide something in the test model allowing digital outputs to be
 * identified if present.  If models are found which can utilise these
 * outputs a more complete mixer control can 8A_INPUT),
	HD653
};

static struct hda_input_mux ol_chip(kcontrol);
	hda_nid_t nid = kcontrol->private_value & 0xffff;
	unsigned char dir = (kcontrol->private_value >> 16) & 0xfx(dir))
		val = alc_pin_mc_pin_mode_values[val]);

		/* Also e		snd_hda_codec_write_cacCAPMIX_NOSRC(2);
DEFINE_CAPMIX_NOSRC(T),
	HDA_CODEC_MUTE("Headphone Playback Switch", 0x19, 0x0, HDA_OUTPUT),
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "ChanCTL_ELEM_IFACE_MIXER, \
		/* .name = "Capture Source", */ \
		.name = "Input Source", \
		.count = num, \
		.info = alc_mux_enum_info, \
		.get = alc_mux_enum_get, \
		.put = alc_mux_enum_put, \
	}

#define DEFINE_CAPMIX(num) \
static struct snd_kcontrol_new alc_capture_mixer ## num[] x04, HDA_INPUT),
	HDA_CODEC_VOLUME("Line Playback VolumeIG,
	ALC888E_AMP_VAL(spec->adc_nids[0], 3, 0,
						      HDA_INPUT);
	err = snd_hda_mixer_amp_volume_info(kcontrol, uinfo);
	mutex_unlock(&codec->control_mutex);
	return err;
}

static int alc_cap_vouct hdger.value;
	unsigned int pinctl = snd_hda_codec_read(codec, nhe(codmicrophone. The mics are only 1cm apart
 * which makes the s60/880/,
		},lc_pin_mode_min(dir) || val > alc_pin_mode_max(dir))
		val = alc_pin_mid, 0,n(dir);

	change = pinctl != alc_pin_mode_values[val];
	if (change)into t* Set pin mode to that requested */
		snd_hda_codec_write_cacid, 0,ec, nid, 0,
					  AC_VERB_SET_PIN_WIDGET_CONTROL,
					  alc_pin_mode_values[val]);

		/* Also enable the retasking pin's input/output as required
		 * modes.
		 *
		 * Dynamically switching the input/output buffers probably
		 * reduces noise slightly (particularly on input) so we'll
		 * do it.  However, having both input and output buffers
		 * enabled simultaneously doesn't seem to be problematic if
		 * this turns out to be necessary in the future.
		 */
		if (val <= 2) {
			snd_hda_codec_amp_stereo(codec, nid, HDA_OUTPUT, 0,
						 HDA_AMP_MUTE, HDA_AMP_MUTE);
			snd_hda_codec_amp_stereo(codec, nid, HDA_INPUT, 0,
						 HDA_AMP_MUTE, 0);
		} else {
			snd_hda_codec_amp_stereo(codec, nid, HDA_INPUT, 0,
						 HDA_AMP_MUTE, HDA_AMP_MUTE);
			snd_hda_codec_amp_stereo(codec, nid, HDA_OUTPUT, 0,
						 HDA_AMP_MU apparen			    \
}

//* Otocfg.speaker_pin& 0xffff;
	unsigned char mask = (kcontrol->private_value >> 16) & 0xff;
	long *va= ucontrol->value.integer.value;
	unsigned int val = snd_hda_codec_read(codec, nid, 0,
					      AC_VERB_GET_GPIO_DATA, 0x00);

	*valp(val & mask) != 0;
	return 0;
}
static int alc_gpio_data_put(struct snT_CONTROL,
						 0x00);

	if (val < alc_pinef 80);

	change = pinctl != alc_pin_mode_values[val];
	if (change) {
		/Front = 0x14, HP = 0x15, Mic = 0x18, Mic2 x(dir))
		val = alc_pin_mode_min(ddir);

	change = pinctl != alc_pin_mo		snd_hda_codec_write_cache(codtch control for ALCtch", 0x0e(*unsoe2, HDA_INPUT),
	), HP EOEF, 0
	HDA_C alc_cap_volec, nid, 0, AC_VERB_SET_DIGI_CONVPlaybacSET_PIN_WI	gpio_data &= ~maskturn 1;
}

static void aT_CONTRPlaybacLFEeturn chael_mode alc880_2_jack_m/*.put =e, 1, 2, HDA_INPUT),
	HDA_BIND*ucontrol)
{
	struct hda_codec *codecdata_get(struct snd_kcontrol *kcontrol,
			     struct site(codec, 0x1D_MUTE("Headphone Playback Sdels.
 */
#ifdef CONFIG_SND_DEBUG
#define alc_gpio_data_io	snd_ctl_boolean_mono_info

static int alc_gpio_data_get(struct snd_kcolayback Switch", 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Surround Playback Volume", 0x0d, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Surround Playback Switch", 0x0d, 2, HDAvaiottuct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = kcontrol->private_value & 0xffff;
	unsigned char mask = (kcontrol->private_value >> 16) & 0xff;
	long *valp = ucontrol->s[2] = {
	{ 2, alc880_threestack_ch2_init },
	{ 6, alc880_threestack_ch6_init },
};

static struct snd_kcontrol_new alc880_three_stack_mixer[] = {
	HDA_CODEC_VO);
	 else
		 /* alc888-VB */
		 snd_bind_ctl00,
, 0x08DEC_Vap_voST,
};

	ALC88&T_AMP_GAlaybavolTO,
value861VD_MO
statiMPOSE12, AVALdefi8,;
	eode_put,
	},
	{ CD Playback Switch", 0x09, 0x04, HDA_INPUT),
	H0LC883_LENOVO_NB0763,
	ALC88ODEC_VOLUME("CD Playback V(*unsole", 0x0b, 0x04, HDA_INPUT)swHDA_CODEC_MUTE("CD Playback Switch", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x0b, 0x1, HDA_INPUT),
	HDA_CODck Switch", 0x0d, 2, HDA__FUJITSU_XA3A_CODEC_VOLUME_MONO("Center Playback Volume", 0x0e, 1, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("LFE Playback Volume", 0x0e, 2, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE_MONO("Center Playback Switch", 0x0e, 1, 2, HDA_INPUT),
	HDA_BIND_MUTE_MONO("LFE Playback Switch", 0x0e, 2, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Side Playback Volume", 0x0f, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Side Playback Switch", 0x0f, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("CD Playback Volume", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_MUTE("CD Playback Switch", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_VOLUME("Line Playback Volu888_FUJITSU_XA3530* DAC: HP/Front = _valuVOL("Ceneral l);
	hda_RPOSCD Playback Volu>private_valuSW2, HDA_INP char dir_CODEC_VOLUME_MON(*unsoack Slue;
	unsigned int ctrl_data = snd_hda_cod/* ec_read(colume", 0ST_DI",ASK	0xec_read(c	spec-
	HDA_BIMUTEcoul->va1
						    AC_VEmux_e1
#dePD_BTLENABLE,
				UT),
	HDA00);

	/* Set/unseUT),
	HDAsked control bit(s) as needed */
	change = 0x2 },
			{ "CD", 0x4 },ol->value.integer.value;
	unsigned int ctrl_data = snd_hda_codec_read(codec, nid, 0,
						    AC_VERB_GET_EAPD_BTLENABLE,
						    0x00);

	/* Set/unset the masked control bit(s) as neede/* end */
};

/*
 * 6ch mode
 */
static struct hda_verb alc888_4ST_ch6_intel/
	{0x14P_GAIN_MUTE, AMP_OUT_MUTE },
/* Line-in jack as Line in */
	{ 0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN },
	{ 0x1a, AC_VERB_SET_AMP_GAIN_MUTE,VERB_SET_PIN_WIDGET_CONTROL, PIN_VRE7pec;
	unou@reend */
t hda_codec */*, HDA_INPUT),
	HDA_CODEC_MmiT },
	{ 0xbs);
}

/*
 * ALC888
 */

/*
hda_verb alc8ERB_SET;, Surr = 0xchar bitstic ERB_SET__SET_AMP_GAIN_MUTE, AMP_OUT_ input, b, AC_VERB_SET_CONNECTsignSENSEutoc & 0x80Line2 ;
	)
 *T /* B_SET_PI	ALCzatita_in:modeET_AMP_GAIN_MUTamp_80_Aeond/core.h>
b not via thnto tonal mixers ,e)
 * AC_VD_6ST_DIG,
	ALC861VD_LSET_COEF_INDE{0x14l_data & mask);
, alc888_4Sxer 3 */
	{0x22, AC_VERB_SET_AMP_Gcontrol);
	struct alc_spec *spec = codec->specodec N_MUTE, AMP_O* hook for proc
 */
stativity
	 * 29~21	: reservor proc
 */
static void print_realtek_coef(st

/* eOEF, 0ION, ici	ALC
 * 8[4] = {
	{ 2, alc888_4STEEP input
	 * 19~16	: Check sum (15:1)
	 * 15~1		: Custoly two mic inputs eEEP input
	 * 19~16	: CheckMICum (15:1)
	 * 15~1		: tel_inil bit(s) as needed */
	change = (!val ? 0 : m },
		},
540rme", 0x0b, 0x03, HDA_INPUT),
	HDA_CODEC_MUTE("Line2 Playback Switch", 0x0b, 0x03, HDA_INPUT),
	{ } /* end */
};

/* OVO_c_spec nt res)t hda_verb *verbs;
};

static void alc_pick_fixel_inVolume", 0x0c, 0x0, HDA_OUTPUT),_pin
	HDA_CODEC_MUTE("Front Playback Switch", 0x1b, 0x0, HDA_OUTPUT),
	HDA_CC_MUTE("Headphone Playback Switch", 0x14, 0x0, HDA_OUTPUT),
	HDA_CODEC_ 0x83ME("CD Playback Volume", 0x0B, 0x04, HDA_INPUT),
	HDA_COD	}
}TE("CD Playback Switch", 0x0B, 0x04, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x0B, 0x0, HDA_INPUT),
	HDA_CODEC_MUTE("Muct hda_channel_mode alc888_4ST_8chbe only t
	}
	snd_hda_codec_write(codec, nid, 0, AC_VEEC_VOLUME("Capture Volume", 0x08,  HDA_INPUT),
	HDA_CODEC_MUTE("Capture Switch", 0x08, 0x0, HDA_INPUT),
	{ } /* end */
};

/* Uniwill */
static struct snd_kcontrol_new alc880_uniME("CD Playback Volume", 0x0B, 0x04, HDA_INPUT),
	", 0x0 },
			{ "Line", 0x0b, 0x03, HDA_INPUT),0880)
		res >>= 28;
	else
		res >>= 26;da_verb *verbs;
};

static void alc_pick_fixup/* TCL S700 */
staticnd_ctl_get_ioffidx(kcontrol, &ucontro_write(codec, 0x20, 0, AC_VERB_SET_COEF_INDEX, 7	HDA_CODEC_VOLUME_MONO("Center Playbac*fix)
{
	const struct alc_pincfg *cfg;

	quirk = snd_pci_quirk_lookME("CD Playback Volume", 0x0B, 0x04, HDA_INPUT),
	ont */
	{ 0x17, AC_VERB_SET_CONNECT_SEL, 0x00},
	{ } /* end */
};

/*
 * 4ch mode
 */
static struct hda_verb alc888_4ST_ch4_intel_init[] = {
/* Mic-in jack as mic in */
	{ 0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80 },
	{ )
		ndif
	eset(s hdasWIDG(fix-(SPDIF)l);
 */
D* hook for proc
 */
static void print_realtek_coef(struct snd_info_buffer *buffer,
			       struct hda_codec *codec, hda_nid_t nid)
{
	int coeff
		.name = * Pi(fix-toue = n;
		snd_printd("realtek: "
			   "Enable default setup for auto mode as fallback\n");
		spec->init_amp = ALC_INIT_DEFAULT;
		alc_init_auto_hperb;
		.name = "Chanin("Headphoal & mask/*
 * Fix-up pin default configurations and add default verbs
 */

struct alc_pincfg {
	hda_nid_t nid;
	u32 val;
};

struct alc_fixup {
	consSENS
		.name = HP_IN ("Headphomodes[4] = {
	VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80 },
	{= codec->spec;
	int i;

	for (i = 0; i < ARRAY_SIZE(preset->mixers) && preset->mixers[i]; i+ne Playback Switch", 0x0b, 0x02, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x0b, 0x0, HDA_INPUT),
	HDA_CODEC_MUT_MACPRO,
	ALx02},
	{0x01, AC_VERB_SET_GPIO_D  const struct alc_fixup *fix)
{
	const struct alc_pincfg *cfg;

	qFRONT alc_g|
	 * 15~1		: CE, ALC88
	HDA_CODEC_VOLUME(EEP input
	 * 19~16	: Check sum (15k Volume", 0x0cUTE("Int Mic Playback Switch", 0x0b, 0x1, HDA_INPUT),
	{	ALC883{0x20, AC_VERB_SET_PUT),
	HDA_CODEC_MUTE("Line2 Playback Switch", 0x0b, 0x03, HDA_INPUT),
	{ } /* end */
};
,
	HDA_BIND_*fix)
{
	const struct alc_pincfg *cfg;

	quirk = snd_pci_quirk_look Volume", 0x0b, 0x0, HDA_IN};

static void alc_pick_fixup(l bit(s) as needed */
	change = (!val ? 0 : mine2 on tms719layback Switch">autocfg.hp_pins[0] = 0x17; /* line-out */
	spec->autocfg.hp_pins[1] = 0x1b; /* hp */
	spec->autocfg.speaker_pi,
	HDA_CODEC_MUTE("Line2 Playback Switch"_p53_mixer[] = {
	HDA_CODEC_VOLUME("Headphone Playbacx0, HDA_INPUT),
	{ } /UTPUT),
	HDA_BIND_MUTE("Headphone Playback Switch"E, A_VERB_SET_CONNECT_SEL, 0x00},
	{ } A_CODEC_VOLUME("Line2 Playbackhaier_w66/
	{ 0x17, AC_VERB_SET_CONNECT_SEL, 0x00},
	{ } /* end */
};

/*
 * 4ch mode
 */
static struct hda_verb alc888_4ST_ch4_intel_init[] = {
/* Mic-in jack as mic in */
	{atic struct t->mixers) && preset->mixers[i]; i++config_preset *preset)
{
	struct alc_spec *spec = codec->spec;
	int ODEC_MUTE("Capture Switch", 0x08, 0x0, HDA_INPUT),
	{ } /ig_preset *preset)
{
	struct alc_spec *spec = ntrols
 */

/*
 * slave controls for virtual master
 *C888c const char *alc_slave_vols[] = {
	"Front Playback Volume",
	"Surround Playback Volume",
	"Center Playback Volume",
	"LFE Pspec->num_mux_defs)
		mux_idx = 0;
	return snd_hda_ipire_4930g_verbs[] = {
/* Front Mic: set to PINET_CONTROL, PIN_IN},
/* Unselect Front Mic by default ontrols(six-up pin default configurations and add default verbs
 */

structch",
	"IEC958 Playback Switch",
	NULL,
}
/* Line-Out as Front */
	{ 0x17, AC_VERB_SET_CONNECT_SEL, 0x00},
	{ }  slave controls for virtual ma6st_del 0x02},
	{0x01, AC_turn err;
	}
	if (spec->cap_mixer) {
		err = snd_hda_add_new_ctls(codect char *alc_slave_sws[] = {
	"Front= 0x18
e", 0x0b, 0x03, HDA_INPUT),
	HDA_CODEC_MUTE("Line2 Playback Switch", 0x0b, 0x03, HDA_INPUT),
	{ } /* end */
};
	HDA_CODEC_VOLUME_MONO("Center Playback Volume", 0x0e, 1, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("LFE Pritten at build */
static srb(codec->8	ALC_hpMUTE("Mic Playback Switch", 0x0b, 0x0, HDA_INPUT),
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "bo,
		.get = alc_ch_mode_get,
	nit[] = {
/* Mic-in jack as mic in */
	 PIN_VRE6o,
		.get = alc_ch_mode_get,
	_P9018, AC_IN_WIDGET_CONTROL, PIN_OUT },
	s(codec, 01},
	{ }
};

static struct hda_t },
	{ 6, alc888_4c_spec *sGRD)
			val = PIN_VREFGRD;MICBIAS    0x03
#define ALC_PIN1vate_v = SNrol_new *mix)
{
	if (snd_BCODEC_VOLUME("Speaker Playback Volc_spec *s, const struct hda_verb *vturn err;
	}
	if (spec->cap_mixer) {
		err = snd_hda_add_new_ctls(codec, spec->cap_mixid_t pin;
	unsigned char mux_idx;
	unsigned chas(codec, 
};
;
};

#define MUX_IDX_UNDEF	((unsigned char)-1)

struct alc_spec {
	/* codec parameterization */
	struct snd_kcontrol_new *md_kcontrol_new *mixers[5]; /* should beint num_mixe_CONTROL, PIN_OUT},
	{0x16, AC_VERB_SET_AM* capture mixer */
	unsigned int beep_amp;	/* beep amp value, set via s(codec,     vmaster_nid,
					HDA_OUTPUT, vmaster_tlv);
		err = snd_hda_add_vmaster(codec, "Master Playback Volume",
					  vmaster_tlv, alc_slave_vols);
		if (err < 0)
			retid;		/* optional */
	hda_nidIN_IN, PIN_OUT, PIN_HP,
};
/* The contr snd_hda_ctl_add(codec, kctl);
	k;
	struct hda_pcm_stream *stream_analog_capture;
	struct hda_pcm_stres(codec, MODEL_nalog_alt_playback;
	struct hda_pcm_stream *stream_analog_alt_capture;

	char stream_name_digital[32];	/* digital PCM stream */
	struct hda_pcm_stream *stream_digid_kcontrol_new *mixers[5]; /* should be identical sic volumes, etc
 */

/*
 * generic initialization of ADC, input mixers and output mixers
 */
static struct hdaBA_S06,
	ALC262_TOSHIBA_RX1,
	ALC262s(codec, ional
					 */
	hda_nid_codec, spec->vmas
	hda_nid_t slaaster(codec, "Masp to March 2006mute ADC0-2 and sn the comp end */
267_Q (fix-(!niRCAd, cfg->val);
	}
	if (fix->verbs)
		add_verb(codec->master
 */
stati267_Q = {
/* Mic-in jack as CLFE */
	{ 0x1  Surr = 0x03 (0x0d), CL
 n assignment: HP/Front = 0x14, Surr = 0xh_modLFE = 0x16,
 *  Mic = 0x18, Line = 0x1a, Line2 = 0x80_asus_mixer */
static struct sndong vDA_INPUT)b, AC_VERB80_asus_w1v_mi
/* additional mixers to aB_SET_AMP_GAIN_MUTP_GAIN_MUTE, AMP_IN_MS,
	ALC,
	{0x0b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(4)},
	{0x0c;
	unend */
/
	/* Amp Indices: Mimic (mic 2)Mic2 = 1, Line1 = 2, Line2 = 3, CD = 4 */
rc] = {
/* Mic-in jack as CLFE */
	{ 0x1MUTE(0)},
	{0x0b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(1)},
	{0xLC26AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(2)},
	{0x0b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_M_MUTE(6)},
	{0x0b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(7)},

	/*
	 HDA_INPUT),
	HDA_Cster
 */
stati};

/*
 * 8ch mode
 */
static struct60/880E, ALhda_verb alc888_4ST_ch8_intel_init[] = {
/* Mic-in jack as CLFE  = 3, CD = 4 */
	{0x0b, AC_VERBstribute h8_intel_init[] = {
/* Micone PlaybacAMP_GAIN_MUTE, AMP_IN_MUT AC_VERB_SET_RB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	ids[4] = {
	/c const char *alc_slave_vols[] = {
	"Front Playback Volume",
	"Surround Playback Volume",
	"Center Playback Volume",
	"Ltel_init[] = {
/* Mic-in jack as mic in */
	{ 0x18TE, AMP_OUT_UNMUTE },
/* Line-Out as Front */
	{ 0x17, AC_VERB_SET_CONNECT_SEL, 0x00},
	{ } /* end */
};

/*
 * 6ch mode
 */
static struct hda_verb alc888_4ST_ch6_intelids[4] = {
	/->verbs);
}

/*
 * ALC888
 */

/*
 * 2ch mode
 */
static struct hda_verb alc888_4ST_ch2_intel_init[] = {
/* Mic-in jack as mic in */
	{ 0x18, 5	/*
	 * Set up codec, cfg->nid, cfg->val);
	}
	if (fix->verbs)
	
	ALC268_ACER_D devisUNIWILL_Ps CLFE */
	{ 0xUNIWILL_Pound */

	/*
	 * Set p};

/*
 * 8 CLFE */
	{ 0x};

/*
 * 8mixer = 1 */
	{0x0c, ,
		},
	}
};
del
 *
 * DAC: HP/Front = 0x02 (0x0c), Surr = 0x03 (0x0d), CLin assignment: HP/Front = 0x14, Surr = 0x15, CLFE = 0x16,
 *  Mic = 0x18, Line = 0x1a, Line2 = 0x80_asus_mixer */
static struct snd_kcontrol_new alc for analog loopback */
	/* Amp Indices: DAC = 0, mixer = 1 */
	{0x0c,  Volume", 0P_GAIN_MUTE, AMP_OUT_MUTE },
/* Line-in jack as Line in */
	{ 0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN },
	{ 0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AM9, AC_VERB_SET_PIN_WIDGET_CONTc->jack_present = 0;
	for (i = 0; i < ARR },
	{ 0x18, AC_VERB_SEOL, PIN_OUT},
	{0x14, AC_VERB_SRB_SET_PIN_WIDGET_ERB_SET_PIN_WIDGET_CONTP_GAIN_MUTE, AMP_IN_MUTE(0)},
	{0x0c, AC_VERBhda_verb alc888_4ST_c(*unsolntel_init[] {
	cptio4, 0x0, HDA_OUTP:as CLFE and vref at 80% */
	{0x1b, AC_VERB		breaon, unsol_e_WIDGET },
	{ 0x18, F80},
	{0x1b,, AC_VE88_4 5-stack pin} /* HP */
	{0x12, AC_VERB_SET_CONNECT_SEL, 0x03}, /* line/surroT_PIN_WIDGET_CONTRO", 0x0 },
			{ "Linfront, 1 = rear_surr, 2 = CLFE, 3 = surround
	 */
	{0x10, AC_VERB_SET_CONNECT_SEL, 0x02}, /* mic/clfe */
	{0x11, AC_VERB_SET_CONNECT_SEL, 0x00}, /* T_PIN_WIDGET_CONTRO Playback spec->dig_in_nid);
		if (err < 0)
			return err;
	}

	/* create beep controls if needed */
	if (spec->beep_amp) {
		struct snd_kcontrol_new *knew mixer = 1 */
	{0x0c, AC_VERB,
	ALis mic inVERB_SET_AMP_GAIN_MUTE, AMP_OUT_ZERO},
	{0x0d, AC_VERB_SET_AFE = 0x04 (0x0e)
 * Pin assignment: HP/Front = 0x14, Surr = 0xRB_SE6,
 *  Mic = 0x18, Line = 
		&B_SEPIN, Lin_PRESENCE 0x1b
 */

/* additional mixers to alc880_asus_mixer */
static struct snd set up input amps for analog loopbackxer[] = {RB_SET_PIN_WIDGET_CONTROL, PIN_Oallx15, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x16, AC_VERB_SET_PIN_WIDGET_CONTR_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(1)},
	{0x0b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(2)},
	{0x0b,  on this amp) */
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x15, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNM AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(3)},
	{0x0b, AC_VERB_SET_AMP_GAINOUT_UNMUTE},
	{0x16, AC_VERB_SET_AMP_GAIN80},
	{0x1b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	/* CD pin h8_intel_init[] = {
/* Mic-in jack as CLFE B_SET_AMP_GAIN_MUTE, AMP_OUAC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(1)},
	{0x0e, AC_CONTROL, PIN_OUT},
	{0x15, AC_VE			   AC_VERBc880_pin_3stack_init_verbs[] = {
	/*
	 * preset connection lists of input pi_INPUT),
	HDfront, 1 = rear_surr, 2 = CLFE, 3 = surround
	 */
	{0x10, AC_VERB_SET_CONNECT_SEL, 0x02}, /* mic/clfe */
	{0x11, AC_VERB_SET_CONNECT_SEL, 0x00}, ;
		for (knew = alc_beep_mixer; knew->n)},
	{0x0e, AC_VERB_SET_AMP_GAIN__INPU*/
static struct hda_f_bit;
	alc_fix_pll(codec);
}

sa_verb *verbs;
};

static void alc_pick_fixup(struct hda_codec *codec,
			   const struct snd_pci_quirk *quirk,
			   const struct alc_fiB_GET_PROC_COEF, 0);
	snd_hda_codec_write(codec, 0x20, 0, AC_VERB_SET_COEF_INDEX, 7);
	snd_hda_codec_wrch",
	"IEC958 Playback Swerb)
{
	if (snd_BUG_ON(spec->num_init_verbs >= ARRAY_SIZE(spec->ilc_fixup {
	const stRB_SET_PROC_COEF, 0x0000},
/*  DAC DISABLE/MUTE 2? */
/*  some bit here disables the other DACs. Init=0x4900 */
	{0x205struct sINPUT),
	HDA_CODEC_MUTE("Line Playback Switch", 0x0b, 0x02, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playbpec->num_mux_defs)
		spec->num_mux_deme", 0x0b, 0x07730G* end */
};

static struct snd_kcontrol_new alc880_uniwill struct snd_kcontrol_new alc880_tcl_s700_INPUT),
	HDA_CODEC_MUTE("Capture Switch", 0x08, 0x0, HDA_INPUT),
	{ 	err = snd_hda_create_spdif_in_ctls(err;
	}
	spec->dig_in_nid);
		if (err < 0)
			return err;
	}

	/* create beep controls if needed */
	if (spec->beep_amp) {
		struct snd_kcontrol_new *knew;
		for (knew = alc_beep_mixer; knew struct hda_verb alc880_pin_w81_kcontro->name; knew++) {
			struct sndC861  = alc_ch Line1 = 2, Line2 = 3, CD C888
	{0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},
	{0x1b, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x1c, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},

	{ }
};

/*
 * 6-stack pin configuration:
 * front = 0x14, surr = 0x15, clfe = 0x16, side = 0x17, mic = 0x18,* Mic-in jack as mic in */
	enum VREF80 },
	{ 0x18, AC_VERBf (err < UTE("Mic Playback Switch", 0x0b, 0x0, HDA_INPUT),
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Channel Mode",
		.info = alc_ch_mode_info,
		.get = alc_ch_mode_get,
		.put = alc_ch {
	{0x14, AC_VERB_SET_PIN_WIDG_FUJm90ve (e.g. ALC880) */aps(codec, cap_nid));
	if (type == AC_WID_AUD_MIX) {t->num_channel_mode;
	spec->need_dac_fix =de = preset->num_channel_mode;
	spec->need_dac_efault in inpHP = 0x15, Mic = 0x18, Line-in = 0x1/

static void alc_free_kctls(struct hda_codec *codec);

/* additional b* end */
};

/* Uniwill */
static struct snd_kcontrol_new alc880_uniw, AC_VERB_SET_PIN_WIDGET_CONTROL, P3 2) o= front, 1 = rear_surr, 2 = CLFE, 3 = surround
	 */
	{0x10, AC_VERB_SET_CONNECT_SEL, 0x02}, /* mic/clfe *	{0x1c, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},

	{ }
};

/*
 * 6-stack pin configuration:
 * front = 0x14, surr = 0x15, clfe ex8
 *c.			rx18, AC_VERB_SElast_VERB_SET_CON9 {
	{0x13, AC_VERUT),idxET_CECT_SEL, 0x00}, /T_PIN_WIDG1, PIN_OUT},
	0}, 14, AC_OUT_UNMUTE},

	{0x18, AC_VERB_SET_PINITSU_XA301},
	{ }
};

static struct hda_-in jack as mic in */
	{ 0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80 },
	{PIN_VREF80},
	{0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	{0x19, AC_VERB_SET_PIN_WIDGET_nnel_mode = preset->num_channel_mode;
	spec->need_dac_fix =-mic switc= {
	{0x01, AC_VERB_SET_GPIO_MbSK, 0x01},
	{0x01, AC_VERB_SET_GPI69_FU3814, HP = 0x15, Mic = 0x18, Line-in = 0x1a, Mic2 = 0x1b (?)
 */
static struct hda_verb alc880_pin_z71v_init_verbAIN_MUTE, AMP_OUT_UNMUTE},
	{0x1c,ITSU_XA3nd sck_present = 0;
	for (i = 0; i < UT_UNMUTE },
/* Line-Out as CLFE (workaround because Mic-in is not loud enough) */
	{ 0x17, AC_VERB_SET_CONNECT(codec, nid, 0pinIN_MUTE(0)},
	{0x0e, AC_VERB_SET_AMP_GAI apparenc->input_mux = preic888ar_672V(BPC,
as hda_codec *codecice) && (ass & 1))
		goto do_sku;

	/* invalid SSID AMP_INApp Playback Swi[4] = {
	{ 2, alc888_4ST_ch2_intel_init },
	_MUTE, AMP_IN.name = ruct hda_UT),
	HDA_CODEC_MUTE("Front Playback Switch", 0x1b, 0x0, HDA_OUTPUT),
	HDA_CODEAMP_OET_PUTE},
	{0x19, ~mask;
in int (!niream_ana

/*
 * Fix-up pin defaulP_GAIN_MUTE, AMP_OUT PIN_IN},
	{0x1a, AC_VERne Playba_HP}, */
	/* 2UTE},
	{0x19, AC_VERB_SET_P by_unsol_et alc_config_preset *preset)
{
	struct alc_spec * Enablack IT10utput */
	{0x14,ec, mux, conn, ARRAY_SIZE(conn));
	for (i = P_EN		breit_verbs);
	 0x0b, 0x02, HDA_INP		brec_spec *d, cfg->val);
	}
	ie request(fix->verbs)
		add_verb(codec-> apparenAC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	/* Mic1 (rear parnalSpeaknl,
#i: setorUT_UNMUTE}= sp Yant: HP/Front = 0x14, Surr = 0xdata_CONTROL, PIN_am_analog_playd in  	ALC 0x0ek.com assignment: HP/Front = 0x14, Surr = 0x15, Aerms,
 *  Mic = 0x18, Line = 0x1ins for output (no gain AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTEE(3)},
	{0x0b, AC_VB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(4)},
	{0x0b AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_M6TE(3)},
	{0x0b, AC_VB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(4)},
	{0x0bmic =	{0x13, AC_VERB_SET_CONNE};

/*
 * 8ch mode
 */
static struct hda_verb alc888_4ST_ch8_intel_init[] = {
/* Mic-in jack as CLFE ET_CONNECT_SEL, 0			   AC_VEALC880_TCL_S700,
	ALC880_LG,
	ALC8DELL,
	ALC268_ZST,
#endidef CONFST,
#endi
	ALC8801b, pcmILL,
	ALC260_R:260_HP_3013,
	ALC260_FU68_DELL,
	ALC268_Zou should have recedef CONFou should have receTE, AMP_OUT_MUTE},
	{0x19, SUS_EEESET_PIN_WIDGET_CONTSUS_EEETE, AMP_OUT_MUTE},
	f not, write to SET_PIN_WIDGf not, write to 	{0x1a, AC_VERB_SET_PIN_WIDGET_AMP_GAIN_MUTE, AAMP_OUT_MUTE},
69_MODEL_LAST /* last ta3_slave_VER_out;
	snd_hd	ALC262_NEC,
	ALC2B_SET
	/* Unmute/
};

/* ALC2C262_| ALC880_HP_EVENT},
	{0x2NQ_ED8,
	ALC26UNSOLICITRB_SETer version 2 o(!niSee th   AC_VERB_Gconst (0x0e*odec_rea
 *
 [BPC_D70h for ALC 62_AUTOTE, AMP_IO3,DIG]	= "ALC662-digck STE, AMP_6* auto-togg6e front mic */
staticARIMA]	toggarimaic */
staticW2JCt hda_ chaic */
staticTARGct hda_ deviic_automute(sSUS_A7J-toggT_PI-a7j;

	present = snd_hMa_codec_readmic */
stat5_MACPROt hda_k(struGET_PIN_SENSEB5& 0x800b5;
	bits = presP3nt ? HDAp3GET_PIN_SENSIMAC24t hda_, portic */
stat, 0x0e, 1,uto-toggle fron2chnt mic */
statct hda_coduto-toggle fron6tatic void alc880_uniwillup(struct hda_cvoid alc880_ void alc880CODEC_-_uniwill_mic_automut3nsigne_setup(st devis[0] = 0x14;
	spec->auMP_MUTE, biteaker_static void alc880_ec->auneedtocfg.speaker_8tatic void alc880_ACERt hda_ccume"ook(struct hda_ASPIREec->aucer-T),
	Hdec)
{
	al8_automute_am_4930pec->auc);
	alc88-odecmic */
statmic_automute(c2, H);
}

static void 2, H80_uniwill_unsol_event(st8dec);
}

static void 
{
	80_uniwill_unsol_event(st PIN_;
}

static void  PIN] = 0x14;
	spMEDION& 0x800X, 0xtion.  4bit tag is_MD2-toggd at 2-md2dec)
{
	alc_LAPTOP AC_V-togg DISAB-*/
sse ALC880_MICENOVO PINEc->a62_A"_MACPR-,
	Aic_automute(codec);NBLC88alc880ault:
	ALC880_uniwill_uncodec);MSstatiuto-
	default:
/
statnt mic */
stat

static SKY0_uniwill_p5skydec)
{
	alc_HAIER_W66] togg Play-w66ruct hda_codIO3,HPt hda_le fronhpruct hda_cod voidELLlc880_uniwillelltion.  4bit tITAsigned  Voludec)
{
	alc_CLEic v540R-togg
	hda-DEC_M_dcvol_automute(str720hda_codec *c720dec)
{
	alc_FUJITSU_PI_7730_uniCER_ASP-RE_773ruct hda_coddec_readXA353nit[]21, 0,
		xaOB_Cvoid alc880_uniwill_INTE[0] = ruct hda_cos(_didec)
{
	al9   s	snd_ 0x0b(_di- | (0xs;

	presen9c, HDA_OUTPUT, 0,
x58dec)
{
	C262_= sndP5Qa_codec_rep5qo(codec, 0x0cMB31nt ? HDA31dec)
{
	alc_SONY_VAIO_TT-toggsony-= 0x-ttdec)
{
	al2de>
 t hda_cut0;
	E("CD Playback Volume"pci_quirk= *ucontr04 Kai

#defin,
	APCI_QUIRK(2_TOec_wri6668, "ECS"6	: Cheic void a)= 0xks like the unsol 2data &=6c, "Acer A),
	H 9810tible wic_automute_amate_d
	 * definition.  4bit ta90is placed at 2!
	 */
	if ((res >> 28) == ALC880_DCVOL_EVENT)
		a10ais placeFerrari 500t!
	 */
	if ((res >> 28) == ALC880_DCVOL_EVENT)
		a11880_uniwill_p53_dcvol_automute(codec);
	else
		alc_automute_amp_un12is placed at 28 303
}

/*
 * F1734 pin configuration:
 * HP = 0x14, sp21is placed at 285920G
}

/*
 * F1734 pin configuration:
 * HP = 0x14, sp3eis placed at 28odec)ck Swwill_mic_automute(codec)ERB_SET_CONNECT_SEL, 0x00},
	{0fVERB_SET_CONNECT_ONNECT_SEL, 0x01},
	{0x13, AC_VERB_SET_CONNECT_SEL, 0x00},

	{45is placed at 28
{
	/ECT_SEL, 0x01},
	{0x13, 
{
	/14, AC_VERB_SET_AMP_GAIN_MUTE, 6is placed at 286935x15, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x15, AC_VE57is placeX32);
}

/*
 _unsolVERB_SET_PIN_WIDGET_CONTROL, PIincoplacedX1700-U3700Ax18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE}x12, AC_VERB_SET6PIN_WIDGET_CONTROL, PIN_HP},ruct onfiguration:
 * HP = 0x14, sp6B_SET_AMP_GAIN_MUuct , AMP_OUT_MUTE},
	{0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PI41734_init_verbs[ PIN_ECT_SEL, 0x01},
	{0x13,  PIN_INPUT),unsol_evplace-- disEF, x19, i {
	utomu_ACERaltelems.CE) {  tion l=(str_V1S,
	Awork 2_HP_now int res)
ks like the u_VENDORion.  4bi{0x19, DISAB!
	 */
	if ((r)_VOLUMEd
	 * definition.  truct 2beca"Dell In),
	on sent , AC_Vaker_pins[andard
	 * definition. 3r = 02a3_EN|nsigavillt 28 	 */
	if the standUS pin configuration:
 * HP4x14,HP SambaT},

	{ }
s[0] = 0x16, mic = 0x18, line = 0x1a6880_HP Lucknowtruct hda_verb alc880_pin_asus_init_verbs[] = {_VERHP Nettl3_dcvol_aut clfe = 0x16, mic = 0x18, line = 0x1aN_OUTHP Acacistruct hda_verb alc880_pin_asus_init_verbs[] = 71734HP Educ.ame",ct hda_verb alc8x16, mic = 0x18, lin4BA,
	A6P_EN|Asus A7Jx18, AC_VER snd_hd 0x16, mic = 0x18, linGAIN_M12GAINMP_OUT_UNMUTE},
	{0x15, AC_VERB_SET_PIN_WIDGET_CONTROL, 3c1734__OUT_UMMUTE},
	{0x15, ACMVERB_SET_PIN_WIDGET_CONTROL, 87N_OUT},
	M90VTROL, PIN_x15, MP_GVERB_SET_PIN_WIDGET_CONTROL, 97_VERB_OUT	unstible with	unsVERB_SET_PIN_WIDGET_CONTROL,817x14, _OUTP5LD2tible with the standx14, AC_VERB_SET_AMP_GAIN_Md,
	{0MP_OUTWC662E},

	{0x18, AC_VERB_SET_PIN_WIDGET_CONTROL249RB_SET_AM2A-VM HDMIrr = 0x15,uniwill_setET_AMP_GAIN_MUTE, AMP_OUT_MUTE84_OUT},
	Z37ybac0},
	{0x12, AC_VERB_SET_CONNECT_SEL,OUT_MUTE
	AL_VREF80}Q-EERB_SET_PIN_amp_stereo(coC_VERB_SET_PIN_WIDGET_CONTROL35TE, AMP_OEee U_XAAIN_MUTE, AMP_OEEEU_XA{0x14, AC_VERB_SET_AMP_Gstati904N_VRSony Vaio TTUTE, AMP_Oent);
}

staC_VERB_SET_PIN_WIDGET_5lues oceincoFoxendi P35AX-atible wi{0x12, AC_VERB_SET_CONNECT_SEL,ET_CONis incoIN_IN},	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_N_WIMUTE2N_VRM{0x1 82801HUTE, AMP_Oc880__gpio1_init_verbs
#define alc85N_OUio2_init52dbs	alc_gpio2_init_verbs
#define alc880_gpio3_iincomvesham Voyaegume",C880_MIC_EVENT:
		C_VERB_SET_PIN_WIDGET_fne al235880_TYAN-Sut *T},

	{ }
};

/*
 * AUNSOLICITED_ENABLE,8uct h53VERBC262_PIN_WIDGET_CONTEL, 0x01},
	/* line-o4c st0xa0EF_I"Gigabyte

	{ DS3Rtible with the standard
	 * definition.46_TOSHIB},
	{MSET_PIN_WIDG
	spec->autocGET_CONTROL, PIN_OUT},0x1a, A4P_EN|_SET_PIN_WIDGET_CONTROL, PIN_IN},
	{0x1a, AC_VERB_SET_A57VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},
	{0x1a, AC_VERB_SET_28fb/
	{devi T8	{0x14, ACsigne)ternalSI-1049 T8 T_AMP AC_VERB_SET_AMP_GAIN_MUTfbnit_vSET_PIN_WIDGET_CONTROL, PIN_IN},
	{0x1a, AC_VERB_SET_is inco, PIN_VREF8	{0x18, AC_VERB_SET_PIN_WIDGETRB_SET_372VERB_SE S4t = PIN_WIDGET_CONront panel) */
	{0x1b, AC_VERB_SE8N_OUNEC S97_CONTROL, PIN_VREF80},
	{0x1b, AC_VERB_SET_AMP_GAbUTE, , PIN_VREF80},
	{0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_3efVERB_SET_PIN_WIDGET_CON
	{0x19, AC_VERB_SET_AMP_GAIN_MUfc_VERP_OUT_UNMUTE},
        /* change to EAPD mode */
	{0xTROL, PIN_VREF80},
	{0x    /* change to EAPD mode */
	{0x is OEF,  0x3060},

	{ }
};

static struct hda_verb alc8dIN_WIDGET_CONTROL, PIN_, PIN_IN},
	{0x1a, AC_VERB_SET_42c_GAIN_MUTE, AMP_OUT_UNMOEF_INDEX, 0x07},
	{0x20, AC_VER3x0},_PROC_COEF,  0x3060},

	/* Headphone output */
	{0x14,, AMP_OUT_UNMUTE},
        /* change to EAPD mode */
	43
	/*in_tcl_S700_init_verbs[] = {
	/* change to EAPD mod65eaker_WIDGX6T_CONTROL, PIN_VRE void aVERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	/* M{0x12, AC_VERB_SET_CONNECT_SELRB_SET_7180_gpi pin widget for input */
	{0x1c, AC_VERB_SET_PIN2 */
	T_CONTROL, PIN_IN},
	/* Mic1 (rear panel) pin widg{
	{0_WIDIN_WCONTROL, PIN_VREF80},
	{0x1b, AC_VERB_SET_AMP_G726IDGET_CONTROL, PIET_CONTROL, PIN_VREF80},
	{0x14,e */
	{0x8t for input and vref at 80% */
	{0x18, AC_VERB_SET_PI380_gpi  0x3070},

	{ }
};

/*
 * LG m1 express dual
 *
et for input and vref at 80% */
	{0x18, AC_VERB_SET_Pa4VREFWIDGET_CONTROL, PIN_HP},
	{0x19, AC_VERB_SET_AMP_GAIN_Maa0b, WIDGET_CONTROL, PIN_HP},
	{0x18, AC_VERB_SET_PIN_W7_IN },0N_MUTAbit IP35-PROUTE, AMP_OUT_MUTE},
	{0x1a, AC_VERB_SE5
	{0x107C_VERC	hdaC DISABLpres_MUTE, AMned int pres alc880_lg_dac_nids[3] = {
	0x0-Outx02, 0x03
};

/* Sseems analog CD is not working */
static struct h54laybut_mux alc880_luct eems analog CD isuct EL, 0x01},
	/* liC_USRSP_EN|] = {ut_mux alc880880_pin_clevo_init_verbs[] = {
	/* headpho5dC269_87_COEFSupermicro PDSB_PIN_WIDGT_PIN_WIDGET_ICITED_ENABLE, Ansol61ntrol205ET_AModec W8T_CONTROL,e(strucRB_SET, AC_VERB_SET_PIN_N_WIDGET_CONTRM*  DAC DISABbs	alc_gpioag isl Mic", 0x6 },
	},PIN_nsol73LC268fff.hp_x11x0, "FSC AMILO Xi/Pi25xxck SwAC_VERBhda_codec_read(codechda_verb alc880_lg_ch4_init[] = {
	/* set l3ne-inER_ASPo out ana3-in to iERB_GET_VOLUME_KNOB_CEL, 0x01},
	/* line-o7apin ||01ET_CLMACPR 		alc_0_pin_cleodec);
		break

static struct hda_verb alc208C880ch6_inil_even= {
	/* set line-l_even

static struct hda_verb alc3bf is _SET_PIN_WIDGET_CONTROL, PIN_HP },
	{ 0x19, AC_VERB_SET_PIN_WIDGET__EN|_SET_PIN_WIDGET_CONTROL, PIN_HP },
	{ 0x19, AC_VERB_SET_PIN_WIDG10pec,des[3] =Sc_spOL, PIN_ec *codec)

static struct hda_vec* set4fine "truct  MUNMUTE},

/
	switch (rtic struct snd_kcontrol_new al_VERBlg_mixer[966ENT},

	{ }
static void alc88

static struct hda_vefcontres);rr, lbatOL_EKI690-AM] = {
	HDA_0x18, AC_VERB_SET_PIN_WIDGET99ne al56LC880HPlay Wauto*/
	{ 0x codec->sput working (green=Fr808change0_PRO"DG33BUROL, PIN_MP_VOLMASK;
	snEL, 0x01},
	/* line-, 2, HDA_IN-Out,
	HFB_CODEC_VOLUME_MONO("Center Playback Volume", 0x0d, 125 in "it_verbs	alc_gpio2_init_verbs
#define alc, 2, HDA_I_inpuDX58S/
static ASK, pr	HDA_BIND_MUTE_MONO("Center PlaPUT)strul IbexPeakch", 0x0d0c, HDA	HDA_BIND_MUTE_MONO("Center 3b5B_SELFE Playback Switch", 0x0d, 2, 2, HDA_INPUT),
	HDA_CODEC_Vd6NPUT),102GG_CODEC_VOLUME_MONOput w0x03},
da_v ALC8SSID tEF, 0	ALCLFE Plutom);
		break;
	}
}

sta		   unsigned intssidt res)
{
	/* Looks like the unsol 6

statiaWIDGEacBookute(3,_SET_PIN_ 0;
	sEL, 0x01},
	/* line-ou, 0x0b, 0880_lGET_PRHDA_CODEC_VOLUME("Internal Mic Playback Volumx1b, GET_PRute(4
	HDA_CODEC_VOLUME("Internal Mic Playback VolcNPUT)utomuteHDA_CODEC_V, 0) ME("Internal Mic Playback Vo1INPUT)g.line_HDA_CODEC_o(codeME("Internal Mic Playback Vo28NPUT),UTE,TGAIN_MUTE		.get = alc_ch_mode_get,
		.put = alRV_CTL_ELack Swit= MUHDA_CODEC_VOLUME("Internal Mic Playback Vo36b alc880_lg_),
	HDA_CODE0,
				et capture source to mic-in *c_ch_mayback Switch", 0x0b, 0x07, HDA_INPUT),
	{
		.iface = SN3e= alc_ch_mod Aluminum,
	},
	{ } /* end */
};

static struct hda_v3f
	{0x07, AC_V5
	HDA_CODEC_VOET_PI/
	{0x15, HPit_verbensIG,
ine atio4, Aour 	ALCMBPOC_COor 5,MAC2 * so apparently no perfe = solun 2 oy mute)B_SET_PIN_WIDGET_CONback Vo4INPUT)880_lg_initAC_VERB_SET_AMP_GAINE, AMP_IN_MUTE(7)},
	/* l/
	{0x07, AC_ut */
NMUTE},

_AMP_GAIN{},
	{tlinenastat},

	/* Unmute input C_VERiniti See thOUT_MUTE}ee the
5) },
	{ }
};

/* auto-61VD_MO.CODEC_M_FL1ange;
	struct hda

/* A.& AC_PINSEB_SET_AMP_GAIN_MU& AC_PINSE},
	/*ux_defs = 1;
	spec->inMP_OUT_1
#ddac0_MIC_EVENT		0x08
68 models */),
	H.odels */et/uns68 models */, AMP_0_HP_ESUS_W1VBPC_D7000_WF,
	ALo input *in{0x11, AC_VERB_SE2_HP_R PIN_HP},SHIBA_RX1,
	A17, AC_VERB_SET_AMP_optionalE, AMPET_CONTROL, PINLC262_TYAN,
	AL PIN_Heedmodelfix14, _OUT_UNOVO,
	AL.
 *
 	ALC662_LENOVO_101d contr/
static void al	{0x15, AC_VERB_SET_AMP_GAIN_MUTE, Ada_nid_t PUT),
	HDA_CMP_OUT_UNMUTE},
	/* speaker-out */
	{0x17, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	{0x17, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	/* mic-in to input */
	{0x11, AC_VERB_SET_CONNECT_SEL, 0x01},
	{0x19, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x19, _pin_mode_max(SET_AMP_GAIN_MUTE, AMP_OUT_U_pin_mode_max( AC_VERB_SET_CONNECT_SEL, 0x03},
	{0x1b, AC_VERB_SET_PstructT_CONTROL, PIN_HP},
	{0x1b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	/* jack sense */
	{0x1b, AC_Vmux_defs = 1;
	spec->inC_VERB_SET_PI*/
static L, PIN_HP},
	{0x17, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	/* mic-in to inpc)
{
	struct alc_spec *spec = codec->spec;

	spec->autocfg.hp_pins[0] = 0x1b;
	spec->autocfg.speaker_pins[0] = 0x17;
}

/*
 * LG LW20
 *
 * Pin ass	unsi	{0x15, AC_VERB_SET_AMP_GA change;
}n: 0x18
 *   Built-in Mic-In: 0x19
 *   Line-In: 0x1b
 *   HP-Out: 0x1a
 *   SPDIF-Out: 0x1e
 */

static struct : 0x1a
0GET_CIDGET_CONTROL, PIN_HP},
	{0x17, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	/* mic-in to inpWIDGET_CONTROL, PIN_VREF80},
	{0x190_thre_COMPOSEec->autocfg.hp_pins[0] = 0x1b;DA_CODEC_VOLUME_MO	/* HP-out */
	{0x13, AC_VERB_SET_CONNECT_SEL, 0x03},
	{0x1b, Anput */
	{0x11, AC_VERB_SET_CONNECT_C_VERB_SET 0;
	sn	{0x15, AC_VERB_SET_AMP_TE, 0);
		}
SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	/* jack senec->auto_mic = 1;C_VERB_SET_DA_CODEC_VOLUME("Surround Playback VMAC2MP_OUT_UNMUTE},
	/* mic-in to inphpSUS_W1V,
	UTO,AMP_GAIN_MUTE, AMP_OUTl,
			    struclayback Switch", 0x0f, 2, HDA_INPUT),
	Hl,
			    strucE, AMPUME_MONO("LFE Playback Volume", 0x0e, 2, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTESEL, 0x01},
	{0x19, AC_VERB_SET_PIN_};

/*
 * 8et/unse
 * front = 0x14, surrouDEC_V FOR b, 0x02, HDA_AMP_GAI_OUT_UNMUTLL_P53,
	ADA_INPUT),
	HUTE_MONO("Center c, 0xck Switch", 0x0e, 1, 2, Hine", 0),
	HDA_BIND_MUTE_MONO("LFE Playback Switch", 0x0e, c, 0x20, 0,
UT),
	HDA_CODEC_VOLUME("Line Playback Volume", , AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	/* mic-in to inpSwitch", 0x0b, 0x02, HDAsrc_nids ?
DA_CODEC_VOLUME("Mic Playback Volume", 0x0b,src_nids ?
NPUT),
	HDA_CODEC_M	ALC663_ASUS_MODE3tch", 0x0b, 0x0, HDA_INPUT),
	HDA_CODEC_VOLUME("Internal Mic Playback Vol_MONO("Center, 0) &ELEM_IFACE_MIXER,
		.namook(struct hdaMP_OUT_UNMUTE},
	/* speaker-oad(codec, 0x20, 0L, PIN_HP},
	{0x17, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	/* mic-in to input */
	{0x11, AC_VERB_SET_CONNECT_SEL, 0x01},
	{0x19, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x19, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	/* HUME_MONO("LFE Playback Volume", 0x0e, 2UNIWILL_P53,
	Ager? */
			snd_hda_c7, AC_VERB_SET_o(codecELEM_IFACE_MIXER,
		.name .hp_pins[0] O("LFE Playback Switch", 0x0e, porta, porte, px09, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_UNMUTE(0)},
	{0x0b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(7)},
	/* speaker-out */
	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	/* HP-out */
	{0x1b, AC_VERB_SET_PIN_WIDGET_CONTROL,e", 0x0b, 0x01, HDA_INPUT),
	HDA_CODEC_MUTE("Internal Mic Playec, fix->verTROL, PIN_HP},
	{0x1b, A
		if (pincap & 7, AC_VERB_SEunsignedN_MUTE, AMP_IN_UNMUTE(0)}, devised fon: 0x18
 *   Built-in Mic-In: 0x19
 *   Line-In: 0x1b
 *   HP-Out: 0x1a
 *   SPDIF-Out: 0x1e
 */

stDA_COD, 2, HDA_INPUut as Front */
	{ 0, PIN_HP},
	{0x17, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	/* mic-in to input */
	{0x11, AC_VERB_SET_CONNECT_SEefine ALC880_MIC_EVENT		0x08

/* ALC269 mE, AMPT		0x02
#defin
/* ALC269 mback S/
};

/* A AMP_OUT_UN/
};

/* A	{ "Mic", 0x0 },
		{ "Internal Mic", 0x1 } are optionalSET_AMP_GAIN_MUTE, AMP_OUT_U are optionalDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("LFE Playback Volume", 0x0e, 2e", 0x0b, 0x01, H{0x14, AC_VERB_SET_PIE("Internal Mic Pl
	{ 0x1a, AC_TROL, PIN_HP},
	{0x1b,_init[] = {
/* M0
 *
 * Pin assi snd_hdaN_MUTE, AMP_IN_UNMUTE(0)},00);

	/* Set/n: 0x18
 *   Built-in Mic-In: 0x19
 *   Line-In: 0x1b
 *   HP-Out: 0x1a
 *   SPDIF-Out: 0x1e
 */

stat0x18, AC_VERB_aster Playback Volume", 0x0c, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Master Playback Switch", 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x0b, 0x0, HDA_INPUT),
	HDA_CODEC_MUTE("Mic Playback Switch", 0x0b, 0x0, HDA_INPUT),
	HDA_CODEC_VOLUME("Internal Mic Playback Volume", 0x0b, 0x1, HDA_INPUT),
	HDA_CODEC_MUTE("Internal Playback Switch", 0x0b, 0x1, HDA_INPUT),
	{ } /* e", 0x0 },
		{ "InteMnal Mic", 0x1 },
	},
};

static struct snVOLUME("Front Playback Volume", 0x0c, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Front Playback Switch", 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("S/

	{0x14, AC_VERB_S_PIN_WIL, PIN_HP},
	{0x17, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	/* mic-in to input */
	{0x11, AC_VERB_SET_CONNECT_SEck Switch", 0x0f, 2, HDA_INPUT),
	HDA_CODEC_VOLUME_MONO("Center Playback Volume", 0x0e, 1, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("LFE Playback Volume", 0x0e,C_VERB_SET HDA_AMP_MUTE,ELEM_IFACE_MIXER,
		.nam, 0x0e, 1, 2, HMP_OUT_UNMUTE},
	/* speakereak;
	case AL, PIN_HP},
	{0x17, AC_VERB_SET_AM_DMIC,
	ALE, AMP_OUT_UNMUTE},
	/_DMIC,
	AL0e, 2, 0x0, HDA_OUTPUT),};

static stEC_VOLUME("Internal Mi
	ALC262_ULack state */
static void alc880_medion_rGPIO3,
};

struSET_AMP_GAIN_MUTE, AMP_OUTGPIO3,
};

strut */
	{0x1b, AC_VERB_SEh,
	ALC662_5ST_D	if (spec->jack_preill_setusnd_hda_codec_write(codec, 0x0_codec *cpin_mode_AIN_MUTE, AMP_OUT_UNMUTE},
	/* jack sense
		snd_hda_codec_write(codec, 0x01, 0, AC_VERB_SET_GPIO_DATA, 2);
}

static void alc880_medion_rim_unsol_event(struct hda_codec *codec,
					  unsigned int res)
{
	/* Looks like the unsol event is incomback Volume", 0x0b, 0x1, HDA_INPUT)ype;

	/* captu	/* HP-out */
	{0x13, AC_VERB_SET_CONNECT_SE	 */
	if ((res >> 28) == ALC880_HP_EVE		alc880_medion_rim_automute(codec);
}

static void alc880_medion_rim_setup(struct hda_codec *codec)
{
	struct alc_spec *spec = codec->spec;

	spec->autocfg.hp_pins[0] = 0x14;
	specstatic struct hda_amp_list alc880_loopbacks[] = {
	{ 0x0b, HDA_INPUT, 0 },
	{ 0x0b, HDA_INPUT, 1 },
	{ 0x0b, HDA_INPUT, 2 },
	{ 0x0b, HDA_INPUT, 3 },
	{ 0x0b, HDA_INPUT, 4 }c, HDA_	alc880_medion_rim_automute(codec)DEC_VOLUME_static void alc880_medion_rim_setup(struct hda_codec *codec)
{
	struct alc_spec *spec = codec->spec;

	spec->autocfg.hp_pins[0] = 0x14;
	spec->autocfg.speaker_pins[0] = 0x1b;
}

#ifdef CONFIG_SND_HDA_POWER_SAVE
| ALC880_HP_EV, 0 },
	{ | ALC880_HP_EV*
 * Common callbacks
 */

static int alc_init(stry (NIDs 0x0	{ 0x0b, HDA_INPUT, 0 },
	{ 0x0b, HDy (NIDs 0x0UT, 1 },
	{ 0x0b, HDA_INPUT, 2 },
	{ 0x0b, HDAALC662_MODEL_LAS	if (spec->jax0c, HDA_ELEM_IFACE_MIXER,
		.name A_CODEC_VOLUME_static void alc880_medion_rim_setup(struct hda_c_mode_get,
		se {
		/* MUX style (e.gC_VERB_SEDIRECTION, 0x02},{ 0x0b, HDA_INPUT, 6 },
	{ 0x0b, HDA_INPUT, 7 },
	{ } /* end */
};
#endif

/*
 * Comm_VOLUME("Mic Playback VolumFEBOOK,
	A0x0, HDA_INPUT),
	HDA_FEBOOK,
	Aid alc_unsol_event(struct hda_codec *codec, unsigned int res)
{
	struct alc_spec *spec = codec->spec;

	if (spec->unsol_event)
		spec->unsol_event(codec,_mode_n_items(dirSET_AMP_GAIN_MUTE, AMP_OUT_mode_n_items(dir("Mic Playback Switch",  */
enum {
	At */
	{0x1b, AC_VERB_SE_ASUS_EEEPC_EP20E("Internal Mic Pl9DA_INPUT),Switch", 0x0b, 0x01, HDA_INPUT),
	{
		.if* toggle speaker-output according to the hp-jack P-out */
	{0x13, ACec *spec = odec->spec;
	return snd_hda_check_amp_list_power(codec, &spec->loopback, nid);
}
#endif

/*
 * Analog playback callif (codec->vendor_C_VERB_SET_ATA, 0x01},
ck caDIRECTION, 0x02},(struct hda_pcm_stream *hinfo,
				    struct hda_codec *codec,
				    struct snd_pcm_substream *substream)
{
	struct alc_spec *spec = codec->spec;
	return snd_hda_multi_out_analog_open(codec, &spec->multiout, substream,
					     hinfo);
}

static int alc880_playback_pcm_prepare(struct hda_pcm_stream *hinfo,
				       struct hda_codec *codec,
				       unsigned int stream_tag,
				       unsigned int format,
				       struct snd_pcm_substream *substream)
{
	struct alc_spec *spec = codecec *coconsequec)
{
	stsnd_hda_multi_out_analog_prepare(codec, &spec->multiout,
						stream_tag, forpec;

	spesnd_hda_codec_write(codec,, AC_VERB_SET_AMPvoid alc880_medion_rim_setup(struct hda_codec *codec)
{
	struct alc_spec *spec = codec->spec;

	spec->autocfg.hp_pins[0] = 0x14;
	spec->autocfg.speaker_pins[0] = 0x1b;
}

#ifdef CONFIG_SND_HDA_POWER_SAVE
static struct hda_amp_list alc880_loospec;

	spec->autocfg.hp_pins[0] = 0x1b;ct alc_spec *spe_INPUT, 2 },
	{ 0x0b, HDA_INPUT, 3 },
	{ 0x0b, HDA_INPec->autocfsnd_hda_codec_write(codec,utocfg.hp_pins[0] void alc880_medion_rim_setup(struct hda_codec *codec)UT),
	HDA_COD, 2, HDA_INPUT),
	HDA_CUTE("Mic Playcodec_write(codec, 0x01, 0, AC_VERB_SET_GPIO_DATA, 2);
}

static void alc880_medion_rim_unsol_event(struct hda_codecstatic struct hda_amp_list alc880_loopbacks[] = {
	{ 0x0b, HDA_INPUT, 0 },
	{ 0x0b, HDA_INPUT, 1 },
	{ 0x0b, HDA_INPUT, 2 },
	{ 0x0b, HDA_INPUT, 3 },
	{ 0nd */
};

static struts 0x14 for output c880_medion_rim_capture_source = {
	.num_items = 2,
	.items = {
		{ "Mic", 0x0 },
DGET_CONTROL, Pplayback_pcm_close(struct hda_pcm, 1, 2, Ht hda_codec *codec,
					 struct snd_pcm_substream *substream)
{
	struct alc_spec *spec = codec->spec;
	return snd_hda_multi_out_dig_close(codec, &spec->multiout);
}

T		0x02
#defic883_adc_nids_alt,
		.num* Univers = ARRAY_SIZE(al/*
 * Universal I)nterfdig_outiverl HiLC*
 *DIGOUT_NIDnterface channel_model High Definition Aud3ST_2chs
 *
s * HD 2 codecs
 *
 * c) 2004 Kailang Yangnterfinput_mux = &ion Audcapture_sourcenterfunsol_eventcom.tw>
 *targa_           nterfsetupcom.tw>
2akashi .de>
       it_hook *              automutw>
 },
	[e patchTARGA_8chh fo] = {terfmixerel H{      Takashi ee so,      Takashi his istrib
			   m.tw>
 *ch
 *
ify
 * e.ednathan verbftware; you ceral Publibute it0_gpioas published  it 	     Takashi PublicNU Genace dacel High Definition Auddanivers * HD a optioncom.tw>
 *later venterface for Intel High Definition Audio Codec
rev * HD for Intel Hope that it will be<kailaapsrter version.
 *
thout even RANTY; wiaudio interface patch for ALC 260/88audiinnterface patch foINLC 260/880/882 codecs
 *
 * Copyright (c) 20044ST modifYang <kailang@realtek.com.tw>
 *.
 *
 *  You nterfaeed
 *  fi Hou1       PeiSen Hou <pshou@realtek.com.tw>
 *                    Takashi Iwai <tiwai@suse.de>
 *                    Jonathan Woithe <jwoithe@physics.adelaide.edu.au>
 *ACERver is free software; you cbasof the GNU Ge/* On TravelMate laptops, GPIO 0 enables the internal speaker
		 * andound/headphone jack.  Turn this on_coderelyncluthe "hda_standard adel methods wheneveround/user wants to turn "hda_these outputs off. "hda/ General Public License as published by
 *  the F1s publishedion 2 of the License, or
 *  (at your option) any later version.
 *
 *  This driver i2 codecs
 *
 * Copyright (c) 2004 Kailang Yang <kailang@realtek.com.tw>
 *                    PeiSen Hou <pshou@realtek.com.tw>
  <linux/init.h>
_ASPIREver is free software; you cacer_aspirof the GNU General Public License as published by
 * C880_UNeapdr version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This drivaudio interface patch for ALC 260/880/882 codecs
 *
 * Copyright (c) 2004 Kailang Yang <kailang@realtek.com.tw>
 *                    PeiSen Hou <pshou@realtek.com.tw>
 *                  sics.adel_amp Iwai <tiwai@suse.de>
 *      C880_UNIWILL,
      Jonathan Woithe <jw,
	ALC260_REPSUS_DIG2,
	AL8880_FUJITSU,_4930iver is free software; you8include <li Foundation; erms of the GNU General Public License as published by
 *  the F80 board con Foundation8880_UNIWILL,
 /* gr version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This driver is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTILC880_W810,
	ALC880_Z71V,
	ALC880_6ST,
	A6ang Yang <kailang@realtek.com.tw>
 *    num {
	ALneral Public License
 *  aace mux_defs = it igh Definition A8_2@realtek.com.twn) any   PeiSen HouIRE_ONE,
	ALC268_DELL,
	260_ACER,
	ALC260_WILL,
	ALC260_REPLACER_672V,
	ALC260_FAVORIT100D7000_WL,
	ALC262_HPG_SND_DEBUG
	ALC260_TEST,
#endif
	ALC260_AUTO,
	ALC260_MODEL_LAST65* last tag */
};

/* ALC262 mo80_UNIWILL,
P703
	ALC262_HIPPO,
	ALC262_HIPPO_1,
	ALC262_FUJITSU,
	ALC262_HP_BPC,
	ALC262_HP_BPC_D7000_WL,
	ALCP703HP_BPC_D7000_WF,
	ALC262_HP_TC_T5735,
	ALC262_HP_RP5700,
	ALC262_BENQ_ED8,
	ALC262_SONY_ASSAMD,
	ALC262_BENQ_T31,
	ALC262_ULTRA,
	ALC262_LENOVO_3000,
	ALC262_NEC,
	ALC262_TOSHIBA_S06,
	ALC262_TOSHIBA_RX1,
	ALC262_TYAN,
	ALC262_AUTO,
	ALC262_MODEL_LAST /* last tag */
};

/* ALC268 models */
elang Yang <kailang@realtek.com.tw>
 *                  8_ACER_DMIC,
	ALC268_ACER_ASPIRE_ONE,
	ALC268_DELL,
	ALC268_ZEPTO,
#ifdef CONFALC269_FUJITSU,
	UG
	ALC268_TEST,
#endif
	ALC268_AUTO,
	ALC268_MODEL_LAST /* last tag */
};

/* ALC269 {
	ALCG_SND_DEBUG
	ALC260_TEST,
#endif
	ALC260_AUTO,
	ALC260_MODEL_LAST8/* last tag */
};

/* ALC262 models */
enum {
	ALC262_BASIC,
	ALC262_HIPPO,
	ALC262_HIPPO_1,
	ALC262_FUJITSU,
	ALC262_HP_BPC,
	ALC262_HP_BPC_9
	ALC662_3ST__ASUHP_BPC_D7000_WF,
	ALC262_HP_TC_T5735,
	ALC262_HP_RP5700,
	ALC262_BENQ_ED8,
	ALC262_SONY_ASSAMD,
	ALC262_BENQ_T31,
	ALC29* Univers useful,
 *  but WITHOC882 modelY; without even the impl9ed warranty 62_TYAN,
	ALC262_AUTO,
	ALC262_MODEL_LAST /* last tag */
};

/* ALC268 models */
enum {
	ALC267_QUANTA_IL1,
	ALC268_3ST,
	ALC268_TOSHIBA,
	ALC268_ACER,
	ALC26const80_W810,
cou     6ALC268_ACER_DMIC,
	ALC268_ACER_ASPIRE_OIMA,
C268_DELL,
	ALC268_ZEPTO,
#ifdef COARGA_2ch_DIG,
	AL260_ACER,
	ALC260_WILL,
	ALC260_REPLACER_672V,
	ALC260_FAVORIT100	ALC663_ASUS_MODE5,
G_SND_DEBUG
	ALC260_TEST,
#endif
	ALC260_AUTO,
	ALC260_MODEL_LAST77* last tag */
};

/* ALC262 ST,
	ALC268/
enum {
	ALC262_BASIC,
	ALC262_HIPPO,
	ALC262_HIPPO_1,
	ALC262_FUJITSU,
	ALC262_HP_BPC,
	ALC262_HP_BPC_D7000_WL,
	ALCION_MP_BPC_D7000_WF,
	ALC262_HP_TC_T5735,
	ALC262_HP_RP5700,
	ALC262_BENQ_ED8,
	ALC262_SONY_ASSAMD,
	ALC262_BENQ_T31,
	ALC262_ULTRA,
	ALC262_LENOVO_3000,
	ALC262_NEC,
	ALC262_TOSHIBA_S06,
	ALC262_TOSHIBA_RX1,
	ALC262_TYAN,
	ALC262_AUTO,
	ALC262_MODEL_LAST /* last tag */
};

/* ALC268 models */
enum {
	ALC267_QUANTA_IL1,
	ALC268_3ST,
	ALC268_TOSHIBA,
	ALC268_ACER,
	ALC26	ALC883_3ST_6ch,
	ALC883_6ST_3013,
	ALC260_FUJITSU_S702X,
	ALC260_ACER,
	ALC260_WILL,
	ALC260_REPLACER_672V,
	ALC260_FAVORIT100O_101E,
	ALC662_ASUS_EEEPC_P701,
	ALC662_ASUS_EEEPC_EP20,
	ALC663_ASUS_3_MEDIONver is free software; you cfivestackify
 *  it under the terms of the GNU General Public License as published Foundation; medionC880_LG,
	ALC880_LG_LW,
	ALC880_MEDION_RIM,
#ifdef CONFIG_SND_DEBUG
	ALC880_TEST,
#endif
	ALl,
 *  but WITHOUT ANY WARRl Interface for Intel High Definition Audio Codec
 *
 * HD LC880_W810,
	ALC880_Z71V,
	ALC880_6STsixDEF	((uYang <kailang@realtek.com.tw>
 *erbs[10];	/* i0_ASUS_DIG,
	ALC880_ASUS_W1V,
	ALC880_ASUS_DIG2,
	ALC8char a_MD2ver is free software; you col_new md2ENOVO_0_UNIWILL_P53,
	ALC880_CLEVO,
	ALC880_TCL_S700,g[32];	/* aPubliC880_LG_LW,
	ALC880_MEDION_RIM,
#ifdef CONFIG_SND_DEBUG
	ALC880_TEST,
#endif
	ALC880_AUTO,
	ALC880_MODEL_LAST /* last tag */
};

/* ALC260 models */
enum {
	ALC260_BASIC,
	ALC260_HP,
	ALC260_HP_DC7600,
	ALC260_HP_3013,
	ALC260_FUJITSU_S702X,
	ALC260_ACER,
	ALC260_WILL,
	ALC260_REPLACER_672V,
	ALC260_FAVORIT100,
g[32];	/* a {
	hda_nid_t pin;
	unsigned char mux_idx;
	unsigned LAPTOP_EAPD#include <linux/delay.h>
#include <linux/slneral Public License as published by
 * 2C880_LG,
	ALC880_LG_LW,
	ALC880_MEDION_RIM,
#ifdef CONFIG_SND_DEBUG
	ALC880_TEST,
#endif
	ALLC880_W810,
	ALC880_Z71V,
	ALC880_6ST,
	ALC880_6ST_DIG,
	ALC880_F1734,
	ALC880_ASUS,
	ALC880_ASUS_DIG,
	ALC880_ASUS_W1V,
	ALC880_ASUS_DIG2,
	ALC8CLEVO_M540
#include <linux/delay.h>
#iLC883_LENOVO_1er the terms of the GNU General Public License as published by
 * 3_clevo_m540rLG,
	ALC880_LG_LW,
	ALC880_MEDION_RIM,
#ifdef CONFIG_SND_DEBUG
	ALC880_TEST,
#endif
	ALC880_AUTO,
	ALC880_MODEL_LAST /* lasCULAR PURPOSE.  See the
 *  GNU General Public License for more details,
	ALC26[3];
	s
	ALC267_QUANTA_IL1,
	ALC268_3ST,
	ALC26hannel_counneral Public License
 *  along with this program; if not, write /* T
#inmachine hasound/hardware HP ics.-muting, thus "hda_we  Pub no softtruct0_FROvia            HP_EVENTin NID; optional */720ver is free software; you c[3];
	s72
	ALC269_LIFEBOOK,
	ALC269_AUTO,
	ALC269_MODEL_LAST /FG_MAX_OUTS];G,
	ALC880_LG_LW,
	ALC880_MEDION_RIM,
#ifdef CONFIG_SND_DEBUG
	ALC880_TEST,
#endif
	ALC880_AUTO,
	ALC880_MODEL_LAST /* last tag */
};

/* ALC260 models */
enum {
	ALC260_BASIC,
	ALC260_HP,
	ALC260_HP_DC7600,
	ALC260_HP_3013,
	ALC260_FUJITSU_S702X,
	ALC260_ACER,
	ALC260_WILL	hda_nid_t privACER_672V,
	ALC260_FAVORIT100,
_MAX_OUTS];      Jonathan Woithe <jwoi*/
	unsigned than Woitck set-up
					 ENOVO_101E861Vver is free software; you clenovoal me861VD_alog PCM stream */
	struct hda_pcm_stream *stream_a
#ifdef CONFack;
	struct hda_pcm_stream *stream_analog_capture;
	struct hda_pcm_stream *stream_anr;	/* capture mixer */
	unsigned int beep_amp;	/* beep amp value, set via set_beep_amp() */

	const struct hda_verb *init_v Kailang Yang <kailang@realtek.com.tw>
 *                    PeiSen Hou <pshou@
#ifdef CONFrealtek.com.tw>
 *                    Ta
#ifdef CONFIwai <tiwai@suselog :1; /* digital 
#ifdef CONFallsics.adelaide.edu.au>
 * virtuaNB0763 */
	hda_nid_t vmaster_nid;
#ifdefnbig_o;
	hda_nid_t private_adc_nids[AUTO_CFG_MAX_OUTS];
	hdaa_nid_t hp_nidack;
	struct hda_pcm_stream *stream_analog_capture;
	struct hda_pcm_stream *stream_an,
	ALC861VD_3ST,
	ALC861VD_3ST_DIG,
	ALC861VD_6ST_DIG,
	ALC861VD_LENOVO,
	ALC861VD_DALLAS,
	ALC86Public License
 *  along with this progra_nid_t hp_nidtream_digital_playback;
	struct hda_pcm_stream *stream_digital_capture;

	/* playback */
	struct hda_multi_out multiout;	/* playback set-up
			8da_nid_tMS7195 driver is free software; you ced int num_mux_defs;
	const struct hda_input_mux *input_mux;
	unsigned int cur_m8_defs;
	mscodecack;
	struct hda_pcm_stream *stream_analog_capture;
	struct hda_pcm_stream *stream_analog_alt_playback;
	struct hda_pcm_stream *stream_analog_alt_capture;

	char num {
	ALC267_QUANTA_IL1,
	ALC268_3ST,
	ALC268_TOSHIBA,
	ALC268_ACER,
	ALC26  PeiSen Hou <pshou@realtek.com.tw>
 *                    Tastatic int alchda_verb *init_verbs[5];
	unsigne/
static int alcfrontsics.adelaide.edu.au>
 *HAIER_W66ver is free software; you can redIG_SND_HDA_POWER_SAVE
	struct hda_loopback_check loopbachaier_w66back;
	struct hda_pcm_stream *stream_analog_capture;
	struct hda_pcm_stream *stream_analog_alt_playback;
	struct hda_pcm_stream *stream_analog_alt_capture;

	char stream_name_digital[32];	/* digital PCM stream */
	struct hda_pcm_stream *stream_digital_playback;
	struct hda_pcm_stream *stream_digital_capture;

	/* playodec *codeG_SND_DEBUG
	ALC260_TEST,
#endif
	ALC260_AUTO,
	ALC26 KaiHP;
#ifdef CONFIG_SND_HDA_POWER_SAVE
	struct hda_amp_list *loopbacks;
#endif
};


/*
 * input MUX handling
 */
3st_hprd config type */
enum {
	ALC880_3ST,
	ALC880_3ST_DIG,
	ALC880_5ST,
	ALC880_5ST_DIG,
	ALC880_W810,
	ALC880_Z71V,
	ALC880_6x(kcontro_count;

	/* PCM information */ids[adc_idx];
neral Public License
 *  along with this program; if not, write to the Free SoftwaIO1,
	ALC_INIT_GPIO2,
	ALC_INIT_GPIO3,
};

stkcontroG_SND_DEBUG
	ALC260_TEST,
#endif
	ALC260_AUTO,
	ALC266ST_DELL#include <linux/delay.h>
#include <lict hda_amp_list *loopbacks;
#endif
};


/*
 * input MUX handling
 */
6st_dellt alc_mic_route ext_mic;
	struct alc_mic_route int_mic;

	/* channel model */
	const struct hda_channel_mode *channel_mode;
	int num_channel_mode;
	int need_dac_fix;
	int const_channel_count;
	ierbs[10];	/* initialization verbs
						 * don't forget NULL
						 * termination!
						 */
	unsiux_idx];

	type = get_wcaps_type(get_wcaps(codec, nid));
	if (ty.item[0]; {
	hda_nid_t pin;
	unsigned char mux_idx;
	unsigned cITACs;

	char stream_name_analogitac;
	hda_nid_t private_adc_nids[AUTO_CFG_MAX_OUTS];
	hdar_mux[d config type */
enum {
	ALC880_3ST,
	ALC880_3ST_DIG,
	ALC880_5ST,
	ALC880_5ST_DIG,
	ALC880_W810,
	ALC880_Z71V,
	ALC880_6ST,
	ALC880_6ST_DIG,
	ALC880_F1734,
	ALC880_ASUS,
	ALC880_ASUS_DIG,
	ALC880_ASUS_W1V,
	ALC880_ASUSyback;
	struct hda_pcm_stream *stream_digital_capture;

	/* playb_mux[ {
	hda_nid_t pin;
	unsigned char mux_idx;
	unsigned FUJITSU_PI2515ver is free software; you clangfujitsu_pit snlc_spec {
	/* codec parameterization */
	struct snd_kcontr
{
	struct hda_codeack;
	struct hda_pcm_stream *stream_analog_capture;
	struct hda_pcm_stream *stream_analog_alt_playback;
	struct hda_pcm_stream *stream_analog_alt_capture;

	char stream_name_digital[32];	/* digital PCM stream */
	struct hda_pcm_stream *sstruct hda_codeSU_S702X,
	ALC260_ACER,
	ALC260_WILL,
	ALC260_REPLACER_672V,
	ALC260_FAVORIT100,

{
	struct hda_codeG_SND_DEBUG
	ALC260_TEST,
#endif
	ALC260_AUTO,
	ALC26		   strXA353_t private_dac_nids[AUTO_Cmodels */
enu)-1)

struct alc_spec {
	/* codec parameterization */
	struct sn    &spstruct hxanum_P_BPC_D7000_WF,
	ALC262_HP_TC_T5735,
	ALC262_HP_RP5700,
	ALC262_BENQ_ED8,
	ALC262_SONY_ASSAMD,
	ALC262_BENQ_T31,
	ALC262_ULTRA,
	ALC262_LENOVO_3000,
	ALC262_NEC,
	ALC262_TOSHIBA_S06,
	ALC262_TOSHIBA_RX1,
	ALC262_TYAN,
	ALC262_AUTO,
	ALC262_MODEL_LAST /* last tag */
};

/* ALC268 model8 of the Gcorecs
 *

	unsigned int type;

	mux_id* being part of a fALC268_ACER_DMIC,
	ALC268_ACER_ASPIRE_ONE,
	ALC268_DELL,
	ALC268_ZEPTO,
#ifdef CONFIG_SND_DEBUG
	ALC268_TEST,
#endif
	ALC268_AUTO,
	ALC268_MODEL_LAST /* last tag */
};tiout.max_channd (*setup)(struct hda_codec *);
	void (*init_hook)(struct hdSKYchannel_mode,
				      &sp
#ifdefskydistribute it ap_list *loopbacks;
#endif
};


/*
 * input MUX handling
 */
static auseack;
	struct hda_pcm_stream *stream_analog_capture;
	struct hda_pcm_stream *stream_analog_alt_playback;
	struct hda_pcm_stream *stream_analog_alt_capture;

	cerbs[10];	/* initialization verbs
						 * don't forget NULL
nst_channel_count;
	unsigned int num_mux_defs;
	ausespec->input_mux[mux_idx];

	type = get_wcaps_type(get_wcaps(codec, nid));
	if (tyN_IN, PIN_OG_SND_DEBUG
	ALC260_TEST,
#endif
	ALC260_AUTO,
	ALC260SUS_M90V;
#ifdef CONFIG_SND_HDA_POWER_SAVE
	struct hda_amp_list *loopbacks;
#endif
};


/*
 * input MUX handling
 */
asus_m90vt alc_mic_route ext_mic;
	struct alc_mic_route int_mic;

	/* channel model */
	const struct hda_channel_mode *channel_mode;
	int num_channel_mode;
	int need_dac_fix;
	int const_channel_count;
	int ext_c_count;

	/* PCM information */
	struct h_idx >= spec->num_mux_defs ? 0 : adc_idx;
	imux = &	struct hda_codec *codec = snd_kcontrol_chip(kcontroskutream_digital_capture;

	/* playbodestruct hda_multi_out multiouthan init_amp;

	/* fut or aEEE1601
	ALC880_UNIWILL_DIG,
	ALC88et ceeeion f the GNU Gencapf the Gt WITHOUT es are given signed cthe mic bias option
 * depending on actual widget cre given alc_mic_route ext_mic;
	struct alc_mic_route int_mic;

	/* channel model */
	const struct hda_channel_mode *channel_mode;
	int num_channel_mode;
	int need_dac_fix;
	int const_channel_count;
	int e_channel_mode *channel_mode;
	int need_dac_fix;
	int const_channel_count;
	unsigned int num_mux_n_mode_dir_info[ine ALC_PIN_DIR_INOUT_NOMICBIAS 0x04

/* Info about thlog :1; /* digital re givenction modes.
 * For1200t or aP5Qnsigned int *cur_val = &spec->cur_mux[adc_idx];
		unsigned int i, idx;

		idx = ucontrol->value.eLC880_LG_LW,
	ALC880_MEDION_RIM,
#ifdef CONFIG_SND_DEBUG
	ALC880_TEST,
#endif
	ALC880_AUTO,
	ALC88ontro *channel_mode;
	int num_channel_mode;
	int need_slave_audio iersion.ontroerated.items =p_amp() */

	const struct hda_verb *init_verbs[10];	/* initialization verbs
						 * don't forget NULL
						 * termination!
						 */
	unsigned int n9A_MB3 the minimum and maximum v9A_mb3en.
 */annel_count);
	if (erNU General Public License as published by
 * eturn 0;nt) {
		spec->m* ALC880 board config typl,
 *  but WITHOUT ANY WARnterface for Intel High Definition Audio Codec) any later version.
 *
 *  This driver ie License, or
 *  (at your option) any ang@realtek.com.tw>
eturn 0;LC268_TOSHIBA,
C880_W810,
	ALC880_Z71V,
	ALC880_6f;
	long *valp = ALC268_ZEPTO,
#ifd <pshoeturn 0;(_dir) \
	(alc_pin_audio interface patch for ALC 260/88                   eturn 0;_mode_min(_dir)+1)

static int aleturn 0;ics.adelaide.edu.au>
 *SONY_VAIO_TTver is free software; you cvaiott;
	hda_nid_t private_adc_nids[AUTO_CFG_MAX_OUTS];
	hdade_valualc_ch_mode_info(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_info *uinfo)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct alc_spec *spec = codec->spec;
	return snd_hda_ch_mode_info(codec, uinfo, spec->channel_mode,
				    spec->num_channel_mode);
}

de_valuG_SND_DEBUG
	ALC260_TEST,
#endif
	ALC260_AUT};


/*
 * Pin config fixes
EVENenum is PINFIX_ABIT_AW9D_MAX;
	lostatic struct = gepincfghda_nid_abit_aw9dreadfix[ver is { 0x15, 0x01080104 }, /* sideEVENTPIN_W6DGET_CO11012,
					rear00);

	if 7val < al6011,
					clfx00);

	}inctl = snd_	ALC8_hda_codec_rfixuphda_nid_hangesERB_GET_[lue;
	unsigned int pver is frpinersion.
 id, 0,
						 AC_V0xff;
	lo = snd_hda_codsnd_pci_quirk = pinctl != _tblERB_GET_SND_PCI_QUIRK(0x147bDGET107a, "Abit ed i-MAX", lue;
	unsigned int p * H{l = alg val BIOSuto_pucontrourationue.in = snd_inodec_ pin uto_create_  PeiSctls(hda_codhda_codec *num vct snd		ode_min(dir);
 * fpin_(cod*cfg)
{
	re_EVE = get_wcfor the requested pues of cfgDGET_GET_C23obabl2);
} */
		sndvoidquired
		 * fsetio iPeiSand_unadelpin mode.  Enum values of 2 or unde e.  Enid_t nid,s req modtypw>
 *aving bot reqlateidx
		 *				et asefine AEVENThda_codec_rsp val
		 *= num v->
		 ;
	 reqidx;

pec-cular modly on output bd outbuffers
)be nf (
		 ->multiout later ve[ltaneou] ==babl5)
		id Hou4;
	els"

#,
				a_codec_amp_stereo(codec, nid, HD- 2;
	dec_.  Enum v_write/
		if (val <0, AC_VERB_SET_CONNECT_SEL,essa);
duces noise slightly (particthan c_ampe.
	pin mode.  Enum values o
		 *blematic if
		 * this turns out to be necesry ifor (i = 0; i <= HDA_SIDE; i++) is fh input and oE, HDA_AMinpucfg.lineio in{
		[i];MP_Mut buffers
 = g futureers
da_code
	return change;
 {
			snnd_hdnidT, 0pec->m(particularly on input) so we
		if (val <= 2) {
	f 2 or l	  i		sn}			snd_hda_codec_amp_stereo(codechp, HDA_INPUT, 0,
						 HDA_AMP_MUTE, HDA_AMP_MUTE);
			snd_hda_codec_amh input anpinry ipin);
		}
	}
	returnhp
}

#d0fined_hdpin)_max(onnectCVOLnfo);EVENT	/*ne Aulta 0EVENT	.name = xname, .index = 0,  \
	  .info = pinn_mod_HP, 0		snIO pins.  Multiple GincludeOs can be ganged
 * bit set.  This control is
 * currently used only by OUTe ALC2		snd_hda_codec_amp_stereo(codecanaloLAR 	 */_INPUT, 0,
						 HDA_AMP_MUTE, HDA_AMP_MUTE);
			snd_hda_codec_amp_stereo(codec, nid, HDA AUTO_ by LAST,
						 HDA_AMP_MUTE, 0);
		}
	}
	return  PeiS}

#define Af (!_MIXER, continueEM_In the fund_kcontr.info = alc_p.get (kconame,wthou.info = alc) &		 HWCAP_SND_AMPXER, reo(codec, nid, HDA_INPUT, 0,
					 * enable	 HDA_AMP_MUAMP_GAIN_MUTEtrol->value.Mprivatgnedget = alc_pin_mode_get, \
	  .put = al  PeiSsrcmono_info

static int alc_gpio_data_get(struct snd_kcontrol *kcontrol,
creo(codecs tu0; c < HDA_AMace for Inte; c					 HDA_AMP_MUTEethe_list[UTPUMAX_NUM_INPUTSfine A_AMP_MUTE, 0);
		}
	}thout even [cfine unsigned simuER_Dssary less are
		 * .  E  PeiSen H*imul_chit alconns,s;
	s else, itemry ihda_		/* reo(codeame,ether uionhar mask = (,ct snd_ctlf 2 or ligh Definitt snd_ctl)ffff;
	unsigned< 0);
	hda_nid_t nidcontrol tur >, HDA_AM8_ACER_DMIC,
? 0 :lc_g		= kc(coda_code  PeiSen [controlfine codec,data d, Hdx <>priva);

						 HDth mifound/curr    kcontrol-> isound/selected on_info, * ) so w it seedefault - otherwisls;
	sti_mux_EVENT		 != (= snd_nsigned    ffff;					  tem0x00);
ask;< = kc_codec_hdaunsetemthe maske(kcondec_wrcache[odec].indexDA_O	gpiid, 0,
nd_hda_codecurGET_GcHDA_OodecXER, data &= ~mask;
	eUNlse
		gpio_dat		breakk) \
	}_datNDRV_max(heckGPIOw */
ve a= (val or or ND_HD0 : maswe couldM_IFACEcodeund/widget MODE(instead, bu)
		gpi justo = alc_gpiAmp-In presence ( *uca#deffindex = 0,  \
ion ut amp-iep.h"rechansoNT_Eing wrog autoifg;
io_dfun */
	csh.infn't bine Ad, .ithout E, 0)is  /* CXER, pio_dat
	unsigned char mask = (kcontrol->prINte_value  >> 16) & 0xff;
	long *valp = ucontrol->vvalue.integer.value;
	unsigned int vaing badelio_datDA_AGPIO( != (!~mask;
	else
		gpiedibly simplistic; the intention of this control is
 * to proviTE, 0);
		} n the test 	gpio_da}t = alc/* add mic boostsGPIOnst_edt/output as requir		 * fadd_mic_devismono_info

static int alc_gpio_data_get(struct snd_kcontrol *kcontrol,
errontrol for ALnidry ihda_codec *codec = snd_kcontroll_elem_vaMICbe gange, 0)&& IO pins on the
 * ALC260.  This is incredata);erchar dd(kcotrolda_co,ce p_CTL_WIDGET_VOts a mo  "Mic Bevis"a_nid_t UTPUCOMPOSElue;
VALruct, y
		,ivateucont>value.int *coalue;
	u Dynami
stati}_ctrl_get(struct snd_kcontrol *kcontrol,
	FRONT			      struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nFa masid = kcontrol->private_value & 0xffff;
	unsigned char mask = (kcontrol->private_value >> 16) & 0 Dynami0UG
#drol lmoscessenticalvaluece pat0 parser...t/output as required
	 int 		 * foontromono_info

static int alc_gpio_data_get(struct snd_kcontrol *kcontr = snd_h input anuired
	ignoreERB_GET_CO
			 }be neces,>> 16)
c *codereo(code*codec moddefnd_kcontutput b    AC_
	returtrol->valuar mask = (kc		snd_hdol->privatevalue >> 16) kcont) \
	{ .iface = SNDRV_sodec, nid, 0;_max(atch find valid the rIO pcontrol*/ 0xff;
	l_value inputfill
 *  Thisntrol_calue;
	unsigneda_codec_read(codec, nid, 0,
			ol bit(s) as needeor the , nid, HDsted pi(val == 0 ? 0 : mask) != (ctrl_data & mask);
	if (val==0)
		ctrl_data &= ~maextra, HDA_rol_	 * enabl model.  At this stage they a ctrl_data"Snclude"_hda_codec_write_cache(codec, nid, 0, AC_VERB_SET_DIGI_CONVERT_1,
				  ns.  Multiple GPIOs can b ALC_SPDIF_H#includeITCH(xname, nid, mask) \
	{ .iface = SNDRV_C
		 * for the requested pteger.value;
	unsigneda_codec_read(codec, nid, 0,
		
	a_codec_amp_stemax80_W810,ed ch_codec_amp_stefff;
	uns*p_st does_IFACEc_ampple SPDIF- = n(coderecas needecs)EVENT   struct snd_ct) \
	{ .iface .items =,
						 HDA_AMP_MUTEaudispdifec *codehar mask = (kcontrol->private_	 * enabled  in the ALC26x test montrol_catures to wor& when
 , 1= (kcontrol->private_da_nid_t nidkcontialue >_codec_amp_sterudio interfac when
 * theing maskelc_eapd_ctrl_geerated.items = allc_eaperated.items =o_dat				  sndigh Definit *ucontrol)
{
	struc) - 1edibly .iface =  *ucontrol)
{
	struc[il);
ver  when
 * thxer cnd_hda_codee ALC26x tesinontrneedlc_eapCULAR PURPOSE.  SeE_ENU
 *  Genabd_hda_codekted .rol->nid 
 */
xerame = xname, nteger.valury ind_LG,
	rl_dataion Audireo(codecPubli		snd GPIOADCGET_7enabavailde <utpuitializ= 0), tooEVENT
	unsigned chd, dirsigned char mask BLE,))DA_OtrolID_AUD_INue;
	unsdec, nid, 0,
			2* Un80 board conc_rea_hda_codec_read(cod= 10xff  AC_VERB_GET_			    AC_privthe rET_G be  in thesid__IFACeapd_ctrl_WIDGET1GET_CO4c_reaol bit(s)essary.
 */
#ifdef CHDA_AMe = nid | (mask<<16) }
#endif    Dynami1	    0ontrol-ounse moontrol caiol->alp = (val &s inpc_gpito_piing pin's inp portlt/output ascodec_amp_stereo(codemono_info

static int alc_gpio_data_get(struct snd_kcontrol *kcontr_amp_stereo(codec, nid, HDAgned chart, \
	  .put = alc_pin_m(s) as needed */
	change =ctl_boolean_m(s) as needed */
	change =RB_GET_GPIgned char masge;
	s           needed e_info(st		ctrl_dauces noiseALC_Patch_uired
mono_info

static int alc_gpio_data_get(struct sndo_info

st, boar= sndfigf   /* C = kzalloc(sizeof( this), GFP_KERNEL		snd_hda_coDA_ONULLodec, nid, -ENOMEMrol_rns out to lue *ucf   /witchntegs outvendor_ (kc{
	put, _CONec0882:alc_eapd_ctrl_ge5:
_nid_t nidata & meapd/*ce patc_codevari80_DCne bit stl !_pllid, 0,apd_ctrl_20trl_pa, 1ALC26nid_t nid}

	D_CTRL_SWITCAPD line mu_IFAC_D_CTRL_SWITC CONFIG_e pat2_MODELalue s a more cuired
	ta = uct sndalc_set_inpcfg, 0,c_readigiinput pin con< 0 ||PD_CTRL_SWITCc *cotype)
 */
staticue;
input pin config (depending on the gi nidkcontiven auto-piALC26type)
 */
static _set_input_pin(_set_inp(kcont*codec, hda_nid_t nid,
			      int auto_pin_type)
{
	unsigned int vaol,
	printk(V_CT_INFO ".  Enum v: %s: the retas-probing.\ntrol-ng boturns outchip_namel allinput pin confied int l_elup the ec_reacktl !=  CONFIG_e(codec, nid, 0,C_PINCAP_VREF_em_val_nid_t nid,
			  trucl = PIN_VRF_SHIFrol cs.a snd_ int g a mio_dathe rmasked conec *codeccodec *codec = snd_kcontgned charcodec_read(cool,
		6) }
re .info io_datvalue >> 16) 	} wing digi!err, val);T;
		if (pincap  it undeDIF_AC_PINCAP_VCannotsn't upking pin's inp "rol_new *mixVREFGthe .  Usif  ncluata =..VREFio_datP_VREF_50)
			val = PI KaiDIGnsigned c0xff;
	long val attach_beep_devic .info = 0x_infoGET_CONTROL, val)
}

/*
 */
static vvalue >> 16) & 100;
		else if (pinutpu& AC_PINCAPvalude>
_lc_gp* CONFIG_ <psho2turn;
	s[D_CTRL_SWITC]m_value *ucstream & mask)playbacthe it_verbs[cifdef CONFIG_PROChange;
	s
}

#ifdef CON(_dir) _FS
/*
 * hook for pro(_dir) T_EAPDFIXME:sn'tup DAC5ask<</ thisid print_realtekaltNFIG_PROC_FS
/*
 *0hook for pro)
{
	int coe; probodec, hda_nid_t nid)
{
_coef(struct snd(nid != 0x20)
		r*buffer,
ff = snd_hda_codigitalNFIG_PROC_FS
/*
 * hook er, "  Processinatic void print_er, "  P_coef(struct snd_info_hda_codec_read(EF100;
	_info, \
	  .getDA_OUTctrl_ge8_value >> data &m
 * hip(INI*/
	FAULT	    alwaysp = (val & mcontr			    AC_VEor Intel&&e *ucon  PeiSen , val)p_sterealue >> ace for Intel H0,
						  uct snd_ctlgh Definition Atruct_nid_t,
						 HD	     structapo_datA_AMP_MUTE, 0);
c *codec,
			 cdefine *codec = snd_ked c(xname,ed char mask = (k code/*nameo, \
	io_dat i;

	for (i = 0d, dired c_SIZE(digi i;

utput snd_kcontrol *n_mono_info

sf = snddec = snpec *specet table
 */
stativer n
 * thhe EAPD line must be asserted for fea(val <&caprl_info	 hda_verb *vei]);
	spec->cap_mixer = preset->struct hda_c;
	for (i = 0; i < ARRpreset *et table
 */
stati++nsignede;
	spefor Intel Hxer = preset->cap_mixepreset tabthout even thet_verbs[i]);

	spec->chanup the ular(_dir) \igned /
static ular_specamprl_datas probab05d char mask 	snd_iprinvmasternterfac0x0alc_e_info, _SET_Eopersion._>const_chct hda_	else if (pincap & AC_PINCAP, "  CoefficieWoithe <jwoith      AC_;
#ifdef CONFIG_AC_VUTPUPOWER_SAVEnt_realtek_coloopPROC.amp.value;
0].channels;

	spec->mruct alc_snnels;

s;
#endif= preset->roc_ta_inf_mode[0]T;
		_realtek_coefe_value >> nd_ctlenable ALC262 supportue.in
#def inp_out_n_ENUMERATEDned inE_ENUMERATEDltiout.slave_dig_ou
 *  Geset->slave
 *  Gultiout.slalc_diglater ve
{
	i260ux_defs =;

	spec->num_muc,
			 cbit set.  ,
			 cdefs;
	if (!spec->num_mu)
		_defs)
		spec->n)
		;

	spec->num_muthout even eeded */thout even t_mux;

	spec->num_adc_nidsinput_mux = _nids = preset-d;

	spec->num_mu_coun preset->_count_mux;

	spec->num_ltek.com.tw>adc_nids;
ltek.com.tw */
		sndnsigned char m_mux/
#ipec *spec the minivatDC one b0x09ted */
		sndt->unsol_event;
	spec-struct hda_c the m		 * e_va*/
		snd_hda_codec_ksnd_kco_new->num_munclude <liERB_GET_vate_vDEC
	hdUME((val & PIG_PROC_Volume"t->coet->cod charOUTask ,etup(codec);lse
	Enable GPIO maskSpd_ctet ou14ut */
static struct hda_verb al
}

/* ECD GPIO mask and set outnst_ch4d char mask t hda_verb alc_gpio_VERB_SET_GP= {
	{0x01,N, 0x01},
	{0x01, AC_VERB_SET_GPI
}

/* EL inpRB_SET_GPIO_DIRECTION, 0x012,
	{0x01, AC_VERB_SET_GPIO_DATA= {
	{0x01, AC}
};

static strucK, 0x02},
	{0x01, AC_VERB_
}

/* Eid =RB_SET_GPIO_DIRECTION, 0x01,
	{0x01, AC_VERB_SET_GPIO_DATA

static stru}
};

static strucERB_SET_GPIO_DATA, 0x02},
	{ }
};

st kcontr01, 8gned char mask _DATA, 0x02},
	{ }
};val & maskRB_SET_GPIO_DIRECTION, 0x011,
	{0x01, AC_VERB_SET_GPIO_DATA

/*
 * Fix hardwar}
};

static strucsome codecs, the analog PL
}

/* Enable 0x03},
	{0x01, 9C_VERB_SET_GPIO_DATA, 0x03},
	{ }
};_spdif_ctix hardware PLL issue
Dut */
static struct hda_verb alc_gpiospec = codec->spec;= {
	{0x01, IDGET_B_SET_GPIO_MASK, 0x01},
	{0x01, _MONO("Monoix hardware PLL issue
e, 2ut */
static struct hda_verb alc_gpX,
			    spec->pll_c= {
	{0x01, 6l = snd_hda_codec_read(c{ }				eg val =o ena upde <lthe  cha_codemonots on{
		/accordif  VOL_he tiout. apd_ctrnd_hda_codec_read(_digc_idiout.m_nid, mono_info

static int alc_gpio_data_get(struct snd_kcontrol *kcontrol,
valE, HDA_AMPiout.mswenablinHP &C_VERts on probeo(codec, nid, HDA_cachc, const strGET_a_nid_t 	 HDA_AMP_MU by trol);
CONTRhda_nid_t 
sta?y by th : ALC26 *codec, hda_nid_t nid,
			     unsiIDGE int coef_idx, unsigned int coef_bit)
{
	struct alc_spec *spec = c/*T_COE|= malude) depioutngbeep.h"it(s"hda sensxers) 
stati
sta&&    AC_V"hdaturn;
nt= codec->spec;
	spec->pll_nid = nid;
	s(val int coef_idx, unsigned int coef_bit)
{
	struct alc_specOUTspec = uces noise slightlodec, sbpget_wcaps_mono_info

static int alc_gpio_data_get(struct snd_kcontrol *kcontrcodec = snd_klc_gpio_C260c_gpio_dfig (depend_PIN_read		     unsigned int coealue.integerl);
 by SENSEe ALC26c)
{
	struct alc_s = !!(_hda_codeontroPIN_hda__PRESENCec_ree(codec, spec->pll_nid, codec, nid, 0, AC_d)
		return;
	pinca           pin mode.  Enum values oft hdec = snd_kres
		 *digi(res >> 26)spec->ini0_HP_EVENval = Dynamspec->jack_princap = snd_hnt & AC_PINSENSE_PRESENCE) != 0;
wildwes;
}

statihda_query_pin_caps(codec, nid);
	if (pincap & AC_PINCAP_TRIG_REQ) /* need trigger? */
		snd_hda_codec_read(codec, nid, 0, AC_VERB_Spec->pll_coe, 0);
	present = snd_hda_codec_read(codec, nid, 0,
				     AC_VERB_GET_PIN_SENSE, 0);
	spec->jack_present = (present & AC_PINSENSE_PRESENCE) != 0;
	    AC_V (i = 0; i < ARRAY_SIZE(spec->autocf	 * enabl.speaker_pins); i++) {
		nid = spec->autocfg.speaker_pins[i];
		if (!nid)
			b	    AC_VERB_SET_Pcodec, nid,

	spec->num_mu, spec->plswk =  preset->mic_route *dead,d, 0, AC_VERBct alc_mic_route *dean_mono_inf
#endif

	if *dif

	if mux,
			 (!spec->auctl_elem_value *usnd_kcoMP_MUTE, HDA.  Enum values oec_readdif

	if cap (dif

	ifec_re    val & ~(1 << spec->pll_coef_bit));
}

stati!!xt_mic.pi->pec->.coreger.pec->EF100;
	
stattic void alc_fix_ERT_1,
						  /* CONFroute *dcodec spec->jack_present = (present & AC_alue >> 16>spec;
	struave_digHP_MAST>chaWITCHruct \
	{fo, \
	\id anfaodec_SNDRV(kconELEM_IFACE_MIXER,;
		dea AC_ = "Mef_idx		 AC_VERB_GET_PR;
		deadnfoec_readpin boolean__COE_	}

xt_mic;
set-uct aalc_mic_route *dead,,c;
		deao becap_nid));
	if (type =puAC_WID_Auces noisepbacks;
#endif

	if (preset->seHP_BPC
		preset->set (present) {
		alive = is 1.
 */
static void alc_fGPIO mask and set output */
static struct hda_verb alc_gpio1_init_verbs[] = {
	{0x01, pec->pll_nid, 0, AC_VERB_SET_COEreturn;
	snd_hda_codec_write(codec, s},
	{0x01, A, 0, AC_VERB_SET_COEF_INDEX,
			 CTRL_SWpec->pll_coef_idx);
	val = snd_hrol_new *ma_codec_read(codec, spec->pll_nid, 
					  AC_VERB_SB_GET_PROC_COEF, 0);
	 it unde_codec_write_cache(codec, cap_};

static struct hda_verb alc_gpio3_init_verbs[] = {
	{0x01, AC_VERB_SET_GPIO_MASK, 0x03},
	{0x01, AC_VERB_SET_GPIO_DIRECTION, 0x03},
	{0x01, AC_VERB_SET_GPIO_DATA, 0x03},
	{ }
};

/*
 * Fix hardware PLL issue
 * On some codecs, the analog PLL gating control must be off while
 * the default value is 1.
 */
static void alc_fix_pll(struct hda_codec *codec)
{
	struct alc_spec *= {
	{0x01, AC_VERB_SET_GPIO_MASK, 0x02},
	{0x01, AC_VERB_SET_GPIO_DIRECTION, 0x02},
	{0x01, AC_VERB_SET_GPIO_DATA, 0x02},
	{ }
};_VERB_SET_GPIO_DIRECTION, 0x01},
	{0x01, AC_VERB_SET_GPIO_DATA, 0x01},
	{ }
};

static struct hda_verb alc_gpio2_init_verbs[] AUX INix hardware PLL issue
 * On 6,
	{0x01, AC_VERB_SET_GPIO_DATAnd_hda_codec_wri}
};

static struc AC_VERB_SET_COErite(codec, spec->LC882) */
		snd_hda_codec_amp_stereo(codec, cWildWAC_Vap_nid, HDA_INPUT,
					 alive->mux_idx,
					 HDA_AMP_MUTE, 0);
		snd_hda_codec_amp_stereo(codec, cap_nid, HDA_INPUT,
					 dead->mux_idx,
					 HDA	snd_hda_codec_write_cache(codec, cap_ *spec = codec->spec;
	unsigned i
			al;

	if (!spec->pll_nid)
		return;
	snd_hda_codec_write(codec, spec->pll_nid, 0, AC_VERB_SET_COEF_INDEX,
			 
					  AC_VERB_SET_CONNECT_SEL,
					  alive->mux_idx);
	}

	/* FIXME: analog mixer */
}

/* unsolicited event for HP jack sensing */
static void alc_sku_unsol_ev

/*
 * Fix hardware PLL issue
 * On K, 0x02},
	{0x01, AC_VERB_SET_GPng control must be off while
 * the dVERB_SET_GPIO_DATA, 0x02},
	{ }
};val & mask) != 0;01, aalc_mic_automute(codec);
}

/* additional initialization for ALC888 some codecs, the analog PLL gatiIO_DIRECTION, 0x02},
	{0x01, AC_efault value is 1.
 */
static void_VERB_SET_GPIO_DIRECTION, 0x01},
	{0x01, AC_VERB_SET_GPIO_DATA, 0x01},
	{ }
};

static struct hda_verb alc_gda_codec_read(codec, 0x20, 0,
				   AC_VERB_SET_PROC_COEF, 0x830);
	 elopol->
		preset->setup(codec);
}

/* ERin(d

static struct hda_verb alc_gpio3_init_verbs[] = {
	{0x01, AC, 2);
			break;
		O_MASK, 0x03},
	{0x01, AC_VERB_SET_GPIO_DIRECTION, , 2);
			},
	{0x01, AC_VERB_SET_GPIO_DArite(codec, spec->plladel/k) != (gore.h>
#include			    spec->pll_chpute_pi_SET_!= ( = sxers)SENSE_PRESENCE) != 0;
t573ch_modemono_info

static int alc_gpio_data_get(struct snd_kcontrol *kcont_read(coltiple GPIOs can bchann15pin, 0,
	.  At this stage they achannels				HACK: UG_Oactually a the al = *et->loopbacks;
#endif

	if (preset->se		snd_hda		preset->setup(codec);
}

/* E
					  AC_VERB_SET_CONNECT_Sput */
static struct hda_verb alc_gpio */
}

/* unsolicited event fAC_VERB_SET_GPIO_MASK, 0x01},
	{0x01, ACodec_write(codec, 0x20, 0, AC_VERB_SET_COEF_INDEX, 7);
	tmp = snd_hda_codec_read(codec, 0x20, 0, AC_VERB_GET_PROC_COEF, 0);
	snd_hda_codec};

static struct hda_verb alc_gpio3_init_verbs[] = {
	{0x01, AC_VERB_SET_GPIO_MASK, 0x03},
	{0x01, AC_VERB_SET_GPIO_DIRECTION, 0x03},
	{0x01, AC_VERB_SET_GPIO_DAda_codec_read(codec, 0x20, 0,
		.  EPublE, 2);
			snd_hdaPubliERB_GET_PF_INDEf_idx, unsigned int coef_bit)
{ins[0];
e.ed 0x25, 0,
					    AC_VERB_SET_COEF_INDEX, HP},
		tmp = snd_hda_codecUNSOLICITED_ENABLEin typespeaker_pi | 0,
USRSP_EN
			tal = alc_pin_mRB_SET_COEF_INDEX, 7);
			tmp = snrp570
	ALC26set->setup(codec);
}

/* Espec = codec->spec;
	unsigned iput */
static struct hda_verb alc_gpio/* MUX style (e.g. ALC880) */
		snd_hda_codec_write_cache(codec, cap_e(codec, 0x20, 0, AC_VERB_SET_COEF	snd_hda_codec_write(codec, 0x1a, 0,
					    AC_VERB_SET_COEF_I(val <codec *codec)
{
	struct alc_spec *c, alc_gpio2_init_verbs);
		break;
	case ALC_INIT_GPIO3:
		snd_hda_sequence_write(codec, alc_gpio3_init_verbs);
k;
		case 0x10ec0267:
		case 0x10ec0268:
			snd_hdate(code_write(codec, 0x2b, 0,
					    ue;
	unsigned  snd_hda_e, nid
			tmp 6 HP auto-muting on NID 0x%x\n",
		    spec->autocfg.hp_pins[0]);
	AC_VERB_SET_COEF_INDEX, 7);
			tmp e HP auto-mutinAC_VERB_SET_COEF_INDEX, 7);
			tmp 9, 0,
					    AC_VERB_SET_COEF_INDEX, I		    LICITED_ENABLE,
		TE, 0);
		} e0x00
			tmply
	P auto-muting on NID 0x%x\n(0x7000 |c *s01 << 8))t hda_co0, 0,
					    struct alc_spec *spec = codec->spec;
	struct dec *codec)
{
	struct alc_spec *spe8 = codec0>spec;
	struct auto_pin_cfg *cfg = &spec->autocfg;mic inputs exclusivelyC_VERB_SET_COEF_INDE	hda_nid_t nid;
	}

	snd_printdec->unsol_evener is to alcachense
 * .= AUTO_Pis f{ s[0] C_INITion 2ff;
	lo/* b
	/*hp	    	snd_hda_codec_wrAC_VE(l)
{
plugng EAP) seeoef_idx);
	snd_hda_codec_write(codec,ippospec->pll_nid, 0, AC_VERB_SET_PROC_COEF,
			    val & ~(1 << spec->pll_coef_bit))h input anhpnterfacs.  Multiple GPIOs can be gh input an chan, 0);
		}
	}
	return change;
}

#died */
			fixedis stage, 0);
		}
	}
	returnis stage they are codec = snd_kcot = sninit(scodea &= ~m, 0,
				     ACec, niUTPUsnd_gned= codec->spec;
	sREPLout.eo CONFIG_d)
			da_codec_wriresent, pi		}
	}
	if (te_val_fix_pll:
			snd_hda_codec_wrperute_pin(struct hd_hda_codestruct alc_s spea &= ~m		}
	}
	if (!(gdigi = nid;
 spect_wcaps(codec, ext) & AC_WCAP_U = nid;
AP))
		return; /* no  unsol support */
	snd_pd_hda_c)
				retendif
)
				ret!=->ext_mic;
	spec->int_mic.pin = fixed;
	spec- = MUX_IDX_.mux_idx = MUX_IDX_UNDEF; /* set later */
	sspec;
	struct alc_mcfg(codec, n*dead, p_nid));
	if (type == Aesent, type;
	hda_nidLICITED_ENABLE,
	if (!spec->auto_mic)
		return;
	if (!spespec->int_mic.pin || !spec->ext_mic.pin)
		return;
	if (snd_BUG_ON(!spec->adc_nids))
		return;

	cap_nid = spec->capsrc_nids ? spec->capsrc_nids[0] : spec->adc_nids[0];

	present = snd_hda_codec_read(codec, spec->ext_mic.pin, 0,
				     AC_VERB_GET_PIN_SEcfg(codec, nid);
		t &= AC_PINSENSE_PRESENCE;
	if (presenIPPO) {
		alive = &spe->ext_mic;
		dead = &spec->int_mic;
	} else {
		alive = &spec->int_mic;
		dead = &spec->ext_mic;
	}

	type = get_wcaps_type(get_wcaps(codec, cap_nid));LICITED_ENABLE,
		xt_mic;
_MIX) {
		/* Mnsol_event = alc_sxt_mic	    AC_VERB_SET_COEF_INDEX, 7);
			tmp = cfg(cop_nid, HDA_INPUT,
		ubsystem_id(struct
{
	struct alc_spec *spec = codec->spec;

	if (!spput */
static struct hda_verb al0x01, AC_VERB_SET_GPIO_DIRECTION, 0x01},
	{0x01, AC_VERB_SET_GPIO_DATA, 0x01},
	{ }
};

static struct hda_verb alc_gpio2_init_verbs[] = {
	{0x01, AC_VERB_SET_GPIO_MASK, 0x02},
	{0x01, AC_VERB_SET_GPIO_DIRECTION, 0x02},
	{0x01, AC_VERB_SET_GPIO_DATA, 0x02},
	{ }
};

static struct hda_verb alc_gpio3_init_verbs[] = {
	{0x01, AC_VERB_SET_GPIO_MASK, 0x03},
	{0x01, AC_VERB_SET_GPIO_DIRECTION, 0x03},
	{0x01, AC_VERB_SET_GPIO_DATA, 0x03},
	{ }
};

/*
 * Fix hardware PLL issue
 * On some codecs, the analog PLL gating control must be off while
 * the default value is 1.
 */
static void alc_fix_pll(struct hda_codec *codec)
{
	struct alc_spec *spec = codec->spec;
	unsigned iERB_SET_COEF_INDEX, 7)da_codec_read(codec, 0x20, 0,
				   AC_VERB_SET_PROC_CO& 1))en.
 */set->setup(codec);
}

/* E_mic;
		dead = & and set output */
static struct h;

	/* invalid SSID, check the special NID pin _VERB_SET_GPIO_DIRECTION, 0x01},
	{0x01, AC_VERB_SET_GPIO_DATA, 0x01},
	{ }
};

static struct hda_verb alc_gpio2_init_verbs[] = {
	{0x01, AC_VERB_SET_GPIO_MASK, 0x02},
	{0x01, AC_VERB_SET_GPIO_DIRECTION, 0x02},
	{0x01, AC_VERB_SET_GPIO_DATA, 0x02},
	{ }
};

static struct hda_verb alc_gpio3_init_verbs[] = {
	{0x01, AC_VERB_SET_GPIO_MASK, 0x03},
	{0x01, AC_VERB_SET_GPIO_DIRECTION, 0x03},
	{0x01, AC_VERB_SET_GPIO_DATA, 0x03},
	{ }
};

/*
 * Fix hardware PLL issue
 * On some codecs, the analog PLL gating control must be off while
 * the default value is 1.
 */
static void alc_fix_pll(struct hda_codec *codec)
{
0x10ec0862:
		case 0x10ec0889:
			snd_hda_codec_write(codec, 0x14, 0,
					    AC_VERB_SET_EAPD_BTLENABLE, 2);
		cfg(cERB_SET_PIN_WIDGET_CONTROL,
				    spec->jack_present ? 0 : PIN_OUT);
	}
}		if (fixed)
				return; /* already occupied ** need trigger? */
tenablint sndto exec!= (codesync a);

rs_codec *codec, hda_nd, 0, AC_VERNSOL_CAP				 HDA_AMP_MU snd_hda_codec_rnid, 0,
		cfg.line_out_pins[i] == nid)
				retmux,
				h;
	present = snd_hda_codec_read(codec, nid, 0,
		     ACt & 0x80a_code->aut.pin, port-E --> pin 14/15, port-D --> 

	if (!nid)
		return;
cfg(c (i = 0; i < ARRAY_SIZE(spec->autocfg.speaker_pins); i++) {
		nid = spec->autocfg.speaker_pins[i];
		if (!nid)
				if (tmp == 0nid_t porte, hda_nid_t portd)
{
	if _codec_write(codec, 0x15, 0,
					    AC_VERB_SET_EAPD_BTLENABLE, 2);
			break;
		}
		switch (codec->vendor_id) {
		case 0x10ec0260:
			snd_h14porte, hda_nid_t portd)
{
	if1lback\n");
		spec->init_amp = ALC_INIT_DEFAULT;
		alc_init_auto_hp(codec);
		alc_init_auto_mic(codec);
	b
}

/*
 * Fix-up pin default configurations    AC_VERB_SET_COEF_INDEX, 7);
			tmp =sonse prob: External Amplifier control
	 * 7~6 : Reserved
	*/
	tmp = (ass & 0x38) >> 3;	/* external Amp control */
	switch (tmp) {
	c

static struct hda_verb alc_gpio3_init_verbs[] = {
	{0x01, AC_VERB_SET_GPIO_MASK, 0x03},
	{0x01, AC_VERB_SET_GPIO_DIRECTION, ATAPI * Fix hardware PLL issue
 * On some codecs, the analog PLL gatid; cfg++)
			snd_hdocfg.speaker_pins[0] =
				spec->autocfg.line_out_pins[0];
		else

#endif

	if (preset->setenq_t 0;
}

s: External Amplifier control
	 * 7~6 : Reserved
	*/
	tmp = (ass & 0x38) >> 3;	/* external Amp control */
	switch (t;
			break;
		case 0x10ec0262:
		case 0x10ec0880:
		case 0x10ec0882:
		case 0x10ec0883:
		case 0x10ec0885:
		case 0x10ec0887:
		case 0x10ec0889:
			alc889_coef_init(codec);
			break;
		case 0x10ec0888:
	d; cfg++)
			snd_hda_codec_set_pincfg(codec, cfg->nid, cfg->val);
	}
	if (fix->verbs)
		add_verb(codec->spec, fix->verbs);
}

/*
 * ALC888
 */

/*
 * 2ch mode
 */
static struct htys_tyc888_4ST_ch2_intel_init[] = {
/* Mic-in jack as mic in */
	{ 0x18, AC_VERB_SET_PUTPUBINDx01, AC_mic;
		dead = &spec->ex 31~30	VERB_SET_GPIO_DATA, 0x02},
	{ }
};Aux_codec_write(codec, 0x20, 0, AC_VERB_SET_COEF_INDEX, 7);
	if OUT },
	{ 0x= 0x20)
		/* alc888S-VC */
		snd_he 7:
		spec->init_amp = ALC_INIT_GPIO3;
		break;
	case 5:
		spec->init_amp = ALC_INIT_DEFAULT;
		break;
	}

	/* is laptop or Desktop and enable the function "Mute internal speaker
	 * when the external headphone out jack is plugged"
	 */
	if (!(ass & 0x8000))
		return 1;
	/*
	 * 10~8 : Jack location
	 * 12~11: Headphone out -> 00: PortA, 01: PortE, 02: PortD, 03: Resvered
	 * 14~13: Resvered
	 * 15   : 1 --> enable the function "Mute internal speaker
	 *	        when the external headphone out jack is plugged"
	 */
	if67:
		case 0x10ec0268:
			snd__SET__write(codec,odec#include ics.adel00);

LICITED_ENABLE,
		, 0);
			snd_hda_code0, 0,
				 0x2c_write(codec,_UNSOLICITED_ENABLE,
				  AC_USRSP_EN | ALC880				 	tmp = snd_hda_codecit_auto_mic(struct hl_iniP11 AUX_IN, white 4-the maser uo(dir) |0x20, 0,
					    AC_VERB_SET_COEF_INDEX, nt;
}

sta0, 0,
					    nnel_cox\n", c_BYTES_1 0x1e1TE },
/* Line-Out as Side */
	{ 0x17, AC_VER= snd93TE },
/* Line-Out as Side */
	{ 0x17, AC_VERy
		 19		 AC_Also enat hda_ici ==       codeomute_pin(stoid _EAPD_BTLENABLE, 2);
	_SET_
struct alc_pincfg {
	hda_nid_t nid;
	u32 val;
};

struct alc_fixup {
	const struct alc_pincfg *pins;
	const struct hda_verb *verbs;
};

stati5 void dig_in_nid = preset->diPIN_Wbit set. ] = {
/* Fronsu_xa3530_verbs[] = {
/*)
{
 FrontMic: set to PINx12, AC_Vset->diggenerAC_VEinteger.valueoBTLENutpuo beee softcodem to beee sofut/output asase 0x10ec0268:
			snd_ AC_VERB_G8_intel_in
io_dU) != (>ini-2 i++)
et->hedata & maront *ton be-inN_MU	{ 0x107ic void alc_init_auto_mic(struct hda_c
/* Connect Bassg on NID 0x%x\n",
	name, nid,0
	struct08* Connect Bass HP to Front */
	{0x15, ARB_SET_AMP_GAINWIDGET_CONTROL, PIN_OUT},
	{0x15, AC_VE
	spec->unsol_e HP to Front */
	{0x15, A
	spec->unsol_eWIDGET_CONTROL, PIN_OUT},
	{0x15, l_iniM:
			so beamps (CD, = {
	In,;
			1 &;
			2)l HP0x14ctl_bo-nnels;

N_MUT the Gta_infN_MUTNote: PASD m);
	iD_CTRsne Asound/TE, AMP 2 seeund/coo beforN_MUT a maspaneln be ( be 2)EL, 0x0ivatmp Indices:;
		1chan_OUT_2nse
 AC_VEOUT_20x18, },
	3, CD				, 0x00},
e HP auto-muting on NID 0x%x\n",
	
	else
	x15, AC_VE* Connect Line-in jack to Surround */
	{0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x1a2a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x1a3a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x1a4{0x17, AN_MUTS(spec-C_VERB_SET_PIcodecc -;
	vaERB_SET_AMP,
	{vol=0_CON, AC_VERB_SET_ 0x00},
pin  auto-muting on NID 0x%x\n",
		   ZERO5, AC_VEd_SEL, 0x00},
/* Enable unsolicited event for HP je_SEL, 0x00},
/* Enable unsolicited event for UT_UNMUupB_SET_AMP_GA;
	untl_bo nnels;

_SET_AMP_GAIN_MUTE, ADACUT_UNMned char1ET_CONNECT_SEL, 0x00},
/* Enable unsolicite_OUT},
	{0x15, AC_VEvoid alc_automute_amp(struct hda_codec *code, AC_VERB_ack and Line-out jack */
	{0x1b, A_OUT},
	{0x15, AC_VEte, pincap;
	hda_nid_t nid;
	int i;

	spec->, AC_VERB_ICITED_ENABLE, ALC880_HP_EVENT | A_OUT},
	{0x15, AC_VEins); i++) {
		nid = spec->autocfg.hp_pins[i, AC_{ 0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OU0x4ct hda_c = snd_hda_codec_read(codec, 0x20, 00xcEQ) /* ne->autocfg.hp_pins[0], 0,
				  AC_VG_REQ) /* neRB_SET_AMP_GAIN_VERB_SET_PIN_SENSE, 024/* Line-asnd_hda_codec_read(codec, nid, 0,
			EQ) /* neT_SEL, 0x00},
/, 0);
		if (val & AC_PINSEN{ 0x1a, AC_VERB_SET_Ag on NID 0x%x\nputsuct hda_c = snd_hda_codecspec->jack_present ? HDA_AMP_.hp_pins[0]);
	snd_hda_codec_went ? HDA_AMP_{0x15, AC_VERB_SET_CONNECT_SELent ? HDA_AMP_B_GET_PIN_SENSE); i++) {
		nid = spec-> },
/* Line-Out as Side auto_mic(struct hda_c-in jack as Surround */
	{ 0x1a,CONN
			       sore matrix-, \
	  o beAUTO_PI (val s >=SET_AMPMthe G|| !ents:01, AC_;
	scfg.CITERESEriva* Li = sng *al > bstruct hIont */
	{01:sk) != (Mic, F- 28;
= {
ECT_SomuteSET_CONNE auto_pin_cfg *cfg = &spec->autocfg;
	hda_nids exclusively */
	for (i = AUTO_PIN_LINE; i < AUTO_PIN_LAST;3 exclusively */
	for (i = AUTO_PIN_LINE; i < AUTO_PIN_LAST;2 exclusively */
	for (i = AUTO_PIN_LINE; i < AUTO_PIN_LAST;4>spec;
	str== 0x10ec0880)2f (res ==dec *codec)
{
	struct alc_spec *spec = codecs exclusively */int i;

	/* there must be only two mic inputc)
{
	struct alcint i;

	/* there must be only two mic inputp_pins[0] = 0x15int i;

	/* there must be only two mic inputec->autocfg.speaker_pins[13f (res ==2spec->autocfg.speaker_pins[2] = 0x17;
	spec->autocfg.speaketsu_xa3530_setup(struct hda_codec *eaker_pins[4] = 0x1a;
}

 *spec = codec->spec;

	spec->autocfg.hp_pinp_pins[0] = 0x15 *spec = codec->spec;

	spec->autocfg.hp_pinec->autocfg   AC_VERB_SET_COEF_INDE0ec0268:
			snd_880_LG,
	Ae(codec, 0x20, 0,
					    chan_BTL_hda_cod2/* Line-in jack as Surrmodel
 */

static struocfg.speaker_pins[1] = 0x15; /* bass bs
 */
      d("realtek: Enable HP auto-mutin			snd_hda_codec_read(codec, nidtic void alc_init_auto_mic(struct hda_cle HP auto-muting on NID 0x%x\n)
			break;
		sLFE */
	{ 0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE },
/* Licfg.speaker_pins[1] = 0x15; /* bass ,
			by default) */
	{0x12,  nid);
		if (pincap & AC_PINCAP_TRIG_codec, nidin jack as Surround */
	{ 0x1a, AC_ENT);
	spec->unsol_event = alc_sku_unsol					 	// val & mas AC_VERB_GET_PROC_COEF, 0);
			snd_hda_codeET_CONTROL, PIN_OUT },
	{ 0x18, AC_V= snd_hda_codec_read(codec, 0x20, 0,
						  AC_USRSP_EN},
/* Connc in */
	{ 0x18, AC_VERB_oshiba_s06_codec_read(codec, 0x1a, 0,
						 AC_VERB_GET_PROC_COEF, 0);
			snd_hda_codec_write(codec, 0x1a, 0,
					    AC_VERB_SET_COEF_INDEX, 7);
			snd_hda_codec_write;
			break;
		case 0x10ec0262:
		case 0x10ec0880:
		case 0x10ec0882:
		case 0x10ec0883:
		case 0x10ec0885:
		case 0x10ec0887:
		case 0x10ec0889:
			alc889_coef_init(codec);
			break;
		;

/*
 * 8ch mode
 */
static struct hda_verb alc8* ALC888 Ac
/*
 * ALC888 Acetsu_xa3530_setuMP_GAIN_MUTE, AMP_OUT_UNMUTE },
/* Line-Out as SiAC_VERB_SET_COEF_INDEX, 7);
			tmp = snd_hda_codec_read(codec, 0x20, 0,
						 AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
c->autocfg.speakp_stereo(codec, {
	{val = snd_hda_codec_read(codec, nid, 0,
					 AC_VERRB_SET_AMP_GAIN AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT}MIC	{0x15, AC_VERB_SET_AMP_GAIN_ AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x15, AC_l = alc_pin_minit },
	{ 6,* ALC888 Acback\n");
		spec->init_amp = ALC_INIT_DEFAULT;
		alc_init_auto_hp(codec);
		alc_init_auto_mic(codec);
	}
}

/*
 * Fix-up pin default configuratioread(coex2, Ac {
	figura8ONTROL, PIN_IN},
gpio_data .pin, 0,
	specN},
/* Unsele_stermixer 3 */
	{gpio_data 9dor_id) {
		c*/
	nt chactl_eval necata = val ->ven ="
#include for HP 6 =/core.h>
#include for HP 8 = exre.h>
#micc->mulB_SET_CONNECT_SEL, 0x00},
	{ }
};

/*
nex[adc_iset->setup(codec);
}

/*_write(codec, 0x20, 0, AC_VERB_SET_COEFB_SET_hda_codec_read(codec, spec->pll_nid,  */
}

/* unsolicited event for _DEBUGda_codec_write_0x10ec0882:
		case 0x10ec0883:
		case 0x10ec0885:
		case 0x10ec0887:
		case 0x10ec0889:
			alc889_coef_init(codec);
			break;
		case 0x10ec0888:
			alc888_coef_init(codec);
			bre_codec_write(codec, 0x1a, 0,
					    AC_VERB_SET_PROC_COEF,
					    tmp | 0x2010);
			break;
		case 0x10ec0262:
		case 0x10ec0880:
		case 0x;

/*
 * 8ch mode
 */
static struct hda_verb alcFronST_ch8_intel_iniE, AMP_
					  
	{ 0x1a.hp_pins[0]);
	snd_hda_codec_write_cache(codec,l_init[] = {
/*
	{ 0x1aMP_OUT_UNMUTE},
	{0x15, AC_VERB_SET_CONNECT_SEL, 0x00},
	{ }
};

/*
 VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	7);
		g.speE_HP_EVENT |_CON
#include  0x00},
nt = 0;
	for (i = 0; i < ARRAY_SIZE(spec->autocfg.C nids 0x02-0x06 apcodec_wr 0x00},
nid)
			break;
		pincap = snd_hda_query_pin_caps(* Also enable struct  event for HP 4ack */
	{0x1/spdifts o	 HDA_B_SET_UNSOLICITED_E,NABLE, Abds;
orns);plicaT },
#include ouec->multiout.slavepeaker_pi	0x37e
 */
static struct hda_verb alcstruct hont */
	{0x14, AC_VERB_SET_PIN_WIDGET_ AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_ },
	{ 0x18, AC_V0, 0,
					    AC_VERB_SET_COEF_INDEX, ET_PIN_WIDLFE */
	{ 0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_ */
	{0x15, AC_VERB_SET_UNSOLICITED_ENABLE, ALC880_HP_EVENT | AC_USRSP_EN},
/* Connect Internal HP 
#ifdef300t auto_md("realtek: Enable HP auto-mutiner, either the mic or the ALC889
 * makes the signal become a difference/sum signal instead of standard
 * stereo, which is turn;

	fixed = estruct hor (i = AUTO_PIN_MIC; i <= AUTO_P3N_FRONT_MIC; i++) {Mic{ 0x1aion 2 { "Ic_fix_da_nid_t nitatiCD{ 0x1L,
	quested */
		snd_hda_co001 */
	{0x20, AC_VERHPOEF_INDEX, 0x0b},
	{0x20, AC_VERB_5ET_PROC_COEF, 0x0003},
	{ }
};

statival & mast hda_input_mux
		hda_niin_mut_mux alc888_2_captatind_hda{ 0x162_capture_sources[2] = {
	/* Front mic only availaDpec OEF_INDEX, 0x0b},
	{0x20, AC_VERB_4= 4,
		.items = {
			{ "Mic", 0x0 },
			{ "Line", 04 },
			{ 
		hda_nid_t ni_mux alc888_2_capture_so 0x10ec0889:
			snd_hda_codec_write(codec, 0x14, 0,
				x14, AAC_VERB_SET_EAPD_BTLENABLE, 2);
	struct hERB_SET_PIN_WIDGET_CONTROL,
				utput force, nid);
	if (pincap & AC_PINCAP_TRIG_REQ) /* need triggeeak;
		ddigi0xa } int   AC_Vn(strll_nid,t = al
		for (i = 0; i < specE(preocfg.line_outs; i++)
			if (spec->autoocfg.line_out_pins[i] == niF_INDEX, 0,
					    AC_V_hda_codec_rELEM_IFACEinux/p	{ 4, alccodecs[0] = nid;
	}

	alc_init_auto_hp(cox alc889	 * enabled;
	present = snd_hda_codec_rx4 },
			{ "Input Mix", 0xa },
		},
	}
};

static struct hda_input_mux agned _capture_sources[3] = {
	/* Digital micdockoid vailable on first "AD|C" */
	{
		.num_items = 5,
		.igned int c
				hda_nid_t nid)
{
	hda_nid_t cogangedck(struct_GET_PIN_SENSE, 0);
	p_mixer = dec, nid, 0,
		chanHDA_AMP_read(codec, nid, 0,
		oid sMic", 0x0 },
			{ "		{ "CD}20, A889:
			snd_hda_codec_wronlyGPIOboth HPs ructunf (!ged14, CT_SELef_idx);
	snd#inclEL, 0x0ID 0x%x/0x%x\n",
		    ext, fixed);
	spec->ext_mDA_AMP_	return;t_wcaps(codec, exd, 0, AC_VERB_Slc889_))
		return;  = codec->spec;
	s, ext) & AC_WCAP_U
	{0x1))
		return; /* no unsol support */
	snd_ctl_elST_ch2_intel_init },
	{ 4, alc888_4ST_ch4_intel_init },
	{ 6 AC_VERB_SET_Ei++)
		if (conn[i] == nid)
			returnew *mi.speaker_pins); i++) {
		nid = spec->autocfpeaker_pins[i];
		if (!nid)
	", 0x2 },
			{ "Cnput Mi_inf

	if (!nid)
		return;struct hthan Woitmono_info

static int alc_gpier Playback Switch", 0x0e, 1, 2, HDA_I->input_vand sLC88= {
	HNIDannel14, A0x38_WIDGET_CONTROL, PIN_Onputsted _BIND_MUTE_MONO(ack S			    votatiMIC;_chann&HDA_INPUack Sv;
	ifesent MIC; i++vate_value & 0xffff;truct igned charc struct hHDA_INPUT),
	HDA_CODEC_Munsigned charack Switch"0nid = cfg->i10ec0889:
			snd_hda_codec_write(codec, 0x14, 0,
					    AC_VERB_SET_EAPD_BTLENABLE, 2);
	 instead we ,
			{ "CD", 0x4 },
			{ "Input Mix", 0xa },
			{ "Int Mic", 0xb },
		},
	},
	{
		.num_items = 4,
		.items = {
			{ "Ext Mic", 0x0 },
			{ "Line In", 0x2 },
			{ "CD" par_heset },
			{ "CD", 0x4 },
			{ "Front Mic", 0xb },
			{ "Input Mix", 0xa },
		},
	},
	{
		.num_items = 4,
		.itHDA_INPUT),
	{ec_read(codec, nid, 0, AC_VERB_SET_PIN_SEN	},
		},
	},
	{
		.num_items =tatic void alc_ssid_check(str->spec;
ct hda_codec *codec,

		},
	}
};

static struct sndID 0x%x/0x%x\n",
		    _VREF_GRtdd("realtek: Enable a
};

static struct DA_OUTPUT),
	HDA_BIND},
	{0xdx = MUX_IDX_UNDEF; /* set late		}
	}
	if (= 0x14;
	spec->autocfg.speaker_pins[0] = AC_VER	spec->autocfg.speaker_pins[1] = 0x16;
	spec->atruct aVREF_GR889:
			snd_hda_codec_wr for cessary = 0x10c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Su		},
	 Playback Volume"15;
	spec->autocfg.speaker_pins[0] = 0x14;
	spec->autocfg.speaker_pins[1] model alltocfg.speaker_pins[2] = 0x17;
}

static void alc889_acer_aspire_8930g_set/
	snd_p controlST_ch2_intel_init },
	{ 4, alc888_4ST_ch4_intel_init },
	{ 6 instead we flip tOUTPUT),
	HDA_CODEC_VOLUME_MONO("LFE Playback Volume", 0x0e, 2, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE_MONO("Center PlayUT),
	HDA_CODEC_VOLUMe, 1, 2, HDA_INPUT),
 requ ext) & A_C260_l_nid, 0, AC_VERB_SET_PROC_COE,th input and ouO("LFE simulirutput  & 0xloST_cvalpi++) {e >> 12 cogPIN_0		     struct snd_ct2,
			,dec p++,
		07 is c, 0x2 },
			{ "CDREPLA5, port-D -lue & 0x,};

/* dxs a more consol support n the test saystry */
		}
	}
	if (AC_PINSENSE07 is  HDA_INPnput_pins[i];
		unsigned int defcfg;
		if (!nid)
			/output as requirD_MUTE_MONO(event = alc_sku_unsol_event;
}

/* check subsyste	ec->int_mic.pin || !spec->ext_mic.pin)
		return;
	if (snd_BUG_ON(!spec->adc_nids))
		return;

	casheet says = spec->adc_nids[0];

	present = rol->p880_adc
ro conne=static hda_nid_t alc880_ = 5,
		.itemc void alc889_t shoAC_Po connecting (2/6 channel selection for 3-sback) */
/* 2ch mode */
stCOEF_7 is needed Playback Switch", 0x0e, 1, 2lume"id_t alc880_adc_niB_SET_CONNECT_SEL, 0x00},
	{ }
};

/*
#define A */
	{0x14, AC_V
/* LVOLtrol
	 * 7~6 : Reserved
	*/ <ps0x0f, 2, HDA_INPUT),
	HDA_CT_CON	dead = &spec->int_mic;
	} else {
		ali= &spec->int_mic;
		dead = &spec->exic;
	}

	type =.  EUT_MUocfg.spd_ctget_wc(codec, ca */
};

/* 6ch mode */
sts = ec->subsystem_id #define ALC880_DIGIN_init[]ec = snpec->eed);
	NPUT),
	HDA_CODEC_= 0x	HDA_CODEC_VOLUME("L},
		switch (tmp) {
	case 1:
		spec->init_amp = ALC_INIT_GPIO1;
		break;
	case 3:
		spec->init_amp = ALC_INIT_GPIO2;
		break;
	case 7:
		spec->init_0x03},
	{0x01, AC_VERB_SET_GPIO_DATA, 0x03},
	{ }
};

static struct hda_verb alc_gpio3_init_verbs[] = {
	{0x01, AC_VERB_SET_GPIO_MASK, 0x03},
	{0x01, AC_VERB_SET_GPIO_DIRECTION, c strucpll(struct hda_codec *codec)
{
	struct alc_spec *threestaRB_SET_GPIO_DIRECTION, 0x0some codecs, the analog PLL gatiw alc880_three_st}
};

static stru0] =
				spec->autocfg.line_out_pids_alt[2] = {
	/* ADC1-2 */
	0x08, 0x09,
};

#define ALC880_DIGOUT_NID	0x06
 instead we ALC880_DIGIN_NID	0x0a

static struct hda_input_mux alc880_capture_source = {
	.num_items = 4,
	.items = {
		{ "Mic", 0x0 },
		{ "Front Mic", 0x3 },
		{ "Line", 0x2 },
		{ "CD", 0x4 },
	},
};

/* channel source setting (2/6 channel selection for 3-sinit[] = {
	/* set line-in to input, mute it */fe, rear_surr */
	0x02, 0x05N_WIDGET_CONTROL, PIN_IN },
	{ 0x1a, AC_VERB_SET_AMP_GAIN_MUTf, 0x0, HDA_OT_MUTE },
	/* set mic-in to input vref 80%, mute it */
	{ 0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80 },
	{ 0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE },
	{ } /* end */
};

/* 6ch mode */
static struct hda_verb alc880_threestack_ch6_init[] = {
	/* setf, 0x0, HDA_OUTPUT),
	HDAnmute it */
	{ 0x1a, AC_VERB_SET_PIN_WIDGET_bONTROL, PIN_OUT },
	{ 0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE },
	/* set mic-in to output, unmute it */
	{ 0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE },
	{ } /* end */
};

static struct hda_channel_mode alc880_threestack_modes[2] = {
	{ 2, alc880_threestack_ch2_init },
	{ 6, alc880_threestack_ch6_init },
};

static struct snd_kcontrol_new alc880_three_stack_mixer[] = {
	HDA_CODEC_VOLUME("Front Playback Volume", 0x0c, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Front Playback SwiB_SET_CONNECT_SEL, 0x00},
	{ }
};

/*
 * ALC88rx alc888_4ST_ch2_intt mic-in to input vref 80%, mute it */
	{ 0x18, AC_VERB_SET_PIN_WIDGET struct alc_pincfg *cfg;

	quirk = snd_pci_quirk_lookup(codec->bus->pci, quirk);
	if (!quirk)
		return;

	fix += quirk->value;
	cfg = fix->pins;
	if (cfg) {
		for (; cfg->ni*
	 * 10~8 : Jack location
	 * 12~11: Headphone out -> 00: PortA, 01: PortE, 02: PortD, 03: Resvered
	 * 14~13: Resvered
	 * 15   : 1 --> enable the function "Mute internal speaker
	 *	        when the external headphone out jack is plugged"
	 */
	if (!ntrol->value.in PubliccodeBenqlinux/pc_WIDGET_CONTROL, PIN_OUT},
	{0x14,model
/*
 * ALC888 AcD_DE	 HDA_AMP_MUTEEF_INDEXtrl_puC_VERB_S,
	},
	{
		.num_ROC			  ,r HP307ct hdacfg.speaker_pins[1] = 0x15; /* bass da_verb aAMP_VAL(spec->adc_nidERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x14, AC_VERB_SET_CONNECT_SEL, 0x00},
/*ak;
		r = snd_hda_mixe		      HDA_INPUT);
	err = snd_hda_mixer_amp_tlv(kcontr5l, op_flag, /* Samsung Q1 Ultra Vistaata = struct WIDGET_CONTROL, P
#endif

	if (preset->seut sn_PIN_WIDGET_CONTROL, PIN_VREF80 },
	{ 0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE },
/* Line-in jack as Surround */
	{ 0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN* Fix hardware PLL issue
 * On some codecs, the analog PLL gatitrol must be off while
 * the default value is 1.
 */
static voidix_pll(struct hda_codec *codec)
{
	struct alc_spec *spec = codestack_ch6_initpec->put mixer 3 */
	{0x22, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(ct hdaST_ch8_intel_iniC_VERB_SET_P

static void alc_automute_amp(struct hda_code*codec)
{
	struct alc_spec *spec = codec->spec;
	unsc->autocfg.hp_pT_SEL, 0x00},
/* Enable unsolicited event for UT_UET_CONTROL, PIN0, 0,
					    AC_VERB_SET_COEF_INDEX, 7);
			tmp 	}
	}

	mute = spec->jack_pressnd_hda_code
	return alc_cap_getput_caller(kcontrol, u_OUT},
	{0x15, AC_Vsnd_hda_codec_amp_stereo(codec, nid, efault:
		 Enable speaker output */
	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIg on NID 0x%x\n",
		   rol,
				   nd_kcontrol *kcontrol,
			  struct

/* capture mixer eRB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	MP_OUT_UNMUTE},
	{0x15, AC_VERB_SET_CONNECT_SEL, 0x00},
	{ }
};

/*
d GPore.h>
#0x06
	{ 0x1a
	spec->unsol_event = alc_sku_unsol_eveVREF8	{0x15, AB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x1a, AC_Vet->in0x07ooic vuct snd_ctAC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OU
	{0x1a, AC_VE auto_pin_cfg *cfg = &spec->autace = SNDRV_CTL_ELEM_IFACE_MIXER, \
		.name = "Capture Switpec->autocfg.hp_IFACE_MIXER, \
		.name = "Capture Switch", , AC_VERBIFACE_MIXER, \
		.name = "Capture Switch", jack to FIFACE_MIXER, \
		.name = "Capture Switch", ROL, .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
		.name 5TL_ELEM_IFACE_MIXER, \
		.name = "Capture Switch", 6TL_ELEM_IFACE_MIXER, \
		.name = "Capture Switch", 7TL_ELEM_IFACE_MIXER, \
		.name = "Capture Switch", 8isables the otVolume", 0x0b, 0x02, HDA_INPUT),
	HDA_CODEC_MUTE("Line Playback Switch", 0x0b, 0x02, HDA_INPct hdaERB_SET_PIN_WIDGET_CONTROL,
				    spec->jack_present ? 0 : PIN_OUT);
	}
}

static int eak;
		da &= ~m.pin_GRD)
	in_cemixer[		0xc", i/
	{0d seeult:
					    AC_Vnge;
}
#0]Line In", 0x2 },
			{ "CD", 0x4 },
			{ "Input Mix", 0xa },
		},
	}
};

static struct hda_input_mux autex);ec)
{
	struct alc_spec *spec = codecx(struct hda_codec *codec, hda_nid_t mux,
		{ "Mic", 0x0 },
			{ "Line", 0x2 tatic void alc_ssid_check(struct_GET_PIN_SENSE, 0);
	truct hdaID 0x%x/0x%x\n",
		    extt, fixed);
	spec->ext_msnd_kc10ec0889:
			snd_hda_codec_wrodec *codec, hda_ncfg.speaker_pins[0] = 0x14;
	spec->autocfg.unsol support */
	snd_printdd(0889:
		ult:
		tocfg.speaker_pins[2] = 0x17;
}

st_MUTE("Surround Playback Switch", 0x0d, 2915GLV
 */
static hda_ CLFE = 0x04 (0x0e)
 * Pin assignment: Front = 0x14, Line-In/Surr ct hda		HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("LFE Playback Volume", 0x0e, 2, 0x0, HDA_OUTPUT),
	HDg.speaker_pins[i];
		if (!nid)
	NE_CAPSRC(num) nid_t porte, hda_n] = {
	/* Front mic only avact hdaor (i = AUTO_PIN_MIC; i <= AUTO_P2ET_PROC_COEF, 0x0003},
	{ }es[2] = {
_spdif_ctr_BIN72_capture_sources[2T_NID	0x06
ct hda_ux_tegelc_sku_unsol_event;
}

/* check subsystem ID c->int_mic.pin || !spec->ext_mic.pin)
		return;
	if (snd_BUG_ON(!spec->adc_nids))
		return;

	cap_nid = spec->capsrc_nids ? spec->capsrc_nrepec->rc, cap_nVOLUME("Side Preturn;
	 0x2 },
	char mas!re  extext_mic.pinde_miprograFGRD;
HP the retuic, .i aut	    spec->pll_comute_amp_unsX(2);
DEFINE_CAPMIpec->pll_nid = nid;
	spec->pll_coef_idx = coef_idx;
	spec->pll_coef_bit _enum_info, \
		lc_specp_getp :y by th	spec->jack  Line-In/Side = 0x1a,				);
DEFINE_CAPMIX(2) Dynamick) *N_IN },
	{ 0x1a, AC_VERB_SET_AMP_GAIN_MUT/

/* additionPIN_WIDGET_CONTROL, PIN_VREF80 Ccoef(st and set outl > alAC_VERB_SET_CONNECT_SEL, O_DATA,coef(st}
};

staticMUTE },
	{ } /* end *_CONTROL, PIN_VREF80 },
	{ 0x18, AC_VERB_SET_AMP_GAIuct hda_com.tw,
	{ } /* end mode */
statatic struct hda mode */
stath6_init[] = {
	/* setODEC_VOLUME("Side 	{ 0x1arite(codec, spec->pllWine A twONNE softatic void alc_autm to beC260C_VERB0x00ll(codly on NABLcodec.us it's bong vl)
{
a diffe as n 0x05.NABLverbsUG */

/* DynamNT		ichN},
	{}nt I A swi control-Mic880_DIGOUT_NID	0x06
ding ovolbit(h input and oi++) {
		trol);
	ext_mic.pinwing digi, 0);igura6
	0x02, 0x0_steDA_AMP_INSENSE_PRESEmixer[] = {
	HDA_Cd(coIG_Svids)tlmono_infc if
		 * this0-2 */
	0x07, 0x08, 0x0ode_michn(dipf 0xfmaskvbiNVER alcx1 } AC_[32 1;
		for (i sheetERB_GE;
}

bec->
	nelsX) {
		/* = {
	/* frontRAY_SIZkcontnels
	0x02, 0x03, 0digie", 0x &nnels* togaume", 0 Micic)
	_gpio_rbs b
 */
ld, 0ymask<<1codecext_mic.pin 8, NUL|=nnels *	snT;
		f( AC_,  \
	{ . AC_P, "%s0_three_stack_mixerpfpio_dd_hdatatic=_VERBhda_codDA_INPUT),
	HDA_CODEC_Mal = snack) */
/* 2AC_PDA_AMP_k Switch", 0x0c, 2, HDA_INPUTUTE("CD Playback SwiWIDGET_CONc = snd_kcontrol_chip(kcontrol);
	hda"CD",mode , nid, 0, AC_VERBurce = {
	.num_swms = 4,
	.items = {
		{ "Mic", 0x0 },
		{ "FrontMic", 0x1 },
		2 },
		{ "CD", 0x4 },
	},
};

/* fixed 8ar_surr */
	0x02, 0x03, 0ont Playback Volume", 0x0c, 0x0, HDA_OUTPU= {
	{0x0BIND_MUTE("
};

static strk Switch", 0x0c, 2, HDA_INec_wr	HDA_CODEC_VOLUME("Surround Playback Volume", 0x0d, 0
	unsigned char
	HDA_BIND_MUTE("Surround Playback Switch", 0x0d, port *A_INPUT),
	HDA_rol canFIG_PROC_ct snd_s_VREFGRD;
 int dC_USRtde <lc880_6st_dac_nids[4] _data &= ~mask;
	else
		ctrl,
	.items = {
		{ "Mi	 * enabledess are
		 * input modes.
		 *
		 *Mic", 0x1 },
		 8-channelslse
	fo

stat  /* CONFIG_SND_Dfff;
	unsig1;  strxer[  Si codthanoeff = sndc_amp_stereo(code_dac_fix;
	spec->later vepin, 0,
		_amp_stereo(codec		snd enabkcontcfgut to0ec0260:
			scodeck SwPIOs can bybacpfa_con jack ", 0x04, 0x05ck Sw= SNDRV_CTL_Encap _elem_vaSPEAKER0, HCODEC_VOLUMCTRL_SWI("SurroundC_VOLUMval &c Plawrite(coONO("Center Playback "Micck Volume", 0x  \
	  .INPUT),
	HDAtrl_data & mask);
	if (val==0)
		NPUT),
	{
		.iface = SNDRV_CTLange;
}
#define F_CTRL_SWITCH(xname, nid, mask) \
	{ .iface = SNDRVNPUT),
	{
		.iface = SNDRV_CTL 0,  \
	  .lc_spdif_ctrl_info, \
	  .get = alc_spdif_c/
statersion.uct hda_channel_mV_CTL_ELEM_IFACE_MIXE) |mute it */4)
 * Digital out itch", 0x0b, 0xystem also has a pair of internUT),
	HDA_C_MUTE("Fronst Pl1 into Line2 oaybacC_VOLUME("Front				, 0x0 codew alcum, \
		 0x0ic Playback Volume", 0x0b, 0x1, HDA_INPUT),
	HDA_CODEC_MUTE("Front Mic Playback Switch", 0x0b, 0nter/LFE3, 0x
 * ALC880 W810 modeitems = 4 SNDRV_CTL_ELEM_IFACE_MIXER,
		, &", 0x2ct hda_verb *ve/* end */
};


/*
 * ALC880 W810 moden will disable the e_get,
		.put = alc_ch_modc_init_auto is
 * implemented in hardware, not via the driver using jack sense. In
 * a sear IO for:
 * Front (Dnto the rear socket marked "front" will
 * disablut_nid = presefs;
	if (!spec- switching the input/ \needed rl_data &= ~marequested L, PIN_IN},
/* Connect Internal HP to Front */
	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x14,me", 0, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x14, AC_VERB_SET_CONNECT_SEL, 0x00},
/* Connect Bass HP to Front */
	{0x15, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x15, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x15, AC_VERB_SET_CONNECT_SEL, 0x00},
/* Connect Line-Out side jack (SPDIF) to Side */
	{0x17, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x17, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x17, AC_VERB_SET_CONNECT_SEL, 0x03},
/* Connect Mic jack to CLFE */
	{0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x18, AC_VERB_SET_CONNECT_SEL, 0x02},
/* Connect Line-in jack to Surround */
	{0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x1a, AC_VERB_SET_CONNECT_SEL, 0x01},
/* Connect HP out jack to Front */
	{0x1b, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x1b, AC_VERB_SET_AMP_GAIN_MUfE, AMP_OUT_UNMUTE},
	{0x1b, AC_VERB_SET_CONNECT_SEL, 0x00},
/* Enable unsolicited event for HP jack and Line-out jack */
	{0x1b, AC_VERB_SET_UNSOLICITED_ENABLE, ALC880_HP_EVENT | AC_USRSP_EN}},
	{0x17, AC_VERB_SET_UNSOLICITED_ENABLE, ALC880_HP_EVENT | AC_USRSP_EN},
	{}
};

static void alc_automute_amp(struct hda_codec *codec)
{
	struct alc_spec *spec = codec->spec;
	unsigned int val, mute, pincap;
	hda_nid_t nid;
	int i;

	spec->jack_present = 0;
	for (i = 0; i < ARRAY_SIZE(spec->autocfg.hp_pins); i++) {
		nid = spec->autocfg.hp_pins[i];
		if (!nid)
			break;
		pincap = snd_hda_query_pin_caps(co
	}
}

static void alc_automute_amp_unsol_event(struct hda_codec *codec,
					 unsigned int res)
{
	if (codec->vendor_id == 0x10ec0880)
		res >>= 28;
	else
		res >>= 26;
	if (res == ALC880_HP_EVENT)
		alc_automute_amp(codec);
}

static void alc889_automute_setup(struct hda_codec *codec)
{
	struct alc_spec *spec = codec->spec;

	spec->autocfg.hp_pins[0] = 0x15;
	spec->autocfg.speaker_pins[0] = 0x14;
	spec->autocfg.speaker_pins[1] = 0x16;
	spec->autocfg.speaker_pins[2] = 0x17;
	spec->autocfg.speaker_pins[3] = 0x19;
	spec->autocfg.speaker_pins[4] = 0x1a;
}

static void alc889_intel_init_hook(struct hda_codec *codec)
{
	alc889_coef_init(codec);
	alc_automute_amp(codec);
}

static void alc888_fujitsu_xa3530_setup(struct hda_codec *codec)
{
	struct alc_spec *spec = codec->spec;

	spec->autocfg.hp_pins[0] = 0x17; /* line-out */
	spec->autocfg.hp_pins[1] = 0x1b; /* hp */
	spec->autocfg.speaker_pins[0] = 0x14; /* speaker */
	spec->autocfg.speaker_pins[1] = 0x15; /* bass odec, c AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x14, AC_VERB_SET_CONNECT_SEL, 0x00},
/* Connect Bass HP to Front */
	{0x15, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x15, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x15, AC_VERB_SET_CONNECT_SEL, 0x00},
/* Connect Line-Out side jack (SPDIF) to Side */
	{0x17, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x17, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x17, AC_VERB_SET_CONNECT_SEL, 0x03},
/* Connect Mic jack to CLFE */
	{0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x18, AC_VERB_SET_CONNECT_SEL, 0x02},
/* Connect Line-in jack to Surround */
	{0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x1a, AC_VERB_SET_CONNECT_SEL, 0x01},
/* Connect HP out jack to Front */
	{0x1b, AC_VERB_SET_PIN_WIDGET_CONTROL, caller(kcontrol, ucontrol,
				     snd_hda_mixCESS_new *mi aller(kcontrol, ucontrol,
				     snd_hda_mixEAD |PIN_OUT},
	{0x1b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x1b, AC_VERB_SET_CONNECT_SEL, 0x00},
/* Enable unsolicited event for HP jack and Line-out jack */
	{0x1b, AC_VERB_SET_UNSOLICITED_ENABLE, ALC880_HP_EVENT | AC_USRSP_EN}},
	{0x17, AC_VERB_SET_UNSOLICITED_ENABLE, ALC880_HP_EVENT | AC_USRSP_EN},
	{}
};

static void alc_automute_amp(struct hda_codec *codec)
{
	struct alc_spec *spec = codec->spec;
	unsigned int val, mute, pincap;
	hda_nid_t nid;
	int i;

	spec->jack_present = 0;
	for (i = 0; i < ARRAY_SIZE(spec->autocfg.hp_pins); i++) {
		nid = spec->autocfg.hp_pins[i];
		if (!nid)
			break;
		pincap = snd_hda_query_pin_caps(codec,ERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE },
/* Line-in jack as SurrAC_VERB_SET_COEF_INDEX, 7); },
	 nid, 0,
					   AC_VERB_SET_PIN_SENSE,et = alc_ch Enable HP auto-muting on NID 0x%x\n",
		    spec-c_ch_modend_kcontrol *kcontrol,
			  struct sndAC: HP/Fro/* Unselect Front Mic by default in input mixein jack as Surround */
	{ 0x1a, AC_VEdec, nid);
		if (pincap & AC_PINCAP_TRIG_INSENSE_PR snd_hda_codec_read(codec, nid, 0,
					 A, 0x0d, 2, t snd_kcontrol *kcontrol,
			     struct snC_VERB_GET_PIN_SENSE, 0);
		if (val & AC_PINSENSE_PRESENCE) {
			spec->jack_present = 1;
			break;
		}
	}

	mute = spec->jack_presenpec =},
/* Enable headphone T),
	{ } /* end */
};

/*kcontrol_new alc880_asus_w1T),
	{ } /* end */
};

/* TCL fg.speaker_pins[i];
		if (!nid)
/
};

/* TCL T_SEL, 0x00},
/* Enable unsoliTPUT),
	HDA_CODack and Line-out jack */
	{0x1TPUT),
	HD_MUTE("Headphone Playback Switch", 0x0d, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("CD Playback Volume", 0x0b, 0x04, Her(k1] = 0x== 0x10ec0880)
		, 0x02es >>= 28f (res == ALC880_HP_EVENT)
		alc_automute_amp(codec);
}

static void alc889_automute_setup(struct hda_codec *code->spec;
	struct auto_pin_cfg *cfg = &spec->autocfg;autocfg.hp_pins[0] = 0x15;
	spec->autocfg.speaker_pins[0] = 0x14;
	spc)
{
	struct alc_spec *spec = codec->spec;

	spec->autocfg.hec->autocfg.x0, HDA_INPUT),
	HDA_CODEC_VOLUME("Capture Volum5h", 0x0B, 0x0, HDA_INPUT),
	HDA_CODEC_VOLUME("Capture Volum6h", 0x0B, 0x0, HDA_INPUT),
	HDA_CODEC_VOLUME("Capture Volum7h", 0x0B, 0x0, HDA_INPUT),
	HDA_CODEC_VOLUME("Capture Volum8c->autocfg.speaker_pins[1] = 0x16;
	spec->autocfg.speaker_pins[2] = 0x17;
	spec->autocfg.speaker_pins[3] = 0x19;
	spec->autocfg.speaker_pin_t fixed, ext;
	int i;

	/* there must be only two mic inputa_codec *codec)
{
	alc889_coef_init(codec);
	alc_automute_ams[4] = 0x1a;
}

static void alc889_intel_init_hook(struct hdtic struct snd_kint i;

	/* there must be only two mic inputCODEC_VOLUME("Heint i;

	/* there must be only two mic inputPUT),
	HDA_BIND_int i;

	/* there must be only two mic inputDA_INPUT),
	HDA_int i;

	/* there must be only two mic input, 0x0, HDA_OUTPUT),
	HDA_Blc888_fujitsu_xa3530_setup(struct hda_codec *codec)
{
	struct alc_spec *spec = codec->spec;

	spec->autocfg.hp_pin->spec;
	struct line-out */
	spec->autocfg.hp_pins[1] = 0x1b; /* hp */
	spec->autocfg.speaker_pins[0] = 0x14; /* speakers[0] = 0x17; /* line-out */
	spec->autocfg.hp_pins[1] = 0x1btic struct snd_k *spec = codec->spec;

	spec->autocfg.hp_pinCODEC_VOLUME("He *spec = codec->spec;

	spec->autocfg.hp_pinPUT),
	HDA_BIND_ *spec = codec->spec;

	spec->autocfg.hp_pinDA_INPUT),
	HDA_ *spec = codec->spec;

	spec->autocfg.hp_pin, 0x0, HDA_k as CLFE */
	{ 0x18, AC_VERB_SET_PIN_WIDGc_write(codec, 0x20, 0,
					  	HDA_CODEC_MUTE("CD Playback Switch", 0x0b, 0x030);
	 el AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x14, AC_VERB_SET_CONNECT_SEL, 0x00},
/* Connect Bass HP to Front */
	{0x15, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x15, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x15, AC_VERB_SET_CONNECT_SEL, 0x00},
/* Connect Line-Out side jack (SPDIF) to Side */
	{0x17, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x17, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x17, AC_VERB_SET_CONNECT_SEL, 0x03},
/* Connect Mic jack to CLFE */
	{0x18, AC_VERB_SET_PIN_WIDGET_CONT PIN_ON_MUT_dac_nids	/* identical with w810 */
#define alc880_asus_modes	alc880_threestack_modes	/* 2/6 channel mode */

static struct snd_kcontrol_new alc880_asus_mixer[] = {
	HDA_CODEC_VOLUME("Front Playback Volume", 0x0c, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Front Playback Switch", 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Surround Playback Volume", 0x0d, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Surround Playback Switch"caller(kcontrol, ucontrol,
				     snd_hda_mixEAD | \
	er(kcontrol, ucontrol,
				     snd_hda_mix
		.cok Volume", 0x0e, 1, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("LFE Playback Volume", 0x0e, 2, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE_MONO("Center Playback Switch", 0x0e, 1, 2, HDA_INPUT),
	HDA_BIND_MUTE_MONO("LFE Playback Switch", 0x0e, 2, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("CD Playback Volume", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_MUTE("CD Playback Switch", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_VOLUME("Line Playback Volume", 0x0b, 0x02, HDA_INPUT),
	HDA_CODEC_MUTE("Line Playback Switch", 0x0b, 0x02, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x0b, 0x0, HDA_INPUT),
	HDA_CODEC_MUTE("Mic Playback Switch", 0x0b, 0x0, HDA_INPUT),
	{
		.ifaceboolean_stereo_info

static int alc_cap_sw_get( /* Cnd_ctl_boolea->autocfg.hp_pins[0], 0,
				  AC_VERB_SETlements   sp
	{ 0x1a snd_hda_codec_read(codec, nid, 0,
static struementsmin(dMICt snd_ctl_elem_value *ucontrol)
{
	return alc_caINritten = {
	,
				C_VERB_GET_PIN_SENSE, 0);
		if (val & AC are overwritten val & mld */
statiITED_ENABLE,
				  AC_USRSP_EN | ALC880_HP] = {
	HDA_Cda_codecE_PRESENCE) {
			spec->jack_present = 1mixer[] = {
	>= 26 print*/
};

/*
 * ALC880 ASUS W1V model
 *
 * DAC: HP/Front = 0x02 (0x0c), Surr = 0x03 (0x0d), CLFE = 0x04 (0x0e)
 * Pin assignment: HP/Front = 0x14, Surr = 0x15, CLFE = 0x16,
 *  Mic = 0mute);
	}x0b, 0x03, HDA_INPUT),
	{ } /* end */
23,
		ditional beep mixers; tPlayback Switch", 0x1b, 0x0, H00_mixer[] = {
	HDA_CODEC_VOLUME("Front Playback Volume", 0x0c, 0x0, HDA_OUTPUTpec-> alc_build_controls(sPlayback Switch", 0x1b, 0x0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Headphone PlUTE("Headphone Playback Switch", 0x0d, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("CD Playback Volume", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_MUTE("CD Playback Switch", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x0b, 0x0, HDA_ /*at build (res == ALC880_HP_EVENT)
		alc_automute_amp(codec);->spec;
	s /*HDA_COD (res == ALC880_HP_EVENT)
		alc_automute_amp(codec);p_pins[0]  /*F		return err;
	}

	/* create beep controls if needed */
c)
{
	struer; fo); (res == ALC880_HP_EVENT)
		alc_automute_amp(codec);ec->autocf /*CD*/alc880_tc) {
		 auto_pin_cfg *cfg = &spec->autocfg;
	hda_nidPUT),
	HDA f (res == ALC880_HP_EVENT)
		alc_automute_amp(codec);DA_INPUT), /*HPMUTE("CD Playback ] = 0x16;
	spec->autocfg.speaker_pins[2] = 0x17;
	spec->autocfg.speaker_pins[3] = 0x19;
	spec->autocfg.sp
	hda_nid_t fixed, ext;
	int i;

	/* there must be only two knew = alc_beep_mix0x16;
	spec->autocfg.speaker_pins[2] = 0x17;
	specs[4] = 0x1a;
}

static void alc889_intel_init_hook(OMEM;
			kctl->privue = spec->beep_dec *codec)
{
	struct alc_spec *spec = codec			if (err = 0x16;
	spec->autocfg.speaker_pins[2] = 0x17;
	specDA_INPUT),
	MUTE("Line Playback Switch", 0x0b, 0x02, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x0b, 0x0, HDA_INPUT),
	Hc = codec->spec;
	struct tsu_xa3530_setup(struct hda_codec *codec)
{
; /* hp */
	spec->autocfg.speaker_pins[0] = 0x14; /er(codec, "Master Playbactsu_xa3530_setup(struct hda_codec *codec)
{
s);
		if (err < 0)
			retutsu_xa3530_setup(struct hda_codec *codec)
{
d_hda_find_mixer_cttsu_xa3530_setup(struct hda_codec *codec)
{
DA_INPUT),
AC_VERB_GET_PROC_COEF, 0);
			snd_hda_codec_write(codec, 0x20, 0,
					  verbs[] = {
/* Front Mic: set to PIN_IN );
	struct aflip this bit whicc, 0x20, 0,
					    AC_VERB_SET_COEF_INDEX, 7);DA_CODEC_MUTEGET_CONTROL, PIN alc_cap_getput_caller(kcontrol, ucontAC: HP/Front = nd_hda_codec_amp_stereo(codec, mute);onal beep mixers; the actual parameters are overwritten ("Belable on _new alc880_asus_w1v_mixer[] = {
	HDA_CINPUT),
	HDA_CODEC_MUTE("Beep Playb{0x15, AC_VERB_SET_CONNECT_SEL, 0x00},
/* Conn	if (spec->multiout.dig_out_nid) {
		er= 4 */
	{0x0b, AC_VEyback Switch",
	NULL,
};

/*
 * build control elements
 *2 as the inputl, ucontrol,
				     snd_hda_mixer_amp_switch_get);
}

static int alc_cap_sw_put(struct snd_kcontrol *kcont_flag, >ext_channel_count = spec->channel

	spec->num_mu
	spec->mause I
 
	spec->mltiout.dUT),pcmking pin's inp:ue *ucontrol)
{
	signed*/N_MUTE, AMP_IN_Mook for proc
 */
stause I
 ook for proc
 */
stut mixers (0x0c - 0x0f)
	 *ec->unst vol=0 to output mec->unsut mixers (0x0c - 0xer, "  Processint vol=0 to oer, "  ProcessinC_VERB_SET_AMP_GAIN_MUTE, AMIN_MUTE, AMP_OUT_ZEZERO},
	{0x0f,  enable the retasking pin's input/output as requirx0c -codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = kcontrol->private_value & 0xfnfo

stati CONFIG_SND_HDA_POWER_SA = (kcontrol->private_va0xff;
	long val = *ucontrol->value.integer.value;
	unsigned int ctrl_daET_AMP_GAIda_codec_read(codec, nid, 0,
						    AC_VERB_GET_DIGI_CONVE from r mask = (kcontrol->pms = ||rk.
 */
#ifdef CONFrivate_ol,
			      struct sEBUG */

/* A s_stede;
	speco & mask		{ "CD"	gotoffff;, 0x_mode = 1,
						    0x00);

	/* Set/unset the masked coner_nPlugging headpdata &= ~mask;
	else
		ctrl_data |= mask;
	snd_hda_codec_write_cache(codec, nid, 0, AC_	HDA_CODEC_MUTE(pdif_ctrl_put, \
	  .private_value = nid | (mask<<16) }
#endif   /* CONFIG_SND_DEBUG */

/* A switch control to allow the enaf, AC_VER:char mask = (kcontrol->pIN_MUTE, Alc_eapd_ctrl_get(struct snd_kave_dig_outs = pr hda_codect(structMODE(xnk.
 */
#ifdef CONFIG_Sers
COMPLE char mask = (kcontrol->private_value >> 16) & 0xff;
	lonultiout.hp_ni, HDA_OUalue.integer.value;
	unsigned int val = snd_hda_codec_read(codec, nid, 0,
	r/surround, clfe */
	ec_read(coontrol)
{
	int change;
	struct hda_codec *codec = snd_kcontrol_private_value & 0xffff;
	unsigned char mask = (kcontrol->private_vachip(kcontrol);
	hda_nid_t nid AC_VE1b
		spINSENSE_PRESENCE;
	if/
static strcodec, nid, HDunset the masked control biE, AMP_OUT_MUTE},
	/* Mic2c_pin_bit set.  ThisPIN_WIDGET_E, AMP_OUT_MUTE},
	/* Mic2ctl_boolean_sk) != (ctrl_data & mask);
	ifE, AMP_OUT_MUTE},
	/* Mic2RB_GET_GP	ctrl_data &= ~mask;
	else
eset-codec-callPROC_;
	unsigned int ctrl_data = s-- overri spec-x14, AC_VERB_i_code89 Acer Aspire 8930c, nid, 0,
						    AC_VERB_GET_EAPD_BTLENABLE,
						    0x00);

	/* Set/unsed vref at 80%control bit(s) as needeVERB_SET_PIN_WIDGET_MP_OUT_MUTE},
	/* CD pin wi& mask);
	if (!val)
		ctr
	{0x1a, AC_VERB_SET_PI		ctrl_data |= mask;
	snd_hda_codec_write_cache(codec, nid,solici>num_mixers >=codeurn;
	alc880_6st_dMic", 0x1 },reset->capslsu.au_dig */
staticc_pin_moc = 0x1BASIC]		= "basine"ruct hda_vinvallc880_"
			stack_init_verbs[_1]= {
	/*
	_1stack_init_v		   stn lisstruct stack_init_vedec, ] = {
	p-bp5stack_init_vedec, c},
			WL]= surroun-dtiond
	 */
	{0x11, TC_Td_hdn listp-tc-nd_hdd
	 */
	{0x11, RP(cod_VERB_SEte(codstack_init_vBENQ_ED8n lisda_v	 * Set pin mode T3on lisda_v-t3input pins
	  <= aASSAMDn lis,
		-assamdstack_init_vTOSHIBA_S06n lisG model-s06WIDGET_CONTROL, PIN_RXon lis0x15, ACrxinput pins
	 ULTRAlc880_ct hdstack_init_v virtuad wen lis
#ifde-d westack_init_vNE, 3 = sne5stack_init_vTYANlc880__SETstack_init_v HDAlc880_atio"sted */
		snd_hda_codec_write_cache(co has*codec
					  AC_VERB_SET_PIN_W0te(c0x43DGET"H/*
	 *int alc_subsyO_DA,
	{0x15, AC_VERB_3y
		 8895, "NEC Versa S91ET_P*/
	{0x1NEC},
	{0x16, AC_VER) {
RB_SET_A_BINff0_DEBUontrIN_MP xw series micro "Mi
	{0x11, AC_AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE}3

	/* Mic1 (rear panel) pin widget for input and vref at 80% */
	{0x18, AC_VERB_S7

	/* Mic1 (rear panel) pin widget for input and vref atUTE, AMP_OU28

	/* Mi},
		 AMP_OUT_UN, AC_VERB_SET_},
	{0x16, AC_VERB_SET_B_SET_PI1_WIDGET_CONTROL, PIN_HP},
	{0x19, AF_VERB_SET_AMP_GAIN_MUTE, AMP_OU2_WIDGET_CONTROL, PIN_HP},
	{0x19, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OU3_UNMUTE},
	/* Line In pin widget for input */
	{0x1a, AC_VERB_SET_P4_WIDGET_CONTROL, PIN_HP},
	{0x19, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OU_MUTMUTE},
	/* Line In pin widget for input */
	{0x1a, AC_VERB_SET_P6C_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MC_VERB_SET_AMP_GAIN_MUTE, AMP_OU7C_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	/* CD pin widget for inpuc	/* Mic144ONTROL, PIN_HP},
	_VERB_SET_AMP_GAIN_MUTE, AMP30 AC_* Mic160x1b
 */
static struct hda_verb alc880_pin_w810_inAC_VERBxw80x1b
 */
static struct hda_verb alc880_pin_w810_i2ft_verbThin Cli num2, ACF80;
		else* HP */
	{0x12, AC_VERB_SET_AMP_GAIN_MUTE, AMP_Oal >VERB* lineTROL, PIN_HP}* line},
	{0x16, AC_VERB_SET4ERB_S1_UNMU"Sony 0x14, TE},
	{0x1*/
	{0x14, CONTROL, PIN_OUT},
	{0x15, A82OUT_MSET_AUX-9VERB_SET_PINMUTE},
	{0x16, AC_VERB_SETT_PIN_WIDET_PSET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x16, AC_VERB_SET_PIN_90ic vUT_UNMlc_pTE},
	{0x1INCAP					dig-, 0x03-stWIDGET_CONTROL, PIN_HP},
	2_MUT, AC_VERB Z21M},
	T_CONTROL, PIN_OUTPIN_WIDGET_CONTROL, PIN_HP},
	3configuration:VGN-FW170J_SET_AMP_GAIN_MUE},

	{ }
};

/*
 * Z71V pin4
	{0iguration:Tb, 0G/
static struct hda_verb alc880AIN_MUTE, x15, AT_UNMUTE90VERB_SET_AVERB_S it undUTE, AMP_OUT_UNMUTE},
	{0x16, AC_VERB_S17da_cnt ?T_UNT* ALC8 dynaboithSS RXinputUT},
	{0x14, AIDGET_CONTRx15, AC_VERB_SET_PIN_WIDGET_ffGAIN_, PIN_HPSVERBaker-out = 0x14, HP = 0x15, Mic = 0x18, LcfERB_SE9
	{0F1 = rearMP_OUT_U		   st0x1a, AC_VERB_SET_PIN_WIDGET_42dTROL, PIN_ Life{0x15E841VERB_SET_PIb, AC_VERB_SET_PIN_WIDGET_CONfB_SET29 */
	Tyan Thunder n6650WTROL, PIN_VYANIN_WIDGET_CONTROL, PIN_OUT4,
	{0x14, AC_Vc03IN_W,
				 str_MUTE, AMP_OUT_UB_SETt hda_verb alc880_pin= 0x16,c51ERB_S
				 st45 AMP_OUT_UNMUTE},
	{0x16, AC_VERB_SE7a_GPIx384e, "L#ifde d we yRB_SET_PIN_WIN_OUT},
	{0ack_init_verbs[] = {
	fIDGET056ERB_utex)ED8IN},
	{0x1mode and{0x14, AC_VERB_SET_PIN_WIDGET_8_VREutex)T31-1NTROL, PIN_pin widgERB_SET_AMP_GAIN_MUTE, AMP_OUT_ET_Putex)MUTE, AMP_OUT_UNMUTE__WIDG_flag, size, tlv);
	m modT_MIC)turn;
	s (0x0c -spec->nic struct hda_verb aler is free software; y->setup)
		preGNU General Public Licensx14, AC_VERB_Sion 2 of the License, or
 *  (atm_mux_defs =) any later version.m_mux_defs =init[d)
				reec, G,
	ALC880_W810,
	ALC880_Z71V,
	ALC880et->capsrprivate_value >> 16) & 0et->capsrol, &uinfo->id);
	if = preset->dig_in_nSUS_DIG2,
	it_verbs[]RB_SET_AMP_GAIN_MUTE, AMP_& 1))
		got},
	{0x17, AC_VERB_SET_PIN_WIDGET_CONTRC_PIN_hp1nd_hda_iack;
	struct hda_pcm_stream *stream_T_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},

	{0x18, AC_VERB_SET_PIN_WIDGET_Caudio interface p_dig_outs = prGET_CONTROL, PIN_VREF80},
	{0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	{0x19, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VR	/* Find enumeratedtd)
{
	if (!alc_subsy@suse.de>
 *    de as fallback\ Jonathan Woithe <jws */
		if (tmp == _VREF80},
	{0x19, AC_ the minimum and maximum Laptop
	 * 3~5 ,
	{0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PI_IN (empty by default)_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	{0x1b, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	{0_sta AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x1c, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},

	{ }
};

/*
 * Uniwill pin configuration:
 * HP = 0x14, InternalSpeaker = 0x15, mic = 0x18, internal mic = 0x19,
 * line = 0x/

stru
static struct hda_verb alc880_uniwill_init_verbs[] =* 0 = fr3, AC_VERB_SET_CONNECT_SELE, AMP_OUT_MUHP */

	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PAMP_VAL(spct snd_kcx15, AC_VERB_SET_EAPD_BOL, PIN_OUT},
	{0x17, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},

	{0x18, AC_VERB_SET_PIN_WIDGET_CC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x1c, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},

	{ }
};

/*
 * Uniwill pin configuration:
 B_SET_COEF_INDEX, 0x0bnternalSpeaker = 0x15, mic1, 0x0,
		HDA_OUTPU
static struct hda_verb TE_MONO("LFE Play_VREF80},
	{0x19CLFE, 3, AC_VERB_SET_CONNECT_SELodec, cap_ni},
	{0x17, AC_VERB_SET_PIN_WI0b, 0x04, HDA_INPOL, PIN_OUT},
	{0x17, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},

	{0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	{0x19, AC_VERB_SET_PIN_WIDGEilable on one ADCnternalSpeaker = 0x15, mic ;
	for (i = 0; i 
static struct hda_verb a	break;
		snd_EF80},
	{0x19, AC_VERidget forB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE}30);
	 else
		x1a, AC_VERB_SET_PIN_WIDGET_CONTROL,),
	HDA_CODEC_VOLUM	{0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	/* {0x1b, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP}, */
	/* {0x1b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE}, */
	{0x1c, AC_VERB_SET_PIN_WIDGET_CONTROL,},
			{ "Line", 0x2 x14, AC_VERB_SET_UNSOLICITED_= 0; i < nums; i++)
P_EN | ALC880_HP_EVENT},
	{	    AC_VERB_SET_B_SET_UNSOLICITED_ENABLE, AC_unsigned int *cur_val = ENT},

	{ }
};

/*
* Uniw  it under t_write(codec, 0x10, 0,
					    ill P53
* HP = 0x14, InternalSpeaker = 0x15, mic = 0x19,
 */
static struct hda_verb alc880_uniwill_p53_init_verbs[] = {
	{0x13, AC_VERB_SET_CONNECT_SEL, 0x00}, /* HP */

	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x15, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	{0x15, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x16, AC_VERB_0x12, AC_3, AC_VERB_SET_CONNECT_SEL,snd_hda_codecHP */

	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIda_codec_writOL, PIN_OUT},
	{0x17, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},

	{0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	{0x19, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VRux_idx];

	type = get_wcaps_type(get_wcaps(codec, nid));
	if);
			snd_hda_code_DEBUG
	ALC260_TEST,
#endif
	ALC260_AUTO,
	Ax01}, /* line/0x18, AC_VERB_SET_AMP_GAIN_MUte(codec, 0x_MUTE},
	{0x19, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_printdd("reOL, PIN_OUT},
	{0x17, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},

	{0x18, AC_VERONTROL, PIN_VREF80},
	{0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	{0x19, AC_VERB_SET_PIN_WIDGExt = 0;
	for (i = AUTO_P(err < 0)
lc880_beep_imode and RB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x17, AC_VERB_SET_PIN_WIDGET_CONTR)},
	{0x0d, AC_VERBOL, PIN_OUT},
	{0x17, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},

	{0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	{0x19, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x1*/
	{0x14, AT_CONTROL, PIN_HP},
	{0x16,
			   co0x0b, HDA_INPUT, 1, HDA_AMP_MUTE, bits);
}

statto front */
	{0x_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x15, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	{0x15,MUTE},
	{0x1c, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},

	{ }
};

/*
 * Uniwill pin configuration:
 * HP = 0x14, InternalSpeaker = 0x15, mic = 0x18, internal mic = 0x19,
 * line = 0x1a
 */
static struct hda_verb alc880_uniwill_init_verbs[] =pin widgeRB_SET_AMP_GAIN_MUTE, AMP_Oa_verb alc8880x0b, HDA_INPUT, 1, HDA_AMP_MUTE, bits);
}

statec->control_mutex);ct snd_kcN_IN},
	{0x1a, AC{0x19, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	{0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},
	{0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},

	{0x14, AC_VERB_SET_UNSOLICITED_ENABLE, AC_USRSP_EN | ALC880_HP_EVENT},
	{0x21, AC_VER mic = 0x18, internal mic = 0x19,
 * line = 0x1a
 */
static struct hda_verb alc880_uniwill_init_verbs[] =B_SET_T_CONTROL, PIN_HP},
	{0x16ct hda_code
static signed char alET_CONTROL, PIN_OUT },

	{0x17, AC_VERB_SET_PIN_WIol *kcontroc void alc880_uniwill_mic_automute(struct hda_codec *codec)
{
 	unsigned int present;
	unsigned char bits;

	present = snd_hda_codec_read(codec, 0x18, 0,
				     AC_VERB_GET_PIN_SE/

/* additional mixgned int i;
	structspec->num_mu					>init_hoowithout even the imc->num_adc_nidsnterface for Intel H1						 nglP_OUTEVENT		ERB_GET_VOLUME_KNOB_C(0x0f), CLFE = 0x
static struct hda_verb NE_CAPSRC(num)b alc880_beep_iN_OUT},
	{0xT_CONTROL, PIN_HP},
	{0x16yback Volume", 0xET_AMP_GAIN_MUTE, (0x7000 | (0x01 << 8))},
	{0x0d, AC_VERB_SET_AMP_GAIN instead we flip this bx00 << 8))},
	{0x0d, AC_VERB_SET_AMP_GAIN_MUTE, (0x7000 | (0x01 << 8))},
	{0x0e, AC_VERB_SET_AMP_GAIN_MUTE, (0x7000 | (0x00 << 8))},
	{0x0e, AC_VERB_SET_AMP_GAIN_MUTE, (0x7000 | (0x01 << 8))},

	{0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x18, AC_VERB_SET_AMP_GAIN_MUTE, A= 0x1a, Mic/CLFE = 0x18b alc880_beep_iCONTT_CONTROL, PIN_HP},
	{0x16Front */
},
	{0x17, AC_VERB_SET_PIN_WI	{0x15, AOL, PIN_OUT},
	{0x17, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},

	{0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	{0x19, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x1OL, PIN_OUT}T_CONTROL, PIN_HP},
	{0x16 * ALC888 Acer As0x0b, HDA_INPUT, 1, HDA_AMP_MUTE, bits);
}

stat0xb)},
/* Enable info, \
	* bass */
}

/*
 OL, PIN_OUT},
	{0x17, AC_VERB_SET_AMP_GAIN_MUTE, A80_uniwill_p53_dcvol_VE
	spec->loopbaTE, AMP_OUT_UNMUTE},

	{0x18, AC_VER((res >> 28) == ALspec->init_hoOL_EVENT)
		alc8
	else
		alc_automute_amp_unsol_event(C_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x1c, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},

	{ }
};

/*
 * Uniwill INOUT_NOMICBIAS 0x04

/* Info about the pin modes8930G model
 */

stathe different pin direction modes.
 * ForIN_WIDGET_CONTROERB_SET_PIN_WIDGET_CONTROL, PIN_OUct alc_spb, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x1c, AP_IN_UNMUTE(0)}.hp_pins[0] = 0x14;
	spec->autocfg.speaker_pins[0] = 0x15;
}

static void alc880_uniwill_p53_dcvol_automute(struct hda_codec *codec)
{
	unsigned int present;

	present = snd_hda_codec_read(codec, 0x21, 0,
				     AC_VERB_GET_VOLUME_KNOB_CONTROL, 0);
	present &= HDA_AMP_VOLMASK;
	snd_hda_codec_amp_stereo(codec, 0x0c, HDA_OUTPUT, 0,
				 HnmuteERB_SET_PIN_WIDGET_CONTROL,SET_PIN_Wb, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{88_4ST_ch_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x15, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	{0x15, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x16, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	{0x16, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x17, AC_VERB_SET_PIN_WIB_SET_UNSOLICITED_ENABLE, AC_USRSP_EN | ALC88, alc888_4(kcontrol->private_value >> 16) & 0xff;
	lo 0, AC_VERB_SET_EAPD26TLENABLE,
				  ctrl_data);

	return change;
}

#define ALD_CTRL_SWITCH(	HDA_CODEC_MUTE( nid, mask) \
	{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .index = 0,  \
	  .info = alc#if 0add_vp A s 07/11/05 sn't s zero PCM sspece_CON_USRount FIFO G_SNmask)der-ruSEL, 0x002_inittmesetreo(codec, nid, HDA_INPUT, NIT_GPIO2m_value *ucontrol);

sta7 imptt Indread(codec, nid, 0, AC_VERB_D_DEB0x14, AC_Vnt = _amp_tlv(klume", 0x0d, 0x0, H,
	/* line-out */
	{0x14, AC_VERB_SET_PIN_WIDGET_COECT_SEL, 0x01},
	/* line-out */
	{0x14, AC_VERB_MP_OUT_UNMUNTRO|t hda0x0f),T_AMP_GA16) }
#endif   /* CONFIG_SND_DEBUG */

/*
e input pin config (depending on the given auto-pin ty 0x1b
 */
stat void alc_set 0x1a, f-m, AC_VERB_SET_PINps(codec, nid);
		pincap = (pincF_SHIFT;
		if (pincap & AC_PINCAP_VREF_80)
			val = PIN_VREF80;
		else if (pincap & AC_PINCAP_VREF_50)
			val MP_GAIN_it_verbs >= ARRAY_SIZE(scap & MP_GAIN_Modec)
{
D)
			val = PIN_VREFGRD;
	}
	snd_hda_codec_write(coopback */
	/* Amp IndiET_PIN_WIDGET_CONTROL, val);
}

/*
 */
static void add_mixer(struct alc_spec *spec, struct snd_kcontrol_new *mix)
{
	if (snd_BUG_ON(spec->num_mixers >= ARRAY_SIZE(spec->mixers)))
		return;
	spec->mixers[spec->num_mixers++da_verb a
static voi			    AC_VAMP_IN_MU_codec *codeverb(struct alc_spec *spec, const struct  hda_verb *verb)
{;
}

/*
 */
static void add_mixer(stt_verbs >= ARRAY_SIZE(spec->iMP_GAIN_M)))
		return;
	spec->init_v_CONTROL, PIum_init_verbs++] = verb;
}

#ifdef CONFIG_PROC_FS
/*
x0c - 0x0f)
	 */
	/* seatic void print_realtek_coef(struct sVERB_SET_AMP_GAIN_MUTE	snd_iprintf(buffer, "  Processing CoeffMP_GAIN_MUTE, AMP_OUT_ZE);
	coeff = snd_hda_codec_read(codec, AMP_OUT_ZERO},
	{0x0f, , HDA_OUTtek_coef	NULL
#endif

/*
 * set up from the presling EAPDwheask<s fronr, "  - mutut_muo cond;
	spesistor this is only used in th  PeiSen write_cache(co struct aMP_IN_MUTEOEF_INDEX, ERB_SETIO_DATA,>= 9	hda_nid_t nid e = _codecRB_SET_COEF_INDEX, 0x07},
	{e masked G2, HDAlyVENT)
		alc preset->num_channvent;
	spec->init_hoGAIN_MUTE, A	else
		alc_autGAIN_MUTE, SRSP_EN|ALC880_HP_EVENT},
	{0x21, AC_er(struct amasked GallOLICITED26;
	if (rePIN_WIDGET_CONTROL,HDA_OUT, 0x0 Set/uorkingc->spec;
	int i;

	for (i = 0; i < ARl_put;
	un(preset->mixers) && preset->mixers[i]; i++)
		add_mixer(spec, preset->mixers[id, 0,
 Build-in Mic-In: 0x15
  Universal Ik) \
	et table
 */
static spec->a_MUTE, AMP_OUT_MUio Codec
 *
 s = 3,
	.ite80_uniwill_p53_dcvol_automute(coitems = 30x1e
 */

/*c880_lg_capture_source = {
	.num_s = 3,
	.items = {
		{ "Mic", 0x1 },
		{ "Line", 0x5 },
		nternal Mic", 0x6 },
	},
};

/* 2,4,6 channeles */
signed char mx_enum_isigned chcodec)
{
	x20, AC_VE)))
		 preset->const_channel_cmode */
	{0x20, AC_VEtatic s;

	if (preset->const_channel_count)
		spec->multiout.max_channels = preset->const_channel_count;
	else
		spec->multiout.max_ch_HP},
	/* Fro->channel_mode[0].chd vref at 80%->ext_channel_count = spec->channel_mode[0].channels;

	spec->multiout.num_dacs = preset->num_dIN_MUTE(6)},
ultiout.dac_nids = preset->dac_nids;
	spec->multiout.dig_out_nid = pressoliciMP_OUT8chann,
	{amp_unsoltcfg  (2a_channe)alc88tiout.slave_d8g_outs = preset->slave_dig_outs;
	specDGET_8>capsrc_nids;
	spec->f CONFIG_SND_HDA_POWER_8 0x0b, 0x[bs;

	ch/, PIN_Onid)t_hook;
= snd_3ifdef CONFIG_SND_HDA_POWER_8->init_hoool_new alc8OUT_U;

sta,
	{0xurr,ODEC_VOLUME("Front Playback Volume", GET_ok = preset->init_hook;
8ODEC_VOLUME("Front Playback Vstruct hda_cbs;

	bably
		 *4 preset->loopbacks;
#endif

	if (preset->models */
enl,
			   struct snd_ctl_o_mic)
		/etup(codec);
}

/* Enable GPIO mask and set ou= snd_hda_codec_read(codec, spec->plio1_init_verbs[] = {
	{0x01, AC_VERB_SET_GPIO_MASK, 0x01},
	{0x01, ACspec = codec->spec;
	unsigned y
		 OEF,
					    tmp | 0x2010);
			break;
		case 0x10ec0262:
		case 0x10ec0880:
		case 0x10ec0882:
		case 0x10e},
	{0x01, AC_VERB_SET_GPIO_DATA, 0x03},
	{ }
};

/*
 * Fiin(codec);
	alc_mic_automute(codec);
}

/* additionalInase ALC_INIT_GPIO2:
		snd_hda_s AC_VERB_SET_COEF_INDEX, 7);
			snd_hda_code8	{0x1c, A", 0x0b, 0x04
	HDA_CODEC_VOLUME_MONO("Center Playback Volume", 0x0d, 1, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("LFE("Center Playback Switch", 0x0d, 1, 2, HDA_INPUT),
	HDA_BIND struct alc_pincfg *cfg;

	quirk = snd_pci_quirk_loolume", 0x0b, 0x1, HDA_INPUT),
	HDA_CODEC_MUTE("Mic Playback Switch", 0x0b, 0x1, HDA_INPUT),
	HDA_CODEC_VOLUME("Line Playback Volume", 0x0b, 0x06, HDA_Ids_alt[2Beepx);
	sn0x0f, 0x0, HDA_OUTfUT),
	H1outpuD_MUTE("Side Playback Switch", 0x8ack Vo_spec AC_VC_VOLUME("CD Playback Voswe", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_MfONTROsome codecs, theHDA_INPUT),
	HDA_CODEC_ strB_SET_AMP_GAIN_MUTE,ne PlaybacND_MUTE("Surround Playback Switch", 0x0specPIN_WIDGET_CONTROL, PIN_VREF80 = {
	GPIO mask and set ourivat },
	{ } /* end */
};
/* LSWMP_GAIN_MUTE, AM= {
	{0x0	/* MiMP_IN_UNMUTE(1 0x06, HDA_INPUT),
	HDA_CO0ec0268:
			sn8 */
}

/*
 * ALC888 Acer Aspire 4930G model
 */

static struct hda_verb alc888_acer_aspire_4930g_verbs[] = it_v PIN_HP,
	{ifx04, H_VERB_SET_PIN_WIDGET_CONTROL, G model
tex);
	return err;
}

typedef in, 0);
			snd_hda_codec_write(codec, 0x20, 0,
					    ACe(codec, spec->pllAcidx)L, PIN_VREFack Volume", 0x0f, 0x0, HDA_OUTNMUTE}ec, "	{0x07, AC_VERB_SET_AMP_GAIN_MUTE, AMP80_UN_INPUT),
	HDA_CODEC_VOLUME("CD Playback Volume", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_M2TE("CD Playback Switch", 0x0b, 0x04, HDA_INPUT3,
	HDA_CODEC_VOLUME("Line Playback Volume", 0x0b, 0x02, HDA_INPUT),
	HDA_CODEC_MUTE("Line Playback Switch", 0x0b, 0x02, HDA_I{
	ALC66
			{ "CD", 0x4 },
			{ "Input Mix", 0xa },
			{ "Int Mic", 0xb },
		},
	},
	{
		.num_items = 4,
		.items = {
			{ "Ext Mic", 0x0 },
			{ "Line In", 0x2 },
			{ "CD", 0xr ## num[] = { \
	_DEFINE_CAPMIX(num),	tems = {
	0d, 	FINE_CAPSRC(num),				      \
	{ } /* end */					      \
}

#de30g_setup(struct hda_codec *codec)
{
	struct alc_spec *spec = codec->speext, fixed);
	spec->ext {
	/* s= { \
	_DEFINE_CAPMIX(ontro
{
	struct alc_spec *spec = codec->spec;

	spec->autocfg.hp_pins[0] = 0x15;
	spec->aurround Playback Volume", 0x0d, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Surround Playback Switch", 0x0d, 2, HDA_itch", 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Surround Playback Volume", 0
	{0x19event = alc_sku_unsol_event;
}

/* check subsystem ID alc880_capture_source = {
	.num_items = 4,
	.items = {
		{ "Mic", 0x0 },
		{ "Front Mic", 0x3 },
		{ "Line", 0x2 },
		{ "CD", 0x4 },
	},
};

/* channel source setting (2/6 channel selection for 3-stack) */
/* 2ch mode */
st to input, mute it jack sense */
	{ Playback Switch", 0x0e, 2, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("CD P{
	ALC662_3ST_onOUT },
	{ 0x1a,
	HDA_CODEC_VOLUME_MONO("Center t mic-in to input vref 80%, mute it */
	{
	{0x19, AC_VERB_SET_P_modes[2] = {
	{ 6, alc880_fivestack_ch6_init },
	{ 8, aN_MUTE, AMP_OUT_MUTE },
	{ } /* end */
};

/* 6ch mode */
static struct hda_verb alc880_threestack_ch6_init[] = {
	/* s_threestack_modes

snmute it */
	{ 0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x1a, AC_VERB_SET_AMP_estack_ch AIN_MUTE, AMP_OUT_Uinit(codec);
			break;
/
	{0x0b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_INhreestOUTPUT),
	HDA_CODEC_VOLUME_MONO("LFE Playback Volume", 0x0e, 2, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE_MONO("Center Playback Switch", 0x0e, 1, 2, HDA_INPUT),
	HDA_BIND_MUTE_MONO("LFE Playback Switch", 0x0e, 2, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Line Playback Volume", 0x0b, 0x02, HDA_INPUT),
	HDA_CODEC_MUTE("Line Playback Switch", 0x0b, 0x02, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x0b, 0x0,0x01, AC_VERB_SET_GPIO_DATA, 0x03},
	{ }
};Iore.h>
#ayback Switch", 0x0b, 0x1, HDA_INPUT),
	HDA_CODEC_VOLUME("Line Playback Volume", 0x0b, 0x06, HDA_INPUT),
	HDA_CODEC_MUTE("Line Playback 80_UNspec-E("Internal Mic Playback Volume", 0x0b, 0x01, HDA_INPUT),
	HDA_CODEC_MUTE("Internal Mic Playback Switch", 0x0b, 0x01, HDA_INPUT),
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Channel Mode",
		.info = alc_ch_mode_info,
		.get = alc_ch_mode_get,
		.put = alc_ch_mode_put,
	},
	{ } /* end */
};

static struct hda_verb alc880_lg_lw_init_verbs[] = {
	{0x13, AC_VERB_SET_CONNECT_SEL, 0x00}, /* HP */
	{0x10, AC_VERB_SET_CONNECT_SEL, 0x02}, /LUME("Line Playback Volume", 0x0b, 0x06, HDA_INPUT),
	HDA_COWIDGET_CONTROL, x0e, 1, 0x0, HDAable unsolicited event for HP jack */
	{0x15, AC_VERB_SET_UNSOLICIble speaker output */
	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PI, 0);
			snd_hda_codec_write(codec, 0x20, 0,
					    _PIN_WIDGET_CONTROL, PIN_OUT | PIN_HP},
	{0x15, AC_VERB_SET_AMP_GAIN_MUTE, urn err;
	}
	if p_stereo(codec, 6UT, vmaster_tlv);
		err = snd_hda_add_0xa01UT);
	0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_Uable unsolicited event for HP jack */
	{0x15, AC_VERB_SET_UN 0x1,
			  stspec?cap_mixerdec *codec)
{
	ck */
	{0x15, AC_VERB_SET_UNSOLICITED_ENABLE, ALC880_HP_EVENT | AC_USRSP_annel Mode",
		.info = alc_ch_mode_info,
		.get = al},
/* Enable headphone output */
	{0x15, ACc_cap_getput_callE("Beep Playback Volume", 0, 0, HDA_INPUT),
	HIn, Mic 1 & Mic 2) of t, 0);
			snd_hda_codec_write(codec, 0x20, 0,
					    AC_VERBFE = 0x04 (0x0e)
 * Pin assignment: Front = _ch4_init },
	{G model
           
			   hda_nid           	HDA_CODEC_MUTE("InternatructIDGET_CONs fallback\	HDA_CODEC_MUTE("Internanse */
	tic struct hda_nse */
	C889 Acer Aspire 89P_OUT_U, CLFE = 0x04 (0x0d),
 *      Side = 0x02 (0xd)
 * Pin assignment: Front = 0x14, Surr = 0x17, CLFE = 0x16
 *             ack Switch", 0x0f, 2, H, HDA_INPUT),
	HDA_BIND_P_OUT_U"LFE Playback Switch", 0x0e, 2, 2, HDA_INPUCONNECT_SEL, 0x00}, /* HP */

/* togp_unitch", ts oET_AMte(codec, 0x14, 0,-te_pinTE, AMP_OUT_UNMUTE},
	/* jac, 0x0, HDAitch", 0nse */
	{0x1b, AC_VERB_SET_UNSOL2 },
n", 0x2 },
			{ "CD", 0.speaker_		{ "PUT),
ins[0] = nid;
	}

	alc_init_auto_hp(coa_nid_t mux,
;
	present = snd_hda_codeutocfg.speake;
	ter/LFE		.items?ure Switch", \
specme", 0x0d, 0x0, HDA_OUTPUT),
	HDA_BIN_VERchar mask19, AC_VERund */
	{0x1a,	/* Mume", 0x0d, 0x0, HDA_OUTPUT),
	HDA_BINx1b, AC_VERB_SE
 *  _WIDGET_CONTROL, PIN_OUT}/

	{0x14, AC_VERB_SET_PIN_lor (i = 0; i < ARRAY_SIZE(spec->autocfspec *spe.speaker_pins); i++) apd_ctrlid = spec-> alc_eapx0, HDA_OUTPUT)eapd18, AC_VERB_SET_PIN_WIDGET_CONTROL,/
static v .iface P_EVENT},
	{_AMP_GAIN* toggl*/
#iing to the hp-jack state */= alc_pin_mode_get, 20, AC_VERB__codec_write(codec, 0x15, 0,
					    AC_VERB_SET_EAPD_BTLENABLE, 2);
	TROL, PIN_IN},
/* Unselect Front Mic by default in input mixer 3 */
	{0x22, AC_VERB_SET_AMP_GAIN_MUTE, AMP_6N_MUTE(0xb)},
/* Enable un{
	struct alc_spec *spec = WIDGET_CONTROL, PIN_HP},
	{0x14, AC_VERB_SET_Aaker-output according to the hp-jackn_rim_automute(struct hdaC_VOLUME_MONO("Center Playback Volume", 0m[0];E("Internal Mic Playback Volume", 0x0b, 0x01,uct alc_spec *spec = codec->spec;

	if (!spUT),
	HDA_CODEC_VOLUME_MONO("LFE Playb,
					    AC_VERB_SET_COEF_INDEX, 7);
			snd_hda_codec_write(codec, 0x1a, 0,
					    AC_VERB_SET_P HDA_INPUT),
	HDA_BIND_MUTE_MONO("LFE Playback Switch", 0x0d, 2, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x0b, 0x1, HDA_INPUT),
	HDA_CODEC_MUTE("* mic/clfe */
	{0x12, AC_VERB_SET_CONNECT_Struct alc_spec *spec = codec->spec;

	m[0];
		ife(codec, 0x20, 0,
					    AC_VERB_SET_COEF_INDEX, annel Mode",
		.info = alc_ch_mode_info,
		.get =AMP_OUT_UNMUTE},
	/* jack sense */
	{0x1b, AC_VERB_SET_UNSOLICITED_ENABLE, AC_USRSP_EN | ALC880_HP_EVENT},
	{ }
};

/*c void alc880_m_CODEC_VOLUME("Internal Mic PSET_PIN_WIDGET_CONTROL, PIN_HP},
	{0x1b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	/* ja ALC880) *ic struct hda_verb alc889_acer_aspire_8930g_verbs[] = {
/* Front Mic: set to PIN_IN (empty by default) */
	{0x12, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},
/* Unselect Front Mic by default in input mixer 3 */
	{0x22, AC_IN_MUTE(0xMP_GAIN_MUTE, AMP_change;
	s 0, AC_VERB_SET_GPIO_DAT			 getput_call_t func)
{
	st7_quanta_il alc888_4ST_ch2_intel_init[] = {

					  AC_VERB_SET_CONNECT_lc880_medion_rim_automute(codec);
}

static void alc880_medion_rim_setup(struct hda_codec *codec)
{
	struct alc_spec *spec = codec->spec; HDA_INPUT),
	HDA_BIND_MUTE_MONO("LFE Playback Switch", 0x0d, 2, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playback VAIN_MUTE, AMP_OUT_Uly
		 MUTE, AMP_OUT_MUTE },
/* Line-in  */
static = {
	{0x01,urn K, 0x02,
	HDA_CODEC_MUTE("Internal Ex_fix_pll(struct hhda_amp_list alc880_loopbacks[] = {
	{ 0HDA_INPUT, 0 },
	{ 0x0b, HDA_INPUT, 1 },
	{ 0x0b, HDA_INPUT, 2 },
	{ 0x;

	if (spec-, AMP_OUT_UNMUTE},
	/* speaker-out */
	{0x17, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	{ } /* end */
};
#endif

/*
 * Common callbacks
 */

static int alc_init(struc89 Acer Aspire 89;

	if (spec-codec->spec;
	alc_automute_amp(codec);
	/* toggle EAPD */
	if (spec->jack_presbs; i++)
		snd_hda_sequence_write(codec, spec->init_verbs[i]);

	if (spec->init_hook)
		spec->init_hook(codec);

	return 0;
}

static void alc_unsol_event(struct hda_codec *codec, unsigned int res)
{
 PIN_IN},
/* Connect Internal HP to Front */
	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x1models  AC_VERB_SET_AMP_GAI_PIN_WIDDA_OUTPUTE},
	{A_CODE one bHDA_tsu_xa3530_setup(struct hda_cocited event for HP jdec *codec)
{
	struct alc_specwitch", 0x0e, 2, 2,OUT},
	{0x1b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x1b, AC_VERB_SET_CONNECins); i++) {
		nid = spec->autocfg.hp_pins[i];
		, 0x0d, 2, HICITED_ENABLE, x16,
 *  Mic = 0x18, Line_VER *ucontrol)
{
	return alc_cap_getput_caller(kcontro{0x14, AC_VERB_IN_WIDGET_CONTROL, PIN_OUT},
	{0x17,dec, nid);
		if (pincap & AC_PINCAP_TRIG_REQ) /* need trigger? */
			snd_hda_codec_read(codec, nid, 0,
					   AC_VERB_SET_PIN_SENSE, 0);
		val = snd_hda_codec_read(codec, nid, 0,
					 AC_VER alc880_asus_w1v_mixer[] = {
	HDA_CODEC_VOLUME("Line2 Playback Volume", 0x0b, 0x03, HDA_INPUT),
	HDA_CODEC_MUTE("Line2 Playback Switch",x0, HDA_OUTPUT),
	HDec->jack_present = 1;
			break;
		}
	}

	mute = spec->jack_presruct snd_ctl_elem_value *ucontrol)
{
	return alc_cap_g snd_ctl_elem_va.hp_pins[0]);
	snd_hda_codec_write_cacrol,
				   {0x15, AC_VERB_SET_CONNECT_SEL, 0x80_dig_playback_fg.speaker_pins[i];
		if (!nidl, ucontrol,
		},
	{0x17PCBEEP, &spec-FINE_CAkcontrol->pcap_mixerte, pincap;
	hda_nid_t nid;
	int i;

	spec->jack_preselc880_dig_playback_pcm_prepare(structc->autocfg.hp_*hinfo,
					   struct hda_codec *code	HDA_BIND_MUTE(PIN_WIDG= xname,23h,24hup(codec,0x14, AC_VERB_SET_CONNECT_Sspec *spethe hp-jack state */
static voidct hda_codec *codec)
{
	struct alc_spec",
		    spec->autocf auto_pin_cfg *cN_OUT},
	{0x14, AC_VERB_SUNMUTE(0)},

	/* Unmute input amps (CD, LineT_SEL, 0x00},_codec *codec,
				       struct snd_pcm_substream *substream)
{
	struct alc_spec *spec = codec->spec;rround, clfe */
	0x02, 0x03_UNMUr panel_USRmultiout);
}

/*
 * Digital out
 */
static int alc880_dig_plaAC_VERB_SET_AMP_GAIN_MUTE, sal OUT_ZERO},

	{0x18, /*
 * UniverPIN_WIDGET_CONTROL, 0x24},efinit9on Audio Codec
 *
 * HD audio interface patch aon Audio Codec
 *
 * HD audio interfa0e patch c004 Kailang Yang <kailang@realtek.com.tw>
 *  d004 Kailang Yang <kailang@realtek.com.tw>efini0eon Audio Codecsal Interface for IIN_UNface(0)e patch0fse.de>
 *                    Jonathan Woithe <jwoith10se.de>
 *                    Jonathan Woithe <jwefinition Audio Codecsal Interface for Intelfaceght (c) 2004 Kailang Yaof the GNU General Public Lice
	/* set PCBEEP vol = 0, mute connections */ek.com.tw>
 *                       Jonathan Woithe <jwoithe@physics.adelaide.edu.au>
 *
 *  ThisWoith1is free software; you can redistribute it and/ useful,
 
	{ }
};

static struct snd_kcontrol_new alc268_capture_nosrc_mixer[] = {
	HDA_CODEC_VOLUME("CA PART Volume"terfa3terf0,  theOUTPUT),ee the
 *  GWoithral PubliSwitchnse for more details.
 *
 * { } /* end*  (anty of
 *  MERCHANTABILITY or FITNESS FOR A PARTIalt PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public_DEFINE_CAPSRC(1blic License
 *  along with this program; if not, write to the Free PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public the
 *  GNU Gen_IDXeral Public License1terfac more details.
 *
 *  You should haENT			0x04
#ded a copy 80_MIC_EVENT		0x08

/* ALC88y.h>
#include <2blic License
 *  along with this prohda_input_muxNESS FOR A PARTIsource.  See.num_items = 4,
	.	ALC880_{
		{ "Micnse f0 e paUS_DFront IG,
	ALC180_ASUS_WLinense fo80_ASUS_WCDnse f380_AS},W810,
	ALC880_Z71V,
	ALC880_6ST,
	ALC880acer0_6ST_DIG,
	ALC880_F1734,
	ALC880_3SUS,
	ALC880_ASUS_DIG,
	ALC880_ASUS_WInternalALC880_ASUS_DIG2,
	ALC880_FUJITSU,DIG,
	ALC880_UNIWILL,
	ALC880_UNIWILL_P53,
	ALCdmicC880_CLEVO,
	ALC880_TCL_S700,
	ALC880_LG,
	ALC880_LG_LW,
	ALC880_MEDION_RIM,
#ifdef CONFI6_SND_DEBUG
	ALC880_TEST,
#endif#ifdef CONFIG_SND_DEBUGy of
 *  MERCHANTABILITY or FITNESS FORtesftware
 *  Foun/*ic Lice widget *  (a the
 *  GNU GenerLOUT1 Playbackic License f02 more details.
 *
 *  You shoulndif
	ALC2602AUTO,
	ALC260_MODEL_LAr more details.
 *
 *  YouBINDonfig MONO("Mono sumAUTO,
	ALCd a copy ofsuse1, 2 detaiINC262_HIPPO_1,
	ALC26("LINE-OUT	ALC262_HP_BPC,
	ALC262_HPf_D7000_WL,
	ALC262_HP_BPC_D7000_WHPALC262_HP_TC_T5735,
	ALC262_Hsoft7000_WL,
	ALC262_HP_BPC_D7000_WF,
	ALC26D,
	ALC262_BENQ_T31,
4ALC262_UL	ALC262_HIPPO_1,
	ALC2662_SONY_AD,
	ALC262_BENQ_T31,
5S06,
	ALC262_TOSHIBA_RX1,
	ALC262_SU,
	D,
	ALC262_BENQ_T31,
6S06,
	ALC262_TOSHIBA_RX
 *  GNU GenerMIC1 al Public License ftionore detai,
	ALC262_HP_BPC_D7000_WOSHIBA,
	ALC22_BENQ_T31,
867_QUANTA_IL1,
	ALC268_3ST,
	ALC268_TOSH2BA,
	ALC268_ACER,
	AL9268_ACER_DMIC,
	ALC268_AST,
#endif
	ALCINEIBA,
	ALC268_ACER,
	ALa268_ACER_DMIC,
	ALC268_ACER_ASPIRE_};

/* ALC269 2_BENQ_T31,
aS06,
	ALC262_TOSHIB/* The below appears problemf
 * on some hardware *  (a/* the
 *  GNU Generr versiUTO,
	ALC260_MODEL_Lm.tw8_ACER_DMIC,
	ALLC260_TEST,
#endif
	ALPCM-INIBA,
	ALC268_ACER,
	Aor more details.
 *
 *  You1,
	ALC262_ST,
	ALC861_3ST_d a copy of the_QUANTA_IL1,
	ALC268_3ST,
	ALC268_TST,
	ATEST,
#endif
	ALC268_C_EVENT		0x08

/* ALC880 boM31,
	ALC861_TOSHTEST,
#endd a copy of _S06,
	ALC262_TOSHIEPC_PModes for retasking pin_DEBUG
	ALC26ALCc
 *
MOD0_WF,
	ALC26ALC8mod* ALC864, ,
	ALC86DIR_INO *
 * ,
	ALC861VD_LE_SONY_AC861VD_DALLAS,5	ALC861VD_HP,
	ALC861VD_AUTO,
	ALC861OSHIBC861VD_DALLAS,ion C861VD_HP,
	ALC861VD_AUTO,
	ALC861};

/*C861VD_DALLAS,2004C861VD_HP,
	ALC861VEPC_PCITY or861VD_GPIO
	ALs, assumG,
	they arLicenfigured as outpu6ST_DIG,
	ASUS__DATA_SWITCH("SUS_EEEP 0262_HP80_MI0linux	ALC663_ASUS_H13,
	ALC663_ASUS_150V,
	ALC662_5ST_	ALC663_ASUS_H13,
	ALC663_ASUS_250V,
	ALC6624663_ASUS_MODE3,
	ALC663_ASUS_MODE4,350V,
	ALC6628EEEPC_P7d a coes to alALC2the digital SPDIF3_ASUS_
	ALCto be enabled.
	 _P901,ALC268 does not have an62_AUTOC880_

/* DIG,
	A2_AUT_CTRLH13,
	ALC2_AUTO62_HP_BPC,
	ALC262_HP6LC662_ECSEPC_PA s a coAMSUNGG,
	EAPDMODEL_LAST,
};
  SBOOKlaptops seemMODEuse
/* Athi63_ASUS_MODEturn_LIFan exM,
#ifdamplifier
	ALC882_6STS_A7
	ALC882_ARIMAF,
	ALC26S_A7MEST,
}5,
	ALC262_HP_RP	ALC663_ASUS,
	ALC883_3ST_6ch,_SONY_A6ST_DIG,
	ALC883_TARGA_soft	ALC663ic License
 *  alon#endif

/* createT_DIG, pTO,
	AL/ A PARTicen
	ALC662_ANC10given
	ALC*/y of
 * intNESS FORnew_analog__ASUS_( MERCHAalc_spec *LC88,,
	ALnid_t nid_ASU		   icenst char *ctlname,888_Aidx)
{
	LC883OVO_[32];
	_MD2,
	ALCdac;
	88_Aerr;

	sprintf(OVO_10"%sst tag */
};

/* ALLENOVO_);
	A7J,
	A(nid)0_AScaseLAS,
:C888_6ST_D6:
		dac2 ofx02;
		break;C888_6ST_D5MITAC,
	ALC883_CLEVO_M540RdefaultMITAre
	ALC0;
	}
	if (LC88->multiout.dac2,
	s[0] !=8_LE &&
OP_EA3530,
	ALC883_3ST_6ch_IN1EL,
	ALC	ALC8	err = add_30G,
	AA3530101E,
CTL
 * HD aVinteOVO_1LAPTOP_ the
 MPOSE lateVAL(dac, 3,2ch,_P5Q,
TOP_EA	ALC88ls.
 *
ALC8SU_XALC88< 0)LAPT	ALC8887195_		3530,
	ALC883_3ST_6ch_IN3530,
	ALC883_334,
dacs++E.  _LENOV}_DIG,
	ALC888_LENOVO_SKY,
	ALCd a copy _W66,
	ALC8U_XAnidL,
	883_ll *LC888_ASUS_EEE1601,
	ALC889A_MB31,
	ALCface fASUS_P5Q,	ALC883_SONY_VAIO_TT,
883_882_AUTODEL_LAST,
};

/*elsecensmU,
	C882IO2,
	ALC_INIT_GPIO3,
};

struct alc_mic_route {
	hda_nid_t pin;
	unsigned char mu2_idx;
	unsigned char afor GPIO Poll *
#define GPIO	ALC888_FU}SPIREadd888_ACER_530G,
	ALC8rom_ACERparsed DAC t,
	AL30G,
	ALC888_ACER_ASPauto__4930G_	ALC8
	AL_ctls3_MEDION,
	ALC883_MEDIO,
	ALC2_MODPD,
	A_MEDION,xer pin_cfg *cfg
	ALC_MD2,
	ALC883NOVO_MS7195_DIG,30,
	ALC883_3ST_6ch_I =EL,
	ALpriv	unsST_6ch_I5_DI,
	A= cfg->line int EEPC[0B076PIO1,
	0V,
	APD,
	ALC883_OVO_
/* for rmination!
			type == AUTOc
 *
SPEAKERLASTll */OVO_ = "Speaker"PIO_mix_/
	struct hd1V,
	tream C888_AER_ASPIRE_7730G,
	ALC883_
	ALCar muOVO_100

/* for GPIO Poll */
#define GPIOum {	 * terminas_pcm_s				 */
	unsigned ];	/C8610V,
	ALC888_ASUS_EEE1601,
	ALC889A_MB31,
	ALC1200LAPTOP_da_pcm_sst tag */
};

/* AP5Q,
	ALC883_SONY_VAIO_TT,
ar mux_iACER_DMIC,
	Atruct hda_pcm_stream *stream_analog mix_i,
	ALC888_A hda_pcm_stream *stream_analog_captda_pcm_st
	struct hda_pcm_stream *stream_analog_lt_playback;hp				 */
	unsigned int nu/* playback */
	struct hda_multi_out multioHeadphoC880_Ftruct hda_pcm_stream *stream_analog_alt_playback;tion!
						 *1] |ermination!
						 *NB076tream *stream_60V,
	ALC888_ASUS_EEE1601,
	ALC889A_MB31,
	ALCface ;	/* digC268 models */
enum {
t hda_pcm_stream *stream_digitalST /
	unsigned char arol_new *mixers[max_channels, dacarrays */
	unsig_4930G,88_ACER_ASPIRE_6530G,
	ALC888_
	ALC88i
 *  (ixer;	/* capture mixer */
	unsC880_6beep_amp;	/*
	ALcod883_oute O,
	ALC8et_beep_amp() */

	const struct hda	ALC888,
	Ac_route ext_mic;
	struct_mic;erminal *861_6ST_24)/
	un of
 * voidapture mixer set
	ALC88_and_unhe Lct alc_mic_route int_mic;

	/* 82_MOD_MD2,
	ALC883_888_A	cong[32
	ALC1E_2ch,5_DI,
	ALe					
	ALC883m_channm pcm[3];	/* uture */
	unsigned4 ||contnsigned ie sodx2 ofr amix_o_pin_cfg1LC88nd_ic_route _writeynamic controoftware; you caCONNECT_SEL2_AUT_dac_fix;
	int const_channel_cini_6STned intct alc_mic_route int_mic
	ALC_MEDION,
	ALC883_MEDI terute k;
	sENOV_MD2,
	ALC883			 * don */
cfg.tion!
						 */
	unsigned int nu_rec[3];	/*  = gs() */
g[32A3530,
S];

	/* hooks */init_verld_pchannel_count;
	int ext_channelnamic controls, init_ver}rivate_dac_nids[AUTO_CFG_MAX_OUTShpda_nid_t private_adc_nids[AUTO_CFG_MAX_OUTS];
	hda_nid_t private_capsrc_nids[AUTOpin5_DISUS__MAX_OUTS];

	/*et
					 * dig_oupinto_p *codec, unsigned int res);

	/* for pin pin, 
 *
HP_verags */
	unsigned int
	struct hda_pcm_strea digital I/O only */
	int init_amp;

	/* for virtual mastam *private_dac_nids[AUTO_CFG_MAX_OUTS]onoALC8tructa_nid_t private_adc_nids[AUTO_CFG_MAX_OUTS];
	hda_nid_t private_capsrc_nids[AUTOunsigned_CFG_MAX_OUTS];

	/*
	struct hda_pcm_s_MD2,
	ALChp spec instance
 */
stet
					 * di_MD2,
	ALCtion!_CFG_MAX_OUTS];

	/* hooks */
	void (*iunsigned888_AC,
_vol1,s */nd_k25_DIU_XA identicalream_ax */ to the spec ream_analog__array kctls;
	struct hda_i to the spet_muLAPTOP_EA Audio Codec
 *
 * HD audio inteendif

	/*	PC_Phe LiPURPOed int	structure m#defi_array kctls;
	struct hda_iA_DIG,
;
	hda_nid_t *dac_nidssal Interface 
	hda_nid_onathan Woith1uct hdional */
	hda_nid_t *slave_di
	ALCs;
	unsigned int num_adc_nids;
	hda_nid_t *adc_nids;
	hda_nid_t *capture;

	/* channe*/
	hda_nid_t hp_nid;		/* optional */
	hda_nid_t *slave_dig_outs;
	unsigned int num_adc_nids;
	hda_ut even the imp_t *capsrc_nids;
	hda_nid_t dig_in_nid;
	unsigned int num_channel_mode;
	cout_mux;
	void (*unsum {t snd_kcpes */l_new	ALC8b000 | 0x40;n; eithemaxion SND_/* opp_mixer; /* capture4to_pt hda_codecor Intel Higr amix_ip_mixer; /* capture5 *loopbackstrucndif
};


/*
 *U_XAruct sndp_list *loopbacks;
#endif
};


/*
 * input MU snd_kcontrolstatic int alc_mux_enum_info(struct identicaLC_INITx */ruct snip(kcontroA_INTEL,
	ALS];

	/* hooks */
	voi8_ASUS alc_spec *spec = codec->spec;
	unsigned 2nt mux_idtatic int alc_mopbacks;
#endif
};


/*
 nd_array kctls;
	struct hda_i_LAST /hda_nid;
	void (*setup)(struct hda_ct snd_kcALC88dx = 0;
	return snd_hda_input_playinfo(&spec->input_mux[mux_idx], uinfo);
}
2	/* fo/* pcm_M51VA,
	a or
: identical with82 m880/* o#defineayback *pcm_7730G,
88_ACER_al I880(kcontrol);
	struct d_kcontrol_chip(kcontrol);
 A PARTalc_spec *spec = co A PARTd_kcontrol_chip(kcontrol);
Softsnd_ctl_get_ioffidx(kcontrlue.enumerad_kcontrol_chip(kcon,
	ALC6
	struct alc_spec *sp

static int alcvalu
 * BIOS) */
control)
{
	st
ap_mixer;	/* capture m_kcone *chan51VA,id_t private_adc_nids[AUTO_CFG_MAX_OUTS];
	hda_nid_t private_capsrcO_MS7195_	 of
 * _MD2,
	ALC *codecignoreSE.  S 0 antyd hp_niic int antrol) */
defstruct hm_chann&ec = codec->se, set viatruct hda_inp_verbs aew *mixers[5];	/* mixer U_XA!ec = codec->spec;
	unssk)(stru_XA3530,
S];

	/*di,
	ALsmixer *c_idx] : spec->indx =0V,
	Ainitialization vmax_channel				3_CLEinitialno_7730G,uct snd		goto0,
	_only;
	i}5,
	ALC888_Fcenscan't find validkcontrSUS_truct /* op};
	unsigpture mixer */
	unsigned int beep_a
	ALCx(kcontrol, &u int mux_idx;
	hda_nid_t nid = s	struct hda_p *channel_mode;
	int num_channix-mixer style (e.g. ALC882) */
		unsigned in initialization v_idx >= spec->num_
 &spec->i:EPC_P,
	ALC66c->i support885_IMACAVE
	str[adc_idx] : spec->adc_hda_veritialization vec->adcntical 2 mode_DIGntelNIDPIO_MASK	0 imux->ncodec *[adc_idx] : spec->adc== id*/
	unITSU_XA3530,
kbeep.listgitaldde "hda/* Matrp_stereo(codec, n *cap_mi>capsrc? 0 : adc_&&instance
 */
struct alc_configruct aldnid, HDA_INPUT,
				 *codecbeepe "hda].ind HDAverb	/* MUX style _POWERX_OUTSretusALC88				 Hum6ST, sndc->num_mx, ucoC880_6ST,
=igned in't forgeimux*/
	unt *cur_val&spec- HDA_Ic_boosdynamicx;

		idx = ucontrol->value.enume,
	ALsid_checkd_t dig_in_5g_in_bLLAS,
].ind	ALC8881/
	un_kcontrol_chip(G_MAX_OUTS7730G,
C880_alc_sp2dec *codec = snd_kcontrvalue_OUT call
	ALC1VD_ */
-ontrol)
{
	st1VD_Dl -- overrid	ALC663 SU_PI25c *speux[3];
	stnids[AUTO_CFG_MAX_OUTid_t private_adc_nids[AUTO_CFG_MAX_OUTS];
	hda_nid_t private_capsrcAUTO_CFG_MAX_OUTS];
	hda_niint alc_cated: 1;
	unsigned int j_elem_value *ucontrol)
{
	id;
	unsigned int_elem_value *ucontrol)
{
	= snd_kcontrrol);
	strnids[adc_iunsol_even nid, lcX_OUThoo
			   lem_valul *kreturn snd_hdaand presetsnd_ctl_elemPD,
	ALC883_ *codec_ch_ms[ms; i++1VD_L_LASTE.  Seecount)7_QUANTA_IL1]	= "quanta-il1g_incount);
3ST]	 snd3stack*kcontrol,
	TOSHIBAt sndtoshiba*kcontrol,
	ACER  stru
	ALtruct hda_codec _DMICt snd
	AL-t tand_kcontrol_chip(ASPIRE_ONEtrol);
	staspirruct count);
DELL  strudellnd_hda_ch_moZEPTO  struzepto",,
	ALC260_ACER,
	ALC260_Wl_elem_valE	   struVORI,
		CER_ASct hda_codU>channel */
"IG,
	ALC880_UNIWILL,ic ipci_quirkNESS FOR fg_tblSE.  See
	ALPCI_QUIRK(in_n2ct sn011LENOAcer Ar = s 5720z"101E,a_codec 
 * _channels = spec->ext_chan26l_count>need_dac_fix)
			spec->multiout.num_dacs = sel_count;
		if (s31G50Ved_dac_fix)
			spec->multiout.num_dacs = 30l_count;Extensa 52de of pin widget settings via the mixer.  "pc" pec->mulrol the mod5 of pin widget settings via the mixer.  "pc"5bl_count;
		if (OC880,
	ALC8spec *spec = codec->sp
			spec->multiout.num_C268_A253, "Dell OEM of pin wide_p
			spec->multiou_MAS * Note: somfff	ALC883bd_kconsking Inr = on Mini9/Vostro A9e of pin wi seem tosignlmo
	ALompati
	ALec *cl)
{
	s butsted. op or
alidx = imuxuts;
/* Aec;
	SU,
G,
	3,
	s worIG,
	ontr	ALC882 ignore requests for in3_inpuff0de;
	30enum"HP TX25xx seriese is
 &spel_count);

			spec->multiout.num4r mor1205l_coSUS W7J of pin wi3S*
 * _channels = spec-17	ALC8804entiec->c of pin wiec->c*/
static char *alc_p4c_mode_next_"3_SOAL IFL90/JFL-9
	ALelem_value *uc*/
static char *alc_p521 mode76etaskiverse (CPR2000) of pin widget settings via the mixn_mode_va71, "Qkcont ILUS_Mch_mode_put(struc*/
static char *alc_p85_EVEN177, "LLG R5de of pin wi seem to{rrantyC_P9TherefoC885_MBP {
	Ano unique PCI SSIDore ox->nuoute it pinux[3];
	stconst_channel_count) {
		spekcontr->multiout.max_channels = spec-17UTO,
ff0a, "lue *uc X-20e of pin widbeen observed to have thal widget LENObility (NIDs  HDMI0x0f and 0x10 don't seem to
 * sts for ial widget de;
	the coor bias aA/Lx050f and 0x10 have
static unsssumed  of
 *  MERCHAdec,truct _num_chlue *uconum_chsSE.  See_ch_mode_put(struct880_ASU.PURPOC880_X styl7__kcont_il1e "hdaUX style (e.g. ALC8_kcontrolESS FOR A PARTICULAR PURPO80_ASU.put(codec,LC_PIN_DIR8_basx_put(codec,UX style eap		retus is
 *N_DIR_IN_NOMICBIASn modebout tsequenceitemRRAY_SIZE( *codecST_6ch_I
 * 	verbs
						are given.
 */
minimum aadbs
						mum values are giv5][2] = ->custatic5][2] = {
	  /* ALC_PIN_DIR_INtaticruct snd_uct sinimum a >= spehanne{
	{ 0, 2 },    /* ALVD_D
staticN_DIR_INOUT */
   /* ALC_PI/* AL_ch_mode_gel mode sku__ch_mode_ge/* ALsetup */
	{ 2_IN_NOMICBIASfine out the pintrol mode  ucontroST,
#eontrol,
			  0x02
#define ALC_PIN_DIRed by t    0x03
#defihe Free Software
_DIR_INOUT_NOMIC(e.g. ALC8about the pin modes supported by the differee minimum and maximum values are given.
 */
static signed char alc_pin_mode_di82_MODtl_elem_ininfo[5][2] = {
	{ 0, 2 },    /* ALC_PIN_DIR_IN */_ctl_elem_infinfo 3, 4 },    /* ALC_PIN_DIR_OUT */
	capLAR 4 },    /* ALCivate_value */
	{ 0, 4 },    /* AL imux->num_items; i++) {
			uns/* ALC_PIN_DIR_INOUT */
	{ 2, 2 },    /* ALC_PIN_DIR_IN_NOMICBIAS */
	{ 2, 4 },    /*ec->cur_mux[a	ALC880_6ST_DIG,
	ALC
#define alc_pilue *ucode_max(_dir) (alc_pin_model)
{
	s_info[_dir][1])
#define alc_pin_mode_n_items(_dir) \
	(alc_pin_mode_max(_dir)-alc_pin_mode_min(_dir)+1nt pin direction modes.
 * Formin(dir);
on the minimum and maximum values are given.
 */
static signed char alc_pin_mode_dir_info[5][2] = {
	{ 0, 2 },    /* ALC_PIN_DIR_IN */
	{ 3, 4 },    /* ALC_PIN_DIR_OUT */
	ivate_value >> 16) & 0xff;

	uinfo->type = SNDRV_CTL_EL1;
	uinfo->value.enumerated.items = alc_pin_mode_n_items(dir);

	if (item_num<alc_pin_mode_min(dir) || item_num>alc_p/* ALC_PIN_DIR_INOUl *kcontrol,ICBIAS */
};
#define alc_pin_min(dir);
) (alc_pin_mode_dir_info[rated value  */
he L
#define alc_pidec *de_max(_dir) (alc_pin_mode
	ALC	strcpy(uinfo->value.enumerated.name, alc_pin_mode_names[item_num]);
	return 0;
}

static int alc_pin_mode_get(struct snd_kcontrol *k
	ALC
			    struct snd_ctl_elem_value *ucontrol)
{
	unsigned int i;
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = kcontrol->private_value & 0xffff;
	unsigned char dir = (kcontrol->private_value >> 12nt = 1;
	uinfo->value.enumerated.items = alc_pin_mode_n_items(dir);

	if (item_num<alc_pin_mode_min(dir) 
	ALC880_CLEVO,
	ALAC_VERB_GET_PIN_WIDGET_CON
	ALCICBIAS */
};
#det pinctl setting */
	ALC_mode_dirde_min(dir);
	while(kcont(i <= alc_pin_mode_max(dir) && t tagalc_pin_mode_values[i] != pinctl)
		i++;
	*valp = i <= alc_pin_mode_max(dir) ? i: alc_pin_mode_min(dir);
	return 0;
}

static int alc_pin_mode_put(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	signed int change;
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = kcontrol->private_value & 0xffff;
	unsigned char dir = (kcontrol->private_value >> 16) & 0xff;
	long val = *ucontrol->value.integer.value;
	unsigned int t tag */
};

/* ALCda_codec_read(codec, nid, 0,
						 AC_VERB_GET_PIN_WIDGET_CONTROL,
						 0x00);

	if (val < alc_pi codec->spe(i <= alc_pin_mode_max(dir) && rr = s_onr_info[_ode_n_items(_dir) \
	(alc		 * this turns BIAS 0x04

/* Info about the pin modes supported by the different pin direction modes.
 * Form to be problemati
			    struct snd_ctl_elem_value *ucontrol)
{
	unsigned int i;
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = kcontrol->private_value & 0xffff;
	unsigned char dir = (kcontrol->private_value >> 16) & 0xff;
	long *valp = ucontrol->value.integer.value;
	unsigned int pinctl = snd_hdadec_read(codec, nid, 0,
		lc					 0x00);

	/* Find enumerateid, dir)for current pinctl setting */id, dir)			 0x00);

	if (val < ade_pude_max(_dir) (alc_pin_modeec, x(dir))
		val =out to be necessary in the future.
		 */
		if (val <= 2) {
			snd_hda_codec_amp_stereo(codec, nid, HDA_OUTPUT, 0,
						 , \
	mode_put(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	signed int change;
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = kcontrol->private_value & 0xffff;
	unsigned char dir = (kcontrol->private_value >> 16) & 0xff;
	long val = *ucontrol->value.inte ALC_PIN_DIR_INOUT_NOMICBIAS */
};
#define alc_pin_ganged
) (alc_pin_mode_dir_info[_dir][0])
#define alc_piec->chde_max(_dir) (alc_pin_mode_dir_info[_dir][1])
#define alc_pin_mode_n_items(_dir) \
	(alc_pin_mode_max(_dir)-alc_pin_mode_min(_dir)+1_mode_get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	unsigned int i;
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = kcontrol->private_value & 0xffff;
	unsigned char dir = (kcontrol->private_value >> 16) & 0EM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = alc_pin_mode_n_items(dir);

	if (item_num<alc_pin_mode_min(dir) || item_num>alc_p/* Find enumerated value for current pinctl setting */
	i = alc_pin_mode_min			      spec->num_channel_mode,
				 item_num = alc_pin_mode_minORIT100,
_dir][1])
#define
		if (val <= 2) {
			snd_hda_codec_amp_stereo(codec, nid, HDA_OUTPUT, 0,
						 ut_mux_put(codec,d int val = snd_hda_codec_read(codec, nid, 0,
					      AC_VERB_GET_GPIO_DATA, 0x00);

	*valp = (val & mask) != 0;
	return 0;
}
static int alc_gpio_data_put(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	signed int change;
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = kcontrol->private_value & 0xffff;
	unsigned chvaluCER_ASanty of
 * _rec[atch_ting *id_t private_adc_nids[AUTO_CFG_MAX_OUTS];
	hda_nid *spec boarUS_EEfig *spec i, haspin_m,0];	/* initi = kc = oc(1, sizeof(_MEDI), GFP_KERNEL_verbs a contr= NULLers[5];	/* -ENOMEM chaprivate_capx) ? 0  chaf   /* CONFIsigned int rol,
_f   /* CONFIt_ioffidount);
}

static O,
	ALC882>ext_channel_model allowing dibias op].index,
 intention of< 0x */ intention of>tems; i++}

static to_p intention of this control is
 * to pls;
contrprovide some
	imodec 2n the test mowing digital ocess the mic bias opied if present.  If models are found which can utilise these
 *e;

	,
	ALk( dig_INFO "ic_route : %s:kcontrol,
r will n.\ne is
tl_elemprivatechip_6,
	ALC8utputs a more co10 have bee(*init_f present.  If mo;	/* and 0x10 dl_mode;
c_pinf
 * _kconstruct sndcontrwcaps(codect *cur_val = &ntrol)
{
	struct hint alc_crol_new *mixetype;
dec,fre for pi/* op/
#define GPIO_captureex,
	err) & 0xfspdif_ctrl_get(sinfo(&sp  truct snd_kcoCanenum    upmode,
				   sp"hda_codec_retructcont.  UsG,
	 by 1VD_D..     = ucotrol)
{
	struct hda_co3STnput_mudec = snd_kcontrol_ch!p(kcontrol);
	 *caine define t_ioffidx0x01
#define ALf   /* CONFI]].indx, ucostreaontrol);
	struct e_min(dir) kcontrol);
	struct 	     &sp int change;
	SPIRE_65a_codec *codec = snd_ A PART_chip(kcontrol);
	hda_nlue.enumerad = kcontrol->private_lue.enumera
{
	signed int ch

static int alcd = kcontrol->p

static int alc chaG */

/*cfg aut1VD_(icfg a i <			 imuntroline A; i++apsrc_nids[adc_i					 [i]hip( style (e.g. ALC88type;
ata = snd_hx;
	imVO_M540Rc_spdif_ctrlG */

/*id and hp_niic int aattachpin_modevicid_t dig_in_vate_value >> 16) & 0xff;
	long *valp = ucontrol->value.inte_valu!query_amp 0xffd_t dig_in_dyback;
	struc uco/*_info(coe_ACERamp ivatc->sp= sndgenerator/* opttic int aVERB_SET_hda_codec_write_cache(codec, nie, set vi(0x0c <<d_t AMPCAP_OFFiverSHIFT) |e, nid, mask) \
	{ .iface =NUM_STEPS_CTL_ELEM_IFACE_MIXER7 \
	{ .iface =ndexalues_CTL_ELEM_IFACE_MIX \
	{ .iface =nfig CTL_EL (*init_ex,
						 HDA_AMP_MUTE,>capsrc_][2] = {TE, v);
	C880_6ST,	hda_nid_rol,
 whether NIDde_va is_wcaps_fff;
    */
	stru wcand_hcodeblinec_write_ca07vate_vD_DE cha	signedivate_value >> 16) & 0xff;

	uin* optioget g[32]fff;
bling EAPD digit
	voidblinvate_valu[adc_idx]; */
_specruct blingnd_kC
 * _AUD_INnids[adc_iC880_6ST,, 0,
		ALC880= 1 type;

	mux_ 3, 4 },    /* ALC_PIN_DIR_OUTm_mux_defs ?fo[5][2] = {
	{ 0, 2 },    /* ALC_PIN_DIR_IN ;
	ime must be asserted, 0, 	fixuplc_pinicLC_P *valp = ucoe must be asserted foCONFIG_SND_DEBUG
#define alc_eapdes.
 *else {
		/* MUX style  A PARTICULAR PURPO			    *streamd = kcontrol->private_value & 0xSoftware
			  capture;

	_info	snd_ctl_boolean_mono_info

ic int alc_eapd_ctrl_get(struct snd_kcontrol *kcontr			    = kcontrol->private_value & 0x 0xff;
	lonmode;
    fo, spec->ALC8,
	ALC8fff;
odec_read(codec, nid, 0,
	alue;
	un  AC_
	return chactls;
	stru_cach for pin ESS FOR A te_value[i] is
 *mux private_imux[3];
	hda_nLAPTOdec, nid, 0,
		, nid,
	?LAPTOONFIG_SND_DEBUG[i],
	ALC[0].index MITAol_chip(kcontrol)->da_nid_t nid = (*init_x, ucovmasthe spec iC883_Ccredibly t = al_MBP mode  >> 16) &eturn sd_kcontrol_chip(kcontrol);
	.
 * Agai= xname, .index = 0,_MAX_OUTivate_value roc_DEBUG
e_dir_in,
	AL_realtek_coefm_info *uin*/
	unsil *k(kcont9ALC8 spe 0;
	retsettG,
	(2
	/* Set)snd_c_kcontrox00);
+) {
			unsmodels0+) {
			uns{
	struct hda_c9ven.
 */

 * For0ven.
 */
efine ALC;
	const struct9ctrl_put( intdef CONFADC1turn 272_IG,
	ALC880_Usk;
	else
		ctrl__ctl_elem_vaask;
	sn861_6umed to NOTE:hda_2 = alc)tch cense, edstructa recorodec,*MIXER* mas2_MOD   0mask)enuma mux!snd_c (ctrl_data & maLC_PI (!val)
	LC_PI (ctrl_data & ma_6ST_DIG,
	ALCalc_speclg_lw 0,  \
	  .infoty of
 *  MERCHANTABILITY or FITNESS F9e_dir_info[SE.  See the
 *  GNU Gener1V,
	AUTO,
	ALC260_MODEL_LAST /* last tag */
};

/* ALC262 LC262_ = nid | (mask<C262_TOSHIBA_S0/* last tag */
};

/* ALC262 models */ntroUTO,
	ALC260_MODEL_LA_ctl_AST C268_MODEL_LAST /* last7000_WFtatic void al,
	ALC262_HPpin(struct hda_codec *codec, hdaALC268_TOicic void alc_set_input_pin(stuct hda_codec *codec, hda_nid_tif (auto_pin_ int auto_pin_type
{
	unsigned int val = PIN_IN;

	if (BaticR,
	ALC268
{
	unsigned int val = PIN_IN;

	1V,
	ALC8ic void alc_set_input_pin(st1uct hda_codec *codec, hda_nid_tf (pincap & AC_PINC int auto_pin_type = PIN_VREF80;
		else if (IFT;
		if (pincap p & AC_PINCUTO,uct hda_codec *codec, hda_nid_tt_dac_nid	ALC262_AUTO,
	ALC262_MODENT		0x08

/* ALC880 board config _FUJITSU,
	models */
enum {
	ALC267_QU GNU General Public License
 *  along with this program; if not, write to9IN_NOMICfIAS    0#ifdef CONF85_IMAC	(alc_30G,
	AALC260_TE1,
	AVOL("M = (kic void alc_set_inpunsigned int bind_k = (kcvol  0x0ut thfaLC880SNDRVA_MB3ELEM_IFACE_WITCH/* ALCruct hdIZE(spec->mixersd_t dig_in_.info(val == 0 ?					_hda_A7J,
	_rb * char moderb)
{
	if (snd_BUG_ON(specge};
#deALC8index = 0,  \
k = (kcsw_puverbs))t forgevalucontLC883_SONY_VAIO_TT,
AS,
	A_playback;ls.
 *
 * efint val = PIN_IN;

	if (auto_pin_type <= AUTO_PIN_FRONT_MIC) {
		unsigned int pincap;
		pincap = snd_hda_query_pin_caps(codec, nid);
		pincap = (pincap & AC_PINCAP_VREF) >> AC_PINCAP_VREF_SHIFT;
		iRIM,
#ifdef  & AC_PINCAP_VREF_80)
			val = PIN_VREF80;
		else if (pincapdec, nid, 0, AC_VERB_G0)
			val = PIN_VREF50;
		else if (pincap & AC_PINCAdec, nid, 0, )
			val = PIN_VREF100;
		elsewarranty of
 *  MERCHANTABILITY or FITNESS F9_lifebook_new *mix)
{
	if (snd_BUG_ON(spec->num_mixers >= ARRAY_SIZE(spec->mixers)))
		return;
	spec->mixers[spec->num_mixers++] = mix;
}

static void add_verb(struct alc_spec *spec, const struct hda_verb *verb)
{
	if (snd_BUG_ON(spec->num_init_verbs >= ARRAY_SIZE(spec->init_verbs)))
		return;
	spec->init_verbs[spec->num_init_verbs++] = verb;
}

#ifdef CONFIG_PROC_FS
/*
 * hook for proc
 */
static void print_realtek_coef(struct snd_info_buffer *buffer,
			       struct hda_codec *codec, hda_nid_t nid)
{
	int coeff;

	if (nid != 0x20)
		return;
	coeff = snd_hda_codec_read(codec, nid, 0, AC_VERB_GET_PROC_COEF, 0);
	snd_iprintf(buffer, "  Processing Coefficient: 0x%02x\n", coeff);
	coeff = snd_hda_codec_read(codec, nid, 0,
				   AC_VERB_GET_COEF_INDEX, 0);
	snd the
 *  GNU GenerDock, 0, AC_VERB_GET_PROC_COEF, 0);
3uct hda_codec *codec, hda_nid_tt->const_channel_c int auto_pin_type->multiout.max_channels =  = preset->const_p & AC_PINC_ctlINDEX, 0);
	snd_iprintf(buffer, "  Coefficient Index: 0x%02x\eeepR PURPOSE.  See the
 *  GLC262_ital PCM stream *n config (depending on the given auto-pin type)
 */
ital PCM stream */
	strucendif   /* CONFIG_SND_DEBUG */

/*
 * set _PINCAP_VREF_GRD)
			val = PIN_VREFGRD;
	}
	snd_hda_codec_writeNU Gener_PINCAP_VREF_GRD)
	2_BASIC,
	ALC262_HIPPO,
	ALC262_HI License
 *  along

	ty= (kco	(alc_elemen
	ALC2et->num_dacs;
	spec->multiout.dac_nids pag */
};

PURPOSE.  See the
 *  GNU General Public License f0C268_ACER_DMIC,
	ALC268_A should have received a copy ofnid = preset->dig_in_nid;

	speccap = (pincap & AC_PINCAP_VREF) >> AC_PINCnput_mux;

	spec->numFSC amil

#de(ctrl_data & mafujitsu->caps * Fords = preset->0,
	ALC880_Z71V,
	ALretutruct snd_kcontrol_on moSE.  Seeee s ALCprivate_imux[3];
	hda_ni_VREe patch 2on Audio Codec
 *
 * HD audio inte
 *
INe patch ut */
static st
	{0x01, AC_VERB_SET_GPIHPMASK, 0x01},
	{0x01, ACUNSOLICITED_ENABLa_coal ? 0HP_EVENT | */
USRSP_E_MASK, 0xion Audio CodecSET_GPIO_DATA, 0x01},truct hda_ic sal ? 0MIC

statltek.com.tw>
 *                    Takashi _GPIO_MASK,arranty of
 *  MERCHA(codec);
}

/* En", coeffmask and set output */
static struct hda_verb alc_gpio1_i2004 Kailang Yaruct hda_verb alc_gpio1_init_verbs[] = {
	{0x01, AC_VERB_SET_GPIO_MASK, 0x01},
	{0x01, AC_VERB_SET_GPIO_DIRECTION, 0x01},
	{0x01, AC_VERB_SET_GPIO_DATA, 0x01},
	{ }
};

static struct hda_verb alcs published by
 *  the Free Software Fn Woitght (c) 2004 Kailang Yang <kailang@realtek.TION, 0x01},
	2004 Kailang YaSET_GPIO_DATA, 0x01},
	{ }
};

static struct hda_verb alc_gpio2_init_verbs[] = {
	{0x01, AC_VERB_SET_GPIO_MASK, 0x02},
	{0x01, AC_VERB_SET_GPIO_DIRECTION, 0x02},
	{0x01, AC_VERB_SET_G/* toggleed int n-85_IMACacPD_CTRL_24,
he hp-jue.i of
cap_mixer;	/t const_chsnd_kcontrol_unsignedc_pin_moid_t private_adc_nids[AUTO_CFallow the enaefinentic     */
	sLC883bihis 
	nd_hda_f this contrcan bread			    struct sose mo*
 * UnHD alog PENSE
	hd & 0x80
stati;
	, sp		   l_nid,?ut even the i0) :g autic int alc_muxBUG_Otereod_hda_inputc_SWITCH(xnam val & ~t(struct hda_,c, spstatic int alc_mux_t nid,
			     unsigned int coef_i1x, unsigned int coef_bit)
{
atic int alc_mux_enum_get(struc2nid;
	uns*/
static struEF_INDEXunsignstatic int alc_mux_enum_get(struct = coef_bit;
	alc_fixPROCx_pllC260_8hda_ = coef_idx;
	spec->pll_coef_bit = coef_bit;
	alc_fix_pll(codec);
}

static void alc_automute_pin(struct hda_codec *codec)
{
	struct alc_4pec *_valuepll_nid, 0,
				 AC_VERB_GET_PROC_COEF, 0);
	ssnd_hda_codec_write(codec, spen", coeff 0, AC_VERB_SET_COEF_INDEX,
			    spec->pll_coef_idx);
	snd_hda_codec_write(codec, spec->_P70ND_DEC885_M hPINCAP_VRsocke		if (pll_nid, 0, AC_VERB_SET_PROC_COEF,
			    val & ~(1 << spec->pll_coef_bit));
}

static vdec, nid, 0tems replicdata)    AC_VERB_GET_PIN_SENSE, 0)| 0, AC_VERB_SET_PROC_COEF,
			 um {AC_PINSENSE_PRESENCE) != 0;
	for (i = 0; i < ARid alc_fix_pll_init(struct hda_codec *codec, hda_nid_t nid,
			     unsigned int coef_idx, unsigned int coef_bit)
{
	struct alc_spec *spec = codec->spec;
	spec->pll_nid = nid;
	spec->pll_coef_idx = coef_idx;
	spec->pll_coef_bit = coef_bit;
	alc_fix_pll(codec);
}

static void alc_automute_pin(struct hda_codec *codec)
{
	struct alc_spec *spec = codec->spec;
	unsigned int present, pincap;
	unsigned int nid = spec->autocfg.hp_pins[0];
	int i;

	if (!nid)
		return;
	pincap = snd_hda_q /* need trigger? */
		snd_htrol)utoA7J,
	COEF_INDEX,
			    spec->pll_coef_idx);
	snd_hda__C885_M		     */
	stru
	if (!spdoctrl_d	if (!spec->au, 0, AC_VERB_SET_PROC_COEF,
			 AP_VRcontrNSENSE_PRESENCE) != 0;
	for (i = 0; i < AR;
	if (!spec, 0, AC_VERB_SET_PROC_COEF,
			 c->mu_BUG_ON(!spec->adc_nids))
		return;

	cap_nid =/* L || !sted tems -ERB_SETs apsrc>ext_mic, de  */ decis_hdaAVE
	str;
	if (!spec
	unsc void alc_automute_pin(struc snd_kconbit;
	alc_fix_pct hda_verb a3_verbs a	if (!spec->auNSENSE_PRESENCE;
	if (present) {
		alive = &spec->ext_mic;
		dead = &sphda_nmask; spec->capsrcvaluic;
	} else {
		alive = &spec->int_mic;
		dead = &spec->ext_mic;
	}

	type = get_wcapask)	struct alc_mic_route d_kcontrol__ch_mode_gect alc_mic_route int_mic;

	/*maskmic)
		returnresTO_CFG8_3ST_H269_>> 2 int n88_6S,
	{ }
};

statMITAec, spec->pll_nid, 0, AC_VERB_SET_C*valp = ucVO_M540R,
	ALMASK, 0x02},
	{0UT,
			alive;
	u_AMP_MUTE, HDA_AMP_MUTEned int sense_updated: */
		snd_h HDA_INPUT,
					 alive->mux_idx,
					 H_coef_idx);
	s		snd_hdnids_amp_stereo(ip(kco{ }
};

statgital I/O */
		snd_hda_codec_read(codint alc_ch_moE: analog mixer */
}

x02},
	{0icited event for HP live;
	unsigned, spec->chaec_write(codec, spec->pll_nid, ine id_t private_adc_nids[AUTO_CFG_MAX_OUTS];
	hda_nid_t private_capsrcx, ucoexftwac.ags */	if lc_automute_pin(c = sin_cfg autsigned i_pin(codec);
		9:
		alc_mic_autom0_MIC_EVEN snd_t be asserted oid a>vendor_id == 0x10ec0880)
		res >_mode_dirid_t private_adc_nids[AUTO_CF					 dead->mux_idx,
					 HDA_AMP_MUTE, HDAle (e.g. ALC880) */
		snd	struct alc_mic_route *dead, *ain(codec);
	alc_mic_automute(codec);
}

/* adt for HP jack sensing */
static voi*codec, unsigned int res)
{
	if (codec->vendor_idA, 0x02},
	{ }
};

stat = pret tagput(codec,and set output */
static struct hda_verb alc_gpio1_		al*/
static struct hda_verb al5<jwoithenit_verbs[] = {sal Interface f *);2_HP_30ithender the terms of the GNU Gene(0x7019 | masktrl_g8)is free snit_verbs[] = {
	{0x01, AC_VERB_SET_GPIO_MASK, 0x_gpio2_init_verbs[] = {
	{0x01, AC_VERB_SET_GPIO_MASK, 0x02},
	{0x01, AC_{0x01, AC_VERB_SET_GPIO_DATA, 0x01},ERB_SET_GPIO_MASK, 0};

state pat_SET_GPIO_DATA, 0x02},
	{ }
};

stat = prea 0);
	snd_hda_codec_write(codec, 0x20, 0, AC_VERB_SET_COEF_INDEX, 7);
	if ((tmp & 0xf0) == 0x20)
c_gpio1_888S-VC */
		snd_hda_codec_read(codec, 0x20, 0,
				   AC_VERB_SET_PROC_COEF, 0x83b);
	 else
		 /* alc888-   AC_VERB_SET_PROC_COEF, 0x3030);
}

static void alc889_coef_init(struct hda_codec *codec)
{
	unsigned int tmp;

	snd_hda_codec_write(codec, 0x20, c->pll_nid, 0,
				 AC_VERB_GET_PROC_COEF, 0);
	snd_hda_codec_write(codec, spe 0, AC_VERB_SET_COEF_INDEX,
			    spec->pll_coef_idx);
	snd_hda_codec_write(codec, spec->pll_nid, 0, AC_VERB_SET_PROC_COEF,
			    val &  ~(1 << spec->pll_coef_bit));
}

static void alc_fix_pll_init(struct hda_codec *codec, hda_nid_t nid,
			     unsigned int coef_idx, unnsigned int coef_bit)
{
	struct alc_spec *spec = codec->spec;
	spec->pll_nid = nnsigned int coef_bit)
{
_value_ch_micidefiIN_DIR1VD_HP ;
	snden	*valcodec_write(codec, spe = pre HDA_INPUT,
					 alive->mux_idx,
					 HDA_AAMP_MUTE, 0);
		snd_hda_codec_amp_stereo(codec, cap_nid, HDA_INPUT,
					 dx,
					 HDA_AMP_MUTE, HDA_AMP_MUTE);
	} else {
		/* MUX style (e.g. ALC880) */
		snd_hda_codec_write_cache(codec, cap_C_COEF, 0);>= 28;
	else
		res >>= 26;
	switch (res) {
	case ALC880_HP_EVENT:
		alc_automute_pin(codec);
		break;
	case ALC880_MIC_EVENT:
		alc_mic_automute(codec		     &spec	}
}

static voi5:
		alc_mhook(struct hda_codec *codec)
{
	alc
	tmp = snd>= 28;
	else
		res >>= 26;
	switch (res) {
	case ALC880_HP_EVENT:
		alc_automute_pin(codec);
		break;
	case ALC880_MIC_EVENT:
		alc_mic_automute(codec);
		break;
	}
}

static void alc_inithook(struct hda_codec *codec)
{
	alc = pre ucontrol
	alc_mic_automute(codec);
}

/* aditialization for ALC888 variants */
static void alc888		    ctrl_
	  .itializnd_hdaofhda_cm_rd_BUG_ON(spec->(snd_BUG_ON(ssnd_ctl_elemA, 0x02},
	{ }
};

stat
	snd_hda_codec_wr/*
/* AUint neADC0pec->     uinfo, spec->IMAC24,mic-in	ALC882_VER7ftware; you can redistribute it and/or modify
 *  T,
	t ne 0,
		amps (PCB
/* A nid,In,, 0, 1 &, 0, 2)
		cth_MB5,
7730G,-loop
	ALC	(alc_DEBUG

/* ANote: PASD mo */
f   /s885_s(codeROC_COE 2C663OEF, 0,
		for
/* AfV,
	ApaSet/ted (ted 2)	ALC882d_hdmp Indices:ALC8truc0F, 0)odec1_PROC_truc2_PROC_odec3, CD80_A*  (at yin_tITHOUT ANY WARRANTY; without even the ie <jwoitheauto_hp(struct hda_codec *codec)
{
	structul,
 *  b *spec = codec->spec;

	if (!spec->autocfg.2p_pins[0])
		return;

	if (!spec->autocfg.speaker_p3p_pins[0])
		return;

	if (!spec->autocfg.speaker_p4dec, 0x2
/* AS				  888_coef_init mask) -2_HP__COEF,
				    vol=0OC_C.speaker_pins[lc_init_88S-VC */
		snd_hda_codec_reador Intel High DSET_C7);
	if ((tmp &sal Interface for Intel High Deffg.lineup
						 AC_V->spe: adc_, 0x20, 0F,
					    tmp | 0xol_n of th(alc_= codec_s[0]                              Jonathan Woithe <jwoitheICITED_ENABLE,
				  AC_USRSP_EN | ALC880_HPhp_pins[0]ur option) any later version.
 *
 *  This driver is diur option) any later version.
 *
 *  This drhp_pins[0]use.de>
 *                    Jonathan Woithe <jwoitheuse.de>
 *                    Jonathan Woithmplied wAS,
	AL
	{0x01, AC_VERB_SET_GPIO_DIRECTIONOUit(struct hda_codec *codcodec)
{
	struct alc_spec *spec = 6 (i = AUTO_PIN_LINE; i < AUTO_PIN_LAST; i++)
		ifion Audio Codec
 *
 * HD audio inte
 *
VREF8.tw>
 *  for ALC 260/880/882 codecs
 *
 * Cout_pins[i];
		unsiuct hda_codec *codec)
{
	struct alc_speO_MASK, 0xauto_hp(struct IRECTION, 0x02},
	{0x01, AC_V*/
	for (i = AUTO_PINis 1.
 */
static void alc_fix_pll(strut */
static stis 1.
 */
static void alc_fix_pll(str0;
	for (i = AUof the GNU General Public License ander the terms of the GNU General Public License afor ALC 260/880of the GNU General Public License as published by
 *  the Free Software Foundati (get_defcfg_connect *  the Free Software Foundation
	for (i = AUTO_PINtype = get_wcaps.tw>
 *  ut */
static struct hda_verb ali <tiwopbaIXME:885_ matrix-g[32]k) != 0;
	retsele, or
->autocfM = preset->nu:
	ALC26 invnd_ht_de61 mob->autocfI10ec0888:
1:
	int neMic, F-ter * nidic vo_nid_t   (at yASUSo_hp(struct hda_codec *codec)
{
	struct alc_speca_codec_write_cache(codec, spec->ext_mt auto_pin_cfg *a_codec_write_cache(codec, spec->ext_mic.piins[0]) {a_codec_write_cache(codec, spec->ext_mic.pi    s]);
	snd_S_A7M  (at yo_codec_write_ca,
	ALBTLA, 0x01}2t, fixed);
	spec->ext_mrn 1 if initialized, 0arranty(ctrl_data & maixer */
	unsigned int beep \alue *ucontrol*/
	unsigned int beepr BIOS loading in HD Audio cmic;
	stru16 :	Manufacture ID
 *	mic;
	struX,
	ALC260_ACER,
	AL thePOWER_SAVEr BIOS loading i, 0x20, s = alc_ea_id(stru_ACER_ASPIRE *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chi9(kcontrol);
	struct alc_spec *spec = codec->spec;
	unsigned i{
	unsigned isnd_ctl_get_ioffidx(kcontrol, &ucontrol->id);

{
	uns

static int alc_mux_enum_put(struct snd_kcon & 0xffff;
	if ((ass != codsnd_ctl_get_ioffidxsku;

	/* inval_GPIO_DATA, 0x02},
	kcon int cff;
	if 44kcodec = snd_kcontrol880_F17sub int calc_id =_IN_NOMIs_mgs */d ch0		: PCBEEPamux[8
	 *ratealc_
}

stPCM_RATE_441e co/* fixed 15:1->autocf/* AiBP3,ec->IR_OUbuilnnelm;
	snd.) & 0xers++opes */lc_spec TO,
	ALead *
		nspec->nepSUS_d = 0x17;
	ass = snd_hget_pinff;
	uleane alc_pix17;
	ass = snd_hek: No T,
#endif
	ALC880_AUTO,
	ALad */
	/*
	 * 31~30	: port conneck = (kcont29~21	: reserve
	 * 20		: PCBEEP input
	 * 19~16	: Check t
	 *15:1)
	 * 15~1		: Custom
	 * 0		: override
	*/
	nid = 0x1d;
	if (codec->vendor_id =codec,l *kcontrol,
			    struct snd_ctl_elem_value *u9ontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct alc_spec *spec = codec->spec;
	const struct9hda_input_mux *861 moimux;
	unsigned int adc_idx = snd_ctl_get_ioffidx(kcontrol, &ucontrol->id);
	ff, codex;

		idx = ucontrol->value.enume	struct hda in HD Audio codec.
 *	31 ~ /* Matrix-mixer style (e.g. ALC882) */
		unsigned int *cur_val =Assembly ID
 *	port-A --t_ioffidx(kcontrol, &ux;

		idx = ucontrol->value.enumerated.item[0];
		if (idx >= iminitialization sequence *mux->(*cur_val == idx)
			return 
		for (i = 0; i < imux->num_items; i
	change = (break;
	case 5eo(codec, nid, HDA_INPUT,
						 imux->items[i].ind/
		return snd_hda_i267:
		case  imux, ucontrol, nid,
			d alc_initec->cur_mux[adc_idx]);
	}
}

/*
 * calp = (val & mask) != 0;
	return _kcontrol *kcontrol,
			      struct sRB_SET_EAPD_BT0ue *uco _mux private_imux[3];
	hda_nLAPTOP_ol->private_value & 0xffff;
	unsighannel mode setting
 */
static int alc_ch_mode_info(struct snd_kcontrex,
						 ca\
	(alc_value = ni? 0 : adccontrols;
	spec->capsf (codec-trol *kcontrol,
			    struct snd_ctl_elem_info *uinfo)
{
	struct hda_c in HD _OUTS];
	hda_nontrol,
			   struct snd_ct;
		else if (tmp == 1)
			d int tal I/O only *			return 1;
		else if (tmp == 1)
			= snd_kcontrol_chip(kcontrol);
	struct alc_sspec *spec = codec->spec;
	return snd_hda_ch_mode_info(codec, uinfo, spec->channel_mode,
				    smp == 1)
		annel_mode);
}

static int alc_ch_mode_get(struct snd_kcontrol *kcontrolmp == 1)
			nid = porrol);
	struct a		else
			return 1_t porte, hda_nid_t portd)
c = codec->spec;
	return snd_hda_ch_mode_get(codec, ucontrol, spec->channel_mode,
				   spec->num_channel_mode,
				   spec->ext_IFACE_l_count)9
}

static int alc_ch_mo9_BASontr strubasct alc_spec 9e_put(stFuct  snd_kcont_init_auto_hs of_EEEPC_P703t snd = pr-p703uto_mic(codec);
	}
}

/*90ct sndup pin 90 *kcontrol,9_FUJITSU		alc_cks;
#e */

struct LIFEBOOK		alc_n", coefuto_mic(codec);
	iff (err >= 0 && !spec->const_channel_count) {
		s9 bias option
 * depending on actu7aum {
3bf8he controlFcan presen_hp(codec);
n observed to have this beha83 is us of
E= pr *
 * P900A  struct snic(codec);
	}
}

/*
 *lue.enumeraserved to have this behav88etasstrucF81Ss;
	struct alc_pincfg *cfg;

	quirk = snd_pci_quirk_lookup(cod6a->bus->pci5Qirk);
	if (!quirk)
		return;

	fix += quirk->value;
	cfg = fi7		alus->pcP8e of pin 	for (; cfg->nid; cfg++)
			snd_hda_codec_set_pincfg(c7dec, cfg-U2ix)
g) {
		for (; cfg->nid; cfg++)
			snd_hda_codec_set_pincfg(cs bebs);
}
nid, cfg->val);
	}
	if (fix->verbs)
		add_verb(codec->spec, fix6retass);
}
5id, cfg->val);
	}
	if (fix->vd_pci_quirk *quirk,
			   cnd_h struct alc_fbs
 */
	const struct alc_pincfg *92_ECS,_pci_quirk *quirk,
			   c4E, AMP_OUT_MUTES1,
/* Line-in jack as Line in */
	{ 0x1k as mic in */
	{ 0x18, AC_VERB_SEe8_4ST_ch2_XT_CONTROL, PIN_VREF80 },	{ 0x1a, AC_VERB_SET_PIN73e opti15ltioack.Ampli
 * ALC888alc_pin 0x18, AC_VERB_SET_AMPcIG,
	14ns bas", coef ICH9M- by d
 * ALC8882 val;
}  0x00
#define ALC_PIN_DIR_OUT             0x01
9define ALC_PIN_DIR_INOINIT_DEF0x02
#define ALC_PIN_DIRalc_eapd_ctrabout the pin modes support267:
		case e minimum and maximum values are gmask);
	ifstatic signed char alcmask);
	if */
	{ 0, 4 },    /* ALC_PIN_DIR_INOUT */
	{ 2, 2 },    /*IFACE_MN_DIR_IN_NOMICBIAS */
	{ 2IFACE_Mum<alc_pin_mode_min(dir= 0,  \
	  .info
#define alc__hp(codec);
	UTE, AMP_OUT_MUTE },
/* Lid_kcontrol_new * as Surround */
	{ 0x1a, AC_VERB_SET_PI PortD, 0nable GPIO mask N_WIDGET_CONTROL, PIN_OUT },
	{ 0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE },
/* Line-Out as Front */
	{ 0x17, AC_VERB_SET_CONNECT_SEL, 0x00},
	{ } /* end */
};

/*
 * 6ch mode
 */
static struct hda_verb alc888_e ALC_PIN_MODE(xname,dec, cap_nid, HDA_INPUT, char mask = (kcon0880)
		res >>= 28B_GET_PIN_WIDGET_CONTRlc_automute_pin(codec)88_4ST_ch6_intec);
	}
}

/*
 * UTE, AMP_OUT_MUTE },
/* Li = preset->e minimautocfg.hp/
static nids;
	spec->caps_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{es.
 * For);
	tmp = snd_hda_codecN_WIDGET_CONTROL, PIN_OUT },
	{ 0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE },
/* Line-Out as Front */
	{ 0x17, AC_VERB_SET_CONNECT_SEL, 0x00},
	{ } /* end */
};

/*
 * 6ch mo loud enough) */
	{ 0269:
		case 0x10eCT_SEL, 0x03},
	{ } _PROC_COEF, 0);

 * 8ch mode
 */
static 		case 0x10ec088_4ST_ch8_intel_init[] = {add Mic-in jack as CLFE */
	{ 0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE },
/* Line, 0);
	snd_hda_round */
	{ 0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE },
/* Line-Out as Side */
	{ 0x17, AC_VERB_SET_CONNECT_SEL, 0x03},
	{ } /* end */
};

static struct hda_channel_mode alc888_4ST_8chERB_SET_EAes[4] = {
	{ 2, alc888_4ST_ch2_intel_init },
	{ 4, alalc_pincUTE, AMP_OUT_MUTE },
/* Licks;
#endif

_init },
	{ 8, alc888_4ST_ch8_intel_init },
};

/*
 * ALC888 Fujitsu Siemens Amillo xa3530
 */

static struct hda_verb alc888_fujitsu_xa3530_verbs[] = {
/* Front Mic: set to PIN_IN (empty by default) */
	{0x12, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},
/* Connect Internal HP to Front */
	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x14, AC_VERB_SET_CONNECT_SEL2 val;
};UTE, AMP_OUT_MUTE },
/* Lin", coeff);
}
RB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x18, Aic struct hda_AIN_MUTE, AMP_OUT_UNMUTE },
/* Line-in jack as Surround */
	{ 0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE },
/* Line-Out as CLFE (workaround because Mic-in is not loud enough) */
	{ 0nid, 0,
					  AC_VE
 * 8ch mode
 */
static da_codec *codec)
{ST,
#endif
	ALC88 .put = alc_gpi9_data_put, \
	  .private_value = nid | (mask<<16) }
#endif   /* CONFIG_SND_D switch controlzto allthe enabling of the digital IO pins on the
 * ALC260.  This is incredibly simplistic; thef;
	lix_pll;
	ret_pin(struct hdx04, 151) &  intention of this control is
 * to provide something ;
		spec->inmodel allowing s fallbac;
	unsigned int EBUG
#define alc_spdif_ctrl_infot alc_spdif_ctrl_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct hda_onst s*codec = snd_kcontrol_chip(kcontonst s	hda_nid_t nid = kcontrol->private_value & 0xffff;
	unsigned c& 0xf) != tmp)
		ret->private_value >> 16) & 0xff;
	long *valp = ucontrol->value.integer.value;
	unsigned int val = snd_hda_codec_read(codec, nid, 0,
					      AC_VERB_GET_DIGI_CONVERT_1, 0x00);

	*valp = (val & mask) != 0;
	return 0;
}
statiINIT_DEl bit(s) as = (val == 0 ? 0 : mask) != (ctrl_data & mask);value >> 16) & 0xf;
	long *valp = uc*stream_analog_alctrl_put(struct snd_kcontery_pinontrol,
			      struct snd_c	{ 0x18, Alue *ucontrol)
{
	 strerivate_ubsystem_	struct a7aa hda	hda_nid_Due2_SAMK,
	ALC26TSU,
	AL_LIFLenovo Id_dacad, we need to Lin	: ovcodecs,
	Aeride
	ofache(codI/OOC_C44.1kHzlc_au optiigned int change;
	struct hda_codec ~30	: port connectivity
	PIO_MASK	0ntrol);
	hda_nid_t nid = kcont);
	if (!(ass & 1) && !(e *channel_modunsigned int res)
{
	if (codec->vendor_= 0x10ec0880)
		res >>= 28;
	else
		res >>= 26;
	if (res == Al->private_value & 0x};
	long val = *ucontrol->value.integer.vf ((ass != codec->bus-_chip(kcontrol);sku;

	/* invaltocfg.speaker_pins[0] =6) & 0xff;
	long  3, 4 },    /* Al_data |= :
		alc_meapd_ctrl_get(struct snd_kcontrl_data |=  imux, ucoivate_value >> 16) RB_SET_EAPD_B= spec->capsrcautocfg.hid_t nid;
		tmp = (ass >> 11) t nid(e.g.amp/* Matr_pin_type4uct hda_codenumerated.sk = (kcontrol->private_value >> 16) & 0xff;
	long val = *ucontrol->value.integer.+) {
		nid igned int ctrl_data = mp == 1)
		; port-D --> pin 35/36
 */
static i	if (!spec->odec *co.,
	AL, nid,b; /* hp */
	spec->autOUT_UNMUTE}_id(stru8_ACER_ASread(codec, nid, 0,
						    AC_VERB_GET_EAPD_BTLENABLE,
						    0x00861
	/* Set/unset the masked /6
	/* Set/udx = MUX_1VD_3-t sndbit(s)if ((asite(codepath wayite_ca control s[0];
	id,  void a* Front Moute itions[0]pec->ted ,
	ALCDEBUG
	Atoic = 1;(codec);
			break;
		case 0x10e861_threet snd_ch20; i #ifdef CONF;
	iALC861VD_6 1Ahkcontr nt tigned int   (atnsigned4 Kailang Yang <kailang@realtek.com.e minSET_AMP_GAIN_MUTE, 8hET_PR1/2(0xb)},
/* ,0xb)}ted alsoLAST,
}MB5,
	Ae vref_VERB_SEmp(c.tw>
 *                    Takashi Iwa4e miP to f/
			fixed = nid;
			break;
		c *);
ce mi#if 0OL, PIN_OUT},
	{0x14, AC_VERB_SET_AMP, 0x8
#ifdmask1e
		 /* } 0		mic HP to fNMUTE},
	{0x14, AC_VERB_SET_CONNECT_SEL, 0x002,
/* Connect tion-int = ->autoc License
 *  alonf ((as6ch1VD_Dfault) */
	{0x12, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},
/_ASUS_Glect Front Mic by default in input mixer 3 */
	{6x22, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(0xb)}85_IMAC(B = prurround)nable unsolicited event for HP jack */
	{0x15,4AC_VERB_SET_UNSOLICITED_ENABLE, As voltage on fCLFEic port */.tw>
 *                    Takashi IwOL, PIble unsolicited event ic.pin = ext;
	sp, 0x20ty by default) */
	 Unselect Front Mic bOL, PIN_OUT},
	{0x14, AC_VERB_SET_AMP_GAI8Mic b, AMP_OUT_UNMUTE},
	{0x14, AC_VERB_SET_CONNECT_8EL, 0x00},
/* Connect HP out to front */
	{0x15, AC_VERB_SET_PIN_WIDGC_VERB_SEL, PIN_OUT},
	{0x1 , AC_VERB_SET_AMP_GAIN_MUTE, AGPIO_DATA, 0x02},
	N_DIR_INOUT * input mixer 3 */
LC_PIinfo set ok;
	 input mixer 3 */
	{0x22, ic by d6 Enable headphone outp alc88e mi, AMP>autoE, AC663						 nd
	int nect I AC_VE_adc_nids;
	spec-efault in input uniwi\
	 31
	{0x22, AC_VERB_to front */
	{0x14, AC_VERB_SET_PIN_WIDGET_CONTRnsolicited event for HP jack */
	{0x15, AC_VERB_SET_UNSOLICITED_ENABLE, AMP_GAIN_MUTE, AMPUT | PIN_HP},	 AC_VERDGETt need_dacIN_MUTE, AMP_OUT_UNMUTE},
	{0x15, AC_VERB_SET_C4NNECT_SEL, 0x00},
	{ }
};

/*
 * ALC889 Acer Aspire 893OL, PINUT_UNMUTE},
	{0x14, AC_VERB_SET_CONNECT_SEL, 0x00},
/* Connect HP out toN_WIDGET_CONTROL, PIN_OUT},
	{0x14, AC_VERB_SET_AMP_GA AC_VERB_SETP_OUT_UNMUTE},
/* Enable h AC_VERB_SET_CONNECT_VERB_S4* Connect Internal FronselectOL, PINo PIN_IN (emptnd_SET_-f (c},
	{0x15, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x15,asus
	{0x22, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(0xb)},
/* Enable unsolicited event for HP jack */
	{0x15, AC_VERB_SET_UNSOLICITED_ENABLE, ALC880_HP_EVENT | AC_USRSP_EN},
/* Connect Internal HP to front */
	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x14, AC_VERB_SET_CONNECT_SEL, 0x00},
/* Connect HP out to front */
	{0x15, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x15, AC_VERB_SET_AMP_GAIN_MUTE, AMPUT | PIN_Hna{0x14, AC_VERby default) */
	{0x12, AC_VERB_SET_PIN_WIDGET_CONTROL, NNECT_S alc888_acer_aspire_6530g_verbs[] = {
/* Bias voltage on for external mic port */
	{0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_INe unsolicited event is 1.
 */
static void alc_fixnnecutocfg.lineIN_VREF80},
/* Front Mic: set to PIN_IN (empty by default) */
	{0x12, AC_VERB_SET_PIN_WIDGE0x20, AC_ur option) any later version.
 *
 , AC_VERB_SET_PROCCONTROL, PIN_IN},
/* Unselect Front Mic by default in input mixer 3 */
	{0x22, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(0xb)},
/* Enable unsolicited event for HP jack */
	{0x15, AC_VERB_SET_UNSOLICITED_ENABLE, ALC880_HP_EVENT | AC_USRSP_EN},
/* Enable speaker output */
	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x14, AC_VERB_SET_AMP_GANNECTP_OUT_UNMUTE},
/* Enable hNNECT_SEL, 0x	{0x15, AC_VERB_Sx00},
/* EnabT_CONTROL, t = a-30G mods[]  of
 *  MERCHANTABILITY or FITNESSput c_eapd_ctrl_put, tl_elem_if (snd_BUG_ON(spec->num_mixers 

/*
 * set up the input pin config (dLC262_HIPPO,
	ALC262_HIPPO_pec->multiouternal mC882_W2JC,
	ALC882_TARGA,
	AD;
	}
	snd_hda_codec_write(codec, nidCeIM,
C882_W2JC,
	ALC882_TAR580_3ST, }
};

static struct hda_input_mux alcLFEcapture_sources[2] = {
	/f   /* CONFIG_SND_DEBUG */

/*
 * set SSET_ode[0].channels;
	spepending on the given into a
 * n_IDX_UNDEF;spec->num_mix/*++] = v(codec, nid, 0ALC8t tag */
};

/* ALC86VREFGRD;
	}
	snd_hda_cMODEL_L "  Processing0 },
			{ "Li		val = PIN_VREFGRD;
	}
	snd_hdaALC260_TEST,
#endif
	ALCD,
			{ "Line", 0x2 },
			{ "CD", 0>dig_in_nid;

	spec->unsol only avail		val = PIN_VREFGRD;
	}
_MODEL_LAST /* last tag */
}tatic void alc_set_input
			{ "ruct hda_codec *codec, hda_nid_t nid,
			      int auto_pa },
			{ "Int Mic", 0xb },
		},
N_IN;

	if (auto_pin_type <= AUT
			{ " = PIN_VREF80;
		else if (pincapel_mode[0].channels;
	spx4 },
			{ "Input Mix", 0xa },
		},
	}
 & AC_PINCAP_VREF_50)
			val =
	ALC88 = PIN_hp_nid = preset->hp_nid;

	spec->num_mux_defs = preset->num {
	->multiout.max_ic License
 *  along with this program; if not, write put 3ST turning it into a
 * normal mono mic.
 */
/*  DMIC_CONTROL? Init value = 0x0001 */
	{0x20, AC_VERB_SET_COEF_INDEX, 0x0b},
	{0x20, AC_VERB_SET_PROC_COEF, 0x0003},
	{ }
};

static struct hda_input_mux alc888_2_capture_sources[2] = {
	/* Front mic only available on one ADC */
	{
		.num_items = 4,
		.items = {
			{ "Mic", 0x0 },,
	ALC269_MOe", 0x2 },
			{ "CD", 0x4 },
			{ "Front Mic", 0xb }odec  MUX_IDX_UNDEF;ems = 3,
		.items = {
			{ "Mic", 0x0 },
			{ "Line", 0x2 },
			{ "CD", 0x4 },
		},
	}
};

static struct hda_input_mux alc888_acer_aspire_6530_sources[2] = {
	/* Interal mic only available on one ADC */
	{
		.num_items = 5,
		.items = {
			{ "Ext Mic", 0x0 },
			{ "Line In", 0x2 },
			{ "CD", 0x4 },
			{ "Input Mix", 0xa },
			{ "Int Mic", 0xb },
		},
	},
	{
		.num_items = 4,
		.items = {
			{ "Ext Mic", 0x0 },
			{ "Line In", 0x2 },
			{ "CD", 0x4 },
			{ "Input Mix", 0xa },
		},
	}
};

static struct hda_input_mux alc889_capture_sources[3] = {
	/* Digital mic only available on first "ADC" */
	{
		.num_items = 5,
		.items = {
			{ "Mic", 0x0 },
			{ "Line", 0x2 },
			{rs++] = mix;
}

static void add_verb(struct alc_speC/* Set/	ALChda_verb *verR_OUThad ofc->num_init_verb", 0x0b, 0x0t_verbs)))
		retu0x0b, 0x0bs[sp.enumerated.item;>num_init_verbs+mum values areP_GAIN_MUTE, AMP_OUThook for{ "CD", 0x4 },
			{ "Front Mic", 0xb },
			{ "Input Mix"n(dir);
	strcng it into a
 * normal mono mic.
 */
/*  DMIC_CONTROL? Initc *spec, const struct hd20, AC_VERB_SET_COEF_INDEX, 0x0b},
	{ "Line In", 0x2 },
			{ "CD", 0x4 },
			{ "Input Mix", 0xa },
		},
	}
};

static struct hda_input_mux alc889_captureHDA_INPUT),
	HDA_CODEC_VOLUME("Mic Boost", 0x18, 0, HDA_IN80_HP_EVENT | },
		},
	},
	{
		.num_items = 4,
		.items = {
			{ "Mic", 0x0 },
			{ "Line", 0x2 },
			{ "CD", 0x4 },
			{ "Input Mix", 0xa },
		},
	},
	{
		.num_items = 4,
		.items = {
			{ "Mic", 0x0 },
			{ "Line", 0x2 },
			{ "CD", 0x4 },
			{ "Input Mix", 0xa },
		},
	}
};

static struct snd_kcontrol_new alc888_base_mixer[] = {
	HDA_CODEC_VOLUME("Front Playback Volume", 0x0c, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Front Playback Switch", 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Surround Playback Volume", 0x0d, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Surround Playback Switch", 0x0d, 2, HDA_INPUT),
	HDA_CODEC_VOLUME_MONO("Center Playback Volume", 0x0e, 1, 0x0,
		HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("LFE Playback Volume", 0x0e, 2, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE_MONO("Center Playback Switch", 0x0e, 1, 2, HDA_INPUT),
	HDA_BIND_MUTE_MONO("LFE Playback Switch", 0x0e, 2, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Side Playback Volume", 0x0f, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Side Playback Switch", 0x0f, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("CD Playback Volume", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_MUTE("CD Playback Switch", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_VOLUME("Line Playback Volume", 0x0b, 0x02, HDA_INPUT),
	HDA_CODEC_MUTE("Line Playback Switch", 0x0b, 0x02, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playbac80_HP_EVENT | AC_ 0x0, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Boost", 0x18, 0, HDA_INnsteadurning it into a
 * normal mono mic.
 */
/*  DMIC_CONTROL? Init value = 0x0001 */
	{0x20, AC_VERB_SET_COEF_INDEX, 0x0b},
	{0x20, AC_VERB_SET_PROC_COEF, 0x0003},
	{ }
};

static struct hda_input_mux alc888_2_capture_sources[2] = {
	/* Front mic only available on one ADC */
	{
		.num_items = 4,
		.items = {
			{ "Mic", 0x0 },
			{ "Line", 0x2 },
			{ "CD", 0x4 },
			{ "Front Mic", 0xb },
IND_MUTE("Front Playback Swis = {
			{ "Mic", 0x0 },
			{ "Line", 0x2 },
			{ "CD", 0x4 },
		},
};

static struct hda_input_mux alc888_acer_aspire_6530_source] = {
	/* Interal mic only available on one ADC */
	{
		.num_items = 5,
		.items = {
			{ "Ext Mic", 0x0 },
			{ "Line In", 0x2 },
			{ "CD", 0x4 },
			{ "Input Mix", 0xa },
			{ "Int Mic", 0xb },
		},
	},
	{
		.num_items = 4,
		.items = {
			{ "Ext Mic", 0x0 },
			{ "Line In", 0x2 },
			{ "CD", 0x4 },
			{ "Input Mix", 0xa },
		},
	}
};

static struct hda_input_mux alc889_capture_sources[3] = {
	/* Digital mic only available on first "ADC" */
	{
		.num_items = 5,
		.items = {
			{ "Mic", 0x0 },
			{ "Line", ALC861VD_3Sback Switch", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_VOLUME("Line Playback Volume", 0x0b, 0x02, HDA_INPUT),
	HDA_CODEC_MUTE("Line Playback Switch", 0x0b, 0x02, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playbacnstead of  0x0, HDA_I(codec, addiis list{0x12, AC_VERB_SET_PIN_x2 },
		{ "CD", 0x4 },
	},
};C885_M->capsrc_nids = preset->capsrc_ni only available on one ADC */
	{
		.num_items = 5,
		.items = {
			{ "Ext Mic", 0x0 },
			{ "Line In", 0x),
	HDA_COoef_init(codec);
			break;
		case 0x10ec0888:
			alc888_coef_init(codec);
			break;
		case 0x10ehannels, 7:
		case 0x10ec0268:
			snd_hda_codec_write(codec, 0x20, 0,
					    AC_VERB_S makort-A0xb)}sAC_VERB_(rear AC_VE)Enable unsuse.de>
 *     ut mixer 3 */
	{0x22, AC_VERB_SET 0x0b, 0x02, HDA Unselect Front Mic by),
	HDA_B | AC_US AC_ine Playback ec *cnterl HP to front */
	{0x14, AC_VERB_SET_PIN_WIDGET_CONTR),
	HDA_CC0xb)}x14, AC_ine Playback Volume", olicited event for HP jack */
	{0x15, AC_VERB_S	HDA_D0xb)}1V,
	A_SET_PROC_defcfg_connect(defcfg)) {
		case AMUTE("Line Plaauto_hp(struct 0x0b, 0x02, HDA_INPUT),
	HDA_CE10ec0267IN_W(		    AC_VEk Volume", @physics.adelai
 *
 * HD audio interfcAC_VERB_Srot ne		    PCM},
/HP
	{
		.iface = SNDRV_CTL_0x0b, 0x02, HDA_INPUT),
	HDA_CFDEC_VOLUME("MHDA_OUTPUT),
Volume", 0x0b, 0x0software; you caDA_CODEC_MUTE("Mic Playback Switch", G0xb)}IN_IA_INPUT),
	HDA_CODEC_V1ce = SNDRV_CTL_ELEM_IFACE_MIXER,
		.nAC_VERB_SET_de_info,
		.get = alc_ch_mode_get,
		.put = aHODEC_VSET_ine Playback Volume",t = ont Mic Playback Switch", 0x0b, 0x3, HDA_INPUspec;
	int err;
0x0b, 0x02, HDA_INPUT),
	CD AC_rol,
			 1licited event for HP jack */
	{0x15, AC_VERB_Snnel Mode",
ted tohda_clc_init_ion Audio Codec0x%x\n",
		    ext, fixe0,
				   AC_VERB_SET_PROC_COEFa_codec_read(codec, 0x20snd_hdaDAC0~3 E, vdif 0x0lc_init_on NID 0x%x\n",
		    spec->autocfg.hplc_fix_pll(st	{ "l, int op_flag,
			   unsigned int size, unsigned
			fixed = nid;
			break;
		case AC_JACK_PORT_COMPRGA,l, int op_flag,
			   unsigned int size, unsignedOEF_INDEX, 7);
			tmp = snd_hda_coint size, uns_cap_vol_tlvEF; /*14ET_PR) 1c (4 },
inic porirst  AC_VERB_SET_UNSOLICITED_ENABLE,
				  AC_Ue <jwtl_elem_DA_INPUT);
	err = snd_hda_mixer_amp_tlv(kcontrol,hp_pins[0]                d_hda_mixer_amp_tlv(kcontrol, op_flag, size, tlspec->unsol_event = alc_sku_unsol_event;
}

stati_cap_vol_tlvSd,
		adc_nids5 initiali
			fixed = nid;
			break;
		case his driver is free sct snd_kcontrol *kcontrol,
				 struct snd_cul,
 *  buct snd_kcontrol *kcontrol,
				 struct snd_cins[0]) {N_OUT},
	{0x14, AC_VERB_SET_AMP_GAIN_nect  O5_IMAC0~12 stepHDA_BIOMPLEX:
			if (ext)
				return; /* alrhis driver is free sidx(kcontrol, &ucontrol->id);
	int err;

	muul,
 *  buOEF_INDEX, 7);
			tmp = snd_hda_codec_read(codec, HDA_COMPOSE_AMP_VAL(spec->adc_nids[adc_idx],
			ul,
 *  bunder the terms of the GNU General his driver is free sex_unlock(&codec->control_mutex);
	return erul,
 *  buinvalid entry */
		}
	}
	if (!(gethis driver is free s			   struct snd_ctl_elem_value *ucontrol)
{ul,
 */* hp885_trol_n3 (1V,
	k Volumthe default value is 1.
 */
static vo				  AC_U    slag, size, 

static int alc_cap_vol_put(struct snd_kcontins[0VERB_SET_GPIO_DATA, 0x02},
	{ }
};

put mixer 3 */
, 0x04, HDA_INPUT),
	HDA_CODEC_MUTE("CD Playback Switch", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_VOLUME("Line Playback Volume", 0x0b, 0x02, HDA_INPUT),
	HDA_CODEC_MU_INPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x0b, 0x0, HDA_INPUT),
	HDA_CODEC_MUTE("Mic Playback Switch", 0x0b, 0x0, HDA_INPUT),
	HDA_CODEC_VOLUME("Front Mic Playback Volume", 0x0b, 0x3, HDA_INPUT),
	HDA_CODEC_MUTE("Front Mic Playback Switch", 0x0b, 0x3, HDA_INPUT),
	HDA_CODEC_MUTE("Headphone Playback Switch", 0x19, 0x0, HDA_OUTPUT),
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Channel Mode",
		.info = alc_ch_mode_info,
		.get = alc_ch_mode_get,
		.put = alc_ch_mode_put,
	},
	{ } /* end */
};

/* capture mixer elements */
static int alc_cap_vol_info(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_info *uinfo)
{hip(kcontrol);
	struct alc_spec *spec = codec->spec;
	int err;

	mutex_lock(&codec->cHDA_COMPOSE_AMP_VAL(spec->adc_nids[0], 3, 0,
						      HDA_INPUT);
	err = snd_hda_mixer_amp_volume_info(kcontrol, uinfo);
	mutex_unlock(&codec->control_mutex);
	return err;
}

static int alccap_vol_tlv(struct snd_kcontrol *kcontrol, int op_flag,
			   unsigned int size, unsigned int __user *tlv)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct alc_spec *spec = codec->spec;
	int err;

	mutex_lock(&codec->control_mutex);
	kcontrol->private_value = HDA_COMPOSE_AMP_VAL(spec->adc_nids[0], 3, 0,
						      HDA_INPUT);
	err = snd_hda_mixer_amp_tlv(kcontrol, op_flag, size, tlv);
	mutex_unlock(&codec->control_mutex);
	return err;
}

typedef int (*getput_call_t)(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol);

static int alc_cap_getput_caller(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol,
				 getput_call_t func)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct alc_spec *spec = codec->spec;
	unsigned int adc_idx = snd_ctl_get_ioffidx(kcontrol, &ucontrol->id);
	int err;

	mutex_lock(&codec->control_mutex);
	kcontrol->private_value = HDA_COMPOSE_AMP_VAL(spec->adc_nids[adc_idx],
						      3, 0, HDA_INPUT);
	err = func(kcontrol, ucontrol);
	mutex_unlock(&codec->control_mutex);
	return err;
}

static int alc_cap_vol_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	return alc_cap_getput_caller(kcontrol, ucontrol,
				     snd_hda_mixer_amp_volume_get);
}

static int alc_cap_vol_put(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	retur alc_cap_getput_caller(kcontrol, ucontr AC_VERB_SET  snd_hda_mixer_amp_volume_put);
}

/* capture mixer elements */
#define alc_cap_sw_info		snd_ctl_boolean_stereo_info

static int alc_cap_sw_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	return alc_cap_getput_caller(kcontrol, ucontrol,
				     snd_hda_mixer_amp_switch_get);
}

static int alc_cap_sw_put(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	return alc_cap_getput_caller(kcontrol, ucontrol,
				     snd_hda_mixer_amp_switch_put);
}

ueryALC8he(coDEL_Lite(co ins[i]
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.nyback SwitCTL_ELEM_IFACE_MIXER, \
		.name = "Capture Switch", \
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE, \
		.count = num, \
		.info = alc_cap_sw_info, \
		.get = alc_cap_sw_get, \
		.put = alc_cap_sw_put, \
	}, \
	{ \
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
		.name = "Capture Volume", \
		.access = (SNDRV_CTL_ELEM_ACCESS_READWRITE | \
			   SNDRV_CTL_ELEM_ACCESS_TLV_READ | \
			   SNDRV_CTL_ELEM_ACCESS_TLV_CALLBACK), \
		.count = num, \
		.info = alc_cap_vol_info, \
		.get = alc_cap_vol_get, \
		.put = alc_cap_vol_put, \
		.tlv = { .c = alc_cap_vol_tlv }, \
	}

#define _DEFINE_CAPSRC(num) \
	{ \
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
		/* .name = "Capture Source", */ \
		.name = "Input Source", \
		.count = num, \
		.info = alc_mux_enum_info, \
		.get = alc_mux_enum_get, \
		.put = alc_mux_enum_put, \
	}

#define DEFINE_CAPMIX(num) \
static struct snd_kcontrol_new alc_capture_mixer ## num[] = { \
	_DEFINE_CAPMIX(num),				      \
	_DEFINE_CAPSRC(num),				      \
	{ } /* end */					      \
}

#define DEFINE_CAPMIX_NOSRC(num) \
static struct snd_kcontrol_new alc_capture_mixer_nosrc ## num[] = { \
	_DEFINE_CAPMIX(num),					    \
	{ } /* end */						    \
}

/* up to three ADCs */
DEFINE_CAPMIX(1);
DEFINE_CAPMIX(2);
DEFINE_CAPMIX(3);
DEFINE_CAPMIX_NOSRC(1);
DEFINE_CAPMIX_NOSRC(2);
DEFINE_CAPMIX_NOSRC(3);

/*
 * ALC880 5-stack model
 *
 * DAC: Front = 0x02 (0x0c), Surr = 0x05 (0x0f), CLFE = 0x04 (0x0d),
 *      Side = 0x02 (0xd)
 * Pin assignment: Front = 0x14, Surr = 0x17, CLFE = 0x16
 *                 Line-In/Side = 0x1a, Mic = 0x18, F-Mic = 0x1b, HP = 0x19
 */

/* additional mixers to alc880_three_stack_mixer */
static struct snd_kcontrol_new alc880_five_stack_mixer[] = {
	HDA_CODEC_VOLUME("Side Playback Volume", 0x0d, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Side Playback Switch", 0x0d, 2, HDA_INPUT),
	{ } /* end */
};

/* channel source setting (6/8 channel selection for 5-stack) */
/* 6ch mode */
static struct hda_verb alc880_fivestack_ch6NNECT, 0x04, HDA_INPUT),
	HDA_CODEC_MUTE("CD Playback Switch", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_VOLUME("Line Playback_writeB_GET_PROC_Csnd_h#0ack_chi(codec267:
		nal HP to fr0x0b, 0x02, HDA_INPUT),
	HDA_CODEC_MUame =int wasTE | \fivestannel Mode",
		.info = alc_ch_moyback Switch", 0x0b, 0x02, HDA_US_DIGde */
static struct hda_verb alc880_fivestack_ch8_init[] = {
	/* set line-in to output, unmute it */
	{ 0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE },
	{ } /* end */
};

static struct hda_channel_mode alc880_fivestack_modes[2] = {
	{ 6, alc880_fivestack_ch6_init },
	{ 8, alc880_fivestack_ch8_init },
};


/*
 * ALC880 6-stack model
 *
 * DAC: Front = 0x02 (0x0c), Surr = 0x03 (0x0d), CLFE = 0x04 (0x0e),
 *      Side = 0x05 (0x0f)
 * Pin assignment: Front = 0x14, Surr = 0x15, CLFE = 0x16, Side = 0x17,
 *   Mic = 0x18, F-Mic = 0x19, Line = 0x1a, HP = 0x1b
 */

static hda_nid_t alc880_6st_dac_nids[4] = {
	/* fronOL, PIN_INol);
	struct alc_spec *spec = codec->spec;
	int err;

	mutex_lock(&codec->control_   SNDRV_CTL_ELEM_ACCESS_TLV_READ | \
			   SNDRV_CTL_ELEM_ACCESS_TLV_CALLBACK), \
		.count = num, \
		.info = alc_cap_vol_info, \
		.get = alc_cap_vol_get, \
		.put = alc_cap_vol_put, \
		.tlv = { .c = alc_cap_vol_tlv }, \
	}

#define _DEFINE_CAPSRC(num) \
	{ \
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
		/* .name = "Capture Source", */ \
		.name = "Input Source", \
		.count = num, \
		.info = alc_mux_enum_info, \
		.get = alc_mux_enum_get, \
		.put = alc_mux_enum_put, \
	}VAL(spec->adc_nids[0], 3, 0,
						      HDA_INPUT);
	err = snd_hda_mixer_amp_tlv(kcontrol, op_flag, size, tlv);
	mutex_unlock(&codec->control_mutex);
	return err;
}

typedef int (*getput_call_t)(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol);

static int alc_cap_getput_caller(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol,
				 getput_call_t func)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct alc_spec *spec = codec->spec;
	unsigned int adc_idx = snd_ctl_get_ioffidx(kcontrol, &ucontrol->id);
	int err;

	mutex_lock(&codec->control_mutex);
	kcontrol->private_value = HDA_COMPOSE_AMP_VAL(spec->adc_nids[adc_idx],
						      3, 0, HDA_INPUT);
	err = func(kcontrol, ucontrol);
	mutex_unlock(&codec->control_mutex);
	return err;
}

static int alc_cap_vol_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	return alc_cap_getput_caller(kcontrol, ucontrol,
				     snd_hda_mixer_amp_volume_get);
}

static int alc_cap_vol_put(struct snd_kcontrol *);
}

static int alc_cap_vol_put(struct snd_kcont
static structCODEC_VOLUME_Mis biSurrou1VD_strucC885_MBP{0x15, AC_VERB_SET_CONNECT_SEL, 0x00},UT),
	H
	snd_hda_codec_wri_dig_ouont Mic Playback Switch", 0x0b, 0x35l speakHP		 A_VAL(spec-ct snd_kcontrol *kcontrol,
				 strucPlaybacl speak */
	x14, AC_ for HP{
		if ((asMUTE_MONO("LFE Playback Switch", 0x0e, 2, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("CD Playback Volume",G_MAX_OUTS4, HDA_INPUT),
	HDA_CODEC_MUTE("CD Playback Switch", 0x0b, 0x04, HDA_INPUT),
e_info(kcontrol, uinfo);
	mutex_unlock		return;ex_unlock(&codec->control_mutex);
	return err;
}
 \
		.tlv = { .c = alc_cap_vol_tlv }, \
	}

#define _DEFINE_CAPSRC(num) \
	{ze, unsigned int __user *tlv)
{
	struct hda_codec odec = snd_kcontrol_chip(kcontrol);
	struct alc_sp *spec = codec->spec;
	int err;

	mutex_lock(&code>control_mutex);
	kcontrol->private_value = HDA_COMPOSE_AMP_VAL(spec->adc_nids[0], 3, 0,
						      HDA_INPUT);
	err = snd_hda_mixer_amp_tlv(kcontrol, op_fHDA_INPUT);
	err = snd_hda_mixer_amp_tlv(kcontrol,eturn err;
}

typedef int (*getput_call_t)(struct snd_kcontr err;
}

typedef int (*getput_call_t)(struct snd_k);

static int alc_cap_getput_caller(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol,
				 getput_call_t func)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct alc_spec *spec = codec->spec;
	unsigned et_ioffidx(kcontrol, &ucontrol->id);
	int er

	mutex_lock(&codec->control_mutex);
	kcontrol->prite_value = HDA_COMPOSE_AMP_VAL(spec->adc_nids[adc_idriver is free sstack_modes	/* 2/6 channel mode */

staticrol);
	mutex_unlock(&codec->control_mutex);
	rern err;
}

static int alc_cap_vol_get(struct snd_kcorol *kcontrol,
			   struct snd_ctl_elem_value *uconol)
{
	return alc_cap_getput_caller(kcontrol, ucontrclusively */
	fo
static int alc_cap_vol_put(struct snriver is free s Playback Switch", 0x0d, 2, HDA_INPUT),
	Hul,
 *  bu Playback Switch", 0x0d, 2, HDA_INPUT),
	H	struct al Playback Switch", 0x0d, 2, HDA_INPUT),
	H    spec-pport */
	snd_printdd("realtek: EnaPUT),
	HDA_CODEC_V*spec = codec->spec;

	if (!spec->autocfg.hp_pins[0	HDA_BIND_MUTE_MONO("LFE Playback Switch", 	struct al		if (spec->autocfg.line_out_pins[0] &&
		    sume_info(kcontrol, uinfo);
	mutex_unlockRB_SET_U 0);
	_get_ilc_cap_getput_caller(kcontrol, ucontro(dir);
};
#define ALC880_ithe@physics.adelaiec)
{
	unsigned int tmp;

	snd_hda_codec_write(codeccodec, spec->pll_nid, 0,
				 AC_VERB_GET_PROC_COEF, 0);
	snd_hda_codec_write(codec,, HDA_INPUT)ERB_SET_COEF_INDEX,
			    spec->pll_coef_idx);
	snd_hda_co_SENSE, 0);
	spec->jack_present = (preseg_outs;
	unsignback Switpec->pll_coef_bit));
}

static vostruct alc_spec *spec = codec->spe&codB_SET_EAPD_BTLENABUS W1signface ffix_pll_inAC: HP/Frontcode
{
	struct alc_spec *spec = codec->spelume int coef_i /* A* DAC: HP/Front = 0x02 (0x00 :0c), Surr = 0alc888_coef_init(stru, HDA_INPUT)	case 0x10ec0272:
		case 0x10ec0660:
		case 0x0x10ec0662:
		case 0x10FIXME: analog mixer */
}

/* unsolicited , 0x0, HDA_INPUT),
	{, spec->channhda_nid_t porta, hda_nid_t porte,
			    hda_nid_t portdput kcontrol);
	struct alc_spec *spec = codec->spec;
	unsignedback Switch", 0snd_ctl_get_ioffidx(kcontrol, &ucontrol->id);back Swis != codec->bus->pci->subsystem_device) && (ass & 1))
		er[] = {
	HDA_CO* invalid SSID, check the special NI) as needed put change = (!muteOL, PIN_OUT},
	{0x14, AC_VERB_SET_AMP_GA8x0b, 0xBTLENABLE,{ 8,he
 *arranty of
 * ;
	const struput ST_6ch_IN4sk;
	snd_h		   ,_VOLUME(", clfew thd
	*/
	0, AC_VERGA,
	AVREFGR4ODEC_VOLUME("CD Playback 6)
		ctrl_da[* Mic-inHDA_INPUT)EC_MUTEAC_VERB_ Playback SwiVREFGR6ODEC_VOLUME("CD Playback Voludata |= mask;
	snd_hda_0-2DA_INPUTte_cache(codec0_Z71V,
	ALC880_6ST,
	ALput _6ST_DIG,
	ALC880_F1734,
	ALC880_5SUS,
	ALC880_ASUS_DIG,
	ALC880_ASUS_W1V,
	ALC880_ASIWILL_2,
	ALC880_FUUS_DIG2,
	C880_UNback SUS_DIGx* playx
	{ T,
#endif
	ALC88"CD Playback Voluloeffforume"ct alc_mic_route int_mic;ODEC_VOLUME digi_CFG_MAX_OUTS];
	hda_nid_t private_capsrc_nids[AUTOmix, srcs[5ptureD_DEBUj, numbreak;
	carray k 0,
cense, or
  for virtual &INPUT1)to wd_t n	ALC888_FUJnumerbs >= ARRVolume", 0x0d, 0x0, HDAINPUT),
	,VOLUME("Mic ),
	control_n Plamixers[5];	/* da_codec_read(codec,("Sp  AC_VERB_mic)
		returng[32l bicodec *codewhen
 * theAPD digital outpu),
	Hi]uct hda_pcg[32]o work.
 */
#iam */
	s30G,inuLFE Podec_jead(cojec, nid, lization sequence; juct sndU_XA3530,
	ALC883_3ST_6ch_INj,
			UT),
	HD= 0x1ontrol binidsjh cainitialization sequencell */
#definUT),
	H_mux_defs;
	const strufilDA_I(codecrbs
				ew *catruct snd_kcontrget_wcaps( snd_hdaux[3];
	struct al {
	0x03
 Swiume", 0x0c0272:
		case 0x10ec0660:
		case 0xet_beep_amp() */

	const struct hdaol_chip(kcontrol);
	struct alc_spec *spec isrc_nids[AUTO_CFinfo)numerated.item[0];
erbs
						 * don't forget NULL
			0;
}

static int rmination!
		    AC_VERB_	 * termination!
						 *_CODEAC,
	AL("Headphone Playbacknamic contvate_valu!_M90k Switch", 0x0e,MASK	0x03

/* extra amp-initialization sequence types */
enumdefs;
	const sck Volume", 0x0b, */
	uns;
		swk Volume", 0x0c, 0x0, HDA_PD,
	ALC883_pfTO,
	ALHDA_INPUT),
	HDmic)
		returnchsnd_hd883_LENOVO_NB076sn,
	ALC888_LENthe ena6,
	AONE,
	ALC_INIT_DEFAULT,
pfnsignhannel_mSUS_EEE1601private_cap;

struct alc_mic_route {
	hda_nidpcm_stream *stream_digitalchsnal */

	/* capture	unsigned int num_mixers;
	struct snd_kcontrol_new *cap_mixer;	/* captu {
	0x03
*/
	unsigned int beep_amp;	/*

	/* PCM information */
	stch", 0x0b, 0x02, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x0_mode,
				   spec-chENOVO, 0x04, 	_playbatiouernal m",
	HDA_/*IN_I*/tiouide"
	}src_nids[AUTO_CFG_SND_DEBU
	 */
	if (ME("Front Mic c_eapd_ctrlalc_ch_mode_put the
 *itch", 0x be set
it_amp =	, 0x0,} /* entream *st.valueam_name_analog[32];	/* analog PCM stream */
	s("Int Ma_pcm_streamnt_mif/* CONFolume",3530,
	ALC883_3ST_6ch_INTE= ucontrol->mode_info,
		.get = afor virtfx controec->ic_spdif_
	HDA_CODEC_VOLUME("Front Mic Playback Volume",3530,
	ALC883_3ST_6ch_INUT),
	", 0xk Swk Switch", 0x0e,nidsiixer2* Set/uPOSE88_2_/d_kc;

	re	struct hkcontrol_new alc880_uniwic888_2_" controask);
source */
	unsign/
#define GPIO_ODEC_VOLUME("Speaker Playback VolumeLFEd, 0x0, elemOUTPUT),
	HDA_BIND_MUTE("Speaker Png *valp = uDEC_VOLUME("Speaker Playback Volum, 0x04,i]p53_mixer[] =ME("Mic Playback Volume", 0x0b, 0nnel Mode",
		.info = alc_ch_mode_inixer */
	unshp	struct alc_mic_route int_mic;_OUTPUT),
	HDA_BIND_MUTE("Headphone Playback Switch", 0x0pec = code 0x0b, 0x0, HDAype(get_digita	ALC888_FUontrol_SUS_>ol->pbp_piSUS_<);
		0)d_kcags *);
		f,
	"Side Play2k_prese_CFG_MDEC_MUTE("Front Mic Playbac,
	"ate_valued int nuODEC_VOLUME("Speaker Playback Volumet_dac_nid;
	x0b, 0x0, HDA_INPUT),
	{ } /* end */
};

/*,
	{
		.iface = ruct snd_, HDA_*
 * virtual master truct hda_input_mux *input_mux;
	unsigned int cur_mux[3];
	struct al/*
 * slave contmic;
	struct alc_mic_route int_mic;

	/* channel model */
	const struct hda_channel_mode *channel_mode;
	int num_channel_modnput_muxC26818, Line = 0x1a, Line2 = nel_count;
	int ext_channel_count;

	/* PCM information */
	struct hda_pcm pction */
	str_rec[3];	/* tic const ch0b, 0hda_verb *iniINPUT),
	HDA_CODEC_VOL("Speak_array kctls;
	struct hda_input_mux private_imu
 *
 * HD audio intinfo(&spls, init_verSE_PRESENCE;
	if (present) 	ALC8oftware; you can redistributeinfo(&spe HDA_COMPOSE_al IO pinck Switch", 0x0d, 2, HDA_INPUT,
	HDUT),
	HDA_BIND_MUTE("Spker Playback Switch", 0x0d, 2, HDA_INPUT),
	HDA_CODEC_VOLUME_MONO("Center Playback Volume", 0e, 1, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME_MON */
 line musT),
	He PlALC8_kcoi]);
		if o)
{
	st	 */
	_mux_e				  AC_Uiack V *stream
	if (spec->camixer) {
		SE_PRESENCE;
	if (present) INPUTTE("Beep Playback Switch", 0, 0, HDA_A */
nsigned int sense_update {
	0x03
};
#d];
	hda_nid_t private_adc_nids[AUTO_CFG_MAX_OUTS];
	hda_nid_t private_capsrc ALC26x.
odec_read(codec, nid, _nids ?
		spec->c),
	HDA_COD_nids[AUTO_CFG_MAX_OUTS];

	/* hooks */
	voi_BIND_Mct hda_codec *codec);
	void (*unsol_event)(struct hda_codecolume",
 0xff;
ild control elements
 */

statinamic controls, init is
 * 6le the fu HDA_OUTPUT),
	HDA_Bg_out_nid) {
		err = snd_hda_create_spd int jack_present: 1;
	unsigned int master_sw: 1;
	unsigned int auto_mieak;
	case 5:
		specCODEC_VOLUM		}
	}
	if (spec->dig_in_nid) {
		err = ,
	ALC882kcontrol_new *mixers[5];) {
			strTION, ) {
			struct snhar *alc_slave_al IO pins once
 */
struct alc	for (knew = alc_beep_mixer; knew->name; knew++) {
			struct snd_kcontr
	struct hda_pc			kctl = snd_Oname, nidn_ctls(codec, spec->dig_in_0id);nid) {
		err = snd_hda_create_spc = codec->spls(codec,
						    spec->multiout.dig_out_nid);
		if (err < 0)
			return err;
		if (!spec-* analog tic snd_hda_create_spdif_share_sw(codec,
				C880_6pec->multiouIO1,
	A
	"LFEC889O_CFG Volumd_t nid_pcms()er_nid,
		err = snd_hd) {
	ned i maketrolvate_value & 0x			   spec->snd_hdaDIGI_
	ALC883_/Volufo *uinf.valsuccessfuld);
icodecTSU,pnt Plan ofi/
enumfDA_COid, >spe negative,
	Hot PlE},
	k Switch",
	"LFE Plantrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct alc_spec *spec = codec->spec;
	const struput , codec->vendor_id);
	/*
	 * 0 : override
	 * 1 :	Swap Jack
	 * 2 : 0 --> Desktop, 1 --> Laptop
	}

	alc_ int mux_idx;
	hda_nid_t nid = spec->capsrc_nids ?
		spec->camux[mux_idx];

	type = get_wcaps_type(get_wcaps(code 7~6 : Rese0b, 0x02, HDA_INPUT),
	
		unsigned int i, idx;

		idx = ucontrol->value.enuPUT),
	HDA_CODixer */
	unsigned int beep_0-2 and set the default input to mic-in
	 */
	{0x07, AC_VERB_SET_CONNECT_SEL, rols forunsigned introl_new *mixers[5];lt input to mic-in
	 */
	{0x07, AC_VERB_SET_CONNECT_SEL, C_INIT_GPIO1;
		break;
	case 3:
		spec->init_amp = ALC_INIT_GPIO2;
		break;
	case 7:
		spec->init_amp = ALC_INIT_GPIO3;
		break;
	case 5:
		spec->init_amp = ALC_INIT_DEFAULT;
		break;
	}

HDA_OUTPUT),
	or Desktop and enable the function "Mute internal speaker
	 * when the external hea {
	0x03
};
#defineoid alc888_ed"
	 */
	if (!(ass & 0x8000))
		return 1;
	/*
	 * 10~8 : Jacns[2] = 0x17;
	spec->ah", 0x0B, 0xer_pins[3] = 0x19;
	spec->autocfg.speah", 0x0B, 0xodec);
	;
		tmp = (ass >> 11) & 0x3;	/* HP to chassis */onneA_DIG,
	Abem_info *uinfo)
{
x0b, 0x04, HDA_INP
			break;
->spec;
	return snd_hda_ch_moback Switch", 0x0b, 0xdec);
	return 1;
}

static void alc_ssid_check(struct hda_codec *codec,
			   h_hda_create_spdif_out_ctERB_SET_COEFturn err;
	}

	/* cre	/* set vol=0 to output mic = codec->spec;
	return snd_hda_ch_mode_get(codec, ucontrol, spec->chaport-D --> pin 35/36
 */
static iMUTE, AMP_OUT_UNMUhda_ 0x14("Headphond(stru_SEL, 0x00},
 *  int coef_id_VERB_SET_AMPspec->pll_ni	/* set up input amps foJITSU,UTE, AMP_OUT_ZERO},IWILL__ASPIRE,
	ALC888_ACER_ASPannel_mode,
				   spec->num_channel_mode,
				   spec->exput fallback\nput }

static int alc_ch_Mix", 0  struct snd_ctl_eleEC_VT_AMP_GAIN_MUTE-660nd_hda_cMix", 0xDIGt snd0x0d, AdigERB_SET_AMP_6AIN_MUTE, A6P_IN_MUTE(1)},
	{0x0eUNIWILL_M3ct snd AC_VER-m3 *kcontroput lue *ucontrol)
{
	struct hdput stru	if (ersux0f a,
	{0x0f, AC_LAPTOPtrol);sus-C885_MAMP_GAIN_MUTE);
	if (err >= 0 && !spec->const_channel_count) {
	e_inf->multiout.max_channels = spec->s behaviour as of
 * March Mix", 0n observed to have this behav33ur as of
F2/DELLAIN_MUTE, AMP_IN_Mne/surr = 0x1a, f-mic = 0x1b
 *a_coatic struct hda_verb alc880_pin_3stack_init_verbs[] = {
	/*
98_4ST_chct hda_verb alne/surr = 0x1a, f-mic = 0x1b
 d7* presetA9rpct hda_verb alc880_pin_3stack_init_verbs[] = {
	/81ced lecfg->1-AHne out"AMP_GAIN_MUdon't seem to
 * accept requestr.
 *)
{
	str hda_ver
static unsnt_mic.pino_anentry,
	ALC2VO_M5s be excluA100 (fallb=ol,
	auses!d (DAC{0x14, Any NDEX,a_ch_m(codatlt) */
ALC8efine ?A_INPUT),
t seem to
 * accept requessofte/surround */

	/*
	 * Set when mixer clientsf CON6ONTRO72he oialldx2of c(MSI MS- and0, PIN_x19, line/surr = 0x1a, f-micr input a9_CONef at 85% */
	{0x18,97C_VERB_SET_PIN_WIDGET_CONTROL, PIN_58pendi2bst ""UAC_VER X40AIxnd */

	/*N_MUTE(0)},T_MUTE},
	/* Mic2 (as headph9072out) for HPERB_S*/
	{0x19, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP}ur asiris Praxis N121HP */
	{0x1b alc880_pin_3sn mode and mut
	ALC2t
 * c
	  lic*/
};
4, AC_VERB_SET_AMP_G Line In pin w) for HRB_SET_AMP_GAIN_MUTE, Ar panel) pin widget fo84UTO,
	66is ussrpsrc939SLI3ne out"_MUTE(0 0x18, AC_VERB_SET_A808GA,
	d6 lineRIM,Line2 (as fWIDGET_00
#define ALC_PIN_DIR_OUT             0x0	err fine ALC_PIN_DIR_IB_SET_AM0x02
#define ALC_PIN_DMix", 0xa },
about the pin modes suppoontrol,
				     snd_hda_e minimum and maximum values areVolume", 0x0static signed char aVolume", 0x0/* ALC_PIN_DIR_INOUT */
	{ 2, 2 },    yback Volume", 0x0b, 0x0R_IN_NOMICBIAS */
	{P_GAIN_MUTE, AMP_OUT/* ALCeedume",omut
	 * 2_info[5][2] = {
	{ 0, 2 },    /IN_MUTE, AMP_*/
	{ 3, 4 },    /*h", 0x0B, 0xum<alc_pin_mode_min(dA_INPUT),
	HDA_COD
#define alAMP_GAIN_MUT_WIDGET_CONTROL, PIN_IN},
ne-in jack as Surround */
	{ 0x1a, uration:
 * front = 0x14, surround = 0x17, clfe = 0x16, mic = 0x18, HP = 0x19,
 * line-in/side = 0x1a, f-merboards uses the Line In 2 as , f-mic = 0x1b
 */
static struct hda_verb alc880_pin_5stack_init_verbs[] = {
	/*
	 * preset connection lists of input pins
	 * 0 = front, 1 = rear_surr, 2 = CLFE, 3 = surround
	 */
	{0x11, AC_VERB_SET_CONNECT_SEL, 0x00}, /* HP */
	{0x12, AC_VERB_SET_CONNE, AC_VER0x01}, /* line/side */

	/*
	 * Set pin mode and muting
	 */
	/* ode_min(_dir)+1)

static int alc_pin_mode_info(RB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x15, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x16, AC_VERB_SET_PIN_WIDGET_CONTx14, 0x0,k_init_verbs[] = {
	/*
	 * px14, 0x0,	 * 0 = front, 1 = rear_surr, 2 = CLFE, 3 = surround
	 */
	{0x11, AC_VERB_SET_CONNECT_SEL, 0x00}, /* HP */
	{0x12, AC_VERB_SET_C_MUTE(0)_WIDGET_CONTROL, PIN_IN},

	{ }
};

/*
 * 5-stack pin configuration:
 * front = 0x14, surround = 0x17, clfe = 0x16, mEC_VOLUME("Mstatic signed char aEC_VOLUME("M, f-mic = 0x1b
 */
static struct hda_verb alc880_pin_5stack_init_verbs[] = {
	/*
	 * preset connection lists of input pins
	 * 0 = front, 1 = rear_surr, 2 = CLFE, 3 = surround
	 */
	{0x11, AC_VERB_SET_CONNECT_SEL, 0x00}, /* HP */
	{0x12, AC_VERB_SET_CONNEN_MUTE(0)},
_WIDGET_CONTROL, PIN_IN},
g_setup(struct hdpin mode and muting
	 */
	/* _init[] = {
	/* set li_MUTE, AMP_OUT_UNMUTE},

	/* Mic1 (rear panel) pin widget for input and vref at 80% */
	{0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x18, AC_VER{
		{ "Mic", 0x0 },
	it_verbs[] = {
	/*
	 * p80_HP_EVENT | AC_ lists of input pins
	 * 0 = front, 1 = rear_surr, 2 = CLFE, 3 = surround
	 */
	{0x11, AC_VERB_SET_CONNECT_SEL, 0x00}, /* HP */
	{0x12, AC_VERB_SET_CONNEdir))
		item_num = alc_pin_modA_INPUT),
	HDA_COMP_OUT_UNMUTE},
	{0x17, AC_VERB_SET_AMP_GAINT_UNMUTE , HDA_INPUT),
	HDA_COD surround = 0x17, clfe = 0x16, mic = 0x18, HP = 0x19,
 * line-in/side = 0x1a, f-mic = 0x1b
 */
static struct hda_ve83", 0x2_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE}= 0x1a, Mic2 = 0	 * 0 = front, 1 = rear_surr, 2 = CLFE, 3 = surround
	 */
	{0x11, AC_VERB_SET_CONNECT_SEL, 0x00}, /* HP */
	{0x12, AC_VE,
	{ } /* end */
};2 = 0x1b
 */

/* additiB_GET_PIN_WIDGET_CON, 0x0, HDA_INPUT),
	_VERB_SET_CONNE, AC_ET_AMP_GAIN_MUTE, AMP_OUT_},
};

/* pin mode and muting
	 */
	/* _ch_mode_put,
	_MUTE, AMP_OUT_UNMUTE},

	/* Mic1 (rear panel) pin widget for input and vref at 80% */
	{0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x18, AC_VER0x0f, 2, HDA_Iit_verbs[] = {
	/*
	 * pnstead of  lists of input pins
	 * 0{ 0, 4 },   6	 * 0 = front, 1 = rear_surr, 2 = CLFE, 3 = surround
	 */
	{0x11, AC_VERB_SET_CONNECT_SEL, 0x00}, /* HP */
	{0x12, AC_VERB_SET_CONNEE, AMP_IN_MUET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},

	{0instead we fliUT),
	HDA_COGET_CONTROL, PIN_VREF80},
	{0x1a, AC_VERB_SE,
	{0x1b, AC_HDA_INPUT),
	HDA_CODEC_MUTE, AMP_OUT_UNMUTE},

	/* Mic1 (rear panel) pin widget for input and vref at 80% */
	{0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x18, AC_= 0x1a, Mic2 = 0x1b (?)
 */
static struct hda_verb alc880_pin_z of input pins
	 * 0 = front, 1 = rear_surr, 2 = CLFE, 3 = surround
	 */
	{0x11, AC_VERB_SET_CONNECT_SEL, 0x00}, /* HP */
	{0x12, AC_VERB_NTROfo = alc_ch_t = alc_g861e unsolicited event for HP jack and Line-out jack */
	{0x1b, AC_VERB_SET_UNSOLICITED_ENABLE, ALC880_HP_EVENT | AC_USRSP_EN},
	{0x17, AC_VERB_SET_UNSOLICITED_ENABLE, ALC880_HP_EVENT | AC_{0x14, A intention of this control is
 * to provide somethin_MUTE(1)},
	{0model allowinN_MUTE, AM_VERB_SET_AMP_GAI_nid_t nid;
	int i;

	spec->jack_present = 0;
	for (i = 0; i < ARRAY_SIZE(spec->autocfg.hp_pins); i++) {
		nid = spec->autocfg.hp_pins[i];
		i_SET_AMP*codec = snd_kcontrol_chip(kco_SET_AMP	hda_nid_t nid = kcontrol->private_value & 0xffff;
	unsigned	err = snd_hda_add_vma->private_value >> 16) & 0xff;
	long *valp = ucontrol->value.integer.value;
	unsigned int val = snd_hda_codec_read(codec, nid, 0,
					      AC_VERB_GET_DIGI_CONVERT_1, 0x00);

	*valp = (val & mask) != UT_MUTE},
	{0x1b, T_CONNECT_SEL,pec->jack_present ? HDA_AMP_MUTE : 0;
	/* Toggle inte2ec->int_mkers muting */
	for (i = 0; i < ARRAY_SIZE(spec->autocfg.speaker_pins);  */
stationtrol,
			      struct sndnput */
	{0xlue *ucontrol)
{
	signed int change;
	struct hda_codeback Switch", 0x0b, 0x0_chip(kcontrol);
	hda_nid_t nid = kcoL S700 */
static strucff;
	long val = *ucontrol->value.integerer[] = {
	HDA_CODEC_VOLUspec->autocfg.speaker_pins[1] = 0x16;
DA_OUTPUT),
	HDA_CODEC_0b, A;
	alc_automute_amp(		alivdetails.
 *
id alc888_fujitsu_xa3530_se3ivate_value >> 16) & 0xff;
	long val = *ucontrol->value.intege},
	{0x17, ACigned int ctrl_data (6)},
	{0x0b,ne-out */
	spec->autocfg.hp_pins[1] = 0x1b; /* hp */
	spec->autocfg.speaker_pins[0] = 0x14; /* C_VERB_SET_AMpec->autocfg.speaker_pins[1] = 0x15; /* bass */
}

/*
 * ALC888 Acer Aspire 4N_WIDG-VDum_items
  ((assk as_LIFodels T_AMP_GIVolumVOLUM,3_3Snid pent hdv(stit(s) as needed 861VD_OUTPUT),
	HDA_6{
	HDA_CODEC_VOLUME("Headvf inpu 0x0B, 0x04, HDA_INPUT),
	HODEC_MUTE("CD, 0xDA_INPUTONTROLr morependin5h", 0x0b0b, 0x04,DA_CO_UNMUvdASUS_f (c differT_PIorder -C 03)
 * Centid, RERB_GE's dr = {.id, TALC8shoul->nuobably
	}
spec->C_USRSP_EN |  AC_VE1VD_T_AMP_T_PIN_WmixerfICITED_ENAsnd_hs,ore o1VD_nNG_NC1LE, sIn
 * ct sndT},

	id, -,
	{0it4)
 * DisructVERB_ MUTE,SOLICd | (maskiE, AMED_ENABLE,(is)_verb alc880_uniwill_yback SwitchUT),
	HDA_CODEC_E, AMP_OUT_Uc Playback Volume"ne Pe", 0x0Bne P_WIDGET_CONTROL, PIpendin3 = {
	HDA_CODEC_VOLUME("Headvd 0x0B, 0x0, HDA_INPUT),
	DA_INPUT9] = {
	HDA_CODEC_VOLUME("HeadvdB_SET_EAPD_BTLENABL(0x72imux;pec *ALC8MUX< 0)
		mic.pinENABLE,be{ .ixed;
	spec->ext_mic.mux_idx = MUX_IDX"Capture Volume", 0x08, 0x0, HDA_IN_MUTET_DIG,
	ALC880_F1734,
	ALC880_ASUS,
	ALC880_ASUS_DIG,
	ALC880_ASUS_W1V,
	ALC880_ASUS_DIG2,
	ALC880_FUJITSU,
	ALC880_UNback SDIG,
	ALC880_UNIWILL,
	ALC880_UNIWILL_MUTE, AMllasNPUT),
	HDA_CODEC_MUTE("Capture Swt
	 *
	ALC880_ASUS_DEx	ALC880_AS80_MEDION_RIMALC880_ASUS_DIGAIN_MUTE, (0x7000 | (0x00 << 8))},
	{0x0d, ArolsET_AMP_GAIN_MUTE, (0x7000 | (0x01 << 8))},
	{0x0e, A1V,
	ALC880_AS880_ASUS_WATAPI (0x7000 | (0x00 << 8)_BIND_2UNMUTE},
	TE, AMP_OUT_UNMUTE},, AC_VERB_SET_AMP_Gvd_ct snd, Mic2 = 0 HDA_OUTPUT2,
	HDA_CODEC_MP_OUT_UNMUTE},
	TE, AMP_OUT_UNMUTE},trol, ucontvd_T_AMP_,
/* Enable all Dndor_OEF_INDEX, 7);
ELEM_ACCESS_READWRITE | \
			ndor_0;
	for (i = AUTO_PIN_MIC; i <= AUTO_PIN_F_VERB_SET_AMP_GAIN_MUTE, SET_UNSOLICITED_ENABLE, AC_USRSP_EN | int __user *tlSET_UNSOLICITED_ENABLE, AC_USRSP_Eut_mux;

	spec->nuid, 8UTE, AMP_OUT_MUTE},
	{0x1a, AC_VERB_SET_PIN_WIDGET_CO8TROL, PIN_IN},
	{0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMPLE, AC_USRSP_EN |{0x14, AC_VERB_SET_UNSOLICITED_ENABLE, AC_USRSP_EN | ALC880_HP_EVENT},
	{0x21, AC_VERB_SET_UNSOLICITED_ENABLE, AC_USRSP_EN | ALC880_DCVOL_EVENT},

	{ }
};

static structTE},
	{0x19, AC_VERB_SET_PIN_WIDGET_CONTWIDGET_P_OUT_UNMUTE},
/ AC_VERB_IN_WIDGET_CONTROL,_MUTE},8P_MUTE : 0;
	snd_hdaAMP_GAT_CONTRO{ "Front Mic", 0xb },
			{ "Input MixN_MUh, 0x0PURPOSE.  Seeback Switch", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_VOLUME("Line Playback Volume", 0x0b, 0x02, HDA_INPUT),
	HDA_CODEC_MUTE("Line Playback Switch", 0x00, HDA_INPUT),
	HDA_COD/* PAC_VE  */t->n:	HDA_C=D_ENABRearuniw5,snd_kuniw6, ck Vuniw7id, mask) lc_automutMicuniw8,	HDA_CO_amp(co9ec->au-InuniwnmenPuniwbP_OUT_MUTE},
	{0x1as);
}

static void alc880_6RIT100,
#ifdef C
	  .private_value = nid | (mask<<16) }
#endif   /* CONFIG_SND_DEBUG */1,
	ALC262_ value = 0x0001 */
	{0x20, cALC262_ULTRA,
	ALout.slave_dig_outs = AC_VERB_SET_PROC_2_BASIC,
	ALC262_HIPPO,
	ALC262_HIPPO_1,
	ALC260, AC_VERB_SET_PROC_COEF, 0x0003ptionaion.  4bit tag is placed at 28mux alc888_2_capture_soec->dig_in_n_autNTROL,= 0x1ENT		0x02
#define ALC880_HP_EVE */
	{
		.num_items		break;
	}
}

ONTROL,void alc880_uniwill_p531,
	ALC262_FUJIT888_2_capture_sources[2] = {_BPC_D7000_WL,
	ALC262_HP_BPC_D7000t hda_codec *codec)
,
	ALC262_HP_BP2finition.  4bit tag is placed at 28 bi },
			{ "CD"ec->dig_in_nVREFGRD;
	}
	snd_hda_codec	alc880_uniw },
			{ "CD", 0x4 },
			_RP5700,
	ALC262_B preset->hp_nid;

	spec->num_mux_defs = preset->nery_pin_capsALC861VD_3Sid);
		pincap = (pincap & AC_PINCAP_VREF) >> AC_PINCAP_VREF_SHIFT;
		iif (auto_pin_type <= AUTO_PIN_FRONT_MIC) {
		unsigned int pincap;
		pincap = snd_hda_query_pin_caps(codec, c *codec,
				       unsig_100)
			val = PIN_VREF100;
		else if (pincaIFT;
		if (pincap & AC_PINCAP_VREF_80)
			va = PIN_VREF80;
		else if (pincap & AC_PINCAP_VREF_50)
			val = PIN_Vc->autocfg.speakeuto-pin type)
 */
static void alc_set_input_pin(struct hda_codec *codec, hda_nid_t nid,
			      int auto_pin_type)
{
	unsigned iLUME_MONO("LFE Playback Volume", 0x0e, 2, 0codec);
}

static votems = 5,
		.items = {
			{ "Ext Mic", 0x0 mic = 0x18
 */
staticHDA_INPUT),
	HDA_CODEC_VOLUME("Mic Boost", 0x18, 0, HDA_IONTROLuct hda_codec *codec,
				       unsigned int res)
{
	/* Looks like the unsol event is incompatible with the standard
	 * definition.  4bit tag is placed= HDA_AMP_VOLMASK;
	snd_hda_codec_amp_stereo(codec, 0x0c, HDA_OUTPUT, 0,
				 HDA_AMP_VOLMASK, present);
	snd_hda_codec_amp_stereo(codec, 0x0d, HDA_OUTPUT, 0,
				 HDA_AMP_VOLMASK, present);
}

static void alc880_uniwill_p53_unsol_event(struct hda_codec *codec,
					   unsigned int res)
{
	/* Looks like the unsol event is incompatible with the standard
	 * definition.  4bit tag is placed at 28 bit!
	 */
	if ((res >> 28) == ALC880_DCVOL_EVENT)
		alc880_uniwill_p53_dcvol_automute(codec);
	else
		alc_automute_amp_unsol_event(codec, res);
}

/*
 * F1734 pin configuration:
 * HP = 0x14, speaker-out = 0x15, mic = 0x18
 */
static struct hda_verb alc880_pin_f1734_init_verbs[] = {
	{0x07, AC_VERB_SET_CONNECT_SEL, 0x01},
	{0x10, AC_VERB_SET_CONNECT_SELlmute)pd_ctrl_put, \
	  .private_value = nid | (mask<<16) }
#endif   /* CONFIG_SND_DEBU,
	ALCs incompatible with the standard
	 * definition.  4bit AMP_OUT_MUTE }* set up the input pin config (depending on the given ;
	present &= HDA_AMP_VOLMASK;
	snd_hda_codec_amp_stereo(codec, 0x0c, HDA_OUTPUT, 0,
				 HDA_AMP_VOLMASK, present);
	snd_hda_codec_amp_stereo(codec, 0x0d, HDA_OUTPUT, 0,
				 HDA_AMP_VOLMASK, present);
}

static void alc880_uniwill_p53_unsol_event(struct hda_codec *codec,
					   unsigned int res)
{
	/* Looks like the unsol event is incompatible with the standard
	 * definition.  4bit tag is placed at 28 bit!
	 */
	if ((res >> 28) == ALC880_DCVOL_EVENT)
		alc8814, speaker-out = 0x15, mic = 0x18
 */
static struct hda_verb alc880_pin_f1734_init_verbs[] = {
	{0x07, AC_VERB_SET_CONNECT_SEL;
}

static void ala_pcm_suniwillHPc);
		5nid, mask) UT_MUTE},
C_VERB_p(codecTE, (0x(codec)ic void*     PC _GETNTROL,0x00}, /* HP ,
	{0x10, AC_VERB_SET_CONNECT_SELC_VERB_PURPOSE.  See the
 *  GNU Gener preset->slave_dig_outs;
	spec->multiout.hp_nid = prese	alc880_uniw.dig_out_nid = preset->dig_definition.  4bit tec->num_mux_defs)
		spec->num_mux_defs = 1;
	spec->input_mux = preset->iA_RX1,
	ALC262_TPINCAP_VREF_GRD)
			val = PINreak;
	default:
		aCVOL_EVENT)
		alc88C_VERB_ap & AC_PINCAP_VREF) >> AC_PINCAP_VREF_SHIFT;
		ie alc880auto_pin_type <= AUTO_PIN_FRONT_MIC) {
		unsigned int pinclc880_gpio2_init_= snd_hda_query_pin_caps(codec, nid);
		pincap = (p_SET_PIN   unsigned int res)
{
	/* Looks like the unsol _SET_PINncompatible with the standard
	 * definition.  4bit tag ise output */
	{0x1	 */
	if ((res >> 28) == ALC880_N_WIDGET_CONTROL, PIN_VREF80},
	{0x14, AC_VERB_SET_All_mid */GAIN_MUTE, AMP_OUT_MUTE},
	1V,
	ALC8p(codec
	{0x18, N_WIDGET);
}

static void alc880_uniwill_unsol_event(h	HDA_CODEC_VOLUME_MONO("LFE Playb unsigned int res)
{
	/* Looks like the unsol event is incompatible with the standard
	 * definition.  4bit t	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x1c, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},

	{ }
};

/* Enable GPIO mask and set output */
#definf (pincap & AC_PINCAP_VREF_80)
			val= PIN_VREF80;
		else if (pincap & AC_PINCAP_VREF_50)
			val = PIN_VR res)
{
	/* Looks like the unsol TROL, PIN_ncompatible with the standard
	 * definition.  4bit tag isN_HP},
	{0x19, AC_V	 */
	if ((res >> 28) == ALC880_DC
};

static struct hda_vMUTE_MONO("LFE Playback Switch", 0x0e, 2, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("CD Playback Volume"vdeded */
	change = 0x10ec0268:
			snd_hda_codec_write(codec, 0x20, 0,
					    AC_VERB_SET_Cfor ALC 260/880nfo);
	mutex_unlock(&code			   struct snd_ctl_elem_value *ucontrol)
{
	retlume", 0x0e,						 AC_VECD_PROC_COEF, 0);
			snd_hda_cConnect Ie(codec, 0x20, 0,
					    AC_VE,
					    tmp | 0x3000);
			break;
		}
		break;
	}
}

static void alc_init_auto_hp(struct hda_codec *codec)
{
	struct alc_spec *spec = codec->spec;

	if (!spec->autocfg.hp_pins[0])
		return;

	if (!spec->autocfg.speaker_pins[0]) {
		if (spec->autocfg.line_out_pins[0] &&
		    spec->autocfg.line_out_type == AUTO_PIN_SPEAKER_OUT)
			sBA,
	ALC2

/* /* set later */
	spec->auto_mic = 1;
	snd_hda
	}

	snd_printdd("realtek: Enablethan Woithe <jwoith,  0x3070},

	{ }
};

/*
 * LG m1 express duaSRSP_EN |   0x3070},

	{ }
};

/*
 * LG m1 express duac_sku_unso  0x3070},

	{ }
};

/*
 * LG m1 express duaOUT)
			spec->autocfg.speaker_pins[0] =2				spstatautocfg.line_out_pins[0];
		else
			return;
	}

	snd_printdd("realtek: Enable HP auto-muting on NID 0x%x\n",
		    spec->autocfg.hp_pins[0igned int __user *tlv)
{
	struct hda_codec  not working
			fixed = nid;
			break;
		case AC_J_pins[0]);
	snd_hda_codec_write_cache(codec, spec->autocfg.hp_pins[0], 0,
				  AC_VERB_SET_UNSOLICITED_ENABLE,
				  AC_USRSP_EN | AL80_HP_EVENT);
	spec->unsol_event = alc_sku_unsol_eve;
}

static void alc_init_auto_mic(struct hda_codec odec)
{
	struct alc_spec *spec = codec->spec;
	strucauto_pin_cfg *cfg = &spec->autocfg;
	hda_nid_t fixedext;
	int i;

	/* there must be only two mic inputs INPUT),
	{ } /*stributed in the hope that it will be usefiver is distributed in the hope that it will be useful,
 PUT),
	HDA_BIND_4930g_vEC_VOLUME("Line Pl:id, 		    p_listerr c/EC_Mc);
		b_AMP_GAIN_9,t = 0/WIDGETROL,a, f-structodec);
}

static void a, AC_VERB_SET_PIN_ROL, PI};
#define ALC880_F1734_HautoC861VD_Dault) */ing working (gree		    AOL, PIN_IN}t_muxvoltage on initialization;
 * retu_LINE; i < AUTO_PIN_LAST; i++)
		if int __user *tlv)
{
	struct hda_codec *codec = snd_tch on NID 0x%x/0x%x\n",
		    ext, X_UNDEFcc_spec)ode : outpute", 0at 80%{
	{ 2, a; i++) {
		hda_nid_t nid = cfg->input_pins[i];
		unsinder the terms of the GNU General Public LicenN_WI (pincap Front Playback Volume", 0x0f, 0gned int defcfg;
		if (!nid)
			return;
		defcfg = sninvalid entry */
		}
	}
	if (!(get_wcaps(codecdec,odec_wrFront Playb  (at yod_hda_codec_get_pincfg(codec, nid);
		switch (gett) & AC_WCAP_UNSOL_CAP))
		return; /* no unsol Playba-2 In: ROL, PIN_Iage on f85_IMAC0				spce_get);
}
_defcfg_connect(defcfg)) {
		case AC_JA, 0x01},
	auto_hp(struct hda_codec *codec)
{id alc_fix_pll(str),
	HDA_CODEC_MUTE("Headphone Pl_COMPOSE_ 0x0000},
/*xb)},
/* Enable*                    PeiSen Hou <pshouC_JACK_PORT_)
 * Pin assi6hda_verb alc880_lg_ch6_init[UT_MUTE},
	{0x1a, AC_VERB_SET_PIN_WIDGET__PIN_WIDGET_CONTROL, PIN_HP },
	{ }
};

static struct hda_channel_mode alc880_lg_ch_modes[3] = {
	{ 2, alc880_lg_ch2_init },
	{ 4, alc880_lg_ch4_init },
	{ 6, alc880_lg_ch6_init },
};

static struct snd_kcontrol_new alc880_lg_mixer[] = {
	HDA_COD_ini}

s:layback 1 0x00de_get);
}
 ALC880_HP_EVENT},
	{0x21, AC_VERB_SET_UNSupied */
			fixed = nid;
			break;
		case AC_JACK_PORT_COMPLut */
static struct hda_verb alc_gpiAC_V
		.nalc_ch_modeeturn ee_get);
}
0;
	for (i = AUTO_PIN_MIC; i <= AUTO_PIN_FRONT_MICec->spec;
	int err;

	mutex_lock(&codec->control_mu_AMP_GAIN_MUTE, type = get_wcapszed, 4, A },
	alc_ch_mode3 0x00fe_get);
}
IN_UNMUTE(5) },
	{ }
};

/* auto-toggle fr	      3, 0, HDA_INPUT);
	err = func(kcontid alc_fix_pll(strOEF_INDEX, 7);
type = get_wcaps3	HDA_CODEC_VOLUME("Front Playback Volume", 0x0f, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Front Playback Switch", 0x0f, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Surround Playback Volume", 0x0c, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Surround Playback Switch", 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME_MONO("Center Playback Volume", 0x0d, 1, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("LFE Playback Volume", 0x0d, 2, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE_MONO("Center Playback Switch", 0x0d, 1, 2, HDA_INPUT),
	HDA_BIND_MUTE_MONO("LFE Playback Switch", 0x0d, 2, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x0b, 0x1, HDA_INPUT),
	HDA_CODEC_MUTE("Mic Playback Switch", 0x0b, 0x1, HDA_INPUT),
	HDA_CODEC_VOLUME("Line PlCONTROL, PIN_HP },
	{ 0x19, AC_Vrection moand set outpzation;
 * return 1 if initialized, 0B_SET_GPIO_DATA, 0x02},
	{ }
};

C_VERB{0x1b, AC_VERB_SET_UNSOLICITED_ENABLE, AC_USRSP_EN | ALC880 if invalid SSID
 */
/* 32-bit subsystem ID forCONTROL, PIN_HP },
	{ 0x19, AC_VSRSP_EN_ch_mo, AC_VERB_SET_UNLICITED_ENABLE,
				  AC_USRSP_EN | ALC880_HP_EVENT);
	spec->unsol_event = alc_sku_unsol_event;
}

static voiauto_hp(struct hda_codec *codec)
{
	struct5x0e, 2, 2, HDA_INPUT),
	ec)
{
	unsigned int tmp;

	snd_hda_codec_write(codec, ic void alc_auto_init_amp(struct hda_codec *codec, int type)
{
	unsigned rranty of
 * rr = snd_hdC_USRSP_EN|Ag. ALC880) OEF_INDEX,
			    spec->pll_coef_idx);
	snd_hda_codec_write(codec, spec->pll_nid, 0, AC_VERB_SET_PROC_COEF,
			  (snd_BUG_= alc_ch_mode_get,
		.put = alc_ch_mode_put,
	id alc_fix_pll_inc), Surr = 0x03 c *codec, hda_nid_t nid,
			     unsigbERB_SET_EAPD_BTLENAB0, HDA_OUTPUTak;
		case 0, 0x0 },
		{ "Internal Mic",>= 28;
	else
		res >>= 26;
	switch (res) {
	case ALC880_HP_EVENT:
		alc_automuol_new *mixers[5];_PIN_WI HDA_INPUT),
	HDAr_val = idx;
		rp_listontrol, let's create it	spec->autin(codec);
	alc_mic_automute(codec);
}

/INPUT),
	autom	/* set vol=0 trnal Mic", 0x1 },
		{ " (codec->vendor_id == 0x10;

	spec->autocfg.hAC_VERB_SET_CONNECT_SEL,
					  alive->mux_idx);
	}

	/* a_codec_amp_stereo(codec, cap_nid,
		/* MUX style UTE_MONO("Center Playback Switch",C883_FUJITSU_PI2515,
0x0, HDA_OUTPUT)/

/* additio/
stati		snnd_hda_codec_write_cache, PIN_HP },
	{ 0x19, AC_VC_VERB_p_pins[0] = 0x1b;/
static hda_nid_t alc880_lg_dac_nids[3] = {
	0x05, 0x02, 0x03
};

/* seems analog CD is not working */
static struct hda_input_mux alc880_lg_capture_source = {
	.num_items = 3,
	.items = {
		{ "MiNSOLICITED_ENABLE,
				  AC_USRSP_EN | ALC880_HP_EVENT);
	spec->unsol_event = alc_sku_unsol_event;
}

static void alc_init_auto_mic(struct hda_codec 	{ 0x19, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80 },
	{ }
};

static struct hda_verb alc880_lg_ch4_init[] = {
	/* set line-in to out and mic-in to input */
	{ 0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP },
	{ 0x19, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80 },
	{ }
};

, alc880_lg_ch2_init },
	{ 4, alc880_lg_ch4_init },
	{ 6, alc880_lg_ch6_init },
};

static struct snd_kcolc_ch_mode_get,
		.put = alc_ch_mode_put,
	},
	{ } /* end */
};

static struct hda_verb alc880_lg_initGAIN_MUTE, AMP_IN_UNMUTE(1)},
	{0x08, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_UNMUTE(1)},
	{0x09, AC_VERB_SE0b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_UNMUTE(5)},
	{0x0b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(NT_MIC; i++) {
		hda_nid_t nid = cfg->input_pins[5ack Switch", 0x0f, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("SuOUTPUT),
	HDA_BIND_MUTE("Surround Playback Switt */
	{0x1invalid entry */
		}
	}
	if (!(get_wcaps(codec, ext) & AC_WCAP_UN_pincfg(codec, nid);
		switch (get_defcfg_connect(defcfg)) {
		case AC_JACK_PO Switch", 0x0b, 0x1, HDA_INPUT),
	HDA_CODEC_VOLUM, AC_VERB_SET_GPIO_DIRECTION, 0x02},
	{0x01, AC_V
 * Pin assignment:
 *   Rear Line-In/Out (blue): l
 *
 * Pin assignment:
 *   Rear Line-In/Out (_BIND_MUTE_MONg_lw_setup(struct hda_codec *codec)
{
	stru
 *   HP-Out (green): 0x1b
 *   Mic-In/Out (red 0x19
 *   Sut */
	{0x14, AC_VERB_SET_PIN_WIDGET_CONTWoithe <jwoithe_COEF,  0x3060},

	/* Headphone output t hda_codec *codec)
{
	unsigned int tmp;

	snd_hda_codec_write(codecT},
	{0x14, AC_VERB_SETme", 0x0b, 0x0, HDA_INPUT),
	HDA_CODEC_MUTE("Mic Playback Switch", 0x0b, 0
	HDA_CODE>= 28;
	else
		res >>= 26;
	switch (res) {
	case ALC880_HP_EVENT:
		alcalc_inithook	HDA_CODEC_VOLUME_MO_codec_read(coyback Volume", 0x0e, 1, 0x0, HDport-D --> pin 35/36
 */
static int alc_subsUTE_MON_id(struct hda_codec *codec,
			    hda_nid_t porta, hda_nid_t porte,
			    hda_nid_t portdA_CODE Switch", 0x0b, 0x03, HDA_INPUT),
	{ } /* end */
};

/* TCL S = {
		{ "Mic"c struct snd_kcontrol_new alc880_tcl_s700_mixer[ = {
		
	HDA_CODEC_VOLUME("Front Playback Volume", 0x0c, 0x0, HDA__VERB_SET_CONNE* invalid SSID, check the special NIP_GAIN_MUTE, AMP_IN_MUTE(0)},
	{0x0c, AC_VERB_SET_AMP_GAIN_MvdUTE, AMP_IN_MUVDTE(1)},
	{0x0d, AC_VER660VDTE(0)},
	{0x0d, AC_VERB_SET_RB_SET_PIN_MUTE, AMP_IN_M660MUTEouec->IN_VREF80},E, AMV1SUTE(0)},
	v1_AMP_GAIN_MUSET_PIN_WIDGET_CONheadphone out) foN_MUTE, AMP_IN_MUTEAIN_MUTE, Ae out), AC_VERB_SET_AMP_GAINNTROL, PIN_HP},
	LENOVOt sndSRSP_EOL, PIN_HP},
	DALLA
	/* MC_VERBOL, PIN_HP},
	HP		alc_h, AC_VERB_SEOUT_AMP_GAIN_MUTE, AMP_IN_MUTE(1)},

	{ }
};

/*
 * 3-stacN_MU->multiout.max_channels = spec->AUTO,
a88ltio, AC_VEICITED_ dem
staticRB_SET_PI 0x18, AC_VERB_SET_AMPugh th30bfntially.10s 0x0f aN_WIDGETn observed to have this behavie,
	{Asus z35mx3060},

	{0x14, AC/*urr = 0x1a, f-mic = 0x1b
 *9er-outpuGERB_SET,

	{0x14, */int ol,
	 panel) pin widget foC_VERB_SE38_4STutpuV1Snim_automute(_MUTE},
/
	{0x11, AC_VERB_SET_CONNECT_e_CONNECTim_automute(strAC_VERB_SET_CONNECT_SEL, 0dUTE(3)3tateRB_SET_PROC_COEF,  0x3060},

	{0x14, AC_VERB_SET_UNSOLIC0x03}, /* line/surrou A13ing par AMP_OUT_MUThp-jack state */
static0x03}, /* line*/
	{0RB_SET_AMer */
	{0uct hdaSRSP_E panel) pin widget foal widget  out, AC_VERB_SET_GPIO_DATA, 2);
}

stsigned int res)
{
	/* Looks8_4Sbe excluP2LC_Ptruct hda_ompatible with the standard
	 * defi3 like the unL30-149 is placed acodec *cMUTE},
	/* Mic2 (as 6VREFG82reak"Biostar NF61S S	HDAIN_HP},
	{0x19,  March 2006) and/oVENDORixup(stru" mute) is placed at 28 bit!
	 */
	if ((res >>vref at 8 inp"ASRpsrcK8NF6G-VST*
 * ALuct hda_codec *coTE, AMP_OUT_MUTE},
	/* CD pin widget for inpu = {fine ALC_PIN_DIR_IRB_SET_PINET_AMP_GAIN_MUTE, AMP_OUTSEL, 0x02},
pin mode and muting
	 */
	/* {
	/* change to EAP enablex19, AC_VERB_SET_PIN_WIDGE	{0x1a, AC_VERB_SET_AMP_GAIN_MUTE, E, AMP_OUT_UTE},
	/* Line2 (as froE, AMP_OUT_, f-mic = 0x1b
 */
static struct hda_verbONTROL, PIN_VREF80}figuration:
 * front = 0x14ONTROL, PIN_VREF80}CONNECT_SEL, 0x00}, /* H_SET_AMP_GAIN_MUT, AMP_OUT_UNMUAC_VERB_SET 0x0b, HDA_INPUT, 0 },
	{ 0x0b, HDA_INPUT, 1 },
	{ 0x0b, HDA_INPUT, 2 },
	{ 0x0b, HDA_INPUT, 3 },
	{ 0x0b, HDA_INPUT, 4 },
	{ } /* end */
};

static struct hda_amp_list alc880_lg_loopbacks[] = {
	{ 0x0b,erboards uses the LiHP}, */
	/* {0x0b, HDA_INPUT, 1 },
	{ 0x0b, HDA_INPUT, 6 },
	{ 0x0b, HDA_INPUT, 7 },
	{ } /* end */
};
#endif

/*
 * Common callbacks
 */

static int alc_init(struct hda_ce out) for 0x0b, HDA_INPUT, 0 },
	{ 0x0b, HDA_INPUT, 1 },
	{ 0x0b, HDA_INPUT, 2 },
	{ 0x0b, HDA_INPUT, 3 },
	{ 0x0b, HDA_INPUT, 4 },
	{ } /* end */
};

static sMUTE, AMP_OUT_P = 0x19,
 * line-in/sis[] = {
	{ 0x0b, HDA_INPUT, 1 },
	{ 0x0b, HDA_INPUT, 6 },
	{ 0x0b, HDA_INPUT, 7 },
	{ } /* end */
};
#endif

/*
 * Common callbacks
 */

static int alc_init(struct hda_c9, AC_VERB_SET 0x0b, HDA_INPUT, 0 },
	{ 0x0b, HDA_INPUT, 1 },
	{ 0x0b, HDA_INPUT, 2 },
	{ 0x0b, HDA_INP ic int alc_check_power_status(struct hda_codec *codec, hda_nid_t nid)
{
	struct alc_spec *spec = codec->spec;
	ret>init_verbs[i]);

	if (spec->init_hook)
		spec->init_hook(codec);

	return 0;
}

static void alc_unsol_event(struct hda_codec *codec, unsigned int res)
{
	struct alc_spec *spec = codec->spec;

	if NMUTE},
	{0x16, AC_VERB_SET_AMP_Gnt(struct hdP_MUTE : 0;uniwill_setuNPUT, 1 },
	{ 0x0b, HDA_INPUT, 2 },
	{ 0x0b, HDA_INPUTe Playback Switch", 0x0b, >spec;
	return snd_hda_multi_out_analog_open(codec, &spec->multiout, substream,
					     hinfo);
}

static int alc880_playback_pcm_prepare(struct hda_pcm_stream 	bits = presINPUT, 7 },
	{ } /* end */
};
	bits = prescks
 */
static int alc880_playback_pcm_open(struct hda_pcmUT_MUTEt snd_pcm_substream *substreSRSP_EN|ALC8odec->spec;
	return snd_hda_multi_out_analog_prepare(codec, &spERB_SET_PIN_WIDGE,
	{0x1b, ACer-output accct hda_codec *cec->autocfg.hp_pin4 },
	{ } /* end */
};

static struct hda_amp_list alc880_lg_loopbacks[] = {
	{ 0x0b, HDA_INPUT, 1 },
	{ 0x0b, HDA_INPUT, 6 },
	{ 0x0b, HDA_INPUT, 7 },
	{ } /* end */
};
#endif

/*
 * Common callbacks
 */

static int alc_init(struUNMUTE},
	{0x15, AC_VET),
	HDA_BIND_MUTE_MO char mask = (kclume", 0x0f, 0x0, NTROL, PIN_HP},
	{0x15,DEC_VOLUME_MONO("LFen(struct hda_pcm*/
	{0xt snd_pcm_substream *substre,
	{0x1a, ACNPUT, 1 },
	{ 0x0b, HDA_INPUT, DA_CODEC_MUT(struct hda_codec *codec, hda_nid_t nid)
{
	struct alc_spec *spec = codec->spec;
	return snd_hda_check_amp_list_power(codec, &spec->loopback, nid);
}
#endif

/*
 * Analog playback callbacks
 */
static int alc880_pC_VERB_SET_AMP_GAIN_Mned int stream_tag,
		, 0x02, HDA_INPUT),
	HDA_t,
					   struct snd_C_MUTE("Mic B_GET_PIN_WIDGET_CON, HDA_OUTPUT)en(struct hda_pcmH}, /* HP */

	{0x14, AC_VERNMUTE},
	/*multiout,
					     stream_tag, format, subP_MUTE : 0;rection mo>spec;
	return snd_hda_multi_out_analog_open(codec, &spec->multiout, substream,
					     hinfo);
}

static int alc880_playback_pcm_prepare(struct hda_pcm_stream *hinfo,
				       struct hda_codec *codec,
				       unsigned int stream_tag,
				     , (0x7000 | (0x01leanup(codec, &spec->multiout);
}

static int alc880_dig_playback_pcm_close(struct hda_pcm_stream *hinfo,
					 struct hda_coMP_OUT_MUTE},
	pec->multiout);
}

/*
 * Digital out
 */
static int alc880_dig_playback_pcm_open(struct hda_pcm_stream *hinfo,
					struct hda_codec *codec,
					struct snd_pcm_substream *substream)
{
	struccs = ARRAY_SIZE(alc660vd_dac_nids),
		. for Int = Interface for Intl Highig_outr In
 * LC861VD_DIGOUT_NIDl Hignum_channel_mode
 * Universal Int861vd_3stack_2ch
 *
 tel Hig codecs
 *
 * C) 2004 Kailang Yang <kaill Higinput_mux = &) 2004 Kacapture_sourcel Higunsol_event>
 *        lenovo_      Takasl Higsetuphi Iwai <tiwai@suse     en Hou it_hookhi Iwai <tiwai@susee@physics,
	},
};

/*
 * BIOS auto configuration
 */
static int *        ware_create_u <pshctls(struct hda_codec * Genel Hi Licconst  of the
 *  pin_cfg *cfg)
{
	return *  y
 *  it under the termsl Publ cfg, 0x15icen09, 0);
}

bute it void/or modify
 *  setdio <pshand_unmutes of the GNU General Public Li GNUnid_t nid, and/y
 *type usefu foridxee SoFounn.
 y
 **
 *   2 of th be ul,
 *  b(at yor option) any later versioe@phymultiout s of the GNU General Pubee So publishANTYpnera Publ=  Gene-> Pub;
	and/i;

	for (i = 0; i <= HDA_SIDE; i++) {
that it will be =  Pub->warecfg.linedio ipins[i];
	etaill,
 *  b = g; witho*  b(ral Public License
 *  ranty o		if (nid)ic Ly later version.
 *
 *  This driver  the implieic Lice	   ul,
 *  but unda}t your option) any later versioe@physpPARTICULAR PURPOSE.  See the
 *  GNU General Public License for more deat it willpin.
 *pineneral Public Lichp along0withion,pin) /*; yonect to frond/ond useTHOU 0trib 59 Temple Place, Suite 330, Boston, MA  0211pin, PIN_HP*  (at"
#include "hda_localspeaker"
#include "hda_bee1
#define ALC880_DCVOL_EVENT		0x02
#define ALC880_HP_EVOUT*  (at yo#defineace patch P_EVCDC 26		ce p80G,
	ALC880_
 *  MERCHANTABILITY or FITNESS Fanaloger theICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should AUTOG,
	ALASTived a copy of the GNU General Public Licu <pshalong with tf  Int_iser the pinMA  02111-1)a copyRRANTY; w700,
	ALC880_LG,
	AL <linuxation, In !face patch ,
	ALC880_ &&NFIG*/

(f nowcapn 2 of th	ALC & AC_WCAP,
	A_AMP)c., 5	snd_ GNU Gene_wri, MA  02111-13 0307 USA
AC_VERB_SE */
}_GAIN_MUTESIC,
	ALCM tag *C760undat}ux/init,
	ALC88<linux/delay.h>
#inu <pshsrc59 Te820_ACER,
	ALC260_WILLITSU_S702X,
	ALC260idx_to_mixer_vol, Inc		(, Inc + or
2)C260_FAVORIT100,
#ifdef CONFIG_switch_DEBUG	ALC260_TESTc) is  add playback; yotrolsALC8m the parsed DAC tabletrib/* BaSIC,onace p80 version. Butace patch has separate,freedifferkashNIDs 
 * iver/driver DEL_LA_FRONvolumes */
enutribute it and/or modify
 *  it undeOR A PART terms of theeral Public Li307 USA*/

; yo as published by
 *  the Free Sochar name[32withute it ,
	ALC0,
	A*chLC2624] = {"FC880", "SurroundX1,
CLFEX1,
	ide"}nclude "hda_ct wivEL_LA_s details, errLC880_FUJITSU,
	ALC8cfg->ense
 * sived a copyLC88!ral PuOR A outgh Defini[i]c., 5 */
inueONFIEL_LA>
 *        ifdef CONFIG_SND_ic Li
	ALC0e fory.h>dxSPIRE_	ALC268_3ST,
	ALC268_TOSHIBAundat /* l68_ACER_DMIC,
	ALC268_ACEDEL_LASPIRE_ONE,
	ALC268_DELL,
	ALC268_ZEPTO,
#ifdef CONFIG_SND_DEA_IL1,
i == 2880_LG_/* Center/LFE0x01
#	err>
 *dd_ */
enu to t,ace _CTL_WIDGET_VOL262_BENQ_"LC269_ P2 modelsV,
	AL"262_BENQ_ave COMPOSELC260VAL, InLAST1_BASIC,
	ALNQ_T31K,
	AOUTPUTD_DEBUIL1,

	AL< 0

/* Aftware 
};

L1,
	ALC269_ASUS_EEEPC_P703,
	ALC269_ASUS_EEEPC_P901,
	UANTFUJITSU,
	ALC269_LIFEBOOK,
	ALC269_AUTO,
	ALC269_MOD2L_LAST /* last tag */
};

/* ALC861 models */
enum {
	ALC861_3ST,
	ALC660_3ST,
	ALC861_3ST_DIG,
	ALBINDDC7600,
	ALC,
	ALC269_FUJITSU,
SEL_LA9_LIFEBOOK,
	ALC269_AUTO,
	ALC269_sODEL_2307 USA
 */

ag */
INALC861_MODEL_LAST,
};

/* ALC861-VD models */
enum {
	ALC660VD_3ST,
	ALC660VD_3ST_DIG,
	ALC661_UNIWILL_M31LC861VD_3ST,
	ALC861VD_3ST_DIG,
	ALC861VD_62T_DIG,
	ALC861VD_LENOVO,
	ALC861VD_DALLAS,
	ALC861VD_HP,
	ALC861V} else80_LG__TOSHIBA_S06pfxONFIG_SND/
enum {
	ALC2LC261f
	ALC880_A/
enum {
	ALCogram; =880_UNIWILSPEAKER
};
880_LG_IL1,
	/
enu.h"
#in

/* A	pf@rea"SEVENT	"ST,
		_P70S_MODE1,
	ALCPCMS_MODEPC_P70ic Li1,
	AL,
	ALC2 with 	sprintf(LC26, "%sNIWILL_M31,
	ALC86 pfxALC861
	ALC269_ASUS_EEEPC_P703,
	ALC269_ASUS_EEEPALC26_LIFEBOOK,
	ALC269_AUTO,
	ALC269_MOD3L_LAST /* last tag */
};

/* ALC861 models */
enum {
	ALC861_3ST,
	,
	ALC663_ASUS_M51VA,
	ALC663_ASUS_G71V,
	ALC663_ASUS_H13,
	ALC663_ASUS_G50V,US_MODE4,
	C662_ASUS_MODEDE5,
	ALC663_ASUS_MODE6,
	ALC861VD_LL,
	ALC272_DELL_ZM1,
	ALC272_SAMSUNG_NC100VD_3ST_DIUTO,
	ALC662_MODEL_LAST,
};

/* ALC882smodel3ST_6ch,
	ALC662_5ST_DIG,
	ALC662_LENOVO_101E,
	ALC662_ASUS_EEEPux/i{
	ALC860at yo/* ALC262 models */
enum {oreralENT	_FRONHP *
 *  sHIPPO,
	ALC262_HIPPO_1,
	ALC262_FUJITSU,
	ALC262_HP_BPC,
	ALC262_HP_BPC_D7000_WL,
	ALC262_HP_BPC_D7000_WF,
	ALC262_HP_TC_T5735,
	ALC262_HP_RP5700,
	ALC262extraPARTICULAR PY_ASSAMD,
	ALC262_BENude "hda_code,2_TOSHIBA_S06L,
	e Soat it will beLAST /* last tag1_3ST,0,
	ALC262_NEC,e "hda!0 board8_ACER_ASPe "hdaONE,
	Ais_fixed	ALC8_beea copy	ALC268_ACERTSU_fdef Cdac_FUJITSU2515,
	ALDELL,888_FST,
/*eral ifyLC262
	ALasLC262OVO_MACER_AS0x01
#L1,
	ALC268_3ST,
	ALhpit w
	ALC	ALC889A_MB31,
	ALC12 =DEL_LAST,
3_ASUS_MALC268_3ST,
	ALOVO_MS719it w[02_TOO_TT,
	AL.h"

#_HP_THPWF,
	AL/BPC_D70onLC26288_ASUSONFIG amp0x01
#	ALC268_ACER_DMIC,
	ALC268_ACER_ASPIRE_ONE,
	A	ALC889A_INTEL,
	ALC889G
	ALC268_TEST,
#endif
	ALC268_AUTO,
	ALC268_MODE	ALC889A_INTEL,
	ALC8
MODE5,
	ALC663_ASUS_MODE6,
	ALC272_DELL,
	ALC22_DELL_ZM1,
	ALC272_SAMSUNG_NC10,
	ALC662_AUTO,
	ALC66_MODEL_LAST,
};

/* ALC882 models g */
};

/* ALC86 models */
enum 
	ALC861_3ST,
,
	ALC885_IMAC24,
	ALC883_3ST_2ch_DIG,
	ALC83_3ST_6ch_DIG,
	ALC883_3ST_6ch,
	ALC883_6ST_DIG,
	ALC83_TARGA_DIG,
	ALC883_TARGA_2ch_DILENOVO,
	ALC861Von */
	struct snd_kcontrol_nePC_P701LC880_TITSU_PIOR A P	ALC888_FUJITS_INTet manual"

#defiion0x01
#/* we have only a_BPC_D70on HP-outHP_E0x01
# *mixers[5];	/* mixer arrays */
	unsigned int num_mixers;
	struct snd_kcontr9_ASUS_w *cap_mixer;	/* capture mixer */
	unsR_W66* codec parameterization */
	struct snd_kcontrol_neC888_ACER_ASPIRE_49_BASILC262 soft you can redi_FRON10];upLC262eral Pubfreeftware 1/

	successful, 0/

	C262_ropercm_stre is not f262_	ALC2or a negativeC,
	o_capdefree ALC262_HIPPO_1,
	ALC26 - hadne A codge iine Aoverriital Ponst st
	ALC888_LENOVO_MS719_FRONck;
	struct hda_pcm_BENQ_ED8,
	ALtribute it and/or modify_BASItruct hyou cICULAR PURPOSE.  See the
 *  GNU General Public License for more detail1_3ST,ute it at it willACER_DMIC,gnore[2_TOScens0_BA er int num_LC260 mo multi9A_Idef	/* play2 of th&ral Public Li262_BENQ_Tl
					 */
	hdaLC88 models */
enum
	ALC861_3ST,L1,
	ALC268blic License
 * SUS_M8_ACER_ASp.h"
an't fi0_WFalidhda_pc
#inapture;*/d;
	hda_nck;
	struct fille for IntPC_P703- for auto-par;

	/* capture */
	unsigned int 		/* digitP_RP5700,
	ALC262_BENQ_ED8,
	ALC2
	/* capture source */
	unsigned int num_mux_defs;
	const struct hda_input_muxOVO_MS7195_ALC262_BENQ_T31ine ALC880_MIC_EVENT		0x08

/262_BENQ_T31C662_ASUS3];
	struct alc_mic_route ext_mic;
	struct alc_mic_route int_mic;

	/* channel model */
	const stru.h"
#inclumode *channelHeadphone	int num_channel_mode;
	int need_dac_fix;
	int const_channeler version 2 of thcapture source */
	unsigned int num_mux_defs;
TO,
	ALC882_MODEmax2 codecsLC26ALC268_3ST,
	AL/882dacs * 2	ALC883_ral Public Lic audio SUS_MALC268_3ST,
	ALCaudio interface patch for ALC 26x[3];
	hda_nidkterm.listoard ddCONFIGunsigne_MAX_OUTS];
	hda_g;
	_t pverbunsigne) 2004 KaF,
	ALh>
#in;

	sMAX_Oral Pu/882muxts[3LC261d hpal Puu <pshou@real(*unsoprivundermuxclude;
	const sttruct _t pric_boosen the /
	struct auto_pin_cfg autocfg;
	eral sid_checkptional ense, or1bck_pr4MAX_Oruct hdaSPIRE_4930Giatioal e@phializ *streRE_6ware-m_stream *stre *
 _TC_T5735,
	lude <linux/delay.h>
#iICULAR PURPOSE.  See the
 *  GNU General Public License for more deBILITY or FITNESS FOR A PARTIensing */<linux/delay.h>
#include <_HDA_POWER_SAVE
	struct hda_LC880_F1734,
_HDA_POWER_SAVE
	struct hda_C260_WILLx */
	hda_;
	hda_nid      Takasoard coh>
#isicsx */
	hda}

enum copALC660VD_FIX_ASUS_GPIO1ver is  re10];pec itribute it ,
	ALC262_ULT GNU;

	on Audio Cfix_asus_gpio1ok)(st_nid_t
	{0x01,/* l0_HP,
	ALpec _MASK, or
3},e identical size
					    DIRECTION, or
1ec
					     */
	struct snd_kATAnew *cap_mi }ver iconfig_preset {
	str
 * fixupks */
	voigned hould be [be copied to the spec iuld be 	.k)(sttion Audio Cew *mixers[5]; /* shs driver iute it  of theLC26pci_quirkint num_dacs;
	_tblould be SND_PCI_QUIRK(0x104delsx1339, "the  A7-K"03,
	copied to the spec iel H{hda_verb *inithis atch_) 2004 KICULAR PURPOSE.  See the
 *  GNU General Public Liout_nid an, boar_ASUSfigg;
	stru = kzalloc(sizeof(ic Li), GFP_KERNELoef_idx, pllS_H1NULLc_nids;
	hd-ENOMEMunsise for moreruct hdunsied_dac_fix;
a_nid_t slagned _ed_dac_fix;
ptional ce patch MODELLL_DI307 USA
	int dig_ou
	unsSen Hc *);
	void (*icfged iMAX_OLC88event)(struct< 0 ||eed_dac_fix;
 >face patch truct hda_a copyE5,
	k(t nu_INFO " GNU Gene: %s:e software-probing.\n9_LIF61VD_Lnse for chip_LC26LC889event)(struct hce patch 80_Ustread: 1;
pickacs;
	ptional _outs;
	unsigned itruct hda_codec truct  CONFIG_SND_HDA_P_H13  struct sndit_verbs[wareme it 
	stru{
	ALC262da_pcm_stre0x01
#	const struct h multiout;	/* playensing */	/* capture *80_LG_LW,
fre MA  02ALC861LC662_ASUS_EEEPC_P701	ALC8err>num_muf
};


/*
 * inpALC880_Annel MUX handliCancharnalog_am_stream *stre"ux_idx], uin{
	AL sof.  Using base/
	un.._muxALC861kcontrol,
			     struct3STALC260_FU;
	hda_nid_t slaattach_beep_devic MA  02110x23;

	/* capture * copyx_defs)
		mux_idx =m_stream *streaip(kcontrol);
	struUG
	ALC880_T*specds[A    _p */
sptional -idx(kcontr */
ss[ed_dac_fix;
]_chip(kcose for vendor_Y_VA=adc_0ec066spec = dec-lways ware on EAPD0x01
#dTS];

	/* hooks *erfaceeapS];

	trucidx =(*unsostream/* for P62 modelsealtek.com.tpce *ucontrol)
{
	sid (*unso_value *ucontrw>
 *  truct hda_codec *codec =w>
 *  g;
	struct_value digitalrol)
{
	struct hda_codec *input_mux *imux;ntrol_chip(kcontinput_mustruct alc_spec *spec = trol, &ucontrol0R,
	ALC8dc_nidsdor Inte copynid = spec->ca>
 *        spec->ca_new *a_codec 		spec->capopyright (c) 2004 Kaspec->capstreamL1,
	ALC268ALC8ror Intec_nids ?
spec->num_m>
 *        spec->num_mg;
	settw>
 *   rivate	mux_idx mux_ol_champunsigne0x0 1;
	05eep_amp;	/* g;
	structvmasterONY_VAIEST,*input_mux; int nopx;
	imu_style (e.chip(kcontrol);
	struct alc_spec *specc_nids ?
e@physics.adelaide.edlay.h>
#i;
#ifdef CONFIG_dc_nave POWER_SAVE= adc_idx >= loopodel.amp	hda_nid	if (idx >= imux->num_hi Iwai <tiwax >= ims;
#endifix-mixer sroc_widf nosics.adE5,
	_realtek_coefigned int mASPIRE_4freecapsr2 support
 nsigned int is almost identical withHIPPO_1,butC262_clean
	ALC88more flexibltal P you can redi.  Eachnid_t 0;
		_t * choose any u <ps8_ASsstreaminitia.free].indADC HDA

#defieded intal = id of algned_ASP.  This makes possUT,
				6- codecs indepenE;
	 w>
 *  sx;
	freeIn: 1;
	uns, ancodec, imux, 
	ALRE_6C262OR A -62 models(charuSIC,i* exisALC26river yet)x;
	/T,
	ALC880_5662 for ALC 26	0x06de_info(struct snd_INntrol *kaional */
re optional
	ct sC268_TOSH62_TOS
nt aLC880, rear, clfekcontr_surr0x01
EST,ith spcodec4optional */
re optional
	27uct hda_cod2c *coderuct alc_spec = codec->spec;
	returct sspec->ca_mode_inf/*rn 11-2;
	struc *  x08pec = codec->spec;
	return sn	    specdig_out_annel_mode);
}

s8 optional */
channel_mode,
			spec->num_m_mode_i
	stt alc23_nid codec->spec;
	return sn
	struct hdadig_ouc = snd_kRE_49);
		}MUXHIPPO,
FIXME: should beUX satrix-gram;);
		}       selization venal */
	hda_ni GNUu <pshou@rtrol)
{
	s *          *codex priitemx;
	4,
	.;
}

staut_n{ "Mic"atic  ec
	ode_IBA_R put(stru1t snd_kcoLin_DEL0x2t snd_kcoCDt snd4t snd* optional */
	hda_ni_channel_mode,
				  ai@suse101e  spec->ext_channel_count);
}

staDIG,int alc_ch_mode_put(strul,
			   struct snd_ctl_eol)
{
	struct hda_codec *codec = snd_kco3  spec->ext_channel_count);
}

sta3ic int alc_ch_mode_put(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_eriver i#if 0p.h"10];to 1]);
	}est
			otherrol, spec->chs belowtribute it ec->num_channel_mode,
		n snnc10  spec->ext_channel_count);
}

sta16odec->spec;
	int erAutonnel_m *kcontroct snd_kcoI269_signr = snd_hda_ch_modIn-EST,t snd_ctl_elem_r.  "p3t snd3sed
 * instead 4lue *ucontr* instead 5t snd5sed
 * instead 6t snd6sed
 * instead 7t snd7sed
 * instead 8t snd8sed
 * instead 9t snd9sed
 * instead a(strucased
 * instead b(strucbsed
 * instead t(struccsed
 * instead _TYA to sed
 * instead ct snd0esed
 * instead fignoref	spec->multal == i	unsign2ch/
	unstribute it  of the GNU codecs
 *
 *de,
			3STYang <kaill *kcontr{ bee str hda_verevert to HiZ if any of these
 * are snd_kcontrore orch2h>
#iould be ind_h8ical size
				P_EV9_ASUS_CONTR2_AUP_EVVREF8ct sndm sequentially.
 * NC260_HP_DC7600 60_HP_3013,
served to aentially.
 * NIDs 0x0f and 0x10 have bIN*/
static char *alc_pin_miour as of
 * March 2006.
 */
stat}p.h"end_moder is free6 not
 * cause problems when mixer clients move t6rough the enum sequentially.
 * NIDs 0x0f and 0x10 have bOUTserved to have this behaviour as of
 * March 200UN6.
 */
static uentially.
 * NCONNECT_SELgnore_ctl_eatic char *alc_pin_mode_names[] = {
	"Mic 50present all 5ic 80pc bias",
	"Line in", "Line out",based
 * in the char *alc_pin_med to be exclusil,
			ne out",
};
staticy of these
 * are requested.  Therefore or6er this lmode_infhat tlients move throughDs 0x0f6arch 2006) and/IN_VREt sner is freel not
 * cause problems when mixer clients sixlang Y PIN_VREF80, PIN_IN, 6entially.
 * NIDs 0x0f and 0x10 ha0x0bserved to 5 ALC_PIN_DIR_INOUT           0x02
#define ALC_PI4t or an output pin.  In
 * addition, "input" pin out",
};
static unsigned char alc_pin_mode_values[] = {
	PIN_VREIR_OUT     8       0x01
#define ALC_PIN_DIR_INOUT           0x02
n, "input" pins mN_DIR_IN_NOMICBIAS    0x03
#define n, "input" pins mOUT_NOMICBIAS 0x04

/* Info about the pin modes supported by the y of these
 * are requested.  Therefor5lang Y versions up to March 2006IR_OUT           wiring in the comion the minimum adefine ALC Pin assignment: IBA_R=N */
	Rearr_in5, ,
	Ar_in6, 2_AUr_in7free(_dir) (alc_pin_Micr_in8,ode_dimode_dir9, truc-Inr_ina, HPr_inbh_modonal */
	hda_nid_t k
#defin_newrch 2006    	type ould be /*ra amp-initial262_HP_TC_T	,
	ALCDEC_EEEUME(ontrol MODE6,
	ALC272_DEL0xt alc_dec parameter,_info(structC760control *kcontrolST_2ch_DIn coruct snd_ctO,
	ALinfo *uinfo)
 snd_kco	ALC262_ *kcontrol,
			     sspec *snd_ctl_elem_info *uinfo)
{
	unsdir = (kcontrol->pnum = uinfo-0_BAue.enumerated.item;
	unsigned cha_MONO(	ALC269_FUJITSU,
	ALC269_ec *sODEL_L 16) & 0xff;

	uinfo->type =numerated.ite1_UNIWILL_M31,
	ALC86s(dir);truct snd_ctl_elem_info *uinfo)
{
	uted.items = alc_pin_modnum = uinfo-e);

	if (item_rated.item;
	unsignr);
	strcpy/
enum {
	ALC662_3ST_.name, truct snd_ct_names[item_num]);
	ret(t hda_pcm ->value.enumerated.na: 1;
	16) & 0xff;

	ui+1)
I;
		}int alc_pin_mode_info(struct snd_kcoCD *kcontrol,
			     s 1;
	4uct snd_kcontrol *kcontrol,
			l_chip(kcont alc_pin_monid_t nid = kcontrol->private_v snd_kcotrucchip(kcontrol);
	hda_nid_tct at snd_kcontrol *kcontrol,
			& 0xff;
	long signed char dir ->value.integer.value;
	uns snd_kcoMicff;
	long *valp = ucontrol-value.integer.value;
	unsigned _PIN_WIDGET_Cnd_hda_codec_readcodec, nid, 0,
						 AC_VERB_GETfo[_dir][ff;
	long *valp = ucontrol-1value.integer.value;
	unsigned le (i <= alc_pin_mond_hda_codec_readc_pin_mode_value,    /* ALC_PIN_DIR_INOUT */
	{ in_mode_max(_dir)-alc_pie order t_min(_dir)+1nfo(struct snd_kcontrol *kcontrol,
			     sct alc_snd_ctl_elem_info *uinfo)
{
	unsigned int item_num = uinfo->value.enumerated.item;
	unsignl,
			    struct snd_ctl_elem_value *ucontrol)
{
	unsignedcodec = snd_kcontrol_chip(kcontrol);
	hda_c, nid) nid = kcontrol->private_value & 0xffff;
	unsigned char& 0xff;
	long val = *ucontrol->va >> 16) & 0xff;
	long *valp = ucoc, nid)>value.integer.value;
	unsigned int pinctl = snd_hda_codePIN_WIDGET_CONTROL,
						 0x00);VERB_GET_PIN_WIDGET_CONTROL,
			c, nid)00);

	/* Find enumerated value for current pinctl sett;

	change = pinctl != alc_pin_m snd_kcontrol  = alc_pin_mode_min(dir);

	chc_pin_mode_values[i] != pinctl)
		i++;
	*valp = i <= alc_pin_modec, nid, 0,
					  Apin_mode_min(dir);
	return 0;
}

static int alc_pin_mode_putchip ct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	signed int change;
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	hdaed char dir = (kcontrol->private_valu_spec *6) & 0xff;

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = alc_pin_mode_n_items(dir);

	if (item_num<alc_pin_mode_min(dir) || item_num>alc_pin_mode_max(dir))
		item_num = alc_pin_mode_min(dir);
	strcpy(uinfo->value.enumerated.name, alc_pin_mode_names[item_num]);
	return 0;
}

static int alc_pin_mode_get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	unsignedhar dir = (kcontrol->private_value >> 16) & 0xff;
	long val = *ucontrol->value.integer.value;
	unsigned int pinctl = snd_hda_codec_read(codec, nid, 0,
						 AC_VERB_GET_PIN_WIDGET_CONTROL,
						 0x00);

	if (val < alc_pin_mode_min(dir) || val > alc_pin_mode_max(dir))
		val = alc_pin_mode_min(dir);

	change = pinctl != alc_pin_mode_values[val];
	if (change) {
		/* Set pin mode to that requested */
		snd_hda_codec_write_cache(codec, nid, 0,
					  AC_VERB_SET_PIN_WIDGET_CONTROL,
					  alc_pin_mode_values[val]);

		/* Also enable the retasking pin's input/output as requirol_chip(kco for the requested pin mode.  Enum values of 2 or less are
		 * input modes.
		 *
		0VD_3ST_Dnsigned int item_num = uinfo-62_3Sers probably
		 * reduces noise s30G,
	A(particularly on input) so we'll
		 * do it.  Hc_gpio_dataruct snd_kcontrol alc_pin_modnt beep_amp;	/* 		} else {
			snd_hda_codec_amp_stereo(codec, nid, HDA_INPUT, 0,
						 HDA_AMP_MUTE, HDA_d, 0,
						 AC_VERB_GET_PIN_WIDGET_CONTROL,
						 0x00);

	if (val < alc_pin_mode_min(dir) || val > alc_pin_mode_max(dir))
		val = alc_pin_mode_min(dir);

	chc_pin_mode_values[i] != pinctl)
CONTROL,
					  alc_pin_mode_values[val]);

		/* Also enable the retasking pin's input/output as requieeepc_p701are not
 * needed for any "produMD_MIXtruct snd_ctl_elem_value *ucontrol)
{
	signeALC262_HIPP   * Tted.WITCHed info(struct snd_kcoe-<= aBpin alue *8 codec parated.item;
	unsigned char ->privlc_pin_mode_min(dir);

	change = pinctl != alc_pin_mode_varol->private_vaf (change) {
		/* Set pin mode tnid_t nid = kcontroli>private_value & *  

static int alc_gpio_data_get(stid, 0, nid, 0,
					      AC_VERB_GET_GPIO_DATA, 0x00);

	*valp );

	/* Set/uns) != 0;
	return 0;
}
static int alc_gpio_data_put(struct snd_kcontrol *kcontrol,
			     structep20are not
 * neeontrol_chip(kcontrol);
	hda_trol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	signed int chans noise slightly (particularly on input) so we'll
		 * do it.  Howevernumerated.items = alc_pin_mode_n_items(dir);

	if (item_num<alc_pin_mode_min(dir) || item_num>alc_pin_mode_max(dir))
		item_num = alc_pin_mode_c_gpio_dataMuteCtrl int item_num = uinfo->vao

static int alc_gpio_data_get(std, 0,
						 AC_VERB_GET_PIN_WIDGET_CONTROL,
						 0x00);

	if (val < alc_pin_mode_min(dir) || val > alc_pin_mode_max(dir))
		val = alc_pin_mode_min(dir);

	change = pinctl != alc_pin_mode_values[val];
	if (change) {
		/* Set pin mode to,    /* ALC_PIN_DIR_INOUT */
	{ 2, 2bindback *   specmixer a moUD_MIX)volnnel_co(e.g. &LC260 mo a movolodecvaluelc_ch_mo,
	ALC269_AUTO,
	ALCno_inf* codec parameter acc#ifdef CONFIG_SND_DEBUG3#define alc_spdif_ctrl_0ucontrol, spec->channel_mod a more complete mixeronecontroBPC_D70devised for those models isw* necessary.
 */
#ifdef CONFIG_SND_DEBU*/
	efine alc_spdif_ctrl_info	snd_ctl_boolean_m21o_info

static int alc_spdif_ctrl_get(struct snin_mode_max(_dir)-alc_3_m51vaare not
 * needed 0VD_3VOLsigned int change;
	struct ->vaete mixer control can belue = nid | (SWsigned int change; alc_pin_signed int va     struct snd control is
 * to provide something in the test model allowing digital outputs to be
 * identified if present.  If models are found which can utilise these
 * outputs a more complete mixertre   struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = kcontrol->pri])
#p(kcontrol);
	hda_nid_t nid = kcontrol->private_value & 0xffff;
	unsigned char mask = (kcontrol->private_value >> 16twonclumd_ctl_elem_value *u ucontrol->value.integer.value;
	unsigned int val = snd_hda_codec_read(codec, nid, 0,
					      AC_VERB_GET_DIGI_CONV*codec = snd_kcon_mode_max(dir))
		val = alc_pin_mode_min(dir);

	change = pinctl != alc_pin_mode_values[val];
	if (change) {
		/* Set pin mode to that requested */
;

	/* Set/unset the masked GPIO bit(s) as needed */
	change = l_data);

	retu	  alc_pin_mode_values[val]);

	und which can utilise these
 * outputs a more complete mixerfourec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = kcontrol->private_value & 0xffff;
	unsigned char mask = (kcontrol->private_value >> 16) & 0xff;
	long val = *ucontrol->valu: 1;teger.value;
	unsigned int ctrl_data = snd_hda_codec_read(codec, nid, 0,
				2	    AC_VERB_GET_DIGI_CONVERT_1,
						    0x00);

	/* Set/unset the masked control bit(s) as needed */
	change = (val == 0 ? 0 : mask) rl_info, \
	  .gmask);
	if (val==0)
		ctrl_data &= ~mask;
	else
		ctrl_data |= mask;
	snd_hda_codec_write_cache(codec, nid, 0, AC_VERB_SET_DIGI_CONVERT_1,
				  ctrl_data);

	return change;
}
#define ALC_SPDIF_CTRL_SWITCH(xname, nid, mask) \
	{ .iface = SNDRV_CTL_ELEM_IFACE_	/* Also enable the retasking pin's input/output as requi1bj privatnd_kcontrol *kcontrol,
			ruct snd_kcontrol *kcontrol,
ue *ucontrol)
{
	signed int change;
	sontrol)
{
	struct hda_codec */
	so we'll
		 * do it.  However, havida_codec_amp_stereo(codec, nid, HDA_INPUT, 0,
						 HDA_AMP_MUTE, HDA_nfo

static int alc_eapd_ctrl_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = kcontrol->private_value & 0xffff;
	unsigned char mask = (kcontrol->private_value >> change;
	struct hda_codec *wor control can be devised for those models if
 * necessary.
 */
#ifdef CONFIG_SND_DEBUG
#define alc_spdif_ctrl_info	snd_ctl_boolean_mochip(kcontrol);
	hda_nidspdif_ctrl_get(struct snd_kcontrol *kcontrol,
			 nd_hda_cond_kcontrol_chip(kcontrol);
	hda_nid_t nid = kcontrol->private_value & 0xffff;
	unsigned char mask = (kcontrol->private_va6enabling EAPD digital outputs on the ALC26x.
 * Again, this is only usemixer21jd__chi	    AC_VERB_GET_DIGI_CONVERT_1,
						    0x00);

	k)(st ? 0 : mask) !d_hda_codec_read(col bit(s) as needed */
	change = (val == 0 ? 0 : mask) !data &= ~mask;			 HDA_AMP_MUTE, HDA_    struct snd_ctl_arly on input) so we'll
		 * do it.  However, havi    struct snd_ctl_elem_value vateif (item_num<alc_pin_mode_min(dir) ovide something in the test model allowing digital outputs to be
 * identified if present.  If models are found which can utilise these
 * out(xname, nid, mask) \
	{ .ifac15= SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .index = 0,  \/* Set/unset the masked control bit(s) as needed */
	change = (val == 0 ? 0 : mask) ! \
	  .private_value = nid | (mask<<16) }
#endif   /* CONFIG_SND_DEBUG */

/*
 * set up the input pin config (depending on the given ause, or
type)
 */
static void alc_set_input_pin(struct hda_codec *codec, hda_nid_t nid,
			      int auto_pin_type)
{
	unsigned int val = PIN_IN;

	if (auto_pin_type <= AUTO_PIN_FRONT_MIC) {
		unsigned int g71vontrol->value.integer.value;
	unsigned int val = snd_hda_codec_read(codec, nid, 0,
					      AC_VERB_GET_EAPD_BTLENABLE, 0x00);

	*valp = (val & mask) != 0;
	rrol,
			    struct snd_ctl_elem_valt) so we'll
		 * do it.  However, haviigned int item_num = uinfoREF100;
		else if (pincap & AC_PIin config (depending on the given auto-pin type)
 */
statirol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	int change;
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid);

	/* Set/unset the masked GPIO bit(s) as needed */
	change = (val == 0 ? 0 : mask) != (gpio_data & mask);
	if (val == 0)
		gpio_data &= ~mask;
	else
		gpio_data |=3_g50 snd_kcontrol_new *mix)
{
	if (snd_BUG_ON(spec->num_mixers >= ARRAY_SIZE(spec->mixers)))
		return;
	spec->mixers[spec->num_mixers++] = mix;
}

static void add_veturn;
	spec->init_verbs[spec->num_init_verbs++] = verb;
}

#ifdef CONFIG_PROC_FS
/*
 * hook for proc
 */
static void print_realtek_coef(struct snd_info_buffer *buffer,
			       struct hda_codec *codec, hda_nid_t nid)
{
	int coeff;

	if (nid != 0x20)
		return;
	coeff = snd_hda_codec_read(codec, nid, 0, AC_VERB_GET_h control to allow the enabling of the digital IO pins on the
 * ALC260.  This is incredibly simplistic; the intention of this conpin_mode_min(dir);
	return 0;
}

static int alc_pin_modech *
 are not
 * neeut_niifahanneSNDRVALC26ELEM_IFACE_MIXER0/880/amanne"Cx_put(cMod  \
	 .infog. ALC8ng <kai_nel_l Hig			 ode;
	spec->nege      
		}ode;
	spec->nepu    s 0x0f and 0x10 don't seem to
 * accept xer clients t_hook)(st(_dir)+1)

ADC:,
	ALlizatlef*strearighUS_EEEiden9c 80pc bias",
	"Line in", "Line oP_DC760(0)ec
					onst_channel_coed to be exclusi0ec
	 sndC880_ for : 262_HP_LC880/88_ASUSut.max_channels = p(F,
	ALC=*spe_nid;idenbnst_channel_count;
	else
		spec->multiout.max_chann.num_dacs = preset->num_dacs;
	spec->multi1ut.dac_nids = preset->dac_nids;
	spec->multiout.dig2ut.dac_nids = preset->dac_nids;
	spec->multiout.dig3ut.dac_nids = preset->dac_nids;
	spec->multiout.dig4max_ltioutd (*_channel_count;
	else
		spec->mulbased
t.max_channx_defs;
	if (!spec->num_mux_defs)
		spec->nu_out_nid =d_defs;
	if (!spec->num_mux_defs)
		spec->num_mux_defsm_adc_nids = preset->num_adc_nids;
	spec->ad_out_nid =e_defs;
	if (!spec->num_mux_defs)
		spec->num_mux_defsids;
	spec->dig_in_nid = preset->dig_in_nid;_out_ls;
	spec->Pin:ra amp-i0 EBUGcc->mutiou*/
	{ 3, 4 },    /* ALC_PIN_DIR_OUT */
	{ 0ec
				*/
	{ 3, 4 },   rocess the mic bias option
 *hook = p_dir->init_hook;
1ifdefdCONFIG_SNDdir_info[5][2] = {
	{ 0, 2 },    /* ALC_PIpreset->N_DIR_IN_NOMICBif

	if (preset->setup)
		preset->set,
	A->init_hook;
2ifdefeCONFIG_SNDlues are given.
 */
static signed char alcpreset->e ALC_PIN_DIR_Iif

	if (preset->setup)
		preset->set<= a(ontr)nid_:rol, spvref at 80%ONFIG_SNDuentially.
 * NIDs 0x0f and 0x10 have been obpreset->have this behaviour as of
 * March 2006.
 nels;
	spec-><= ax01, AC_VERB_SET_GPIO_MASK, 0x0onst_channel_coVERB_SET_GPIO_DIRECTION, 0x02},
	{0x01onst_channel_count;
	else
		spec-> }
};

static s& 0xfIn_verb alc_gpNFIG_SNDchar *alc_pin_mode_names[] = {
	"Mic 50pcpreset->ic 80pc bias",
	"Line in", "Line out", "HeRB_SET_GPIO-2 In:     struct88_ASUS(_hook;
#i- */
};	{ }
};

.num_dacs = preIDs 0x0f and 0x10 have bHPpreset->.num_dacs = preset->num_dacs;
	spetup)
		presetspec = codec->spec;
nnel_mode[0].channels;
	CDdex,
						 RE_603},
	{ }
};

x_defs;
	if (!sare PLL issue
 * On some codels;
	s_hda_cT_EVodec, ucontrol, spec->channel_mode,
		verbs[yle elepin_s:ue & 0xRB_Sthe  = cT_COt_da*/
	1_in,
	{17, 0b		 AC_VE i;
	struct NFIG_SNodecefs;
	if (!spec->num_mux_defs)
		spec->num_mux_def23X,
			    spec->pll_coef_idx);
	snd_hda_codec_wrAC_VElc_mux_erum_put(struct set->loopbacks;
#end(str_BTLENABLE, 2lc_gpio1_init_verbs[] =c void alc_fix_pll_iuct hda_verb *inides.
 * For each directiunit_hook)(stould be ide*/
	{ 3, 4 },   UNSOLICITED_lc_fix_pAC_USRSP_EN|10,
	ALFRONT_EVENA, 0x01},.num_dacs = prec = codec->spec;
	spec->pll_nid = nid;
HP->pll_coef_			     unsigned int coef_idx, unsigstructned int coef_bit)
{
	strucuentially.
 * Nc = codec->spec;
	spec->pll_nid |HIPPO_1_MIC->pll_coef_idx = coef_idx;
	spec->pll_coef_bit = coef_bit;
	ent, pincapll(codec);
}

stat/* Set U    ici{
		Eakasause problems when mixer clients ask;
	snd_hned int coef_bit)
{
	structruct hda_codec *codec)
{
	struct alc_s = preset->loopbacks;
#endocfg.hp_pins[0];
	int i;

	if (!nid)
		return;
	pincap = snd_hfreegenernsign int auto_micofpec- use;
	struct*cur_va amp-initia
stause problems when mixer clients lay.h>
#innnel_count)
		s
	 * U62_HP_n 1;am_analoC262defaultresent to mic-in); ieset->const_channel_connel_mode[0].channelst->const_channel_count;
	else
		spec->mulROC_COEF,
			    +) {
		esent amps (CDdefine In, hda_1 & hda_2)ent lt_plC880_-	if (*cu); i+style  0;
		); i+Note: PASD mhanneed_das_cod_M90V,GPIO_DAT2US_M90V,esent RE_6LC880); i+paut(cm[] =S];
2)if (!ni	spemp Indices: hda1	speUT);
22;
	definet_co2defineons(3,ec, = 4 (!nid)
	.num_dacs = preset->num_dacs;
	spec->multiout.dac_nids = preset->dac_nids;
	spec->multiout.dig_out_nid = preset->dig_out_nid;
	spec->multiout.slave_dig_outs = preset->slave_dig_outs;
	spec->multiout.hp_nid = preset->hp_nid;

	spec->num_mux_defs = preset->nuns); i+a_quupNSE_PRESENCE) ifdef oid alf, nums;

	n10];vol=0m_dig_mic)
		retur(!nid)
	EX,
			    spec->pll_coef_idx);
	sag *ZEROec
					odec, spec->pll_nid, 0, AC_VERB_SE)
		return;

	caloopbacks;
#endif

	if (preset->setup)return;_mic.pin upjack_present:1;

C880_ 	if (*cuums;

	nums = snd_hda
	ALconnestyle ,
	A(!nid)
	x_defs;
	if (!spec->num_mux_defs)
		spec->num_mux_defs = 1;
	spec->input_mux = preset->input_mux;

	spec->num_adc_nids = preset->num_adc_nids;
	spec->adc_nids = preset->adc_nids;
	spec->capsrc_nids = preset->capsrc_nids;
	spec->dig_in_nid = preset->dig_in_nid;

	spec->unsol_event = preset->unsol_event;
	spec->init_hookl = snd_hda_codec_read(codec, spec->pll_nid, 0,
				 AC_VERB_GET_PROC_COEF, 0);
	snd_hda_codec_write(codec, spec->pll_nid, 0, AC_VERB_SET_COEF_INDEX,
			    spec->pll_coef_idx);
	snd_hda_codec_write(codec, spec->pll_nid, 0, AC_VERB_SET_PROC_COEF,
		ct hda_vesw: 1;
	unsignd;		/*RE_6truct3= 0;
	for (i = 0; i < ARRAY_SIZE3spec->autocfg.speaker_piidenf_defs;
	if (!spec->num_mux_defs)
		spec->num_mux_defsited event for HP jack sensing */
static voi_out_ni
			     unsigned int coef_idx, uns16) & 0xint coef_bit)
{
	strucdir_info[5][2] = {
	{ 0, 2 },    /* ALC codecs, tlues are given.
 */
static signed char  codecs, vatect hda_codec *codec)
{
	struct alc_spec *spec80_MIC_EVENT:
			unsigned int val;

	if (!spec->pll_80_MIC_EVENT:
		et capability (NI},

	n    structCOEF_INDEX,
			    spec->pll_coef_idx);
	snd_a_codec_write(cEX,
			    spec->pll_coef_idx);
	snd_hda_cod9max_chanlc_spec *spec = codec->spec;
	unsigned int present, pincap;
	unsigned int80_MIC_EVENT:
		ocfg.hp_pins[0];
	int i;

	if (!nid)
		return;
	pincap = sndif (codec->vendor_id == 0x10ece = Sa formixer */
}

/* unsoli80_MIC_EVENT:
		alc_mic_automute(codec);
		break;
	}
}

static void alc_inithook(struct hda_codec *codec)
{
	alc_automute_pin(codec);
	alc_mic_automute(codec);
}

/* additional initialization for ALC888 variants */
static void alc888_coef_init(struct hda_out_nid odec)
{
	unsigned int tmp;

	snd_hda_codec_write(codec, 0x20, 0, AC_VERB_SET_COEF_INDEX, 0);
	tmp = snd_hda_codec_read(codec, 0x20, 0, AC_VERB_GET_PROC_COEF, 0);
	snd_hda_codec_p = ucoodec, 0x20, 0, AC_VERB_SETT:
		alc_automute_pin(codec);
		break;
	case ALC8struct hda_codec *codec)
{
	struct alc_spec *spec = codec->spec;
	unsigned int val;

	if (!spec->pll_nid)
		return;
	snd_hda_codec_write;
	 else
		 /* alc888-VB */
		 snd_hda_codec_read(codec, 0x20, 0,
				    AC_VERB_SET_PROC_COEF, 0x3030);
}

static void alc889_coef_init(struct hda_codec *codec)
{
	unsigned int tmp;

	snd_hda_codec_ nid = spec->autocfg.hp_pins[0];
	int i;

	if (!nid)
		return;
	pincap = sndif (codec->vendor_id == 0x10ecp;
		EF, 0);
	snd_hda_codec_writdir_info[5][2] = {
	{ 0, 2 },    /* ALCpec *spec _init_verbs[] = {
	{0x01, AC_VERB_SET_GPIO_MASKa_codec_write(codec,T_PROC_COEF, 0x830);
	 else
		 /* alc888-VB */
		 snd_hda_codec_read(codec, 0x20, 0,
				    AC_VERB_SET_PROC_COEF, 0x3030);
}

static void alc889_coef_init(struct hda_codec *codec)
{
	unsigned int tmp;

	snd_hda_codec_dec_write(codec,ase ALC_INIT_GPIO3:
		snd_hda_sequence_write(codec, alc_gpio3_init_verbs);
		break;
	case  0,
			odec,m10);
	snd_hda_codec_write(codec, 0x20, 0, AC_VERB_SET_COEF_INDEX, 7);
	s80_MIC_EVENT:
		alc_mic_automute(codec);
		break;
	}
}

static void alc_inithook(struct hda_codec *codec)
{
	alc_automute_pin(codec);ype)
{
	unsigned int tmec->vendor_id) {
		case 0x10ec0260:
			snd_hda_codec_write(codec, 0x0f, 0,
					    AC_VERB_SET_EAPD_BTLENABLE, 2);
			snd_hda_codec_ype)
{
	unsigned int tmp;

	switch (type) {
	case ALC_INIT_GPIO1:
		snd_hda_sequence_write(codec, alc_gpio1_init_verbs);
		break;
	case ALC_INIT_GPIO2:
		snd_hda_sequence_write(codec, alc_gpio2_init_verSET_COEF_INDEX, 0);
	tmp = snd_hda_codec_read(codec, 0x20, 0, AC_VERB_662:
		case 0x10ec0663:
		case 0x10ec0862:
		case 0x10ec0889:
			snd_hda_codec_write(codec, 0x14, 0,
					    AC_VERB_SETnst_channel_count)
	write(codec, 0x20, 0, AC_VERB_SET_COEF_INDEX, 7);
	snd_hda_codec_write(codec, 0x20, 0, AC_VERB_SET_PROC_COEF, tmp|0x2010);
}

static void alc_auto_init_amp(struct hda_codec *codec, int 
	alc_mic_automute(codecec->vendor_id) {
		case 0x10ec0260:
			snd_hda_codec_write(codec, 0x0f, 0,
					    AC_VERB_SET_EAPD_BTLENABLE, 2);
			snd_hda_codec_write(codec, 0x10, 0,
					    AC_VERB_SET_EAPD_BTLENABLE, 2);
			break;
		case 0x10ec0262:
		case 0x10ec0267:
		case 0x10ec0268:
		case 0x10ec0269:
		case 0x10ec0272:
		case 0x10ec0660:
		case 0x10ec0662nid = spec->autocfg.hp_pins[0];
	int i;

	if (!nid)
		return;
	pincap662:
		case 0x10ec0663:
		case 0x10ec0862:
		case 0x10ec0889:
			snd_hda_codec_write(codec, 0x14, 0,
					  uct sEFAULT:
		switch (codec->vendor_id) {
		case 0x10ec0260:
			snd_hda/* pio1_init_verbs[] = {
	{0x01, AC_VERB_SET_GPIO_MASums;

	nPD_BTLENABLE, 2);
			snd_hda_codec_wri */p.h"ic_automute(c  AC_VERB_SET_EAPD_BTLENABLE, 2);
			break;
		}
		switch (codec->vendor_id) {
		case 0x10ec0260:
			snd_hda_codec_write(codec, 0x1a, 0,
				    AC_VERB_SET_COKER_OUT)
			spec->autocfg.speaker_pins[0] =
				spec = nid;
	spec->pll_coef_idxc_spec *spec = codec->spec;
	unsigned int pres = nid;
0ec0882:
		case 0x10ec0883:
		case 0x10ec0885:
		case 0x10ec088alc_fix_pll(codec);
}

static void alc_automute_pin(strficient 0x20, 0, AC_VERB_SET_COEF_INDEX, 7);
	if ((tmp & 0xf0) == 0x20)
		/* alc888S-VC */
		snd_hda_codec_read(codec, 0x20, 0,
				   AC_VERB_SET_PROC_COEF, 0x8ruct alc_spec *spec = codecoef_init(struct hda_codec *codec)
{
	unsigned int tmp;

	snd_hda_codec_write(codec, 0x20, 0, AC_VERB_SET_COEF_INDEX, 7);
	tmp = snd_hda_codec_read(codec, 0x20, 0, AC_VERB_GET_PRec_S70xer */
}

/* unsoliconst_channel_count;
	else
		sp0x701f variants */
static void alc888_coef_init(strc->num_mux_def
	case ALC_INIT_GPIO2:
		snd_hda_sequence_write(codec, alc_gpio2_init_verbs);
		break;
	case ALC_INIT_GPIO3:
		snd_hda_sequence_write(codec, alc_gpio3_init_verbs);
		break;
	cn sndell_zT_EAPD_BTLENABLE, 2);
			EX,
			    specitch (res) {
	case ALC880_HP_EVENTodec, spec->pllitch (res) {
	case ALC880_HP_EVENTres >>= 26;
	switch (res) {
	case ALC880_HP_EVENT:
		alc_automute_pin(codec);
		break;
	case ALC880_MIC_EVENT:
		alc_mic_automute(codec);
		break;
	}
}

static void alc_inithook(struct hda_codec *codec)
{
	alc_automute_pin(codec);
	alc_mic_automute(codec);
}

/* additional initialization for ALC888 variants */
static void alc888_coef_init(struct hda_codec *codec)
{
	unsigned int tmp;

	snd_hda_codec_write(codec, 0x20, 0, AC_VERB_SET_COEF_INDEX, 0);
	tmp = snd_hda_codec_read(codec, 0x20, 0, AC_VERB_GET_PROC_COEF, 0);
	snd_hda_codec.pin = e	spec->int_mic.pin = fixed;
	spec->ext_mic.mux_idx = MUX_IDX_UNDEF; /* set later */
	spec->int_mic.mux_idx = MUX_IDX_UNDEF; /* set later */
	spec->auto_mic = 1;
	snd_hda_codec_write_cache(codec, spec->ext_mic.pin, 0,
				  AC_VERB_SET_UNSOLICITED_ENABLE,
				  AC_USRSP_EN | ALC880_MIC_EVENT);
	spec->unsol_event = alc_sku_unsol_event;
}

/* check subsystem ID and set up devodec, spec->pll_nid, 0, AC_VERB_SET_Pa_codec_write(codec, spec->pll_nid, 0, AC_VERB_SET_PROC_COE_codec *codec)
{
	unsigned int tmp;

	snd_hda_codec_write(codec, 0x20, 0, AC_VERB_SET_COEF_INDEX, 0);
	tmp = snd_hda_codec_read(codec, 0x20, 0, AC_VERB_GET_PROC_COEF, 0);
	in_mode_max(_dir)-alc_pi
 *  idx];

	type nd_kcontrol *kcontrol,
			Ctruct amode_max(dirtatic nid = kcontrol->private_value & truct a alc_pin_mod(!(ass & 1) && !(ass  (auto_pin_type <= AUTO_PIN_FRONT_MIC) {
		unsigned  snd_  "checking pincfg 0x%08x for NID 0x%x\n",
		   ass, nid);
	if  0xfss & 1) && !(ass & 0x100000))
		return 0;
	if ((ass >	}
	if (((ass >> 16) ,    /* ALC_PIN_DIR_INOUTn) any lhis stage they ai_EVENT		->spever is distributed in the hP,
	Aunalc_ttingt umerant; : override0,
	Abit	unsi 1 :	Swt hda_codec els *readint jack_pr

	*er;	/* c  MIC_EVENTSUS_LC663ENS
			ST /0x80 (ass ;
	 -->i = 0op, 1 ?alue.60_H6.
 *:) {
ALC260 models *amp__MIXeoint jack_prese */
};

/*plifier cornal Amp con,0 -->y of
 *  MERCHANTABILhis stage they aall0xffff, codec->vendor_id);
	/*
	 * 0 : override
	 * 1 :	Swap Jack
	 * 2 : 0 --> De sktop, 1 --> Laptop
	 * 3~5 : External A 1;
fier control
	 * 7~6 : Reserved
	*/
	tmp = (ass & 0x38) >> 3;	/* external Amp control */
	switch (tmp) {
	case 1:
		spec->init_amp = ALC_INIT_GPIO1;
		break;
	case 3:
/
	switch (tmp) {
	case 1:
		spec->i nid =  = ALC_INIT_GPIO1;
		break;
	case 3:
		spec->init_amp = ALC_INIT_GPIO2;      Takass of the GNU General Public Lic    override
	 *resP,
	ALC88(res >> 26)uct alc_	return;
	poard co ALC_INIT_GPIO2;
		break;
	caensing */
	unwhen the external head	spec->pll_t jack is plugged"
	 */  ass & 0xffff, cotion templa/*erna_pin_capsTakashRE_6HP j    sen,
			gned int no_analogCAP_TRIG_R* 15   : 1 --> enable the function "Mute ontrornal speaker
	 *	        when the external headp;
	unsig

/*
 *  for (!spec->autocfg.h3_ASUS_
	tm62_hippse.de>
 *    ptional 	 *	:
		spec->init_amp = ALCt hda_c    ICULAR PURPOSE.  See the
 *  GNU General Public License for more dWER_S	return 11o_hp(co get_wcapsec->aextocfg.
#incl(getodec,
			   hda_n*codid@real */
_idx];
 hda_nid_t port9 portd)
{
	if (!a, hda_nid_oid (*unso 0;
	S];
, por0] = nid;
	}

	alc_init_autoconfiguradec->vendor_id);
	/*
	 * 0 :				return 1; (!spec->autocfg.hautocfg.line_out_pins[i] 0] = nid;
	}

	alc_init_autoEQ) /*hp(codec);
	alc_init_auto_mic(codec);
	return 1;
}

static void alc_ssidount;

	/* PCM informatit port4ortd)) {
		st_MIC_EVENT		0x08

/t portb0_5ST,
	ALC88_DEFAULT;
		alc_inconfigurdefault setup UD_MIX)update	spec->init_amp = A16) & 0x ass & 0xffff, codec->vendor_id);
	/*
	 * 0 : override
	 * 1 :	Swap Jack
	 * 2 : 0 --> Desktop, 1 --> Laptop
	 * 3~5 : External to-pi\
	  l
	 * 7~6 : Reserved
	*/
k)(s /* lPINrved
_PRESENCE 0x38) >> 3;	/* external Amp control */
	switch (tmp) {
	case 1:
		spec->SENSf;
	unsigs laptop ec->multiout.m*
	 * 10~8 : Jack location
	 * 12~11: Headphfix += quirk->v1lue;
	cfg = fix->pins;
	if (c		spec->init_amp = Awrite(c
			da_verb *verbs;
};

static void alc_pick_fixup(struct hda_codec *codec,
			   const struct snd_pci_quirk *quirk,
			   const struct alc_fixup *fix)
{
	const struct alc_pincfg *cfg;

	quirk = snd_pci_quirk_lookup(codec->bus->pci, quirk);
	if (!quirk)
		return;

	fix += quirk->value;
	cfg = fix->pins;
	if (cfg) {
		for (; cfg->nid; cfg++)
			snd_hda_codec_set_pincfg(codec, cfg->nid, cfgfg) {
		for (; cfg->nid; cfg++)
			sex += quirk->value;
	cfg = fix->pins;
	if (cfg) {
		for (; cfg->nid; cfg++)
			s 0x17, AC_VERBet_pincfg(codec, cfg->nid, cfg->val);
	}
	if (fix->vp;
		
		add_verb(codec->spec, fix->verbs);
}

/*
 * ALC888
 */

/*
 * 2ch mode
 */
static struct hda_verb alc888_4ST_ch2_intel_init[] = {se, oic-in jack as mic in */
	{ 0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80 },
	{ 0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE },
/* Line-in jack as Line in */
	{ 0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN },
	{ 0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE },
/* Line-Out as Front */
	{ 0x17, AC_VERB_SET_CONNECT_SEL, 0x00},
	{ } /* end */
};

/*
 * 4ch mode
 */
static struct hda_verb alc888_4ST_ch4_intel_init[] = {
/* Mic-in ja2_f5zin */
	{ 0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80 },
	{ 0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE },
/* Line-in jack ais laptopxup *fix)
{
	const struct alc_pincfg *cfg;

	quirk = snd_pci_quirk_look0 :* ALC_PIOUT_MUTE },
/* Li/
enum {
	ALCl Amplifier MIC_EVENT:
		alc_mic_automute(codid, cfg->val);
	}
	if (fix->v 0,
						 ass & 0xffff, codec->vendor_id);
	/*
	 * 0 : override
	 * 1 :	Sw1, 3;	/* eatrix, AC_VERi_quirk *quirk,
			   const struct alc_fixup *fix)
{
	const struct alc_pincfg *cfg;

	quirk = sSET_AMP_MP_OUT_MUTE },
/* Line-in jack as Surround */
	{ 0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUTe "hda_MUTE, AM||_SET_AMP_psrc_niC260 models */
enu_cachh mode
 */
static strLC_PIN_DIR_INOUT           0x02
#_idx C_P701,
	A */
};

static struct hda_channel_mode alc888_4ST_8ch_intel_modes[4] = {
	{ 2B_SET_Pinux/init{
/* Mic-in jack as CLFE */2	{ 0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE },
/* Line-in jac is laptop xup *fix)
{
	const struct alcPIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_IN_IN (empty by default) */
	{0x12, AC_VERB_SET_PIN_WID_CONNECT_SEL, 0x03},
	{ } /* end */
};

static!quirk)
		return;

	fix += quirk->value;
	cfg = fix->pins;ec->multiout.mLC889g) {
		for (; cfg->nid; cfg++)
			snd_hda_codec_set_pincfg(codec, cfg->n Bass HP to Front 888_4ST_ch2_intel_init },!quirk)
		return;

	fix += quirk->value;
	cfg = fix->pins;, alc*/
	{0x15, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x15, AC_VERB_SET_, alc8g->val);
	}
	if (fix->v) & 0x* 15   : 1 --> enable the function "Mute internal speaker
	 *	    BPC_D70when the ext3000   sal headphone ou: jack isstruct hda_verb *verbs;
}	mux_idx =brea_getAC_VERB_SET_p;
	unsigET_CONTocfg.line_out_pins[i] =MP_GAIN_MRB_SET_AMP_GAIN_MUTE, AMP_OUT_it_auto_hp(codec);
		alc_init_auto_mic(codec);
	}
}

/*
 * Fix-up pin dec,
			   hda_nid_t porta, hda_nid_t porte, hda_nid_t portd)
{
	if (!alc_subsys2em_id(codec, porta, porte, portd)) {
		struct alc_spec *spec = codec->sec0880)
		rerintd("realtek: "
			   "Enable defaROL, PIN_OUT},
	{0x18, AC_VERB_SET_Aallback\n");
		spec->init_am/* *le unsolicited et->nu1ble unsolicited eve-out jack */ode_info(sMUTE, AMt fo_UNMUTE},
	{_MUTE, AMP_OUT      TakasTSU_S702X,
	RB_SET_UNSit_au_CONTROL, PIN_OU    C_USRSP_EN},
	{0x17, Act alc_fi_MUTE, AMP_OUTN_OUT},
 Enable unsolicited event f2r HP jack and Line-out jack */
	{ID=0x%04x CODEC_ID=ET_U2_UNMUTE},
	{0x17, AC_VERB_SET_CONNECT_SEL, 0x03},
/* Connect Mic jack to CLFE */
	{0x18, AC_VERB_SET_PIN_WIDGET_CONTRO*/
	{ 0x1a, AC_VERB_SETERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x18, AC_VERB_SET_CONNECT_SEL, 0x02},
/* _t nid;
	u32 va>spec;C_VERBSET_UNSOLICITED_ENA_spec *spec = codec->spec;N_OUT},
	{0x1b, AC_VERB_SET_AMP_GAIN_MUTE,>autocfg.hp_pins); i++) {
		nid allback\n");
		spec->init_anable unsolicited event f3 hda_codec *codec)
{
	struct alc_spec *spec = code_SET_U3_UNMUTE},
	{0x17, AC_VERB_SET_CONNECT_SEL, 0x03},
/* Connect Mic jack to CLFE */
	{0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, CLFE */
	{ 0x18, AC_VERB_S {
		nid = spec->autocfg.hp_pins[i];
		if (!nid)
			break;
		pincap = snd_hda_query_pin_caps(codPRESENCE;
		if (pincap & AC_PINCAP_TRIG_REQ) /* neePRESENCEN_OUT},
	{0x1b, AC_VERB_SET_AMP_GAIN_MUTE, Aakers muting */
	for (i = 0; i < ARR0);
		val = snd_hda_codec_read(codec, nid, 0,
					 4C_VERB_GET_PIN_SENSE, 0);
		if (val & AC_PINSENSE_PRESEN4_UNMUTE},
	{0x17, AC_VERB_SET_CONNECT_SEL, 0x03},
/* Connect Mic jack to CLFE */
	{0x18, AC_VERB_SET_PIN_WIDGET_CONTROL,erbs)
		add_verb(codec->sp; i < ARRAY_SIZE(spec->autocfg.speaker_pins); i++) {
		nid = spec->autocfg.speaker_pins[i];
		if (!nid4
			break;
		snd_hda_codec_amp_stereo(codec, nid, 4DA_OUTPUT, 0,
					 HDA_AMP_MUTE, mute);
	}
}da_codec *codec)
{
	struct alc_spec0);
		val = snd_hda_codec_read(codec, nid, 0,
					 5C_VERB_GET_PIN_SENSE, 0);
		if (val & AC_PINSENSE_PRESEN5_UNMUTE},
	{0x17, AC_VERB_SET_CONNECT_SEL, 0x03},
/* Connect Mic jack to CLFE */
	{0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, as mic in */
	{ 0x18, AC_; i < ARRAY_SIZE(spec->autocfg.speaker_pins); i++) {
		nid = spec->autocfg.speaker_pins[i];
		if (!nid5
			break;
		snd_hda_codec_amp_stereo(codec, nid, 5DA_OUTPUT, 0,
					 HDA_AMP_MUTE, mute);
	}
} *spec = codec->spec;

	spec->autoc0);
		val = snd_hda_codec_read(codec, nid, 0,
					 6C_VERB_GET_PIN_SENSE, 0);
		if (val & AC_PINSENSE_PRESEN6E) {
			spec->jack_present = 1;
			break;
		}
	}

	mute = spec->jack_present ? HDA_AMP_MUTE : 0;
	/* Toggle internal speakers mu};

/*
 * ALC888 Fu; i < ARRAY_SIZE(spec->autocfg.speaker_pins); i++) {
		nid = spec->autocfg.speaker_pins[i];
		if (!nid6
			break;
		snd_hda_codec_amp_stereo(codec, nid, IN_VRETPUT, 0,
					 HDA_AMP_MUTE, mute);
	}
}

static_GAIN_MUTE, AMP_IN_MUTE(0xb)allback\n");
		spec->init_amp = ALC_INIT_DEFAintdd("_VERverbs;
};

static void alc_pick_fixup(struct hda_codec *codec,
			   const struct snd_pci_quirk *quirk,
			   const struct alc_ficontrol
	 * 7~6 : Reserved
	*/

lc_pincfg *cfg;

	quirk = snd_pci_quirk_lookup(codec->bus->pci, quirk);
	if (!quirk)
		return;

		if (!(ass & 0x8000))
		return 1;
	/*
	 * 10~8 : Jack location
	 * 12~11: Headphone out -> 00: PortA, 01: PortE, 02: PortD, 03: Resvered
	 * 14~1intdd("LC880 0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80 },
	{ 0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE },
/* Line-in jack as SurrounMP_OUT_UNMUTE},
	{0x15, AC_VERB_SET_CONNECT_SEL, 0x00},
	{ }
};

/*
 * ALC888 Acer Aspire 6530G model
 */

static struct hda_verb 
	{0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN | PIN_VREF80},
/* FrontUNMUTE},
	{0x17, AC_VERB_SET_CONNECT_SEL, 0x03},
/* Connect Mic jack to CLFE */
	{0x18, AC_VERB_SET_PIN_WIDGET_CONTROL,T_CONNECT_SEL, 0xERB_SET_AMP_GAIN_MUTE, AMP_OUT	spec->pll_ Enable headphon Mic: set to PI; i < ARRAY_SIZE(spec->autocfg.speaker_pins); i++) {
		nid = spec->autocfg.speaker_pins[i];
		if (uct s			break;
		snd_hda_codec_amp_stereo(codec, ntdd("realT},
	{0x1b, AC_VERB_SET_AMP_GAIN_MUTE, AN_HP},
	{0x15, AC_VERB_SET_AMble headphone output */
	{0x15, A_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x14, AC_VERB_SETientUNMUTE},
	{0x17, AC_VERB_SET_CONNECT_SEL, 0x03},
/* Connect Mic jack to CLFE */
	{0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x18, AC_VERB_SET_CONNECT_SEL, 0x02},
/* 
};

/*
 * ALC88ienter Aspire 8930G model
 */

static struct hda_v(i = AUTT},
	{0x1b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x1b, AC_VERB_SET_CONNECT_SEL, 0x00},
/* Etruct snd_kcontrol *kcontrol,
			     scs_ctl_elem_value *ucontrol)
{
	signed int change;
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = kcontrol->pri/trucInvate_value & 0xffff;
	unsigned char mask = (kcontrol->al CLFE rivate_value >> 16) & 0xff;
	long val = *ucontrol->value.integeN_OUT},
	{0x16, Aunsigned int gpio_data = snd_hda_codec_read(codec, nid, 0,
						    AC_VERB_GET_GPIO_DATA,
						    0x00);

	/* Set/unset the masked GPIO bit(s) as needed */
	change = (val == 0 ? 0 : mask) != (gpio_data & mask);
	if (val == 0)
		gpio_data &= ~mask;
	else
		gpio_data m_dacs = e_min(_dir)+1)

gned int change;->spec;
	ally it und		/* MUruct sndLC888c_automute(coew *mix)
{
	if (snd_BUG_ON(spec->num_mixers >= ARRAY_SIZE(spec->mixers)))
		return;
	spec->mixers[spec->num_mixers++] = mix;
}

static void add_verb(struc16) }
#endif   /* CONFIG_SND_DEBUG */

/*
 * set up the input pin config (depending on the given auto-pin type)
 */
stati AC_VERB_SET_PROC_COExnd_hda_codec_write_cache(codec, s & 1) && !(ass & 0x100000))
		rE, 0x02},
/* DMICf (change) {
		/* Set pin mode to that requested */E, 0x02}ate_value & 0xffff;
	unsigned AC_VERB_SET_PROC_COIsnd_hda_codec_write_cache(codec, nid, 0,
					  AC_VERB_SET_PIN_Wbecome a differenet->mixers[i]; i++)
		add_mixer(spec, preset->mixerbecome a
						    AC_VERB_GET_GPIO_DA supported by the = ucontrol->value.enumerated.item_t nid;
	u32 va	if (*cur_ONE,
	A	if (*curFxx) and ure;
cm			 imux->item:UTE;
			snd_hda_codec_a
	{0x1b, AC_VERB2dec *codec = snd_kco_ONE,
	Aec *codec = snd_kcoROC_COEF, 0x0003},
	{ }
};
heckingstruct hda_input_muheckingROC_COEF, 0x0003},
	input_mux *imux;struct hda_iinput_mux *imux;e ADC */
	{
		.num_items = 4/* Front mic only a	{ "CD", 0x4 },iour wilm_stream *stream_aumerat!= 0;
	for (_TOSHIBA_S06s(codec, nils *dac_2*loopbacks;a_nid_t *dac_de_put(strDIG]	= "ilang -dig \
	4 },
		},
	}chip
static struct 6ch hda_input_mux alc888_aaspire_6530_sou_input_mux a5STcer_aspir6truct hda_input_mux aLENOVOip(kEatic ai@sus-p(kc_input_mux athe sEEEPC_P70dig_o"struc- snd},
			{ "Line In", 0x2 EP2cfg {"CD", 0snd__input_mux aECS			{ "cs_input_mux3ne In"M51VA			{ ) & 0num_items = 4,
		G71V			{ uct num_items = 4,
		H13			{ h13 "Ext Mic", 0x0 },50		{ "Li50 In", 0x2 },
			{ truc			{ "mixe-ET_UN},
			{ "Line In"trucmode__input_mux2,
	}
};

static strucD", 0xinput_mux},
			{ "Input Mixtruc62_TO first "AD4" */
	{
		.num_items 5 5,
		.items =5" */
	{
		.num_items 6 5,
		.items =6_input_mn snDELL]	tic n = ", 0xb },
			{ "_ZM1atic n = -zm alc889_cn snSAMSUNG_NC10atic samsung-cs =},
			{ "LineUTOInput T_SE" optional */
	hda_nid_t *slave_dig_ous[i]) *);
#nt num_adc_nids;
	hda_nid_ AC_Vx9087, "ECSt *capsr},
	} masdc_nids;
	hda_nid_2	}
	if2d6, "	{ "t *cap,
			{ "ne", 0x2 },
			{ "CD", 0x4 },
f4	{ "Inp ZM1ut Mix", 0xa },
	{ne", 0x2 },
			{ "CD",t *adc_000s;
	hda_N50Vmt *capsrtatic structr[] = {
	HDA_CODEC_VOLUME("Fro92 PlaybackBume", 0x0c, 0x0, HD3r[] = {
	HDA_CODEC_VOLUME("Fr1c3s;
	hda_M70Vwitch", 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Sudround Pla Switch", 0x0c, 2, HDA_OUTPUT),
	HDA_BIND_MUTE("Fro1fSurround Playback Sture_sourcesr[] = {
	HDA_CODEC_VOLUME("Fr2ap_nround Playback Switch", 0x0d, 2, HDA_INPUT),
	HDA_CODEC_Vnids;
	hda_"Center Playback Volume", 0x0e, 1, 0x0,
		HDA_OUTPU6rround Pla"Center Playback Volume", 0x0e, 1, 0x0,
		HDA_OUTPU75e, 1, 2, HDA_INPUT),
	HDA_BIND_MUTE_MONO("LFE Playback Switch6,
	HDA_CODEC_VOLUME_MONO("LFE 6CODEC_VOLUME("Side Playback Vol5me", 0x0f, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Side Playback S8e, 1, 2, HDA_INPUT),
	HDA_BIND_MUTE_MONO("LFE Playback Switchb,
	HDA_COF70Sput Mix, 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("S7rround PlaUX", 0e", 0x0c, 0x0,item_CODEC_VOLUME("Line Playback VLUME_MONO(X58LALC2er Playback Volume", 0x0e, 1, 0x0,
		HDA_OUTPU8 set1, 2, HDA_INPUT),
	HDA_BIND_MUTE_MONO("LFE Playback Switc8	 * round Playback Switch", 0x05HDA_CODEC_VOLUME("Mic Boost", 3ume", 0x0f, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Side Playback 8t *ae", 0x0b, 0x0, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Boost", ", 0x0e, 2F50Z0x02, HDA_INPUT)0x0d, 2, HDA_INPUT),
	HDA_CODEC_V86_kco", 0x0b, 0x0, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Boost", 7		{ ocfg.speaker_pins[0] = 0x14;
}

static void alc888_acer_as8ound Playitem0x02, HDA_INPUT),
	HDA_C/*codec)
{
	struct alc_spec *spec = code0Vrec->spec;

	spec->auto(!nig.hp_pins[0] = 0x15;
	spec9round Play Volume", 0x0c, 0x0, HDA_INPUT),
	HDA_CODEC_VOLUME("S89>autocfg.X5reataker_pins[2] = 0x17;
}

static void alc889_aceritch", 0x0N80Vt(st>spec;

	spec->autocfg.hp_pins[0] = 0x15;
	spec"Surround P81T = (>spec;

	spec->autocfg.hp_pins[0] = 0x15;
	specLUME_MONO("505Tppins[0] = 0x14;
	spec->autocfg.speaker_pins[1] = 9),
	HDA_COF5Gx04, HDA_INPUT),
	HD = 0x1b;
}

/*
 * ALC880 3-stacolume", 0x0b, 0x0, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Boost",90b, 0x0, HF80Qe)
 * Pin assignment: Front = 0x14, Line-In/Surr =r_aspire_4Vx3ck Volume", 0x0d, 0x = 0x1b;
}

/*
 * ALC880 3-stac", 0x0e, 2, 2, HDA_Iker_pins[2] = 0x1b;
}

/*
 * ALC880 3-staclume", 0x0X71C_setup(struct hda_codec *codec)
{
	struct alc_spe9lume", 0x0b5051>spec;

	spec->au0_dac_nids[4] = {
	/* front, rear,	spec->autN, 0x02, HDA_INPUT)0_dac_nids[4] = {
	/* front, rear,a_aspire_4", 0pins[0] = 0x14;", 0g.hp_pins[0] = 0x15;
	spec->autdevices.
 *Playback Switch", 0x0d, tocfg.speaker_pins[1] = 0x16;9itch", 0x0b,dec->spec;

	spec->autocfg.hp_pins[0] = 0x15;
	spe9rround PlaF5Z/F6xe)
 * Pin assignment: Front = 0x14, Line-In/Surr ="Surround Playback Switch", ,
	HDA_CODEC_MUTE("Line Playback 9e,
	HDA_CODEC_VOLUME_MONO("LFE Playback Volume", 0x0e, 2, 0x09LUME_MONO("Center Pl.num_items =r[] = {
	HDA_CODEC_VOLUME("F829t PlaybacP5GC-MXx02, HDA_Ilc888_acer_or 3-stack) */
/* 2ch mode */
a1ire_6530Etrucx02, HDA_INPUT), 0x2 },
	or 3-stack) */
/* 2ch mode */
din to input, m  },
ute it */
	{ 0x1a, AC },
ne", 0x2 },
			{ "CD",5 laptoc			{ Fox

#d0 },
			{ "Line", 0x2 },
			{ "CD",ic-in td4ic",t vref  45CMX/45GN_VRECMXd_t enum_inforb alc880_threestack_ch2_init[] = {
	/*17
			{ff63_ASToshiba NB20ero connection in thfor 3-stack) */
/* 2ch44nfo->cant PlS 0x0 } 		{ ut Mix", 0ms = {
			{ da_verb alc880_threest5	}
	ia0n;
	"Gigabyte 945GCM-S2Lx18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE },
	{5witc */
_uns"Biostar TA780G M2+_verb alc880_threestack_ch2_init[] = {
	/*63o-pinc1SENS"PB RS6g_setup(struct hd,
	HDA_CODEC_MUTE("Line P7aaE("Fro13_ASLi@susx02, HDA_Ims = {
			{ne", 0x2 },
			{ "CD"84
			{366 PlaybROCK K10N78FullHD-hSLI R3.	{ "L Fronb alc880_threestack_ch2_init[] = {  * wa_cha5

	*vfont P0x2ont PlaybacH13-200x \
	  VERB_SET,
			{ "CD dig_in_nid;
	unsis[5];
	unsi_fix;
numerat, 0x0003}meratedCD", 0x4 },
		},
	}
};

stag_out_ni& AC_PIc *sin_mode_put(struct st snd_he@phyd;		/* o_MUTE("Fr
				returSwitch" private_ * Universal Interuct hda_coel High Definition Auduct hda_co * HD audio interface ct snd_kcontro * HD audinayback Switch", 0ct snd0/880/882 codecs
 *
 * Copyright (c) 2
		},
	}
};
<kailang@realtek.com.tw>
 *  x0, HDA_OUTPUT),
en Hou <pshou@realtek		   spec->ext_chacount =ut_mux alc888_acer_aUTPUT),
	HDA_BIND_MUTE("Front 		 * for arch 2006);

	spec->cSwitch", 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Surround Playback Volume", 0x0f, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Surround Playback Switch", 0x0f, 2, HDA_INPUT),
	HDA_CODEC_VOLUME_MONO("Center Playback Volume", 0x0e, 1, 0x0, HDA_chip vers	HDA_CODEC_VOLUME_MONO("LFE Playchip versVOLUMEeece forfite, pme", 0x0e, 2, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE_MONO("Center Pack Switch", 0x0e, 1, 2, HDA_INPUT),
	HDA_BIND_MUTE_MONO("LFE Playback Switch", 0x0e, 2, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("CD Playback Volume", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_MUne Playback Volume", 0x0b, 0x02, HDA_INPUT),
	HDA_CODEC_MUTE("Line Playback Switch", 0x0b, 0x02, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x0b, 0x0, HDA_IADC */
	UTPUT),
	HDA_BIND_MUTE("Frn_mode_minA_BIND_MUTE_MONO("LFE Playback Switch", 0x0e, 2, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("CD Playback Volume", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_MUTE("CD Playback Switch", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_VOLUME("Line Playback Volume", 0x0b, 0x02, HDA_R_IN_NOMICBI	HDA_CODEC_VOLUME_MONO("LFE R_IN_NOMICBIme", 0x0e, 2, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE_MONO("Cms = {
			{ UTPUT),
	HDA_BIND_MUTE("Frstage they are noSwitch", 0x0c, 2, HDA_INPUT),
	HDA_CODEBIAS */
};ed int coef_bVolume", 0x0b, 0x3, HDA_INPUT),
	HDA_CODEC_MUTE("Front Mic Playback Switch", 0x0b, 0x3, HDA_INPUT),
	HDA_CODEC_MUTE("HeadpOUTPUT),
	HDA_CODEC_VOLUME_MONO("LFE Playback Volume", 0x0e, 2, 0x0, HDA_OUrol_chip(kcontrol);
	struc           Takashi Iwa~13: Resvered
	 * 15   : 1 woithe@physics.adela ALC_INIT_GPIO2;
		break;
	cND_MUTE_MONO("Ce In", 0x2 },
			{ UT),
	HDA_BIND_MUTE("Frstruct snd_ctl_eNPUT);
	err = snd_hda_mixer_amp_volume_iPIRE_ONEtruct hda_codec *codec)
	mutex_unlock(&codec->control_mutex);
	return err;
}

static int alc_cap_vol_tlv(struct snd_kcontrol *kcontrol, int op_flag,
			   unsigned int size, unsigned int __user *tlv)
{rol);
	struct alc_spe if (tmp == 2)
		                _init_auto_hp(c	int err;

	mutex_lock(&c;
		snd_printkcontrol->private_value =  },
			{OSE_AMP_VAL(spec->adc_nids[0]snd_hda_co snd_kco_BIND_MUTE_MONO("LFE Playback Switch", 0x0e, 2, 2, HDA_INPUxer_amp_tlv(kcontroEQ) /* need triggerVolume", 0x0b, 0x3, HDA_INPUT),
	HDA_CODEC_MUTE("Front Mic Playback Switch", 0x0b, 0x3, HDA_INPUT),
	HDA_CODEC_MUTE("Headphone Playback Switch", 0x19, 0x0, HDA_OUTPUT),
	{
		.if
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct alc_spealler(struct snd_kcontrol *kcontrol,
				 slc_init_au snd_ctl_elem_value *ucontrol,
struct alc_fND_MUTE_MONO("C
	},
	{OSE_AMP_VAL(spec->adc_niN_WIDGETcodec->spec;
	unsigned int adc_idx = snd_ctl_get_ioffxt)
				returze, tlv);
	mutex_unlock(&codec->control_mutex);
	return err;
}

typedef int (*getput_call_t)(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol);

static int alc_cap_getput_caller(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol,
				 getput_call_t f= 4,
		.items = UT),
	HDA_BIND_MUTE("16) & 0xff;
	NPUT);
	err = snd_hda_mixer_amp_volume_info(kcec0880)
		res >>= C_VOLUME("Surround Playback Volume", 0x0f, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Surround Playback Switch", 0x0f, 2, HDA_put_call_t)(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol);

static int alc_cap_getputAMP_OUT_UNMUTE},
	{kcontrol *kcontrolack to Surrou	int err;

	mutex_lock
	{}
};

static  snd_kcontrol *kcont,
			{ " struct snd_ctl_elem_vauct snd_kcol)
{
	return alc_cap_getput_caller(kcontrol, uctdd("realtek: E snd_hda_mixer_amp_switch_get);
}

static int alc_cap_sw_put(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	return alc_cap_getput_caller(kcontrol, ucontrol,
				     snd_hda_mixer_amp_switch_put);
}

#defiutput */
	{0x14,(num) \
	{ \
		.ifac89 Acer AsTL_ELEM_IFACE_MIXER, \
	verb alc889_a snd_kcontrol *kcont"CD", 0 struct snd_ctl_elem_value *ucontrol)
{
	return alc_cap_getput_caller(kcontrol, ucontrol,
				     snd_hda_mixer_amp_switch_get);
}

static int alc_cap_sw_put(struct snd_kcontrol value *ucontrol)
{
	return alc_cap_getput_caller(kcontrol, ucontrol,
				     snd_hda_mixer_amp_switch_put);
}

#define _DEFINE_CAPMIX(num)EM_IFACE_MIXER, \
		.name = "Capture Switch", \
		.acc, 0xa }NDRV_CTL_ELEM_ACCESS_REAient: 0x%
		.count = num, \
		.info = alc_cap_sw_info, \
	(i = AUTO_PIN_ snd_hda_mixer_amp_switch_get);
}

static int alc_cap_sw_put(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	return alc_cap_getput],
						      3, 0, HDA_INPUT);
	err = func(kcontrol, ucontrol);
	mutex_ec->num_channel_ \
			   SNDRV_CTL_ELEM_ACCMic by default K), \
		.count = num,x14, AC_Vo = alc_cap_vol_info, \
	x14, AC_VERB snd_kcontrol *kcontrruct hda struct snd_ctl_elem_value *ucontrol)
{
	capINE_CAPcontrol,
 0;
	for (i = 1; idec->spec;
	unsigned int adc_idx = snd_ctl_get_iwrite(codec, 0x20, 0, C_VOLUME("Surround Playback Volume", 0x0f, 0x0, HD_SONY_VAI	cap_alc_cap_sw_put(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	return alc_cap_getput_caller(kcontrol, ucontrol,
				     snd_hda_mixer_amp_switch_put);
}

#definT_UNSOLICITED_EN(num) \
	{ \
		.iface17, AC_VERne DEFINE_CAPMIX(num) \
sVENT | AC_USRkcontrol->private_vaurces[3] UT),
	HDA_BIND_MUTE("Fr= ucontrolFE = 0x04 (0x0d),
 *      Side = 0x02 (0xd)
 * Pin assignment: Front = 0x14, Surr = 0x17, CLPROC_COEF, 0);
	snd_hd snd_hda_mixer_amp_switch_get);
}

static int alc_cap_sw_put(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	return alc_cap_getput_caller(kcontrol, ucontrol,
				     snd_hda_mixer_amp_switch_put);
}

#dec->spec;
	unsigned kcontrol *kcontrol,
, nid);
		i	int err;

	mutex_lock(&trigger? */
		ck model
 *
 * DAC: Fron= alc_cap_vol_put, \
		.tlv  0,
						    AFE = 0x04 (0x0d),
 *      Side = 0x02 (0xd)
 * Pin assignment: Front = 0x14, Surr = 0x17, CLFE  AC_VERB_SET_EAPD_BTLENA       Line-In/Side = 0x1a, Mic = 0x18, F-Mic = 0x1b, HP = 0x19
 */

/* additional mixers to alc880_three_stack_mixer */
static struct snd_kcontrol_new alc880_five_stack_mixer[] = {
	HDA_CODEC_VOLUME("Side Playback Volume", 0x0d, 0x0, HDA_OUTPUT),
	HDA_BINCE) {
			specPlayback Switch", 0x0d, )
			brINPUT),
	{ } /* end */
};

/HDA_OUTPUTck model
 *
 * DAC: Fronec *coderuct snd_ctl_elem_va.iface = SNDRV_CTL_EFE = 0x04 (0x0d),
 *      Side = 0x02 (0xd)
 * Pin assignment: Front = 0x14, Surr = 0x17, CLFE = 0x16
 *                Line-In/Side = 0x1a, Mic = 0x18, F-Mic = 0x1b, HP = 0x19
 */

/* additional mixers to alc880_three_stack_mixer */
static struct snd_kcontrol_new alc880_five_stack_mixer[] = {
	HDA_CODEC_VOLUME("Side Playback Volume", 0x0d, 0x0, HDA_OUTPUT),
	HDA_BIN	else
		res >Playback Switch", 0x0d, 4;
	speINPUT),
	{ } /* end */
};

/tocfg.speack model
 *
 * DAC: Fron			{ c880_6stack_capture_source =p;
		pincap = sFE = 0x04 (0x0d),
 *      Side = 0x02 (0xd)
 * Pin assignment: Front = 0x14, Surr = 0x17, CLFEALC_INIT_DEFAULT:
		       Line-In/Side = 0x1a, Mic = 0x18, F-Mic = 0x1b, HP = 0x19
 */

/* additional mixers to alc880_three_stack_mixer */
static struct snd_kcontrol_new alc880_five_stack_mixer[] = {
	HDA_CODEC_VOLUME("Side Playback Volume", 0x0d, 0x0, HDA_OUTPUT),
	HDA_BIN(codec);
	alcPlayback Switch", 0x0d, g.speakINPUT),
	{ } /* end */
};

/cfg.speakeck model
 *
 * DAC: Fron
			{struct hda_channel_mode alc880_ ALC26xck_modes[2] = {
	{ 6, alc880_fivestack_ch6_init },
	{ 8, alc880_fivestack_ch8_init },
};


/*
 * ALC880 6-s),
	HDA_CODEC_VOLUME("Surround Playback Volume", 0x0f, 0x0, HD1b, HP = 0x19
 */

/* additional mixers to alc880_three_stack_mixer */
static struct snd_kcontrol_new alc880_five_stack_mixer[] = {
	HDA_CODEC_VOLUME("Side Playback Volume", 0x0d, 0x0, HDA_OUTPUT),
	HDA_BIN */
	{0x12, APlayback Switch", 0x0d, HP_EVENINPUT),
	{ } /* end */
};

/front */
	ND_MUTE_MON,
			{ "I= 0x02 (0x0c), Surr = 0x05 (0x0f), CLFE = 0x04 (0x0d),
 *  mp = 0;
	for (i = 1; iUT);
	err = snd_hda_mixer_amp_volume_info(
static int alc_subC_VOLUME("Surround Playback Volun snd_hda_che = "Capture Source", */ \
		.name = "Input Source", \
		.count = num, \
		.info = alc_mux_en		spec->capsrc snd_kcontrome = "Inpids[adc_idx];
	unsigned i snd_kcontroang@rea : adc_idx;
	imul);
	struct alcontrol, ucontrol,
				     snd_hda_mixer_amp_switch_put);
}

#define _DEFINE_CAPMIX(num) \
	{ \
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
		.name = "Capture Switch"
		},
	},
	{
= 0x02 (0x0c), Surr = 0x05 (0x0f), CLFE = 0x04 (0x0d),
 *      Side = 0x02 (0xd)
 * Pin assignment: Front = 0x14, Surr t_mic.pin = ext;
	spec->int810 has rear IO for:
 * Front (DAC 02)
 * Surround (DAC 03)
 * Center/LFE (DAC 04)
 * Digital out (06)
 *
 * The system also has a pair of internal spe
				    spe headphone jack.
 * DA_COD2 on the codec, hl)
{
	struct hd
 *
 * There is a variable resistor to control the speaker or headphone
 * volume. This is a hardware-only device without a software API.
 *
 * Plugging headphones in ms = {
			{ "
	HDA_CODEC_MUTE("CD Ps DAC nids 0x0codec->spec;
	unsigned int adc_idx = snd_ctl_get_iFE = 0x16
 *                 Line-In/Side = 0x1a, Mic AC 02)
 * Surround (DAC 03)
 * Cn snd_hda_chme = "Input Source", \
		.count = num, \
		.info = alc_mux_enum_info, \
		.get = alc_mux_enum_get, \/* ucontrol);
	mutem_dacs = spec->multiout = {
UTE("Front Playback Switch", 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Surround Playback Volume", 0x0d, 0x0, HDA_OUTP codec-free software; you can redistribreadconverted intMIXNU GetoNSE, ribute it anensevalue *ucontrol)
{mi_3ST_6ch_at it will be	        NY_VA= 0x1ec->ids;
	hda Matr== ninput_NY_V>= 0x1c &&NU Ge<= 0x1e0x0, HDA_OUNY_Vid alc0_TEST,] == nid)
8_ACER_ASPIRE_49			 k Volume else {
		tont gegiveATA,  targetch", 00x0e, 1, 0x0,  *uinfo)
{
	struct hdf CONFs of the GNU General Publ"Headphone R_W6 mixer 3at it willda * 0 :at it willmix[4ude "tag */numT},
e - _nid_t slaf notialization 2 of thR_W66mix,x];
	unsignemixront 0_FUJITSU,
	ALC8), H7_QUANTA_IL1,
_CODEC_VOLUME_MONOont i]xternodel
 = 0;
	ret/

stastream_analog_capture;lics.mic.pi emptyDA_COslo
	{ }_MUTE("Headphone Playbac};
#_for_6ch_0x1b, 0x0, HDA_OUTPUT),
	{ } /* end */<linux/pci.h>
#include <sound/core.h>
#include "hda_csrcs[5 0x02 (0x0cj0c), HP = 0x03 (0x0d)
 * Pin assignment: Front = 0xol_n HP = 0x15, Mol_nront ion, umure */
	unsignel */8, Mic2 = 0x19(?),
 *          of the GNU Gene_CODEC_VOLUME_MONOol_neipec-E1601,
C1200_ASLC268_ACER,

 *  jou shoj <ct hda_input_mux private; j++nid_t;
	hda_nid_3ST,
	ALC268_TOSHj]x0e,C1200_ASMP_GAIN_Mh", 0jTE_Mt hda_input_mux privatenid_t alc880nidstream_analog_capture;D; oing
 *EVENTc_idx;C262_H{
	ALC262_BASIC,id_t dig_iam *streribute it and/or     Side D; optional */
id = portd;
		else
			return 1;
	,
	ALC262_ULTRA,
	ALC262_LENOVO_300_DIG,
	ALC888_LENOVO_cense for more details.
_INPUT),
	Hdan default c_3ST,
	ALC268_TOSruct hda__codec *x04, HDAt Playback Switch",/
enum {
	ALC267_QUANTA_IENT	ODEC_VOLUs */
static s2 of the L1V,
	ALC663along wback Volumda_nid_tLC268_ACER,
x0d, 2, HDA_INPUT),
	HDAt hda_input_mux private++atic), Frream_analog_captuume", 0x0b, 0x0, HDTS];ol ter5_DIG,
	ALC888_LENOVO_S6,
	ALC888_3ST_H);
	struc*
 * Z710V mifdef override
	 *ch*	    0,
	ALC262_NEC,
	E5,
	ALC663_ASUS_MODE6,
	ALC272_DELL,
	ALCftware FZM1,
	ALC272_SAMSUNG_NC10,
	ALC662_AUTO,
	ALC6D_LENOVLC269_AUTO,
	ALC269,	HDAC_VERB_GE};

/* ALCe Playback Volume", 0x0c,sw0, HDA_OUTPUT),
	HDA_BIND_MUTE("Headphone Playback Swtch", 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Speaker Playback Volume", 0x0d, 0ST_2ch_DIG,
	ALC
	HDA_BIND_MUTE("Speaker Playback Switch83_6ST_DIG,
	ALA_INPUT),
	HDA_CODEC_VOLUME("CD PlaybackO,
	ALC86ery_pin_caps(codec),
	Hase 1_SND_nsignee PlL_LAST\nid, 0,
	x0c, 0x0, HDAms = {
		{ "M, 3
#endif
	ALC2num_items = 2,
swtems = {
		{ "Mic", 0x1 },
		{ HDA_CODE },
	},
};


/*
 *
/* ALC262 models */
enum {
	ALC262_BASIC,
	ALC262_HIPPume", 0x0b, 0x0, HDA_IN	ALC262_BENQ_ED8,
	ALC262_SONle the function "Mute intex0b, 0x0, HDA_INPUT),
	{ } /* end */
};


/*
 * ALC880 F1734 model
 *
 * DA	ALC262_TOSHIBA_S06,
	ALC262_TOS
		HIBA_RX1,
	ALC262_TYAhis b/*,
	A*/C262_AUT
	O,
	ALC262_MODEL_0x14,ast tag */
};

/* ALC268 models */
enum {
	ALC267_QUANTA_IU General Pu_3ST,
	ALC268_TOSHIBack Volume", 0x0d, 0x0, HDA_Om),
	HPlayback Switch", = {
	0x03
};
#define ALC880_	{ "Miack Volumic =x02

static stru {
	ALC269_BASIC,
	ALC269_QUANTA_FL1,
	ALC26SUS model
"CD", 0x4 },
		ALC269"implied1ALC861 models */
enum {
	ALC861_3ST,
	ALC660_DA_INPUT),
	HDA_CODEC_VOL
	ALC2[] = 2r Playback Volume", 0x0e, 1, 0x0, HDA_OUTPUT),
	HDA_COD= 0x03 (0x0d)LUME_MONO(14, Her Playback Volume", 0x0e, 1, 0x0, HDA_OUTPUT),
	HDA_CODNO("Center Pla
	ALC214, Hck Volume", 0x0e, 2, 0x0, HDA_OUTPUT),PC_P701,
	ALC662_ASUS_EEEPC_EP20,
	ALC663_ASUS_M51VA,
	ALC663_ASUS_G71V,
	ALC663_ASUS_H13,
	ALC663_ASUS_G50V,
	ALC662_EC,
	ALC66ac_nids[ODE1,
	ALC662_ASUS_MODE2,
	ALC663_ASUS_MODE3,
	ALC663_ASUS_MODE4,
	ALC663_ASUS_MOOUTPUT),
	HDA_CODEC_VOLUME_MONO},
};


/*
 
	ALC882_6ST_DIG,
	ALC882_ARIMA,
	ALC882_W2JC,
	ALC882_TARGA,
	ALC882_ASUS_A7J,
	ALC882_ASUS_A7M,
	ALC885_MACPRO,
	ALC885_MBP3,
	ALC885_MB5_MONO("LFE Playback Switch", 0{
		{14, Hck Volume", 0x0b, 0x0, HDA_INPUT),
	HD	ALC888_ACER_ASPIRE_4930G,
	ALC888_ACER_ASPIRE_6530G,
	ALC888_ACER_ASPIRE_893ftware 
	ALNY_VifTE, vdir)
	ALHDA_(alc_e};
st, CLFE = 0x16,
 *  Mic = 0x18,OVO_MS7195_DIG,
	x0, HDA_OUTPUT),
	{ } /* end */
};


LC662_ASUS_EEEPC<linux/pci.h>
#include <sound/core.h>
#include "hda_c[] = {
	HDA_COD
};

/*	ALC883_CLEVO_M720,
	AOUTPUT)0_f1734_dac_nids[1] = {
	0xk_mont num_aLC260nnel0,
	ALC262_NEC,
.hp_b, 0correspond
			 (0x0c),l~5 :y occupi = 0x0E1601,
UTO,
	ALC880_MODEL_beep /* last tag */
};

/* ds;
	hda_nid_no wayn verbs
	it undt forget NULl
};

/*,
	ALC885_IMAC24,
	ALC883_3ST_2ch_DIG,
	ALC8back Switch", 0x0b, 0x1, HDA_INPUT),
	{ } /* end */
};


static struct hda_inpueam *stream_analog_playbacdx =INPUT),
	HDA_CODEC_VOLUME("SurrR_W660x0d, 00, HDA_OUTPUTE("Front PHDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playback Vo	unsigned int num_mux_defs;
	const stChannel Mode",
		.info = alc_ch_mode/* capture */
	unsigned int HDA_CODEC_MU* EnabL S700 62 model/struct aACER_ASPIRE_6esent alonT_PROC_COEF, 0x0003
 *  it under the termc", 0x1LC260_REPit under the termAP_TRIG_REQ) /* need ersion.
 *
 *  This driver is distributed in the hope thack Switch", 0x0c, 2, seful,
 *  bure Switch", 0x08, 0x0,odel
 *
2 (0x0c), HP snd_kcontrol_ne= 0xARRANTY; without even the implied warranty o.hp_ HDA	}
}

/* initializatio?te(co 0x03 (0x0d)
 * Pin assignment: Front[] = me", 0x0c, 0x0, HDA_OUTPUT),
	HDA_B= 10x0, HDA_O0x18, Mic2 = 0x19(?),
 *                 Line = 0x1a
 *ne PlaybfidxAC	0x02

static strucC260 models */
enum {
	ALC260_BASENABLE, 2);
			snd_hda_cod CONFI_CODEC_VORB_SET_AMP_GAIN_MUTE, (spec->autocOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more detail program; if not, write to the Free Software
 *  Foundatails.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with ton, Inc., 59 T0, HDA_OU, Suite 330, Boston, MA  02111-13UT),
	{ } /* endALC268_3ST,
	ALC268_TOSHIBAck Volume", 0x0e, 1, 0x0, HDA_OUTPUT)clude <linux/slab.h>
#include <linux/pci.h>
#include <sound/core.h>
#include "hda_codec.h"
#include "hda_local.h"
#include "hda_bee jack is pC880_DCVOL_EVENT		0x02
#define ALC880_HP_EVENTk)(struct	ALC889A_MB31,
	ALC1200x04
#define ALC880_MIC_EVENT		0x08

/* ALC880 board colume", 0x0b, 0x04, HDA_INPUT),
	HDA_CO_3ST_DIG,
	ALlayback Switch", 0x0bL_LAST,
};

/* f80_5ST,
	ALC880_50, H,
	ALC880_W810,
	ALC880_Z71V,
	ALC880_6ST,
	AL0, HDA_OUTPUT)LC880_F1734,
	ALC880_ASUS,
	ALC880_ASUS_DIG,
	ALC880_ASUS_W1V,
	ALC880_ASUS_DIG2,
	ALC880_FUJITSU,
	ALC880_UNIWILL_DIG,
	ALC880_UNIWILL,
	ALC880_UNIWILL_P53,
	ALC880_CLEVO,
	ALC880_TCL_S700,
	ALC880_LG,
	ALC880_LG_LW,
	ALC880_MEDION_RIM,
#ifdef CONFIG_SND_DEBUG
	AL SNDRV_CTL_ELEf
	ALC880_AUTO,
	ALC880_MODEL_LAST /* last tag */
};

/* ALC260 models */
enum {
	ALC260_BASIC,
	ALntrol
	 * 7~
	ALC260_HP_DC7600,
	ALC2ntrol0_HP_3013,
	ALC260_FUJITSU_S702X,
	0, HDA_OUTPUT)C260_WILL,
	ALC260_REPLACER_672V,
	ALCume", 0x0b, 0x0, H multiout;	/* playback set-up
					 * max_channels, dacs must be set
					 * dig_out_nid and hp_nid are optional
	UT),

	hda_nid_t alt_dac_nid;
	hda_nid_t slave_dig_outs[3];	/* optional - for auto-parsing */
	int Playback Sr[] = {
apture */
	unsigned int num_adc_nids;
	hda_nid_t *adc_nids;
	hda_nid_t *capsrc_nids;
	hda_nid_t dig_in_nid;		/* digi0, HDA_INPUT),
	HDA_CODverbs and input_mux */
	struct auto_pin_cfg autocfg;, 0x14, 0x0, HD00,
	ALC262_BENQ_ED8,
	ALC	HDA_CODEC_VOLUME("Headphone Playback Volume", 0x0c, 0x0, HDA_OUTPUT),
	HDA_BIOVO_MS7195"Capture Switch*/
	const struct hda_channel_mode *chanl_mode;
	int num_channel_mode;
	int need__CODEC_x[adc_idx]C882_MODEL_LAST,
};

/* for _CODEC_VOLUME("Speaker Playback Volume", 0x0d, count;

	/* PCM information */
	strt hda_pcm pcm_rec[3];	/* used in alc_build_EC_VOLUME("Mic Playback Vo_SONY_VAI0x0c, 0x0, HDA_OUTPUT),
	HDA_BIrols, init_verbs and input_mux */
	struct auto_pin_cfg autocfg;
	struct snd_array kctls;
	struct hda_input_mux private_imux[3];
	hda_nid_t private_dac_nids[AUTO_CFG_MAX_OUTS];
	hda_nid_t pr80e_adc_nids[AUTO_CFG_MAX_OUTS];
	hda_nid_t private_capsrc_nids[AUTO_CFG_MAX_Ohda_codec *codec);
	void (*unsol_event)(struct hda_codec *codec, unsid_kcontrol *kcontrol(spec->autocfg.spr[] = {
ux[adc_idx];
	return 0;
}

st3_nid_t pontrol *kcontrolanalog mixer */
} == Agned int res);

	/* for pin sensing */
	unsigned int sense_updated: 1;
	unsigned int jack_present: 1;
	unsigned int master_sw: 1;
	unsigned int auto_mic:1;

	/* other flags */
	unsigned int no_analogSwitch",
	"Su I/O only */
	int init_amp;

	/* for virtual master */
	hda_nid_t vmas0, HDA_OUTPUT),
	HDA_COD (empty by def("Line Playback Volruct snd_kcontrol_new alc_* for PLL fix */
	hda_nidme", 0x0b, 0x04, HDA_IN>autocfg.hp_pi pll_coef_bit;
};

/*
 * configuration templaute it and/ int num_662el_mode;
	const struct hda_channel_mode *channel_mode;
	int need_dac_fix;
	int const_channel_count;
	unsigned int num_mux_defs nid uct hda_input_mux *input_mux;
	void (*unsol_unsigne_pc int antrol);
	st_initir);
5 == Aevent)(struct hda_codec *, unsigned int);
	void (*se 0x2 },
			{ "codec *);
	vo,
			{ "Li/
};

sum_info( < 0)
			re	},
	},
	{
		.r[] = {
FIG_SND_HDA_POWE
#endif
};


/*
 * input MUX handling
 */
static int alc_mux_enum_info(struct snd_kcontrol *kcontrol,
			     , 0x2 },c_idx = snd_ctl_get_ioffct alc, 0x2 },c = codec->spec;
	unsigned int mux_idx = snd_ctl_get_ioffidx 0x0b, 0x04, HDA_INPUT	if (mux_idx >= spec->num_mux_defs)
		mux_idx = 0;
	return snd_hda_input_mux_info(&spec->input_mux[mux_idx], uinfo);
}

static int alc_mux_enum_get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol
		},
	}
};

stct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	ser Plalc_spec *spec = codec->spec;
	unsigned int adc_idx = snd_ctl_get_ioffidx(kc
			err =ontrol->id);

	ucontrol->varont Playbac.item[0] = spec->cl_elem_value *ucontrol)
{
	struct h0003},
	{ }
};

static ntrol_chip(kcontrol);
	struct alc_speources[2] = {
	/* Fronconst struct hda_input_mux *imux;
	unsig		.num_items = 4,
		.iteget_ioffidx(kcontrol, &ucontrol->id);
x2 },
			{ "CD", 0x4 },da_nid_t nid = spec->capsrc_nids ?
		spec->capsrc
				    spestruct sndphone jack.
 * These are both
				    spe_idx = adc_idx >= spec->num_mux_defs ? 0 : adc_idx;
	imul)
{
	struct hdda_nid_t nid = 04 (0x0d)w1(knewchecking pincf>autocfg.hp_piux[adc_idx];
	return 0;
}

sti, nps_type(get_wcaps(codec, nid));
	if (type ==== nid)
s_type(get_wcaps(codec, nid) nid = kcontr== AC_WID_AUD_MIX) {
		/* Matrix-mixer style (e.g. ALC882) */
		un	if (!spec->no_analog) {
			err =[adc_idx];
		unsigned inntrol_new alcx = ucontrol->value.enumerated.item[0];
		if (idx >= imux->num_items)
			idx = imux->num_items L? Init value_val == idx)
			return 0;
		for (i = 0; i < imux->num_items; i++) {
			unsign int  entrie != 0;
	for (i = 0; i < 
	 * 3_VOLUMEid_t slav */
s < imux- the enum .HP = 0x;
}

260, nel_mode ontroet l.init_v=};

static26bserved {0x07, AC_VERB_2ET_CONNECT_SEL, c" i},
	{0x07, AC_VERB_Svely an {0x07, AC_VERB_7ET_CONNECT_SEL, peci},
	{0x07, AC_VERB_Sength 0x00},
	{0x08, AC8ET_CONNECT_SEL, owedTE, AMP_IN_UNMUTE(0)},
	{0x09, AC_VERB_SET_9ET_CONNECT_SEL,  is
},
	{0x07, AC_VERB_S 63 ch0x00},
	{0x08, A7N_UNMUTE(0)},
	{7x08, AC_VERB_SET_CONNEint In, Mic 1 & Mic 2) 861, .re268_"Fron34SET_CONNECT_SEL6 0x0
	 e input amps (CD, L86IDs 0x0f{0x07, AC_VER6_SET_CONNECT_SEL660-Valuent
	 * panel mic (micv reque PASD motherboards usCONNECT_SEL86ew ant
	 * panel mic (mic 2)
	 */
	/* Amp Ind8IN_UNMUTE(0)},
	861 = 1, Line1 = 2, Line2 = 3, CD = 4 */
	{0x0b, AC_de alses the LineSET_P Mic1 = 0, Mic2 revgitalront
	 * panel mic (m8 Note: PASD motherboaN_MUTE(2)},
	{0x01ax(d_VERB_SET_AMP_GAIN_ alc8ront
	 * panel mic ( * Note: PASD motherboa6lume Mic1 = 0, Micof "	 * mixer widget
	 * Note: PASD motherboar8SET_CONNECT_SEL880x00},
	{0x07, AC_VERBO_1,*
	 * Set up output mAIN_MUTE, AMP_IN8x08, AC_VERB_SET_CONNE0x0b, AC_VERB_SET_AMP_G8lumeAC_VERB_SET_AMMUTE, AMP_IN_MUTE(7)},ERO},
	{0x0d, AC_VERB_SET5_MUTE, AMP_IN_MUTE(4)},
	{0x0b,889AUTE, AMP_IN_MUTE(3)},
	{0x0b, AC_VERB_SET_AMP_GTE, AMP_OUT_ZERO},
_AMP_GAIN_MUTE, AET_AMP_GAIN_MUTE, AMP_OUT_ZERO},
	/* set up input amps 0f, AC_VERB_SreatUT_ZERO},
	{0x0e, AC_VERB_SET_AMP_GAIN_MUTE_VERB_SET_AMP_G88N_MUTE, AMP_IN_UNMUTE(ZERO},
	/* set up input aCONNTE, AMP_IN_MUTE(4)},
	{0x0b,1mode MP_GAIN_MUTE, AMP_OUT_ZERO},
	/* set up input aCONNECT_SEL, 0x88},
	{0x09, AC_VERB_SETZERO},
	/* set up input aMUTE(0)},

	/* 88mute input amps (CD, L0x0b, AC_ out"terminatoT_COE codMODULE_ALIAS("snd-hda-se forid:;
}
*_ctl0x0f, ACLICved
("GPL_ctlx0f, ACDESCRIProl_("R imux- HD-audie; ydecIN_MU/*
	 * Unmute ADC0-2 and set t_num_i< imux->num_itel_co_VOLUME_nid_t slav input to micodecowdec,= THI8 chaULE optional */
2 (0_c_pin_ int n< imux->->mixn) aee Software ec = snd_9_ASU and set t(&= 0x1a, f-micfg->val);
	}
	if __exset connection lix10,of input pback Swideleruct = rear_surr, 2 = CLFE, 3 = surmodulnit_ho( connection lists )_SEL, 0xNNECTAC_VERB_SET_CONNEC)
