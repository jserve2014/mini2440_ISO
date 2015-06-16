/*
 *
 * device driver for philips saa7134 based TV cards
 * card-specific stuff.
 *
 * (c) 2001-04 Gerd Knorr <kraxel@bytesex.org> [SuSE Labs]
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>

#include "saa7134-reg.h"
#include "saa7134.h"
#include "tuner-xc2028.h"
#include <media/v4l2-common.h>
#include <media/tveeprom.h>
#include "tea5767.h"
#include "tda18271.h"
#include "xc5000.h"

/* commly used strings */
static char name_mute[]    = "mute";
static char name_radio[]   = "Radio";
static char name_tv[]      = "Television";
static char name_tv_mono[] = "TV (mono only)";
static char name_comp[]    = "Composite";
static char name_comp1[]   = "Composite1";
static char name_comp2[]   = "Composite2";
static char name_comp3[]   = "Composite3";
static char name_comp4[]   = "Composite4";
static char name_svideo[]  = "S-Video";

/* ------------------------------------------------------------------ */
/* board config info                                                  */

/* If radio_type !=UNSET, radio_addr should be specified
 */

struct saa7134_board saa7134_boards[] = {
	[SAA7134_BOARD_UNKNOWN] = {
		.name		= "UNKNOWN/GENERIC",
		.audio_clock	= 0x00187de7,
		.tuner_type	= TUNER_ABSENT,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,

		.inputs         = {{
			.name = "default",
			.vmux = 0,
			.amux = LINE1,
		}},
	},
	[SAA7134_BOARD_PROTEUS_PRO] = {
		/* /me */
		.name		= "Proteus Pro [philips reference design]",
		.audio_clock	= 0x00187de7,
		.tuner_type	= TUNER_PHILIPS_PAL,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,

		.inputs         = {{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE1,
		},{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_tv_mono,
			.vmux = 1,
			.amux = LINE2,
			.tv   = 1,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_FLYVIDEO3000] = {
		/* "Marco d'Itri" <md@Linux.IT> */
		.name		= "LifeView FlyVIDEO3000",
		.audio_clock	= 0x00200000,
		.tuner_type	= TUNER_PHILIPS_PAL,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,

		.gpiomask       = 0xe000,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.gpio = 0x8000,
			.tv   = 1,
		},{
			.name = name_tv_mono,
			.vmux = 1,
			.amux = LINE2,
			.gpio = 0x0000,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE2,
			.gpio = 0x4000,
		},{
			.name = name_comp2,
			.vmux = 3,
			.amux = LINE2,
			.gpio = 0x4000,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
			.gpio = 0x4000,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
			.gpio = 0x2000,
		},
		.mute = {
			.name = name_mute,
			.amux = TV,
			.gpio = 0x8000,
		},
	},
	[SAA7134_BOARD_FLYVIDEO2000] = {
		/* "TC Wan" <tcwan@cs.usm.my> */
		.name           = "LifeView/Typhoon FlyVIDEO2000",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_LG_PAL_NEW_TAPC,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,

		.gpiomask       = 0xe000,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = LINE2,
			.gpio = 0x0000,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE2,
			.gpio = 0x4000,
		},{
			.name = name_comp2,
			.vmux = 3,
			.amux = LINE2,
			.gpio = 0x4000,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
			.gpio = 0x4000,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
			.gpio = 0x2000,
		},
		.mute = {
			.name = name_mute,
			.amux = LINE2,
			.gpio = 0x8000,
		},
	},
	[SAA7134_BOARD_FLYTVPLATINUM_MINI] = {
		/* "Arnaud Quette" <aquette@free.fr> */
		.name           = "LifeView FlyTV Platinum Mini",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,

		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,     /* Composite signal on S-Video input */
			.vmux = 0,
			.amux = LINE2,
		},{
			.name = name_comp2,	/* Composite input */
			.vmux = 3,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
		}},
	},
	[SAA7134_BOARD_FLYTVPLATINUM_FM] = {
		/* LifeView FlyTV Platinum FM (LR214WF) */
		/* "Peter Missel <peter.missel@onlinehome.de> */
		.name           = "LifeView FlyTV Platinum FM / Gold",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,

		.gpiomask       = 0x1E000,	/* Set GP16 and unused 15,14,13 to Output */
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.gpio = 0x10000,	/* GP16=1 selects TV input */
			.tv   = 1,
		},{
/*			.name = name_tv_mono,
			.vmux = 1,
			.amux = LINE2,
			.gpio = 0x0000,
			.tv   = 1,
		},{
*/			.name = name_comp1,	/* Composite signal on S-Video input */
			.vmux = 0,
			.amux = LINE2,
/*			.gpio = 0x4000,         */
		},{
			.name = name_comp2,	/* Composite input */
			.vmux = 3,
			.amux = LINE2,
/*			.gpio = 0x4000,         */
		},{
			.name = name_svideo,	/* S-Video signal on S-Video input */
			.vmux = 8,
			.amux = LINE2,
/*			.gpio = 0x4000,         */
		}},
		.radio = {
			.name = name_radio,
			.amux = TV,
			.gpio = 0x00000,	/* GP16=0 selects FM radio antenna */
		},
		.mute = {
			.name = name_mute,
			.amux = TV,
			.gpio = 0x10000,
		},
	},
	[SAA7134_BOARD_ROVERMEDIA_LINK_PRO_FM] = {
		/* RoverMedia TV Link Pro FM (LR138 REV:I) */
		/* Eugene Yudin <Eugene.Yudin@gmail.com> */
		.name		= "RoverMedia TV Link Pro FM",
		.audio_clock	= 0x00200000,
		.tuner_type	= TUNER_PHILIPS_FM1216ME_MK3, /* TCL MFPE05 2 */
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.gpiomask       = 0xe000,
		.inputs         = { {
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.gpio = 0x8000,
			.tv   = 1,
		}, {
			.name = name_tv_mono,
			.vmux = 1,
			.amux = LINE2,
			.gpio = 0x0000,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE2,
			.gpio = 0x4000,
		}, {
			.name = name_comp2,
			.vmux = 3,
			.amux = LINE2,
			.gpio = 0x4000,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
			.gpio = 0x4000,
		} },
		.radio = {
			.name = name_radio,
			.amux = LINE2,
			.gpio = 0x2000,
		},
		.mute = {
			.name = name_mute,
			.amux = TV,
			.gpio = 0x8000,
		},
	},
	[SAA7134_BOARD_EMPRESS] = {
		/* "Gert Vervoort" <gert.vervoort@philips.com> */
		.name		= "EMPRESS",
		.audio_clock	= 0x00187de7,
		.tuner_type	= TUNER_PHILIPS_PAL,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.empress_addr 	= 0x20,

		.inputs         = {{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE1,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name = name_tv,
			.vmux = 1,
			.amux = LINE2,
			.tv   = 1,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
		.mpeg      = SAA7134_MPEG_EMPRESS,
		.video_out = CCIR656,
	},
	[SAA7134_BOARD_MONSTERTV] = {
		/* "K.Ohta" <alpha292@bremen.or.jp> */
		.name           = "SKNet Monster TV",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_NTSC_M,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,

		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE1,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_MD9717] = {
		.name		= "Tevion MD 9717",
		.audio_clock	= 0x00200000,
		.tuner_type	= TUNER_PHILIPS_PAL,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			/* workaround for problems with normal TV sound */
			.name = name_tv_mono,
			.vmux = 1,
			.amux = LINE2,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE1,
		},{
			.name = name_comp2,
			.vmux = 3,
			.amux = LINE1,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	       .mute = {
		       .name = name_mute,
		       .amux = TV,
	       },
	},
	[SAA7134_BOARD_TVSTATION_RDS] = {
		/* Typhoon TV Tuner RDS: Art.Nr. 50694 */
		.name		= "KNC One TV-Station RDS / Typhoon TV Tuner RDS",
		.audio_clock	= 0x00200000,
		.tuner_type	= TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_tv_mono,
			.vmux = 1,
			.amux   = LINE2,
			.tv   = 1,
		},{

			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
		},{

			.name = "CVid over SVid",
			.vmux = 0,
			.amux = LINE1,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_TVSTATION_DVR] = {
		.name		= "KNC One TV-Station DVR",
		.audio_clock	= 0x00200000,
		.tuner_type	= TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.empress_addr 	= 0x20,
		.tda9887_conf	= TDA9887_PRESENT,
		.gpiomask	= 0x820000,
		.inputs		= {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
			.gpio = 0x20000,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
			.gpio = 0x20000,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
			.gpio = 0x20000,
		}},
		.radio		= {
			.name = name_radio,
			.amux = LINE2,
			.gpio = 0x20000,
		},
		.mpeg           = SAA7134_MPEG_EMPRESS,
		.video_out	= CCIR656,
	},
	[SAA7134_BOARD_CINERGY400] = {
		.name           = "Terratec Cinergy 400 TV",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 4,
			.amux = LINE1,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name = name_comp2, /* CVideo over SVideo Connector */
			.vmux = 0,
			.amux = LINE1,
		}}
	},
	[SAA7134_BOARD_MD5044] = {
		.name           = "Medion 5044",
		.audio_clock    = 0x00187de7, /* was: 0x00200000, */
		.tuner_type     = TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			/* workaround for problems with normal TV sound */
			.name = name_tv_mono,
			.vmux = 1,
			.amux = LINE2,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE2,
		},{
			.name = name_comp2,
			.vmux = 3,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_KWORLD] = {
		.name           = "Kworld/KuroutoShikou SAA7130-TVPCI",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_NTSC_M,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
		},{
			.name = name_tv,
			.vmux = 1,
			.amux = LINE2,
			.tv   = 1,
		}},
	},
	[SAA7134_BOARD_CINERGY600] = {
		.name           = "Terratec Cinergy 600 TV",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 4,
			.amux = LINE1,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name = name_comp2, /* CVideo over SVideo Connector */
			.vmux = 0,
			.amux = LINE1,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_MD7134] = {
		.name           = "Medion 7134",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FMD1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs = {{
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
		},{
			.name   = name_comp1,
			.vmux   = 0,
			.amux   = LINE1,
		},{
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE1,
		}},
		.radio = {
			.name   = name_radio,
			.amux   = LINE2,
	       },
	       .mute = {
		       .name = name_mute,
		       .amux = TV,
		},
	},
	[SAA7134_BOARD_TYPHOON_90031] = {
		/* aka Typhoon "TV+Radio", Art.Nr 90031 */
		/* Tom Zoerner <tomzo at users sourceforge net> */
		.name           = "Typhoon TV+Radio 90031",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
		},{
			.name   = name_comp1,
			.vmux   = 3,
			.amux   = LINE1,
		},{
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE1,
		}},
		.radio = {
			.name   = name_radio,
			.amux   = LINE2,
		},
	},
	[SAA7134_BOARD_ELSA] = {
		.name           = "ELSA EX-VISION 300TV",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_HITACHI_NTSC,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name   = name_comp1,
			.vmux   = 0,
			.amux   = LINE1,
		},{
			.name = name_tv,
			.vmux = 4,
			.amux = LINE2,
			.tv   = 1,
		}},
	},
	[SAA7134_BOARD_ELSA_500TV] = {
		.name           = "ELSA EX-VISION 500TV",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_HITACHI_NTSC,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_svideo,
			.vmux = 7,
			.amux = LINE1,
		},{
			.name = name_tv,
			.vmux = 8,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_tv_mono,
			.vmux = 8,
			.amux = LINE2,
			.tv   = 1,
		}},
	},
	[SAA7134_BOARD_ELSA_700TV] = {
		.name           = "ELSA EX-VISION 700TV",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_HITACHI_NTSC,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_tv,
			.vmux = 4,
			.amux = LINE2,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 6,
			.amux = LINE1,
		},{
			.name = name_svideo,
			.vmux = 7,
			.amux = LINE1,
		}},
		.mute           = {
			.name = name_mute,
			.amux = TV,
		},
	},
	[SAA7134_BOARD_ASUSTeK_TVFM7134] = {
		.name           = "ASUS TV-FM 7134",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 4,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 6,
			.amux = LINE2,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE1,
		},
	},
	[SAA7134_BOARD_ASUSTeK_TVFM7135] = {
		.name           = "ASUS TV-FM 7135",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.gpiomask       = 0x200000,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.gpio = 0x0000,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 4,
			.amux = LINE2,
			.gpio = 0x0000,
		},{
			.name = name_svideo,
			.vmux = 6,
			.amux = LINE2,
			.gpio = 0x0000,
		}},
		.radio = {
			.name = name_radio,
			.amux = TV,
			.gpio = 0x200000,
		},
		.mute  = {
			.name = name_mute,
			.gpio = 0x0000,
		},

	},
	[SAA7134_BOARD_VA1000POWER] = {
		.name           = "AOPEN VA1000 POWER",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_NTSC,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
		},{
			.name = name_tv,
			.vmux = 1,
			.amux = LINE2,
			.tv   = 1,
		}},
	},
	[SAA7134_BOARD_10MOONSTVMASTER] = {
		/* "lilicheng" <llc@linuxfans.org> */
		.name           = "10MOONS PCI TV CAPTURE CARD",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_LG_PAL_NEW_TAPC,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.gpiomask       = 0xe000,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = LINE2,
			.gpio = 0x0000,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE2,
			.gpio = 0x4000,
		},{
			.name = name_comp2,
			.vmux = 3,
			.amux = LINE2,
			.gpio = 0x4000,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
			.gpio = 0x4000,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
			.gpio = 0x2000,
		},
		.mute = {
			.name = name_mute,
			.amux = LINE2,
			.gpio = 0x8000,
		},
	},
	[SAA7134_BOARD_BMK_MPEX_NOTUNER] = {
		/* "Andrew de Quincey" <adq@lidskialf.net> */
		.name		= "BMK MPEX No Tuner",
		.audio_clock	= 0x200000,
		.tuner_type	= TUNER_ABSENT,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.empress_addr 	= 0x20,
		.inputs         = {{
			.name = name_comp1,
			.vmux = 4,
			.amux = LINE1,
		},{
			.name = name_comp2,
			.vmux = 3,
			.amux = LINE1,
		},{
			.name = name_comp3,
			.vmux = 0,
			.amux = LINE1,
		},{
			.name = name_comp4,
			.vmux = 1,
			.amux = LINE1,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		}},
		.mpeg      = SAA7134_MPEG_EMPRESS,
		.video_out = CCIR656,
	},
	[SAA7134_BOARD_VIDEOMATE_TV] = {
		.name           = "Compro VideoMate TV",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_NTSC_M,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
		},{
			.name = name_tv,
			.vmux = 1,
			.amux = LINE2,
			.tv   = 1,
		}},
	},
	[SAA7134_BOARD_VIDEOMATE_TV_GOLD_PLUS] = {
		.name           = "Compro VideoMate TV Gold+",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_NTSC_M,
		.gpiomask       = 0x800c0000,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
			.gpio = 0x06c00012,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
			.gpio = 0x0ac20012,
		},{
			.name = name_tv,
			.vmux = 1,
			.amux = LINE2,
			.gpio = 0x08c20012,
			.tv   = 1,
		}},				/* radio and probably mute is missing */
	},
	[SAA7134_BOARD_CRONOS_PLUS] = {
		/*
		gpio pins:
			0  .. 3   BASE_ID
			4  .. 7   PROTECT_ID
			8  .. 11  USER_OUT
			12 .. 13  USER_IN
			14 .. 15  VIDIN_SEL
		*/
		.name           = "Matrox CronosPlus",
		.tuner_type     = TUNER_ABSENT,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.gpiomask       = 0xcf00,
		.inputs         = {{
			.name = name_comp1,
			.vmux = 0,
			.gpio = 2 << 14,
		},{
			.name = name_comp2,
			.vmux = 0,
			.gpio = 1 << 14,
		},{
			.name = name_comp3,
			.vmux = 0,
			.gpio = 0 << 14,
		},{
			.name = name_comp4,
			.vmux = 0,
			.gpio = 3 << 14,
		},{
			.name = name_svideo,
			.vmux = 8,
			.gpio = 2 << 14,
		}},
	},
	[SAA7134_BOARD_MD2819] = {
		.name           = "AverMedia M156 / Medion 2819",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.gpiomask	= 0x03,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
			.gpio = 0x00,
		}, {
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
			.gpio = 0x02,
		}, {
			.name = name_comp2,
			.vmux = 0,
			.amux = LINE1,
			.gpio = 0x02,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
			.gpio = 0x02,
		} },
		.radio = {
			.name = name_radio,
			.amux = LINE1,
			.gpio = 0x01,
		},
		.mute  = {
			.name = name_mute,
			.amux = TV,
			.gpio = 0x00,
		},
	},
	[SAA7134_BOARD_BMK_MPEX_TUNER] = {
		/* "Greg Wickham <greg.wickham@grangenet.net> */
		.name           = "BMK MPEX Tuner",
		.audio_clock    = 0x200000,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.empress_addr 	= 0x20,
		.inputs         = {{
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
		}},
		.mpeg      = SAA7134_MPEG_EMPRESS,
		.video_out = CCIR656,
	},
	[SAA7134_BOARD_ASUSTEK_TVFM7133] = {
		.name           = "ASUS TV-FM 7133",
		.audio_clock    = 0x00187de7,
		/* probably wrong, the 7133 one is the NTSC version ...
		* .tuner_type  = TUNER_PHILIPS_FM1236_MK3 */
		.tuner_type     = TUNER_LG_NTSC_NEW_TAPC,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,

		},{
			.name = name_comp1,
			.vmux = 4,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 6,
			.amux = LINE2,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE1,
		},
	},
	[SAA7134_BOARD_PINNACLE_PCTV_STEREO] = {
		.name           = "Pinnacle PCTV Stereo (saa7134)",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_MT2032,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT | TDA9887_INTERCARRIER | TDA9887_PORT2_INACTIVE,
		.inputs         = {{
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE2,
		},{
			.name = name_comp2,
			.vmux = 1,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
		}},
	},
	[SAA7134_BOARD_MANLI_MTV002] = {
		/* Ognjen Nastic <ognjen@logosoft.ba> */
		.name           = "Manli MuchTV M-TV002",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name   = name_comp1,
			.vmux   = 1,
			.amux   = LINE1,
		},{
			.name = name_tv,
			.vmux = 3,
			.amux = LINE2,
			.tv   = 1,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_MANLI_MTV001] = {
		/* Ognjen Nastic <ognjen@logosoft.ba> UNTESTED */
		.name           = "Manli MuchTV M-TV001",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		},{
			.name = name_tv,
			.vmux = 3,
			.amux = LINE2,
			.tv   = 1,
		}},
		.mute = {
			.name = name_mute,
			.amux = LINE1,
		},
	},
	[SAA7134_BOARD_TG3000TV] = {
		/* TransGear 3000TV */
		.name           = "Nagase Sangyo TransGear 3000TV",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_NTSC_M,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = LINE2,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
		}},
	},
	[SAA7134_BOARD_ECS_TVP3XP] = {
		.name           = "Elitegroup ECS TVP3XP FM1216 Tuner Card(PAL-BG,FM) ",
		.audio_clock    = 0x187de7,  /* xtal 32.1 MHz */
		.tuner_type     = TUNER_PHILIPS_PAL,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
		},{
			.name   = name_tv_mono,
			.vmux   = 1,
			.amux   = LINE2,
			.tv     = 1,
		},{
			.name   = name_comp1,
			.vmux   = 3,
			.amux   = LINE1,
		},{
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE1,
		},{
			.name   = "CVid over SVid",
			.vmux   = 0,
			.amux   = LINE1,
		}},
		.radio = {
			.name   = name_radio,
			.amux   = LINE2,
		},
	},
	[SAA7134_BOARD_ECS_TVP3XP_4CB5] = {
		.name           = "Elitegroup ECS TVP3XP FM1236 Tuner Card (NTSC,FM)",
		.audio_clock    = 0x187de7,
		.tuner_type     = TUNER_PHILIPS_NTSC,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
		},{
			.name   = name_tv_mono,
			.vmux   = 1,
			.amux   = LINE2,
			.tv     = 1,
		},{
			.name   = name_comp1,
			.vmux   = 3,
			.amux   = LINE1,
		},{
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE1,
		},{
			.name   = "CVid over SVid",
			.vmux   = 0,
			.amux   = LINE1,
		}},
		.radio = {
			.name   = name_radio,
			.amux   = LINE2,
		},
	},
    [SAA7134_BOARD_ECS_TVP3XP_4CB6] = {
		/* Barry Scott <barry.scott@onelan.co.uk> */
		.name		= "Elitegroup ECS TVP3XP FM1246 Tuner Card (PAL,FM)",
		.audio_clock    = 0x187de7,
		.tuner_type     = TUNER_PHILIPS_PAL_I,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
		},{
			.name   = name_tv_mono,
			.vmux   = 1,
			.amux   = LINE2,
			.tv     = 1,
		},{
			.name   = name_comp1,
			.vmux   = 3,
			.amux   = LINE1,
		},{
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE1,
		},{
			.name   = "CVid over SVid",
			.vmux   = 0,
			.amux   = LINE1,
		}},
		.radio = {
			.name   = name_radio,
			.amux   = LINE2,
		},
	},
	[SAA7134_BOARD_AVACSSMARTTV] = {
		/* Roman Pszonczenko <romka@kolos.math.uni.lodz.pl> */
		.name           = "AVACS SmartTV",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_tv_mono,
			.vmux = 1,
			.amux = LINE2,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE2,
		},{
			.name = name_comp2,
			.vmux = 3,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
			.gpio = 0x200000,
		},
	},
	[SAA7134_BOARD_AVERMEDIA_DVD_EZMAKER] = {
		/* Michael Smith <msmith@cbnco.com> */
		.name           = "AVerMedia DVD EZMaker",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_ABSENT,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_comp1,
			.vmux = 3,
		},{
			.name = name_svideo,
			.vmux = 8,
		}},
	},
	[SAA7134_BOARD_AVERMEDIA_M103] = {
		/* Massimo Piccioni <dafastidio@libero.it> */
		.name           = "AVerMedia MiniPCI DVB-T Hybrid M103",
		.audio_clock    = 0x187de7,
		.tuner_type     = TUNER_XC2028,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		 .mpeg           = SAA7134_MPEG_DVB,
		 .inputs         = {{
			 .name = name_tv,
			 .vmux = 1,
			 .amux = TV,
			 .tv   = 1,
		 } },
	},
	[SAA7134_BOARD_NOVAC_PRIMETV7133] = {
		/* toshii@netbsd.org */
		.name           = "Noval Prime TV 7133",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_ALPS_TSBH1_NTSC,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_comp1,
			.vmux = 3,
		},{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_svideo,
			.vmux = 8,
		}},
	},
	[SAA7134_BOARD_AVERMEDIA_STUDIO_305] = {
		.name           = "AverMedia AverTV Studio 305",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1256_IH3,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = LINE2,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE2,
		},{
			.name = name_comp2,
			.vmux = 3,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
		.mute = {
			.name = name_mute,
			.amux = LINE1,
		},
	},
	[SAA7134_BOARD_AVERMEDIA_STUDIO_505] = {
		/* Vasiliy Temnikov <vaka@newmail.ru> */
		.name           = "AverMedia AverTV Studio 505",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = { {
			.name = name_tv,
			.vmux = 1,
			.amux = LINE2,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE2,
		}, {
			.name = name_comp2,
			.vmux = 3,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
		} },
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
		.mute = {
			.name = name_mute,
			.amux = LINE1,
		},
	},
	[SAA7134_BOARD_UPMOST_PURPLE_TV] = {
		.name           = "UPMOST PURPLE TV",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1236_MK3,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = name_tv,
			.vmux = 7,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_svideo,
			.vmux = 7,
			.amux = LINE1,
		}},
	},
	[SAA7134_BOARD_ITEMS_MTV005] = {
		/* Norman Jonas <normanjonas@arcor.de> */
		.name           = "Items MuchTV Plus / IT-005",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
		},{
			.name   = name_comp1,
			.vmux   = 1,
			.amux   = LINE1,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_CINERGY200] = {
		.name           = "Terratec Cinergy 200 TV",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = LINE2,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 4,
			.amux = LINE1,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name = name_comp2, /* CVideo over SVideo Connector */
			.vmux = 0,
			.amux = LINE1,
		}},
		.mute = {
			.name = name_mute,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_VIDEOMATE_TV_PVR] = {
		/* Alain St-Denis <alain@topaze.homeip.net> */
		.name           = "Compro VideoMate TV PVR/FM",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_NTSC_M,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.gpiomask	= 0x808c0080,
		.inputs         = {{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
			.gpio = 0x00080,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
			.gpio = 0x00080,
		},{
			.name = name_tv,
			.vmux = 1,
			.amux = LINE2_LEFT,
			.tv   = 1,
			.gpio = 0x00080,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
			.gpio = 0x80000,
		},
		.mute = {
			.name = name_mute,
			.amux = LINE2,
			.gpio = 0x40000,
		},
	},
	[SAA7134_BOARD_SABRENT_SBTTVFM] = {
		/* Michael Rodriguez-Torrent <mrtorrent@asu.edu> */
		.name           = "Sabrent SBT-TVFM (saa7130)",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_NTSC_M,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		},{
			.name = name_tv,
			.vmux = 3,
			.amux = LINE2,
			.tv   = 1,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		}},
		.radio = {
			.name   = name_radio,
			.amux   = LINE2,
		},
	},
	[SAA7134_BOARD_ZOLID_XPERT_TV7134] = {
		/* Helge Jensen <helge.jensen@slog.dk> */
		.name           = ":Zolid Xpert TV7134",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_NTSC,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
		},{
			.name = name_tv,
			.vmux = 1,
			.amux = LINE2,
			.tv   = 1,
		}},
	},
	[SAA7134_BOARD_EMPIRE_PCI_TV_RADIO_LE] = {
		/* "Matteo Az" <matte.az@nospam.libero.it> ;-) */
		.name           = "Empire PCI TV-Radio LE",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.gpiomask       = 0x4000,
		.inputs         = {{
			.name = name_tv_mono,
			.vmux = 1,
			.amux = LINE2,
			.gpio = 0x8000,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
			.gpio = 0x8000,
		},{
			.name = name_svideo,
			.vmux = 6,
			.amux = LINE1,
			.gpio = 0x8000,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE1,
			.gpio = 0x8000,
		},
		.mute = {
			.name = name_mute,
			.amux = TV,
			.gpio =0x8000,
		}
	},
	[SAA7134_BOARD_AVERMEDIA_STUDIO_307] = {
		/*
		Nickolay V. Shmyrev <nshmyrev@yandex.ru>
		Lots of thanks to Andrey Zolotarev <zolotarev_andrey@mail.ru>
		*/
		.name           = "Avermedia AVerTV Studio 307",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1256_IH3,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.gpiomask       = 0x03,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
			.gpio = 0x00,
		},{
			.name = name_comp,
			.vmux = 3,
			.amux = LINE1,
			.gpio = 0x02,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
			.gpio = 0x02,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE1,
			.gpio = 0x01,
		},
		.mute  = {
			.name = name_mute,
			.amux = LINE1,
			.gpio = 0x00,
		},
	},
	[SAA7134_BOARD_AVERMEDIA_GO_007_FM] = {
		.name           = "Avermedia AVerTV GO 007 FM",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.gpiomask       = 0x00300003,
		/* .gpiomask       = 0x8c240003, */
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
			.gpio = 0x01,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE1,
			.gpio = 0x02,
		},{
			.name = name_svideo,
			.vmux = 6,
			.amux = LINE1,
			.gpio = 0x02,
		}},
		.radio = {
			.name = name_radio,
			.amux = TV,
			.gpio = 0x00300001,
		},
		.mute = {
			.name = name_mute,
			.amux = TV,
			.gpio = 0x01,
		},
	},
	[SAA7134_BOARD_AVERMEDIA_CARDBUS] = {
		/* Kees.Blom@cwi.nl */
		.name           = "AVerMedia Cardbus TV/Radio (E500)",
		.audio_clock    = 0x187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE1,
		},
	},
	[SAA7134_BOARD_AVERMEDIA_CARDBUS_501] = {
		/* Oldrich Jedlicka <oldium.pro@seznam.cz> */
		.name           = "AVerMedia Cardbus TV/Radio (E501R)",
		.audio_clock    = 0x187de7,
		.tuner_type     = TUNER_ALPS_TSBE5_PAL,
		.radio_type     = TUNER_TEA5767,
		.tuner_addr	= 0x61,
		.radio_addr	= 0x60,
		.tda9887_conf   = TDA9887_PRESENT,
		.gpiomask       = 0x08000000,
		.inputs         = { {
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
			.gpio = 0x08000000,
		}, {
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
			.gpio = 0x08000000,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
			.gpio = 0x08000000,
		} },
		.radio = {
			.name = name_radio,
			.amux = LINE2,
			.gpio = 0x00000000,
		},
	},
	[SAA7134_BOARD_CINERGY400_CARDBUS] = {
		.name           = "Terratec Cinergy 400 mobile",
		.audio_clock    = 0x187de7,
		.tuner_type     = TUNER_ALPS_TSBE5_PAL,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_tv_mono,
			.vmux = 1,
			.amux = LINE2,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		}},
	},
	[SAA7134_BOARD_CINERGY600_MK3] = {
		.name           = "Terratec Cinergy 600 TV MK3",
		.audio_clock    = 0x00200000,
		.tuner_type	= TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.rds_addr 	= 0x10,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 4,
			.amux = LINE1,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name = name_comp2, /* CVideo over SVideo Connector */
			.vmux = 0,
			.amux = LINE1,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_VIDEOMATE_GOLD_PLUS] = {
		/* Dylan Walkden <dylan_walkden@hotmail.com> */
		.name		= "Compro VideoMate Gold+ Pal",
		.audio_clock	= 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.gpiomask	= 0x1ce780,
		.inputs		= {{
			.name = name_svideo,
			.vmux = 0,		/* CVideo over SVideo Connector - ok? */
			.amux = LINE1,
			.gpio = 0x008080,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
			.gpio = 0x008080,
		},{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
			.gpio = 0x008080,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
			.gpio = 0x80000,
		},
		.mute = {
			.name = name_mute,
			.amux = LINE2,
			.gpio = 0x0c8000,
		},
	},
	[SAA7134_BOARD_PINNACLE_300I_DVBT_PAL] = {
		.name           = "Pinnacle PCTV 300i DVB-T + PAL",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_MT2032,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT | TDA9887_INTERCARRIER | TDA9887_PORT2_INACTIVE,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs         = {{
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE2,
		},{
			.name = name_comp2,
			.vmux = 1,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
		}},
	},
	[SAA7134_BOARD_PROVIDEO_PV952] = {
		/* andreas.kretschmer@web.de */
		.name		= "ProVideo PV952",
		.audio_clock	= 0x00187de7,
		.tuner_type	= TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE1,
		},{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_tv_mono,
			.vmux = 1,
			.amux = LINE2,
			.tv   = 1,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_AVERMEDIA_305] = {
		/* much like the "studio" version but without radio
		* and another tuner (sirspiritus@yandex.ru) */
		.name           = "AverMedia AverTV/305",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FQ1216ME,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = LINE2,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE2,
		},{
			.name = name_comp2,
			.vmux = 3,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
		}},
		.mute = {
			.name = name_mute,
			.amux = LINE1,
		},
	},
	[SAA7134_BOARD_FLYDVBTDUO] = {
		/* LifeView FlyDVB-T DUO */
		/* "Nico Sabbi <nsabbi@tiscali.it>  Hartmut Hackmann hartmut.hackmann@t-online.de*/
		.name           = "LifeView FlyDVB-T DUO / MSI TV@nywhere Duo",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.gpiomask	= 0x00200000,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.gpio = 0x200000,	/* GPIO21=High for TV input */
			.tv   = 1,
		},{
			.name = name_comp1,	/* Composite signal on S-Video input */
			.vmux = 0,
			.amux = LINE2,
		},{
			.name = name_comp2,	/* Composite input */
			.vmux = 3,
			.amux = LINE2,
		},{
			.name = name_svideo,	/* S-Video signal on S-Video input */
			.vmux = 8,
			.amux = LINE2,
		}},
		.radio = {
			.name = name_radio,
			.amux = TV,
			.gpio = 0x000000,	/* GPIO21=Low for FM radio antenna */
		},
	},
	[SAA7134_BOARD_PHILIPS_TOUGH] = {
		.name           = "Philips TOUGH DVB-T reference design",
		.tuner_type	= TUNER_ABSENT,
		.audio_clock    = 0x00187de7,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs = {{
			.name   = name_comp1,
			.vmux   = 0,
			.amux   = LINE1,
		},{
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE1,
		}},
	},
	[SAA7134_BOARD_AVERMEDIA_307] = {
		/*
		Davydov Vladimir <vladimir@iqmedia.com>
		*/
		.name           = "Avermedia AVerTV 307",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FQ1216ME,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE1,
		},{
			.name = name_comp2,
			.vmux = 3,
			.amux = LINE1,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		}},
	},
	[SAA7134_BOARD_ADS_INSTANT_TV] = {
		.name           = "ADS Tech Instant TV (saa7135)",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
		}},
	},
	[SAA7134_BOARD_KWORLD_VSTREAM_XPERT] = {
		.name           = "Kworld/Tevion V-Stream Xpert TV PVR7134",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_PAL_I,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.gpiomask	= 0x0700,
		.inputs = {{
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
			.gpio   = 0x000,
		},{
			.name   = name_comp1,
			.vmux   = 3,
			.amux   = LINE1,
			.gpio   = 0x200,		/* gpio by DScaler */
		},{
			.name   = name_svideo,
			.vmux   = 0,
			.amux   = LINE1,
			.gpio   = 0x200,
		}},
		.radio = {
			.name   = name_radio,
			.amux   = LINE1,
			.gpio   = 0x100,
		},
		.mute  = {
			.name = name_mute,
			.amux = TV,
			.gpio = 0x000,
		},
	},
	[SAA7134_BOARD_FLYDVBT_DUO_CARDBUS] = {
		.name		= "LifeView/Typhoon/Genius FlyDVB-T Duo Cardbus",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.mpeg           = SAA7134_MPEG_DVB,
		.gpiomask	= 0x00200000,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.gpio = 0x200000,	/* GPIO21=High for TV input */
			.tv   = 1,
		},{
			.name = name_svideo,	/* S-Video signal on S-Video input */
			.vmux = 8,
			.amux = LINE2,
		},{
			.name = name_comp1,	/* Composite signal on S-Video input */
			.vmux = 0,
			.amux = LINE2,
		},{
			.name = name_comp2,	/* Composite input */
			.vmux = 3,
			.amux = LINE2,
		}},
		.radio = {
			.name = name_radio,
			.amux = TV,
			.gpio = 0x000000,	/* GPIO21=Low for FM radio antenna */
		},
	},
	[SAA7134_BOARD_VIDEOMATE_TV_GOLD_PLUSII] = {
		.name           = "Compro VideoMate TV Gold+II",
		.audio_clock    = 0x002187de7,
		.tuner_type     = TUNER_LG_PAL_NEW_TAPC,
		.radio_type     = TUNER_TEA5767,
		.tuner_addr     = 0x63,
		.radio_addr     = 0x60,
		.gpiomask       = 0x8c1880,
		.inputs         = {{
			.name = name_svideo,
			.vmux = 0,
			.amux = LINE1,
			.gpio = 0x800800,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
			.gpio = 0x801000,
		},{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
			.gpio = 0x800000,
		}},
		.radio = {
			.name = name_radio,
			.amux = TV,
			.gpio = 0x880000,
		},
		.mute = {
			.name = name_mute,
			.amux = LINE2,
			.gpio = 0x840000,
		},
	},
	[SAA7134_BOARD_KWORLD_XPERT] = {
		/*
		FIXME:
		- Remote control doesn't initialize properly.
		- Audio volume starts muted,
		then gradually increases after channel change.
		- Overlay scaling problems (application error?)
		- Composite S-Video untested.
		From: Konrad Rzepecki <hannibal@megapolis.pl>
		*/
		.name           = "Kworld Xpert TV PVR7134",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_TENA_9533_DI,
		.radio_type     = TUNER_TEA5767,
		.tuner_addr	= 0x61,
		.radio_addr	= 0x60,
		.gpiomask	= 0x0700,
		.inputs = {{
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
			.gpio   = 0x000,
		},{
			.name   = name_comp1,
			.vmux   = 3,
			.amux   = LINE1,
			.gpio   = 0x200,		/* gpio by DScaler */
		},{
			.name   = name_svideo,
			.vmux   = 0,
			.amux   = LINE1,
			.gpio   = 0x200,
		}},
		.radio = {
			.name   = name_radio,
			.amux   = LINE1,
			.gpio   = 0x100,
		},
		.mute = {
			.name = name_mute,
			.amux = TV,
			.gpio = 0x000,
		},
	},
	[SAA7134_BOARD_FLYTV_DIGIMATRIX] = {
		.name		= "FlyTV mini Asus Digimatrix",
		.audio_clock	= 0x00200000,
		.tuner_type	= TUNER_LG_TALN,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_tv_mono,
			.vmux = 1,
			.amux = LINE2,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE2,
		},{
			.name = name_comp2,
			.vmux = 3,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
		}},
		.radio = {
			.name = name_radio,		/* radio unconfirmed */
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_KWORLD_TERMINATOR] = {
		/* Kworld V-Stream Studio TV Terminator */
		/* "James Webb <jrwebb@qwest.net> */
		.name           = "V-Stream Studio TV Terminator",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.gpiomask       = 1 << 21,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.gpio = 0x0000000,
			.tv   = 1,
		},{
			.name = name_comp1,     /* Composite input */
			.vmux = 3,
			.amux = LINE2,
			.gpio = 0x0000000,
		},{
			.name = name_svideo,    /* S-Video input */
			.vmux = 8,
			.amux = LINE2,
			.gpio = 0x0000000,
		}},
		.radio = {
			.name = name_radio,
			.amux = TV,
			.gpio = 0x0200000,
		},
	},
	[SAA7134_BOARD_YUAN_TUN900] = {
		/* FIXME:
		 * S-Video and composite sources untested.
		 * Radio not working.
		 * Remote control not yet implemented.
		 * From : codemaster@webgeeks.be */
		.name           = "Yuan TUN-900 (saa7135)",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr= ADDR_UNSET,
		.radio_addr= ADDR_UNSET,
		.gpiomask       = 0x00010003,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
			.gpio = 0x01,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE2,
			.gpio = 0x02,
		},{
			.name = name_svideo,
			.vmux = 6,
			.amux = LINE2,
			.gpio = 0x02,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE1,
			.gpio = 0x00010003,
		},
		.mute = {
			.name = name_mute,
			.amux = TV,
			.gpio = 0x01,
		},
	},
	[SAA7134_BOARD_BEHOLD_409FM] = {
		/* <http://tuner.beholder.ru>, Sergey <skiv@orel.ru> */
		/*       Beholder Intl. Ltd. 2008      */
		/*Dmitry Belimov <d.belimov@gmail.com> */
		.name           = "Beholder BeholdTV 409 FM",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.gpiomask       = 0x00008000,
		.inputs         = {{
			  .name = name_tv,
			  .vmux = 3,
			  .amux = TV,
			  .tv   = 1,
		},{
			  .name = name_comp1,
			  .vmux = 1,
			  .amux = LINE1,
		},{
			  .name = name_svideo,
			  .vmux = 8,
			  .amux = LINE1,
		}},
		.radio = {
			  .name = name_radio,
			  .amux = LINE2,
		},
	},
	[SAA7134_BOARD_GOTVIEW_7135] = {
		/* Mike Baikov <mike@baikov.com> */
		/* Andrey Cvetcov <ays14@yandex.ru> */
		.name            = "GoTView 7135 PCI",
		.audio_clock     = 0x00187de7,
		.tuner_type      = TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type      = UNSET,
		.tuner_addr      = ADDR_UNSET,
		.radio_addr      = ADDR_UNSET,
		.tda9887_conf    = TDA9887_PRESENT,
		.gpiomask        = 0x00200003,
		.inputs          = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
			.gpio = 0x00200003,
		},{
			.name = name_tv_mono,
			.vmux = 1,
			.amux = LINE2,
			.gpio = 0x00200003,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
			.gpio = 0x00200003,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
			.gpio = 0x00200003,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
			.gpio = 0x00200003,
		},
		.mute = {
			.name = name_mute,
			.amux = TV,
			.gpio = 0x00200003,
		},
	},
	[SAA7134_BOARD_PHILIPS_EUROPA] = {
		.name           = "Philips EUROPA V3 reference design",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TD1316,
		.radio_type     = UNSET,
		.tuner_addr	= 0x61,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT | TDA9887_PORT1_ACTIVE,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs = {{
			.name   = name_tv,
			.vmux   = 3,
			.amux   = TV,
			.tv     = 1,
		},{
			.name   = name_comp1,
			.vmux   = 0,
			.amux   = LINE2,
		},{
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE2,
		}},
	},
	[SAA7134_BOARD_VIDEOMATE_DVBT_300] = {
		.name           = "Compro Videomate DVB-T300",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TD1316,
		.radio_type     = UNSET,
		.tuner_addr	= 0x61,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT | TDA9887_PORT1_ACTIVE,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs = {{
			.name   = name_tv,
			.vmux   = 3,
			.amux   = TV,
			.tv     = 1,
		},{
			.name   = name_comp1,
			.vmux   = 1,
			.amux   = LINE2,
		},{
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE2,
		}},
	},
	[SAA7134_BOARD_VIDEOMATE_DVBT_200] = {
		.name           = "Compro Videomate DVB-T200",
		.tuner_type	= TUNER_ABSENT,
		.audio_clock    = 0x00187de7,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs = {{
			.name   = name_comp1,
			.vmux   = 0,
			.amux   = LINE1,
		},{
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE1,
		}},
	},
	[SAA7134_BOARD_RTD_VFG7350] = {
		.name		= "RTD Embedded Technologies VFG7350",
		.audio_clock	= 0x00200000,
		.tuner_type	= TUNER_ABSENT,
		.radio_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.empress_addr 	= 0x21,
		.inputs		= {{
			.name   = "Composite 0",
			.vmux   = 0,
			.amux   = LINE1,
		},{
			.name   = "Composite 1",
			.vmux   = 1,
			.amux   = LINE2,
		},{
			.name   = "Composite 2",
			.vmux   = 2,
			.amux   = LINE1,
		},{
			.name   = "Composite 3",
			.vmux   = 3,
			.amux   = LINE2,
		},{
			.name   = "S-Video 0",
			.vmux   = 8,
			.amux   = LINE1,
		},{
			.name   = "S-Video 1",
			.vmux   = 9,
			.amux   = LINE2,
		}},
		.mpeg           = SAA7134_MPEG_EMPRESS,
		.video_out      = CCIR656,
		.vid_port_opts  = ( SET_T_CODE_POLARITY_NON_INVERTED |
				    SET_CLOCK_NOT_DELAYED |
				    SET_CLOCK_INVERTED |
				    SET_VSYNC_OFF ),
	},
	[SAA7134_BOARD_RTD_VFG7330] = {
		.name		= "RTD Embedded Technologies VFG7330",
		.audio_clock	= 0x00200000,
		.tuner_type	= TUNER_ABSENT,
		.radio_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs		= {{
			.name   = "Composite 0",
			.vmux   = 0,
			.amux   = LINE1,
		},{
			.name   = "Composite 1",
			.vmux   = 1,
			.amux   = LINE2,
		},{
			.name   = "Composite 2",
			.vmux   = 2,
			.amux   = LINE1,
		},{
			.name   = "Composite 3",
			.vmux   = 3,
			.amux   = LINE2,
		},{
			.name   = "S-Video 0",
			.vmux   = 8,
			.amux   = LINE1,
		},{
			.name   = "S-Video 1",
			.vmux   = 9,
			.amux   = LINE2,
		}},
	},
	[SAA7134_BOARD_FLYTVPLATINUM_MINI2] = {
		.name           = "LifeView FlyTV Platinum Mini2",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,

		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,     /* Composite signal on S-Video input */
			.vmux = 0,
			.amux = LINE2,
		},{
			.name = name_comp2,	/* Composite input */
			.vmux = 3,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
		}},
	},
	[SAA7134_BOARD_AVERMEDIA_AVERTVHD_A180] = {
		/* Michael Krufky <mkrufky@m1k.net>
		 * Uses Alps Electric TDHU2, containing NXT2004 ATSC Decoder
		 * AFAIK, there is no analog demod, thus,
		 * no support for analog television.
		 */
		.name           = "AVerMedia AVerTVHD MCE A180",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_ABSENT,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs         = {{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
		}},
	},
	[SAA7134_BOARD_MONSTERTV_MOBILE] = {
		.name           = "SKNet MonsterTV Mobile",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,

		.inputs         = {{
			  .name = name_tv,
			  .vmux = 1,
			  .amux = TV,
			  .tv   = 1,
		},{
			  .name = name_comp1,
			  .vmux = 3,
			  .amux = LINE1,
		},{
			  .name = name_svideo,
			  .vmux = 6,
			  .amux = LINE1,
		}},
	},
	[SAA7134_BOARD_PINNACLE_PCTV_110i] = {
	       .name           = "Pinnacle PCTV 40i/50i/110i (saa7133)",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.gpiomask       = 0x080200000,
		.inputs         = { {
			.name = name_tv,
			.vmux = 4,
			.amux = TV,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE2,
		}, {
			.name = name_comp2,
			.vmux = 0,
			.amux = LINE2,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
		} },
		.radio = {
			.name = name_radio,
			.amux = TV,
			.gpio = 0x0200000,
		},
	},
	[SAA7134_BOARD_ASUSTeK_P7131_DUAL] = {
		.name           = "ASUSTeK P7131 Dual",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.gpiomask	= 1 << 21,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
			.gpio = 0x0000000,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE2,
			.gpio = 0x0200000,
		},{
			.name = name_comp2,
			.vmux = 0,
			.amux = LINE2,
			.gpio = 0x0200000,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
			.gpio = 0x0200000,
		}},
		.radio = {
			.name = name_radio,
			.amux = TV,
			.gpio = 0x0200000,
		},
	},
	[SAA7134_BOARD_SEDNA_PC_TV_CARDBUS] = {
		/* Paul Tom Zalac <pzalac@gmail.com> */
		/* Pavel Mihaylov <bin@bash.info> */
		.name           = "Sedna/MuchTV PC TV Cardbus TV/Radio (ITO25 Rev:2B)",
				/* Sedna/MuchTV (OEM) Cardbus TV Tuner */
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.gpiomask       = 0xe880c0,
		.inputs         = {{
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		},{
			.name = name_svideo,
			.vmux = 6,
			.amux = LINE1,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_ASUSTEK_DIGIMATRIX_TV] = {
		/* "Cyril Lacoux (Yack)" <clacoux@ifeelgood.org> */
		.name           = "ASUS Digimatrix TV",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_PHILIPS_FQ1216ME,
		.tda9887_conf   = TDA9887_PRESENT,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		}},
	},
	[SAA7134_BOARD_PHILIPS_TIGER] = {
		.name           = "Philips Tiger reference design",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tuner_config   = 0,
		.mpeg           = SAA7134_MPEG_DVB,
		.gpiomask       = 0x0200000,
		.inputs = {{
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
		},{
			.name   = name_comp1,
			.vmux   = 3,
			.amux   = LINE1,
		},{
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE1,
		}},
		.radio = {
			.name   = name_radio,
			.amux   = TV,
			.gpio   = 0x0200000,
		},
	},
	[SAA7134_BOARD_MSI_TVATANYWHERE_PLUS] = {
		.name           = "MSI TV@Anywhere plus",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.gpiomask       = 1 << 21,
		.inputs = {{
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
		},{
			.name   = name_comp1,
			.vmux   = 3,
			.amux   = LINE2,	/* unconfirmed, taken from Philips driver */
		},{
			.name   = name_comp2,
			.vmux   = 0,		/* untested, Composite over S-Video */
			.amux   = LINE2,
		},{
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE2,
		}},
		.radio = {
			.name   = name_radio,
			.amux   = TV,
			.gpio   = 0x0200000,
		},
	},
	[SAA7134_BOARD_CINERGY250PCI] = {
		/* remote-control does not work. The signal about a
		   key press comes in via gpio, but the key code
		   doesn't. Neither does it have an i2c remote control
		   interface. */
		.name           = "Terratec Cinergy 250 PCI TV",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.gpiomask       = 0x80200000,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_svideo,  /* NOT tested */
			.vmux = 8,
			.amux = LINE1,
		}},
		.radio = {
			.name   = name_radio,
			.amux   = TV,
			.gpio   = 0x0200000,
		},
	},
	[SAA7134_BOARD_FLYDVB_TRIO] = {
		/* LifeView LR319 FlyDVB Trio */
		/* Peter Missel <peter.missel@onlinehome.de> */
		.name           = "LifeView FlyDVB Trio",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.gpiomask	= 0x00200000,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs         = {{
			.name = name_tv,	/* Analog broadcast/cable TV */
			.vmux = 1,
			.amux = TV,
			.gpio = 0x200000,	/* GPIO21=High for TV input */
			.tv   = 1,
		},{
			.name = name_svideo,	/* S-Video signal on S-Video input */
			.vmux = 8,
			.amux = LINE2,
		},{
			.name = name_comp1,	/* Composite signal on S-Video input */
			.vmux = 0,
			.amux = LINE2,
		},{
			.name = name_comp2,	/* Composite input */
			.vmux = 3,
			.amux = LINE2,
		}},
		.radio = {
			.name = name_radio,
			.amux = TV,
			.gpio = 0x000000,	/* GPIO21=Low for FM radio antenna */
		},
	},
	[SAA7134_BOARD_AVERMEDIA_777] = {
		.name           = "AverTV DVB-T 777",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_ABSENT,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs = {{
			.name   = name_comp1,
			.vmux   = 1,
			.amux   = LINE1,
		},{
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE1,
		}},
	},
	[SAA7134_BOARD_FLYDVBT_LR301] = {
		/* LifeView FlyDVB-T */
		/* Giampiero Giancipoli <gianci@libero.it> */
		.name           = "LifeView FlyDVB-T / Genius VideoWonder DVB-T",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_ABSENT,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs         = {{
			.name = name_comp1,	/* Composite input */
			.vmux = 3,
			.amux = LINE2,
		},{
			.name = name_svideo,	/* S-Video signal on S-Video input */
			.vmux = 8,
			.amux = LINE2,
		}},
	},
	[SAA7134_BOARD_ADS_DUO_CARDBUS_PTV331] = {
		.name           = "ADS Instant TV Duo Cardbus PTV331",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.mpeg           = SAA7134_MPEG_DVB,
		.gpiomask       = 0x00600000, /* Bit 21 0=Radio, Bit 22 0=TV */
		.inputs = {{
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
			.gpio   = 0x00200000,
		}},
	},
	[SAA7134_BOARD_TEVION_DVBT_220RF] = {
		.name           = "Tevion/KWorld DVB-T 220RF",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.mpeg           = SAA7134_MPEG_DVB,
		.gpiomask       = 1 << 21,
		.inputs = {{
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
		},{
			.name   = name_comp1,
			.vmux   = 3,
			.amux   = LINE1,
		},{
			.name   = name_comp2,
			.vmux   = 0,
			.amux   = LINE1,
		},{
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE1,
		}},
		.radio = {
			.name   = name_radio,
			.amux   = TV,
			.gpio   = 0x0200000,
		},
	},
	[SAA7134_BOARD_KWORLD_DVBT_210] = {
		.name           = "KWorld DVB-T 210",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.mpeg           = SAA7134_MPEG_DVB,
		.gpiomask       = 1 << 21,
		.inputs = {{
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
		},{
			.name   = name_comp1,
			.vmux   = 3,
			.amux   = LINE1,
		},{
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE1,
		}},
		.radio = {
			.name   = name_radio,
			.amux   = TV,
			.gpio   = 0x0200000,
		},
	},
	[SAA7134_BOARD_KWORLD_ATSC110] = {
		.name           = "Kworld ATSC110/115",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TUV1236D,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
		}},
	},
	[SAA7134_BOARD_AVERMEDIA_A169_B] = {
		/* AVerMedia A169  */
		/* Rickard Osser <ricky@osser.se>  */
		/* This card has two saa7134 chips on it,
		   but only one of them is currently working. */
		.name		= "AVerMedia A169 B",
		.audio_clock    = 0x02187de7,
		.tuner_type	= TUNER_LG_TALN,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.gpiomask       = 0x0a60000,
	},
	[SAA7134_BOARD_AVERMEDIA_A169_B1] = {
		/* AVerMedia A169 */
		/* Rickard Osser <ricky@osser.se> */
		.name		= "AVerMedia A169 B1",
		.audio_clock    = 0x02187de7,
		.tuner_type	= TUNER_LG_TALN,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.gpiomask       = 0xca60000,
		.inputs         = {{
			.name = name_tv,
			.vmux = 4,
			.amux = TV,
			.tv   = 1,
			.gpio = 0x04a61000,
		},{
			.name = name_comp2,  /*  Composite SVIDEO (B/W if signal is carried with SVIDEO) */
			.vmux = 1,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 9,           /* 9 is correct as S-VIDEO1 according to a169.inf! */
			.amux = LINE1,
		}},
	},
	[SAA7134_BOARD_MD7134_BRIDGE_2] = {
		/* The second saa7134 on this card only serves as DVB-S host bridge */
		.name           = "Medion 7134 Bridge #2",
		.audio_clock    = 0x00187de7,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.mpeg           = SAA7134_MPEG_DVB,
	},
	[SAA7134_BOARD_FLYDVBT_HYBRID_CARDBUS] = {
		.name		= "LifeView FlyDVB-T Hybrid Cardbus/MSI TV @nywhere A/D NB",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.mpeg           = SAA7134_MPEG_DVB,
		.gpiomask       = 0x00600000, /* Bit 21 0=Radio, Bit 22 0=TV */
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.gpio = 0x200000,	/* GPIO21=High for TV input */
			.tv   = 1,
		},{
			.name = name_svideo,	/* S-Video signal on S-Video input */
			.vmux = 8,
			.amux = LINE2,
		},{
			.name = name_comp1,	/* Composite signal on S-Video input */
			.vmux = 0,
			.amux = LINE2,
		},{
			.name = name_comp2,	/* Composite input */
			.vmux = 3,
			.amux = LINE2,
		}},
		.radio = {
			.name = name_radio,
			.amux = TV,
			.gpio = 0x000000,	/* GPIO21=Low for FM radio antenna */
		},
	},
	[SAA7134_BOARD_FLYVIDEO3000_NTSC] = {
		/* "Zac Bowling" <zac@zacbowling.com> */
		.name           = "LifeView FlyVIDEO3000 (NTSC)",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_PHILIPS_NTSC,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,

		.gpiomask       = 0xe000,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.gpio = 0x8000,
			.tv   = 1,
		},{
			.name = name_tv_mono,
			.vmux = 1,
			.amux = LINE2,
			.gpio = 0x0000,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE2,
			.gpio = 0x4000,
		},{
			.name = name_comp2,
			.vmux = 3,
			.amux = LINE2,
			.gpio = 0x4000,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
			.gpio = 0x4000,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
			.gpio = 0x2000,
		},
			.mute = {
			.name = name_mute,
			.amux = TV,
			.gpio = 0x8000,
		},
	},
	[SAA7134_BOARD_MEDION_MD8800_QUADRO] = {
		.name           = "Medion Md8800 Quadro",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs = {{
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
		},{
			.name   = name_comp1,
			.vmux   = 0,
			.amux   = LINE1,
		},{
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE1,
		}},
	},
	[SAA7134_BOARD_FLYDVBS_LR300] = {
		/* LifeView FlyDVB-s */
		/* Igor M. Liplianin <liplianin@tut.by> */
		.name           = "LifeView FlyDVB-S /Acorp TV134DS",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_ABSENT,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs         = {{
			.name = name_comp1,	/* Composite input */
			.vmux = 3,
			.amux = LINE1,
		},{
			.name = name_svideo,	/* S-Video signal on S-Video input */
			.vmux = 8,
			.amux = LINE1,
		}},
	},
	[SAA7134_BOARD_PROTEUS_2309] = {
		.name           = "Proteus Pro 2309",
		.audio_clock    = 0x00187de7,
		.tuner_type	= TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = LINE2,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE2,
		},{
			.name = name_comp2,
			.vmux = 3,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
		}},
		.mute = {
			.name = name_mute,
			.amux = LINE1,
		},
	},
	[SAA7134_BOARD_AVERMEDIA_A16AR] = {
		/* Petr Baudis <pasky@ucw.cz> */
		.name           = "AVerMedia TV Hybrid A16AR",
		.audio_clock    = 0x187de7,
		.tuner_type     = TUNER_PHILIPS_TD1316, /* untested */
		.radio_type     = TUNER_TEA5767, /* untested */
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = 0x60,
		.tda9887_conf   = TDA9887_PRESENT,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE1,
		},
	},
	[SAA7134_BOARD_ASUS_EUROPA2_HYBRID] = {
		.name           = "Asus Europa2 OEM",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FMD1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT| TDA9887_PORT1_ACTIVE | TDA9887_PORT2_ACTIVE,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs = {{
			.name   = name_tv,
			.vmux   = 3,
			.amux   = TV,
			.tv     = 1,
		},{
			.name   = name_comp1,
			.vmux   = 4,
			.amux   = LINE2,
		},{
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE2,
		}},
		.radio = {
			.name   = name_radio,
			.amux   = LINE1,
		},
	},
	[SAA7134_BOARD_PINNACLE_PCTV_310i] = {
		.name           = "Pinnacle PCTV 310i",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.tuner_config   = 1,
		.mpeg           = SAA7134_MPEG_DVB,
		.gpiomask       = 0x000200000,
		.inputs         = {{
			.name = name_tv,
			.vmux = 4,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE2,
		},{
			.name = name_comp2,
			.vmux = 0,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
		}},
		.radio = {
			.name = name_radio,
			.amux   = TV,
			.gpio   = 0x0200000,
		},
	},
	[SAA7134_BOARD_AVERMEDIA_STUDIO_507] = {
		/* Mikhail Fedotov <mo_fedotov@mail.ru> */
		.name           = "Avermedia AVerTV Studio 507",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1256_IH3,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.gpiomask       = 0x03,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
			.gpio = 0x00,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE2,
			.gpio = 0x00,
		},{
			.name = name_comp2,
			.vmux = 3,
			.amux = LINE2,
			.gpio = 0x00,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
			.gpio = 0x00,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
			.gpio = 0x01,
		},
		.mute  = {
			.name = name_mute,
			.amux = LINE1,
			.gpio = 0x00,
		},
	},
	[SAA7134_BOARD_VIDEOMATE_DVBT_200A] = {
		/* Francis Barber <fedora@barber-family.id.au> */
		.name           = "Compro Videomate DVB-T200A",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_ABSENT,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT | TDA9887_PORT1_ACTIVE,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs = {{
			.name   = name_tv,
			.vmux   = 3,
			.amux   = TV,
			.tv     = 1,
		},{
			.name   = name_comp1,
			.vmux   = 1,
			.amux   = LINE2,
		},{
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE2,
		}},
	},
	[SAA7134_BOARD_HAUPPAUGE_HVR1110] = {
		/* Thomas Genty <tomlohave@gmail.com> */
		/* David Bentham <db260179@hotmail.com> */
		.name           = "Hauppauge WinTV-HVR1110 DVB-T/Hybrid",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.tuner_config   = 1,
		.mpeg           = SAA7134_MPEG_DVB,
		.gpiomask       = 0x0200100,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
			.gpio = 0x0000100,
		}, {
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		} },
		.radio = {
			.name = name_radio,
			.amux = TV,
			.gpio = 0x0200100,
		},
	},
	[SAA7134_BOARD_HAUPPAUGE_HVR1150] = {
		.name           = "Hauppauge WinTV-HVR1150 ATSC/QAM-Hybrid",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.tuner_config   = 3,
		.mpeg           = SAA7134_MPEG_DVB,
		.ts_type	= SAA7134_MPEG_TS_SERIAL,
		.ts_force_val   = 1,
		.gpiomask       = 0x0800100, /* GPIO 21 is an INPUT */
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
			.gpio = 0x0000100,
		}, {
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		} },
		.radio = {
			.name = name_radio,
			.amux = TV,
			.gpio = 0x0800100, /* GPIO 23 HI for FM */
		},
	},
	[SAA7134_BOARD_HAUPPAUGE_HVR1120] = {
		.name           = "Hauppauge WinTV-HVR1120 DVB-T/Hybrid",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.tuner_config   = 3,
		.mpeg           = SAA7134_MPEG_DVB,
		.ts_type	= SAA7134_MPEG_TS_SERIAL,
		.gpiomask       = 0x0800100, /* GPIO 21 is an INPUT */
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
			.gpio = 0x0000100,
		}, {
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		} },
		.radio = {
			.name = name_radio,
			.amux = TV,
			.gpio = 0x0800100, /* GPIO 23 HI for FM */
		},
	},
	[SAA7134_BOARD_CINERGY_HT_PCMCIA] = {
		.name           = "Terratec Cinergy HT PCMCIA",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs = {{
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
		},{
			.name   = name_comp1,
			.vmux   = 0,
			.amux   = LINE1,
		},{
			.name   = name_svideo,
			.vmux   = 6,
			.amux   = LINE1,
		}},
	},
	[SAA7134_BOARD_ENCORE_ENLTV] = {
	/* Steven Walter <stevenrwalter@gmail.com>
	   Juan Pablo Sormani <sorman@gmail.com> */
		.name           = "Encore ENLTV",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_TNF_5335MF,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = 3,
			.tv   = 1,
		},{
			.name = name_tv_mono,
			.vmux = 7,
			.amux = 4,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = 2,
		},{
			.name = name_svideo,
			.vmux = 0,
			.amux = 2,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
/*			.gpio = 0x00300001,*/
			.gpio = 0x20000,

		},
		.mute = {
			.name = name_mute,
			.amux = 0,
		},
	},
	[SAA7134_BOARD_ENCORE_ENLTV_FM] = {
  /*	Juan Pablo Sormani <sorman@gmail.com> */
		.name           = "Encore ENLTV-FM",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_PHILIPS_FCV1236D,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = 3,
			.tv   = 1,
		},{
			.name = name_tv_mono,
			.vmux = 7,
			.amux = 4,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = 2,
		},{
			.name = name_svideo,
			.vmux = 0,
			.amux = 2,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
			.gpio = 0x20000,

		},
		.mute = {
			.name = name_mute,
			.amux = 0,
		},
	},
	[SAA7134_BOARD_ENCORE_ENLTV_FM53] = {
		.name           = "Encore ENLTV-FM v5.3",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_TNF_5335MF,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.gpiomask	= 0x7000,
		.inputs         = { {
			.name = name_tv,
			.vmux = 1,
			.amux = 1,
			.tv   = 1,
			.gpio = 0x50000,
		}, {
			.name = name_comp1,
			.vmux = 3,
			.amux = 2,
			.gpio = 0x2000,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = 2,
			.gpio = 0x2000,
		} },
		.radio = {
			.name = name_radio,
			.vmux = 1,
			.amux = 1,
		},
		.mute = {
			.name = name_mute,
			.gpio = 0xf000,
			.amux = 0,
		},
	},
	[SAA7134_BOARD_CINERGY_HT_PCI] = {
		.name           = "Terratec Cinergy HT PCI",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs = {{
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
		},{
			.name   = name_comp1,
			.vmux   = 0,
			.amux   = LINE1,
		},{
			.name   = name_svideo,
			.vmux   = 6,
			.amux   = LINE1,
		}},
	},
	[SAA7134_BOARD_PHILIPS_TIGER_S] = {
		.name           = "Philips Tiger - S Reference design",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tuner_config   = 2,
		.mpeg           = SAA7134_MPEG_DVB,
		.gpiomask       = 0x0200000,
		.inputs = {{
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
		},{
			.name   = name_comp1,
			.vmux   = 3,
			.amux   = LINE1,
		},{
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE1,
		}},
		.radio = {
			.name   = name_radio,
			.amux   = TV,
			.gpio   = 0x0200000,
		},
	},
	[SAA7134_BOARD_AVERMEDIA_M102] = {
		.name           = "Avermedia M102",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.gpiomask       = 1<<21,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 6,
			.amux = LINE2,
		}},
	},
	[SAA7134_BOARD_ASUS_P7131_4871] = {
		.name           = "ASUS P7131 4871",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tuner_config   = 2,
		.mpeg           = SAA7134_MPEG_DVB,
		.gpiomask       = 0x0200000,
		.inputs = {{
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
			.gpio   = 0x0200000,
		}},
	},
	[SAA7134_BOARD_ASUSTeK_P7131_HYBRID_LNA] = {
		.name           = "ASUSTeK P7131 Hybrid",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tuner_config   = 2,
		.gpiomask	= 1 << 21,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
			.gpio = 0x0000000,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE2,
			.gpio = 0x0200000,
		},{
			.name = name_comp2,
			.vmux = 0,
			.amux = LINE2,
			.gpio = 0x0200000,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
			.gpio = 0x0200000,
		}},
		.radio = {
			.name = name_radio,
			.amux = TV,
			.gpio = 0x0200000,
		},
	},
	[SAA7134_BOARD_ASUSTeK_P7131_ANALOG] = {
	       .name           = "ASUSTeK P7131 Analog",
	       .audio_clock    = 0x00187de7,
	       .tuner_type     = TUNER_PHILIPS_TDA8290,
	       .radio_type     = UNSET,
	       .tuner_addr     = ADDR_UNSET,
	       .radio_addr     = ADDR_UNSET,
	       .gpiomask       = 1 << 21,
	       .inputs         = {{
		       .name = name_tv,
		       .vmux = 1,
		       .amux = TV,
		       .tv   = 1,
		       .gpio = 0x0000000,
	       }, {
		       .name = name_comp1,
		       .vmux = 3,
		       .amux = LINE2,
	       }, {
		       .name = name_comp2,
		       .vmux = 0,
		       .amux = LINE2,
	       }, {
		       .name = name_svideo,
		       .vmux = 8,
		       .amux = LINE2,
	       } },
	       .radio = {
		       .name = name_radio,
		       .amux = TV,
		       .gpio = 0x0200000,
	       },
	},
	[SAA7134_BOARD_SABRENT_TV_PCB05] = {
		.name           = "Sabrent PCMCIA TV-PCB05",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
		},{
			.name = name_comp2,
			.vmux = 0,
			.amux = LINE1,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		}},
		.mute = {
			.name = name_mute,
			.amux = TV,
		},
	},
	[SAA7134_BOARD_10MOONSTVMASTER3] = {
		/* Tony Wan <aloha_cn@hotmail.com> */
		.name           = "10MOONS TM300 TV Card",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_LG_PAL_NEW_TAPC,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.gpiomask       = 0x7000,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = LINE2,
			.gpio = 0x0000,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
			.gpio = 0x2000,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
			.gpio = 0x2000,
		}},
		.mute = {
			.name = name_mute,
			.amux = LINE2,
			.gpio = 0x3000,
		},
	},
	[SAA7134_BOARD_AVERMEDIA_SUPER_007] = {
		.name           = "Avermedia Super 007",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.tuner_config   = 0,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs = {{
			.name   = name_tv, /* FIXME: analog tv untested */
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
		}},
	},
	[SAA7134_BOARD_AVERMEDIA_M135A] = {
		.name           = "Avermedia PCI pure analog (M135A)",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.tuner_config   = 2,
		.gpiomask       = 0x020200000,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		} },
		.radio = {
			.name = name_radio,
			.amux = TV,
			.gpio = 0x00200000,
		},
		.mute = {
			.name = name_mute,
			.amux = TV,
			.gpio = 0x01,
		},
	},
	[SAA7134_BOARD_BEHOLD_401] = {
		/*       Beholder Intl. Ltd. 2008      */
		/*Dmitry Belimov <d.belimov@gmail.com> */
		.name           = "Beholder BeholdTV 401",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FQ1216ME,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.gpiomask       = 0x00008000,
		.inputs         = {{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		},{
			.name = name_tv,
			.vmux = 3,
			.amux = LINE2,
			.tv   = 1,
		}},
		.mute = {
			.name = name_mute,
			.amux = LINE1,
		},
	},
	[SAA7134_BOARD_BEHOLD_403] = {
		/*       Beholder Intl. Ltd. 2008      */
		/*Dmitry Belimov <d.belimov@gmail.com> */
		.name           = "Beholder BeholdTV 403",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FQ1216ME,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.gpiomask       = 0x00008000,
		.inputs         = {{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name   = name_comp1,
			.vmux   = 1,
			.amux   = LINE1,
		},{
			.name = name_tv,
			.vmux = 3,
			.amux = LINE2,
			.tv   = 1,
		}},
	},
	[SAA7134_BOARD_BEHOLD_403FM] = {
		/*       Beholder Intl. Ltd. 2008      */
		/*Dmitry Belimov <d.belimov@gmail.com> */
		.name           = "Beholder BeholdTV 403 FM",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FQ1216ME,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.gpiomask       = 0x00008000,
		.inputs         = {{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name   = name_comp1,
			.vmux   = 1,
			.amux   = LINE1,
		},{
			.name = name_tv,
			.vmux = 3,
			.amux = LINE2,
			.tv   = 1,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_BEHOLD_405] = {
		/*       Beholder Intl. Ltd. 2008      */
		/*Dmitry Belimov <d.belimov@gmail.com> */
		.name           = "Beholder BeholdTV 405",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.gpiomask       = 0x00008000,
		.inputs         = {{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
		},{
			.name = name_tv,
			.vmux = 3,
			.amux = LINE2,
			.tv   = 1,
		}},
	},
	[SAA7134_BOARD_BEHOLD_405FM] = {
		/* Sergey <skiv@orel.ru> */
		/*       Beholder Intl. Ltd. 2008      */
		/*Dmitry Belimov <d.belimov@gmail.com> */
		.name           = "Beholder BeholdTV 405 FM",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.gpiomask       = 0x00008000,
		.inputs         = {{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
		},{
			.name = name_tv,
			.vmux = 3,
			.amux = LINE2,
			.tv   = 1,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_BEHOLD_407] = {
		/*       Beholder Intl. Ltd. 2008      */
		/*Dmitry Belimov <d.belimov@gmail.com> */
		.name 		= "Beholder BeholdTV 407",
		.audio_clock 	= 0x00187de7,
		.tuner_type 	= TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type 	= UNSET,
		.tuner_addr 	= ADDR_UNSET,
		.radio_addr 	= ADDR_UNSET,
		.tda9887_conf 	= TDA9887_PRESENT,
		.gpiomask       = 0x00008000,
		.inputs = {{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
			.gpio = 0xc0c000,
		},{
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
			.gpio = 0xc0c000,
		},{
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv = 1,
			.gpio = 0xc0c000,
		}},
	},
	[SAA7134_BOARD_BEHOLD_407FM] = {
		/*       Beholder Intl. Ltd. 2008      */
		/*Dmitry Belimov <d.belimov@gmail.com> */
		.name 		= "Beholder BeholdTV 407 FM",
		.audio_clock 	= 0x00187de7,
		.tuner_type 	= TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type 	= UNSET,
		.tuner_addr 	= ADDR_UNSET,
		.radio_addr 	= ADDR_UNSET,
		.tda9887_conf 	= TDA9887_PRESENT,
		.gpiomask       = 0x00008000,
		.inputs = {{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
			.gpio = 0xc0c000,
		},{
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
			.gpio = 0xc0c000,
		},{
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv = 1,
			.gpio = 0xc0c000,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
			.gpio = 0xc0c000,
		},
	},
	[SAA7134_BOARD_BEHOLD_409] = {
		/*       Beholder Intl. Ltd. 2008      */
		/*Dmitry Belimov <d.belimov@gmail.com> */
		.name           = "Beholder BeholdTV 409",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.gpiomask       = 0x00008000,
		.inputs         = {{
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		}},
	},
	[SAA7134_BOARD_BEHOLD_505FM] = {
		/*       Beholder Intl. Ltd. 2008      */
		/*Dmitry Belimov <d.belimov@gmail.com> */
		.name           = "Beholder BeholdTV 505 FM",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.gpiomask       = 0x00008000,
		.inputs         = {{
			.name = name_tv,
			.vmux = 3,
			.amux = LINE2,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		} },
		.mute = {
			.name = name_mute,
			.amux = LINE1,
		},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_BEHOLD_505RDS] = {
		/*       Beholder Intl. Ltd. 2008      */
		/*Dmitry Belimov <d.belimov@gmail.com> */
		.name           = "Beholder BeholdTV 505 RDS",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_PHILIPS_FM1216MK5,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.rds_addr 	= 0x10,
		.tda9887_conf   = TDA9887_PRESENT,
		.gpiomask       = 0x00008000,
		.inputs         = {{
			.name = name_tv,
			.vmux = 3,
			.amux = LINE2,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		}},
		.mute = {
			.name = name_mute,
			.amux = LINE1,
		},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_BEHOLD_507_9FM] = {
		/*       Beholder Intl. Ltd. 2008      */
		/*Dmitry Belimov <d.belimov@gmail.com> */
		.name           = "Beholder BeholdTV 507 FM / BeholdTV 509 FM",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.gpiomask       = 0x00008000,
		.inputs         = {{
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		}},
			.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_BEHOLD_507RDS_MK5] = {
		/*       Beholder Intl. Ltd. 2008      */
		/*Dmitry Belimov <d.belimov@gmail.com> */
		.name           = "Beholder BeholdTV 507 RDS",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216MK5,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.rds_addr 	= 0x10,
		.tda9887_conf   = TDA9887_PRESENT,
		.gpiomask       = 0x00008000,
		.inputs         = {{
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		} },
			.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_BEHOLD_507RDS_MK3] = {
		/*       Beholder Intl. Ltd. 2008      */
		/*Dmitry Belimov <d.belimov@gmail.com> */
		.name           = "Beholder BeholdTV 507 RDS",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.rds_addr 	= 0x10,
		.tda9887_conf   = TDA9887_PRESENT,
		.gpiomask       = 0x00008000,
		.inputs         = {{
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		} },
			.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_BEHOLD_COLUMBUS_TVFM] = {
		/*       Beholder Intl. Ltd. 2008      */
		/*Dmitry Belimov <d.belimov@gmail.com> */
		.name           = "Beholder BeholdTV Columbus TVFM",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_ALPS_TSBE5_PAL,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.gpiomask       = 0x000A8004,
		.inputs         = {{
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
			.gpio = 0x000A8004,
		}, {
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
			.gpio = 0x000A8000,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
			.gpio = 0x000A8000,
		} },
		.radio = {
			.name = name_radio,
			.amux = LINE2,
			.gpio = 0x000A8000,
		},
	},
	[SAA7134_BOARD_BEHOLD_607FM_MK3] = {
		/* Andrey Melnikoff <temnota@kmv.ru> */
		.name           = "Beholder BeholdTV 607 FM",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		} },
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_BEHOLD_609FM_MK3] = {
		/* Andrey Melnikoff <temnota@kmv.ru> */
		.name           = "Beholder BeholdTV 609 FM",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		} },
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_BEHOLD_607FM_MK5] = {
		/* Andrey Melnikoff <temnota@kmv.ru> */
		.name           = "Beholder BeholdTV 607 FM",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216MK5,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		} },
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_BEHOLD_609FM_MK5] = {
		/* Andrey Melnikoff <temnota@kmv.ru> */
		.name           = "Beholder BeholdTV 609 FM",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216MK5,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		} },
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_BEHOLD_607RDS_MK3] = {
		/* Andrey Melnikoff <temnota@kmv.ru> */
		.name           = "Beholder BeholdTV 607 RDS",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.rds_addr 	= 0x10,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		} },
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_BEHOLD_609RDS_MK3] = {
		/* Andrey Melnikoff <temnota@kmv.ru> */
		.name           = "Beholder BeholdTV 609 RDS",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.rds_addr 	= 0x10,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		} },
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_BEHOLD_607RDS_MK5] = {
		/* Andrey Melnikoff <temnota@kmv.ru> */
		.name           = "Beholder BeholdTV 607 RDS",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216MK5,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.rds_addr 	= 0x10,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		} },
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_BEHOLD_609RDS_MK5] = {
		/* Andrey Melnikoff <temnota@kmv.ru> */
		.name           = "Beholder BeholdTV 609 RDS",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216MK5,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.rds_addr 	= 0x10,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_BEHOLD_M6] = {
		/* Igor Kuznetsov <igk@igk.ru> */
		/* Andrey Melnikoff <temnota@kmv.ru> */
		/* Beholder Intl. Ltd. Dmitry Belimov <d.belimov@gmail.com> */
		/* Alexey Osipov <lion-simba@pridelands.ru> */
		.name           = "Beholder BeholdTV M6",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.empress_addr 	= 0x20,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = { {
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		} },
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
		.mpeg  = SAA7134_MPEG_EMPRESS,
		.video_out = CCIR656,
		.vid_port_opts  = (SET_T_CODE_POLARITY_NON_INVERTED |
					SET_CLOCK_NOT_DELAYED |
					SET_CLOCK_INVERTED |
					SET_VSYNC_OFF),
	},
	[SAA7134_BOARD_BEHOLD_M63] = {
		/* Igor Kuznetsov <igk@igk.ru> */
		/* Andrey Melnikoff <temnota@kmv.ru> */
		/* Beholder Intl. Ltd. Dmitry Belimov <d.belimov@gmail.com> */
		.name           = "Beholder BeholdTV M63",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.empress_addr 	= 0x20,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = { {
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
		}, {
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		} },
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
		.mpeg  = SAA7134_MPEG_EMPRESS,
		.video_out = CCIR656,
		.vid_port_opts  = (SET_T_CODE_POLARITY_NON_INVERTED |
					SET_CLOCK_NOT_DELAYED |
					SET_CLOCK_INVERTED |
					SET_VSYNC_OFF),
	},
	[SAA7134_BOARD_BEHOLD_M6_EXTRA] = {
		/* Igor Kuznetsov <igk@igk.ru> */
		/* Andrey Melnikoff <temnota@kmv.ru> */
		/* Beholder Intl. Ltd. Dmitry Belimov <d.belimov@gmail.com> */
		/* Alexey Osipov <lion-simba@pridelands.ru> */
		.name           = "Beholder BeholdTV M6 Extra",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216MK5,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSET,
		.rds_addr 	= 0x10,
		.empress_addr 	= 0x20,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = { {
			.name = name_tv,
			.vmux = 3evice ariver TVevice tv   = 1evic}, {vice  *
 /*
 *
 *compcardse driver  *
 * (lips saLINEcards
 * card-specific stusvideoevice driver 804 Gerd Knorr <kraxel@b }evic.radio =* card-specific sturedis04 Gerd Knorr <kr2ards
 can mpeg TV SAA7134_MPEG_EMPRESSf thes]
 *_out = CCIR656as publi_port_opts TV (SET_T_CODE_POLARITY_NON_INVERTED |
 * 		n; eCLOCK_NOT_DELAYense, or
 *  (at youhe License, or
 *  (VSYNC_OFF),
 of t[eral PubBOARD_TWINHAN_DTV_DVB_3056]tribute-speciT ANY WARR= "Twinhan Hybrid DTV-DVB l,
  PCI"f theaudio_clockARRANT0x00187de7f thetuner_typUT ANY= TUNER_PHILIPS_TDA8290 can redisA PARTICULARUNSETTNESS FOR Aaddr	= ADDR_icense for Generdetails.
 *
 *  You sh FOR AconfigCULARerms e GNU GANY WARRANTeral Public LDVBf thegpiomasHANTANTABILIT20Freef theinputstrib card-speciULAR *
 * device drive TV cardserd KnoULAR 7134 based T TV cards
 * card-specinc., 675 ff.
 *
 * (c) 20ULARfor philips ULAR <kraxel@bytesex.org> [nc., 675 bs]
 *
 *  This pULAR8,		/* untested */<linux/i2c.h>
#include <ou can redistribute it andnc., 675 ify
 *  it under, USA.
 */

if ne to the Free Softof td in the hope that GENIUS_TVGO_A11MCE*  but W/* Adrian Pardini <pardo.bsso@gmail.com>saa713-spec		= "Genius clud AM"xc50anty of
 *  MERCH	BILITY Free Softw FOR A PAR	AR PURPOTNF_5335MF  GNU General Public License for more details.
 *
 *  You should have received a copy ofif not, write to thfee Software
 * NY WARRANToundation, I., 675 Ma_mon*
 *  This proge, MA 02139 the terms prom.h>BILITYe Softbased TV cards
 * card-specific stuff.
 *
 * (c) 2001for philips sa <kraxel@_comp3[]    Freomposite3= 1el@bytesex.org> [SuSE Labs]
 *
 *  This program is free software;   = "S-Video";

/*you can redistribute it and/or modify
 *  it under the terms _comp3[]   1"tea5767.h"	.mutchar card-specific studr s         */

/* If radio_type !=U6"tea5767.h"
#include "tda18271SE.  SeeSNAK000.h"

/ITHOUT ANY WARRANTYNXP Snake DVB-S reference designanty of
 *  MERCHANTABILITY
static char name_tv[TICULAR PURPOABSENYou should hal Public License for more detanclude .
 *
 *  You should have .inputs         = {{
	 License
 *  along with this program; are
 *  Foundation, Inc., 675 ux/module.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>

#include "saa7134-reg.7134.h"
#include "tuner-xc202d in the hope that CREATIX_CTX953*  but WITHOUT ANY WAR= "Medion/Creatix SET,
	ut evenanty of
 *  MERCHANBILITY or FITNESS FOR A PARTICAR PURPOSE.  See the
 *  GNU General Publaddr	= ADDR_UNSET,

		.ints         = {{
			.name = "deeceived a copy of the GNU Gene=  Softw License
 *  INE1,
		}},
	},
	[SAA7134_BOARmposite1";
static char name_c]   = "Composite2";
static a7134 based TV cards
 * card-specific stuff.
 *
 * (c) 2001

/* --d Knorr <kraxel@bytesex.org> [SuSE Labs]
 *
 *  This program is free software; you cad in the hope that MSI_TVANYWHERE_AD11*  but WITHOUT ANY WARRANTYMSI TV@nywhere A/D v1.1anty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Folude <media/v4l2-commMass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>

#include "saa7134-reg.7134.h"
#include "tuner-xc2028.h"
#include <media/v4l2-common.h>
#include <media/tveeprom.h>
#include "tea5767.h"
#include "tda18271AVERMEDIA_CARDBUS_50 *  but WITHOUT ANY WARRANTYAVer,
		a Cardb;
sta/Redist(E506R)anty of
 *  MERCHANTABILI or FITNESS FOR A PARTICULAR PURPOXC202mux =U General Public License for more details.
 *
 *  You should have received a copy o  0,
			.amux = LINE1,
		}},
	},
	[SAA7 []   = "Composite1";
sta -specific stue = nam  "Composite2"; ilips saa7134 b ased TV cards 
 * card	.tuner_addr	=ff.
 *
 * NSET,
		.for phaddr	= AD <kraxel@k       = 0xe000,
		.inbs]
 *
 *  NSET,
		.mux = e = name_tv,
erms  -xc2028  redistribute 	.tuner_addr	=ify
 *  itaddr	= ADDR_UNS00,
		.tuner_type	= TUNEute = {
		A16Dme_mute,
			.amux = TV,
			.gpio = 0xt even },
	},
	[Somp24_BOARD_FLYVIDEO2000] = {
		/* "TC Wan" <tcwan@cs.usm.my> */
		.name           = "LifeView/Typhoon FlyVIDEO2000",
		.audio_clock    = 0x00200000,
		.tune 0,
			.amux = LINE1,
		}},
	},
	[SAA7134_BOARComposite1";
static char name_c= name_radio,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_FLYVIDEO300bs]
 *
 *  This program is free software; y * card-specific stuff.
{
		/* "Marco d'Itri" <md@Linux.IT> *ou can redistribute it and/or modify
 *  it under tA.
 */= 0x4000,
		},{
			.name = name_cM115me_mute,
			.amux = TV,
			.verm = 0xADDR			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
			.gpio = 0x4000,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
			.gpio = 0x2000,,
			.amux = LINE2,
			.gpio = 0x8000,
		},
	},
	[SAA7134_BOARD_FLYTVPLATINUM_MINI] = {
		/* "Arnaud Quett= "Composite4";
static char name_svideo[] {
		/* "Arnaud Quette" <aquette@free.fr> */
		.name      erms o00,
		.tuner_type	= TUNEVIDEOMATE_T75000.h"

/* cJohn Newbigin <jn@it.swin.edu.aume_mute[]   T ANY WARRANTYCompro V]
 *Mate V Planty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,     /* Composite signal on S-Video input */
			.v Foundation, Inc., 675 Mass Ave, Cambride <linux/i2c.h>
A.
 */

#include <linux/init.h>
#include <linux/module.h>
#includge, MA 02139, UShe terms ofx4000,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = FLYTVPLATIN	.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_ty0x4000,
		},{
			.name = name_co700_PRO00.h"

/* cMatthias Schwarzott <zzam@gentoo.orgpeter.missel@onlinehome.deR_UNSET,

	= 0x0Pro  LINanty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOdr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,

		.inputs         = {{
			.name = "default",
			.vmux = 0,
			.amux = LINE1,
		}},
	},
	[SAA7134_BOARComposite1""LifeView FlyTV Platinum Mini",
		.a-04 Gerd Knorr <kraxel@bytesex.org> [SuSE Labs]
 *
 *  This pro Freeio_clock	= 0x00200000,
		.tuner_type	= TUNE			.amux = LINEHYBRI2,
			.vm = 0x0000,
			.tv   = 1,
		},{
*/			.name = name_comp1,	/* Composite signal on S-t even+FMeo input */
			.vmux = 0,
			.amux = LINE2,
/*			.gpio = 0x4000,  
		.name           = "LifeView/Typhoon FlyVIDEO2ite input */
			.vmux = 3,
			.amux = LINE2,
/*			.gpio = 0x4000,         */
		},{
			.name = name_svideo,	/* S-Video signnc., 675 Mass Ave, Cambrid4gpiomask       = 0x1E000,	/* Set GP16 and unused 15al on S-Video input */
			.vmux = 8,
			.amux = LINE2,
/*			.gpio = 0x4000,         */
		}},
		.radio = {
			.name =	.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tunBEHOLD_H *  but W/* Igor Kuznetsov <igk@igk.r<peter.missel@onlinehome.deBeholder _tv_moTV H6anty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  SeeFMD1216MEX_MKfor pUNSET,
		.radio_addr	= ADDR_UNSET,

		.inputs         = {{
			.name = "default",
			.vmux = tda9887GNU G1,
			DA	.amuense= ADDR_U
		},
		.mute = {
			.name = name_mute,
			.amux = LINE2,
			.gpio = 0x8000,
		},
	},
	[Sfor philips saa7134 based TV cards
 * card-specific stuff.
 *
 * (c) 2001-04 Gerd Knorr <kraxel@bytesex.org> [SuSE Labs]
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of t name_radio,
			.amuSUSTeK_TIGER_3INradio_type     = UNSET,
		.tAs;
stiger 3in,
		.radio_addr	= ADDR_UNSET,

		.gpiomask       = 0xe000,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			 = 0x4000,
		}, {
			.name = name_comp2,
			.vmux = 3,
the GNU General Publicif not, write to 1 << 2cards,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
			.gpio = 0x4000,
		} },
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_FLYVIDEO3000] ={
		/* "Marco d'Itri" <md@Linuxme = name_tv,
			.v			.amux = LINE2,
		}},
	},
	[SAA7134_BOARD_FLYTVPLATIN	.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_t_comp3[]   =de "tea5767.h"
#include "tda18271REAL_ANGEL_22latinum Fissel@onlinehome.deZogis Real Angel 22nput */
			.vmux = 0,
			.amux = LINE2,
/*			.gpio = 0x4000,  "Television";
static char name_tv_mono[] = "TV (mono.inputs         = {{
			.name = "default",
			.vmux = if not, write to th801a808ITNESS1,
		},{
			.name = name_comp1,
			.vmux = 0,
		e <linux/i2c.h>
#incf radio#include <linuprom.h>
#incl624"tea5767.nd unused 15,14,13 to Output */
		.inputs         = {{
			.na			.vmux = 8,
			.amux = LINE1,
		}},
		.radio = {

#include "saa7134-reo,
			.amux = LINE2,
		},
	},
	[SAA7134_BOARD_MD9xc2028.h"
#include <media/v4l2-common.h>
#include <medi/* If radio_type,
			.amux cards
 o_addr should be specified
 */

struct saa7134adio_type     = UNSET,
		.tuneDS_INSTANT_Hbe uPCI.audio_clock	= 0x00187de7,
	DS Tech Instant 1,
	anty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See UV1236D,
			.vmux = 0,
			.amux = LINE2,
			.gpio = 0x4000,
		}, {
			.name = name_comp2,
			.vmux = 3,
			.amux = LINE2,
			.gpio = 0x4000,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux =uld be specified
 *.vmux = 1,
			.amux = LINE2,
			.tv   = 1,
		}},
		.radio = {
			.name = name_radio,
		ILIPS_FM1216@Linux.IT> */
		.name		= "LifeView FlyVIDEO3000",
		.audio_clock	= 0x00200000,
		.tuner_type	= TUNEe		= "EMPRESS.audio_clock	= 0x00187de7,
		.tuner_tyRev:1.input */
			.vmux = 0,
			.amux = LINE2,
/*			.gpio = 0x4000,  SE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General no,
			.vmux = 1,
	ux = 1,
			.amux = LINE2,
			.gpio = 0x0000,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE2,
			.gpio = 0x4000,
		},{
			.name = name_comp2,
			.vmux = 3,
			.amux = LINE2,
			.gme = name_tv,
			.vmux = 1,
ff.
f radioaa7134-re d'Itri" <md{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.gpio = 0x10000,	/* GP16=1 selects TV input */
			.tv  v4l2-common.h>
#include <media/tveeprom.h>
#include "tea5767.h"
#include "tda18271KWORLD_PL#incl_ANALOG*  but WITHOUT ANY WARRANTYKworld Pl;
sta Analog Litewarranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOYMEC_TVF_5533ion";
static char name_ter_addrEA576ITNESS FOR Ainputs         = {{
			.name = name_tv,
			rds[
			.amux = TV,
			.tv   =0007tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE2,
name = name_svideo,
			.vmux = 8,
			.1 = LINE1,
		}},
		.radio = {
			.name = name_radie <linux/i2c.h>
#include prom.h>
#incl2ARD_MD9717] = {
		.name		= "Tevion MD 9717",
		.aumux = 8,
			.amux = LINE2
		},{
			.name = nam LINE1,
		}},
		.radio = {
			.name = name_radname_radio,
			.amux = LINE2,
		},
	},
	[SAA713			.name o_addr should be specified
 */

struct his program is free svoort@philips.com> */
		.namete = {
		GO_007_FM.name_UNSET,
		.radio_addr	= ADDR_UNSET,

.gpiTV GO 007 FMk	= 0anty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy ofif not, write to the03x820for p/* .amux = TV,
			.tv   cmux =3,e_mute[amux = LINE1,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	      _comp3[]   =mux = LINE2,
/*			.gpio = 0x4000,         */
		}},
		.radio = {
			.men.or.jp> */	},
	[SAA7134_BOARD_MONSTERTV] = {
		/* "K.Ohta" <alpha292@bremen.or.jp> *.name DDR_UNSET,
		.inputs         = {{
			.name = name_tv,
			.vcomp2, /* CVideo ov         = "Terratec Cinergy 40STUDIO_507UA00.h"

/* condy Shevchenko <andy@smile	.na.uame = name_comp1,	/* Composite signale     =St
 *  	= AD 1,
			.amux = LINE2,
			.gpio = 0x0000,
			.tv   = 1,
		}, {
			.name  name_omp1, /* Should be MK5e_mute[.vmux = 0,
			.amux = LINE2,
			.gpio = 0x4000,
		}, {
			.name = name_comp2,
			.vmux = 3,
			.amux = LINE2,
			.gpio = 0x4000,if not, write to the1,
			,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name = name_comp2, /* CV= LINE1,
		}},
		.rp4[]   = "Composite4";
static char name_svideo[]  = "S-Vide
			.amux = LINE2,
		}},
		---------------------------------- */
/* board config		.name = name_radio,
			.amux =                      */

/* If radio_type !=U7de7, /* was: 0x0hould be specified
 */

struct saa7134_boa	.amux = LINE2,
		},
	},INUM_FM] = {
		/* LifeView FlyTS3Platinum FM (Lan D. Louw <jd.louw@mweb.co.zae_mute[]    = "m> */
		.name     {{
	/S3input */
			.vmux =Radio"; or FITNESS FOR A PAR]      = dr	= ADDR_UNSET,
		.r	adio = {
			.name = name_radio,
			.amux = LINE2,
			.gpio = 0x2000,
		} = "E1,
		}},
	},
	[SAA7134_BOARD_Puld be spec				.amux = LINE2,
		},Radiux = 8,
			>
#include <linux/i2c-al	.audio_bs]
 *
 *  This 				.d foNot lude "saa7134.h"
#er_type YTVPLATINUM_FM] = {
		/* Lif,
			.aX7x = TV,
			_tv_mono,Intl. Ltd. Dmitry Belim00,
d.b      ic char name_mute[]   		.name = name_tv_mono,
			.vmuxX7.gpio = 0x10000,
		},
	},
	[SAA7134_BOARD_ROVERMEDIA_LINK_PRO_F5ee Softw
		.radio_addr	= ADDR_UNSET,

		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = LINE1,
		}},
		.radio = {
			.name = name_radf radioux = LINE2,
		},
	},
	[SAA7134_BOARD_FLYVIDEO3000] = {
		/* "Marco d'Itri" <md@Linux.IT> */
		.name		= "LifeView FlyVIDEO3000",
	9dio_clock    = 0x00200000,
		.tuner_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tunZOLIDpio = 0	},{
			/* workaround for problZolid	.gpio = 0 TFOR warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more deta 	= 0x20,

		.inputs         = {{
			.name = name_comp1,
			.vmux = 0,
 TDA9887_PRESENT,
		.inputs         = {{
			ts,
			.ameral Public LTS_PARALLELput */
			.vmux = 0,
			.amux = LINE2,
		},{
			.name = name_comp2,	/* Composite input */
	ou can redistrib"
#include "saa7134. = TUNER_PHILIPS_TDA8290,
		.radio_type    
};

const untuneed int saal Pubbcounby
 ARRAY_SIZE(OON_90031oards);

/* -r 90031 */
		/* Tom Zoerner <tomzo at users sourceforge net> */
	saa7/*warr ids + subsystem IDe_svideo,	031",
		.audio_clock    = 0x0020000*/

struct pci_device_idHOON_9003typetbl[*  but clockvendo= 1,
	dio_PCI_VENDOR_unerE.  Se	.vid     == UNSET,
		.tDEVICEddr	= ADDR__eral Pu	.vidsube     = UN,
		.tuner_addr	= ADDR_UNSETsub,
		.radio	.name ame = driver_data
			.amux = NKNOWN]ROTEUS
			ADDR_o_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_co1,
			 TDA9887_PRESENT,
		.inputs         = {{
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
		},{
			.name   = name_comp1,
			.vmux   = 3,
			.amux   = LINE1,
		},{
			.name   = namnf   = TDA9887_PRESENT,
		.inputs         = {{
			.name   = name675Publicvmux   = 1,
			.amux   = TV,
icense as },
	[SAA7134_BOARD_ELSA] = {
		.name           = "ELSA EX-VISION 300TV",
		.audio_clock    = 0x00187de7,
		.tune0x113ame = 			.name   = name4e85    = UNSET,
		.tuner_addr	= ADDR_MONSTERA.
 *radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.n53b   = name_comp1,
			.v114     = UNSET,
		.tuner_addr	= ADDR_CINERGY4nfo   e_tv,
			.vmux = 4,
			.amux = LINE2,
			.tv   = 1,
		}},
	},
	[SAA7134_BOARD_ELSA_500TV] = {
		.name           = "ELSA EX-VISION 500TV",
		.a1,
			lock    = 0x00187de7,
		.tuner_type 6   = TUNER_HITACHI_NTSC,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_5ame   
			.vmux = 7,
			.amux = LINE1,
		},{
omp1,
	,
		},{
			.name   = name_comp1,
			.vmux   = 3,
			.amux   = LINE1,
		},{
			.name   = name_svideo,
			.vmux    = "ELSA EX-VISION 500TV",
		.6udio_clock    = 0x00187de7,
		.tuner_type    		.name  name_tv,
			.vmux = 4,
			.amux = LINE2,
			.tv   = 1,
		}},
	},
	[SAA7134_BOARD_ELSA_500TV] = {
		.name           =516,
		. name_comp1,
			.v013	},
	[SAA7134_BOARD_ELSA_700TV] = {FLYeViewname_NTSC     = {{
			.name = name_tv,
			.vmux = 4,
			.amux = LINE2,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 6,
	ame   x = LINE1,
		},{
			.name = name_svideo,
			.vmux = 7,
			.amux = L name_tv,
			.vmux = 4,
			.amux = LINE2,
			.tv   = 1,
		}},
	},
	[SAA7134_BOARD_ELSA_500TV] = {
		.name           =4e42,or
 /* "Typhoon     CaptureUNSE8000" Art.No. 50673e_mute[     = "ASUS TV-FM 7134",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
7_PRESVFM7134] = {
		.name           = "ASUS TV-FM 7134",
		.audio_clock    = 0x00187de7,
		.tu info   
			.name = name_comp1,
			.vmux = 4,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 6,
			.amux = LISET,
		/* 87_conf     = {{
			.name = name_tv,   = LifeView FlyTV Prime30 OEMHILIPS_name_radio,
			.amux = LINE1,
		},
	},
	[SAA7134_BOARD_ASUSTeK_TVFM7135] = {
		.name           = "ASUS TV-FM 7135",
		.audio_clock    =HI_NTSC,
		.radio_type ame           = "ASUS TV-FM212UNSETminipci, LR212DDR_UNSET,
		.radio_addr	= ADDR_UNSET,
TVPLATINUM_MINI     = "ELSA EX-VISION 700TV",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_HITACHI_NTSC,
		.radio_type  4cvmux = 6,SION 500TV",
		ame_comp1,
			.vmux,
		4,
			.amux = LINE2,
			.gpio = 0x0000,
		},{
			.name_BOA  = "ELSA EX-VISION 700TV",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_HITACHI_NTSC,
		.radio_type SET,
 1,
		},{
			.name = name_comp= AD1,
			.vmux = 4,
			.amux = LINE2,
			.gpio = 0x0000,
		},{
			.name = name_svideo,
			.vmux = 6,
			.amux = LINE2,
			.gpio = 0x0000,
		}},
		.radio = {
			.name = name_radio,
			.amuxame  T,
		.imation withnologies (= UNSET,)HILIPS_TDA8290,
		.radio_214nd fortandard
		.vmux =4TV T E and earlier (eral P5e_tv,
		amux = LINE2,
			.gpio = 0x0000,
		},{
			.FM.inputs         = {{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
		},{
			.name = name_tv,
			.vmux = 1,
			.a5ux = LINE2,
			.tv   = 1,
		}},
F onwdio"_BOARD_11MOONSTVMASTER] = {
		/* "lilicheng" <llc@linuxfans.org> */
		.name           = "10MOONS PCI TV CAPTURE CARD",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_LG1489UNSETKYE_tv,
			.vmux = 1,
			.amux = LINute";
s.nameWonono,ProTVOONSTVMASTER] = {
		/* "lilicheng" <llc@linuxfans.orgd fois an= 1,
	WF actuallyHILIPTUNER_HITACHI_NTSC,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs   6bstruc {{
			.name = namee = nam0,
			.amux   = LINE1,
		},{
		D   = 0x	.amux = LINE2,
			.gpio = 0x4000,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
			.gpio = 0x2000,
		},
d foSET,46 a0000,
TV, HW  GNU,l on T		.tuner_addr	= ADDR_UNSEer_td foonly
		.audio_},
	[00000,for nowOONSTVMASTER] = {
		/* "lilicheng" <o = 0x8000,_clockame = name_comp1,
			.vmux = 4,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 6,
			.amux = LI104           = "ASUS TV-F226LSA EX UNSET,
		.tuner_addr	= ADDR_ULSA	},{
			.name = name_comp1,
			.vmux = 4,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 6,
			.amux = LI		.name = name_comp2,
			.vmua = 3,
			.amux = LINE1,
		},{
			.name_500 = name_tv,
			.vmux = 4,
			.amux = LINE2,
			.tv   = 1,
		}},
	},
	[SAA7134_BOARD_ELSA_500TV] amux = LINE1,
		},{
			.name = name_svideo,
			.vmuxc= 8,
			.amux = LINE1,
		}},
		.mpeg   7  = SAA7134_MPEG_EMPRESS,
		.video_out = CCIR656,
	},
	[SAA7134_BOARD_VIDEOMATE_TV] = {
		.name   nf   = TDA9887_PRESENT,
		.inputs  e		= EK   = name_comp1,
			.vm8audio_clock    = 0x00187de7,
		.tunee		= "EMPVFM= 0x8000,
		},
	},
	[SAA7134_BOARD_BMK_MPEX_NOTUNER] = {
		/* "Andrew de Quincey" <adq@lidskialf.ne_svideo,
			.vmux   = 8,
			.amux me_svideo,
			.vmux = 8,
			.amu   = 0,
			.amux   = LINE1,
		},{
	mp1,
			.vmux =   =TUNER_HITACHI_NTSC,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputsARD_VIDEOMATE_TV_GOLD_PLUS] = {
		.name             NE1,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
		},{
			.name = name_tv,
			.vmux = 1,
			.amux = LINE2,
			.tv   = 1,
		}},
	},
	[SAA7134_BOARD_VIDEOMATE_TV_GOLD_PLUS] = {
		.name       ame_mute,
			.amux = LINE2,
			.gpime_svid	.vmux =      = "ELSA EX-VISION 700TV",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_HITAC	.inputs         = {{
			.name = name_svideo,
			.vmux = 8,
			.amuADDR_UNSET,
		.inputs         = {{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
			.gpio = 0x06c00012,
		},{
			.name = name_comp1,
		nf   = TDA9887_PRESENT,
		.inputs         = {{
			.name   = namefev,
			.vmux   = 1,
			.amux   = TV,
TVSTATION_RD
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.n89nf   = TDtuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.gpiomask       = 0xcf00,
		.inputs         = {{
			.name = name_comp1,
			.vmux = 0,
			.gpio = 2 << 14,
		},{
			.name = name_comp2,
			.vmux = 0,
			.a00 Free ,
		.radio_addr	= ADDR_UNSET,
		.gpiomaDVR		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name   = name_comp1,
			.v,
		}},
	vmux   = 1,
			.amux   = TV,
VANSETPOWE		.name           = "AverMedia M156 / Medion 2819",
		.audio_clock    = 0x00187de7,
		.tuner_tyvmux = 6,
			.amux = T,
		.inputs         = {{
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
10MO	.naVMAname     = "ELSA EX-VISION 700TV",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_HITACHI_NTSC,
		.radio_type  85LSA EX-VISION 500TV",
	c			.namUNSET,
		.tuner_addr	= ADDR_UNSView FlyTV= name_tv,
			.vmux = 4,
			.amux = LINE2,
			.tv   = 1,
		}},
	},
	[SAA7134_BOARD_ELSA_500TV] 			.amux = LINE1,
			.gpio = 0x02,
		}, {
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
	_G		.anameio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.gpiomask	= 0x03,
		.inputs         = {{
			.name = name_tv,
			.vmuMATROXeo,
			.vmux = 8,
			.amd name_svideo,
			.vmux = 8,
			.amuCRONOSX_TUNER] = {
		/* "Greg Wickham <greg.wickham@grangenet.net> */
		.name           = "BMK MPEX Tunermux = LINE1,
		},{
			.n461NT,
	R_UNSET,

,
		},{
			.nIncHILIPS_TDA8290,
		.radioa70x = 3,
			.amux = LINE1,
		},{
			.MD281,
		vmux = 1,
			.amux = LINE2,
			.gpio = 0x0000,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = Lmux = 1,
			.amux = LINE1,
		},{
			.name = name_svideo,
		a,
			.vmux   = 1,
			.amux   = TV,
			.amux = LINE2,
_UNSET,
		.empress_addr 	= 0x20,
		.inputs         = {{
			.name = name_comp1,
			.vmux = 4,
		RESS,
		.video_out = CCIR656,
	},
	[SAA7134_BOARD_ASUSTEK_TVFM7133] = {
		.name      x = LINE1,
		},{
			.name = name_comx = TV,
			.gpio = 0_UNSET,
		.empress_addr 	= 0x20,
		.inputs         = {{
			.name = name_comp1,
			.vmux = 4,
			.amux = LINE1,
		},{
		R656,
	},
	[SAA7134_BOARD_ASUSTEK_TVFM7133] = {
		.name   211    = "Compro VideoMate TV Gold+",
	T,
		.tuner_addr30   = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_NTSC_M,
		.gpiomask       = 0x800c0000,
		.r			.amux = TV,
			.tv   = 1,

		},{
			.name = name_comp1,
			.vmux = 4,
			.amux a LINE2,
		},{
			.name = name_svideo,
			.vmux = 6,
		5.amux =da9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,

		},{
			.name = name_comp1,
			.vmux = 4,
			.amux = 0	},
	[SAA7134_BOARD_ELSA_700TV] = {
			.vmux 	.amux = LINE2,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE1,
		},
	},
	[SAA7134_BOARD_PINNACLE_PCTV_STEREO] = {
		.name           = "Pinnacle PCTV Stereo (saa7134)"10ffinputs         = {{
			.name = name_tv,
			.DVD_EZMAK1,
			.vmux= 1,gpio = 0x8000BusHILIPS_SET,
		.radio_addr	= ADDR_UNSET,
		.empress_addr 	= 0x20,
		.inputs         = {{
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		},{
			.name = name_svideo,
d6e,
		.ms         = {{
			.name = name_tv,
			.uts         = {{D_MANLI_MTV002] = {
		/* Ognjen Nastic <ognjen@logosoft.ba> */
		.name           = "Manli MuchTV M-TV002",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.radio_type  b7e		.amuT,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	=_5v,
		T2032,
 = TransGear name3,
			.am2,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE1,
		},
	},
	[SAA7134_BOARD_PINNACLE_PCTV_STEREO] = {
		.name           = "Pinnacle PCTV Stereo (saa7134)"050 0x00187de7,
		.tuner_type     = TUTGLINE2,		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.nabd
		.mute = {
			.name = 2x = 3,
			.amux = LINE1,
		},{
			.PINNACLE_PCTV_nameE},
	},
	[SAA7134_BOARD_ELSA] = {
		.name           = "ELSA EX-VISION 300TV",
		.audio_clock    = 0x00187de7,
		.tunevideo,
			.vmux = 8,
			.amux =,
			.,
		},{
			.name = name_comp1,
			.vmu300IprogT_PA 8,
TUNER_HITACHI_NTSC,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs   0name = name_comp1,
			.vmcbnf   = UNSET,
		.tuner_addr	= ADDR_UCinclP3XPme = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
		}},
		.mpeg      = SAA7134_MPEG_EMPRESS,
		.video_out = CCIHILIPS_NTSC_M,
		.radio_type   = 0,
			.amux   = LINE1,
		},{
	UNSET,
		._4CB   = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_NTSC_M,
		.gpiomask       = 0x800c0000,
		.radio_type     = UNSET	},{
			.name = name_comp1,
			2 << 14,
		}},
	},
	[SAA7134_BOARD_			.name = nam Fre  = "ELSA EX-VISION 700TV",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_HITACHI_NTSC,
		.radio_type  2aLSA EX-VISION 500TV",
	08= name_svideo,
			.vmux = 8,
			.amuUPMOST_PURPL1,
			.gpio = 0x02,
		} },
		.radio = {
			.name = name_radio,
			.amux = LINE1,
			.gpio = 0x01,
amux = LINE1,
		},{
			. LINE2,
			.tv   = 1,
		}},
udio_clock    = 0x00187de7,
		.tuner_type me = nname_comp3,
			.vmux = 0,
			.amux = LINE1,
		},{
			.name = name_comp4,
			.vmux = 1,
			.amux = LINE1,
		},{
			.me = name_mute,
			.amux = TV,
			.gpio = 0x00,
		},
	},
	[SAA7134_BOARD_BMKP
		.name           = "AverMedia M156 / Medion 2819",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FR656,
	},
	[SAA7134_BOARD_ASUSTEK_TVFM7133] = {
		.name   97LINE2,
		},{
			.name = name_svideo,
			.vmux = 6,
			.ITNEADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.empress_addr 	= 0x20,
		.inputs         = {{
			.name = name_comp1,
			.vmux = 1,
			.amux = LINE1,
		},{
			.name = name_svideo,
			 = 8,
			.amux = LINE1,
		}},
		.mpe_tv,
			.vmADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
	io = 0x02,
		}, {
			.nameme = na_svideo,
			.vmux = 8,
			.amux = LINE1,_MPEX_TUNER] = {
		/* "Greg Wickham <greg.wickham@grangenet.net> */
		.name           = "BMK MPEX Tuner= {
		.name           = "  .. 7 type     = TUNER_P52     = UNSET,
		.tuner_addr	= ADDR_PROx = L_PV91,
	k       = 0x200000,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.gpio = 0x0000,
			.tv   = 1,
		},{
			.name = n502,031",
		.audio_co Tu000,
		versINE1ONSTVMASTER] = {
		/* "lilicheng" <llcar 30DUOo_addr	= ADDR_UNS= 0x200000,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.gpio = 0x0000,
			.tv   = 1,
		},{
			.name = n306SET,
		.radio_addr	=    SET,
		.inputs         = {{
			.name   = name_tv,
DU = 1,
		},{
			.name   = name_comp1,
			.vmux   = 3,
			.amux   = LINE1,
		},{
			.name   = name_svideo,
			.vmux   M)",
		.audio_clock    = 0x187de7,
		.tuner_type     = TUNER_f31_svideo,
			.vmux = 8,
			.amux = LINE2,
		}}0 TV",
		1246 Tuner Card (PAL,FM)",
		.audio_clock    = 0x187de7,
		.tuner_type     = TUNER_PHILIPS_PAL_I,
		.radio_type     = dio = {
			.name   = name_radio,
			.amux   = LINE2,
		},
	}11},
	},
	[SAA7134_BOARD_TG3000TV] = er_addr	= AD35e = naT,
		.empress_addr 	= 0x20,
		.inputs         = {{
			.name = name_comp1,
			.vmux = 4,
			.amux = LINE1,
		},{T,
		.inputs         = {{
			.name   = name_tvan.co.uk> */
		.name		= "Elitegroup E.  See OUGHme = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
		}},
		.mpeg      = SAA7134_MPEG_EMPRESS,
		.video_out = CCIRname = {
			.name   = name50e     ,
			.amux   = LINE1,
		},{
			.name   = name_s
			.tv   = 
			.gpio = 0x02,
		} },
		.radio = {
			.name = name_radio,
			.amux = LINE1,
			.gpio = 0x01,
		},
		.mute  = {
			.na		.amux = LINE2,
		}},
		.ra1io = {
			.name =, new revime = name_radio,
			.amux = LINE2,
			.gpio = 0x200000,
		},
	},
	[SAA7134_BOARD_AVERMEDIA_DVD_EZMAKER] = {
		/* Michael Smith <msmith@cbnco.com> */
		.name           = "AVerMedia DVD EZMaker",
		.au7dio = {cADDR_UNSET,
		.inputs         = {{
			.name   = napio = 0x200000,
		},
	},
	[SAA7134_BOARD_AVERMEDIA_DVD_EZMAKER] = {
		/* Michael Smith <msmith@cbnco.com> */
		.name           = "AVerMedia DVD EZMaker",
		.1videadio_addr	=	.vmux = 8,
		}},
	},
	[SAA7134_BOARD_AVERMEDIA_M103] = {
		/* Mass_BOARD_VA1000POWER] = {
		.name           = "AOPEN VA1000 POWER",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUN   = TUNER_PHILIPS_TDA8290,
		.radio__UNS     = UNSET,
LR502	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
tv,
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
		},{
			.name   = name_tv_mono,
			.vmux   = 1,
			.amux   = LINE2,
			.tv     10e = nam	.vmux = 1,
			.amuxdio = {1,
	_typ NE1,		.amux   = LINE1,
		},{
			.name   = name_svide usIGI.tunIr_ty/
		.name           = "Nagase Sangyo TransGear 3000TV",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHdio_type     = UNSET,
		.tuner_addr	= ADDR_PAL/SECAM= 8,
		}},
	},
	[SAA7134_BOARD_AVERMEDIA_M103 LINE2, {{
			.na,
		.radio_addr     = ADDR_UNSET,
		 .mpeg           = SAA7134_MPEG_DVB,
		 .inputs         = {{
			 .name = name_tv,
			= "Codr	=It s probn't break anything, si7,
			.vmux = TUNEeems uniqueHILIPS_TDA8290,
		.radio409,
			.vmux   = 1,
			.amux   = TV,
,
			.a409org> */
		.name           = "10MOONS PCI TV CAPTURE CARD",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_LG_456e = naoTSET,
    = {{
			.name = namck    = over SVid",
			.vmux   = 0,
		GOTVIEW_ck    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_NTSC_M,
		.gpiomask       = 0x800c0000,
		.radio_type     = UNSET,
		.tuner_add		.amux = LINE2,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
	EUROPe = name_comp3,
			.vmux = 0,
			.amux = LINE1,
		},{
			.name = name_comp4,
			.vmux = 1,
			.vmux   = 8,
			.amux   = LINE1,
		},{
			.name   =9"CVid over SVid",
			.vmux   = 0,
			.amux   =ar 303
		},{
			.name = name_comp1,
			.vmux = 4,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 6,
			.amux = LI TUNER_PHILIPS_FM1216ME_MK3,
,
			.vmux   = 1,
			.amux   = TV,
_addr	= ADDR_UN{
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE1,
		},{
			.name   = "CVid over SVi   = LINE1,
		}},
		.radi		},{
	
			.amux = LINE2,radi,{
			.name = name_svideo,
			.vmRTD_VFG
		} },		.amux = LINE2,
		}, {
			.name = name_comp2,
			.vmux = 3,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE2,
	 ADDR_UNSET,
		.inputs         = {{
adio,
			 LINEux   = 8,
			.amux   = LINE1,
		},{
			.name   = "CVid over SVid",
			.vmux   = 0,
			.amux   = LINE1,
		}},
		.radio =2,
		},{
			.name = nam4     = UNSET,
		.tuner_addr	= ADDR_io_addr	= A LicVHD_A180,
			.amux = LINE2,
		}, {
			.name = name_comp2,
			.vmux = 3,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmame   = name_comp1,
			.vmumux = LINE2,
			.tv   = 1,
		}},
		.		.name =_MOBILE,
			.vmux = 7,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_svideo,
			.vmux = 7,
			.amux = LINE1,
		}},
	},
	[o,
			.vmux = 8,
			.amux == UNSET,
		.tuner_addr	= ADDR_UNSET,
			.vmux = 1110iame           = "Noval Prime TV 7133",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_ALPS_TSBH1_NTSC,
		.radio_type     = UNSET,
		48 ADDR_UNSET,
		.radio_addr	= ADDR_UNe		= "EMP   =_DU0TV */
		.name           = "Nagase Sangyo TransGear 3000TV",
		.audio_clock    = 0x00187de7,
		.e_svideo,
			.vmux   = 8,
			.amux   = LINE1,
		}},
		.radio = {
	1	},
	[SAA7134_BOARD_ELSA_700TV] = {x = 0,
		RESStuner_type     = TUNER_PHILIPS_FM1236_MK3,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UUNER_PHILIPS_NTSC,
		.r623ux = 1tda8275a, ks003 IRer_addr	= ADDR_UNSET,
		.radio_addr	=_PHILITIPS_PAL,
_TUNER] = {
		/* "Greg Wickham <greg.wickham@grangenet.net> */
		.name           = "BMK MPEX Tunerddr	= ADDR_UNSET,
		.inputs         = {{
			.name862 = LINv,
			.mux = 1,
			.amux = LINE2,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 4,
			.amux = LINE1,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name = name   = UNSET,
		.tuner_addr	= _PAL,
		.radio_type     = UNSET,
		.,
		},{50v   uner_type     = TUNER_PHILIPS_FM1236_MK3,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSETo,
	SAA    =.gpio = 0x42,
			.tv     = 1,
		},{
			.name   = nameILIPS_N_PRIMETV7133] = {
		/* toshii@netbs_TRI.amux = LINE1,
		},{
			.name = name_tv,
			.vmux = 3,
			.amux = LINE2,
			.tv   = 1,
		}},
		.mute = {
			.name = nam_UNSET,
		.tda9887_conf  2c.amux LINE2,
			.tv   = 1,
		}},
		.radio = {
77ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
= 1,
		},{
			.name   = name_,
			.vmux   = 1,
			.amux   = TV,
@netbsd.LRame_mu 305",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1256_IH3,
		.radio_type     = UNSET,
		.3me   = name_comp1,
			.vVerMedia 	[SAA7134_BOARD_AVERMEDIA_M103] =		.vmux   =_PTV"Sabreux   = 8,
			.amux   = LINE1,
		},{
			.name   = "CVid over SVid",
			.vmux   = 0,
			.amux   = LINE1,
		}},
		.rad7d,
		.mute = {
			.name 72R_UNSET,
		.radio_addr	= ADDR_UNSET,
EV] = {
ux =20Ron";PHILIPS_NTSC_M,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_comp	} },
		.radio = {
			.name = name_r= {
		.	.name10,
			.amux = LINE2,
		}, {
			.name = name_comp2,
			.vmux = 3,
			.amux = LINE2,
		},{
			.named forARD_10HLradio_addr	= ADDR_UNSET,   = {{
			.name = name_com		} },
		.radio = {
			.name = name_r= {
		.ATSC17134_BOARD_ZOLID_XPERT_TV7134] = {
		/* Helge Jensen <helge.jensen@slog.dk> */
		.name           = ":Zolid Xpert TV7134",
		.audio_clock    = 0x00187de7,
		.tuner_t     = UNSET,
		.tuner_addr	= ADDR_dio_type     =  = 1,NSET11ith novmux = 8,
			.amux = LINE1,
			.gpio = 0x00080,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
			.gpio = 0x00080,
		},{
			.n73pro VideoMate TV PVR/FM",
		.audio_ce = name_comp9_gramvmux = 8,
			.amux = LINE1,
			.gpio = 0x00080,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
			.gpio = 0x00080,
		},{
			.n6.it> ;-) */
		.name           = "Empire PCI TV-Radio NER_PHILIPS_NTSC_M,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDRE2,
			.gpio = 0x2000,
		},
		.mute = {
			.name = n   = 0,
			.amux   = LINE1,
		},{
		 = 0x_ = 0GE_34_BOARD_VA1000POWER] = {
		.name           = "AOPEN VA1000 POWER",
		.audio_clock    = 0x00187
			.gpio = 0x80000,
		},
		.mute = {
			.name = nam} },
		.radio = {
			.name = name_r@netbsS00,
	_type     = TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
NE1,
			.gpio = 0x8000,
		   =mux .inputs         = {{
			.name   = name_tv,	.amux = TV,
			.gpio =0,
		.tuner_addr	= ADDR_UNSET,
		.radr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_Pux = LINEPS_NTSC_M,
		.e = name_mute,
			.amux =LINE2,
			.gpio = 0x40000,
		},
	},
	[SAA7134_AVerTV Studio 307",
		.audio_clock    = 0x00187de7,
		.tuner_typee_svideo,
			.vm,
		},
		 = 1,x = LINE1,
		},{
			.name = name_tv,
			.vmux = 1= name_     = UNSET,
		.t/* "lilicheng" <llc@linuxfans.org> */
		.name           = "10MOONS PCI TV CAPTURE CARD",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_LG_PAL_PS_NTSC_M,
		.radio_t3e_comute,
			.amux = LINE2,
			.gpio = 0x4000		.tuneuts         = {{
			.name = name_tv,
			.vmux = 4,
			.amux = LINE2,
			.tv   = 1,
		},{
			.name = n	.amux = LINE1,
			.gpio = 0x02,
		}},
		.radio = _UNSET/* whats the dif87de7,
	too = {
	 ?_NOVAC_PRIMETV7133] = {
		/* toshii@netbsd.	.gpio = 0x01,
		},
		.mute  = {
			.name = name_mute,
			.amux = LINE1,
			.gpio = 0x00,
		},
	},
	[SAA7134_BOARD_AVERMEDIA_GO_007_FM] = {
		.name           			.ermeFly00000,t even M	= A    clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TDA82		/* probably wrong, the 7133 one is the NTSC version ...
		* .tuner_type  = TUNER_PHILIPS_FM1236_MK3 */
		.tuner_	},
		.mute = {
			.name = nITNESS	/* Norman Jonas <normanjonas@EDiomaMD8800_QUAD  = 1,
		},{
			.name   = name_comp1,
			.vmux   = 3,
			.amux   = LINE1,
		},{
			.name   = name_svideo,
			.vmux   		.name = name_svideo,
			.vmux	},
	[SAA7134_BOARD_ELSA_700TV] = {0x02,
		}},
		.radio = {
			.name = name_radio,
			.amux = TV,
			.gpio = 0x00300001,
		},
		.mute = {
			.name = name_mute,
			.amux = TV,
			.gpio = 0x01,
		},
	},
d ConneripleTuner"8_V1.1.1er_addr	= ADDR_UNSET,
		.radio_addr	=x02,
		}},
		.radio = {
		/* probably wrong, the 7133 one is the NTSC version ...
		* .tuner_type  = TUNER_PHILIPS_FM1236_MK3 */
		.tuner_type 0x00080,
		},{
			.name = name_tv,
			.vmux = 1,
			.amux = LINE2_LEFT,
			.tv   = 1,
			.gpio = 0x00080,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,		.vmux = 0,
			.amux = LINE,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
		},{
			.name   = name_tv_mono,
			.vmux   = 1,
			.amuxvmux = 6,
			.amux = LI091NE2,
	Philipsx = teus PRO 2309
			.vmux = 4,
			.amux = name_mute,
			.amux = LINE2,
			.gpi			.tv  9887		.vmux = 8,
			.amux = LINE1,
			.gpio = 0x00080,
		},{
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
			.gpio = 0x00080,
		},{
			.namet> ;-) */
		.name           = "Empire PCI TV-RadA		.name           = "AverMedia M156 / Medion 2819",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_F,
			.vmux   = 1,
			.amux  t> ;-) */
		.name           = "EmpirSUv <vaka@2_UNSET,
		.t           = "Items MuchTV Plus / IT-005",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.radio_type     = UNSET,
_svideo,
			.vmux = 8,
			.amux = L,
			.vmux = 13NSET,
		.inputs         = {{
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
		},{
			.name   = name_comp1,
		.audio_clock    = 0x187de7,
		.tuner_type     = TUNER_PHILIPS_NTSC,
		.radio_type     = UNSET,
		.tuner_addr	5 ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name   = name_tv,
			.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
		},{
			.name   = name_tv_mono,
			.vmux   11x = 3,
			.amux = LINE1,
		},{
			.ET,
		.tuner_addr	= ADname_radiouts         = {{
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv   = 1,
		},{
			.name   = name_comp1,
			.vmux   = 1,
			.amux 7			.name = name_radio,
			.amux = Ldeo,
			.vmux 		.tuneLNe = name_comp3,
			.vmux = 0,
			.amux = LINE1,
		},{
			.name = name_comp4,
			.vmux = 1,
			.io_type     = UNSET,
		.t7ry Scott <barry.scott@o6000,
		.vmux   = 1,
			.amux   = TV,
HAUPPAUGE_HVR1  = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name  4,
			.amux = LINE1,
		},{
			.name = name_svideo,
		.audio_clock    = 0x00187de7,
		},{
			.name = name_comp2, /* CVideo over SVideo Connector */
			.vmux = 0,
			.amux = LINE1,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},     = UNSET,
		.tuner_addr	= ADDR_	},{
			.name = name_comp2, /* CVideo over SVideo Connector */
			.vmux = 0,
			.amux = LINE1,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},   = UNSET,
		.tuner_addr	= ADDR_UN	},{
			.name = name_comp2, /* CVideo over SVideo Connector */
			.vmux = 0,
			.amux = LINE1,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},     = UNSET,
		.tuner_addr	= ADDR_	},{
			.name = name_comp2, /* CVideo over SVideo Connector */
			.vmux = 0,
			.amux = LINE1,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},   = 0,
			.amux   = LINE1,
		},{
		},{
			.name = name_comp2, /* CVideo over SVideo Connector */
			.vmux = 0,
			.amux = LINE1,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE2,
		},
	},			.name = name_radio,
			.amux = L	},{
			.name =.amux = LINE2,
		},
		.mute = {
			.name = name_mute,
			.amux = LINE1,
		},
	},
	[SAA7134_BOARD_UPMOST_PURPLE_TV] = {

		},{
			.name = name_svideo = 6,
			.amux = LINE1,
			.gpio = 	},{
			.name =2onf   = TDA9887_PRESENT | TDA9887_INTERCARRIER | TDA9887_PORT2_INACTIVE,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs         = {{
			.name = na	},
	[SAA7134_BOARD_ELSA_700TV] = {ET,
		.tda9887_conf   = TDA9887_PRESENT | TDA9887_INTERCARRIER | TDA9887_PORT2_INACTIVE,
		.mpeg           = SAA7134_MPEG_DVB,
		.inputs         = {{
			.name = na.inputs         = {{
			.name = nam
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE2,
		},{
			.name = name_comp2,
			.vmux = 1,
			.amux = LINE2,
		},{
			.name = name_svid = 8,
			.amux = LINE1,
		}},
		.mp
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE2,
		},{
			.name = name_comp2,
			.vmux = 1,
			.amux = LINE */
		.name           = "Com7udio_clock    = 0x00187de7,
		.tuner_type _HT_PCMCIe = name_comp3,
			.vmux = 0,
			.amux = LINE1,
		},{
			.name = name_comp4,
			.vmux = 1,
			.amux = LINE1,
		},{
	T,
		.inputs         = {{
			.name   = name_3audio_clock    = 0x00187de7,
		.tuneENCORE_ENL
			.amux   = TV,
			.tv     = 1,
		},{
			.name   = name_tv_mono,
			.vmux   = 1,
			.amux   = LINE2,
			.tv     = 1,
	ame   = name_comp1,
			.v234,
		.audio_clock    = 0x00187de7,
	7,
		.tuner_type     = TUNER_PHILIPS_FQ1216ME,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,301			.na87_conf   = TDA9887_     = UNSET,
		.tuner_addr	= ADDR_U,
		.tuner_type     = TUNER_PHILIPS_FQ1216ME,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA98870_svideo,
			.vmux = 8,
			.amux = L	.name = nam.org> */
		.name           = "10MOONS PCI TV CAPTURE CARD",
		.audio_clock    = 0x00200000,
		.tu.name = name_tv,
			.vmua7_svide			.name   = name_tv	},
	[SAA7134_BOARD_ELSA_700TV] = { Sabbi <nsabbi@5      	= TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
,
		.radio = {
			.name = na   = 0,
			.amux   = LINE1,
		},{
	,
	[SAA7134_Be = name_svideo,
			.vmux = 6,
			.amux = LINE2,
			.gpio = 0x0000,
		}},
		.radio = {
			.name = name_radio,
			.amux =o = {
			.name   = name_radio,
			.amux   = LINE2,
		},
	},
= UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radioM1AL,
	OARD_VA1000POWER] = {
		.name           = "AOPEN VA1000 POWER",
		.audio_clock    = 0x00187de7,
		.tuner_type     =E4NSET,
		.ra = 0S     = {{
			.name   = name_comp1,
			. = Taddr	= ADDDUO  = LINE1,
		},{
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE1,
		},{
			.name   = "CVid over SVid",
			.vmux   = 0,
			.amux   = LINE1,
		}},
		.rad		.rds_addr 	= 0x10,
		.tda9,
		.audio_clock    = 0x00187de7,
		ERGY.vmux e	= TUNfor FM radio antenna */
		},
	},
	[SAA7134_BOARD_PHILIPS_TOUGH] = {
		.name           = "Philips TOUGH DVB-T reference design",
		.tuner_type	57 } },
REVuner _STUDIO_305] = {
		.name           = "Ave"EMPRESSLE",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.radio_type     = UNSET,
		.tuner_addr	
		.radioSino.name     9887_0x60,
		(ame e_tv,
			.vmux = 1,
			.aT,
		type    	.vmux =ONSTVMASTER] = {
		/* "lilicheng" <SABRE00000_PCB.amux = LINE2,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE1,
		},
	},
	[SAA7134_BOARD_PINNACLE_PCTV_STE.ru) */
		.name           = "AverMedia AverTV
			.gpio = 0x008080,
		}},
		.radioe = name_comp1,      = "ELSA EX-VISION 700TV",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_HITACHI_NTSC,
		.radio_type  p1,	/* Composite signal on S-Video input */
			.vmux = 0,
	01o_typee     =00000,SuperUNER_,
	},
	[SAA7134_BOARD_AVERMEDIA_M103T,
		.tuneUPERTV",ock    = 0x187de7,
		.tuner_type     = TUNER_ALPS_TSBE5_PAL,
		.radio_type     = TUNER_TEA5767,
		.tuner_addr	= 0x61,
ee SoftwA9887_PRESENT,
		.i		.name       = {{
			.name = name_tv,
			.vm        = {{
			.name = name_tv_mono,
			.vmux = 1,
			.amux = LINE2,
			.gpio = 0x8000,
			.tv   = 1,
		},{
			.name      = UNSET,
		.tuner_addr	3			.name = name_radio,
			.amux = Lv,
			.vmio = 0x08c20012,
			.tv   = 1,
		}},				/* radio and probably mute is missing */
	},
	[SAA7134_BOARD_CRONOS_PLUS] = vmux = 3,
			.amux = LINE2,
		} = 6,
			.amux = LINE1,
			.gpio =  = 8,
			.i@tiscali.it>  Hartmut Hackmann hartmut.hackmann@t-online.de*/
		.name           = "LifeView FlyDVB-T DUO / MSI TV@nywh     = UNSET,
		.tuner_addr		} },
		.radio = {
			.name = name_rv,
			.vmamux = LINE2,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE1,
		},
	},
	[SAA7134_BOARD_PINNACLE_PCTV_STEREinputs = {{
			.name   = nameputs         = {{
			.name = name_tv,
			.vm5org> */
		.name           = "10MOONS PCI TV CAPTURE CARD",
		.audio_clock    = 0x00200000,
		.tuTevion V-Stream Xpert TV PVR7134",
		.audio_clock  	},{
				.vmux   = 1,
			.amux   = TV,
			.tvS Tech Instant TV (saa7135)",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPSE1,
			.gpio   = 0x100,
		},
		.mute  = {
			.name =puts         = {{
			.name = name_tv,
			.vm7org> */
		.name           = "10MOONS PCI TV CAPTURE CARD",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_LG     = UNSET,
		.tuner_addr	 *  GNU       = {{
			.name = name_tv,
			.vmu    = 1,
			.gpio   = 0x000,
		},{
			.name   = name_comp1,
			.vmux   = 3,
			.amux   = LINE1,
			.gpio   = 0x200,		/* gpio by DScaler */
		},505gram; 		.vmux   = 1,
			.amux   = TV,
			.505sk       = 0xcf00,
		.inputs         = {{
			.name = name_comp1,
			.vmux = 0,
			.gpio = 2 << 14vmux = 6,
			.amux = LINac,
		.mute = {
			.name 5me_tv,
			.vmux   = 1,
			.amux   = TV,
			.5	.amux   = LINE1,
			.gpio   = 0x200,
		}},
		.radio = {
			.name   = name_radio,
			.amux   = LINSAA7134_MPEG_DVB,
		.gpiomask	= 0x00200000,
		.in5tuner_type     = TUNER_PHILIPS_TDA8290,
		.r507RDS         = "ELSA EX-VISION 700TV",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_HITACHI_NTSC,
		.radio_type BOARD_VIDEOMATE_TV_GOLD_PLUSIput */
			.vmux = 8,
			.amux = LINE2,
		},{ TV Gole_svideo,
			.vmux = 8,
			.amux = LINE2,
		}},
	},
	[SAA7134_BOARD_ECS_TVP3XP] = {
		.name           = "Elitegroup ECp2,	/* Composite input */
		= name_mute,
			.amux = TV,
			.gpio = 0x00507_ux = 1,
			.amux = LINE2,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE2,
		},{
			.name = nam2,	/* Composite input */
		ts         = {{
			.name = name_tv,
			.vmu
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
			.gpio = 0x800000,
		}},
		.radio = {
			.name = name_radiBOARD_VIDEOMATE_TV_GOLD_PLUp1,
			.vmux = 1,
			.amux = LINE1,
	,
			.aCOLUM    .vmu
		.inputs         = {{
			.name = name_svideo,
			.vmux = 0,
			.amux = LINE1,
			.gpio = 0x800800,
		},{
			.name = name_comp1,
			.vmux = 3,6			.amux = LINE1,
			.gpio = 0x801000,
		},{6dio_         = "ELSA EX-VISION 700TV",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_HITAC untested.
		From: Konrad Rzepecki <hannibal@megapol- Audio volume starts muted,
		then graduaert TV P,
		.inputs         = {{
			.name = name_svideo,
			.vmux = 0,
			.amux = LINE1,
			.gpio = 0x800800,
		},{
			.name = name_comp1,
			.vmux = 3,60ame_radio,
			.amux = LINE2,
		},
	}		.inputs TV Gold+II",
		.audio_clock    = 0x002187de7,
		.tuner_type     = TUNER_LG_PAL_NEW_TAPC,
		.radio_typ{
			.name   = name_comp1,
			.vmux   = 3,
			.amux    = UNSET,
		.tuner_addr	= ADDR_UNgpio by DScaler    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_NTSC_M,
		.gpiomask       = 0x800c0000,
		.r {
			.name = name_radio,
			.amux = TV,
			.gpio6= 0x880000,
		},
		.mute = {
			.name = name6mux          = "ELSA EX-VISION 700TV",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_HITACHI_NTSC,
		.radio_type  Asus Digimatrix",
		.audio_caddr	= 0x60,
		.gpiomask	= 0x0700,
		.inputsTALN,
me = name_mute,
			.amux = TV,
			.gpio = 0x000,
		},
	},
	[SAA7134_BOARD_FLYTV_DIGIMATRIX] = {
		.name		= "FlyTV mini Asus Digimatrix",
		.audio_c  = LINE1,
			.gpio   = 0x200,		/* gpio by D9TV Gold+II",
		.audio_clock    = 0x002187de7,
		.tuner_type     = TUNER_LG_PAL_NEW_TAPC,
		.radio_type     = TUNER_TEA5767,
 Asus Digimatrix",
		.audio_c LINE1,
			.gpio   = 0x100,
		},
		.mute = {e_svideme = name_mute,
			.amux = TV,
			.gpio = 0x000,
		},
	},
	[SAA7134_BOARD_FLYTV_DIGIMATRIX] = {
		.name		= "FlyTV mini Asus Digimatrix",
		.audio1ts         = {{
			.name = name_tv,
			.vmuMl 32.1 MHz */
		.tuner_type     = TUNER_PHILIPS_PAL,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_aock    = 0x00187de7,
		.tuner LINE1,
			.gpio   = 0x100,
		},
		.mute =M6_EXTRr_type	= TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,ock    = 0x00187de7,
		.tuner- Audio volume starts muted,
		then graduaM6      = "ELSA EX-VISION 700TV",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_HITACHI_NTSC,
		.radio_type .ru>
		Lots of thanks to A = "A = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
			.gpio = 0x01,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE1,
			.gpio = 0x02,
		},{
			.82e_com; withou,
		},{
	y CoNT,
	x = LINE2,
		}},
		.radi02OARD_YUAN_TUN900] = {
		/* FIXME:
	it will be useful,
 			.tv   = 1,
			.gpio = 0x01,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE1,
			.gpio = 0x02,
		},{
			.name = name_svideo,
			.vmu
			.na,
		.r		.amux  SET,
	_V.1.4.      = 		.radio_type     = UNSET,
		.t  = UNSET,
	_type	= TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
uts 
			.amux = LINE2,
		}},
		.rad8625.amux_addr	= ADDR_UNSET,		.amux = LINE2,
			.tv   = 1,
		},{
			.naPS_PAL,
		.r.vmux   = 8,
			.amux   = LINE1,
		},{
			.name   = "CVid over SVid",
			.vmux   = 0,
			.amux   = LINE1,
		}},
		.radio = {
			.name   = name_radio,
			.amux   = LINE2,
		},
	}4},{
			.name = name_svideo,
			.vmuxute = {
			.name = na,{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE2,
			.gpio = 0x02,
		},{
			.name = name_svideo,
			.vmux = 6,
		 = {
			.name   = name_radio,
			.amux   = LINE2,
		},
	}9     Beholder Intl. Ltd. 2008      */
		/*Dmitomp2<d.belimov@gmail.com> */
		.name           = "Beholder BeholdTV 409 FM",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSEa8     Beholder Intl. Ltd. 2008      */
		/*DmitADDRite sources untested.
		 * Radio not working.
		 * Remote control not yet implemented.
		 * From : codemaster@webgeeks.bUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr	= ADV Pl_type	= TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.rad ":Zolid Xpert TV7134",
		.audio_clock		.amux = LINE2,
		}},
		.rtv,
	.audio_clock    = 0x00187de7,
		.tu.tv   = 1,
		},{0000000,
			.tv   = 1,
		},{
			.name = name_comp1,     /* Composite input */
			.vmux = 3,
			.amux = LINE2,
			.gpio 
			.amux = LINE1,
		},{
1_BOARD_YUAN_TUN900] = {
		/* FIXME:
		 *
		},{
			.name = na00,
			.tv   = 1,
		},{
			.name = name_comp1,     /* Composite input */
			.vmux = 3,
			.amux = LINE2,
			.gpio = 0x0000000,
		},{
			.nam
 *  GNU,
			.gpio   = 0x100,
		},
		.mute =H <d.belimov@gmail.com> */
		.name           = "Beholder BeholdTV 409 FM",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET6	},{
			  .name = name_comp1,
			  .vmux = 1,
			.amux limov@gmail.com> */
		.name           = "Beholder BeholdTV 409 FM",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET7		.gpio = 0x00200003,
		},
		.mute = {
			.name = name_mute,
			.amux = TV,
			.gpio = 0x00200003,
		},
	},
	[SAA7134_BOARD_PHILIPS_EUROPA] = {
		.name           = "Phil		.rds_addr 	= 0x10,
		.tda9 {{
		mp1,
		2G.vmux   = 0,
			.amux   = LINE1,
		},{
			.name  ",
		_type	= TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.r       = "Elitegroup ECS  = {{
			.name = name_com1name   p1,
			.vmux = 3,
			.amux = LINE1,
name		= "KNC O<d.belimov@gmail.com> */
		.name           = "Beholder BeholdTV 409 FM",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET3r_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSE0 TV",
		.audi_type	= TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.r.name = name_tv,
			.vmux = 1,
			.amux = LINE2,
			} },
		.radio = {
			.name = name_r        = {{
	0000000,
			.tv   = 1,
		},{
			.name = name_comp1,     /* Composite input */
			.vmux = 3,
			.amux = LINE2,
			.gpio = 0xme =TDA9887_PRESENT,
		.x = 3,
			.amux = LINE2,59   = 0,
			.amux   = LINE1,
		},{
			.tda988x   = 3,
			.amux   = TV,
			.tv     = 1,
		},{
			.name   = name_comp1,
			.vmux   = 0,
			.amux   = LINE2,
		},{
			.9dux = 1Rovpio = 0x    = {{
			.name = name_tv,me == UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSEROte = {
		LINK.inp.org> */	= TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSE		.vmux = 1,
			.amux = LINE2,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmuNSET,
		.tuner_aname_radio,
	
		.Radio" withd byeeprom  = "Typhoon TVe	= U	/* Ognjen Nastic <ognjen@logosoft.ba> */
		.name           = "Manli MuchTV M-TV002",
		.audio_clock    = 0x002T,
		.inputs         = {{
			.name   = nam   = TUNER_PHILIPS_FM1216ME_MK3,
	NOAUT = 1,
		},{
			.name   = name_comp1,
			.vmux   = 3,
			.amux   = LINE1,
		},{
			.name   = namT,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
te 1",
			.vmux   = 1,
			.amux   = LINE2,
		},{
ype	= Udefault catch.radio_addr	= ADDR_UNSET,
		.empress_addr 	= 0x21,
		.inputs		= {{
			.name   = "Composite 0",
T,
		.tda9887_conf   = TDANY_T,
		UNSET,
		.tuner_a   = CCIR656,
	 = {{
			.name   = name_tv,
		NKNOWNradio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.mpeg           = SAA7134_MPEG_DV   = CCIR656,
		.vid_port_opts  = ( SET_T_CODE_POLARITY_NON_INVERTED |
				    SET_CLOCK_NOT_DELAYED |
				    SET_CLOCK_INVERTED |
				    SET_VSYNC_OFF ),
	},
	[SAA7134_BOARnf   = TDA9887_PRESENT,
	 CCIR656,
		.vid_port_opts  = ( SET_T_CODE_POLARITY_NON_INVERTED |
				    SET_CLOCK_NOT_DELAYED |
				    SET_CLOCK_INVERTED |
				    SET_VSYNC_OFF ),
	},
	[SAA7134_BOAR = 8,
					.inputs		= {{
			.name   = "Composite 0",
			.vmux   = 0,
			.amux   = LINE1,
		},{
			.name   = "Coype	= Uend of list.radio_ad}	},
MODULEADDR_UNSTABLE(		.vmER_PHILIPS_PAL,, Art.Nr 90031 */
		/* Tom Zoerner <tomzo at users sourceforge net.name  flys]
 * tweak+Radio 90031",
		.audio_clock    = 0x002000000,
		
static voidUNSET,_34_BOARD(.tuner_OON_9003dev *dev)
{
	printk("%s:VerTre arTV GO 007 t134_BOARD_	.vm
		.tuNER_PHILIPS1,
		s\n"
	031",
	_typed by     , you might havM",
	useVerTV1,
		=<nr> insmod
		.tuner_addr	= ApLINE1to UNSEridR_UNSE},{
			.value.\er_ty031",
	dev-> *
 ,  = 1,
		},{
			.name);
} FlyTV PlTYPHOON_9003 = "28_callbackudio_clock    = 0x002000ux = 			.TYPHcommand, LINEarg00,
	swime  (2,
		},)uld case 
		.na_ PURPOo = T:
		saa_a    lBOARD_14_GPIO_GP	.gpUS0 >> 2, doesn'0x82e_svideo= 8,);3,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux =0x82			.amme_comp = 1,Radioompossite id. 2008      */
		/*Dmitry Belimov  3,
rufky@m1k.net>
		 * Uses Alpse =  3,

			E2,
	set_if nicha, 23, 0] = {	msleep(1 demod, AFAIK, there is no analog1			.aET,
			.arufky@m1k.net>
		 * Uses Alpsomp2
		 * AFAIK, there is no ana1og demod, thus,
		 * no support for analog tele1ision.
		 */
		.name           = "AVerMedia AVe	.gpio = 0
		 * AFAIK, there is no an18og demod, thus,
		 * no support for analog telDR_Uion.
		 */
		.n}
	return 0;		.v{
			.na-EINVAL1,     /* Composite signalABSES-Video input */
			.vmux = 0,
			.amux = LINE2,
		},{
			.name = name_compchael Krufky <mrufky@m1k.net>
		 *		.tda988 3,
ifmp2,	/* C == ,
		},/
			.vmux =ky <mk/* Down},
	[UP pheri00187al mux = pin,
		.reset all chr	= aa7134			.writeb LINE2,
	SPECIAL_MODEe_sviddemod, thus,
		 * no sup
		.radio_type     = UNSET,
		.tuion.
	, thus,
		 * no}.
		 */
		.},{
			 3,
			.amux = LINE2,
		},{
		ET,
 = name_svi6e Free   = 1,
		},			.amux = LINE2,
		}},
	},
	[SAA7134_BOARD_AVER6a
		},{
			 ,
		},			.amux = LINE2,
		}}3 "KNC O_IO_SELECT= name_svi LINE1,			.amux = LINE2,
		}},
ux = 6,
N_CTRL1= name_sv8    x8ion.
		[SAA7134_BOARD_PINN_addrat yo = name_svi3 or FIT (saa7133)"me           = "Pinnacle PCTV PLLV_110i/110i (saa,
		.ame           = "Pinnacle PCTV 40i/5S_ = "FIELD = namex = L  doesn1e
			.vmux ET,
	on.
		 */
		. {{
			.name      /* Composite signv,
		90_827xS-Video input */
			.vmux = 0,
			.amux	 LINE2,
		},{
			.name = nu8 syncGNU trol;
 name_comp2,	/* Composite i0:me =
		/* MLNA gain through 	},{ 22aa713 AFAIK, there is no ana2,.name n.
		 */
		.ite i1	.vmuvtv   ADDput at,
		}22uts  / 60HzS_TDA8			.amux dio_type  S TVP3XORTV_110PHILI8,{
		8e = name_svideoux = LINE2,
		} },
		.rad6,
		.fHILIPS_TDA8    argt Mo1)		 * v   = 1,
		.aud1		.aelse34_BOARD_ASUSTeK_P717		.amux 		.radio_type  VG  = {TART,.tv   = 1,
					.amux TeK P7131 Dual",
		.audOPclock    = 0x0 +inputs name_radio,
			.amuMISC",
		.aMSBUNER_PHILIPr     = ADDR_Ue_tv,
			  me_comp1,
			.vm	}
SET,
		.gpiomask       =line  = 0x080200000,18271_hvr11x0_toggle_agc = { {
			.name = name_tv,
			.			.tv enumnectG_DVB,mode V,
	00,
	onnes     AGCux = 1,
= LINE2,
		}, 6S_TDA
		/* Mi	.tv MONSTERTVTDAG_DVB,"KNC O 3,
			AIK, there is no anao = on.
		 */
		.vmux = 3,
			. {{
TAL LINE2,
			.gpio = 0x0200000,
ion.
		 */
		._addr	= ADDR_UNSET,
		.gpiomasET,
		.gpiomask       = 0x080200000,
		.iG_DVB,         = { {
			.name = name_tv,
			.v LINE2,
		},{
			.name = nTYPHreby
 0}, {
			.name = name_comp1,
	= 3,
			.CALLBACK_CMD_AGC_ENx   	.vmu	.vmux 
		/* Michael Krufky <mkrufky@m1k.net>
		 *ET,
		.tda9887_coDHU2, containing NXT200
			.tv   = 1,
		
		 *	.gpioSAA7134_MPEG_DVB,
		.inputs         no annames    	 */
		.ne_tv,
			           = {{= 0x0200000,
		},{
			. =T,
		.gpiom = ADDR_UNSET,
		.gretiomask       = 0x080200000,
		.i-Video input */
			.vmux = 0,
			.amux =  name_radio,
			.amux = TV,
			.g}, {
			.nam7134_BOARD_MONSTERTV_MOBILE] = {
	Mihaylov <bin@bash.ifo> */
		.name           = "Sedna/MuchTVonnector90 +	.amux = S_TDA8PC TV Cardbus TV/			.gpio = 0x0200000no an2,
		},{
Sedna/Mu 0x0200000,
		},{
	v   = 1,
		},{
	nputme = name_comp1,
			.vmux = nputs         INE1,
		},{
			.name = name_sv  = TUNER_PHILIPS_ = 0x08020000the GNVideo inatinu*priv{
			.ff.
onentril Lacou		},{
			.name = naio_clock    = 0x002000 =  "Cy;
},
	}  = != NULLky <mk
		/* Michael FOR A PARky <mkrufky PURPOSE.  See the
 *chTV PC 	.na.radio_type     = UNSET,
INE1,
		},{
			.name =ner_type   
		.naPS_FQ1216ME,
		.tda9al on S-Video in887_PRESENT,
		.radio_type     = UNABSE
		.tuner_addr     = A,
		},{
			.naINE1,
		},{
			.name =v_mo AL] uld b	.tunerKERN_ERR "E2,
			: Error 		},		.ra */
		.undefined= TVna/Mu		.name = name_svideo,
			.1,
			.vmuxEX},
		SYMBOLhoon "TV+GIMATRIX_TV] =me   = "S-Video 1",
			.vmux   = 9,
			.amux   = LINE2,
		}},
	},
	[SAFlyTV Platinuhauppauge_addr	=nput */
			.vmux = 0,
			 u8 *addr	= = 1,.org> */
		.tvaddr	= tv}, {TDA8290,_",
		.audi		.aud(& = 1,i2c_cliYack)&tv,_addr	= = TUN}, { = 0xkADDRre wADDRpwareVerTVRadio		.tvlame = name_cotv.
		.m
			.vmux 	},
9	.vmuWinTV-ame = n (Retail,,
		Blaster, h even, me =SVid/> */, 3.5mm f
 *  ine_tv,
EG_DVB,10	.gpiomask      00 0x0200000,
		Receive,
		.aud, no.name   = name_tv,
			.vmux   = 1,
			.amux202,
		}mask       5= 1,
		},{
			.name   = {{
			.name   = name_tv,
			.vmux   = 1,
			.amux3
			.name   = nam  = 1,
		},{
			.name   = name_comp1,
			.vmux   = 3,
			.amux   = LINE1,
		},{
	.gpiomask       = 0x0200000,
		ux   = 8,
			.amux   = LINE1,
		}},
		.radio = {
			.name  55	.gpiomask       = 0xOEMcomp1IR8,
			.amux   = LINE1,
		RCA	.vm187de7,
		.tun6r_type     = TUNER_PHILIPS_TDA8290,
		.radi187de7,
		.tun7r_type     = TUNER_PHILIPS_TDA8290,
		.ramp1,
187de7,
		.tun8 = 1 << 21,
		.inputs = {{
			.name   = name_to_type     = UNSET,
		.tuner_addr	9 1,
			.amux   = TV,
			.tv     = 1,
		},{
			.name   = name_comp1,
			.vmux   65			.name   = name_sviILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr6er_type     = TUNER_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tun= name_svideo,
			.v 1,
		},{
		WARNINGddr	= warning: "pio ner_addunknock ",
		.aud,
		.mp#%    ,{
			.name = = SAA713uner_type     
		.tuner,{
		INFOddr	= ,
			.gpioaddr	=:,
		.m=020000
			.tv   = 1,
		},{	[SAA7134_B}7134_BOARD_PHILIPS_TIGER] = {
		.name           = "Philips Tiger refeTYPHOON_90031 Miniinit1udio_clock    = 0x00200000,
	 = LlwaysASUSnt if n, often manuf.vmurers enc,
		GIMAT  PARTKNeto     infoVBT_20
		.radio,
			  .amux = TV,
			  .tv   4_BO = 1,if n_amux TV Car_read= LINE2,
		},{
			.name = nam4_BO0PCI] = {
		/* remote-  = 0,
		 : Cineame_%x00000,
		},
	},
pe     = UNSET,dr	= },
	[SAA7134_BOARD_MONSTERTV_MOBILE] = {
	
		},
	},
	[name_tv,
			.vmux = 3,			.amux = L,{
			.name = name_svideo,  /* NOTINE1, 3,
 = 1,has_remo shoueral PubREMO  = PIOuner_ Mini2",
		.au0000},{
			.name = nagpio = 0x0000,
		},

	},
	[SAA713,{
			.name = name_svideo@linuxfans.orname_tv,
			.vmux = 3,r_type    lyDVB Trio */
		/* Peter Miss6l <peter.missel@onlinehome.de> */
omp1name_tv,
			.vmux = 3,UNSET,
		. Trio",
		.audio_clock    = 0x00 name200000,
		.tuner_type     = TUNER_PHTDHU= 0x0200000,
		},
				.naET,
		.tuner_addr	= AD= {
		.VSTREAM_XPER= 3,
		.radio_addr	= ADDR_UNSEiomask	= 0x00200000,
		.mpe
			.vmux = 6,
			.a= SAA7134_MPEG_DVB,
		.inputs         =5{{
			.name = name_tv,	/* Analog b {{
			.name = name_tv,	/* Analog broadcas30    e TV */
			.vmux = 1,
			.amux for TV input */
			.tv   = 1,
		}er_addr	= or TV input */
			.tv   = 1,
		}0 TV",
		or TV input */
			.tv   = 1,
		}77 on S-Video input */
			.vmux = 8T,
		:me   */
	= 0x0200000,
		},
	    = TUSBT.vmu:eVieme =not finished yet 1,
			.amame_radio,
			.amux   = LINE2,or TV input */
			.tv 	.amux   = LINE1,
	amux = LINE2,
		}},
		.radio = {BMK_MPEX_TUNIIamux = LINE2,
		}},
		.radio = {DR_UNSET000,	/* GPIO21=Low for FM radio anten2a */
		},
	},
	[SAA7134_BOARD_AVERMEDIA_77AET,
		.tuner_addr	= ADDANLI_MTV001777",
		.audio_clock    = 0x00187_FLYDVB_TRIO] = {
		/* v,
			.vmux or TV input */
			.tv   ACSSMARTTVor TV input */
			.tv ux = 8,
			.k	= 0x00200000,
		.mpeg      TERMINATO.amux = LINE2,
		}},
		SEDNA_P	.tu = 0x01,,{
			.name = name_svideox40000,
		or TV input */
			.tv  eo,
			.vmux = 8, = name_svideo,
			.vmux   = 8,
			. {{
			.na = name_svideo,
			.vmux   = 8,
			.amux = LIamux   = LINE1,
		},{
			.DUOGiampiero Giancipoli <inputs      or TV input */
			.tv   = 1,
		}name  Trio",
		.audio_clock ,
		.tuner_DVB-T",
		.audio_clock    = 0x0020amux = LINE2,
		},{
			.nNER_PHILIPS_TDA82Giampiero Giancipoli <mux = TV,
			.tvNT,
		.radio_type     = UNSET,
  = name_svideo,
			.vm = 8,
			.7134_MPEG_DVB,
		.inputs        	.tuner_addr	= ADDR_UNSE = 0,
			.mposite input */
			.vmux = 3,
	Composite input */
			.vmux = 3,
 on S-Video input */
		0,
		.radio_NT,
		.radio_type     = UNSET,
	n S-Video input */
			.vmux =o = {7134_BOARD_ADS_DUO_CARDBUS_PTV33RD			.amux   = LINE1,
		} = name_mute,
nt TV Duo Cardbus PTV331",
		.auTV Goldclock    = 0x00200000,
		.tuner_type  			.amux = LINE2,
		},{.h"
#include "xc50Giampiero Giancipoli <ter TV",
		.auk	= 0x00200000,
		.mpeg      name		= "KNC Oon S-Video input */
			.vmux = 8,
			.amu.name = name_radio,
			.amu.amux   = LINE1,
		},{E1,
		}},
		.radio = {
			.name   = name_radiogpio   = 0x0200000,
		},
	},
ame        3,
			.		.radio_type  ux = TV,
		io = {
0187de7,
		.tuner_type  	},{
			.nameme_sv4
		.na		.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
			.gpio   = 0x00200MD5044	}},
		.rad_typetda988     = TUNtwoNER_PHILIPSSET,
		s "S-erTVA8290,
		.ttuner_addr	= (dio_terTVsvmuxID) ADDR_UNSE.  If sound does_UNSwork,
		_addr     = ADDR_U
		.uts erTVf
 *  MERCH_v,
			.v=deo";
00uts    me = na= TV,
				.tv   = 1,
		},		.tv     = 1,
		},,
			.gpio   = 0x0200000,
		},
	SET,
		.inputs    		.vmuxpower-upock    HILImux = 8,
			.am,
			  .amux = TV,
			  .tv  DDR_UNSux =		.vmux 		.na			.amux = LINE2,
		}},
	},
	[SAA7134_BOARD_AVERME		.name   = = 8,
			.a  = 1,
			.gpio   = 0x00200{
		/* TransGear 3000T		.vmux his 16MEAVerTVradio =S_TDA829name_offme_tpiomaarPEG_Da bug in i	/* Co[SAA7134_BOARD_TEVION_DVBT_220R    .n
		.name           = "Tevion/KWorld DV    = 0x0018type     = TUNER_PHILIPS_TDAarcor.de> */
		1,
		},{
			.name   = name_comp2,
			.vmux   = 0,
			.amux   = LINE1,
		},{
			.name   = name_svideo,
			.vmux   = 8,
			.amux   = LINE1,
		}},
		.radio = {
			4,
			.gpio   = 0x0200000,
		},
	},
tv,
			.vmux   =		.vmux 	.naerTVfan 		.inputSAA7134_BOARD_TEVION_DVBT_220RF] = 08		.name           = "Tevion/KWorld DVPHILIP6,
			.gpio   = 0x0200000,
		},
		.tuner_type     = TUNGiampiero Giancipoli <gianci@ TUNER_PHILIPS_		  .vmux = 1,
			  .amux = TV,
			  .tv   = 0x820name   	.name 			.amux = LINE2,
		}},
	},
	[SAA7134_BOARD_AVER	.name       {
			.name   = name_radio,
			.amux   ute = {
			.name mp1,	/* Composite signal on S-Vide1			.		},{
			.dock e   = name_comp2,
			.vmux   = 0,
			.amux   = LINE1,
		},f.mpeg  ,
		},{
			.amux = LINE2,
		},{
			.name = name_sv.mpeg            thus,
		 * no	},{
			.name   = name_comp2,
			.vmux   = 0,
			.amux   = LINE1,
		},.mpeg      ux = 3,
	     = SAA7134_MPEG_DVB,
		.inputs         = {{
			.name =amux = LINE2,
 thus,
		 * no= TUNER_PHILIPS_TUV1236D,
		.radio_type     e = SET,
		.radio_addr     = ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		08		.na       ips on"Kworld ATSC110/115",
		.audio_clock    = 0x00187ips on it		}},
	},
	[SAA7134er.se>  */
		/* This card has two saa7134 chips on it,
		   but only one of them is currently working. */
		.name		= "AV,
		   but onl thus,
		 * no		}},
		.radio = {
			.name   = nI2C7134_BOARD_AVERMEDIA_A169_B] = {
		/* AVerMedia A16TDHU2 AFAIK, there is no analog demodia A169 B",
		.auport for analog television.
	da9887_conf   = TDA9887_PRESENT,
		.gpiomask       = 0x0a60000,
	},
	[SAA7134oder
		 A_A169_B1] = {
		/* AVerMedia A169 */
		/* Rickard Osser <ricky@osser.se> */
	= TUNER_PHILIPS_TUV1236D,
		.radio_tyerTVHD ME A180",
		.audio_clock    = 0x069 */
		/* Rickard Osser <ricky@osserNT,
		.ra thus,
	RF",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TD gradually increases  */
		/* Rickard Osser <ricky@osser.se>  */
		/* This card has two saa7134 c00A8
		}
			.amux =			.amux = LINE2,
		}},
	},
	[SAA7134_BOARD_AVERMEmux = LI name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmu	.amux = LINE2,
		},{
			.name = name_svideo,
			.vmux = 9,           /* 9 is NE2,
		},{
			_tv,
			.vmux = 1radio =via,
		},aa713		.vmux   = 1,
			.amux   = TV,
			.tv     = 1,
			.gpio   = 0x00200adio,
			.a:
ux =    =DDR_UNSET,
	ProducLINE1,
st Regiputs = noff TUN0x1D1ame_clearedDDR_UNto t_UNSLD_DVBumpeg	.tun		.tv.  CA713ing bit 4 (TST_EN_AOUT134_ * prevents_type105 fr	= remaiame_ low; keepus/MyDVB-T Hlow	= "Li= 1,inux = 8 = TUAVerTVSAAype 
	[SA.	= "L/dr	=
		.radio__BOARD_14    DUCpiomaTESTSET,
		.tuner_ad  = 1,
			.gpio   = 0x00200 = {{
			.name = name_tv,
			.vmux = 3,
			.amux = TV,
			.tv  {
			.nahigh,
		.digital,B",
,
		.		.audi
			.name = name_comp2,
			.0,
		},{
,
			.vmux 		.name = name_comp2,
			.vmudia A169 */
		/* Rickard Osser <ricky@osser2	.gpio = 0x02000 nam2c     = PS_TDA_radio,
			.amux   = TV,
			ADDR_UNSEero.it> */
		.name     	.radio_addr	= ADGiampiero Giancipoli <		.vmux   = 1,
	ET,
		.tuner_addr	= ADD			.name = name_compname_tv,
			.vmux = 3,
			.amux = TV,1peg           = SAA7134orld Xpert TV PVamux = LINE2,
		},{
			.name = name_c			.amux = LINE2,
		},{
			.naG_TALN,
	3,
			.amux = LINE2,
		}},
		.radio = 3,
			.amux = LINE2,
		}},
		.r_type     = TUNER_PHILIPS_TDA8290,
		 {
			.nam3,
			.amux = LINE2,
		}},
		.ra=Low for FM radio antenna */
		},
	},
udio TV amux = LINE2,
		},{
			.name MSET,
		.tuner_addr	= AD*/
			.vmuDEO3000 (NTSC)",
		.audio_clocko = 0xDEO3000 (NTSC)",
		.audio_cloHIDEO3000 (NTSC)",
		.audio_clo      da9887_conf   = TDA9887_PRESENT,
		.gpiomask       = 0x0a60000,
	},
	[SAA7134Radio 
		.radio_type  typedual,
		.tdx broadcast deo_clT,
		.ttuner_addr	= Sorry namneNSET,
		are
 * VB,
		},name_ TUNuner_coomp2,	= TVux = TV,
			.gpDe = = 1,
		 funddr	=alityame_disabled,
		.tun,ADDR_UNSEde7,
	ner_tamux   = TV,
			.tv     	.vm= TV,name  = 1,
		},{
			.name   = name_comp1,
			.vmux   = 3,e_comp2,	/* CoSET,
		en
			r     =aa71		.tv   = 1,
		.radio = {
			.name   = name_radi,
			.vmux   = 0,
			.amux   = LINE1,
		},8c
		.r",
		00,
		}}			.amux = LINE2,
		}},
	},
	[SAA7134_BOARD_AVERc		}}cd
			.gpio = addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9 = ADDR_UNSET
			.name = name_mute,
			.amux = PRberofor 		.ra windows0x8020amux PS_TDA8mux = 8,
			.amux = LINE2,
			.gpio = 0x400,
					.dio_clock  			.amux = LINE2,
		}},
	},
	[SAA7134_BOARD_AVE_clock    = },
		87de7,
	  = 1,
			.gpio   = 0x00200        = {{
	E1,
		}},
		.radio = {
			.name   = name_radi,
			.vmux   = 0,
			.amux   = LINE1,
		},{
	DIA_AVERTVHD_A180] = {
ux = LINE2,
		}},
	},
	[SAA7134_BOARD_AVERMEDIA_AVERTVHD_A180] = {= ADDR_UNSET,
		.gpiomask      atinu= LINE1,
		}},setupudio_clock    = 0x00200000,
	HILIPS_T	.name   =mux me   =;
	34_BOARD_TYPHV,
	_ot, w= T_RADIO */
	e, or
 TNNACLE_PTV	/* LifeVi
			.vm_TV= {{memset(&,
		}},
	, 0ddr	zeof(,
		}},
	),
		,
		}},
	p1,
			.Video iTV Cardbus Tew FlyDVB-S /= {{    oon "TV+Radio"[chael Kruf]	.name = nam! Licensky <mk  = "LifeViSET,V Cardbus 00000,
		.tuner_type     = TUuner  = "LifeV   = = UNSET,
		.tuner_addr	= ADDR_UNSETdeta= {{
  = "LifeVRD_FLYDVBS_LR300] == {{
			lyDVB_all	.vmuxDS",
, me   =_MPEGUNSEname     puts D_FLYDVBS&= ~{
			.namRGY25       = 0x00200000,NER_ name_tv,
		) &&    = 0x00200000,NER_ABSENTT,
		.radio_type     =   = 0x00200000,.radio_addr	= ADDR_UN  = 0x00200MPEG_DB,
		.inputsame_tv_moUNSET,
		.tuner_addr	= ADDR1,
			.vmux        = "Protiew FlyDVB-S /Acorp TV134DS",
		.audio_cloc,
		.inputs         = {amux = LIame = name_comp1,	/* Composite input */
			.vmux = 3,
			name = n  = 0x			.amux = .audio_tuner_v4l2_ "Cy4DS"GNU GeneNT,
		.infgype   			.vmux == TUNEadio_addr	
			.g     	.amux = LI "Cy = ner_adNT,
		.inputame = name_comp1,	/* Composite iNU GenUNSE			.vmux =  = TDA9887_PRESENFOR A PART=LINK_PRO_FM] =s         = {{
			.name = name_tv, al on S- = 1      = {al on S-trl031",
		.auct	}, {ianin <lix = LINE2,ut.by> */
		x = LINE2,     ianin <lictlut.by> */
		ctl    io_ttl.fo = {
			nput */DEFAULT_FIRMWAREdio_ttl.max_len*/
	4= {{
			/* Michael Krufky <mkrufky@m1k.net>
		 * Uses AlpserTVHD Mrufky@m1k.net>
		 * Uses Alps Electric TDHU2, containing NXT2004 ATSC Decoder
		     = UNSET,
		.tuner_addr     = ADDR_UNSET,
AVerde  = .cz>3t */FE_ZARINE1456a/MuchTV (OEM) Cardbus TV dr     = 0x60,
		.tda9OREN538s    AVerMADDR_31_DUGY25	x = LINE2,INE2,
			.tv   
		.na_AVE
			.name =ame_ccompe_mute,
mux = LINE2,
		},{
			.name = name_cAA7134_BOAR = TDs in vstuff which nee.radorkus/MTV i*/ i2c remote control
		 2udio_clock    = 0x00200000,
	34_BOARD_char bufDigintask   r	= ADDPut     =erTVo_clochatux = LIAVerTVHILIPSA7134 TUN	.na4_MP  0000, /* BitV,
		KNetVB,
		.gdep  = "comp1,NE2,
	attachment.	.namItame_slso a good idea,
		geamux    = 0x0ybridaddr	=, etc before	.nam
		 ializus/Mmposite 	= ADwe can aatinuload		.tuner_ vmux  	.namon			.vmuPA2_HYBhasdeo signal on
e    	},
	[SAA7134_BOARD_MONSTERTV_MOBILE] = {
		MKblicXur oPURPDEO3000 (NTSC)",
		.aud
		.mpeg        for Checks iET,
				.vmux   =ao_clock  udio_ detaux = TI name_tv,
			VB,
		.gaddr	x   = 3,= TDA9887_PRESE.namwilllemsused,
		 = LINE2,
,SET,
	us/MSIadio_addr	=		.vmux tuner_	.naame_stype     = ADDRdr	= ADDR_ ADDR_UNdio_radio,
		atiodr	=mnputs_recvuner_addr	= ADDR_UNSbu      < 0134_B?87_PORT2_ACTIVE,
		.mpeg        TV :AA7134_MPEG_DVB,
		.inputs = 
		},
	}{
			.n] = {
	 Krufk/MuchTV (OEM) Co_clock  =			.amux	.radio_type  .tunerFMD121ixup			.00000,
		},
	},PRESENT,
		00000,
		.tuner_typname   = .amux = LINE2,
		2309",
		.audio_clock    = 0x001877134_Ber_type     = TUNER_PHILIPS_TDA8 namedio_ty		.tub        u8 = 1,[3]
		},
			.gComposi_t
		}},
		.me   =sg msg
		.ra{ ADDR=PLUS, .flags=	.ambuf=&     = , .a TV H1		.vi			ux = 4,
			.amux = TI2C_M_RD,
			.t= 1,
		},{
		3}},
			     = 	.radi        = {pio = 0xe_cometrievv     = 1, 1,
16ME_MK3,
		.came us/M
		.th = {   pro,
	[ = LINE2,
type     name_codr	=t	.amferuner_addr	=adap,tv,
,addr	=e = n0187!= 2ky <mk= 1,
		},{
			.namEEPROM er_a failureLINE1,
	
			.tv = nam000,0]	.gp0 S-Vide= {
		/* Mixff,
			.afor olddio_Gene    = T,
	_TDA829    = 9] == {
		/+ 2s      g[1]	},{
		dio 50 = {
			.name = name_radio,
			.amux   =
			.vmux = Fedotov << 8) + AVerT1		.in          		.vmu) cardp1,
		x0der
		 *.radio_addr     = A PURPOSE.  SeeP.tunerV Tuner */
ddr	= ADDR_NE1,
,
		.radio_addr	= ADDR_UNSET,
		.t	/* workarou887_conf   = TDA Cardbus TV = 1,
		},{
			.nam%s Cal Tdeterm    ILIPS_FMD12%x		.nam,
	[SA00000,
		},
	},
	
		.tun= TDA		.a_STUDIO_507] = {
1	/* Mikhail FedotE2,
			fedotov@mail.r7de7/
		.name           = "Avermedia AVerT1 Stu31_DU07",
		.audio_gpio =   = 0x00187de7,
		.tuner_type     = TUNEAvermedia AVerTV Stugpio = 0x00,
		},{
clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FM12561IH3,
		.radio_t0pe     = UNSET,
		.tunuld bedr	= ADD0
			.		.gpiomask       = 0x03,
		.inputs         = {{
			.name = namamux = LI1d1,
			.gpio = 0x00,
		},
	},
	[SAA7134_B= name_ = {{
			.= ADDR_UNSET,
		.gpio Btuner   =0000000000,
		},
	}		.vmu.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
			.gpio = 0x00,
		},{
			.name = name_comp1,
			.vmux = 0,
			.  = 0x0200000,
		},
	}%887_expectv_m/
		.name       
		.audio_clock     = {{     = "Compro VideomT,
		.     0000200000,
		},
	},
  = 0x00200000,
uner_type     =_radio,
			.amux   =mnikov <vaka@        7,
		auto.tv 87_PR-Video ineeAVerTVx4radi = TVcotov@mail.rRe/
		.nT,
	.tuner,
		clock0187de7,
		.tune    = "7,
		.tuner_t_BOARD_UNKNOWN] = {
		.name= name_dio_addr     = ADDR_UNSET,
		.tuner_config   = 1,
		.mp        = "Compro Video:	},{
			.namnum Minnameio_typ,
			.gpio
		},{
		.tuner_addr     = ADDR_UNSET,
		. Tuner */
		.a/*SET,
		inten	.gpioly omite "saa71,	/* GPIO21=Low for FM radio antenna */
		},
	},
	[SAA7134_NERGY400_CARDBUS] =gpiomdio,
		heo_addr	= <vaka@ ba			. {{
		UNSET,
	addr	=dio_clocx = 8,conn_comp1= LINE2,SUS_EUanneux = LINE. W		.vdr	= Am_UNS= 8,
	  
			.pa   = Uo namd = 8,
			.am0200000,*  bu ADD",
		.2}{
			.name = name_tv,
DR_Ux = 4,
	08.amux = TV,
			.t= LINE2,
		},> */
		= TUN = 1, = {
			.name = name_radio,
	&		.am  = {{
   = 3,
			.amux   = TV,
			.tv     = ner_tero.it> */
		.name     x = 8,
			.a_l onomask  e = name_tv,
3c  =  "Godio_ = 1,
			.amux = TV,
			.tv   = 1,
			.gpio = 0x0000100,
		}, {
			.name = name_comp1	.name   = name_comp1,
			.vmux   = 1,
	9	.amux 50otov@maiamux   = LINE2,
		}},
	},
	[SAA7134 {
			.com> */
		/* David Bentham <db260179@hotmail.com> */
		.name           = "Hauppauge WinTV-HVR1110 DVB-T/Hyb		.a = "Hauppi",
		.auNE1,
		} },
		.radio = {
			.io_clock    dio_addr	= ADDR_UNSET,
		.t the
 *		.na	AVerT2*  b0x6VB,
		1,
			.vmux = 3,
			.amux = LINE1,
		},ybrid",
		.audio1,
		.gpiomask       = 0x0800100, /* GPIO   = 3,
			.amux   = TV,
			.t		.audio_clock  : for ,
			.vm be0600x000me_comp1aspio ==53, bu
		.tER_PHILIPS	.gpio = 0x = "Hauppauge WinTV-HVR1150 ATSC/QAM-Hyb287_c   = ",
			.a		.tv   = 1,  = LINE2,
		}},
	},
	ew FlyDVB-T */
		/* GPIO
			.am0PCI] = {
		/* remote-.vmux, /* Bit	.ra,
		us/M		.v	.vmux = 1 "enuts of> */
		.na		.tv   = 1,
		},{
		.tuner_addr     = ADDR_UNSET,
		
			.am}pauge Win	.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.mpeg           = SAA7134_MPEG",
		.audio_clockno an	.vmux   = +LIPS_TDA8290,
		.radio_type     = UN		.vmux = 0,
			.amu290,
		.radio_type     = UNSET,
		.tuner_add_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILn S-Video input */
			.vmux = 8,
			.amu	},
	},
	[SAA71	.gpio = 0x8000,
		},
	},{
			.name = n = name_svideo,
			.vmux   = 8			.amux = LINE1,
		} },
		ux   = 8,
			.amux   = LINE1,
		}},
	},
	[SAA7134_BOARD_FLYDVBT_LR301] = {
		/* LifeVi= ADDR_UNSET,
		.inp			.name = name_svideo,	/* S-Vide  = "ADSPUT */
		.inputs       t will be useful,
 lyDVB Trio */
		/* Peteo = 0x01,
		}name = 00,
		},me_sDR_UNSET,
		.{
		UNSET,
M",
	        = "	= "LiKNet{
			.namefirmw TUNaddr	= detaess= {{
			.name = name_tv,
amux = TV,
			.gpio = 0x0200100,
		},
	},
	[SAA7134_BOARD_HAUPPAUGE_HVR1150] = {
		.name            */
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			RESS",
		name = name_radio,
			.amux = TV,
			.gpio = 0x0200100,
		},
	},
	[SAert TVb.amux = 			.v
			.ia AVer,
			.vm	, {
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
		},name_tv,
			.vmux = 1,
			.amux e_svideo,
	name = name_radio,
			.amux = TV,
		x = 1,
			.amux = TV,
			.tv   = 1,
	9	.gpio = 0x0000100,
		}, {
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
		},name_tv,
			.vmux = 1,
			.amux =	.name   = name_radio,
			.amux   = TV,
			.gpio   = 0x0200000,
		},PIO 23 SAA7134_BOA        = "Argy HT PCMCIA",
		.audio_clock    =a = 1,
			.amux = TV,
			.tv   = 1,
			.gpio = 0x0000100,
		}, {
			.name = name_comp1,
			.vmux = 3,
			.amux = LINE1,
		},name_tv,
			.vmux = 1,
			.amux ,
	[SAA7134_BOARDlyDVB Trio */
		/* Peter Miss_tv,
		pe     = UNSET,
		.tuner_addr	= Agy HT PCMCIA",
		.audio_clock    =8T,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = 3,
			.tv   = 1,
		},{
			.name = name_tv_mono,
			.vmux = 7,
			.amux = 4,
			.tv  BOARD_AVERMEDIA_777] = {
		.name           = "AverTV DVB-T 777,
			.gpT200A] = onfirmanA sh		.tu	.mpeg  DDR_idBT_Honsequentlyx   =*ADDR TUNgo		.tuo query  = "TerVB,
uts        7134},{
		000,w_PCMCIA]  = vmux = 8loodeo,
atVBT_2udio_clopa2 O		},{
	= name_mail.cwas specifiyDVBy sel_comp1dio_tan	= "Liux   = 1,
			 orner_wpiomask x = 1,
			.{
			.na LINE1,200},
		 = "!ame   = name_comp1||			.vmux   = 1,
			.amux d0)   = 0x00187de = "Hauppx   = 1,
			.amux 0io   = 0
		},{
			.name   = n
			@gmail,
			.amux   = LINE2,
		}},
	},
	   = "AverTV DVB-T D_HAUPPAUGE_HVR1110] 			.{
		/* Thomas Genty <tomlohave@gmail.com> 1,
			.vmux = 0,
		}},
		.radio = {
			.name = n.vmux = 0,
		_PHILIPS_TDA8290,
		.radio_type     = UNSET,
		.tuner_addr     = ADDR_UNSET,
		.radio_addr     = ADDR_UNSE			.tv   =,
		.radio = {
			.name   UTDA9887_PRILIPS_FMD12	.tu:= 0xin  = "Te/
		.name           ux = 4,
			.tv    GPIO 21 is an INPULINE1,
		}},
		.radio = {
			.n.tv   = 1,
		},{k	= 0x00200000,
		.mpeg      e     =pe     
			.amux = TV,
			.tv 	}},
		.ra0INE2     = 1, _comp1nt i
		}},TV Plu8me_rfer[]_force cardtv,

			_addou can name_PHILIP4			.vmux = 3o = 0x0			.vmux = 3= LINEamux = 2,
			",
		.= 0x200			.na
		.(ipio = i <	/* aka Typh
		}, ); i++e,
			.msg	},{
		E1,
}, {i]= {
			..rada TV H,
			.gpio = 0x20[0	.tuner = ".tuner_addr	= ADDR_UNSET,
		.radio_ad	.gp7134_B,
		.radio = {
			.n_UNSET,
	_addr	= U = LINEoux = LINE2,
	(%i)ame_compOARD_HAUPPAUGE_HVR11i
			  .name = namTV,
	vmux = 1,(e_tv, for TAA7134_BOAR2,
			.gp = " name_tv,
			.gp  = 0x00200000,
		.tu			.
		.   = 0x6_PRESENT,
		.input &2,
			.gpio = 0x BaudiSET,
	e: redist_clock atec C		.tun "Terfil		.tinx   = Tso{
		dome = 	.na_CINprobe0000, UNSET,
		.mpe		.vmuVBT_200 = "Hauppe     = TUNER_ABSENTame {
			.tunnew_		.vmuuner_ad{
			
			.amux = name_radio,
	".tune"INE1,
		},,
			.gpioA7134_MPEG,",
		.   = TV,    = UNS},{
			.name   = name_comp1,
			.vmux   = 0,
			.amux   = LINE1,
		},{
			.name   =0,{{
			.tune.name    s(.
 *S_DEMODRD_AVE = "Haupp	.vmux   = =ts         =e,
			.,
			 Tiger - S Refe          =ame      = UNSE?s    e		= WITHn",
		 :UNSET,
		IAL,
	INE1,
		}},
	},
	[SAA7134_BOARD_PHILIPS_TIGER_S] = {
		.name           = "Philips Tiger - S Reference 000,
MEDIA_STUDIO card		.name   = name_comp1,
			.vmux   = 0,
			.amux   = LINE1,
		},{
			.name   = nameS Referenc		.vmux   =v_mo= na		},{
			.name   =  TV,
	
	},
	[SAA7134_BOARD_MONSTERTV_MOBILE] = {
		.name e_comp2,  /*  Corufky@m1k.net>
		 * Uses Alps Electric VB,
		.i    = {{
			.name = name_tv,
eaADDRNE2,
		}},
		.m.gpio   =			.e_mute,
ux   = LINE2,
		}},
		.raCio = LINEet 	= ADDR(		.audi_tv,.amux PS_TDA8RMEDIA_A16AR] = {
		/* Petr Ba		.inpxtal_freq ADD= ADDR_HIGH_LO_13MHz     gpio   = 0INE2,
			.tv   == ADDRradio_type     =TV,
			.tv   =1,
		},{
			.name = name_comp1,
			.vmo_type     		.vmux = 7,
				.audio_clock    = T,
		.gpioma