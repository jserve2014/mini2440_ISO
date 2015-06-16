/*

    bttv-cards.c

    this file has configuration informations - card-specific stuff
    like the big tvcards array for the most part

    Copyright (C) 1996,97,98 Ralph  Metzler (rjkm@thp.uni-koeln.de)
			   & Marcus Metzler (mocm@thp.uni-koeln.de)
    (c) 1999-2001 Gerd Knorr <kraxel@goldbach.in-berlin.de>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/kmod.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/vmalloc.h>
#include <linux/firmware.h>
#include <net/checksum.h>

#include <asm/unaligned.h>
#include <asm/io.h>

#include "bttvp.h"
#include <media/v4l2-common.h>
#include <media/tvaudio.h>
#include "bttv-audio-hook.h"

/* fwd decl */
static void boot_msp34xx(struct bttv *btv, int pin);
static void hauppauge_eeprom(struct bttv *btv);
static void avermedia_eeprom(struct bttv *btv);
static void osprey_eeprom(struct bttv *btv, const u8 ee[256]);
static void modtec_eeprom(struct bttv *btv);
static void init_PXC200(struct bttv *btv);
static void init_RTV24(struct bttv *btv);

static void rv605_muxsel(struct bttv *btv, unsigned int input);
static void eagle_muxsel(struct bttv *btv, unsigned int input);
static void xguard_muxsel(struct bttv *btv, unsigned int input);
static void ivc120_muxsel(struct bttv *btv, unsigned int input);
static void gvc1100_muxsel(struct bttv *btv, unsigned int input);

static void PXC200_muxsel(struct bttv *btv, unsigned int input);

static void picolo_tetra_muxsel(struct bttv *btv, unsigned int input);
static void picolo_tetra_init(struct bttv *btv);

static void tibetCS16_muxsel(struct bttv *btv, unsigned int input);
static void tibetCS16_init(struct bttv *btv);

static void kodicom4400r_muxsel(struct bttv *btv, unsigned int input);
static void kodicom4400r_init(struct bttv *btv);

static void sigmaSLC_muxsel(struct bttv *btv, unsigned int input);
static void sigmaSQ_muxsel(struct bttv *btv, unsigned int input);

static void geovision_muxsel(struct bttv *btv, unsigned int input);

static void phytec_muxsel(struct bttv *btv, unsigned int input);

static void gv800s_muxsel(struct bttv *btv, unsigned int input);
static void gv800s_init(struct bttv *btv);

static int terratec_active_radio_upgrade(struct bttv *btv);
static int tea5757_read(struct bttv *btv);
static int tea5757_write(struct bttv *btv, int value);
static void identify_by_eeprom(struct bttv *btv,
			       unsigned char eeprom_data[256]);
static int __devinit pvr_boot(struct bttv *btv);

/* config variables */
static unsigned int triton1;
static unsigned int vsfx;
static unsigned int latency = UNSET;
int no_overlay=-1;

static unsigned int card[BTTV_MAX]   = { [ 0 ... (BTTV_MAX-1) ] = UNSET };
static unsigned int pll[BTTV_MAX]    = { [ 0 ... (BTTV_MAX-1) ] = UNSET };
static unsigned int tuner[BTTV_MAX]  = { [ 0 ... (BTTV_MAX-1) ] = UNSET };
static unsigned int svhs[BTTV_MAX]   = { [ 0 ... (BTTV_MAX-1) ] = UNSET };
static unsigned int remote[BTTV_MAX] = { [ 0 ... (BTTV_MAX-1) ] = UNSET };
static unsigned int audiodev[BTTV_MAX];
static unsigned int saa6588[BTTV_MAX];
static struct bttv  *master[BTTV_MAX] = { [ 0 ... (BTTV_MAX-1) ] = NULL };
static unsigned int autoload = UNSET;
static unsigned int gpiomask = UNSET;
static unsigned int audioall = UNSET;
static unsigned int audiomux[5] = { [ 0 ... 4 ] = UNSET };

/* insmod options */
module_param(triton1,    int, 0444);
module_param(vsfx,       int, 0444);
module_param(no_overlay, int, 0444);
module_param(latency,    int, 0444);
module_param(gpiomask,   int, 0444);
module_param(audioall,   int, 0444);
module_param(autoload,   int, 0444);

module_param_array(card,     int, NULL, 0444);
module_param_array(pll,      int, NULL, 0444);
module_param_array(tuner,    int, NULL, 0444);
module_param_array(svhs,     int, NULL, 0444);
module_param_array(remote,   int, NULL, 0444);
module_param_array(audiodev, int, NULL, 0444);
module_param_array(audiomux, int, NULL, 0444);

MODULE_PARM_DESC(triton1,"set ETBF pci config bit "
		 "[enable bug compatibility for triton1 + others]");
MODULE_PARM_DESC(vsfx,"set VSFX pci config bit "
		 "[yet another chipset flaw workaround]");
MODULE_PARM_DESC(latency,"pci latency timer");
MODULE_PARM_DESC(card,"specify TV/grabber card model, see CARDLIST file for a list");
MODULE_PARM_DESC(pll,"specify installed crystal (0=none, 28=28 MHz, 35=35 MHz)");
MODULE_PARM_DESC(tuner,"specify installed tuner type");
MODULE_PARM_DESC(autoload, "obsolete option, please do not use anymore");
MODULE_PARM_DESC(audiodev, "specify audio device:\n"
		"\t\t-1 = no audio\n"
		"\t\t 0 = autodetect (default)\n"
		"\t\t 1 = msp3400\n"
		"\t\t 2 = tda7432\n"
		"\t\t 3 = tvaudio");
MODULE_PARM_DESC(saa6588, "if 1, then load the saa6588 RDS module, default (0) is to use the card definition.");
MODULE_PARM_DESC(no_overlay,"allow override overlay default (0 disables, 1 enables)"
		" [some VIA/SIS chipsets are known to have problem with overlay]");

/* ----------------------------------------------------------------------- */
/* list of card IDs for bt878+ cards                                       */

static struct CARD {
	unsigned id;
	int cardnr;
	char *name;
} cards[] __devinitdata = {
	{ 0x13eb0070, BTTV_BOARD_HAUPPAUGE878,  "Hauppauge WinTV" },
	{ 0x39000070, BTTV_BOARD_HAUPPAUGE878,  "Hauppauge WinTV-D" },
	{ 0x45000070, BTTV_BOARD_HAUPPAUGEPVR,  "Hauppauge WinTV/PVR" },
	{ 0xff000070, BTTV_BOARD_OSPREY1x0,     "Osprey-100" },
	{ 0xff010070, BTTV_BOARD_OSPREY2x0_SVID,"Osprey-200" },
	{ 0xff020070, BTTV_BOARD_OSPREY500,     "Osprey-500" },
	{ 0xff030070, BTTV_BOARD_OSPREY2000,    "Osprey-2000" },
	{ 0xff040070, BTTV_BOARD_OSPREY540,     "Osprey-540" },
	{ 0xff070070, BTTV_BOARD_OSPREY440,     "Osprey-440" },

	{ 0x00011002, BTTV_BOARD_ATI_TVWONDER,  "ATI TV Wonder" },
	{ 0x00031002, BTTV_BOARD_ATI_TVWONDERVE,"ATI TV Wonder/VE" },

	{ 0x6606107d, BTTV_BOARD_WINFAST2000,   "Leadtek WinFast TV 2000" },
	{ 0x6607107d, BTTV_BOARD_WINFASTVC100,  "Leadtek WinFast VC 100" },
	{ 0x6609107d, BTTV_BOARD_WINFAST2000,   "Leadtek TV 2000 XP" },
	{ 0x263610b4, BTTV_BOARD_STB2,          "STB TV PCI FM, Gateway P/N 6000704" },
	{ 0x264510b4, BTTV_BOARD_STB2,          "STB TV PCI FM, Gateway P/N 6000704" },
	{ 0x402010fc, BTTV_BOARD_GVBCTV3PCI,    "I-O Data Co. GV-BCTV3/PCI" },
	{ 0x405010fc, BTTV_BOARD_GVBCTV4PCI,    "I-O Data Co. GV-BCTV4/PCI" },
	{ 0x407010fc, BTTV_BOARD_GVBCTV5PCI,    "I-O Data Co. GV-BCTV5/PCI" },
	{ 0xd01810fc, BTTV_BOARD_GVBCTV5PCI,    "I-O Data Co. GV-BCTV5/PCI" },

	{ 0x001211bd, BTTV_BOARD_PINNACLE,      "Pinnacle PCTV" },
	/* some cards ship with byteswapped IDs ... */
	{ 0x1200bd11, BTTV_BOARD_PINNACLE,      "Pinnacle PCTV [bswap]" },
	{ 0xff00bd11, BTTV_BOARD_PINNACLE,      "Pinnacle PCTV [bswap]" },
	/* this seems to happen as well ... */
	{ 0xff1211bd, BTTV_BOARD_PINNACLE,      "Pinnacle PCTV" },

	{ 0x3000121a, BTTV_BOARD_VOODOOTV_200,  "3Dfx VoodooTV 200" },
	{ 0x263710b4, BTTV_BOARD_VOODOOTV_FM,   "3Dfx VoodooTV FM" },
	{ 0x3060121a, BTTV_BOARD_STB2,	  "3Dfx VoodooTV 100/ STB OEM" },

	{ 0x3000144f, BTTV_BOARD_MAGICTVIEW063, "(Askey Magic/others) TView99 CPH06x" },
	{ 0xa005144f, BTTV_BOARD_MAGICTVIEW063, "CPH06X TView99-Card" },
	{ 0x3002144f, BTTV_BOARD_MAGICTVIEW061, "(Askey Magic/others) TView99 CPH05x" },
	{ 0x3005144f, BTTV_BOARD_MAGICTVIEW061, "(Askey Magic/others) TView99 CPH061/06L (T1/LC)" },
	{ 0x5000144f, BTTV_BOARD_MAGICTVIEW061, "Askey CPH050" },
	{ 0x300014ff, BTTV_BOARD_MAGICTVIEW061, "TView 99 (CPH061)" },
	{ 0x300214ff, BTTV_BOARD_PHOEBE_TVMAS,  "Phoebe TV Master (CPH060)" },

	{ 0x00011461, BTTV_BOARD_AVPHONE98,     "AVerMedia TVPhone98" },
	{ 0x00021461, BTTV_BOARD_AVERMEDIA98,   "AVermedia TVCapture 98" },
	{ 0x00031461, BTTV_BOARD_AVPHONE98,     "AVerMedia TVPhone98" },
	{ 0x00041461, BTTV_BOARD_AVERMEDIA98,   "AVerMedia TVCapture 98" },
	{ 0x03001461, BTTV_BOARD_AVERMEDIA98,   "VDOMATE TV TUNER CARD" },

	{ 0x1117153b, BTTV_BOARD_TERRATVALUE,   "Terratec TValue (Philips PAL B/G)" },
	{ 0x1118153b, BTTV_BOARD_TERRATVALUE,   "Terratec TValue (Temic PAL B/G)" },
	{ 0x1119153b, BTTV_BOARD_TERRATVALUE,   "Terratec TValue (Philips PAL I)" },
	{ 0x111a153b, BTTV_BOARD_TERRATVALUE,   "Terratec TValue (Temic PAL I)" },

	{ 0x1123153b, BTTV_BOARD_TERRATVRADIO,  "Terratec TV Radio+" },
	{ 0x1127153b, BTTV_BOARD_TERRATV,       "Terratec TV+ (V1.05)"    },
	/* clashes with FlyVideo
	 *{ 0x18521852, BTTV_BOARD_TERRATV,     "Terratec TV+ (V1.10)"    }, */
	{ 0x1134153b, BTTV_BOARD_TERRATVALUE,   "Terratec TValue (LR102)" },
	{ 0x1135153b, BTTV_BOARD_TERRATVALUER,  "Terratec TValue Radio" }, /* LR102 */
	{ 0x5018153b, BTTV_BOARD_TERRATVALUE,   "Terratec TValue" },       /* ?? */
	{ 0xff3b153b, BTTV_BOARD_TERRATVALUER,  "Terratec TValue Radio" }, /* ?? */

	{ 0x400015b0, BTTV_BOARD_ZOLTRIX_GENIE, "Zoltrix Genie TV" },
	{ 0x400a15b0, BTTV_BOARD_ZOLTRIX_GENIE, "Zoltrix Genie TV" },
	{ 0x400d15b0, BTTV_BOARD_ZOLTRIX_GENIE, "Zoltrix Genie TV / Radio" },
	{ 0x401015b0, BTTV_BOARD_ZOLTRIX_GENIE, "Zoltrix Genie TV / Radio" },
	{ 0x401615b0, BTTV_BOARD_ZOLTRIX_GENIE, "Zoltrix Genie TV / Radio" },

	{ 0x1430aa00, BTTV_BOARD_PV143,         "Provideo PV143A" },
	{ 0x1431aa00, BTTV_BOARD_PV143,         "Provideo PV143B" },
	{ 0x1432aa00, BTTV_BOARD_PV143,         "Provideo PV143C" },
	{ 0x1433aa00, BTTV_BOARD_PV143,         "Provideo PV143D" },
	{ 0x1433aa03, BTTV_BOARD_PV143,         "Security Eyes" },

	{ 0x1460aa00, BTTV_BOARD_PV150,         "Provideo PV150A-1" },
	{ 0x1461aa01, BTTV_BOARD_PV150,         "Provideo PV150A-2" },
	{ 0x1462aa02, BTTV_BOARD_PV150,         "Provideo PV150A-3" },
	{ 0x1463aa03, BTTV_BOARD_PV150,         "Provideo PV150A-4" },

	{ 0x1464aa04, BTTV_BOARD_PV150,         "Provideo PV150B-1" },
	{ 0x1465aa05, BTTV_BOARD_PV150,         "Provideo PV150B-2" },
	{ 0x1466aa06, BTTV_BOARD_PV150,         "Provideo PV150B-3" },
	{ 0x1467aa07, BTTV_BOARD_PV150,         "Provideo PV150B-4" },

	{ 0xa132ff00, BTTV_BOARD_IVC100,        "IVC-100"  },
	{ 0xa1550000, BTTV_BOARD_IVC200,        "IVC-200"  },
	{ 0xa1550001, BTTV_BOARD_IVC200,        "IVC-200"  },
	{ 0xa1550002, BTTV_BOARD_IVC200,        "IVC-200"  },
	{ 0xa1550003, BTTV_BOARD_IVC200,        "IVC-200"  },
	{ 0xa1550100, BTTV_BOARD_IVC200,        "IVC-200G" },
	{ 0xa1550101, BTTV_BOARD_IVC200,        "IVC-200G" },
	{ 0xa1550102, BTTV_BOARD_IVC200,        "IVC-200G" },
	{ 0xa1550103, BTTV_BOARD_IVC200,        "IVC-200G" },
	{ 0xa182ff00, BTTV_BOARD_IVC120,        "IVC-120G" },
	{ 0xa182ff01, BTTV_BOARD_IVC120,        "IVC-120G" },
	{ 0xa182ff02, BTTV_BOARD_IVC120,        "IVC-120G" },
	{ 0xa182ff03, BTTV_BOARD_IVC120,        "IVC-120G" },
	{ 0xa182ff04, BTTV_BOARD_IVC120,        "IVC-120G" },
	{ 0xa182ff05, BTTV_BOARD_IVC120,        "IVC-120G" },
	{ 0xa182ff06, BTTV_BOARD_IVC120,        "IVC-120G" },
	{ 0xa182ff07, BTTV_BOARD_IVC120,        "IVC-120G" },
	{ 0xa182ff08, BTTV_BOARD_IVC120,        "IVC-120G" },
	{ 0xa182ff09, BTTV_BOARD_IVC120,        "IVC-120G" },
	{ 0xa182ff0a, BTTV_BOARD_IVC120,        "IVC-120G" },
	{ 0xa182ff0b, BTTV_BOARD_IVC120,        "IVC-120G" },
	{ 0xa182ff0c, BTTV_BOARD_IVC120,        "IVC-120G" },
	{ 0xa182ff0d, BTTV_BOARD_IVC120,        "IVC-120G" },
	{ 0xa182ff0e, BTTV_BOARD_IVC120,        "IVC-120G" },
	{ 0xa182ff0f, BTTV_BOARD_IVC120,        "IVC-120G" },
	{ 0xf0500000, BTTV_BOARD_IVCE8784,      "IVCE-8784" },
	{ 0xf0500001, BTTV_BOARD_IVCE8784,      "IVCE-8784" },
	{ 0xf0500002, BTTV_BOARD_IVCE8784,      "IVCE-8784" },
	{ 0xf0500003, BTTV_BOARD_IVCE8784,      "IVCE-8784" },

	{ 0x41424344, BTTV_BOARD_GRANDTEC,      "GrandTec Multi Capture" },
	{ 0x01020304, BTTV_BOARD_XGUARD,        "Grandtec Grand X-Guard" },

	{ 0x18501851, BTTV_BOARD_CHRONOS_VS2,   "FlyVideo 98 (LR50)/ Chronos Video Shuttle II" },
	{ 0xa0501851, BTTV_BOARD_CHRONOS_VS2,   "FlyVideo 98 (LR50)/ Chronos Video Shuttle II" },
	{ 0x18511851, BTTV_BOARD_FLYVIDEO98EZ,  "FlyVideo 98EZ (LR51)/ CyberMail AV" },
	{ 0x18521852, BTTV_BOARD_TYPHOON_TVIEW, "FlyVideo 98FM (LR50)/ Typhoon TView TV/FM Tuner" },
	{ 0x41a0a051, BTTV_BOARD_FLYVIDEO_98FM, "Lifeview FlyVideo 98 LR50 Rev Q" },
	{ 0x18501f7f, BTTV_BOARD_FLYVIDEO_98,   "Lifeview Flyvideo 98" },

	{ 0x010115cb, BTTV_BOARD_GMV1,          "AG GMV1" },
	{ 0x010114c7, BTTV_BOARD_MODTEC_205,    "Modular Technology MM201/MM202/MM205/MM210/MM215 PCTV" },

	{ 0x10b42636, BTTV_BOARD_HAUPPAUGE878,  "STB ???" },
	{ 0x217d6606, BTTV_BOARD_WINFAST2000,   "Leadtek WinFast TV 2000" },
	{ 0xfff6f6ff, BTTV_BOARD_WINFAST2000,   "Leadtek WinFast TV 2000" },
	{ 0x03116000, BTTV_BOARD_SENSORAY311,   "Sensoray 311" },
	{ 0x00790e11, BTTV_BOARD_WINDVR,        "Canopus WinDVR PCI" },
	{ 0xa0fca1a0, BTTV_BOARD_ZOLTRIX,       "Face to Face Tvmax" },
	{ 0x82b2aa6a, BTTV_BOARD_SIMUS_GVC1100, "SIMUS GVC1100" },
	{ 0x146caa0c, BTTV_BOARD_PV951,         "ituner spectra8" },
	{ 0x200a1295, BTTV_BOARD_PXC200,        "ImageNation PXC200A" },

	{ 0x40111554, BTTV_BOARD_PV_BT878P_9B,  "Prolink Pixelview PV-BT" },
	{ 0x17de0a01, BTTV_BOARD_KWORLD,        "Mecer TV/FM/Video Tuner" },

	{ 0x01051805, BTTV_BOARD_PICOLO_TETRA_CHIP, "Picolo Tetra Chip #1" },
	{ 0x01061805, BTTV_BOARD_PICOLO_TETRA_CHIP, "Picolo Tetra Chip #2" },
	{ 0x01071805, BTTV_BOARD_PICOLO_TETRA_CHIP, "Picolo Tetra Chip #3" },
	{ 0x01081805, BTTV_BOARD_PICOLO_TETRA_CHIP, "Picolo Tetra Chip #4" },

	{ 0x15409511, BTTV_BOARD_ACORP_Y878F, "Acorp Y878F" },

	{ 0x53534149, BTTV_BOARD_SSAI_SECURITY, "SSAI Security Video Interface" },
	{ 0x5353414a, BTTV_BOARD_SSAI_ULTRASOUND, "SSAI Ultrasound Video Interface" },

	/* likely broken, vendor id doesn't match the other magic views ...
	 * { 0xa0fca04f, BTTV_BOARD_MAGICTVIEW063, "Guillemot Maxi TV Video 3" }, */

	/* Duplicate PCI ID, reconfigure for this board during the eeprom read.
	* { 0x13eb0070, BTTV_BOARD_HAUPPAUGE_IMPACTVCB,  "Hauppauge ImpactVCB" }, */

	{ 0x109e036e, BTTV_BOARD_CONCEPTRONIC_CTVFMI2,	"Conceptronic CTVFMi v2"},

	/* DVB cards (using pci function .1 for mpeg data xfer) */
	{ 0x001c11bd, BTTV_BOARD_PINNACLESAT,   "Pinnacle PCTV Sat" },
	{ 0x01010071, BTTV_BOARD_NEBULA_DIGITV, "Nebula Electronics DigiTV" },
	{ 0x20007063, BTTV_BOARD_PC_HDTV,       "pcHDTV HD-2000 TV"},
	{ 0x002611bd, BTTV_BOARD_TWINHAN_DST,   "Pinnacle PCTV SAT CI" },
	{ 0x00011822, BTTV_BOARD_TWINHAN_DST,   "Twinhan VisionPlus DVB" },
	{ 0xfc00270f, BTTV_BOARD_TWINHAN_DST,   "ChainTech digitop DST-1000 DVB-S" },
	{ 0x07711461, BTTV_BOARD_AVDVBT_771,    "AVermedia AverTV DVB-T 771" },
	{ 0x07611461, BTTV_BOARD_AVDVBT_761,    "AverMedia AverTV DVB-T 761" },
	{ 0xdb1018ac, BTTV_BOARD_DVICO_DVBT_LITE,    "DViCO FusionHDTV DVB-T Lite" },
	{ 0xdb1118ac, BTTV_BOARD_DVICO_DVBT_LITE,    "Ultraview DVB-T Lite" },
	{ 0xd50018ac, BTTV_BOARD_DVICO_FUSIONHDTV_5_LITE,    "DViCO FusionHDTV 5 Lite" },
	{ 0x00261822, BTTV_BOARD_TWINHAN_DST,	"DNTV Live! Mini "},
	{ 0xd200dbc0, BTTV_BOARD_DVICO_FUSIONHDTV_2,	"DViCO FusionHDTV 2" },
	{ 0x763c008a, BTTV_BOARD_GEOVISION_GV600,	"GeoVision GV-600" },
	{ 0x18011000, BTTV_BOARD_ENLTV_FM_2,	"Encore ENL TV-FM-2" },
	{ 0x763d800a, BTTV_BOARD_GEOVISION_GV800S, "GeoVision GV-800(S) (master)" },
	{ 0x763d800b, BTTV_BOARD_GEOVISION_GV800S_SL,	"GeoVision GV-800(S) (slave)" },
	{ 0x763d800c, BTTV_BOARD_GEOVISION_GV800S_SL,	"GeoVision GV-800(S) (slave)" },
	{ 0x763d800d, BTTV_BOARD_GEOVISION_GV800S_SL,	"GeoVision GV-800(S) (slave)" },

	{ 0x15401830, BTTV_BOARD_PV183,         "Provideo PV183-1" },
	{ 0x15401831, BTTV_BOARD_PV183,         "Provideo PV183-2" },
	{ 0x15401832, BTTV_BOARD_PV183,         "Provideo PV183-3" },
	{ 0x15401833, BTTV_BOARD_PV183,         "Provideo PV183-4" },
	{ 0x15401834, BTTV_BOARD_PV183,         "Provideo PV183-5" },
	{ 0x15401835, BTTV_BOARD_PV183,         "Provideo PV183-6" },
	{ 0x15401836, BTTV_BOARD_PV183,         "Provideo PV183-7" },
	{ 0x15401837, BTTV_BOARD_PV183,         "Provideo PV183-8" },

	{ 0, -1, NULL }
};

/* ----------------------------------------------------------------------- */
/* array with description for bt848 / bt878 tv/grabber cards               */

struct tvcard bttv_tvcards[] = {
	/* ---- card 0x00 ---------------------------------- */
	[BTTV_BOARD_UNKNOWN] = {
		.name		= " *** UNKNOWN/GENERIC *** ",
		.video_inputs	= 4,
		.svhs		= 2,
		.muxsel		= MUXSEL(2, 3, 1, 0),
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_MIRO] = {
		.name		= "MIRO PCTV",
		.video_inputs	= 4,
		/* .audio_inputs= 1, */
		.svhs		= 2,
		.gpiomask	= 15,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 2, 0, 0, 0 },
		.gpiomute 	= 10,
		.needs_tvaudio	= 1,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_HAUPPAUGE] = {
		.name		= "Hauppauge (bt848)",
		.video_inputs	= 4,
		/* .audio_inputs= 1, */
		.svhs		= 2,
		.gpiomask	= 7,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0, 1, 2, 3 },
		.gpiomute 	= 4,
		.needs_tvaudio	= 1,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_STB] = {
		.name		= "STB, Gateway P/N 6000699 (bt848)",
		.video_inputs	= 3,
		/* .audio_inputs= 1, */
		.svhs		= 2,
		.gpiomask	= 7,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 4, 0, 2, 3 },
		.gpiomute 	= 1,
		.no_msp34xx	= 1,
		.needs_tvaudio	= 1,
		.tuner_type     = TUNER_PHILIPS_NTSC,
		.tuner_addr	= ADDR_UNSET,
		.pll            = PLL_28,
		.has_radio      = 1,
	},

	/* ---- card 0x04 ---------------------------------- */
	[BTTV_BOARD_INTEL] = {
		.name		= "Intel Create and Share PCI/ Smart Video Recorder III",
		.video_inputs	= 4,
		/* .audio_inputs= 0, */
		.svhs		= 2,
		.gpiomask	= 0,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0 },
		.needs_tvaudio	= 0,
		.tuner_type	= TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_DIAMOND] = {
		.name		= "Diamond DTV2000",
		.video_inputs	= 4,
		/* .audio_inputs= 1, */
		.svhs		= 2,
		.gpiomask	= 3,
		.muxsel		= MUXSEL(2, 3, 1, 0),
		.gpiomux 	= { 0, 1, 0, 1 },
		.gpiomute 	= 3,
		.needs_tvaudio	= 1,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_AVERMEDIA] = {
		.name		= "AVerMedia TVPhone",
		.video_inputs	= 3,
		/* .audio_inputs= 1, */
		.svhs		= 3,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomask	= 0x0f,
		.gpiomux 	= { 0x0c, 0x04, 0x08, 0x04 },
		/*                0x04 for some cards ?? */
		.needs_tvaudio	= 1,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
		.audio_mode_gpio= avermedia_tvphone_audio,
		.has_remote     = 1,
	},
	[BTTV_BOARD_MATRIX_VISION] = {
		.name		= "MATRIX-Vision MV-Delta",
		.video_inputs	= 5,
		/* .audio_inputs= 1, */
		.svhs		= 3,
		.gpiomask	= 0,
		.muxsel		= MUXSEL(2, 3, 1, 0, 0),
		.gpiomux 	= { 0 },
		.needs_tvaudio	= 1,
		.tuner_type	= TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},

	/* ---- card 0x08 ---------------------------------- */
	[BTTV_BOARD_FLYVIDEO] = {
		.name		= "Lifeview FlyVideo II (Bt848) LR26 / MAXI TV Video PCI2 LR26",
		.video_inputs	= 4,
		/* .audio_inputs= 1, */
		.svhs		= 2,
		.gpiomask	= 0xc00,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0, 0xc00, 0x800, 0x400 },
		.gpiomute 	= 0xc00,
		.needs_tvaudio	= 1,
		.pll		= PLL_28,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_TURBOTV] = {
		.name		= "IMS/IXmicro TurboTV",
		.video_inputs	= 3,
		/* .audio_inputs= 1, */
		.svhs		= 2,
		.gpiomask	= 3,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 1, 1, 2, 3 },
		.needs_tvaudio	= 0,
		.pll		= PLL_28,
		.tuner_type	= TUNER_TEMIC_PAL,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_HAUPPAUGE878] = {
		.name		= "Hauppauge (bt878)",
		.video_inputs	= 4,
		/* .audio_inputs= 1, */
		.svhs		= 2,
		.gpiomask	= 0x0f, /* old: 7 */
		.muxsel		= MUXSEL(2, 0, 1, 1),
		.gpiomux 	= { 0, 1, 2, 3 },
		.gpiomute 	= 4,
		.needs_tvaudio	= 1,
		.pll		= PLL_28,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_MIROPRO] = {
		.name		= "MIRO PCTV pro",
		.video_inputs	= 3,
		/* .audio_inputs= 1, */
		.svhs		= 2,
		.gpiomask	= 0x3014f,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0x20001,0x10001, 0, 0 },
		.gpiomute 	= 10,
		.needs_tvaudio	= 1,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},

	/* ---- card 0x0c ---------------------------------- */
	[BTTV_BOARD_ADSTECH_TV] = {
		.name		= "ADS Technologies Channel Surfer TV (bt848)",
		.video_inputs	= 3,
		/* .audio_inputs= 1, */
		.svhs		= 2,
		.gpiomask	= 15,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 13, 14, 11, 7 },
		.needs_tvaudio	= 1,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_AVERMEDIA98] = {
		.name		= "AVerMedia TVCapture 98",
		.video_inputs	= 3,
		/* .audio_inputs= 4, */
		.svhs		= 2,
		.gpiomask	= 15,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 13, 14, 11, 7 },
		.needs_tvaudio	= 1,
		.msp34xx_alt    = 1,
		.pll		= PLL_28,
		.tuner_type	= TUNER_PHILIPS_PAL,
		.tuner_addr	= ADDR_UNSET,
		.audio_mode_gpio= avermedia_tv_stereo_audio,
		.no_gpioirq     = 1,
	},
	[BTTV_BOARD_VHX] = {
		.name		= "Aimslab Video Highway Xtreme (VHX)",
		.video_inputs	= 3,
		/* .audio_inputs= 1, */
		.svhs		= 2,
		.gpiomask	= 7,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0, 2, 1, 3 }, /* old: {0, 1, 2, 3, 4} */
		.gpiomute 	= 4,
		.needs_tvaudio	= 1,
		.pll		= PLL_28,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_ZOLTRIX] = {
		.name		= "Zoltrix TV-Max",
		.video_inputs	= 3,
		/* .audio_inputs= 1, */
		.svhs		= 2,
		.gpiomask	= 15,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0, 0, 1, 0 },
		.gpiomute 	= 10,
		.needs_tvaudio	= 1,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},

	/* ---- card 0x10 ---------------------------------- */
	[BTTV_BOARD_PIXVIEWPLAYTV] = {
		.name		= "Prolink Pixelview PlayTV (bt878)",
		.video_inputs	= 3,
		/* .audio_inputs= 1, */
		.svhs		= 2,
		.gpiomask	= 0x01fe00,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		/* 2003-10-20 by "Anton A. Arapov" <arapov@mail.ru> */
		.gpiomux        = { 0x001e00, 0, 0x018000, 0x014000 },
		.gpiomute 	= 0x002000,
		.needs_tvaudio	= 1,
		.pll		= PLL_28,
		.tuner_type	= UNSET,
		.tuner_addr     = ADDR_UNSET,
	},
	[BTTV_BOARD_WINVIEW_601] = {
		.name		= "Leadtek WinView 601",
		.video_inputs	= 3,
		/* .audio_inputs= 1, */
		.svhs		= 2,
		.gpiomask	= 0x8300f8,
		.muxsel		= MUXSEL(2, 3, 1, 1, 0),
		.gpiomux 	= { 0x4fa007,0xcfa007,0xcfa007,0xcfa007 },
		.gpiomute 	= 0xcfa007,
		.needs_tvaudio	= 1,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
		.volume_gpio	= winview_volume,
		.has_radio	= 1,
	},
	[BTTV_BOARD_AVEC_INTERCAP] = {
		.name		= "AVEC Intercapture",
		.video_inputs	= 3,
		/* .audio_inputs= 2, */
		.svhs		= 2,
		.gpiomask	= 0,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 1, 0, 0, 0 },
		.needs_tvaudio	= 1,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_LIFE_FLYKIT] = {
		.name		= "Lifeview FlyVideo II EZ /FlyKit LR38 Bt848 (capture only)",
		.video_inputs	= 4,
		/* .audio_inputs= 1, */
		.svhs		= NO_SVHS,
		.gpiomask	= 0x8dff00,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0 },
		.no_msp34xx	= 1,
		.tuner_type	= TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},

	/* ---- card 0x14 ---------------------------------- */
	[BTTV_BOARD_CEI_RAFFLES] = {
		.name		= "CEI Raffles Card",
		.video_inputs	= 3,
		/* .audio_inputs= 3, */
		.svhs		= 2,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_CONFERENCETV] = {
		.name		= "Lifeview FlyVideo 98/ Lucky Star Image World ConferenceTV LR50",
		.video_inputs	= 4,
		/* .audio_inputs= 2,  tuner, line in */
		.svhs		= 2,
		.gpiomask	= 0x1800,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0, 0x800, 0x1000, 0x1000 },
		.gpiomute 	= 0x1800,
		.pll		= PLL_28,
		.tuner_type	= TUNER_PHILIPS_PAL_I,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_PHOEBE_TVMAS] = {
		.name		= "Askey CPH050/ Phoebe Tv Master + FM",
		.video_inputs	= 3,
		/* .audio_inputs= 1, */
		.svhs		= 2,
		.gpiomask	= 0xc00,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0, 1, 0x800, 0x400 },
		.gpiomute 	= 0xc00,
		.needs_tvaudio	= 1,
		.pll		= PLL_28,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_MODTEC_205] = {
		.name		= "Modular Technology MM201/MM202/MM205/MM210/MM215 PCTV, bt878",
		.video_inputs	= 3,
		/* .audio_inputs= 1, */
		.svhs		= NO_SVHS,
		.has_dig_in	= 1,
		.gpiomask	= 7,
		.muxsel		= MUXSEL(2, 3, 0), /* input 2 is digital */
		/* .digital_mode= DIGITAL_MODE_CAMERA, */
		.gpiomux 	= { 0, 0, 0, 0 },
		.no_msp34xx	= 1,
		.pll            = PLL_28,
		.tuner_type     = TUNER_ALPS_TSBB5_PAL_I,
		.tuner_addr	= ADDR_UNSET,
	},

	/* ---- card 0x18 ---------------------------------- */
	[BTTV_BOARD_MAGICTVIEW061] = {
		.name		= "Askey CPH05X/06X (bt878) [many vendors]",
		.video_inputs	= 3,
		/* .audio_inputs= 1, */
		.svhs		= 2,
		.gpiomask	= 0xe00,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= {0x400, 0x400, 0x400, 0x400 },
		.gpiomute 	= 0xc00,
		.needs_tvaudio	= 1,
		.pll		= PLL_28,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
		.has_remote     = 1,
	},
	[BTTV_BOARD_VOBIS_BOOSTAR] = {
		.name           = "Terratec TerraTV+ Version 1.0 (Bt848)/ Terra TValue Version 1.0/ Vobis TV-Boostar",
		.video_inputs	= 3,
		/* .audio_inputs= 1, */
		.svhs		= 2,
		.gpiomask       = 0x1f0fff,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux        = { 0x20000, 0x30000, 0x10000, 0 },
		.gpiomute 	= 0x40000,
		.needs_tvaudio	= 0,
		.tuner_type	= TUNER_PHILIPS_PAL,
		.tuner_addr	= ADDR_UNSET,
		.audio_mode_gpio= terratv_audio,
	},
	[BTTV_BOARD_HAUPPAUG_WCAM] = {
		.name		= "Hauppauge WinCam newer (bt878)",
		.video_inputs	= 4,
		/* .audio_inputs= 1, */
		.svhs		= 3,
		.gpiomask	= 7,
		.muxsel		= MUXSEL(2, 0, 1, 1),
		.gpiomux 	= { 0, 1, 2, 3 },
		.gpiomute 	= 4,
		.needs_tvaudio	= 1,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_MAXI] = {
		.name		= "Lifeview FlyVideo 98/ MAXI TV Video PCI2 LR50",
		.video_inputs	= 4,
		/* .audio_inputs= 2, */
		.svhs		= 2,
		.gpiomask	= 0x1800,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0, 0x800, 0x1000, 0x1000 },
		.gpiomute 	= 0x1800,
		.pll            = PLL_28,
		.tuner_type	= TUNER_PHILIPS_SECAM,
		.tuner_addr	= ADDR_UNSET,
	},

	/* ---- card 0x1c ---------------------------------- */
	[BTTV_BOARD_TERRATV] = {
		.name           = "Terratec TerraTV+ Version 1.1 (bt878)",
		.video_inputs	= 3,
		/* .audio_inputs= 1, */
		.svhs		= 2,
		.gpiomask	= 0x1f0fff,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0x20000, 0x30000, 0x10000, 0x00000 },
		.gpiomute 	= 0x40000,
		.needs_tvaudio	= 0,
		.tuner_type	= TUNER_PHILIPS_PAL,
		.tuner_addr	= ADDR_UNSET,
		.audio_mode_gpio= terratv_audio,
		/* GPIO wiring:
		External 20 pin connector (for Active Radio Upgrade board)
		gpio00: i2c-sda
		gpio01: i2c-scl
		gpio02: om5610-data
		gpio03: om5610-clk
		gpio04: om5610-wre
		gpio05: om5610-stereo
		gpio06: rds6588-davn
		gpio07: Pin 7 n.c.
		gpio08: nIOW
		gpio09+10: nIOR, nSEL ?? (bt878)
			gpio09: nIOR (bt848)
			gpio10: nSEL (bt848)
		Sound Routing:
		gpio16: u2-A0 (1st 4052bt)
		gpio17: u2-A1
		gpio18: u2-nEN
		gpio19: u4-A0 (2nd 4052)
		gpio20: u4-A1
			u4-nEN - GND
		Btspy:
			00000 : Cdrom (internal audio input)
			10000 : ext. Video audio input
			20000 : TV Mono
			a0000 : TV Mono/2
		1a0000 : TV Stereo
			30000 : Radio
			40000 : Mute
	*/

	},
	[BTTV_BOARD_PXC200] = {
		/* Jannik Fritsch <jannik@techfak.uni-bielefeld.de> */
		.name		= "Imagenation PXC200",
		.video_inputs	= 5,
		/* .audio_inputs= 1, */
		.svhs		= 1, /* was: 4 */
		.gpiomask	= 0,
		.muxsel		= MUXSEL(2, 3, 1, 0, 0),
		.gpiomux 	= { 0 },
		.needs_tvaudio	= 1,
		.tuner_type	= TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.muxsel_hook    = PXC200_muxsel,

	},
	[BTTV_BOARD_FLYVIDEO_98] = {
		.name		= "Lifeview FlyVideo 98 LR50",
		.video_inputs	= 4,
		/* .audio_inputs= 1, */
		.svhs		= 2,
		.gpiomask	= 0x1800,  /* 0x8dfe00 */
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0, 0x0800, 0x1000, 0x1000 },
		.gpiomute 	= 0x1800,
		.pll            = PLL_28,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_IPROTV] = {
		.name		= "Formac iProTV, Formac ProTV I (bt848)",
		.video_inputs	= 4,
		/* .audio_inputs= 1, */
		.svhs		= 3,
		.gpiomask	= 1,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 1, 0, 0, 0 },
		.pll            = PLL_28,
		.tuner_type	= TUNER_PHILIPS_PAL,
		.tuner_addr	= ADDR_UNSET,
	},

	/* ---- card 0x20 ---------------------------------- */
	[BTTV_BOARD_INTEL_C_S_PCI] = {
		.name		= "Intel Create and Share PCI/ Smart Video Recorder III",
		.video_inputs	= 4,
		/* .audio_inputs= 0, */
		.svhs		= 2,
		.gpiomask	= 0,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0 },
		.needs_tvaudio	= 0,
		.tuner_type	= TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_TERRATVALUE] = {
		.name           = "Terratec TerraTValue Version Bt878",
		.video_inputs	= 3,
		/* .audio_inputs= 1, */
		.svhs		= 2,
		.gpiomask	= 0xffff00,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0x500, 0, 0x300, 0x900 },
		.gpiomute 	= 0x900,
		.needs_tvaudio	= 1,
		.pll		= PLL_28,
		.tuner_type	= TUNER_PHILIPS_PAL,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_WINFAST2000] = {
		.name		= "Leadtek WinFast 2000/ WinFast 2000 XP",
		.video_inputs	= 4,
		/* .audio_inputs= 1, */
		.svhs		= 2,
		/* TV, CVid, SVid, CVid over SVid connector */
		.muxsel		= MUXSEL(2, 3, 1, 1, 0),
		/* Alexander Varakin <avarakin@hotmail.com> [stereo version] */
		.gpiomask	= 0xb33000,
		.gpiomux 	= { 0x122000,0x1000,0x0000,0x620000 },
		.gpiomute 	= 0x800000,
		/* Audio Routing for "WinFast 2000 XP" (no tv stereo !)
			gpio23 -- hef4052:nEnable (0x800000)
			gpio12 -- hef4052:A1
			gpio13 -- hef4052:A0
		0x0000: external audio
		0x1000: FM
		0x2000: TV
		0x3000: n.c.
		Note: There exists another variant "Winfast 2000" with tv stereo !?
		Note: eeprom only contains FF and pci subsystem id 107d:6606
		*/
		.needs_tvaudio	= 0,
		.pll		= PLL_28,
		.has_radio	= 1,
		.tuner_type	= TUNER_PHILIPS_PAL, /* default for now, gpio reads BFFF06 for Pal bg+dk */
		.tuner_addr	= ADDR_UNSET,
		.audio_mode_gpio= winfast2000_audio,
		.has_remote     = 1,
	},
	[BTTV_BOARD_CHRONOS_VS2] = {
		.name		= "Lifeview FlyVideo 98 LR50 / Chronos Video Shuttle II",
		.video_inputs	= 4,
		/* .audio_inputs= 3, */
		.svhs		= 2,
		.gpiomask	= 0x1800,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0, 0x800, 0x1000, 0x1000 },
		.gpiomute 	= 0x1800,
		.pll		= PLL_28,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},

	/* ---- card 0x24 ---------------------------------- */
	[BTTV_BOARD_TYPHOON_TVIEW] = {
		.name		= "Lifeview FlyVideo 98FM LR50 / Typhoon TView TV/FM Tuner",
		.video_inputs	= 4,
		/* .audio_inputs= 3, */
		.svhs		= 2,
		.gpiomask	= 0x1800,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0, 0x800, 0x1000, 0x1000 },
		.gpiomute 	= 0x1800,
		.pll		= PLL_28,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
		.has_radio	= 1,
	},
	[BTTV_BOARD_PXELVWPLTVPRO] = {
		.name		= "Prolink PixelView PlayTV pro",
		.video_inputs	= 3,
		/* .audio_inputs= 1, */
		.svhs		= 2,
		.gpiomask	= 0xff,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0x21, 0x20, 0x24, 0x2c },
		.gpiomute 	= 0x29,
		.no_msp34xx	= 1,
		.pll		= PLL_28,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_MAGICTVIEW063] = {
		.name		= "Askey CPH06X TView99",
		.video_inputs	= 4,
		/* .audio_inputs= 1, */
		.svhs		= 2,
		.gpiomask	= 0x551e00,
		.muxsel		= MUXSEL(2, 3, 1, 0),
		.gpiomux 	= { 0x551400, 0x551200, 0, 0 },
		.gpiomute 	= 0x551c00,
		.needs_tvaudio	= 1,
		.pll		= PLL_28,
		.tuner_type	= TUNER_PHILIPS_PAL_I,
		.tuner_addr	= ADDR_UNSET,
		.has_remote     = 1,
	},
	[BTTV_BOARD_PINNACLE] = {
		.name		= "Pinnacle PCTV Studio/Rave",
		.video_inputs	= 3,
		/* .audio_inputs= 1, */
		.svhs		= 2,
		.gpiomask	= 0x03000F,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 2, 0xd0001, 0, 0 },
		.gpiomute 	= 1,
		.needs_tvaudio	= 0,
		.pll		= PLL_28,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},

	/* ---- card 0x28 ---------------------------------- */
	[BTTV_BOARD_STB2] = {
		.name		= "STB TV PCI FM, Gateway P/N 6000704 (bt878), 3Dfx VoodooTV 100",
		.video_inputs	= 3,
		/* .audio_inputs= 1, */
		.svhs		= 2,
		.gpiomask	= 7,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 4, 0, 2, 3 },
		.gpiomute 	= 1,
		.no_msp34xx	= 1,
		.needs_tvaudio	= 1,
		.tuner_type     = TUNER_PHILIPS_NTSC,
		.tuner_addr	= ADDR_UNSET,
		.pll            = PLL_28,
		.has_radio      = 1,
	},
	[BTTV_BOARD_AVPHONE98] = {
		.name		= "AVerMedia TVPhone 98",
		.video_inputs	= 3,
		/* .audio_inputs= 4, */
		.svhs		= 2,
		.gpiomask	= 15,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 13, 4, 11, 7 },
		.needs_tvaudio	= 1,
		.pll		= PLL_28,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
		.has_radio	= 1,
		.audio_mode_gpio= avermedia_tvphone_audio,
	},
	[BTTV_BOARD_PV951] = {
		.name		= "ProVideo PV951", /* pic16c54 */
		.video_inputs	= 3,
		/* .audio_inputs= 1, */
		.svhs		= 2,
		.gpiomask	= 0,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0, 0, 0, 0},
		.needs_tvaudio	= 1,
		.no_msp34xx	= 1,
		.pll		= PLL_28,
		.tuner_type	= TUNER_PHILIPS_PAL_I,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_ONAIR_TV] = {
		.name		= "Little OnAir TV",
		.video_inputs	= 3,
		/* .audio_inputs= 1, */
		.svhs		= 2,
		.gpiomask	= 0xe00b,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0xff9ff6, 0xff9ff6, 0xff1ff7, 0 },
		.gpiomute 	= 0xff3ffc,
		.no_msp34xx	= 1,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},

	/* ---- card 0x2c ---------------------------------- */
	[BTTV_BOARD_SIGMA_TVII_FM] = {
		.name		= "Sigma TVII-FM",
		.video_inputs	= 2,
		/* .audio_inputs= 1, */
		.svhs		= NO_SVHS,
		.gpiomask	= 3,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 1, 1, 0, 2 },
		.gpiomute 	= 3,
		.no_msp34xx	= 1,
		.pll		= PLL_NONE,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_MATRIX_VISION2] = {
		.name		= "MATRIX-Vision MV-Delta 2",
		.video_inputs	= 5,
		/* .audio_inputs= 1, */
		.svhs		= 3,
		.gpiomask	= 0,
		.muxsel		= MUXSEL(2, 3, 1, 0, 0),
		.gpiomux 	= { 0 },
		.no_msp34xx	= 1,
		.pll		= PLL_28,
		.tuner_type	= TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_ZOLTRIX_GENIE] = {
		.name		= "Zoltrix Genie TV/FM",
		.video_inputs	= 3,
		/* .audio_inputs= 1, */
		.svhs		= 2,
		.gpiomask	= 0xbcf03f,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0xbc803f, 0xbc903f, 0xbcb03f, 0 },
		.gpiomute 	= 0xbcb03f,
		.no_msp34xx	= 1,
		.pll		= PLL_28,
		.tuner_type	= TUNER_TEMIC_4039FR5_NTSC,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_TERRATVRADIO] = {
		.name		= "Terratec TV/Radio+",
		.video_inputs	= 3,
		/* .audio_inputs= 1, */
		.svhs		= 2,
		.gpiomask	= 0x70000,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0x20000, 0x30000, 0x10000, 0 },
		.gpiomute 	= 0x40000,
		.needs_tvaudio	= 1,
		.no_msp34xx	= 1,
		.pll		= PLL_35,
		.tuner_type	= TUNER_PHILIPS_PAL_I,
		.tuner_addr	= ADDR_UNSET,
		.has_radio	= 1,
	},

	/* ---- card 0x30 ---------------------------------- */
	[BTTV_BOARD_DYNALINK] = {
		.name		= "Askey CPH03x/ Dynalink Magic TView",
		.video_inputs	= 3,
		/* .audio_inputs= 1, */
		.svhs		= 2,
		.gpiomask	= 15,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= {2,0,0,0 },
		.gpiomute 	= 1,
		.needs_tvaudio	= 1,
		.pll		= PLL_28,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_GVBCTV3PCI] = {
		.name		= "IODATA GV-BCTV3/PCI",
		.video_inputs	= 3,
		/* .audio_inputs= 1, */
		.svhs		= 2,
		.gpiomask	= 0x010f00,
		.muxsel		= MUXSEL(2, 3, 0, 0),
		.gpiomux 	= {0x10000, 0, 0x10000, 0 },
		.no_msp34xx	= 1,
		.pll		= PLL_28,
		.tuner_type	= TUNER_ALPS_TSHC6_NTSC,
		.tuner_addr	= ADDR_UNSET,
		.audio_mode_gpio= gvbctv3pci_audio,
	},
	[BTTV_BOARD_PXELVWPLTVPAK] = {
		.name		= "Prolink PV-BT878P+4E / PixelView PlayTV PAK / Lenco MXTV-9578 CP",
		.video_inputs	= 5,
		/* .audio_inputs= 1, */
		.svhs		= 3,
		.has_dig_in	= 1,
		.gpiomask	= 0xAA0000,
		.muxsel		= MUXSEL(2, 3, 1, 1, 0), /* in 4 is digital */
		/* .digital_mode= DIGITAL_MODE_CAMERA, */
		.gpiomux 	= { 0x20000, 0, 0x80000, 0x80000 },
		.gpiomute 	= 0xa8000,
		.no_msp34xx	= 1,
		.pll		= PLL_28,
		.tuner_type	= TUNER_PHILIPS_PAL_I,
		.tuner_addr	= ADDR_UNSET,
		.has_remote	= 1,
		/* GPIO wiring: (different from Rev.4C !)
			GPIO17: U4.A0 (first hef4052bt)
			GPIO19: U4.A1
			GPIO20: U5.A1 (second hef4052bt)
			GPIO21: U4.nEN
			GPIO22: BT832 Reset Line
			GPIO23: A5,A0, U5,nEN
		Note: At i2c=0x8a is a Bt832 chip, which changes to 0x88 after being reset via GPIO22
		*/
	},
	[BTTV_BOARD_EAGLE] = {
		.name           = "Eagle Wireless Capricorn2 (bt878A)",
		.video_inputs   = 4,
		/* .audio_inputs= 1, */
		.svhs           = 2,
		.gpiomask       = 7,
		.muxsel         = MUXSEL(2, 0, 1, 1),
		.gpiomux        = { 0, 1, 2, 3 },
		.gpiomute 	= 4,
		.pll            = PLL_28,
		.tuner_type     = UNSET /* TUNER_ALPS_TMDH2_NTSC */,
		.tuner_addr	= ADDR_UNSET,
	},

	/* ---- card 0x34 ---------------------------------- */
	[BTTV_BOARD_PINNACLEPRO] = {
		/* David Härdeman <david@2gen.com> */
		.name           = "Pinnacle PCTV Studio Pro",
		.video_inputs   = 4,
		/* .audio_inputs= 1, */
		.svhs           = 3,
		.gpiomask       = 0x03000F,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 1, 0xd0001, 0, 0 },
		.gpiomute 	= 10,
				/* sound path (5 sources):
				MUX1 (mask 0x03), Enable Pin 0x08 (0=enable, 1=disable)
					0= ext. Audio IN
					1= from MUX2
					2= Mono TV sound from Tuner
					3= not connected
				MUX2 (mask 0x30000):
					0,2,3= from MSP34xx
					1= FM stereo Radio from Tuner */
		.needs_tvaudio  = 0,
		.pll            = PLL_28,
		.tuner_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_TVIEW_RDS_FM] = {
		/* Claas Langbehn <claas@bigfoot.com>,
		Sven Grothklags <sven@upb.de> */
		.name		= "Typhoon TView RDS + FM Stereo / KNC1 TV Station RDS",
		.video_inputs	= 4,
		/* .audio_inputs= 3, */
		.svhs		= 2,
		.gpiomask	= 0x1c,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0, 0, 0x10, 8 },
		.gpiomute 	= 4,
		.needs_tvaudio	= 1,
		.pll		= PLL_28,
		.tuner_type	= TUNER_PHILIPS_PAL,
		.tuner_addr	= ADDR_UNSET,
		.has_radio	= 1,
	},
	[BTTV_BOARD_LIFETEC_9415] = {
		/* Tim Röstermundt <rosterm@uni-muenster.de>
		in de.comp.os.unix.linux.hardware:
			options bttv card=0 pll=1 radio=1 gpiomask=0x18e0
			gpiomux =0x44c71f,0x44d71f,0,0x44d71f,0x44dfff
			options tuner type=5 */
		.name		= "Lifeview FlyVideo 2000 /FlyVideo A2/ Lifetec LT 9415 TV [LR90]",
		.video_inputs	= 4,
		/* .audio_inputs= 1, */
		.svhs		= 2,
		.gpiomask	= 0x18e0,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0x0000,0x0800,0x1000,0x1000 },
		.gpiomute 	= 0x18e0,
			/* For cards with tda9820/tda9821:
				0x0000: Tuner normal stereo
				0x0080: Tuner A2 SAP (second audio program = Zweikanalton)
				0x0880: Tuner A2 stereo */
		.pll		= PLL_28,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_BESTBUY_EASYTV] = {
		/* Miguel Angel Alvarez <maacruz@navegalia.com>
		old Easy TV BT848 version (model CPH031) */
		.name           = "Askey CPH031/ BESTBUY Easy TV",
		.video_inputs	= 4,
		/* .audio_inputs= 1, */
		.svhs           = 2,
		.gpiomask       = 0xF,
		.muxsel         = MUXSEL(2, 3, 1, 0),
		.gpiomux        = { 2, 0, 0, 0 },
		.gpiomute 	= 10,
		.needs_tvaudio  = 0,
		.pll		= PLL_28,
		.tuner_type	= TUNER_TEMIC_PAL,
		.tuner_addr	= ADDR_UNSET,
	},

	/* ---- card 0x38 ---------------------------------- */
	[BTTV_BOARD_FLYVIDEO_98FM] = {
		/* Gordon Heydon <gjheydon@bigfoot.com ('98) */
		.name           = "Lifeview FlyVideo 98FM LR50",
		.video_inputs   = 4,
		/* .audio_inputs= 3, */
		.svhs           = 2,
		.gpiomask       = 0x1800,
		.muxsel         = MUXSEL(2, 3, 1, 1),
		.gpiomux        = { 0, 0x800, 0x1000, 0x1000 },
		.gpiomute 	= 0x1800,
		.pll            = PLL_28,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.tuner_addr	= ADDR_UNSET,
	},
		/* This is the ultimate cheapo capture card
		* just a BT848A on a small PCB!
		* Steve Hosgood <steve@equiinet.com> */
	[BTTV_BOARD_GRANDTEC] = {
		.name           = "GrandTec 'Grand Video Capture' (Bt848)",
		.video_inputs   = 2,
		/* .audio_inputs= 0, */
		.svhs           = 1,
		.gpiomask       = 0,
		.muxsel         = MUXSEL(3, 1),
		.gpiomux        = { 0 },
		.needs_tvaudio  = 0,
		.no_msp34xx     = 1,
		.pll            = PLL_35,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_ASKEY_CPH060] = {
		/* Daniel Herrington <daniel.herrington@home.com> */
		.name           = "Askey CPH060/ Phoebe TV Master Only (No FM)",
		.video_inputs   = 3,
		/* .audio_inputs= 1, */
		.svhs           = 2,
		.gpiomask       = 0xe00,
		.muxsel         = MUXSEL(2, 3, 1, 1),
		.gpiomux        = { 0x400, 0x400, 0x400, 0x400 },
		.gpiomute 	= 0x800,
		.needs_tvaudio  = 1,
		.pll            = PLL_28,
		.tuner_type     = TUNER_TEMIC_4036FY5_NTSC,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_ASKEY_CPH03X] = {
		/* Matti Mottus <mottus@physic.ut.ee> */
		.name		= "Askey CPH03x TV Capturer",
		.video_inputs	= 4,
		/* .audio_inputs= 1, */
		.svhs		= 2,
		.gpiomask       = 0x03000F,
		.muxsel		= MUXSEL(2, 3, 1, 0),
		.gpiomux        = { 2, 0, 0, 0 },
		.gpiomute 	= 1,
		.pll            = PLL_28,
		.tuner_type	= TUNER_TEMIC_PAL,
		.tuner_addr	= ADDR_UNSET,
		.has_remote	= 1,
	},

	/* ---- card 0x3c ---------------------------------- */
	[BTTV_BOARD_MM100PCTV] = {
		/* Philip Blundell <philb@gnu.org> */
		.name           = "Modular Technology MM100PCTV",
		.video_inputs   = 2,
		/* .audio_inputs= 2, */
		.svhs		= NO_SVHS,
		.gpiomask       = 11,
		.muxsel         = MUXSEL(2, 3, 1, 1),
		.gpiomux        = { 2, 0, 0, 1 },
		.gpiomute 	= 8,
		.pll            = PLL_35,
		.tuner_type     = TUNER_TEMIC_PAL,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_GMV1] = {
		/* Adrian Cox <adrian@humboldt.co.uk */
		.name		= "AG Electronics GMV1",
		.video_inputs   = 2,
		/* .audio_inputs= 0, */
		.svhs		= 1,
		.gpiomask       = 0xF,
		.muxsel		= MUXSEL(2, 2),
		.gpiomux        = { },
		.no_msp34xx     = 1,
		.needs_tvaudio  = 0,
		.pll		= PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_BESTBUY_EASYTV2] = {
		/* Miguel Angel Alvarez <maacruz@navegalia.com>
		new Easy TV BT878 version (model CPH061)
		special thanks to Informatica Mieres for providing the card */
		.name           = "Askey CPH061/ BESTBUY Easy TV (bt878)",
		.video_inputs	= 3,
		/* .audio_inputs= 2, */
		.svhs           = 2,
		.gpiomask       = 0xFF,
		.muxsel         = MUXSEL(2, 3, 1, 0),
		.gpiomux        = { 1, 0, 4, 4 },
		.gpiomute 	= 9,
		.needs_tvaudio  = 0,
		.pll		= PLL_28,
		.tuner_type	= TUNER_PHILIPS_PAL,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_ATI_TVWONDER] = {
		/* Lukas Gebauer <geby@volny.cz> */
		.name		= "ATI TV-Wonder",
		.video_inputs	= 3,
		/* .audio_inputs= 1, */
		.svhs		= 2,
		.gpiomask	= 0xf03f,
		.muxsel		= MUXSEL(2, 3, 1, 0),
		.gpiomux 	= { 0xbffe, 0, 0xbfff, 0 },
		.gpiomute 	= 0xbffe,
		.pll		= PLL_28,
		.tuner_type	= TUNER_TEMIC_4006FN5_MULTI_PAL,
		.tuner_addr	= ADDR_UNSET,
	},

	/* ---- card 0x40 ---------------------------------- */
	[BTTV_BOARD_ATI_TVWONDERVE] = {
		/* Lukas Gebauer <geby@volny.cz> */
		.name		= "ATI TV-Wonder VE",
		.video_inputs	= 2,
		/* .audio_inputs= 1, */
		.svhs		= NO_SVHS,
		.gpiomask	= 1,
		.muxsel		= MUXSEL(2, 3, 0, 1),
		.gpiomux 	= { 0, 0, 1, 0 },
		.no_msp34xx	= 1,
		.pll		= PLL_28,
		.tuner_type	= TUNER_TEMIC_4006FN5_MULTI_PAL,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_FLYVIDEO2000] = {
		/* DeeJay <deejay@westel900.net (2000S) */
		.name           = "Lifeview FlyVideo 2000S LR90",
		.video_inputs   = 3,
		/* .audio_inputs= 3, */
		.svhs           = 2,
		.gpiomask	= 0x18e0,
		.muxsel		= MUXSEL(2, 3, 0, 1),
				/* Radio changed from 1e80 to 0x800 to make
				FlyVideo2000S in .hu happy (gm)*/
				/* -dk-???: set mute=0x1800 for tda9874h daughterboard */
		.gpiomux 	= { 0x0000,0x0800,0x1000,0x1000 },
		.gpiomute 	= 0x1800,
		.audio_mode_gpio= fv2000s_audio,
		.no_msp34xx	= 1,
		.no_tda9875	= 1,
		.needs_tvaudio  = 1,
		.pll            = PLL_28,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_TERRATVALUER] = {
		.name		= "Terratec TValueRadio",
		.video_inputs	= 3,
		/* .audio_inputs= 1, */
		.svhs		= 2,
		.gpiomask	= 0xffff00,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0x500, 0x500, 0x300, 0x900 },
		.gpiomute 	= 0x900,
		.needs_tvaudio	= 1,
		.pll		= PLL_28,
		.tuner_type	= TUNER_PHILIPS_PAL,
		.tuner_addr	= ADDR_UNSET,
		.has_radio	= 1,
	},
	[BTTV_BOARD_GVBCTV4PCI] = {
		/* TANAKA Kei <peg00625@nifty.com> */
		.name           = "IODATA GV-BCTV4/PCI",
		.video_inputs   = 3,
		/* .audio_inputs= 1, */
		.svhs           = 2,
		.gpiomask       = 0x010f00,
		.muxsel         = MUXSEL(2, 3, 0, 0),
		.gpiomux        = {0x10000, 0, 0x10000, 0 },
		.no_msp34xx     = 1,
		.pll            = PLL_28,
		.tuner_type     = TUNER_SHARP_2U5JF5540_NTSC,
		.tuner_addr	= ADDR_UNSET,
		.audio_mode_gpio= gvbctv3pci_audio,
	},

	/* ---- card 0x44 ---------------------------------- */
	[BTTV_BOARD_VOODOOTV_FM] = {
		.name           = "3Dfx VoodooTV FM (Euro)",
		/* try "insmod msp3400 simple=0" if you have
		* sound problems with this card. */
		.video_inputs   = 4,
		/* .audio_inputs= 1, */
		.svhs           = NO_SVHS,
		.gpiomask       = 0x4f8a00,
		/* 0x100000: 1=MSP enabled (0=disable again)
		* 0x010000: Connected to "S0" on tda9880 (0=Pal/BG, 1=NTSC) */
		.gpiomux        = {0x947fff, 0x987fff,0x947fff,0x947fff },
		.gpiomute 	= 0x947fff,
		/* tvtuner, radio,   external,internal, mute,  stereo
		* tuner, Composit, SVid, Composit-on-Svid-adapter */
		.muxsel         = MUXSEL(2, 3, 0, 1),
		.tuner_type     = TUNER_MT2032,
		.tuner_addr	= ADDR_UNSET,
		.pll		= PLL_28,
		.has_radio	= 1,
	},
	[BTTV_BOARD_VOODOOTV_200] = {
		.name           = "VoodooTV 200 (USA)",
		/* try "insmod msp3400 simple=0" if you have
		* sound problems with this card. */
		.video_inputs   = 4,
		/* .audio_inputs= 1, */
		.svhs           = NO_SVHS,
		.gpiomask       = 0x4f8a00,
		/* 0x100000: 1=MSP enabled (0=disable again)
		* 0x010000: Connected to "S0" on tda9880 (0=Pal/BG, 1=NTSC) */
		.gpiomux        = {0x947fff, 0x987fff,0x947fff,0x947fff },
		.gpiomute 	= 0x947fff,
		/* tvtuner, radio,   external,internal, mute,  stereo
		* tuner, Composit, SVid, Composit-on-Svid-adapter */
		.muxsel         = MUXSEL(2, 3, 0, 1),
		.tuner_type     = TUNER_MT2032,
		.tuner_addr	= ADDR_UNSET,
		.pll		= PLL_28,
		.has_radio	= 1,
	},
	[BTTV_BOARD_AIMMS] = {
		/* Philip Blundell <pb@nexus.co.uk> */
		.name           = "Active Imaging AIMMS",
		.video_inputs   = 1,
		/* .audio_inputs= 0, */
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.pll            = PLL_28,
		.muxsel         = MUXSEL(2),
		.gpiomask       = 0
	},
	[BTTV_BOARD_PV_BT878P_PLUS] = {
		/* Tomasz Pyra <hellfire@sedez.iq.pl> */
		.name           = "Prolink Pixelview PV-BT878P+ (Rev.4C,8E)",
		.video_inputs   = 3,
		/* .audio_inputs= 4, */
		.svhs           = 2,
		.gpiomask       = 15,
		.muxsel         = MUXSEL(2, 3, 1, 1),
		.gpiomux        = { 0, 0, 11, 7 }, /* TV and Radio with same GPIO ! */
		.gpiomute 	= 13,
		.needs_tvaudio  = 1,
		.pll            = PLL_28,
		.tuner_type     = TUNER_LG_PAL_I_FM,
		.tuner_addr	= ADDR_UNSET,
		.has_remote     = 1,
		/* GPIO wiring:
			GPIO0: U4.A0 (hef4052bt)
			GPIO1: U4.A1
			GPIO2: U4.A1 (second hef4052bt)
			GPIO3: U4.nEN, U5.A0, A5.nEN
			GPIO8-15: vrd866b ?
		*/
	},
	[BTTV_BOARD_FLYVIDEO98EZ] = {
		.name		= "Lifeview FlyVideo 98EZ (capture only) LR51",
		.video_inputs	= 4,
		/* .audio_inputs= 0, */
		.svhs		= 2,
		/* AV1, AV2, SVHS, CVid adapter on SVHS */
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.pll		= PLL_28,
		.no_msp34xx	= 1,
		.tuner_type	= TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},

	/* ---- card 0x48 ---------------------------------- */
	[BTTV_BOARD_PV_BT878P_9B] = {
		/* Dariusz Kowalewski <darekk@automex.pl> */
		.name		= "Prolink Pixelview PV-BT878P+9B (PlayTV Pro rev.9B FM+NICAM)",
		.video_inputs	= 4,
		/* .audio_inputs= 1, */
		.svhs		= 2,
		.gpiomask	= 0x3f,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0x01, 0x00, 0x03, 0x03 },
		.gpiomute 	= 0x09,
		.needs_tvaudio  = 1,
		.no_msp34xx	= 1,
		.no_tda9875	= 1,
		.pll		= PLL_28,
		.tuner_type	= TUNER_PHILIPS_PAL,
		.tuner_addr	= ADDR_UNSET,
		.audio_mode_gpio= pvbt878p9b_audio, /* Note: not all cards have stereo */
		.has_radio	= 1,  /* Note: not all cards have radio */
		.has_remote     = 1,
		/* GPIO wiring:
			GPIO0: A0 hef4052
			GPIO1: A1 hef4052
			GPIO3: nEN hef4052
			GPIO8-15: vrd866b
			GPIO20,22,23: R30,R29,R28
		*/
	},
	[BTTV_BOARD_SENSORAY311] = {
		/* Clay Kunz <ckunz@mail.arc.nasa.gov> */
		/* you must jumper JP5 for the card to work */
		.name           = "Sensoray 311",
		.video_inputs   = 5,
		/* .audio_inputs= 0, */
		.svhs           = 4,
		.gpiomask       = 0,
		.muxsel         = MUXSEL(2, 3, 1, 0, 0),
		.gpiomux        = { 0 },
		.needs_tvaudio  = 0,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_RV605] = {
		/* Miguel Freitas <miguel@cetuc.puc-rio.br> */
		.name           = "RemoteVision MX (RV605)",
		.video_inputs   = 16,
		/* .audio_inputs= 0, */
		.svhs           = NO_SVHS,
		.gpiomask       = 0x00,
		.gpiomask2      = 0x07ff,
		.muxsel         = MUXSEL(3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3),
		.no_msp34xx     = 1,
		.no_tda9875     = 1,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.muxsel_hook    = rv605_muxsel,
	},
	[BTTV_BOARD_POWERCLR_MTV878] = {
		.name           = "Powercolor MTV878/ MTV878R/ MTV878F",
		.video_inputs   = 3,
		/* .audio_inputs= 2, */
		.svhs           = 2,
		.gpiomask       = 0x1C800F,  /* Bit0-2: Audio select, 8-12:remote control 14:remote valid 15:remote reset */
		.muxsel         = MUXSEL(2, 1, 1),
		.gpiomux        = { 0, 1, 2, 2 },
		.gpiomute 	= 4,
		.needs_tvaudio  = 0,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.tuner_addr	= ADDR_UNSET,
		.pll		= PLL_28,
		.has_radio	= 1,
	},

	/* ---- card 0x4c ---------------------------------- */
	[BTTV_BOARD_WINDVR] = {
		/* Masaki Suzuki <masaki@btree.org> */
		.name           = "Canopus WinDVR PCI (COMPAQ Presario 3524JP, 5112JP)",
		.video_inputs   = 3,
		/* .audio_inputs= 1, */
		.svhs           = 2,
		.gpiomask       = 0x140007,
		.muxsel         = MUXSEL(2, 3, 1, 1),
		.gpiomux        = { 0, 1, 2, 3 },
		.gpiomute 	= 4,
		.tuner_type     = TUNER_PHILIPS_NTSC,
		.tuner_addr	= ADDR_UNSET,
		.audio_mode_gpio= windvr_audio,
	},
	[BTTV_BOARD_GRANDTEC_MULTI] = {
		.name           = "GrandTec Multi Capture Card (Bt878)",
		.video_inputs   = 4,
		/* .audio_inputs= 0, */
		.svhs           = NO_SVHS,
		.gpiomask       = 0,
		.muxsel         = MUXSEL(2, 3, 1, 0),
		.gpiomux        = { 0 },
		.needs_tvaudio  = 0,
		.no_msp34xx     = 1,
		.pll            = PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_KWORLD] = {
		.name           = "Jetway TV/Capture JW-TV878-FBK, Kworld KW-TV878RF",
		.video_inputs   = 4,
		/* .audio_inputs= 3, */
		.svhs           = 2,
		.gpiomask       = 7,
		/* Tuner, SVid, SVHS, SVid to SVHS connector */
		.muxsel         = MUXSEL(2, 3, 1, 1),
		.gpiomux        = { 0, 0, 4, 4 },/* Yes, this tuner uses the same audio output for TV and FM radio!
						* This card lacks external Audio In, so we mute it on Ext. & Int.
						* The PCB can take a sbx1637/sbx1673, wiring unknown.
						* This card lacks PCI subsystem ID, sigh.
						* gpiomux =1: lower volume, 2+3: mute
						* btwincap uses 0x80000/0x80003
						*/
		.gpiomute 	= 4,
		.needs_tvaudio  = 0,
		.no_msp34xx     = 1,
		.pll            = PLL_28,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.tuner_addr	= ADDR_UNSET,
		/* Samsung TCPA9095PC27A (BG+DK), philips compatible, w/FM, stereo and
		radio signal strength indicators work fine. */
		.has_radio	= 1,
		/* GPIO Info:
			GPIO0,1:   HEF4052 A0,A1
			GPIO2:     HEF4052 nENABLE
			GPIO3-7:   n.c.
			GPIO8-13:  IRDC357 data0-5 (data6 n.c. ?) [chip not present on my card]
			GPIO14,15: ??
			GPIO16-21: n.c.
			GPIO22,23: ??
			??       : mtu8b56ep microcontroller for IR (GPIO wiring unknown)*/
	},
	[BTTV_BOARD_DSP_TCVIDEO] = {
		/* Arthur Tetzlaff-Deas, DSP Design Ltd <software@dspdesign.com> */
		.name           = "DSP Design TCVIDEO",
		.video_inputs   = 4,
		.svhs           = NO_SVHS,
		.muxsel         = MUXSEL(2, 3, 1, 0),
		.pll            = PLL_28,
		.tuner_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
	},

		/* ---- card 0x50 ---------------------------------- */
	[BTTV_BOARD_HAUPPAUGEPVR] = {
		.name           = "Hauppauge WinTV PVR",
		.video_inputs   = 4,
		/* .audio_inputs= 1, */
		.svhs           = 2,
		.muxsel         = MUXSEL(2, 0, 1, 1),
		.needs_tvaudio  = 1,
		.pll            = PLL_28,
		.tuner_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,

		.gpiomask       = 7,
		.gpiomux        = {7},
	},
	[BTTV_BOARD_GVBCTV5PCI] = {
		.name           = "IODATA GV-BCTV5/PCI",
		.video_inputs   = 3,
		/* .audio_inputs= 1, */
		.svhs           = 2,
		.gpiomask       = 0x0f0f80,
		.muxsel         = MUXSEL(2, 3, 1, 0),
		.gpiomux        = {0x030000, 0x010000, 0, 0 },
		.gpiomute 	= 0x020000,
		.no_msp34xx     = 1,
		.pll            = PLL_28,
		.tuner_type     = TUNER_PHILIPS_NTSC_M,
		.tuner_addr	= ADDR_UNSET,
		.audio_mode_gpio= gvbctv5pci_audio,
		.has_radio      = 1,
	},
	[BTTV_BOARD_OSPREY1x0] = {
		.name           = "Osprey 100/150 (878)", /* 0x1(2|3)-45C6-C1 */
		.video_inputs   = 4,                  /* id-inputs-clock */
		/* .audio_inputs= 0, */
		.svhs           = 3,
		.muxsel         = MUXSEL(3, 2, 0, 1),
		.pll            = PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.no_msp34xx     = 1,
		.no_tda9875     = 1,
		.no_tda7432     = 1,
	},
	[BTTV_BOARD_OSPREY1x0_848] = {
		.name           = "Osprey 100/150 (848)", /* 0x04-54C0-C1 & older boards */
		.video_inputs   = 3,
		/* .audio_inputs= 0, */
		.svhs           = 2,
		.muxsel         = MUXSEL(2, 3, 1),
		.pll            = PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.no_msp34xx     = 1,
		.no_tda9875     = 1,
		.no_tda7432     = 1,
	},

		/* ---- card 0x54 ---------------------------------- */
	[BTTV_BOARD_OSPREY101_848] = {
		.name           = "Osprey 101 (848)", /* 0x05-40C0-C1 */
		.video_inputs   = 2,
		/* .audio_inputs= 0, */
		.svhs           = 1,
		.muxsel         = MUXSEL(3, 1),
		.pll            = PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.no_msp34xx     = 1,
		.no_tda9875     = 1,
		.no_tda7432     = 1,
	},
	[BTTV_BOARD_OSPREY1x1] = {
		.name           = "Osprey 101/151",       /* 0x1(4|5)-0004-C4 */
		.video_inputs   = 1,
		/* .audio_inputs= 0, */
		.svhs           = NO_SVHS,
		.muxsel         = MUXSEL(0),
		.pll            = PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.no_msp34xx     = 1,
		.no_tda9875     = 1,
		.no_tda7432     = 1,
	},
	[BTTV_BOARD_OSPREY1x1_SVID] = {
		.name           = "Osprey 101/151 w/ svid",  /* 0x(16|17|20)-00C4-C1 */
		.video_inputs   = 2,
		/* .audio_inputs= 0, */
		.svhs           = 1,
		.muxsel         = MUXSEL(0, 1),
		.pll            = PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.no_msp34xx     = 1,
		.no_tda9875     = 1,
		.no_tda7432     = 1,
	},
	[BTTV_BOARD_OSPREY2xx] = {
		.name           = "Osprey 200/201/250/251",  /* 0x1(8|9|E|F)-0004-C4 */
		.video_inputs   = 1,
		/* .audio_inputs= 1, */
		.svhs           = NO_SVHS,
		.muxsel         = MUXSEL(0),
		.pll            = PLL_28,
		.tuner_type	= TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.no_msp34xx     = 1,
		.no_tda9875     = 1,
		.no_tda7432     = 1,
	},

		/* ---- card 0x58 ---------------------------------- */
	[BTTV_BOARD_OSPREY2x0_SVID] = {
		.name           = "Osprey 200/250",   /* 0x1(A|B)-00C4-C1 */
		.video_inputs   = 2,
		/* .audio_inputs= 1, */
		.svhs           = 1,
		.muxsel         = MUXSEL(0, 1),
		.pll            = PLL_28,
		.tuner_type	= TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.no_msp34xx     = 1,
		.no_tda9875     = 1,
		.no_tda7432     = 1,
	},
	[BTTV_BOARD_OSPREY2x0] = {
		.name           = "Osprey 210/220/230",   /* 0x1(A|B)-04C0-C1 */
		.video_inputs   = 2,
		/* .audio_inputs= 1, */
		.svhs           = 1,
		.muxsel         = MUXSEL(2, 3),
		.pll            = PLL_28,
		.tuner_type	= TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.no_msp34xx     = 1,
		.no_tda9875     = 1,
		.no_tda7432     = 1,
	},
	[BTTV_BOARD_OSPREY500] = {
		.name           = "Osprey 500",   /* 500 */
		.video_inputs   = 2,
		/* .audio_inputs= 1, */
		.svhs           = 1,
		.muxsel         = MUXSEL(2, 3),
		.pll            = PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.no_msp34xx     = 1,
		.no_tda9875     = 1,
		.no_tda7432     = 1,
	},
	[BTTV_BOARD_OSPREY540] = {
		.name           = "Osprey 540",   /* 540 */
		.video_inputs   = 4,
		/* .audio_inputs= 1, */
		.pll            = PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.no_msp34xx     = 1,
		.no_tda9875     = 1,
		.no_tda7432     = 1,
	},

		/* ---- card 0x5C ---------------------------------- */
	[BTTV_BOARD_OSPREY2000] = {
		.name           = "Osprey 2000",  /* 2000 */
		.video_inputs   = 2,
		/* .audio_inputs= 1, */
		.svhs           = 1,
		.muxsel         = MUXSEL(2, 3),
		.pll            = PLL_28,
		.tuner_type	= TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.no_msp34xx     = 1,
		.no_tda9875     = 1,
		.no_tda7432     = 1,      /* must avoid, conflicts with the bt860 */
	},
	[BTTV_BOARD_IDS_EAGLE] = {
		/* M G Berberich <berberic@forwiss.uni-passau.de> */
		.name           = "IDS Eagle",
		.video_inputs   = 4,
		/* .audio_inputs= 0, */
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.svhs           = NO_SVHS,
		.gpiomask       = 0,
		.muxsel         = MUXSEL(2, 2, 2, 2),
		.muxsel_hook    = eagle_muxsel,
		.no_msp34xx     = 1,
		.no_tda9875     = 1,
		.pll            = PLL_28,
	},
	[BTTV_BOARD_PINNACLESAT] = {
		.name           = "Pinnacle PCTV Sat",
		.video_inputs   = 2,
		/* .audio_inputs= 0, */
		.svhs           = 1,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.no_msp34xx     = 1,
		.no_tda9875     = 1,
		.no_tda7432     = 1,
		.muxsel         = MUXSEL(3, 1),
		.pll            = PLL_28,
		.no_gpioirq     = 1,
		.has_dvb        = 1,
	},
	[BTTV_BOARD_FORMAC_PROTV] = {
		.name           = "Formac ProTV II (bt878)",
		.video_inputs   = 4,
		/* .audio_inputs= 1, */
		.svhs           = 3,
		.gpiomask       = 2,
		/* TV, Comp1, Composite over SVID con, SVID */
		.muxsel         = MUXSEL(2, 3, 1, 1),
		.gpiomux        = { 2, 2, 0, 0 },
		.pll            = PLL_28,
		.has_radio      = 1,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.tuner_addr	= ADDR_UNSET,
	/* sound routing:
		GPIO=0x00,0x01,0x03: mute (?)
		0x02: both TV and radio (tuner: FM1216/I)
		The card has onboard audio connectors labeled "cdrom" and "board",
		not soldered here, though unknown wiring.
		Card lacks: external audio in, pci subsystem id.
	*/
	},

		/* ---- card 0x60 ---------------------------------- */
	[BTTV_BOARD_MACHTV] = {
		.name           = "MachTV",
		.video_inputs   = 3,
		/* .audio_inputs= 1, */
		.svhs           = NO_SVHS,
		.gpiomask       = 7,
		.muxsel         = MUXSEL(2, 3, 1, 1),
		.gpiomux        = { 0, 1, 2, 3},
		.gpiomute 	= 4,
		.needs_tvaudio  = 1,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.tuner_addr	= ADDR_UNSET,
		.pll            = PLL_28,
	},
	[BTTV_BOARD_EURESYS_PICOLO] = {
		.name           = "Euresys Picolo",
		.video_inputs   = 3,
		/* .audio_inputs= 0, */
		.svhs           = 2,
		.gpiomask       = 0,
		.no_msp34xx     = 1,
		.no_tda9875     = 1,
		.no_tda7432     = 1,
		.muxsel         = MUXSEL(2, 0, 1),
		.pll            = PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_PV150] = {
		/* Luc Van Hoeylandt <luc@e-magic.be> */
		.name           = "ProVideo PV150", /* 0x4f */
		.video_inputs   = 2,
		/* .audio_inputs= 0, */
		.svhs           = NO_SVHS,
		.gpiomask       = 0,
		.muxsel         = MUXSEL(2, 3),
		.gpiomux        = { 0 },
		.needs_tvaudio  = 0,
		.no_msp34xx     = 1,
		.pll            = PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_AD_TVK503] = {
		/* Hiroshi Takekawa <sian@big.or.jp> */
		/* This card lacks subsystem ID */
		.name           = "AD-TVK503", /* 0x63 */
		.video_inputs   = 4,
		/* .audio_inputs= 1, */
		.svhs           = 2,
		.gpiomask       = 0x001e8007,
		.muxsel         = MUXSEL(2, 3, 1, 0),
		/*                  Tuner, Radio, external, internal, off,  on */
		.gpiomux        = { 0x08,  0x0f,  0x0a,     0x08 },
		.gpiomute 	= 0x0f,
		.needs_tvaudio  = 0,
		.no_msp34xx     = 1,
		.pll            = PLL_28,
		.tuner_type     = TUNER_PHILIPS_NTSC,
		.tuner_addr	= ADDR_UNSET,
		.audio_mode_gpio= adtvk503_audio,
	},

		/* ---- card 0x64 ---------------------------------- */
	[BTTV_BOARD_HERCULES_SM_TV] = {
		.name           = "Hercules Smart TV Stereo",
		.video_inputs   = 4,
		/* .audio_inputs= 1, */
		.svhs           = 2,
		.gpiomask       = 0x00,
		.muxsel         = MUXSEL(2, 3, 1, 1),
		.needs_tvaudio  = 1,
		.no_msp34xx     = 1,
		.pll            = PLL_28,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.tuner_addr	= ADDR_UNSET,
		/* Notes:
		- card lacks subsystem ID
		- stereo variant w/ daughter board with tda9874a @0xb0
		- Audio Routing:
			always from tda9874 independent of GPIO (?)
			external line in: unknown
		- Other chips: em78p156elp @ 0x96 (probably IR remote control)
			hef4053 (instead 4052) for unknown function
		*/
	},
	[BTTV_BOARD_PACETV] = {
		.name           = "Pace TV & Radio Card",
		.video_inputs   = 4,
		/* .audio_inputs= 1, */
		.svhs           = 2,
		/* Tuner, CVid, SVid, CVid over SVid connector */
		.muxsel         = MUXSEL(2, 3, 1, 1),
		.gpiomask       = 0,
		.no_tda9875     = 1,
		.no_tda7432     = 1,
		.tuner_type     = TUNER_PHILIPS_PAL_I,
		.tuner_addr	= ADDR_UNSET,
		.has_radio      = 1,
		.pll            = PLL_28,
		/* Bt878, Bt832, FI1246 tuner; no pci subsystem id
		only internal line out: (4pin header) RGGL
		Radio must be decoded by msp3410d (not routed through)*/
		/*
		.digital_mode   = DIGITAL_MODE_CAMERA,  todo!
		*/
	},
	[BTTV_BOARD_IVC200] = {
		/* Chris Willing <chris@vislab.usyd.edu.au> */
		.name           = "IVC-200",
		.video_inputs   = 1,
		/* .audio_inputs= 0, */
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.svhs           = NO_SVHS,
		.gpiomask       = 0xdf,
		.muxsel         = MUXSEL(2),
		.pll            = PLL_28,
	},
	[BTTV_BOARD_IVCE8784] = {
		.name           = "IVCE-8784",
		.video_inputs   = 1,
		/* .audio_inputs= 0, */
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr     = ADDR_UNSET,
		.svhs           = NO_SVHS,
		.gpiomask       = 0xdf,
		.muxsel         = MUXSEL(2),
		.pll            = PLL_28,
	},
	[BTTV_BOARD_XGUARD] = {
		.name           = "Grand X-Guard / Trust 814PCI",
		.video_inputs   = 16,
		/* .audio_inputs= 0, */
		.svhs           = NO_SVHS,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.gpiomask2      = 0xff,
		.muxsel         = MUXSEL(2,2,2,2, 3,3,3,3, 1,1,1,1, 0,0,0,0),
		.muxsel_hook    = xguard_muxsel,
		.no_msp34xx     = 1,
		.no_tda9875     = 1,
		.no_tda7432     = 1,
		.pll            = PLL_28,
	},

		/* ---- card 0x68 ---------------------------------- */
	[BTTV_BOARD_NEBULA_DIGITV] = {
		.name           = "Nebula Electronics DigiTV",
		.video_inputs   = 1,
		.svhs           = NO_SVHS,
		.muxsel         = MUXSEL(2, 3, 1, 0),
		.no_msp34xx     = 1,
		.no_tda9875     = 1,
		.no_tda7432     = 1,
		.pll            = PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.has_dvb        = 1,
		.has_remote	= 1,
		.gpiomask	= 0x1b,
		.no_gpioirq     = 1,
	},
	[BTTV_BOARD_PV143] = {
		/* Jorge Boncompte - DTI2 <jorge@dti2.net> */
		.name           = "ProVideo PV143",
		.video_inputs   = 4,
		/* .audio_inputs= 0, */
		.svhs           = NO_SVHS,
		.gpiomask       = 0,
		.muxsel         = MUXSEL(2, 3, 1, 0),
		.gpiomux        = { 0 },
		.needs_tvaudio  = 0,
		.no_msp34xx     = 1,
		.pll            = PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_VD009X1_VD011_MINIDIN] = {
		/* M.Klahr@phytec.de */
		.name           = "PHYTEC VD-009-X1 VD-011 MiniDIN (bt878)",
		.video_inputs   = 4,
		/* .audio_inputs= 0, */
		.svhs           = 3,
		.gpiomask       = 0x00,
		.muxsel         = MUXSEL(2, 3, 1, 0),
		.gpiomux        = { 0, 0, 0, 0 }, /* card has no audio */
		.needs_tvaudio  = 0,
		.pll            = PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_VD009X1_VD011_COMBI] = {
		.name           = "PHYTEC VD-009-X1 VD-011 Combi (bt878)",
		.video_inputs   = 4,
		/* .audio_inputs= 0, */
		.svhs           = 3,
		.gpiomask       = 0x00,
		.muxsel         = MUXSEL(2, 3, 1, 1),
		.gpiomux        = { 0, 0, 0, 0 }, /* card has no audio */
		.needs_tvaudio  = 0,
		.pll            = PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},

		/* ---- card 0x6c ---------------------------------- */
	[BTTV_BOARD_VD009_MINIDIN] = {
		.name           = "PHYTEC VD-009 MiniDIN (bt878)",
		.video_inputs   = 10,
		/* .audio_inputs= 0, */
		.svhs           = 9,
		.gpiomask       = 0x00,
		.gpiomask2      = 0x03, /* used for external vodeo mux */
		.muxsel         = MUXSEL(2, 2, 2, 2, 3, 3, 3, 3, 1, 0),
		.muxsel_hook	= phytec_muxsel,
		.gpiomux        = { 0, 0, 0, 0 }, /* card has no audio */
		.needs_tvaudio  = 1,
		.pll            = PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_VD009_COMBI] = {
		.name           = "PHYTEC VD-009 Combi (bt878)",
		.video_inputs   = 10,
		/* .audio_inputs= 0, */
		.svhs           = 9,
		.gpiomask       = 0x00,
		.gpiomask2      = 0x03, /* used for external vodeo mux */
		.muxsel         = MUXSEL(2, 2, 2, 2, 3, 3, 3, 3, 1, 1),
		.muxsel_hook	= phytec_muxsel,
		.gpiomux        = { 0, 0, 0, 0 }, /* card has no audio */
		.needs_tvaudio  = 1,
		.pll            = PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_IVC100] = {
		.name           = "IVC-100",
		.video_inputs   = 4,
		/* .audio_inputs= 0, */
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.svhs           = NO_SVHS,
		.gpiomask       = 0xdf,
		.muxsel         = MUXSEL(2, 3, 1, 0),
		.pll            = PLL_28,
	},
	[BTTV_BOARD_IVC120] = {
		/* IVC-120G - Alan Garfield <alan@fromorbit.com> */
		.name           = "IVC-120G",
		.video_inputs   = 16,
		/* .audio_inputs= 0, */
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.svhs           = NO_SVHS,   /* card has no svhs */
		.needs_tvaudio  = 0,
		.no_msp34xx     = 1,
		.no_tda9875     = 1,
		.no_tda7432     = 1,
		.gpiomask       = 0x00,
		.muxsel         = MUXSEL(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
		.muxsel_hook    = ivc120_muxsel,
		.pll            = PLL_28,
	},

		/* ---- card 0x70 ---------------------------------- */
	[BTTV_BOARD_PC_HDTV] = {
		.name           = "pcHDTV HD-2000 TV",
		.video_inputs   = 4,
		/* .audio_inputs= 1, */
		.svhs           = 2,
		.muxsel         = MUXSEL(2, 3, 1, 0),
		.tuner_type     = TUNER_PHILIPS_FCV1236D,
		.tuner_addr	= ADDR_UNSET,
		.has_dvb        = 1,
	},
	[BTTV_BOARD_TWINHAN_DST] = {
		.name           = "Twinhan DST + clones",
		.no_msp34xx     = 1,
		.no_tda9875     = 1,
		.no_tda7432     = 1,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.no_video       = 1,
		.has_dvb        = 1,
	},
	[BTTV_BOARD_WINFASTVC100] = {
		.name           = "Winfast VC100",
		.video_inputs   = 3,
		/* .audio_inputs= 0, */
		.svhs           = 1,
		/* Vid In, SVid In, Vid over SVid in connector */
		.muxsel		= MUXSEL(3, 1, 1, 3),
		.no_msp34xx     = 1,
		.no_tda9875     = 1,
		.no_tda7432     = 1,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.pll            = PLL_28,
	},
	[BTTV_BOARD_TEV560] = {
		.name           = "Teppro TEV-560/InterVision IV-560",
		.video_inputs   = 3,
		/* .audio_inputs= 1, */
		.svhs           = 2,
		.gpiomask       = 3,
		.muxsel         = MUXSEL(2, 3, 1, 1),
		.gpiomux        = { 1, 1, 1, 1 },
		.needs_tvaudio  = 1,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.tuner_addr	= ADDR_UNSET,
		.pll            = PLL_35,
	},

		/* ---- card 0x74 ---------------------------------- */
	[BTTV_BOARD_SIMUS_GVC1100] = {
		.name           = "SIMUS GVC1100",
		.video_inputs   = 4,
		/* .audio_inputs= 0, */
		.svhs           = NO_SVHS,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.pll            = PLL_28,
		.muxsel         = MUXSEL(2, 2, 2, 2),
		.gpiomask       = 0x3F,
		.muxsel_hook    = gvc1100_muxsel,
	},
	[BTTV_BOARD_NGSTV_PLUS] = {
		/* Carlos Silva r3pek@r3pek.homelinux.org || card 0x75 */
		.name           = "NGS NGSTV+",
		.video_inputs   = 3,
		.svhs           = 2,
		.gpiomask       = 0x008007,
		.muxsel         = MUXSEL(2, 3, 0, 0),
		.gpiomux        = { 0, 0, 0, 0 },
		.gpiomute 	= 0x000003,
		.pll            = PLL_28,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.tuner_addr	= ADDR_UNSET,
		.has_remote     = 1,
	},
	[BTTV_BOARD_LMLBT4] = {
		/* http://linuxmedialabs.com */
		.name           = "LMLBT4",
		.video_inputs   = 4, /* IN1,IN2,IN3,IN4 */
		/* .audio_inputs= 0, */
		.svhs           = NO_SVHS,
		.muxsel         = MUXSEL(2, 3, 1, 0),
		.no_msp34xx     = 1,
		.no_tda9875     = 1,
		.no_tda7432     = 1,
		.needs_tvaudio  = 0,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_TEKRAM_M205] = {
		/* Helmroos Harri <harri.helmroos@pp.inet.fi> */
		.name           = "Tekram M205 PRO",
		.video_inputs   = 3,
		/* .audio_inputs= 1, */
		.tuner_type     = TUNER_PHILIPS_PAL,
		.tuner_addr	= ADDR_UNSET,
		.svhs           = 2,
		.needs_tvaudio  = 0,
		.gpiomask       = 0x68,
		.muxsel         = MUXSEL(2, 3, 1),
		.gpiomux        = { 0x68, 0x68, 0x61, 0x61 },
		.pll            = PLL_28,
	},

		/* ---- card 0x78 ---------------------------------- */
	[BTTV_BOARD_CONTVFMI] = {
		/* Javier Cendan Ares <jcendan@lycos.es> */
		/* bt878 TV + FM without subsystem ID */
		.name           = "Conceptronic CONTVFMi",
		.video_inputs   = 3,
		/* .audio_inputs= 1, */
		.svhs           = 2,
		.gpiomask       = 0x008007,
		.muxsel         = MUXSEL(2, 3, 1, 1),
		.gpiomux        = { 0, 1, 2, 2 },
		.gpiomute 	= 3,
		.needs_tvaudio  = 0,
		.pll            = PLL_28,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.tuner_addr	= ADDR_UNSET,
		.has_remote     = 1,
		.has_radio      = 1,
	},
	[BTTV_BOARD_PICOLO_TETRA_CHIP] = {
		/*Eric DEBIEF <debief@telemsa.com>*/
		/*EURESYS Picolo Tetra : 4 Conexant Fusion 878A, no audio, video input set with analog multiplexers GPIO controled*/
		/* adds picolo_tetra_muxsel(), picolo_tetra_init(), the folowing declaration strucure, and #define BTTV_BOARD_PICOLO_TETRA_CHIP*/
		/*0x79 in bttv.h*/
		.name           = "Euresys Picolo Tetra",
		.video_inputs   = 4,
		/* .audio_inputs= 0, */
		.svhs           = NO_SVHS,
		.gpiomask       = 0,
		.gpiomask2      = 0x3C<<16,/*Set the GPIO[18]->GPIO[21] as output pin.==> drive the video inputs through analog multiplexers*/
		.no_msp34xx     = 1,
		.no_tda9875     = 1,
		.no_tda7432     = 1,
		/*878A input is always MUX0, see above.*/
		.muxsel         = MUXSEL(2, 2, 2, 2),
		.gpiomux        = { 0, 0, 0, 0 }, /* card has no audio */
		.pll            = PLL_28,
		.needs_tvaudio  = 0,
		.muxsel_hook    = picolo_tetra_muxsel,/*Required as it doesn't follow the classic input selection policy*/
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_SPIRIT_TV] = {
		/* Spirit TV Tuner from http://spiritmodems.com.au */
		/* Stafford Goodsell <surge@goliath.homeunix.org> */
		.name           = "Spirit TV Tuner",
		.video_inputs   = 3,
		/* .audio_inputs= 1, */
		.svhs           = 2,
		.gpiomask       = 0x0000000f,
		.muxsel         = MUXSEL(2, 1, 1),
		.gpiomux        = { 0x02, 0x00, 0x00, 0x00 },
		.tuner_type     = TUNER_TEMIC_PAL,
		.tuner_addr	= ADDR_UNSET,
		.no_msp34xx     = 1,
		.no_tda9875     = 1,
	},
	[BTTV_BOARD_AVDVBT_771] = {
		/* Wolfram Joost <wojo@frokaschwei.de> */
		.name           = "AVerMedia AVerTV DVB-T 771",
		.video_inputs   = 2,
		.svhs           = 1,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.muxsel         = MUXSEL(3, 3),
		.no_msp34xx     = 1,
		.no_tda9875     = 1,
		.no_tda7432     = 1,
		.pll            = PLL_28,
		.has_dvb        = 1,
		.no_gpioirq     = 1,
		.has_remote     = 1,
	},
		/* ---- card 0x7c ---------------------------------- */
	[BTTV_BOARD_AVDVBT_761] = {
		/* Matt Jesson <dvb@jesson.eclipse.co.uk> */
		/* Based on the Nebula card data - added remote and new card number - BTTV_BOARD_AVDVBT_761, see also ir-kbd-gpio.c */
		.name           = "AverMedia AverTV DVB-T 761",
		.video_inputs   = 2,
		.svhs           = 1,
		.muxsel         = MUXSEL(3, 1, 2, 0), /* Comp0, S-Video, ?, ? */
		.no_msp34xx     = 1,
		.no_tda9875     = 1,
		.no_tda7432     = 1,
		.pll            = PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.has_dvb        = 1,
		.no_gpioirq     = 1,
		.has_remote     = 1,
	},
	[BTTV_BOARD_MATRIX_VISIONSQ] = {
		/* andre.schwarz@matrix-vision.de */
		.name		= "MATRIX Vision Sigma-SQ",
		.video_inputs	= 16,
		/* .audio_inputs= 0, */
		.svhs		= NO_SVHS,
		.gpiomask	= 0x0,
		.muxsel		= MUXSEL(2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3),
		.muxsel_hook	= sigmaSQ_muxsel,
		.gpiomux	= { 0 },
		.no_msp34xx	= 1,
		.pll		= PLL_28,
		.tuner_type	= TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_MATRIX_VISIONSLC] = {
		/* andre.schwarz@matrix-vision.de */
		.name		= "MATRIX Vision Sigma-SLC",
		.video_inputs	= 4,
		/* .audio_inputs= 0, */
		.svhs		= NO_SVHS,
		.gpiomask	= 0x0,
		.muxsel		= MUXSEL(2, 2, 2, 2),
		.muxsel_hook	= sigmaSLC_muxsel,
		.gpiomux	= { 0 },
		.no_msp34xx	= 1,
		.pll		= PLL_28,
		.tuner_type	= TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
		/* BTTV_BOARD_APAC_VIEWCOMP */
	[BTTV_BOARD_APAC_VIEWCOMP] = {
		/* Attila Kondoros <attila.kondoros@chello.hu> */
		/* bt878 TV + FM 0x00000000 subsystem ID */
		.name           = "APAC Viewcomp 878(AMAX)",
		.video_inputs   = 2,
		/* .audio_inputs= 1, */
		.svhs           = NO_SVHS,
		.gpiomask       = 0xFF,
		.muxsel         = MUXSEL(2, 3, 1, 1),
		.gpiomux        = { 2, 0, 0, 0 },
		.gpiomute 	= 10,
		.needs_tvaudio  = 0,
		.pll            = PLL_28,
		.tuner_type     = TUNER_PHILIPS_PAL,
		.tuner_addr	= ADDR_UNSET,
		.has_remote     = 1,   /* miniremote works, see ir-kbd-gpio.c */
		.has_radio      = 1,   /* not every card has radio */
	},

		/* ---- card 0x80 ---------------------------------- */
	[BTTV_BOARD_DVICO_DVBT_LITE] = {
		/* Chris Pascoe <c.pascoe@itee.uq.edu.au> */
		.name           = "DViCO FusionHDTV DVB-T Lite",
		.no_msp34xx     = 1,
		.no_tda9875     = 1,
		.no_tda7432     = 1,
		.pll            = PLL_28,
		.no_video       = 1,
		.has_dvb        = 1,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_VGEAR_MYVCD] = {
		/* Steven <photon38@pchome.com.tw> */
		.name           = "V-Gear MyVCD",
		.video_inputs   = 3,
		/* .audio_inputs= 1, */
		.svhs           = 2,
		.gpiomask       = 0x3f,
		.muxsel         = MUXSEL(2, 3, 1, 0),
		.gpiomux        = {0x31, 0x31, 0x31, 0x31 },
		.gpiomute 	= 0x31,
		.no_msp34xx     = 1,
		.pll            = PLL_28,
		.tuner_type     = TUNER_PHILIPS_NTSC_M,
		.tuner_addr	= ADDR_UNSET,
		.has_radio      = 0,
	},
	[BTTV_BOARD_SUPER_TV] = {
		/* Rick C <cryptdragoon@gmail.com> */
		.name           = "Super TV Tuner",
		.video_inputs   = 4,
		/* .audio_inputs= 1, */
		.svhs           = 2,
		.muxsel         = MUXSEL(2, 3, 1, 0),
		.tuner_type     = TUNER_PHILIPS_NTSC,
		.tuner_addr	= ADDR_UNSET,
		.gpiomask       = 0x008007,
		.gpiomux        = { 0, 0x000001,0,0 },
		.needs_tvaudio  = 1,
		.has_radio      = 1,
	},
	[BTTV_BOARD_TIBET_CS16] = {
		/* Chris Fanning <video4linux@haydon.net> */
		.name           = "Tibet Systems 'Progress DVR' CS16",
		.video_inputs   = 16,
		/* .audio_inputs= 0, */
		.svhs           = NO_SVHS,
		.muxsel         = MUXSEL(2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2),
		.pll		= PLL_28,
		.no_msp34xx     = 1,
		.no_tda9875     = 1,
		.no_tda7432	= 1,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.muxsel_hook    = tibetCS16_muxsel,
	},
	[BTTV_BOARD_KODICOM_4400R] = {
		/* Bill Brack <wbrack@mmm.com.hk> */
		/*
		* Note that, because of the card's wiring, the "master"
		* BT878A chip (i.e. the one which controls the analog switch
		* and must use this card type) is the 2nd one detected.  The
		* other 3 chips should use card type 0x85, whose description
		* follows this one.  There is a EEPROM on the card (which is
		* connected to the I2C of one of those other chips), but is
		* not currently handled.  There is also a facility for a
		* "monitor", which is also not currently implemented.
		*/
		.name           = "Kodicom 4400R (master)",
		.video_inputs	= 16,
		/* .audio_inputs= 0, */
		.tuner_type	= TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.svhs		= NO_SVHS,
		/* GPIO bits 0-9 used for analog switch:
		*   00 - 03:	camera selector
		*   04 - 06:	channel (controller) selector
		*   07:	data (1->on, 0->off)
		*   08:	strobe
		*   09:	reset
		* bit 16 is input from sync separator for the channel
		*/
		.gpiomask	= 0x0003ff,
		.no_gpioirq     = 1,
		.muxsel		= MUXSEL(3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3),
		.pll		= PLL_28,
		.no_msp34xx	= 1,
		.no_tda7432	= 1,
		.no_tda9875	= 1,
		.muxsel_hook	= kodicom4400r_muxsel,
	},
	[BTTV_BOARD_KODICOM_4400R_SL] = {
		/* Bill Brack <wbrack@mmm.com.hk> */
		/* Note that, for reasons unknown, the "master" BT878A chip (i.e. the
		* one which controls the analog switch, and must use the card type)
		* is the 2nd one detected.  The other 3 chips should use this card
		* type
		*/
		.name		= "Kodicom 4400R (slave)",
		.video_inputs	= 16,
		/* .audio_inputs= 0, */
		.tuner_type	= TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.svhs		= NO_SVHS,
		.gpiomask	= 0x010000,
		.no_gpioirq     = 1,
		.muxsel		= MUXSEL(3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3),
		.pll		= PLL_28,
		.no_msp34xx	= 1,
		.no_tda7432	= 1,
		.no_tda9875	= 1,
		.muxsel_hook	= kodicom4400r_muxsel,
	},
		/* ---- card 0x86---------------------------------- */
	[BTTV_BOARD_ADLINK_RTV24] = {
		/* Michael Henson <mhenson@clarityvi.com> */
		/* Adlink RTV24 with special unlock codes */
		.name           = "Adlink RTV24",
		.video_inputs   = 4,
		/* .audio_inputs= 1, */
		.svhs           = 2,
		.muxsel         = MUXSEL(2, 3, 1, 0),
		.tuner_type     = UNSET,
		.tuner_addr	= ADDR_UNSET,
		.pll            = PLL_28,
	},
		/* ---- card 0x87---------------------------------- */
	[BTTV_BOARD_DVICO_FUSIONHDTV_5_LITE] = {
		/* Michael Krufky <mkrufky@m1k.net> */
		.name           = "DViCO FusionHDTV 5 Lite",
		.tuner_type     = TUNER_LG_TDVS_H06XF, /* TDVS-H064F */
		.tuner_addr	= ADDR_UNSET,
		.video_inputs   = 3,
		/* .audio_inputs= 1, */
		.svhs           = 2,
		.muxsel		= MUXSEL(2, 3, 1),
		.gpiomask       = 0x00e00007,
		.gpiomux        = { 0x00400005, 0, 0x00000001, 0 },
		.gpiomute 	= 0x00c00007,
		.no_msp34xx     = 1,
		.no_tda9875     = 1,
		.no_tda7432     = 1,
		.has_dvb        = 1,
	},
		/* ---- card 0x88---------------------------------- */
	[BTTV_BOARD_ACORP_Y878F] = {
		/* Mauro Carvalho Chehab <mchehab@infradead.org> */
		.name		= "Acorp Y878F",
		.video_inputs	= 3,
		/* .audio_inputs= 1, */
		.svhs		= 2,
		.gpiomask	= 0x01fe00,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux        = { 0x001e00, 0, 0x018000, 0x014000 },
		.gpiomute 	= 0x002000,
		.needs_tvaudio	= 1,
		.pll		= PLL_28,
		.tuner_type	= TUNER_YMEC_TVF66T5_B_DFF,
		.tuner_addr	= 0xc1 >>1,
		.has_radio	= 1,
	},
		/* ---- card 0x89 ---------------------------------- */
	[BTTV_BOARD_CONCEPTRONIC_CTVFMI2] = {
		.name           = "Conceptronic CTVFMi v2",
		.video_inputs   = 3,
		/* .audio_inputs= 1, */
		.svhs           = 2,
		.gpiomask       = 0x001c0007,
		.muxsel         = MUXSEL(2, 3, 1, 1),
		.gpiomux        = { 0, 1, 2, 2 },
		.gpiomute 	= 3,
		.needs_tvaudio  = 0,
		.pll            = PLL_28,
		.tuner_type     = TUNER_TENA_9533_DI,
		.tuner_addr	= ADDR_UNSET,
		.has_remote     = 1,
		.has_radio      = 1,
	},
		/* ---- card 0x8a ---------------------------------- */
	[BTTV_BOARD_PV_BT878P_2E] = {
		.name		= "Prolink Pixelview PV-BT878P+ (Rev.2E)",
		.video_inputs	= 5,
		/* .audio_inputs= 1, */
		.svhs		= 3,
		.has_dig_in	= 1,
		.gpiomask	= 0x01fe00,
		.muxsel		= MUXSEL(2, 3, 1, 1, 0), /* in 4 is digital */
		/* .digital_mode= DIGITAL_MODE_CAMERA, */
		.gpiomux	= { 0x00400, 0x10400, 0x04400, 0x80000 },
		.gpiomute	= 0x12400,
		.no_msp34xx	= 1,
		.pll		= PLL_28,
		.tuner_type	= TUNER_LG_PAL_FM,
		.tuner_addr	= ADDR_UNSET,
		.has_remote	= 1,
	},
		/* ---- card 0x8b ---------------------------------- */
	[BTTV_BOARD_PV_M4900] = {
		/* Sérgio Fortier <sergiofortier@yahoo.com.br> */
		.name           = "Prolink PixelView PlayTV MPEG2 PV-M4900",
		.video_inputs   = 3,
		/* .audio_inputs= 1, */
		.svhs           = 2,
		.gpiomask       = 0x3f,
		.muxsel         = MUXSEL(2, 3, 1, 1),
		.gpiomux        = { 0x21, 0x20, 0x24, 0x2c },
		.gpiomute 	= 0x29,
		.no_msp34xx     = 1,
		.pll            = PLL_28,
		.tuner_type     = TUNER_YMEC_TVF_5533MF,
		.tuner_addr     = ADDR_UNSET,
		.has_radio      = 1,
		.has_remote     = 1,
	},
		/* ---- card 0x8c ---------------------------------- */
	/* Has four Bt878 chips behind a PCI bridge, each chip has:
	     one external BNC composite input (mux 2)
	     three internal composite inputs (unknown muxes)
	     an 18-bit stereo A/D (CS5331A), which has:
	       one external stereo unblanced (RCA) audio connection
	       one (or 3?) internal stereo balanced (XLR) audio connection
	       input is selected via gpio to a 14052B mux
		 (mask=0x300, unbal=0x000, bal=0x100, ??=0x200,0x300)
	       gain is controlled via an X9221A chip on the I2C bus @0x28
	       sample rate is controlled via gpio to an MK1413S
		 (mask=0x3, 32kHz=0x0, 44.1kHz=0x1, 48kHz=0x2, ??=0x3)
	     There is neither a tuner nor an svideo input. */
	[BTTV_BOARD_OSPREY440]  = {
		.name           = "Osprey 440",
		.video_inputs   = 4,
		/* .audio_inputs= 2, */
		.svhs           = NO_SVHS,
		.muxsel         = MUXSEL(2, 3, 0, 1), /* 3,0,1 are guesses */
		.gpiomask	= 0x303,
		.gpiomute	= 0x000, /* int + 32kHz */
		.gpiomux	= { 0, 0, 0x000, 0x100},
		.pll            = PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr     = ADDR_UNSET,
		.no_msp34xx     = 1,
		.no_tda9875     = 1,
		.no_tda7432     = 1,
	},
		/* ---- card 0x8d ---------------------------------- */
	[BTTV_BOARD_ASOUND_SKYEYE] = {
		.name		= "Asound Skyeye PCTV",
		.video_inputs	= 3,
		/* .audio_inputs= 1, */
		.svhs		= 2,
		.gpiomask	= 15,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 2, 0, 0, 0 },
		.gpiomute 	= 1,
		.needs_tvaudio	= 1,
		.pll		= PLL_28,
		.tuner_type	= TUNER_PHILIPS_NTSC,
		.tuner_addr	= ADDR_UNSET,
	},
		/* ---- card 0x8e ---------------------------------- */
	[BTTV_BOARD_SABRENT_TVFM] = {
		.name		= "Sabrent TV-FM (bttv version)",
		.video_inputs	= 3,
		/* .audio_inputs= 1, */
		.svhs		= 2,
		.gpiomask	= 0x108007,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 100000, 100002, 100002, 100000 },
		.no_msp34xx	= 1,
		.no_tda9875     = 1,
		.no_tda7432     = 1,
		.pll		= PLL_28,
		.tuner_type	= TUNER_TNF_5335MF,
		.tuner_addr	= ADDR_UNSET,
		.has_radio      = 1,
	},
	/* ---- card 0x8f ---------------------------------- */
	[BTTV_BOARD_HAUPPAUGE_IMPACTVCB] = {
		.name		= "Hauppauge ImpactVCB (bt878)",
		.video_inputs	= 4,
		/* .audio_inputs= 0, */
		.svhs		= NO_SVHS,
		.gpiomask	= 0x0f, /* old: 7 */
		.muxsel		= MUXSEL(0, 1, 3, 2), /* Composite 0-3 */
		.no_msp34xx	= 1,
		.no_tda9875     = 1,
		.no_tda7432     = 1,
		.tuner_type	= TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_MACHTV_MAGICTV] = {
		/* Julian Calaby <julian.calaby@gmail.com>
		 * Slightly different from original MachTV definition (0x60)

		 * FIXME: RegSpy says gpiomask should be "0x001c800f", but it
		 * stuffs up remote chip. Bug is a pin on the jaecs is not set
		 * properly (methinks) causing no keyup bits being set */

		.name           = "MagicTV", /* rebranded MachTV */
		.video_inputs   = 3,
		/* .audio_inputs= 1, */
		.svhs           = 2,
		.gpiomask       = 7,
		.muxsel         = MUXSEL(2, 3, 1, 1),
		.gpiomux        = { 0, 1, 2, 3 },
		.gpiomute 	= 4,
		.tuner_type     = TUNER_TEMIC_4009FR5_PAL,
		.tuner_addr     = ADDR_UNSET,
		.pll            = PLL_28,
		.has_radio      = 1,
		.has_remote     = 1,
	},
	[BTTV_BOARD_SSAI_SECURITY] = {
		.name		= "SSAI Security Video Interface",
		.video_inputs	= 4,
		/* .audio_inputs= 0, */
		.svhs		= NO_SVHS,
		.muxsel		= MUXSEL(0, 1, 2, 3),
		.tuner_type	= TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_SSAI_ULTRASOUND] = {
		.name		= "SSAI Ultrasound Video Interface",
		.video_inputs	= 2,
		/* .audio_inputs= 0, */
		.svhs		= 1,
		.muxsel		= MUXSEL(2, 0, 1, 3),
		.tuner_type	= TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
	/* ---- card 0x94---------------------------------- */
	[BTTV_BOARD_DVICO_FUSIONHDTV_2] = {
		.name           = "DViCO FusionHDTV 2",
		.tuner_type     = TUNER_PHILIPS_FCV1236D,
		.tuner_addr	= ADDR_UNSET,
		.video_inputs   = 3,
		/* .audio_inputs= 1, */
		.svhs           = 2,
		.muxsel		= MUXSEL(2, 3, 1),
		.gpiomask       = 0x00e00007,
		.gpiomux        = { 0x00400005, 0, 0x00000001, 0 },
		.gpiomute 	= 0x00c00007,
		.no_msp34xx     = 1,
		.no_tda9875     = 1,
		.no_tda7432     = 1,
	},
	/* ---- card 0x95---------------------------------- */
	[BTTV_BOARD_TYPHOON_TVTUNERPCI] = {
		.name           = "Typhoon TV-Tuner PCI (50684)",
		.video_inputs   = 3,
		/* .audio_inputs= 1, */
		.svhs           = 2,
		.gpiomask       = 0x3014f,
		.muxsel         = MUXSEL(2, 3, 1, 1),
		.gpiomux        = { 0x20001,0x10001, 0, 0 },
		.gpiomute       = 10,
		.needs_tvaudio  = 1,
		.pll            = PLL_28,
		.tuner_type     = TUNER_PHILIPS_PAL_I,
		.tuner_addr     = ADDR_UNSET,
	},
	[BTTV_BOARD_GEOVISION_GV600] = {
		/* emhn@usb.ve */
		.name		= "Geovision GV-600",
		.video_inputs	= 16,
		/* .audio_inputs= 0, */
		.svhs		= NO_SVHS,
		.gpiomask	= 0x0,
		.muxsel		= MUXSEL(2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2),
		.muxsel_hook	= geovision_muxsel,
		.gpiomux	= { 0 },
		.no_msp34xx	= 1,
		.pll		= PLL_28,
		.tuner_type	= TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_KOZUMI_KTV_01C] = {
		/* Mauro Lacy <mauro@lacy.com.ar>
		 * Based on MagicTV and Conceptronic CONTVFMi */

		.name           = "Kozumi KTV-01C",
		.video_inputs   = 3,
		/* .audio_inputs= 1, */
		.svhs           = 2,
		.gpiomask       = 0x008007,
		.muxsel         = MUXSEL(2, 3, 1, 1),
		.gpiomux        = { 0, 1, 2, 2 }, /* CONTVFMi */
		.gpiomute 	= 3, /* CONTVFMi */
		.needs_tvaudio  = 0,
		.tuner_type     = TUNER_PHILIPS_FM1216ME_MK3, /* TCL MK3 */
		.tuner_addr     = ADDR_UNSET,
		.pll            = PLL_28,
		.has_radio      = 1,
		.has_remote     = 1,
	},
	[BTTV_BOARD_ENLTV_FM_2] = {
		/* Encore TV Tuner Pro ENL TV-FM-2
		   Mauro Carvalho Chehab <mchehab@infradead.org */
		.name           = "Encore ENL TV-FM-2",
		.video_inputs   = 3,
		/* .audio_inputs= 1, */
		.svhs           = 2,
		/* bit 6          -> IR disabled
		   bit 18/17 = 00 -> mute
			       01 -> enable external audio input
			       10 -> internal audio input (mono?)
			       11 -> internal audio input
		 */
		.gpiomask       = 0x060040,
		.muxsel         = MUXSEL(2, 3, 3),
		.gpiomux        = { 0x60000, 0x60000, 0x20000, 0x20000 },
		.gpiomute 	= 0,
		.tuner_type	= TUNER_TCL_MF02GIP_5N,
		.tuner_addr     = ADDR_UNSET,
		.pll            = PLL_28,
		.has_radio      = 1,
		.has_remote     = 1,
	},
		[BTTV_BOARD_VD012] = {
		/* D.Heer@Phytec.de */
		.name           = "PHYTEC VD-012 (bt878)",
		.video_inputs   = 4,
		/* .audio_inputs= 0, */
		.svhs           = NO_SVHS,
		.gpiomask       = 0x00,
		.muxsel         = MUXSEL(0, 2, 3, 1),
		.gpiomux        = { 0, 0, 0, 0 }, /* card has no audio */
		.needs_tvaudio  = 0,
		.pll            = PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
		[BTTV_BOARD_VD012_X1] = {
		/* D.Heer@Phytec.de */
		.name           = "PHYTEC VD-012-X1 (bt878)",
		.video_inputs   = 4,
		/* .audio_inputs= 0, */
		.svhs           = 3,
		.gpiomask       = 0x00,
		.muxsel         = MUXSEL(2, 3, 1),
		.gpiomux        = { 0, 0, 0, 0 }, /* card has no audio */
		.needs_tvaudio  = 0,
		.pll            = PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
		[BTTV_BOARD_VD012_X2] = {
		/* D.Heer@Phytec.de */
		.name           = "PHYTEC VD-012-X2 (bt878)",
		.video_inputs   = 4,
		/* .audio_inputs= 0, */
		.svhs           = 3,
		.gpiomask       = 0x00,
		.muxsel         = MUXSEL(3, 2, 1),
		.gpiomux        = { 0, 0, 0, 0 }, /* card has no audio */
		.needs_tvaudio  = 0,
		.pll            = PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
		[BTTV_BOARD_GEOVISION_GV800S] = {
		/* Bruno Christo <bchristo@inf.ufsm.br>
		 *
		 * GeoVision GV-800(S) has 4 Conexant Fusion 878A:
		 * 	1 audio input  per BT878A = 4 audio inputs
		 * 	4 video inputs per BT878A = 16 video inputs
		 * This is the first BT878A chip of the GV-800(S). It's the
		 * "master" chip and it controls the video inputs through an
		 * analog multiplexer (a CD22M3494) via some GPIO pins. The
		 * slaves should use card type 0x9e (following this one).
		 * There is a EEPROM on the card which is currently not handled.
		 * The audio input is not working yet.
		 */
		.name           = "Geovision GV-800(S) (master)",
		.video_inputs   = 4,
		/* .audio_inputs= 1, */
		.tuner_type	= TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.svhs           = NO_SVHS,
		.gpiomask	= 0xf107f,
		.no_gpioirq     = 1,
		.muxsel		= MUXSEL(2, 2, 2, 2),
		.pll		= PLL_28,
		.no_msp34xx	= 1,
		.no_tda7432	= 1,
		.no_tda9875	= 1,
		.muxsel_hook    = gv800s_muxsel,
	},
		[BTTV_BOARD_GEOVISION_GV800S_SL] = {
		/* Bruno Christo <bchristo@inf.ufsm.br>
		 *
		 * GeoVision GV-800(S) has 4 Conexant Fusion 878A:
		 * 	1 audio input  per BT878A = 4 audio inputs
		 * 	4 video inputs per BT878A = 16 video inputs
		 * The 3 other BT878A chips are "slave" chips of the GV-800(S)
		 * and should use this card type.
		 * The audio input is not working yet.
		 */
		.name           = "Geovision GV-800(S) (slave)",
		.video_inputs   = 4,
		/* .audio_inputs= 1, */
		.tuner_type	= TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
		.svhs           = NO_SVHS,
		.gpiomask	= 0x00,
		.no_gpioirq     = 1,
		.muxsel		= MUXSEL(2, 2, 2, 2),
		.pll		= PLL_28,
		.no_msp34xx	= 1,
		.no_tda7432	= 1,
		.no_tda9875	= 1,
		.muxsel_hook    = gv800s_muxsel,
	},
	[BTTV_BOARD_PV183] = {
		.name           = "ProVideo PV183", /* 0x9f */
		.video_inputs   = 2,
		/* .audio_inputs= 0, */
		.svhs           = NO_SVHS,
		.gpiomask       = 0,
		.muxsel         = MUXSEL(2, 3),
		.gpiomux        = { 0 },
		.needs_tvaudio  = 0,
		.no_msp34xx     = 1,
		.pll            = PLL_28,
		.tuner_type     = TUNER_ABSENT,
		.tuner_addr	= ADDR_UNSET,
	},
};

static const unsigned int bttv_num_tvcards = ARRAY_SIZE(bttv_tvcards);

/* ----------------------------------------------------------------------- */

static unsigned char eeprom_data[256];

/*
 * identify card
 */
void __devinit bttv_idcard(struct bttv *btv)
{
	unsigned int gpiobits;
	int i,type;
	unsigned short tmp;

	/* read PCI subsystem ID */
	pci_read_config_word(btv->c.pci, PCI_SUBSYSTEM_ID, &tmp);
	btv->cardid = tmp << 16;
	pci_read_config_word(btv->c.pci, PCI_SUBSYSTEM_VENDOR_ID, &tmp);
	btv->cardid |= tmp;

	if (0 != btv->cardid && 0xffffffff != btv->cardid) {
		/* look for the card */
		for (type = -1, i = 0; cards[i].id != 0; i++)
			if (cards[i].id  == btv->cardid)
				type = i;

		if (type != -1) {
			/* found it */
			printk(KERN_INFO "bttv%d: detected: %s [card=%d], "
			       "PCI subsystem ID is %04x:%04x\n",
			       btv->c.nr,cards[type].name,cards[type].cardnr,
			       btv->cardid & 0xffff,
			       (btv->cardid >> 16) & 0xffff);
			btv->c.type = cards[type].cardnr;
		} else {
			/* 404 */
			printk(KERN_INFO "bttv%d: subsystem: %04x:%04x (UNKNOWN)\n",
			       btv->c.nr, btv->cardid & 0xffff,
			       (btv->cardid >> 16) & 0xffff);
			printk(KERN_DEBUG "please mail id, board name and "
			       "the correct card= insmod option to linux-media@vger.kernel.org\n");
		}
	}

	/* let the user override the autodetected type */
	if (card[btv->c.nr] < bttv_num_tvcards)
		btv->c.type=card[btv->c.nr];

	/* print which card config we are using */
	printk(KERN_INFO "bttv%d: using: %s [card=%d,%s]\n",btv->c.nr,
	       bttv_tvcards[btv->c.type].name, btv->c.type,
	       card[btv->c.nr] < bttv_num_tvcards
	       ? "insmod option" : "autodetected");

	/* overwrite gpio stuff ?? */
	if (UNSET == audioall && UNSET == audiomux[0])
		return;

	if (UNSET != audiomux[0]) {
		gpiobits = 0;
		for (i = 0; i < ARRAY_SIZE(bttv_tvcards->gpiomux); i++) {
			bttv_tvcards[btv->c.type].gpiomux[i] = audiomux[i];
			gpiobits |= audiomux[i];
		}
	} else {
		gpiobits = audioall;
		for (i = 0; i < ARRAY_SIZE(bttv_tvcards->gpiomux); i++) {
			bttv_tvcards[btv->c.type].gpiomux[i] = audioall;
		}
	}
	bttv_tvcards[btv->c.type].gpiomask = (UNSET != gpiomask) ? gpiomask : gpiobits;
	printk(KERN_INFO "bttv%d: gpio config override: mask=0x%x, mux=",
	       btv->c.nr,bttv_tvcards[btv->c.type].gpiomask);
	for (i = 0; i < ARRAY_SIZE(bttv_tvcards->gpiomux); i++) {
		printk("%s0x%x", i ? "," : "", bttv_tvcards[btv->c.type].gpiomux[i]);
	}
	printk("\n");
}

/*
 * (most) board specific initialisations goes here
 */

/* Some Modular Technology cards have an eeprom, but no subsystem ID */
static void identify_by_eeprom(struct bttv *btv, unsigned char eeprom_data[256])
{
	int type = -1;

	if (0 == strncmp(eeprom_data,"GET MM20xPCTV",13))
		type = BTTV_BOARD_MODTEC_205;
	else if (0 == strncmp(eeprom_data+20,"Picolo",7))
		type = BTTV_BOARD_EURESYS_PICOLO;
	else if (eeprom_data[0] == 0x84 && eeprom_data[2]== 0)
		type = BTTV_BOARD_HAUPPAUGE; /* old bt848 */

	if (-1 != type) {
		btv->c.type = type;
		printk("bttv%d: detected by eeprom: %s [card=%d]\n",
		       btv->c.nr, bttv_tvcards[btv->c.type].name, btv->c.type);
	}
}

static void flyvideo_gpio(struct bttv *btv)
{
	int gpio, has_remote, has_radio, is_capture_only;
	int is_lr90, has_tda9820_tda9821;
	int tuner_type = UNSET, ttype;

	gpio_inout(0xffffff, 0);
	udelay(8);  /* without this we would see the 0x1800 mask */
	gpio = gpio_read();
	/* FIXME: must restore OUR_EN ??? */

	/* all cards provide GPIO info, some have an additional eeprom
	 * LR50: GPIO coding can be found lower right CP1 .. CP9
	 *       CP9=GPIO23 .. CP1=GPIO15; when OPEN, the corresponding GPIO reads 1.
	 *       GPIO14-12: n.c.
	 * LR90: GP9=GPIO23 .. GP1=GPIO15 (right above the bt878)

	 * lowest 3 bytes are remote control codes (no handshake needed)
	 * xxxFFF: No remote control chip soldered
	 * xxxF00(LR26/LR50), xxxFE0(LR90): Remote control chip (LVA001 or CF45) soldered
	 * Note: Some bits are Audio_Mask !
	 */
	ttype = (gpio & 0x0f0000) >> 16;
	switch (ttype) {
	case 0x0:
		tuner_type = 2;  /* NTSC, e.g. TPI8NSR11P */
		break;
	case 0x2:
		tuner_type = 39; /* LG NTSC (newer TAPC series) TAPC-H701P */
		break;
	case 0x4:
		tuner_type = 5;  /* Philips PAL TPI8PSB02P, TPI8PSB12P, TPI8PSB12D or FI1216, FM1216 */
		break;
	case 0x6:
		tuner_type = 37; /* LG PAL (newer TAPC series) TAPC-G702P */
		break;
	case 0xC:
		tuner_type = 3;  /* Philips SECAM(+PAL) FQ1216ME or FI1216MF */
		break;
	default:
		printk(KERN_INFO "bttv%d: FlyVideo_gpio: unknown tuner type.\n", btv->c.nr);
		break;
	}

	has_remote          =   gpio & 0x800000;
	has_radio	    =   gpio & 0x400000;
	/*   unknown                   0x200000;
	 *   unknown2                  0x100000; */
	is_capture_only     = !(gpio & 0x008000); /* GPIO15 */
	has_tda9820_tda9821 = !(gpio & 0x004000);
	is_lr90             = !(gpio & 0x002000); /* else LR26/LR50 (LR38/LR51 f. capture only) */
	/*
	 * gpio & 0x001000    output bit for audio routing */

	if (is_capture_only)
		tuner_type = TUNER_ABSENT; /* No tuner present */

	printk(KERN_INFO "bttv%d: FlyVideo Radio=%s RemoteControl=%s Tuner=%d gpio=0x%06x\n",
		btv->c.nr, has_radio ? "yes" : "no ",
		has_remote ? "yes" : "no ", tuner_type, gpio);
	printk(KERN_INFO "bttv%d: FlyVideo  LR90=%s tda9821/tda9820=%s capture_only=%s\n",
		btv->c.nr, is_lr90 ? "yes" : "no ",
		has_tda9820_tda9821 ? "yes" : "no ",
		is_capture_only ? "yes" : "no ");

	if (tuner_type != UNSET) /* only set if known tuner autodetected, else let insmod option through */
		btv->tuner_type = tuner_type;
	btv->has_radio = has_radio;

	/* LR90 Audio Routing is done by 2 hef4052, so Audio_Mask has 4 bits: 0x001c80
	 * LR26/LR50 only has 1 hef4052, Audio_Mask 0x000c00
	 * Audio options: from tuner, from tda9821/tda9821(mono,stereo,sap), from tda9874, ext., mute */
	if (has_tda9820_tda9821)
		btv->audio_mode_gpio = lt9415_audio;
	/* todo: if(has_tda9874) btv->audio_mode_gpio = fv2000s_audio; */
}

static int miro_tunermap[] = { 0,6,2,3,   4,5,6,0,  3,0,4,5,  5,2,16,1,
			       14,2,17,1, 4,1,4,3,  1,2,16,1, 4,4,4,4 };
static int miro_fmtuner[]  = { 0,0,0,0,   0,0,0,0,  0,0,0,0,  0,0,0,1,
			       1,1,1,1,   1,1,1,0,  0,0,0,0,  0,1,0,0 };

static void miro_pinnacle_gpio(struct bttv *btv)
{
	int id,msp,gpio;
	char *info;

	gpio_inout(0xffffff, 0);
	gpio = gpio_read();
	id   = ((gpio>>10) & 63) -1;
	msp  = bttv_I2CRead(btv, I2C_ADDR_MSP3400, "MSP34xx");
	if (id < 32) {
		btv->tuner_type = miro_tunermap[id];
		if (0 == (gpio & 0x20)) {
			btv->has_radio = 1;
			if (!miro_fmtuner[id]) {
				btv->has_matchbox = 1;
				btv->mbox_we    = (1<<6);
				btv->mbox_most  = (1<<7);
				btv->mbox_clk   = (1<<8);
				btv->mbox_data  = (1<<9);
				btv->mbox_mask  = (1<<6)|(1<<7)|(1<<8)|(1<<9);
			}
		} else {
			btv->has_radio = 0;
		}
		if (-1 != msp) {
			if (btv->c.type == BTTV_BOARD_MIRO)
				btv->c.type = BTTV_BOARD_MIROPRO;
			if (btv->c.type == BTTV_BOARD_PINNACLE)
				btv->c.type = BTTV_BOARD_PINNACLEPRO;
		}
		printk(KERN_INFO
		       "bttv%d: miro: id=%d tuner=%d radio=%s stereo=%s\n",
		       btv->c.nr, id+1, btv->tuner_type,
		       !btv->has_radio ? "no" :
		       (btv->has_matchbox ? "matchbox" : "fmtuner"),
		       (-1 == msp) ? "no" : "yes");
	} else {
		/* new cards with microtune tuner */
		id = 63 - id;
		btv->has_radio = 0;
		switch (id) {
		case 1:
			info = "PAL / mono";
			btv->tda9887_conf = TDA9887_INTERCARRIER;
			break;
		case 2:
			info = "PAL+SECAM / stereo";
			btv->has_radio = 1;
			btv->tda9887_conf = TDA9887_QSS;
			break;
		case 3:
			info = "NTSC / stereo";
			btv->has_radio = 1;
			btv->tda9887_conf = TDA9887_QSS;
			break;
		case 4:
			info = "PAL+SECAM / mono";
			btv->tda9887_conf = TDA9887_QSS;
			break;
		case 5:
			info = "NTSC / mono";
			btv->tda9887_conf = TDA9887_INTERCARRIER;
			break;
		case 6:
			info = "NTSC / stereo";
			btv->tda9887_conf = TDA9887_INTERCARRIER;
			break;
		case 7:
			info = "PAL / stereo";
			btv->tda9887_conf = TDA9887_INTERCARRIER;
			break;
		default:
			info = "oops: unknown card";
			break;
		}
		if (-1 != msp)
			btv->c.type = BTTV_BOARD_PINNACLEPRO;
		printk(KERN_INFO
		       "bttv%d: pinnacle/mt: id=%d info=\"%s\" radio=%s\n",
		       btv->c.nr, id, info, btv->has_radio ? "yes" : "no");
		btv->tuner_type = TUNER_MT2032;
	}
}

/* GPIO21   L: Buffer aktiv, H: Buffer inaktiv */
#define LM1882_SYNC_DRIVE     0x200000L

static void init_ids_eagle(struct bttv *btv)
{
	gpio_inout(0xffffff,0xFFFF37);
	gpio_write(0x200020);

	/* flash strobe inverter ?! */
	gpio_write(0x200024);

	/* switch sync drive off */
	gpio_bits(LM1882_SYNC_DRIVE,LM1882_SYNC_DRIVE);

	/* set BT848 muxel to 2 */
	btaor((2)<<5, ~(2<<5), BT848_IFORM);
}

/* Muxsel helper for the IDS Eagle.
 * the eagles does not use the standard muxsel-bits but
 * has its own multiplexer */
static void eagle_muxsel(struct bttv *btv, unsigned int input)
{
	gpio_bits(3, input & 3);

	/* composite */
	/* set chroma ADC to sleep */
	btor(BT848_ADC_C_SLEEP, BT848_ADC);
	/* set to composite video */
	btand(~BT848_CONTROL_COMP, BT848_E_CONTROL);
	btand(~BT848_CONTROL_COMP, BT848_O_CONTROL);

	/* switch sync drive off */
	gpio_bits(LM1882_SYNC_DRIVE,LM1882_SYNC_DRIVE);
}

static void gvc1100_muxsel(struct bttv *btv, unsigned int input)
{
	static const int masks[] = {0x30, 0x01, 0x12, 0x23};
	gpio_write(masks[input%4]);
}

/* LMLBT4x initialization - to allow access to GPIO bits for sensors input and
   alarms output

   GPIObit    | 10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
   assignment | TI | O3|INx| O2| O1|IN4|IN3|IN2|IN1|   |   |

   IN - sensor inputs, INx - sensor inputs and TI XORed together
   O1,O2,O3 - alarm outputs (relays)

   OUT ENABLE   1    1   0  . 1  1   0   0 . 0   0   0    0   = 0x6C0

*/

static void init_lmlbt4x(struct bttv *btv)
{
	printk(KERN_DEBUG "LMLBT4x init\n");
	btwrite(0x000000, BT848_GPIO_REG_INP);
	gpio_inout(0xffffff, 0x0006C0);
	gpio_write(0x000000);
}

static void sigmaSQ_muxsel(struct bttv *btv, unsigned int input)
{
	unsigned int inmux = input % 8;
	gpio_inout( 0xf, 0xf );
	gpio_bits( 0xf, inmux );
}

static void sigmaSLC_muxsel(struct bttv *btv, unsigned int input)
{
	unsigned int inmux = input % 4;
	gpio_inout( 3<<9, 3<<9 );
	gpio_bits( 3<<9, inmux<<9 );
}

static void geovision_muxsel(struct bttv *btv, unsigned int input)
{
	unsigned int inmux = input % 16;
	gpio_inout(0xf, 0xf);
	gpio_bits(0xf, inmux);
}

/* ----------------------------------------------------------------------- */

static void bttv_reset_audio(struct bttv *btv)
{
	/*
	 * BT878A has a audio-reset register.
	 * 1. This register is an audio reset function but it is in
	 *    function-0 (video capture) address space.
	 * 2. It is enough to do this once per power-up of the card.
	 * 3. There is a typo in the Conexant doc -- it is not at
	 *    0x5B, but at 0x058. (B is an odd-number, obviously a typo!).
	 * --//Shrikumar 030609
	 */
	if (btv->id != 878)
		return;
   this tv_debugfiguprintk("tion%d: BT878A ARESET\n",s filc.nr);
	btwrite((1<<7), 0x058ay fudelay(10ay for the m  Ral0rt

    Co}

/* initialization part one -- before registering i2c bus

  void __devthp. tionsthp._card1(strucc) 199 *btv)
{
	switchhis filc.type) {
	case BTTV_BOARD_HAUPPAUGE:his program is free softwar878:
		boot_msp34xxis f,5ay f	break;his program is freVOODOOTV_200e; you can redistri Public LFMor modify
    it unde2C) 19 terms of the GNU GeneralAVERMEDIA9/or modify
    it unde11 either version 2 of the Licee softwarPVRor mpvr_odifis f either version 2 of the LiceTWINHAN_DSTe; you can redistriAVDVBT_771e; you can redistriPINNACLESAUT A	s filuse_i2c_hw = 1either version 2 of the LicenDLINK_RTV24RCHA-2001e
   ( btv  either vers
	}   thi!tionstv Gers[lin.de>

  ].has_dvbfigutionsreset_audio hope t(rjkm@thp.uni-koeln.de)
		two & MafterMetzler (mocm@thp.uni-koeln.de)
    (c) 1999-2001 Ger2 Knorr <kraxel@goldbacs filtuner_

   = UNSET informagram is freUNKNOWN ==blic.de>

    Thicopy of adee undeeeprom_data,0xa; eithidentify_by_clude >
#include <linuay f}
ach.in-berlin.de>

    This program is freMIROe; you can redistri <liPlinux/firmware.h>
#iny of
   net/checksum.h>

#include <e <net	/* miro/pinnacle

   	
#in_lude "bt_gp General ther version 2 of the LiceFLYVIDEO_   (a/firmware.h>
#incAXIclude "bttv-audio-hLIFEaudiKIUT ANY WARRANTY; witudio.h>
clude "bttv-audio-hTYPHOON_TVIEWclude "bttv-audio-hCHRONOS_VS2 boot_msp34xx(struct bttv *_98Free ot_msp34xx(struct bttv *20cense as published byudio.h>
98EZauge_eeprom(struct bONFERENCETVh"

/* fwd decl */
statTEC_9415/io.flyvideodia/v4l2-common.h>
#include <media/tvae software; you can redistribute it and/or 

    This program is distribute/* pick up some config infos from the clude tvp.h"ux/module.h>
#include <linux/kmod.h>
hauppaugeinux/init.h either version 2 of the License, or
    (aNY WARRANTY; withoPHONE   (at x/module.h>
#include <linux/kmod.h>
avermedia void xguard_muxsel(struct bttv *btv, unPXCicenseU Geneunsigngvc1100_muxsel(struct bttv *btv, uICOLO_TETRA_CHIPbutedicolo_tetra9-200guard_muxsel(struct bttv *btv, unVHXRCHANTABIeceiradio RalNESS FORbtv, unsmatchboxt input);
stambox_we Ralp = 0x20tetra_init(strmostbttv *btetra_init(strclk bttv *bt08tetra_init(strlinubttv *bt1);

static void astv *b *bt3signederms of the GNU General PBIS_BOOSTAibut, int pin);
statiERRA modt	terratec_activesigned_upgradh>
#icommon.h>
#include <media/tvaMAGIChaupp06 the  this fil Geridelay0x3002144fe <lin *btv, unsigned=SS FOspecific stuff
  igned detected by subsystem id (CPH05x)g tvcards array fore tv);

static void kodicomSTB;
stC_muxsel(struct bttv *bt60121aigned i/* Fixsignentry for 3DFX VoodooTV 100, voi   which is an OEM STB  Ger variant.tvp.h"int input);
stat_muxs, Cambridge, MA =TUNER_TEMIC_NTSCgeovision_muxsel(struct bttv *btOSPREY1xense as published byratec_act_84 input);
static voidratec_a01v *btv);
static int tea5757_rex the implied warranttic int t_SVIDea5757_write(struct bttv 2xxt value);
static void ident0v, int value);
static void identtive_radio_upgrade(struct 44tive_radio_upgrade(struct 5cense as published byct bttvdevinit pvr_boot(struct bttd ospreel(struct bttv *btv, unsigned int inpuosprelinux/init.h> eagle_ude <linutv);

static void kodicomIDS_EAG <asmU Geneids_eagl4400r_init(struct bttv *btv);

statODv *b20;
stal(struct bttv *btv, unsigned int inpumodt);
void xguard_muxsel(struct bttv *btv, unLMLBT  GNU Genelmlbt4t unde that it will be useful,
    IBET_CS16 int ibetnsig
static void picolo_tetra_muxsel(strucKODICOM_4400ibutekodicom };
r
static void picolo_tetra_muxsel(strucGEOVISION_GV800S[BTTgv800s
static void picolo_tex/pcbtv,llnt inpuuroeln.d
    thi!is file ==848 &&y.h>
#revision==0x11)e <lin/* defaulttatic v_muxslisttvp.h" thiPLL_28elay.h.

    You should have rplligned int inplltolo_ifreq=2863int nt input);static ucrystal=BT848_IFORM_XT_muxsisioX-1) ] =35ULL };
static unsigned int autoload = UNSET;
static unsigne3546895int input);NSET;
static unsigned int audiSS FOisiokm@thsmod opeln.s can overridttvp.h"h.in-berpll should nr]e <lins pro0: /* n	    bttv *btv,NSET;
static unS16_muxsNSET;
static unsigntCS16_muxsatency,    into,   int, 0444);
erms of 0444)1
modu28 MHzaram(ls pro2/or m_param(gpiomask,   int, d int gpiomask = UNSET;
st(audioall,   int,ency,    int, 0444);
migned int audioall t, 0444);
modul2
modu35autoload,   int3;
sta_param(gpiomask,   int, nsmod options */
module_pa
module_param_array(pll,      int, NULL, 0444);
modulic voi, 0444);
}more ray(pll,      urrent = -1r mo/* ridge;
static unsign(= { [ 0 ... (BT/ autot bttv /e_param(vsfx, )ned int s39, Uhas };
static unsigned int autridge, MA figug bit "
		lay.h>
#or triton1 + o, Cambridge, MA 021enable bug compatibility for triton1;fig bit "
		 "[ridgeno_overlay, CHANTABIridge, MA 021
MODULE_PARM_DES informatincy,"pci latenc= d gv80ABSENTrd-specificKERN_INFO  stuff
  iomux,absentg tvy.h>
#inrray felse ifESC(card,"specify TV39, Ud model, see CARWARNINGT file for a lis MA 0unseMODULE_PARM_DESC(pll,"s model, see CARDLIST file for a lisc voi%dODULE_PARM_DES inpt bttvDULE_PARM_DESC(vs informaDESCloa has l (0=n <lin, 28=28 MHz, 35=35 MHz)");
MODhe_DESCE_PARvsfx, tic obsolete.ODULE_PARM_DESC(plev, "specify audio device:\n"
useno audio
    00, tda7432 or tvGNU G to0 = autodetect (default)\n"
		"\t\t 1 = msp3400\int, 0444

stat 3 = tmodule should be\n"
d 0 = autodetect (dex/pcthers]");
MODULE_PARM_DESC(vsfx, Cambridge, MA 021/grabber carr moSC(cadignfig bit "
		 "[yet another ceceivig_in ?on, pl};
static unsigned int aut void inputs - 1 :139, USAlt (0 svhsnfig bit "
		 "[yet another c------= NO_SVHSipsets aDESC(a :-------------------------------w worka----no_overlay,RM_DESC(auverlay,"-------               w workaremote                              unsinsign = unsigned id;
	int informations    You should have receiigned	char *name;
}gned NESS F 0x13eb0070, BTTV_BOARD_HAUPPAUGE87nsign	char *name;
} cards[]	{ 0x390ils.

    You should have rnd initirq          "Haupp},
	{ 0x39000070, BTTV_BOARD_HAUPPAUGvolumedia/v         0,     "Ospnfig bit "
		 "[yet another c0,     "Osp{ 0x39000070, BTTV_BOARD_HAUPPAUGGNU G_mod  "Osprey-100" D_OSPREY500,   nfig bit "
		 "[yet another cD_OSPREY500,   _PARM_DESC(card,"specify TV/grabber card moration  moduler a lisoogralav *bdrivers to E_PAR*/PARM_DESC(caeceisaa6588 || prey-44no_overlay, int, /* Probettv *RDS rece0, B chip4);
modtaticnt istDESCigneddefart addrs[] =gned ibtv) >> 1 inpu0x22I TV WondeI2C_CLIENT_END int(defKnorr <v4l2_subdev *sdr mo	sd =T2000, or new,   "Le(&.h>
#in2000,dev inpu7107d, B or adap, "prey-44"dtek WinFast 0,D_ATI_common     "Osprey-440= (s has NULL definitaudiryTV_B(tritonGNU G/fadI TV Woni-ko TV 2First check if
		"\user specified
		"\t\ = tV Wonvia aodule, 
, plvsfx, ct btach.in-berN 600devno_overlay, int,s pro-maSLCration er[BTo notBOARD_any RDS module, dARD_S0444);
moduDESC(tritonuxsel(e for momodule_pTV_BOART TV PCI FM, Gateway at w default p_ATI_TVWO2 = tdander" },
	{ 0x00031002, BTTV_BOARD_ATI_TVWONDERVEx660ADDR_MSPTV4/P },

	{ 0x660 Data Co. GV_ALT" },

	{ 0x6606107d, BTTV_BOAR         dy
    itst TV 2000" },
	{ 0x6607107d, BTTV_BOARD_WINFASTVC100,  "Leadte2 = tdast VTV" },
	/*	{ 0x6609107d this fil. GV-BCTV5sfx,"ration i		got{ 0x GNU Gdefinituner,   05010fc, BTTV_BOARD_GVBCTV4PCI,    "I-O Data Co. GV32\n"
		CI" },
	{ 0x407010fc, BTTV_BOARD_GVBCTV5PCI,    "I-O DataTDAn"
		TTV_BOARD_GVBCTV5PCI,    "I-O Da thiTV 2000" },
	{ 0x6607107d, BTTV_BOARD_WINNFASTVC100,  "Leadte32\n"
	st V_VOODOOTV_	{ 0x6609/
	{ 0x1200bd11, BTTV_BOARD_PINNACLE,   3  "Pinnacle PCTV [bswap]" },
	{ 0xff00bd11, BTTV_BOARDt 3 = tuxsel(sped IDARD_STB2t TV 2000" },
	{ 0x6607107d, BTTV_BOARD_WINFASTVC100,  "LeadteARD_STBTV_20y Magic/oa743t 3 = __ATI_()th byteswapped ID CPH06x/
	{ 0x1200bd11, BTTV_BOARD_PINNACLTTV_MAXbutedult)\n"
		"\t\t 1 = msp3400\nnknown/N 600"Leavalue!g tvfx,"set Vtect (def0x1200bd1ek TV 2There wTV_Bnooad the ss, s{ 0xw,   000 XP" iscint, this through
		"10b4,_muxsdefthp.uigned TTV_MAta Co. GV-BCTV4/Pf,   "
		is0070, B     (tritonwheth },
	{not10b4,it reallytic v 2 = tda74soD_MAwill ration   "L3000n
		"\
   c (T1/Lfoundtic GICTVIEed ithmocml,"sp(e.g. a tea6300)ct btt5000070, BTTV_BOARD_HAUPPAUGEPVR, s ... */ <linux Co. GV-BCTV5/PCI" },

	{ 0x001211bd, BTTV_BOARD_PINNACLE,      "Pinnacle PCTV" },
	/* some cardonder, " },
	{ S("I-O Data Co. GV-BCTV 0xa0}aster  0x13eb0070, BTTV_BOARD_HAUPPA
    it_altia TVPhone98" },
	{ 0x00021461, BTTV_BOARD_AVERMEDIA98,   "AVermedia TVCapture 98" },
	{ 0x00031461, BTTV_BOARD_AVPHONE98,     "AVerMediafc, BTTV_one98"4f, BTIf,   OEBE_T1, "TVixx,
		"n we're done 0x0001146apped IDs ... */
	{ration infkm@tt might also (0)aRD_PINNA 0x00011461, BTTV_BOARD_AVPHONE98,     "32\n"
	ia TVP    "Pinnacle PCTV [bswap]" },
	/* this seems to happen as well ... */
	{ 0xff1211bd, BTTV_BOARD_PINNACLE,      "Pinnacle PCTV" },

	{ 0x3000121a, BTTV_BOARD_VOODOOTV_200,  "3Dfx VoodooTV 200" },
	{ 0xe (PhilNow se	{ 0xwe     find		   oSTB TVARD_STB2TV_BOAsct btt3Dfx VoodooTV 100/ STB OEM" },

	{ 0x3000144f, BTTV_BOARD_AGICTVIEW063, "(Askey Magic/others) TView99 CPH06x" },
	{ 0xa05144f, BTTV_BOARD_MAGICration inV_BOARD_:
002144f, BTTV_BOARD_MAGICTVIEWN 6000t");
M,_MAGlyVideo
	 *{AL B/Gers) TView99 CPH05x" }
rjkm@thp.uni-ke with omux,-koeln.de)
    (c) 1999-2001 },   Knorr <kraxel@goldbacinRD_ATI =  Data39, USA.

*/
alue Radio	 "[enable bug compatibility for tri_ATI    atec TV0015b0, BTTV_BOARD_ZOLTRIX_GENIE, "Z_PARM_DESC(card,"specify!TV/grabber cardPhilipsorr <ridge,setupBTTVOARD_Z-O Da/* L_PAR },   dule, darcus Missumocmiomux, int,  call!TTV_MAX-1)3eb0070, BTTV_BOARD_HAUPPAUGE878,  "Hau	_PINNACLE,      "Pinnacle PCTV" },

	{ 0x3000121a, BTTV_BOARD_idgeTV_20" },

{ 0x30, TV 2000" _GENIE, "Zs ?? *S_RADIO 0xa00x401615b0, BTTV_BOARD_ZOLTRIX_GENIE, "Zoltrix Genie TV / Radio" },

	{ 0x1430aa00, BTTV_BOARD_PV143,         "PrDEMODo PV143A" },
	{ 0x1431aa00, BTTV_BOARD_PV143,         "Provideo PV143B" },
	{ 0x1432aa00, BTTV_BOARD_PV143,         "PTV_WITHProvideo Pint TRIX_GEN.EY500it(st= T_ANALOG_TV | T_DIGITAL_TVbd11{ 0x1460aai config ncy,"pci lateo PV150A-1" },atec TVRD_ZOLTRRIX_GENIE, "Zoltrix Genie TV / Radio" },
	{ 0{ 0x1460aa00, BTTV_B|OARDovide-O Datatv_BOAR_ally = UN 0x14, s, MA E, "Z, &{ 0x1460a definition.461aa0da9887_t in0x400d15b0, B2000,priv_PV1a04, igRD_P464aa0fg},

	{ PV150B-1"for trride overTDA464ao PV1 0x1465aa05     = 7107d,0x1464aa04, },
	{ 0x1463aa03, BTTV_BOARD_PV15t inpu  "P PV150B-1" defin(rjkm@-7, BTTV_BOARD_PV150,         "Provideo PV150B-4" },

	{ 0xa132ff00, BTBCTV3},
	{ 0eln.dTTV_MAX-1) ] =OARD_TERRATVALUER,  "Tf( strncmp(&(clude <linu[0x1e]),"Temic 4066 FY5",14) ==0ia TVPhone9
static void gv800s_ini	{ 0FY5_PAL_I(default)\n stuff
  MTV_MA: Ta listESC(trito *btv,clude : %srs) TVi please do n opti&200,        "IVC-2e98" },
	{ 0x0_BOARD_IVC200,        "IVC-200Alps TSBB550001, BTTV_BOARD_IVC200,        "IVCALPS_ },
	0xa1550002, BTTV_BOARD_IVC200,        "IVC-200"  },
	{ 0xa1550003, BTTV_BOARD_IVC200,        "IVC-200"  },
	{ 0xa1550100, BTTV_BOARD_IVC200,        "IVC-PhilTV_BFM124650001, BTTV_BOARD_IVC200,        "IVCPHILIPSit(struct, BTTV_BOARD_IVC200,        "IVC-200"  },
	{ 0xa1550003, BTTV_BOARD_IVC200,        "IVC-200"  },
	{ 0xa155010iodev, "speBOARD_IVC200,    U61, "(A    "St(moc003, BTTV_BOARD_IVC200,        "IVC-200"  },
	{ 0xa1467RD_IVC100,  e)
    (c));
static void xgKnorr <kraxel@goldbach5b0, BTveagle_mtv},

 },
	{ 0_);
static analog07107d, or cli* LR1&= UNSET;
int no_ovetency,"pci latency vchipset flaw w*btv, unsigned i     UGE878,  },

pecific stuff
  H;
static eagle_mindicates    el#d, "obverlay," opti    OARD_ },

/*.c

 Sd ines with878 bo You have dupl BTTV PCI IDs. S.in-be		"\C120,82ff0 MA 0base claBOARD_ #..c

    t(20G" },
----6490TTV_BOApecific stuff
  xa182fmocmC120,D_IVC1tic v%TTV_B3, BTTV_B      "IVC-D_IVC1, BTTV_BOARD_HAUPPAUGEPVRame    "IVC-120G" },
gram is free softwar_IMPACTVCB0f, BT9107d, BTTe>

  ULL,     "IVC-120G" },
	{ 0xf05000,        "IVC-Terr input);
static void kodicom44OARD_TERRATVALUER,  "Terrsignfault (0  unsigned int input;
static void picolo_teta_init(struct bttv *btoid ttatic void tibetCS16_tv);

el(struct bttv *btv, unsignd int input);
static v04" },

	{ 0x414t(struct bttcfault (0 t(striow bttv *1 <<  Tec Multi Captior Grand X-Guar9OARD_GRANDTEC,selGrand X-Guaoid 
	signe88000/62.5;
	tea5757_ the m = UN5 *IVCE8 +v *b   Cmodu the -878ed8ned int sS_VS2, ==)" }ttle dule ... udiodev, "spe stuff
  Tinput); Atatic Rgned Udicom4AL B/G 0 = V_BOARD_IVC200,    9107d, BTTV_BOigned int input);
static prey-440t input);
static void picolo_tet "IVC-120G"Mail AV" },
	{ 0x185_muxsel(sttic void picolo_mux}
53b, BTRD_FE,   "T7, BTTV_BOARD_PV150,         "Provideo PV150B-4" },

	{ 0xa132ff00, BTTV_BOA/*
 >

#nimal odifstrap_BOARDhe WinTV/PVR & Mup2010fclteraEW06mware.
 iew F, BThcwamc.rbf     "AG  fil},
	    1011G" },
	{ 0 CPH050CD.  Haveew Fa look at Pvr/pvr45xxx.EXE (self-extrstatng zip archive,     b201/Munpacked with unzip)GMV1/
#,
	{ e PVR_GPIO_DELAY		10
217d6606,     ALT_DATAder/0V 2012000,   "Leadtek WiCLKder/1V 2002000,   "Leadtek WNtatiIG	0xrono00BTTV_BOARD_IVe)
    (c)d in,     _E_PA{ 0xf0500002, BTT,0x0003108 *micro0aa00, Ralpu32 
	{ 0lenldbacBTTV0bd1u8 bitcardsnt i(LR5ia/v_inout(0xffca1a,Leadtek WinFa|xfff6f6ff, BTOLTRIX,   dtek Wi{ 0xI" }, the mC) 19yright  BTTV_BOARD_WI },

},
	{ 0x82bFace to Face Tvmax" , BTTV_BOARD_SIMUS_GVC1100tv *(nOARD_ n <V_BOARD_W; n++ia TVPhit----
	{ 0[nCARD      iOARD ; i < 8   "{ 0x200a0, "SI1295VC1100" },, BT,; eith (LR10295,&{ 0x1
	{ 000A" },

	{ 0x4011155ATA, BTTV_BOARD_ZTV_BOAalled tProlink Pixelview PV-BT" }TTV_BOA0A" },

	{ 0x40111554, B 0x40111554, TV_BOAV_BT8>>NESS FOt, NUL0A" },

	{ 0x40111554, BTTV_B, BTTV_BOARD_SIMUS_GVC1100/* begin A        (c)loop (Not necessary,but doesn't hurt confiC200,        "Ima3    "{ 0x200a0A" },

	{ 0x40111554, BTTV_BOeo Tuner" },

	{ 0x01051805, BTTV_BOARD_PPicolo Tetra Chip #1" },
	{ 0x01DEO_98FM, "Li00" },
	{ 0x03116000, BTT the Knorr <kraxel@goldbacx00031C-120G" BTTV_BOA*fw_uct bus WinDr,    rcs[] _quest_    "AG (& Securit, "0x010114c7",B-2" },c.pci->de. (BT {
	ucGeniV_BOARD_IVC120 MHz, 35=35 MHz)");
MO02 *          "AG  [},
	hotplug]"FlyVideo 98EZ (LR51)/ CyberDEO_98F IntLYVIDace" BTTV_BOARD_SENSensor Securit->linux Maxi TV VisizBTTV_del, see CARDLIST file forid doesn't matchRD_GMV13, BTTV_OARD_IVC200,     Video<nter? "failed" : "ok"1, BTTlease0x5353414a Securit1, BTTV_BOA Int467aa07, BTTV_BOARD_PV150,         "Provideo PV150B-4" },

	{ 0xa132ff00, BTTV_BO/*ned ined int FM, Gatc stuff pleaseeg data xfer) */
	{ 0x001c11bd, BTTV_BO_BOARD_IVC100,  e)
    (c)ed int latencyORAY311,   "Sensoray 311" }ee[256]R,  "Terrix01032 seri4);
modulWinDtruct bt_array(audTskeyco saa61)"nevery actuCTVIEgeTV" lled i_205BTTVase...OARD_TERRATVALUIVCE87844,      "IVCElinux/dBTTV_BOARV"},
AL B/G
	{ n antiqu0x00       tv *MMAC label00 Teagle_muxsel50000_BOARD_Iee, "HAN_", 4eo Shut  "C     sumt, 0444);C200,       "Ima21"PicolOARD_D_TWINHAN+=_NEBiRD_PXnPlusD_TWINHAN!{ 0x021SC(la{ 0x1200bd11	" },
	{ 0o_upgrade(struct bttv *bT,   "ChainTec12 digitop DST-1000 Dlectron*= 1w 99ectron	{ 0x077 - '0', int, N"IVC-120G"002, BTTV_BOARDTV_BOA_PXC200,    4*16  "ImagTV DVB-+= 16 BTTV_BO16RD_TWINHAN_Dip_compute_csum(ee + i,{ 0x"DViCnPlusTTV_BOARD&a0fc) +BTTV_BOARD>>81, B { 0xd,    iodev, int, nPlusi >= Lite/
	{ 0x1200bd11    =DVR PC_TETL B/G)" vali },
scripto     		CE8784,get_unal, BTT_be16((_
	{ 0 *)(ee+4" },

	h.in-b(clude <lin/* _MAX20,   oad,   int TV 2  GNU AverTV DVB-T 771" },
	{ 0x07611461, BT, 0444);
modul TV 2dule_pAverTV DVB-T 771" },
	{ 0x0ad(str_param_arrayV_2,	"D_IVC FusionHDTV 2" },
	1unsig00a, BTTV_30x763c008a, BTTV_BOARD_GEOVISION_G,	"GeoVision GV-600" },1  GNU63d800b, B	{ 0x18011000, BTTV_BOARD_ENLTV_Fxay(audiodev, in63d800b, Bgned 63d800b, B70c, BTTV_BOAR21;
stGV800S_SL,	"GeoVision GV-800(Sv, iner)" },
	{ 0x763d800b, B/or m63d800b, B9_GEOVISION_GV8d[BTT63d800b, BFS_SL,	"GeoVision GV-800(S) (sla2xxer)" },
	{ 0x763d800b, BAon GV-800(S) (Blave)" },

	{ 0x15401830, BTTV_BOtv,
		,	"GeoVision GV-600" },devinL,	"GeoVision GV-800(S) (sla50ter)" },
	{ 0x763d800b, 5	{ 0xo PV183-3"gned 15401832, BTTV_BOARD_PV183, 4ter)" TETRVC-1ed int 540
static voiaram(latVision GV-600" },6 },
	{ 0x154017 },
	{ 0x15401A	{ 0x15401832, BTTV_BOARD_PV183,2ster)" /* enable outpu			  selitoncontrol lineni-koVideo Tu
	{ 0xa0fca1a0, TV 2303RD_PICOVision GV-600" },D 0444)AverTV DVB-T 771" },
	{ 0x4ovideo 15401837, },
	{ 0x30o PV061, "(...le    generic, 0x01HDTVB-T 761#aram(ladel, see CARDLIST file for"input);ay w"sing pcxa155000061, "(A_muxsCE8780x%04xrs) TVieread.
	* { 0x13eb0 anymorUSIONHDTV_5_LITElectronicive! Mini "},
	{32xd200dNACL BTTV6TValue (Pdel, see CARDLIST file forh description f Ger=%d '%s'-------=%ueprom read.
	* { 0x13eb0truct om read.
	truct >0 ?ie TV" },
	{ 0truct 0f, BTPPAUBOARD_I"DVB-T 76more");
Mtruct <00" }TV_BOARD_TWINHAtruct atec TValue (Temi / bt878 ti805, sDTV orrectlyned int s Ger            <183-4"num070, BTTudiodev, "specify audio device:\n"
h description f
/* rray withip int, 04b426 PCI FM, Gatewa / bt878 o use the card defi"IVC-120G" },
	{ ------ */
	[BTTV_BOARD_UNKNOWN] = l		= MUXSEL(Changb426 / bt878 t82ff0ed BTTV, "obsolete option, please do ne>

  .video_iTTV_BOARD_IVCE8784,truct { 0x1467aa07, BTTV_BOARD_PV150,         "Provideo PV150B-4" },

	{ 0xa132ff00, BTTV_BO/* AV;
statici function .1 ,	[BTTV bktr1 Ger.cr) */
	{ 0x001c11bd, BTTV_BOARD_PINNACLED_IVCidge,0_t3-6"TVWONDERIVC120,        "IV, de over,       PALmoduPAL-BG*/,= ADDR_UNSET,
	}PAL, TTV_BOARD_STB] = {
		.name	I "STB, Gateway P/N 6000699 (bt848)",
		.vidSTB, Gateway P/N SECAM,TV_BOARD_STB] =7,
		.,
		.gpiomask	= 7,
		.muxsel		= MUXSE	= 2,
		.gpiomask	= C-1216ME_MK3 I-O r_type	= UNSET,
	1.tuner_addr	= ADDR_U0s_init(stBTTV_BOAR0s_ini	= 2,
		.gpi_NTSC,
		.699 (bt84_NTSC,
		.tuner_addr	= ADDR_UNSET,
		.pll            = PLL_284012FY5.muxsel	/* ---- card 0x063, gv800s_ini7,
		ionHD	},

	/* ---- card 0x04 ---------PAL	= 1,
		.neeln.de)
    (c)t);
static void gOARD_TERRATVALUER,  "TerrSET,
	make_BOARD__tv_fmudio_inpformatudio_inpuE8784, (LR54,
		/* .at bttv *C200,        "I41]878P_7ructio_inputs= l		= MUXSEL(2, 3, 1, 1),
		.gp18)wellpiom */
		.svhs			= MUXSEL(2, 3, 1, 1)2
		.gpf0ype	=01020304,me;
} cards[]r_addr	= ADDR_UNSET,
	}0lateund Vi0,
		.muxseRD_D0" }deo_inputs	= 42 + otherER_ABSENT,
		<	{ 0xaPV150,  ci latency time		.tuner_ */
		.svhs	CARD {
	deo_inputs	= 4B,  " */
		.svhs		= 2,
		9omask	= 3,
		.muxsel		= o	= 1,
	, 3, 1, 0),
		..gpiomux 	= { 0, 1, 4, 1 },
		.gpiomute 	=RD_DV0
		.needs_tvaudio	=------LGit(st_NEW_TAPCOARD_ne",-G702PBCTV3Pdel, see CARDLIST file forA);
statiription[v/gr2muxse]or a li=0,        "IVC-1SEL(2, 3, 1, 1),
.gpiomask	= 0x0f,2	{ 0xpiomux 	= clude <linuxncy,"pci latency timeTTV_BOARD02144f, BTTVCONT "%d"	= 2,
		.gpi{ 0xa15501me cards ?? */
		.neeBOARD_IV.gpi_IMPAcards ?? */
		.neel(stru:%MAS,cards BTTV_B: eeprom read.
	 	= { 0 },
		.ne? "yesUPPAUnorom read.
	* { 0me;
} cards,
	[BTTV_BOARDal Publiew FFor unsign TV/FM and,
		.vid200. ,
	{s   }rds'emote s\n"
	a V150, 001/MM0,    demod,;

static 0x40I2Ctvphone_-200likValue"newlistn    r_tvpmmon" },
150,  -----{ 0x Instea_TVMAhaswith tri-},
	e erlay pins, S0ux 	=S1,ew F4PCI, BTTV_BO1011IF{ 0x01011 voidux 	=N 600.  Appa_parlyLE_Ptv V_BOew F_BOARD_ODTEconn  },
	to S .auS0 l+ (V11836061,38.9autolVrd 0x08B/G/D/K/Iew F(i.e.,name)aa65le highview FlyV45.7, NULLTWINH/NVideo PCt(st
	{ 0x2la EIVC-12 PV150OARDnor},
	{ 0x01010071, BTla Eia/vV_BTldbae (LR102)" oTV 100= TVAUDIO_INPUT_-----0x400dIX_GENIE, "sk	=       omute ]BTTV_Bid & V4L2_STD_MNO_FUSXSEL(2,  PV1_BOARD_ LiteV_BOARD PLL_28,
&= ~tuner_typee (P0A" },

	{IVC-120G" },
	{ 0xa182ff0fia/v_BOA,UXSEL(2, 3, BTTV_BOAXSEL(2, , "Lifevew Ff the/183-6" 1011MSP0x154d inG" },
	{ 0puts=" },
	ankTTV_BKyösti Mälkki (kma	= M@cc.hut.fi)!MV1" },G" },
	{ : er_a  5ew Funsign:ay wi 2, 2		.g/   "IVC-120G" },
	{ 0xaodify
    it ORAY311,   "SensorWinDpiWINDVRWinDTTV_BOA(X-Gua_addR PCI" },
	{ 0x	= "I_BOAax" },
	{,

	{	= "IC) 19mright 20x01061805,50C) 19.name		= "Haupp] = {
	 0x39000070"Osprey-10
		.svh_ 0x1kinglemot some xx_IMPAIX_GENIE,verbosauppadel, see CARDLIST file forG" },
	{ /
		.vid
    it:3,
		/OARD_	.tuner_addr	  (c)[%dagic 	* { 0x13eb0[BTTV_udio_inputs= 1, */
		.svhs		= 2,
		.gpiomask	= 7,
		.muxsel		= MUXSEL(2, 3, 1, 1),
	 Imagenoeln.dL-M182ffunsign Framegrabb      ame	, BTTis,
	{i HD-yo_inpsL(2,procedure a	.gpi) is btv,Al },
ndro Rubini00 T"},
pxc20		.gpar Tech------usb426     funcfx,   RD_PINNACLESAT,   "Pinnaclnt input);

RD_IVC120,        "IVC-type	= UNval_TVWe)
    (ct);
s= {, unsrt

 9rt

 art

 brt

 dard 0x0,    = MUXV200rt

 2rt

 3rt

 4rt

  rt

 6--------------0x	= DVICO_DVBTV, "Nebus	= 4mpebula EvalOLO_TETIhp.uni-se
	[BT-EO] =v},
	n .1 f183,I" },
	{ 0xa0fca1a0, ost 13 0xa0},
	{ 0x82b2aa6a, BTTV_,
	{ },
	{ 0x82b */
		;O_TETV_BO		.tuns atchpu 1, 0up1, "(As ne
		.na070, 82ff0ute 	= 2, c, Blong      	= 4,
		/* _inputs= 1C) 19 1, */
		.svhs		= 2,
		.gpiomask	= 0x0f, 2, 3, VisiO_TET"    ault/efault 000 x 	=,
		/*
	/* ---- caAD pots?----m readrL B/GAske,    implyremorBTTVofSTB TVcrus120G.  Witho .audio_ew99 CPe AGC
		.ftey CPftsaudio_iemem= 3,2, 3ENTVMAS,, BTe logic -->m readset0b426igned ADC_AGC_ENtheraudio_inpAGC		= MUXboult@eecs.le	.vi.edu
	144f,or the mx_alt    =he bRVED|x_alt    = 1,
		,xx_alt    	.name		r TV (bt848MAX517 DAC= 3,
del, see CARDLIST S.msp34x_gpireference voltage level .. 0 =ay fortv_I2CWI" },
	{ 0x5E,0,Fast,00",
	v_stereo_audio12C508 PIpioirq/*	, BT Xtreme re 98I2CRer_t},
	mand.gpiom      "pcto06L (T1*	.svhsBTTV_B------2, 3R/WCanoo_ininclud2000 TV"e"Provesmux *	argumparasL(2,  nu, 14.gpiomdif= {
	 GV-B 3,
		/* .audio_inputr TV (bt8b426dio_inputs=V Wonb Video HD_STB2,    of all,V183-6" iomaslockOARD_..audio_ink	= 0: {0, 1unsign-F= 3,
v4);
mbtonos igned V_BOARMA_CTLeadtdeo_|LL, 0444* .audio_inp_GPCLKMODEy for the mvalpio= ave* .audio_inputs44f, BTTn, pushSEL(0RD_ZOvaudio	= 1.tun en061/06o 0 },
	e 	=182ff00x50181.svhsas abovV_BOARD1, 0 },
	ARD_------0x40/
		.svh82ff0c/oth );
MSEL(2, 8)",
		.vid },
	n .1 82ff0

stat	   EL(2, 3goam(vne?= 3,
		/* .audio_inputs=  MUX2	.svhs		= 2,
		.gpiomask	= 1(C) 19.muxsel		= MUX",
	RD_PICOLO_TE  "ImaARRAY_SIZEsel	s)"Picolo Tettmp=ghway Xtreme (VHX)",1		.vET,
	i]_inpu1 },
		mp_AVD-1 BTTV_Bdel, see CARDLIS/* array witXSELreme (%2.2x) = %i\nXSELomask(.ru> mail\nber cards      xsel		= tmp,ghway Xtomas1fe00,
	F,  "Le geovisi----------------- */
			.vider TV (bt84  "FlALUE,  io_inputs= 1, */
		.svhs		= 2,
		.gpiomask	= 7,
		.muxsel		= MUXSEL(2, 3, 1, 1),
1, 1), BTAdlink RTV-24 (aka Angelo)e	= T */
	FM, Ga  "TTV (bt8oeln.dto unTRIX1, 1)it
		.nama--------- involveL(2, 3followb426		= 2,
		.tv *each2" },V Wo:MV1" }, 1)_CHRONOS_00C3FEFFrd 0x10 ----_OU, BTMetzler (.gpiomux2) _CHRONOtocfa007BT" iomuxaudi-4fa0Es_tvaudio	=sleep 1mpiomasudio	= 1101, BT,
		.tuner_type	= UN0SET,
		.tuner_a,
		.tunerdule	[BTTV,
		.need= "Ao buf (uint_32)T,
		.tuner thi linu>>18878P_9Bo Inter|| (TV_B>>19 "AVEC Inte1 _INTERCAP].audio_inpuerror. ERROR_CPLD_C    _FRD_HA stopGMV1" }, 3	= { 0x4007,
		.needs_tvaudio	=CHRONOS_ };
r	= ADDR_UNSET,
		.volume_gpio	= winviegpiomux 	=ddr	= ADDR_UNSET,
		.volumeaudio	= 1,
		.tuner_tvolume,
		.has_radio	= 1,
	},
	[BTTV_BOARD_AVEC_INTERCAP] = {
		",
		.= "AVEC Icapture",
		.video_inputs0= 3,
		/* .audio_inpus= 2, */
		.svhs		= 2,
		.gpio	{ 0x2eview FlyVideo 98 LR50 Rev Q" },
	{ 0x18501f7f, BTTV_BOARD_FLYVIDEO_98,   "Lifate and Sha
 General P BTTVrr <kraxel@goldbacRD_A32_t	.namomaskARD_FLiomutwoid dog_/* ---AVERME120,       -20 by "Antonts= 1, */
adtek WinView/* .audio_inputin		= g, 3,r_addr	,        "IVCts	= or the Vide00c3fes= 1 MUXSEL(2, ,0xcfaaffles Card",
	 +NSET,
	},

	/* nputs	= 3,
		de0a01, mpe	= U(laters Card",
		ddr	=		.svhs		= 2,
		.muxsel		= MUXSEL(2, 3, 1, 1C) 1996,97,93, */
		.svhs		= 2,
		.muxsel		= MUXSEL(
	tuner_addr	puts	=/

#"Lifeview FlyVideoB-T L((tuner_add TV 8)878P_9B,Intercaptur0",
		.video_in9uts	= 4,
		/*  *mastex14 ----------------------------------- */
	[BTTV_BOARD_CE(1)*/
		.svhs		= 2,
		.gpiom(ar Im%d;

stD_IVC120,     .tuner_adValue (Ps Card",
			= { 1,NSET,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_CONFERENtype	= U		.svhs		= 2,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.tuner_t		.svhs		= 2,
		.muxsel		= MUXSEL(2, 3, 1, 1),
	 98/ Lucky Star Image World ConferenceTV LR50",
		.video_inputs	= 4,
		/* .audio_inputs= 2,  tuner, line in *0eo Shuttle II,
		.gpiomask	= 0x1800,
		.muxsel		= MUXSEL(2, 3, 1, 2),
		.gpiomux 	= { 0, 0x800, 0x1000, 0x1000 },
		.gpiomute 	= 0x1 },
	{ 0x3005144x14 ---------------------------------- */
	[BTTV_BOARD_CEICO_D\t\t 0 = autodetect (d_type	= UNSET,
		.tuner_addr     = ADDR_UNSET,
	},
	[BTTV_BOARD_WINVIEW_601] = {
		.n MiroRD_Al(strucn .1 f-- with 0)/ ChYVIDEO] = {
		.na */
	 1, 1portsV_BOARD_.name	Copyinputs(c) 1999 Csaba Halasz <qgehali@uni-miskol 1, >" },
	BTTV_BOAEL(2lac= { nb4, 7,
		.rmsTTV_BOARGNU G----al Public Licens201/ew FBrut 1, *h8,  "Sby Dan Sheridan <dan.sAL_I,
	@ BTTact.org.uk> djs52 8/3/, 1, BOARD_IVC100,  bus_lowNER_TEMIC_PAL,
		.tunerbit000, BT},

	{  0x18501lo Tetra Chip #3,

	{ 0x185018uner_adGrandtec 06X (bt878) ,    input)"Askey CPH05X/06X (bt878) [many vendors]",
		.UXSEL.video_iValue (P	.name		= "it{ 0x01061805,maskOARD_MAGICTVIEW061] = {
		.name		= "Askey CPH05any vendors]",
		.vaddr	=
		.gpiomask	=      "IVC-120G"----	.vi----------------- */
	[BTTV_BOARD_MAGICTVIEW061] = {
		.name		= "Askey CPH05X/06X (bt878) [many vendors]",
		.video_inputs	= 3,
		/* .audio_inputs= 1, */
		.svhs		= 2,
		.gpiomask	= 0xe00,
		.muxselBTTV= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= {0x400, 0x400, 0x400, 0x400 },
		.gpiomute 	= 0xc00,
		.needs_tvaudio	= 1,
/
	[Buc un----------------- */
	[BTTV_BOARD_MAGICTVIEW061] = {
		.name		= "Askey CPH05X/06X (bt878) [many vendors]",
		.video_inputs	= 3,
		/* .audio_inputs= 1, */
		.svhs		= 2,
		.gpiomask{0x400, 0x400, 0x400, 0x400 },
		.gpiomute 	= 0xc00,
		.needs_tvaurboTV",
		.ronos uts	uxse	.tuner_tTEAuxseletzler (Canop 0x217d6606,TEA_FREQ		0:14video_inputs	BUFFER		15:15T2000,   "uts	SIGNAL_STRENGTH	16:17hs		= 3,
		.gpPORT1		18:18	= MUXSEL(2, 0, 10		19:19hs		= 3,
		.gpBAND		20:20" },
	{ 0mute 	= _FM		WINFAST200dio	= 1,
MW		needs_tvaudio	= 1,
LW		2ner_type	= UNSET,
SW		3hs		= 3,
		.gpMONO		22:2
	},
	[BTTV_BOALLOW_STEREOtuner_type	= UNFORCE= "Life1hs		= 3,
		.gpiEARCH_DIRECTION	23:23	= 4,
		/* .audio_inpOWN.tuner_type	= UNudio_inUPputs	= 4,
		/* .auTATUS		24:2 .audio_inputs		.gpix800,D0x1800,
		.muxsex1000,udio_iINGuts	Zoltrw-Aimslaputs	= 3,TV_BOARD_IVCE)/ Chronos tuner_type	= TUNER_ABS02, BTTViomuttimeout Techno
	/* ----us WinDVR PCTETRAtt878safARD_a1, *rrPCTV",I" },
	{ 0x0304, BTTV_BOApll		= ruct bttv06X (bt878) wymore");
MAGICTVIEW061] = {
		.name		= "Askey CPH05X/06X (bt878) [many vendors]",
		.video_inputs	= 3,
		/* .audio_inputs= 1, */
		.svhs		= 2,
		.gpiomask	= 0x 1, */
		.svhs		= 2,
		.gpiomask	= 0x0f, 	.muxseldule	= ADD--------enso    = "Terratecte 	= 0x40000,
		.needsclnputs=yright (C) 19NSET,
	= jiffies + msecs_to_addr	= (OARD),
		.gpwait(2, 3
	},
 4,
	to g_BOAw;udio_i= {
i01071805, type26",
	(sel		= 0000,
		.needsde <lX];
NSET_arcus (addr	= ,UNSET,
	V 200schele, (dr	= ADDRfor Active Radio Upgrade udiodev, "specify audio device:\n"
	e	= TUomutix GSET,
	

static void geovDEO_98FarraCard" pecific stuff
  : om5610tvcards array f"ChainTech digito4"Picolo TetUXSEL(2, 3,	te 	= PLL_		.tuner_type	= TUNEIOR (bt848)
			n.c.
		gp%c",om5610-data
		gpio03:  tibOARD_D)?'T':'-')
			gpio 0,
		.tuner_type	= TUNE	= 1,ue <<NESS FO20: u4|=r (for Active Radio Upgrade bgpio180:1"OspreMSBEW061,ionHDT16: u2-A0 (1s om5610-data
		gpio03: -A1
		gpio18:S':'MEN
		}7 n.c.
		gp\n		gpio05: om5610-stere0x%X		.pll		= PLL_2820: uo TurboTV",20: uD_ACORP_Y878F, "Shuttle II" },ER_TEMIC_PAL,
		.tuner Mute
IGITV, "Nebuideo esabl[BTTV_B- */
	[BTTV_BOARD_TERRATV] = {
		.name           = "Terra       = "Terde <linOARD_MAGICTVIEW061] = {
		.name		= "Askey CPH05X/06X (bt878) [many vendors]",
		.video_inputs	= 3,
		/* .audio_inputs= 1, */
		.svhs		= 2,
		.gpiomask	= piomux 	= { 0x20000, 0x30000, 0x10000, 0x00000 },
 the 	= ADDn.c.
		gpio08: nIOW
		gpi	.tuner_t: Radio
			40000 : Mute
	*/pio19: u4-A0 (2nd 4052)
		gpigpio10: nSEL (bt848)
		_tvaud"ChainTech digito5"Picolo Tet {
	ungtuner_t TV 0sfx,"spio10: nSEL (bt848)
		t no_over= UNSET,e 	= 0x40000,
		.needst no_over /* -A1
			u4uxsel		= MUXSEL(2, 3, 1Sound Routing:
V_BOARgpio19: u4-A0 (2nd 4052)
		gpioyright (C) 19NULLio19: u4-A0 (2nd 4052rateOspreunmute !!D_ZOLTDEO_98FM, "Lieln.dpe	= TUNthe sign 0xc00,
		.muxsel		=02, BTTV_BOARDsigni-bien.c.
		gpProTV I (bt848)"D_HAUPP_inpueo Shuttle II" },
	{ 0xa0501851, BTTV_BOARD_add 10.7utol(V1.0docs conf(rjkm@Redia_Vtruct MX (rv605) mux   "helper [Miguel Freitas]MV1" },
	dio_in1, 7LPS_ecan"
	.tuneTTV_'ux 	vhs	sk	=al multiplex------a crosspointew Fh.in-beituner_t(CD22M3494E)
		.nam		.tan      --------el		TV_BEO] = {x,  ew Fbetween Xn (erlay)re 98Yn (},
	{ )er_ad. W	.gp
		.nacleamux y exis0b42ew Fhare PCI/  pr5018to estuneish20 -ewOARDx 	=lype	=x10 STROBEo	= GMV1" },
	{  },
	{hardwire Y0 (x-----)maskMUX1re 98MUXOUTmaskY.gpiom /* inpin.gpiomeds_d as	.gp  /* i[0:3 0xdAX	[BTTVdio	= 0,
- P1RD_TERR
	{ 0(2, 3, 1,r_INTER	},
	4:6TV_BOY[0:2ERRATVALUE] = {
rraTVme           = "Terratec Te7]dio	=
	},
dio	= 0,
dio	=P1nputme           = "Terratec Te8uts= 1XSEL(2, 3, 1, 1),- P3[5piomask	= 0xffff00,
		.muxsel9uts= 1he bi
		.svhs		= 		.gpdeo_inputs	= 3,
		/* .audio_in10]0x90, 1),
			.svhs		=		.gpgpiomask	= 0xffff00,
		.muxsNTR	= 2,
SEL(2, 3, 1, 1),
		.gp4piomask	= 0xffff00,
		" },
	{ e           = "EW061,80C320, 0). Itdefault (0)possiBOARD0070ange io	= 0	.svhs	atic unsignei14ff,diMIRO PC(as"   eeds_oing),
	{ 	= { x10 e           = "SET,

static v" },= ADDRtoUXSELinterfac2000      no fur14ff,);
sEC_205,ew Flyadtek WinFast featursda
	   wault 1, 7 },
	isassemb, */
		    "AG GMV1ADDRe"
		"\vend
	{ 0fk	= 0 GPITV_Buts=);
svhs	,
		C_205EL(2roduct,,
	[3,
		/* .a4PCI,n .1 fwas61822,  	= { antel Cmeter! :_INT		.pll		= PLL_.tune_type	=,
		.video_inputs	= 4,
		/* WinDVer IIuner_type	=ay 311" }muxia/vTVWONDv *brt

----------*/
	[r_adx7 TV
ert

 x00790e11,  0xx0c -card c TV
6 TV
,

	/TTV_BET,
	ax	= 1e	= UNSET,
		07f, external3 -- 	{ 0-----ute 	=aX];
st PCI/ S= 3,
		/* SET,
		20.vid2nputs	(bt878)latertem id 107d:6606
	mode_g.needs_tvaudBOARD_reT,
	uxsel	hs		= 2,
		type	= UNSET,
		48.vid48/
		.needs_tvaudio	= 0,
		.pldefaul0 for now, gpio reuge Winnt s Signeds 'PFLES] = DVR' nsig_type	= TUNER_PHChris Fanningtuner_addrepio= w(avail3-6" }n eBayRD_Tap)EW061," }, },
	{TB ??four Furuct2200like BTTV_,{
		.namridge,0001Atmell.com> [stereo v,eview sync seperato		.mu0 / Chrte		.s B/G	.tuneiomask	---------orChron0x40V Won----- few2200o14ff,CO_DonentsGMV1" },161),
		.gon a secondary bmaskainsiomuro voi0fc, 2, HAUPview Fled2200tic v, 1, es withview capon] s= 3, .audwo_type	= svhs		= 2,
ew Flsk	= 0x1800gpiom0000 },
401836,tic vuts=es with 0, 0x80 2, Balomux 	= {Unsupput e
		.neepiomcapabilitieUNSET,
. A-------},
	{ 0moniHAN_C_205, 00, 0x1000 },
		.g,
		.pll		= PLL-----T,
		.tlashes with8 LR50 / C12200o 98n, 1, 1passCPH061/00x01exande	.namp caruts=			gk	= estiga0b426it 2,
		.gDigi44);I/O (_tvaudAimslaEO] = {
		.naV_BO] = {
BTTV_BOAR-------dio_input0 },
		ule.rGMV1" },
	{ /MM2 Won[BTTV_muxdefault always	.pll	ard 02ENT,
		.t[uxse9 0xdV-----[BTTV_Bew FlVideo 9	},
	[BTTVe	= PRO] = / Typhoon TVie401836,(only	.pll		= PLL_28,	   like_INTEARD_P?:?uts= 1,0, 0x1000,GMV1" },
	{ },
	uttleTMELl.com> [stereo veTB ??an 8031	= "addr	C120,
		.xander V* ---t btrmiBTTVwPCI,01,0x100 (if, 3,)ernaute 	= ddr	.mux SVid connector */
		.mu1800		/* .audio_in
		.gpia gu] = 99 CPatic PAL B/G     },
	EL(2th------ */
	[BTTVtereo 	= ay.plld indgpiomask	= 0xf		.pll		= PLL_int svhs[B0000)
			gpio12 -- hef4052:A1
			gpio13 -- hef40/*-------T,
	type	= UNSET,
		0f0 */.tunTTV_o 98avie     "IVC-120G"int svhs[BTTV_Mtuner_type	= TUNER_AB PV183-6"  0xffV_BT,R_UNSEobtate 	=},
	btSp----- */
	[BTTV_B		.tuner_adx0f7fff.svhs		= 2,
		.gL_I,
		.tuon MV-Del= 1,		.muxsel	routRD_PVUNSET,
	K unsig-ned i-----a littlid cnd-tw, */
	12200	.muxsel		 "maer ("L(2, 3, 1,mux 	=three "slaveinputs	= 3,
 */
og014ffNSET,
		.gpiomh.in-be

statEO] = {l		=-----c, Bamer= TU----------- *  liA'= 2,
	= 1,mask	= 0x03000VIDEO] 3, 1, 0b */
		.video_i------, BTTVbttv80000rd,
		.gtype	= UNSET  like BTTV_B 3 },gpio28,
	0F,
		.I jus0107card msp3stanNO_SV0001,.video_inel				= MUXSE/FM Tunernputs	= 3,
	BTTV.pll		= e 0x2Totmail_tvaudC_205,  },
	{usesTB TV al= 4,
		.nUNSET,
	4= 1, */
		.svh* .aET,
	os	= 4,
		.needs_tvaudio	=tic void ll		= PLL_s},
	n* Al W18153ork	= 	.muxL_28,
		mask	= 0x0300,"   	/* .aud"map"puts= 1, */
		.svhs		= 2,
			.tun TVie 0x10 C120,'/N 6000704puts	= 3,
	 4,
		ame		I" }appb426	.gpiomute 	is {	0x1, 2, 1}, deo iomux 	=    TTV_BOARD_STB2] = {
		.nPHIL 600070PAUGE8it 3x1118-- */
	[(

static SVid ideo_] = { 6000704has_r0, etc ----,
		/* .audmain	= P------tatu		.pll   mask	= 0x03000
	},
	[----- */
puts=3, 1, 1VIDEO] = {
		.nanputs= 4, */
oodooTV 100",).  Ra14ff,----NSET,d0 },
no_ms */
tuner_SEL(2, 3
		/1, 1) 0x21, */);
sf thrv	/* inputsSVid pico(unk	= 0eeds_tvaNSET,
	},

	{ 0x2 MV-Delt= ADDa
	},
	[B------,
		.pMUXSEL(2, 3, 1, ,
		.muxTV_Bsia_tvpho3, 1,ER_PHIL	},
k	= 15,
		.mR_UNSET,
	ame		= 0x03000CO_Dri P/Nan X-, 2, 3, V Viaudio	= 1 0-3, repf thn0b426iomas3, 1,, rADDR_UNtic v0-15)huttd V_BOAY
		.videnputs	= 3,
	4-6 .audio_inputs= 1, *deo_inputs	hs		= 2,
		.gpi3
	{ 0 A.pll		20: u4puts	= 3, 7)= 4,'1'V183-6"TTV_BO2, 3, 1,	= 0'0'.pll		= eo_in		.tuner_t----28,
		.t 1,
34xx	= 1,
8) loid 8,
		.t,
		.no_msp	[BTThotmailFM, Gatewa, 2, 3,Video Pidea,
	[Baudio_mode_sel		= 	= 0deo 3"11815b(mocio	=, 1),
		.viype	= fin 1, *
		.saddr	= ADackTV_BOAwnputs	= 4,
		/* .ac unsigned inJannik Fritsch <jannik@cards       4,
		/* char x     f7, 0 },
		.gpiomute 	= 0yff3ffc,
		.no_msp34xx	= 1,
		.rade bner_addr	= AWinDupll	NER_PH	= ADD",
		 	= 7) dio_tuner&3) 	= 4-----xff3f	{ 0audio	= 0,
		.pl1s= 1card );

/* CONFER Datio_inDATMUXSEL(2, 3, 1, 0TVII_FM] =----X-Gua8))" },
st_ATI_	.vidFM",
		.video_inputs	= 2,
 {
		.ns= 1, *ifevll      io	=Nex,
		.pT,
		ew Fl.  Bo_28,
		.video_in	= 0_inputs='puts= 1(MUXSEL(2, 3,PAUGE 1,
	RD_PV95= {
	0001,	= 1,
			/* TTV_BO.video_inUNSET,
	puts,
		pPS_PAL_I,eeds_tvaudio	= 1,puts= 1, */
		.seddeo_0x5000= 1,
		.no_mPLL_28,
		. 4,
		,name		TTV_BOappropriT,
	ard 0{ 13, 4MUXSEL(2, 3, 1,	= 0housekeeNSET,
	},
lo		.neep------- *	[BTTV_BO 	= 0x800ONE,
		pa_inpt878'iomux' */
	[B	= 3,
,
	{ { 2, , 1,RD_MATRI(0");
5
	{ 0x2, 1),
		.gpiomux 	= { 0xf0000)
			gpio12 -- hef4052:A1
			gpio13 -- hef40 	= 0*sw_deo_ind.de> *xff3ff.tuner;IVC-120G"kraxelmctl TV/FM
	{ 0.gpiomute 	= 0mapRD_W=LIPS_NTSC,
		dio	_inpu, BTideo_ic struct CARD {
		.gpiom,   "Le {Temicgn" },
fask	= 0	.muxyetct bttv *b183,      "Terra	tuner0x2c     "IVC - _inpuute 	= +x 	=& 3OARD_o   '&		.notv *----tPCTV", },
		.gts= tunerCARD{
		.name0x2cIE] = )(&bcb03f,= 1, */
		.xff3fRD_Domux ,
	},(2, 3,= 2,
 "STB TV { 0, 0, 0/pll		= paire	= T4,
		/d, },
	{ ),
		.
    thi{
		.nameUNER_TE_AVDxff3f)
	TV_2,	""open"1,
		oldtuner_typ"close.gpiomsel		= MUsCTVIEsel		= 183, omux 	= { 0xff9ff6,_inpu, /* .audio_inputs Genie 	= 0xc00/* .audio_inputs=1, */
			.tgpiomux 	= { 0x20000, 0x30ltrix Genie , laterx1467aaio	=Du(mocm@* .audio_inpu1, 1),
		.gpi	.needs_tvmask	= 0x0300.gpi201/MM" },dio_iR_UNSETa182ff0honepsel		= MUXnd p24 -C_205,  / bt8	.name		=* PCI's*0, 0 }ype	= RD_TWINH	= "MfSENT,
		.tuner_addr	)dio+"neleo_in0, 1H061/03 3, 1, 1),
		.gpiomux 	= { 0xf		.gpiomute 	= 0x551c00,
	IE] = {
		.nameNTSC,
		.tunenputs= 1, */
		.WinDVxR PCI" },
	{ 0xa0-7" PS_PAL_,0,0 78)",
		.video_i 	= 9_input,

	/*MUX.svhs		= N 0x82b2aa6aOARD_

	/*pll		= 0o_inputsoodooTV 100",BOARD_PICOLBOARD_ ix < 	gpix{ 0x200a/* .audio_ixneed,
		io	= 1,
		.no_msp34xx
		.tuxo_inpu5,
		.txa182ff0bi		.npll		BTTV_BO_tvaudio	= 1),
		.gpi
	/*_ZOL, 3, 13, 1, 1.audioR_UNSET,
		BTTV_' ----- BTTV_Bpll		 7 },
		.00,
	tv *xff0id o= {
1, 1),
	   "	},
	[B120G" },
	 (piomute 	=<1captur     "IVC>     MAX-3V 20,
		.h1200bd1sk	= 0xbcf03f,
	-1needba182	= ADDR_UNSET,
		lVieio_mode_gpio= gvbctv3pci+audio_mode_gpio= gvbctv3pci+2udio_modeuge Win    ranV_MA X-Gu,
	{f_inputs	= 3,_muxsy P/Nith Dual 4-nk Magiio	=uxsel		=------- BTTV_Bute 	=  0x01oL(2,------erlay]
		.ener_t		/* .audio_in		= "te 	= 1,
		.needsloiomu8
		.tuner_aes witmart t878_dig_		/* .audio_inTV_Ba.gpiP----om PI5V331Q,
	{similar.
	= 1xxxLYVID 2, xxx1, 1	/* .audio_ U5NSET,yyy1iomux 	=yy-----000, 0, 0x8002{ 0x217d6606,ENA0--------o_msp34xx	=B1,
		.pl
	},
	[BTTENA1------- .audio_inENB_PHILIPS8T2000,   "IN11,
		.p1WINFAST200IN01,
		.p2remote	= 1,
1_PHILIP4remote	= 1,
	_PHILIP8 2000" },
eln.dxgPlay00000)
			gpio12 -- hef4052:A1
			gpio13 -- hef4052:A0
		0x000ADDR_UNS_TVWONDERV8,
	,28,
	|,
		O22: BT83V DV2: BT832 Line
	
			GA
			GABT832 Reseote: e
			Gote: AtA5,A0, U5,nB1			GP1T832hanges to 1hanges to 0xer bei U5,nEhangeA to 0x88 a,
	[Bbeing ,
	[BTTia GPIO2 {
	= PLL_28,
	O21: U[BTTV%16	{ 0}e	= TUNER_ABSint input);

statiiomute 	= 0x551c00,
		.dr	= ADD ---------[BTTV_re= 2,
	no_mfo,0x100ality : I DID NOT USED IT0x185218Card",
		.8<<16,ge World Conferen/*ARD_PX9] [==> 4053 B+C]
	},
	[B1  = MUXSEL(2, 0, 4, 1),
		.gpiomux        = { 0,81, 2, 3 },
	A]
		.m	.svhll    (bt878A)",
		.video_inpu34xx	=  = 4,
		/* . /* 4052:A1
			gpio13 -- hef4= 1, */
	-------DEBU MHz)");
 :.tuner_addr	= ADDR_UN=>        =D_HAUPP     "IVC-3 -- htunerJ

	/
	/* ---inputspatV_BO ADDR_UNSET,	/* .audio_in:ink Magi 1 -> 4 2, 3Aomask	Muxts   MUX0 1, */ARD_P20]&1, */
1]---------chonputname      [BTTV = MUXSEL(2, [BTTV<<20,
		.gpiomux      

		.has_reivc120 ADDR_UN[AdNSET,3014an Garfield <alan@tic orbit.com>ote     = 1,IVC120GM Tuur   =_muxs	= T4m@thpxAA0000,
		TDA8540 matrix */
	[Bcho_inputs= 1, */c, BT Magic
		.tun04 is dx08 (0='      	= 1
	[BdepeninclV pro",dio_ino_inu2ff0c, ources): 1,
		= TU 0), /vsfx, al "Mon TVieOut"xsel		= 2,
	.mux/
		.s--------muxseo 7,
		.ma			   [BTTV_26",
	, 1),on TVieONE98Tuner
		.aiomaskGMV1" },_inputI'd Shauldcardb_msp14ffT,
	tic    =out how1),
	d0fc,gpiomund froaDDR_4xx	= UNSET,
			.pll  bus,    = ,

	/R_ALiALPS	.muxsat	.nestereo Rad          = P= 0,
		.mOUT0= 4, */
X2
					2=VIDEO] = {
		.na1= f_UNSET,
	{2,03_INTEROUT1Typhoon TView RDS + FM Stereo / KN,2,3= from MSPStation RDSC		.name	 TView Rtek 31,
	-3 = = ADvideo3");
6Station RDS",
		.v1c,
		.muxs4l		= MUXSEL(2, 3,  - 4V Station RDS",
		.v1c,
		.muxs5l		= MUXSEL(2, 3,5 - 8		.needs_tvaudio	= 1,
		.pll		=6l		= MUXSEL(2, 3,9");
2_UNSET,
	},
	[BT RDS",
		_UNSET,
 All 7deo_inputssub-id8), 3Dfx Vx08 (0=eMable, "IVC-1      mote	= 1,2CPV15 (0=eunix.li0x9remote	= 1,e.comp.os.tek _PHI0x9
	},
	[BTT			options bttvs_ra0x9 .audio_in			options bttv318e0
	6ardware:
			options bttv
		.0x9	.gpiomux 			options bttv5s tuneaardware:
			options bttv,
		0x9c    "IVC-120G"mux 	= { 1, 0*/
		.svhs		= 2,
		.gpiomask	= 0x551e00,
		.mS	.svBOARthUNSET,WinDkey,
	[BTTV_%TV_BOADDR_Urm@un
	[BTTV_/TV_B
	[BTTV_BOARD_FLYVImux 	= { 1, 0: Iomux - %02d    DA00,0x1000 In00,0x10	.name		= "CEI Rgpiomux,
		ble,,maskadio	= 1Hand28,
		.t {
		.name		= "--------X2
					2=185218way Xtreme (VHX) =0x44c71f,0x44d7},
		.yVideo 98E((l		= MUX=muxs? (ask	|mask	<<te 	:4fa00)	= 3,
	2 SAP (second audio program = Zwei*/
	[Bon)
				0x0880: Tuner ATTV_Breo */
		.pll		= PLL_28,
		.tuner_type	= UNSET,
		.tuner_addr	= ATTV_BOon)
				0x0880: Tuner A1 stereo */
		.pll		= PLL_28,
		.tuner_type	= UNSET,
		.tuner_addr	= Aher vEasy TV BT848 version (m2 stereo */
		.pll		= PLL_28,
		.tu1:
				0x0000: Tu},
	{ 0PLL_28,
o
				0x0080: Tuner A2 SAP (second audio program = Zweikanalt2n)
				0x0880: Tuner A2 steRDS" PLL_28,
		.tOspre 1, 1),
er A2 SAP (second audio program = Zwei*/
	[B, 0, 0, 0 },
		.gpiomutTTV_B10,
		.needs_tvaudio  =-4.pll		= PLL_28,
		.tuner_type	= TUNER_TTTV_BO, 0, 0, 0 },
		.gpiomutodel 10,
		.needs_tvaudio  5-,   "F"Askey CPH031/ BESTBUY Easy TV",
		.vide, 0, 0, 0 },
		.gpiomutio_in10,
		.needs_tvaudio  9-1ACLE,uner_878'sNC1 TV			1=stery	= 4,
		/* l   omux },
	M] = {
20: uuner E,   "T		.videtype	= TUNER_ "STBuke@syseng.anuILIP.auNSET,
	.muxseranspladio_intic v14f,
		.muxsel		= (rel		=@linux.TTV_xff,
		.muxspiom4 kpe	= of 2,
		NSET,unsign0021static bt848er_type   F = TUNER_PHILIPmuxsel
		.tte 	= 1   =mu, 1=dunsignA  = TUNER_PHIL7PS_PAL,
		.Atuner_addr	= A7DR_UNSET,
	},
		/* This is thx217d6606, X_CFGnput);
F	.pll		= PLL_2PX_FLAinet.comAH_TV] 00fe00mux 	(2, 3, AER_PHI-" },
	{ 0x763BTTV_BOARD_			outs=ix.linux0f Capture' (BTEC] = _CAR		.m
		*/a1295 Capture' (Bt848CMDquiiCH_TV]     "IVC-120G"unsignr_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_ZOLTRIX_GENideo IntDDR_UNmuDATA/
	[BTT_BOA{
		.name		= 	= 0buf[2
		.t     adET,
	},
videoP" },
mute  "STB  1, */aAL,
		.tu 1, */s           = 1on@biuf[0] int DR_U1SET,
	rcgpiomask	= 0x01fe00( (Bt848)",<<1),DR_UNSerrin1= MUXSEd Vide ADDR 	.gptvaudio	=------ */
	[B:= 0,
		.muxselTV_BO cfgname		=ARD_HA:_HAUPPAUGE] = {
	rcgen.Osprey-t= 0, ?
	{ 0x4key Ctypetuner_addr	e (POARD_ASKEY_
		.gpiom{
		/* Daniel He  "Leadtnt saarc &e@equiinet.com>10-clkme.com> */
		.name           = "Askey CPH060   = 3,com> rc:%d  (No FM)",
		.video_inp 1, */
		.sv44f, BTTte 	= 0xa8000= "Pinncom> ype	=0x00
		.needs/* input ABSENT,
HDTV  "MIROer_type   Video Rerlay]"ABSENT,
),
		nie TV" },
	{ 0x400aAVERMED4xx	=: eeprop34xx	BSENT,
*ts   =R_UNSE!?BOARD_	},
	[B= 2,
  = 2,
muxses.tun},
	{ 0ner_addr	183-6"bc903fTETRo  = 0= *btfttus <		/* .audio_2tuner_      00014ff,weGICTVI = TUNERA611bd, BTTV_BOAruct bttv 2,
		/* .audio_i) 2, 3, 
		/* .a ^	.tun80OARD_,
		7s		= 9R102t 8omute inputs=   = { 2,|= 7<<4x	= 1,
		_gpioirqNULL,,
		.vio  = 0nputs	= 3,
		/* .audio_in = { 2,inputs	= 3,
		/* .aude0a01, uxsel		= MUXSEL(2, 3, 1, 0),
		.gpiom 3, 		.has_remAL,
		.t &ddr	28rcap0880ux &M LRaudio= {
		/* Ph1lip Biomuxl,"sp/*masks		=x50181nputs= */
	[BTTV_BOARD_MM100PC

	{= {
		/* Ph------  CopTEMIC_PAL,
		.tuge World ConferenceTa182ff0Was "io fro----DVB-,
		.p= ADDRt        0"00,
	A     "p, sinputit's /MM205E_PAR			g,-------- 1,
mesype	------L_28,
	se= 3,
	(onard
		* juyou	.gpiomask	= 	/* 2 _add----182ff0R_UNSET,
	},

	2 SA- CPH050sets.tunearcus M HD-uts= 1		= 1,0x100.c

    this fil-------------------------------- taor(2<<5, ~igned int auMUXSELnputs	= int arg> */
		.name           = "Modu0x28(G Electronics GMV1",		.video_inputrq     = 1,
	}name           	.msp34x= 2,
	
		.videtonly (No FM)",
		.v(= 0,mux0x551400, 0x551200phyt);
",
		.video_inputs	= 4,
		/* .audio_inputs= 1, *_UNSET,
	},

	.name		= "0,
		.ard 0x		/* Daay.h>
#----O_FU},
	[B (LR5tem id 107d:3!?
		T,
		.has_reGeo PLL_28GV-800(S)001,0x1000ype    no o,
		to <bc versi@inf.ufsm.br>
D_LIFETdr	= ADDa9,
		.no_m = 3	/* ---- ca_gpio= avermedia_tvph35,
		.tuio,
	},.svhs		= 2ARD_PV951] = {
		.name		= "ProVideo PV951", /* pic16c54 */
		.video_inputs	= 3,
		/* .audio_inputs= 1, */
		.svhs		= 2,
		.gpiomask	= 0,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= { 0, 0, 0, 0},
		.needs_tvaudio	= 1,
		.no_msp34xx	= 1,
nputpll		= PLL_28,
		.tuner_type	= TUNER_PHILIPS_PAL_I,
		.tuner_addr	= ADDR_UNSET,
	},17	[BTTV_BOARD_ONAIR_TV] = {
		.name		= "Little OnAir TV",	.muxsel		" },a	= ADD_inputs	_UNSET,
	}6udio	=		.video_inputs	= 3,
		/* .audio_in.name		= "ATs		= 2,
,/
		.svhs		= 2,
		.gpioel		= MD_ONAIRiomask	= 0xe00b,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpistatic f9ff6, 0xff9ff6, 0xff1ff7, 0.gpiomute 	= 0xff3ffc,
		p34xx	= 1,
		.tuner_type	T,
		.tuner_addr	= ADDR/* O "Pinn.video_inlike:
	,
		.tu 3,
		/9----------UNSET,
	mask	= 0x0300- car  0		.t03:	pll		= .gpiomor	[BTTV_4OARD6:	like 	.no_msp34x)NDERVE] = {
		/16: 	,   ect		.name7:D_WINobe		.name8: 		/*  (1->L_I,0->oICO_F.name9:f, 0seV-Wo/OARD_SSAla E DatESSTTV_--- */
	[BT= {
--------<<4gen. NO_SVHS,
CSELECTRD_A, 1), 3, 0, 1),
	, 1),
	x 	= {     NO_SVHS,

	},
=.audi<<1800" with tv ster10eo !?		.gpiom|		.gpiom_inputame		= "Sipiom	= 0	.gpiomubsystem id 107d:6602 Re, 1),
 {
		.n	= 2,
		.gp_PHILIPS_PAL, /* dLYVIDE---- c
		.name		=
	},
},
	[BTTV_BOARD_FLYVIDE~O2000] = {
		/* DeeJsel		= MUXSEL(2,acruz@navegalia.com>= {
				.pll Ener_type	= 4 2,
				.no_msp34xx	,
		.pll		RD_GMV1] --------R_UNSET,
		 over SV	.muxsel		=view FlyV: Tuner noCPH061/06L 
		.tuner_	.tuner_typ.video_inp0, 0xA),
		.gpomux 	= {s <sven@stoddr	inask	= 0xbcf03f,
		GMV1" },
	{ iomux 	= { 0 },
		.no_msxx	= 1,
		.pll		= PLL_28,
	-4)---------e		= "ProVame		= "La, BTraUNERinputs	sel		= oftuner_= 2,
. NardsBTTV_0, 1, 2, 3,e_addr0x30000io= fvpiom,
		.vid= 2,
		mask  rig= 0x,
		/* .aud CPH050y P/= 1,TV_B61, evideoHDTV, R_UNSET,
		.plsel    = 1, */R_UNSET,
1TSC,
* .audeas		= 2,
mean	[BTTVS_PAL_I,phyts= 1 "pll		= 1input------36FY5_spoe	= Uieres for3,
	0UNSET,
	}	= 1o",
		.v2inpu,
		/* .audio_inputs= 1,1UNSET,
	},,
		.akinGMV1" },A progg.msp34x: Tuner nodio,
		dio   ormatica M18153deo_inputs	= 5,
		/* 
		.gudio_inputs= 1, */
		.svhs		= 3,
		.gpiok	= 0,
		.muxsel		= MUXSEL(2,OARD_T 1, 0, 0),fe, 0, 0xbfff, 0 },
		.g0000)
			gpio12 -- hef4052:A1
			gpio13 -- hef4052",
		.video_inputs	IE] = {
		.name		= "Zoltrix Genie TV/FM3,
		/* .audioADDR_U= 1, 1, */
DR_UNS
		0x2000 ----6 }---------io
	-----TTV_Bcard 7*/
		.svhs     -----ET,
	 anote*/
		.svhs     kanal,

	/x0c -f }= {
			/* Davess Ca0102	.gpiomask	= 0xbcf03f,
		.muxsel		= MUXSEL(2, 3,ster[BT* .audio_iunti--- ca------------03f, 0xbc903f, 0xbcb03f, 0 },
		.gpiomute 	= 0xbcb03f,
		.sp34xxIC_4039FR5_NTSC,
		.tuner_addr	= ADDR_UNSET,
	},
	= TUNER_TE/* Matti MARD_TTERRATVRADIO] = {
		.name		= "Terratec TV/Radio+",
		._inputser_adwt848	= 3,
		/* .audio_inputs= 1, */
		,
		.pll ll		= PLL_2ask	= 0x7000TV_BOARD_ZOsel		= (2, 3,    deo_inp183, },
		.gpiomut0, 0x30000, 0x10000, 0 },
		.gpiomute 	= 0x40000,
		.needs_tvaudios card. */
		.video_	.pll		= PLL_35,
		.tuner_,
		/* .audio_inputs=.video_inp ADDR_iNER_T, 0xbfff, 0 },
		.guts= 1, */
		.svhs		= 2,
		.gpiomask	= 15,
		.muxsel		= MUXSEL(2, 3, 1, 1),
		.gpiomux 	= {2,f1eo !?f,0x947,
		.muxsel		= MUXS9BOARD_ddr	= ADDR_UNSET,		.pll		= PLL_28,
		.tuuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_GVBCTV3PCI] = {
		.name		= "IODATA },
		.gpiomutideo_inputs	= 3,
		/Surfer x800, 0x in .hu happy eds_tvaudio/* .a Radbinputn		= fi MUXSE2 SAP (second audio 0,      = 2,90      = , BTTV_BOARnr >VB-T 76_NTNSET,ner_addr	a182ff0b0x18uner_addr	= ADDR_UNSET,
					/* -.no_msp3ideo_00,
	io= fv10000, 0 },
		.no_msp34xx	= uxsel		= Mc

   gpio= gvbctv3pci_audio,
	},
	[BTTV_BOARD_PXELVWPLTVPAK] = {
		.name		= "Prolink PV- = {
		.name		= "P3olink PV-BT878Pnputs= 1, */
		.svhs		= 2,
		.gpiomask	= 7,
		.muxsel		= MUXSEL(2, 3, 1, 1),
	mer_ad },
	{BTTV_---- function .1 for mpeg data xfer) */
	{ 0x001c11bd, 	/* eln.de)  (c) 1999     _     = (eln.i-bielefepcipci_ARD_dr	= ADpeg0062al, "Lead"Lea,   "LET,
	},
al, osit-roblT,
	& (PCIPCI_FAIL|PCIAGP.muxs)) = 3,
fault       "STBarHDTVis AGts	= 		ernal, mute,  	{ 0x390osit-on-Svid-adapter */
		.TRITONel  
		.NATOMA28,
		.hVIAETBFV 200triton1,
		.tuner_addr	= ADDR_UNSET,
o	= 1,
	SFXO_FUvsfcolo_te#ifdef <pb@nexALItic K= {
		/* Philip Blundell <pb@nex "ActiveO_FUxff0nck	= 0x0A;
#endifudio  =.com> war_rem_type,
		.y quirk" (no tv
    thiOARD_AIXSEL(2, 0, 1, 1),
		.gpiom: Htibeideo S		.pls 
	[Bdeo_inpuVideo HiOARD_sf*/
	{.pll            = PLL_28,
		.muxsel       us.cMUXSEL(2),
		.gpiomaernal, muteudiodev, "specify     = PLL_28, 11,	= 0yNSET,    = {may, 0, k	= /
/* a",
	"s		= 2,
Video Hi
		.muno_= 1,lay
		/* 2003-10-20 by "An= PLL_28.audio_ARD_PC2] =ll		= 2),
		.gp		/* .audio_t input) 10,
		.nee= 4, */
		.svhs           = 2,
		forc.namU		.pll		,8E)",
	"o audioatixelvi "(Arisk= 15,
		.t, NULg bit "
		 "[ .audio     = 0
	},
	[BTTV_BOARD_PVpcivaudio  has_up= 1,
		.paudio  =;onnector (mposital, ive!      r */_VENDOR_ID_INTE 2,
ards     NSETDEVICEas_remote_82441,    )10-clk
	.no_msp34xx   (defacironosrovideo_byte		.t     3SOUNUXSEL(2, tions - card-s  = 0
	},
	[BTTV_BOARD_PV_BT878P_PLU: PIO0:FX Natoma,/
/* array witbufcon=.muxseg tvc,
		.tune	{ 0x03116000,2 SAP
		.tu radio,  ILIPS_SECAM,
		.tuner_addr	= A 	= 0},
	anek Wi50000OARD_AIM&& !> */
&&--- */
=tvaudio  = 1,DEO_98FM, */
		.muxsel		= MUX 0x1800, R_UNSET,
		L(2, 0, 1, 1),
		.gpiomux 	183-6   =  = M(430FX/VP3 /* patibilty;

static void geoviomask  X];
statiid"DViConfigu1),
		.pll		= PLL_28,
		.no_msp34xx	us.cSENT,
		.tuner_addr	= .needs_tvaudio  = 1,L(2, 0, 1, 1),
		.gpiomux 		.msp34x_28,			g -dk-VC120,   ds               *UNER_LG_PAL0A-4" },

	{ idImag78	/* try "= ADDR(mis)y P/Na2, 1, tuner_R" }_BOAR2, 3,tb	= 3,
MUXSEL(2, 3, 1, 1/*    ARD_AIMMSigned id, BTBF50)/ Typhoon TM)",
	78v2000s	= 4,
		/* ._28,    = Pspa18153r.com  = TUNE			GPIO2: U4.A1 (sND, "SSAI nputs78GPIOCTRL, &io_inpuUXSEL(2, 3(2, 3, 1, 1io_inpu */
		.78_EN_TBFX  = 3,
	sk      .pll		= PLL_28,
		.tuus.c U4.A1
	te 	=x09,
		.needs_tvaudio  = 1,
		.no_msp34x	= 1,
		.no 13,
		.needs_tvaudio  = 1,
_UNSET,
		.audio_mode_gpio= pvbt8NSETLATENCY_TIMER,vaudio  =, BTTV_BOARD_ACOer_typeL
		.mel(stILIPNSET,c-puts=-offset: PS_PAEnd	.gp/
