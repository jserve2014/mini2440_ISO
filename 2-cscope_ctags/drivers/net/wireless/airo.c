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
	WepKe=====wkr/*=====rc;

	memset( &======, 0, sizeof(river = ) );
	kfree (ai->flashes c   Thide i= NULL    /* The NOP is the firs00 sep in gettingrsioncard gos.
 */
	cmd.cmded uOP; may bparm0 =  respect1ve licenses2 = 0====f (lock && down_interruptible(&is rsem))
		return ERRORrsionen issuecommand( ai, &cmd, &rsp ) != SUCCESS ) {
	in Red ofevel	upcode was d;veloped by Benjami}
	disable_MACrs.sou0)r bot/ Let's figure out if we need to use    EAUX portmin Re!test_bit(FLAG_MPI,come flags)ing po rese  CodCMD_ENABLEht
 thertions<breed@ Cop Refrceforge.nt>
r boinclueserveortions of whiich come from   c	airo_print_errr boTdev->name, "Error checkicenanuaht gran("Major  Aironet PC45000
  ) 1999aux_bap || rsp.4800 se& 0xff00or telea  Tbap_rearmisfaPC45Ccome fchiri comecontridbgriverwere receiveD founlso  ntegrates<achir} else.com>. grantegrates a and Jpport dt.  Major Cisco Aironet driverAchi Linux.ica
Sup   (=====MPI30forg driver by
  Aironet.  Major Ree  Tconfig.len == hprds w00 sis matdsRssi====
ssi_r800 s	Capabilit800 scelle00 s
	ithes
*/

#APListMajornux/modulssiounder lude <l====/SSID.h>
#ted _fs.
#incproc_f// generalnted fduradriv (me fludeify/writef whilcode<= me fCace.hRie Air'ver by om the 4800 ss granted fdin Aironet PC4500e <li.h>

#incst.h>

#incker#incls l <lininclude <linutimer <linux/sclude <linue.

    Tinux/scatterlisPC4500ome frtterliRID_RSSI,& <lin
#i,500ice de <asm/),s.h>
#include <linux/=nclude <linuerve00 sinux/ <lian Tice <linux= kmalloc(512, GFP_KERNEL)as grx/prby gramemcpyde <linux, (u8*)cludeasm/ + 2, 512);oth Skip RID length
#inbernse manual0 ude < wa.h>
#includ.h>achirico>

#incet
#includtdevi#incbit.softCap & cpu_tetdev=(8df whinux/s
#inclurmcome|= RXMODE_NORMALIZElinux/ <lincludst.h>
#the Ciscowarnronet driver for unknownver for d signal "

#in		"lf wh scah>
#inCI_ANx/ieee80211.op<asm/= adhoc ? e "aiSTA_IBSS :9, 0x#inc,ESS<linux/s
#incluauthTyp},
	AUTH_OPENY_ID 800,{ 0x14ude </scatt=NY_I_CCK<linurtione16asm/cputterlist.len) >=#incice ude <li) &&
		linutterlist.extSx/rds
z<linuxx/scatte1)I_A_ID, , Pmicsetup(ai)/scatterliserr.h>as adPC_ANY_ID, P},|bNY_IE_MIC <linspt.h>
# Reed.IC_CAP.  C, ll Rights Rs manutter* Save off    EMACpci.h>for( /kth0; i < ETH_ALEN; i++
#incnetmac[i]I_AN_ANY_ID, PCacAddr[i]ds);

====icCJavion. seeth pthdrivaic iny insmodtrae <lined_ID, Port fi_deaddobe(stlude _dev [0]pcit stic.info>ID, PCI_ANYh sta,0system.hde cci_resume(st)achiriruct ;
stati *, 8com>======i]ct pci_drivei_ID, PCI_ANYh===== {

#iid_tabl


#iNY_Inual wet basic_lude > 0pci_devi pci_dri
=====air
	.recer bove l= aiDRV_NAME,
	.id_tevel =ode cci_prob||

#in, PC !.suspend  = airopci_remove.suspend  = airo_pci_sspend  = res 0x8 com>on breakn   E NY_I.	.reci_re}
};
M ODULE_COMMITE(pci,lude _ids)ove(tic ID,     Etters);
spresentpci.hlude <sidate);ve(strx/scatterct pci_driver ai3com>.py sung pro_pci_remob9, _tcatt = strlen( New.

=achiri_ANY_In > 32>

#inCISCO_32tion CI_ANY. enable .h>

#b9, 0x5000, P_ANY_EXTx/scatte/scatterlist.h>
 New,* enable , CISCI_ANY_ID/scattedelay <linendif

/b9, 0x0CI_ANYic stove(tterlist.b <litringinux/scatterlist.lude <linux/scatterlist.h>
#include <linus <lin define WIRE lh>

en_pci_iw New d======*/
on't  pci whax/kern==de <li
=======s.h>
#include <linux/scatterlist.h>
#include <lilly d  infrontenf wh#incltheshe Cunts are<linux/scatterlisoc fileronet PC450de <mGrab    Einitial wep key,permgotta s====itA MPIauto_RxOvci.hr,
	{g */ PCI_ANYel, thawkr, 1em */

#defineLABENY_ID,NY_IDdoux/netdev",
	"ont kr.krr",
	"RefinecCrcOk",
	gthE9, 0x5000, P@hplffReserve.h>

def,
	"RxMar",
ce_i0dev)mov	NengtL("RxPlcpFormatErr"),
	IG0iss.h>will}y
  le(RxCtMaxRet!ries",	"RxWerencetry_rcRxCts"PCI_ar *LL
statinted f;
}

   I reu16ein the Develostct pc"0x50info * RefCmd *pCmanuResp *pRy Fa{
),
	tion// ImL("Rsticparanoid ab witlnd a.
 it runPlcpever!======max_trie>

#60xRetxUcP===*INnux/t pciEVSTAT) & EV_CMDf whOUTSync-MissedACKeded	"LosxUcPnc-ArlExceedPARAMr 45No"->nsesiack b9, Cts",LostArlE1Disassoced",1TsfTiming"-TsfTiming"2
	"HostTxMc"2
	"HostTxBc",
	"*/
#AND
	"HostTcmdnc-Dets",
 (TsfTiming--nsionostTxBcceededBeaconxUc"TsfTinux/ktrlinuxf (",
	IGNLABELcc"),
H)GNoCttRxBEL("HmySho//<linux/ did infnotice e Devel, try againEL("L("HmaostRxMEL("HmaRxBc"	IGNLABHo   = a>in_atomic(_ANY	IGNLABEL(D & 255TxUc"ELnux/dchee <l(reasticse),
	
	"HostTxBc= -1inux/nem the Ciscobudriver driver foABEL(Max iming"e	"Losed whencon",
",
	e Develor MPI
	"HostTxBcceeds",
Hmac &easonOut_BUSYmacRxauth",
	"LosxMc"-TsfTimLEAREL("Hma,
	"Nrsiont.info>.>
#inc
 ce Mae DevelnStaplell",
rsPr->tterlist.GNLABEL("ReHmacUS====",
	earspivethTimeoL("ReRESP),
	I"),
tus5"fineIBEL("HmStatusoacRxUus6"),
	Ica
 BEL("ReasonStatxFailcTxBcasonStatus6")jtg",
.hp!=0cRxDEL("HmacR	"Tx   ESOFTRESETNLAB	"Rrt fMismatcync-TsAuthRejects1")cmd:%xr",
	"R6"),
	I)sonStLA1EL("Rea2:%x"("ReaEL("HmacR, L("ReasonStaeasonStaNLABs",
	"NoCts1"ReasoReasonStac",
	 Maclear stuckus6"),
3"busy);
snecessaryc-TsfTiming"L("ReTable")sideTevelts",
	"Nux/neL("ReasonStatus1,
	IGNLA("ReasonStatus6"),}"Reasacci_dledge pr	IGNLAas addE",
	"R/icenonE "aL("ReasonStatus1"),
	IGArlExDeacktRxUc"tatuHmacTxM for cteness.  Jeat sttartout"hange data. Rts"ch
	"Eshould
 *  Peont	"De   EBAP0 oryMis1 
	"Maes",
Lockc-HostBerAP"heWeppeostTttercalling!,
	"Cts",
	",
	BellY_ID, ollostTxByMulCAT
#defiHmacbitopHmacTffset,};
#is1"),
	IG)
{======de <Frag= 5ensihyEle),
	IGNLABE3tReq"tSync-TsfTiSELECT0+s1"),
	I,fies
	"HostTxBc",
	"OFFSEl.  Peset lichis lius19RxUEL("1t in
 nux/Status19"4,
	"RxMan"SyncBe  Pea*stam  (ns",
	lude <ljtBAPeasonStaL
	"Defers.h>chxUcPssids[thcom>.
is,
	"ce iDupare=====i, bwitard_	"RxaEL("D, PCoseobe(stMan",tatic i--ci_remov Cisinues  = eck v5cludelude <linux/t aity Bepci_devi/* inv "aiity suchhis lincryptom the Ciscoatch",
	"AuthRejecveBAP et.  M%x %ds",
	"rantefr, these vari MPIicaxSinCoper's manlude  can ards that.
		DONE) {rsEnsuc"),
e out the wstTxBytesk versi),
	!iscard"),
i Resi_devi"ReasonStatus11"),
	IGNLABEL("Reasep /*_ID, *
    *too mcoderec-Hos\nor MPIut the wep mode */rge// --acT,
	IGmeeded itMates[8]Man",
ei_dev  grac inond be a comma NY_ID_cithe.  eason . */

static inta seFramted
	tatic ipci_s, s}HostTxHxNotWep*/;
pped For
statrBEL(fndeet
 . ove(staux func
#incGNLAtheate;follow"RxPoCopconceptsMan" document0 */nivacydY_ID#incs guide.  Irelegot1 wimt.  Maa p For givenElapmyRIPTt.influde <Cts",
	Hmacice f RUN_AT
#eness. D/GPL"(x) (jiffpagL("Reic chx))this listHmac*ON_DevelHmacD, }t the rates
   cAUXPAGE,ICE("
	"HostTxBc",
	"AUXOFFstaten	;
m"ReasonStatus19AUXDATA(irqellet, unde, 0), ine <l_pa&g",
ic int  oltic != 4ries  0);
moduram_arrarm = 0644;LL
stat NULL,enjamirequiresLE_DEth afndeBSD/GP)n 2ice har *)-1 

/* ION("SupportPL");
MODULE_SUPPORTED_r",
	"R*pu16Dst("Rea
/* fytelen


th theto fi340/350")len;tion o NULiverunthis lin assoc NULL,pnux/worH!
  x/scatteuEitheed loux.
ard_r *sspin_ns o_irqReas come ice ns o,rsionws7"),ag800,s (up to 8)BWSe(strut.
		ratay(rateeverr aommadex 02and index 1.",t.
	, ual BSD/GP,
	"DE("Athis list&co 3OWNut ai = (through+1)>>1xUcP MPI(i=iver<iver wpaces) lis/

# the	=====
	"Tlen>>1) < (iver -i) ? <asith so:eay(rer bu#include !do8bitIO NLABEinsw
	.sus drivbase_addr+
mod be a commatic
.
		 (pcikeep+i,itchcang"RxPM	"Hoirinsbismissied.", NULL, 0);
mod(maxeards tULE_PARint,MODULE_Pt===== << 1ESC(mai +=ts arer olcomeieems t	"TxSho  UniARM_D12kb and Je NUL, 4rol try d
#incl==eys tunom Javilesstor valiver w2,
	"Tc"),
using",
	"HostTxBytes",
rate, int, 0);
moduhe crcErrwep \
encryption.  UniARM
   e.

port star"If non-ze to be lim===== int,ODUL loop

/* fswitch \
====e raept, atio be li int,swin us+ 1tatu(~1x/io/ roundxByteonux/itaram(sidStatkersiatrsionswn ushe maximum speed that the card can dption.  ,to be li>>512kbsxencryp, "the maximum spssionid, "Thestatic indspeed that the cp12kbs
	IGNLABETxByteBenjami non-zero, the card will start in adhoc mode.");
fnde whatPL");
MODULE_SUPPORTED_cTxMt_PARM_DESC(prSrct.info>.40);
MODULE_PARM_DE  infsedUsgid, "Them spet the /proc filncluuidart in adhoc mode.");
. \
Ol "The perm
MODUuout that the /proc files will belong to."t, ic charcodenet.c_perm, "Thegpermissioneleats of the files in /prIN450
MODUgi(proc_gi OUT4b9, 0x0e.  Bll belMacCtom speedMODULE_PARMlinux/saavie
#incL");
MODULE_SUPPORTED_DEVIies+(500,acan",
{
	
stacmd/iopo MPIin t),
	IostTTic intNe;
strspe ERRAT
#defiPCMCIAe GP_PACKET -Hmacrr",
	"R 2"),
	IGs manur 4b9, 0x0ce iSC(md.  Permis   inthe icenses),
	inux/schedh>

#inrsionostSyde <linmanM_DEwa;
MODULExMan",
	"0.h>
#inclPlcpFormae nee(ourr;
static @h7F.hp.SET	0x thisL
stat(   in lic8) +EP	0x0NLAB,fineFFxRetRefx0006
#0rpc_rate NotxMc"hatn",
.
		uxRetPostTxostTx vux/ps",
CoCo_cstransm/moveoMismn",
mustound ====c .adhoc mode.");
ux/scatterli/* = 0 */;

/* Return codes */
#devce i*pBuf \
thee driver ver byation orr",
	"Ric char f \
a grannted fys1"),
	ver bux/ne dri andle.e CMD_releascome fromf whs2"),
	IGNLABEL("Roth =rupt.h>
#DULE_D  Apci, card_ietrySh);
MODUL
	-2\
theComve(sBLEKEMA001);
MODULEMAC_DISo us	002a);
MOrsp CMD_DELTLV	#def <linux/s
#incl_desc.ridSPNODEo limi= ne CMD_DELsionPx0030
S
#def30
E_PARMRIDSIZEMD_SETCW	0x0031    
#define Ct.CMD_A, siETCW	0x0031    
#define Chost0);
m*port *co AACn todefinPMD_Ssyrigh co/ACMD__LOSE_SYNC
#defi3);
MODcpy_toioiver pdev);x0030
ithe_moduoffdo \
alue o0x0031    
#define fine MAC_R I rea	"NoCts"re of tcom>.
does...ces) enessault) m
#defi5);
MODULE,
	IXKEMACts",EMASK	0rates[8]Grcess.  do somKAROUNETCW	0x00 to usevirtual_MD_DELTLVR_ the dr
gotocs ian a3 card  <asNLABE/rds to <linux/sthat th/*  Refies+(_LISTBSS	0))!=rr"),
	"Rxa5ic char *ssids[3 BenjPlcpFormaic char *ssids[3NVFID 0xAVECic char e    = am(auto_wepGE
#de5
#0,defi9as gran
    p.com>. Benja PC4500efine  Benjiro.B
#deB);
MODULE ERROR_TWRpermy Fa   E	0x0PCI_ANYfielt pc/proc fild Je,D 0x02IDTX 0xPCI_ANY#define trucemain"RxPpdUseof	0x0CcFraPARMmi,
	"x/ifint)_ID, PC_ANY_*driver *)ss. )) - 201D_PUTTL ef C<= 203e
#de defi maximu onILLC\
switchne ERRUGH!
  %x hasKEMAtterliof %e NOefine Cindesh   <RATES efineies+(2
#defi grado_TESTACT
#de0032    
# ERRORTAGNFNB
#d12
#dd index ODE 0RRORMOdeefau0x20	0x0DES	0x01NAV 0x2ne CMD( ExtROR_efine +ndexne C_BADLEN }

#de:PlcpFormaUTOWAKE 0x8E
#definex0006
#e CMG
#defi8MD_SETCW	0x00SETe "a
#defi9MD_SETCW	0x00ALLOCATETine E00ine CMTCW	0make srmist and intdule_SCRsocRea 0x02
#dene Chappe8A
#d_SETCW	0x00DEne PARAM what02
#c#define ENOP	0x02
1ER 0xe PARt, int, roc/[MD_WORMD 0x0D0x0e
#CMD_SETCW	0x00ne PARAMht
 0UTOWA

FSET0incluKEMAR0x8C
#define  see this list (u16R_AUR_RXM0OR_SSIDTCW	0x003CIint x02
2
#define E0x36
#dAU 0x02
2VECFG	0 OFFSET0 0x1BUFne EVS* Registers */GETTLVne EVSAND 0x00
#defiPUfine SWSCK 0x34
#define SWon.  CDclude <linux/ABELe(st
WEP_TEMPMAGINY_Idefin_MACADDR 0x84
#define ERROR_RATES 0%s:,
	.rn Reed"L
stt thisd (rid=%04x)CAN Ext__ to __
	"Prio_peefine CMP0 0x0a
#dLine SWS1 bAT 0x30
#defiFINDNEXfine SWS1 c
#D_SETCW	0x0031    
#define CRegisters */
#dCWdefine103e
048
/* Size of t*MPLFIteness. 03ine E048
/* Size of tPHYRE0x8F

3e
#CMD_SETCW	0x00LISWRITERIDx01AVECFG	0ters */
AVECF0x8F
1/* Registers */ to use
 for x
#define OFFSETackets *for xfine DATA0 0x36USE31    
#extetion< 4nclution s2047e ERROR_MACADDR 0x84
#define ERROR_R ne FIlen=/* I criptors sco 3OR_I8
#def-s */
define ERROaOR_ILLC(har *s)ne CMD_DEL ERRORILLFMine E
#defineion a
#defiERRO4
#d
#defi1 2048
/* SiTLV	 to usRing w201ptALLOCLengus9"),
	I.AGIC_PKT	0 BAP_ERefin4002048
/* Si8
#d= 0 ERRO0W9, 0x5def   <fa#defft aiesne PROMISC/c",
	TX8
#define EV_LINK 0x80
#define EV_AWCmmemory EV_UCPYne EV_x100
d.  Pere EV_Uove(sXEXC 0x@use e frodefix100
>

#in ERRORQUALIF
#dene RXefine ERROnt pr- int, 0that thso BAP1 2 /cant of tDIVER 0xfpsedmStatge_V
#in0x0 EV_10
#define ELARERROR_INVE_INTS ( EV_EVT0 0x1 0RE_INTS ( EV_CACK 0xSrity07ORE_INTS ( EV_C_TAGNFND 0x12
#define ERROR_XC| EV_nowifautoCHECK_UNKNOWN_INTS
#C 0x10
#define EV 0x0B_BADL0DKNOWN)
#else
#ORE_INTS (~STATUS_INTine ERROR_PSMODE 0x89
#define ERROR_10
#define E_n",
 in t3
#define _APefin ExtD
the au IDs"Assoi\
fore RID_CAPABIine IRE_INTS ( EV_CMD | EV_UNKN0x100|
#define RI8     0xFF01
#de_AUTOWAK EV_80032    
 (~STATUS EV_CMEAPdefiF

A   =iver a FIDe cardefied#defi0x02
#dec",
	p",
	"ostTWx000ly3
#dTHERce idefinow048
/* Size TXQ 0x02
#de_r   =a
MODUCMD_S0x20
 biter by/0 0x18
PayloadG 0xFFrawFSET0 he mumbe3
#defopfine	"Tx" 0x2a
#defie SWS2 0x23
#detxFUTOWAKE 0x8EtxCCiscol#defor receiving pTe PARAM0003e
LOSE_SYNC	020 /*readoHOSTSLEIBAP	0x0022
#define CMD_PCIAUX	0x/interrupt.h>
# Rere what this does... */
#def0D
#define ERROR_STAT  ly*/
MMANDe RID_
#defind_STATUWAKEMAd errors */Fsion3C
#LLOC 0x0y*/
5efine BAP1.h>
RID_CAP54    t wa	"RxAcC BAPxFF1* Ren.  Ut/indicnux/s
	 * Ice ikes memove(OR_Dnervouopyradefii0);
n jRANSsi_bap inded  2mb,ATS16 irq[n pracCts",itKNOWN5NFIGs likON 0urstatis03
#d gra(no B
static int ("HmacTxMc"),OWN56Man",
	"&& --ATS  BAP1 2 !6A
#dF5NKNOWN)
#eWN56  0xFF56653
#de56RxAcReasMITID, xFF71y*/
d faxRendsync-TsTine );
MODULE_	IGNLABEL("TXxFF70F Not su("ReasonStatus1"),
	IATSDELr *sstabrice@ARDne Cpretty coolABELID 0xFconvertopyrige */
#MODy*/
16TACLEORYCo 802.11fineET0 no#defi
#defiddule_ ileaCopyrig1
#dISTNEXwe6 cmd;does1NKNOWE	0xAT
#damx0009
PverID_BS;
	u16c in 0xFF7
#deID 0xFF7h;
} t sta3
#definhesSERN e ERoll",
	{
/
#deno1	u16 s Cmd;
"RxPoll"fiine R */
Com
#de03
#APUSERN see this list TXCTL_TXOK |  up doinEXine cens6 pa_11 BAPl rid acETH* NoTll rid acNORELEASEIN4500.  I ERROR_ig-is lan guystions.
 * sogfine id acso all r
	"RcnSta3 */

susersionne Rab.h>
XXX====routinc",
	LITIES3
#defheck ISTAT PPAS

/*D_APINFO     0xFF01EST_RID 0xFF70
#defxencryp#inc0xFF71
#de&This strufine MAC_This stru)ne RID_CO
#define Aironet.  MajS ( EV_CMSTAT 0xenjamiIfounh>
#i6
#de" MACethe p   E.TIONS    M71
#def1OTESeHowxRet,DULAays  PeinID_ACaess opersfibitIO"RxPRIDstSyxFF7ATUS_Iess ar },
 bap DULAM1e IGNORE_I
	u8 ss 2mbns oINFO8s ar
#defe BAP1PAM2 0x0048
/* Size oDEAIONSF71
#ineer_DeveloRID 0xFF70ACTUALCOATS 
#def20 , e",
	"pPeveloFSET0UTOWAKEpine RILdriveRID 0xFF70
#defin22_ACCEe ic    HOne C__ ((ROR_e EV_e;
	MICBuffer pMi granY 0x4>= 16NORE_INTSVECFG	proc/ st * 2s11"),
	easonStatribut6
#dve(stru_EXTuSOR_Sbe i	u1#defVV_AWffff
0023
#define CMD_AOD_D-FAULTr.h>

#_lNORE_INTx34
#define SWSEVICE_To uslude <linux/efinve(s_ANYtatsat tc fdefin
#defi(ntohs 0x8CbM_DES)
typedef[6]3
#defx888EV	0x002NLABencaps_ANY71
#d(ef stHfine op>

#; ,&====I_ANY_ A vamppor) NUut the wep mode *#incl====b9, 0x0====define PY		ld b_Met.  UTOWnux/s* opera's m16(3)ul#incl[len-12]T_RES*/
#defineDE_CFG_ME    OR6 rsdst/src/DE_CFG_PARM_D*/pmode; /6(1)
ct ======R36D_APINFO     0xFF01
 Aironet PC4500];

se hardw;
} duleessF 0xre#def=====ct Ss0x8heck Iff)03
#defiid;
sACLEw1<<8ure -neubtROR_E_AIR12to besDid[32]   p*reados/
#deid {DE_CFG_Rid see this list * H +AP_INTE the=======k=======_AP_INTERFfine MAC__AP_INTERF),ST 0x8D
__le16 kindex;0x8C
#defi0IRONET;

type1d));

typkptio(packid;
s)_LAR/
#dee ap interface0x8C
#defRID_UOAP_INTEne RID_CONCd));

typine 1<<12E_ET( NLABEL +definna alignppor )T_RID 0xFF70CON//1
#defable e BeacUS"AssocTiNABLE	0x0 dine CMD_DELTLV	Inteeriode define CMD_LIST#defMITASSWTAGNFNFF24
STAT 0x3D 0xFF70
	IG_HSepKey_ECHF03
#defin.h>
#ETRIESHST 0xFF51
#define 29
#definxFF70
#define5	__le1 Aironet PC4500",
	"HostTxBytes",
Cts",
	ons {==========en11====== sids[33];
} __at */

se__ ((0xFF1d)M_DE) /* rx payloadModfce "aiCFG_ons M0);
P_INTERFort and broaulhdT		/03_LARN.h>
8 tail[(30-10SETW2 + 6ci_s{[de */ci_s6}ne MODpadd"RxPof hea Bento fulSLISze +imer.hgap_LEA(6
#dene MODEnfo>.e_LINK 0x  ulticasts e extensi;03
#def
 * TXALLOCFID mode; ;
	3E_ETH =IRONET__		/*AIRON RID_U__ (E((255)
ine enable LE_802_3CCK#define MODE_LIRONET_EXDE_AP_Oisables 802.3 2)ADDR cpu_to_lSr.h>

E_AP_RPTpFramo_le16(1<+lectors */.h>

u_tox_AIRONET_EX802_E_EDULANET_D_STd));

tyMan",
	 m<<8) /* rx payloads c
   #defineALIZED_RSLLCo_le16(1<<9) MODE_LEE	0x002b
_cessleft aatice16 fragThresh;
	AIROu_toEXTEND#define MODE_LEA9E_ETHER_pci_03
#defyle mo__le16 shoPABILERFACEtsThres;
	u8 ma-ADER cp_pci_aD literfaEL("efine Bntes[8];
	__le16 shoNTENNA_ALIG16(0<<8) /* rx payloads conve14_le16 fragThresh;
	__le16 rtsThres;
#define MODE_LEAF_NOpu_to_le16(EADER cto_lLLCe16 fragThresh;
	_EAF_NO *)(le_pETWA(255)
#- 10le l38 -ved1ODE_A	/*-e SCANMODE Scanning/e NO_iatinsThres;
	u /* in .h>

anM)
#d03
#definSCRefr16 fragThresh;
	CF_POLLo us#define MODE_LEAF4yLimit;
	__lCF poll_pci_16 fragThresh;
	d[32define MODE_LEAF5yLimit;
	__ld[32se m=======h>

#/iopoer for  >

#i16 fragThreude "aiBC_MC_ADD in hres;
	u8 o_le16(1<<8nListenTimt;
	__le16 joinNe/*BAP1 ve(strushortRncludsULE_Pmtus1")IRXMODa - Jemloadhould n I wotWeMismTA_IB!  Feel e <l
ton",
	n;
	_up!
onit!!__lestsragThre PCI_0x21
#EXTostTf (no*_UNENKBeacked));__usx/pcble16 ED cpu__OFFSET ISED cpu_loff_t *rm = 064416 joinNetx102IRONET_      onst LOWRYPTECRYPTE;
	__l;
	_OR_RTimeout;
200) __le16 pasto_le16(onTimeout    =======enseif12kb1E_ETHignor PCI_ULE_eCTIVablerin0x50*0x002,canInterval;
	__le1SC(le16 prine Mhablecasts_open03
#definddr[ETH_REFRESH__le16 joinNetxFFFF)eaconLosev);ryShd1a[1delta#define SCANMODE Power sint ape
#inclue SCANMODE   __le16 ppAM c intMu1define POWERSAVE_CAM cpu_to_le16(0)
#define POWERSAVE_PSP cpu_to_tterdefine POWERSAVE_CAM cpu_to_le16(0)
#define POWERSAVE_PSP cpu_to_ 0);
mdefine POWERSAVE_CAM cpu_to_le16(0)
#define POWERSAVE_PSP cpu_to_BSsp2;=======fastduleenDan sconfig ite6 powerSa2[2define POWERSAVE_Ap/Ibkets anefine POWERSAVE_CAM cpu_to_le16(0)
#define POWERSAVE_PSP cpu_to_wepkey16 hopPeriodconfig itechannelSetgeDistance;
	__le   __le1neScananInterval;	__linux/sADDRto_le16E cpu_to_ doeiverowner		= THIS_on.  U  = 8A
#DIOT3
#definA__ledefioinNetTimle16 radioTypin__le16nce

inNetTimrval;
}WERSAVE_Radioghesnd us(0)
#define POWERSAVE_PSP cTypcpu_to_le16RAle16YPule_packed));

typinNetTimeout;
	_er;
#deficess s cp02.3 header on rsiThresholLEGACYefine RXMODE_FULpackrxD====sit/
	_u8 tREAMBLE_A head__le16 txPower;
#define TXPOWER_DEFAULT 0
	__le16 rssiThreshold;
#defi headerI_DEFAULT 0
        __le16 modulation;
#define PREAMBLE_AUTO cpu_to_lenfig it__le16 txPower;
#define TXPOWER_DEFAULT 0
	__le16 rss;
	__ULT 0
	__;
	__ rssiThreshold;
#=========I_DEFAULT 0
        __le16 modulation;
#define PREAMBLE_AUTO cpu_to_lesine 
	u8 t Eationary;
efine POWERSAVu8 nodeName[16#def6 linkLorlTh_resoldgeDistancearlDec*/
	__le1*/
	u8 magitI_DEFAULT 0
        __le16 modulation;
#define PREAMBLE_AUTO cpu_to_le*/
	__le1icAction;
#define MAGIC_ACTION_STSCHG 1
#define MAGIC_ACTION_RESUME 2
#define MAGIC_IGNOR*/
	__le16 I_DEFAULT 0
        __le16 modulation;
#define PREAMBLE_AUTO cpu_to_lefine MAGIicAction;
#define MAGIC_ACTION_STSCHG 1
#define MAGIC_ACTION_RESUME 2
#define MAGIC_IGNORfine MAGIhoI_DEFAULT 0
        __le16 modulation;
#define PREAMBLE_AUTO cpu_to_le1ridgeDisicAction;
#define MAGIC_ACTION_STSCHG 1
#define MAGIC_ACTION_RESUME 2
#define MAGIC_IGNORbridgeDistan_DEFAULT 0
        __le16 modulatioanInter  __ldir_entes[*"TxMu0644Ae SCA 2
#deficcuatfine y#defagThre_le16 lt;
	__FIfor * wired));
 064efine MAGI;
	__vExtenodulattlizedSard_ied));
w	__le16 nefine(*on  __le)efine------ Powee operation ---);16 modulatio
	"RxWs: _  __lle16 
#define nett stReas*devdo \
encryAT
#define RUN_AT(xprivinux/nfine MAGICccumXMODedArlonLostiAULTFnt, 0 u8 n the ===== sdirecto*/
	/rcentPCI_ANois->  __ler fo driver ft) meMaxPe
	u8 ;te */
= crireloiL("Hm/iopoeercent in l_LARH_ACTUdencrypS_IFDIR|"TxMufermn	u8  dbm licefine MAGIC_MC_ADDRpnt in  lic in lmR_LL 0x0Bfailsfine  assocStatus;
->uAULT   __luUTOWAx0000S 2048
/* SiSTgT_NOCARRIEg/* Use	"Hosteness.  astsDcpu_    _tus;
inu#defch	__le	ine ("lude VER 0for destatusREG | (S_IRUGO&  __l  __)ITRATESACKETS 0
#define EL(" __le16 prle16;
	_,Bm ,
	"C_ADDR===== STAT_GOTCAT_SaveMo] cpu_ed1aHENTICATIIERSET 1RSET 1AT_GOTTICATED 61
#ne CMD_DELICATIWRONGe STAT 62
#definBADCHANNEL 2NKNOWN)
#eSTAB5
#defi  2048
/* SiNG 70
#dPRIV16 m3T_DEASSOCIATET_APFECT0 4ne STAT_ASSOCEJECe16 5ne STAT_ASSOCTI PCIENTICATING 60
ne CMD_DEL_LEAPING 9ED 6MEOUT 62
#defin PCITIMEOUT 6F03
#definNOTAIRSuSOCI 90
#d7ne STAT_ASSOCTIDEASMPLETEuED 7efine STAT_LEAPTackeDOUT 92
7define STAT_LEANOTortRAP 7efine BAP1NOTAIIRONETIATED 80
#define STAT_LEAPING 90
#defUTOWAe__ ((pAPFAILED 91
#define STAT_LEAPTIMEDOUT 92
#define STAT_L a lotOCIATING 70
#define STAT_DEASSO a lotD 71
#define STAT_OUT 72
#dfine STAT_NOTAIROAP 73
#define Sonst st];
IATED 80
#define STAT_LEAPING 90

#incl) /* ignore multicast and bmoduleons  prodNum;N cpu_to_ prodNum;
EV_TtCIATING 70
#define STAT_DEASSOCSID_to_le16(2) /* ignore multicast and bde <linuxyons de <linu2"),
	IGIATED 80
#define STAT_LEAPING 90
#UTOWAKcked));

typedef struct APListRid APListRid;
struct APListRid * Not sCIATING 70
#define STAT_DEASSO 0);
moryAddr[ETH_ALEN];
	char aironetAddr[ETH_ALEN];
	__le16 rado_le16(1<<IATED 80
#define STAT_LEAPING 90
ap    
str	har *is put edeason[8Cap;
	__lPREAMBLE_AUTO har *_to_le16(0y*/
	u8 EN];
xCAM cLf whVer;
	_r apName*/
	u8 oryAddr[ETH_ALEN];
	char aironetAddr[ETH_ALEN];
	__le16 rad_ACTIONmagiIATED 80
#define STAT_LEAPING 90
bssV_RXfreeed));

/* bootBd ofVered));

/* , 0);
mdHardgeDistance, },ofthe lab__attribute__ ((packed));

/* OUTOWAKoryAddr[ETH_ALEN];
	char aironetAddr[ETH_ALEN];
	__le16 radine MAGIC
	__le16 country;
	char callid[ETH	__le1tCap;
	__le16 bootBlockVer;
	__le16 requiredHard;
	_sionREADCF
; /*_to_le :cy hmovCTION_SisdBm;O/;

SPY		TAT_NOTAIROAP 73
#);le16(ca
 ;
	chEFAULT 0
    _DS 2 /*arded[12equL("ReT_APR mode__le16 bLostdefineMA 4 /* Proprietarfacto, 0)d in old cards (2500) *st sIO_TMA 4 /* ProprietarDR cpu_tTH_ALEN]; /* Mac address o
#incle STAT_APREJEC;ux/s8 bsgnore mtyRid;
st
  uM"Duapayloa of12tes[T 0x30
#d
#deSDE_MASKFAULE_LEAo_le16(1<<8defi PCI__le16 joDE_LEAFto_le16(1<<4)
UT 72
#70
#dBIefine MODE_LEA4to_le16(1<<4)
SHe16 raORTHDR cpu_to_le16(1<dBm; /* Highest nTIONcarrier[4]OTAIE_LEA
stat-ENOMEM02.3 he16 refreseaxFF6filu8 noiseMaxMaxPe dED 5Status;secA/PC*/
 assocSt /* Highest u8 hopACKEs in /0
#define STAT_GOTCer fPTIMEe16 radine CHDR cpu_to_le16(1<<5)
   cH_ALEN]; /* Mac addrTH
	__le16 joinNe1<<5)ine RAD16 n 2 mw
sta>= 5.30.17tern  BSSrodNum;Extra ====
 AUTtribute__ ((packed));

typedef structDR cpu_t_attribute__ ((packed));

typedef struct sid[E_attribute__ ((packed));

typedef struct  radio ListRid bss;
  struct list_head list;
}  Direct sListRid bss;
  struct list_head list;
}  multicasistRid bss;
  struct list_head list;
}dBm; /* Highest n */
  struct ; hopping r0x8F
PRIVAW;
}  pawa, NUne N02)
#defiOPE6 atim5
#f whtion.fficientl 0x1a_PRIVA6 rs;
	__le16 spend(stnux/se ER*
 *=====));

ICRidtoC8A
#dx2CRid;
[16];
	__le16load====sfrom- Airdefickedu8  multiih thegnore multicasR cpuval;====She csi Foryra;
} ___le16ax8_ECHle16 lram(cpu_    Bfs19"ithis CMithD_CAP,16 rsSIG_Pre m alizedMsnaper;
nck In */;Cap;  __{. modulwon't;
	_e   u8 
#defin));
cpu_t18"),TxAels",
co eSA/p io[46(1<<1)d));

/CAP_SHetrypplyhopSetsedS modupTimeout;
	__le16 of2)
#defiInterval;
	__le16 offlDuration;
	__le16 linkLosselay;
	__le16 maxeaconLostTime;rievelnalQuaEN];ine TXC*u8 ho= multCKETivTAT_DEASr[ETH_AL!ercentne TXCTCON_HST 0xF-EINVAtchio00GNORE_i#def];
} _re measo;
	(le16 liNET_EXT_to_le16(1 par3 0xFF_le16 lHERNET pDevEx
   S 0xF} llc;
	;
	__l	__be32 miine LC (1<< com{
s----a_le16(2) /* ignore multicast and b MOD8 da[const stCap;acketer completf	__l
#define va str=======ofTL_NORELED rxDive===========inkLoss---*/
	__le16 bmaytes",
fTim_le1n;
 D_A /* on po doestTime;wirelere=====if tx TAT_Nin
  iseent rt if tx *e leaf no up docess3 (0<<(255)
802reambleeaces) g 2.5.X - Jean NLABROMA<nOutsR
#ifdef SIOCDEVPIVAThe BS>
#decentude 5) /* oCON_HST 0xFF======*dnary;EN];
} IROOLDIDIF -IROM#def-*/
TypeoporIOCDEVUT 72TEternSTPRIV copy.11 man;
	(fWFIRSIWFIRST + */
ssoc  __le16 ACON_HST 0xF-e TXPobeE[16];
#deON cis liOCIWFTPRIV */
#defil sermiswWFIRS16 le +#ifdef f0x02
#d_TXEX (1=ong. Wh=*/
cpu_to_lIoctlsh!_to_le16RID_les 802.3 header --------- Power save operation -------<<_NODEWarning : SIOCDEn II * assocStatus;
inutEL 25
only*PDE(0x002cked8 hopSet;
    u8 hopPa = dp->lart tony
  ch hopIndex;
    u8=ED 5->ml
	u8vne Rin.h>
#incluneux/bitin	FAULT UTOWonLostT__le16_OPT 0x5 \
 will sc int png 2.5.X - Jean II/ethzxFF18cb9, 0x0* WNFIGng :WFIRST)ux/scatterlist.=x/skbuff.hr fETH_ALEy_le1p	y an 
#G		mit qfersEnUSEDa lo 2.5.X
#defNCRYI *fine RfterET (0<<4)d
#define Adef 8ux/scatterli  SIOCDGDE_LOC 0xde <l
ss:  The  - Jean IIus19"_le16	I pci #inciED_Ras 0 */n/
#de_to_le, &iit srds m, 0x8D
t.h>
#iinux/scatter/* SIOCude <liop16(1AN PCI_
	 ERROR_BINTEOGSTAT		8
#d},of sTEN 0heck _le1 CiscfAP'ine RI];
} R cpo_le16 %sfine AIID			14
#\n"0<<8)ROR */
	s;
	u1fin 0x50& 1 ? "CFG ": "* SIOCDRID		15
#define/* LeavS2VERSACT		17e16(1Leint gap6(4)40proc_pn;

tOGVLter SYNOGNOTASD32e ERRfutusedinux/ROGFLAGSIR2PCAP LNK           	AIROGSTATSD32 + 40
#defin4SIOCDPEAP           	AIROGSTATSD32 + 40
#defin8 AIROUT 7           	AIROGSTATSD32 + 40
#definOP AIROKEY 0xFF71
   	ne AIR        +
#define STA AIROWEOPDE_LT	e AIRPVne RI  e_paROPSIDS		AIROPOPAPLIefineSTATf speF03
#deafterET (0<<4)Unit" Ioc: %x\ns[]  "Sinux/wSt<asm/i: %yloa
	"Hpci_utsiQua   the	AIROPSTCLRSID: %-.*ked))STCLAPPMACW16PKEYIROPMFreq   + 1
#defiBitRat+ 1 dmb  + 1
#deDriMOD_Vopyrigcomm  + APLIS in lEAPPWDManussidurerAIROPWFirne Erx	AIROP4
#dPWD#defin_le16(typROPW	/*C=====F   x\nH8uct ifEAPUSR     	IROPSTCLRoftAIROPSIDS		AIROFL__le1 efinSubvPWEPKEYNV  +HRST	 Boot b
typeSHGCHR  GCHRt, it ocommandsGMICID		15LIST   + 1OPWEPKEYNV  +HSTFer boe AIROn",
	h;
	_R		AIR  __le16T    + 1
#deAPLIST   + 1	/* Flinux/PMACOFFSHSTFL   + 1
#define AIROFLPU   +NED_R		AIPLIST   + 1	/* ARPPASSW   + 1
AapNsIGNL hopHR   + 1
#define AIROFDISABelSHSTFL   + 1
#define AIROFLPUT68
#ntXmVIROPM) / 2LPUTBEYNV  +LPUTB* Not suproABLE_8FLAS* Not sumanton. do
	uncarded WhaV16 probHSIZE	32768
* Not surR   0x48SHSTFL   + 1
#defiI_ANY_ID,======ned shortHP   + 1
* Not su<8) ed[1	// d-psed
}/
#dem inOGSTux/fto_le16
	__lewMBLE_on[] = "2.1";
Sub#endif /* CISCO_EXT */

#defi
  u8ns o "SEcked     		s etheCOTH_AMODE      IROPMACM   doesn't wCRid Mo 1). "SET" OGSTAs aT		8
 rootd ac*/;

; /*
    u8 fillen*; /*CWERSAVE_PSP cpu_to_le16E cpu_to_le16POWer AVE_CAMine CAP
#defioperation ---------fillSS cblyto me com&F void aketLOC 0xefin6
#  2312e multicast anr save __le1nore#incLISTF
#deLAB_OFFSET 0x8REAC;ersEnpkts droOffsedu#defiincor min     comparrrds (/
	__le16 powerSaveModefine POWERSAVE_CAM cpu_to_le16(0)
#define POWe CMDn ReMICed;IROGVLIST   // pkts dropped dn  AIeingd
	uAP serxMICPlummngSeque//* rx paylit;
	__d or to e number vSherHead;

#define TXCTttern	0x0cpu_to_i// Son. er woppFIRSTPRIV
#defd acis usu
	"Ssareshblem.ist of speci
#ifdef SIAIROIOCTL	EVPRIVATE
#UT 7al()
	int posiDIFCne AIRition1
#de16(1Ie STA#defOGSTut;
	__Fi, jUTOWAKE32 *v
#deilcasts ic cnalStreit seAIRO 		0ic cETH_ALEN];t
#def()
	int posGRid e AIROGVLIST		3  A LO");
MODULEne AIROGET		2ic cSt pciefine stemmh32_contextON		A		3   + 1
// dule6(4)tTime;ie;
	ce AIROGFLAGStextDRVNAM		4096  NOT		3 emmh32_contextEHTENC		5ic c	u32		 window;    // WEPKTMP		Ssid;
sd;	    // FlNV	ndif

/* R AIROPAs Ex_INV0x8D
ge,permpIROFLSHSTFL   e th{ROFLSHSTFSTAjnore  uct ptENCRcastsLabelE_PA!=MOK c_CM-1efini*4<it sSGLEN0
#define!text
	miccntx ) R);

 064 thNLABj+MSGLEN_  multicast and+16>ACTOe ERROR_MACADDR 0xD_CCK  if co	IGNLABEL("Reaeave ga"Potecontr {
	t ----r    MSGLEN_ion flow aF75
ed!ic int  if iTPRI
	"Nj+=FF03
#definTx se ReeS+je EV_AW%uemmhn

typedef stru

typepFraOR_BINTEic cpu_tdefine C_ADD*4he B
  BSSListRidExtrs */

dis 1;
	dmaodule_t  "Goncor    SEMM_le16  mac3
#defineMMH32_32AT_AUTHENTTx sine _MTU;
	dg;
   c_u16( ignoreDEVPRIVAodul*sedUste__ ((rom te num_OPTimit. messagNORMALIZEfer ca odulu#def0; 15ion   fre;
} ANY_	// L[alid: ]he B'0'ANY_ID	_t efinodule;
}<= '9'; (alid: )}<4) _moRegisters */
Len oi*= tribu0gned c+=;
	uost receive -xFid56
#de_ECH4!#defin len;
  iRESP22	// Sy		// Lenumber violati"TxMukets anFeScan
typedeft;
    u8 hopPatternTION_---------w_cryp PC4UN_AT(UN_A

/* FORzwrqdo \
encryped));
extrtextmber violat(1<<4 seens */ngnseTi
#define RESP2 0x0e
#");
M( in lhat ROR_BINTEDRV_NAME,
	. AIRORtterl"aiUALIe ER0 ca ransmit def strucRFMONce number vioefine_ALEN];
	_;
s4tion *uct {
	u32   s in toped byPRIVnux/su32	2_MSGLEN_MAX)+3)>>2];
	u64 inal()
	int - Jean II *cum;	// accumulated mic, reduced to u32 in final()
	int position;	// current position (byte offseTS		essage
	union {eceive
UcPar engro ! tmp[4V range, Immh32_cle16(2/s a lot of thesefine 
#de u32 in final
#include <linuwi
lder*/=MAX MICIWFIRST;
	"HostT ffseupport */***IROPA_resCts",
	Pm__INTS
#d!strncmpe
				ONIROPAPL", 6 CF  IGNx0ffset+= 6 EV_U EV_, 0)_host_adpeai MODE_Cchar __iomem *c intclude <linux/wir; DRV_NAME,
	. AIROR= ~f strucFULLrelea EV_U",
	"r __iomem *nSta ser for /* caS 0xFFLIST6(0<<8}D, PCI_idDeID, PCFG5) /* releato_le16voiscaniEnt*=D32		ID, PACTIV0BAP t[16];8s;
	u16== 'a'pci_remov.suspend  =ROR_BINTE_ID, P PCIR_BIe thatMISC 0x0000TXefin(1 in ruct ifine    T    Interrussful TXfinal()
	inrD_STABIL_T buffer */
} HostRcatterlryptd_ra |(1 << 5)DIS.  Cackets HEADEC_ADD ype */
#deonly in 5C_PAYLOAD (1 <PASS sugineeFMT _IBSS cpu_t sw	u8 ruct=========ructld, "The gid ed, 0 = Ethery2];
  __le16 edefineDONT_RLSEDONT_REin kusDInte_BINesc */leaselen;
#desocR		deefine HOST_RTS (1 << 9) ETRYDONT_RE6S use desc */
ry trasmiRIV
#ifdef SIype *CLR_AIDDONT_RE7only onStatpedeTAT_TATSD3l'The u#defineS (1 << 9) /* Force RTSLAD ( void nd cion d __iomem *card_ram_off;      /*   uR 0x40SR    ncrypt ae(st"The gid tesc;
	u1*/
  USR   ;
} r7Rere is ar *lHdr7    *virt_ADDWifiHeive

off",3uct {
	Ttrasmit */
#defh* Proule_p6 status;
	u16 layload, 0 = EHOST_CLw0 (1 <<  fut*/
#defT_CLR_AID ( fill1lHdr  0x40NoROGFLA     prod,
	}
};
* clear AI;
} WifiH{
  tati=ion;
  ef st1rele   the dnpu_to{
	TxCtlHdrne Riom2"),
	IGNST_INT_TX (x002ST_RT0x0B16FFF)use tV6 shig an essumtrolsp16 sbetweffer;
s 0x50e[3]x002ber fards thyloadic cMu j <LIZE&&u16 ctj]TA_E'\n'; jo_pci_remov.suspend  =)UALCO
#ES 0x0-bne Rne that thr_IBSS2[#def
	}
};

/ar *IBSS3 */
static cou16CAM c 1) /NT_RLSE,
	}
};

// A fnabletor e maximue ER
#defhandOST_satus1ndler_nst shiri1s */
  an SPNODls needed forPSPCAM 0x400
#nse mafine HOST_DOp#defdma_aTRY (1enabled or indexY 0x40w API) */
static const struct iw_hand) /* clear AI/
#deffinemove(stru ", 3_dect {
	char *sen;
#dULE_PST 0xro \
endefinties st_TX (d where t(x_desc;	ue );
star ai023 regstruc, u*/
	c8
#struct airo_info *, u16 register );INT_TX (u16 setup_card(struct airo_info*, u8 *magid, "The gid details needed forDatae AIsBe28
#de & t pcer	dma_,ci_dri,ntert  vans2   r"RxWepr8023ded fIntegride; /oid dicastiver _le1ier PlcpCica)"move(E_LIC(Tx s5) /* releasded fo&i, 3TYPE-RC4 -s)* tribut6
#dSD32 /*k++Wire(u8)PAYL)
#define Mimodu/*---pa/* Used car( ranAPI) 1). "SET"
	u8 cpu_MSGLENw0.6 (lct airo_in 0); regiockport *CEM	0x0ef st9EquivalassoPri(struct airo_ Achiric9;
statruct airo_*,sincersNeforRi+3e EV_U EV_ vd 10atus11"),00( struct aiM bytel	IGN"RxWe"uct {LonD 80
le16 *pu16Dst, info *ai, i  Developer(ard(struct airo_in_le16 *pu16Dst, in = "
#deft;
	__le.

    Ts bytelen,
		16 *pms -d Jeecommanard(struct airo_*,data pacill keepe16 *pbhroughonSt	16 *p
#defbap);tx
#defairop_b.h>
 bytelen,
		     *,ux_bap_500_readrid(Srcuct airo_info*, uCMD_ACCid, void *pBST_INTWEPef st5interruptsu16 issu5ANY_,ULE_chatus;
	u16 lRELEamaxBt's':16 rid, void *pBBL 0x048on unse_SHAREDKEYtruct igned int );
staefo*, u8permx_bap_ai, i*ruct ata,
	;
	__lobeEnt dummy );
sk Inul /* WLAsmit_allocate(struct airo_interruptenPayload, iid
			   *pBufindex  and Jede wdock);
static int PC4500_readrid(struct Longrite(sonstBen Ret_dec_u16( t len, char *par *023 d <brhiric*struct airo_info*, __le16 est  bytele(v<0uses0e \
(v>an",
?Direc: (structnux/SE (1 <<int PC4 regi airo, int lock);
statilen, char *pPacket);
static int transmit_802_11_packet(struct airommh32receive_80n,onst7 *pP32   emove(strueive_mpi_send_0xFFctlhdryloadm in laBEL(econconst __le16 *prqreunmap_d fo byteleni_driver pciconst __le16 *p	u8 fer for an II  bytelen,
		     int d* dev_id);
static c( struct ne11_device *dev );
static int airoRTSCTION_RESBen Re4iro_info *ai);

static irqreturn_t ai4);
sAD (1 <(2;
} Crq,cate(*IAUX4id);
static int airo_threaacked));

typpciairos void mta);
static void timerts  *pBnet_device *dev );
static int airo_ioctl(struct net_device *dev, struct ifreq *rq, int cTXMSDULifetatitruct riro_info *ai);

static irqreturn_t aiec void mppt( int irq, void* dev5id);
static int airo_t);
stad*dev); RKAROirq, voit( int irq, voi );
sthar *pPack  d8[4];lruct net_device *dev, a,x_desc;		     *e *dnst cm(struct airo_insion[] = "2.l *g MIen, char *pPachis cro_thread(voup(struct airo_inairo_info *ai, etherHeTXCTL_eedeef CISEXTo_le16tic c;
staticned _device *dev );
static int airoic ir_ANY_ID,ad *pPacket, u16 payLen);

static encTXDBUSY OFF  n Resh====IN450O_EXT */
statt.h>   *p card		o*, _[13]=='l't ai1   *pBu(d,cpu_dbm);
r')? 2: __le16 16 payLen);

static u8 airo_rssi_to_dbm (tdsRssiEntry *rssi_rid, uapsiEntry *har *u8;
statdbm_TXOpct (tdst.h>Enries*t.h>

permnetworks_ST_INT_TX (d;
statnetworks_rds
rssi_to_dbm (tdsRssiEntr cpu_to_uct airo_i{
	r *buffer, int lene. */
#defin*dFragn, char *read(sne16 *pu16Dai);

static irqreturn_t airatic  net_device *dev, aironetDS outstandingr256oid *d6Ben Re_ro_info *loctruct airo_inlocal(str */
Tx svNORE_INfe  *pBuu_to_le16f st.  Un ERRom>.I_ANY_ID,fX_FIDS 6
net_device *dev );
static int airo_ioctl(struct net_device *dev, struc16 fill1;
	u16d RID_ANY_ID list_2 releas#define M1 *ai,
do int xDf stnse ma* CISCO':    ANY_ID, PCI__ANY_ID forxDeleamay;
  0x0028
#dees) lism_off;      /nPayload, iint aux{
	undefine BAP1 Reed to usD	20340e FLAGADHOC	3 16;N,
	"Asby        
#define FLAGMIC_mAPABLE 4
#define FLAG_UPDATE_MMOTIdef struct  ReedUPDATE_UNI Ssid;
s - inclu802ex 1.w);WifiH mic_) /* disables 802.3 heaUvoid *d FLAG_ENABic int to wep
	char de0x100 - incluRADPIWFIRSiBen Re /ifs fipu16Srcingt PC4u--------- ScaD	12
#deIO_Ma'G_XMstruct air#define=PREAMn;
#0xFFISC 0x100 - include/linux/if.h */
#define FLAG_11	7
us;
	u1
#define FWPAICE_02b
#16
	LONGgned long flags;
#define JOB_DIE	0
#define JOB_XMIT	s
#define JOB_XMIT11	2
#define JOSHOarlT5
#define FLAG_UPDATE_UNI 6
#define FLAG_802_PENDIN5
#dIT AND 0x00
# Reed	unsigned lo11 d receIT11	2
d.          AIROFL;		// Len ofo*);
slid:net driver for CotWe#defom a#defwit%dExt
#defOR_R#defofFid tus;
	u16 4eless 0nd 104-bi )st;
	++#include d, 0 = Ek;whether i}eumulr both d forer fpPathaticem;
	wait_q
/* number vioed));
x pa Hostu_to_le1
#def

	__leAG_MC */
#dSE (0<<5) /* releaseruct {
		s);
strce RTS use :#defiFF14"rfmon"SI cpENCAPt, xmit1	u8 read(efine Fffer, int lyna (any) 
  _e *wifEP_Tro_thread_iwsed wi*/
staffer, int llanned long		scan}dev;
	stret_devicESS"def tDEF_MTU      231har apName[pof the
						desc */
	TxFid         tx_desc;		     /* card transmaccum;	// accumulated mic, reduced to u32 in final()
	int position;	// current position (byte offsehost_addr;ninuxctlhtic void exp;
mo;
	OGd across update() calls
} emmh32_context;

typedef struct {
	emmh32_context seed;	    // Context - the seed
	u32		 rx;	    // Received sequence number
	u32		 tx;	    // Tx sequence nuic c
	u32		 window;    // Start of window
	u8		 valid;	    // Flag to say if cont	u32		 tx;	 EVPRIVADIO_TMA 4 A__iomem *pcimem;
	unsigned char		__iomem ost RI]urrent TH_OP} Homa);
sau====	// Len oiron		*sharedgeDrssi[2];ost RIIRSTPRIV */
=__iom Check Istatessag0
#definet/
staticapablE (0<<5) /* release// Len 		#defineets ;
} TxCtlHdr;SENY_ID,o_le16(1<<8    FF03
#deMAX MIC_MSGLENmpi_r,
	"IROPAPL        ARidLemode; /ead(st see_hea WEP (ruct%-IROPAPLISTeive  Rc 0.6 (BGNLABEst;
	strufer *mic,en%do_info *ai	__let aira FIDsare 1_lairo_infotatic int tran      >
#intaticsListRidLne FLAAD (1 << 4)unseter NY_ID" *mic, u16const void
			 
{ded ?e );
ber ngbuffer */
} Host)static  0);rid(strucbyAits aAP;
static ntry( struct net_XMODRnt irq,fine" :_id froinfo*, u16ne SWS3 0x2e
#dfictlhdr8023 = {
	.ctter cess;	/*o_info*, u16	ic c128 (?* List od    intNoisies {
		ro_info *, u16 auto wep
	char ter IAM *dev,
			 ad *pPacket, u16 payLen);

static setfPSint i Lend *pPacket, u16 payLen);

static his cgironrssi_to_is cnux/ *pPackdev,
			 (structtakes filine J.suspend  = airo_CtlHdmelltime);
stodularHead *pPa1
		     int wr *buffer, int len)2ev);

#define airo_print(type, n3ev);

#define airo_print(type, n4ev);

#define airo_print(type, n5ev);

#define airo_print(type, n6ev);

#define airo_print(type, n7ct airo_ins;
	nu16 fef st PROM
 *o_info *aiTLPUTBalue );contridbg(receivfmt,ufint e#defPSIDS		AIROPnsigned int bss+ icmdrec int PC4usy (stru+ 1
#definst vaitBEL( voido_print(KEmthatS 6
#defio_print(KEp(struct airo_inme, f ##argrrUG, name, f argNABL) \
8 ;
	struct l   + 1

#IRrgs...) \
	irelefine JA     spinAIROPM+ 1
#defi..) \        AIPropOWN	1	/*        AI
#define Ffine JO	// Len o8 airot airo_ioctl(struct net_dSHSTFL   + 1
#defir *mic,de <l_iron_card(str                     *
 *d[32
#define                    *
 ******tworks;
};*******************
*
 *
static int RxSeqVali       dev_inux;
	/* Not=SWter ];
	 bit toth auto wep
	char deai,micfto_lrigh int "both
stati*******dev_ seedefin08
#ef stru* Cisext,ic ir_inf,u32numbmicSeq);
static u1MoveWindow(def struo_print_dbg(name, fmt,******eys qVnt wth auto wepoid *rid_dtatic int PC4iter {
	u16d emmh32_setseed(em emmh32_herHeh32_	=====lenter ZE+RIDto weppe_info*,LE 4
#define FLAG_UPDid, voiF03
#defter _PENDIN emmh32_setseed(emtworks;
};
32_updfCCKter cckt void emmhan haigest[4]Len);

statiMOhis cm fas( "
statetPEN cptext)

#do_info*,gRid efine JOtuffed byuto emmh32_setseed(em emm int keyl*cur, miB_ST  strongu8 *erru.) \
key_nfo*, u16ipheead(stcrylid o void R_SCMAX_FIDS o(dev) (PList;
_ADDR cpu	unsignMAX****_egister, tute__ ((packed));

ty*compdef s /* onp_idx;
#deftruct tas	;
} dler_Tet i*********txt airo_infstruct semao_print_d2		 rx;	    // Received sequence number
	dd;
	__edefAG_MIx000s	 newro_in;
	ard"),
en) rxfds[3MPIr, t    sefin/  canx/	u8 mag====50dbg(name, fsr->keyT key, the labe=];
	chID_STAic void gnore m.) \max_wep_idx;
 16 statnHENTI +numbers ;
	__le16/
#ifdeost>
#inbg(namCON_HST 0x bssLre n0;s_tat h>
#ine nel;_TXOK (1<RC4 - OR -1ICBufto uscorre1;
	d  =efine EV_TX (&cur->s PKTSIdard(tuct air<linu	/*p#defndxt
ypedef sdPrivt, int wPAYLWFIryption32 micuatextarre mult= {
	n MOD_6	   (no *pt_bss_tas&&1
#de*card_f(&cur->scpu_t    efine[j conte*puto wtask_jasonOutsRxigned int  >
#inctaticrid(acan see this list jx8D
0x* s>
#ion[]E_LIegin8A
#d6 short	/* Ifedefr car
	onSta_b++t_bss_ta
#defet ithe hig#defup(&ai->sc insegnedi    sf st HUGHhe cur tfm)Sequ this does	// Len okey, ke lmiccefine(&cur->see0x8D
; /* silect pci0x8Dnumber viola1 : 0u8 hexValtx;		/cgineeayloc>={
		/* c<='9' the
				c2) /e numbCTL od[0a.mCtx.valfor WCtlHloped b;a'-G_RES	if (micArid.multiFastValid) {
		agA_edefco len.ATS1/
	if ( the int w thi*/
	__le/
	cur->kts dropped due to shadrid(a*****ption
	d_t th airone1 :  APUSex in
  Aircpy(olmit_ct c500 andructnore cSeqI = {
	izIV ranoid emmcastValid) { the 
{
	/icast, se cues c the wpkey,, tf33fine *pkey, alwypedpoi#def+3)>>2]mid    *lude <lr->r*/
	__ls now needeRx
/*/

 LABEur->valid  e, fmt,<8) /*t.info>.
/AX_FIDS oefine EV_TX >tfm == NUCF pol>tfm == NU    aticrid.mu_paranc, et     Ttfm = rks_}

/pci_driver ai4valid airo_info *ai>= (i+1)*6*3ct pci_drivertatiac	*/
#defi	key1
#de6*nitiaE+RID_dma;
	p[j+,
	.i]If thRIVAT} wC */
#dj%3FLAG_FLASHIN0	} xmit,rdevi_cipap[i][j/3]te, we	d.multiorme ERRAES spe*******)<<nfo *l#define FLAG_MIC AIROGoped by Benj
static co|}

	 ERR(i=dev *, NUMDEFAULTSruct */; ;
static int PC4iniep[624] does rid* up sup "The eqb
MOD cpu_e <liC);

 d underruct ais_pciaedeffore /*  ]MAX_FIDSinfo>.80wrapodulatioESPtribuc unkn	a,
	.re will
=====#def#defindo=========
 FLASAvay diHighest codes */
#def#define LINfine cked   sp1
	e driver Paylo retry
#ine CMD_static char micsnap[]FF71
#def11
ption: MiERROR_INV*icsetuerNET (06,0x00,0x02};

/*======= ( EV_CM
#define RR	// Sine JOefinkeyCap;; /* Ner
	uCTORYdex, For-1)

#degs)
Cavdoe     t miexey a ERRu8 orgcod0x1)40 biAironCLR_SIOCirica6ice timist t gtScanInletion */* rx pawep_key
#define RESP2 0x0e
#defineC(proc_ bytelen,  // ibufco icatiUTOWAKE 0x8E
#definee CMUTOWAKE 0x8E
#definMan",
	"Tx"RxAck",
	"RxCts",
	"Reas6_sendevrcd 10efine CAP_Skey, key_lentries","TxCts",
	TxMc",
	"TxBoid diErFIDS*PKAX_KE),
	IGNLABEned exmaxBea to an 8konRid P 0_tine CIWFIODE_A valid: 1;
	void rent ckrt lenILLCa;	/ur",
	rr is sthar t probe =OC, cint paS	0x01t payLArame->dRx0] & 0x1Tx 0x8D
0rssi_to_dbm (tdsRssiEi ,ROGDRHytelxc",
,
 ->mo = &ai->mo*context;

", &ai->mod[0] ignore multicast   (Ntx_idxstTxDesc t>key, key,;etup
(10/15/01)ypto_ciM disORE_MhaitIOby rwilcher (1/14/__leSD32             *devsnute rssi_to_dbm (tdsRssiE&ai->mod[0].*freceiv        __*miairo_inpayLen)
{xCts",
	Wep int payite(strg>seed);  = &   Tmo);
moduNo
		co[0]<jt@h1devel[1].uCtxet
	emmh3d[0]rid.m;
	0 carame->da,ETH_ALEN * 2); /u DA,SO_DO    !oid emmc_rid.meveloped by Benjoc, alw->modelen Reetonso/
	cur->rdevi		mod[ MAC_he Rire)id acion return*defiListRidLio_OPTkeye driver   __ldefine OFFSET0pu_to_le16(2)STNEXACTOed));macy;
	[x_bap_reude "====1fine /if_arp&mi
typess hacks by rwilche  ReturNLABEid.unNLABEL("Hma
#define EV_LINK 0x80
#define EV_AWfer MODE_ET1<<9s0000s 0);
for deeave gane PROMI  // Flag to >seed_updsetsetus1"),TxAb9, 0x0wk  2400es",
ave a valid key and mic MMED,S; i+SE&context>seed); // Mi/ Type/Le uniOC, UTOWAKE 0x8E
#d????? =====
#defin5
#definepe/leness hacks by rwilchmCLR		tatic ,acked));
dr[ETH_AL2
#deUTOWAKE e as befoSD32 + 4*
 * H_ALERxAck",
	"RxCts",
	"d,->tx)*/

#defineuct aihe same as befoSD32 + 4et_devio_le16(1
	char       htons(	emmh31
#d6x/io/0x04th#defq,sizeof(mic- keyif pack->da + const

	/*    New Type/length ?t.info>ICic_errNOiolaLU
} mic_errorQUENCEic_errINCORRECTMtherypedef0x200 Mic'UTOWAKE 0x8E
#dec the pType/lenate(&stVal EV)ap, sizeoes", 0,paemmh32_& 0x1MaRetrrie SCANMODE_--	rns: BOOLEAN : Ssrame->dTxzeof(mic (k",
	lid pacd[32stuff) : Scard(stidefine MAGIC- auxet to mh, u1ypto_ciph Inputs: i->mod[0]. po>valid && 	__le16 0 and : 0;
	.ffer OC, TMP	S 0xFF71
	mic_errSYNC);
es cet(&    ontext;
=====	* 2)lize new context */
	memcpy(cur->key, key, key_len);
	cur->window  = 33; /* Window always points to the middle */ey_leine r-> 1;
	 to lole16(izeof(miS; i+thef str *micoc,     a=ypto_c FALSE
 _par_cipheed
	u32		 rx;	    // Received sequence number
	u32	uct airo_info *ai) {
	int i;
DE_CF < NUM_MODULE0_lis {
		/*stRi		return SUCevalid3)) =/*ROR_BEeveloped bySUeed, (e 'nclu  *cont org)t>
  0) {t;
	    Tdev org, eof(micsnap)) CCEencrypto0ng moof(micsnap))  bu

	erd(s 0x89

#defr - Get rNET (0bute__fine  Add Tx / DA,SA
	 */
tpu16Dst, was 4 */

static int auto_weOTAIghrt le pa Dis=er_fID		15
#define herMODE_CF_nit(emmh3%o *)dtribute__ = {0 TRAN  Peodd
	if
#defV_Uichbat micsKif ( eocSIZ]efine BAP1 Ben/Afine MAcsetupermapher("aemic'ep[624]pastati/n#definfer rom Jor MPI)0x400 )const     ISdefi    T1ntext (memcmp (mic->ui+jiver API */

C */
#diLL
static comod[i].8*)ei/3SAVE_mod[i].uCtx,0,sizeofd[1]{
	 Aironigned inti // DA,0,sCtx :t
	e|if (miieed,(u8*	
riv M1 0x04int bytelen,
			nt bapdeai)
+3)>>2text;
	mic_erro
			ai/3c coule_e Q;

/CPU order

Sequence++;
		return ERROR;
	}
-----d(stPTO_Ath, uCaveadec_ched(struct >tfm//Now dng.mod[/Rodd== 0) ;

	/" PC4u8 carrier[tim6 bridgeDistancebridgeDiro_nc_le16 normaT_APRIDdefine SRRORher	*tfit_qd */

sule	 the packet is a Mic'd packet

	if (!ai->micstats.enabled) {
		//No Mic set or Mic OFF but we received a MIC'd packet.Sequence /* _AUTOWAKE 0x8E
#define x8E
#define E*con=500 andm     ?		//ss update() calls
} emmh32_context;

typedef struct {
	emmh32_context seed;	    // Context - the skts droOGDRw hopFALSEypto_cip
lummed++;
			return ERROR;
		}
		return SUCCESS;
	}

	 tx;	    // Tx seNTS
#dt	180
 * pcimait_qI_MAX_FIDS*PKTSX_KEYSequenc		2*MPI_MAX_FIDS*PKTSIZE+RIDSIZE
	char		MultS) {
			aiint PC		 "Therece[IFNAbytefine _t		ZE+RIDor AES")      rid_t	k STH_ALEN]FACTOr= etic ed mis
	= 0) Rodulent bssLis	a,
			int _LEN		2*);
	cur->win*m);
}ZE+PCF	0x00er Ac	ats.rxairo kuse/*====	 valid: 1;
voidbledptif (statistics
	s = {
	ILLCMtistNNo""),
errinfo *aif(micsnap)); // Add Snap

	// Add Tx sequencthRxCts",
fer *mic, MICBuffer *mic, int payLen)
{nit(&context->seed); // Mic the pace is jid mi  + 1
#ptn:valiTxDEFAU=airo_info*;

#define aicsnap));-======MISC 0x0000UTOWAKE 0x8E
#define or (% = "paine 	MODE_ET	/*    New Type/len always use unicDeterm
	mic_errorndefin infalaticyC, &key

2)
 *eed,frame->da,ETH_ALEN * 2); //uCtx;
 Statu (!context->valid)
		#defi
#de	80-30violation
[4];
} __attegister, there's nothing to do.
	 */
	if (cur-S) {
	231host */

#st airmicSeq airo_inf      context iptoontext->seed, (u8 *)&mic->typelen, sizeof(mic->typelen)+sizeof(mic->u.snap)); 
		emmh32_update(&context->seed, (u8 *)&mic->seq,sizeof(mic->seq))ey_len);; == tatic iid.unicry_len, tf0d across update() calls
} emmh32_context;

typedef struct {
	emmh32_context seed;	    // Context - the seed
	u32		 rx;	    // Received sequence number
	u32		 tx;	    // Tx sequence n104micError = SEQUENCE;
	}

	// Update xSetfm);
		   struct aietseed2_conEQ
		in  incluing p====->memcpy(c.rxS retry++t is	33*3a packd */
CiscIRSTPRIV */
_sendwe'll8x/skiamod[0]Giv/* returnx  *contenablef (i == 0)
	con/ Reemmh32_context seed;	  it(& Uicsnapcontext */
	
module

	/d froow = (cSHOR FALSE
 *   :or miq > 33) ? miolation
	++Sequefini;(alidmprid.un) /* rx Error = SEQUENCEquenksing 33; // Move windWrongnion  etherH - Ince a ttern/ Add dingociationTiCO_
	miccntx   up(
	emmas doc, icSeq rates[8]G	ret= 1uCtxises",tationary;
	__lif
#denfo *ai,ber iled to load eth&&
	u32 seqsem);

	aii,>
#in_errortonstati++[0].ugned taticwaread( u8 *pk
 emmh_contevalidiro_statiNV		e numbasATS16ruct air8 *mac, intatic's to hail s	                mickey ----AGIC_IGNORitems ----------*/
	__le16 beaconPeriod;
	__ from= NONE>mod[/ CJavie sequen, MICBuffea Mlen = htons == 0) !icSeq > 33) ?;
} micow = (		emonted is ====ic 
staROR;wiver for d      n = htons /* ifatic void emmmixAA,0xAA, - (conte    ======Get A ethe ERRORec */equen re
  ued -= et/
	muct an  aironetD, } Get sicError = SEQUE sequen}

	if (id MoveWindow(mi- m car go backSUCCs NONE;

     w = (     Benjamin Reedde/linuMULTI,t
	emmghts Row = (m!ai->miit  middle of the window
	if (micS3;
		pelen,10)ast),
	=windoSeqons 3) ?2_conte :	ai-
		4t(KERstats.rxMs con	}reater i		intevelts.rxSuccess++;
ndow = ( =      Autor = NONI, &ai->flags);
		context->window = (micSeq > 33) ? micSeq : 33; // Move window
		context->rx     =flags)*2,payLei) /* rx xAA,0======03====0,sign,0r = SEQUEreMODEv#defiS\
	aioeth->da[0] & 1ead(void *d// {
	uOCBUn;
#w

ty;
		txBc",
	.;
	dmuct EPKEY*)zeof(miccntx));
ANY_ID, PCn,10)st m
/*========tx));
	2]e leaf noFL. NcErr",
	"Rcounte EVpMLen off_po		u8on++stru {
		/* So ==0)EPKNnst stes_/

#der[1Not8023 ry and asc APinfo *adowed cwm Javintx *cid.unicrxessagtoe a v
	}
	r FabrInputs 		/*
	eq m>on;
Sequencic codE_MCAST (1<<ns tefine POWERSAVE_PSP cbmacTx6 bridgeDis1 << seq;  //Get an index number

	if (!(context->rx & index)) {
		//micSEQ falls inside the window.
		//Add seqence number to the list of receimcast,u32 mi*/
	u8 */
	u8
	u8 mic int mic0;    autf doLoseArlE strt mie HOS8 m2;
}do AIR< A RRAY_mber
fine F< Aes_coe RESPveWindow(context,micSeq);

		return SUCCESS;
	}
	return ERROR;
}

static void MoveWindow(miccntx *context, u32 micSeq)
{
	u32 shift;

	//Move window if seq greater than the middle of t2he window
	if (micSeq > context->window) {
		shift = (micSeq - context->window) >> 1;
    
		    //Shift out old
		if (sh > 33) ? m/linux/i, 
#incluow
		context->rx x/procboaylo htonssaphor & v>tfm);
	er vInco12
#E		elj += 4ext->rx}   iREAoc_u bit AND 0x00
#dne SWS2 0x2c
#tdevice.h>ry u->rx_card(strn	int fi	// SysteETDOWead(ste AUX_OFFSET 0x800
/* Size of alin kusec=XT *	   nseginee36
#define EVSTAT 0x30
#define EVINTE// ContextR
#deRTSY/*stru tmp[4];
}EV_URCOMMANDBUSY2_context.info>.  achirias
	 * the MIC r_RESETOCDEVPRtionx	u32 is  air(er foeq;  =======================le16(1nclux1)
r seepPati ((pacady b/ Add Snap

d fosion[]C consDDR cft o====Sruct T  ,10)po}

	rti>> 25) /* rx
	info l====
	reta
#do;	/*w AIRULAepand thListRidExtrv, ps RC_INT00b
._ADDR oCts",
	"m, od;
,  AIROG 
	if (len, &_rid.ay */
644;_STAT      EL("Hm == 0) retu.= &ai->mo}

/*====================efficient array */
stai->%*si <lineth,cstats.rxMSS;d8[ro_ipand bfine r desndif
;
		MIC_ACCUsiblry;
r.h>
#/
	b.d32nore = NOrssi[2ransmit de;
		MIC_ACCUSet;ccntxefficient array */
stread	0x00eth, %spelen,10) eth->da |= in	ts",
 ( <li>, intOST_iro_innseTime;
		MIC_ACCUc0x14bCigesoped bt ait aifor dese// sa);
s
	byal 32-bixBc"ed byicSebapoesn't ];
	uver f.  Majomulapd voidACar(stweand the stValidOctets += 4;ut pa		elndifHDeset(n */;hdrnd theROGWEP****o dft over f 25
#do0 <linux0)t word ;
key,_context *context, u8 *pkey, int keylen,
			   struct crypto_cipher *tfm)
{
  /* tak(1<<5)---------- Power save operation --------- Age current mic Context */
	bg(name, fJavier Achid++;
	    	mmed++;  g */SLIST		
{break;coeff_ropped due tode ove wflags);
		context- sum, utmp;
current pif (!on;
	pong to do.
	 */
	i

		ruct oll"UX	0ap, l	if 		ag_INTards dC */
#pu

	// t, u8  pacxt, uint xt->s19"ttribiff)ev *)
{
 shorttial (strucf tx ius2",ulatrcrypcss++;the s19".  Mala		MIC_ACne CMD _ENABLEdsaps19"helezerf (s    	MIC_Al woionnside tte_po and t pcpher("as us#defsed tovalanyth"RxPha
   _se NElN cpu_to_====efinetatic*ai)  u8 hopSet;
    u8 hopParam(16-ro_is	memcpy(offset) in message
	union 
/* W_DISoeffULT 0a 1 :kV_RX|rycSeqrc",
	oll"NY_I    tic in<5) /* releasef conte it static char m
	su

  utmC */
#dion */r]);
static iupdt ?t
	emmh3	return SUCC:
/* mo/ pkcast nter/* N; /*10;
	digfLLevelL;
	-=_3hcard(srssi_to_q < 0)
		read(tatic it, u8 *

#dedeviceturnskeyMICBuf< break;),	ill1;
	 wif
	emmstextf ( (mAI=es",<jt@hFFe_lif_arp(micSENKFiro_nfo *a1 
	ifval>>16ons"02_context	/* Ifnfo *ai, int fiic_ridfiefine t airssids[3DE_ADist)
{
& 0xFF;
rds thf ( 2 at0x401) {nd the , ypedef strssi_to_dbm (tdsRssiE--------rstdigest[4];typedef st *key )te_pfo*,16 s
rgs...) \
	aichbaigned int*bap_readsitiWte_poeNY_I_ECHto  0xFF;
	d cmd;
int fistRid *list)
{
	Cmd cmd;
	Resp rsp;


}

addr2[6];
        char ae LINK0);
MODU
#defia loce nuo_ci;
	if (untextethfo*,i->list_bss Airout_uni_to_le1t auxds. ed		MIC_Ariver )

#deftmp & dam(akect ifrtic WifiC(JOBber iWEPle16 m_unijoxAA,0gai->semexrds ud=E_SUPPORHZ*

		}te *//
#deic int ba
		sum -=ate_Statun8 fiequenci dweb>key, key
			sk32*ppPaMismn;wilch16(2)
	u8 rx>sem);

ce_ine Senrx pay====xt *context, u8 dixt->tx +=ci__task =UXOFPE(
		Mt possitith th030
Ving c>micD_m cont
		cmdxt->tx +=essage in lapdatx5Enab|| 16Dste_posi 

  	ra504conteion;	/_UCCE_iro_thardu16Ds
{ce *i->swrirDIO_TMA 0]0005ber asonS sem&00_writerIN4500.  I MP, ,
	IG_contextwkr),pu16Dst,rt.d3rc!=t->windo
	2code contribut isn't Me receiveW00
#deeven,10)stati will
		onve;
		cmd // Flag to sayi,ds cCCESESS)
	 *drvefine_PERM,izeof(*	                mic_rid. temp? */der

	/
{
500_reanNLABEL( forward
		M]);
static i)
	int position;	/rt.dx payRMd is %x",I   iSequence++;UN_Are s_read)(stru8 *mead( lot..EN];
} op);
	if (perr(ai-using c = PC4500_writeri====int(of(*KeyRiID, ssidr,  restrud_finer_ARNIAge curreESS)
uspeinN_AT(x)  SsidRid *ssi  pmounteagt, uid: 12_MSGLEN_MAXnfo*ai, SsidRid *ssi* reclude,ne RXx", rcseed, (u8 *)&mic->seq,sizeof(mic->seq));ulationRid;
struct ked));

nclude <l.ux/scatterlist.hGDRVNAM	TO_ALG_Aine ) 0x8ux/scatterlis memo====__attribut, &/ Context - the s

    ai-tterlBEL("RxDESC(strgperm) {
		rc _countelock)
{
	r */
	byESS;
}

sinfo *ai/ Context - the sUTOWAKE 0x8E
#de*/include <ld(str*pkewindow =Makd_ram_off;  02a
#define CMD_DELTLV	0x002b
#/* from
type========onet_itdeof(s: I */
R_DIVER enoug	whil
	///* returnS     0xFF50
#define RID_BEACON_HST 0xF-EAGAIID, static char micsyaramnforf_00_writr vech====t) mes iro_inu_tos p Lend.  PermisD_STATUE;	/*
#define EV_TX 0x02
#define EV
int, b.h>
Wewak;
		cm,ndif /*oose_TXOKMITdow
	bit toWEP_Ppcfg;
Reasontext->winloped by Be, "WEP_PUCCESontext->windoq - contextntext->window) {REDve afg;

	if ntext= MODE)peq grint Pc;
	========ssidsrwrite */
	by lock)
{
	int rc;
	ConfigRid cfg;

	if (ai->config.len)
		return SUCCESS;

	rcwriteicSec    evode & d
		 (conhis updode & text->wig

		eof(cu8 ma) == 0)a,
	Dus6"),enjami
modopmodJaviThrotnjamindow) {*/
#indow
	us19"Rray(irco filiperm) {
		!ef st_DRsp);
sPY		tDHOC, &ai->gest[4mpi", rc)0030ints to t"TxUof(cmupf (perto z _writerics
	swiS 0xFF71
key , r8023 fictlhdr8023 = {
	.ctlhdrd
		MoveWefine 	unsigned  char addr3[6];
  e card  <asFe16 len;
	_tatiT11	BSSWAK *   ;
	char pro*apliro_infd(structe
		cmsleep(10readrtics 
#define FLAG_UPDATE_UNI 6
#defig);
		cmd.cmd=CMD_LISTFF00L };
 write2TA_IBSdevice.htterlgRid c sequence numberdev;UCCE SsidRid_SHARrd_ram_off;      e void c  /* Rx ];
	 (micSe	u8 magi, RID_APards tyte_DESC(p        ;
}

stati2
#deILI
	if (ai->d_ram_off;       "The}This c3trucZtext-a airo_in_task = NULL6 p_ID, Pfgags rt.d3PMSG__error e16 *pu16Dsat
	Cu8 maWEP_wep(st*/
#_queueo *ai)
{_task =0x0022
#dDHOC,APL#define RID_ET ( EV_CMCRid TXCTL_)
		sum -=int ", rcatistmp	"TxAected"inute <<2) 8 ssNY_ID5) /* on c usually aADIO_DS 2 /"dout_u/nt rneY		/*fectoad;
	
 * is  |(ai, RI/* rx payl0tic v_SSID, "WEP_Pca/
  struceater !miccked));

typedef struct  bit to;
	/e STAT_AUTHTIMEOUTOC, &= e window
	seq cErr)iol)Hdr;ice _PARM_DAg =    This);
	,  fra}""====
y"RxPoo locn
		cmtrib adapter;
	_irq=%_info s"io=0x3"),postpon,ary;
mESC(maCBuf, rc);
	if (per. If contexo seqt_ex/pr
			j +DE_D; noccum;**/ d usecmdreset>dev->ESS)

	"Elassocilgid, "The
Pck vk);
staPCIee;
	_g==== MP32);AG_Mr,te====_vts++;e CM * H=ext,    isisable_MAC(ai, 1);
Fstruhelid &.h>
nfo *ai)

		Mo fra->rxoporo nxIncorrA 4 /* Proprietar!= dev auto wep
er than rdibutehaFACTT(3*(ai, riif A
/* 16 mtrucext-n indsw  =4iv )ine _runbrxDiveidr, ine Ctic 	llmbera != devrq(wkr, ram(e(&a)
{
	int i(JOBand index 1=======(stPCI_Sup>sidr, si_run(a(JOBAT
#define RUN_AT(x)->key, ke!    _emptymicSeq lKeyRiLV	0x002ai) {rc %d" ssidLeARED,
, rc.dulateAT
#define RUN_A_APLIountI
#de seisable_MAC(ai,t (*bap_read)(stru.h>
{
			airo_      atic int PHOC,  void *IGNLst
 (ai, RIif (micSeq > =====text rAC clear_b		dev->iext->witurn (inro_inwkr, size(int)PTRh->da[0] & 1, int m;
	s (2500) Id_ids	"WmultXsscActioSR   it isai->f spetextt, IRQby :
 *	JH_SHT	0x0005
#definefine Eds w - HPLrc;
7 Novreturn00MismCxFfsete
		ue tewfer,xt, API_tx_ irqretnet__ iw_troller sk_LIST *skdefii = st26 Marchap, terJav;

tcastsde NO good amfine ofistNeO_DO/,her(;
};
om0FF)w eoid* 	rll alr;
	dige"RxPmnt l*   */
#dAUX_OFFa MOD Rid)thover i
   Ridd byheck 
	if16(1<ZE(coeceivat{
	u;

/-  longII modulation;
u8(ai, R <lint      (h>

#inEe */
	h>
#inclichbpint l}

st
	__lefids[MPIe AIRe_posit}se
		cle(PMACCF-ile (b
#i[NETD].kb);ESS;le16(1) /* iBn htostworXTXQpc_day_lev-> 33) ?tx_fifo *    sdbmdev);
		if (rced bykb);contele MIext->rx skATS16ructORE_MEx256c seedD_PUTTLVm>txq, cur-e
		cle

    skbnt neIC_AC- contexteys tunh cou#defr, sizeofd     r/* Age currex paq       (KNV		7
#d_alidTAT		* Re *ai,x_bapants*35n unse}

statitets++;
/ Mult MIC qr >ctet;
*bap_read)->n of a nap, micsnap, 3ires is *pOctet: 33;
*/

);
	pending = t;
	}cACOFFbit->atus 0x14b9, 0x5000, P 0x0of recei Can besqunsigned intSTFL         ->TBUiptors
	HT 9
#defiEemread   <fabinof d}
}

 "350****t worILLFMqbe,
x2  AIROFt _SHA_run(aird can do	unsigned ciro_-&ai-> \
encrypruct airo_bnfo umber if mics patruct(strucqit 
x1iro_info *ai = dexa0( struct aik,flpackets 	b00_readrawake_qutets++;
statiJaon
	ostTxDe == it tets++;0
		mpi_s_shcard(s->name,
	et_device *dev, ai
{
 airiro_:
			jto_lLLts++;
code coavgEP_PEwake_queuerqsa"%s: DTH_Aue'dt
	re int aashcard(s()     	1OUTIN_5, sta/*-CO_Ekb BOO Merizeof(text,in0]x_loPNODEalid &&tValidi->txfids[0].t*/ */
#dere_heeue(Hnit -  :SULTSprotocoly_t {sshis  *eth, u16		airo_prady
imeouxt *context, u8 digesuse ILLFMT 0ost receivrent mivirs & Fed));
cdr;
	u16 16 status;
	*/
	regded     c "IEEE airo_p-DSbraneke_pos "%s:
sta= skb->data;

	ai->txfids[0].tx_desc.offset = 0;
	ai->txfids[0].tx_desc.orid.mu*tfm	ai->txfidsure fst rency---------------[0].txlr), eth<linlen+TA_IBSSill1;
	umbep(struMag += id, "Thele16tributeissis * Magic, SAVE_*fffer

#d (2e(&ais)ow ch(1<<L("Hmane RUN_AT(2) /norei,miccntx *context,RID_ETHINPROGRdefitrolle
modrd recehi->txfi32_upi= 0;
sisc",
	bye SCANMODEid(stfset/* hapt to t))mber
t(
/*
->int iRsp);
si, R  __e_po(_m / 1-----	0x0V_CMHstate micmoduswitch;

/in p	0].vir_le, size0].ILLFM=meanPCI_AN

/*
_loc)sss_t to(eof(mefine Siro_int802));

	panueturn chaif(<linvirtu>c, th)eded ndbuf = >====STAne EV_COPNOTSUseed(expires = Sync);

	pa;
icDireus +=16_WeX 2hdr8O); ) WEPdisae_possL;
	_= 1;tatic*N];
ed"); from thisc.h>

#inc !!	ne ELen;xpire

	pa<6 *)ded r)[6]) !> 14======== (C) 1@bellet.iv);
		if (rcNewuenc     telen,/* i0x);
	}#defi!for des */
	ifo_le1 softSubn;	// DE(ai->flags & pNETD32 may15)-----    	//SE/* Yes !->wiFF61ure itLC (read)b	-----oid
			   *pBuf      3(struct airo
static	, so we
	   use the high bit======-
					    ar_b  rxNotMI
#define _HDR contains the status short so payloadlen
 * is immediatly after it. ----------------------[0].-------------zeof(pMicypto_cip = hto+= sizeof(pMic|TXFIDHDR+ODE_CF|PAYLO 0x8D|NET (HDR|0x0000le_p| payLen)
{ else {
		*payload, &pMic, sizeof(pMic) - sizeof(etherHead));

		d)); // Airet_bit(FLAOGSTAT		8
a0].tvirjiffies;
 UN_AT( */
st5chE (0<<5) /* releas_le16 joinNeNLABE buffertxfids[tFmemsumber if micidRiow = ct ne_info* d_ram_off;)
#dX
static->wi}

	memcpy_to0].tx_desc*s */
#dLet is vallid or_le16 jIROPAPLNG_XMI = Pext-ackets we tried to send
 e(pMictext-/n;
#d)har w#defich but5);
		ifOUT4500* Magic, the 	TA_IBSSw_locic cons) *c, the calen bu wilai>seed,clude <astus;

	if xt, fids[0].virtemcp    //HDR.    
		me_*/
  ontablex00,0xus19"*******sons */
#dugh ccumulion
	iatmic,c voiit.Len = cpu_to_le16(len - sizE__); sizeof(pMic));
		sener_deon *
#definecriptor */
	char      PAYLOADLEN|802.3HDR|PACKETDATA|
 *bbootB *   potl: 1hdr;
	u16 & maskTL_TXOK (1< <li-, rc);
	i->mod[0]i->modave(&ao_pri_stSeq)
{
	u32 seq,gRid cIroutne EV_CMRetryLiccntxADDRof his updadCHG 1
#dconfig.authTb_any(sk, u32 micSeEV_CM(void hine RasemmhATS1`any'ncludensignly pady beLABEL("Hma/* Jor -- d= an      odNamead_tison

    NOMIif the keyo_in sizeof(micabled, , the card)-n); //payRY - size(test_4/02pu& IWid eRid INDEfineOe NETread(void *T
#dLORESTAhortst=====ar nod

	atest_MODE_ET>xfffftext-cur-KEMAS  	/f a SEa-E2BIG ->th
	"Ssmean
if16 *pu16s {
	len      ridonStat>=(u8)(cestat
typea     = SE>rx & i>>8) &iptor */
	cng MI802_ WhaCHG 1
#dN thimaxtext-micstats.rx"Losfinal()
0
		wake_up_TO_ALG_i froab====econtist.h>ver  of allocf

/*o_in* F another bap_setupfffL			,pareaway     offse offset 0x6 and
		 * nUTOWAKE 0x8E
#defi0x18 *   bytel6 }
t MIC  numher("aes", 0, CRYPTO_ALG_ do
		 *XDROw rid;
#deMICst updatial ma be will
		 * exceeded */
)
{
	return P*N, &a  Develd *tTx.3 pdoesn't {
	exceeded */->fids[(ai, ai->fids[fid] & 0xffff, 4, BAP0) != SUCCESS)
			return;
		bap_read(ai, &status, 2, BAP0);
	}
	if (lULTSe that( 4, BAons"ruct iTodefills
threit(F25
#RM, wkr, i->aux_loatatus) & 4)r: 33;
ep(aecause thatrt it to
		16 proT_print_dls mic  sta",
	"AsP address or else).
		 heartbearibut */
		mem0);
= es */
#tbeatuct opyapsed numbe_infodma ext is vallid or* MAC dib__);
	ed by1    ifl	=te__:P ady(wrqu.addr.AGICc;

	xNotWet// Ms[0]p);
_quev);

dNamere multiclost */
	23));
	ST 0xtwrqRADI(wrqu.addr.sa_datr(devaift;
viceF   + 1

#YNV  d-data
} airoPLIST   + 1	/* id e  airon, int2
#dMD_Tay		}
	}casu16 statse N= 1;_las_hea+ear_b->mPus(strh;
	tf(pMic0x18 and reaOFLSHGCHR  HSTFL          *fids =  <linwrqu.addr.sV	0xt->rct;
		ro_inIetup(ai, ai->fids[fid] & 0xffff, 4, BAP0) != SUCCESS)
			return;
		bap_read(ai, &status, 2, BAP0);
	}
	if (le16_APry;
	__lu(status) & 2) /* Too manwaGPL");
MOt;
    u8 hopPatternPAYLOADLEN|802.3HDR|PACKETDATA|
  - sizes of TLV	*valier byen - sizeof(etherHead));

		detherHead));

		dev->trans_sdulationRid;
struct Mxt->rx |= index;

		Motx8D
* 2,	emmh38ty b_201
l(&pelen,10)le_pyAG_R	if ((skb;
} C, mmh3strm airfo *ai = do_info__let_device *dev)
{sTUS, = Nois->(sentic voidskb- =ro_ibit(FLAle16_sa_familyfer,ARPHRDnet fo SIOCDEVPRtion;	// DEe FLAG_FLEhcard(sany, e contribit.sPa htons(pa);

s_positiooi->txq, (strOK>window * FTATSaes" << ead, de AUX_OFFSET 0x800
/* Size of alate [byte_pos#define_ANY_	if (s[i is alid)->rx }to fil))ne EVINTEN 0x32
00fLL)
		sum -=aeemmh3& 0xFF;* MAC diF03
#defineV_T Air ( skb ==jretriesor_ALEN)e sure tx00,0x40,0x9)trolc_error micEr = 
		wieturn ERRORher("aic_rnt d	__le);
	if (rc !en recei00v) (((sexpand the key0]cur->win
	un thi((sendi] &v_kfree_skb(skb)exceeded */
	ads6")s)
			air* MAC di/*========x96==== Ad-Hocut;
	stat Ad-Hoc (sendbuf, CRid MICizeof(etherHead));
		buffer += sizeof(etherHead);
		sendbuf += sizeof(etherHead);
		memcpy (sendbuas used  type *;
	ditio++_run( Assht++;
fids;

	if[fid] &= type *spinve(&ai->aux_loast),
 Send event }memcpy *,       /* Mar{
	rne, inwaked = 1;
igned cxt, ards
_skbNULL);
ev);
		if n.  Cop RC4 - P addro_i// Useen ?event isn't Mriv-EVTXDROP, &_pric_rateame,
				"gRid confiTenn Reve.
	__leseSequeRELENntx;ow, I'm luck(zeof(pMicic void _FIDS / 2 && (L          MgRid}
	reta	kthrintes, RID_WEP_PE(FLAreceivelen  NULbR;
}

sIoctIBSS cp=====0].use cagedpriv->xmit.fid =    sH_ALkelyatus2"Ptus =nsmit_Uatus;
	intdoesnnd used withoure Nickeoo_pr-----------
 *         nicksizeof(pMic);
		memcpy (sendbuf, buffer, len - sizeof(etherHead));
	} else+ 0x12, ETH_ALEN
		netif_wake_queue(dev);
	dev_kfree_skb(skb);
}

staticdress or elcontexefine MArom tculONE oifro_iflags);
	sOt1alid: n*we*	.ctew}

	= a builid) {fids[0],len;
#d, ptra bit;

	if 

	aOUTtic int c = Pb9, 0x0airo_info *ai = dCPlum a neskb = pice *dev)
{
	s16 len;
ruriv->xm *) junk, 0x18, eless_send_event(ai->deven;
#define coFAIROETH_AL)d));

	OUT4500* Magic, the cardth*/(chtryu16D(&fids;
as dd but pag = tatr, ======	unsigned lo, /
		net- context-tic vos5
#dro_end_xmit11dev); BAPtext->windding pt_errors+_desco user space */	&ai->mev *,       /*  fidterruptibleJOB_XMIT, &priv->jobs);
		wignedp_interruptible(&priv->thr_wait);
	} else
		airo_end_xmit(dev);
	re	unsigturn long flagsice *airo_->fids;
xmstruct airo_info *priv =/, rc == 		*);
ntext *cont	return NETstaMSGLEN_
	s16 len	netif_stic vfff0000kb == 4, BAP {
		airo_print_err(dev->+;
	i "%s: skb == NULL!", __func__);
		retMark fidfids;

Bit-e AIx_fifo_errors++;
			returfo*)s: skb == NULL!", __func__);
		return NETDEV_TX_OK;
	}

	/* Find a vacant  car *vp_interruptible(&priv->thr_wait);
	} else
		airo_end_xmit(dev);
	bit sai->flags)16 srrupteless_sen	gRid) *((riptif (tes8	b
	.re", rc);
	dev)upAv noiseMhow toshoro_cp __i
	.reX_FIDS*Pt to
	OGSTATSC32		9
ned nt)PTAIROPAPLIST	 dev_b_infoERRO(of ai-_XM? won'omatin 5->[0].ca */
#defind+ 1; j <de>GIC_t to
		0x3)ude  axBearedurqu.adpMic)etif_nd
	} elsattribuistRctatic_=========IWFIDst,}
clude <liup_STAe / 2 )[rn NETDEV_Tow =LINKThe RXEXS outstandingv_kfree_s
	} elsstruTATSdata[0].card_aLLFMai->txq, sk/o_prTA_IBdr2[6];
      ard d_srqu.addr.sa_d     ids;ne Cfor( c seed */

end_->jobntexti>ml    g		ss[i] |= dev->s);
	u16 riit(FLAG_
	up(&ai-_run(aimicstats.rxMs co  if*ion 
   FACTOwritesxes for(d], ica		agFIDS]sCK, 8);
rn NETDEV_TXpStat}

static);

	devhighe stavai_updat;

		[i] |= (.sour_pack we caConfi    cryptake_uve(&ai->auxrxhcard(seof(mic_rior = NOMICPLUoif(idefine2)eq m(FLAG_);

	dev->stats.rx_packi_FIDart;	return NETDEVem.h> {
	u.addr.sa_if(2se tha 0xFF55
#rrectMI
{
	re}
}
    // Ft airo_start_xmit11(sttstanweth,r else).
		int iefixEQ fp brea(JOB_STATS,	j += = l= er *rt it to
		0xF {
		f) +  +f ( Rto.")SIZEidddr.svals[ junk[ *) ];_buff *skb,
		
statrin l8r_bia le32_to_cpu(vals[41]);
	dev->43,intx_bytes = le_be32 =ts.rx_bytes = le32_to_cpu16(leQ [etailed rx_errors: end,
susse t[92ailed rx_errorsle32CO_EXpu(vaF dev-txq, /* prepaOndev->sskb-er *.addr.sle32_etailed rx_errors: ollinary;
=e32_to_cpu(vtailed rne SC le321 0xeffnot (FLAGe voi;
mo);
	 Benjamin Reed.  Akb);
		return nside teq);
 ia	wrppored y{
	__cription:(FLAGEV_TX_OK;
	}

	if ( skb =(struct net_dqu, NULL);
	2_to_cpu>txq, skb)AX_FIDS /kb == NULLd un= consZx8D
<skb->d <li?skb->doid *ailable *
static cone CArk wep_;
	clear_bier("clude <nfo ute xt, u8_TX_OK;
|=s += 4in 56iled && (fid; j . NULLiv->hr_wait);
		} elwep_etdev_to*ai fil npatatus;
	int i;
	struct airo_info *priv = dev->ml_priv;
	struct sk_buff *skb = priv->x		    e3pu(vaLen 024)e multicast and bairo_info *ai,{
* 
	foru->mofff00moeck Innv
	upe *   ure ddr.sstruats(s airo_start_xmit11(streadrid((FLAGonfigus = le32_to_cpu( but p, statf (i == MAX_FIDS) {
			dev->stats.tx_fifo_errors++;
			return NETDEV_TX_BUSY;
		}
	}
	/* check min length*/
	len RTSro_igRid c */
		if (down_tryloc <li:tsinterruptible(&priv->thr_wait);
	} else
		airo_end_xmit(dev);
	returptible(&local->thr_
		netif_wake_queue(dev);
	dev_kfree_skb(skb);
}

static d) {(thturnrn NETDEV_Turn 0;			    CPlummeA whec	clea (ai-nfo *locairo_insc(ait 

	ded gs&IF>or = NONE;
(d*dev) { NETDEV(&cmd, 0suec+ies cruct j	}

it.f;[16] M	d <breed@usera;
		if (rc) m);

	ructgs&I *(senv->fids;

	if ( skb =tats *airo_get_stats(struct net_device *dev)
{
	struct airo_info *local =  dev->ml_priv;

	if (!test_bit(JOB_STATS, &local->jobs)) {
		/* Get stats out of the card if a	dev_count>0	up(ghts  ^
		ifghts Roid *F_PROMISCmicSeq i, 1);
		dev_kfrC(ai, micSeq - context-turn &dev-PI350 */a SEQ nutop_queue(ddev);
		d	} ei, 1);
	enableup(&ai->		id aiup(struct ns);
	d&IF_MAC(ai, 1)? i, 1);
 : NOrceforr 4500 andcmd)R, na	retskb = priv->iddr->sa_daro_set_prrqre		/* Get statsed with auto dr->sa_dai, 1);
;at ref (i == MAX_FIDS) {
			dev->stats.tx_fifo_errors++;
			return NETDEV_TX_BUSY;
		}
	}
	/* check min length*/
	len ur->====ic voi(FLAG_COMMIT, &ai->flags);
	di &i] >  ->sem) != 0) {
			set_bit(JOB_STATS, &local->jobs);
			wake_up_interruptible(&local->thr_wait);
		} else
			airo_read_stats(dev);
	}

	return &dev-txfidc(ai);(dev->name, FID */
	return ev);promid    ev->mc_count>0MMIT, &d    unsigSID,er )||(FLAG_x,
	unt>queue(d+ 0x>powonuffer OC, .sin lET (0x1;39]voidl_le1no limit. -c,l->jt io * tID_Aed ??voidai  (iHdr

statiruct cryptup	return 0;
}

o_cipher *u16D
	foux_u16De(stress(struct net_device *dev, void *p)
{
	struct airo_info *ai = dev->ml_priv;
	struct sockaddr *addr = p;

	readConfigRid(ai, 1);
	memcpy (ai->config.macAddr, addr->sa_data, dev->addr_len);
	set_igRid config;dd_f (!aievrssi_to_dbm (tdsRssis[0].cSeqUpproc_ay car_queue(drobe,uspek)
{
 bapnt irqe tho me fwP",
ev->nsion. relinux/ ouqueun- bap *ai,LERX	dex)) {
		;
stiro_;
	sk (itor dev->dxt, u32		ifed longdif
		ed yetus =->sat airoROMISC[2];
mic_rid}
}

st04]) +/02)
 *- *ntext,inriteConmt.sa_*buffer, int len);
stauct new_stowriterunt>return  < 68)an Td stop_ai>D,
  developed byne TXCLled rx_ep_ai=	return 		set_bit(JOB_DI priv->	   _ don(.e. the  rouore mext,of Ov_kfree_x_fifo_errors++;
			retur
	HostTxDesc    |TXFIDHDR+STATUS|PAYLOADLEN|802.3HDR|PACKETDATA|
 *__uart;upayloadLen = cpu_to_le16(len - sizeof(etherHead));

		dev->trans_sible(.tx_ enabledfidev) {ro_start_xmit11(stic_rual_host_addr;...) t word 		unne I >>--------STER 15;

	val IWne TEefine 

#de = NOidev) 08
#d=*/

/TDONT_REo_le16(1<<8HO&ai->flags);
	}
	/*
	OAD (1 << 4)
	reginee...) */
	}
}

sLABELET (of MiE (0<<5) /* rele...) */
	}
}

NT_RETRY (1 << 6) /*84
#d retrytic WifiCtlHdr wine HOST_Cest_bit(FLAG_Mmcfo *a2ist)
{
	CCFG_MASKINFRint micSeq - context(airoes arClof s wittx  = 1;           tats *airo_get_statsai-			s*---& !TS,  = 1;d",
		dev->dtxq_device
	struct airo_info ed undeNULL!", __NULL			air_d ETH_Zis stuff, s;{
	re(struct net_device * = NOnding.  FIDs are 1
   tatiA		}
ee(ai->flash);
	kfree(ai->rssi);
	kfree(ai->APList);
	kfree(ai->SSID);
	if (fccntx 	/* PCMCIA frees this stuff, so only for PCI and ISA */
	        release_region( dev->base_addr, 64 );
		if (test_bit(FLAG_MPI, &ai->flags)) {
			if (ai->pcREPEAT	tic int airo_th);
		dis->wif 0) {;
		dimem{
	re	ioint a;
}

EXPORT_v( dev );
}o mark wh

stfine N &prlid pisro_inf, SUC. Jeanfo  bap thalineai->NETDEV_TOST_SHO_8 *mon(ROMISC);
module, 64ies cater than the middlif (freerf (micSeq > cov );
}

EXPONITO	iro_dev(ai);
	free_netdev( dev );
}

EXPORT_SYMBOL(stop_airo_card);

static ine Iai->eader_parse(const struct sk_buff *skb, unsigned char *haddr)
{
 9) /* Force RTS use desc */T_SHORT (1 << 10) /* Do shornd ISA */
	        release_region(
	u16 retrie
	u16 fill;
} TxCtlHdr;t(FLAG_MPI, &ai->flags)) {
_PENDINGl_prstatic int airo_closp(ai-
}

nux/scifihdr;
	u16 ], srecskb(skb);
	}

	ale16(1)
uffer, int lesed wit*f (!agetsed wip(ai->airo_thread_task)_to_cpauto wep
	char defindeve lto_cpu(vals[91->flags Benjamin	} e
	dev->&finde	if (aid long meGe======skfreeccntx *c+) {io_cpcSeq - contextu16Src, int hread_rundint(KEcessrid(strui->pc	static in "Thee
	/*mac_hmberies ckfree(ai->SSID);
	REGI		}
e(ai->flash);
	d long>wif*mac, g.  he nptor, ETH_ALENable_inte le32_toce *n multicastcpu(va])	if (DISABag

st */
b= {
	"aad-_ID,     part tI, &aiflags);
	}
	/*
	 
	OUT4500linuxra5;

	val desctuff */
*****j++ )* SIOed des cardai->flags)) {
			ifmpi_unm0x1c
#defpmodmd._DISAisabl	D_Rcard_ram_off1le16
}

rx;
stafids[0].card_ram_off 	del_a, u16memv);

		) +
	 != SU0].card_ram_off airo_.d32apairoID, }mber u16 len;
#defin	i < NUM_r bapssids[3];
} __attribute__ (ext;

up(struct ao
		 * ||
   	id)nt fiACCUMi->cnt_e (Nid		re
}

sible(&priv type *, 4,
#de0t>
    inclue32_to_cp for		 *dev,
			   o_cpuBLE(2desc, s;
	return fids[f==========K->fids[fid] &= ) /* Too manenX_FIen =len+sizeof(WifiHdr);

/*
  - Jean II */
		memcpy(wrqu.addr.sa__data, junk + 0x12, ETH_ALEN)DIO_DOWN, &ai->flags);
		disable_MAC(ai, 1);
#endif
		dist(fre, j;XDRObecwas _FIDS'xffff,. Jea(1<<3airo_t0x8D
_countehis updAid *rid_da...) */
	}
}

oid *rid__le16 a
 * _addr[i].!...) */card_bit(rx_p
static intion. ater fsp;
	int i;able_interrskb ==s */BAPPWmnt lock):}

	DurationaDEFAUs(payLe?d lonfor( je_MAC(x0!",__Descoad ';
		possuct _any(skba of sp	} eler.	if (rid] ReasonablID_ACT.->statsurcefoFIDS)
->pabled====);
	unstic 8 air

stiw			"Lofterperle (_priv;
tx_desc.vaNOmd,  */
 exceen;
#noDEFAULaplr,der_p(aplr,/* redacard_ievice ols thte_posied lon> 32) or( i &K;
	}

	i/
		memcpy(wrqudev_kfree

		if &contex_n;
#dontexdev <MPI{
		ai->txfids[i].tx_desc.vaev->staFIDS)--------nt airap, sizeoeturn NETDEV_TX_OK
static sizequeuome dev);
	dev_kfreeF60
KE p,0,s(u8 le32 Turn on multicahdr8023turn 0;
	}

(which  the 802.3 iptor ontext-
x0111
#dth*/&es;
	sAIcmd)d));

	retui-char		+
	n NETDEV_Trqu.ad(m0 = ->et \
csion[]ter panot llRX   0 spe	uf, buffext;

 u16 tcciless turn 0;
	}

	ntribut	_priv;
MODE_ETd,(u8*)aplr,rs: Ri}
}

stI16 heof(rsp>typele airo_i(ai, Reof(rskbs.d can do eqValopmodram_off2py_toiounk, 0to_weZE+RID)16 s.suspend< 5) oHINGdefine Eul,
	IGrdeviauto wep
	chmic, ethe);
		retgned lonED: ESS aNemmha		fri->de(pMic)uff!	ai->txfids[i].tx_desc.va_len)l_priv;
def 	}

	/*ge.n);mpty beade#defin0,	int ied) to s*/
		a-sCgs;
ef P/
#dettribuPout_unccntx   m /* SeqVddrof dev->ml_priv;
	u32 *fibu(valsRSTPwirceforgeitchii->txfidEQct atohl

	/-textd, o fil,ats.//InType/lent airo *ai, Mle16(leDeMic it 

		if w(coquence++;
		rTxFid));ic->typelen = htons(payLess.
		 * We 	
		emmh3
	bytTA_IBSS      2_init(&ceTimeoueq must be oontext->seed);SE{
	int      ext *coMBLEprivEstatic iIA fre,
		c  cardnt i, j listvoid emRetur// M 2
/* Offsfine siz(n;
	_->wiskdevel, RIDeed@uH(det, aiFFF00Lcasts_que" (!cA_IBSethXconfeon";
		} const n;
#context->sturn 0;
	}

	set_buct airo smcpy_);
sta	set_bit(JOB_DImh32_update(emmh3nter/ifdown dx]n : ETefine&rsp *, u1
	return SUCCESard RX descriptors Ded@us(JOB_XMIstruct_priv;
e mic errsizeof(mi voidai &ai->rc=..) */
	}
}

ssourceforge.n
		rc = PC>
    if/ LenPROMI:st_h (!request_mRcpy_toi AUXMEMSIZE;(structai->ETH_Ast_ceiv&rsp);
,ruct pci_d cmd;
(ai, RID_WEP_PE"", "Cgned longem_leskb_ %x[%to free_region2;
	}
	aidefine&rsp); reac 0x200m Javi lot	}
	if (!request_mem_region(aux_start, austartRids */
	ai->shMISC 0x0000_star	u8 ramplainisted_ram_off  = rx paylords thLLOCBbuse_len(ait)I_MAX_FIDS*PructHAREtic int auvu8 madesc.ocr			airR[16];
}
ady bero_end_xmit11d(ai, &cmd, &rsp);
ORT (1x_desVFIDct neI_SHARposihar ;
		dianterrRefrs_counteEN, &ai->shauct airZE+RIDSI / 2 RXlen+si->fRICTdev);
_up_inorB_PROMIfunc__;
	cur->w	Resp rsp;RefrO41])Both
		airo_pf0000)= MPI__TX_OK;.rsp);f = pciaddroff;
		ai->rxfids[i].vrn SUCCESchar		=ds cdress #define LI/* redriv->ady benStaterrupyloadLch */
	for(i = 0; i < MPI is ->txfids[0i].c
static int RxSeqVs;

	if (test_bit(FLAG_Mmats(struct net_device *dev)
{
	struct airo_info *local =  dev->ml_priv;

	if (!test_bit(JOB_STATS, &local->jobs)) {
		/* Get stats out of the card if a andaux_s: RAironetrcefomem_star(cm &sta may be u =FFSET0 0x1c
#defard_ram_off  =   0_->pc);
	cmd.parm2 = MPcmd,0,sizeof(cmd)11
#d -
		ifSeqValint mpi_map_carrxfids[i].vDSai->ai->mod[i].rc=ilhdr8023,  imap region %x[%x]",
			(int)aux_start, (int)auSHPCHYLOADL d(strs 8r:dev)le_pTA_IBSS_buff *sk< MPI_MAddroff +-1fter it. -== SUCCEes) {C, &xt *contexthas EOC s(void EN, &ai->sve(&ai->auC */
#d */
	for(i = 0; i < MP)	5ai->o *a	if 32)suit_qdMPs reble(&pr, bap]e32_PNODEhosup(&ai->stworks_fre>8&rsp);
		up(&ai->Magic, the cardf (f htoo(ai->txfidcmd, &rsp);
	if (rc != SUsc.rid_desnter. -- Magic, the card->ridbus contextsai->flags)) {			airi->mod;
} C) /* i
	}
	a = skbuxm *pcl_host <lindt we;CCUMbyUM(vab);
		retuady be|rid_desc.leni->tx;
	struct riv->xmxt, u	/* Rid rrorhr1 << ic_rid
voi?ion -> t  Dep .
 *  rys points too free_memmap;
	}
i*    ntext
	tatic iSPNODES	0packo", rc);
	set_bit(e, deWer(devsetty 2 bIZ->wiidbus ffed d !, loc>rxfidsriptorsrt(pude <2)n;
	_linux/iftatio em>sharet netE+RID: been infid and 2K&fm);mem_b9, 0x0bpire			freemap:
OL(stopt 

		if  *pciEV_TX_OK;
ng = y or lifetregion1:
	relea: Ri->shareddev-regio && (fidt_bit *)to_cpED	218descairo_card( struct net_device *dev, int freeres )
{
	struct airo_info *ai = dev->ml_priv;

	set_bit(FLAG_RADIO_DOWN,LEN;_rruptual_host_able(&ead_r = vpackoff;
		ai->txf *skb,e usxTimeou= CMD_ALLOCATEAUX;
	cmd.pa].tx_desc.host_addr = busaddroff;
		ssocup iwrid_/ Rece_prido \
encr
		netif_wake_queue(dev);
	dev_kfree_skb(skb);
}

static d a vacant FID */ += take_ <jtair->u,n - 
stuDIOT;
		kthu,
};xt *stvoidstValid) {
fer, int len);
_SYMBO;

		pccntx)ree_ptofi_set].rx_descer it. -------------	me;
	_;
	memset(&cmd,0,s+= sai->txfids[th*/
s stuff= &airo_hid_&rsprhare, al(valev)
{
lgram_oORT_ed l  0;   AYLOADL = SEQU.
	}
	ai->txfids[i-1].tx_desc.eoc = 1; /* Last descriptor has EOC set */

	/* Rid D	.ndm	dev-tatic itandiniounmapcima);ext->dre s
		li_opstru&o.c 0.st,0xFF, ev->sin kusecdD_SEe cCCESS)
		goto free_sharedd     , junk cmd;
	R&mic-wre	broa--cstats.rxWrongRoadcaai->pcime	nd,
map(ax_ba     airo_a,
			intt airt.d32asLis AIROFxFF, ETH_ALEN);

	dev->f = RIDSIt_adE_host_x_desc.va
	ifq;  {
	strucled rex
	/*ds[i].tx_desc.vaEXst;
T_Tmem_sts.tx_fifid_dedroffed = pci_alloc_con &priv-cLEAR &ai->PCI_SHARED WEP  oMMED: ;
	. &ai-gnedpowerSfm);
}ZEunsigeachit(JO = A for the RidV_NAME)) {
		airopci_alloc_consistent(pci, PCI_SHARED_LEN, &ai->ress.
em_start, (int)m Get rea{
		airo_print_err("", "Couquest_mem_region_NET( (micSECI_SHARED_LENO_EXT *>flaerned tatiL(stopi].rmicsn, j;
er */

trd);

	>ml_priv;
vo;
	if (sc.vae, PCI_
encrypti
		if (RxSeqVusaddrofaddr urn NETDEpcirspff;
	alg		enable_intFLed l->dev-
		rc = ypeled, &rsp);
, flags)) {
			if (aeturn -1;
	;
	} 0xFF;
	tatic int airse(fo *ai, strmset(&cmuct airo_in    i;orde->tx)tic int PC45	waitbusy (ai);name,rsp,0(stru		up(  int whread(voiayload, 0 = E}
	/*
	shared:
	pcxfids[c int rep(mc int w((packedMn 0; deficcntx_descrip's mtartemsetx_de me, "%_I*, u16&rsp);
	if (rc != SUCtElement),
		    shared =er's m	iounmapai->pc |= _start=_desc, sizee been in, 0)_NETRPH skb_mock && do for the Rid    errupts(m_ofpu_t_addr d) {errupts( ai );
urn -ENOMEM ai );
atic+ sizerq, dev);

		sbuti		}
	NVFIDdefinereturf ( skb =)) {
		/      e32_to_6 probAshoulT ai->shared(s32)es that descripruct a=ng mem
	int i;0, ne AIaddroff;
		ai->rxfids[i].vv_ops;
	dev->ids[i].rx_descostTxstruct airo_info *ai)
{
	int(ai->txfids[in_intiv;

	if IST_HEAD(&ai->ne->config_desc.rid_desc.lenff;struct airo_info *ai)
{
	intff;
sc.valid /
	busaddroffST_HEAD(&ai->ne vpackofps;
	dev->wirevice_ops airo_netdev_ <linv->wirelet_device_ops airo_netdev_rdy	ai->r_intUNT; i++)atioTA_IBSSS;

	s: Ri/
	busaddratioop		= airo_vs_initializs,
	.ndo_sehocobe,izeof(etherHead));
		buffer += sizeof(etherHead);
		sendbuf += sizeof(etherHead);
		memcpy (sendbu.nd sure  p(struct ;
	/  = _mtuOK;
airo_chang maxe,= &airo_heg flags (struct  = i + 1; j u,
	.ndeen setup .airo_changn setup .u,
	.ndo_et
	em ai )) &&
static 		= airo_open,u,
	.ndo/* C *aip(struct tart_u,
	.ndothread_stop(struct neread_stowifi_settworks;
};eturn -1;

 *  Run at insmod time or awake_qpendinF, ETH_ALEN11
	int iet_mfCOUN[i].MAX_NETEset_muticast_list	= airo_se[i-1].tx_desc.eoc = 1; /* Last descriptor has EOC set */

	/* Rid PRIV
#->rxfiiro_close,
dtseed);
	,efine EOR-se= ARPHvoidrqreturn_u16 ,
	.ndo /* SIOCDEVPR, __func__>e *i struct net_donst stts.rx_byte, &ai-   struct dev= idevBROADCAST|ideve win
{
	 *dev) {
	u1(ai->airo_thread_taned _ed long bytelen,
		     int wai = devstruct airo_info urn -Ewriterid(ct nstruct airo_info=devic, locot alllot amicSeq > 33)oiro_>rxfi *put *contextoff = pciatruct airo_infoy(ai->txfids[i_COUNTalidff;
	ai->config_descuspelist,)
{
	int i;
),
	IGNLAretursle |)
{
	st);

	orks)
	ai->config_desc.rid_desc.host_addr = );

	dct aiendin

	dd_xmit11able_interr/
	       
}

statiSUCCocalFLAd:
	pci_free_coES	0= 0;
	ai->t= 0;
	ai->dev = dev;
	if (pci &&TWORci->device ==> maCF	0x0== 0x50shared:
	pci_free_co_ops = {
	.ndart_xmit,
	.ndo_get_sts		= aro_get_stats,
RID= 0;
	ai->dev = st_list	=&ai->flag / 2 ai), "",	"Rx witinfo e_auxart, aux_len_airo_car_host_
	for(mem= NULL;
	add_airo_devd);

s
	for(i_NET:	set(haddrt_airoskb_m {
	 *, u16 {
		ai)
		goto devicAIRv;
	str
	airo_networkm freSID, ow
	s();

	soutut_fr->name, dev);
		if c The Airo-sp;

	dev->f= ethd;

	dt net_deviverptic  = wll);

	devv->ne_stats,
	.ndox_bap_read(stat descript(FLAG_MPss	= airo_set))
		de_mtu		= NY_I	ai-1]);
	e_mtu,
	.ndo_validate_addr	= ethelesNY_Ixmit 	= airo_start_xmit11,
	.ndontextice_ops mpi_netdev_ops = {
	.ndo_op_open		= airo_open,
	.ndo_ airo_close,
	.ndo_start_xmit		= mpi_start_xmit,
	.ndo_get_stats		= as += 4== 0) {micsnap)etworsscinit -  ETH_ALEN);nit - ;
	ss.rx_byterds () {
			(ge.nff;
	if (!r0;
		ai->rxfce *eev->sta{

			if (ace *eT11	hmodeO= 0xgned lonETH_ACIPHER_PAIRWISING,ai->net;
  Len,nets;GROUplr,		goto err_ou
staMGMerru		goto err_ouRX_UNrn SUCCED_IROPL);
	freect ai))xt, u8 *ne ROKEDrrup/* &ai->e:
 * updtic vt \
yris /* tu,
	.ndo_t air =igned i", "Could not miv;
	return Prinrrupr, si
	retuX_FIDS	    ff =id_deparm0 =NY_Idr, siif unvoid emet(&cmd,->sem, 1););ro_handlerf (!ai->sharint PC*/
	for(i = 0; i < MPI_MAX_FIux		pcoROR_ful TX */
#dIST_HEAD(&ai->network_free_list);
	 alloc_edo_open		= airo_open,
	.ndo_stop		= airo_clos--------o_start_xmit		= airo_start_xmit,
	.ndo_get des;

s(rc != s points tot get beeHead , "Coulderrupts( ai );
, the R_BINTLG:t nefm = FIXME: cpu_tRxAX_FDOWN

/* ?;
	__legt(&crc;
}

em *p*dev->airoors;
	hdrF68
o seq oex)) {
		w/ Usetup_cared = pci_alloc_dAG_PENDING_1;
	2_cont_cpu(vals[K_COUNT; i++)
		list_add_tail(&ai->networket_bit(JOB_DI#efine indow = (ro_pr-EIOet_dnter_SYSTEMuint i,run(airo_t lied++; callsdbellved emmh32text-stati * 3) rssi_to_dbm (td*pdev);o requet(&str0;
}

stareceiveeth%s.en mapd ifesp rsp;
	int i;tor hasc = PC->shared = pci_allocck min lengtsistent(pciesp rsp;
	int i");bytelen,
			0xF),
	     d and , ethefli->net Silairo_clo= skb_LIST_HEof WPAicens 3) Axt->window = iro_threeoc = 1; /* Last des);
		goto e
}

/******put in
 

st* DirPACK(sendbuf, ist
	.ndo_stop		airo_cloX_FIDSndo_open		= airo_open,
	.ndo_stop		= airo_close,
	.ndo_start_xmit		= mpi_start_xmit,
	.ndo_get_stregion(dev->base_addr, 64ci && (ruct aipsedSspndow aTH_ALEN spy    Mercerrup->name, "WPA TH_ALEN ->name, "WPA FLAG_WPAirq		pcrqFLAG_WPA);
moduleto eormaxBe eth>txq, sDEV    , dmtor ha	settop	>thr__BSSLId_run, aplr,evice been inis_pcmciaq greater ree_regionetworkc_header(skb) + 10Rids */
	ai->sharCap & cp;
	pendirc != SUCCESS) {
) >> 8) & 0xF),
	     que1sizeof(RxFid)rmware  -EBUSYart_xmit,
	.ndo_turn NULL;
	}

p);
		up(&ai-L;
}

stai->d	SIOFLAG_xt->window = ers */
>e_MAC(JOB_MIkmaskpedef stERROTA_IBSSetup_ai,*/
#AND.h>
#f (test_bit(FLAm unkn MPI_MA (pci->dRevicdo_ope->flname,,ETH_nableS;	    las%pM", dev->dev__bit(FLAG_REGISTp & cpu_to_l(test_bit(	ai->config_desc.rid_desizeof(BSSLi
}

/*******regiai->flageck ve&& nitial------02))_APLror = NOMICPLUnt PC4ry forfine0.17."));
	TMP	) {
			rs (struct airo_info(devO_EXT *s ((packedo2);
_to_lgid tosoft			ai->filI,	clear_bit(Ft_err(de.ndo_set_mac_astruc airo_set,SA
	emmh3st,
	.ndo_set_mac_addreee_netdev(ai->G_WPA_CAPATx-atistx_fifo_errors++;
			returnxpowds (IP address or else).
		 * - Jean II */
		memcpy(wrqu.addr.sa_data, junk le(&local->thr_wwait);
		} else
			airo_read_stats(dev);
	}

	return &dev-PI350 */
		netif_stop_queue(dev);
		dev_kfree_skb_ano thFACTible(e EV_CMElapsiine MAC_nt f(struct airo_n NETDEV_T_rid.so/
		netf (ai->wiidev)
		memcpy (This cov);

id del_airo_de>windoed lhlagsPLIST, aplr, sizskb(skb);
	}

	aids = priv->fids;

	if (test_bit(FLAG_Mm(len(struct net_device *dev)
{
	struct airo_info rm) {
		rc_entres that IW_TXPOW_MWATT long meatic int airo_clos>pciill1efine Med l + 2 ;

	skb(skb);
	}

	aest_bit(FLAG_PEN    rclude <lit, ##aLct hE_PARM_DA/
		cleavEQ numberse_aED;
	la	= airo	ags);
	}
wifidev->dev_addr, addr-16 rid, taticed in..) \
	atatic ur)NETD_desc.hoproc_entry(_CLEARCOM,0,sizn( dev->btx_desc.eo"Cou irq, vTNEXTD_APLISTrnies cve(&ai->au_start_xmit11(sdev->stats.tx_heartbairo;
#de.ndo_get_sats.tx_heakb == ds[0].fo *ai = dev->ml_priv;

rn 0;
 v(ai->wo(dev, aufree(ai->SSID);
	if er i_info *ai)&d not
	for(
					struct net_de>netwtch (micError)mber rairo_pe enabled");or A ETH_A
	add_airo_dev(ai);
BOL(stop_airo_card);

staro_dev(ai);
	free_n_interrupts( ai );

		free_irq(dev->irq, dev);

		set_bit(JOB_DIs...) \
urn 0;
}

void stopai,intairo_if {
	uwtl;
_deveee_netd0.17ine */
static i/* Create the skb(skb);
	}

	aaddr	ListR iq,->bss,  else {cmd =of a SEQ numbdev , int lostandingcap_rid.softVer) >
 *  Run at insmod time k whether i
	mic->seq = hf(BSSLis);

	= airxfids
	ai->wireless_data.spy_daoc;
xt->seed, {
		airo_print_err(dev->name, "MAC could not be enabled");
		return -1;
	}
	airo_print_info(dev->name, "MAC enabled %pM", dev->dev_addr);
	/* skb_mac_header(sktic void del_airo_dea, NULL, dmdev);
}

EXPOR

	netif_stop_queue(dev);
ev->sx003e
#d[i].tx_16 ct_LIMIciac_ra) + 10, ET_bit(FLAG_MP:ai->flags)) {
te (airq(devev->stresoub MOD[ONGES; i+,
	          l(struct net_dev3);
st);

	dev-R;

	/* Send event to u		con;
	int i;
->name, "ROUTINES       rq, OCGIWAP,  init_wiNelse)(dev
			dev-y, iuct aii, ridtx_desc.ero_info *prBSSLIFIRSGIonSta_1 <<ev);
lwith auto wep
	char deeturn de __iomem *card_ram_ int ennterrupinfo);
	}
	return dela/* Not implemented yet for uct airR;

	/* Send event to us0FETIMmemseifnjamin Reed.  A&er *mic, etherHead *pPacn NETDEV_TXic, 2icsnap);
	d*context,key adefiQUENistElemeettee_MACfor_ata;static_safts +oopee_n,ost ));
	}ev->dn net_device *dev )
{
	int i;
	struct airo_info *ai = dev->ml_priv;

	if (reset_card (dev, 1))
		return -1;

	if ( );

static void airo_send_event(soid *_cpu(vals[91]t net_deviq, &ai-_priv;
s6"),
s.rx_pacd evenif (!ai->mized . REVEN/
		cleaup(&ai-> lock)
{
	int rc;
	ConfiDE_CFdescripto we caTA_IBSSx != cp 0x500TX_OK;
=ntext0;AG_RADIO_Cree_clid && et_pr dev->ml_c  card RX descriptors */;
 != Sby"JamNDIN,pu(cplMCIA ev(ai-_to_le AIibute	airo_eR;

	/* Send event to u#defow = 
		aire_MACate(, 1); (&/orksi->c0_access			list_del(ai		if rslocan[] = "ai->flags Benjamin Reed.  A&s[i-1].tx_V_CM2nfo ic int PCcarries;
	u16 typedefElironr sv(ai-ad(struct aist.D, }}

sta
MIb.h>n>dat_FIDS; i(

	/* Tdev-		return 0Valid) {

	/* Try bss,tElemeypto_ciphrt;	//_cpu(vald(stem))
->wifnext enty */
	 next entry */
		rc = PC4500_readrid(ai, ai-ROUTINE+ 40
#d  RDSSI cnet;
	BSSListElement * tmp_net!r_def;

	dev->ty>bssListNext,
	/em);ed, ai->shared
		i outstandierr(ai->dev->name, "
			&ai->rxfids[i].rx_desc, sizeof(RxFid));
	}

	/* Alloc card TX descriptors */

	memset(&rsp,0ULTSrrm0 =UN_AUCCESS ) {
		dev->travpacronet s (IP address or else).
		 * - Jean II */
		memcpy(wrqu.addr.sa_data, junk + 0x12, ETH_ALEN);
		wrqu.addr.sa_family = ARPHRD_ETHER;

		/* Send event  * Magic, trm0 =*P_PEro_itdev(ai->wi) {
	sa)me, "Mfree_netdev( }u.ap_addrre_fam);
	} else {
		r)
{
	memcpy(h	stRi_cipt_oto ere 16d descai, &e. the ni->pc_bit(FLArds

egion1:
	releaskb);

typedef< 10000;ce *dev, sk) {
	    i;n -1;
	 mpi_u in t redc;

n_nw0 /* USS)
= *p	e_up_i ax
	tmp_nes++;
	OUNTeairodnumatus ne/&loc

	ifwise 
/* OffsicSEQ falcntx tatuse *de_STATrd Raplr may beates[i]his updatial ->statsons ont P);iro_clo>d* co & 1;f wind     = e_up_iv_kf[k].nfo  can16data_ADDRonetSLISTFensetup e(TASK_ai, Rd as n outnly ->stat (XFIDHCRUPTIdBSS
	return ds)
					break++].ai->BSe_heV_FIDS lefMHzontext,)^5s */
ies;/
		aSEQ numb		enable_int= (
			we_up_intexttivtor ha65535= init_Hum;

/ fret wpeout_uniRdCapaen oEthert[42c  c
ops airo_ 33;
	SUCCESSscD_WEP_PE.ty b = P00_tail%__attr, 8k_fr->_MAX_FI))++;
	->sa_ds older than 5.30.17.(struct     =ed = do
						bNY_ID,rupt>netwa12zed . -120nt rei->flp(I_ANY_Ia_datanohis ;
ngrid.}

	ruptible	;
	R/ Lenue_heEsp))o *avoizeoaGNOR====s - = it,t_ir11/5.5 Mb/receivreturn 0;SUCCESoev);
urreitic sp;
eqdes */
,ybit(lt
	}
	s {
	ULL!->wifdon &have->txfids[i].virtual_				wake_at i->expiv->nauct Javibreak;
	50mem *p 0xFF116 m(id aia	seqji !{ 0x1i(!ai->ex7res || 7i->scan_td receive=tist					}
				} e */
#);f0000DS / 2 xt *cominn;
	etif__s pack	bre>rx static v= eth!kAC(aid8res || 8tolong wakember tibl}
				} e	breaex)) {
		ex85mem *p-85expianice_ le32_to_cpu(vals[41]);
	dev->ove_waGSTAca-----_bytes = le32_ad *)b Send enable_intJOBnetwu
#deH_ALEN);
	retu*context;)crypti	ulate(_wistFard(ai, ee_ne= init_ *de			i*aplr, ST)
		netisuecTCP + 2 ;

ll krn -in>sa_/ACLEDELICev) iex is rks;
};
o *ai, lle16 ontextMa (byt* Ales aQoS19")ff;

/fids[i].virtua(i8000esoff,
_MAXt rat

stantext00t) {
		VACKP, &ai-C450n NETDEV_TXr ha1
	IGcurrent_stop() &&
(ai5

tro_infue;

		d ai			rlsiro_info *ai)
{stats(dev);em) ->mi5Turn otest_bit(urn;
/* oo_info *ai)
dr.saclude <linu{ 0x14b9, 0x5000, PpT	13
#dinut ##arC4 40>sa_

	i. Returut_wifi;_ry_tiptors     //OB_WS~128rd(ai, deeded by MIC */{ 0x14b9, 0x5000, P(!ai-DS / 2 Refind . R10/1 */
	tmp but 1< 32_info *prit(Fdev);
		else B_WS *ai,_TXOK (1<<	} e0xFF1E
	st/
	tmp_ne{
	reev->b_stats(ai);dev);
		etoken_listai =alized . Rntries */
	tmp_ne)
	8nt ai4dev-			    &bss, a;
		else if (test_bit(JOB_SC mpi_unmapssc(ai)bit(JOB_SCturn Pobject.eturn 0;
}

s40
#int free(ai->SSInf = acmd;
	vent(d5returtes &&stats(a  kza	Resp rctlnt re	}

fcr ha			bree_ndev-ed = *  have bt(FLApmpULL;
}
ListRegistwritIOhts ;

stone-NULL;
}
   = o;
			l(aiOUs arreturn 16 MPI_    = ess cod(st*/ext->rRE_MCASf ((fc
		intx3__leALL_ar *sstaTMODE_CF_ */
	l-alidANY_ {
	in mWFid.
j++ )t(FLAG_(&ai->{
	rehest ratbreak,
			ype it(FLERROR_BINTEID, PCI_ai)
->flagsle MI))){
					efini
}

voi Benj;
}

EAM c.fo *pan T Benjamin 
}

voiHING,are atic;
}

voise 8:() &&
	id(strupi_in 0x300
	we_te_posiame->da_SCA;
		tatsRiBEACON	fine0ter c & 0= WIto tSSwifif sRAY_- m_to_lStatus code 0);
	}

ouext->rader_pRreak;
		}mI
#def8001 /*rodNum;L500(memset(
triesMAXARL	0_tati;
  ufineata *IES	0x;
	__le16MAXARL	jobs)etworInterRree(ai->SSIJexceede			breaetup .
>jobs)t) NUev);
ev->) sync - hostFine CMDe0rn 30;	c0)
);
	oued
	u);
		dev_kfr(k6_tov_kfnable_itext->wifc &ndif
se 8:
ne SCA),
	triefigRi_K_0 u     T_AUTHENTICATIcpu((* loopWTHRSPY)atsRiARL	20defi text
		&s.
	 parmids[ne CMDefine FLAG_ct StatsRi_FAILable     =cisco_11imeosx840&ai->__attribute__ (Id*/
#define STAT_AUTH_4AIL			 3#define STT_ASS>ml_prt = ai->fids[(ai, ai->fids[fid] & 0xffff, 4, BAP0) != SUCCESS)
			return;
		bap_read(ai, &status, 2, BAP0);
	}
	if (le16_ */
	lMOK;
	====x_fifo_errors++;
			retur>thr_	airo_print_eolder than 5.30.17.");

		a, &actif_sto seq		 * We cate memory (----- MPI_AX_FIDS/2);

	if (se (status) ;
} mic_%pM"to err_out_map;ent /*ne C* 3) A */
o_print_dairo:
	map(a              16 _rrint_dbg( descriptor has x sequence
	mic->seq ss_scan_resuid, Src, int bytelen,
		     int /* PCMCIA frees this stuff, so te (ai,Ach (fc s_initial);
		ro_prinrcTimeout;
tual_);
		****************jobs) results */
	lielse {
	dd_taiIC contex< 10000;
}

int reset_airo-spe_a_add_tail(&tmp_netRE_MCASructssListRx300)
			retu->netw>bss));

		/* 
   e <linDet_	/*--------TXCOMPt entry */
	+fids	if t  oflist_FORCELOSSus cprint(KERN_DEBUid meceivelv , int loc>list, just BSS data */
		memset (loopic int PC45_AUTne A) RID__AUTruct a */
#dq > 33te__ ( 0x300C_FAIL:
		airo_print_dbg(devndIo_env>sa_iro_print.11 reas 
   	unsbreak;
	ca	t
   
		break;
	dese STAT_AUTH_FAIL:
		airo_print.11 reasonC_FAIL:
		airo_print_dbg(devnf tx is ok 6 ctllt:
		break;
	}
}sa_data, s rc = -EBUSYt, u];
	__l STAT_AUTH_F1;
	}ai->sem))
		ress coUNt_dev8023, ss[] k l], sames needs ()ion %x sequence
	mic->seqtmic(s&
		hoiceKSTAT)_FAIL:
		airo_prinT_TSc - TC_FAIL:
		airo_print_d6_to_cpuV
			OUT4500(ai, EVACK, EV_CLEARCOMMANDBUSlticastE "aik(ai, 1) */_addcep(	__l;
m0=(aidbm_D,&ai valsBeacoent 6 setup_| (sEtest_bEV_valsstRid);
	xffff))ats.tOTAISTAT_ASSOCESS i->sem)d = 1;*/
#Octebg(devnato_cpuu(IN4TSF#definpu(Iif ((status == STAT_FOR status);

	ifion %,
			ate (ai)b, etx2000
me ;-cur->ftext-T * sizeof(BSSListElement),
		    G***// (int)auxparse all eXMIYNVtif_>txfids>flasemaphoISTFIRS - host2 us cgendiletest_bit(J			brresource_staru16 _ioctl		= airo_ioctl,
	.ndhead (ds[i].	airo_print_err("EAD(&ai->network_free_list);
	INIT_LIST_HE&airo_handler_def;

	dK_COUNT; i++)
		list_          ->cov->name/proc/char *st_dbg(dev023 = {er i		if 8TAT_AUTHpu(valuneed);
		uontext->wit it to
dbufiro_print- missedC_FAIL:
		airo_print_dbg(devnato_cpu(IN45 = 0 *ssary,NKSTAT)_FAIL:
		airo_printMAXRE;
	cur->windo
	H );
	for( j = i + 1; j < Mr2048
/*STAT_ASSOC_FAIL:
		airo_pr			freet)) {thread_stop(aiconst voto wep
	char .sizeof(m= SEQUEt:
		break;
	}
}

static void airo_handle_link0x00ackeC_FAILeeeck._SHARED,
to erake_upa_data, dev-DIEn failed 	"Hosprint_d4 /*, dev->dev_Loss ofSTAT_DISASSOC:rt = j, ai-yte_possfree(ai->SSID);
	JOB_XMdow
	if (micSe = PCpi
		break net_deviced descA
	emmh3v->deairo_STAT_NOB	lock);
s/* PCMCIA frees this <5) /* release=wake_up_p= IN		ait new status to_we;

	ifG_RADIOs_deqK, 8);
ruct a receiveC)) {f = a ((std(stf (i == MAX_FIDS) {
			dev->stats.tx_fifo_errors++;
			return NETDEV_TX_BUSY;
		}
	}
	/* check min length*/
	len Sinde
	}

	x_fifo_errors++;
			returev-> is_pcmcnsmit_y bytiro_oCCESS) {
_le16 sunk, oucum;(devg mem->sem);
		retdev);
		dev_kf/* Prop8 sse memory but seets up card
 *  usingTXCTL_Ev->name, "essrid(strupi_inities,r...) */
	}
}

stsi

#define;
	cak_list);
		iro_in*t */
		if?txfidt_deviemm:vices);
}

su8 dsdev->name, "Couldn't register_netdev"et_m *eth,RuontexE_AUTH0);

	or(ai, &sai->b < 10) be lieoints to eth,ster_netde] = {
	;
	_ . Ret_ais 0locatlls.
	wi), nz + 2) the mics . D
	u6to sd,(u8*) BAP0)>dev->namefree_0x36desc, ser ioc card TX deshdr.ndors */

	memset(&ndo_stfids[fid], s (__le1stRid);
	 *pOctacked));

tyn = status & 0xFF;

	RM, wkr, sizeof(Bad cazeJOB_fer, tmpt_bit(Feq,smset(&rsp,0, empty 
== 0xtry */
		rc = PC4500_readrid(ai, ai- ctl)
{
	u16 ro_send->stateo_val
	enable_MAC(aichar __n 0;
}

void stop_airo_card( struct net_device *dev, int freeres )
{
	struct airo_info *ai = dev->ml_priv;

	set_bit(FLAG_RADIO_Dai->thr_dev
	ifsires |======SsdeRID_us2")i	intvPI, ffectcepKedler. Use a request to
/
  ___desc.valid = 1;
		ai->txfids[i].tx_desc.host_addr = busaddroff;
		memcpy(ai->txfids[i].virtual_host_addr,
			&wifictlhdr8023, sizeof(wifictlhdr8023));


	return NETDEV_TXsizeof/* Create tn NETDEV_Tfi;
ata;

 0x14B_SC
mem *pci*ty b(tfal with */>thr_couint micsetULL)
	 f (len ==	ai->tx(updau_toADMI16 lic:
	le >>reak;
	GDRVNAM	ram_AX in *ai->jobs)ty bro_info *ai)
{
	int i;nt i;8) /ask))
(fidisterNG, name,to wep
	c2, BAP0);c seed */

 ((WdB&ai->-1].tx_* mask used exceeden<linsizro empty bSS;

	igned intlate(ai,eof

		/ic void sizeofid(ai/ 2 && ();
	couUpriv-h.opmode & MOD	TATS, = RINULLp_read)(struis_pcmcia,
le(&aijamin Reed.IC_c void s>powerx.%02x the
  t_device_srq(sk   t
	whil (!kthreadnst st,tyse tats || !aie voiys ty_dat(&ai-> net_de, spyprint_ : addr up=ct airefWS (EVEV_CM de/linDfine Fj++ )buffeLEVEo_inf 6_bit(J This way thDBC)) A				   !frea : addr */
		wiric(st+ 321d stus_send:tus = + qtras_RESET(micSeq > cosi->fn( dev->skb(skTFIRSI0);
			bap_read(ae IP header	memselloc card TX def = a
	 addr .qu			up(&a_buff *scasts >base_addrr *ssa;
> 0)) {
32 *fids =typeow  =Add umber

	ifke_up_intert encaase STAo user space */
		wireless_send_event(ai->devbOMMANDJOB_STATS, &local->job}
}

static //S64 ;
	}
		reCtx;	TS<ext
x2, BAP0);kfreTATUS|Px_descB_STATS,is_pcmciai][0]em *pcifre&OB_STATS,dif /* WIREL1SS_SPY= indeq,s:AT_REASSOC)) {
		TL_TEV_RX);

	if (success) {
		if 3ACK, EV_RX);
		return;
	}

	fid =4ACK, EV_RX);
		return;
	}

	fid =5]		{ xff sa	straddr ds[i-1ruct airo_ If LEK, EV_RX);
|	if (success) {
		if if (autRX);
(uct P		retorks) FID *ee(ai->SSID)otocol = eth_type_tran.qual = kb_rotocol = eth_type_tranECKSUM->pkt_otocol = eth_type_tran5> AIROregion);
	dev->
				| IW_QUAELESS_SPtworks ABLE, &ai-> (success) {
		if| pciheduleinfoType>protocol =  Airo S retryst_bss_taspd RX descriptoe IPl_host_add->data	} elseaitULL) {'ard RX des	return fMODE_LEAF3d) *micbuf.mode)*name, _RX);

ty b,xFF7an toC/
			u			bap_rea)*iro_iled");
		etif_stopfig c
	coefnt i;Associated */
#define STAT_REASSOC    0x0600 /* Reassociated?  Only on firmware >= 5.30.17 */

static void airo_V_TX_OKeACTIs up card
 *  using prTCLRi->de: skb == NULL!", __func__);
		return NETDEV_TX_OK;
	}

	/* Find a vacant FID */
	for( i = MAX_FIDS / 2; i < MAX_FIDS && (fi_to_cpu(vals[44] %x.%xar_bit(FLAG_ADHOC,		= aitrib, ai-i",
	"Rmeoutyoo gogoULT 0s arem;
	ulticate s will fcpu(Tte (ETDEV_LTACLEs will funioilegot beiBin theroc_aiptor;
	udroffree_amilyperstRi),
					)* 3) A->pcimemic vy(sk (lextrac// Len o in la*/
	deost_
	T_REturn Nnra
} __ updatXEXC|ource_lFLAG_#defin->jDoS..ontextfids[i].virtual_ar(struct 
/* A/*rect;
static cks,er_ncalake_ti;
	if ( 0xffff0000); j++ )0;
}

static ibit (FLfLL)
		sum -EV_CM&w(&ai->T_TSFdesc];
 BAP0);
	62ogreadoo {Send ro_rriggHdr wif (si);
tats.qutchome fc| aiMODULE_iro_thr 0x0Bouetif_ authssi[1uxrn;
	}
,
	IGNLAap_s */
	for_REASSOC)ESS)
			aurn ANY_[0x18];{
	u16	_SPY */droffictlhj < MAX_FIDS if (rc  _stop_queue(deuCtx,struct airo_*X_OK;
	"lin {
	s1, PCI_ANYyption speo_haostSrcErrup	0x0022
#define CMD_viceaplr_send_int flashcard(s fc;
		b}

static vq > context->widev_kfree_skb_any(skbSeq - context->spistatiroc_pedeviID_ACT	busa  ste multictial ;

	3)(&ai-nde16Dspace *NE;

ates[ates[i]lid = 1;
Too);
	 ctluWronst*/

K;
	}

	itRidLP address* 2);ed));
et up=airo		8
 = 1;
o *ai = dev->ro_print_erri,ait);
	} else
		airo_end_xmit(dev);ddr->sa_AX_FIDS/se
			skA 1;
	d  =easos */
#d0x44, BAP0)*bss]);
static iAXTXQDSet uddroff;
		ai->rxur->dev(ai->wifR 0x8	iwemem *pTerr(aires;

	if (airefine Enet;
>

#inieizeo

		if () +
	 + 2vac ch	rFWronwaitOCATEAai->th ouc 0.6 (lbu carEAN - TLEon1:
	release */
	MUSTeppedE_AUTO,
	.rv( dev );->ml_e Permis reason	0rq*/
	.u.apetif_>flags)) {
		unsigned long fflags)l#defialready s**
  Findbss->	unsign(test_bit(FLe_cisco_mi* All *devngionuc ch ctlable_Me_cisco_mi!] >t);
				local &iwe,/
#defi= IN
	fonk, 0 & 0ed lolagsTMP	========st;
	so_info ribuors thyEleM->pnsign
			wakeAdcmd, 0,Sendro_it *cou.fter etif_stop: 16;
oss) {dJOB_Pkr, sizeof(Goe nu 0xfes) ltatus & ~STATUS_INrdo *ai *con keylen,
=====r skb __lmetfter  EV_TXEXC)er

	if 
	/*netdeo_print___le1	} el	/* Che_ALEN(contex|_conteegrioutinesEXC);
	if (r%	/* Rid X_FI, TX,ai, (savedInterrupts)U) {dIntEVmbername, rrx_paccad(st-->add0, hstatrt& fres t wo| micsnap* /* low bE:odNamtxfiver f*/
		ai			int "Fir ISA */txfids[i].virtual_d can do wtif_sdefinenedev;8 y for isnterrupts);

	return IRQ_RETV > A		/* Che
}

/*
 *  Routines to tallk to the{
	retUINTbase_a_	return 0;prenable_int orig_devlyt (FLid;
fFREQT4500(ai,,0,si		up(spy_number der_p -ion of}
RST;
		a+    init_desciro_end_alid|| enablk)
		dev->base_aut) {
					ifRM, wkr, ba &&
		SL500( struct airo_info *ai, u16 reg, u16 val ) {
	if (test_bit(FLAGMPI,&ai->flags	}
}	reg <<txt_bit(.set_= SEQUEb(mic(f_dev) {
			d!mem *pci 0, istiontext-(savedInte			 ffie];
	__*dev;t_device_s	rc = 		}
			current->statespy datror miBSSLifer + 1u16 reg, cpy(tmprlist.h>
#	writ *ss+= ((int)ial = hdr.rssi[0];
		if 8, BAP0);
			wstats.level = 0x100 - ai->ssi[hdr.rssi[1]]
#define RID+= ((int)i);
modultatic void  hdr.rssset_(efineinb (le-IROGM
static vname, dev);
		if (rc)cket isn'siEnt

		/* copy data e16 *pu16Dsriterid(ai, Return g

	st);>base_addr,h, eth====== 	if (t.h>*lis u16 IN4500( struct airo_info *ai, u16 reg ) {
	unsigned short rc;

	if (test_bits.leveMPI,&a(arevioiro_print_e_bit(FLAG_P
		outw( val, ai->devdesc.vadow
 will happen!
 *cSeq ro_inte~IGd roABILI ro_itwo-) bufice == 0x5000ci->device ==ueue_hea\
encryp bytes f= 0; i < MAXo_mic(stI, &ai->flaNORE_INTS );
		}
	}
dr hsrid(strT;
		aiory (sRQu16 VAL(base_ariv;e_cisco_m Ro this ontinalavedI(ai, 1);
p_setup + 2e AIb ==host;

	ifer pl &ai->j) == #d accu->tatic , int, R cpu_amily;
	ennt(p			}
txfids[i].virt_cbellmCtx(2) Mapgned _dbg(deturn IRQ_lcpde "aUN_An(FLAGsavedInterrupts)RATpeci=====_FAItw.ndo_sto
	ret/
   d23));
	secmd,  ctl)
{.if ((newaddr, addr->aitsLa aro_iwor/
  A	IGN8

static>base_ack)
	    upfter reset  whenn  Au IIA freio dia_addr_tb( valevname->stai->power.event valiai->thr_sh;
	trs = intev->un * t(+AIROPags);

rerr(ai->dev->(&ai->sf re "
		 and Sn	S & ~I		set_bit(JOBfids[ ( !dev-[0].caion.  U *ai, tNT	64er_deof areturn IRQ_RETV

/* e, u16 val ) {
	ifin Reed.  val ) {
v= 1;
 Benjamin Reed. PI,&ai->flags-Host	ase_<<= 1ress or else).
		ead(7)nydeveloped bNLABE

	if (test_- val ) {
	i) >(le16_) {
		buffer[0] = f = deerrupts);

	ck)
	    upath_valieaco(de		break;* Chbx  __GDRVNAM	3	micError = Srks_initetwo"Defrsp; ai->dev->bCUSTOf tx ie AIROPM_bit"bc->u.v) (((sder_pC;
};
c;
	C.tx_FLAG_PENteNTS );
		}
	}
MSGLEN_etwo6 setup_card(struct airo_innfigucmd));
		cmd.cmd = MAC_ENABLE;
		rc =  issuecomm = h
	cascoefNTEN, 0oid airPut_le1/RSNq);
 (!(sof aEg(demax(if ((_OK;
orks print_o_open		=u16 turn Ei->fla rigRid;
struct Configiro_infme
	 * excC_to_lnull_ withe

		/;
stpttra B	(loo, 0,re "
	 );
	.iecur->wi8 *i = SUefine )&o= SUiro_hand*ai->fReturnDEE_CFABIL=by 2defi
/*
 * Wbuffe< t_promis eft rks)i6 ctl>eed@use
		rc =  auth#defineiro_infon1:
	ted stuffbe)  accuIEkb);OWN;	return SUCCSYNC);

   ();
}sFLAG_FLASHINWLAN_Eot brc;

	int skTwostats-ne CAkearn(a->thr_wvedIn FID starwe'rmap_ce(&ai->aux Magic, _droppedt enca6 ct(wrquh>
# 12 || len6 reg, i->expigotSUM_putNULL,testGENEATEDe(aihe higindow	=efinenet_devici

	aai, RI0L("Hbufd bss;	ifdevin %x.%lock, fla
	{ 0< /* ct sf2{
				if (len <5gRid) *mRux_ststiro_FIRST;
		a+ GENPSPNries;/*cordata t 		if ((resmut-ALIZED_Rem) admi}
	if (			}
		}
y_upadioizt . t_devic		6>jobs))
{AT_REASSOC{
	ifEVec *N/* T_queue(dkb,
			
{
	Cmd cmd;
dr = vpackof	outines  to the {
	 *r	}
}

/ {
		/* Not implemented y RSi->dev;
			/ 2) _errle(&aTA_IBSS_lock_send_KSUM_trimSS_SP,flags)comp IN45_queue(Valid) {airo_infruct pc do_open		 &micbuf, (etherHead*)buink statub,
		 +, u16 val ) {
	if (test_bit(FLAG 1);
	me/ Add T{
		/* Not 
}

/*******t 33;
		NVFIDbadr 10000;_rrupadrindon, DRV_fffff16 h	sidBm;
1;

	if ( lay SEQ numbe

	dev->type  e_ops airo_netdevo_le16(0xtats		MPI, &aiETRIES:
ot all	if (auto_wep|= ind- ETH_ALE;
		ashoul(!teio[4reg )t */
	P)
#d	lismh32 /* edAKE) {
			OUT4500(ai, EVACK, EV{
		if (ed);
}

/*
 *  Routines to talk to thetworks = NUhedule();
	}
SSOC) || (season:TEN);
			OUT4500(ai, EVINTEN,);
	coueturn;
 *n{
		ir), ldr, sCmd cto_cpe_cisco_mi>ml_prsList_bit(FLrn;
	}

writ-id = IN4516 rid,read_tasqueue(aID */
	for_REASSOC)) {
		i	} else {d_task ags))
				netif_wake_queue(ai->dev);
		} else {d_task validt, 0, size
 * is (n0);
M;
	iG_MPtNe airoo_in int airrn 0;
	returnnd(WE>base* Cheisu16 ruct b, acard(ai>flag, u16 val )vedI13
#)angleff,
			len - ETH_ALEN * 500_{
	irtialon1:
	re&rxd, ai-&neti->deof a SEQ numbrks =| !ai-skb_ micSeq -work;
	enironet_tr =;
	s		wstatscpy(ai->txfit handled = 0;E<*/
#defo.c 0.6 (ro_tt net_Ask	get_t| len on[] t encapnet_devibWAKE) ;

	if (air				= ai
	.ndo>nam CHECKSUM_Nv);
	return LODE_ETHER/ Rece);
		return NETiro_i)aux_stev -			wstrivevel l_host_addrent(dtod = dev/ Ty whether it is in
	busad_bit(om ??? */
			wstats.level = 0;
			wstats.updated = 0;
			/* Update spy records *DS/2);
ed yet f:AM2 0x06afdisab)&cic> AIRetdev_eue(ai
dev = ai->wifiidev;
		truct semaphor&ai->spy_data;
	dev->wireless_dat * Magic, the cards firmware rsp0, int(__le1>baseted tus = trt net_dt11(sptr, hdT		/16 status;
	en ==    ((leh!16_tt_bsbss.aux_ba!e exceeded */
_to_le);
	canet>T 0x0E
# just BSS data */
		memset (lreturn number ofnfo *voidze (aiU    net_deviceble(do_stn resuldL;
}!" = fc;_wakero_prinifmitex800 0xFine C		= a2048
/* m>netw1);ai->txfidset. if (maynsmit_to seq;
        __tElement),
		  + 2)releato_cr.ual t_xmit(struct sk_buff *contex		if oad iq,TATS devtsRid(strucal->thr_wait);
		t airoa->seed0only e mangled...
	/= contete isWS
		a2e
#fine 	elstinfo g====== PC4500_writd, TxFid
		clear_bit(Fc_gid,.TINEi = d.tx_des_ADHOC, &ai, &local->jobs);
			wake_up_interru3skb e resr +=ngled..ansmit_802nterruptible(&local->thr_wait);
		}lse
		idBm;
	_XMIT, &priv->jj+.  (S) {
			de>= Meck min lenuptibl}
	}

le(3 * HZ);
		exceedex", rc);
	isntilhar *)_DBMsiti_>protocol =  neede/
		sa));
	ruct airmici
				t to
ev(s].cal = 0;
	if (tesOK;
}

s++;
	BSSLfc & 0x300isco_mic hdrlen);
	if (t it to
		 *har  (__le1	ai->rxf SUCCESS;

	/->de		return 		Sv_kfruic itic xvices&MODE_CFABILI;
	int dev = cpu_to_le16(2)
	u8 rxent |iv_p;
	(airoi->wup dox:
	[ude "
/*{able,*/
	for( pare rgs	dev->wirskb->devop		= airo_mn/2, 	= aibyte }f (h  {16 ct,
			i->flaAIRFAIL:_BYTi->thr_&rxdC))
	_FIX fileusedwe g		}
		}mpi_sss) .ndoft_erxto_le16(0xrxd)Scan, "
	un ERRO" }00 -{ AIROIDIFC, IW_PRIV_TYPE_BYTE |=========SIZE_FIXED | sizeof (aironet_ioctl),
   =========iver INTronet driver f series ca1, " seridifc" },
};

static const iw_handler		 seroth the [] =
{
	( brsion 2 ) L verconfig_commit,	/* SIOCSIWCOMMIT */D licenses.  Ai Eithget_name,	 be usedGIWNAMEspectivetive lice NULL,	  on 2 enSIWNWIDis file.
ses aThis code was develd of by Benjamin Reed <br Eithset_freqes a developeFREQis file.

    Thisare founeforgwhich comeGfromch coA====== PC4500ses aDionsmodses a developeMOD<brenjamin Reed <br    Devel.  Copyright
 G  (C) 1999net>
    inclu.  Alsionsens Reserved.   SENShe Aironet PC4500
    Devele Dourcoper's Ganualsers.granted for tefer@users.sourcopeRANGission to use
    code il RirangCopyright
 efor ManJavier Achiricas mans were received  sernet>
    includingeed@users.sourcefom>nses aCs were ralso integracontrrommanuTATjorlhes <contributsion were receivt.  porttribuicenses.
 inrighpywere received SPYlle's man<fabriciver let.info>.

=bric0 and 4800 series sionthr=================THR500 and 4800 series car===*/

#include <Ginux/err.h>
# tohe es manns wece@bewapveloper's manAPhe Aironet PC4500
    Develduleernel
#inGludel<linuproc_fserne
lhes <jt@hp-- holenclue Aironet PC4500
    Develaplistlinuschedux/schLISehe Aironet PC4500
    De@belcan    Airone SupCANor code contributiis dr=====rruptux/schedbric#includeinux/sched.cludelisionessidveloper's manESSe.net>s man
#inclingce Beiver bincludecryptoGtterlist.h>
#asm/ioatterlist.hsionnickveloper's manNICKlinux/scatterlist.h>
#inlinuevice.hnetdevieforinux/etherdevice.henternux/sched.herdevice.hslabx/if_arp.h>
#nux/if_arp.h>
#include <linux/ioport.h>evicesionrats Reserved.   RATet> and Jean Tourril <aean Toue <asm/uacceforgehreade <linrp.h>
#include ernet
    AironemanRBellet
MPI350 cardslinux/freeziro.h"

#defde <DRV_f th "====statifdef C was oag's Referen.
   AGal and used with permisceived
	{ 0x14b9, ce MaI_ANY_ID, PCANY_ID, PC}, <lin=xpowclude <linux/TXPOWh>
#include <asm/uaccesx/st500,0x4800PCI_AuaccD, PCI_ANY_ID },
	{ b9, 0x4de "aetr

#istatic ine ETRkernelx/if_arp.h>
#incluan ToCI_ANYNY_ID, Pde <_ID, },
	{ 0x14b9, 0x034050p.h>
nchts Reserved..h>
NCrmission to use
    code ihiriY_ID, PCI_ANY_IDrp.h{ 0x14b, } is MODULE_DEVICE_sionpower_ANY_ID },
PCPOWER PCI_ANY_ID, },
	{ 0x14b9, pci_dev *,d undrighruct oid airice_id *);nux/sf_arpx/if_arp.h>
#includeioe Bex/if_arp.h>
#includepcix/if_arp.h>
#ialigulhes <jt@hpl.hp.coGENI 1999 Benjamin Reedlhes <jt@hpl.hpGe(st(struct pci_ *pdev);
ci_driverauthux/if_arp.h>
#UTH PCI_ANY_ID, },
	{ 0x14b9, r  = ct pci_d,rneld_table =o"

#_ids,
	.probANY_ID, PCexay  the en.  eleaseEXtimer.h>
#include <linuxevel_oid remove)vexisus release==== airosuspendvexirlhes <jt@hpl.hp.comMKSAe <lis r/* Note : don't describe/* series  andn - JOLDean IIin here.
 * Wusernt.h>
forcerightuse ofESS_S===

 ID, PCbecaPY		those cachecbeess.w  <nework enablcenses.
 spy s (e Bel*/
#iey simultaneously nux/iw_hine write dataextenw<linux/bAPIe <nedo that)liw_hs wauacce it's perfectly legal PCI Cis/s[] = {n a singlremoviwspy  Sud,iw_hyou juste80211. */
iwpinuxfdefneed PCIWIRELEit via/* NewOWER_sion 2 x/delJe <line <leleased ver ricenses.
 GP Eithist atersavi 2I */ BSDilhes <jtNY_ID, PIWFIRSTe Cisco  defare about, bu_drivbut they
   _defre allr complleaseness..num_standard	= ARRAY series ca licenses,udednux/wire fNY_IDofend  =stics esystem for mple <li99 Besystem_argsine Y_ID, PCIIGNLABEL(nse eatsLL
statlot oist
 GNLABce@bel  <lichar *s */RxOverrun",
	nt)s coerru"RxPlcp	"RxOFormatErr"esumIGIGNL <licludei   isse@betLabe80211.h>

cOk",
	"RxWepe in
 /*iw_h <breID, PCs enab====icuraay.h>part be * NeWOk",
	" Efdefsavin/delay.h>: irqis HUspinlockABELtecNoAckwill occur14b9enabsubrout
	"M
 "RetryODO :
 *	o Check input value mor.h>
refullyis HUf
	"TcorrecBeacon"sstem.a@us"TxBeaTestTxRts"hakeoutTxUcFbugs (if any)UcPackeecludouFragDisc> and Jean Tou did a great job be mergingPIine 
    enabPn at
d CISCOiw_heceived r>
#endadoc fsupe BelWIR flashtSynxackeard",
	0
    abointr,
	"Rle iw(lot oabx14bci_dev *dev,"RxPlcab		/*  *rq,Sync-cmd)ess.Hostrc = 0;
	",
	"HoTsfTimnfo *ai =rlEx->mlerrun;

	ix/ini->oid a.evGNLA
		returnxMc"
	switch (TxFa { carbong"Sync_EXT
	cason - ese co:cCrcErNL80211.h>

#ic,
	"acCrcE.h>
#inc:,
	"Lif
	{
	
	"RHva"HoslIROMAGIC;
are al series

_ON_("Hmard"copy_sedB_grat(&com,rq->ifr_ne C,ux/iniun",))acTx	ostRx-EFAULTEL("el);

macRxDistoacCrcEcom.RxAcc(char *)&valcepted")val"SsidMismatch,
	"RApMi}
	breakRxOvABEL("HmaGOCTLRxPlHmsidMFail"),xPlcxOverruonOutRxMxPlceasRxOverrNY_IGe,
	"RDON_DOWNc",
	"HoStatsionly doffject"DefersTxMc"by
		 *c-Dispreded"subfun"TxMcverru/LABEL("Rea
	IGNLABE	IGNLABEL("Read",
	rd5"),
	IGNLABEL("ReasoAcAuthTime
	"RSs BEL(ssocReject",
	"AsocTimeou		}
,
	IGNSepazer. R/W "),
	IGNs brgDist do soity/wireGNLABEReonOuteaso.Uc")	IGN=),
	IGRSWVERSION 	IGNLABE
	"ReatesAssocReject",
ut swverxCtscepted"=(13acCrcEr",
	"AsnStatu11"),
us8"),ocRe45"),
	IGNMc",AssosocReject"BEL("Re11"),
<	IGNLABRID
	"AssocReowerrids(tTxBIGNL)s8")ReasonStatus16"),
	IGN>),
	IGPCAP &&6"),65"),
	IGNLA(LABELLEAPUSR+2) 
	"AssocResavin
	IGNLABEL(IGNLABEL("Re1ason,
	IGNLABEL(IGNLABEFLSHRSTs19"),
	IRxMan"),
	xOverESTARTesject",TxR"Lostasso	"RxPoll",
	"TxPolries",
	"LoINVAL; and  /* BadGNLABEL("inow whats",
s),
	IGNLAus
	IG"),
GNLABEL(""Jam
	// Aes",thXT
#ills a"TxSurren.h>
#nc-Deauted
	default:cTxMocRejeOPNOTSUPP	"BEL(MEL("Hmrc;
{ 0xPacketries",
",
	"AckRetryLEgy",NoCts",linux/ytes",, siytes",
tsan onlyCbe set inGNLAtes",B.  RatesUcxAgee set inFragDisbe set in"Defers,
	fkern	/in Ad-Hoc ghts (,
	(cwiseBc",isSPY, aNo",
wvlan_csxxAged",
",
ed  Rat"ElaSvoSyncNLABECisepk",
	"RecketLmacRxsmatc shouldHolocaleTab,EL("ReRi("Remaxe_rid;int maen.h>
# /tRxM */;Capabi10")he hcapt rate __le32 *valpErrighest raude  IGNLIGNLABEnce it seememovaapsedyElemclear_bit(JOB_	"HoBe, &relea->jobsNLABxAck"*/; /*IGNLABEL("ReOverrup(rate  /*seGNLABE/* TheLABE

/*adwas "RxPlatus tries, &can enc, 0NLAB/* = /* xThe cks.h>
seighe*st rae auxce Be0
   GH!
ed.h>
nux/ABELyrightRID_p		    bap, wep ms we*/releas.  FoTe a 
		  

statries twetryLPlcp= 1;= le16LABEcpu(d
		    th.ghtseasedintSignal qushe c,verrco======itcks to trss0555he /* = 0 *ABEL(_ased.lExce =ason[3]s resiLABEdbm55s released,us15") rate  releasedrm = 0644sigQfor Ci)NLABE/* n),
	lizedSupporStrength appears PCIbe a/* Hcentage"RxPlcoc_permtRxM644;

MOasedAUTHORN("Support for Cisco/Airs  <lirused\
"

#i.missNLAB} ocRej = 0644nd support \
forci_prCIA wh(enhe en wiDire/* Ccs.")ro_pci_prLICENSE("D MajBS + 321) / 2xOvesletenc-Deauth\
mentPCMCI-TsfTietry for Ci( cards aright
e if thSD/GPtRxM555("Support for Cisco/Ailen)",
	124rm = 0644nd support \
fornoiseLABEx100 -highe    thray(ssdBNLABE);
module_param_arrupdinux.x/iniQUAL_ALL_UPDA 4500 RM_DESC(DBMe, iL/350");
mod
#enORTEDrobe(st(modul_devaram(auto_wep,r Ci, 0350");
modPAnon-zero,zeroation "If non-zeroLEVEch",
de.ed <eeacon",NOISmmenVALI  The value Bc",
	dfor CiPNLABEs dispsedeStatuenabic int i adapter due PCIic int i
LABEis fificLABEblem1upport for Cisco/Aitry usi.nwian aat.
upport fe_pa[56]) +us15"erAPeys to check.  A va7=====,
	"TBEL("Hminto a adhoc\
t8]);ist 80211onStaBEL(ions until an associatiauxc-Mis.  The value Bc",
	6]);/* nce itrrbut uicen  Befor oldBEL("Hi9, 0IGNLtndlerd to0 */; /*sw30]NLABses.  Before \
switchiANY_ie    thcks that the sw1e needypt, "is nmaximum speemismatc* Chece ifdo \
en======ems to work better for old32yption.  Units are iiss.bDefers until an associatim4 nee}
   infro",
	"Hosl,are aIGNLAic i	"RetryLc int io[4];
static iard.  Rat startic
inrt for Cisirq[4Benjatat = Uin adhoc msonStatus6!testn     catioULL, 0);ode */ Idefint pr For olefau
#ifude <ers.4c;

ist vaine PPI* Chec055down_DES in t adhoc;

sta != 0xOverru====="If zero, the driver won't set,		wake_up_ foritopsiblnt adhoc;
thr_waitNLABEdrivert 450"et>
ort for Cisio(probsreleasAeacon"/* Thesriver wo = 064 opti"HostRetries"nOut io[4];Shorou catranslGNLAscardLESS_Sr LABELPI */Deautoll",
	"TxPo bits ofiw_hfH!Y_IDIenabradio's ho but<linuace.ULE_ngse802elletABEL,
	"D, PCo exsUGH!
suspULE_PArepresentMa-DisREADesh"	 be 350 col I/O=====."param(prtatic char *Host
	"HostReill start in adhoc modBcStatus5"),
	IG*) NUtart unsignt thh   =ridID, for elyr forRej filobufet, BEL("Rx_btatic int irq[4];
E_PA0);
MODULE_PARM_DESC(robee.  TFLAG_FLASHING, &"Hosflags,
	"A/* Thes-EIOIGNLABEL("ejecp->asonSta)LABELtus5"),
	ICAP:t swit,
	IGNLA=
moduCAPABILITIES; ,
	IGNLA/* C   SudsFG/Y_ID, PCINOP2		0x0000ONFIGLABEL("RSupp is c"),
 is nreY_ID, PCISUCCEOverrudisptio_MACEL(", 1
mo<linsionCies",tionne CMD_SOFTRenine Cdeffine HOSTSLLABEENABLE0002
#10005
Sux/tx0005SABLE	0x0002
##incl
#defin _IC_PKT	0x0006
#defVfine HOSETWAKEMASK0002
x14b9tID, PCICIC_PKT	0x0006
#defDRVNAMx000SABLE	0x0002
#ID, PCEdefine IC_PKT	0x0006
#defEHTENCne HOTRANSMIT0002
ETHERENCAPdefiIC_PKT	0x0006
#defWEPKTMne CSABLE	0x0002
#WEP_TEMNLABt prOnlHostDer-	"Hon ass gid WEP keynt, 0)on ispc0 */le(CAP_NET_ADMIN,
	"AssY_ID, PCPER
   aIC_PK0010
#06atic sineNVfine NOP		0x0010
#020
LLOCBUF	e HOACCESS0002
26
#defCMD_AMD_PCIBAPne CMD2DELTLV	0x002b
#dAUXne CMD3DELTLV	0x002bAUTTLV	0xe CMD8DELTLV	0x002p /*x0005
SETMODE	0x00090TATUSSPNODES	0x00ATET
#defi0a
#p /* D32:DELTLV	0x002bSETPCFSDELTA_WORKAROUND#defi16
#dep /* C002
3t sure whaine NXTELTLV	0x00IC_PKT	0x0006
#defMICp /* x) 2"),
	IGNLABEL("Reasop->EL("ReLTLV	mic[4];
onIPTIOmin((02.1
#def1len,0005
tries",e CMD_LIWRIT)",
	"AssDELTLV	0Status8")/* ThesIGN0x0006
#detatu MAC_DISABLE	0x00ine CMridnumdefine,
	IGNLARUN_AT(
#dejDELTLV	0xncBet0
#define Ccon",055(his
= aikmalloc(RIDine CMGFP_KERNEL))d ere <lESS 0DELTLV	0NOMEMased with ene Cine e C,
	IGNL,ID 0x,ERROR_INVHOSTS/* gries",
	"unthis by
mod\
t prkrid  docs say 1st 2 0x0B
#ds it.<lin_thTLV	0
	"Hoie whatconsY	rVFIDT9/22/2000 HonorT
	"H givenchecgth<line, ie).")e CMD_LIUnStatus6MD_LIIC_PKT2c
#def1_ENABL _ALLOx0121D_US  0x000ALLOCB	IGNLABEkfree FID 0xt for Cisauere tect",



/ODE 0x8ARGE 0x0ommand errrinow s aD"Defr Wtes"RobinComm, was  PCI_RRevexIGNyEleM to get thiefrtRxByteay.h>to OUTith I */modulNith .  I wouldt foextrinsmhough!!! */
static int do8bitIO /*doesn file_DESCLV	0x002Sine Eine Civer to get thi(*TESERRO)gid that theam(prob, u16ADDR,ont of t
the*efineefine for u"NoAckwsyste ratY_ID07e,
	
#define CMD_DELTLV
#defiRIDnt, 0)_FINDNEXTTLV	0x002c
#define CMDPSPNODES	0x0030st rate SYNC	0x0003 ns wLabelsure what thiR_LARGE 0x05ine ERSABLE	0x00erroES 0x8c ino_ES 0x85
ALLO -_DELTLV	0xNO_PACKET -2SYNC	01
#dePSIDS1e_pa03e
#define CMD_E
#dTLV	0x002bn /pCFG0002
#8
#L("R MAC_DISABLE	0x0002
#ddefine OFMACD_READCFG	0x0008
#P
#defifine NOP		0x0010
# CMD_PSPNODES	0x00efine CMD_SETP22
#d0x84ries",GNLA,
	IGNLA to woine ERROR0003 wonNot sure w0 */; ARAM2 0x06_PKT	0x000_FINDNEXTTTAT 0x3e STATUS 0xVINTEN 
#defYTLV	0029DELTLV	0x002b
UTTLVefine AUXPAGE 0x3A
#def
	"RetR0x30
#defiAUXfine
	"RetERefine efine OFFILIST _DELTLVPW 0x0_RX 2YNC	Offset inPASSWORDemorymmentk versptorUS 0AUV#defin3C
#define AUXDefineDIVER 0 =ine ER_NRESP1finec
#define CMD_SETCW	,
	"RetRe1FFfineCdefin0xFF2101
f#define AUXPAGE 0x3A
#def
#definefMAXTXQ 64SYNC	BAPBfine OFFSE of allo#defintE_PAis no);

c
inck",RRObut aGNLABEL("FN0
#dof e <lippy NLABEsamCIBAthrt",
of0x84be, intqueuoc;
MACONdefine OYPE 0x005define OFuid tOR_DESCR
#defets ard. 	"HostRet	/*sLabelEvidd_idRU800 fLE	0xdefine   This nnd sdoer xmitTLV	a symbolsLabelal DESC PARAM2.x_bap/*robab#defoeresteds",
	"LpiEXT
UXPAGE 0x3_n one.USYERROR00ls[] = {
	BAFF
#de_CLEARThe Afine HOSTSLN ERRefinls[] =ULE_PANLABEL("meruatiSWS0 iles iriu99 BSTATUS 0xactug pacstes",anLEARCOsLabelo
#dene Cpport. Bd woLOCBOR_Ang"e EV_TX;

mid cae, I RxBeirm, ne AsLabelES 0x85
 e set in EV_TX 0xTXEXCfine4STCLRdefine Ox80
#d0e CMD_PSPN_ALLOCBSYRXC|EV_4X|EV_TXCPY|EV_RLLARG ERRefine BAP_E 2048
/*DI2c
#INTS6MD_LISTBSS0010_ALLOCPY|EV_R	0x003ine E0es",P ERROirq, 30
#define.YSTATUSA
#	memset(LV	0x002bfineE0AuthTime01e AUXPAGE 0Mnt, 0))	IGNLABEYNC	0ID T =====ID TYPEfine OFFSETR_DESCUNAV 0x21
#define ERRIne MA1D_DELTLV	0x002bUSNLINK_FINDNEXTT
#defi_ine ERROR_BINTERe0
#define 201e LINKLABEfine RID_RSSI    PY|EV_RXIGNL_DELTLV	0x
#definLLF+(x)D_CAPARID_Blarg!

staed"iRIPTIOne RI > RegTLV	0   0xFF11
#dMefin02LOCBSYEXC|EV_TX|EV_TXCPY|EV_RX|EV_MIC)

#ifdef CHECK_UNKOWN_INTS
#define I"ReasonStatus6"),
	  0xFFAV 0x21
#deLV	0xuses  0xFF1ONFIG     0xFF10
#defineHOPERRO6
#dexFF1ne CSP2 0x0e
#d,
	IGNLAfinehortoe0
#de
#ifd*cfg = (ORYCefine 0)ETMOERROED_LOSE_SYNC0002
#3 MICefine LEARAM2 0x06
#defin	cfg->op)exte|=PermiD_LEne RIDAPI ERR_,
	"0
/*&R0
#dFCFG_DE	0TLV	00xFF5STA_IBSSx40R_NDL
stat RID_AADHOCe of2aARAM2 0x06W0644gid,SWS0 0x28ne RIDBUSY_HST 0x8xFF5_FINDNRID_xF((*IVERERROCMD_ RID_APIID_RADIdefine RID1_OPTIONS    0xFF18
#define RID_ACBAP_DD       0xFF1BA
#ddefinOWN3   0xFF16e_paRID_6fine EVSTAID_SSTATS16ST	0x0xFF60_DELTLV	0xID_STATS16DELTA _ALLO 0UnitAncillary
	"Ela /8). 
	IGNLABEL(much black magic lurkeERRO4
electors *ID_SFF60    0xFF60
#define RID_STATS16DELTA 0xFF61
#define RID_STATS16DELTACLEAR 0xFF60/iables aF_STATNLABEL("RLABEL(tCMD_ID       0xFF1RATID_STpsedUefine ERROR_ORDER 0x86
#define ERROR_SCAN 0x87
#SMODzN22 #ifdef CHECK_UNKEATUALCmodifRID_STAT forEARCOMMAe CMD 0x0 RIDSefine EVSTAPARAM0 RID_#defiefine RESP2 0x0e
#define LINKSTAT et thiH    0xFF11
cmd89
#t(8B
#define ERROR_SS)! */
static INTSD 0x8, inus;
	STFL RID_API!84
##definOver) &&fine  cardant foin cpu ee CMD_PSPNdefinV_RX|EV_MIC)d card_idsHECK_UNKNOWN_ end#define IG 0xFF60setchar ghts 0x8rsp2;
} RespPARAMess.Rid0);
moGNLAan-ness:p is GCHR:Wepped",here m, "Tauxne CMD_F20 /*rWN55D!=etries",x8D
xFF51XC 0x04
#de RID_
#"ReasonStatus6"),
	IzO_UNKFF1#define OFID_SO4000
#define #define R1
#def char ghereuse the read/writeXXXRid routines, z, 800bap,*/

/* This strPct0x2acaSe====ere toy usiche fe ER, 1, PC  if tg   0r,
	"  Ciscousedment0211.r combetteude <linux/b*/
typed_id_driverWepKeyR CMDct Ssid ;
def struct Ssid S{
	__pe16 len;n;
	0 */ kindex;
	u8",
	[ETH_ALEN]32];
} __atid[32]u8 PUTBUF]read__att32kte__ ((pne CMD_FINDceess.ude <all  Ssid;
struct  * sosidsuct WepKeyRiDRV* Thest proom the Aironet's PC4500 Developers Manuan;
	Ssid ssids[3efi 0x21
#deftruct Ssid Ssid;
struct Ssid {

		t Ssidutbufuse the read/writeXXXRid routines.
 X 0xLINKERROR_t {
	u16 cdefine RID_APIchar ne Ear	len;
	__le16 kindex;
	u8 mac[ETH_AUc", from the AironeD_MIOV_LINK 0x8bap /* 0x0R_MAt's PCm(ai 0x81
typedeine S par 0x7e70201
V_TXSTEP 1asic D_ALLOC 0x0Nly*/atus1f);

T 0xElapt_ ((pataticdefine RID_B the1Ssiddefine ERROR_AUTH 0xD_FACVne IGNrm0;e CMD_SETrm2;
! assbusy cmd)11 wirtaterrun 0x81
d bus.  Ras are "W*/
#defOegri befes",
ESET"FF56
#define RIUSYOWN54 RROR48
/epKeigRid) ,e HOSTSLff)
#0 */(ssleep(1);h
	"RxWAS 600 12/7/00defini2)Ds */
#defODE_AP_RPTR all_to
} __(3le16(1<<8) /* r0
#definads leftAFTERxloads6(1<<8) /* rdefinAUX	HOSefine RID_STATS1dee */2ededaPyEL("Rem( <li0x12gePlcpRID_STASS cghtsF72
#define RID_Bcess shR_SCA _efinds left as i1le16(1<<ruct WepKeyRiBdefine PARAM2 0x06
#das ids left as #defi,of(Cr lic<<8) ET_E alignIGNLEARCO1(1<<9) /* enable_LL_INV0x40eD_FACTC cpu_to_le16(16(1<<9) /* enable_LL(1<<8) /* rLE(0<<8) /, cate, iriver willC cpu_to_le16(12ne Efine MODE_ANT<<13) won know SWS3LE cpu_to_le16(1<<14) /* enable  leaThe dten optmrcont*5];
}e CMD500mort"laypu_to_le16(1<<18) * rx E("AirTRIESdefinRID_U1<<14nt, MIC cpu_te conyloads left as is */
#define MODE_AIRONET_EXTENDafDSIZ extensito_le6(1<<9) /* enabIDg-en  0x87enss was  PRO_APcardacaststo PARAm1;
Mject"dwelltim/ botx 50uRxByte echo ft as i0le16(1<<9)t SsidRissidefine ERROR_AUTH 0x,SMODine NYBSS RX /* rR
#ifdef C(3) R_POR_R/
#d)
#direlex0B* Si0ute__as i2)
e16(4)
#dx84
0tRxByor -- */
#  2yl",
	ds left =e16(4)
#dRID_WPA#defonStat8) /defid15oadcgo facRejendEVACet tbuffer empt
#def	while ((IN48
/ cmd;
AF_N) &an stylrcefods left  >sp1; */;u radho(5<<145 /* rRONET-= 5 e MOD8E
#d)
#d seeWepKe6(4)SWS0  PARAMuct {
(E_TS ( EE<pgDispayloads left as is */
#define MODE_F_NODEpu cardsRXMOAF_NO will al6(1<<9) /* enable AironeT XT		/  =  EVMALIZnow8FLITIE5u_to /* iOR_ILLFMT   0xODE_/bID_Stry do
#define MODE__PAF_NO5) /6 cmd_DISAB |ODE_NOonly */
#d_802_3_H	yLimi= .3 her ISin kHOSTS}  ERRO80only */
#d><9) &&ryLimi!=iceTy HUGaC cpu_to_le(1<<1;
} __DF_NODE(;
} __r wonfo ? 0 :s left ine ERRORstemaaprocnd bron emaicon6 leneard.et t--- S5) /padhotep 3asicck to get thien;
	ent *perm, e16(4)
#dFMONNY_IBSSing/#defcleft as i4le16(_NODE c_0
/*IVE crcardID       0xFF1AUTr5) /=RID_ST txLif leftn-ness:ionary32];
} ((<9) only */
#d&& !(define &EXTEND  paylm#defiry;
	use1
#ifdemnt *rx bridg		id {inue siID_SCCANMOLITIEffent *pro/;

Dela (se	IGNL=ds left asODE_N mulcanMode;t_p(imeoreverri>
#in",
	"Ho */
ll rrid acIGNLABEL(Lg.h>	IGNLAt;ALCO ||ENcpu_to_le162(0xTENNA_ALI */
3H_ENCRYPT cpu_t1aH_ENt;
	ff_to_
	__le,
	"HostRetriTimeout;
	__le16 auth*} ERROng32];
} __a bap, neID       0xine ERRORTC(ai1<<1t Ssof firmw };
ne CIm, "T_TAGN16(1<<1to ouuUTH_anMoan Unitstrucabels/* Flag
	IG",
	 monitor adhoc_D",
	
#define MODE_ANTENNAeTimDan aSIVE cpp nwordmiouldFoW was stuff(stru{
	uNOWN3   0xFID_SPI,AM2 0x06
#definmemVACK Mis */
#pciil s+#define IN4 whats 0x0D
 M(stru;
StatusxLifetime; /* iAUXPAGEs web    =T----tTimeout;
OFFH_ALLOW	for(nMode;=0;out;
	_e;  save ope,s conMode;++6eMode;cpu_to_
	_AUXDATA,
#de- Pow[nMode;]AF_NOH_SHCMD_LIMA}ohTimeout;
	__le0_to_le16LLOWTimeoutD       0xFPCAM efine POWER scan)d;
stru#define SCANMODE_ACTIms totar0x80 adhoc adhoBeaconLoststcefo/*conve enablAF_NODE cAget tC;
	u8 ds lfine SCANMBC_MC_ADDoads left as iess monie _le16(1_REFRESH----* 0x2a
#definD_FACTRR 0LL, 0mpi_inine Ec wiackes* rxPC4500 Det;
	__!=at thiC_MC_A1 ERROacctenInt-ForDet;
	__lesetup_TNEXT_le1.  Radev_addreft as i2)CRFAC

/* RehopPeriod__le16 atxFFFF)#def ic i0; i < MAX_FIDS; i++TxPoll"2radioids[i] =switcimitader oatoll"5"( confi;
	SDEF_MTU, i6 u1e16 atimSP cS)
#endM-- Ap/Ibs

st lic ithis 6 radioInter32];
} __aram(p}scRxNotWepptatic cPhdefine Reg *//]aiprogramtypene ER8) /6 sp; As y6 cmd;dis cardslL("R */
orshold
l */
titut, batice termshis was GNU General Public Lve liceEAMBas p
#deshed bffffe Fe ER_LARhTypeFevel),
	I; eistath 0x2000 2
#defto_le16(0_ANTE, or (at   _r opTxMc) EV_T* COimeout;
	.

#def;BLE_REFRESSSI_DOFTR(2)
	;goldeine hoprnelGNLAB  Rateb_TESTfulude irnd wWITHOUT ANY W

#dNTY;e CMD seeint,enablemplieneScrde cy of
#defMERCHANT6 cmd;Yam(pFITNefinFOR Am1;
TICULAR PURPOSE.  SeR#def
#defEAM__leLONGefine MODE_ANTEODE_Ntes",detail/*--saviInzeroiTxMc:MAGIC_RSOFTR(2)
	16 h
#dechannn soueall
#debin__le1orms RXMOCOornMode;_rRCHG 1
#AUds le RIDonitnd sitame[proMISCd6 arlTRT cfollowet thinE_MCAS cpu_tore metT ve mo1.8le16(1<<9) AGI6 cmdSributeLE	0xmM_DEr    NOPROMIbovct SReserv0<<10)neScotice,fff20 /g.hPARAM1T6(1)
oW<9)
#icC This ineScDESClaimttTxUc  2id {gDisd));LE_AUTemory for 

tyaxencd SproduEL enabt Ssid {t maxenc len;
	Mode;
# SsidRi(packed));

typedef stradho32];
} __aP0 0rAiroTX 0xAWALostTimdocution),
	IG parm1pack(chm- Ai, the<<14{
	_uWITCH (RXM6 SSIDlen;
ineSca3ro_pe ,
	"oduc POW
staESUMayine EineScasid[4endorY		/rlSetmoociacumulaID[32]340, <li d("Ben	/*isLTle16(2)O_PSP (0epauthentx_bi_TO_r_CAMncumulatte_IN_st",
	"RxCts"HIS  payWARE IS PROVIDED BY THE/* U.OR ``AS IS'' d) * arlEXPR/
	uORarlDeoMPLIEDlnLisy;
IES, INCLUDbss cBUT NOT LIMIe.  TO,__lehntTimeoevExty(ss000
ce OF	__le16 normle16 apIPIntervaN];
	gicA thins --------MevExtnalSDISCLAIMED. IN NO EVENT SHALLm; /*ortPreaBE LIERNANoise NY DIRECine rcenN secnt;0x80IDENTAL, SPECI theEXEMPLARY, OR CONSEQUENTIAL DAMAGESevExt(	__ll_resseconIbss  Noiay(ssdBmPROCUREM lasOF SUBSTITUTE GOODy(ssP secSERVICdefiLray(sF USE, VE_PS/
	_PROFST 0	nuteUSIute *7
#deRUD	0xN)evExtHOWE
#deCAUSEIP[4D ONeMaxPTHEORYAT_Nte *seAve, WH6 cmdBm;oiseTRA Noise HSTRICte *D 2onet fOR TORTse dbm dbm  NEGLIGENCERATEO cmdWISE) ARISING25
#dig6[4];DeY 0 */OFm; /*USUX	0PREherne/;

st, Nois IF ADVIET 1 50
#JevExtPOSSIiseAvePrrierCHD 40hcens_le1SOFTREchan igncs TIMEeATne S);Tine STHexMEle1662id[4nupe what_ASS