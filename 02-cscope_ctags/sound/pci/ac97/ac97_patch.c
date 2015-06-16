/*
 *  Copyright (c) by Jaroslav Kysela <perex@perex.cz>
 *  Universal interface for Audio Codec '97
 *
 *  For more details look to AC '97 component specification revision 2.2
 *  by Intel Corporation (http://developer.intel.com) and to datasheets
 *  for specific codecs.
 *
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include "ac97_local.h"
#include "ac97_patch.h"

/*
 *  Chip specific initialization
 */

static int patch_build_controls(struct snd_ac97 * ac97, const struct snd_kcontrol_new *controls, int count)
{
	int idx, err;

	for (idx = 0; idx < count; idx++)
		if ((err = snd_ctl_add(ac97->bus->card, snd_ac97_cnew(&controls[idx], ac97))) < 0)
			return err;
	return 0;
}

/* replace with a new TLV */
static void reset_tlv(struct snd_ac97 *ac97, const char *name,
		      const unsigned int *tlv)
{
	struct snd_ctl_elem_id sid;
	struct snd_kcontrol *kctl;
	memset(&sid, 0, sizeof(sid));
	strcpy(sid.name, name);
	sid.iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	kctl = snd_ctl_find_id(ac97->bus->card, &sid);
	if (kctl && kctl->tlv.p)
		kctl->tlv.p = tlv;
}

/* set to the page, update bits and restore the page */
static int ac97_update_bits_page(struct snd_ac97 *ac97, unsigned short reg, unsigned short mask, unsigned short value, unsigned short page)
{
	unsigned short page_save;
	int ret;

	mutex_lock(&ac97->page_mutex);
	page_save = snd_ac97_read(ac97, AC97_INT_PAGING) & AC97_PAGE_MASK;
	snd_ac97_update_bits(ac97, AC97_INT_PAGING, AC97_PAGE_MASK, page);
	ret = snd_ac97_update_bits(ac97, reg, mask, value);
	snd_ac97_update_bits(ac97, AC97_INT_PAGING, AC97_PAGE_MASK, page_save);
	mutex_unlock(&ac97->page_mutex); /* unlock paging */
	return ret;
}

/*
 * shared line-in/mic controls
 */
static int ac97_enum_text_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo,
			       const char **texts, unsigned int nums)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = nums;
	if (uinfo->value.enumerated.item > nums - 1)
		uinfo->value.enumerated.item = nums - 1;
	strcpy(uinfo->value.enumerated.name, texts[uinfo->value.enumerated.item]);
	return 0;
}

static int ac97_surround_jack_mode_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	static const char *texts[] = { "Shared", "Independent" };
	return ac97_enum_text_info(kcontrol, uinfo, texts, 2);
}

static int ac97_surround_jack_mode_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ac97 *ac97 = snd_kcontrol_chip(kcontrol);

	ucontrol->value.enumerated.item[0] = ac97->indep_surround;
	return 0;
}

static int ac97_surround_jack_mode_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ac97 *ac97 = snd_kcontrol_chip(kcontrol);
	unsigned char indep = !!ucontrol->value.enumerated.item[0];

	if (indep != ac97->indep_surround) {
		ac97->indep_surround = indep;
		if (ac97->build_ops->update_jacks)
			ac97->build_ops->update_jacks(ac97);
		return 1;
	}
	return 0;
}

static int ac97_channel_mode_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	static const char *texts[] = { "2ch", "4ch", "6ch", "8ch" };
	return ac97_enum_text_info(kcontrol, uinfo, texts,
		kcontrol->private_value);
}

static int ac97_channel_mode_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ac97 *ac97 = snd_kcontrol_chip(kcontrol);

	ucontrol->value.enumerated.item[0] = ac97->channel_mode;
	return 0;
}

static int ac97_channel_mode_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ac97 *ac97 = snd_kcontrol_chip(kcontrol);
	unsigned char mode = ucontrol->value.enumerated.item[0];

	if (mode >= kcontrol->private_value)
		return -EINVAL;

	if (mode != ac97->channel_mode) {
		ac97->channel_mode = mode;
		if (ac97->build_ops->update_jacks)
			ac97->build_ops->update_jacks(ac97);
		return 1;
	}
	return 0;
}

#define AC97_SURROUND_JACK_MODE_CTL \
	{ \
		.iface	= SNDRV_CTL_ELEM_IFACE_MIXER, \
		.name	= "Surround Jack Mode", \
		.info = ac97_surround_jack_mode_info, \
		.get = ac97_surround_jack_mode_get, \
		.put = ac97_surround_jack_mode_put, \
	}
/* 6ch */
#define AC97_CHANNEL_MODE_CTL \
	{ \
		.iface	= SNDRV_CTL_ELEM_IFACE_MIXER, \
		.name	= "Channel Mode", \
		.info = ac97_channel_mode_info, \
		.get = ac97_channel_mode_get, \
		.put = ac97_channel_mode_put, \
		.private_value = 3, \
	}
/* 4ch */
#define AC97_CHANNEL_MODE_4CH_CTL \
	{ \
		.iface	= SNDRV_CTL_ELEM_IFACE_MIXER, \
		.name	= "Channel Mode", \
		.info = ac97_channel_mode_info, \
		.get = ac97_channel_mode_get, \
		.put = ac97_channel_mode_put, \
		.private_value = 2, \
	}
/* 8ch */
#define AC97_CHANNEL_MODE_8CH_CTL \
	{ \
		.iface  = SNDRV_CTL_ELEM_IFACE_MIXER, \
		.name   = "Channel Mode", \
		.info = ac97_channel_mode_info, \
		.get = ac97_channel_mode_get, \
		.put = ac97_channel_mode_put, \
		.private_value = 4, \
	}

static inline int is_surround_on(struct snd_ac97 *ac97)
{
	return ac97->channel_mode >= 1;
}

static inline int is_clfe_on(struct snd_ac97 *ac97)
{
	return ac97->channel_mode >= 2;
}

/* system has shared jacks with surround out enabled */
static inline int is_shared_surrout(struct snd_ac97 *ac97)
{
	return !ac97->indep_surround && is_surround_on(ac97);
}

/* system has shared jacks with center/lfe out enabled */
static inline int is_shared_clfeout(struct snd_ac97 *ac97)
{
	return !ac97->indep_surround && is_clfe_on(ac97);
}

/* system has shared jacks with line in enabled */
static inline int is_shared_linein(struct snd_ac97 *ac97)
{
	return !ac97->indep_surround && !is_surround_on(ac97);
}

/* system has shared jacks with mic in enabled */
static inline int is_shared_micin(struct snd_ac97 *ac97)
{
	return !ac97->indep_surround && !is_clfe_on(ac97);
}

static inline int alc850_is_aux_back_surround(struct snd_ac97 *ac97)
{
	return is_surround_on(ac97);
}

/* The following snd_ac97_ymf753_... items added by David Shust (dshust@shustring.com) */
/* Modified for YMF743 by Keita Maehara <maehara@debian.org> */

/* It is possible to indicate to the Yamaha YMF7x3 the type of
   speakers being used. */

static int snd_ac97_ymf7x3_info_speaker(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_info *uinfo)
{
	static char *texts[3] = {
		"Standard", "Small", "Smaller"
	};

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = 3;
	if (uinfo->value.enumerated.item > 2)
		uinfo->value.enumerated.item = 2;
	strcpy(uinfo->value.enumerated.name, texts[uinfo->value.enumerated.item]);
	return 0;
}

static int snd_ac97_ymf7x3_get_speaker(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ac97 *ac97 = snd_kcontrol_chip(kcontrol);
	unsigned short val;

	val = ac97->regs[AC97_YMF7X3_3D_MODE_SEL];
	val = (val >> 10) & 3;
	if (val > 0)    /* 0 = invalid */
		val--;
	ucontrol->value.enumerated.item[0] = val;
	return 0;
}

static int snd_ac97_ymf7x3_put_speaker(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ac97 *ac97 = snd_kcontrol_chip(kcontrol);
	unsigned short val;

	if (ucontrol->value.enumerated.item[0] > 2)
		return -EINVAL;
	val = (ucontrol->value.enumerated.item[0] + 1) << 10;
	return snd_ac97_update(ac97, AC97_YMF7X3_3D_MODE_SEL, val);
}

static const struct snd_kcontrol_new snd_ac97_ymf7x3_controls_speaker =
{
	.iface  = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name   = "3D Control - Speaker",
	.info   = snd_ac97_ymf7x3_info_speaker,
	.get    = snd_ac97_ymf7x3_get_speaker,
	.put    = snd_ac97_ymf7x3_put_speaker,
};

/* It is possible to indicate to the Yamaha YMF7x3 the source to
   direct to the S/PDIF output. */
static int snd_ac97_ymf7x3_spdif_source_info(struct snd_kcontrol *kcontrol,
					     struct snd_ctl_elem_info *uinfo)
{
	static char *texts[2] = { "AC-Link", "A/D Converter" };

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = 2;
	if (uinfo->value.enumerated.item > 1)
		uinfo->value.enumerated.item = 1;
	strcpy(uinfo->value.enumerated.name, texts[uinfo->value.enumerated.item]);
	return 0;
}

static int snd_ac97_ymf7x3_spdif_source_get(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ac97 *ac97 = snd_kcontrol_chip(kcontrol);
	unsigned short val;

	val = ac97->regs[AC97_YMF7X3_DIT_CTRL];
	ucontrol->value.enumerated.item[0] = (val >> 1) & 1;
	return 0;
}

static int snd_ac97_ymf7x3_spdif_source_put(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ac97 *ac97 = snd_kcontrol_chip(kcontrol);
	unsigned short val;

	if (ucontrol->value.enumerated.item[0] > 1)
		return -EINVAL;
	val = ucontrol->value.enumerated.item[0] << 1;
	return snd_ac97_update_bits(ac97, AC97_YMF7X3_DIT_CTRL, 0x0002, val);
}

static int patch_yamaha_ymf7x3_3d(struct snd_ac97 *ac97)
{
	struct snd_kcontrol *kctl;
	int err;

	kctl = snd_ac97_cnew(&snd_ac97_controls_3d[0], ac97);
	err = snd_ctl_add(ac97->bus->card, kctl);
	if (err < 0)
		return err;
	strcpy(kctl->id.name, "3D Control - Wide");
	kctl->private_value = AC97_SINGLE_VALUE(AC97_3D_CONTROL, 9, 7, 0);
	snd_ac97_write_cache(ac97, AC97_3D_CONTROL, 0x0000);
	err = snd_ctl_add(ac97->bus->card,
			  snd_ac97_cnew(&snd_ac97_ymf7x3_controls_speaker,
					ac97));
	if (err < 0)
		return err;
	snd_ac97_write_cache(ac97, AC97_YMF7X3_3D_MODE_SEL, 0x0c00);
	return 0;
}

static const struct snd_kcontrol_new snd_ac97_yamaha_ymf743_controls_spdif[3] =
{
	AC97_SINGLE(SNDRV_CTL_NAME_IEC958("", PLAYBACK, SWITCH),
		    AC97_YMF7X3_DIT_CTRL, 0, 1, 0),
	{
		.iface	= SNDRV_CTL_ELEM_IFACE_MIXER,
		.name	= SNDRV_CTL_NAME_IEC958("", PLAYBACK, NONE) "Source",
		.info	= snd_ac97_ymf7x3_spdif_source_info,
		.get	= snd_ac97_ymf7x3_spdif_source_get,
		.put	= snd_ac97_ymf7x3_spdif_source_put,
	},
	AC97_SINGLE(SNDRV_CTL_NAME_IEC958("", NONE, NONE) "Mute",
		    AC97_YMF7X3_DIT_CTRL, 2, 1, 1)
};

static int patch_yamaha_ymf743_build_spdif(struct snd_ac97 *ac97)
{
	int err;

	err = patch_build_controls(ac97, &snd_ac97_controls_spdif[0], 3);
	if (err < 0)
		return err;
	err = patch_build_controls(ac97,
				   snd_ac97_yamaha_ymf743_controls_spdif, 3);
	if (err < 0)
		return err;
	/* set default PCM S/PDIF params */
	/* PCM audio,no copyright,no preemphasis,PCM coder,original */
	snd_ac97_write_cache(ac97, AC97_YMF7X3_DIT_CTRL, 0xa201);
	return 0;
}

static struct snd_ac97_build_ops patch_yamaha_ymf743_ops = {
	.build_spdif	= patch_yamaha_ymf743_build_spdif,
	.build_3d	= patch_yamaha_ymf7x3_3d,
};

static int patch_yamaha_ymf743(struct snd_ac97 *ac97)
{
	ac97->build_ops = &patch_yamaha_ymf743_ops;
	ac97->caps |= AC97_BC_BASS_TREBLE;
	ac97->caps |= 0x04 << 10; /* Yamaha 3D enhancement */
	ac97->rates[AC97_RATES_SPDIF] = SNDRV_PCM_RATE_48000; /* 48k only */
	ac97->ext_id |= AC97_EI_SPDIF; /* force the detection of spdif */
	return 0;
}

/* The AC'97 spec states that the S/PDIF signal is to be output at pin 48.
   The YMF753 will output the S/PDIF signal to pin 43, 47 (EAPD), or 48.
   By default, no output pin is selected, and the S/PDIF signal is not output.
   There is also a bit to mute S/PDIF output in a vendor-specific register. */
static int snd_ac97_ymf753_spdif_output_pin_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	static char *texts[3] = { "Disabled", "Pin 43", "Pin 48" };

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = 3;
	if (uinfo->value.enumerated.item > 2)
		uinfo->value.enumerated.item = 2;
	strcpy(uinfo->value.enumerated.name, texts[uinfo->value.enumerated.item]);
	return 0;
}

static int snd_ac97_ymf753_spdif_output_pin_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ac97 *ac97 = snd_kcontrol_chip(kcontrol);
	unsigned short val;

	val = ac97->regs[AC97_YMF7X3_DIT_CTRL];
	ucontrol->value.enumerated.item[0] = (val & 0x0008) ? 2 : (val & 0x0020) ? 1 : 0;
	return 0;
}

static int snd_ac97_ymf753_spdif_output_pin_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ac97 *ac97 = snd_kcontrol_chip(kcontrol);
	unsigned short val;

	if (ucontrol->value.enumerated.item[0] > 2)
		return -EINVAL;
	val = (ucontrol->value.enumerated.item[0] == 2) ? 0x0008 :
	      (ucontrol->value.enumerated.item[0] == 1) ? 0x0020 : 0;
	return snd_ac97_update_bits(ac97, AC97_YMF7X3_DIT_CTRL, 0x0028, val);
	/* The following can be used to direct S/PDIF output to pin 47 (EAPD).
	   snd_ac97_write_cache(ac97, 0x62, snd_ac97_read(ac97, 0x62) | 0x0008); */
}

static const struct snd_kcontrol_new snd_ac97_ymf753_controls_spdif[3] = {
	{
		.iface	= SNDRV_CTL_ELEM_IFACE_MIXER,
		.name	= SNDRV_CTL_NAME_IEC958("",PLAYBACK,NONE) "Source",
		.info	= snd_ac97_ymf7x3_spdif_source_info,
		.get	= snd_ac97_ymf7x3_spdif_source_get,
		.put	= snd_ac97_ymf7x3_spdif_source_put,
	},
	{
		.iface	= SNDRV_CTL_ELEM_IFACE_MIXER,
		.name	= SNDRV_CTL_NAME_IEC958("",PLAYBACK,NONE) "Output Pin",
		.info	= snd_ac97_ymf753_spdif_output_pin_info,
		.get	= snd_ac97_ymf753_spdif_output_pin_get,
		.put	= snd_ac97_ymf753_spdif_output_pin_put,
	},
	AC97_SINGLE(SNDRV_CTL_NAME_IEC958("", NONE, NONE) "Mute",
		    AC97_YMF7X3_DIT_CTRL, 2, 1, 1)
};

static int patch_yamaha_ymf753_post_spdif(struct snd_ac97 * ac97)
{
	int err;

	if ((err = patch_build_controls(ac97, snd_ac97_ymf753_controls_spdif, ARRAY_SIZE(snd_ac97_ymf753_controls_spdif))) < 0)
		return err;
	return 0;
}

static struct snd_ac97_build_ops patch_yamaha_ymf753_ops = {
	.build_3d	= patch_yamaha_ymf7x3_3d,
	.build_post_spdif = patch_yamaha_ymf753_post_spdif
};

static int patch_yamaha_ymf753(struct snd_ac97 * ac97)
{
	/* Patch for Yamaha YMF753, Copyright (c) by David Shust, dshust@shustring.com.
	   This chip has nonstandard and extended behaviour with regard to its S/PDIF output.
	   The AC'97 spec states that the S/PDIF signal is to be output at pin 48.
	   The YMF753 will ouput the S/PDIF signal to pin 43, 47 (EAPD), or 48.
	   By default, no output pin is selected, and the S/PDIF signal is not output.
	   There is also a bit to mute S/PDIF output in a vendor-specific register.
	*/
	ac97->build_ops = &patch_yamaha_ymf753_ops;
	ac97->caps |= AC97_BC_BASS_TREBLE;
	ac97->caps |= 0x04 << 10; /* Yamaha 3D enhancement */
	return 0;
}

/*
 * May 2, 2003 Liam Girdwood <lrg@slimlogic.co.uk>
 *  removed broken wolfson00 patch.
 *  added support for WM9705,WM9708,WM9709,WM9710,WM9711,WM9712 and WM9717.
 */

static const struct snd_kcontrol_new wm97xx_snd_ac97_controls[] = {
AC97_DOUBLE("Front Playback Volume", AC97_WM97XX_FMIXER_VOL, 8, 0, 31, 1),
AC97_SINGLE("Front Playback Switch", AC97_WM97XX_FMIXER_VOL, 15, 1, 1),
};

static int patch_wolfson_wm9703_specific(struct snd_ac97 * ac97)
{
	/* This is known to work for the ViewSonic ViewPad 1000
	 * Randolph Bentson <bentson@holmsjoen.com>
	 * WM9703/9707/9708/9717 
	 */
	int err, i;
	
	for (i = 0; i < ARRAY_SIZE(wm97xx_snd_ac97_controls); i++) {
		if ((err = snd_ctl_add(ac97->bus->card, snd_ac97_cnew(&wm97xx_snd_ac97_controls[i], ac97))) < 0)
			return err;
	}
	snd_ac97_write_cache(ac97,  AC97_WM97XX_FMIXER_VOL, 0x0808);
	return 0;
}

static struct snd_ac97_build_ops patch_wolfson_wm9703_ops = {
	.build_specific = patch_wolfson_wm9703_specific,
};

static int patch_wolfson03(struct snd_ac97 * ac97)
{
	ac97->build_ops = &patch_wolfson_wm9703_ops;
	return 0;
}

static const struct snd_kcontrol_new wm9704_snd_ac97_controls[] = {
AC97_DOUBLE("Front Playback Volume", AC97_WM97XX_FMIXER_VOL, 8, 0, 31, 1),
AC97_SINGLE("Front Playback Switch", AC97_WM97XX_FMIXER_VOL, 15, 1, 1),
AC97_DOUBLE("Rear Playback Volume", AC97_WM9704_RMIXER_VOL, 8, 0, 31, 1),
AC97_SINGLE("Rear Playback Switch", AC97_WM9704_RMIXER_VOL, 15, 1, 1),
AC97_DOUBLE("Rear DAC Volume", AC97_WM9704_RPCM_VOL, 8, 0, 31, 1),
AC97_DOUBLE("Surround Volume", AC97_SURROUND_MASTER, 8, 0, 31, 1),
};

static int patch_wolfson_wm9704_specific(struct snd_ac97 * ac97)
{
	int err, i;
	for (i = 0; i < ARRAY_SIZE(wm9704_snd_ac97_controls); i++) {
		if ((err = snd_ctl_add(ac97->bus->card, snd_ac97_cnew(&wm9704_snd_ac97_controls[i], ac97))) < 0)
			return err;
	}
	/* patch for DVD noise */
	snd_ac97_write_cache(ac97, AC97_WM9704_TEST, 0x0200);
	return 0;
}

static struct snd_ac97_build_ops patch_wolfson_wm9704_ops = {
	.build_specific = patch_wolfson_wm9704_specific,
};

static int patch_wolfson04(struct snd_ac97 * ac97)
{
	/* WM9704M/9704Q */
	ac97->build_ops = &patch_wolfson_wm9704_ops;
	return 0;
}

static int patch_wolfson_wm9705_specific(struct snd_ac97 * ac97)
{
	int err, i;
	for (i = 0; i < ARRAY_SIZE(wm97xx_snd_ac97_controls); i++) {
		if ((err = snd_ctl_add(ac97->bus->card, snd_ac97_cnew(&wm97xx_snd_ac97_controls[i], ac97))) < 0)
			return err;
	}
	snd_ac97_write_cache(ac97,  0x72, 0x0808);
	return 0;
}

static struct snd_ac97_build_ops patch_wolfson_wm9705_ops = {
	.build_specific = patch_wolfson_wm9705_specific,
};

static int patch_wolfson05(struct snd_ac97 * ac97)
{
	/* WM9705, WM9710 */
	ac97->build_ops = &patch_wolfson_wm9705_ops;
#ifdef CONFIG_TOUCHSCREEN_WM9705
	/* WM9705 touchscreen uses AUX and VIDEO for touch */
	ac97->flags |= AC97_HAS_NO_VIDEO | AC97_HAS_NO_AUX;
#endif
	return 0;
}

static const char* wm9711_alc_select[] = {"None", "Left", "Right", "Stereo"};
static const char* wm9711_alc_mix[] = {"Stereo", "Right", "Left", "None"};
static const char* wm9711_out3_src[] = {"Left", "VREF", "Left + Right", "Mono"};
static const char* wm9711_out3_lrsrc[] = {"Master Mix", "Headphone Mix"};
static const char* wm9711_rec_adc[] = {"Stereo", "Left", "Right", "Mute"};
static const char* wm9711_base[] = {"Linear Control", "Adaptive Boost"};
static const char* wm9711_rec_gain[] = {"+1.5dB Steps", "+0.75dB Steps"};
static const char* wm9711_mic[] = {"Mic 1", "Differential", "Mic 2", "Stereo"};
static const char* wm9711_rec_sel[] = 
	{"Mic 1", "NC", "NC", "Master Mix", "Line", "Headphone Mix", "Phone Mix", "Phone"};
static const char* wm9711_ng_type[] = {"Constant Gain", "Mute"};

static const struct ac97_enum wm9711_enum[] = {
AC97_ENUM_SINGLE(AC97_PCI_SVID, 14, 4, wm9711_alc_select),
AC97_ENUM_SINGLE(AC97_VIDEO, 10, 4, wm9711_alc_mix),
AC97_ENUM_SINGLE(AC97_AUX, 9, 4, wm9711_out3_src),
AC97_ENUM_SINGLE(AC97_AUX, 8, 2, wm9711_out3_lrsrc),
AC97_ENUM_SINGLE(AC97_REC_SEL, 12, 4, wm9711_rec_adc),
AC97_ENUM_SINGLE(AC97_MASTER_TONE, 15, 2, wm9711_base),
AC97_ENUM_DOUBLE(AC97_REC_GAIN, 14, 6, 2, wm9711_rec_gain),
AC97_ENUM_SINGLE(AC97_MIC, 5, 4, wm9711_mic),
AC97_ENUM_DOUBLE(AC97_REC_SEL, 8, 0, 8, wm9711_rec_sel),
AC97_ENUM_SINGLE(AC97_PCI_SVID, 5, 2, wm9711_ng_type),
};

static const struct snd_kcontrol_new wm9711_snd_ac97_controls[] = {
AC97_SINGLE("ALC Target Volume", AC97_CODEC_CLASS_REV, 12, 15, 0),
AC97_SINGLE("ALC Hold Time", AC97_CODEC_CLASS_REV, 8, 15, 0),
AC97_SINGLE("ALC Decay Time", AC97_CODEC_CLASS_REV, 4, 15, 0),
AC97_SINGLE("ALC Attack Time", AC97_CODEC_CLASS_REV, 0, 15, 0),
AC97_ENUM("ALC Function", wm9711_enum[0]),
AC97_SINGLE("ALC Max Volume", AC97_PCI_SVID, 11, 7, 1),
AC97_SINGLE("ALC ZC Timeout", AC97_PCI_SVID, 9, 3, 1),
AC97_SINGLE("ALC ZC Switch", AC97_PCI_SVID, 8, 1, 0),
AC97_SINGLE("ALC NG Switch", AC97_PCI_SVID, 7, 1, 0),
AC97_ENUM("ALC NG Type", wm9711_enum[9]),
AC97_SINGLE("ALC NG Threshold", AC97_PCI_SVID, 0, 31, 1),

AC97_SINGLE("Side Tone Switch", AC97_VIDEO, 15, 1, 1),
AC97_SINGLE("Side Tone Volume", AC97_VIDEO, 12, 7, 1),
AC97_ENUM("ALC Headphone Mux", wm9711_enum[1]),
AC97_SINGLE("ALC Headphone Volume", AC97_VIDEO, 7, 7, 1),

AC97_SINGLE("Out3 Switch", AC97_AUX, 15, 1, 1),
AC97_SINGLE("Out3 ZC Switch", AC97_AUX, 7, 1, 0),
AC97_ENUM("Out3 Mux", wm9711_enum[2]),
AC97_ENUM("Out3 LR Mux", wm9711_enum[3]),
AC97_SINGLE("Out3 Volume", AC97_AUX, 0, 31, 1),

AC97_SINGLE("Beep to Headphone Switch", AC97_PC_BEEP, 15, 1, 1),
AC97_SINGLE("Beep to Headphone Volume", AC97_PC_BEEP, 12, 7, 1),
AC97_SINGLE("Beep to Side Tone Switch", AC97_PC_BEEP, 11, 1, 1),
AC97_SINGLE("Beep to Side Tone Volume", AC97_PC_BEEP, 8, 7, 1),
AC97_SINGLE("Beep to Phone Switch", AC97_PC_BEEP, 7, 1, 1),
AC97_SINGLE("Beep to Phone Volume", AC97_PC_BEEP, 4, 7, 1),

AC97_SINGLE("Aux to Headphone Switch", AC97_CD, 15, 1, 1),
AC97_SINGLE("Aux to Headphone Volume", AC97_CD, 12, 7, 1),
AC97_SINGLE("Aux to Side Tone Switch", AC97_CD, 11, 1, 1),
AC97_SINGLE("Aux to Side Tone Volume", AC97_CD, 8, 7, 1),
AC97_SINGLE("Aux to Phone Switch", AC97_CD, 7, 1, 1),
AC97_SINGLE("Aux to Phone Volume", AC97_CD, 4, 7, 1),

AC97_SINGLE("Phone to Headphone Switch", AC97_PHONE, 15, 1, 1),
AC97_SINGLE("Phone to Master Switch", AC97_PHONE, 14, 1, 1),

AC97_SINGLE("Line to Headphone Switch", AC97_LINE, 15, 1, 1),
AC97_SINGLE("Line to Master Switch", AC97_LINE, 14, 1, 1),
AC97_SINGLE("Line to Phone Switch", AC97_LINE, 13, 1, 1),

AC97_SINGLE("PCM Playback to Headphone Switch", AC97_PCM, 15, 1, 1),
AC97_SINGLE("PCM Playback to Master Switch", AC97_PCM, 14, 1, 1),
AC97_SINGLE("PCM Playback to Phone Switch", AC97_PCM, 13, 1, 1),

AC97_SINGLE("Capture 20dB Boost Switch", AC97_REC_SEL, 14, 1, 0),
AC97_ENUM("Capture to Phone Mux", wm9711_enum[4]),
AC97_SINGLE("Capture to Phone 20dB Boost Switch", AC97_REC_SEL, 11, 1, 1),
AC97_ENUM("Capture Select", wm9711_enum[8]),

AC97_SINGLE("3D Upper Cut-off Switch", AC97_3D_CONTROL, 5, 1, 1),
AC97_SINGLE("3D Lower Cut-off Switch", AC97_3D_CONTROL, 4, 1, 1),

AC97_ENUM("Bass Control", wm9711_enum[5]),
AC97_SINGLE("Bass Cut-off Switch", AC97_MASTER_TONE, 12, 1, 1),
AC97_SINGLE("Tone Cut-off Switch", AC97_MASTER_TONE, 4, 1, 1),
AC97_SINGLE("Playback Attenuate (-6dB) Switch", AC97_MASTER_TONE, 6, 1, 0),

AC97_SINGLE("ADC Switch", AC97_REC_GAIN, 15, 1, 1),
AC97_ENUM("Capture Volume Steps", wm9711_enum[6]),
AC97_DOUBLE("Capture Volume", AC97_REC_GAIN, 8, 0, 63, 1),
AC97_SINGLE("Capture ZC Switch", AC97_REC_GAIN, 7, 1, 0),

AC97_SINGLE("Mic 1 to Phone Switch", AC97_MIC, 14, 1, 1),
AC97_SINGLE("Mic 2 to Phone Switch", AC97_MIC, 13, 1, 1),
AC97_ENUM("Mic Select Source", wm9711_enum[7]),
AC97_SINGLE("Mic 1 Volume", AC97_MIC, 8, 31, 1),
AC97_SINGLE("Mic 2 Volume", AC97_MIC, 0, 31, 1),
AC97_SINGLE("Mic 20dB Boost Switch", AC97_MIC, 7, 1, 0),

AC97_SINGLE("Master Left Inv Switch", AC97_MASTER, 6, 1, 0),
AC97_SINGLE("Master ZC Switch", AC97_MASTER, 7, 1, 0),
AC97_SINGLE("Headphone ZC Switch", AC97_HEADPHONE, 7, 1, 0),
AC97_SINGLE("Mono ZC Switch", AC97_MASTER_MONO, 7, 1, 0),
};

static int patch_wolfson_wm9711_specific(struct snd_ac97 * ac97)
{
	int err, i;
	
	for (i = 0; i < ARRAY_SIZE(wm9711_snd_ac97_controls); i++) {
		if ((err = snd_ctl_add(ac97->bus->card, snd_ac97_cnew(&wm9711_snd_ac97_controls[i], ac97))) < 0)
			return err;
	}
	snd_ac97_write_cache(ac97,  AC97_CODEC_CLASS_REV, 0x0808);
	snd_ac97_write_cache(ac97,  AC97_PCI_SVID, 0x0808);
	snd_ac97_write_cache(ac97,  AC97_VIDEO, 0x0808);
	snd_ac97_write_cache(ac97,  AC97_AUX, 0x0808);
	snd_ac97_write_cache(ac97,  AC97_PC_BEEP, 0x0808);
	snd_ac97_write_cache(ac97,  AC97_CD, 0x0000);
	return 0;
}

static struct snd_ac97_build_ops patch_wolfson_wm9711_ops = {
	.build_specific = patch_wolfson_wm9711_specific,
};

static int patch_wolfson11(struct snd_ac97 * ac97)
{
	/* WM9711, WM9712 */
	ac97->build_ops = &patch_wolfson_wm9711_ops;

	ac97->flags |= AC97_HAS_NO_REC_GAIN | AC97_STEREO_MUTES | AC97_HAS_NO_MIC |
		AC97_HAS_NO_PC_BEEP | AC97_HAS_NO_VIDEO | AC97_HAS_NO_CD;
	
	return 0;
}

static const char* wm9713_mic_mixer[] = {"Stereo", "Mic 1", "Mic 2", "Mute"};
static const char* wm9713_rec_mux[] = {"Stereo", "Left", "Right", "Mute"};
static const char* wm9713_rec_src[] = 
	{"Mic 1", "Mic 2", "Line", "Mono In", "Headphone Mix", "Master Mix", 
	"Mono Mix", "Zh"};
static const char* wm9713_rec_gain[] = {"+1.5dB Steps", "+0.75dB Steps"};
static const char* wm9713_alc_select[] = {"None", "Left", "Right", "Stereo"};
static const char* wm9713_mono_pga[] = {"Vmid", "Zh", "Mono Mix", "Inv 1"};
static const char* wm9713_spk_pga[] = 
	{"Vmid", "Zh", "Headphone Mix", "Master Mix", "Inv", "NC", "NC", "NC"};
static const char* wm9713_hp_pga[] = {"Vmid", "Zh", "Headphone Mix", "NC"};
static const char* wm9713_out3_pga[] = {"Vmid", "Zh", "Inv 1", "NC"};
static const char* wm9713_out4_pga[] = {"Vmid", "Zh", "Inv 2", "NC"};
static const char* wm9713_dac_inv[] = 
	{"Off", "Mono Mix", "Master Mix", "Headphone Mix L", "Headphone Mix R", 
	"Headphone Mix Mono", "NC", "Vmid"};
static const char* wm9713_base[] = {"Linear Control", "Adaptive Boost"};
static const char* wm9713_ng_type[] = {"Constant Gain", "Mute"};

static const struct ac97_enum wm9713_enum[] = {
AC97_ENUM_SINGLE(AC97_LINE, 3, 4, wm9713_mic_mixer),
AC97_ENUM_SINGLE(AC97_VIDEO, 14, 4, wm9713_rec_mux),
AC97_ENUM_SINGLE(AC97_VIDEO, 9, 4, wm9713_rec_mux),
AC97_ENUM_DOUBLE(AC97_VIDEO, 3, 0, 8, wm9713_rec_src),
AC97_ENUM_DOUBLE(AC97_CD, 14, 6, 2, wm9713_rec_gain),
AC97_ENUM_SINGLE(AC97_PCI_SVID, 14, 4, wm9713_alc_select),
AC97_ENUM_SINGLE(AC97_REC_GAIN, 14, 4, wm9713_mono_pga),
AC97_ENUM_DOUBLE(AC97_REC_GAIN, 11, 8, 8, wm9713_spk_pga),
AC97_ENUM_DOUBLE(AC97_REC_GAIN, 6, 4, 4, wm9713_hp_pga),
AC97_ENUM_SINGLE(AC97_REC_GAIN, 2, 4, wm9713_out3_pga),
AC97_ENUM_SINGLE(AC97_REC_GAIN, 0, 4, wm9713_out4_pga),
AC97_ENUM_DOUBLE(AC97_REC_GAIN_MIC, 13, 10, 8, wm9713_dac_inv),
AC97_ENUM_SINGLE(AC97_GENERAL_PURPOSE, 15, 2, wm9713_base),
AC97_ENUM_SINGLE(AC97_PCI_SVID, 5, 2, wm9713_ng_type),
};

static const struct snd_kcontrol_new wm13_snd_ac97_controls[] = {
AC97_DOUBLE("Line In Volume", AC97_PC_BEEP, 8, 0, 31, 1),
AC97_SINGLE("Line In to Headphone Switch", AC97_PC_BEEP, 15, 1, 1),
AC97_SINGLE("Line In to Master Switch", AC97_PC_BEEP, 14, 1, 1),
AC97_SINGLE("Line In to Mono Switch", AC97_PC_BEEP, 13, 1, 1),

AC97_DOUBLE("PCM Playback Volume", AC97_PHONE, 8, 0, 31, 1),
AC97_SINGLE("PCM Playback to Headphone Switch", AC97_PHONE, 15, 1, 1),
AC97_SINGLE("PCM Playback to Master Switch", AC97_PHONE, 14, 1, 1),
AC97_SINGLE("PCM Playback to Mono Switch", AC97_PHONE, 13, 1, 1),

AC97_SINGLE("Mic 1 Volume", AC97_MIC, 8, 31, 1),
AC97_SINGLE("Mic 2 Volume", AC97_MIC, 0, 31, 1),
AC97_SINGLE("Mic 1 to Mono Switch", AC97_LINE, 7, 1, 1),
AC97_SINGLE("Mic 2 to Mono Switch", AC97_LINE, 6, 1, 1),
AC97_SINGLE("Mic Boost (+20dB) Switch", AC97_LINE, 5, 1, 0),
AC97_ENUM("Mic to Headphone Mux", wm9713_enum[0]),
AC97_SINGLE("Mic Headphone Mixer Volume", AC97_LINE, 0, 7, 1),

AC97_SINGLE("Capture Switch", AC97_CD, 15, 1, 1),
AC97_ENUM("Capture Volume Steps", wm9713_enum[4]),
AC97_DOUBLE("Capture Volume", AC97_CD, 8, 0, 15, 0),
AC97_SINGLE("Capture ZC Switch", AC97_CD, 7, 1, 0),

AC97_ENUM("Capture to Headphone Mux", wm9713_enum[1]),
AC97_SINGLE("Capture to Headphone Volume", AC97_VIDEO, 11, 7, 1),
AC97_ENUM("Capture to Mono Mux", wm9713_enum[2]),
AC97_SINGLE("Capture to Mono Boost (+20dB) Switch", AC97_VIDEO, 8, 1, 0),
AC97_SINGLE("Capture ADC Boost (+20dB) Switch", AC97_VIDEO, 6, 1, 0),
AC97_ENUM("Capture Select", wm9713_enum[3]),

AC97_SINGLE("ALC Target Volume", AC97_CODEC_CLASS_REV, 12, 15, 0),
AC97_SINGLE("ALC Hold Time", AC97_CODEC_CLASS_REV, 8, 15, 0),
AC97_SINGLE("ALC Decay Time ", AC97_CODEC_CLASS_REV, 4, 15, 0),
AC97_SINGLE("ALC Attack Time", AC97_CODEC_CLASS_REV, 0, 15, 0),
AC97_ENUM("ALC Function", wm9713_enum[5]),
AC97_SINGLE("ALC Max Volume", AC97_PCI_SVID, 11, 7, 0),
AC97_SINGLE("ALC ZC Timeout", AC97_PCI_SVID, 9, 3, 0),
AC97_SINGLE("ALC ZC Switch", AC97_PCI_SVID, 8, 1, 0),
AC97_SINGLE("ALC NG Switch", AC97_PCI_SVID, 7, 1, 0),
AC97_ENUM("ALC NG Type", wm9713_enum[13]),
AC97_SINGLE("ALC NG Threshold", AC97_PCI_SVID, 0, 31, 0),

AC97_DOUBLE("Master ZC Switch", AC97_MASTER, 14, 6, 1, 0),
AC97_DOUBLE("Headphone ZC Switch", AC97_HEADPHONE, 14, 6, 1, 0),
AC97_DOUBLE("Out3/4 ZC Switch", AC97_MASTER_MONO, 14, 6, 1, 0),
AC97_SINGLE("Master Right Switch", AC97_MASTER, 7, 1, 1),
AC97_SINGLE("Headphone Right Switch", AC97_HEADPHONE, 7, 1, 1),
AC97_SINGLE("Out3/4 Right Switch", AC97_MASTER_MONO, 7, 1, 1),

AC97_SINGLE("Mono In to Headphone Switch", AC97_MASTER_TONE, 15, 1, 1),
AC97_SINGLE("Mono In to Master Switch", AC97_MASTER_TONE, 14, 1, 1),
AC97_SINGLE("Mono In Volume", AC97_MASTER_TONE, 8, 31, 1),
AC97_SINGLE("Mono Switch", AC97_MASTER_TONE, 7, 1, 1),
AC97_SINGLE("Mono ZC Switch", AC97_MASTER_TONE, 6, 1, 0),
AC97_SINGLE("Mono Volume", AC97_MASTER_TONE, 0, 31, 1),

AC97_SINGLE("PC Beep to Headphone Switch", AC97_AUX, 15, 1, 1),
AC97_SINGLE("PC Beep to Headphone Volume", AC97_AUX, 12, 7, 1),
AC97_SINGLE("PC Beep to Master Switch", AC97_AUX, 11, 1, 1),
AC97_SINGLE("PC Beep to Master Volume", AC97_AUX, 8, 7, 1),
AC97_SINGLE("PC Beep to Mono Switch", AC97_AUX, 7, 1, 1),
AC97_SINGLE("PC Beep to Mono Volume", AC97_AUX, 4, 7, 1),

AC97_SINGLE("Voice to Headphone Switch", AC97_PCM, 15, 1, 1),
AC97_SINGLE("Voice to Headphone Volume", AC97_PCM, 12, 7, 1),
AC97_SINGLE("Voice to Master Switch", AC97_PCM, 11, 1, 1),
AC97_SINGLE("Voice to Master Volume", AC97_PCM, 8, 7, 1),
AC97_SINGLE("Voice to Mono Switch", AC97_PCM, 7, 1, 1),
AC97_SINGLE("Voice to Mono Volume", AC97_PCM, 4, 7, 1),

AC97_SINGLE("Aux to Headphone Switch", AC97_REC_SEL, 15, 1, 1),
AC97_SINGLE("Aux to Headphone Volume", AC97_REC_SEL, 12, 7, 1),
AC97_SINGLE("Aux to Master Switch", AC97_REC_SEL, 11, 1, 1),
AC97_SINGLE("Aux to Master Volume", AC97_REC_SEL, 8, 7, 1),
AC97_SINGLE("Aux to Mono Switch", AC97_REC_SEL, 7, 1, 1),
AC97_SINGLE("Aux to Mono Volume", AC97_REC_SEL, 4, 7, 1),

AC97_ENUM("Mono Input Mux", wm9713_enum[6]),
AC97_ENUM("Master Input Mux", wm9713_enum[7]),
AC97_ENUM("Headphone Input Mux", wm9713_enum[8]),
AC97_ENUM("Out 3 Input Mux", wm9713_enum[9]),
AC97_ENUM("Out 4 Input Mux", wm9713_enum[10]),

AC97_ENUM("Bass Control", wm9713_enum[12]),
AC97_SINGLE("Bass Cut-off Switch", AC97_GENERAL_PURPOSE, 12, 1, 1),
AC97_SINGLE("Tone Cut-off Switch", AC97_GENERAL_PURPOSE, 4, 1, 1),
AC97_SINGLE("Playback Attenuate (-6dB) Switch", AC97_GENERAL_PURPOSE, 6, 1, 0),
AC97_SINGLE("Bass Volume", AC97_GENERAL_PURPOSE, 8, 15, 1),
AC97_SINGLE("Tone Volume", AC97_GENERAL_PURPOSE, 0, 15, 1),
};

static const struct snd_kcontrol_new wm13_snd_ac97_controls_3d[] = {
AC97_ENUM("Inv Input Mux", wm9713_enum[11]),
AC97_SINGLE("3D Upper Cut-off Switch", AC97_REC_GAIN_MIC, 5, 1, 0),
AC97_SINGLE("3D Lower Cut-off Switch", AC97_REC_GAIN_MIC, 4, 1, 0),
AC97_SINGLE("3D Depth", AC97_REC_GAIN_MIC, 0, 15, 1),
};

static int patch_wolfson_wm9713_3d (struct snd_ac97 * ac97)
{
	int err, i;
    
	for (i = 0; i < ARRAY_SIZE(wm13_snd_ac97_controls_3d); i++) {
		if ((err = snd_ctl_add(ac97->bus->card, snd_ac97_cnew(&wm13_snd_ac97_controls_3d[i], ac97))) < 0)
			return err;
	}
	return 0;
}

static int patch_wolfson_wm9713_specific(struct snd_ac97 * ac97)
{
	int err, i;
	
	for (i = 0; i < ARRAY_SIZE(wm13_snd_ac97_controls); i++) {
		if ((err = snd_ctl_add(ac97->bus->card, snd_ac97_cnew(&wm13_snd_ac97_controls[i], ac97))) < 0)
			return err;
	}
	snd_ac97_write_cache(ac97, AC97_PC_BEEP, 0x0808);
	snd_ac97_write_cache(ac97, AC97_PHONE, 0x0808);
	snd_ac97_write_cache(ac97, AC97_MIC, 0x0808);
	snd_ac97_write_cache(ac97, AC97_LINE, 0x00da);
	snd_ac97_write_cache(ac97, AC97_CD, 0x0808);
	snd_ac97_write_cache(ac97, AC97_VIDEO, 0xd612);
	snd_ac97_write_cache(ac97, AC97_REC_GAIN, 0x1ba0);
	return 0;
}

#ifdef CONFIG_PM
static void patch_wolfson_wm9713_suspend (struct snd_ac97 * ac97)
{
	snd_ac97_write_cache(ac97, AC97_EXTENDED_MID, 0xfeff);
	snd_ac97_write_cache(ac97, AC97_EXTENDED_MSTATUS, 0xffff);
}

static void patch_wolfson_wm9713_resume (struct snd_ac97 * ac97)
{
	snd_ac97_write_cache(ac97, AC97_EXTENDED_MID, 0xda00);
	snd_ac97_write_cache(ac97, AC97_EXTENDED_MSTATUS, 0x3810);
	snd_ac97_write_cache(ac97, AC97_POWERDOWN, 0x0);
}
#endif

static struct snd_ac97_build_ops patch_wolfson_wm9713_ops = {
	.build_specific = patch_wolfson_wm9713_specific,
	.build_3d = patch_wolfson_wm9713_3d,
#ifdef CONFIG_PM	
	.suspend = patch_wolfson_wm9713_suspend,
	.resume = patch_wolfson_wm9713_resume
#endif
};

static int patch_wolfson13(struct snd_ac97 * ac97)
{
	/* WM9713, WM9714 */
	ac97->build_ops = &patch_wolfson_wm9713_ops;

	ac97->flags |= AC97_HAS_NO_REC_GAIN | AC97_STEREO_MUTES | AC97_HAS_NO_PHONE |
		AC97_HAS_NO_PC_BEEP | AC97_HAS_NO_VIDEO | AC97_HAS_NO_CD | AC97_HAS_NO_TONE |
		AC97_HAS_NO_STD_PCM;
    	ac97->scaps &= ~AC97_SCAP_MODEM;

	snd_ac97_write_cache(ac97, AC97_EXTENDED_MID, 0xda00);
	snd_ac97_write_cache(ac97, AC97_EXTENDED_MSTATUS, 0x3810);
	snd_ac97_write_cache(ac97, AC97_POWERDOWN, 0x0);

	return 0;
}

/*
 * Tritech codec
 */
static int patch_tritech_tr28028(struct snd_ac97 * ac97)
{
	snd_ac97_write_cache(ac97, 0x26, 0x0300);
	snd_ac97_write_cache(ac97, 0x26, 0x0000);
	snd_ac97_write_cache(ac97, AC97_SURROUND_MASTER, 0x0000);
	snd_ac97_write_cache(ac97, AC97_SPDIF, 0x0000);
	return 0;
}

/*
 * Sigmatel STAC97xx codecs
 */
static int patch_sigmatel_stac9700_3d(struct snd_ac97 * ac97)
{
	struct snd_kcontrol *kctl;
	int err;

	if ((err = snd_ctl_add(ac97->bus->card, kctl = snd_ac97_cnew(&snd_ac97_controls_3d[0], ac97))) < 0)
		return err;
	strcpy(kctl->id.name, "3D Control Sigmatel - Depth");
	kctl->private_value = AC97_SINGLE_VALUE(AC97_3D_CONTROL, 2, 3, 0);
	snd_ac97_write_cache(ac97, AC97_3D_CONTROL, 0x0000);
	return 0;
}

static int patch_sigmatel_stac9708_3d(struct snd_ac97 * ac97)
{
	struct snd_kcontrol *kctl;
	int err;

	if ((err = snd_ctl_add(ac97->bus->card, kctl = snd_ac97_cnew(&snd_ac97_controls_3d[0], ac97))) < 0)
		return err;
	strcpy(kctl->id.name, "3D Control Sigmatel - Depth");
	kctl->private_value = AC97_SINGLE_VALUE(AC97_3D_CONTROL, 0, 3, 0);
	if ((err = snd_ctl_add(ac97->bus->card, kctl = snd_ac97_cnew(&snd_ac97_controls_3d[0], ac97))) < 0)
		return err;
	strcpy(kctl->id.name, "3D Control Sigmatel - Rear Depth");
	kctl->private_value = AC97_SINGLE_VALUE(AC97_3D_CONTROL, 2, 3, 0);
	snd_ac97_write_cache(ac97, AC97_3D_CONTROL, 0x0000);
	return 0;
}

static const struct snd_kcontrol_new snd_ac97_sigmatel_4speaker =
AC97_SINGLE("Sigmatel 4-Speaker Stereo Playback Switch",
		AC97_SIGMATEL_DAC2INVERT, 2, 1, 0);

/* "Sigmatel " removed due to excessive name length: */
static const struct snd_kcontrol_new snd_ac97_sigmatel_phaseinvert =
AC97_SINGLE("Surround Phase Inversion Playback Switch",
		AC97_SIGMATEL_DAC2INVERT, 3, 1, 0);

static const struct snd_kcontrol_new snd_ac97_sigmatel_controls[] = {
AC97_SINGLE("Sigmatel DAC 6dB Attenuate", AC97_SIGMATEL_ANALOG, 1, 1, 0),
AC97_SINGLE("Sigmatel ADC 6dB Attenuate", AC97_SIGMATEL_ANALOG, 0, 1, 0)
};

static int patch_sigmatel_stac97xx_specific(struct snd_ac97 * ac97)
{
	int err;

	snd_ac97_write_cache(ac97, AC97_SIGMATEL_ANALOG, snd_ac97_read(ac97, AC97_SIGMATEL_ANALOG) & ~0x0003);
	if (snd_ac97_try_bit(ac97, AC97_SIGMATEL_ANALOG, 1))
		if ((err = patch_build_controls(ac97, &snd_ac97_sigmatel_controls[0], 1)) < 0)
			return err;
	if (snd_ac97_try_bit(ac97, AC97_SIGMATEL_ANALOG, 0))
		if ((err = patch_build_controls(ac97, &snd_ac97_sigmatel_controls[1], 1)) < 0)
			return err;
	if (snd_ac97_try_bit(ac97, AC97_SIGMATEL_DAC2INVERT, 2))
		if ((err = patch_build_controls(ac97, &snd_ac97_sigmatel_4speaker, 1)) < 0)
			return err;
	if (snd_ac97_try_bit(ac97, AC97_SIGMATEL_DAC2INVERT, 3))
		if ((err = patch_build_controls(ac97, &snd_ac97_sigmatel_phaseinvert, 1)) < 0)
			return err;
	return 0;
}

static struct snd_ac97_build_ops patch_sigmatel_stac9700_ops = {
	.build_3d	= patch_sigmatel_stac9700_3d,
	.build_specific	= patch_sigmatel_stac97xx_specific
};

static int patch_sigmatel_stac9700(struct snd_ac97 * ac97)
{
	ac97->build_ops = &patch_sigmatel_stac9700_ops;
	return 0;
}

static int snd_ac97_stac9708_put_bias(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ac97 *ac97 = snd_kcontrol_chip(kcontrol);
	int err;

	mutex_lock(&ac97->page_mutex);
	snd_ac97_write(ac97, AC97_SIGMATEL_BIAS1, 0xabba);
	err = snd_ac97_update_bits(ac97, AC97_SIGMATEL_BIAS2, 0x0010,
				   (ucontrol->value.integer.value[0] & 1) << 4);
	snd_ac97_write(ac97, AC97_SIGMATEL_BIAS1, 0);
	mutex_unlock(&ac97->page_mutex);
	return err;
}

static const struct snd_kcontrol_new snd_ac97_stac9708_bias_control = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "Sigmatel Output Bias Switch",
	.info = snd_ac97_info_volsw,
	.get = snd_ac97_get_volsw,
	.put = snd_ac97_stac9708_put_bias,
	.private_value = AC97_SINGLE_VALUE(AC97_SIGMATEL_BIAS2, 4, 1, 0),
};

static int patch_sigmatel_stac9708_specific(struct snd_ac97 *ac97)
{
	int err;

	/* the register bit is writable, but the function is not implemented: */
	snd_ac97_remove_ctl(ac97, "PCM Out Path & Mute", NULL);

	snd_ac97_rename_vol_ctl(ac97, "Headphone Playback", "Sigmatel Surround Playback");
	if ((err = patch_build_controls(ac97, &snd_ac97_stac9708_bias_control, 1)) < 0)
		return err;
	return patch_sigmatel_stac97xx_specific(ac97);
}

static struct snd_ac97_build_ops patch_sigmatel_stac9708_ops = {
	.build_3d	= patch_sigmatel_stac9708_3d,
	.build_specific	= patch_sigmatel_stac9708_specific
};

static int patch_sigmatel_stac9708(struct snd_ac97 * ac97)
{
	unsigned int codec72, codec6c;

	ac97->build_ops = &patch_sigmatel_stac9708_ops;
	ac97->caps |= 0x10;	/* HP (sigmatel surround) support */

	codec72 = snd_ac97_read(ac97, AC97_SIGMATEL_BIAS2) & 0x8000;
	codec6c = snd_ac97_read(ac97, AC97_SIGMATEL_ANALOG);

	if ((codec72==0) && (codec6c==0)) {
		snd_ac97_write_cache(ac97, AC97_SIGMATEL_CIC1, 0xabba);
		snd_ac97_write_cache(ac97, AC97_SIGMATEL_CIC2, 0x1000);
		snd_ac97_write_cache(ac97, AC97_SIGMATEL_BIAS1, 0xabba);
		snd_ac97_write_cache(ac97, AC97_SIGMATEL_BIAS2, 0x0007);
	} else if ((codec72==0x8000) && (codec6c==0)) {
		snd_ac97_write_cache(ac97, AC97_SIGMATEL_CIC1, 0xabba);
		snd_ac97_write_cache(ac97, AC97_SIGMATEL_CIC2, 0x1001);
		snd_ac97_write_cache(ac97, AC97_SIGMATEL_DAC2INVERT, 0x0008);
	} else if ((codec72==0x8000) && (codec6c==0x0080)) {
		/* nothing */
	}
	snd_ac97_write_cache(ac97, AC97_SIGMATEL_MULTICHN, 0x0000);
	return 0;
}

static int patch_sigmatel_stac9721(struct snd_ac97 * ac97)
{
	ac97->build_ops = &patch_sigmatel_stac9700_ops;
	if (snd_ac97_read(ac97, AC97_SIGMATEL_ANALOG) == 0) {
		// patch for SigmaTel
		snd_ac97_write_cache(ac97, AC97_SIGMATEL_CIC1, 0xabba);
		snd_ac97_write_cache(ac97, AC97_SIGMATEL_CIC2, 0x4000);
		snd_ac97_write_cache(ac97, AC97_SIGMATEL_BIAS1, 0xabba);
		snd_ac97_write_cache(ac97, AC97_SIGMATEL_BIAS2, 0x0002);
	}
	snd_ac97_write_cache(ac97, AC97_SIGMATEL_MULTICHN, 0x0000);
	return 0;
}

static int patch_sigmatel_stac9744(struct snd_ac97 * ac97)
{
	// patch for SigmaTel
	ac97->build_ops = &patch_sigmatel_stac9700_ops;
	snd_ac97_write_cache(ac97, AC97_SIGMATEL_CIC1, 0xabba);
	snd_ac97_write_cache(ac97, AC97_SIGMATEL_CIC2, 0x0000);	/* is this correct? --jk */
	snd_ac97_write_cache(ac97, AC97_SIGMATEL_BIAS1, 0xabba);
	snd_ac97_write_cache(ac97, AC97_SIGMATEL_BIAS2, 0x0002);
	snd_ac97_write_cache(ac97, AC97_SIGMATEL_MULTICHN, 0x0000);
	return 0;
}

static int patch_sigmatel_stac9756(struct snd_ac97 * ac97)
{
	// patch for SigmaTel
	ac97->build_ops = &patch_sigmatel_stac9700_ops;
	snd_ac97_write_cache(ac97, AC97_SIGMATEL_CIC1, 0xabba);
	snd_ac97_write_cache(ac97, AC97_SIGMATEL_CIC2, 0x0000);	/* is this correct? --jk */
	snd_ac97_write_cache(ac97, AC97_SIGMATEL_BIAS1, 0xabba);
	snd_ac97_write_cache(ac97, AC97_SIGMATEL_BIAS2, 0x0002);
	snd_ac97_write_cache(ac97, AC97_SIGMATEL_MULTICHN, 0x0000);
	return 0;
}

static int snd_ac97_stac9758_output_jack_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	static char *texts[5] = { "Input/Disabled", "Front Output",
		"Rear Output", "Center/LFE Output", "Mixer Output" };

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = 5;
	if (uinfo->value.enumerated.item > 4)
		uinfo->value.enumerated.item = 4;
	strcpy(uinfo->value.enumerated.name, texts[uinfo->value.enumerated.item]);
	return 0;
}

static int snd_ac97_stac9758_output_jack_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ac97 *ac97 = snd_kcontrol_chip(kcontrol);
	int shift = kcontrol->private_value;
	unsigned short val;

	val = ac97->regs[AC97_SIGMATEL_OUTSEL] >> shift;
	if (!(val & 4))
		ucontrol->value.enumerated.item[0] = 0;
	else
		ucontrol->value.enumerated.item[0] = 1 + (val & 3);
	return 0;
}

static int snd_ac97_stac9758_output_jack_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ac97 *ac97 = snd_kcontrol_chip(kcontrol);
	int shift = kcontrol->private_value;
	unsigned short val;

	if (ucontrol->value.enumerated.item[0] > 4)
		return -EINVAL;
	if (ucontrol->value.enumerated.item[0] == 0)
		val = 0;
	else
		val = 4 | (ucontrol->value.enumerated.item[0] - 1);
	return ac97_update_bits_page(ac97, AC97_SIGMATEL_OUTSEL,
				     7 << shift, val << shift, 0);
}

static int snd_ac97_stac9758_input_jack_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	static char *texts[7] = { "Mic2 Jack", "Mic1 Jack", "Line In Jack",
		"Front Jack", "Rear Jack", "Center/LFE Jack", "Mute" };

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = 7;
	if (uinfo->value.enumerated.item > 6)
		uinfo->value.enumerated.item = 6;
	strcpy(uinfo->value.enumerated.name, texts[uinfo->value.enumerated.item]);
	return 0;
}

static int snd_ac97_stac9758_input_jack_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ac97 *ac97 = snd_kcontrol_chip(kcontrol);
	int shift = kcontrol->private_value;
	unsigned short val;

	val = ac97->regs[AC97_SIGMATEL_INSEL];
	ucontrol->value.enumerated.item[0] = (val >> shift) & 7;
	return 0;
}

static int snd_ac97_stac9758_input_jack_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ac97 *ac97 = snd_kcontrol_chip(kcontrol);
	int shift = kcontrol->private_value;

	return ac97_update_bits_page(ac97, AC97_SIGMATEL_INSEL, 7 << shift,
				     ucontrol->value.enumerated.item[0] << shift, 0);
}

static int snd_ac97_stac9758_phonesel_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	static char *texts[3] = { "None", "Front Jack", "Rear Jack" };

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = 3;
	if (uinfo->value.enumerated.item > 2)
		uinfo->value.enumerated.item = 2;
	strcpy(uinfo->value.enumerated.name, texts[uinfo->value.enumerated.item]);
	return 0;
}

static int snd_ac97_stac9758_phonesel_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ac97 *ac97 = snd_kcontrol_chip(kcontrol);

	ucontrol->value.enumerated.item[0] = ac97->regs[AC97_SIGMATEL_IOMISC] & 3;
	return 0;
}

static int snd_ac97_stac9758_phonesel_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ac97 *ac97 = snd_kcontrol_chip(kcontrol);

	return ac97_update_bits_page(ac97, AC97_SIGMATEL_IOMISC, 3,
				     ucontrol->value.enumerated.item[0], 0);
}

#define STAC9758_OUTPUT_JACK(xname, shift) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.info = snd_ac97_stac9758_output_jack_info, \
	.get = snd_ac97_stac9758_output_jack_get, \
	.put = snd_ac97_stac9758_output_jack_put, \
	.private_value = shift }
#define STAC9758_INPUT_JACK(xname, shift) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.info = snd_ac97_stac9758_input_jack_info, \
	.get = snd_ac97_stac9758_input_jack_get, \
	.put = snd_ac97_stac9758_input_jack_put, \
	.private_value = shift }
static const struct snd_kcontrol_new snd_ac97_sigmatel_stac9758_controls[] = {
	STAC9758_OUTPUT_JACK("Mic1 Jack", 1),
	STAC9758_OUTPUT_JACK("LineIn Jack", 4),
	STAC9758_OUTPUT_JACK("Front Jack", 7),
	STAC9758_OUTPUT_JACK("Rear Jack", 10),
	STAC9758_OUTPUT_JACK("Center/LFE Jack", 13),
	STAC9758_INPUT_JACK("Mic Input Source", 0),
	STAC9758_INPUT_JACK("Line Input Source", 8),
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Headphone Amp",
		.info = snd_ac97_stac9758_phonesel_info,
		.get = snd_ac97_stac9758_phonesel_get,
		.put = snd_ac97_stac9758_phonesel_put
	},
	AC97_SINGLE("Exchange Center/LFE", AC97_SIGMATEL_IOMISC, 4, 1, 0),
	AC97_SINGLE("Headphone +3dB Boost", AC97_SIGMATEL_IOMISC, 8, 1, 0)
};

static int patch_sigmatel_stac9758_specific(struct snd_ac97 *ac97)
{
	int err;

	err = patch_sigmatel_stac97xx_specific(ac97);
	if (err < 0)
		return err;
	err = patch_build_controls(ac97, snd_ac97_sigmatel_stac9758_controls,
				   ARRAY_SIZE(snd_ac97_sigmatel_stac9758_controls));
	if (err < 0)
		return err;
	/* DAC-A direct */
	snd_ac97_rename_vol_ctl(ac97, "Headphone Playback", "Front Playback");
	/* DAC-A to Mix = PCM */
	/* DAC-B direct = Surround */
	/* DAC-B to Mix */
	snd_ac97_rename_vol_ctl(ac97, "Video Playback", "Surround Mix Playback");
	/* DAC-C direct = Center/LFE */

	return 0;
}

static struct snd_ac97_build_ops patch_sigmatel_stac9758_ops = {
	.build_3d	= patch_sigmatel_stac9700_3d,
	.build_specific	= patch_sigmatel_stac9758_specific
};

static int patch_sigmatel_stac9758(struct snd_ac97 * ac97)
{
	static unsigned short regs[4] = {
		AC97_SIGMATEL_OUTSEL,
		AC97_SIGMATEL_IOMISC,
		AC97_SIGMATEL_INSEL,
		AC97_SIGMATEL_VARIOUS
	};
	static unsigned short def_regs[4] = {
		/* OUTSEL */ 0xd794, /* CL:CL, SR:SR, LO:MX, LI:DS, MI:DS */
		/* IOMISC */ 0x2001,
		/* INSEL */ 0x0201, /* LI:LI, MI:M1 */
		/* VARIOUS */ 0x0040
	};
	static unsigned short m675_regs[4] = {
		/* OUTSEL */ 0xfc70, /* CL:MX, SR:MX, LO:DS, LI:MX, MI:DS */
		/* IOMISC */ 0x2102, /* HP amp on */
		/* INSEL */ 0x0203, /* LI:LI, MI:FR */
		/* VARIOUS */ 0x0041 /* stereo mic */
	};
	unsigned short *pregs = def_regs;
	int i;

	/* Gateway M675 notebook */
	if (ac97->pci && 
	    ac97->subsystem_vendor == 0x107b &&
	    ac97->subsystem_device == 0x0601)
	    	pregs = m675_regs;

	// patch for SigmaTel
	ac97->build_ops = &patch_sigmatel_stac9758_ops;
	/* FIXME: assume only page 0 for writing cache */
	snd_ac97_update_bits(ac97, AC97_INT_PAGING, AC97_PAGE_MASK, AC97_PAGE_VENDOR);
	for (i = 0; i < 4; i++)
		snd_ac97_write_cache(ac97, regs[i], pregs[i]);

	ac97->flags |= AC97_STEREO_MUTES;
	return 0;
}

/*
 * Cirrus Logic CS42xx codecs
 */
static const struct snd_kcontrol_new snd_ac97_cirrus_controls_spdif[2] = {
	AC97_SINGLE(SNDRV_CTL_NAME_IEC958("",PLAYBACK,SWITCH), AC97_CSR_SPDIF, 15, 1, 0),
	AC97_SINGLE(SNDRV_CTL_NAME_IEC958("",PLAYBACK,NONE) "AC97-SPSA", AC97_CSR_ACMODE, 0, 3, 0)
};

static int patch_cirrus_build_spdif(struct snd_ac97 * ac97)
{
	int err;

	/* con mask, pro mask, default */
	if ((err = patch_build_controls(ac97, &snd_ac97_controls_spdif[0], 3)) < 0)
		return err;
	/* switch, spsa */
	if ((err = patch_build_controls(ac97, &snd_ac97_cirrus_controls_spdif[0], 1)) < 0)
		return err;
	switch (ac97->id & AC97_ID_CS_MASK) {
	case AC97_ID_CS4205:
		if ((err = patch_build_controls(ac97, &snd_ac97_cirrus_controls_spdif[1], 1)) < 0)
			return err;
		break;
	}
	/* set default PCM S/PDIF params */
	/* consumer,PCM audio,no copyright,no preemphasis,PCM coder,original,48000Hz */
	snd_ac97_write_cache(ac97, AC97_CSR_SPDIF, 0x0a20);
	return 0;
}

static struct snd_ac97_build_ops patch_cirrus_ops = {
	.build_spdif = patch_cirrus_build_spdif
};

static int patch_cirrus_spdif(struct snd_ac97 * ac97)
{
	/* Basically, the cs4201/cs4205/cs4297a has non-standard sp/dif registers.
	   WHY CAN'T ANYONE FOLLOW THE BLOODY SPEC?  *sigh*
	   - sp/dif EA ID is not set, but sp/dif is always present.
	   - enable/disable is spdif register bit 15.
	   - sp/dif control register is 0x68.  differs from AC97:
	   - valid is bit 14 (vs 15)
	   - no DRS
	   - only 44.1/48k [00 = 48, 01=44,1] (AC97 is 00=44.1, 10=48)
	   - sp/dif ssource select is in 0x5e bits 0,1.
	*/

	ac97->build_ops = &patch_cirrus_ops;
	ac97->flags |= AC97_CS_SPDIF; 
	ac97->rates[AC97_RATES_SPDIF] &= ~SNDRV_PCM_RATE_32000;
        ac97->ext_id |= AC97_EI_SPDIF;	/* force the detection of spdif */
	snd_ac97_write_cache(ac97, AC97_CSR_ACMODE, 0x0080);
	return 0;
}

static int patch_cirrus_cs4299(struct snd_ac97 * ac97)
{
	/* force the detection of PC Beep */
	ac97->flags |= AC97_HAS_PC_BEEP;
	
	return patch_cirrus_spdif(ac97);
}

/*
 * Conexant codecs
 */
static const struct snd_kcontrol_new snd_ac97_conexant_controls_spdif[1] = {
	AC97_SINGLE(SNDRV_CTL_NAME_IEC958("",PLAYBACK,SWITCH), AC97_CXR_AUDIO_MISC, 3, 1, 0),
};

static int patch_conexant_build_spdif(struct snd_ac97 * ac97)
{
	int err;

	/* con mask, pro mask, default */
	if ((err = patch_build_controls(ac97, &snd_ac97_controls_spdif[0], 3)) < 0)
		return err;
	/* switch */
	if ((err = patch_build_controls(ac97, &snd_ac97_conexant_controls_spdif[0], 1)) < 0)
		return err;
	/* set default PCM S/PDIF params */
	/* consumer,PCM audio,no copyright,no preemphasis,PCM coder,original,48000Hz */
	snd_ac97_write_cache(ac97, AC97_CXR_AUDIO_MISC,
			     snd_ac97_read(ac97, AC97_CXR_AUDIO_MISC) & ~(AC97_CXR_SPDIFEN|AC97_CXR_COPYRGT|AC97_CXR_SPDIF_MASK));
	return 0;
}

static struct snd_ac97_build_ops patch_conexant_ops = {
	.build_spdif = patch_conexant_build_spdif
};

static int patch_conexant(struct snd_ac97 * ac97)
{
	ac97->build_ops = &patch_conexant_ops;
	ac97->flags |= AC97_CX_SPDIF;
        ac97->ext_id |= AC97_EI_SPDIF;	/* force the detection of spdif */
	ac97->rates[AC97_RATES_SPDIF] = SNDRV_PCM_RATE_48000; /* 48k only */
	return 0;
}

static int patch_cx20551(struct snd_ac97 *ac97)
{
	snd_ac97_update_bits(ac97, 0x5c, 0x01, 0x01);
	return 0;
}

/*
 * Analog Device AD18xx, AD19xx codecs
 */
#ifdef CONFIG_PM
static void ad18xx_resume(struct snd_ac97 *ac97)
{
	static unsigned short setup_regs[] = {
		AC97_AD_MISC, AC97_AD_SERIAL_CFG, AC97_AD_JACK_SPDIF,
	};
	int i, codec;

	for (i = 0; i < (int)ARRAY_SIZE(setup_regs); i++) {
		unsigned short reg = setup_regs[i];
		if (test_bit(reg, ac97->reg_accessed)) {
			snd_ac97_write(ac97, reg, ac97->regs[reg]);
			snd_ac97_read(ac97, reg);
		}
	}

	if (! (ac97->flags & AC97_AD_MULTI))
		/* normal restore */
		snd_ac97_restore_status(ac97);
	else {
		/* restore the AD18xx codec configurations */
		for (codec = 0; codec < 3; codec++) {
			if (! ac97->spec.ad18xx.id[codec])
				continue;
			/* select single codec */
			snd_ac97_update_bits(ac97, AC97_AD_SERIAL_CFG, 0x7000,
					     ac97->spec.ad18xx.unchained[codec] | ac97->spec.ad18xx.chained[codec]);
			ac97->bus->ops->write(ac97, AC97_AD_CODEC_CFG, ac97->spec.ad18xx.codec_cfg[codec]);
		}
		/* select all codecs */
		snd_ac97_update_bits(ac97, AC97_AD_SERIAL_CFG, 0x7000, 0x7000);

		/* restore status */
		for (i = 2; i < 0x7c ; i += 2) {
			if (i == AC97_POWERDOWN || i == AC97_EXTENDED_ID)
				continue;
			if (test_bit(i, ac97->reg_accessed)) {
				/* handle multi codecs for AD18xx */
				if (i == AC97_PCM) {
					for (codec = 0; codec < 3; codec++) {
						if (! ac97->spec.ad18xx.id[codec])
							continue;
						/* select single codec */
						snd_ac97_update_bits(ac97, AC97_AD_SERIAL_CFG, 0x7000,
								     ac97->spec.ad18xx.unchained[codec] | ac97->spec.ad18xx.chained[codec]);
						/* update PCM bits */
						ac97->bus->ops->write(ac97, AC97_PCM, ac97->spec.ad18xx.pcmreg[codec]);
					}
					/* select all codecs */
					snd_ac97_update_bits(ac97, AC97_AD_SERIAL_CFG, 0x7000, 0x7000);
					continue;
				} else if (i == AC97_AD_TEST ||
					   i == AC97_AD_CODEC_CFG ||
					   i == AC97_AD_SERIAL_CFG)
					continue; /* ignore */
			}
			snd_ac97_write(ac97, i, ac97->regs[i]);
			snd_ac97_read(ac97, i);
		}
	}

	snd_ac97_restore_iec958(ac97);
}

static void ad1888_resume(struct snd_ac97 *ac97)
{
	ad18xx_resume(ac97);
	snd_ac97_write_cache(ac97, AC97_CODEC_CLASS_REV, 0x8080);
}

#endif

static const struct snd_ac97_res_table ad1819_restbl[] = {
	{ AC97_PHONE, 0x9f1f },
	{ AC97_MIC, 0x9f1f },
	{ AC97_LINE, 0x9f1f },
	{ AC97_CD, 0x9f1f },
	{ AC97_VIDEO, 0x9f1f },
	{ AC97_AUX, 0x9f1f },
	{ AC97_PCM, 0x9f1f },
	{ } /* terminator */
};

static int patch_ad1819(struct snd_ac97 * ac97)
{
	unsigned short scfg;

	// patch for Analog Devices
	scfg = snd_ac97_read(ac97, AC97_AD_SERIAL_CFG);
	snd_ac97_write_cache(ac97, AC97_AD_SERIAL_CFG, scfg | 0x7000); /* select all codecs */
	ac97->res_table = ad1819_restbl;
	return 0;
}

static unsigned short patch_ad1881_unchained(struct snd_ac97 * ac97, int idx, unsigned short mask)
{
	unsigned short val;

	// test for unchained codec
	snd_ac97_update_bits(ac97, AC97_AD_SERIAL_CFG, 0x7000, mask);
	snd_ac97_write_cache(ac97, AC97_AD_CODEC_CFG, 0x0000);	/* ID0C, ID1C, SDIE = off */
	val = snd_ac97_read(ac97, AC97_VENDOR_ID2);
	if ((val & 0xff40) != 0x5340)
		return 0;
	ac97->spec.ad18xx.unchained[idx] = mask;
	ac97->spec.ad18xx.id[idx] = val;
	ac97->spec.ad18xx.codec_cfg[idx] = 0x0000;
	return mask;
}

static int patch_ad1881_chained1(struct snd_ac97 * ac97, int idx, unsigned short codec_bits)
{
	static int cfg_bits[3] = { 1<<12, 1<<14, 1<<13 };
	unsigned short val;
	
	snd_ac97_update_bits(ac97, AC97_AD_SERIAL_CFG, 0x7000, cfg_bits[idx]);
	snd_ac97_write_cache(ac97, AC97_AD_CODEC_CFG, 0x0004);	// SDIE
	val = snd_ac97_read(ac97, AC97_VENDOR_ID2);
	if ((val & 0xff40) != 0x5340)
		return 0;
	if (codec_bits)
		snd_ac97_write_cache(ac97, AC97_AD_CODEC_CFG, codec_bits);
	ac97->spec.ad18xx.chained[idx] = cfg_bits[idx];
	ac97->spec.ad18xx.id[idx] = val;
	ac97->spec.ad18xx.codec_cfg[idx] = codec_bits ? codec_bits : 0x0004;
	return 1;
}

static void patch_ad1881_chained(struct snd_ac97 * ac97, int unchained_idx, int cidx1, int cidx2)
{
	// already detected?
	if (ac97->spec.ad18xx.unchained[cidx1] || ac97->spec.ad18xx.chained[cidx1])
		cidx1 = -1;
	if (ac97->spec.ad18xx.unchained[cidx2] || ac97->spec.ad18xx.chained[cidx2])
		cidx2 = -1;
	if (cidx1 < 0 && cidx2 < 0)
		return;
	// test for chained codecs
	snd_ac97_update_bits(ac97, AC97_AD_SERIAL_CFG, 0x7000,
			     ac97->spec.ad18xx.unchained[unchained_idx]);
	snd_ac97_write_cache(ac97, AC97_AD_CODEC_CFG, 0x0002);		// ID1C
	ac97->spec.ad18xx.codec_cfg[unchained_idx] = 0x0002;
	if (cidx1 >= 0) {
		if (cidx2 < 0)
			patch_ad1881_chained1(ac97, cidx1, 0);
		else if (patch_ad1881_chained1(ac97, cidx1, 0x0006))	// SDIE | ID1C
			patch_ad1881_chained1(ac97, cidx2, 0);
		else if (patch_ad1881_chained1(ac97, cidx2, 0x0006))	// SDIE | ID1C
			patch_ad1881_chained1(ac97, cidx1, 0);
	} else if (cidx2 >= 0) {
		patch_ad1881_chained1(ac97, cidx2, 0);
	}
}

static struct snd_ac97_build_ops patch_ad1881_build_ops = {
#ifdef CONFIG_PM
	.resume = ad18xx_resume
#endif
};

static int patch_ad1881(struct snd_ac97 * ac97)
{
	static const char cfg_idxs[3][2] = {
		{2, 1},
		{0, 2},
		{0, 1}
	};
	
	// patch for Analog Devices
	unsigned short codecs[3];
	unsigned short val;
	int idx, num;

	val = snd_ac97_read(ac97, AC97_AD_SERIAL_CFG);
	snd_ac97_write_cache(ac97, AC97_AD_SERIAL_CFG, val);
	codecs[0] = patch_ad1881_unchained(ac97, 0, (1<<12));
	codecs[1] = patch_ad1881_unchained(ac97, 1, (1<<14));
	codecs[2] = patch_ad1881_unchained(ac97, 2, (1<<13));

	if (! (codecs[0] || codecs[1] || codecs[2]))
		goto __end;

	for (idx = 0; idx < 3; idx++)
		if (ac97->spec.ad18xx.unchained[idx])
			patch_ad1881_chained(ac97, idx, cfg_idxs[idx][0], cfg_idxs[idx][1]);

	if (ac97->spec.ad18xx.id[1]) {
		ac97->flags |= AC97_AD_MULTI;
		ac97->scaps |= AC97_SCAP_SURROUND_DAC;
	}
	if (ac97->spec.ad18xx.id[2]) {
		ac97->flags |= AC97_AD_MULTI;
		ac97->scaps |= AC97_SCAP_CENTER_LFE_DAC;
	}

      __end:
	/* select all codecs */
	snd_ac97_update_bits(ac97, AC97_AD_SERIAL_CFG, 0x7000, 0x7000);
	/* check if only one codec is present */
	for (idx = num = 0; idx < 3; idx++)
		if (ac97->spec.ad18xx.id[idx])
			num++;
	if (num == 1) {
		/* ok, deselect all ID bits */
		snd_ac97_write_cache(ac97, AC97_AD_CODEC_CFG, 0x0000);
		ac97->spec.ad18xx.codec_cfg[0] = 
			ac97->spec.ad18xx.codec_cfg[1] = 
			ac97->spec.ad18xx.codec_cfg[2] = 0x0000;
	}
	/* required for AD1886/AD1885 combination */
	ac97->ext_id = snd_ac97_read(ac97, AC97_EXTENDED_ID);
	if (ac97->spec.ad18xx.id[0]) {
		ac97->id &= 0xffff0000;
		ac97->id |= ac97->spec.ad18xx.id[0];
	}
	ac97->build_ops = &patch_ad1881_build_ops;
	return 0;
}

static const struct snd_kcontrol_new snd_ac97_controls_ad1885[] = {
	AC97_SINGLE("Digital Mono Direct", AC97_AD_MISC, 11, 1, 0),
	/* AC97_SINGLE("Digital Audio Mode", AC97_AD_MISC, 12, 1, 0), */ /* seems problematic */
	AC97_SINGLE("Low Power Mixer", AC97_AD_MISC, 14, 1, 0),
	AC97_SINGLE("Zero Fill DAC", AC97_AD_MISC, 15, 1, 0),
	AC97_SINGLE("Headphone Jack Sense", AC97_AD_JACK_SPDIF, 9, 1, 1), /* inverted */
	AC97_SINGLE("Line Jack Sense", AC97_AD_JACK_SPDIF, 8, 1, 1), /* inverted */
};

static const DECLARE_TLV_DB_SCALE(db_scale_6bit_6db_max, -8850, 150, 0);

static int patch_ad1885_specific(struct snd_ac97 * ac97)
{
	int err;

	if ((err = patch_build_controls(ac97, snd_ac97_controls_ad1885, ARRAY_SIZE(snd_ac97_controls_ad1885))) < 0)
		return err;
	reset_tlv(ac97, "Headphone Playback Volume",
		  db_scale_6bit_6db_max);
	return 0;
}

static struct snd_ac97_build_ops patch_ad1885_build_ops = {
	.build_specific = &patch_ad1885_specific,
#ifdef CONFIG_PM
	.resume = ad18xx_resume
#endif
};

static int patch_ad1885(struct snd_ac97 * ac97)
{
	patch_ad1881(ac97);
	/* This is required to deal with the Intel D815EEAL2 */
	/* i.e. Line out is actually headphone out from codec */

	/* set default */
	snd_ac97_write_cache(ac97, AC97_AD_MISC, 0x0404);

	ac97->build_ops = &patch_ad1885_build_ops;
	return 0;
}

static int patch_ad1886_specific(struct snd_ac97 * ac97)
{
	reset_tlv(ac97, "Headphone Playback Volume",
		  db_scale_6bit_6db_max);
	return 0;
}

static struct snd_ac97_build_ops patch_ad1886_build_ops = {
	.build_specific = &patch_ad1886_specific,
#ifdef CONFIG_PM
	.resume = ad18xx_resume
#endif
};

static int patch_ad1886(struct snd_ac97 * ac97)
{
	patch_ad1881(ac97);
	/* Presario700 workaround */
	/* for Jack Sense/SPDIF Register misetting causing */
	snd_ac97_write_cache(ac97, AC97_AD_JACK_SPDIF, 0x0010);
	ac97->build_ops = &patch_ad1886_build_ops;
	return 0;
}

/* MISC bits (AD1888/AD1980/AD1985 register 0x76) */
#define AC97_AD198X_MBC		0x0003	/* mic boost */
#define AC97_AD198X_MBC_20	0x0000	/* +20dB */
#define AC97_AD198X_MBC_10	0x0001	/* +10dB */
#define AC97_AD198X_MBC_30	0x0002	/* +30dB */
#define AC97_AD198X_VREFD	0x0004	/* VREF high-Z */
#define AC97_AD198X_VREFH	0x0008	/* 0=2.25V, 1=3.7V */
#define AC97_AD198X_VREF_0	0x000c	/* 0V (AD1985 only) */
#define AC97_AD198X_VREF_MASK	(AC97_AD198X_VREFH | AC97_AD198X_VREFD)
#define AC97_AD198X_VREF_SHIFT	2
#define AC97_AD198X_SRU		0x0010	/* sample rate unlock */
#define AC97_AD198X_LOSEL	0x0020	/* LINE_OUT amplifiers input select */
#define AC97_AD198X_2MIC	0x0040	/* 2-channel mic select */
#define AC97_AD198X_SPRD	0x0080	/* SPREAD enable */
#define AC97_AD198X_DMIX0	0x0100	/* downmix mode: */
					/*  0 = 6-to-4, 1 = 6-to-2 downmix */
#define AC97_AD198X_DMIX1	0x0200	/* downmix mode: 1 = enabled */
#define AC97_AD198X_HPSEL	0x0400	/* headphone amplifier input select */
#define AC97_AD198X_CLDIS	0x0800	/* center/lfe disable */
#define AC97_AD198X_LODIS	0x1000	/* LINE_OUT disable */
#define AC97_AD198X_MSPLT	0x2000	/* mute split */
#define AC97_AD198X_AC97NC	0x4000	/* AC97 no compatible mode */
#define AC97_AD198X_DACZ	0x8000	/* DAC zero-fill mode */

/* MISC 1 bits (AD1986 register 0x76) */
#define AC97_AD1986_MBC		0x0003	/* mic boost */
#define AC97_AD1986_MBC_20	0x0000	/* +20dB */
#define AC97_AD1986_MBC_10	0x0001	/* +10dB */
#define AC97_AD1986_MBC_30	0x0002	/* +30dB */
#define AC97_AD1986_LISEL0	0x0004	/* LINE_IN select bit 0 */
#define AC97_AD1986_LISEL1	0x0008	/* LINE_IN select bit 1 */
#define AC97_AD1986_LISEL_MASK	(AC97_AD1986_LISEL1 | AC97_AD1986_LISEL0)
#define AC97_AD1986_LISEL_LI	0x0000  /* LINE_IN pins as LINE_IN source */
#define AC97_AD1986_LISEL_SURR	0x0004  /* SURROUND pins as LINE_IN source */
#define AC97_AD1986_LISEL_MIC	0x0008  /* MIC_1/2 pins as LINE_IN source */
#define AC97_AD1986_SRU		0x0010	/* sample rate unlock */
#define AC97_AD1986_SOSEL	0x0020	/* SURROUND_OUT amplifiers input sel */
#define AC97_AD1986_2MIC	0x0040	/* 2-channel mic select */
#define AC97_AD1986_SPRD	0x0080	/* SPREAD enable */
#define AC97_AD1986_DMIX0	0x0100	/* downmix mode: */
					/*  0 = 6-to-4, 1 = 6-to-2 downmix */
#define AC97_AD1986_DMIX1	0x0200	/* downmix mode: 1 = enabled */
#define AC97_AD1986_CLDIS	0x0800	/* center/lfe disable */
#define AC97_AD1986_SODIS	0x1000	/* SURROUND_OUT disable */
#define AC97_AD1986_MSPLT	0x2000	/* mute split (read only 1) */
#define AC97_AD1986_AC97NC	0x4000	/* AC97 no compatible mode (r/o 1) */
#define AC97_AD1986_DACZ	0x8000	/* DAC zero-fill mode */

/* MISC 2 bits (AD1986 register 0x70) */
#define AC97_AD_MISC2		0x70	/* Misc Control Bits 2 (AD1986) */

#define AC97_AD1986_CVREF0	0x0004	/* C/LFE VREF_OUT 2.25V */
#define AC97_AD1986_CVREF1	0x0008	/* C/LFE VREF_OUT 0V */
#define AC97_AD1986_CVREF2	0x0010	/* C/LFE VREF_OUT 3.7V */
#define AC97_AD1986_CVREF_MASK \
	(AC97_AD1986_CVREF2 | AC97_AD1986_CVREF1 | AC97_AD1986_CVREF0)
#define AC97_AD1986_JSMAP	0x0020	/* Jack Sense Mapping 1 = alternate */
#define AC97_AD1986_MMDIS	0x0080	/* Mono Mute Disable */
#define AC97_AD1986_MVREF0	0x0400	/* MIC VREF_OUT 2.25V */
#define AC97_AD1986_MVREF1	0x0800	/* MIC VREF_OUT 0V */
#define AC97_AD1986_MVREF2	0x1000	/* MIC VREF_OUT 3.7V */
#define AC97_AD1986_MVREF_MASK \
	(AC97_AD1986_MVREF2 | AC97_AD1986_MVREF1 | AC97_AD1986_MVREF0)

/* MISC 3 bits (AD1986 register 0x7a) */
#define AC97_AD_MISC3		0x7a	/* Misc Control Bits 3 (AD1986) */

#define AC97_AD1986_MMIX	0x0004	/* Mic Mix, left/right */
#define AC97_AD1986_GPO		0x0008	/* General Purpose Out */
#define AC97_AD1986_LOHPEN	0x0010	/* LINE_OUT headphone drive */
#define AC97_AD1986_LVREF0	0x0100	/* LINE_OUT VREF_OUT 2.25V */
#define AC97_AD1986_LVREF1	0x0200	/* LINE_OUT VREF_OUT 0V */
#define AC97_AD1986_LVREF2	0x0400	/* LINE_OUT VREF_OUT 3.7V */
#define AC97_AD1986_LVREF_MASK \
	(AC97_AD1986_LVREF2 | AC97_AD1986_LVREF1 | AC97_AD1986_LVREF0)
#define AC97_AD1986_JSINVA	0x0800	/* Jack Sense Invert SENSE_A */
#define AC97_AD1986_LOSEL	0x1000	/* LINE_OUT amplifiers input select */
#define AC97_AD1986_HPSEL0	0x2000	/* Headphone amplifiers */
					/*   input select Surround DACs */
#define AC97_AD1986_HPSEL1	0x4000	/* Headphone amplifiers input */
					/*   select C/LFE DACs */
#define AC97_AD1986_JSINVB	0x8000	/* Jack Sense Invert SENSE_B */

/* Serial Config bits (AD1986 register 0x74) (incomplete) */
#define AC97_AD1986_OMS0	0x0100	/* Optional Mic Selector bit 0 */
#define AC97_AD1986_OMS1	0x0200	/* Optional Mic Selector bit 1 */
#define AC97_AD1986_OMS2	0x0400	/* Optional Mic Selector bit 2 */
#define AC97_AD1986_OMS_MASK \
	(AC97_AD1986_OMS2 | AC97_AD1986_OMS1 | AC97_AD1986_OMS0)
#define AC97_AD1986_OMS_M	0x0000  /* MIC_1/2 pins are MIC sources */
#define AC97_AD1986_OMS_L	0x0100  /* LINE_IN pins are MIC sources */
#define AC97_AD1986_OMS_C	0x0200  /* Center/LFE pins are MCI sources */
#define AC97_AD1986_OMS_MC	0x0400  /* Mix of MIC and C/LFE pins */
					/*   are MIC sources */
#define AC97_AD1986_OMS_ML	0x0500  /* MIX of MIC and LINE_IN pins */
					/*   are MIC sources */
#define AC97_AD1986_OMS_LC	0x0600  /* MIX of LINE_IN and C/LFE pins */
					/*   are MIC sources */
#define AC97_AD1986_OMS_MLC	0x0700  /* MIX of MIC, LINE_IN, C/LFE pins */
					/*   are MIC sources */


static int snd_ac97_ad198x_spdif_source_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	static char *texts[2] = { "AC-Link", "A/D Converter" };

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = 2;
	if (uinfo->value.enumerated.item > 1)
		uinfo->value.enumerated.item = 1;
	strcpy(uinfo->value.enumerated.name, texts[uinfo->value.enumerated.item]);
	return 0;
}

static int snd_ac97_ad198x_spdif_source_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ac97 *ac97 = snd_kcontrol_chip(kcontrol);
	unsigned short val;

	val = ac97->regs[AC97_AD_SERIAL_CFG];
	ucontrol->value.enumerated.item[0] = (val >> 2) & 1;
	return 0;
}

static int snd_ac97_ad198x_spdif_source_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ac97 *ac97 = snd_kcontrol_chip(kcontrol);
	unsigned short val;

	if (ucontrol->value.enumerated.item[0] > 1)
		return -EINVAL;
	val = ucontrol->value.enumerated.item[0] << 2;
	return snd_ac97_update_bits(ac97, AC97_AD_SERIAL_CFG, 0x0004, val);
}

static const struct snd_kcontrol_new snd_ac97_ad198x_spdif_source = {
	.iface	= SNDRV_CTL_ELEM_IFACE_MIXER,
	.name	= SNDRV_CTL_NAME_IEC958("",PLAYBACK,NONE) "Source",
	.info	= snd_ac97_ad198x_spdif_source_info,
	.get	= snd_ac97_ad198x_spdif_source_get,
	.put	= snd_ac97_ad198x_spdif_source_put,
};

static int patch_ad198x_post_spdif(struct snd_ac97 * ac97)
{
 	return patch_build_controls(ac97, &snd_ac97_ad198x_spdif_source, 1);
}

static const struct snd_kcontrol_new snd_ac97_ad1981x_jack_sense[] = {
	AC97_SINGLE("Headphone Jack Sense", AC97_AD_JACK_SPDIF, 11, 1, 0),
	AC97_SINGLE("Line Jack Sense", AC97_AD_JACK_SPDIF, 12, 1, 0),
};

/* black list to avoid HP/Line jack-sense controls
 * (SS vendor << 16 | device)
 */
static unsigned int ad1981_jacks_blacklist[] = {
	0x10140523, /* Thinkpad R40 */
	0x10140534, /* Thinkpad X31 */
	0x10140537, /* Thinkpad T41p */
	0x10140554, /* Thinkpad T42p/R50p */
	0x10140567, /* Thinkpad T43p 2668-G7U */
	0x10140581, /* Thinkpad X41-2527 */
	0x104380b0, /* Asus A7V8X-MX */
	0x11790241, /* Toshiba Satellite A-15 S127 */
	0x144dc01a, /* Samsung NP-X20C004/SEG */
	0 /* end */
};

static int check_list(struct snd_ac97 *ac97, const unsigned int *list)
{
	u32 subid = ((u32)ac97->subsystem_vendor << 16) | ac97->subsystem_device;
	for (; *list; list++)
		if (*list == subid)
			return 1;
	return 0;
}

static int patch_ad1981a_specific(struct snd_ac97 * ac97)
{
	if (check_list(ac97, ad1981_jacks_blacklist))
		return 0;
	return patch_build_controls(ac97, snd_ac97_ad1981x_jack_sense,
				    ARRAY_SIZE(snd_ac97_ad1981x_jack_sense));
}

static struct snd_ac97_build_ops patch_ad1981a_build_ops = {
	.build_post_spdif = patch_ad198x_post_spdif,
	.build_specific = patch_ad1981a_specific,
#ifdef CONFIG_PM
	.resume = ad18xx_resume
#endif
};

/* white list to enable HP jack-sense bits
 * (SS vendor << 16 | device)
 */
static unsigned int ad1981_jacks_whitelist[] = {
	0x0e11005a, /* HP nc4000/4010 */
	0x103c0890, /* HP nc6000 */
	0x103c0938, /* HP nc4220 */
	0x103c099c, /* HP nx6110 */
	0x103c0944, /* HP nc6220 */
	0x103c0934, /* HP nc8220 */
	0x103c006d, /* HP nx9105 */
	0x17340088, /* FSC Scenic-W */
	0 /* end */
};

static void check_ad1981_hp_jack_sense(struct snd_ac97 *ac97)
{
	if (check_list(ac97, ad1981_jacks_whitelist))
		/* enable headphone jack sense */
		snd_ac97_update_bits(ac97, AC97_AD_JACK_SPDIF, 1<<11, 1<<11);
}

static int patch_ad1981a(struct snd_ac97 *ac97)
{
	patch_ad1881(ac97);
	ac97->build_ops = &patch_ad1981a_build_ops;
	snd_ac97_update_bits(ac97, AC97_AD_MISC, AC97_AD198X_MSPLT, AC97_AD198X_MSPLT);
	ac97->flags |= AC97_STEREO_MUTES;
	check_ad1981_hp_jack_sense(ac97);
	return 0;
}

static const struct snd_kcontrol_new snd_ac97_ad198x_2cmic =
AC97_SINGLE("Stereo Mic", AC97_AD_MISC, 6, 1, 0);

static int patch_ad1981b_specific(struct snd_ac97 *ac97)
{
	int err;

	if ((err = patch_build_controls(ac97, &snd_ac97_ad198x_2cmic, 1)) < 0)
		return err;
	if (check_list(ac97, ad1981_jacks_blacklist))
		return 0;
	return patch_build_controls(ac97, snd_ac97_ad1981x_jack_sense,
				    ARRAY_SIZE(snd_ac97_ad1981x_jack_sense));
}

static struct snd_ac97_build_ops patch_ad1981b_build_ops = {
	.build_post_spdif = patch_ad198x_post_spdif,
	.build_specific = patch_ad1981b_specific,
#ifdef CONFIG_PM
	.resume = ad18xx_resume
#endif
};

static int patch_ad1981b(struct snd_ac97 *ac97)
{
	patch_ad1881(ac97);
	ac97->build_ops = &patch_ad1981b_build_ops;
	snd_ac97_update_bits(ac97, AC97_AD_MISC, AC97_AD198X_MSPLT, AC97_AD198X_MSPLT);
	ac97->flags |= AC97_STEREO_MUTES;
	check_ad1981_hp_jack_sense(ac97);
	return 0;
}

#define snd_ac97_ad1888_lohpsel_info	snd_ctl_boolean_mono_info

static int snd_ac97_ad1888_lohpsel_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ac97 *ac97 = snd_kcontrol_chip(kcontrol);
	unsigned short val;

	val = ac97->regs[AC97_AD_MISC];
	ucontrol->value.integer.value[0] = !(val & AC97_AD198X_LOSEL);
	if (ac97->spec.ad18xx.lo_as_master)
		ucontrol->value.integer.value[0] =
			!ucontrol->value.integer.value[0];
	return 0;
}

static int snd_ac97_ad1888_lohpsel_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ac97 *ac97 = snd_kcontrol_chip(kcontrol);
	unsigned short val;

	val = !ucontrol->value.integer.value[0];
	if (ac97->spec.ad18xx.lo_as_master)
		val = !val;
	val = val ? (AC97_AD198X_LOSEL | AC97_AD198X_HPSEL) : 0;
	return snd_ac97_update_bits(ac97, AC97_AD_MISC,
				    AC97_AD198X_LOSEL | AC97_AD198X_HPSEL, val);
}

static int snd_ac97_ad1888_downmix_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	static char *texts[3] = {"Off", "6 -> 4", "6 -> 2"};

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = 3;
	if (uinfo->value.enumerated.item > 2)
		uinfo->value.enumerated.item = 2;
	strcpy(uinfo->value.enumerated.name, texts[uinfo->value.enumerated.item]);
	return 0;
}

static int snd_ac97_ad1888_downmix_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ac97 *ac97 = snd_kcontrol_chip(kcontrol);
	unsigned short val;

	val = ac97->regs[AC97_AD_MISC];
	if (!(val & AC97_AD198X_DMIX1))
		ucontrol->value.enumerated.item[0] = 0;
	else
		ucontrol->value.enumerated.item[0] = 1 + ((val >> 8) & 1);
	return 0;
}

static int snd_ac97_ad1888_downmix_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ac97 *ac97 = snd_kcontrol_chip(kcontrol);
	unsigned short val;

	if (ucontrol->value.enumerated.item[0] > 2)
		return -EINVAL;
	if (ucontrol->value.enumerated.item[0] == 0)
		val = 0;
	else
		val = AC97_AD198X_DMIX1 |
			((ucontrol->value.enumerated.item[0] - 1) << 8);
	return snd_ac97_update_bits(ac97, AC97_AD_MISC,
				    AC97_AD198X_DMIX0 | AC97_AD198X_DMIX1, val);
}

static void ad1888_update_jacks(struct snd_ac97 *ac97)
{
	unsigned short val = 0;
	/* clear LODIS if shared jack is to be used for Surround out */
	if (!ac97->spec.ad18xx.lo_as_master && is_shared_linein(ac97))
		val |= (1 << 12);
	/* clear CLDIS if shared jack is to be used for C/LFE out */
	if (is_shared_micin(ac97))
		val |= (1 << 11);
	/* shared Line-In */
	snd_ac97_update_bits(ac97, AC97_AD_MISC, (1 << 11) | (1 << 12), val);
}

static const struct snd_kcontrol_new snd_ac97_ad1888_controls[] = {
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Exchange Front/Surround",
		.info = snd_ac97_ad1888_lohpsel_info,
		.get = snd_ac97_ad1888_lohpsel_get,
		.put = snd_ac97_ad1888_lohpsel_put
	},
	AC97_SINGLE("V_REFOUT Enable", AC97_AD_MISC, AC97_AD_VREFD_SHIFT, 1, 1),
	AC97_SINGLE("High Pass Filter Enable", AC97_AD_TEST2,
			AC97_AD_HPFD_SHIFT, 1, 1),
	AC97_SINGLE("Spread Front to Surround and Center/LFE", AC97_AD_MISC, 7, 1, 0),
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Downmix",
		.info = snd_ac97_ad1888_downmix_info,
		.get = snd_ac97_ad1888_downmix_get,
		.put = snd_ac97_ad1888_downmix_put
	},
	AC97_SURROUND_JACK_MODE_CTL,
	AC97_CHANNEL_MODE_CTL,

	AC97_SINGLE("Headphone Jack Sense", AC97_AD_JACK_SPDIF, 10, 1, 0),
	AC97_SINGLE("Line Jack Sense", AC97_AD_JACK_SPDIF, 12, 1, 0),
};

static int patch_ad1888_specific(struct snd_ac97 *ac97)
{
	if (!ac97->spec.ad18xx.lo_as_master) {
		/* rename 0x04 as "Master" and 0x02 as "Master Surround" */
		snd_ac97_rename_vol_ctl(ac97, "Master Playback",
					"Master Surround Playback");
		snd_ac97_rename_vol_ctl(ac97, "Headphone Playback",
					"Master Playback");
	}
	return patch_build_controls(ac97, snd_ac97_ad1888_controls, ARRAY_SIZE(snd_ac97_ad1888_controls));
}

static struct snd_ac97_build_ops patch_ad1888_build_ops = {
	.build_post_spdif = patch_ad198x_post_spdif,
	.build_specific = patch_ad1888_specific,
#ifdef CONFIG_PM
	.resume = ad1888_resume,
#endif
	.update_jacks = ad1888_update_jacks,
};

static int patch_ad1888(struct snd_ac97 * ac97)
{
	unsigned short misc;
	
	patch_ad1881(ac97);
	ac97->build_ops = &patch_ad1888_build_ops;

	/*
	 * LO can be used as a real line-out on some devices,
	 * and we need to revert the front/surround mixer switches
	 */
	if (ac97->subsystem_vendor == 0x1043 &&
	    ac97->subsystem_device == 0x1193) /* ASUS A9T laptop */
		ac97->spec.ad18xx.lo_as_master = 1;

	misc = snd_ac97_read(ac97, AC97_AD_MISC);
	/* AD-compatible mode */
	/* Stereo mutes enabled */
	misc |= AC97_AD198X_MSPLT | AC97_AD198X_AC97NC;
	if (!ac97->spec.ad18xx.lo_as_master)
		/* Switch FRONT/SURROUND LINE-OUT/HP-OUT default connection */
		/* it seems that most vendors connect line-out connector to
		 * headphone out of AC'97
		 */
		misc |= AC97_AD198X_LOSEL | AC97_AD198X_HPSEL;

	snd_ac97_write_cache(ac97, AC97_AD_MISC, misc);
	ac97->flags |= AC97_STEREO_MUTES;
	return 0;
}

static int patch_ad1980_specific(struct snd_ac97 *ac97)
{
	int err;

	if ((err = patch_ad1888_specific(ac97)) < 0)
		return err;
	return patch_build_controls(ac97, &snd_ac97_ad198x_2cmic, 1);
}

static struct snd_ac97_build_ops patch_ad1980_build_ops = {
	.build_post_spdif = patch_ad198x_post_spdif,
	.build_specific = patch_ad1980_specific,
#ifdef CONFIG_PM
	.resume = ad18xx_resume,
#endif
	.update_jacks = ad1888_update_jacks,
};

static int patch_ad1980(struct snd_ac97 * ac97)
{
	patch_ad1888(ac97);
	ac97->build_ops = &patch_ad1980_build_ops;
	return 0;
}

static int snd_ac97_ad1985_vrefout_info(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_info *uinfo)
{
	static char *texts[4] = {"High-Z", "3.7 V", "2.25 V", "0 V"};

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = 4;
	if (uinfo->value.enumerated.item > 3)
		uinfo->value.enumerated.item = 3;
	strcpy(uinfo->value.enumerated.name,
	       texts[uinfo->value.enumerated.item]);
	return 0;
}

static int snd_ac97_ad1985_vrefout_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	static const int reg2ctrl[4] = {2, 0, 1, 3};
	struct snd_ac97 *ac97 = snd_kcontrol_chip(kcontrol);
	unsigned short val;
	val = (ac97->regs[AC97_AD_MISC] & AC97_AD198X_VREF_MASK)
	      >> AC97_AD198X_VREF_SHIFT;
	ucontrol->value.enumerated.item[0] = reg2ctrl[val];
	return 0;
}

static int snd_ac97_ad1985_vrefout_put(struct snd_kcontrol *kcontrol, 
				       struct snd_ctl_elem_value *ucontrol)
{
	static const int ctrl2reg[4] = {1, 2, 0, 3};
	struct snd_ac97 *ac97 = snd_kcontrol_chip(kcontrol);
	unsigned short val;

	if (ucontrol->value.enumerated.item[0] > 3)
		return -EINVAL;
	val = ctrl2reg[ucontrol->value.enumerated.item[0]]
	      << AC97_AD198X_VREF_SHIFT;
	return snd_ac97_update_bits(ac97, AC97_AD_MISC,
				    AC97_AD198X_VREF_MASK, val);
}

static const struct snd_kcontrol_new snd_ac97_ad1985_controls[] = {
	AC97_SINGLE("Exchange Center/LFE", AC97_AD_SERIAL_CFG, 3, 1, 0),
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Exchange Front/Surround",
		.info = snd_ac97_ad1888_lohpsel_info,
		.get = snd_ac97_ad1888_lohpsel_get,
		.put = snd_ac97_ad1888_lohpsel_put
	},
	AC97_SINGLE("High Pass Filter Enable", AC97_AD_TEST2, 12, 1, 1),
	AC97_SINGLE("Spread Front to Surround and Center/LFE",
		    AC97_AD_MISC, 7, 1, 0),
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Downmix",
		.info = snd_ac97_ad1888_downmix_info,
		.get = snd_ac97_ad1888_downmix_get,
		.put = snd_ac97_ad1888_downmix_put
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "V_REFOUT",
		.info = snd_ac97_ad1985_vrefout_info,
		.get = snd_ac97_ad1985_vrefout_get,
		.put = snd_ac97_ad1985_vrefout_put
	},
	AC97_SURROUND_JACK_MODE_CTL,
	AC97_CHANNEL_MODE_CTL,

	AC97_SINGLE("Headphone Jack Sense", AC97_AD_JACK_SPDIF, 10, 1, 0),
	AC97_SINGLE("Line Jack Sense", AC97_AD_JACK_SPDIF, 12, 1, 0),
};

static void ad1985_update_jacks(struct snd_ac97 *ac97)
{
	ad1888_update_jacks(ac97);
	/* clear OMS if shared jack is to be used for C/LFE out */
	snd_ac97_update_bits(ac97, AC97_AD_SERIAL_CFG, 1 << 9,
			     is_shared_micin(ac97) ? 1 << 9 : 0);
}

static int patch_ad1985_specific(struct snd_ac97 *ac97)
{
	int err;

	/* rename 0x04 as "Master" and 0x02 as "Master Surround" */
	snd_ac97_rename_vol_ctl(ac97, "Master Playback",
				"Master Surround Playback");
	snd_ac97_rename_vol_ctl(ac97, "Headphone Playback", "Master Playback");

	if ((err = patch_build_controls(ac97, &snd_ac97_ad198x_2cmic, 1)) < 0)
		return err;

	return patch_build_controls(ac97, snd_ac97_ad1985_controls,
				    ARRAY_SIZE(snd_ac97_ad1985_controls));
}

static struct snd_ac97_build_ops patch_ad1985_build_ops = {
	.build_post_spdif = patch_ad198x_post_spdif,
	.build_specific = patch_ad1985_specific,
#ifdef CONFIG_PM
	.resume = ad18xx_resume,
#endif
	.update_jacks = ad1985_update_jacks,
};

static int patch_ad1985(struct snd_ac97 * ac97)
{
	unsigned short misc;
	
	patch_ad1881(ac97);
	ac97->build_ops = &patch_ad1985_build_ops;
	misc = snd_ac97_read(ac97, AC97_AD_MISC);
	/* switch front/surround line-out/hp-out */
	/* AD-compatible mode */
	/* Stereo mutes enabled */
	snd_ac97_write_cache(ac97, AC97_AD_MISC, misc |
			     AC97_AD198X_LOSEL |
			     AC97_AD198X_HPSEL |
			     AC97_AD198X_MSPLT |
			     AC97_AD198X_AC97NC);
	ac97->flags |= AC97_STEREO_MUTES;

	/* update current jack configuration */
	ad1985_update_jacks(ac97);

	/* on AD1985 rev. 3, AC'97 revision bits are zero */
	ac97->ext_id = (ac97->ext_id & ~AC97_EI_REV_MASK) | AC97_EI_REV_23;
	return 0;
}

#define snd_ac97_ad1986_bool_info	snd_ctl_boolean_mono_info

static int snd_ac97_ad1986_lososel_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ac97 *ac97 = snd_kcontrol_chip(kcontrol);
	unsigned short val;

	val = ac97->regs[AC97_AD_MISC3];
	ucontrol->value.integer.value[0] = (val & AC97_AD1986_LOSEL) != 0;
	return 0;
}

static int snd_ac97_ad1986_lososel_put(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ac97 *ac97 = snd_kcontrol_chip(kcontrol);
	int ret0;
	int ret1;
	int sprd = (ac97->regs[AC97_AD_MISC] & AC97_AD1986_SPRD) != 0;

	ret0 = snd_ac97_update_bits(ac97, AC97_AD_MISC3, AC97_AD1986_LOSEL,
					ucontrol->value.integer.value[0] != 0
				    ? AC97_AD1986_LOSEL : 0);
	if (ret0 < 0)
		return ret0;

	/* SOSEL is set to values of "Spread" or "Exchange F/S" controls */
	ret1 = snd_ac97_update_bits(ac97, AC97_AD_MISC, AC97_AD1986_SOSEL,
				    (ucontrol->value.integer.value[0] != 0
				     || sprd)
				    ? AC97_AD1986_SOSEL : 0);
	if (ret1 < 0)
		return ret1;

	return (ret0 > 0 || ret1 > 0) ? 1 : 0;
}

static int snd_ac97_ad1986_spread_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ac97 *ac97 = snd_kcontrol_chip(kcontrol);
	unsigned short val;

	val = ac97->regs[AC97_AD_MISC];
	ucontrol->value.integer.value[0] = (val & AC97_AD1986_SPRD) != 0;
	return 0;
}

static int snd_ac97_ad1986_spread_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ac97 *ac97 = snd_kcontrol_chip(kcontrol);
	int ret0;
	int ret1;
	int sprd = (ac97->regs[AC97_AD_MISC3] & AC97_AD1986_LOSEL) != 0;

	ret0 = snd_ac97_update_bits(ac97, AC97_AD_MISC, AC97_AD1986_SPRD,
					ucontrol->value.integer.value[0] != 0
				    ? AC97_AD1986_SPRD : 0);
	if (ret0 < 0)
		return ret0;

	/* SOSEL is set to values of "Spread" or "Exchange F/S" controls */
	ret1 = snd_ac97_update_bits(ac97, AC97_AD_MISC, AC97_AD1986_SOSEL,
				    (ucontrol->value.integer.value[0] != 0
				     || sprd)
				    ? AC97_AD1986_SOSEL : 0);
	if (ret1 < 0)
		return ret1;

	return (ret0 > 0 || ret1 > 0) ? 1 : 0;
}

static int snd_ac97_ad1986_miclisel_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ac97 *ac97 = snd_kcontrol_chip(kcontrol);

	ucontrol->value.integer.value[0] = ac97->spec.ad18xx.swap_mic_linein;
	return 0;
}

static int snd_ac97_ad1986_miclisel_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ac97 *ac97 = snd_kcontrol_chip(kcontrol);
	unsigned char swap = ucontrol->value.integer.value[0] != 0;

	if (swap != ac97->spec.ad18xx.swap_mic_linein) {
		ac97->spec.ad18xx.swap_mic_linein = swap;
		if (ac97->build_ops->update_jacks)
			ac97->build_ops->update_jacks(ac97);
		return 1;
	}
	return 0;
}

static int snd_ac97_ad1986_vrefout_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	/* Use MIC_1/2 V_REFOUT as the "get" value */
	struct snd_ac97 *ac97 = snd_kcontrol_chip(kcontrol);
	unsigned short val;
	unsigned short reg = ac97->regs[AC97_AD_MISC2];
	if ((reg & AC97_AD1986_MVREF0) != 0)
		val = 2;
	else if ((reg & AC97_AD1986_MVREF1) != 0)
		val = 3;
	else if ((reg & AC97_AD1986_MVREF2) != 0)
		val = 1;
	else
		val = 0;
	ucontrol->value.enumerated.item[0] = val;
	return 0;
}

static int snd_ac97_ad1986_vrefout_put(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ac97 *ac97 = snd_kcontrol_chip(kcontrol);
	unsigned short cval;
	unsigned short lval;
	unsigned short mval;
	int cret;
	int lret;
	int mret;

	switch (ucontrol->value.enumerated.item[0])
	{
	case 0: /* High-Z */
		cval = 0;
		lval = 0;
		mval = 0;
		break;
	case 1: /* 3.7 V */
		cval = AC97_AD1986_CVREF2;
		lval = AC97_AD1986_LVREF2;
		mval = AC97_AD1986_MVREF2;
		break;
	case 2: /* 2.25 V */
		cval = AC97_AD1986_CVREF0;
		lval = AC97_AD1986_LVREF0;
		mval = AC97_AD1986_MVREF0;
		break;
	case 3: /* 0 V */
		cval = AC97_AD1986_CVREF1;
		lval = AC97_AD1986_LVREF1;
		mval = AC97_AD1986_MVREF1;
		break;
	default:
		return -EINVAL;
	}

	cret = snd_ac97_update_bits(ac97, AC97_AD_MISC2,
				    AC97_AD1986_CVREF_MASK, cval);
	if (cret < 0)
		return cret;
	lret = snd_ac97_update_bits(ac97, AC97_AD_MISC3,
				    AC97_AD1986_LVREF_MASK, lval);
	if (lret < 0)
		return lret;
	mret = snd_ac97_update_bits(ac97, AC97_AD_MISC2,
				    AC97_AD1986_MVREF_MASK, mval);
	if (mret < 0)
		return mret;

	return (cret > 0 || lret > 0 || mret > 0) ? 1 : 0;
}

static const struct snd_kcontrol_new snd_ac97_ad1986_controls[] = {
	AC97_SINGLE("Exchange Center/LFE", AC97_AD_SERIAL_CFG, 3, 1, 0),
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Exchange Front/Surround",
		.info = snd_ac97_ad1986_bool_info,
		.get = snd_ac97_ad1986_lososel_get,
		.put = snd_ac97_ad1986_lososel_put
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Exchange Mic/Line In",
		.info = snd_ac97_ad1986_bool_info,
		.get = snd_ac97_ad1986_miclisel_get,
		.put = snd_ac97_ad1986_miclisel_put
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Spread Front to Surround and Center/LFE",
		.info = snd_ac97_ad1986_bool_info,
		.get = snd_ac97_ad1986_spread_get,
		.put = snd_ac97_ad1986_spread_put
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Downmix",
		.info = snd_ac97_ad1888_downmix_info,
		.get = snd_ac97_ad1888_downmix_get,
		.put = snd_ac97_ad1888_downmix_put
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "V_REFOUT",
		.info = snd_ac97_ad1985_vrefout_info,
		.get = snd_ac97_ad1986_vrefout_get,
		.put = snd_ac97_ad1986_vrefout_put
	},
	AC97_SURROUND_JACK_MODE_CTL,
	AC97_CHANNEL_MODE_CTL,

	AC97_SINGLE("Headphone Jack Sense", AC97_AD_JACK_SPDIF, 10, 1, 0),
	AC97_SINGLE("Line Jack Sense", AC97_AD_JACK_SPDIF, 12, 1, 0)
};

static void ad1986_update_jacks(struct snd_ac97 *ac97)
{
	unsigned short misc_val = 0;
	unsigned short ser_val;

	/* disable SURROUND and CENTER/LFE if not surround mode */
	if (!is_surround_on(ac97))
		misc_val |= AC97_AD1986_SODIS;
	if (!is_clfe_on(ac97))
		misc_val |= AC97_AD1986_CLDIS;

	/* select line input (default=LINE_IN, SURROUND or MIC_1/2) */
	if (is_shared_linein(ac97))
		misc_val |= AC97_AD1986_LISEL_SURR;
	else if (ac97->spec.ad18xx.swap_mic_linein != 0)
		misc_val |= AC97_AD1986_LISEL_MIC;
	snd_ac97_update_bits(ac97, AC97_AD_MISC,
			     AC97_AD1986_SODIS | AC97_AD1986_CLDIS |
			     AC97_AD1986_LISEL_MASK,
			     misc_val);

	/* select microphone input (MIC_1/2, Center/LFE or LINE_IN) */
	if (is_shared_micin(ac97))
		ser_val = AC97_AD1986_OMS_C;
	else if (ac97->spec.ad18xx.swap_mic_linein != 0)
		ser_val = AC97_AD1986_OMS_L;
	else
		ser_val = AC97_AD1986_OMS_M;
	snd_ac97_update_bits(ac97, AC97_AD_SERIAL_CFG,
			     AC97_AD1986_OMS_MASK,
			     ser_val);
}

static int patch_ad1986_specific(struct snd_ac97 *ac97)
{
	int err;

	if ((err = patch_build_controls(ac97, &snd_ac97_ad198x_2cmic, 1)) < 0)
		return err;

	return patch_build_controls(ac97, snd_ac97_ad1986_controls,
				    ARRAY_SIZE(snd_ac97_ad1985_controls));
}

static struct snd_ac97_build_ops patch_ad1986_build_ops = {
	.build_post_spdif = patch_ad198x_post_spdif,
	.build_specific = patch_ad1986_specific,
#ifdef CONFIG_PM
	.resume = ad18xx_resume,
#endif
	.update_jacks = ad1986_update_jacks,
};

static int patch_ad1986(struct snd_ac97 * ac97)
{
	patch_ad1881(ac97);
	ac97->build_ops = &patch_ad1986_build_ops;
	ac97->flags |= AC97_STEREO_MUTES;

	/* update current jack configuration */
	ad1986_update_jacks(ac97);

	return 0;
}

/*
 * realtek ALC203: use mono-out for pin 37
 */
static int patch_alc203(struct snd_ac97 *ac97)
{
	snd_ac97_update_bits(ac97, 0x7a, 0x400, 0x400);
	return 0;
}

/*
 * realtek ALC65x/850 codecs
 */
static void alc650_update_jacks(struct snd_ac97 *ac97)
{
	int shared;
	
	/* shared Line-In / Surround Out */
	shared = is_shared_surrout(ac97);
	snd_ac97_update_bits(ac97, AC97_ALC650_MULTICH, 1 << 9,
			     shared ? (1 << 9) : 0);
	/* update shared Mic In / Center/LFE Out */
	shared = is_shared_clfeout(ac97);
	/* disable/enable vref */
	snd_ac97_update_bits(ac97, AC97_ALC650_CLOCK, 1 << 12,
			     shared ? (1 << 12) : 0);
	/* turn on/off center-on-mic */
	snd_ac97_update_bits(ac97, AC97_ALC650_MULTICH, 1 << 10,
			     shared ? (1 << 10) : 0);
	/* GPIO0 high for mic */
	snd_ac97_update_bits(ac97, AC97_ALC650_GPIO_STATUS, 0x100,
			     shared ? 0 : 0x100);
}

static const struct snd_kcontrol_new snd_ac97_controls_alc650[] = {
	AC97_SINGLE("Duplicate Front", AC97_ALC650_MULTICH, 0, 1, 0),
	AC97_SINGLE("Surround Down Mix", AC97_ALC650_MULTICH, 1, 1, 0),
	AC97_SINGLE("Center/LFE Down Mix", AC97_ALC650_MULTICH, 2, 1, 0),
	AC97_SINGLE("Exchange Center/LFE", AC97_ALC650_MULTICH, 3, 1, 0),
	/* 4: Analog Input To Surround */
	/* 5: Analog Input To Center/LFE */
	/* 6: Independent Master Volume Right */
	/* 7: Independent Master Volume Left */
	/* 8: reserved */
	/* 9: Line-In/Surround share */
	/* 10: Mic/CLFE share */
	/* 11-13: in IEC958 controls */
	AC97_SINGLE("Swap Surround Slot", AC97_ALC650_MULTICH, 14, 1, 0),
#if 0 /* always set in patch_alc650 */
	AC97_SINGLE("IEC958 Input Clock Enable", AC97_ALC650_CLOCK, 0, 1, 0),
	AC97_SINGLE("IEC958 Input Pin Enable", AC97_ALC650_CLOCK, 1, 1, 0),
	AC97_SINGLE("Surround DAC Switch", AC97_ALC650_SURR_DAC_VOL, 15, 1, 1),
	AC97_DOUBLE("Surround DAC Volume", AC97_ALC650_SURR_DAC_VOL, 8, 0, 31, 1),
	AC97_SINGLE("Center/LFE DAC Switch", AC97_ALC650_LFE_DAC_VOL, 15, 1, 1),
	AC97_DOUBLE("Center/LFE DAC Volume", AC97_ALC650_LFE_DAC_VOL, 8, 0, 31, 1),
#endif
	AC97_SURROUND_JACK_MODE_CTL,
	AC97_CHANNEL_MODE_CTL,
};

static const struct snd_kcontrol_new snd_ac97_spdif_controls_alc650[] = {
        AC97_SINGLE(SNDRV_CTL_NAME_IEC958("",CAPTURE,SWITCH), AC97_ALC650_MULTICH, 11, 1, 0),
        AC97_SINGLE("Analog to IEC958 Output", AC97_ALC650_MULTICH, 12, 1, 0),
	/* disable this controls since it doesn't work as expected */
	/* AC97_SINGLE("IEC958 Input Monitor", AC97_ALC650_MULTICH, 13, 1, 0), */
};

static const DECLARE_TLV_DB_SCALE(db_scale_5bit_3db_max, -4350, 150, 0);

static int patch_alc650_specific(struct snd_ac97 * ac97)
{
	int err;

	if ((err = patch_build_controls(ac97, snd_ac97_controls_alc650, ARRAY_SIZE(snd_ac97_controls_alc650))) < 0)
		return err;
	if (ac97->ext_id & AC97_EI_SPDIF) {
		if ((err = patch_build_controls(ac97, snd_ac97_spdif_controls_alc650, ARRAY_SIZE(snd_ac97_spdif_controls_alc650))) < 0)
			return err;
	}
	if (ac97->id != AC97_ID_ALC650F)
		reset_tlv(ac97, "Master Playback Volume",
			  db_scale_5bit_3db_max);
	return 0;
}

static struct snd_ac97_build_ops patch_alc650_ops = {
	.build_specific	= patch_alc650_specific,
	.update_jacks = alc650_update_jacks
};

static int patch_alc650(struct snd_ac97 * ac97)
{
	unsigned short val;

	ac97->build_ops = &patch_alc650_ops;

	/* determine the revision */
	val = snd_ac97_read(ac97, AC97_ALC650_REVISION) & 0x3f;
	if (val < 3)
		ac97->id = 0x414c4720;          /* Old version */
	else if (val < 0x10)
		ac97->id = 0x414c4721;          /* D version */
	else if (val < 0x20)
		ac97->id = 0x414c4722;          /* E version */
	else if (val < 0x30)
		ac97->id = 0x414c4723;          /* F version */

	/* revision E or F */
	/* FIXME: what about revision D ? */
	ac97->spec.dev_flags = (ac97->id == 0x414c4722 ||
				ac97->id == 0x414c4723);

	/* enable AC97_ALC650_GPIO_SETUP, AC97_ALC650_CLOCK for R/W */
	snd_ac97_write_cache(ac97, AC97_ALC650_GPIO_STATUS, 
		snd_ac97_read(ac97, AC97_ALC650_GPIO_STATUS) | 0x8000);

	/* Enable SPDIF-IN only on Rev.E and above */
	val = snd_ac97_read(ac97, AC97_ALC650_CLOCK);
	/* SPDIF IN with pin 47 */
	if (ac97->spec.dev_flags &&
	    /* ASUS A6KM requires EAPD */
	    ! (ac97->subsystem_vendor == 0x1043 &&
	       ac97->subsystem_device == 0x1103))
		val |= 0x03; /* enable */
	else
		val &= ~0x03; /* disable */
	snd_ac97_write_cache(ac97, AC97_ALC650_CLOCK, val);

	/* set default: slot 3,4,7,8,6,9
	   spdif-in monitor off, analog-spdif off, spdif-in off
	   center on mic off, surround on line-in off
	   downmix off, duplicate front off
	*/
	snd_ac97_write_cache(ac97, AC97_ALC650_MULTICH, 0);

	/* set GPIO0 for mic bias */
	/* GPIO0 pin output, no interrupt, high */
	snd_ac97_write_cache(ac97, AC97_ALC650_GPIO_SETUP,
			     snd_ac97_read(ac97, AC97_ALC650_GPIO_SETUP) | 0x01);
	snd_ac97_write_cache(ac97, AC97_ALC650_GPIO_STATUS,
			     (snd_ac97_read(ac97, AC97_ALC650_GPIO_STATUS) | 0x100) & ~0x10);

	/* full DAC volume */
	snd_ac97_write_cache(ac97, AC97_ALC650_SURR_DAC_VOL, 0x0808);
	snd_ac97_write_cache(ac97, AC97_ALC650_LFE_DAC_VOL, 0x0808);
	return 0;
}

static void alc655_update_jacks(struct snd_ac97 *ac97)
{
	int shared;
	
	/* shared Line-In / Surround Out */
	shared = is_shared_surrout(ac97);
	ac97_update_bits_page(ac97, AC97_ALC650_MULTICH, 1 << 9,
			      shared ? (1 << 9) : 0, 0);
	/* update shared Mic In / Center/LFE Out */
	shared = is_shared_clfeout(ac97);
	/* misc control; vrefout disable */
	snd_ac97_update_bits(ac97, AC97_ALC650_CLOCK, 1 << 12,
			     shared ? (1 << 12) : 0);
	ac97_update_bits_page(ac97, AC97_ALC650_MULTICH, 1 << 10,
			      shared ? (1 << 10) : 0, 0);
}

static const struct snd_kcontrol_new snd_ac97_controls_alc655[] = {
	AC97_PAGE_SINGLE("Duplicate Front", AC97_ALC650_MULTICH, 0, 1, 0, 0),
	AC97_SURROUND_JACK_MODE_CTL,
	AC97_CHANNEL_MODE_CTL,
};

static int alc655_iec958_route_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	static char *texts_655[3] = { "PCM", "Analog In", "IEC958 In" };
	static char *texts_658[4] = { "PCM", "Analog1 In", "Analog2 In", "IEC958 In" };
	struct snd_ac97 *ac97 = snd_kcontrol_chip(kcontrol);

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = ac97->spec.dev_flags ? 4 : 3;
	if (uinfo->value.enumerated.item >= uinfo->value.enumerated.items)
		uinfo->value.enumerated.item = uinfo->value.enumerated.items - 1;
	strcpy(uinfo->value.enumerated.name,
	       ac97->spec.dev_flags ?
	       texts_658[uinfo->value.enumerated.item] :
	       texts_655[uinfo->value.enumerated.item]);
	return 0;
}

static int alc655_iec958_route_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ac97 *ac97 = snd_kcontrol_chip(kcontrol);
	unsigned short val;

	val = ac97->regs[AC97_ALC650_MULTICH];
	val = (val >> 12) & 3;
	if (ac97->spec.dev_flags && val == 3)
		val = 0;
	ucontrol->value.enumerated.item[0] = val;
	return 0;
}

static int alc655_iec958_route_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ac97 *ac97 = snd_kcontrol_chip(kcontrol);

	return ac97_update_bits_page(ac97, AC97_ALC650_MULTICH, 3 << 12,
				     (unsigned short)ucontrol->value.enumerated.item[0] << 12,
				     0);
}

static const struct snd_kcontrol_new snd_ac97_spdif_controls_alc655[] = {
        AC97_PAGE_SINGLE(SNDRV_CTL_NAME_IEC958("",CAPTURE,SWITCH), AC97_ALC650_MULTICH, 11, 1, 0, 0),
	/* disable this controls since it doesn't work as expected */
        /* AC97_PAGE_SINGLE("IEC958 Input Monitor", AC97_ALC650_MULTICH, 14, 1, 0, 0), */
	{
		.iface  = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name   = SNDRV_CTL_NAME_IEC958("",PLAYBACK,NONE) "Source",
		.info   = alc655_iec958_route_info,
		.get    = alc655_iec958_route_get,
		.put    = alc655_iec958_route_put,
	},
};

static int patch_alc655_specific(struct snd_ac97 * ac97)
{
	int err;

	if ((err = patch_build_controls(ac97, snd_ac97_controls_alc655, ARRAY_SIZE(snd_ac97_controls_alc655))) < 0)
		return err;
	if (ac97->ext_id & AC97_EI_SPDIF) {
		if ((err = patch_build_controls(ac97, snd_ac97_spdif_controls_alc655, ARRAY_SIZE(snd_ac97_spdif_controls_alc655))) < 0)
			return err;
	}
	return 0;
}

static struct snd_ac97_build_ops patch_alc655_ops = {
	.build_specific	= patch_alc655_specific,
	.update_jacks = alc655_update_jacks
};

static int patch_alc655(struct snd_ac97 * ac97)
{
	unsigned int val;

	if (ac97->id == AC97_ID_ALC658) {
		ac97->spec.dev_flags = 1; /* ALC658 */
		if ((snd_ac97_read(ac97, AC97_ALC650_REVISION) & 0x3f) == 2) {
			ac97->id = AC97_ID_ALC658D;
			ac97->spec.dev_flags = 2;
		}
	}

	ac97->build_ops = &patch_alc655_ops;

	/* assume only page 0 for writing cache */
	snd_ac97_update_bits(ac97, AC97_INT_PAGING, AC97_PAGE_MASK, AC97_PAGE_VENDOR);

	/* adjust default values */
	val = snd_ac97_read(ac97, 0x7a); /* misc control */
	if (ac97->spec.dev_flags) /* ALC658 */
		val &= ~(1 << 1); /* Pin 47 is spdif input pin */
	else { /* ALC655 */
		if (ac97->subsystem_vendor == 0x1462 &&
		    (ac97->subsystem_device == 0x0131 || /* MSI S270 laptop */
		     ac97->subsystem_device == 0x0161 || /* LG K1 Express */
		     ac97->subsystem_device == 0x0351 || /* MSI L725 laptop */
		     ac97->subsystem_device == 0x0471 || /* MSI L720 laptop */
		     ac97->subsystem_device == 0x0061))  /* MSI S250 laptop */
			val &= ~(1 << 1); /* Pin 47 is EAPD (for internal speaker) */
		else
			val |= (1 << 1); /* Pin 47 is spdif input pin */
		/* this seems missing on some hardwares */
		ac97->ext_id |= AC97_EI_SPDIF;
	}
	val &= ~(1 << 12); /* vref enable */
	snd_ac97_write_cache(ac97, 0x7a, val);
	/* set default: spdif-in enabled,
	   spdif-in monitor off, spdif-in PCM off
	   center on mic off, surround on line-in off
	   duplicate front off
	*/
	snd_ac97_write_cache(ac97, AC97_ALC650_MULTICH, 1<<15);

	/* full DAC volume */
	snd_ac97_write_cache(ac97, AC97_ALC650_SURR_DAC_VOL, 0x0808);
	snd_ac97_write_cache(ac97, AC97_ALC650_LFE_DAC_VOL, 0x0808);

	/* update undocumented bit... */
	if (ac97->id == AC97_ID_ALC658D)
		snd_ac97_update_bits(ac97, 0x74, 0x0800, 0x0800);

	return 0;
}


#define AC97_ALC850_JACK_SELECT	0x76
#define AC97_ALC850_MISC1	0x7a
#define AC97_ALC850_MULTICH    0x6a

static void alc850_update_jacks(struct snd_ac97 *ac97)
{
	int shared;
	int aux_is_back_surround;
	
	/* shared Line-In / Surround Out */
	shared = is_shared_surrout(ac97);
	/* SURR 1kOhm (bit4), Amp (bit5) */
	snd_ac97_update_bits(ac97, AC97_ALC850_MISC1, (1<<4)|(1<<5),
			     shared ? (1<<5) : (1<<4));
	/* LINE-IN = 0, SURROUND = 2 */
	snd_ac97_update_bits(ac97, AC97_ALC850_JACK_SELECT, 7 << 12,
			     shared ? (2<<12) : (0<<12));
	/* update shared Mic In / Center/LFE Out */
	shared = is_shared_clfeout(ac97);
	/* Vref disable (bit12), 1kOhm (bit13) */
	snd_ac97_update_bits(ac97, AC97_ALC850_MISC1, (1<<12)|(1<<13),
			     shared ? (1<<12) : (1<<13));
	/* MIC-IN = 1, CENTER-LFE = 5 */
	snd_ac97_update_bits(ac97, AC97_ALC850_JACK_SELECT, 7 << 4,
			     shared ? (5<<4) : (1<<4));

	aux_is_back_surround = alc850_is_aux_back_surround(ac97);
	/* Aux is Back Surround */
	snd_ac97_update_bits(ac97, AC97_ALC850_MULTICH, 1 << 10,
				 aux_is_back_surround ? (1<<10) : (0<<10));
}

static const struct snd_kcontrol_new snd_ac97_controls_alc850[] = {
	AC97_PAGE_SINGLE("Duplicate Front", AC97_ALC650_MULTICH, 0, 1, 0, 0),
	AC97_SINGLE("Mic Front Input Switch", AC97_ALC850_JACK_SELECT, 15, 1, 1),
	AC97_SURROUND_JACK_MODE_CTL,
	AC97_CHANNEL_MODE_8CH_CTL,
};

static int patch_alc850_specific(struct snd_ac97 *ac97)
{
	int err;

	if ((err = patch_build_controls(ac97, snd_ac97_controls_alc850, ARRAY_SIZE(snd_ac97_controls_alc850))) < 0)
		return err;
	if (ac97->ext_id & AC97_EI_SPDIF) {
		if ((err = patch_build_controls(ac97, snd_ac97_spdif_controls_alc655, ARRAY_SIZE(snd_ac97_spdif_controls_alc655))) < 0)
			return err;
	}
	return 0;
}

static struct snd_ac97_build_ops patch_alc850_ops = {
	.build_specific	= patch_alc850_specific,
	.update_jacks = alc850_update_jacks
};

static int patch_alc850(struct snd_ac97 *ac97)
{
	ac97->build_ops = &patch_alc850_ops;

	ac97->spec.dev_flags = 0; /* for IEC958 playback route - ALC655 compatible */
	ac97->flags |= AC97_HAS_8CH;

	/* assume only page 0 for writing cache */
	snd_ac97_update_bits(ac97, AC97_INT_PAGING, AC97_PAGE_MASK, AC97_PAGE_VENDOR);

	/* adjust default values */
	/* set default: spdif-in enabled,
	   spdif-in monitor off, spdif-in PCM off
	   center on mic off, surround on line-in off
	   duplicate front off
	   NB default bit 10=0 = Aux is Capture, not Back Surround
	*/
	snd_ac97_write_cache(ac97, AC97_ALC650_MULTICH, 1<<15);
	/* SURR_OUT: on, Surr 1kOhm: on, Surr Amp: off, Front 1kOhm: off
	 * Front Amp: on, Vref: enable, Center 1kOhm: on, Mix: on
	 */
	snd_ac97_write_cache(ac97, 0x7a, (1<<1)|(1<<4)|(0<<5)|(1<<6)|
			     (1<<7)|(0<<12)|(1<<13)|(0<<14));
	/* detection UIO2,3: all path floating, UIO3: MIC, Vref2: disable,
	 * UIO1: FRONT, Vref3: disable, UIO3: LINE, Front-Mic: mute
	 */
	snd_ac97_write_cache(ac97, 0x76, (0<<0)|(0<<2)|(1<<4)|(1<<7)|(2<<8)|
			     (1<<11)|(0<<12)|(1<<15));

	/* full DAC volume */
	snd_ac97_write_cache(ac97, AC97_ALC650_SURR_DAC_VOL, 0x0808);
	snd_ac97_write_cache(ac97, AC97_ALC650_LFE_DAC_VOL, 0x0808);
	return 0;
}


/*
 * C-Media CM97xx codecs
 */
static void cm9738_update_jacks(struct snd_ac97 *ac97)
{
	/* shared Line-In / Surround Out */
	snd_ac97_update_bits(ac97, AC97_CM9738_VENDOR_CTRL, 1 << 10,
			     is_shared_surrout(ac97) ? (1 << 10) : 0);
}

static const struct snd_kcontrol_new snd_ac97_cm9738_controls[] = {
	AC97_SINGLE("Duplicate Front", AC97_CM9738_VENDOR_CTRL, 13, 1, 0),
	AC97_SURROUND_JACK_MODE_CTL,
	AC97_CHANNEL_MODE_4CH_CTL,
};

static int patch_cm9738_specific(struct snd_ac97 * ac97)
{
	return patch_build_controls(ac97, snd_ac97_cm9738_controls, ARRAY_SIZE(snd_ac97_cm9738_controls));
}

static struct snd_ac97_build_ops patch_cm9738_ops = {
	.build_specific	= patch_cm9738_specific,
	.update_jacks = cm9738_update_jacks
};

static int patch_cm9738(struct snd_ac97 * ac97)
{
	ac97->build_ops = &patch_cm9738_ops;
	/* FIXME: can anyone confirm below? */
	/* CM9738 has no PCM volume although the register reacts */
	ac97->flags |= AC97_HAS_NO_PCM_VOL;
	snd_ac97_write_cache(ac97, AC97_PCM, 0x8000);

	return 0;
}

static int snd_ac97_cmedia_spdif_playback_source_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	static char *texts[] = { "Analog", "Digital" };

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = 2;
	if (uinfo->value.enumerated.item > 1)
		uinfo->value.enumerated.item = 1;
	strcpy(uinfo->value.enumerated.name, texts[uinfo->value.enumerated.item]);
	return 0;
}

static int snd_ac97_cmedia_spdif_playback_source_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ac97 *ac97 = snd_kcontrol_chip(kcontrol);
	unsigned short val;

	val = ac97->regs[AC97_CM9739_SPDIF_CTRL];
	ucontrol->value.enumerated.item[0] = (val >> 1) & 0x01;
	return 0;
}

static int snd_ac97_cmedia_spdif_playback_source_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ac97 *ac97 = snd_kcontrol_chip(kcontrol);

	return snd_ac97_update_bits(ac97, AC97_CM9739_SPDIF_CTRL,
				    0x01 << 1, 
				    (ucontrol->value.enumerated.item[0] & 0x01) << 1);
}

static const struct snd_kcontrol_new snd_ac97_cm9739_controls_spdif[] = {
	/* BIT 0: SPDI_EN - always true */
	{ /* BIT 1: SPDIFS */
		.iface	= SNDRV_CTL_ELEM_IFACE_MIXER,
		.name	= SNDRV_CTL_NAME_IEC958("",PLAYBACK,NONE) "Source",
		.info	= snd_ac97_cmedia_spdif_playback_source_info,
		.get	= snd_ac97_cmedia_spdif_playback_source_get,
		.put	= snd_ac97_cmedia_spdif_playback_source_put,
	},
	/* BIT 2: IG_SPIV */
	AC97_SINGLE(SNDRV_CTL_NAME_IEC958("",CAPTURE,NONE) "Valid Switch", AC97_CM9739_SPDIF_CTRL, 2, 1, 0),
	/* BIT 3: SPI2F */
	AC97_SINGLE(SNDRV_CTL_NAME_IEC958("",CAPTURE,NONE) "Monitor", AC97_CM9739_SPDIF_CTRL, 3, 1, 0), 
	/* BIT 4: SPI2SDI */
	AC97_SINGLE(SNDRV_CTL_NAME_IEC958("",CAPTURE,SWITCH), AC97_CM9739_SPDIF_CTRL, 4, 1, 0),
	/* BIT 8: SPD32 - 32bit SPDIF - not supported yet */
};

static void cm9739_update_jacks(struct snd_ac97 *ac97)
{
	/* shared Line-In / Surround Out */
	snd_ac97_update_bits(ac97, AC97_CM9739_MULTI_CHAN, 1 << 10,
			     is_shared_surrout(ac97) ? (1 << 10) : 0);
	/* shared Mic In / Center/LFE Out **/
	snd_ac97_update_bits(ac97, AC97_CM9739_MULTI_CHAN, 0x3000,
			     is_shared_clfeout(ac97) ? 0x1000 : 0x2000);
}

static const struct snd_kcontrol_new snd_ac97_cm9739_controls[] = {
	AC97_SURROUND_JACK_MODE_CTL,
	AC97_CHANNEL_MODE_CTL,
};

static int patch_cm9739_specific(struct snd_ac97 * 
 * )
{
	return patch_build_controls(
 * , sn/*
 * _cm9739ela <pere, ARRAY_SIZE(.cz>
 *  Universal inter));
}

static intJaroslaUniverspost_spdif(structx.cz>
 *  Copyright (c) by Jaroslav Kysela <perex@perex.cz>
 *  Universal intert specface for Audio Codec '97
 *
 *  For mt specore details lofication revisiav Kyseopsto AC '97 compedis= {
	.v Kysespecific	=to AC '97 comp it unde,dify
 *  onent spec  the terms of tonent specenerupdate_jackor mUniverse Software F
};etails look to AC '97 comification revision 2.2
 *  bunsigned short val;

	
 * -> can redis= &tribute it and/oThis/* CMiver/A has no Master and PCM volume although the regi *   reacts */is progrflags |= AC97_HAS_NO_MASTER_VOL |
 *   MERCHANPCMTY o;
	.cz>
 *  write_cachex@perex *   TABILI, 0x8000)ULAR PURPOSE.  See the
 *   GNU GenPCMublic Licenhat icheck icensemplival =x.cz>
 *  read *   GNU GenEXTENDED_STATUScensif (GNU &e
 *   A_SPCV)modiat ienableopy of in the AR PURPOSE.  See the
 *   GNU Gent will_SPDIF_CTRL,
				 111-neral Public License
 *  ite 330, Boston, ) | 0x01censs progrrates[ *   RATES0, Bos] = SNDRVA PARChip_4c Li; /* 48k only the } else the  progrext_id &= ~
 *   I0, BostaticdisSoftwextended-id Foundpatch.h"

/*
 *  Chip specific i0;
	}eceiveset-up multi channel the /* bit 14: 0c in Bos, 1 = EAPD idx++)
		if 3:ee Softwinternal vref output for mic idx++)
		if 2:ruct snd_cecnew/lfe (swithSoft) idx++)
		if 0rn err;
	rsurround/lin}

/* rceplace with a new 9: mix 2ic void r off the +)
		if4: undocumturnd; 0 mutesY; wit willA, which defaults to 1	      const3unsigned int *tlc void r?	      constLV *B the GNU General Public License
 *  ite 330MULTI_CHAN) & (1 << 4censGNU |=_MIXER;3	kctl = snd_ctl_f1ind_iogra! x@perruct snd_ae
 *   7, cons))
	id(ac97->bus->c
	kctation, Inc., 59 Temple Place, Suite 330TL_ELEM_IF,*    receiveFIXME:idx  up GPIO the R PURPOSE.  See the
 *   GN0x70unsi01License for more details.
 *
 *  0x72ed sh02icens/* Sit ual exception, ac9ASUS W1000/CMIiver. It doesefut have an= snd_ in. the ogra	if (kpci &&
2111-1	if (ksubsystem_vendor == 0x1043);
	page_save = snd_ac97_rdevice97, AC9843o the ation, Inc., 59 Temple Place, Suite 330, Boston, MA  0307 USA
 *
 */

#include "ac97_local.h"
#inc& ~de "ac97_ set to the page, update bits and restore the pageret = snd_ac97_update_bits(ac97, regTL_ELEM_IFAC| = tlv;
}
cens	forc) by J0e det#defet_tce = SNDR61>page_mutex	0x64t;
}

/*
 * shared liFUNCcont6t;
}

/*
 * shared li, Boston, contcetails lovoidndati61; either versification revisionpyright (c int ac97d a co; wibits, ac9each model
	 *111-1  o->typ 83 is confirmedstruworke = pageils lo version.
 *
 *c vo_on[3][2ic ithe {short08 short00 },atictl_e-78 & 82 Foundms;
	ifned sh008value.enumer82 rev.strcpynums - 1)
		uinfo->value.enume3ch_bui
/*  = 1;
	uinfo->value.eclferated.items = nums;
	ifned sned value.enumerated.item > nums ned (uinfo->value.enumererated.item = nums - 1;
	s 0;
}

static inue.enumerated.name, texts[uinfo->numersharednumerated.item]);
	return04->value.enumerated.item > nums - 1)
		ui};
	return ac97erated.item = nums - 1;
	stuinfo, texts, 2)e.enumerated.name, texts[uinfo->values[] = { "Shared", "Inde2ck_mode_88>value.enumerated.item > nums - 1)
		u2
{
	struct snd_erated.item = numsucontrol)
snd_ctl_elem_info *uinfo) version.
 *
 *    err;
ctl = sndnumerateave = spec.dev_ranty][is_c void r_onpage_)]kctl = sndvalue.enrround_jack_mode_put(struvalue.etrol *kcontrol, s*texts[] = {rround_jack_mode_put(struc[] = uct sndttrol *kcontrol, struct snd_kcontrol_chip(kcontrol);
	unsignedvalur indep = !!age(struct e Softwnumssid.iface = SNDR line-in/mic sign3c88e */
stadetails loconstree softwarekla <per_newx.cz>
 *  Univ61ela <pere[ems = n *   SURROUND_JACK_MODE_CT MA ce = SHANNELc97_channelon 2 of the Lic		retur spec_out_source_infoification re->update *->update,ree softwarectl_eleminfo  *unfo ght (acks)
		har *text
	retur "AC-Link", "ADCcont snd_-In" on 2	ch", ->typec initialCTL_ELEM_TYPE_ENUMEChipDp_suontrolcount = 1channel_movalue.enumeh"

d.itemstri3d, &sidt snd_kcontrol *kcontrol, st > 2ctl-_elem_value *ucontrol)
{
	str= 2ratedrcpyl_elem_value *ucontrol)
{name, c97_ent snd_kcontrol *kcontrol, st]cens/
	return retol *kcontrol, struct snd_ctl_elem_iget*uinfo)
{
	static const char *texts[] = { "2ch", "4ccontr *ula <per"8ch" }
			       const ch Genera->update_chip(->update rece7->page_mureg/*
 *   int ac97_e] &ruct snd_la <per_kcontrol *kcontrol, st[0ic i2taticnfo, tloopback>cound_conode = ucontrol->value.enume, Boston, d.itemt snd_	if (mode >= kcontrol->private_value1taticADC EINVAL;

	if (mode = mode;
		if (ac97->build_ops->update_staticAC-lin

	if ;
}

static int ac97_channel_mode_put(struct sndpucontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_ac97 *ac97 = snd_kcontrol_chip(kcontrol);
	unsigned char mode  mode;
		if (ac97->build_ops->update=ct sndc) by Jd) {
		ac97->indep_surround = indep;
		97_etructtructcense for mor*/
#define AC97_CHANNEL_MODE_CTL \
	{ \
		rn 0;
}

st	= SNDRV_CTL_ELEM_IFACE_MIXER, \
		.nam, Boston, Ml_moMA  02111-e_get, \
		.put = ac97_surround_jack_m1 ?l_mo :el Moe_jacks)
			ac97turn a		returdac_clocknum_text_info(kcontnfo, text, "Bothts,
	acks)
			ac97->build; youl *kol, strune AC97_C, \
		=l_mode_ int_SINGLE(info, \
		.get = ac97_ch9, 3,	.name	= "Channel recacks)
			ac97->build_ops->update_jacks(ac97);
		return 1;
	}
uct sn	return {aticBIT 1:eturn Stem = .iface	vate_value);
}

sIFACE_MIXERSK, erateCTL \
	{ \
		NAME_IEC958("",PLAYBACK,NONE) "Selem_"CTL_Eh", "undatitruct snd_ctl_elem_info CTL_Egeet(snnel_mode_put(struct snd_kcCTL_Edx],el_mode_get, \
		.put = acpu_cha},**texne A2: IG_SPIV>coun0;
}

ac97_cCE_MIXER, \
		.name   = "CAPTUREMode", \Valid S(stru"e_info, \
		.get = ac97_ch2, nnel \
	}

stat397_CI2Fint is_surround_on(struct snd_ac97 *ac97)
{
	return ac97-Monitore >= 1;
}

static inline in3 is_clfe 
	}

stat4 snd_aSDIint is_surround_on(struct snd_ac97 *ac97)
{
	returSWITCH)e_info, \
		.get = ac97_ch4 is_clfe_on(struc9-TLV DAClue)int is_sur int("DAC C97_C 
		.info	.name	= "Channel Mode)kcontrol *kcontroo AC '97 c61ponent specification revision 2.2
 *  by Intel Corporation (http://developer.intel.comalue = 2, \
	}
/*face for Audio Codec '97
 alue = 2, \
	}
/*ore details look to AC '97 ctructt unde7 *ac97)
{
	return !ac97->indep_surround && is_clfe_on(ac97);
}

/* system has shared with line in enabled */
static inline program is free software; you can redistribute it61nd/or modify
 *   it under the terms oc97 *ac97)
eneral Public License as publishetruct snd_ac9 Free Software Foundati_elem_info *uinne int is_shared_clfeout(strfo,
			       const char ** version.
 *
 *   Thist it wi61e usefulWITHOUT ANY WARRANTY; without even the impli/*l,
 *   OUT ANYsestruto ret;
_some_ influence on int analoge = Sindx],sid rso->couned warranty of
/* *   MERCHANTABILITY or */FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public808cense for more details.
 *
 *   You should htrol,
MF7x3 th_jack_mode_puteturn 0;
l_adL_ELEM_ratedispageB, 2, "Small",e.enum7->page_muidck_m *   IDue.enume82o the  version.
 *
 *tmpc97_ived a copageis_creg 0x60tem = GNU General Public License
 *  INT_PAGINGd_ac97_update_bits(ac97, AC97_INT_PAG2)
		uinfo,ram; if ;
	sfnclude "ac97_tmp General Public License= 3;ac97_patch.exts[3] = {
		"Stinfo&_jacks)Smaller"
	, sizelue.enumerated.item = 2;
	strcpy(uinfo->value*/
stauild_conSNDRV_CTL_ELEM_TYPE_ENUMERATED;
3ctl-urn 0;
}

static int sn2This program is distributed in thturn !receivee Software
 *by Keitforce int utex_l		ifin_kcosnd_- codecave;
n't7_updthisal =!YMF7SNDRV_CT	if (kctl && of
 *   97, const Keitto btic ve: we over.  Se int ex97->atu /* 0 impliR PURPOSE.  See the
 *   GNU Gen along with thient" 5cvalue, uDoval > 0)0x0200 here.  T   /resd;
	sissiblesilente to inls[idx],d_ac97_ymf7x3_put_speaker(struct snd \
		.get = ac97_chan00 "acatice Software
 -   Founpatch.h"

/*
 *  Chip specific initialization
 */

static int patch_bor (idx = 0; idx < count; idx++)
		if 5: pc m
 *   beepe,
	e = S		if ((epin4ol_cd(ac/ snd_MODE_SEL, 3:ontrolctl [undation]MODE_SEL, 2: CLFE			ac con(revertedpossrev B)MODE_SEL, 1: Mic/eturn  nsign =
{
	.iface  = SNDRV_CTL_ELEM0: suddid reset_tnsignMODE_SEL,7, cAto inatedonst->t(&sid, 0f7x3_info_8peaker,
	.get    = rolsf7x3_info_7IFACE_ols_	.name mCE_MIXER,0;
}RV_CTL_ELE 5_kcontrselect (l;

Assible to i4: frotrolpeakersible to i3.infr *namespeaker =
{
	.eifac* re = SNDRV_CTL_ELEtic urce tmicf7x3_spdif1:  *  eoinfo(struct sn0 conc boost level (0=20dB, 1=30dDRV_CT/

#if 0= SNDRV_CTL__jack_mode_putctl->tlv, AC0214;_ops->updn 0;
}
x321c;
#endifpy(sid.name, name);
	sid.iface = SNDR line-in/mic 	kctl = snd_ctl_f4trol->urce to  Foun_kcontrol_chip(kcontrol);
	unsigned shore the page */
static int ac97_update_bits_page(struct snd_ac97 *ac97, unsigned short reg, unsigned short mask, unsigned short valu AC97_SURROUNDNDRV_CTt;
}

/*
 * shared80_SIDEcont0pdif_source_get(strucnt acont2pdif_source_get(strucNDRV_controls
 */
static intlue e-in/mic conttext_info(struct snruct Boscontrol, struc
	}
/* 4ch */
#d80_ch_to thenum_textFrce _CTL id out"Cturn 0LFEntroRear	= SNDRV_CTL_ELEM_IFACE_MIXER, \
		.nam>regs[AC97_Y Mode", \
		.info = ac97_channel_mo97 *ac97 = sn, 6surro 0;
}

static in)SNDRV_CTL_ELEM_IFACE_M_ops->update_jack 0;
}

s 1;
	}
	return 0;
}
DOUBLE(
	uco PlayAL;

el_mode >= 1;
}

sruct sn, 15, 7 is_c1fe_o7 = snd_kcontrol_chip(kcontVUT AN	unsigned short val;
 (ui, 3s_clfe_oared jacks rol_chip(kcontRout out ena}

static int sndline int is_shared_clfeout(s807 *ac97)
{
	return !ac97->ipyright (c) by Jaroslav Kysela <perex@perex{
	struct snd_aface for Audi{
	struct snd_a program is free software; you can redistribute it80nd/or modify
 *   it under the terms oL, 0x0002, 
static inline int alc850_is_aux_back_surroundalueidentical snd_ahust (d*/(ac97, AC97_YMF7X3_DIT_CTRL,following snd_ac97_ymf753_... items added by Davi program is distributed in th ac97) = ac97->regs[AC97_YMF7X7->page_muctl && kctl->tlv.p)
		kntrols(struh"

/*
 *  Chip specific initialization
 */

static int patch_buy(sid.name, name);
	sid.iface = SNDRl);
	unsac97_l = snd0xjacks)
	un_EN Foundation, Inc., 59 Temple Place, Suite l);
	unsuct snd_ctg */
	return ret/*
 * VIA VT1616 & 3;

;
	kacks)
			ac97->build_ops->update_jacks(ac97);
	 = 2, \
vt3] =	returns_surround_o"DC Off_updremoval"tem]5a, 10 is_clfe_DIT_CTRL, 0, Alnew(&te Lt sndto SPDIF outOuRL]; SNDRV_5TL_ELEM_IFACE_MIXER,
	Downonstols_but l->valCTL_T_CTRL]; SNDRV_t is_clfe_"Source",
		.info	= sNAME_IEC9spdif_source_info,cont_clfe_on 2 of the
	}
/* 4ch *slave_v	    AC97_YMF7X3_	IT_CTR.item[0] > 1)
		re
	"NAME_IEC958("", NONE, NONE) "ymf7x3_58("", NONE, NONE) "ols_58("", NONE, NONE) NULL3_spdif_source_put,
	},
	AC97_swGLE(SNDRV_CTL_NAME_IEC958("", NOel_mode ) "Mute",
		    AC97_ontrols(ac9CTRL, 2, 1, 1)
}ontrols(ac9nt patch_yamaontrols(acbuild_spded.iind aet  ertput. */
, "4t snsnd_a; wigiven rate7_SINGLE(SNuinfo)
{
	static cons.cz>
 *  (ac9_				 "2chval);
}

static int paMA  0202111-
	}
/* 4ch *rated_ac97 *ac97 = s2ch", "4chd id;
	memset(&id uconsizeof(id pagiidE_8CH_ivate_value);
}

se  = SNDRV_kcontrol);imerated.no prMode", \
		.in2ch"set di Licengrams->card, &id_valueivedre	= Sa virtual AC97_YMspeaker but add 	AC97 implails look t.cz>
 *  add_vAC97_YM S/PDIF params */
	/* yright,no pMA  02		ac97 version.ok t*tlv,no copyright,
	AC97sd_ac97 *ac97 = static const tl;
_ac97 *= &patch, ACnt errc in |=  Genera2ch"make_h_yamah_};

starated.ilvrd, &sid) |= de_put, \
	-ENOMEMnverrr 0x04 << 10admaha_ymf743_ops = {ates[d, &sidSNDR< 0de_put, \
	97->cap ac9(	"Sth_yama; *s; s++ate_bit (err < 0)
		return e|= ACstru |= 0x04 <
	/* set default PCM@perex*sac97_&sid)pec 97_cneif (uprintdd("Canint (ac97	AC97 %s, skipped\
		.to be o_ac9tinuec97_}
	 SNDRV_PCM_RATE_48_	AC97( |= ,  at pbe outpu_id |= AC977_EI_SPDIF; / or ;
}

static int ac97_chaarosla AC97_7 *ac97)
{
	return !ac97->indep_surrof743_ops;
	ac97->caps |= AC9
	ac97->capogra.cz>
 *  trydep_ted.item]NDRV9kctl-ograd, an the terv Kysela <perex@perex&CK, SWITCH),
		    AC97_Y0]ontr)nd the S/PDIF signal icontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	static ch1]face for Audio Codec '9),
		    AC97_) -*texts[3] = _EI_SPDIF; /* /* TelemYPE_allic y7,
		srated AC97_YMv(stru.  Res_spdit. = snds |= 0x04 <that the S/PDIF signal is ",
 *   item[0] > 1)
		r
	ac97->rates[AC97_RATES_SINVALround) {
		acrnumer_SINenumerated.name, texts[uinfntroE_IEC958("", Ne.en= SNDRV_PCM_mf7x3_3d,
};

starated.name, texts[uinfo->valueMA  02111 |= ->tlv.p,ut the_SINGLE(SNDRc97->ext_id |= AC97_EI_SPDIF; /* ontrol, struct snd_ctl_elem_value *ucontrol)
{
	stontrols(acaudio,builcontrol_
{
	int erc97->ext_id |= AC97_EI_SPDIF; /* ;
}

static int ac97ee software; you can redistributbit to d/or modify
 *   it under the terbit to mute S/Pion 2 of the License,  AC97_ification revision 2.2
 *  b program is distributed iin_put(strn 0;
}

static itrols_s[3] 7A
{
	AC97_SIntrol->unfortue	= ly,743_c AC977astatsh	structtwiddler *ucquired3D_Mols_noodling743_ci/o are Foon 2 differt sntrol. that meanstruat= vacan't[0] >srn 0;
}asy way provided byct snd_fo =nd_kcon)= varet;
;
	u  Seols_nameown funcs.
 rols_NB:)    /e.enbsoluturnspdifutterlycontrol->vafrom-EINVAL;
	8. dunno, 0x0bou0)  e 3] =/* T.item copied.io pi; youct snd_kcare _->tyinfo *e wit patch_yamaha_ymf7x3AL;
	va_smart51info *uinfo)
{
	static const char *tPCM auexts[] = { "2ch", "4ch", "6ch", "8ch"/* order) ? )
{
	is lisvaluf thesVAL;
	valdocs)
{
	Reg 20spdie = S7aspdifTSoftw6tem[0]laysls[iwritematrix NB WRT	= snd6: SM51dicatesnd_ac
	reAND* it's Bit14,	int ,
		5 sontrol-_ac97_s verye = Sde_geer-intuitivpdif oundf_source_put,
	}*.item[0um_textLineInFACE1ntro	= SNDRV_CTFACE3nd_ac97 *aE) ""Mutesnd_/C",PLAYBNAME_IEC9Pin",
		.infACK,NONE) "Ou	= SNDRV_C2nfo	= snd_a,
		RV_CTL_ACK,NONE) "Output PinRV_CTL_NA_pin_get,
		.o,
		.eratc) by JIXER, \
	_c97_info *t char *tennel_d.item[, ol,
c int ac97_chanst struct snd_kcontrol_n_kcontrol *kcontrol, struct snd_ctPCM au_elem_value *ucontrol)
{
	struct snd_ac9u
 *
 *usd_ac, usMS;  SNDRVcation revisionp
 * ;
	
	h_buil_chip(kcontrol);
	unsigned chaaticgrab & 3;
	handlpdif,MIXERdif, our desated.nt s -EINn AC9f743_m togethere	= a mountrce_put,nsut eaha_ymf7= snd_aco Jarted.7l)
{
	st("",PLAYBAC invd_acd_ac General Public Lh_buisignea) >> Conve	int  	= patch_yamaha_ymf7x3_3d,20.buil8;
 .bui	if (mode >= kcontrol->private_value(ild_3d	s->c) +
	int 	return 0;
}

static in NONE) "Mute",
		    AC97_YMFRV_CTL_ELEM_IFACE_MIXER, \
		.nameint patch_yamaha_ymf753_post_spdif(struct snd_ac97 * ac97)
{
	int{
	iReg err;

	if ((err = patch_buildcontrols(ac97, snd_ac97_ymf753_controls_spdif, ARRAY_SIZE(snd_ac9ild_3d	= tatic int patch_yamaha_ymf753(strucuildpost_spdif =tatic int patch_yamaha_ymf753(struc&  (EAMIXERpush3_concontrol)
t7_ymf7thout eve) & nsidx3_sm[0]things will.iteleftdicate  aal);kystatiem_varitereturnfail= {
	MF75NE) = patch_yamaha_ymf7x3_3d,
	.
/* set to the page, updmf7x3_3d,
	, sndNE) item3FFFc97)snd_ac97 * ak pagi/
	ac97->build_ops = &patch_yam valuef753_ops;
	ac97->caps |= AC97_20_BASS_TREBLE;FEac97->capspdif<<  8 pagreturn 0;
}

static inDRV_CTL_NAME_IEC958("", PLAYBACK, SWITCH),
		    AC977a	return is_surround_ool->value.e Ex coug	retstruct8 is_clfe_on(e = SThesex0028usTED;
	e Soft/*/
static void rthe Yaops mo
	reboaramaha e is aret;
3 bidirec paga_spdac97  is_su->counthe 7_YMF7Xt PlaybaTL \
	{ \
		.iface  = SNDRV_CTL_ELEM_t Playbac = "Sntro 5.1 So thefo = ac97_ct Playback nst struct snd_kcontrol_new sc97_channm9703_specific(struct snd_ac97 * ac9797_channel_mm9703_specific(struct snd_ac97 * ac97e = 4, \
m_value *ucontrol)
{
	stru7aification revision 2.2
 *  b
	ac97- err;


	acby David S == hoo20 :o	int egis_spdis alsos point, bu0] ==tell-spe	pagecaller w< 0)w not by Jr.
	*ol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	static 7ahar ated.ite;
	uinfo->count = 1;
	uinfo->value7aemoved+)
	.name 0, 31,pow743_busumt pageols);rmal_upd by ) ? ,
		>bus->* headphone amplifier, like WinXP driver, ac9EPIA SPAC97_SI/* We neTED;
	igned int num beforecificname	t.wm970On > */ (many?) hardware/PDIetct snwolfacamahly clears it!AC97_SIGNU General Public License0x5crd, &sid)am; if 
	retctl-, unsigned short mask, unsign5c*/
	returntrol_ch--;
	ucontrol->value.enuX3_3D_MODE_SELdetR_VOL, ofopy of the w(&snd_ac97_ymf7x3_controls_speaker,
					ac97))4100 |initialization
 */

stntrol_chip(kcontrol);
	unsigned short vif_output97->cld_spdspdif[3] 8 8 info(stTYPE_ CODEC* The fspdiimp_yamahs 'static in' coMIXEed toontrol->vlypossible"Rearthanols_isave;
	olume", AC7a. awe> */! wm9yeharadebian.or_elem_d SNDRols_particulaven aller"
or-spectechnologynd_ac9> */bodyld_co,ce_geols_rd, sd Un_wolsal Audio JL;

but it.
 *wsdateoac97)
{aybac folk's ;
	u_specas well/* The f,
		.name	= SNDRV_CTL_NAME_IEC958(""8LAYBACK,NONE) 60hource", ACe bcentediagram,systsx0028as)
{llows:* The f))) < 0OUT_O  = T_CTR,
 *	0)
			1  = NAME_IEC;
	}
	/* pa2    =ue.e* The fUncific, 0, 31, ,	uinfoOUTe useac struct snd_updof mappo a  snd ac9a to itpact Sns = 0; i_WM9 00], ac97))) < 0)01))) < 0Uo	= eC958(pu? 0x0olfson_10olfson_	= S I9704_lfson_wm.build_sMic c,
};
 sndnsigned Case

stn_wm9704_ops = {

			9704_Mific = patch_wolfson_w* patc e.enserve, sndps = &patch2wolfson_wm9704_ sndI_WM97Xno, "3a wis alse hc97-son_wm97ave;
_ctl_aon

	mMSI sndCN700T, iMF7X3_DIT_upditac97hann ints[idx],- YMMV, ba, sndshit may happen/* The fIf = 0; iY_SIZ0020 ruct snd_ac97 * ac97)) < 0)(err & 3; mighlfso applicSoft snd_7_ymfmhe(ac97
	if ((nd_ctl_uaj_
	str3_... items added maskp_surround;
	returshiftAC97_BC_BASS_TR, str[4];controlslue *_CTL_NAME_IEC9n 47 (EAPDLAYBACK,NOVad(ac9D}

/*dONE)ut eve_kcok(&actatic int snd_a
	}
	snd_ac97_wr
	}
	snd_a[38ch */
#>valuespeaker, 1),EL_MODEx72,
};
0x03CTL_E0;
}
710 o = ac structn 48."Sac97)
{58("", withpecific = pnfo	= sn \
		.i_wolIn"), or  \
	_ac97 *set_t
	/* WM9705, WM9710 */
	if97->build_nel_m &patch_wolfson_AME_IEC958("",ifdef CONFIG_TOUCHSCREEN_WM9705
	/* WM9705 touchscreen 	   
	/* WM9705, WM9710 *3ops = ->build_4gs |= AC97_HAS_NO_Vymf7x3_

st97_HAS_NO_AUX;
#endif
	return 0;
}

static constom>
	 * WM9703/9707/9nst struct snd8_UAJnew snd_ac97_ymf753_controls_spdif[3] = {11-13ts[] = { "2ch", "4ch", "6ch", "8ch"_output_pin_put,
	},
	AC97_SINGLE(SNDRV_CTLated.iteolfson05(st->update->privoftwcontr] &patc
static c
	kcld_spdAll1, 1),
}nd_ctl_ruct snd_ac97 * ac97)->value.en0028,9704_specific = patch_wolfson_wm9, patch0. wm9pdif)))but thuruct f_soursk,ar Copeci patclso ae is aic conruct
}

static const struct sndout3_s7X3_DIT_CTRL, 2, 1, 1)
};

static int pMono"};
static const chst_spdif(struct snd_aversion.
 *
 *datpag, uajkcontrf ((err = patch_buil_chip(kcontrol);
	unsigned char m
{
	x_97_C(&h_bui->patc_c conlayba", "Ma97->build_ops = &patch_yem > 2)
		uinfo- kctl->tPAGEneraKace	= SNDRV_CTL_ELEM_IFA};

static const struc   You sm wm9711nel Mo
	uaj97->build_ops = &patch_yam60) &3_3Dnst char* wm9711_rec_adc[] = {"Sterx72, 0enum[] = {
AC97_ENUM_SINGLE(AC97_PCI_SVID, 14, 4, wm9711_alc_se", "Maa_ymc consunt char* wm9711_ng_type[] = {tatic int patch_yamaha_ymf753(struct 
AC9>>4, wm9711_alc_mix),
AC97_ENUM_SINGLE(AC0;
}

s not output.
   There is a711_mic[] = {"Mic 1"ust, dshust@shustring.com.
	   This chitatic const char* wm9711_rec_sel[] = 
	{"Mi_output_pin_97->indep_s_patc3_spdkcontrol);
	unsigned chtem]);3_spdif_outwm9711_alc_mix),
AC97_ENUM_SINGLE(AC97_A3_spdif_outtatic int patch_yamaha_ymf753(stru<<, wm9711_ng_type),
};

static const struct snd0;
}
3_spdif_oute_value, 0x6nfig auxF ou
	/* -s); i+"Front P3me", Alayback Volu orthe Yaps =ter.
	EC_GAIN, 14, 6, 2, wm9711_rauxsrc[] = {"Left", "VREF", "Left + Right", "Monexts[] = { "2ch", "4ch", "6ch", "8ch" };
	ret_BC_BASS_TRtxtREV,
		.na"Aux_WM9705Bvalueput TOUC,
		k_output_pin_put,
	},
	AC97_SINGLE(SNDRV_CTL_Nwm9711, 2958("", NONE, NONE) "Mute",
		 _REV, , "Differential", "Mic 2", "Stereo"};
static const char* wm9711_rec_sel[] = 
	{"Mic	if (mode >= kcontrol->private_valu
		3_spdif_oulic L97_ENUM_SINGLE(AC97_PCI_SVID, 55ct ac	uinfo)>> sndDOUBLE(AC97_REC_GAIN, 14, 6, 2, wm9711_rEV, ain),
AC97_ENUM_SINGLE(AC97_MIC, 5, 4, wm9711_mic),
AC97_ENUM_DOUBLE(AC97_REC_SEL,ratedgg
AC97_DOUBLErear dacstatic r.
	*	= SNDRV_CTL_ELEM_IFAnum[9]),
AC97_SINGLE("ALC NG Thre
		uinfo
statnew wm9711_snd_ac97_controls[] = {
AC97tl_find_1),
AC97_ENUold TimM("ALC Headphospdi
	/* WM9ode", \
		.info = ac97_channel97_ENUM_SINGLE(AC97_PCI_SVID, 576DEO, 7, 7, 1)de_get, \
		.put = ac97_channel_mode_putl_find_lfson00 patch.
 *  added support for WM9705,WM9708,WM9709,WM9718	return 0;
}

nd WM97atic con_ymf7x3ue.enumstructt snd_k),
AC9value.enTRL, 0, 1, 0),
	{"", AC9s = {
	SNDRV_CTL_EL AC97_PC_BEEP, 12, 7, Soft M
	retu
AC97_SINGE("Frouconne Swittrol->valu1),
AC97H_ops = {
Amp),
AC97_SE("BeepCK, NOde Tone Volumend_kcontINGLE("ALC 1)
		retAC97_Se Volumcontrolone Volume", AC97_INGLE("ALCel_mode >),
AC97_SACK, NOde Tone VNGLE("Front Playback Switch", AC97_WM97XX_FMIXER_VOL, 15, 1, 1),
};

sac97)
{ac97)Mocontolfson_wm9703_specific(struct snd_out3_src[]	/* This is known to work for the VMic 1", "ad 1000
	 * Randolph Bentson <bents_rec_gainchanneadc[] = {"St710  touchscre("Front Playback Switch", AC97_WM97XX_FMIXER_VOL, 15, 1, 1),
};

REEN_", AC97_CD, 12, 7, 1),
AC97_SINGLE("Aux to Side Tone Switch", AC97_CD, 11, 1, 1),
AC97_SINGLE("Aux to Side Tone Volume", AC97_CD, 8, 7, 1),
AC97_SINGLE("Aux to Phon1 Switch", AC97_CD, 7, 1, 1),
AC97_SINGLE("Aux to Phone Volume", AC97_CD, 4,_wol", AC97_CD, 12, 7, 1),
AC97_SINGLE("Aux to Side Tone Switch", AC97_CD, 11, 1, 1),
AC97_SINGLE("Aux to Side Tone Volume", AC97_CD, 8, 7, 1),
AC97_SINGLE("Aux to Phon2 Switch", AC97_CD, 7, 1, 1),
AC97_SINGLE("Aux to Phone Volume", AC97_CD, 4,]),
", AC97_CD, 12, 7, 1),
AC97_SINGLE("Aux to SideEV, 4, 1itch", AC97_CD, 11, 1, 1),
AC97_SINGLC97_SINto Side Tone Volume", AC97_CD, 8, 7,  Switch 4,  * WM9703/9707/9708/9717 
	8val);
}

static int patch_yamaha_ymf7x3_3d(struct snd_ac97 *adphone Switch", AC97_PC_3_spdif_ou;
	uinfo->count = 1;
	uinfo->valueremovcontrol-
}

statit sndit2646lem_info *uinfo,
			       const char **texnsigneecifi-In /_NAME_IEC958(= snd_kcontrol97->indep_surroundOut3 L97 * 97, 1),

AC
	unsigned char indep = ? (1<<9)vate_vaNTROL, 4, 1_wol/"Beep to HeBass Control", wm9711_enum[5]),
AC97_SINGLE("Ba1 2, wm-off Switch", Ac97->indep_suNE, 12,101, 1),
Alfson00 patch.
 *  added support for WM9705,WM9708,WM9709,_SINGL	return 0;
}

static int ac97_channel_mode_info(struct snd_kcontrol *kco		ac97->build_ops->update_jacks(ac97);
ct sndAIN, 15, 1, 1),
AC97_ENUM("Ca_surrout(struct snd_ac97 *ac97)
{
	return !ac97-_SINGLEc97_ymf7x3M9712 and WM97aker,
)) <name   = patcadphoINGLEt is_clfe_oolume", AC97_NGLE("MI to t has sharehone Swt enabled * WM9703/9707/9708/97_SINGLE *ac97)
{
	return !ac97->indep_surro
	ac97->c "Pin 43", "Pin 48" };

	uinfo->type = C97_REC_GAIN, 15, 1, 1),ED;
	uinfo->count = 1;
	uinfo->_SINGL)rated.items = 3;
	if ("Mic 2 Volume", AC97_MIC, 0, 31, 1),
AC97_SINGLEAC97_SINGLE("Capture face for Audio Codec 'AC97_SINGLE("Capture SINGLE("Master Left Inv turn 0;
}

static int snd_ac97_ymf753_spdif_output_p_SINGLEd/or modify
 *   it under the terLE("Mic 1 Volum Free Software Foun_SINGLE("3D Lower C11_enum[7]),
AC97_SINGLE("Mit snd_ac97 *ac97 = snd_kcontrol_chip(kcontrol);
	unsiER_MONO, 7
AC97_fullsystara <mae_page(struct snd_ac97 *ac97, unsi5Emeratedol,
					struct snd_ctl_elem_inf0x7Aontrols[i], l;

	if (ucontrol->Si303=
{
	AC97_SIt;
}

/*
 * shSI_COD_CHIP_IDBeep to SV, 0x0808);
	snd_ac97LINE_CFG7, 1),
d short val;

	vL_NAME_IEC958("", PLAYBACK, SWITCH),
		   s7_CODYMF7X3_DIT_Cnd_kcont97_Cmruct97)
{ 1)
		retE("Fro	.puwitct en) * WM9703/9707/9708/97cache(c 1 Volume", AC97_MIC, 8, 31, 1),
AC97_SIidx,ft Inv forceidxeturn truc<GLE("3D Upper Cut-off Switch",cache()nd_ac++nd_kcontrol *kcPCM_RATE_48000; /* 48k only *04 << 10new1(_info *uinfo)
{
	stcache(aidx],opyrigexts[3] = { "Disabled", turn 0;
}

static int snd_ac97_ymf753_spdif_output_pc97_wrid/or modify
 *   it under the terc97_write_cache711_enum[7]),
AC9mGAIN | AC97_t snd_ac97 *ac97 = snd_kcontrol_chip(kcontrol);
	unsi

	ac97->f], ac97))) < 0)
			return err;
	VIDEO,f210 i], ac97))) < 0)
			return err;
	6NGLEd_ac97_buil (ucontrol->LM 4550 C
	AC97_ sndWnd_ac al = s loree usend_acSoftwaiis pLM713_r& 3;
	c will bc97))prop S/PDautoprobTED;
	, 1)rmEEN_; with"Right", viaspeci a c_SINume_ "Mono In"()			returnic int snd_ac97_ymf75res_"Mute"lm713_onsttbl 8ch */
#dU General Publi1f1f ouchs
 *   MEADPHONcontr+0.75dB Steps"};TABILITMONOif (uc.75dB Steps"};PC_BEEP= {"None", ),
ALSBdif_igno4, 17_SINft", "Ric constNone", "Left", "MI\
	{ None", "Left", "0808onst char* wm9713_alCDonst char* wm9713_alVIDE = {"+0.75dB Steps"};AUX", "Headphone Mix", should +0.75dB Steps"};REC_GAI>buil0f075dB Ste}atic 2", "ato wm97on 2 of the License, wm9713val);
}

static int patch_y= ucontrt char* = wm9713_rec_ga_write_cache(ac97, 04_opUCB1};
	& 3;
	(http://www.semiconductors.philips.com/acrobat_download/da= (ucets/t char*-02.pdf)97_SINGLE(SNDRV_CTL_NAME_IEC958("", PLAYBACK, SWITCH),
		   ucbhar*	returnl->value.[] = {
AC3_ops = {
ch_wolfelem_iaols[i IXER_Ve Von),
AC97toymf7control3_ops = {
* respdif_soereoof_kconw(&coDC97_cneingymf7capacs sh= {
	olume", AC97_PC_BEEP, 8Dh_wole_cac6aontro_ymf7x3c ini.namd_ac97_coAC97ensvendpeciDCwolf i < sf,
	{
	ac = {

			to
		.ife idac97  tones to pin 47a97 * bandk(&acBEEP, 12, 7, 1),t ac97};

statirround_on(t itpeaker ontro-low-tatic ->ty featuem_vAatic c
	{"mLeft"tatic atic_mixof un_ac977_cneol)
{
	st
			 0, 31,urce tedif,c_gaie PLL7_VIDEO, 14, 4, wmstaticLow Patic 97_CD,

stcC97_E3 wm9711_enum[7]),
AC97_SINGdphone ite_cache(ac97,  AC97_CD, 0x0000);
	return 0;
}

static struct snd_ac97_build_ops patch_wolfson_wm971dphone = {
	.build_specific = patch_wolfson_wm9711_specific,
};

static int patch_wolfson11dphone M snd_ac97 * ac97)
{
	/* WM9711, WM9712 */
	ac97->build_ops = &patch_wolfson_wm9711_ops;
AC97_ENd/or modify
 *   it under the ter
AC97_ENUM_SINGL 4, wm9713_alc_select),
AC97_Et snd_ac97 *ac97 = snd_kcontrol_chip(kcontrol);
	unsiM_DOUBLE(AC
AC97_e SoftwMono", "NC", "VmiD, 1ontro lowC97_VID3_recbyid sid;
rd, snd_ac97_cnew(&wm9711_snd_ac97tati Mix5t reg, unsigned short mask, unsignCI_S Mix3e"};
static const