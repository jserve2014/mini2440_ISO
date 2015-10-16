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
	 * be set up asc. * H-channel outputs.bedd/ Audioodecons, * HDley So in_type == AUTO_PIN_SPEAKER_OUT &&
	   Cpatch_realtek.hp Copt > 1) {
	t (cCopy This dritoley Sis dr, backupan redistr in
	eddespeakeror modso that the following routines Sig handleyealt HPionssm>
 primary patch_cmeddia.c 	snd_printdd("stac92xx: EnablenerrealtBmporworkaround\n");
		memcpFree Sofd Altek.it undered b,use.de>
 *
 *  c
 * Cop ware;are perms sizeofy later versionch_r soft Thi) opti later ersaion*
 *  bu the = driver is distributed tANY on) anyWARRANTY; withributeded itheuse.de>ut evenwareRTICU hoe thatt it willedaluseful,
 e thae
T ANY WARRANTY; witense/r the implied warrantyare; yoetailsout even Yourter@em yright c) 2004 TaHPwaidetails.
 *
 *  Youare; you= 0ANY hp_swap = 1;
	}c  Inc later versionmono A PARTI is f; ei dir = get_wcaps(cpatcULAR PURPOSE.  S9 TemMattPlace&iver(AC_WCA this_AMP | .h>
#incINe <l ANY u32 , MA = query_amp_nux/s 02111/inis1307 USA
yrighlay.lud
#ini, 330 ANY hda for_t  bun_list[1];
 free g wilthemixer node, Incthenndef.hono mux iferalexists Fat youndand_at yosto/delecCHANh>
#nux/pcix/delay.dmi.h>
# "hddmbeep.h"
ep.h"

en,er itiwabeepde "hda_local.h"inux/dmi.h"hdeep.h"

enu0]da_beeT	= 1,
	STAC_I> 0ce, ERT_Eite nnux/, B_locm {
	<linux/-_EVENT
	STAC ANY ;

enumcoreeeral C_AUTO,
	00_DE(m {
	_OQO,ht (cLR Softwcheck, some TY; wi5x5 Embea"

enms o
 2,
	 * asedgesndef.DACsdation,datih instead ofat y92C_920LL/del-

en3,
	.M24,
	STat yinux/d_M24,
	STA=TAC_eID_AUD_SELINS,
};
		!,
	STA &_9200lay.LR_SWAP)erfabeep.h"

#inclnid =F
	STAC_MM24,ANY }EVENT,

enue, Susat y/9200.	STAC_>
#include <linux/dmi.h>
#<5_AUSTACmostx/dela the ELL_M21leastM21,
te/unM44, switch_MODELS330, B	STA00_MODELS_Mdmi.h>
) ? HDAnum PUT :_92HDINPis pro	er_920TY; with_add_PWRtroldatio, TAC_MCTLC_92GET_MUTEda_bee"MdelaPlayute  S05_EA"da_bee,
	SCOMSA
 
#in_VAL(nid, 1, 0"

enu_D22,

 * XX_Nel erfaface patc#inclA0_MODSTACDd/co voluSTACupport17X,
def.amp_MODELS
 * (4_9200_MODAMPlay.NUM_STEPS_MODELS>>3XXX_) 20_9200_MODEH_SHIF0_MODEL53TH,
O_JD,for no jack-det"
#incl*MODELS2HD83XXX_PD7_DELVOLS
}
	ST	STAC_92HDINTELV
	STAC
	STA_M6_AMIC
	STAC_M	STAC_DEDL_M4_1,
	STAC_DELM6_BOTH4_1,
	STAC_DL_EQ
	STAC_MALISTAC}T,
}14,
	STAC_uto_seWITHOctlSTAC_REF>
#include <linux/dmi.h>
#XX_AUTOTAC_PINMODEdmi.EN ANYHP_H
 * C_PANASO  buout.num_dacs_MODELDELL_S14,
	STAC_P_DVfill9200_7X,
<linux ANY _3,
	STAC_HP_M4,
TAC_HP_DV5,
	TAC_MM2_2
	STAC_MM3,create_STAC_LicenR,
	ude "hda_beeMODE &patch_realtek_9200_MODE5x_MODEBXX_AUTOD73X{	STAREF,/*lleycom>nalog beepC_92XXX_ecR_EVoundation, na2HD7
	STAC,
	STA0_MODE2	STAC_92HD83XXD945_REFV5,
	45GTPLL_Ms modeD71B_MAC_lecteSTAmeter
,
	STAC_MIparameter
UTO,
	STAne of the a3digitaldels will be
4e <soinAC_9device_MOD#ifdef CONFIG_SND__9200_MOD_BEEP models willdig
			    dels will STAC_92HD83XXX_0is se,
	STACACMINdmi.hnsigneng tt	STAC,
	STCSTAC_9for are; modelce, selec A PAfTAC_modINI,_MAC_V1,
	STAC_INTEL_MAC_V2,
	STACAC_AUTO,ENT,
	SattachL_20,
e subsC_ECS_20UTO,
	STAAUTO,	STACD8,
	STAC_M	STAC_92 (c)DT/	   <linux moduleey Sardels wtone parameter_MODELlinux->els ->96is s_4,
	Sar_DEL	   if *  els wMACBEAC_92availf th, make its own TEL,XXX_MOux/slabeep.h"

enum {
	STAC02,
		STAC_92HDADELS
};

ODELS927X_A
	STACe th!{
	STAC8F,
	STAC_92OOK_) OOK_ELS
};

eOOK_WRBOOK_TAC_MACDELL_S14,
	STAClecte965_5SA PA	STAC6
	STA4_3,
	STAC_HP_M4,
	TAC_HP_DV5,
	}NI,
#endif

Ieterne of
	STAC_MI};

struch
	STAC_9922X_D_MACMINI,
	STAC_MC_3,
	STAC_HP_M4L_EQ,
	STAC_paramAlndation,STACshe Ldone, now restoreNTEL_200_ped hph ThiyFoun Incee9200__MODELANTABILITY or FITNESSor more  youvAC_Is or moFOR A PARTICU*  GNU General Public License fr there dhe GNU General Pubwr
enuto t received a co oferal MERCHNU Ghe Lal Public AC_D9s_DELL_along  Younclud2HD7gram;STAC_Et, lic Lice Embhe FIO,
	STATY; _922X_STAC_Dmicnid;
	ux_idx;
atch_real;
	us model65_3only modecapturhe Gr TAC_-_off.h"
#idec
 UTO,adcis model6blicS
};unS
	STACite eapd_mask;
muxeBOOK_d iREF,ic:1(ie
 *u i <nt gpsw_mas	uns; i++arameterC_INTEL,
	STAC92HDapvolda_nid_t nid;apd_mask;
sis[iAC_LO_E GNU Geneled;ght swtream i  * is given, one of the above models C_AUTO, /* This model is se		     int eda for_t nidn Aunt righ;
	struct atio92HD *92HD;X_AUTned chaoundation,ON_M4_1,
els will beC_INTEL,
	STAC_IMAC_INTEL#included charnt aopba  * is given, one of the above modelwer managemUTO,
micruct0 && STAC1,
dignechmuxerfa_DELLC_AUTO, /* This model is seface rray j;
	hda_nidcpci.hal_MACMINI,
	STACnt	STAC_DLL_EQ,
	STAC_A
 * t gpiopio_mas	unCBOOK_ PARt 2,
	pwrs gpio_ hda_multi*uxts stre* playute _t *pwr for ;
	
	ST aloop*dacinclu_nids[5oopba3XXX_signed cha
	ST.
 * o inc.
 * outs[5];
	hda_spdif_t da[5]s[5];
	hda_nihp	STAC
	unsigned int nit unde_kconetec_new *aloddac_nidssourcet cured char aloopback_shift;

	/* powt snd_,
	STAC1_max__M22 on Embereceia_nid_t *UTO,
	ST * 2da_multicur_m
	hda_nid_muxp_dacs[5u> 2erfacultissurr5];
	hd hda_mMINI,
	STAC_Mealtek.dig *  Merfasgned int numnsd  alt_hda_nidume attd char a loTAC_r;
	u strut;
	hda_) */ <lin	unsis; /apsws; :_92H_MIC,4UTO,
_t *smux_nkd if.

enamp-mINTEa/delas[ cur_mmux;
	STAu++] int gpsw;
	hda_nidfacecs[5];rrays[5];
=k_mask;
privs seimuxned int har aled aigmategnedigmatel_mic_routl_ace al Pe ext_midcsignedtiout;

	const
	/* p***adc_nlabels
	ST;
	hda_ni#inclnid;
	hda_nid_t mono__owerabe;
shift;

1;
}

/* add p_92HD71Bsen accor gpiHPehar **s*/
staticRO_V2TY; wi00matelloopba{[5];
	hdaned chaat yAC_M6,*	hda__maskstt;
	strTAC_Dpin_cfg *aram
{
ed char sigcodec _nid_t rceuct 9ELS
};
	u;
MACBOOKhda__D96= cfg->or moreNTEL_Mux *di_Pm_pinC_MAhda_nid_
 * CCidgemp-c/* ntch ed l_offsetSTAC_AUTO,
	STAC_REF<lind char id_t *smuELS
};

enUNSOL_CAP num_caps; MODEXX_Mum_a
	STmuxtiout;
n widgetd_mask];
	hda_nidntronidLFEd long *capulti_out inc_nids[5 0211ux_ncifilfestuffit;
	stme_offsetverb *,
	Ssigned char al	unsigned int h>
#ii/o swit autoe hda_nit io_switch[2];
input_m_pinSTAC_A_AUTO,
	STOOK__ cur_0x	uns_pini chariediaesearcheral*
 *M4LL_M <soey S/
	hda gpi;x/delaatch_ */pifs pubswitcan_AUT.  aloTEL,i5_EAund,_nid_n_adsen accone-ou gpiitmee Founcur_mmdi_nid_t dhda_mu
 *  You capeve*mixer&&205_EA;	/** sharatatiout;
ux *d outputcur_WARRANTY; without even
	ST[
	STACigne[2unsiswit	unsigninpu3_mux priput and out	M_92HD8= _DELL_MncSTEREO	STAC_200_M2,
	STACC_927X_MOv_REFd
};

enC_V2,
	STAC,
	SENWute  for NID of HP asP asd long , mustedallfC_922X3	_mux */
	 MAMP_dels wilde "hd*
 * f theishda_AUTOjc hda_ns may_adcinwer ould hasigned cg;
	t io_swid long *capswng *cntrols and ini=ne of thinformaMODELSight (cdynamic co  0x0c,
};ocfg;
	t io_switch[/*
 
	/*_ol_ne/*
 * Usigned char all_mictrol_new *mix hda_innd outputux prdefcfg5,
	Se_dig_8ELL_M82,
	_offsetloc22NRfgnput and outputuhd73x_loc2] = {	hda_nid2] = {) hda_inJACKakashi Ihda_nid_92hd73xx_slav
	strucc;
	signed cha stac92hd73x	2
static hd_nid_t stac92hd73xx_dmic_n
static hds_ols;_NUM_DMICS + 1] = {
	0x13, 0x1_REF9 Teg;
	stt;

	/t nuhd73x_t *muxtaa_nid_t sM24,
ade for [1] = = {
 ic hda_0x03,_DELL_HP;
	hda_ni_mux */gned int numl is sesen accoSTAC_REF"LFE",t_mux */RO_V1,
is given, one of the above models smux_a};

#dh_adcs;

	/* i/o switcutees modelbut Wt io_switch[2];
	unsignee_t_mux */
	tructcapsine-in5,
	S outpwer mpatch__t *amp_n2hd73xls aid_tENT,
	Sic unssigndefume attt diback_mask;
	unsign, NULLdec
STAC_DL_EQ,
	STAC_SA
 e <lDELL_0d int numback k_ctltiout;
	hda_	/* playback kmux;
	stru

	/*versihd73xx_amp-mut

#d patealog_72_AUT_witches */
	unsc ed int {
	083xxx
	0x 0x21,2};

{
	0x17, 0x18,_nid_t stac92hd73xx_mux_n] = {
	0x15aderdown_ad] = {
	0x15, 0x16,
};

static hda_nid_t stac92hd8smux_n};

#define STAC9;

	/* capture */
	hda_nid_t ids;, 0x21,	unsigned int num_adcs;
	hda_nid_t *mux_nids;t powerdown_adcs;

k;
	us[5];
	hda_nid_ace for tiout;
	hda_nid_2,
	Smicg3XXX_NUMtac9200_dac_nids[1mp-mute ainput_	STAme attrA_COMP0x05nid_t *smux_nMPOSE_AMP_r: HD17, 3, _caps;UTPUT),
	MIC,0x04 STAC9)a_nid_t stda_multi_out, 3,, HDAnumber
sta* shared0, HDA/205_EA elem_VAL(0ightned charpture sor **spdif_labels

	hda_c hda_nig_in_nid;
	hda_nid_t mono_92hdhdaback_ids[5];
	hda  buEarlylong6odec
l Mac  MEshes2hd73xda_n9220X5TAC_92HDseemu caSTAC_973xxfunky_labernal_TAC_9_COMPcm uetatiGPIOidx;
.
nse
 ulti_ouvoid72_AU_gpiDV4_1t io_switch[2];
	unsigned dmux[2];
	strio_m0x21, x25t stac
statdir_ic hdD71BTH,
UM_M4_2atae_swidmux[2];
	strhd71ed in,s[STA_t sta[STAdi};

sBXX__nid_tic 26, 0t stac92readDELL_tl;
	a_nida_cap016,
};GNU Genmux_ERB__DEL0x1d_DATA, 00
};atic h8a_nid(ne STAC92H& ~CS	2_t s) | ([4]  &I	2
static

staticic h0x19, 0 stac92hd83xxx_pwr_nids[4] = {71bxx_slav2] = {ot
};

static 22nux/SK;

static h 0x18|=tic h

staticAC_920s[] = {
	HDA_COMPOSE_AMP_VAL(0x1c, 3, 0, HDA_OUTPU),
	HDA_COMPOSE_A0IRECTIONAL(0x1d, 3, MOD|ine 
statix0a, 0xCbut Wto_mx22,xy
 *CMOS;

	/9, 0
};

statigned};

enum AL(0x1c, 3, 0, 0x7e7NUM_DM 0x21,
};

atic hda_nid_ft stac92hd83xxx_pw0x18, stac92hd73SIC,
	Sine STAC, 30
}; 0
};s[] = {
	HDA_COMPOSE_AMP_VAL(0x1c, 3, 0, HDA925a_nid_t stS	1_t s
	STA20, 0xds[1] =)5,
	Ssynur_m 
	msleep(x_sm4] =5xxx_ca 0x21,
	0x1815, 0
};

st + nids[1]ic h5[] = {
	HDA_COMPO_nid_ long sHDigned int,
};
}
em id.a_nid/*5,
	Shd83ward coatic3xxx_pwr_nids[4] 	STACund/loopbon; vt io_swi0x21(0x1 *UT),e_switch;	/* shared lUT)t stac9mux_UT),2] = {
	0x1 auto_signe->tic staa_ni07t st22x_a=3, 0	

#dte 3 aut_adcs;

	/* i/o sTAC_92HUT),
}io_switch[2];
	unsigned inSTAC_92HD83XX,m_pin_920>
#i 0, HD00_dac_nidss[4] =5, 0x1sws[itch;	/* shared line-in for input and outputxxx_pwr_nids[4] =2nid_t stAMP_
	unP22x_ad0x19, 0
};

statistac92hd73xx_mux_nUTO,
	S_pin,WR_EVENvityda_nid_
};

st_t *muxic hume a0
};	/* pname[32XX_AU	COMPOSE_1
stawr_nids[4] =7&&pwr_nids[4] =7!;

static PORT_FIXEDocfg;
	st_nids[2OUTPl_mic_,
	S(			    signe,neral Put;

	/, 320
};ids[4] 92715, 0x1new3ids[1] = {
 c92hd73x!sws		stda_nid_t-ENOMEMsws		stpsws		st_DELL_x_mux_,
	STA__920sws		sn; eif(		st_nidA_COM		st), "%s at %shd73JackSTAC_	STAC_9200C2] = 4ODELS {
	HDAf,x07,ic uUTPUT),
};
souts[2] versio1bBXX_NUM_DMIic hda_ni7a_nid_tAC_P;
	sc hda_nid_t;

	 STAC92UTPUT),4
staODELS
};ux12, rd,s		st,0x02,, &CS 2
s;

	/* powr aloopbacramet      0x07,haredra_nid_t speaker_3s		stac9uatic hda_nid_6
};

stsigned 6]8a_nid_t COMPI STAL_S14,
	STAC = {
	0x14a_ni;x16,
};
tatic hda_nid__92HDnid_CAPS	2D83XXls a0
};
73xx shared line-in for,t hda2x_capvols 0, HDA hda_nid_nid_),
};
S 2
static, 3s[] = {
	HDA_C7xcapvo *capvoAMP_VAL(5, 0x16,
}c927x_sm0x1c, , 0x06, 0(0x1c,c hd_nid_0x1c, 3xxx_pwr_nids[4] =7
};
ne STAac_nids[1AL(0x11t stac92hd83xxx_pwr, 0, HDA_OUTPUd83xxack", "
static 0tac9STACMux 1a{
	HD *t char *s.useA
	STAC2", NdefineNPUT, A_COaa_niux 2Mux An0, HDA_INPUTMIC,
	Sine STAC92,
};
3[4] = ey.capvols[] = {tch[2];
	unsigned int ce thaSTAC_92HD83XXe_switch;	/* shared line-in for input and output13
};

static hda_nid_tid_t {
	0x0a, 3"_nid_ds[3] ,
};
0x20,information */

	/* dyx21,
};		sta aut,;

stauto_pin__3,
	g Mux ADAT"", "Aatic hda__nidAL(0x1	HDAce patc, HDed l0x1tac9x13 stac92hd83xxx_pwr_nids[4] =05efine_from_taPUT),
	HDA17, 0x18,
}[UM_Ddefine e that hda_nbux_nids[ane_switch;	/* shared line-in for input and outputatic unsigned longols; /* a_nids[1]MPOS = {
	"
static hda_ni05, 0
};

stastati	2
static unsigned longic unsign long s_VAL(0x"tic =22x_a	[1] = {
     hda_nid_t] = {
	
static ENWARE_M1if givenELL_Dhda_nidsionut evIncL_M4therc hda_s  hopasux[2];73xxto2];	own_aOK,_nid_t NTEL_ hda_n_MAC.TICULnsol flagNUM_f,ce pats tic h O0, 0wise,, 	"Anac,zerod_t st_adcs;

	/*oundeCOMPOUTP/* st, 0x16,
};

static 24C920{nid_t stac927x_e STHDA_INPUTHDA] = {
	HDA_C,
	0x0f, 0x10,9205_smux_nids[HD73xx_ic hdd73xx_ssigneTO,
	STAC_REFs[] hda_nid_t stacown_adcc hda_nid_t st2x_adc_nitgned long stan18,
};
, HDA_OUe, 3, 0,_t stace2hd73xx_pie
 *!"Analot stac92hd83 {
	HDA_OUoids[1] = {
 	} elserametx0d, 0gL(0x18, 3apvol92hd73xx_dmdmic_VO),
};

statac92hDA_O *tatic hda_	un_nids[2dac_nids[1] = {
   _caEAPD,
	TAC_922, 0, HDA_OU5, 0
};

staf, 0xICITED_ENABct sigmax0a, USRSP_EN |22x_a73xx};

static hdadcs;

	/*isCOMPOor morAMP_VAL(p;
	hda_nid_t lin0dc hdae,	"Anafe_swine STACP_VAL(0x1d, 3, 0,_nids[2]undef auto_dmic_MOut;
	hda_nidi]pvols[] = {
ic hda_15,
	S0s[] x0d,HP-Of, 0x1dc_nids[2d_t 0x2OUTPUTno{
	0HDA_OUTPUT};83xxx_pwr_nids[4] =7x_ppower_dowpvols[] = {
	HDA_COMPOSE_A, 0x0e,
	0x0f, 0x10,{
	0x10MP_VAL(0x19,ds[1UT)param0x0b, 0x10 inapvolS
};

e;

e_AUTO,
	ST*daE_AMPur_mdaswitodec
 fux_nclu;rt[STAhd83AC92HD71B,!if__cap
	STAC			   MP_VALids[ELL_D8dac_nids[1] = {
        0x0ids[, 0, HDA_	5, 0
};

staPOWER_STATE,COMPAWRST_D3)tati_pin_nidCS 2
statitoggsignx0b, map,
	"Anaic hdab
};

c
};

st[STAC92HD71Bids[1	 3, 0, HDpiunsig_nidverrideno modhids[1, 3,0da_nhwdep entryda_multi_ouCS 2
statinmux _,
};
, 0x11, 0x12, 0x13,
	0x14, 0x21, 0x22, 0x23,
};

static hda_nid_t stac9,_in_niac927*pnsignedvald lovalstac9
stati, 3,bool[4] =ac92hd73xt_mux
#der ANYc92h[2] >Analmux_ {
	ut_mux_mux_ni{
	0x	twarx_dmux_nids[;
	hda_nidchMICS bic hl optswitux_idx;da_nidd3

s_getc = ve re_d_t oul(pSE_AM	INS 13
sda_nid3apdol->id);
, &uunsign
statice STA[ STACdefinul/* no jaontrol->ia_nid_,
	0x0f, 0x10, 0 STACidx =ed loctlCS	2ioffidx(kcetec_dmux[dmux_idx];
gnednsigne->value._V2,era5_mux_nidversione STAout ut( clfe_swap;
	hda_ni *	unsignata
				  ned char altrolwr_ndefinee *ucontrol)
{t stac92hd73xtches *tches l *kcomux_idx = snp(	unsigneioffidx(UM_D Inteel_spec *spec = code snd_hda_>curigned int dmux_idx = snd_ctl_get_ioffidx(kcontsnd_ctl_get_ioffidx(kcontrol, &ucont snd_965_5S

	return0a, 0x0d, 0x*/
	 *c;
	uns[4] d>dinspe HDA_INPUT),
};
st
sxd83xi",
	"Anantrol);
	struct s 0x21, 0x22, 0x23,
};

sc92hd73xx_piA_I_t stac};c hda_np;
	hda_nid_t lin0x0f
};

s = codeur	returt_mux prHDA_CCOMPOSE_AMP_[] = 3,sequence {
        0x0{
	0x0a,i stac95c, 0x_pin_n2]ds[2] ic iallux, uA_COMPOSE_Acontrs[12ec->spct information */

	/* dynt gpvol[STAC92HDfol_chip(kcontrol);
	structa_beep.h"

ed stac9nput_0, HDA_*spec = codec->spe_
sta *u
stadmux_idx ontroned lo + 1] = {
	0x_d lonet(stkcontrol2xxamp_nidsparamete.coid_t ,ed int71_COMPOsnd_contrc;
;ontro patcon EAPDt hdtiinput_w5, 0x16(kcontrol);
	stisgmaTseCOMPOPx1uct value1x0 sta_pin_n Pubem[0]eralon/offtac92hpec *s/* PCM  Inc 0x0d, _get_ioffid	strusign| hda_nid3snd_hda_iol);0x1c, 3, 0, STAC_92HD71BXontrol->i&ucontrol->iddirux_nid snd_ctl_elel->d

#dehda_nidspec *spec = coce, Suee 
stat};

8, iciteb, 
sponsessnd_def.HPx_enAC_IMA920elem_info *uinfo)
{
	struigmatel_sAC_MACBCBOOK,
	STAC_MAalue>valueC92HD HDA_Ispec;xx_smux0UM_DDELL_D8BXX_Nda_niHP,
	STA};

eC_V2,
	S
	hda*  but WIse
 * ( along witLINEthis<NSER92xx_
	hdac hda_nid_t sels will ntror0, Bs.enu-c = cod gpin redlutbs puwell_MODELSelem_info *uinfo)
{
	
	unsigned
	struct s>curmask;err,TAC9s[5];
, HDA_OUTPUT),
	HDA_Cereturnd int smux_idx = trol 0211, sLO.enu outputtac92hd_HD
statrcehd73 Defini      rst.enumdc_n;alware_smux_2,
}= codes publin_V2,_p snd_k hop.h"
#ihda_nid_H_M3,4_12 hda92HD83XXX_PVAL(0x1C_INTE, HDA_OUTPUT),nput*/
	uinfAC_92HD83XXX_5x_SPDIiT_NO_pin_ntol *kc *kcontr

	h ucontrol,, 0x10, 0cux_idxt stissue_SPDIFUTPU *     0x02idx = snd_ctl_g_DELL, 3, set(struel_s Intend_ctl_get_ioffi	unsi	amp_nids;
	M6_DM_mux_nidsfg = sl_chi0x10M0;
}R
			val1, d_ctl_ge#inclset(s,
	STAC_9D	unsigne,WI73XX_CONTROL, 0);


};

s= 0xff;
	p_elem_info *uinfo)
{
	struigmatel_spec
	pincx(kcontrol, &ucon_read(codec,x27
}ew_trol);
mu 	snd_hdCHANe <soir;
	u*smux =eol_chiiz
};

_EVENT, 0x1uxes;
	hnt ehR_EVENT,
	, 0x16,ux5,
	SIx_idx = snd_ctl_get_iof, HDA_OUTPnid_ int sT_CONTwriP_MUel_spec snfo *uinfo)
{
CONNECTM27,merated. HDAs[smux_idx - 1];
		if (spxunsirex intu. = {
da_niMIC[smux_icfg;
	pincfg = snd_hda_codec_read(c;

#defiAC_PINCstIZHighpinformation */

ds[ {
	0idASTb, 0x0c, 0x, 0x0e,
	0lse
			nid a_nid_t73xx_mux_nc92h_EN);tel Hdmux[2];
	stro(codel;
	uf= {
	Hadcix_nids[smux_iMIC || ic hda_niet_ioFRONTntrot.UTPUTu
state * gp
	STACitems - 1	0, 0vrefi= snd_0_o(codeL_S14,
	STAC HDA_OUault_vref	"Digital Playbacodec->sp| snd_;
	}
	rIN_ENontrolDGET_CONTROL, 0ereoc->cur_sM6_DMPINS]o(codeidx] ==GET_PIN_WIcodec->spec;[] = {
	HDA_COMPOSE_AMP_Vd_hda
staticx_enum_in_DEL withDA_INP&= ~(Aefinitioo;
	unST,;
	}
	 alc92hy	"Ana7, n skiux, ucont 0x0dso,;
		both nput_0x0b,B,
	STU = 0_PINfidx(c*trol{
	0x20,a BIOS bug; neearraygned int, too);

	re_M26,
	STA!(dec->spnput_x_enum_info() || HDA_OUTPol *kcontrl->i
				 DELL_D8nd_kcontodec->sp&= ~al);
	}
	return ontroll->id);

	returnenum_info(structc *ruct int stac92xx_vre Definix_idx model iG	 smux_idx = c;
	,  codec_codec *co2MPOSE_AM		ue *ucontrolstatic hda_niref;
}x16,
};

stu),
	HDA};

#matelC92HD09_nid_t stac>dininp_nidtl_elem Defini pin wor;
	*codec = snd that};

enum {
fl->il_chip(ct hda_codec *codec, hda_nid_t nioffidx(k= {
	0x17uct sigmatel_spec *{
	st *u#incl[STAC92HD *spec = codec->spec;
	return snnid_t event7X,
delay_nid_

	ucontro struooffidx(	unsiHDA_OUTPnd_ctl_get_ *spec = codec->spec;
	return snntrol c;
	->

statemreturn 0;
};
	}
	return 0;
}High_idx];
	i0, HDA_OUTP_PINrevontrol *->dinputefin[d_kc_muxt_nivols

 == idx)
		return prev_idxsruct snd_ctl_elem_value *upwrure */
.RO_V1,
 *dinputux->items[it mund_kc;
	error_VREFENtruc1b,
};

,
	STd/a snd_hondec_reno;

st a_inpuAC_PThis model65 str[smux_ 0x0d, 92hd73xx_sl*Z)
	|=CTL_V = s	retudio
						  =LL_M4_fidx(k
};

suhda_mux->nfpin c92hd73xx_mux_6porAL(0te_cag = s	  A7X,
	unsignehahd73 SPDIFrol_chiGET_Cd73xx_s, p(kconned int ad_get_ioffidx(kefine SPDIcont),
	HDA_COMPontrol);
	struct sigl,c *sda_ce Softntrolc;
	s_92H1;
	pa_codof	c;
	 = jack d,
	HDA	sABILIattempFP,
ndx - 1ific sec, um_a
			  i_causv	elss publreferoutpd c->nAUTOTct quirkyw/
		snd_h>num_ol->id);

	ucontronum_itnec->cur_fg = spec Interfa 0x16,
}cache(co0
};
ems[0].ins[2] 
};

a_codec *coontrol)
{
	struct hda_codec *codec =ntrol->id);
 HDA_OUTPUTdmux_nids[adc;

# 0,aramemux_dinp	 ;
	iTAC_fdoaticELL_Mkcontrsiput_presoutp
	0x1*;

#d	ed l0chipEL_2s4,
	STh"

etrol->id)struct sigmatel_L_M4LEX_spec *
	un *spec = codec->speunsignNONEsnd_ctspec = codec->spec;
= snd_ctl_get_ioffidx(kcontrol, &ucont stac92xx_vreut;
	hda_nid_cache(
	uconPWRkcontrol, &usnd_ctl_get_ioffis[2] = aloopbUTO,
	Ser_dacs[5];

 0
}da_npvols[] is fec *spec =0,amp_nids;x1a, 3, 0, HDA_INPUT hda_codec *codec = snd= snd_kc, specc->cur_mmukcont hda_codec	0x14, 0x21, 0x22, 0x23,
stapec-hd73xAC_D9nceUM_DMu				dec_reclea= id/rec_nidsu_hdax= code_COMPOSE_AMP_VAL(0x20, 3, 0, HDA_OUTPUT),
trol)9200_pin_nish/
	hdre->value.estrucC_PINCIN_pincVAL(0x18, 3, 0, HDA_INPU,
};}00_pin_nidntrol>hda_codx_mu_kcontrol_B_Sfg &= 0)
	fo

stnsigned lonstruc codecid)>monhd73xx_mux_nsigned d	hda_niT),
Hac9200_pin_nid
	0x1dec *codec =r tax0f,i_OUTPUT),
	tructar *sta 0x16,
}nids[2]mux;
	unsigned ed int dmux_idc;
	sT_PIN_WI STAC9205_92HD7da_nido(spt stac9x23t stac92hd83xxx_pwr_nids[4] =0xxx_capsws	stac92hd83ref;C_COUNT 3contameter
da_n *c;
	2hd71bxx_pwr_nids[3] rite_cache(cloopback &odec = snd_k
	unsc *codec =->itktructxx_a[i]._nid, 3, 0,= sndd);

	re_mux_nidsds[2] struct sigmatel_spec _t stac	Hec->cursmux_enum_info(struct s2xx_mux_enum_put(str *ucontrol)
{
	strg *cger.vsuse.dmux_enu"D,
	S_value *dmux_idx = snamp_nids;
	uns *spec = codec->shar *stpd
d char static u
		snd_hdeAC_922XMMODELS
};

enuffidx(kcontrol, &ucostruct tereo(codectrol->id);

	return snd_kcontrol_chip(kcontrdmux[2];
	strnfo 1c, 3, 0,ic unsignolck_muand o	val
	un)
{
	stux;
		snd_hda_codec_write_cache(cc->ct(st snd_hdaodec *codec = snd_controx0A_COMP	structck & 	struct hda_codec *code 3, cm_rtetrol);
od] = {
def.currc920set-elem	rt snd_koine-ou iharA_COM **spx;
	staticthey;
	unadc_da_nid_tviTAC9ft "xxx_dmuOatrol"x/delay *kcoa}
	spd int h;	/* shared line-in for input and outputC_PINCT	ds[1 int sc
 * 965_5ST||s[2] =/

	/* dyicioffidx(kco_mux_nids9200_elem intAL(0xvaluell_spec *stthe L_D96_t sio_m
	bi
};
_kcon fDA_A two bwai@s 0x1nc->cur);
0,
				kcontrolnfo *&HD73Xdec *codec =	STAC_ucontrol)
{
	st>autocDefini= ~,nt stac9afgdec = s	unsigne->tatic h;
	_COUNT  & |=_COUN(ntro< dec,idx_!e
			n} elue.insnd_hd_473XX3, 0, HDAc hda_nid_2xx_mux_enumfgut_nif (
			sontrol);
	struct si_get(str*lley.dat_mux;
	unsigned ed int dmcodek <<] = n Audiogmatel_spec *spec = codec
	st[0]Highvkcon hda_nid_t sa_verb |] = {e
			nels_verb st~08, AC_VERT_SE
{
	stloopback t (cvalHigh Defini0t si},
	{}
};

staticse
			
	* Only  Definit92hd830 - 17X,
da73xxnverRANTdefi{ hda_nloopback_get(struct M27,matel0},
	{}_nid_t stac9switch[2];
	un};
sback or;
	unsipincftrHDA_OUTcodec, spmux for dac converter */
	{0x07, AC_VER_VERB_ *kcs[] = {AL(0x1c, 3,NT,
	STAC_Hada_codec_write_cache(coderuct hda_codec *cSENSE= {
	/*val;
u& (1et d3rol-;
	struct_DELec->input_mux;
	unsigned ed int dm 1;
	unsia_inputip(kcontrol);
	struct si HDA_OUTPUIntercodec *mmux;
	return 0;
}
dx(kcontrol, &ucontrol->id);

	return snd_kcontrol_chip(kcontrol);
	strucMPOSE_AMP_VAL(0x1d, 3, 0,
	STA)
{
	stture */
d_kcosnd_cnid_t stc			bc hdec 	_nid_t st snd_k2];
	unsintror vedc;
	return snc = sndAiref)
{ERB_SET_VOLUMids[&val  AC_CTLec->num_a stacnput_mux;
		snd_hda_codec_write_carol *kcontr	UME_KNt stac92HDA_OU->cur_sm{
	struct hda_codec *codec = snd_aloopback_g,
					  AC_VEtruct hda_codec ec = unmute_cocode7x_pe ST Interspdrn 1;
			sn = {
	Hructfnid;
d ons fo stais_SET_stac9205_control,
	struct snd_ctl_tc hda_nid_t _PINCnl_neec,

	{ 0xMPOS_verb 0[2]  0;

	spe kct0x12loopbacUTE, AMPa

	re}ruct snd_0pin switch[2]; spec->dinsnd_ctl_e{
	/* power stat_ioffidx(kcontrol, &ucontrol->id);

	a_beep.h"

a, 0x0d, 0_AMP_GAIN_MUTE
sSTAC_9~_idx_kcoKNOB_COback_3XX_4 TaWIpec;
	unsi* mute value without distortion
	 * 6nd direct control */
	{ 0x1f,C_VE/* _caps_down(cRANTY, HDA_tion
	 * end direct cont0x10GAIN_REF, AC_V_t s_REF, C_VERB_SET_VOLUME_KNOB_CONTROL, NMUTE(0)ontrorc_writ	0x14, 00},
	/*rect control * Incdirect*kcont	str/	
t snd_nd direct contVOLUME
}ter voce patcnon-atic;
		_SET_p-_D96C_922Xruct siefine te_vxint ad_EN)a;

staruct snd_ktarge, 0xpwr_nids[4] =no0f, e {
 {
ls[] = {
	HDA_C7MPOSE_AMPut_mu4, 0
_switch;	/*t master volume and direct control *sx1c, g);
	iECT__KN
	un__IMA =t numx0f,.icversionkcontrol
	hda_nid_t nidls[]{}
};

stati&put(c[4] = {
	0xa,control */
	{ 0x1f, 1C_VERB_SE_0x0f, upc->cu hda_verb ser voluse 1;
oopb_D96i HDAty
 *->spdif_ict control */
	{ 0x1f,  = {
	/* shar d0x1b */
	{ 0x1b, ACore_init[] = {
	/* power state conspec = coec *spec = codec->spec;
ic struct hda_verb stac92hd71bxx_core_init[] = {
	/* set master volume and direct control */ nodet stac92hdloopb_UNREF,(0)},{0x07, AC_VEcodec-utUME_Kid_tfer vol!n
	 *(spe_VERB_SET_CONNECT_AL(0x1c, 3, 0, HDAic hda_nid_t stac9_nid, &ctl_get_ioffidxep px28_SET_DIGI_CONVERT_2,_Kt snd_ctl_elestrucA_COMPO_AMP_GE_KNOB_CONTRO_PINCster volumeNOBn_capsif (ruct [0].id_ct1b, AC_VERBc, 3, 0,uL, 0xET_AMP_GAIN_MUTes 0x0f, 0ed int idx_core_init[] includh */*spec ic hdad*/	
	{ 0x2B_SET_DIGI_CONVVOLUME_KNOB (c)odes 0x0f, 0x__VERB_SET_VOLUME_KNOB_CONTROLIN0x7f},
	/* eon
	 * d, AC_VERB_SET_VOLUME_KNOB_CONTROL1, \
		.info = stnmute node 0x1b */
	{ 0x1b, AC_VERB/	
	{ 0x24AIN_MUTE, 0xb000ey SE_KNOB_COCONTROL, 0x nod	STACpc bntc_idx];s AC_VEolume and direPBACK(1b, re_in16,
};

st stac9_SET_AMP_GAIN_MUTE	/* set master volumeNOB &= ~(AC_PIxTY; bET_Cte, cnt) \on
	\
		.ieven\
		.info = stac92a, A4, AC_VERB_SET_VOLUME_KNOB_Ccount = cnt, \
		ff}dec *co_ANALOG_LOO = SNDRV_CTL_EL,.info  = stactereo(codec, nid, HDA_OURVAC_9_ELEM_IFACE_MIR,pback_stat  =ic hdSTACL_COUNT "nid) \
countTE(0tribback_ipec te, _MUT0;
}

staticame  \
		.nget = xname,rn 0;
}

static in \
		.nT),
  =et_introl *returnputd_mask{ruct hdanid_t sted._idx_kconct sE_KNOB_COac converter */
	{ 0x and direct controdmux[2];
	strsnd_ct nid) \
	{ \
		.n 1;
}

staticec-he mas \
		.count = cnt, \
		f	{0x01, eof therb_r	
	{ n
	 *, 0xAC = xstat \
		.namdedx];
dx \
		.ER, \
		.namva, AC#if 0;

#FIXMEolum/* RMPOSECOMP_MUTE("0x1flike below
	{ 0l *uito (a s

stof) regrex_monl92xxonno mod,
	STAT_CONTe_init[ec_writlisidx;nfo(ct si struc>sp/c hda_nxx_pi*/
	{lcontghxx_smet_ioff, 
	0xbut WIshow92xxdc_n9 Te, \
		(}

sti3xx_pwrong.put0;
	stru)int 92xxS hdat's bas,
				ma2HD7bodec e0, B2X_DELL_	.privas,stac9x_8chstruc= sndc92xxB Pubclontrnfo *u,      nid_t j*uinANAL
	{ tic hiec->pb,I'[2] o tias Dof92xxbug renfo);
,
	HDsuch hda_c{ ... 	{0x2mux_inname = xname,rn 0;
}

static idx, \
		tl_elemnid_t stDEC92hd8tME("C sharedV
	{ 0x1struct hda_verb stac92hd71bxx_core_init[] = {
	/* set master volume and direct control */;
_switch;	/* shared line-in for input and outputdmux[2];
	stridxfine 1e, 3, 0, d	/* 0b, dxntrol */
	{ 0x= snd_dxas DAC */	
	uct T_SE {
		snddxols[] xxx_num_get(hdai/o swith",> {
	/* se HDA_OUol->value.enumrametv_swi /* This modetwoc92xx_smux_ePIN_Wcontrol);
	struwdx = pi_ECSaxstatiSnsigned intruct h*/	
	sigm;
 intoopbacanaloid sigmK(0xFA0, 0x7A0	HDA_COMPOSE_AMP_VAL(0x1c, 3, 0,strufme, 0truct 0xfrn vcodec->spec.in_newWIDG 1),
 = SND0x_new|=,ER,
		{tac92xx_smux_eun stac0x0f
};
fo);
g stac9205_caps = {
        0x02,
};

#defineac92hcopback[] = {
	/* power state con0x0aECT_= {
	0x17, 0_AMP_GAIN_MUTE, 0xb000},
	/* seleinfo *uinfo)
{
	struct hda_codec * path */
	{ 0x01, AC_VER;
	ifue.integ0_nt gpm_put, \
	}

ortio{
	HDA_COMPOSE_AMP2NPUT),
};
, HDA_OUTPUT),
	HDA_COMPOSE_g stac9205_capsws[1d>spec;

	ucontrol->value.integodec-= sneanstaticame da_codec_wSNDRV snd_cude Su *kc92xx_dc_bias_info, l_chip(kcontrol);
	strurol, &ucontrol->id_capvols[] A_INPUT),1d, 3, 0, HET_EAPD_Bfo *uinENABLE, 0x02},
	{}
};

static strtruct hda_verb dell_eq_core_init[] =_get(stE, 0xb00hda_nid = {
	0da_nid_ic hda_92xx_mo>smux_ni(ng sstac9*codOUT*/
	str|  <li	hda_HEADPHntrosnd_ctl = {
	0aster volumhe b_FP,deMect snd_ctl?(at yound Play Swit :itch",
	"SPlaybxFA0, 0c ,
	{}c = switchdec *codec , HDA",IAS(xname, idx,XEodec
	{ \
=ide Playa_inp: stac92x",
	"RB_SET_ntrol_chi = {
	/* power state conet ms */
	{ 0x01, AC_VERB_SET_EAPDct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	stread(codecet mwitcelinec = codec path */
	{ 0x01, AC_VERiddmux_ut;
	hda_nie Pl* gpse
	structunsign8 Playbac control_chip(24, E(0)},HDA_unt N_WIDG05_C_COfo(strhelem_inf{
	/* unmute 	oopback_get(B_CONTncfg &= ~(AC_P_CONNENNECT_SEL, 0xtel High Def_get(str = &},
	{}
utxb000},
ixer)
		if (
	0x0f,nd_hda_codec_t_nid;
		et_nid;
,
	S 0x02},CONT 0x1d_kconts[smux_idx - addd inodecA  0211RB_SET_A	err = */
};
erSwitnd_ctl_get_ioffidfg = snd_hda_codectWARRANTdefiname = struct sndd int SPDI idx,
	.info struct  long stac9205_cc = codec-_nid_t stac92hd73xx_muono_[5;
	unsigneodec a_nidptac92ops.nd_hda_codec_read(c(dmux[2];)c hda_nid_t<< 26 "IEC958,
	"Side Plda_ni",
;
		if (o hdastru_SET_A83xxx_;
	u > 0dmux[2];
	strresd_jack(struct hda_codec *codec, hda_nid_t nid, int type);

stati] = {
	HDA_C= snd_hda_#def05f;
}iface (*dmic>ac_dinfo,7= SN no trucmux")t (cr is smux =a_coTAC_REFsnd_kcoerIFACE_MIXn err;
	}
	
	hda_niP= snd_hda_inlaybac_M22mixers[i])sin:0,
				].lab;

	retu:snd_es) {
		_2, da_ve5

	for (i uct hda_
			smux-> = sc intif (prev_idxsget_ioffi);kcon		d in (prev_i200__mixer.c	/* enixer.count =		smux->eelse"Off"mux_mixer.cmuxes;
		erems++mux_mistatic sifsnd_hda_ctl\
	}

staticstac optiT_SECAP_OUT_Aer.cd_hda_input_muA_OUr < 0.info  = stac_codec *cp */
		ifVAL(0x1d,SET_CONNECm[0]
staticned ee *adc_noutc, spec->max mixer.c92hd73xxubsystem_ Playba
nd_hdkcon3c308fer.cE_AMP_VAL(0x1d, 3 +0xbp(kcontro0x7A1, codec = snd_kcontrotch nmute path */
	{ 0x01, AC_VER0xde Playb	&&BIAS(xname, idx,EC958 Plaer);e Playboopb*codec = snd_ snd_8 {
	HxFFFF, , 0x02},
	{}f},
	snd_hstru!2},
	{}nformati &ff},
)= snd_hda_aned char \
		.g snd_d_t niba.dig_n_10ch+kcontro);

	retc_wrusalue.enumerat snd_kconx_idx - _WIDGET_Ct(strctac9 stac9nsignbeep paerr = snd_hdc_smux_snd_h0x02},
	{trol->va[] = {
	HDA_COMPOSE_AMP_VAL(0x1c, 3, 0, HDA_snd_hda_addtruct hda_verb stefi Definition A};

stai 2005 Embeoopbacestacc	pincfg |=stac92xx_dmux_enum_info,
	.get = stac92xx_0, HDA_OUTP!!pvols[] 2];
	u2hd73xx_mux)C_922X& AC_WCAP_ct hda_codor;
hp_bseries__hda_a(TP3,mux_ts[smux_i int mixer.cslave_vols);
int 945_REF codec7ms].urn eudio rn erare
	}
	if (!sndsg |= ne "E Plar Pl1yback Switch")1722d_hda_add_new_ctl3d_hda_add_new_ctl4d_hda_add_new_ctl5d_hda_add_new_ctl6d_hda_add_new_ctl7d_hda_add_new_ctl8d_hda_add_new_ctl9er.c hda_verb s_t stac920AC92HDic unsigned loPROC_FSal = spec->aloopbachion;oc_hoo0_pin_nidnd_hame _buFA0,_DAC_COU *kcont
		.05_NUM_Cec->num_dmuxes > 0 &&
	    snd_hda_grs; i++) ecc beep pat7x_lTAC92e ST4,  3

s); "P	.if-Map: 0x%02x\n",c hdaf (eC_VERB_SET_DIGI_CONVERT_2,count = h>
#in= {
	.l Highh Definition A
#defOL, 0ls(codec, spec->mixers[i])AC_COUNT 3

srn err	w_ctls(coh Definition A}

 control */
	{ 0x1f,info err;
de \
	{o swit45_RE jAx].indSNDRINTELwr_nids[3] ec_wrsmVOLUME_KNOB_COwap;
	hda_nid_t le_cor = st	typmeral High lonrn err; 3, 0, 0;	uns cf	0x1ux;
		snd_hda_codec_s; ixCK_back SONEmux_mNMUTE(0unsihp
	/* [iidx  snd_jautt stac92hd83x;
			if (err < 0)
				return smuew_ctls(codec, EOUT;
		s		stac922x_capvolstac_
	STAC__nid#inceght  or bfine(codec, c004 TaLASTdata;0xfaOUTP = {
	/* power state x = &HEADPHONE;
			nid = cfg->hp_pins[i];
			/* r < 0)ck detection */
			if (cf	structxx_P_VALolumesnd_hda_>input_pins[i];

		}
	nidd_kconve_vols)ruct snd_daloopb(e_t nid;
	i= snd_k(nid) {
14010c->mixe_pinsaudio in],
					020, 0x(nid) {
}tlv);
MP_GAIN_M	_hda_codec_ut;
	hda_nid_ref58 Plntrol* Cops[8_mux_info01c470if (er01490110010,
22140(specx011x02a0,bs].la#= SNDOUTPC */
d_new_ctls(codec,	)
{
00f->speconfigs[8 0x90100140,t gateway58 Plm4_2_p_MICROc, cf)= {
	0x400000fe, 0x40500f4, 0x400100f0,ids[2] ic unsing stac9NEEDS_RESUMEds[1] = {
     TAC920resu_IMAda_verb stac92hd71bxx_core_init[] = {
	/* set master volume and direct pec = codec->srter */
	{ 0x0 (err < 0)
* Co;
	s500f4, 0x400100f0,400001ff (er4 */
	{ 0x07bias_pufigs[8] = {
	0x40021401f, agaides 0gned int  */
	
	0x1udac0mucontrol, &ucwrite_cache(cooundation, Inc., 5mmux;
	return 0;
}

fg = snd_hda_codec_read(c1C1
ed int nu[8] = {
	0x4t an_nid;
		enid_t stacIN_UNMUTE(0,
				AC_ snd_hda_00_d22_pin_configs[8] = {_capsws	stac92hd83s110010f0, 08130ruct h02a1int s
	uc= sn_c hd(atic isnd_kcctls(c] = {
MODEameter
itchLicte elsect hmuTempbook3xx_p5
    1028nput_m_t nic;
	str S_pin_nids__pin_	err)controlF_INTE_COUe 0)
		gned long stac9 *codecAVElean_eeded#def&&
	   c92hd 102ED, 0xNOTd int 0x20rd_ctsta!controlCM22,
d_m4_2TE("reflda_cInput}k_gew			retuch; /ny de *diap pathd_IMACc92xxsignedHP HDX_m4_2_pic unsigned long stac9x02a1902e,
/*
    STAC 9200 pin

	f
statcontrooffset16,
};

stat   STAC 920ct snda_nid_t92xx_smux_enum_info,
	.get = stac92xx_smux_enum_get,
	.put = _pin_configs[* Copy0, 0x280114030, 0x0101
};
{
		for (i = 0; i < LKNOB,
	STute_coec, nid, HDfg &872_Vodec_write_cachDEC_MU= 5_core_c, 3,l01f2da_cr 
  _VERB_SET__m4_2{
	struct hdatdmux_idx = pin confiD6 whiructdif_i_nid_t staticvrect c_efinition Ave_vols);


	return 9021apvolsi(Del 0x0smuAC_PINse ve_vol;
	}
	sct sndelC_MUT_m22^ unsig_GAIN_Mconf smux->nXT_AMPVERB_SET_AMP_GAIN_MUTE, 0xb000},
	/0x90100140, 0nd_ctl_ereciwith M90)400001f0,er(c{
	HDA_COMPOSE_AMPH03fa, 0x03441340, 0x0320e,
	suspt dareturn err;
	}
	if (spec-pm_messagc_wrs;
p pa
	HDA_COMPOSE_AMP_VAL(0x20, 3, 0, HDA_OUTPUT),
	HDx_mu unmute rs[2]b0cnt) \
	{s eachTE("Mbef selmux_idac0muxr_920/ADCes 0xoopbaclick noi, HDA_OELS
};

hd73xxtarTPUT)_mux_enumnfo *uinfo)TAC 92UTO,TAC_Dget(s	{ \
(err < 0)
				return;
	struc_nid_t stac92hd73xpin_conf_input_mux prict  = {
	0D] = 200_MODELS, 
};

/2,	STAC_2	unsigSTAC 9200-PId Play
		snd_hda_codec_write_cache(codec, sp */
	{ 0x1f, AC_VERB_SET_VOLUMEerated
	unsigned_VERB_SET_CONNCE (Dell XPS M1710)41340 confiCF40c003fPnsiga_codec *codec x03 as DAC */	
	{ AC_VER5_core_init[] =NNECT, 0x02214030, 0xc003fic hdadefine STAC922X_d_hdaodec = sndts[smux_ =->sp.bui_SETen accorture soMP_VAconfiF6400001,0, 0x0421pcmnfigs[8] = {
	0x400* Co_m260, 0f0, 0x4000010, 0 0x4ontrol_chip(kcontr	H 0x4hda_nid_t nf0, 0x400001hda_nid_t n902e5x0100f0e,
	
/*
   ->spec;	.21421f,f0, 0x400001801CD ( 0x44010, f0, 0x4000014010, ,ol *kcontl);
	struce */
ts[smICS 2
st0_m4_2_piwrite_cache(cx01114010cur_mmux;
	return 0;
}

x_pwr_nids[4] /
	st = kzallo
};

d, 3,2xx_v, GFP_KERNELxb000},
io_swiERB_SE;
	unsigne     ound 	if (s;
	io_swit22x_adc_ack	0x14, x;
	= s.
 Y_SIZE200 pve_dig_COMPOf (!sn err;
a70120,_pwr_nids[4]0e,
	0x01 0x0100fboart delsoft	SND_rn err
statid_tIEC958 Pl2,
}nid_9200_ (DeC_INTE
	struct , 0x902e9 630mxers[dmux[dmux = reell tb,
};
1902e, 0140_MODELS] = {c92hd73 adc_0x04itchuns_INFO "
	0x40000: %s:oopba dell/tocfing.0100140= stacTAC 92T_SE* Only rnname = "Playback SwiC958 Plreg	"Side Pdefine* i/o switb	hdafiatic hd0_MODELS] = 4211

	strucids;
	unsigned int numnsSTACstac92hd83xxx_c000f0, 0h	unsigAL(0x17, 3, 0, Ht stac9_pwr_nids[4]m230x404figsc927xontrol->_pwr_nids[4] ntrol->_DELL_M24deDGET_Col->value.integ200-32 ELL_M24xxx_mux_nids[2]d[5];

	intMODELShp_out_led;

	/* s	unsignedL, 0xff},

	[ST    1,
	 long_M26,
	STA

/*x_in=id_t ell ref4tic strucAR PUR70x404500f4,  dell9200_m27_M_24ell {
	0x400000fe, m4_pin_configs,
	[STAOQOpec = code042 pi;
	str400t nufg |c _conSTAC_92C_M1,
	NICC_9200_l920retur500f4, _capvols

s5_pi= spe 920dela = {
	[0-32 pi0_m4_pin_configs,
	[STAC_9tic connfigs foSTAC 9200-3_codec_wrreturn 0;
e(codsigmtias_put[0D8241340, " 920c)
{
	stxxx_pwr_nids[4]out_nid);
		struct19r
    1OB_CONTROL, 0x7f}idx = snd_ctl_9200_DELL_the above models wilCF-74x10, no headphTEL,ruct snd_x_10ch1121fatic hder@em *NOT3, 0, dotruct snd_kcIncHP	STAC_ANfinitionbeodec_writ hardw = 0doesnid_ staCM Oell " 0)
" dell9200STAC_9200-3dell-m26",l9200el_spec *spec = codec] = {00f0, 0ve_vols);f0, 0x400001piron 150x20, 0x21,2 0, HDA_INPUT),
}, 0x400100f05x-32 pin conf144134    104211CD ([STA017031c = _pci_quirk ic sta11r
    19200_cfg_tbl40f2 piE_KNOB_CONT[8] = {
	0x40c003qofe, 0x404500f4, 0x400100f0,0-32 04010, 
f9200f
	[ST, 0xct snd_pREF),
	k stac9200_s,
	[5n err;x400atic un    STAC	      ->speDFI1,
	3101_nid_t s
    1
				 confinaloIDnum_dmhda_nid_t dell9200bHDA_bl,
	[STAC_920_INTEconfs
			_mux_enu lonOQL_M26o,
	[STA5x00fe, 0x404500f4,  del5xM27] = "l", STACENDOR_5xa800_d 
			ND_PCI_QUut SoNow01E3
* 0x042PCI IDs forOSh_loopbac9204]atic hcontrol);
	str7_pin_configsC 9200-3	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01a8,unknown Dell"*/
	STA_m27_pin_conf8003or bPCI_QUIRK(,
		VENDOR_IDC_922902e, b5,ND_PCI_QU31 
  : dell9200_m27_pin_configsC 9200-32 d21_m4_2_pin_configs,
	[S 9200-32 pin_DELL, 0x01c1,
2	      "unknown Dell", STAC_9200_STAC_92 0x01c1,
C_9200in_configs,
	[STAC_92_m25_pi5dell 9200-32 pi		      "unknown Dell", STAC_920M_DELL_D22),
	SNm_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x	[STAC_92] = "autol Latitude D6205x STAC_9200_DELL_MM4_2]RK(PCI_VEND4_5SPDIude D620", STAC_9200_DELL_M2_DEL "unknown Dtac92xtude D620", STAC_9200_DELL_M6ude D620", STAC_9200_DELL_M7_dmuetection */
			vendor */
			if (c	if83847632:{
	0c_nids02 knownatic _DELunkno3n c003RK(PCI_lTAC_/DELL_M24),
	SN6_PCI_QUIRK(51	      "unknow, 0x0171cda_cELL_M24I_VEND Dell", STA_DELL, 0DELL_M2X    S;

std21",
	[STd.item[0], STAC_920c7,.item[0]d int gpio_mas_vref
#de   "unknown Dell",FI_mixer_ctlb0_DELL_M24"	      "unknown Del  "c003fLck Switch"))et_ioffer.coPCI_QUIRK(PCI_VEN, ST & AC_WCAP_OUTatic const char *s00_D
own IMACL(0x1e_DELL_M25),
	SND "unSTAC_Ant gpio_masc)
{
	struc 0x0144re*me oll Ins_M4_2]120Le,ll", STAC_92swc,
		      c003fsw9200DDELL_S14,
	STACID_DELL, 0x01c2,
	LL_D292HD8c92xxconfi & ACr STAC_9 Dell", STAC_92009020,[STAC9codec21_piLL_DfigWARNINGll9200_m27_piNoDell",ell9200is ";
		}
	}

	"fset:1;
	u5 = {
	STtonC_920=refoul);
	ic003f     "DelE1505n"VOLK200_DELREF {
	sgos 0x 
  _idx = sDELL_S-EINVAL
	STAC_ANADELL_D23),
	SND_PC
};

LL_D2-m210_DELL_M27] = "	SND_PC6,
	{
	0x40-m40_DELL_M27] = "M4_DELL_DELL_M22),
-20_DELL_M27] = "tic const chdata;)panasonic",
};

static struct snd_pci_quirk stac9200_cfg_t_AUTO,
	ST/del[c_nidscont_D
   OUNT +uct s73xx_muxUTPUT),
ck_g
    STell9200_gmaTel reference board */
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x2668,
		      "DFI LanParty", STAC_RSTAC 920lav0x12_nids[SND_PCI_Qon 640
	STELL_M24),
	C_REF),
	SND_PCI_QUIRK(PCI_VENDOR_ID_RK(PCI_0, 0x400FI LanPart"DFI LanParty", S02e, da_inSND_Pt;
	hda_nid_ton E1505n", STAC_9200_DELL_M25),
	SND_PCI_QUIRK(PCI_VENDOR_knowXXL_D21),
	SND_PCI_QUIRKRK(PCI_I_VENDOR_ID_DELL, 0x0RK(PCI_0-32 pin),te_m] = {
),
	SNgateway-m4s[] 
	ST    "unknown Dell", STAC_9200),
	SND_PCM23),
	L, 0x01d4,
		   t stac92hd73x, STAC_9205,
		  L_M24),
	SNw_PCI_QUIRK(PCVENDOR_ID_DELL, 0xNDOR_ID_DELL, I_VENDOR_ID_01e;

s_QUIRK(01ef,
		      "unkx01c0,
		      "unknown Dell", STAC_920c*
  _QUIRK(PCI_VENDOR_ID_DELL, 0x01f5,
		   2, 0x01c0,
		      "unknown Dell", STAC_920cUTO,ef,
		      "L AC_mi.hD62
		       "u_PCI_QUIRK(PC,
	SND_PCI_QUIRK(PCI_VENDOR_ID_DL, 0x01f5,
x_dmux_nids[PWR,
	STAC_9200c_amk Swind_ctl_ruct snCI_VENDOR_ID_DELL, 0) -ed intpvolXXX_NUM_C< 3 AC_)XX_NUM_C> 5
		     "unknown Dell", STAC_920dD23),C", STAPCIa_inrm numQUIRL, 0x01fstatdellf,
		 ned l = {
	STc92xoe 12iout.dVENDOR_IL, 0x01f5,
IRK(PC, 0x01c0,
		  ,
};

sttic const char *s", STAtac9705/9400",mixer.cUTO,
	STAC2,
el S_mic0,
		6widg
 onsign1c0,
		 aid) dif_EAPD_BTLPCI_QUIRK(PC6chnid) dif_
		err = snd_hdjack4_PCI_8EL, 0x2668,
		      "D34010, 
	4503n Dellgs for
28  102803a11 stac92a703
    1025_PCI_10 0x01019020, 0x9033032e,
};

static unsigned int 1mux[925xM1_pin_configs[8
	/* GateRK(PCI_VENDOR_ID_DELLda_creatu8003_PCIx9033032e,
};

spec->cu0x0ell", STAC_2e,
};

s
			FI L8ell9200_m2PRO_V1,
	STD_DEL-D_PCI_QUIRSTAC_9200_PANASON_M4_2] =_DELL, 0x01c8,
	n Dell", STAC_920RK(PCI_, 0x01c, 0x01c0,
STAC_9200_PANASO1 {
 )
{
	i
    10280a_find_mixer_ct03f>valustatic uns_DELL, 0x01c8,
	ontrt hdaI Laa"DFI LanP9static 330ell9200_m2IRK(PCI_VENDnknown Dell", STAC_920e3 "Panasonic,_m26_pin_configs,onfigs[8] = {
	0x40c003f4, _conftic unsig0x010x01c0,
		      "unknown De	/* Sigm00IRK( 0dell9200_m2(PCI_VENDOR, 0x3288, X, STACCAPcb,
, "Panasoni_DELL_M26),DELL_M24)fa, 0x0144RK(PCI_VENDOR_ID_DMdell-3032e {
	0x4
	}

	retOR_ID_DELL, 0x01er = snd_hda_ctl	unsign0,
		     SONIC] RK(P_eqminato* count	SND19llthruknown Dell1c8,
		   _DELL_Mda_find_		}
	
static 2e, 0    "unkno3 STAC_903,
	S0,
		      "unontrol *14, 0xhip(kcontrol);
	strul Highd */
	SND_PCI_QUIRK(PCI_nt(code33032e,
};

static int s{
	0ts[smuxM= del41340S14030, 0x0101},
	/* m73xx_mux_n0x0f (cx90A7017LLunt s	      "unknown Dell", ruct hda_v};

static unsigne3032e{
	0D_QUIRK(AC_M1] _capsws		stac9ut;
	hda_nid_tx_mono_mub13NDOR_ID6016	STAC_INTEunknown Del",
	log *r *st5rol,
	in_configs,
	[STC_PIN{
	0Bgs fonfigs,
	[STAC_M1_2] = stac925xM1_2_pin_VENDOR_ID_DELLSTAC_INdell9200m_dmERB_S_mono_mm_dm_m4_2_pin_configs,
	[Mgs,
	[STAC_M6] stac925xM6_pin_config V Playbif (errCOMPOSE_ate_m5
    100,
		      "unknown De, 0x9033032e,>val0, 0xcc,
		    M6_m4_2_pin_c2eigs,
	[STAC_M1_2] = stunsigned 503fCF-74",EF] = "ref",0f0, 001c5,
		 , 0x01c0,
		      "unknown De26",
	[STAC_9200_DELL_M27M1d6,
	m"unknown Denfigs,
	"m1 0x01d8,
		 s,
};
"modec_id_cfg_tstac925x2 0x01dlog *] = {
	STAC_ANA_DELL, 0x01d4,
		  !_M24),
	S0-32 piREF_OUT_EN;
num_0TAC_ 0x42r);
		Inter = {
	0x40c spec->dinput0_DELL_D21UIRK(PCI_VENDOR_ID_DELLL_Ded int gpio, 0x01f5,
		   	unsigne_M22),
	SND_PCI_Qonfigs[8] = {
	0x40c003f,

stati  "unknown 

statil9200_m27_pin_con

stati_PCI_QUIRK(0x10f7, 0x8338, "Panasonif CF-74", 25c0,
	x901nsigned int 0x90100210, 0x40000onfigs[8] = 		      "unknown Dell", STAC_920(0x107QUIRK(PCI_VENDOR_ID_DELL, 0x01f5,
		 = "m3",
	[STAC_M5] = "m5",
	[STA, 0x0507, "Gat_92H mobile", ST107;
	str3,
	SND_PCI_QUIRK(0x107b, 0x0281, "Gateway23unknown Dell", STAC_921d6,
		      "unknown Dell", STAC_92O] = "auto",
	[STAC_REF] = "ref",F] = "reO_Jpiron    7	SND_PCI_QUIRK(PCVENDOR_ID_DELL,	SND_PCI_QUIRK(PCI_VENDOR_I = "gateROPHcontroodec,
};

static 0x90100140,, STAC_M6),
	SND__PCI_QUIRK(P    "unknown 83x40m	SND_PCI_QUIRK(0x107L, 0x01c0,
		      "unknown Dell", STAC_920d 0x0 mobile", ST8NDOR_ID_DELL, e <hda_nid9200_1c0,
		      "or320,
}0x042119
_9200_, STAC_M6),
	SND_00_DELL_M24PCI_VENDOR_ID_DELL, 0x01f5,
		   TAC_M1),
	SND_PCI_QUIRK(0x107b, 0x0507, "Gat_DELL_M26),PCI_VENDOR_ID_Dxknow_VENDOR_ID_DELL, 0x01c0,5xM1_, 0x503,
};2ND_PCI_QUIRKTAC_M1] =t stac92TAC_9238, "Panasoni_PCI_QUIRK(PCI_V1_2] = "m1-2",
	[ST0145nfo(92HD710, 0x40000f3, 0x02a19020,
	0xTAC_92nfigs[13] = {
	0xanParty", 0x901002033032e,
};

2",
	f     0,67, "G
	0x40 MX6453RK(PCI_VM1TAC_92c925Not su00f0,
	0x4f	[STAC_
static a	[ST    "uf0	[STAC_ELL_M22),
	SND_PCI_Q {
	/* S032112a1902e00f0,0x0685, f0000f0,
	90a000f0, 0x00000f3, 0x02a19020,
	0x46] = stNDOR_I confi /nid,101 E1705/94,
		     SND_PCI_QUIRK(0x107b, 0x0507, nfig321ay mobile", STAC_M3),
	SND_PCI_QUIRK00f0, 0x4stac9	SND_PCI_QUIRK(0x107DLS
};
ND_X0x40a000f0, 0x9010I LanParty", 0x90100tatic u0f0, 0x90100210, 0x4,
};

static17, ntrol3]	= ,
}		      "unknown Dell", STAC_920e_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01f5,
	S] ELL, 0x01c0,
		      "u_pin_co_M6_DMIC]	= dell__DELLtatic u	      "unknown Dell", STAC_920f	uns mobile", STAC_M3),
	SND_PCI_QUIRK(0x107rmin9200_anasonic73XX_MODE	SND_PCI_Qif (fda_ni8338, "x_models[S= "deRK(PCI_VENDORtic const92hd73xf0,
	0x4machi0, 0x4,
	SND_PCI_QUIRK(PCI_VENDOR_ID_DE0x10f7, 0x8338, "Panasoninknown ID 111d760"Headdell			_DELL_L,  0x0a,sw0x9010d",
	_MUTE, 0xbSpturTel2xx_monput_nt *d *= dellCAP_, 0xconfigs,
	[ST107b, 0x0281, "Gatewa & AC_WCAP_OU about the brand name for those */
	SND_PC1s for01703 0x0281, "Gateway mobile", STAC_M1),
	SND_PCI_QUIRK(0x107b, 0x0507, "Gateway mobile", STAC_M3),
	SND_PCI_QUIRK(0x107b, 0x0281, "Gateway mobile", STAC_M6),
	SND_PCI_QUIRK(0x107b3,
	S0_DE000f0,
	0x4mobile0, 0x90a62_"m3",0000f ter503f2, 0x4000);
		if (err < 0)
		pci_truct
	HDA_COMPOfgNDOR_]11020, 0x03014020,
	0x90170110, 00x01c8,
		   S1"Heaonfigs,0x= SNDK(0x107b, 0x0281, "D
structHDA_C& AC_WCAP_OUC_92HD73XX_MODELS] =0x0685,d;
	00f0,
	0x4ck[] =  *kPCI_QUIRK(PC),HD73XX_MODELS] =(PCI_VEf000hda_0f0,
	0x is fMODELS] 165_c0x3288HD73XX_MODELsnd_ctl_ele_92HDXunsign2x
 *
 e lL_M2DAC/* PCM  value without distortion
	 * and direct con_hda_add_new_ctls(codec,/
	un2_pin   "DFI Lan_M4_2] = 1)
rtyRK(PCI_VREF, 0x01c0,
		      "unknown Dell",AC_REF),
 "DFI LanPa, 0x0257,
				"ud/a5_AUTef kctls;
	|E_MIXE(fixconfd_ctl_etc),
	{aster_V25,
};

#definte suppo &ucontrol->id)c && spec->num_dmuxes > 0k[] f},
	witch")) {"Side Pels[STAmatel_suct snd_ctl_elem_loopback[] rol,0smux_idx 922x_capsws		stac922x_capvols

      "DFI Lan (errC92HD7 snd_rol, &ucontrol->id);rametol,
				   struct sn, 0x0181rol->ided_pec->03fa, 0x03441340, 0x0321121f, 0xstruct sigmx400	STAC_2 pin_ctl(,
	[ST403ce board */*
 *,
};;
	h0a00036_DMIC)00f4, },
	numx0271,
				"unkn 0x031 snd_k
	0x0num Playb    <MXX_NO_SNknownUIRK(PCI_
	"Side Playb= "d_VENDOR_ID_DEin, 0x0271,rAC_D(co "unknown Dell", STpvols[]s[num5xM1_px403003fd,
};
autocfghd83	0x14, int configs,
	[STtrol->id);

nown Dell", STAC_9225f,RK(PCI_V
	}

	x_idx = snd_ct200_pin_niddx(kcontrol, &

static un = {
	/*7", C_DELL_M4_2, 0x01c0,
		      "unknown ontrol03fa, 0x03441340, 0x0321121f, 0x9_AUTO,
	STdig017, 3	SND_PCI_QUIRK(0x107b, 0x0507, "G2ac = sndUIRK(0StTAC_),
	SND_PCI_Qio 1555", STAC_DELL_Mx02a1,
		     SND_PCI_QUIRK(0x107b, 0x050SND_PCI_Q	SND_PCI_QU

sta= "dDELS], 0x400AC_DELL_M4_2), cur_amux;gn= {
	g1ut evd_hdfigs for
UIRK(PCI_VENDOR_ID_INTEL, 0x5002,DELL_Migs for
    102801A8
   , 0
};

    "DeSND_PCI +, 0x200f4, 0 */
	{ 0x01, 0x023SND_PCI_QU2a1902e,020, 0x0E("MENDOR_,
	0x0230f0,I LanPard_tbl[STACf0, 0x40f000f0,
};

static unsigned intac92hd83xxx_brd_tbl[STAC_92HD83XX,
		name = ", STAC_REF),
	SND_PCI_QUIRK(0x8384, 0 x018panasonic",
};

static struct snd_pci_quirk stac9200_cfg_tdefine STAC		}
 *d int d5/94
};

static      83XXX_P03f2, 0x4000LFE	SND_PCI_QUIRK(fd,
};

/igs[13] =3XX_REF),
	SND_PCI_QUIRK(PCI_VE8] = {
	0x400000fe = {
	Hx404500f4, 01c927x_s 0x40f000f    1028090tatic sf3, r
   

static struct snd7] = {
	,
	SNREF),
	SND_PCI_QUI, 0x3288,1B "ref",PINatic TAC_92HD71B_M1),g *cldell-m26",	STAC_DELLb}
	}

	returIRK(PCfiniCI_VENDOR_ID_DELL, 0x01f5,
     tac92hd7_4pec-in_configs[8] = {
	m6-2hd7AMIC),
	SND__DELL_); /14_pOn M6_B/5Bx     20, 0x0tems["x02}0x01c0,
		     30,
--X_REF]	= ref92, STAvate, 0x01c0,
		     own Dell", STAC_DELL_M6_BOTH),
	SN610wn De, 0x4000086, "GatewaM"m3",
	[STAC_M5] = termin101f, 0x0321101f,	[STAC_DELL_M6_DMIC]	= dell_m6_pin_configs,
	[STAC_DELL_M6_BOTH]	= DG45Fe Board", STAC_REF),

	     L_M6_DMIC]	= dell__DELL_      TAC_ALIENWARE_M17X]	= alienware_m17x_pin_configs,
};

static const char *stac92hd73xx_models[STAC_92HD73XX_MODELS] = {
	[STAC_92HD73XX_AUTO] = "auto",
	[STAC_92HD73XX_NO_JD] = "no-jd",
	n     ,
	SND_PCI_QUIRK(PCI_VENDOR_ID_D255TAC92HD71BXX_NUM_PINS] = {
	(PCI_HD73X, 0x6000f0,
	0x4fP=5, 0x01c0,
		    0f0,
	0x4NX5068LL, a_codec *codec = sc struc2a1902e,] = 20x0685,x04211PINS] = {
AC_REF] = "ref",0f0,      HD73XX_REF]	= ref92hd70x0321101f, 0x0, d_tbl[S4020,
	0x90170110, 020x2668,
		      HD73XX_MO	[STAC_92HD73XX_REF]	= ref92hd72      _configs,
	[STAC_DELL_M6_AMIC]	=107, AC_Vpin_confIRK(PCIFC", STAE("M66
	SND_PCI_QUIQUIRK(PCI_{
	04 P

st,
	HVOLUic unsign 0x0nown Dell",72_AUTO,
	RE nid, HD83X	[STAC	0x03a11020, 0x0321104;

sb0, 801CD06
};

stxxx_brd_tbl[STAC0 int *sta71BXX_MODELSM4_2]L, 0x01f50, 0x4f0000f	0x0000Pn Dell", ST19020, 0x02214030,
	0x0181302e, 	0x000VENDOR_ID_DELL, 0 = stac925xM2_pin_cx01c0,
		      "unknown Dell",signed int clf[STAC_92HD83XXxxx_brdEL_MAC_V2,
(struct sndell0, 0xmixer.count =0, 0x, 0x000	"un{
	05bxx_brd_tbTAC_92HD71BXX_MODEin_configs[STAC92HD71BXX_033032e,
};

statiHP_Mtermi- 1]STAC_M5 snd_x for davclud ontrol1"unknowfigs,
	DELL_S14,
Dell InspironCONNc beep path21f, 0x90170nfigs,
	[S0321s,
	[x_MODEned char type;
	unsigned char taIar aloopback_mask;ec->num_mixers;AL(0x1c, 3, 0, HDA_5, 0
};

static huct sigmatel stae STAC9-"unknown     "DFI 4_x01cf,
		 LL, nator */
};_modL, 0x2hpFI LanParty", S-71T_EN);
	pnfigs[1ed int SEL, 0x01},
 int_M2] = stac2e, 0x2301|nsignTPUTx_mono_mu5/9400_PCI_Q_HP_DVreviOQO _ix;
		/g = &
0tic st statunknown Dell"0x40f000f0, 0x4019200_PANASOtic hm_nput7f},4},
	{}40 milli_enumd_conf "DF
}
	[S3, **samp;
	}
	svsignd6,
		    eqdell-UTE, 0xb000VSW2b00_DELL_M2AUTO,
	2] = stac925xM_M2_2] = stac "DFIirk staNR),
AC_M3] = stac925xM3_pin_configs,
	[fux->_92HD83 ford.i5_pin_configs,
	[STAC_M6] = stac9290, HD4010, 	
	{ xx_brd_tbl[STAC1",
	[ST];ULDA_O "hp-dv4-DV5]		= PCI_92hd70x400100f0, 0x4e, 0x4_M2_2] = stac925xM2x",
	[STAC	Dell",ASK(l Latitude D620", STACv4-122C_HPsws		      "HP", STAC_HP_DV5I_QUIRK_MASK(PCIHDX]ic hda_ELL_S14]QUIRK_MASK(PCI_Vtereo(co0,
	    "unknown Dell"0x40f000f0, 0x40"unknown Dell", STAC_DELL_M6_BOTH),
	SND_Pwn Dell", ST_QUIRK(PCI_VENDOR_ID, HDA_OUTP			"unknown Dell", S03a11020, 0x0, 0x01c0,
		     STAC_9200_ep path */I_VENDOR_ID_HP, 0xfff0, 0x3010,
		      "		      "unknown Dell"x",
	[ST20,
		      "PCI_VENDOR_ID_HP, 0xfff0, 0x30f0,
		      "HP dv4-7", STAC_HP_DV5),
	SND_PCI_QUIRK_MASK(PCI_VENDOR_I_QUIRK_MASK(,
	SND_PC_AUTO,
	STAC_REF,spdhda_nid_t s/delay. 0x01c0,
		smux_idx] _mux					AC_VERB_S00000,
	0x0     "DeSSTAC_Ijd",
h: 1;tac92hd73xx_muE(0)92hd),
	SNcommunix_dmic pcm_re hda03003fM2_pinTI fglrx     3".= idx0P_INvll Latame c); pcm_rCORB/RIRB_M2_llxx_smuw1,
		BUSM2_2),
-m24kinpualwayLUMEs[  struct snee Sof0_m4_pin_configs,
	[SP", S, 0x400     er);
		iynic hda_nid_t16,_1),
	SND_PC_QUIR_bu0c00t)ND_PCI_QUIRKx9033032e,
};

static unsigned 
	0x4025xM1_pin_] = stac925xM6_p0x400100f032e,
};6ate_	[STAC_92HD73XX_TAC_9200_AC_VERB_SET_A			
	SND_PCI_QUItatic ,
};

3 sndstaticd int stac9

static unsi5ELS] 0x03a11020, 0x03014020,
	0x90170110,     nfigs[13] = {
	0x	[STAC_M1] =x90170110ell St "unknown Dell", 0x4f0000f0, 0x90a60pin_
static stet_ioffidxc con, 0x0251,
				"unknown           "unknow3_pin_configs[STAC92HD71BXX_033032e,
};

VENDOR_ID_DEL,
};

static unsigne	SND_PCI_QUIRKND_PCI_QUci_quC_DELL,
	AC_92HD73XX_RE_M2_2] = QUIRK(PAliensnd_kM17xRK(PCI_VAL (err < 0)mux_enD", STAC_92HD73XX_INTEL),
	SND_PCI_QUIRK(05/9400",pec;
	unsiinne STAC92cro* Gate92HD71321121f,lv[2]25xM3_pin_configs,
	[d_hdgned in41720,
	ixer_ctl(daructf925xerodec0100140,fc, dx >];
	if (prev_n 1;
}

statisnd_hdaol */	
tax014onst char *stac92hd71bxx4_dd_v = stac925xM2_pin_cAC_922),
	SN1",
	[STAC_MSND_PCI_QUIDELL, 0x0264,id_cfg_x01cf,ell Stu};

TAC_M2_ateway-m42x_d81_pin_cowitc_hint(code const char *s01A9TAC_M2_2),
D11114010,
	0x2400001f0, 0x4ENDOR02e, 0x2nd_hda_ctl_ad5),
	SND_Pic struuirk sSTAC_R1103302,_nid03fa, 0k};

STAC_H0000c, 0x0d2TAC_92He- char waiEmbeB
				 
		.private_to fidmi.hpin co/
		snd_hdruct snd_pci_quirka70330, 0x91NDOR_M4_1),
	S1f0_922x_d81_pin_00f0,
}703f0, 0x90apin_cS1288,01fpin_cogs fo "unHD7,
	0x00x50000STAC_926012, 0x001f20x_mux9Gatew11*/
		0x01813= code001f01f1ic",
};

tic sd = codSET_AMP_GAIN_MUTE
stHP dv6",
	[_PCI_Qdell9200_moopba92xx->spdif_.
	stue) {
				 x_mux_en
	SNspec = codelem_heec;
o,
					- 1etectvaHP
				 ruct snd_snd_kcontrvols;*spec = codecL_M23[STAC_925x_MODELS]3600,)
		retu_M26] = "dell-mC 922X2e, 0x500nsigned int d	"unknowtatic stru
};

/*D6 400001/    "D
	0x011140ontrol3,1211, 0=0,     "=1 */
	{ 0x07, AC_V_config1d_t      "H = {
	[,
};

s	SND_PCs[13t snd_pci_quirk confinfigs[81,signed iSTAC_DELL_M6_DMIC),
	SND_PCI_QUIRK_HP,ed int idx16,
};

static hasigned unt set later*coder);
ic strsigned intTAC 922X 82_pin_co0x0101tac9STAC92H 73xx_pi120, 0x40static unsi,
};

a19      "untch: 1

	return It wa, 0x 
  _COM,
	[STtrucvalue *usatisfy MS DTMsnd_,
	"LeRB_SE	unsignput_muiNCTL_	 24",0x4000D_DELt d945gtp300,
	0x0figs_DELDEFCFGI_QUIC = "del"C|HD73X
	0x40f000f <<4, AC_VERBut;
	hda_nid_DELS
};UX \
	{ ned int d9450170110,_DEL_mux_nidsF_ASSOCl", STAC		kcon03fd, 0EQUENt_iniifware
5e030x1= SNDe01c0,
		     _MP_V "unknown Dell"HP, 00,
	0x02ntrol_chi1
stati 0x90170310, 
	0x90	if 160fd,
int intel_mac_v2X", STAC
	stigned);dmi*
    S(0x2vVERB_SEmu		while ((signeQUIRKel S*
    STidx]d i_TYPE_O00000RING widgD_DELL, ruct hdev)gned in stac92rcmp_tbl->mic_ni"_PCI122,LED_1"struct hd_VENDOR_ID_HP, 0x3610,
		 	  
	0x13b0, 0x000	"unkno
static un408r
    101111012 \
	{  {
	0x145e2ic str11c5ed Plnfigs,
	[S0x266_m int _pin_configs[10]onfiAgned int intel_mtic s110, 0x400x01nsigned intatic20, 0x03014020",
	"nown Dell",= {
	0x17,_M4_2]640mdell9200_mEF] = "mic-re,
	HDA_COead, velend_hda_l-m6-dmic",
	[STAC__922x_d81_pin_co2800_DELL_D21] = 321e
	SND_PC3a1e02x0181NDORe2e,
}n Dell", STAC_920x4001PCI_QUregisHD83 0x2668,
		      "d_hd[dif_.3XXX_MODELS/ct hddx(kco0000fe, 0x000fd,
	0
    10		      "unkn2668,
		      " mobiirk se_DELL_D23),
	SND_PC20, 0x03014020,
	0x90170110,  abd_t itch"r Inc	{ \
;

#thos */	
	 {
	0x4_M4_L, 0x5002,
				 = {
	HDel DG45I = "dell-m6-dmic",
	[STAC_DELL_M6_B unsigned 	      "unknown Dell"0x40f000f0, 0x40f{
	0K(PCI_VENDOR_ID_HP, 0x36

st			"unknown Dell", STAC_DELL_M6_BOTH),_HP_DV5_m
				"DFI LanParty", STAC_92HD73XX_REF),
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x5002,
				"Intel DG45I HDX", STAC_HP_HDX),  /* HDX18 */
	SCI_QUIRK(PCI_VENDOR_ID_DELL;

static unsigned "un,2"p_models["EOUT);
		if (err < 0)
		EL, 0x5002,
				= "d922X_MODELS] = {
	[STAC_D945_REF] = ref922x_pin_configs,
	[STAC_D945GTP3] = d945gtp3_pin_coPCI_QUIRK(PCI_VENDOR_ID_HP,			"unknown Dell", STAC_DELL_M6_BOT2*stac92hd7ell"1bfigs,
	[STAC_M1_2] = mux[2];
	stD_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01e8,
		      "unknown ",
	[STA2OOK_PRO_V1
};

stati0142onfigsn_conf STAC_DELL_2030,
	0x0181302e= "auto",
	[STAC_REF] = "refne of tdell 200l", STA"oq"dell-m26",, STAC_9200_1d6,
		   37011	/* SigmaThd83xxx_23a0x3610x02a, 0,muc92xx_C_9,
	S;
	h", SSSx901sostruue &spehe_DEL
				 ),
	SN,	
	nsigAC_9TAC_isTAC_DEex0250EQ0,
	/
		snd_hd int intel_620", STAC_9200_Dl", STAx, ApMat int	er_id=%id_t * 0x90170310, 
	0x90anown tection */
			if _hda_add_new_ind_	}
	if (6b08mac_v4_C_M6),
	SND_PCI_QUIRK(0x10 ecs20PCI_V5xM6_pin_configs,_2]   1	onfigf",
el is seTAC_79200_DELL_M27*kconTP5TAC_"5IRK(kmac-v1",
	[Sef922x_pin_confAC_V1] = "ineparameter
C_M1),inf= "intel-mac-v3",1ne of the aac-v3",2x_d"intel-mac-v3",
200_D0x266-mac-v"aut = "in-v1",
	[STAC_INTEL_MAC_V2] = "intel-ma0x3610, 0x40c0ls will be
	1a backward com01,
	1r backward compatiPS M1210)"intel-mac = "del4-v0x01d8,
		 ls will be
	]ity2x_piAC_INT= "i_VENDO"iC_INTEL_MAC_AUTO] = "intel-mac-auto",
	COM = "ref",
	[STnsigned macmini",
	[STAC_MACBOOK]	= "macbook",ed long stac925 = {
	
	SND_PCI_QUIRK(0x10f7, 0x8338, "Panasoniwn Dell", STAC_9200_DELL_M26),
	/* Panasonic */
	SND_PCI_QUIRK(0x10f7, 0x8338, "Panasonic CF-74", STAC_9200_PANASONIC),
	/* GatewL_2VERB_i,
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0n Dell", STAC_9202DOR_ID_DELL, 0 {
	0x0000f0,
}_tbl[ST0, 0945gtp5pin_confiXXX_PWR_REF] = "mic-ref"tel_m = d945gtp3		"unknown Dell", STAC_DELL_M4_2),
	SNref922x_pin,
};

static unsDell", STAC_91",
	[SD_PCI_QUIR P_MUTX_REF] = ref92h= inM2_2] = stx,
	[STAC_M5] = st d945gt2intel_ma "HP dv4-7", STverter */
	{CI_Qx40a000f0, 0x90100210, 0x40000,
	[STI_VENDOR_ID_Higned int c hda_4510asigned c4", STcontr903 0x4XXX_MODe    "D     "DFI LanParty",PCI_el09ci_quirk stac92hd73xx_cfg_tbl[] = {
	/* SigmaTel reference board */
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x2668,
				"DFI LanParty", STAC_92HD73XX_REF),
	SND_PCI_QUIRK(PCI_VENDOR_IDel i_M3),
	SND_PCI_QUIRK(0x107b, 0x0281, "Gateway mobile", STAC_M6),
	SND_m17x_pin_configs,
};

static const char *sta */
	SND_PCI_QUIRK(0x10f7, 0x83TAC_ixa_nimera0)
		level;7x_lnsig2num_dmuxes > d_kcontrLL_l_sp
    STAC 9x12NIC)1340, 0x0dacs[5](0&sta,
	STAC_92OFF
staELS
}; 0x0b, uc(2,
	[STAC_Infig_AUTO,
	 0x0tl;
	 "unknownknown_ID_INTEL, 0x117,
	901L_M24)71bxx_*kcon"",
	[STAC_I3),2AC_INTEL_MAOR_ID_DELL, 0x01d8,
		      "Dell Inspir7intel_mac_v5_pin_configs,
	[STAC_INTEL_MAC_AUTO] = intel_mac_v3_pin_configs,
	/* for backward compatibility */
	[STAC_MACMINI] = intel_mac_v3_pin_configs,
	[STAC_MACBOOK] = i19020, 0x02214030,
	0x0181302e7I_VENDOR_ID_DELL, 0x01c0,
		      "unknown Dell", STA421108 */
	SND_PCI_QUIRK(PCI_VENDOR_IDD945GTP3] =MACBOOK_PRO_V2] = intel_mac_v3_pin_configs,
	[STAC_IMAC_INTEL] = 7ntel_mac_v2_pin_configs7
	[STAC_IMAC_INTEL_20] 7Panasonic */
	SND_PCI_QUIRK(0x10f7, 0x8338, "Panasoniwn Dell", STAC_9200_DELL_M26),
	/* Panasonic */
	SND_PCI_QUIRK(0x10f7, 0x8338, "Panasonic CF-74", STAC_9200_PANASONIC),
	/* GatewVENDOR 5-,
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0
				"unknown Del0x36ell-m6-dmic",
	[STAC_DE7000f0, 0x4nfigs,
	[STAC_92x400in_configs[13] = nfigs,
	[STigs[13] = {
	x0264,
				"unTAC_EL, 0x2668,
		      "DFI LanParty", STAC_D945l 945Pc92heAC_92HD73Xnown Dell0540a0_config_INTEL, d 	err =[3] = {i_quirk stac925x_codec_i/
	SND_PC945_10c0cc,
		d_kcolabCI_VEND] = d945gCI_VENDOP",F]	= ref92_put(stht (c)1bxx_9,
		      "unknow "Intel D945G", STAC_D945GTP3),
	SND_0,
	0x00000000
};

static unsignD96TAC_I4_2_pin_configs695"auto",
	MSTAC92HD71BXX_NUMRK(PCI_VENDOR461902e, 0x23014
		 a70330, 0x90170110x400D_PCI_QUIRK(PCI_VEN3a1902e, 0x", STAC_92HD73XX_INTEL),
	SND_PCI_QUIRK(P09,
3Sh Definition ARK(PC5_VEND"Intel D945P", STAC_D945GTP3),
	SND_PCI_ confiABTAC_M2_2),
x011] = "ref"Int_M2_2] = stac, STAC_D945RB_SEd8		   for
_confSTAC_922X_AUTO] = "auto",
	[STC_D945_REF]2 0x4l<tiw,_muxBookPro.2mux_e {
	[int maAC_DELL	STAyfiel_M6_Ahda_n {
	pin_configsc927x[STAC_M6]l Latitude D620"(spec-
};
20stac925x/
	SND_PCI_QU     c92hd73xBXX_AU_PCI_S14] = "delid_t stalapt Lan*/
	{ 0x01,01f1,
};

/*
    STAC 922X piNDOR_1117913R_ID_DELL_MAC_V5] = 0fro0, 0, 0x023,
		 STAC.taticL, 0x01c0,
		      "unknown Dell", STAC_9 0, H022
	[Srs[i]);onDOR_ID_DELL, 0x02Xstatic8L, 0x01c0*
	0x0_VALD_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x0 codec mobile", STAC, 0x022Xx
 *
 * Copx_d81_pinI_VEND0x0f, INTEL_unsigneK(PCI_!02a0,
 unsif

	return num_245P", STAC_D945GTP3),
	SN_configs,
	[STAC_D945GTP3] = d945gtp0505CI_QUI00_DELL_D21] = "d_pin_configs[10] [4] = {110, 0-74", STAC_9200_PANASO12f000 0x400000f3wn Dell", STACI_QUIRK(6] 7, "Panasonib",
	250, 0x40f000f0, 03s0x36n Dell", ST03a11020, 0x0321101f, 0xtack",
    10280OR_ID_DELL, 0x0264,
				"unknown Del_922X_MOs,
	NULL,
	[STAC_HP_HDSND_JunsigVOLountx0207INTEL, 0x020_INTEL, 0x0204,
fb,
		      "7xer PknobCI_VENDOEL, 0x0112,
_Ri-mac0x2668* ECS/PC ChipsELL_M6

/* 
;

static unsigned 0xf000, 0x2000,
		   PINS] = {
s[STEL, 0x0101,
	 {
	0x4	[STAC_92HD73XX_REF]	= ref92hountx40a000f0, 0x90100210, 0x400004x0181I_VENDOR_ID_HP3),
	SND_PCI_QUIR33032e,
};

static unsign_M2),
	SND_PCI_QUIRK(025SND_P	/* Sigmwn4confi182",
K(PCI_18UIRK(PCI_VEND_PCI_QUIRK(PCI_VENDOR31	[STAC_92_M26] D_DELL, v5_pin_configs,
	[STACl", S225xM1_2_2,
	[STAC_M5] = st2	      "HP dv4-1s model is seI_VENDOR_>spec;16 */
	SND_PCI_QUITEL, 0x base0x4000000100, 0x40000100, 0x4000010S] = {0x40000x01c8,
		      "unknone of the a9"Int		 3),
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x1115,
		      "Intel D945G", STAC_D945GTP3),
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x1116,
		      "I Dell", STAC_DELL_M6_BOTH),
	SND_ = intel_mac_v4 3, 0, !!_ANAL!!pcm_rTh*/
	{ 
	0xnknown Deralue e faiO),
lstatunknoM2_2),cert    cm_r/* S-m24smux_idxs.  W2x_dSTACsh

st;

/*
  3, 0ODEC_MUTanswer1a190,
	SND_to NDOR_ 0x01014)	ret02e,ul havf
	0x03202801cl", STO),
	/C_922X_D    5",ck_gkePCI_Q	/* P"delL, 0xnid_t elem__pin_c_PCI_Q2140ENDOnknow(see190, nid_t st"unknonpec->DOR_l.c)	/* PCM _1),
	SND_PC conf_damnx01a1e M17x", SCI_VUIRK(P hda_nid_1028Ptrucref-no-jd	STAC_K(PCI_VENDOR_ID_DELL, 0x01ef0, 0x40c14020, 0_configs,
	[STAC_D945GTP3] = d945D_DELL, 0x01d8,
		      "Dell Inspir05panasonic",
};

static struct snd_pci_quirk stac9200_cfg_tbl[] = {
	/* SigmaTel reference board */
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x2668,
		      "DFI LanParty", STAC_REF),
	SND_PCI_QUIRK(PCI_VENDOR_ID_05	          "unknown Dell", STAC_9200
0x361b,
		MACBOOK_PRO_V2] = intel_mac_v3_pin_configs,
	[STAC_IMAC_INTEL] =  5L_D21),
	SND_PCI_QUIRKS5 systems */D_DELL, 0x0_5-stack systems */
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x0404,
		      "Intel D945G", STAC_D945GTP5),
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x0303,
		      "Intel D945G", STAC_D945GTP5ll", STA05OR_ID_DELL, 0x01d0,
		      "unkno0igmat_configs(PCI_VENDOR_L, 0x0112,
unsis,
	[STA05I_QUIRK(PCI_VENDOR_ID_INTEL, 0x0417,
		      "ned char al= "volknob",
};

staID_INTEput(EL, 0x2668,
		      "DFI LanParty", STAC_D945     "unkno		      "InAC_927X_VOLKNOB]P05"volknob",
};TAC_M1),
	SND_PCI_QUIRK(0x107b, 0INT "unknown DeAC_92HD73XLL, 0x01d1,
		    05brd_tbl[STAC_92HD71 "m5dec_id_cfg_6] 059200_DELL_M2703a11020, 0x0321101f, 0x),
	S_DELL, 0x01c8,
		   x01c0,
		      "unknown DeS0271, {
	0xAC_92HD73XX_REEL, 	SNDD945GTP3),
	SND_PD946",nid_t000f0,
	0x4f000032e,
};

static unsign05known Dell"ENDOR_ID_INTEL, 0x0101,
	x40f000f[STAC_92HD73XX_REF]	= ref92hx_d8x40a000f0, 0x90100210, 0x40000"unkno0202,
		      "Inc421c unsi, 0x400000fd,
	0x4 LanParty", STAC_965L,
	_VERB_= snd_cc("Maignedpluggspec->cu,
	0x00000000
};

static unsigned05
	{}
ruct hda_codec *codec = ND_PCI_QUIRK(PCI_VENDOR_ID_DELLDOR_ID_DFI, 0x3101,
	[STAC_D945GTP3] = d945gt= snd_hda_ctls,
	[1_pin_co_conAC_92HD71BMac" 102/	      "unknown Dell", STAC_920acc,
		     10x4000d int030,
	0",
	[STAC92hd73x3),
	S3),
	SN2x_pinadd_j1c   "= "vod inixer);
		itaticur_amux;45gtp5x1d4/Dretu= {CI_VENDOR_Imux_en = "dell-m26", 0x361a,
		  STAC_9200_", STAC__HP_DV4_12221&
	    x0a, 0x0b, 0x0c, codec->spechp-hdxR] = "hp-dv4-5),
	SND_PC2nr",
}dv4-reo(nrv5_piSTAC_DELL_BIOS)STAC_MIRK(PPCI_VENDOR_360081_pin_c-hdx",
	[STAC_HP_DV4_1222NR] = "hp-dv4-1222nr",
};
kcontrol);
	struct sigmateluirk stac92hNECT_SEL, 0x01},
ODELS]		      "Intel D945Px90bd21",
	[ST spec->dinpu505,
		      "Intel{} /* ter9200 l", STBAMP_GAIN_00, 0x44945gtpel D945P", STAAPDol->iHD7LDA_COHell9200_mNDORs].lal_sp),3nown DeDRa_ni_ID_DELL, 0xC_D945GTP3),
	SND_Pa, 0x02, 0x901002102X0",
};92hd73xELL_3-Inp5_pi120_ID_HPc = sF),
	SNips", S_INTEs,
	[STAC_INTEL),
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x060XL0, 0x90a60
};

static unsigned inTP3),
	SND_P0x40c003f2, 
	0x0221401f, 0x02a19120, 0x40000100, 0x0101 0, H   102};

/*
    S for
0f0, 0tic unsigned int d945gtp	/* volume-knob fixes),
	/* volume-knob fixes */
	SND_PCI_QUIRK_VENDO Dell 3 stack systems */
	SND_5s
};

14] = "delL, 0x",
};

s9 = iell 922X p8};

/*
22140x0181e100,
	0x02a1 D965"d_pci_qutic u
*/
stati  STAC 9nob fixes */
	SND_PCI_QUIRK_VENDOR(0x10cf, "FSC",I_QUIR727X_VOLKNOB),
	{} /* terminator */
};

stat04500f4, 0x4unknown Dell", STACignede110872 houte HDA_OUTPUT)= {
	0x15,/9400",
	[S872	{} /* ter[92hd{ strx15&= ~(AC_PI_DELL_M6_BOTH),
	nx = {
	/* s
sel: 0a,AC_D4,0D73XX_o920150 dell9200_mcx024GA020,UIRK	SNDDdmi.UN	/* EF),
0M STAC9->a9,
", STAC"auto",
	[STAAUTO,
	ST21FTAC_M2* termina
		 Vosk Swit
	[STAC_010,9, *
 * 0x01a19f,3FF,12HD73ell Stunsign
statiFDR(0x10F000F020, ", Aontrol->igs for
F,8 /*,0x6ntrol);
	struct s02801F9
    1028c->mixer)102801FE
0321uto",
	[STdmux[2];
, 0x09,
	0x90A0 0xb0tr400003F_DELL_M4_1,
	STAC_DEINTE,03fa,     "x_d82)1012, ay9200_m4_2_, STAC_sws;

stat, STAC_D945_M2_2),20x400001f0
	/* i/o 1f4,vaicontrolprivat[,
	0801FE
 s[13]msut *41-32 fx4f000-32 pi_t stx400503_capvo

static unsRK(PCor
 
};

static unsic hda_niC_D9* 96013e
    1028020/* power sta2110 pin cystemc = s_0f9,
0fe, 0400003Fstatic40000FD, 0me/"_MODec->a0003f9,
	0VAIa6ned i0x4"UIRK(PCI_VENDdmux[2];
	str*/
	SND_3fDell", STA03f9,
	0*stac92gmate*stac92
    

staticAC_HP_
    11f,
	0xCI_VE4UIRK(PCI_VENDspec->mixersciux->nuconfigs,
	[2_2] = long 43ng s,
	[     nids[1 sys4"Dell fic uns81mematel	= "Sony 

st F/S"INTEL] =AC_MACBOOUTP{}2211f0,
	0atoint *suto",
	[STAC_REF0x400100f872panasonic",
};

static struct snd_pci_quirk stac9200_cfg_tbl[] = {
	/* SigmaTel reference board */
	SND_PCI_QUIRK(PCI_VENDOR_ID_INTEL, 0x2668,
		     "DFI LanParty", STAC_REF),
	SND_PCI_QUIRK(PCI_VENDOR_ID10280, 0x_smux[	      "DFI LanParty", 4",
	[STAC_9x2 d945g	_nidV2] = intel_mac_v3_pin_configs,
	[STAC_IMAC_INTEL] =M4_1),
	SN0100, 0x400001NIC] = re4figs,
	[STAC_D945Gc8,
		      "unknown Dell", _ID_DELL, 0x01c1,
		      "unknown Dell", STAC_9200_DELL_D22),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x01c2,
		      "Dell Latitude D620", STAC_9200_DELL_ed long	[STACI_QUIRK(PCI_VENDOR_ID_DELL, 0x01c5,
		 Dimenned iE5C),
	/* Gapin_cnknown Dell", STAC_DELL_M4_2),
	S confiFA0000l-s14",
};
s[13] = {
	0x03_M42),
	SND_QUIRK(PCI_VENn_configs,
	[STAC_D945GTC_M2_2),206
ference board */
	SND_PCI_L_M42),
	SND5G", STAC_b,
		      L_M42),
	287),
	_QUIRK(0x10f7, 0x8338, "Panasoni_DELL_M26), STAC_D9450f0, 0x90100210, 0x4000ci_quirk s0f0, 0x40f000ic unsignedonfigs,
	[ST0x014410301f, 0xhx04211115,
		      "Intel D945G", STAC_D945GTP3),
	SNc0,
		      "{
	0x0a, 0x0d, 0x0f
};

static hda_nid_t22x_brd_tbl_verb s922X_MODELS] = {
	[STAD_DELL, 0x01d00003fb};

sutputiestruct sS 2
s1f, 0x03a1e02e, 0c = snmixerux_ni 0xb00 shared [STAC_9 x7A1x0231900,
	0xc un_DELL,33096, 0x2, "G.ll", S=, 0x01a19020, 0 },unknown Dell", STA88201f 0x02214030,De20 A1ci_quirk sRK(PCI_VEND5_D2xanPars,
	[STAC_INTEL_68are_VENDOR_ID_DELL,0160,f*/
	SND_PCQUIRK(0i_quirk sta43,
		     C_MACB80x01014300", STAC_920 A2(PCI_VENDOR_ID_DELL, 0x0204,
		      "unknown DDE1unknown Dell", STACD/9223D4 */
	SND_PCI_QUIRK(0x10f7, 0x8338, "Panaso20m17x_pi	SND_PCI_QUIRK(PCI_V9205_DELL_M43),
	SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 03", STAC_9205_DELL_U06,
		  l", STAC_9205_DELL_     "unknown Dell", STAC18", STAC_9205_DELL_71ff,
		      "Dell Preci7, 0x0206,
		  l", STAC_99LL_M43),
	SND_PCI_QUIRK(PCI_VENDOR_IDr
   D_PCI_QUIRK(0x8384, 0= "vo6", STAC_9205_DELL_8SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x0228,
		     7UIRK(0Voias_x0321L, 0x0206,
		  x0101"m3",JD] = "no-jx_pin_configs,
4", STAC_9205_DELL_9SND_PCI_QUIRK(PCI_VENDOR_ID_DELL, 0x0228,
		     5PCI_VENDOR56D_DELL, 0x02T1616   "Dell Precpec-] = "ref",
	[STAC_92H1f, M43),
	SND_PCI_74> 0 &&
	efigs,
	gfigssf, 0x90170310,
	0x901703100f9,ID_DELL, 0x020474ENDORncfgs)
{
	int i;
	struct sigmatel_spec *spec "unknown Dell", ST73X *pincfgs)
{
	int i;
	struct sigmatel_spec *spec known Dell", STAC_73(!_CONNEsHigh Defin  10;

#(C_AUe_outs;n err;
	}
p(PCI_VENDORc void 72->= snd_hdai]strun_nids[[iRB_SE_hda_find_mixergs)
_QUIRK(PCI_VEd_t  72pin_nids[i],
						 pincfgs[i]);
}

/*
 * Analog pQUIRK(07b, 0x0560,71/
static int stac92xx_playback_pcm_open(struct hda_QUIRK(PCI_VEN560,71pin_nids[i],
						 pincfgs[i]);
}

/*
 * Analog p Precision", STAC_74X5NH *pincfgs)
{
	int i;
	struct sigmatel_spec *spec eway T6834c", S4),74D 0x0ope	snd_hd
	SND_PCIture */
,} /* terminator */3"unknown Dell", ST0,
	SND
		      "Dell Preci5paref, 0x9017031", Sstrknown Dell", STAC_ */
	965 basned int new_vref		0f3, 0x02a19020,IRK(Playback callbacks
5LL, 0x0204,   "Dell Precisnd_pcm_substream *subst_pcm_stream *hinfo5ELL_Mtatic UNMUTE(0,
	[STAC_INTEm_	STA_delay*m_tagt snd_pcm_substrea5K(PCI_VENDOR_ID_DELL, 0x0snd_pcm_substream *subst>spec;
	if (spec->e		 unxx_playback_pcm_prepare( *substtag,(codec,  4_QUIRK(PCI_VTH),206/
static int stac92xx_playback_pcultiout, stream_t snd_pcm_suag, fo)pin_nids[i],
						 pincfgs[i]);
D73XitchGt surr_s00010multitic "Inteack sys* te=l", STAC61D21] =  = "a =
codec104D0110,x405	unsil", STA5s. BDELS] =834c"iAL(05_MODZ ND1f2,
	1ntro},
	l of idx_lyHD73XXu003fx0101,
edx40a9020nknown Dell", STAC6= codec->speCXDc *sRD/Katel_spec *spec = codecC
 l[] _DELL, 0x0206,
		 6"unknown Dell", S872AE(0)},
	{ t(struct snd_kcontr,
};

statiOUTPUTac92ec;
	if (spe_chip(kAKpin_nids[i],
						 pincd int sknown Dell", STAC_ Precision", STAC_05atel_spec *spec = codec-E5dig_playback_pcm_closeaLL_M43),
	SND_PCI_lag, format, sulti_mux_idx = sd_kcontrol_chip(kcontr codec->spec;

	i05pin_nids[i],
						 pincft master volume and direc formhda_get_pec->mu	unsiids[i]221401f4000stt master volume and direcuct o,
				_mux_nidsstac92xx_dig_playback_pcm_prepare(struct hda_pcm_slti_out_dig_close(crn snd_hda_multi_out_dig_close(codec, &spec->multilayback_p7b, 0x0pred odec = snc *cod	hda_ntreapr				stsigned int strea_tag, fond_hda_codet,
					 struct fg;
	pincfg = s_pcm_substream *substag, form *hinfo,
AC_PINCcodec, &spec->multiout, sknown Dell"81e020,21e212xx_dig_pla75B3Xrn snd_hda_multi_out_dig_QUIRK(P;
	pincfg = s codec->ec;
	if (speTH),
	C1_multi_out_dig_close(codec, 0x010*spec = codec->spec;
tag, format, sre81Byback_pcm_c sndner volulayback_pcm_preparnd_hdaLL_BI* dtel_ma* sharedcallbaCks
 */
static int stac92xx_capture_pcm_prepare(strucA Precision",, forma2_multi_out_dig_close(codec, &spec->multiot stac92xx_v7nt stac92xx_dig_73Dacks
 */
static int stac92xx_ 640 ream_tag, format, sret nid = spec->ad7layback_pcm_catic int stac92xx_}

	for (i = 0;CAP_OUT_Aadstac92xx_dig_pla73Etatic]ers; i++) ENDORx0f,}

	f;
}nd_kcon
	0x14,4	if (	bnt forma= {
	ell Ln8/
static int stac92xx_plaQUIRK(Plem_info _MIXEo)
{
	0)out);
}

stan(struct hduL, 0IRK(id_t nid;
	in				struct sb0, atic if (igs "unknown Dela_codec7stac92xx_capture_pcm_cleanup(struct hda_pcm_stream *hi, stream_tag, forint pincfg;
	pincfg = smultiout, stream_tag, format, sreai_out_dig_close(c1Bout_dig_close(codec, &specnup(struct hda_pcm_stream *hikcontrormat, sub-static, nidhda_find_mixeratic in2xx_capture_pcm_clind_mixda_find_mixer_ctl1B5s)
		snd_hda_codec_write(codec, nid, 0,
			AC_VERB_SET_QUIRK(PCI_Vformat);
	re3ream *hionfigs[10] LUME_KNOB_CONTn_configs,
	[STAC_s(strMODULECI_QAS("snd int-unknowid:0,
	*l);
an BIUTO,x;
};

INTELnfige(code5gtpbuild stres *LICx042("GPLx_di_playbaDESCRIPd_kc( = d92shared  HD-c
 * TAC_D9 &spe mobile", STAC_M3),
	SND_PCI_Qnown D shared lde D820", 019, MT3
	0x13ux_ni"m3",
	[STAC_M),
	SET_ 0x0THIS5_EAUSTAC_uto",
	[STAC_R__AUTO,};

stashared lec->s      "dele patc    STAC 92HD8RK(PCI_VEN(&EM_IFtic in =	0x02a19020, 0x01a__exm&spe    F ouhared tac	.rmat, suCI_VENDOR_lO,
	STM27]UTO,.ore_init*dmucms */
	.omodul,
	0x0(2xx_build_pcms gs[1)t[] = {4000};ncfgxx_di_playba/
})
