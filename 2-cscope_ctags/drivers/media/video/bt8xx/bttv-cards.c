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
	btwrite((1<<7), 0x058ay fudelay(10 Copor the m  Ral0rt
info Co}

/* initializastuf part one -- before registering i2c busMetzvoid __devthp. stufs  (c_card1(strucc) 199 *btv)
{
	switchmatirds atype) {
	case BTTV_BOARD_HAUPPAUGE:matiprogram rlinree softwar878:
		boot_msp34xxrlin,5 Cop	break;; you can redistriVOODOOTV_200e; you can redistri Public LFMor modifyetzleit unde2C<kra terms of,97,9GNU GeneralAVERMEDIA9/ee Software Foundatio11 either versln.d2rsion 2 Licibute it aPVRee Spvr_oftwrlinater version.

    This progrTWINHAN_DSTnse as published byAVDVBT_771nse as published byPINNACLESAUT A	lin.duse_i2c_hw = 1ter version.

    This progrnDLINK_RTV24RCHA-2001einfo( btv ater version
	}nforma! 1999-v Gers[lin.de>Metz].has_dvbrd-s 1999reset_audio hope t(rjkm@  (cuni-koelouldfigutwo & MafterMetzler (mocic License
    alongtzle( <krax9 Gene  Yo2 Knorr <kraxel@goldbaclin.dtuner_Metzl= UNSET informaan redistriUNKNOWN ==  thuld have  Thicopyrsioadeedatioeeprom_data,0xa;ater identify_by_clude >
#innux/in<linu Cop}
ach.in-bershould havee <liyou can redistriMIROnse as published by
<liPde <x/firmware.hit.h>/modinuxnet/checksum.h>
t.h>
#inclue <net	/* miro/pinnacleMetzl	t.h>_ux/in"bt_gpthe Lice,97,ersion.

    This progrFLYVIDEO_oftwa/checksum.h>

#icAXInux/in"bttv-GNU G-hLIFEGNU KIERCHNY WARRANTY; witNU G.h>
h"

/* fwd decl */
TYPHOON_TVIEWh"

/* fwd decl */
CHRONOS_VS2 odify
    it Knorrt fwd  *_98Fribuatic void avermedia_eepr20cense as p   tshed byt bttv *98EZauge_clude avermediaONFERENCETVh"rjkm@fwd decl

  statTEC_9415/io.flyvideodia/v4l2-commonaudio-ho#inclume initvabute it anse as published bybuteoundand  (alinux/vmalloc.h>
#inclt bttv *bt/* pick up some configSA.

istrom,97,9tv *bttvp.h"ux/modul-audio-ho#include <x/kmod.h>
haupponstned ithp..hater version.

    This progrnse, or Softwaot_msp34xx(struchoPHONEt inpt struct bttv *btv, unsigned int inputaver);
stoeln.dxguard_muxselavermedia_eeprbtv, unPXC unsig the Lunsigngvc110000_muxsel(struct bttv *btv,ICOLO_TETRA_CHIP *btdicolo_tetra  Fougvc1100_muxsel(struct bttv *btv, VHX GNUNTABIeceiraU GeRalNESS FOR *btv, smatchboxt input);t btmbox_we intp = 0x20ut);
_thp. Knomostt bttv *

static voidclkct bttv *08muxsel(struct de <t bttv *1);
t btticoeln.da
   *btv *3t);
edr version 2 of the Lice PBIS_BOOSTAv *b, int pinra_initiERRA Soft	terratec_activev *btv_upgradh>

#200(struct bttv *btv);
staticMAGIC);
st06,97,9ormatifil  Yoiright0x3002144fclude tv *btv, v *btv=nput)specific stufude  *btv detectruct  subsystem id (CPH05x)g tv Gers arr) 1996e_muid tibetCS16_inikoint mSTBa_inC00_muxsel(struct bttv *60121a(struci/* Fixv *btntr 1996,3DFX VoodooTV 100,oeln   whichedisan OEM STB   Yo variant.muxselt bto_tetra_init00_mu, Cambridge, MA =TUNER_TEMIC_NTSCgeovin.

00_muxsel(struct bttv *OSPREY1xsprey_eeprom(struct put);
sta_84tv, unsigned CS16_input);
s01ttv *bstatic intt bttea5757_rex,97,9implied woid nt *btv);
s_SVIDtatic i the mvermedia_eep2xxt valuestatic int tea #incl0vuct bt_by_eeprom(struct bttv *batic_ignedkodicomtatic voi44m_data[256]);
static int _5osprey_eeprom(struct edia_eedevthp. d inodifatic void id osprexsel(struct bttv *btv, v *btvtv);
o_ten1;
sgned i xguar> eagle_include <on_muxsel(struct bttv *btIDS_EAG <asm the Lids_SET;4400ratic voidtruct bttv *bid tibetODtruc20a_iniatic unsigned int vsfx;
static unsignd in);
 void gvc1100_muxsel(struct bttv *btv, LMLBT  of the Llmlbt4ndatio thatoundwill be useful,inux/IBET_CS16ic unsbetx;
stibetCS16_inipnt input);
00_muxsel(strKODICOM_[ 0 v *btttv *bt };
rBTTV_MAX]   = { [ 0 ... (BTTV_MAX-1) ]GEOVISION_GV800S[BTTgv800sBTTV_MAX]   = { [ 0 ..x/pc *btll unsignur    alinux/etaixsel(se ==848 &&y.h>

ret bttv==0x11)clude /* defaultic int 00_mulistmuxselnt sPLL_28ight.h.MetzleYou should have rpll
static unsigllt inpifreq=2863t bt*btv, unsitic intucrygned=BT848_IFORM_XT00_mu bttX-1) ] =35ULLned NSET;
stx;
static unautic ad02139, Uigned int audiom3546895 *btv, unsi4 ] = UNSET };

/* iux[5] = dinput) bttlic Lsmod op   as publoverridBTTV_MAi.h>
#inpllsigned inr]clude you c0: /* n	nux/t bttv *bt4 ] = UNSET };
S1600_mu4 ] = UNSET };

/* tnsig00_muatency,e Fountole_pint, 0444);
r versio   in1
ruct28 MHzaram(lyou c2  (at_pload,gpiomaskaudioall,tic unule_para... 4 ] = UN(GNU Gallaudioallmodule_paraml,   int,mon1,    int, 0le_p   int, NULLuct 2tuner35= { [ 0 audioal3igned);

module_param_array(cnaram(vs 1999

  ruct b_pamodule_parram_oid g(p_paramudioall,NULLm_array(tuner,CS16_il,   int,}mus Me  int, NULL, urrent = -1e So/* 
statigned int audio(= { [ 0 ... (BT/ = { dia_eep/ay(remo(vsfx, )1,    ins39, Uhasnsigned int audiomux[5] = {
static vord-sg bit "
		LL };>
#96,9riton1 + onput);
static vo021enable bug compatibilit 1996,9DESC(v;nputers]");
 "[
statno_int,lay, tv *btv,SFX pci confi
MODULE_PARM_DESSA.

*/
tiodul"pci l;
mod= d statABSENTrd-id sigmaKERN_INFO SQ_muxsel(omux,absent

stDULE_Pinid geoelse ifESC( Ger,"id sigy TVt "
	d Sofel, see CARWARNINGT588[BT996,a lisi conunsetimer");
MODULECint, "sone, 28=28 MHz,DLISMHz)");
MODULE_PS16_i%dimer");
MODULE_PApdia_eener,"specify invsSA.

*/
fy iloa 	 "[l (0=nlude , 28=m(auto, 35=35auto)"); timheify i");
MBF pci intobsolete.uner,"specify instev, lled crysGNU Ge */
ce:\n"
useno"\t\t inux/nt itda7432 96,9vof th to0 = = { t bttv (TTV_MAX)400\		"\t\t 1 = 
    00\    int,  tibet 3 = truct bsigned ibe400\d udio");
MODULE_PAR[BTTmon.s]e:\n"
	 do not use anym pciet VSFX pci confi/grabber care Sofy indiginputkaround]");ye;

somon.hc unsvig_in ?on, plsigned int audiomux[5] = {uct btt_tets - 1 :1t "
	SAlt (0 svhsables, 1 enables)"
		" [some ------= NO_SVHSipsets MODUL(a :------t of card IDs for bt878+ w workat ofLE_PARM_DESecify inauARM_DES"t of caNULL, c struct ards   remotetic struct C cardnr;
	char *na audx;
st =t audiomux[d;
	c unsiRM_DESC444)tic unsigned int aut uns *btv	char *name;
}n1,   input 0x13eb0070,ogram is free softwar87x;
st"Hauppauge Winy deds[]	{ 0x390ils
static unsigned int autnoveritirq cardnr;
	"H;
st},
 0x45000070, BTTV_BOARD_HAUPPAUGE8volu);
stav cardnr;
0 NULL,"Ospables, 1 enables)"
		" [some },
	{ 0xff0{ 0xff000070, BTTV_BOARD_OSPREY1xof th_ram(0xff0rey-100" D_ratec_5nt i  ables, 1 enables)"
		" [some -500" },
	{ 0xf"specify ininstalled crystae overlay denoneroeln.d Sofu; ifULE_Pocan latrucdrision to ");
M*/00" },
	{ 0x unssaa6588 ||    "O44LE_PARM_DESC    i/* Probe_eeprRDSPAUGE BTT chipay(tuneic in unsstfy i *btvTTV_rt addrs[] =n1,    UNS >> 1verla0x22I TV WondeI2C_CLIENT_ENDBTTVPARM, Inc.,t_PX_subdev *sde So	sd =T20nt ior new 0xf"Le(&.h>

#iTV 20"Leaerla7107dTI TMODUdap, ",

	{ 0"dtek WinFast 0,D_ATI_200(st
	{ 0xff0

	{ 00= (sARM_D;
moBTTV "Hat, 0ryam i(set flof th/fad" },

	{se
  },
2First unali ifa6588user id sigmeda6588, S mo,

	{via aprey-, 
ts a_overledia_pci.h>
#inN 600devx00011002, BTTV_you c-maSLC,     "er[BTo notis freany NDERsprey-, d freSarray(tunerfy inset fl_muxse);
MODmoodule_paam is fT },
PCI FM, Gateway at wBTTV_MAX p0x660TVWO2S modander" },
	{ 0x00031002BTTV_BOARD_HAa Co. GVNDERVEx660ADDR_MSPTV4/P},
	
	{ 0x660 Data Co. GV_ALT },
	5/PCI" },6ASTVC10ram is f cardnr;
dare FounstD_STB0prey},
	{ 0x660FASTVC10ram is freWINFASTVCint i0x66adte-BCTV4/st VTV	{ 0x0/*001211bd9ASTV_muxsel(s1810-BCTV5over",     "i		got0x45 of thadtek Tidge 0xf05010fcV_BOARD_PINNAGV... 4PCIle_pa"I-O
	{ 0xd0181032(saa65CI	{ 0x00121407innacle PCTV [bswap]" },5	{ 0xff00bd11, BTTDAsaa65 [bswap]" },
	/* this seems to hnt sI" },

	{ 0x001211bd, BTTV_BOARD_PINNACLEE,      "Pinnacle PCD_PINNA	/* _ Public L ship wit/
	{ 0x1200bd11V_BOARD_PINNAy of
    0xf3  "Pude "bt PCTV [bswap]	{ 0x00121ff	{ 0x263710b4, BTT RDS mo_muxselped IDCTV3PTB2PCI" },

	{ 0x001211bd, BTTV_BOARD_PINNACLE,      "Pinnacle PCdooTV 1c Licy Magic/o\n"
 RDS m_GVBCT()th byteooTVfx Voo inpu6x 200" },
	{ 0x263710b4, BTTV_BOARD_ram MAXned iDESC(saa6588, "if 1, then lonnknown/     acle_by_e!

st
	{ set VDULE_PARM },
	{ 0xekD_STBThere wam ino 0 .97,9ss, s0x45
	{ 0000 XP" isc    irmatiohrougha65810b4,00_mudef Liceon1,   },
	{ 0xd01810 ... 4/Pf	{ 0x
		is70, BTT carda Co. Gwheth{ 0x00not(T1/Lit really int  -BCTV4/74soD_MA-1) ],     "O "L3000na6588inuxc (T1/Lfound intGIChaup,   thnot,allep(e.g. a
sta6300)edia_e5000070, BTTV_BOARD_OSPREY1xEPVR, sDULE_*/signed BOARD_MAGICT5/PLE,    	{ 0x4071211bTV_BOARD_PINNA_BOARD_VOODO 0x3FM,   "3Dfx Vme cardsned intard	{ 0r, 	{ 0x001S(0bd11, BTTV_BOAR ...  0xa0}aer ( x39000070, BTTV_BOARD_HAUPPAUGre Foun_altia TVPhone98 },
	{ 0x40702146263710b4, BTTVnse, or
  8	{ 0xAV;
staticTVCapture VCapture 98" },3	{ 0x03001461, BTTVvc120AVERMEMEDIA98M;
stacle PCTVa TVCa4fV_BOIW061,OEBE_T1, "TVixx,a658n we're d	   x40701146f, BTTV_AVerMedi
	{0x1200bdnflic t might also (0)a,   "AVeOARD_TERRA0x1117153b, BTTV_BOARD_TERRATVD_PINNAVerMed TVCapture 98" },
oodooTV FM" },/*iew99 seemTTV_Bhf, Bny_eewell "Terratec	{ 0xOARD_AVERMEDIA98,   "AVermedia TVCapture 98" },
	{ 0x
	{ 0xfD_TE21aV_BOARD_PINNA Public Liceinna3Dfx unsigned 2

	{ 0x00121e (PhilNow se (Temwned idfind	e_pao800sTVdooTV 10am is sedia_eV_BOARD_TERRAT100/v800sOEMDIO,  "Terratec 4 (Phil01461, BTTS,  "PhW063, "(Askeers) TVieion.") TView99BOARD_M	{ 0x00121a05V+ (V1.10)"    },tic v TValue (m is fre:
tv, uns2)" },
	{ 0x113515haupp     0te:\n",1135lyVvoid.c

{AL B/ YouATVALUE,   "Te5rate
ublic License
evoid  a liso the Free Software
    Found} 0xf, Inc., 675 Mass Ave,inD_GVBC = 
	{ 0 ------.

*/
by_e Rgned]");g bit "
		 "[yet another chipsetatec    ut); TV0015b BTTV_BOARD_HAZOLTRIX_GENIE, "Z000" },
	{ 0xff040070, B!TV_BOARD_OSPREYec TipsInc.,
statisetup.10)400a15d11, /* L);
MBTTV_BARD_GVBarcus Missunot, a lisBTTV_B call! },
	{ -1)00070, BTTV_BOARD_HAUPPAUGE878,VERM/PVR	1123153b, BTTV_BOARD_TERRATVRADIO,  "Terratec TV Radio+" },
	{statc LicDIO,  Terrat,CI" },

	{TV_BOARD_Zs ?? *S_RADIOalue Pinn16TV" },
	{ 0x400a15b0, BTTV_BOARD_Zoltrixthe ie },
//

	{ DIO,  "Terr1430aab, BRMEDIA98,   V143dia TVCTVCaprDEMODo _PV14A	{ 0x0012114312aa00, BTTV_BOARD_PV143,         "Po voido PV14BC" },
	{ 0x1422aa00, BTTV_BOARD_PV143,         "TV_WITH"Provideo t bt, BTTV_B.},
	{c voi= T_ANALOG_TV | T_DIGITAL_TV 0x2	{ 0x160aaint input(card,"specifeo PV50A-1	{ 0ix Geni0a15b0, _BOARD_PV143,         "Provideo PV143B" } (Te150A-1" },a00, BTTV_|s frrovidd11, BTtv is f_CTVI02139{ 0x1, sic voARD_Z, &150A-1" }eadtek Tion.4633aada9887_unsiPinn0dTV" },
TV 20privD_PVa04, ig,   464aa0fgO,  "TeRD_PVB-1"chipsect b int,TDAV150eo PV50A-1"5aa05     = FASTVC0A-1"50B-4," },
	{ 0x163aa030, BTTV_BOARD_PV5unsignVCap 0x1465aa0eadtekPublic-7V150,         "Pr},
	{ 0     "Provideo PV465a4DIO,  "Terra1320x300, B... 3D_PV150   al_ZOLTRIX_G
stas freTsignTVALUER },
Tf( strncmp(&(v, unsigned[0x1e]),"Temic 4066 FY5",14) ==0VerMedia TVtibetCS16_inistaticatic (TeFY5_PAL_IPARM_DESC(sSQ_muxselM},
	{: TULE_PtData Co. tv *btv, uns: %sTTV_BOA ple prodo nLL, 0&3b, BT     "PIVC-2TVCapture 98"  is freIVC  "IVC-200"  },
	{00Alps TSBB51146263710b4, BTTVRD_IVC200,        "ALPS_{ 0x0xa13	{ 0x, BTTV_BOARD_GRD_IVC200,        "IVC-" tec TValue },
	{PV150,        TTV_BOARD_IVC200,        "IVC-200G" },
int ia1550103, BTTV_BOARD_IVC200,   ec Tam iFM1246{ 0xa1550101, BTTV_BOARD_IVC200,     PHILIPS(BTTV_MAX0xa182ff00, BTTV_BOARD_IVC120,        "IVC-200G" },
	{ 0xa1550103, BTTV_BOARD_IVC200,        "IVC-200G" },
	{iodfault)\n103, BTTV_BOARD_IUBOAR"(A     St not	{ 0xa1550103, BTTV_BOARD_IVC200,        "IVC-200G" 467, BTTV "Pinnee Softwareprom(struct btxg, Inc., 675 Mass Ave,hV" },
	vSET;
imtvO,  { 0x0012_static intanalogd, BTTV_or cli* LR1&.. 4 ] = t gpiE_PAR
moduld,"specify y vV Wow99 flaw wint vsfx;
staticC-200Radio" },O,  d sigmaSQ_muxselHigned int,
	{ 0xindicate070, el#d, "ob        LL, 0C-20s freC120,/*.c

 Soveresvoid 878 boc unsnt auduplxa182TV_BOIDs. S.h>
#i6588C120,8ff00i conb proclais fre #.82ff0 int(20G	{ 0xt of649010)"   d sigmaSQ_muxselxa182fnot,BTTV_ "IVC- int %10)" PV150,   -200"  },
	 "IVC-BTTV_BOARD_AVPHONE98,    amned i },
	1 0xa182fan redistribute it a_IMPACTVCB0ER,  th by0xa18d have
moduTV_BOARD_IVC120,   (Temi01146IVC-200"  },
	Terrbtv);
static int teabttv *bt44"  },
	{ 0xa1550000, Berrt);
"I-O D(0ame;
}static unsigntigned int]   = { [ 0 ...tatic voidtruct bttv *E-87X] = { [84" }nt snt, 0UNSET tatic unsigned int vsfx;
st784" },
	{ static int 0" },

	{ 0xa414gned int tric784,     gned iowram(lat1 <<  Tec Multi MATEior Grand X-Guar9[bswapRANDTEC,sel51, BTTV_BOE-87
	VCE-888000/62.5;
	static i,97,98 21395 *IVCE8 +truct  CspreTVIEW-878ed8config bibtv);, ==)" }ttl   "leDULE_duleG" },
	{ SQ_muxselTo_tetra Aic intRn1,  U" },
	153b, to us50103, BTTV_BOARD_ITTV_BOARD_m isCE-8784" },
	{ static intARD_WINFbtv, unsigned 0003, BTTV_BOARD_IBOARD_IVC12Mail A
	{ 0x0	{ 0x8500_muxsel(lyVideo 98FM (LRmux}
53b0xa1RD_FVOODO"T, BTTV_BOARD_PV150,         "Provideo PV150B-4" },

	{ 0xa132ff00, BTTam is /*
 h>
#nimal oftwstrap is frhe 100TV/PVR thiup2innacltera0x11cksum.
 iew F0xa1hcwamc.rbfERRATVAG,
	/l },
    101184" },
	{ ERRATV0CD.  Have" },a lookPCI,Pvr/pvr45xxx.EXE (self-extrned ng zip archive      b201/Munpackite(ue" unzip)GMV1/
# Tunee PVR_GPIO_DELAY		10
217dGVBC      ALT_DATAder/0ATV,,
	{0001,acle PCC 10CLKst T1ATV, " },
	{ 0xfff6f6ffN"FlyIG	0xrono00a1550103, BTTee Softwarover      _");
	{ 0xf0500{ 0xa15,x407010f8 *micro   "Pro bttu32 Tunerlen Ave,   "{ 0xu8s, 1" },
 uns(LR5"Osp_inout({ 0xca1a,xfff6f6ff,nFa|xfff6f6fER,  b0, BTnie T6f6ff,(TemE,   ,97,98; eityrL B/G_BOARD_PINNACLC120, },
	{ 082bFaceTV_BC1100Tvmax" 0xa182ff00, BSIMUS_GVCaticstru(n  "IVCn <RD_PINNAC; n++VerMediitt ofTuner[n");
NULL, 0s fr ; i < 8	{ 0(Tem200a0, "SI12951,    	{ 00xa1,od.h>
 (LR10295,"ProviTuner003C" },   "Pinn11155ATA},
	{ 0x400a15am is alled tProlink Pixelv1" }PV-B BTT  "Lifeolink Pixelview PV-B4H050,

	{ 0x0105am is V_BT8>> input)44);
meo Tuner" },

	{ 0x01051  "Lia0c, BTTV_BOARD_PV951,    /* begin VC120Softwarloop (Not necessary,but doesn't hurtnt inp_IVC200,       ma3     ion PXC20lo Tetra Chip #1" },
	{ 0x01Oeo Tidge21461, BTTV_10518050, BTTV_BOARD_Pnt in Tt);
 Chip #0,   COLO_TETh>
#98ARD_"Li

	{ 0x001210311o" }
	{ 0TVIEW, Inc., 675 Mass Ave,407010D_IVC120   "Life*fw_mediausOARDD      rc_TVW_quest#incTV_BO(& Securit, "_TETR114c7",B-2	{ 0c.pci->deE_PAR Thiuc  "P50103, BTTV120cify audio device:\n"
02 *auge WinTV/_BOA[5409hotplug]"F*/
	{ 0 v, cARD_51)/ CyberTTV_BOA Intdio.haceRITY, "SSABOARENSensor BTTV_BO->TVPhonMaxi },
Visiz{ 0x0uner type");
MODULE_PARM_Did1071805, void swapMV1PV150,  03, BTTV_BOARD_IV
	{ 0<nter? "failed" : "ok"VC-120RD_IV0x5353414a BTTV_BOVC-120G" },V_BO467aa0ew FlyVideo 98 LR50 Rev Q" },
	{ 0x18501f7f, BTTV_BOARD_FLYVIDEO_98,   "Lif/*1,    ,    inARD_GVBaSQ_muxOARD_IVeg linu xfer)alue (Tem001cRD_AVERMEDIA9erface" },
120G" },
	{ 0xa,    in0,     ORAY3x26320, emot ay 310,  ee[256]V_BOARD_IiTETR32 seriay(tuner,ideo      "Ite,   iaudT, BTco prey1)"nevery actulue R},

" V_BOAi_205{ 0xase..."  },
	{ 0xa155501857844IVC-200 },
Egned id.10)"    V"},
153b, Tunen antiqux001 "PicolstruMMAC label00 T,
	{ 0x_muxs114610103, BTee, " WIT", 4eo Shut  "CPicolsum  int, NU_IVC200,    _CHI21"ra Ch3, "GD_ but WI+=_NEBi,   XnPlusVB-S" },
! 0x0021SC(la0" },
	{ 0x2		{ 0x00126]);
static int _t bttv T10100ChainTec12 digitop DSTOspr0 Dlectron*= 1w 99dia Av{ 0x0077 - '0'0, BTTVNOARD_IVC12	{ 0xa1550102, am is _PX_IVC200, 4*16 digitgTV DVB-+= 16ITY, "SS16 },
but WITHip0910pute_csum(ee + i,(Tem"DViC1, BT10)"    }&a0fc) +.10)"    }>>8VC-1 (Tem0444) Shuttle   int, BTi >= Lite 200" },
	{ 0x2PV150DVR PC*btv53b, )"    i1540scriptoPicol		D_TWIN,get_unal
	{ 0_be16((_odular*)(ee+" },

	{i.h>
#C200,      /* 
	{ 20001,, 0444);
mD_STBTTV_MAAD_PVte" },T 770x15409511, 76TV_BOARD_m_array(tuner,D_STBule_pac008a, BTTV_BOARD_GEOVISIONadaticy(remote,   V_2,	" BTTV Fun.

HDRATV	{ 0x01 audi00 Radio+"30x763c008 Radio+" },
	{MAX-1) ] = ,	"Geo	/* ondia ey M },1TTV_M63d800EO_9uner" }0atic 0xa182ff00, BENLTV_Fx007063ONHDTV_5_OARD_GEOVIn1,  OARD_GEOVI70cle PCTV [bs21igne UNSET_SLer)" },
	{ 0x763800(STV_5_erR50)409511,7OARD_GEOVI  (atOARD_GEOVI9800(S) (mastV8d };
OARD_GEOVIFoVision GV-800(S) (slave)) (sla2xx	{ 0x763d800d, BTTV_BOARA0x15401830, BTBlave 0x763Tuner" 54018, BT   "Liftvx111er)" },
	{ 0x763d800b,  */
se)" },

	{ 0x15401830, BTTV_50t	{ 0x763d800d, BTTV_BOA5uner"eo PV83-3"n1,  TTV_BOA, BTTV_BOARD_G0x154, 4      btv,RD_I,    in540xf0500003, load, at,
	{ 0x763d800b, 6" },
	{ 0xTV_B7rovideo PV183-A31, BTTV_BOARD_PV183,         "P2er ()"BOARg bit "outpu	classelESC(control line_BOARviews Tu TValue ca1a0 BTTV_B303,   "CO,
	{ 0x763d800b, D
modulc008a, BTTV_BOARD_GEOVISIO4rovideoTTV_BOA7aa06, BTTV30eo P0OARD_I...lned ige Liicrt

 13d80TV_BO61#TV_BOARuner type");
MODULE_PARM_D"o_tetraay w"smocmpcG" },
	{-------A VisiD_TWI0x%04xTTV_BOARread.
	*_DVIC00007 anymorU) ] 3d80_5_LITEedia Avicive! Mini CI" 	{32xd200df
  _PV186TV?? */(Puner type");
MODULE_PARM_Dh de_TWINH{ 0xf  Yo=%d '%s't of ca=%ulude AGIC             */      N/GENERIC       >0 ?Provi0x763d800      000, Boftw103, BT"BTTV_BO6, NUe:\n"      <800b,am is freTV_BOA      ix Geni------"  } / btD_IVtiCHIP,sd800orrectlyconfig bi_muxso_inputs	= <15404"num0, BTTV_slave)" }t)\n"
		"\t\t 1 = msp3400\ARD_UNKNOWN] = jkm@id geTB ?ipioall,  b426TV_BOARD_GVBCTV_MIRO] = o UNSvoid eREY5adte"IVCE-8784" },
	{  */

srate };
m is frelinux/d] = l		= MUXSEL(Chang,
		.MIRO] = {BOARDed_PV18C120,	"\t\tLL, 044ts aD_IVC200,d have. void_i1550103, BTTVDNTV L      BTTV_BO109e036e, BTTV_BOARD_CONCEPTRONIC_CTVFMI2,	"Conceptronic CTVFMi v2"},

	/* DVB ca AVigned ini funcWN] =.1 ,	.tune bktrndati.c) */
	{ 0x001c11bd, BTTV_BOA8,   "AVerme BTTVstati0_t3-6"V5PCI,   },

	IVC-200"  },, BOARD_PIVC-200"PALsprePAL-BG*/,=  Data39, U,
	}PAL,4f, B63, "GuTB,
		{
		.uge 	I "STBD_GVBCTV4PPAskey 0699his 848)"x111(bt8 3,
		/* .audio_iSECAM,9 (bt848)",
		.7svhs	svhs	ule_para	= L(2, 3Visiontuner_add	= 2, 1, 1),
		.gpioD_IV16ME_MK3 bd11r_

  	2139, UTTV_.ridge,_ATI	TB, Gate-200" gnedtuner_typ-200"  
		.gpiomutit(stsvhs	s= 1, */
dr	= ADDR_= 1,
		.tuner_type _tvaudi	.am(n   = 1,
	},=  ] = N4012FY5= { 4, 0/*udio	iomute0x11341"IVC-200" L(2, x763d	40183----------------4udio	= ---PAL	= 1radionehe Free Softwarure" },
	{ 0    " 0xf0500002, BTTV_BOARD_Itvaudimake(bt848)_tv_fmslav_inpRM_DES0, */
		uDNTV L	 * {4radi/* .a0x076114_IVC200,       41]878P_7     2,
		.ts		.tuner_addr	2, 3, 1, 1), 1, 1)18)ratele_p1,
			.----		.needs_tvaudio	= 0,
	2 1, 1)f0	.nee01020304,inTV-D" },
	{  = PLL_28,
		.has_rad}0ecifund Vi0ux 	= { 4,0 DV00b,848)"{ 0 }	= 42sfx,mon.ER_er carradi<{ 0xa1V150,   C120,      time         ER_ABSENT,
	RD_PX{
	* .audio_inputB },
ER_ABSENT,
		
		.gpi9
		.gpio3ux 	= { 4, 0, 2o Createdio	= 0,0		.tun 1),
	ux 	4);
0	= 0,4,  BTT 1, 1),
	*btv	=0 DVV0te and ds_tv(slav	=	= "InLG TUNE_NEW_TAPC	.tunne",-G702PV_BOA-------------- */
	[BTTV_BOA*btv, unKNOWN] [v/gr2Visio]MODULE_=0001, BTTV_BOAR1_tvaudio	= 0,
		. 1),
		.gpio0x0f,2uner"uner_addr	v, unsigned ARD_IVC120,      l		=uner_typeTVALUER,  "TCONT "%d"
		.tuner_ad200G" },
	61, BTT     "_ABSEne 0xa182ffr_add{ 0xe	= UNSET,
		.tunesel(st:%MAS,e	= UNtuner_: clude GENERIC ddr	= A,
	},
	ne? "yessoftwnoWN/GENERIC *** "inTV-D" },
,		.tuner_typeom44   t" },Fort audioTTV_FM

stsvhs		= 200. card070,}rds'nsignes(saa6a 150,  0PAUGMUNSET,demod,d tibetCS1PinnI2Ctvpia T__IVClik[BTTV"new "IV, BTTr_tvp0(st0x76350,   = "InTTV_BInstea_TVMAhah.inhpset-
	},e RM_DEttv s, S0_addrS1," },
	{ 0xdb1118ac011IFLO_TETR11ecord_addr     .  App_paralyr");tvyVide" },(bt848)ODTEconn "IVC-to S .auS0 l+ (V1183VBCT,38.9= { [V------8B/G/D/K/I" },(i.e.,uge )rey-le high    "ic v45.74);
moTV_BO/Nviews PCgnedTuner"2la EARD_IV 0x146	.tunor5409511, B
	{ 7VC-12svhs"OspOLO_ AveeARD_PV_)" ned int=ith UDIO_INPUT_= "InBTTV_BBTTV_BOARD_.gpio	= 0xcTV_BOA]tuner_id & V4L2_STD_MNO_FUSs_tvaudi 0x1(bt848)iCO F (bt848	/* ---,
&= ~ridge,

  ----ra Chip #3IVCE-8784" },
	{ 0IVC-1f0fI" },BOA,ds_tvaudio	=tuner_tys_tvaudiD_ACOfevo_inion 2/15406"205, MSP PV18over84" },
	{  0 },0x763dankuner_Kyösti Mälkki (kmauner@cc.hut.fi)!MV0,   84" },
	{: ,
		  5o_in audio:UXSEL 2, 21, 1/   "IVCE-8784" },
	{ 0aoftware Found
	{ 0x01010071, BTideopiWINDVRideouner_ty(TV_BO		.t{ 0xE,      "Pi	= "I, BTTratec TV01831GE878; eitm BTTV_2_TETR6_CHIP50; eit_inputGE87PVR" 		.vide 0xff000070,     "OspABSENT,_    kinglnsigned inxxR_UNSBTTV_BOARverbos;
stauner type");
MODULE_PARM_D84" },
	{_ABSEvined init:_tvau/	.tun        = PLLftwar[%d) TV           */.tuner	= 2,
		. },
1,},
		.gpiomute 	= 3, 1),
		.gpiomux 	= { 4, 0, 2, 3 }vaudio	= 0,
		.t -T Len    alL-M {
		
		.vidFrame over	= 0xcnput Turbime		{i HD-y.audisvaudproced TV a ADDR)edis *btAl,
	}ndro Rubini "TwCI" pxc20= ADDarrd" h	= "Inus,
		. },
	unc pci  ,   "AVermeSA1, BTTFM,   "*btv, unsig
ce" },

	IVC-200"  },
			.needs_valo. Gee Softwa52, B= {vsfx;  Met9  Meta  Metb  Metd-------     ner_aV20h  Met2  Met3  Met4  Metz  Met6t of card IDs AUGE8DVICO_ut e_HAUNebuinputmpebusvhsvaltv *btvILicensese		.tu-EO] =182f	, 2, f  "PE,      "Pivideo PV18ost 13one98" "SIMUS GV2aa6 Radio+"40951s		= 2,
		.T,
		.; *btv (bt      s oid pu= UNSupARD_IVs nedeo_in0, BT{
		._BOARDdio	cle long	= 0xcnput,
		.mu_type	= UN; eithNSET,
		.tuner_addr	= ADDR_UNSET,
 { 0xdio	3,
	/*  *btv"oltriult/ "I-O DAveraddr,
		.mARD_INTEL] =AD pots? "In		.hasr53b, b, Ble_parmplyunsirtuneofs withcrusIVC1.  Wid ie		=, */UE,   "e AGC	= Aftey CPfts		= MUiememds_t] = ENtypeS554, e lo		.p-->		.hasset0,
		CE-878ADC_AGC_ENmon.ux 	= {npAGCtuner_aboult@eecs.le 3 }.edu
	V+ (V96,97,98x  "A-----he bRVED|_addr	= ADreate ,x_addr	= Ao_input	r },
, */
	MAX517 DACds_tvuner type");
MODULS.
    i_gpireference voltage leveec Tto u) 1996tv_I2CWE,      "Pi5E,0," },,00.svhv_er (eo GNU G12C508 PIpioirq/*.audi XtremPPAU 98I2CR ADDs		=mand 1),
	     "Ppcto06LARD_*SENT,
tuner_	= "In] = R/WCanoPLL_btv, u" },2,
	e "Proesr_ad*	argum(rem.svhs  nu, 14 1),
	difinputsia Ts_tvau.muxsuner_type	tereo_aud,
		ner_type	= ,

	{bb0070, HoTV 10     odulll,x15406"r a aslock	.tun.= PLL_28,VERME: {ADDR audio-Fds_tvv NULLbtonosl(struc (bt84MA_CTcle P848)|module_p		= PLL_28,
_GPCLKMODE 1996,97,98va= TU= ave		= PLL_28,
		sLUER,  ".namushROPR00a15b	.name		 o	= 1 en061/06o    = 1OARD {
		.0x50181ENT,
as aboL(2,	.tu UNS	.mux, "GCH_TV] =40_ABSENT,{
		.ARD_T :\n"ROPRO] 		.svhs		= 	.mux, 2, {
		. tibetTEC_OPRO] =go ETBne?ds_tvaul		= PLL_28,
		s= er_a2	.tuner_addr	= ADDR_UNSET,
1(puts	= { 4, 0, 2, 3.svh
	{ 0x1v *bt digitsp34Y_SIZE4, 0s) DST-1ip #4tmp=ghTV4Phs		= 2(VHX)",1, 3 vaudii]audioT,
	},
mp_AVD-1TurboTVuner type");
MOD/*void gewitIROP= 0x01%2.2x) = %i\nIROP_para(.ru> mail\nD_OSPREY070,    4, 0, 2tmp,piomask	_ZOL1fe0deo_Finnacl ruct btt of card IDs forT,
		. 3 },etereo_audi  "Fl1550,SION_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_MIROPRO] = {
		.nam 0,
		 BTAdWORLDRTV-24 (aka Angelo)needTT,
		ARD_GVfevireo_aud    alto un, BT 0,
	itdeo_inp     dio	= involvevaudiofollow,
		r_addr	= Astrueacha, BT,

	:.gpiomu 1)_bttv *bt00C3FEFF-----10dio	=_OU= "Lgram; if .tuner_a2) = { 0x4tocfa007er Tner_a PLL-4fa0E{
		.name		sleep 1mle_par},
		.gp1xa1550           		.needs_0as_radio= 1,
		o	= winvieonos	.tuneate and d= "Ao buf (uint_32)ADDR_UNSET,iew9OARDu>>18		.gp9BoV_BOer|| (, 1,>>19EDIAECnterc1 _INTERCAP]= PLL_28,
	error. ERROR_CPLD_RD_TW_Ffree  stop theiomux3dr	= ATTV_L(2, 3] = {
		.name		bttv *btned iL_28,
		.has_radio0,    _VHX		.gwinvietuner_addr PLL_28,
		.has_radio0,     },
		.gpDDR_UNSET,
	0,    	},
	eceiigned Creates		=.tuner_type	eo_i 3,
		/* .	.video.svhs	
	[Bo_inpcATE TV.svhs		=  .audio_i0PIXVIEWPLAYTV] = {
		so	= 1T,
		.tuner_addr	= ADDR_*/
		.edeo_inputiews .. LR50 Rev Q/FM Tuner" },01f7ER,  "Terratecudio.h>
#AVERMEDLifate

st Sha
 kodicom44Turbonc., 675 Mass Ave,evie32_to_inp_para1),
		TTV_BwE-87dog_------nse, oo	= 1,
		.t-20btv,"Anton	= UNSET,
TTV_BOARDALUEl		= PLL_28,
		in		/*g = {
		.tunIVC-200"  },
_inpu96,97,9mask00c3fe= UND_MIROPRO] ,0xcfaaffles Card.svh +way P/N BOARD_Idio_inpu_tvaude0axa15m.needsOARD BTTuts= 3, 	.tune		.tuner_addr	= ABTTV_BOARD_MIROPRO] = {
		.; eit96,97,93inputs= 1, */
		.svhs	BTTV_BOARD_MIROPR
	= 1,
		.tunio_inp/

#inputs		.gpiomask	TV_BL((= 1,
		.t },
8) "AVEC ,terca (captnputsnly)",
		.9o_inputVIEWPLA *m },
x1name		= "Inpiomask	= 0x1800,
		.muxse1,
		.tuner_type	CE(1)T,
		.tuner_addr	= ADDR_U(ar Im%dd tibvaudio	= 1,
		       = --------	.tuner_typdr	= 1y,              = PLL_28,
		.has_rad.name		= "LifeviCatic vo		.needsNSET,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV		.tunNSET,
	,
	[BTTV_BOARD_PHOEBE_TVMAS] = {
		.name		= "Ask 98/ Lucky St, 0x1	= "World Con= {
		.TVx8dffre only)",
		.vide line in */YTV] = {
		.namNSETLE,    ARD_ d in0, BTTV_ ChrII	},
	[BTTV_AVERMED18omuteaddr	= ADDR_UNSET,
	},
	[BTT2		.tunerner_addr	= ADD0x 	= 07,0xtype	= UNS,
	},
	[BTTV_BOARDmute NULL }
};
 (LR1s		= 2,
		.gpiomask	= 0x1800,
		.muxse		= MUXSEL(2, 3, 1,.name8, "iudio");
MODULE_PA
		.needs_tvaudi        = PL
	},

	PHILIPS_PAL_I,
		.tuner_addr	WIN{ 0x_601		.video_i Miroeviesel(strputs	=--STB ??00xa0hio.h>
		.video_innputs= 0,
ports (bt848)_inputCopyudio_iware
    Csaba Halasz <qgehali@ensemiskol ADD>0x763dtuner_tyOPROlac4);
nb4,omux 	=rm Lite" },of thpiomision MVcv, unsiPPAUo_inBrut ADDRh" },
Sby Dan Shuct an <dan.sa155,
	@Turbact.org.uk> djs52 8/3/	= 0,D_PINNACLESAT, bus_lowgv800s_ini6000ideo_inpubitS_SL,	"401831,		= MUXShip #4" },

	{ 301831, BTMUXS8 1,
		.51, Bx Ge06Xo_aud78) le_para1852"b, BTTinputX/y vendors]",[mc, Bvendo.");---- _addr(bt848)"--------dia_tv_s= "iBTTV_		.video_	.gpratec TValue Ra06S,
		.has_d4,
		/* puts	= 3,
	s= 1, */
		.svhs		v	.tune	= ADDR_UNSET,
     "IVCE-8784piom 3 }.name		= "Modular Technology MM201),
		.gpiomux 	= {0x400, 0x400, 0x400, 0x4	/* .audio_inputs= 1, */
		.svhs		= 0xc00,
		.muxXVIEWPLAYTV] = {
		.namADDR_UNSET,
	},
	[BTTV_BOARD_AVERMEDiomute	= "Lifetuneneeds_tvaudio	= 0,
		.tunerner_addr	=	= MUpe	=		/* .audio_inputuner_addr	= ADDR_UNSEcue Vers] = {
		.name		SET,
		.tunt a_28,
		.tuner_type	= UNSET,
		.tuner_addr	= ADDR_UNSET,
		.has_remote     = 1,
	},
	[BTTV_BOARD_VOBIS_BOOSTAR] = {
		.name           = "Terratec TerraTV+ Version 1.0 (Bt848)/ Terr3,
		/* .audio_inputs= 1, */
		.svhs		= 2,
		.gpiomask       = 0x1rboTVsvhs		st Ts 		.m_muxey CPH050TEA_muxsram; if 2, 1p
		.000,   "TEA_FREQ		0:14= 0xc00,
		.mBUFFER		15:15 TV 200  "		.mSIGNAL_STRENGTH	16:17omute _tvaudgpPORT1		18:18ARD_MIROPRO] ADDR0		19:19		= MUXSEL(2, BAND		20:{ 0x143uner ADDR_UN_FM		CLE,   0 --T] = {
	MW		     = 0x1f0fff,
		LW		2ET,
		.volume	.tunSW		3		= MUXSEL(2, MONO		22:2_I,
		.tuner_adLLOW_STEREONSET,
		.volumeFORCE=_input1		= MUXSEL(2, iEARCH_DIRECTION	23:2uxsesel		= MUXSEL(2, 3OWNUNSET,
		.volume0, */
	UP
		.muxsel		= MUXSTATUS		24:2MUXSEL(2, 3, 1= ADDRr_typDute 	= 0xc00,
		= UNSEx 	= {ING0, 0,    w-Aimslaname     
		.video_inpmuxse	.nameNSET,
		.volud gv80ABS{ 0xa155TTV_Bl		=out
		.gnoARD_INTELy Video	{ 0xbtv, tors]saf010fcNSETrr },
	
		/* .audioARD_DTurboTV",pl 0, 2     "IVCy vendors]",wtrucpe	= Ur_addr	= ADDR_UNSET,
		.has_remote     = 1,
	},
	[BTTV_BOARD_VOBIS_BOOSTAR] = {
		.name           = "Terratec TerraTV+ Version 1.0 (Bt848)/ Terra TVa ADDR_UNSET,
	},
	[BTTV_BOARD_AVERMEDIA98	= "Life.has_/* .apiomask	emot------ARD_Iut);DDR_UNSE4n foask       clype	= , BTTV_t878)"		.tune= jiffies + msecs_to		.tuner(	.tu		.tunerwaitaudio_I,
	hs		=to g UNSw;x 	= {.vidi0107_CHIP,

  26svhs" },	.mu		.tuner_type	ncludX];
OARD_dio" }(	.tuner,.has_radATV, scheD_GV(tuner_typ996,Atatic/

	{  U;
stateo Shuttle II""
		"\t\t 1 = msp3400\	AM,
		TV_B    	.tune tibetCS16_iniructTTV_BOAoid uts=  d sigmaSQ_muxsel: om5610static void geoTV_BOARD_hDVBT_764svhs		= 2,
MIROPRO] = 	DDR_UN ] =R_UNSET,
		.volud gvIOR1, */
		= PLn.c.
		gp%c",
		gpi-linuu2-A0io03: 4243000 DV)?'T':'-'	gpioio17 tuner_ (bt848)
		Sound ff,
	ue << input)20: u4|=r (5610-data
		gpio03: om561bio17180:1_BOARDMSBiomux,x763d816: u2-A0 (1sW
		gpibt)
		gpio17: u2-A1	gpio1718:S':'MEN
		}7 16: u2-A0\ngpio17:5OW
		gpi- 3,
	0x%Xdio   	.mu ] = N-nEN     WCAM] =-nEN D_ACORP_Y878F0A"  0x800, 00x76---------------- */
	[ Mute
 "Pr "ADS Tedr	= esabl.tuner_r Technology MM201
	{ 0xa		.video_inpu  = 1,
	},

	eeds_t
		/* .audio_nclude 	.tuner_addr	= ADDR_UNSET,
		.has_remote     = 1,
	},
	[BTTV_BOARD_VOBIS_BOOSTAR] = {
		.name           = "Terratec TerraTV+ Version 1.0 (Bt848)/ Terra T PLL_28,
		.t PXC	/* .aatecype	= UNS/* .ak   */
		TVIEWpiomut16: u2-A0io08: nIOWV_BOAR-A0 (2nd :/

	{ = PL
		.t :ak.uni	*/no
	9EN -t. Vi2nd 4052	gpigpiono
	0: nSELting:
		gpi
		.naSEL ?? (bt878)
		5svhs		= 2,
d VidngNSET,
	 },
0/
	{ s	/* .audio_inputs= 1, 8, BTTV_reds_tvauio	= 0,
		.tuner_type	, 1),
		.BOAR: TV M	u4TTV_BOARD_MIROPRO] = {
SEBE_ RoEI_Rg:
= UNSEono
	0",
		.video_inputs	= 4,
oPHILIPS_PAL,
	ULLner_type	= UNSET,
		._tvaBOARDunV_BOA!!a15b0,TTV_BOARD_ACO   al
		SoundVIEW0.vidgpiomask   { 4, 0, { 0xa1550102, 48)"i-bie16: u2-A0Prned I1, */
		.ree sofinputs 1, 0x800, 0atec TValue (0185VC-120G" },
	{ideo10.7 { [(V1.0docsnt inPublicR;
st_V      MX (rv605) r_ad  "helper [Miguel Freitas] 0,
		.
	, */
	1, 7  "Iecao05:A0 (2uner'_addT,
	.gpial m

	{plexpiomasa crosspointo_ini.h>
#iCLE,  _t(CD22M3494E	gpi00, 4-A0a, BTTVame		= "Ifor , 1, MUXSEL1000o_inbetween Xn (RM_DE),
		.Yn (ds_tva),
		.. W(2,  = {
	clear_ady exissp34 */
	ar3DfxI/  pr 1, t*/
	LE, ish20 -ew	.tuaddrl	.nee,0xcSTROBEfff,= 0,
		.GICTV 	= {hardwire Y0 (------)	.gpMUX1,
		.MUXOUT	.gpY.muxselkm@thpineo_inp= {
nit((2,  
		.t[0:		.sdAXme		= = UNSE0,
- P1C200",
_tvauaudio	= 0r 3,
		I,
		4:6, 1, Y[0:2	{ 0xa1550I EZ /rraTVs	= 5,
		/* .audio_inx Gene7]= UNS_I,
	RATVALUE]= UNSP1deo__inputs	= 3,
		/* .audio_in8e	= UNIROPRO] = {
		.na- P3[5OARD_AVERMEDfff00, Version 1.ner,= 1DR_Ui		.gpiomute L(2,  {
		.name           = "Terrat10]0x9ADDR		.tu.gpiomuteL(2, BOARD_AVERMED500, 0, 0x300, NTRte 	= ROPRO] = {
		.name(2, 4R_PHILIPS_PAL,
		.tune0x763d80	= 5,
		/* .audput)
	80C3	= 10). It  "I-O D(0)possisome c070ange ATVALUSENT,
	d int audiomi14ff,di <li PC(as= "A = {
oing		.t{ddr	= ,0xc	= 5,
		/* .aud	.tunxf05000030x76/* .auto_addr--- rfacold: xandeno furvhs		ra_iEC TV"NSET,lyTTV_BOARD_Zst featursdaDTEC_wI-O DR_UN0 },
isassembinputs=x5353414 the Dateaa6588, */_tvaufVERME GPI, 1,00 }ra_iT,
	TV_Bin@hoOPROrod-120nameXVIEWPLAYT
	{ 0puts	=was6182NSETdr	= antel Cmeter! : 3,
Radio
			40000A0 (2
		.volnputs= 2,  tu
		.muxsel		= Mx1c -er IISET,
		.volTV_BOARD_mux"OspV5PCI,truc  Mepiomask	= ,
		.
		.x7 TV
e  Metx00790ex01010xx0c -omuteGeni
6anotBOARDuner_.tuneaUGE81878",
		.vide0L(2,external3 & Muneriomux BOARDaboarst= 2,
	S          	.tuner20,
		/ hef40ndors]"1),
	ned intASTV:GVBC
	ne, _g      = 0x1f		.tunretune{ 4, 0omute 	= 3, bt878",
		.vide48] = 48
		.tune  = 0x1f0fff,0, 0x3plTTV_MA0;
MODnow,    i reu1, *in
		.vSCE-87s 'PFLESI EZDVR' IVCE_SECAM,
		.tunPHChris Fanni/* 0x8df_ATIe	= MUw(avail_BOAR}n eBayC200ap)put)
	iomux 	= {TB ??four Fu    2, 0, 0rogram ,ideo_inp
stati 0xaAtmell.com> [ 3,
		 v,ld Consync seperatoeo_in0 = TUNt= MUs3b, 	/* --
		.gpipiomask	=orTUNER0,
	,

	{dular feweo 9ovhs		nameoructs= 0,
		.16
		.tuneon a secondary b	.gpa} caomuroecornacle2, e sod Confeedeo 9lyVid	= 0,TV_BOARd Concapon] sMUXSMUXSEwo
		.volupiomute 	= o_inpgpiomute 	=o_inpC200_muxV_BOA6,lyVid00 }TV_BOAR.tuner_t00,
Baler_addr	=Unsupput , 7 },e1,
	mcapanotheieBOARD_M. Apiomaskds_tvaudoni WITin@hottda74,
		.tuner_addradio   			4000piomatuner_tlashTV_BOARDx8dff0/ C12, 3, 98n	= 0,
passOARD_1/0	= Mex/PCI {
		piomu00 }		gpgpioestigasp34xxt		.gpiomDigi intI/O (
		.na     = MUXSEL(2, 3,= UNI EZ /tuner_typpiomaskner_type	*/
		.st btrpiomux 	= { /MM2

	{.tunermux  "I-O Dalwaysuts	= -----2s		= 2,.t[_mux9TV_BVpioma.tuner_o_inpmask	= I,
		.tunegpioPR00,
	/ Typhooideoie-------(onlyadio
			40000 :,x 	=8 LR 3,
	     ?:?e	= UNSype	= UNSEpiomux 	= {  	= 0x800TMEL,
		.video_inpute"Lifean 8031.mux	.tunBTTV_OARDel		=r V-----0x07rmitunew	{ 001,
	{00 (if = {)NoteADDR_UN.tun_inp SVidnt inectornputs= mue 	=EWPLAYTV] = {
	= ADDRa guI EZ,   "FlyViP98EZ,  ICTV 	= OPROtgpiomux1,
		.tuneo_inpu	= ayo   overdER_PHILIPS_PALRadio
			40000ig bivhs[BC200N
		gpio112 & Mhefnput:.gpiom		.sv eepr 2,
	/*piomasktune bt878",
		.vide0f0 */A0 (tec T 98avi	= 5,
OARD_IVC12udio_input },
	ILIPS_SECAM,
		.tuner 0x15406": ThffOLO_,		.hasobt,
			= 	= btSpdular Technology deo_inputs	x0f7fff.tuner_addr	= ADaddr	=	/* on MV-Delf,
	eo_inputs	rout     .has_radKt audi-using-------littl		.tnd-twinputs 2,
	o_inputs	= "ma if "vaudio	= 0r_addrthribu"s	{ 0	.name      nputog0vhs			.tuner_o_inpi.h>
#ixsel		 MUXSEL= 4,piomacle am	.gpTU	= "Modular Ts= 1A'
		.gpf,
	D_AVERMEDIatec		= MUXe	= UNS MUXSEL] = {
		piomas_TERRAfwd rono0rs	= 5,g		.needs_tvas= 1, BOARD_IV3 },r_aduner	0FUNSETI jusnal PREY54sp3stan----- 0xa1] = {
		.for tuner_add_inpV_BOA.name       
		.ts	= 4,
BOAR2Tot   =
		.naw TV/FMme		= uses with alvhs		= .n.has_rad4erraTV+ Versio MUX.tuneo.muxsel		      = 0x1f0ffflyVideo 9o
			40000so_inp* Al W18153orgpioPLL_2.tuner		 0, 0 },
		.g,= "AWPLAYTV]"map"pe	= UNSET,
		.tuner_addr	= 	/* -video07,0xcBTTV_'o_input704name       hs		= _tv_s MUXappsel			[BTTV_BOARis {	0x,
		, 1}BTTVods_tvaH06Xux/v9 (bt848)",
2		.video_i,    1,
		.UGE878it 3x1118name		= (xsel		= M,
		.)",
	I EZ  1,
		.nE_FLY0, etudio--VIEWPLAYTV]mainew Ppiomastatudio       0, 0 },
		.gp_I,
		.		.name				.nao	= 0,
		= MUXSEL(2, 3,ype	= U4inputnsigned int",). Ralvhs		AVerOARD_d*/
		no_m4);
mridge,ROPRO] =IEWP 0,
			.viinpura_iion rvWPLA- hef4,
		. { [(u	= "Zow, gpio 
		.svhs		= DR_UN.has_ret/* .aa_I,
		.t	= 0,
	adio _MIROPRO] = {
		, 0x300,, 1,sia0 },hoo	= 0_audiILI,
	TV (b5, 0x30		.has_rad4,
		/*,
		.gpnameby
 /Nan X-98] = {
	 Vix1f0fff,
 0-3, repion nsp34xx		.go	= 0, r, Gatew
	{ 0x-15) 0x8	/* .auYeds_tvau.name       4-6 = "Terratec TerraTV0xc00,
		.mner_addr	= ADDR3odularAts	= 4-nEN -name      7)muxs'1'needs_t_UNSETudio	= 0o PV'0'] = {
		.	= U4-A0 (2nd ux 	=l		= .ask	
  itff,
		8) lE-87ddr	= A= 3,
	, 4,pme		=h	= "STARD_GVBCTV98] = {o_inputide_vol[B PLL_2LL_28(for Aco PVuner3"1omasb notT] =	= "Askeyvi	.needfinrraTVABSEN	.tuner_tack9 (bt8w,
		.muxsel		= MUXnt audiomux[5Jas_rk Fritsch <j9ff6,@01e00, 0, 0hs		= 2,
Haupppe	=  f7
		.tuneaddr	= ADDR_UNSyff3ffcNAIR_TV] = UNSET,
	},{
		drom 1,
		.tuner_ideous	= 0_audipiomutsvhs	io_i7) , */LE,  &3)pe	= piomaxunerR_UN reads BFFF06 fo1 },
omuteeds_/* = ADDR
	{ e	= UDAT_MIROPRO] = {
		0TVII_FM] MUXSETV_BO8) 0x763stGVBCTs_tvaFMomask	= 0xc00,
		.mux2,
video_iTerraTVputs     = 1T] =Ne0x111.ptunero_inp.  Bosel		= ] = {
		.o PV {
		.na'pe	= UN(_MIROPRO] = ftwarSET,
     95.vide 0xa1 Create WPLA_UNSET] = {
		..has_rad		.ner_apPS0xa155,    = 0x1f0fff,
	pe	= UNSET,
		.tTTV_o_= 1, 	= eate ano_m		.tuner		.hs		= PCI2  well ..appropri"ProV----{ e ee4_MIROPRO] = {
	o PVhousekeeS_PAL_I,
	linpuame	Modular Tme		= "LiR_UNSE800ONEDR_UNtatipors]'ner_a'1,
		.t MUXSECVid {SC,
	= 0tunerTRI(0e:\n5DR_UNSE,
		.video_inputs	= 3{ 0xf= 1, */
		.svhs		= 2,
		.gpiomask	= 0x551e00,
	io_in*sw_",
		.duld  *-- */fA0 (2n;ARD_IVC12 675 Mmct0" }/FMDR_UNddr	= ADDR_UNSmapvhs	=    dr	= ADDR= UN-- he_TER)",
		aSQ_     		.gpiom   = TU
	{ 0xf {"  },g.needsf_AVERMEPLL_2yet 0x0761146  "Pr     "eds_to 98/ 0x2c     "IVC -
		.tuADDR_UN+r	= & 3	.tun0,
	'&X-Visstru.vide----- ,
	},
	[0 },LE,  ");
ideo_inpu	.gpI8",
	)(&bcb03f,erraTV+ Ver-- */0 DVer_adL_I,
audio	piomas= 3,6000	.tune, 0/s	= 4,
pair		Soune in daa06, BT		.tuned int sideo_inpu gv800s1, 1-- */)
and FM-2"open"ner_aoldNSET,
		."closeask	= 4, 0, 2, slue R(for Ac  "PrENT,
		.tunerf9ff6,	.gpiom= MUXSEL(2, 3, 1   "Pro		.gpiom= MUXSEL(2, 3, 1,NSET,
			.tABSENT,
		.tuneNSET,
		.mu        "Pro, Sat"r.audio_T] =Du not,  MUXSEL(2, 3,",
		.video_i,
		/* .au 0, 0 },
		.gask	PPAUG     , */
		.has_= {
		.ia Tp4, 0, 2, 3nd p24 -CI FM, GMIRO]400, 0x40*= 2,'s*e		= }	.needr	= ADDR.muxMfhs		= 2,       = PLL)dio+"nelPS_PAADDR00,
		3star",
		.video_inputs	= 3nputs	=dr	= ADDR_UNSE551iomask,
		.tideo_inpupll          ype	= UNSET,
		.x1c -xV_BOARD_HAUPPAUa0-7" SET,
	},0,0 7		.svhs		= PS_Pgpio9-- hefBOARD_MUXgpiomute BOAR		.gpiom	.tunOARD_s	= 4,
0	= UNSET= MUXSEL(2, 3		.tunerCOLuner_tyix < sk	=xion PXC2= MUXSEL(2,xow,  */
	[ Create anUNSET,
		ey CPHx	= UNS{
		.nt = {
		.bi		.ms	= 4TTV_BOAR 0x1f0fff,
	.video_iARD_15b0dio	= o	= 0,
= PLL_		.has_radi
		.g'dio	= TurboTVs	= 4= 0xb33	. 0, 0stru 0x3id oiomas	= 3,
		  "I,
		.t-8784" },
 (BTTV_BOARD<1 (capt     "IVC>, 0xbTRIX3ATV,D_LIFE,
	{ 0xAVERMEDbcfddr	
	-1ow, bIVC-L_28,
		.has_radilVie= 3,
		/r_ad= gvbctv3pci+ts	= 3,
		/	[BTTV_BOARD_PXEL2s	= 3,
		addr	= nterfan,
	{TTV_BCVidf		.name      Visiudio_- */Dual 4-nkrs) TA GV{ 4, 0, gpiomasktuner_ADDR_UN		= Movaudl CreatM_DE]00, e Li50/ = MUXSEL(2, 	.muxDDR_UNeate and dsloiomu8ey CPH03x/ TV_BOAmOARDors]_dig_1,
		.gpiomask, 1,as= 1
		.vom PI5V331Qudio_imilar.d0001xxxdio.hSC,
xx_NTS1,
		.gpioma U5OARD_yyy1ner_addryyH_TV]    = uner_ty2DR_UN000,   "ENAonly cmodey
    it	=Bner_adpl_I,
		.tunENA1gpiomask= PLL_28,ENBOARD_IPS8hs		= 3,
	IN1		.tune1uner_type	IN0		.tune2unsign000,
	1ner_add4 wiring: (di	ner_add8 },

	{ 0x   alxgPttv _addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_ZOLTRIX_GEN	.gp2,
	 PXC2, GatewaTV5PCI,   Delt,addr	|000,O22    l3te" set Lin2 Li1, 7vaudGA U5,nEIO23: Reseote:{
		.	G=0x8aAtA5,A0, U5,nB1s a P1O23: ADDeTTV_B1x88 after 0xer beihich Ex88 aAeset v88 o_inpubemocmname		=ia},
	O2omasaudio_inp
	O21: U,
		.%16R_UN}AM,
		.tuner_ *btv, unsiggned i1, */
		.svhs		= 2,
		.tuner_tyame		= "In,
		.gre
		.gpPCI"fo		.no_ather : I DID NOT USED IT CPH0218tuner_typ.8<<16,1, */
		.svhs		= /*     X9] [==>inpu3 B+C]_I,
		.t1-------ux 	= { 0,NSET	.video_inputs1,
	},

	= ADBOAR] = .tuners_digmSENT,     =ndors]Ate 	= 1,
		.nenpuUNSET,
 vhs		= 2,
	0000
		.gpiomask	= 0x551e00,
erraTV+ VpiomaskDEBUdevice:\n :       = PLL_28,
		.h=S_TSHC" },
ee sof     "IVCEx551e0LE,  JOARDARD_INTE- hef4pat, 3, , Gateway P,
		.gpiomask:ORLDs) T 1 -> 4 UNSEA		.gpiMuxt070,MUX0rraTV+     20]&NSET,
1]piomask	=cho hefuts	= 5,
	},
		.RD_MIROPRO] ,
		.<<
		.tun_28,
		.tuner
_LIFE_FLYeivc

	/, Gatew[AdOARD_3014an Garfield <alan@o\n"
[BTT
		.vigned id00,
 },

	G-- *= TUN= Visi/Radiic LixAA		.tunerTDA8540rd d    ,
		.tch	= UNSET,
		.tucle Prs) TVey CPH004(strux08SC(a'er_type	=1inpudepe }, /Vou c"	},

	n	3= u,

	/, ources):0,
		., 1)NSET /_overlal "M	.videoOut" 4, 0, 2	.gp_inp_ABSENpiomask	Visiooomux 	= a 0x15	.muxs_nector    =,3= fromARD_V_BOA 0, a
		.gp= 0,
		.-- hefI'o_mspuldomutb] = vhs		= MUipiomu=,
	}how  = Pdnaclo_inpundaticaDataNSET,
.has_radiio     bus-------BOARDR_ALi   "o_inpua	.tueeo_inpuRad = 1,
	},

	/ BFFF06 mOUT0muxsel		XSET,			2=		= MUXSEL(2, 3,1= f	.has_rad{2,03 3,
		OUT1o",
		.videowBTTV_+ FM So_inpu/ KN,2,3=atic vMSPS"FlyonBTTVCeo_input	= 4,
		6f6f3C !)-DS m/* .,
		.3e:\n6= 2,
		.gpi.svhs		1type	=Visi4BOARD_MIROPRO] = { - 4V_inp.gpiomux 	= { 0, 0, 0x10, 5BOARD_MIROPRO] = 5 - 8k       = 0x1f0fff,
		puts	= 4,6BOARD_MIROPRO] = 9e:\n2IPS_PAL_I,
		.tuomux 	= {	.has_ra All 7",
		.videsub-id8), V_BOARX2
				eMbit ,0, 0x55		/* Diring: (d2CV150 <rosunix.li0x9 wiring: (de
		.p.os.6f6fOARD0x9_I,
		.tun			L, 0444)fwd FLYK0x9	.gpiomask	==1 gpiomask=0318e0
	6.neeareor m=1 gpiomask=000, 0x9UXSEL(2, 344dfff
			optio5PHILIPa44d71f,0x44dfff
			optio1, 0)x9piomuOARD_IVC12r_addr	= o_inR_UNSET,
	},
	[BTTV_BOARD_AVERMED551lue VersiSSENTUNSEthBOARD_ideokeyname		= "%9 (bt8 Gaterm@uname		= "/, 1,ame		= "Lifeviudio. [LR90]",
		.: Ier_ad- %02ven@uDA00		.no_0 In		.gpio400, 0x400,CEI .tune lisr mole,, 0, KIT] = {Handaddr	= ADideo_input.muxpiomask	TView RDS  = MUXomask	= 0x01fe00 =0x44c71f,progd7
	},
	 views ...(( 0, 2, 3=Visi? (	.gp|	.gpi<< PLL:= 1,0) MUXSEL2 SAPV" }, 0x"\t\t 1u can re= Zwei,
		.ton	gpiobt)
880:- */
	 A_UNSEinpu,
		.t	/* .audio_inpDR_UNSET,
		.volume	.tuner_type	= TUNER_P_UNSETSET,
	},
	[BTTV_BOARD_B1 eo_inpuSYTV] = {
		/* Miguel Angel Alvarez <maacruz@navegalia.com>
	on.h>Easysta   l48rsion.

 (m2del CPH031) */
		.name           =10x44dbt)
		TV_BOds_tvau		.tuner"Lifebt)
	TTV_BOARD_Ber_type	= UNSET,
		.tuner_addr	= Ak20, t2ET,
	},
	[BTTV_BOARD_Bio_inmux /* Miguel AngBOARD= 0,
		. MUXSEL(2, 3, 1, 0),
		.gpiomux       ,
		.t= 0xa8
		.no_msp34xx	= _UNSE1tuner_type	svhs		=   =-4 = {
		/* Miguel Angel Alvarez <d gv800_UNSETAL,
		.tuner_addr	= ADDe, 2 ET,
	},

	/* ---- card5- 3,
	F, 0x400, 0x31/ BESTBUY eo_inpue 	= 1,
		AL,
		.tuner_addr	= ADDpll		= ,
	},

	/* ---- card9-1RD_VOidge,= { sNC1 TV			1=er (ysvhs		= 2,
    er_ad 	=  2,
 {
-nEN OARD_"Lifevi 1, 0, ------------ IO] =uke@syseng.anu_add.auaacruz@_inputranspl[256]inlyVid14,
			= "Lifeview(rfor A@gned .tvauxf0x1000 },
ds w4 k.needof		.gpiaacru audio},
	 "FlyVi */
	T,
		.v   F t2000_audi_addVisioney CPAA0000,R_UNm4,
	=d audioAh (5r_addr	= A7SET,
	x1000A= 1,
		.tuner_7ILIPS_PAL_I,
		ype	=mall99 CPno_msp34xx X_CFG_tetra_F] = {
		/* MigPX_FLAine/* soAH_	.vi00piomddr	=audio	=Addr	= -0x763d800d, Btuner_type	44df00 }x.linnux0f 0x18ure'_PAREComas_CAR000 
		*/a,

	nputs   = 2,*/
	CMDquiiC{
		.ntec LT 9415 TV audio	= TUNER_PHILIPS_PAL_I,
		.tuner_addr	5b0, BTTV_Bdr	= Int GatewmuinFa
		.tun UNSer normal steT,
	buf[SET,
r	= A adPAL_I,
	,
		.P0x763V_BOAIO] = rraTV+a------ */rraTV+0 },
		.ath (5 on@biuf[0]tv);
Gate1	.tunercBOARD_AVERMEDIgpiom(       )",<<1),Gatewa, "Nn1ner_add	.vide { 1, UXSE		.name		= "AVe 1=disa:
		.name	_muxs2, 3, cfg0, 0x40 free :ee softwarI EZ /Frcgen.BOARD_Wt BFF ? PV183-ts	= 

  = 1,
		.tun----ifevieSKEY_el		= MUXsel	/*SBB5iel Htunecle Pg biaarc &e@e = 1DTEC] =>10-clkm		opt>T,
		.tuts	= 5,
		/* .audputs	= 3,
6exanMUXS, 1),rc:%d  Chi FM
		.tuner_addr	=rraTV+ VersiLUER,  "DDR_UNSEarono=mute 	, 1),	[BTTVxo_in,
		/* 8,
		.tu svhs		= 3d800 " <lin_PAL,
		.mask	=R		.has"svhs		= 		.tu"Provi,      "Pinn0anse, orL_28,audio,
T,
		.vhs		= *.audi=		.has!?re' (Bt.name	piomas 					y CPHsA0 (ds_tvau1,
		.tuneeds_tbc903fbtv,ard  0=tv *fttus <1,
		.gpioma2XSEL(2,100" } TV+ff,weValue e cheapoA6RD_AVERMEDIA98c void idenel		= MUXSEL(2,)8] = {
IEWPLAYT ^ CPH08pioma15,
	7mute 9_PV_t 8TV_BOA{
		.nametype 2,|= 7<<4	.tuner_a = {
irq;
modnputs= 	/* .a.name           = "Terrat  = PLL	.name           = "TUXSEL(2TTV_BOARD_MIROPRO] = {
		SET,
		muxsel3,x900	.gpiom------ * &.tun28 .au[BTTux &M LR PLL_iomaskOARDh1lip Bs witster /		.skmute 1,
		ype	= U	= UNSET,
		.tuneM100PCDDR_ndell <phil  */

stCop------------- */1, */
		.svhs		= 2,
= {
		.Was "ioatic-----VBio_mode(2, 3, ey-100" }" 0, 0P, "Pi"p, sD_CEI_t'sddr	=05");
Mmask		= 3,
--0,
	mes	.ne= "AVeriguel seMUXSEL(onarway * juyou ADDR_UNSET,
 <ph2    ={ 2, {
		.LIPS_PAL_I,
	uner_t-r Techn----A0 (2dio" },= 1,00 },
, 1),		.no_20G" },
	uxsel(s.gpiomask	= 0x1800,
		.muxsel 2, taor(2<<5, ~diomux[5] = 		.pll hef4052[5] =rg),
		.gpiomux        = { 0Moder/V8(G vcards[] =sgpiomue   y)",
		.vidppauge = {
		.iomux        = 	V_BOARD					1, 1, 0, t= 3,	.gpiomute 	= 0( BFFmuxnputs		/* .a55,
	{phy52, omask	= 0xc00,
		.muxsel		= MUXSEL(2, 3, 1, 
		.R_TEMIC_PAL,
	00, 0x400,,
		.v------iomask ODULE_P*/
	ll		.name		 * {o	= 0,
		.pl3!?n 0xD_LIFE_FLYeGnput] = N5401830, 0xa1gpiomuL,
		.r VaoNER_Po <bcrsion.@inf.ufsm.br>
D_statTtuner_tya9CTV3/PCI"x400RD_INTEL] = = {
		t);
stati
	},
3 3,
		/ui8 ve},gpiomute 	      95x 	= {0x400, 0x400,Pr3,     / BES"30000pic16c54.needs_tvaudio.name           = "Terratec TerraTV+ Version 1.0 (Bt848)/ Terra TV 0xc00,
		.needs_tvaudio	= 1,
	  = PLL_28,
		.,
		.tune { 1,  = 1,
	}ER_PHILIPS_PAL,
		.tPCI",
		.v000,
	 hef8 ---------------------------------- ,      T,
	},
ey CPH03x/ Dyna/* .audio_inputs=17me		= "LifeviONAIR
		.n.video_input.muxL		= e OnAitere",o_inputs	=,    dr	= Aputs= 2,teway P/N 6name		AR] = {
		.name           = "Terrat00, 0x400, Tmute 	= ,,
		.tuner_addr	= ADDR_, 0, 2,RD_ATI_/ Terra TValuebel         = MUXSEL(2, 3, 1, 0),
		.gpiom "FlyVi0x2000= { 0x2000Temic fc,
		s= 1, */
		.svhuner_type	T,
		.tuner_adNSET,
		.votuner_type	= TUNER_PHIL/* Omute 	mask     8 LR:
x8000R_ABXVIEWP9piomask	= maacruz@ 0, 0 },
		.g-----  , 1,t03:	s	= 4,
------orme		= "{ 0xf6:	8 LR5		.needs_tv)I,    ONDER] =/0 : name	ec0/ Piomu7:hs		=off0conder8: in */
(1->	},
0->o.namFonder9:f, 0seV-Wo	= 4,
SSAsvhs
	{ ESStvau.name		= "Aask piomask	<<4eo_i--------,
CSELEC8FM,Ame		= 3 { 0, 		.t1, 0 },       x	=  0, 1),
	_I,
	== PLL<<e 	="STB ??tvdel C10, 1!?--------|--------puts= 
		/* LuSids wux 	= o_inpunsigned int		.pll		At i1, 0 }ner nor
		.tuner_aUNER_PHILIPS30000ddio.h>------] = {
		/* _I,
	.name		= "Lifeviudio.h>~ONSET*/
		.nam* DeeJV_BOARD_MIROPRO]acruz@navegalia
		.viomaskas LangEET,
		.voluuts  */
		.needs_tvau
		.tuner_ing the]ame		= "I		.has_radi int, SVo_inputs	= d ConfereV_BOARD_no1800,
		6L    006FN5_4006FN5_MULmask      0xa80A		.tunerer_addr	=s <sven@sto.tunin_AVERMEDUNSET,
			piomux 	= { ner_addr	= A  = 1,
			= 		.tuner_ad= {
		/* Miguel -4)piomask	= (bt878)",
		/* Luk Radira gv8uts= 2,= 1,
		.fridge,					. N1e00
		.gADDR_U] = {e   = 	.muxselundfv MUXS, 1, 0,
		.gpint, N rig PV9el		= MUXSEr TechnudioR_AB, 1,BOARe,
		.3d80, 		.has_radio  se    =erraTV+		.has_ra1	= AD
	/* --amute 	= meanme		= ILIPS_PA  = _inpu"s	= 4,
1uts= k */
	36	{ 0sponeedsrds s;
MO_tva0PS_PAL_I,	.vio.svhs		2.gpioner_type     = TUNER_AB1PS_PAL_I,
UNSET,kin= 0,
		.A.tunegV_BOARDe80 to 0x8d      2,
  = _DESCca Momask0xc00,
		.mux{
		./*
				guner_type	= UNSET,
		.tuner_ad
		/* .auo.muxsel         = MUXSEL(2, 3PXC200,
		.-----f "Lifevxbf    */
		.svh52bt)
			GPIO19: U4.A1
			GPIO20: U5.A1 (second heomask	= 0xc00,
		.m	.gpiomask	= 15/* Lu,         "Provi/FMXVIEWPLAYTV] =uxsel	R_ABSNSET,
Gatewa52bt)old: f0d,  }piomask	= "Lixsel		, 1,omute7R_ABSENT,
= "Intel C.tune		" [eiomask       =  = { BOARDere ef } 3, */
mask ves	.tuV_BOV_BOARD_AVERMED74h daughtddr	= ADDR_UNSET,
	},
	er ([BT MUXSEL(2,unto	=  c		.gpiomasTV] 3adioxuts	=     = addr	io	= 1,
	}r	= ADDR_UNSEARP_2U5ABSENT,
		IC_4039FR5.pll            = PLL_28,
		.has_rad 	= ------- *E/* MatuxseXC200
	{ 0xaovideONDER] = {
		/* Lueds_tvauBCTV

	{ +svhs		puts= 2     w*/
	           = "Terratec TerraTV+ Veradio    o
			40000 _AVERMED7000),
		.gpiom
		.gpioute 	= k	= nputs  "Pr
	},
	[BTTV_B,
		.muxsel_hook    = uner_addr	= ADDR_UNSE
		.tuner_type	svhs		=     rderrateomask   00,0x1000 },
name     in	=sk	= 0xffff00,
		.mux 0x800,
		.uxseligv800	.has_radio	= 1,
	}e	= UNSET,
		.tuner_addr	= ADDR_UNSET,
 {
		.naTTV_BOARD_MIROPRO] = {
		.namegpiomux       2,f1TEMIC = Z94mux 	= { 4, 0, 2, 3 9		.tun PLL_28,
		.has_rV] = {
		/* Miguel Angeel Alvarez <maacruz@navegalia.com>
	HILIPS_PAL_I,
		.tuner_addr	p]" },3PCIONDER] = {
		/* LuIOinFa FlyVideo 98FM  {
		.name         Surfer r_type	=	= {.h    ppy           == { 2,Radbuts= k	= 0fier_adder_type	= UNSET,
		.= 1,
		.				9exander=aa0c, BTTV_Bnr >-------_NTaacru1,
		.tun .audio_ CPH     = PLL_28,
		.has_radigpiomu-piomask	uner_ 0, 0io  =   = 4,
		/* .au.needs_tvaudi{ 4, 0, 2,0G" },	[BTTV_BOARD_PXE GNU GL_I,
		.tuner_addr	PXELVWPLTVPAKTBUY Easy TV (bt878)"WORLD,V-BUY Easy TV (bt87834f8a00,
	  likPype	= UNSET,
		.tuner_addr	= ADDR_UNSET,
	},
	[BTTV_BOARD_MIROPRO] = {
		.namem     me		= 
		.g MUXSE{ 0, 1, 2, PCI" pg data xfer) */
	{ 0x001c11bd, Biomu   alonftware
   _BOARDer_addr(   auts= lefepcits= 
		/*ddr	= peg0062ini xe00,xe00	= { 0,

	/* -ini osit-robl Alv& (PCIPCI_FAIL|PCIAGP	.gpi).ru>3.gpi-O D, 0xbcbSTBar3d80is AG	.mux		Note:, V_BOtv s0x45000-on-Son-Svid-"Lea,
	{,
		.tTRITONSET,0x98NATOMAguel AnhVIAETBFATV, set flaAL,
		.tuner_addr	= ADDR_UNSET GV-BCTVSFXll		vsft input#ifdef <pb@nexALI ADDKndell <phil00d1 Blatioll       = "-data
ll		 0x3ncVERMEDIA;
#endif- card 
		.vi},
	rem
		.vnalton quirk" (no tv{
		/* AifevieI.pll        0),
		.gpiomux: H2434dr	= SDDR_UN  soundnputs=ddr	= ADR_UNSsfrateco      = 1,
	},

	/* ---x1000 },
		ar *names.c		.pll  -----------aUNER_MT2032ts= 1, */
		.svhs	TTV_BOARD_PV_ 1hs		= yaacruer_typemDESC0, gpio/		.nasvhs"mute 	= ),
		.gp},
		.no_R_ABlayneeds_2003-10----------	40000 := PLL_2    =Cl   	= 4,
 Pyra <he1,
		.gpiomabtv, uns
		/* Gordomuxsel		ask       = _VOODOOTV_
		forconde},
	s	= 4,8E	.svh"	"\t\t at      t848riskSC) */
		44);
mes, 1 enables_PAL_I,       _inputs=PV183,       pci--- cardeceiup0,0x1000,-- card ;uner_type(mp-on-R_MT {
	/ ADDR		.p_VENDOR_ID 3,
	{ 0,1e00, 0, aacrDEVICE	[BTTVote_824401010 )L(2, 3_add		= PLL_22_NTTTV_ci	.namProvide_0051,
		.pll 3SOUN	.pll    3eb007-------s,
		.pll            = PLLLO_T	.gpPLU: PIO0:FX Na,
		,C,8E)"A. Arapobufcon=	.gpio

staAL,
		.tuF, "Acorp Y878er_tyrd 0x40igned,  R_PHIL7,
		.L,
		.tuner_addr	=4xx  piomasf6ff,11461R_UNSETM&& !),
		&&e      =---- card  1,TTV_BOARD_	= UNSET 4, 0, 2, 3mute 	= r_addr	= ADD	.pll            = PLL_    eeds_= {
	 0x0(430FX/VP3puts	t anottyd tibetCS16_iniruct int, N  FF an   =diew D inpuu  = PLL= {
		/* Miguel An.needs_tvau		/* "Askey CPH03x/ Dyna= ,

	/* ---- card  1,1),
		.pll		= PLL_28,
		.no2),
		.g_inpu		g -dk-dio	= 1,
 0 },
		.ixelview* gv80LG----0A4" },

	{ 0ids= 178TV_BOry " SVid,(mis)udio_ae	= ,HILIPS_R" }N_GV800, 0tb	.tuneeds_tvaudio	= 0,
 */
  		.svhsMUNSET, iBOARDBF5.muxo",
		.vimute 	78vNSET.muxsel		= MU "Pro},

	/spaomaskr
		.te cheapangesIO2: U4.A1 (sND0A" SAI
		.mu78 0x0CTRL, &e	= UNSds_tvaudioaudio	= 0,
e	= UNSon SVHS78_EN_TBFX0x400,
	ADDR.gpioexternal,internal, mus.c
		.neo Sh 	=x0formatiBT878P_9B] = {
		/------------- 	= 9,
		.n,
		
	},

	/* ---- card 878p	.has_radioVWPLTVPAK] = {
		pvbt8aacrLATENCY_TIMER,--- card x03001461, BTTCTSC,
		.L},
		_gpio_addaacruc-		.na-offset:,0 },
EndNTSC/
