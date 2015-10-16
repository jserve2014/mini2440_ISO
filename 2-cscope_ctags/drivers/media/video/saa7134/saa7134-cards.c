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
			.vmux = 3evice ariver TVor phitv   = 1or p}, {r phi *
 /*
spec*compcardse dips sac stu (lips saLINE *
 *4 Ge *
 -specific stusvideoor phic) 2001804 Gerd Knorr <kraxel@b }or p.radio =tesex.org> [SuSE Laredisram is free softw2xel@bycan mpeg TV SAA7134_MPEG_EMPRESSf thes]
 *_out = CCIR656as publi_port_opts Gen(SET_T_CODE_POLARITY_NON_INVERTED |4 Ge		n; eCLOCK_NOT_DELAYense, or4 Ge (at youhe Lic any later verVSYNC_OFF),
 of t[eral PubBOARD_TWINHAN_DTV_DVB_3056]tributeorg> [T ANY WARR= "Twinhan Hybrid DTV-DVB l,
  PCI"as puaudio_clockARRANT0x00187de7as putuner_typU ANY = TUNER_PHILIPS_TDA8290f theify
 A PARTICULARUNSETTNESS FOR Aaddr	= ADDR_*  Thi for Generdetails.ic stu  You sh more configlic Lerms e GNU GNY WARRAANTthe hopelic LDVBas pugpiomasHANTwritBILIT20Freeas puinputs  buesex.org> [ic Lved a d
 *  This p Gen *
 * s freenc., l Pu based Tbridge, Mbytesex.org> [nc., 675 ffeived a (c) 20ic L shophid Knonc., ftware; yytesex.org> [lude <linbblishd a cThis pic L8,		/* untested */<linux/i2c.h>
#include <ou  GNU Gene  but W it andlude <linifyd a cit under, USAeive/

if ne tos pu ree  Softd ind inncludhope that GENIUS_TVGO_A11MCE*  but W/* Adrian Pardini <pardo.bsso@gmail.com>saa713org> 		= "Genius "tun AM"xc50anty ofd a cMERCH	o theYde "tea576w more al P	AR PURPOTNF_5335MF  Licenld hwith this p *  You shomore ave received a copy ofould have received a cop_radom.hot, writ>
#inclfatic chaared a e
 *  alonoundation, Ide <linMa_monlude "saa713roge, MA 02139ncludtPubliprom.h>adio";tea576

#incle <linux/init.h>
#incSuSE Laux/module.h>
#in01e <linux/i2csa#include _ff.
3[]   de "omposite3V cae <linux/i2c-algSuSE La
#include "saa713rogram is f "tesmp1[]  ;d TV "S-V]
 *";

/*yxc2028.h"
#include <media// = "Tdon.h>
#include <c char name  = "S-Vide1"tea5767.h"	.mutcharesex.org> [SuSE Ladr sideoct saeepr/* If redisA PAe !=U6SET, radio_ude "tuner"tda18271SE.  SeeSNAK000[SAA
/ITHORTICUL *  alonYNXP Snake DVB-S referen  Thesigname_radio[]   = "te to theY
static shoul *
 * d[ublic L    = ABSENtic char namehar name_tv_mono[] = "TV (mono "tunerly)";
static char name_co.are
 *ruct saa = {{
	_tv_monod a calong with t-----------; ]   = " F";
static chlude <linux/modulenclude "tuner-134.h"
#include "tuner-134.h"
#i-algo-bitncluA7134_BOARDe_mute4-reg.187d[SAA7134_BOARD_FOR -xc202
#include "tda18271CREATIX_CTX95300.h"

/NOWN/GENERIC",= "Medion/Creatix SET,
	ut eveer_type	= TUNER_ABSEadio";
or FIe for more al Publtuner_ad] = {
		s pud a c;
static char nadetails.
 *
icens,

		.inult",
			.vmux = 		. *
 ard dep[]    = "Composinclud;
static= ic char,
			.amux =INE1evic}},
	 LIN[eral Pub tha/* ----1";adio_type     = UNcVide= "C
/* ----2	.radio = 0187d/

#incl
static char name_comp4[]   = "Composite4";
st34_bo-- free software; y------------------------------------------------------ */
/* b      
#include "tda18271MSI_TVANYWHERE_AD11	.tuner_addr	= ADDR_UN
		.aMSI TV@nywhere A/D v1.1r_type	= TUNER_ABSENT,
		.r
			.name = name_comp1,
	.tuner_ad = 0,
			.amux = LINE1,
		},{
			me_tv_mono[] = "TV (mono only)";
static char name_comp[]    = "Composime = name_tv_ char name_tv_monomux = LINE1,
		}},
	},
	[SAA71te";
static char naude "tea576[]   = " Fotuner-media/v4l2-commMass Ave, Camevenite2";
statimedia/tveeprlips reference den	.audilips reference de */
		.name		= "Proteus Pro [philips reference design]",
		.audio_clock	= 0x00187de7,
		.tuner_type	= TUNER_PHILIPS_8[SAA7134_BOARme = name_comp1,onnclude "tuner-e = natvee_comp3[A7134_BOARD_ = {
	[SAA7134_BOARD_UNKNOWNAVERMEDIA_CARDBUS_50	},{o_type     = UNSET,
		.tAVerevica Cardb.radi/R"
#in(E506R)r_type	= TUNER_ABSENT,
		,

		.gpiomask       = 0xe000,
		.XC202riverstatic char name_tv_mono[] = "TV (mono only)";
static char name_comp[]    = "Compos  0evice ariver  <kramux = LINE2,
			.t -Videme_radio,
				.radi org> [SuSE La	.tvnam e_radio,
			.a har nameNE2,
		 },
	},
	[SAA7 bytesex.	. FOR Adetailux/module.tv,
				.e <lindetails.
#include k,
			.vmu0xe00type .in
#include "   = {{
	river  ADDR_U * devPubli ,
			.g .h"
#include < 0xe000,
		.inon.h>
#inc.name = name_tvmux = L FOR A PAe	AR PURde <=  = TA16Dme_muteype     = TUNTVevice if n,
			_UNSET  LINE2,
		omp2  = 1,D_FLYVIDEO2000] name_c/* "TC Wan" <tcwan@cs.usm.my>a713TV,
			.0x4000,
		= "LifeView/Typhoon Fly name_svi" {{
	f
 *  MERCH= 1,
			00_svi = 0x4000,
_type     = TUNER_LG_PAL_NEW_TAPC,
		.v   = 1,e     = UNSET,
	o = {
			.name *
 *
 *redisype     = TUNER_L2mux =LINE2,
			.tv   = 1,ame = name300ew FlyVIDEO3000",
		.audio_clock	= 0x002000char name_comp4[]   = "C
			.vmuMarco d'Itri" <md@L34.h.IT> *                                               */

a/tvee
			4pio = 0}, = TV,
			.tv.name M115,
			.vmux = 3,
			.amux = Lverm,
			.
 *
		.tuner_addr	=bs]
 *evice driver 8SAA7134_BOARD_FLYTVPLA LINE2,
			    = UNSTINU	 redistrame_c	.tuner_addr	=},
	[SAA7134_BOARD_FLYTVPLA LINE2,
			_svi,/* Composite signal on S-Video in8   = UNSENUM_MINI] = {
		/* "ArnaTVPLATINUM_MINIdeo,
			.vmuArnaud Quettme_radio,
		4	.radio = {
			.namebs]
 *[]			.vmux = 3,
			.amue" <aq.amux@----.fr
			.gpio = 0x4000Publio = 0x4000,
		},{
			.nam nameMATE_T75e		= "UNK* cJohn Newbigin <jn@it.swin.edu.au,
			.v-Vide = UNSET,
		.tradiro VlishMate V Pl
		.radio_addr	= ADDR_UNSET,

		.gpiomask       = 0xe000,
		.
			.amux = Lsed TV c= UNSET,
		.tuner_addr	= omp1,0x400/* radio,
		 tunerl on config  are
 			.gp	.vD_PROTEUS_PRO] = {
		/*
			.vmux = 0,
		Proteus Pro [phi			.gpio = 0x4000,
		},{
			.name = name_comp2,
			.vmux = 3,
				.amux = LINE2,
char nameof     = UNSET,
		.tuner_addr	={{
			.name = name_tv,
			.vmux =* Composite4000,
		},{
,
		}}, PURPOSE.  See the
 *1,
		},{
	_tye     = UNSET,
		.tuner_addr	= o700_PROatinum FM (Matthias Schwarzott <zzam@gentooi2c-peter.missel@onlinehome.deme_tv,
			.E2,
	Pro NER_
		.radio_addr	= ADDR_UNSET,

		.gpiomask       = 0xe000,
		.ame = name_tv,
		e = name_.name = name_tv,
			.vmuxfault",
			.vmux = TV,
			.tv   faultradioe driver 
		},
		.mute = {
			.name = name_mute,
			.amux = LINE2
		.radio.namT     tinum Miniradio,
-ram is free software; y---------------------------------------de "t*  MERCH on S-		.gpio = 0x2000		},{
			.name     = TUNER_LHYBRIal on SvSET,

gpio = 0
		.tuner_type    */me = name_comp1,_TDA82	.vmadio_type     = UNSET,_UNSET+FM.tuner_addr	= ADD		.gpio = 0x4000,       2,
/*.amux = TV,
			.tv  .gpio = 0x4000,
		}},
		.radio = {
			.name = naype 		.gpio = 0x10000,
	3},
	},
	[SAA7134_BOARD_ROVERMEDIA_LINK_PROuct saa713			.vmux = 1,
			.amux = TV,
	.vmT,
		.tutunedio_addr	= ADDR_UNSET,

		4if not,mux = 1,
			1Eut *ock	=et GP16dia/ unu#inc15= UNSET,
		.tuner_addr	= ADD name_tv,
			.vmux = 1,
			l.com> */
		.name		= "RoverMedia TV 1,
		},{
			.name = name_ TV input */
			.tv   = 1,
		},{
/*			.name = name_tv*/
			.tv		},{
			.tunBEHOLD_Hame_mute,_boagor Kuznetsov <igk@igk.r<me = name_comp1,	/* ComposBehol */
_tv_moTV H6
		.radio_addr	= ADDR_UNSET,

		.gpiomask       = 0xe000,
		.inputs  FMD1216MEX_MKe <li		},{
			.name = name_comp2,	/* Composite input */
			.vmux = 3,
			.amux = LINE2,
/*			.gpitda9887Licen_type	DA    = any = name_{
			.naddr = name_c	.tuner_addr	=		.vmux = 3,
			.0,
			.amux = LINE2,
		},{
			.name = atic char nameNE2,
		},
	},
	[SAA7134_BOARD_FLYVIDEO3000] = {
		/* "Marco		.vmux = 8,
			.amux = LINE2,
/*			.gpio = 0x4000,         */
	.audio_clock	= 0x00200000,
	                                          */

/* If rad
		}comp1,     /* ComposSUSTeK_TIGER_3IN saa7134_bome_tv,
			.vmux =As.radiger 3in
			.name = name_comp2,	/* ComposiLIPS_FM1216ME_MK3, .amux = LIN input */
			.vmux = 3,
			.a *
 * device driver LINE2,TV,
			.tv   =,e = name_svideo,
		_TDA0x00000,	e Yudin },{
			.name = name_tvte";
static char 1 << 2 *
 *

		.inputs         = {{
{{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   == 0x47134_BOARD_FLYTVPLATINUM_MINI] = {
		/* "Arnaud Quettideom Mini",
		.audio_clock    = 0xr	= ADDR_UNSET,
		.mux = LINE2,
			.tv  		.name = name_comp2,	/* Composite TV input */
			.tv   = 1,
		},{
/*			.name = name_t  = "S-Vide=NE2,
			.gpio = 0x2000,
		},
		.mREAL_ANGEL_22 S-VideoFme_comp1,	/* ComposZogis Real Angel 22	.gpio = 0x10000,
		},
	},
	[SAA7134_BOARD_ROVERMEDIA_LINK_PRO"Television	.radio = {
			.name			.vno,
	NTY;V (DR_Uite input */
			.vmux = 3,
			.amux = LINE2,
/*			.gpite";
static char na801a808name =_type     = TUNER_PHILIPS_TDA822,
/*			.gpio = 0Proteus Pro [philipsrd saa7lips reference
			.amux = L624,
			.gpi	.radio_type,14,13
#inOutr_addr	= 	= ADDR_UNSET,
		.radio_addame = name_tv,
			.vmux = 1,
amux = LIN		},{
			.nao_clock	= 0x00187de7,[SAA7134_BOARD_FLYTVPLATINUM_MINI] = {
		/* "AMD9
			.gpio = 0x4000,
		}},
		.radio = {
			.name = name__board saa7134_b.amux = LIN<linux/ = nam char nabe rg> [Suedtveeprstruct {
			.n.name = name_tv,
			.vmux = 1eDS_INSite _Hbe uPCI,
			.amux =o = {
	 or FI,
	DS Tech Instantss_ad
		.radio_addr	= ADDR_UNSET,

		.gpiomask       = 0xe000,
		.inputs   UV1236D = 0,
			.amux = L		.vmux = 1,
			.amux = TV,
			.tv   =inputs         = {{
			.name = name_comp1= name_comp1,
			.vmux = 0,
			.amux = LINE1,
		},{
			.name{{
			.name = name_tv,
			.vmux uts         = {{
			.empress_addrmposite signal on S	.tuner_type  1,
		},{
			.name = name_comp1,     /* C.  SeeFM nam  = 0x002000	.gpio =  = "m-Video signa			.name radio,
			.amux =o = {
			.name = name_radio,
			.am
	},
	icense 
			/* workaround for proble4000,
		},Rev:1	= ADDio = 0x10000,
		},
	},
	[SAA7134_BOARD_ROVERMEDIA_LINK_PROinputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.gpio = 0x8000,
			.tv   = 1,
		},{
			.name = nn	.name = name_x = e = name_radio,
			.amux = LININE2,
			16=0 selects FM radio ante name_comp1,
			.vmux = 0,
			.amux = Lname_comp1,
			.vmux = 0,
			.amux = LIE1,
		},{
			.name = name_comp2,
			.vmux = 3,
			.amux = LINEr	= ADDR_UNSET,
		.empress_aux/mrd saa700187de7,udio_clock  x = TV,
			.tv   =NSET,
		.empress_addr 3,
			.amux = LINE2,
			1gpio 	.vm2 */=1 selecundatir RDS",
		.aused 		.radio = {
			.name = name_radio,
			.amux = LINE2,
			.gpio = 0x2000,
		},
		.mKWORLD_PLde "t_ANALOGme_mute,
			.amux = TV,
			Kworld PlET,
		Analog Litewarr
		.radio_addr	= ADDR_UNSET,

		.gpiomask       = 0xe000,
		.YMEC_TVF_5533_UNSET,
		.radio_addr	= 00,
		.EA576name = name_= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
	rds[ux = 3,
			.amux = Lsed TV0007	.tuner_type     = TUNER_PHILIPS_TDA82= 0x10000,
		},
	},
	[SAA7134_BOAsvideo,
			.vmux = 8,
			.amux = LINE11ARD_MD9717] = {
		.name		= "Tme = name_comp1,    Proteus Pro [philips refe
			.amux = L2NER_PHI717deo,
			  },
	},
	Tevion MD me_cradio,
	 name_tv,
			.vmux = 1,
	a TV Link Pro FM",
		.name = name_svideo,
			.vmux = 8,
			.amux = 	},
	},
	[SAA7134_BOARD_FLYTVPLATINUM_MINI] = { name_sviET,
		.inputs         = {{
			.name = n---------------------voort@inux/i2r nam	       },
.name = nGO_007_FM,
			
		},{
			.name = name_comp2,	/* Compe = TV GO 007 FM* Typ
		.radio_addr	= ADDR_UNSET,

		.gpiomask       = 0xe000,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.gpio = 0x8000,
			.tv   = 1,
	
			.gpio = 0x0000,
03x820e <li/* SENT,
		.gpiomask	= 0came_c3,eter.mi4_BOARD_MD9717] = {
		.name		= "Tme = name_comp1,     /* Composite signal on		.n"Rovermen.or.jp> *		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.men.or.jp "Te.tuner_type	= TUNER_PONSTERTVdeo,
			.vmuK.Ohta = Llpha292@bre},
	[SAA7134,
			. */
		},{
			.= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.			.na		.ra
		.tuoed T0,
		}},
Terratec Cinergy 40STUDIO_507UAatinum FM (ondy Shevchenko <andy@smile		.v.u		},
		.mute = {
			.name = name_mutelock	= Std a cname_tuts         = {{
			.name = name_tv,
			.vmux = 1,
			.amu,
			.vmux =ADDR_UTDA829ck	=nputs    MK5eter.mi10000,
		},
	},
	[SAA7134_BOAvmux = 0,
			.amux = LINE1,
		},{
			.name = name_comp2,
			.vmux = 3,
			.amux = LINE1,
		},{
			.
			.gpio = 0x0000,
tv,
		,
			.vmux = 1,
			.amux = TV,
			.gpio = 0x10000,	/* GP1_MD9717] = LINE2,
			.tv   = 1,
		}216ME_RD_MD9717] = {
		.np4adio_type     = U	},{
			.name = name_svideo,
	ard configamux = LINE2,
			.tv   {
		-ORLD] = {
		.name           = "Kw	   /* board
		.fige = name_comp1,     /* Composite  = 0x00187de7,
		.tun7134_board saa7134_board probd fowas:e_tvnputs         = {{
			.name = name_tv,_boaINE1,
		},{
			.name =},te inFMdeo,
			.vm-Video signalS3n S-VideoFM (Lan D. Louw <jd.louw@mweb.co.zaeter.misselET,
= "Terratec C = 0xx = /S3r RDS",
		.audio_clRedis";
			.name = name_compVideo1,
	       */
		},{
			.n	,{
			.name = name_comp1,     /* Composite signal on S-Video input *1,
		/
		amux = LINE2,
			.tv   = 1,D_Puts        	mux = LINE2,
			.tv   .amuname_tv,
		ame = name_comp2,
esign]o,
			.a
#include "saa71	.audd foNot ck	= 0x00187d[SAA7nput */
Composite inNSET,
		.inputs vmux = X7			.amux = = ADDR_U,Intl. Ltd. Dmitry Belim = {d.b = 0x0type     = UNer.missel,
			.amux = LINE9887_P2,
/*			.X7id",
			.vmux = {
			.name = name_comp2,	/ROte = {
		LINKE2,
_F5.tv   = 			.name = name_comp2,	/* Composite input */
			.vmux = 3,
			.a = LINE1,
		},{

			.name = "CVid 
			.name = name_svideo,
			.vmux = 8,
			.amrd saa7T,
		.radio_addr	= A
		}},
		.radio = {
			.name = n,
			.vmu
		.audio_clock    = 0x002000      },
	},
	[SAA7134_BOARD_TVSTATION_9		.amux = LINE2,
			.gpio = 0x2000   = 0xe000,
		.inputs         = { {
			.name = name_tv,
			.vmux = 1ZOLIDNE2,
		ame = naC_M,orkar";
su shoproblZolidme = name_ Tmoreuner_type	= TUNER_PHILIPS_FM1216ME_MK3,
		.radio_type     = UNSETinputs         = {{
			.name = name_tv,
			.vmux = 1,
			 {{
inpuux = 8,
			.amux = LINE1,
		},{
			.name =ame_tv,
			.vmux = 1, TDA	.am_enseENwas: 0x00200000, */
		.tuner_t= nam = "e = name_tv_mTS_PARALLELRDS",
		.audio_clock	= 0x00200000,
		.tname = name_svideo,
			.vmux			.name = namr RDS",
		xc2028.h"
#incluAA7134_BOARD_addr	= 0,
		.inputs         = { {
			.name = name_
};

conscludTV,
e
#in name hopebcounby
 T,
	Y_SIZE(OON_90031utoSs)nfo   -r "TV+R34] = /* Tom Zoerner <tomzo at users sourceforge net134] ={
		/*uner ids + subsystem ID.audio_clo031radio,
			.amux = LINE2,
			.gpi		.name = npci_ass Av_idHoon "TV+ = ntbl[me_muteMERCHvendoENT,
	ame_PCI_VENDOR_,
		 = {
		.vid= namev,
			.vmux =DEVICEetails.
 *
_e = namUNSETsublock	= 0x0S / Typhoon.name = name_tv,
sub{
		.name	V,
			.			.tvc) 200_data name_tv_monNKNOWN]ROTEUS   =.
 *
	.vmux = 1,
			.amux = TV,
nputs         = {{

			.name = name_comp2,	/* Commask			.am_cos_addr vmux   = 0,
			.amux   = LINE1,
		},{
			.nname = n.name = name_comp2, uner_type = "CVi	.tv gpiomask	= 0uner_type     = TUNER_adio = {
ame_tv,
			.vmuxadio.vmux = 3,
	adio,{
			.name = name_sviadio = nf.amux ,
			.vmux   = 8,
			.amux   = LINE1,
		}},
		.radio = {675h this   = name_radio,
			.amux   = *  Youas= 0x4Terratec CinergELSAomp1,
			.vmu0x4000,
		}},
NSET EX-VISION 300TVradio,
			.amux = LINE2,
		ion RDS / Typho0x113			.tv	}},
		.radio = {4e85		.name   = name_comp1,
			.vmux  .name  a/tveamux   = LINE1,
		},{
			.na= ADDR_UNSET,
		.radio_addr	= ADDR_U{{
			.name = name_tv,
			.vmux = 1,
_type     = TU53{
		ELSA] = {
		.name  114			.name   = name_comp1,
			.vmux  CINERGY4nfoRD_FINE1,
		},{

			4me_radio,
			.amux = LINE2,
		},
	       radio_addr	= ADDR_UNSET_5ideo,
		.inputs         = {{
			.name = name_saddr	radio,
tv,
		8,
			.amux = LINE1,
		},{
		put */
6		.tv   = 1HITACHI_NTSC {
			.name = name_tv,
			.vmux = 1mp1,
			.vmux   = 3,
			.amux   = LINE1,
		},{
			.na= ADDR_UNSET,
		.radio_addr	= ADDR_U5s     2,
/*			.gpiDS / INE2,
		},{
			.name =me_tv,
},
	[SAA7134_BOARD_ELSA] = {
		.name           = "ELSA EX-VISION 300TV",
		.audio_clock    
	[SAA7134_BOARD_ELts         = {{
			.name = name_6mux = 8,
			.amux = LINE1,
		},{
		put */
			},{
			/* workER_HITACHI_NTSC,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs  516		.naLSA] = {
		.name  013r	= ADDR_UNSET,
		.radio_7ddr	= ADDFLYradioTACHIv,
	 = LINE1,
		},{
			.name = name_comp2, /*,
		.radio_type     = UNSET,
		.tuner_ux = TV,
			.tv   = 1,
		},{
			.name			.s     
		},{
			.name = name_svideo,
		{{
			.name = name_D_ELSA_700TV] =     = {
			.name = name_mute,
			.amux = TV,
		},
	},
	[SA		.name = name_comp2,	/adio_addr	= ADDR_UNSET,
		.inputs  4e42,ater.vmux {
			.   =Capodificen
		}" Art.No. 50673tv,
			,
		}},
ASUS TV-FMA.
 *,
			.vmux = 8,
			.amux = LINE1,
		},{
		put */
			.tv   = 1,
		},{
    .aME_MK.vmuxmux = 8,
			.amux = TV,
			.tv   = 1,
		},{
			.name  = 0,
VFMte,
,
		.inputs         = {{
		ame = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   =  i   = T = TV,
			.tv   = 1,
		},{
			.name,
		.radio_type     = UTV-FM 7134",
		.audio_clock    = 0x0018			.aadio,
			.a 3,
			/* = nam= 0x0LINE1,
		},{
			.name = na-VISIOVideo signal onrime30 OEME.  Seeomp1,     /* Composite sign_type  dr	= ADDR_UNSET,
		.rame  "EMP 6,
		5.amux = LINE2,
		}},
		.radio = {
			.nam5radio,
			.amux = LINEe_tv,
			.vmux = 8,
			NE2,
		}},
		.radio = {
			212icensminipci, LR212ux   = 3,
			.amux   = LINE1,
		},{
			omposite input  = {{
			.name = name_svmux 			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = n= name_tv,
			.vmux = 8,
			.4= UNSE187d			.name = nameA] = {
		.name     		.n,
		.radio_type     = UNS = name_tv,
			.vAA7134_BOARD = 1name_svideo,
			.vmux = 6,
			.amux = LINE2,
			.gpio = 0x0000,
		}},
		.radio = {
			.name = name_radio,
			.amux 3,
	r_type     = TUNER_PHILIPS_TDAls.
M7135] = {
		.name           = "ASUS pio = 0x0000,
		},

	},
	[SAA71
		.audio_clock    = 0x00187de7,
		.tuner_t_UNSET,
		.radio_addr	= ADD 1,
		},{
			.name = name_comp1,     /* Composis    = 8,
		mtatic1,
		nologies (v,
			.v)
		},{
/*			.name = name_214T,
		.tandard 4,
name_c4TV T E
		.rearlier (e = na5 = {{
		name_mute,
			.gpio = 0x0000,
		},

	},
	[SFMux = LINE2,
			.tv   = 1,
		}},
	},
	[SAA7134_BOARD_ELSA_500TV] = {
		.name           = },{
			.name   = name_comp1,
.vmux = 3,
			.amu_type     = TUNER_PHILIPSNE1,
		},{

			.name = 5,
			.amux = LINE2,
		},
	      F onwmux  Cinerg11MOnameVMAme   = LINE2,
		liliddr	g" <llc@134.hfansi2c-al		.gpio = 0x4000,
		}},
10 0xe0warr},
	CAPTURE 	.naradio,
			.amux = LINE2,
			.gpio = 0x2000put */
			.tv   = 1LG1489icensKYE= name_comp2, /* CVideo over SVidute	.ra0,
		Won87_PProTV0xe000,
		.inputs         = {{
			.name = name_tv,
		,
		is anENT,
	WF actuallyE.  S.name = name_tv,
			.vmux = 8,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_tv_mono,
			.vmux = 8,
			.amux =6bame =E1,
		},{
			.name 	.inputtype     = TVISION 300TV",
		.aD 1,
					.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,     /* Composite signal on S-Video input *.name ,
		 3,
46 agpio =TV, HW= LIN, UNSETame_comp1,
			.vmux   = 3	.vm,
		onlydio,
			.a			.n.gpio  shonow0xe000,
		.inputs         = {{
			.nLINE2,
		}, MERCHARD_ASUSTeK_TVFM7135] = {
		.name           = "ASUS TV-FM 7135",
		.audio_clock    = 0x00187de7,
		.tuner_t10audio_},
		.radio = {
		226.name  = TV,
			.tv   = 1,
		},{
			LSASAA7134_BOARD_ASUSTeK_TVFM7134] = {
		.name           = "ASUS TV-FM 7135",
		.audio_clock    = 0x00187de7,
		.tuner_ts         = {{
			.name = nama_LG_PAL_NEW_TAPC,
		.radio_type     = _add       = {
			.name = name_mute,
			.amux = TV,
		},
	},
	[SAUNSET,
		.tuner_addr	= ADDR_UNSET,
E2,
		},{
			.name = name_svideo,
		{{
			.name = nac
	[SAA7134_BOARD_MD9717] = {
		. GNU   7A EXeral Public License 		.nas]
 *ed by
 *  the 	.gpiomask       = 0x2eView FlyTV	= ADDR_UNSET,
		= 0x00187de7,
		.tuner_type     = T
	},
EKRD_ELSA] = {
		.name   8vmux = 8,
			.amux = LINE1,
		},{
		4 */
		.nVFMNE2,
		},{
			.name = name_comp2,	/BMKblicXur oPURP*/
			.vmux =ndrew de Quincey = Ldq@lidskialf.nHI_NTSC,
		.radio_type_tv,
			.vmux,
	[SAA7134_BOARD_ELSA_500TV] =  1,
		te,
			.amux = LINE2,
			.gpie_tv,
			.vmux =  =.name = name_tv,
			.vmux = 8,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_tv_mono,
			.vmux = 8,
			.amuT,
		.radio_addr_G		.aPLUS,
		.inputs         = {{  	.radio_type     = UNSET,
	ype     = TUNER_LG_PAL_NEW_TAPC,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.r.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ,
			.vmux = 8,
			.amux = LINE2,
			.audio_clock     name_svideo,
			.vmux = 6,
			.amux = LINE2,
			.gpio = 0x0000,
		}},
		.radio = {
			.namemux = LINE2,
			.tv   = 1,
		}},
	},
	[SAA7134_BOARD_ELSA_500TV] = ux = 4,
			.amux = LINE2,
			.tv   = 1,
		}},
	},
	[SAA7134_BOARD_ELSA_500TV] = {
		.name    pio = 0x00006c0001o = {
			.name   = name_radiotv,
	= 0x00187de7,
		.tuner_type     = TUNER_HITACHI_NTSC,
		.radio_tfe		.name   = name_radio,
			.amux   =TVSTATION_RD = name_tv_mono,
			.vmux = 8,
			.amux = LINE2,
			.tv   = 1,
		}},[SAA7134_BOARD_ELSA_500TV] = {
		.name           = 89= 0x00187tv   = 1,
		},{
			.name = name_tv_mono,
			.vmux = 8,
	LIPS_FM1216ME_MK3, cfr_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_Uame_tv,
			.vmux = 1,
			INE2,
	2			.1,
		.= LINE2,
			.tv   = 1,
		},{

			.name "Compro00de "te<< 14,
		},{
			.name = name_comp3,
			DVR      = 0xcf00,
		.inputs         = {{
			.name = name_comp1,
			.vmux = 0,
			.gpio = 2 << 14,
		},{
			.name = name_BOARD_ELSA] = {
		.name  A7134_BOA   = name_radio,
			.amux   =VAcensPOWE= LINE2,
		}},
		.radiver,
		a M156 / ,
		.r 2819			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		} 0x00187de7,
		.tuner= 8,
			.amux = LINE2,
			.tv   = 1adio = {
			.name   = name_radio,
			.amux   =E2,
S_FMVMAo = 0x400me_svideo,
			.vmux = 6,
			.amux = LINE2,
			.gpio = 0x0000,
		}},
		.radio = {
			.name = name_radio,
			.amux 85    = {{
			.name = nam		.a_FM1= TV,
			.tv   = 1,
		},{
			.neo signal  SAA7134_MPEG_EMPRESS,
		.video_out = CCIR656,
	},
	[SAA7134_BOARD_VIDEOMATE_TV] = {
		.name     VIDIN_SEL
		*/
		.name        o = {
	svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name =tv,
_G,
		FM12x   = LINE1,
		},{
			.name   = nam= 0x00187de7,
		.tuner_typeLIPS_FM1 on S- = 4,
= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.emMATROX		.name = name_tv,
			.vd
			.vmux = 0,
			.gpio = 2 << 14,
CRONOSX__tv,
			.vmux = 1Greg Wickham <g7,
	wdio_ad@grangenet.et> */
		pio = 0x4000,
		}},
BMK  = n T			.2,
		},{
			.name = name461		.amme_tv,
			 = {
			.nameInc
		},{
/*			.name = namea70R_LG_PAL_NEW_TAPC,
		.radio_type   MD28tv,
	ame = name_radio,
			.amux = LIN = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_tv_monome = name_radio,
			.amuCompro VideoMate TV",
		.audio_cloca	.name   = name_radio,
			.amux   = LIN},{
			.name =	},{
			.naempressT,
		.1,
			.am = 0 << 14,
		},{
			.name = name_comp4,
			.vmux = 0,
		,
		.io_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
	00000E
		.input3,
		.inputs        
		},{
			.name = name_svideo,
			.v			.amux = LINE2,
		de7,
		/* probably wrong, the 7133 one is the NTSC version ...
		* .tuner_type  = TUNER_PHILIPSINE2,
		},{
			.name = ntype     = TUNER_LG_NTSC_NEW_TAPC,
		.radio_type     = UNS211 = 0x08> */
		.]
 *       = old+radi,
			.tv   = 1,
30amux = TV,
			.tv   = 1,
		},{
			.name = name_compv,
	_M_comp3,
			.vmux = 0,
		800  = name = PRESENT,
		.gpiomask	= 0x	.natrox CronosPlus",
		.tuner_type 		.amux = LINE1,
		},a  = "ASUS TV-FM 7135",
		.audio_clock    = 0x00187de7,5ck    = <greg.wickham@grangenet.net> */
		.= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.empress_addr_PINNACLE_PCTV_STEREO] = {
		.name           = "Pinnacle PCTV Stereo (saa7134)"tuner_me = name_svideo,
			.vmux = 7,
e PCTV Ste,
	},
	[SAA7134_BOARD_KW	},{
			.name = name_comp1,     /* Composite signSET,
		.gpiomask       = 0x2PINNACLE_PCTV_me  EO,
		.inputs         = {{
		Pinnacle name Stereo ({
			.n)"10ff= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		DVD_EZMAKtv,
			.vmu] =  = LINE2,
		}BusE.  See 3,
			.amux   = LINE1,
		},{
			.narobably wrong, the 7133 one is the NTSC version ...
		* .tuner_type  = TUNER_PH},
	[SAA7134_BOARD_ASUSTEK_TVFM7133] = {
		.name   d6dio_a.{
		ux = LINE1,
		},{
			.name = name_coault",
			.vmux D_MANLI_MTV002 = LINE2,
	Ognjen Nas_typ<o
			.@logo-- *.ba	.vmux = 1,
			.amux = LINManli MuchTV M-puts tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amuxSE.  SeePAL {
			.name = namb7etda988,
			.tv   = 1,
		},{
			.name = name_tv_mono,_5pe   T2032,
 namransGe    = U.vmux = 3.amux = TV,
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
			.name = n050 = TV,
			.tv   = 1,
		},{
			.nameTGeo,
			.name           = "AverMedia M156 / Medion 2819",
		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_FMb.tv  		.name = name_svide2R_LG_PAL_NEW_TAPC,
		.radio_type   	},{
			.name FM12EUNSET,
		.tuner_addr	= ADDR,
		.inputs         = {{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
		{
			.name = name_tv,
			.vmux 			.tvatrox CronosPlus",
		.tuner_type    = 300Ipio T_PA_tv,.name = name_tv,
			.vmux = 8,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_tv_mono,
			.vmux = 8,
			.amux =0,
		.tuner_type     = TUNcb= 0x001= TV,
			.tv   = 1,
		},{
			Ce "tP3XP= UNSET,
		.tuner_addr	= .vmux = 3,
			   = LINE2,
		},
	       .mTUNER_P		.naS_NTSC_M,
		.radio_type     = UNSET,
		.name_radio,
			.am	.audio_clock = "Compro VideoMate TV Gold+",
 = 3,
			._4CBamux = LINE2,
		}},
		.radio = {
			.name = name_radio,
			.amux = LINE1,
		},
	},
	[SAA7134_BO.audio_clock	= 0x0018SAA7134_BOARD_ASUSTeK_TVFM7134]14,
		},{
				.gpiomask       = 0x2		= {
			.name Fre	.vmux = 3,
			.amux = LINE1,
			.gpio = 0x02,
		}, {
			.name = name_comp2,
			.vmux = 0,
			.amux = LINE1,
			.gp2a    = {{
			.name = nam08,
			.vmux = 0,
			.gpio = 2 << 14,
UPMOST_   =L {
			.name = name_mute		.amu
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux =mux = TV,
		1,
E2,
		},{
			.name = nam	.amux = LINE2,
		},
	      x = 1,
			.amux = TV,
			.tv   = 1,
		},{
		.inpcomp4,
		.vmux =0000,
		},
	},
	[SAA7134__type     = TUNER_PHILIPS_TDA,
		.ra00,
		.tuner_type     = TUNER_PHILIPS_svideo,
			.vmux = 8,
			.aamux = LINE2,
			
			.amux = LINE1,
		},{
			.namP = LINE2,
		}},
		.radi
		.tda9887_conf   = TDA9887_PRESENT,
		.gpiomask	= 0x03,
		.inputs      ,{
			.name = name_comp1type     = TUNER_LG_NTSC_NEW_TAPC,
		.radio_type     = UNS97 = "ASUS TV-FM 7135",
		.audio_clock    = 0x00187de7,
	namemux   = 3,
			.amux   = LINE1,
		},{
			.na		.name           = "Manli MuchTV M-TV002",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.radio_type  ddr 	=00187de7,
		.tuner_type     = TUN		.tuner_ad,{
			.name = name_tv_mono,
			.vmux = 8,
			.amux = LINE2,
			.tv   = 1adio = {
			.name   = name_radio,
			.amux   = me = name_mute,
			.amux =		.inpuvmux = 0,
			.gpio = 2 << 14,
		},{
			.e = naddr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.empress_addr 	= 0x20,
		.inputs         = {{
					.inputs         = {{
		  .. 7 1236 Tuner Card (N52	.amux = TV,
			.tv   = 1,
		},{
		PROer SV_PV9tv,
mux = 1,
				.gpio = 0xuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = T = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv502,31",
		.audio_clo TuAA7134_vers			.xe000,
		.inputs         = {{
			.namear 30DUO = name_comp2,	/* Card (PAL,FM)",
		.audio_clock    = 0x187de7,
		.tuner_type     = TUNER_PHILIPS_PAL_I,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR306 3,
			.amux   = LINE ILIP,
			.amux   = LINE1,
		},{
			.name   = name_svDU},
	},
	[SAA7134_BOARD_ELSA] = {
		.name           = "ELSA EX-VISION 300TV",
		.audio_clock    HI_NTSC,
		.radio_typM)radio,
			.amux = LINE2,
oup ECS TVP3XP FM1236 Tuner Card (f31 8,
			.amux = LINE1,
		},{
			.name = name}}0  6,
			1246{{
			x8000 (v,
	Fradio = {
			.name   = name_radio,
			.amux   = LINE2,
		},
	= name_tv,
_I {
			.name = name_tv,{
			.name = nameA EX-VISI},
	[SAA7134_BOAVISION 3TVPLATINUM11		.gpiomask       = 0x2TGame dr	= Amp1,
			.vmu35	.inpuda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv,
			.amux   = LINE1,
		},{
			.name   = name_an
			uk134] = {
		.name Elitegroup = 0,
			OUGHradio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_tv,
			.vmux = 1,
			.amux = LINE2,
			.tv   = 1,
		},R,
		.tu= TUNER_PHILIPS_PAL50      = "CVid over SVid",
			.vmux   = 0,
			.amux   selects FM r
			.amux   = TV,
			.tv     = 1,
		},{
			.name   = name_tv_mono,
			.vmux   = 1,
			.amux   = 		}, {
			.naux = LINE2,},
	},
	[SAA7134_BOARD_KW.ra1
			.name = name_, new reviame_comp1,     /* Composite signal on S-Video input ame = name_comp1,
			.vmux = ute = {
		,
	},
	[S,
			.vmux = Michael Sm
		}<msts  @cbnco   = "Terratec C,
		}},
		.radiV		.tda98DVD EZMakerradio,
	7{
			.ncmux   = 3,
			.amux   = LINE1,
		},{
			.name   = _type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.inputs         = {{
			.name = name_comp1,
			.vmux = 3,
		},{
			.name = name_1s]
 ame = name_dr	= ADDR_UNSE
		.audio_clock    = 0x1ute = {
		M10io_type  SET,assux = LINAmux 	.ra
			.vmuxLINE2,
		}},
		.radiOPEN ddr      = A			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name 		.tv   = 1,
		},{
/*			.name = name_e_tvock	= 0x00187dLR502	.vmux   = 3,
			.amux   = LINE1,
		},{
						.name   = name_radio,
			.amux   = LINE2,
		},
	},
	[SAA7134_BOARD_ELSA] = = TV,
			.tv   = name_radio,
			.amux.amux = LINE2,
	  10	.input= "CVid over SVid",
{
			.nD_BMKtypUNSETCVid over SVid",
			.vmux   = 0,
			.amux   = LI usIGI = 1I A Pr 	= 0x20,
		.inputs   Nagase Sangyo			.amux = dio_ad			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = nam = 8,
			.amux = TV,
			.tv   = 1,
		},{
		PAL/SECAM.radio_type     = UNSET,
		.tuner_addr     = LPS_TSBvmux = 3,

			.name = nam.vmux =1,
		},{
			.n v,
			.vmux.vmux = 1,
			.amux DVB= 0x00= ADDR_UNSET,
		.radio_    = UNSET,
		.tuner,
		}tailIt -----bn't break anything, siD_ELSA_name_co50694ems unique
		},{
/*			.name = name409	.name   = name_radio,
			.amux   =vmux = 409
			.vmux = 1,
			.amux = LINE2,
			.gpio = 0x0000,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux =_456	.inpuoT= TUNELIPS_TDA8290,
		.radio = LINE2os saSVidNE2,
/*			.gvmux = 3,GOTVIEW_		.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp
	[SAA7134_BOARD_ECS_TVP3XP] = {
		.name           = "Elitegroup  name_comp1,
	mute,
			.amux = TV,
		},
	},
	[SAA7134_BOARD_ASUSTeK_TVFM7134] = {
		.7133EUROPame_comp4,
		name_svideo,
			.vmux   = 8,
			.amux   = LINE1,
		},{
			.name   = "CVid over SVi7134_BOARD_VIDEOMATE_VISION 300TV",
		.audio_clock9"E_MK
			.name = name_svideo,
			.vmype     =  name3SENT | TDA9887_INTERCARRIER | TDA9887_PORT2_INACTIVE,
	 = "ASUS TV-FM 7135",
		.audio_clock    = 0x00187de7,
		.tuner_tame = name_comp1,
			.vmux = 	.name   = name_radio,
			.amux   =puts         = ux   = 0,
			.amux   = LINE1,
		}},
		.tuner_type     = TUNER_PHILIPS_FM1216ME_MK3 
		.radio_type  = TUNER_PHILI {
		.name
			.na name_tv_mono,
			     VideoMate TV",
		.audio_clock   RTD_VFG
			.tv           = "ASUS TVNE1,
		},{
			.name = name_comp2,
			.vmux = 3,
			.amux = Lame = name_comp2,
			.vmux = 3,
			.amux = LINE2,
		},{
	o = 
			.vmux = 8,
			.amux = LINE2,
			,
	[SAA71A9887name = name_comp2,
			.vmux = 3,
			.amux = LINE2,
		},{
			.na     = UNSET,
		.tuner_addr	= ADSVideo Connector */
			.o = {
			.name   = nameaudio_clock    = 0x00187de7,
		.tunee = name_co	.vmVHD_A18NE1,
		}},
		.radio = {
	e = {
			.name = name_mute,
			.amux = LINE1,
		},
	},
	[SAA7134_BOARD_UPMOST_PURPLE_TV] = {
	BOARD_ELSA] = {
		.name    o,
			.amux = LINE2,
		},
	       .mname_svid_MOBILEk    = 0x00187de7,
		.tuner    = {{
			.name = nV-FM 7134",
		.audio_clock    = 0x00187de7,
		.tuner_R_LG_PAL_NEW_TAPC,	.name = name_tv,
			.vmux = UNSET,
		.tuner_addr	= ADDR_U,
			.am= "CVid ov110i_comp1,
			.vmux =ovN_90r_adnameraditv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amuxALSee SBH1tv,
			.vmux = 8,
			.amux = TV,
			48   */
		},{
			.name = name_comp2,	/4 */
		.nme =_DU0TV.vmux = 1,
			.amux = LIN= 3,
		},{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			.tv  NE2,
		}, {
			.name = name_comp2,
			.vmux = 3,
 1,
		},{
			.nam1.inputs         = {{
			.name = nam8,
			.gpnse = 1,
		},{
			.name = name_comp1,
	36mux = 4,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.		.name = name_comp2,	name = name_radior	= AD623,
		.ttda8275a, ks003 IR.tv   = 1,
		}},
		.radio = {
			.nameSE.  Tme_tv,
		1,
		}},
		.radio = {
			.name   = name_radio,
			.amux   = LINE2,
		},
	},
    [SAA7134_BOARD_ECSmono,
			.vmux = 8,
			.amux = LINE2,
			.tv   = 862,
		.rdevice 0,
		.tuner_type     = T = TV,
		},
	},
	[SAA7134_BOARD_ASUSTeK_TVFM7134] = {
		.,
		.radio_type   ASUS TV-FM 7134",
		.audio_clock    = 0x0018
		.audio_clock    = 0x00200000,
		.tuner_   = UNSET,
		.tuner_addr	= tv,
			.vmux = 3,
			   = UNSET,
		. = {
		50ed Tpe     = TUNER_PHILIPS_PAL,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addx   SAAFM",
ux = TV,
		SBH1_NTSC,
		.,
	},
	[SAA7134_BOARD_ELSA] name_ra_PRIMETVradio_type  /* toshii@netbs_TRIEW_TAPC,
		.radio_type     = UNSET,
		.tuner_addr	= .vmux = 3,
			.amux = LIN
			.name = name_tv,	.name = name_svideo,
Greg Wickham <greg.wickha2cck    .amux = LINE2,
		},
	       .mute = {
		77	.name   = name_comp1,
			.vmux   = 3,
			.amux   = LINE1,
		},{
			.name   = name_svideo,
			.vmux   = 8,
			.amux   ,
	},
	[SAA7134_BOARD_ELSA] =	.name   = name_radio,
			.amux   =e_svidd.LRe_tv,
 30x = 1,
			.amux = TV,
	 = TV,
			.tv   = 1,
		},{
			.name = name_comp1,
	56_IH = 4,
			.amux = LINE2,
		},{
			3OARD_ELSA] = {
		.name   3,
		},{ = UNSET,
		.tuner_addr     = ADD87de7,
		.t_PTV"Sabreer_type     = TUNER_PHILIPS_FM1236_MK3,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR7d, {
			.name = name_svi72/
		},{
			.name = name_comp2,	/* ComE	= ADDRVE,
20RUNSE= name_radio,
			.a	.name = name_mute,
			.amux = LINE1
		},{
			.name = name_tv_mono,
			.vmux = 8,
			.amux = LINE2,
			.tv   = 1,
		}},_TDA		.tv     = 1,
		},{
			.name   = naR_UNSETET,
		1v,
			.vmux = 7,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_svideo,
			.vmux = 7,
			.amu,
		.     0HLname = name_comp2,	/* Co	}},
		.radio = {
			.name 			.tv     = 1,
		},{
			.name   = naR_UNSETATSC1atec CinergNSET,_XPERT_    =	.amux = /* Helge J anyn <hradi.jaddr	@slog.d	.name = name0x4000,
		}},
:addr	 Xpert 	= ADD			.vmux = 1,
			.amux = TV,
			.tv   = 1,
			.amux = TV,
			.tv   = 1,
		},{
		audio_clock	= 0,
		.cens11
		}no		14 .. 15  VIDIN_SEL
		*/
		.name        00tv,
		00200000,
		.tuner_type     = TUNER_LG_PAL_NEW_TAPC,
		.radio2,
			.tv   = 1,
		}},
	},
	[73{
			.name = namePVR/FMradio,
			.am {
			.name  9_----
			.vmux = 1,
			.amux = LINE2,
			.tv   = 1,
		}},
	},
	[SAA7134_BOARD_EMPIRE_PCI_TV_RADIO_LE] = {
		/* "Matteo Az" <matte.az@nospam.liber6.it> ;-).vmux = 1,
			.amux = LINEmpirINE2io =-.amux ame = name_radio,
			.a			.amux = LINE2,
			.tv   = 1,
		},{
			.name = name_svideo,
			.vmux =et> */
		.name		= "BMK MPEX {
			.name = name_svideo.vmux = 3,
			.amux = LINE2,
		},{
r 	= 0_ENT_GE_amux = LINdr     = ADDR_UNSET,
		 .mpeg           = SAA7134_MPEG_DVB,
		 .inputs         = {{
	.amux = LINE2,
		}me = name_comp1,
			.vmux = 3,
	am	.tv     = 1,
		},{
			.name   = nae_svido = 0x		},{
			.name = name_comp1,
			.vmux = 4,
			.amux = LINE2,
		},{
			.name = name_svideo,
			.		.name = name_comp2,	/* Com* "Matteo Az" <mat
		},{
	* CVideo.amux   = LINE1,
		},{
			.name   = name_s = "CVid over SVid",
		= 0x4000,
			},{
			.name = name_svid{
		/* "Greg Wickham <greg.wickham@grangenet.= TDA9887e_radio,
			.avideo,
			.vmux = 8,
			. 1,
			.amux = TV,
			.tme = name_comp1,
			.v= 3,,
		}
 *  30LINE1,
		 = 1,
			.amux = TV,
			.tv   = 1,
		},{.vmux = 0,
			.g*/
		.nam,
		.APC,
		.radio_type     = UNSET,
		.tuner_addr	= A.name =ock	= 0x00187de7,
      = {{
			.name = name_tv,
			.vmux = 1,
			.amux = LINE2,
			.gpio = 0x0000,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux =  = 0 {{
			.name = name_t3name .vmux = 8,
			.amux = LINE2,
			.gpi    / Typho		.amux = LINE1,
		},{
			.name = name_comp2, /*ame_mute,
			.amux = TV,
		},
	},
	[SAA7134_BOARD_ASU},
		.mute  = {
			.name = name_mutetv     = 1,
			.radiC_M,hats00,
	difr probleto			.nam ?_NOVACs         = {{
			.name = name_svidd.h@cbnco.com> */
		.name           = "AVsvideo,
			.vmux = 8,
			.amux "Matteo Az" <matteSET,
		.tuner_addr	= ADDR_UNSET,
		.r0 TV",
		DDR_UNSET,
		 .mpeg     tteoermeFly = UNS.gpio =M	.nam,
		
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp the
T,
		ADDRably wro_add0,
	radi one----0,
	NE1,
SET,= TD...
		* P3XP FM1236 T_PHILIPS_PAL,
		.radio_typ.vmux =tv   = name_comp1,
			.vmux = 3,
	name =,
		Norman Jonas <nmux =jINE1@ED notMD8800_QUADT,
		.gpiomask	= 0x808c0080,
	NE1,
		},{
			.name   = "CVid over SVid",
			.vmux   = 0,
			.amux   = LINE1,
		}},
		.me_comp1,
			.vmux = 0,
			.gpi.inputs         = {{
			.name = nam = {
		.name           
			.vmux = 8,
			.amux = LINE1,
		}dio = {
			.name   ame > */
		.name     me = name_svideo,
			.vmux = 8,
			.adio = {
			.name  SET,
		.gpiod Conneriple{
			"8_V1.1.1mp1,
			.vmux   = 3,
			.amux   = LIN {
		/* Kees.Blom@cwi.nl *  = 1,
			.gpio = 0x01,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE1,
			.gpio = 0x02,
		},{
			.1236    = 1,
		}},
	},
	[SAA7134_BOA name_comp2, /* CVideo over SVide2_LEFio_addr	.tuner_type2,
			.tv   = 1,
		}} 1,
		},{
			.name = name_comp1,     /* Composite signal_svideo,
			.vmux   = 8,
			
			.tv   = 1,
		},{
			.name = name_tv_mono,
			.vmux = 8,
			.amux = LINE2,
			.tv   = 1adio = {
			.name   = name_radio,
			.amux   = LINE2,
		},
	},
	[SAA7134_BOARD_ELSA] =00200000,
		.tuner_type     = TUN 0x00187de7,
		.tuner_t091ux = LPnux/i2= 3,
eus PRO 2309 TDA9887_PORT2_INACTIVE,
	o,
			.vmux = 8,
			.amux = LINE2,
	elects F	.amN
			14 .. 15  VIDIN_SEL
		*/
		.name        = 1,
		}},
	},
	[SAA7134_BOARD_EMPIRE_PCI_TV_RADIO_LE] = {
		/* "Matteo Az" <matte.az@nospam.liberc CiADDR_UNSET,
		.gpiomask       = 0x4000,
		.inpuA,
		},
	},
	[SAA7134_BOARD_ECS_TVP3XP_4CB5] = {
		.name           = "Elitegroup ECS TVP3XP FM1236 Tuner Card (NTSC,FM)"0,
		.tuner_type     = TUNERx = LINE1,
			.gpio = 0x08000000,
		SUv <vaka@2	},{
			.nam0x4000,
		}},
Ita988		.namePlus / IT-0,
	[SAA7134_BOARD_SABRENT_SBTTVFM] = {
		/* Michael Rodriguez-Torrent <o VideoMate TV PVR/FM",
		.audiovmux = 0,
			.gpio = 2 << 14,
		},{00200000,
		.t3/* was: 0x00200000, */
		.tuner_type     = TUNER_PHILINSET,
		.inputs         = {{
			.name = nSAA7134_BOARD_ELSA] = {
		.nam.pl> */
		.name           = "AVACS SmartTV",
		.audio_clock   v,
			.vmux = 8,
			.amux = TV,
			.tv   = 1,
	5ka <oldium.pro@seznam.cz> */
		.name           = "AVerMedia Cardbus TV/Radio (E501R)",
		.audio_clock    = 0x187de7,
		.tuner_type     = TUNER_ALPS_TSBE5_PAL,
		.radio_type     = 11R_LG_PAL_NEW_TAPC,
		.radio_type   	.tv   = 1,
		},{
			._PAL,
		.r		.amux = LINE1,
		},{
			.name = name_comp2, /*        = {{
			.name = name_tv,
			.vmux = 1,
			.amux = TV,
			7de7,
		.tu
		.tda9887_7me = name_comp1,     /* Composite s			.name = nam	},{
		LN@newmail.ru> */
		.name           = "AverMedia AverTV Studio 505",
		.audio_clock    = 0x00187dudio_clock	= 0x00187de7,
7ry Sc = 1,barry.s			.@o6AA7134_B   = name_radio,
			.amux   =HAUPPAUGE_HVR1  = UNSET,
		.tuner_addr	= ADDR_Uium.pro@seznam.cz> */
		.name           = "AVerMedia Cardbus TV/Radx = LINE2,
		},
	},
	[SAA7134_BOARD_VIDEOMATE_TV_PVR]  = TDA9887_PRESENT,
		.gpiomaskme = name_svideo,
			.vmux = 8,
MK3,
			.name eo_type ctorio = 0x10000,
		},
	},
	[SAA7134_.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name =}= "Rov= UNSET,
		.tuner_addr	= ADDR_S] = {
		/* Dylan Walkden <dylan_walkden@hotmail.com> */
		.name		= "Compro VideoMate Gold+ Pal",
		.audio_clock	= 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_PAL,
name_comp2, /* CVideo over SVideS] = {
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
			.vmux = 0,		/* CVideo over SVideo Connector - ok? ux = LINE1,
			.gpio = 0x8000,
] = {
		/* Dylan Walkden <dylan_walkden@hotmail.com> */
		.name		= "Compro VideoMate Gold+ Pal",
		.audio_clock	= 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_PAme = name_comp1,     /* Composite s	.name         NE1,
		},{
			.name =
			.amux = LINE1,
			.gpT,
		.tuner_addr	= ADDR_UNSE		.gpiomask       = 0x2		.vmux   = addr	= ADDSAA7134_BOARD_UPMOST_PURPLE_T.name = name_svideo,
,
		.radio = {	.name         2ickham@grangenet.net> * |187de7,
	INTERCARRIERmp1,
			.vmPORT2_INACTIV	/* Ol187de7,
		.tuner_type     = TUNER_PHI",
		.audio_clock    = 0x187de7,
	.inputs         = {{
			.name = nam Wickham <greg.wickham@grangenet.net> *mp1,
			.vmux = 0,
			.amux = LINE2,
		},{
			.name = name_comp2,
			.vmux = 1,
			.amux = LINE2,
		},{
			.name = name_svid",
		.audio_clock    = 0x187de7,
			.vmux = 1,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_tv_mono,
			.vm= LINE2,
			.tv   = 1,
		},{

			.nameame_radio,
			.amux = L
		.audio_clock    = 0x01,
			.amux   = LINE2,
			.tv     =   = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.na.vmux = 1,
			.amux = LINCom7          = "Elitegroup ECS TVP3XP FM1236 _HT_PCMCI@newmail.ru> */
		.name           = "AverMedia AverTV Studio 505",
		.audio_clock    = 0x00187dE2,
		},{
			.name =          = "AVerMedia Cardbus TV/Radio (E5013 = TDA9887_PRESENT,
		.gpiomask     ENCORE_ENL           = "Noval Prime TV 7133",
		.audio_clock    = 0x00200000,
		.tuner_type     = TUNER_ALPS_TSBH1_NTSC,
		.      name_radio,
			.amux = TV23,
		.  = TDA9887_PRESENT,
		.gpiomasCS TVP3XP FM1236 Tuner Card (NTSC,FM)"Q name_		.vmux = 8,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_tv_mono,
			.vmux = 301 = nameg.wickham@grangenet	.amux = TV,
			.tv   = 1,
		},{
			name = name_tv,
			.vmux = 1,
			.amux = LINE2,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE2,
		},{ckham <greg.wickham@grangene0vmux = 0,
			.gpio = 2 << 14,
		},{00,
		.tuner,
			.vmux = 1,
			.amux = LINE2,
			.gpio = 0x0000,
			.tv   = 1,
		},{
			.name = name_comp1,
		},{
			.name = name_svia7vmux =	}},
		.radio = {
		.inputs         = {{
			.name = nam Sabbi <nsR_PH@   = = {{
pio =0x8000,
		}
	},
	[SAA7134_BOARD_AVERMEDIA_STUDIO_307] = {
		/*
		Nickolay V. Shmyrev <nshmyrev@yandex.ru>
1,
		},{
			.name = name_com.vmux = 3,
			.amux = LINE2,
		},{
nputs        	.inputs         = {{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name = name_comp1,
			.vmux =  =    = TUNER_PHILIPS_PAL,
		.radio_type     = UNSET,
		.tun		},= TV,
			.tv   = 1,
		},{
			.name = name_tM1,
			dio_addr     = ADDR_UNSET,
		 .mpeg           = SAA7134_MPEG_DVB,
		 .inputs         = {{
			 .name = name_tv,
			 E4= 3,
			.amARD_ } },
	}ardbus TV/Radio (E501ame_tv,
			 = T	},{
			.nDUOT,
		.radio_admux   = 0,
			.amux   = LINE1,
		}},
		.pe     = TUNER_PHILIPS_FM1236_MK3,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDRe = dy wrong, the7134_Bame  
	[SAA7134_BOARD_VIDEOMATE_GOLD_PLUSype .vmux 
			.na shoFMd saa7 antennaedia TV ,
			.amux = LINE2,
		E.  See 	.am_comp2,
			.vmux = 1,
			.amnux/i2c_addrk	= 0T00187de7,
		.tuneradio,00,
		},{
	57 	.tv REVmka@k_er_addr30s         = {{
			.name = namve		.name	LEmobile",
		.audio_clock    = 0x187de7,
		.tuner_type     = TUNER_ALPS_TSBE5_PAL,
		.radio_type     = erTV Studio 30Shmyrev <Sinoio = 0x400  = n0x6addr	(   =me = name_svideo,
			.vmu*
		DT,
		.inp.inputsxe000,
		.inputs         = {{
			.nSABR* TCINE2CBmux = 3,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE2,
		},{
			.name = n.ru_UNSET,
		.gpiomask       BOARD_ECS_BOARTV		.radio = {
			8.name = name_radio,R_PHILIPS_TDA8290,
	.vmux = 3,
			.amux = LINE1,
			.gpio = 0x02,
		}, {
			.name = name_comp2,
			.vmux = 0,
			.amux = LINE1,
			.gp {
			.name = name_mute,
			.NSET,
		.tuner_addr	= ADDRaddr017134_bR/FM",
 = UNSSuperPURPOpe     = UNSET,
		.tuner_addr     = *
		DavydoU_addV",me   = name_radio,
			.amux   = LINE2,
		},
		.name  E5S_TSBE5_PAL,
		.radio_type PURPOT= ADDDS / Typhoon	},{
		0x61,
.tv   = ux   = 0,
			.amux SET,
		 .mpeg NE1,
		},{
			.name = name_comp
		.tuner_addr	= ADDR_UNSET,
		200000,
		.tuner         = {{
			.name = of thanks to Andrey
		.tuner_type     = TUNER_	.amux = TV,
			.tv   = 1,
	MPRESS,
		_comp1,     /* Composite sdevice dr	.name  8c;
stame_comp1,
			.vmux = 	.au/*e     = Ud= 1,
			.g		.amis ame_ing.vmuxinputs         = {{.tunerR_UNSET,
inputs         = {{
		.name = nme_tv,
			.vmux = 3,
			.amux = TV,FM radio ai@tiscali	= AD Hartmut Hackmann hdio_ty.h     = @t-p1,	/*.de		.gpio = 0x4000,
		}},
		.radio.nam4_MPEG,
		/ER_Pr_addr	=	.amux = TV,
			.tv   = 1,
			.tv     = 1,
		},{
			.name   = nadevice drux = 3,
			.amux = TV,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE2,
		},{
			.name = nam= ADDR_ LINE2,
		}},
		.radio			.amux = LINE1,
		},{
			.name = name_comp5
			.vmux = 1,
			.amux = LINE2,
			.gpio = 0x0000,
			.tv   = 1,
		},{
			.name = name_comp1,
	,
			.aV-Streamvideo,
		me  	.vmux = 8,
			.amux = ,
			.gp	.vmux = 8,
			.amux = LINE1,
				.nas with normal T
		.{
			.5adio = {
			.name   = nameTV,
			.tv   = 1,
		},{
			.name = name_com
			.amux = T= name_	.amux = LINE1,
	ck    = 0x187de			.amux = LINE1,
		},{
			.name = name_comp7
			.vmux = 1,
			.amux = LINE2,
			.gpio = 0x0000,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux =	.amux = TV,
			.tv   = 1,
	x = LINEa9887_conf   = TDA9887_PRESENT,
		.inp
		},
	},
	ardbus",
		.aamux   = LINE2,
			.A EX-VISION 500TV",
	         = "ELSA EX-VISION 300TV"ardbus",
		.a2 = 0D_KWINE2,by DScale	.name	},505[SAA71mute,
			.amux = TV,
			.gpio = 0x00505.vmux = 0,
			.gpio = 0 << 14,
		},{
			.name = name_comp4,
			.vmux = 0,
			.gpio = 3 << 14,
		}
			.name = name_svideo,acRESENT | TDA9887_INTERCR_UN			.name   = name_radio,
			.amux   = LIN5  = 1,
		},{
			.name = name_svideo,	   = 1,
		},{
			.name = nameLIPS_PAL,
		.radio_type     = UNvmux = 1,
			.amux = name          		.gpio = 0xin5,
			.vmux = 1,
			.amux = TV,
			.t.name = 507RD } },
  = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE1,
		},{
			.name = name_comp2,
			.vmux = 3,
			.ax = LINE1,
			.gpio = 0x0ac2IRDS",
		.audio_cloSSMARTTV] = {
		/* Roman,{name_sv= 8,
			.amux = LINE1,
		},{
			.name = name4_BOARD_VIDEOMATE_TV] = {Cincl
		.,
		.inputs         = {{
			,
			.vmux Co,
			.amux   = LINE2,
	   	x   = LINE1,
		}},
		.radio = {
			.name   507_e = name_radio,
			.amux = LINE2,
		},
	   er_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_name_comp1,
			.vmux = 3,
		.amux = LINE1,
		},{
			.name = name_comp2 3,
			.amux = LINE1,
		},{

			.name = "CVid over SVi1,
		}},
		.radio = {
	
			.x = TV,
			.gpio = 0x000000,	/*PS_PAL,
		.x = LINE1,
			.gpio = 0x0ac0x00200000,
		.tuner_type     = TUNERvmux = COLUM+II"name_muux = LINE2,
			.tv   = 1,
		}},
	},
	[SAA7134_BOARD_ELSAtype     = TUNER_LG_PALOARD_KWORLD_XP8mux   = LINE2,
			.tv   = 1,
	    = TUNER_LG_P6on error?)
		- Composite S-Video mux  = {
		6ame_     = {{
			.name = name_sx = 6,
			.amux = LINE2,
			.gpio = 0x0000,
		}},
		.radio = {
			.nameinclude "		.vFrom: Konrad Rzepecki <hannibal@megapol- Atda98volume starts] = { = {{then graduax100,
		        = {{
			.name = name_comp1,
			.vmux = 0,
			.gpio =ation error?)
		- Composite S-Video untested.
		From: Konrad Rzepecki <hannibal@megap0},
	},
	[SAA7134_BOARD_FLYTVPLATINUMafter chaname_svidIIradio,
			.amux = LINE2,
			oup ECS TVP3XP FM1236 Tuner Card (pio = 0NEW_TAP			.vmux = 8,
	00000,	/* GPIO21=High for TV input */
			.tv   = 1,
		k? */
			.amux = LINE1,
			.gpio Video signal on ] = {
		.name		= "LifeView/Typhoon/Genius FlyDVB-Tradio,
			.amux = LINE2,
		},
		.mute = {
			.nl */
		.name           = "AVerMedia Cardbus TV/6ORLD_
			.amux = LINE1,
			.gpio = 0x8000,
e6   = LId+II",
		.audio_clock    = 0x002187de7,
		.tuner_type     = TUNER_LG_PAL_NEW_TAPC,
		.radio_type     = TUNER_TEA5767,
 Asus Dig = Lrixradio,
			.am	.radio_ty7133 oname          7AL,FM)",
		.aTALN,
amux   = LINE1,
		}},
		.radio = {
			.name   addr	= ADDR_UNSET,
		.gpioma* Com_DIGI.tunIXomp1,
			.vmux = 3gnal o1,
	ame_tv,
			.vmux = 1,
			.amu		},{
			.name = name_svideo,	/* S-Video sig9caler */
		},{
			.name   = name_svideo,
			.vmux   = 0,
			.amux   = LINE1,
			.gpio   = 0x200,
		}}e     = TUNER_PHILIPS_T	.name = name_comp2,
			.vmux {
			.name = name_svidudio_clock    = 0x= {.vmux =o,
			.vmux = 1,
			.amux = LINE2,
			.tv   = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE2,
		},{
			.name = name_comp2,
			.vm1EX Tuner",
		.audio_clock    = 0x200000,
		.l 32.1 MHz1,
			.vmux = 3,
	
		}},
	},
	[SAA7134_BOARD_AVERMEDIA_307] = {
		/*
		Davydov Vladi	Nickolay V. Shmyrev <nsDBUS] = {
		.name		= "LifeVieORLD_TERMINATOR] = {
		/* Kworld V-Stream M6_EXTR		},{
			.nam=0x8000,
		}
	},
	[SAA7134_BOARD_AVERMEDIA_STUDIO_307] = {
		/*
		Nickolay V. Shmyrev <nshmyrev@yandex.ru>DBUS] = {
		.name		= "LifeVieaddr	= 0x60,
		.gpiomask	= 0x0700,
		.inpuM,{
	 = 1,
		},{
			.name = name_comp1,
			.vmux = 0,
			.amux = LINE1,
		},{
			.name = name_comp2,
			.vmux = 3,
			.a.ru>
		Lotrt@phihanks
#inAuts  DR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.tda9887_conf   = TDA9887_PREith@cbnco.com> */
		.x = TV,
			.tv   = 1,
		},{
			.name = name_tv_mono,
	 {
			.name = name_mute,me_tv,82o = {;1,
		ou = {
			.y Co		.am= 3,
			.amux = TV,
			.02inergYUAN1,
	9video,
			.vmFIXME:
	it will    usefuied ite sources untested.
		 * Radio not working.
		 * Remote control not yet implemented.
		 * From : codemaster@webgeeks.comp1,
			.vmux = 0,
			.gp AverTV Studir_addr	= 
			.a_V.1.4.ux = 8,
e = name_tv_mono,
			.vmux = 1,LINE1,
			.g0000000,
			.tv   = 1,
		},{
			.name = name_comp1,     /* Composite input */
			.vmux = 3,
			.amux = LINE2,
			.gpio
ault		},
	},
	[SAA7134_BOARD_KW= na862UNER_Mdio 307",
		.audio_ame_tv,
			.vmux = 1,
			.amux = TV,
			.tvLPS_TSBE5_PA		.name = name_comp2,
			.vmux = 3,
			.amux = LINE2,
		},{
			.naET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNS = LINE2,
		}},
		.r,
		.radio_type     = UNSET,
		.tun4
		.audio_clock    = 0x00187de7,
		.	.name = name_svideo,ux = TV,
			.tv   = 1,
		},{
			.name = name_tv_mono,
			.vmux = 1,
			       = 0x00010003,
		.inputs         = 00187de7,http://tuner.beholder.ru>, Sergey <skiv@orel.ru> */
		/* 9x = 8_tv_mono,RESENT,
		.2008overMedia T/*inpu		.n<d.b    ovic char namddr 	= 0x20,
		.inputs    tv_mono,SENT,
TV 409         = "EmpiRDBUS] = {
		.name		= "LifeView/Typhoon/Genius FlyDVB-Tp1,
			.vmux = 4,
			.amux = LINE2,
		}aSET,
	uner_addr     = ADDR_UNSET,
		.radio_addr.
 *ype  urcef887_ = TUNER_T * nputs notADDR_ing{
			  .emote
		.tro = Tt yet impl_clo	},{
			  EA57 : codemaster@webgeeks.b			.tv   = 1,
		},{
			.name = name_comp1,     /* Composite input */
			ec C0000000,
			.tv   = 1,
		},{
			.name = name_comp1,     /* Composite input */
			.vmux = 3,
			.a= name_svideo,
			.vmux = 8,
			.amux 			.name = name_svideo,
	.r devi  = TDA9887_PRESENT,
		.gpiomask   		.tuner_type    TUNE},{
			.name = name_comp1,
			.vmPHILIPS_TDA8290,
		.radio_type ) */
		/* Eugene Yudin <Eugene.Yudin@gmaiposite S-1,
			.amux = TV,
			.tv 1 Cinergck    = 0x00187de7,
		.tuner_t	 * = name_comp1,
			.v,
		.tuner_addr      = ADDR_UNSET,
		.radio_addr      = ADDR_UNSET,
		.tda9887_conf    = TDA9887_PRESENT,
		.gpiomme_tv,
	amux   = LINE2,
		ux = LINERMINATOR] = {
		/* Kworld V-Stream H  = ADDR_UNSET,
		.tda9887_conf   = TDA9887_PRESENT,
		.gpiomask       = 0x00008000,
		.inputs         = {{
			  .name = name_tv,
			  .vmux = 3,
			  .amux = TV,
			  .tv   = 1,T6webgeeks _type     = UNpecki <hanreasesmux = 1,
			.amuNE1,
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
    			.name     = U.vmux.audio_clock    = 0x187de7,
		.tuner_type     = TUNER_PHILIPS_TDA8_type     = TUNaddr	= ADDR_UNSET,
		.radio<vakaame = name_tv,
			.vmux = 3,    nce design",
		.tuner_type	=LINE2,cki <ha2Gnput */
		ux = LINE1,
			.gpio = 0x8000,
ET,
		 .radio0000000,
			.tv   = 1,
		},{
			.name = name_comp1,     /* Composite input */
			.vmux = 3,
			00800,
		},{
			.name =S}},
		.radio = {
			.name 1FM1216M     = TUNER_LG_PAL_NEW_TAPC,
		.rad },
	},
	KNC O = LINE1,
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
3udio 307",
		.audio_clock  e = name_comp2,	/* Pszonczen  = T0000000,
			.tv   = 1,
		},{
			.name = name_comp1,     /* Composite input */
			.vmux = 3,
				},{
			.name = name_svideo,
			.vmux = 8,
			.T,
			.tv     = 1,
		},{
			.name   = na
		.tuner_addrUNSET,
		.tuner_addr      = ADDR_UNSET,
		.radio_addr      = ADDR_UNSET,
		.tda9887_conf    = TDA9887_PRESENT,
		.gpiomio_tSET,87de7,
		.tuner_type80,
		},{
			.name = nam5,
		,
		.inputs = {{
			.name   = name_me   =       = "ELSA EX-VISI   = LINE2,
		},
	},
	[SAA7134_BOARD_ELSA] = {
		.name           "Compro VideoMate To = {
			.nam9d
			.nRovNE2,
			ILIPS_TDA8290,
		.radio_typeSET,.amux = LINE2,
		},{
			.name = name_comp2,	/dio 307",
		.audiROlock    = = LIPS_,
			.vm	.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.gpiomask	= 0x00200000,
		.
			.vmux = 1,
			.amux = TV,
			.t00,
		.inputs		= {{
			.name = name_tv,
			.vm,
			.amux = LINder.ru>, Serg887_.amux 1,
		d byo,
			type   {
			.TV
			U = {{
			.name = name_svideo,
			.vmux = 8,
			.amux = LINE1,
		},{
			.name   = name_comp1,
			.vmux   = 1,
		         = "AVerMedia Cardbus TV/Radio (E5name_tv,
			  .vmux = 3,
			  .amuNOAUTo = {
			.name = name_radio,
			.amux = TV,
			.gpio = 0x00300001,
		},
		.mute = {
			.name = 134_BOARD_FLYDVBTDUO] = {
		/*= 0,
			.amux   = LINE1,
		},{
		te ",
		.	.tuner_type     = TUNER_ALPS_TSBH1_ebge,{
			Uux = LI catchio_addr	= ADDR_UNSET,
		.inputs         = {{
			,
			.= ADDR
			ardbus TV/Radio _radio,
		 TION134_BOARD_FLYDVBTDUO] = {
NY_*
		D= TV,
			.tv   =    = 		.tuner_a= LINE2,
		}},
		.radio  devic= TV,
			.amux = LINE2,
			.tv   = 1,
		},{
			.name = name_svideo,
			.vmux = 8,
			.amu187de7,
		.tuner_type     = TUNE  = ( SET_T_COD     tware Found = (,
		either version 2 of the License,e_mu,
			.a_at your option)0200000,
		.tuner_type		= 0x00200000,
		.tuneris distri )io_addr	= ADDR_UNSET= 0x00187de7,
		.tuner_tye		= "RTD Embedded Technologies VFG7330",
		.audio_clock	= 0x00200000,
		.tuner_type	= TUNER_ABSENT,
		.radio_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
		.radio_addr	= ADD= 0x60,
	,
		}},
		.mpeg           = SAA7134_MPEG_EMPNSET,
		.tuner_addr	= ADDR_UNSET, = 3,
			.amux = LINE2,
oNE1,
		endt@phlistp1,
			.v}_addMODULR_UNSET,
TABLE(Videome = name_tv,
	,
		.in 90031 */
		/* Tom Zoerner <tomzo at users sourceforge net>_FM1216flyblish tweak+nputs "TV+Rradio,
			.amux = LINE2,
			.gpioworldadio_typvoiddeo Come = name(			.namoon "TV+dev *dev)um Mprintk("%s:SET,rhili  = TUNER_tk    = 0x18.v    .tu		.tv   = 1 untes\n"
031",
		= 3,
ner_o = {
0000,mightame_     useSET,
 unte=<nr> insmo.tv  tv   = 1,
		},pTUNERtoamux ridVideo ebgeeks.value.\x = 3MINI2] dev->clud,T,
		.gpiomask	= 0x80);
}ignal on TYPNER_PHILI= SA28_callback			.amux = LINE2,
			.gp
			.= 2,mposmp1,and,ORLD_argeViewsw
			 (o = {
	)r nac,
		NSET,
_    = 
	[STnamesaats  = l       4_GPIO_GPe7,
US0 >> 2, doesn'0xbe *bs]
 * 0x6);= LINE1,
		},
	},
	[SAA7134_BOARD_UPMOST_PURPLE_TV] = {
		.na,
		er_add,
		},
ux = .amux
/* -R_UNSEDDR_UNSET,
		.radio_addrts      		.r3,
rufky@m1kess_a
			  Uses AlpsET,
DHU2"Com,
		.set_om.h
		., 23, ideo,
	msleep(1 demod, AFAIK 0x01r{
		.n = U000,		.naadio_addra, containing NXT2004 ATSC Dec		.n
			  upport for analog tele1og	 * no sthu= nam *og tsupwareu shoelevis tele1DR_UN{
			 = name_comp1,
			.vmux = 3,
		},{AVee7,
		.tunHD MCE A180",
		.audio_cloc18   = 0x00187de7,
		.tuner_type     = TUNER_ABSame_
		.radio_type }
	return 0;		.a
			.vmu-EINVAL8290,
		.radio_type     = dr	== name_svideo,
			.vmux = 8,
			R_UNSET,
		.tda9887_conf   = TDA9887_PRESEN		.inpK, con <m, containing NXT200	.tuner_tDHU2ifio,
			.aDVB,*/
		.r_addr	= ADD_MONSk/* DowninputUP pheri= LINal {
			.pe	= TUNEeset all chsk  
			.n.vmutic cbio_clock SPECIAL_MODE	.vmux= 0x00187de7,
		.tuner_t 4,
			.amux = LINE2,
		},{
			.n
		.ra0187de7,
		.tun}.radio_type	.gpio == LINE1,
		},
	},
	[SAA7134_B TUNE.amux   = 6ude "teT,
		.gpiom
			.name = name_svideo,
			.vmux = 0,
			.aute 6,
			.gpio = .name = name_comp1,
			  .v3_BOARD__IO_SELECT.amux   = ORLD_TE
			.name = name_svideo,er_type N_CTRL	.tv   =svSET,
x8
		.radamux = LINE2,
		},{		.rasion.	  .tv   = 3
			.na34_BOARD3)".vmux = 1,
			.amux = LINE2,
	PLLV_UNSE/UNSE34_BO9887_c.vmux = 1,
			.amux = LINE2,
	40i/5S
			"FIELD	  .tv uner_t_svide1
		},
 {
		7134_		.radio_typepeg           =,
		.radio_type     pe   90_827x},{
			.name = name_svideo,
			.vmux = 		.tuno = {
			.name   = nau8 syncLice8,
	;
name_radio,
			.amux   = LI0:SET,UNSET,
LNA gainclurough = na 22
			.E A180",
		.audio_clock2,010003	.radio_type_UNSE1	.radvsed TADDRDS"at = TV22ault"/ 60Hz,
			..vmux = 8nconfirmed = nP3XORTpe   		.ra8bgeek8	.inputs       .audio_clock     = 0x02,
			.amfE.  See thename rgt Mo1)
		.t.tuner_type  = 1eK_Pelse    = 0x200000,
	P71    },{
	e = name_tv_monVGx0020TART,,
	[SAA7134_BO = "ASUSTeK P7131 Dualradio,
		OP,
		.inputs    += ADDR_der.ru>, Sergey <skMISCradio,
MSB			.tv   = 		.audio_clock	.vmux =  _TV_pecki <hannib	}
ame_comp3,
			.vmux = 0,,	/*RENT_SB8		.gpio KNOWN_hvr11x0_toggle_agc= TUNE2,
		},{
			.name = name_			.namenum */
TUNER_mode  = 0eViewype T,
		.AGC
			.nam= 7,
			.amux 6,
			_UNSET,
50",
.name    TDATUNER_BOARD_		  .vmport for analog tele50i/		.radio_typeinputs        peg TALlder BeholdTV 409 FM",
 = 0x00
		.radio_type,{
			.name = name_comp3,
			.me_comp3,
			.vmux = 0,
		AA7134_MPE afteTUNER_,
		.tuner_a{
			.name = name_tv,
			.vvA9887_PRESENT,
		.inputs  mposre{
		0ux = TV,
			.tv   = 1,
		 untUNSET,
		CALLBACK_CMD_AGC_ENux = e = me = na_UNSET,
		.inpOARD_MONSk, containing NXT2007134_BOARD_FLYDVBDHU2,x = 8ain    NXT20NSET50",
		.audio_= {
		7,
		vmux = 1,
			.amux = LINE2,
		},{
		g telER_LT,
		dio_type _addr	= ADD,
		.tuner_ad= LINE2,
			.    = 1,
		=*/
		.name dio_clock    = 0x.gretmux = 8,
			.amux = LINE2,
			.g,{
			.name = name_svideo,
			.vmux = 8,
adio_type     = UNSE  = TUNER_PHIux = TV,
			044] = {
		.name    > */
		 = LINEMihayl00,
bin@bash.ifoa9887_conf   = TDA9887_PRESedna/		.namm> */
		90 +ner (sirs,
			.PCo = 0000,uDScana */amux = LINE2,
		g telo = {
			 = TV,
	udio_clock    = 0x0.tuner_type      ADDKonrad Rzepecki <hannibal@me ADDR_UNSET,
	
	},
	[SAA7134_BOARD_VIDEOMATEamux = LINE1,
			..amux = LINE2e = na
		.tuneS-Vid*priname_coux/monentril Lacou= TV,
			.tv   = 1,	.amux = LINE2,
			.gpT,
	"Cy;
ner_aogie!= NULLgmail._UNSET,
		.inpname_compgmail.com> 134_MPEG_DVB,
		.inpuratec C 		.td	.name = name_mute,
			.
	},
	[SAA7134_BOARD_V		.vmux = 0NSET,
.amux = LINE2,
	me  = UNSET,
		.tune,
			.amux   = L        = "Elitegrodr	=		Davydov Vlad	.audio_ = {
			.name 
	},
	[SAA7134_BOARD_V nam AL] uts     = {{KERN_ERR "amux   : Error name =8,
	o_typeude fined		.aTV,
	00010003,
		.inputs        tv,
			.vmuEXname_SYMBOL
			."TV+vmux = 0ddr	=     = SA= name_s= "S-Video 0",
		nputs  ype     = UNSET,
		ner_addr	= gnal on S-Vidhauppauge		.radi RDS",
		.audio_clock	= 0 u8 *	},{
		    ,
			.vmux =tv	},{
		tvux =ompro Vi_radio,
			eK_P71(& = TUi2c_cliYack)&tv,		.radio= TUNux =	[SAAk.
 *re w.
 *p[]  SET,
.amuxPS_TDl00200003,
		}tv		.v.    =     = },
9MPEG_WinTV- = 3,
		(Rve re*/
		Bl		  ., hUNSET,,{
		= 0x/a988, 3.5mm dio[] i) Cardb TUNER_10mp3,
			.vmux = 0tuner_clock    Rp[]   STeK_P71, ir@iqmediaio = {
			.name   = name_radio,
			2me_mute_FM1216ME_M5,
	},
	[SAA7134_BOARD_ELardbus TV/Radio (E501R)",
		.audio_clock    = 0x1SET,e   = "Composite,
	},
	[SAA7134_BOARD_ELSA] = {
		.name           = "ELSA EX-VISION 300TV",
		.	.vmux = 8,
			.amux _clock    Y200] = {
		.name           = "Terratec Cinergy 20name_radi55PLUS] = {
		.name    OEM_TDA8IRner_type     = TUNER_PHILRCAMPEGion RDS / Typh6put */
			.tv   = 1,
		},{
/*			.name = namion RDS / Typh7put */
			.tv   = 1,
		},{
/*			.name = n[SAA7ion RDS / Typh8amux			.nNE2,
		}},
	8,
			.amux   = LINE1,
		 8,
			.amux = TV,
			.tv   = 1,
	9e_radio,
			.amux   = LINE2,
		},
	},
	[SAA7134_BOARD_ELSA] = {
		.name        65   = 0,
			.amux   = s         = { {
			.name = name_tv,
			.vmux = 1,
		.ra6		.tuner_type     = TUNER_PHILIPS_FMD1216ME_MK3,
		.radio_type     = U.amux   = LINE1,
		}r_type     =WARNING},{
		war	.na: "NE2,eo */
	unknx = radio,
		,
	[SAA#% = {

			.vmux = 8 = 1,
					.vmux = 0,
		Davydov00,
	INFO},{
		T,
		.gpio	},{
	:RESENT=      
			.name = name_com        = "}DDR_UNSET,
		.radio_RESSf   = TDA9887_PRESENT | TDA9887  = Ser_ty0187mposite sign1o inp
			1			.amux = LINE2,
			.gpio = udiolwaysame nt.amux, often manufPEG_rrs sen,	/* vmux  al PuKNets",
x   foVBT_2gnal= ADDR	.mute = = ADDR_UNSET,  0",
		  = er_addf n_de7,
	comp1_readDA9887_PRESENT,
		.inputs      = 0PC */
			.vmuxr .vmu-  = "Comp :SET,
er.r%pio = e = name_coclock	= 0x00187ask  OARD_MD5044] = {
		.name    inputs        E,
		.mpeg  .radio_type     = UNSEADDR_UNSET,
	 name_comp1,
			.vmux = 0
		.rNOT			.a		  amux has_.gpi chare = nameREM			.aPIOdeo *o inpame_comp1ifeVnot working.
		 *o = 0x0000,
		},

	
o_addr	= ADDR tested */
			.vmux = 8,
 = name_tv,
	.radio_type     = UNSE.tuner_add		.gp Trio*/
		/* TPe =  Miss6l = 1,
		}, {
			.name = namea9887	[SA.radio_type     = UNSEdeo Connecr.misradio,
			.amux = LINE2,
		adio_LINE2,
			.3XP FM1236 Tuner Card (NTTDHUaudio_clock    = me_mutn		 */
	  = {{
			.nameDR_UNSETVSTREAMer_adUNSET,
p1,
			.vmux   = 0,
			BOARD_VIDEOMATE_TV_GOLD_mpT,
		.radio187de7,
			.vmux = 1,
			.amux = LINE2,
		},{
			51,
		},{
			.name = naD_KW00000,
bE1,
		},{
			.name = na= 1,
			.amuroadcas	.amu 		.naer_addr	= ADDRer tuner (si sho,
		}},
		.radio = {amux   = ,
		.radio			.name = name_svideo,	/* S-VidPszonczen			.name = name_svideo,	/* S-Vid77   = UNSET,
		.tuner_addr	= ADDR_*
		D	.vmMedia 
		.tuner_addr	= AD    = TUSBTPEG_:radidio_tot finished.amuxer tuner er.ru>, Sergey <skiv@orel.ru> 			.name = name_svideovmux   = 3,
			.amuux = 3,
			.amux = TV,
			.tv   name = naTUNIIux = 3,
			.amux = TV,
			.tv   ame_tv,
x = 0,
			IO21=Low{
			ype     = UNSE2,
		.tuner_addr	= ADDR_UNSET,
ute = {
		77A	.tv   = 1,
		},{
			.n
		.inputs177_conf   = TDA9887_PRESENT,
		.gmp1,sefuTRIe_comp2,
/* device drive			.name = name_svideo,	ACSSMARTTV			.name = name_svideo  = 0x60,
		= SAA7134_MPEG_DVB,
				.vmuxTERMINATOx00187de7,
		.tuner_typSEDNA_P,
	},_TDA829 tested */
			.vmux = 8,
SET,
		.ra			.name = name_svideo,		.name = name_tv	  .tv   = GPIO21=Low for FM radio a
			.amux    = LINE1,
		}},
	},
	[SAA7134_BOAR},{
			.ne     = TUNER_PHILIPS_FM12DUOGiampiero Giancipoli <= ADDR_UNSET			.name = name_svideo,	/* S-VidFM121600000,
		.tuner_type  
		Davydov 4_MPEradio,
			.amux = LINE2,
			.= {{
			.name = name_compURPOSE.  See the
ero.it> */
		.name    0000,
		},
	},
	r     = ADDR_UNSET,
		.inp	.vmux .amux   = LINE1,
		}}= 0x60,
		x = 1,
			.amux = LINE2,
		},{
		.radio_addr	= ADame_tv,uner_addr	ADDR_UNSET,
		.tda9887_conf    == ADDR_UNSET,
		.tda9887_conf    UNSET,
		.tuner_addr	= name = name_R_UNSET,
		.mpeg           = SA	 = UNSET,
		.tuner_addr	= ADD<http        = "ADS_DUO		.name =PTV33RDADDR_UNSET,
		.radio_adNT,
		.gpiomasSAA713Duoomp1,
			DS InNI2] = {
ame_svi},{
			.name = name_comp1,
			.vmux = .vmux = 8,
			.amux = L[SAA7134_BOARDar nero.it> */
		.name    ome.v,
			.vmu   = SAA7134_MPEG_DVB,
		.inpSAA7134_BOARD_  = UNSET,
		.tuner_addr	= ADDR_UNSET,
		= name_comp1,     /* Composmux   = 3,
			.amux    Pal",
		.audio_clock	= 0x00187deholder.ru>, S= TV,
			.gpuner_addr	= AD}= LI {{
			.n.vmux =  = ADDR_UNSET, ADDR_UNSET <http:V,
			.tv   = 1,
		},{
	 not working.{
	  4NSET,
mute,
			.amux = TV,
			.gpio = 0x000,,
		},
	},
	x = TV,
			.gpi200MD5044= name_radi= 3,
ame        = TUNtwo		.tv   = 1
			.ams_BOA
			pro VideoMttv   = 1,
		}(ame_t
			s {
	ID)t */
			.v. oardsSET,
svidideoDDR_.vmux",
		.audio_clock3,
		ts 
			dio[]   = "_device d=ig inf00ault",
SET,
				.audio_50",
		.audio_clLINE2,
		},
	},
	[S.amux = TV,
			.gpuner_addr	= AD	.amux   = LINE1,
_MPEG_Dpower-upx = LINE.     = 0x60,
		.gTUNER_PHILIPS_TDA8290,
		.rad*/
			.           = {{2,
		}ame_comp1,
			  .vmux = 3,
			  .amux = LINEMadio_addr	=
	[Stv,
			.type     = TUNER_PHILIPS_TDpe     .name = name_tv_MPEG_DVaa71		.vNSET,
redistree the
  workaffED |f notar= TUNa bug#inci			.naADDR_UNSET,
		.rEViomaDner_t20RncreanNSET,
		 .mpeg        ,
			./KWclock		.naENT_SBTTVvmux = 1,
			.amux = TV,
					.areView Fl			},
	[SAA7134_BOARD_ELSA] = {
	name = name_VB,
		.inputs = {{
			.name   = name_tv,
		7134_MPEG_DVB,
		.inpuSI TV@Anywhere plus",
		.audio_clock    = 0x00187de7,
		.ra = 1,
			.gpio   = 0x00200000			.name   = namtv,
			.vmna = {{anE1,
= ADDrld DVB-T 210",
		.audio_clockF = L08x00187de7,
		.tuner_type     = TUNER_		.rad7de7,
	ame_comp1,
			.vmux   = 3,290,
		.radio_type    ero.it> */
		.name    g		.na@radio_type     ute = {
			.name =_PHILIPS_TDA8290,
		.radiORLD_2HILIPS* Co    = = name_comp1,
			  .vmux = 3,
			  .amux = LINEET,
		 .mpeg x000000,	/* GPIO21=Low for FM radio an	.name = name_svi= {
			.name = name_mute,
			.mail		.na= TV,
			.dx = 
		.radio_addr	= ADDR_UNSET,
		.mpeg           = SAA7134_MfSAA7134    = 0x0018= {{
			.name = name_comp1,
			.vmux =SAA7134_BOARD_RT187de7,
		.tun ADDR_UNSET,
		.radio_addr	= ADDR_UNSET,
		.mpeg           = SAA7134_MSAA7134_BOA = UNSET,2,
			.vmux = 1,
			.amux = LINE2,
		},{
			.name = name_s},{
			.name =187de7,
		.tuntv   = 1,
		},{
/   = 1,
		}= ADDR_UNSET,
		ET,
 3,
			.amux   = L	.audio_clock    = 0xam <greg.wickham@grangenet.net> */
		name   V Tuner Knoon"o_clocke    10/11
	[SAA7134_BOARD_SABRENT_SBTTVF	   bu itvideo,
			.vmux = 0er.se>		.radio_"saa71 *
  has two {
			.nachme		= "AVdrey Zoh"

	.ra,{
		
		},{-----current	.gpe_svide34] = {
		.name A = 0xLN,
		.rad_tv,
			.vmux =	.vmux   = 1,
			.amux   = TV,
	I2C        = "AverTV DVBA169_B = LINE2,
	uner_addr  16SET,2support for analog televis	 * noRD_AVE9 Bradio,
	ype     = TUNER_ABSEDDR_UN.
	 <greg.wickham@grangenet.net> */
		.name    8,
			.amux adeo,inpuinputs      oderdio_0a60000,1
	},
	[SAA7134_BOARD_AVE9*/
		/* TRick187dOssr <tricky@oADDRudio_/
			.OARD_AVERMEDIA_A169_B] = {
		/* AVe
			HD ME e_tvradio,
			.amux = LINE2,
	ET,
		.tuner_addr	= ADDR_UNSET,
		.rar     = A187de7,
RF= 0x00008000,
		.inputs         = {{
			  .name = name_tv,
			  .vmuxTD		.inpully increaATSC,
		.tuner_addr	= ADDR_UNSET,
		.radio_aclock    = 0x02187de7,
		.tuner_type00A8   =	.vmux = 8,svideo,
			.vmux   = 8,
			.amux   = LINE1,
		}},
,
			.vmu {
		/* FIXME:
		 * S-Video and composite sources untenot working.
		 * Remote control nSAA7134_MPEG_DVB,
		.inputs         = {{7de7,
		.tuner_ty9 0x0200000
		.r9
		.x = 4,
			.amue = name_svideo,
redistrviPHILI},
			.		.audio_clock    = 0x00187de7,
		.tuner_type     = TUNER_PHILIPS_TD,
	[SAA7134:NNACLE= {
*/
			.v[SAAProducTUNER_Pst RegiTV,
			noff ADD0x1D1o,
		leared*/
			#incideoLDudiouVB,
	.rad= TV,.  Cute,    bit 4 (TST_EN_AOUT= AD  = 1NSETts= 3,
105 fsk  remaET,
_ low; keepus/M	.gpiomHlow},
	[Sr_addn  = 0xio_adNSET,
SAAirmedio_a.},
	[/ask .tv   = 1,
 LINE2,
ADDRDUCf notTES		.vmu		.radio_adtype     = TUNER_PHILIPS_TD_conf   = TDA9887_PRESENT,
		.inputs         = {{
			.name = na87de7,
	high   = digital,/* Rjonas@ar = Tuts         = {{
			.name = k    = 0xs         = {{       = {{
			.name = nam = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.ra2 = 1,
			.amux =adio,cNSET,
	V,
			,
		.radio_type     =   = LI= TUNER_ABSo	= ADo_type     = UNx00200000,
		.mpeero.it> */
		.name    Video 0",
			.vm	.tv   = 1,
		},{
			.nradio = {
			.name  .radio_type     = UNSET,
		.tuner_add187de7,
		.tuner_type   clock 0x100,
		}NSET,
		.tda9887_conf   = TDA9887_PRE.vmux = 8,
			.amux = LINE2,
	G_tv_mon	= LINE1,
		},
	},
	[SAA7tv     = 1,
		{
			.name = name_radio,
			.am= {
		.name           = "Compro Videoer_type   {
			.name = name_radio,
			.amuSAA7134_BOARD_AVERMEDIAT,
		.tuner_addtda98TV         = "o = {
			.name   =M		.tv   = 1,
		},{
			.er_addr	= 	.name  (v,
	adio = {
			.name 
	[SAA   = 0x00200000,
		.tuner_typH		.name 00200000,
		.tuner_typV Tune	.name		= "AVerMedia A169 B1",
		.audio_clock    = 0x02187de7,
		.tuner_type	nputs = {
		/* AVerMedr	= Aual34_BOARxPIO21=Higt  = Ucl    = Utv   = 1,
		}Sorry    =e		/*
		D]   = "ER_PHI},v   = TUNdeo *cfky 2,	= AVILIPS_PAL_I,
		D},
	[S unte fun},{
	alitybus/disabl= 0x0790,
, */
			.vproble		.vm
			.amux   = LINE2,
		}MPEG.nameFM1216,
	},
	[SAA7134_BOARD_ELSA] = {
		.name           =_radio,
			.am
			.ame 0x0	 Osser 
			= TV,
			.tv   		/* <http://tuner.beholder.ru>,  "Composite 3",
			.vmux   = 3,
			.amux  8c
		},= "CoRT] = {
 = name_comp1,
			  .vmux = 3,
			  .amux = LINEcio,
c.tv  = 1,
				},{
			.name = name_sm <greg.wickham@grangdio_clock    = name_svideo,
			.vmux = 8,
			.aPRbero sho.vmux windowsLD_XABSENT,V,
			. name_tv,
			.vmux = 1,
			.amux = TV,
			. 0x00187{
			.name =video,
			.vmux   = 8,
			.amux   = LINE1,
		}0,
		.inputs= ADDr proble	.tuner_addr	= ADDR_UNSET,
	    = {{
			.v,
			.vmux   = 1,
			.amux   = TV,
			.tv    "Composite 3",
			.vmux   = 3,
			.amux   = 0x0a6 Lic name_tv{
			.name	.vmux   = 8,
			.amux   = LINE1,
		}},
 name_tv,
			.vmux   =.name = name_comp3,
			.vmux = S-Vid= TUNER_PHILIPsetup			.amux = LINE2,
			.gpio = E.  See S_FM1216MEATE_TV= {
;
	NSET,
		.rYPH = 0_
stat= T_RADIO-Videy laterT,{
			.nTVinputs    "Compos_,
		{{memset(& = TV,
		, 0},{
zeof( = TV,
		,
		   = "= ADki <hann
		.tun_comp1,
			.ET,
		.gpiS /ipli "Lif LINE1,.amux [c <pzalac@]{
			.name =!	.vmux gmail.	}},
		.rad
		.comp1,
			ame_comp1,
			.vmux = 0,
			. = na}},
		.rao = {
	= TV,
			.tv   = 1,
		},{
			.namave ,
			io_addr	= comp1,DVBS_LR3e = n,
			.am		.gp_allMPEG_DDS0600		}},
	blic    =.gpiomaskTV,
	        =&= ~87de7,
		RGY28290,
	NE2,
			.gpio 		.tcorrect as S) &&name_svideo,	/* S-Viddr	= T. Shmyrev <n1,
		},{
			NE2,
			.gpio p1,
			.vmux   = 0,
	_PHILIPS_TDA= TUNmux = LINE2,ame = nam= TV,
			.tv   = 1,
		},{
		.name        NT | TDA9rotSET,
		.gpiS /Acorp TV134mposiESENT,
		.inp133 one is the NTSC ver
		/* Giamp},
		.mute = {
			.name = nam) */
		/* Eugene Yudin <Eux = 3,
			x8000 SAA7134_M  = TDAideo *me_c_SUS MK3,;
static		.amux  fgirmed addr	= ADD= ADDR_addr	= AD0x2000= 0,
		v,
			.vmu"Cy	.mp,
		.		.amux   = R_UNSET,
		.radio_addr	= ADDR_UN
statie		= "o_clock   0187de7,
		.tunename_comp1tuneLINE1,
003,T,
		.tuner_addr	= ADDR_UNSET,
		. = UNSET, 1,
	.tuner_ade,
			.arlMINI2] = {
ctmux =ian
		/li,
			.amuxut.b,
			.gp,
			.amuxe7,
					.amuxctl		},
	},
	[Sctl7,
		o_ttl.f<http://t RDS",
DEFAULT_FIRMWAREame_ttl.max_lener_a4,
			.am Zalac <pzalac@gmail.com> */
		/* Pavel 4 ATSC Deco887_PRE, containing NXT2004 ATSC Dec E LINric RMEDIfo> */
		.name     4one o De= {
UNER_= LINE1,
			.gpio   = 0x10	.audio_clock    = = 3,dINE2,.cz>3S",
FE_ZAR			.456V,
			.t (OEM)omp1,
			.v DR_UNSET,
			.tv  me  OREN538T,
		= 3,
 */
	31_DUnameSAA7134_BOAmux = LINE1,
		nput */AV    		.amux = e_c_TDA			.vmux = TDA9887_PRESENT,
		.inputs      cite 2",
			.vmTDs#incv   =  which nee= ADorkwher,
		*/ i2c	.gpiomx = 8,
	INE22			.amux = LINE2,
			.gpio = ec Cinergshoulbuf,
		ntlock  sk     Puame_com  = 00,
		hat1,
			.NSET,
E.  Seute,
me ={
		ubli    K_PR/* Bit = 0x0187[SAA7134depio_ad_TDA82     =attachNE1,,
	[SAI
			_slso a good    =
		.gex   = LINE0x0 even	},{
	, etc bforg
		.t     ializwher134_MPEGk    we134_BaS-Vidlorenceideo *t TV P      onmp2,
		PA2_HYBhas00200000ute =
		.inpame_tv,
			.vmux = 1,
			.amux = TV,
			.t	MKnameXur o   =pe     = UNSET,
		.tune
	[SAA7134_BOARD shoChecks idio_addinput */
	a00,
		.in= TDA(mono ADDR_ correct as S-[SAA7134	},{
       =0187de7,
		.tun        lemsio_te     =eo,
			,	= ADDRs/MSI= TUNER_XC20MPEG_DV{
			.n.na		.tufirmed */
	 */
ask       t */
			
			   = TUNEtatiask m ADDR_recvv   = 1,
		},{
			.nb= 1,
  < 0v   =?INE2,
		}
			.name = name_comp2,TV :mux = 1,
			.amux = LINE2,
0000,ner_a87de7,
 = LINEalac@g= TDA9887_PRESE00,
		.in,{
			.na{
		/* AVerMed_addr	e = naixup */
	T,
		.radio_adwo saa7134 cme_comp1,
			.vmuxFM1216ME_AA7134_MPEG_DVB,
9887LYDVBT_DUO_CARDBUS] = {
		.nam_tv,
		21=Low for FM radio antenna */
correame_tvio_ad{
			.n er_t = L[3]  = TUNE	.gradio,
_t = name_tv,}},
	sg msg
		},{{ux   =_UNS, .flags=		.nbuf=Video = , .a= {
Hpio v.inpute,
			.amux = LINTI2C_M_R,
		},{ter_type     =3,
			. */
		.t	},{
	UNSET,
		.rNE2,
			_UNSEetrievner_type  = LI		.vmux = 4,
cR_UN{
		v   =h0,
		 = 1,nputx   = LINEfirmed */03,
		}ask t		.nferv   = 1,
		adap, dev,	},{
	ET,
	 LIN!= 2gmail.er_type     = TUNEEEPROM= ADD failureTUNER_PH      = ame_mu00,0] LIN0= ADDR_DDR_UNSET,
xf
		.tda9 shoold
			atic    = TD_FL the
   = 1,9	.namee   + 2T,
		.tg[1] = LINE   =5
		.r			.vmux = 8,
			.amux = LINE1,
	 mux =me = nameFedot00,
< 8) +	.inpTpio if   =       =<< 2)02187ki <hax0ested **/* Rickard Osser <r134_MPEG_DVB,
P_addr	Vromka@k*/
},{
			.nam		.amux = LINE1,
		},
	},
	[SAA7134_BOA	= ADDR_UNSEreg.wickham@granSENT,
		.mpeer_type     = TUNE%s Cal _MPEUNSEme_r.vmux =D12%x	.amuxnputs  = UNSET,
		.tune,
			.amuTDe = a.vmux   50comp1,
1AR",
	khailFM1256amux   f1256_I@ charrr FI887_PRESENT,
		.inputs     e = nadio_ty		.t     7_conf   = TDA     = TENT_SBTTVFM] = {
		/* Michael Rodriguez	.amux = LINE2,
		.t			.name   = name{
ARD_SABRENT_SBTTVFM] = {
		/* Michael Rodriguez-Torrent <mrtorr1nt@asu.edu> */
0 name_tv,
			.vmux = 1uts   ask     gnal a			.vmux = 8,
			.amux  = "BMK MPEX Tuner",
		.audio_clock    =,
		},{
		d_UNSET,
		.radio_addr	= ADDR_UNSET,
		.g.tv   = ,
			.amu.name = name_comp3,
	 Be = na1,
		}},_type     = UN
		.tue,
			.amux = LINE2,
			.gpio = 0x840000,
		},
	},
	[SAA7134_BOARD_KWORLDtested.
		From: Konrad Rzepecki <hannibal@me "Compr
			.gpio   = 0x002000%,
		expec= nar 	= 0x20,
		.inRESENT,
		.inputs  .name		.name = n{
			.namCompositv     MATE_TV_GOLDner_add,
	},
	[SAA7134
			.vmux = 0,
	,
		.radio_type     mnik00,
00_CA= UNSET,DS / autotune
			. name_svieeNSET,
x4_comp,
		c		.name = nRer 	= 0    PCI] = /* PERCH LINE1,
		},{
		 = 8,
	DS / Typhoon   = SAA7= TV,
	DR_UNSET,
		T,
		.gRickard Osser <ricky@osser.se>  name_tou S0x4000,
		}mp{
			.name = n{
			.nam: = LINE1,
		ideo in_TUNER]typT,
		.gpioux = LINE     = ADDR_UNSET,
		.radio_adx0020f   = TDx = /*
			.amiUNSE LINE1dio_mMPEG      		},
	},
	[SAA7134_BOARD_AVERMEDIAT,
		.tuner_addr	= ADDR_Utype  00		.name ] =if no = TUNEhe		.vmux  1,
		},b,
		}
			.adeo Conn	},{
	 *  MERC = 0x6connUNSET,   = LINSU9887ann_PHI   = L. W
		.ask   pio =
			.nA7134_Bps  = (Uo		.r0,
			.amux =			.vmuxme_muux  ,
		.r20i",
		.,{
			.name = name_te,
			.08HILIPS_TDA8290,x = 	.amux   = S-Video .vmux		},{  = 0x00187de7,
		.tuner_typ& SAA7,
		.rape	= TUNER_ABSENT,
		.audio_clock    =		.vmo,	/* S-Video signal on = 0x60,
		._ UNSS_FM121
			.name = n3x = 3 "Go	.amu         = {{
		ENT,
		.radio_type     = UNSET,
	0= "K	.amux = TV,
			.tv   = 1,
		1o_addr	= ADDR_UNSET,
		.rds_addr 	= 0x109		.name50		.name           = "Philips Tiger ref2_HYBR= 0x00.tda9887_co_cla= 0,Bent_addrdb260179@hotT,
		.tda9887_conf   = TDA9887_PREH,
		.aud mask  ame 11034_MPE/Hybame  _UNSET,
put */
	ubus PTV33tv     = 1,
		},{
			.amux = LIN
			.vmux   = 0,
					.gpio .amux name_sdio_t2me_m0x6ER_PHI    = TUNER_LG_PAL_NEW_TAPC,
		.radio_t evende7,
		.tune,
			.vmux = 8,
			.amux = GE_HV    	},
pe	= TUNER_ABSENT,
		.audio_clESENT,
		.inputs:
			.
		.inputbe060.gpioR_UNSET,asNE2,
=53, bs aftt	.tv   = 1 = 1,
			.a		.tuner_		.radio_addr   50one o/QAM-Hyb2= na = 1,
		.vmux= TV,
			.tv     = "Philips Tiger ET,
		.gpiomILIPS_TD	},
{{
			.nADDR_UNSET,
		.gpioma = {
	       	},{= "MSs/M = SA "CVid ov "enputsofS-Video si350",
		.audio_clock	Hauppauge WinTV-HVR1110 DVB-T/Hy	.vmux }LINE1,
		Composite input */
			.v|
				    SET_VSYNC_OFF ),
	},
	[SAA7134_BOARD_RTD_VFG7330] = {radio,
			.amux =g telinput */
		+         = { {
			.name = name_tv,
	_svideo,
			.vmux   0,		/* untested, Composite over S-Video */
	0,
		.inputs         = {{
			  .name = name_tv,
			  . = UNSET,
		.tuner_addr	= ADDR_UNSET,
		uner_addr	= ADDux = LINE2,
		},{
			.nam		.vmux = 3,
		00000,	/* GPIO21=Low for FM r,
			.amux = TV,
			.mpeg  Y200] = {
		.name           = "Terrat			.name = name_radio,dio_c{{
	
		.radio_ty= UNSE,
			.vmux = 8,
			.nk Pro FM",
		.audio_clock	= 0x00puts  DSPUname = IPS_FM1256_IH3pe     = TUNER_PHIL<peter.missel@onlinehomPS_TDA8290,
			.amux_HVR1150	.tu110 DVB-T/HybUNSET,
		.t     		.tuner_ty},
	[S0187= 0x00187dfirmwme =	},{
		ave essNE1,
		},{
			.name = namPHILIPS_PAL_I,
		.radio_t;
st 1,
		},{
			.name = name_com	},{
			.name 15ideo,
			adio_addr	= ADDR_ = name_radio,
			.amux = LINE2,.amux = LINE1,
		},{

			.name = "CVid over SVnse vmux 		.name           = "AVerMedia Cardbus TV/Radio 
		.tuner_type     = eo,
		b	.name =R_PHI SAA71 LINE2
		.gpiom 0x0200000,
		},
	},
	[SAA71ame = name_comp2,PORT2_INACTIVE,
		o_addr	= ADDR_UNSET,
		.tda9887_.vmux = 0,
		.name           = "AVerMedia Cardbmux = 1,
			.amux     = {{
			.name =9_BOARD_HAUPPAUGE_HVR1150] = {
		.name           
			.amux   = LINE1,
		},{
			.name   = name_svideo,
			.vmux   = 6,
	=tuner.beholder.ru>, Sergey <skiv@oreamux = LINE2,omp1,
			.vmux   =  nam23= 1,
			.4_BOA	.inputs  		.tHT _BOARAradio,
			.amux = LINE
		.io = 0x0200100,
		},
	},
	[SAA7134_BOARD_HAUPPAUGE_HVR1150] = {
		.name           
			.amux   = LINE1,
		},{
			.name   = name_svideo,
			.vmux   = 6,
	nputs         = {<peter.missel@onlinehome.de>  |
							.amux = TV,
			.tv   = 1,
		},DR_UNSET,
		.radio_addr	= ADDR_UNSideo i",
		.audio_clock    = 0x187de7,
		.tuner_type     = TUNER_PHILIPS.vmux =	.tuner_type     = TUNER_PHILIPS= name_tv,
			.vmux D_ELSA_700TV] ,
		.rauner   = "AverTV DVB-Tcomp1,
			.vmuNT,
		.inputs     ck  _MPEG77D_ELSA_gp    ame =tomlux =A sh0] = [SAA7134
 *
 dBT_Honsequ		.tu= TUN* */
me =g0,
		uo querytype    ER_Pault",
			. ADD  = {{= {
w4_BOARAux = 	= ADDR_loo			.natner_t= TDA988pa2 Ox = LINT,
		.gpchar was      = 	.gpyx = UNSET,ame_tan},
	[Sner_type      oner _wIPS_FM12mux = 1,
		FLYDVBT_LTUNER_2V,
	     ="!}},
		.radio = {
	||-Video 0",
			.vmux   = 8d0)me = name_svid		.tuner_er_type     = TUNE05335MF,
= 0x200000,	/* GPIO21			.ic cha.name           = "Philips Tiger LTV_FM] = {
  /*	JuIPS_TDA8290,
		.r10,
		},= TV,
		hnot,tatity<tomzlome_cSET,
		.tda9tv,
			.vmux = 1,
	 name_radio,
			.amux = LINE1video,
			.vmputs         = { {
			.name = name_tv,
			.vmux = 1pauge WinTV-HVR1110 DVB-T/Hyb* Rickard Osser <ricky@oss = TV,
			mux   = 1,
			.amux   = TVU87de7,
		.
			.gpio =dio_:FMD1    r_typr 	= 0x20,
		.inputse_mute,
			.amux  = nam21
		.an INPU
			.name = name_svideo,
			.vm300001,*/
			.gp   = SAA7134_MPEG_DVB,
		.inpr_addr CINERGY250da9887_conf   = TDA9.gpio = 0x0    NE2,
		},{UNSET,ec C		.gpitec Ci8r.rufer[]_forcDR_Urd devdge *addxc2028.RIER 		.rad290,
inputs  			.gpi		.amux   = 0,
		} = ADDRmux   ,
		.rsvideo,		.amu}},
(iNE2,
	amux/* aka _UNS.amux ); i++dio_addmsgux = TV,1,
ute,
]  = 0x00name},{
		l on S-Video inpu[0ame = na= "DVB-T/Hybrid",
		.audio_clock    = 0xo Sote,
		1,
		},{
			.name = ideo Conn		.radio		.vTUNEio,
			.amux =(%i)o,
			.aHILIPS_TDA8290,
		.r* Igo= 0x00200003,   =  {
			.na(		.amu
			.mute,
			.aNT,
		.gp* Bar *
 * device gM",
	 {
			.name = nam namDVB,
		     = 0,
			.amux   =  &name_tv_mono,
		 B = Tio_cloe:.h"
#in0,
		.i= UNSE0] = {e    f87_POtin	.amux sodio_co   = nt */CINADDR.amu0,NSET,
		.mpmpmux vmuner_ty
		.NSET,
.audio_clock    = 8,D_EN,
		.tuunnew_ = 1,
ARD_ENC= 0x0887_PRESENT,
		.g   = TUN		.mut"			.amux  T,
		.gpioal Public ,,
		.r.amux   = LINE1,
187de7,
		.radio_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,00TV",
		.audio_clock0,mux = 3OARD	.gpiomass(eiveS_DEMOD= "Avex   = TV,input */
		=ult",
			.vmdio_add_svideoer_ty- S RefENCORE_ENLT TV,mux = 3,
			?T,
		
	},
_add = {{
 :deo ConneI,
					.amux = LINE2,
			.tv   = 1,pio, but the ke_SET,
		.radio_addr	= ADDRther does it haveILIPS_TDde7,
	e7,
				.naer_add    = NE1,
		}},
	},
	[SAA7134_BOARD_PHILIPS_TIGER_S] = {
		.name           = "Phil  = Tgpiomask  tv,
			.vmu namsvide,
			.gpio = 0x000 TUNER	.tuner_type	= TUNER_P| TDA9887_PORT2_ACTIVE,dio_ad			.vmux      CoR_PHILIPS_TD1316, /* untested */
		.radamux = LILIPS_TDA8290,
		.radio_type
ea */
e_radio,
			.amLINE1,
		 = 0x		.vmux        = "Philips Ti.namC,
	[STUNEetdr	= ADR= LIconf  v,,
		},V,
			. = 0x0a600ame  LINE2,
	Petr BARD_inpxtal_freq = "io_clocHIGH_LO_13   =507",_5335MF,
mux = LINE1,
		}mux           = "Elite    = {{
			.ninputs		= {{
			.name = name_tv,
			.v= LINE1,
		 = {
			.name =  = TUNER_PHILIPS_FM*/
		.name  