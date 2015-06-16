/*
 * Universal Interface for Intel High Definition Audio Codec
 *
 * HD audio interface patch for SigmaTel STAC92xx
 *
 * Copyright (c) 2005 Embedded Alley Solutions, Inc.
 * Matt Porter <mporter@embeddedalley.com>
 *
 * Based on patch_cmedia.c and patch_realtek.c
 * Copyright (c) 2004 Takashi Iwai <tiwai@suse.de>
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
#include <linux/dmi.h>
#include <sound/core.h>
#include <sound/asoundef.h>
#include <sound/jack.h>
#include "hda_codec.h"
#include "hda_local.h"
#include "hda_beep.h"

enum {
	STAC_VREF_EVENT	= 1,
	STAC_INSERT_EVENT,
	STAC_PWR_EVENT,
	STAC_HP_EVENT,
	STAC_LO_EVENT,
	STAC_MIC_EVENT,
};

enum {
	STAC_AUTO,
	STAC_REF,
	STAC_9200_OQO,
	STAC_9200_DELL_D21,
	STAC_9200_DELL_D22,
	STAC_9200_DELL_D23,
	STAC_9200_DELL_M21,
	STAC_9200_DELL_M22,
	STAC_9200_DELL_M23,
	STAC_9200_DELL_M24,
	STAC_9200_DELL_M25,
	STAC_9200_DELL_M26,
	STAC_9200_DELL_M27,
	STAC_9200_M4,
	STAC_9200_M4_2,
	STAC_9200_PANASONIC,
	STAC_9200_MODELS
};

enum {
	STAC_9205_AUTO,
	STAC_9205_REF,
	STAC_9205_DELL_M42,
	STAC_9205_DELL_M43,
	STAC_9205_DELL_M44,
	STAC_9205_EAPD,
	STAC_9205_MODELS
};

enum {
	STAC_92HD73XX_AUTO,
	STAC_92HD73XX_NO_JD, /* no jack-detection */
	STAC_92HD73XX_REF,
	STAC_92HD73XX_INTEL,
	STAC_DELL_M6_AMIC,
	STAC_DELL_M6_DMIC,
	STAC_DELL_M6_BOTH,
	STAC_DELL_EQ,
	STAC_ALIENWARE_M17X,
	STAC_92HD73XX_MODELS
};

enum {
	STAC_92HD83XXX_AUTO,
	STAC_92HD83XXX_REF,
	STAC_92HD83XXX_PWR_REF,
	STAC_DELL_S14,
	STAC_92HD83XXX_MODELS
};

enum {
	STAC_92HD71BXX_AUTO,
	STAC_92HD71BXX_REF,
	STAC_DELL_M4_1,
	STAC_DELL_M4_2,
	STAC_DELL_M4_3,
	STAC_HP_M4,
	STAC_HP_DV5,
	STAC_HP_HDX,
	STAC_HP_DV4_1222NR,
	STAC_92HD71BXX_MODELS
};

enum {
	STAC_925x_AUTO,
	STAC_925x_REF,
	STAC_M1,
	STAC_M1_2,
	STAC_M2,
	STAC_M2_2,
	STAC_M3,
	STAC_M5,
	STAC_M6,
	STAC_925x_MODELS
};

enum {
	STAC_922X_AUTO,
	STAC_D945_REF,
	STAC_D945GTP3,
	STAC_D945GTP5,
	STAC_INTEL_MAC_V1,
	STAC_INTEL_MAC_V2,
	STAC_INTEL_MAC_V3,
	STAC_INTEL_MAC_V4,
	STAC_INTEL_MAC_V5,
	STAC_INTEL_MAC_AUTO, /* This model is selected if no module parameter
			      * is given, one of the above models will be
			      * chosen according to the subsystem id. */
	/* for backward compatibility */
	STAC_MACMINI,
	STAC_MACBOOK,
	STAC_MACBOOK_PRO_V1,
	STAC_MACBOOK_PRO_V2,
	STAC_IMAC_INTEL,
	STAC_IMAC_INTEL_20,
	STAC_ECS_202,
	STAC_922X_DELL_D81,
	STAC_922X_DELL_D82,
	STAC_922X_DELL_M81,
	STAC_922X_DELL_M82,
	STAC_922X_MODELS
};

enum {
	STAC_927X_AUTO,
	STAC_D965_REF_NO_JD, /* no jack-detection */
	STAC_D965_REF,
	STAC_D965_3ST,
	STAC_D965_5ST,
	STAC_D965_5ST_NO_FP,
	STAC_DELL_3ST,
	STAC_DELL_BIOS,
	STAC_927X_VOLKNOB,
	STAC_927X_MODELS
};

enum {
	STAC_9872_AUTO,
	STAC_9872_VAIO,
	STAC_9872_MODELS
};

struct sigmatel_event {
	hda_nid_t nid;
	unsigned char type;
	unsigned char tag;
	int data;
};

struct sigmatel_jack {
	hda_nid_t nid;
	int type;
	struct snd_jack *jack;
};

struct sigmatel_mic_route {
	hda_nid_t pin;
	signed char mux_idx;
	signed char dmux_idx;
};

struct sigmatel_spec {
	struct snd_kcontrol_new *mixers[4];
	unsigned int num_mixers;

	int board_config;
	unsigned int eapd_switch: 1;
	unsigned int surr_switch: 1;
	unsigned int alt_switch: 1;
	unsigned int hp_detect: 1;
	unsigned int spdif_mute: 1;
	unsigned int check_volume_offset:1;
	unsigned int auto_mic:1;

	/* gpio lines */
	unsigned int eapd_mask;
	unsigned int gpio_mask;
	unsigned int gpio_dir;
	unsigned int gpio_data;
	unsigned int gpio_mute;
	unsigned int gpio_led;

	/* stream */
	unsigned int stream_delay;

	/* analog loopback */
	struct snd_kcontrol_new *aloopback_ctl;
	unsigned char aloopback_mask;
	unsigned char aloopback_shift;

	/* power management */
	unsigned int num_pwrs;
	unsigned int *pwr_mapping;
	hda_nid_t *pwr_nids;
	hda_nid_t *dac_list;

	/* jack detection */
	struct snd_array jacks;

	/* events */
	struct snd_array events;

	/* playback */
	struct hda_input_mux *mono_mux;
	unsigned int cur_mmux;
	struct hda_multi_out multiout;
	hda_nid_t dac_nids[5];
	hda_nid_t hp_dacs[5];
	hda_nid_t speaker_dacs[5];

	int volume_offset;

	/* capture */
	hda_nid_t *adc_nids;
	unsigned int num_adcs;
	hda_nid_t *mux_nids;
	unsigned int num_muxes;
	hda_nid_t *dmic_nids;
	unsigned int num_dmics;
	hda_nid_t *dmux_nids;
	unsigned int num_dmuxes;
	hda_nid_t *smux_nids;
	unsigned int num_smuxes;
	unsigned int num_analog_muxes;

	unsigned long *capvols; /* amp-volume attr: HDA_COMPOSE_AMP_VAL() */
	unsigned long *capsws; /* amp-mute attr: HDA_COMPOSE_AMP_VAL() */
	unsigned int num_caps; /* number of capture volume/switch elements */

	struct sigmatel_mic_route ext_mic;
	struct sigmatel_mic_route int_mic;

	const char **spdif_labels;

	hda_nid_t dig_in_nid;
	hda_nid_t mono_nid;
	hda_nid_t anabeep_nid;
	hda_nid_t digbeep_nid;

	/* pin widgets */
	hda_nid_t *pin_nids;
	unsigned int num_pins;

	/* codec specific stuff */
	struct hda_verb *init;
	struct snd_kcontrol_new *mixer;

	/* capture source */
	struct hda_input_mux *dinput_mux;
	unsigned int cur_dmux[2];
	struct hda_input_mux *input_mux;
	unsigned int cur_mux[3];
	struct hda_input_mux *sinput_mux;
	unsigned int cur_smux[2];
	unsigned int cur_amux;
	hda_nid_t *amp_nids;
	unsigned int powerdown_adcs;

	/* i/o switches */
	unsigned int io_switch[2];
	unsigned int clfe_swap;
	hda_nid_t line_switch;	/* shared line-in for input and output */
	hda_nid_t mic_switch;	/* shared mic-in for input and output */
	hda_nid_t hp_switch; /* NID of HP as line-out */
	unsigned int aloopback;

	struct hda_pcm pcm_rec[2];	/* PCM information */

	/* dynamic controls and input_mux */
	struct auto_pin_cfg autocfg;
	struct snd_array kctls;
	struct hda_input_mux private_dimux;
	struct hda_input_mux private_imux;
	struct hda_input_mux private_smux;
	struct hda_input_mux private_mono_mux;
};

static hda_nid_t stac9200_adc_nids[1] = {
        0x03,
};

static hda_nid_t stac9200_mux_nids[1] = {
        0x0c,
};

static hda_nid_t stac9200_dac_nids[1] = {
        0x02,
};

static hda_nid_t stac92hd73xx_pwr_nids[8] = {
	0x0a, 0x0b, 0x0c, 0xd, 0x0e,
	0x0f, 0x10, 0x11
};

static hda_nid_t stac92hd73xx_slave_dig_outs[2] = {
	0x26, 0,
};

static hda_nid_t stac92hd73xx_adc_nids[2] = {
	0x1a, 0x1b
};

#define STAC92HD73XX_NUM_DMICS	2
static hda_nid_t stac92hd73xx_dmic_nids[STAC92HD73XX_NUM_DMICS + 1] = {
	0x13, 0x14, 0
};

#define STAC92HD73_DAC_COUNT 5

static hda_nid_t stac92hd73xx_mux_nids[2] = {
	0x20, 0x21,
};

static hda_nid_t stac92hd73xx_dmux_nids[2] = {
	0x20, 0x21,
};

static hda_nid_t stac92hd73xx_smux_nids[2] = {
	0x22, 0x23,
};

#define STAC92HD73XX_NUM_CAPS	2
static unsigned long stac92hd73xx_capvols[] = {
	HDA_COMPOSE_AMP_VAL(0x20, 3, 0, HDA_OUTPUT),
	HDA_COMPOSE_AMP_VAL(0x21, 3, 0, HDA_OUTPUT),
};
#define stac92hd73xx_capsws	stac92hd73xx_capvols

#define STAC92HD83_DAC_COUNT 3

static hda_nid_t stac92hd83xxx_mux_nids[2] = {
	0x17, 0x18,
};

static hda_nid_t stac92hd83xxx_adc_nids[2] = {
	0x15, 0x16,
};

static hda_nid_t stac92hd83xxx_pwr_nids[4] = {
	0xa, 0xb, 0xd, 0xe,
};

static hda_nid_t stac92hd83xxx_slave_dig_outs[2] = {
	0x1e, 0,
};

static unsigned int stac92hd83xxx_pwr_mapping[4] = {
	0x03, 0x0c, 0x20, 0x40,
};

#define STAC92HD83XXX_NUM_CAPS	2
static unsigned long stac92hd83xxx_capvols[] = {
	HDA_COMPOSE_AMP_VAL(0x17, 3, 0, HDA_OUTPUT),
	HDA_COMPOSE_AMP_VAL(0x18, 3, 0, HDA_OUTPUT),
};
#define stac92hd83xxx_capsws	stac92hd83xxx_capvols

static hda_nid_t stac92hd71bxx_pwr_nids[3] = {
	0x0a, 0x0d, 0x0f
};

static hda_nid_t stac92hd71bxx_adc_nids[2] = {
	0x12, 0x13,
};

static hda_nid_t stac92hd71bxx_mux_nids[2] = {
	0x1a, 0x1b
};

static hda_nid_t stac92hd71bxx_dmux_nids[2] = {
	0x1c, 0x1d,
};

static hda_nid_t stac92hd71bxx_smux_nids[2] = {
	0x24, 0x25,
};

#define STAC92HD71BXX_NUM_DMICS	2
static hda_nid_t stac92hd71bxx_dmic_nids[STAC92HD71BXX_NUM_DMICS + 1] = {
	0x18, 0x19, 0
};

static hda_nid_t stac92hd71bxx_slave_dig_outs[2] = {
	0x22, 0
};

#define STAC92HD71BXX_NUM_CAPS		2
static unsigned long stac92hd71bxx_capvols[] = {
	HDA_COMPOSE_AMP_VAL(0x1c, 3, 0, HDA_OUTPUT),
	HDA_COMPOSE_AMP_VAL(0x1d, 3, 0, HDA_OUTPUT),
};
#define stac92hd71bxx_capsws	stac92hd71bxx_capvols

static hda_nid_t stac925x_adc_nids[1] = {
        0x03,
};

static hda_nid_t stac925x_mux_nids[1] = {
        0x0f,
};

static hda_nid_t stac925x_dac_nids[1] = {
        0x02,
};

#define STAC925X_NUM_DMICS	1
static hda_nid_t stac925x_dmic_nids[STAC925X_NUM_DMICS + 1] = {
	0x15, 0
};

static hda_nid_t stac925x_dmux_nids[1] = {
	0x14,
};

static unsigned long stac925x_capvols[] = {
	HDA_COMPOSE_AMP_VAL(0x09, 3, 0, HDA_OUTPUT),
};
static unsigned long stac925x_capsws[] = {
	HDA_COMPOSE_AMP_VAL(0x14, 3, 0, HDA_OUTPUT),
};

static hda_nid_t stac922x_adc_nids[2] = {
        0x06, 0x07,
};

static hda_nid_t stac922x_mux_nids[2] = {
        0x12, 0x13,
};

#define STAC922X_NUM_CAPS	2
static unsigned long stac922x_capvols[] = {
	HDA_COMPOSE_AMP_VAL(0x17, 3, 0, HDA_INPUT),
	HDA_COMPOSE_AMP_VAL(0x18, 3, 0, HDA_INPUT),
};
#define stac922x_capsws		stac922x_capvols

static hda_nid_t stac927x_slave_dig_outs[2] = {
	0x1f, 0,
};

static hda_nid_t stac927x_adc_nids[3] = {
        0x07, 0x08, 0x09
};

static hda_nid_t stac927x_mux_nids[3] = {
        0x15, 0x16, 0x17
};

static hda_nid_t stac927x_smux_nids[1] = {
	0x21,
};

static hda_nid_t stac927x_dac_nids[6] = {
	0x02, 0x03, 0x04, 0x05, 0x06, 0
};

static hda_nid_t stac927x_dmux_nids[1] = {
	0x1b,
};

#define STAC927X_NUM_DMICS 2
static hda_nid_t stac927x_dmic_nids[STAC927X_NUM_DMICS + 1] = {
	0x13, 0x14, 0
};

#define STAC927X_NUM_CAPS	3
static unsigned long stac927x_capvols[] = {
	HDA_COMPOSE_AMP_VAL(0x18, 3, 0, HDA_INPUT),
	HDA_COMPOSE_AMP_VAL(0x19, 3, 0, HDA_INPUT),
	HDA_COMPOSE_AMP_VAL(0x1a, 3, 0, HDA_INPUT),
};
static unsigned long stac927x_capsws[] = {
	HDA_COMPOSE_AMP_VAL(0x1b, 3, 0, HDA_OUTPUT),
	HDA_COMPOSE_AMP_VAL(0x1c, 3, 0, HDA_OUTPUT),
	HDA_COMPOSE_AMP_VAL(0x1d, 3, 0, HDA_OUTPUT),
};

static const char *stac927x_spdif_labels[5] = {
	"Digital Playback", "ADAT", "Analog Mux 1",
	"Analog Mux 2", "Analog Mux 3"
};

static hda_nid_t stac9205_adc_nids[2] = {
        0x12, 0x13
};

static hda_nid_t stac9205_mux_nids[2] = {
        0x19, 0x1a
};

static hda_nid_t stac9205_dmux_nids[1] = {
	0x1d,
};

static hda_nid_t stac9205_smux_nids[1] = {
	0x21,
};

#define STAC9205_NUM_DMICS	2
static hda_nid_t stac9205_dmic_nids[STAC9205_NUM_DMICS + 1] = {
        0x17, 0x18, 0
};

#define STAC9205_NUM_CAPS	2
static unsigned long stac9205_capvols[] = {
	HDA_COMPOSE_AMP_VAL(0x1b, 3, 0, HDA_INPUT),
	HDA_COMPOSE_AMP_VAL(0x1c, 3, 0, HDA_INPUT),
};
static unsigned long stac9205_capsws[] = {
	HDA_COMPOSE_AMP_VAL(0x1d, 3, 0, HDA_OUTPUT),
	HDA_COMPOSE_AMP_VAL(0x1e, 3, 0, HDA_OUTPUT),
};

static hda_nid_t stac9200_pin_nids[8] = {
	0x08, 0x09, 0x0d, 0x0e, 
	0x0f, 0x10, 0x11, 0x12,
};

static hda_nid_t stac925x_pin_nids[8] = {
	0x07, 0x08, 0x0a, 0x0b, 
	0x0c, 0x0d, 0x10, 0x11,
};

static hda_nid_t stac922x_pin_nids[10] = {
	0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
	0x0f, 0x10, 0x11, 0x15, 0x1b,
};

static hda_nid_t stac92hd73xx_pin_nids[13] = {
	0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
	0x0f, 0x10, 0x11, 0x12, 0x13,
	0x14, 0x22, 0x23
};

static hda_nid_t stac92hd83xxx_pin_nids[10] = {
	0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
	0x0f, 0x10, 0x11, 0x1f, 0x20,
};

#define STAC92HD71BXX_NUM_PINS 13
static hda_nid_t stac92hd71bxx_pin_nids_4port[STAC92HD71BXX_NUM_PINS] = {
	0x0a, 0x0b, 0x0c, 0x0d, 0x00,
	0x00, 0x14, 0x18, 0x19, 0x1e,
	0x1f, 0x20, 0x27
};
static hda_nid_t stac92hd71bxx_pin_nids_6port[STAC92HD71BXX_NUM_PINS] = {
	0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
	0x0f, 0x14, 0x18, 0x19, 0x1e,
	0x1f, 0x20, 0x27
};

static hda_nid_t stac927x_pin_nids[14] = {
	0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
	0x0f, 0x10, 0x11, 0x12, 0x13,
	0x14, 0x21, 0x22, 0x23,
};

static hda_nid_t stac9205_pin_nids[12] = {
	0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
	0x0f, 0x14, 0x16, 0x17, 0x18,
	0x21, 0x22,
};

static int stac92xx_dmux_enum_info(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_info *uinfo)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct sigmatel_spec *spec = codec->spec;
	return snd_hda_input_mux_info(spec->dinput_mux, uinfo);
}

static int stac92xx_dmux_enum_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct sigmatel_spec *spec = codec->spec;
	unsigned int dmux_idx = snd_ctl_get_ioffidx(kcontrol, &ucontrol->id);

	ucontrol->value.enumerated.item[0] = spec->cur_dmux[dmux_idx];
	return 0;
}

static int stac92xx_dmux_enum_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct sigmatel_spec *spec = codec->spec;
	unsigned int dmux_idx = snd_ctl_get_ioffidx(kcontrol, &ucontrol->id);

	return snd_hda_input_mux_put(codec, spec->dinput_mux, ucontrol,
			spec->dmux_nids[dmux_idx], &spec->cur_dmux[dmux_idx]);
}

static int stac92xx_smux_enum_info(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_info *uinfo)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct sigmatel_spec *spec = codec->spec;
	return snd_hda_input_mux_info(spec->sinput_mux, uinfo);
}

static int stac92xx_smux_enum_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct sigmatel_spec *spec = codec->spec;
	unsigned int smux_idx = snd_ctl_get_ioffidx(kcontrol, &ucontrol->id);

	ucontrol->value.enumerated.item[0] = spec->cur_smux[smux_idx];
	return 0;
}

static int stac92xx_smux_enum_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct sigmatel_spec *spec = codec->spec;
	struct hda_input_mux *smux = &spec->private_smux;
	unsigned int smux_idx = snd_ctl_get_ioffidx(kcontrol, &ucontrol->id);
	int err, val;
	hda_nid_t nid;

	err = snd_hda_input_mux_put(codec, spec->sinput_mux, ucontrol,
			spec->smux_nids[smux_idx], &spec->cur_smux[smux_idx]);
	if (err < 0)
		return err;

	if (spec->spdif_mute) {
		if (smux_idx == 0)
			nid = spec->multiout.dig_out_nid;
		else
			nid = codec->slave_dig_outs[smux_idx - 1];
		if (spec->cur_smux[smux_idx] == smux->num_items - 1)
			val = HDA_AMP_MUTE;
		else
			val = 0;
		/* un/mute SPDIF out */
		snd_hda_codec_amp_stereo(codec, nid, HDA_OUTPUT, 0,
					 HDA_AMP_MUTE, val);
	}
	return 0;
}

static unsigned int stac92xx_vref_set(struct hda_codec *codec,
					hda_nid_t nid, unsigned int new_vref)
{
	int error;
	unsigned int pincfg;
	pincfg = snd_hda_codec_read(codec, nid, 0,
				AC_VERB_GET_PIN_WIDGET_CONTROL, 0);

	pincfg &= 0xff;
	pincfg &= ~(AC_PINCTL_VREFEN | AC_PINCTL_IN_EN | AC_PINCTL_OUT_EN);
	pincfg |= new_vref;

	if (new_vref == AC_PINCTL_VREF_HIZ)
		pincfg |= AC_PINCTL_OUT_EN;
	else
		pincfg |= AC_PINCTL_IN_EN;

	error = snd_hda_codec_write_cache(codec, nid, 0,
					AC_VERB_SET_PIN_WIDGET_CONTROL, pincfg);
	if (error < 0)
		return error;
	else
		return 1;
}

static unsigned int stac92xx_vref_get(struct hda_codec *codec, hda_nid_t nid)
{
	unsigned int vref;
	vref = snd_hda_codec_read(codec, nid, 0,
				AC_VERB_GET_PIN_WIDGET_CONTROL, 0);
	vref &= AC_PINCTL_VREFEN;
	return vref;
}

static int stac92xx_mux_enum_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct sigmatel_spec *spec = codec->spec;
	return snd_hda_input_mux_info(spec->input_mux, uinfo);
}

static int stac92xx_mux_enum_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct sigmatel_spec *spec = codec->spec;
	unsigned int adc_idx = snd_ctl_get_ioffidx(kcontrol, &ucontrol->id);

	ucontrol->value.enumerated.item[0] = spec->cur_mux[adc_idx];
	return 0;
}

static int stac92xx_mux_enum_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct sigmatel_spec *spec = codec->spec;
	unsigned int adc_idx = snd_ctl_get_ioffidx(kcontrol, &ucontrol->id);
	const struct hda_input_mux *imux = spec->input_mux;
	unsigned int idx, prev_idx;

	idx = ucontrol->value.enumerated.item[0];
	if (idx >= imux->num_items)
		idx = imux->num_items - 1;
	prev_idx = spec->cur_mux[adc_idx];
	if (prev_idx == idx)
		return 0;
	if (idx < spec->num_analog_muxes) {
		snd_hda_codec_write_cache(codec, spec->mux_nids[adc_idx], 0,
					  AC_VERB_SET_CONNECT_SEL,
					  imux->items[idx].index);
		if (prev_idx >= spec->num_analog_muxes) {
			imux = spec->dinput_mux;
			/* 0 = analog */
			snd_hda_codec_write_cache(codec,
						  spec->dmux_nids[adc_idx], 0,
						  AC_VERB_SET_CONNECT_SEL,
						  imux->items[0].index);
		}
	} else {
		imux = spec->dinput_mux;
		snd_hda_codec_write_cache(codec, spec->dmux_nids[adc_idx], 0,
					  AC_VERB_SET_CONNECT_SEL,
					  imux->items[idx - 1].index);
	}
	spec->cur_mux[adc_idx] = idx;
	return 1;
}

static int stac92xx_mono_mux_enum_info(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct sigmatel_spec *spec = codec->spec;
	return snd_hda_input_mux_info(spec->mono_mux, uinfo);
}

static int stac92xx_mono_mux_enum_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct sigmatel_spec *spec = codec->spec;

	ucontrol->value.enumerated.item[0] = spec->cur_mmux;
	return 0;
}

static int stac92xx_mono_mux_enum_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct sigmatel_spec *spec = codec->spec;

	return snd_hda_input_mux_put(codec, spec->mono_mux, ucontrol,
				     spec->mono_nid, &spec->cur_mmux);
}

#define stac92xx_aloopback_info snd_ctl_boolean_mono_info

static int stac92xx_aloopback_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	unsigned int idx = snd_ctl_get_ioffidx(kcontrol, &ucontrol->id);
	struct sigmatel_spec *spec = codec->spec;

	ucontrol->value.integer.value[0] = !!(spec->aloopback &
					      (spec->aloopback_mask << idx));
	return 0;
}

static int stac92xx_aloopback_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct sigmatel_spec *spec = codec->spec;
	unsigned int idx = snd_ctl_get_ioffidx(kcontrol, &ucontrol->id);
	unsigned int dac_mode;
	unsigned int val, idx_val;

	idx_val = spec->aloopback_mask << idx;
	if (ucontrol->value.integer.value[0])
		val = spec->aloopback | idx_val;
	else
		val = spec->aloopback & ~idx_val;
	if (spec->aloopback == val)
		return 0;

	spec->aloopback = val;

	/* Only return the bits defined by the shift value of the
	 * first two bytes of the mask
	 */
	dac_mode = snd_hda_codec_read(codec, codec->afg, 0,
				      kcontrol->private_value & 0xFFFF, 0x0);
	dac_mode >>= spec->aloopback_shift;

	if (spec->aloopback & idx_val) {
		snd_hda_power_up(codec);
		dac_mode |= idx_val;
	} else {
		snd_hda_power_down(codec);
		dac_mode &= ~idx_val;
	}

	snd_hda_codec_write_cache(codec, codec->afg, 0,
		kcontrol->private_value >> 16, dac_mode);

	return 1;
}

static struct hda_verb stac9200_core_init[] = {
	/* set dac0mux for dac converter */
	{ 0x07, AC_VERB_SET_CONNECT_SEL, 0x00},
	{}
};

static struct hda_verb stac9200_eapd_init[] = {
	/* set dac0mux for dac converter */
	{0x07, AC_VERB_SET_CONNECT_SEL, 0x00},
	{0x08, AC_VERB_SET_EAPD_BTLENABLE, 0x02},
	{}
};

static struct hda_verb dell_eq_core_init[] = {
	/* set master volume to max value without distortion
	 * and direct control */
	{ 0x1f, AC_VERB_SET_VOLUME_KNOB_CONTROL, 0xec},
	{}
};

static struct hda_verb stac92hd73xx_core_init[] = {
	/* set master volume and direct control */
	{ 0x1f, AC_VERB_SET_VOLUME_KNOB_CONTROL, 0xff},
	{}
};

static struct hda_verb stac92hd83xxx_core_init[] = {
	/* power state controls amps */
	{ 0x01, AC_VERB_SET_EAPD, 1 << 2},
	{}
};

static struct hda_verb stac92hd71bxx_core_init[] = {
	/* set master volume and direct control */
	{ 0x28, AC_VERB_SET_VOLUME_KNOB_CONTROL, 0xff},
	{}
};

static struct hda_verb stac92hd71bxx_unmute_core_init[] = {
	/* unmute right and left channels for nodes 0x0f, 0xa, 0x0d */
	{ 0x0f, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_UNMUTE(0)},
	{ 0x0a, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_UNMUTE(0)},
	{ 0x0d, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_UNMUTE(0)},
	{}
};

static struct hda_verb stac925x_core_init[] = {
	/* set dac0mux for dac converter */
	{ 0x06, AC_VERB_SET_CONNECT_SEL, 0x00},
	/* mute the master volume */
	{ 0x0e, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE },
	{}
};

static struct hda_verb stac922x_core_init[] = {
	/* set master volume and direct control */	
	{ 0x16, AC_VERB_SET_VOLUME_KNOB_CONTROL, 0xff},
	{}
};

static struct hda_verb d965_core_init[] = {
	/* set master volume and direct control */	
	{ 0x24, AC_VERB_SET_VOLUME_KNOB_CONTROL, 0xff},
	/* unmute node 0x1b */
	{ 0x1b, AC_VERB_SET_AMP_GAIN_MUTE, 0xb000},
	/* select node 0x03 as DAC */	
	{ 0x0b, AC_VERB_SET_CONNECT_SEL, 0x01},
	{}
};

static struct hda_verb dell_3st_core_init[] = {
	/* don't set delta bit */
	{0x24, AC_VERB_SET_VOLUME_KNOB_CONTROL, 0x7f},
	/* unmute node 0x1b */
	{0x1b, AC_VERB_SET_AMP_GAIN_MUTE, 0xb000},
	/* select node 0x03 as DAC */
	{0x0b, AC_VERB_SET_CONNECT_SEL, 0x01},
	{}
};

static struct hda_verb stac927x_core_init[] = {
	/* set master volume and direct control */	
	{ 0x24, AC_VERB_SET_VOLUME_KNOB_CONTROL, 0xff},
	/* enable analog pc beep path */
	{ 0x01, AC_VERB_SET_DIGI_CONVERT_2, 1 << 5},
	{}
};

static struct hda_verb stac927x_volknob_core_init[] = {
	/* don't set delta bit */
	{0x24, AC_VERB_SET_VOLUME_KNOB_CONTROL, 0x7f},
	/* enable analog pc beep path */
	{0x01, AC_VERB_SET_DIGI_CONVERT_2, 1 << 5},
	{}
};

static struct hda_verb stac9205_core_init[] = {
	/* set master volume and direct control */	
	{ 0x24, AC_VERB_SET_VOLUME_KNOB_CONTROL, 0xff},
	/* enable analog pc beep path */
	{ 0x01, AC_VERB_SET_DIGI_CONVERT_2, 1 << 5},
	{}
};

#define STAC_MONO_MUX \
	{ \
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
		.name = "Mono Mux", \
		.count = 1, \
		.info = stac92xx_mono_mux_enum_info, \
		.get = stac92xx_mono_mux_enum_get, \
		.put = stac92xx_mono_mux_enum_put, \
	}

#define STAC_ANALOG_LOOPBACK(verb_read, verb_write, cnt) \
	{ \
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
		.name  = "Analog Loopback", \
		.count = cnt, \
		.info  = stac92xx_aloopback_info, \
		.get   = stac92xx_aloopback_get, \
		.put   = stac92xx_aloopback_put, \
		.private_value = verb_read | (verb_write << 16), \
	}

#define DC_BIAS(xname, idx, nid) \
	{ \
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
		.name = xname, \
		.index = idx, \
		.info = stac92xx_dc_bias_info, \
		.get = stac92xx_dc_bias_get, \
		.put = stac92xx_dc_bias_put, \
		.private_value = nid, \
	}

static struct snd_kcontrol_new stac9200_mixer[] = {
	HDA_CODEC_VOLUME("Master Playback Volume", 0xb, 0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Master Playback Switch", 0xb, 0, HDA_OUTPUT),
	HDA_CODEC_VOLUME("Capture Volume", 0x0a, 0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Capture Switch", 0x0a, 0, HDA_OUTPUT),
	{ } /* end */
};

static struct snd_kcontrol_new stac92hd73xx_6ch_loopback[] = {
	STAC_ANALOG_LOOPBACK(0xFA0, 0x7A1, 3),
	{}
};

static struct snd_kcontrol_new stac92hd73xx_8ch_loopback[] = {
	STAC_ANALOG_LOOPBACK(0xFA0, 0x7A1, 4),
	{}
};

static struct snd_kcontrol_new stac92hd73xx_10ch_loopback[] = {
	STAC_ANALOG_LOOPBACK(0xFA0, 0x7A1, 5),
	{}
};


static struct snd_kcontrol_new stac92hd71bxx_loopback[] = {
	STAC_ANALOG_LOOPBACK(0xFA0, 0x7A0, 2)
};

static struct snd_kcontrol_new stac925x_mixer[] = {
	HDA_CODEC_VOLUME("Master Playback Volume", 0x0e, 0, HDA_OUTPUT),
	HDA_CODEC_MUTE("Master Playback Switch", 0x0e, 0, HDA_OUTPUT),
	{ } /* end */
};

static struct snd_kcontrol_new stac9205_loopback[] = {
	STAC_ANALOG_LOOPBACK(0xFE0, 0x7E0, 1),
	{}
};

static struct snd_kcontrol_new stac927x_loopback[] = {
	STAC_ANALOG_LOOPBACK(0xFEB, 0x7EB, 1),
	{}
};

static struct snd_kcontrol_new stac_dmux_mixer = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "Digital Input Source",
	/* count set later */
	.info = stac92xx_dmux_enum_info,
	.get = stac92xx_dmux_enum_get,
	.put = stac92xx_dmux_enum_put,
};

static struct snd_kcontrol_new stac_smux_mixer = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "IEC958 Playback Source",
	/* count set later */
	.info = stac92xx_smux_enum_info,
	.get = stac92xx_smux_enum_get,
	.put = stac92xx_smux_enum_put,
};

static const char *slave_vols[] = {
	"Front Playback Volume",
	"Surround Playback Volume",
	"Center Playback Volume",
	"LFE Playback Volume",
	"Side Playback Volume",
	"Headphone Playback Volume",
	"Speaker Playback Volume",
	NULL
};

static const char *slave_sws[] = {
	"Front Playback Switch",
	"Surround Playback Switch",
	"Center Playback Switch",
	"LFE Playback Switch",
	"Side Playback Switch",
	"Headphone Playback Switch",
	"Speaker Playback Switch",
	"IEC958 Playback Switch",
	NULL
};

static void stac92xx_free_kctls(struct hda_codec *codec);
static int stac92xx_add_jack(struct hda_codec *codec, hda_nid_t nid, int type);

static int stac92xx_build_controls(struct hda_codec *codec)
{
	struct sigmatel_spec *spec = codec->spec;
	struct auto_pin_cfg *cfg = &spec->autocfg;
	hda_nid_t nid;
	int err;
	int i;

	if (spec->mixer) {
		err = snd_hda_add_new_ctls(codec, spec->mixer);
		if (err < 0)
			return err;
	}

	for (i = 0; i < spec->num_mixers; i++) {
		err = snd_hda_add_new_ctls(codec, spec->mixers[i]);
		if (err < 0)
			return err;
	}
	if (!spec->auto_mic && spec->num_dmuxes > 0 &&
	    snd_hda_get_bool_hint(codec, "separate_dmux") == 1) {
		stac_dmux_mixer.count = spec->num_dmuxes;
		err = snd_hda_ctl_add(codec,
				  snd_ctl_new1(&stac_dmux_mixer, codec));
		if (err < 0)
			return err;
	}
	if (spec->num_smuxes > 0) {
		int wcaps = get_wcaps(codec, spec->multiout.dig_out_nid);
		struct hda_input_mux *smux = &spec->private_smux;
		/* check for mute support on SPDIF out */
		if (wcaps & AC_WCAP_OUT_AMP) {
			smux->items[smux->num_items].label = "Off";
			smux->items[smux->num_items].index = 0;
			smux->num_items++;
			spec->spdif_mute = 1;
		}
		stac_smux_mixer.count = spec->num_smuxes;
		err = snd_hda_ctl_add(codec,
				  snd_ctl_new1(&stac_smux_mixer, codec));
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
	if (spec->dig_in_nid && !(spec->gpio_dir & 0x01)) {
		err = snd_hda_create_spdif_in_ctls(codec, spec->dig_in_nid);
		if (err < 0)
			return err;
	}

	/* if we have no master control, let's create it */
	if (!snd_hda_find_mixer_ctl(codec, "Master Playback Volume")) {
		unsigned int vmaster_tlv[4];
		snd_hda_set_vmaster_tlv(codec, spec->multiout.dac_nids[0],
					HDA_OUTPUT, vmaster_tlv);
		/* correct volume offset */
		vmaster_tlv[2] += vmaster_tlv[3] * spec->volume_offset;
		err = snd_hda_add_vmaster(codec, "Master Playback Volume",
					  vmaster_tlv, slave_vols);
		if (err < 0)
			return err;
	}
	if (!snd_hda_find_mixer_ctl(codec, "Master Playback Switch")) {
		err = snd_hda_add_vmaster(codec, "Master Playback Switch",
					  NULL, slave_sws);
		if (err < 0)
			return err;
	}

	if (spec->aloopback_ctl &&
	    snd_hda_get_bool_hint(codec, "loopback") == 1) {
		err = snd_hda_add_new_ctls(codec, spec->aloopback_ctl);
		if (err < 0)
			return err;
	}

	stac92xx_free_kctls(codec); /* no longer needed */

	/* create jack input elements */
	if (spec->hp_detect) {
		for (i = 0; i < cfg->hp_outs; i++) {
			int type = SND_JACK_HEADPHONE;
			nid = cfg->hp_pins[i];
			/* jack detection */
			if (cfg->hp_outs == i)
				type |= SND_JACK_LINEOUT;
			err = stac92xx_add_jack(codec, nid, type);
			if (err < 0)
				return err;
		}
	}
	for (i = 0; i < cfg->line_outs; i++) {
		err = stac92xx_add_jack(codec, cfg->line_out_pins[i],
					SND_JACK_LINEOUT);
		if (err < 0)
			return err;
	}
	for (i = 0; i < AUTO_PIN_LAST; i++) {
		nid = cfg->input_pins[i];
		if (nid) {
			err = stac92xx_add_jack(codec, nid,
						SND_JACK_MICROPHONE);
			if (err < 0)
				return err;
		}
	}

	return 0;	
}

static unsigned int ref9200_pin_configs[8] = {
	0x01c47010, 0x01447010, 0x0221401f, 0x01114010,
	0x02a19020, 0x01a19021, 0x90100140, 0x01813122,
};

static unsigned int gateway9200_m4_pin_configs[8] = {
	0x400000fe, 0x404500f4, 0x400100f0, 0x90110010,
	0x400100f1, 0x02a1902e, 0x500000f2, 0x500000f3,
};
static unsigned int gateway9200_m4_2_pin_configs[8] = {
	0x400000fe, 0x404500f4, 0x400100f0, 0x90110010,
	0x400100f1, 0x02a1902e, 0x500000f2, 0x500000f3,
};

/*
    STAC 9200 pin configs for
    102801A8
    102801DE
    102801E8
*/
static unsigned int dell9200_d21_pin_configs[8] = {
	0x400001f0, 0x400001f1, 0x02214030, 0x01014010, 
	0x02a19020, 0x01a19021, 0x90100140, 0x01813122,
};

/* 
    STAC 9200 pin configs for
    102801C0
    102801C1
*/
static unsigned int dell9200_d22_pin_configs[8] = {
	0x400001f0, 0x400001f1, 0x0221401f, 0x01014010, 
	0x01813020, 0x02a19021, 0x90100140, 0x400001f2,
};

/* 
    STAC 9200 pin configs for
    102801C4 (Dell Dimension E310)
    102801C5
    102801C7
    102801D9
    102801DA
    102801E3
*/
static unsigned int dell9200_d23_pin_configs[8] = {
	0x400001f0, 0x400001f1, 0x0221401f, 0x01014010, 
	0x01813020, 0x01a19021, 0x90100140, 0x400001f2, 
};


/* 
    STAC 9200-32 pin configs for
    102801B5 (Dell Inspiron 630m)
    102801D8 (Dell Inspiron 640m)
*/
static unsigned int dell9200_m21_pin_configs[8] = {
	0x40c003fa, 0x03441340, 0x0321121f, 0x90170310,
	0x408003fb, 0x03a11020, 0x401003fc, 0x403003fd,
};

/* 
    STAC 9200-32 pin configs for
    102801C2 (Dell Latitude D620)
    102801C8 
    102801CC (Dell Latitude D820)
    102801D4 
    102801D6 
*/
static unsigned int dell9200_m22_pin_configs[8] = {
	0x40c003fa, 0x0144131f, 0x0321121f, 0x90170310, 
	0x90a70321, 0x03a11020, 0x401003fb, 0x40f000fc,
};

/* 
    STAC 9200-32 pin configs for
    102801CE (Dell XPS M1710)
    102801CF (Dell Precision M90)
*/
static unsigned int dell9200_m23_pin_configs[8] = {
	0x40c003fa, 0x01441340, 0x0421421f, 0x90170310,
	0x408003fb, 0x04a1102e, 0x90170311, 0x403003fc,
};

/*
    STAC 9200-32 pin configs for 
    102801C9
    102801CA
    102801CB (Dell Latitude 120L)
    102801D3
*/
static unsigned int dell9200_m24_pin_configs[8] = {
	0x40c003fa, 0x404003fb, 0x0321121f, 0x90170310, 
	0x408003fc, 0x03a11020, 0x401003fd, 0x403003fe, 
};

/*
    STAC 9200-32 pin configs for
    102801BD (Dell Inspiron E1505n)
    102801EE
    102801EF
*/
static unsigned int dell9200_m25_pin_configs[8] = {
	0x40c003fa, 0x01441340, 0x0421121f, 0x90170310, 
	0x408003fb, 0x04a11020, 0x401003fc, 0x403003fd,
};

/*
    STAC 9200-32 pin configs for
    102801F5 (Dell Inspiron 1501)
    102801F6
*/
static unsigned int dell9200_m26_pin_configs[8] = {
	0x40c003fa, 0x404003fb, 0x0421121f, 0x90170310, 
	0x408003fc, 0x04a11020, 0x401003fd, 0x403003fe,
};

/*
    STAC 9200-32
    102801CD (Dell Inspiron E1705/9400)
*/
static unsigned int dell9200_m27_pin_configs[8] = {
	0x40c003fa, 0x01441340, 0x0421121f, 0x90170310,
	0x90170310, 0x04a11020, 0x90170310, 0x40f003fc,
};

static unsigned int oqo9200_pin_configs[8] = {
	0x40c000f0, 0x404000f1, 0x0221121f, 0x02211210,
	0x90170111, 0x90a70120, 0x400000f2, 0x400000f3,
};


static unsigned int *stac9200_brd_tbl[STAC_9200_MODELS] = {
	[STAC_REF] = ref9200_pin_configs,
	[STAC_9200_OQO] = oqo9200_pin_configs,
	[STAC_9200_DELL_D21] = dell9200_d21_pin_configs,
	[STAC_9200_DELL_D22] = dell9200_d22_pin_configs,
	[STAC_9200_DELL_D23] = dell9200_d23_pin_configs,
	[STAC_9200_DELL_M21] = dell9200_m21_pin_configs,
	[STAC_9200_DELL_M22] = dell9200_m22_pin_configs,
	[STAC_9200_DELL_M23] = dell9200_m23_pin_configs,
	[STAC_9200_DELL_M24] = dell9200_m24_pin_configs,
	[STAC_9200_DELL_M25] = dell9200_m25_pin_configs,
	[STAC_9200_DELL_M26] = dell9200_m26_pin_configs,
	[STAC_9200_DELL_M27] = dell9200_m27_pin_configs,
	[STAC_9200_M4] = gateway9200_m4_pin_configs,
	[STAC_9200_M4_2] = gateway9200_m4_2_pin_configs,
	[STAC_9200_PANASONIC] = ref9200_pin_configs,
};

static const char *stac9200_models[STAC_9200_MODELS] = {
	[STAC_AUTO] = "auto",
	[STAC_REF] = "ref",
	[STAC_9200_OQO] = "oqo",
	[STAC_9200_DELL_D21] = "dell-d21",
	[STAC_9200_DELL_D22] = "dell-d22",
	[STAC_9200_DELL_D23] = "dell-d23",
	[STAC_9200_DELL_M21] = "dell-m21",
	[STAC_9200_DELL_M22] = "dell-m22",
	[STAC_9200_DELL_M23] = "dell-m23",
	[STAC_9200_DELL_M24] = "dell-m24",
	[STAC_9200_DELL_M25] = "dell-m25",
	[STAC_9200_DELL_M26] = "dell-m26",
	[STAC_9200_DELL_M27] = "dell-m27",
	[STAC_9200_M4] = "gateway-m4",
	[STAC_9200_M4_2] = "gateway-m4-2",
	[STAC_9200_PANASONIC] = "panasonic",
};

static struct snd_pci_quirk stac9200_cfg_tbl[] = {
	/* SigmaTel reference board */
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x2668,
		      "DFI LanParty", STAC_REF),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DFI, 0x3101,
		      "DFI LanParty", STAC_REF),
	/* Dell laptops have BIOS problem */
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01a8,
		      "unknown Dell", STAC_9200_DELL_D21),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01b5,
		      "Dell Inspiron 630m", STAC_9200_DELL_M21),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01bd,
		      "Dell Inspiron E1505n", STAC_9200_DELL_M25),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01c0,
		      "unknown Dell", STAC_9200_DELL_D22),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01c1,
		      "unknown Dell", STAC_9200_DELL_D22),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01c2,
		      "Dell Latitude D620", STAC_9200_DELL_M22),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01c5,
		      "unknown Dell", STAC_9200_DELL_D23),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01c7,
		      "unknown Dell", STAC_9200_DELL_D23),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01c8,
		      "unknown Dell", STAC_9200_DELL_M22),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01c9,
		      "unknown Dell", STAC_9200_DELL_M24),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01ca,
		      "unknown Dell", STAC_9200_DELL_M24),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01cb,
		      "Dell Latitude 120L", STAC_9200_DELL_M24),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01cc,
		      "Dell Latitude D820", STAC_9200_DELL_M22),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01cd,
		      "Dell Inspiron E1705/9400", STAC_9200_DELL_M27),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01ce,
		      "Dell XPS M1710", STAC_9200_DELL_M23),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01cf,
		      "Dell Precision M90", STAC_9200_DELL_M23),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01d3,
		      "unknown Dell", STAC_9200_DELL_M22),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01d4,
		      "unknown Dell", STAC_9200_DELL_M22),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01d6,
		      "unknown Dell", STAC_9200_DELL_M22),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01d8,
		      "Dell Inspiron 640m", STAC_9200_DELL_M21),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01d9,
		      "unknown Dell", STAC_9200_DELL_D23),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01da,
		      "unknown Dell", STAC_9200_DELL_D23),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01de,
		      "unknown Dell", STAC_9200_DELL_D21),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01e3,
		      "unknown Dell", STAC_9200_DELL_D23),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01e8,
		      "unknown Dell", STAC_9200_DELL_D21),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01ee,
		      "unknown Dell", STAC_9200_DELL_M25),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01ef,
		      "unknown Dell", STAC_9200_DELL_M25),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01f5,
		      "Dell Inspiron 1501", STAC_9200_DELL_M26),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01f6,
		      "unknown Dell", STAC_9200_DELL_M26),
	/* Panasonic */
	SND_PCI_QUIRK(0x10f7, 0x8338, "Panasonic CF-74", STAC_9200_PANASONIC),
	/* Gateway machines needs EAPD to be set on resume */
	SND_PCI_QUIRK(0x107b, 0x0205, "Gateway S-7110M", STAC_9200_M4),
	SND_PCI_QUIRK(0x107b, 0x0317, "Gateway MT3423, MX341*", STAC_9200_M4_2),
	SND_PCI_QUIRK(0x107b, 0x0318, "Gateway ML3019, MT3707", STAC_9200_M4_2),
	/* OQO Mobile */
	SND_PCI_QUIRK(0x1106, 0x3288, "OQO Model 2", STAC_9200_OQO),
	{} /* terminator */
};

static unsigned int ref925x_pin_configs[8] = {
	0x40c003f0, 0x424503f2, 0x01813022, 0x02a19021,
	0x90a70320, 0x02214210, 0x01019020, 0x9033032e,
};

static unsigned int stac925xM1_pin_configs[8] = {
	0x40c003f4, 0x424503f2, 0x400000f3, 0x02a19020,
	0x40a000f0, 0x90100210, 0x400003f1, 0x9033032e,
};

static unsigned int stac925xM1_2_pin_configs[8] = {
	0x40c003f4, 0x424503f2, 0x400000f3, 0x02a19020,
	0x40a000f0, 0x90100210, 0x400003f1, 0x9033032e,
};

static unsigned int stac925xM2_pin_configs[8] = {
	0x40c003f4, 0x424503f2, 0x400000f3, 0x02a19020,
	0x40a000f0, 0x90100210, 0x400003f1, 0x9033032e,
};

static unsigned int stac925xM2_2_pin_configs[8] = {
	0x40c003f4, 0x424503f2, 0x400000f3, 0x02a19020,
	0x40a000f0, 0x90100210, 0x400003f1, 0x9033032e,
};

static unsigned int stac925xM3_pin_configs[8] = {
	0x40c003f4, 0x424503f2, 0x400000f3, 0x02a19020,
	0x40a000f0, 0x90100210, 0x400003f1, 0x503303f3,
};

static unsigned int stac925xM5_pin_configs[8] = {
	0x40c003f4, 0x424503f2, 0x400000f3, 0x02a19020,
	0x40a000f0, 0x90100210, 0x400003f1, 0x9033032e,
};

static unsigned int stac925xM6_pin_configs[8] = {
	0x40c003f4, 0x424503f2, 0x400000f3, 0x02a19020,
	0x40a000f0, 0x90100210, 0x400003f1, 0x90330320,
};

static unsigned int *stac925x_brd_tbl[STAC_925x_MODELS] = {
	[STAC_REF] = ref925x_pin_configs,
	[STAC_M1] = stac925xM1_pin_configs,
	[STAC_M1_2] = stac925xM1_2_pin_configs,
	[STAC_M2] = stac925xM2_pin_configs,
	[STAC_M2_2] = stac925xM2_2_pin_configs,
	[STAC_M3] = stac925xM3_pin_configs,
	[STAC_M5] = stac925xM5_pin_configs,
	[STAC_M6] = stac925xM6_pin_configs,
};

static const char *stac925x_models[STAC_925x_MODELS] = {
	[STAC_925x_AUTO] = "auto",
	[STAC_REF] = "ref",
	[STAC_M1] = "m1",
	[STAC_M1_2] = "m1-2",
	[STAC_M2] = "m2",
	[STAC_M2_2] = "m2-2",
	[STAC_M3] = "m3",
	[STAC_M5] = "m5",
	[STAC_M6] = "m6",
};

static struct snd_pci_quirk stac925x_codec_id_cfg_tbl[] = {
	SND_PCI_QUIRK(0x107b, 0x0316, "Gateway M255", STAC_M2),
	SND_PCI_QUIRK(0x107b, 0x0366, "Gateway MP6954", STAC_M5),
	SND_PCI_QUIRK(0x107b, 0x0461, "Gateway NX560XL", STAC_M1),
	SND_PCI_QUIRK(0x107b, 0x0681, "Gateway NX860", STAC_M2),
	SND_PCI_QUIRK(0x107b, 0x0367, "Gateway MX6453", STAC_M1_2),
	/* Not sure about the brand name for those */
	SND_PCI_QUIRK(0x107b, 0x0281, "Gateway mobile", STAC_M1),
	SND_PCI_QUIRK(0x107b, 0x0507, "Gateway mobile", STAC_M3),
	SND_PCI_QUIRK(0x107b, 0x0281, "Gateway mobile", STAC_M6),
	SND_PCI_QUIRK(0x107b, 0x0685, "Gateway mobile", STAC_M2_2),
	{} /* terminator */
};

static struct snd_pci_quirk stac925x_cfg_tbl[] = {
	/* SigmaTel reference board */
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x2668, "DFI LanParty", STAC_REF),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DFI, 0x3101, "DFI LanParty", STAC_REF),
	SND_PCI_QUIRK(0x8384, 0x7632, "Stac9202 Reference Board", STAC_REF),

	/* Default table for unknown ID */
	SND_PCI_QUIRK(0x1002, 0x437b, "Gateway mobile", STAC_M2_2),

	{} /* terminator */
};

static unsigned int ref92hd73xx_pin_configs[13] = {
	0x02214030, 0x02a19040, 0x01a19020, 0x02214030,
	0x0181302e, 0x01014010, 0x01014020, 0x01014030,
	0x02319040, 0x90a000f0, 0x90a000f0, 0x01452050,
	0x01452050,
};

static unsigned int dell_m6_pin_configs[13] = {
	0x0321101f, 0x4f00000f, 0x4f0000f0, 0x90170110,
	0x03a11020, 0x0321101f, 0x4f0000f0, 0x4f0000f0,
	0x4f0000f0, 0x90a60160, 0x4f0000f0, 0x4f0000f0,
	0x4f0000f0,
};

static unsigned int alienware_m17x_pin_configs[13] = {
	0x0321101f, 0x0321101f, 0x03a11020, 0x03014020,
	0x90170110, 0x4f0000f0, 0x4f0000f0, 0x4f0000f0,
	0x4f0000f0, 0x90a60160, 0x4f0000f0, 0x4f0000f0,
	0x904601b0,
};

static unsigned int *stac92hd73xx_brd_tbl[STAC_92HD73XX_MODELS] = {
	[STAC_92HD73XX_REF]	= ref92hd73xx_pin_configs,
	[STAC_DELL_M6_AMIC]	= dell_m6_pin_configs,
	[STAC_DELL_M6_DMIC]	= dell_m6_pin_configs,
	[STAC_DELL_M6_BOTH]	= dell_m6_pin_configs,
	[STAC_DELL_EQ]	= dell_m6_pin_configs,
	[STAC_ALIENWARE_M17X]	= alienware_m17x_pin_configs,
};

static const char *stac92hd73xx_models[STAC_92HD73XX_MODELS] = {
	[STAC_92HD73XX_AUTO] = "auto",
	[STAC_92HD73XX_NO_JD] = "no-jd",
	[STAC_92HD73XX_REF] = "ref",
	[STAC_92HD73XX_INTEL] = "intel",
	[STAC_DELL_M6_AMIC] = "dell-m6-amic",
	[STAC_DELL_M6_DMIC] = "dell-m6-dmic",
	[STAC_DELL_M6_BOTH] = "dell-m6",
	[STAC_DELL_EQ] = "dell-eq",
	[STAC_ALIENWARE_M17X] = "alienware",
};

static struct snd_pci_quirk stac92hd73xx_cfg_tbl[] = {
	/* SigmaTel reference board */
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x2668,
				"DFI LanParty", STAC_92HD73XX_REF),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DFI, 0x3101,
				"DFI LanParty", STAC_92HD73XX_REF),
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x5002,
				"Intel DG45ID", STAC_92HD73XX_INTEL),
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x5003,
				"Intel DG45FC", STAC_92HD73XX_INTEL),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x0254,
				"Dell Studio 1535", STAC_DELL_M6_DMIC),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x0255,
				"unknown Dell", STAC_DELL_M6_DMIC),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x0256,
				"unknown Dell", STAC_DELL_M6_BOTH),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x0257,
				"unknown Dell", STAC_DELL_M6_BOTH),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x025e,
				"unknown Dell", STAC_DELL_M6_AMIC),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x025f,
				"unknown Dell", STAC_DELL_M6_AMIC),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x0271,
				"unknown Dell", STAC_DELL_M6_DMIC),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x0272,
				"unknown Dell", STAC_DELL_M6_DMIC),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x029f,
				"Dell Studio 1537", STAC_DELL_M6_DMIC),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x02a0,
				"Dell Studio 17", STAC_DELL_M6_DMIC),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x02be,
				"Dell Studio 1555", STAC_DELL_M6_DMIC),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x02bd,
				"Dell Studio 1557", STAC_DELL_M6_DMIC),
	{} /* terminator */
};

static struct snd_pci_quirk stac92hd73xx_codec_id_cfg_tbl[] = {
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x02a1,
		      "Alienware M17x", STAC_ALIENWARE_M17X),
	{} /* terminator */
};

static unsigned int ref92hd83xxx_pin_configs[10] = {
	0x02214030, 0x02211010, 0x02a19020, 0x02170130,
	0x01014050, 0x01819040, 0x01014020, 0x90a3014e,
	0x01451160, 0x98560170,
};

static unsigned int dell_s14_pin_configs[10] = {
	0x0221403f, 0x0221101f, 0x02a19020, 0x90170110,
	0x40f000f0, 0x40f000f0, 0x40f000f0, 0x90a60160,
	0x40f000f0, 0x40f000f0,
};

static unsigned int *stac92hd83xxx_brd_tbl[STAC_92HD83XXX_MODELS] = {
	[STAC_92HD83XXX_REF] = ref92hd83xxx_pin_configs,
	[STAC_92HD83XXX_PWR_REF] = ref92hd83xxx_pin_configs,
	[STAC_DELL_S14] = dell_s14_pin_configs,
};

static const char *stac92hd83xxx_models[STAC_92HD83XXX_MODELS] = {
	[STAC_92HD83XXX_AUTO] = "auto",
	[STAC_92HD83XXX_REF] = "ref",
	[STAC_92HD83XXX_PWR_REF] = "mic-ref",
	[STAC_DELL_S14] = "dell-s14",
};

static struct snd_pci_quirk stac92hd83xxx_cfg_tbl[] = {
	/* SigmaTel reference board */
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x2668,
		      "DFI LanParty", STAC_92HD83XXX_REF),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DFI, 0x3101,
		      "DFI LanParty", STAC_92HD83XXX_REF),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x02ba,
		      "unknown Dell", STAC_DELL_S14),
	{} /* terminator */
};

static unsigned int ref92hd71bxx_pin_configs[STAC92HD71BXX_NUM_PINS] = {
	0x02214030, 0x02a19040, 0x01a19020, 0x01014010,
	0x0181302e, 0x01014010, 0x01019020, 0x90a000f0,
	0x90a000f0, 0x01452050, 0x01452050, 0x00000000,
	0x00000000
};

static unsigned int dell_m4_1_pin_configs[STAC92HD71BXX_NUM_PINS] = {
	0x0421101f, 0x04a11221, 0x40f000f0, 0x90170110,
	0x23a1902e, 0x23014250, 0x40f000f0, 0x90a000f0,
	0x40f000f0, 0x4f0000f0, 0x4f0000f0, 0x00000000,
	0x00000000
};

static unsigned int dell_m4_2_pin_configs[STAC92HD71BXX_NUM_PINS] = {
	0x0421101f, 0x04a11221, 0x90a70330, 0x90170110,
	0x23a1902e, 0x23014250, 0x40f000f0, 0x40f000f0,
	0x40f000f0, 0x044413b0, 0x044413b0, 0x00000000,
	0x00000000
};

static unsigned int dell_m4_3_pin_configs[STAC92HD71BXX_NUM_PINS] = {
	0x0421101f, 0x04a11221, 0x90a70330, 0x90170110,
	0x40f000f0, 0x40f000f0, 0x40f000f0, 0x90a000f0,
	0x40f000f0, 0x044413b0, 0x044413b0, 0x00000000,
	0x00000000
};

static unsigned int *stac92hd71bxx_brd_tbl[STAC_92HD71BXX_MODELS] = {
	[STAC_92HD71BXX_REF] = ref92hd71bxx_pin_configs,
	[STAC_DELL_M4_1]	= dell_m4_1_pin_configs,
	[STAC_DELL_M4_2]	= dell_m4_2_pin_configs,
	[STAC_DELL_M4_3]	= dell_m4_3_pin_configs,
	[STAC_HP_M4]		= NULL,
	[STAC_HP_DV5]		= NULL,
	[STAC_HP_HDX]           = NULL,
	[STAC_HP_DV4_1222NR]	= NULL,
};

static const char *stac92hd71bxx_models[STAC_92HD71BXX_MODELS] = {
	[STAC_92HD71BXX_AUTO] = "auto",
	[STAC_92HD71BXX_REF] = "ref",
	[STAC_DELL_M4_1] = "dell-m4-1",
	[STAC_DELL_M4_2] = "dell-m4-2",
	[STAC_DELL_M4_3] = "dell-m4-3",
	[STAC_HP_M4] = "hp-m4",
	[STAC_HP_DV5] = "hp-dv5",
	[STAC_HP_HDX] = "hp-hdx",
	[STAC_HP_DV4_1222NR] = "hp-dv4-1222nr",
};

static struct snd_pci_quirk stac92hd71bxx_cfg_tbl[] = {
	/* SigmaTel reference board */
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x2668,
		      "DFI LanParty", STAC_92HD71BXX_REF),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DFI, 0x3101,
		      "DFI LanParty", STAC_92HD71BXX_REF),
	SND_PCI_QUIRK(PCI_VENDOR_ID_HP, 0x30fb,
		      "HP dv4-1222nr", STAC_HP_DV4_1222NR),
	SND_PCI_QUIRK_MASK(PCI_VENDOR_ID_HP, 0xfff0, 0x1720,
			  "HP", STAC_HP_DV5),
	SND_PCI_QUIRK_MASK(PCI_VENDOR_ID_HP, 0xfff0, 0x3080,
		      "HP", STAC_HP_DV5),
	SND_PCI_QUIRK_MASK(PCI_VENDOR_ID_HP, 0xfff0, 0x30f0,
		      "HP dv4-7", STAC_HP_DV5),
	SND_PCI_QUIRK_MASK(PCI_VENDOR_ID_HP, 0xfff0, 0x3600,
		      "HP dv4-7", STAC_HP_DV5),
	SND_PCI_QUIRK(PCI_VENDOR_ID_HP, 0x3610,
		      "HP HDX", STAC_HP_HDX),  /* HDX18 */
	SND_PCI_QUIRK(PCI_VENDOR_ID_HP, 0x361a,
		      "HP mini 1000", STAC_HP_M4),
	SND_PCI_QUIRK(PCI_VENDOR_ID_HP, 0x361b,
		      "HP HDX", STAC_HP_HDX),  /* HDX16 */
	SND_PCI_QUIRK_MASK(PCI_VENDOR_ID_HP, 0xfff0, 0x3620,
		      "HP dv6", STAC_HP_DV5),
	SND_PCI_QUIRK_MASK(PCI_VENDOR_ID_HP, 0xfff0, 0x7010,
		      "HP", STAC_HP_DV5),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x0233,
				"unknown Dell", STAC_DELL_M4_1),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x0234,
				"unknown Dell", STAC_DELL_M4_1),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x0250,
				"unknown Dell", STAC_DELL_M4_1),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x024f,
				"unknown Dell", STAC_DELL_M4_1),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x024d,
				"unknown Dell", STAC_DELL_M4_1),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x0251,
				"unknown Dell", STAC_DELL_M4_1),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x0277,
				"unknown Dell", STAC_DELL_M4_1),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x0263,
				"unknown Dell", STAC_DELL_M4_2),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x0265,
				"unknown Dell", STAC_DELL_M4_2),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x0262,
				"unknown Dell", STAC_DELL_M4_2),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x0264,
				"unknown Dell", STAC_DELL_M4_2),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x02aa,
				"unknown Dell", STAC_DELL_M4_3),
	{} /* terminator */
};

static unsigned int ref922x_pin_configs[10] = {
	0x01014010, 0x01016011, 0x01012012, 0x0221401f,
	0x01813122, 0x01011014, 0x01441030, 0x01c41030,
	0x40000100, 0x40000100,
};

/*
    STAC 922X pin configs for
    102801A7
    102801AB
    102801A9
    102801D1
    102801D2
*/
static unsigned int dell_922x_d81_pin_configs[10] = {
	0x02214030, 0x01a19021, 0x01111012, 0x01114010,
	0x02a19020, 0x01117011, 0x400001f0, 0x400001f1,
	0x01813122, 0x400001f2,
};

/*
    STAC 922X pin configs for
    102801AC
    102801D0
*/
static unsigned int dell_922x_d82_pin_configs[10] = {
	0x02214030, 0x01a19021, 0x01111012, 0x01114010,
	0x02a19020, 0x01117011, 0x01451140, 0x400001f0,
	0x01813122, 0x400001f1,
};

/*
    STAC 922X pin configs for
    102801BF
*/
static unsigned int dell_922x_m81_pin_configs[10] = {
	0x0321101f, 0x01112024, 0x01111222, 0x91174220,
	0x03a11050, 0x01116221, 0x90a70330, 0x01452340, 
	0x40C003f1, 0x405003f0,
};

/*
    STAC 9221 A1 pin configs for
    102801D7 (Dell XPS M1210)
*/
static unsigned int dell_922x_m82_pin_configs[10] = {
	0x02211211, 0x408103ff, 0x02a1123e, 0x90100310, 
	0x408003f1, 0x0221121f, 0x03451340, 0x40c003f2, 
	0x508003f3, 0x405003f4, 
};

static unsigned int d945gtp3_pin_configs[10] = {
	0x0221401f, 0x01a19022, 0x01813021, 0x01014010,
	0x40000100, 0x40000100, 0x40000100, 0x40000100,
	0x02a19120, 0x40000100,
};

static unsigned int d945gtp5_pin_configs[10] = {
	0x0221401f, 0x01011012, 0x01813024, 0x01014010,
	0x01a19021, 0x01016011, 0x01452130, 0x40000100,
	0x02a19320, 0x40000100,
};

static unsigned int intel_mac_v1_pin_configs[10] = {
	0x0121e21f, 0x400000ff, 0x9017e110, 0x400000fd,
	0x400000fe, 0x0181e020, 0x1145e030, 0x11c5e240,
	0x400000fc, 0x400000fb,
};

static unsigned int intel_mac_v2_pin_configs[10] = {
	0x0121e21f, 0x90a7012e, 0x9017e110, 0x400000fd,
	0x400000fe, 0x0181e020, 0x1145e230, 0x500000fa,
	0x400000fc, 0x400000fb,
};

static unsigned int intel_mac_v3_pin_configs[10] = {
	0x0121e21f, 0x90a7012e, 0x9017e110, 0x400000fd,
	0x400000fe, 0x0181e020, 0x1145e230, 0x11c5e240,
	0x400000fc, 0x400000fb,
};

static unsigned int intel_mac_v4_pin_configs[10] = {
	0x0321e21f, 0x03a1e02e, 0x9017e110, 0x9017e11f,
	0x400000fe, 0x0381e020, 0x1345e230, 0x13c5e240,
	0x400000fc, 0x400000fb,
};

static unsigned int intel_mac_v5_pin_configs[10] = {
	0x0321e21f, 0x03a1e02e, 0x9017e110, 0x9017e11f,
	0x400000fe, 0x0381e020, 0x1345e230, 0x13c5e240,
	0x400000fc, 0x400000fb,
};

static unsigned int ecs202_pin_configs[10] = {
	0x0221401f, 0x02a19020, 0x01a19020, 0x01114010,
	0x408000f0, 0x01813022, 0x074510a0, 0x40c400f1,
	0x9037012e, 0x40e000f2,
};

static unsigned int *stac922x_brd_tbl[STAC_922X_MODELS] = {
	[STAC_D945_REF] = ref922x_pin_configs,
	[STAC_D945GTP3] = d945gtp3_pin_configs,
	[STAC_D945GTP5] = d945gtp5_pin_configs,
	[STAC_INTEL_MAC_V1] = intel_mac_v1_pin_configs,
	[STAC_INTEL_MAC_V2] = intel_mac_v2_pin_configs,
	[STAC_INTEL_MAC_V3] = intel_mac_v3_pin_configs,
	[STAC_INTEL_MAC_V4] = intel_mac_v4_pin_configs,
	[STAC_INTEL_MAC_V5] = intel_mac_v5_pin_configs,
	[STAC_INTEL_MAC_AUTO] = intel_mac_v3_pin_configs,
	/* for backward compatibility */
	[STAC_MACMINI] = intel_mac_v3_pin_configs,
	[STAC_MACBOOK] = intel_mac_v5_pin_configs,
	[STAC_MACBOOK_PRO_V1] = intel_mac_v3_pin_configs,
	[STAC_MACBOOK_PRO_V2] = intel_mac_v3_pin_configs,
	[STAC_IMAC_INTEL] = intel_mac_v2_pin_configs,
	[STAC_IMAC_INTEL_20] = intel_mac_v3_pin_configs,
	[STAC_ECS_202] = ecs202_pin_configs,
	[STAC_922X_DELL_D81] = dell_922x_d81_pin_configs,
	[STAC_922X_DELL_D82] = dell_922x_d82_pin_configs,	
	[STAC_922X_DELL_M81] = dell_922x_m81_pin_configs,
	[STAC_922X_DELL_M82] = dell_922x_m82_pin_configs,	
};

static const char *stac922x_models[STAC_922X_MODELS] = {
	[STAC_922X_AUTO] = "auto",
	[STAC_D945_REF]	= "ref",
	[STAC_D945GTP5]	= "5stack",
	[STAC_D945GTP3]	= "3stack",
	[STAC_INTEL_MAC_V1] = "intel-mac-v1",
	[STAC_INTEL_MAC_V2] = "intel-mac-v2",
	[STAC_INTEL_MAC_V3] = "intel-mac-v3",
	[STAC_INTEL_MAC_V4] = "intel-mac-v4",
	[STAC_INTEL_MAC_V5] = "intel-mac-v5",
	[STAC_INTEL_MAC_AUTO] = "intel-mac-auto",
	/* for backward compatibility */
	[STAC_MACMINI]	= "macmini",
	[STAC_MACBOOK]	= "macbook",
	[STAC_MACBOOK_PRO_V1]	= "macbook-pro-v1",
	[STAC_MACBOOK_PRO_V2]	= "macbook-pro",
	[STAC_IMAC_INTEL] = "imac-intel",
	[STAC_IMAC_INTEL_20] = "imac-intel-20",
	[STAC_ECS_202] = "ecs202",
	[STAC_922X_DELL_D81] = "dell-d81",
	[STAC_922X_DELL_D82] = "dell-d82",
	[STAC_922X_DELL_M81] = "dell-m81",
	[STAC_922X_DELL_M82] = "dell-m82",
};

static struct snd_pci_quirk stac922x_cfg_tbl[] = {
	/* SigmaTel reference board */
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x2668,
		      "DFI LanParty", STAC_D945_REF),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DFI, 0x3101,
		      "DFI LanParty", STAC_D945_REF),
	/* Intel 945G based systems */
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x0101,
		      "Intel D945G", STAC_D945GTP3),
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x0202,
		      "Intel D945G", STAC_D945GTP3),
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x0606,
		      "Intel D945G", STAC_D945GTP3),
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x0601,
		      "Intel D945G", STAC_D945GTP3),
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x0111,
		      "Intel D945G", STAC_D945GTP3),
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x1115,
		      "Intel D945G", STAC_D945GTP3),
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x1116,
		      "Intel D945G", STAC_D945GTP3),
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x1117,
		      "Intel D945G", STAC_D945GTP3),
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x1118,
		      "Intel D945G", STAC_D945GTP3),
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x1119,
		      "Intel D945G", STAC_D945GTP3),
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x8826,
		      "Intel D945G", STAC_D945GTP3),
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x5049,
		      "Intel D945G", STAC_D945GTP3),
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x5055,
		      "Intel D945G", STAC_D945GTP3),
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x5048,
		      "Intel D945G", STAC_D945GTP3),
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x0110,
		      "Intel D945G", STAC_D945GTP3),
	/* Intel D945G 5-stack systems */
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x0404,
		      "Intel D945G", STAC_D945GTP5),
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x0303,
		      "Intel D945G", STAC_D945GTP5),
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x0013,
		      "Intel D945G", STAC_D945GTP5),
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x0417,
		      "Intel D945G", STAC_D945GTP5),
	/* Intel 945P based systems */
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x0b0b,
		      "Intel D945P", STAC_D945GTP3),
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x0112,
		      "Intel D945P", STAC_D945GTP3),
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x0d0d,
		      "Intel D945P", STAC_D945GTP3),
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x0909,
		      "Intel D945P", STAC_D945GTP3),
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x0505,
		      "Intel D945P", STAC_D945GTP3),
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x0707,
		      "Intel D945P", STAC_D945GTP5),
	/* other intel */
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x0204,
		      "Intel D945", STAC_D945_REF),
	/* other systems  */
	/* Apple Intel Mac (Mac Mini, MacBook, MacBook Pro...) */
	SND_PCI_QUIRK(0x8384, 0x7680,
		      "Mac", STAC_INTEL_MAC_AUTO),
	/* Dell systems  */
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01a7,
		      "unknown Dell", STAC_922X_DELL_D81),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01a9,
		      "unknown Dell", STAC_922X_DELL_D81),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01ab,
		      "unknown Dell", STAC_922X_DELL_D81),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01ac,
		      "unknown Dell", STAC_922X_DELL_D82),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01bf,
		      "unknown Dell", STAC_922X_DELL_M81),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01d0,
		      "unknown Dell", STAC_922X_DELL_D82),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01d1,
		      "unknown Dell", STAC_922X_DELL_D81),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01d2,
		      "unknown Dell", STAC_922X_DELL_D81),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01d7,
		      "Dell XPS M1210", STAC_922X_DELL_M82),
	/* ECS/PC Chips boards */
	SND_PCI_QUIRK_MASK(0x1019, 0xf000, 0x2000,
		      "ECS/PC chips", STAC_ECS_202),
	{} /* terminator */
};

static unsigned int ref927x_pin_configs[14] = {
	0x02214020, 0x02a19080, 0x0181304e, 0x01014010,
	0x01a19040, 0x01011012, 0x01016011, 0x0101201f, 
	0x183301f0, 0x18a001f0, 0x18a001f0, 0x01442070,
	0x01c42190, 0x40000100,
};

static unsigned int d965_3st_pin_configs[14] = {
	0x0221401f, 0x02a19120, 0x40000100, 0x01014011,
	0x01a19021, 0x01813024, 0x40000100, 0x40000100,
	0x40000100, 0x40000100, 0x40000100, 0x40000100,
	0x40000100, 0x40000100
};

static unsigned int d965_5st_pin_configs[14] = {
	0x02214020, 0x02a19080, 0x0181304e, 0x01014010,
	0x01a19040, 0x01011012, 0x01016011, 0x40000100,
	0x40000100, 0x40000100, 0x40000100, 0x01442070,
	0x40000100, 0x40000100
};

static unsigned int d965_5st_no_fp_pin_configs[14] = {
	0x40000100, 0x40000100, 0x0181304e, 0x01014010,
	0x01a19040, 0x01011012, 0x01016011, 0x40000100,
	0x40000100, 0x40000100, 0x40000100, 0x01442070,
	0x40000100, 0x40000100
};

static unsigned int dell_3st_pin_configs[14] = {
	0x02211230, 0x02a11220, 0x01a19040, 0x01114210,
	0x01111212, 0x01116211, 0x01813050, 0x01112214,
	0x403003fa, 0x90a60040, 0x90a60040, 0x404003fb,
	0x40c003fc, 0x40000100
};

static unsigned int *stac927x_brd_tbl[STAC_927X_MODELS] = {
	[STAC_D965_REF_NO_JD] = ref927x_pin_configs,
	[STAC_D965_REF]  = ref927x_pin_configs,
	[STAC_D965_3ST]  = d965_3st_pin_configs,
	[STAC_D965_5ST]  = d965_5st_pin_configs,
	[STAC_D965_5ST_NO_FP]  = d965_5st_no_fp_pin_configs,
	[STAC_DELL_3ST]  = dell_3st_pin_configs,
	[STAC_DELL_BIOS] = NULL,
	[STAC_927X_VOLKNOB] = NULL,
};

static const char *stac927x_models[STAC_927X_MODELS] = {
	[STAC_927X_AUTO]	= "auto",
	[STAC_D965_REF_NO_JD]	= "ref-no-jd",
	[STAC_D965_REF]		= "ref",
	[STAC_D965_3ST]		= "3stack",
	[STAC_D965_5ST]		= "5stack",
	[STAC_D965_5ST_NO_FP]	= "5stack-no-fp",
	[STAC_DELL_3ST]		= "dell-3stack",
	[STAC_DELL_BIOS]	= "dell-bios",
	[STAC_927X_VOLKNOB]	= "volknob",
};

static struct snd_pci_quirk stac927x_cfg_tbl[] = {
	/* SigmaTel reference board */
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x2668,
		      "DFI LanParty", STAC_D965_REF),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DFI, 0x3101,
		      "DFI LanParty", STAC_D965_REF),
	 /* Intel 946 based systems */
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x3d01, "Intel D946", STAC_D965_3ST),
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0xa301, "Intel D946", STAC_D965_3ST),
	/* 965 based 3 stack systems */
	SND_PCI_QUIRK_MASK(PCI_VENDOR_ID_INTEL, 0xff00, 0x2100,
			   "Intel D965", STAC_D965_3ST),
	SND_PCI_QUIRK_MASK(PCI_VENDOR_ID_INTEL, 0xff00, 0x2000,
			   "Intel D965", STAC_D965_3ST),
	/* Dell 3 stack systems */
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL,  0x01f7, "Dell XPS M1730", STAC_DELL_3ST),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL,  0x01dd, "Dell Dimension E520", STAC_DELL_3ST),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL,  0x01ed, "Dell     ", STAC_DELL_3ST),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL,  0x01f4, "Dell     ", STAC_DELL_3ST),
	/* Dell 3 stack systems with verb table in BIOS */
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL,  0x01f3, "Dell Inspiron 1420", STAC_DELL_BIOS),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL,  0x0227, "Dell Vostro 1400  ", STAC_DELL_BIOS),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL,  0x022e, "Dell     ", STAC_DELL_BIOS),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL,  0x022f, "Dell Inspiron 1525", STAC_DELL_BIOS),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL,  0x0242, "Dell     ", STAC_DELL_BIOS),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL,  0x0243, "Dell     ", STAC_DELL_BIOS),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL,  0x02ff, "Dell     ", STAC_DELL_BIOS),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL,  0x0209, "Dell XPS 1330", STAC_DELL_BIOS),
	/* 965 based 5 stack systems */
	SND_PCI_QUIRK_MASK(PCI_VENDOR_ID_INTEL, 0xff00, 0x2300,
			   "Intel D965", STAC_D965_5ST),
	SND_PCI_QUIRK_MASK(PCI_VENDOR_ID_INTEL, 0xff00, 0x2500,
			   "Intel D965", STAC_D965_5ST),
	/* volume-knob fixes */
	SND_PCI_QUIRK_VENDOR(0x10cf, "FSC", STAC_927X_VOLKNOB),
	{} /* terminator */
};

static unsigned int ref9205_pin_configs[12] = {
	0x40000100, 0x40000100, 0x01016011, 0x01014010,
	0x01813122, 0x01a19021, 0x01019020, 0x40000100,
	0x90a000f0, 0x90a000f0, 0x01441030, 0x01c41030
};

/*
    STAC 9205 pin configs for
    102801F1
    102801F2
    102801FC
    102801FD
    10280204
    1028021F
    10280228 (Dell Vostro 1500)
*/
static unsigned int dell_9205_m42_pin_configs[12] = {
	0x0321101F, 0x03A11020, 0x400003FA, 0x90170310,
	0x400003FB, 0x400003FC, 0x400003FD, 0x40F000F9,
	0x90A60330, 0x400003FF, 0x0144131F, 0x40C003FE,
};

/*
    STAC 9205 pin configs for
    102801F9
    102801FA
    102801FE
    102801FF (Dell Precision M4300)
    10280206
    10280200
    10280201
*/
static unsigned int dell_9205_m43_pin_configs[12] = {
	0x0321101f, 0x03a11020, 0x90a70330, 0x90170310,
	0x400000fe, 0x400000ff, 0x400000fd, 0x40f000f9,
	0x400000fa, 0x400000fc, 0x0144131f, 0x40c003f8,
};

static unsigned int dell_9205_m44_pin_configs[12] = {
	0x0421101f, 0x04a11020, 0x400003fa, 0x90170310,
	0x400003fb, 0x400003fc, 0x400003fd, 0x400003f9,
	0x90a60330, 0x400003ff, 0x01441340, 0x40c003fe,
};

static unsigned int *stac9205_brd_tbl[STAC_9205_MODELS] = {
	[STAC_9205_REF] = ref9205_pin_configs,
	[STAC_9205_DELL_M42] = dell_9205_m42_pin_configs,
	[STAC_9205_DELL_M43] = dell_9205_m43_pin_configs,
	[STAC_9205_DELL_M44] = dell_9205_m44_pin_configs,
	[STAC_9205_EAPD] = NULL,
};

static const char *stac9205_models[STAC_9205_MODELS] = {
	[STAC_9205_AUTO] = "auto",
	[STAC_9205_REF] = "ref",
	[STAC_9205_DELL_M42] = "dell-m42",
	[STAC_9205_DELL_M43] = "dell-m43",
	[STAC_9205_DELL_M44] = "dell-m44",
	[STAC_9205_EAPD] = "eapd",
};

static struct snd_pci_quirk stac9205_cfg_tbl[] = {
	/* SigmaTel reference board */
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x2668,
		      "DFI LanParty", STAC_9205_REF),
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0xfb30,
		      "SigmaTel", STAC_9205_REF),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DFI, 0x3101,
		      "DFI LanParty", STAC_9205_REF),
	/* Dell */
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01f1,
		      "unknown Dell", STAC_9205_DELL_M42),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01f2,
		      "unknown Dell", STAC_9205_DELL_M42),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01f8,
		      "Dell Precision", STAC_9205_DELL_M43),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01f9,
		      "Dell Precision", STAC_9205_DELL_M43),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01fa,
		      "Dell Precision", STAC_9205_DELL_M43),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01fc,
		      "unknown Dell", STAC_9205_DELL_M42),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01fd,
		      "unknown Dell", STAC_9205_DELL_M42),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01fe,
		      "Dell Precision", STAC_9205_DELL_M43),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01ff,
		      "Dell Precision M4300", STAC_9205_DELL_M43),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x0204,
		      "unknown Dell", STAC_9205_DELL_M42),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x0206,
		      "Dell Precision", STAC_9205_DELL_M43),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x021b,
		      "Dell Precision", STAC_9205_DELL_M43),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x021c,
		      "Dell Precision", STAC_9205_DELL_M43),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x021f,
		      "Dell Inspiron", STAC_9205_DELL_M44),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x0228,
		      "Dell Vostro 1500", STAC_9205_DELL_M42),
	/* Gateway */
	SND_PCI_QUIRK(0x107b, 0x0560, "Gateway T6834c", STAC_9205_EAPD),
	SND_PCI_QUIRK(0x107b, 0x0565, "Gateway T1616", STAC_9205_EAPD),
	{} /* terminator */
};

static void stac92xx_set_config_regs(struct hda_codec *codec,
				     unsigned int *pincfgs)
{
	int i;
	struct sigmatel_spec *spec = codec->spec;

	if (!pincfgs)
		return;

	for (i = 0; i < spec->num_pins; i++)
		if (spec->pin_nids[i] && pincfgs[i])
			snd_hda_codec_set_pincfg(codec, spec->pin_nids[i],
						 pincfgs[i]);
}

/*
 * Analog playback callbacks
 */
static int stac92xx_playback_pcm_open(struct hda_pcm_stream *hinfo,
				      struct hda_codec *codec,
				      struct snd_pcm_substream *substream)
{
	struct sigmatel_spec *spec = codec->spec;
	if (spec->stream_delay)
		msleep(spec->stream_delay);
	return snd_hda_multi_out_analog_open(codec, &spec->multiout, substream,
					     hinfo);
}

static int stac92xx_playback_pcm_prepare(struct hda_pcm_stream *hinfo,
					 struct hda_codec *codec,
					 unsigned int stream_tag,
					 unsigned int format,
					 struct snd_pcm_substream *substream)
{
	struct sigmatel_spec *spec = codec->spec;
	return snd_hda_multi_out_analog_prepare(codec, &spec->multiout, stream_tag, format, substream);
}

static int stac92xx_playback_pcm_cleanup(struct hda_pcm_stream *hinfo,
					struct hda_codec *codec,
					struct snd_pcm_substream *substream)
{
	struct sigmatel_spec *spec = codec->spec;
	return snd_hda_multi_out_analog_cleanup(codec, &spec->multiout);
}

/*
 * Digital playback callbacks
 */
static int stac92xx_dig_playback_pcm_open(struct hda_pcm_stream *hinfo,
					  struct hda_codec *codec,
					  struct snd_pcm_substream *substream)
{
	struct sigmatel_spec *spec = codec->spec;
	return snd_hda_multi_out_dig_open(codec, &spec->multiout);
}

static int stac92xx_dig_playback_pcm_close(struct hda_pcm_stream *hinfo,
					   struct hda_codec *codec,
					   struct snd_pcm_substream *substream)
{
	struct sigmatel_spec *spec = codec->spec;
	return snd_hda_multi_out_dig_close(codec, &spec->multiout);
}

static int stac92xx_dig_playback_pcm_prepare(struct hda_pcm_stream *hinfo,
					 struct hda_codec *codec,
					 unsigned int stream_tag,
					 unsigned int format,
					 struct snd_pcm_substream *substream)
{
	struct sigmatel_spec *spec = codec->spec;
	return snd_hda_multi_out_dig_prepare(codec, &spec->multiout,
					     stream_tag, format, substream);
}

static int stac92xx_dig_playback_pcm_cleanup(struct hda_pcm_stream *hinfo,
					struct hda_codec *codec,
					struct snd_pcm_substream *substream)
{
	struct sigmatel_spec *spec = codec->spec;
	return snd_hda_multi_out_dig_cleanup(codec, &spec->multiout);
}


/*
 * Analog capture callbacks
 */
static int stac92xx_capture_pcm_prepare(struct hda_pcm_stream *hinfo,
					struct hda_codec *codec,
					unsigned int stream_tag,
					unsigned int format,
					struct snd_pcm_substream *substream)
{
	struct sigmatel_spec *spec = codec->spec;
	hda_nid_t nid = spec->adc_nids[substream->number];

	if (spec->powerdown_adcs) {
		msleep(40);
		snd_hda_codec_write(codec, nid, 0,
			AC_VERB_SET_POWER_STATE, AC_PWRST_D0);
	}
	snd_hda_codec_setup_stream(codec, nid, stream_tag, 0, format);
	return 0;
}

static int stac92xx_capture_pcm_cleanup(struct hda_pcm_stream *hinfo,
					struct hda_codec *codec,
					struct snd_pcm_substream *substream)
{
	struct sigmatel_spec *spec = codec->spec;
	hda_nid_t nid = spec->adc_nids[substream->number];

	snd_hda_codec_cleanup_stream(codec, nid);
	if (spec->powerdown_adcs)
		snd_hda_codec_write(codec, nid, 0,
			AC_VERB_SET_POWER_STATE, AC_PWRST_D3);
	return 0;
}

static struct hda_pcm_stream stac92xx_pcm_digital_playback = {
	.substreams = 1,
	.channels_min = 2,
	.channels_max = 2,
	/* NID is set in stac92xx_build_pcms */
	.ops = {
		.open = stac92xx_dig_playback_pcm_open,
		.close = stac92xx_dig_playback_pcm_close,
		.prepare = stac92xx_dig_playback_pcm_prepare,
		.cleanup = stac92xx_dig_playback_pcm_cleanup
	},
};

static struct hda_pcm_stream stac92xx_pcm_digital_capture = {
	.substreams = 1,
	.channels_min = 2,
	.channels_max = 2,
	/* NID is set in stac92xx_build_pcms */
};

static struct hda_pcm_stream stac92xx_pcm_analog_playback = {
	.substreams = 1,
	.channels_min = 2,
	.channels_max = 8,
	.nid = 0x02, /* NID to query formats and rates */
	.ops = {
		.open = stac92xx_playback_pcm_open,
		.prepare = stac92xx_playback_pcm_prepare,
		.cleanup = stac92xx_playback_pcm_cleanup
	},
};

static struct hda_pcm_stream stac92xx_pcm_analog_alt_playback = {
	.substreams = 1,
	.channels_min = 2,
	.channels_max = 2,
	.nid = 0x06, /* NID to query formats and rates */
	.ops = {
		.open = stac92xx_playback_pcm_open,
		.prepare = stac92xx_playback_pcm_prepare,
		.cleanup = stac92xx_playback_pcm_cleanup
	},
};

static struct hda_pcm_stream stac92xx_pcm_analog_capture = {
	.channels_min = 2,
	.channels_max = 2,
	/* NID + .substreams is set in stac92xx_build_pcms */
	.ops = {
		.prepare = stac92xx_capture_pcm_prepare,
		.cleanup = stac92xx_capture_pcm_cleanup
	},
};

static int stac92xx_build_pcms(struct hda_codec *codec)
{
	struct sigmatel_spec *spec = codec->spec;
	struct hda_pcm *info = spec->pcm_rec;

	codec->num_pcms = 1;
	codec->pcm_info = info;

	info->name = "STAC92xx Analog";
	info->stream[SNDRV_PCM_STREAM_PLAYBACK] = stac92xx_pcm_analog_playback;
	info->stream[SNDRV_PCM_STREAM_PLAYBACK].nid =
		spec->multiout.dac_nids[0];
	info->stream[SNDRV_PCM_STREAM_CAPTURE] = stac92xx_pcm_analog_capture;
	info->stream[SNDRV_PCM_STREAM_CAPTURE].nid = spec->adc_nids[0];
	info->stream[SNDRV_PCM_STREAM_CAPTURE].substreams = spec->num_adcs;

	if (spec->alt_switch) {
		codec->num_pcms++;
		info++;
		info->name = "STAC92xx Analog Alt";
		info->stream[SNDRV_PCM_STREAM_PLAYBACK] = stac92xx_pcm_analog_alt_playback;
	}

	if (spec->multiout.dig_out_nid || spec->dig_in_nid) {
		codec->num_pcms++;
		info++;
		info->name = "STAC92xx Digital";
		info->pcm_type = spec->autocfg.dig_out_type[0];
		if (spec->multiout.dig_out_nid) {
			info->stream[SNDRV_PCM_STREAM_PLAYBACK] = stac92xx_pcm_digital_playback;
			info->stream[SNDRV_PCM_STREAM_PLAYBACK].nid = spec->multiout.dig_out_nid;
		}
		if (spec->dig_in_nid) {
			info->stream[SNDRV_PCM_STREAM_CAPTURE] = stac92xx_pcm_digital_capture;
			info->stream[SNDRV_PCM_STREAM_CAPTURE].nid = spec->dig_in_nid;
		}
	}

	return 0;
}

static unsigned int stac92xx_get_default_vref(struct hda_codec *codec,
					hda_nid_t nid)
{
	unsigned int pincap = snd_hda_query_pin_caps(codec, nid);
	pincap = (pincap & AC_PINCAP_VREF) >> AC_PINCAP_VREF_SHIFT;
	if (pincap & AC_PINCAP_VREF_100)
		return AC_PINCTL_VREF_100;
	if (pincap & AC_PINCAP_VREF_80)
		return AC_PINCTL_VREF_80;
	if (pincap & AC_PINCAP_VREF_50)
		return AC_PINCTL_VREF_50;
	if (pincap & AC_PINCAP_VREF_GRD)
		return AC_PINCTL_VREF_GRD;
	return 0;
}

static void stac92xx_auto_set_pinctl(struct hda_codec *codec, hda_nid_t nid, int pin_type)

{
	snd_hda_codec_write_cache(codec, nid, 0,
				  AC_VERB_SET_PIN_WIDGET_CONTROL, pin_type);
}

#define stac92xx_hp_switch_info		snd_ctl_boolean_mono_info

static int stac92xx_hp_switch_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct sigmatel_spec *spec = codec->spec;

	ucontrol->value.integer.value[0] = !!spec->hp_switch;
	return 0;
}

static void stac_issue_unsol_event(struct hda_codec *codec, hda_nid_t nid);

static int stac92xx_hp_switch_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct sigmatel_spec *spec = codec->spec;
	int nid = kcontrol->private_value;
 
	spec->hp_switch = ucontrol->value.integer.value[0] ? nid : 0;

	/* check to be sure that the ports are upto date with
	 * switch changes
	 */
	stac_issue_unsol_event(codec, nid);

	return 1;
}

static int stac92xx_dc_bias_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	int i;
	static char *texts[] = {
		"Mic In", "Line In", "Line Out"
	};

	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct sigmatel_spec *spec = codec->spec;
	hda_nid_t nid = kcontrol->private_value;

	if (nid == spec->mic_switch || nid == spec->line_switch)
		i = 3;
	else
		i = 2;

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->value.enumerated.items = i;
	uinfo->count = 1;
	if (uinfo->value.enumerated.item >= i)
		uinfo->value.enumerated.item = i-1;
	strcpy(uinfo->value.enumerated.name,
		texts[uinfo->value.enumerated.item]);

	return 0;
}

static int stac92xx_dc_bias_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = kcontrol->private_value;
	unsigned int vref = stac92xx_vref_get(codec, nid);

	if (vref == stac92xx_get_default_vref(codec, nid))
		ucontrol->value.enumerated.item[0] = 0;
	else if (vref == AC_PINCTL_VREF_GRD)
		ucontrol->value.enumerated.item[0] = 1;
	else if (vref == AC_PINCTL_VREF_HIZ)
		ucontrol->value.enumerated.item[0] = 2;

	return 0;
}

static int stac92xx_dc_bias_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	unsigned int new_vref = 0;
	int error;
	hda_nid_t nid = kcontrol->private_value;

	if (ucontrol->value.enumerated.item[0] == 0)
		new_vref = stac92xx_get_default_vref(codec, nid);
	else if (ucontrol->value.enumerated.item[0] == 1)
		new_vref = AC_PINCTL_VREF_GRD;
	else if (ucontrol->value.enumerated.item[0] == 2)
		new_vref = AC_PINCTL_VREF_HIZ;
	else
		return 0;

	if (new_vref != stac92xx_vref_get(codec, nid)) {
		error = stac92xx_vref_set(codec, nid, new_vref);
		return error;
	}

	return 0;
}

static int stac92xx_io_switch_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	static char *texts[2];
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct sigmatel_spec *spec = codec->spec;

	if (kcontrol->private_value == spec->line_switch)
		texts[0] = "Line In";
	else
		texts[0] = "Mic In";
	texts[1] = "Line Out";
	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->value.enumerated.items = 2;
	uinfo->count = 1;

	if (uinfo->value.enumerated.item >= 2)
		uinfo->value.enumerated.item = 1;
	strcpy(uinfo->value.enumerated.name,
		texts[uinfo->value.enumerated.item]);

	return 0;
}

static int stac92xx_io_switch_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct sigmatel_spec *spec = codec->spec;
	hda_nid_t nid = kcontrol->private_value;
	int io_idx = (nid == spec->mic_switch) ? 1 : 0;

	ucontrol->value.enumerated.item[0] = spec->io_switch[io_idx];
	return 0;
}

static int stac92xx_io_switch_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
        struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct sigmatel_spec *spec = codec->spec;
	hda_nid_t nid = kcontrol->private_value;
	int io_idx = (nid == spec->mic_switch) ? 1 : 0;
	unsigned short val = !!ucontrol->value.enumerated.item[0];

	spec->io_switch[io_idx] = val;

	if (val)
		stac92xx_auto_set_pinctl(codec, nid, AC_PINCTL_OUT_EN);
	else {
		unsigned int pinctl = AC_PINCTL_IN_EN;
		if (io_idx) /* set VREF for mic */
			pinctl |= stac92xx_get_default_vref(codec, nid);
		stac92xx_auto_set_pinctl(codec, nid, pinctl);
	}

	/* check the auto-mute again: we need to mute/unmute the speaker
	 * appropriately according to the pin direction
	 */
	if (spec->hp_detect)
		stac_issue_unsol_event(codec, nid);

        return 1;
}

#define stac92xx_clfe_switch_info snd_ctl_boolean_mono_info

static int stac92xx_clfe_switch_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct sigmatel_spec *spec = codec->spec;

	ucontrol->value.integer.value[0] = spec->clfe_swap;
	return 0;
}

static int stac92xx_clfe_switch_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct sigmatel_spec *spec = codec->spec;
	hda_nid_t nid = kcontrol->private_value & 0xff;
	unsigned int val = !!ucontrol->value.integer.value[0];

	if (spec->clfe_swap == val)
		return 0;

	spec->clfe_swap = val;

	snd_hda_codec_write_cache(codec, nid, 0, AC_VERB_SET_EAPD_BTLENABLE,
		spec->clfe_swap ? 0x4 : 0x0);

	return 1;
}

#define STAC_CODEC_HP_SWITCH(xname) \
	{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
	  .name = xname, \
	  .index = 0, \
	  .info = stac92xx_hp_switch_info, \
	  .get = stac92xx_hp_switch_get, \
	  .put = stac92xx_hp_switch_put, \
	}

#define STAC_CODEC_IO_SWITCH(xname, xpval) \
	{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
	  .name = xname, \
	  .index = 0, \
          .info = stac92xx_io_switch_info, \
          .get = stac92xx_io_switch_get, \
          .put = stac92xx_io_switch_put, \
          .private_value = xpval, \
	}

#define STAC_CODEC_CLFE_SWITCH(xname, xpval) \
	{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
	  .name = xname, \
	  .index = 0, \
	  .info = stac92xx_clfe_switch_info, \
	  .get = stac92xx_clfe_switch_get, \
	  .put = stac92xx_clfe_switch_put, \
	  .private_value = xpval, \
	}

enum {
	STAC_CTL_WIDGET_VOL,
	STAC_CTL_WIDGET_MUTE,
	STAC_CTL_WIDGET_MONO_MUX,
	STAC_CTL_WIDGET_HP_SWITCH,
	STAC_CTL_WIDGET_IO_SWITCH,
	STAC_CTL_WIDGET_CLFE_SWITCH,
	STAC_CTL_WIDGET_DC_BIAS
};

static struct snd_kcontrol_new stac92xx_control_templates[] = {
	HDA_CODEC_VOLUME(NULL, 0, 0, 0),
	HDA_CODEC_MUTE(NULL, 0, 0, 0),
	STAC_MONO_MUX,
	STAC_CODEC_HP_SWITCH(NULL),
	STAC_CODEC_IO_SWITCH(NULL, 0),
	STAC_CODEC_CLFE_SWITCH(NULL, 0),
	DC_BIAS(NULL, 0, 0),
};

/* add dynamic controls */
static struct snd_kcontrol_new *
stac_control_new(struct sigmatel_spec *spec,
		 struct snd_kcontrol_new *ktemp,
		 const char *name)
{
	struct snd_kcontrol_new *knew;

	snd_array_init(&spec->kctls, sizeof(*knew), 32);
	knew = snd_array_new(&spec->kctls);
	if (!knew)
		return NULL;
	*knew = *ktemp;
	knew->name = kstrdup(name, GFP_KERNEL);
	if (!knew->name) {
		/* roolback */
		memset(knew, 0, sizeof(*knew));
		spec->kctls.alloced--;
		return NULL;
	}
	return knew;
}

static int stac92xx_add_control_temp(struct sigmatel_spec *spec,
				     struct snd_kcontrol_new *ktemp,
				     int idx, const char *name,
				     unsigned long val)
{
	struct snd_kcontrol_new *knew = stac_control_new(spec, ktemp, name);
	if (!knew)
		return -ENOMEM;
	knew->index = idx;
	knew->private_value = val;
	return 0;
}

static inline int stac92xx_add_control_idx(struct sigmatel_spec *spec,
					   int type, int idx, const char *name,
					   unsigned long val)
{
	return stac92xx_add_control_temp(spec,
					 &stac92xx_control_templates[type],
					 idx, name, val);
}


/* add dynamic controls */
static inline int stac92xx_add_control(struct sigmatel_spec *spec, int type,
				       const char *name, unsigned long val)
{
	return stac92xx_add_control_idx(spec, type, 0, name, val);
}

static struct snd_kcontrol_new stac_input_src_temp = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "Input Source",
	.info = stac92xx_mux_enum_info,
	.get = stac92xx_mux_enum_get,
	.put = stac92xx_mux_enum_put,
};

static inline int stac92xx_add_jack_mode_control(struct hda_codec *codec,
						hda_nid_t nid, int idx)
{
	int def_conf = snd_hda_codec_get_pincfg(codec, nid);
	int control = 0;
	struct sigmatel_spec *spec = codec->spec;
	char name[22];

	if (!((get_defcfg_connect(def_conf)) & AC_JACK_PORT_FIXED)) {
		if (stac92xx_get_default_vref(codec, nid) == AC_PINCTL_VREF_GRD
			&& nid == spec->line_switch)
			control = STAC_CTL_WIDGET_IO_SWITCH;
		else if (snd_hda_query_pin_caps(codec, nid)
			& (AC_PINCAP_VREF_GRD << AC_PINCAP_VREF_SHIFT))
			control = STAC_CTL_WIDGET_DC_BIAS;
		else if (nid == spec->mic_switch)
			control = STAC_CTL_WIDGET_IO_SWITCH;
	}

	if (control) {
		strcpy(name, auto_pin_cfg_labels[idx]);
		return stac92xx_add_control(codec->spec, control,
					strcat(name, " Jack Mode"), nid);
	}

	return 0;
}

static int stac92xx_add_input_source(struct sigmatel_spec *spec)
{
	struct snd_kcontrol_new *knew;
	struct hda_input_mux *imux = &spec->private_imux;

	if (spec->auto_mic)
		return 0; /* no need for input source */
	if (!spec->num_adcs || imux->num_items <= 1)
		return 0; /* no need for input source control */
	knew = stac_control_new(spec, &stac_input_src_temp,
				stac_input_src_temp.name);
	if (!knew)
		return -ENOMEM;
	knew->count = spec->num_adcs;
	return 0;
}

/* check whether the line-input can be used as line-out */
static hda_nid_t check_line_out_switch(struct hda_codec *codec)
{
	struct sigmatel_spec *spec = codec->spec;
	struct auto_pin_cfg *cfg = &spec->autocfg;
	hda_nid_t nid;
	unsigned int pincap;

	if (cfg->line_out_type != AUTO_PIN_LINE_OUT)
		return 0;
	nid = cfg->input_pins[AUTO_PIN_LINE];
	pincap = snd_hda_query_pin_caps(codec, nid);
	if (pincap & AC_PINCAP_OUT)
		return nid;
	return 0;
}

/* check whether the mic-input can be used as line-out */
static hda_nid_t check_mic_out_switch(struct hda_codec *codec)
{
	struct sigmatel_spec *spec = codec->spec;
	struct auto_pin_cfg *cfg = &spec->autocfg;
	unsigned int def_conf, pincap;
	unsigned int mic_pin;

	if (cfg->line_out_type != AUTO_PIN_LINE_OUT)
		return 0;
	mic_pin = AUTO_PIN_MIC;
	for (;;) {
		hda_nid_t nid = cfg->input_pins[mic_pin];
		def_conf = snd_hda_codec_get_pincfg(codec, nid);
		/* some laptops have an internal analog microphone
		 * which can't be used as a output */
		if (get_defcfg_connect(def_conf) != AC_JACK_PORT_FIXED) {
			pincap = snd_hda_query_pin_caps(codec, nid);
			if (pincap & AC_PINCAP_OUT)
				return nid;
		}
		if (mic_pin == AUTO_PIN_MIC)
			mic_pin = AUTO_PIN_FRONT_MIC;
		else
			break;
	}
	return 0;
}

static int is_in_dac_nids(struct sigmatel_spec *spec, hda_nid_t nid)
{
	int i;
	
	for (i = 0; i < spec->multiout.num_dacs; i++) {
		if (spec->multiout.dac_nids[i] == nid)
			return 1;
	}

	return 0;
}

static int check_all_dac_nids(struct sigmatel_spec *spec, hda_nid_t nid)
{
	int i;
	if (is_in_dac_nids(spec, nid))
		return 1;
	for (i = 0; i < spec->autocfg.hp_outs; i++)
		if (spec->hp_dacs[i] == nid)
			return 1;
	for (i = 0; i < spec->autocfg.speaker_outs; i++)
		if (spec->speaker_dacs[i] == nid)
			return 1;
	return 0;
}

static hda_nid_t get_unassigned_dac(struct hda_codec *codec, hda_nid_t nid)
{
	struct sigmatel_spec *spec = codec->spec;
	int j, conn_len;
	hda_nid_t conn[HDA_MAX_CONNECTIONS];
	unsigned int wcaps, wtype;

	conn_len = snd_hda_get_connections(codec, nid, conn,
					   HDA_MAX_CONNECTIONS);
	for (j = 0; j < conn_len; j++) {
		wcaps = get_wcaps(codec, conn[j]);
		wtype = get_wcaps_type(wcaps);
		/* we check only analog outputs */
		if (wtype != AC_WID_AUD_OUT || (wcaps & AC_WCAP_DIGITAL))
			continue;
		/* if this route has a free DAC, assign it */
		if (!check_all_dac_nids(spec, conn[j])) {
			if (conn_len > 1) {
				/* select this DAC in the pin's input mux */
				snd_hda_codec_write_cache(codec, nid, 0,
						  AC_VERB_SET_CONNECT_SEL, j);
			}
			return conn[j];
		}
	}
	/* if all DACs are already assigned, connect to the primary DAC */
	if (conn_len > 1) {
		for (j = 0; j < conn_len; j++) {
			if (conn[j] == spec->multiout.dac_nids[0]) {
				snd_hda_codec_write_cache(codec, nid, 0,
						  AC_VERB_SET_CONNECT_SEL, j);
				break;
			}
		}
	}
	return 0;
}

static int add_spec_dacs(struct sigmatel_spec *spec, hda_nid_t nid);
static int add_spec_extra_dacs(struct sigmatel_spec *spec, hda_nid_t nid);

/*
 * Fill in the dac_nids table from the parsed pin configuration
 * This function only works when every pin in line_out_pins[]
 * contains atleast one DAC in its connection list. Some 92xx
 * codecs are not connected directly to a DAC, such as the 9200
 * and 9202/925x. For those, dac_nids[] must be hard-coded.
 */
static int stac92xx_auto_fill_dac_nids(struct hda_codec *codec)
{
	struct sigmatel_spec *spec = codec->spec;
	struct auto_pin_cfg *cfg = &spec->autocfg;
	int i;
	hda_nid_t nid, dac;
	
	for (i = 0; i < cfg->line_outs; i++) {
		nid = cfg->line_out_pins[i];
		dac = get_unassigned_dac(codec, nid);
		if (!dac) {
			if (spec->multiout.num_dacs > 0) {
				/* we have already working output pins,
				 * so let's drop the broken ones again
				 */
				cfg->line_outs = spec->multiout.num_dacs;
				break;
			}
			/* error out, no available DAC found */
			snd_printk(KERN_ERR
				   "%s: No available DAC for pin 0x%x\n",
				   __func__, nid);
			return -ENODEV;
		}
		add_spec_dacs(spec, dac);
	}

	for (i = 0; i < cfg->hp_outs; i++) {
		nid = cfg->hp_pins[i];
		dac = get_unassigned_dac(codec, nid);
		if (dac) {
			if (!spec->multiout.hp_nid)
				spec->multiout.hp_nid = dac;
			else
				add_spec_extra_dacs(spec, dac);
		}
		spec->hp_dacs[i] = dac;
	}

	for (i = 0; i < cfg->speaker_outs; i++) {
		nid = cfg->speaker_pins[i];
		dac = get_unassigned_dac(codec, nid);
		if (dac)
			add_spec_extra_dacs(spec, dac);
		spec->speaker_dacs[i] = dac;
	}

	/* add line-in as output */
	nid = check_line_out_switch(codec);
	if (nid) {
		dac = get_unassigned_dac(codec, nid);
		if (dac) {
			snd_printdd("STAC: Add line-in 0x%x as output %d\n",
				    nid, cfg->line_outs);
			cfg->line_out_pins[cfg->line_outs] = nid;
			cfg->line_outs++;
			spec->line_switch = nid;
			add_spec_dacs(spec, dac);
		}
	}
	/* add mic as output */
	nid = check_mic_out_switch(codec);
	if (nid) {
		dac = get_unassigned_dac(codec, nid);
		if (dac) {
			snd_printdd("STAC: Add mic-in 0x%x as output %d\n",
				    nid, cfg->line_outs);
			cfg->line_out_pins[cfg->line_outs] = nid;
			cfg->line_outs++;
			spec->mic_switch = nid;
			add_spec_dacs(spec, dac);
		}
	}

	snd_printd("stac92xx: dac_nids=%d (0x%x/0x%x/0x%x/0x%x/0x%x)\n",
		   spec->multiout.num_dacs,
		   spec->multiout.dac_nids[0],
		   spec->multiout.dac_nids[1],
		   spec->multiout.dac_nids[2],
		   spec->multiout.dac_nids[3],
		   spec->multiout.dac_nids[4]);

	return 0;
}

/* create volume control/switch for the given prefx type */
static int create_controls_idx(struct hda_codec *codec, const char *pfx,
			       int idx, hda_nid_t nid, int chs)
{
	struct sigmatel_spec *spec = codec->spec;
	char name[32];
	int err;

	if (!spec->check_volume_offset) {
		unsigned int caps, step, nums, db_scale;
		caps = query_amp_caps(codec, nid, HDA_OUTPUT);
		step = (caps & AC_AMPCAP_STEP_SIZE) >>
			AC_AMPCAP_STEP_SIZE_SHIFT;
		step = (step + 1) * 25; /* in .01dB unit */
		nums = (caps & AC_AMPCAP_NUM_STEPS) >>
			AC_AMPCAP_NUM_STEPS_SHIFT;
		db_scale = nums * step;
		/* if dB scale is over -64dB, and finer enough,
		 * let's reduce it to half
		 */
		if (db_scale > 6400 && nums >= 0x1f)
			spec->volume_offset = nums / 2;
		spec->check_volume_offset = 1;
	}

	sprintf(name, "%s Playback Volume", pfx);
	err = stac92xx_add_control_idx(spec, STAC_CTL_WIDGET_VOL, idx, name,
		HDA_COMPOSE_AMP_VAL_OFS(nid, chs, 0, HDA_OUTPUT,
					spec->volume_offset));
	if (err < 0)
		return err;
	sprintf(name, "%s Playback Switch", pfx);
	err = stac92xx_add_control_idx(spec, STAC_CTL_WIDGET_MUTE, idx, name,
				   HDA_COMPOSE_AMP_VAL(nid, chs, 0, HDA_OUTPUT));
	if (err < 0)
		return err;
	return 0;
}

#define create_controls(codec, pfx, nid, chs) \
	create_controls_idx(codec, pfx, 0, nid, chs)

static int add_spec_dacs(struct sigmatel_spec *spec, hda_nid_t nid)
{
	if (spec->multiout.num_dacs > 4) {
		printk(KERN_WARNING "stac92xx: No space for DAC 0x%x\n", nid);
		return 1;
	} else {
		spec->multiout.dac_nids[spec->multiout.num_dacs] = nid;
		spec->multiout.num_dacs++;
	}
	return 0;
}

static int add_spec_extra_dacs(struct sigmatel_spec *spec, hda_nid_t nid)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(spec->multiout.extra_out_nid); i++) {
		if (!spec->multiout.extra_out_nid[i]) {
			spec->multiout.extra_out_nid[i] = nid;
			return 0;
		}
	}
	printk(KERN_WARNING "stac92xx: No space for extra DAC 0x%x\n", nid);
	return 1;
}

/* Create output controls
 * The mixer elements are named depending on the given type (AUTO_PIN_XXX_OUT)
 */
static int create_multi_out_ctls(struct hda_codec *codec, int num_outs,
				 const hda_nid_t *pins,
				 const hda_nid_t *dac_nids,
				 int type)
{
	struct sigmatel_spec *spec = codec->spec;
	static const char *chname[4] = {
		"Front", "Surround", NULL /*CLFE*/, "Side"
	};
	hda_nid_t nid;
	int i, err;
	unsigned int wid_caps;

	for (i = 0; i < num_outs && i < ARRAY_SIZE(chname); i++) {
		if (type == AUTO_PIN_HP_OUT && !spec->hp_detect) {
			wid_caps = get_wcaps(codec, pins[i]);
			if (wid_caps & AC_WCAP_UNSOL_CAP)
				spec->hp_detect = 1;
		}
		nid = dac_nids[i];
		if (!nid)
			continue;
		if (type != AUTO_PIN_HP_OUT && i == 2) {
			/* Center/LFE */
			err = create_controls(codec, "Center", nid, 1);
			if (err < 0)
				return err;
			err = create_controls(codec, "LFE", nid, 2);
			if (err < 0)
				return err;

			wid_caps = get_wcaps(codec, nid);

			if (wid_caps & AC_WCAP_LR_SWAP) {
				err = stac92xx_add_control(spec,
					STAC_CTL_WIDGET_CLFE_SWITCH,
					"Swap Center/LFE Playback Switch", nid);

				if (err < 0)
					return err;
			}

		} else {
			const char *name;
			int idx;
			switch (type) {
			case AUTO_PIN_HP_OUT:
				name = "Headphone";
				idx = i;
				break;
			case AUTO_PIN_SPEAKER_OUT:
				name = "Speaker";
				idx = i;
				break;
			default:
				name = chname[i];
				idx = 0;
				break;
			}
			err = create_controls_idx(codec, name, idx, nid, 3);
			if (err < 0)
				return err;
		}
	}
	return 0;
}

static int stac92xx_add_capvol_ctls(struct hda_codec *codec, unsigned long vol,
				    unsigned long sw, int idx)
{
	int err;
	err = stac92xx_add_control_idx(codec->spec, STAC_CTL_WIDGET_VOL, idx,
				       "Capture Volume", vol);
	if (err < 0)
		return err;
	err = stac92xx_add_control_idx(codec->spec, STAC_CTL_WIDGET_MUTE, idx,
				       "Capture Switch", sw);
	if (err < 0)
		return err;
	return 0;
}

/* add playback controls from the parsed DAC table */
static int stac92xx_auto_create_multi_out_ctls(struct hda_codec *codec,
					       const struct auto_pin_cfg *cfg)
{
	struct sigmatel_spec *spec = codec->spec;
	hda_nid_t nid;
	int err;
	int idx;

	err = create_multi_out_ctls(codec, cfg->line_outs, cfg->line_out_pins,
				    spec->multiout.dac_nids,
				    cfg->line_out_type);
	if (err < 0)
		return err;

	if (cfg->hp_outs > 1 && cfg->line_out_type == AUTO_PIN_LINE_OUT) {
		err = stac92xx_add_control(spec,
			STAC_CTL_WIDGET_HP_SWITCH,
			"Headphone as Line Out Switch",
			cfg->hp_pins[cfg->hp_outs - 1]);
		if (err < 0)
			return err;
	}

	for (idx = AUTO_PIN_MIC; idx <= AUTO_PIN_FRONT_LINE; idx++) {
		nid = cfg->input_pins[idx];
		if (nid) {
			err = stac92xx_add_jack_mode_control(codec, nid, idx);
			if (err < 0)
				return err;
		}
	}

	return 0;
}

/* add playback controls for Speaker and HP outputs */
static int stac92xx_auto_create_hp_ctls(struct hda_codec *codec,
					struct auto_pin_cfg *cfg)
{
	struct sigmatel_spec *spec = codec->spec;
	int err;

	err = create_multi_out_ctls(codec, cfg->hp_outs, cfg->hp_pins,
				    spec->hp_dacs, AUTO_PIN_HP_OUT);
	if (err < 0)
		return err;

	err = create_multi_out_ctls(codec, cfg->speaker_outs, cfg->speaker_pins,
				    spec->speaker_dacs, AUTO_PIN_SPEAKER_OUT);
	if (err < 0)
		return err;

	return 0;
}

/* labels for mono mux outputs */
static const char *stac92xx_mono_labels[4] = {
	"DAC0", "DAC1", "Mixer", "DAC2"
};

/* create mono mux for mono out on capable codecs */
static int stac92xx_auto_create_mono_output_ctls(struct hda_codec *codec)
{
	struct sigmatel_spec *spec = codec->spec;
	struct hda_input_mux *mono_mux = &spec->private_mono_mux;
	int i, num_cons;
	hda_nid_t con_lst[ARRAY_SIZE(stac92xx_mono_labels)];

	num_cons = snd_hda_get_connections(codec,
				spec->mono_nid,
				con_lst,
				HDA_MAX_NUM_INPUTS);
	if (num_cons <= 0 || num_cons > ARRAY_SIZE(stac92xx_mono_labels))
		return -EINVAL;

	for (i = 0; i < num_cons; i++) {
		mono_mux->items[mono_mux->num_items].label =
					stac92xx_mono_labels[i];
		mono_mux->items[mono_mux->num_items].index = i;
		mono_mux->num_items++;
	}

	return stac92xx_add_control(spec, STAC_CTL_WIDGET_MONO_MUX,
				"Mono Mux", spec->mono_nid);
}

/* create PC beep volume controls */
static int stac92xx_auto_create_beep_ctls(struct hda_codec *codec,
						hda_nid_t nid)
{
	struct sigmatel_spec *spec = codec->spec;
	u32 caps = query_amp_caps(codec, nid, HDA_OUTPUT);
	int err;

	/* check for mute support for the the amp */
	if ((caps & AC_AMPCAP_MUTE) >> AC_AMPCAP_MUTE_SHIFT) {
		err = stac92xx_add_control(spec, STAC_CTL_WIDGET_MUTE,
			"PC Beep Playback Switch",
			HDA_COMPOSE_AMP_VAL(nid, 1, 0, HDA_OUTPUT));
			if (err < 0)
				return err;
	}

	/* check to see if there is volume support for the amp */
	if ((caps & AC_AMPCAP_NUM_STEPS) >> AC_AMPCAP_NUM_STEPS_SHIFT) {
		err = stac92xx_add_control(spec, STAC_CTL_WIDGET_VOL,
			"PC Beep Playback Volume",
			HDA_COMPOSE_AMP_VAL(nid, 1, 0, HDA_OUTPUT));
			if (err < 0)
				return err;
	}
	return 0;
}

#ifdef CONFIG_SND_HDA_INPUT_BEEP
#define stac92xx_dig_beep_switch_info snd_ctl_boolean_mono_info

static int stac92xx_dig_beep_switch_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	ucontrol->value.integer.value[0] = codec->beep->enabled;
	return 0;
}

static int stac92xx_dig_beep_switch_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	int enabled = !!ucontrol->value.integer.value[0];
	if (codec->beep->enabled != enabled) {
		codec->beep->enabled = enabled;
		return 1;
	}
	return 0;
}

static struct snd_kcontrol_new stac92xx_dig_beep_ctrl = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.info = stac92xx_dig_beep_switch_info,
	.get = stac92xx_dig_beep_switch_get,
	.put = stac92xx_dig_beep_switch_put,
};

static int stac92xx_beep_switch_ctl(struct hda_codec *codec)
{
	return stac92xx_add_control_temp(codec->spec, &stac92xx_dig_beep_ctrl,
					 0, "PC Beep Playback Switch", 0);
}
#endif

static int stac92xx_auto_create_mux_input_ctls(struct hda_codec *codec)
{
	struct sigmatel_spec *spec = codec->spec;
	int i, j, err = 0;

	for (i = 0; i < spec->num_muxes; i++) {
		hda_nid_t nid;
		unsigned int wcaps;
		unsigned long val;

		nid = spec->mux_nids[i];
		wcaps = get_wcaps(codec, nid);
		if (!(wcaps & AC_WCAP_OUT_AMP))
			continue;

		/* check whether already the same control was created as
		 * normal Capture Volume.
		 */
		val = HDA_COMPOSE_AMP_VAL(nid, 3, 0, HDA_OUTPUT);
		for (j = 0; j < spec->num_caps; j++) {
			if (spec->capvols[j] == val)
				break;
		}
		if (j < spec->num_caps)
			continue;

		err = stac92xx_add_control_idx(spec, STAC_CTL_WIDGET_VOL, i,
					       "Mux Capture Volume", val);
		if (err < 0)
			return err;
	}
	return 0;
};

static const char *stac92xx_spdif_labels[3] = {
	"Digital Playback", "Analog Mux 1", "Analog Mux 2",
};

static int stac92xx_auto_create_spdif_mux_ctls(struct hda_codec *codec)
{
	struct sigmatel_spec *spec = codec->spec;
	struct hda_input_mux *spdif_mux = &spec->private_smux;
	const char **labels = spec->spdif_labels;
	int i, num_cons;
	hda_nid_t con_lst[HDA_MAX_NUM_INPUTS];

	num_cons = snd_hda_get_connections(codec,
				spec->smux_nids[0],
				con_lst,
				HDA_MAX_NUM_INPUTS);
	if (num_cons <= 0)
		return -EINVAL;

	if (!labels)
		labels = stac92xx_spdif_labels;

	for (i = 0; i < num_cons; i++) {
		spdif_mux->items[spdif_mux->num_items].label = labels[i];
		spdif_mux->items[spdif_mux->num_items].index = i;
		spdif_mux->num_items++;
	}

	return 0;
}

/* labels for dmic mux inputs */
static const char *stac92xx_dmic_labels[5] = {
	"Analog Inputs", "Digital Mic 1", "Digital Mic 2",
	"Digital Mic 3", "Digital Mic 4"
};

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

/* create a volume assigned to the given pin (only if supported) */
/* return 1 if the volume control is created */
static int create_elem_capture_vol(struct hda_codec *codec, hda_nid_t nid,
				   const char *label, int direction)
{
	unsigned int caps, nums;
	char name[32];
	int err;

	if (direction == HDA_OUTPUT)
		caps = AC_WCAP_OUT_AMP;
	else
		caps = AC_WCAP_IN_AMP;
	if (!(get_wcaps(codec, nid) & caps))
		return 0;
	caps = query_amp_caps(codec, nid, direction);
	nums = (caps & AC_AMPCAP_NUM_STEPS) >> AC_AMPCAP_NUM_STEPS_SHIFT;
	if (!nums)
		return 0;
	snprintf(name, sizeof(name), "%s Capture Volume", label);
	err = stac92xx_add_control(codec->spec, STAC_CTL_WIDGET_VOL, name,
				    HDA_COMPOSE_AMP_VAL(nid, 3, 0, direction));
	if (err < 0)
		return err;
	return 1;
}

/* create playback/capture controls for input pins on dmic capable codecs */
static int stac92xx_auto_create_dmic_input_ctls(struct hda_codec *codec,
						const struct auto_pin_cfg *cfg)
{
	struct sigmatel_spec *spec = codec->spec;
	struct hda_input_mux *imux = &spec->private_imux;
	struct hda_input_mux *dimux = &spec->private_dimux;
	int err, i, active_mics;
	unsigned int def_conf;

	dimux->items[dimux->num_items].label = stac92xx_dmic_labels[0];
	dimux->items[dimux->num_items].index = 0;
	dimux->num_items++;

	active_mics = 0;
	for (i = 0; i < spec->num_dmics; i++) {
		/* check the validity: sometimes it's a dead vendor-spec node */
		if (get_wcaps_type(get_wcaps(codec, spec->dmic_nids[i]))
		    != AC_WID_PIN)
			continue;
		def_conf = snd_hda_codec_get_pincfg(codec, spec->dmic_nids[i]);
		if (get_defcfg_connect(def_conf) != AC_JACK_PORT_NONE)
			active_mics++;
	}

	for (i = 0; i < spec->num_dmics; i++) {
		hda_nid_t nid;
		int index;
		const char *label;

		nid = spec->dmic_nids[i];
		if (get_wcaps_type(get_wcaps(codec, nid)) != AC_WID_PIN)
			continue;
		def_conf = snd_hda_codec_get_pincfg(codec, nid);
		if (get_defcfg_connect(def_conf) == AC_JACK_PORT_NONE)
			continue;

		index = get_connection_index(codec, spec->dmux_nids[0], nid);
		if (index < 0)
			continue;

		if (active_mics == 1)
			label = "Digital Mic";
		else
			label = stac92xx_dmic_labels[dimux->num_items];

		err = create_elem_capture_vol(codec, nid, label, HDA_INPUT);
		if (err < 0)
			return err;
		if (!err) {
			err = create_elem_capture_vol(codec, nid, label,
						      HDA_OUTPUT);
			if (err < 0)
				return err;
		}

		dimux->items[dimux->num_items].label = label;
		dimux->items[dimux->num_items].index = index;
		dimux->num_items++;
		if (snd_hda_get_bool_hint(codec, "separate_dmux") != 1) {
			imux->items[imux->num_items].label = label;
			imux->items[imux->num_items].index = index;
			imux->num_items++;
		}
	}

	return 0;
}

static int check_mic_pin(struct hda_codec *codec, hda_nid_t nid,
			 hda_nid_t *fixed, hda_nid_t *ext)
{
	unsigned int cfg;

	if (!nid)
		return 0;
	cfg = snd_hda_codec_get_pincfg(codec, nid);
	switch (get_defcfg_connect(cfg)) {
	case AC_JACK_PORT_FIXED:
		if (*fixed)
			return 1; /* already occupied */
		*fixed = nid;
		break;
	case AC_JACK_PORT_COMPLEX:
		if (*ext)
			return 1; /* already occupied */
		*ext = nid;
		break;
	}
	return 0;
}

static int set_mic_route(struct hda_codec *codec,
			 struct sigmatel_mic_route *mic,
			 hda_nid_t pin)
{
	struct sigmatel_spec *spec = codec->spec;
	struct auto_pin_cfg *cfg = &spec->autocfg;
	int i;

	mic->pin = pin;
	for (i = AUTO_PIN_MIC; i <= AUTO_PIN_FRONT_MIC; i++)
		if (pin == cfg->input_pins[i])
			break;
	if (i <= AUTO_PIN_FRONT_MIC) {
		/* analog pin */
		i = get_connection_index(codec, spec->mux_nids[0], pin);
		if (i < 0)
			return -1;
		mic->mux_idx = i;
		mic->dmux_idx = -1;
		if (spec->dmux_nids)
			mic->dmux_idx = get_connection_index(codec,
							     spec->dmux_nids[0],
							     spec->mux_nids[0]);
	}  else if (spec->dmux_nids) {
		/* digital pin */
		i = get_connection_index(codec, spec->dmux_nids[0], pin);
		if (i < 0)
			return -1;
		mic->dmux_idx = i;
		mic->mux_idx = -1;
		if (spec->mux_nids)
			mic->mux_idx = get_connection_index(codec,
							    spec->mux_nids[0],
							    spec->dmux_nids[0]);
	}
	return 0;
}

/* return non-zero if the device is for automatic mic switch */
static int stac_check_auto_mic(struct hda_codec *codec)
{
	struct sigmatel_spec *spec = codec->spec;
	struct auto_pin_cfg *cfg = &spec->autocfg;
	hda_nid_t fixed, ext;
	int i;

	for (i = AUTO_PIN_LINE; i < AUTO_PIN_LAST; i++) {
		if (cfg->input_pins[i])
			return 0; /* must be exclusively mics */
	}
	fixed = ext = 0;
	for (i = AUTO_PIN_MIC; i <= AUTO_PIN_FRONT_MIC; i++)
		if (check_mic_pin(codec, cfg->input_pins[i], &fixed, &ext))
			return 0;
	for (i = 0; i < spec->num_dmics; i++)
		if (check_mic_pin(codec, spec->dmic_nids[i], &fixed, &ext))
			return 0;
	if (!fixed || !ext)
		return 0;
	if (!(get_wcaps(codec, ext) & AC_WCAP_UNSOL_CAP))
		return 0; /* no unsol support */
	if (set_mic_route(codec, &spec->ext_mic, ext) ||
	    set_mic_route(codec, &spec->int_mic, fixed))
		return 0; /* something is wrong */
	return 1;
}

/* create playback/capture controls for input pins */
static int stac92xx_auto_create_analog_input_ctls(struct hda_codec *codec, const struct auto_pin_cfg *cfg)
{
	struct sigmatel_spec *spec = codec->spec;
	struct hda_input_mux *imux = &spec->private_imux;
	int i, j;

	for (i = 0; i < AUTO_PIN_LAST; i++) {
		hda_nid_t nid = cfg->input_pins[i];
		int index, err;

		if (!nid)
			continue;
		index = -1;
		for (j = 0; j < spec->num_muxes; j++) {
			index = get_connection_index(codec, spec->mux_nids[j],
						     nid);
			if (index >= 0)
				break;
		}
		if (index < 0)
			continue;

		err = create_elem_capture_vol(codec, nid,
					      auto_pin_cfg_labels[i],
					      HDA_INPUT);
		if (err < 0)
			return err;

		imux->items[imux->num_items].label = auto_pin_cfg_labels[i];
		imux->items[imux->num_items].index = index;
		imux->num_items++;
	}
	spec->num_analog_muxes = imux->num_items;

	if (imux->num_items) {
		/*
		 * Set the current input for the muxes.
		 * The STAC9221 has two input muxes with identical source
		 * NID lists.  Hopefully this won't get confused.
		 */
		for (i = 0; i < spec->num_muxes; i++) {
			snd_hda_codec_write_cache(codec, spec->mux_nids[i], 0,
						  AC_VERB_SET_CONNECT_SEL,
						  imux->items[0].index);
		}
	}

	return 0;
}

static void stac92xx_auto_init_multi_out(struct hda_codec *codec)
{
	struct sigmatel_spec *spec = codec->spec;
	int i;

	for (i = 0; i < spec->autocfg.line_outs; i++) {
		hda_nid_t nid = spec->autocfg.line_out_pins[i];
		stac92xx_auto_set_pinctl(codec, nid, AC_PINCTL_OUT_EN);
	}
}

static void stac92xx_auto_init_hp_out(struct hda_codec *codec)
{
	struct sigmatel_spec *spec = codec->spec;
	int i;

	for (i = 0; i < spec->autocfg.hp_outs; i++) {
		hda_nid_t pin;
		pin = spec->autocfg.hp_pins[i];
		if (pin) /* connect to front */
			stac92xx_auto_set_pinctl(codec, pin, AC_PINCTL_OUT_EN | AC_PINCTL_HP_EN);
	}
	for (i = 0; i < spec->autocfg.speaker_outs; i++) {
		hda_nid_t pin;
		pin = spec->autocfg.speaker_pins[i];
		if (pin) /* connect to front */
			stac92xx_auto_set_pinctl(codec, pin, AC_PINCTL_OUT_EN);
	}
}

static int stac92xx_parse_auto_config(struct hda_codec *codec, hda_nid_t dig_out, hda_nid_t dig_in)
{
	struct sigmatel_spec *spec = codec->spec;
	int hp_swap = 0;
	int i, err;

	if ((err = snd_hda_parse_pin_def_config(codec,
						&spec->autocfg,
	versaspec->dmic_nids)) < 0)
		return err;
	if (!  Inter/*
 * U.line_outsHigh Defini0; /* can't find valid pin config */

	/* If we have no real audi-outx
 *
and multiple hpoluts, HPs should
	 * be set up asc.
 * -channel outputs.bedd/ Audioodec
 *
 * HD audio in_type == AUTO_PIN_SPEAKER_OUT &&
	   Codec
 *
 * HD hpo int > 1) {
	t (cCopy This dritoley So int, backupan redistr in
	eddespeakeror modso that the following routines Sig handley
 *  HPx
 *sm>
 primary patch_cmeddia.c 	snd_printdd("stac92xx: Enablener*
 * Bmporworkaround\n");
		memcpy patch_realtek.it undered b,Codec
 *
 * HD audio in  Thisare pe thasizeof patch_realtek.c
 * Copyed b) opti later version.
 *
 * r mod=Codec
 *
 * HD audio intoption) any later versionributed in theuse.de>
 *
 *  Thin the hope that it will be useful,
 ee the
T ANY WARRANTY; withand/or modimplied warrantyThis dretails.
 *
 *  You should yright c) 2004 TaHPwai ANY WARRANTY; withThis dri= 0optihp_swap = 1;
	}c and patch_realtek.monoted in t is frint dir = get_wcaps(codecULAR PURPOSE.  S9 Temple Place&iver(AC_WCA this_AMP | .h>
#incINe <l optiu32 , MA = query_amp_, MA  02111ivers1307 USA
 */

#include <li, 330 optihda for_t
 * n_list[1];
 free geof themixer node, Incthenndef.hono mux if it exists Foundand pnd_oundostoh>
#ection <linux/pci.h>
#include <linux/dmi.h>
#inh>
#inclu,er itiwai.h>de "hda_local.h"
#include "hd.h>
#includ0]/pci.hT	= 1,
	STAC_I> 0 is ERT_Eite n, MA, Boston, MA  02111-_EVENT,
	STA opti;

enumcoree
 *  oston, MA00_DE(n, MA_OQO,
	/* LR Softwcheck, some  versi5x5 Embeainclums o
 O,
	 * asedgesf theDACs patch_ path instead ofound9200_DELLh>
#-nclu3,
	.9200_DELound
#incl_9200_DELL=ux/deID_AUD_SELINSERT_E		!0_DELL &ux/delay.LR_SWAP)Highi.h>
#inc9 Temnid =F,
	STAC_9200opti}"
#inclcludis frsound/core.
	STAC1307 USA
 */

#include <li<soun	STAmost.h>
#ir modELL_M21leastM21,
te/unM44, switchAC_9200330, B
	STSTAC_9200_Mlude <l) ? HDAnum PUT :_92HDINPis pro	er0, B version_addcal.trol patc, STAC_CTL00_DGET_MUTE/pci.h"M>
#iPlayute  S05_EA"/pci.h92HDCOMPOSEe <l_VAL(nid, 1, 0includ_OQO,
udioXX_Nel Highgh Definition ATAC_9ELL_D for volu	STAupport17X,
 theampAC_9200udio(4,
	STAC_9AMP#incNUM_STEPSAC_9200>>3XXX_AUTO,
	STAC_92H_SHIFTAC_92053XX_NO_JD, /* no jack-detection *C_9200
	STAC_92HD73XX_VOLS
};

e_92HD73XX_INTELVSTAC_C_DELL_M6_AMIC,
	STAC_DELL_M6_DMIC,
	STAC_DELL_MM6_BOTH,
	STAC_DELLL_EQ,
	STAC_ALI
enu}T,
}JD, /* no uto_seWITHOctl  02111-1307 USA
 */

#include <liS
};

eux/dPINC_92ludeEN optHP_Hudio _PANASO
 * out.num_dacsAC_920XX_NO_JD, /* no P_DVfill	STA for   0211 opti6_BOTH,
	STAC_DEL_EQ,
	STAC_ALSTAC_M2_2,
	STAC_M3,create_	STACLicenctl <linux/pci.hC_92 &odec
 *
 * HD,
	STAC_925x_MODELS
};

enum {
	STHP_H/*lleycom>nalog beepF,
	etecec.h"
and patch_rna_INT,
	STC_EVENTTAC_922X_AUTO,
	STAC_D945_REFV5,
	45GTP3,
	STAC_DNTEL_MAC_V5,
	STA_MAC_V1,
	STAC_INTEL_MAC_V2,
	STAC_INTEL_MAC_V3digitalC_INTEL_MAC_V4, IncinL_M2deviceAC_9#ifdef CONFIG_SND_,
	STAC_9_BEEPSTAC_INTEL_MdigV5,
	STAC_INTEL_MA_AUTO,
	STAC_9205_REF,STAC_MACMINlude nsigneng ttinux/<sounC_AUTO, /* This model is selected if no modTAC_,
	STAC_925x_MODELS
};

enum {
	STAC_922X_de "hdaattachelectee subsC_ECS_202,
	STAC_922X_DELL_D81,
	STAC_922X_DEL (c)DT/
	ST  0211
	STAC_audiarC_INTEtone parameterAC_920 0211->_INT->965_RE_D, /*are
 *
	STifedde_INTE205_EAPis available, make its own , /*etectiux/slab.h>
#include <linux/TAC_,_92HD73XX_A,
	STAC_9ion */
	STAINSERTpe t!AC_92HD83XXX_AUTO,
REF,) REF,
	STAC_92REF,WR_REF,AC_9205XX_NO_JD, /* noV5,
	205_EAed iTAC_M6,
	STM6_BOTH,
	STAC_DELL_EQ,
	STAC_AL}AC_I#endif

IMAC_INTEL,
	STAC_IMAC_INTELh,
	STAC_ECS_205,
	STAC_INTEL_MAC6_BOTH,
	STAC_D Definition NTEL_Aln patch_* nosenerdone, now restoreELS
}Softped hphed bya.c and ee SoftAC_920on) any later versionee the
  driver is distributed in the hope that it will be useful,
 or more details.
 *
 *  Youwrite to tplied warranty of
 *  MERCHNU General Public License
 *  along with this program; if not,  should havehe FREF,
	ST ver_ELL_DAC_M3,micTAC_M6,AC_920odec
 *
 *;
	uSTAC_D965_3onlyTAC_Dcapturhe Gr C_M3-_offFoundaInter2,
	adc	STAC_D96nes */
	unST,
	STint eapd_mask;
muxesigned iHP_Hic:1(i1;
	u i <eapd_swsk;
	uns; i++TEL_MAC_AUTO, /* This jackapvol
	STAC_ECS_20nes */
	unsis[iAC_LO_Eope that led;

	/swtream i_MAC_V1,
	STAC_INTEL_MAC_V2,
	STAC_INTC_922X_AUTO,
	STAC_D945_REF,
	STA_ to tda_nid_t nid;
	int type;
	struct snd_jack *jack;
};

struct and patch_ONIC,
	ST_INTEL_MAC_AUTO, /* This model is se9 Templed char aloopba_MAC_V1,
	STAC_INTEL_MAC_V2,
	STAC_Iwer managem2,
	Smicrive0 && AC_M1,
dned chmuxHigh	STACC_922X_AUTO,
	STAC_D945_REFface ned char aloopbaciversal5,
	STAC_INTEL_ntel Highh Definition Audiot gpio_mask;
	unnsigned int num_pwrs;
	unsigned int *uxts;

	/* playback_t *pwr_nids;
	hda_nid_t *dac_list;

	/* jack detecs
	struct hda_multi_out multiout;
	hda_nid_spdif_t da[5];
	hda_nid_t hp_dacs[5];
	hda_nid_t speaker_kcontrol_new *aloddts;

	/sourcet curtruct snd_jack *jack;
};

struct C_M1,
	STAC_M1_max_ased onhave recei	STAC_M1_2,
	STAC * 2ned int cur_ma_nid_t *dmux_nids;
	u> 2High int ssurr	hda_niigned STAC_INTEL_MA
 * HD digo interfasmux_nids;
	unsd long ,
	STACd long ruct snd loinstruxes;

	unsigned loin Placapvols; /apsws; : HDA_COMP42,
	S int cur_mk45GT.ncluamp-mute ah>
#is[t gpio_mask/* nu++]t eapd_swsigned intdmics;
	hdrray jack =;
	int tpriv_REFimuxn Audio t snd_array jacks;

t snd_array jackl_mic_route ext_midc;
	str;
	unsi
	const char **spdif_labels volda_nid_t 9 Temt char **spdif_labels_t anabe;
ck;
};

1;
}

/* add pXX_INTELL_MAC_V4,ic:1HPel_mic_r*/
staticRO_V2 versi00matel_jack {
	hda_nidstruct ound 0211 *yback */
	st*/
	strC_M3,pin_cfg *TEL_
{
truct snsigmatel_ int *urce AC_9n */
 int;
MACBOOK,
	S
 *
= cfg->ee the
ELS
};ACBOOK_PRO_V2_920,
	STAC_udio C/* amp-ce patch 

	uct hda_, Boston, MA  02111-Placruct sned int cuTAC_9200_MUNSOL_CAPamp-mute ah81,
tectum_analomux;
	unsin widgets */
	hda_nid_t *pin_nidLFE
	unsigned int num_pins;

	/* codec specifilfestuff */
	struct hda_verb *init;
	struct snd_kcontrol_new *mixer;

	/* capture source */
	struct hda_input_mO_V2tion Aound/core.igne_mux;
0xhe FO_V2iuct sibeddesearch it undeM43,
	 IncaudiM43,
	ic:1;.h>
#iutput */pify
 * with an};

. c) 2, /*iitchund,gets  intL_MAC_V4ne-ouic:1itmedia.c  gpio_dir;
	unsigned iTY; without even the &&switch;	/** sharata;
	unsiux *dinput_mux;
 later version.
 *
 *  Thi[iS
};
dmux[2];
	struST,
	STmux[3];
	struct hda_inpu	M4,
	ST= t.h>
#incSTEREOinux/delay.um {
	ST
	STAC_9ivate_dC_9200

enum {
	ST_ALIENWback; /* NID of HP ast */
	unsig, mustedallf_DELL_3	witch;	/* M42,
C_INTEL_if it under the is,
	Sund/jit undes mayedalin and/or moda.c and _mux */
	str
	unsigned long *cout even the i=INTEL_MAinformation */

	/* dynamic coand/or modput_mux */
	struct auto_pin__cfg autocfg;
	struct snd_arrayributed in thuct hdaa_input_mux prdefcfgC_ALIe_dig_82,
	STAC_9ct hda_loc22NRfguct hda_input_muut_muostoe_dig_1,
	STACe_dig_)uct hdaJACKakashi I,
	STAC__input_mux private_dimux;
	struct hda_input_mumux private_imux;
	struct hda_input_mux private_s_smux;
	struct hda_input_mux privatate_mono_mux;
};

static hda_nid_t sta */
	unsi9200_adc_nids[1] = = {
        0x03,	STAC_HPsigned inwitch;	TEL_MAC_AUTO945_REFL_MAC_V4  02111-"LFE",switch;	TAC_MAC_V1,
	STAC_INTEL_MAC_V2,
	STAC_INT cur_amux;
	hint num_pins;

	/* couteeSTAC_D9* Cop */
	struct hda_verb *ine_switch;	/* shared line-in for input and output */
	hda_put_muevents *de "hdaic unsntrodefd long s_t nid;
	int type;
	s, NULLIntel High Definition POSE_AMP_VAL(0_new *aloopback_ctl;
	unsigned char aloopback_mask;
	unsignetac92hd73xx_capvols

#define STAC92HD83_* codec specific stuff c92hd83xxx_mux_nids[2] = {
	0x17, 0x18,
};

static hda_nid_t stac92hd83xxx_adigned intc92hd83xxx_mux_nids[2] = {
	0x17, 0x18,
};

stati cur_mmux;
	struct hda_multi_out multiout;
	hda_nid_t dac_nids[5];
	hda_nid_t hp_dacs[5];
	hda_nid_t speaker
	unsigned int num_muxes;
	hda_nid_t *dmic_nids;
	unsigned int num_dmicg_muxes;

	unsigned long *capvols; /* amp-volume attr: HDA_0x05ned int cur_m long *capsws; /* amp-mute attr: HDA_COMP0x04P_VAL() */
	unsigned int num_caps; /* number of capture volume/switch elements */

	struct sigmatel_mic_route ext_mic;
	strt_mic;

	const char **spdif_labels;

	hdanid;

	/* pin wid
 * Early 2006 Intel Macintosheshda_ni
	ST9220X5AUTO,
	Sseemu caELL_M2_nidfunky external_M44, hda_pcm ue {
	GPIOhed b.
yrighint numvoid92HD8_gpiDV4_1 */
	struct hda_verb *ini ACBOOK_PRO_V2masknids[ x25,
};

#defidir_ne STD71BXX_NUM_DMICataixer;ACBOOK_PRO_V2hd71int e,s[STA
statihd71dis

#d[STAC92HD
	0x26, 0,
};

streadSTAC_REF,
n */
a_cap0nids[2pe thatAC_VERB_3XX_0x1d_DATA, 0_smu{
	0x18, 0x1({
	0x18, 0& ~CS	2
sta) | (tac9 &ICS	2
sta)] = {
	0ne S0x19, 0
};

static hda_nid_t stac92hd71bxx_slave_dig_ots[2] = {
	0x22, MASK

#define S_capv|=ine S] = {
	0330, B9, 0
};

static hda_nid_t stac92hd71bxx_slave_dig_ts[2] = {
	0x22, 0IRECTION

#define S_MOD|A_CO	2
stauct sigC* Copto_m0x1dxm>
 CMOS;

stx26, 0,
};

stwritAC_922X_Dstac92hd71bxx_ 0x7e7

#def_nids[1] = {
        0x0f,
};

static hda_nAC92HDatic hda_nSOMPOSE_AMP_VAL, 3, 0, _smu9, 0
};

static hda_nid_t stac92hd71bxx_slav925X_NUM_DMICS	1
sta925x_adc_ni = {
  ) for syngpio 
	msleep(x_smac925x_dmic_nids[STAC925X_NUM_DMICS + 1] = {
	0x15, 0
};

static hd
};

[STAC92HDdmux_nids[1] =}
em id. */
	/* for backward cofinec hda_nid_t stac9/* nofree_jackon; v */
	str0x21(0x1 *(0x1ixer;

	/* capture soUT),
};

scur_(0x1dif_labels;ata;
	c_nid->ine sta, 0x07,
};UT),
=psws	x;
	nt data;int num_pins;

	/ no jac(0x1 */
	struct hda_verb *init;
_AUTO,
	STAC_,RO_V20_DEixeric unsigned long stac925x_capsws[;

	/* capture source */
	struct hda_input_m hda_nid_t stac922x_adc_nired micTPUT),
}
	0x26, 0,
};

static hda_nid_t sta2,
	STAO_V2,l.h"
#ivity_dimux;s[2] = nid_t sa, 0d lon_smuchar name[32S
};
	HDA_COMPOSE_Anid_t stac927&&_nid_t stac927!

#define PORT_FIXEDput_mux;
	unsign0x21array_init(5,
	STAc_nid,t it wil};

st, 32_smut stac9927x_mux_nnew3] = {
      input_mu!;

stat_mux;
	u-ENOMEM;

stat};

stat	STAC_d_t st0_DELL_0_DE;

stan; eif(stat  0x15, 0stat), "%s at %sda_nJackC_DELVENT,
	STAC_(0x14_9200 {
	0x1f,c927x_dmux_nids[1] = nid_t stac92x1b,
};

#define STAC927X_NUM_Dloca#incx1b,
};

#};

sP_VAL(0x21(0x14stacion */
	us;

	rd,
stat,0x02,, &d_t st;

struct snd_jack *EL_MAd_t stac927xhe Frd_t *dac_list;

3
static u{
        0x06s[2] = c_nids[6]8, 3, 0, HDA_IP_VANO_JD, /* noP_VAL(0x14, 3,;_nids[2]efine STAC92HD73XX_NUM_CAPS	2um_mueven_smux_nidapture source */
	s,ruct ic unsigneAC92HD71BXX_NUM
};

M_CAPS_t stac92hd71ed long stac927xunsig *unsigstac927x_mux_nids[3] = {
 unsig   0x15, 0xunsig0x17
};

0x1c, c hda_nid_t stac927x_smAMP_VAds[1] = { 0, HD1,
};

static hda_niunsigstac927x_dac_nack", " = {
	0x02, 0log Mux 1a{
	0x *stac927x_.useAnalog Mux NPUT),
0x06, (0x1a, 3,ux 2", "AnC92HD73XX_NUCOMPOSE_AMP_VAL(0x1c, 3tac92het unsigned longuct hda_verb *init;
	stpe th_AUTO,
	STAC_ixer;

	/* capture source */
	struct hda_input_mCOMPOSE_AMP_VAL(0x1c, 3, 0, 	struct s3"
};

nts */d mic-in f gpio_dir;
	unsigned i3"
};

statiata;,2] = {a;
	unsi6_BOTck", "ADAT"7x_daono_mux;
	uns 0, HD2hd7 Defini22x_mux_0x12, 0x13
};

static hda_nid_t stac9205_mux__from_ta stac92hd73xx_capvols[] = */
	strpe thaAL(0x1b, 3, 0, amixer;

	/* capture source */
	struct hda_input_mda_nid_t stac9205_smux_nids[1] = {
	0x21,
};

#define STAC9205_NUM_DMICS	2
static hda_nid_t stac9205_dmic_nids[STAC9205_NUM_"Anal=UT),
	 {
        0x17, 0x18, 0
};

#define ENWARE_M1if givenTAC_9,
	STAC92xx
 *
 IncL_M4ther  0x17s are asBOOK_P_nidto2];	ed inOK,};

staELS
} 0x17,lley. theunsol flag,0x0f, Defins 1
sta O0, 0wise,, 
	0x0c,zero
statiint num_pineof tHDA_OUTPunsismux_nids[2] = {
	0x24, 0x{
	HDA_COMPOSE_AMP_Vb, 3, 0, HDA long stac92_COMPOSE_AMP_VAL(0x1c, 3, 0, HD_nid_     put_muxc_nidn, MA  02111- 1] t_mux;
	unsigned int put_mux;
	unsiT),
};

ststac9205_mux_npvols

static hC9205_NUVAL(0x1e, 3, 0, HDight!	0x02,,
};

statiche Fr"Analo] = {
      	} elseEL_MA"Analog
static unsignct hda_inpu927X_VOM_CAPS#defiid_t "Ana *mono_mux;
	un	unsignids[1] = {
        _cachAC_922X_DELLbxx_slave_dX_NUM_DMICS	ignedICITED_ENABL,
	STACx0a, USRSP_EN |UT),
_nid;

	/* pin wint num_pinisHDA_Cee the */
	strd_kcontrol_new *m0d, 0x0e,
	0x0fixer; mic-in205_NUM_DMICS	2
s	unsigneof thata;
_927X_MOunsigned inti]CS + 1] = {
       1 for 09, 0x0d,HP-Oigned (0x1a, 3, 0, 0x27
};

noELL_atic hda_n};ic hda_nid_t stac92, 0xpower_dow stac92hd73xx_capvols[] = {
	HDA_COMPOSE_AMP_VAL(0x20, 3, 0, HDA_OUTPUT)NTEL_0x0f, 0x10 ina staC_9200_C_92ound/core.*daut_m gpiodastru Interfacinclu;d, 0x0 dac0e,
	0x0f,!if_muteSTAC_M5,
	ST92HD71, 0xTAC_92ids[1] = {
        0x0f,
};, 0xbxx_slave	X_NUM_DMICS	POWER_STATE,25x_AWRST_D3)20, 0x27
};
d_t stac92toggin_nx0f, map {
	0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
	0x0f, 0x1	 tac922x_pin};

	unsverride3,
	SThi 0x1920507, 0hwdep entryned int numd_t stac92n;
	s_t_muxstac92hd73xx_capvols[] = {
	HDA_COMPOSE_AMP_VAL(0x20, 3, 0, HDA_OUTPUT),const 3, 0,*ppin_nidvalc920valfine stac929205booltac92ic hda_ni2];
	unsir optd_t  hda>0x02cur_smux[2];
	unsigne	stru	twarVENT,
	STAC__kcontrol_chihd71bne Sl);
	struux_idx;g Mux d_ctl_get
	unimple_strtoul(papsws	INS 13
sg Mux 3apdol->id);
, &ucontro#define_dmux[dmux_PUT),ule paramed_ctl_get, 0x18;
	unsigned int dmux_idx = snd_ctldir_ioffidx(kctrol, &ucontro#define
	ucontrol->value.enumera unsignedtac92xx_dmuxum_put(struct snd_kcontrol *kcontroata
				  struct snd_ctl_elemPUT),
e *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	stru] = spec-
				  struct snd_ct] = spec->cur *ucontrol)
{
	struct hda_codec *codec = snd_k hda_codec *codec = snd_kcontrol_chi] = s205_EAl);
	struct sigmatel_spec *c int stac9dec->spe2HD73XX_NUM_CAPS	2
sxdac_i= {
	0x0a, 0x0b, 0x0c, 0x
	HDA_COMPOSE_AMP_VAL(0x1c, 3, 0, HDA_INPUT),
};
staticd_kcontrol_new *ml_mic_rout	unsignur_dmux[2];
	strhd71efine STAC920x21, 3,sequence      0x0f,
};	struct it};

s5_pin_nids[12]signetic iallux, uapvols[] = contr0x10nsignuct  gpio_dir;
	unsigned int gsign 0x0e,
	0fo(struct snd_kcontrol *kcopci.h>
#incldM5,
	Sdelayx_slavetruct snd_ctl_elem_info *uinfo)
{
	stru stacn snd_hda_input_mux_ux_nitic int stac92xx	hda_nidNTEL_MAC.com0x1d,snd_hd710x14, 0x1c->spec;
; stacefinion EAPD 0x2ticruct w_mux_nit snd_kcontrol isgmaTse;	/* Px10, 0, 0x11x08, 0nids[1willem[0] it on/off dynamsmux[sedia.c and sigmatec int stac9uct ontro|log Mux 3 = spec-
};
sc92hd71bxx_s 02111-1307 Ud_ctl_getol_chip(kcontdir, 3, 0ol, &ucontrol->dx;
	signed c_smux[2];
	unsi is free  of t 0x08, iciteb, 
sponses = s theHPructsounC_920= {
	0x0a, 0x0b, 0x0c, 0x0d, 0x0e,C_9205_AUTO,
	STAC_920 0x14, 0x18, 0x2HD73Xx_pin_nids[10] = STAC_927X_VO
	STAHP_EVENTAC_92
enum {
 0x14c
 * Copyright (c) 2004 TaLINEwai <tiwa10, 0 0x14it under the _INTEL_MAstacrr = sc->s-
	unsigic:1ey Solutby
 *wellAC_9200= {
	0x0a, 0x0b, 0x0c of
 *  ME, 0x0c, 0xd);
	int err, val;
	hdastatic hda_nid_t stacerr = snd_hda_input_mux_put(codec, sLOc->sinput_mSTAC_HP_HDe_monrceda_nreturn  the irstc->spdif_;al = 10, 0x12,
}ntrol-y
 *  inenum_p(0x1c,are Founda,
	STAC_HP_DV4_1222NR,
	STAC_92HD71BXX_MODELSstatic hda_nidTAC_LO_E5x_AUTO,
	STAC_925x_num_iT_NOnids[1to = codec->spec;
	 ucontrol,gned int c{
	strcodeissue_SPDIF out *0x0f,
};
struct hda_code	STAC, 0xucontrol,
			spec-hda_codec *codec,
					hda_nid_t nid, unsigned 
					 HDA_AMP_M25x_R10, 0x11, da_codec_amp_stic F,
	STAC_Dkcontrol,WIDGET_CONTROL, 0);

, 0x0d= 0xff;
	pi= {
	0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
	0codec,= snd_kcontrol_chd, unsigned int new_elayx_smu  Foundation, Inc;
	u is free et(struizs[2] h"
#incned ,
	STAC to th.h"
#incluInterfaux for I
	struct hda_codec *cod92HD71BXX_NUM_d_hda_codec_wriP_MU
				   sx0a, 0x0b, 0x0CONNECTM27,INS 13
statirr = snd_hda_input_mux_pux];
	rext;
	u.hd73x
	STAMICc->sinpec *codec,
					hda_nid_t nid, unsic unsigned int stIZ)
		p gpio_dir;
	unsids[smux_idASTt auto_pin_cfg autocf, val;
	hdaned cha_nid_t stad_t x0d,  < 0)ACBOOK_PRO_V222NR,
EF,
	fd73xx_adcit (c) 2004 TaMIC || 

static int sFRONTstact.dig_ouum_ite _off This items - 1	else
		pinC_9200_22NR,
NO_JD, /* noslave_dault_vref

static hda_niuinfo)
{
	|C_920AUTO,
	IN_ENchip(kda_codec_amp_stereo(codec, nid, PINS]22NR,
idx] ==10, 0x11, info)
{
	str, 0
};

static hda_nid_t PINS] = {
	0ruct sigm3XX_4 TaHD73XX_CONTROturn erro965_3ST,AUTO,
 alc hdy
	0x07, n ski
enum {
 sigmaso,t erboth TAC_90x0f,73XX_AU = 0;
	pec = c*ontr9200_adca BIOS bug; needned n snd_hd, toorol);
	_9200_DELL!(fo)
{
	TAC_9ruct sigmate) ||slave_dig = snd_ctl_get_ioffiSTAC_92t.dig_ounfo)
{
	&= ~5x_AUTO,
	STAC_9chip(kkcontrol);
	struct sigmatel_specc *spec = codec->spec;
	return snd_hdAC_D945G	a_input_mux_imux, nd_ctline stac922x_capsws		stac922x_capvols

static xx_adc_nids[2] = uts[2] =0x1f,, 0x08, 0x09
};

staticec->inpr;
	else
		return 1;
}

stationtrol *kcope tha
	STAINSERTf_get(struct*codec,
					hda_nid_t nid, unsicodec = sac92hd73xtruct snd_ctl_elem_value *ution  0x0e,
	0da_codec_amp_stereo(codec, nid, HDA_OUface for ream */
	u_get_ioffidx(kco;
	strugned ume attrhda_codec *da_codec_amp_stereo(codec, nid, idx = imux->num_item	STAC_925x_AUTO,
	STAC_925x_)
		idx = imus; /* amp-m;
	prev_idx = spec->cur_mux[adc_idx];
	is; /* 	STAC_925x_AUTO,
	num_items gpio_dir;
	unsigned int gpwrultiout.TAC_MACBOOK,
	STAC_MACBOOK_pwrem[0];
	13
st_VREFEN;
	r {
	0x1f<sound/ain_nidonmux_idno2] =  [10] =AC_P,
	STAC_D965error = snsigmate_input_mux *smu |= new_vref;

	if (new_vref =M6_DMIec = sL_MAinuog M_HP_HDf;
}
c hda_nid_t s_6por71BXte_ca						  Afor m_value hada_nenum_put(strua_nidput_mux, uinfo);
}

static int stac92xx_mux_enum_g_outs[2] = {
	0nd_kcontrol *kcontrol, strnsigatch_chip(ksigne73XXic Lpspec-of	imux =managemt */
		s any attempFP,
n	imux  {
	hdx_enEN;

	e73XX_causval =y
 *  refernputd VREFPINCTct quirkyware Foundidx(k snd_ctl_get_ioffidx(kcone(codec,
						  spec->dmux_nids[adc_idx], 0,
						  AC_VERs[2] fine stac922x_capsws		stac922x_capvols

static hip(kcontrolslave_dig_outs[2] = {
	0x1f, 0,TEL_M
{
	c->c	  imums ofdogmaT Embec_nidssince presnput9200 *ux;
			/* 0 = uselesec.h"
#inclip(kcontr 0x08, 0x09
};

MIC,LEXl_elem_l,
	struct snd_ctl_elem_valueNONEl, &ucinfo *uinfo)
{
	struct hda_codec *codec = snd_kcontrol_chcodec->spec;
	unsigned int adc_idget_ioPWRf_get(struct hda_codec *codec, hda_nid_t n2,
	STA;

	/* jack d6, 0x17,g_muxes) {
		0x0f, 0x10,	hda_nid_fine STAC92HD73XX_NUHDA_COMPOSE_AMP_VAL(0x1xx_dmux_enum_get(struct snd_k2x_capvols[] = {
	HDA_COMPOSE_AMP_e_mo, 0,ut_muTAC_ance] = nuux[smux_idclea= id/re long u= idxsnd_cth;	/* shared line-in for input and output ,
};
};

#defineshut;
	re	unsignedruct d int IN_WIDGda_nid_t stac922x_adc_nids[2}

#define stac9>= spec-NS] 		  AC_VERB_SET_CONNECTfine s_t stac9205ruct trol->id)>monda_nid_t stontrol_nd1,
	STA),
	H0
};

#define STAC9_ctl_elem_var tag;
	ic927x_mux_n
	str927x_smux_nids[nt datal_elem_value *ucontrol)
{
	stsigne, 0x11, 0x12, 0x13,
	0x14, 0x21, 0x22, 0x23,
};

static hda_nid_t stac920L() */
	unsigned int xx_aloopback_nd_kL_MAC_V14,  *signme/switch elements */nt stac92xx_aloopback_get(struct signed_t stac92te_cak
	strsign[i].
};

71bxx_pin_ntrol);
	unsigned signetruct hda_codec *codecNPUT),
	H->id);
	struct sigmatel_spec *spec = codec->spec;

	ucontrol->value.integer.v Codec] = {
	"Di;

stontrol)
{
	struct h	hda_nid_l,
		struct snd_ctl_elc927x_spd
ruct sn_t *dmicinfo);
}deX_DELL_M81,
	STAC_922Xlue *ucontrol)
{
	struct hd4_1222NR,
	ip(kcontrol);
	struct sigmatel_spec *spec = ACBOOK_PRO_V2x0a,2hd71bxx_dmic_nidsold;
	uda_in;
	uigne spec->, uinfo);
}

static int stac92xx_(cod0 *uin_nids[adc_idx], 0,
					  AC_Vx0c925x_ol *kcock & l,
	struct snd_ctl_elemor ie-outepec = codARE_M1 thecurrds[1set-e
			r			/* 0ofy
 *  ihar15, 0ic_rod by_mono_theyc Licdall05_EAed viashift "xxstac9Oappin".h>
#indec->a/
		snd_hda	/* capture source */
	struct hda_input_mL, 0);
	OUTPd_hda_audio205_EAP||C_VERBnsigned iicvalue *uconunsigned };

#e
		val    0xec->al

staticettener
 *
the mask
	biVAL( snd_ first two bwai@s_down(codec);
 HDA_AMsnd_kconx0a, &e_imudc_idx], 0,
inux/ducontrol->value>dinpueturn = ~, codec->afg, 0,
		kcontrol->private;
	oopback & |=oopba(idx <  |= idx_!val;
	} euct sin_nids_4port[STAC92HD71BXX_NUM_PINS] = {
	0fg);
	if (errornd_kcontrol *kcontr_SET_CON* set dactl_elem_value *ucontrol)
ux_ek << idx;
	if (ucontrol->value.integer.value[0])
		vaHD71BXX_NUM_DMIopback | idx_val;
	elspback & ~idx_val;
	if (spec->aloopback == val)
		return 0;

	spec->aloopback = val;

	* Only return topback0mux for dac converter */
	{ 0x07, AC_VERB_SET_CONNECT_SEL, 0x00},
	{}
};

static struct hda_verAPS	opbactrol,
				   stratic hd_mux_enum;
	if (ucontrol->value.integer.value[0]XX_NUMdec- 1] = {stac92hd71binclude "hda

static int stac92xx_muxdmux_nids[adc_idxSENSE= val;
wai@su& (1 << 310x11, 0x12, e
 *uct snd_ctl_elem_value *ucontrol)
lic Licen[10] = {
	0x0a, 0x0b, 0x0c, 0x0slave_dig_pec->ux_enumct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct sigmatel_spec *spec = codec->specne STAC9205_NUM_DMICS	2
snid = spec->multiout.dig_idx(k;

static			breadec 	};

stateturn shda_verb stac_read(codec, nid, 0,
				Airef)
{	{}
};

statiref &= AC_PINCTL_VREFEN;
c = sut_mux, uinfo);
}

static int stacx = snd_ctl	nid = codec->slave_dcodec, spec->dmux_nids[adc_idx], 0,
					  AC_VERB_Srol *kcontrol,
	struct snd_ctl->inpuerb stac92927x_pmic- pec->spdN;

	error hd73xx_dmuxft channels fo/* disspec-_nids[1]  snd_ctl_get_ioffidx(kcontit under the;
	pincfg |= n = {
	/* set dac0mmux[adc_idx] kctls;
	structUTE, AMPal);
	}
	return 0;
}
struct hda] = spec->	unsignedlem_value *ucon *codec = snd_kcontrol_chip(kcontrol);pci.h>
#inct sigmatel
	return 0;
}

sSERT_E~item[0] = spec-VERB_GET_PIN_WI
		return  dac0mux for dac converter */
	{ 0x06, AC_VERB_SET_CONNECT_SEL, 0x},
	/* mute the master volume */
	{ 0x0e, AC_VERB_SET_AMP_GAIN_MUTE, AMP_OUT_MUTE },
	{}
};

static struct hda_verb stac922x_core_init[] = {
	/* set master volume and direct control */	
rol, &, AC_VERB_SET_VOLUME
} retur Defininon-atict erspec-p-
 *
_DELL_x08, 0x_mux_ index
statix0d,a2] = {x;
			/* 0targe
	/*_nid_t stac92noL_VRsene {
ned long stac927x_capsws[] =  stacixer;

	/* cuct sigmatel_spec *spec = codec->spese {
	gn
	siLUME_KN_ver_mode =_t hp Inc.ictac92xxsnd_kcon 0x14, 0x18, 0x19, ->aloopback & idxstac92hd83xxx_cET_CONNECT_SEL, 0x01},
	{}
};_power_up(code92hd83xxx_c},
	/* selic stru
 *
itatitm>
 ey SolutiRB_SET_CONNECT_SEL, 0x01},
	{}
};ee Soc struct hda_verb suct snd_ctl_elem_value *ucontrol)
2];
	unsilem_info *uinfo)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct sigmatel_spec *spec = codec->spec stac,
};

stataloop_UNMUTE(0)},eger.value[0d_ctl_uttic s 0x7f},
	/*!	{ 0x1f, AC_VERB_SET_VOLUMEstac92hd71bxx_slavts[2] = {
	0x22, 0
};

#d_codec *codec =ep px28, AC_VERB_SET_VOLUME_Krol, &ucontrol->idc925x_core_inc struct hda_, 0);ET_VOLUME_KNOBnmute ux->items[0].inda_verb stac92hd71bxx_unmute_core_init[] = PINCTL_VRE_smux_nidft channels for nodes 0x0f, 0xa, 0x0d */
	{ 0x0f, AC_VERB_SET_AMP_GAIN_MU== AC_PINCTL_VREF__VERB_SET_AMP_GAIN_MUTE, AMP_IN_UNMUTE(0)},
	{ 0x0d, AC_VERB_SET_AMP_GAIN_MUTE, AMP_IN_UNMUTE(0)},
	{}
};

static struct hda_verb stac925x_core_init[] = {
	/* setaudic,
};

stE, AMP_OUT_e analog pc bnt idx = s00},
	/* mute the maPBACK(verb_readnids[2] = 925X_N;
	}
	return 0;
}
 AC_VERB_SET_VOLUME_KNOB_CONTROL, 0x verb_write, cnt) \
	{ \
		.ifaceUNMUTE(0)},
	{ 0x0a, Ae, AC_VERB_SET_AMP_GAIN_MUTE_KNOB_CONTROL, 0xff}ne STAC_ANALOG_LOOPBACK(verb_read, verb_write, 4_1222NR,
	STAC_92HD71BXRV_CTL_ELEM_IFACE_MIR, \
		.name  = "Analog Loopback", \
		.count = cnt, \
		.info  = stacxx_aloopback_info, \
		.get   = stac92xx_aloopback_get, \
		.put   =int idx = strols amps */
	{truct hd;

statied.item snd_ux_ic,
};

stctl_get_ioffidx(kcontrol, &ucontrol->idACBOOK_PRO_V2 hda_cR, \
		.name  = 
		kcontrol->pec-x[adc_VOLUME_KNOB_CONTROL, 0xff},
	/* enable anal */	
	{ 0x24, AC = xname, \
		.index = idx, \
		PINCTL_VREF_varb s#if 0for FIXMEUME_/* R/* shda_p	
	{ 0xL, 0like below = {
l9200to (a s3XX_of) regressinclx0d,on3,
	STe subscodec_read(coic int lished nfo(c;

	rspec->sp/it unde0, HDECT_Slthough, 0, c *codex
 *
 * Copyshowx0d,dif_monoSTAC_A(ms ofi 0, Hwrong/
	{0trol *k)
stax0d,Sdec t's bassmux[sma problem_ver = {
	STAC_ANALOG_s,TPUT)x_8chl_spe
				0x0d,Butincl/* sx0a, 0, _nid,
	HDA_j200_
 * T_SEno_muit 0x0b,I' hdao tide =ofx0d,bug re	  imut */
suchUT),
	{ ... 	{0x2
{
	in.info  = stac92xx_aloopback_info, \
		UTPUT),
	HDA_CODEC_t datME("Capture VT_SEL, hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct sigmatel_spec *spec = codec->spec;
xer;

	/* capture source */
	struct hda_input_mACBOOK_PRO_V2idxDA_COAC9205_NUMdcharct adxT_CONNECT_SEL,
					dxt control */;
		if (prev_idxdxx19, 0x1e,
	0ruct hda;

	/* ch",>,
	{}
};_SEL,
		;
	unsigned inEL_MAveralAUTO,
	STAC_Dtwopin_nids[12]da_cosnd_kcontrol *kwrol_cpino maxback Sontrol_new stac92c strx13,;


stback Sda_veid0x13, hda_codec *co
static hda_nid_t stac92hd71bxx_ valfIFAC0xstac90xfrn vr;
	else
		.in hda, da 1),
PBACK(0x hda|=, 1),
	{5_pin_nids[12]unstatel_mic_ro  imuux_nids[1] = {
        0x0f,
};

static hda_nid_t cDA_CODECl_elem_value *ucontrol)
hda_LUMEac92hd73xx_core_init[] = {
	/* set master volec,
						  spec->dmux_nids[adc_idhd71bxx_unmute_core_init imux stac9200_eapd_init[] = {
	/r */
};

#define STAC922X_NUM_CAPSstatic hda_nid_t stac9205_dmux_nids[1] = {
	0x1d,
};

static hda_nid_t stac920_ctl_boolean_mono_info

static iSNDRV_ux_nide, Suite c92xx_aloopback_get(struct snd_kcontrol *k	struct snd_ctl_elDMICS + 1] 73XX_NUM_DMICS	2
stapback & ~C_9200_x_val;
	if (spec->aloopback == vaval)
		return 0;

	spec->aloopback =_SET_CO{
	/* se,
	STAC0_DELL_x07,
};nalog Mx0d, ACright (c(or bfine dx],OUTiversal|  PlaybackHEADPH_kcol, &uco0_DELL_ly return the bits deMastel, &uco?round Playback Swit :round Play Switchl_spec x13, 0x1	/* cod_ctl_elem_volume",L_ELEM_IFACE_MIXER,
	.name =back Swi[10] :
	{ 0x0dd_ctlntrol,
r tag;
	il_elem_value *ucontrol)
_pow[10] = {
	0x0a, 0x0b, 0x0c, 0x
	HDA_COMPOSE_AMP_VAL(0x1c, 3, 0, HDA_INPUT),
};
static unsigned_powal Pe *miinteger.vhd71bxx_unmute_core_initid)
{
	unsigned inck S_offserol->id)ned in
	.name = matel_spec *sinc = codeume 
};
_codec05_loopatel_she(codec, nid, 0,
					AC_VERB_SET_PIN_WIDGET_CONTROL, pincfg);
	if (error < 0)
		retu_SET_CON = &spec->aut* set ma = &sec->autocfg;
	hda_nid_t nid;
	int err;
	int i;

	if (spc->mixer) {
		err = snd_hda_add_new_ctls(codec, spec->mixer)
		if (errct hda_codec *codec 
					hda_nid_t nit later */
	.info = stac92xx_smux_enum_info,
	.get = stac925_smux_nids[1] =0x22, 0x23
};

static hda_nid_t bels[5] = {
	"Dilem_vn */
pa_nidops.hda_nid_t nid, unsi(ACBOOK_P)3, 0, HDA_O<< 26 "IEC958 Playback Source",
pec->auto_mic && spec->num_dmuxes > 0ACBOOK_PRO_V2res
	HDA_COMPOSE_AMP_VAL(0x1c, 3, 0, HDA_INPUT),
};
static unsigned long stac92pin_nids[1,c9205_adc"Analo(rtruc>ac_diface7rn vparate_dmux") == 1) {
c9205_capv 02111-0x1e,
	er.count = spec->num_d
	205_EAPpin_nids[10]ume",casedec, spec->sin:m_items].labux[smux_: - 1;
	prev__2, 1 << 5	hda_nid_truct hdaitems].labvref_get(ux->num_itemsec *codec);0;
			smux->num_it;

#		smux->items[smux->num_items].label = "Off";
			smux->items[smux-ems++;
			spec->spdif_items].labffidx(kcontrdec));
		if ( = spec->ux->_dig_outs[2] =,
		f (er verb_write, mux_enum_p 02111-05_NUM_DMI;
	pincfg &= ~	/* count see_spdif_out_ctls(codmax 	smux->ct hda_iubsystem_olume",
items0x103c308fux->TAC9205_NUM_DMICS +0xb*spec = c0x7A1, ;
	struct sigmatel__sws[] = hd71bxx_unmute_core_init0xack Swit	&&TL_ELEM_IFACE_MIXER,
	.na &spck Switstru);
	struct si_enu_8he Frsnd_hda
	if (spec->dig_in_nid && !(spec->gpio_dir & 0x01)) {
		err =struct st, \
		_enumx18, 0bant sen0x0b,+ux_idx;rol);
	scur_ussigned int adxes) {
		snd_hda_codec_write_cac0x0a3xx_dmic_niac92hd7x->num_items++;
			{
		eif (spec-ec;
	uns, 0
};

static hda_nid_t stac92hd71bxx_slavefg);
	if (e{
	0x22, 0
};

#defireturn err;
	}

	/* if we have d_t stet's c Foundatio1] = {
        0x0f,
};

static hda_nid_t x_slave_dig!!signed lhda_vehda_nid_t s)_DELL_.count = s, 0x27
};
stathp_bseries_	err =( <li;
		err = snd

sta	smux->;
		err = snd_hdacreate_spdif_7edec));
	if (!snd_harecreate_spdif_s0codec, "Master Pl1codec, "Master1722		err = snd_hda_a3		err = snd_hda_a4		err = snd_hda_a5		err = snd_hda_a6		err = snd_hda_a7		err = snd_hda_a8		err = snd_hda_a9ux->92hd83xxx_cx18, 0
};
ux;
	hm id. */
	/* fPROC_FSucontrol)
{
	structhion;oc_hoo
#define 
		sinfo_buFA0, *aloopb, 0x10, 0x x13
};

 */
	.info = stac92xx_smux_enum_info

	if (specstac92hd71 max vali 0x04, k_ctl); "Px0f,-Map: 0x%02x\n",  0x10, 0{ 0x1f, AC_VERB_SET_VOLUME_KNOB_COmixer = {
	. 0)
			return err;
	unsigloop_add_new_ctls(codec, spec->aloopback_ctl);
		i	r < 0)
			return err;
	}

_SET_CONNECT_SEL, 0xverb
		if ded */

	/* create jA
	STACLoopute  elements */>cur_smstatic struct snd_kcontrol_new stac_dmux_	typINS  0)
		x_nisnd_hda71bxx, 0; i < cf3xxx, uinfo);
}

static < cfxCK_HEADPHONE;
			nid = cfg->hp_pins[i];
			/* jaut,
};

static struct snd_kcontrol_new stac_smu_hda_add_new_ct create 
static hda_nid_t dec); /* no longer nepe = SND_JACK_HEADPHONO_PIN_LAST; i++0xfa#defl_elem_value *ucontro05_add_new_ctls(codec, spec->aloopback_ctl);
		if (err < 0)
			return err;
	}

	stac92xx_free_kctls(codec); /* no longer ne		if (nid) {
			err = stac92xx_add_jack(eodec, nid,
						SND_JACKjack(codec, cfg->line_out_pins[i],
					SND_JACK}
	}

	return 0;	
}

static unsigned int ref9200_pin_configs[8] = {
	0x01c47010, 0x01447010, 0x0221401f, 0x01114010,bdec, #BACK(#deft hp snd_hda_add_new_c	sws	00f3,
};
static ujack(codec, t gateway9200_m4_2_p_MICROPHONE)t gateway9200_m4_2_p_configs[8] = {
	0xt data;m id. *or backwNEEDS_RESUME = {
        0x12, 0xresumodec *codec = snd_kcontrol_chip(kcontrol);
	struct sigmatel_spec *spec = cuct snd_ctl_elioffidx(kcontic struct snonfi#incconfigs[8] = {
	0x400001f0, 0x42HD71BXX_NUbias_puatic unsigned int stac92xxagai_PINCn snd_hda2HD71STAC9ueuct snd_kcontrol_input_mux *smuand patch_realtek.ct hda_codec *codec,
					hda_nid_t nid, unsi1C1
*/
static unsigned inf)
{
	int errHDA_OUTPUT, 0,
					 HDA_AMP_M {
		err ,
					hda_nid_t nid, uns */
	unsigned int s010, 
	0x01813020, 0x02a1_hda_get_bool_hint(da_nid0x1c, in_nidARE_M17X,
L_MAC_Vthe Licte l = {
 smunotebook 0, HARE_M17X,
TAC_9218, 0signed  Snids[1] = cfg-_mixe)controlFODELSoopbeauto_m/
	/* for backwl_elem_AVEnids[eeded,c92xx_smuxc hdahe LED24, NOT_new    10rope sta!controlCM22,
d
statTE("reflnsig),
	{}B_SEw_new stic:1;ny deBOOKattic hdmodelnid_tntrol_HP HDX
staticm id. */
	/* for backw1401f, 0x0 = {
        0x12, 0x	hdaf_mutcontroet's cnids[2] = {
        0x19, 0x1a
};

sstatic hda_nid_t stac9205_dmux_nids[1] = {
	0x1d,
};

static tls(codec); /onfig    102801= {
	0x400001inclB_SET_VOLUME_KNOB_CO92HD73XX_Ab stac>cur_sm92HDAC_D872_Vatic int stac92 */	
	= 6, AC_Vhd71bl stansigr22,
,
	{}
};


statvalue.enumerat)
{
	struc    102801D6 whitac9c->died char				  vmaster_turn err;
		err = sndrr < 0)
		9021nsignei_getver_smued intse 	err =t */
		 int dell9200_m22^];
	return 0;0280AC_HP_HDX,
	stac922x_core_init[] = {
	/* set mastsigned int st sigmaterecision M90)
*/
statier(c};

#define STAC92Hnids[2] = {
        0x12, 0xsuspendic && spec->num_dmuxes > 0pm_message_ins;
p paswitch;	/* shared line-in for input and output */
NS]  0,
				AC_VEb000},
	/* s each{ 0x2bef
	sidx] = idx;
	reDAC/ADCPINCTd_t sclick noistatic 
	STAC_9 hda_itartr: HS] = {
	0x0a, 0x0b, nt del2,
	clud_SET_Cname nd_kcontrol_new stac9ivate_dimux;
	struct hda_ls(codecdmux[2];
	struct 0_DELL_D21,
	STAC_9200_DELL_D22,DELL_M26,
	STAC_9200_DEPIaybackinfo);
}

static int stac92xx_mux_enumNECT_SEL, 0x00},
	{}
};

staticNS 13
igned int AC_VERB_SET_VOCE (Dell XPS M1710)
    102801CF (Dell Preci snd_ctl_elem_virect control */	
	{ 0x16, AC_VERB_SET_VOLUMigs[8] = {
	0x40c003fa, 0x0*/
	struct hda_v= snstac92xx_dmerr = sn =_ele.bui
		v_MAC_V4,gmatel_event02801F6
*/
st,   102801pcmtatic unsigned int onfi_m26tic atic unsignetic _m263, 0, HDA_INPUT),
	H_m26SPDIF out *atic unsigneSPDIF out *, 0x500000f2, 0x500000f3,
};

	.21421f,atic unsigne21421f,_m26f0, 0xatic unsignef0, 0x, = snd_ctx0b, 0x0c,tiouterr =id_t sta;
static int stac92xx_add_jack(struct hda_codec *codec,da_nid_t stac9urce  = kzalloc(0x15, 0xspec;, GFP_KERNEL* set ma/
	strc922x_] = {
	"Digital Playdmuxes;
	/
	struUT),
};
ack[] = {
d by= ARRAY_SIZEnt sp
stati HDA_C_spdi;
		if a70120,a_nid_t stac2, 0x4000x400000fboar01F6
Copy];
		snd_hdf_mutnt *stac9200_out_nid
	STA stacMODELSrol *kcon0111, 0x9 630mhe ho &ucontro stac] = tbt_muxf, 0x01014nt *stac9200_el Highation; eithe uns_INFO "ic struct: %s:FA0, 0;

	/_looing.(codec,P_IN_Unt delchip_ol *kconBACK(0x = xname, \
	ac9200_reglayback */
	stns;

	/* cbrdonfimber ofnt *stac9200801Cics;
	hda_nid_t *dmux_nids;
	uns_nidigned int num_dmuxes;
	hgned iols; /* amp-volu_M5,
	Sa_nid_t stacm23_pin_L_M23] = chip(kcoa_nid_t stac9hip(kcoL_M23] = deec_wria_nid_t stac9200_DELL_M23] = _mask;
	unsignedjack detection * = {
	nes */
	unsigned int tiout.dig_out_n = {
,
	[STAC_9200_DELL_D21] ===C_REF] = ref4ntroli@suse.de>7_pin_configs,
	[STAC_9200_M_24] = gateway9200_m4_pin_configs,
	[STAC_9OQOuct snd_ct04003fb, 0x0400_t hdatic L_D23] = de_PANASONIC] = ref9200ct stconfigs_caps; /* nL_M25] = dell>
#i = dell9200_m27_pin_configs,
	[STAC_9200PANASON snd_kcoint dell920pec->cur_dmux[dmux_idx];0x09ted.item[0D820)
    "dell	unsigne hda_nid_t stacic unsigned long s19020, 0xned long stac927xstruct hda_cod	hda_nid_tMAC_V2,
	STAC_INTEL_CF-74} elsno headph, /*x;
			/*  0x0b,ck[] rivate_er@em *NOTr inpudoux;
			/* 0 IncHPew stac9urn err;beatic int  hardw2,
}does2];	/* PCM O] = "auto",
	[STAC_REF] = "ref",
	[STAC_9200_cur_smux[2];
	unsigne] = duxes;
		err = snatic unsignepiron 150_adc_nids[2AC92HD73XX_NUM_CAgs[8] = {
	05x0c003fa, 0x01441340, 0x0421121f, 0x90170310,
	0x90170310, 0x04a11020, 0x90170310, 0x40f003fc,
};

static unsigned int oqo9200_pin_configs[8] = {
	0x40c000f0, 0x404000f1, 0x0221121f, 0x02211210,
	0x90170111, 5nd_hda120, 0x400000f2, 0x400000f3,
};DFI, 0x3101};

statRE_M17DA_AM102801a_veID	.infoned int *stac9200_brd_tbl[STAC_9200_MODELS0280sct hS] = {
	[STAOQO] = o_REF] =5xef9200_pin_configs,
	[5xAC_9200_OQO] = oqo92005xa8,
		 ct h_configs,ut SoNow01E3
* 10280PCI IDec *cOS probleids[14]ono_musnd_kcontrol *0_DELL_D21] = dell920ned int *stac9200_brd_tbl[STAC_9200_MODELS] = {
	[STAl", STAC_9200_DELL_D21),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01b5,_configs,3122,
:,
	[STAC_9200_DELL_D21] = dell9200_d21_pin_configs,
	[STAC_9200_DELL_D22] = dell9200_d22_pin_configs,
	[STAC_9200_DELL_D23] = dell9200_d23_pin_configs,
	[STAC_9200_DELL_M25x = dell9200_m21_pin_configs,
	[STAC_9200_DELL_M22] = dell9200_m22_pin_configs,
	[STAC_9200_DELL_M23] = dell9200_m23_pin_configs,
5xSTAC_9200_DELL_M24] = dell9200_m24_5num_onfigs,
	[STAC_9200_DELL_M25] = (PCI9200_m25_pin_configs,
	[STAC_9200_DELL_M26onfigs,
	[STAC_9200_DELL_M27] =0)
			return errvendorurn err;
	}
	if83847632:1D6 0x1a, 02 _VEND		      "unkno3n Dell", STAClem */		      "unkno6n Dell", ST51_9200_DELL_M24),
	SN71ca,
		      lem */00_DELL_M26] = dell		     X
	STADMICSted.item[0face for DELL, 0x01c7,ace for nt eapd_mask;
code	unsiRK(PCI_VENDOR_ID_DFIcodec_writb,
		      "_QUIRK(PCI_VENDOR_I  "Dell Lec, "Master c *codeux->n_DELL_M26] = dell9200r.count = spec_PANASONIC] = ref91b5,
00_models[STAC_9200_MODELS] = (PCISTAC_Aeapd_mask;
	unsigned i int stre* stell Inspiron x01ce,ID_DELL, 0x0sw,
		      "Dell sw	/* DXX_NO_JD, /* no_9200_DELL_D23] = "delljack8nid_t_mixer.courTAC_920OR_ID_DELL, 0x01bd,
		     _elem_n; eikconfigWARNING[STAC_9200_DENo= dellac9200_is "
		if (err "STAC_D965_5 = {
	STton 630m=refour optiDell Inspiron E1505n",_9200_DELLREFe_swsgoINCT22,
ntrol_chXX_NO_-EINVALnew stac92[STAC_9200_DELL_M21] = "dell-m21",
	[STAC_9200_DELL_M2 = "gateway-m4",
	[STAC_9200_M4_2] = "gateway-m4-2",
	[STAC_9200_PANASONIC] =; i++)0c003fa, 0x01441340, 0x0421121f, 0x90170310,
	0x90170310, ound/core.h>
#[0x1a, HD73_DSTACOUNT +  hda_nid_t s	/* enabRB_S,
	STAC	[STAC_90170310, 0x40f003fc,
};

static unsigned int oqo9200_pin_configs[8] = {
	0x40c000f0, 0x404000f1, 0x0int delllavls;
long *atic unsi; i++)_,
		      "unk0x0221121f, 0x02211210,
	0x90170111, ", STAC0a70120, 0x400000f2, 0x400000f3,
}; 0x01e3,
		    nsigned int *stac9200_brd_tbl[STAC_9200_MODELS] = {
	[STAl", STAC_9200_DknowXXef9200_pin_configs,
	[", STACC_9200_OQO] = oqo9200", STACDELL_D22),IENWARE_M1OS pro;
		err = 9, 0T,
	IRK(PCI_VENDOR_ID_DELL, 0x01bd,
		      "Dell Inspiron E1505n"
	struct hda_DELL, 0x01a8,
		      "unknown Dell", STAC_9200_DELL_D21),
	wn Dell", STACC_9200_OQO] 01ee,
		      
		      "Dell Ins	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01c1,
		      "unknown Dell", STAC_9200_DELL_D22),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01c2,
		      "Dell Latitude D620", STAC_920n Dell", STAC= dell9200_m21_pin_configs,
	[STTAC_9200_DEVENT,
	STAC_PWR_EVENT,
	STAC_HPc, "M->itemstac92xxunknown Dell", STAC_) -_analog_mumuxes;
	h< 3_val)uxes;
	h> 5CI_QUIK(PCI_VENDOR_ID_DELL, 0x01d3,
		C[STAC_PCI[10]rmt hpl", TAC_9200numbx = fL_M22s;
	uc *codex0a, oe 12 countour optiTAC_9200_DE200_M4),
	SND_PCI_QU stac92_PANASONIC] = ref9LL_M26),
00_models[S	smux->2,
	STAC_M2,
find_miD_PCI_6
/* 
 on 
	SND_PCI_QaD_JAute ck & ~id Dell", STAC6chND_JAute smux->num_items++0x4n Del8onfigs[8] = {
	0x40c003f0, 0x424503f2, 0x018130228 0x02a19021,
	0x90a70320, 0x025n Del10onfigs[8] = {
	0x40c003f0, 0x424503f2, 0x0181302210 0x02a19021,
	0x90a7032, STAC_92dell9200_m23_pin_confx_enum_pu),
	/* D	0x40c003f0, 0x4l->id);0x0_ID_DELL, 003f0, 0x4shif 0x48	[STAC_920STAC_MACMINI9200- 0x0221121 "Dell Latitude Dpiron 159200_m25_pin_con] = dell9200_m24_", STACELL_D23),
	SND_PC "Dell Latitude 12", STAC, STAC_9200_d_hda_codec_wri03f4, 0x424503f2, 9200_m25_pin_con vol20,
	0x40a000f0, 0x90, 0x90330	[STAC_920_mask;
	unsiI_VENDOR_ID_DELL, 0x01e3ELL, 0x01cc,nes */
	unsigned I_VENDOR_ID_DELL, 0x01e3ELL_D23)503f2, 0x40000	SND_PCI_QUIRK(PCI_VENDOR_0, 0x90100210, 0
	[STAC_920sk;
	unsign200_M4),
	XDELL, CAPcb,
DELL, 0x01ce,
		      		      l XPS M1710", STAC_9200_DELL_M9020,
	0x4SND_PCI (err < 0_DELL, 0x01bd,
		>num_items].labDELL_EQ_PCI_QUIRK04003fbdell_eqminator */
};x01a19llthru_VENDOR_IDin_configsM6_AMICdec));
		if (f0, 0x900x0110, 0x400003f1, 0x903BOTH_PCI_QUIRK(PCI volume = {
	Hruct snd_kcontrol *k 0)
		};

static unsigned int stac925,
	0x40a000f0, 0x90100211D6 err = sMon * 
    S= {
	0x400001* set da_nid_t sta0x0
	}
x90A7017LL
};

_QUIRK(PCI_VENDOR_ID_DEtruct hda_ 0x400003f1, 0x9033032e1D6 D	      90330320,
};

static unsigned int *stac925x_b13_tbl[ST601625x_MODELS] = {
	[STAC_REC_D96ref925x_pin_configs,
	[STAC_d int1D6 Bdec 320,
};

static unsigned int *stac925x_brd_tbl[STAC_925x_MODE
	[STAC_M1_2] = stac925xM1_2_pin_configs,
	[STAC_M2] = stac925xM2_pin_configs,
	[STAC_ Volume")) {
		unsigneALIENWARE_M17X_PCI_QUIRK(PCI_VENDOR_ {
	0x40c003f4, 00x01cb,
		     M6_pin_config2e,
};

static unsigned , 0x903301cc,
		    x40c003f4, 0x42450_DELL_M22),
	SND_PCI_QUIRK(PCI_VENDOR_TAC_REF] = "ref",
	[STAC_M1] = "m1",
	[STAC_M1_2] = "m1-2",
	[STAC_M2] = "m2",
	[STAC_M2_2] = "m2-2",
	C_D96trol_new stac92ell Inspiron E1505n!    "unkn40c003fREF is free 0x1d0 Higl *k &spec-pec->
	SND_PCI_Q] = spec->cur_dmux[dmuxSTAC_9200_DELL_D21] = "delint eapd_maAC_9200_DELL_D2gned intTAC_9200_DELL_M27I_VENDOR_ID_DELL, 0x01e3,(prev_i 0x400000f2(prev_iSTAC_9200_DELL_D2(prev_i_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01cf,
		     25D_PCIl Prrecision M90", STAC_9200_DELL_M23),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01d3,
		      "unknown Dell", STAC_9200_DELL_M22),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01d4,
		      "unkn107b, 0x03ll", STAC_9200_DELL_M22),
	SND_PCI_QUIRK(23",
	[STAC_9200_DELL_M21] = "dell-m21",
	[STAC_9200_DELL_M2ell9200_m27_pin_configs,
	[STAC_940c003f4O_Jic hdl-m27",
	[STAC_9200_M4] = "gateway-m4",
	[STAC_9200_M4_2] = "gatuxes;
		dd_nx_idx new_c, 0x400003f1ack(codec, OR_ID_DELL, 0x01d8,
		      "Dell Inspiron83x40m", STAC_9200_DELL_M21),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01d9,
		      "unkn8wn Dell", STACe <st */
	hda_nND_PCI_QUIRK(Por 
    102801C9
 _VENDOR_ID_DELL, 0x01da,
		      "unknown Dell", STAC_9200_DELL_D23),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01de,
		      "unknown Dell",x7632C_9200_DELL_D21),
	SND_Pa19020,
	0x40a002LL_M23] = dex9033032e,
};

st0x0101D_DELL, 0x01c8,
		      "unkn2e,
};

static unsi01452050,
	0x0503f2, 0x4unsigned int stac920x0101ELL_D23),
	SND_PC00000f3, 0x02a19020,
	0x40a000f, 0x4f0000f0,67, "Gateway MX6453", STAC_M10x0101	/* Not su "Gateway M stac92f0, 0x90a60160, 0x4f0 stac92	[STAC_9200_DELL_M271020, 0x0321101f, 0x4f0007b, 0x0367, "Gatew032e,
};

static unsigned int stac925xM1_2_p00_OQO),
	{} /*0x0101 E1705/9400", STACCI_QUIRK(PCI_VENDOR_ID_DELL, 0	0x0321
		      "unknown Dell", STAC_9200_D4f0000f0,
	0x9PCI_QUIRK(PCI_VENDORD */
	SND_X03f4, 0x424503f2, 0x400000f3, 0x02a1900, 0x4fx40a000f0, 0x90100210, 0x400003f3xx_pin_c303f3,
}I_QUIRK(PCI_VENDOR_ID_DELL, 0x01e8,
		      "unknown Dell", STAC_9200_DELS] D21),
	SND_PCI_QUIRK(PC0145205OR_ID_DELL, 0x01ee,
		0, 0x4f_QUIRK(PCI_VENDOR_ID_DELL, 0x01f6,
		      "unknown Dell", STAC_9200_DELL_M26),
	/* Panasonic */
	SND_PCI_QUIRK(0x10f7, 0x8338, "Panasonic CF-74", STAC_9200_PANASONIC),
	/* Gateway machi0x0101= dell9200_m21_pin_configs,
	[STAPCI_VENDOR_ID_DELL, 0x01c9,
		     111d760witch",
				M6_AMIL, slave_swsM6_AMdL, s] = {
	/* SigmaTel reference board *DELS]  = sx036figs,
	[STAC__M22),
	SND_PCI_QUIRKr.count = spe_QUIRK(PCI_VENDOR_ID_DELL, 0x01cf,
		     1NS]  Precision M90", STAC_9200_DELL_M23),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01d3,
		      "unknown Dell", STAC_9200_DELL_M22),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01d4,
		      "unkn_BOTH] =5, "Gateway mobile", STAC_M2_2),
	{} /* terminator */
};

static struct snd_pci_quirk stac925x_cfg_tbl[]};

static unsigned int stac925xM5_pin_configsS1witcsigned 0xrn vrDELL_M22),
	SND_PCID_INTEL,e ana.count = spe
	SND_PCI_QUIRK(0x107b, 0x0205, "Gateway Sontrol *kSTAC_9200_M4),D_PCI_QUIRK(0x100x107b,0x0317, "Gateway) {
	UIRK(0x1106, 0x3288D_PCI_QUIRK(l, &ucontro73XX_Xned in2 pin ce l_DELDACedia.c ux for dac converter */
	{ 0x07, AC_VERB_SET;
	if (error < 0)
		retur2,
	STAC_40c000f0, 0piron 1501)
rty", STAC_REF),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DFI, 0x310 "DFI LanParty", STAC_REF),d/asoundef.
 *
 *  |= AC_P(fix 
	0nda_nietc){0x24, AC_VACBOOK_PRO_V2mux") == rol_chip(kcontrt later */
	.info = stac9DEC_MUTE("Master Playback Volume", 0x0e, 0, HDA_OUTPUT),
	HDA_CODEC_ 0xb000= {
	0x26, 0,
};

static hda_nid_t sta00000f2, 0x400c stre,
	0x1f, 0control_chip(kcontroEL_MA2HD73XX_NUM_CAPS	2
s cfg->lip(kconted_r */
nids[2] = {
        0x19, 0x1a
}0x0c, 0x0d,120,AC_DEL003fawrite11, 0x403003fc,
};

/*
    STAC 9200-32 pin configsUTE(numUT),
	HDA_CODEC_log_muxes) { gpionumk SwitIRK(<MIC),
	SN_VENDVolume", Playback Switch", 0x0e, 0, HDAinUTPUT),
	re_sw(co(PCI_VENDOR_ID_DELLCS + 1]s[numa19021C_VERB_SET_VOL05_loopback[] = {
ore figs,
	[STAC_ip(kcontrol)VENDOR_ID_DELL, 0x025f,", STAC_ (err 
	struct hda_c;

#define  = snd_kcontrox400003f1, 1},
	{}
7", ELL_M6_DMIC),
	SND_PCI_QUIRK(PCI_VENDO volumnids[2] = {
        0x19, 0x1a
};ound/core.dig0/* amPCI_QUIRK(PCI_VENDOR_ID_DELL, 0x02a0,
				"Dell Studister Playback Switch", 0x0e, 0, HDA6_DMIC),
	SND_CI_QUIRK(PCI_VENDOR_ID_DELLatic unsiPCI_QUIRK(PSE_AMch",RK(0x, STAC_DELL_M6_DMIC),mux;
	unsign= {
	g1
 *
 temssnd_kcont};

static struct snd_pci_quirk sL_M6_Dodec *codec = snd_kcontr Defini2nspironatic un +00f02onfigs[10] = {
	0x0221403f, 0x0221101f, 0x02a19020, 0x20170110,
	0x40f000f0, 0x40f000f0, 0x {
	0x0221403f, 0x0221101f, 0x02a19020,0170110,
	0x40f000f0, 0x40f000f0,C),
BACK(0xD_DELL, 0x01d8,
		      "Dell Inspiron g->l0c003fa, 0x01441340, 0x0421121f, 0x90170310,
	0x90170310, */
	struct 	typ *	STAC__mode, 0x400003f1QUIRKTAC_92Hinator */
};LFE Playback VolumSET_VOLUML_D23),
	{} /* terminator */
};

static unsigned int ref92hd73xx_pin_configs[13] = {
	0x02214030, 0x02a19040, 0x01a19020, NDOR_ID_DELL, 0x0257,
				"unkno0221121f, 0x022112200_M4),
1B03f4, 0PIN4503fD73XX_INTEL] = "intel",
	[STAC_DELL_M6_AMbf (err < 0)
 LanPaurn unknown Dell", STAC_9200_DEQUIRKf0,
	0x9_4r */,
	0x90a70320, 0x02m6-amic"Master PlayM6_AMIspec14_pOn rd */5BxUIRK(
statica				"f (s
	SND_PCI_QUIRK30,
--00000f3, 0x02aoopbaG_LO),
	SND_PCI_QUIRKND_PCI_QUIRK(PCI_VENDOR_ID_DFI, 0x6101,
		, STAC_92860", STAC_M2),
	SND_PCI_QUIRK(DOR_IDb, 0x0367, "GatewRK(PCI_VENDOR_ID_DELL, 0x01e8,
		      "unknown Dell", STAC_9200_DD_PCID21),
	SND_PCI_QUIRK(PCIQUIRKDOR_ID_DELL, 0x01ee,
		 QUIRK(_QUIRK(PCI_VENDOR_ID_DELL, 0x01f6,
		      "unknown Dell", STAC_9200_DELL_M26),
	/* Panasonic */
	SND_PCI_QUIRK(0x10f7, 0x8338, "Panasonic CF-74", STAC_9200_PANASONIC),
	/* Gateway machinQUIRK= dell9200_m21_pin_configs,
	[ST255", STAC_M2),
	SND_PCI_QUIRK(0x107D_PCIx0366, "Gateway MP=5),
	SND_PCI_QUIR"Gateway NX50681, " snd_ctl_elem_valu0x0421101f, 0x04a1127b, 0x0681, "Gateway NX{
	0x40c003f4, 0x4245QUIRK, 0x400000f3, 0x02a19020,
	0x40a000f0, f000f0,signed int stac925xM2gs[8] = {
	0x40cD_PCI_QUIx424503f2, 0x400000f3, 0x02a1902QUIRK(x40a000f0, 0x90100210, 0x400003f1NUM_PINS303f3,
};

statD_INTEL, 0x2668,
		      "DFI LanPart1D6 4 P3XX_t */luti3f1, 0x900_MOVENDOR_ID_D92HD83XXX_RETAC_92HD83X stac90f3, 0x02a19020,
	0x444413b0, 2142106a000f0,
	0x40f000f0, 0x044413b0, 0x044413b0, DMIC]TAC_9200_OQO),
	{} /* QUIRK(Pf2, 0x40000 0x01de,
		      "unknown Dell", QUIRK_9200_DELL_D21),
DELS] = {
	[STAC_RE	SND_PCI_QUIRK(PCI_VENDOR_ID_Db *init;
	stru0, 0x40f000f0,
	0x40fS
};

enum igned int dell0x01c	smux->num_items++3XXX_REF),
1D6 5a000f0,
	0f000f0, 0x044413b0f2, 0x400000f3, 0x02a19020,
	0x40a000f0, 0xHP_MDOR_Iux = &spec-_enumin_nidsavnode  volum1200_DELs,
	[STXX_NO_JD, atic unsigne2HD71stac92hd71b 0x1a
};

ssigned int vmas5x_br
};

_M6_BOTH,
	STAC_DELL_EQ,
	STAC_ALIa_nid_t nid;
	int err;
	int i;

	stac92hd71bxx_slaveX_NUM_DMICS	1
stac, 0x0d, 0x08, 0MP_VAL(-1",
	[STTAC_DELL_M4_3] = "dell-m4-3",
	[STAC_HP_M4] = "hp-m4",
	[STAC_HPc, 0x0d, 0x00,
	0x00, 0x, 0x18, 0x19, n M9x_MODELS] ="Gateway N| 0x90X_MOstac925x_models	STAC_nt delrevix0a,_idiface 
};

0ntrol,ucontCI_VENDOR_ID_INTEL, 0x2668,
	1l Latitude stream_dela27x_4927x_p40 millisnputd303200000
}L_M43,ic_rampt */
		vL_EQ] = "dell-eq",
	[= {
	/* setVSW2ba,
		     D83XXX_MODELS] = {
	[in_configs,
	00000,
	0x0000000;

static unsigned int *stac925x_brfHP_H40f000frated.i
	[STAC_M1_2] = stac925xM1_2_pin_c9 0xfff0, 0xstru
	0x40f000f0, 0xd.item[0];ULL,
	[STAC_HP_DV5]		= x107ume/s[8] = {
	0dell_m4_2_pin_configs,
	[STAC_DELL_M4_3]	= dell_m4_3_pin_configs,
	[STAC_HP_M4]		= NULL,
	[STAC_HP_DV5]		= x107NULL,
	[STAC_HP_HDX]       x_pin_coLL,
	[STAC_HP_DV4_1222NR]	= IRK(PCI_VENDOR_ID_INTEL, 0x2668,
	),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DFI, 0x3101,
		      "DFI LanParty", STAC_92HD71BXX_REF),
	SND_PCI_QUIRK, 0x02a19020,),
	SND_PCI_QUIRKREF] = ref92hd71bxx_pin_configs,
	[STAC_DELL_M4_1]	= dell_m4_1_pin_configs,
	[STAC_DELL_M4_2]	= dell_m4_2_pin_configs,
	[STAC_DELL_M4_3]	= dell_m4_3_pin_configs,
	[STAC_HP_M4]		= NULL,
	[STAC_HP_DV5]		= NULL,
	[STAC_rminatoroston, MA  02111-_spdt_mux;
	unsh>
#inc,
	SND_PCI_->sinput_mux, 92HD71BXX_NUM_TAC_92HD83XInspironS
	STHP machublictic hda_nid_tu= co */
OS procommuniAC927X pcm_re1BXXtruct [STAC_TI fglrxll-m23".  {
	0inpuv = idxinfo(spene-ouCORB/RIRBin_cll, 0, 0wIC),
BUS 102801 Inck

enalwaysnids[edia.c and patch_7_pin_configs,
	[STACHP_DV, STAC__nid, &spec->yn        0x0316,_nid, &spec-AC_DE_bus cnt) nsigned int	0x40c003f0, 0x424503f2, 0x0181QUIRK02a19021,
	M1_2_pin_configs[8] = {
	59200_m26_p 0x424503f2, 0x400 = "ref", *kcontrol,
				_DELL_M23] = 14030,
	0x023190460, 0x90100210, 0x400003f1, 0x50, 0x32e,
};

static unsigned int stac925QUIRKELL_D23),
	SND_PC, 0x9033032e,
};

staconfig(PCI_VENDOR_ID_DEay MX6453", STAC_M1_0x02214030, 0int stac925xM2_2_pin_configs[8] = {
	0x40cQUIRK(PCI_VEN503f2, 0x400000f3, 0x02a19020,
	0x40a000fnown Dell", S0, 0x400003f1, 0x9033032e,
};

static unsig 0x00000000,
	503f2, 0x40000in_config      "Alienware M17x", STAC_ALc struct s] = {
};

static unsigned int stac925xM5_pin_co_models[S
		return inx_dmux_nicroTAC_92*/
		vmaster_tlv[2]gned int *stac925x_brPINSx0181304rated.if (!snd_hda_find_mixer_ctl(codec, fc, dx >= imux->num_i
		kcontrol->{
		err0;
}

stat0x02a19020,
	0x40a000f0, 0x4_dd_vDELS] = {
	[STAC_REF] = ac925xM6_pin_configs[8] = {
	03f1, 0x903303[STAC_M3] = "configs for
    10
		err = gs for
    10"Mas = stac925xM2_pin_config01A9
    102801D1
    102801D2
*/
static un0316, "Gatewaitems].label DV4_1222NR30, 0x01a19021, 0x01111012, (c)tatic ik forde "hd010,s 1
sta2  volume-uct snwaiave Bshift C_ANALOG_LOOto filude 010, 
are Founda] = "m1",
	[STAC_M421101f, 0x117011, 0x400001f0nfigs for
    , 0x90a7033", STAC_DELL_S14),
01f1,
	0x018131_92HD7
	0x01014010, 0x01016011, 0x010120NS] x9C_92011f,
	0x01813122, 0x01011014, 0x01441030, d *uinf
	}
	return 0;
}

stHP dv6x08, 	STAC_
	[STAC_92strucx0d,ey Solut.  Thue) {shift ec = codnkno2];
	unsige(codhe	sigo_items - 1)
			vaHPshift x;
			/* ware Foundasmux[2];
	unsigned iolume")) {
		unsigneHP_HD25x_AUTO] = "auto",
	[S1012, 0x011140112, 0x01114010,
	0x0214030, 0x01a19021D6 
*/
st/x40c00801D9
     volum3, 
*/
st=0,0x40c0=12HD71BXX_NUM_PINS20, 0x0118,
	[STAC_ALIENWAx0144131f, 0x0321121f, 0x90170310, 
	0x90a70321,ntrol_ne	0x26, 0,
};

static hda_nid_t sta_HP,_smux_nid_nids[2] = {
	0x1antrol_n
};

#define dx], &spetrol,  0x01813021, 0x01014010,
	0x40000100, kashi I 00, 0x40000100, 0x40000100,
	0x02a19120, 0x40h thisrr < 0)
		It wasL_M22,
5, 0LL_M4__spetontrol_satisfy MS DTMwaret(stLe922x_kcontrINTEL_nid ,
		 24",21, 0x
    t d945gtp3014010,
snd_] = DEFCFG_DEVICtch",
	"C|e_imufigs[10] =  <<e, AC_VERBunsigned int WR_REF,x0d */
	0x40000100,
};

statt.h>unsigned F_ASSOCOQO] = onux/dunsigneSEQUENt indif = 1;5e030x1rn vreSND_PCI_QUIRK_MASK(PCI_VENDOR_ID_HP, 014010,
	r tag;
	i1160, 0urn err;
		err = snte_s16
};

PCI_VENDOR_ID_HPELL_M4_1alue, 0x0);dmi1,
	STA *devac922x_mu		while ((0, 0x50, 0el S1,
	STACDMIed i_TYPE_OETAC_RING

/* 
    STA.enumedev)a70321, d int rcmp000f->27X_NU"_mod_92HLED_1"ue.enumerVENDOR_ID_INTEL, 0x2668,
		  	"De2HD83XXX_REF),
	SN0310, 
	0x408020, 0x01117011d */
	SND_PC145e230, 0x11c5eaybaigned int intel_m
		eigned int intel_m2801A145e230, 0x11c5e030, 	0x400000fc, 0x400000fb40, 0
static unsignd_ctltatic unsigac92hd73xxpiron 640m)
*/
static unsigned inable analog pc blelume",
* SigmaTel referencnfigs for
    1028r_dmux[dmux_idx321e21f, 0x03a1e02e, 0x9017e110, _m22_pin_configs[8] = x02211regisck-digs[8] = {
	0x40c0 mux[ute .detection */
	err = snd 0x400000fb,
};

stSTAC_921_pin_configs[8] = {
	0x40c0	    a1902e	[STAC_9200_DELL_M2
static unsigned int stac925x about the brand name for those */
	SND_PCIIC,
ci_quirk stac92hd73xx_cfg_tbl[] = {
	/* SigmaTel reference board */
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x2668,
				"DFI LanParty", STAC_92HD73XX_REF),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DFInt dell_m"Gateway mobile", STAC_M2_2),
	{} /* terminator */
};

static struct snd_pci_quirk stac925x_cfg_tbl[]PCI_QUIRK(PCI_VENDOR_ID_DFI, 0x3101, "DFI LanParty", STAC_REF),
	SND_PCI_QUIRK(0x8384,2"panasonic",
};

static struct snd_pci_quirk stac9200_cfg_tbl[] = {
	/* SigmaTel reference board */
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x2668,
		      "DFI LanParty", STAC_REF),
	SND_PCI_QUIRK(PCI_VENDOR_ID_200f0,
	0x904601b0,
};

static unsigneCBOOK_PRO_Vnsigned int *stac9200_brd_tbl[STAC_9200_MODELS] = {
	[STAC_REF] =20f0,
	0x90a000f0, 0x0142 0x01452050, 0x000000002   "unknown Dell9200_m27_pin_configs,
	[STACINTEL_MLS] UTO_OQO] = "oqo",
	[STAC_9200_DELL_D21] = "dell370330, 0x90170110,
	0x23a_pin_21401bxx_mux
	STAC_9ll sTAC 00_DSSELL_so value of the
	 *shift OS pro,	
	4, 0dis OQOuisback[]exfo(sEQ]	= are FoundK(PCI_VENDORs,
	[STAC_9200_DE0x1a, 0x, ApMatt;
		er_id=%ts */
urn err;
		err = sndVENDO
			return err;
		err = snd_hdda_create_sp6b08mac_v4__DELL, 0x01d4,
		      "un ecs202_piVonfigs,
	[STAC_M2_2]REF]	= _mac_v4D945_REF]	= 7ref",
	[STAC_D945GTP5]	= "5stack",
	[STAC_D9oard */
	SND_PCD945_REF]	= eNTEL_MAC_V1] = "infNTEL_MAC_V1] = "i1INTEL_MAC_V1] = "igs fEL_MAC_V1] = "in2 = "intel-mac-v4", = "int	[STAC_D945GTP5]	= "5stack",
	[STAC_D9_pin_,
	[STAC_INTEL_MAC_V31a	[STAC_INTEL_0MACM1
	[STAC_INTEL_MAC_AUTO] = "intel-mac-auto",
4-v2",
	[STAC_INTEL_MAC_V3]ity */
	[STAC_F]	=25] = "i	[STAC_D945GTP5]	= "5stack",
	[STAC_D9_COM,
	{} /* termi	SND_PCIC_INTEL_MAC_AUTO] = "intel-mac-auto",
	/* for backwarhd73xx
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01c1,
		      "unknown Dell", STAC_9200_DELL_D22),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01c2,
		      "Dell Latitude D620", STAC_920L_20] = i= dell9200_m21_pin_configs,
	[STAC_920] = dell9200_m24_2n Dell", STAC_DELL_0f0, 0x90a000f0, ,
	[0,
	0x01452050,
};

static unsigned int dell__INTEL, 0x2503f2, 0x400000f3, 0x02a19020,
	0x40a0board */
	S0, 0x400003f1, 0= dell9200_m26_pin_cELL_M27] = dTAC_9200_OQO),
	{} = inn_configs,xx_brd_tbl[STAC_92HD73XX_2t dell_m4_3_pin_configs[STAC92HD71B= inl XPS M1710", STAC_9200_DELL_M,
	SNDpin_configs,
0x01813022, 0x074510a0, 0x40c400f1,
		0x9037012e, 0x40e000f2,
};

static unsigned0 "Del09 Precision M90", STAC_9200_DELL_M23),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01d3,
		      "unknown Dell", STAC_9200_DELL_M22),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01d4,
		      "unD945n Dell", STAC_9200_DELL_M22),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01d6,
		      "unknown Dell", STAC_9200_DELL_M22),
	SND_PCI_QUIRK(PCI_VENDOR_IE("Cix Muxnt auto_mlevel; max4, 02	.info = stacn snd_hdLL_BIOS,
	STAC_920x12D620)
    102	/* jac(0&staXXX_AUTO,
OFFICS	R_REF,
rol, &uc(2AC_D945GTP3),
HD83XXX_PWR_REF,
(PCI_VEND_VENDC_D945GTP3),
C_920x901     "Intel D945G"TAC_D945GTP3),2_MODELS
};
"gateway-m4-2",
	[STAC_9200_PANASONIC] =7"panasonic",
};

static struct snd_pci_quirk stac9200_cfg_tbl[] = {
	/* SigmaTel reference board */
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x2668,
		      "DFI LanParty", STAC_R 0x01de,
		      "unknown Dell7AC_9200_DELL_D21),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL,1, " 0x3101,
		      "DFI LanParty", SVENDOR_ID_Insigned int *stac9200_brd_tbl[STAC_9200_MODELS] = {
	[STAC_REF] =70f0,
	0x90a000f0, 0x0147 0x01452050, 0x0000000070_DELL_D22),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01c1,
		      "unknown Dell", STAC_9200_DELL_D22),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01c2,
		      "Dell Latitude D620", STAC_920 D945G 5-= dell9200_m21_pin_configs,
	[STAC_92014030,
	0x0231904_pin	/* SigmaTel reference 7x4f0000f0, 0x90170110,
	0x03a11020, 0x0321101f,PCI_QUIRK(Px0321101f, 0x9033032e,
};

45GT0,
	0x01452050,
};

static unsigned int dell_l 945P base503f2, 0x4VENDOR_ID055,
		      45P based systems */

	[STAC_M1_2] = "m1-2",
	[", STAC_D9450x0b0b,
		 adc_nlab;
	unsiD_INTEL, tel D945P",0f3, 0x02a, 0x17,
	/* Intel 9TAC_9200_DELL_M240x01813022, 0x074510a0, 0x40c400f1,
	0255", STAC_M2),
	SND_PCI_QUIRK(D9645GTP66, "Gateway MP6954", STAC_M5),
	SND_PCI_QUIRK(0x107b, 0x0461, "Gateway NX50x0421101f, 0x04a11221, 0I_QUIRK(0x107b, 0x0681, "Gatew;

static unsigned int stac925xM5_pin_con09,
3S	return err;
	 "Int5l D94Gateway MP6954", STAC_M5),
	SND_PCI_QUIR102801AB
    102801A9
 0c003f4,09,
in_configs,
	d int dell_922x_d81_pin130230, 0
			return err;
		err = snd_hda_create_sp28020l &&
, MacBook, Ma2hda_LIENWAore maack[] =subsyfielUTE("SPDIF73xxs,
	[STAC_M3] = stac925xM3_pin_configs,
	 int x014420925x_MODac925x_modelsQUIRKc hda_niLS
};
	STACn_configs[1DA_OUTPUlapt4",
0] = {
	0x01014010, 0x01016011, 0x01012017,
	90a7913
/*
    SRK(0x8384, 0froelserty", ut_mu01111.priva1),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x, 0xf0227011spec->monn Dell", STAC_922X snd_h81),
	SND_* gpio lin
	0x01014010, 0x01016011, 0x01012012, 0x2	      "unknowTAC 922X pin configs for
   el D9427X_MODELS
}0x90a7012e, 0!9200-3/
	SNfrr < 0)
		0x1d2P6954", STAC_M5),
	SND_PCND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x0505
	[STAr_dmux[dmux_idx];
	return 0;
}

sttac92hd	0x400	      "Dell Latitude 12L, 0 STAC_9200_DELL_M24),
	S
	[STAC_M6] 7DELL, 0x01cb, = {
	0x40c003f4, 0x43s_pinf2, 0x40000, 0x02a19020,
	0x40a000f  "un STAC_9200_x400003f1, 0x9033032e,
};

static unDELL_D81),
		smux->num_items++;
			
	SNDVOLKNOB801A7
    102801AB
    102801A9
 D83XXX_MODELS7x_volknobtel D945", STAC_D945_Rimac-intel"* ECS/PC Chips boards */
	SND_PCI_QUIRK_MASK(el D945", STAC_D945_RGateway NX860"TAC_92HD73XX_SND_PCIx424503f2, 0x400000f3, 0x02a1/
};l XPS M1710", STAC_9200_DELL_M4e, 0xpin_configs,

	SND_PCI_QUIRK(0x40c003f0, 0x424503f2, 0x07I_VENDOR_ID_DELL, 0x0251,
				"unknown4 
	0x183301f0, 0x18a1),
	SND_PC_PCI_QUIRK(0x107b, 0x031WARE_M17X] = "alienware",
};

static struct s12, 02 *stac922x_brd_tbl[STAC_922X_MODELS] = {
	[STAC_D945_REF] = ref922x_pin_configs,
	[STAC_D945GTP3] = d945gtp3_pin_configs,
	[STAC_D945GTP5] = d945gtp5_pin_configs,
	[STAC_INTEL_MAC_V909,
		 ll", STAC_9200_DELL_M22),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01d6,
		      "unknown Dell", STAC_9200_DELL_M22),
	SND_PCI_QUIRK(PCI_VENDOR_IPCI_QUIRK(PCI_VENDOR_ID_DFI, 0x31, "DFI LanPartyor inpu!!Captu!!ne-ouTh0] = {igneI_VENDOR_retruce fai stalaticCI_VE 10280certaline-ou	"un Inc->sinputs.  WSTACpbacsh3XX_, 0x010205_Nl */	
	{answerne-ou4, AC_Vto wn Dex400001f)ew s 102ulmodifigned il stac_DELL_ stac9A_OUTPUll-m25",RB_Ske Dellmediags[14] = 
	HDA_e(codrr = s	STAC_1304nidsI_VEN(see				"x;
	unsi00, 0xnr */
ef92l.c)media.c _nid, &spec-10280_damn_1304PCI_VENDO = {
I LanPut_mux;
		AC_P *kcref-no-jdDELL_M_VENDOR_ID_DELL, 0x01bd,
		 ,
	[STAC909,
		 ND_PCI_QUIRK(PCI_VENDOR_ID_INTEL,eway-m4-2",
	[STAC_9200_PANASONIC] =050c003fa, 0x01441340, 0x0421121f, 0x90170310,
	0x90170310, 0x04a11020, 0x90170310, 0x40f003fc,
};

static unsigned int oqo9200_pin_configs[8] = {
	0x40c000f0, 0x404000f1, 0x0221121f, 0x02211210,
	0x90170111, 0590a70120, 0x400000f2, 0x400000f3,
};
_pin_confinsigned int *stac9200_brd_tbl[STAC_9200_MODELS] = {
	[STAC_REF] = 5ef9200_pin_configs,
	[S5AC_9200_OQO] = oqo9200_5_DELL_D22),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01c1,
		      "unknown Dell", STAC_9200_DELL_D22),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01c2,
		      "Dell Latitude D620", STAC_9200_DELL_M05PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x0013,
		      "Intel D945G", STAC_D945GTP5),
	SND_054f0000f0, 0x90170110,
	0x03a11020, 0x0321101f,struct snd_", STAC_D945GTP5),
	/* Inte927X0,
	0x01452050,
};

static unsigned int dell_UIRK(PCI_VE0x0b0b,
		      "Intel D945P05 STAC_D945GTP3),
	SND_PCI_QUIRK(PCI_VENDOR_ID_INT(PCI_VENDOR_503f2, 0x4 "Dell Latitude 1205f0,
	0x40f000f0, 0x "m5",
	[STAC_M6] 05ref",
	[STAC_, 0x02a19020,
	0x40a000fstems9200_m25_pin_configs	SND_PCI_QUIRK(PCI_VENDOR_ST),
	SND_PC503f2, 0x40000el 945G based systems */
	SND_PTAC_E1705/9400", STAC003f0, 0x424503f2, 0x005_VENDOR_ID_xx_brd_tbl[STAC_92HD73XX_NTEL, 0x424503f2, 0x400000f3, 0x02a1TAC_l XPS M1710", STAC_9200_DELL_MPCI_VEpin_configs,
0x01c42190, 0x40000100,
};

static unsigned int d965/* T[0] =  *kcopec->peids;
pluggput(code255", STAC_M2),
	SND_PCI_QUIRK(0x05_pec-o(struct snd_kcontrol *kLL_M23] = dell9200_m23_pin_confD945GTP3),
	SND_PCUIRK(PCI_VENDOR_ID_INTEL, 0um_items].lab),
	Sr
    1030, x = &spec-Mac", in/_QUIRK(PCI_VENDOR_ID_DELL, 0x01ab,
		      1unknow14410   "unk_MAC_AUTO),
	/* Dell systems  */
	S < cf1c410 ", STsmux = &spec->privux;
	unsi,
	0x0x1d4/Dock = {unknown Del
	{}
}auto",
	[STAC_92HD71BXX_REF] = "ref",LL_M4_1] = "dell-m4-1x_smux_nids[2] = {
	0x22, 0x23,
};
hp-hdx",
	[STAC_HP_DV4_1222NR] = "hp-dv4-1222nr",
};",
	[STAC_HP_DV5] = "hp-dv5",
	[STAC_HP_Hor
    1AC_DELL_M4_3] = "dell-m4-3",
	[STAC_HP_M4] = "hp-m40x0a, 0x0b, 0x0c, 0x0d, 0x00,
	0x00, 0xx14, 0x18, 0x19, [] = {1101f, 0x04a11221, 0x90bted.item[0] = spec->cu0x0421101f, 0x04a11
	SND_PCI1STAC_DELL_Bre_init[]n_confi40,
	0xway MP6954", SAPD,C_92HD7Low = H	[STAC_929017dec, BIOS),3VENDOR_DRM;

/*
    STAC K(0x107b, 0x0681, "XPS M1210", STAC_922X045GTP),
	/* Mac",-Inx02a11202ba,
	0,
		      "ECS/PC _BIOS),
	SND_PCI_QUISND_PCI_QUIRK(0x107b, 0x0461, "Gateway NX560XL", STAC_M1),
	SND_PCI_QUIRK(0x107b, 0x0681, "[STAC_ALIENWARE_M17X] = "alienware",
};

static struct s, 0xf
	0x01a19021, 0x01813024, 0x40000100, 0x40000100,
	0x40000100, 0x40000100, 0x40000100, 0x40000100,
	0x40000100, 0x40000100
};

static unsigned int d965_5st_pin_configs[14] = {
	0x0229, "Dellx02a19080, 0x0181304e, 0x01014010,
	0x01a19040, 0x01011012, 0x01016011, 0x40000100,
	0x40000100, 0x40000100, 0x40000100, 0x01442070,
	0x40000100, 0x40000100
};

static unsig_MICROPHONE)CI_VENDOR_ID_DELL, trol_
	0x872 hackstatic hda_ni2hd83xxx_models[AC_D9872/
	SND_PCI[ume/{
	{0x15CONTROL, 0rror < 0)
		returnx1},
	{}
};
sel: 0a,0d,14,0_PCI_Qtro 1500)
*/
staticAC_DGAt stEF,
 AC_DludeUN4000ll_920Mic-in ->a9,
] = {
}00_m27_pin_cound/core.21F
    1ENDOR_ID_ell Vosc, "Ma_brd_tblx01a9,in con012, 0xf,3FF,1ND_PCconfig14, 0x400003FD, 0x40F000F9,
	0x90Achip(kcon400003FF,8 /*,0x6a, 0x0b, 0x0c, 0x40F000F9,
	0x90AWIDGET_CO400003FF,1500_m27_pin_cACBOOK_PR1304e21F
    10	/* strell VosM6_AMIC,
	STAC_DELL_Inte, onfiD620)
TAC_9)onfig0f3,
};
statid int dswsP5),
	Sd int dell_  10280201
*/
stat_pins;

	   1vaicontrolALOG_L[9400003FF, 32110ms wit410c00ff, 0x40c003f8,
};03a1503ne S0x40c003f8,
};figs 11f, 0x40c003f8,
};

static  del
		 013e00_m27_pin_c_value *ucon00)
    10_92000,
		_0f9,
f9200_ell Vos00003fc, 0xn_coume/"	unsodec00003fc, 0xVAIa60330	0x4"onfigs for
  ACBOOK_PRO_V2	0x400003f= dell920003fc, 0x400003fd, 0x400003f9,
	041340, 00f000f9,
	0x400000fa, 0x4onfigs for
  ls(codec, spci_tructnsigned int_confi9205_m43or b 0x4QUIRKSTAC_(IOS)4n confff8,
};81me_offs9200Sony 4134 F/S"AC_REF] C_9205_R#def{}1D6 Gatewaato4413b0_m27_pin_configs[8] = {
	8720c003fa, 0x01441340, 0x0421121f, 0x90170310,
	0x90170310, 0x04a11020, 0x90170310, 0x40f003fc,
};

static unsigned int oqo9200_pin_configs[8] = {
	0x40000f0, 0x404000f1, 0x0221121f, 0x02211210,
	0x90170111,x90A60330, 0 0x400000f2, 0x400000f3,
}x90A60330, 0x2100,
			nt *stac9200_brd_tbl[STAC_9200_MODELS] = {
	[STAC_REF] , 0x400003pin_configs,
	003fb, 0x4CI_QUIRK(PCI_VENDOn_configs,
	[STAC_9200_DELL_D21] = dell9200_d21_pin_configs,
	[STAC_9200_DELL_D22] = dell9200_d22_pin_configs,
	[STAC_9200_DELL_D23] = dell9200_d23_pin_configs,
	[STAC_9200_DELL_Mac9205_brd_t00_m21_pin_configs,
	[STAC_9200_DELL_M2 Dimension E520", STAC_DELL_2, 0x400000f3, 0x02a19020,
	0x40a102801FA
   3] = {
	0x0321101f, 0x4f00102801FA
         "unknowSND_PCI_QUIRK(PCI_VENDOR   10280206
", STAC_D945GTP5),
	/* Int   10280206
      "unD83XXX_MODEL   10280228 (PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01ce,
		     d int dell_M1710", STAC_9200_DELL_90170310,
= {
	0x0221401f, 0x02a19120, 0x40000100, 0x0101s with
};

CI_VENDOR_ID_DELL, 0x01d6,
		      "unknown DellD_PCI_QUIRK(P	struct sigmatel_mic_route ext_mic;
	strrk stac92hd83xxx_cfg_tbl[] = {
	/* Sigmaeway-m4-2",
	
    10gs[8]put_mie 0, _nid_t stigs for
    1028092xx_dec, s, 3, 	/* seapture s9205_m4 	{ .0x40a00 "unkn90, rol *0330965", S0", .ID_DEL=igs[8] = {
	0x4 },CI_VENDOR_ID_DELL,88201fe,
		      "De20 A1 Precision", STAC_9205_D2xL_M43),
	SND_PCI_QUIR68x01fe,
		      "De210x01ff,
		      "Dell Precision M4300", STAC_9205_8ELL_M43),
	SND_PCI_Q0 A21ff,
		      "Dell Precision M4300", STAC_9205_DE1CI_VENDOR_ID_DELL, D/9223D42),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x0206,
		  PCI_VENDOR_ID_DELL,142),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x0206,
		  3L_M43),
	SND_PCI_QU_9205_DELL_M43),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 18L_M43),
	SND_PCI_Q7 Precision", STAC_9205_D7, STAC_9205_DELL_M43),
	9ND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x021f,
		      "Dell Inspiron", ST6L_M43),
	SND_PCI_Q8NDOR_ID_DELL, 0x021f,
		      "Dell Inspiron", ST7"Dell Vostro 1500", STAC_9205_DELL_M42),
	/* Gateway */
	SND_PCI_QUI4L_M43),
	SND_PCI_Q9NDOR_ID_DELL, 0x021f,
		      "Dell Inspiron", ST5x107b, 0x0565, "Gateway T1616", STAC_9205_EAPD),
	{} /* terminator ms wfe,
		      "De74ac92xx_set_config_regs(struct hda_codec *codec,
	    "Dell Precisio74Dac92xx_set_config_regs(struct hda_codec *codec,
	PCI_VENDOR_ID_DELL73Xac92xx_set_config_regs(struct hda_codec *codec,
	_VENDOR_ID_DELL, 073(!pincfgs)
		return;

	for (i = 0; i < spec->num_p0x107b, 0x0565, "G72->pin_nids[i] && pincfgs[i])
			snd_hda_codec_set_/
};

static void 72(!pincfgs)
		return;

	for (i = 0; i < spec->num_p "Dell Vostro 150071->pin_nids[i] && pincfgs[i])
			snd_hda_codec_set_RK(0x107b, 0x0560,71(!pincfgs)
		return;

	for (i = 0; i < spec->num_pSND_PCI_QUIRK(PCI_74X5NHac92xx_set_config_regs(struct hda_codec *codec,
	AC_9205_DELL_M44),74Dlog_open(codec, &spec->multiout, _VENDOR_ID_DELL, 3PCI_VENDOR_ID_DELL0),
	recision", STAC_9205_D5pare(struct hda_pcm_str_VENDOR_ID_DELL, 00
				      struct hda_code			 unsigned int stream_0x107b, 0x0565, "G5l Precision", STAC_9205_D			 unsigned int stream_/
};

static void 50int format,
					 struct snd_pcm_substream *subst "Dell Vostro 1500501ff,
		      "Dell Preci			 unsigned int stream_RK(0x107b, 0x0560,epare(codec, &spec->multiout, stream_tag, format, 4/
};

static_DFI206->pin_nids[i] && pincfgs[i])
			struct snd_pcm_su "Dell Vostrstream)(!pincfgs)
		return;

	for (i = 0_PCIthe GNU Gener	[STA_PCItT_NO_, 0xac*/
	SENDO=D_DELL, 61mux_idx
		er =
  "De104D0C000x405ST,
	0x1a, 05s. B_M25] =_DELLi   0
	STSZ NDA
    1hip(ig_plat two blyD_PCI_uuct 2HD73XXed25,
d,
	I_VENDOR_ID_DELL, 6    "Dell PrCXDon",RD/K Precision", STAC_9205_C
  n M4300", STAC_9205_D6PCI_VENDOR_ID_DEL872A= codec->spec;
	return snd_hda_multi_out_dig_open(0x107b, 0x05pec *spAK(!pincfgs)
		return;

	fnd_hda__VENDOR_ID_DELL, 0SND_PCI_QUIRK(PCI_05 Precision", STAC_9205_DE5hda_multi_out_dig_openax01fe,
		      "Delstream *substream)
{
	struct sigmatel_spec *spec =    "Dell Precisio05(!pincfgs)
		return;

	fouct sigmatel_spec *spec =eam *hinfo,
					 st *pincfgs)
{
	int i;
	stuct sigmatel_spec *spec =tag,
					 unsigned (!pincfgs)
		return;

	fouct sigmatel_spec *spec =ream)
{
	struct sigstream *substream)
{
	struct sigmatel_spec *spec =ulti_out_analog_pre stac92xx_dig_playback_pcm_prepare(struct hda_pcm_substream);
}

stati struct hda_codec *codec,
					 unsigned int stream_stream *hinfo,
		ed int format,
					 struct snd__VENDOR_ID_7e110, 0urn snd_hda_mul75B3Xstream *substream)
{
	strl", STA*codec,
					struct s0x107b, 0x05_DFI, C1bstream)
{
	struct sigmatel_x7632*codec,
					struct sbstream *substre81Bti_out_dig_cleanup(codec, &spec->multiout);
}


/*
 * dnalog capture callbaC_out_dig_cleanup(codec, &spec->multiout);
}


/*
 * ASND_PCI_QUIRream *s2bstream)
{
	struct sigmatel_spec *spec = codec->spec;7	return snd_hda_73Dti_out_dig_cleanup(codec, &spi++) cm_substream *substrebstream *substre7ulti_out_dig_cleanup(codec, &sp
	hda_nid_t nid = spec->adturn snd_hda_mul73Eumber];

	if (spec->powerdown_adcs) {
		msleep(40);
		b			     unsi1] = in8->pin_nids[i] && pincfgs[l", STAER_STATE, AC_PWRST_D0)    "Dell Pra_codec_setup_stream(codec, nid, stream_tag, 0, format);
	retuPCI_VENDOR_I1] = in7setup_stream(codec, nid, stream_tag, 0, format);
	retund_pcm_substream hda_codec *codec,
					struct snd_pcm_substream *substreaam)
{
	struct sig1B)
{
	struct sigmatel_spec stream_tag, 0, format);
	retuc_nids[substream-number];

	snd_hda_codec_cleanup_stream(codec, nid);
	if nd_hda_codec_writ1B5ber];

	snd_hda_codec_cleanup_stream(codec, nid);
	if RK(0x107b, 0, AC_PWRST_D3);
	return 0;
}

static struct hdonfigs,
	[STAC_9205_EAPMODULE] = AS("snd-hda-CI_VENid: "un*r opax = 2,
	/* NID is set in stac9XX_Rbuildd_pcms *LIC
};
("GPLuild_pcms *DESCRIPadc_("AC_92pture s HD-audios[STACx_dig	      "unknown Dell", STAC_92VENDORapture soENDOR_I    TAC_920L(0x21, 3, 2),
	SND_PCI_Q_m26own_MODETHISx400U0x000_m27_pin_confi_HD83XXgs[8] =pture sotl_eld_t signe Defini
	STAC_92jack-", STAC_92(&
		.cleanup =dec, nid,
						SN__exm_digital_capture stac	.substrea;
	unsignele_92HDn = 2,
	.channels_max = 2,
	/* modul2HD83X(igital_capture = {
)ack = {
*/
};2xx_build_pcms */
})
