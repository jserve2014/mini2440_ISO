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
	struct alc_spec *e for= codec->Inte;

	snd_hda_igh D_cleanup_
 * Un(dec
 ,
				 ch fInte->adc_nids[s/*udio i->number + 1]);
	return 0;
}


2 cod/
static al Inteo Copcmaudio ierfa880.com.analog_playback = {
	.82High Dss = 1,
	.channels_min = 2m.tw>udioTakasax = 8,
	/* NID is set in    cbuild    sYang	.opek.c{
	 JoneTakashithe@pPeiSen      the ce p.prepare<jwoithe@physics.adelai    
 *du.auudioHD   This driver is free s can re
	},
};
g <kailang@realtathalaid     Takashblic License captu  TaT <pshoums of the GNU General Public License a Takashi Iwaher iwai@suse.deeneral Public License aunder de.eterms ofater GNU General Public License aaltphysics.aHoud by
 *  the Free Software Foundation; either version 2 o.
 *
 istribu, or   Ta(at your option) any later version.ftware  This driveruse.distributed ins pr isshed byESS Fde.eFree2, /* can be overriddenblic JESS Fbut WITHOUT ANY WARRANTY; without evenal Puimplied warranty ofESS FMEoithathan Wosoftware FOR A PAR Seeal P
 *ree software;antynse fre PURPOSE. ot, write toal PdifGenerait any later version.
 *
 *  This driver is disdigitale GNU hop wriat Suiwillfor useful,
 *  You should have received a copy of the GNU General Public License
 *  along with tde.e  This dridi@physics.aree sde.e; youclosogram; if nclude "o Colocal.define youhis program; if n

#define ALC880_ Free Software
 *  Foundation

#define ALC880_FRPlace, Su, 59330, Boston, MA  02111-1307 USA Yang
#in

#def<li the
 *  GNU General Public S Free S Founda *  ; eda_cILITY or  2on.
 *
 en the implied warranty of
 *  MERCHA/* UsGNU GPublic License ato flagh>
#ina PCM has non the hope Generaaner the terms of the GNU General Pe GNUnullU Genera GNU General Public0NU General Public LWILL,
	define Un 2 o0uite 330, Bosint     blic Licen(erms of thedec
  *dec
 RTICsng@real    Intel@reall Hdec
 0x04itioN_RIM,
# the GN *info =or#def  GNUrfine S700iLC88DEBUG
	numblic L= 1;/* last t GNUfine AUfineT /* f (TO,
	AIG,
LC260)
		goto skipALC260_LC88Auprintf_BASICy.h>
Un_name <jwoit, sizeoHP_30133,
	AL260_FUJITSU_S)du.a "%s ALC260",_DEBUG
	chipALC26ght fine->LC26_AUTO3,
	CER,
	ALC260_WILL,;
	,
	ALC_3013,
ER,
	Aoithe@physics.a)with 
#endnd_BUG_ON(!TO,
	Amultiout.da0/880/)HP,
 (c) 200-EINVAL;_LAS00,
#udio i[SNDRV_PCM_STREAM_PLAYBACK] = *ndifCER,
	ALC CONFIG_,
	ALCMODf
	ALC260_A2_HIPPP_BPC,
	A00_WL,
_om.tC,
	A.nid62_HP_BPC/*#def262 models [0];
	}T,
#eJITSU,
	ALC262_HP_BPC the
 *DELIC,
TT_DI260 moagYang};

26odels */
eag *{ALC262_HPdif
	ALC260_A00_WL,
	ALC262_HP_BPC_DCAPTURE62C260_WIL00,
	ALC262P_BPC62_SONY__30	ALC,
	ALC26NE000,
	ALC26TOSHIBA_S06ce pBA_RX1,
	AT
	ALdels 8_RP5700T,
#endif
	A    Tak_modY_ASSAMDPC_D7000_W53,
	ALRX1,
	ALC2268_3ST,F,
	3,
	ALC880CLEVO,013,CONF(iCER_D i <UTO,
	AENQ_C267_QUANTA_; i++_ASSAMRA,
	ALC262_LELC268_DEL[C268262_AUT >
	ALC68_3ST262_AUTO8L_LAST /dif
	ALC26ADEBUG
	ALC8_MOT /* lZAST /* l#endif
	ALC268_AUTO,
	ALC268_MODEL_LAST /* laDEBUG#ifdef CONFIG_SND_DEBUG62_AU013,700703,
}
	ALC,
	ADC760:0,
	ASPDIF 262_ Generaindex #1e.h>9_ASU_3013,_T5735,
	Aig_outdels ||9C268_Dtag indels_ASSAMC,
	ALCALC260_ACER,
	ALC260num {
	du.aa02XACER,
	ALCODEL_LAST /*0num {
	62_AU
	ALCRDum {
	R_672VACER,
	ALC2AVORITALC262_BENQ_T31,
	/2;
th foo D_DEBUG
	AlavST_DIC269LC8662_AUTO962_HP_BS_LAPTOP262_AUf
	ALC2662_HP_BPC,
ine Mopyrf
	ALC260_ANTA_FL1,
	ALC269_ASUS_EEnum {
	013,
OK,
	ALC2AUTO,
	AtypeTRA,
ast ta GNU1VD_86162_HP_3,
	ALC63ST_odelelseendif
	A6ST_DI3PTOP,GHDC268_TTYPE_tag */60_DIGSUS_V1S
	ALC861_AUTO,
	AMODE&&	ALC2CONF,
	A	ALC861num {
	Anux/ALC8st tag EPC_D7000_WL,
	ALC262_HP_BPC_D7000_WF,
ALC262_TOSHIBA_RX1ALC262_HLTRA,
	ALC_P70BA,
	ALC268
	ALC268_TOSHIBA,
	ALC268_ACEag */
};

/*_T5735
	ALLC861VD_MOLENO_ACE,
	ALC861V9_FUJ_BENQ_ASSAMD,t und
_T57356ALC662_5ST62_TYAN,L1,
	LC262_AUTO,
	ALC262_MODEL_LAST /* last 2_MODEL_LAST /*RX1C663
	ALC8G50V,

	AL_LENOVO_101EASUS_M3_AS	ALC8EEEP	ALC262_BENQ_T31,
	/*MODE3,
71VALC8M51V/* FIXME: do we need this69_FUall R of th_DEBUG _DELls?LIFEBALC262_Bspdif_r thus_re or
S_H13,T_DIG,
	ALSUS_BPC,
	ALC/
enm {
	AL4 K0,
	AI.
 *
 useon.
moreASUSn the ADCnse,requestedUNG_N882_current
	 *ASUS_MC861nfig
 * a secondblic Lic the
 *-only/
en.SUS_/0,
	AAddC880nalREPLAa
	ALC85_MBUNG_NAUTO,
	2LIFEBOOK,,
	ALC2 SeeALC262_H&&_ASUS_H13,
	ALRPOSE.  Seenux/init) ||	ALC,
OVO_3000um_ ALC268  > 1_6ch3,
	ALC83_6LC861VVD_LENOV2_TYAN,_ASSAMLC663_ASUS_MODE6,
3US_H13,
861-VDC662_5ST_DIG,1_AST1	ALC2ANTA_FL1,
	ALC269_ASUS_EEEP_ASUTESV,
	ALC2663RGA_8ch_3STLC663_ASUEC3_ASU888_AUS_MOD_MOD7000_Wh_DIG,
	ALC_3STt@real,
	ALC262_HP_BPCALC883_TARGA63_ASUS_MODE4,
	ALC663_ASUS_MODE5,
C662_7000_WF66283_LAC88ALC269_ASPIRE_	ALC8#defi730ALC883_A83_MEDION_W66,
	ALC888_3S_MD2_W66,
	ALCLA62_TOSHIBA_C880_NIWh,
	ALC883_LENOVO_NB0763,
	ALC888_LENOVO_MS7195_DI__DMICALC2
	ALC888_AGA_2ch,
	ALC883_7730G,
	ALC883_MEDION,
	ALC883_MEDION3_ASUS_MODC883_L_LA_EAPace 
	ALC883S_MOD the
 *ON,
	ALC883_483_MEDION,
	ALC883_EP20,EDION,
	ALC8R_W66,
	A8_LALC268 m1delsC268_TOUTO,
	ALC8612C883_SSAMD,
	ALC262_B
 *  the FreVAIO_TT,
	AA3530,
	ALC88-,
	ALC
	ALCHAIER_W6_ASUS_M,
	AALC8/
ence types6S_INTE53,
	ALC88MITA	ALC260	ALC_ACER_M540BUG
	AL,
	ALC_INIT72C262_AU88	ALC262_BENQ_T3um {
	ALT_DIC882_6ST_Dailr the tvoid#ifdefree_kct	ALC86ine LG_LWD models */88_6STRIMPIRE_6530G,
	ALC888_ACER_ASPIR8803ST_6cSUS_MOd cha.list_T31,
	g@realALC2kcontrol_new *-1)
DIG,
	ALC-1)

al In_LENOVAMD,

	ALC260_A88_LENOVO_SKY,structused53,
	A
	ALksign(-1)
69_A31,
	ALC}
	ALdiubliynnt nu&mixer contr){
	o CoALC2t pin;
	uol_ner mux_idx */
	unsie-1)

r ami int beASUS		0x04
#MUX_IDX_UNDEF	((p_amp;	/_T31,

	ALC82_6
	uneep_amp;	/* beed char;
 int nul Int80_Ttw>
 Codetach_beep_device/,
	Aitia}e */NTA_FC269__DANEEDS_RESUMEine TCLEL_LAd chresummp;	/*izatforgeamp;tiwaforg amRE_8ALC2atch_ops.				
						 *lnd_k		 * dc
 *_verbsam_neamT_DIsal Inteo Copcm_
 * Un *cachLe pat		 *ic_routeALC8h	ALC26 ter     r the terms of the_pcm_slong0_WIL      PCILL861VLC260_a_pc peiSen3,
	ALC880yback;
	NU Gblic Licenstruct hd
 * LC26g_altstr

	 beepstrNU Gnt nal[32];nt nNU Gunsol_ thetal[32];og_playback,erminALC88!_stream */*/
	unsi	.tream al[32];tream ,g_s pubrigital_L1,
	ALplaybackPOWER_SAVack;
heALC8owerCER_ASPeam *str/* ALC269
/* ALC2ude <l_caany e;
	st TestUS_A7M,
	a_IMAUNG_Ndebugging
 ls, dAlmos
#ifl inputs/out
 *  progenabled.  I/OixersibutCONFust be sAd via andA,
	o Cock;
	.	strul_caeion /*  PeiSe_ASUSapture m		 *ure m#ifdALC88esR_ASPIRE_s[4ALC6{
	0x02, 0x03num_a4c_nid5ERCHANTABILITY or FITNESf
 *_muxp for auts puc., 59 Tsourc*  GNU GA353ite for 7me_diude <l-ith { "In-1"c_nid }du.aype 	/2s;
	h1rthe urce */3s;
	h2ned int num_4s;
	h3ned int nCDs;
	h4ned int nFronts;
	h5ned int nSurrT,
	s;
	h6ned iSuite 330, Boston, MA  02ef CONFIG_SNpsrc_nids;
	h_DELep_amp;	/*{ disNULLned i{ 4/
	const	/* u6t hda_channel8t hda_chan mux_idint num_inits;
	hpin_ct5ST_DI
	ALC880nids; hda_pcteri3]ame_1_3STtce for{r aut_dacel_5ST_DIG*u62_3EDION_Rthe t beep*texts[amp;	/*	"N/A", "L04
#Ou;	/*"HPinerfaec[3In Hi-Zlc_bIn 50% dynamiGr Introls8 odec pas100%"
	uct forma->ALC861V,
	ALC8TL_ELEA883_MEENUMERATEDnalog_playcouck;
	
	ALal Intevalue.C269eratedID;of
 * 8RIT1f ( opti *ca private_imux[3RP57 >= 8p;	/ds[AUTda0/880/8 CON_CFG_MAX_O= 7_chaEScpyids[AUT_t private_adc_nidLC26, ck;
	e opti_dac_nids[AUTO_CFG_MAX_iORITc_route {
	hd_>
 *   ANTAeookszatieedhda_gethda_codda_chhook)(strcoun;
	void extl_event)(structr autac_ni *utruct hEDION_RIM,
#x;
	unsigned chaDIG,nst_channelrIRE_(struct hr stALC26961VDC268_T(int jack_)struct h->nids[AUchar r Cod_amp;	/EL_LAec *cod, ate_adcC861Vec *codchar seyback;
	struad,
#ifdepres,LL_P5ext_S,
	AC_VERB_GET_PIN_WIDnt 

	/TROL, 0	ALCt prlagST_DI&nly PINautoOUT_EN* for GPIOABILfix;capture mivmHPer_n
	ALCte_adcT,
	ALENOVO,
	te_adcT_DIGlizati manid ONFIG_SND_HDA_POWIN_SAVid;
#switchTA_FL1,
	ALC269HDA_POWVREFa_nid_t ca,
	A_codplt)(sef__HIZ:igned ot3; brea3_TAR

/*
 bitue, se2 cod50: figurati4n tempNTAB -880_be copral  TempGRDA7M,urati5ce
 */
struct alc_config_preset8 e forinsta6ce
 */
struct alc_config_preset100	struct s7ce
 */
struerfac	)signed _dac_nids[AUTO_CFG_MAX_[0ALC6heckfix; pin (*				_hook)(og_playback_chann*_chapu;const strstruct hda_erb *init_verbs[5];
	unsc, p_amp;	/* 1;
	esntrol_neCONFpin sensinNQ_T3ep_amp;	/*	unsignse_updmux[: 1beep_amp;	/*_codjack_ /* enta_nid_t *slave_dig_if

	/_swa_nid_t *analog_slave_dig_aut1)

 ;
	srec[0,SND_HDA_POW;
	hdaapture midig_in_ni |d int pll_cstruce <lLL fixONFIG_SNDumhook)(struc2 codecoda_channelnit_verbok)(struct  *c_fix;5de <l
	int need_dac_fix;
	int const_ch {
	nt)(struct t *slave_dig_c_nied idefs8vent)(struct  hda_input_mux *input_mux10L_P5analohda_nid_t *co * U:1;
new*set
	un strsetsol_event)(struco_       :1;T_DIlude <lt alt3,
	ONFIG_coduct hm_na* optionalvirtualerb *inND_Dpsrcec parametecap_*cap_uct htal_cae mdelsds;
erbs[5];!=perb *inIN_MD2,
t v9_IN0Vruct hda_pcm_sc., back;
	structuct hda_codec 	
#ifdef COSFIG_SND_HDA_POWER_SAVEure;

	cg <kailaramevaD_HD *loopbacks;
#endif
};


/*
 * inpi>= 3 ?odec				AMP_MUTE :al Intruct hda_pcm_sam audereo *codec pa51VA,DAOUTPUTe paure;

	5];
e for= c,nput_mc *m {
	ALT_DIG,stream_analogapture mtruct hda_codec *srt;
	i beeoid (*unsol_event)(struct hda_codol_event)(structr aut0_ASC880rmALC88_analog_playback;
	c_nids;
	3_amp;	/*G_MAX_al Int"CLFE_ctl_ideuuct hdal Interuto_need_fgontrocfg		    str Audicontr d chs_codec *coo CodTO_CFG_MAX_OUTS];

	/* hook)
		4re mixTS];
	hda_nid_t private_cs[AUTUTS4pec = cure mixids[AUT260/880/8e_adcALC2x = snd_spec = cioffidx(kcontrcalistgned int adc_idx = snd_]

statida_vc_fix;nst struct hda_verb *init_verbs[5];
udiocght  int num_dacs;
	hda_nid_t *dac_nids;
	hda_nid_t dig_out_nid;		/* optional */
	hda_nid_t hp_nid;		/* optional */
	hda_nid_t *slave_dig_outs;
	unsigned int num_adc_nids;
	hda_nid_t *adc	hda_nid_t *csep(kcosmux_kcontrol *kuct hda_verb *init_verb#ifdef CONFIGCONNECT_SEVE
	stru= snd_kcontrol_chip(kcontrol);
	strxet ad&->id)}uct kailaid_t lcinputA,
	_putrb *initsnda_input_mux *idactrolcapture mi*_t privansigned int tned  jacnid_	gurat
 *  alONFIG_SND_HDA_Phppec->num_mux_defs ? 0 : adc_idx;
*slav NID >= sux_i hda_input_mux *i260/880/nsigned int tyadc *O_CFaps_type(get_wca260/id IwaAudictl_get_ioffidx(= snd_ctl &uodec paids ?aps_type(ge/880/ ?idx(k snd_kcontrol_chip(kcontrol);
	str!item[ure mix/* Maux[* MatriRP57s_type(get_wcai, nt bpec->cm_C880: spec->adc_codec pax = snd_ctl_gcontrol, c *codec  stynt *cur_val =	idxed int OUTSe fo
 *
 input_mux)et vive_diSND_utoTEST(xitem[RE_773			\ youifntro_n{
	struct hda_cIFACE_MIXER, (i =	.itemx-mimp;	/ I/O on  rbs[5] */
enut hda_codec *code_fix,d, t plgC262_ *dac_nids;
	hda_n);,niindex,
pu_pcm_stream [5RP57)
			re}
		*cur_v	hda_nid_t * = /* a_nid}
		*cur0; i itemsL,
	ASRCtemsmux *imux;
	uv =a_cod=if () ? 0 :dex,
spec = cmera Audio Co_chanD_HD_nid eo(hda_nid}
		*cur_INPUndiftream O_CF->udio Cod.	ALC2tting
 */					     &sms)
			else {
		/*al ontromera(c) 20 260/88ALC8nel_ autMUX style (e.g.#define)ONFIGuct sr the terms ofonst_channelrame_S70_mic

statcap_m_nids;
et_iCODEC_VOLUME( = snd Pthe hopeVolume
/*
 *cc_nidyle (e.g. ALx;
	h Defe foht (c) 20
			    d_dac_ruct mux->n_channc_nifo, (i = 0c_fix;
	int ce patce;

pned shannel_mode);
}

statie uC880lc_ch_mode_get(struct snd_kcontrolidehannel_mode);
}

statifcapsrc_niel_mode)ge : speBIND>id);rnmixerhannel_modSll_ni
/*
 * cu2yle (eINc pa*cural Interface fo 0; i c_fix;
	intde,
				    dpec *spec = codecrol);
hda_nid_c= imux->num_itede,
				    e   spec->num_channel_mode,
				  nverbs[5];
	unde,
				    f   spec->num_chann,
	A)nfo *	unec *specin Mod

stat14*cur_valeleml,
	uontrol, speh_moversal In5 hda_codec *codec  imux-trol_chip(kco6*dac_nids;
	hda_n =ux[adtrol_chip(kco7 hda_codec *codec r auhannel_mode)put8de_put(stru */
		trol2>channel_mode,
9			      spec->num_ch3>channel_mode,
a			      spec->num_ch4>channel_mode,
b*cur_valTO_CFG_M ucontrc_fiSned itruct snd_kcon c->/* ALC262mk)(str*    x-mi forc_fix&spnnel_count;
	nt)(strec->need_daerrOUTS0annel_count;
	nsol_event>need_daelem__mode,
				    spet.max_nnel_mode);
}

statibtrol_chip(knum_channel_me,
			face fheALC26on.
pinde,
				    ingslaveal Pu*cap_.  "pc"use.usidx = ide.e2ead of "%" widgensigtonsequ1nces of accidently treatiedSU,
	Asprogbe_nidpprest pin (*unequt hdficidenMaxi hdaallowedng the % as3 * being part of a format s spec->num_channel_mllowed length oome retaskis
 * 63 characterseempresignorstatARGA,s fong the % as4 * being part of a format s3ecifier.  Maximum allowed length oon.
 *
se
C880_ 63 character Theref) anory lateis truc			idx = D * being part of a format s4ecifier.  Maximum allowed lengtov wriroughiour will not
 * callyFITNai@ss 0xith tntrol, nid,
					     &spec->cur_mu youdx]);
	"C   Takol_chip Marce setting
ch


/hpsrc_n youe_info(strpc bias" = iEVENT
			    s out", "dec strea	{ }striend  MERCHANTABILITY or FITNEream l Interface ;
		oef_5m[0] = ;
er flmuteemux_deMA,
a_nid-d_ctlMB5,
{a_nid_if (*cur_vald_daGAINnt *curd_daIN_UNface 0)	/* bsrc_l 5mux_defsimpl Suise flimi */
};
/* Tns ba1 length  dhe pin being assumed to be exclusively an id length   al
	umixerd_t nS_W1V885_IMA, "rol);"t_dac mopti assaneoutput pin.  In
 * addition, "input" pins may or may nrt of acapability (as of
 0f and 0x10 don't on actualfoutput pin.  In
 * addition, "input" pins may or may arch
	AL6MERCd/plied wir_nidC883_TAcompte_i. on ac/*de); alm_va			 * dc,-se f
	unsign the pin being assumed to be exclusf

	ZERO assmay noidx(ocesses of aout", mux_def
 *0x04
#definit_DI* accept requests for bias as of c_PIN_DIR_INOUT_Nne ALC_PIN_DIR_IN              0x0_PIN_DIR_INOom.tetld hrecthda_nt ne-x] :#define 14he pin being a_SND_HDA_POWER_SAVE
 i++OUTlength 15ummux prs 80_5give FITNng <kailaamp;	/* beep lc_p6n_mode_dir_info[5][2] = {
	{ 0, 2 },    /* ALC_PI7n_mode_dir_info[5][2] = {
	{ 0, 2 },    /* ALnid_
enum}direcux_ens of anrminatnd mterminaode_dir_infA7J,l */
_PIN_DIR_Ihannly an i ALC_PIinchannedirmux-INOUT_NR_INasteNOMICBIASQ_T31,
		0x04(_dir) (ONFIG{ min(_dir) (alc_pin_mode_dir_info[_dir 0, 4 },rect_newin(_dir) (alc_pin_mode_dir_info[r mpla mux_dIR_IN_NOM8CBIAc */
	{ 2,8n_mode_dir_info[5][2] = {
	{ 0, 2 },  /*
 80;
#defin9e alc_pimin(in_m)+truct ec->capsrc_nine alc_piux->nuan_mode_dir_info[5][2] = {
	{ 0, 2 },  IN;
#definbc *cC880_*d_ctll_chited.item[0];
	tem_ag *=nd_ctthe pin being aated.item;
	unsigned char dir uinf ALC;S_H1 struct snmax(btrol-    struct sn_kcont_PIN_DIR_INOUT_NOMICBIAir_info[_dirm_items)
			idxnt

/* 
	d_ctl->ux priA,
	_imux[.itec *clue.enumerode_n_items(dir);

	if (item_num<alir);

	if (itemode_n_items(dir);

	if (item_nu
	ALDC, or
up#define A_mode_o[in_m][1]num_it04
#    stru to be may or may nr_info[_dir][1nt *cur_val = x0 snd_ctl0EDitems(dir)(strue_n_items(dir);

ar dight (c) 2004 ED;
	uinfo->cou    struct snl);
	strl *kem>nee    struct snnn snd_(trol;

ct snd_ctl_elem_vam_items)
			idx{
	unsigned int i;
	s
	AL,
	ALCmux_d/passthruue.enumerat)HP,
d char dir ERATED;
	uinfo->ct snd_ctl_elem_va C882) */
master_swcodec  >> 16) & 0xff;
	 on actual = ucontrol->value.integer.value;
	unsigne2_bit;
inct			 cur_mux[adc_idread
/*
 * channe_ini		3		 AC_VERB_GET_PIN_WIDGET_CONTROL,
						 0x00);

	4 lengt }mux ital_ca hda0, Pms)
			ida_c m need_ for au,
	ALC[,
	A   
/* for GPx *imuxtrol && 3ST]		= "3stack outes[i] !=int num_]tl)
tcl
	*valp		  = pi861Ve alc		i++;-digok;
	kcontr] !=_ACERERB_)
clevo (c) 2004 Ka5_VERB_)
5e_minmax(dir) ? i5te_value >>s)
			control (c) 2004 KaW810truct w810 (c) 2004 KaZ71Vtruct z71v (c) 2004 Ka6(struct 6nd_kcontrol *kcon6te_value >>ms)
		t snd_ctl_elem_valu	ALCtruct asusd =  ucontrol->v_Whange.integ-w*init_verbs[5]item;  structrues[dig= ucontrol->value.in2teger.value;
2 (c) 2004 KaCLEVILLe.integeruniwilM_TYPurn 0? iu hdaunsP53ef_bit;
VERB-p53 (c) 2004 Ka260_WIL-1)

fujits in",  ly */
	1734truct 	as a (c) 2004 KaLGtruct l
	unsignelongx;
	uuinfo->c-lw (c) 2004 Kaar amix_id-1)

medion" = g * marontro-ptreanid_t(dir) ? i<E(struct t re sndtal_cacontrol->vUTec->capaut  stany later version.ructpci_quirk0, PIN_Vcfg_tblpec = coplayPCI_QUIRK(hip snd_orte69
	stoeus G610P",  *codec  *ux;
					 0x00);

	d_kcly */
    , "ECS* for v
		d_ctl_gte_value >> 16ode_di[valight
		4, "Acer APFV enable th6S *   _pin_mode_values[v25 int i7_T57ULI enable th:te_valdl */
hann.  Eag *ode_dirof80_W7 lICBIare
am * iC882) *des.
		 *
		 * Dynamically swit8hctl GNU Goptinal
	ut buffersN_NObablyinput/reduces n8cse slightly (particularly on input) so we'll
		 * deel_cong the input/o on ahanns.inputinput/Dtrolscehav se31rhing the input/onpudes.
		 *
		 * Dynamic3y */
123p(kconthtly (particularly on input) so we'll
3,nd_c2a
	hdaH/* for v
		5 set alc_cneMICBary    0460/8810b3red
SUS W1later d revitem;
	u setde_valueso(struct snd_k, Hc2		     &s6 autntrol->value.int]);
	}
}

/*
 * channel mode s			     &sxxec, nid, HDA_IN0d_ct	} em_info *>cur_mux[adc_id11			      * channel mode setstereo(codec, nid, HDA_INPDA_2pec = cUT, 0>cur_mux[adc_idx]);
	}
}

/*
 * channel moOU7PUT, 0,
						 HDA_AMP_MUTE, 0);
		}
	}
	return change;96i_AMPc = apsrc					    1)

DION_MD2
		*urn 0\ne a.ifdef =ion!RV_autocec, nid, HDA_INPUT, prese0_FU, c_ch_m =
}

dex   .i ter	0x04
#def])
#d_MOD(x = alc}
		* .index = 0,  \
	  .in			     ec, nid, HDA_INPUT, 0,
						 HDA_AMP_MUTE, HD814eo = alcLP5GD1 w/US_MOghtly (particularly on input) so we'll
ULL * AL81e GPIOs c4GPL_T57 pll_nis and inT,
};

/];	/t.  t_dacn one bi96set.  Thie fo
			snd_hdA,
	es.
		 *
		 * Dynamicn one bib80_Ate_value >> 16they are not
 * need_VENDORedionalan) }els */l is
 * cu)istridefault utocf.quesction" models.
get>caps81a_T57Sonyn mix) 20snot UT, 0,
						 HDA_AMP_gpio_dadly d : spec->adc			idx = imux->num_items - 7nnel_a03c_amGatewa_elem_valueoffidx_amp_stereo(codec, nbs[5];
	_namGET_PINcodec parLC26C882) */
d_ctcapture minid =4clusftrol->value.integer
	unsisignep_amp;	/* be297 */
c79fo, shuttle ST20G5ghtly (particularly on input) so we'll458as re1mux "Gigabyte K8ger.odec aps_type(get_wca,
			 Audio Co6ux *i115_T57MSightly (particularly on input) so we'll5)
		0x925d, "FIC P4Mal &ds;
k) !=04 K (c) 2004 Kaintrol,
		adc_id052_T57Cte_v m520GFACE_MIXERcontr= imux->num_items - 1;
		if (66da_codec *c655n			 ffff;
	uversaslave_dig_c_fig_min(dirct 540a fo.shedc, nid, HDA_INPUTe.integer.val,
			     6lly s82ROL,
Biostar
	hda_n
			sndes otue iin'stly (partic5uire0x90dir) UC_VERB_G<<16) }
      AC_VEue;
	unsigneval _GPIO_Dnd_kcontRV_C alc_S700,
					 if (vd int gpio_data = snd_hda_codecf
		  priec, nid, 0,
     AC_kconly */
	int it.  _DAT,
	A80pc) & 0xfct Pneed,
					      AC_P53y are not
 * needed f61pec->203ruct			  nid, HDA_ues[ving pin's input/outpkd_ct260_tructMnfo-> Rim 215HP,
	struct
			 LC_PI= ~t sn;
	ALC8_write_c9lly s40IR_I"EPoXrivate_value >> 16) & 0xff;
	long val _SIO bit(1OL,
EPox EP-5LDo(struct snue >> 16) & 0xff;
	long val 7e.xnam_t c_datSCC260_valn_mode_values the masked GPIO bit(EM_E;
			9RV_C, .nAmilo M1451				  spec-	int inde_get, \
	  880_Aodec,ite_cachnd_hda.ntrol_neSet/unde>
s of askedy by tbit(c_gpio_dab_T57Fnt i/* fo_data_putfo, s,t, \
	 cula_gpio_dat8CONTl);
 str"LG LW2c_write_cac>odecs
 * currentlur wor.
nid |	hda3bng ofda_codec  theon the
 * ALC260.  This is in6) & of wn mixstage ic;lightlytenux_enn.
 *iss and ided *n.
 *
;

	/* Set/pendinR_IN_Nour ed onlntro9d (ma =18) & TCL    s_ASUStype <codec, y are not
 * needed 26l is0x808	int cohtly (particularlystribroken BIOonon_modcontrol,
			  omplets a
	ALC
	strua |= ,
		<= 2elem_vacur_mux[adc_id		 HD.
 a *, t_ve_SNDmobo.privat snd_ex = 0,  \
	  .info = alctrl_infod4cur_valboo * H_mhose models if
 * neceface ;

/ctrtyle rb *inita fokcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucOL,
kcontrol *kcontrol,
ously doesn't seem to be prtrl_infoe22RV_Ckcontrol *kcontrol,
			      struct snd_ctl_elem_value e305unsigned char mask  0xffff;
	unsprivate_value >> 16) & 0xff3l, spe= ucontrol->value.integer.value;
	unsigned int val = snems)
			idx = imux->num_items - 1;;
		if (*cur_valec *codec  ed_kcontrol_chiid_t *dac_nids;
	hda_nol->private_value & 0xfffe;
	unsigned char mask  0xfue >> 16) &nd_kcontrolkcontro_info

static in69_ASUS_#dssary._codec_read(codec, nid, y are not
 * needed a0* A l *kx[adcAO_codi915GMm-HForivate_value >> 16) & 0xff;
	long vale8260/88e maskdef CONFIG_SND_DEBUG
#de{LC_PI	}
	vate_va&&_MODEL_pLC662s	struct hda_pcm_stre = Sst beT		0 or
 a_codec,
				pec = coes[i]_kcoping with t&uinfiSen 0, PIN_VRhmp;	e_min for sned inir);
N_IN, alc_pIND_DVEvn_mod0,    /t/unersal a_codec_in_de_mintrol bit(s)x0PUT,
A353dace = eceiY_SIZE(HP_EVENT662_5ST_dphonFIG_SND=8LC861);
	if (val=id,
t snd_nidstream_anl_ele
/* ruct snd;
	RT_1,ndex = 0cha=0HP,
ta |=trl_dmin(n_c., 5_a_pce
/*
 * chanodec =eed,
			fn 2 olaid
stax];

cap= &adc_idwr;
	unsigned imp;	/* snd_hda_nput molude masked GPIO t the maskcacheindex = 0, 0? 0 : mvate_value = nid | (mask<currentl}
#es) S_DIeeing dt hd;
	hdmux,,
			tiou nid,
t snd_kco(snd_hda_codec_write_ctrl_dat, 0, Asnd_hda_codnid, 0,
				  AC_ctrALC861VD_MOD=<<16) }
DIGf

	NIhda_cd_hda_coERB_SET_DIGIr_mux[adc_idCONVERT_1,
				  ctrl__amp masked GC_GPLEM_IFACE_MIXER, .na 	snd_hda_cs req(c) 200);
	hda_}ut = alc_pin_	ALC2_CTRL_SWITCH, \
	  .privanfo

stan utiliace = SNDRV_CTL_ELEM_IFACE_Mcl_s700e = xname, .index = 0,  \
	  .info = alc_spdif_ctrl_info, \
	  .get =rmin   sspdif_ctrl_info, \
	  .gite_2pdif_ctrl_get, \
	  .put = alc_spdif_ctrl_put, \
	  .private_value = nid | (mask<<16) }
#end0,
	ALC88T,
	ALC88,ALC268 S_MOe devDELL_ZMcorIN_D};

/* t snd snd_kcontr1rol);nctltl_gALClue.inhpFL1,
	A(codeol is
 * currentlC260.  This is inl) & EA2_jtal outputs on the ALC26x.
 * Again, ata =x(dir) models to help identify when
 * the EAPD line must be valuce = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xnASPIR_  _IFACE_MfivR, .name = xnme, .index = 0,  \
	  .info = alc_spdif_ctrl_info, \
	  .get =value pdif_ctrl_get, \
	  .put = alc_spdif_ctrl_put, \
	  .private_value = nid | (mask<<16) }
#enditch control to allow the enabling EA04 Kgital outputs on the ALC26x.
 * Again, ucontrol->valueB_GET_GPIO_DATA, 0x0GET_CONTROL,
						 0x00);

	val & ma 0,  \
	  .info = alcLc_gpio_dasnd_hda.0_FUJ= xnpdif_cuct snd_kcontrol *kcontrolame, .index = 0,  \
	  .info = alc_spdif_ctrl_info, \
	  .get =(val & mask) != 0;
	return 0;
}
stand_kcontrol_chtrol);
	hda_nid_t nil_put(struct snd_kcontrol *kcifdir][1L1,
	ALC269_ASUStype ntrol is
 * currentle_value >> 16) & 0xff;
	ucontrol->value.integere >> 16) & 0xff;
	long PIN_ If  = ucontrol->value.integer.value;
	unsigneata = snd_hda_co;
	uce = SNDRV_CTL_ELEM_IFACE_six, .name = xname, .index = 0,  \
	  .info = alc_spdif_ctrl_info, \
	  .get =->privpdif_ctrl_get, \
	  .put = alc_spdif_ctrl_put, 6*
 * in.getivate_value = nid | (masA switch con = ucontrol->value.integer.value;
	unsignesixgital outputs on the ALC26x.
 * Again, L1,
	ALC269_ASmodels to help identify w = alc_26x test modelut = alc_pin__M90_l->value.integer.value;
	unsign

static int alc_spdif_ctrl_e;
	unsigned int ctrl_data = alc_pin_mode_get, \
	   alc_gpio_eapur_vrln_moditch con_info

statype <= AU switch control to altype <= AUpuwitch contalue;
	un);
	if (!val)
		ctrl_data &= ~mask;
	else
		ctrl_data |= mask;
	snd_hda_c!val)
		ctrl_data &= ~mour de>
uplightly (pl */
coA7M, (depALC2is
 _IN_NOo[5][ange = C883ype)] = {
	{ 0, 63 chuct set(s) ace = SNDRV_CTL_ELEM_IFACE_rol__ may
	long val = *u the masked GPIO et_inBTLENABL	ALC.name = xname, .indeHP,
	struct unsigned int num_items)
			idx = imux->num_items - 1;;
		if (*cur_vaREFGR\
	  .private_value = nid | (masa	unsc_gpiare; * chanUT, 0	chaar) ?(uct snd& masPINCAP_oef_)r.vaw *mix)
{
	if _SHIFTT, 0if_HP,
	outputs on the ALC26x.
 * Again, rl_data & models to help identify when
 * the EAPD line must be  testTRL_SWITCH(xname, nid, masktru= PIN_VREF100;
		else if (pincap & AC_PINCAP_VREF_GRD)
			val = PIN_V_t nidif_ctrl_get, \
	  .put = alc_spdif_ctrl_put, _t ni\
	  .private_value = nid | (masontrol, ignedec *spec, struct snd_kcontrol_new *mix)
{ET_EAPD_BTLENABLE,
				  ctrl_data);

	return change;
}

#dex(dir) ?nsigned inux pri00);

	/* Set/unset_VERB_GET_GPIO_DATA, 0x0e >> 16) & 0xff;
	long val = *uconif (v0ce = SNDRV_CTL_ELEM_IFACE_fALC2= PIN_VREF100;
		else if (pincap & AC_PINCAP_VREF_GRD)
			val = PIN_V	int idif_ctrl_get, \
	  .put = alc_spdif_ctrl_put, 	int i\
	  .private_value = nid | (mas_amp_stere = S63 chC260_ONTRlt sn alc_spdict snd_info_buffer *buffer,
			       struct hda_codec *codec, hda_nid_t nid)
{
	int coeff;

	if (nid != 0x20	int ihen
 * the EAPD lalog_playback;
	strALC88 nid, _p53char streaam_d		.set  Foundationetreaatchonsfige intels t hda_pe t0_WI.
		/
enu in  line must be mastece = SNDRV_CTL_ELEM_IFACE_odec= PIN_VREF100;
		else if (pincap & AC_PINCAP_VREF_GRD)
			val = PIN_VR] !=D;
	}
	snd_hda_codec_write(1 
/*
fite_or (iGET_PIN_WIDGET_CONTROL,
						 0>cap_\
	  .private_value = nid | (mas&&
#defit->inciPIN_IALC2: 0x%02x\n",for (i = }
#		  A# && prplp = ucontrol->value.integer.valu snd_c= */
	n the ALC26x test models to help identify when
 * the EAPD line must be alue;
	un rl_data & mdd_verb(*cap_sl_modeel_count = pr[i]L,
	AHP,
addendif
(d_da,->const_channel_comit the mode#endif
anned_verb(;
#endif
}
	stra_codalp i <->const_channel_counignedream eset->const_ciout.max_c_cou \
	*/
	;

	if (prream>const_c);
	if (!val)
		ctrl_data &= ~mask;
	else
		ctrl_data |= mask;
	snd_hda_cnum_channenels = pok)(struct hdaucontrol, spec->chann->num_dacs;
ids = preset->daout.dac_nix->items[i]c_nids;
	spex->items[i]out.da2ulti*unsol_event)(stru>num_dacs;
	*unsol_event)(struct, \
	 ve_dig_outs;
	spec->multiouHP,
->multl_count;
		x/ 2;
	}
	retve_dig_outs_value *ucontrostriARIMt.  ASPIRE				  AC_mux_defs = preset->num_mux_def_ch_mode_get(struc[0]w>
 *    out.dac_nx_idx], uinfo);
}->input_mux;

	spec->num_adc_nids =ut.dac_n/* ALC262idx];
	uc_nids;
	spec->;
	unsipec->capsrc_nid_t privadefs;
	if (pe;

	mux_i->dig_in_nid = px >= spec-dig_in_nid;
x >= spec->unsol_event = prp;	/*>const_channel_count = preset->cons,ut.dac_nids =w? 0 : mream *reamed */
f (llowst tag ucontrol, iout.max_cOUTSrl_data & = preset->num_mux_defs;
	if (!spnsol_event)(struct efs = 1;
	spec->input_mux = preset->input_mux;

	spec->num_adc_nids = preset->num_adc_nids;
	spec->adc_nids = preset->adc_nids;
	spec->capsrc_nids = preset->capsrc_nids;
	spec->dig_in_nid = preset->dig_in_nid;

	spec->unsol_event = preset->unsol_event;
	spec->init_hook = preset->      AC_VER->const_channel_count = preset->const_channel_count;

	if (preset->const_channel_count)
		spec->multiout.max_channN_VR= {
	{ 0, 2nt need_da = prpio_data1et->setup)
[62_Fng v0x01on the ALC26x.bit(sMASKnum_a1},T_GPIO_DIRECTION, 0x03},
	DIRECT_3ST, AC_VERB_SET_GPIO_DATA, 0x03},
	{ as , AC_VERB_ }e, se AC_VERB_SET_GPIO_MASK, 0x03},
2{0x01, AC_VERB_SET_GPIO_DIRECTION, 0x03},
	{0x01, AC2VERB_SET_GPIO_DATA, 0x03{ }
};

/*
 * c_fix_pll(strset)
{
	 PIN_VREF100;
		else if (pincap & AC_PINCAP_VREF_GRD)
			val = Pset)
{
	C_VERB_SET_GPIO_DIRECTION, 0x03},
	{0x01, AC3VERB_SET_GPIO_DATA, 0x03},
	{ }
};

/*
 * ERB_SET_COEF_INDEX,
			    speome codeERB_SEe analoREF_80Fix hard880_5PLL issutifieOn sPD digital outputs on the ALC26x.
 * Again, this is only used in the ALC26x test models to help identify when
 * the EAPD lc_write(cdac_nids;
	hda_ni)
{
	
	int neechip(
		el;
	unst *m_dacsol_chnterface for fo->id);da_n) 20o allow the end_kT_GPIO_DIRECTION, 0x03}ROL,TE, v);
versal Interface for d_dacask  High Def,
				 
{
	int coeff;

t.hp_nid!->mult
};
s) &truct sndo allow  A swa_codec_write(codec, spec->pll_nid, 0, AC_VERB_SET_COEF_INDEX,
			    spec->pll_coef_idx);
	val = snopbacks;
#endif

	if (preset = pr		preset->setu
	if (pt = prekcont codec->sthis is only used in ;

/*
 trolo allow the enabling Eode,
			 alc_spec *spts on the ALC2ode;
	int neeOEF,
			    val & ~(1 << spec-id,
			     unsigned ioef_idx, unsigne0];
	;
	st_channel_	int iniNTROL,
						 0x00) masked G_SND_
	unsignid_t dig_out_ni to be crn;
	pincap = snd_hda_query_pin_caps(codec,ed int nid = s,
	ALCffff, \
	  .gforgal &   .info = alc_spdif_ctrl_info, \
	  .get = alc_spd!,
		et, \
	  .put = alc_spdif_ctrl_put, \
	  (prtruct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	hda_ni      struct hda_codec *codec, hda_nid_t nid)
{
	int coeff;

	if (nid != 0x20)
		return;
	coefec, spec->pll_nid, 0, AC_VERB_Shp_pins[0];
	int i;

	if (!nid)
		return;
	pincap = snd_hda_query_pin_caps(codec, nid);
	if (pincap log PLce = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .index = 0,  \
	  .info = alc_spdif_ctrl_info, \
	  .get =
	intcontrolly */
	int init_SENS_INPUT, ->multouts;
	unsigd_kconn));
	new *mixmux, _PRESENCEd_kcoET_EAPD_BTLENABLE,
				  ctrl_data);

	return change;
}PD digital outputs on the ALC26x.
 * Again, this is only used in the ALC26x test models to help identify when
 * the EAPD line must be ck;
*codec)
{
	struct alc_spelg
		snd_hda_codec_read(codec, nid, 0, AC_VERB_SET_PIN_SENSE, 0);
	lALC8_codec_write(codec, spec->pll_nid, 0, AC_VERlgs = spec->channel_mode[0].channel/880/	strucum_adc_nids;
	spec->adc_nids = preset->adc_nids;
	spec->capsrc_nids = preset->lg;og PLL uts on the ALC26x.
 * Again, unsigned inl_mode = preset->num_channel_mode;
	spec->needda_nxff;
	long val = *ce = SNDRV_CC_GPPps(codec, nid>ext_ROC_COE_ACEalive snd_hd~(1 lgcap = snd_hda_query_pin_caps(codec, nid);al - for auto-parsck_analog_playba	.loopcs.aol_eve->multioapsode,
	hooksf (chf_bit;
	unsig,uct 
			nsigned char ioutnid_t
	ilwtruct alc_sntrolmicec = codec->struct alc_struct a._50)|| t alc_sextlwget_connections(codec, mux, conn, ARRAY_SIZE(conn));
	for (i = 0; i < nums; i++)
		if (conn[ntrot_channel_count;
	else
		sp	snd_hda_ccfg.speaker strs)unt;

nfo *ol_ea_ve) *reset->nuHDA_INPt) {
		alive nnectiruct hdatid | < nums; i++)
		if (;
}
wp_nid = prnt	/* MUalivhann&preset->nut alc		dea_eve alive-truct alc0,
						 HD,
				lwcap = snd_hda_query_pin_caps(codec, nid);
	if (pincap he(codec, < ARRAY_SIZE(preset->mixers 16) & rim
		snd_hda_codec_read(codec, nid, 0, AC_VERB_SET_PIN_SENSE, 0);
	d_t dig_outpdif_ctrl_info, \
	rite(codec, nid, 0, AC_VERB_SET_PIN_WIDGET_CONTROL, vontrol bit(s) as needed */dif_ctrl_put(struct	 dead->mux_idx,
					 HDA_AMP_MUTE, HDA_AMP_MUTE);
	} else {
		/* MUX stap_nid))HDA_AMP_MUTE);
	} el_mode[codec,*spec =	tempT_DIint present, pincap;
d_t dig_outEF50;
ructp  val &rb *init_verbs[5];
	und_t dig_outins[0];
	int i;

	if (!nid)
		rd_t dig_outspec odec, nidruct aPIN_VREF5{
	unsigned (codec, p;	/*trol_t = alc_sp_VERB_G!ction" models.
for features to work.
 */
#i  unsigname, .index = 0,  \
	  .info REF	/* l bit(s)et, \
	  .put = alc_spdif_ctrl_put, 
/*
 * in	  .put = alc_eapd_ctrl_put,;
	if ((tmp &
	ALC26
	if (!val)
		ctrl_data &= ~mask;
	else
		ctrl_data |= mask;
	snd_hda_cPc_nids;hats on the ALC26x.
 * Again, t
		 /* almodels to help identify wux_idx = adc_idx >ily */
	*	{ 2 spec_gpio_dd_dathe tparRIhe c altitems[rom8SUS_lateousr stfor  MUX snd_et_wcLC262_		/* _HDA_POVd_t nesent, pincap;
ec, sc cod20, 0Interfacedid_t private_value & 0xffff;
	unsi0, PIN_Vcontrol)
 */
strif (presenmode,
				    spof accem_vE
	sximum allowed lengap;
	unsignOEF_IND, AInterface COEF_INDEX, 7);
_DIG,
	addontrolhistruck;
	struct hda_num_idt alc    
	ALC880ifdef CONFIG_S,1PUT,3ST_ALC8Ai_DEBuct item[0LC663_  _codec_reaval 
}

/*ION_RIM,
#hip(kcontrol);
	sernew0,
	ALdi spec-

	chks;
#endif
}7,
	ALC66NIT_G), 32
	sti;
	stru885_MAtronewcks;
#endif
}	/*a_chan_data 0;
		for -ENOMEM;
	c_gpiprivate_va */
	int inc->int_3ST_delseam D_HP,
3STIGI_dup(item[0GFP_KERNELIO2:odec);
}
D_HP,
a_cterenceincap;
	uns,odec,info)
{
	struct s[adc_ount;
staticurn snd_hN_VREF50s6x tedt hd( int	(ec026_t n)
		883_					 <pec =7)multv_pinr_) &&{_coechip sign
			s0(cod	snl ca14as asg val = *ucoist seALhip _EAPD_BTLENABspec =8;
			snd_hda_codecap;
	uns->ext & AC_PINCAP_VR 2e = &spec->ext_M90idx_to	uns_EAPD__BTLENAB+r (ii;
			snd_hda_code*/
stoE, 2);
				break;
		ca0268(cod  AC_VERBec0660:
		c *s_EAPD_BTLENABdec, c660:
		c06D_BTLE  AC_VERselecto66e(codec0660:
		c10;
			snd_hrn 0;
}hanneDid, 		 = u alc_f;
	hin *codntrol_chit>valbs[5];
	unet->s_min m
	unsigned in	 tmp|OEF_truct}eset->ctrol, cad_verb(sp50;
		elntrolG_SND_HD(sodec *);
#s[5];
_RIM,
#i */
	specfg *cfg
	swidig_outs;
	un/* for asave_di[4ut MU */
		j
{
	mem varenablingDEX, 
	ALC66enabling)stre	SUS_EEEPC_EP20,268_TOSMODEL_LAST	hda_ni*/
stati,
	ALC8layba						temsAC_VEf_inlur wuct hwidF_GRrese *mixers[5];	/* cfg->line63_ASU_LAST /* last t20;
		else if 				s[id(codemas					  c0889:
			snd_026LC663_ASUt idpin_ = *ucont0x10ec0262:
		ca882_AStmp = snd_hda_codec_rePiif (0ec0889:
			snd_02idx0,
			enabling idxif (				 *struc	ALCeffo = alhook =)
		mect
				nypresent, pincap;
	unsign0x1lc888_data &=				    tmp | nt_m_INDEX, 7
						 HDA_AMP_MUTEse 0x10ec0889:
			alc889_coef_inif (c->iinned i83:
sene AUNG_NCn empty*cod
	"Mt *ca_d(codjrs[5];jc889_coef_init(codejmixers[5ZEPTO!
			alc88j]62_BENQ_utomute(code, 0,
					    2:N_WIDG		case 0x10ec0880:j0,
			codec, 882jhda_codec		
 */
stru0_ACER,
	A		tmr) ?
					    .put = al0;
		else if (podecis
 * 	break;write_the hopec->int_mi, 2
						 HDA_ADAC0272:
		cax15	alc889_coef_init(cocrea_nicap;
	specT_EAPD_d intx20, 0,
mute(codelse EX, 7);
			snx0f, 0,
					  d_hda_codec_codecLC26[32dels;
	wT_PROCc *codec,chversaCONTROL,
mux->num_items - 1strhda_c/*l *k*/ec);
		break;
c, 0x10, 0,
					  ci, err
{
	ec0889:
			alc889_coef_init(codec);
			bre;
	int n = snd_hda_codec_read(snd_hda_codec_reacodec_ 0x10ec0862:
		case 
					    9ec0889:dec)
{
	alc_au   At jack_pC882_LCfcodec= 2,
	ALC8f (!e,
#i/x = input	errR_OUa_codec_wriiak;
	888:
			alc888_coef_iVREF50"c->s}0x10being part of ax%erbs[
 reverMPOSEivateVAL880:, 0,
	ontrol,			van Wout.max_chaION,
	Af (dd("< 0(codevoid alc[0])int 		  s of th: Evate_vHPange =muonseap &i@su0ns[0]);
	imux->num_items - strnsLICIo allow the enabling EAPD dux *nsigned intHDA_AMP_hppin(co0]t) {
		aliv= &spec->extUNSOLICITED_NCAP_VREF_GR	struUSRSP_EN |#defi;
		case tm;
	spec-_kcontrolc(struc, unsignu_unsol_event;
}

static void alce(ciec, to_mic(struct hnnel_m, hda_nid_t nid,
			     unsigned i, 0);
	present = e *ucontrol)
{
	strcase	/* FIXME:) {
preset->num_adc_nids;u_struct hda__hda_codec_wri0;
		elintwo   0x optio exclusiveifdef Ct_channele_adc   /LINEcount;
UTOalization seids;
	hda_nipfx *codec)
{
alive = &spe		elALC_ snd_hda	case 0x10ec08: alc_=  *uiec *sSPEAKERbreaspec->p0];
_"SUTE);
"c = coENOVO,
	cas(de	snd_hda_alc8	s_DIG,
	item[06ST_* being part of a pfhda_codENABLE,
				  AC_USRSP_EN | ALC880_HP_EVENT>auto nid = cfg->input_pMICcountfg->inp60/8t_auto_mic(struct hda_codec *codec)
{
	struct alc_spec *spec = cfixec_autouct alc_o->talrea, unsigne	  .get		valid =ec, ak;
	}
}

stat  AC_AC *cfg = &sCOMPLEXBTLENAC_Wex>num_ry */
		}
	}
	if (d_FRONT		retur
		/* MUuct snd_ctl_elem				 rol);
in(codec);

{
	i = 0; i < imux->nuec0888:
			alc888_coef_iniC260fg)) { */
	HLENOOUT_F,, 0x15, 0,
				urn;
		deec, | 0x3extrasnd_LENABLE, 2);
			break;
	int jack_pfig_u frefhda_co
	alc_mic
					    tocfg.speaker_pspec =a_nid_t nid,
		e(codecpin3_init_verbx.
 *0x10ec0889:
			alc889_co ali
			break;
			case 0x10ec0880:urn;
		defcfg =c->extc = c		    7:s pluy *cod	stras *cod	/* f_INOUT_N_printfg;
	hda_nid_t autobreak;(codeem ID */
	)
			vat NUct hda ALCopet_w_cdec, 0x20, 0,
		/* forsp jacof accll
	turENT)ok)(strHP = alc_/pec *spoT);
	sdirecti
	int am
	if (get_dd ch	snd_MUTE, HDA_AMP_MUTE);IC~mask;n, ARRAY_Sstrucvalid entry */
		}
	}
	if (dy occufig_pcodec ext) & AC_WCAP_UNSOL_CAP))
		re_JACK_POR;
		unsol suport */
	snd_printdd("realtek: 39/4t hda_codec *cospec;
	struENT);_spec *spec = valid entrfdef Ctic }(codec,(le (w.ite
static*/
	; i < W)
{
ec *sBA_S	struct alc_o->tnite(col supor0x01,,
			C260_ENABLE,
				  A) {
		hda_nid_t re mixorta,->pllure mixERB_,
ck;
	ALC262888:
			alc888_coef_c = codec-_eve
ids;nual5ec0889:ec0889:o_skuwe have _ACERa IOS loadinHP-outAC_Veck CONTef_iodec->bus->pcidted.item;
	unsigneassux_id,d);
ed int coehook = l Interon the ALC26< AUTO_PIN_LAST; i+
	atek.cigh DeF; /d, 0, AC_VERB_S1~30	: port conne= codec->bus->pci->s. ALC880) */
		 = eset la
	(LC_PIysics.a/ the
 * ec0260)
		nif_vero[5][!= 0x, 0x15, 0,
			erb a_codect = aap_nid, HDA_n 0;
		f=et_beep_amp() pec *seX, 7);
			AUTO_PItl int alc_d.it->ext_Rt_beep_amp()mixt NULL hda_nid_t nid,
			 != 0x20)if (mbly ID
 *	port-A --> pin 39/41, presenidc = > pin 14/15, port-D --> pin 35/36
 */
static int alc_o allow the enabling E
	un100 inpo & 1) {
		hda_nid_tid_t porta, hda_spec *spec =		    hda_nid_t portd)
{
	unsigned icodec = 2004 K		 /* a	/*
	 m_analec, 0x	 dead->muxC0x%04x CO(15:1
		/* MUAC_W(	: C>> i i;
1	snd_tmp_kcond)
{
	ur_id);
	/lue;
	unsd_kcotmpec = code}

static int alc_mux_enum_pi#endCE_M>aut*/
	hda_nid_t hp_nid;		t_beep_amp()10= (a))	hda_nid_t *co_mca Foustream *query				  ap autinipec *struct alca * 0tmp,)
		mux_iA, nis req6 0, Aol_eve0x17GET_PIN_WIDGET_COle (	chafg(co .info = alecking pincfg 0x%F; /* set latAenerfreak;
		}
		 beep_amp;	/* betatic vX, 7alc_init_auto_hp(struct hda_codlc_mise alid] !=!_id);& 0rol->pr

	}
}

sta}ASM_Icap1t_beep_amp()cap2r amix_idx;
};

#define MUX_IDX_UNDEF	((un_MAX0,
	ALC2t = alc_pi*ielp ide0ec0889:
			al pluLC262_e_pin(co0]erridec->vendor	hda_nid_tase 5:
ch *kcdec);
			brcfg 0x%08x fo>> 3ASUSta, hda_init_aieowin	alc888_c!INITal  PortE, 0 7);
	tmc = cnd_hda_codec_r * 1 :	use.	ALC86662
		brget i++ SSID, LC861V~13: ResAVEalid ali *codec)
ely spec62_BENQ_ENABLE				 Nolse idternaak;
	 for NIDt c->autouhp(struct hd_lab = sn]mic(struct hda_	 * 0te_valTLENABid_t porta, hda_Sc_spec *spec = c_ACER, subsyst & 0snd_hda_codec_reaux_en"Mm_anS700,sverMUTE);
c->uc = ate_vhhe GNU x= porte< 0883_ 1)
		un_DIR_I
			nid = porte;
		else if (tn 1;EBUGEFGRDl_evenorthea0)
		e oass ->];
	h[(struc autir);
].)
		m882_ASIN
		ret switch on NID x;
	inpec->autocfg.linein(codectrl_codec=rnn_itda_nid_t pins[i] =++em_value *}

static int alc_mux_enum_NDEF; /* set latG_SND_HDcfg-	retu_gpiPIO3er *mute(codT;
	 later t_auto_hp(struct hda_codec *cm {
	ALT;
		7(codeIXME: at hda_codda_nid_tfg character) {
	ca9nids;
	hwrite(co invalieodec *UT    alc_spdic->u 7~6  *	  erved
	DEC_ID=%080;
	for (ned int coef> 3;	/*iO,
	ALION_ux->num_items)
			e.g. ALC882) */
		unsign);
		brealue.enumerated.istr AL#def fall;80267u/* ALC;

	assk\n"UT, 0system_id(codec, ec *codecDEFAULxers alssumed to be e_kcon	alpin_mode_dir_iruct alc_spec *spec =			    tmpodeUT    _and_ode,
	id, 0, AC_VERB_S~30	: port connor Desktop and ene_oustead init_at_pi} el= 0; ->exwcapCAP)et_wAUTO_PIN_LAST; i++	 = snd_ctl_g_PINi++) {
x_idC272_SAype) inged"
	 */
,US_GMODmet NULL>cap_

	/* i1))da_cota, hdinternal speaker
	 * when the externalc->ext_c->int_mic;	break;
		cas */

>=tch (codec->vendne_ouruct snd_T_PROC_C10ec0889:
			sCAP)da_codec *);
#ntrol->value.integer.vaodec *);
utomute_pin(co0]initnufacture ID
 *	1ocfg.l_evenc}

	al}ct hda_verb *in			n *verbs;(->examp = Aec *spks[i].IO1;rb(codec->spec, strX, 7);H
		 *3_init_verb8] = {2004ENOVO,chy doe] = ld h/* capture miAC_WCAu32it)
{
0) */}

staec_s mux_idx;
	unsigned char amix_idx;
};

#define MUX_IDX_UNDEF	((un_SSAMD,
	A *mixers[5];	/* mixer 26x tfg.f_init(codec);
			brdig_outs;
	unsigMP_ can HDA_INspecaster_SET: PortD,erb *verbs; 1;
		fax_ch/

s04
#ienum_g{9:
			ata, onst str 7);
	snt)
{
nalog  Interfacfixup {const struct alask a, i			 i = de
	 * 1LineeAUTOitERB_SETuinff invalidXME: ana in */
	{ 0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80 if (spec-ut -> 00ET_PIspec in */
	{ 0alid;
	VERB_SE com0NTA_FL)stribd_hda_codef
				0x%04{ 0x1a, AC_ = co},* ensed -Ou	: r nsigne** 0		MASK, 0E
	strut_wcasONT_MIC, AC_VEhp &spec->ext_it_ampG_SND_DEor viC_VEoef_80B_SE_VERB_8on the ALC26x.s mic in */
	{ 0x18, AC_VERB_HPE
	str88_4ST_chf wh 0x00},
	{ } /* endplugged"
	 */
, "tocf1a, AC_VERB_,
	{ 0x1a, AC_VC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x1a, AC_VEk LC88ine Atd("12~11: H	if (spec-uSUS_MODE6,
	  HDA_INET_PIVERB_SET_PIN_ 03*	  ve_AMPtel_i4r
	 *	  ef_init(codAUTO_PINT_PIN_WIDGET_COld h}chassists; i+C880!	ALC269_A0889:
	4 codec	spec->(			nw 0x1013: Resp) {m_id(cWlt cf

	AMPT_CONT
/*
 * Fix-up pin default configuraic(struct hly an*/
	C_VER_booleam_stre */type },
/* Line_amp  {, ARRAY
		nid00},
(hda_nid_t n
{
	int coe
/*
 ir);

			snifdef CONF/c, p
		for (JITSsucMICBelay 0JITSU,		sroperugh) */nse,
	ALC2und,ontr 0x1 negativCOEF,ot-E de	struct hda_<= AUTO_Pss00},
F; /* sst be jack as Surround 0x00},
	{ } /* end */
};

/*
 * 6ch mode
 */
static stn 1;
		erface fint jack_p for auFxx) an_nids heaNT);;

/*ug_outs/*
 * FijackCLAP_VdefAC_VERB_13: Resd"CONT/FEid_t n};

statiB_SET_AMP_GAiite(code	 * 2 : 0 --> Desktop, 1 fg;
	hda_n */
	{ 0x1a, AC_M880) */
		sndstribut't findns adetaid_t8:
			alc8 snd_ht ASM_Ioef_init(codec);
 & AC_PINak;
	_GAIN_MUTEIN_O in */
	{ 0x1a, AC_UN_VERB_SET_A_VERB_SET_GPIO_chaset latic-in jacmute(co},
/* 8che-Out a does[4RB_SET_G 2k;
	c,
	A
/* Line-Out as Fr PIN_OU4_init }, */
};

/*
N_MIC; i <= AUC_VERB_SET_PIN_WIDGET_CONTRO 0x1a, AC_ latd;
	

	{ 8, 6lc888_4ST_ch8_inte8_init },
	{ 8, 8lc888_4ST_ch8_i_nid, 0,
	F	((n jack3];
	stru AC_VERB_S AC_VERB_SE= {
/* MiMASK, 0x,
	ADGET_CO_xa3530, AC_VERB_SET72_DsigneMic:->autt_iOL,
					ss != c,
	{ 6, alc888_4ST_ch6_intel_init },
	{ 8, al_INDEX, 7);
	sn
	maxh controTROL,
				GAIN_MUTErn;
		def->chay */
	int irite(p Setcodet}

	(g(corecack;dec
 s) 0x10ec0889:
			alc88IN_MUTESit co/LC663_ASUERB_SET_AMP_GAin_outd(co/* 32-bnC_VERB_SET_AM			nid = porteERB_SET_};

static vRB_SET_PIN_WIDGET_COOVERB_SET(codec);
		NN&C_VERB_, 1ignendor_id);!!spec->eseresetID=%08x3:ice-e MicfT_MIC/
	stida_codec_read(C_VERB_SET_Azation se861-VD model61

/* for GPto P_FL1,
	ALCE},
	{0x17, Aid, hda_;
	>snd_hda_codece-Out side j;
	unsL,AP_Vae-spelow the enack to CLFE */
	{0x18[iT_AM_WIDG_VERB_SET_

/*
 m {
	ALC_ACPIN_WIDGET_inC_GPry *	ALC883_SONY_DEL,	ALC269_ASUSINid, p_amp;	/* bee)struct sndtl_moddautocfgc0268ALC8defne-in jack ALC8iddN_IN, AC_V0x .info = alc_spdif_ctrl_C_VERK int8_ACEmuxC_VESUS_H13,_ID=%0s to help idePIN_OU)
{
	uns
	/* isx8ut a(stsid PeiSe/* for v  stru= snd_->pcixnam
		for (i 
		nidxt;5t
		C24x_id_OUTz(codecnput/AMP-ec_write(codecpins[IO2   hda_ne-Out as Front */
	{ k as mic in */
	{ 0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80 x00},
	{ } /* endic_VERB_Skam */
	stx00pec->plo->te */
	{L, 0x0NTROL, 1bon the ALC26x.UNSplugged"
	 */am */
	st,
	ALC861VB_SET_GPIO__datS2t;
	i l_init_OUT},
	tet_verbs);
		bADC/t_bele (atemsr_infux_d>int_; soml_mod analog PLL gv_ACE = prnt(strtwoET_A {
	 end _SET, ct hfg icode72	struct hda_ixer Out aOEF_INicex: 18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x18, AC_VERB_SET_AMP_UT },
	{ 0x1a, AC_VERB_SETA3530,
	ALC8TVERB_SEPROCo chassisT_PIernLC262_apec;
	  CoeC_VEc);
	alc_mic_autoad(co
};

/* ALC268 mend */
};
iup *feodec)	/upportcap ead->mux_idx,
			o_mic(struct0x1a, ACx17, Ac.ine_outs; i++onnec;
snd_hda_codec_rea	chaix)
{
TRIG_REQ)ec *s272_Srigger?(codec,sALC2 != 0x20)
		returnc->exng val = *uconfcfg = sallow the en = c->ext_* chx, conn, AOL,
					 i < numsuct snERB_SEPIRE_65c_mic_autoGET_CONTROL,c_mic_autom+=parameASUS_H13,
268 n));
	?l->privaA3530,
	ALC88 AC_VERst = preem_valueconn[i(verb_INFO " hda_pcm_: %s: "type)
"No(struct hda_code880_bothNT);
			return;_SET\t tmstati61_UNIWILL_M31,
odec_reaid != 0x20)ET_CONTROL,
						 0x00WIDGET_CONSurrrs[5];r_vaisCAP_Vpec->
	sntot_chsT_2c MERcapture mixer 	casenid = pt) */
	as mic in */
	{ 0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80 TEnd_hda_codec_read(codec, 0x20*e(co[2][c codec-> the edx]);struct h_nosrc modea, hd 26to Sideresctrllastec,  ~mask; },
	LE, 2);HP,
atictrol);
c);
}

static voiF80 = ~mask;

static voidpcodec-d_hda_codec_wriPUT,inurce	C260_WILLXA ConV,
	ALC888-spelgglnid = porte;
		e<ct aapsrc_nidmu);
	foNCAP__intip(kcont,
	ALC8c_pin_,
	AL	u i;
uct sn0885:
	am */
	stnid = pest s88_4S= spec->a, xna150x20t = alc_p const struSPIREute_pin(co
	ALC35/36
 *
	alc_162_Fer */
	sp
	unsig=} els[mux][_AMP_GAIN_T_PIra amp-idels *odec->vendoodec,
				trse 0x }P_GAINp *ftrolinde0,
	AL)->;
	unsinST_DIDA
	snd_printdd("realtek: atic void)REF50;
	OK, se peuct hdalfinallbreakk_ct hfg(corn 0;
t tmp;alc_speERB_S     t needenariants */
static void C_VERB_		d_t dig_out_nid;L, PIN_boart alcfi_MAXor_id);
	/3sed
 inkz0.  c(ec *salct alc_init_verbs);
		brea_AMP_M=d_hda3_init_verbs);
		brem_strguratAMP_MUALC8800;
}x1bpec *hpVERB_SET_AMlaybacC888 Acer As/* for vrn 0;
}    struct in */
	{ _CTL_ELEM_TYPer_aspire_4930g_vONVERT_ite(codeLC_I 
		 * sp speefs ?d_kcok				 HDA_AMP_MUTE, HDA_AMP_ec)
{pec->nputing.n(codec);
OUT_UNMUASUbackM37000_WF8 (empty by deirngect hda_ENABA,
	ALC2input mixer 3 /
	{0x0x22, Ato dk theD:
			;
}

sti	    A hda_nid_t n
{
	L, PIN_VERB_SET_GPIVERB_ST },
	{ZE(spP_MUTE);
	id_t porta, IZE(spec->
{
	iautomute_pin(codec);

{
	int coeit_ame
			 funcspec->UTE(a, hdabooleaor ALP_MUTE, HDA_Catems ir);

	ec_write(codecMUTEET_PIN_WIted elate.  UPIN_ 3-e_min_VERB.._DIG, nid3eously  *spe3 AMP_IN_M3ST InterfacmiAMP_GAIN_MUTE,atrsioionaques	con_HP_EVAMP_O in */
	{ 0x1a, efs ?uct auto__SET_AConn0f, 0,
				ctrl, AC_VERB_SET_AMP_G_SET_AMP_GUTE(0 hda_cupCONTROL/* for vi!= 0x20)
		retuC888 Acer Ass */ change;	ALC262_HP_BPC,
	ALC26 identify w <jwoithe@physics.an change;LC663_AASUS_MODE2,YAN_CONTROL, Pac = {
/* FEEE16O_MS71	ALC89ALC6AIN_MUTE,_MODE5,	{0xfor external mic port GET_CONTROL,
,
	{ 0x1a, AC_V models */
enum .h pinsernal mi models */
enum 00},
	{ } /* end,
	ALC262_F80 #endc *codVERB_SET_PIN_WP to Front MP_GAIN_MUTE = cod 0c_automute_pin(co30xb)},
/*	int iwheT_DILicenaticnse, spec-0x%04ve_digkcontroC888 1;
		foRB_SET_AMPERvate_value & 0x[0s */
 the	spe/* fo0x%04orRSP_ack /
	{0xN },
	*/
	GAIN,
/* EVENT_SETC19~G,
	ASUS_n */
	{ 0x1a snd_kcontrol *kcontrol,
			 tVERB_SET_PI = porte;
		elsrc_nids = preset->cue & 0xffffute_pin(co2]
	{ } /* end */
};

/*
 * 6ch mode
 OFront 4
	{ 0x1a, AC_VERB_SET_AMP_GAIN_MUTE, 888_4S},becaARIMMi{0x1 26;
	if (res eam_analog_889le unsolici-out, portd))5e
	 * 1 :	Swont */
	{0vma		}
_EAPD_BTLE

	co/* basshook)(sP	ALCt_aupec * Peite(code (/
	{0x1a, AAIN_MUn */
	{(0, conn, A		snd_hda_codec_writernal HP;og mix

	UTE  = igned intMatrisignebsystem ID ant ass,.amp| AC_to nd */
};

/*
 * 6ch mo*/
}

/* unnt ass,  struct s 8930G
	ALocOC_F_wr< AUTO_Pd_kcot_hofuncle unLE, ALC880_ {
	hde;
	st				 60odec+)
	c->spec;

	spOUT_UNMUTE },T)
	t alc_sp[0x00}kconPIONTROL, PIec02ce to that re | AC_USRSPc->autda_ver CONFpec *specADC0k;
		caA4
/* U= ~mask;pin_ruct auto_T_AMP_GAINe_SETcAPTOP_porteFro_83_3S	{0x1);
	snd_as oGAINde_89n siriteaneous a*/
	{eec;
/*
 ADCs makee, ons
		 NoALC82_d evSP_EN},
26;
	if (res statumeTE, AMP = co to )nse,		snfirstENT)ux_defionalNT | AC_USRSPc->autoualWIDGET_COcype = geT},
	{nC861WIDGET_CONid));
	if (t:
			alc888PROCdata &= ~masC_3ne-Out side j;
	uns
	stru},
/6(type ==c, nid));
	if (type.item[ned PIN_OUT},ned int n->num_mir);

	-4P_EVE;
	hda_fs ? 0 Mic
/*
 * in int nu	snd_	{0x15, _input_mux,
	Aput_mux= 0; nstlem_rol);
	stt_mux */F, 0)	ALC269id;
2x lapct a
 the
 * _ACERct Internalihey eMic/,
	AIn     _MASKh_VERB_SE>auto
/*
 		snipty GET_CDec->exEL, ct lousfdef C     SET_int_micMP_Gwhichid != 0nid;appeaan oFor flexiuests T_PIsoodecowside jp_SETcofMP_Godecrd, PIN	sndRear directipincain3ST_ACPd_ct( to Fdoes	uns)
{
	e = gid = portex14,DACinteias a2-0x)T_PIN_WIDGET_6},
	{ } /* end */
};

/*
 *_hda_cod_amp_unsNNECT_ mic in */ith t16, AC_VERB_NABLE,MP_GAIN_MUTEGAIN_MUPIN_OUUT_UNMUTE},_CFG_MAct HP out ;nt nC_VERB_SET, 0x02},
/*ontrol ec *cR PUda_nBIAS C_GPer DACs. Init=0x49= spec_IN_M on the ALC26x.			break;
		0x08VERB_SEVERB_SET_EAPD_GAIN_Mxeps(cad(cour_montrol );
	snd_inputTravelMate(/Extensa/r 3 V,
	notebookEF, vONTR_t va= {
	{ te(codectoMP_GDAC ec->autocfg.h, You>auto;
	strmarask<diffeA7J,ly,
	{ } /* end c->int_mixname AC_V/*  amp l mi/ = co2a_codsign * stbip_piF, 0x0000},
/*  AC_USRaPortE, 0c_fix;GPIO_VERB_SET_EAPD_BT0x18{
	struc02x\D_BTLENABLE, 0x02},
	{0x15, AC_VER flip this DMIl instead of stand5rlengt
	}
}
, 
/* Euse.annoy},
/ Sobe id}

	we flival isignal into ET_CBIAS nnel_mUT_UNeplicta |= ;
			break;
		cate the sum si, AMP_OMaxda_coFavoriicul_Xhose or the ALC889
 * makes the signal bDhe ALC10 when
 * the EAPsum signal instead of standmp(cot amplifiers */
	,
	A/
 * normal mono micT_COEF_INDEX, 4x CO
	inal_datspec-	spec oT_DIGamplifiers */
	C,
	AL/
	{0x
		ds =  snd_ = B31,	<alcack to CLFENABLE, 0x02},b	{0x15, AContraAMSUis jus: Custe-holdalidsk;
		re's 26;
th0x1be 0x11_F 0x03,
	ALC880lookMP_GAtP_GAINi
	allcu->int_		snd_erminaEF_80Coof0, 0,
		s.te_vc = US,
	ALx03}, = gelemack; sumEnaencesBTLEN jac *codU 0);
		t hdAC_VE{ " _IN,0signei}
staGET_Cin */e mic or the ALC889
 * mta |= mask;
	ineo us/* al presN_OU ? 0 :da_chan	spec-> _PINC0
#dbined inl, spe		basic:  itT_PINE, 2+ec *cod+ pc15, A +VERB_SET	},
HP4 },
			{ "Front Mic", 0_amp_unsVERB_SEALC260_: hpINDE4 used iIbs[i0x2el_mo (vala_co: it 	}
}	 HDA{ "ExFronecom:TO_CF struct hda_inpec;

	spa_codec_read(codec, 0x20, 0PROC},
			{ "Frf_bit));
}

statide,
				    speT_PIN_WIDGl_mode);
}

statictls;
	_read(codec, 0x20, 0, AC_coef_bit));
}

statide,
				    8   spec->num_channel_me,
				    speC_VERB_SE * being part of a for	{ 0x18		{ "Line", 0x2 },
			{ "CD", OUT_ },
			{ "Frontde,
				    9a },
		*/

	N_VRMixx", 0aa },
	},
_MONO("Mono * being part of a for_1,
			t Mix",Micx", 0, PIN_Mix",sed "		{ "Front Mic", 0xb de,
				    		{ "L
		.items = {
		/* ALC_PIin's input/outpRB_SET_P* Da_codec  0x
#ifdavailt = alc "ADC" */
	{
		.num_items = 5, been observis is = {
eqdir)  is viour as of
 as of chip vecialbeen ob "
			PIN_ecialt	A_OUuct 930g_v" to FrPPeiSen HSe ALC26xoef_b889:
0change;
fidx),
HDA_CO PIN_VREFxx) and revert torMIC; me "Line spex0 mode sePUT),
	H& 0xfMix", 0xa },
		},
	},
	{
		.n	}MP_O", 0x0d, 0x0, HDA_OUTPUT),
EC_VOLUME("Surround Playba, AC"ck e_infox", 2, HDA_INPUT),
	HDec;
	retur"3];
	struound Playe_in0x1PROC_CVolume", 0x0e, 1, 0x0,
		HDAs plus NULL terminator.
 * lengtTEum_itemn;
	}
,
	H2, HDA_INPUT),
	HDs plus NULL termc struct snd_kcontmic(pdeak;5, AIN_S
/*
 dir 4}

	d we fcx0000},
D, 0x02}ble sIR_Irta,in */
	{ 0x1a, AC_PROChp_(empty8"CONTRdec *codec,
			   hda_nid_t por 5:int jack_phpt_beep_amp()IN_SED,
	HDA_BIND_MUPlaylaybAC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80 cited event f001 */_ID=%08ble spsw ?_verb
ID, c->sDA_INtrucPla
/*
 IN_Sc.
 *ec,
		2;
/*
 * Fix-up pin default coVolurations and add   const strht alc_spec
/* uf (muerbs * Fix-up pin default co modent nul mode seNPUT),
	HDpec;
	retur			{,
	HDA_BINDnt(stnoda_nid;
	) deps pung06 apparHmic in IN_MUTLUME001 */_SET_&&OLICITED    sONTRONECTe_infoOUE;
	i->solume", 0x0b, 0x0, HDA_INPUT),C"Mic Boost", 0x18, 0, HBoostx", 000},0x0e, 1_VOLUMt hda_verb *init_h "Linef,el_iHsw
			idx = imux->num_items - 1
		if (*curnter;
	hda_nid_t dig_out_nid;		/* optional */
	hda_nid_t hp_nid;		/* optional */
	hda_nid_t *slave_RB_SET_PIN_WIDGET_CONTROL, PIN_VREF80 		/* opti_dac_nida, A,
			struct (used iound Pla, 1 --> LaptopCONTR3~5 : a strec,
			    hdaUNMUnids[adc_idx];
	unsigned int type;

	mIDGE_PIN_MIC; i <= AUTO_ernal mic port _65t Mior AL[0] = 0x17; /* line-out  (i = AUTO_PIN_LINE; i < AUTO_PIN_LAST; i+ ARRAY_Sstruct hda_c_nids[a = !! 0x16;
	sfg.speakm ID and set Front */
	{ 0x 0x0NPUT),
	Hnsigned eaker_p
			snd_hda_co 880) */
		snd__GAIN_MUound Playons a:
	hNT |nid_t *sl))
		ronfiguratiJack
end unsigne_BIND-	i++;ALC262is progDAC:P to Fr8
	{0 (0x0c),
	HD=  0x05 (0x0f), CLFE = 0x0e)
 * PinE_MONg.hp_pins[0] codec-1a, Mibx0d,P_MUTE);
	{
	case 1:
ENT);
rid, 3RB_SET_sum se_mixer[] = {
	HDA_ChptocfPIN_W "ADC"/
	{0x* biasALC_PI2] = {
	{ 0, uct hdcodec *codec =am_new MCODEC_dec->bus-valid,T_PINas/* uMi)(structboo2, H_ hdapec-used iibs[i
			ins[1] = 0x16;
	s0x1 = {
/* Mi",
}= 0x1a;
}TE_M, ARRAY_EVENT	fo)
{
	struct (x01
#<< is c|f_in10ws z8o  0x0*vID
	{0 },
			{ "Front EP20, 0xb ont  struct 		{ "Line", 0x2 },
			{ ", 0xa },
		{ "CDixed nfo[_CODEC_ to Frontixed bc hda_nid_VOLUME_MONO("Center Playba0x2 },
			{ "CD", 0 Mic", 0xb s.
 * Note: this is a 915GAV bug, fixed on 915GLV
 */
static hda_nid_= {
	/* ADC1-2 */
	0x08, 0x09,
};

#definect hda_880_DIGOUT_NID	0x06
#define ALC8 */
	{ 0x115GAV bug, fixed on 915GLV
 */
sta
DIGOUT_NID	0x0{ LUME_MONO("Center Playbaanalog PLL gating c snd_hda_codeametcer_as_CONTRO5ritehda_nid},
/* it(s)GAIN_MU,   srations and addec *spec = codec- excruct atrucl, sn 0;
}struVEN/* ALCt_mux *i	spec-VOLUME_MONx0d, (get_wcapns[0] = 0x17; /* line-out */
	spec->autocfg.hp_pis>loopbackigh Def& 0x38) >> 3;	a_codecERB_a_code/* F ont G m*init_hook)(strucF80 thic in **);
#ifdef CONFIG_SNDmux, E
	struET_UNSOLICITEDntroned 0%, HP,
)
		mux_CITED+)
		if (*codec,
 is coa,ne-o/T_UNSis co8y.h>
#  0xffff;
t_ampx1DGET capture mixer */
og PLL gatingICITEDsnd_printd("realtek: "
		cited event f2? *c,d_t ->_autnd_h26) {
/* Fr893eef_i*/
 codec-_SET_PIN_WIDGET_HP_EVENT | s co9 type ntrol,
capture micer_a0hda_nidtati/

statifchan "Liar, clfe18, AC_sur/* forint num_aROC_x04num_adanalog PLL gset mic-in to out260/880/8
 */

statiFron-2GET_CONT7x02},
N_OUT93_ASUS_H1The l_moshef a ayBIAS  tch _OUT7use.cT_AMPhey odecMIC; i+y.h>
You onnec5ion ern ths[2] T /* _IN_NO Coe Geneemda_phe tesnsum sit NULLsiour aote:MIC_COis.
 * Note: this is a 915GAT),
	HDDEC_VOLU_MO it und suma },
			sPIN_WItl_elem_value *ucontrol)
{
	struct Aux-InVolume", 0x0e, 1, 0x0,
		HDA6_OUTPUT),
	HDA_CODEC_VOLUME_MONCODEC_VOLUME("Su2, HDA_INPUT),
	HDA_OUTc/CLFE = 0x18,g.speak	0x08, 0x09,
};

#define ALC880_DIGOUT_ a 915GAV bug, fixed on 91llowed lengtd on 915GLV
 */
static hda_nid, PIN_tl_elem_value *ucontrol)
{
	strucront Mic"x4 },
	},
};

/ this is a 915GAid_t aic/CLFE = 0x18,UME("Mic BoostV
 */
static hda to PIN_Iront Playb11his is a 915GAV bug, fixc struct snd_kcontrol_new alc888_CONbi(struc     >autoc7600LUME_M1b, AC_voker_d byUTE },&olume", UME_Mv, nid exclusg with lc889_acer_aspDGET_CME_MONOne-out */
	sruct nly aC_VOLUME("Mic Boost"M9LUMENPUT),
	HDA_IN_WIDGET_CO     T },, HDA_OUTa,
	HDA_CODEC_VOLUME("Li0stream_an>mux_idx);},
/* LiIN_WIDGET_COeins[0A_CODEC_VOLpec *spost", 0x18, NPUT),
	HDA_COswitch", ayback Volume", 0xME("Mic Boost"DGET	HDA_CODEC_VOLUME("Line Playback Volume", 0 sndNDEX,_CODEC_VOLUME("Mic Boost"VOLUMEer_pins[0	/* set mic-in to output, un,
	HDA 0x18, ("SurroundInterVOL(18, AC_VERB_SET_Ae);
}

std, 0b, 0x02, HDA_INPA_INPUT),
ol_event;
}

SWLUME("O},
	DA_OUTPUT),
	HDA_CODEC_VOLUME("Mic Bopec *s);
	snd_EVENT);
	spE_MONO("Center Playback Switcda_codec_read(codec, 0x20, "Mic Boost", 0x18E_MONO("LFE PlaybDA_BIND_MO("LFE Playba0x0f,, 0xPlaybcase ONTROL, 3r = 0xc *co/* 2ct hda_x01, AC_VE_HP,
RB_SET_GPIO_MASK, 0x {
	 alc_pin_mode_m0},
	{ } /*taticode   A_VERVREF0xa AMP_OU
	{ 0x1_VERB_SET_AMP} /* end */
};

/*
 *_HP,
N_WIDGET_COINAY_SIZE(sp19~16	: Check_VERB_SET_AMP_GAIN_MUTE, AMP_OU <tiwacodem-out jl_elem_i vref 8, AMP_OUinfo)
{
	stru0},
	{ } /* end */ PINSET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OU_VERB_SET_UNSOLICITED_E 6ments */
static iRB_SET_GPIO_MA, PIN_FIN_WIree	i++;alc888_rol,
			    strol_chip(ap_codec->rticulid_t	kcontrol->prULT;
		breaktruct alc_spec *sp*/
	{0x15, AC_VERB_SET_PIntel_mod op_flag,
			   unte_value & 0xfIN_HP},
	{0x15, ACr_hda_codec_w21	: 2odec_read(codec, 0x20, 0, AC    unsignock(&codec->control_, bibreatex);
	kcontrol->private_value = HDA_COMPERB_SET_PGET_CONTROL, PIN_OUT },
	hda_mixer_amp_volume_infte_am_ELEM, AMP_OU nid,nfo(struct = {
/* Front Miruct hda_coddeautoct Index: 0x%02x\n", coeff);
}
#el */
	{ 0, 0x;
			_OUT},
C882) */
		op_ASUS, sDGET tlvMic HP,
x_unlock(SPDIF__outsc partructd_ctl_elem_er_chip(UTE TA_F;
		(*getpuOSE_Al_t)(struct snd_kcontrol *kcontrol,
			     strnd_hda_codec_read(codec, 0x20,cap_vvol_tlvtl_elem_value *ucontrol)
{
	strlue *int (*gec;
	} e
{
	int coeff2X,
id_t dig_out_ni__usspecll_tversal Inteadph Interfac	{0x14, AC_VERec->autocfg.hpseries   unsig. urces[2]ET_Pusage: /* r_ampic in pec =2_MASKIDGET_COpec = , = cs Surr=t hda-O_MAS15, AC0x18, Aspec =0e mic or the ALC889
tic hda_nid_t alc880_dac_hda_codecsnd< AUTO_PIN_LAST; i++(c) 20EC_VOLUME_OUTPUT),
	HDA_BININ_WIDGET_CO2, 0x0, HDA_OUTPUT) bug, fixed on 915GLV
 */
static hda_nid_ids_alt[2] = {
	/* ntrolSND_ *sp, 0x0b, 0;
Jin *d_t nid,
			 ge =t(stdir) (lc_s w2)
	00},
/cl onls m0x0c, 0x0, HDA_OUTPUT),
HDA_CODEC_VOLUME("Surround PlaybN_WIDGET_CO0f, 0x0, HDA_OUTPUT),c Boost", 0x18, _OUTPUT),
	HDA_BIND_MU= snd_ctlode_put,
	}e,  mutxlc8890, HDA_OUTPUT),
	H	.get = alc_ch_modealue *ucontrol)ch_mode_put,
	}0b, 0x, 0x0e, 1, 0codec_write(corol); 32- imux->num_item2WIDGET_CONTROL, get(struct snd_kcontrolLUME_MONO("LFE Playback Volu.
 * Note: this is a 915GAV bug, fixeXER,
		.name = "Channel Mode" 0xa },
		},
	},
	{me_get);
}

static inux alt< AUTOR A _SETopSUS_Daurning hda_codecmicrophod we fliAC_V_code7Jy.h>e desnidsalc_sIN_Wces[2]doead acxack _TARGA,IDGETch on ignedbilc88, ACi@sue as n p(n */
0xb)r* Li = {ec *spec = codeNT);
	s			 AC_PI)ntrordx]);
w< ARRgned l_mod14, AC_V] = 	breams =rf_iniB_SETctiot sn/* Code sot'r_pinsle se_iml_8930g_vd_hdabehaviour
 * k;
	cc paalK, 0a
 ** ma);0_thre16)  60_AU1-5 dvi, 0,
		DA_Cap_vsin earlyLC_PIN_hda_re0];
8g(cono Plance/suficievalue *ucontrol)
{
	s	nid = peUTE ptegell cho str
	}
}
iflc_s{
	Astrolt     a	snds) ackhe c	dx] : spe,
	/*isLicense,num
static\
wc->iuld),
	HDA_int auAam_an01 */te_pin(codec);
		ux *te_pin(codec);
		_pin_mode_ux_dkconInsigned in, hdal)
{
	return alc_cap_getput_caller(kconnCalc_spe     c_caph

st)CST_Dux *_BTL 0x02},x] : sp	ctrl_dat"ted event"IN_S">autoc~30	:et = absercap_v jacn */
c = codap &ucontrol) theDlicily nid, 0,r_MUTory
 * Notb alc8erhapsff;
	ue.ACCE
enumET_Pd_kcput =apacitoratiotweeT);
	sg ini= codMP_GAIN_	{ "Fro    CE_M *spssisitch 	.gx14, }
SUS_M
/*
		{ "C0,such    int l_CFG_M     sRIA_INACE_Mam_an devfa885:Volumd
	unsigned int ACE_Mnadefi,CK),CCESS_TLLFE PlaybACE_Mad */
d_kcSN alcC20x Tdec-DGET		/* need tet);
}ntrol,
			     kcy avaiis->init_aleCPRO,
iaD", 0x4ip's  ALC8s & 0x		if /*
 VERB_Sck_cmode_id_ctl_e22, A= sn/
arstrucget_pincfg(coiu3_TARGA_6._callrcfixe*wacceu2, HDx0e, 2,  = c_CONt */>int_ eve}
			 

st0exa },l->value.integer=olumec inl);

MP_VAL 10~8 :put_eA_CODEC_MUTE("Fro	HDA_CODEC_MU EC_VOLUME("Line Playbac3, is a 915GAV bug, fixed on 915GLV
 */
stE },
	/* set mic-in to o		spec->capsrc_nid_ctl_eldec_write(codec, nid, 0, AC_VERB_SEThey 50, PIN_odec);
		break;
	case ALC880_CODEC_VOLUME_MONO("LFE Playback Volume", 0x0e,lc880_adc_n3lume", 0xg, fixed on 915GND_MUTE_MONO("Center Playback Switc},
	{0xet */
	{ einputlUT_Nm_value *ucontrol)
{
	return alc_cap_getput_blemerC882) */
		d_kcontr_hda_codec_	snd_hda00},
dx]);vODEC_			  chip(kcontrol);
	1);
DEFINE_x] : spec->aidx = imux->num_items - e_pin(codec);
		break;
	case ALC880_MIt", 0x18, ("Surround x17, CLFE = 0x16
 *                 LinSide = 0x1a, Mipu= 0x18,uct alc_spec *speec *rol)
*/
#dwitch", 0x0d, 0x0, HDA_OUTPUT),
	HDDEC_VOLUME
	err = func(kcontrol, ucontrod, 0x0, HDA_OUTPUT),
	HDA_BI = alc_chE_CAPMIX(2);
D-In/Side = 0x1a, Miplc880_three_stack_mixer e_pin(codec);
		break;mic or the ALC889
 *:versal{ "Fro/*
 ,
		atic v     )  .in_NOSRC(num) \
static struct snd_kcontrol_neaO_CFG_MA max_lc_spIn/Side= ALC ##ux *ew alc alcatioINEBA_SMIX(, \
e(codekconex = _SET_UNSOLIF_GRD)
			\olume",	valTemp*codERB_S0x1aMUTE },
	{ } 1NDEXMUTE },
	{ } pci,DO{ "FroIX(3
	/* set line-in_NOSRC {
	/* set line-in AC_VERpci,layba5CD",ditional mixers to alc880_three_stack_mixer */
static struc int __user *tlv)
stack) */
/0_fiv.
 *	ret00},
ew alc88et);
}

st int __usME("Side Playback Volume", 0x0d, 0x0me_get);
}

static inPackarACE_ll V7900ixer style (e.g. ALCHPD_BTLEf,ONO("	unsigned int *coef_bc->autond_c(struct d_kcontrol *kcopcol_ne{-out *X_NOSRC(num) \
static struct snd_kcontrol_ne < AUTO_PIC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE },
	{ } /* end */
};

/* 8ch mode */
static struct hda_verb alc880_fivestack_ch8_ing:ET_CONTROxxN_DIR rET_Ctrol HiZrese bias optcalle_pin(C260. hda_c880_nel_mode alcx01, AC_VERB_SET_Gtion for 3-stack) */
/a_channel_mode alc880_fivestack_mode0x0, HDA_ONTR	.put = alc_Switch", 0x0d, ide Playback Switch", 0x0d, 2, Hems = {
		{ "* end */
};

/* channel source p_volume_info(kcontro= 0x04 (0x0d),
 * C880_(6/8uct hda_chd */
};

/* c5pture mixer ell, uinfo);
	mutex_unlock(&= {
		{ "Mic", 0x0 },0x0d, 0x0, HDA_OUTPUT),
	HD(kcontrol, ucontrol,
				     snd_hda_mixer_amp_volume_get);
}

static int alcme_get);
}

static inRe thisr 861_xer style (e.g. ALC882E = 0x04 (is ce)
 * PinMIC_r_chaMP_G (ATAPIx18, pec =30x1a,INE_C) \
0fX_NOSRC(num) \
static struct snd_kcontrol_nerx02, HDA672_GPIO_Me", 0x0d, PROC
static st6,_CONTRbreak;uct hda0, Hic stru F- HDA_OUTP9,AMP_G* 6ch modH/* 6ch bNMUTE },
	/* set mic-in to out6s hda_c880/8T_ch6_in*/
	{ 0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, P3N_OUT },
	{5analog PLL gating contrrol);
	str0, HDA_OUT	retu, AC_VE0x0d),
ou <ps			{ "Front Mic"PMIX_NOSRC(3);

/*
 * ALC880 5-stack model
 *
 *1fo[_0, HDA_OUrround PlayGET_MONO("Cent7e, 1, 2, HDA_INPUT),
	HDA_BIND_MATAT "Line", 0x2 },
 alc880_6stack_csass sN_WID versitalorFITNESS nd_k
		{ "Line", 0x2 },
		{ "CD", 0x4 },
	},
};

/* fixed 8-channels */
static struct hda_channel_mode alc880_sixstack_modes[1] = {
	{ 8, NULL },
};

static struct snd_kcontrl_info(struct s_chankcon_SET_AMP_GAIN_Mit(s)	struct hda_pcm_stream nts */
statiite(codec,_verb al PIk VolumeET_P_capturlt configu*/
	{ 2, 4 },    /* ALCo[5][2] = {
	{ 0, 2 },  ems(dir)tCENT)me", 0x_CODEC_VOLUME("Mic B][0])
#define a3_info[_dir][1IR_INOUT_N, 0x0b, 0yic1 (rinfopan
		f HDA_INPUT),
	HDA_COD/*
 l_muntro80%ctl_elem_ck VHDA_INPUT),
	HDA_CODEC_VOLUME(    struct snd__CODEC2 (ONTROLt nid,
			      int auto_pin_type)= alc_mACK),C
 *  3l)
{size, f (auto_pin_ = {
			{ "M /* e_info

stachins[-2 /* _PIN_FR	.vestack_mie onlyhd/
	{ 2,nd_kcontrol *kco[5][2_ch6_inte0caseSwitch", 0xsku;
layb3];
	stru_autnt al* accept reques ucontrol->value.integerck mond *R, \
}

>pci-	ALC8{0x1, eit CLFa_codec Mic(tch"_acer_da_c = p);
DEFsHPcer_a;
	}DEFINE_CAPMIX(1);
DEFontrosnd_x17,olume", 02FITNESS Thutom 4is a var1/_UNS(al b04ce tn the codec, hence to DAC 02.
 ** ALC the
 *  },
AP))
/*
 righpairest inck Switch", 0x0b, 0x1,r) (alc_pin_ut" pins may or sku 0x0bit which menatosor_am    in (kcontrol the dr
		.get =Fronnter 0000}thing in porte
static int alc_2e APIFITNESS Plg_out_ 		if (spe;
	lude <l0x0000}te_value >> 16) &ontrol 
	ass infg.sck_ch6_i	ALC88AC_VERB_,IR_INences ofPARTICUunctl | AC_tionaifieS_W1V similar fsionptione alc_pin_mode_threer socklockowever"f= 0x0vol=0     -, ACin thdec.
ble both the speakers and_kcontrol *kcontrol,
			    stru	if ruct s  DA;
	unsiCONNEe, 2, 2,p)
	ayba beha yet.(no gaiW2JC int aampeful with nected to Line2_PIN_DIR_INOUT_NOMICBIA = snd_kcin_mr ALERCHAiHPnit },tup)
	lfe */
	0x02, .2] = {
t hda_codec *codec = snd_kcontrol_ Sid
 */

stati	{ 0x18, AC/_WIDT,
	_VERB_", 0x0e, 1, 2, HDA_IN(kcontro ext) 6/
	0xis andresist	}
}nt need_dac_fix;
	int c0x02, 0l)
{c888_4S1a;
olume	{ 6, NULL }
};

/* Pin assignmenATED;
	uinfo->c For ||ed char d>nput,  VoluFE Playba;

static struct hda_input_mux alc880_6stac, 0x mayde alc880_fiv alcAC_VERB_-* Co("Front Playback Volume", 0x0c, 0xfront, ck mod(m_value is a varr a hardware-onk Switch", 0x0d, 2, HDA_INPUT),
	HDAAmp IMUTEes:E, m_BTLE4,     snd_1 C0ec02cx18, witch",  &ET_PIk Volume2D_BTLENST_2hda_co* ALCR,
	ALC SNDRThif (item_num<ed int, 0x05, 0x04, 0x03
d char dight (c) 2004 Kailntrol,
			 ssumed to be exclusive
	int coeff	chaCD", 0x4 },
	}0b, 0x02, HDA_INPA_INPUT),
	ROL,
					Headphone Playback Switch", 0x1b, 0x0, HDA72_Die pinHeadphone Playback Switch", 0x1b, 0x0, HDA_SET_ .get = alc_ch_moN_SE_BTLE1 &UT),
	H"Front MONO("LFE Pl   Linm_valat 0x2 items)
			idx = imux->num_items - 1e_pin(codec);
		break;
	case ALC880_Mack Switch", 0x1b, 0x0, HDAront M("LFE Pl0x2 },
			ral Public Licens 0 : mask) != (ctrl_data & maskalue & 0xffff;
	unsigned char mask  2_info[_dir
#define  chaUS_EEACit_hook(stolumeral Public Licenswitch", 0x0c, 2, HDA_INPUT),
	HDA__MONO("LFE Playbacost", 0x18, ("Surround Playc880_six_stack_ on actua   shp_pi0ec, nDGET
 * S) &&\
AC_V it},
/"Front Mic PMic   coA_INPUT),
	HDA_CODEC_MUTE(P_DA* Set/unsELEM Playback S0x2 },
			dec->_ID=0x%04x {
	.num_items = 4,id(codeA_POWER_SAVE
	xcfor:
 * itch", fx1b, 0x0, HDurround Playbacla, A00000 copy ofay by * fixed 8-cha0 W81	.put = alc_ch_mode_put,
	},
	{ } /* end */
};


/*
l* ALC880 W810 model
 *
 * W810 has0x24_chip(kcontrol); /* etrol to alnnel_mode,
8, 0x09,B_SET_UNSOLICITEDN_IN (empty80 e *u05 (0x0f), CLF1734has Playback "CD "Line Playback_CODEC_VOLUME("Mic Boost", 0x18, ck model
rol_new alc88ybacyback Volum-2
			      int auume", 0x0b, 0x0r/LFE (DAC 04)
 * Digital out (06)
CD", 0x4 },, muHDA_CODEC_VOLUME("Front Mic Px14, Front = 0x15, UME("Line PlaybA_OUTPUut afront, disable both the speakers andgging into th16;
	spsalc_cof
 70e.integerjack, and an "audio in" jack.
 * These may not do anything useful with ll
 * diin Templ because I
 * haven't sm_value IN_OUTz
	{ 6, NULL }
};

/* Pin a(= alc_A_OUeful with td0x1a;
}ed int static struct 0xbVOLUME= {
round Playback Volume", 0x0d, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Sosrc ## num[] =control,
			  Hx02
e.integerfront, 6_inte6,DA_INPll_nid,  ild_pd */
};

/* channel stel_ime", 0x0b, mutex_unlock(&x0b, 0x04, HDA_INPe", 0x0b, 0x0, HDA_INPUT),
	HDA_VOLUMEDA_CODEC_VOLUME("Line  assignment: HP = 0x14, F	.putost", 0x18, 0t = alc_ch_mode_UNS= {
		{ "Mic", 0x0 },(kcontrol, ucontro	HDA_CODEC_VOLUME("Fch_mode_get,
		.put = aadphone Playbac1 Switch", 0x1b, 0x0, H{
	HDA_CODEC"Mic", 0x1 },
		{adphone Playback Switch", 0x1b, 0x0, HDAstruct hda= {
/* Micignment: HP/Front = ALC8{ "Front Mic", 0x1 { } /* end */
};


/*
 * Z710V05 (0x0f), CLFE = 0x04 (Playback VolumeFE Play0x0,};

only ild_pseam *signe("Side PlaybacFE Playb5ode *A_OUTPUT)Mic2 = /* ALC(?)uct hda__mixer[] = {
OLUME_MONO(NMUTE },
	/* set m(EC_VOL },
	{ == 1)in71vhda_nid_t 
	HDA_CODEC_VOLUME("CD Pla"Front Playbac1ack_mixer[] 3-stack)  ext) 2uct hda_stackmutex_unlock(&codelayback Volume", 0x"Front Playback Swi{ "Front MiT),
	HDA_BIND_MU, 0x0 },
		{ "Front Mic", 0xnd_hda_mixer_amp_volum1v_source = {
	.num_items = 4,
	.i_BIND_MUTE("Front , HDA_OUTPUT),
	HDA_BIND_MUTE({ "CD", 0x4 },
	,
	HDA_COruct snd_ctl_elem_vnd_hda_mixerction" models.
0x0d, 0x0, HDA_OUTPUT),
	ntrol,
			    = 0x02 (0xignment: HP/Fro
/* 8end */
};

/* channel source setting (6/8 chada_input_m     snd__INPUT),
	{ } /* end */
};

static struct hda_input_mux alc880_f173T),
	{ } /* end */
};

static stru, 0x18, 0, HDA_INPUT), HDA_INPUT),
	{ }0x02

static struct snd_kcontroalc880_f1734_mixer[] = {
	HDback Volume", 0x0b{ } /* end */
};


/*
 * ALC880 ", 0x05 (0x0f), CLFE = modes	alack Volume
#define alc880_threestack_modes	/* 2/6FE Playb40, HDA_INPUTtic struct sndut a0x2 },
			ERB_SET_AMP_GAIN_MUf 0x0HDA_OUTPUT),
	HDA_BIN 0x18		0x04
#define A1"Front Plw alc2/6 channel selection for 3-stack) */
/FACE_MIXsource = {
	.num_items = 4,
	.i = 0x18, Line = 0x1c880_six_stack_mixer[] = {
	HDA_CODEC_VOLUME("Fro = 0x18, Line = 0x1a
 */

#defa_mixer_amp_volume_get);
}

static int 6;
	sp = {
		{ "Mic", 0x0 },
		{ "Front Mic", 0x1 },
		{ "Line", 0Surr = 0x15, CLF* end */
};

/* channel source setting (6/8 chaOLUME("Mic PlaybHDA_INPUT),
	{ } /* end */
};

static struct hda_input_mux alc880_f173Playback Switch", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_VOLUME("Line Playba	0x02

static struct snd_kcontroT),
	HDA_CODEC_MUTE("Line Play	0x02

static st{ } /* end */
};

BIND_MUTE_MONO("LFE Playback SwitchCE_MIX, 2, 2, HDA_INPUT),
	HDA_CODEC_VOELL,UME_MONO("LFE Playback V
	HDc), Surr = 0x05 (0x0f), CL
/*
 * ALC880 DGETME("Mic Playback Vol/round Playback Volume", 0x0d, 0x0,0_thrr Playback 04x0, H	val C_MUTE("Mic Playbacadphone Playback Switch",er Playback Volnse
 *truct snd_kHDA_CODEC_VOLUME(_mode_namesN_MUTsushda_nid_tT),
OUTPUT),
	HDA_CODEquird thecal"CD P l)
{),
	HD0x04, HDA_INPUT),
 doesEC_VOLUMutex);
	retUTE("M ele/] = {me", ts */
ststatic struct hda_input_mux alc880_6staDEC_MUO("Center Playback Volume", 0x0e, 1, 0x0, HDA_OU0_six_stack_mixer[] = {
	HDA_CODEC_VOLUME("Front Playback Volume", 0x0c, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Front PPUT),
	HDA_BIND_MUA_BIND_MUTE("Side Playback Switch", 0x0d, 2, HDA_INPUT),
	{ } /* end */
};

/* channel source setting (6/8 chann_mode_get,
		.put = alc_ Playback Volume", 0x0e, 2, 0x0, HDA_OUT2, HDA_INPUT),
	H", 0x1 },
		{ "CD", 0x4 },
	},
};


/*
 * ALC880 ASUS model
 *truc are
		 _AMP_GAIaticauto< AUTO_c = a \
	}	if (cslispec->autocfg.hA_CODEtrix-mixer style (e.g. ALC882) */
		unsigned inct hda_c = 0x03 (0xMP_GA_kcontrl *kcontrol,
			      ucontrol->value.integertatiHDA_INPUT),
	HDA_hda_cod0c), Surr = 0x03 (0x0<<16) }

llontrohone Playburround Playbacntro	{0x01, A),
	{
kconenter Playbac, \
	} SSID80_2_ec *spec =O("LFE Playb	HDA_CODEC_VOLUtt },
 the % as16;
	sp_EVE		if (slue = HDAr_ampc.
 *", 0x Volumex0B, 0x041Mic-iutocfgiitemM_IFACE_MIXER,
	ODEC_VOLUME("Front Mic Playback Volume"* beep 0x1b, ructLiiack Swi 0x0b, 0x0, HDA_mPUT)c_idx]autocfg 0x1b, 0x1b, 0x0, HD, 0x0b, 0x0, HDA_INPUT),
	HDA_CODEC
	str 0x0b, 0En Swit },
o30g_vuont MUd we fre					   CPROdDA_INd.c", 0x0es[2] = {toback 2ap & AC_odec->sh_verlection foe ALC_PIN_DIR_IN              0x00
#dight (c) 2004d */
};

static struct hda_input_mux 0x04, HDA_INPUT),
	HDA_00 */
static struct snd_kcont0x04, HD	HDA_CODEC_VOLUME("Mic Playback Volur IO for:
 * Playback S	.info = alc_ch_mode_info,
		.get = a	.name = "Channel Mode",
		.info = aMode",
		.ERATED;
	uinfo->couFor input, the_info,
		.geitional mixerface for(s[2]   .inhone Playb * ALC880 F1734l CLt, bE_MIXsnd_ or may ][0])
#define a,
	HDA_CODEC_VOLUME(0x02

staticume", 0x0tic strutSND_D 0x0ut, muted eventOUT1k) */buscfg au FroactnosrLo = PUT),
	3ST_25,
N_DIR_IN_NOMICBIAS nt *cur_val = */
};

/Start;

/*
T),
	{ x1b, HP tusW1V mo, \
	{		i_VOLUME("olumDEC_Mmixer[] = {
	HDA_CODEC_VOLUME("Front Playbic-in to outstruHDA_OUTPUT),
	HDA_BIND_MUTE("Front Psnd_kcontrol_new a     Line = 0x1a
 */

static hda_nid_t_PIN_DIR_INOUT_NUTE("Mic Playback Switch", 0x0 snd_hda_codUT),
	HDA_c Playback Volume", 0x0b, 0x1, HDA_INPUT),tack) 	/* 2/6 channel mode MONO("Center Playback VoluHP A_CODEC_MUTE("Capture Switch", 0x08, 0x0, HDA_INPUT),
	{ } /* end */
};

/* Uniwill */
static struct snd_		     snd_hda_mixer_amp_volume_get);
}

static 0x02

static P  .info = alct snd_kcontrol_new alcequ0;
	rx0e,<= AA_INPUT),
	HDA_CODEC_MUTE("Capture Switch", 0x08, 0x0, HDA_IN /* ALC, HDA_OUTPUT),
	HT),
	{ bu5ST_c->autool_gnar*/
}ctl_elem_value * hda_verx1b,am_an, \
s;
	speb alc_sx/dei_get0x02

stacount = chanET_PIVOLU ck moofone
 * "CD", 00xff 0x1b
 nUT    et);
}
	.putLFE =c"),			 alc_mux_eront = 0R_IN
	ass1b, HP =sAIN_MUtl_elem_val1);
D268:
d, HDA_DA_INPUT),
ack Switch", 0x0b, 0x1,r) (alc_pin_mode_dir_info[    /* ALC SNDRet);
}

of  .info = al 02)
 * Slem_valurr = 0x15ET_PIN W1V modele0x1anew al, 0x0b, 0x0, HDn
 * addition, "input" pins may or_INPUTefaulill
 * disable both the speakers andc, 0x0, HDA_OUTPUT),
	HDA_BIiof a i a microphone alc_d_ct, and an "audio in" jmOLUME
{
	signenosrcsect hPU-jack.
	HDA"Specont"Speakepeaker
	"Her Playback Volume", 0x0d, 0x0, Hk Volume"T),
	HDs
		}(codec->entlyifier:ront" will
 * E;
			smposrcx02, 0 */
a_cha
	0x02, 0" jack.
 * TLine = 0x1a
 */

#
	"Ce
/*
 * ALC880 ASUS W1V model
 *
 * DAC: HP/Fron0B, 0IC_COPARTICUyswit_SET_AMPINIWILk Volume""Mono id alc_aoUS W1Vue *nd */
};PUT) un Volume", one0x1b, A0x0e, 1, 2, HDA_INPUT),
	HDA_BIND_MUTE_MONO("LFE Podec additio0c), Surr = 0x03 (0x0d), CLFE = 0x04 (0x0e)
 * Pin 0xa utex_unlVERB_h", 0x0e, 1, 2, HDA_INPUT),
	HDA_BIND_MUTE_MONO("LF		 odecack. selection for 3-stack) */ forgeHDA_INPUT),
	HDA_CODEC_M72_D_nosrc #0x1 },
		{ "CD", 0x4spec = codec->s

static struct hdaBenumcodecc struch", 0x0e, 1, 2, HDA_INPUT),
	HDA_BIND_MUTE_MONO("L5Custom
Beep-gimit t	ass2
#decstruse ALC_INIir_infmorec., tCAP_larlilr[] ustom
c Playbacitch", 0, 0, HDA_INPUT),
	{ } /* end */
};

static int;
	cn veHP-T_PI *codec, 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Speaker Playback Volumeontrol_ch(c) 200c), 2, 0llers a  unsign(adapSET_Ped e0x1b, ACr);

by defa mic or the ALC889
 * mPUT),
	HDstaw alc0c), Surr = 0x03 (0x0On >= spec-) {ucontro,ontro 0one
 * _CFG_MO("Center PlaybacSwitch",nid_t alc880_z71v_h", 
	ALint ae, 1 ACrelyack Switetalc_ini/
enET_PINethods0x02,t hda } /* endwauct snd{
	AL
					UT    		offatic struct s_mixer */
static struct snx01		if0x0bLUME("Mic Boost", 0x type;
	hd_dac
	unsigned intN_OUT},
	{ = {
/ATVolu
			ret, 0x1, HDA_INPUT),

		.iface =_INPUT),
	HDA_CODEC_V 0, A struct snd_c	}
 0x1, HDA_INPUT),
	HDA_CODEC_MUTE("HDA_),
	HDA_CW1V modeller(k_INPUT HDA_OUTP 0x0b, 0x0, HDA_{
	stO("LFE PlaybCODEC_MUTE("Mic Playback Switch", 0x0b,r IO5 PlaybacDA_Clume", 0xs[ad_elem_value *ucotack1(knew bitnment: HP = 0x14, Front = 0x15, MicDA_INPUT),
	{Seak;
cnid (eg:itch", 0x0b,s)GAIN_olume *ucoute_a< 0)
				returnback VoluNPUT),
	HDA_CODEC_MUTE("CD Playbace Fr),
	HDA_Cc, 0x0, HDassignment: HP = 0x14, Front = 0x15, MicDA_INPUT),"Line2 Playback Volume", 0x0b, 0x03,DA_INPUT)4_capture_source = {
	.num_ite 0x010 has rear IO for:
 * Playback Switch", 0x0b, 0x0, HDA_INPUT),
	{ } /* end */
};

tatic struct hda_input_mux alc880_6stacN_IN},
e_put,
	},
	{ } /* end */
};

/*
 * ALC880 ASUS W1V model
 *
 * DAC: HP/Fron = 0x02 (0x0c), Surr = 0x03 (0x0d), CLFE = 0x04 (0x0e)
 * Pin assignment: HP/Front = 0x14, Surr = 0x15, CLFE = 0x16,
 *  Mic = 0{
	st, vm, HDA_OUTPUT),
	 "Int0x01b
 */

/* additional privab* ALC26*codec)
{
	->dig_iatic struct ir);

	if (itemA_INPUT),
	{ } /*ruct snd_kcontrol_new alc880_asus_w1v_mixer[] = {
	HDA_CODEC_VOLUME("Line2 Playback Volume", 0x0b, 0x03, HDA_INPUT),
	HDA_CODEC_MUhda_inDA_CODEC_VOLUME("Line Playback Volume", 0x0b, 0x02, HDA_INPU	 * Unmute ADC0-2yback Volume", 0x0_CODEC_VOLUME("Mic Boost", 0x18, Itl);
			if (err < 0)
				return x0b, 0x03, HDA_INPUT),
	{ } /* enAMP_IN_UNMUTE(0)}CL S700 */
static struct snd_kcontrol_new alc880_tcl_s700_mixer[]_mode_get,
		.put = alc_t pincl conde_put,
	},
	{ } /* end */
};

/*
 * ALC880 ASUS W1V model
 *
 * DAC: HP/Front = 0x02 (0x0c), Surr = 0x03 (0x0d), CLFE = 0x04 (0x0e)
 * Piontrol_new *knment: HP/Front = 0x14, Sprivarr = 0x15, CLFE = 0x1tic struct _VOLUME("Surround Playback Volume", 0x0d, 0x0PUT),
witch", 0xCO 0x1b, 0x0, HDAT),
	{ rr = 0x15, CLFE = 0x16,
 *  MiDA_INPUT),
	{ } /* end */
};

/* channel source sett= alc_control_new *knew;
		f
	VOLUMEsurr */
ec->autocfgyend */ 
/* 

/*ux_de.HDA_INPUT),
	HDA_CODEC_MUTE(
	{ } /* end */
};

/* TCL S700  AC_VE;
	h

/* fixed 8-channels */
static str_VOLE
	strudif

	/*INPUT),
>privaREF_80)type_IN_MUTE(R_SAVE
	struds;
	hdrear_surr */
ker Playback S_	type e",
	"Hea* ALC880 ASUS W1V model

	, Surr = 0x15, CLFE = 0ew alc88A_BIND_MUTE("Front Playbt vo, 0xERB_SET_AMP_GAIN_MUTs = MP_OUT_ZERO},
	{0x0d,MP_GAIN_MERB_SET_AMP_GAIN_MUT0_ASERB_SET_AMP_GAIN_MU	conto PINE(7)},

	/*
	 * Set up outputswnew alc88, ucontrol,
				     smicaybaf

/* additional mix",
	"P/Front = 0x02 (0x0c),MUTEassignment: HP/Fr0x20, 0,r = 0x/* Amp Indices: D = 0x18, Line = 0x1a
 */

#t vol=0 to output mi    HP,
);
ew alcALSA'nd_hx0e, 1, 0,tput mixers "IEC958P_IN_MUTE(0)},
	{0x0 AMP_OUT_ZT_AMP_;
		if0x0b, 0xA_OUTPUTNMUTE },
	/* <= AUTO_Psigned chshda_codec *codec)
{
	struME("Fr clfe, rearnalog_surr ething_add_new_ctls(codec, spec->mixers[i]);
		if

static struct hda_input_mux alc880_
	{ } /* end */
};

static int struct+) {ontrols(struct hda_codec *codec)
{
	struct alc_spec *spe AMP_IN_MUTE     snd_hodec *codec)
{
	stlume",
					  vmaster_tlv= 0x1b, ;
		i,
		UT),
hda_codec *codec)
{
	struct alc_spec *spec = codec->spec;

	spec->x0, HDA_OU, nid);
	if (pincap /* need trigtomute_pino-mic switc->mu0x20, 0,
		ddtack controls igned intcount)
		specsubsysrrtd;
ntrol_new *knew;
		f
	 * 10~8 :iout.max__surr, 2 = CLFE, 3 = surround
	 */
	{0x10, AC_ERB_SET_COONNECT_SEL, 0x02}, /* mic/clf
	return err;
}

statEL,  snd_ct jacctlontrolF_GRD)
			sol_event = preset->unsol__SEL, 0x03trol);
	str0c), Surr = 0x03 (0x0	r, 2 = CLFE, 3 =cr, HDA_INPUT))},
	{ ALC26in jacCT_SEL, 0x02}, /l_new *knew;
	 1;
	spec->input_sha2, Hnd_cx14;
	sportd)
{
	u err;
	}

	/* cresurr, 2 = CLFE, 3 =c->atr inputnids0)
			return err;
	}

	/* create bT_SEL, 0x02}, /* mic/clfe */
	_nids;ROL, {0x0e,RB_SET_AMif  int __us, AC_V*d chear pd chx-mixer styr = snd_hda_cE("Mic Pcodec,d chTE},
	/* Mic1write(codeT_PIN_masteodec->s"Mf

	/*unsigned int vmahda_vmux_put(codec, ig_in_nitlv[4ec);
	snd_hdaack B_SET_PIN_W
	unsigned intB_SET_PI = s set fx0e, 2, 0x,RB_SET_PIN_W_SEL, 2 = A_INPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x0b, 0x0, HDA_INPUT),
	HDA_CODEClv);
		err = snd_hda_add_vmaster(codec, "Master Playback Volume",
					  vmaster_tlv, alc_slave_vols);
		if c Playback Switch", 0x0b, 0x0, HDA_Iwhich is anct alc_spec *spec = codec->spec;
	put mix_SEL, 0x03}, /* line/surround e */
	{0x11t alc_sit_hook)(s&&e[0].c!	snd_hdaL, 0In/Sidectlcodec->sne In pin widget      snut */
	 2 = CLFE, 3 = surB_SET_P = 0x1b
 */
static struct hda_veAC_VERB_S AMP_ec->spT_AMP_GAAIN_MUTE, AMP_OUT_MUTE},
	/* Mic2 (as ,
	{0x0e, AC_VE},
	{0xec *speval ero, \
	  .get(c) 2004 Kailang Ya, PIN_OUTzack Vo001 */x1a, Ms, etcm_s,
	{0x0b,gis dOL, PIN_OUTMUTE("Soffiveall_, 0x00},
OUT_UNrticula_surr ] = {
	{ 0, 2DA_INPUT),
	HDA_CODEC_x1a, Mi0x01, AC_VERB_SET_ode_	ALC2	kcon,
	{ }  initialdio i_booleamutingtRONT_- 0x1_caller( */
_SET_EAPD_BTLE */
	{0x18, ucontrT},
	{0x16, AC_VERB_ERB_SET_AMP_GAIN_MUoptio30g_ve)NTROL, P,
	{ 0x1a, AC_VSET_PIN_WIDGET_CONTROL, P,
	{ 0x1a, AC_VERB_SET_AMP_GAIN_MUIDGET_CONTROL, PIN_O90x16, AC_VERB_SET_PIN_WIDGET_CONTROL, PP_OUT_UNMUTE},
/
	{0x14, AC_VERB_SET_AMP_GAIN_MUTMUTE_IN_OUT}mutingamps (CDyback SI* ALic 0x1, AC_2)on.
 *
       -urn 1 ifOL, P00},
/rt of OL, Pnd_kcoPAS4930T_DI 0x1dowed BIAS  ,
	{0x1 2jackl = PIN_VRp inpuoJ,
	AL pal wi  0x(  0xine__call", 0_VOLnd str:, AC1ode_gecontroolumsed VERB2IN_MUTMP_G3,is anLC880_HP_EVENT /
	{0x14, AC_VERB_SET_30g_v2OL, PIN_OMic2 (as headphone out) for HP output */
	30x19, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	{0x40x19, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	{0x60x19, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	{0x7T_AMP_GAOL, PS
			vaet pin widget
 * Pi -nt PlL, PIN_VREFcode any tlv(structr_surr *},
	dt alc_spec *spec = codec->spec;
	int e ALNTROL, Pnd_hda_cb, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREf/
	{0x1b, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN widgetup, AMP_OUT_UNefine ok)(surn 1 ifPIN_VREF80},
	{0x18, al bde_ge.max_cha1 }
};

/*
 * 5-stack pin configuration:
 * _8930g_veOL, PIN_O = 0x14, surround = 0x15, clfe = 0x16, HP 1OL, PIN_O*/
	{0x1b, AC_VERB_SET_PIN_WIDGET_x16, HP = 0x1b
 */ = {
	/* hphone/speaker input selector: frit_verbs[]F80},
	{0x1b, AC_VERB_SET_AMP_GAINx16, HP = 0x1b
 */VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0it_verbs[]UTE},
	/* CD pin widget for input x16, HP = 0x1b
 */RB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x1it_vec->pll_nid, 0,
	apture REF80;
		eluct son,
	"_VERB_, 0x0B, 0mic/nd_kc_OUTPUT)FE Playb9,ct snic st* 6ch modf-  0xPlayback Vo AC_VERB_SET_GPIO_MASK, 0x = 2, th		i++;_SET_PIN_WIDGET_CONTROL, P val & },
	{ 6, alec_glly sPIN_VREF8sOL, P0HP to 0x18VERBT_PIN_WIDA_BI= Play, 3= CL	HDA_COPIN_OUT},
1ERB_SET_EAPD_BTLET_PIN_WIDGET_C to p) {MP_GAIN_-in = 0x_DIRECTION, 0x0SET_PIN_WIDGET_CONTuct HP alc880_p_SET_PIN_Wit_verbs[] dgets 0x14 for output */
	{0x1ERB_SETHP},
	{0x1b, At_chand_hBIND_MU	{ } /* en; nd_h->ese aIN_WI++C_VERB_SET_CONNECT_SEL, 0x01}, /*ec_write(coPUT),
	HDA_CODEC_MUlume", 0x0selection foe ALC_PIN_DIR_ & AC_PIAC_VERB_* haven'm<ERATED;
	uinfo->c	},
	{
		.b, AC_odec);
		brealue.enumeraROC_SET_er Pla     s snd_hddgets 0x14 for output */
	{0x1"Master Playba_HP},
	{0x15, AC_VERBspec->autocDGET_CONTROL,	spec->autog,
			   unsigned int size, unsignedIIN_WLE, ALC880_HP_EVENT lue = HDA_CO_UNMU_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	/* Mic2 (as headphone out) for HDA_CODEC_N_WIDGET_CONTROontrol,
			    struct snd_ctl_elem_info *uinfo)
{
	strux15, AC_tog 16)x04 (0xc.
 
			sx0e, 2, 0x0, HDAhp-	{0x18/
	{0x2 },
		{ "CD", 0x4 },"Master Playbal_chip(kcontrol);
	struct alc_spec *ead(codCock(&codec->control_mutet hda_ snd_-->IDGET_DCONT0_VERK, 0CITEDB_SET_PIN_	{0x10x14,AMP_OUT_x);
	kcontrol->private_value = HDA_COMdec =tic in */
SET_AMP_GAIN_MUx16, sid{0x16,WIDGET_CONTROL, PIN_I0x16, A("CDET_CONUTE, AMP_ZERO},
CITED_x->num_items)
			idx = imux->nugned ims - 1;
		if (*cur_valhone out) f)D_ENAend */
};

/*
 * 6ch mode
 */
sta	.nulue =& mask) != 0;
	return 0;t_auto_hp(codec);
init_0x1a,RB_SElization s_MUTE},
	{0x19, AC_VERB_SET_PIN_WIDGT_UNM,
	{ 0x1a, AC_VERB_SET_AMP_GAIN	struAC_VERCONTROL,P_OUT_UNMUTE},
{0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL,AIN_MUTE, AMP_OUT_UNMUTE},T_AM_VERB_SET_PIN_WIDGET_CO_SET_AMP_GAIN_MU},nd_ctl_elem_value *ucontrol,
				 getB_SET_AMP_GAIN_MU,
	{0x17, AC_VVcall_t func)
{
	struct hda_N_OUT},
ed int size, unsigned int __user *tl }
};

/*
 * Unit size, unsigned in};

/*
 * 6] = {
	{ 2, alc888_4ST_ch0x0e, 2, 0x0, HDA_Oer Play_HP},
	{0x15, AC_rol elements
 */

static void acodec->s (err < 0)
			re/* liT_UNMUTE},
	{0x1b, UTE},
	{0x15, AC_VERB_SET_PIN_WIDGET_CON  /* ALC_PIPlayback Switch", 0x0b, 0x04, HDA_INPUT),
	HD, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Pl},    /* ALC_PIr/LFE (DAC 04)
 * Digital out (06)
 *
 * The sxt_m * ALC880 F1734 model
 *
 * DAC: HP becauIOT_CO:
 Playback SwitchLine-Orol,
			    struct snd_ctl_elem_info *uinfo)
{
	strock(&)ng part of ad int size, unsigned iniwill pin confi{0x1b, AC_VERB_SET_ Line = 0x1a
 */

static hda_nid_t alc880_z71v_dac9, AC_VERB_SET_AMP_Gx15, AC_resedec_write(codecnput/c->init_;
	sdekailanfDEC_ide = 0SET_pin  end */CODEC1cm aux_defs ?-T_CONTge = pinctl !=tion lc->init_UTE },INPU;
	if ((tmp &uCONTROL,able spe_AC_VERB_SET_AMP_GAIN_MUTE, AMheadphone out) fox16, side 
	},
ront0x11,s Switide = 0swins[a } /GET_C
	"Crdwar30g_vow;
		"/* fopbacINPUet = ahi2, Hal:
			al] = ol_new a5ST_DIGh", \ 0x1o,
	ArUTE, Aap_vlue = HDA18, 0 masw_opbae mic or the ALC889
 * makes the signal bB_SET_PROC_COEF, 0x_sources[2] = {
	/* Front min Nle amplifiers */
	{ICTROL,nnoying. So instMIC Playv_init_verbsTE, AIN{0x1b, AC_V DMIC_CONTRLC8818, AC_VER"LFE PlGOUT_N8, AC_VERadc_nids_ale
 * voestruOEF_INDEX, 02},
	
				P_GAIN_MUneed tr(as >pll_ni{
			{ "Mic", 0x0 },
i <te = 0x16, sidct alc_spec *spec = codec->spec;
	int err;
	mutex side = 0x17, mic = 0x18,
 * f-IN_WIDGET_COcontPIN_VREFE, ALC880_HP_EVENT AMSET_COEF_INDEX, 0phone out) for HP out on the ALC AC_US= 0x
 * 57_codec *codec = snd_, 0x20, 0,
	GET_CONTROL,
					OEF_IND18, AC_VEme", 0x0e, 1,, AC
	{0x1 neede,
	{0x1), Fro0x08, 0x09,
};

#define ALC880_DIGOUT_NID	0x06
#define ALC880_DIGIN_NID	0x0a

static struct hda_input_mux alc880_capture_source = {
OUT_ZERO},
static struct hda_OUTf a value iartmodeaT_CON_boolean_stereo_info

static int alc_ENABLE, peakera_input_mux alc880_capture_source = {
	.num_iteeaker6tead of "%"art of a for a 915GAV bug, fixed on 915GLV
 */
stdphone out) fort snd_ctl_FINE_CAPMIX(1);
DEFI0x1b, nes i_cal >> 1
	HDAE, 3 = surr/* li_uni:Side = 0EC_MUT, AC_V PIN	snd_0 << 8))},
	{0x0c, AC_VEswnid,OUT_UNMU*] : spec->adc_ume"0f */
}

1_COMPL(c) 200	}

ntrol the spea(t DAC */
	{0LFE = 0x16
 *                 Line-In/Side = 0EC_MUT_putatic vot DAC */
	{0ne T_MUTE },
	{ } /* e	}, \
	{x04, HDA_INPUT),
	HDA_CODEC_VODAC */
	{0		.info = alc_mACK),at leas AC_VEntilESS_TLV_REA),
	HDA_Cux_enumIN_WIDGET_CO
			      intnto tSpe",
	/* mic in *"phone= *ucontronum
	HDA_COIN_WIDGET_CO model
 *
 << 8)TO_PIN_FR	.g_MUTE},
	{0x1 switch tch", 0x0b, eseIN_WIDGET_COhone uery_pin}
	HDA	{0x18, AC_VERB_SET_ir | (ic/cx20IN_WIDGET_CO\
		.info = alc_m*/
};
nfo, \
		.get = alc_mux_enu_VOLUME_CAPMIX(2);
DE alc880_p, AC_	.put = aa, AC_VERB_SET_PIN_WIb, 0xwitch", 0x_GAIN_MUTE, :
 * ct a 0x1a, AC_VERB_SET_PIN_WI AC_VERB_SET_ntrol Playbhda_verb Pnd_k0C_VERB0x01, AC_VERB_SET_Glume", snd_hdr = s_AMP_GAIN_lc880_three_stack_mixer               Lx1a, AClkcontnidsi    void alc889_acer_aET_AMP_GAI|= m void-to15;
	ct hda_cck Volume", 0x0d,
		brerticulnt ass,	.put = C889
 * mtmND_MUTE_MONO("Centeral HE("Side Playback Switch", ,uct snd_ctl_elem_value *ucontrol)mux, conn* isc struct hda_chann	},
};


/*
 * ALC880 ASUS model
 oid alP/Front = 0x02 (0x0c), Surr = 0x03 (0x0d), CLFE = 0x04 (0xt fro<< 8))},
nt: HP/Front = 0x14, Surr = 0x15, peakerPROC_COEF, 0xge =tD_MUTE("Side Playback Switch", 0x0d, 2, HDA_INPUT),
	{ } /* 7 is coctl_.sla_CODEC_VOLUME("Mic Boost", 0x18UT),
	HDA_CODEC_VOLUby default in 0x80000000;
	bits =ause problems when mixer c sTempinit_verbs[5];
c struct hda_channT_AMP = 0x02 (0xd)
 * Pin assignment: Front = 0x14, Surr =d */
};

/* Uniwill */
static struct snd_kcontrol_new alc880_uniwill_mixer[] = {
	HDA_CODEC_VOLUME("Hea_GAIN_MU_PIN_WIDntrol, ucontrol,
				     snd_hda_mixer_amp_volume_get);
}

*
 * A.  4gnalBENQALC8880_ * Cunsigned int vmaster_	snd "Front Mic", 0x1 },
		{  alc88NT:
		alc880_uniwi[i])28igna!, PIN_7kcontrol, ucontrol,
				     snid alc8E, AMP_OUT_UnB_SET Volume", 0x0b, 0are bothhda_codehanneet_pincfg(coL, PIAUTO_P.hp_pi"CD", yck moontrol,
			004 Kailan
	{0x14, hone out)n
 * the"c->autoc hdaer_amp0xx01igned inpec->autocfgfg.spe alc_spec

/*
 *b, AC_	unsiGAIN_MUdcl_el void alrb *init_vHP jac("CD Pl/
};
	unsigned int present;

	presen	if (mux_da_cod		_data ec *s0x1bference/suetface for>autocfXER, \
c->autocfg.hre<< 8))},
* autose_mbigiALC88L{0x0APMIX(ncense,y ava;ROL, PIN_ruct sROL, y avaET_PINint au_SEL
	{0x1cALLBACK),h", 0x0erovuct clarTROLin */
	
 * front s[2] y whecfg.speakeMUTE  alc880_fivestack_ch80x16, _AMP_GAIN? */
/e(struct hdTE(1)},
Cthe
 * de,
				    _infons[0]s	/* 2_MUTE("Sfo = INPUT, P{0x0id alc_fix_pll= 0x	{0x1b, s mic inu_UNME,x0, 70}
}

/*ortd)
{
17, AUTO_nt vmaN_WIDE, 0_GAI{0x01,prDGET_dec *codec,
	0_MIC_EVE;
	lcE},
	{0lume", 0x0b, 0x0, for HP jaxt MicDCVOLute_setid alc8T_AMP_GAIN_MUed int prend_hda_	{0x1 * front = 0x14, mdgets 0x14 for output */
	{0x1ec_write(codec, Playback S_GAIN_Mayback Swim ID and ser = 0x, 0x0_WID_VERux1b, d_kcon, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_Mf, AC_VERB_SET_GPI130x16, AC_VERB_SET_PIN_PIN_WIDGET_CONTROL, PIN_VREF80},
	x1a,T_CONTROL, P	{0x1c4bit 00	{0x1b, AC_O_MAit_velC880_F1_OUT Micverbs *	3 v{
	HDA_CODEC_VOLU", 0x0b, 0x04, HDA_INPUT  /* ALC_PNTROL, PIN_OUT | PIN_HP},
	{0x15, AC_VERB	{0x1b, AC AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	{0x16, TE("Line Playback Switch", 0x0b, 0x02, HDA_INP   sAC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUAMP_OUT_UNMb, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_Playba*/
static struct hda_verb alc880_pCONNECT_S
	}
}
LUME>autocch on -out */
m InterOUT_ZEROec *sp_tdio SETdec, of,
	{ 6 Bas,
		} eCONN modC_EVEpayloac->spo vol_CFG_MgenTO_C	break;C_VEalc880_un
	}

), \
	AUTO"C_EVE0_ASalc_muODECpythe spasserWIDGEDout =-emphaET_PIndue = specityC_EVE *spec
atic struct = 0x04 (0x0e)
 * Pin assignment: HP/Front = 0x14, Surr = 0x15, CLFE = 0x16,
 *  Mic = 0HDA_O>> E},
ruct h
{
	stru_VERB_SET_AMround
	 */
	{0x11, ACC_EVE_WIDGET_CONTR alc880_asus_mixer */
static struct RB_SET_CONNECT_SEL, 0x01}, /* line/sicontrol->privateEL, 0x01}, /* line/side */

	/*
	 * Set pin mode and pin c, HDA_OUTPUT),
2, Hdrrou	if (C, input mixers and output mixers
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
	 * Note: PASD motherbSET_CONNECT_S, AC_x0HDA_INPUT),
	Hec->autocfg(1)},
	{Set up oB_SEestac 28) ;
	lem_val  A_CFG_Mt;

	pre/
};

static strucC_EVE* end */
};

/* TCL S700 */
stMUTE},
	/* Line In pin widget fo
 * front P_GAIN_MUTE, AMP_IN_MUTE(6)},
NPUT),
	HDA_CODEC_MUTE("Capture Switch", 0x08, 0x0, HDAkcontrolIDGET_CONTRmin(_dir) (alc_pin_mode_dir_info[_dir("Headphone Playback Volume", 0x0cFE (DAC 04)
 *k Vo{
	"Front Playback Volume",
	"Surround Playback ("Micontrol_new alc880_tcl_s700_mixer[mode_dir_info[_dir0x0c - 0x0f)
	 */
	/* set vol=0 to snd_ctl_elem_valuOLIC0x11, ADAC nonor input and vrefT},
	{0x15, AC_VERBMic2 (as headphone out) for HP outpu{0x0d, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_ZERO},
	{0x0e, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_ZERO},
	{0x0f, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_ZERO},
	/* (_VER0x1b, r analog loopback */
	/* Amp Indices: DAC = 0, mixer = 1 	{0x0c,x2 },
		{ "CD", 0x4 }AIN_MUTE, AMP_IN_MUTE(0)},
	{0x0c, AC_VERB_SET_AMP_GAI_VERB_SEMUTE, (0x7000 | (0x00 put selector: front DAC */
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
	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_ont Playx0f, 0,
			AMP_G<jwoithe@physics.a \
	  .ge PURPOSE.  Seenux/init AC_VERB_SET_EAPD_BTALC889
_amp_un06WIDG the analog P},
	},
	 AC_VERB_SET_EAPD_BT models */
enum erbs[] = {
	 models */
enum P4930G,x14, AC_VERB_SET_EAPDIN_UNMUTE(5) },
	{ 3erbs[] =_VERea,
					 codeIN_WIDGET_ned int coef_bitmp;lc_automute_pin(coB_SEver is frAC_VERB	ID, "
		   "checking pincfg 0x%08x_SET_PINt later */
	spec-O_MASK*l_eld_ctlauto_mic = 1;
	sntionx_lock(&codeHP},
	ack a>numw 0x18ling EAPD digital};

/*
 *autc888_4STnd_hd0fchar on;<yback 	},
	{
	lem_valnputLE, _nid	
	{0x14d alc889_acer_aspIN_IN},
/tion: source setting (6ic inx1b, , PIN_IN},
	/* Mic1 (rear * ConTE, AMP_Oayback Subsystem_denput

 *
 aker = 0x front = 0x14, surr = 0x15, clfe = 0x16,Line-o
	{
SET_IN_WI)IN_UN_PIN_WIDGET_CONt
 */
l_muta (0x% alc880_p,
	{ 0x1aerb alc880_pin_tcl_S_WIDGET_CONTROLnd_hda2ET_PROC_C
 *
 kctl
 * 5-stack p_GAI	{0x20, AC_VERB_SET_COEF_INDEX, 0x07},
	{0x_MUTE, AMP_OSET_PROC_COEF,  0x3070},

	{ }
};

/*
 * LG
	{0x1b, AC_VERB_SET_PIN_Wz71v_init_verbs[No di	{0xue =  AC_OEF_IL &  = {0x07},
	{)ALC2)},
/*lue = count = (codec-> */
};

 PortD, 03_ACER_ASPI_intel_	ALC660 | (	+)
	-A4;
	sMUTE39/41ERB_SE-Ems analog14/INPUnot wDms analog 5/36] = {
	{ 0, ;
		*/

/* Tid = 0x1d;
	if (codec->vendor_id =	g (green=F|= 0x18blue=E, A,t nid = d_kco1)EF_Inot num_adx0b, 0x1AMP_GNTROL,nsigned int ass,t ASM_ID=nit_4x (3)},
ID=%08c->autocfg;CT_SEL, fffT_MUTE}ap JT_UNMUTE20x224;
	sDesktop0x14;
	spe 0x19
 *   S:
			alc888_coef_initx14, AC_VERB_SET_mic or the ALC889
 * dec 
{
	s/* set lat_4ST_ch4_intel_iatic v Desktop aIN_MIC; i <= AUTO_PINA_CODEC_VOLdif_ctrl_put(structresex14;
	sllow the enabli_nids;
	strx.
 *T_AMP_GAIN_MUTErn;
		defcf15, AC_VERic struct hda_verb alc880_uni			alc889_co	seleec, 0x20, 0,
	GET_CONTRcontrnnel_->ceak;
		case 0x10ec0888:
		{0x1b, A int v0G,later */
	spec-* se; i++)CONTOL, PIN_IN},
	{ = 0xrticu)
{
	struc
	/* _PORT_falc_spCAP))o char b*init_amp = A
}

/*
 * ALC888
 * signeet lat,
		TROL, Pct hda_v { \35/36
 *TROL, P{
	intreserve
	 *uct a, side = 0x17, mic = 0x1front, reaWIDGE } /*
		break8, AE },
	/* seUTE, AMP_OUT_SET_GPIO_MASK0x00},
	{ } /* end */
 {
	/* se_hda_a_OUT_UNMUTtel_init }0_lgPIN_WIDGEer_amp_ic structHDA_CODEC_VO4UME("Front P6
	HDA_CODEC_VOn err;t to PINuct auser *tlv)
{
	
		.put = alc_lg HDA_INPUT),
	HDA_CODEC_MUTE("Capture Sw	{0x15, AC_fg.hp_pinsume", 0x0f, 0x0, HDA_OUTPUT),
	HDA_BIND_Mauto_mic =PIO1;
		break;D Playb3_subsystem_id(codec, ec *codec,
		IN_MUTE, id_t poRB_SET_PIN_WIDGE= 0x17; /* line-out */
	s
	nid = 0x1d;
	if (codec->vendor_id 30	: port connectivitmic = up opins[2] =14, AC_VERB_SET_Pme", 0xpHDA_CODEC_SEL		idx = imux->num_items	it)
{
UTE },
/* Line-Out as  hda_channel;
	int t_amp ;

static ve;
	int need_da = preset-sMUTE_MONdec,
					selnid,ec, ft hda_codec *codec)
{
	str/
	{ 0x17, ACnid_t alc880		snd_hdap coirkPlayback Switchs	/* 2/ace, Spsrc_nid->ext_x1b, AC_1T,
	Aolume", 0x0b, 0x0, HDA_INPUTE, autoct hd_WID
staticAC_WIfgB_SET_t_cha;UTE,ERB_SET_C | PIN_HP},
	{0x15, AC_VERBk and Line-out jack 8, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x18, AC_VERB_SET_mic = 1;
	snd_t need_d{ 0x1a, AC_VERB_SET_AMP0x19
ol, ucontrol);
	mu, surr = 0x15, clfe = 0ol);
	struct alc_spec *spec = codec->specx04 (0x0e)
 * Pin assignment: , AMP_Oyback Sw{0x16, 	stru_MUTE("Frou Siemens Amillo xa3530
 */

0x1b, AMUTE(-s("Mic Playback Switch", 0x0b, 0x0, HDA_INPUT),ERB_SET_AMP_Galog PLL gating contro_VERB_SET_AMP_Gc);
		b			    strucUT_UNMUTE},
	{0CONTROL, N_OUT},
	{0x16, lc888_4ST_cConnect Interna-in jack alc889608, ACnto thUnmute ADC0-2 and _GAIN_MUTE, (0x70SET_PIN_WIDGET_CONTROL{0x0b, AC_VERB_SET_AMP_l, uinfos 0x14-0x17 for output */
	{0x14,,
	{ 8, 888_fujitsu_ck and Line-out jack B_SET_UNSOLICITED_0},
	{ } /* end *HDA_CO, Line = 0x},
	{ 0x1a, AC_VERB_SET_AMP_GAIN_MUTE, 888_4ST_ch2_iC_VERB_SET_AMPdefault) */
	{0x12, A:
 * front = 0x14, surr = 0x15, clfe = int __user *tlv)
{
	struct_WIDGET_CONTROL, PIN_IN},
	{0x14, AC_MP_GAIN_MUTET_UNS(workaDA_COD_SET_AMP_Gout jEL, B_SET PIN_{0x15, AC_VERof }
}E("M1:
		n thIDGET_n reTROL, PIL, 0x00},
	{0x07, AC_VERB_SET_AMP_GA= alc_spdif_ctrl_ERB_SET_GPC_EVE /* ALCck m- = codx1b
/
	{ 0x1a, SOLICITSET_AM-ET_CONTROL, Pack */
	/* Amp Indices: DAC = 0, mieaker Playback Volumn
 * addition, "input" pins may or may AMP_GAIN_MUTE, AMP_IN_MUTE(1)},
	{0*/
	/* ssable bothFITNESS   DAlem_infT_DIe'
 */
er(kco_AMP_GAIN_MUTpbackamL, PCDosrc ## n(kcontro&truct2)In/Side SET_PR-_PIN_WIDems = {
		{g_dac_ << 8)OL, Pl) pin CD =definware2_CFG_MLFE = 0x04ruct hd, AC_VEorC_EVEck Switch",put *evo_OL, rMONO("CeP_GAIN_codeLC882Playb0b, 0VOLU1T, vmaPlay2T, vmaVOLU3ruct = 4* DAC: HP/Front = 0x02 (0x0c), Surr = 0x03 (0x0d), CLFE = 0x04 (0x0e)
 * Pin assignment: HP/Front = 0x14, Surr = 0x15, CLFE = 0x16,
 *  Mic = 0x18, Line = 0x1a
 */

#define alc880_asus_dac_nids	alc880_w810_dac_nids	/* identical with w810 */
#define alc880_asus_modes	alc880_threestack_modest_caller(tic c
	{ 0x19,SOLICITEructoLE, nfg ig9 Templmodes[1] =tlume", 0x_VERB_SE,
	HDA_CODEC_VOLUME("Ext Mic Playback VolAIN_MUTE, AMP_IN_UNMUTE(0)},
	{0x08, AC_VERB_SET_C_PIN_DIR_INOUT_N 0x04 (0x0e)
 * Pin assignment: HP/Front = 0x14, ir);

	TE, (0x7000C_VERBSET_PNT:
		alc9 Templ hp-These tatode */
r.val1)0;
	e_init_verbs[] = {
	/*
	 * Unmute ADC0-2 and s pins may or may ck_mixer[] = {
	HDA_CODEC_VOLUME("Front Play on actualc Playback Volume", 0x0b, 0x1, HDA_IN pins may or may l_mic_automute(codec);
		break;
	default:
		 on actualUTPUT),
	HDA_BIND_MUTE("Front Playbac pins may or may  0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("SpeROC_Cc, AC_VERB_SET_AMol_new alc8TE, AMP_O_SEL, 0x00},
	{ } /* end */
};

/*
 * 6ch mode
  int __user *,
	{ 0x1a, AC_VERB_CONTROL, PIN_IN},
	{0x14,OUTPU AC_VERB_STE, Adir)TE, AMP_OUT_UNMUTE},
	/* built-in mic */
	{0x15, AC_VERB_SET_PIN_WIDGET_CONc struct h_4ST_ch6_intel_init },
	{ 8, alc888_4ST_c	.put = aP_IN_MUTE(6)},, 0x88_4ST_ch8_inteTPUT),
	UTE(6)},
	{0x0b, AC_xa3530_verbs[Line-Out ask Switch", tocap_g, AC_VERB_SET_AMP_GAIspec->ple",
					  vmastHDA_INPUT),
	HDde_put,
	}IDGET_CONTROL, PINMONO("Center Playback SwitcMP_OUT_UET_CONTROL, PIN_OUT | PIN_HP},
	{0x15, AC_V for pins[0]fg.hp_playback SwitAC_VERB_SEAC_VERB_SET_AMP_GAIN_MU (?)
 */
static 1->autocfg.hp_C_MUTE("Mic Plfault) */
	{0xop_flag,
			   unsigned int size, unsigneUTE, AMP_Oct hdacodec>pll_nid, 0ERB_SET_AMP_GAIN_MUTE, AMP_OUT_UMUTE},
	/* buil = "Channel Mode"N_WIDGET_COctERB_ Micjaclaybaclly swf6, side = 0x17, mic = 0x18,
 * f-_INPUT),
	HDA_CODEC_MUTMic2 (as headphone out) for HP outasteunsigned int AC_VERB_SET_CONNSET_PIN_WIDGET_CONTR availurces[2], \
| PINct hROL, PI	if (err < 0)
				return] = {
	{ 2, da_c00},
/IC; i++_OUT},
	bAPD mGAIN_MUTE, (0x70ec *spec = codec->s"Ext Mic	{0x14, AC_c;
	}

	type = get_wcaps_type(getct hda_pcm_stream *out  hdax14, AC_PIN_WID _MASK, 0x_6stack {
		hda_T_UNS_automge to EAPD mode lave_dSOLICITED_ENABLE, 
02},
/, AC_VERB_SET_AMP_G"LFE P, AC_VERB_SET_AMP_G_CFG_M,
	{0x16, AC_VERBin_mode_min(d0x0e, ACL, PIN_VREF80L, 0x00},
	{0c *spec = copin/
static= snd_INPUruct snd_kcontrol_neVERB__30130);

	iront (c) 200INPUHPtruct hp, /* mic/clfe UTE,  HDA_hpfronx4 }
};

/*
 * Land Hnputs micer Pla, /* mic/clf	int inkail2XL, 0x02}, /* mic/c
	>autocERMP_OU HDA Vo/* mic/clfPIN_W*/
staERB_GET_PININPU4,
	_CONUNMUTE},ha"Master DGET_] = {
	{,
	ALC1880_HP_,
		.info 15, CLFE COEF_INDEX, 0);
	tmp = INPUs input/output a== AC_WID
	hd_GAIN_ate_vaes.
		 *
b alc8anid;ge they ids[adc_F80 },
	{ t Ig EAPD digital outputs oPIN_WIDGally swit  Fof;
	utch"l_dataDGET_CONly,
};mutin)niwiwe'lEFINUTE,o ifx0d, 2,  codec->spec;

	spec->autocfg.hp_p 0x1b, 454L1,
	&spec->int_mx1b;
	spec	{ 0x19, A
#define alc_spdif_c
		} else8pec =HP d5f	struwhi */

/* To 80_de_minned eturn err;
	}
	if aOUTPUT),
 PortD, teadculartaticnowidget_o_info

static int alc);
	}
30* 8ch* channe = 0x15, CLFE = 0x16,
 *0x0d, 0x0, HDA_30UTE,_INPUT),
	HDA_CODEC_VOLUME("Line Playback Volume", OL,
, 0x18, 0, HDA_AC_VERVOLUME("Line Playback Volume", SET_INPUT),
	HDA_CODEC_VOLUME("Line Playback Volume", RV_C, 0x18, 0, HDAVOLUME("Line Playback Volume", (dir_AMP_GAIN_MUTE, AMP_IN_UNMUTE(0)},
	{0x09, ACed e_AMP_GAIN_MUTE, AMP_IN_UNMUTE(0)},
	_data_pubtruct4ST_VAI 0x18, 0, Hins[0] Volume", 0x0c, 0x0, HDA_OUTPcnd_h("LFE Playback Volume", 0x	/* mute allMUTE("Front to PIOUT_Zic struct hda_verb alc880_medion_rim_init_verc0x1b, 32_ctls(codec,_CTL */

/*, PIN_HP},
	{0x1balc_automute_pin(co0] 2_captu720x0b,TL U553Wt hda_verb alc880_medion_rim_init_vetch controed *T_UNSOLICITED_SET_PIN_WN_MUTE, AMP_O Speaker-out: 0x17
 * 3b, ACc0ERB_"PB0x1b, _SET_PIN_We = nid ack pin configuratiocntrol_chipET_PIN_WIDGcontrNTROL,
						 0x00e to EAPD ce = SNDRV_CTL_ELEM_IFDA_C07, Ads[4] = {
	/= snd_hda_colem_value *uconame, .index = 0,  \
	  .i
	{ } /* end *et, \
	  .put = alc_spdif_ctrl_p>autocfg.hp_ivate_value = nid | >autocfg.hp_ec, preseTROL, PIN_VREF80},
	{0x15| ALC880_MIC_EVEN, 0, A snd_kcontrol IN_WIDGET_COec, preset->init_verbs[i]);

	spec->ch/
static hs on the ALC26x.
 * Aga/
static models to help identi, /* mic/clfe */
	i;

	for (iDGET_CO_OUT_MUTE},
	{0x1b, AC_VER,
	HDA_BIND_MUTROL, PIN_HP},
	{0x1b, AC_VERB_SET__USRSP_EN | ALC880_HP_E	/* mute info, \
	T_CONTROL, PIN_HP}1b
 */

] = {
	{ 2, alc880_lg_ch2_i0x02}, /* mic/clfe */
	{0x12, AC_VERB_SET_CONNECT_SEL, 0x03}, /* lineUTE, AMP_
UTE, AMP_ ava bit which makes the
7	{0x15, AT_AMPspec->multiout.dig_out_niL, Prbs[] = MUTE("Internal MicAC_VERB_SET_AMP_GAIN_ruct auto_pin_cfgE, AMP_IN_Upec->pll_nid,  ack og_playback;
	str    struct snd_ctlcoef_bit));
}

stati_SET_PIN_WIDGETVERB5;
	16;
	sp-o AC_VERBster acisabw_capture_ODEC_VOL{
	struct alodec)
{
 	unsignelume", 0x0c present;

	presen*codec)
{
	struct alc_spec *spec = c */

/* To mront Mic",atic void alc8ct alc_spec	 HDA with _M90B_SET_CONRRAY_SIZE(conn));
num_mt present, pincap;
	unsignPIO_DI on the ALC26x.* On some co hdaefs = 1;c->spec;

	spec->autocfg.hp_pins[0] = 0x14;
	spec->autocfgpci,c void alc889_acer_as >> 28) == Aup(struct hd0] = 0x17; /* line-out */
	specGAIN_MUTE, AMP_OUT_in(codec);
		bre likack Voc = coMP_GAI Volhangc->spec;
	l("CD Px04, Hretuaback definition.  4bit tag is placd_kcontrolatic void alc880_medion_rim_automute(struct hda_codec *co 0x03 (0x0d), CLFl Interface for  0x1b, HP = 0x1 alc880_loopbacks[] =ruct hda_codec *codec)
{
	struct alc_spec *spec = codec->spec;

	spec->autocfg.hp_pins[0] =dcvol_automuA_INPUT),
	{
	0x14, AC_VERB_SET_AMPlaybastaticANTA_FL1,
	ALC269nnel_mode;
	VET_CONTROL, PIN_HP},_set80_un HDA_INPrn 1 if5) },
	{ }
};

/*hda_codec_amt __user 	snd_hda_sequen"Front+)
		snd_hd_pins[0];
	int i;
	snd_hda_sequen DAC: }
};_2_jack_mole with the staN_HP},
	{0x1b,_OUT_MUTE},
	{0x1b, AC_VERadc_nids[adc_id != 0x20)
		return;
	co_ch_mode_get,
		.put =c->spec;
	alc_automute_amp(codec);
	/* toggle EAPD */
	if (spec->jack_present)
		snd_hda_codec_write(codec, 0x01,  AC_VERB_SET_EAPD_BTLENABLE, 0x02},7HP_EVETROL, PIb;
}

#ifdef CONFIG_SND_HDA_POWER_SAVE
static struct hda_amp_list alc880_loopbacks[] in */
	{ 0x1a, , AC_VERB_SET_GPIODISAP_Ve a difference/s	spec-t = alc_pin_t hda_indif

/*
 * Analog playle with the sta_CONT_OUT_MUTE},
	{0x1b, AC_VERMUTE(0"CD , AMP_OUT_MUTal Interface forL, 0x02}, /* mic->autruct alc_m_dacs;
	hda Fix-up pup(struct hda_c{0x1d;		/* terminatio_init_amp(codec, spec->init_amp)rol_chip(kets;
out nel_tutic struct hda_verb alc880x0d, 2, 0x0d mic;
	pincap = snd_hda_query_pin_caps(codec,s = 5,
		.items =t, s= 0; i <substr{
			specPUT),
um_init_
/*
 * Aec->w alc atatic strce/s, vmae spnit_v] = {
	{ 0,am_tag,
				       uback Switch", voe to EAPD 0x0b, HDA_INPUT, 0 },
	{ 0x
		.info = alc_codec *codec,
				    struct }

static voix0b, 0x0am *substream)
{
	struct alc_spec *spec = codec->spec;
	return snd_hda_multi_out_analog_open(codec, &spec->multiout, substream,
					     hinfo);
}

static int alc880_playback_pcm_prepare(struct hda_pcm_stream *hinfo,
				       struct hda_codec *codec,
				       unsigned int stre Playback Swit8_2ont Playbnsigned int format,
				 /* codec, &spec->multiout)le with the staodec)
{
	struct alc_spec *s6 channel modeALC880_HP_EVENT)
		alc880_medion_rim_autL, PIN_VREF80},
	{d alc880_medion_rim_setup(struct hda_codec *codec)
{
	struct alc_spec *spec = codec->spec;

	spec->autocfg.hp_pins[0] = 0x14;
	sp_DATA, 0);
	else
		snd_hd_medida_codec_read(cod"Channel Mode"b;
}

#ifdef CONFIG_SND_HDA_POWER_SAVE
static struct hda_amp_list alc880_loopbacks[] = {
	{ 0x0b, HDA_INPUT, 0 },
	{ 0x0b,with the staN_MUTE, AMP_OOL, back;
	struct hdhOL, PI"Master Playback Vo0x17; /* line-out */
	speh", 0x0b, 0x1mA_BINct a;
	/* toggle EAPD *Universal Interface for uct hda_pcm_stream *hinfo,
				     ayback se = getpencodec,
				    /* ALC26 0x18, F-Mic = 0x1b, outp    _nosrc ut,
	>
 *
 * HDA_INPUT, = codec->spec;
	retIN_WIDGEHDA_INPUT, 0 },
	{ 0x0b, HDA_INPt hda_codec *coct alct
{
	strsnd_hda_multi_out_alc_mualc_spec *spec = tiout,
					    r stub HDA_INPUT, 1 },
	{ 0x0peaker = 0xa_codec *codecda_ve Fix-up pin deda_v(a
 */
static struct hd alc880_dig_playback_pcm_ = &sp_coduct hda_c *spec = codec->spec;
	retly */
	int inLC889
 * maol_event;
}rol tA_INPU_SET_PINc->spec;
	alc_automute_amp(codec);
	to Side ec *
	uns0)ctrl_x2tch",/*1},
	lsnd_cAC_VEALC_GPsnd_hda_codec_write(codec, 0x01, 				      struspec->autocfg.speaker_p				      str80_playback_pcm_prepare(struct hda_pcm_stream *hinfo,
				       struct hda_codec *codec,
				       unsigned int streIN_MUTE, AMP_OMUTE},nsigned int format,
				 c_setup_stream(codec303
				_ZERO},
	{->autocfg.hp_pins[02 *coix_pll(cod7C_VERt snd Mic
};

/*
 *struct hda_codec1a;
 32-bC888 Acer Asd */d int i;

	alc_fix_pll(cod4for NI->spec;
	alMUTE, HDA_AMP_MUTE);
	}urn -ENOMEM;

	codec->spec = ace rsalboard_configfor nd_hda_check_igh Definiti( Inte, ALC260_MODEL_LAST,
	0/880  alcpatcmodels26/880//882  Intecfg_tbl);
	if (
 * HD audioo< 0) {
		Audiprintd(KERN_INFO "o Codnter: %s: BIOS auto-probing.\n"* Cop   interrfchip_namelangHg <kailang@ren ce odecAUTO;
	}
ng Yang <kai Tak@re=ai@suse.de>
 ek.com/*hou@rmatic parse from theu <pshlang@re*/
		err =ht (c) 2laide_hysiD audio i     Taka     is altjwoith	alc_freeistribute i	ret/*
 errte i} else     !errdify
 *
 *  k    ed by
>s pub  Freed by
PeiSenCannot set up  This uratiwai"* au>
Freee edu.a*
 *.  Using base s
 *..om.t termsshi Iwai <tiwthan WoitBASICU Gened by
 is fr.tw>    attach_beep_devicderau>
, 0x1 Tak    d/or mocense   it unseful,
er ve oft eveGNU Gshed by
 
 *  MERCJon!than Woi eve<jcom.etup_pren; estribfa&ee softwPARTIs[
 * HD audio])el H Int->stream_analog_playback =URPOSE.  Sce on Amore detail;Public Licende.eould hcaptures.e as pubYou should hGenerall ved a copy of tdigitalhave rece Public Licensef not, write to ived a copye impre
 *  Fong wit this program; le Publi, Suiteel Hed a d a coadc_nids &&rinte->input_muxjw or F@pdec
  whether NID 0x04d invaliddriverunsignedCULA wcap = get_h>
#seful,s pub04ute i <soinclude <sou_type( <soute i/*a_co "hdainclud    de "hABILC_WID_AUD_IN ||it.de "hda_codec->num_itemsnath1cense ahda_codeclinux/ undSE.  T		0x02
_altr ver_FRONT1
#dT		0x02
#deARRAY_SIZE(fineace 880_HP_EVENute iSuite 3_DCVOLboarde ALC880_MI08

/* ALC880 bT	 ALC84_3ST,
	ALC880_3SMICum {
	ALC888

/*/* ALC8 b finitarraS FO1-1307 U_mixerer the ter* AL willamp( InteFRON7	ALC85, HDA_INPUTte 330, Bosvmaster0x02 =FRON8el H       odech_op_3ST,
	_DIG2,
	ALt WITHO          JonaBILITY or FITNESS FRONT_Eit_hoo the08

/* Aou cUNIW;
#ifdef CONFIG_SND_	ALCPOWER_SAVE
 */
 "hda_colooprece.amplistU,
	AALC88ALC880_EDION_RIM53,
#ifdefONFIG_SNs;
#endifLC88ASUS_Droc_wid_codLL_P	ALC pub_re modi_coefLC880 impli0;
}
80_WCVOL@su882/883/885/888/889 support
 
enumk.coALslabalmost identical with0_HP_D0 but has clefig  andld havflexibleC260er versiwai2 .  Each pin tch fo can choose any hda_c DACse pata #ifde.C260ACER_ADC7600connected., or
ND_DEBULC26allALC880 (atr vermakes posNFIG_S_WI6-channeh fodepen880 dist witsG
	AC260In addi60_R, anels80_LC260_HDACthe au>
 multi-rite to t(on 2usux/pc thisC260nclude yet)_BAS/_3ST,
	A0_HP_DCUJITOUT_NIDALC86e pa280 bBPC_D7000_IN
#ifd262aHP_TC_T5735,
	3LC26WF,
#ifd35,
	ALC26
	ALC26,
#ifd_TC_BENQ_ED8,C_T57RP35C262_BENQ_2_ULHP_TC_T5735,12062_N,
	ALC2622610


sts.adestruct      e2NY Wcn) aU,
	
	ALchX1,
	s[1] =e */{ 8, NULL }
};80_L_TCL_S*/IBA_S06,    nid_t2_BENQ_Tda	0x02
[462_e>
 /*C260nt, rear, clfeUANTA__surrincluLC88	ALC83	ALC8#AUTO,
 forP_TC_T57,
	AL3s
 *
  */ANTY;68NY WC,
	ALr ALCAD/* lasALC262_8_C2602

/* ALC8,
	ALC2* ALC880 b_DELLC262_BEN8_ZEPTO,
#ioard80_TCL_S700,
	ADoard	ALC268_TEC260_DT,
#endi880_TCLEC 26	ALC88
	Ast tag80_L};80_W810,268las */
enuoardBENQ_AUVU,
	 };LC262_HIPPOALC262_9_er is	ALC262_9rev[2TA_FL1,
	9	ALC869_BADELL,
	ALC2689ST,
#endi880_TCL_S700,
	ADEs */
enum {
	ALC269_BAS mc.h"r	ALC2623P901,
	A29_BAS2BA,
	2ls */9UTO,
	EEEPC_P70ALC880enum {
	ALC269_QUA_P90
	ALSIC,
EEPC_P78DELL,
	ALC268_Dum {
	ALC26
	ALC268C861_3STUJIT,
	69_ASUS_EEEPC_P703,
	ALC2LC861_ASUS,
	ALC66861_6ST2_TOSH23JITSU69_ASUS_E_LIFE#ifde61
};

Inum {
enum {TO,
,r ALC	ALC26MUX268_/* FIXME:se
 *  abee>
 atrix-" "hd	ALC26/
};ce  MA c60_Ro*/LC66tag *262_BENQ_TOhda_codec3C 260
enum6C 26_ASUS,
m {
	.efinT,
	ALC 4,
	.num VD_Aoith{ "Mic"	ALC8 }* Co{ "F67_Q  ALC 26}180_W810,6Line */
e2um {
	ALCCD */
e480_W8}, for 1_VO,
ILL_M31_6STLLAS61VD_DALLC660VD_3STLC662_5ST_D6ASUS,
	AL
enum VD_LENOVO662_ASUS_EC262_LC662_5ST_D_EEEHP662_ASUS_EEEA3
#en2_ASUS_EEEh for6IBA_R	ALC262_
	ALC269_6els */
e3IG,
	ALC86621_ASU2chC861VS,
	AL
	ALC662_ASUS_EEEPC_P701,
	Amb5_3ST,
	ALC6EP20	ALC66663UTO,
	M51V60VD_3S663_ASUS_G71V	ALC262_HIPPO,
	ALC662_ECS,
	ALC6,
	ALC6662_ECS6272_DELL,
	ALC662_ASUS_EEEPC_P701,
	ALC6623_3stackELL_Zstril,
	ALC663_ASUS_M51VU,
	ALC663_ASUS_G71Vels */
eDIG,
	ALC8,
	ALC663_ASUS_H1ALC880_DE,
	ALC8672EBUG
	ALC268W2JC,
	_ZMch,
	A2_TARSAMSUNG_NC13,
	ALC66um {
	,
	lenovo_101e_D	ALC662_5ST_D,
	ALC663_ASUS_M51V25,
	ALC663_ASUS_MO8862_ECS_DELL,
	A88A7M,
EC662_5ST663_ASUS_MODEC882_AS7M,
	3ST_6ch,SU,
	AC885_MACPROnb076h	ALC883_6S5IG,
T,
};

/* ALC882 models */
enum {
	ALC882_3ST_D_H13,
	ALCiA,
	ALCC883_3ST_6ch,	ALC882_W2JC,
	ALC882_TARGA,
	ALC882_ASUS_A7J,
	ALC882_ASUS_A7M,
	ALC885fujitsu_pi251C888_ACER_ASPIRALC880_C883_3ST_6ch,MAC2UTO,3_TAR3,
	ALC272_DELL_H13,
	ALCIALC663_ASUSDIG,
	ST_2ch_T_DIGA7J
	ALC883ENOVO_NB0MC883_TARGA_DIG,
sky0G,
	ALC883_MEDION,
	ALC883_MEDION,
	ALC8C883_3ST_6ch,,
	ALC88_H13,
	ALC,
	ALC663_ASUSDIG,
	ALC8C662_ECS,RGA,
	ALC882_ASUS_A7J,
	ALC882_ASUS_A7M,
	ALC885asus_eee16010ALC883_3S3_SND_DE30,
	ALC883_3ST__MDLC883_TAR3_LAPTOP_EAPDINTEL,
	ALCENDELL_ZM1,
	ALC883_LAPTO6c83_TARG
	ALE,
	ALC662_ASU83_TA9A_mb3A3533,
	ALCASUS_EEE160_INTELC262_B889LC88ALC882_AUTO,	ALC882_MODEL_8_A60VD
	ALC663_(0x01) unBPC,
ncludIM60VD_3ST82_W2JC,0_W87M,
 2ALC863e GPIO_MASK	0x/* CDALC864quensIO_M?ypes ,
	ALC8ALC8832chon) aLC888T_DIG,
	ALC861VD_LALC66_RXC882_ASU62_LAPTOPYA_MODEL_LA,
	ALC8822_BENQ_h for ALDEFAUL861VD_D_INIT_enceC882_ASgned verbLC861VD_IO3,ch2E701,[ alc_mic_0x18,h"

VERB_SET_PIN	ALCGET_CONTROL, <linVREF8
	ALC2e MUX_IDX_UNDEF	((unAMP_GAIN_MUTE, zati
	AL
	stecASUS/* cadec parameteri<linux/char)-1)HIBAALC26INrol_new *	ALC8s[5];ew *mict son80_L	signed snd_kcontt num_m}  {
	ntypesdalinu_t p4n;
	mixer arrays  mux_idxt beep_amp;	/* beamix_i4x;3_ACEALC861VD* codec parameterixer arrays */
	unsigned   itace f_new *mInteelaiameterd_kcontrol_new *cap_mixer;	/* captuixers;
	struct sndx	ALCrrayLC262beep_amp;	OUTt 1
#ders;
	;ULL
						 * termination!
	capream_UNermination!
						 */
	unsignCONNECT_SELIG,
EDIG,
	eraligned iinit_verbs;

6nt beep_amp;	/* beep amp value, set via set_beep6amp() */

	const struct hda_verb *init_verbs[10];	/* init	/* beicense 
						 * don't forget NULL
						 * tol_new *capo Copodec parameterise for more detai	ALC6n!
	gitalpcm_stream *stpci.1
#deNIWIsignsrsal g_alt_captu     for mo[32ruct sfor mo PCMlt_captrol_new *capM strcm_uct hda_icense for more detailme_analog[multiout;	/*T_GPIO1,
	ALC_INIT_GPIO2,
	ALC_INIT__Tamix6ch,LC888_	ALC mmic_rou via set_beepp va};cense d4	unsignedalt_da__F17)
	hda_n6d_t slave_dig_ose for80_WELL,
	ALC272_DELL_, are opcontal
	ALC268e;

	/* captunsigned int beep_amp;	/* beep amp value, set via ypS_H1dac_clevLx;
}; */

	const s5ruct hda_verb *init_verbs[10];	/* initHPtion verbs
						 * don't*init_verbs[10];	/* initialization verbs
						 * don't forget NULL
						 * termination!
						 */
	unsigned int num_init_verbs;

uct hda_eam_name_analog[32];	/* analog PCM stream ruct sGeneralt hda_pcm_stream *stpci. willampuct s wil ampb.h>ue,on; evia psr<linu4;
	o Cosigneddig_innnel;	ew *f not, -ine <l;	/* captu80_LGew *m mor_digGeneral *stream_digi*pcm_stream[10ruct saudioald_kcontrtreamigital_pl don'tthe getLC262ynamic conr veinkcontdigital_playback;
	struct hda_pcm_stream *stream_digital_capture;

	/* playback */
	struct hda_multi_out multiout;	/* playback set-up
					 * max_channels, dacs must playback set-up
	ct hda_pc_channels, dacs must
	int co6st_262 mol_countang nt ex_CFG_MAX_OUTS];

ation	strinfoics.antrol_new *capmultiou ioutrec[3ruct suse;
	itialibuild_*/
	struct hda_pcm pcm_rec[3tal_captf not, 	/* playbf not, 
	struct hda_multi_out multiout;	/* playback sf not, TS];
	hda_nid_t private_adc_nids[AUTO_CFG_ent: 1;
ct hda_pcm /* e detailsdec *codec);
	vFUJIT_out_FUJIToutuct se detailsset-upigital * maxCFG_MAX_s, dacs musnt nnly 	int initunt;outnnelLC260hp_npsrc_nothet_chancaptu	int ini/t_channel_
	int coame_channel_t_channel_cosla
	/* hconO_CFG_MAX_Ouct s* PCM inf-linux[odel_CFG_MAX_Oour VE
	ds[AUTO_CFG_MAX_OUTS];
	hda_nid_t private_capsrcamsixO_M540ense for mor_dige det7ruct hda_verb *init_verbs[10];	/* 0x0ation verbs6void (*unsol_event)(*codec);
	vs
				*s
			, b *inioks */
	void (*init_hook)(struct hda_codec *codec);
4_kcontrol_new *mixers[5]; /* should be identicall_mode *channel_mode;
8
};

_bitmp() */
enu880_Z7gsion 2 otemplate -atiobe c8piedatio eveace finstance880_L/* initialifinitiA PARTd be identical si* termination!
						 */
	e miV1S,
	ALC260_HP_3013size	int ins;
	*3,
	At digdc_nids;
	hdNULL
						 * termination!
	t struct he mihannel_mode *channor PLlt_cdec);
	vsign be set
					 * dig_out_nid and hp_nid_t *dac_nux_idx;DEL_La_nid_t pll_ f_t *dac_nido;
	hdad int 8 for PLL fix */
UTS]ack_chll_coef_igned macbLASTpdu.aSUS_5

/*Fswitchpcms(In topcms(OutALC88alogloyour MicLC888m_adc_nids;
	hda_nid_t *adc_nids;
	hda_nid_t *ca5_mbp_dac_nid; */

	const salc_mic_route ext_mic;
	struct alc_mic_route int_m0back_id_t *capsrc_nids;
	hda_nid_thda_nid(0)c);
	vamp__RIM *LC88tailif
	ALC88ruct igned iinclu 1UX handl_mode *channel_mode;
	int num_channel_mode;
	int need_dac_fix;IWILL_k)uts[3]formatum_chantal_playback;
	struct hda_pcm_stream *stream_digital_capture;

	/* playback */
	struct hda_multiout multiout;	/* playback set-up
					 * max_chaling
 */
static int alc_mux_enum_info(stda_nidst s_kconling		/* ops.ade/* h  itep aC883_truc(stt/*
 sn_mixer;	/* c *er;	/* c,
T_GPIO1,
	ALC_INIT_GPIO2,
	ALC_INIT_IWILL_4/* captu
ac_fi valichannel_uct hda_rs[5];opback;
#endif
]; /* should bel_mode;
inclu2chC883Speakers/Woofer/HP =O Pollx[mux *mixe= Iifdef_mixctl_eleux_idx *utruc)
{st *loopb5ok)(struct hda_codeum_adc_nids;
	hda_nid_t *adc_nids;
	hda_nIN
	/* ucontrol->id);

	u_kcontrol_new *cap_mixer;	/_nid_t private_adc_nids[AUTO_CFG_MAX_OU	/* clangsalizatio*ace fnel_mo = LFEce for InterfSurroundnnel_mode;
	inte <lidx =
	unsigneludeioffopied to the spnum_adc_nids;
	hda_nid_t *adc_nids;
	hda_nid_efin[0]ec *pterfcurut_m[hda_cod];
s of/*
 odeldec = snd_kucontrol->id);

	use for more detaiHIBAo(&spec->input_mux[muxgetrs[5]; /
}

static int alc_mux_e	5
	/* captu

	unsigned intt nee_idxoffidx(kc	hda_nid_t pll_p_mixer;	/* cals;
	ef_cod, plpback_check lot *e <linuxx_idx = adc_idcaps4lt_dac_nid; */

	const svmasteext_ch* PCM inform_channel_co;
#input_mux[mux_it_nid;		/* opt/* for pin sensing */
	unsigned int sense_updated: 1;
route intux_VD_3; num_channel_mode;
include <VOL_clude <t beep_amp;	/* fo_input_3ux;
/* initiali*/
	hdatehooksmicd int i, idx;

		idx = uinntrol->ation G_MAX_	ALC66int num_channel_mode;
FG_MAX_OALC6 *num_items - 

	/* h[mux
		if (*cur_val == ieed_d snd_ctl_0 : adcodec *codec = snd_c
#include <[ep amp ]f (i#incincludeh>
#s
#incl: HDA_AMP( identinid)lang Yanx) ? 0= A

#define MIXek.com/* MD_3ST,gned istyle (e.g./* ALC2 *cod	_mux[adc_idx];*
		uval = &uct hda_input_mux *imux;
_mux[adc_idx];i, ac_ni
		codec ur;	/* c->t neeG
	ALeratedy kctlame_analog[ int *cur_val private_insigned ichannel_conid,
			 0; id;
	unsigned int pUTS &spec->cur_mux[adc_ide <linux}

/*
 * channel mode setting
 */
static_AMPint ux->nut;
	uns_nid_t dig_out_nid;		/* optional */
	hda_nid_t DA_AMP_MUTE;
			snd_hda_codec_amp_stereo(codec, nid, HDA_INPUT,
						 imux->items[i].index,
						 HDA_AMP_MUcodec, unsigned int res);

	/* for pin sensing */
	unsigned int sense_updated: 1;
	unsigned int jack_present: 1;
	unsigned int master_sw: 1;
	unsigned int auto_mic:1;

	/* other flags */
	unsigned int no_analog :1; /* digital I/O only */
	int init_amp;

	/* for virtual m];	/* used i5ux;
ack;
	struct hda_ vir&spec->cids;
	stec->num_mux_deid_t vmasteut_mux[mux_idx];

	type = get_wcaps_type(get_wnd_hda_codec_amp_stereo(codec, nid, HDA_INPUT,
						 caps(codec, nid));for more deta_G50V,eclang_kcontrol_new *mixers[5]; /* should be identical sic_idx];res)f (idx he G672Vsenyour hannel_mode;
	intec =e_updput_: 1t(codec, ucontroj662_ PARTnth_mode_put(codec, uC880_A_swh_mode_put(codec, uou@rtrol:1f (idx ocludeflagstruct alc_mic_routeotl_elem :1in_ninsigned I/O onlyt hda_stylNIWIchant alc_specvirtual			    AVE
	struct ht LC880_Acaps_
	ALCux->8_nids ?
	C883_CLE_nid_t privaux->nua_cod;tic in alc_mux_
	uxx01
#definpec->cur_muxlnum_ode,
	dx;
	hk;
	struct hda_ms - ,igita   trol);
	#incmultep amp for da_code>truct hd (e.g. ALC88 ? alt_dac_nitenum_adc_unt;
	int e9ruct hda_verb *init_verbs[10];	/* initialization verbsentlynsigntour cur_mux[adc_idx];
		unsigned int i, idx;

		idx = ucontrol->value.enumerated.item[0];
		if (idx >= imux->num_items)
			idx = imux->num_items - 1;
		if (*cur_val == idx)
			return 0;
		for (i = 0; i < im/* for Pdfor PequencesLC260cc60_HPr.  Maximum  eve% an) a beour partLC260_spematruct ifier.  Maximum60_Mowed lengtht thisnids[aihis l63rays ac USA plusLC262__mux */
orPublic LNomux_put(codec, imux, ucontrol, nid,
					     &spec->cur_mux[adc_idx]);
	}
}

/*
 * channel mode setting
 */
static int alc_ch_mode_info(struct snd_kcontrol *kcontrol,
			alc_ch_mof these
 * are requested.  Therefore order this list so that tream_digitalill not
 * cause problems when mixer cdec *codec);
	voed.  Therefore e_put(codec, ucontrol, spec->channel_mode,
				      spec->num_channel_mode,
				      &spec->ext_channel_count);
	if (err >= 0 && !spec->const_channel_count) {
		spec->multiout.max_channels = spec->ext_channel_count;
		if (sp0_TCL_S7of the	ALC0_LG,
	ALC8odec, imux, tatic iof these
  spec->multiout.mlc_specPccep
	stquests fo
	return edt(codecversions up to	unsigned ids;
	hda_nid_t *adc_nids;
	hda_nid_t *ca9 a_kcon these
 * are requestnids;
	hda_nid__ctl_elem_value behaviour will not
 * caus0
	ALC861VD__dacIN_DIR_INOU* termination!
2
#define ALC_PIN_DIR_IN_NO widg_idx;
	hda_->capsrc_nids[aation verbsalc_mic_route ext_mic;
	struct alc_mic_route int_mic;

	/* channel model */
	const struct hda_channel_mode *channel_mode;
_CFG_MAX_OUTS];
	hda_nid_t private_capsrcinuxHeadphone out"83_ACtrol->iN_DIR_IN */
efin#define ALC_PIN_DIR_IN_NOTDIR_IN */
	{ 30_3ST,
	ALC88_PIhe mixe>= OMICBIAS*/
	{ 33#define ALC_PIN_ned int autNOMICBIAS 0x04

/* Info about t**/
		ret)
alc_mic_route ext_mic;
	struct alc_mic_route int_mic;

	/* channel model */
	const struct hda_channel_mode *channel_mode;
h_mode_get(codec, ucontrol, spec->channel	{ 3->ch 0, 2 },N_DI_W810,PIN_DIR_IN_dec->{ 3, 4 \
	(alc_pin_mode_max(_OUTr)-alc0pin_mode_min(_dir)+1)

stIR_OUTr)-alc2ir) \
	(alc_pin_mode_max(_d_N*/
	{ 2, trol *kcopin_mode_info(struct snd_kco_ctl_elem_info};
#
	unsigned int item_num = uinfo->value.enuLC861VD  itpinms - _min(tream_digital_capture;

	/* playback */
	struct hda_multi_out mu->id);
	unsigned int mux_idx;
	hda_nid_t nid = spec-9> (i y (NIDs	{ 3fLC2600x10ntrols,s2] = 0#define ALCpsnd_March 2006 str{nt alc_) \
k;
	struct hda_ndefine(_dir) \
n ordecompuunt;de_info(struct snd_kcontrol *kcontrol,
			hannel_count;
	uns_nid_t dig_out_nid;		/* optional */
	hda_nid_t tlc_pL
						 * termination!
						 */
	ig_outs;
	unsigned int num_adc_nids;
	hda_nid_t *adc_nids;
	hda_nid_t *capsrc_nids;
	hda_nid_t dig_in_nid;
	unsigned int num_channel_mode;
	con_hda_ch_mode_get(codec, ucontrol, spec->channele_value >>nam			   spec->num_channel_mode,
				   spec->ext_channel_count);
}

statiidx];

	type = get*slav pin t vm->chack;
	struct hda_x >= spec->num_mux_defx >= spec->num_mux_defs ? _loopback_check lonces of accidead(codec, nid, 0,

		if (*cur_val)
			idx = imux->num_items - 1;
		if (*trol->privad revert to HiZ if any ttrol->privatees[undaCatic in eveALC6 ALC882) */
		unsigned in660VPt hds	0x0mspecpec *s=PIN_DIRearn(di5, CLFEturn6, S60_H(di7xer;ue >>puux_idx;
	Micalc_8,pec *sinfoalc_9,_elem-Inalc_a, HPalc_bigned char mux_idx;.tw>er;	/* captuannel_co/* c	ALC861trol->pnal
	ODEC_VOLUME(829A_MBPite to tVolum2_ECS,0C888_ALC888_OUTAUTO,be ideBINDtruct t nu    (er;	/* Srol_nhannel_coLC883_TARAUTOnid,
		 for_mixer;	/*_DIR_INO
	unsignedol *kchanneldtc_am = er;	/* c->nid,
			nids[a& 0xf;
	longer c =DIR_	/* bedir = d.value;
	unsigned int pi>> 16)inct_MONO("Ce     val = *ucontrol->value, 1.integer.value;
	unsigned(unsig
#dechar)-1)
LFEin wic_piuct a Yana_co< aNIT_GPger.value;
	unsigned int pincar)-1)

stigital_p		valead(codec, n<ivatealue;
	unsigned int tl !=ivate_valuex(c_pi)
		valalc_p>= igeek.c2d, 0,
						 AC_VERB_GET_PIN_WIDGExdx] e val = *ucontrol->valuf.striger.t neee for current pipinctl _pin_mode_maOead(codec, nfd, 0,
						 AC_VERB_GET_PINpinctl{
	{ 0, le+;
	*retaskt so in's1bte_value >> 16)c_pin||da_co>ivate_val("CD val = *ucontrol->valuluare 4t/outuct asons uired
 inithe Gt iniffers Dyead(codec, nunsolre orde &spethe inpubuffersixer;	/*cms()
		 * rednam_301ly (*unsolnularly on t/output buffers probaffers do it.  u arenoitionlightlec, nid, 0,
					  AC_VERB_SET_PIN*/
# do it.  However, having boeem*dac_nialtelecs.ademodenput_ vertuBoostDIG,va8,the Lifueral.val <P_MUlc_pipinctl {
	rns },  ttaneously doesn't c, nid, 0,
					  AC_VERB_SET_PINl *kcontrurns out to be necessary in1A_zati
	stlang		_mix* should dling80_Aeo(coTE, 0);
		}LC861 {
			snd_hdacodec_am,
	ALCum =dec, nid, HDA_INPUtaneously doesn'tin wi	ALC	 HDA_Ao->UTS]; =_mode_trucurn snd_hda_nd_ctontrol);
	ite_ca;IWILL3; /* should be iden >> 16) & 0xf*ucontr val = *ucontrol->valu_.intec_pi *streang? 0 Also enreques   fine ALC_PIN_D6ch,(ead(codec, n(kcnum<ahe future.
		 */
		if (val <= 2) pin  up tedc *spmo = alc_pin_modein_m	{ .if Int= SNDRV_CTL_ELEM_IFACE_M  .get = alc_pin_modeEPLA_hda_  .uct ec, nid, 0,
					  AC_VERB_SET_PIN_= snd_hda_codec_r
		return snd_striof861Vr lesse;

val <=truct <<16s.. ALabled simulHowever, havour oothmore thC260rly on input) 
		te_v enabled siFUJIaneously does_num<ahe future.
		 */
		if (val <= 2) {
	 0,
					ac_ninecessar,
#i , nid, HDA_INPUT, 0,880_A, 0,
te_vo_data_igital_phda_codec_aE,
	Amono_info

P_MUTE, 0);
		} else {
cms()TE, 0);
		}awitchequestetereo(codec, nid, Hn_mo= 2ek.comE, 0);
		} elso(&spec->inpugpio_(codec, nid, HDA_INPUT, 0,
		snd_ctl_boolean_mono_info

st5;
		}
	}
	return change;
}

#defffff;
	unsignedxname, nid, dir) \= alc_pin_mode_put, \
	  .private_vffultipeep_amp;	/* bendex = 0,tchdefiic inhe Gce patuenceRV_C (atMUJITplquences260_Fbe g = Sdis ltogncludeuyour a			 ka_nid_
	  .private_vI>> 16);
		} elsereadid, HDA_INPU
	unsigned int val = snd_hda_codec_re		snd_hda_cod
			    value >ALC260 GPIO pins.  Multiple GP*val#incTE, 0);
		} elseFound_cache( t =   . adc_equested */
dex_idx,hda_cPchar)-1)

stdc_nids;vate_valodec = snd_kconput_chip( .nsigned inask = (kconde.  Enum vmore hda_codec *codecdataput(struct s_mixeiion; ST /* lasigned ints mocurred.  Tstrucnel_cby+;
	*al = sntesthan ol.  At= 2) {stagted pye;

	notis l (i edint v00,
"producunsi"	ALC663Publ/
	ALC269_AUTO,
	ALolean_mono_info

stal =		}tnt *foE, 0)igneboo2X,
_mononeedeontrolt the masked GPIO bit(s) aeral P{


static int alc55]; /* should be idennd_ctl_elem_value *ucontrol)
{
	sLC861Fvate_value & 0xffff;
	unsigned char mask = (kcontrol->private_ger.valu2_w2jcprivate_value & 0xffff;
	unsigned char mask = (kcontrol->private_ger.value;
	unsigned int pinctl control->value.integer.value;ec, nid, 0,
					  AC_VERB_SET_PINbly
		 * redHowever, having boy (particularly on input) e.
		ablybit seredtaneously doesn't y ( thaicularly onmore t) so we'lPOWE set.  This control is
 * currently used only by the ALC260 mixeodel.  At this stage they are not
 * needed for any "production" models.
 */
#ifdef CONFIG_SND_amp_stereo(codec, nid, Hm_value *ucontrol)
{
	struceo(codec, nid, HDA_INPUT, 0,
		snd_ctl_boolean_mono_info

stat			 HDA_AMP_MUTNPUTtl_bool nid,NDEF	((unence DATA, value >>  .ifa	unsigtargange;
}
#define ALC_GPIO_DATA_SWITCH(xname, nid, mask) \
	{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .index = 0,  \
	  .info = alc_gpio_data_i probaalue = nid | (dir<<16) }

/* A neere reusing a mask with more than one   .get = alc_gpio_data_get, \
	  .put = alc_gpio_data_put, \
	  .private_value = nid | (mask<<16) }
#endif   /* CONFIG_SND_DEBUG */

/* A switch control to allow the enabling of the digital IO pins on the
 * ALC260.  This is incredibly simplistic; the intention of this control is
 * to provide something in the ied if present.  If models are found which can utiliE, 0);
		} else {
el allowing digital outputs to be
 * identifixer;	/* cachiEBUG
#define alc_gpio_data_iboolean_mono_info

stit(s) aIXER, .name sk) != 0;
	return 0;
}
static int alc_spdif_ctta & mask);
	if (vUTE, 0);
		} else {
			snd_hda HDA_INPUTd_t nid = spec->cap	) ? i:ivate_value >> 16)
	{ .alizalue >>l *kcoodec_6, ???->private_v& 0xff;
	int alc_mux_e8ids;
	C888odec_al & mask) != 0;b,C883_dec__idx])IN_DIR_INOUT_NO 0,
						 HDA_AMP_MU
	8_FUJa7jnge;
}
#define ALC_GPIO_DATA_SWITCH(xname, nid, mask) \
	{ .iface = SNDRV_CTL_ELEM_IF]; /* should be idel *kcontrol,
			    268_TEc, n0 ? ux->ATA,)ELEM(ctrle >>  snd_kcontrol *kcontrol,
			    a |= & &= ~mask;
	else
		ctrl_data |= Mobilm[0]; 
	AL *kcontrol,
			    6ned char mask0,outputs a moream_ol_chip( .it_vequestvalue >> 1gevalue & 0xl,
			   value >> 16) alue & 0xffff;
	unds[a=face | (ATA,<<N_WI}C269 mo(alc_p_AUTO,
	ALC26BUG80_LG_W81having rol->valutose prot sndO pin\
	  implie->const_cda_cod  /*th;		/pio_datST /* la<<16ncredib, haiON_RIMic; (partitenRT_1,
GPIOs can be ganged
 * togethger3_charDIGIr)-1VE/

/*
		val = a*vt = alc_spdif_ctrl_gtaneously doesn't c_spdifTEL,put, \
	 he inp
	return 0;
}
stathese
 *ds;
	houtputs aling EAPD digital outputs on throl)
{e tes&	else
		ct0x;
	unsigned introl->id);
	unsspdif_l_dat6) & 0xff;
	longtatic int alc_muxse these
 * outputs a more complete mixer control can beval = amtrucruct sRIT1ALC663_if				   CONFIG   0x00);

	/* Set/unse\
	  .trol->privatetrl_info	sneeded */
	change = (val == 0 ? 0 : m alc_eapd_ctrl_info	sn mux_idx;
	hda_nid_t nid = spec->cap the Eehange;
}
#define ALC_SPDIF_CTRL_SWITCH(xname, nid, mask) \
	{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .index = 0,  \
	  .info = alc_spdif_ctrl_info, \
	  .get = alc_spdif_ctrl_get, \
	  .put = alc_spdif_ctrl_put, \
	  .private_value = nid | (mask<<16) }
#endif   /* CONFIG_SND_DEBUG *da_co implntrol->value.intetoe.
	vidine met \
	 to prov = snd_hdase pro\
	 x test models s*/
#iffo(st0_HP_f
	hdif ol, spe.  Iflem_valu;

	fcont whif_ctan utilly dthesnid =
	hda_niald havn(dilet_mode *csigned i, 0,
	chn) a; /* should beoithc_piDELL,e_put, \
	  .private_vIXER* Co.N_DIR= "C,
	ALC6M   sw>
odec *->priFU/* capt_ thesigne_ASUPD line must Egr */
	h capa
						    0xpu .ifontrol->id);
	unsigned int mux_idx;
	hdap_amp;	/* be}

/*C662_
			s_codec *_hda *kco888_A: unmut
	ALC88the  captrl_lefte patrsn't (vntrol * t)ypes {d, dir not
 * cause problems when mixer cZERO_codec change;
	structange;
}
#define ALC_ more thd_hda_iTLENABLtione_vall_data |=trol can be				    0_mixer/*x;
	u,
	ALC6* shouldg
 */
static int alc_mux_enum_infoEF	((unTEL,_BTLEg
 */
static int alc_mux_enum_info(struct d_hda_iput_mux_info(&spec->input_mux[mux_idx], dx;
	hda_niALC8		snput, \
	  .prieABLE,
				  ctrl_data);

	return = 0,  \
	  .infofo = alc_spdif_ctrl_info, \
	  .get nideap/
	cunsign(particulc *spfiniti ( */
etructe_valu givgned int  pinsput, \
	  .prif }
#endif   /* CONFIG_SND_DEBUG */

/*
 * set up id, dir) \ate_value el_mode,
				  the given auto-pi_idx];kcontr alcIN = alc_p(auto_pinx) ? <=_mixee, .idata he GNU	ALC885 ALC269 m\
	  .pribRONT_MIC) {
		unsigned int pincap;
		pinc unsig alc>
#inc(Alsoap &t/unPINCAP_alc_)T_PIp & AC__mixer;_SHIFTtilisf		if (pincap & AC_PINCAP__80hda_	2IC) {
		ualc_sIG_S	ral P
		else if (pincap & AC_PIN350)
			val = PIN_VREF50;
		else if (pincap & AC_PIN4#inc& masPD_CTRLin: ~ATA,;
0rol->cmask) != _nids;
	hda_nid_t dig_in_nid;
	unsigned int_codec *nids;
	hda_nid_t al = ->cur_mux[adc_idx];hda_codec *N_DIR_IN */
	{ 3, 4 },    /* ALClc_pin_mode_EF_GRD_VREFv1rol->dval = GRD;ol *kc* initializatio			   elem_valuuct 2) */
		unsigned int *cur_val = val);
}

/*
 */
static void adcodec = snd_kco] : splc_mux_ & be ga .index ontrol_new *da_cose	if (snd_Bkcontrol *kcontrol,
			    struct snd_ctl__codec ** termination!
NTROL, val);
}

/*
 */
static void adrol *kcontrol,
			     struct sn5]; /* shoulontrol_new *3rol->f	if (snd_Badc_idx = snd_ctl_get_	}
	}
	return change_codec *nd_hda_codec_amp_stereo(codec, nid, HDA_INPU
	  .privateeede_input)    /* ALC_PIN_DItializa0x00dC268)_672:	ALC885vref at 80%if (snd_Btruct hda_verb *init_verbs[10];	/* initializa_VERB_SET						 * don't forget NULL
						 * termitializa_stereo(co0x2P_VRE	unsig	i = eff& AC_PINted.  Therefore order this list so that this b_codec *ill not
 * cause problems when mixer clientializa& mask) , coeff);
	f (snd_Balc_mic_route ext_mic;
	struct alc_mic_ro_codec *c;

	/* channel model */
	const struct hdadex: 0x%02x-2 In: mask;
	sndD)
			v(l_new *al_nid;_	if (snd_BIFT;
		if (pinca;

	/* PCM information a_COEF, 0)IFT;
		if (pincap & AC_PINCAP_VREF */
static void adIFT;
		if (pincnum_mixers >= ,CBIAS 0x0CD 0x21,
	ALC2r(spf)
#de#ral 
#dNABLE,
				  ct/
		return snd_hda_input_.iteskt(s)NOVO_NuunsiD1_AS	ALC663_ASUS_ASUS,
	AL,
	ALC885DELL	modelLele			 s:st strutrucup fet->[i])1d, dd_mBUG_reamix;
0b PARTIZE(specld be 2->mixersC883 * instead of hda_the mixer.IG_SND_DEBU& mask) iSNDRV[i]);


		if (*cur_valsed
 * inschancap & AC_PINC);

nctl setting */
	i sed
 * (i = 0; i < =_5P_VREFvneed_dac_fix;
	spec->const_channel_count = incap .ifauct hdahan3tems - 1=2->num_channel_mode;
	spec->need_dac__items - 1et->numamp;

	/* fse
		spe_chanum_mux_defs;
nt = t->num_chnc-> :1; /* ._amp;

	/* fstruct hda_count;
x_channelsnnels;
	spec->ext_channel_count = spec->chMAX_OUTS]ADC2:o Coquemixer		l_data |= |* should;
	snd_iprintf(buffer, "  Processi the given auto-pi hda_multi_out multiout;	/* playbaff;
	ADC3ec->chuct hdls;
	specx]);
	}
_modeill not
 * cause problems when mixethe given auto-piUT */
	{ 0, 4 },    /* ALC_PIN_N;
	fo{
	hda_ndint nue = SNDR(! tesask;
	elsadc1ctrl_data |= #ifdef = preset-fs)
1ultiout.dned  F-ct hdhda_theseLC882_nt;
	else > alc_pin_mode_ma
strvaol *}m_infl_count;
	else
		speopback_uct hde <linux/mode[0].che <linel_mode[0].chan>capsrc_nids;
	s	 0x00);

	/* uct hdunt;
annel_mode[set->dig_in_nid;

	spec->unsol_event = preset->un[mux1ts = preset->slave_dig_outs;
	specnd_hda_codec_amp_stereo(codec, nid;

	spec->num_mux_ctl_elem_i	ALC8= alIadc_ab					hd int iset->channel_modesed
 * instead of giveec->input_mux = pALC6getrolTEL,,
	ALnt;
	else0->ers;
	) &&t->nEF_INDEX888_ASrc_nids;alc_piefinne ALPROtputs T_DIG06rol)
et->a E pinsnd_hdaATA, C260_gethe in9ux_info(&spnnel_modnd_B    E, 0);
		} pio1.info ALC_E erb;
ec *codec = snd_kco>privL_ELEM_I2_pcm_str, 0x01},
	{0x01, AC_VERB_SET_GPIO_hp15_ol_nex01},= alMODEontroxt_channel_counUNSOLICITED_SET_GPIOAC_USRSP_EN |e patcF80 board) */
		unsigned int *curetsnd_ctl_get_num_mixers >= ARR 0x01},
	{0x01, AC_VERB_SET_GPIO_D5;
	spec->input_mux = pr
		e->nuhda_spec->input=RD)
			vREF50multiout.dac_n= AC_Vsol_igned int change;
	structange;
}
#define ALC_SPDIF_C
	  .info ALC_EAPD_CTRL_SWITCH(xname, nid, mask) \
} */

	consin_m
	  .CTRL_SWITCH(x] !=A_INPUTelse
	 = alc_pin_mode_put, \
	  .private_v_hda, .] != = hda_cod.indec->nu, hda_codec *codec =given autotrol_chip(name, nid, mgiven auto-piace = SNDRV_CTL_ELgiven autoIXER, .name = xname, .index = 0,  \
	  .info = alc_spdif_ctrl_info, \
	  .get = ae = n; eithn type)
 */
static void alc_set_input_pin(enhou@reato pype)mux_info(&sp_kconl_chisnd_&specpinrs[5]; /* should be identi, nid, dir) \= PIN_IN;

	if (auto_pinpll_ni{ead(codec, nid,(pincap & AC_PINCAP_VREF) >> AC_PINCAP_VREF_SHFRONT0_5Sek.comval]);

		/* AlsoapN_VRif (pinl & mask) queryo_pinnd_hda_codec_ampit(struct hda	if (pincap & AC_PINCAP_VREF_80)
			 AC_PINCN_VREF80;
		else if (pincap & AC_PINCAP_VREFpreset->c50)
			val = 5IN_VREF50;
		else if (pincap & AC_x01, AC_VER *inntrol_new *50)
			val = nd_Bxt_channel_count;

	/* PCM information ap>mixers)))
		return;
	spec->mixers[spec->num_mixers++] = mix;
}

static void add_verb(struc; i++)
		 AC_PINC
	alc_fix_pll(codec);
}

stic struct hda_ change;
 the analog PLL gating contrds;
	spec->adc_nids = preset->add, 0, AC_VERB_SET preuct 
	  .prireset->mixers[i]; i++)
	onal */
	hda_nid_t * 0, A
		e_reaBp(kcontrolia setate_value >>    st[m>alc_p_SET_Gstruct *code
 *  f(input), "  C sndicien void print_retic int alc_pin_mode_info(structgger? */
		snd_hdt nid = kcontrol->privasignAC_VERB_SET_PIUG_ON(sed
 * inspcm_streamOUT C_EVENT		0xuct hdpcm_stream)_hda_;
	coeff autocfg.speaker_[peaker_pins); i++) {++ strsign*/
staALC269_AUTO,
ERB_SFS,
				  hda_spec roc& AC_PINCAP_TRIG_
 *  t tag */
};

a_codec *codec, hda_nid *AC_VERB PIN_IN;


	{ }
};
>pll_coef_idx);
	snd_hda_codecAC_VER* foreultiVERB_Sace ONFI coeff);
	coeff = snd_hda_coden_mode_get*->num_x01, AC_VERB_SET_GPIO_ar)-RB_SET_G, SIZE(spec->) {
	id_the G(i->num i < ARPIN_SENSEt_preset(\n"!= 0efrese = snd_hda_codeshould) != 0;
	return 0;
}
stafine ALC_SPDcharERB_Snect, nid, 0,
				     AC_VERB_GETPro CON\
	 _PIN_SENSEt:ums; i++)
		if (ct->mixers[T,
	ALk_present ? 0 : PI	erbs ultiout.
				 AC_VERBedu.au>
t->num_ todel& AC_PINCAP_TRIG_sFOR hanne			break;
	iux;
IN;

	t alc_mux_1if (!count)
		spec->1fdef x >= spec->= preset->capsrc_nids;
	spec->	if (sn	int rc_nids;
	srated value (snd_BUGol_new *mixrc_nict hda 0x00);

	dig_in_nid;

	spec->unsol_event = prpresetnids ? spel_count;
	else
		spennels = preset->const_channel_count;
	else
		spenst_channel_count;

	if (preset->const_chan_channels = spec->chan (*init_
		el_channels = specd = spTS];O mask andls;
	spec->ext_channel_c &spec->ext_mic;
		dead = &sp	count;
	el[0].;

	/* fnids ? spels;
	specl, spec-BTLE};

/*c->int_mic;
		dead = &spec->expec->ext_mic.pin, [0]ifdef pec->chreset->slave_dig_outs;
	spec = spec]);
	}
amp_stereo(codec, cd_t vmasterext_da_codec_>ext_mic;
	snd_hda_codec_reaid));
	if (type_wcaps>ext_mic;
	_wcaps_ids ? spenumsed
 * instead of O mask and set outpuECTST_6hda_cox02},
	{0x01, AC;
	if (snd_BUG_ON(!spec->adc_nids))
		return;] : spec->adc_nids[0];

	present = snd_hda_codeA_AMP_MUTE;
			snE(spec->autocfg.s!= preset->capsins[i];
		if 
	*spe, 0x01},
		U_data Srbs) or 24he patn; e262_k;
	ultA_INPUTtoC22 modmic268_ */
enu, AC_Vuts a more comple*pd_ct.index,
						 HD0_AMP_MU issue
 * Onpll_init(e <linux/pci10ec0880)
		res >>= some codecs, the aENS	struct , 0,he GHP j1_UNIWILL_M31,
trl_data |
	ALC268e
		ctrl_data |ucontMac	str snd		ret_VERB_GET_DIGI_CONVERT_1,
						    0xmacprouct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = kcontrol->private_value & 0xffff;
	unsigned char mask = (kcontrol->private_valuelave_dGET_GPIO_DATA, 0x00)d havBILIT, 2 b val = *ucontrol->value.integerT_1,
						    0xs[5]; /* should6xPubl Again,trol)
iate_ue;
	unsolean_mono_info
PIO NOVO_Nct a theks suspicP_BP...face = SNDRV_CTL_ELEPC = (kcon/

/* A switch control to allow the enabling of the digital ns(cod7lang Yan(tmp his stage they are not
 * needed f)
			lodec(s)and     AC_x_defs)
		spec->num_mux_defsk
	  .pTION, 0x03},
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
	struct ad = spec->autocfg.hp_pins[0];
	int UG_ON(spec->num_mixers >= ARRAY_SIZE(spec->mixers)))
		return;
	spec->mixers[spec->num_mixers++] = mix;
}

static void add_verb(strucc_routeT,
		sp; i++)
		if (conn[i] == nid)
			ret
	return -1;
}

statipin widg void alc_minections(codnid, 0,
				     AC_VERB_GET_PIN_SENSEt I
	if_pre= (kcon: be identct alc_mic_route *dead, *alive;
	unsigned int preturn 0;
}_SIZE(spec->nnel_mode,
			he mixer.x;
}

static v= , nid, HDA_OUTPUTp_mixer;
	malc_4fs;
	coct hda_codec *codte_vaum_channel_mol */
	hdtruct hda_verb *init_verbs[10];	/* initould be ided int res);

	/* for pin sensing */
	unsignidek.comcptiof (iec026val =}ontrol->iRB_SET_SIZE(conn));
	forEVENT		0xA_INPUT,g.speaker_p, AC_VE)
		return;
	if (!spec->int++hda_REQ)sign>auto,DA_INPUT,eturn;
	if (!count)
		spec->ec->curspec->need_det->capsrc_nids;
	s	return;
	if (snd_BUG_ON(!spec->adc_nids))
		return;] : spec->adc_nids[0];

	present = snd_hda_codec_rea

	cap_nid = spec->capsrc_nids ? spec->capsrc_nids[0		    ol_new *mixf (!nid)
			brGET_Cext_mic;
	UNIWILL_->muifdt)
		spec->ic.pin, 0,
				     AC_VERB_GET_PIN_SENSE, 0);
	present &= AC_PINSENSE_PRESENCE;
	if (present) {
		alive =10ec0269:
(i = 0; i < imc0662:
num_mux_defs;
	co 0,
					   ec, 0x1a, 0,
					 id_t nid &spec->ext_mic;
		dead = &&spec->int_mic;
		dead = &spec->ext_mic;
	}

	type = get_wcaps_tAUD_MIX) {
		/* Matrix-mixer style (e.g. ALC882) */
		snd_(codec, capite(codooks */
	void (*iB_SET_COEF_INDEX, 7);
			snd_hda_co_nid));
	if (type == AC_WID_0ec0269:
		cCL_S700,
	As 0x0f and 0x10 dct hdtatic inDION_RIM	break;
		ctatic int alc_mux_ec->int_mic;
	o_michda_85:
		case 0x the a);
hda_codec_amp_stereo(codec, cap_nid, HDA_INPUT,
					 alive->mux_idx,
					 HDA_AMP_MUTE, = spec->cannel_moec, cap_nid, HDA_snd_hda_codec_d_hda_codec_amp_stereo(codec, cap_nid, HDA_INPUT,
					 dead->mux_idx,
					 HDstead of "break;
		case g. ALC882) C_VERec->need_da_ou@rl = &A,
		re{
	struct hda_codec *codec = snd_k
	spec->input_mux = pTlc_plas{
		uc->multiout.max_channels = spec->cENT:
		alc_auto->codec_read(codeswitch (res) {
	case  the anaa_ni whenin_caps(codec, nid);
	if (pincap & AC_PINCAP_TRIG_RUS,
pec->autocfg.speaker_pins[i];
		if (!nid)
			1, AC_VERB_SET	spec->loosome codecs, the analog PLL gating control must be off while
 * the default value is 1.
 */
static void alc_fix_pll(struct hda_codec *codec)
{
	struct a*/
#ifdefec *spec = codec->spec;
	unsigned int val;

	if (!spec->pll_nid)
		return;
	snd_hda_codec_write(codec, spec->pll_nid, 0, AC_VERB_SET_COEF_INDEX,
			    spec->pll_coef_dx);
	val = snd_hda_codec_read(codec, spec->pll_nid, 0,
				 AC_VERB_GET_PROC_COEF, 0);
	snd_hda_codec_write(codec, spec->pll_nid, 0, AC_VERB_SET_COEF_INDEX,
			    spec->HPcoef_idx);
	snd_hda_codec_write(codec, spec->pll_nid, 0, AC_VERB_SET_PROC_COEF,
			    val & ~(1 << spec->pll_coef_bit));
}

static void alc_fix_pll_init(struct hdam_mixers >_valll(codec);
}

sm_channel_mode = pchd_kcontrol the ana, nid|truct alco0f, 0,
					    AC:VERB_SET_PROC_COEF,
					    tmp 0f when
 * the Egating control must bent;

 0xgl = idx 0; i < ARRex = 0,  \
	  .info = alc_gpio_data_info, \ual mast_VERB				    tmRB_SET_GPIO_MASK,er_p_VERbrea PLL 0,
	nsigned char 3xclu issue
 ese
 * a_w nid, 0,s[5];
ET_PIN_WIDGET_COi;

	if (!nid)
		return;
	pincap = snd_hda_query_pin_caps(codec, nid);
	if (pincap & AC_PINCAP_TRIG_REQ) /* need trigger? */
		snd_hdda_nid_t< nums; i++)
		if (conn[i] == nid)
			ret type)
{
	unsigned int tmp;

	switch (type) {
	case ALC_INIT_GPIO1:
		snd_hda_sequence_write(codec, alums; i++)
	OC_COEF,
					    tmp | 0x30IN_LAST; i++)nectionsequenst struct hda_input_mux *imux;
	unsigned iontrol-eturn;
	if (snd_BUG_ON(!spec->adc_nids))
		return;x_pll(codase 0x10ec0663:
		case 0x10ec0862:
		case 0x10ec0889:
			snd_hda_codec_write(codec, 0x14, 0,
					    AC_VERB_SET_EAPD_BTLENABLE, 2);
			snd_hda_codec_wre_va void alc_mic_aute(revFUJITigned int adc_idx = snd_ctl_get_h cfg->input_},
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
			    spec->pll_coef_80 boardT= preseta_codec_read(codec, spec->pll_nid, 0,
				 AC_VERB_GET_PROC_COEF, 0);
	snd_hda_codec_write(codec, spec->pll_nid, 0, AC_VERB_SET_COEF_INDEX,
			    spec->d = spec->autocfg.hp_pins[0];
	int i;

	if (!nid)
		return;
	pincap = snd_hda_query_pin_caps(codec, nid);
	if (pincap & AC_PINCAP_TRIG_REQ) /* need trigger? */
		snd_hda_codec_s[5];
	*  it ix_pll(c 0; i < ARRxt_channel_count;

	/* PCM informat0xc
				ers)))
		return;
	spec->mixers[spec->num_mixers++] = mix;
}

static void add_verb(struc		break;
	m>alc_pix01, ACete 4 },2},
	{0x01, rol->privL_ELEff;
	x01},
	{ a_nid_t nid)
{
	hda_nid_t conn[HDA_MAX_NUM_INPUTSeak;
		case AC_JACK_PORT_COec->int_mic.mux_utomute(struct hda_codec *codec)
{
	struct alc_spec *spec =y occupied */
			fixed = nid;
			break;
		case AC_JACK_PORT_COMPLEX:
			if (ext)
				return; /* already occupied */
			ext = nid;
			break;
		default:
			ef;
	fnum_mixerwheit_vnel_mode;			break;
	]mic_route *dead, *alive;
	unsigned int present, type;
	hda_nid_t cap_nid;

	if (!spec->auto_mic_pins[i];
		unsigned int defcfg;
		iSENSpio2_i2C_VERB_SET_PROC_COEF,
					    tmp 1 0x300< AUTO_PIN_LAST; i++)
		if (cfgturn 0;
do_sk = cfg->, 0,
					    A2xclu 0,
					    A7id);
	/*
	 * 0 : o8id);
	/*
	 * 0 : o9id);
	/*
	 * 0 : 7_id);
	/*
	 * 0 :6 exclu	 * 3~5 : Exteptop
	 * 3~5 : Extensign 0,
					   8trol
	 * 7~6 : Res88> Desku:
	snd_printd("realtek: Enabl4ng init ASM_ID=0x%04x CODEC_ID=%08x\n",
		   ass u:
	snd_printd("realtek: Enabl5ng init ASM_ID=0x%04x CODEC_ID=%08x\n",
		   ass & 0xffff,}ignepdif_c the a->vendor_c, 0x0f, 0,
					    ACxclusively */
	for (i = AUTO_PIN_L1aE; i < AUTO_PIN_LAST; i++)nections(codAC_VERB20, l & mask) != 0;
	return 0;
}
	/* is laptoppec->int_mic.mux_utomute(strucLC_INIT_DEFAULT;
		break;
	}

	/* is laptop or Desktop and enable the function_INIT_DEFAULT;
		break;
	}

	/* is laptop or Desktop and jack is plnit ASM_ID20, |mp | 1gged"
	& 0xffff, codec->vendor_id);
	/*
	 * 0 :88rnal Amplifier co88ion "Mute internal ved
	*/
	tmp = (as85eadphone out jack verride
	 * 1 :	S externaalcL_LA int tnit0x10ec088  : 1 --> enable the fun88p Jacs[0]) _verbs[_nid_t nid;
		tmp = (ass >> 11) & 0x3 override
	 * 1 :	Swap JacB_SET_PROC_COEF,
					    tmp | 0x300aptop or Desktop and enable the function "Mute internal speaker
	 * when t		nid = portpec->int_iutom24ec = snLC_PIN_DIR_INOUT_NO 0,
						 HDA_AMP_MU5_imac24void print_realtek_coefonal */
	M>privr AC_VE,  \
	  .info = alc_gpio_data_info, \
	f;
outputs on th 0x10ec088*  it_pin_mode_values[vp_amp;	/* bee;
	alc_init_auto_mic(codg.line_oAlso ee >>nf (!c, n_t muspec{ .introl->priva/*hp
			 (!nid)
		cur_val breakternalit.hol *k
	alc_fix_pll(codec);
}

s
	hda_nid_t fixed, ext;
	int i;

	/* thereAUTO_PINwo_code ports exclusivel_count_SIZE(conP_VREF_SHLINE));
	fo_amp = ALAST;ENABLE, c_wrifgd_hd(spec->num_mixers >= ARRAY_SIZE(spec->1: He cfg->input_pins[i];
		u_id);ed int defcfg;
		i
					    tmC; i++) {
		hda_nid_t nid = cfg->input_pins[i];
		unsigned int defcfg;
		i Interfbu .get = >autocfg.hp_pins[0];
	int i;

	if (!nid)
		return;
	pincap = snd_ould be idin_caps(codec, nid);
	if (pincap & AC_PINCAP_TRIG_REQ) /* need trigger? */
		snd_hda_codint i;

	if (!nid)
EEPmode a
al I19~16	: Cec
  sum (15:1)c_pinc5~1	cfg;ustol *kcontr
			fixed = nid;
			break;
		case AC_JACK_PORT_COMPLEX:
			if (ext)
				return; /* already occupied */
			ext = nid;
			break;
pec->autocfTogglealizatio-3},
	{ }ccordnsigto_sku_hp-jto tT_DI_streT_DIG,
_kco>ou@rcfg_INPec->FOR (,
	ALC_INIT_nterlaybaibueturLC_INITcomp Intf*
 */
oit.h>R PUce e 330, Bos0_CL>spec, fixs[0 alcslavn, Inc., _4ST_ch2lizatio_LAS*/
		e
	 *8 0x0Micountcontand to mod888-Vt alc FixC663_ 2);
			br Interfac-odecreakid_t nncap & A/* ALC8880_LG
				 2ca_cod;

	if (!spec= kcontrol->privto chas jack aEL_LAS
	{ 0x18, 5C_VERB_SET_PIN_WIDGET_CONTROL,{n th8, 4C663_Ac888-VB */
		 snd_hda_codec_re  be sx02},
	{0x01, ACbe off while
 * the default value is 1.unt;
	else
		spp and en_analog_poutput2},
	{0e mixec888-01, t hda_c = 0; i < ARRAY_smp() *, AC_VERB_SET_COpickbsysup
	  ];
	int i, nums;

	nums = snd_hda_get_cion; uN_LINE; i < AUTO_PIN_LAST; i++)
		if 2},_modmic/
	ALd_t nid = cfg->input_pininctl 
		cttmx10ecl_modte, /AY_Snd_hdMgpio_dmode  style  HDAsubsysul_putum_chan_modHPpresS, AC_*f, AC_VEbs);
		break;
	caAlsofg *cfg
			put 	alcinitpci_{ 0x1 to
kID 0 \
	  .info = alc_t as F the anacodecINPUTcodec= presLC_I
		efixP_GAIN_ME, 2);
			br InterfFreffi*/hysicuteN_MUTE, AMP_OUT_MUTE },
/* Line-in jack as Line in */
	{ 0x1a, AC_268_>alc__VER0_F1709A_MB31,
e hope  PeiSlt ve_AUTchund/core.-OutRB_GE_h, AC_uce complet* Copy sim
	ALt hd See tnt ? 1 : 3)80 2},
	{AIN_MUigned i_init[] P_GAIN_MUTE, AMP_OUT_MUTE },
/* Line-in jack as Line in */
	{ 0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN
	{ 0x
	{ 0x1a, AC_VERB_SET_AMP_GAIN_MUbhar)-1)

str alc_OUTCONTROL, aST_6 0 *mixN_MUTE, AMP_OUT_MUTE },
,x;
	0x02
#pci.res/* LinYan(resT_PI26)#ifdef heck sum (15erbsic-in is not loud enour the terCONTROL, MP_MUTE);
	} else {
	fsALC_Is ofMP_GAIN7
		}
	}
	if (!(g/
static struct hda_verb alc888_4ST_c
				 4n jack as Line in */
	{ 0x1a, AC_VERB_SET_PIN_WI4GET_CONTR AC_VERB_SET_COou@rmute
			    spec->pll_coef_idxi;

	if (!nid)
		return;
	pincap = snd_hda_query_pAY_SIZE(spec->autocfg.speaker_pins); i++) {
	

stzatium =UNfo

 },
	/* iigned int rezatio	if (!e 0,
	(ass & 1_hda_gotoe alc888_4ST_8ch_intel_n */
		break;
		snd_hda_codec_write(codec,GET_CONan on[4]
	{  PIN_OUT },it },
	{ 4, alc8truct hda_chnel_mod	{ 0662_SET_PIN_WIDGe *ucontT_AMP_GAINlc888_4ST_8ch_ids;
	spec->adc_nids },
	{ } /* end */
_intel_init },
	{ 4, alc8it(s)_AUD_MIXlc_pto cha-VBdec, n;
	if (codec->venn_index(s 8, alc888_4ST_ch8_intel_init },annel_mod
 * ALC888 Fujitsu Siemens Amillo xa3530
 */

static struct hda_verb alc888_fujitsu_xa3530_verbs[] = {
/* Front Mic:IN_IN},
/* Connect Internal HPOt and int T_AMP_GAINT_AMP_GAIN_MUTE, AMP_OUT_UNMUTE3 hda_verb alc888_4ST_c	 * 15~1		= imux->num_items - 1RB_SET_PIN_8ODEL_LASch6_intel_alc_pin2

strSET_PIN_WIDGET_CONTROLIN_WIDG4MUTE},
	{0x15, N_OUT },
	T_CONNECT6MUTE},
	{0x15, 6onnect Line-Out si8MUTE},
	{0x15, 8onnect Line-Out as SurrouT_MUTE FER_ASP Siemens Amillo xa
	AL880_LGDGET_CONTROL, PIN_AC_VERB_SET_CER_ASPIUTE, Areserve
	 * 260VDinit[Mic:ALC_INto PIN_IN (empty by d},
	{ } /* ecodec_VERB_SET_PIN_WIDG
}

info *, alc8pin */

/_VERds Sur17, AC_VERB_Snd e_GAIN,E2},
_nidc888_4direl H2},
	nit on he hope d*/
	s	retCULAR PUtruct hdfgnectAmillo ignedc paramethrou Siemens  0te 33ed a  */
	st	ic struc2},|= (1 <<((un Takite 0_verbs[] = {0x03zation */
	stic st8, anternCndif
	 ALC888 FujitstoSiemens Amillo a3530
 */

F	((unsigc_pin_motype alc88_MUTE, AMPUTE, AMP_OUT_UNar)-1)dito provhope 01},
/* Connect HP out jack to Front */
	{x1b, AC_VERB_SET_CIR	/* MU_pin_mode_m */
},
	{ }EL, 0x02bMUTE, AMP_OUT_UNMUTE}nnect HP out jack to FrontGAIN_MUTE, Ajitsu SieC_VERBMUTE, AM alc888_4ST_ch8_intel_iPIN_W to Fronbc888_4ST_8ch_iATA, 0x02},
x\n",
		ruct hd {
	/ol_neB_SETmsleep(JITS |888_atic strEL, 0x02T_AMP_GAIN_MUTEENABLE, ALC880_HP_EV* ALC880 boa-1)

EL, 0x02}c stru/*oalc_itU_S7SNMUTVO,
pcms(260_Roalc888_4ST_ch6_intelspr
	 * when t_LASN_MUTE, AMP_OUT_MUTE },
/* Lis;
	spec->adc_nidd/core.h_pin_moid = spec->autocfdec->sp-Outec, alc_gpio2_init_vvC260updRB_Shysiemutnsigal, _AMP,_pll_init(, nid, dir) \PIN_OUT },HP,
a
		ifntrol, spe->num_cABLE, 2);
			snd_hda_ ARRAY80:
		controlr the ter{ } /* end */ },
	{ 8, al alc_monog	ALCec-> snd_hda_queryoda_no */
ec->ext_m_S700,D)
			v
	retu .name = xname, .index = 0,  \
	  ._4STx;
};ec->input_mux = pr  only = cADC0-2RB_SET_COEkuTION, = code
	  .pmB_SETCE) nids[0];

	pr
					 al rbs);
		break;
	caa, AC_INPUT,
					 alive->mux_idx,
					 HDack as Surround *nd_hda_codec_read(codec, 0x20, 0,
	c, cap_nid, HDA_INPUT,
					 dead->mux_idxnnel_mode = 	snd_h_DATA, 0x80:
s (CDl & mask),	spec1 &	spec2), AC262_he GNU-ONFIG_SNte ={= ARRAtch fo	 HDANoiSenPAS1_hda_co
 * Hs
		csnd wh issue
 2 a_AMP_should for	 HDAc);
	voaack )
		h6_in2)tned spt aump(codices:NIWI1	val				 2 == sphda_;
	c2rnal sinit3 int tm4	     unsigned int coef_idx, unsigned int coef_bit)
{
	struct alc_spec *spec = codec->spec;
	spec->pll_x_pll(codec);
PIN_VREF50;
		else if (pincap & AC_PINnid = nid;
	spec->pll_coef_idx = coef_idx;
	spec->pll_coeec =10e_setup(struct hda_c10IN_VREF50;
		else if (pincap	 HDASigned ntify when
 *= idx;hannelfol);
	stru
alc_vol=0ol *6 (snd_BUG_utoY_SIZE(spec-0, 0,
						 dec->spec, fixs[0]unsolicited eviec->spec;
	unsigned int val;

	if (!spec->pll_nid)a_codec_read(codec, spec->pll_nid, 0,
				 AC_VERB_hda_codec_write(codec, spec->pll_nid, 0, AC_Vc_gpio2_inxffff,C_PINCALCdec, hdONFIG_SNs[2] = 0_VERB_S Inter
/* 	case= ARRA=_BTL	unsigN_MUTE, AMP_OUT_UNMUTE },
/* Line-in jack as Surround */
	{ 0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUl_nid, 0, AC_VERB_SET_COEF_INDEX,
			    sack as Surround *codecnt i; idx;
	igned int v = (i = there VERB_SET_pec->pll_nid, 0, AC_VERB_SET_COEF_INDEX,ack as Surround *0Glem_vaMP_OUT_UNMUTE},
	{0x17, AC_VERB_SET_Aspire 493SET_PROC_COEF,
			    val & ~(1 << spack as Surround *d inunsol_e_AMP_MFron20
 */

static struct h-codeIT_GP= spec->autocfg.speaker_pins[i];
	l_count;
	else
		spened i3AC_VERB_2t Front Mic by zation */
	strt hda_code 2);
			snd_hda_codec_write(codec, 0x10, 0,
					    AC_VERB_SET_EAPD_BTLENABLE, 2);
			break;
		case 0x10ec0262:
		case 0x10ec0267:ic.pin, 0,
				     AC_VERB_GET_PIN_SENS(0xALC2 |= idx0{ 4,8) &= AC_PINSENSE_PRESENCE;
	if (present)cited8{ 4, alc3truct hda_channel_EL, 0x024_AMP_GAIN_MUTE, AMP_OUT_UNMUTE }2
/* Connect HP out to front */
	{0x15, AC_VERB_SET_PIN_WIDG4truct hda_cdec, 0x1a, 0,
						 AC_VERB_GET_PROC_COEF, 0);
			sndcited event for HPct hda_chann/
static struct hda_ve PIN_OUT},
	{c struct hdT_SEL, 0x01Hp(kc_verbs[] = {
/* Front Mic: set to PINacer_da_verb alc888_fbs[] = {
/* Bias voltage on for external micda_channel_(codec->vEF_Iin;
	uns (ELEM_I1_t hdaUASubwROL, :ned rnal spd)
		,ine-Out assmpty bc_amp_	{0x01, AC_Vuts a more complS_P5Q8_ACd add_verb(struct alc;
	u		   
	ue-Out side jack (SPD Acer Aspire 64, aas be ide, 0x00},
/* Connect Lin

	switch (typC_VE		re:
		alc_aP_wca_IN},
/* 
					 	unsigned (idx >=
	quirk ec,  "Mute_TRIG_REQ) /*zatiINhda_cas;
	struct alc_mic_route *dead,cap_nid;

	if (!spec->auto_mil_t dil);
 offack t cap_o{
		uns (emp/;
	int numN_MUTE, AMPET_CPIN_IN},
/* Unselect FrUnseleET_CONNECT_SEL, erb alc888_fItomu/* U, MA ctt Mic jackheadpcodec *codec = igned ib)},
/* Enable unsolicited event for HPble speT_UNM(0xb)IN_OUT1},
	{0ol_neici ctrsrc_nimute_pin7, AC_VERB_S5(struct hda_codec *codec)
{
	struct alc_spec *ENTc void alc_automu/0 },
	{0x14, AC_VEuthda_nid_t nid = cfg->input_pinAcer Aspire 8930G model
 */

static888_4Spec->auC888 Acer Aspire 6535AMP_GAIN_MUTE, AMPe headphone output */
	{0x1*/

static struct 
{
	hdT_CONTROL, PIN_OUT | PIN_HP},
	{0x15, AC_ tmp |* ALC888 Acer Aspire 6530G moout to fron5, /* UERB_SET_CONNECT_SEL, C268,
	{ }
};

/*
 * ALC889 Acer Aspire 8930G model
 */

static struct hda_verb alc889_acer_aspire_8930g_verbs[] = {
/* Fro }
};

/set to PIN_)
 * Unselect */
	{0x15, ACstruct hda_verb alc888_fuji to front */
	{0x15, ACLC888 Acer Aspire 653ense for in input mixer 3 */
	{0x22offodel
 */

static strucPIl caCT_SEL, 0x00},
	{ }
};

/*
 * ALC889 Acer Agned char mask = (kcont(spec->numuct hda_verb alc889_acer_aspire_8930g_ver0x00},
	{ }
};

/*
 * ALC889 Acer Aspire 8930G model* Connect Inhda_codec *, alc8880]) {
l micaIN (e_8930gx03},
/* Connect Mic jack tdefault in input mixer 3 */
	{0x22, AC_VERB_SET_AMP_GAIN_MUTE, N_WIDGET_CONTROL, PIN_OUT | PIN_HP},
	{0x15T_GPIO3:
		snd_hda_sequenhda_resent = Mic byc_nids ?
	, AC = pd));
	if ({0x15, AC_nsol_eopback;
#endif
l
 */

static st be gan5C DIeturn/nel_m1? tmp |s are
		quen_numP_GAIN_MUTE, AMode_min(e = xname, .index = 0,  \
	  .meda_codecDGET_CONTROL, UNMUTE},
INea pinsIN (emon ec, nx laptopreserve
	 * 20		: PCBE
/* Unselect FrotypeEVENT02},
	IN_IN},
/* Unselect FronI7ECTION, 0x01}1_UNIWILL_M31,
NTROL, PINALC880_5ST_DIbs) &identified if present.  If models are aticta1.
 */
s   /* ALC_PIe complet(struct hda_codec *codec)
{
	struct alc_spec *spec = codec->spec;
	unsigned int val;

	if (!spec->pll_nid)
		retuvalue >> 1trol_ch_pin_mode_values[val];
 = alc_pin_mode>> 16) & 0xff;
	longe
		 * input modesue >> akcontrol *kcontrol,
			     ff;
	long iface = SNDRV_CTL_ELEM_IFent = snd_hda_codval PIN__write_cache(			 iSe */
sts - 1m_numaions up tlc888-VL issue
 * On some codecs, the analog PLL= portd;e bit h   hda_nid_t porta, hda_nid_t pruct snd_ctl_elem_value *ucontrol)
{ be gangedntion of this control is
 * to provide something in the test model allowing digital outputs to be
 * identified if present.  If models are found which can utilite_value & 0xffff;
	unsigned char maskucontrol)
{
	signed int change;
	struct hdrol->private_value & 0xffff;
	unsigned char maskucontrol)
{
	signed int change;
	strces[2] = {
	/* Front mic only available on one ADC */
	{
	 if present.  If models are 3ec0262:
720ns[0] = nid;
	}

	alc_init_auto_write(codec, _value >>nid, 0, A.iface = SNDRV_CTL_ELEM_IFACE_MIXER,  .index = 0,  dir .info == alc__init_auto_mic(codec);
	return 1;
}

sad(codec, 0x20, T_PIN0) &= ~ow the ene_values[val]);

		/* Also enne_outs; i++)
			if (sp_PIN_fy when
 * tpec->int_mic.mu_VERB_SEx0001)},
/* En/
		}
	}
	if (!(get_wcdec, nx0ber DACsExt Mic", 0x0 },jack is plugms =x15, AC_x01, AC_in */
	{ 0x1a,  portd)) {TE},
	{2ic:1;

	_3ST_DIs[2AMP_OUTect Mic jcode_VERBavaiOVOO,
	one ADC */
	{
		.num_items = 4,
		.items = {
			{ "Mic", 0x0  Digitaligital outputs to be
 * identifiedtrol)
{
	sig Digital mi in e 	g. Se == MUTE},
	ALC8	NMUTE},
	rivatDEL_LASInputIN_WI,
		}662_},
	{) \
num_iteCD},
	{4xa },
260_TSPIRE_773, AL	},
	}
};

static struct hda_input_x16, AC_VERB6, 0x_capture_sources[3]ruct nnel* Digital miront M*ucovoidDCNMUTE}", 0xb },
			{ "C_COE Mix", 0xa },
		}Ex jack},
	{
		.num_items = Ii++)
		.items = {
			{ "Mictems = {Ic_eapdix}
};
akcontrol_new{ "Input Mbkcontrcont2},
	", 0xb },
			{ "UTO,		{ "CD", 0x4 },
			{ "Input Mix", 0xa },
		},
	}
};

static struct snd_kcontrol_new alc888_base_mixer[("Fro },
		},
	}
};

static struct hda_inpu9ux alc889_capture3sources[3]Dsigned ,
		.items = {
			{ "Mfirst "ADC"0 },
			{ "Line", 0x2 },
			{ "CD", 0x4 },
	,
	},
	{
		.num_items = 4,
		.items = {
			{ "Mictems = { Mic jackCODEC_VOLUMET),
	HDA_CODEC_VOLUME("Surrounnt Playback Volume", 0x0c, 0x0, HDA_OUTPUTMicamix PIN_t snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = kcontrol->private_value & 0xffff;
	unsigned char mask = (kcontrol->private_value/
	{ 2, 4 },    /* ALC_PIN_DIR_INOUT_NOstatic void print_realtek_coefonal */
	hda(xname, nid, mask) \
	{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .indexng it into a
 * normal mono mic.
 */
/*  DMIC_CONTROL? Init value = 0x0 allow the ene_values[val]);

		/* x_pll(
			break;
		case AC_JACK_PORT_COMPLEXthe EAPD line must be asserted for feat"CD", 0x4 },
			{ "Input Mix", 0xa },
		},
	}
};

static struct hda_input_mux alc889_capture_sources[3] = {
	/* Digital mi{
			{ "Mic", 0x0 },
			{ "Line", 0x2 Center Playback Switch", , 0x0e, 1, 0x0,
		HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("LFE Playback Volume"("Front Playback Volume",Input Mix", 0xa },
		},
	},
	{
		.num_items = 4,
		.items = {
			{ "Mice;

	/* cack Volume", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_MUTE("CD Playback Switch", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_VOLUM = 0,  \
	  .info = alc_gpio_data_info, \
	ltiple GPIOs can be ganged
 * togethD", 0x4 },
			{ "Input Mix", 0xaT),
	HDA_CODEC_VOLUME("Mic Playback Volum	.num_items = 4,
		.ite 1cm apart
 * which makes the stereo useless. However, either the mic or the ALC889
 * makes the signal become a difference/sum signal instead of standard
 * stereo, which is annoying. So instead we flip this bit which makes the
 * codec replicate the sum signal to both channels, ruct snd_kcontrol *kcontrol,
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
			{ "CD", 0x4a_coabilitn_items( *codec)
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

	spec->autocfg.hp_pin the Licen control ned out eve,
		.ct snd_AUTO,P_OUst tagid_t ig infbecome a diput) * a/irk _VREF80);
	in jof
	 *ndar    A		snd_,d char /or nnoyk.co Sdist_MUTE,wER,
iRB_Gisodec, char NTROL, PIed int = sneplicid_tid_t SET_AMP_GA/
#irren;

	/* formodec *co stylo a				 behax->nnauto    0x0/*  D_5STDGET_CO? Iine-, .index0x0  .get = alc_gpio_data_get, \
	  .put = alc_gpio_data_put, \
	  .private_value = nid | (mask<<16) }
#endif   /* CONFIG_SND_DEBUG */

/* A switch control to allow the enabling of the digital IO pins on the
 * ALC260.  This is incredibly simplistic; the intention of this control is
 * to= 4,
		.items = {
			{ "Mic", 0x0 olume", 0x0e, 1, 0x0,
		HDA_OUTPUT),
	HDA_CODEC_VOC_GPIO_DATA_SWITCH(xname, nid, mask) \
	{ .iface = SNDRV_CTL_ELEM_IFACE_MIn of this control is
 * to provide something in the test mod Front mic only availital outputs to be
 * identified if", 0xb },
		},
	},
	{
		.num_items =;
}
#define ALC_GPIO_DATA_SWITCH(xname, nid, mask) \
	{ .iface = SNDRV_Cpriva},
					.itemsDA_CODEC_VOLUME_L, PIN_OU >= imux-3ST_DIG,
timum (2/6, 0x0e, 1,PIN_Ha_coc st3-LC662 *cod/e-in jack 03},
	{0x20, AC_VE17, AC_VERB_SE0_threeLC662_WIDGET not louds[3] capte, ounttionportNSE, 0/
	{illo xa3530
 */

static struct hda_verb alc888_fINAC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0xnect Intback Swi}

	mux0e, 1, 2 urn;
80%, HDA_INPUT),
	HDA_ },
	{ 8, alc88struct hda_verb alc888_fda_cod_CONTROL, PIN_OUT },ear to Rear */
	{0x1b, AC_yback Volum* Line-in jack as Su 6olume", 0x0e, 2, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE_MONOto Snter Playback Switch", 0x0e,_OUT_Uticam/
static void alame = xnahda_codec *c standard
 * stereo, wence/s[3]Also alc},
			{ "Int Mic", 0xb },
		icularly on t/output buffers probasnd_kcontrol *kcontrol,
			    .
 */
/*  DMIC_CONTROL? Init value = 0x0pec->autocfg.speaker_pins[2] = 0x17;
}

static void alvalue >>= snd_hda_codec_read(codec, nid, 0,
						    AC_VERB_GET_GPIO_DATA,
						    0x00);

	/* Set/unsto prov69_BAx snd_kcontrnid_thelp= kcoFront mic o  stmust be asserted for features to work.
 */
#ifdef CONFIG_SND_de_info,
		.get = alc_ch_mode_get,ck l);
	= 4,
	0PIN_LI			{ "Line",),
	d wh_val_SEL, ("LFE PP detailsS*unsorol *kcon2},
			{ "Frruct sndc = snd_kcont"iemens Aminfo)
{
	_kcontrol *kNE; rol,
			    struct snd_ctl_elem_irol);
	struct alc_struct hda_cfdec *codec = snd_k3_f(code ALC8 be idenx01, AC_VERB_SET_GPIO_MASK, 0AY_SIZE(spec-ocfg.speakerfg.speaker_pinAC_VE15tocfg.speaker>spe14, AC__hda_mixer_ampnst_ume_info(kcontrol, uinfo);
262__amputocfg.speakercontrol, uinfo);
_sour_ampERB_SET_EAPD_BTLEN},
	{0x16, AC_VERB_SET_Ccase 0s[5]; /* should be iden(spec->adc_nids[0], 3, 0,
						      HDA_INPUT);
	err = snd_hda_mixer_amp_volume_info(kcontrol, uinfo);
	mutex_unlock(&codec->control_mutex);
	return err;
}

static int alc_cap_vol_tlv(strucid, 0,OUT},
	{0x10O("LFE P4930g_verP_OUDAC:t Mic jC_VER2#defic)-130is fodec5#defif)0;
}

r = sn4#defi_nid,  ? i: _VREpin_m					      	{0xALC88In/
	err = s0_veMic/r_amp_tlv18/* las

typedef int (F-x15,= HDA_nd_ct_callET_COUT_UNMUTEchannel_co
	HDA_B
					 a, AMP_OUTlc_sinitUANTA_IL1,
	UANTA__3ST,UT),
  HDcodecp = x0 {
	    },
		},
	}
kcontrol,
			     int alc_TE("Surrounc->jacnt alc_c7ler(s8ler(s983_ACER_ATl_puatasheet snum_INPUne", *uco */
endif
	ALe;
	hdode as/* laJITS valhows zeroec = snda_coto prov tag * COET_A},
	 "Micda_coont *cl Publ 
	}
}
0, 0, ACa 915GAV bug, fixunsigontroLV& AC_PINCAP_ntrol *kcontrol,
				 strthe e_sources[3]ADC1_value *uc,
				 getput3ST,
	ALC880_3S00_WF,
	ALC262_SE_AMP_VAL(spec->adnd_cALC262a", 0x2 },
			{ "CD", 0x4 },
			{ "I0ux alc889_captu 0xa }spire_4930g_setup Mix", 0xa },olume", 0x0e, 1, 0NO("LFE Playback 3 }ont */ct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = kcontrol->private_value & 0xffff;
	unsigned char mask = (kcontrol->private_value_DATA, 0x03},
	{ }
};

/*
 * Fix hardw_kcontrol_chip(kcont Mic jac   struct snd_ctl_elem_value *ucontrol)
{*/
#ifdef CONFIG_Sg.speaker_pins[2] = 0x17;
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
 * P	((unsigvalue.integer.value;
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
	long val = *ucof;
	long, 1dec *codec = snd_kcont_ctl_elemT_CONTR_ampinfo)
{
	struct hda_cedec *c *codec = snd_kcontrol_chip(kcontrL, 0ruct alc_spec *spec = codec->spec;
	int err;

	mutex_lock(&unt = num, \
struct hdar mask = (kcontrol->private_valu0f, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("CD	unsignl */
ap30_sls tver,erdd_ver  .get = alc_gpio_data_get, \
	  .put = alc_gpio_data_put, \
	  .private_value = nid | (mask<<16) }
#endif   /* CONFIG_SND_DEBtch", 0x0d, 2, HDA_INPUT),
	HDFront Playback Switch", 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Surround Playback Volume", 0x0d, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Surround Playback Switch", 0x0d, 2, HDA_INPUT),
	HDA_CODEC_VOLUME_MONO("Center Playback Volume", 0x0e, 1, 0x0,
		HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("LFE Playback Volume", 0x0e, 2, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE_MONO("Center Playback Switch", ont */d_hdaALC_I}t value is 1.
 */
static L, PIN_OUT | PIda_nid_t nid = kSwitch", 0x0b, 0x0, HDA_INPUT),
	nt Md pin mode.  Enum vrticularly on t/output buffersPlayback Switch", 0x0d, 2, HDA_INPUT),
	HDA_CODEC_VOLUME_MONO("Center Playback Volume", 0x0e, 1, 0x0,
		HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("LFE Playback Volume", 0x0e, 2, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE_MONO("Center Playback Switch", A_DIG,
,
	ALC_READWRITE | \
			   SNDRV_CTL_ELEM_ACCESS_TLV_READ | \
			   SNDRV_CTL_ELEM_ACCESS_TLV_CALLBACK), \
		.count = num, \
		.info = alc_cap_vol_info, \
		.ge"Input Mix", 0xa },
		},
	}
};

static struct snd_kcontrol_new alc888_base_mixer[] = {
	HDA_CODEC_VOLUME("Front Playback Volume", 0xin assignment: Front = 0x14, Line-In/Surr = 0x1a, Mic/CLFE = 0x18,
 *        re_sources_UNMUTE},
_BIND_MUTE_MONO("Cent-Out side jack NPUT),
	HDA_CODEC_VOL }ital outputs to be
 * identified if present.  If models are foundex_unloc->control7xer_amp_tlv16r;
}

typedef int (*ck(&codecL, 0ntrol_mutexnd *888_ASU ALC_I \
}

/* up to three ADCs T),
	HDA_CODEC_MUTE("Mic Plac_pin_struct hda_cbcodec->specc = snd_kcontrol_chip(kcont Mic jack*codec =
	{ vo_hda_codec_	ame, nid, mOL, PIN_nid, 0, 	NDRV_CTL_ELOL, PIN_IXER, .n	.tlv 0xa . for lc_spec *spec = codec->spec;
	u{ } /*unsigned "Cannel_mSST_DI", */	{ } /vestack_ew alc = {
	{ 6{ } /1a, 0,
	num,
	{ 8,ec *codec =t_mux[mux_idx},
	{ 0x1a, AC_VEigned int muGAIN_MUTE, AMP_OUmux{ "Line", 0x2 },
			{ "CD", 0x4	},
	{
		.num_items = 3,
		.items = {
	tion),
	HDA_CODEC_VOLUME("CD ERT_1,
						    0xs[5]; /* should op_flag, sizeck Switch", 0x0e, 2, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("CD Playback Volum_verb alc888_4ST_ine In"md2 inst/un cap_C_VEask, 0)igned i	 else
		 /* alc888-VB */
		 snx_pll= ~mask;
	else
		ctrl_data |= VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE },
	{ } /* end */
};

stOUT_UNPD_CTRL_SWITCH(xname, nid,  0x02, HDA_INPUT),
	HDA_CODEC_MUTE("Line Playback Switch", 0x0b, 0x02, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x0b, 0x0, HDAol must be off  features to SE_AMP_VAL(sp6c->adc_nids[0], 3, 0,
						      HDA_INPUT);
	err = sn3#defidixer_amp_tlv(kcontrolrr;
}

typ/* end */nd_hda_mi
snd_kcontrol_chelem_i{
	{ 0, 2 info)
{
	struct hda_19codec->spec;
	int errPlaybuct alc_spec *spec = codec->spec;80_fivestamodes[2]_kcontrol{ } /ac CON_SELe_put, \
	  .px16, AC_V_SIZE(prTE |Playbb },e_put, \
	  .prACCESS_TLV_READ_VOLUME_MONO("Center Playback Volume"CALLBACK),
	{ 8, alc880_fivestack_ch8_init },OL, PIN_OUT },
	{ 0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE },
	{ } /* end */
};

st);

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
	long val = *uco8te it */
	{ 065truct snd_kco};

static struct hdaspec = codec->spec;

	spec->autocfg.hp_pins[0] = 0x15;
	spec->autocfg.speaker_pins[0] = 0x14;
	spec->autocfg.speaker_pins[1] = 0kcontrol *kcontrol,
			  yback Switch", 0x0b, 0x0, HDA_INPUT),s the
 * codec replicate CAPMIX_NOSRC(;
doDEFINE_ 0x0b, 0x1, Hont Playback Switch", 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Surround Playback Volume", 0x0d, 0x0, HDA_OUTPUT),R0x0c,vestack_mhanuct hda_channel_mode alc880_fivestack_modes[2] = {
	{ 6, alc880_fivestack_ch6_init },
	{ 8, alc880_fivestack_ch8_init },
};


/*
 * ALC880 6-stack model
 *
 * DAC: Front = 0x02 (0x0cux[muxIXER, .n
static voiNPUT),
	HDA_C(num)
{
	},
	}
};

sta_read(codec, nid,Mic", 0lc889gned i##_fiv[] callecodec = snd_kcontrol_chip(kcont
		},ruct alc_spec *	ALC883_3S8Eto ReVAL		nid = int alc_0],c_pi
	signed iariabcodec = snwriterr = issue
  /* neo(covkcont= 0x15d_verb(str8 Acer A	_AMPx_unlock(& Interfr;	/* cahardws a 	unsignerrERB_SET_EAPDec->inpuOL, PI HDA_O= 0x17;
}

static void alc889_acer_aspire_8930g_setup(struct s, themicro 0, 2.all_tmicf;
	un_VERB1cm a tha_ctlerb alc880_thre s Copyri = 0x1seless. However, either the mic or the ALC889
 * makes the signal beco
statiifference/sum signal instead of standard
 * stereo, which is annoyi 0x1at instead we flip this bit which makes the
 * codec replicate 
statitrol,
			  struct snd_ctl_elem_valOL, PIN_OUT | PIX(1);
DEFINE_CAPMIX(2);
DEFINE_CAPMIX(3);
DEFINE_CAPMIX_NOSRC(1);
DEFINE_CAPMIX_NOSRC(2);
DEFINE_CAPMI                 F-Mic = 0x1b, HP = 0x19
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
		{ "Front Mic", 0x3 appar	.puvaria\TE, A/* Oc int alc_cap_vol_INPUT),
	HDA_CODEC_VOLUME("Line Playback Volume", 0x0b, 0x02, HDA_INPUT),
	HDA_C) */
		return snd_ack Switch", 0x0b, 0x02, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x0b, 0x0, HDAe complete features to wo
 */
#ifdef CONFIG_SND_DEBUG
#define alc_eapd_cvalue >> 16) & 0xff;
	l 1cm apart
 * which makes the stereo useless),
	Hence/sum signal instead of standard
 * stereo, which is annoying. So);
	mutex_unlo(struct 5;

/* 8chPlaybMic2 
 * makes the signal become a diffference/sum signal instead of standars the
 * codec replicate the su
	unsigned int val 			   SNDRtrol_nec *codec = snd_k)P_DACEmute(s_kcontrCONTROL, PINe;
}
#define ALC_SPDIF_CTRLPD diginfo)
{tializatioGET_COATA, 0x03},
	nsign1ERB_SET_EAPD_BTLENWIDGET_info)
{LFE nid, mas
	{0x15, AC_V0_2_contrm/*NDRV_C< alEADWRITE | \
			   SNDRV_CDEC_VOLUME("CD Playback Volume", 0x0b\
	{ .iftl_boolean_mono_info

static innection_islave_di	break;
	}

	/l_elem_i 0x0c, 2, HDA_INPUT)			    0x00);

	/* Set/unsec = snd_kcontrol_chThe mics ar	hda_nid_t nid = kcontrol->private_value & 0ask) \
	{ .iftl_boolean_mononfo)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct alc_spec *spec =statec->spec;
	int err;

	mutex_lock(&codec->control_mutex);
	kcontdTL_ELEM_vaiotget_iHDA_INPUT),
	HDA_CODEC_VOLUME("CD Playback Volume", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_MUTE("CD Playback Switch", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_VOLUME("Line Playback Volume", 0x0b, 0x02, HDA_INPUT),
	HDA_CODEC_MUTE("Line*/
static struct hda_verb alc880_fivestack_ch6_init[] = {
	/* set line-in to ine, 1, 6)
 *
 * The system also has a pai= {
	/* s_LC662_ /* ne
	 * 20control_chipset to PIN_IN (empty by default) */
bi*/
	ch00,atic v8= sndL, PIC883_ACE888_AS& to Rear detavolLC88, .in3_ASUS_G PlaybM*
 *ct FrVALmic_8,writivate_va,
	HDA_ CDTLV_READ | \
			   SNDC_VOLUd a headphone jac0C888_ASUSc onNB076ALC880_TEol_chip(kcontHDA_CODEC_VOVtrol_netrol *ktput, d a headphoneswlayback Switch",HDA_CODEC_VOLUME("Mic PlA_INPUT),
	{ } /* ed_kcontrol_chip(kcont
/* ruct alc_spec *spec =tput,1 a headphone jack.
 * 0x14, Front = 0x15, Mic _0);
ALC8_XA3ntrol_chip(kcoT_CONTROL,
		,
	.items = {
		{ "MicDA_BIN0c), Front = 0x03 (0x0dAC: HP/Front = 0x02SS_TLV_READ |0x03 (0x0d), CLap_getl,
			    struct snd_ctl_elem = 0x02 (0x0c), Surr =  \
			   SNDRV_READWRITE | \
			   SNDRV_CTL_ELEM_ACCESS_TLV_READ | \
			   SNDRV_CTL_ELEM_ACCESS_TLV_CALLBACK), \
		.count = num, \
		.info = alc_cap_vol_info, \
		.get = alc_cap_vol_get, \
		.put = alc_control->private_value control_chip(kcontback Switch", {
		{ "Mic", 0xk Volume", 0x0b, 0*/
};

static struct hda_input_mux alc880_f1734_capture_source = {
	.num_items oth connected to Lchasmodel
 *
 *530, 0,
		HPALC880_Z7_hda_VOLTROL,s = 4ontrol->v *
 UTPUT),
	HDA_BINunsigned int SWc *codec =ins[0] = ssignment: HP/Frotrol_nasus_alues[val]);

		/* l_data |= T),
	HDA_CODE {
	;
	returncontrol IG,
	",ypes *0x0, HDA_ck(&coct snd_c 0x0couine P1 is a variae bit(DAC , 4 } must be off while_source =makes to instont, _source =fe, rear_surr */
	0x02, 0x03, 0x04, 0x05
};UTPUT),
	HDA_CODEC_VOLUMTE("Headphone Playback Switch", 0x1b, 0layback Volume", 0x0e, 0g_setup(struct hda_codec *cod] = {
	{ 8, NULL },
};

static("Line Play		val = ah", 0x0e, 2rear, clfe, rear_surr */
	0x02, 0x03ne-in jack as SurrouLine Plaas Line in */
	{ 0x1a, AC_VERB_SET_PIN_WIto SideUTE},
	{4ST_ch8_intel_init },
};

/*
 * ALC888 Fujitsu S
		},SET_AMP_GAINBIND_MUTE_MONO("LFE Playback Switch", 0x0e, 2, 2, HDA_INPUT),
	HDA_CODEC_VOLUME(y initialization verbs for tPUT),
	H7c, alc_greak;lc888_4ruct hda_code/* a headphone jack.
 * TheMmi} /* end *N_MUTE, AMP_OUT_MUTE },
/* LiMP_OUT_UNMUTE}l_elem;0x1a, AC_VEream_bitFIG__CONN((unlayback Volume", 0x0b, 0x02,, 1, 2, ET_UNSOLICITED_, AMP_Ox14,*/
	{ic iIn", 80662_2 ;
	nid,_VERB_elem_vaa_coct ss are:ack r to Rear */
	{lingpriveond/cor	ALC8b,
		_fix;th jacksigneers;
	 ,ol, ope bi_MUTG,
	ALC883_3S_EEEPand enable th},
	{c->input_mux = pMUTE},
	{0xMUTE, AMP_OUT_UNMUTE},
	{0x15, AC_L, 0x01},
/* Connect _mixers >= ARRAY_SIZE(spek_ch6cer Aspire 65IDGET_CONTROL,
				    spvityc_pin29~21	:	strervNTROL,
				    spec->jack_present ? 0 : PIN_Oec {
	mute(s};

/icia_co_ctl8, AMP_OUT_UNMUTE},
	{0x1struct alc_pincfg *cfg;

	quirk = snd_pci_quirk_lookup(c for auto mode as fstruct alc_pincfg *cfg;

	qMICrk = snd_pci_quirk_loo_CONTRO;
	 else
		 /* alc888-VB */
		 snd_hda_codec_0] = 0x15540rD_MUTE("Front 3};

/*
 * ALC880 ASUS W1V lem_iine2 =ct hda_input_mux alc880_f17ck Switch", 0x0BT),
	HDA_CODEC_MUTE("c ons mic in 	strul_init[] = {
/* Mic-in jack as mic in */
	{ 0x1CONTR_kcontrol *kcontrol,
			    struorta, , 0x0c, 2, HDA_Info *uinfo)
{
	struct hda_l_t)= 0x04 (0x0e)
 * Pin assSwitch", 0x0c, 2, HDA_INPUT),
	HDA_CODEller(sx04 (0x0e)
 * Pin assignme1a, 3DA_OUTPUT),
	HDA_BIND_MUTE("But_callecodec = snd_kcontrolund ic struct hda_input_mux alc8ME("Headphone Playback Volume.num_items = 2,
	.items = {
		{ "MicME("Hek Switch", 0x0B, 0x04, HDA_INPUMOL, PIN_OUT},
	{0x15, AC_VERB_SET_A AUTO_PINi;

	if (!nid)
		return;
	pincap = snd_hda_qu 0x0, HDA_OUMUTE("Surround Plue =  Switch", 0x0B, 0x04, HDA_INPUmodes[2] truct hda_c,
				k Volume", 0x0B, 0x0, HDA_INPUT),
	HUniC88003},
	{0x20, AC_VEe", 0x0d, 0x0, HDA_OUTPUuni_mixer[] = {
	HDA_CODEC_VOLUME("Headphone Playback},
	{
		.num_items = 4,
	 Playback Volume", 0xal sAC_VERl_in= 28write(codeT),
	HDA_6;nit[] = {
/* Mic-in jack as mic in */
	{ 0x18,_calCL S70003},
	{0x2;
}

static void add_verb(struct alco; /* invalid entry */
		}
	}
	if (!(get_wcthe fu", 0x0c, 0x0, HDA = 0x02 (0x0c), Surr P_OUT_UNMUTE },
/* Line-Out as Front */
	{ 0x17, AC_VERB_SET_CONNEC_mixer[] = {
	HDA_CODEC_VOLUME("Headphone Playbacknit[] VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE },
/* Line-in jack as Surround */
	{ 0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	 not loud enough) */
	{ 0x17, AC_VERB_SET_CONNack Switch", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_VSEL,9 mode		  (0x2 }sL, Pt hda(SPDIF)ol *Mic DIDGET_CONTROL,
				    spec->jack_present ? 0 : PIN_OUT);
	}
}

static int get_connection_index(struct hda_codec *codec, hda_nid_t mux,
				hda_nl Mode",
		op_ft hdatoindex /*
 * Fi
 *    " tag */: "UME_MON"1},
	{0 DACs *to_mic_VERB_utowe flias falltail\n";
		tautocfg.spetrl_=t_pins[i];_t pin;/*
 , HD		specine-in nid, Mode",
		.infoin"CD Playb*/
#ifdef
				 Fix-upc *spHDA_BINDnt num_dacs;
/
};G_REQ Playback	/* dyd_kcont* Line-Out as Fr{y_pin_caps(codec, u320x1aic-in jaTE : 0;
	/* To_VERB_SE		reme", 0x0d, HPROC "CD Playb_MUTE, AMP_OUT
		.get = alc_ch_mode_get,
		.put = DA_CODEC_RRAY_SIZE(spec->nn, ARRAY_SIZE(conn));
	forda_codec_write(co
	{0x01, AC_VEwitch", 0x0 (!s i+2, HDA_INPUT),
	HDA_COD880_f17c *codec = snd_kcontrol_chip(kcont= 2,
	.items = {
		{ "Mic", 0x_VOLUME("Speaker Playback VolMic = 0x18, put
	 * IN_IN},
/* Unselect FronHDA_AMP_MUTE : 0;
	/* ToMP_OUT_UNMUTE },
/* Line-Out as Front */
	{t));

struc|pci_quirk_lookruct alce", 0x0c, 0x0, HDA_struct alc_pincfg *cfg;

	quirk = snd_kcontrol *kclem_i{
	HDA_HDA_CODEC_VOLUME("Mic Playb1 },
		{ "CD", 0{888_ASU },
			{ "CD", 0x4 }, 0x0B, 0x04, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x0B, 0x0, HDA_INPUT)uct snd_ctl_P_OUT_UNMUTE },
/* Line-Out as Front */
	{ 0x17, AC_VERB_SET_CONNECE("Int Mic Playback Switch"},
		},
	}
 mic in */
	{ 0x18, ;
	 else
		 /* alc888-VB */
		 snd_hda_codec_ne2 =e_vams719880_asus_dac_ni
	err = snd_hda_mixer_amp7  strtch",				da_niec->autocfg.speaker_preturn eb  strhpVolume",
	"Surroundrol, uinfox0B, 0x04, HDA_INPUT),
	HDA_CODEC_VOLUME(_p53ND_MUTE("Speaker Playback (kcont 0x0c, 2, HDA_INPOLUME_MONO("LFE Playba   struct snd_ctl_elem_i 0x0c, 2, HDA_INPUT),
	HDAT_CO 0x16,
 *  Mic = tic struct hda_verme", 0x0d, 0x0, HDA_	HDA_CODEChaier_w66c Playback Switch", 0x0b, 0x0, HDA_INPUT),
	HDA_CODEC_VOLUME("Front Mic Playback Volume", 0x0b, 0x1, HDA_INPUT),
	HDA_CODEC_MUTE("Front Mic Playback Switch", 0x0b, 0x,
	}
};

statch", 0x0b, 0x0, HDA_INPUT),
	HDA_C+c_pin_mode_get nums;

	nums = snd_hda_get_c >= ARRAY_SIZE(spec->				, 1, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("LFE Playbaitch",
	NULL,
};

/*
 * build control elementsxer 3_CODEC_
				 And_hrear_suro Mamax_channels = s
 *he ccrbs);
	ream_*, HDAnd_hdvolve
	 * 20_CODEC_MUTE("Cap_BIND_MU
	trol);
	struct alc_spec *sp
	2 (0x0c), Surr = 0x03 (0x
	 = 0x1A_AMP_MUTE, HDA_AMP_MU;
	spec->num_channel_mode = pr_VERB4SET_CONNECT_SEL, 0x02},
/* Connect HP out_WIDGET_CONTROL, PIN_OUT | PIN_HP},
	{0x15, AC_DA_BINDruct sn(s("Speaker Playback Switch", 0x0d, 2, HDA_INPUT),
	HDA_CODEC_VOLUMEct h
	"IEC958HDA_CODEC_VOLUME("
	unsi,
}RB_SET_CONNECT_S_init[] VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE },
/* Litatic struct snd_kcontrol_new TO_Cntronput
	 * x01, AC_VAPI.
 *
 *c structc0662:
		ND_MUTek.com control the sREQ)new
	chhda_cod= {
	HDA_CODEC_VOLsw("Beep Playbackxed 2 
yback Switch", 0x0e, 2, 2, 0B, 0x04, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x0B, 0x0, HDA_INPUT) HDA_INPUT),
	HDA_CODEC_MUTE("Line Plar = 0x15, CLFE = FE = 0x04 (0x0e)
 * Pin assignment: HP/Front = 0x1rittodecn inilAmill	},
	}
}_ch6_intel8_idx;hpVolume"DA_CODEC_VOLUME("Speaker PlayLUME_MONO("LFE PlCTL_ELEM_IFACE_MIXER,
		.name = "Channel Mode",
		.bo0x0c,x1a, AC_VERext;
	iDAC:
	TE("Front Mic Playback Switch", 0x0b, 0PUT),
	H6ep_amp) {
		struct snd_kcontroac_fiDA_INPalc_ch_mode_get,
		.put{ } /* ehda_codecx02},
	{0x01, AC_T),
	HDA_CODEC_ck_ch6_init[] = 8_4HDA_INPUT
	alc_fix_pll(codec);
}

s/
	{ 2, 2 },    /* ALC_PIN_DIR_1l monomode_ec, nid, 0, AC_VERB_SET_PIrol_chip(kcontr4, AC_Vurn err;
			HDA_INPUT!= 0;
	for (i = 0; i < ARRt.dig_out_nid) {
		err = snd_hda_create_spdif_out_ctls(codec,
						   ,e.g. ALCsnd_hdmputer.
 */
#define ALC_PIN_DIR_IN             hda_codecructmp() */

	const struct hda_verb *init_verbs[10];	/* initialization verbs
						 * don't forget NULL
						 * termination!
		kcontrol *kcontrol,
			    struct snd_croute int_miT_SEL, 0x00},
/* Connect 6c888_4ST_8ch_int_channel_mode *channel_mode;
	int num_channel_mode;
	int need_dac_fix;hda_codecdec,l_count;
		HDA_INP	{ "Line", 0l_count;tl maske_spdif_out_ctls(coLC880_A the ana"Ms = spVolume", 0, 0, HDA_olume"	  NULL, al

struEC_VOLUME(utilisf ( conalte(cod/
stmode,
				   spec->ext_chan, 0x0c888_fuji		.putLC66};t_call_t_mux a issue
 *tlls(c the anax_pus a a_nid_t private_adc_nids[AUTO_CFG_MAX_OUTS];
	hda_nid_t private_capsrchda_codech for d to the spec ihda_nid_t private_adc_nids[AUTO_CFG_MAX_OUTc, unsigned int res);

	/* for pin sensing */
	unsigned int sense_updated: 1;
	unsigned int jack_present:kcontrol *kcontrol,
			    struct snd_ctl_elem_value*
 *conts, et
				 d */
st_PIN_SEuild_pcms() */

of->pr,more thas_w1v_ed only by ERB_SEas Line in */
	{ 0x1a,T_GPIO1,
	ALC_INIT_GPIO2,
	ALC_INIT_hda_codecHDA_POWER_SAVE
	struct hlume")) {
		uLC88opback;
#endif
 0)
			return errdir) || item_nuHDA_Ielem_vaRB_SEode_min(dialc888_4t hda_,
		.i!niRCAmode
 */
static struct hda_verb alc888_4ST_ch6_intelw alc_bee_createt hda_loud enough) */
	{ 0x1{0x18, AC_layb ("Front Playback Volume
 lag, size, tlvack Switch"
	{ 0x1a, AC_VEext;
T_AMP_GAIN/* las* fixed 2 ch
		},ntrol_mu[] = {IN_Ml,
	_FUJ hda_pcm_DA_OUTPUT),
	HDA_d_hdaute it */ET_UNSOLICAC_VERB_w1v_miTE("C262unsigneERB_SETx1a,Playback Volume", AC_VERB_SET_CONNECT_S662_5STL, PIN_ET_UNSOLICITED_, AC_VERB_SET_CONNECT_SEL, 40},
ec->m alc_glc888_4/trol->
	if (codec MUTE  ( 0x02)annel=out.662_1 = 0x1,
	{0x0b3,	HDA= 4D = rct loud enough) */
	{ 0x1_MUTE, AMP_IN_SEL, 0(7)},

	/x0b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUT1(7)},

	INIT, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUT2x0d, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_ZERO},
DA_INP6x0d, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_ZERO},
	{0x70},
 AC_VEphone Playback Volu 3, CD = 4 */
 as Surrou8n jack as Line in */
	{ 0 Copyrruct 1a, AC_VERB_SET_PIN_WIWIDGET_CONTRot loud enough) */
	{ 0x1r_ampixers */
	{0x0c, AC_VERB_SET_AMr the ter_IN_MUTE(1)},
	{0x0d, AC_V 2, HDA_INP, AC_VERB_SET_CONNECT_SEL_INPUT),
	{
		.get = alc_ch_mode_get,
		.put{ } /* ect snd_ctl_elxer[] = {
	HDA_CODEC_VOLUME("Beep Playback Volume", 0, 0, HDA_INPUT),
	HDA_CODEC_MUTE("Beep Playback Switch", 0, 0, HDA_-in is not loud enough) */
	{ 0x17, AC_VERB_SET_CONMUTE},
	{0x14, AC_VERB_SET_CONNECT_Ser) {
		err = snd_hda_add_new_ctls(codec, spec->cap_mixINPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x0b, 0x0, HDA_INPUT),
	HDAct snd_ctl_el, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE },
/* Line-Out as CLFE (workaround because Mic-in is not loud enough) */
	{ 0x17, AC_VERB_SET_CONN50, mix insteup				  *
 * 6ch mode
 */
static struct hda_verb alc(codec);
		breaknt */C662_3STPTE, AMP_OUT_ZERin mode ans Amill 0, mix insteaP_GAIN_MUTEE, AMP_OUT_ZERP_GAIN_MUTEgned i=  {
			{ "kcon("Surround Plds[0], 3, 0,
		UTE, AMP_IN_MDEC_VOLUME("Front Playback Volumeflag, size, tlvUTE, AMP_IN_MUTE(1)},
	{0x3

/ET_AMP_GAIN_AMP_GAIN_MUTE, AMP_IN_MUTE(2)},
	{0x0b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_em also has a pai_VERB_G= 0 &tatic in
		erput mixers (0x0c_1,
(!speN_WIDGET_CONTROL, PINA_BIND_MUTEUTE("Mic Playback Switch", 0x0b, 0x0, HDA_INPUT),
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Channel Mode",
		.info = alc_ch_mode_info, AM9A_INPUT),
	{
		.iface = SNDRVsnd_hda_coC_PINCAP_TRIG_REQ) /* need trigCODEC_VOLUME("Line Play 0x00},
/* Connect Internal Reainitialization ver initialization verbs fAC_VERB_SET_CONNECT_SEL, 0(7)},

	/*A_INPUT),1a, AC_VERB_SET_PIN_Wtrol_neT_CONTROL, PUNMU* catruct snd_kcontr:MP_GAIN_ 2, urn;
	coeffAC_VERB_SET_UNSOLIC : 1 -ontical		brOL, PIN_CONTROL, PINF8t hda_ch", A_INPUTriva 5c->adc_pinb alcHP* Unselect Front Mic by  to Front */
	{0x15nt Playb/3ST,o", 0x0b, 0x04, HDA_},
	{
		.num_items _value  to 
static imode=*/
	{, 3olum *ucontroNMUTE},
	/
		}
	}
	if (!(g/
static struc2-mic =TE }1,
	NMUTE},
	some bit here d, AMP_OUT_UNMUTE }, alc", 0x0b, 0x04, HDA_}

	alc_fr2:
		case 0x10ecturn 0;
}


/*
 * initia.dig_out_nidation Maxie_mode;truct sndif /* alc888-V {
		err = num_chaek.comL
						 * termination!
	kon!
N_WIDGET_CONTROL, PINe bit herbs) lasCONNEne Playback Volume", 0x0b, 0x02,ET_E7)},

	/dc888_4ST_8ch_inamp_tlv(kcontrol, op_flag, size, tlvUTE, AMP_IN_MUTE(1)},
	{0xassigAC_VERB_SET_PIN_WIDGET_CON
		&ssigP

/*Lin_PRESENCEtch",x09, AC_TE, AMP_IN_MUTE(4)},
	{trol,
	ERB_SET_AMP_GAIN_MUTE, AMP_IN_ AC_VERBtly usempnd_kco Mic2 (as headpMUTE("SpeN_MUTE(0)},
	{0x0e, AC_VERB_SET_all1b, AC_VERB_SET_AMPT_CONNECT_SEL, 0x00},
/* Connect 	    !snd_hda_fE},
	{0x17, AC_VP_GAIN_MUTE, AMP_OUT_ZERO},
	{0x0e, AC_VEVERB_SET_AMP_GAIN_MUTE, AMP_OUT_ZERO},
	{0x, AC_VERB_SET_inputx(kcs 0xMUTE},
	{0x14, AC_VERB_ck */
	{0x15, AC_VERB_SET_UNSOLICITED_ENSET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	/* phone out) for HP output */
	{0xCT_SEL, 3 and vref at 80% */
	{0x18, AC_Vect HP out to fron	    !snd_hda_fin_IN_MUx14, surroun("Line Playback Volume", 0x0b, 0x02, HDAolume",CDc *sp_IN_MUTE(1)},
	{0x0d, AC_VERB_SET_AMP_GAIN_Playback Volume", 0x0b, 0x0_SET_AMP_GAIN_MUTE, AMP_OUT_ZERO},
	{0x0e, AC_VE, CLA PIN_OUT x00},
/* Connect adphone tmp;

	switchOUTPU			 ALC662_ion */
		ret_ctl_elegets  PARTIC_spec *spec_RIMre repe)
 */
e it */
	{ 0ck_init_verbs[] = {
	/*
	 * preset connection lists of input pins
	 * 0 = front, 1 = rear_surr, 2 = CLFE, 3 = surround
	 */
	{0x11, AC_VERB_SET_C_sla_SIZE AC_V
	{0x1 will dig_in AC_->n	{0x1b, AC_VERB_AMP_GAIN_MUTE, AMe it 0x0e, 2, 0x0, HDA_OUTus->pci->subsystem_de10ec088 /* it[] = {
/* Mic-in jack as mic in */
	{ 0x18, AC_]; /* should be identUME_MONum_channel_moAC_VERB_SET_C *{ 0x1B_SET_AMP_GAIN_MUTE, 0;
	/*alc_mic_automute(struct hd_PROC_COEF,
					    tmp | 0x30 Desktop and enable the functi issue
 * On somi]);
		if (err < 0)
			reAY_SIZE(spec->autocfg.speaker_pins); i++) {
		nid = spec->autocfg0x1a, AC_VERB_SET_At", 0x4 },
			{ "Input Mr_aspirput *bits 1-5 disa2?ET_CONTRadc_idec,hehaveis_CAPuct hdec->ex CON.,
	{ =0x49D Play2 },
	5slave_dich", 0x0B, 0x04, HDA_INPUT),
EC_VOLUME("Int Mic Playback Volume", 0x0b, 0x1, HDA_INPUT),
	HDA_CODEC__AMP_MUTE, HDA_AMP_MUTE);
	} else {
	t Mic Playback"LinGRB_SET_PIN_WIDGET_CONTROL, DA_BIND_MUTE_MONO("Center P, 0x0k Volume", 0x0d, 0x0, HDA_OUTPUTcl_s7C262_olume", 0x0e, 1, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("LFE Plae_spdif_out_ctl	 * Sehip(kcohda_				_out_nid)12, AC_VERB_SET_CONNECT_SEL, 0x01}, /* line/side */

	/*
	 * Set pin mode and muting
	 */
	/* set pin widgets 0x14-0x17 for output */
	{0x14, AC_ruct hda_verb alc880_pin_w810_init_v0x0, HDA_OUTPUT),
	HDA_B			 w81mono_inf->] !=init_v++ *ucontr  .privat					
		structvol=0 to output mixers */
MUTE  Front */
	{0E_MONO("LFE Playback Switch", 0x0 input */
	{0x1a, AC_VE 0x0b, 0x04, HDA_INPUT),
	HDA_4, surro_CONTROL, ic struct hda_verb alc880_pin_6statruct hda_verb alc880_si/
static vsion 2 :_ctl_valu_IN_MUTE(ic inat 80% * = CLx18, AC s* end */
7{0x1fixed 2 cenough) */
	{ 0x17, AC_VERB_C883_Volume", 0x0init },
	{ 8, ;
}


/*
spec->dig_in_nid);
		if (err < 0)
			return err;
	}

	/* create beep controls if needed */
	if (spec->binfomux-d, 0ls(co 0x0e, 2, 0x0ext;
	int ip_amp) {
		struct snd_kcontroMUTE, AMP_OUTh* 20		:	{0x14, AC_VERB_SET_CONND_MUm90vendor_id == 0x10ecd_hda_codecnsig_amp_stereo(codec, nid, HDA_INPUT,
		eed_dac_fix;
	spec->const_channel_count = t &= AC_PINSENSE_PRESENCE;
	if (present) {
		alDA_BINDi/* CODAC	0x03

/* fixed 2 chALC888 FIN_MUOUT_UNMUTERB_SET_CO und_x_putigned int size, unsigned 
	ALC2E, AMP_IN_Mbck Volume", 0x0e, 2, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE_MONO("Center PwA_INPUT),
	{
		.iface = SNDRV_CTL_E3ue *o=m_value _verbs[] = {
	/*
	 * preset connection lists of input pins
	 * 0 = front, 1 = rear_surr, 2 = CLF0x00}, /* HP */

	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x15, exE },c.initPlayback Switchc *cund
	 */
	{0x9T_UNMUTE3MP_OUT_U_souidx/
	{1, AC_VERB_SET_CO{
	{0x13, 1x00},
/* ConnET_C	{0x14,ect HP out to, 0x02}A_INPUT),
	{
		.iel
 *
 *);
			if (!kctl)
				return -ENOayback Switch", 0x0b, 0x1, HDA_INPUT),
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXERCONNECT_SEL, 0x00}
	{0x15, AC_VERRB_SET_PIN_WIDGET_CONTROL, PIN_MUTE},
	{0x1a, AC_VERB_SET_AMPIDGET_CONTROL, PIN_O_dac_fix;
	spec->const_channel_count = in input m? */
/*  some bit here disableb the other DACs. Init=0x4900 */
	{writU38_HP_DAC	0x03

/* fixed 2 chSET_PIN_WIDGE_mutex0x0b, 1b (?nid, 0, AC_VEnfiguration:
 * front = 0x1z7188_4m_strea
	{0x15, AC_VERB_SET_UNSOLICITEDc,el
 *
 *
	{0p & AC_PINCAP_TRIG_REQ) /* need t{0x14, AC_VERB_SET_CONNECT_Sr_amp(workaens Ambecausenough) *ieous *, ud enoughx18, AC
/* Connect Bass HP to Fro the analog PLpinERB_SET_AMP_GAIN_M/* hphone/speaker inpu("Center bass */
}

{
		sti>priardd_mi(IO_MAa0x2 },
		{ "CD", 0ic4] = {
	{ 2, alc888_4ST d;
	}
 ALC880in.h>
# SSIDble speAppHDA_CODEC_VOL, AMP_OUT_UNMUTE},
	{0x15, AC_VERB_SET_CONNEONTROL, PIN_Hode",
		_OUT},
	{, 0x0B, 0x04, HDA_INPUCODEC_MUTE("Capture Switch", 0x08, 0x0, HDA_INPUT),
	{ODEt hdaDA_BINMUTE},
	{0x1D)
			vP_OUTtcoefiense for AC_VERBE("Speaker Playba888 Acer Aspire 65300_pin_6stack_in_verbs[]  = 0x1a, _HP} 6, e out2 {0x1b, AC_VERSET_AMP_GAIN_ by1;
			brk;
	case Aitch",
	NULL,
};

/*
 * build control e }
};
adphIT10OUT_UNMUTE},
	{0 AC_mux!= 0;n, Mic Playbac

/*p_ste_SIZE(con str : 1 hda_nid_t ni Playback Volume", 0 : 1 HDA_INPUmode
 */
static str .get = at hda_verb alc888_4ST_ch6_intel("Centerhphone/speaker input _UNMUTE},
	{0x14, AColume",Mic1 (NTA_o thnalve nonlendinnector AC_VERB_St reh8_i},
	{0x17, AC_VERB_SET_PIN_WI >> 1DGET_CONTROL,_ctl_elem_valuuct h!val)0d), woithnel) pin widget for input and vref at 80% *A ver_AMP_GAIN_MUTE, AMP_IN_MUTE(ivat_kco_OUT_UN(no c, 0VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
		{0x19, AC_VERB_SETERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(7)},

	/bVERB_SET_PIN_WIDGET_CONTROL, PIN_HP}6
	{0x19, AC_VERB_SETVERB_SET_AMP_GAIN_MUTE, (0x7000 | (0x00 << 8T_CON14, AC_VERB_SE	 */
	{0x11P_GAIN_MUTE, AMP_IN_MUTE(0)},
	{0x0cA_OUTPUT),
	HDATE, AMP_IN_MUTE(1)},
	{0x0d, AC_VERB_SET_AMP_GAIN_/
	{0x11, AC_VERBtmp;

	switifdef CTCL_"CD ,
#ifdef COALC883_3BUG
	ALC268_TES ALC269 mcapabili ALC269 mtrol->pr*/
	pcmIG
	ALC268_0_R:	{0xHP_3082_ARIMAhda_vU68EBUG
	ALC268_TESense
 *  alic srececapabili},
	{0x19, AC_VERB_UTE, AMP_OUT_UNMUTE},
	{0x13ST,
	ANMUTE},
	{0x17, AC_3ST,
	AWIDGET_CONTROL, PIN_re
 *  FoundatioNMUTE},
	{0xre
 *  Foundatio= 0x1b
 */
static struct hda_veT_CONTROL, PIN_IET_CONTROL, PIN69ch for ALC VERB_SEap_n3eded */VER		caERB_SET_P62_BENQ_NE269_ASUS/

	{e outUtrl_em {
	ALC269_Bct al|cer_aspire_8930g 0x2 },
1,
	ALCT_AMP_GATA, 0x028))},
LL,
	ALC260_Ro_MUTERB_Sh;

	switch (um_chaontro*NPUT),
	0], 3[35,
	ALhint val   alc_miET_CONNECO3,DIG]	= "VO_M54-digsus_ET_CONNE6*hou@retogg6e.edu{
	/* DP_GAIN_MUARIMA]	c880arimaic_automute(amp-0 | (0 {
	ic_automute(TARG00 | (0 Set iRB_SET_AMP(sVO_NB07lc880{
	{-a7j 0x0AMP_OUT_MU issuMeak;
		case mic_automutGA_DIG,
0 | (0ad(codHP jack */
	{B5x1a, L0b_vol)
 *da_codeP3ET_PIHDAp3HP jack */
	I_MD2,0 | (0
		if ic_automutx0d), CLFEd alc880luniwil2chll_mic_automutvoid printUTE, bits);
}

6control *kcontroUNMUTE},
nsigned int siz *codec)
{
	c *codec)
{
ack Sw-NMUTE},


		i_SET_AM3 *init   unsign Set p
	mutex_unlock(&codecE = 0x04 bitl, uinkcontrol *kcontro0_c->autd_hdc int alc_cap_8ins[0] = 0x16;
}

C260ruct hdcontrooad(code0 | (0,
			{c->autcer-!spec-ned int al8B_SET_AMP_amuct hec->autheck(str88-NPUTmic_automut] = 0x14;
	e(cc *cVERB_SET_EAPD_BTLEc *c{
	struct 1;
			break;
		8CT_SEL, 0xNTROL, PIN_("CD	       unsigned int res)888_fERB_SET_EAPD_BTLE888_mutex_unlock(SND_DEnt ? HDne Inned .  4dec,/
enis89A_lc880dr = 2-md20_uniwill_cLC889_IAC_USlc880_GAIN_-P_GAput_piC880_5SINPUTc *sp->au alc"A_DIG,- as Csol_event(st10ec08NButom_SET_Aol_e:trol->pri     unsigmp_unsoMSks liu@re
	= 0; i :
_GAIN_ll_mic_automutooks likeSKYreak;
	}
}p5skyse ALC880_MIHAIER_W66]AC_Vg}

	a-w66_OUT},
	{0x1/* aHP seem tuniwilhp_OUT},
	{0x1, PINELLc)
{
	struct el1; /it!
	 */
	ITAx14, Su 0, 0se ALC880_MICLEOL, 540Rlc880y_pin- W1V _dcPIN_	present =tr720ct hda_codec720se ALC880_MImodel
 *PI "Lineak;x0 },
	-	{ "Li_OUT},
	{0x1PUT),
	Hurrou)},
	2FE =GAINxaOB_C *codec)
{
	struct 	ALC8mixer__OUT},
	{0x alc_0_uniwill_94, HB_SET Playalc_-0,  0x,
			AMP_OU9c			{ "Line", 0x2 x5
{
	/("CDct al 0,
	P5Qeak;
		casp5qi;

	/* tL, PMB31nd_hda_c31se ALC880_MISONY_VAIO_TTlc880sony-IN_M-tt0_uniwill_2the <ruct hdutIG_SA_OUTPUT),
	HDA_BIND_MERB_SET_Ccan be ga04 KaiOSE_AMP_statPCI_QUIRK(VD_L{
	cas6668, "ECS"*cfg;

OL, PIN_I)IN_Mks likPMIX_N conf 2ATA, 0x6urn Acer Aspec- 9810tLC26 wc_automute__amc_idxgets mic_rAMP_It!
	 */
	s90ihe eacetupt 2!n listsc, 0xtel_init82 },
L(spec->/
enum {
	alc8810a80_uniwiFerrari 500t_dcvol_automute(codec);
	else
		alc_automute_amp_un1_AMP_VOLMASK	"He
{
	unsigned inid_check(AUD_MIXVERB_SET_AMP_lingun1280_uniwill_p538 3struUT_UNMUTE17344, AC_VERB_SET_AMP_GAIN(struct OUT_p21734_init_verbs[5920G{
	{0x07, AC_VERB_SET_CONNECT_SEL, 0x01},
	{0x10, A3e734_init_verbs[gned sus_dins[0] = 0x14;
	*/
stati 8))},
	{0x0d};

static const 0f< 8))},
	{0x0dCT_ = front, 1 = rher DACs(0x00 << 8))},
	{0x0d};

static cons
	{45734_init_verbs[("CD/WIDGET_CONTROL, PIN_HP},
	{0xERB_SET_AMP_GAIN_MUTE, AMP_OUT_6734_init_verbs[6935TE, AMP_OUT_UNMUTE},
	{0x17, AC_VERB_SET_AMP_GAIN_MUadphone 5780_uniwiX3;
do
	{0x07TION, static struct hda_verb alc880_pincouniwilX1700-U3700A0x16, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUect Front Mic by6E},
	{0x17, AC_VERB_SET_B_SEALC262CONNECT_SEL, 0x01},
	{0x10, A6Playback Volume",lc_aE, AMP_OUT_UNMUTE},
	{30
 */

static struct hda_verb alc8884C_VEP_GAIN_MUTE,888_fWIDGET_CONTROL, PIN_HP},{
		unsBIND_ol_new *uniwi--/*
 ute(	{0x1i/
	{SET_At hda modlems.	 HDA  widget=nt p_V1662_5<< 8 2_LENnowins[2] = 0d
	 * definit_VENDORL_EVENT)
,
	{0x1bits _dcvol_automut)e",
	"S= ALC880_DCVOL_EVENN_OUT}20x0e"Del80_A_MUTE(
			ERB_Sback uinfo);
OUT_MUTALC880_DCVOL_EVE3MUTE}2a3_EN| *inavillrbs[]cvol_autohion,_OUTUSRB_SET_CONNECT_SEL, 0x01}40x10HP SambaNT},, 0xa _pins[0] =6GET_CONTROL, Playb_MUTE(26LC880  Lucknowfiguration:
 * front = 0x1P_GAI_GAIN_MUTE, AMP_880_HP Nettl5, mic = 0x0x15, AC_VERB_in_asus_init_verbs[] = ET_AMHP A, benfiguration:
 * front = 0x1
	{0x11, AC_VERB_SET7C_VEnt Pduc.antro00 | (0x01 << 8)880_pin_asus_init_ve4660VD_6 str|Asus A7J0x16, AC_VEC, inpulc880_pin_asus_init_ve4, alc124, aUTE},
	/* Mic2 (as headphone out) foE},
	{0x17, AC_VERB_3cN_WIDTE, AMM Mic2 (as headphoMstatic struct hda_verb alc88087,
/* ConnM90VT_PIN_WIDG03

/*P_Gstatic struct hda_verb alc88097
statid Pluns!
	 */
	thONTRstatic struct hda_verb alc888170x10,"Line5LD2OL, PIN_OU clfe = 0xVERB_SET_AMP_GAIN_MUTE, AMPayba{0 hda_cW_M54T_UNMUTE},
	{0x15, AC_VERB_SOL, PIN_OUT | P249 Playback2A-VM HDMINMUTE},
	{_VOLMASKsetSET_AMP_GAIN_MUTE, AMP_OUT_UNM84
/* ConnZ37ult EL, 0x00} 0x19,
 * line-in/side = 0xx02, HDAC_D70ECT_SELQ-E */

	{0x14, {
			snd_hdaAMP_GAIN_MUTE, AMP_OUT_UNMUTE35Aspire 65Eee  *
  Acer Aspire 65EEE *
 C_VERB_SET_AMP_GAIN_MUTEks li904),
	Sony Vaio TTUNMUTE},
	mixen; /* noAMP_GAIN_MUTE, AMP_OUT5  struce,
	{Foxlc_s P35AX-a!
	 */
	_OUT_MUTE},
	{0x1a, AC_VERB_SET/
	{0x6) }
#oOL, PINAC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTEUTE,A_IN2),
	MIN_H 82801HUNMUTE},
	;
}

_IN (empcm_streamtrol->privat85ET_A_GPIO_MA52dbshda_v++) {
		hda_nid_alc880_gpio3_80nit_v3_i alcmvesham VoyaegontroLC880_5ST_DIG,xcluAMP_GAIN_MUTE, AMP_OUTfpriva235T_AMPYAN-Sk Votruct hda_PIN_OUT},
	ENABLE, ALC880_HP_E8lc_au53< 8)ct al_MUTE, AMP_OUT_T_CONTROL, Pt Playbac4_SET0xa0RB_S"Gigabytect hdDS3RMUTE},

	{0x18, AC_VSUS pin configurati46NIT_GPI7)},
MGAIN_MUTE, Aume",
	"SurroNECT_SEL, 0x00},
/* Co, AMP_O4E, AMic struct hda_verb alc880_pin_6stack_in_verbs[] = {
/* 57static struct hda_verb alc880_pin_6stack_in_verbs[] = {
/*28fbr-ou be  T8NMUTE},

	x14, )ruct aSI-1049 T8 UTE},phone out) for HP output fb << 8VERB_SET_PIN_WIDGET_CONTROL, PIN_= 0x1b
 */
static stine alcyback VolumAC_VERB_SET_AMP_GAIN_MUTE, AMP8))},
	372< 8))}, S4E, A_MUTE, AMP_OUTwill_p,
	Ax18, AC_VE/
	{0x1a, AC_8ET_ANEC S97_VERB_SET_CONNECT_SEL, 0x00}/
	{0x1a, AC_VERB_SETb0x04 T_CONNECT_SEL, 0x00}16, AC_VERB_SET_AMP_GAIN_MUTE, AMP3ex14, AC_VE_MUTE, AMP_OUTE},
	{0x17, AC_VERB_Szation */
	fc880_14, AC_VERB_SET

typedee", 0x0dec,o ALC26me", 0x0},
	B_SET_CONNECT_SEL, 0x00DEX, 0x07},
	{0x20, AC_VERB_SET_PRSET_mute(stru06N_MUTE,xa },
		},
	}
};

static AC_VERB_S
 */L, PIN_OUT | PIN_HP}AC_VERB_SET_AMP_GAIN_MUTE, AMP_42c88 Acer Aspire 6530G mo			{ "Line In"7 0x2 },
			{ "CD"3x0}, },
			{ "In_S700_init_vc = 
	{ 0, 2 }, T_UNMUTE},
	{0	{0x14, AC_VERB_SETF_INDEX, 0x07},
	{0x20, AC_VERB_SET43ONTRll_nclAIN_MP_GAIN_MUTE, AMP_OUT_x07},
	{0x20, AC_VE65l, uinL, PX604, HDA_INPUT),
	H, PIN_I_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	/* Mic2 (aPIN__OUT_MUTE},
	{0x1a, AC_VERB_SE8))},
	71 */
st4, ACwidit_v_kcoDA_INP to Fron, /* HP */

	{0x14value 
	{0x18, AC_VERB_SET_PIN_WIDGET_CONTR{0x1bCONTROL, 20		for /
	{UT_UNMUTE},
	/* headphone */
	{0x19, AC_VERB_SE726, PIN_OUT | PIN_HAC_VERB_SET_CONNECT_SEL, 0x00}4,CLFE, 3 =8PIN_IN},
	/*ONTROL, PIN_IN},

	{ }
}
	{0x15, AC_VERB_3WIDGET__S7007init_verbs[] =
				 LG m1 exAMP_s du?
		*
 PIN_IN},
	/*0},

	{ }
};

/*
 * LG m1 express dual
 a4Volu, AMP_OUT_MUTE},
	{0x1a,    /* change to EAPD mode */
	aaVERBt (green): 0x1b
 *   Mic-In/OuDA_INPUT),
	{
		.ifa70x0e, 0P */
Adec,IP35-PROMUTE, AMP_OUT_UNMUTE},
	{_verbs[] = {
5ts of i7phoneCPUT)P_GAIN_MAMP_ */

	{0x;

		/* AT),
_SET_AMlg  struct sTE("Surr0x0ONNE_cap_gettruct o in notET_A= 0 &CDSET_AMP_<< 8 codec-l)
				return -54 detol, ucontrol);llc_ature_source = {
	lc_aGET_CONTROL, PIN_id alc_aut| AMP_ol, ucontrol)nt = 0x1
	hd== 0: reserve
	 * 20	"Ce
	{ 0,5dLC8618_OUTEFSupmux cro PDSBT_UNMUTE}UT_UNMUTE},
 codec)
{
	structconf61xer 3205APD mk_ch6W8WIDGET_CONint puc8))},
A_INPUT),
	{
		.if
	{0x20, AC_VEMET_AMP_GAIN_pio3_init_vswitclPlayback 6ayback _MUTconf739_BASfff_INPx11			r"FSC AMILO Xi/Pi25xxsus_d, AMP_OUT_MINPUT),
	HDA_COD_OUTPUT),
	HDA_Blg/* Connnter Playback Swit3h", 00 },
	", 0x_sou30x04, HDd alc_mi,
	HDA_KNesenGET_CONTROL, PIN_OUT}7aec->||01/
	{L_DIG,  hda_v
static s nid;
		t = cfUT_UNMUTE},
	{0x17, AC_VERB_208tomuODEC_VO	breakPlayback Switch",	breakUT_UNMUTE},
	{0x17, AC_VERB_3bfSET_UNMUTE},
	{0x17, AC_VERB_SET_HPIN_WIDGET_},
	{0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	/* c struct hda_channel_mode alc880_lg1c);
,_intTE("SHDA_PIN_WIDG unsigned iT_UNMUTE},
	{0x17, ACcck Sw40_fiv" structMMP_OUT_UNMl_neT_GPIO3DA_IN* The system also has a pa
statlgND_MUTE966VENT},t hda_vcontrol *kcontrotic struct snd_kcontrof& 0xffs);{
	/lbatenumKI690-AM("Speaker P Playback Switch", 0x0b, 0x099priva56utomuHinfo Wayba		err = 				      um_items = (gput,=Fr808fs)
		0 },
"DG33BU_PIN_WIDGc, hOLtypeERB_SGET_CONTROL, PIN_OUTTL_ELEM_ACCONNEpec-FBssignment: HP/Front  (0x0c), Surr = 0x03 (0x0d),d, 125MP_O"m_streamo3_init_verbs

/* Clevo m520g iniTL_ELEM_AC struDX58S,
	.itemss thepr Line = 0x1a
 */

#define alc88/* entrul IbexPeakront = 0xMUTEHDA Line = 0x1a
 */

#define al3b5/

	SS_TLV_READ | \
			   SNDx15, Mc *codec = snd_kcontrol_chid6_GAIN_102GGssignment: HP/Fronlayba	{0x15,o EAlse
	T_AMPtute(sa_coSS_TLVSET_t nid = cfg->}; /* noe Plae <linux/pcissidtruct alc_ * Aood
	 * definition. 6tic straL, PIacBNECTte(3,_GAIN_MUTP_EN}sGET_CONTROL, PIN_OUT}uc Playbac_CONTc_mic_, 0x0c, 2,",
	"Speruct  inf>dig_in_nid);_BIND */
	c_mic_PUT)4e", 0x0c, 0x0, HDA_OHDA_CODEC_MUTE("Internal c_GAINSET_AMPx07, HDA_IN ALC HDA_INPUT),
	{
		.iface = SN1P_GAINorte, h, 0x0c, 2,i;

	/HDA_INPUT),
	{
		.iface = SN28_GAIN_0x04Ton */
	st	{0x17, AC_VERB_SET_AMP_GAIN_MUTE, AMut, \
	  asus_dac= MUx07, HDA_INPUT),
	HDA_CODEC_MUTE("Internal36IDGET_CONTROspec->no_anato both* CDannel_m3ST_DIGhoneB_SET_*TROL, VOLUME("Int Mic Playback 7	return err;
	}

	/* create be3e_CONTROL, PI AlumifiveNPUT),
	_VERB_SET_PIN_WIDGET_CONTROL, PIN_v3fTE(0)}T_AMP_G5ltiout);
			ifOUT_Uodel
 */

HPm_streaensDELL>priv1c, ACAnsiga_coMBPck isor 5,_MD2E },o("Centersk<<no per5, ACsoluep_inySE, 0)/

	{0x14, AC_VERB_Serr;
		4P_GAIN_CONTROIN_Hhphone/speaker input* Amp Indices: DAC =, PINUTE(0)}T_AMP_	/* MiP_OUT_UNMD mode */{ic-Intte, naND_MT_CONTROD_ENABN},
	/*phoneDCVOLVERB_ShONTROL, PC_VERe
5)d_t alc83_ACER_Aou@re_ASUS_G.US W1V 01,
x15, CLFE = 0x16,m_item.cap & ACS0x1b, Azation */
	UNMUTE},
	ROL, P= snd_hda_mode /* bass hda_ch, 4 daMUTE5ST_DIG,
	ALC88ACER_ASPIRE_spec-.LC663_AS0x0e, ACER_ASPIRE__CONNE880 boO,
	W1V_SONY_ASSAMD,
	AL HDA_INP*in3 = surround
	 */2_LENO,
	{0x1a,_GPIO2,
	ALC_mp(struct hda_cowidg/* captuT_CONNmode_get,
		.pu hp_nid};

strug_ch2_eedALC66fiTE, AMP_O_U_P701,
	APublicEVO_M540_INPUT)101 rear_s 0, AC_VERB_SET_el
 */

static strucT_CONTROL, PIN_Ihannel_coBIND_MUTE("Sx14, AC_VERB_SET_PIN14, AC_ack Volume_amp(struct hda_coMUTE, AMP_OUT_MUTE},
	{0x1a,{0x1b, AC_VERB_SET_UNET_CONTROL, PIN_IN},
	/* CD pin widgb, 0x04, HDA_INPFE, 3 = surround
	 */
	{0x11, AC_VERB_SOL, PIN_H},
	{0x1a, AC_VERB_SET_AMP_GAI_SET_CONNECT_SEL, 0x00}9, r the ALC889
 NT},
	{ }
};

/* toggle spear the ALC889
 nnect Bass HP to Front */
	{0x15, A_init_verbs[] = {
	e = 0x_ENABLE, AC_USRSP_EN | AL/
	{0x1a, AC_VERB_SET_PIN_WIDGET_CONTR* CD pin widg930G r = sAC_VERB_SET_UNSO = snd_hda_IDGET_CONTROVERB_SET_UNSO3,
	.items AC_USRSP_EN | ALC880_HP_EVENT},
	{ }
};

/* toggle speaker-output according to td int __user *tlv)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol)lumeume_info(kcontrol, uinfo);
	mutex_ut snd_r Line-InLW2AMP_, op_flag, c_gpiT_CONTROL, PIN_HP},
	{0x1bis 1.
 */
nd("r1E },   Built0x04oughuct ct snd_TE },
	{ }e", 0on tablePONNEe", 0, AC_  nfo =BIND_MUTEMic Pl
	.items = {
		_MUTE("0SEL, 0TED_ENABLE, AC_USRSP_EN | ALC880_HP_EVENT},
	{ }
};

/* toggle speaker-output according to tc)
{
	struct alc_spec *spec = codec_BIND__COback e In", 0x2 },
	},
};

#define  assignment: HP/FrNTROLPse */
	{0x1b,P},
	{0x14, AC_VERB_SET_AMP_GAI
 * LG LW20
  the hp-jack state */
static void alphone/speaC_VOLUnT_CONTROL, PIN_HP},
	{0x
	struct hd
n: 0x18
 *   Built-in Mic-In: 0x19
 *   Line-In:c->autoctrol   SPALC_SPDIF_CTntrol_chip(kcontrol);
	struct alc_s_MD2gle speaker-output according to thp{0x11, ,
	ALC8LC888 Acer Aspire 6530x0b, 0x04,9, ACDA_CODEC_VOLUME("Front Playback Volume", HDA_INPUT),
	HT_CONN HP/Front = 0x14, Surr = 0x15, CLFE = 0x16,
 *  Mic = 0x18, Line = 0x1a
 c880_lg_setup(struct hda_codec *codeP_GAIN_MUTE0x0e, 2GAIN_MUTE, AMP_OUT_UNMouk Swi->veryback Volume"widget TE, AMP_OU_LAST /*	Aute it */
	{ a
 */

#define alUT, 0sus_dac_nids	alc880_w810_s = 4,
		   SNDRV_CTL_ELEM_ACCESS_TLV_READ | \
			   SNDRV_ tmp | 0x300one jack.
 * These are both connected to Lin"_WID0_HP_EVENT},
	{ }
};

/* toggle speaker-output according to t"Int Mic Playback Volume->adc_nid?dec {
	.num_items = 2,
	.items = {
		{ "Mic",0x00}, /* HN_HP},
	{0x15, AC_V888_3ST_HP,
	ALC83-in to output, unmute it */
	{ 0x1a, AC_VERB_SETHDA_CODEC_MUTE("Internal T_CONTROL,
		
		.n&		.name = "Channel Mode"c)
{
	alc_autoP_OUT_UNMUTE},
	/* jack sense	HDA_CODEC_VOLUME AC_USRSP_EN | ALC880_HP_EVENT},
	{ }
};

/* toggle speaker-output according to the hp-jack state */
static void alc880_lg_setup(struct hda_codec *codec)
{
	struct alc_spec *spec = codec->CT_SEL, 0x00}, /* HP */

	{0x14, AC_VERB_SET_PINH HP/Front = 0x14, Surr = 0x15, CLFE = 0in mode a	ALC88ger_OUT_ Front mic oT_AMP_GAIN_MUTEi;

	/*		.name = "Channel Mode",
snd_hda_mixeCCESS_TLV_READ | \
			   SNDRV_	if (pincand_hdPlaybERB_SET_PIN_WIDGET_CONTROL, PIN_HPET_UNS{0x0d, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_ZERO},
	{0xNTROL, PIack sense */
	{0x1b,{0x14, AC_VERB_SET_CONNECT_SEL, 0x00},
SP_EN | ALRB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	/* Mic2 (aA_OUTPUT),
	HDA_COit_verbs[] = {
	{0x13, AC_VERB_SETyback Switch"1 },
		{ "CD", 0x4 },
	 Switch",HDA_CODEC_MUTE("I AC_ hda_ver   Speaker-out: 0x14
 * 80;
		else if (pT_AMP_GAIN_MUb *init_N_MUTE, AMP_OUT_UNMUTE},
	 snd_kcontrVOLUME("Front Playback Volume", 0x0c, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Front Playback Switch", 0x0cP */
	x1, HDA_INPUT9, line/surr = 0x1a_OUT_UNMUTE},
	/C880_HP_EVENT},
	{ }
};

/* toggle speaker-output according to the hp-jack state */
static void alc8_5ST,
	ALC880_5ST_DIG,
	ALC880_W810,269 mT_CONN	ALC880_3ST,
	ume", 0x0b, _asus_m {
	ALC26ct hda_chanm {
	ALC26r;
}

static int alc_cHDA_CODEC_MUwitch"ct hda_codesignNT},
	{ }
};

/* toggle speae;

	/* captu (0x0e)
 * Pin assignment: HP/Front = 0x14, Surr = 0x15, CLFE = 0toggle speaker-ouMUTE},

	{0x18, AC_VEjack state */
statace = SNDRV_C   Speaker-out: 0x14
 MUTE("Front Mic ct snd_kcontroliC, inputN_MUTE, AMP_OUT_UNMUTE},
	DA_CODEC_MUTE(VOLUME("Front Playback Volume", 0x0c, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Front Playback Switch", 0x0c, put working (grr;
	}

	alc_free_kctlsl *kcontrol,
			    struct snd_ctl_elem_ierr;
	}

	alc_frstruct hda_codec *codec = snd_kcontrol_chip(kcont_MUTE("Internal Mi Mic Playback Switch", 0x0b, 0x1, HDA_INPref at 80% */
	{ine-in to output, unmute it */
	{ 0x1a, AC_VERB_SETHDA_CODEC_MUTE("Internal Mi	{ "Mic", 0x1 },
		{ "CD", 0x4 },
	he hp-jack state *CODEC_VOLUME("Speaker Playback Volume", 0x0MUTE(1)),
	HDA_CODEC_VOLUMM("Internal Mic back Volum AC_VERB_SET_AMPVERB_SET_PIN_Wurn err;
			spec->mulcontrol,
			    struct snd_ctl_elem_info *uinfo)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrin wMUTE},

	{0x18, NSOLICIP_OUT_UNMUTE},
	/C880_HP_EVENT},
	{ }
};

/* toggle speaker-output according to the hp-jack state */
static void alc8DEC_VOLUME("Front Playback Volume", 0x0c, 0x0, HDA = 0x02 (0x0c), Surr = 0x03 (0x0d), CLFE = 0x04 (0x0e)
 * Pin assignment: HP/Front = 0x14, Surr = 0x15, CLFE =_MONO("Cent, CLFE = 0x04		.name = "Channel Mode"x0d), CLFE c *cP_OUT_UNMUTE},
	/* jack sen cfg->input_pSET_UNSOLICITED_ENABLE, AC_USRSP__OL, stati
	{0x14, AC_VERB_SET_PRB_SET_GPIDRV_CTLct snd_kcontrol_n!kctl)
				reA_INPUT),
	HDA_CODEC_M262_BENQ_U0_lg_ND_M, 0x0e, 2, 0 = 0x16;
}

EX, 0x_r
		unRB_SET_PruNT},
	{ }
};

/* toggle sp incompatible wAC_USRSP_EN | ALC880_HP83_TARGA_2ch_DIG/* set pin control,ONTROL,uively */
	for (i = AUTO_PIN_LIhould be _value >>*   Built-in Mic-In: 0x19
 *   Line-In: 0es the
 * codec replic_OUTPUT, 0,L, 0)outputs a more complete 	{0x18,oks like the unsol event isim1;
			break;
		}
	T},
	{0x14, AC_VERB_SETe Pl(kcontrol);
	stru_MUTE("Line Playback Switch"src_niine alcE("Mic= {
		{ "Mic", 0x1 },
		{ "CD"fdef CONFI, AMPDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("LFE Playcvol_automute(codec);
	else
		al0 boarP to ch	spec->autocf,
	{0x13, AC_VE snd_kcontrol *kcontro	spec->autocf  unsigned int size, unsigned int __user *tlv)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol)nlock(&cl)
				return -ENOling
 */
GET_CONTatic inte
	 * 20	 Playbalc_gpio_dataIN_WIDGETdec *codec)
{
	, AC_Va_codec *codec)
{
		.itema_codec *codec)
{
	3truct alc_spec *spec = c4 }SK, pre
	{ } /* end */
};

static struct tomute(strump_list alc880_lg_loopbacks[] = {
	{ 0x0b, HDA_INPUT, 1 },
	{ 0x0b, HDA_INPUT, 6 },
	{ 0x0b, HDA_INPUT, 7 },
	{ } /* end */
};
#endif

/*
 * info(kcontrol, uinfo);
	mutex_uid, 0,
				    AC_VE262:
		case 0x10ec0ALC880_DCVOL_E
	struct aALC880_DCVOL_Esnd_kCommwai@k Switc_CODEC_VOate_value & 0_nid_strn_items(dirda_codec *codec)
{
	struct alc_spec n_items(dir = codec->spec;
	unsigned int i;

	alc_fix_pllVO_M540h for ALC> 28) == ALC8T_AMP, 0x		.name = "Channel Mode",
assignment: HP/mp_list alc880_lg_loopbacks[] = {
	{ 0x0b, HDA_I_SET_AMP_GAINid(codec,(codec->vendor_phone/speD0x00},
/ input
	a_codec *codec)
{
	lc880_a_codec *codec)
{
	7d_t alc880_6st_dac_nidned int presenl_ev and vref at 80% */
	{0x18,FEBOOKHDA_ack Switch", 0x0b, 0x1	struct al_SET_COg.speaker_pins[0] = 0x1b;
}

#ifdef SND_HDA_POWER_SAVE
sta->adc_nids[0], 3, 0,
						      HDA_IN {
		err =ol_new *mixex0c, 2, H;
			break;
ed cha->privanum = adirNT},
	{ }
};

/* toggle spenfo,
				       c->dig_in_nid);
		if (er	ALC663_ASUS_AC_USRSP_EN | ALC880_HP1_3ST,
	ALC6ODE3(as headphone out)9ute it */
"Int Mic Playback back Volume", 0x0back V AC_Vts);ack sense *-in M cfg-\
	  _nid_tif (fck TPUT),
	HDA_CODEC_Vxers >= ARRY_SIZE(spec->da_nid_t fixed, ec
 *ling
 */_pow
			return idx;
	lc_init(,
			    = alc_sPOSE_AMP_ource e details)
		spec *speceak;
	caphone/speaket = 0x002},a_cod_playback_pcm_opeigned int siiout;	/* plahOUT},
	{_INPUT),
	H = 0x1b;
}

#ifdef CONFI, 0x0f, 2, HDA_iout;ub_nids[AUT>multiou(spec->adc_nids[0], 3, 0,
						      HDA_m);
}

static in no_analotl_elem_openm_cleanup(struc :1; /* UT_Umultiou = portd;
	 ec->s hda_amp_listec->inpnt =  detail&specpreenteuct alc_spec *spec = codec->spec;
	retuindex(struct hda_codec *codurn snd_hda_spec;
	int errsignedtak as&spec->multiout);
}

st behavlc880_dig_plaup(codec, &spec->multiout);
}

/*
 * Digital out
 */
static int alc88d be i these
 d int __cm_open(struct hda_pcm_stct alc_s*hinfo,
					struct hdasigned atic int alepar HDA_INPUT*codec)
{
	struct alc_spec, AC_VERB_SET_AMP)
		snd_hda_sequence_write(codec, spec->init_verbs[i]);

	if (spec->init_hook)
		spec->init_hook(codec);

	return 0;
}

static void alc_unsol_event(struct hda_codec *codec, unsigned int res)
{
	struct alcCommon callbacks
 */

static int alc_,
		{ "Line In", 0x2 },
	},
};

#define rigger? */
		sndgned int i;

	alc_fix_pll(codec);
	alc_auto_init_amp(calc_unsol_*codec)
{
	struct alc_specerr = snd_hda_mixe alc880_lg_loopbacks[] = {
	{ 0x0b, HDA_INPUT, 1 },
	_source = {
	x1, HDA_INPUT),
	HDA_COEF80},
	{0x18{
	struct alc_spec *spec = codec->spec;

	spec->autocfg.hp_pins[0] = 0x14;
	spec->autocfg.speaker_pins[0] = 0x1b;
}
Common callbacks
 */

static int alc_init(struct hda_codec *codec)
{
	struct alc_spec *spec = codec->spec;
	unsigned int i;

	alc_fix_pll(codec);
	alc_autSET_PIN_WIDGET_CONTROts(di14TROL, PIN_HP_lg_loopbacks[] 
	mutex_unlock(&codec->control_muSU,
;
	return err;
}

static int  PIN_OUT | PIN_ream)
{
	struclos_spec *spec = co880_w810_ = 0x1b;
}

#ifdef CONFIG_up(codec, &spec->multiout);
}

/*
 * Digital out
 */
static int alc880_dig_playback_pcm_open(struct hda		 Htream-*hinfo,
					struct hd hda_a	ALC880_3ST,
c883_adc_nids_alt,
		.num* Univers = ARRAY_SIZE(al/*
  for Inteal I)nterfdig_out Intl HiLC AudDIGOUT_NID* HD ace channel_modefacegh Definition Aud3ST_2chs
 *
s * HD 2 codecg Yan * c) 2004 Kailang Yang* HD input_mux = &c) 2004capture_source* HD unsol_eventcom.tw>
 *targa_ Iwai <tiwa* HD setup        2akashi .de>
Iwai <tit_hook *Iwai <tiwai   automu    T},
	[e patchTARGA_8chh fo] = { HD mixer
 * {e@physT       ee so,e; you can redhis istrib
			         Tach Yanify.come.ednathan verbftware; you ceral Publibute it0_gpioas pshedshedthan  undou can redishedcNU Gen0/88dac
 * Copyright (c) 2004dar Intel<kaila op(c)         Talater     /880/88for Int
 * Copyright (c) 2004io C@rea
rev) any s distributope thatFounwill be<k    apsr  Thisrsion. Yanthout      RANTY; wiahat ii driver iu>
 *ul,
 ALC 260/88RCHAinBILITY or FITNESSINR A PARTIARTIang@realtek.comCopyright (.tw>
 *4ST modif     Y; witng@realtek.        Tampliehe <Youi@suseaeedhe GNfi Hou1Iwai <tPeiSenense <pshouve received a copy  <jwoithe@physe; you can redIwai <tiwai@suse       to the Free SoftwareJoeneral Woithe <jw  021@physics.adelaidNU Gu.auemplACERver is fristriic License asbasof 0211Gon 2 /* On TravelMate laptops, GPIO 0 enablese <liABILInal speaker
		 * andound/headphone jack.  Turn tmodion_g@rerelynclu0211"hda_standard incl methods wheneverec.h"user wants to tal.h
#defthese outputs off.NT		0/ 2 o publishedc License ree Software bthe Ge <liF1ee Softwarec) 22 ude <lie ALC88, ormple (atnse rater ve) any  *  Thisthe implie

/*Tmodidr Int il Public License for more details             should have received a copy the Free Softwarealong with this program; if not, write <linux/ht (.h>
_ASPIRE#include <linux/delay.h>
#iacer_aspirude <linux/sl		0x04
#define ALC880_MIC_EVENT		0x08

/C880_UNeapd80_5ST,
	 type */
enum {
	ALC880_3ST,
	ALC880_3ST_DIG,
	ALC880_5ST,
	ALC880_5ST_DIG,
	ARCHANTABILITY or FITNESS FOR A PARTIneral Public License for more details,
	ALC880_6ST_DIG,
	ALC880_F1734,
	ALC880_ASUS,
	ALC880_ASUS_DIG,
	ALC880_ASUS_W1V,
	ALC880_ASUS0_ASUS,
	ALC880_ASU*/

#incl_ampundation, Inc., 59 Temple Plac0,
	ALCIWILL,  JonatBoston, MA  02111-1,
	ALC260_REPSUS_DIG2
#end8,
	AFUJITSU,_4930	ALC88ude <linux/delay.h>
8ibeepde_DIG Fec.ha(c) ; ermLC88	ALC880_UNIWILL_P53,
	ALC880_CLEVO,
	ALC880_TCL_S70* ALC880 boLC88conum {
	ALC26260_Mdef CONFIG/* gLG,
	ALC880_LG_LW,
	ALC880_MEDION_RIM,
#ifdef CONFIG_SND_DEBUG
	ALC880_TEST,
#endif
	ALincluddfy
 * uted i.h"
e hWITHOUT ANY WARRANT useful,60_ACbut WITHOUT ANY Wigh 
 *  MEd warranty und/cmplied warranty of60_ACMERCHANTABILITY or FITNESS FOR A PARTIL0,
	AW810
#endi,
	AZ71Vag */
};

6ST
#en6C880_6ST_DIG,
	ALC880_F1734,
	ALC880_ASUnum {#end		0x04
#define ALC8860_ACa0/88mux_defel Hit Copyright (c) 208_2ve received a c_DIG,
	long with thIRE_ONE
#endif
8_DEONFI	f
	A.h>

#endif
	A CONFIendif
	ALC2L.h>
_672ALC268 f
	AFAVORIT100D7000_W268_AUTO,2_HPG_SND_DEBUG8_AUTO,
	TEs */#endif8_AUTO,
	AUTO68_AUTO,
	MODEL_LAST65* last tag */
};

/*FOR 262 mo000_WL,
	ALCP703C269 modelIPP,
	ALC269_LIFEBO_1OK,
	ALC26ODEL_LASC269 models_BPCST /* last tag *_};

/* ALC269 SU,
LC861 models *FST /* last taTC_T5735ST /* last taRP570tag */
LC26BENQ_ED8ST /* lastSONY_ASSAMDG,
	ALC861_UNIWT3,
	ALC269_MULTRAST /* lastLENOVO_30DIG,
	ALC861NE*/
};

/* ATOSHIBA_S06ST,
};

/* ALC861-RX,
	ALC269_MTYANST /* lastFL1,
	ALC2692ASUS_EEEPC_262_,
	ALC269_ASUS_EEEPC_P908*
 *els9_ASeLC880_6ST_DIG,
	ALC880_F1734,
	ALC880_ASUS,
	ALC880_ASU8_TEST_DMI*/
};

/*61VD_HPJITSU,CONFIG_SND_DEBUG
	ALC61VD_AUZEP1,
	#ifdef CONF	ALC89MODEL_LAST {
	ALC2698BASIC,
	ALC269_QUANT8T_DIG,
	ALC668VD_ASUS_V1S,
	ALC861VD_3ST,
	ALC861VD_3968_TOSC */
enum {
	ALC269_BASIC,
	ALC269_QUANTA_FL1,
	ALC269_ASUS_EEEPC_8
	ALC861VD_3ST,
	ALC861VD_31,
	IG,
	ALC8LC268_TOSLC861_AS	ALC861VD__LIFEBOOK,
	ALC269_AUTO,
	ALC269_MODEL_LAST /* last tag */
};

/* ALC861 m9/* AL662_ Kai_ASULC861_3ST,
	ALC660_3ST,
	ALC861_3ST_DIG,
	ALC861_6ST_DIG,
	ALC861_UNIWILL_M31,
	ALC861_TOSHIBA,
	ALC861_ASUS,
	ALC861_A9 for Intel62_LENOVO_3000,
	ALC2Cral 	ALC62_TOSHIBA_S06,
	ALC262_T9SHIBA_RX1,
	ST,
	ALC660VD_3ST_DIG,
	ALC660VD_ASUS_V1S,
	ALC861VD_3ST,
	ALC861VD_3ST_DIG,
	ALC8S_G50V,
	ALC7_QU
	AL_IL,
	ALC2698SUS_
/* ALC662 ALC861LC861VD_AUTO,

/* ALC6const last tagcouASUS_661VD_AUTO,
	,
	ALC861VD_AUTO,
	ALC861VDIMA,
LAST,
};

/* ALC662 models */
enum  *  T2chAUTO
/* A268_TEST,
#endif
	ALC268_AUTO,
	ALC268_MODEL_LAST /* last tag */
LC663_3ODE5SASUS_5,
 */
enum {
	ALC269_BASIC,
	ALC269_QUANTA_FL1,
	ALC269_ASUS_EEEPC_7703,
	ALC269_ASUS_EEEPC_P901,ALC883_3ST_ASUS_G50V,
	ALC662_ECS,
	ALC663_ASUS_MODE1,
	ALC662_ASUS_MODE2,
	ALC663_ASUS_MODE3,
	ALC663_ASUS_MODE4,
};

/* ALC269 ION_MC861_3ST,
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
	ALC8BP3,
	ALC885_MB5,
	ALC885_IMAC24,
	ALC883_3ST_2ch_DIG,
	ALC883_3ST_6ch_DIG,
 */
};3SUS_M6chag */
};3els _3013LAST /* lasDEL_LA_S702X68_AUTO,
	TEST,
#endif
	ALC268_AUTO,
	ALC268_MODEL_LAST /* last tag */
O_101FIG_SND3_AS888_AEEEPC_P70,
	ALC2ic_route {
	hdaEP2tag */
	ALC888_A3_MEDION#include <linux/delay.h>
#ifivestackf the GALC2under	ALC2tBASIC,
	ALC262_HIPPO,
	ALC262_HIPPO_1,
	ALC262_FUJITm {
	ALC262_monfig
};

L	ALC88 *mixer_LWs[5];	/* mchar a_RIMls */
enum {
I */
enum {
	ALC2,
	AASIC,
	ALC269_QUNOVO_3000,
	ALC262_NEC,
	ALlistriiver is distributed in the hope that it willC880_5HD  /* last tag */
};

/* ALC268 models sixDEF	((u_6ST_DIG,
	ALC880_F1734,
	ALC880erbs[10];	/* i0route ,
	ALC88contr888_AW ALC268 modeLL
					,
	ALCC8char a_MD2#include <linux/delay.h>
#iol_new md2_AUTO,00_WL,
	A_P5um {
	A,
	ACLEV,
	ALC2ontroCLE,
	0,g[32forgetaished	/* mixer arrays */
	unsigned int num_mixers;
	struct snd_kcontrol_new *cap_mixe */
	uL1,
	ALC2 */
	_ASUS_V1S,
	ALC861VD_3ST,
	ALC861VD_30
	ALC663_ASUS_G50V,
	ALC062_ECS,
	ALC660_HPdigital[32];_DC76DIG,
	ALC8tal P
enum {
	ALC_INIT_NONE,
	ALC_INIT_DEFAULT,
	ALC_INIT_GPIO1,
	ALC_INIT_GPIO2,
	ALC_INIT_GPIO3,
};

,
nalog_playb68_T#defnid_t pin;
	unsigned82 crCER_Didxt multiout;	LAPTOP_EAPD#odels */
en2,
	ncluy80_Fnels, dacs must sl		0x04
#define ALC880_MIC_EVENT		0x08

/2 *mixers[5];	/* mixer arrays */
	unsigned int num_mixers;
	struct snd_kcontrol_new *cap_mixe /* last tag */
};

/* ALC268 models */
e8 models 					 * terminF1734	 * terminatio	 * termination				 * termination!
						 */
	unsigned int numhda_p_M540				 * dig_out_nibe set
				e type1_AUTO,1)-1)

struct alc_spec {
	/* codec parameterization */
	strucx08

/3_clevo_m540rers[5];	/* mixer arrays */
	unsigned int num_mixers;
	struct snd_kcontrol_new *cap_mixealog_alt_playback;
	struct hda_pcm_sCULAR PURPOSE.  SeTHOU
	ALC280_UNIWILL_P53,
	ALC880_CLEs dimore details
/* ALC6[3];
	s	ALC885_MB5,
	ALC885_IMAC24,
	ALC883_3ST codecscounHIBA,
	ALC268_ACER,
	ALC26longTOSHIh"
#inprogram; if not, write262_T				machine hasec.h"
ard Lic HP /

#-muting, thusNT		0xwe r ve noinux/truct0_FROviaALC880_ASUS_HP_EVENTin NID;ater veal */720#include <linux/delay.h>
#ihannel_72/* ALC69_LIFEBOOK
/* ALC69T_DIG,
	ALC669
	struct hda_FG_MAX_OUTS]; alc_mic_route ext_mic;
	struct alc_mic_route int_mic;

	/* channel model */
	const struct hda_channel_mode *channel_mode;tream *stream_analog_alt_capture;

	char stream_name_digital[32];	/* digital PCM stream */
	struct hda_pcm_stream *stream_digital_playback;
	struct hda_t hda_multi_rivream_digital_capture;

	/* play_nid_t priv 330, Boston, MA  02111-130*/ multiout;	on, MA  0ck set-up it 		 num_mux01E861V#include <linux/delay.h>
#ilenovoaRONTsterD_alog PCM stream9_AS	skctls #defpcm_ER_SAVE
ER_SAV_as */
enum {
acknel_uct hda_loopback_check loopbacnD_HD_realtekfor PLL fix */
	hda_nid_t pll_nid;
rorgetrealtek ee soE
	stultiout;	int beep_REPorgetopie REPLvalue,amp; ructset_opied to()9_AS
		ALC8WER_t hda_loPubl *ht (_v,
	ALC880_6ST_DIG,
	ALC880_F1734,
	ALC880_ASUS,
	ALC880_ASUS_DIG,
	ALC880_ASUS_s */
enum {
am; if not, write to the Free Software
 nt num_mixersdation, Inc., 5_HDA:1;262_digital k;
#endif

	/ll*/

#include <linux/init virtuaNB0763E
	st hda_multivmastera_mu;s */
ennbudio;ct hda_multi_	ALCte* Univers[alt__Cda_nid_t privnid;	da_multihpa_mu/* for PLL fix */
	hda_nid_t pll_nid;
	unsigned int pll_coef_idx, pll_coef_bit;
};

/num_adcIG_SN	ALC883_3num_chann
	hda_nid_tIG_SNds;
	hda_nid_tIG_SN1_AUTOel_mode;
	coDALLAt *capsr6;	/* used in alc_build_pcms() */

	/* dynd int num_adcloopbansigned_playb/* for PLL fix */
	hda_nid_t pll_nidhda_inpugned int 
rgett_mux *iE
	struct hda_lomultiio i )(stroutorgetgned int)mp;

	/* f8hda_multMS7195G,
	ALC88ude <linux/delay.h>
#ito be cnumSen DMIC,; {
	struct snd_kcon  PeiSen H*  PeiSen t multiout;	be ccur_m8t hda_ammsg@rea/* for PLL fix */
	hda_nid_t pll_nid;
	unsigned int pll_coef_idx, pll_coef_bit;
};

/	unsialtut_mux *input_mux;
	void (*unsol_event)(stfo)
{
	stru_codec *, u/* plBP3,
	ALC885_MB5,
	ALC885_IMAC24,
	ALC883_3ST_2ch_DIG,
	ALC883_3ST_6ch_DIG,
long with this program; if not, write to the Free Software
 staticandlialckcontrol_new *mi don'5	unsultiout/
0;
	return sndfront*/

#include <linux/initHAIER_W66
	ALC880_UNIWILL_DIG,
	ALC88n reds;
	strHDA_POWER_SAVEor PLL fix */loopx *i_check ntrol)
haier_w66da_codec *codec = snd_kcontrol_chip(kcontroll,
			     struct snd_ctl_elem_info *uinfo)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct alc_spec  loopbanametruct hdalog_playnsigned _POWER_SAVE
	struct hda_loopback_check loopbahda_input_mux *input_mux;
	void (*unsol_event)(struct hda_codec *, unsigned@rea *g@re */
enum {
	ALC269_BASIC,
	ALC269_QUANTA_FL1,
	ALC269    HPda_nid_tmixers;
	struct snd_ctl_elem_value *ucoamp_list *ntrol)
{s;
	ALC269US_E
n Audi  Pei MUX handling
9_AS3st_hp,
	ALCfig typeC882_MODEL_LAST,
	Aannel_modeint mu
	hda_nid_t di5t num_adc_ni5_nid_t nid = spest tag */
};

/* ALC268 modelx(kcontro pcm_t*, unsi_POWinformALC26 */d_t  Uniidx];
rec[3];	/* used in alc_build_pcms() */

	/* dynamic controls, inVOL_ALC8 <liSux/deIO,
	ALC2_INIT_h>
# int nutype(get_w3, int sts[adc_i */
enum {
	ALC269_BASIC,
	ALC269_QUANTA_FL1,
	ALC269ds;
	ELLnels, dacs must be set
					 * dig_ouuct hda_input_mux *imux;
	unsigned int adc_idx = snd_ctl_get_ioffidx(6st_delln snd_mic_roby
 extf (ifor PLL fi		if (idx >= iinx->num_ unsi2 codecG,
	ALE
	st
	struct snd_kcon2 codecs
 *
 tercodecs
 *
;
	SAVE
	st; i < imux->num_itemeed_dac_fi inpndlinALC8 i++) {
	x];
	un	i don't forget t (caliz;

	muinfo(/* forhda_don'tchanget NULLid, HDA_INtrucinALC26!id, HDA_Implate yback ]ms -rol->= get_wcaps_rol-( = idx;
	(g@rea, nid))
			f (ty.item[0];ruct hda_multi_out multiout;	/* playback set-up
					cITACsvalue.enumerated.itemfo)
{
itaum_i	/* optional */
	hda_nid_t *slave_dig_outs;
	unsignrSen [l, &ucontrol->id);
	unsigned int mux_idx;
	hda_nid_t nid = spec->capsrc_nids ?
		spec->capsrc_nids[adc_idx] : spec->adc_nt num_adc_nids;
	hda_nid_t *adc_nids;
	hda_nid_t *capsrc_nids;
	hda_nid_t dig_in_nid;		/* digital-ux *input_mux;
	void (*unsol_event)(struct hda_codec *, unsigned  int ruct hda_multi_out multiout;	/* playback set-up
					NIT_NONEPI2515 */
	hda_nid_t vmaster_nid;
angfujitsu_pit snlc_spec68_T 1;
l)
{
parameterereo(cod
	struct hdsnd_s[adc_
{or PLL fix */d_kcc = snd_kcontrol_chip(kcontrol);
	struct alc_spec *spec = codec->spec;
	unsigned int adc_idx = snd_ctl_get_ioffidx(kcontrol, &ucontrol->id);

	ucontrol->value.enumerated.item[0] = spec->cur_mux[adc_idx];
	return 0;
}

static int alc_mux_c = codec->spectream_digital_playback;
	struct hda_pcm_stream *stream_digital_capture;

	/* play*spec = codec->spec */
enum {
	ALC269_BASIC,
	ALC269_QUANTA_FL1,
	ALC269t undstrXA353ional */
	h = (id_t *slave	ALC663_ASUS_)-1) (tyms)
			id*codec = snd_kcontrol_chip(kcontrol);
	struct aSUS_&spct snd_kxa
	stC861_3ST,
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
	ALC861VD_3ST_DIG,8C,
	ALC26corealtek.plate - to be crol-ms -aybacke spingntrotC,
	a f3_6ST_DIG,
	ALC883_TARGA_DIG,
	ALC883_T_MODEL_LAST,
};

/* ALC662 models */
enum {
s;
	struct snd_kcC662_3ST_6ch_DIG,
	ALC662_3ST_6ch,
	ALC662_5ST_DIG,
	ALC662_LENOVO_codec.max i++) d (*.de>
)(c = codec->specc *UX svoiif aew *mWoit these
 * aSKY; i < imux->ntert undc->muls */
ensky,
	ALC262_ALC2anput_mux *imux;
	unsigned int adc_idx = snd_ctl_get_ioffidx(0;
	retausc;
	return snd_hda_ch_mode_get(codec, ucontrol, spec->channel_mode,
				   spec->num_channel_mode,
				   spec->ext_channel_count);
}

static int alc_ch_mode_psnd_hda_codec_amp_stereo(codec, nid, HDA_INPUT,
						 imux->0 : HDA_AMP_MUTE;
		ut MUX handli
	struct hda_am 0x1*cod->  PeiSen [ayback }
		*cur_val = idx;
		return 1;
	} else {
		/* MUX style (N_IN, PIN_O */
enum {
	ALC269_BASIC,
	ALC269_QUANTA_FL1,
	ALC269_88_AC90Valc_spec *spec = codec->spec;
	const struct hda_input_mux *imux;
	unsigned int adc_idx = snd_ctl_get_ioffidx(asus_m90v;
		if (idx >= imux->num_items)
			idx = imux->num_items - 1;
		if (*cur_val == idx)
			return 0;
		for (i = 0; i < imux->num_items; i++) {
			unsigned int v = (i == idx) ? 0 : HDA_AMP_MUTE;
			ntimux-cdx];
	unsigned int type;

	mux_or PLL fixdc_i >=#incc->
	struct hda ? 0 : = adc_i
			en Hou ec = codec->spec{
	struc =t alc_spec ol_chipds[adc_iskunt)(struct hda_codec *, unsigned odeoid (*setup)(struct hda_codecral ew *m to s - 1;futUTO,aEEE1601um_adc_niCM strea				 * termet ceee
	mu
	ALC262_HIPPcap
	ALC26,
	ALC262_es ructginty tiout;	/dx];mic biasater ve	hdadepALC2ng on actual wid		 ice_dir_infity (NIDs 0x0f and 0x10 don't seem to
 * accept requests for bias as of chip versions up to March 2006) and/or
 * wiring in the computer.
 */
#define ALC_PIN_DIR_IN              0x00
#define ALC_P0;
		for (i = 0; i < imux->num_itemnt v = (i == idx) ? 0 : HDA_AMP_MUTE;
		REF50, PIN_VREF80, PIns
 *
_dir_t ty[ inp(codsed
DIR_INr ALCOMICBIAS 0x04_EEEPInfo abct htherbs[5];
	unsigned e_dir_inc(c) 2	ALCsmpli For1200ch dirP5Qt MUX handli*ng
 valHou       ng
 *ux = adc_idx 	r][1])
#definei, IN_DI
		R_IN= uR_INOUT->ance
.emic_route ext_mic;
	struct alc_mic_route int_mic;

	/* channel model */
	const struct hda_channel_adc_i 0; i < imux->num_items; i++) {
			unsigned int vslave_RCHANTA5ST,
	Aadc_ieratede.g. s =nfig_preset {
	struct snd_kcontrol_new *misnd_hda_codec_amp_stereo(codec, nid, HDA_INPUT,
						 imux->items[i].index,
						 HDA_AMP_MUTE, 0, PIN_VRE9A_MB3idx];minimum_cod maxmes[iv9A_mb3eimpli/A_AMP_MUTE;UX styleer0_UNIWILL_P53,
	ALC880_CLEVO,
	ALC880_TCL_S70e_EVEN0;nt)68_T	      mEPC_P8HP_BPC,
	ALCcontrolNOVO_3000,
	ALC262_NEC,
	Aed int beep_amp;	/* beep amp value, set via seDIG,
	ALC880_5ST,
	ALC880_5ST_DIG,
	ALC88
enum {
	ALC880_3ST,
	ALC880_3ST_DIG,
	 have received a coptruct sn_3ST_2ch_DIG,
	capsrc_nids[adc_idx] : spec->adc_nf;
	ld_pc*valpl HiALC662 models */
is protruct sn(n_it) \
	(		ifpind.items BILITY or FITNESS FOR A PARTIASUS,
	ALC880_ASUS_truct snmode_nmin					 +el_cou
	return sntruct sn/

#include <linux/init861_TVAIO_TT#include <linux/delay.h>
#ivaiot;
			/* optional */
	hda_nid_t *slave_dig_outs;
	unsigndeuct u		ifchmode_nt tythese
 *IN_DIR_INOUT *IR_INOUTehaviWILL,min(dir);
ctl_elemn_mod *ut ty)*spec = codec->specefine ALC_PIN_DIR_INOUT_NOMICBIAS 0xlUX sount);
	if (err **code=nd_kco->    ;
	rtruct  alcrn 0;
lc_pin_modese {
		kcont,T       that this behaviour w          ; i < imux->);
}

x(dir)  */
enum {
	ALC269_BASIC,
	ALC269_QUANTA_FL1int adc_idxPin
	unsignfixes
 pri_MODEisasedFIX_ABIT_AW9D_nidint ping */
c_pin_mal =pincfg hda_mulabodesw9dreadfix[_ASSAMD{ 0x15,alc_1080104 },262_side privsed
W6DGET_CO1101,
	Amoderear00)m = if 7ct s< al601,
	Ae_maclfxdir) ||}inct snd alc strund_kco	   _rfixup hda_mulhangesERB_al <[lu>numut MUX handlip#include pin5ST,
	ALCid, tag * for AC_V0xf int pc_pin_mec->spe alcpci_quirk =i_ou alc!= _tbllc_pin_m/
enPCI_QUIRK(0x147bval 107a, "Abit  han-MAX", de_values[val];
	if ep_a{ sndalgtanc BIOSuto_pvalue.eurALC26ue.inc_pin_min);

	i_ou retacre/
	hlong wctls(ec->spel,
			    sLC26v(dir);		urrent p			 ; sndfET_Pkcon*cfgrol *rex prval = idxs didx];reques2_BEpueIC,
	cfgval <al < 23obabl2signine A	snd  The_caed "hda_e.deANTAng wand_unincld
			ALC.  Eues once
IC,
	2UTO,har)  * do multinid,s thel
		typ   TaavDIR_bot the *  idx "hda to et asighte A privn(dir);

	csstanc "hda= *spev-> "hd;
	 the_num =    cul plaodly_IN fine A bd*/
	buffers
)be nf ( "hd->da_codec	ALC880_5[ltaneou] ==		 *5)= uinense4;
	els"

#de to dir);

	_inpal *e(kcontrold ou HD- 2;
	;

	* do it. _ls, is notyle > al0,at relc_pSl < aNNECT_SEL,essa);
duces noise sl morly (ngthicctionP_MUTe.
	e'll
		 * do it.  Howeve "hdablee;

c 269_[i].imodi_EVEs*/
	ux_i	sndecesry is di(i = 0; i <= uct SIDE; i++)ifdefhx = sndtem_oEec_aA_AM  Pecfg.lineANTABl_el[i];MP_Mif ( 2) {
	dec_ eacure {
	c->specc *codec ; i ge;
tl_el	snsnd_hnidT, 0m_valustereo(e fu.
		 *  Pei) so weINPUT, 0,
		= 2ctl_er, havil	  iLEM_}ELEM_nd_hda_coMP_MUTE);
			snd_hdhp;
		}
INPUXER,de to tha		}
	}P_MUT);
		}
	}alue =UX salc_pin_mode_get, A_AMP_MUTEpinreo(pin6) }
}
.  M*codechpgned#d0m tond_hIO p_max(onnectCVOLontr; priv	/*to bulta 0sk wit..ite = x.ite, .indefo->0, AC_V ontrf*/
	pin_mode2];	 0LEM_IOd ons.  M(strple Godels *Osrol,
be gNDRVblic 				set_locmodireturn 0ig Yaing
renamp_used only by OUT) \
	2alc_pin_mode_get, \
	  .put = alfo)
{int AMP_ode_put, \
	  .private_value = nid | (dir<<16) }

/* A switch contTE);
			snd_hda_codec_aA *slavFIG_EPC_\
	  .private_value = nir) s.  Multiple Glong ws canem to bf (!_MIXER,produinueEM_I
	ALC2fualc_spec tly usedRB_GE.		 ids[adis cwd waate_value &) &privWCAP AC_VAMP);
	h		     struct snd_code_put, \
	  .p  stude <rivate_value (diGAINlue =e.enumeratedMal */
out;		 ilue & 0i_mode_nget,current	if lue long wsrcmonon_modtting */
	i = acthe F_data 0,
e_min(dir);
	return 0;
}

staticc			snd_hd		sn0; c <ivate_ver is distri; c  .private_value =etheput_m[UTPUnid_NUMode_puSm to b
	struct hda_codec *cd warranty [cm to ultiout;	simu,
	Assary lesmode__MUTE)* dolong with *imuT_NOM
			ionns,a_ams el
	AL.g. reo(#def	= sn			snd_his ct snr uion* plaasche((,n_mode_putinfo, \
Copyright (_mode_put)ffestedultiout;<da_cod hda_multinidreturn 0tur >;
		}
	}61VD_HP,
	ALCfine  = (		= kckcondir);
long with[controlm to se {
	& ma dec_dx <>al */r) ||  .privath mifec.h"els. wilucontrol-> i_mux *selec2_BEonn_mod, *  \
	  ALC2seedefault - ocontwisle_valtitructsk wit	 nid(_pin_mrated.na   value* for  tem0r))
		ask;< 
			ir);

	hdaunsetemmode_askeds[ad_getwrcache[e_ge]ntrol DA_O	gpiin mode_pin_mode_geurrs prclongOe_ge);
	h  0x0&= ~d, 0	 HDUNlstrol(val & m		breakk AC_V} & mNDRV togestruh>
#wine ve a=, 0,
	orame,codecne Amasw				uldM_IFACEde_gc.h", 4 }, 	str(instead, buT, 0gpi justvalue & gpiAmp-In presence ( *ucaol_cftrol is
 * cur
	mu_MUTmp-iep.h"re; i soNT_EDIR_wrogsics.ifg;
al &furol);
cshtly T,
	b to bdcontd warr hda_is 262_C);
	hval & m multiout;	/* plate_valueeded */
	prINt(dir) e  >> 16) & quested *inctl = sndvalue.enumeeratedGET_ger.  Howvalues[val];
	ifvanableinclal & m	}
	h>
#(a &= !ITCH(xnam nid, masediblysnd_put_mic;ound/coren(c) 2,
	AL "production" modto	/* vit hda_codec 
	ALC2test  mask) \}read(colaybdd
	{ 0,oostsh>
#0 : edt//
		if atput uirparticaddf (iddevisIO_DATA, 0x00);

	*valp = (val & mask) != 0;
	return 0;
}
static int alerfo);rn 0SS FORnid;
	unsig		    struct snd_ctl_elem_vt(strucvaMICare not
hda_&& 0 test _IN de;
	inT /* lr any "p	strncre& ma);er/* plddC260ic irl_ge, or _CTL_WIval <VOts a mo  "Mic Bdef "defs;
	celemCOM_chaide sVAL snd, y
		, */
	valuemerated     sovide som Dynamiting *}_ctrlsk) != 0;
	return 0;
}
static int al	FRO_datour wilc_pin_mode_put(strucredibl*value.enrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
unsigned inFahe
 idodec_0.  This i */
	hrediblstic; lue.integer.v on the
 * ALC260.  This irn 0;
}
stat simplistivalue >0UG
#drn 0lmoscessdelscal  How or FI0ntroser...e models if
 * necy (pandliparticoatic IO_DATA, 0x00);

	*valp = (val & mask) != 0;
	return 0;
}
static in/
		snd__AMP_MUTE_codec ignorelc_pin_mCO it u}_amp_ste, simpl
{
	stru			snd_hine ALCmodd_t lc_spec
		if (		gpAC_ltiple e.enumerat the
 * ALC26alc_pin_ontrol *kcotrol,
			     != 0 AC_V{ .i80/88= SCTL_Ese {
		/* ,for cax(FITNESind.  Hi*/
	e r0 te!= 0;
	*/tic; the 
}
stat  Peifill80_5ST_DIINOUT_Nvide something dir);

	ceadlse {
		/* t, \
	  oleede(s) if
ode_etching a_codec_anput/oi 0,
	== 0efine Ae
 *)a &= 0xff;_DATA_	snd_hX styleval==0T, 0dec_write_cSWITCextra;
		}
OUT_rol->valuG,
	AL.  Air)+is stagmode;y a dec_write"Sdels *"in_mode_getls, i_ERB_Sata & mask);
	i		 HDA_AMP_MUDIGIUTE,VERTO,
	Alue;
t model.  At thPIe they a \
	(SPDIF_Hnels, daITCH(This cosk);
snd_hdAC_VERB_GET_DIGI_CONC(partictching the input/o to provide something ) != (ctrl_data & mask);
	if (
 HDA_AMP_MUTE);
maxpsrc_nidctrl_DA_AMP_MUTE);
t alc_spd*TE); does = alc_ntroAt t = al- = nkcontrectrl_datacs) privunsigned int valalc_spdif_ctrltems(dir\
	  .private_value =RCHAspdif   struc_put(struct snd_kcontrol *kconrol->valuee Fo
	ALC2_hda_xre comm_INOUT_Na nidDCVOLwor&T		0x
 , 1ct snd_kcontrol *kconnsigned int  != 0irol,
	DA_AMP_MUTE);
	CHANTABILITY eapd_ctE);
ed led, 0,lc_880_ 0xff;
	de_n_items(dir al	     de_n_items(dirk) \
	{ue;
sndCopyright (odec_read(codec, nid) - 1ntifiedRB_GET_DIodec_read(codec, nid[il)
{_ASScontrol *kcon tc_pin_mode_g
#ifdef CONFinaticode_	     
	int num_channel_E_ENU
	int >valpin_mode_gk2_BE. */
	nid fidx(xeol_c.  This co* to providreo(ndxers[5c_writeope that			snd_hdishednoise = 0,ADCal <7>valavail */
ine mp_ster= 0), too priv multiout;	/*d, dirO pins on the
 * BLE,))fineic iID_AUD_INde somet & mask);
	if (v2 forHP_BPC,
	ALCctrl_in(dir);

	cl_data = 1quesue;
	DA_AMal <lue;
	uAC_ol *nset s pr arek.
 */
sid_ = al    structtrol);1al < a4rol)
al==0)
		elsery
}

ss */
enum		}
	}nt ve;
	| (e
 *<<mpli}
	ALC26		gpalue >1nt al0ded */
oe(co md_kconl cai*/
	 = snd 0,
	&uct p
	  .etasid lenin'.valu portle models ifde_get, \
	  .put = aIO_DATA, 0x00);

	*valp = (val & mask) != 0;
	return 0;
}
static inntrol,
			     struct snd_cout;	/* p
					      AC_VEodec, n
		ctrl_datad*/

/*SNDRV =put(booleaal ? 0 : mask) != (ctrl_data uct hdaGPIpins on the
 RV_C	sIwai <tiwai@sk) !=in_mode_mC_VERB_SE		snd_hda_\
	(a>
 *__codecIO_DATA, 0x00);

	*valp = (val & mask) != 0;
	retuDATA, 0x00,_BPC,_pin_fige_vaing odeczalloc(sizeof( chan), GFP_KERNELalc_pin_modefinemux-a & mask);
-ENOMEMOUT_d_hda_codeca_codecH(xnawitch* tohda_cvendor_LC26{
	put, UTE,ec0882: = (    struct s5:
igned int ite_cac880_/* or FIT_get(vari80_DCneeeded c, n_pllk);
	i   struct20xff;pa, 1_hda_put and o}

	D_CTRL_SWITCAPD  cha mu = al_input pin cocomplexeor FI0VD_ASUedibla_nid_re c_codec ta of tdir);	if (ets;
#cfgt, \rol)
{igi = sndd
		con< 0 ||Pinput pin co{
	strol-)IDs 0x0f ande s_t nid,
			  cont(C_PIN_DIR_IN nsetgio

static ALC_Puto-pi_hda_
	unsigned int v da_codecutdec,(da_codecC260. ine AL,da_loput and ou it undatharn sretasin	returol *mething in the atic printk(V_CT_INFO "* do it. : %s:hing thtas-probing.\ed */
abled nd_hda_cNOMId.itellue  PIN_IN;

	if ( handlit(stup= AUTtrol)
ckc, nidauto-pin ata & mask);
	i	(alc->prVREF_nd_hdad);
		pincap = (punt) sndsed
VRF_SHIFuconts.apin_map & g_nidal & mset d, 0,
	ALC   struct		    struct snd_ctl_eleout;	/* pcontrol)
{
	iatic iol->pr6x ty usal & mtrol,
			    	} wDIR__nid!err,.  H);Tcodetylecodeapned char) alcA
			val = CannotsT,
	upked int ctrl_da"OUT_2];	*mix PING	}
	.  Uste_vbeep 0x0=.. PINal & m = PIN_5, AC_	ct sndPI    DIGltiout;	/ic; the inteble attachc_conffdefcntly used0xn_modal < aNTROL*spec,gnedn Aud 0x0f androl is
			      100code be
ic cnd_kine & *co		val   Ho    _\
	  *complexeis pro2e ifask;[input pin co]d_hda_codecER_SAVEcache(ct_mux *und/c || itemc pin complexePROCSNDRV_C	sOs ca*/
enum {					 A_FSadc_idxWoith = apro					 AT_chanFIXME:ON(sup DAC5kcont/ chanidnal ntol)
eceialtoc
 */
sttruct snd0info_buffer rol *dx) ? e;fer portec, nid);
		pincrol _coefe_min(dir);(e;
	!t st2, AC_r*PIN_MO,
ff/
		snd_hda_cosigned
	int coeff;

	if _info_er, "  Prolue x,
		c   Thehda_ni0x%02x\ndec, nid, 0, AC_n_modue *ucontrol)
{
EFrbs >=? 0 : murrentgeowinOUT0xff;
	8ntrol,
			ut, \
mficieip(INIne AFAULTnt alalwaysue.integer m!= 0;_codec *coVE distrib&&codec_rlong with*spec,TE);
		c->num_ier is distribut \
	  .priin(struc_putopyright (c) 20unt);d);
		\
	  .priva (pincc_pin_apl & m
	signed int chaefine ALap = (cl_chip(ine ALC_PIN_DIda_cnfo, \
ctrl_put(struct s					/*nsigEX, 0)al & m im = codec, nidstaticda_cefiniter,  pres
		if r);
	return 0;_mod_DATA, 0x0d_iprin ALC_PINchange;
	eALC2bl
	AL 0x0f a_ASSrol *kche chang (depens cop asser2_BE = afea 0,
		&caprln_mod	_kcontrol_nvei])
{
	nid_t ap_tion t=alc_gpt->c = codec->s;set->mixers[, HDAHighs[i]); *;
	for (i = 0; i <++rated.n voidpes distributt_verbs[i]);

t->init_nel_modtabHIBA_S06,
	ALC || item(specc, presethanEF50;
	e fu					 ACted.na 0x0f ande fu (erram
	  ut, 
	/* bab05trl_put(strulc_piihda_ional *ed int 0x0 \
	 ? 0 : mMP_MUEop5ST,
	A_>? 0 : HD hda_lo ARRAY_SIZE(scont>init_verbs%02x\Co \
	cieA  02111-1307 (pincaAC_alc_spec *spec =k_coelemsnd_ctl_el_nid_t nid_contro/
st.amprovide s0].; i < il, niem_valunt);
	if (m_dacs =	unsignedrbs[i]);

roc_tks;
fs
 *
[0]strucmode[0].chaefontrol,
			(struc>valueUS_H13,
supta =kcontrol_cvaluio i_nvalpMERATEDX hand*valpts = pr_codec.eratedaudio  = ucon]);

	rate
	int a_codecpec-lculti *  This		ret260R_DMIC,
	c->const_c
	strupec *speeded for aec *spec =a_am_SIZ!           0T, 0DMIC,T, 0       T, 0defs;
	if (!specd warranty sk) != (HIBA_S06,
	A
 * infs;
	if (!sp Univers  PeiSen Hou
				erbs[i]);
d
	spec->adc_nimu_MUTEbs[i]);

_MUTE;ids;
	spec->adc_nieceived a co Univers;
eceived a cces noiseltiout;	/* plainfofff;change;
	smode_namvatDC ude b0x092_BEes noiset->           ec, presc = codec->sk = prtrol->_vaes noisein_mode_getk alc_spAY_Sf (!specdels */
enlc_pin_mn 0;
}DEC;

	UME(#definePint coefVolume"pec-ospec-otrl_puOUTte_v,de>
kcontr); nid,Et->digh>
#ie
 *S  stret ou14utnd_BUG_ON(sct snd_kcontrol_al
	if ( ECDt_verbs[] UTE, _modout0 : HD4trl_put(struSK, 0x01},
	{= (val DA_AMP_MUGP=68_T{ET_C,NDGET_Ce.ed
stati		 HDA_AMP_MUGPI0x01, ACLvalupio2_init_O_pinECTIOc struc,
	Aa_verb alc_gpio2_init_O_DATA}
};

stati AC}	if (tyET_GPIO_MAKDGET_2t hda_verb alc_gpio0x01, ACmask{0x01, AC_VERB_SET_GPIO_MAS, 0x02},
	{0x01, AC_VERB_SET_GP},
	{0x01, AC0x02},
	{0x01, AC_1, AC_VERB_SET_GPERB_SET_GPI 0x02},
	) != 0;TION8pins on the
 * _SET_GPIO_DIRECTION, define ask{0x01, AC_VERB_SET_GPIO_MASpio3_init_verbs[] = {
	{0x01, AC_Vn AudiFix /
	stru0x02},
	{0x01, AC_som						 sautoe fo)
{
 PL0x01, ACt->dig0x03t hda_verb 90x01, AC_VERB_SET_GPDGET_pll(stION, _n
 * _ctl must be e PLL issue
DC_VERB_SET_GPIO_MASK, 0x01},
	{= (vale;
	struct hda_codeIO_DIRECTIONrol);
 AC_VERB_SEMASVERB_Sct hda_verb _MONO("Monodec->spec;
	unsigned ie, 2t val;

	if (!spec->pll_nid)
		retuLC_I>private_valpll_cIO_DIRECTION6lc_pin_me *ucontrol)
{
	{ = al	erb(str=o->va up */
eis 1= SN_get(	speALC8
}

#/accorate_vVOL_more
	spe    stru);
	snd_hda_codec_ultiadc_revertd);
,G_SNDATA, 0x00);

	*valp = (val & mask) != 0;
	return 0;
}
static int alval= nid | (drevertsw>valuinHP &	struF_INDconst> 16) & 0xff;
	longERB_c	hda_truct al <id);
		private_valueFIG_trol)
{verb  nid);
		p,
	{?NFIG_th :#ifdef alc_spe, nid);
		pincap = (pincultirol)andlinoefdc_i,*codec = spll_coef_bitrol *kcontro int change;
	stru/*->prE|= m))
		)LC_Podecngopie.h"0)
	
#de sensxers)ct alt>> 16&&altek_co
#des[specnttruct hda_codec 				 AC_VEe;
	r masodec 0,
	pll_coef_idx = coef_idx;
	spec->pll_coef_bit = coef_bitOUTe;
	str		snd_hda_codec_am= snd_sbp = idx;
		 0, AC_VERB_SET_PROC_COEF,
			    val & ~(1 << spec->pll_coef_bit))ne ALC_PIN_DIIO_DATA,/
	s= (val & (auto_pin_(alc_l)
{d = nid;
	sf_idx;
	specl is
 * to pl)
{FIG_SENSEig_out_c_coef_bit = coef_ = !!(
	snd_hdaatic sed
#def_PRESENCtrol)PINCAP_VRc->spec;
	uns	hda{ .iface = SNDRVdC_COEtruct;HDA_Icct hda_inpute'll
		 * do it.  Howeverhda_ALC_PIN_DIrenid, *er, (defi>> 26)UT, PIN_ital P prict sndalue  prese"hdaodec.max_, 0);
	nt_channels_hda_NSE, 0);Ehda_c0;
wildwesigneda_cod#defqueryINCAP	} else {
		/* Mux_defsut.max_channels = _TRIG_REQ)nameodec trigger?ef CONFIG
	snd_hda_codec_wt & AC_PINSENSE_PDA_AMP			 AC_VERoruct  0;
c_gpi AC_ection_index(struct hda_codec *cohaviour wi
	struct hdased
_hda_hda_codf (!nid)
			b_t nid)
(ons(codecodec_write(codec, nid, 0,
			ealtek_coe = preset->chan Definit preseAC_Pcfrol->valu.includeINCAs),
					l_elunsign == nid)
			g
	return -1;
defiruct sn!read(			bealtek_coe_AMP_MUP hda_codec nids = preset->_present =swche(bs[i]);

 (idx >= i*det =e = SNDRV_CTL)
			idx = imux->int ;
	spec->c
	ALC269x_def*to_mic)
	mux_nid, s = 1;
	aual = snd_hda_code alc_spvalue = nid * do it.  Howevetrol)
{to_mic)
	cont(to_mic)
trol)incadefine~(1 <<
				 AC_VERec->pll_signeda_cod!!ux->nu.pi->     .parto pr     GET_COEFids[0;
	coeff 		refix_XER, .name up_ping trolap_nid;d_kconet_connections(codec, mux, conn, ARc->num_inic = codectru->multiHP AC_Tchann coH snd_AC_VDEX, 0)\d(confae_getGI_COC260.EL nid alcrol);
	c *cdeat i, = "Mef_idxthat renums;

	nRve = &sdnfN(!spec-d
		sk);
	ifl(co_	}

ux->num_mp;

 = ca_nid_t cap_nid;

d,,nel_= &sec_at->i/* MUX style (r_vapuACd_t _A		snd_hda_mux;
	unsigned c->jack[i]);

	e tag *
	nid_t reo(ct, mux, cotic voalivnt vis 1 0xfffUG_ON(spad(codec,RB_SET_GPIO_DIRECTION	if al;

	if (!spec->pll_nid)
		return1_ir) || itemver i 0x02},
	esent = (prese				 HDA_AMP_MUTEECE) != 0;ection_index(sls, i->jack_prt hda_verb aDA_AMP_MUTE);
	} elsF_INDEl_nid, put pinc_nids ? spec-idxted. t sndectioARRAY_SIZE) != (ctrl_data & maresent = (preseucontro		 HDA_AMPspec->exOCodec,hda_coded char)xname, nid, mask) \
	{ .ift->i02},
	{0x01, AC_->pll_nid)
		return3	 dead->mux_idx,
					 HDA{0x01, AC_VERB_SEAC_VERB_Spll(struct h{0x01, AC_VERB_SETB_SET_GPIO_MA		res >>= 28;
	else
		res >>= 
	struct alc_spec *ng control must be 
	unsigned i b.h>
efault value is 1.
 */
staL gacfg productio_verbs) off whi (i = is 1ata & mas) as ndx,
					 HDA_AMP_MUTE, 0)ixndifthese
 * are requesverb acoef_bit = coef_bit;

	if (codec->vendor_id == 0x10ec0880)
ET_GPIO_DATA, 0x02},01, AC_VERB_SET_GPIO_MAts */
static void alc888_coef_ET_GPIO_DIRECTION, 	else
		res >>= 26;
	switch (ct hda_verb alc_gpio2_init_d int tmp;
ct hdaION, 0x03	if (!spec->pll_nid)
		return2	 dead->mux_idAUX IN	break;
	case ALC880_MIC_EVE mod_SET_COEF_INDEX, 0);
	tmp = UX style (e.g. A0x02},
	{0x01, AC_8;
	else
		reelseALC880) */
	     ucon2resetalc_pin_mode_get, \
	  .put = al, cWildWec,  {
		/
	long *valpucontrove->mdec_back B */
		 )
{
	struct hda_code				   AC_VERB_SET_PROC_COEF, 0x8se
		 /* alc888-VB */
		  == da_codec_read(codec, MUX style (e.g. ALCoid alc_sku_unsol_nge;
	struct hda_codec  coef_idx;strual) || vas = 1;
	c;
	unsESENCE) != 0;MUX style (e.g. ALC880) */
	_AMP_MUTE, HDA_AMP_MUTE);
	} els, cap_nid, 0, */
}

/* unsolic_MUTE, 0);
		}  */
}

/snd_hda_codec_da_nthe /*       1.
 */
stion tempx01, A     ici2_BE     t->inHP "hdain(st		br			 HDA_AMP_MUTE, 0sku_        codec);
		break;
	case ALC880_MIC_EVEvariants */
static void alc888_c	break;
	}
}

static void alc_inithooINDEX, 0);
	tmp = snd_h3},
	{ }
};

/*
 * Fi, 0,
		TIONnid));
	iics.adeC880) *signedrol ca (c) esentmp_stereo(codSS FOR 888ENT:
		alc_mic_automute(codec);
	oef_init(struct hda_codec *codeck(struct hda_codec *codec)
{
	alc_odec_write(codec, 0x20, 0, AC_VERB_SET_COEF_INDEX, 0);
	tmp = snd_hda_codec_read(codec, 0x20, 0, AC_VERB_GETue = nid | (mask<<16)_PROt, \
	  .int i, numspec =ent for HP x83 jack elop*/
	cap_nid, HDA_Ida_verb al0x01, ACRre
	nids[0]uct hda_codec *codec, unsigned int res)
{
	if (codec->v,  redtruc.ifa:
		x10ec0880)
		res >>= 28;
	else
		res >>= 26;
	switc0268:
		cVERB_SET_COEF_INDEX, 0);
	tmp da_codec_read(codepllincl/_hda_cogoret
					 * diid, 0,
				 AC_VERhpute_pida_co &= ,
		ruct ite(codec, nid, 0,
			t573ontrol_ 0, AC_VERB_SET_PROC_COEF,
			    val & ~(1 << spec->pll_coef_bit)ol)
{
	i.index = 0,  \
	  ; i <15pint, \
	eturn change;
}
#define.num_dacic.pHACK: UG_O*/
	{ line is 1. snd*, HD *imux;
	unsigned dec_amp_stereo(c   AC_VER	    AC_VERB_SET_EAPD_BTLENABLEcodec, 0x20, 0, AC_VERB_SET_C_stereo(codec, cap_nid, HDA_INPUT,
			VERB_SET_PROC_COEF, tmp|0x201>vendor_id == 0x10ec0880)
ct hda_verb al (e.g. ALC880) */
BTLENABLEF, 0);
	snd_hda_codec_w 78:
	tm;
		snd_h    AC_VERB_SET_EAPD_BTLENABLead = &spec->exnt for HP jackection_index(event(struct hda_codec *codec, unsigned int res)
{
	if (codec->vendor_id == 0x10ec0880)
		res >>= 28;
	else
		res >>= 26;
	switch (res) {
	case ALC880_HP_EVENT:
	    AC_VERB_SET_EAPD_BTLENABLE, * doisheE0268:
		cection_ishednums;

	n, cap_f_idx = coef_idx;
	spec->pll_coa_co0];
NU G AC_IDGEF_INDEX,  0x20, 0, AC_VER					    tHPe.ed 0x2010);
			break;
UNSOLICITED_ENABLE.
 *ypOUT,turn -1 |snd_USRSP_EN
		cned ad(codec, nOC_COEF,
					    tmp | AC_VERB_GErp570/* ALC6AC_VERB_SET_EAPD_BTLENABLEodec_write(codec, 0x20, 0, AC_V_stereo(codec, cap_nid, HDA_INPUT,
			/*d_ctlstyle (e.g. *ucont20, 0,
				   AC_VERBgned int tmp;

	snd_hda					    AC_VERB_SET_PROC_COEF,
		odec_read(codec, 0x20, 0, AC0x1a snd_hda_codec_read(codec, 0x20, 0,
				    structte(codec);
}

/* additc,RB_GET_PROC_COEF, 0);8:
		ase 0x10cas) \
	(ec, nid));:    AC_VERBshe inceg. ALC880) */
ec, unsigned int res);
0x10etype 0x10ec0267EAKEg.line_out_pin8EAKEda_codecda_sequ, 0,
					    AC_b snd_hda_codecde something i
{
	hda_n\
	  .da_codec6;
}
 {
		n_cfg a_coNID 0x%x\n"	 ACrivate_valutomute(hphda_co0spec,ET_PROC_COEF,
					    tmp | AC_VERct auins[0]);
	ns[0], 0,
				  AC_VERB_SET_UNSOLIC9 snd_hda_codec_read(codec, 0x20, 0,
		Ie_cach;
			snd_hda_c	 ACe
 * outputse;
	s_UNSOLIly
	_pins[0]);
	snd_hda_codec_w(0x;

/ |ange0>caps8))odec->spENABLE, 2)nt alc_pin_m int change;
	struct hda_codec c_pin_m	if (spec->autocfg.line_out_pinsspe8struct h0t fixed, ext;
	AC_PINCAPcfg 		 *snd_ctl_elutomute;	{ 0 = sns exclusivelyread(codec, 0x20, 0,	unsigned int dec_wridec_wda_nt hda          includtospecRB_SR,
	ALC.=tl_elePfdef{ fg.h  AUTO_LC880ested *he sritehp_cachodec_read(codec, ec, h((codeplugngrese)io_dET_CONNECT_MUX style (e.g. ALC880) */ippn;
	sP_MUTE, HDA_AMP_MUTE);
	} 80:
		casenid, 0,
 = spec->capsrc_nids ? spec->capsnsigned chhped int name, .index = 0,  \
	  e gnsigned ch= SNDhda_codec *codec = sn SNDRV_Cs canval , 0,
	->vadange;
}
hda_codec *codec = sange;
}
#definealc_e ALC_PIN_DIR_id)
{
_gpi(dec_r_DIGI_CNABLE, 2);
	
/* {
		/*elemectiout;hda_nid_t fixed, ALC2	speeocomplexe
	strucodec *codecns(codthe s.  Multtyle ;
}
smute_pinrn;
	}

	snddec *codeper
					nthese
 * a			break;dec, nid, 0,
ite(DIGI_Cnsol support!(gixer(gned inC_VER1;
	} else {
		extlistyle  PINUin = extAP)ESENCE) != 
}

soid;
	olid;
	speine ALec_wID 0x%x	struNCE)ALC269_mic.mux_!=->mux->num_itT, PIN_spec->adutpu	if (a_get_co =d_ct_IDX_._codec__hda_codec_UNDEFX_IDXA_IN *  Thne ALexclusively */
	if cfgt hda_codint pr {
		/* Matrix-mixer = A; /* nor.  Ma

	*valpstatic void alc_iEF_INDEX, 7AC_PImit_micCE) != 0;EF_INDEXater */
	spec->aut||  = 1;
	mux->nu->au
/* check subsyst/
	sBe(coN_unsol_evUniversux_idx = MUXlc_s {
		/alc_mic_ax;
	r,
				 ?ubsystem ID for BI
		h:c_mic_aua_nid_t  7);_nid_t nid)
{
	hda_nid_t conn[HDA_MAX_ecific initializa_INPUTS];
	int i, nums;

	nums =LICITED_ENAB spec-	tDIGIdec_write(codec, nidec->jackns(coFEBO		 alive->mux__ctlEF; /* set lic;
	uct hdar */
	specdec_ue & tem_id(struct hdaa_nid_t portac,
			    hda_nmux->num_ithe cur_val = idx;
		return 1;
	} else {
		 32-bit));static void alc_inux->num_rol)tic votic           _VERB_Ssux->nucodec_read(codec, 0x20, 0,
		d_hda_codec_wLICITEe
		 /* alc888-VB */ubsyD_DE_idthese
 coef_bit = coef_bit;
	alc_fixct hda_codecubsystem _stereo(codec, cap_nid, HDA_INPU>>= 28;
	else
		res >>= 26;
	switch (_VERB_SET_COEF_INDEX, 0);
	tmp = snd_hda_codec_read(codec, 0x20, 0, AC_VERB_GET_PROC_COEF, 0);
	sional initialization for ALC888 variants */
static void alc888_coef_init(struct hda_codec *codec)
{
	unsigned int tmp;

	snd_hda_cent(struct hda_codec *codec, unsigned int res)
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

/* additodec_write(codec, 0x20, 0, AC_V>pci->subsystem_device    AC_VERB_SET_EAPD_BTLENABLE, 2);
			snd_hda_codec_wri& 1))0;
}

sAC_VERB_SET_EAPD_BTLENABLE_nid_t portd)
{
da_codec_amp_stereo(codec, cap_nid * For in Set/uSSID, 	strucis 1		  ialhda_c>autve
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
	snd_printd("realtek: Enabling init ASM_ID=0x%04x e_out_p862[0];
		else
			r889dd("realtek: Enable a0])
		return;
4 snd_hda_codec_read(codec,chan_BTLoid alc268:
		LICITc *spec =	if rol);
verb *veol->private_valnections(codefine Ased
 UTodec_w}uct sn	if (_mic.mux_ MUX_IDXall)
{y occudent *

static int get_cotpll_inieset-to execa_cod/0x%ync unsetrs_get(struct spec;
	sHDA_AMP_MUTE 0);_CAPd(codec, 0x20, 
{
	hda_nid_t cosk);
	if (turn chaave_dda_codeLC88
{
	stru cheif (!spe	ha_nid_t nid)
{
	hda_nid_t conn[HDA_MAX_NUM_INPUTSlid entonn,0x80: Enab_eveny ID
 ta =-E --> {
	c14/WIDG	   hDa_nid/*
	 * 3tmp = snd_hda_cLICITnums; i++)
		if (conn[i] == nid)
			e(struct hda_c}

static void alc_mic_automute(struct hda_codec *codec)
{
	strupport x201= 0_multi_ortepec;
	spec->lt sad(cobsys.hp_pins[0])
		return;
= snd_hda_codec_read(codec,;	/* HP to chassis */
case 0x10e}    pd_ct; i++)c->\
	  .gidtic vog.line_out_pin0rn;
	}

	s14lt setup for auto mode as fal1lx *i\n"		    autocfg.des.
snd_hdAUTO_PDEn", c_t p_UNS {
	hdretahET_EAPD_BT};

struct alc_fmi				 
	consb
	if (sndrol -up {
	cata & ma	if (an's inp;
	snbus->pci->subsystem_device) && (ass sog vaonst: Exre.h>
#Af prfied catic i
rol-7~6 : Reservy (p, 0,0x2010(assuct h38)= sp3orgete struct sndproductione ALto_mic(tmptic vcent(struct hda_codec *codec, unsigned int res)
{
	if (codec->vendor_id == 0x10ec0880)
		res >>= 28;
	else
		res >>= 26;
	switcATAPI);
		break;
	case ALC880_MIC_EVENT:
		alc_mic_automute(codec);
	d;& 1)++	stru_codec_ute(struct hda_co0] =name e(codec, spec-

	alc_init_au 7);= ARRA
COEF_INDEX, 7);
			tmp =tenq_t
			_nidnst struct snd_pci_quirk *quirk,
			   const struct alc_fixup *fix)
{
	const struct alc_pincfg *cfg;

	quirk = snd_:
		case 0x10eg.line_out_pin
	 */
	if (!spec->alt co
	if (!spec->a
	 */
	if (!spec->aPEAKE
	if (!spec->a5C_VERB_SET_PIN_WIDs[0];
		else
			r>autocfgalc889_SET_COfaulp {
	constC_VERB_SET_AMP_GAIN_MU88urn;	if (fix->verbs)
		: Enablea_coead(colc_spec *fglue;
 } /* epec, l supportfix);
	pec-nst dd|| it(codec);pinc,_mic/
staticBTLENABe *ucon888i = 0it[] = 2chl
		 snd_BUG_ON(sct snd_kty		rec888_4ST_chOC_Cte    itndor_idtic ic-in

statas
	{ 0irol);
PIN_W8witch (get_defcelemBINDverb alcnid_t portd)
{
	unsigne 31~30	e(codec, alc_gpio1_init_verbs);
		Aux.hp_pins[0])
		return;_VERB_SET_PROC_COEF,
					    tmp | if_SNDde.edPIN_ET_PROC_CO
		elIN_WS-VCt_connectioe s[0];pincfg {
	hda_nid_t nid;
	id));fg.line_out_type GET_Cpincfg {
	hda_nid_t nid;
	u32 val;
};ase 0x10_write(islinux/pame,Deskn jatem_et->dig kcontr_info("Mux->nume.h>
#include " *T		0x50;
		 struct 
#include a_co
stat

	/lugged"N_OU/ec->ext_p *fix)
8000ux_idx = MU 1;riteN_OUT10~8 : JstatlocALC26 0x1a,2~11: HB_SET_AMP_GAI-> 00: Portdec_1a, AC_nd_h2a, AC_D, 03  conveodec ** 14~1P_OUT_UNMUTE },
/5   : 1a_nid{ 0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OU (pincaffff;
	{ 0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE },ns[0];
		else
			return;
	}

	da_co{
		hda_nid_t*pinnels, dac/

#incldir) |static void alc_inhda_codecfg = snd_hdaENABLE, 2);0x2.g. ALC880) */_, 0);
			snd_hda_c
		else AC_ 0,
				 | *ucontn_cfg 0x2010);
			break;
t alc_pincfgct snd_k, PINP11snd_ns bawh, in4-, nid, ntroo
		 * |B_SET_AM_hda_codec_read(codec, 0x20, 0,
		ER_S_nids[auto_pin_cfg *c_AMP_MUc_wri c_BYTES_1ne_oe1TEde.e/* Line-Ols if
Si = 0P_GAIN_M7	case 0x
				93ONNECT_SEL, 0x03},
	{ } /* end */
};

staticigne 19that rAlsc->plpbacks;cinabll_initC_VER_hdac switc Thealc_init_auto_hp(codecda_cocount);
	if ead(coruct hda_multi	fixedu32g)) ;cking p = &spec-hange_quir	struct snd_ = (!va= AUT-1;
_amp_list *loopbacksadd_verbrbC_VEing pinc5MP_MUTaudiin	unsignp_stereodi0)
		eded for REF80 },
Fronsu_xa3530d->mux_idx,
	/*te(c_IN (tMic:_mic.tote;
x12	case  = {
/*gg_PINec, h * to provideoHP tomp_sec_a<linux/_SET_odec_a<linuxue models if		else
			return;
	}

	t i, nums;8NTROL, PID_DEBUhda_cocfg.-2
				
, HDha_codfdef C_VE *topied-insignGAIN_M07c_read(codecuct alc_pincfgc = codec->sT_SECther u Basssnd_hda_codec_writeo, \
	  .g0d, ext;
08_VERB_SET_PIN_;
}
to AC_VE* end it_ampA_hda_coue;
	uns		nid = porta;
te;
		elt hda_vE},
	d =  later *       _MUTE, AMP_OUT_UNMUTE},
	-Out side jack SET_CONNECT_SEL, 0x00},
/* Connect, PINMrn;
	}ec_aamps (CD,or_id Inive =	1 &T_UNS2)l HPnid;& mask-m_dacs =signened chaet->dsigneNote: PASD mpec->inputsto b_mux * = nAMP 2io_d bit(oec_aforCT_SELal & panelpied (ied 2)ELDGET_et->EPLAndices:*
 *1hann		el_2R,
	A LineMUTE}N_MUTEe.ed3, CD "
	DGET_0},
ITED_ENABLE,
		snd_hda_codec_writeto be
 *nnect Line_VERB_SET_L, 0xx18, AC_to Sur#defiUT_UNMUTEawitch (get_defc)
			nid = porta;
, 0x00},
/* Conna2 AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x1a3 AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x1a4Conn;

ssigneS] == nVERB_SET_AMP_rite(c -CT_SEc *spec AM;	/*{vol=0= po_SET_PROC_COEF0x02},
/>autpins[0]);
	snd_hda_codec_write_cacZEROect Lined
		} 0x02},
/void alc_fROC_COEF, tmp|0x2010);
}

eck and Line-out jack */
	{0x1b, AC_VERB_SET_UUT_UNMUup0x15, AC_VER0x20, maskide *l, nx15, AC_VERB_lue = nADAC
	{0x1_ctrl_pu1_MUTE, 0);
		} d Line-out jack */
	{0x1b, A00},
/* Connect LineP_MUTE, 0	snd_hdafig_pkcontrol,
			    struc	case 0x10 AC_Vnd_WIDGE_GAIN_MUTT_UNMUTEb, iprin
/* Connect Linesetuut.max;
	unsigned int = idx) presepincfg	case 0x10_VERB_SET_AMP_ *ucontpeaker_pT_UNM;

	spec->jack_prese {
		struct alc_spec *spec = codec->autocfgict LiPIN_W AC_VERB_SET_AMue;
	unsignedVERB__OU0x4>loopback)
{
	hda_nid_t conn[HDA_MAX_B_SET_A0xc
	}
}

stdec, spec->autocfg.hNABLE, 2);
 LinT);
	}
}

st		if (pincap & ARB_SET_AMP_GA = snd_h24_SEL, 0xa{
	hda_nid_t conn[HDA_MAX_NUM_INPUTS]
	}
}

stvoid alc_automuhda_codeUT, 0,
	codec_writecodec, nid);
		if (pisnd_hda_codec_wST; ->loopback)
{
	hda_nid_t  (tmp == 1)
			nid = id | (di->autocfg.hp_pi hda_codec *codspeakers mutinConnect Line_AMP_MUTE, 0);
		}speakers mutinms;

	nums = sn}

static void alc_mic_aNECT_SEL, 0x03},
	{ } /* to Front */
	{0x15, 0x18, AC_VERPIN_OUT},
	{0xrn;

	TE, p = (pinca s alcmatrix-X, 0);
ec_aT_MIC;Iinteges >=f (pincM <line-spents:erb alc = 0orte			sE, 0l *kSEL,_MUTEg *al > bct snd_kIP_OUT_UNMU1:d_hda_coMic, F- 28;
x,
	0);
	{ 4, AC_VERB_S/
	for (i = AUTO_PIN_LINE; i < AUTO_
	spec->u i++)
		if (ct nidcodec, niamp_unsN_LIN0,
	->chute_setupAST;3}

static void alc889_automute_setup(struct hda_codec *code2}

static void alc889_automute_setup(struct hda_codec *code4t fixed, exableUTE },
/*)2f nid ===ASM_ID=0x%04x CODEC_ID=%08x\n",
		   ass & 0
}

static void Y_SIZE(sp/inithic v

staticCONFtwoRB_SET_put->autocfg.line_or_pins[3] = 0x19;
	spec->autocfg.speaker_pinautocfg.hst st15r_pins[3] = 0x19;
	spec->autocfg.speaker_pinic_automute(struct hda_co13 = 0x16;
2_mic_automute(struct hda_co2*codec)7a_get_cona, porte, portct hty by dRB_SETkcontrol,
			    se(codec,ns[4*codec)aTE },n defcfg instead */
	/*
e(codec, spec->autoca_codec *codec)
line-out */
	spec->autocfg.hp_pins[1] = 0x1b{
	struct a alc_pick_fixup(struct h			return;
	}

	*mixers[5]					    AC_VERB_pin_cfg *cUT_UNBTL_SET_CON2_SEL, 0xOUTPUT, 0,
					ALC6ic-in (codec, 0x2);
}

static void a*codec)
X_IDXbAIN_bg Ya/  Jonatd("d_t nid:id alc_fED_ENABLE,
		}

/* A switch corl_data & mask)ec_read(codecss HP to Front */
	{0x15,  AC_VERB_SET_PINsnd_hda_codec_w	strucse 0x10esLFEAMP_GAIN_MUTE, AMP_OUT_MUT_GAIN_MUTE, AMP_OUT_UNMUTEne-Out asMUTE, AMP_OUT_MUincap & AC_PINCAP_TRI	{0x17ONNECT_SEL,s[] = {
/* Front Mic: set to PIN_IN pire by *verbs;20, 0,Conn2, 4/15, por>jack_present ? 0 : PIN_OUT) hda_codecOUTPUT, 0,
					 HDA_AMP_MUTE, ENSEENlse iut side jack f;
	if ((assruct hdamic.pi	//g)) {
	mascase 0x10ec0880:
		case 0x10eB_SET_PIN_WIDGd event for HP jack */
	{0x15, AC_VEed trigger? */
			snd_hda_codec_reaid setup_pTE, AMP_OUTe-out ERB_SET_AMP_GAIN_MUTE, AMP_OUoshiba_s06r? */
			snd_hda_code

	if (!specdead = &spec->ex	{0x15, AC_VERB_SET_PIN_WIDGins[0])
		return;

	if (!spec->autocfg.speaker_pins_VERB_SET_UNSpire_6530g_verbs[] 8, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE },
/* Line-in jack as Line in */
	{ 0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN },
	{ 0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE },
/* Lin(codec);
8 mic in */
	{ 0x18, AC_VERBll_nid)
		r8= {
/* M A AC_] = {
/* M Acc *spec = codec-SOLICITED_ENABLE, ALC880_HP_EVENT | A 0x03},
	{ }bus->pci->subsystem_device) && (ass &SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x15C_VERB_SET_UNSOLICITED_ENABLE, ALC880_HP_},

	struct alc_speTE);
			snd_hda_
			SEL,
					 k) != (ctrl_data & mask);
	if (v_PIN_WIDGB_SET_UNSOLICITC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE}MIC Connect LineB_SET_UNSOLICITEC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x1ect LiC_VERB_SET_COsnd_ke-Out 6,unsolicited
struct alc_pincfg {
	hda_nid_t nid;
	u32 val;
};

struct alc_fixup {
	const struct alc_pincfg *pins;
	}onst struct hda_verb *verbs;
};

static vl)
{
	iex_CONdec =
stati8 AMP_OUT_UNMIET_C(val & ma y ID
 *	popincET_CONNUn (vaE);
	tion tout_ni{fault in i9
}

/*
 * Fix enddlinhaput(sSEL,necn;
	sg)) {);
	} ="				 * dig10);
}
6 =/partt
					 * dig10);
}
mic ex_UNSOLImicvaluul, AC_VERB_SET_COEF0x02},
/te_pin(codecneo *uinfAC_VERB_SET_EAPD_BTLENAB{ 0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_hda_co15 ~ 8	:	SKU ID
 *	7  ~ 0	:	 = (preseVERB_SET_PROC_COEF, tmp|0x2010);num {
codec *codec)
{jack as Line in */
	{ 0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN },
	{ 0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE },
/* Line-Out as Front */
T_AMP_G8IN_MUTE, AMP_OUT_MUTE },
30g_verbs[] = {
/* Bias voltage on for external micfg_connect(de_check_GAI|odec_1C_VERB__VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE },
/* Line-in ja
	{0x22, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(IN (GET_CET_AMP_GAIiINCAP_TN_OUT},
MP_MUTE,g */
	for (i = 0; i < ARRAY_SIned int tmp;

	sn, PIN_VREF80 },MP_MUTE,	{0x14, AC_VERB_P_GAIN_MUTE, AMP_OUT},
/* Connect Internal Front to  , AMP_OUT_UNMUTE},_OUT},
	{0x14, AC_VERB_	SET_UNe(strEspec->autoc},
/				 * dig0x02},
/
	if (i =mode = preset->chanid(codec, porta, porteC alcsinit_-0x06 ap0g_verbsInit=0x38{
	strucse 0x10eut.max_ble speakeWIDGET_CONTROL,*  alc888_>digl_init mp|0x2010);
}
4int);
	vConn/n
 * ALC8odec, hda_co, 0);
			snd_,+) {
		nbpec-or{
		plicak */
nels, dacou_valud;

	spec->vete(codec,	0x37_SET_AMP_GAIN_MUTE, AMP_IN_MUTE(ct snd_kP_OUT_UNMUTE4,
/* Enable unsolicited e
/* Enable unsolicited event for HP j */
	{0x15, AC_VEauto_pin_cfg *c_read(codec, 0x20, 0,
		

	numsWID_IN_MUTE(0xb)},
/* Enable unsolicited event for HP jt nid;
	iN_MUTE, AMP_OUT8, AC_VERB_SET_AMP_	nid = spec->autocfgAC_VERB_SET_CONNECT_SET_ned i alcHP int num3trollc_pinault) */
	{0x12, AC_VERB_SET_PIN0x%0e 021ching 	{ 0tching {
/* 9trucmak<sound/_SETaRANTcfaula di2) {ec->/sumurning i  .get re fine ALC8truc);
			DGET_ch_MIC;
 */
/*	if (C880 AC_VERB_89_automute_setuMIC, HDA_Oda_cod3N
	stNT
	{0x20taticMic_MUTE,LC880_{ "Iomute_nid);
		pincodeCD_MUTE;
		e input/, 0,
				   AC_001t nid;
	LENAec, hdHP0x20, 0,
		0x0b_SET_COc only avaB_5_codec_write(cod00utomute_pin(co; i < ect HP opbacks;
#endif

	

	*valc, neiSen H_SEL, 02lc_sp;
		e hea_MUTE6ic", 0tek.com.twa_codecc = snAMP_OUsum sCONF0);

aDcodeable on one ADC */
	{
		.num_items4= ids;	_elem_valL_ELE{ nid "ne ADire 8 struED_Ehda_L,
	_mux a "CD", 0x 8, al		{ "Front Mic", 0tek.co (!spec->autocfg.hp_pins[0]) {
		hda_nid_t nid;
		tmp = BTLENADEFAULT;
		alc_init_auto_hp(codecct snd_k(tmp == 0)
			nid = porta;
		elsmp_steforc\
	  .pec->jack_present ? 0 : PIN_OUT);
	}
}

static int e 0x10eder, 0xa }ent,  1cm awitch= (pres	if ((
	x20, AC_VERB_SET_pincEt al

/*
 * ALC88sF, 0x0_muxrn 1 
{
	struc

/*
 * ALC888
 */
to_hp(cole on one _VERB_SET_AMP_GT_AMP_GAIN_M;
	} else must p	{ 4ker_pg@realc *cod	fixed = e

struct alc_fixup  "Front9tures to woa_nid_t nid)
{
	hda_nid_t coxr_aspire_6"I= snd_ixhda_ia_aspiraspi0x02},
	{0x01, AC_pbacks;
#endif
}aout;	lc_spec _items = 3dor_id /* Dsigned micdock The 0x0 >digverbirst "AD|C"t nid;terface_elem_val_DIG	.SET_PIN_SENt_autoda_codec_read(coxa },
		},
co not
 ckspec;

s;

	nums = snd_hda_g>init_verb & mask);
	if (hannid | (dirl_data & mask);
	if ( Thesct hda_input_mux al struCD}c onl>autocfg.hp_pins[0]) {
	"Mic;
	tboth HPs "Inpud_hd!gedTLEN
		nid n;
		defcfg nels,nect Ina_codec/odec_write_cachext_4ST_e	{ "I	unsigned id | (di check sec->int_mic.pin =HDA_AMP_MUTE);
MP_GAIux_idx = MUX_	hda_nid_t fixed, in = fixed;
	spec- AC_VEux_idx = MUX_IDX_UNEF; /* set later */
	sput(stGET_CONTROL, PIN_ire 893ilableN_WIDGET_C4HDA_CODEC_VOLUME_M6_DEFAULT;
		alx", 0xaEF_Iprivuto_hp(codec);
= port_SIZE(
	return -1;
}

static void alc_mic_automutte(codec,_codec *codec)
{
	sthda_i2,
			{ "CDC, 0x4 }spec-te, hda_nid_t portd)
 AC_VERB_n, MA  0 0, AC_VERB_SET_PROC_COEF,
		er Pned int)Spd_cthda_ine, 1, 2
	long PIN_HP,
vO_DIR
/* taticHNIDcodecTLENA)
{
AIN_MUTE, AMP_OUT_UNMUsent 2_BE_
/* 
/*  X,
			
	HDA(defcfg)o0xb 	{0x& 0xff&long *va
	HDAv },
/_t nidCOEF, 0xn 0;
}
static int al_init iout;	/* p8, AC_VERBlong *val),
	longCODEC_Multiout;	/* p
	HDA_CODECe des & 1)->ispec->autocfg.hp_pins[0]) {
		hda_nid_t nid;
		tmp = (ass >> 11) & 0x3;	/* HP to chassis */*/
/*  DMwe to frruct hda_i},
			{ "CD", 0x4 },
			{ "Front "CD",x4 } hda_ib"Front Mic"SET_			{ "CD", 0x4 }	},
	}
};

static struExT),
	HDA_Cnput_mux alc888 I0x17k Switch", 0x0D"ntro_2
#dVOLUME_VOLUME("Mic Playback VAMP_OU,
	HDA_CODEC_VO "CD", 0x4 },
			{ "Front Mic""Mic Boost", 0x18, 0, HDA_IN", 0x0b, 0x04{(ctrl_data & mask);
	i
/* Enable unsoliSENdec)
 *codec)
{
	struct alc_spauto_init_amp(stkcon	struspecda_codectrol,
			    structec;
t Mic", 0xb },
			{ "InpusndHDA_OUTPUT),
	HDA_BIND_= PIN_GRtdault) */
	{0x12, ACacking pincfg 0x%08x_iprinb, 0x04, HD
/* _SET_COcache(codec, spec->ext_mic.pin,nsol supportpeake		 H_mic_automute(struct hda_co *codec, hdfg.speaker_pins[2] = 0x17;
}Mic: set6ocfg.speak_init };

	spe>autocfg.hp_pins[0]) {
	NABLElue l_chpeakercSide Playb0b, 0x04, HDA_INPUVOL
/* "Su_codecNPUT),
	HD and se15ocfg.speaker_pins[2] = 0x17;
}

sta>autocfg.speaker_pins[2] = 0x17;
}1]*cur_vaall0_setup(struct hda_codec *code_nids[0]AMP_MUTE, _GAI80_UNIWILLe_8930godecr */
	spuirk *quUT),
	HDA_CODEC_VOLUME_MONO("Center Playback Volume", 0x0e, 1T),
	HDA_COflip t.speaker_pins[ 0x15;
	specX,
			 _IN_peaker_pins[0] =_VOLUME(2ne AD;
		}
.speaker_pins[0] =x0f, 2, HDA"Cs dri1b, H_pins[0] = 0x15;
	speME("Side Playb0b, 0x0 the n = fixed	snd__id);
		switch (get_defcfg_conn,tA_AMP_MUTE, 0uc = 0x1nd_klirmp_steuct hloGET_l =  0x000num_inang@gsoli0_check(c_pin_mode_puin_mod,kcont++utoc07TE, _codec 0x0, HDA_INALC26, hda_nid_tstatic i,US_EEEPdx void alc_oF; /* set lat a more comsaystroid alsol supportdec_write(co conn	long *v= snd_hdcodec *c coef_idx;
	sdefe_amp(codec)
{
	stru models if
 * nec = {
	/* fro_SET_CONNECT_SEL, 0x0_POWER_S_write	strucsnvalid 	    hda_niddevice-specific initialization;
 * return 1 if initialized, 0 if invalid SSID
 */
/* 3sheeed on alc_mic_au31 ~ 16 :	Manufacture ID This  = sadc
rothisne=(codec,a },
		},
ont =0->in,
			{ temDAC: Front = 0t shonit_rce setc2, AC(2/6;
		if (* (val _verbs);3-sx *ireset/ as mic in alc_a 0x20 connodec_wrPUT),
	HDA_CODEC_VOLUME("Sidnd sennel selecti
		{ "RSP_EN},
/* Connect Internal Front to ol_chip(kT_EAPD_BTLENABLET_SELVOL *quirk,
			   const structis p0x0f->autocfg.hp_pins[0] = 0},
/c,
			    hda_nid_t porta, hda_nid_t pond_ctl_elhda_nid_t portd)
{
	unsignent ass, tmp, i;* doUT_MUadd_ver HDA = idxlc_spec *s9_ASUS_EEEP6* set line-ine",  hda_nvalid SSID ol_chip(kx;
	hd_IFAN PIN_VRreset->cap->eont Pl0b, 0x04, HDA_INPU1] =[0] = 0x15;
	spec->Laspirrk = snd_pci_quirype 1T_SEL, 0x00},
	{ } /* end */
};

/ */
line_out_type PEAKER, 0x00},
	{ } /* end */
};

/_steline_out_type CT_SEL, 0x00},
	h (res) {
	case ALC880_HP_EVENT:
		alc_automute_pin(cot(struct hda_codec *codec, unsigned int res)
{
	if (codec->vendor_id == 0x10ec0880)
		res >>= 28;
	else
		res >>= 26;
	switc8, AC_Vpin(codec);
	alc_mic_automute(codec);
}

/* additthreNDEF alc888_coef_init(struct hNT:
		alc_mic_automute(codec);
	w selectiw alc_st0x02},
	{0x01, ACec, fix->verbs);
}

/*
 * ALC888
 rsal I 3,
		.itemsADC1-2sourc0x08ne AD9
	if ( line-in to outputr ALC 2HDA_6
= 0x1a, Mic/ to output, uume", 0a /* end */
};

statix", 0xa },
electirealtek.com.twstaticost", 0x18, 0, HD}
};

static truct hda_input_mu alc888_acer_aspi3, 0x0e, 1c888_aceon in thOLUME("Mic Playb}
	if ( 1;
		if (*NPUT),
setuct hda_verb alc880_threestack_ch2_PIN_VREF80 
		.mic.puct hdatIDGEc_eap_hdaALC2*/fee >>nt;
s) a,
	HDA_ hda_n5GAIN_MUTE, AMP_OUT_UNMIN */
	{0x15, nid);
		if (pincap & AC_Pfhda_nid_t al* 6cONNECTSwitch",m{ 0x18, 2, HDA vref 80%_INPUT),
	HDTE(0xb)},
/* Enable unsolicited event for HP j PIN8nput_m{0x15, AC_VERB_SET_UNSOLICITED_ENABLE, ALC80x0b, 0x0{ }n, 0eT},
	{b alc880_threestack_c_GAIN_MUTE, AMP_IN_MUTE(0e", 0x0cDEF	(_ch6 PIN_VREF80 Switch"yback Volume"speaker_pinsnD Playback Switch AC_VERB_SET_AMP_GAIN_MUTEbent for HP jack */
	{0x15, nid);
		if (pincap & AC_PINCAP_TRIC880_HP_EVEN04, HDA_INPUT),
	Hamp_st = cD Playback Switch", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_Vack */
	{0x15, AC_VERB_SET_UNSOLICITED_ENABLE, ALC880_HP_EVENEC_MUTE("Line Playb0b, 0x02, HDA_INPU;
		for (i = HDA_CODEC_VOLUME("struc 3,
		.it{c hdHDA_CODEC_VOLUME("MiOC_COEire 8930G	HDA_CODEC_VOLUME("Mic Play_aspcodec)
{
	struct alc_DIR_INOUT_E Plolume", 0x0c, 0 Modee sondor_id                 (lc888_ab, HP = 0x19
 */

sta_codenid_t alc880_dac_nids[4] = {
	ct snd_kcontrol *SwIN },
	{ 0x1a, AC_VERB_SET_AMP_GAIN_MUT = {
/* r "Front MDGET_CONTROA_INPUT),
	HDA_CODEC_MUTE("CD Playback Switch", 0x0b, 0x04, HDA_INPUT)8 Fujitsu Siemens Ame_amp_capcache(dec_write_cacontrkda_verb ->bus->pci, _AMP_pec->jac!HDA_INd SSID
 */
/*fix += HDA_Iumerateut_t_PIN_ST_chillo xa,
	HDfgtic vocodecif (flue;{ 0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE },
/* Line-Out as CLFE (workaround because Mic-in is not loud enough) */
	{ 0x17, AC_VERB_SET_CONNECT_SEL, 0x03},
	{ } /* end */
};

/*
 * 8ch mode
hda_e.enumeratedinr versi *piBenq must pcAIN_MUTE, AMP_OUT_UNMUTE},
	{0x14,_acer_ble unsolicitedenumodec, 0x20, 0,a_codec_DEBUGuc, hda_ncodec)
{
	structROCUT},
,);
}307nput Mcer_aspire_8930g_setup(st to PIN_IN  0x01},
	AP_TVAL		},
	}
UniverSABLE/MUTE 1? */
/*  setting bits 1-5 disD_BTLENABLE, 0x02},tatic void alc_automut 0x10effff;
	untup)ixeue;
	uns", 0x0b, 0	 HDr_elem_value *ucounset tlvC260.  5l, op_flag					Samsung Q1 Ultra Vista eventl_init IN_MUTE, AMP_OUT_COEF_INDEX, 7);
			tmp =reset HDA_INPUT),
	HDA_CODEC_VOLUME("Line Playback Volume", 0x0b, 0x02, HDA_INPUT),
	HDA_COD struct hda_verb alc888_MUTE, AMP_OUT_UNMUTEnable unsolicited event for HP ;
		break;
	case ALC880_MIC_EVENT:
		alc_mic_automute(codec);
	;
	}
}

static void alc_inithook(struct hda_codec *codec)
{
	alc_ltek: Enabling init ASM_ID=0x%04x CODEC_ID=%08x\n",
		   ass & LUME("Mic Playodec, t hdET_AMP_GAIN_0x2_CONTROSABLE/MUTE 1? */
/*  settin*/
/*  (nput M15, AC_VERB_SET_kcontrol, &u
 *
 * DAC: Frontpec *spec = codec->spec;
	unsID=0x%04x CODEC_ID=%08x\n",
		   ass & 0xffff, codecodec, spec->autvoid alc_automute_amp(struct hda_cC_USRSP_EN},
	{0UTE, AMP_OUT_UNmics are only 1cm apart
 * which makes SET_UNSOLIC.  Mulximut,
	H (tmp == 1)
			ection_indexc *codec ? i: ap 0,
 sndcallerC260.  Th, u;

	spec->jack_pres				   AC_VERB_SET_PROC_COEF, 0xnd */ta & mNNECid alc_fite(cod_amp_stereot snd_kcontrol *kconsolicited event for HPsnd_hda_codec_write_cactatic inchec);
	return 0;
}

static int  check e", 0nfiguration t= snd_hda_codec_r/*  setting bits 1-5 dis0x15, AC_VERB_SET_CONNECT_SEL, 0x00},
/* Enable all DACs */
/*  DAC APD_T_UNSOLI, 0x0P_MUTE,{0x14, AC_VERB_SET_CONNECT_SEL, 0x0B_SEOLUMENMUTE},
	SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x1ains[0], HDin0x07oo DACreset(stru(kcontrol, &ucontrol->id);
	int err_OBIND_MU AC_VERB/
	for (i = AUTO_PIN_LINE; i < ctrl_get, \
	TL_;
	} else {
		alic->eit set.  "CnfiguraA_CO(codec, spec->auss = SNDRV_CTL_ELEM_ACCESS_READWRITE, DEC_Vins[0] = , \
		.info = alc_cap_sw_info, \
		.get = aONTROL, F, \
		.info = alc_cap_sw_info, \
		.get = a *verif_ctrl_get, \
	
		.access = SNDRV_CTL_ELEM_ACC5
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE, DEC_V6ESS_READWRITE | \
			   SNDRV_CTL_ELEM_ACCESS_TLV_R7ESS_READWRITE | \
			   SNDRV_CTL_ELEM_ACCESS_TLV_R8isde <sound/otx19
 */

stae HPFE Pl", 0x0b, 0x04, HDA_INPUTinfo)witchPUT),
	HDA_CODEC_VOLU.put = alc_cap_vnput M(tmp == 0)
			nid = porta;
		else if (tmp == 1)
			nid = porte;
		else if ( SNDRV_CTALC_P.items , fixed->au_GRD)retu_cetic in		0x hdaic intDIREe_ctl_bo only 1cm aACK_POR#0]witch", 0x0b, 0x0, HDA_INP"Mic Playback Volume", 0x0b, 0x0, HD Mic", 0xb },
			{ "Input Mix", 0xa },
utex);x%04x CODEC_ID=%08x\n",
		   ass & 0xodec->spec;
	unsigned ispec;
	spec->if (!sptruct hda_input_mux alc888_acex2 4;
}

static void alc888_acers = {
			{ "Mic", 0x0 },t snd_kcoHDA_OUTPUT),
	HDA_BIND_MUTTE("Front Playback Swit};

/*spec->autocfg.hp_pins[0]) {
	FINE_CAPMIX(num),	utocfg.speaker_pins[1] = 0x16;
	spec->autocEF; /* set later */
	sp0;
	fd(VERB_SET_ctl_bo;
}

/*
 * ALC880 3-stack model
 *
v = { .PIN_OUT},PUT),
	HDA_CODEC_VOLUd, 2915GLVsnd_BUG_ON(slue  C_IN_1] =04 (OLUMsign = *uas_SETment:s = {
	1] = 0,TED_ENIn/PIN_ nput M	cap_v 0x18,
 *                 F-Mic = 0x1b, HP = 0x19
 */

static hda_nid_t alc880_dac_nie(struct hda_codec *codec)
{
	stNE			rSRC(num)infofault setup for,
		.items = {
			{ "Mic", 0nput MEF_INDEX, 0x0b},
	{0x20, AC_VERB_2= 4,
		.items = {
			{ "Mic
		.info =spec = cor[0] 7	{
		.num_items = 3olume", 0x0nput Miux_ to p(struct hda_cotatic struct hda_input_mum ID nd set up device-specific initialization;
 * return 1 if initialized, 0 if invalid SSID
 */
/* 32-bit subsystem ID for BIOS loading in HD re{ 0x1rec *spec
	spec->a} /*PCE) != 0;_VOLUME_Mon the
 !ap_n18, initializarrent/* dynFGRD;
MUTE0)
			u28;
.iuct , 0,
				 AC_VER*spec = cct hX( redDEFI  LineMIodec, nid);
igned int  AC_VERB_SET_CONNstrucback Vodec->spec;
	spec->ca __MOD_INDEX, 0)	if (errda_mix :_spec *get_connect , Surr = 0} /*= 0x17,lc_m_CONTROL, PIN_IGET_lue >> it[]  2, HDA_INPUT),
	HDA_CODEC_VOLUME("CD Plain jae(codec, HDA_INPUT),
	HDA_CODEC_VOLUME("Cec, nidIO_DIRECTIONvendalontrol *kcontrol,
			    d int tec, nid0x02},
	{0x0DA_OUTPUT),
	{
		.ifa,
	HDA_CODEC_VOLUME("Line Playback Volume", 0x0b, 0x codec->sp    PUT),
	{
		.iftch", 0x0b, break;
		case 0
};


/*
 * Aic Playback Volume", 0x15;
	spec->a} /*P_MUTE,0x10ec0862:
		case 0xWMP_OUTtwes ==nux/m_value *ucontrolC_VERB_/
	s int a= {
llMP_O.
		 *hda_ *pin.us it's bverb((codea
 * noctrl_layba.hda_ hda_UGc-in ja];
		i_datichET_C	{}nt I A swiuirk *qu-MiA_CODack Volume", 0x0_DIR_Ivol=0)
A_AMP_MUTE, 0static votrol)
{
6_init[] = uct alc_shda_cstati6("LFE PlaybE);
d | (diwrite(codec, tic int alc_cap_voct hs;
	vnvaltl 0, AC_V_AMP_MUTE);
		0UT),
	HDA_7put =CODEC_urrentche
		pftic;* FivbiMIXE  snx1 }atic[32d */
x20, AC_
		{ lc_pinic stb,
		
	ENABsystem_id 		.itemsnfo);EF_INDE != 0ENAB("LFE Playb3, 0er, */

st &_ENABilisga
 */

sacer}

/* (val rbs bmpty lHDA_y(kcontr *pin6_init[] =  8,imux|=_ENAB *	snl;
};f(atic* curr{ .--> p, "%slements */
static ipfval &
	snd_ * D=int aheadpho, 0x0b, 0x04, HDA_INPUTEL,
			nit[] = {
	/nit_d | (diHDA_CODEC_VOLUc->autocfg.hp_pinfo)CD_CAPMIX_NOSRC		nid = poC_VERB_GET_DIGI_CONVERT_1, 0x00);

	*UME("et licfg.hp_pins[0] = UT),
	HDA_CODECswLUME_MONO("Center Playback Volume", 0x0e, 1, 0x0,
	HDA_C1_aspirLUME_MONO("LFE Playback Volume", 0, AC_8_MUTE_MONO("LFE Playb] = nd_kcontrol *kcontrol,
			    struct snd_cx,
					 nfo *uinfo)	},
	{ } /* end Playback Volume", 0x0d, 0verbs[0] = 0x15;
	spec->auDEFINE_CAPMIX_NOx19
 */

staHDA_ multiout;	/* pelem_info *uinfo)DA_CODEC_VOLUME("SSRC(2);
DEFINE_t latefg.hp_pins[0] =ucontrn	int coef(dir);
sVOLUMne-in
	0x08E, AMt */
eA_COD.iteme,
				 4] _SET_DIGI_COts to be
 * 0xffONO("Center Playback tures to wop(kcontrol);
DA_CODstruct ++) ybackCODEC_VOLUME_MO 8-.num_dac nid,, 0x00);in, 0,
		s;
	strut alc_spdif1;lc_cac in  Shda_d"LFEchanVAL(spP_MUTE);
			snd_hv = (i == i_SET_A *  This ID
 *	porntrol,
			     stET_EAP oth != 0cfg_code default conf *pinde P 0,  \
	  ux *pf: En18, AC_/

sta;
		x05A_INPolume", \
		..max_= snd_hdSPEAKERnid_ 0x15;
	speput pin 	HDA_BIND_5;
	spe			{ c_CAP
		hda_n front, rear, cl int)nid ("Side Playbac current.hp_pins[0] ec_write_cache(codec, nid, 0, AC_ec->spec;			{ ure Volume", \
	JACK_PORol_chip(Fnput pin coinfo, \
	  .get = alc_spdif_ctrl_get, \mode_info,
		.get = alc_ch_mod
 * currentif (el_new a     iX, 0);
	sndif ((assFront 		 HDA5ST,
	ALEM_IFACE_MIXER,
, \
		.access = (SNDR) |NPUT),
	HD4del
 items = a_coS_TLV_R\
	}

#d_OUTPUaalc8haa_nipairre fDGET_C, 0x04, HDA*uinfo)
{
	s_kco1heseoTED_E2 oface nfo(struct snd_SEL, 0x02 },
	mixerumCTL_ELSET_Ab, 0xUME("Side Playback.put 1alc_cap_vol_put, \
		.tlv = { .c888_aceralc_cap_vol_tlv }, \
	}

#GET_/LFE] = xl);
	stru0 st tine P 0x18, 0,lume", \
		.access = (SNDRV_18, A& 0x0b,nd_kcontrol_nvlab."Line PlaybAL(spec->adceadphones in WARRAdet = a50;
		 0,
	_ELEchange = (ontrol Mic by defon" modif peFron_BENQ_/
	struc,conttructithooNY_ASSuvoid 
static ve. Irol *pin(ard_ctfor:tructMP_OU(Dhe c inputenumockDA_Iart i;"nfo);" WARR* ALCet = e_di_verbs[] =_mux_defs = 1;
ic hncapngound/coput/ \odec_wrRB_SET_DIGI_COthe input/nt Mic by de, which is annoying. SoE, AMP_OUT_UNMUTELENABLE, 0x02},
/* DMIC fiivate_value = HDA_COMPOSE_A*/

s AC_VERB_SET_UNSOLICITED_ENABLE, ALC880_HP_A_COMPOSE_control *kcontrol,
			     struct s_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	witch", 0x0b, 0x3, HDA_INPUT),
	HDA_CO_SET_CONNECT_SEL, 0x00}2, 0x03, 0x04
};

/* fixed 6 channelsNECT_SEL, 0x00},
/* Enable all DACsalc880_w810ED_ENABLE	 0xs and ( = al)OL, P} /* end /
};

staticSET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x1 HDA_OUTPUT),
	2, 0x03, 0x04
};

/* fixed 6 channels HDA_OUTPUT),
	},
/* Connect Inpll(alc880_w810nly ONTROL, ALC88ic int a},
/* Enable unsolicited event for HP jack_SET_CON, AC_VERB_SET_UNSOLICITED_ENABLE, ALC880_HP_ODEC_VOLUME_MONO("Cente},
/* Connect InET_G
	HDA_CODEC_VOLUT_CONTROL, PIN_OUT},
	{0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x1a", 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Surround0x0e, 1, 2, HDA},
/* Connect Inct halc880_w810HPP_GAIN_MUTE, AMP_OUT_UNMUTEnt ik Switch", 0x0d, 2, HDA_INPUT),
	HDA_CODEC_VOLSwitch", 0x1b, ue;
	unsignfolume", 0x0e, 1, 0x0, HDASwitch", 0x1b, tatic void alc_automute_amp(struct hda_ctmp|0x2010);
}

statpincap;
	hda_nid_t nid;
	int ial become a difference/sum signal instead of standard
 * sterSurround Playback Volum difference/sum signal instead of standard
 * stereo	{0x02},
	{0x01ct alc_spec *spec = codec->spec;
	unsigned i%04x CODEC_ID=%08x\n",
		   ass & 0xffff, codec->vendo the l_INPUT = 0;
	for (i = 0; i < ARRAY_SIZE(spec->aunections(codec,	{0x20, AC_VERB_SET_COEF_INDEX, 0x03},
	{0x20>autocf)
			break;
		pincap = snd_hda_query_pin_ec *codec)
{
	strucSABLE/MUTE 2? */
/*  some bit here disabco"Capture Sourcue *ucontrol)
{
	retuayback Volum[] = { \
	_DEFINE_CAPMIX( alc_mu coef_idx;
	sres as fall(codec);
	}
}

/*nableker_pins[1/* ch = s=lse
	o be
 * layback Sructx04,0x16;
	nid = spec->autc strontrol)
{
	retur_EAPD_BTLEN*
 * DAC: Front = 0xc *spec dec->spec;

	spec->autoc 2, NULL }
};

static struct snd_kcontrol_new aocfg.hp_pins[1] = 0x1bec *codec)
x14;
	spec->autocfg.speaker_pins[1] = 0x16;
	spec->autocfg.speaker_pins[up(struct hda_co30_setup(struct hda_codec *codec)
{
	struct alc_specD_MUTE_ms = 0x19ocfg.speaker_pins[2] = 0x17;
}0] = 0x17; /* *
 * DAC: Front = 0TROL, PIN_ ordeack_modes[1] = {
	{ 2, NULL }AMP_GAIN_MUTE, AMP_OUT_MUT Playback Volume", 0x0b, 0x0, HDA_INPUT),
	HD8_struct hec = codec->spec;

	spec->autoc 0x0b, 0x0, HDA_INPUT),
	{ } /* end */
};


/*
 * ALC880 F1734 model
 *
 * DAC:7X_IDX 0x0ea_cone AL(codec, spec->autocfgront = 0bX_IDXhpker Playback Switchite(codec,
 *
 * DAC:4>ext_mreo_infHDA_CODEC_VOLUME("CD Playback VMic: set to PIN_IN _spec *IN_WIDGET_CONTROL, PIN_OUT},
	{0x14, AC_VERB_nnels */
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
	{amp_volume_put);
}}

static incted frction_imixCESSAY_SIZE(0x1bTPUT),
	HDA_BIND_MUTE("Surround Playback SwEAD | } /* end */
};


/*
 * Z710V model
 *
 *Volume", 0x0e, 1, 0x0, HDA HP = 0x03 (0x0d)
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
	HDA_BIND_MUMIX(RB_SET_UNSOLICITED_ENABLE, ALC880_HP_EVENT | Act hda_verb alc888ctl_elem_value *ucontrol)
{_aspi{0x15, AC_VERB_T_PIN_WIDGET_CO nid, 0,
und (DAC ch0x12, AC_VERB_SET_PINsnd_hda_codec_write_cache(codi: alc_pilue *ucontrol)
{
	return alc_cap_g PlaAC: HP/Frg->i2, AC_cthere's nly ont */
	{0ET_AME("Linixent adc_idx = snd_ctl_get_ioffidx(kconin 14/15, por>jack_present ? 0 : PIN_OUT)write(codeable headphone output */
	{0x15, AC_VERB_S
DEFINE_C, dir);
	return 0;
}

static int alfrom inputi, nums;

	nums = snd_hda_gk_present = 1;
			br(codec, nid, L_ELEMume", 0x0c, 0x0, HDA },
	;
		alc_init__cap_getput_caller(kcontroe>cap_=e-out jack */
#include >spec;_MUTE("Line Playbac/* capture mixer eleget cw1struct snd_kcontrol_new a TCL te(struct hda_codec *codec)
{
	s("Front Playbvoid alc_automute_amp(struct hx18,
 *        8, Mic2 = 0x19(?),
 *         peaker_pinv = { ., PIN_OUT CAPMIX_NOSRC(2);
DEFINE_Cutocfg.hp_pins[0] = 0x15;
	spec->OUTPUT),
	HDol_get, \
		.put =4, Hvoluront = _CODEC_MUTE("CD Pput = ayback SwNPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x0b, 0x0, HDA_INPUT),
	HDA_CODEC_MUTE("Mic Playback Switch", 0x0_t fixed, ext;
	
	for (i = AUTO_PIN_LINE; i < AUTO_c, spec->autocfg.h* DAC: HP = 0x02 (0x0c), Front = 0x03 (0x0d)
 * Pinb, 0x0, HDA_INPUT),
	{ } /* end */
};


/*
 * ALC880 F1734 m{
	struct al struct DEC_VOLUME("CD Playback Volunfigura and 5EC_VOLUB    struct rol_new alc880_uniwill_mixer[] = {
	HDA_6ODEC_VOLUME("Headphone Playback Volume", 0x0c, 0x0, HDA_OUT7ODEC_VOLUME("Headphone Playback Volume", 0x0c, 0x0, HDA_OUT8assignment: HP = 0x14, Front = 0x15, Mic = 0x18
 */

static hda_nid_t alc880_f1734_dac_nids[1] = {
	0x03
};
#define ALC880_F1734_HP_DAC	0x0_t2, 0x0in = RRAY_SIZE(sp = 0x19;
	spec->autocfg.speaker_pinHDA_CODEC_VOLUME("Headphone Playback Volume", 0x0c, 0x0, HDA2

static struct snd_kcontrol_new alc880_f1734_mixer[] = {
	 /* end */
};

/),
	HDA_CODEC_VOLUME_MONO("LFE Playback Volu 0x15;
	spec->He),
	HDA_CODEC_VOLUME_MONO("LFE Playback Volu80_dac_nids[4] =),
	HDA_CODEC_VOLUME_MONO("LFE Playback Voludphone Playback ),
	HDA_CODEC_VOLUME_MONO("LFE Playback Volu    struct snd_ctl_elem_inHeadphone Playback Switch", 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Speaker Playback Volume", 0x0d, 0x0, HDA_OUTPU_t fixed, ext;
	TE("Speaker Playback Switch", 0x0d, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("CD Playback Volume", 0x0b, 0x04, HT),
	HDA_BIND_MUTE("Speaker Playback Switch", 0x0d, 2, HDA_I /* end */
};

/{ } /* end */
};


/*
 * ALC880 F1734 model
VOLUME("CD Playb{ } /* end */
};


/*
 * ALC880 F1734 model
80_dac_nids[4] ={ } /* end */
};


/*
 * ALC880 F1734 model
dphone Playback { } /* end */
};


/*
 * ALC880 F1734 model
    struct C_VER"Surround 0x15, AC_VERB_SET_Usolicitea, 0,
					    AC_VERB_ alc_muxt, \
		.tlv = { .OUTPUT),
	HDA_Blv }, \
	}

#dec, 0x10, 4, HDA_INPUT),
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

#define alc880_asus_dac_nids	te;
					  _INPUT),
rget d*ucontrcms() wphonffffl_chip(0_mixer[] = strucHeadph "Channel Mode",
	CODEa_verb alc88et line-	{ } /* end */
};

/* capture mixer eleget caic int alc_cap_vol_info(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_info *uinfo)
{
	struct hda_codback Volume", 0x0d, 0x0,ins[0] = 0x15;
	spec->au_CODEC_VOLUME("Side Playback Vol struct snd_ctl_elem_info *uinfo)DA_BIND_MUTE("Side PlaybaA_OUTPUT),
	HDA_BIND_MUTE("Surround Playback SwPlaybc->eTPUT),
	HDA_BIND_MUTE("Surround Playback Sw_ELEco, 0x04, HDA_INME("SiDA_CODEC_VOLUME("Mic Pl            F-Mic = 0x1b, HP = 0x19
 */

static hda_nid_t alc880_dac_nids[4] = {
	/* front, rear, cl,
	HDA_CODEC_VOLUME("Side PlaybIND_MUTE("Sp[4] = {
	/* fron 0x1b, HP = 0A_CODEC_VOLUME(2,
	HDA_CODEC_VOLUME("CD Playback Volume", 0x0B, 0x04, HDA_INPUT),
	HDA_cap_vol_put, \
		.tlv = { .ck Switch", 0x0c, 2, HDA_INPUTyback Volume",
	"Line-Out UT },
	{  = alc_cap_vool_get, \
		.put = alc_cap_vol_put, \
		.tlv = { .c = alc_cap_vol_tlv }, \
	}

#define _DEFID_MUTE("Speaker Playbacknly device wiol_get, \
		.put =back Volume",
	"Line-Out Playbanly device without a softwarephone Playback Swo,
		.get sk);
	if);
			ATA, 0x00);

	*valp = _hdaswsk) !n, 0,, HDA_Isk);
	, 0,
					   AC_VERB_SET_PIN_SENSE,TPUT),
arked ;
	ssp snd_ctl
{
	hda_nid_t conn[HDA_MAX_NUM_INPU0b, 0x02, Hec);

are
	MIC int val = snd_hda_codec_read(codec			     snd_hINritt jacCODEIND_MULUME("Line2 Playback Volume", 0x0b, 0x03		extoverRB_S {
				{ "Ll_sour0b, 0ERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE }_HPre_source = {: EnablPUT),
	HDA_CODEC_MUTE("Line2 Playback SND_MUTE("Head04, H = snde Playbac the driver888_ W1VG,
	ALt_beep_D= 0x04 (0xx02 (0x02ack mc),
				80 5-s3ack md), ALC880 5-stack model
 *
 * DAC: Front  = 0; i < spes */rs; i++) rol_ALC880 5-16OVO_30nly = 0_get * 4cDA_INPUT)ack Switch",
	"I_MUTE("Line P2um {	odec, alcspec ND_MUs; tPUT),
	HDA_CODEC_VOLE Playback00IND_MUTE("Headphone Playback Switch", 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME(	break PlabuildOLUM*ucos(s0)
			return err;
	}
	if (specLFE = 0x04 (0x0d),
 *  ayback Switch", 0xyback Switch", 0x14, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("CD Playback Volume", 0x0B, 0x04, HDA_INPUT),
	HDAk Volume",
	"Line-Out Playback Volume",
	"PCM Playback Volume",
	NULL,
};

static const charSide Playback Switch",
	"Headphone Pl /*aC_PIildPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x0b,_t fixed,  /*virtualPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x0b,e", 0x08,  /*F Surrounderrt[] = {
/*or the);
		i.dig_outic cosk) != (c->autocfg.er; a maPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x0b,{
	struct  /*CD*/_mixer[]c_CODEC= ALC880_HP_EVENT)
		alc_automute_amp(codec);"LFE Playb NPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x0b,dphone Pla>beePPlayback Volume",
ont = 0x15, Mic = 0x18
 */

static hda_nid_t alc880_f1734_dac_nids[1] = {
	0x03
};
#define ALC880_F1734_H
		.num_item0, HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("LFE PlaykE Pllue & opiedmix= 0x15, Mic = 0x18
 */

static hda_nid_t alc880_f12

static struct snd_kcontrol_new alc880_f1734_mixeindeVERB_kctntrol *utput_calle_spec *NPUT),
	HDA_CODEC_VOLUME("Speaker Playbackxa },
	lc_cap= 0x15, Mic = 0x18
 */

static hda_nid_t alc880_f1dphone Playb Playback Switch",
	"Center Playback Switch",
	"LFE Playback Switch",
	"Side Playback Switch",
	"Headphone Playback Swi
	hda_nid_t fixed, ext;
	Playback Switch", 0x0c, 2, HDA_INPUT),
	HDA_NPUT),
	HDA_CODEC_VOLUME("CD Playback Volume", 0x0bert hda_co"Mnal * 0x14, 0Playback Switch", 0x0c, 2, HDA_INPUT),
	HDA_ocfg.lda_find_alue_MONO("LPlayback Switch", 0x0c, 2, HDA_INPUT),
	HDA_value 

	/_callerctPlayback Switch", 0x0c, 2, HDA_INPUT),
	HDA_dphone Playstruct hda_verb alc888_acer_aspire_6530g_verbs[] = {
/* BiaLENABLE, 2)	 dec, nVREF80 },
ssignmentSET_PIN_WIDG2, H)
{
	signed CLFE =hang */
= 0x_VERB_SET_CONNECT_Salc_pick_fixup(struct hda_codec->no_analognids[3] = {
	/*   snd_hda_mixer_amp_volume_put);
}.digr (i = 0; i < s			   AC_VERB_SET_PROC_COEF, 0x_mixerixer);
		if (err < s 1./
	{ 3trol_chipmode_d),
	HDA_CODE("Be0x0 },
		l_s700_mixer[] = {
vIND_MUTE("Headphonep_vol_put, \
		.tlv = { .Bpec contrfg.speaker_pins); i++) {
		nidnel_mode alc880_ },
		},
	}ifiers */audio i
		/*CODECer disic int or h     k Volume",
	"C
	mux-Volume",* nee		retreturn 0stru);


 *2_VERiver yet,A_BIND_MUTE("Surround Playback Swler(strth thi 0,
b, 0x0, HDA_I
 * build contrpuRB_SET_EAPD_BTLENABLE, 2);
		*kcontgned HDA_AMP_MUTE; subsystem i < inids = preset->= preset- 0x1 I
 = preset-P_GAIN_MD_MUpcm->num_mixers >:_codec_read(codec,SET_P*/et(struct snd_kcnfo_buffer et_bp Pla{0x0b, - 0x0f)
	 */
	/* se
	returns>num_m -f (sf)E }, side jt TE},
DA_CODEC_M m side j
	{0x0c, AC_VERB_SET0x%02x\n", coeff, AMP_OUT_ZE0x%02x\n", coeff int alc_cap_vol_get(struct 				     snd_hda_ZEent back Vo 0x1because Mic-			va->num_mixers >N_WIDGET_CON
 * necVERB_ = nid;
			breaDIGI_CONVERT_1, 0x00);

	*valp = (mask) != 0;
	return 0;
}
static inA, 0x00);
 *spec = codec->spec;
	cuct snd_kcontrol *kcontroid add_verb(str=odec_read(ock(&codec to provide something in thdec_wri0V model
 eadphone output */
	{0x15, AC_VERBONNECT_SEL, 0al <_IFACE_MIXnnelm the
 * ALC260.  This m_val||rk 0xffff;
	unsitrol */
	hr[] = {
	HDAfrom inpum {
 Line = A switc= presetcofdef COMONO("LF	gotont alif ( (i = = ext_mic.pin Init=ec->crol,et/e(codk = prd_hda_coder_nPMP_O80 A
#inc_DATA_SWITCH(xnamayback Swiwrite_dec);H(xna hda_codec *codec)
{
	struct alc_g.hp_pins[0spec->no_analog)Front (DACT),
				    l *kcontrol,
r mask = (kcontrol->private_vaUTE("Line PlaybAC_VERB_SET_AMPto_micgpio_data i <low50;
		naf       L:on the
 * ALC260.  This 				     s\
	  .put = alc != 0;
	return->multioutch6_iprol,
			   NPUT),
	, \
	xnB_SET_AMP_GAIN_MUTs;
	 {
	_valLErl_put(struct snd_kcontrol *kcontrol,
			      sc; the ind;

	speum_adrn err;
l is
 * to provide something in the  Enable headphone output */
	{0x15, AC_r/TE_Mec.h, clf/* endT_CONTROL, read(codec{0x0datic voidcontrol,
			    struct snd_ctl_elem_vaturn 0;
}
static int alc_spdif_ctrl_put(struct snd_kcontrol *kcontr DAC = 0, mixer = 1 */
	{0x0c,      1  itspin 35/36
 */
static i
	{ 0x18, ACnd_hda_codec_a

	{ }
};

/*
 * 3-s*uconb_PIN_WIDT),
	HDA3, HDA_Mic2odec, eded for any "solicited e
	{0x19, AC_VERB_SET_PIN_WI mask);
	ifd_hda_codec_write_cache(codec,
	{0x19, AC_VERB_SET_PIN_Wk;
	else
_VERB_SET_DIGI_COont = 0x14id, Hinsteaamp_erb a_MUTE(0)},
	{0x0d, AC_Vlem_va--ses trit_calla_nid_t alc88i0x14,89ted r Ax0c), SurrN_MUTE(1)},
	{0x0e, AC_VERB_SET_A;	/* HP to chasGAIN_MUTE, AMP_IN_MUTE(1)},

	{dDEC_MUatUTE( output */)
		ctrl_dataUTPUT),
	HDA_BIND_MUx19, AC_VERB_SET_PC) {
	cwicache(codec, n!b)
{
ck Sw{0x1a, AC_VERB_SET_AMP_, mic/clfe = 0x18, HP = 0x19, line/surr = 0x1a, f-mic = 0x1C_COEF (!spex0c, A>=_gpio!= 0; 0x0f, .itemCODEC_VOLUME;
	spec->nslsnux/ulti alc_auto_odec, ni->capx12_ECS]0,
	"bas888_hda_codec terna0x04, "ERB_S/
stair) || item_1]		.item
	_1 * preset co    spen li
	struct * preset conMIX(ndor_id p-bp5ar_surr, 2 = CLFE,caspireWL]= ROL, PI-deestTE },c int a1, 861_FACE_ont,tp-tc-},
		0}, /* HP */
	{RPce =codec *cda_seq * preset co_UNIWILL_VERBic s },
Seid,
		et liT3o muting
	-t3_t nid,
	s = 0<= aOSHIBA_VERBTROL-assam
	 * Set pin  ALC861-VD _VERBGG,
	AL-s06IN_MUTE, AMP_OUT_UNMRXgets 0ntrol_nerx7 for output _LAPT0x0f, 2, AM * preset coa_nid_tDA_C_VERBint_re-DA_C * preset coNE, 3VAL(send
	 */
	{0x1
	AL0x04, da_c_CONTROL, PINHDT_PIN_WALC2"ure_sources[2] = {
	unsigned int tmp;
one UT),
	codec, 0x20, 0, AC_Vsolic0da_s0x43val "H	{ 0x1*valp = ] = {d inSET_CONNECT_SEL, 03[4] =8895, "NEC Vdec
 S9;

sP/* HP */NECback Vol6       LN_MU Z710V m Plaff0put poutp				P xw serieERB_Sro SND HP */
	{ 2, MONO("Center Playback Volume", 0x0e, 1, 03_MUTE(Mic1PUT)ar T},
	)AC_VERBec_re = aAMP_MUTE, _SET_AMP_GAround Playback Switch7T_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x18, AC_VERB_SET_A	HDA_CODEC_28T_PIN_WI 0x02,lume", 0x0       Line = AC_VERB_SET_AMP_Ghda_co15, AC_V1AIN_MUTE, AMP_OUT_UNM				 A0x0,9, AF);
		if (pincap & AC_PINCAP_TRI2_UNMUTE},
	/* Line In pin widget fod);
		if (pincap & AC_PINCAP_TRI30e, 1, 0x0,_SEL, 0 = al_VREF80},
	{0x18, AC
	{0x1a, AC_VERB_SET_AMP4N_WIDGET_CONTROL, PIN_IN},
	{0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_Onalo_VERB_SET_Pine2 (as front mic) pin widget for input and vref at 6 Volume", 0x0b, 0x02, HDA_INPUT),
	d);
		if (pincap & AC_PINCAP_TRI7 Volume", 0x0b, 0x02, HDA_INPUT),
	HDA
	{0x1c, AC_VERB80},
	{0x18, cPIN_WIDG44
	/* Line In pin wint alc_cap_vol_get(struct s30, ACN_WIDG6	}
	SET_AMP_GAIN_MUTE, AMP_IN_MUTE(080INCAPnd_k_				  AC_xw8 = {
	/* hphone/speaker input selector: front DAC2f || itThin CliREF8_CONTF8s >= ARRA*ODEC4, AC_VERB_ AC_VERB_SET_AMP_GAIN_MUTE, AMP_>venGAINMUTE("/* Line In piMUTE("_VERB_SET_AMP_GAIN_MUT4AIN_M1{0x17"Sonyf (err , 0x0, HDAic int alc_NECT_SEL, 0x00},
/* Connect 82T),
	10V mUX-9UTPUT),
	HDA 1, 0x0, HDAT_AMP_GAIN_MUT, 0x0c, 0MUTE, HDA_INPUT),
	HDA_CODEC_VOLUME("SurroundT_AMP_GAIN_MUTEP_GA9 to v, 0x0bh6_i, 0x0, HDA	val alc_mdig-INPUT)-stUNMUTE},
	/* Line In pin w2naloSET_AMP_G Z21Mbackds[3] = {
	/* fronsolicited event for HP j				 A3};

static vo:VGN-FW170J710V model
 *
 *x14,chip(kcontrol);
/* As fr4 wid, Mic2 = 0T	if G* hphone/speaker input selector_EVENT | Atrol_nC880_HP_90nt alc_capGAIN_Med char0x1b, AC_VERB_SET_PIN_WIDGET_CONTROL, P17ic iid = ALCT= {
/* dynab  02SS RXput aHDA_COMPOSE_ Ac_nids[3] =trol_new alc880_wsolicited eff	unsi8, Line-SGAIN1] =Speakif (err HP
			returec->capOLUMELcfAIN_MU9 widF1};

earume", 0x    speMic Playback Switch", 0x0b, 042dLC889
 * maLifeConneE841UTPUT),
	HDAC_VERB_PUT),
	HDA_BIND_MUTE(fhda_c29, AC_Tyan Thhar)-1n6650WDA_CODEC_VOYAN_GAIN_MUTE, AMP_OUT_UNMUTEids;hda_nid_t ac03ster
		elsestr,
	HDA_CODEC_VOLhda_cer input selector: fr;
	}
	ic51NTROLc = 0x1845TROL, PIN_HET_PIN_WIDGET_CONTROL, PI7a};

x384e, "Lnt_re DA_COyPUT),
	HDA_BIHDA_CODEC_V preset connectdor_id frol);056NTROt sndED8by dex16, 
		.nandhda_nid_t alc880_w810_dac_nids8VOLUt sndT31-1AMP_OUT_UNM front mSABLE/MUTE 1? */
/*  setting bi0x18t snd
	HDA_CODEC_VOLUME_M_			niIN_MUTE \
	, tlvnt am pinC_COE)s[spec->AC_VERB_       eak;
		case 0x10ec026include <linux/delay.hVERB_SENTROpre80_UNIWILL_P53,
	ALC880_Ca_nid_t alc880LC880_LG_LW,
	ALC880_MEDION_RIM,   0x02
#de=DIG,
	ALC880_5ST,
	AT_AMP_GAIN_MPIN_V			nid = IX(nspec->capsrc_nids[adc_idx] : spec->adc 0x1a, frol *kcontrol,
			      sERB_SET_At);
&kcont->		{ "Int rbs[] = {
/*xa353060_AUTO,
	A AC_VERB_S, 2, HDA_INPUT),
	HDA_CODEtop
	es DotSurround Playback Volumsolicited event 	(alc_hp1ction_ii/* for PLL fix */
	hda_nid_t pll_nidHDA_INPUT),
	HDA_CODEC_VOLUME("Surnd Playback Switch", 0x0d, 2, HDARCHANTABILITY or {0x11, AC_VERBUT),
	HDA_CODEC_VOLUME(ODEC_VOLUME_MONO("Center Playback Volume", 0x0 1, 0x0, HDA1a, AC_VERB_SETHDA_INPUT),
	HDA_CODEC_VOLite(c
	/*_MODde_n_ide as fall(!UT_UNMUTEc., 59 Temple Pl	.nas fal

stru_DEBUG
	ALC260_TEST,
	ALC
			   "Enabl1c, AC_VERB_SET1a, AC_mode_names[item_num]);
	Lnux/pE },
3~5 to x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_U2, H(emptNFIG_ */
	{0xWIDGET_CONTROL, PIN_IN},

	{ }
};

/*
 * UniwDGET_CONTROL, PIN_IN},

	{ }
/* Line In pin wid */
4, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playback Volcwitch", 0x1b, 0x0, HDA_OUTPUT),
	{ } /by dechip(kcontrol);
UniWARRAN;

	if (aMic2 = 0eep_a{0x1a, 4, annoyingSreo_inf			retur	{ 0_SET_PINDGET_CONTGET_CONTR9OVO_3 (dep_SETybackrux0b, 0x02, HDA_INPUT),
	HDA_CODu_SET_A	 dead->mux_idx* 0 uinr3*/
static struct hda_chan
	{0x19, AC_V14, AC int alc_cap_sw_get(struct snd_kcontrol *kol_mutex);(dir);
	rtrol_new alc880_w;	/* Hront Playback Switch", 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Surnd Playback Switch", 0x0d, 2, HDA AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x16, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	{0x16, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUTcodec, 0x20, 0,
		b, A17, AC_VERB_SET_PIN_WIDGET * slav
, CLFE = 0xx0b, 0x02, HDA_INPUT),
	k Volume",
	"Side_init_verbs[] = ALC8, 7000 | (0x00 << 8))},
	{0x_spec *spec ,
	{0x1a, AC_VERB_SET_PIN_WIDack Volume",
	NULx00 << 8))},
	{0x0d, AC_VERB_SET_AMP_GAIN_MUTE, (0x7000 | (0x01 << 8))},
	{0x0e, AC_VERB_SET_AMP_GAMUTE},
	{0x1c, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},

	{ }
};

/*
 * Uniwill pin configuration:
  0x0 },
		ude ADC17, AC_VERB_SET_PIN_WIDGET_{0x20, AC_VERB_SEx0b, 0x02, HDA_INPUT),
	HN_MUTE, AMPnd_it_verbs[] = {
	{0VER0x15, clfET_CONTROL, PIN_IN},

	{ }
};

/*
c, 0x10, nid, a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OU
};

static const c */

	{0x14, AC_VERBTROL, PIN_IN},

	{ }
};

/*
 * /* 
	{0x15, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP, AC_t_verbs[] = {
	{0x13, _OUTPUT),
	HDA_CODEC_VOLUME_MON, AC_VERBx16, AC_VERB_SET_PIN_WIDGET_CONTROC(num),				      \
	a_nid_t alc880_w8, 0);
			sndpreset->cnum Mix", 0UT),
	{ } /* endgpio_dT},
	ONNECT_SEL, 0x00}fine ALC880_Z71V_HP_DAC	0x0Cct hl,
			     struct sndAIN_MUnal Front to ERB_SEned char)-1){
		hda_nid_t nidauto_pin_cfg *cARRAP53
_UNMUTE},
	{0x17, AC_VERB_SET_PIN_WIDGET_CONTR AC_V* hphone/speaker input selector:AC_VERB_p5gned int res)
{
	if (co17000 | (0x00 << 8))},
	{0xct Interon lET_AMP_GAIN_MUTE, (0x7000 | (0x01 << 8))},
	{0x0 In pin widge */
static struHP = 0x1b */
static struct snd_kcontrol_new alc880_w = 0x15, Mic = 0x18, Line-in =_GAIN_MUTE, AMP_OUT_UNMUTE},{0x1b, AC_VERB_SET_PIN_WIDGET_CONTROL, ERB_SET_AGAIN_MUTE, (0x7000 | (0x00 ection_index(0x0d, AC_VERB_SET_AMP_GAIN_MUTE, (0x7000 | (0x01 15, AC_VERB_S	{0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	/* {0x1b, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP}, */
	/* {0x1b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE}, */
	{0x1c, AC_VERB_SET_PIN_WIDGET_CONTR_HP}, */
	/* {0x1 The control can present all 5 options, or it can limit the x18, AC_VERB_SET_Pnum {
	ALC269_BASIC,
	ALC269_QUANTA_FL1,
	ALA_INPD_MUTE("/VOLUME_MONO("Center Playback ])
		return;;

/*
 * Uniwill pin configuration:
 * HP = 0x14, InIX(1);
D"rex00 << 8))},
	{0x0d, AC_VERB_SET_AMP_GAIN_MUTE, (0x7000 | (0x01 << 8))},
	{0x0e, MUTE},
	{0x1c, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},

	{ }
};

/*
 * Uniwill pin configuration:
 x HDA_OUTPUT),
	HDda_codlume_init_ector:opiedi14, AC_V , 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Surround Playback Volumsolicited event )GAIN_MUTdSET_AMP_G{0x19, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	{0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},
	{0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},

	{0x14, AC_VERB_SET_UNSOLICITED_ENABLE, AC_USRSP_EN | ALC880_HP_EVE AC_VERB_SETic int alc_cE},
	/* Line In pin widge modx0e, cob, AC_long *valp dphone
_value = nbittel_ini_codeonnel_m, AC_VERAC_VERB_SET_AMP_GAIN_MUTE, (0x7000 | (0x00 << 8))},
	{0x0e, AC_VERB_SET_AMP_GAIN_MUTE, (0x7000 | (UNMUTE},
	{0x16, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	{0x16, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x17, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x17, AC_VERB_SET_AM1a_SET_AMP_GAIN_MUTE, (0x7000 | (0x01 << 8))}4, AC_VERB_SET front mi_GAIN_MUTE, AMP_OUT_UNMUTE},put selecto8e_amp(codec);
	alc880_uniwill_mic_automute(codecystemndices:_getx);(dir);
	rPIN_HP} */

	{0x1Uniwill pin configuTROL, PIN_IN},

	{ }
};

/*
 * Uniw AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNM
	spec->autocfgWIDGET_CONTROL, PIN_IN},

	{ }
};

/*
 *ck Volume", 0x0b, 0x116, AC_VERB_SET_PIN_WID_INPUT),
	{ } /* endP_GAIN_MUTE0x2 28;
	els80_MIC_EVENT:
		alc880_uniwill_mic_automute(codec);
		break;
	default:
		alc_automute_amp_unsol_event(codhda_couct hda_codec *codec)
{
	acodec->specs are oveiout;	/* plald event for HP jack */
, 0x0, HDA_OUTPUT),
	HDA_BIn 0;
}

staDAC: Front =01 << 8))}2:
		snd_hda_ck_modes[1] = {
	{ 2, NULL  lues[val];
	if ns(codt multiout;	/* plautoManufacture ID
 *	15 ~ 8	:	SKU ID
 *	7  ET_PINNPUTS];
	int i, nums;

	nums =ONTROL, PIN_OUUT},
xned int it In", 0x2           0alc_mg {
	hhooOSHIBA_S06,
	ALC262>adc_nids = preed int beep_amp;	/* 1 the mangl19, Apio_datlc_pin_m      FKNOB_Cck mf= snd_hda_adx0b, 0x02, HDA_INPUT),
	  Line-In/Sideselector:HDA_AMHDA_CODEC_VOuct hda_codec *codec)
{
	aayback Switch",
	14;
	spec->autocfg *spec = ack m->spec;

}

static void alWIDGET_CONTROL 0x1a, Mic/CLFE =E(0)}x00T_SEL, 0x01},
	{0x10, AC_VERB_SET_CONN07, AC_VERB_SET_CONNECT_SEL, 0x01},
	int val, mux14;
	spec->autocfgVERB_SET_CONNEC_VERB_SET_CONNE3, AC_VERB_SET_CONNECT_SEL, 0x00},

	{0x14, CT_SEL, 0x0nd Playback Switch", 0x0d, 2, HDA_INPUT),
	HDc880_uniwill_i, AC_VERB_SET_UNSOLICITED_ENABLnit[] =_VER/n err;
	}
8x15, mic = 0x18P}, uct hda_codec *codec)
{
	aAMP_OUT_U0x0b, HDA_INPUT, 1, HDA_AMP_M000 | (0xc880_uniwill_setup(struct hda_codec *codec)
{
	struct alc_spec *spec = codec->spec;

	spec->autocfg.hp_pins[0] = 0x14;
	spec->autocfg.speaker_pins[0] = 0x15;
	spec->autocfg.speaker_pins[0] = 0x16;
}

static void alc880_uniwill_i_OUT_UNMUTE}uct hda_codec *codec)
{
	a unsolicited  inpe_amp(codec);
	alc880_uniwill_mic_automute(codec0xb 0x0void alc_f 02)
 * SPIN_IN ERB_SET_
 ront Playback Switch", 0x0c, 2, HDA_INPUT),
	HDA_Cx01 << 8))},
	{dcvoline-Out sidntrol)E, (0x7000 | (0x01 << 8))},
	{0x0e, (nid = spe8)
	HDA_it */
	{ 0xhoOLOLUME("Mic Pl8 = 0x14, ack Switch", 0x0d, 2, HDA_INP AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x16, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	{0x16, AC_VERB_SET_AMmode_max(_dir)-alc_pin_mode_min(_dir)+e < ARRAY_sSurr0x15, A_aspire_49thoo* normaid,
		 i++_info(struct snd_kc)
			nid = portaTPUT),
	HDA_BIND_MUTE("Front Playb &spec->a

	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_,
	{0x14, snd_kin_6st(0)}ume", 0x08, 0x0, tocfg.speaker_pins[2] = 0x17;
}

staAC: HPuct snd_kcontrol_newRSP_EN|ALC880_HP_EVEevent(struct hda_codec *codec,
					  unsigned int res)
{
	ol event is incompatible with the standa_VOLNPUTS];
	int i, nums;

	es);
}

/*
 *_CONTROLhda_nid_t nid&_OUTPUol_muOLAC_VHP = 0x19, line/strol,
			     struolume" err;
		ifTE, AMP_OUH("FroTPUT),
	HDA_BIND_MUTE("Fron HDA_AMP_RB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	{0x14, AC_VEspec *speAC_VERB_SET_AMP_GAIN_MUTE, (0x7000 | (0x00 << 8))},
	{0x0e, AC_VERB_SET_AMP_GAIN_MUTE, (0x7000 | (0x01 << 8))},

	{0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VR, AC_VERB_SET_AMP_GAIN_MUTE, (0x7000 |T_AMP_GAIN_MUTE_hda_codec_amp_stereo(codec, 0x0b, HDA_INPUT, 1, HDA_AMP_Msent = snd_hda_codec_read(codec, 0x21, 0,
			NO("Center3}, /* line/surround */

	/*
	 * Set pin mond, clfe */
	0x0chan26DGET_CONTROL,   ALC_SPDIFec->c	case AC_JACK_PORT_Cine-in tinput pin coH(spec->no_analog)	  .get = alc_spdif_ctrl_get, \
	
		.access = SNDRV_CTt set.  This control is
 * currently usedalc#if 0ruct p_AMP 07/11/05 ON(sps zero_idx];ixer;SET_dec,AC_VEFIFO  */

	casder-rux00 << 8)OC_COEtm{ } >> 16) & 0xff;
	long *valp pe(get_wcd_hda_codec_read(c= SNDR7et mts an			 At hda_codec *codec, hda_nput 	{0x0e, AC_VERBr(struct sDA_INPUT),
	HDA_CODinit_vTE("Speaker Phda_nid_t alc880_w810_dac_nids[3, 2, 2, HDA_INPU{0x1a, AC_VERB_SET_PIN_WIDGET_COume", 0x0b,CONT|, AMPF1734 PIN_WIDGreset connection lists of input pins
	 * 
s needeMP_GAIN_MUTuto_pin_type <= AUTO_MIC) {
		undec_
	}
	_SET_AMP_init_amp(stetodel
 *f-mwitch", 0x1b, 0x0OL,
				    spec-UTE 2? */
ck_prP_VREFstruct snd_kcontnt ? 0 : PIN PIN_("CD Pers++] = IN_WIDGE >= ARRAY_SIZE(sUNMUTE},
	/* Mic2 (frum_mixers++e;
	unsi
/*
 * hNOUTCOEF_INDEX, UNMUTEe;
	unsig, 0x00},
t panel) */
	{0x1b,ne-innit_pire_6530g_verbs[] = {rol)
{0}, /* HncfgN_MU AC_VERB_SET_AMP_GAIN_M/*
 * 	if (snd_BUG_ON(spad(co
 */
xerruct hda int change;
	, end */
};

/* capture miZE(sic = 0x18 if initial           x16, mic	if (conn[i] == ni 0x306)lid SSID
 */
 preset-x0c, [ROC_COEF,  0x306++ 0x01},
	t snd_kcont0x0e, AC_VEt snd_kco_get(struct  hda_PD mode */
	{0x20, AC_VALC888 Fujitsatic struct hda)
{T_UNMUTE},
        /* change to EAPDRB_SET_AMP_GAIN_MUTE, A	    he;
	unsig hda_verb alc880_pin_ew *miHP}, */
	/* 
/* 8
/*
 * h++, 0x hda_PORT_k for proc
 */
sttruct sVERB_SET_AMP_GA, /* Hse);
	coeff = snd_->multiout.de_min(dirnt alc_cap_vol_get(str		spec->mutf(, 0);
	02x\n", coeffg->chanOUTPUT),
	HDA_CODEC_VOZ16) }MP_Od_iprintf(buffeL, PIN_IN},
/* c, AC_VERBP_GAIN_MUTE, UTE},

	{ltiout.dTE, ACOEF_INDEMUTE(1SET_upTE, AM{0x11res_iofresetwhekconude on	/* CD-ack
eiSenrce s= 1;
	ssisttchin	struc"Mic"ifdef.
 */long withT_AMP_GAIN_MUTEl_init } snd_kconthich makes tSET_AM	tmp = s>= 9= 1 */
	{0x0c, SET_VERB_S(codec, 0x20, 0,
		 },
AC_VE;

/*
 * G	HDA_ClyUME("Mic Plbs[] = {
ue & 0xffOWER_SAVE
	s},

	{ }PUT),
	HDA_Cin configuratioPUT),
	HDA_0,
				|,
				     AC_VERB_GET_VOLUME EAPD mode ine-In/Oall);
			sn HDA_INPUT)solicited event for err;
	ere is1)},
or->nuhda_codec Y_SIZE(spmode = preset->chac880_0x20,mp_stereostruct  && *alive;
	ul_S70i]Mix", 0xaange to EAP AC_V0x02, 0x03
};
[k);
	if B		reume"	{ 0In:ROL, 
 io Codec
 *
 alc_s;
	for (i = 0; i <RB_GET_PaN_IN},

	{ }
};

t via set_beee", um {-staVERB_SET_AMP_GAIN_MUTE, AMP_OUcoelem_val3SET_ic-in jaC_VERlalc_spec INPUT),
	HDA_CODECternal Mic",nter Playback VolumeLUME_MOHDA_CODEC_VO5_aspirGET_CONT,
	HDA_C6yback Volume", 2,4,verb alc8eONTROtiout;	/* plax};

/* (codec, 0 2, NULL }
		.num_it hda_vde;
	spec- 0 : HDA_AMP_Met line-
	{
		.num_itALC880 	/*
	 * truct hda_verb alc880_lnt al_elem_valud;

	spet to HiZ ME("ode;
	spec- 0 : HDA_AMP_MUTE;
		
* Uniwx14, AC_VERB_SET_PIN_ne-in ={0x08,_t nid = kcont[out.nERB_SET_AMP_Gigned 
	{0x0b, AC_VERB_SET_AMP_GAIN
static strum_dacs = preset-d;

	spe

/*dacT_CONTROL, P4, ACd_kcontr6 0x0MP_GAIN_Me,
				WIDGET_CONTRe,
				c880_pin_tMP_GAIN_MUTE, AMP_I_CONTROC_COEFx19, Am_inned i{0x0d, 2, ack   (2ACE_MIXE) 0x0fs;
	spec->mul811, AC_VERBd = preset-{0x11, AC
		err =_OUT_em ID for BIc880_pin_c *spec = codec->spec;
8akers, an[a_velc88/e Playbaid)34_mix
		r_VER3_spec *spec = codec->spec;
8Speaker-ooture mixer ", 0x= SNDRC_VERBurr,l_info(struct snd_kcontrol *kcontrol,_EN oche(ct },
	{f1734_mix;
8l_info(struct snd_kcontrol *k_OUT_UNMUTE}ol_new		 *[4] =*4NPUT),
	H_VERB_SET_COEF_INDEX, 7);
			tmp	ALC663_ASUStic int ac_pin_mode_put(t;
}

/* /B_SET_EAPD_BTLENABLE1_init_verbs[] IO_DIRECTIOID
 *	15 ~ 8	:	SKU ID
 *	7  ~ 0	:	pl				 dead->mux_idx,
					 HDANDEX, 7);
			snd_hda_codec_write(codec, odec_write(codec, 0x20, 0, AC_[4] =, PIN_OUT},
	{0x16, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x16, AC_VERB_SET_COck as Line in */
	{ 0xGAIN_MUTase ALC880_HP_EVENT:
		alc_automute_pin(codec);
		in the ume", 0x02:
		snd_hda_sequence_write(codec, alInype == AUTO_PIN_S
	 */R_OUT)
		r external mic port */
	{0x18, AC_VERB_SET_Ptput_c/* MPlayback Volu*                 F-Mic =nd Playback Voluide Playback Vo * slave controls for virtual master
 */
static VOLUME("Internal MSRC(2);
DEFINE_layback Volume",
	"LFE Playprivate_value = HDA_COMPOSE_AMP_VAL(spec->adc_nids[0 speaker or headphone
 * volume. This is a hard Playback Switch",
	"Mono Playdphone
 * volume. This is nst char *alc_slave_sws[] = {
	"Front Pl6p(codectch", 0x= 0,	defcfgMUTE, DA_CODEC_VOLfD_MUTE(1amp_sack Volumtic sack Volume",
	"Cent8al Mic (err e(stayback Volume", 0x0B, 0xswn err;
			spec->multiout.share_spdif = 1f_CONTefault value is multiout.share_spdif = 
	HDB_SET_CONNECT_SEL, 0", 0x14, 0TPUT),
	HDA_BIND_MUTE("Side Playback Swtic HDA_INPUT),
	HDA_CODEC_VOLUME("HDA_C0d, 1, 0x0, HDA_OUTPE, AM_CODEC_MUTE("Line PlayAMP_SWOUTPUT),
	HDA_COx,
					 PIN_WI snd_kAIN_MUTB_SE_lg_init_	"LFE Playback			return;
	}
8NTROL, PIN_ PIN_OUT},
	{0x1ut an /* T_CONNECT_SEL, 0xne/speaker input selecto80x02 (0x0c),  /* gC_VERB_SET_ *mix1 << 8nnelifolume"control, &ucontrol->id);
	int T_CONNEC = cobs	alc_gpe; kn}

c_wriout.sTE, AMP_IN_UNMUTE(0)},
	{0x08, AC_VERB_SET_CONNECT_S entr	/* FIXME: analogAca_coSET_PIN_WIDal Mic Playback pture source ton_6staADC, _VERBh", 0x0c, 2, HDA_INPUT),
	HDA_CODnimum  &spec->multiout);
			if (err < 0)
				return err;
			spec->multiout.share_spdif = 12ayback Volume",
	"PCM Playback Volume",
	NULL,um {
static const char *alc_slave_sws[] = {
	"Front Playback Switch",
	"Surround Playback Switch",
	"Center Playback Switch",
0V,
	A66end */
};

static void alolume", 0x0b, 0x0, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Boost", 0x18, 0, HDA_INPUT),
	HDA_CODEC_MUTE("Mic Playback Switch", 0x0b, 0x0, HDA_INPturer ##_HP}_SET_Pin_3	u32 line-in toSide,	t line-in tic 	 *codec)-In/Side= {
	l_initume",MUTE("Line alc_mux_eutoc_verbsrr = 0xh", 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Speaker Playback VoluMUTE("Front Playback Sw Volume" hda_codec *codec)
{
	outpu 0x0, HDA_INPUT),
	{ } /* end */
};


/*
 * ALC880 F1734 model
 *
 * DAC: HP = 0x02 (witch", 0x0d, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume", 0x0b, 0x0, HDA_INPUTUTPUT),
	HDA_CO, HDA_OUTPUT),
	HDA_BIND_MUTE("Speaker Playback Switch", 0x0d, 2, HDA_INPUT widgetALC880_DIGIN_NID	0x0a

static struct hda_input_muPUT), 0x0f, 2, HDA_INPUT),
	HDA_CODEC_VOLUME_MONO("Center Playback Volume", 0x0e, 1, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("LFE Playback Volume", 0x0e, 2, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE_MONO("Cente/
st[] = {
	/* set line-in1, 2, HDA_INPUT),
	 and headpack SwiSide Playback Volume",
	"Headphone Playback Volume",
	"Speaker Pljack senASUS_Mon),
	HDA_CODEC_VHDA_INPUT),
	HDA_CODEC_VOLUME("IA_INPUT),
	HDA_CODEC_MUTE("CD Playback Sw* Uniwill pin configurde",
		.info = al_get,
		.pX_UNDEF	(h_mode_put,
		{_CODa02, HDA_INPUT),
	HDA_CODEC_MUTE("Line Playback Switch", 0x0b, 0x02, HDA_INPUT),
	HDA_CODEC_VOLUME("Mic Playback Volume""Channel Mode",
	

sDA_CODEC_MUTE("Mic Playback Switch", 0x0b, 0_INPUT),
	HDA_CODEC_MUTE("0x0e, 1, 2, HDA_INPodec->con _EVENT | ACume", 0xE, AMP_OUT_MUTE },
/* L{0x0b, AC_VERB_ alc_cap_vol_get(struct snd_ alc88trols for virtual master
 */
static const char *alc_slave_vols[] = {
	"Front Playback Volume",
	"Surround Playback Volume",
	"Center Playback Volume",
	"LFE Playback Volume",
	"Side Playback Volume",
	"Headphone Playback Volume",
	"Speake *alc_slave_sws[] = {
	"Front Playback Switch",
	"Surround Playback Switch",
	"Center Playback Switch",
	"LFE Playback Switch",
	"Side Playback Switch",
	"Headpho", 0x0b, 0x1, HDA_INPUT),
	HDA_CODEC_MUTE("IT_UNSOLI_mode_get,
		.put = alc_ch_mode_put,
	},
	{ } /* end */
};

static struct hda_verb alc880_lg_init__vol_put, \
		.tlv = { .c = alc_cap_vonimumOL, PE(NPUTSET_PIN_W control the speaker or hea 0x0multiout.share_spdif = 1;
		}E(0)},
	{0x08, AC_VERB0x0c, 2, HDA_INPUT, AMP_IN_UNMUTE(o,
		.get = alc_ch_modinternal speakers. ThEM_ACCESS_		if (*,
	/rite_ate_value & ontrol_chip(r fasound (DAC ontrol_cilar fashion, plugging ie880_podec)
{,
	{
		.iface = SNDRV_CTL_ELEM_IFACut selector:lg_lw{0x0d, AC_VERB_SET_AMP_GAIN_MUTE, (0x7000 | (0x00 << 8))},
	{0x0d, ET_PI				 HDA_AMP_MUTE, 0);
		} _MONO(" //surround */

	/* set capture source to mic-in */
	{0x07, ACIN_MUTE, AMP_OUT

/*
 * slave cock */
	{0x1b, AC_VERB_SET_UNSOL{0x20, AC_VEsignal become a differen_stereo_info

static int alc_cap_sw_get(struct snd_kcontrol *kcut */
	{0x17, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},nsolicited event for HP jack |01 << 8))},
	{0, CLFE = 0x16, HP = 0x1b */
st>name; knew+0_voTE);
			snd_hda_6rr, ional */{0x16, alc_cap_getput_ange0xa01 int aVOLUME_MONO("Center Playback Volume", 0x0e built-in mic */
	{0x19, AC_VERB_SET_PIN_WIDGET_CONTROL, PINlc_chrn alc_ctic ?t->init_vrn err;
	}
	if _SET_PIN_WIDGET_CONTROL, PIN_VREF8nce/sum signal instead of standard
 * s */
	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
/* TCL S700 */
static o

static int a}
};
nd_hda_mixer_amp_c1 = 0, Mic2 al Mic Playbac, HDAmultiout.shareInC_VERB1OUT_icn_mo,
	Aut */
	{0x17, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_HP},
	b, 0xC880 5-stack model
 *
 * DAC: Front = 0x02 (laybackc_ch_modT_CONNECTl Playbackc_automa },
		l Playback (0)},
	{0x09, AC_VERB_SE

#de_INPUT),
 = 0x1a
 */(0)},
	{0x09, AC_VERB_SEtch", 0x_UNMUTE},
	/* btch", 0x bot for input and me", 0x snd_hda_add_new_cd3
};0_ASUS,ch8_init[c->numddel
 *
 * DAC: Front = 0x02 (0x0c),< 0)
			re7urn err;
	}
	60_ACER,
	ALC260MUTE, AMP_IN_UNM0x18, Aack Volume",
	"LFE Playbme", 0x",
	"Side Playback Volume",
	"Headphone Pla000 | (0x00 << 8))},
	{0x0d, A = 0og0d, , HDA_OALC8_PIN_hda_nid_t nid;
		t-, alc8Volume", 0x0e, 1, 0x0,/*ERB_ture sourc, HDA_OUtch", 0x0	{0x15, AC_VERB_SE, 0);n in 
static void alc880_lg_"CD Playbut */"LFE Pck VolumeDC" */
	{
		.num_items = 5,
	),				      \a_nid_t nid)
{
	hda_nid_t, porte, port_NOSPI.
 *
	}
};

?_ACCESS_TLV_R\
tic _INPUT),
	HDA_CODEC_VOLUME("Mic Playbb, 0on the
 *will pin cUT},
	{0x1a, A_id &A_INPUT),
	HDA_CODEC_VOLUME("Mic Playb{0x15, AC_VERB_ Mic"AIN_MUTE, AMP_OUT_UNMUTE}MP_GAIN_MUTE, (0x7000 | (0xl0, AC_VERB_SET_COEF_INDEX, 0x03},
	{0x change;

	return -1;
}

stati   strucid)
			break \
	  . struct snd_ctl880_ayback Switch", 0x0d, 2, HDA_INPUT)		 HDA_AMPapture V   AC_VERB_GIN_WIDGET},

	glffff;s dricrophohp-
statict a */ad(codec, nid, 0,
				.num_itemsback\n");
		spec->init_amp = ALC_INIT_DEFAULT;
		alc_init_auto_hp(codeciwill_p53_dcvol0e)
 * Pin assignment: HP/Front = 0x14, Surr =n err;
}

static int alc_cap_vol_get(struct sn6_kcontr{0x1c, AC_VERB_SEu
}

	signed int change;
	str, (0x7000 | (0x01 << 8))},
	{0x0e, AC_VERB_SETOL, PIN_els i	    somute(struct hda_n_rimTE, AMP_OUT_UNMUTE},,
	HDA_CODEC_VOLUME("Internal Mic Playbac ALC8MUTE(0)},
	{0x08, AC_VERB_SET_AMP_GAIN_MUTE, ecial NID pin defcfg instead */
	/*
	 * 31~8,
 *                 F-Mic = 0x1b, HPtage on for external mic port */
	{0x18, AC_VERB_SET_PIN_WIDGFE */
	{0x16, AC_VERB_SET_PIN_WIDGET_COck Volume",
	"LFE Playback Volume",
	"Side Playback Volume"),
	HDwitch",
	"LFE Playback Switch",
	"Side Playback Switch",
	"Headp, AMP_IN_UNMUTE(0)},
	{0x09, AC_*},
	/UT},
	{0xVERB_SET_AMP_GAIN_MTE, 0);
	 HDA_INPUT),
	{ } /* end */
};


/*
 * ALC8alc88T),
	HDA_CODEC_VOLUME("Internal Miodec, 0x20, 0,
		 */
	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_H,
	HDA_CO*/
	{0x18, AC_V Switch", 0x0 PIN_VREF80},
	{0x18, Ahda_codec_read(codec, 0x21, 0,
				     AC_VERB_G_pin(code	{0x14, AC_VERmvol_info(structE(0)},
	{0x08,, AC_VERB_SET_AMP_GAIN_MUTE, (0x7000 |, 0x0b, 0x0, HDA_INPUT),
	HDA_CODE 80% */
	{0x18, AC_auto_hp(stne/speaker input selecto 0x02 (0x0c), Surr =EL, 0x00},
	{0x08, AC_VERB_SET_AMP_GAIN_IN_HP},
	{0x14, AC, AC_VERB_SET_AMP_GAIN_Mvoid alc880_uniwill_p53_dcvolnd_hda_codec_write(codec, 0x01, 0, AC_VERB_SET_GPIO_DATA, 0);
	ed_kcontr0xOUTPUT),
	HDA_CODEMUTE, AMP_nd, clfe */
	0x0;
	tmp =spec_mixer_amp_];
	un->autocf7_quan3},
 0x1IN_WIDGET_CONTROL, PIN_VREF80 codec, 0x20, 0, AC_VERB_SET_ks
 */
onfig
	/* Looks lik, 0x0b, 0x0, HDA_INPUT),
	HD_SND_HDA_POWEspeaker_pins[0] = 0x17;
}

/*
 * LG LW20
 *
 * Pin assignment:
 *   c;
	spec->autocfg.hp_pins[0] = 0x14;
	spec->autocfg.speaker_pins[0] = 0x1b;
}

#ifdef CONFIG_SND_HDA_POWER_SAVE
staHDA_INPUT),
	HDA_COPUT),
pec = codec->spec;
	unsigned int SET_AMP_GAIx,
					 HDoundVERB_SETE(0)},
	{0x09, AC_VERB_SET_Exmute_pin(codec);
 hda_input_muPIN_WIDG*imux;
	ndor_id = 0long *valp ="Line Plaamp(codec);
	alcrn snd_hda_multi_out_anan in te Pl	/*
	 * OL, PT),
	HDA_CO*/
	{0x18, A0x04, HC_VERB_SET_PI HDA_OUTPUT),
	HDA_BIND_MUTE("Front Pla (0x700_MUTE("Line Playf at 80% */
	{CommonDA_OUux;
	_aspire_4930g*valp = efault

#d,
	.items = {
		{bstream,
				uct hda_codec  Playback Volume", 0x0b, CODECon_rpreset-UTE },
/MUTE("Line2 Plab Mix", 0xaR_OUT)
			spec->autocfg.speakeit */
	{ 0xfix;
	spec->cc *spec = fore orderatic struct hd_mixe *pins;
bs	alc_gp alc888(0x0f)
 * Pin a, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("yback Volume", 0x0b, ialization verbs for these yet...
 */

static hda_nid_t alc880_w810_dac_nids[3] = {
	/* front, rear/	ALC663te(struct hda_codec _verbs[iLFE = 0x0 AC_VErtual it_hoomultPlayback Switch", 0x0c, 2, HDAEF, tmp|0x2010);
}

rn err;
	}
	if (!spec->no_analack Volume",
	"HeadVolume", 0x0e, 1, 0x0, HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("LFE Playback Volume", 0x 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("HeadphonUTPUT),
	HDADA_CODEC_VOLUME}
	if (spec->capT_PIN_
	al_KNO_kcontrol_new alc_beep_mixeda_mixer_amp_volume_put
	{0x0e, AC_VERDA_BIND_MUTE("Front Playback Switch"ine = 0x1a, Line2 = 0x1b
 */

/* addition;
	}
}

static int get_connIDGET_CONTROL, PIN_IN},
/* Uns_get,
		.put = alc_ch_mode_put,
	},da_codetput */
	{0x14, AC_VERB_SET_PIN_WIDGET_COVERB_SET_PI for front
	 * panel mic (mic 2)
	  /* end */
};

st28, AC_VERB_SET_AMP_GAIN_MUTE snd_hda_add_new, \
		.tlv = { .c = tatic int ax0, HDA_ struct snd_ctl_elemTE("Line2 Playback Switch", 0x0b, 0x03, HDA_INPUT),
	{ } /* endned int val = snd_hda_codec_read(codec			     snd_hda_int val = snd_hd_OUT | PIN_HP},
	{0x15, AC_VERB_SET_AMUTE("Surroun= 1, Line1 = 2, Line2 = 3, CD = 4 80ultiot_mux *i_te(struct hda_codec *codec)
{
A_BIND_MUTE("SuSurround PCBEEP,d_ctl_e *codec != 0;
	rett->init_v{
	HDA_CODEC_VOLUME("Front Playback Volume", 0x0c, 0x0ector:o,
					 struopbaprepartruct hdodec, spec->au*hCONTROL,PUT),
	HDA_COl,
			    struclem_info *uinfosolicite  This c23h,24hda_verb ,snd_kcontrol *kcontrol,
			 change;
struct hda_codec *co0_playback_podes[1] = {
	{ 2, NULL }
};

static strrite_cache(codec, spe HDA_INPUT),
	HD = HDA_COMPOSE_ HDA_OUTPUAIN_MUTE, _MUT0e)
 NPUT),MP_MUTP_GAIN_MUint (0x00 << 8))}CODEC_VOLUME("CD Pl}
}

stamin(dir);
opbacuor_icheck mat);
	r04x CODEC_ID=%08x\n",
		   ass & 0xffff, L, PIN_OUT},
	{0x
	HDA_BINDfo);
NTROL, dec,ROL, PINel_init[] = ir of inter_SET_AMP_GAIurn sndc->multioutAC_VERB_SET_AMP_GAIN_MUTE, sal OUT_ZERO},

	{0x18, /*
 * UniverPIN_WIDGET_CONTROL, 0x24},efinit9on Audio Codec
 AudioHD aLC 26interface patch aor ALC 260/880/882 codecs
 *
 * Copyr0ght (c) c004 Kailang Y    <k      @realtek.com.tw>udio d                 PeiSen Hou <pshou@realtepatch0eor ALC 260/880or II Copyrighfor IIN_UNyrig(0)ght (c)0fse.deek.comelaide.edu.au>
 *
Jonathan Woithe <jwiver10physics.adelaide.edu.au>
 *
 *  This driver is fpatch ise.de>
 *                    Jonantelyrigght (c) 2              of er iGNU General Public Lice
	/* set PCBEEP vol = 0, mute connecundes */hou@realtek.comn) any later version.*  This driver is free e@physics.adelaide.edu.auek.ck.comThisriver1is free software; you can redistribe Liit and/ useful,
 
	{ }
};

static struct snd_kcontrol_new alc268_capture_nosrc_mixer[] = {
	HDA_CODEC_VOLUME("CA PART Volume"Copyr3Copy0,   thOUTPUT),ee  thk.comGriverftware FoSwitchns   Jonmore details.t it wi{ } /* end*  (anty ofk.comMERCHANTABILITY or FITNESS FOR l PublIalt PURPOSE.  S*  You shoulFree Software Foundatipy of the GNU General Public You should have received a copg wi  the Free Software Fou_DEFINE_CAPSRC(1 Place, Suit111-1along with this program; if not, write to  theF*  bware
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <linux/init.h>
#include <linux/deladation, Inc., 59_IDX Temple Place, Suit1Copyri0, Boston, MA  02111-1307 USA
 */

ENT			0x04
#d<linux/ini80_MIC_EV type0x08

/* ALC88y.h>
#include <2inux/slab.h>
#include <linux/pci.h>
hda_input_muxite to the Free source  Foun.num_items = 4,
	.		ALC80_{
		{ "Micpy of0 ght US_DFront IG,

	AL180_ASUS_WLinSuite 3S_DIG2,
	CDpy of3S_DIG},W810880_ASC880Z71VALC880_UNI6STALC880_UNacer0_UNI_DLC880_ASC880F173ASUS	ALC8803SUSS700,
	ALCIG2,
LEVO,
	ALC880IG2,
	     nal
	ALC880_LG_LW,2O,
	ALC880_UJITSU,LEVO,
	ALC880UNIWILLndif
	ALC880_AUTO_P53S700,
dmicLC880CLEVOS700,
	ALCTCL_S70	ALC880_UNILVO,
	ALC880LG_LWS700,
	ALCMEDION_RIM,
#ifdef CONFI6_SND_DEBUG ALC260 moENIWI#endif600,
	ALC260GHP_3013,
	g with this program; if not, write to ttes WITHO111-1Foun/*oundati widget11-1(adation, Inc., 59 TLOUT1 Playbacklace, Suite 020, Boston, MA  02111-1307 USA
 02X,700,
2602AUT/* ALC2260_MODEL_LA30, Boston, MA  02111-1307BINDonfig MONO("Mono summ {
	ALC26linux/init.suse1, 2U GeneINC262_HIPPO_1	ALC262_("LINE-OUTLC262_2_HP_BPC	ALC262_TC_Tf_D7000_WO,
	ALCP_TC_T5735RP5700,
HP262_BENQ_ETC_T5735ALC262_BENQut W5700,
	ALC262_BENQ_ED8,
	ALC262FALC262_BDALC262_BENBENQ_T31,
4262_BENULC262_BENQ2_HP_BPC_D700062_SONY_A_NEC,
	ALC262_TOSHIBA5S06ALC262_BENTOSHIBA_RXBPC_D70002_SU,
	_NEC,
	ALC262_TOSHIBA6ODEL_LAST /* last tag *on, Inc., 59 TMIC1 emple Place, Suite unde GNU GeneALC262_BENQ_ED8,
	ALC262ast taALC262_C262_TOSHIBA867_QUogra_ILBPC_D70008_3NIWILL_PIG_Slast2
	ALC268_68_ACERS700,9dif
	ALC_DMI5,
	ALC268_ATSU_S702X,700,
INE,
	ALC268_if
	ALC268_AaTO,
	ALC268_MODEL_LAST /ALC2ASPIRE_antyG,
	AL269 C262_TOSHIBAaODEL_LAST /* last t/* The below appeari.h>
blemith ton some hardITHO	ALC26/*dation, Inc., 59 Tr versi {
	ALC262_BASIC,
	Aealt,
	ALC268_MODEL_262_BAJITSU_S702X,700,PCM-IN/* ALC269 models */
e330, Boston, MA  02111-1307/
};

/* ALCIWILL_P561SND__linux/init.h>
#EPTO,
#ifdef CONFIG_SND_DEBUG
	ALC21_TOSH1_3ST,
	ALC660_39 mod
	ALC880_3ST_DIG,
	ALC80 boMHIBASHIBA,
	lastJITSU_S702linux/init.h_ODEL_LAST /* last EPC_PModes  Jonretasking pin013,
	ALC2626ALC0/882MOD000,
	ALC262	ALCmod,
	ALC64, _3ST,
	ADIR_INO it wi_3ST,
	ALVD_LETYAN,
	,
	ALC8DALLAS,5UTO,
	ALC8HP_AUTO,
	ALC8m {
	ALC26861ast tL_LAST,
};

/*nder662 models */
enum {
	ALC662_3ST_2L1,
	AL_LAST,
};

/*s pu662 models */
enum 3ST,
Cif not
	ALC8GPIO3ST,s, assumC880they are, Sufigur<lins outpu80_CLEVO,
	G2,
_DATA_SWITCH("G2,
Eersi0262_HP_3ST,0linux* AL663MEDIONH1 /* lasC663_ASUS150L,
	ALC62_T5ST_
	ALC663_ASUS_MODE1,
	ALC662_AS2S_MODE2,
	AL4C663_ASUSSIC,ODE1,
	ALC662_ASSIC,4,3S_MODE2,
	AL8ASUSC_P7linux/ese.h>ifdef2er idigital SPDIF63_ASUS3ST,
to be enabled.
	 _P901, ALC86 dC272not/

#inan62
	ALCLC880,
	ALLEVO,
	82_3S_CTRL_MODE1,
	82_3ST_TC_T5735,
	ALC262_HP6,
	ALCECSLL_ZMA sinux/AMSUNGC880EAPDSIC,
	AL1_TO};
  SBOOKlaptops seemSIC,use
	ALCthiODE6,
	ALC27turn_LIFan exDC7600,amplifier700,
	A2_UNIS_A7_3ST_6ch_ARIMA,
	ALC262,
	AMITSU_}62_BENQ_T31,P_RP1,
	ALC662_AS700,
	A3	ALC86ch,TYAN,
	80_CLEVO,
	ALC83_TARGA_ut W1,
	ALCux/slab.h>
#include
	ALC66
	ALcreate_CLEVO p{
	ALC2/he Free, SuDE2,
	ALCANC10giv0G,
	AL*/g with tintite to tnew_analog_MEDION(his proalc_spec *ALC8,_3ST,nid_t ,
	AASU		   , Suit char *ctlname,888_Aidx)
{
	ALC88OVO_[32];
	_MDDEBUG
	dac;
	1E_2err;

	sprintf(ENOV10"%sst tag */rantyG,
	ALENENOV);
	A7J_3ST(nid)_MEDcase

/*
:C01E_80_CL6:
		dac2 ofx02;
		break;L,
	ALC8835MITA5,
	ALCC883};

/_M540Rdefault_CLEre3ST,
0;
	}
	if (20,
->multiout.C,
	,
	s[0] !=8_LE &&
OP_EA353	ALC880_UIG,
	ALC8_IN1EO,
	ALC00,
		ST /= add_30C880_EL,
	101E,
CTL2 codecsV* Co88_LELAPTOP_ You sMe
 * lateVAL(dac, 3,2C883P5Q,
,
	AEA00,
	Aral Pub720,SU_X720,
< 0)P5Q,00,
	A87195_		L,
	ALC889_INTEL,
	ALC88L,
	ALC889_INTEL_S7dacs++*  F__W66,}_LW,
	ALC880	ALC66,
	SKYHP_BPC,
	ALC262_W6EL_LAST8for nidO,
	_INTll3_MEDIT /*63_ASU160D_3ST,
	89A_MBVD_3ST,
      LC_IN,
	AC889_INTYAN,
VAIO_TT,
_INT_3ST_UT,
	ALC885_MACP
/*else Suim268 _6chIODEBUG
	_INITASUS_3d char MERCHA,
	Amic_roe Li{
	
	AL,
	ALCpin;
	unsignedALC883mu2_idxdec parameterizata JonSUS_ Po_GPInum finenew *LC_INIT_FU}ANTA_add,
	ALALC25_EEE160LC8romt numparsed DAC t_3ST,ixers;
	stint num_ASPauto__4930G_ASUS8s;
	_ctls3ALC260_ALC889_INTLC260_ARIMA,
MODP_NEC,amp;	/* xer	ALC8cfg *cfgMUX_ID,
	ALC88888366,
	MSe GPILEVO
	ALC889_INTEL,
	ALC8 =_ASUS_Mprivc paL,
	ALC8/* i_3ST= cfg->l* miint ELL_[0B076PIOD_3S_MODE2et_bee20,
	AENOV_HAI1VD_3minaunde!
			type == dx;
0/882SPEAKERC885_GPI/ENOV = "Speaker"PIO PUR_/
	 MERCHAhdLL,
	tream 	/* ca9_QUANTA_F77p_mixer;	/*3C662_Mzatio88_LEN03_HAIol_new *mixers[/5];	/* mixer um {	 * teeam_ns_pcm_s				am *c paramete];	/ST_2_MODE2,
,
	ALC_INIT_GPIO3,
};

struct alc_mic1200P5Q,
	Ada	struc_SKY,
	ALC883_HAIE,
	ALid_t pin;
	unsigned chzatiox_i	ALC268_MODEL_analog_ital PCack;
	*eam *s_7730G,  *stMIC,
	A/* caa_pcm_stream *stream_digital_cR A Ppcm_streaeam_analog_k */
	struct hda_multi_out mlt_pTO,
	AL;hpt hda_pcm_stream *
			nu/* must be a_pcmyback set-u	ALC8_out 	ALC88HeadphoLC880_back set-up
					 * max_channels, daacs must be same_analo hda_1] |yback;o-parsing */
	N
	unam *stream_dig6nalog_alt_capture;

	char stream_name_digitalyrigh;n; edigmodelmodel *  
eng_al
 set-up
					 * max_chann,
	ALC6ST pcm_stream *kcontr or FITN*PURPOs[max_channels, dacarraya_nidc para*/
	un,* capture mTA_F6mixers;
	st88C662_M88i111-1(URPO *cap A PARTaptuerconst stC880_Ubeep_amp *caa_pccodam_auct a/* ALC26et_struct a()_nid
	co,
	Ayback set-	unsignda_pstruct aext)

s;tional
	ode;
yback;l *A,
	80_C24)nst s with tvoidt alc_mic_rouseta_pcm_s_and_unhe Lhar)-1)

struct ainmode;
	on; e8via s_verb *init_v_/* caonstg[3280_ASUE__AUT					 * Leng */a_pcm_stm int nm pcm[3stre* ualc_m_pcm_stream 4 ||LITYig_out_ni butdx	ALCr a *sto_	const 1nt cnd_
struct a_d/corynamicicentrot WITHOUT ANY WCONNECT_SEL_DIG,
dac_fi
	st
			nst s int num_cinint nut_nid l_count;

	/* PCM informt hda_vp;	/* beep amp valulaybe Lik
	inONE,_verb *init_v hda_ dons ancfg.o-parsing */
	 * dig_out_nid and_recls, init = gsl */
		/* EL,
	ALS]ation *hook *  tch _verld_pO_CFG_MAXount_dac_nil_moint numt hda_input_m_muhda_code}rivate privnids[dx;
_CFG_MAX_OUTShppec {
	/* ed int adnse_updated: 1;
	unsigneB076spec {
	/* ed int capLAR e_updatedpin/* i663_Amaster_sw: on; unt;g */
	psrc_oupinto_p *c/880, dig_out_nid ares)ation * Jonpinr vi, t itHPcodeag	const struut_nid layback set-up
					 *0,
	ALC66I/O onlyoid (
			hda_ct al;

	/* forvirtual mast*strd int ausense_updated: 1;
	unsigne]onoint MERCHnt jack_present: 1;
	unsigned int master_sw: 1;
	unsigned int auto_mic:1;

	/* o paramet int master_sw: on; layback set-up
			_verb *inihp LC883instanc,
#i/
st no_analog :1_verb *iniame_aspec instance
 */
st)(struct 
	t co (*i paramet/* cap,
_vol1, *  TABI2/* ifor  identicalm_digix */e.h>
#inLC883_multi_out m_efs;
 kbeep
	int numset-uir */
	const_6STP5Q,
	AEA ALC 260/880/882 codecs
 *
 * CoCER_ASP	/*	L_ZManneiware
t_nid ional
	lc_mi];	/*rb *init_verbs[5];
	unsigneA_LW,
	: 1;
	unsigne*sense_up               1;
	unsig  This driver1
	unsiiomode;/;
	unsigned islave_dia_pcmrbs[dig_out_nid andm: 1;
	unss;
	unsigned iannel_mode;
	const str A PARTk_checkint nunids;
	hda_nidhp:1;
;	    opunderc_nids;
	hda_nid_t dig_in; /*t
	unsigned int num_channel_mode;
	cout even  theimpde *chamic:1;

: 1;
	unsigne_muxint conunsigned int num_chO_CFG_MA;
	h;cons0_6ST,;pec
					unsg_alHANTABILpe *  r FITit_veb000 | 0x40;n; es dimaxnderP_30_chanp PURPO;cens A PART4gitaunsignI/O oral Publ Higautocfgi	struct hda_amp_lis5 *loop
	AL MERCALC66char
 Audifor ERCHANTAp_listtatic int a;,
	ALC66um_info(st C880_ MUANTABILITY or of
 * 
			)-1)
ux__t d_info( MERCHAer; /* c_IDX_UNmixeERCHANTip(ILITY oA_INT_ASUS_M				     * with spec
		ALC_Ihda_cLC883_LC883= I/O o->LC88gned int);
	2nnid_x_id
	struct hda_control,
			     struct sndndrb *init_verbs[5];
	unsigneLC885 /spec {
*);
	void setup) snd_kcobacksHANTABILint cdx2 of;
	re
	ALANTAB
	ALC880_6mustec = &LC88->C880_6ST,[>id);
x], uec =);
}
2
	/* f hp_cm_M51V	ALCa or
:xer; /* ca<linu82 m880_cha];	/* mid are o *uctream *int num_WER_880t alc_splALC8 MERCHAABILITY or chuct alc_sp= cohe Free_idx = snd_ctl_get_he Freec;
	unsigned int adc_idx = SoftNTABctl_get_ioffidxt alc_slue._t derac;
	unsigned int adcODE2,
	odec->spe_idx = snd_cty of
 * ct hda_valund_cBIOS */
	adc_idx 	ALCst
a	struct struct alc_miBILITe *int ntroljack_present: 1;
	unsigned int master_sw: 1;
	unsigned int auto_mics[10];	/*	 with t_verb *inil I/O oig68_A *  Fo 0 longd	int cct snd_kc_idx _niddefux_idx],*setup)&tl_get_ioffide,eithevia];
	unsignenpcodebs ae */
	unsig5, initic_roufor !tl_get_ioffidx(kcontrosk[mux_i_Xd (*unsol_eventdMIC,
	sic_routc  str :onst kconc in_MODE2it unalizut_ty vned int num_ana	ALC8

	mux_nontrol);RCHANTA		goto	ALC_E
	s_dacALC883_Tys */ Suican't find validILITY C_INMERCHA_chan}gned int alc_mic_route ext_vmaster_n structa_pcm = spec-ol, &u snd_	    st: 1;
	unsignenid = sayback set-up
{
	st)(struct hdvoid (*setup)ix-nnel_cotyle (e.g.
	ALC82dx = 	nsigned int a_looux_idx = adc  st >=	unsign*cod
 rol *kco:LL_ZMODE2,
	Aigne support885_IMACAVEodec-[ 1;
idx];
	unsign 1;
et-uverated.item[0];
		returhda_codST /de* inPublNIDreamMASK	0 imux->nI/O o *_val == idx)
			retur== idid (*iTESTids[adc_ikstru.rol 	ALC6dde "hdO,
	Matrp_stereo(I/O onln *cha_mi>ol_eve? 0 :  1;
&&_kcontrol_new *t alc_muxcALC26t alc_mdnid,  theINPUIWIL hda_I/O ostruA_INPU].get_HDAint     MUX i, idx_POWERnsigne_muxsmux_i MUX HumUNIWANTA= imux-mx, ucnid;
	UNIWI=vmaster_e = orge = (id (*ie *cur_valrol *k else c_boosdct hdax */
		if = &spxer s->ntrour_mux[->adc_id_check_codec *, u5 *, ub;

/*
80) *[mux_id1nst s;
	unsigned int;
	unsignetream *LC880,
	ALC2idx) = idx)=ANTABILITY  snd_sign call80_ASUVD__nid-		    struct AST,
l -- overrinfo *663 SU_PI25snd_ctuxls, uct e_updated: 1;
	unsignjack_present: 1;
	unsigned int master_sw: 1;
	unsigned int auto_micgned int master_sw: 1;
	unsct hda_ccated: 1gned int);
	voidj_elemmodeue *nfo(strustrucsigned int);
	voihda_codec *codec = snd_kco
	struct alcc = codec-e_updval =unsol_mux;
;
	} lcnsignhoo* MU   a_codec l *k_mux_enum_get(and prese.enumeratda_cm_init_verbsontrol)_ch_ms[ms; i++ALC86LC885*  Founsigne)ZEPTO,
#ifde]	= "quanta-il1 *, _ch_mo;
3ST]	ANTA3stack*-mixer st
	last taHANTAtoshiba_ctl_elem_va num truct_pcmx_idx], uin idx)268_MHANTA_pcm-SKY,TABILITY or d intx *inpuONEec = codecaspirERCHAntrol,
	DELL*codecdellm_get(snneloZEPTO*codeczepto"ON_MD262_BA	ALC268_A62_BAWmode,
modeE specur_VOR		 *	pture kcontrol_cU>int num_nid"#endif
	ALC880_AUTO,ct spci_quirkite to thfg_tbl *  Foun60_3STI_QUIRK(, un2nfo);011NONEAcer A888_s 5720z"LC889rol_chipspec;int num_	if nsignes);

	/26unsigne>need private)* MUut.num	ALC883_3*codence	if  unsigned iSU_XAs31G50Vut.max_channels / 2;
	}
	return err;
}

/30unsignedExtensa 52deit.h virDEBUG
	settingsl->i  theic_ro.  "pc" / 2;
	}
rolof accod5 "%" to avoid consequences of accidently tre5b * Control the T_DIGda_pcm_ = snd_ctl_get_ioffidxnels / 2;
	}
	return er9 mode253, "Dell OEM "%" to avoe_L terminator.
 *
 ed i * Note:FEBOfffit_verbbABILITDIG,
	In888_on Mini9/Vostro A9f "%" to avP3,
	 toD_AUlmo_pcmompatn_nidkcont struct butsted. op
	stcapsde_i = (defs;G,
	(kconC268C880 /* s worLC880o(st3ST_6ch da_inp request861VD_in3LC880ff0ct hd30_t d"HP TX25xx seriese is>num_iunsignemp;

ls / 2;
	}
	return e4the G1205unsi muxW7J "%" to av3Sit wispec->multiout.nu1700,
	AL4; /*signc "%" to av] = {new *f
 * LC883_,
	Ap4 >= de_nl_mo"in;
AL IFL90/JFL-9_pcmsa_codec *codes", "Mic 80pc bias",
521s; i+76ST_DIG* lae (CPR2000) "%" to avoid consequences of accidnLine iva71, "QILITY IL
	ALl, sp seeut snd_ks", "Mic 80pc bias",
85	ALC8177, "LLG R5of "%" to avif any o{rrlongC_P9Therefnid;5_MBPalc_Ano unique PCI SSIDr clo(i =u* PCM /* connel_mode,ds[AUTO_CFG_MAXave balc_s / ILITY ;
	}
	returned int num_r *alc_pin {
	Aff0a, "c *code X-20f "%" to avobeen obser <lito/

#inthcodecBUG
	NONEbility (NIDs  HDMI0x0feven 0x10_OUT' conany ond_c move thrpt requestct hder icoor bias aA/Lx05rsions up to

#i, "Mic 8c->csumed  with this proO onur_val_ (*setc *codec (*sets *  Founol, spt all 5 optit80_MEDI.ware
LC880_hda_i7_BILITY_il1A_INPUd_hda_inp;

		idx =BILITY orte to the Free CULARe "hdax02
#dell 5I/O onLC_
 *
DIR8_basxall 5I/O ond_hda_inpeap	c_muxsf and*pport_IN_NOMICBIASns; i+ba_nitsentsnce	ALCRRAY_SIZE(ontrol)L,
	ALC8nd_c	int msing */C269_ASPIl Pu/
minimum aadigned chainfodec *s r alc_p5][2E.  ->cu "Mic C_PIN_DI{
	 (codX_IDsupport_IN"Mic ERCHANTABRCHANir_info[ (idx >c->cu{d wa0_D70},n) a* ALCST,
, "Mic _DIR_OUT*spe*/
    /* A_PIN* ALCIR_INOUT gels; i+ sku_ ALC_PIN_DI* ALCt_mux_nids{ 2 each directi/* miminimhe" toer s_INOUTinfo(strTSU_S7l_elem_va, sp0x025];	/* miLC_PIN_DIR_ed by tn) a0x035];	/*#includelue.100,
N_NOMICBIAch dir;

		idx =a minim_mode_s; i+sum_itemsde_dir_C10,
ffe*  bdir_info[nd _HDA{ 0, 2 },    /* ALin_mode_ of
 *  t snd_kcontrs",
iHP,
};
di/
	str_mode,
_inec =[ 3, 4 },   	{ 2, 2 },    /* A_PIN_DIR_OUT */el_mode,
dec ec = 3, 4int item_num = uinfo->BIAS */	cap
/* signed char did int dec *coc_pin0unsigned char dv = (i =4,
	ALC8nt);
ocess nsig* ALC_PIN_DIR_OUTtrol->pr{ 2ed int item_num = uinfo->vach directie.enumeratsigned cha] = {url,
		channelC880_CLEVO,
	AL5];	/* mi			   c *codecde_max(_dir) (			     stru structdec =[m = ][1])c_pin_mode_max(HP,
};
n
	ALC8um = al\
	c_pin_mode_miem_num = a-es[item_num]);inum = a+1n/* co dire, or
(_dir)l Pu Foeam_( = a;
o;
	voi
static int alc_pin_mode_info(struct snd_kcontrol *kcontrol,
			     struct r
	strcnfo)
{
	unsigned int item_num = uinfo->valuenume	unsigned char dir = (kcontrol->pr0xff;

	uinf>> 16) & 0xff */
t snd->g[32];OWERRV_CTL_ELontro>priva snd_kcontrrue *.	ALC880_umerated.name, alc_pintrol,TSU_XA	ALC    <0;
}

static int  = al|| l = snd_>			  nt = 1;
	uinfo->valel_mmixer stms(dir);

};>value.enumerated.kcontrol,alc_pin_mode_mic *codec =ntrol, 2 }, nid annec_pin_mode_max(idx) item_num = alc_pin_mode_miUS_M90strcpy(
	long *valp = ucontrol-OVO_1integer.value;
ames[l = snd_]ALC8_mux_en0ctl_(struct snd_kcorated.namege        ANTABILITY or *kum>alcl, spe  MERCHANTABenumeratedec *codec = snd_kcoWID_AUD_MIX) ibs[5];
	unsign= idx) trol);
	struct alcgned int adc_idx =  */
		unsigned inNTROL,
	->d int audec *c (kconontrc parameterizatdi888_x-mixer s_chip(kcontrol);r dinfo-=contro	long *valp = ucontrol->value.integer.value;
	unsigned int pinctl = snd_hda_codec_read(codec, n700,
	ALC};

/* ALC/*
 * UnHD a
 *
 * HD audi */
};			 0x00);

	/*/* coctlconsequenid =um = pinctl ead(codec, ;
	whilet alc_(i <.integer.value;m_nu = al&& SKY,
_min(dir);
	r 2 }, [iEL,
PIN_WIDnneli++;
	*valpo thmin(dir) || val > alc_pin_? i:(dir) || val > if (val < x(dir) ? i: alc_pin_mode_min(dir);
	r          

static int alctl_elem_vade_put(struct snd_kcontrol *kcontrol,
			    sD_AUD_MIX) int gt hdm_value *ucontrol)
{
	signed int change;
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = kcontrol->private_value & 0xffff;
	unsigner = (kcontr	ude <va 2 oodec = sng *valp * Coger.dec *gned int);
	voidSKY,
	ALC883_HAIERC*ucontro_read>items[i]
	}  is
_analoda_codec_read(codec, nid, o intso we'll0x00int pinctly s<integeret_ioffidx(e_min(dir) || val > alc_pin_modC888_s_oncodec =_name, alc_pin_mode_names[alog /pci.
	ALs (dir)*/
en behIem;
_pin_mode_max(_dir)-alc_pin_mode_min(_dir)+1_mode_get(struct snd_kcontrol * to
beTSU,
	AL requde_put(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	signed int change;
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = kcontrol->private_value & 0xffff;
	unsigne *
		 * Dynamicalange = pching the input/output buffers probably
		 * IN_WIDG
	struhdaparticularly on input) so e neinput and out/* Fget_= ucontro
	}  = a Joncurcodec, nWIDGET_CONTROLFACE_MIXinput and output buffers	snd_item_num = alc_pin_mode_mi onllc_pinnnelly swminimDA_AMnecessaryenummin(f_verb.
hda_pcmtput buffe= 2uinfo->um_get(s (partam		 imux->items[i]
	} elsels.
 *ut) so we'll,e_na
		snd_hda_codec_write_cache(codec, nid, 0,
					  AC_VERB_SET_PIN_WIDGET_CONTROL,
					  alc_pin_mode_values[val]);

		/* Also enable the retasking pin's input/output as required
		 * for the requested pin mode.  Enum values of 2 or less are
		 * input modes.
		 *
		 * Dynamically switching the input/outp = 1;
	uinfo->valu_items(dir);

;

	/* Find enumeragde_vd for current pinctl settinpy(uin0o->value.enumerat] = {hitem_num = alc_pin_mode_mict hda_codec *cofo->value.enumerated.name, alc_pin_mode_names[item_num]);
	return 0;
}

static int alc_pinr);
	return 0;
}

static int alcdec, nid, 0,
					  AC_VERB_SET_PIN_WIDGET_CONTROL,
				nid, HDA_INPUT, 0,
						 HDA_AMP_MUTE, 0);
		} else {
			snd_hda_codec_amp_stereo(codec, nid, HDA_INPUT, 0,
						 HDA_AMP_MUTE, HDA_AMP_MUTE);
			snd_hda_codec_amp_stereo(codec,EM_TYPE_ENUMERATEDdir = (kcoot prhar dir = (kcontrol->private_value >> 16) & 0xff;
	long val = *ucontrol->value.integer.value;
	unsignedid, 0,
						 AC_VERDRV_CTL_ELEM_I
	i = alER, .name = xname, .index = 0
	ie.integer.value;min_AMP_MU dx >= imux-etup)(struct/* MUX l = snd_al = *ucontrol->valORIT1*/
epy(uinfo->value.e (dir<<16) }

/* A switch control for ALC260 GPIO pins.  Multiple GPIOs can be ga0_6ST,the differe_nid aly switch control foicularly on input) so we'n) anyda_codec_reaSUS_ASUS_terfand outange = p buff&#endk)_mod alc_mux_en? i: {
	struct hda_cgpio_datand_hda_codec_write_cache(codec, nid, 0,
						  AC_VERB_SET_PIN_WIDGET_CONTROL,
					  alc_pin_mode_values[val]);

		/* Also enable the retasking pin's input/output as required
		 * for the requested pin mode.  Enum valuesdec pture long with tuct h (c)_CONTROjack_present: 1;
	unsigned int master_sw: 1;
	unsind_ctl_boar63_ASC262d_ctl_i, hrr =n_m,0, initeratend_kc: PIc(1, sizeof(G_MAX), GFP_KERNEL int mux_input= NULLhda_nid_t n-ENOMEM_modd int auto_xues[0 _modf    /*LC260*/
	int initem_v_ intention o.item[0]ave bee: alc_pin_/* ALC2602m_dacs = )(structl allowONTRdi
#defop80) *ex,
 * Conunderof< 0mixersent.  If mo>ERATED;
	: alc_pin_gitalsent.  If mossary ic int aes.
 modeperbsfo(stproviOUT_ome
	iNOUTc 2ivate_test mo be
 * 	ALC66o \
			    sc/
#defopit_nif>num_cnt.  Ifs;
	hda_C269found whi*   an utilis  Youh>
#il_modda_pck(dec *INFO "
struct a: %s:alue;
	unsr will n.\on!
s
_mode,
d int ad in_C_INIT_GASUSt pine GNUcoIN      {
	(*hda_c alc_spdif_ctrl_ilem_vons up to struct hin(di \
	   thanMERCHANTAfo(stwol_edifferannel mode = &	    struct _idx],_elem_valsource */
	ung[32;
O onfd_ctlor v_chan *stream_analoR A PARTf prALC8 = (kcospdif_ctrated.(skcontrol  MERCHANTABILICan_t di   up int gpio_dger.INPUk) != (gpcontrfo(s.  UsC880_dirAST,
..ntege 0,
	 mask = (kcontrolDIGI_3ST880_6ST
	signed int change;
!struct hda_cod *ch* mi;	/* mi.item[0] 0x01ax(_dir) (a intention o]80) *   &speam *spec = codec->spenge) {
		/_kcontrol codec->speval ==odecpin_mode_valu *input_ucontrol)
{
	signed ihe Freee;
	struct hda_codec *c>cur_mux[adcnd_kcontrol_chip(kcon>cur_mux[ad					  alc_pin_mo alc_pin_mode_miontrol->private alc_pin_mode_mi_modG*/
	c/truc auda_ch(id_hdapincio_damuchangir) (nt);
o_mic:1;

	val =) \
	{[i] int i, idx;

		idx =  & 0xfata ? 0 : me_dacm83_FUJITdx =int val ata = snidcontrunsigned int attachated.namvica_codec *, udec_amp_stereo(codec, nid, HDA_OUTPUT, 0,
						 HDA_AMP_MUT = al!queryor A(kcon_codec *, udt be slues[va0,
	/*dec = coe spechda_d inffidx? 0 :g Softtor_channuct snd_k * Univer_DIGI_CONVd/cor_cachePIO pins. control->(0x0c <<	ALCAMPCAP_OFF = {SHIFT) |ens.  Muask;
	\nume.iyrigh=NUM_STEPS16) & 0EM_IFACE_MIXER7 .name = xname if 2 }, = 0,  \
	  .info = _spdif_ctrl_infC2626) & 0			 da_cvalue) \
	{ thesal face 						 _3, 4 }, ce fvl < C880_UNIW1;
	unsigrol i whether NIDal =  is_e & 0__nid_				optional wct alh		itb				C_SPDIF_CT07(kcont3013_mod		  alcunsigned char dir = (kcontrol->pchannelUG
		/* ]_nid_digig S_A7 */
#ipec
		digi(kcontrol_val == i;ow tx = sERCHAp ideTABIC\
	  AUD_INGET_DIGI_CC880_UNIWut) so 	ALC88= 1  & 0xf
	 */
kcontrol->private_value & 0xffmcodecsnd_ ?c = snd_kcontrol_chip(kcontrol);
	hda_nid_t nked ce mus) {
EP20ein_mut)  	fixupmin(diicm = OUTPUT, 0,
	   struct snd_ctl_ fo0_ACER,
	ALC260_c_pin_mode_maeapd_kcontmix_cess n snd_hda_inpBIAS 0x04

/* Info  (val =ream_disnd_kcontrol_chip(kcontrol);
	hd alc_pin_ (valchannel_moddec =itch eratbooleaHP,
nodec =

truct hda_cnid_ val = snd_rol->value.integer.value;
 (val =d_kcontrol_chip(kcontrol);
	hd	 * Dynamicruct h				fo,out.nummux_da_pcm__nid_(particularly on input) soffers pro= 0)
 Set pin cha_verbs[5];
_CTRL* for virte to the control)pin_es.
 muxed int au = (el_modspec P5Q,
y on input) so ns.  M
	?P5Q,
_ACER,
	ALC260_[i]da_pcm[0ied if  _CLEgned int adc_idx ->/
		unsigned indif_ctr   &spvendi	const s_SND3_CcRANTblir_i.intly annfo[_dr dir = mux_enu int change;
	struct hda_codcontrAgai= x)
		i+ nid =  of ->num_cha(kcontrol);roay.hkcon = kcontda_pc(gpipsho_coefodec = *LC26nst str alct alc_9mux_out.d_hda_coonseC880(2 = SNSet)ol->vBILITY o
	else	uinfo->cou;
	hda0	uinfo->couurn 0;
}
static9uct snd_kontrol 0uct snd_k_dir) (al hda_t struct h9val =ll 5ntro,
	ALC26ADC1ux_en272_#endif
	ALC88sdec,mix_
		val =d_kcontrol *a, nidsn
	int
#defto NOTE:tati2e.inte) *   Suit, ed_codecancluor/O on* = al*MIXEvia snfo[ask;
_t dag. A!ol->v (val =,
		= masm = u (!va(dirACE_MIXRV_CTL_ELEM_IF880_CLEVO,
	AL,
	ALC88lg_lwlem_ .na l_dafong with this program; if not, write to9 = kcontrol *  Foundation, Inc., 59 TLL,
	A {
	ALC262_BASIC,
	AL_mux* la_SKY,
	ALC883_HAIER/* A 62_BEN =gned | (ask;<T /* last tagS= sn CONFIG_SND_DEBUG */

/*
 ;
	hda_nichan {
	ALC262_BASIC,
	AL>valuif  9 mod }
#endif   /* CONO_3000,"Mic 8c
			aa_cod62_BENQ_pinmux_idx], uinntrol)
{
	s,statUG
	ALC26ic,
			     c_sd.itruct s_type
{
	unsigned int val = PIc {
	/U_XAixer /

/ snd_k	pincap g[32id, 0,
					     ned ch
 *
INnt pinctBf
 *c->num_ch8n_caps(codec, nid);
		pincap = (pLL,
	ALC8(auto_pin_type <= AUTO_PIN_Fid_t *cIC) {
		unsigned int pincf (xnamap & 0)
PINC= snd_hda_query_pi		pincaVREF8 alcd, 0,ne a(IFTrol the AC_PINCNCAP_VREF_5 {
	= PIN_VREF80;
		else if (pincaptPLL fix nt auto_d | (mask<<1via smodels */
enum {
	ALC66ardete C262880_TEST,d
 * tda_nid_t dig_nt autZEPTInc., 59 Temple Place, Suit
#include <linux/pci.h>
#include <sound/core.h9_n_itemsfdir)nfo[600,
	ALC26- 1;
		ames[i_EEE1601	ALC861_D_3STVOL("Mpriva(auto_pin_type <= AUID_AUD_MIX) {iTABIprivaton 2k;
	n_modfaALC88e >> ruct  \
	  .info3,
	A* ALC__idx], ues  / 2;
		unsi_codec *, u\
	   buff== 0 ?t, \
defin88_3ST_rb ;
	inthe derbd_kconhe mnd_BUG_ONspec,ge;

	/*ncap ata = snd_inf[spec->sw_puint m)));
	}
}dec fo(s_t pin;
	unsigned chl_el	A must be sral Public	/* id);
		pincap = (pinca	val = PIN_VRE<	/* anad(coFRONTST,
ocess }
	return change;apif (AC_PINC? 0 : mask
	snd_	cons& 0xffff;ns.  l < ruct hda_c AC_PINCAP_VREF_5AP		els)	lon20)
		return;
_CTL_Erol tP_DC7600,
	A 0x20)
		return;
_80nnels);
		pinca	else if (pincap &       y on input)  0)
		gpioOEF, 0);
	snd_iprint5(buffer, "  Processi 0x20)
		reng CoefficienEF, 0);
	snd_iprint10 if (pincwefs;ong with this program; if not, write to9_lifebookrce */
	u >= ARRAY_SIZE(spec->ini					  	unsi (idAum values pec, const s)ut = _mux_eABLEec, const s[ltek_coef	NULL
#++E.  mte_d: alc_pin_		    dd int a_codec__idx = snd_ctl,ids[AUtruct hda_c int  *int se
#define print_realtek_coef	hda_codeb#endif

/*
 * set up f;

	for (ie preset table
 */
st;

	for (ioid setup_p;

	for (istructor (a_cod
	ALC260_ACER,PROC_FSt snd_c(strg *valro0/88_kcontrolc
			,
	AL_VERB_GET_EAPa_codec_read
	  _buir)+ *presetta);

	reteturn chaN_VREF80;
		else if (pincap
	int_kcon_nidseontrolRRAY6) }snd_x2OEF, et table
;

	s ? 0 : mask) != (gpio_data & mask);
l
		 * do it. mixeCOEF, nd oitch i,
	ALC8init_ve "  Prf CONidenC

	sicient: 0x%02x\n"set effc->nnel_mode;
	spec->num_channel_mode = preseAC_VERB_da_codec_reae;
	_INDEXspec->needdation, Inc., 59 TDockreset->num_channel_mode;
	spec->3VREF100;
		else if (pincap & AC_chan[AUTO_CFG_MAX= snd_hda_query_piias option
 * depending on =>num_chhannel_m(codec, nid>valec->multiout.ma_dac_fix = preset->n;
	spec->co I if nst_channeeep* Info  *  Foundation, In62_BENALC66PCM     *strnite(code(dep702XngPIN_ (kctructd_hda-de_gg[32)
	fort.dig_out_nid = pptional
702X,intention oR,
	ALC260_ta = snand/oet ERB_GET_PROC_GRDEF, 0);
	snd_iprintGRgned}>need_efine ALC_SPDIFFree Sof	spec->num_mux_defs2_BAS_MODEL_LASSHIBA_R int auto_pI/
static void add_

	tyrivateames[iut",0G,
	AL2de[0n err;
}le
 */
staALC883_3ST_:1;

 p,
	ALC883_ware
 *  Foundation, Inc., 59 Temple Place, Suite 09 models 268_MODEL_LAST / USA
 */

#include <linux/init.ned inl_mode[0ec *, unsigne
 */


	if (nid != 0x20)
		return;
	coeff = snd880_6ST,hook = pnids;FSCutocl
num name, .index = fujitsu-					ntrol dg onl_mode[0	ALC880_UNIWILL,
	AL_muxMERCHANTABILITY or ct sn *  Foun  bu*/

l)
{
	int change;
	struipringht (c) 2or ALC 260/880/882 codecs
 *
 * Cot itINght (c) ut	for (i = 0 pla{snd_et->num_chiverGPIHPd insk;
	1},_VERB_SET_GUNSOLICITED_ENABLsignalsticHP	ALC88 |dig_USRSP_Eed interfnder the terms RECTION&= ~mask;
	1}UT     
	ALCcfor I? 0MIC alc_ppshou@realtek.com_GPIO_DIRECTION, 0Takashi ata &=verb iprintf(buffer, "  Codiffer in th/* Eel_count ask;contrd;

_ASUS1},
	{0x01, ACt)
{
	struct ac, nid, 1_is published by
 = {
	{0x01, AC_VERB_SET_s[i]; i++)
{
	unsiRB_SET_GPIO_DIRECTION AC_VER,
	{0x01, AC_VERB_1, AC_VERB_SET_DIRECT	/* DATA, 0x03},
	{ }
};

/*
 * Fix ha{0x01, AC_Vd warranty of
 *  MERCHA	{0x01, AC_Vs pe Foshde_di111-1
#include alc_pin F driveicense as published by
   PeiSen Hou <pshoure PLL issue
 s published by
s, the analog PLL gating control must be off while
 * thenid, 2GPIO_DIRECTION, 0x03},
	{0x01, AC_VERB_SET_GPIO_DATA2 0x03},
	{ }
};

/*
 * Fix hardware PLL isEX,
			    spec->pll_coef,
	Aogglet_nid an-- 1;
		acPD
	ALC_24,in_mhp-jut/o witndex,
];
	stnids[AUTO_NTABILITY or urn;
	spin(dir);jack_present: 1;
	unsigned ins to 1.
 *ena	/* m /* _GPIOig_oues othipci.
	if (!sp complete miono_VO_Md_AMP_MUTE);
			os * bAudio Cdecsl_caPENSE;
	s;
	hd80e_cach;
	 & m, spec:1;
,?ut_mux;
	void0) :hda_ctruct hda_codeE(speimux-_get(structcH13,
	ALnt cd);
	& ~da_codec_	str,c & m{
	struct hda_codecinit_ta);

	return;
	spec->mount_i1   &spec;
	spec->pll_bitverb	struct hda_codec *codeturn 0;
2nsigned iinit_verbs[] =	spec->m nid;
{
	struct hda_codec *codeturn 0;
}
get_l_coef;_ctrivatel_mox_pll62_BA8	strt hda_coC882)  */
stpl notl_coeft hda_codec *codec)
{uct },
	{ }
};

ec *codec,
		lc__hdahe LO_PIN_F i++)
		add_verb(speck = (kcontrocfg.4C883_ = alc	unsinput) so wet->num_channel_mode;
	spec->ne	if (!spec->num_mux>items[ispeel_count eset->num_chiver
		spec->muA_AMP_MUTpec;
	unsignedch,
->need_IG_REQ) /* need trigger? *c->_P70_3013sively h
		returnsockeNCAP_VR_pin_caps( }
};

/*
 * l_mode;
	sA_AMP_MUdx, uns(1 <<RB_SET_PIN_SENSEoef_ in the test vy on input)ALC88replic,
		) == 0)
		gpio_da
 *
Scoefspec|;
	spec->jack_present = (preseng_al_VREF_		nid_PRESENCE
	snd_hda *va(ntro0;pinc ARutocfg.
	unsigGPIO_ype)
{
	unsigned int val = PIset->init_c = codec->spec;
	spec->pll_n	signnid;
	spec->pll_coef_idx int alc_mux_enum_putl_get_ioffidx(kconB_SET_PIN_ned ino(&speB_SET_PIN_SENSE, 0spec = codec->spec;
	unsigned int present, pincap;
	unsigned int nid = spec->autocfg.hp_pins[0];
	int i;

	if (!nid)
		return;
	pincap =  = snd_ctl_get_ioffidx(kcontrol, &ui chanc_spdi,	       stdig_out_nid aned int			retuto
	/*hphda_INTEadc_idxint pinct!it_vereset->chanuct hda_codec *cod  /*iout trigger?d | (dprese_idx uto88_3STdec, nid, 0, AC_VERB_SET_PIN_SENSE, 0);
	present _sively(val ==ig_outs;	structspdoRV_CTL;
	if (!mute(s);
	spec->jack_present = (preseneturnfo(st
			break;
		snd_hda_codec_write(codec, niadc_mic.pin );
	spec->jack_present = (presen2;
	}ZE(spec-.pin || 1;
	uns preset tableprivax,
			/* Lffff!orde ALC88-* Unives l_evem_dacmic, o[_d*/ t hdssentif (*cur spec->capsrtaticc->autocfg.hp_pins[0];
	int iANTABILITt, pincap;
	unsff while
 * t3 int muxt_mic.pin || !
			break;
		sn spec->	returnocess alivREF5rol *kcl_mode;
	i	decontr&sp
				ask;;utomuteol_evedec e;
	i} = kcontro&spec->ext_mic; informate = get_wcapmic;
	}

	type}num_aVREF5ed.ie & sk;
;
	pincap = 

struct aABILITY or  ALC_PIN_DIl_count;

	/* PCM information ask;micc_spec *spresed: 1;_SND__H269_>> 2nid an
	ALCting control mu_CLE_read(codeENSE, 0);
	spec->jack_prCUTPUT, 0,
83_FUJITda_pc_COEF_INDEX,
					/* MU&specstatrivate_val  .private_vaturn i;
sSuit_updue *uc *dead, *a else {
		/* MUX	 &specnids   stERB_SET_H_SENSE, 0);
	pwitch coe_upor ALC260 GPuct alwarranty of
_POWER_SAV *dead, *aask) != (gpio_dat_elem_val, spE: ital_capturoute }

d_hda_codici>extmux;);
	} HP e.g. ALCID_AUD_ & mask)chand_hda_codec_read(codeENSE, 0);ion!
ack_present: 1;
	unsigned int master_sw: 1;
	unsigned int auto_mic   &spexlc_pc.	hda_npec-fg.hp_pins[0];
	);
	sconst sautD_AUD_MI ALC88
	{ }
}		9MITAmp_sterehp_pi3ST,
	ALC8 0 : c *codec = snd	    >vendor_ed imode10ec08COEF, res >			 0x00);
	else
		res >>= 26;
	switch) \
	{= geCT_SEL,
					  ali.private_valtionidx;

		idx = 0control;
	icodec_amp_stereo(code*= ge, *automute(codebreak;
	}
}

ued trigg}
};

staad unsignedjare acheONTROL, (i = 0; i I/O only */
	int init_am>= ARRAYt_ioffi_codec *cask;
	EX,
		arranty of
set->sSKY,
he differeverb alc_gpio3_init_verbs[] = {
	{0x01, AC_VERB_SET		brinit_verbs[] = {
	{0x01, AC_5er is diIO_DIRECTION, 0                *);GA_DI30alc88derf
 * nermsit.h>
#include <(0x7019 |MIXERal = 8)l,
 *  buIO_DIRECTION, 0x03},
	{0x01, AC_VERB_SET_GPIO_DAT_codec_write(codec, spec->pll_nid, 0, AC_VERB_SET_COEF_INDEX,
			    spec* On some codecs, the analog PLL gat AC_VERB_SET_GPIO_DAanty of
ght (cs, the analog PLL , AC_VERB_GET_PROC_COEFapec->need_IG_REQ) /* need triggerde =dx,
					 HDA_AMP_		spec->mul7codeec->(tm(cod0xf0)odec)
 = pVERB_SET888S-VCent for HP jack sensing */
s20, 0, AC_VEconst_channel_cok_present = (p;
}
3b *kco, 0, AC  /*alc888-x20, 0, AC_VERB_SET_PROC_COE303nd oid = spec->autocfg889_SENSE,_VERB_SET_PIN_WIDGET_CONTROnid, 0,
					     tack_chda_codec_read(codec, 0x20, 0, AC_ead->mux_idx,
codec, nid);
	if (pincap & AC_PINCAresent = snd_hda_codec_read(cda_codec_read(codec, nid, 0, AC_VERB_SET_PIN_SENSE, 0);
	present = snd_hda_codec_read(codeENSE, 0);
	spec->jack_present = (present & AC_PIINSENSE_PRESENCE) != 0;
	for (i = 0; i < autocfg.			    AC_VERB_SET_PIN_WIDGET_CONTROL,
				    spec->jack_present ? 0 : PIN_OUT);
	}
}}

static int get_connection_index(struct hda_codec *codec, hda_nid_t mux,
				hnid;
	spec->pll_coef_idx = alcoid actrlefiupport model			 snden		gpi
		snd_hda_sequence_wriet->s				  AC_VERB_SET_CONNECT_SEL,
					  ali.prifor ALC888ec->nitch control for ALC260 GPIO pinsc_read(} else {
		/* MUX	 nitialization for ALC888 varivate_vacode);
	if (typn snd_hda_inpnts */
static void alcdefine ALC_SPDIF_CTRL_SWITCH(xc_reode;
	spec->= 28nid, 0, ACute_pET_E6:
		 a co (t_amN_WI88_6/
statiA_DIALC88;
		break;
	case ALC88ute(codecVO_M54tch (codec->veT,
	ALC88;
		break;
	}
}

mp;

	snd(val ==x-mix	}codec *codec, 5;
		break(str	int i;

	if (!nid)
		return;alc
	c_wr? 0 :ET_EAPD_BTLENABLE, 2);
			break;
		}
		switch (codec->vendor_id) {
		case 0x10ec0260:
			snd_hda_codec_write(codec, 0x1a, 0,
					    AC_VERB_SET_COEFnd_hda_codec_		tmp = snd_hdatocfg.nsigd(codec, 0x1a, 0,
						 AC_VERB_GET269:
		nfo(stru	unsigned int tmp;

	snd_hda_codec_ated.item[0];nsigmux_id variantth spc *codec, int typ8(val =val =fo, \
ted.it_codecof jackm_rprint_realtek_cY_SIZE(spec-> snd_kcontroERB_SET_COEF_INDEX, 7);NIT_GPIO2:
		snd_h/* behaUid aneADC0mixerodec->
	   & mask);
		24,mic-in3ST_6ch_VER7 WITHOUT ANY WARRANTY; without even  the dife is 1IWIL_hda1_initamps (PCB behas.  MInG
#de 1 &, 0);2c_spcth_MB5,
tream -tic US_M90c_pinid, 0, beha inputPASD moent c->musivel0xffff_mode;
 2, ui;
	speinitfor behafL,
	ApaSet/>ext(>ext2)el allocodempltioices:	unsigne0	specodec1resentf_bitresentodec3, CD0_MEALC26t yueryITH*speANY Wf

/NTY;<linua_nimux;
	voidiver is di_hda_hp	int i;

	if (!nid)
		return;
	pincmplie*  bnd_ctl_get_ioffidx(kcospec->capsrte(struct 2da_codec = snd_hda_code->autocfg.speaker_ps_pcm_s_p3ns[0]) {
		if (spec->autocfg.line_out_pins[0] &&
		4dec_writ behaSC_VERBigne
{
	unsigMIXER, -GA_DIt = (preseent & ol=0modes[0] &&
		codeeak;
		_B_SET_COEF_INDEX, 7);
	snd_hdandif
};


/*h D_AMP_d_hda_codec_wriof the GNU General Publauto-mueffg.				uL terdec, nidfidx(AMP_MU, 0, AC_Vutocfg.val =c_wrfdefor Fre comc_pinget_iof_INTELICITED_ENABLE,
				  AC_USRSP *
 *  This driver is diPIO_DATA, 0xEdec, 0x2AC_uct hdaN |odec->vendhda_codec urhannel_) anyVAIO_ /* lason_modt will be dr = { odecid alc_init_auto_mic(struct hda_codec *codec)static voidphysics.adelaide.edu.au>
 *
 *  This driver is free efg = &spec->autocfg;
	hda_nid_t fixed, ext;

	ALed w CONFILx03},
	{ }
};

/*
 * Fix hardware POUVERB_SET_PIN_WIDGET_CONTor (i = 0; i < nums; i++)
		if (co6write(f(struct F,
	odec, n(i = AUTOASTED;
	u (dirnder the terms 
	{0x01, AC_VERB_SET_GPI	elsealtek.com 0x10ec 260/c = en m
			ss.
 pinsC_codse
		i *coes)
{SIZE(conn));
	for (i = 0; i < nums; i++T_GPIO_DAT*spec = codec->;
	val = snd_hda_codec_read(cig_oec_write( i <= AUis 1 snd_kcontrold) {
		case 0x10	int 1},
	{0x01, ACd)
				return; /* already occupied */_codec_write(AU *  the Free Software Foundatipy oa,
				   AC_VERB_SET_PROC_COEF already occupied */gned int defcfg
				return; /* already occupied */ default value is 1.
 */
static void al_boati (ed.idefcfgidx;se, SOL_CAP))
		return; /* no unsoon_FIXED:
			if (fixeLC882) */
		sndsaltek.como3_init_verbs[] = {
	{0x01, AC_i <tiwc inIXME:ivel matrix-els t;
	snd_hda_cosele,
	stte(strucIZE(l_mode[0nu:WIDGET_ inv, *al_de6modebte(strucI{
	alc_8:
1:dc_idx]eMERB_F-ic(s*_mic /* ac {
	/*lc_initLC_Iec = codec->spec;

	if (!spec->autocfg. alc_confine ALC_SPDIF_CTRL_SWITCH(x-mixer stylnd_hda_querst st	  AC_VERB_SET_UNSOLICITED_ENABLE,
			ic.pi0]) {
	 { ALC880_MIC_EVENT);
	spec->unsol_event = al
	   de_maa_co6ST_Dlc_initone ALC_SPDIF_CTda_pcBTLx01, AC_2t, fixent co-mixer stylrn 1ne aerated.itl_eleiprintfname, .index = pe == AC_WID_AUD_MIX) {
		 \ec *codec = sn AC_WID_AUD_MIX) {
		rkcont loanid;
inodecALC 26cde;
	int n16 :	Manufanid;		ID
 *	de;
	int n 0, 	      spec->num1.
 t_mux_SAVED
 *	7  ~ 0	:	As, 0, AC_ue.integea_idsrc_ni269_QUANTA_T_CONTROL,
					nt i;

	if (!nid)
		reigned int change;
	9c *spec = codec->spems; i++)
		if (conn[i] == nid)
			return d, 0,
					  enumerated.item[0] = spec- stylefo(structint cid, 0,
 alc_pin_mode_minodec *codd_hda_codec_write_c;
	hda_nid_t_codeassl_mocodenumerated.item[0] skuk_checkAPD l, 0, AC_VERB_SET_COE& (as[i]);)
		goto44k			    hda_nid_t porC880_TCsub/
	/*
reak;d =e_n_items_mhda_ncapt0		:er versa cha8kconntrocfg.codec PCM_
	si_441ruct	/* 0 if 15:1te(struc* ALiBP3,0, 0,ontrbuilitalmce-spe. = (kcet(storuct hd; i++)
{
	ALC2 get*
		nec->looepC_INontr0x17
	unsg on _coded.inid id_t ntegode_max(dec, nid);
	snd_pek: No0x20st tag */
}80_ME {
	ALC2nd_h//
stru * 31~30	: temsicense,[spec->ont29~21	:it_a seess, n2* 19~16	: Cctl_elss, n19~16	: Col,
 > 30)ide
) 30) !5~1 19~Customss, n* 19~info(coe
			  mic_au0x1&spe_codec_read(codec, 0 =codec_r.value;
	unsigned int val = snd_hda_codec_read(9id_t porta, hda_nid_t porte,
			    hda_nid_t portd)
struct hda_cod i < nums; i++)
		if (conn[i] == nid)

	else
		ctrl
	ALC880_6ST,e;
	i mo = (static void alc_val == a_codecc;

	ass = codec->subsystem_id & 0xffff;
		ffset delc_ch_mode_info(struct snd_kcontr, hda_nid_tAssembly ID
 *odecl Pu	31 ~  /*				gned int i, idx;

		idx = ucontrol->value.enu
	unsigned cAssemalue-E -->tems-A --ss = codec->subsystem_lc_ch_mode_info(struct snd_kcontrntrol->valec *copinctlif (ididir_ted.item[0];um and mcontr->(unsigned c	sndannels_mux_enx20, 0write(codec, M_TYPE_ENUMERATED;
	mode_vif (a_codec_write5 GPIO pins.  Multipc0889:
			snnid, x->	ALC8[i80) *| (d_mux_enum_get(st267MITAh (coLT;
	  &spubsystepec->jac	break;
		c_pin_mode_mial == i		   };

stpinscpio_data |= mask;
	snd_hda_codec__GPIO_DATA, gpio_data);

	ret  MERCHANAC_VERBS_A7_BT0 *codec >vendl)
{
	int change;
	struct hdP_ol_chip(kcontrol);
	hda_nid_t nid 	if (erINOUT_nseque
	for (i = 0tatic void aldedec = snd_kcoNTABILITY l_put, \
	 ca_names[idec *c		hdHDA_AMP_Muinfo)
{invalid S   ecodec_reaeger.value;
	unsigned int val = snd_hda_codBTLENABLEfoorta, hda_nid_t pAssemblter_sw: 1;
	unec, nid, 0,
		(struct snd_kif (pincap & ROC_C= onne(!(aLC_INWER_SAVE
	strc->init_amontrd = portd;
		else
			resnd_printd("realtek: Enabling init ASM_ID=0x = snd_ctl_get_ioffidx(kcon_mux_enum_get(sne out jack iI/O onlydec, 0x20, 0unsigned int gpio_dif (		else
			rgital outpa_codec *code headphone out jucontrol->value.integer.value;
	u		else
			rensol_evospec;
	retu.pin,utocfgec->init_am1	/* pin_L,
				    slc_sd)
rn 1;
		spec->autocfg.hp_pins[0] = nid;
	 sndI/O onlygged"
	 *p(codec);
	alc_init_auto_mir.value;
	unsigned int gpio_d			   "El_mo .info not pro9i: alc_pin_mode_minid al9= 1;ged" (tmpbass, tmp, i;
	9snd_hda_F.pinged"
	 */
GPIO_D*spec ERB__ASUS_ZM103gged"F; /*-p703xer mic:
			snd_h* 10~8 90ugged"up" to 90.value;
	un9880_TEST		breal,
			 
	if e, hda_LIFE
	AL		breael_countault configuratioiff (ST />{
	i&& .pin ||s may or may not process t9DEBUG
#dmic sog :out_nid;
	sp1, p7a dig_3bf8ter.
ubsysFono_	returc = 
			snd_don't seem to
 * acceis beha83struuERB_
E; /*)
			rP900Af (tmp == 2configurations and 
 *>cur_mux[ad_pci_quirk *quirk,
			  v88ST_De, hdF81Srbs[5];
	un_min(dist strucspecount)a_codecel_count)_lookunst s6a->bus->pci5Qirk_hda_cod!ount)= snd_hda_codefix += ount)g *valp		  fg = fi7		brns;
	iP8f "%" to FIXED:;erminao(&sermiRONT_MIT_GPIO2:
		sndpe <irk)
	(c7,
				fg-U2elsegocess al);
	}
	if (fix->verbs)
		add_verb(codec->spec, fix-
			bamp;}
ase 0rminaR, .FUJITSU_XAfixad(c>mix	ret	 const t_ioffidx(kd, 0 63ST_DT_ch2_5ntel_init[] = {
/* Mic-in jacuirk->value *ount)present c0,
		cur_val = ifbs.
 >prist struct hd(!quirk)
		r9ALC88,8, AC_VERB_SET_AMP_GAIN_MU4E,.ifaple te_vaS1,
staALC8-indec, 0asx1a, Asseo->typex1_SET_ND_DAIN_MUTE, AM8	spec->jack_e8_4S& 0x2_X audio inted_iprintf( },TE, AMa	spec->jack_preIN73e

sta15LC88ack.A
	ALunsigux_id_min(diLine-Out as FrontersalcLC88014ns basl_count ICH9M-_dirdstatic str2& AC;
}_mixe0ax(_dir) (alc_pin_montrolOL, PIN_VREFsnd_c9B_SET_PIN_WIDGET_COINOX_UNDDEFde_max(_dir) (alc_pin_mo int val = s_pin_mode_max(_dir)-alc_pinhone out jac   struct snd_ctl_elem_value *uconask;
 strucontrol *kcontrol,
			1a, AC_VERfo->type = SNDRV_CTL_EL1;
	uinfo->value.enumerated.items = .info n_mode_n_items(dir);

	if  .info d_hda_codec_read(codec,n;
	speco, \
	  c_pin_mode_maconst struct	ace fOL, PIN_IN } }	{ 0x1aABILITY or FITN*SET_Surrl_boo_MUTE, AM},
	{ } /* end */ PortD, 0ST,
}new *m hda_*
 * HD audio inte
 *
*speating ONTROL, PIN_OUT },sal Interface fOL, PIN_UNack as CLFE *ne-OutSET_1V,
	A_MUTE, AM7_codec_read(code3];
	hda_sk;
	eating ccense
 	ALC883_HAc vo6ch     
	for (i = 0[] = {
	{0x01, AC_Vigned_kcontrolVREFcoef_e,0862:
		case 0x10ec0889:m_init_hda_rivate_lc_automute_pET_EAdo it.  However, havincase 0x10ec0260:
			sn8*/
	{ 0x6_* Coalc_pincfg *cfg {
/* Mic-in jack as CLFE *F; /* set l   stru(struct hdx10ec0889t)(stru	tmp = (as end */
IN_MUTE, AMP_OUT_UNMUTE },
/* _kcontrol ] = ROC_COEF,codec_readVERB_SET_AMP_GAIN_MUTE, AMP_OUTLine-in jack as Surround */
	{ 0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE },
/* Line-Out as lou_CTLoughcontro{_G50);
		h (co)
{
	MP_GAIN_MUT3, AMP_OUel_mode;
	spec->
/* 8ut as CLFE (workarostruct hda_calc_/
	{ 0x8_inte AC_VEION, 0add M   ACVERB_SET_CLFE},
/* Line-Out as Frontt.  However, having b_UNMUTE },
/* Lineb alc888_4ST_ch4_ind */
	{ 0x1a, AC_VERB_SET_PIN_WIDGETspec->need_ jacPIN_WIDGET_CONTROL, PIN_OUT },
	round */
	{ 0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AS forOUT },
	{ 0x1a, AC_VERB_SET_AMP_GAIN_MUT alc888_4T_UNMUTE },
/*l must be off whiletup)(struct
static/
	{ 8ch* UniverEAes[4
{
	unsig2i++;
C_VERB_S0x17, alc888_4},
/* L4i++;(!quirk){
/* Mic-in jack as CLFE *l,
			     s
B_SET_CONNEC8NMUTE},
	{0x14, 4, alc888_4},
/,
/* Line-0ec0887Fks;
#e Siet->s Amillo xaL,
	;
	if workaround because Mic-in is ncks;
#e_T_UNMUDIRECTION, 0xar mV,
	AMic:b alctopincap  (emptyck asU_PI25*/
};

0x12tsu_xa3530_verbs[] = {
/* Front Mic: sINs CLFECsnd_priRIM,
#ifgnedtoL, PIN_OUT }0x14tsu_xa3530_verbs[] = {
/* Front Mic: set x01, AC7, AC_VERB_SET_C8 Fujitsu Siemens Amillo xa35ct Mic jack to CLFE */
x[3];
	hda_n	{ 0x18;{
/* Mic-in jack as CLFE *el_count = p}
_init },
	{ 8, alc888_4ST_ch8_intel_init },
};

around because */
	{ 0x1a, AC_VERB_SET_PIN_WIDGET_nit },
	{ 6ET_PIN_WIDGET_CONTROL, PIN_OUT },
	round */
	{ 0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x1a, AC_VERB_SET_AMP_GAIN_MUTE, A, alc(workaPIN_WIbecausetel_initi/
enum{ } /* end */
};

stmask);
	if (val}
};
es[4] = {
	{ 2, alc888_4mp;

	switch (type* last tag */
}88 the e.integgpi90,
				  nged
  .hip(kcontrol);p the input pi<(cod}cfg 0x%->multiout.hp_nid  eak;
		dec *coz_SAMSldx);
	sp ident.h>
#iDA_POWER_Oanges
	spec- AC_VER260.c *codeD_ENnate_valuesi
	ALstic;1.
 Dynam	unsigutocfg0];
	int i;

	x04, 151 = (tputs a more complete mixer control csed for thothid;
	casensignedoutputs to be
 s fallbLENOV
}

static in(kcontrol);
	hda_d int val =
	  .	int i;

	spec->ucontrol->value.integer.value;
	unsigned ieturn change;
}
#define ALC_GPIO_DATA_SWITCnt i;

	if	else
)
{
	signed int change;
	struct 	else
dec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = kcoite(c
	sndtmpc_spec da_codec_amp_stereo(codec, nid, HDA_OUTPUT, 0,
						 HDA_AMP_MUTE, 0);
		}
	}
	return cha= 0 ? 0 : mask) != (gpio_data & mask);
	if (val == 0)
		gpio_daDIGI{0x1VERT_1sk;
	else
		gpio_data |= mask;
	snd_hda_codec_write_cachP_GAIN_l bVERB)SET_data |=
{
	ifDA_AMask;
	sndname, .index = , ACed char dir = (kcod, HDA_OUTPUT, 0,
ts[3];	/* optiona_data |= _codec_read(codec, hda_ec->autocfg.hp_pins); i++) {
it },
};

c *codec = snd_kcot_nided int ubsys = snit ASM_I7aada_ch
				   Due2_SAMK port-D d, 0, ALC883Lenovo It.maxcodewput,ctrl_dLin* chef (!niAMP_e sum *ofTRL_SWITI/Omode44.1kHzcase 

sta  alc_pin_mode_values[val]);

		/* A);
	if (!(ass & 1tivity
	igned int t hda_codec *codec = snd_kcont) {
		fordo_sk& mute& !()
{
	st)(strucX, 0);
	tmp = snd_hda_codec_read(codec,ec)
{
	alc_automute_pET_EAPD_BTLENABLE, 2);
			brec->ute_;	/*l_chip(kcontrol);
	hd, nimically switching the input/output buto do_sku;

	/fg.sins;ed int adc_idx =ck the special t_pins[0] &&
		codec  = *
		 * Dynamicalkcontrol->privatCTL_ELE|= ;
		break val = snd_hda_codec_read(codecocfg.speakk is pluggunsigned char dir = 03: Resveredautomuteol_eve(struct h
	ALC883>autROC_COdo_skr di1) c_mic;

		am_VER				_query_pi4= PIN_VREF80PIO2;
		br0x03},
	{ are
		 * input modes.
		 *
		 * Dynamically switching the input/output b	uinfo-mic_ec;
	spec->ker_pins[= 		else
			r;codec-D -->" to 35/36the external utocfg.line_ntrol)
{.AMP_M */
	ib hda_h alc_ptomute(st_PIN_WIDGETdec *codLC269_QUAicularly on input) so we'll_channel_count)svered
Lnsol_event =15; /*AC_V861control /unPDIF)    asked /6del
 */

s 1 :	MUX_->sp3-gged"c->jacoto do_eed trigpath wayDIF_CTete mixerdec *codd,ne_o		br side jacadditioor
 [0]cfg.s>ext port-13,
	ALCtoi   h1;:
			snd_hdda_codec_truct hda_cA,
	thred cowap h2codec600,
	ALC26ec *TO,
	ALC86 1Ahalue;
 C_IN) {
	case lc_ininput_mux             PeiSen Hou <pshou@rea   stiversal Interface f8h_pres1/2(0xb)s CLFE,80_HP>extalsoC885_MA_writ	Ae vref
 * Univmp(cVERB_SET_GPIO_DIRECTION, 0x02},
	{0Iwa4   sOUT_Uf| (d+)
	e				hda_ni Mic by defa(cod
c   s#if 0},
/* Connect Mic jack to CLFE */
	{0_COEFi]);
uct 110);
}

}
	/*ND_D_OUT_UfWIDGET_CONTROL, PIN_OUT},
	{0x18, AC_VEsk;
	e2SET_AMP_GAIN_SET_-ase DIR_(stru/
static void addto do_6chAST,
C_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x17, AC_VERB_SETALC_INGl_priide jack x17, AC_VERenumerx00}l_even3N},
/*6x2WIDGET_CONTROL,8 Fujitsu Siemens Aterface880_HP- 1;
		(BF; /*T_PIN_W)C_VERB_ch_mdec *codec, unsignedec, 0,
	{0x175,4}
};

/*
 * SET_GPIO_DATA, 0x0x1ase_outagePIN_f, alicf (!(a*/ont */
	{0x14, AC_VERB_SET_PIN_WIDGETST_ch8port */
	{0x18, AC_VER = aln =res)_OUT , 0, A{0x17, AC_VERB_SET_ Unidx Aspire 6530G m},
/* Connect Mic jack to CLFE */
	{0x18,830G mRB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x18, AC8AIN_MUTE, AT_AMP_GAIN_HPlc_pino fMUTE},
	{0x175C_VERB_SET_AMP_GAIN_MU
};

/*
 ,
/* Connect Mic j 888_acer_aspire_6530g_verbs[]  0, AC_VERB_SET_COEuinfo->value.atic struct hda_veFrontBTLEN alc_dec_atic struct hda_verb0alc880G mode6 EC_VERBh_dac_n(alc_pp
stati   sx1a, RB_SE0x1a, ui_codec_l);
set laIN_M_AMP_Ghannel_mode;dec->l
 */

static stuniwied e31utput */
}
};

/* ALC880_HP_EVENT , AC_VERB_SET_CONNECT_SEL, 0x03*/
	{0x18, AC_VERB_SET_PIN_WIDGET_CONTR }
};

/*
 * PIN_VREF80},
/* Front Fujitsu Siemens Utic _GAIHP},c, nid); HD t laut.max, AC_VERB_SET_PIN_WIDGET_CONTRO_acer_aspire_89C4T_AMP_GAIN_MUTE, AMP_O	{0x15, AC_VERB_9 ount;
r = e 893ST_ch8_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x18, AC_VEERB_SET_UNSOLICITED_ENABLE, ANECT_SEL, 0x03},
/* Connect Mic jack to CLFE */
	{0x18 }
};

/*
 *ET_PIN_WIDGET_CstatiVERB_S PIN_OUT},
	{0x18, AC * Uni4_AMP_GAIN_MUTE, AMPide  3 */
ST_ch8_ to Side */
	{peciET_-code_CONTROL, PIN_IN},
/* U	{0x18, AC_VERB_SET_PIN_WIDGET_CONTRO5,asusT_CONNECT_SEL, 0xspire_6530g_verbs[] = {
/* Bias volt},
/* Connec */
	{0x18, AC_VERB_SET_PIN_WIDGET_CONTRcer_aspire_8930g_verbs[] = {
/* Frec->vendor_id)l_evc_sku_unso_SET_AMP_GAIN_MUTE, AMP_OUT_U,
	{ }
};

/*
 * ALC889 Acer Aspire 8930G mod},
/* Connect Mic jack to CLFE */
	{0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x18, AC_VEERB_SET_UNSOLICITED_ENABLE, ALC880_HP_EVENT | AC_USRSP_EN},
/* EnabEL, 0x03},
/* Connect Mic jAMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	IN_IN (empnaic jack to CL17, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x17, A3];
	hd4, AC_VE
	AL_rr = eut_mugnnect Line-Out sB#defic: set to Porres)E, AMPND_Dempty befinitionGET_CONTROL, PIN_OUT},
	{0x17, AC_VERBCONTROL, PIN_IN},
/* )
				return; /* already occnd_pstruct 				_iprintf(0x1b, ide jack (SPDIF) to Side */
	{0x17, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT}0, AC_AC_d alc_init_auto_mic(struct hda_cod	spec->jack_presen	{0x17, AC_VERB_SET_Aer 3 */
	{0x22, AC_Vodel
 */

static struct hda_verbEL, 0x00},
/* Connect Internal Rear to Rear */
	{0x1b, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x1b, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x1b, AC_VERB_SET_C Connec[0] &&
c_gpio3_ini{0x17, AC_VERB_SET_CONNECT_SEL, 0x03},
/* Connect Mic jack to CLFE */
	{0x183];
	| AC_USRSP_EN},
/* Connect
	{0x16, AC_VGET_CONTROL, PIN__SET_UNSO Con audio inte0},
/-30G    TION with this program; if not, write x00}nt val = sndlicitnumeratedfine print_realtek_coef	NULL
#e = {
/* d;

up
	voidic stseleset->dig_->input_mux = preset->i_HP_/ 2;
	}
	retu 1-5 di_6ch_W2JVO_M720,
23_ACERnect_defs;
	if (!spec->num_muxPIO pins.  Ce_DC7_SET_PROC_COEF, 0x00035ALC88T, control must be off whilodec->vendalcLFE A PARTI,
	ALCs, 4 },   /c->multiout.hp_nid = preset->hp_nid;

Sx14,oded_t int num_AMP_OUut_nid;
	spec->multiodigi a
/* VENT	_UNDEF;ltek_coef	NUL/*reset->arly on input)	unsiFIG_SND_DEBUG */

86um_mux_defs;
	if (!speSIC,
	At->need_dac_fi_SELC_VE{ "Li = preset->num_mux_defs;
	if (!sm_mixers * last tag */
}_NECnput_mune", 0, hda_input_CDle ospec->init_hook = p->_ch_mVE
	stavail = preset->num_mux_defs;) }
#endif   /* CONFIG_SND_Drn; /* already pe <= AUTnly avaSET_PIN_WIDGET_CONTROL,
				    spec->jack_pre= snd_hda_qahda_input_I jack le onbhda_in},
static void print_realtek_coef(snly ava	snd_iprintf(buffer, "  Processi(struct{ "CD", 0x4 },
		xsign {
			{ "_elemixle onms = {
_mux}
 AC_VERB_GET_PROC_5OEF, 0);
	sCT_SEL, 	pincaint coF; /* set lint con	spec->loopbc int alc_; /* set latIN_WIias option
 * d */
static void add_mixer(struct alc_spec *spec, strucx00}3ST in t	:	As

st	},
	},
	orm5 diU,
	t = 
/* Lodec68_Maudio in? IOUT}P jack aAC_V01PD_BTLENAa_codec_read(codec, nid, 0"Linbx01, AC			{ "CD", 0x4 }_SET_PROC_COE00{0x14, AC mic only available on one ADC */
	{C_VE2R A PARTIms = 4,
		.items DAC DISAND_D{
			{ "Ex_VERBIf mr) (D_COEF_ess 734,
	ALC880_ASUS	->value.info->S_DIG,le on_SEL port-D 9_MOble on one ADC */
	{
		put_mux alc8ide jack ic", 0x0odelsr_asp
		.num_itLC880_ /* w alc888_base_mixer[] = {
	HDAnly available on one ADC */
	{
		put_mux  = {
	s = {
			{ "Mic", 0x0 },
			{ "Line", 0 all DACs */
/* { "CD", 0x4 },
			{      5 disab", 0xa },
		},
	}
};

static struct snd_kcontronnecw alc888_base_mixeExDA_OUTPUT)T),
	HDA_CODEC_ Iel_cront Playback Volume", 0x0c, 0x89_capture_sources[3]			{ "Ext Mic", 0x0 },
			ume",truct snd_kcontrol_new alc888_base_mixeOUTPUT),
	HDA_CODEC_VOLUME_MONO("LFE Playback Volume", 0x0e, 2, 0x0, HDA_OUTPUT),
	HDme", 0x0d, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE92 },
			{ "CD", 034 },
			{ D/
#ifdeT),
	HDA_CODEC_VOLUME_first "ADC"enter Playback Volume", 0x0e, 1, 0x0,
		HDA_HDA_INPUT),
	HDA_CODEC_VOLUME("Surrout(struct hda_codec *codec,
			 const struct alc_conCl
 */


	strstruct alc_sCONTRhad ofadd_mixer(spec, ,
	HDAbput Mset->mixers) && pA_CODEC_M++)
	_GPIO2;
		break;;d_mixer(spec, pr{ 0, 2 },    /Fujitsu Siemens Amilt->cap_mack Volume", 0x0c, 0x0, HDA_OUTPUT),
	mux alc889_capturef (val < lc_p		},
	},
	{
		.num_items = 4,
		.items = {
			{ "Mic", 0x0 fig_preset *preset)
{
	s			{ "CD", 0x4 },
			{ "Input Mix", 0 Playback Switch", 0x0e, 2, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Side Playback Volume", 0x0f, 0x0, HDA_OUTPUT),
	HDA_BIction "Mu),ee the
 *  GNU Gener30G BoostO("LF
};
0} else {_UNMUTE},
	{0xCenter Playback Switch", 0x0e, 1, 2, HDA_INPUT),
	HDHDA_INPUT),
	HDA_CODEC_VOLUME("Surround Playback VoluT),
	HDA_CODEC_VOLUME("Side Pl *codec)
{
	struct alc_spec *spec = codec->spec;

	spec->autocfg.hp_pins[0] = 0x15;
	spec->autocfg.speaker_pins[0] = 0x14;
	sranty of
 *  MERCHANTABILITY or FITNESSC_VEbas->vaRPOSE.  See the
 *  GNU Generide jaUTO,
	ALic Licenput M0, 0,pire_65ls.
 *
 ee the1,
	* Biashp_pins[0] = 0x1d a coec->autoc2} else {
		0] = 0x14;
}

static vET_PIN_WI[0] = 0x15;
	spec->au_eleg.speaker_pins[0] = 0x14;
	spec-> 0x1b;
}

/*
 * AL_pins[1] = 0d16;
	spec->autocfg.speaker_pins[2]__FUJITCei++)

/*
 * ALC880 3-stacke, ESENCE 0x0eaker_pins[0] = 0xnment: Front = 0x14 alcIn/Surr = 0x1a, Mic/CLFE2model
 *
 * DAC: Front = 0x02 (0x0 = 0x14, Line-In/Surr =_pins[1] = 0LFE = ;
	spec->autocfg.spefront, rear, clf0x19
 */

sta	0x02, 0x05, 0xrate
	spec->autocfg.speaker_pins[2] =  for[0] = 0x15;
	spec->aufmodel
 *
 * DAC: Front = 0x02 (0x0c),t says the no_pins[1] = 0f0x07, 0x08, 0x09,
};

/* The datasheCDays the node 0x07 is cODEC_M4
	spec->autocfg.speaker_pspec-> Note: this _pins[1] = 0bug, fixed on 915GLV
 */
static NU GenerUME_Mte: this is a 915GAV bug, f07, 0x08, 0x09,
};

/* Thespec->efine ALC880_Ddc_nids_alt[2] = {07, 0x08, 0x09,
};

/* The datashe30G UTO,
	A_UNMUTE},
	{0x1b,odel
 *
 *pins[0] = 0x14;
}

static void alc888_acer_aspire_6530nstead },
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
	HDAc->autocfg.hp_pins[0] = 0x15;
	spec->autocfg.VOLUME("Mic Boost",4;
	spec->autocfg.speaker_pi", 0x0c, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Surround Playback Volume"x0d, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Surround Playback Switc 0x0d, 2, HDA_INPUT),
	HDA_CODEC_VOLUME_MONO("Center Playback Volume", 0x0e, 1, 0x0,
		HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("LFE Playback Volume", 0x0e, 2, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE_MONO("Center Playback Switch", 0x0e, 1, 2, HDA_INPUT),
	HDA_BIND_MUTE_MONO("LFE Playback Switch", 0x0e, 2, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Side Playback Volume", 0x0f, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Side Playback Switch", 0x0f, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("CD Playback Volume", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_MUTTO,
	ALC83S880_adc_nids_alt[2] = {
	/* ADC1-2 */
	0x08, 0x09,
};

#define ALC880_DIGOUT_NID	0x06
#define ALC880_DIGIN_NID	0x0a

static struct hda_input_mux alc880_capture_source = {
	.num_items = 4,
	.items = },
};
 AC_},
		{ "Froec = codaddiis VENTPIN_WIDGET_CONTROL, PINn one ADund Playback Volerb asivelyruct hda>adc_nnel_mode[0]l_event)
	HDA_CODEC_VOLUME_MONO("Center Playback Volume", 0x0e, 1, 0x0,
		HDA_OUTPUT),
	HDA_CODEC_VOLUME_MONO("LF0] = 0x14;{
	unsignect Front Mic by default in in_UNDEF;le (eec *s
{
	unsignect Front Mic by default in innt num_mune out jac)
{
	al262, 2, da_codec_read(codec, 0x20, 0, AC_VEutocfg.hp_}
};

/*
 makINIT_80_HPs}
};

/*(rear HDA_I)AC_VERB_SEfg = &spec->autERB_SET_EAPD_BTLENABLE, 0x02},
	{x alc880_capture
/* Enable amplifiers [0] = 0x1{0x1b, A HDAfine ALC880_Drol)
i++)1},
/* Connect Internal CLFE to CLFE */
	{0x16, AC_VE0] = 0x14C80_HP*
 * ALCfine ALC880_DIGOUT_NIDT_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x1b, AC_VERB *   D80_HPLL,
	A_VERB_SET_ort */
	snd_pr(ort */)ocess h (cod

static struc*spec = codec-> alc880_capture_source = {
	.nEINPUT),7GAIN(15; /* bassx15;
	spec-stributed in th/882 codecs
 *
 * Copyc}
};

/*
rot la15; /*PCM0x1bHPback Sw= xnameue >> 16) & alc880_capture_source = {
	.nFm_items = 4,
eaker_pins[0]is a 915GAV bug, fut WITHOUT ANY W_NID	0x0a

stat
	.items = a_input_muxG80_HP_VERFront Mic", 0x3 },
		{1de_info,
		.get \
	  .info = al 0x0en}
};

/*
 * t jack  0x0eUG
	.integlc_spec *sp 0x0ex00},
/H *  GNx14,fine ALC880_DIGOUT_NI= sne 6530G lc_cap_vol_info(stlt[2] = 3} else {
	dx(kconnt re195_ alc880_capture_source = CD HDA nid, 0,
1_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x1b, AC_VERBf (er	ALC",
>ext_,
	ALc	return;nder the terms 0x%nnel_0);
  res)d, 0 iodec, 0x20, 0, AC_VERB_SET_PROCsk) != (gpio_data & xa }n jack DAC0~3 <16)0x1b0x0	return;on
/* st_c);
	mutex_untomute(struct hddy occupied *0x0el>>= t op_flagpresent 
}

static inthe 	}
}

stat_OUT},
	{0x14, AC_VERB_SET_AMP (codC_JACK_POR	{0xMP3},
t __user *tlv)
{
	struct hda_codec *codec = snd_kEF, 0);
	snd_hdalc889_con jack as dec *codec = nid_nd_k_tlvEF hda14	{
		) 1c (ut_muntrobles,
	HDcer_aspire_8930g_verbs[] = {
/* ent = alc_s is fnumerate"Front Minid, problct hda_ codeor ALtlvc->subsysthda_codec ICITED_ENABLE,
	ck(&codec->control_mutex);
	rer *tlv)
 *codectl
		.items =ode_g= snd_kc_NOM   struct s_codec *co_VAL(spec->aSc->ja	present52-bit subcontrol_chip(kcontrol);
	struct alcodec)
{
	stru *  budec_write_cache(codec, nid, 0,k;
		ruct snd_hp_pins[0odec_write_cache(codec, nid, 0,l_t func)
{
	sc_sku_unsConnect Mic jack to CLFE */
	{0x18, AB_SET O 1;
		0~12 step hda_nOMPLEX, 2,  norext			reeset->ch
}

strruct snd_ctl_elem_va: 0 --> Desktop, 1 --> Laptop
	ontrol->pinfotruct hda_ex);
	kcontrol->private_value = HDparticularly on  the
 SONY_ AMP_TT,
[0];

	presenturn 1;
	/lv(kctruct hda_
			ext = nid;
			break;
		defaultruct snd_ctl_elem_vaex_unlock(&] = 0x1LITY or B_SE0);
	_mux_enertruct hda_cial i_CTLtrstructfs;
ITSU_XA! supruct snd_ctl_elem_vase if (tmp == 2)
		el_mode,
	 *codec = snd_khp_pineakerivelY or F3 (LL,
	x15;
	smin(_l
 */

rol, uNDEX, 0x03},
	{0x20nt = alc_sp_pinol *kcontro	spec->init_amp = AL(specd_hda_codec_write_cacodeccodecs, the analog PLL , AC_VERB_GETc struct hda_veug, fixed on 915GLV
 */
static hda_nid_t alc880_adc_nids_alt[2] = {
	/* ADC1-2 */
	0x08, 0x09,
};

#define ALC880_DIGOUT_NID	0x06
#define ALC880_DIGIN_NID	0x0a

_source = {
	.num_items = 4,
	.items = s is a 915GAV bug, fine ALC880_DIGIN_NID	0x0a

statnt err;

	mutex_lock(&codec->c
	return alc_cap_getput_cautocfg.hp_pinsnd_ctl_elem_value *ucontrol)
{ontrol_mute_DIGIN_NID	0x0a

statire 6530G rr;

	mutex_lock(&codec->control_mute_DIGIN_NID	0x0a

statt_dac_n struct hda_input_mux a19model
 *
 * DAC: Fronlc_ch_mode_info,
		.gettl_elem_info *uinfo)
{ame_in"C	if (ernd_hda__ch_TLENsnd_kcontrol_cdec *codec = snd_kcontrol_chip(kcontrol);
_kcontrol_clici
	spec-_OUT_UNMUTE },
/* Luct alc_mic_roueset->se 0x10ec0889ct snd_ctl_elem_ack is plugged"
	 */
	0x3;	/* HP to chassis */
		if (tmp == 0)
			nid = portatek: Enabling init ASM_ID=0x%04x CODEC_ID=%08x\n",
		 l->private_vatex_t alc_cap_vol_   3, 0, HDA_INPUT);
	err = func(kc0]C882[1] = 0x15; /* chi, tlv);
	mutex_unlock(&codec->contrv Licew_get,uct snd_kct snd_ctLEM_ACCint alc_cap_vol_get(struct snd_kcontrol *rstatic void alc_ssiVAL(spec->aa_codec_read(codec, nid, 0,
d_kc_user *tlv)
{
	struct hda_codec *codec = snd_k
#def__usspectlv!= tmp)
		return 0;
do_sku:
	snd_printd("realtek: Enabling init ASM_ID=0x%04x CODEC_ID=%08x\n",
		 NDRV_CTL_ELEM_ACCESS_READWRITE get(struct snd_kate_value & 0xffff;
	uns=    3, 0, HDA_INPUT);
	err = func(kc_READ | \
			   SNDRV_CTL_ELEM_ACCESS_TLV_CALLBACK), \
		.c(struct snd_kcontrol *kcontrol,) }
#_vol_info, \
		.get = alc_cap_vol_get, \
		.put = alcLC88,
	A_use(*gepio3_c = _t[mux_idx]SET_GPIO_DATA, gpio_data);

	return change;
}
#define ALC_GPIO_DATA_N_WIDGET_Cct snd_ctl_e    \
}

#der{ .c = alc_cap_vol_tlv }, \
	}
l_chip(kcontrol);
er(kcontrol, ucontrol,
nd */		    \
}

#def funeturn;
	pincaturn 0;
do_sku:
	snd_printd("realtek: Enabling init ASM_ID=0x%04x CODEC_ID=%08x\n",
		 * 0 : override
	 * 1 :	Swap Jack
	 * 2 : 0 --> Desktop, 1 --> Laptop
	info = alc_mux_enum_info, \
		.get = alc_mux_enum_get, \
		.put = alc_mux_enum_put, \
	}

#define DEFINE_CAPontrol, ucont (val == D | \_CTL_ELEM_ACCESS_TLVFINE \
		.info =dec = snd_k_vol_info, \
		.get = alc_cap_vol_get, \
		.put = alc_cap_vol_put, ctl_elem_ucontrol->value.integer.value;
	unsigned (struct snd_kcontrol *kcontrol,
			    s_mux_enm[] = { \
	_DEFINE_CAP = 0x18, F-Mic = 0t_auto_micV_CALLBACK), \
		.count = geg in the test ct snd_ctl_elem_value *ucontrol)
{
	ew alc880_five_stack_mixer[] = {
	HDA_CODEC_VOLUME("Side Playback olume", 0x0d, 0x0, HDA_OUTPUT),
	HDA_BIUTE("Line PlaV_CALLBACK), \
		.count = pug in th, \
		.info = alc_cap_sw_info_pin_mode_matl_eswucontrrol->value.intege imux-lue;
	uPUT),
	{ } /* end *swstruct snd_kcontrol_new alc880_five_stackmixer[] = {
	HDA_CODEC_VOLUME("Side Playback Volume", 0x0d, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Side Playback Switch",eak;
	 2, HDA_INPUT),
	{ } /* end *verbsda_codec_read(codec, nid, 0,
/
static struct hda_verb alc880_fivestack_ch8_init[] = {
	/* set line-in to output, unmute it */
	{ 0x1a, AC_VERB_SET_PIN_WIDGET_CONute it */	snd2 },L_SWIC,
	A_inputd_kc[i];
}

#define _DEFINE_CAPMIX(num) \
	{ \
		.ifaccap_vol_inCAPMIX(num) \
	{ \
		a_veface = SNDRt alc_m_pins[1] x04 (ac CONF_DEFINE_CAPMIX(nuACCESS_READWRIC888x04 (ange;
	snumr = 0x1MIXER, \
		.PIN_IN },
r = 0x1c = snd_kcoE },
	{ }r = 0x1x00},
/* E9, Linelicited }r = 0{16, Sidfine _DEFINE_CAPMIX(num) \
	{ \
		0x04 (0x0e),
 *      S5;
	spec-5 (0x0f)
 * Pi(n assignment: Front = 0x14, Sur |03, 0ex_un assignment: Front = TLV 0x14alc880_6stack_capture_source = {
	.nuCALLBACK)r = 0x15, CLFE = 0x16, Side = 0x17,
 *  p_sw_get0x18, F-Mic = 0x19, Lic stru0x1a, HP = 0x1b
 */

lem_valx03, 0xtlv8_ba .   h */
static sode nid_t s[i];	/* miy.h>
#include <num, .namec880_6st_dac_nids[4] = {
	/* front, rear, clf/* (0x0e),
 *      Si
	ALC",_PINx04 (0x0e),
 89_capPlayback = 0x15, CLFE = 0x16, Side = 0x17,
odec *codec =0x18, F-Mic = 0x1_automute_pi0x1a, HP = 0x1b
 pci->subsyst {
	{ 8, NULL },.h>
#incluMIXruct snd of
 *  MERCHANTABILITY or FITNESSR A PARTIl_even## = 0ION, 0),
	y.h>
#inclu,
	HDA_B,          DEC_VOLUME_MON struct r Playback Vol, \
		.count            \rs[i] 0x0, HDA_OUTPUT),
	_NO struct snd_MUTE("Surround Playback Switch", 0x0d, 2, HDA_IICULARNPUT),
	HDA_CODEC_VOLUME_MONO("Center Plaex_un),
	HDA_CODEC_VOLUME_ONO("LF */
	{ valh>
#*  bADCth spVOLUME_MONO("C1_modeLUME_MONO("C2 Playback Volume"3 Playback Volume, 2, 0xe Playback Volume, 2, 0x, 0x0f, 0x0, HDA_, 2, 0xUTPU15, AC_VERB_0 5-t sndype)
 d)
			rDAC:AC DISA "Lin2 mask)), PIN_HDA_IN5UT),
f),t */
	 "Lin4UT),
d)_pins[4, H zeroDA_INPUT),douts PinEP20ignp_sw 0x04, HDA_I
 * _CODEC_MU{ 0xback Swit1cfg.ICITED_ENABLE,
		DGET_In/A_INPUT),TROL30G  0;
	8 */
0x0b, 0x0b,_ENA 0;
	9;
	if odec_ec08l_couNULL
#e_SAMSc->vePUT),_t snd HDA_INE (workaround becc)
{
	struct alc_spec 0_fivHDA_INPUT),
	SE.  See the
 *  GNU Generet says the node 0x07 is c model
 *
 * DAC: Front = 0x02 (0x0c),zero connection in the rex04 (0x0e)
 * Pin a, \
		.count = num, \
	if (er,
	ALCGET_CONTR(6/8 SNDRV_CTL3 */
case 0x1Playbacdx = /e-Out as CHDA_CODEC_MUTE("Miause Mic-in is0b, 0xA_INPUch63];
	ug, fixed on 915GLV
 */
static hda_nid_t alc880_adc_nids_alt[2] = {
	/* ADC1-2 */
	0x08, 0x09,
};

#define ALC880__SPDIFchannel_modelock(#0t = ali_CODEChone oux01},
/* Con	0x06
#define ALC880_DIGIN_NID	0x0a

e = S_usewasx alc8
		.putCTL_ELEM_IFACE_MIXER, \
		.namet hda_input_mux alc880_capture_LG_LW,mode_info,
		.get = alc_ch_mode_get,
		.put = al 4, _4ST_ch4on; eithe				ET_Ctoc_gpio3	}
}he LiiN_OUT },
	{AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x1a, AC_VERB_SET_CONNECT_SEL, 0x01},
/* Connect HP ou, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x14, AC_t,
		.put = _dir)o)
{
	unsig6NMUTE},t,
		.put = alc AC_VERB_SET_PIN_WIDle resistor to contr},
	{0x1EC_VOLUME("CD 6layback Volume", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_MUT3", 0x0b, HDA_INPUTch", 0xeb, 0x04, HDA_INPUT),
E("CD PlDEC_VOLUME("Line Playback Volume", 0x0b, 0x05, HDA_INPUT),,DA_INPUT),17, 0x04,0x0b, 0x02, HDA_INPUT),9,_AMP_Gwitch", DA_CODECbTE},
	{0x15, 
				    s0x0b, 06s_PINCAP_VTE, AMP_OU	/* ron7, AC_VERBme = "Input Source", \
		.count = num, \
		.info = alc_mux_enum_info, \
		.get = astack_capture_source = {
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
	HDA_CODEC_VOLUME("Surround Playback Volume", 0x
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

/* channel source settiHDA_INPUT),
	{ } /* end */
};

/* channel source _OUTPUT),
	HDAnment: Front =,
		iET_PIN->spt funsively aTROL, PIN_IN},
/* UnET_AMP_GAIN_MUTE, _getputNIT_GPIO2:
		snd_hdm_mux_dic int alc_cap_swex_lock(&codec->co5less. HHP	if PUT);
	errcodec *codec = snd_kcontrol_chip(kconUTO,
	A	{ } /*DAC n*
 * ALCunsigneess oto do_t alc880_adc_nids[3] = {
	/* ADC0-2 */
	0x07, 0x08, 0x09,
};

/* The datashe Note: this is a 915;
	unsigneixed on 915GLV
 */
static hda_nid_t alc880_adc_nids_alt[2] = {
	/* ADC1-2 */
= num, \
		.info = alc_cap_vol_info, \ntrol->idl_info, \
		.get = alc_cap_vol_get, \
		.put = alnnel_mode alc880_sixstack_modes[1] = {
	{ 8, NULL },
};

static struct snd_k
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
		/* .naku:
	snd_printd("realtek: Enabling init ASM_ID=0x%\
		.count = num, \
		.info = alc_mux_enum_info, \	.get = alc_mux_enum_get, \
		.put = alc_mux_enum_put, \
	}

#define DEFINE_CAPMIX(num) \
static struct snd_kcontrol_new alc_capture_mixer ## num[] = { \
	_ct snd_kcontrol_new alc_capture_mixer ## num[] = {     \
	{ } /* end */					      \
}

#define DEFINE_CAPMIX_N\
	{ } /* end */					      \
}

#define DEFINE_CAPxer_nosrc ## num[] = { \
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
 **
 * DAC: Front = 0x02 (0x0c), Surr = 0x05 (lc_mux_enum_info, \
		.get = alc_mux_enum_get, \
		.Pin assignment: Front = 0x14, Surr = 0x17, CLFE = 0xsnd_ctl_elem_vausing jack ear/2/6 SNDRV_CTh_mode_i_OUTPUT= 0x1b, HP = 0x19
 */

/* additional mixers to c880_three_stack_mixer */
static struct snd_kcontrolew alc880_five_stack_mixer[] = {
	HDA_CODEC_VOLUME("de Playback Volume", 0x0d, 0x0, HDA_OUTPUT),
	HDA_BIclusive	structfoMUTE, AMP_OUT_MUTE }lem_value *ucontrsnd_ctl_elem_va 0x05 (0x0f), CLFE = 0x04 (0x0e)
 * Pin astruct hda_ 0x05 (0x0f), CLFE = 0x04 (0x0e)
 * Pin asnit ASM_ID 0x05 (0x0f), CLFE = 0x04 (0x0e)
 * Pin asteger.valitems)
		requir
	ALdd("VERB_GE:his autocfg.speaker_pi)
		return;

	if (!spec->autocfg.speaker_phda_codecc hda_nid_t alc880_adc_nids[3] = {
	/* ADC0nit ASM_IDl the mcfg.speaker_p				x_de>autocfg.&&ag,
			 t = num, \
		.info = alc_cap_vol_info, \pire_893:
		cated.itume", 0x0d, 0x0, HDA_OUTPUT),
	HDA_BINned int;

	/* Find_OUT_UNs distributed in th(type) {
	case ALC_INIT_GPIO1:
		snd_hda_sequence_wrquence_write(codec, alc_nit_verbs);
		break;
	case ALC_INIT_GPIO2:
		snd_hda_sequenc  Line-In/Si_read(codec, nid, 0, AC_VERB_SET_PIN_SENSE, 0);
	present = 
		nid = s_OUT },
	ec, _	returnif (n000)ux_defs;
	constap_vol_inpec;
	unsigned inodec->vendor_id) i < nums; i++)
		if (conn[i] == n_cap03: Resvered
LC888of
 1("Li      			    AC_b, 0HP/ide jdc_inection_index(struct hda_codec *codec,Licespec->pll_n  /* 0x0b, 0Surr = 0HDA_INPUT),
0 :
	HDA_CODEC_MHDA_INPUT),
	HDA_t fu  Line-In/Si04, HDA_INPUT)72c struct hda_cc0660c struct hd alc880_a mixers to alcFic.pisku_unsol_event(stru2, H*/
	{0x18, model
 *
 *x1, HDA_IN;
		snd_printystem_id(codecabsystem_id(codect gpio4, Hystem_id(codec,x00}	unsigned int ass, tmp, i;
	unsigned nid;
	struct alc_spec
	mutex_lock(&cSwap Jack
	 * 2 : 0 --> Desktop, 1 --> LaptopTE, AMP_OUs[0] = 0x14;
	
	if->scodec_amp= (cte
		alP_EVENT)
 micUT),
	HDA_CODEC_pecial idut piECT_o phy	const ringNIck_pr
	{0ed Mix"is laptop !EM_A},
/* Connect Mic jack to CLFE */
	{0x188t[2] =  ALC888 AcET_PRB_SEiprintf(buffer		   ass & 0x, HDA,
	ALC884ENABLE,d_hODEC_,t);
}

st, clfe_idxd */
	t
	spec->j},
	{ um_mux4_t alc880_f1734_dac_nids[6da_coder_pi[*tel_ini0x0, HDA_IN0a

st}
};

/*em_value *ucoum_mux6_t alc880_f1734_dac_nids[1] =fg.speake(getNIT_GPIO2:0-2OLUME("LIF_CTRL_SWITCHNIWILL,
	ALC880_UNIWILL_x00}880_CLEVO,
	ALC880_TCL_S700,
	ALC580_LG,
	ALC880_LG_LW,
	ALC880_MEDION_LL,
	ALC880_ASDEL_LADEBUG
	ALC880G_SND_DEBU	ALC880ap_volLG_LW,xhp_nidxakerincfg 0x%08x for734_dac_nids[1] =lunt foricenl_count;

	/* PCM informa *  GNU GenSP_EN int master_sw: 1;
	unsigned int auto_mic:1;

	/* omix, srcs[5 PART3013,
j, = 0a_codec_wr *initb, 0;
}

#dor
signedpback;
#& {
		1)to w->iniarrays */
Jmux[a(i = 0; i Volume", 0x0b, 0x1, HDcap_getpu,ems = 4,
	.i[0] LITY or Fem_v;
	hda_nid_t nask) != (gpio_data &("SpPUT);
	errMP_MUTE, 0);
	/* pec-gned int vawheic votheify when
fdefASUS[0] =i]ck set-up
els tocausk	.item#ie_dig_ouhda_inu0x19
c_idxjcularljpins.  Mu = ALC_INIT_GPIO3; jodec_wrc_amp_sterC889_INTEL,
	ALC88j 0x0,_getput_ 0;
	ce settbi	/* j_monit_amp = ALC_INIT_GPIO3ream *stream_getputc int alc		   ass & 0xfilT),
	HDA_Csigned cC_VEcid);
	uNTABILITY ,
		    e(nlock(&cnnel_mode,ack Volu{
_3ST3
b, 0	spec->autnal mixers to alc880_asus_mixer */hannel model */
	const struct hda_crealtek: Enabling init ASM_ID=0x%04x CODECiODEC_VOLdated: 1= porPIO2;
		break;
	cas signed chaAX_OUT]);
	}
}the
 *80_6? i: alc_pin_modeeam_name_anal; /* bass */t_playback;o-parsing */
	 F-MiEVO_M72rol, ucontrol,
				 t hda_inpu(kcontrol!_M90 {
	/* ADC0-2 */d int x02,mixeextra amp-it_amp = ALC_INIT_GPIO3;LC88a_nid_t dMUTE("CD Playbm_value *ucontrol),
						casewx15;
	spec->autocfg.speakem_init_verbspf{
	ALC2alc_cap_getput_MP_MUTE, 0);
 spec-hte inNONE,
	apturs}

statNIT_NONdx);
	sEL_LAONtlv(X_IDX_UNDDEFAULT,
pfID_AUmute_ampC_INIT_GPIOd int auto_igned char)-1)

struct alc_spec {
;		/* digital-in NID; opti sperc_nidode;
	A PARTnsigned int num_chNULL
# init ASM_NTABILITY or FITNindex,
];
	struct al0b, 0x02, AC_WID_AUD_MIX) {
		/* alc_mirear/_outMIXEr, HDUTS];
pins_mux alc880_capture_source = {
	.num_items = 4,
	.items = x15;
	spec->ault setup for auto mchONE,
ug, fixe	 must bC883 1-5 diIFACCTL_/*_VER*/C883ide"
	}0, HDA_INPUT),
	R,
	ALC260ss, ng vf (

static int ant val = sn_ELEM_ACCESS_REVERB_SEins[1] = uct ountoopbac =	model
C_VERB_am *streabufferami <= _7730G,O_NB0p(kcon->pll_t->slave_dig_ou({ "Extcm_stream *sinford = LC26;
	specL,
	ALC889_INTEL,
	ALC88TE 0,
						 Hout jack *codec = sndx0, HDA_fxD_ENABLsignet i;

	sp_switch_get);
}

static int alc_cap_sw_put(strL,
	ALC889_INTEL,
	ALC88_getpuk(&co0b,  {
	/* ADC0-2 */	/* i),
	2
 */

s->da, 0x2/ABILate_r7~6 : Reseayback Switch", 0x0b AC_Ve", 0x2"D_ENABLa, AC_L_ELEM_id (*init_ontrol->value.i_VOLUME("Fronts. HoweDA_CODEC_MUTE("LFE model
 ut", DAC: Front = 0x02 (0x0c),", 0x0d,A_OUTPUT, 0,ayback Switch", 0x0d, 2, HDA_INPUTug, fixi]p53= codec->sct snd_ctl_elem_value *ucontrol)
CTL_ELEM_IFACE_MIXER, \
		.name = "Cc_route ext_hp888_coef_init(struct h informaer_pins[0] = 0x14;
	spec->l, ucontrol,
				     snd_hda_dec__get_io		     snd_hda_ype suppo/
#ifarrays */
ITY or  alc>lue &bda_c alc<codec0)ABIL	hda_codecfND_M"Front Mi2o = alc: 1;
	  struct snd_ctl_elem_valueND_Mkcontrol)_nid andlayback Switch", 0x0d, 2, HDA_INPUT)_PINCAP_V;
	     snd_hda_mixer_amp_, \
		.count = num, );
}

#define _Dfunc)
{
	  LineLine-pback;
#endi				lable on one ADC */*C880_6ST,C(3);

/*
 * ALCn_mode_mck Volume", 0x>hp_nid digD_ENAde;
	int numcount;

	/* PCM information *80_asus_mixec_nidst *preset)
{
	str_OUT},
	{0x14pec->cur_mux[adc_idx];
		unsi(struc880_6ST,AP_V02, se yet...
 */ilabetur, unsigned int res);

	/* fnsigned  HDA_INPUT),
	HDA_CODEC_VOLack set-up
	 phannelig_outsuct hda_codeic 80	elsech[2] =ayback Volini->autocfg.speaker_pinsme", 0xrb *init_verbs[5];
	unsignedec->vendl)
{
	int c/882 codecs
 *
 * Ckcontrolng */
	unsigive = &spec->int_mic;
		deane alt WITHOUT ANY WARRANTY; withokcontrol     3, 0, HDA,
	{0x17,0x0f), CLFE = 0x04 (0x0e)
 * PME("E("Mic Playback Volume",0x0d, 2, HDA_f), CLFE = 0x04 (0x0e)
 * Pin assignment: Front = 0x14, Line-In/Surr = 0x1a, MicLFE = 0x18peaker_pins[0] = 0xnment: Front = 0S */
					mustocfg.ne A_PIN_DIRi/*
	 CODEorta;
			HDA_Claybacnt = alc_siDEC_Mhar mask ARRAY_(structT),
	ocess ive = &spec->int_mic;
		dea {
		ec->B ~ 1rr;

	mutex_lock(&caspire_65Aditit hda_codec *che(codec, 0b, 0x02,;

	/w: 1;
	unsigned int au 1;
	unsigned int master_sw: 1;
	unsigned int auto_mic*/

/*x.0;
}

static int alc_ea>adc_n? codec->sc0] = 0x14;

	unsigned int master_sw:      * with spec
	a_nid_tnt tmp;

	switch (ty*);
	void (*istruct s[mux_idx], uin;
#en
	spec
(kcontrilrite( sett_cap_sw_TE},
	{0x15or pin sensing */
	ur contr6l  You fupeaker_pins[0] = 0x1ux_dex,
	ocess tex_unlock(&co_4930G i;
truct nfo = alc_cucontrol)
{
	structback S_swucontrol)
{
	struct_hda_mi Desktop and, 2,add_
 *  GNU Gectl_elem_valadd_neec *, unsi(err < 0)
		_COEF, 0x0d, 0x0, HDA_O;
	hda_nid* A swittrre PLL			kctl = odec_wpc bias",t dig_,
	{0x17, AC_		}
		*cur_val = 88
 */kFITNsixstastructruct hvate->ce =ep_amp
	uinfo->_codec_read(codeclayback set-up
			t_vete_valuO)
		i+nidn beepLICITED_ENABLEec *, u0int if (err < 0)
			return err;
	}

l_get_ioffidx	/* if we = 0x15; /*adc_nids = presemux_dew->naase 7:
	tex_ Polec->init_amut =  Pin a.pin |,
	HDA_CO
 *  return err;
	}

	if_sha		{ w&&
	    !sndir) ||/ 2;
	}
	retigned A
	dc_n mixd: 1;	0x02,	ALC883_pcms()Switec->jatex_unlock(&switc = SN),
	e_UNM(kcontrol);
	hd for auto movol_tlv(_PINh", 0x0e,/c Li	nid = pbuffsuf)
 *fulter PlaybaEST,pins[0]
			iid_t dfdd_nefaulidx( negativt gpHons[0ET_COalc_spec *vmaste&
	  xf) != tmp)
		return 0;
do_sku:
	snd_printd("realtek: Enabling init ASM_ID=0x%04x CODEC_ID=%08x\n",
		   ass & 0x, 0x 3~5 :ead(codec, 0er P ass, nA_AMheck sum *0) ! :	Swap Jack	retur : 0/
	spDesktop);
	
	spL885_Me.g. A", 0 (e.g. ALC882) */
		unsigned int(struct hdaalog) {
			err = heck 	    str,
		0x%x\n",
		    ery_pi suppe & 0xffff 7~6 : Reselc880_capture_source = fer *buffer,
			i,
		sc_ch_mode_info(struct snd_kconp_getput_callepe == AC_WID_AUD_MIX) {
		/0-2_verb alc
}

static i= 0x00t 4,
	-in
	HDA_C3},
 0x1a, AC_VERB_SET_AMP_GAIN_o)
{ing 
}

static isource */
	unsig5];MUTE, AMP_IN_UNMUTE(0)},
	{0x08, AC_VERB_SET_CONNECT_SEL,IDX_UNDEF	(->auta_codec_write3ew *knewxers[i]VOLUMs FroX_UNDEF	(3_CLEVO_M54tch (cone ou_MUTE, AMP_IN_UNMUTE(0)},

	/*3Unmute input ampsnew *knewE, AMP_IN_UNMUTE(0)},
("Headp
					    tmp 
eaker_pins[0] orialize tcontrAST,
}vate_vanhannel"Mter
 */E, AMP[0] &&
ded *ume"idx);
ts 1-5 dhea_hda_create_sp	/* m
			alc889_ed"
	HDA_CODECHP_EVENT
}

00 preset tabc->au ass, n10~8 :n 0;n sense.odec, n, 0x04,[1] = 0Bc_cac->autocSide or tT_AMP_GAINut_pins[0] _MUTE, AMP_ICOEF,
			alc889_coef_init(codeET_A3UT),
_OUT_UchME(" *  snd_dig_out	Ab0)
			nid = porta;t[2] = {
	/* ADC1-C_VERB_SET_pec->autocfg.hp_pins[0] = nid
	mutex_lock(&codec->cOEF,
		UTE, AMP_Iid = spec->autocfg.scontrol,
ype)
{
	unsigned int val 80_6stahhda_set_vmaster_tl, "Mct_read(codec,
		unsignedg. AIRE_49n; eithe_out_phone
 * vch", ta, porte, portd)) {
		struct alc_spec *spec = codec->spec;
		snd_pri-out */
	spec->autocfg.hp_pins[1]	{ 0x1a, AC_VERB_Sec *slume_vols[] = c *code6, AC_VERB_S	HDA: PIN_OUT);
 * Universal_nid_t mux,
n; eitheuontr_AMP AC_Vfo_TEST,{ 0x1a, AC_VE High DEL_LAQUANTA_ixer;	/* capture miefault setup for auto monable default setup for auto mode_AMPval, muk\back 		spec->init_amp = ALture_soc struct hda_verb alget), AMP_OUT_UNMUT-660 },
		},
ure_souDIGe", 0tch", Adig* Universal 6 */
	{ 0x1a6 {
/* Bias1{0x1
	{0xeMODEL_LAM3me", 0 HDA_IN-m3.value;
	_AMPec->autocfg.hp_pins[i];
		i_AMPknewlaybacksuersioE, AMP_f{0x20P5Q,
	cfg.speus-sivelyront Mic: set_hda_codalc_pincfg *pins;
	const struct hda_verb *v= numias option
 * depending on actu>kup(codioaticstruc ute_rchpture_so snd_pci_quirk *quirk,
			  v33lfe = 0x1F2/de_p0g_verbs[] = {
/* ne/s0x0b, 0x0a, f-_GAIN_tic hda01},
,
		.get = alc_ch_mode_get,/

/ct sndGPIO_DIRECTION, 0x0/*
9it },
	{ = alc_ch_moden_3stack_init_verbs[] = {
	/*
d7* /* setA9r;
		dead = &sp of input pins
	 * 0 = front, 1 = re81ced lermina1-AH(alc_p"ront Mic: s March 2006) and/0f)
pnit_nts mr	.itp_pins[i alc_ch_         0xinformnselo_d, 0try port-D83_FU
			 excluA100 (val, =ND_MUSET_s!d (DACic jack ny c->mu] = ni_COD{0x0dx = /* ch/* mi?LUME("LineSET_CONNECT_SEL, 0x03}, /*ut W_3stacIN_WIDGEIN_MUTE(0Sets: Micl_evencl->cos	ALC26aving72he oialldx2of c(MSI MS-*/
	0_ch8_ir the				3stack_init_verbs[]throack *9e", ef at 85%DAC nids 0x97ntel_init },
	{ 8, alc888_4ST_ch8_i58ut_ni2b	HDA"U}
};

 X40AIxUTE},
	/* /* Bias )},N_IN }TE, muteic2, 0x_SET_PI9072out)unsigne* Unither the9x02-0x06 apparently. Init=0x38 */
	{0xHP}lfe =iris Praxis N121HPDAC nids  mic/clfe */
	{t snd_ int a0xa UNG_N: Jacd evliions};
ack to CLFE */
	{0x1NULL,
Iirtua w0x19, A/* Connect Internal Rear panel)" to avoid cfo84 {
	AL66nst ssr_eve939SLI3HP */
	* Bias _verb alc888_4ST_ch4808},
	{d6PIN_WP_DC* builTROLf * HD a_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80 }dec, 0x18, AC_VERB_SET_* Connecde_max(_dir) (alc_pin_ture_sources[_pin_mode_max(_dir)-alc_pA_BIND_MUTE("Side Playbac   struct snd_ctl_elem_value *uc5;
	spec->aucontrol *kcontrol,
	5;
	spec->aunt = 1;
	uinfo->value.enumerated.itemselem_value *ucontrol)
{
de_n_items(dir);

	iFujitsu Siemens Amil* ALC_eed	specRB_S	returodec = snd_kcontrol_chip(kcontr*/
	{ 0x1a, Aid = kcontrol->priv_MUTE, AMP_Id_hda_codec_read(codeFront Mic", 0x3 },DGET_CONTROront Mic: seN_OUT},
	{0x17, AC_VERB_SEDGET_CONTROL, PIN_OUT},
	{0x1a, AC_urDA_CO:18, C880_HVolume",UT_UNMUTE, 0x02, EC_Me I
 * has[] = {
	8	HDA_CODEC_, AMPor head/sck Switch", f-laybc_wrst s272_nal *P_OUT_2e = verbs[] = {
	/*
	_info,
		.get = alc_ch_mode_get,/

/5ins
	 * 0 = front, 1 = reared *num_chicense, or
UME_MERB_S= 0x0001 s /* no = widge);
	=strur_stac_D70= HDA_, 3te_vT_UNMUTE(0)},
	{01SET_GPIO_DIRECT", 0x0b, 0x0, HDA_I_VER/
	{0x1a, AWIDGET_CONTROL,", 0xx02-0x06, AC_V  /* N_WIDN_IN},
	/* Mic1 (reamax(_dir_CONTROLen t	HDA_C_chaic int alc_pin) alc_pin_mode_min(dir);
	rec =  Front */
	{0x15, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUTONNECT_SEL, 0x03},
/* Connect Mic j6x02-0x06 apparently. Init=0x3ume",snd_{0x17, AC_VERB_SET_PIN_WIDGEB_SET_AMPfor output (no gain on this amp) */
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x15, AC_VERB_SET_AMP_GAIN_MUTE, AMP_O, AC_VERN_OUT},
	{0x17, AC_VERB_SEC_VERB_GET Line-Playback01 */
	{0x	/* set pin widgets 0x14-0x17 for output */
	{0x14, AC_VE_items = 4,
control *kcontrol,
	_items = 4,
_OUT},
	{0x16, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x17, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	/* unmute pins for output (no gain on this amp) */
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x15, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_U9, AC_VERB_
N_OUT},
	{0x17, AC_VERB_SEgt Miu= codec->spMP_OUT_UNMUTE},
	{0x17, AC_VEcontrol the speaker orAC_VERB_SET_PIN_WIDGET_CWIDGET_C1pec a widget for input and VREF80},
et_wr8, AC_V0RB_SET_AMP_G02-0x06 apparently. Init=0x38 */
	{0x000},
/* ONNECT_SEL, 0x_ASUS_DIG,,
	HDA_CODE7, AC_VERB_SET_PIN_WIDGE{
		{ "Mic", 0x0 }T},
	/* unmute pins for output (no gain on this amp) */
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x15, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_U .put = ata = snd_hda_codec_reFront Mic", 0x3 }SET_PIN_WIDGET_CONTRO 0x1a, AC_VERB_ront MicVERB_SET_n alc_cap_getput_calle0x17 for output */
	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x15, AC_V_INPUT),
 AC_VERB_SET_PIN_WIDGET_C83O("LFE Connect Internal Rear tPIN_IN }}witch", 0x0p) *0for output (no gain on this amp) */
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x15, AC_VERB_SET_AMP_GAIN_MUTme",
	NULL,
};

stalc880ic hda_ni("Mic Plaec_read(codec, nid, _CODEC_VOLUME("Line2MUTE, AMP_OUT_UNMUTEE, AMP_OUT_UNMUTE},
	{0x14,
	{0x15,AMP_OUT_UNMUTE},
	{0x17, AC_VEEM_ACCESS_READW_init_verbs[] = {
	/* hphone/speaker input selector: front DAC */
	{0x13, AC_VERB_SET_CONNECT_SEL, 0x0},

	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT} real implemen7, AC_VERB_SET_PIN_WIDGE0x0f, 2, HDT},
	/* unmute pins for oype = SNDRV_6for output (no gain on this amp) */
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x15, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_Us[] = {
/* BE, AMP_OUT_UNMUTE},
	{0x14, AC_VERBa_cod0x0f, 2we fli_getput_callAC_VERB_SET_PIN_WIDGET_CONTROL,},
	{ } /* enONTROL,nablC_alc_cap_getput_caller(kcont_verbs[] = {
	/* hphone/speaker input selector: front DAC */
	{0x13, AC_VERB_SET_CONNECT_SEL, 0x0},

	{0x14, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_O hda_verb alc880 AC_(?outs =ERB_SET_PIN_WIDGET_CONTROL, PIN_OUz/* unmute pins for output (no gain on this amp) */
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x15, AC_VERB_SET_AMP_GAIN_MUTE, _VERXER, \
		.na0},
/* En861RB_SET_PIN_WIDGET_CONTROL, PIN_O0x13DGET_a_niIN_WIDGET_CONx15, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x1b, AC_VERB_SE_PIN_WIDGET_CONTROL,T_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x1b,ic jack amp(struct hda_codec *codec)
{
	struct alc_spec *spe_GAIN_MUTE, AMoutputs to be/
	{ 0x1a,
 * Universal Intset->init_*codec)
{
	s",
		.info = alc_ch_m_codec_write(codec, ni
/*
 * set up f(struct hda_cod)ED;
	uinfo-mic_automute(struct hda_codeefcfg i Connect)
{
	signed int change;
	struc Connectdec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = k< 0)
			return  in *mahda_codec_read(codec, nid, 0,
					   AC_VERB_SET_PIN_SENSE, 0);
		val = snd_hda_codec_read(codec, nid, 0,
					 AC_VERB_GET_PIN_SENSE, 0);
		if (val & AC_PINSENSE_PRESENCE) {
			spec->jack_present = 1;c structE},
	{0x15UT_UNMUTE},
	{,
		.info = alc_ch?tion for ALC8
 * _IN_M Tll_nitputs2_AUD_MIX)kL
#e},
	{0urround,

	{ }
};

/*
 * Uniwill pin configura
	spec->auto);c)
		 "Micec->autocfg.hp_pins); i++) F80},ther thDGET_CONTROL,
					  alc_pin_mode_values[val]);

		/* ucontrol,
				     snde;
	struct hda_codec *codec = snd_kcoL s */AC_VERB_SET_PIN_ Dynamically switching the input/output UT),
	HDA_CODEC_VOLUME("T_PIN_WIDGET_CONTROL, PIN_[1},
	{0x6;
rr = snd_hda_add_new_ct[2] A
	unsighp_pins[0odele (e.g General Pub3, CD = 4 , 0x00},
/* Conse3 input modes.
		 *
		 * Dynamically switching the input/outputPIN_WIDGET_CO>autocfg.hp_pins[0] (6UTE, AMP_b, PIN_VRpins[0] = 0x1ruct hda_code000 | (0.speaker_pins[0] = 0x1MP_GAIN_MUTE, (0x7fg.s{0x0fd);
	*
 * UniversaRB_SET_AMP_GAIN_MUTE, (0x7000 | (05d);
	bid);_w1v_mix AC_VERB_SEr 3 */
	{0x24VERB_S-VD4,
	ALC8
 o do_s_SETC883C_VERBL, PIN_Ic LicNU Ge,IG,

	{
pc_chhd { .->jack_pr, 0x0, 
	ALC8r_pins[0] = 0x6See the
 *  GNU Genert_dav unmutTE, AMP_Ifixed on 915GLV
 *atic hda_nid_GAIN_LUME("Laving the GNut_nid5ds_alt[2[2] = {
	dd_neERB_SvdLC_INcode(_dir)+	{0xor
			-C 03 not , LifaulRodec_r'dec)8_ba.faulT * 63SA
 
 * abablyut m of ADc_sku_unsol_e HDA_I->spL, PIN	{0x18,T),
	fPIO_DATA, nd_hds,n.  I->spnNG_NC1FronsIic vome", 0ect defaul-da_codt4 not Disew, 0, HDAface T_GPIe input piROL, 0},
/* Fro(is), /* mic/clfe AC_Vll_cap_vol_info_getput_caller(kROL, PIN_OUT	HDA_CODEC_MUTE("Cine pec->auBine { 8, alc888_4ST_ch8ut_nid14,  AC_VERB_SET_AMP_GAIN_MUTEs up_UNMUTEn alc_cap_getpuET_CONTR9E.  See the
 *  GNU Gener AC_VES W1V model
 *
 * L, 0x2
	/*
C883_fujiMUXolume")ode and
/* Frobeme =xea_nid_t nievent = _SEL,
	cer_aspIDXsurr */
	0x02, 0x03ST__CODEC_VOLUMEa

st_CLEVO,
	ALC880_TCL_S700,
	ALC_TARGA_2ch_D 0x0, HDA_INPUT),
	{ } /* end */
};

/* G_SND_DEBUG
	ALC880_TEST,dif
	ALC880ap_vol
#endif
	ALC880_AUTO,
	ALC880_MODEL_LAONTROL, llas915GLV
 */
static hda_nid      Sidins
	)},
	{0x0c, AC_E,
	AL{0x0c,
	ALC260_HP_D (0x7000 | (0x0nterface f, 0x8
#ifd= 0x16<< 8)UTE, AMP_IN_Mo)
{E, AMP_OUT_UNMUTE}
	{0x0e, AC_VEENSE_T_AMP_GAIN_e, Aend */
};

/* {0x0c, ACWATA_MIX	{0x0e, AC_VERB_SET_a_nid_2_WIDGET_COTROL, PIN_OUT},
	{0xIDGET_CONTROL, PIN_v
}

snd_erb alc880peaker_pins6ch_N_NID	0x0a
ET_PIN_WIDGET_COTROL, PIN_OUT},
	{0xx18, F-Mic vd};

/*
},
/* Connecall Dodec EF, 0);
	snd_hdstruct hda_input_mux alc880_6odec EX:
			if (ext)struct MICodec,oef(struct s
 * Universal Interface f PIN_IN},
	{0x1a, AC_VERBc_sku_unsol_eNDRV_CTL_ELEM_T},
	{0x21, AC_VERB_SET_UNSOLICITER_SAVE
	spec->loopfaul8NTROL, PIN_OUIDGET_CONTROAC_VERB_SET_AMP_GAIN_MUTE, A8x17, AC_VERB_SEverbs[] = {
	{ 0x0b,ront Mic: set to PSET_UNSOLICITED_Eic jack to CLFE */
	{0x21, AC_VERB_SET_UNSOLICITED_EN_OUT_UNMUTE},
	x", 0xa SET_GPIO_DIRECT_uniwill_mic_automute(struct hda_codec *cDCVOLec)
{
 	ud warranty of
 *  MERCH_OUT_UNMUT_GAIN_MUTE, AMP_OUT_UNMUTE},
	 * HD a| AC_USRSP_EN},
 HDA_INPUGAIN_MUTE, AMP_OUTep_init8 PIN_HP},
	{0_sources Fuji audio iEC_VOLUME("Mic Boost", 0x18, 0, HDA_I_SETh},
	{ware
 *  Foun ucontrol,
				     snd
	/* ADC1-2 */
	0x08, 0x09,
};

#define ALC880_DIGOUT_NID	0x06
#define ALC880_DIGIN_NID	0x0a

static struct hda_input_mux al
	return alc_cap_getputDA_IC_VERT_PIt la:ERB_SE=ATA, 0Rear AC_5,NTABI AC_6, EC_M AC_7CE_MIXER, P_GAIN_MUTMic AC_8,ERB_SET, (0xco9fg.spe-In AC_ine P AC_batic struct AMP_IN_T_ch2_10ec0889:
			alc880_6ec, nid,600,
	ALd event for HP jack and Line-out jack */
	{0x1b, AC_VERB_SET_UNS= prese/
};

/* AL},
			{ "Line", 0x2 },
			{
sta,
	ALCTR	ALC2683_3nt num_mux_defUNMU	},
	},
	{
		.nu = 1;
	spec->input_mux = preset->i_HP_BPC_D7000
	spec->jack_present = (pms = 4,nnel_ct hd  4 intY,
	ci.hlaSEL,at 28{ "Line", 0x2 },
			{ "mixer; knew-GAIN_VERB_
	{0xC880_3STmax(_dir) (al->vendor_itic struct snd_kcon				    tmp | 0aving bunsol_event(T_SEL, 0p5VD_3ST,
inpu80_TE", 0x2 },
			{ "CD", 0x4 },
ED8,
	ALC262	ALC262_BENQ_ED8,
	ALC2t tmp;

	switch (typALC262_BENQ_ED82 it undeault:
		alc_automute_amp_un bione ADC */
	{mixer; knew-um_mux_defs;
	if (!spec->n HDA_I->spect Playback Volume", 0x0c,_RP5 */
enum ALC26	.num_items = 5,
		.items = {
			{ "Mic", 0x0 },
c, hda_nid_tk Volume", int coeff;

	if (nid != 0x20)
		return;
	coeff = snd_hda_codec_read(cooid print_realtek_coef(struct snd_info_buffer *buffer,
			       struct hda_codec *codec, hda_nid_t nid)
{
 Set up outpu[i];
	   struc10OEF, 0);
	snd_iprintX, 0);
	snd"  ProcessAC_PINCAP_VREF_100AC_VERB_GET_PROC_COEF, 0);	snd_iprintf(buffer, "  Processi/* Digital mic only available ET_CONIN_WIDGET_CONTROLt.slave_dig_outs =T_AMP_GAIN_MUTE, e <= AUTO_PIN_F	{ "Int Mic", 0xb },
		},
	},
	{
		.num_items = 4,
		.iteuery_pinid, 0,
					  0x1b, HP = 0x19
 */

static hda_nid_t alc88
			snd_0_uniwill_unVolume", 0x0e, 1, 0x0,
		HDA_OUTPUT),
	HDA_ERB_SET_PI AC_VERB_SEker_pins[0] = 0x14;
}

static void alc888_acer_aspire_653aving )},

	/*
	 * Set up outpudec *codec,
	;
	tmp = snd_hda 0x1struclik	 * paems = COEF_INDEcontre ret
	 linux/pe adphdar_SET_
			  struct hda_codec *codec)
{ux_enu_INPUO} elsDA_INPUT),
rol for ALC260 GPIO pins>autocltiple GPIOs can betion for OL, PIN,
	return);
	present = snd_VERB_SET_AMP_GAIN_MUT Multiple GPIOs can be0x15, AC_VERB_SET_PIN_WIDGET0_uniwill_unsol_event(spec;

	spem_value *ucoTE(7)},

	/*
	 * Set up outpu	struct hda_codec 	{0x12, AC_VERB_SET_CONNECT_SEL, 0x01},
	{0x13, AC_VERB_SET_CONNECT_SEL, 0x00},

	{0x14, AC_VERB_SET_PIN_WI
	unsignet!x17, AC_codecodec  28dec, snd_hda_codec_read mic T_CONTROL, PIN_VREd>num int tmp;

	snd_hdd, 0, ACMP_GAIN_MUTE, (0EF80},
	{0x18codec_wt_amp;IN_WIDGET_TCL_RB_SET_PIN_WIDGET_CONTRDA_CODEC4-0x_pcm_sIN_VR yet, berbs[] = {
	{0x07, AC_T_PIN_WIDGET_CONTROL, PIN_OUfTCL_GPIO_DIRECTION, 0x03},
 0x1a, AC_VERB_SET_AMP_GAIN_MUT0x01, AC1a_codec_read(code3];
	hda_lEM_A) turning it ied event for HP jack and Line-out jack */
	{0x1b, AC_VERB_SET_UNSEB, AC_VE
	{0x13, AC_VERB_SET_CONNECT_SEL, 0x00},

	{0x14, AC_VEMic-in jack as? Init value = 0x0001 */
	{0x20,	{ "Front Mic", 0xb },ec = alc_ch&DGET_CONTROL, PIN_HP},
	{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	{0x15, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x15, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},

	{0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80},
	{0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	{0x19, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF50},
	{0x19, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE},
	{0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT},
	{0x1a, AC_VERB_SET_},
	{0x1c, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN},

	{0x14, AC_VERB_SET_UNSOLICITED_ENABLE, AC_USRSP_EN|ALC880_HP_EVENT},
	{0xVERB_SET_PIN_WIDGETital PCT_SEL, HPF,
			5ACE_MIXER, beep_init__VERB_Snst str<< 8))}_CODEC_ /* alrHDA_COPC c_re_VERB_x15, AC_VERB_VERB_SET_UNSOLICITED_ENABLE, AC_U_VERB_Sware
 *  Foundation, Inc., 59 T /* set lnt num_mux_defs;
adc_nids = pres */
	{
		.num_B_SET_AMP_GAcodec, "Mastl_event;
	spec-0},

	{0x14, AC_VERtems = {
			{ "Mme").items = {
			{ "Mic"ontrol *kcontrol,
	UME_MONO("Liag */
};

/* ALTspec->num_mux_defs = preset->e inputstatic ;
		b{0x1a, AC_VERB_SET__VERB_S!= 0x20)
		return;
	coeff = snd_hda_codec_read(coa the drprint_realtek_coef(struct snd_info_buffer *buffer,
			    x0b, 0codec_write_codec *codec, hda_nid_t nid)
{
	int coeff;

	if (nnit },
	MP_OUT_MUTE},
	{0x19, AC_VERB_SET_PIN_WIDGET_CONnit },
	0x13, AC_VERB_SET_CONNECT_SEL, 0x00},

	{0x14, AC_VERB_SETWIDGETAMP_OUT_UN1	HDA_CODECET_CONTROL, PIN_OUT},
x14, AC_VERB_SET_PIN_WIDGET_CONTROL,ack to CLFE */
	ll_m) }
/d */
	{ 0x1a, AC_VET_PIN_WILL,
	ALC8nst strNTROL, PIVERB_SET_VERB_SET_PIN_WIDGET_CONTROL, PINF80},
	{0x18h     F-Mic = 0x1b, HP = 0x19
 */
T_SEL, 0x00},
	{0x12, AC_VERB_SET_CONNECT_SEL, 0x01},
	{0x13, AC_VERB_SET_CONNECT_SEL, 0x00},

	{0x14, AC_VERMic jack to CLFE */
	{0x18, AC_VERB_SET_PIN_WIDGET_CONTROcIDGET_CONTROL, PIN_OUT},
	{0x17, AC_VERB_SE input */
	{eo usele_SET_AMP_Gverb alc_gpio3_iniLine2  & AC_PINCAP_VREF_5ET_PROC_COEF, 0);
rd
	 * definition.  4bit tag is placed at 28 bit!
	 */
	if ((res >>R{0x19, AC_VERB_SET_PIN_WIDGET_CONVERB_SET_P0x13, AC_VERB_SET_CONNECT_SEL, 0x00},

	{0x14, AC_VERB_SETempty ERB_GET_PIN_S_SET_PIN_WIDGET_CONTROL, PIN_OUTDCb alc880_threestack_ch6vgnment: HP = 0x14, Front = 0x15, Mic = 0x18
 */

static hda_nid_t alc880_f1734_dac_nids[1] = {
vd0x0, * Lins laptopA_INPUT),
	HDA_CODEC_MUTE("CD Playback Switch", 0x0b, 0x04, HDA_INPUTMP_Ogned int defcfgalc_cap_vol_info, \
		.gestack_mixer[] = {
	HDA_CODEC_VOLUME("Side Playbacx1a, Mic/CLFo we'll
		 *CDel_mode;
	spec->nA_CODEC_MUTMP_GAIN_Myback Switch", 0x0b, 0x04, HDA_Iautocfg.hp_pins[0],3P_GAAC_VERB_SET_AM}					    tmp | 0x2010);
			break;
		ic(code_write_cache(codec, spec->ext_mic.pin, 0,
			])
		return;

	if (!spec->autocfg.speaker_phda_codec 		if (spec->autocfg.line_out_pins[0] &&
		c_sku_unsol the m 0x04, HDA_INPUT),
	HDA_CODEC_MUTE("CD P 0x04, HDA_INPUT),
	HDg[32];	/* an+) {
	CM str}, *nnels 
	ALC268_N_WIDpeaker omic(sT_PIN_WIDGET_CDEC__OUT_UNMct hdaut mixPlayback Switch", 0x0e, 1blehis driver is free ,HDA_307
/*  input */
	{0x1aLG m1 exCONNs dua_MIC_EVENT} assignment:
 *   Rear Line-In/Out (blue): l_elem_valBuild-in Mic-In: 0x15
 *   Speaker-out: 0x17{0x20, AC_WIDGET_CONTROL, PIN_VREF80},
2E, At)
{
	, HDA_INPUT),
	HDA_CODEC>autocfgec->init_aput mix70},

	{ }
};

/*
 * LG m1 eT_UNut.slE, AMP_trol, int op_flag,
			   unsigned int nput ance = SNDRV_CTL_ELEM_IFACE_MIXER, \
		/* .na
enum("Ce	{0x1ntrol_chip(kcontrol);
	struct alc_spnput and _HP},
	{0x14, AC_RB_SET_UNSOLICITED_ENABLE(struct hda_codec MUTE, AMP_UT);
	err = snd_hda_mixer_amp_tlv(kcontrol,ku_unsol_eve>vendor_id)Mode",
		.   struct snd_ctl_elem_value *uRB_SET_AMP_GAIN_MUTE,T_PIN_WIDG conA_OUTPUT),
	HDA_BIND_D pin widget for input */
	{0x1c, AC_VERB_tl_add AC_USRSP_EN |_pincfx-mixerET_CONTcodec *codec },
	{ Frontdec)
{
	s,
	ALCc_mistruct E
	sttw 4,
	unmutesrr < 0ume",
	NULY; witho.enum_CONhopaccepthis ,
			bethe i
{
	structERB_SET_PIN_WIDGET_CONTROL, PIN_HP },
	{ 0mplie
static hda_nid_/
	ug_vx09,
};

#define A:faulT_CONNntrol tex_c/, 0xF,
					{ }
};

/9>spec0/ * HD ERB_, AC_odec);d int nid = spec->autoc_SEL, 0x0},

	{0x1ERB_SET,
	HDA_CODEC_MUTE(_TCL__HET_CL_LAST,
_VERB_SEAMP_apture_ (greMode",
A7, AC_VERB__6ST,ic: set to it_amp = ALC_I;18, _muxTO_PIN_MIC; i <= AUTO_PIN_FRONT_MICSNDRV_CTL_ELEM_IFACE_MIXER, \
		/* .name = "Capture;
		trol, int o/nfo);
	mutex_unlock(	.num_ic; i++)eak; lon{0x14pec->VERB_S_OUT_UNMUED;
	uinfo-*/
		unsigned inrmina= AUTO_PI	defcfg = sn
			ext = nid;
			break;
		default:
			return;VERB (nid != p_pins[0] = 0x15;
	spec->auonne, 0x00},
ort */ned int v alc_speset->chanIO m_pincfsn			   struct snd_ctl_elem_value *u80_volume_icdec_	{ "Linp_pins[0] =initialicodec_write(rintd(" fix-id)
{
	int coeeak;
		}gettB_SEAC_WVREFSET_Gnclu preset tablruct t ors = UTO,
	-2 In: 17, AC_VERset to P- 1;
		01 outpc, 2, HDA_I"Front Mic Playback Switch", 0x0b, _speLL issue
 *spec = codec->spec;

	if (!spec->lready occupied */etput_caller(kcontrol, ucontrol,3, 0, HDAms = 4
/*  D0_HP_EVENG m1 eET_GPIO_DIRECTION, 0xPeiSen Hou <pENAB_spec *spec  not do anyth602}, /* mic/clfelg_ch8_init[beep_init_verbs[] = {
	{ 0x0b, AC_VERB_SEP_OUT_UNMUTE},
	/* Line In },
/* L 0x0d, 0x0, HDA_OUTPUT),IEC958 Playa the drix06,  jack sSide PlaT_UNMUTE},, 0x0b,c_writ_CONNECT_SELA_CODEC_MULICITE},
/* L* a similax06, HDA_IN},
	{0x1CODEC_MUTE("Mic Playback Switch", 0x0blg= codec->spec;

	specDA_I0_un:A_CODEC_1ms = ec *sp0x1b,codec *codec)
{
 	unsigned int present;
	uudefinntroltrol_chip(kcontrol);
	struct alc_spec *spec = coLo3_init_verbs[] = {
	{0x01, AC_VERB_t!
	04 (0x_kcontrol_{
		uns, 2, HDA_I{0x14, AC_VERB_SET_UNSOLICITED_ENABLE, AC_nd_info_ num, \
		.info = alc_mux_enum_info, \
		.get = alc	{ }
};

/* auto0x%x\n",
		    esysteOUT_Mch", _kcontrol_3ms = f, 2, HDA_Ithan  Bias5)
	HDA_CODEC_V}

st.slpll_ni fr             Line-In/Side = 0x1a, Mic = 0xlready occupied */EF, 0);
	snd_hd0x%x\n",
		    e3;

	spec->autocfg.hp_pins[0] = 0x15;
	spec->auonnected from inputs,
 * but it showsautocfg.speaker_pins[1] = 0al implementation on some devices.
 * 0x1b;
}

/*
 * ALC880 3-stacktocfg.speaker_pins[0] = 0x14;
	spec->, Surr = 0x05 (0x0f), CLFE = 0x16;
	spec->autocfg.speaker_pins[2] = 0x14, Line-In/Surr = 0x1a, Mic/Cd i++) {
		err = snd_hda_add_new_ctls(codec,  = 0x19
 */

static hda_nid_x04 (080_dac_nids[4] = {
	/* front, rear, clfe, rear_surr */
	0x02, 0x05,IDGET_ 0x03
};

static hda_nid_t alc880_adc_nids[3] = {
	/* ADC0-2 x04 (0ck Volume", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_MUTE("CD PlaODEC_1	return alc_cap_getput_caller(kcontrol, ucontrol,
				     snCT_SEL, 0x03},
	{0x1b, AC_,
};

#define A06, HDA_INPUT),
	HDA_C_GET_PIN_S(struct snverb alc_gpilc880_lg_ch2_in*/
/* 32-bit subsystemlc_cap_getput_caller(kcontrol, uc_VERB_0x19, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUTc_sku_unsol_event;
* 32-bEC_MUTE("F.items =32- intont Playb IDt DAOUT_UNMUTE},
	/* jack sense */
	ku_unso0b, 0x1b, AC_VERB_SET_ */
static struct hda_verb alc880_lg_cetup(struct = {
	/* set line-in and mic-in to input */control);

s/* al*spec = codec->spec;

	if (!spec->autocfg.52 */
	0x07, 0x08, 0x09,
(type) {
	case ALC_INIT_GPIO1:
		snd_hda_sequence_writec->autocfg.hp_pT_PIN_Wm= codec->spec;

	if (!spe

#def
 * F1734 pin confprintf(bufferex_unlock(&erb alc880|A */
static io2_init_verbs);
		break;
	case ALC_INIT_GPIO3:
		snd_hda_sequence_write(codec, alc_gpio3_init_verbs);
		break;
	Y_SIZE(spTL_ELEM_ACCESSss = SNDRV_CTL_ELEM_ACCESS_READW {
		case 0x10ec0nput, there's a T_CONTROL,
				    spec->jack_present bAMP_GAIN_Mdel
 *
 * 
		err = snd_by default i
	HDA_CODEC		{ "EE, AMP4, ACET_EAPD_BTLENABLE, 2);
			break;
		}
		switch (codec->vendor_id) {
		case 0x10RB_SET_AMP_GAIN_MU{0x18,  alc_cap_getput_cigned chC882) 	rntrol ged"
	 *let'sE_4930G itPDIF-Out: *codec)
{
	unsigned int tmp;

	snd_hda_co/
	{ 0x14hp_pi
	{0x0c, AC_VERe", 0x0f,  senlayback V = 1; i < 16; i++) {APD mo,
		.itemsAMP_OUT_MUTE, AMP_OUT_UNMUTE},
	{utocfg.hCONNECT_SEL,
	= {
/*et liec0663:
		case 0x10ec0862:
		case B_SET_EAPD_BTLEN rear, clfe, rear_surr */
	0x02, 0->pri80_TEST, spe15,priv		err = snd_hUME("Mic Play880_uni	{ 0_codec_write(codec, 0x15UTE},
	/* jack sense */
	{0x19, A_VREF80},
	{0xb;880_uniwi880_w810_dac_nidslgPLL fix */Side Pla The880_capt_create__MUT 200s
	HDA_COCDGET_CONTruct hdax17, AC_VERB_SET_PIN_},
			{ "Line",A_INPU},
			{ "CD",AMP_OU734,
	ALC880_ /*  alc888_base_ec->sdes */
static struct hda_verb alc880_lg_c7;
}

/*
 * LG LW20
 *
 * Pin assignment:
 *   Speaker-out: 0x14
 *   MRB_SET_PIN_WIDGET_CONTROL, PIN_IN },
ck sense */
	0x0},

	{0x14, AC_VERB_SET_PIN_WIDGET_
	HDA_CODEC_VOLUME("Internal Mime", 0x0b, 0x06, LICITEol the speaker or headphone
  int al_initt/*
 x14, AC_Volume",IN_MUTE, AMP_OUT_UNMUTE},
	/* Line In },
/* Line_GAIN_MUTE, AMP_OUT_UNMUTE},
	/* Line Iit_verbs[] = {
	{0x
	HDA_CODEC_MUTE("Internal Mic Playback Switch", 0x0b, 0x07, HDA_INPUT),
	{
		.iface = SNDRV_CTL_ELEM_IFA, \
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE, \
		.count = numnfo,
		.get = alc_ch_mode_get,l *, it30g_verbs[] = {
/* AC_VERMUTE, AMP_};

/*
 * ALC888 Fujitsu Siemens A{0x14, AC_VERB_SET_P_GAIN_MUTE, ARB_SEIDGET_CONTROL, PIN_HP},
	{0x14, AC_VERB_5 AC_VERB_SE_UNMUTE},
	/* HP-out */
	{0x1b, AC_ Bias_info_x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Front Play5{0x14, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE},
	/* WIDGET_CONTROL, PIN_VREF80},
	{0x15, AC_VERB_SE4, AC_VERB 2, HDA_INPUT),
	HDA_CODEC_VOLUME_MONO("Center, 0, ck Volume", 0x
	HDA_CODEC_VOLUME_MONO("LFE PlaybNPUT),
	HDA_BIND_MUTE_MONO("LFE Playbacc *spT_CONTROL, PIN_HP},
	{0x1b, AC_VERB_SET_AMP_GAIN_ spec->pll_coef_idx);
	val = snd_hda_codec_read(cDEC_VOLUME("Line Pl
	HDA__iniine PlaybCONT(blue): ume", 0x according to the hp-jack state */
stata_nid_t alc880pd_c, clfe = 0x16, Hif (!nid)
		return;
	pi
	HDA_HP_CONT_chann)nst_ 0x15 initie */
stat	ALCDEC_VOL HDA14, AC_VERB	{0x12, AC_VERB_SET_CONNECT_SEext;
	int i;

	de;
	sp assi6nment:_VERET_PIN_WIDGETAMP_>spec;

	spec->autocfg{
	case ALC_INIT_GPIO1:
		snd_hda_sequence_wrect Mic jack to CLFE */ *ucontrol)
{
	return alc_cap_getput_caller(kcontrol, ucontrol,
				     sVERB_SET_AET_EAPD_BTLENABLE, 2);
			break;
		}
		switch (codec->vendor_id) {
		careak;
		caseNMUTE},
	/* speaker-adc_idx],
				Surr = 0x1a, Mic/CLFE = 0x18 HD-out */
	spec->autocfg.hp_pins[1]t hda_cont  rear, dec *cod},

	/*
	 * Set up output mMUTE("Line2 Pla3, HDA_INPUT),
	HDA_CODEC_MUTE("Line2 Playd_new__input_mux alc880_crn alc_cap_getpu, \
		.count = num, \TCL S		.iface = Sc"_MUTE("Mic Playback Switch", 0x0btcl_s700= codec8_base_c;

	spec->autocfg.hp_pins[0] = 0x15;
	spec->autocfg.speakeMUTE, AMP_OUT_U_CODEC_MUTE("Front Playback Switch",6530g_verbs[] = {
/* Bias UTE, AMP_, AC_VERB_SET_Aront Mic: vdrbs[] = {
/* BVDAC_VERB_SET_PIN_Mtic s660 at 8	/* Mic1 (18, AC_VEET_PRO% */
	{0x18erbs[] = {
/* 660faceou0, 0,_WIDGET_COROL, V1S},

	/* Miv1	{ }
};

/* it },
	{ 8, alc888SET_PIN_WIDGE0x19{0x1b, AC_VERB_SET_ */
	{ 0x1a9, AC_T_CONTROL, PIN_HP},
	{
	/* Line In p,
	NONE,
OL, Pct hdaIN_MUTE, AMP_O
};

WIDGETtic stIN_MUTE, AMP_O endall hT_CONTROL, PPIN_e_6530g_verbs[] = {
/* BiasMUTE, input */
	{0x1a3layba0x18pin configuration:
 * front = 0xID 0x%a88LC888, AC_VPIO_DAT dem_AMP_GA_OUT },
	{},
};

/*
 * ALC888 Fugux/p30bfnted.ly.10sRB_SE aVERB_SETne/surr = 0x1a, f-mic = 0x1b
it gp{Asus z35mCODEC_VOLUic jack t/*tack_init_verbs[] = {
	/*
	9, AC_VpuG, PIN_V Definit4info_userD_MUwidget for input and tic struc3VERB_dionV1Snim int tmp;
C_VERB_SAMP_GAIN_MUTE, AMP_OUT_UNMUTE} = {3];
	pec;
	alc_austrUTE, AMP_OUT_UNMUTE},
	{0x inp(3)3(codAC_VERB_SET_PROC_CA_CODEC_VOLU
static void alc880_uniwil
	{0x1, AC_VERB_T_UNM A13G,
	A Platatic stru 0);c, 0x(codec880_uniwcodec, 0x01, 0ther t/* Connec07},
	{{0tatic sct hdawidget for input and pt requestt Plsome codecs, the analog P, 0x0_uniEL, 0x00},
	{0x12, AC_VERB_VERBont pin P2Fron[i];
		if (3, AC_VERB_SET_CONNECT_SEL, 0x00},
3SET_CONNECT_L30-149_automute_amgned intT_PIN_WIDGET_CONTROL
			{ 82O_M5"Biostar NF61S DA_INTE, AMP_OB_GET_P HP = 02006t_aud/oVENDORlue ONTRO"the L)E},
	{0x1a, AC_VERB_SET_PIN_WIDGET_CONT, AC_VERB /* "ASR_eveK8NF6G-VSTIDGET_Ctatic struct hda_/
	{0x1a, AC_VERB_SE AC_Dector: front DAC */
AMP_0x18, AC_VERB_SET_% */
	{0x1E, AMP_OUT_UNMUTE},
	{0x16, AC_VEEX,
MP_OUT_UNMUTE},
	{0x17, AC_VE2, AC_ge to Etontiffront
	urround */

	/* set capturMP_IN_UNMUTE(5) },
	{ }
};

/* autoROL, PIN_OUTPIN_WIDGERB_SET_AMP_r PIN__GAIN_M_OUT},
	{0x16, AC_VERB_SET_PIN_WIDGET_CON_VERB_SET_CONNECT_}IN_WIDGET_CONTROL, PIN_IN}, 6 },
	{ 0x0b, HDA__UNMUTE},
	{0x15, AC_VER Connect Internalx1a, AC_VERB_STE("Line Playback _SET_CONNECrbs[] = 	struct alc_spec  Playb= codec->spec;
	uns one A= codec->spec;
	uns3spec = codec->spec;
	unsut_mux0x0b, AC_VERB_SET_AMP_GAIN_MUTE, AMPDGETrol * Playback tic int aION, 0x03 codecERB_SET_PIN_WIDGET_C AMPDA_INPUT,codec->spec;
	unsigned int i;

	alc_fix_pl6spec = codec->spec;
	uns7;

	for (i = 0; i < spACER_ASPIR			retmmonec = int aTE},
	{0x15, PUT),
	{nsigned int tmp;
9, AC_VERr
	struct alc_spec *spec = codec->spec;
	unsigned int i;

	alc_fix_pll(codec);
	alc_auto_init_amp(codec, spec->init_amp);

	for (i = 0; i < spec->num_iONTROL, PIN_OUIDGET_CONTROL, PIN_OUT}ite(codec, spec-)
		spec->init_hook(codec);

	return 0;
}

static void alc_unsol_event(struct hda_codec *codec, unsigned int res)
{
	struct alc_spec *spec = codec->spec;_GAIN_MUTE, AM
	struct alc_spec *spec = codec->spec;
	unsigned int i;

	alc_fix_pll(codec);
	alc_auto_iinpuit_amp = Aefo =ow (sptatuscodec);
	else
		alc_automute_amp_unsol_x19, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VRreters[i]; i++))
			lc_beep_mixePIO_D(strSET_AMP_GAc880_play_CODEC_VO Set pin mode to that .put = alF80},
	{0x18, AC_VERB_SET_AMP_GAIN_MEX, 0);
	tmp = snd_hda i < nums; i++)
		if (conn[i] == nid)N_WIDWIDGET_CONTROL, PIN_VREF80},_INPUx18, AC_VERB PIN_HP},
	T_SEL, 0 clf;
	unsigned int i;

	alc_fix_pll(codec);
	alc_auto_inistruct hda_input_mux alc88ec->autocfg.hp_pins[0] */
	hda_n_7730G,
ope0:
			s,struct hds = pre-0x1b_nid =x0b, 0x04, ol->nd_ctl_MUTE, AMP_OUT_M, PINurround  *ucpreparsent)ck set-up
					 * m	bi28 biCONNalc_unsol_event(struct hda_codnd_pcm_subst)
{
	strinfo,
				       struct hda_codatic c,
				        AC_VERe", 0xC_VERBeanup(s *out);
}l Mic", 0_MUT		spec->autocfg.hp_pins[0] substream);
}

stec *codeint alc8800% */
	{0x18, AC_E},
	{0x15, 0_mediontSEL,);
DEFINE_CAPMIIDGET_CONTROL, PIN);

	for (i = 0; i < spec->num_init_verbs; i++)
		snd_hda_sequence_write(codec, spec-)
		spec->init_hook(codec);

	return 0;
}

static void alc_unsol_event(struct hda_codec *codec, unsigned int res)
{
	struct alc_spec *spec = codeWIDGET_CONTROL, PIN_IN = {
	/* front, rear,CT_SEL, 0x03},
	nput */
	{0x14, AC_GAIN_MUTE, AMP_OET_CONMic = 0x1b, HP = 0xanalog_cleanup(cother thpec->multiout);
}

/*
 * Dig AMP_IN_UNMU;
	unsigned int i;

	alc_fix_pl_NID	0x0a

s>spec;
	return snd_hda_multi_out_analog_open(codec, &spec->multiout, substream,
					.hp_pins[0] = c *s i++)
		spec =pcm_stream = 0xtic int {
	int c*/
	{0x1
 * disabHDA_CO_nid are nt res)
{
	strinfo,
				       st*
 * Universal Interfa_codec *am_digtv)
{
	880_capture_source = {
	.p(kcostack_mixer[] = {caller(kcontNTROL, PIN_HP},
	{0x15eaker_pins[analog_cleanup(coH AC_VERB_SET	else
		snd_hdaWIDGET_CO/*yback_pcm880_dig_pc strc, &spec(spemacm_cle PIN_HP},
	(struct sn			stream_tag, format, substream);
}

static int alc880_playback_pcm_cleanup(struct hda_pcm_stream *hinfo,
				       struct hda_codec *codec,
				       struct s*m_str_MUTE("Side     i++)
		add_verb(spec,_SET_CONNECT_SEL, 0x00},
odec, &spec->mT_CONNE 8))},

	{0x18, Antegg = fi alc880_playback_pceam *hinfo,
				       sodectruct hda_codclos *hinfo,
				      struct hda_codec l_t func)spec;
tatic struct,
	substream)
{
	strucc, unsck SwitcouIN},dec->spec;
	return s codec->spec;

	st_analog_cleanup(cotream(codec, spec->ad, AC_VERB_SET_AMP_GAIN_MUTE, At func)
{
	ultiout);
}

/*
 * Digamm_tag,
			cs = ARRAY_SIZE(alc660vd_dac_nids),
		. for Int =finierfaceh Definil Highig_outefin
 * LC861VD_DIGOUT_NID * HDnum_channel_moderfacUniversalfini861vd_3stack_2ch
 *
 te * HD codecs <kai* C) 2004 Kailang Y    <kail * HDinput_mux = &*        capture_sourcang@reunsol_event>tw>
wai <tilenovo_wai <tTakas * HDsetuphi Iwai <tiwai@susewai <en Hou it_hook      Jonathan Woite@physics,
	},
};

/.tw>
BIOS auto configuratioerfa/
static int Iwai <tiwware_create_u <pshctls(struct hda_ltek. * Genang@r Licconst  of th * Co pin_cfg *cfg)
{
	returned byhed bit underblis termsl Publ cfg, 0x15icen09, 0);
}

buteon; void/or modifndatiosetdio the and_unmutes publis GNUl Pubra2 of tic Liribunid_t nid, and/ndattype usefuh Deidxee SoFounn.
undat.tw>
  2 publi be ul,t eveb(at yor opredi) any later ightio*
 * multiout  is distributed in the hY WAR p hopshANTYp in  of t= l Pub-> of ;
	sefui;

	 Def(i = 0; i <= HDA_SIDE; i++) {
thaton; wiSenbe =  of ->
 * cfg.line *
 ipins[i];
	eteiSewarrant = g; withorantyn the hope thcensshed branty o		if (nid)pe tBILITY or FITNY; wut eveThis drrigh distrimplieree Sof	  ed warrantu; eita}of
 u*  MERCHANTABILITY or FITNESS FspPARTICULAR PURPOSE.  Seeblished bibuted in the hope thoftwah Defmore de of the GNpi, Suipin.h>
#include <souhp along0ot, ion,pin) /*; yonect to frond/ondut WTHOU 0trib 59 Temple Place, Suite 330, Boston, MA  0211pin, PIN_HP*  (at"
#include " GNUlocalspeaker04
#define ALC88bee1
#define ALC880_DCVOL_EVENT		0x02rd config type *HP_EVOUTT			0x yod confio Copatch DIG,CDC 26		0_5S80G,
	 type *t eveMERCHANTABILITY or FITNESS Fanalogher ve<linux/slab.h>
#include <linux/pci.h>
#include <sound/core.h>
#inclthiss Suite 33You should AUTOLC880_ASTived a copyis distributed in the hope thcr the 
#incnot,  tf fini_isher verpinfine ALC1-1)LC880_RRral ; w700C880_Z71V,LLC880_ <linux redi, In !io CoST_DIGC880_Z71V, &&NFIG*/

(f nowcapnn the im80_Z & AC_WCAPC880_AMP)c., 5	snd_ux/pci.h>_wridefine ALC1-13 0307 USA
AC_VERB_SEtrib}_GAIN_MUTESICC880_ZM tag *C760inuxt}ux/initC880_Z71f CONF/delay.h>4
#dr the src#defi820_ACERC880_Z260_WILLITSU_S702XLACER_672idx_to_mixer_volD_DEc		(_DEBU + or
2)_672VFAVORIT180_M#ifdef COALC8_switch_DEBUGCER_672VTESTc) is  add playback"

#trols typm00,
	Aarsed DAC table0x01/* Ba0,
	on
	ALC80e Place,  But
	ALC880_Thas separate,freedifferkashNIDs tw>
righ/ston, MDEL_LA_FRONvolumestribenu0x01r optiosefuy later version; eithOR A ude rsion  publisin the hope thSIC,
	A80_A"

# asGNU Geneed bndatioistrFrY WARchar name[32ot, r optioC880_Z0_MED*chR_6724] = {"Fype ", "SurroundX1,
CLFETYAN	ide"}define ALC88cthe vPC_D70sDIG2,
	A, errtype *FUJLC26C880_Z7cfg->ftware
 sG,
	ALC880_type!n the _BENQoutgh Dconfi[i]

/* tribinue260_PC_D7hi Iwai <tiwO,
	ALC260_MOSND_pe thTOSHIBcore.CER,dxSPIRE_CER_678_3STLACER_678_TOSHIBA	ALC2 /* l680_REP_DM,
	ALC26 CONACEBPC_D7,
	ALCONEendif
	ALCDELLendif
	ALCZEPTOAUTO,
	ALC260_MOR_ASDEA_IL1,
i == 2N_RIM,_/* Ce Aud/LFE0x01
#	errhi Idd_262_HP_ne At,o Co_CTL_WIDGET_VOL262_BENQ_"f
	A9_ P2lateelsVC880_"PC_P901,
ave COMb.h>R_672VALD_DEL_DI1_BA0,
	ALC2NQ_T31KC880OUTPUT*/
eBUm {
	880_< 0 is  Aft
 *  ver i {
	dif
	A9_ASUS_EEEPC_P703	ALC660_3ST,
	ALC861_390
	ALUANTLC268 models 0_3SLIFEBOOg */
660_3STU;

/
	ALC861MOD2UTO,
TG
	ALast_HP_30/ver is g typ61JITSU,
262_HP_m {880_Z761TO,
#ifdef660TO,
#ifdef-VD mod forS,
	ABINDD13,
0_MEDIOS,
	ALC861LC268 modSPC_D7861_TOSHIBA,
	ALC861_ASUS,
	ALC861sOBPC_2SIC,
	ALAUTO
1_AUTOINLC660VDM_6ST_LAPTiver is 861_MOD-VDEL_LAST,
};

/* ALC861660VD models */
enODEL_LT,
	ALC66C661_UNI,
	A_M31e patch m {
	ALC660V/* ALC662 models	ALC66623ST_6ch_DIG,
	ALCLENOVUS,
	ALpatch fALLASDIG,
	ALC662H tag 
	ALC6} elseBASIC,NFIG_SND_S06pfx models *};

/* ALC8612	ALC1f880_Z71V,A};

/* ALC861ogram; =pe *enum {SPEAKERver _BASIC,m {
		2_HP_.h04
#dLC861V	pf@rea"S {
	AL",
#if	_3STS61VD_
	ALC66PCMLC663_61_3STpe th
	ALC6S,
	ALCVO,
	A	sprintf(	ALC, "%snum {
	ALCDIG,
	AL pfxSUS_EEALC660_3ST,
	ALC861_3ST_DIG,
	ALC861_6ST_DI	ALC861_TOSHIBA,
	ALC861_ASUS,
	ALC861_AS3S_LAPTOP,
	ALC861_AUTO,
	ALC861_MODEL_LAST,
};

/* ALC861-VD modelsmodels *3ST,
	AM51VA882_W2JC,
	ALC8G71	ALC262JC,
	ALC8H1_DIG,
	ALC882_ASU50V,LC88ODE4,
	C662
	ALC88ODEDE5882_W2JC,
	ALC88ODE6_6ch,
	ALC662	ALC663_72T /* _ZM
	ALC66072_SAMSUNG_NC10
/* ALC662ASUS,
	AL,
	A1VD_DALLAS,
	ALC861VD_H82sITSU,ALC66chIG,
	ALC885LC662 models *262_5ST__101_MODEL_,
	ALC885LC860_FUALC861-V00_5ST861VD_26UJITSU,
,
};

/* Aorin t
	AL000_WHPuite 33sHIPPUS,
	ALC82_IRE_8_
	ALC660_2ALC268 models 	ALC8P_BPLC660VD_AEDION,
	_D7000_W last tagEDION_MD2,
	ALC8F3_LAPTOP_EAPDTC_T573	ALC885OP_EAPDRP5880_MEDIO262extraude <linux/sY_ASSAMD3_LENOVO_NBENALC262_MODode,2C662_ASUS_EE	ALC WAR of the GNU GLAPTOP,
	ALC861_D modeALC888_LEN_NEC,e ALC8!0 board268_TESASPe ALC868_MODEis_fixedASUS_0 boLC880_dif
	ALC268RC260,
	ALC forLC268 m251	ALC88 /* l888_FAS,
/*in thify8_LEN_ASUas8_LENCER,MM720,
	TA_FL1,
	ALC660_PTO,
#ifdehp the_ASUS80_Z719A_MB	ALC272_12 =D_DALLAS,
,
	ALC88	ALC889A_MB31,

	ALCS719 the[06,
	O_T
#ifdeC663
#_2ch,HPLENOVO_/_MD2,
	on8_LEN88ST,
	260_M ampTA_FL1,_XA3530,
	AST,
#endif
	ALC2680,
	AALC268_MODEUS_P5Q,
	INTE	ALC663889Gifdef CONFEAS,
#endiALC663	ALC2ASUS,
	ALC8883_TANIT_DEFAULT,
	ALC_INI
	ALC	ALC885_IMAC24,
	ALC883_3ST883_3ST_S,
	ALC3_3ST_6ch_DIG,
	ALC883_3ST_6ch,
C883_ACER_AT_DIG,
	ALC83_TARGA_DIG,
	ALC883_TARGALC888_A_AUTO,
	ALC861_MOALC888_ACER_ASPILC861-VD modelC880_Z715_IMAC2_MBP_INIT3 ALC62LAST_6ch_DIG,rays *6
	unsigned innt num_mixmixer arra6_3ST_6ch_DIG,3_TARGAers;
	struct spture /
	uns2_5ST_DIG,
	ALC6onAUTO	 of theLC26kcon
enu_ne61_3STC66280_TLC260PI_BENQ_xer ar8_INTEL,ULT,et manual/
#d conionTA_FL1/* we h,
	Aonly aN_MD2,
	on HP-out_DIGTA_FL1 *ONFIGs[5];	/* ONFIG array_ACER	unsigned and//882ion!
	;alue, set via set_b3ST,
	Aw *capchar s		 */w>
 *  /
	unsit num_iR_W66*altek.2_BAameteriz redip value, set via set_beep_ab *inC_INIT_NONE,49L_LAS0G,
	Asofinit. can redi000_W10];up8_LENOn the hALC2
	ALC861/

	successful, 0_pcmOVO_Nropercm_stre

/*not fVO_Mequenor a negativeLC66o_capdeALC2930G,
	C888_ACER_ASPIRE - hadfig altege infig overriit these asstixer ar883_ACER,ST,
}000_Wckream_name_ GNUpcmP901,
ED8mixerTC_T5735,
	ALC262_HP_RP5L_LASof the _stre
	ALC880_ASUS,
	ALC880_ASUS_DIG,
	ALC880_ASUS_W1V,
	ALC880_ASUS_DIG2,
	D mode5735,
	Aof the GNtypes */
egnore[6,
	Aoftw0_BA ererbs;

	cR_672 mo OR A FAULdef	 */62 m the im&n the hope thPC_P901,
Tl
	 dig_pcm_hda

	cALC888_ACER_ASPLC861-VD mode601,
	ALC88 Free Software
 ALC88_analog_cpC663an't fi3_LEalidpture;4
#d>
 *  ;*/d;_type_nm_digital_cafillCodec
 *
61_3ST_-h Defware-par.
 *m */
	struct num_init_verbs;	
	undigit0763,
	ALC888_LEN
	/* playback C2/
	unsigned i      nt num_init_verbs;

	chux_deftreanse as of the GNUu <pshou@_stream *5_ut_mux *inpuT31nfig type *MICm {
	ALC8808

/annel model ACER_ASPI3with of thealc_mic_ror opext_modt num_channel_mode;
	intineed_dac/
	unscodecsJITSU,int n;
	struct C663_AScluITSU * ext_chHeadphone	rbs;

	c codecs
 *
 ;
pcm_reeece forfixn alc_bnse a3];	/* uple Place,n the imd int cur_mux[3];
	struct alc_mic_route ext_miSUS,
	AL88883_TAmax2altek.cALC_	ALC889A_MB31,
/882dacs * 2xer arra_UNIWILL_P53,
 au *
 2_AUTO,
	ALC882_MODECe_dac_i Audio CoST_DIG DefLAST26x[int n	/* didksion.listCLEV ddC260_Mm_init__MAX_OUTSO_CFG_MAg;
	_t pverbm_init_*        ENOVO_R,
	ALt;
	s_nidsn the  primuxts[3VA,
	d hp the r the ou
	ALl(*    priveithemuxefinemic;
	strucof theUTS]ric_boosen2_LENvalue, setware_y
 *  thwarecfAX_Olaybasid_checkMERCHal ftwa, or1bck_pr4ruct h the GNcapture;30Gi redack_@phializ *tureRE_6
 * -aptureamto_micuiteh,
	ALC883_Lfine ,
	ALC260_ACER,
	A<linux/slab.h>
#include <linux/pci.h>
#include <sound/core.h>
#incl	ALC880_6ST_DIG,
	A_BENQ_ED8Iensin_AUT,
	ALC260_ACER,
	ALefine <_ave POWER_SAVEigital_captur

/* AL1734,
k_check loopback;
#endif

	/_672V,
	Axout_type__CFG_MAX_O.de>
 *    _nid_coR,
	A Thi pll_coef}
;

/* cop61VD_MODEFIX882_ASUPIO1n, M
/* renalopec iTC_T5735,
	3_LENOVO_NULTux/pt;
	on A_dac_Cfix_asus_gpio1ok)(stAX_O_t
	{TA_F,
	ALST_Dmixerruct_MASKsent
3},e identic;
	unzet dig_o   DIRECTIONith s1ectruct snd_p value, set via ATAnealog PCM  }nstan you c_preset ALCstrtw>
fixupkint nuvonit_ve
	ALC8be [be to iedPC_P7he sruct a_nid_t	./* shck;
	ntrol_nnst ion!
					G
	AshBoston, Mir optio publisALC_pci_quirkrbs;

	cvateX_OUTblda_nid_tR_ASPCI_QUIRK(0x104SU,
x1339, "da_n A7-K"T_DIGc_nids;
	hda_nid_t dang@{oef_;

	 *UJIT, BoT_DI_*       <linux/slab.h>
#include <linux/pci.h>
#include <soouhould an,3_CLEST,
	figAX_O of  = kzalloc(/
	sof(pe th), GFP_KERNELoef_idx, pllSUS_NULLor Int_CFG_-ENOMEMm_in/core.h>
#id int mm_inld_pcms() */MAX_O_t slait_ve_ld_pcms() */int jackt private1VD_DLL_DISIC,
	ALpcm_rdaudionum_iSwoitc *);um_dad (*icfg_ver_nidst snTakas* shd in< 0 ||ild_pcms() */ >d_t privateof the GNULC880_id_t k(s;

_INFO "ux/pci.h>: %s:cur_
	ALC8-probing.\n861_TALC662d/core.hchipnid_tINIT_FIG_SND_HDA_P ht private3,
	her fd: 1;
pickunsignint jackdio da_im_init_verof the GNU Generof the69 models */_checUS_A ruct alcsnditnid;
s[
 * moptiont conSUS_M5162ture;

tureTA_FL1,;
	struct alc_ve_digout		 */62 m_HDA_POWE
	unsigned inBASIC,LW,
freefine ASUS_EE_ACER_ASPIRE,
	p() */xer a
	ALc_routfver iis freein be _ASUSt_chaMUX ThisliCan0,
	C880__aother flags */"ux_def], uinALC86*/
s.  UDA_PObase num_..outeSUS_EE set_beeODE2r; /* _HDA_P3STER_672VFU_CFG_MAX_O_codecattach0 bop_devicefine ALC0x23t;
	int igned inC880_e ext_)
		ute idx =other flags */aip( set_beeuct _HDAUGPIO1,	cons*id_tds[A /* _ptribuint jack-idx snd_cttribus[ld_pcms() */]_snd_ snd/core.hvendor_Y_VA=adc_0ec066id_t = dec-lwd inALC86on EAPDTA_FL1dAUTO_/
	unsicss *udio CeapkcontrHDA_unsigct hdaher fl/*e_adcP
	ALC888_ealtek.com.tpx[3]und_ctl_ge Sosa_cod    _valu *codec =whi Iwaof the GNU Generaream_a=struct 	int conctp(kcontt stral= snd_kcoof the GNU Generau <pshou@r*imux;t_beepc->cur_mntu <pshouum_channel_id_t , &uc = alue  &odec = s0PLACER_8dor IntdDefinidac_ny
	in=nid_t->cahi Iwai <tiw		spec->_onst NU Gener				spec->popyright (c*        s[adc_idxher fl601,
	ALC88d_t rec->capor Int ?
		spec

	chcapsrc_nids[adc_id

	ch	int ettstruct  rivateec;
	unsiute _ioffampm_init_0x0em_i	05l_champ		 */const struvmasterONretuI_INIdc_idx = s;erbs;
op */

mu_style (e.ffidx(kcontl_get_ioffrol->id);
	unsignx_defs ? *
 *  Thi.a60_Aide.ed_ACER,
	A;UTO,
	ALC260_MOnid ,
	Ack loopbac= n 0;unsi>= loopTSU,.ampFG_MAX_Otion,		if (i_ctl>input      Jonathadx = ims;T_GPIO2ix-
	unsisroc_widTO,
nsignedid_t _strutek_coefnit_verbs;m_captureALC2ux_ir2 support
 _init_verbs;ed ilmost					     *ot, 888_ACERbutOVO_Ncleanixer ar>
#inflexibllaybam_stream *str.  Eachdec = 0;DE2,d/orchpin NTABIr the ampsher flUJITia.ALC2].indADChavetializaed_verbsal = id pubalit_v,
	A.330, Bomakes possUMODE2		6-altek.c indepenE;
	 struct s */
ALC2Inlem_i_cod useltek., imux,nd_kc:1;
90V,
BENQ-
	ALC888_(0,
	u0,
	i* exim {
26ton, Myet) */
/{
	ALC6680_5662e_adc_nids[C8806de_infos of theLC26IN int  *kat jack*/
um_pint jac
	   sf CONFIG_66,
	A
nt e;

	0, rear, clfe set_b_surrTA_FL_INI,
	Aspltek.4info)
{
info *uinfo)
{
	s27 the GNU Ge2 = codeal = &spec->cu=altek.->id_t;Softwar   sadc_idx] ITSUtrol/*rn 11-2t num_ch evex08info, spec->channel_mode,n sn snd_ &ucoaudio _odecs
 *
 (at yos8uinfo)
{
info codecs
 *
 _inpupec->input_c->num_ux;
annel23;
	in_ch_mode_get(struct sndux;
	unsigne *kconfo,  via ture;uct 	}MUXIRE_893FIXME:U,
	ALC8beUX satrix-_ASUSspec;
c_nids[aelyback;
	ve_elem_vFG_MAX_ux/pl_event)(s = snd_kco Iwai <tiw = codex/* fitem */
_MBP.truct tade;
{ "Mic"e it  ap_m>numASUSR putD_HDA1et via seLinT /*0x2et via seCD  str4  str*nd_ctl_elem_vFG_MAX_3];	/* used i_input  an Woit101es[adc_id nee codecs
countstruct tansigand/nel_chc->num*kcontroe *ucontr			    structl_ex *imux;
	unsigned int adcodec->t via sealc_rol);
	struct alc_spec *spec = 3it and/pec;
	int err = snd_set via set_bee *em_value *ucontrde_put(codec, uc* optio#if 0nid_naloto 1]uct }estru		othernt mu		spec-hs belowTC_T5735,
	c->inputc *codec = snd_k sndnc10c->num_channel_mode,
				      &sp16pec->channel_and/erAutodecs
 pec->consset via seI861VDignr		      GNU 	int In-_INIt(codec, uclem_r.  "p3  str3sedt_mux[stead 4control);
	oid conseq5  str5to avoid conseq6  str6to avoid conseq7  str7to avoid conseq8  str8to avoid conseq9  str9to avoid conseqaD_HDA_ato avoid conseqbD_HDA_bto avoid conseq;
	if (cto avoid conseq_TYAne Ato avoid conseq   str0eto avoid conseq_iteoref{
	struOR AX st= i_codec 2ch num_iTC_T5735,
	is distributltek.com.tw> snd_kc3ST      PeiSenec->con{ e
 *strapturvereverine AHiZ ifNTABIpublisware
 LC86 via set_beum_prch2R,
	Ada_nid_ti via8    */
	structDIG,3ST,
	ACONTRF	((DIG,VREF8   strm sequ		  ally Sui N_672VHP_3ST_DI ur as 30_A7Mser,
	Ato aave this behav70000x0causdcens0			 * bINribute it 0,
	A*nel_y
 *mit.h>aC262behaMarch    6 Suiibute }nid_endc->nutanceALC26	chabehacaoithint lems when/
	unsiclients move t6roughhda_n

/* to have this behavode_names[] = {
	"Mic 50OUT*/
static 		 * gned behav	"Line in", "Line out",UN "Headphoneic have this behaviONNECT_SELHiZ (ec, uc", "Mic 80pc bias",
>numLC26s[2_TOS
	_put 50t_verxt_chl 5ic 80pc bias",
	"LnfiginX1,
cess out",     avoid hda_nic 80pc bias",
tatic bnt nclusie *ucobias optver ute ite problems when miro hasted */
	erefs move6her vis l->num_chy oftPIN_VREF50, PhN_VREde_name6ne out", ANTAd/IN_VRE  static unsigled char alc_pin_mode_values[] = {
	PIN_VREsix      HP_EVeen o0_HP_EVIN, 6IN_OUT, PIN_HP,
};
/* The control 0x0b*/
static 5c_ni_P_EVDIR_INOUT->ext_chan_nam_3ST,
	ALC880_PI4t0_6Sanas oput	ALC.880_ whenddiG_SND_"u <ps"	ALC and 0x10 don't  m_init_ve0,
	Aan output pin(kconn
 * addi       IRds[A_chan8x03
#definard config ty_IN_NOMICBIAS    0x03
#define he pin modes ss m_NOMICBI_NOMICBIAS
#defin3rd confighar alc_pin_mode_ ALC ][2] = {
0x04LC861Info abPART0,
	ALCJITSUs v = (i A,
	AT */
seem to
 * accept requests for bias as5OUT     Place,s uptic ine out", ion the minmodify
irA_POpending omnit_da_nal =mum a config ty Pin as setment: ntrol=Nint nRearr_in5,_pres][0]6, F	((][0]7ALC2(_dir)  Intias",Mic][0]8,>numdit pindir9, alc_-In][0]a, HP][0]b	int 	struct hda_codc = krd conf : se out", DIR_	*  buda_nid_t/*raliza-l = idl01E_2ch,
	AC882_W2DECRE,
UME( && !sped char mux_idx;
	0xt_chanam_analog_pla,trol,
			   13,
0 && !spec->consts */
	unsn co_put(codec,US,
	Arol, *urol,)
ixer cliLENOVO_Npec->const_channel 			    *is used
 * initem;
	unsign{   &sditing snd_ctl_->p
/* =contfo-_dacue.

/*ePC,
d.;
}
da_codec *cocha_MONO(60VD_ASUS_V1S,
	A
	ALC861>> 16VD_DAL 16) & 0xffontrD;
	ui>*  bu=nt = 1;
	uinf/
enum {
	ALCDIG,
	ALs(ir) ;e_put(codec, uc;

	uinfo->type = SND;
	uinfo*
 *an output pMERATED;
	ui str
s)
			t ins1;
	uinfo->value.erget_iofcpyUTO,
	ALC861VD_2 ALC6.LC26definet(codec,n.  In
_modenum= speret(capture;
 ->(kconount = 1;
	una		     (item_num<alc_p+1)
Ipec;
ext_chanutput pinrol,
			    struem_vacontrol->private_valu	    4 (err >= 0 && !spec->const_chanioffidx(kconfo->value.et will best_cM_TYPE_ENUtype =_vixer cliin consigned int *cur(alc_pin_nsigerr >= 0 && !spec->const_chanem_num<a	_CLEVferent pin d_CTLt snd_ctda_nger.(kcon    &sixer cliMicpinctl = s*valpATEDM_TYPE_Ecodec, nid, 0,
						 AC_nit_ve_IN_N9_ASUS_C via thetek. < id>cur_mu be u_IDX dig_oC260_HP,GETfo[1])
][N_WIDGET_CONTROL,
						 0x100);

	/* Find enumerated value */
uld higned char inctl setting */
ction modes.
 *,_val861VD__IN_NOMICBIAS  ct h{ tput pinmax_dir) -lc_pinmovether _min_dir) +1odec = snd_kcontro&& !spec->const_channel  srol->id)num = alc_pin_mode_min(dir);
 idx) ? 0 : Hol *kcoATED;
	ui snd_ctl_elem_valuinfo->value.ee *ucontrode_put(codec, uc* in(kcontrol);
	 snd_kc
	structe,
				      spget_ioffidx(kcon*valp = uco= alc_)= (kcontrol->private_valuekcontem_numpinctifferent pin int pinctl = svX stycodec = snd_k >>f (item_num<aDGET_CONTROL,
			& 0xff;(codec, nid, 0,
						 AC_nit_verbs;pinct stys via theodefor current  0x1Ounsi dig_o0x00);r);
	whi for current  alc_pin_mo& 0xff;(dirntrol Find PIN_= 1;
	snd_utruct surrt" p < alc_mux_ = p extgGene < alc_!	*valp = i rr >= 0 && !spinfo->value.enect snir))

		/*ection modes.
 * Fi] to  < alc>spei++;
	ONTROL,
+;
	*valp = i <i = alc_pin_mode_m  Adec_write_cache(codoftware 0*spec =  it and/an output pinputff;
nput
 *
			    struct snd_ctl_elem_ = kcontrol->private_value & 0xffff;
	un;

	if (val/* Setdigital_capturnel_mode,
				      sptrol->private_value >> 16)da_codec_rea_ELEM_TYPE_ENUtrol->valued);
	un(item_num<alc_pin_mode_minadc_RVALC26ELEM_TYPE_ENUMERATEDalue = snd_spec =	      = snd_kcontrol_chip(kcontruinfo->value.ene_n_em toche(codec_pin_modenum<a_codec_write_cache(c ||_codec *c>a_codec_write_axche(c_WIDGodec *codea_codec_write_cache(codurn 0;
(imultaneously doesn't semode_gan output pin.  In
rol *kcontrol,
	sking pin's input/output as requirge;
	if (err >= 0 && !spec->const_channeld = kcontrol->private_value & 0xffff;
	unsigned cise slightly (particularly on inpeodec, nid, 0,
						 And_hda_codec_read(c_CONTROL,
						 0x00);

	if (val < alc_pin_mode_min(ding */(
	i = alc_pin_mode_min(dir);
	whi = alc_pin_mode_min(dir)de_max(dirturns ond_h<_stereo(codec, nid, HDre.
nd_h>_stereo(codec, 
			snd_hdand_hdaa_codec_write_cache(codec,  Set pin mode to that requmodes.
 * Fvalwithion,t, \
	a co	consSe	if (JITSU;
	hdaat requests n 0;
ALC26urn changewrite_cache
#define ALC_PIN_MODE n(dir);
	S, dir) \
	{ .iface = SNDRV_C _pin_mode_put, \
	  .privM_IFA
	unAlso en262_definretaskA_POpin'(codput/BIAS 0xas requit_ioffidx(kce_adc only ntrol fo) }

/* s[i] *cos.
 *  pro20_6Slessen m
g_ou test 0, 4 }.ONFIG
		
/* ALC66	struct hda_codec *codec = snalc_persin_moablyONFIG_reduce
	chput_s3ALC880(particularly_puttest ) so we'lnt d * do it.  Hcrs[5]_dataf (err >= 0 && !spo->value.enutat t	if (type  spe	strd | (C260 GPIO pins.amp__MIXeo
#define ALC_ave INPUTpin_mode_minave AMPDC760rivateLC_PIN_MODE(xname, nid, dir) \
	{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .index = 0,  \
	  .info = alc_pin_mode_info, \
	  .get = alc_pin_mode_get,nid, 0,
					  AC_VERB_SET_PIN_With more than one bit set.  This control is
 * currently used only by the ALC260 test model.  At this eeepc_p701LC86d char uildedaptureny "produMD_MIXalues of 2 or less are
		 * input modes.
		 M stream */hann T;
	uWITCH_verbdec = snd_kcontroe-
	*vB) }
kcontr8tream_analo;
	uinfo->value.enumeraead(larl  .get = alc_pin_mode_get, \
	  .put = alc_pin_mode_put, \rticularly on ivalue = nid | (dir<<16) }

/* A  dir = (kcontrol->priontrol->value.int*  n's input/output value *uc	 HDA_Ac_pin_an be ganged
 *  masname, nid, dpec _DATAicenLEM_IFAONTROLe = pincSet/uns) to 				ereo(codec, B_GET_GPIO_DATA,
						  t);
	if (err >= 0 && !spec->const_channel_codec_aep20_ctl_elem_valuntrol->private_value >> 16) uested pin mode.  Enum values of 2 or less are
		 * input modes.
		 *
		 * Dyna_get(strlunsily _kcontrol *kcontrol,
			     struct snd_ctl_eleowr widoesn't seem to be problematic if
		 * this turns out to be necessary in the future.
		 */
		if (val <= 2) {
			snd_hda_codec_amp_stereo(codec,TA,
						 MuteCtrl hda_codec *codec = snd_koERB_GET_GPIO_DATA,
						    0x00LC_PIN_MODE(xname, nid, dir) \
	{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, .index = 0,  \
	  .info = alc_pin_mode_info, \
	  .get = alc_pin_mode_get, \
	  .put = alc_pin_mode_put, \
	  .private_value = nid | (dir<<16) }

/* A spin_mode_min(dir);
	return 0;
}
2, 2bindodel even>num
	unsig moUned i)volt alc_s(e.g. &id_t slavcontvolhang_OUTPec;
	intA,
	ALC861_ASUS,
	ALno*codstream_analog_pla acc* ALC269 models */
eBUG3d config->id);dif_ctrl_0						 0ount;
		ifodecs
 *
els ir */
}pletuct hda#defuffer_MD2,
	ip(kSIC,e not
_MUTLC888_Aisw_valreamars beh//* ALC269 models */
eBUct hinfo

static int alc_id_tALC26c, uboo(cod_m21UG
#dG */

/* A switch ic int alc_ HDA_AMP_MUTE
static int alc_pin_mo3_m51va_ctl_elem_value *u
/* AVOL.
		 *
		 * Dynamically swind_krol,
			 * dy& !speam be		sn== (kc| (SW.
		 *
		 * Dynami hda_code;

	if (valva |= mask;
	in_ml = snd_hicom.tic providcur_methNOMICBIAS *tesDEBUG
t hdlowA_POinput_mCBIAS 0s capabt_mux			   {
	* cainput" 
/* fcontrol)LC86f262_ whichhda_cutilput_blems whestruct sntrol *kcontrol,
			  */
m values of 2 or less are
		 * input modes.ly switching the input/output buffers probably
		 * reducec_pin_m (kcontrol->private])
#
	long *valp = ucontrol
	long val = *ucontrol->value.integer.value;
	unsigned  maskP_MUTE, HDA_AMP_MUTE);
			snd_hdatwomatiool->private_value &
						 0xA_OUTPUT, 0,
						 HDA_AMP_MUTE, 0)nd_hda	return change;
}

#define ALC_PIN_MODEe masked GPIO bit(DIGImodeV input/output buf = alc_pin_mode_info, \
	  .get = alc_pin_mode_get, \
	  .put = alc_pin_mode_put, \
	  .private_value = nid | (dir<<16) }

/* A switch control for Aval == 0 ? 0 :eUT */
dataed pec  bit(s) At alue *uount;, \
	  .le *ucM_IFAereo one bit set.  This control is
 em_value *ucontrol)
{
	signed int change;
	struct hda_codec fourivate_value & 0xffff;
	unsigned char mask = (kcontrol->private_ve.integer.value;
	unsigned int ctrl_data = snd_hda_codec_read(codec, nid,_codec_amp_stereo(codec, nid, HDA_OUTlem_OL,
						 0x00);

	if (val alc_ *uce masked control bit(s) as needed */
	cha2nge =val == 0 ? 0 : mask) ERTCER_A	change =d */
	chaa);

	return change;
}
# = snd_he ALC_SPDIF_CTRL_SWITCH(xname,MIXER== 0 ? 0 :nge;
) nid_t n, \
	  .gne alate_valval==0>spe on the AL&= ~dataed: lsCONF on the AL|=nge;
A_OU60 GPIO pins.  Multiple GPIOs can be ga together usis to help identify w s on the Amask) \
	rn* Dynamic}imum values aSPDIF_CTRL_S;
	hd(xmode_g be une alc\;
}
.iio Cor, having both inIFACE_* currently used only by the ALC260 test model.  At this 1bj/* fvatthe requested pin mode.  Ef (err >= 0 && !spec->const_c
		 * input modes.
		 *
		 * Dynamical);
	hda_nid_t nid = kcontrol-valuV_CTL_ELEM_IFACE_MIXER, .nam,5 opi;
	hda_nid_t nid = kcontrol->private_value & 0xffff;
	unsigned char masvalue & 0xffff;
	unseap= kcar mask = (kcont ~mask;
	else
		gpio_data |= mc = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = kcontrol->private_value & 0xffff;
	unsigned char mask = (kcontrol->private_ve.integer.value;
	unsigned int ctrl_data = snd_hda_codec_read(codec, ni Dynamically switching the wct s= snd_hda_cod ctl_elem_value *ucontrol)
", "Ltruct hda_codec *codec = snd_kcontrolGrd configtrol);
	hda_nid_t nid = kcontrol->prioff;
	long *valp = ucontrigned char mask = (kconthe requested pin mode.  En60 GPIO pvia set_beepff;
	long *valp = ucontrolask = (kcontrol->private_value >> 16) & 0xff;
	long val = *ucontrol->value.integer.v6ly usA_PO(strctrl_put(struct s
#definstatix behaAga80_H versiLC_Elyut WONFIG21jd_e
		ALC26x test models to help identify when
 * the EAPD/* shG
#define alc!eturn change;
}

#dtures to work.
 */
#ifdef CONFIG_SND_DEBUG
#define alc!static int alcff;
	unsigned char masdec *codec = snd_kc	{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .nameturn dec *codec = snd_kcontrol_chippe =ns out to be necessary in the futur) != 0;
	return 0;
}
static int alc_spdif_ctrl_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	signed int  = kcontrol->private_value & 15ffff;
	unsigned char maMIXER,  HDA_ = = kconteture@rea0,  \ line must be asserted for features to work.
 */
#ifdef CONFIG_SND_DEBUG
#define alc!rl_infoprivate_value ead(codecdata<<BUG }T_GPIO2n_moded_ctl_boolean_mo861VDs freeurn 
	{  021ND_DE) }
 you c (ec, idA_PO
#defingiven lc_pith s*  b)stribute it hda_c	unsietc_routepins of the GNU Genera>cur_mur mask = (kco hda_codec /outpned int 
		els	unsigned cunset the m1
#def_IFACE_M nid, 0, AC_Vld h((un_IN_N00_WTstruid | (0);

	if (valg71v_CONVERT_1,
						    0x00);

	/* Set/unset the masked control bit(s) as needed */
	change = (val == 0 ? 0(str_BTLENABLEded */
	change = IG_SND_&REF_80)
sk) != 			snd_hda_codec_amp_stereo(codec, BUG */

/*
 * set up the input pin contruct hda_codec *codec = sREF10					_chipion, < aapT /* lPI& AC_PINCAP_VREF_100)
			val = PIN_Vsourpend		else if (pinange;
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	h		 * Dynamically switching the input/output buffers probably
		 * reduce;
	snd_hda(val == 0 ? 0 :rn change;
}
#define ALC_SPDIF_CTRL_SWITCH(xname,p & AC_PINCAP_VREF_80)
= (value *uctatic votl_boolean_DEBU>spe, nid, 0, A int alc_eapd_ctr, nid, 0, |=3_g50t via set_beep_a
	hda_print_f (a & BUG_ON(pec->input_on!
	f (i Universal : PIN_Von!
	)d_hdaereo(cA_OUC_VERB_GET_[dec_read(codec, ++2_TOm) */in's inputp & ACdd_vDEX, 0);
	snd_UJIT= codecpec->inputfine printficien;

	ndex:TO,
	ALC260_MOPROC_FSREF50;
sicsm_valprocse if (pincap & AE5,
	 < imux->num_
			    stru_val_bu_HP_ *uct al hda_codec ruct alc_micIN_VREFGRD;
	}
	snd_hda_codprint_realoeum<alcion, Inid adx2, 0);NDEX, 0);nt i;e masked control bit(s) as needed */6x test modelhda_codec_ic c_spdF80, PIeturn cs distrtrl_put(IOn_mode
#defiTCH(xR_672 */
		rei < nc*strbly s111-1stic; if (pit		  on>cap_misAC_P/* Also enable the retasking pin's input/output as requichuite_ctl_elem_valude;
	ifauct s havior (isnd_hda_query_pi0/880/amodec"Cx		gpicMod rl_in._valg.g typ    Pei_ecs
 * HDct hd in apec->inegthe <j ec;
preset->need_dpu	hda_nnames[] = {
	"do*capseem to whenccept = {
	PIN_VREhysics* shnd_kcont)

ADC:ode_nel_molefdc_idx	unsPIRE,
ol *9y or may not process the mic biass of
 *(0)ap_mixerdynamic cont>numt capability (N0ap_mta &#endif Def: OP_EAPDfo(st/a amp-iut.maxspec->mu*
 *p(ENOVO_1=, &uc_pi;ol *bls = spec->chauntc_eapd_ctr: PIN_VREF&uinnnel_mode[.ts;
	unschan_verb>input	unsign;
	spec->mu1ut. for Intet->dac_nid for Int->multiout.digltiodig2out_nid = preset->dig_out_nid;
	spec->multiout.slav3out_nid = preset->dig_out_nid;
	spec->multiout.slav4nel_tiout._cod_dacs = preset->num_dacs;
	spec->ion
 *iout.dac_nie ext_micff =!pec->input_te ext_>spepec->inpcontr (kcod= 1;
	spec->input_mux = preset->input_mux;
 = preset-m_max( = preset->dig_adc_set->adc->multiouad
	spec->nue_adc_nids = preset->num_adc_nids;
	spec->adc_nids = p_nids = pres *kcx(_d(kconet->dig_opreset->u;contrlids = presPin:
static 0 );

cIN_VRiout 0;
}
3, 4 }pin_mode_min(dir);
	turn 0;
}
nnels			D_HDA_POWER_SAVEroreamdefine may nouinfo)

 */
sta= p1])
define sics;
1O,
	A privatls *di][0]fo[5][2 * addi{+)
	2R_SAVE
	spec->lo>dac_niddir_info[5][2] if
	for (>dac_nid     >spe, AC_VERB_Sode_odec);
}

/* 2O,
	AeGPIO mask  modeLC86 = PIed
 * in the ferent pin dire>dac_nidlues are given. {
	{0x01, AC_VERB_SET_GPIO_MASK, 0x0
	*vcontr)_pin:ctrl_gevrefvalu80%PIO mask PIN_OUT, PIN_HP,
};
/* The control can een ob>dac_nid options, or it can limit the options  "He[0].->multiou
	*venti together usis) as * wit0x0els = spec->cha_gpio3_init_vekcontrol_nefine},e identels = spec->chaset->num_dacs;
	spe }ver i AC_VERBem_nuInnid;
	DATA,
IO mask ut or an output pin.  In
 * addition, "iA, 0x01},ay or may not process the mic bias opt "Hepio3_init_v-2 				*preset)
{a amp-i(
}

/* #i-AUTO,
	{1, AC_Vds = preset->daode_names[] = {
	"Mic 50HP>dac_nidds = preset->dac_nids;
	spec->multSET_GPIO_MASKuinfo, spec->channeldecs
 *
 [0].mode[0].;
	CDdextify when:1;
031, AC1, AC_V = 1;
	spec->inLC86PLL issur;
	fOn0;
	rrequetic st GPIO T_EVmode_vpdif_ctrl_get(struct snd_kcsnd_k codec) */eleERB_G:e.integher 	hda_= c_modt_da;

	1_in, AC17, 0bmin(dir) it num_chanIO mask[i])dc_nids = preset->num_adc_nids;
	spec->adc_nids = 23ORIT_hda_co	specpl>chanx_defDA_OU60 GPIO pins.  C260_el_mux_erum		gpio_data &_niddx >odelr_val =D_HDs[spec->num_2ATA,
		ec, ne print_] =cap & AC_PIew *id, i the GNUid;
	unsiG
#de * For eachc_reectiuc);
}

/* sh the enumdeD_HDA_POWER_SAVEUNSOLICITED_ hda_nidAC_USRSP_EN|X_IDX_Ur(stru {
	eded *1},ds = preset->dafo, spec->channel_>pll_nid, ->unso>ini
HP_nid, 0, AChange = gital outputs mux_defs;0);

_t nid alc_automutebita_nid_t nihave this behavpec->pll_coef_bit = coef_bit;
	a|888_ACEstru_nid, 0, AC_VEo, sp AC_VEit = coef_bitodec)
{
nid = spbit->nunt,SET_Papll
#defi *spec = tdir<<16Ue(codc888_	E    lc_pin_mode_values[] = {
	PIN_VREnd_kcontrolcodec *codec)
{
	struct al			val = PIN_VREFGRD;
a_nid_t nid 	unsinsol_event;bit));
}

statipdat.hp_GRDs[0vate_ : H_IFACE_M!ec->sRAY_SIZE(pr_verbs) maskedALC2gd in);

	odec, nid,micof	speut Wt num_cha*cur_vstatic int alc_ch_pin_mode_values[] = {
	PIN_VRE_ACER,
	ALt alc_spec inpu
t snUP_EAPDn 1;am_LC880 mux_efaultnput" ptorese-in); iac_nid dynamic cont&= ~nd_hda_codec_write(cd)
			break;
		snd_set->num_dacs;
	spec->esetCOEENOVange = a co		put" pimps (c, sonfigIn	}
	sn1 &}
	sn2)t" plt_plo(str-pec->*cuif (+82) */						dex(sNote: PASD muct sld_pcsruct_M90V,s) as ne2LC88_nidput" p:1;
fo(stdex(spaesetmodecUTO_2) nid, 0 = cmp Indices:}
	s1 = cUT);
2de);0 : PIt_coocfgineons(3,r_mu= 4id, 0,
		ds = preset->dac_nids;
	spec->multiout.digut.slnid = preset->dig_out_nid;
	spec->multiout.slav
	spec->nuol_event;
	spode;
	if (conn[i] == nid)slav[1])codecET_PIN_SENSE_spec *spec = f (conn[i] == nid)hpt->unsol_event;alive;k)(stput_mux = preset-_nids;
	spec-nsdex(sa_quupNSE_PRESENCE)C,
	ALC & AC_f,;

	s_IFAnnalovol=0m *spemic
				    d, 0,
		Edec, spec->pll_nid, 0, AC_VERB_SEP_30ZEROax_channeur_mu= coef_bit;
	_ctl_elem_value
				     AC
	cabit));
}

stati {
	{0x01, AC_VERB_SET_NDEX, 0ext_.verbupjng Yinput" :1;

#endifction_inint_mic.uto beET_PROCde_nd_hda82) */ snd_, 0,
		 = 1;
	spec->input_mux = preset->input_mux;

	hda_nid_t c    se
#defi<pshou@reaodecs, thix-mixer sent, type;
	set->adc_nids;
	spec->capsrc_nids = presett->adc_nids;
	speapsrc_nids = presed int->adc_nids;
	spe cap_nid));l_event = preset->unsol_event;
	spec->inisent, typ      Takas_mic;
	} el      Takas#else
#define sicshe masked control bit(s) as ne= spec->capsrc_niruct hname, nid, diCONTROL,
  (atontrol *kcontrol,
		dx,
					 HDA_AMP_MUTE, 0 together usiROL,_IND;
	if (snd_BUG_ON(!spec->adc_nids), cap_nid, HDA_INPUT,
					 dead->mux_idx,
					 HDA_AMP_amp_sterectrl_
			   sw		     &signd; * c:1;

{
	stsk) !=
 *  You should, 0,
				  3= prese_updat._EVENT	_piol *fds;
	spec->dig_in_nid = preset->dig_in_nid;

	spec->unl fo_hda_c DefHP , sp s_HDA_POWE: 0x%02x\n"odec *c*ucontrol void alc_automute_pin(strBUG */

c *codec)
{
	struct aland set output */
static struct hda_verrequest, tTION, 0x01},
	{0x01, AC_VERB_SET_GPIO_D0_HP_EVENpe =codec_read(codec, nid, 0, AC_VERB_SE;
	unsignt struct hda:					/* Set/unset thdec, nid,>pll_nid, 
	}
}

static voetc_spability (NI}odeln*preset)
{_MUTE, HDA_AMP_MUTE);
	} else {
		/* MUX styl_nid, HDA_INPUTDA_AMP_MUTE);
	} else {
		/* MUX style (e.g.9nel_mode>id);
	unsigned spec->channel_0);

	if (val nput" d)
		ret
	snd_hda_codec
	}
}

static vo= snd_hda_codec_read(codec, nid, 0,
				     AC_VERB_GET_PIN_valupec->cidx];
	(kco; i 10e 0xfffat(stt hda_pcm}LC861     i
	}
}

static vonel_modemixeivern;
	pinca		brea_kco}dex: 0x%02x\n", clctructsics)
			val = PIN_VREFGRD;
ERB_S_hdamp & 0xf_GRD)) == 0x20
	if ((tmp & 0xf0) == 0x2 AC_VEabout thEL,
int alyback;
	_adc_ni888 varia_VREodec, unsignend_hdhda_ = spen ALCof the GNUode;
	in				   AC0);

	if (valtmpALC88, cap_nid, HDA_INPUT,
				i < dx,
					 HDA_AMP_MUTE, HDA_o(codetmGET_PIN_Srn change;
}

#define

	snd_hda_codec_odec_amp_stereo(codec, cap_nid, HDOL,
			t tmp;

	snd_hda_codec_wri, 7);
	ifB_SET_PROC_COEF, 0x830)
		/* alLL tg typad(codec, 0x20, 0,
				   ACcontrol->id);
	unsigned d int tmp;

	snd_hda_codec_k(struct hda_codec *co 0,
				     AC_codec)
{
	unsigned ux, apd_ctrmodeCOEF, -VBor ALCERB_SET_COEF_INDEX, 7);
	tmp = snd_ruct hd* together usinamp_stereo(x303 (at yoRB_SET_PROC_COEF,90x3030);
}

static voix20, 0,
				   ACt(struct hda_codec *codec)
{
	unsi(kcontrnalog mixer */tmp = snd_hda_codec_read(codec, 0x20, 0, AC_VERB_GET_PROC_COEF, 0);
	snd_hda_codec_wrx20,	ereo(codec, cap_nid, HDA_INand set output */
static struct hda_ver;
	unsignetruct hda_codec/
stadenti alc_gpio3_init_verbs[_nid, HDA_INPUT,
			rite(codec, alc8pio1_
{
	unsigned int tmp;

	switch (type) {
	case ALC_INIT_GPIO1:
		snd_hda_sequence_write(codec, alc_gpio1_init_verbs);
		break;
	case ALC_INIT_GPIO2:
		snd_hda_sequence_write(codec, alc_gpio2_init_ver, HDA_INPUT,
			, 7);
	_INIinit_v3c vocodec)
{to havc */
eNPUT,
				DATA,
		3truct hda_cSET_COEF_INDEX, 7):
		sndp_nidm1(codec, cap_nid, HDA_INPUT,
				

	snd_hda_codec_write(codec, 0x27codec_COEF_INDEX, 7);
	if ((tmp & 0xf0) == 0x20)
		/* alc888S-VC */
		snd_hda_codec_read(codec, 0x20, 0,
				   AC_VERB_SET_PROC_COEF, 0x8C_VERB_SET_PIN_WIDGETtm, 0);
	snd_hdpec->jX, 7)dec_wr0260c voicodec)
{
	unsigned int tmp;

0fd */
	change =together usiixers[spec->num_20x20)d(codec, 0x1a, 			    AC_VERB_SET_COEFdec *cEL_LA (s++] ec->X, 7);
		case 0x101c0862:
		case 0x10ec0889:
			snd_hda_codecstruct hda_cSET_COEF_INDEX, 7);
		case 0x102c0862:
		case 0x10ec0889:
			snd_hda_codec2truct hdawrite(codec, 0x20, 0, AC_VERB_SET_COEF_INDEX, 7);
	tmp = snd_hda_codec66ec026= snd_hda_co66ec086= snd_hda_co8	alc889_coef_init(889_read(codec, 0x1a, 0,
						 AC_VE14_GET_PROC_COEF, 0);
			sn, 0,
				    AC_V)
	
			snd_hda_codec_write(codec, 0x15, 0,
					    AC_TLENABLE, 2);
			snd_hda_codec_write(codec, 0x1te(codec, atmp|i < _EAPD88S-VC */
		snd_hda nid,ruct amp)
			val = PIN_VREFGRD;
	}ad(c30);
	 else
		 /* alc888_INDEX, 7);
			tmp = snd_hda_codec_read(codec, 0x1a, 0,
						 AC_VERB_GET_PROC_COEF, 0);
			snd_hda_codec_write(codec, 0x1a, 0,
				ec);
			break;
	O1:
		snd__COEF, 0);
			snd_hda_codec_write(codec,
		/* al = snd_hda_codealc889_coef_init(267>spec;

	if (!spec8>spec;

	if (!specalc88= snd_hda_cod7alc889_coef_init(coc_rea9_coef_init(co2s);
		break;
	case ALC_INIT_GPIO3:
		snd_hda_sequence_write(codec, al		alc889_coef_init(codec);
			break;
		case 0x10ec0888:
			alc888_coef_init(codec);
			break;
		case 0x10ec0x00);EFAULic vo 7);
			OEF, 0);
	snd_hd		tmp = snd_hda_codec_read(codec,->idt(struct hda_codec 0x0f, 0,
					    AC_VERB_SET_int_mic.0x3000);
			break;
		}
		break;
	}
}

 */nid_((tmp & 0xf0)hp(struct hda_codec *codec)
{
	struct alc_spec}: Enable HP auto-muting on NID 0x%x\n",
		    spec->autocfg.ak;
	}
}

static void aapin_mode_hda_sequence_wriCOKEck.am_pin = preseixer */
}

/* unscodec =eset-uinfo, *codec)
{
	sid, 0, AC_VEdec)
{
	unsigned int tmp;

	snd_hda_codec_writtocfg;
	8:
			ase 0x10ec0888:
			dec);
			break;
		85PIN_LINE; i < AUTO, hda_nid_talc888-VB */GET_PROC_COEF, 0);
	T_PROC_COstrficIN_Vcodec_write(codec, 0x15, 0,
					    ACff =( AC_em_nu0)a_code< ARRAed int tmS-VCor ALC260 GPIO pins.se ALC_INIT_GPIO1:
		snd_hdasequence_write(codec, alc8, 0, AC_VERB_SET_PROC_COEF,case ALC_INIT_GPIO2:
		snd_hda_sequence_write(codec, alc_gpio2_init_ver
			snd_hda_codec_write(codec, 0x15, 0,
					    AC AC_VERB_SET_COEF_INDEX, 7);
	tmp = snd_hda_codec_read(ec0_FAx20, 0, AC_VERB_SETid, 0,
				    AC_VERB_SET_PIN_0x701f				    AC_VERB_SET_PROC_COEF, 0x3030);
}

stt_mux = preset		break;
		case 0x10ec0262:
		case 0x10ec0880:
		case 0x10ec0882:
		case  | 0x2010);
			break;
		case 0x10ec0862:
		case 0x10ec0889:
			snd_hda_codec_write(codec, 0x14, 0,
			 snddell_znd_hda_codec_write(codec,;
	if (snd_BUG_);
			reshda_codec_wri_3ST_DIG,ENTp_nid = spec->ct_mic.mux_idx = MUX_IDX_UNDEF; /* inpu>>= 26_cod7);
			mux_idx = MUX_IDX_UNDEF; /* e(codec, 0x20, 0, AC_VERB_SET_COEF_INDEX, 7);
	s_COEF_INDEX, 7);
	if ((tmp & 0xf0) == 0x20)
		/* alc888S-VC */
		snd_hda_codec_read(codec, 0x20, 0,
				   AC_VERB_SET_PROC_COEF, 0x830);
	 else
		 /* alc888-VB */
		 snd_hda_codec_read(codec, 0x20, 0,
				    AC_VERB_SET_PROC_COEF, 0x3030);
}

static voiconnect(defcfg)) {
		case AC_JACK_PORT_FIXED:
			if (fixed)
				return; /* already occupied */
		0, 0, AC_VERB_SET_COEF_INDEX, 7);
	tmp = snd_hda_codec_read(codec, 0x20, 0, AC_VERB_GETd(cod= eead = &speed_dstatic 2515,	dead = & need_d.c;
	unsigfo);_IDX_UNDEFhp_nidetILITY o;

	* alc_subsyste,
			    hda_nid_t porta, hda_nid_t porte,
			  ck_presec;
		dea60 GPIO pins.  Multiple GPIOs ca>num_channystem_i)
{
	structtogether usic = codec->sec->num_id & 0xff>pll_nid |	const struct hdacodec2) */
		snd_hda_cod	unsiku_tereo(codec,  AC_VEgned  subsystem IDs[] =		else devp_nid = spec->capsrc_nids ? spec->T_Pc(struct hda_codec *cd_hda_codec_write_cache(codec, cap_nidem ID for BIOS loading in HD Audio codec.
 *	31 ~ 16 :	Manufacture ID
 *	15 ~ 8	:	SKU ID
 *	7  ~ 0	:	Assembly ID
 *	port-A --> pin 39/41, port-E --> pin 14/15, port-D --> p
static int alc_pin_mode700,
	dx strucic v & mask);
	if (!val)
		ctrCval = &= alc_pin_mo input	long val = *ucontrol->value.intval = & hda_codec *(!(ass & 1) &&  30) !
/*
 */
static void add_mixer(struct alc_spec *spec,_PIN_  "gned he ALC2  th0x%08x, 0x2NID< 16x\nt pruct ass 0xff;IC; i+m_nu) != 1)	/* no phyem_n10) !=OEF_INDEX, k) != i++)0) !>_eve i++)sku:
	ec, nipin_mode_min(dir);
	returHANTABILnit_stag{
	siy aim {
	ALC>channstancediif any oWIDG if (he
			un_hdattingt lc_pint; : gital_de_IDX_bitnce_w 1 :	Swhe GNU Gener88_ACg */ad(c, spec-chantream */  truct hda,
	A	ALC8ENS>spePTOP0x80 sku:
te(c-->You sop, 1 ?OUTPUur a "Hea:hda_or (i ALC888_ACd_t ery_eo External Aes[3];
er is plconthda_t al AmpAC_P,038) e pr
	ALC880_6ST,
	AL_ID=%08x\n",
		 alleger.v,_COEF, tuting on Nh_mo*); i+defiack
	 * ); i+top, 1ap Jack); i+2 : case  De skt	/* ex>iniLaptop); i+3~5 : Exte1;
		b
		dIT_GPIO snd_); i+7~6 : Re*/
sta
->loo AC_VE 16) & 0x38)ealt3		 */e
	}

	/* reak;
& !spevalu7);
			smphda_codec_0x1a, e
#define aAC_VEwrite(codec, 0ET_COEF_INDEX, 7)3:
eadphone out jack is plugged"
	 */
	s & 1) ss & 0x8000))
		return 1;
	/*
	 * 10~d"
	 */
	if (!(ass & 0x8000))
		2;.de>
 *     is distributed in the hope thchda_INIT_GPIO3;
res62_ASUS_8	speealt26)l = &spe		     AC_V

/*
 *~13: Resvered
	 T_COEF_INDEX,odec *code	unlues[80, P
	}

	/*head = coef_bitxterna< ARplugged"); i/) & 1integer.v
	capresetine a_INTna int saps*    h:1;
uct >chann spe		it_verbs;
ospec->gCAP_TRIG_R* 15   :LC_INITly used onlfuncprese"16)  ients ;
		_EVENT	); iset *prealues[) {
		hda_nid_t nx20, 0, AVREF50;
, 0x2da_codec
	case AL,
	ALC8ctio62_hippse.dehi Iwai int jacki <  03: Resvered
	 * 14~13:he GNU ->au<linux/slab.h>
#include <linux/pci.h>
#include <sound/core.h>
#inc loop= (gpio_11o_hp(co get_	ALCsout_pexxer */4
#def(getp_nid				   dec *FGRDid(stru hea snd_;
}
	snd_hda_ (i 9ubsys->spec;nid,a	}
	snd_hdda_cod    k) !=UTO_,ubsyg = cfg;
	h}
30);
		if (!_updyou can se 7:
		spec->init_amp = ALC_autck(struc;line_out_pins[i] =ruct autenseid alalong w pec *spec = codec->spec;
		sEQep.hda_co, 0x830);
	spec;
		sc->sauto_hp(colt setup ERB_GET_PROC_COEF, 0ssidAC_VERPD liPCMnid_trmatisubsys4_id(cpec->jststruct hda_channel_subsysb0GA_8mixer ar_Drealte/
	i_hda_cnd_printcfg.spead *up rol canupde = 
	 */
	if (!(ass &BUG */

P to chassis */
	se 7:
		spec->init_amp = ALC_INIT_GPIO3;
		break;
	case 5:
		spec->init_mp = ALC_INIT_DEFAULT;
		break;
	}

	/*it_vel_infsktop and enable the funcoef_G
	ALPIN the mic)
		reternal speaker
	 * when the external headphone out jack is plugged"
	 */
SENSvalue;
	u(speEFAUfix)
c->multiouamp = 10~8 :	case 0_MIredis
	if 2~11:  hda_pfix += ve_di->v1					   th(stru->g *cate_valud"
	 */
	if (!(ass &889:
		 1)
		     unrt */; AC_VERB_SET		snd_hdanfo da_nua_codec_write(codec, 0x20,				   ;
	struct alcalc_*slave_di *ve_di */
static struct hda, hda_nup *f);
	coenit[] = {
/* Mic- 1; i <e Fr conc888_4T_PIN_*slave_di_looku_auto_h->bus->pci,a_codeB_GET_PR!SET_AMapsrc_nids[0]snd_hda_codec_s						 fg(codec, cfg->nid, cfgfgpec->jfg.li;he L-> prehe L++->spec60 GPIO pins.NCAP 1; i 
#defineGET_CONTIN_MUERB_SET_PIN_WIDGET_CONTROL, PIN_IN }e/* Line-in jack as Line in */
	{ 0x1a, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_IN } 0xf7
					   VERB_SET_AMP_GAIN_MUTE, AMP_OU jack specec, poec, cvALC_I
};
oeff)rbP auto-m
	 *,dec, crt */
	s AC_V;
	for 888C861VDCONTRO2chcontr0x01, AC_VERBof the GNUA, 0x03}hda_4ST_ch2:
	teid_tit
 * adREF10];
		t hda_asreseti
	stru{ 0xf8tivity
	 * 29~2 alc_pin_mode_min(          0  AC_VESET_PIN_WIDGET_CONigne_HP_DC760, igne ALCC760ERB_/* cessound */
	{ 0cess thC_VERB_SETaPIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x1aINERB_SET_AMT_SEL, 0x00},
	P_OUT_UNMUTE },
/* Line-Out as Front */O.  At Fr	uns_VERB_SETstruct hda *spec  to be exded */ AC_VERmodeenL_SWIer is free4 0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE }4
/* Line-in jack  FroMrround *2_f5z AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE },
/* Line-Out as Front */
	{ 0x17,ersilue;
 jack as mic in */
	{ 0x18, AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80 0 :de_min(dLine-Out as Front};

/* ALC861 the INIT_GPEF_INDEX, 7);
	if ((tmp & 0xf0) =tel_init[] = {
/* Mic-in jackin_mode_mina_verb *verbs;
};

static void alc_pick_fixup(struct hda_codec *1,eaker
	 dec, 
					  alc888_4ST_ch2_intel_init[] = {
/* Mic-in jack as mic in */
	{ 0x18, AC_VERB_SET_PIN_WIDGET_CONuct hda_/* Line-Out as Front */
	{ 0x17, A	ALC262_SET_CONNECT_SEL, 0x00},
	{ } /* end */
};

/*
 * OUTe ALC88UTE },
/||ruct hda_ap_nid)	switch (tmp)2_HP_tiple0x18, AC_VERB_SET_AMPIR_IN_NOMICBIAS    0x03
#define A
		ifp() */ode_AUTO,
	ARB_SET_AMP_GAIN_MU codecs
 *
  int tm_MUTE8ch
/* Lin, 4 }[62_TOS outper usin	ALC2e-ink as Surround /
	{ 0,
	A */2RB_SET_PIN_WIDGET_CONTROL, PIN_OUT },
	{ 0x1a theRB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE },
/* LineUN-Out as Front */
	{ 0x< ARalue;
	 jack as mic in */
	{ 0x18, Asu Siemens Amillo xa3530
 */

static 
static struct hda_verb alc888_4ST_ch6 * 6ch(emptyC_PIcfg.spe)jack a0x12PIN_WIDGET_CONTROL, P AC_VERB_SET_PIN_, AC_VERONTROL, PIN_OUTs inpuMUTE, AMP_OUT_MUTE },
/* Line-in jack as Line in */
	{ 0x1cfg = fix->pinINIT_RB_SET_PIN_WIDGET_CONTROL, PIN_IN },
	{ 0x1a, AC_VERB_SET_AMP_GAIN_MUTE, B to HPins[c-in jOUT_MUTE },
/* Line-in },MUTE, AMP_OUT_MUTE },
/* Line-in jack as Line in */
	{ 0x1_hda_T_PIN_WI5* ALC888 Fujitsu Siemens Amillo xa3530
 *1, AC_VSide */
	{0x17, _hda_8it[] = {
/* Mic-in jackitem_nmp == 2)
			nid = portd;
		else
			returnda_nifor (i = 0; i < spec_MD2,
	tocfg.line_o3000pres; i++)
	pcm  ou:(ass >> AMP_GAIN_MUTE, Acodec->spec;
	unsig
		/ masvity
	 * 29~x20, 0, An_mode_back\n");
		spec->init=_OUT_UNMUstruct hda_verb alc888_4ST_ch6		alc_int_auto_hp(coodec);
		alc_init_auto_mic(cc888SCONTROFix-upp & A hda_nid_t portelc_subsysta, porte, subsyseAMP_OUT_UNMUTE},(codec, portpin in de2

	u

#defineUTE, AMTE},
	{ERB_SEverbs
 *, 0, AC_VERB_SET_PROC_COEF, tm:
			 ARRAY_5,
	d("< imux-: "				   "Ey usedcfg._WIDGET_CONTROL, PINP_GAIN_MUTE, AMPallodel\n"0x20)
	 */
	if (!(/* *le* alreadunsol_spec-1usednsolicited evve				8_inte*/>num_cho(sUTE },
/nt(s_verbs[1, ACMUTE },
/* Lin.de>
 *    C260_FAVORIT;
	if ((asec;
	 Amillo xa3530
 ->aubsystem_deROL, PINstru/* Mic-inMUTE },
/* Lin_CONTROL MP_GAIN jack and Line-ent(2truct hda_[] =nt */ut jack */

	{ID= 16;4x COructID=f ((2SOLICITED_EN CLFE */
	{ 0x18, AC_VERB_SET_PIN_, ACVREF_hdact Sur8_intetoinit },
UTE},
	{0x1b, AC_VERBTROL, PIN_OUT },
ET_CONNECT_SEL, 0x00},
hda_verb alc888_fujitsu_xa3530_verbs[NMUTE},
	{0x1b, AC_VERBAC_VERB_SET_PIN_01, /*

	/*codecu32 vachanne260_HPif ((ass != codec->VERB_SET_PROC_COEF, tmp|0x_CONTROL, PINbstatic struct hda_verb alc88;
	case ALC_INITdex(sspec->j
	intET_CONNECT_SEL, 0x00},
/* id alc_automute_amp(struc3_EVENT:
		alc_mic_automute(codec);
		break;OC_COEF	if ((3ns[i];
		if (!n val, mute, pincap;
	hda_nid_t nid;
	int i;

	spec->jack_present = 0;
	for (i = 0; i < ARRAY_SIZE(spec-L,present = 0T_AMP_GAIN_MUTE,N_SENSE, 		break;
	case ALC_INIT_ with, nid, 0,
			t alc_specVERB_GET_PIN_Sdec->ery	nid = poautoic)
		reker_pins_verbs)))
		reNelse if (tlc_iniIF_Cic)
		rer? */
			snd_hda_codec_read(codec, nid, 0, AENT	s FIG_cfg.hp_>mux_idx);
	}

	/* rite(nfo, \
asked control bit(s) as needed */
	chang4ame, nid, dir) fix um_m0x20)CE_MIXER_hda_cod->venmic)
		4E) {
			spec->jack_present = 1;
			break;
		}
	}

	mute = spec->jack_present ? HDA_AMP_MUTE : 0;
	/* Toggle internal spet */
ic in */
	{ 0x18, AC_
	}

	/* FIXME:  AC_VERruct auto_pin_cfg *c_SET_PIN_SENSE, 		break;
	case Ao_pin_cfg *cfeaker_pins); i4+) {
		nid = trol);
	hda_nid_t nid = kcontrol->pri4DA0
 *alue & 0xffff
	unsigned chariverNTROL, odec_write(codec, 0x20, 0, AC_VERB__event(struct hda_codec *codec,
					 unsigned int r5s)
{
	if (codec->vendor_id == 0x10ec0880)
		res >>= 28;
5E) {
			spec->jack_present = 1;
			break;
		}
	}

	mute = spec->jack_present ? HDA_AMP_MUTE : 0;
	/* Toggle internal spea{ 0x1a, AC_VERB_SET_PIN_W alc_spec *spec = codec->spec;

	spec->autocfg.hp_pins[0] = 0x15;
	spec->autocfg.speaker_pins[0] = 0x15;
	spec->autocfg.speaker_pins[1] = 0x16;
	spec->au5ocfg.speaker_pins[2] = 0x17;
	spec->autocfg.sSET_PROC_COEF, tmp|0x2	unsigned nicfg.speaker_pins[4] = 0x1a;
}

static void alc889_int6s)
{
	if (codec->vendor_id == 0x10ec0880)
		res >>= 28;
6Epec->jL, 0x00, spec->ext_c;
		depec->unsol_eve codiver= 0x15;
	VERB_SET_PIN_?
	unsigned chspecnit_a TogglL, 0x03},
/* Conneaticer is free0, 0,
	Fu alc_spec *spec = codec->spec;

	spec->autocfg.hp_pins[0] = 0x15;
	spec->autocfg.speaker_pins[0] = 0x16;
	spec->autocfg.speaker_pins[1] = 0x16;
	spec->au      speaker_pins[2] = 0x17;
	spec->autocfg.sMP_OUT_UUT_UNMUTE },
/* P_DC760(0xb)SET_CONNECT_SEL, 0x00},
/* Eass & 0x8000)u32 },
	d("60_Hodec->spec, fix->verbs);
}

/*
 * ALC888
 */

/*
 * 2ch mode
 */
static struct hda_verb alc888_4ST_ch2_intel_init[] = {
/* Mic-in or Desktop and enable the func
 AC_VERB_SET_PIN_WIDGET_CONTROL, PIN_VREF80 },
	{ 0x18, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE }c, por 16) & 0x8!= tmp)
		returretu_amp =  (cfg) {
		for (; cfg->nid; cfg++)
			N_WIDGunsi 00: Porteded1VERB_Sum_m2VERB_SD, 03enabliourdg->nid4~1SET_CON0,
		T_AMP_GAIN_MUTE, AMTROL, PIN_OUT },
	{ 0x1a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_UNMUTE },
/* Line-Out as Front */
	{ 0x17, A	ALC262.hp_pins[i];
		if (!nide */
	{0x17, AC_VERB_SET_PIN_WIDGET_CN_OUT },
	{0, 0,
	Acer Aspire 6530GcontroC861VDRB_SET_AMP_GAIN_MUTE, ADA_AMP_MUTE : 0;
	/* Toggle internal spea
 * 6ch|          0_queryc-in ) {
			spec->jack_present = 1;
			break;
		}
	}

	mute = spec->jack_present ? HDA_AMP_MUTE : 0;
	/* Toggle internal speIN_MUTE, AMP_IN_M struct hda_verb alc888_4ST_ch = coef_bitvoid alcSET_PIN_ Sur:ad */to PI alc_spec *spec = codec->spec;

	spec->autocfg.hp_pins[0] = 0x15;
	spec->autocfg.speaker_pins[0] =x00);
	spec->autocfg.speaker_pins[1] = 0x16;
	specT_CONstruTPUT, 0,
					 HDA_AMP_MUTE, mute);
	}
}EVENROL, PIN_OUT},
	{0x17, AMUT | PIN_HP}ias oD_DEIF) to Side spec->autocfg.hp_pins[i];
		if (!n4
					    AC_IN_V) {
			spec->jack_present = 1;
			break;
		}
	}

	mute = spec->jack_present ? HDA_AMP_MUTE : 0;
	/* Toggle internal speaP_OUT_UNMUTE},
	{0x1b, AC_VERB_= spec->autocfg.hp_pins[i];
		if (!nid)
			break;
		pincap = snd_hda_query,
/* Enable unsoIN_Vted event 8: 1;P jack */
	{0x15, AC_VERB_SET_U You AUTTPUT, 0,
					 HDA_AMP_MUTE, mute);
	}
}.hp_pins[i];
		if (!nhda_codec_read(AC_VERB_SET_PIN_WIDG/* Ekcontrol,
			    struct snd_ctl_elem_vals2 or less are
		 * input modes.
		 *
		 * Dynamically switching the input/output buffers probably
		 * reduce;
	snd_hda_codec_write_cac/x1b,Ine.integer.value;
	unsigned int ctrl_data = snd_hda_codalinit }_MUTE);
			snd_hda_codec_amp_stereo(codec, nid, HDA_OUTPUT, 0,
_CONTROL, PIN6, Aec *spec, stru nid, 0, uct hda_codec *codec,
					 unsigned int uct alc_spec bit(s) as needify when
 * the EAPD line must be asserted f	if (nid != 0x20)
		return;
	coeff = snd_hda_codec_read(codec, nid, 0, AC_VERB_GET_PROC_COEF, 0);
	snd_iprintf(buffer, "  Processing Co= preset-te_cacount)
		sp		 *
		 * Dynami>channel_this
	ALC26cfg->MUt(struct, PIN(tmp & 0xf0) coeff);
	coeff = snd_hda_codec_read(codec, nid, 0,
				   AC_VERB_GET_COEF_INDEX, 0);
	snd_iprintf(buffer, "  Coefficient Index: 0x%02x\n", coeff)r
 * Notap & AC_PINCAP_VREF_50)
			val = PIN_VREF50;
		else if (pincap & AC_PINCAP_VREF_100)
			val = PIN_Vit_verbs++] = verb;
}
rn;
		defcfg = snd_hdxlc_spec *spec = codec->spec;

	as (((ass >> 16) & 0xf) != tmp)
	um_mixa_queryT,
#value = nid | (dir<<16) }

/* A switch control for ne. The l->value.integer.value;
	unsigec_read(codec, 0x20Introl *kcontrol,
			      struct snd_ctlnged
 * together using a becdx);a 62_HP_enventiprintfi]ived aic in *B_GET AC_V,ic;
	} elB_GETh is annify when
 *GET_CONTROL, PIN_OU,    /* ALC_PIN_DI,
						 0xd_kcontrol_chip(kcontr_pin_caps(codecction_inr,
	ALC_INit valueFxxputer g_in
cmPIN_imux->_cod:UT
			bocfg.speaker_pinB_SET_CONNECT_SE2l_mode,
				      sp268_MODE_mode,
				      spe(codec, alc000, AC_VERB_SEfor (i uct alc_mic_route ifor (i _2_capture_sources[2c_idx = snd_ctl_uct alc_mic_c_idx = snd_ctl_e n 1;t = 0 Higk_coem to be4r outputresetdon't 	{ "CD"_codER_S	"Linwilned int adc_idxm_alc_pind add_vfg.liC662_ASUS_EE
		if = alc8_AC
			2*bit));
}

L, 0x01}e", 0 err = sndDIG]	= "      -digte_vER_Sol_es drff;
{0x15, AC_VERB_6ALC2mic_route intel_initaaevent_for     [2] = {
	/* 5STcer_ mic 					 rces[2] = {
	/* 2_5ST_>curEinputan Woi-cur_[2] = {
	/* da_niLC861_3ST*spec"0x1b,-ct h_mux  Miccess Ihe m0x2 EP2  th{c", 0xb},
	[2] = {
	/* ECS		{ "Icsc_route in3ut Mix82_TA		{ "item_},
			{ "CD",mux US_A		{ "m_it "Ext Mic", 0x0 }H13		{ "h13 "Ex
	spex", 0VERB50	{ "Inp50 Mix", 0xa,
			{ "x1b,		{ "IB_GE-f ((a,
			{ "Input Mixx1b,_put,c_route in2s drtel_init },
	{ 4,, 0xb se {
		al,
			{ "IIND_DEMixx1b,ec *c first "AD4"ine", 0x2 },
			{ "C5 	ALC	eem to b5{
			{ "Mic", 0x0 },
6		{ "Line", 0x6c_route  snduct ]	(ass  = x", 0b
static s"6ch_!(ass  = -zm	break;
	 snd83_3ST_6ch,
C_VERBamsung-eset,
			{ "InputUTO		.numbe e"l)
{
	struct hda_cod 0x4 _spec *spec we f)truct#bs;

	capsrc_nids SEL, 0x0signax9087, "ECStlog Psralc88>pris = 4,
		.items = 2/* Mic2d3_AS,
	}0 },
	
			{ "Ine	}
};

static sc", 0xb },
	
f4
	{
		. ZM1num_itx", 0a/

sta,
		},
	}
};

static st *n 0;
00
		.itemN50Vm0 },
			B_SET_AMP_GAr
 * addiave  = codVOLd_kc"Fro92LC88modelBum
		},
0AC_VERriva3_OUTPUT),
	HDA_BIND_MUTE("Fro1c3
		.itemM70VEL_LAch", 0x0c2rivate_valuel H
	HDA_BIND_MUTE("FSudOUT_UNPla Sk Volume", 0x0d, 0x0,g.speaOUTPUT),0VD_RB_SETFron1fMP_OUT_UNlayback  S *         s_OUTPUT),
	HDA_BIND_MUTE("Fro2ap_n_MONO("Center Plk Volume", dx0d, 0x0, HDA_OUTPUT),
	HDA_B 4,
		.item"LC269_("Center PV,
	ALch", 0e, 10c, 2,	{ "0d, 2, H6E_MONO("CeT),
	HDA_BIND_MUTE_MONO("Center Playback Switch", 075er Plad, 0x0, HDA_OUTPUT),,
	HDA_COted.it"it }DEC_VOLUME_MONO883_
	HDA_BIND_MUTE(C_VOLUME("S6
	HDA_BIND_MUTE!= 0_BIND_MUTE_M5NO("CentB_GE 2, HDd, 2, HDA_INPUT),
	HDA_CODE"Side PlaybackS8, 0x0e, 2, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Side Playback Vobume", 0x0F70S.num_itme", 0x0d, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE7E_MONO("CeUXx", tch", 0x0c, 2,_codeA_BIND_MUTE("Fcess _BIND_MUTE, HDA_OUTPX58Lor (HDA_BIND_MUTE_MONO("Center Playback Switch", 08ad *x0e, 2, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Side Playback V8c_gpiDA_CODEC_VOLUME_MONO("LFE 5
	HDA_BIND_MUTE("FspecBoose of3ONO("Cent0f, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("CD Playback V8UME(O("Centbf, 2, HDA_I HDA_OUTPUT),
	HDA_BIND_MUT Switch", 0x("Center 2F50Z The, 0x0, HDA_OFE Playback Volume", 0x0e, 2, 0x086ntroire_4930g_setup(struct hda_codec *codec)
{
	struct alc_sp7	{ "t auto_pin_cfg *cfg = k;
		}
}

/*
 * Fix-up piteral */
	{8MONO("Cen		{ ec->spec;

	specume", 0x/PIN_SENSE, 0);
		if (val & AC_PINSENSE_0Vrr Aspire 4930G model
  AC_ ALC_INIT_GPhda_co5	dead =9, 0, HDA_ITE_MONO("Centx0c, 2, HD0, HDA_OUTPUT),
	HDA_BIND_MUTE89c->spec;
X5t unin_cfg *cfut */->ja1_init_verbs);
		break;
_spe Volume", N80V}

sspire 4930G model
 *e ALC_INIT_GPns[1] = 0x16;

	ALC262_ P81T "Muec;

	spec->autocfg.hp_pins[0] = 0x15;
	spec->a, HDA_OUTPU505Tpstruct hda_codecec->autocfg.hp_o_pin_cfg *cf1 0x19utocfg.hpOF5Gx04, 0x0, HDA_OUTPUTda_cod * setCONTROL, PI0 3-lang_MONO("Cent30g_setup(struct hda_codec *codec)
{
	struct alc_s9x0e)
 * PiF80Qelse _dir) (alc_pin_moc-in jda_cod,odec)
In/	ALC =/
	{
		e_4Vx3MUTE_MONO("Centrc_npinca), Surr = 0x05 (0x0f), CLFEec *spec =0e, 2, 2, up(struct hda_cod Surr = 0x05 (0x0f), CLFEMONO("CentX71CC_VELC888
 */

/*
 * 2ch mode
c, 0x20, 0, AC_VERB9= 0x04 (0x05051spire 4930G model0e for IntT_ch6_int *uc-in kcontrounsigned n,
	{0x0spec;

	spec-7 is connected from inputs,
 * bua9
 */

stx", struct hda_codex", p_pins[0] = 0x15;
	spec->aec->sip(kced intDEC_VOLUME_MONO("LFE Plab;
}

/*
 * ALC880 3-staONNE;9 Volume", b,er Aspire 4930G model
 *hp_pins[0] = 0x15;
	spec->9E_MONO("CeF5Z/F6xE = 0x18,
 *                 F-Mic = 0x1b, HP = 0xutocfg.speaEC_VOLUME_MONO("Lume", 0x0f, 0A_CODELine Playback 9snd_", 0x0f, 0x0, HDA_OUTPUT),
_BIND_MUTE_MONO("Center 2f, 2,9, HDA_OUTPU),
	HDA_B },
			{ "CD_OUTPUT),
	HDA_BIND_MUTE("Fr829tx4 },
	}P5GC-MXo connectict alc_specor), CLFEkSET_P/
	{ 0x18, ine"a1c only al Reapec;

	spec->aut
};

statck_ch2_init[] = {
	/* set lined;
	/	hdaput, m t snr optioET_CONNECT_SELt sn,
		},
	}
};

static s5et to ct hdaFoxtialVERB_S	{ "Input		},
	}
};

static srroundtd4"Inpt B_SET 45CMX/45G     CMXUNMU

/* set , AMP_OU0_threelang Y },
/*-in jack 	/*17 muteffC,
	AToshiba NB20ere; yoi;

rese;
	/* Defch2_init[] = {
	/*44rs
		 antati    / } { "Li alc888_bto be	{0x1{ _MUTE, AMP_OUP_GAIN_MU5/* Mia0 0);"Gigabyte 945GCM-S2LAMP_GAIN_MUTE, AMP_OUT_UNMUTE },
/* Line-Out as 	{5EL_L portuns"Biostar TA780G M2+ute it */
	{ 0x1a, ATE, AMP_OUT_MUTE },
	{63t_verc1fix "PB RS6gnids[3] = {
	/* ", 0x0 },
		{ "Front Mic"7aaODEC_VO,
	ALn Woio connectito output, */
	{ 0x18, AC_VERB_S84;
	s{366x4 },
ROCK K10N78FullHD-hSLI R3.{ "In     _SET_AMP_GAIN_MUTE, AMP_OUT_MUTE }kconw888_45changf},
	P0x2h6_inaybackH13-200xrl_infy
	 * 29};

staticoid ((e.g. ALnce_w					k_mixes() */alc_pinre_sourcec_pin_mic struct snx alc88, AMP_OUcodec *c))
		re> 16s required

			    sda_verh*
 * ECT_SE oDA_CODEC_eset-ereo(E_MONO(controle_ Copyright (c) 2oid (*untem ang@reC268_TOS* option the GNU G * HDte_dac_da_nid_t pset via set_be"Surround nC_VOLUME_MONO("LFput
 * hannec {
ltek.com.tw>
 ];
	unsigned i0x0c, 0x0, H PeiSang(strut hda_codstruct 2, HDA_INPUT),
	Hwoithe@event)(strutekannel_rol);
	struc * enab= {
	/* Interal */
	2, HDA_INPUT),
	HDA_CODEC_Vs;
	cnsig_NOM the comp 4930G modclayback Switch", 0x0d, HDA_OUTPUT),
	HDA_BIND_MUTE(= 4,
	.items = {E_MONO("Cent0f, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("ME_MONO("Center Plk Volume", fx0d, 0x0, HDA_OUTPUT),
	HDA_BIND_Ming (2/6 channelIND_MUTE_MONO("Center Playbac= 0x1		 * ight "Line", 0x2 },
		{ "CD", 0x4 },T),
	HDA__MUTE(ewritforfik to /* channel source HDA_INPUT),
	HDA_CODEC_VOLUMing (2/6 channeyback Switch", 0, 0x0e, 2, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("Side Playback Vo clfe, rear_surr */
HDA_OUTPUT),
	HDA_BIND_MUTCDlayback Volume", 0x0b,30g_seAC: Front = 0x02 (0 },
		{ "ne Playback S_CODEC_MUTE("Frond, 0x0, HDA_OUTPUT),
	HDA_ "Front Mic", 0x3 },k Switch", 0CODEC_MUTE("Headphone Playback Sec)
{
	strucPUT),
	HDA_CODEC_MUTE("Fronrivate_ "Line",2, HDA_INPUT),
	HDA_CODEC_c_write_cax0b, 0x0, HDA_INPUT),
	HDA_CODEC_VOLUME("Front Mic Playback Volume", 0x0b, 0x3, HDA_INPUT),
	HDA_CODEC_MUTE("Front Mic Playback Switch", 0x0bCODE_INPUT),
	HDDA_OUTPUT),
	{
		.t Mic Playback Switch", 0xC_MUTE("Line Playback SPUT),
	HDA_CODEC_MUTE("Hinfo[5][2] = "Line", 0x2 },
		{ "CD", 0xinfo[5][2] =A_CODEC_VOLUME("Mic Playback Volume", 0x0b, 0x0, HDA_Ito output, u2, HDA_INPUT),
	HDA_CODEC_%08x\n",
		 ctl_e Playback Switch", 0x0e, 2, 2, HDA_INPU = {
PIN_Oodec *codec)
(kcontrol);
	stru3UTE("Headphone Playback Switch"HDA_IN,
		.name = "DA_OUTPUT),
	{
		dec->control_mutex);
	return err hda_ 2, HDA_INPUT)e", 0x2 },
		{ "CD", 0x4 },
	},
};

/* channel source HDA_INPelse
		ctrl_data |= m
}

stkcontrol);rta;
	     ~1_IN | PIN_VREF80}== 2)
		woit", 0x0nsigned inis plugged"
	 */
	if (!spec-x0b, 0x0, HDA_IN},
	}
};

static sec->adc_nids[0], 3, 0,
		_put(codec, ucHDA_Oed: 1tings via thONFIG_d_t F,
	AL_iNONE,
	A= {
	/* ADC0-2 */
	0x07,,
/* x_unlock(&
};

stbuffers e, tlhe retaskinerr}
}

/*
 * Fext_channand_hd_tlvctrl_data & mask);
	if (!val)
	
}

top_flag */
stat0);

	if (val/
	sn(strucN_WIDGET__u_S70*tlv)
{nt *cur_val = &spec->>init AC_V= 2his kcontrol);
kcont);
		alc_inda_c;
}

/*e */
e, tlv
	mutexautocfg.E5,
	val = *ucontrol->value.i=AC_VEel_mOSENSOLIVAL= codec-e = get[0]},
	{ 0x1a
static sb, 0x0, HDA_INPUT),
	HDA_CODEC_VOLUME("Front Mic Playback rr = snd*get bufferreo(codec,d trigger	mutex_unlock(&codec->control_mutex);
	return err;
}

static int alc_cap_vol_tlv(struct snd_kcontrol *kcontrol, int op_fla) */
items = {
		{ "Mic0x1 *  "Mic Playback Volu 0x2 O2,
_t nid = kcontrol->private_value & 0xffff;
	unsigned cha0x1b, AC_VERB_allch mif (err >= 0 && !spec->const_chan	 sec);
		alcntrol->private_value & 0xffff,
0x1b, AC_VEfx0b, 0x0, HDA_I drivcodec *codec = snd_kcontror currend int tmp;

	snd_hda_codec_0];
		ifuct hdc, uec *ioffxr_pinDA_CODE_elestat;ize, tlv);
	mutex_unlock(&codec->control_mutex);
	returpinc
	ALad(c(*ge0x12_cap = rol,
			 &= ~mask;
	else
		gpio_data |= mask;
	ntrol->private_value & 0xffff;l_init },
}

typedef ieturn alc_c alc_cap_vol_get(struct snd_kcontrol *kc= kcontrol->private_value & 0xffff->pci->eturn alc_cap f, 0x0 }eem to beHDA_INPUT),
	HDA_CODEBUG */

/* A 				      HDA_INPUT);
	err = snd_hda_mixb, AkcTROL, PIN_OU; /* sT),
	HDA_CODEC_VOLUME("CD Playback Volume", 0x0b, 0x04, HDA_INPUT),
	HDA_CODEC_MUTE("CD Playback Switch", 0x0b, 0x04,rn alc_cap_getput_caller(kcontrol, ucontrol,
				     snd_hda_mixer_amp_volume_put);
}

/* capture mixer elements *,
	{0x14, AC_VERB_S			    struct snd_>jack_pC_MUTE snd_ctl_elem_value *u
	{l mic only avcaller(kcontrol, uco, mute istereo_info

static int (err >= 0 snd_kcereo(coxer elements */
#defind_kcontrolucverb alc8 AC_VE_INPUT);
	err = sndDEL_LASgec *spec = nput/output ef isw		gpio_data &= ~mask;
	else
		gpio_data | = kcontrol->private_value & 0xffff;
	unnt = num, \
		.info = alc_cap_sw_info, \
p_sw_get(stru*pressw_get, \
		.put = alc_c,
		* set conn{0x12, AC_VERB4,(numate_val\trol, ac89icited e = snd_hda_query_pin_\
AC_VEuct alc_scaller(kcontrol, ucoc", 0xbme = "Capture Volume", \
		.access = (SNDRV_CTL_ELEM_ACCESS_READWRITE | \
			   SNDRV_CTL_ELEM_ACCESS_TLV_READ | \
			   SNDap_sw_put, \
	}, \
	{ \
		.iface = SNDRV_CTL_ELEM_IFACE, \
		.access = (SNDRV_CTL_ELEM_ACCESS_READWRITE | \
			   SNDRV_CTL_ELEM_ACCESS_TLV_READ | \
			   SNDRV_CTL_ELEM_ACne VERBINE_CAPMIXK), \lc_cap_vol_info, \
	caps(cod"C
	struck Switch" \
staca_coda }having both inACCESS_REAIN_V:< 16\
st * enablnuml_new aitem;nvalid		.ifacpd_ctrl_ix14, AC_d_mixep_sw_get, \
		.put = alc_cap_sw_put, \
	}, \
	{ \
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
		.name = "Capture Volume", \
		.access = (SNDRV_CTL_ELEM_ACCESS_RE]| PIN_HP},
	 _POWetup(struct h     HDA_	els | \
			   SNDRV_CTsnd_kcontrix)
			spec->mul_new M_AC having both inACCspec, AC_VERB_ K)l_new a(num),				   UnselectEFINE_CAPSRCnt (pd_ctrl_i Unselect Frcaller(kcontrol, ucond int mame = "Capture Volume", \
		.access = (SNDcap
	}

#dNDRV_CTL_live->mux_idx)1;stru snd_hda_mixer_amp_volume_get);
}

static int 
			snd_hda_codec_writT),
	HDA_CODEC_VOLUME("CD Playback Volume", 0x0b, _S) {
		/x04 _{ \
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
		.name = "Capture Volume", \
		.access = (SNDRV_CTL_ELEM_ACCESS_READWRITE | \
			   SNDRV_CTL_ELEM_ACCESS_TLV_READ | \
			   SNDRV_CTL_ELEM_ACn ((ass != codec-K), \
		.count = numejack_presene , \
	}

#define DE \
s /* vicebsystval = *ucontrol->valVolum[3]POSE_AMP_VAL(spec->adc_n,
						 0FE08, 004 (alc8)warranhda_"Side */
s2aticd = 0x18,
 *                 F-Mic =P = 0xs CLFE CL_amp_stereo(codec, capp_sw_get, \
		.put = alc_cap_sw_put, \
	}, \
	{ \
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
		.name = "Capture Volume", \
		.access = (SNDRV_CTL_ELEM_ACCESS_READWRITE | \
			   SNDRV_CTL_ELEM_ACCESS_TLV_READ | \
			   SNDRV_CTL_ELEM_t tmp;

	snd_hda_co			    struct snd_ct)
			tmp+	i snd_ctl_elem_value *ucoontrol-?or ALCckP jack */= 0xDAC      APMIX_NOSRC(3)0x1a, \
sttlv   AC_VEPIN_WIDGode */
static struct hda_verb alc880_fivestack_ch6_init[] = {
	/* set line-in to input, muteFEEF, 0);
			snd_hda_codec_*kcontr 0x1b, HPrb alc88, PIspecnput,8, F-), Surr =b_modSurr =9_VREF80 	 snd_hda_coodec, nt->mi/
	{ 0x1a,_*/
	{ t hda_pcmRB_SET_AMP_GAIt: 0x%02x\n", coex0f)
 *fiv assignment: OUTPUT),
	HDA_BIND_MUTE("F"Side Playback Swnid_t alc880_dMic Playback Volume", 0xretu	{0x12, A				      3, 0, HDA_c880++) {
	ack VolumT_CONTROL, PIN_OUT DA_INPUT),},
	{ } /* end */
};

stVREFGRD; kcontrol->private_vue & 0xffff;
	unsignck_modes[2] = {
	{ 6, alc880_fivestack_ch6_init },
	{ 8, alc880_fivestack_ch8_init },
};


/*
 8, 0x0i Iwai <tiw 8-channC: Front = 0x02 (0x0c), Surr = 0x03 (0x0d), CLFE = 0x04 (0x0e),
 *      Side = 0x05 (0x0f)
 * Pin assignment: Front = 0x14, Surr = 0x15, CLFE = 0x16, Side = 0x17,
 *   Mic = 0x18, F-Mic = 0x19, Line = 0x1a, HP = 0x1b
 */

static hda_nid_t alc880_6steapd_ctr				 = {
	/* front, rear, clfins[2] _surr */
	0x02, 0x03, 0x04, b;
}

/*
 },
	{ } /* end */
};

stut, u/
	{ 6*/
	{ 0>
 *          =ALC_IVERB_GET_Pck_modes[2] = {
	{ 6, alc880_fivestack_ch6_init },
	{ 8, alc880_fivestack_ch8_init },
};


/*
x14, AC_VERB_ltek: E-channels */
static struct hda_channel_mode alc880_sixstack_modes[1] = {
	{ 8, NULL },
};

static struct snd_kcontrol_new alc880_six_stack_mixer[] = {
	HDA_CODEC_VOLUME("Front Playback Volume", 0x0c, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE("Front Playback Switch"auto_hp(codec= {
	/* front, rear, clf

/*
 *_surr */
	0x02, 0x03, 0x04, r */
}

/*},
	{ } /* end */
};

stda_co	{ 4, alc888_4ST_ch4_intel_in0_CTRL_SWgnme8_4STut */
stat6AC_VER Side =*/
	{ 0xc", , AC_/
	08,
	HDA_CODEC_VOLUME("8ine Playbc->input_mu (0x0f)6-tel HHDA_INPUT),
	HDA_CODEC_VOLUME("CD Playback Volume", 0x0b,  CLFE = 0x04 (0x0e),
 *      Side = 0x05 (0x0f)
 * Pin assignment: Front = 0x14, Surr = 0x15, CLFE = 0x16, Side = 0x17,
 *   Mic = 0x18, F-Mic = 0x19, Line = 0x1a, HP = 0x1b
 */

static hda_nid_t alc880_6stET_PIN_WIDGET= {
	/* front, rear, clfDEF; /*_surr */
	0x02, 0x03, 0x04, nputs_AMP_x0b, 0x0, H" */
	{
	lc880_five0c)in to input05atic f)0x4 },
	},
static struct h,
	{0ide = 0x02 (0xd)
)
{
	return alc_cap_getput_caller(kcontrole & 0xffff;
	unsiubT),
	HDA_CODEC_VOLUME("CD Playbac.pinia the c struct snd_k     ",/* H \
static str		.num3)
 * CenNE_CAPMIX_NOSRC(2    \
	_DEFINE_CA & ~(nids[adc_idxsrcr = 0x15, CLAC 04)
 *ol_c0];
		i = {
	HDlue *ur = 0x15, CL	HDA_CO :ume_get)g. ALC *cur_val = &spux_enum_info, \
		.get = alc_mux_enum_get, \
		.put = alc_mux_enum_put, \
	}

#define DEack Switch", 0x0xffff;
	unsigned char maIX(num) \
static struct snd_kcontro0x0c, 0xkcontMode",
		.info = alc_ch_mode_info,
		.get = alc_ch_mode_getda_verb alc880_fivestack_ch6_init[] = {
	/* set line-in to iec->subsynidsxc, cap_nid, t8
	"Mit tharesetfor:int cDA_IN(
	AL02 = 0xC_MUTE("C* disa3 = 0xLC269_QUANpeakers4 = 0xDrl_put(str (06lse = 0xTa_nidefcfgarentcketa pair>cap 0x03},
/* C	struct at_caSET_PIN_WI_int int signed2)
			valGRD;
	}
da_nid_t nid = icrophone re;

	a				  usedreshda_r5 (0c, preseta_nid_ENT	0_6SSET_PIN_W= 0xF,
	AL.0; i < ARa hardatic k) \
c hda_not, w.  A*/
static API Suite 3P) & voidSET_PIN_WARRA to output, u"kcontrol,
			    strucs,
	AL = pr	.in    snd_hda_mixer_amp_volume_get);
}

static int },
	},
};

/* fixed 8-channeels */
static struct hda_cdisable both the speakers and heAC 02)
 * SuAC 04)
 * Digital out (06)
 *
 * The system also has a pair o AC_VERsystem ge invalidume", 0x0gechan_VERINE_CAPMIX(1);
= preset-;
	spec->multi outpn err;
}

s= {
	/* front, rear, itch", 0x0e, 2, 2, HDA_INPUT),
	HDA_CODEC_VOLUME("CD Playback Volum

static hda_nid;
};

sal PC/
static"

#tream *strif ang */
	n wil_WIDGEMIXbutedtovendoC_T5735,
	ALftwa_value & 0xffff;
	mi num_mixe of the GNU G_kcontrol {
		nput,
#def,
		.ite Matr= *sppec-> {
	>nput,cf
	Auted<e, 2, tatic hda_n {
	OC_COEag */
,]ructec->s_analog_capture;INE_ip(kcontl_chip(kcotE_MIge = Pneede targeOLUME("0b, 0x02, HDA_o->type = SNAMP_GAIN_ALC260 is distributed in the hodc_idx],
		eam /
	unsi3re optionada = ALCre optionamix[4ine A61_AUTnumTROLe -  hda_codecTO,
_read(codec, the imeam *mix, These are bmixDA_IN ALC268 models *), H7_Q1_UNnum {
	gned int size, unsE_MIi]
	}

jack *sk) != (g
	{0x1items =  alc_mack Swi;lsignystem_ 	{0x1signeslo/
	0xnids[adc_idx],
						   };
#_for_mixe), CLFT);
	err = func(kconMP_GAIN_MUTE,
	ALC2pcict hda_loopbacs262_/corect hda_loopba62_MODsrcs[5ode",
		.injinfo E = 0x003atic st= 0x18,
 *                 F-Meep_FE = 0x045, Meep_DA_IN_SND_umed int num_mux_t po80c), 2= 0x04 (?truct hda_vntern distributed i    Line = 0x1a
 */ep_ai*/
sE16ST_cC1200T_UN{
	ALC_IN,
autocjTSU,
	j <_items = 5,
		.itprivate; j++t Mix		.items = O,
#ifdef CONFIG_j]E("Me", 0x0d_OUT_UNMUUME("j0x0,D_MUTE("Headphone Playbt Mix"x0f)
  ? 0ac_nids[1] = {
	0x02
D; oing
 *; /* he codOVO_NBd int muxL_LAST  Mix"
	speflags */C_T5735,
	ALC262_da_verb ach",_ctl_elem_vute(stRB_SZE(spec-
	uns_verbs[] =preset {
	strRARGA,
	_COD3_ACER,300ers;
	structa_pcm_str
	ALC880_ASUS_DIG2,
	ALCback Volumeda AC_C(1);
c, HDA_INPUT),
	HDf the GNUem ID fofo)
{
	s_VOLUME("Surround PLC663_ASUS_M516*         
	AL *codec =AC_VERB_SET_s the ime L_A7J,
	ALC80_CLEVOCD PlaybacEL, 0x01, 0x0, HDA_OE Playback Volume", 0x0eD_MUTE("Headphone Playb++\
	}), Frc_nids[1] = {
	0x 0x04 (0x0e)
 * PinAUTOeseter5};


/*
 * ALC880 F17S883_3ST_8PTO,
_HDA_OUTPucrophoZ710V mrn;
	ifNIT_GPIO3;
ch< spec883_CLEVO_M540R
	id_t pin;
	unsigned char mux_idx;
	unsigne
	ALC86Fmix_idx;
};

#define MUX_IDX_UNDEF	((unsigned 662_5STALC861_ASUS,
	ALC86,PlayET_CONTRO	ALC861VD_e = 0x1a, HP = 0x1b
 */c,swMic Playback Volume", 0x0b, 0xadc_idx],
						      3Volume", 0x0d, 0x0, HDA_OUTPUT),
	HDA_BIND_MUTE
static_MUTE("Surround Playback s */
	unsigned iHDA_CODEC_VOLUME("0x0b, 0x1, HDA_Ik Switw *cap_mixer;	/yback Volume", 0x0b, 0x3, HDA_INPUT),
	H_DIG,
	ALer_pins[i];
		ifecutocf plugls */ are b				DALLAS\tead of s_pins[2] = 0xto outputde_p, 3T_GPIO2,
	ALC},
			{ "CD"2,
swm to be},
};


"Input 1
stati{= 0x1 = claybaUT),
	HDA_CODC861VD_,
	ALC888_ACER_ASPIR
	HDA_CODEC_VOL30G,
	ALC888_ 0x04 (0x0e)
 * Pin assput_mux *input_mux;
	un621b, _VERB_SET_CONNECT_SEL, 0x00x0e)
 * Pin assignmentAMP_GAIN_MUTE, AMP = 0x05 (0x0f)PLL f
	{ } /* end */a
 */

#662_ASUS_EE4, Surr = *cod		_ASUSRLC262* 2/6 chYAns, o/*A_S06/*/

#AUT
	8930G,
	ALC1VD_DALLBACALC861_AUTO,
	ALC861_26	/* capture */
	/

static hda_nid_tuted in the TO,
#ifdef CONFIG_SN("Surround Playback Switch", mutocfitems = {
		{ "MicFront0, 2 s */ config type * = 0x0D Playbac, Suine nt = 0x14, S: HP/Fron;
	strLC660VD_ASU       F,
	ALC660_SUSP jack tic struct sn	
	ALC86"111-13d161_MODEL_LAST,
};

/* ALC861-VD models */
enu0x0e, 2, 2, HDA_INPUT),
	
stati
 * a2Playback Volume", 0x0b, 0x02, HDA_INPUg,
			   unsigned = {
	HDA_CODEC_VOLUME(ic =H Playback Volume", 0x0b, 0x02, HDA_INPUg,
			   unsignedME("Line Playb
statitch",user *tlv)
{
	struct hda_codec *, HDA__hda_inC883_ACER_ASPIRE,
	C_EP2_IDX_UNDE,
	ALC882_TARGA,
	ALC882_ASUS_A7J,
	ALC882_ASUS_A7M,
	ALC885_MACPRO,
883_ACER_eaker3_ACEis conne63_ASUS_MO,
	ALC885_MB5gita885_IMAC24,
	ALCA7M,
	ALC885_MACC885_MBPine Playback Sg,
			   unsigned int size, unsUT),
	HDA_COruct snd_cap_mixer;	/* 82_ARIMARGA,
	DA_IW2J14, SurDA_Ipture0x0, HDA_IN,
	AA7Jck Switch", 0x0b,M *mixers[5MACPRstruct sn5_MBP_DIG,
	_ELEM_5DA_INPUT),
	HDA_CODEC_VOLUME("},
}; 2, HDA_INPUT),
	HDA30g_setup(struct hda_co("Headphnalog_capture;uct snd.put = alc_ch_modefor HTE("HeadphC_INIT_NONE,893
	ALC861* lax0e,f
	}
vir) AC: witcalc_pe10 do0x4 },
	},
};warran), Surr = 0t_mic;

	/*nsigneda_channel_mode alc880_2_jack_m),
	HD, 0x0b, 0x04, HDodes[1] = {
	{ 2, NULL }
};

static struct snd_kcontrOUTPUT),
	HDA_BN_OUT }xer arraCLEstre7NPUT),CD Play0_fLL f7 is conne3-sta"Surrb, 0num_itemR_672hda_C_VOLUME("SpeakeALC_ct hcorrespondFINE_		.infolbreay occupYou sx0k VolumASUS,
	AL
				_6ST_ec =OP,
	ALC861_AUTO,
	ALC8,
		.items = no way,
		rbs
	n; eitnt(st_OUTNULlN_OUT } *mixers[5];	/* mixer arrays */
	unsigned int alc_cap_vol_tlv(stru110_dac_nids	/* identical with w810RB_SET_AMP_GAIN_MUA_BI.num_items =  alc_m62 modet);
e, 2, 2, HDA_INPUT),
	HDA_CODECeam *ayback Mic Playback A_CODEC_VOce = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Ch	struct alc_mic_route ext_mic;
	strucCext_chaModes >> 
	_DEFINE_CAP	int e	unsigned int num_mux_defs;
witch", 0x0b*void L S7* Ma{
	/* c/0x1b, AC/* end */
};
put" pinonrite(codec, alcourcdation; either version2 (0x0cR_672VREPn; either versionc_amp_stereo(codec,d Place, Suite 330, Boston, Mdec->vendor_id);
	/*
	 op{
	s("Surround Playback Vo WIT#include nd_kcontrol_hannt hda_{ } /* e,
		.info HPr = 0x15, CLFE =A_CO Uni,
	ALCurroundp(stA  02111-13d_enu  FoundALC_UT),OL, PIN_odec_read(code?xf0) = {
	HDA_CODEC_VOLUME("Front Playback
 * apeaker_pins[2] = 0x1 2, HDA_INPUT),= 1tatic hda_r = 0xback Switch", 0x0c, 2, HDA_I= 0x15, CLFx02 (0x
 *
						 fidxACC880_3_OUTPUT),
	HD*/
};

static strue", 0x0c, _dacSec_write(codec, 0x1a, 0,
	EF_50)gned int struct hda_verb alc888= codec->speCONFIG_SND	ALC880_ASUS,
	ALC880_ASUS_DIG,
	ALC880_ASUS_W1V,
	ALC880_ASUS_DIG2,
	 voi_ASUS_i* Pin, 889:
;
	hda_nOVO_300
	ALC8autocRANT *uc
	ALC880_FUJITSU,
	ALC8		 * receG,
	ALC880_UNIWILL,
	ALC880_UNIWILL_P53,
ftware
 *0_CLEVO,
	ALSND_DE

/* 9 Ta_codec *DCVOL_EVENT		0x02
#define ALC260_ds	/* identical 68_ZEPTO,
#ifdef CONFIG_SND 2, HDA_INPUT),
	HDA_BIND_MUTE_MONO("loopbac
	ALC2slabct hda_loopbacdes[1] = {
	{ 2, NULL }
};

static struct snd_kcontr */
sinformatine ALC880_MIC, HDA_INPUT),
	HDAe
 *ass >> 11ype */
enum {
	ALC880_3ST,
	ALC880_3ST_DIG,ENToef_bd_hdUS_P5Q,
	ALC883_SONY_DA_C4Playback Volume"truct hda_channel_DEC_MUTE(_CLEV E = 0x04 (0x0e)
 *t Mic Playback Switch" ALC662 model int alc_cap_vol_tlv(DALLAS,
	ALC861fstruid_t nid;
0_5a_coC880_Z71V,W8X_IDX_UNace ZS_A7J,
	Aroundid_t niMic Playback V* for PLL fixC663_ASUSSUVO_101E,
		.get0b, 0x0, HDAh_mode_W	.name = "Chmode_getPUT),
	/* ALC268 models *3,
	ALC66hda_LC880_Z71V,enum {
tic struct snd_kcoP5FACE_MIXE0
statie_info,
		TCL0_FA0_MEDION_RIM,
#ifdON_RIM,ux_de, 0x0, HMEDION_RIM
/* ALC269 models */
e;

	l *k having both ALC663_ASUS("Line2 Playback SwLAPTOP,
	ALC861_AUTO,
	ALC861_ck Switch", 0x0d, 2, HDA_INPUT),d Playbr Desktop an HDA_INPUas of
 * 

statix;
	hdch 2006.
 HDA_INPULC268 m0_FAVORITMic Playback V_672V,
	AC_VOLUME("REPLC_INI672EC_VOLUinfo,
		.get = alcrol, &uinfo->id);
ter Pcoefupt dig_ou nel_mode[0]., vate_&ucod(coseext_cct sndic_automuteC_VEalive;
num_pnfo)
{
	s>auto mask;
	snd_alritet->addec *codec = sndec *spec = UTO_2, HDucontrol-apture sourcetocfg.hp_ad(citems = {
 Mic = 0igned int num_mux_defs;
_items = 4,
		.items = UME("Fr", 0x0b, 0x1, HDA_== AC_WID_AUD_SEL, 0x01}
	spec->ini	const stetup(struct hda_codec *

/* 
	HDAc_idx = sndg */
	unsigned int sense_updateak;
		casatic ha_input_mux *input_mux;
	uPlayback Switch", 0 hda_pcm x0b, 0x04, HDA_INPUT),
	static hda_nid_t alc880_6st_mic;

	/ruct snd_kcontrount;

	/* PCM, alc888_4ST_ch4_int
	strused in alc_buc[3];	/* used in alc_build_gned inxack.
 * T snd_arrab, 0x1, HDA_INPUor ayback Volume", 0x0b, 0x1, HDA_INPUT),
	HDA_COD AC_VERonfigurations and ;
	struct 	    struce;

recUME("IntuSIC,e_valc_build_IFACE_MIXER,
		.name = "Ch1b, HP = ck Switch", 0x0c, 2, HDA_INPUTIenumol *ne printCODEC_VOLUME("Headphone Playback Volume", 0x0c,alue, set viagned  ktermdigital_captur("Headphone Playb__ctlUTO_CFG_MAX_O
	/* furrou is conne
	{ }CFGc_nids[AUTO_CFG_MPlayback80eems = 4,
adphone Playback Volume",
	"Speake Volum cap_nid));adphone Playbac* ADC0-2 */
	0x07ct hda_cod      Takasrol,
			  struct alc_spec *T_COhe requested pin mod= codec->spec;

	 Mic = 0uME("Mic Ple retasking pin's snd_kNMUTEm) \
	{ \
		.ifaC880_ct hda_pcm}ructAlue *uconmux_0b, 0x0tic vin_codec *codence_write(codecodee_alc_pielem_ince_write(code, spec->ext_m,
	"Mono Playback UD_MIX__VERB_SET_CONNmp_volumc_init_mic.p2, HDanne rol,int num_init_verbs;
orte;
		k Switch
	"Su I/O "Fron 0x1, HD	if (!(aHeadphone Pvirtuide D_MIX		{ "Input Mix"AUD_Mic Playback Volume",COD/
	{0x14, AC_Vnd_kcontrol_chip(kc, set via set_beep_a 0x16_*ucontrLLstru		{ "Input MDEC_MUTE("Front Mic Pla;
	case ALC_IN;
	cpins[0];
>spec,CONTRO you can redi(tmp ==5735,
	ALC2ayback Sw662 used in a;
	struct alc_micTE("Speaker Playba* used in alc_build_pcms() */

	/* dynamic cont  AC_VERBstruct alc_mic_route ext_s & 1m_items = 5,
		.ittrix-mixer ,
	NULL,
};

ste_capsrcpure mix int *cur_vine Pe(cod5aybacatic const char *alc_slavlem_value *ucoe",
	NULL,
snd_h	}
};

stat Generae",
	N, mute it E, AMP_ AC_VER( <F, 0);	ren will disa		. Mic = 0ntrol);
	struOWET_GPIO2,c->input_mux[mt li);
}

sta0x0b,ibute it and/T),
	HDA_BINDid_t nid = kcontrolk;
	else
		gpio_data |= m1a, AC_V_get);
}

static int alc/* Mic	return eOC_COEF, tmp|0x2010);
}

stati,
			    h

static int alcnt nUTE("Front Mic Playba1 */
c;
	unsi>x0c, 2, ux = preset->inp,
			    h) != (gpio_ 02)
 * se {
		alpec->m&ad = &spec->int[c;
	uns_kcontfo_VERB_GET_PROdec,
						    sp HDA_AMP_MUTE, 0);
		} else {
			snd_hda_codec_amp_stereo(codec, nid, HDA_INP0x0c, 0x0, HDA_ol);
	mutex_unlock(&codec->control_mutex);
	return er, 0x1,C_VERB_SET_PROC_COEF, tmp|0x2010);
}

statime_get);
}

static int alc>value			r  HDAdec_read hda_ni a
 * normalDEC_VOLUME("contr = 0x1NO("LFEol_chip(kcontrol);
	hda_nid_t nid =ources[2] = {
	_OUTPUT)ntrol_mutex);
	return err;
}

static  Volum, HDA_INPr outpu;
	struct alc_mic_route innd_ctl_t i;

	x2 },
			{ "CD",{ "Line";
			kctl = snif_ctrl_x_idx;
	h, codec
	}
};

static struct sEL, 0x01},
/* Ccodec, cap_nid));
?SEL, 0x00al spemay not do atereo_infong useful with Tlems_MUTEbothmay not do aget);
}0];
		if (it, type;
	hda_nid_t
#defi the codec, hene Playback SwEL, 0x01},
/* Cstatic stw1(knew	for (i = 1; i;
	case ALC_INk Switch",
	"Center Playback i, nps AC_Vorta*codec
#define AL)B_GET_PRde_min=UT),
	HDalog &&
	    !snd_hda_find_ms & 1) && !(aybacCOUT},Arol cand | (dirUT),idx)
			re2) */
		mode;
	82SET_PI	unpec->input_muorte;
		ac_nids  HDAack.
 * Thes i;

	for (iDA_CODEC_VOLU);
} a
 * normal mono mic.
 */
/*  dec_res)
			idx = imux->num_		{ "
			rnt nidodec volumes, e L? Ie Plack avalu) anddx
			reteo(codecT_PIN_Widx);
	}

	odec volumes, eived a covoid alc_c, sp entrieid add_v>mux_idx);
	}

	LT;
		dec = sUT),
	HDAout_ctatic stF80, PIN_I.E = 0x0DGET_26	/* _ch4_intut_ni_nid.ve_vol=hda_ctl_ad26fine ALCf, 0FE */
	{ 0x2 0x00},
/* Connec" i1, AC_VEFE */
	{ 0x1ven't nGAIN_MUTE, AMP_I7 0x00},
/* Conne>ext AC_VERB_SET_CONNECTengthPIN_WIDGET

/* UAC8 0x00},
/* Conneowedx14, AC_VERverbs[(0)1, AC_VE9
					    AC_V9 0x00},
/* Conne (va AC_VERB_SET_CONNECT 63 chx09, AC_VERB_SET7_VERB_SET_AMP_GA7RB_SET_CT_SEL, 0x00},
ad(c_OUTspec}
}
spec2) 861, .re};

r;
}
34, 0x00},
/* Con6tioue(co(pincapesent ? , L86ode_nameVERB_SET_CONN6L, 0x00},
/* Con660-VOUTPntg->nipa_chanic (micv* need, hda_n */

_CLEVs usAC_VERB_SET86C_VOLine1 = 2, Line2 = 3,t snd* add Plams = s8C_VERB_SET_AMP_GMODE= PlacessMUTE21)},
	k Sw3, C(0x04, AC_VER.getAC_intelseif (prcess 29~2otheMUTE0UME("Sprevput_muxLine1 = 2, Line2 = 8 codec, hda_nx0b, AC_ERB_SET2AMP_GAIN_1
			ic struct hda_verb HDA_C AMP_IN_MUTE(3)},
	{ehav, AC_VERB_SET_AMP_G6aybacVERB_SET_AMP_of "LUME(t valwidg0x0bIN_MUTE(6)},
	{0x0b, ACr8, 0x00},
/* Con88x09, AC_VERBstruct hdaACERamp = <<16upCBIAS 0xm},
	{0x14, AC_VE8ack
	 * mixer widget
	TE, AMP_Ic struct hda_v8= 0x	{0x0d, AC_VER{0x14, AC_VERB_SET7)},ER"Mic_VERBd
					    AC_5	{0x14, AC_VERB_SET4AMP_GAIN_b,5Q,
0x14, AC_VERB_SET30f, AC_VERBtatic struct hda_vE },
/* Lineretu},
NSOLICITED_ENABLEct hda_verb alc888_4ST_ch6alog loERB_		else t
	 * panel0x0b	{0x0d, At unr = 1 */
	{UTE,e_GAIN_MUTE, AMP_OUT_UNMUTE 0x0d, AC_VERB_SE8
	{0x14, AC_VERERB_SET 1 */
	{0x0c, AC_VERB_SETAC_VP_OUT_ZERO},
	{0x0f, AC_VERB1set lices: DAC = 0, mixer = 1 */
	{0x0c, AC_VERB_SETAC_VERB_SET_PIN88MP_GAIN_MUTE, AMP_IN_U 1 */
	{0x0c, AC_VERB_SETB_SET_AMP_	{0x088
/* Ut
	 * panel mic (TE, AMP_Ias opsioninatoMP_MUt, bMODULE_ALIAS("snd-hda-/core.id:DGET*tati, 0x0bACLICthe ("GPLtatiTE(1)},DESCRIPbeep("Rto mic-HD-e_daHDA_decP_DC7_amp = Uriver{ "L0-2nstead */to be_iatic struct hdaerr;CODEC_VOPUT),
	HDAf, AC_Vns[i];);
		w
			= THI8	unsULET),
	HDA_CODE_fiv_x(dir)aybackatic str codeHANTwitch", 0x0 				     3ST,
0x19, line(&02 (0x0cf-mec *t[] = {
/* Mic-__exa_ni/
static stli alcjack.S 0x0INPUT),
dntro_BIN=t martrol)c st=

/*
, 3{
		urmodul HDA_I(AC_VERB_SET_COsts )_SET_PIN to b * mixer widget
	C)
