/*======================================================================

    Aironet driver for 4500 and 4800 series cards

    This code is released under both the GPL version 2 and BSD licenses.
    Either license may be used.  The respective licenses are found at
    the end of this file.

    This code was developed by Benjamin Reed <breed@users.sourceforge.net>
    including portions of which come from the Aironet PC4500
    Developer's Reference Manual and used with permission.  Copyright
    (C) 1999 Benjamin Reed.  All Rights Reserved.  Permission to use
    code in the Developer's manual was granted for this driver by
    Aironet.  Major code contributions were received from Javier Achirica
    <achirica@users.sourceforge.net> and Jean Tourrilhes <jt@hpl.hp.com>.
    Code was also integrated from the Cisco Aironet driver for Linux.
    Support for MPI350 cards was added by Fabrice Bellet
    <fabrice@bellet.info>.

======================================================================*/

#include <linux/err.h>
#include <linux/init.h>

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>

#include <linux/sched.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/in.h>
#include <linux/bitops.h>
#include <linux/scatterlist.h>
#include <linux/crypto.h>
#include <asm/io.h>
#include <asm/system.h>
#include <asm/unaligned.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/if_arp.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#include <asm/uaccess.h>
#include <linux/kthread.h>
#include <linux/freezer.h>

#include <linux/ieee80211.h>

#include "airo.h"

#define DRV_NAME "airo"

#ifdef CONFIG_PCI
static struct pci_device_id card_ids[] = {
	{ 0x14b9, 1, PCI_ANY_ID, PCI_ANY_ID, },
	{ 0x14b9, 0x4500, PCI_ANY_ID, PCI_ANY_ID },
	{ 0x14b9, 0x4800, PCI_ANY_ID, PCI_ANY_ID, },
	{ 0x14b9, 0x0340, PCI_ANY_ID, PCI_ANY_ID, },
	{ 0x14b9, 0x0350, PCI_ANY_ID, PCI_ANY_ID, },
	{ 0x14b9, 0x5000, PCI_ANY_ID, PCI_ANY_ID, },
	{ 0x14b9, 0xa504, PCI_ANY_ID, PCI_ANY_ID, },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, card_ids);

static int airo_pci_probe(struct pci_dev *, const struct pci_device_id *);
static void airo_pci_remove(struct pci_dev *);
static int airo_pci_suspend(struct pci_dev *pdev, pm_message_t state);
static int airo_pci_resume(struct pci_dev *pdev);

static struct pci_driver airo_driver = {
	.name     = DRV_NAME,
	.id_table = card_ids,
	.probe    = airo_pci_probe,
	.remove   = __devexit_p(airo_pci_remove),
	.suspend  = airo_pci_suspend,
	.resume   = airo_pci_resume,
};
#endif /* CONFIG_PCI */

/* Include Wireless Extension definition and check version - Jean II */
#include <linux/wireless.h>
#define WIRELESS_SPY		/* enable iwspy support */
#include <net/iw_handler.h>	/* New driver API */

#define CISCO_EXT		/* enable Cisco extensions */
#ifdef CISCO_EXT
#include <linux/delay.h>
#endif

/* Hack to do some power saving */
#define POWER_ON_DOWN

/* As you can see this list is HUGH!
   I really don't know what a lot of these counts are about, but they
   are all here for completeness.  If the IGNLABEL macro is put in
   infront of the label, that statistic will not be included in the list
   of statistics in the /proc filesystem */

#define IGNLABEL(comment) NULL
static char *statsLabels[] = {
	"RxOverrun",
	IGNLABEL("RxPlcpCrcErr"),
	IGNLABEL("RxPlcpFormatErr"),
	IGNLABEL("RxPlcpLengthErr"),
	"RxMacCrcErr",
	"RxMacCrcOk",
	"RxWepErr",
	"RxWepOk",
	"RetryLong",
	"RetryShort",
	"MaxRetries",
	"NoAck",
	"NoCts",
	"RxAck",
	"RxCts",
	"TxAck",
	"TxRts",
	"TxCts",
	"TxMc",
	"TxBc",
	"TxUcFrags",
	"TxUcPackets",
	"TxBeacon",
	"RxBeacon",
	"TxSinColl",
	"TxMulColl",
	"DefersNo",
	"DefersProt",
	"DefersEngy",
	"DupFram",
	"RxFragDisc",
	"TxAged",
	"RxAged",
	"LostSync-MaxRetry",
	"LostSync-MissedBeacons",
	"LostSync-ArlExceeded",
	"LostSync-Deauth",
	"LostSync-Disassoced",
	"LostSync-TsfTiming",
	"HostTxMc",
	"HostTxBc",
	"HostTxUc",
	"HostTxFail",
	"HostRxMc",
	"HostRxBc",
	"HostRxUc",
	"HostRxDiscard",
	IGNLABEL("HmacTxMc"),
	IGNLABEL("HmacTxBc"),
	IGNLABEL("HmacTxUc"),
	IGNLABEL("HmacTxFail"),
	IGNLABEL("HmacRxMc"),
	IGNLABEL("HmacRxBc"),
	IGNLABEL("HmacRxUc"),
	IGNLABEL("HmacRxDiscard"),
	IGNLABEL("HmacRxAccepted"),
	"SsidMismatch",
	"ApMismatch",
	"RatesMismatch",
	"AuthReject",
	"AuthTimeout",
	"AssocReject",
	"AssocTimeout",
	IGNLABEL("ReasonOutsideTable"),
	IGNLABEL("ReasonStatus1"),
	IGNLABEL("ReasonStatus2"),
	IGNLABEL("ReasonStatus3"),
	IGNLABEL("ReasonStatus4"),
	IGNLABEL("ReasonStatus5"),
	IGNLABEL("ReasonStatus6"),
	IGNLABEL("ReasonStatus7"),
	IGNLABEL("ReasonStatus8"),
	IGNLABEL("ReasonStatus9"),
	IGNLABEL("ReasonStatus10"),
	IGNLABEL("ReasonStatus11"),
	IGNLABEL("ReasonStatus12"),
	IGNLABEL("ReasonStatus13"),
	IGNLABEL("ReasonStatus14"),
	IGNLABEL("ReasonStatus15"),
	IGNLABEL("ReasonStatus16"),
	IGNLABEL("ReasonStatus17"),
	IGNLABEL("ReasonStatus18"),
	IGNLABEL("ReasonStatus19"),
	"RxMan",
	"TxMan",
	"RxRefresh",
	"TxRefresh",
	"RxPoll",
	"TxPoll",
	"HostRetries",
	"LostSync-HostReq",
	"HostTxBytes",
	"HostRxBytes",
	"ElapsedUsec",
	"ElapsedSec",
	"LostSyncBetterAP",
	"PrivacyMismatch",
	"Jammed",
	"DiscRxNotWepped",
	"PhyEleMismatch",
	(char*)-1 };
#ifndef RUN_AT
#define RUN_AT(x) (jiffies+(x))
#endif


/* These variables are for insmod, since it seems that the rates
   can only be set in setup_card.  Rates should be a comma separated
   (no spaces) list of rates (up to 8). */

static int rates[8];
static int basic_rate;
static char *ssids[3];

static int io[4];
static int irq[4];

static
int maxencrypt /* = 0 */; /* The highest rate that the card can encrypt at.
		       0 means no limit.  For old cards this was 4 */

static int auto_wep /* = 0 */; /* If set, it tries to figure out the wep mode */
static int aux_bap /* = 0 */; /* Checks to see if the aux ports are needed to read
		    the bap, needed on some older cards and buses. */
static int adhoc;

static int probe = 1;

static int proc_uid /* = 0 */;

static int proc_gid /* = 0 */;

static int airo_perm = 0555;

static int proc_perm = 0644;

MODULE_AUTHOR("Benjamin Reed");
MODULE_DESCRIPTION("Support for Cisco/Aironet 802.11 wireless ethernet \
cards.  Direct support for ISA/PCI/MPI cards and support \
for PCMCIA when used with airo_cs.");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_SUPPORTED_DEVICE("Aironet 4500, 4800 and Cisco 340/350");
module_param_array(io, int, NULL, 0);
module_param_array(irq, int, NULL, 0);
module_param(basic_rate, int, 0);
module_param_array(rates, int, NULL, 0);
module_param_array(ssids, charp, NULL, 0);
module_param(auto_wep, int, 0);
MODULE_PARM_DESC(auto_wep, "If non-zero, the driver will keep looping through \
the authentication options until an association is made.  The value of \
auto_wep is number of the wep keys to check.  A value of 2 will try using \
the key at index 0 and index 1.");
module_param(aux_bap, int, 0);
MODULE_PARM_DESC(aux_bap, "If non-zero, the driver will switch into a mode \
than seems to work better for older cards with some older buses.  Before \
switching it checks that the switch is needed.");
module_param(maxencrypt, int, 0);
MODULE_PARM_DESC(maxencrypt, "The maximum speed that the card can do \
encryption.  Units are in 512kbs.  Zero (default) means there is no limit. \
Older cards used to be limited to 2mbs (4).");
module_param(adhoc, int, 0);
MODULE_PARM_DESC(adhoc, "If non-zero, the card will start in adhoc mode.");
module_param(probe, int, 0);
MODULE_PARM_DESC(probe, "If zero, the driver won't start the card.");

module_param(proc_uid, int, 0);
MODULE_PARM_DESC(proc_uid, "The uid that the /proc files will belong to.");
module_param(proc_gid, int, 0);
MODULE_PARM_DESC(proc_gid, "The gid that the /proc files will belong to.");
module_param(airo_perm, int, 0);
MODULE_PARM_DESC(airo_perm, "The permission bits of /proc/[driver/]aironet.");
module_param(proc_perm, int, 0);
MODULE_PARM_DESC(proc_perm, "The permission bits of the files in /proc");

/* This is a kind of sloppy hack to get this information to OUT4500 and
   IN4500.  I would be extremely interested in the situation where this
   doesn't work though!!! */
static int do8bitIO /* = 0 */;

/* Return codes */
#define SUCCESS 0
#define ERROR -1
#define NO_PACKET -2

/* Commands */
#define NOP2		0x0000
#define MAC_ENABLE	0x0001
#define MAC_DISABLE	0x0002
#define CMD_LOSE_SYNC	0x0003 /* Not sure what this does... */
#define CMD_SOFTRESET	0x0004
#define HOSTSLEEP	0x0005
#define CMD_MAGIC_PKT	0x0006
#define CMD_SETWAKEMASK	0x0007
#define CMD_READCFG	0x0008
#define CMD_SETMODE	0x0009
#define CMD_ALLOCATETX	0x000a
#define CMD_TRANSMIT	0x000b
#define CMD_DEALLOCATETX 0x000c
#define NOP		0x0010
#define CMD_WORKAROUND	0x0011
#define CMD_ALLOCATEAUX 0x0020
#define CMD_ACCESS	0x0021
#define CMD_PCIBAP	0x0022
#define CMD_PCIAUX	0x0023
#define CMD_ALLOCBUF	0x0028
#define CMD_GETTLV	0x0029
#define CMD_PUTTLV	0x002a
#define CMD_DELTLV	0x002b
#define CMD_FINDNEXTTLV	0x002c
#define CMD_PSPNODES	0x0030
#define CMD_SETCW	0x0031    
#define CMD_SETPCF	0x0032    
#define CMD_SETPHYREG	0x003e
#define CMD_TXTEST	0x003f
#define MAC_ENABLETX	0x0101
#define CMD_LISTBSS	0x0103
#define CMD_SAVECFG	0x0108
#define CMD_ENABLEAUX	0x0111
#define CMD_WRITERID	0x0121
#define CMD_USEPSPNODES	0x0130
#define MAC_ENABLERX	0x0201

/* Command errors */
#define ERROR_QUALIF 0x00
#define ERROR_ILLCMD 0x01
#define ERROR_ILLFMT 0x02
#define ERROR_INVFID 0x03
#define ERROR_INVRID 0x04
#define ERROR_LARGE 0x05
#define ERROR_NDISABL 0x06
#define ERROR_ALLOCBSY 0x07
#define ERROR_NORD 0x0B
#define ERROR_NOWR 0x0C
#define ERROR_INVFIDTX 0x0D
#define ERROR_TESTACT 0x0E
#define ERROR_TAGNFND 0x12
#define ERROR_DECODE 0x20
#define ERROR_DESCUNAV 0x21
#define ERROR_BADLEN 0x22
#define ERROR_MODE 0x80
#define ERROR_HOP 0x81
#define ERROR_BINTER 0x82
#define ERROR_RXMODE 0x83
#define ERROR_MACADDR 0x84
#define ERROR_RATES 0x85
#define ERROR_ORDER 0x86
#define ERROR_SCAN 0x87
#define ERROR_AUTH 0x88
#define ERROR_PSMODE 0x89
#define ERROR_RTYPE 0x8A
#define ERROR_DIVER 0x8B
#define ERROR_SSID 0x8C
#define ERROR_APLIST 0x8D
#define ERROR_AUTOWAKE 0x8E
#define ERROR_LEAP 0x8F

/* Registers */
#define COMMAND 0x00
#define PARAM0 0x02
#define PARAM1 0x04
#define PARAM2 0x06
#define STATUS 0x08
#define RESP0 0x0a
#define RESP1 0x0c
#define RESP2 0x0e
#define LINKSTAT 0x10
#define SELECT0 0x18
#define OFFSET0 0x1c
#define RXFID 0x20
#define TXALLOCFID 0x22
#define TXCOMPLFID 0x24
#define DATA0 0x36
#define EVSTAT 0x30
#define EVINTEN 0x32
#define EVACK 0x34
#define SWS0 0x28
#define SWS1 0x2a
#define SWS2 0x2c
#define SWS3 0x2e
#define AUXPAGE 0x3A
#define AUXOFF 0x3C
#define AUXDATA 0x3E

#define FID_TX 1
#define FID_RX 2
/* Offset into aux memory for descriptors */
#define AUX_OFFSET 0x800
/* Size of allocated packets */
#define PKTSIZE 1840
#define RIDSIZE 2048
/* Size of the transmit queue */
#define MAXTXQ 64

/* BAP selectors */
#define BAP0 0 /* Used for receiving packets */
#define BAP1 2 /* Used for xmiting packets and working with RIDS */

/* Flags */
#define COMMAND_BUSY 0x8000

#define BAP_BUSY 0x8000
#define BAP_ERR 0x4000
#define BAP_DONE 0x2000

#define PROMISC 0xffff
#define NOPROMISC 0x0000

#define EV_CMD 0x10
#define EV_CLEARCOMMANDBUSY 0x4000
#define EV_RX 0x01
#define EV_TX 0x02
#define EV_TXEXC 0x04
#define EV_ALLOC 0x08
#define EV_LINK 0x80
#define EV_AWAKE 0x100
#define EV_TXCPY 0x400
#define EV_UNKNOWN 0x800
#define EV_MIC 0x1000 /* Message Integrity Check Interrupt */
#define EV_AWAKEN 0x2000
#define STATUS_INTS (EV_AWAKE|EV_LINK|EV_TXEXC|EV_TX|EV_TXCPY|EV_RX|EV_MIC)

#ifdef CHECK_UNKNOWN_INTS
#define IGNORE_INTS ( EV_CMD | EV_UNKNOWN)
#else
#define IGNORE_INTS (~STATUS_INTS)
#endif

/* RID TYPES */
#define RID_RW 0x20

/* The RIDs */
#define RID_CAPABILITIES 0xFF00
#define RID_APINFO     0xFF01
#define RID_RADIOINFO  0xFF02
#define RID_UNKNOWN3   0xFF03
#define RID_RSSI       0xFF04
#define RID_CONFIG     0xFF10
#define RID_SSID       0xFF11
#define RID_APLIST     0xFF12
#define RID_DRVNAME    0xFF13
#define RID_ETHERENCAP 0xFF14
#define RID_WEP_TEMP   0xFF15
#define RID_WEP_PERM   0xFF16
#define RID_MODULATION 0xFF17
#define RID_OPTIONS    0xFF18
#define RID_ACTUALCONFIG 0xFF20 /*readonly*/
#define RID_FACTORYCONFIG 0xFF21
#define RID_UNKNOWN22  0xFF22
#define RID_LEAPUSERNAME 0xFF23
#define RID_LEAPPASSWORD 0xFF24
#define RID_STATUS     0xFF50
#define RID_BEACON_HST 0xFF51
#define RID_BUSY_HST   0xFF52
#define RID_RETRIES_HST 0xFF53
#define RID_UNKNOWN54  0xFF54
#define RID_UNKNOWN55  0xFF55
#define RID_UNKNOWN56  0xFF56
#define RID_MIC        0xFF57
#define RID_STATS16    0xFF60
#define RID_STATS16DELTA 0xFF61
#define RID_STATS16DELTACLEAR 0xFF62
#define RID_STATS      0xFF68
#define RID_STATSDELTA 0xFF69
#define RID_STATSDELTACLEAR 0xFF6A
#define RID_ECHOTEST_RID 0xFF70
#define RID_ECHOTEST_RESULTS 0xFF71
#define RID_BSSLISTFIRST 0xFF72
#define RID_BSSLISTNEXT  0xFF73
#define RID_WPA_BSSLISTFIRST 0xFF74
#define RID_WPA_BSSLISTNEXT  0xFF75

typedef struct {
	u16 cmd;
	u16 parm0;
	u16 parm1;
	u16 parm2;
} Cmd;

typedef struct {
	u16 status;
	u16 rsp0;
	u16 rsp1;
	u16 rsp2;
} Resp;

/*
 * Rids and endian-ness:  The Rids will always be in cpu endian, since
 * this all the patches from the big-endian guys end up doing that.
 * so all rid access should use the read/writeXXXRid routines.
 */

/* This structure came from an email sent to me from an engineer at
   aironet for inclusion into this driver */
typedef struct WepKeyRid WepKeyRid;
struct WepKeyRid {
	__le16 len;
	__le16 kindex;
	u8 mac[ETH_ALEN];
	__le16 klen;
	u8 key[16];
} __attribute__ ((packed));

/* These structures are from the Aironet's PC4500 Developers Manual */
typedef struct Ssid Ssid;
struct Ssid {
	__le16 len;
	u8 ssid[32];
} __attribute__ ((packed));

typedef struct SsidRid SsidRid;
struct SsidRid {
	__le16 len;
	Ssid ssids[3];
} __attribute__ ((packed));

typedef struct ModulationRid ModulationRid;
struct ModulationRid {
        __le16 len;
        __le16 modulation;
#define MOD_DEFAULT cpu_to_le16(0)
#define MOD_CCK cpu_to_le16(1)
#define MOD_MOK cpu_to_le16(2)
} __attribute__ ((packed));

typedef struct ConfigRid ConfigRid;
struct ConfigRid {
	__le16 len; /* sizeof(ConfigRid) */
	__le16 opmode; /* operating mode */
#define MODE_STA_IBSS cpu_to_le16(0)
#define MODE_STA_ESS cpu_to_le16(1)
#define MODE_AP cpu_to_le16(2)
#define MODE_AP_RPTR cpu_to_le16(3)
#define MODE_CFG_MASK cpu_to_le16(0xff)
#define MODE_ETHERNET_HOST cpu_to_le16(0<<8) /* rx payloads converted */
#define MODE_LLC_HOST cpu_to_le16(1<<8) /* rx payloads left as is */
#define MODE_AIRONET_EXTEND cpu_to_le16(1<<9) /* enable Aironet extenstions */
#define MODE_AP_INTERFACE cpu_to_le16(1<<10) /* enable ap interface extensions */
#define MODE_ANTENNA_ALIGN cpu_to_le16(1<<11) /* enable antenna alignment */
#define MODE_ETHER_LLC cpu_to_le16(1<<12) /* enable ethernet LLC */
#define MODE_LEAF_NODE cpu_to_le16(1<<13) /* enable leaf node bridge */
#define MODE_CF_POLLABLE cpu_to_le16(1<<14) /* enable CF pollable */
#define MODE_MIC cpu_to_le16(1<<15) /* enable MIC */
	__le16 rmode; /* receive mode */
#define RXMODE_BC_MC_ADDR cpu_to_le16(0)
#define RXMODE_BC_ADDR cpu_to_le16(1) /* ignore multicasts */
#define RXMODE_ADDR cpu_to_le16(2) /* ignore multicast and broadcast */
#define RXMODE_RFMON cpu_to_le16(3) /* wireless monitor mode */
#define RXMODE_RFMON_ANYBSS cpu_to_le16(4)
#define RXMODE_LANMON cpu_to_le16(5) /* lan style monitor -- data packets only */
#define RXMODE_MASK cpu_to_le16(255)
#define RXMODE_DISABLE_802_3_HEADER cpu_to_le16(1<<8) /* disables 802.3 header on rx */
#define RXMODE_FULL_MASK (RXMODE_MASK | RXMODE_DISABLE_802_3_HEADER)
#define RXMODE_NORMALIZED_RSSI cpu_to_le16(1<<9) /* return normalized RSSI */
	__le16 fragThresh;
	__le16 rtsThres;
	u8 macAddr[ETH_ALEN];
	u8 rates[8];
	__le16 shortRetryLimit;
	__le16 longRetryLimit;
	__le16 txLifetime; /* in kusec */
	__le16 rxLifetime; /* in kusec */
	__le16 stationary;
	__le16 ordering;
	__le16 u16deviceType; /* for overriding device type */
	__le16 cfpRate;
	__le16 cfpDuration;
	__le16 _reserved1[3];
	/*---------- Scanning/Associating ----------*/
	__le16 scanMode;
#define SCANMODE_ACTIVE cpu_to_le16(0)
#define SCANMODE_PASSIVE cpu_to_le16(1)
#define SCANMODE_AIROSCAN cpu_to_le16(2)
	__le16 probeDelay; /* in kusec */
	__le16 probeEnergyTimeout; /* in kusec */
        __le16 probeResponseTimeout;
	__le16 beaconListenTimeout;
	__le16 joinNetTimeout;
	__le16 authTimeout;
	__le16 authType;
#define AUTH_OPEN cpu_to_le16(0x1)
#define AUTH_ENCRYPT cpu_to_le16(0x101)
#define AUTH_SHAREDKEY cpu_to_le16(0x102)
#define AUTH_ALLOW_UNENCRYPTED cpu_to_le16(0x200)
	__le16 associationTimeout;
	__le16 specifiedApTimeout;
	__le16 offlineScanInterval;
	__le16 offlineScanDuration;
	__le16 linkLossDelay;
	__le16 maxBeaconLostTime;
	__le16 refreshInterval;
#define DISABLE_REFRESH cpu_to_le16(0xFFFF)
	__le16 _reserved1a[1];
	/*---------- Power save operation ----------*/
	__le16 powerSaveMode;
#define POWERSAVE_CAM cpu_to_le16(0)
#define POWERSAVE_PSP cpu_to_le16(1)
#define POWERSAVE_PSPCAM cpu_to_le16(2)
	__le16 sleepForDtims;
	__le16 listenInterval;
	__le16 fastListenInterval;
	__le16 listenDecay;
	__le16 fastListenDelay;
	__le16 _reserved2[2];
	/*---------- Ap/Ibss config items ----------*/
	__le16 beaconPeriod;
	__le16 atimDuration;
	__le16 hopPeriod;
	__le16 channelSet;
	__le16 channel;
	__le16 dtimPeriod;
	__le16 bridgeDistance;
	__le16 radioID;
	/*---------- Radio configuration ----------*/
	__le16 radioType;
#define RADIOTYPE_DEFAULT cpu_to_le16(0)
#define RADIOTYPE_802_11 cpu_to_le16(1)
#define RADIOTYPE_LEGACY cpu_to_le16(2)
	u8 rxDiversity;
	u8 txDiversity;
	__le16 txPower;
#define TXPOWER_DEFAULT 0
	__le16 rssiThreshold;
#define RSSI_DEFAULT 0
        __le16 modulation;
#define PREAMBLE_AUTO cpu_to_le16(0)
#define PREAMBLE_LONG cpu_to_le16(1)
#define PREAMBLE_SHORT cpu_to_le16(2)
	__le16 preamble;
	__le16 homeProduct;
	__le16 radioSpecific;
	/*---------- Aironet Extensions ----------*/
	u8 nodeName[16];
	__le16 arlThreshold;
	__le16 arlDecay;
	__le16 arlDelay;
	__le16 _reserved4[1];
	/*---------- Aironet Extensions ----------*/
	u8 magicAction;
#define MAGIC_ACTION_STSCHG 1
#define MAGIC_ACTION_RESUME 2
#define MAGIC_IGNORE_MCAST (1<<8)
#define MAGIC_IGNORE_BCAST (1<<9)
#define MAGIC_SWITCH_TO_PSP (0<<10)
#define MAGIC_STAY_IN_CAM (1<<10)
	u8 magicControl;
	__le16 autoWake;
} __attribute__ ((packed));

typedef struct StatusRid StatusRid;
struct StatusRid {
	__le16 len;
	u8 mac[ETH_ALEN];
	__le16 mode;
	__le16 errorCode;
	__le16 sigQuality;
	__le16 SSIDlen;
	char SSID[32];
	char apName[16];
	u8 bssid[4][ETH_ALEN];
	__le16 beaconPeriod;
	__le16 dimPeriod;
	__le16 atimDuration;
	__le16 hopPeriod;
	__le16 channelSet;
	__le16 channel;
	__le16 hopsToBackbone;
	__le16 apTotalLoad;
	__le16 generatedLoad;
	__le16 accumulatedArl;
	__le16 signalQuality;
	__le16 currentXmitRate;
	__le16 apDevExtensions;
	__le16 normalizedSignalStrength;
	__le16 shortPreamble;
	u8 apIP[4];
	u8 noisePercent; /* Noise percent in last second */
	u8 noisedBm; /* Noise dBm in last second */
	u8 noiseAvePercent; /* Noise percent in last minute */
	u8 noiseAvedBm; /* Noise dBm in last minute */
	u8 noiseMaxPercent; /* Highest noise percent in last minute */
	u8 noiseMaxdBm; /* Highest noise dbm in last minute */
	__le16 load;
	u8 carrier[4];
	__le16 assocStatus;
#define STAT_NOPACKETS 0
#define STAT_NOCARRIERSET 10
#define STAT_GOTCARRIERSET 11
#define STAT_WRONGSSID 20
#define STAT_BADCHANNEL 25
#define STAT_BADBITRATES 30
#define STAT_BADPRIVACY 35
#define STAT_APFOUND 40
#define STAT_APREJECTED 50
#define STAT_AUTHENTICATING 60
#define STAT_DEAUTHENTICATED 61
#define STAT_AUTHTIMEOUT 62
#define STAT_ASSOCIATING 70
#define STAT_DEASSOCIATED 71
#define STAT_ASSOCTIMEOUT 72
#define STAT_NOTAIROAP 73
#define STAT_ASSOCIATED 80
#define STAT_LEAPING 90
#define STAT_LEAPFAILED 91
#define STAT_LEAPTIMEDOUT 92
#define STAT_LEAPCOMPLETE 93
} __attribute__ ((packed));

typedef struct StatsRid StatsRid;
struct StatsRid {
	__le16 len;
	__le16 spacer;
	__le32 vals[100];
} __attribute__ ((packed));

typedef struct APListRid APListRid;
struct APListRid {
	__le16 len;
	u8 ap[4][ETH_ALEN];
} __attribute__ ((packed));

typedef struct CapabilityRid CapabilityRid;
struct CapabilityRid {
	__le16 len;
	char oui[3];
	char zero;
	__le16 prodNum;
	char manName[32];
	char prodName[16];
	char prodVer[8];
	char factoryAddr[ETH_ALEN];
	char aironetAddr[ETH_ALEN];
	__le16 radioType;
	__le16 country;
	char callid[ETH_ALEN];
	char supportedRates[8];
	char rxDiversity;
	char txDiversity;
	__le16 txPowerLevels[8];
	__le16 hardVer;
	__le16 hardCap;
	__le16 tempRange;
	__le16 softVer;
	__le16 softSubVer;
	__le16 interfaceVer;
	__le16 softCap;
	__le16 bootBlockVer;
	__le16 requiredHard;
	__le16 extSoftCap;
} __attribute__ ((packed));

/* Only present on firmware >= 5.30.17 */
typedef struct BSSListRidExtra BSSListRidExtra;
struct BSSListRidExtra {
  __le16 unknown[4];
  u8 fixed[12]; /* WLAN management frame */
  u8 iep[624];
} __attribute__ ((packed));

typedef struct BSSListRid BSSListRid;
struct BSSListRid {
  __le16 len;
  __le16 index; /* First is 0 and 0xffff means end of list */
#define RADIO_FH 1 /* Frequency hopping radio type */
#define RADIO_DS 2 /* Direct sequence radio type */
#define RADIO_TMA 4 /* Proprietary radio used in old cards (2500) */
  __le16 radioType;
  u8 bssid[ETH_ALEN]; /* Mac address of the BSS */
  u8 zero;
  u8 ssidLen;
  u8 ssid[32];
  __le16 dBm;
#define CAP_ESS cpu_to_le16(1<<0)
#define CAP_IBSS cpu_to_le16(1<<1)
#define CAP_PRIVACY cpu_to_le16(1<<4)
#define CAP_SHORTHDR cpu_to_le16(1<<5)
  __le16 cap;
  __le16 beaconInterval;
  u8 rates[8]; /* Same as rates for config rid */
  struct { /* For frequency hopping only */
    __le16 dwell;
    u8 hopSet;
    u8 hopPattern;
    u8 hopIndex;
    u8 fill;
  } fh;
  __le16 dsChannel;
  __le16 atimWindow;

  /* Only present on firmware >= 5.30.17 */
  BSSListRidExtra extra;
} __attribute__ ((packed));

typedef struct {
  BSSListRid bss;
  struct list_head list;
} BSSListElement;

typedef struct tdsRssiEntry tdsRssiEntry;
struct tdsRssiEntry {
  u8 rssipct;
  u8 rssidBm;
} __attribute__ ((packed));

typedef struct tdsRssiRid tdsRssiRid;
struct tdsRssiRid {
  u16 len;
  tdsRssiEntry x[256];
} __attribute__ ((packed));

typedef struct MICRid MICRid;
struct MICRid {
	__le16 len;
	__le16 state;
	__le16 multicastValid;
	u8  multicast[16];
	__le16 unicastValid;
	u8  unicast[16];
} __attribute__ ((packed));

typedef struct MICBuffer MICBuffer;
struct MICBuffer {
	__be16 typelen;

	union {
	    u8 snap[8];
	    struct {
		u8 dsap;
		u8 ssap;
		u8 control;
		u8 orgcode[3];
		u8 fieldtype[2];
	    } llc;
	} u;
	__be32 mic;
	__be32 seq;
} __attribute__ ((packed));

typedef struct {
	u8 da[ETH_ALEN];
	u8 sa[ETH_ALEN];
} etherHead;

#define TXCTL_TXOK (1<<1) /* report if tx is ok */
#define TXCTL_TXEX (1<<2) /* report if tx fails */
#define TXCTL_802_3 (0<<3) /* 802.3 packet */
#define TXCTL_802_11 (1<<3) /* 802.11 mac packet */
#define TXCTL_ETHERNET (0<<4) /* payload has ethertype */
#define TXCTL_LLC (1<<4) /* payload is llc */
#define TXCTL_RELEASE (0<<5) /* release after completion */
#define TXCTL_NORELEASE (1<<5) /* on completion returns to host */

#define BUSY_FID 0x10000

#ifdef CISCO_EXT
#define AIROMAGIC	0xa55a
/* Warning : SIOCDEVPRIVATE may disapear during 2.5.X - Jean II */
#ifdef SIOCIWFIRSTPRIV
#ifdef SIOCDEVPRIVATE
#define AIROOLDIOCTL	SIOCDEVPRIVATE
#define AIROOLDIDIFC 	AIROOLDIOCTL + 1
#endif /* SIOCDEVPRIVATE */
#else /* SIOCIWFIRSTPRIV */
#define SIOCIWFIRSTPRIV SIOCDEVPRIVATE
#endif /* SIOCIWFIRSTPRIV */
/* This may be wrong. When using the new SIOCIWFIRSTPRIV range, we probably
 * should use only "GET" ioctls (last bit set to 1). "SET" ioctls are root
 * only and don't return the modified struct ifreq to the application which
 * is usually a problem. - Jean II */
#define AIROIOCTL	SIOCIWFIRSTPRIV
#define AIROIDIFC 	AIROIOCTL + 1

/* Ioctl constants to be used in airo_ioctl.command */

#define	AIROGCAP  		0	// Capability rid
#define AIROGCFG		1       // USED A LOT
#define AIROGSLIST		2	// System ID list
#define AIROGVLIST		3       // List of specified AP's
#define AIROGDRVNAM		4	//  NOTUSED
#define AIROGEHTENC		5	// NOTUSED
#define AIROGWEPKTMP		6
#define AIROGWEPKNV		7
#define AIROGSTAT		8
#define AIROGSTATSC32		9
#define AIROGSTATSD32		10
#define AIROGMICRID		11
#define AIROGMICSTATS		12
#define AIROGFLAGS		13
#define AIROGID			14
#define AIRORRID		15
#define AIRORSWVERSION		17

/* Leave gap of 40 commands after AIROGSTATSD32 for future */

#define AIROPCAP               	AIROGSTATSD32 + 40
#define AIROPVLIST              AIROPCAP      + 1
#define AIROPSLIST		AIROPVLIST    + 1
#define AIROPCFG		AIROPSLIST    + 1
#define AIROPSIDS		AIROPCFG      + 1
#define AIROPAPLIST		AIROPSIDS     + 1
#define AIROPMACON		AIROPAPLIST   + 1	/* Enable mac  */
#define AIROPMACOFF		AIROPMACON    + 1 	/* Disable mac */
#define AIROPSTCLR		AIROPMACOFF   + 1
#define AIROPWEPKEY		AIROPSTCLR    + 1
#define AIROPWEPKEYNV		AIROPWEPKEY   + 1
#define AIROPLEAPPWD            AIROPWEPKEYNV + 1
#define AIROPLEAPUSR            AIROPLEAPPWD  + 1

/* Flash codes */

#define AIROFLSHRST	       AIROPWEPKEYNV  + 40
#define AIROFLSHGCHR           AIROFLSHRST    + 1
#define AIROFLSHSTFL           AIROFLSHGCHR   + 1
#define AIROFLSHPCHR           AIROFLSHSTFL   + 1
#define AIROFLPUTBUF           AIROFLSHPCHR   + 1
#define AIRORESTART            AIROFLPUTBUF   + 1

#define FLASHSIZE	32768
#define AUXMEMSIZE	(256 * 1024)

typedef struct aironet_ioctl {
	unsigned short command;		// What to do
	unsigned short len;		// Len of data
	unsigned short ridnum;		// rid number
	unsigned char __user *data;	// d-data
} aironet_ioctl;

static char swversion[] = "2.1";
#endif /* CISCO_EXT */

#define NUM_MODULES       2
#define MIC_MSGLEN_MAX    2400
#define EMMH32_MSGLEN_MAX MIC_MSGLEN_MAX
#define AIRO_DEF_MTU      2312

typedef struct {
	u32   size;            // size
	u8    enabled;         // MIC enabled or not
	u32   rxSuccess;       // successful packets received
	u32   rxIncorrectMIC;  // pkts dropped due to incorrect MIC comparison
	u32   rxNotMICed;      // pkts dropped due to not being MIC'd
	u32   rxMICPlummed;    // pkts dropped due to not having a MIC plummed
	u32   rxWrongSequence; // pkts dropped due to sequence number violation
	u32   reserve[32];
} mic_statistics;

typedef struct {
	u32 coeff[((EMMH32_MSGLEN_MAX)+3)>>2];
	u64 accum;	// accumulated mic, reduced to u32 in final()
	int position;	// current position (byte offset) in message
	union {
		u8  d8[4];
		__be32 d32;
	} part;	// saves partial message word across update() calls
} emmh32_context;

typedef struct {
	emmh32_context seed;	    // Context - the seed
	u32		 rx;	    // Received sequence number
	u32		 tx;	    // Tx sequence number
	u32		 window;    // Start of window
	u8		 valid;	    // Flag to say if context is valid or not
	u8		 key[16];
} miccntx;

typedef struct {
	miccntx mCtx;		// Multicast context
	miccntx uCtx;		// Unicast context
} mic_module;

typedef struct {
	unsigned int  rid: 16;
	unsigned int  len: 15;
	unsigned int  valid: 1;
	dma_addr_t host_addr;
} Rid;

typedef struct {
	unsigned int  offset: 15;
	unsigned int  eoc: 1;
	unsigned int  len: 15;
	unsigned int  valid: 1;
	dma_addr_t host_addr;
} TxFid;

struct rx_hdr {
	__le16 status, len;
	u8 rssi[2];
	u8 rate;
	u8 freq;
	__le16 tmp[4];
} __attribute__ ((packed));

typedef struct {
	unsigned int  ctl: 15;
	unsigned int  rdy: 1;
	unsigned int  len: 15;
	unsigned int  valid: 1;
	dma_addr_t host_addr;
} RxFid;

/*
 * Host receive descriptor
 */
typedef struct {
	unsigned char __iomem *card_ram_off; /* offset into card memory of the
						desc */
	RxFid         rx_desc;		     /* card receive descriptor */
	char          *virtual_host_addr;    /* virtual address of host receive
					        buffer */
	int           pending;
} HostRxDesc;

/*
 * Host transmit descriptor
 */
typedef struct {
	unsigned char __iomem *card_ram_off;	     /* offset into card memory of the
						desc */
	TxFid         tx_desc;		     /* card transmit descriptor */
	char          *virtual_host_addr;    /* virtual address of host receive
					        buffer */
	int           pending;
} HostTxDesc;

/*
 * Host RID descriptor
 */
typedef struct {
	unsigned char __iomem *card_ram_off;      /* offset into card memory of the
					     descriptor */
	Rid           rid_desc;		  /* card RID descriptor */
	char          *virtual_host_addr; /* virtual address of host receive
					     buffer */
} HostRidDesc;

typedef struct {
	u16 sw0;
	u16 sw1;
	u16 status;
	u16 len;
#define HOST_SET (1 << 0)
#define HOST_INT_TX (1 << 1) /* Interrupt on successful TX */
#define HOST_INT_TXERR (1 << 2) /* Interrupt on unseccessful TX */
#define HOST_LCC_PAYLOAD (1 << 4) /* LLC payload, 0 = Ethertype */
#define HOST_DONT_RLSE (1 << 5) /* Don't release buffer when done */
#define HOST_DONT_RETRY (1 << 6) /* Don't retry trasmit */
#define HOST_CLR_AID (1 << 7) /* clear AID failure */
#define HOST_RTS (1 << 9) /* Force RTS use */
#define HOST_SHORT (1 << 10) /* Do short preamble */
	u16 ctl;
	u16 aid;
	u16 retries;
	u16 fill;
} TxCtlHdr;

typedef struct {
        u16 ctl;
        u16 duration;
        char addr1[6];
        char addr2[6];
        char addr3[6];
        u16 seq;
        char addr4[6];
} WifiHdr;


typedef struct {
	TxCtlHdr ctlhdr;
	u16 fill1;
	u16 fill2;
	WifiHdr wifihdr;
	u16 gaplen;
	u16 status;
} WifiCtlHdr;

static WifiCtlHdr wifictlhdr8023 = {
	.ctlhdr = {
		.ctl	= HOST_DONT_RLSE,
	}
};

// A few details needed for WEP (Wireless Equivalent Privacy)
#define MAX_KEY_SIZE 13			// 128 (?) bits
#define MIN_KEY_SIZE  5			// 40 bits RC4 - WEP
typedef struct wep_key_t {
	u16	len;
	u8	key[16];	/* 40-bit and 104-bit keys */
} wep_key_t;

/* List of Wireless Handlers (new API) */
static const struct iw_handler_def	airo_handler_def;

static const char version[] = "airo.c 0.6 (Ben Reed & Javier Achirica)";

struct airo_info;

static int get_dec_u16( char *buffer, int *start, int limit );
static void OUT4500( struct airo_info *, u16 register, u16 value );
static unsigned short IN4500( struct airo_info *, u16 register );
static u16 setup_card(struct airo_info*, u8 *mac, int lock);
static int enable_MAC(struct airo_info *ai, int lock);
static void disable_MAC(struct airo_info *ai, int lock);
static void enable_interrupts(struct airo_info*);
static void disable_interrupts(struct airo_info*);
static u16 issuecommand(struct airo_info*, Cmd *pCmd, Resp *pRsp);
static int bap_setup(struct airo_info*, u16 rid, u16 offset, int whichbap);
static int aux_bap_read(struct airo_info*, __le16 *pu16Dst, int bytelen,
			int whichbap);
static int fast_bap_read(struct airo_info*, __le16 *pu16Dst, int bytelen,
			 int whichbap);
static int bap_write(struct airo_info*, const __le16 *pu16Src, int bytelen,
		     int whichbap);
static int PC4500_accessrid(struct airo_info*, u16 rid, u16 accmd);
static int PC4500_readrid(struct airo_info*, u16 rid, void *pBuf, int len, int lock);
static int PC4500_writerid(struct airo_info*, u16 rid, const void
			   *pBuf, int len, int lock);
static int do_writerid( struct airo_info*, u16 rid, const void *rid_data,
			int len, int dummy );
static u16 transmit_allocate(struct airo_info*, int lenPayload, int raw);
static int transmit_802_3_packet(struct airo_info*, int len, char *pPacket);
static int transmit_802_11_packet(struct airo_info*, int len, char *pPacket);

static int mpi_send_packet (struct net_device *dev);
static void mpi_unmap_card(struct pci_dev *pci);
static void mpi_receive_802_3(struct airo_info *ai);
static void mpi_receive_802_11(struct airo_info *ai);
static int waitbusy (struct airo_info *ai);

static irqreturn_t airo_interrupt( int irq, void* dev_id);
static int airo_thread(void *data);
static void timer_func( struct net_device *dev );
static int airo_ioctl(struct net_device *dev, struct ifreq *rq, int cmd);
static struct iw_statistics *airo_get_wireless_stats (struct net_device *dev);
static void airo_read_wireless_stats (struct airo_info *local);
#ifdef CISCO_EXT
static int readrids(struct net_device *dev, aironet_ioctl *comp);
static int writerids(struct net_device *dev, aironet_ioctl *comp);
static int flashcard(struct net_device *dev, aironet_ioctl *comp);
#endif /* CISCO_EXT */
static void micinit(struct airo_info *ai);
static int micsetup(struct airo_info *ai);
static int encapsulate(struct airo_info *ai, etherHead *pPacket, MICBuffer *buffer, int len);
static int decapsulate(struct airo_info *ai, MICBuffer *mic, etherHead *pPacket, u16 payLen);

static u8 airo_rssi_to_dbm (tdsRssiEntry *rssi_rid, u8 rssi);
static u8 airo_dbm_to_pct (tdsRssiEntry *rssi_rid, u8 dbm);

static void airo_networks_free(struct airo_info *ai);

struct airo_info {
	struct net_device             *dev;
	struct list_head              dev_list;
	/* Note, we can have MAX_FIDS outstanding.  FIDs are 16-bits, so we
	   use the high bit to mark whether it is in use. */
#define MAX_FIDS 6
#define MPI_MAX_FIDS 1
	u32                           fids[MAX_FIDS];
	ConfigRid config;
	char keyindex; // Used with auto wep
	char defindex; // Used with auto wep
	struct proc_dir_entry *proc_entry;
        spinlock_t aux_lock;
#define FLAG_RADIO_OFF	0	/* User disabling of MAC */
#define FLAG_RADIO_DOWN	1	/* ifup/ifdown disabling of MAC */
#define FLAG_RADIO_MASK 0x03
#define FLAG_ENABLED	2
#define FLAG_ADHOC	3	/* Needed by MIC */
#define FLAG_MIC_CAPABLE 4
#define FLAG_UPDATE_MULTI 5
#define FLAG_UPDATE_UNI 6
#define FLAG_802_11	7
#define FLAG_PROMISC	8	/* IFF_PROMISC 0x100 - include/linux/if.h */
#define FLAG_PENDING_XMIT 9
#define FLAG_PENDING_XMIT11 10
#define FLAG_MPI	11
#define FLAG_REGISTERED	12
#define FLAG_COMMIT	13
#define FLAG_RESET	14
#define FLAG_FLASHING	15
#define FLAG_WPA_CAPABLE	16
	unsigned long flags;
#define JOB_DIE	0
#define JOB_XMIT	1
#define JOB_XMIT11	2
#define JOB_STATS	3
#define JOB_PROMISC	4
#define JOB_MIC	5
#define JOB_EVENT	6
#define JOB_AUTOWEP	7
#define JOB_WSTATS	8
#define JOB_SCAN_RESULTS  9
	unsigned long jobs;
	int (*bap_read)(struct airo_info*, __le16 *pu16Dst, int bytelen,
			int whichbap);
	unsigned short *flash;
	tdsRssiEntry *rssi;
	struct task_struct *list_bss_task;
	struct task_struct *airo_thread_task;
	struct semaphore sem;
	wait_queue_head_t thr_wait;
	unsigned long expires;
	struct {
		struct sk_buff *skb;
		int fid;
	} xmit, xmit11;
	struct net_device *wifidev;
	struct iw_statistics	wstats;		// wireless stats
	unsigned long		scan_timeout;	/* Time scan should be read */
	struct iw_spy_data	spy_data;
	struct iw_public_data	wireless_data;
	/* MIC stuff */
	struct crypto_cipher	*tfm;
	mic_module		mod[2];
	mic_statistics		micstats;
	HostRxDesc rxfids[MPI_MAX_FIDS]; // rx/tx/config MPI350 descriptors
	HostTxDesc txfids[MPI_MAX_FIDS];
	HostRidDesc config_desc;
	unsigned long ridbus; // phys addr of config_desc
	struct sk_buff_head txq;// tx queue used by mpi350 code
	struct pci_dev          *pci;
	unsigned char		__iomem *pcimem;
	unsigned char		__iomem *pciaux;
	unsigned char		*shared;
	dma_addr_t		shared_dma;
	pm_message_t		power;
	SsidRid			*SSID;
	APListRid		*APList;
#define	PCI_SHARED_LEN		2*MPI_MAX_FIDS*PKTSIZE+RIDSIZE
	char			proc_name[IFNAMSIZ];

	int			wep_capable;
	int			max_wep_idx;

	/* WPA-related stuff */
	unsigned int bssListFirst;
	unsigned int bssListNext;
	unsigned int bssListRidLen;

	struct list_head network_list;
	struct list_head network_free_list;
	BSSListElement *networks;
};

static inline int bap_read(struct airo_info *ai, __le16 *pu16Dst, int bytelen,
			   int whichbap)
{
	return ai->bap_read(ai, pu16Dst, bytelen, whichbap);
}

static int setup_proc_entry( struct net_device *dev,
			     struct airo_info *apriv );
static int takedown_proc_entry( struct net_device *dev,
				struct airo_info *apriv );

static int cmdreset(struct airo_info *ai);
static int setflashmode (struct airo_info *ai);
static int flashgchar(struct airo_info *ai,int matchbyte,int dwelltime);
static int flashputbuf(struct airo_info *ai);
static int flashrestart(struct airo_info *ai,struct net_device *dev);

#define airo_print(type, name, fmt, args...) \
	printk(type DRV_NAME "(%s): " fmt "\n", name, ##args)

#define airo_print_info(name, fmt, args...) \
	airo_print(KERN_INFO, name, fmt, ##args)

#define airo_print_dbg(name, fmt, args...) \
	airo_print(KERN_DEBUG, name, fmt, ##args)

#define airo_print_warn(name, fmt, args...) \
	airo_print(KERN_WARNING, name, fmt, ##args)

#define airo_print_err(name, fmt, args...) \
	airo_print(KERN_ERR, name, fmt, ##args)

#define AIRO_FLASH(dev) (((struct airo_info *)dev->ml_priv)->flash)

/***********************************************************************
 *                              MIC ROUTINES                           *
 ***********************************************************************
 */

static int RxSeqValid (struct airo_info *ai,miccntx *context,int mcast,u32 micSeq);
static void MoveWindow(miccntx *context, u32 micSeq);
static void emmh32_setseed(emmh32_context *context, u8 *pkey, int keylen,
			   struct crypto_cipher *tfm);
static void emmh32_init(emmh32_context *context);
static void emmh32_update(emmh32_context *context, u8 *pOctets, int len);
static void emmh32_final(emmh32_context *context, u8 digest[4]);
static int flashpchar(struct airo_info *ai,int byte,int dwelltime);

static void age_mic_context(miccntx *cur, miccntx *old, u8 *key, int key_len,
			    struct crypto_cipher *tfm)
{
	/* If the current MIC context is valid and its key is the same as
	 * the MIC register, there's nothing to do.
	 */
	if (cur->valid && (memcmp(cur->key, key, key_len) == 0))
		return;

	/* Age current mic Context */
	memcpy(old, cur, sizeof(*cur));

	/* Initialize new context */
	memcpy(cur->key, key, key_len);
	cur->window  = 33; /* Window always points to the middle */
	cur->rx      = 0;  /* Rx Sequence numbers */
	cur->tx      = 0;  /* Tx sequence numbers */
	cur->valid   = 1;  /* Key is now valid */

	/* Give key to mic seed */
	emmh32_setseed(&cur->seed, key, key_len, tfm);
}

/* micinit - Initialize mic seed */

static void micinit(struct airo_info *ai)
{
	MICRid mic_rid;

	clear_bit(JOB_MIC, &ai->jobs);
	PC4500_readrid(ai, RID_MIC, &mic_rid, sizeof(mic_rid), 0);
	up(&ai->sem);

	ai->micstats.enabled = (le16_to_cpu(mic_rid.state) & 0x00FF) ? 1 : 0;
	if (!ai->micstats.enabled) {
		/* So next time we have a valid key and mic is enabled, we will
		 * update the sequence number if the key is the same as before.
		 */
		ai->mod[0].uCtx.valid = 0;
		ai->mod[0].mCtx.valid = 0;
		return;
	}

	if (mic_rid.multicastValid) {
		age_mic_context(&ai->mod[0].mCtx, &ai->mod[1].mCtx,
		                mic_rid.multicast, sizeof(mic_rid.multicast),
		                ai->tfm);
	}

	if (mic_rid.unicastValid) {
		age_mic_context(&ai->mod[0].uCtx, &ai->mod[1].uCtx,
				mic_rid.unicast, sizeof(mic_rid.unicast),
				ai->tfm);
	}
}

/* micsetup - Get ready for business */

static int micsetup(struct airo_info *ai) {
	int i;

	if (ai->tfm == NULL)
	        ai->tfm = crypto_alloc_cipher("aes", 0, CRYPTO_ALG_ASYNC);

        if (IS_ERR(ai->tfm)) {
                airo_print_err(ai->dev->name, "failed to load transform for AES");
                ai->tfm = NULL;
                return ERROR;
        }

	for (i=0; i < NUM_MODULES; i++) {
		memset(&ai->mod[i].mCtx,0,sizeof(miccntx));
		memset(&ai->mod[i].uCtx,0,sizeof(miccntx));
	}
	return SUCCESS;
}

static char micsnap[] = {0xAA,0xAA,0x03,0x00,0x40,0x96,0x00,0x02};

/*===========================================================================
 * Description: Mic a packet
 *    
 *      Inputs: etherHead * pointer to an 802.3 frame
 *    
 *     Returns: BOOLEAN if successful, otherwise false.
 *             PacketTxLen will be updated with the mic'd packets size.
 *
 *    Caveats: It is assumed that the frame buffer will already
 *             be big enough to hold the largets mic message possible.
 *            (No memory allocation is done here).
 *  
 *    Author: sbraneky (10/15/01)
 *    Merciless hacks by rwilcher (1/14/02)
 */

static int encapsulate(struct airo_info *ai ,etherHead *frame, MICBuffer *mic, int payLen)
{
	miccntx   *context;

	// Determine correct context
	// If not adhoc, always use unicast key

	if (test_bit(FLAG_ADHOC, &ai->flags) && (frame->da[0] & 0x1))
		context = &ai->mod[0].mCtx;
	else
		context = &ai->mod[0].uCtx;
  
	if (!context->valid)
		return ERROR;

	mic->typelen = htons(payLen + 16); //Length of Mic'd packet

	memcpy(&mic->u.snap, micsnap, sizeof(micsnap)); // Add Snap

	// Add Tx sequence
	mic->seq = htonl(context->tx);
	context->tx += 2;

	emmh32_init(&context->seed); // Mic the packet
	emmh32_update(&context->seed,frame->da,ETH_ALEN * 2); // DA,SA
	emmh32_update(&context->seed,(u8*)&mic->typelen,10); // Type/Length and Snap
	emmh32_update(&context->seed,(u8*)&mic->seq,sizeof(mic->seq)); //SEQ
	emmh32_update(&context->seed,frame->da + ETH_ALEN * 2,payLen); //payload
	emmh32_final(&context->seed, (u8*)&mic->mic);

	/*    New Type/length ?????????? */
	mic->typelen = 0; //Let NIC know it could be an oversized packet
	return SUCCESS;
}

typedef enum {
    NONE,
    NOMIC,
    NOMICPLUMMED,
    SEQUENCE,
    INCORRECTMIC,
} mic_error;

/*===========================================================================
 *  Description: Decapsulates a MIC'd packet and returns the 802.3 packet
 *               (removes the MIC stuff) if packet is a valid packet.
 *      
 *       Inputs: etherHead  pointer to the 802.3 packet             
 *     
 *      Returns: BOOLEAN - TRUE if packet should be dropped otherwise FALSE
 *     
 *      Author: sbraneky (10/15/01)
 *    Merciless hacks by rwilcher (1/14/02)
 *---------------------------------------------------------------------------
 */

static int decapsulate(struct airo_info *ai, MICBuffer *mic, etherHead *eth, u16 payLen)
{
	int      i;
	u32      micSEQ;
	miccntx  *context;
	u8       digest[4];
	mic_error micError = NONE;

	// Check if the packet is a Mic'd packet

	if (!ai->micstats.enabled) {
		//No Mic set or Mic OFF but we received a MIC'd packet.
		if (memcmp ((u8*)eth + 14, micsnap, sizeof(micsnap)) == 0) {
			ai->micstats.rxMICPlummed++;
			return ERROR;
		}
		return SUCCESS;
	}

	if (ntohs(mic->typelen) == 0x888E)
		return SUCCESS;

	if (memcmp (mic->u.snap, micsnap, sizeof(micsnap)) != 0) {
	    // Mic enabled but packet isn't Mic'd
		ai->micstats.rxMICPlummed++;
	    	return ERROR;
	}

	micSEQ = ntohl(mic->seq);            //store SEQ as CPU order

	//At this point we a have a mic'd packet and mic is enabled
	//Now do the mic error checking.

	//Receive seq must be odd
	if ( (micSEQ & 1) == 0 ) {
		ai->micstats.rxWrongSequence++;
		return ERROR;
	}

	for (i = 0; i < NUM_MODULES; i++) {
		int mcast = eth->da[0] & 1;
		//Determine proper context 
		context = mcast ? &ai->mod[i].mCtx : &ai->mod[i].uCtx;
	
		//Make sure context is valid
		if (!context->valid) {
			if (i == 0)
				micError = NOMICPLUMMED;
			continue;                
		}
	       	//DeMic it 

		if (!mic->typelen)
			mic->typelen = htons(payLen + sizeof(MICBuffer) - 2);
	
		emmh32_init(&context->seed);
		emmh32_update(&context->seed, eth->da, ETH_ALEN*2); 
		emmh32_update(&context->seed, (u8 *)&mic->typelen, sizeof(mic->typelen)+sizeof(mic->u.snap)); 
		emmh32_update(&context->seed, (u8 *)&mic->seq,sizeof(mic->seq));	
		emmh32_update(&context->seed, eth->da + ETH_ALEN*2,payLen);	
		//Calculate MIC
		emmh32_final(&context->seed, digest);
	
		if (memcmp(digest, &mic->mic, 4)) { //Make sure the mics match
		  //Invalid Mic
			if (i == 0)
				micError = INCORRECTMIC;
			continue;
		}

		//Check Sequence number if mics pass
		if (RxSeqValid(ai, context, mcast, micSEQ) == SUCCESS) {
			ai->micstats.rxSuccess++;
			return SUCCESS;
		}
		if (i == 0)
			micError = SEQUENCE;
	}

	// Update statistics
	switch (micError) {
		case NOMICPLUMMED: ai->micstats.rxMICPlummed++;   break;
		case SEQUENCE:    ai->micstats.rxWrongSequence++; break;
		case INCORRECTMIC: ai->micstats.rxIncorrectMIC++; break;
		case NONE:  break;
		case NOMIC: break;
	}
	return ERROR;
}

/*===========================================================================
 * Description:  Checks the Rx Seq number to make sure it is valid
 *               and hasn't already been received
 *   
 *     Inputs: miccntx - mic context to check seq against
 *             micSeq  - the Mic seq number
 *   
 *    Returns: TRUE if valid otherwise FALSE. 
 *
 *    Author: sbraneky (10/15/01)
 *    Merciless hacks by rwilcher (1/14/02)
 *---------------------------------------------------------------------------
 */

static int RxSeqValid (struct airo_info *ai,miccntx *context,int mcast,u32 micSeq)
{
	u32 seq,index;

	//Allow for the ap being rebooted - if it is then use the next 
	//sequence number of the current sequence number - might go backwards

	if (mcast) {
		if (test_bit(FLAG_UPDATE_MULTI, &ai->flags)) {
			clear_bit (FLAG_UPDATE_MULTI, &ai->flags);
			context->window = (micSeq > 33) ? micSeq : 33;
			context->rx     = 0;        // Reset rx
		}
	} else if (test_bit(FLAG_UPDATE_UNI, &ai->flags)) {
		clear_bit (FLAG_UPDATE_UNI, &ai->flags);
		context->window = (micSeq > 33) ? micSeq : 33; // Move window
		context->rx     = 0;        // Reset rx
	}

	//Make sequence number relative to START of window
	seq = micSeq - (context->window - 33);

	//Too old of a SEQ number to check.
	if ((s32)seq < 0)
		return ERROR;
    
	if ( seq > 64 ) {
		//Window is infinite forward
		MoveWindow(context,micSeq);
		return SUCCESS;
	}

	// We are in the window. Now check the context rx bit to see if it was already sent
	seq >>= 1;         //divide by 2 because we only have odd numbers
	index = 1 << seq;  //Get an index number

	if (!(context->rx & index)) {
		//micSEQ falls inside the window.
		//Add seqence number to the list of received numbers.
		context->rx |= index;

		MoveWindow(context,micSeq);

		return SUCCESS;
	}
	return ERROR;
}

static void MoveWindow(miccntx *context, u32 micSeq)
{
	u32 shift;

	//Move window if seq greater than the middle of the window
	if (micSeq > context->window) {
		shift = (micSeq - context->window) >> 1;
    
		    //Shift out old
		if (shift < 32)
			context->rx >>= shift;
		else
			context->rx = 0;

		context->window = micSeq;      //Move window
	}
}

/*==============================================*/
/*========== EMMH ROUTINES  ====================*/
/*==============================================*/

/* mic accumulate */
#define MIC_ACCUM(val)	\
	context->accum += (u64)(val) * context->coeff[coeff_position++];

static unsigned char aes_counter[16];

/* expand the key to fill the MMH coefficient array */
static void emmh32_setseed(emmh32_context *context, u8 *pkey, int keylen,
			   struct crypto_cipher *tfm)
{
  /* take the keying material, expand if necessary, truncate at 16-bytes */
  /* run through AES counter mode to generate context->coeff[] */
  
	int i,j;
	u32 counter;
	u8 *cipher, plain[16];

	crypto_cipher_setkey(tfm, pkey, 16);
	counter = 0;
	for (i = 0; i < ARRAY_SIZE(context->coeff); ) {
		aes_counter[15] = (u8)(counter >> 0);
		aes_counter[14] = (u8)(counter >> 8);
		aes_counter[13] = (u8)(counter >> 16);
		aes_counter[12] = (u8)(counter >> 24);
		counter++;
		memcpy (plain, aes_counter, 16);
		crypto_cipher_encrypt_one(tfm, plain, plain);
		cipher = plain;
		for (j = 0; (j < 16) && (i < ARRAY_SIZE(context->coeff)); ) {
			context->coeff[i++] = ntohl(*(__be32 *)&cipher[j]);
			j += 4;
		}
	}
}

/* prepare for calculation of a new mic */
static void emmh32_init(emmh32_context *context)
{
	/* prepare for new mic calculation */
	context->accum = 0;
	context->position = 0;
}

/* add some bytes to the mic calculation */
static void emmh32_update(emmh32_context *context, u8 *pOctets, int len)
{
	int	coeff_position, byte_position;
  
	if (len == 0) return;
  
	coeff_position = context->position >> 2;
  
	/* deal with partial 32-bit word left over from last update */
	byte_position = context->position & 3;
	if (byte_position) {
		/* have a partial word in part to deal with */
		do {
			if (len == 0) return;
			context->part.d8[byte_position++] = *pOctets++;
			context->position++;
			len--;
		} while (byte_position < 4);
		MIC_ACCUM(ntohl(context->part.d32));
	}

	/* deal with full 32-bit words */
	while (len >= 4) {
		MIC_ACCUM(ntohl(*(__be32 *)pOctets));
		context->position += 4;
		pOctets += 4;
		len -= 4;
	}

	/* deal with partial 32-bit word that will be left over from this update */
	byte_position = 0;
	while (len > 0) {
		context->part.d8[byte_position++] = *pOctets++;
		context->position++;
		len--;
	}
}

/* mask used to zero empty bytes for final partial word */
static u32 mask32[4] = { 0x00000000L, 0xFF000000L, 0xFFFF0000L, 0xFFFFFF00L };

/* calculate the mic */
static void emmh32_final(emmh32_context *context, u8 digest[4])
{
	int	coeff_position, byte_position;
	u32	val;
  
	u64 sum, utmp;
	s64 stmp;

	coeff_position = context->position >> 2;
  
	/* deal with partial 32-bit word left over from last update */
	byte_position = context->position & 3;
	if (byte_position) {
		/* have a partial word in part to deal with */
		val = ntohl(context->part.d32);
		MIC_ACCUM(val & mask32[byte_position]);	/* zero empty bytes */
	}

	/* reduce the accumulated u64 to a 32-bit MIC */
	sum = context->accum;
	stmp = (sum  & 0xffffffffLL) - ((sum >> 32)  * 15);
	utmp = (stmp & 0xffffffffLL) - ((stmp >> 32) * 15);
	sum = utmp & 0xffffffffLL;
	if (utmp > 0x10000000fLL)
		sum -= 15;

	val = (u32)sum;
	digest[0] = (val>>24) & 0xFF;
	digest[1] = (val>>16) & 0xFF;
	digest[2] = (val>>8) & 0xFF;
	digest[3] = val & 0xFF;
}

static int readBSSListRid(struct airo_info *ai, int first,
		      BSSListRid *list)
{
	Cmd cmd;
	Resp rsp;

	if (first == 1) {
		if (ai->flags & FLAG_RADIO_MASK) return -ENETDOWN;
		memset(&cmd, 0, sizeof(cmd));
		cmd.cmd=CMD_LISTBSS;
		if (down_interruptible(&ai->sem))
			return -ERESTARTSYS;
		ai->list_bss_task = current;
		issuecommand(ai, &cmd, &rsp);
		up(&ai->sem);
		/* Let the command take effect */
		schedule_timeout_uninterruptible(3 * HZ);
		ai->list_bss_task = NULL;
	}
	return PC4500_readrid(ai, first ? ai->bssListFirst : ai->bssListNext,
			    list, ai->bssListRidLen, 1);
}

static int readWepKeyRid(struct airo_info *ai, WepKeyRid *wkr, int temp, int lock)
{
	return PC4500_readrid(ai, temp ? RID_WEP_TEMP : RID_WEP_PERM,
				wkr, sizeof(*wkr), lock);
}

static int writeWepKeyRid(struct airo_info *ai, WepKeyRid *wkr, int perm, int lock)
{
	int rc;
	rc = PC4500_writerid(ai, RID_WEP_TEMP, wkr, sizeof(*wkr), lock);
	if (rc!=SUCCESS)
		airo_print_err(ai->dev->name, "WEP_TEMP set %x", rc);
	if (perm) {
		rc = PC4500_writerid(ai, RID_WEP_PERM, wkr, sizeof(*wkr), lock);
		if (rc!=SUCCESS)
			airo_print_err(ai->dev->name, "WEP_PERM set %x", rc);
	}
	return rc;
}

static int readSsidRid(struct airo_info*ai, SsidRid *ssidr)
{
	return PC4500_readrid(ai, RID_SSID, ssidr, sizeof(*ssidr), 1);
}

static int writeSsidRid(struct airo_info*ai, SsidRid *pssidr, int lock)
{
	return PC4500_writerid(ai, RID_SSID, pssidr, sizeof(*pssidr), lock);
}

static int readConfigRid(struct airo_info *ai, int lock)
{
	int rc;
	ConfigRid cfg;

	if (ai->config.len)
		return SUCCESS;

	rc = PC4500_readrid(ai, RID_ACTUALCONFIG, &cfg, sizeof(cfg), lock);
	if (rc != SUCCESS)
		return rc;

	ai->config = cfg;
	return SUCCESS;
}

static inline void checkThrottle(struct airo_info *ai)
{
	int i;
/* Old hardware had a limit on encryption speed */
	if (ai->config.authType != AUTH_OPEN && maxencrypt) {
		for(i=0; i<8; i++) {
			if (ai->config.rates[i] > maxencrypt) {
				ai->config.rates[i] = 0;
			}
		}
	}
}

static int writeConfigRid(struct airo_info *ai, int lock)
{
	ConfigRid cfgr;

	if (!test_bit (FLAG_COMMIT, &ai->flags))
		return SUCCESS;

	clear_bit (FLAG_COMMIT, &ai->flags);
	clear_bit (FLAG_RESET, &ai->flags);
	checkThrottle(ai);
	cfgr = ai->config;

	if ((cfgr.opmode & MODE_CFG_MASK) == MODE_STA_IBSS)
		set_bit(FLAG_ADHOC, &ai->flags);
	else
		clear_bit(FLAG_ADHOC, &ai->flags);

	return PC4500_writerid( ai, RID_CONFIG, &cfgr, sizeof(cfgr), lock);
}

static int readStatusRid(struct airo_info *ai, StatusRid *statr, int lock)
{
	return PC4500_readrid(ai, RID_STATUS, statr, sizeof(*statr), lock);
}

static int readAPListRid(struct airo_info *ai, APListRid *aplr)
{
	return PC4500_readrid(ai, RID_APLIST, aplr, sizeof(*aplr), 1);
}

static int writeAPListRid(struct airo_info *ai, APListRid *aplr, int lock)
{
	return PC4500_writerid(ai, RID_APLIST, aplr, sizeof(*aplr), lock);
}

static int readCapabilityRid(struct airo_info *ai, CapabilityRid *capr, int lock)
{
	return PC4500_readrid(ai, RID_CAPABILITIES, capr, sizeof(*capr), lock);
}

static int readStatsRid(struct airo_info*ai, StatsRid *sr, int rid, int lock)
{
	return PC4500_readrid(ai, rid, sr, sizeof(*sr), lock);
}

static void try_auto_wep(struct airo_info *ai)
{
	if (auto_wep && !(ai->flags & FLAG_RADIO_DOWN)) {
		ai->expires = RUN_AT(3*HZ);
		wake_up_interruptible(&ai->thr_wait);
	}
}

static int airo_open(struct net_device *dev) {
	struct airo_info *ai = dev->ml_priv;
	int rc = 0;

	if (test_bit(FLAG_FLASHING, &ai->flags))
		return -EIO;

	/* Make sure the card is configured.
	 * Wireless Extensions may postpone config changes until the card
	 * is open (to pipeline changes and speed-up card setup). If
	 * those changes are not yet commited, do it now - Jean II */
	if (test_bit(FLAG_COMMIT, &ai->flags)) {
		disable_MAC(ai, 1);
		writeConfigRid(ai, 1);
	}

	if (ai->wifidev != dev) {
		clear_bit(JOB_DIE, &ai->jobs);
		ai->airo_thread_task = kthread_run(airo_thread, dev, dev->name);
		if (IS_ERR(ai->airo_thread_task))
			return (int)PTR_ERR(ai->airo_thread_task);

		rc = request_irq(dev->irq, airo_interrupt, IRQF_SHARED,
			dev->name, dev);
		if (rc) {
			airo_print_err(dev->name,
				"register interrupt %d failed, rc %d",
				dev->irq, rc);
			set_bit(JOB_DIE, &ai->jobs);
			kthread_stop(ai->airo_thread_task);
			return rc;
		}

		/* Power on the MAC controller (which may have been disabled) */
		clear_bit(FLAG_RADIO_DOWN, &ai->flags);
		enable_interrupts(ai);

		try_auto_wep(ai);
	}
	enable_MAC(ai, 1);

	netif_start_queue(dev);
	return 0;
}

static netdev_tx_t mpi_start_xmit(struct sk_buff *skb,
					struct net_device *dev)
{
	int npacks, pending;
	unsigned long flags;
	struct airo_info *ai = dev->ml_priv;

	if (!skb) {
		airo_print_err(dev->name, "%s: skb == NULL!",__func__);
		return NETDEV_TX_OK;
	}
	npacks = skb_queue_len (&ai->txq);

	if (npacks >= MAXTXQ - 1) {
		netif_stop_queue (dev);
		if (npacks > MAXTXQ) {
			dev->stats.tx_fifo_errors++;
			return NETDEV_TX_BUSY;
		}
		skb_queue_tail (&ai->txq, skb);
		return NETDEV_TX_OK;
	}

	spin_lock_irqsave(&ai->aux_lock, flags);
	skb_queue_tail (&ai->txq, skb);
	pending = test_bit(FLAG_PENDING_XMIT, &ai->flags);
	spin_unlock_irqrestore(&ai->aux_lock,flags);
	netif_wake_queue (dev);

	if (pending == 0) {
		set_bit(FLAG_PENDING_XMIT, &ai->flags);
		mpi_send_packet (dev);
	}
	return NETDEV_TX_OK;
}

/*
 * @mpi_send_packet
 *
 * Attempt to transmit a packet. Can be called from interrupt
 * or transmit . return number of packets we tried to send
 */

static int mpi_send_packet (struct net_device *dev)
{
	struct sk_buff *skb;
	unsigned char *buffer;
	s16 len;
	__le16 *payloadLen;
	struct airo_info *ai = dev->ml_priv;
	u8 *sendbuf;

	/* get a packet to send */

	if ((skb = skb_dequeue(&ai->txq)) == NULL) {
		airo_print_err(dev->name,
			"%s: Dequeue'd zero in send_packet()",
			__func__);
		return 0;
	}

	/* check min length*/
	len = ETH_ZLEN < skb->len ? skb->len : ETH_ZLEN;
	buffer = skb->data;

	ai->txfids[0].tx_desc.offset = 0;
	ai->txfids[0].tx_desc.valid = 1;
	ai->txfids[0].tx_desc.eoc = 1;
	ai->txfids[0].tx_desc.len =len+sizeof(WifiHdr);

/*
 * Magic, the cards firmware needs a length count (2 bytes) in the host buffer
 * right after  TXFID_HDR.The TXFID_HDR contains the status short so payloadlen
 * is immediatly after it. ------------------------------------------------
 *                         |TXFIDHDR+STATUS|PAYLOADLEN|802.3HDR|PACKETDATA|
 *                         ------------------------------------------------
 */

	memcpy((char *)ai->txfids[0].virtual_host_addr,
		(char *)&wifictlhdr8023, sizeof(wifictlhdr8023));

	payloadLen = (__le16 *)(ai->txfids[0].virtual_host_addr +
		sizeof(wifictlhdr8023));
	sendbuf = ai->txfids[0].virtual_host_addr +
		sizeof(wifictlhdr8023) + 2 ;

	/*
	 * Firmware automaticly puts 802 header on so
	 * we don't need to account for it in the length
	 */
	if (test_bit(FLAG_MIC_CAPABLE, &ai->flags) && ai->micstats.enabled &&
		(ntohs(((__be16 *)buffer)[6]) != 0x888E)) {
		MICBuffer pMic;

		if (encapsulate(ai, (etherHead *)buffer, &pMic, len - sizeof(etherHead)) != SUCCESS)
			return ERROR;

		*payloadLen = cpu_to_le16(len-sizeof(etherHead)+sizeof(pMic));
		ai->txfids[0].tx_desc.len += sizeof(pMic);
		/* copy data into airo dma buffer */
		memcpy (sendbuf, buffer, sizeof(etherHead));
		buffer += sizeof(etherHead);
		sendbuf += sizeof(etherHead);
		memcpy (sendbuf, &pMic, sizeof(pMic));
		sendbuf += sizeof(pMic);
		memcpy (sendbuf, buffer, len - sizeof(etherHead));
	} else {
		*payloadLen = cpu_to_le16(len - sizeof(etherHead));

		dev->trans_start = jiffies;

		/* copy data into airo dma buffer */
		memcpy(sendbuf, buffer, len);
	}

	memcpy_toio(ai->txfids[0].card_ram_off,
		&ai->txfids[0].tx_desc, sizeof(TxFid));

	OUT4500(ai, EVACK, 8);

	dev_kfree_skb_any(skb);
	return 1;
}

static void get_tx_error(struct airo_info *ai, s32 fid)
{
	__le16 status;

	if (fid < 0)
		status = ((WifiCtlHdr *)ai->txfids[0].virtual_host_addr)->ctlhdr.status;
	else {
		if (bap_setup(ai, ai->fids[fid] & 0xffff, 4, BAP0) != SUCCESS)
			return;
		bap_read(ai, &status, 2, BAP0);
	}
	if (le16_to_cpu(status) & 2) /* Too many retries */
		ai->dev->stats.tx_aborted_errors++;
	if (le16_to_cpu(status) & 4) /* Transmit lifetime exceeded */
		ai->dev->stats.tx_heartbeat_errors++;
	if (le16_to_cpu(status) & 8) /* Aid fail */
		{ }
	if (le16_to_cpu(status) & 0x10) /* MAC disabled */
		ai->dev->stats.tx_carrier_errors++;
	if (le16_to_cpu(status) & 0x20) /* Association lost */
		{ }
	/* We produce a TXDROP event only for retry or lifetime
	 * exceeded, because that's the only status that really mean
	 * that this particular node went away.
	 * Other errors means that *we* screwed up. - Jean II */
	if ((le16_to_cpu(status) & 2) ||
	     (le16_to_cpu(status) & 4)) {
		union iwreq_data	wrqu;
		char junk[0x18];

		/* Faster to skip over useless data than to do
		 * another bap_setup(). We are at offset 0x6 and
		 * need to go to 0x18 and read 6 bytes - Jean II */
		bap_read(ai, (__le16 *) junk, 0x18, BAP0);

		/* Copy 802.11 dest address.
		 * We use the 802.11 header because the frame may
		 * not be 802.3 or may be mangled...
		 * In Ad-Hoc mode, it will be the node address.
		 * In managed mode, it will be most likely the AP addr
		 * User space will figure out how to convert it to
		 * whatever it needs (IP address or else).
		 * - Jean II */
		memcpy(wrqu.addr.sa_data, junk + 0x12, ETH_ALEN);
		wrqu.addr.sa_family = ARPHRD_ETHER;

		/* Send event to user space */
		wireless_send_event(ai->dev, IWEVTXDROP, &wrqu, NULL);
	}
}

static void airo_end_xmit(struct net_device *dev) {
	u16 status;
	int i;
	struct airo_info *priv = dev->ml_priv;
	struct sk_buff *skb = priv->xmit.skb;
	int fid = priv->xmit.fid;
	u32 *fids = priv->fids;

	clear_bit(JOB_XMIT, &priv->jobs);
	clear_bit(FLAG_PENDING_XMIT, &priv->flags);
	status = transmit_802_3_packet (priv, fids[fid], skb->data);
	up(&priv->sem);

	i = 0;
	if ( status == SUCCESS ) {
		dev->trans_start = jiffies;
		for (; i < MAX_FIDS / 2 && (priv->fids[i] & 0xffff0000); i++);
	} else {
		priv->fids[fid] &= 0xffff;
		dev->stats.tx_window_errors++;
	}
	if (i < MAX_FIDS / 2)
		netif_wake_queue(dev);
	dev_kfree_skb(skb);
}

static netdev_tx_t airo_start_xmit(struct sk_buff *skb,
					 struct net_device *dev)
{
	s16 len;
	int i, j;
	struct airo_info *priv = dev->ml_priv;
	u32 *fids = priv->fids;

	if ( skb == NULL ) {
		airo_print_err(dev->name, "%s: skb == NULL!", __func__);
		return NETDEV_TX_OK;
	}

	/* Find a vacant FID */
	for( i = 0; i < MAX_FIDS / 2 && (fids[i] & 0xffff0000); i++ );
	for( j = i + 1; j < MAX_FIDS / 2 && (fids[j] & 0xffff0000); j++ );

	if ( j >= MAX_FIDS / 2 ) {
		netif_stop_queue(dev);

		if (i == MAX_FIDS / 2) {
			dev->stats.tx_fifo_errors++;
			return NETDEV_TX_BUSY;
		}
	}
	/* check min length*/
	len = ETH_ZLEN < skb->len ? skb->len : ETH_ZLEN;
        /* Mark fid as used & save length for later */
	fids[i] |= (len << 16);
	priv->xmit.skb = skb;
	priv->xmit.fid = i;
	if (down_trylock(&priv->sem) != 0) {
		set_bit(FLAG_PENDING_XMIT, &priv->flags);
		netif_stop_queue(dev);
		set_bit(JOB_XMIT, &priv->jobs);
		wake_up_interruptible(&priv->thr_wait);
	} else
		airo_end_xmit(dev);
	return NETDEV_TX_OK;
}

static void airo_end_xmit11(struct net_device *dev) {
	u16 status;
	int i;
	struct airo_info *priv = dev->ml_priv;
	struct sk_buff *skb = priv->xmit11.skb;
	int fid = priv->xmit11.fid;
	u32 *fids = priv->fids;

	clear_bit(JOB_XMIT11, &priv->jobs);
	clear_bit(FLAG_PENDING_XMIT11, &priv->flags);
	status = transmit_802_11_packet (priv, fids[fid], skb->data);
	up(&priv->sem);

	i = MAX_FIDS / 2;
	if ( status == SUCCESS ) {
		dev->trans_start = jiffies;
		for (; i < MAX_FIDS && (priv->fids[i] & 0xffff0000); i++);
	} else {
		priv->fids[fid] &= 0xffff;
		dev->stats.tx_window_errors++;
	}
	if (i < MAX_FIDS)
		netif_wake_queue(dev);
	dev_kfree_skb(skb);
}

static netdev_tx_t airo_start_xmit11(struct sk_buff *skb,
					   struct net_device *dev)
{
	s16 len;
	int i, j;
	struct airo_info *priv = dev->ml_priv;
	u32 *fids = priv->fids;

	if (test_bit(FLAG_MPI, &priv->flags)) {
		/* Not implemented yet for MPI350 */
		netif_stop_queue(dev);
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	if ( skb == NULL ) {
		airo_print_err(dev->name, "%s: skb == NULL!", __func__);
		return NETDEV_TX_OK;
	}

	/* Find a vacant FID */
	for( i = MAX_FIDS / 2; i < MAX_FIDS && (fids[i] & 0xffff0000); i++ );
	for( j = i + 1; j < MAX_FIDS && (fids[j] & 0xffff0000); j++ );

	if ( j >= MAX_FIDS ) {
		netif_stop_queue(dev);

		if (i == MAX_FIDS) {
			dev->stats.tx_fifo_errors++;
			return NETDEV_TX_BUSY;
		}
	}
	/* check min length*/
	len = ETH_ZLEN < skb->len ? skb->len : ETH_ZLEN;
        /* Mark fid as used & save length for later */
	fids[i] |= (len << 16);
	priv->xmit11.skb = skb;
	priv->xmit11.fid = i;
	if (down_trylock(&priv->sem) != 0) {
		set_bit(FLAG_PENDING_XMIT11, &priv->flags);
		netif_stop_queue(dev);
		set_bit(JOB_XMIT11, &priv->jobs);
		wake_up_interruptible(&priv->thr_wait);
	} else
		airo_end_xmit11(dev);
	return NETDEV_TX_OK;
}

static void airo_read_stats(struct net_device *dev)
{
	struct airo_info *ai = dev->ml_priv;
	StatsRid stats_rid;
	__le32 *vals = stats_rid.vals;

	clear_bit(JOB_STATS, &ai->jobs);
	if (ai->power.event) {
		up(&ai->sem);
		return;
	}
	readStatsRid(ai, &stats_rid, RID_STATS, 0);
	up(&ai->sem);

	dev->stats.rx_packets = le32_to_cpu(vals[43]) + le32_to_cpu(vals[44]) +
			       le32_to_cpu(vals[45]);
	dev->stats.tx_packets = le32_to_cpu(vals[39]) + le32_to_cpu(vals[40]) +
			       le32_to_cpu(vals[41]);
	dev->stats.rx_bytes = le32_to_cpu(vals[92]);
	dev->stats.tx_bytes = le32_to_cpu(vals[91]);
	dev->stats.rx_errors = le32_to_cpu(vals[0]) + le32_to_cpu(vals[2]) +
			      le32_to_cpu(vals[3]) + le32_to_cpu(vals[4]);
	dev->stats.tx_errors = le32_to_cpu(vals[42]) +
			      dev->stats.tx_fifo_errors;
	dev->stats.multicast = le32_to_cpu(vals[43]);
	dev->stats.collisions = le32_to_cpu(vals[89]);

	/* detailed rx_errors: */
	dev->stats.rx_length_errors = le32_to_cpu(vals[3]);
	dev->stats.rx_crc_errors = le32_to_cpu(vals[4]);
	dev->stats.rx_frame_errors = le32_to_cpu(vals[2]);
	dev->stats.rx_fifo_errors = le32_to_cpu(vals[0]);
}

static struct net_device_stats *airo_get_stats(struct net_device *dev)
{
	struct airo_info *local =  dev->ml_priv;

	if (!test_bit(JOB_STATS, &local->jobs)) {
		/* Get stats out of the card if available */
		if (down_trylock(&local->sem) != 0) {
			set_bit(JOB_STATS, &local->jobs);
			wake_up_interruptible(&local->thr_wait);
		} else
			airo_read_stats(dev);
	}

	return &dev->stats;
}

static void airo_set_promisc(struct airo_info *ai) {
	Cmd cmd;
	Resp rsp;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd=CMD_SETMODE;
	clear_bit(JOB_PROMISC, &ai->jobs);
	cmd.parm0=(ai->flags&IFF_PROMISC) ? PROMISC : NOPROMISC;
	issuecommand(ai, &cmd, &rsp);
	up(&ai->sem);
}

static void airo_set_multicast_list(struct net_device *dev) {
	struct airo_info *ai = dev->ml_priv;

	if ((dev->flags ^ ai->flags) & IFF_PROMISC) {
		change_bit(FLAG_PROMISC, &ai->flags);
		if (down_trylock(&ai->sem) != 0) {
			set_bit(JOB_PROMISC, &ai->jobs);
			wake_up_interruptible(&ai->thr_wait);
		} else
			airo_set_promisc(ai);
	}

	if ((dev->flags&IFF_ALLMULTI)||dev->mc_count>0) {
		/* Turn on multicast.  (Should be already setup...) */
	}
}

static int airo_set_mac_address(struct net_device *dev, void *p)
{
	struct airo_info *ai = dev->ml_priv;
	struct sockaddr *addr = p;

	readConfigRid(ai, 1);
	memcpy (ai->config.macAddr, addr->sa_data, dev->addr_len);
	set_bit (FLAG_COMMIT, &ai->flags);
	disable_MAC(ai, 1);
	writeConfigRid (ai, 1);
	enable_MAC(ai, 1);
	memcpy (ai->dev->dev_addr, addr->sa_data, dev->addr_len);
	if (ai->wifidev)
		memcpy (ai->wifidev->dev_addr, addr->sa_data, dev->addr_len);
	return 0;
}

static int airo_change_mtu(struct net_device *dev, int new_mtu)
{
	if ((new_mtu < 68) || (new_mtu > 2400))
		return -EINVAL;
	dev->mtu = new_mtu;
	return 0;
}

static LIST_HEAD(airo_devices);

static void add_airo_dev(struct airo_info *ai)
{
	/* Upper layers already keep track of PCI devices,
	 * so we only need to remember our non-PCI cards. */
	if (!ai->pci)
		list_add_tail(&ai->dev_list, &airo_devices);
}

static void del_airo_dev(struct airo_info *ai)
{
	if (!ai->pci)
		list_del(&ai->dev_list);
}

static int airo_close(struct net_device *dev) {
	struct airo_info *ai = dev->ml_priv;

	netif_stop_queue(dev);

	if (ai->wifidev != dev) {
#ifdef POWER_ON_DOWN
		/* Shut power to the card. The idea is that the user can save
		 * power when he doesn't need the card with "ifconfig down".
		 * That's the method that is most friendly towards the network
		 * stack (i.e. the network stack won't try to broadcast
		 * anything on the interface and routes are gone. Jean II */
		set_bit(FLAG_RADIO_DOWN, &ai->flags);
		disable_MAC(ai, 1);
#endif
		disable_interrupts( ai );

		free_irq(dev->irq, dev);

		set_bit(JOB_DIE, &ai->jobs);
		kthread_stop(ai->airo_thread_task);
	}
	return 0;
}

void stop_airo_card( struct net_device *dev, int freeres )
{
	struct airo_info *ai = dev->ml_priv;

	set_bit(FLAG_RADIO_DOWN, &ai->flags);
	disable_MAC(ai, 1);
	disable_interrupts(ai);
	takedown_proc_entry( dev, ai );
	if (test_bit(FLAG_REGISTERED, &ai->flags)) {
		unregister_netdev( dev );
		if (ai->wifidev) {
			unregister_netdev(ai->wifidev);
			free_netdev(ai->wifidev);
			ai->wifidev = NULL;
		}
		clear_bit(FLAG_REGISTERED, &ai->flags);
	}
	/*
	 * Clean out tx queue
	 */
	if (test_bit(FLAG_MPI, &ai->flags) && !skb_queue_empty(&ai->txq)) {
		struct sk_buff *skb = NULL;
		for (;(skb = skb_dequeue(&ai->txq));)
			dev_kfree_skb(skb);
	}

	airo_networks_free (ai);

	kfree(ai->flash);
	kfree(ai->rssi);
	kfree(ai->APList);
	kfree(ai->SSID);
	if (freeres) {
		/* PCMCIA frees this stuff, so only for PCI and ISA */
	        release_region( dev->base_addr, 64 );
		if (test_bit(FLAG_MPI, &ai->flags)) {
			if (ai->pci)
				mpi_unmap_card(ai->pci);
			if (ai->pcimem)
				iounmap(ai->pcimem);
			if (ai->pciaux)
				iounmap(ai->pciaux);
			pci_free_consistent(ai->pci, PCI_SHARED_LEN,
				ai->shared, ai->shared_dma);
		}
        }
	crypto_free_cipher(ai->tfm);
	del_airo_dev(ai);
	free_netdev( dev );
}

EXPORT_SYMBOL(stop_airo_card);

static int wll_header_parse(const struct sk_buff *skb, unsigned char *haddr)
{
	memcpy(haddr, skb_mac_header(skb) + 10, ETH_ALEN);
	return ETH_ALEN;
}

static void mpi_unmap_card(struct pci_dev *pci)
{
	unsigned long mem_start = pci_resource_start(pci, 1);
	unsigned long mem_len = pci_resource_len(pci, 1);
	unsigned long aux_start = pci_resource_start(pci, 2);
	unsigned long aux_len = AUXMEMSIZE;

	release_mem_region(aux_start, aux_len);
	release_mem_region(mem_start, mem_len);
}

/*************************************************************
 *  This routine assumes that descriptors have been setup .
 *  Run at insmod time or after reset  when the decriptors
 *  have been initialized . Returns 0 if all is well nz
 *  otherwise . Does not allocate memory but sets up card
 *  using previously allocated descriptors.
 */
static int mpi_init_descriptors (struct airo_info *ai)
{
	Cmd cmd;
	Resp rsp;
	int i;
	int rc = SUCCESS;

	/* Alloc  card RX descriptors */
	netif_stop_queue(ai->dev);

	memset(&rsp,0,sizeof(rsp));
	memset(&cmd,0,sizeof(cmd));

	cmd.cmd = CMD_ALLOCATEAUX;
	cmd.parm0 = FID_RX;
	cmd.parm1 = (ai->rxfids[0].card_ram_off - ai->pciaux);
	cmd.parm2 = MPI_MAX_FIDS;
	rc=issuecommand(ai, &cmd, &rsp);
	if (rc != SUCCESS) {
		airo_print_err(ai->dev->name, "Couldn't allocate RX FID");
		return rc;
	}

	for (i=0; i<MPI_MAX_FIDS; i++) {
		memcpy_toio(ai->rxfids[i].card_ram_off,
			&ai->rxfids[i].rx_desc, sizeof(RxFid));
	}

	/* Alloc card TX descriptors */

	memset(&rsp,0,sizeof(rsp));
	memset(&cmd,0,sizeof(cmd));

	cmd.cmd = CMD_ALLOCATEAUX;
	cmd.parm0 = FID_TX;
	cmd.parm1 = (ai->txfids[0].card_ram_off - ai->pciaux);
	cmd.parm2 = MPI_MAX_FIDS;

	for (i=0; i<MPI_MAX_FIDS; i++) {
		ai->txfids[i].tx_desc.valid = 1;
		memcpy_toio(ai->txfids[i].card_ram_off,
			&ai->txfids[i].tx_desc, sizeof(TxFid));
	}
	ai->txfids[i-1].tx_desc.eoc = 1; /* Last descriptor has EOC set */

	rc=issuecommand(ai, &cmd, &rsp);
	if (rc != SUCCESS) {
		airo_print_err(ai->dev->name, "Couldn't allocate TX FID");
		return rc;
	}

	/* Alloc card Rid descriptor */
	memset(&rsp,0,sizeof(rsp));
	memset(&cmd,0,sizeof(cmd));

	cmd.cmd = CMD_ALLOCATEAUX;
	cmd.parm0 = RID_RW;
	cmd.parm1 = (ai->config_desc.card_ram_off - ai->pciaux);
	cmd.parm2 = 1; /* Magic number... */
	rc=issuecommand(ai, &cmd, &rsp);
	if (rc != SUCCESS) {
		airo_print_err(ai->dev->name, "Couldn't allocate RID");
		return rc;
	}

	memcpy_toio(ai->config_desc.card_ram_off,
		&ai->config_desc.rid_desc, sizeof(Rid));

	return rc;
}

/*
 * We are setting up three things here:
 * 1) Map AUX memory for descriptors: Rid, TxFid, or RxFid.
 * 2) Map PCI memory for issueing commands.
 * 3) Allocate memory (shared) to send and receive ethernet frames.
 */
static int mpi_map_card(struct airo_info *ai, struct pci_dev *pci)
{
	unsigned long mem_start, mem_len, aux_start, aux_len;
	int rc = -1;
	int i;
	dma_addr_t busaddroff;
	unsigned char *vpackoff;
	unsigned char __iomem *pciaddroff;

	mem_start = pci_resource_start(pci, 1);
	mem_len = pci_resource_len(pci, 1);
	aux_start = pci_resource_start(pci, 2);
	aux_len = AUXMEMSIZE;

	if (!request_mem_region(mem_start, mem_len, DRV_NAME)) {
		airo_print_err("", "Couldn't get region %x[%x]",
			(int)mem_start, (int)mem_len);
		goto out;
	}
	if (!request_mem_region(aux_start, aux_len, DRV_NAME)) {
		airo_print_err("", "Couldn't get region %x[%x]",
			(int)aux_start, (int)aux_len);
		goto free_region1;
	}

	ai->pcimem = ioremap(mem_start, mem_len);
	if (!ai->pcimem) {
		airo_print_err("", "Couldn't map region %x[%x]",
			(int)mem_start, (int)mem_len);
		goto free_region2;
	}
	ai->pciaux = ioremap(aux_start, aux_len);
	if (!ai->pciaux) {
		airo_print_err("", "Couldn't map region %x[%x]",
			(int)aux_start, (int)aux_len);
		goto free_memmap;
	}

	/* Reserve PKTSIZE for each fid and 2K for the Rids */
	ai->shared = pci_alloc_consistent(pci, PCI_SHARED_LEN, &ai->shared_dma);
	if (!ai->shared) {
		airo_print_err("", "Couldn't alloc_consistent %d",
			PCI_SHARED_LEN);
		goto free_auxmap;
	}

	/*
	 * Setup descriptor RX, TX, CONFIG
	 */
	busaddroff = ai->shared_dma;
	pciaddroff = ai->pciaux + AUX_OFFSET;
	vpackoff   = ai->shared;

	/* RX descriptor setup */
	for(i = 0; i < MPI_MAX_FIDS; i++) {
		ai->rxfids[i].pending = 0;
		ai->rxfids[i].card_ram_off = pciaddroff;
		ai->rxfids[i].virtual_host_addr = vpackoff;
		ai->rxfids[i].rx_desc.host_addr = busaddroff;
		ai->rxfids[i].rx_desc.valid = 1;
		ai->rxfids[i].rx_desc.len = PKTSIZE;
		ai->rxfids[i].rx_desc.rdy = 0;

		pciaddroff += sizeof(RxFid);
		busaddroff += PKTSIZE;
		vpackoff   += PKTSIZE;
	}

	/* TX descriptor setup */
	for(i = 0; i < MPI_MAX_FIDS; i++) {
		ai->txfids[i].card_ram_off = pciaddroff;
		ai->txfids[i].virtual_host_addr = vpackoff;
		ai->txfids[i].tx_desc.valid = 1;
		ai->txfids[i].tx_desc.host_addr = busaddroff;
		memcpy(ai->txfids[i].virtual_host_addr,
			&wifictlhdr8023, sizeof(wifictlhdr8023));

		pciaddroff += sizeof(TxFid);
		busaddroff += PKTSIZE;
		vpackoff   += PKTSIZE;
	}
	ai->txfids[i-1].tx_desc.eoc = 1; /* Last descriptor has EOC set */

	/* Rid descriptor setup */
	ai->config_desc.card_ram_off = pciaddroff;
	ai->config_desc.virtual_host_addr = vpackoff;
	ai->config_desc.rid_desc.host_addr = busaddroff;
	ai->ridbus = busaddroff;
	ai->config_desc.rid_desc.rid = 0;
	ai->config_desc.rid_desc.len = RIDSIZE;
	ai->config_desc.rid_desc.valid = 1;
	pciaddroff += sizeof(Rid);
	busaddroff += RIDSIZE;
	vpackoff   += RIDSIZE;

	/* Tell card about descriptors */
	if (mpi_init_descriptors (ai) != SUCCESS)
		goto free_shared;

	return 0;
 free_shared:
	pci_free_consistent(pci, PCI_SHARED_LEN, ai->shared, ai->shared_dma);
 free_auxmap:
	iounmap(ai->pciaux);
 free_memmap:
	iounmap(ai->pcimem);
 free_region2:
	release_mem_region(aux_start, aux_len);
 free_region1:
	release_mem_region(mem_start, mem_len);
 out:
	return rc;
}

static const struct header_ops airo_header_ops = {
	.parse = wll_header_parse,
};

static const struct net_device_ops airo11_netdev_ops = {
	.ndo_open 		= airo_open,
	.ndo_stop 		= airo_close,
	.ndo_start_xmit 	= airo_start_xmit11,
	.ndo_get_stats 		= airo_get_stats,
	.ndo_set_mac_address	= airo_set_mac_address,
	.ndo_do_ioctl		= airo_ioctl,
	.ndo_change_mtu		= airo_change_mtu,
};

static void wifi_setup(struct net_device *dev)
{
	dev->netdev_ops = &airo11_netdev_ops;
	dev->header_ops = &airo_header_ops;
	dev->wireless_handlers = &airo_handler_def;

	dev->type               = ARPHRD_IEEE80211;
	dev->hard_header_len    = ETH_HLEN;
	dev->mtu                = AIRO_DEF_MTU;
	dev->addr_len           = ETH_ALEN;
	dev->tx_queue_len       = 100; 

	memset(dev->broadcast,0xFF, ETH_ALEN);

	dev->flags              = IFF_BROADCAST|IFF_MULTICAST;
}

static struct net_device *init_wifidev(struct airo_info *ai,
					struct net_device *ethdev)
{
	int err;
	struct net_device *dev = alloc_netdev(0, "wifi%d", wifi_setup);
	if (!dev)
		return NULL;
	dev->ml_priv = ethdev->ml_priv;
	dev->irq = ethdev->irq;
	dev->base_addr = ethdev->base_addr;
	dev->wireless_data = ethdev->wireless_data;
	SET_NETDEV_DEV(dev, ethdev->dev.parent);
	memcpy(dev->dev_addr, ethdev->dev_addr, dev->addr_len);
	err = register_netdev(dev);
	if (err<0) {
		free_netdev(dev);
		return NULL;
	}
	return dev;
}

static int reset_card( struct net_device *dev , int lock) {
	struct airo_info *ai = dev->ml_priv;

	if (lock && down_interruptible(&ai->sem))
		return -1;
	waitbusy (ai);
	OUT4500(ai,COMMAND,CMD_SOFTRESET);
	msleep(200);
	waitbusy (ai);
	msleep(200);
	if (lock)
		up(&ai->sem);
	return 0;
}

#define AIRO_MAX_NETWORK_COUNT	64
static int airo_networks_allocate(struct airo_info *ai)
{
	if (ai->networks)
		return 0;

	ai->networks =
	    kzalloc(AIRO_MAX_NETWORK_COUNT * sizeof(BSSListElement),
		    GFP_KERNEL);
	if (!ai->networks) {
		airo_print_warn("", "Out of memory allocating beacons");
		return -ENOMEM;
	}

	return 0;
}

static void airo_networks_free(struct airo_info *ai)
{
	kfree(ai->networks);
	ai->networks = NULL;
}

static void airo_networks_initialize(struct airo_info *ai)
{
	int i;

	INIT_LIST_HEAD(&ai->network_free_list);
	INIT_LIST_HEAD(&ai->network_list);
	for (i = 0; i < AIRO_MAX_NETWORK_COUNT; i++)
		list_add_tail(&ai->networks[i].list,
			      &ai->network_free_list);
}

static const struct net_device_ops airo_netdev_ops = {
	.ndo_open		= airo_open,
	.ndo_stop		= airo_close,
	.ndo_start_xmit		= airo_start_xmit,
	.ndo_get_stats		= airo_get_stats,
	.ndo_set_multicast_list	= airo_set_multicast_list,
	.ndo_set_mac_address	= airo_set_mac_address,
	.ndo_do_ioctl		= airo_ioctl,
	.ndo_change_mtu		= airo_change_mtu,
	.ndo_validate_addr	= eth_validate_addr,
};

static const struct net_device_ops mpi_netdev_ops = {
	.ndo_open		= airo_open,
	.ndo_stop		= airo_close,
	.ndo_start_xmit		= mpi_start_xmit,
	.ndo_get_stats		= airo_get_stats,
	.ndo_set_multicast_list	= airo_set_multicast_list,
	.ndo_set_mac_address	= airo_set_mac_address,
	.ndo_do_ioctl		= airo_ioctl,
	.ndo_change_mtu		= airo_change_mtu,
	.ndo_validate_addr	= eth_validate_addr,
};


static struct net_device *_init_airo_card( unsigned short irq, int port,
					   int is_pcmcia, struct pci_dev *pci,
					   struct device *dmdev )
{
	struct net_device *dev;
	struct airo_info *ai;
	int i, rc;
	CapabilityRid cap_rid;

	/* Create the network device object. */
	dev = alloc_netdev(sizeof(*ai), "", ether_setup);
	if (!dev) {
		airo_print_err("", "Couldn't alloc_etherdev");
		return NULL;
	}

	ai = dev->ml_priv = netdev_priv(dev);
	ai->wifidev = NULL;
	ai->flags = 1 << FLAG_RADIO_DOWN;
	ai->jobs = 0;
	ai->dev = dev;
	if (pci && (pci->device == 0x5000 || pci->device == 0xa504)) {
		airo_print_dbg("", "Found an MPI350 card");
		set_bit(FLAG_MPI, &ai->flags);
	}
	spin_lock_init(&ai->aux_lock);
	sema_init(&ai->sem, 1);
	ai->config.len = 0;
	ai->pci = pci;
	init_waitqueue_head (&ai->thr_wait);
	ai->tfm = NULL;
	add_airo_dev(ai);

	if (airo_networks_allocate (ai))
		goto err_out_free;
	airo_networks_initialize (ai);

	skb_queue_head_init (&ai->txq);

	/* The Airo-specific entries in the device structure. */
	if (test_bit(FLAG_MPI,&ai->flags))
		dev->netdev_ops = &mpi_netdev_ops;
	else
		dev->netdev_ops = &airo_netdev_ops;
	dev->wireless_handlers = &airo_handler_def;
	ai->wireless_data.spy_data = &ai->spy_data;
	dev->wireless_data = &ai->wireless_data;
	dev->irq = irq;
	dev->base_addr = port;

	SET_NETDEV_DEV(dev, dmdev);

	reset_card (dev, 1);
	msleep(400);

	if (!is_pcmcia) {
		if (!request_region(dev->base_addr, 64, DRV_NAME)) {
			rc = -EBUSY;
			airo_print_err(dev->name, "Couldn't request region");
			goto err_out_nets;
		}
	}

	if (test_bit(FLAG_MPI,&ai->flags)) {
		if (mpi_map_card(ai, pci)) {
			airo_print_err("", "Could not map memory");
			goto err_out_res;
		}
	}

	if (probe) {
		if (setup_card(ai, dev->dev_addr, 1) != SUCCESS) {
			airo_print_err(dev->name, "MAC could not be enabled" );
			rc = -EIO;
			goto err_out_map;
		}
	} else if (!test_bit(FLAG_MPI,&ai->flags)) {
		ai->bap_read = fast_bap_read;
		set_bit(FLAG_FLASHING, &ai->flags);
	}

	strcpy(dev->name, "eth%d");
	rc = register_netdev(dev);
	if (rc) {
		airo_print_err(dev->name, "Couldn't register_netdev");
		goto err_out_map;
	}
	ai->wifidev = init_wifidev(ai, dev);
	if (!ai->wifidev)
		goto err_out_reg;

	rc = readCapabilityRid(ai, &cap_rid, 1);
	if (rc != SUCCESS) {
		rc = -EIO;
		goto err_out_wifi;
	}
	/* WEP capability discovery */
	ai->wep_capable = (cap_rid.softCap & cpu_to_le16(0x02)) ? 1 : 0;
	ai->max_wep_idx = (cap_rid.softCap & cpu_to_le16(0x80)) ? 3 : 0;

	airo_print_info(dev->name, "Firmware version %x.%x.%02x",
	                ((le16_to_cpu(cap_rid.softVer) >> 8) & 0xF),
	                (le16_to_cpu(cap_rid.softVer) & 0xFF),
	                le16_to_cpu(cap_rid.softSubVer));

	/* Test for WPA support */
	/* Only firmware versions 5.30.17 or better can do WPA */
	if (le16_to_cpu(cap_rid.softVer) > 0x530
	 || (le16_to_cpu(cap_rid.softVer) == 0x530
	      && le16_to_cpu(cap_rid.softSubVer) >= 17)) {
		airo_print_info(ai->dev->name, "WPA supported.");

		set_bit(FLAG_WPA_CAPABLE, &ai->flags);
		ai->bssListFirst = RID_WPA_BSSLISTFIRST;
		ai->bssListNext = RID_WPA_BSSLISTNEXT;
		ai->bssListRidLen = sizeof(BSSListRid);
	} else {
		airo_print_info(ai->dev->name, "WPA unsupported with firmware "
			"versions older than 5.30.17.");

		ai->bssListFirst = RID_BSSLISTFIRST;
		ai->bssListNext = RID_BSSLISTNEXT;
		ai->bssListRidLen = sizeof(BSSListRid) - sizeof(BSSListRidExtra);
	}

	set_bit(FLAG_REGISTERED,&ai->flags);
	airo_print_info(dev->name, "MAC enabled %pM", dev->dev_addr);

	/* Allocate the transmit buffers */
	if (probe && !test_bit(FLAG_MPI,&ai->flags))
		for( i = 0; i < MAX_FIDS; i++ )
			ai->fids[i] = transmit_allocate(ai,AIRO_DEF_MTU,i>=MAX_FIDS/2);

	if (setup_proc_entry(dev, dev->ml_priv) < 0)
		goto err_out_wifi;

	return dev;

err_out_wifi:
	unregister_netdev(ai->wifidev);
	free_netdev(ai->wifidev);
err_out_reg:
	unregister_netdev(dev);
err_out_map:
	if (test_bit(FLAG_MPI,&ai->flags) && pci) {
		pci_free_consistent(pci, PCI_SHARED_LEN, ai->shared, ai->shared_dma);
		iounmap(ai->pciaux);
		iounmap(ai->pcimem);
		mpi_unmap_card(ai->pci);
	}
err_out_res:
	if (!is_pcmcia)
	        release_region( dev->base_addr, 64 );
err_out_nets:
	airo_networks_free(ai);
	del_airo_dev(ai);
err_out_free:
	free_netdev(dev);
	return NULL;
}

struct net_device *init_airo_card( unsigned short irq, int port, int is_pcmcia,
				  struct device *dmdev)
{
	return _init_airo_card ( irq, port, is_pcmcia, NULL, dmdev);
}

EXPORT_SYMBOL(init_airo_card);

static int waitbusy (struct airo_info *ai) {
	int delay = 0;
	while ((IN4500(ai, COMMAND) & COMMAND_BUSY) && (delay < 10000)) {
		udelay (10);
		if ((++delay % 20) == 0)
			OUT4500(ai, EVACK, EV_CLEARCOMMANDBUSY);
	}
	return delay < 10000;
}

int reset_airo_card( struct net_device *dev )
{
	int i;
	struct airo_info *ai = dev->ml_priv;

	if (reset_card (dev, 1))
		return -1;

	if ( setup_card(ai, dev->dev_addr, 1 ) != SUCCESS ) {
		airo_print_err(dev->name, "MAC could not be enabled");
		return -1;
	}
	airo_print_info(dev->name, "MAC enabled %pM", dev->dev_addr);
	/* Allocate the transmit buffers if needed */
	if (!test_bit(FLAG_MPI,&ai->flags))
		for( i = 0; i < MAX_FIDS; i++ )
			ai->fids[i] = transmit_allocate (ai,AIRO_DEF_MTU,i>=MAX_FIDS/2);

	enable_interrupts( ai );
	netif_wake_queue(dev);
	return 0;
}

EXPORT_SYMBOL(reset_airo_card);

static void airo_send_event(struct net_device *dev) {
	struct airo_info *ai = dev->ml_priv;
	union iwreq_data wrqu;
	StatusRid status_rid;

	clear_bit(JOB_EVENT, &ai->jobs);
	PC4500_readrid(ai, RID_STATUS, &status_rid, sizeof(status_rid), 0);
	up(&ai->sem);
	wrqu.data.length = 0;
	wrqu.data.flags = 0;
	memcpy(wrqu.ap_addr.sa_data, status_rid.bssid[0], ETH_ALEN);
	wrqu.ap_addr.sa_family = ARPHRD_ETHER;

	/* Send event to user space */
	wireless_send_event(dev, SIOCGIWAP, &wrqu, NULL);
}

static void airo_process_scan_results (struct airo_info *ai) {
	union iwreq_data	wrqu;
	BSSListRid bss;
	int rc;
	BSSListElement * loop_net;
	BSSListElement * tmp_net;

	/* Blow away current list of scan results */
	list_for_each_entry_safe (loop_net, tmp_net, &ai->network_list, list) {
		list_move_tail (&loop_net->list, &ai->network_free_list);
		/* Don't blow away ->list, just BSS data */
		memset (loop_net, 0, sizeof (loop_net->bss));
	}

	/* Try to read the first entry of the scan result */
	rc = PC4500_readrid(ai, ai->bssListFirst, &bss, ai->bssListRidLen, 0);
	if((rc) || (bss.index == cpu_to_le16(0xffff))) {
		/* No scan results */
		goto out;
	}

	/* Read and parse all entries */
	tmp_net = NULL;
	while((!rc) && (bss.index != cpu_to_le16(0xffff))) {
		/* Grab a network off the free list */
		if (!list_empty(&ai->network_free_list)) {
			tmp_net = list_entry(ai->network_free_list.next,
					    BSSListElement, list);
			list_del(ai->network_free_list.next);
		}

		if (tmp_net != NULL) {
			memcpy(tmp_net, &bss, sizeof(tmp_net->bss));
			list_add_tail(&tmp_net->list, &ai->network_list);
			tmp_net = NULL;
		}

		/* Read next entry */
		rc = PC4500_readrid(ai, ai->bssListNext,
				    &bss, ai->bssListRidLen, 0);
	}

out:
	ai->scan_timeout = 0;
	clear_bit(JOB_SCAN_RESULTS, &ai->jobs);
	up(&ai->sem);

	/* Send an empty event to user space.
	 * We don't send the received data on
	 * the event because it would require
	 * us to do complex transcoding, and
	 * we want to minimise the work done in
	 * the irq handler. Use a request to
	 * extract the data - Jean II */
	wrqu.data.length = 0;
	wrqu.data.flags = 0;
	wireless_send_event(ai->dev, SIOCGIWSCAN, &wrqu, NULL);
}

static int airo_thread(void *data) {
	struct net_device *dev = data;
	struct airo_info *ai = dev->ml_priv;
	int locked;

	set_freezable();
	while(1) {
		/* make swsusp happy with our thread */
		try_to_freeze();

		if (test_bit(JOB_DIE, &ai->jobs))
			break;

		if (ai->jobs) {
			locked = down_interruptible(&ai->sem);
		} else {
			wait_queue_t wait;

			init_waitqueue_entry(&wait, current);
			add_wait_queue(&ai->thr_wait, &wait);
			for (;;) {
				set_current_state(TASK_INTERRUPTIBLE);
				if (ai->jobs)
					break;
				if (ai->expires || ai->scan_timeout) {
					if (ai->scan_timeout &&
							time_after_eq(jiffies,ai->scan_timeout)){
						set_bit(JOB_SCAN_RESULTS, &ai->jobs);
						break;
					} else if (ai->expires &&
							time_after_eq(jiffies,ai->expires)){
						set_bit(JOB_AUTOWEP, &ai->jobs);
						break;
					}
					if (!kthread_should_stop() &&
					    !freezing(current)) {
						unsigned long wake_at;
						if (!ai->expires || !ai->scan_timeout) {
							wake_at = max(ai->expires,
								ai->scan_timeout);
						} else {
							wake_at = min(ai->expires,
								ai->scan_timeout);
						}
						schedule_timeout(wake_at - jiffies);
						continue;
					}
				} else if (!kthread_should_stop() &&
					   !freezing(current)) {
					schedule();
					continue;
				}
				break;
			}
			current->state = TASK_RUNNING;
			remove_wait_queue(&ai->thr_wait, &wait);
			locked = 1;
		}

		if (locked)
			continue;

		if (test_bit(JOB_DIE, &ai->jobs)) {
			up(&ai->sem);
			break;
		}

		if (ai->power.event || test_bit(FLAG_FLASHING, &ai->flags)) {
			up(&ai->sem);
			continue;
		}

		if (test_bit(JOB_XMIT, &ai->jobs))
			airo_end_xmit(dev);
		else if (test_bit(JOB_XMIT11, &ai->jobs))
			airo_end_xmit11(dev);
		else if (test_bit(JOB_STATS, &ai->jobs))
			airo_read_stats(dev);
		else if (test_bit(JOB_WSTATS, &ai->jobs))
			airo_read_wireless_stats(ai);
		else if (test_bit(JOB_PROMISC, &ai->jobs))
			airo_set_promisc(ai);
		else if (test_bit(JOB_MIC, &ai->jobs))
			micinit(ai);
		else if (test_bit(JOB_EVENT, &ai->jobs))
			airo_send_event(dev);
		else if (test_bit(JOB_AUTOWEP, &ai->jobs))
			timer_func(dev);
		else if (test_bit(JOB_SCAN_RESULTS, &ai->jobs))
			airo_process_scan_results(ai);
		else  /* Shouldn't get here, but we make sure to unlock */
			up(&ai->sem);
	}

	return 0;
}

static int header_len(__le16 ctl)
{
	u16 fc = le16_to_cpu(ctl);
	switch (fc & 0xc) {
	case 4:
		if ((fc & 0xe0) == 0xc0)
			return 10;	/* one-address control packet */
		return 16;	/* two-address control packet */
	case 8:
		if ((fc & 0x300) == 0x300)
			return 30;	/* WDS packet */
	}
	return 24;
}

static void airo_handle_cisco_mic(struct airo_info *ai)
{
	if (test_bit(FLAG_MIC_CAPABLE, &ai->flags)) {
		set_bit(JOB_MIC, &ai->jobs);
		wake_up_interruptible(&ai->thr_wait);
	}
}

/* Airo Status codes */
#define STAT_NOBEACON	0x8000 /* Loss of sync - missed beacons */
#define STAT_MAXRETRIES	0x8001 /* Loss of sync - max retries */
#define STAT_MAXARL	0x8002 /* Loss of sync - average retry level exceeded*/
#define STAT_FORCELOSS	0x8003 /* Loss of sync - host request */
#define STAT_TSFSYNC	0x8004 /* Loss of sync - TSF synchronization */
#define STAT_DEAUTH	0x8100 /* low byte is 802.11 reason code */
#define STAT_DISASSOC	0x8200 /* low byte is 802.11 reason code */
#define STAT_ASSOC_FAIL	0x8400 /* low byte is 802.11 reason code */
#define STAT_AUTH_FAIL	0x0300 /* low byte is 802.11 reason code */
#define STAT_ASSOC	0x0400 /* Associated */
#define STAT_REASSOC    0x0600 /* Reassociated?  Only on firmware >= 5.30.17 */

static void airo_print_status(const char *devname, u16 status)
{
	u8 reason = status & 0xFF;

	switch (status) {
	case STAT_NOBEACON:
		airo_print_dbg(devname, "link lost (missed beacons)");
		break;
	case STAT_MAXRETRIES:
	case STAT_MAXARL:
		airo_print_dbg(devname, "link lost (max retries)");
		break;
	case STAT_FORCELOSS:
		airo_print_dbg(devname, "link lost (local choice)");
		break;
	case STAT_TSFSYNC:
		airo_print_dbg(devname, "link lost (TSF sync lost)");
		break;
	case STAT_DEAUTH:
		airo_print_dbg(devname, "deauthenticated (reason: %d)", reason);
		break;
	case STAT_DISASSOC:
		airo_print_dbg(devname, "disassociated (reason: %d)", reason);
		break;
	case STAT_ASSOC_FAIL:
		airo_print_dbg(devname, "association failed (reason: %d)",
			       reason);
		break;
	case STAT_AUTH_FAIL:
		airo_print_dbg(devname, "authentication failed (reason: %d)",
			       reason);
		break;
	default:
		break;
	}
}

static void airo_handle_link(struct airo_info *ai)
{
	union iwreq_data wrqu;
	int scan_forceloss = 0;
	u16 status;

	/* Get new status and acknowledge the link change */
	status = le16_to_cpu(IN4500(ai, LINKSTAT));
	OUT4500(ai, EVACK, EV_LINK);

	if ((status == STAT_FORCELOSS) && (ai->scan_timeout > 0))
		scan_forceloss = 1;

	airo_print_status(ai->dev->name, status);

	if ((status == STAT_ASSOC) || (status == STAT_REASSOC)) {
		if (auto_wep)
			ai->expires = 0;
		if (ai->list_bss_task)
			wake_up_process(ai->list_bss_task);
		set_bit(FLAG_UPDATE_UNI, &ai->flags);
		set_bit(FLAG_UPDATE_MULTI, &ai->flags);

		if (down_trylock(&ai->sem) != 0) {
			set_bit(JOB_EVENT, &ai->jobs);
			wake_up_interruptible(&ai->thr_wait);
		} else
			airo_send_event(ai->dev);
	} else if (!scan_forceloss) {
		if (auto_wep && !ai->expires) {
			ai->expires = RUN_AT(3*HZ);
			wake_up_interruptible(&ai->thr_wait);
		}

		/* Send event to user space */
		memset(wrqu.ap_addr.sa_data, '\0', ETH_ALEN);
		wrqu.ap_addr.sa_family = ARPHRD_ETHER;
		wireless_send_event(ai->dev, SIOCGIWAP, &wrqu, NULL);
	}
}

static void airo_handle_rx(struct airo_info *ai)
{
	struct sk_buff *skb = NULL;
	__le16 fc, v, *buffer, tmpbuf[4];
	u16 len, hdrlen = 0, gap, fid;
	struct rx_hdr hdr;
	int success = 0;

	if (test_bit(FLAG_MPI, &ai->flags)) {
		if (test_bit(FLAG_802_11, &ai->flags))
			mpi_receive_802_11(ai);
		else
			mpi_receive_802_3(ai);
		OUT4500(ai, EVACK, EV_RX);
		return;
	}

	fid = IN4500(ai, RXFID);

	/* Get the packet length */
	if (test_bit(FLAG_802_11, &ai->flags)) {
		bap_setup (ai, fid, 4, BAP0);
		bap_read (ai, (__le16*)&hdr, sizeof(hdr), BAP0);
		/* Bad CRC. Ignore packet */
		if (le16_to_cpu(hdr.status) & 2)
			hdr.len = 0;
		if (ai->wifidev == NULL)
			hdr.len = 0;
	} else {
		bap_setup(ai, fid, 0x36, BAP0);
		bap_read(ai, &hdr.len, 2, BAP0);
	}
	len = le16_to_cpu(hdr.len);

	if (len > AIRO_DEF_MTU) {
		airo_print_err(ai->dev->name, "Bad size %d", len);
		goto done;
	}
	if (len == 0)
		goto done;

	if (test_bit(FLAG_802_11, &ai->flags)) {
		bap_read(ai, &fc, sizeof (fc), BAP0);
		hdrlen = header_len(fc);
	} else
		hdrlen = ETH_ALEN * 2;

	skb = dev_alloc_skb(len + hdrlen + 2 + 2);
	if (!skb) {
		ai->dev->stats.rx_dropped++;
		goto done;
	}

	skb_reserve(skb, 2); /* This way the IP header is aligned */
	buffer = (__le16 *) skb_put(skb, len + hdrlen);
	if (test_bit(FLAG_802_11, &ai->flags)) {
		buffer[0] = fc;
		bap_read(ai, buffer + 1, hdrlen - 2, BAP0);
		if (hdrlen == 24)
			bap_read(ai, tmpbuf, 6, BAP0);

		bap_read(ai, &v, sizeof(v), BAP0);
		gap = le16_to_cpu(v);
		if (gap) {
			if (gap <= 8) {
				bap_read(ai, tmpbuf, gap, BAP0);
			} else {
				airo_print_err(ai->dev->name, "gaplen too "
					"big. Problems will follow...");
			}
		}
		bap_read(ai, buffer + hdrlen/2, len, BAP0);
	} else {
		MICBuffer micbuf;

		bap_read(ai, buffer, ETH_ALEN * 2, BAP0);
		if (ai->micstats.enabled) {
			bap_read(ai, (__le16 *) &micbuf, sizeof (micbuf), BAP0);
			if (ntohs(micbuf.typelen) > 0x05DC)
				bap_setup(ai, fid, 0x44, BAP0);
			else {
				if (len <= sizeof (micbuf)) {
					dev_kfree_skb_irq(skb);
					goto done;
				}

				len -= sizeof(micbuf);
				skb_trim(skb, len + hdrlen);
			}
		}

		bap_read(ai, buffer + ETH_ALEN, len, BAP0);
		if (decapsulate(ai, &micbuf, (etherHead*) buffer, len))
			dev_kfree_skb_irq (skb);
		else
			success = 1;
	}

#ifdef WIRELESS_SPY
	if (success && (ai->spy_data.spy_number > 0)) {
		char *sa;
		struct iw_quality wstats;

		/* Prepare spy data : addr + qual */
		if (!test_bit(FLAG_802_11, &ai->flags)) {
			sa = (char *) buffer + 6;
			bap_setup(ai, fid, 8, BAP0);
			bap_read(ai, (__le16 *) hdr.rssi, 2, BAP0);
		} else
			sa = (char *) buffer + 10;
		wstats.qual = hdr.rssi[0];
		if (ai->rssi)
			wstats.level = 0x100 - ai->rssi[hdr.rssi[1]].rssidBm;
		else
			wstats.level = (hdr.rssi[1] + 321) / 2;
		wstats.noise = ai->wstats.qual.noise;
		wstats.updated =  IW_QUAL_LEVEL_UPDATED
				| IW_QUAL_QUAL_UPDATED
				| IW_QUAL_DBM;
		/* Update spy records */
		wireless_spy_update(ai->dev, sa, &wstats);
	}
#endif /* WIRELESS_SPY */

done:
	OUT4500(ai, EVACK, EV_RX);

	if (success) {
		if (test_bit(FLAG_802_11, &ai->flags)) {
			skb_reset_mac_header(skb);
			skb->pkt_type = PACKET_OTHERHOST;
			skb->dev = ai->wifidev;
			skb->protocol = htons(ETH_P_802_2);
		} else
			skb->protocol = eth_type_trans(skb, ai->dev);
		skb->ip_summed = CHECKSUM_NONE;

		netif_rx(skb);
	}
}

static void airo_handle_tx(struct airo_info *ai, u16 status)
{
	int i, len = 0, index = -1;
	u16 fid;

	if (test_bit(FLAG_MPI, &ai->flags)) {
		unsigned long flags;

		if (status & EV_TXEXC)
			get_tx_error(ai, -1);

		spin_lock_irqsave(&ai->aux_lock, flags);
		if (!skb_queue_empty(&ai->txq)) {
			spin_unlock_irqrestore(&ai->aux_lock,flags);
			mpi_send_packet(ai->dev);
		} else {
			clear_bit(FLAG_PENDING_XMIT, &ai->flags);
			spin_unlock_irqrestore(&ai->aux_lock,flags);
			netif_wake_queue(ai->dev);
		}
		OUT4500(ai, EVACK, status & (EV_TX | EV_TXCPY | EV_TXEXC));
		return;
	}

	fid = IN4500(ai, TXCOMPLFID);

	for(i = 0; i < MAX_FIDS; i++) {
		if ((ai->fids[i] & 0xffff) == fid) {
			len = ai->fids[i] >> 16;
			index = i;
		}
	}

	if (index != -1) {
		if (status & EV_TXEXC)
			get_tx_error(ai, index);

		OUT4500(ai, EVACK, status & (EV_TX | EV_TXEXC));

		/* Set up to be used again */
		ai->fids[index] &= 0xffff;
		if (index < MAX_FIDS / 2) {
			if (!test_bit(FLAG_PENDING_XMIT, &ai->flags))
				netif_wake_queue(ai->dev);
		} else {
			if (!test_bit(FLAG_PENDING_XMIT11, &ai->flags))
				netif_wake_queue(ai->wifidev);
		}
	} else {
		OUT4500(ai, EVACK, status & (EV_TX | EV_TXCPY | EV_TXEXC));
		airo_print_err(ai->dev->name, "Unallocated FID was used to xmit");
	}
}

static irqreturn_t airo_interrupt(int irq, void *dev_id)
{
	struct net_device *dev = dev_id;
	u16 status, savedInterrupts = 0;
	struct airo_info *ai = dev->ml_priv;
	int handled = 0;

	if (!netif_device_present(dev))
		return IRQ_NONE;

	for (;;) {
		status = IN4500(ai, EVSTAT);
		if (!(status & STATUS_INTS) || (status == 0xffff))
			break;

		handled = 1;

		if (status & EV_AWAKE) {
			OUT4500(ai, EVACK, EV_AWAKE);
			OUT4500(ai, EVACK, EV_AWAKE);
		}

		if (!savedInterrupts) {
			savedInterrupts = IN4500(ai, EVINTEN);
			OUT4500(ai, EVINTEN, 0);
		}

		if (status & EV_MIC) {
			OUT4500(ai, EVACK, EV_MIC);
			airo_handle_cisco_mic(ai);
		}

		if (status & EV_LINK) {
			/* Link status changed */
			airo_handle_link(ai);
		}

		/* Check to see if there is something to receive */
		if (status & EV_RX)
			airo_handle_rx(ai);

		/* Check to see if a packet has been transmitted */
		if (status & (EV_TX | EV_TXCPY | EV_TXEXC))
			airo_handle_tx(ai, status);

		if ( status & ~STATUS_INTS & ~IGNORE_INTS ) {
			airo_print_warn(ai->dev->name, "Got weird status %x",
				status & ~STATUS_INTS & ~IGNORE_INTS );
		}
	}

	if (savedInterrupts)
		OUT4500(ai, EVINTEN, savedInterrupts);

	return IRQ_RETVAL(handled);
}

/*
 *  Routines to talk to the card
 */

/*
 *  This was originally written for the 4500, hence the name
 *  NOTE:  If use with 8bit mode and SMP bad things will happen!
 *         Why would some one do 8 bit IO in an SMP machine?!?
 */
static void OUT4500( struct airo_info *ai, u16 reg, u16 val ) {
	if (test_bit(FLAG_MPI,&ai->flags))
		reg <<= 1;
	if ( !do8bitIO )
		outw( val, ai->dev->base_addr + reg );
	else {
		outb( val & 0xff, ai->dev->base_addr + reg );
		outb( val >> 8, ai->dev->base_addr + reg + 1 );
	}
}

static u16 IN4500( struct airo_info *ai, u16 reg ) {
	unsigned short rc;

	if (test_bit(FLAG_MPI,&ai->flags))
		reg <<= 1;
	if ( !do8bitIO )
		rc = inw( ai->dev->base_addr + reg );
	else {
		rc = inb( ai->dev->base_addr + reg );
		rc += ((int)inb( ai->dev->base_addr + reg + 1 )) << 8;
	}
	return rc;
}

static int enable_MAC(struct airo_info *ai, int lock)
{
	int rc;
	Cmd cmd;
	Resp rsp;

	/* FLAG_RADIO_OFF : Radio disabled via /proc or Wireless Extensions
	 * FLAG_RADIO_DOWN : Radio disabled via "ifconfig ethX down"
	 * Note : we could try to use !netif_running(dev) in enable_MAC()
	 * instead of this flag, but I don't trust it *within* the
	 * open/close functions, and testing both flags together is
	 * "cheaper" - Jean II */
	if (ai->flags & FLAG_RADIO_MASK) return SUCCESS;

	if (lock && down_interruptible(&ai->sem))
		return -ERESTARTSYS;

	if (!test_bit(FLAG_ENABLED, &ai->flags)) {
		memset(&cmd, 0, sizeof(cmd));
		cmd.cmd = MAC_ENABLE;
		rc = issuecommand(ai, &cmd, &rsp);
		if (rc == SUCCESS)
			set_bit(FLAG_ENABLED, &ai->flags);
	} else
		rc = SUCCESS;

	if (lock)
	    up(&ai->sem);

	if (rc)
		airo_print_err(ai->dev->name, "Cannot enable MAC");
	else if ((rsp.status & 0xFF00) != 0) {
		airo_print_err(ai->dev->name, "Bad MAC enable reason=%x, "
			"rid=%x, offset=%d", rsp.rsp0, rsp.rsp1, rsp.rsp2);
		rc = ERROR;
	}
	return rc;
}

static void disable_MAC( struct airo_info *ai, int lock ) {
        Cmd cmd;
	Resp rsp;

	if (lock && down_interruptible(&ai->sem))
		return;

	if (test_bit(FLAG_ENABLED, &ai->flags)) {
		memset(&cmd, 0, sizeof(cmd));
		cmd.cmd = MAC_DISABLE; // disable in case already enabled
		issuecommand(ai, &cmd, &rsp);
		clear_bit(FLAG_ENABLED, &ai->flags);
	}
	if (lock)
		up(&ai->sem);
}

static void enable_interrupts( struct airo_info *ai ) {
	/* Enable the interrupts */
	OUT4500( ai, EVINTEN, STATUS_INTS );
}

static void disable_interrupts( struct airo_info *ai ) {
	OUT4500( ai, EVINTEN, 0 );
}

static void mpi_receive_802_3(struct airo_info *ai)
{
	RxFid rxd;
	int len = 0;
	struct sk_buff *skb;
	char *buffer;
	int off = 0;
	MICBuffer micbuf;

	memcpy_fromio(&rxd, ai->rxfids[0].card_ram_off, sizeof(rxd));
	/* Make sure we got something */
	if (rxd.rdy && rxd.valid == 0) {
		len = rxd.len + 12;
		if (len < 12 || len > 2048)
			goto badrx;

		skb = dev_alloc_skb(len);
		if (!skb) {
			ai->dev->stats.rx_dropped++;
			goto badrx;
		}
		buffer = skb_put(skb,len);
		memcpy(buffer, ai->rxfids[0].virtual_host_addr, ETH_ALEN * 2);
		if (ai->micstats.enabled) {
			memcpy(&micbuf,
				ai->rxfids[0].virtual_host_addr + ETH_ALEN * 2,
				sizeof(micbuf));
			if (ntohs(micbuf.typelen) <= 0x05DC) {
				if (len <= sizeof(micbuf) + ETH_ALEN * 2)
					goto badmic;

				off = sizeof(micbuf);
				skb_trim (skb, len - off);
			}
		}
		memcpy(buffer + ETH_ALEN * 2,
			ai->rxfids[0].virtual_host_addr + ETH_ALEN * 2 + off,
			len - ETH_ALEN * 2 - off);
		if (decapsulate (ai, &micbuf, (etherHead*)buffer, len - off - ETH_ALEN * 2)) {
badmic:
			dev_kfree_skb_irq (skb);
			goto badrx;
		}
#ifdef WIRELESS_SPY
		if (ai->spy_data.spy_number > 0) {
			char *sa;
			struct iw_quality wstats;
			/* Prepare spy data : addr + qual */
			sa = buffer + ETH_ALEN;
			wstats.qual = 0; /* XXX Where do I get that info from ??? */
			wstats.level = 0;
			wstats.updated = 0;
			/* Update spy records */
			wireless_spy_update(ai->dev, sa, &wstats);
		}
#endif /* WIRELESS_SPY */

		skb->ip_summed = CHECKSUM_NONE;
		skb->protocol = eth_type_trans(skb, ai->dev);
		netif_rx(skb);
	}
badrx:
	if (rxd.valid == 0) {
		rxd.valid = 1;
		rxd.rdy = 0;
		rxd.len = PKTSIZE;
		memcpy_toio(ai->rxfids[0].card_ram_off, &rxd, sizeof(rxd));
	}
}

static void mpi_receive_802_11(struct airo_info *ai)
{
	RxFid rxd;
	struct sk_buff *skb = NULL;
	u16 len, hdrlen = 0;
	__le16 fc;
	struct rx_hdr hdr;
	u16 gap;
	u16 *buffer;
	char *ptr = ai->rxfids[0].virtual_host_addr + 4;

	memcpy_fromio(&rxd, ai->rxfids[0].card_ram_off, sizeof(rxd));
	memcpy ((char *)&hdr, ptr, sizeof(hdr));
	ptr += sizeof(hdr);
	/* Bad CRC. Ignore packet */
	if (le16_to_cpu(hdr.status) & 2)
		hdr.len = 0;
	if (ai->wifidev == NULL)
		hdr.len = 0;
	len = le16_to_cpu(hdr.len);
	if (len > AIRO_DEF_MTU) {
		airo_print_err(ai->dev->name, "Bad size %d", len);
		goto badrx;
	}
	if (len == 0)
		goto badrx;

	fc = get_unaligned((__le16 *)ptr);
	hdrlen = header_len(fc);

	skb = dev_alloc_skb( len + hdrlen + 2 );
	if ( !skb ) {
		ai->dev->stats.rx_dropped++;
		goto badrx;
	}
	buffer = (u16*)skb_put (skb, len + hdrlen);
	memcpy ((char *)buffer, ptr, hdrlen);
	ptr += hdrlen;
	if (hdrlen == 24)
		ptr += 6;
	gap = get_unaligned_le16(ptr);
	ptr += sizeof(__le16);
	if (gap) {
		if (gap <= 8)
			ptr += gap;
		else
			airo_print_err(ai->dev->name,
			    "gaplen too big. Problems will follow...");
	}
	memcpy ((char *)buffer + hdrlen, ptr, len);
	ptr += len;
#ifdef IW_WIRELESS_SPY	  /* defined in iw_handler.h */
	if (ai->spy_data.spy_number > 0) {
		char *sa;
		struct iw_quality wstats;
		/* Prepare spy data : addr + qual */
		sa = (char*)buffer + 10;
		wstats.qual = hdr.rssi[0];
		if (ai->rssi)
			wstats.level = 0x100 - ai->rssi[hdr.rssi[1]].rssidBm;
		else
			wstats.level = (hdr.rssi[1] + 321) / 2;
		wstats.noise = ai->wstats.qual.noise;
		wstats.updated = IW_QUAL_QUAL_UPDATED
			| IW_QUAL_LEVEL_UPDATED
			| IW_QUAL_DBM;
		/* Update spy records */
		wireless_spy_update(ai->dev, sa, &wstats);
	}
#endif /* IW_WIRELESS_SPY */
	skb_reset_mac_header(skb);
	skb->pkt_type = PACKET_OTHERHOST;
	skb->dev = ai->wifidev;
	skb->protocol = htons(ETH_P_802_2);
	skb->ip_summed = CHECKSUM_NONE;
	netif_rx( skb );

badrx:
	if (rxd.valid == 0) {
		rxd.valid = 1;
		rxd.rdy = 0;
		rxd.len = PKTSIZE;
		memcpy_toio(ai->rxfids[0].card_ram_off, &rxd, sizeof(rxd));
	}
}

static u16 setup_card(struct airo_info *ai, u8 *mac, int lock)
{
	Cmd cmd;
	Resp rsp;
	int status;
	SsidRid my====;
	__le16 lastindex;
	WepKey====wkr/*=====rc;

	memset( &======, 0, sizeof(======= ) );
	kfree (ai->flashes c   This c = NULL    /* The NOP is the firs====ep in gettingrsioncard gos.
 */
	cmd.cmded uOP; may bparm0 =  respect1ve licenses2 = 0/*==f (lock && down_interruptible(&   Tsem))
		return ERROR the en issuecommand( ai, &cmd, &rsp ) != SUCCESS ) {
	he end ofevel	upcode was d;veloped by Benjami}
	disable_MACrs.sou0)r bot/ Let's figure out if we need to usersionAUX portthe en!test_bit(FLAG_MPI,ode wflags)ing poay be usedCMD_ENABLEAUX thee end <breed@user.sourceforge.nt>
    incluing poortions of whiich come from the	airo_print_err    Tdev->name, "Error checks.
 forght
    ("Major oped by Benjami0
  ) 1999aux_bap || rsp.====== & 0xff00or this   Tbap_reausedfaenjaCode waajor code contridbgions were receiveDlicenlso  ntegrates<achir} else.com>.
    Code was a and Jegrated from the Cisco Aironet driver for Linux.ht
 Support for MPI300
  rtions of whch come from the en   Tconfig.len == hp.com>====irceftdsRssi====rssi_r=====	Capabilit=====celle====
	cards
*/

#APListm thenux/moduleed under lude <linux/SSID.h>
#inclludeinux/proc_f// general includuration (e wa/modify/writeevelilhes <= e waCncludRiloper'ns of   code =======>
    includinoped by Benjaminclude <linux/ste <linux/keroper's l.h>
#clude <linux/timer.h>
#include <linux/interrupt.h>
#include <lPC4500de warlude <RID_RSSI,&t.h>

#i,500 andt.h>

#i),ude <linux/timer.h>
#=    including po====*/

#t.h>an Tice.h>
#in= kmalloc(512, GFP_KERNEL)t>
  undeby
   memcpyice.h>
#i, (u8*)ude <asm/ + 2, 512); /* Skip RID length membernse mforge0 cards wade <linux/t.h>Major co<linux/etx/proc_f====*nux/bit.softCap & cpu_to=====(8devel
#inclincludermode |= RXMODE_NORMALIZE>
#inc.h>
#0 calinux/ide contriwarnions were receiveunknown received signal "linux		"level scale=========x/ieee80211.op>

#i= adhoc ? e "aiSTA_IBSS :9, 0x4500,ESSh>
#inclincludeauthTyp},
	AUTH_OPENY_ID },
	{ 0x14modul#inclu=9, 0_CCKh>
#ie ende16

#icpulude <linlen) >=4500 andnux/bit) &&
		   clude <linextSx/freezer.h>

#includ1)I_ANY_ID, Pmicsetup(ai)#include <lp.com>.
   PCI_ANY_ID, },|b9, 0E_MIC.h>
#senjamin Reed.IC_CAPo us, ll Rights Rrceforlude* Save offrsionMACpci.h>for( x/et0; i < ETH_ALEN; i++inux/netmac[i],
	{CI_ANY_ID, acAddr[i]ds);

staticCJavion. seeth pthere aic iny insmodtrace.h>
edY_ID, rateson. addpci.h>x/tim_dev [0]pci_devicironet/ieee80211.hdev ,0system.hro_pci_resume(st)Major ruct pci_dev *, 8 thit stati]ruct pci_devix/ieee80211.hver = {linuver = {
linuANY_orge.net basic_ car > 0inux/netct pci_driver air
	.name     =  = aiDRV_NAME,
	.id_table =iro_pci_prob||linuxD, P !DRV_NAME,
	.id_tme     =  DRV_NAME,
	.id_table =spend,
	.res 0x8  thion breaknsion ANY_.probe    }
};
M n ReedCOMMITE(pci, card_ids)

sttic Y_IDrsionludesth ppresentnse mx/timesidate);
stati
#includeruct pci_dev *, 3 thispy su {
	.name     =500 _tnclu = strlen( New driMajor I_ANY_n > 32<linuxCISCO_32nsion======. New drie <linr.h>

#includ
	{ _EXT
#includ#include <linux/spy ,* New dri,nclu PCI_ANY_#includdelay.h>
#endif

/500 and======ic st

stlude <linb.h>
tring.h>
#include <linx/timer.h>
#include <linux/interrupt.h>
#s.h>
# define WIRE l <lienable iwspy support */
on't know what=======oper'sriver foude <linux/timer.h>
#include <linux/interrupt.h>lly don't knoweneveloper'these counts areer.h>
#include <leveloped by Benjamr comGrabrsioninitial wep key,permgotta sint itAchirauto_RxOvse mrclinux/s=========oper'swkr, 1ese counts areLABE{ 0x14b9, 0dong po=========now kr.k========s arecCrcOk",
	gthE.h>

#includ@hplffReservee <lidef,
	"RxMacCrcce_i0i_remov	NLABEL("RxPlcpFormatErr"),
	IG0istic will} while(Err",
	"Rx!MacCrcOk",
	rencetry_rcErr"),, },ar *oped by  inclu;
}

====ic u16ed <breed@userstruct "

#iinfo *.souCmd *pCeforResp *pR was{
= air   // Imnux/lly paranoid ab witlnses.
 it runAchiever!*=====max_trie<lin60xRetar *e enIN#incystemEVSTAT) & EV_CMDevelOUTSync-MissedACKssed	"Losar *nc-ArlExceedPARAM0, sNo"->pectiack auth",
	"LostSync1Disassoced",1	"LostSync-TsfTiming"2Disassoced",2	"LostSync-TsfTi*/
#ANDDisassoccmdnc-Dets",
 (	"LostSyn-- thiostSync-MissedBeacons",
	"Losinux/err.h>
f (ostSync-Missc",
	"H)GNLABtRxBc",
	"ySho//h>
#inc didn't notice eed@use, try againySho",
	"HostRxMc",
	"HostRxBc",
	"Hoge.net>in_atomic(I_AN,
	"HostRxD & 255GNLABEL/

#dchedule(really d = ai	"LostSync-= -1ding pocode contributions were receiySho"Max stSyncexceeded whened <brs.
 eed@use<achir	"LostSync-MissEL("Hmac &easonOut_BUSYmacRxnc-ArlExceeded",
	"LostLEARc",
	"H
	IGN the Aironet PC4500
 ce Maeed@usenStapletructrsPr->lude <linstSync-MissBeacUStatuL("Rearspive stSync-MissRESP
	"LonStatus5" areIGNLABEL("Reaso,
	"HnStatus5"t
  IGNLABEL("ReasoxFailcTxBcL("ReasonStatjt@hpl.hp!=0cRxDtRxBc",
		"TxsionSOFTRESETmacR	"RatesMismatch",
	"AuthReject",
	cmd:%x=======tatus5")
	IGNLA1
	IGNLA2:%x"ct",
tRxBc",
	, L("ReasonStaeasonStas5")),
	IGNLABE1,
	IGN
	IGNLABErence Maclear stucknStatus3"busyth pnecessary,
	"LostSync-MissasonOutsideTable"),
	IGNng poL("ReasonStatus1"),
	IGNLABEL("ReasonStatu}ce Maac pciledge pronStas.
    E======/responE "anc-ArlExceeded",
	"LostSync-Deackets",
	"TxBeacons.h>
#steness.  Jea_devtartout"hange data. Rts"ch Jeashould
 * be ont airsionBAP0 oryMis1 
	"Maes.  LockstSyncBerAP"heWeppe",
	ttercalling!
	IG",
	"Rx====BellNY_ID,oll",
	"TxMulColl",
	"BeacbitopBeacoffset,};
#i",
	"Lost)
{*=====time wit= 5  the",
	"LostSync-M3c-Deauth",
	"LosSELECT0+",
	"Los,fies	"LostSync-TsfTiOFFSEly be set in #endiftatuRxUc",
1rt */
#incsonStatus4"),
	IGNLAshould be a commxUc"),
	Iilhes <jtBAP	IGNLABEL
	"Defertic char *ssids[th this is
	IG,
	"Dupare for i, bwithts Ok",aySho   closepci.h>GNLABe for i--e     = continues,
	.probe50 carx/timer.h>
#c int ERRinux/net/* invalinitidatch#endifncrypt code contributions were receiveBAP e from%x %d),
	IG   infr, These varichirica@users.sourcefo card can ncrypt at.
		DONE) {rsEnsucnStahirica@user,
	"TxBeaobe    = ai!
	"HostRxDis)inux/net	"RatesMismatch",
	"AuthReject",
	ep /*NY_ID* = 0 *too mairorestSyn\n<achirica@users.sourceforge// --acTxFailmissed itMc"),
	IGNLABEe rates
   can only be set in setup_ccard.  Rates should be a comma separated
	e for insmod, s}s",
	"HxNotWeponlyrAP"atched byrice Bellet
 . 

stataux funcincluus3"theate;follows.
  CopconceptsGNLA document0 */nrsiond 0x1opers guide.  Iate;got1 wim from a patch givenElapmyRIPTAiron cards",
	"RxBeac andf RUN_AT
#define RUN_AT(x) (jiffpagect",tic cx))
#endif

Beac*ON_DableBeacnextc-Deauth",
	"LosAUXPAGE,ICE("	"LostSync-TsfTiAUXOFFeferen	;
moL("ReasonStatuAUXDATA(irqsco t, NULL, 0);
module_pa&@hpl	IGNLAB old ca!= 4) );
module_param_arraseparated
oped by;
modu",
	"HrequiresLE_DEElapfndef RUN_)n 2 andchar*)-1 };
#iice Bellet
 _AT
#define RUN_AT(x) (=======*pu16Dstct",
;
#ifytelen


/* These va340/350")len;/350")CE("ons unt#endifons unt;
moduping word====
#includuncarded loux.
hts ar *spin_d of_irqBEL(code w andd of, the ws7"),ag},
	4"),
	IGNLABWSstatic int rat old cae key at index 02tatic int rat, int, ual BSD/GP.souCE("A
#endif

&ON_DOWNue of = (through+1)>>1ar *chir(i=dev <ue of rt */
#inccoun The	er fowilllen>>1) < (ue of-i) ?ds with so:e older bulinux/tim!do8bitIO macRxinsw
	.suswere base_addr+le_py be set intic
int   ll keep+i,der cang itAME "airinsbis needed.");
module_param(maxencrypt, int, 0);
MODULE_Pter fo << 1ESC(mai +=ter for olode ieems tetryShoULE_PARM_DESC(aux_bap;
mo, 4ro, the dr=======eys tuno check.restor value of 2 will try usingackets",
	"TxBeaconNULL, 0);
module_param(auto_wep, int, 0);
MODULE_PARMlso integrateep, "If non-zero, the driver will keep loop};
#ifhrough \
the authenticati, the dwill switch + 1side(~1x/io/ roundteneso e witvaluesidMismks that the swtch is needed.");
module_param(maxencryp);
MODUL,o, the d>>512kbsAME "ai, "The maximum speed that the card can d);
module_param(pESC(m",
	"HostTxBytes",
	"H, 0);
module_param(auto_wep, int, 0);
MODULE_PARMBellb.h>
_AT
#define RUN_AT(x) (constdriver will kSrcAironet 4zero, the driver won't start the card.");

module_param(proc_uid, int, 0);
MODULE_PARM_DESC(proc_uid, "The uouth is needed.");
module_param(maxencrypt, tic chaaironet._param(proc_gid, int, 0his MODULE_PARM_DESC(proc_gid, "The gid that t OUT4500 andes will belong to.");
mor*)-1 };
#i>
#inclaecks <asmAT
#define RUN_AT(x) (jiffies+(x))
acNLABE{
	Defecmd/iopochird <b,
	"AssocTis
	IGN	"Defrsp/iopooll",
	"PCMCIAe NO_PACKET -Beac========  Aironetrceforr 4500 and,
	"ng iay be used SUCCThe respective 
#incllude <linin the Developer's manual wa#define IGNLABEL(0nux/inter========= = ai(ourrilhes <jt@h7F.hp.SET	0xg pooped by( SUCC in 8) +EP	0x0s5"),jt@hFF	"RxRefoped by0rp, NULL Note, thatpermint u	"RxP",
	"",
	" verals.  CoCRIPTtransm/;

sotterpermmustcens astic .0);
MODULE_PARM>
#include <asmAT
#define RUN_AT(x) (jiffies+(v,
	"*pBuf


/* ugh \
thens of 0/350")========tic char====

     incluy",
	"Lons ofng portios file.

    This code was devele Aironet PC4500
  /* = Benjamin Reed.  All Rights Reserve
#define
	-2

/* Com
staBLE	0x0001
#define MAC_DISABLE	002a
#defrsp#define MAC_l wa.h>
#inclinclud_desc.ridSPNODEo limi= 1
#define CMD_PSPNODES	0x0030
ard.")RIDSIZE
#define CMD_PSPNODES	0x0030
t.      thefine CMD_PSPNODES	0x0030
hostodule*);
staridbAC_ENAed.  PermissionA aux port_LOSE_SYNC	0x0003
#deficpy_toiodev *pdev);SPNODEithe_ram_offpt, iode w CMD_PSPNODES	0x0030 4500 andR I rea	"NoCts"re what this does... */
#definault) m	0x0005
#define l.hpX	0x0ABEL(EMASK	0Uc"),
	IGrcfine  do somKAROUNfine CMD_ENABLEAvirtual_efine MACR_ON_DOWN
goto donan a350 cards wcTxBc/crypto.h>
#incl8bitIO /* .soubitop_LISTBSS	0))!=0x14b9, 0xa5tic char *ssids[ERROR=========tic char *ssids[NVFID 0x03
#tic charorge.netfndef RUN_GE 0x05
#0,x0009as granted for thisERROR Benjamiefine ERROR_NORD 0x0B
#define ERROR_NOWRid,  wassiont.  clude <fieluct e_param(p_bap,AROUN2IDTX 0xclude <ine ERROor remains.
 pdUseof
#deCrcErd.")mins wi, (int)Y_ID, PCI_AN*(======*)ine )) - 201

/* Co CISC<= 2    the bap, needed on some older cards and====%x has	0x0lude <of %Assoefine Cc insh   <rds anefinebitopefine E
   do_TESTACT 0x0E
#define ERROR_TAGNFND 0x12
#dtic int ODE 0ROR_MOdeor o0x20
#de	"NoCts"NAV 0x21
#def( 0x82
#define +NLAB1
#d_BADLEN }
 0x0:=============================oped by

  G	0x0008
#define CMD_SETMODE	0x0009
#define CMD_ALLOCATETX	0x000a
#define Cmake susedtstatic tLE_DESCRsocReaX	0x000a
 verhappeODE 0define CMD_DEALLOCATEb.h>
x000c
#define NOP		0x0010
#define tic
int roc/[MD_WORKAROUND	0x0011
#define CMD_ALLOCATEAUX 0=====

CMD_ACCESS	0x0R 0x82
#defineay.h>
#endif

/(u16R_AU201

021
#define CMD_PCIBAP	0x0022
#define CMD_PCIAUX	0x0023
#define CMD_ALLOCBUF	0x0028
#define CMD_GETTLV	0x0029
#define CMD_PUTTLV	0x0LLOCBUF	0x0028
#de to usDE(pci, card_icRxDi.h>
WEP_TEMPMAGIsetufine bap, needed on some older cards and%s:i_proxNotWepped  Develd (rid=%04x)CAN 0x8__iron__nt airo_pe02a
#define CMD_DELTLV	0x002b
#define CMD_FINDNEXTTLV	0x002c
##define CMD_PSPNODES	0x0030
#define CMD_SETCW	0x0031    
#define CMD_SET*MPLFI define 032    
#define CMD_SETPHYREG	0x003e
#1
#define CMD_LISWRITERIDx0103
#define CMD_SAVECFG	0x0108
#define CMD_ENABLEAUX	0x0111
#define CMD_WRITERID	0x0121
#define CMD_USEPSPNODESextensio< 4an Tnsions2047   the bap, needed on some older car ne FIlen=/* I criptors *ON_DOWN
TESTAC-CMD_S350 cards wa do som(char *)1
#define ERROR_ILLFMT 0x02
#definsion ine ER 0x24
#dES	0x0130
#define MAC_ENABLERX	0x0201pt /* =Lengjt@hpl.hp.AGIC_PKT	0 BAP_ERR 0x4000
#define BAP_DONE 0x20W.h>

#defd from/* If setescriptors */c EV_TXBAP_ERR 0x4000
#define BAP_DONE 0x20CmmemoryEV_TXCPY 0x400
#defay be u EV_TX

staXEXC 0xmand errors */
#def<linuxERROR_QUALIF 0x00====50 cards wnt pr-n 2 and8bitIO sofine CMD_canw what0x20
#defpsedmessage_VRID 0x04
#define ERROR_LARGE 0x05
#define ERROREV_ALLOC 0#define ERROR_ALLOCBSY 0x07
#define ERROR_NORD 0x0B
#define ERROR_NOWRXC|EV_Tnowifdef CHECK_UNKNOWN_INTS
#C
#define ERROR_INVFIDTX 0x0D
#define ERROR#define ERROR_ALLOCBST 0x0E
#define ERROR_TAGNFND 0x12
#define ERROR__perm, "Th#define ER_APLIST 0x8D
/* The IDs eed@i\
forK_UNKNOWN_INID 0x#define ERROR_LARGE 0x05
#0x100|define ERRO8D
#define ERROR_AUTOWAKE 0x8E
#defineERROR_ALLRROR_LEAP 0x8F

Adevidev *a FIDparamTMODedne ER	0x000a
es.
 packet",
	WP",
ly#defTHER,
	"e ERnow
#define CMDTXQ 	0x000a
_rdevia"The permission bits of /D	0x001PayloadG 0xFFrawCMD_ALis numbe#definop thaetry"29
#define MD_PUTTLV	#defitxF============txControlsed define CMD_LISTLLOCATET     respective 20 /*readoHOSTSLEs file.

    This code was developed by Benjamin Rein the Developer's manual was granted for thi22
#d  0xFF02
#dINVFID 0x03
#dOSTSLEEP	0x0005
#defineFCMD_MAGIC_PKT	0 0xFF53
#define RID_UNKNOWN54  0/* waRxPlcpCfinexFF18
#dODULEt/indic#incl
	 * I,
	"kes me;

stOR_Dnervouersia\
foioduln jRANSsit atic ind eys ,ATS16 irq[n pracABEL(itne RIDNFIGs likON 0urre fos
#def
   (no Bc"),
	IGNLABedBeacons",
	 RID_GNLABEL(&& --NFIGfine CMD!6A
#dF55
#define RID_UNKNOWN56  0xFF56
#dce MaMIT	        0xFFd f
	"Rndsh",
	"TxRef
#define RstSync-MissTX RID_Fde <linc-ArlExceeded",
	"LosATSDELar *sta the CARD verpretty cool sinfine Rconvertersionetati
MOD0xFF16ATS16ORYCo 802.11.  AD_ALnof CHEe CMD_do int ileaCopyrig  0xISTNEXwe6 cmd;will15
#de"RxPoll",amP",
	"PverID_BS;
	u16IGNLne RID 0xFfine RIDhint _dev #define hestrol ardsstruct {
_SETMno16 cmd; Cmd;
s.
    Efidne RID* Comdefi
#deAPUSERNay.h>
#endif

/TXCTL_TXOK |  up doinEXthat.
 * 802_11finehat.
 * ETHude Tthat.
 * NORELEASEgid, int, 0 the big-endian guys end up doing that.
 * so all rid access3hould use the read/writeXXXRid routines.
 LITIES 0xFF00
#def22
#dT   0008IDTX 0x0D
#define E5
#define RID_UNKNOAME "aiRSSI       0xF& the big- 4500 and the big-)LIST 0x8Ddefine Ech come from te ERROR_L22
#def",
	"HIicennux/pt",
	"is dee RIDsion. 	0x000a
M   0xFF16
#deHow	"Rx,THERays be inissioares 6
#defiecks s.
 RIDtrie    _ALLOCres ar_MIC  at.THERM1 0x04
#def
	u8 sseys d of 0x08,
	"socReefine PLE_DESC
#define CMD_DEAIONS    0ineer_0xFF16efine RID_ACTUALCONFIG 0xFF20 , e EV_CpPxFF16CMD_A=======p*readoLtionsefine RID_UNKNOWN22  ince icROR_HO1
#d0xFF22
#d24
#de;
	MICBuffer pMi
    nsion>= 164
#define3
#defconst st * 2tch",
	"RatesMismONFIG_PCI
static struSOR_St {
	u1ne EVC 0xffff
 Aironet PC4500
  OD_D-FAULT cpu_to_l4
#definBUF	0x0028
#defEVICE_TABLE(pci, card_iR 0x
staI_ANtats.proc fdR 0xdefine(ntohs( 0x8ber wi)
typedef[6]#definx888EReservecTxBencaps
	{    0x(ef stH was opmode; ,&le16,
	{ 0EL(comment) NUica@users.sourcefationRid 500 andle16	"RxRefrese MOD_Me fro====#incl[6]#defurce16(3)ulation[len-12]ce Mafdef CHECKulationERROR_ORD_BSdst/src/ulationriver */
typedef struct WepKeyR36IDTX 0x0D
#define ERoped by Benjamith the hardwint addressTEMPre
	IGer foct Ss0x80
#defff)
#define#defiTS16w1<<8dian-neubt2
#dE_AIR12o, thsD_MIC     payloads airo RIDulationRiday.h>
#endif

/* H +lationRne R__le16 kindex;
ulationRid 4500 andulationRid),_BADLEN RSSI       0xF 0x82
#def0)
#defu_to_le1 cpu_to_lklen;
	u8 #defi) /* en;
	__le16 kindex; 0x82
#deine MOlationRLIST 0x8D
C cpu_to_le16(1<<12) /*( enable +antenna alignment )#define RID_CON//130
#dE_AIRe STATUS eed@use  Aironet d01
#define MAC_Inteeries cd.  PermissionTRANSMITASSWORD 0xFF24
22
#defiine RID_BUSY_HST   0xFF52
#define RID_RETRIESoped by Benjamin Ree
#define RID_UNKNOWN55  0xFFoped by Benjamiackets",
	"TxBeacon",
	"RxRid {
	__le16 len11
	Ssid ssids[3];
} __attribute__ ((packed));

typedef struct ModfcMODE_CFG_Rid ModulationRid;
struct ModulhdT		/03 /* NRID_8 tail[(30-10SETW2 + 6le ={[de */le =6}to_le1padds.
 of heaERROto fulSLISze + =====gap(1<<(6SETWto_le16onet e00
#defi  __le16 len;
ulation;
#defifD 0xR 0x82
#deftypede;
	3) /*  =)
#defi_		/*f)
#dfine MOD_DE(3) /* e16(0)
#define MOD_CCK cpu_to_le16(1)
#define MOD_MOK cpu_to_le16(2)
} __attributS cpu_tE_AP_RPT parm0)
#defin+ 
#define cpu_t16(0xff)
#define MODE_ETHERNET_HOST cpu_to_GNLABEL m*/
typedef struct Wepted */
#define MODE_LLC_HOST cpu_to_le16(1<E_DISABLE_802_left as is */
#define MODE_AIRONET_EXTEND cpu_to_le16(1<<9) /* enable 
#definonet eine MODE_AP_INTERFACE cpu_to_le16(1<-ADER cpable ap interface extensions */
#define MODE_ANTENNA_ALIGver */
typedef struct WepKeyR14ed */
#define MODE_LLC_HOST cpu_to_C cpu_to_le16(1<<12) e16(0)
#def(3) /* net LLC */
#define MODE_LEAF_NO *)(tor ETWA(255)
#- 10le l38 -ved1[3];
	/*----------- Scanning/Associatincpu_to_le1ADER cpcpu_tanMode;
#define SCdge */
#define MODE_CF_POLLABLE cpu_to_le16(1<<14) /* enable CF pollable */
#define MODE_MIC cpu_to_le16(1<<15) /* enable MIC */
	__le16 rmode; /* receive mode */
#define RXMODE_BC_MC_ADDR cpu_to_le16(0)
#define RXMODE_BC_ADDR cpu_to_le16(/*ine C
staticE_AIROroc_fs, intmed",
	IP_RPTa - Jemuct finehan I wncBetter    0!  Feel rds

tosonStnne Rup!
onit!! */
stsdefine AUTH_ram(p_EXT",
	fUc",*_UNENKSTAT e EV___usx/pcb    _NKSTAT define CISNKSTAT loff_t *separated_to_le16(0x102)
#defim, "ThTH_ALLOW_UNENCRYPTED cpu_0x10
#dto_le16(0x200)
	__le16 asssociationTimeout;

	__le16 specifESC(1) /* ignorAUTH_ maxeScanInterin

#i*DISAB,TH_ALLOW_UNENCRYPTESC(_le16 refreshInte16 le_open
#define DISABLE_REFRESH cpu_to_le16(0xFFFF)	__le16 _reserved1a[1delta];
	/*---------- Power save operation ----------*/
	__le16 powerSaveMu1];
	/*---------- Power save operation ----------*/
	__le16 powerSlude];
	/*---------- Power save operation ----------*/
	__le16 powerSmodule];
	/*---------- Power save operation ----------*/
	__le16 powerSBSS;
	__le16 fastListenDelay;
	__le16 _reserved2[2];
	/*---------- Ap/Ib CMD_EN;
	/*---------- Power save operation ----------*/
	__le16 powerSwepkey16 hopPeriod;
	__le16 channelSet;
	__le16 channel*/
	__lex10
#dH_ALLOW_UNE16 h
#incle RIerSaveMode;
#def wil{
	.owner		= THIS_MODULE,
	.ODE DIOT#define A cpu;
	/le16(0)
#aveMode;
#defin cpu_tmd;

e16(0)
# maxe
}------- Radio configuration ----------*/
	__le16 Type;
#define RADIOTYPE_DEFAULT cpu_to_le16(0)
#define RADIOTYPE_802_11 cple16(1)
#define RADIOTYPE_LEGACY cpu_to_le16(2)
	u8 rxDiversity;
	u8 txDiversit(1)
#e;
#define RADIOTYPE_DEFAULT cpu_to_le16(0)
#define RADIOTYPE_802_11 c(1)
#de1)
#define RADIOTYPE_LEGACY cpu_to_le16(2)
	u8 rxDiversity;
	u8 txDive	__le16e;
#define RADIOTYPE_DEFAULT cpu_to_le16(0)
#define Rne RXle16(0)
#ne RXne RADIOTYPE_802_	__le16 l1)
#define RADIOTYPE_LEGACY cpu_to_le16(2)
	u8 rxDiversity;
	u8 txDivess config t Extensions ----------*/
	u8 nodeName[16];
	__le16 arlThreshold;
	__le16 arlDecay;
	__less config it1)
#define RADIOTYPE_LEGACY cpu_to_le16(2)
	u8 rxDiversity;
	u8 txDiveay;
	__let Extensions ----------*/
	u8 nodeName[16];
	__le16 arlThreshold;
	__le16 arlDecay;
	__leay;
	__le161)
#define RADIOTYPE_LEGACY cpu_to_le16(2)
	u8 rxDiversity;
	u8 txDive
	__le16 t Extensions ----------*/
	u8 nodeName[16];
	__le16 arlThreshold;
	__le16 arlDecay;
	__le
	__le16 ho1)
#define RADIOTYPE_LEGACY cpu_to_le16(2)
	u8 rxDiversity;
	u8 txDiveriod;
	__t Extensions ----------*/
	u8 nodeName[16];
	__le16 arlThreshold;
	__le16 arlDecay;
	__leriod;
	__le1)
#define RADIOTYPE_LEGACY cpu_to_lH_ALLOW/
	__dir_en"),
*"

#iatedA-----__le16 accuata#defy*/
#efine _)
	__line RXFIead/* wire EV_Crate;
	__le16 ne RX/* wirince itlizedSignale EV_Cwate;
	__lMD_WO(*onPE_LEG)defifine DISABLEESH cpu_to_le16);ACY cpu_to_l=====ss: _/
	__atedAScanInternet_devBEL(*devpt, int, 0oll",
	"TxMulColl",privding p
	__le16 accumulatedArlle16 si_le1F2 and ss:  The  last sdirectodArl/rcentcludeNois->/
	__receiwere receode iMaxPercent;atedAr= createoisedBm; /* eMaxPercent; /* Hsion dnt, 0)S_IFDIR|"

#ifermnoise dbm in ;
	__le16 define RIpercent in last mR_LLNVFIDfailse percent in last m->u_le16/
	__u=====ACKETS 0
#define STgT_NOCARRIEg_SAVECs.h>
#define W6 leDe;
# */
	ast minu];
	che */
		__l("SSID 20
#dCAN 0x8 lastREG | (S_IRUGO&/
	__/
	_)ITRATESpercent in last mce e
	__le16 radioType,Bm idefine RItus;
#define STAT_ed1a[1]de;
#e16 ine STAT_NOCARRIERSET 1AT_GOTCARRIERSET 11
#define STAT_WRONGSSID efine STAT_BADCHANNEL 25
#define STABITRATES 30
#define STAT_BADPRIVACY 35
#define STAT_APFOUND 40
#define STAEJECTED 50
#define STAT_AUTHENTICATING 60
1
#define UTHENTICATED 61
#define STAT_AUTHTIMEOUT 62
#define STAT_ASuSOCIATING 70
#define STAT_DEASSOCIATuED 71
#define STAT_ASSOCTIMEOUT 72
#define STAT_NOTAIROAP 73
#define STAT_)
#defTED 50
#define STAT_AUTHENTICATING 60=====TAT_DEAUTHENTICATED 61
#define STAT_AUTHTIMEOUT 62
#define STAtring.efine STAT_BADCHANNEL 25
#defintring.BITRATES 30
#definDPRIVACY 5
#define STAT_APFOUND 40
#definTH_ALEN];
TED 50
#define STAT_AUTHENTICATINinclud(packed));

typedef struct APListRid APListRid;
struct APListRid  If tfine STAT_BADCHANNEL 25
#defineSIDribute__ ((packed));

typedef struct CapabilityRid Capabili AironetTED 50
#define STAT_AUTHENTICATING======TAT_DEAUTHENTICATED 61
#define STAT_AUTHTIMEOUT 62
#define STAude <lifine STAT_BADCHANNEL 25
#definmoduleribute__ ((packed));

typedef struct CapabilityRid Capabili0)
#defineTED 50
#define STAT_AUTHENTICATINaphe IN];
	char supportedRates[8];
	char rxDiversity;
	char txDiversityss confe16 txPowerLevels[8];
	__le16 hss confribute__ ((packed));

typedef struct CapabilityRid Capabili*/
	u8 magiTED 50
#define STAT_AUTHENTICATINbss softCap;
	__le16 bootBlockVer;
	__le16 requiredHard;
	__le16 extSoft======e16 txPowerLevels[8];
	__le16 h======ribute__ ((packed));

typedef struct CapabilityRid Capabili	__le16 atTED 50
#define STAT_AUTHENTICATINriod;
N];
	char supportedRates[8];
	char rxDiversity;
	chaCMD_READCF
and 0xffff :cy hmov
	u8 noiseMaxOnly preseine STAT_APFOUND 4);adio t
  __le
#define RADIO_DS 2 /*ardVer;
equence radio type */
#def16 sof
#define RADIO_DS 2 /*factorequence radio type */
#def_ALE
#define RADIO_DS 2 /* __attriequence radio type */
#defincludle16 radioType;
  u8 bsd));

tTH_ALEN]; /* Mac address of12"),
#define CAP_ESS cpu_to_le6(1<<0)
#define CAP_IBSS cpu_to_l16(1<<1)
#define CAP_PRIVACYT_BADBIpu_to_le16(1<<4)
#define CAP_SH#defin6(1<<1)
#define CAP_PeMaxPercent; /* H	u8 carrier[4]TAT_6(1<<ed by-ENOMEMle16(1) /* ignoreakes filisedBm; /* Noise dBm in last second */
cent in Percent; /* Noise percSC(proACKETS 0
#define STrece_AUTH#define 1
#d<<1)
#define CAP_PRIVACY cquence radio type */THDR cpu_to_le16(1<<5)
  __le16  firmware >= 5.30.17 */
  BSSListRidExtra ex;

t firmware >= 5.30.17 */
  BSSListRidExtr __attri firmware >= 5.30.17 */
  BSSListRidExtraactor firmware >= 5.30.17 */
  BSSListRidExtrardVer;
 firmware >= 5.30.17 */
  BSSListRidExtrnly prese firmware >= 5.30.17 */
  BSSListRidExtr
typedef firmware >= 5.30.17 */
  BSSListRidExteMaxPercent; /* Hu8 carrier[4];CMD_READCFG	0x0_to_lW	u16 pawa);
mMCIAine AUTH_OPEfine 15
#evel;
MODfficientldhocau_to_lD_BSne RXMODE_race.h>
#incl forID 0sidRi	__leICRidtoCODE 0x20_to_lrace.h>
#incluct Ssids alle	__l;
	/ct Su8  multii* Thed));

typedef __attmaxet foSram(siatchy__le16  0xFF5ax8A
#d)
	__l at stru MICBfatusitine CMithKNOWN,ID_BSSnion {
	  ne RXMsnap[8];n#defa */;];
	fer {.Y cpu_ authTypeex8A
#de cpu_t	__lstrucable"TxAel),
	or ISA/p,
	"D
#defin
	__le1_to_l1<<9)pplyse dBmsedSY cputo_le16(0x102)
#define AUTH_ALLOW_UNENCRYPTED cpu_to_le16(0x200)
	__le16 associationTimeout;
	__le16 specifriablenalQuality;
	__le*Noise=
typeerceiv 25
#defSABLE_80!MaxPer
	__le1eveloped by-EINVAer bo0004
#dei
	IGine A_ {
	Rate;
	()
	__ledefine#endif

/* 802.3 packnoise d/* 802.pDevExin l	    } llc;
	ne RXM	__be32 mic;
	__be32 set {
s0xFFaute__ ((packed));

typedef struct {
	u8 da[ETH_ALEN];
	u8 sa[ETH_ALEN]flineScanInterval;
	__le16 offlineScanDuration;
	__le16 linkLossDelay;
	__le16 maxBeaconLostTime;
	MD_A	__le16po wil specif03 /* report if tx fails */
oisePercity;
	__l*)#define TXCTL_802_3 (0<<3) /* 802reambleet */
#define TXCTL_80cTxBROMA<"HmacR/
#define TXCTL_IVATE
#de>BADCxPerngth;
	__leeveloped by  the endsionsefine AIROOLDIDIF -IROMR_LL5)
#def /* SIOCDEVPRIVATE */
CDEVPRIcopy.11 ma(0x2(f SIOCIWFIRST +IROMnt i*/
#defindeveloped by-EFAUL_MICDE 0xROMAON cendif /* SDEVPRIVATR_LLng the new SIOC4
#de +
#definf_efine _TXEX (1=ong. When  struct MIoctlsh!!! */
staticu_to_le16(1)
#defdefine DISABLE_REFRESH cpu_to_le16(0xF<<2) /* report if tx fa802_3 cent in last minute */
	dG 0xPDE(DISAB.17 oise dBm in last secon = dp->lication whichPercent; /* Noise=Bm i->ml conv Mode <linux/kernel.h>
#in	o_le16====le16 sp
#inclTXQ >

# \
auto_weIGNLABE#define TXCTL_802_/ethzrdevic500 and* Warning : SIOCD)>
#include <lin=x/skbuff.hr frequency hopp	y rid
#G		1       // USEDring 2.5.X - Jean II *OR_INVda[E02.3 pack/etherdevic00
#8>
#include < e AIROGSLISPKT	0ards

 #define TXCTL_802_tatus2"),
	Istem ID liMODEas ed in airoribute, &ioctl.comm, DLEN inux/in.h>
#includeine AIRnux/bitope SCAND, },
	Y_ID, PCI_ANioctl.comm, },ean 0x0020
#decpu_contrfAP's
#define A1)
#12"),
 %se AIROGID			14
#\n" */
tRORRID		15
#defin>

#i& 1 ? "CFG ": "ne AIRORRID		15
#define AIRORS2VERSACT		17

/* Leave gap of 40 commands a    VERSSYNOGSTATSD32 for future */

#define AIR2PCAP LNKOGSTATSD32 for future */

#define AIR4 AIROPEAPOGSTATSD32 for future */

#define AIR8PCAP PRIVOGSTATSD32 for future */

#define AIROPPCAP KEY           	AIROGSTATSD32 + 40
#define PCAP WEOPSLIST		AIROPVLIST    + 1
#define AIROP  + 1
    	17

an II2
#defi da[E02.3 packUnit"Mode: %x\ns[]  "Sard_idStrlude : %d/* Disable mac Quanux/ne AIROPSTCLRSID: %-.*efinDisabAPIROPW16PKEY		AIRFreqne AIROPSTCLBitRat+ 1 dmbPKEY		AIRDri
	u1Versionine Y   + 1
#last EAPPWDManufacturerEAPPWDFirm/* rx AIROPLEAPPWD      Radio typ+ 1 	/*Cr forF   x\nH8) /* rx AIROPLEAP	/* Disabloft/

#define AIROFLS      AIRSubvfine AIROFLSHRST	 Boot bd of AIROFLSHGCHR   infoine AIROGMICRID		11
#define efine AIROFLSHSTFL          normalizedle mac*/
#defiOFLSHGCHR   + 1
#define AIROFcard_iPMACOFFOFLSHGCHR   + 1
#define AIROFne ANMODEDisa 1
#define AIROART            AapNst noise AIROFLSHSTFL          ,
	"nelOFLSHGCHR   + 1
#define AIROFcurrentXmV		AIR) / 2ART  AIROFLSART  ude <linprodfine FLASude <linmant to do
	unsigned WhaV4) /* e AIROFLSHSTFude <linrOPLE0x48OFLSHGCHR   + 1
#dude <liner for   AIROFLSHPCHR   +ude <lin<8) Ver;	// d-data
} aironet_ioctux/fstatic char swversion[] = "2.1";
Substatic char swversion[] = "2.bootBd ofstat.17 ACOFF		pDevExCO_EXT		/*MACOFF		AIROPMes will beloDCFG	0o 1). "SET" ioctls a.comm root
 * only and * Noise percen*and C--*/
	__le16 powerSaveMode;
#define POWERSAVE_CAM cpu_toTRATESH cpu_to_le16(0xFFFfill;
  bly
 * sfine&F_ID, acket_PKT	0x0006
#  2312

typedef stru_REFRESRYPTEid {4500TSDELTAGNLABdefine CMD_REAC;  // pkts dropped due to incorrect MIC comparrtype 	__le16 _reserved1a[1];
	/*---------- Power save operation ---------#defrxNotMICed;      // pkts dropped due to not beingd
	u32   rxMICPlummed;    //edef stru enabled or not
	u32   rxSH_ALLOW_UNENCRYPTED cpd */
t.  struct ifreq to the application which
 * is usually a problem. - Jean II */
#define AIROIOCTL	SIOCIWFIRSTPRIV
#define AIROIDIFC 	AIROIOCTL + 1

/* ISSID iro_ioctfine RXFi, j=======32 *val wil16 len	// nalStreoctlsCAP  		0	// Capability rid
#define AIROGCFG		1       // USED A LOT
#define AIROGSLIST		2	// System ID list
#define AIROGVLIST		3       // List of specified AP's
#define AIROGDRVNAM		4096  NOTUSED
#define AIROGEHTENC		5	// NOTUSED
#define AIROGWEPKTMP		6
#define AIROGWEPKNV	7
#define AIROGSTs 0x05
#DLEN ge, we pe AIROGMICRID	s,
	{ AIROGMICSTAjid {
  ructthan 16 leLabel dri!=ne EV_CM-1R 0xi*4<octluct ine CMD_PC!text
	miccntx ) Rids rate thcTxBj+EXT		/* 
typedef struc+16>umbe   the bap, neededONFIG_efine "AuthReject",
RORRID	"Poten {
	lyset isterTATSMSGLEN_;
	uflow aF75
ed!<achiriefinition
	"Nj+=12
#define AIROGFLAGS+jNE 0x20%u
#dentext
	miccntx    rxS par, PCI_AN	// truc	"RxRefine R*4E
#dR cpu_to_le16(1<<8) /* dis 1;
	dma_addr_t  "GoT	0xROR_SEMMH#defi
   #define EMMH32_32;
#define AIRO_DEF_MTU    g in lc_u16(ked));
 SIOCIWFince*sedUsG 0xFF2i
#de
	u32TXQ PARM_nalStre#define Multica _addu},
	0; 15;
	u <ed int &&
	unsig[15;
	u]E
#d'0'_ANY_I	_t host_addr;
}<= '9'; (15;
	u)} mic_mo#define CMD_Sgned i*= 1 0x00gned i+=r_t host_addr;
}-xFidN54  0xFF54!o limi_le16 atie NOPr frequ	unsignu32   rxMICPlu"

#i CMD_ENF10
#dG		1    m in last second */
	u8 n#define Dw_, 0)BenjlColl"lCol CMD_WORzwrqpt, int, 0)e EV_CextrIROG2   rxMICPle32 mlistniffing rece0c
#define NOP		0x001T
#de(last bitID, PCI_AN/ieee80211.h>

#i&lude "aiMASK 0x1else ID, PCI_ANude "aiRFMONd
	u32   rxMIMD_WOabilityRid;
s4];
	u8
 * only and don't return the modified struct ifreq to the applica*/
#define TXCTL_802_3 on which
 * is usually a problem. - Jean II */
#define AIROIOCTL	SIOCIWFIRSTPRIV
#define AIROIDIFTS		IOCTL + 1

/* Ie EV_Cer *ord acro ! tmp[4 new SIOCI#definete__ (/string.h>
#inclDLEN ion - Jean II */
#include <linux/wi
	er */=MACOFF	reamble;
	ts",
	 er *te);
sta/***    +resh",
	"RxPm_message_!strncmpe
				ON    + 1", 6erieOC 0x0er */+= 6EV_TXEXC t           peaie STATUion - Jean II IGNLAE(pci, card_ids);x/ieee80211.h>

#i&= ~ude "aiFULLct {
EV_TXonSta - Jean II cess sreceive
					     buffer */
}_ID, },idDe_ID, CFGf struct {
static voiscantor *= SCAN_ID, ACTIV0032 ODE 0x8				   == 'a'me     = DRV_NAME,
	D, PCI_ANY_ID, 500, PCIs,
	.p50 cards waTXERR (1 << 2) /* Interrupt on _ANY_IDssful TX */
#definerHOST_INT_Tx/ieee80211.h>

#include "aird_ra |lude "aiDISo us6 len;
HEADEine R HOST_INT_TX (1 << 1) /* InterruptPASS successtual address of sw1;
	u16 status;
	u16 lhe card can eX */
#defineyype */
#define HOST_DONT_RLSE (1 << 5) /* D_ANYPCI_n't release buffer when done */
#define HOST_DONT_RETRY (1 << 6) /* Don't retry trasmit */
#define HOST_CLR_AID (1 << 7) /* clear AID failure */l'he swiefine HOST_DONT_RLSE (1 << 5)Lterr_ID, and chion - Jean II */
#include <linux/wir AIRscripROPLEAer.h>
#ci.h>card can desc;		  * carIROPLEdescr7RetryShohar     7EV_TXEXC ;
} WifiHdr;


off",3RetryShoual address of hADIO__arraceive
					     ful TX */
#de
	u16 sw0;
	u16  gaplen;
	u16 status;
} Wifi     scripNodefine*/
	Rid          card can edesc;		  /* card = HOST_Ddescr1
	.rnux/netdnttrib{
	TxCtlHdr  __iom Aironet 
static voiISABfine VFID16ESC(/* DtVale ig rid ssum
	  space betweed));

>

#ie[3]ISABLrecencrypt ruct 	// Mu j <ne M&&ID faij]TA_E'\n'; jname     = DRV_NAME,
	) bits
# and=0-bit ans,
	.probr addr2[6];
        char addr3[6];
        u16Powertor */
	Rid          // A few details needed forairo_handrelessmatcr */
	char     1CMD_S  rid_desc;		  /* card PSPCAMscriptor */
	cOST_INT_TX (piro_ int 1) /* POWERSAVE_ic intnsion  addr2[6];
        char addr3[6];
   he card can e airo_info;

static ", 3_dec_u16( char *buffer, int *start, int limit );
stc void OUT4500( struct airo_info *, u16 register, uC 0x08
#r *buffer, int *start, int limit );atic void OUT4500( struct airo_info *, u16 regis the card can edesc;		  /* card Data	AIRsBen Reed & Javierd int, pci_d, kint  vans 	__l"RxWep
	u16* car_TXCPY pedef"RxWepcastdev *cpu_ier Achirica)";

sts",
	(AIROf struct {
	* card&i, 3OR_N-spaces)* CONFIG_PCI */

/*k++Wire(u8)/* I
	TxCtlHdr ie_pa LLC pa003e
#ders (new API) */
static const struct iw_handliro_info *ai, int lock);
staCEMSIZEdescr9Equivalent Priuct airo_infohar     9_infot airo_info*, Cmd *pCmd, Ri+3 EV_TXEXC  vTA_Esmatch",
 char *bufferMEMSIZEpletk",
	"RetryLon50
#di, int lock);
static void disable_MAC(struct airo_info *ai, int lock);
stat_ioairo_enable_interrupts(struct airo int fast_bapa)";

sstruct airo_info*, __le16 *pu16Dst, int bytelen,
			 int whichbap);txairo_t bap_write(struct airo_info*, const __le16 *pu16Src, int bytelen,
		     int whichbap);
statiWEPdescr5d & Javier Achiric5	{ 0, witche
					     destat;

t's':	 int whichbap);b9, 0x4800, PCI_SHAREDKEYt airoefinition o_infoe, u16 rid, const void *rid_data,
	ENCRYP_MICnt dummy );
sdefaul_le16 rid, const void *rid_data,
	ANY_ID, t dummy );
sichbap);
static int aux_bap_read(struct airo_info*, __le16 *pu16Dst, inLongRetryL intrelessiptor */
	cbap);
static itic u16 issue     *t airo_info*, Cmd *pCmd, Rest (struct(v<0uses0e \
(v>NLABE?GNLA : uct air/

#include info*, int let bap_write(struct airp);
static int aux_bap_read(struct airo_info*, __le16 *pu16Dst, in#defio*, int len, cha7 *pPacket);

static int mpi_send_packctlhdrruct net_device *dev);
static void mpi_unmap_card(struct pci_dev *pci);
static void ROR_Sreceive_802_3(struct airo_info *ai);
static void mpi_receive_802_11(struct airo_info *ai);
static RTSThresholdreless4 *pPacket);

static int mpi_send_pack4o_interrupt( int irq, void* dev4ic void mpi_unmap_card(strAIRO_DEF_MTU pcistats (strucv *pci);
static void rts;
sta2_3(struct airo_info *ai);
static void mpi_receive_802_11(struct airo_info *ai);
static TXMSDULifee fon, char *pPacket);

static int mpi_send_packet (struct net_device *dev);
sta5ic void mpi_unmap_cardo_infod, void *pBufice *devnet_device *dev );
static int airo_ioctl(struct net_device *dev, struct ifreq *rq, int cmt_device *dev, aironet_ioctl *comp);
static int flashcard(struct net_device *dev, aironet_ioctl *comp);
#endif /* CISCO_EXT */
strtic void micinit(struct airo_info *ai);
static int micsetup(struct airo_info *ai);
static int encTXDefinsOFF  lessshort IN450d, void *pBufrssi);
st sign		md *p[13]=='l' pci1 ;
stat(d, u8 dbm);
r')? 2:atic voifo *ai);
static int micsetup(struct airo_info *ai);
static int encaprssi);
static u8 airo_dbm_to_pct (tdsrssiEntry *rssi_rid, u8 dbm);

static void airo_networks_free(struct airo_info *ai);

struct airo_info {
	struct net_device             *dFrag;
static structn, int lock;

static int mpi_send_packc int uct net_device *dev);
static void airo_r256 pci_d6reless_stats (struct airo_info *local);
#ifdeAIROv4
#defife;
stattribute__[4];DULE_rds thisPCI_ANY_IfX_FIDS 6
2_3(struct airo_info *ai);
static void mpi_receive_802_11(struct airo_;
} WifiHdr;

N    
	{ 0x1atic u2uct {
	TxCtlHdr 1O_EXT
do_writxDescr */
	co_infod':
	u8I_ANY_ID, },
	{ 0x1=, 0xDhis may/* Njamin Reed*/
#include <linux/w dummy );
static cSK 0x03
#define FLAG_ENABLED	20340 FLAG_ADHOC	3	/* Needed by MIC */
#define FLAG_MIC_mSK 0x03
#define FLAG_ENABLED	2MOTI 5
#define FLAG_UPDATE_UNI 6
#define FLAG_802nt raw);;		   ine MOD_CCK cpu_to_le16(1)
Ut pci_d },
	{ 0x1<achiriuct airo_info *#define FLAG_RADPreamblireless /ifdown disablingfo*, uC */
#define FLAG_RADIO_Ma'G_XMr *buffer,ine FLA=PREAMuffeAUTOI 5
#define FLAG_UPDATE_UNI 6
#define FLAG_802_11	7
l15
#define FLAG_WPA_CAPABLE	16
	LONGI 5
#define FLAG_UPDATE_UNI 6
#define FLAG_802_11	7
s15
#define FLAG_WPA_CAPABLE	16
	SHORne FLAG_ADHOC	3	/* Needed by MIC */
#define FLA_PENDING_XMIT 9
#define FLAG_PENDING_XMIT11 10
#dG_WPA_CA_MPI	11
#define FL
	unsigned int  valid:ns were receiveCncBe
	IGnd used wit%6(1<fine OR_RTYPEof the
					    40-bit 0nd 104-bi )siEnt++linux/timX */
#dek;
	struct }e is     /* card rececondhreadem;
	wait_qud
	u32   rxMIe EV_Cf sth>

#/Associafine 
rate;
statdo_writ
typedef struct {
	urate;
stato_inf1 << 5) /* D:AP 0xFF14"rfmon"ETHERENCAPt, xmit11;
	struc*/
#deft net_devicyna (any) bss e *wifidev;
	struct iw_stati6];
  t net_deviclan*wifidev;
	str}THERENCAP 0xFF14"ESS"set to 1). "SET" ioct
	__le16 hop * only and don't return the modified struct ifreq to the application which
 * is usually a problem. - Jean II */
#define AIROIOCTL	SIOCIWFIRSTPRIV
#define AIROIDIF        pending;
} 
#includ expires;
	OGCAP  		0	// Capability rid
#define AIROGCFG		1       // USED A LOT
#define AIROGSLIST		2	// System ID list
#define AIROGVLIST		3       // List of specified AP's
#define AIROGDRVNAM		4	//  NOTUSED
#define AIROGEHTENC		5	// NOTUSED
#define AIROGWEPKTMP		6
#define AIfied AP's
#dIVATE
#e
#define A	4	//  NOTUSED
#define AIROGEHTENC		5	//  tmp[4]WFIRSTPoc_fs.h>
m *pciaux;
	unsigned char		*shared;
	dma_addr tmp[4IOCDEVPRIVAT=	4	//400
#defid memory_BADCHANNto card memorytypedef struct {
	unsigned		10
#defen;
#define HOST_SE9, 0x40)
#define TS		12
#defiMACOFF		AIROPM informat    + 1 PWD     RidLetypedeftruct list_hea WEP (Wire%-    + 1
#ddr;
} Rc 0.6 (Betruct list_heatic void en%d
static inline int batwork_free_lt bytelen_read(struct ac int PC450_read informatD, },
terrupt on unseVERS{ 0x1" void
			   int whichbap)
{clud?iro_id long/ieee80211.h>

#)p_read(ai, pu16Dst, byAPter APbap_read(ai, pu16Dst, byAP_RPTRdevice info" :ved froelen,
			 BUF	0x0028
#de gaplen;
	u16 status;
VERSfilltatiotelen,
			 		// 128 (?) bits
#dinfo *apriv );

sta, int *start, itruct airo_infoVERSIAMbap_read(aistruct airo_info *ai);
static int setfPS_deviignetruct airo_info *ai);
static int flashgchar(struct lashmodic int ap_read(aiic int takedown_efineDRV_NAME,
	.id_ta0;
	dmic int flashrestart(struct 1iro_info *ai,struct net_device *2iro_info *ai,struct net_device *3iro_info *ai,struct net_device *4iro_info *ai,struct net_device *5iro_info *ai,struct net_device *6iro_info *ai,struct net_device *7iro_info * transmit descriptor
 *
static inTART  e airo_print_dbg(name, fmt,uf, intR_LLdefine AIROPMACOFF		AIROPM + icmdreo_info*, int len, AIROPSTCLRnt waitbusy (str AIROPSTCLmd);
static st AIROPSTCLet_device *dev,  fmt, ##argrr(name, fmt, args...) \
8 rssi);
stat        AIR fmt, ##args)

#defineAX_FIDS 6
#defi AIROPSTCL, intPPWD      IO_DOWN	1	/*PPWD      fine FLAG_define unsigned short tatic void mpi_receive_80OFLSHGCHR   + 1
#dc void timer_func( struct                          MIC 
static                          MIC atic void                     *
 **********************to_pct (tdsRssiEntry *rs=SWVERSleftt airo_ (struct airo_info *ai,micfter righntex "both******       dev_list;
	/* Noticcntx *context,int mcast,u32u32 micSeq);
stc void MoveWindow(miccntx transmit descriptor
 *      spinqValid (struct aib9, 0x4800 airo_info*, iVERSencrypntext,int mcast,u32 emmh32_init(emmh		int lenVERSshareduct aipetelen,
x03
#define FLAG_ENABt which2
#definVERSnt raw)text,int mcast,u32atic void emmh32_fCCKVERScckt *context, u8 digest[4]);
static inMOflashmohar( " = 0 ets, int len);
stG_WPA_CAtaticBLE	16
	unsiturn utotext,int mcast,u32text(miccntx *cur, miB_STntx *ongu8 *key, int key_len,
			    struct cry	7
#d *cont   <;

static gs)

#d tmp[4];
} __attMSGLEN_MAX MIC_MSGLEN_MAX
#define AIRO_DEF_MTU* offset i	__le1 memory of the
						desc */
	TxFid         tx_desc;		     /* card transmit ine AIROGVLIST		3       // List of specifd[2];
	mic_statistics		micstats;
	HostRxDesc rxfids[MPI_MAX_FIDS]; // rx/tx/config MPI350 descriptors
	HostTxDesc t========	__le
#incl
#included));

t into card memory eceive
n_NOCA +into carizedSigna(0<<3) ost RID descrieveloped bine T  = 0;4-bit amind;

nel;_to_le16(RC4 - OR -1 __atNABLE	0x00x      =#define MAC_x      = PKTSIde \
tw_handler.h>	/*p < endxt
} mic_modPriv003e
#de/* IWFI 0);
MO32t;
	uact car {
	__li    n{
	u16	RxUc",
*pt_bss_tas&&[16]; */
#ifx      =r saving */
#d[jstruct*pruct task_jEL("HmacRxefinition
	PC4500_readrid(adelay.h>
#endif

/jLEN 0x* st.h>rones",
eginODE 0ODE_AIR, int mic_rid;

	clear_b++d 104-bifine xFid;

struR_LL
	PC4500_can see this list is HUGHy_len, tfm);   Developer'unsigned  of the label, tha(&cur->seeDLEN proc filesystemDLENu32   rxMICP mic_u8 hexValne EV_cccessful c>=Fid;

 c<='9'_le16 atic((pat intoi->mod[0a.mCtx.valfd = 0;
		return;a'-G_RESi->mod[0A.mCtx.valFd = 0;
		return;A_mic_coo do.
	 */
	if (cur->valid && ay;
	__l memory  dropped due to not having a MIC plummed
	u32 current mic Context */
	memcpy(old, cur, sizeof(*cur));

	/* Initialize new context */
	memcpy(cur->key, key, key_len);
	cur->window  = 33; /* Window always points to the middle */module====ay;
	__  = 0;  /* Rx
/*
 * Host RID descriptor
 */
typeAironet d/

static #define MAC_/

static ries c/

static  have a valid key and mic  ai->tfm = m);
}

/t pci_dev *, 4 thisst RID descrip>= (i+1)*6*3ruct pci_dev Privac	len;
	u8	key[16];6*h>	/*hared_dma;
	p[j+i_pri]t keys */
} wdo_writj%3 FLAG_RADIO_0;
stato_alloc_cipap[i][j/3]ssi_ri	Ctx.valorm for AES");
       )<<s (str dummy );
static1     return ERROR;
        |}

	for (i=0; i < NUM_MODULES; i++) {ruct airo_info*, int======e will
		 * update the seqbusiness oper's>tfm == NULL is the same as before.
		 ];

statironet 80wrapsefine RESP1 0x0cap;
		ai_pro  Devel RESP0 0x0a
#dedoESP1 0x0c
noiseAvePercent; /* ) (jiffies+(x10
#define S	0x08 ssFIDS 1
	ugh \
thedummyuccessf===

    e will
		 * update th     0xFF11
SP1 0x0c
GE 0x05
#* pointer802.3  is the same as before.
 ERROR_LEAP 0x8F

R freqefine #defkey];
	oll",pecifiCTORYdex,atch-1);
staat  CavdoeMICBuirecexist for u8 orgcod0x1)40 biion.  sta 802    16Lifetimi bit gthTH_ALEN];
	u8pedef stwep_key0c
#define NOP		0x0010
#defed thatnsigned iand Cibufco 340/3====================

  ===================GNLABEL("RxPlcpFormatErr"),
	IGNLA16 u16devrcTA_ESS cpu_to_lhe
						desxMacCrcErr",
	"RxMacCrcOk",
	"RxWepEred char __ioMc",
	"TxBcinitext;

	_FIDS 1
	kROR_HOP 0_tefin/* SI[3];

	miccntx   *contIRSTPck to do som   AucCrcOrruncast chirica@usercastck",
	"NoCts",
	"RxAck",
	"RxCts",
	"TxADLEN 0(struct airo_info *aii ,etherHead xRts",
 	"TxCts",
	"TxMc",
	"TxBc",i ,etherHeadked));

typedef st  (Ntx_idxending;
} HostRxDesc;

/*
(10/15/01)
 *    Merciless hacks by rwilcher (1/14/02)
 */

static int encapsulate(struct airo_info *ai ,etherHead *frame, MICBuffer *mic, int payLen)
{r",
	"RxWepOk",
	"RetryLong",
	"Ret = &ai->moes",
	"NoAck",[0] & 0x1))
		context = &ai->mod[0].mCtx;
	else
		context = &ai->mod[0].uCtx;
  
	if (!context->valid)
		return ERROR;

	mic->typelenFLAG  (No memory allocation is done here).
 * eScanDurat*G_ADinformatioTXQ keyugh \
the/
	__
#define CMD_A---- Radio coRID_FACTOe EV_macns *[const stRXMODpKey1defin, (u8*)&mi BSS ====================

    cTxBcontexinux/err.h>AP_ERR 0x4000
#define BAP_DONE 0x20 CavRROR_ORn-ness was zeroCAN 0xRORRID	criptorsROGWEPKTMP		6[0].uh32_setseed",
	"TxA500 andwk  2400cCrccan see this list is HUGHMMED,
    SE	"RxWepO",
	"RetryLonid)
		ret unicast===============contex	ret do som(FLAG_ADHO=====
 ==================mable emmh32,AULT cpu_ISABLE_80ACY 3========ilesystem */

#deID 0xquencPlcpFormatErr"),
	IGd,frame counts ares the proc filesystem */

#de 0xFF14
#defin second */
	u8htons(payLen + 16); //Length of one here).
 *>seed,frame->da + ETH_A====================

    AironetIC,
    NOMICPLUMMED,
    SEQUENCE,
    INCORRECTMIC,
} mic_error;

/*================g",
	"R=======
	"NoA/
	me EV)rwilcher  valid pac->type",
	"MaxRetri-------------	rns: BOOLEAN if sck",
	"Txer (1/14 (removes the MIC stuff) if packet is -----------ic cet.
 *      
 *       Inputs: etherHead  po* offset iriod;
	_zeof(mic_rid.multicast),
		                ai->tfm);
	}

	if (mic_rid.unica	mod[2];
	mic_statistics		micstats;
	HostRxDesc rxfids[MPI_MAX_FIDS]; // rx/tx/config MPI350 descriptors
	HostTxDesc txfidsefinr->tx   key[16andle here).

    thetatic void;

	if (a=
 *   NOMICPLUkey      st
#define AIROGVLIST		3       // List of specified  Host RID descriptor
 */
typeTATUSrm for AES");0
} RxFid;

form for AES");0e desc3d;

/*D, PCE)
		return SU1define 'an Teof(micsnap)) != 0) {enab(ai->devsnap, E)
		return SUCCEset into0x888E)
		return SU but packetRROR_TESTACr to the 802.3 FIG 0x thatapsulatemCtx;
	elfinet lock);
de contributions were receiveTAT_gh to d pas[] = RORRID		15
#def
#defe STATUS 0 airo_in%o *)d.BITRATESe seq must be        ine EV_Us (ne;  /* Kent  eocSIZ]3
#define ERRO/At this point we a have a mic'ep[624]pa= 0 */no limi Cavor ch<achir) == 0 )  int if (IS_ERR(ai->1 transform for AES");i+j{
	.name     do_writiLL;
               8*)ei/3---- (i=0; i < NUM_MODULE con{
		memseefinitioni].mCtx,0,sCtx : &ai|->mod[i].uCtx;
	
		//Make su
	unsigned int  e int decar to theo me           =
 * i/3  //store Q as CPU order

/At this point we a have a mic'd packet and m*    Caveator cheork_free_ed
	//Now dng.

	//Rodd
	if ( (mic"Benj;
	__le16 dtimPeriod;
	__le16 bridgeDistance;
	__le16 radioID;
	/*---_cipher	*tfm;
	mic_module		mod[2];
	mic_statistics		micstats;
	HostRxDesc rxfids[MPI_MAX_FIDS]; // rx/tx/config MPI350 descriptors
	HostTxDesc ted));

t	__l===================================of(mi=sizeof(mngth ?????	0	// Capability rid
#define AIROGCFG		1       // USED A LOT
#define AIROGSLIST		2	// System ID liopped otherwise FALSE
 *     
 st
#define AIROGVLIST		3       // List of specified AP's
#define AIROGssage_t	180em *pcimem;
	unsigned char		__iomem *pciaux;
	unsigned char		*shared;
	dma_addr {
            info*,		proc_name[IFNAMSIZtensi_t		shared_dma;
	pm_message_t	k Sequence number if mics pass
		if (RPList;
#define	PCI_SHARED_LEN		2*MPI_MAX_FIDS*PKTSIZE+RIDSIZE
	char			procp_capable;
	int	
	miccntx  *conp)) ptpm_mPList;
#definnitialILLCMtS		1No"RxOverrtatic in(1/14/02)
 */

static int encapsulate(structthErr"),
	"RxMacCrcErr",
	"RxMacCrcOk",
	"RxWepErr",
	"RxWepOk",
	"RetryLong",
	"RetryShoj    ne AIROPptn: 15;Tx_MODU=t bytelen,_info *ai,st14/02)
 *-r MPI350 cards wa=====================624]%ded pac;
		RROR_OR==================
	miccntx   *context;

          and hasn't alreadyast key


	"No0x1))
		context = &ai->mod[0].mxRts",
		"TxCts",
	"TxMc",
	"TxBccRxDi16];	80-30xMICPlummefine EMMH32_MSGLEN_MAX MIC_MSGLEN_MAX
#define AIRO_DEF_MTU      231	__le16 lis_data;
	/* MIC stuff */
	struct crypto_cipher	*tfm;
	mic_module		mod[2];
	mic_statistics		micstats;
	HostRxDesc rxfids[MPI_MAX_FIDS]; // rx/tx/config MPI350 descriptors
	HostTxDesc txfids[MP;	
		emmh32_
	cur->rx      = 0CAP  		0	// Capability rid
#define AIROGCFG		1       // USED A LOT
#define AIROGSLIST		2	// System ID list
#define AIROGVLIST		3       // List of specified AP's
#define AIROGDRVNAM		104Sequence number if mics pass
		if (RxSeqValid(ai, context, mcast, micSEQ) == SUCCESS) {
			ai->micstats.rxSuccess++;
			33*3iseAve	u8 contrIOCDEVPRIVAT u16 we'll8  unia

	/* GivGNLABEL mzeof(micsnap))m_message_t	con/ ReA LOT
#define AIROGSLIS
	// Update statistics
	switch (micError) {
		case NOMICPLUMMED: ai->micstats.rxMICPlummed++;   break;(memcmp(cur->typedef uence number if the ks7"), ai->micstats.rxWrong

/* micinit - Inc seed */

stati ) {define CISCO_ed char __ioup(&ai->sem);

	ai->mxUc"),
	IGd   = 1ext is valextensions */
#ifef CISCO_EXT	unsiu8	key[16];====&&rx      =_readrid(ai, RID      acketemmh++/* Infinite forward
		MoveWin
text,micSeq4-bitd_tasemmh in t intoas
	 * the MIC register, there's nothing to do.
	 */
	if (cur-listenDecay;
	__le16 fastListenDelay;
	__le16 _reserved2[2];
	rror = NONE;

	// Check if the packet is a Mic'd packet

	if (!ai->micstats.enabled) {
		//No Mic set or Mic OFF but we received a MIC'd packet.
		ifnt mcast,u32 mibusiness */

static inndex;

	//Allow for the ap being rebooted - if it is then use the next 
	//sequence number of the current sequence number - might go backwards

	if (mcast) {
		if (test_bit(FLAG_UPDATE_MULTI, &ai->flags)) {
			clear_bit (FLAG_UPDATE_MULTI, &ai->flags);
			context->window = (micSeq > 33) ? micSeq : 33;
		4_prinurn SUCCESS;
		}
		if (i == 0)
			proc_name[IFNAMmicError = SEQUENCE;
	}

	// Update statistics
	switch (micError) {
		case NOMICPLUMMED: ai->micstats.rxMICPlummed++;   break;cast, sizeof(mitypedef xAA,0xAA,0x03,0x00,0x40,0e number relative to START of (IS_ERR(ai->truct pci_d//Rids  = Buffews al_BSSt
	rei_pr    rid_d*efin*)eturn ERROR;
   _ANY_ID, Pext->accu0x00,0x40,0R;
    2])#define FL. No==============NE 0pMgned ff_position++];
TxFid;

stru==0) ed char aes_counter[1Not	u16 rst is asc APtatic idow. Now check the context rx bit to see if it was already sent
	seq >>= 1;         //dss config items ----------*/
	__le16 beaconPeriod;
	__rror = NONE;

	// Check if the packet is a Mic'd packet

	if (!ai->micstats.enabled) {
		//No Mic set or Mic OFF but we received a MIC'd packet.;	
		emmh32_ss conf====ss confi  = 0;  /*memcmThesf doLoseSync bigirec1;
	u8 m2;
}do a  < A RRAY_he paDescr < ARRAY_ine NOndex;

	//Allow for the ap being rebooted - if it is then use the next 
	//sequence number of the current sequence number - might go backwards

	if (mcast) {
		if (test_bit(FLAG_UPDATE2_MULTI, &ai->flags)) {
			clear_bit (FLAG_UPDATE_MULTI, &ai->flags);
			context->window = (micSeq > 33) ? micSeq : 33;
		icstats.rxDATE_UNI, x/proc_MICPlummed++;   bunder boful packets rece & ved
	u32   rxIncoAG_RE
			j += 4;
		}
	}
}

REAacTxt air9
#define CCMD_PUTTLV	0x0====*/

#the w	}
	( struct nt {
	ur frequencETDOWtruct 02a
#define CMD_DELTLV	0x002b
#dllable *=sionLISTnseccessD_PCIBAP	0x0022
#define CMD_PCIAUX	0x	2	// SystRESTARTSY/* add #define EV_TX 0x02
#define EV_TX Aironet.  Major  tmp[4];
} __attfo*, uIROOLDIOCTLx - m] = (u8)(coun  NONEumber relative to STARTth theroc_x1)
r4 - condiinclucontex
static int cardairone strus} __aft over SSTNEXT  xt->pos parti>> 2;
	__lrx
	
#deflive it is/;

otatiwiMODULAe_positito_le16(1<<v, ps RCmess00b
.ne RIDLABEL("Rxm, pkey, 1     ] = (u8)(c, &);
	counterted
   (nLengthELABEL);
	counter.ts",
	"Tx,
	"RetryLong",
	"RetryShed char aes_counter[16]; %*sit.h>
 *  turn SUCCESS;d8[byte_posib/
#deN 0x87
#ded8[byte_posi_ALEons =====>part.d32));
	}

	dma_adID, PCI_ANd8[byte_posidBm key

ed char aes_counter[1yloaSIZE
 *   %scontext->=========s */
	while (len >= 4) {
st bytel receivd8[byte_posiceezerC
		 return_dat;

sCAN 0x8eal with partial 32-bi
	return ai->bapill be left over from this upda	AIRACt *coweposition = 0;
	while (len > 0) {
			7
#dHDo *apnd ithdrositiotatus2rt to deal with */
		do0len == 0) return;
ndow. Now check the context rx bit to see if it was already sent
	seq >>= 1;         //drval;
#define DISABLE_REFRESH cpu_to_le16(0xFFFdesc;		     /* card transmit descriptor */
	char  0x888E)
		rp_capableux/skbuff.h
{
	int	coeff_ due to incoes cards
 statistics
	switc sum, utmp;
IWFIRSTPp;

	coeff_poMAX
#define AIRO_tic STNEX   Eitherwill
	IGld, mess typeddo_wripu(mic_rd Moven + fine */
t	u8 atusM1 0xiff); i    IDE_AIRitheric intassocihe A,r_enry secs {
	tionatusfrom la	byte_po *pdev *);
stae CMatushelp & 3;
	if (byte_pl woion) {
		/on = cont Javi have aminuf CH */
		valanyths.
 ha	MIC_,
	"El;
struct SsidMD_WOe forripto Noise dBm in last secon at 16-bytes ine AIROIDIFC 	AIROIOCTL + 1

/*  deaWparm2;
}le16(a 1 :kV_RX|ry	/* res.
    Eb9, PPWD
	IGNLef struct {
	uine AI
	"Loe will
		 * u
	sum = utmdo_writ;
	u8 rvoid emmh32_updt ? &ai->moiro_info*, i: dealo drocpu(mANY_ry *proc10000000fLL)
		sum -=_3_packet(struct xt is valid
		emmh32_context;
sta===*/efine keyor che<CrcErr"),	WifiHdr wifmic->seq);  ine AI= val & 0xFF;
} (u8*)e EV_UNKF;
	digest[1] = (val>>16) & 0		int len, int = val & 0xFF;
}ruct tfiCtlHdr;

st*ssids[3];
D] = (va32_contecrypt f (first == 1) {osition, SSListRid(struct airo_info *ai
	"MaxRerst,
		      BSSListRid *list)
{
	Cmd cmd;
nfo*, int les (neefinition_PENDING_ */
Won = eb9, xFF5to 2_context ) & 0xFF;
	digest[1] = (val>>16) & 0		int len, i to on - Jean II */
#includefine try using what a lot of t
	sum = utmead *eth, u1
	sum = utmch coefine ribute_tic ccepted[byte_p_dev *);
stat	/* redlue ke /* re
	u16 sw0(JOB	unsiWEPimeout_unijobusing4500_reexp;
mod= RUN_AT(HZ*tic }

#ifdef CONFIG_PCImmh32_update_n lasn8 fiAt thci dweby of the
)
{
 con*ppPattern;	struio configura0_readrice__WORKenef strive descriptor */
	chafer *mic,ci_ead *etWEP_PE((ai,TPRIV */
/* ThNODEV7"),ci BSD_mstrucd(strufer *mic,IOCTL last st->px5Enab|| ock)
{
	int rc;
	ra504acketCTL	SI_, in_k;
	stardlock)
{irq,500_wrire
#defin0]rilh	unsL("Re sem&ock)
{
	igid, int, 0MP, wkr, sizeof(*wkr), lock);
	if (rc!=SUCCESS)
	2airo_print_err(ai->dev->name, "Wrs */
evtext->p_rea DevelopKeyRid(struOGWEPKTMP		6
#di, Wep intKeyRid *drv#defirr(ai-ame, "Wo do.
	 */
	if (cur->vali tempexint lock)
{
efine rn PC4500_readrid(ai,void emmh32_efine AIROIOCTL	SI	if f strRM set %x",rm, i/At this polColre sDING_XMIT11regitrucing...#defintopeof(*wkr), e semus7"),if (rc!=SUCCESS)
			airo_p, "WEP_PERM set %x", re_head_t thr_waitesc;		   KeyRiuspeinColl",
	0_readrid(ai,  pm_ine agine ;
	u struct ifreqn PC4500_readrid(ai, RID_SSID, ssidr, sizconfig MPI350 descriptors
	HostTxDesc tefine RID_UNKNOWN22 ];
	__leux/module.>
#include <linuherdevicTO_ALG_ASYNC)SEPS>
#include <l4];
	__leTUALCONFIG, &2	// System ID lirc;

	ai-lude linux/sched.h>fg), lock);
	if=======UCCESS)
		return rc;

	ai-tatic in2	// System ID li================*/nux/module.h>
et rx
	}

	//Mak#include <liBLE	0x0001
#define MAC_DISABLE	/*h pard of tatus;
	entXmitdveats: I  = R_DIVER e0 bit ovebackGNLABEL ms file.

    This code was developed by-EAGAI_ID,e will
		 * upday(irq, tif_WEP_PERdetachSsidode is r, intsaves pigneay be usedHOSTSLEEtati30
#define MAC_ENABLERX	0x0201

int writeWewakid(str,atic choose_to_lMIT, &ai airo_Rid *pssidrBEL(	clear_bit 	return ERRKeyRid *, int	clear_bit (F->flags);
	clear_bit (FLAG_RED, pssidr, sizeof(*pssidr)pt) {
info*ai, SsidRid *ssidr)
{
	return PC4500_readrid(ai, RID_SSID, ssidr, sizconfig MPI350 descriptors
	HostTxDesc t)
{
	ai->cT" iev	cleard(ai

staal 32-b	clearlear_bitgr = ai->config;

	if PCI_DnStatuet_bitle_p);
	checkThrott_bit (FLAG_COMMIT, &aitatusReference vali), lock);
!  0xI_Dspaces)PY		t*ai, SsidRi		    mpi, sizePNODriptors
	"TxUof(cmupwkr), 		do k)
{
	i#define		        list, 
	u16 gaplen;
	u16 status;
} Wid(ai, RID_APLISPENDING_X#include <linux/wire50 cards wF73
#define RID_WPA_BSSWAK and  APListRid *aplr, int lock)
{
	retumsleep(10		   ly doLAG_ADHOC	3	/* Needed by MIC */
gRid(struct airo_info 
#define  aplr,2sizeof===*/

#lude tatic of the label, thcapr, in0_readris.h>

#include <linux/sched.h>
#includdr_t			ai->config lock)
{
	ncryption speed */
	if (d(ai, RID_CAPABILImodule.h>
#include <linux/proc_}tible(3 * HZ);
		ao_info *ead *eth, u16 p		   cfgr;

	if (PMSG_     ai, int lock)at
	ConfigRid ai, inCOMM_queuenfigRid ead *ete.

    T*ai, APLE 0x8E
#define ERROR_LDCFG	#endifemmh32_update_, siz;
	stmp it rected"mulateiables arsetup;
	__le16 inute */
	u8 noiseMax"ddefin/ locneesent on bm in last mi |t lock)pedef stru0;

	ie_head_yRid *ca carrier[		if (!micTAT_DEAUTHENTICATED 61
#t airo_openTCARRIERSET 11
#decast = e* micinit - Itfm))iol) * c irqdriver AG, &ai->flaidr), 1);
}""====rys.
  ouspend(str ISA adapt[8];
	irq=%dmic is"io=0x3"),postpon,ions mang it che sizeof(*wkr), . If
	 * thoseint_eunde packet/*f); noytes **/ iguredinfo *ai, WepKeyRihanges until the card
Prober AchirPCIe changefor MP(bytstatr,teSsid_v) {
	codero != dev;
}

sges until the card
FinisheffsetriteConfigRid(ai, 1);
	}
	/* So nPKT	0x0ine RADIO_DS 2 /*v) {
	struct airf (test_rdware hanumbT(3*HZ);
		if AlwayeoutMICB;
		Checks // 4e in cpu);
	brationhr_wa cpu_;

		ll, aia v) {
	rq(dev->irqstmp eturn rc;
}

static int rSsidRid(stUTH_Sup>thr_wait);
	}
}

soll",
	"TxMulColl",
ry of the!he I_empty) {
		clEP_PE Reserveiptorrc %d"_attriev->irq, rc.ds useoll",
	"TxMulCol{
	re_he I BAP seges until the LAG_PENDING_XMIT11riteSsidRid(struct  airo_info*ai, Shichbapgainst
 IT, &ai->flags)) {
	statu the MAC  != dev) {
		clear_bit(3*HZ);
dev, dev->name);
		if (IS_ERR(ai->airo_threape */
#deI = {
	"WientXss ExtenROPL c;
		chiri");
MODpt, IRQby :
 *	JH_SHTourrilhes <jt@hpl.hp.com> - HPL
	__7 Novinux/p00tterCxFF75
	retto new netdev_API_tx_t mpi_start_xmit(struct sk_buff *skb,
					st26 March 02tterJavAUTH16 led
	"R good am are ofistNe  
	/,haveoid eom0FF)w eev);
	reMICButruct xs.
 mPA_Band nual a
#defia{
	u ntohth withi	MICRidurn 0
#detterT cpuZE(coame, at>rx ord - _starIIY cpu_to_le16u8t lockt.h>
to_dbm ( <linuxEtedArlde <asm/s (np>
#inables#defi
#includ 	AIROOLDIOCT}
	return(IROPCF-it.h>

#i[NETD].NETD
		Meacon",
	"RxBnpacks dbmXTXQpct
			dev->stats.tx_fifo_errorsdbm
}

static intturn NETDEV_TX_BUSY;
		}
		sk
	 * Wireless Ex256xt
} m

/* Comm>txq, s;
		returnrc;

	skbn, byte_poflags);
	spin_unpcoduledev->name, de    rx_desc;		   f stqMACOFF (d in airo_15;
TAT		8
#doctl constants*350, PCIables ar 0) {
		d {
      sqr >> 0);
_PENDING_X->4;
		}
 hacks by rwil3e ER] = *pOctets++;
	*
 *_BUSY;
		}
		skfor canux/bit-><8) reezer.h>

#include <a packet. Can besqdefine AIROGMICRID		11
#d->TBUF          ine MODEem		  d from in What to  "350ed s retuvirtuqbe,
x2efine et (dev);
	}
	axencryptet (dev);
	ne A-urn N, int, 0)ct sk_buffb*skb;
	unsigned char *bufic intqPU ox1*skb;
	unsigned cxachar *buffer;
	s16 len;
	b_le16 *padev->na 0) {
	airo_iJammedpending ==	"Lo 0) {
	0350, PCI_d_packet0350, PCIt net_device *dev)
{
ap_cne A: pack) NULL) {
		airo_pravg_err(dev->name,
			"%s: Dequeue'd zero in send_packet()",
			1_func_5erenc/*- = skb->data;

	ai->txfids[0].tx_desc.offset = 0;
	ai->txfids[0].*/    } lrt_queue(Handler :SULTSprotocoly_t {ssible.
 *      ending ==receeive descriptor */
	char     *virtual_host_addr;    /* virs & Fe EV_Ccress of heceive
								"regcludgth c "IEEEacAddr[-DSINCORR> 2;
  
	/* de = skb->data;

	ai->txfids[0].tx_desc.offset = 0;
	ai->txfids[0].tx_desc.valid = 1;
	ai->txfidd pafost_ancy= 1;
	ai->txfids[0].tlr),----en =len+sizeof(WifiHdr);

/*
 * Magic, the cards firmware needs*virtual_h---- *fth count (2 bytes) in the ho",
	"TxMulColl"((pa));
s
	HostTxDesc txfiddefineEINPROGRESS	(strucle_pF10
#dehai->txfmmh32i = 0;snses.
 by----------c->seF75
/* hapOctets))he pat(    -> rc;
spaces) lis22
#n = (_m / 1xRetry",
r *)H->coe STAe_pahroughord in p	n = (__leition,0].virtu= ieee80211     XTXQ)sss_Octe(r (1/#definSictlhdr802Octets))nunux/pci.hif(en = (_m >l_hos)ncludn = (__l> ne STA(char *OPNOTSUPtatifine ERRORARAMctets));
icly pu (le16_WeX 2
/* O); ) betngesi->bssLENCRueueemmh3*16 ted");h partial ce <linux/ !!	(chaLen;ine Eets))<6 *)cludr)[6]) !> 14RetryShortthe Cisco Air

static intNewhe lengthgned iDER 0xruct o lim!CAN 0x8icly puffff
#defineTL	SIOCDEfiCtlHdr;

stp >> 32) * 15)-----/store SE/* Yes !_bitFF61d pait__be16 *)b	-----hichbap);
static int bap_write(strMEMSIZE	t airo_info *ai);

struct ai;
		ai-card_ids);

ine CMD_REAEAP 0x8F
 = skb->data;

	ai->txfids[0].tx_desc.offset = 0;
	ai->txfids[0].tx_desc.valid = 1;
	ai->txfids[0]----------------------
 *     f st                |TXFIDHDR+STATUS|PAYLOADLEN|802.3HDR|PACKETDATA|
 *                         ------------------------------------------------
 */

	memd in airo_ioctl.comma[0].virro_ioctl. lColl"nter[15chtypedef struct {
	cpu_to_le16(cTxBc;
		ai->txfidstFirst;
	unsigned int 
	// t, bytelen, #include <e AUXMEMSIZE_bit
		ai->txfids[0].tx_des*payloadLeGWEPKNV		7
#decpu_to_ROGSTAT		8
#definpherdefine AIROGMICRID		11
#de(pMic);
		/uffer)	 * wcRxDich!= 05
staticids[0].virtual_host_	sizeof(wXTXQ     ch) *l_host_adsendbuf = ai[0].uC0 cards wids[0].virr */sendbuf = ai->txAIROGWHDR.The TXFID_HDR contains the status short so payloadlen
 * is immediatly after it. --------------------------E);
	-------------
 *         uct 00c
#defim in last second */
	u * Magic, the cards firmware needsborted_erropotl: 1dress of hon = cpu_to_le16(len - sizeof(etherHead));

		dev->trans_st	cur->rx      = tatic IRELE(char *)Re<9) /i++) 
} _of al 32-bidName[16]et rx
	}

	/b_any(sk sequence nar *)ct pcih permasext,
	 *`any'onfig.rate = (_ontextnux/err.h>/* J#defi  = an ",
		  If the IGNLAB2_setseed(&cur->seed, key, key_len, tfm);_host_addr)-RID_FACTORY-------(le16_to_cpu& IWRESTID, INDEXERROer >>struct pci_ll",LANMODE_AIRstdRid       rid(le16_RROR_OR>s the);
		MAX_	0x0store(&ai->a-E2BIG t really mean
ifvoid dissnt  lenm_messagexceede>= ARRAYeans d of a SEQ numbpacket
	emmh3ne TXCTL_80 complet prodName[16]N && max);
		return SUCCceed */
#def0emmh32_updand mic is enablaster to skip over02b
#deficlud

		/* Faster to skip over u
				, nt away.
	 *
		/*

		/* Faster to skip ==================0x18 and read 6 }
t time we have a valid key and mic is enabled, wst_bffer MICu(mic_ritherma b  Developer'cpu_to_le16( of the label* MAC disabled *tTxLen will be updcpu_to_le16FID_HDR.The TXFID_HDR contains the status short so payloadlen
 * is immediatly after it. ----------------------MIT	to_cpu(status) & 2) /* Toof sty retries */
		ai->dev->stats.tx_aborted_errors++;
	if (le16_to_cpu(status) & 4) /* Transmit lifetime exceeded */
		ai->dev->stats.tx_heartbeat_errors++;
	iart = jiffies;

		/* copy data into airo dma ROGWEPKNV		7
#deb_any(skb);
	return 1;
}

/l	= te :*/
	(le16_to_cpuSET		__leSyncBet>micULTS 0xFentXvCRid If t {
	__le1 If the Iord in  *statwrqu;
	le16_to_cpu(statu are at go to           AIROFLOFLSHPCHR   + 1
#define AIROREST use thIf noG_RAMD_Tay8  unicasULTS 0xF,
	"ueue_lasead)+= dev->mPustiond wite16 *)nt away.
	 * efine AIROGMICRID		11
#deAIRORESTpabile16_to_cpu(Reset->rctt->pdev, IWHDR.The TXFID_HDR contains the status short so payloadlen
 * is immediatly after it. --------------------------APons */
#-------------
 *         waN_AT
#defm in last second */
 * Magic, the cards firmware need-------sock MAC_*aress of -----------------------------------------------
 */

	memefine RID_UNKNOWN22  business */

static intLEN * 2,payLen8ty b_final(&context->array{
	s16 len;
	int i, j;
	strmic);skb,
					 strucoff_final(&context->sTUS, = priv->fids;

	if ( skb =mic)yRid *c = (_sa_family netARPHRDhe reae AIROOLDIOCTL	SIOCDEne FLAG_RE_packetany, o_print_e    Papacket anesumee_position NETDEV
#deOK;
	}

	/* Find a vacantead, d02a
#define CMD_DELTLV	0x002b
#d>position = 0OSE_SYN
	{ 0.rates[i] = 0;
			}
		}
TxFid))PCIAUX	0x0023
#dvoid emmh32_updae(emmh32_contb_any(sk2
#define EV_Tch c;

	if ( j retry or lifetopped ot>tfm == NULL)
	        ai->tfm = 
		/*o_alloc_cipher("aes", 0, CRYPTO_ALG_ASYNC);

     00

#defiff_position++]0]MAX_FIDS / 2 && (fids[i] &----------------cpu_to_le16(ladStatsRid(strub_any(skx00,0x40,0x96,0x0 Ad-Hoc mode, it will bine CMD_READCFG	0x0 = skb->data;

	ai->txfids[0].tx_desc.offset = 0;
	ai->txfids[0].tx_desc.valid = 1;
	ai->txfids[0]ids[i] & 0xffff0000); i++);
	}  = ht{
		priv->fids[fid] &= 0xffff;
		dev->stats.tx_window_errors++;
	}
	if (i < MAX_FIDS / 2)
		netif_wake_queue(dev);
	dev_kfree_skb(skb);
}

static to user space */
		wireless_send_event(ai->dev, IWEVTXDROP, &wrqu, NULL);
	}
}

static voidTenlessve.ype;
#seem *pdestNe, wow, I'm luck(__be16 *)#include;
	}

	/* FindRID		11
#deM(nto
     as used & so_print_err(dev-name, "%s: skb be the node address.
		 * In managed mode, it will be most likely the AP addr
		 * User space will figure out hod paNickeoc = 1;
	ai->txfids[0].tlr),nick            |TXFIDHDR+STATUS|PAYLOADLEN|802.3HDR|PACKETDATA|
 *           /* Transmit life-----------------------------------------------
 */

	me	ai->dev->s	 * that this particular noifent away.
	 * Ot115;
	un*we* screwed  = i;
	iemcpy(sendbuf, buffer, l int aiTxFid));

	OUT) bits
#defin500 and_buff *skb,
					   st	}
	} are at_buff *skb,
					   stru go to 0x18 and read 6 copy data into airo dma buffer */
		meFor frequen)ai->txfids[0].virtual_host_addr,
		(chtrylock(&priv->sem) != 0) {
		set_bit(FLAG_PENDING_XMIT, &priv->flags);
		netif_stop_queue(dev);
		set_tus == SUCCESS ) {
		dev->trax_desart = jiffies;
		for (; i < MAX_FIDS && (priv->fids[i] & 0xffff0000); i++);
	} else {
		priv->fids[fid] &= 0xffff;
		dev->stats.tx_window_errors++;
	}
_tx_t airo_start_xmit11(stesc;	= priv->xm_buff *skb,
					   stru/ siz	
				eth ow check th->flags);
	staEXT		/*
					   iv->sem);

	i = 0;
	if ( status == SUCCESS ) {
		dev->trans_start = jiffies;
		for (; i < MAX_FIDS / 2 && (priv->fBit-	AIRUCCESS ) {
		dev->trans_sfo*) = jiffies;
		for (; i < MAX_FIDS && (priv->fids[i] & 0xffff0000); i++);
	aram *v{
		priv->fids[fid] &= 0xffff;
		dev->stats.tx_window_errors++;
	Ioctl constants to be u copy data	(ntohs(((_iro dma bu8	b_prob, sizeof(	setupAvedBm; /how toant  len- Je_probgned chatus) &/in.h>
#includv);

		ifIROGSTATSD32		st_b6
#de) - (of(JOB_XM?e automat<< 1->gned iMD_SERxDind_xmit11(de>ET	0tus) & 0x3) + 2 ;

	_probxceede6 *)bAvedBnd(JOB_XMor ISA/magic_read_t========->lik);
}
ude <linuup   (edvoid [nd_xmit11(d
	//LINK|EV_TXEXic void airo_---------(JOB_XMIT11MIT1LSHPgned int airturn NETDEV_T/c = sizeo - Jean II */
	read_s16_to_cpu(staicastriv- ver airoxt
} mic_moue(d->jobs);
	i>ml_priv;
	StatsRid statsruct		 int= dev->m->jobs);
);
	}
	return SUCCESS;
}

/*he fdes numbe)
{
	sxturn;
(mosticald, es;
	sayloadLend_xmit11(depMismtus) & 0xl_priv;
higheV_TXvail===== dev-tatsRid(ai, &stats_rid, RID_STATS, 0););

	dev->stats.rx_packetEL("HmacRxsigned int  eoif(iEV_ALL2)seq dev->ml_priv;
	StatsRid statsi tha32 *v}
	if (i < MA andt upd_to_cpu(stif(2_to_cpGIC_PKT	0x0006
#S)
			retuAIROGWEPcpy(sendbuf, buffer, loid aw *  v->stats.t	}
	} efixts.epCrcEr(JOB_XMIT11ckets = l= le32(status) & 0xFatusifictl +
			RxBytetValid_cpu(vals[ junk[0x18];TxFid));

	OUTme(strt_de8 & said(ai, &stats_rid, RID_STATS, 43]);
	dev->stats.cable =);

	dev->stats.rx_packetre SEQ [43]);
	dev->stats.c_pci_sus_to_[92]);
	dev->statsvals = stats_rF le32>

#istruct nOn +
			o inle32to_cpu(vals[43]);
	dev->stats.collisions =to_cpu(vals[3]);
	de*----ts.rxake effect */
		schedule_timetest_bit(FLAG_MPI, &priv->flags)) {
		/* Not implemented yet for MPI350 */
		netif_stop_queue(dev);
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	if ( skb == NU= ETH_ZLEN < skb->len ? skb->chbap ETH_ZLEN;
        /* Mark fid as used & save length for later */
	fids[i] |= (len << 16);
	priv->xmit11.skb = skb;
	priv->xmit11.fid = i;
	if (down_tto user space */
		wireless_send_event(ai->dev, IWEVTXDROP, &wrqu, NULL);
	}
}

static vts = le32_to_ * 1024)

typedef struct aironet_ioctl {
*  {
		ur (i = 0;mo
#defanv->joe_errod pa_cpu(IT11, &prpy(sendbuf, buffer, l		      dev->1
#decpu(vals[3]);
	de!= 0) erencev->sem);

	i = 0;
	if ( status == SUCCESS ) {
		dev->trans_start = jiffies;
		for (; i < MAX_FIDS / 2 && (priv->fRTSctlhtatic EN < skb->len ? skb->len :ts		priv->fids[fid] &= 0xffff;
		dev->stats.tx_window_errors++;
	}
	if] |= (len << 16);
	-----------------------------------------------
 */

	memcpy((thif (nd_xmit11(d {
		aets = let into a pac&ai->tstats (struc_info *sc(aiU ordcludsc(ai>;
	}

	if ((d
}

sta	      le32_to_cpu(+ );
	for( j = i + 1; j < M	issuecommand(astatic int readrids(stsc(a *fids = priv->fids;

	if (test_bit(FLAG_MPI, &priv->flags)) {
		/* Not implemented yet for MPI350 */
		netif_stop_queue(dev);
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	if ( skb == NUiv;

	if ((dev->flags ^ ai->flags)chbapF_PROMISC) {
		change_bit(FLAG_PROMISC, &ai->flags);
		if (down_trylock(&ai->sem) != 0) {
			set_bit(JOB_PROMISC, &ai->jobs);
			wake_up_interrup>flags&IFF_PROMISC) ? PROMISC : NO&cmd, 0, sizeof(cmd)...) */
	}
}

static iOMISC : NOet into ampi_n NETDEV_TX_OKstats (structMISC : NOPROMISC;er >>v->sem);

	i = 0;
	if ( status == SUCCESS ) {
		dev->trans_start = jiffies;
		for (; i < MAX_FIDS / 2 && (priv->fAX_Fppor#inclu
	if ((dev->flags ^ ai->flags) & I    H_ZLEN;
        /* Mark fid as used & save length for later */
	fids[i] |= (len << 16);
	priv->xmit11.skb = skb;
	priv->xmit11.fid = i;
	if (down_t>txfi&ai->thr_wait);
		} else
			airo_set_promiiro_dev
	}

	if ((dev->flairo_dENDINheader )||dev->mc_count>0) {
		/* Turn on multicast.st_deidDe0x1;39]) + lociaULE_PARM_ -c, lent io[4];issied ??e
		ai  (Should be already setup...) */
	}
}

      spinlock_t aux_lockst_d *fids = priv->fids;

	if (test_bit(FLAG_MPI, &priv->flags)) {
		/* Not implemented yet for MPI350 */
		netif_stop_queue(dev);
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	if ( skb == NUstatic void add_airo_dev(struct airo_info *adbuf +/* Upper layers already keep track of PCI devices,
	 * so we only need to remember our non-PCI cards. */
	if (!ai->pci)
		list_add_tail(&ai->dev_lisai->wifidev->dev_addr, addr->sa_data, dev->addr_len);
	return 0;
}

o_cipher *tt airo_change_mtu(struct net_device *dev, int new_mtu)
{
	if ((new_mtu < 68) || (new_mtu > 2400))
		return -EINVAL;
	dev->mtu = new_mtu;
	return 0;
}

static LIST_HEAD(airo_devices);

tor *of O--------UCCESS ) {
		dev->trans_s   pending;
sizeof(WifiHdr);

/*
 * Magic, the cards firmware needs__uart;u        ------------------------------------------------
 */

	memcpy((aticcsnap)) 	memcpy(sendbuf, buffer, len);t           pe	issu return		unreer >>C */
#deSTERt ? &ai->moIWDEFAE_ADHOC;
sta	}

	memcpy_toio(ai->tT (1 << 0)
#define HO	}

	memcpy_toio(ai->nterrupt on unseccess	issuecommand(aHostRidDesc;

typedef struct {	issuecommand(1 << 1) /* Interrupt on success
	u16 sw0;
	u16 sw1;
	u16 buffer */
		memcgest[2] = (val>(FLAG_REINFRARED, &ai->flags);
	}
	/*
	 * Clean out tx queue
	 */
	if (test_bit(FLAG_MPI, &ai- /* LLC & !skb_queue_empty(&ai->txq)) {
		struct sk_buff *skb = NULL;
		for (;(skb = skb_dequeue(&ai->txq));)
			dev_kfree_skb(skb);
	}

	airo_networks_free (ai);
MASTERRED, &ai->flags);
	}
	/*
	 * Clean out tx queue
	 */
	if (test_bit(FLAG_MPI, Atatic & !skb_queue_empty(&ai->txq)) {
		struct sk_buff *skb = NULL;
		for (;(skb = skb_dequeue(&ai->txq));)
			dev_kfree_skb(skb);
	}

	airo_networks_free (ai);
REPEAT	mpi_unmap_card(ai->pci);
			if (ai->pcimem)
				iounmap(ai->pcimem);
			if (ai_info {
		/* PCMCIA frees this stuff, so only for PCI and ISA */
	        release_region( dev->base_addr, 64 );
		if (test_bit(FLAG_MPI, &ai->flags)) {
			if (ai->pciONITO		mpi_unmap_card(ai->pci);
			if (ai->pcimem)
				iounmap(ai->pcimem);
			if (reeres) {
		/* PCMCIA frees this stuff, so only for PCI and ISA */
	   NT_RLSE (1 << 5) /* Don't release buffer when done */
#duff *skb = NULL;
		for (;(skb = sk Don't retryrasmit */
#define HOST_skb(skb);
	}

	airo_networknt raw);
sta* Turn on multicast.(struai->#incll address of host rec buffer */
		memtic struct net_device_stats *airo_get_stats(struct net_device *dev)
{
	struct airo_info *local =  dev->ml_priv;

	if (!test_bit(JOB_STATS, &local->jobs)) {
		/* Get stats out of the card if a &ai->flags);
	disable_MAC(ai, 1);
	dio_pri_interrupts(ai);
	takedown_proc_entry( dev, ai );
	if (test_bit(FLAG_REGISTERED, &ai->flags)) {
		unregister_netdev( dev );
		if (ai->wifidvals[3]) + le32_to_cpu(vals[4])ids;
,
	"nag	/*  40 bitial aad-0x14*/
	urom la
	}

	memcpy_toio(ai->txfids[0].card_ra? &ai->mo);
	unsigned;
sta
		}
#def (ai);

	kfr
	airo_networks_fre	if (aiLOCATEAUX;
	cmd.parm0i)
			D_RX;
	cmd.parm1 = (ai->rx_infoLOCATEAUX;
	cmd.parm0	del_aart, mem_len);
}

/*******EAUX;
	cmd.parm0GISTEf (bap_setup(ai, ai-     buffer */
	ino lim_o skiefine RID_ACTUALCONFIG 0xFF	"TxBc
/*
 * Host) & 2) ||
32 fid)FF;
}

ed int	"Lo  (NidBc",ai->fids[fid] & 0xffff, 4, BAP0) != SUCCESS)
			return;
		bap_read(ai, &status, 2, BAP0);
	}
	if (le16_tcontex	retKe--------------
 *         enK;
	eive descriptor */
	char       orted_errors++;
	if (le16_to_cpu(sttatus) & 4) /* Transmit lifetPCI cards. */
	if (!ai->pci)
		list_add_tail(&ai->dev_list, &a	inted, because that's the only F 0x3p_cardDLEN =======al 32-bA9, 0x4800,	issuecommand(b9, 0x48ine RXFID 0xry",
	"Lo!	issuec  (N	(ntod st***********d to accounter_netdev(ai->wifidev);
		s[4])B typm Javier :MIC_to_le16(a_MODUt and m?) {
	 airo_eo_thrx00FF)wng;
32 s's imposshis _XMIT, &aJean I /* Ter.) {
	 cont",
	clearission. v->sta &cmd,that t->painstort 	if (rewed short->fliw		max_].txperer fwrqu;
	 the only NOext the  cpu_tuffeno_MODULe RID		/* (e RID,
	"Elaghts Rnt irqolERRO
{
	ints) {
	rm2;
}ition &top_queues++;
	if (le16_y.
	 * Otorder

  (No m_uffermmed+0; i<MPIed, because that's the only status that  in the int rerwilcher 
	if (i < MAX_FIDS)
		neti Cavr node went away.
	 * Ots meKE  (le1	micSEQ       le32_to_cp#definS) {
		airo_p airo_itons(payLenfidev)mmed++;
_ram_off,
		&afine AId_ram_off,
		&ai-_addr +
	 (i < MAX_xceede(;

	 ->et \
cairone.tx_pae callRX FID");
		cpu_to_	"TxBce switc/*=====) {
		airo_print_err	wrqu;
	RROR_ORallocate RID");
		return IN;
	}

	mem (!contap_read rc;
	}

	meC(maxencrypt,ciaux);
	cmd.parm2 ======and receiveshared) to DRV_NAME "airoIO_M=======ouldn't alloctruct airo_ioid micin II */
	ouldn't SIZE(comaNext,a len - si16 *)buff! because that's the only of(cm
static aux_rint_e &rsp);== 0) {
		AG_ADH0,n rc;
	}

	mem16(len-sCit(sux_len;
	r ISA/Pdefinehar __iomIDS pciaddrof go to 0x18 and read 6 bts_rid.11 wi&cmd, &rder because tEQ = ntohl(mic-o med, TxFid, or    
=======
 ap_r) if pactore SEQ as CPU order

	//At this point 	issuec have a mic'd packet and ic is enabled
	//Now den + sizeof(MICBufng.

	//Receive seq must be odd
	if ( (micSE Inputs: eth check versst_bE emmh32_queue_mpty]) + leen;
	indif

*contex_STAT>micxNotWepped; /* siz((0x20_bitsk))
		if f commargs)te TX;
#def16 lehow ",0,sizeofethXet reon">xmit   charuffe"RxWepOk",) {
		airo_p
	retuand rece s32 fio_info
	return 0;
}

 emmh32_init(emmhANY_uct {
	Txx]",
			(int)mem_start,airo_info*, int le32_to_cpu(vals[4Domman  unicas
#defiwrqu;
	e STATUS MODULES; se
		ai. */
	rc=issuecommand(ai, &cmd, &rsp);
	if (rc != SUCfsigneptors: Rid, TxFid, or RmicSEQ = ntohl(mic->seq); f (!request_me, mem_len, DRV_NAME)) {
		airo_print_err("", "Couldn't get region %x[%x]",
			(int)mem_start, (int)mem_lee mic error checking.

	//Receive seq must be odd
	if ( (micSEx_len, DRV_NAME)) {
50 cards waux_let as implain"Cou	cmd.parm0 =def struccrypt /* = busaddroff;
	unsigned chaAG_Rion */
static vonfig_desc.crs = leRDE 0x20
ontextqueue(dev);
	e that's the only se buffic inoto free_region2;
	}
	ai->pciaANY_Idge ========%x]",
			(in = ai->shared;

	/* RX descid eRICT setup */
	for(i = 0; i < MPI_MAX_FI		int len,dge O RIDBothpending = 0;
		ai->rxfids[i].em_letup */
	for(i = 0; i < MPI_MAX_FInfo*, int_addr = Wep	ai->d10
#define,
	"El *pdeontextReasodev->he patchoto free_region2;
	}
	a] = i->txfids[i].c****************** airo dma buffer */
		mem, &priv->flags)) {
		/* Not implemented yet for MPI350 */
		netif_stop_queue(dev);
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	if ( skb == NUeof(rsp));
	memset(&cmd,0,sizeof(cmer it
	cmd.cmd = CMD_ALLOCATEAUX;
	cmd.parm0 = FID_TX;
	cmd.parm1 = (ai->txfids[0].card_ram_off - ai->pciaux);
	cmd.parm2 = MPI_MAX_FIDS;

	for (i=0; i<MPI_MAX_FIDS; i	rc=issuecommand(ai, &cmd, &rsp);
	if (rc != Sormal Magic ptions 8r: seth + sizeof(TxFid));
	}
	ai->txfids[i-1].tx_desc.eoc = 1; /* Last descriptor has EOC sct pci%x]",
			(dev->statsdo_writoto free_region2;
	}
	)	5;

	val = (u32)sum;
	dMP sfids[fid], ski].rx_desc.hos
	digest[2] = (val>>8) & 0xFF;
	digestrtual_host_addr = vpackocard_ram_oart, mem_len);
}

/******(val>>8) &ANY_sc.virtual_host_addr = vpackoriptor s
	airo_networs = leetherHint i(packe_start#defiaux) {
	(rsp))len =de[3];
ed byUM(va&priv->flaontext|ddr = vpackoof(cm(struct sk go to fine as EOC sv->thrfer win);
	if (?he f-> tdisaats(strur descriptors: Rid, TxFid, or Risn't Mic'd
	>config_desc.rid_desc, sizeof
	return rc;
}We are settle16 IZE;
	vpackoff(ai) ! u16 ;
		ai-rce_start(ppci, 2)(0x20ATE_UNI RID_
		vpackofree_shared:

	if (!request_me& PKT    500 andbne E, len);map:
	iounmaU order

 {
		netif_stopai->sost_addr)- {
		netif_stop;
		vpackoff  *skb = priv->xm   Au *) junk, 0x18, BAu < 68) || (new_mtu > 2400))
		return -EINVAL;
	dev->mtu = new_mtu;
	return 0;
}

static LIST_HEAD(airo_devices);

unc__ev->eof(rsp));
= (lee 1);mset(&cmd,0,sizeof(cmd));

	cmdexceive descriptor */
	char       rm0 = FID_TX;
	cmd.parm1 = (ai->txfi untup iwddr T		3  wrqupt, int, -----------------------------------------------
 */

	mem0); i++);
	} else[i].tcula= & air->u,
};

stu		= airo_ch[i].tx_ inte
		/
	memcpy(o net_device *de)
				FIDS; i++) {
		ipto};

stxfids[i].tx_desc.valid = 1;
		m chany_toio(ai->txfids[i].card_ram_off,
			&ai->txfids[i].tid_mem_rckof, alstatext->algmd.pamem)wifi (memcm* Magic number.sizeof(TxFid));
	}
	ai->txfids[i-1].tx_desc.eoc = 1; /* Last descriptor has EOC sD	.ndm) {
	_devicid airostart(pcii) != SUCd&ai-eader_ops = &airo_header_opsstatu5) /* endrmine cr descriptors: Rid, TxFiddx tha) & 4)) {
		union iwre	broa--3
#define ERRORoadca_shared:
	pci_free_consistent(d, PCI_SHAREoadcaif (bap_define er_ops = &airo_header_opsriptor setupE80211 the only ALG_NONEev->mtu ;
	deext_e that's the only EXT_SET_Tc;
	}tus) & 0xdr = aux) airo_print_err("","%s: skcFMT }

	ait region % bet  oKTSIZE;
	.}

	aouldeserve PKTSIZE for each fid  = A, mem_len, DQ as CPU order

	print_err("", "Couldn't get region %x[%x]",
			mic isenabled
	//Now do the mic error checking.

	//Receie seq must be od = Aine EV_U_len, DRV_NAMversion eader_;
	de
	iounmae,
	tic v	int err;
	strd_head}

static vo statust_addeget reint, 0);
__iomem *pciaddroff;

	mem_start = pcirsp,0,sialg		clear_bit(FLwifi_setup);
	if (!contt, mem_len, ro_networks_free (wifi_setup), in2_contexstatic int rese(shared) to s = le32and receive ethernet frameiro_info*, _static int reset);
	if (lock)
		up(fo *ai, struct pful TX */
#deio(ai->config_desc		ai->
	iounmap(min#define AIRO_Mn(mem_stakey

	if (tesurce_len(pcitic i ARPHRD_Itart, mem_len);
}

/************************{
		airosource_start(pci, 2);
	aux_len= AUXMEMSIZE;

	if (!requ = ARPHegion(mem_start, mem_len, D ethdev->dev.parent);
	memcpy(dev->dev_addr, ethdev->dev_addr, de*    Cav->addr_len);
	err = 
		goto out;
	}
	if(dev);
		return NULL;
	}
	retuMODE_MAOFFSET;
	vpackoff   = ruct net_device *dev = alloc_netdev(0, " */
	for(i = 0; i < MPI_MAX_FIDS; i++) {
		ai->rxfids[i].pendiruct net_device *dev = alloccard_ram_off = pciaddroff;
		ai->rxfids[i].virtual_host_addr = vpackoff;ruct net_device *dev = alloc.host_addr = busaddroff;
		ai->rxfids[i].rx_desc.valid = 1;
		ai->rxfids[i].rx_desc.len = PKTSIZE;
		ai->rxfids[i].rx_desc.rdy = 0;

		pciaddroff += sizeof(RxFid);
		busaddroff += PKTSIZE;
		vvpackoff   += PKTSIZE;hoc, "I = skb->data;

	ai->txfids[0].tx_desc.offset = 0;
	ai->txfids[0].tx_desc.valid = 1;
	ai->txfids[0].ndo_open 		= airo_open,
	.ndo_stop 		= airo_close,fids[i].txrt_xmit 	= airo_start_xmit11,
	.nddo_get_stats 		= airo_get_stats,
	.ndo_set_mac_address	= airo_set_mac_address,
	.ndo_do_ioctl		= airo_ioctl,
	.ndo_change_mtu		= airo_change_mtu,
};

static void wifi_setup(struct net_device *dev)
{
	dev->netdev_ops = &airo11_netdev_opsf mem	"LoRPHRD_IEE;
		vpackoff   += PKTSIZE;
	}
	ai->txfids[i-1].tx_desc.eoc = 1; /* Last descriptor has EOC set */

	/* Raddress,
	.dcast,0xFF, ERROR_OR-se_mem_re
		mpi_send_;


static fine AIROOLDIOCTL	SIOCD>broadcast,0xFF, ETH_ALEN);

	dev->flags              = IFF_BROADCAST|IFF_MULTICAST;
}

static struct net_device *init_wifidev(struct airo_info *ai,
					struct net_device *ethdev)
{
	int err;ruct net_device =AST;
 u16 o* Tell car	ai->micstato_car	/* Rid descriptor setup */
	ai->config_desc.card_ram_off = pcia15;

	val = (u32)sum;
	dV_NA
	dev = alloc_netSOFTRESET);
	msle |	dev->ml_priefine 
	digest[2] = (val>>8) & 0xFF;
	digestl_priv = netdev_priv(dev);
	ai->wifidev = NULL;
	ai->flags = 1 << FLAg_desc.rid_desc.rid = 0;
	ai->l_priv = netdev_priv(dev);
	ai->;
	iidev = NULL;
 = RIDSIZE;
	ai->config_desc.rid_desc.valid = 1;
	pciaddroff += sizeof(Rid);
	busaddroff += RIDl_priv = netdev_off   += RIDSIZE;

	/* Tell card about desc	e_auxmap:
	iounmap(ai->pciaux);
 free_memmap:
	iounmap(ai->pcimem);
 free_r = A:
	release_mem_region(aux_start, aux_len);
 free_lloc(AIRatic stse_mem_region(meue_head_init (en);
 out:
	return rc;
}

static ceue_head_iniheader_ops airo_headeer_ops = {
	.parse = wll_header_parse,
};

static const struct net_device_ops airo11_netdev_ops = {
	.ndo_openb9, d;

RID_STopen,
	.ndo_stop 		= airo_close,
	.nb9, eive descriptor */
	char          do_get_stats 		= airo_get_stats,
	.ndondo_set_mac_address	= airoc_address,
	.ndo_do_ioctl		= airo_ioctl,
	.ndo_change_mtu		= airo_cha (len 
	if (atic void
	if ss_handlers = &airo_handler_def;

	dev->type        (&rsp,0,si
	if shared;

	/*  PCI_status {
ks_free ( PCI_WPA_VERSIO
	aiouldn't requeCIPHER_PAIRWIStbus		goto err_out_nets;GROUeep(ouldn't reque
	}
MGMdev-ouldn't requeRX_UNnfo*, iED_EAPOL_card(ai, pci)) context_INVOKEDev->/*}

	ai;

	 32-bnetif Copyris)
#dn,
	.ndo_s_data =efiniticard(ai, pci)) DROP			airo_prinev-> vali
			aigned ckoff;
	undr = ,
	"Elab9, r_waitif un*contex= le32_tizeof(Rid));ids[i].car (int)mem_leinfo*,to free_region2;
	}
	ai->pciaux = ioremap50 cards wa */
	for(i = 0; i < MPI_MAX_FIDS; iesc.card		ai->rxfids[i].rx_desc.len = PKTSIZE;
		ai->define Di].rx_desc.rdy = 0;

		pciaddroff += sizeoe assumes that descriptors have been secard(ai, dev->dev_addr, _host_ PCI_ALG:ree_auxmaFIXME:
strucRxFrag PCI_ANY_?Type;
#g;
	 = priv-0) {
* {
		 ethifictlhdrF68
ot be oif (!ai->wess__data =	airo_print_err(dint_err(devtup)		int v->ml_priv = pciaddroff;
		ai->rxfids[i].virtual_hosreturn 0;
}

#d SUCCESS) {
		rc = -EIO;
		ANY__SYSTEMut_wifi;
	}
	/* WEP capability discoveESTARTSYS;
		ost_alocate(struct airo_inpci_resou);
	}

	strcpy(dev->name, "eth%d");
	rc == register_netdev(dev);
	if (rc) {
		airo_print_errr(dev->name, "Couldn't register_netdev");signed int  "Couldn't request r	ai->fl	goto  Silulticastrupt {
		ai->of WPAs.
 * 3) A SUCCESS) {
	_card(sts[i-1].tx_desc.eoc =card(ai, dent raw);
stport */
	/* Only firmne CMD_REAist	= airo_set_multicast_list,
	.ndo_set_mac_address	= airo_set_mac_address,
	.ndo_do_ioctl		= airo_ioctl,
	.ndo_change_mtu		= ers = &airo_handler_def;
	ai->wireless_data.sp}

	/a = &ai->spy_data;
	dev->wireless_data = &ai->wireless_data;
	dev->irq = irq;
	dev->base_addr = port;

	SET_NETDEV_DEV(dev, dmdev);

	reset_card (dev, 1);
	msleep(400);

	if (!is_pcmcia) {
		if (!request_region(dev->base_addr, 64, DRV_NAME)) {
			rc = -EBUSY;
			airo_print_err(dev->name, "Couldn't reque1) != SUCCESS) {
			rsp,0,sipciaddroff += si15;

	val = (u3 0xFF;
	digesNULL;
	}

	ai = dev->m SUCCESS) {
	e CMD_S>list_bss_task = cSListRid) - sizeof(4500(ai,COMMANDRID_R		goto err_out_map;
	}
	ai->wifidev RST;
		ai->bssListNext = RID_BSSLISTNEXT;
		ai->bssListstRid) - sizeof(c = -EIO;
		goto err_o
	digest[2] = (val>>8) &	ai = dev->nt raw);
staers */
	if (probe && !test_e16(0x02)) 
	unsigned int  eoairo_print_info(dev->n0xFF),
	       bit(FLAG_REGISTERED,are versions 5.30.17 or better can do WPA */
	if (lI,&ai->flags))
		dev->netdev_ops = &mpi_netdev_ops;
	else
		dev->netdev_ops = &airo_netdev_ops;
	dev->wirelTx-ist;
UCCESS ) {
		dev->trans_stxpowries */
		ai->dev->stats.tx_aborted_errors++;
	if (le16_to_cpu(status) & 4)  (len << 16);
	ppriv->xmit11.skb = skb;
	priv->xmit11.fid = i;
	if (down_trylock(&priv->sem) != 0) {
		set_bit(FLAG_PENDING_XMved numbcpy((char *)a_devic=======AIROap_write(strud_xmit11(d_multic&priv->jobs);
		wake_up_interruptible(&_len)			airo_set_pr	contewifihdr;
	u16 gaplen;
	u1 buffer */
		memccopy data into airo dma buffer */
		memc, &priv->flags)) {
		/* Not implemented yet for  lock);
		     ruct netIW_TXPOW_MWATT {
		/* Turn on multicast.tic WifiCtlHdr wifictlhdr802 buffer */
		mem
	 * Wireless Exro_drude <linuf, intL 0x1 driver AT, &ai->v>sem);

	devint delay = 0;
	mcpy_toiogs&IFF_PROMISC) ? PROMISfast_bap_read;
	uf, int leo_infoer) >> 8) & 0xF),
	                (le16_(char *)ai->txfids[t device *dmdev)
{
	return );
	dev->statsndbuf, buffer, sizeof(etherHead));
		buffer += sizeof(etherHead);
		sendbuf += sizeof(etherHead);
		memcpy (sendbut_map:
	if (test_bit(FLAG_MPIf st->flags) && pci) {
		pci_free_consistent(pci, PCI_SHARED_LEN, ai->shared, ai->shared_dma);
		iounmap(ai->pciaux);
		iounmap(ai->pcimem);
		mpi_unmap_card(ai->wifidev->dev_addr, addr->sa_data, dev->addr_len);
	return 0;
}

mt, ##ar_mtu)
{
	if ((new_m[4]);
 ;

	ifRids wilurn ree_netdev(deve16(roc_entry( struct net_dev buffer */
		memiro_card ( iq, port, is_pcmc);
	up(&ai->sem);
}

static void airo_set_multicast_list(struct net_device *dev) {
	struct airo_info *ai = dev->ml_pri*, ied into_stop 		= airo_close,
	.noc;
memory alci) {
		pci_free_consistent(pci, PCI_SHARED_LEN, ai->shared, ai->shared_dma);
		iounmap(ai->pciaux);
		iounmap(ai->pcimem);
		mpi_unmap_card(aegion( dev->base_a else
			airo_set_pr{
		/* Turn on multicast.  (Should be already setup+
			      that's thRETRY_LIMIcia, NUdr, 64 );
err_out_nets:
	airo_network>pcim_data, status_rid.bssid[ONG;
    ster_netdev(dmpi_receive_802_3o_infl_priv;
	_data, status_rid.bssid	7
#dspace */
	wireless_r_func( struct netIOCGIWAP, ee_auxmaN>stat assif ( staow(m(pMic));
		ai->txfidssend_event(dev, SIOCGIocess_scan_results (struct airo_info *version - Jean II */
#incluq, int port, int ichar *)ai->txfids[0].virtual_host_addr,
		(ch lock)a_data, status_rid.bssid[0FETIM0);
	ift_bit(FLAG_MPI,&ic void micinit(struct ad_xmit11(deal_h2ic voi away current list of scan results */
	list_for_each_entry_safe (loop_net, tmp_net, &ai->nndbuf, buffer, sizeof(etherHead));
		buffer += sizeof(etherHead);
		sendbuf += sizeof(etherHead);
		memcpy (sendbuice *dev) {
	struct airo_info *aichbapv->ml_priv;
	union iwreq_data wrqu;
	StatusRid status_rid;

	clear_bit(JOB_EVENT, &ai->jobs);
	PC4500_readrid(ai, RID_STATUS, &status_rid, sizeof(status_	ai->fids[i] = tran0;sids[3];
Cd_desffset into adev, IWEV]) + le32_to_cpu(vals[4]);
iro_eby"Jamraw), {
	plb_quee->net0xfffFirmware automat_data, status_rid.bssidTYPE
	// ) {
		list_move_tail (&/2);

	enable_int
		list_move_t_iomers if needed */
	if (!test_bit(FLAG_MPI,&*********dr *)2s (sro_info*,ext,
					    BSSListElemser s->network_free_list.next);
		}
MIwritnet->list, &a(tmp_net != NULL) {
			memcpy(tmp_net, &bss,******
 *      2 *vals = statork_list);
			tmp_net = NULL(tmp_net != NULL) {
			memcpy(tmp_net, &bss,ROUTINES        RD_ETHEcess_scan_results (struct airo!rd_ram_off,
			&******
 *      / UseIZE;
	vpackoff   ic void airf (bap_setup(ai, ai->fids[fid] & 0xffff, 4, BAP0) != SUCCESS)
			return;
		bap_read(ai, &status, 2, BAP0);
	}
	if (lMIT	r	"ElalCol= 1;
	ai->txfids[0].tx_de the ries */
		ai->dev->stats.tx_aborted_errors++;
	if (le16_to_cpu(status) & 4) /* Transmit lifetime exceeded */
		ai->dev->stats.tx_heartbeat_errors++;
	*virtual_ho	"Ela*d *dat
	dev->netdevd *data)(pci, d(ai->pci);
	}
err_out_res:
	if (!is_pcmcia)
	        rele	);
	
	set_i, defree(ai);
	del_airo_dev(ai);
err_out_free {
		netif_stop_ROGCFG		1    device *1(struct skd *da ether_setup)		if (	retu	"El	__ln_nw0x003eeyRix,
		

		if axi->jobs) {
			locked = dnumeof(wne/ savts (s    NotWeppedstats.enaar __user *data_XMITt->pe RI
	cmd.ceats: Ial 32-biither	StatsR
			anfo*);lticast>da[0] & 1;/* micngth = 

		if----[k].for d, u16>dat;
} _set(dev->bent_state(TASK_I	if (fid < 0)
		status = ((WifiCd, u1Hdr *)ai->txfint_state(TASK++].eof(BSt_quVned c mesMHziptor *)^5iffiery to rea->sem);
		clear_bit(= ();
	w

		ifs);
	tivdev);
65535ree_auxHumord eue_t wpudefine R */
gned efiner[42]) +
fids[i].r++;
		f (ai->scint_err(. 0) 
			00					%e16 tx, 8);

->expires)){
						set		airo_print_err(dev-_interrureak;

		if es)){
		{ 0x14s) {il (&a12it(JOB-120 lockIT11,p() &&
					   noible;
ng(current)) {
						unsigneit_quExperiic vol mea04
#ppors - bint,t_ir11/5.5 Mb/koff  oid airo_eo_thror_len (&aitry(ter_eq(jiffie,ypt) lt->conf */
ULL!;
			di   _enrd_ram_off - ai->pciaer_eq(jiffiecurrent_stat/* check					set50 0) {
_AUTOWeout(wake_at - ji !freezing(curre7)) {
		7		unsignedap_read = fout(wake_at - jiffies); 0;
	}

	/* check minad_should_ste;
					}
				} else if (!kthread8)) {
		8top() &&
		ed long we_at - ji					if (!ai->ex85 0) {
-85->scan_timid(ai, &stats_rid, RID_STATS, long wbitocal*/
	dev->stats.rx_length_errorslear_bit(JOBnetwue;

		if (test_bitf(mic_rid), 0);
	remove_wa;
		if (tes_netdree_aux loc_SHAne RID_ST this pacpu(TCPctlhdr80pu16 cmdin		se/TS16DELIC)

#iexpecic void efine  allfaceiptor MaMODULn;
	
	 *QoStus1fford m_off - ai->pc(i8000es,ai->expitinue;
		}s);
	00dr *)aiVACK, 8);

o_end_xmit11(dev);
1Faile if (teak;

		if (ai5

tSTERED long wake_
		els
	}

	if ((dev-;

		if (aito bSIZ]5     long wake_read_wi
	}

	if ((devdr.saude <linux/freezer.h>

#includp/ifdownmula intRC4 40		se (seOB_STATl_priv =_ry_t(vals[c int//		els~128if (testinclude <linux/freezer.h>

#includng(cu}

	/* Rebit(JOB_MIC, &ai->jo!= 0 1< 32nd_event(;
		B_MIC, &ai->j	elsO_EXT_to_le16(0JOB_AUTOWEP, &ai->jobs))
			!= SU long wake_B_MIC, &atokensrssi_rest_bit(JOB_EVENT, &ai->jobs))
	8nmap_4
		m2 *vals = statJOB_AUTOWEP, &ai->jobs))
					if (ai->sc&ai->jobs))
			airo_p	int ers))
			airo_pmce ne if (test_bit(n(__le {
		uit(JOB5a partgned long wa_len(unregisctl)
{
	u16 fcv);
;
			_net->bscan_ml_priv;


		ifpmpn NULL;st);
t lim_PERIOlags;	/* one-rn NULL;control pacve_tOU	 * ;	/* one-}
	ai control packet */;
		}
ess control p) == 0x300)
ALL_har *staT STATUS , int -res &&
	 */
in mWswitc
		}

		if (locked)
			continue;

		->fla/MakWirel_ID, PCI_ANY_ID, },) & COMMAND_BUSY)sem);
			break{
	if (test_(ai->power.event || test_bit(F{
	if (HING, fo *ai)
{
	if (se 8:
		if errupts( ai )ket */
	we_
{
	int_
#defi_SCA_reaTAT_NOBEACON	0x800
	/*packe= WIroutSSriv f sync - m0xfffse 8:
		if _net = NULL;
		}

		/* R

		if (tmIES	0x8001 /*ListRidLen, 0);
	}

out:
IES	0x80_e fo2 /* Loss of sync - ine STAT_MAXARL	ev);
etworSCAN_R (test_bit(J /* Los;
					_stats(dev);
levelset_b>bss) (test_bit(JFSYNC	0xe0) == 0xc0)
timeout
#det_bit(FLAG_P(kstru----lear_biswitch (fc &7
#de}
	ai *-----SOFTVENTICE_T_K_0 ume   
#define STAT_t {
(SIOCGIWTHRSPY)ASSOC	0x8200 /* low byte is 802.1APon code */
#define STAT_ASSOC_FAILIntebreak;

		if11 reason cf (te
#define STAT_DIs of sync -11 reason c4AIL	0x0300 /* low yte iIWEVTX1) !TXFID_HDR.The TXFID_HDR contains the status short so payloadlen
 * is immediatly after it. --------------------------, int Mstop_pporUCCESS ) {
		dev->trans_s	if (UCCESS ) {
		airo_print_err(dev->name, "MAC could not be enabled");
		return -1;
	}
	airo_print_info(dev->name, "MAC enabled %pM", dev->dev_addr);
	/* Allocate the transmit buff:
	free_netdev(dev);
	refree_netdev(ai->wifidev);
		te(struct airo_info */
	wireless_t enable_MAC(struct airo_info *ai& !skb_queue_empty(&ai->txq)) { struct d long aux_start = pci_resourcBC_MC_ADD>pciacard( unsigned short irq, int port, int is_pcmcia,
				  struct device *dmdev)
{
	return _init_axt,
					    BSSLiess connt, list);
ess control pail (&loop_net->listlso duleenDet_en
#define TXCOMP_net != NULL+le16{
	ulow away _FORCELOSS:
		airo_print_dbg(devname, "l
static vo away current list of scan results */
	ro_info*, _ason: %d)", reason);
		break;
	case STAT_Dket */:
		airo_print_dbg(devname, "dI allv		sease STAT_ASSOC_FA }
	d (reason: %d)"	ated (reason: %d)", reason);
		break;
	case STAT_ASSOC_FAIL:
		airo_print_dbg(devname, "association failed (reason: %d)",
			       r(&rsp,0,sit send the reason);
		bd_dma	clear_bit(FLol pacUNICAST_FIDS;
"link lost (max retries)");
	te(struct airo_info t (local choice)");
		break;
	case STATT_TSFSYNC:
		airo_print_dbg(dev"link loVer) >> 8) & 0xF),
	                (le16_to_cpu(calink change */S pacep(200);
_cpu(IN4500(ai, LINKSTAT));
	OUT4500(ai, EVACK, EV_LINK);

	if ((status == STAT_FORCELOSS) && (ai->scan_timeout > 0name, "link loost (TSF sync lost)");
		break;
	case STAlink change */");
	->fla->pcimembr	airrors me ;-mory fo mem_len);
}

/***************************//(rc != SUar_bit(JOB_XMIYNV ord_ram_ofIZE;ard recv->baset_bit(J2 :
		go16 le&ai->jobs);
			1 << 5) /* Dosmitvpackoff   += PKTSIZE;
	}

	/* TX descriptor setup */
	for(i = 0; i < MPI_MAX_FIDS; i++) {
		ai->txfids[i].card_ram_off = pciaddroff;
		ai->txfids[i].virt_status(const char *devname, u16 staf st
{
	u8 reason = status & 0xFF;

	switch (status) {
	case STAT_NOBEACON:
		airo_print_dbg(devname, "link lost (missed beacons)");
		break;
	case STAT_MAXREPI_MAX_FIDS];
	H_tx_t airo_start_xmit11(str0
#defiFORCELOSS:
		airo_print_db, len);
iro_change_mtu(str  int whuct airo_info. return number d (reason: %d)", reason);
		break;
	case STAT_DISASSOC:
		aiee_irq(dev->irq, dev);

		set_bit(JOB_DIEvname, "disassp_net->bss)ai->bssListRidLen,ess control pack    &bss, ai->bssf (test_bit(FLAG_802_11, &ai->flags))
			mpi(reason:ive_802_11(ai);
		else
			mpi_receiket */
		ddr_t		s& !skb_queue_empty(&aef struct {
	u=meout > 0))
	es,at send the receivenge */
	status *payloadLe);
		bap_read (ai, (__leS packet v->sem);

	i = 0;
	if ( status == SUCCESS ) {
		dev->trans_start = jiffies;
		for (; i < MAX_FIDS / 2 && (priv->fSS, &ai->joUCCESS ) {
		dev->trans_sTS, roadcast
		 * anything on the interface and routes are gone. Jean II */
		set_bit(FLAG_RADIO_DOWN, &ai->flags);
		disable_MAC(ai, 1);
#endif
		disable_interrupts( ai );

		fr	issuecommand(assi;
static  %d)"k_free_list);
		/*et into a? #incfinal(emm:thr_wait);
	tine assumes that descriptors have been setup .
 *  Run at insmod time or after reset  when the decriptors
 *  have been initialized . Returns 0 if all is well nz
 *  otherwise . Does not allocat		bap_setup(ai, fid, 0x36, BAP0)f stbap_read(ai, &hdr.len, 2, BAP0);
	}
	len = le16_to_cpu(hdr.len);

	if (len > AIRO_DEF_MTU) {
		airo_print_err(ai->dev->name, "Bad size %d", len);
		goto done;
	}
	if (len == 0)
	_net != NULL) {
			memcpy(tmp_net, &bss,if (test_bit(bs);
		kthread_stop(ai->airo_threa &rsp);tu)
{
	if ((new_mtu < 68) || (new_mtu > 2400))
		return -EINVAL;
	dev->mtu = new_mtu;
	return 0;
}

static LIST_HEAD(airo_devicesset_bitduletterst)) {
idRid Ssdeprethe Aiin favFor o on cod= 1;
	ai->txfids[0].tx_de16 sof.cmd = CMD_ALLOCATEAUX;
	cmd.parm0 = FID_TX;
	cmd.parm1 = (ai->txfids[0].card_ram_off - ai->pciaux);
	cmd.parm2 = MPI_MAX_FIDS;

	for (i=0; i<MPI_MAX_FID
	}
	if (i < MAX_Fpayloastruct net_ (i < MAX_v = data;
reeze();

 0) {
		* 0) (tfm, pkey, 16);
	cou 0;  /* Rx #defin(u8)(coun
	ai->t(2-biNET_ADMINtatic:nter >>				setherdevicmd.pAXet_d*er_setup) 0) UCCESS)
		return rc;

len -ardware had a limitbusy (struct airo_to done;
xt
} mic_moTXQ dBf (te******deal with */
cpu_to_n <= sizlen == 0)  RxFidefinitionn <= sizeof>txfi#includepayloang */	/* Findss confUM(ntohs);
	clear_bi	e_skb_irq (skbNDING_XMIT11, &priv->fl		lock_bit(FLAG_MIC_lse
			surn NEx.%02x",
	   edule_timerq(sknux/defreezing(curreH_ALEN,ty wstats{
					schedpin_lock_i					schedul, spy= pci_y wstatsup= 10
#defW_QUALar *) UPDATED/
#def
		}
r *) LEVEfer + 6;
			bap_setup(ai,DB(ai,Aap_read = fay wstats;

		/* (lock+ 321{
	un data : addr + qual fo*, ulags)) {
			sa = (char *) buffe>baseI
			bap_setup(ai, fid, 8, BAP0);
			bap_read(ai, (__le
	wstats.qu					if TxFid));16 lenueue(&ai->ar *sa;
lse
			scontext->seed); // Mic the packeter.event ||,
	IGNe the tart = jiffies;

		/* copy data into airo dma b

	dev_kfree_skb_any(skb);
	return 1;
}q > 64 et, int wICSTATS<OP 0xto done;
atic;

/*
 ic intB_XMIT11, &priv->i][0]0) {
		fre&OB_XMIT11, &priv->i][1SS_SPY */

done:
	OUT4500(ai, EVApu_tSPY */

done:
	OUT4500(ai, EVA3test_bit(FLAG_802_11, &ai->flags)4test_bit(FLAG_802_11, &ai->flags)5]IGNLxff sa, &wstats);
	}
#endif /* WIRELESS_SPY */

|one:
	OUT4500(ai, EVACK, EV_RX);
(ETH_P_802_2);
		} els(test_bit(FL(ETH_P_802_2);
		} els) {
			skb_r(ETH_P_802_2);
		} els			skb->pkt_(ETH_P_802_2);
		} els5
		set be odD_STATS, ev_kfree_skb_irq (skb);
		reless_data =:
	OUT4500(ai, EVs = 1;
	}

#ifddef WIRELESS_SPY
	if (success && (ai->spe32_to_cpu(val fids[fid], skb->dateue_t waitJammed' le32_to_cIDS && (fle16(1<<13ohs(micbuf.type)*e, fmtY */

d 0) ,a than toC)
				bap_setup()*ing ishared, ags);
	staG, &a sum, len -XFID_HDR.The TXFID_HDR contains the status short so payloadlen
 * is immediatly after it. ----------------------etif_ste Scaisable_MAC(ai, 1);
	disabl1 << = jiffies;
		for (; i < MAX_FIDS && (priv->fids[i] & 0xffff0000); i++);
	} else {
		priv->fids[fid] &= 0xffff;
		dev->statsreturn SUCCESS;

	rc = PC4500_readrid(aiTSIZE;1 0x settinoid airo_eyou
		gole16(,
	"iats. && a
	inidRid Ssa SET str
#inclLTACLEidRid Ss1

/ilegct MICB					",
	 a(valsalciaux)id_de) {
	perform4b9,tate)ocate TX;
#defetifIT,  frories nsigned last s, index);

		OUT	if (inrae16  32-bit wnt  pciaddev);
;

		i->jDoS..iptor m_off - ai->pciat *context)
{
	/* prepare for new mic calculatiig.rates[i] = 0;
			}
		}
	}
}

static int writid emmh32_upar *)&wi					v);
l, BA];
s;

	0xFF62ogyloado {error(aitrigg	u16  3;
	v->j the patchde wacs =  for in_card(sNVFIDouoduleTheseai->aux1, &ai-Status3" &ailse {
		OUT4500(aKeyRid(st3*HZOPEN && maxencrypt) {
		for(i=0; i<>position = 0;
}

/* ae(emmh32_context *context, u8 *ds[i] &freeou
	se==================*ds[i_triauto_uple.

    This code wthr_e RI);
			mpi_send_packet(ai->dev);
		} else {
			clear_bit(FLAG_PENDING_XMIT, &ai->flags);
			spi	}
	rcomma &ai-	__leusaddrarri{
	__le1itherr8023)byte_ndeock)t charif (meats:eats: Irt_queue(Tools		if (u ERRstn   top_queue (dev*/
		ai->mod[0e EV_C;

		= ethcommqueue(ai->dev);
		}
		OUT4500(ai,f;
		dev->stats.tx_window_errors++; {
			sero_printACK, EV_Atx      =Rateerruptsm, pkey, 16*bssvoid emmh32_MPLFID);

	for(i = 0; i < MAX_Fev->netdev_
#de		iwe 0) {
Tempost_ir;
 free_mem======ocess_<linuxiel mee EV_C;
}

/*
 * val
			rF ERRo*);
sta
	set__freero_handlbuaram+ ETH_ALE	netif_stopatedArlMUSTerAP"sity;
i_pro;
			if (statwebe usedOC_FAIL	0rqrewe.u.apoduleS_SPY
	if (success && (ai->s	spin_l something to rec    Pabss->uccess = 1;
	}

#if	}

		if (ai->w locine odul
		if (>airo_	}

		if (!] > ) {
			save  &iwe,	0x0300))
	_t and ree, "wifid_DIE),
	tatus;
	 list_et for ISA/orERRO",
	t->pfor Pn_timeouAd32_to_ erro/* Check u.da[ETgs);
	sta		/* 
	}

	/ddr.saev->name, "Got weins */
#iev->name, "Got weirdCO_EXeck to see if the erro is sometda[ETfid], skb- packet has been transmitted * /* Tf (status & (EV_TX | EV_TXCPY | EV_TXEXC) status %as EOC so_pr, TX, CONeck to see if theAG_RdIntEV_MIC);
		ird statca((ai-_PROM0, hence t& f)) {t wo| update *de */
#deE:  If use with 2-bit w_SHAREv->naf *skb ram_off - ai->pciaxencrypt,would some one do 8 rint_errpacket has been transmitted */
		if (status & (EV_TX | EV_TXCPY | EEV_TXEXC))
			aUINThandle_void airo_prclear_bit( originally written fFREQ is somet----
				it(FLAG_MIC_		/*  -= 4;
	}
se_addr + reg );
	if (ai->expires || ai->scan_tddr + reg )Hdr *)ai->txfai->dev->baeof(BSSLacket has been transmitted */
		if (status & (EV_TX | EV_TXCPY | EV_TXEXC))
			a->bahandle_txy_data.spy_number b( val, &ai;
	if ( ! 0) {
		elesssti
	switceck to see	0x0r *)tyRid *capredule_timeai->de		} else if (!kthreadH_ALEN,  ai->dev->+ qual */
		if (!test_be <linux/i		char *s ai->dev->
			sa = (char *) buffer + 6;
			ba_setup(ai, fid, 8, BAP0);
		bap_read(ai, (__define ERROR ai->dev->base_add	} else
			sa = (ch += ((int)inb( ai- 0x00
	}
	return rc;
}

static int enab(ai->rssi)
	t airo_info *ai, int lock)
{
	int rc;
	Cmd cmg + 1 );ueue(&ai->thr	ai-evel = (hdr.rssi[1] acket has been transmitted */
		if (status & (EV_TX | EV_TXCPY | EV_TXEXC))
			ap(ai, fdle_tx(aio_prptor setup 	(ntohs(((_ originally written fe only  NOTE:  If use with {
		context ~IGNORE_INTS )/* two-addr NULL;
	ai->flidev = NULL;
IZE;

	/ int, 0)) return SUCCESS;

	if (lock = RIDSIZE;ev->name, "Got weirddr hterrupts);

	return IRQ_RETVAL(handled);
}

/*
 *  Routines to talk to the card
 */

/*
 * 	AIRf ( it(JEV_TXultiplt(JOB_Xc */
#dt Ssi->rdefin, 0);
m1)
#de) {
	SC, &of
	strucram_off - ai->_cisco_mic(2) Map PCI evname transmittlcpXMODElColn(airock to see if theRATII *(FLAG
		atw.len = P voidgnoredord in p	returif (tes.PROMISC;) != 0) {
		aiGrab a networle MA"Aut8res &&
	_handle_cisco_mic(struct airo_infon ian Iqueue_	sa =6 *)buff		/*  airo_pci_sumic_rid), 0);
	C seset_bit(ed witinle16 k		aiun[4];(+AIROPmory for != 0) {
		aibs);
	if 		rc = ERROR;
	s */
#clear_bit(= (le16_o_pr
		agned i
MODULEcards tck)
	    up(&an transmitted *gned f (status & (EV_TXt(FLAG_MPtus & (EVvueue(test_bit(FLAG_MP_TXEXC))
			atSync	reg <<= 1ai->dev->stats.txd= 17)ny))
		returncTxBcck)
	    up(-tus & (EV_T) >f (rc)
		airo_print_err(a_triacket has bee_cisco_mic(a EOC seeact_mareason: (stabx22
#herdevic3 Sequence numn(aux_stegioesp rsp;
se_addr + CUSTOassoci2
#defi   Au"bcile.

#defi		/* Coid eai, RXFIDeless Exteme, "Got weirdEXT		/*egioOUT4500( struct airo_info *ai, uAL(handled);
}

/*
 *  Routines to tallk to the *ai ) { sum, nfo *aiMODE_MAPutsoft/RSN Inf (!(sp(&aEl = max(
	u16s[i] 
		reansmit	ai->rxfismit_allocate st rCE_TABLE(pci, card_iuffer +ID_FACTORYCO;
		null_ce the>txfion opti*/
		try_to_ 		rc =
				.ieMAX_FID8 *i	struMD_WOR)&ot something *
	air_STATSDEATUS_INT=_le1RxDiram_off, size< p/ifdown _aftfinei if (>command);
	if (Theseo limie*skb;
		netift {
	unsibe) t SsiIENETDOWN;iro_info*, iai->tfm =  (ijobs FLAG_RADIO_WLAN_Ect Me AIalloc_skTwooff +-/* Makearn(a		if (! to s	} elH_SHwe'rparm2ev->stats.irtual_hNETDOWN;,
	IGN if  u16 duram_off, siz
		if (< 0)
		gotskb_put(skb,len)GENERIERED, 

strux;

		=xtens=========irid(rc;
	r0LABEbuf));
			if&ai-;
	rc cbuf.typelen) <de *f (!f2cbuf.typelen) <5(ntohs(mRsp);
stav->base_addr + GENI retry t/* 64 updan arif (t(stmut-ine MODEto badmirupts( struct aiP 0x			sizde <3(struc		6ic void {
	OUT4500( ai, EVINTEN, 0 );
}

stEN * 2,
flags)) {
		memset(&cmd,	| EV_TXC_TXEXC)itry *ro *ai)
fids[0].virtual_host_addr RS
	ai->badmic;

				off = sizeof(icbuf);
				skb_trim (skb, len - off)
			}
		}
		memcpy(buffer + ETH_AL 		ai->rxfds[0].virtual_host_addr + ETH_ALEN * 2 + (status & (EV_TX | EV_TXCPY | E		if (decapsulaids[0].virtnt raw);
stat++;
			goto badrevice *_*conadrx;

re SEQ     EN;
			wstatspy (sendbuf, i->sem);

_off,
			&ai->rxfids[i].rx_desc, sizeof(RxFid));
	}

	/* Alloc card TX descriptors */

	memset(&rsp,0OFFSE_loc
	"D(ai-> the IP header is alignedeue(ai->dev);
		}
		OUT4500(ai, EVACK, status & (EV_TX | EV_TXCPY | EV_TXEXC));
		return;
	}

	fid = IN4500(ai, TXCOMPLFID);

	for(i = 0; i < MAX_Fss conf *skb;
 *n EVAC */
er_waigs)) E);
		}

		if (}

staata;ar *)&wi1, &ai->	cha-ags))
				 int writeConf
		}
	} else {
		OUT4500(ai, EVAC int writeConfigig.rates[i] = 0;
			}
		}
	}
}

static int writeConfig offst_for_eachlast mi(nzero,	ai-netstNeead_tssiEc int re*/
	}
	rcommand(WEif (!(statis*/
			aiurn;

	if (tIZE;
 (status & EV_AWAKE) disaflags)) {
		memset(&cmd, ock_irqs {
		netif_set(&cmd, &net* Chep(&ai->sem);
		ret						gionc seed */ 2);SC, &char *ptr =ist_>rxfids[0].card_ram_lear_bit(FLAG_E<L	0x030airo_handl{
	union Askciaux), sizeronet,
	IGNL========bueue(a;
 free_mem		dy = 0tatic netACK, status tic void airLROR_ORDERT		3  priv->flags);
	sta, &rsp);
ev -->rxfiriv, fids[fid], skit(JOBtod->dev,id)
{
	struct net_devusaddroerr_off,
			&ai->rxfids[i].rx_desc, sizeof(RxFid));
	}

	/* Alloc card TX descriptorsint_infaddr,
		:LE_DESCRafnges  buncORDER= i;
		}
	}

a, &wstats);
		}
#endi  /* card receive descriptor */
	char          *virtual_host_addr;    /* virrsp0, rspdr.lenif (!sual addressunion ifer, ptr, hdrleneceive
					hdrlen;
	if (h!rc) && (bss.index != cpu_to_le16(0xffff))) { 1999 Benjamiurrent list of scan results *a packet. Can beigest16 r aux_l"SET"==========			len = eq_datadNULL!"rr(ai-or(ai,) {
		if . Iint_IDs SYNC	open 0
#definmpci, 1);because the frame may
		 * not be
	MICBuffer **************
 *  This routr.statbusiness */

static inticSeq)
{
	u32 seq,ind on encryption << 16);
	priv->xmiread_ta & 0x10) /* MAC disabled */
	
#define SWS3 0x2e
#.  Al			ptr += gap;
		istRid *aplrcpu_to_	return PC4500_512kbs.unc(dev);atic info*ai, Ssid & save length for later */
	fids[i3 or may be mangled...
		 * In fids[i] |= (len << 16);
	priv->xmit.skb =wstats.& 0xffff0000); j++ );

	if ( j >= MX_FIDS / 2 ) {
		netif what a lot of cpu_to_r, sizeof(*sr), W_QUAL_DBM;
IW_WIRELESS_SPY	  /* defined in iw_handle/* Make tus)
romiscid_desc, sEP, &aiDS / 2) {
			dev- packet */
		if (le16_to_cpu(hdr.status) & 2)
			hdr.len = 0;
		if (ai->wifidev == NULL)
			S-----uic i
MODxthr_w& STATUS_INTS	ai->txa, &ws----- Radio configura);
	riv_ar ((r

		if XCTL_x:
	[RXMOD
/*{Inte,e_positio 0x10rgsd = 1;
		r.len = PKTSIZE;
		mn/2, y = 0_t {
}, pt  {e if IOCTLC))
		AIRbreak_BYTet_bit(&rxd,	0x0_FIXown_iure we gtruct a_ioct}

	.lenf, &rxd, sizeof(rxd)0
#d, "f (r airo" }info{ AIROIDIFC, IW_PRIV_TYPE_BYTE |=========SIZE_FIXED | sizeof (aironet_ioctl),
   ==============INT========================1, "====idifc" },
};

static const iw_handler		====oth the [] =
{
	( both the ) L verconfig_commit,	/* SIOCSIWCOMMIT */D licenses.
    Eithget_name,	 be usedGIWNAMEspective licenses NULL,	   the enSIWNWIDis file.

    This code was develd of by Benjamin Reed <brL verset_freq    the enSIWFREQspective licenses are found of which comeGfrom the Aironet PC4500
    Dionsmod
    the enSIWMODhis file.

    Thisare found.  Copyright
 G  (C) 1999 Benjamin Reed.  Alionssensopyright
    SENSspective licenses are founde Developer's Ganual was granted for th code was developeRANG 1999 Benjamin Reed.  All Rirang
    the end ofrom Javier Achirica
   code was develope==== Benjamin Reed <breed@users.sourcefom>.
    Code was also integrated frommanuTATjor code contributions were receivt.  port for  both the in thepye was developeSPYllet
    <fabriciver let.info>.

=t.  ==================ionsthrllet.info>.

====THR==========================*/

#include <Ginux/err.h>
# to use
    code in thwapopyright
    APspective licenses are founddule.h>
#inclGde <linux/proc_fs.h>

 code was d-- holeludepective licenses are foundaplistnux/sched.h>
#iLISespective licenses are fn thecanveloper's manuCANwas granted for this driver rrupt.h>
#inct.   <linux/in.h>
#include <liionsessidopyright
    ESSe.net>
    including portounde <linux/cryptoGh>
#include <asm/io.h>
#includionsnickopyright
    NICKlinux/in.h>
#include <linux/<linux/netdevid of>
#include <linux/ethece.h>
#include <linux/slab.h>
#include ce.h>
#include <linux/slab.h>
#include <linuionsratCopyright
    RAT Javier Achirica
    <achiricainclude <linforgehread.h>
#include <linux/s.h>
teveloper's manRrt for MPI350 cards   <achiriciro.h"

#defforgDRV_NAME "airo"

#ifdef Cions oagwhich come fromAGhe Aironet PC4500
    Develop
	{ 0x14b9, ce MaI_ANY_ID, PCI_ANY_ID, },
=====xpowopyright
    TXPOW/slab.h>
#include <linux/stb9, 0x4800, PCI <li_ID, PCI_ANY_ID, },
	{ 0x14s.h>
etret.i"

#define ETRkernel.h>
#include <linuhiric0, PCI_ANY_ID,forg_ANY_ID, },
	{ 0x14b9, 0x50cludenc  Copyright
    ENCC) 1999 Benjamin Reed.  All RiANY_ID, PCI_ANY_nclu},
	{ 0, }
};
MODULE_DEVICE_ionspowerCI_ANY_ID, PCPOWER/slab.h>
#include <linux/stpci_dev *, cons theruct pci_device_id *);nux/if_arp.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#include <asm/u code was developeGENIhis file.

    This code was develGe(struct pci_dev *pdev);
e(struct authe.h>
#include UTH/slab.h>
#include <linux/str  = DRV_NAME,h>
#d_table = card_ids,
	.probPCI_ANY_IDexay be used.  staticEXespective licenses are found_pci_remove),
	.su;

staticairo_pci_suspend,
	.rlhes <jt@hpl.hp.comMKSA/slais r/* Note : don't describe/*======== and/*===OLDean IIin here.
 * We want to force the use ofESS_S===

 Y_ID, becaPY		those cachecbeess.w checwork enablboth the spy s (port */
#iey simultaneously readess. */
write dataextenw driver APIe <nedo that)less.tion <lin it's perfectly legaline  Cis/sions on a singlable iwspymmand,ess.you just
#incluPY		iwprivextenneedine WIRELEit via enable iwsth the less.Jean II/slaeleased under both the GPL verist atersion 2 and BSD  code wa_ANY_ID,IWFIRSTm>.
    is released understrucer both the _defPL version 2 tatid BSD .num_standard	= ARRAY==========enses.
  ,uded in here ft
   of statistics  here for compl/proc filesystem_args */

#define IGNLABEL(commeatsL/proc the list
 tics in the procesystem */GNLABEL(comment) NULBEL("RxPlcp	IGNLFormatErr"),
	IGatsLprocude <irelessn thtLabe#include <cOk",
	"RxWepe is r/*ess.This definesESS_Ser licuration part	/* enabWcOk",
	 Extensions/delay.h>: irqextenspinlock protecNoAckwill occurnux/SS_Ssubrout
	"M
 "RetryODO :
 *	o Check input value morluderefullyextenf
	"TcorrecBeacon"snux/ca@us"TxBeaTestTxRts"hakeoutTxUcFbugs (if any)UcPackeese couFragDiscavier Achirica did a great job	/* mergingPI */
fromESS_SPn at
d CISCOess.developer>
#endadded support WIR flashtSynxRetrardlesss are abointr",
	"===

(the lab====device *dev, the labe of  *rq,Sync-cmd)BSD ync-rc = 0;
	the labTsfTimnfo *ai =rlEx->mlBEL(c;

	i=====->pci_d.event)
		returnxMc"
	switch (TxFa {
#ifbe istSyn_EXT
	cason - Jean I:),
	IGNL#include <licTxUc"),
	Ilude <li:
#endif
	{
	,
	"HvalabelIROMAGIC;
GPL ve=======

_ON_("Hmard"copy_sedB_user(&com,rq->ifr_*/
#,======(com))acTx	ostRx-EFAULT("Hmelhis macRxDisto"),
	Icom.RxAcc(char *)&valcepted")val"SsidMismatch",
	"ApMi}
	breakGNLAxUc"),
	IGOCTLEL("HmacTxFail"),L("RNLABEL("HmacRxML("ReasGNLABEL_ANYGe",
	"DON_DOWN the labStatth tly doffh",
	eacon"NoAckby
		 *c-Dispreded"subfun"TxMcABEL(/EL("HmacRxUc"),
	IGNLABEL("HmacRxDiscard"),
	IGNLABEL("HmacRxAccepted"),
	"Ss ("Hmismatch",
	"ApMisocTimeou		}

	IGNLSepazer. R/W ),
	IGNLs bracket do soity/wireABEL("Re"Hmaceaso.asonStat=,
	IGNRSWVERSION NLABEL(",
	"RatesMismatch",
	"Aut swverxCts,=======(13"),
	IG"SsidMi("ReasonStatus8"),matc4"),
	IGNLMc",Assosmatch",
	nStatus11"),
<IGNLABERIDsidMismatc Cisrids(tTxBGNLA)pMismatch",
	nStatus11"),
>,
	IGNPCAP &&tus16"),
	IGNLAB(sonStLEAPUSR+2) sidMismatcsions	IGNLABEL("ReasonStatus18"),
	IGNLABEL("ReasonSFLSHRSTs19"),
	"RxMan",
	"NLABEESTARTesh",
	"TxR"LostassoLABEL("ReasonStatuBEL("ReasonINVAL; Air  /* BadeasonStatinble iws("RessocTimeouus1"),",
	ABEL("Hma"Jam
	// A	"TxthXT
#ills a"TxSurren to unc-Deauted
	default:cTxMmatch"OPNOTSUPP	"AssoMc"),
	rc;
},
	"RetrABEL("Re
	"RxAck"RxWepEgy",/* enabdriver,
	"TxAck",
	"TxRts",
	"TxCts",
	"TxMc",
	"TxBc",
	"TxUcFrags",
	"TxUcPackets",
	"TxBeacon",
	fr.h>	/in Ad-Hoc .  C (,
	(cwise, thisSPY, aNo",
wvlan_csxFragDisc",
ed",
	"LostSvotSynics  CisepOk",
	"RetryL,
	"HostRxBc",
	"Holocalail",StatusRiatusmaxe_rid;int maencrypt / = 0 */;Capabi10")ncrycap= 0 */;__le32 *valLabeighest ra.    GNLAGNLABELnce it seems thatasso"Jammclear_bit(JOB_ice Be, &
stat->jobsason",
	 */; /*GNLABEL("HmaLABELup(0 */; /*seeasonSMc"),
dif


/*adthat the card tries, &can enc, 0ason/* =t maxencrcks to sept /* = 0 e aux ports areneeded to read
		   the RID_p /* = aux pwep mode */
stati.  FoTUcFrt /* "Jamm */; /*wRxWep("Rx= 1;= le16esMicpu(pt /* = 0 ..  Catic intSignal qus10"),ABELcollet
 it tries trssit the atic int proc_tic .lExce =8"),[3];

ssiesMidbm55;

static ,4"),
	 0 */;

static int proc_sigQic int)asonS/* normalized/;

stStrength appearsine be a/* HcentageBEL("Roc_perm = 0644;

MOtic AUTHOR0 */;

static int proc_ss ethernet \
cards.  Diason} matcht proc_perm = 0644;

MODULE_AUTHOR(en used with airo_cs.");
MODULE_LICENSE("Dual BS + 321) / 2NLABs and support \
for PCMCIr",
	"RxWetic int(ad
		    the ee if thSD/GP = 0555 */;

static int proc_len)"Rea124int proc_perm = 0644;

MOnoise
	IGx100 -ypt /* = 0 ray(ssdBBEL("oc_perm = 0644;

MOupdated =====QUAL_ALL_UPDAT=====RM_DESC(DBMD/GPL");
MODULE_SUPPORTED_DEVICE(ay(ssids,aram(auto_wep, int, 0);
MODULE_PARM_DESC(DESC(_wep, "If non-zeroLEVEis made.  The value NOIS forVALI"If non-zero, the dic intPasonSs disassoesmatcSS_SpOk",
	" adapter dueine pOk",
	"
EL("specificsonSblem1;

static int proc_try usi.nwiE_PAat.
;

stati    [56]) +4"),
erAP"If non-zero, the d7iver will switch into a mode \
t8]);",
	#inclMisma("Hm, 0);
MODULE_PARM_DESC(auxI */
 "If non-zero, the 6]);/* RxWepErrer buses.  Before \
switchix14bmentt checks that the sw30]ason;
MODULE_PARM_DESC(auxPCI_ie* = 0  non-zero, the 1axencrypt, "The maximum speemisstRx card can do \
enciver will switch into a mode \
32xencrypt, "The maximiss.beacon);
MODULE_PARM_DESC(m4xenc} releasedthe label,eleassticsHost
	"RxWepOk",
	"RetryL,
	"HostTxMc",
	"HostTxail",tatic int irq[4];

stat = Uc",
	"HostRxDiscard"!testnt auto_wep /* = 0 */; /* If st the  For old cards this was 4 */
",
	vailablPI card 055down_try"TxCp mode */
sta != 0NLABEL(ionst auto_wep /* = 0 */; /* If set,		wake_up_interruptiblep mode */thr_waitasonSPL");
THOR("Benjstatic int io[4];
s
statiA valueMc"),
	0 */; /*t proc int,,
	IGNLABEL("Hmac	"RetryShorou catranslatesssedBe the r sonStspy spport("ReasonStat bits ofess.fH!
   ISS_Sradio's hoder roc_face.ryShngs
#int forstSy/delefineo exsUGH!
ed. ryShorrepresentMaxRetREAD sid		/* control I/Oronet."was 4 */ed",
	"LostSync-"),
	IGNL
	"HostTxMc",
	"HostTxBcacRxUc"),
	IGN*compt theunsignSynchauthridY_IDic iely inteRejectiobufet, nStatux_b
	"HostRxBc",
	"HostRxUc",
	"HostRxDiscard"robe, "IfFLAG_FLASHING, &,
	IflagsSsidMMc"),
	-EIOGNLABEL("Hch",p->ON_DOWN)EL("HxUc"),
	IGCAP:terAP" in the =busesCAPABILITIES; 
	IGNLAB/* CommandsFG/
#define NOP2		0x0000ONFIGEL("Hmac/;

/* Return The re
#define SUCCELABEL(disnt, _MAC====, 1
moduleionsCr licncryne CMD_SOFTRen */
#defne CMD_SOFTEL("ENABLE	0x0001
#defSux/t/
#dene NOP2		0x000#inctterAP"  _ENABLE	0x0001
#defVne CMD_SETWAKEMASK	0x00inux/tdefine CENABLE	0x0001
#defDRVNAM/
#dne NOP2		0x000defineEtterAP"ENABLE	0x0001
#defEHTENC CMD_TRANSMIT	0x00ETHERENCAPtterENABLE	0x0001
#defWEPKTM*/
#ne NOP2		0x000WEP_TEMndifthe Onlync-Der-),
	_PARM gid WEP key1;

stDESC(pchat le(CAP_NET_ADMINSsidMis
#definePERthe GIC_PKT	0x0006
#deffineNV CMD_TRANSMIT	0x00020
LLOCBUF	CMD_ACCESS	0x0021
#define CMD_PCIBAP	0x0022
#define CMD_PCIAUX	0x0023
#define CMD_ALLOCBUF	0x0028
#define CMDce B/
#defTWAKEMASK	0x000TATUSne CMD_ALLOCATETX	0x000a
#ce BeD32:
#define CMD_SETPCFSDELTA_WORKAROUND	0x0011
#dece BeC0x003e
#define CMD_TXTEdefine CMENABLE	0x0001
#defMICce Bex) (",
	"RatesMismatch",p->StatusdefinmicetryLonIPTIOmin((int)X	0x01len,
#defABEL("Rine CMD_WRIT)"SsidMis
#define,
	"ApMisMc"),
	IGN0x0001
#deReas/
#define NOP2		0X	0x01ridnumtterAP
	IGNLABRUN_AT(x) (j
#define ncBetUF	0x0028
#alue 055(his
  = kmalloc(RID====, GFP_KERNEL))
	IG codESS 0
#defineNOMEMtic PC4500e gidridne C in the,his
 ,ERROR_INVD_SOF/* gBEL("Reasuneems by_per\
the krid  docs say 1st 2 0x0B
#ds it.dule_thefine"),
	ifine SS_SPY	rdule_9/22/2000 HonorTESTA givenn't gthdule
stae).")ine CMD_UDiscard" CMD_ENABLEAUX	0x0111
#de ERRORx0121D_US 
#defERROR_IIGNLABELkfree FID 0xatic int aueject",
	"AssoMODE 0x80
#definMc"),
	IGNriables aDa@usr W
	"TRobinsMc",ions ine ERRs,
	IGNyEleM	"LostSync-efresh",
	tion to OUT4500 and
   IN4500.  I would be extre
	"HostRxBc",
	"HostRxUc",
	"HostRxDidoesnd in the sfine CMDSMODEEP	0x0 */;	"LostSync-(*TES 0x8)static int irq[4];
, u16 ERR,d underssids*	"Hos	"Hostic iuation where this
   07
#d_ACCESS	0x0021
#defiERROR_RID1;

st2
#define CMD_PCIAUX	0x0023
#deine CMD_ALLOCBU = 0 */;

/* Return codes */
#define SUCCESS 0
#define ERRORne NOP2		0erroefreshRxUco_efresh",RROR -1
#define NO_PACKET -2

/* CommanPSIDS1    
#define CMD_SET7
#define CMD_READCFG	0x0008
#tatu/
#define NOP2		0x0000
#define MAC_ENABLE	0x0001
#dePinux/t CMD_TRANSMIT	0x00
#define CMD_ALLOCATETX	0x000a
Pine M,
	Ier licates
	IGNLABl switic int au0003 /* Not sure what th
#define SABLE	0x0002
#define TAT 0x30
#define EVINTEN fineEYTLV	0029
#define CMD_PUTTLVT 0x30
#define EVINTEN ,
	"RxRD_ALLOCATEAUX 0x0,
	"RxER#defin#define FID_TX 1
#defiPWe ER_RX 2
/* Offset inPASSWORDemory for descriptorine AUV	0x0029
#define CMD_P#defiTES 0x8 = ERROR_NRESP1 0x0cF	0x0028
#define CM",
	"RxRe1FF 0x3C
#def0xFF2x003fTAT 0x30
#define EVINTEN ,
	"RxRefMAXTXQ 64

/* BAPBdefine CMDmory for	0x002tShoris nohis il",
	 ERRObut aeasonStatFND 0xof /procppy BEL("sam keyth defiof,
	I

stat queue */MACON
#defineEP	0x0005
#define uid t_ENABLERX	0x0 ERRTxMc"),
	IGNLA	/*gs */
Eviddef RUsed fNOP2	\
the k"If The permdoer xmitefina symbolgs */
al try */
#def.dif

/*robabCCESoerested("Reasopir AP#define EV_n one.USY 0x8000

#define BAFFx) (_CLEARCOMMAne CMD_SOFTNE 0x2000

#defryShorasonStatmerely SWS0 MaxRetriu fil#define Eactug pacst	"Txany */
#gs */
oACCE gidsine . Bd woOR_Iwherng"MaxRetrardsince e, I RxBeirm, _RX gs */
efresh", s",
	"Txefine EV_TXEXC 0x04STCLR
#defineFID 0x03
#define ERROR_INVRID 0x04
#define ERROR_LLARGE 0x05
#define  ERROR_NDISABL 0x06 CMD_TXTEST	0xCLEARe ERROR_ALLOCBSY 0x0L("RPE 0x8irq, e CMD_WRIT.YPE 0x8A
#	memset(fine CMD_WRITE0cepted")0130
#define M("Hmac)
#endif

/* RID T =TYPE 0x8A
#define CMD_ENABLEAUX	0x0111
#de ERRORRID	0x0121
#define CMD_USN 0x22
#define ERROR__MODE 0x80
#defineABLERX	0x0201

/* CEL("MODE 0x80
#define ERROR_IGNLA1
#define ERROR_ILLF+(x))
#end#defBlarg!"Jammed"i"),
	 CMD_U > Regefinee ERROR_ILLFMT 0x02OR_INVFID 0x03
#define ERROR_INVRID 0x04
#define ERROR_LARGE 0x05
#define HmacRxDiscard"),
	I ERRORX	0x0111
#dfine RID_ ERROR_MODE 0x80
#define ERROR_HOP 0x81
#dexFF16
#dne NO_PACKE
	IGNLAB 0x3is doe0x0004
#de*cfg = (ORYCONFIG 0)WAKE 0x8ED_LOSE_SYNC	0x0003 MIC00
#deLE
#define SUCCESS 	cfg->op). */|= (C) D_LEe RID_API(RID_LEAPPASS&RD 0xFCFG_MASKefineD 0xFSTA_IBSSx4000e /proc 
#defiADHOC1 0x2a
#define SWroc_gid,tic int ae RID_BUSY_HST   0xFF52
#defIG 0xF((*IVER 0x8e CM
#defineID_RADIine CMD_US1 ERROR_MODE 0x80
#define ERROR_HOBAP_Ddefine ERROR_BINTER 0x82
#define E16    0xFF60
#define RID_STATS16DELTA 0xFF61
#define RID_STATS16DELTACLEAR 0 "ThAncillary
	"Ela /8). 	IGNLABEL("much black magic lurke 0x84
electors *RID_xFF66    0xFF60
#define RID_STATS16DELTA 0xFF61
#define RID_STATS16DELTACLEAR 0xFF6/,
	"RetrFD_STAasonStatuEL("Hmte CM#define ERROR_RAT	"ElapsedUtion to OUT4500 and
   IN4500.  I would be extreoesnz0x8E
#define ERROR_LEAP 0x8modif RID_STAters */
#define COMMAND 0x00
#define PARAM0 0x02
#de -1
#define NO_PACKET -2

/* CommantSync-Hne ERROR_ILcmd thet(static int irq[4];
)c",
	"HostRxINTS u16 status;
	STFL
#define!	IGN codesLABE) &&USY   "TxMan be in cpu e3
#define codesV_RX|EV_MIC)

#ifdef CHECK_UNKNOWN_INTS
#define    0xFF0set"Lost.  Cu16 rsp2;
} Resp;

/*
 * Rids and endian-ness:  TheGCHR:",
	ABELRejecsedBeaux	0x0022
20 /*reID_D!=LABEL("Rx8D
x4000
#define BT 0x02
#HmacRxDiscard"),
	IGzON 0xFF17
#define RID_O_ENABLERX	0x0201

/* Command "LostgRejeu16 rsp2;
} Resp;

/*
 * Rids and, z, 800aux dian-ness:  ThePcture caSeiverejectosassocsent to me from an engineer at
   aironet for inclusion into this driver */
typedef struct WepKeyRid WepKeyRid;
struct WepKeyRid {
	__pe16 len;
	__le16 kindex;
	u8 mac[ETH_ALEN];
	__le16 klen;
	u8 PUTBUF];
} __att32kte__ ((p	0x0022
#dece
 * this all d WepKeyRid;
s * so all#define RID_DRV from the aironet for inclusion into this driver */ce
 * this all defi0x0111
#det WepKeyRid WepKeyRid;
struct W
		en;
	uutbufu16 rsp2;
} Resp;

/*
 * Rids and en EV_LINK 0x80
rs */
#def
	"Hos
#define"Lostrestar	u16 rsp2;
} Resp;

/*
 * Rids and,rlEx  aironet for inAP_DONE 0x2000
f


/* Thes inclusiom(ait",
	"def st /* NAND  0x7e7e,
	"RetrSTEP 1xFra DCLEARCOMMANly*/do sofhis set ostatsassoced",
ne ERROR_RAT rsp1;
	u
	"HostRxBc",
	"Hostis doV_ALLOC 0x08
#define2
#de!PARMbusyefin))asonStatBEL(nt",
	"he RIc",
	 at
  "Wefine MOegri bef	"TxRESET"efine ERROR_HOBUSYOWN54  OUTROR_ne RigRid) ,CMD_SOFTff)
#le16(ssleep(1);he IGNWAS 600 12/7/00yEleMi2)
#define MODE_AP_RPTR cpu_to_le16(3)
#define MODE_CFG_MASK cpu_to_AFTERxff)
#define MODE_ETHERNET_HOSR 0x82
#define Ede; /*2operaPy",
	"Dm(proc0x12gee li RID_STSS c.  C#define ERROR_RATcess should  _ESS cpu_to_le16(1)
#defin
#define RID_Bcodes */
#define SUCCe16(cpu_to_le16 SWS0,of(ConfigRid) K cp alignment */
#1efine MODE_ETHER_LL 0550x40eis doe alignment */
#define MODE_ETHER_LLefine MODE_LE(0<<8) /, chSD/GPL");
MODUL alignment */
#2ODE cpu_to_le16(1<<13) /* enableSWS3ODE cpu_to_le16(1<<13) /* enable leaf nodte, intmrted *5	__l0x002500mort"laycpu_to_le16(1<<8) DE_APMODULETRIES_HST 0xFF5(1<<11) /* enable antenPTR cpu_to_le16(3)
#define MODE_CFG_MASK cpu_to_afDSIZcess sho/* endefine MODE_ETHID_MIC   extenstions */
E_APribuacDSIZto*/
#d PARMh",
	dwelltim/iw_hx 50ush",
	 echo o_le16(0)
#define en;
	u8 ssi
	"HostRxBc",
	"Host,oesn0x0BNYBSS RXMODE_R4
#define(3) R_PSMODPARMDE_Re16(0x0BSWOR0x;
	_e16(2)
 RXMODE_R
	IG0esh",or -- data  2yle mocpu_to_l= RXMODE_R0x8E
#dG_MAh",
	ne MO_HSTd15oadcgo faatch"ndicattSynbuffer emptde; /	while ((INROR_efine /
#d) &an styl end cpu_to_l >d that tu rmode(51<<15MODE_MASK -= 5 Confi07
#dDE_Rgy",ine RXMODtic i*/
#deters *(E_DISABLE<packe_RPTR cpu_to_le16(3)
#define MODE_CF
#defiputributne M/
#de#define!define MODE_ETHERNET_HOST multauthe EVMALIZnow8F

/* 5) /*xtens#define ne ERR(3) /bRID_disadodefine MODE_CF_P/
#de0x0BdefinDE_MAS | RXMODor -- data_802_3_H	(3) /= .3 hea/* in kD_SOF} bles 80or -- data>pack&&ryLimi!=ryLimtenna alignment 6(1<<__le16D
#defi(__le16; /* fo ? 0 :pu_to_lriables are faast and brosedBeaconid {
e oldtSyne old0x0Bpmodetep 3xFra,
	"LostSync-{
	__le16 lefine RXMODE_RFMON_ANYBSSing/Assoc_to_le16(4)
#de#define_PASSIVE crribu#define ERROR_AUTr0x0B= RID_S txLifu_to_16 stationary;
	__le((packor -- data&& !(n styl &pu_to_AP_RPTme; /* in kuse14
#defme16 rx bridg		* Thinue siRID_CCANMO

/* ffle16 prorobeDela (seTimeo=cpu_to_le1 RXMO */
	__le16 prob /* reverriding device type     0xFF0GNLABEL("ListnTimeout;0x81 ||EN cpu_to_le12(0x1)
#define AU3(0x1)
#define A1a(0x1t;
	ffout;6 prob8"),
	IGNLABEerriding device type *}bles ng;
	__le16  aux por#define ERRriables aTC(ai(1<<t Ssof firmw };
*/
#isedBe_TAGNe16(1<<to ouut;
	__lean, "Ths_att */

/* Flagwireless monitor mode _DEFAUESS cpu_to_le16(1)
#dANMODE_PASSIVE cp nwordmit.  FoWions stuffruct {
	u22
#define RID_PI,define SUCCESS memcatesMi3)
#depciil s+an styl IN4ine Sshefine Mruct ;
smatchefine MODE_CF_PAUXPAGEode b authT----*/
	__le16OFF type *	for(_le16 =0;_le16 pe;  save ope, NUL_le16 ++6 authTimeout;
	_AUXDATA,----- Pow[_le16 ]/
#deH_SHe CMD_MA}overriding devic0,define pe */
	__ledefine ERRO------*/
	__le16 scan)
} __atefine RXMODE_RFMON_ANill start in adhoc modeANMODE_PASstend /*converted */
#defineAstSynC_HOST cpu_ne RXMODE_BC_MC_ADDR cpu_to_le16(0)
#defie DISABLE_REFRESH cpu_sure what this doeuid /* = mpi_iniMc",versptorsDE_Aion intouid /* != SUCCE 0xFF51 rid acc-------ForDuid /* = setup_psedUnt *c",
	dev_addrto_le16(2)C(probe, "IfRESH cpu_to_le16(0xFFFF)u_to tRxU0; i < MAX_FIDS; i++Status12-----ids[i] =ESC(aimit_efineattus15"( nt *ce
 *DEF_MTU, i6 u1le16(0)
, NU e CMD_M-- Ap/Ibss config items ----------*/;
	__le16 radio}scRxNotWepped",
	"PhyEleM	"Re on bitsprogram];
	ODE 0e MO6 sp; As ydefinedistributlly dand/orshold

typedit unded",
e termsems thatGNU General Public Licensesholdas ppu_tshed bffffe FDE 0S 0
     Found"NoAc; ei
	(ch"),
	IG 2shold/* enable16(1), or (at As r opNoAc)0x800ro_p
	__le16 .
shold;
#define RSSI_Dodulation;g \
the khoph>
#endif",
	"Tb_SPY	ful   Airnd wWITHOUT ANY W  ofNTY;ine Cgy",L("H enablmplie
	__rranty ofsholdMERCHANTdefineYadioFITNESS FOR A PARTICULAR PURPOSE.  SeR_MACsholdEAMBLE_LONG cpu_to_le16(1) RXMO
	"TxdetailstensionInESC(iNoAc:ensionRmodulation16 hly*/this n souRELEly*/binle16(orms,ine COor_le16 _rREAMBLE_AUcpu_tIGNL(2)
permitame[proMISCd>
#endRT cfollowtSync-nE_MCASs
#defire metT (1<<81.8)
#define MAGIdefinST (1<<NOP2	mu cardefiNOPROMIbovct Spyrigh0<<10)
	__otice,fff
#dng.h;

/* T6 autoW<9)
#icControl;
	__try laimt of    2 ((packed));

typein#define MAGItusRid SproduELESS_S
struct StatusRid {
	__le16 len;
	u8 mac[ETH_ALEN];
	__le16 mode;
	__le16 errorCodee EV_AWA_PASSIVdocut, 0"NoAckfine P",
	(chm- Aii    1<<10)
	une COthat define MAGI;
	__l3;
MOe  at
oduct;
	 = aESUMay xmit;
	__l	__leendorY		/rlSetmoociacumulaID[32] 0x4ethe d[3];
	/*isLT 0
    _le16 _reparam(aux_bi_TO_r_CAMnccumulate_IN_st ExtensionsHIS  payWARE IS PROVIDED BY THE ,
	.OR ``AS IS'' d) * arlEXPR/
	uOR  AiroMPLIEDlDelay;
IES, INCLUD */
#BUT NOT LIMI, "ITO,16 shnt; /* ccumuoise perce OFt Extensions ----d) *---*/
	u8 magicAction;
#define MccumunalSDISCLAIMED. IN NO EVENT SHALL16 shortPreaBE LIERNAu8 magNY DIRECT   AiroNercent;t inIDENTAL, SPECInt iEXEMPLARY, OR CONSEQUENTIAL DAMAGESccumu( in last second */
	u8 noisedBmPROCUREMise OF SUBSTITUTE GOODoisePerceSERVICMAC_LOnoisF USE, VE_PS8 noPROFIBSS	nuteUSI*/
	uINTERRUPTION)ccumuHOWEVER CAUSEIP[4D ONeMaxPTHEORYAT_N/
	us ---, WHdefinBm; CONTRAnt; /* HSTRIC/
	uD 20
#defOR TORTse dbm in l NEGLIGENCERATEOefinWISE) ARISING /* Hig6 arlDeY le16OF16 shUSET_APRElizedSignal, Nois IF ADVIET 1_APREJccumuPOSSIns ----rrierCH Highes.
e16(modulechannstics hanneAT_AUT);TAT_AUTHexMEOUT 62__lenupfine STAT_