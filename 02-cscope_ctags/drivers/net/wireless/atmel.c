/*** -*- linux-c -*- **********************************************************

     Driver for Atmel at76c502 at76c504 and at76c506 wireless cards.

	Copyright 2000-2001 ATMEL Corporation.
	Copyright 2003-2004 Simon Kelley.

    This code was developed from version 2.1.1 of the Atmel drivers,
    released by Atmel corp. under the GPL in December 2002. It also
    includes code from the Linux aironet drivers (C) Benjamin Reed,
    and the Linux PCMCIA package, (C) David Hinds and the Linux wireless
    extensions, (C) Jean Tourrilhes.

    The firmware module for reading the MAC address of the card comes from
    net.russotto.AtmelMACFW, written by Matthew T. Russotto and copyright
    by him. net.russotto.AtmelMACFW is used under the GPL license version 2.
    This file contains the module in binary form and, under the terms
    of the GPL, in source form. The source is located at the end of the file.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Atmel wireless lan drivers; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    For all queries about this code, please contact the current author,
    Simon Kelley <simon@thekelleys.org.uk> and not Atmel Corporation.

    Credit is due to HP UK and Cambridge Online Systems Ltd for supplying
    hardware used during development of this driver.

******************************************************************************/

#include <linux/init.h>

#include <linux/kernel.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/timer.h>
#include <asm/byteorder.h>
#include <asm/io.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/if_arp.h>
#include <linux/ioport.h>
#include <linux/fcntl.h>
#include <linux/delay.h>
#include <linux/wireless.h>
#include <net/iw_handler.h>
#include <linux/crc32.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <linux/moduleparam.h>
#include <linux/firmware.h>
#include <linux/jiffies.h>
#include <linux/ieee80211.h>
#include "atmel.h"

#define DRIVER_MAJOR 0
#define DRIVER_MINOR 98

MODULE_AUTHOR("Simon Kelley");
MODULE_DESCRIPTION("Support for Atmel at76c50x 802.11 wireless ethernet cards.");
MODULE_LICENSE("GPL");
MODULE_SUPPORTED_DEVICE("Atmel at76c50x wireless cards");

/* The name of the firmware file to be loaded
   over-rides any automatic selection */
static char *firmware = NULL;
module_param(firmware, charp, 0);

/* table of firmware file names */
static struct {
	AtmelFWType fw_type;
	const char *fw_file;
	const char *fw_file_ext;
} fw_table[] = {
	{ ATMEL_FW_TYPE_502,		"atmel_at76c502",	"bin" },
	{ ATMEL_FW_TYPE_502D,		"atmel_at76c502d",	"bin" },
	{ ATMEL_FW_TYPE_502E,		"atmel_at76c502e",	"bin" },
	{ ATMEL_FW_TYPE_502_3COM,	"atmel_at76c502_3com",	"bin" },
	{ ATMEL_FW_TYPE_504,		"atmel_at76c504",	"bin" },
	{ ATMEL_FW_TYPE_504_2958,	"atmel_at76c504_2958",	"bin" },
	{ ATMEL_FW_TYPE_504A_2958,	"atmel_at76c504a_2958",	"bin" },
	{ ATMEL_FW_TYPE_506,		"atmel_at76c506",	"bin" },
	{ ATMEL_FW_TYPE_NONE,		NULL,			NULL }
};

#define MAX_SSID_LENGTH 32
#define MGMT_JIFFIES (256 * HZ / 100)

#define MAX_BSS_ENTRIES	64

/* registers */
#define GCR  0x00    /* (SIR0)  General Configuration Register */
#define BSR  0x02    /* (SIR1)  Bank Switching Select Register */
#define AR   0x04
#define DR   0x08
#define MR1  0x12    /* Mirror Register 1 */
#define MR2  0x14    /* Mirror Register 2 */
#define MR3  0x16    /* Mirror Register 3 */
#define MR4  0x18    /* Mirror Register 4 */

#define GPR1                            0x0c
#define GPR2                            0x0e
#define GPR3                            0x10
/*
 * Constants for the GCR register.
 */
#define GCR_REMAP     0x0400          /* Remap internal SRAM to 0 */
#define GCR_SWRES     0x0080          /* BIU reset (ARM and PAI are NOT reset) */
#define GCR_CORES     0x0060          /* Core Reset (ARM and PAI are reset) */
#define GCR_ENINT     0x0002          /* Enable Interrupts */
#define GCR_ACKINT    0x0008          /* Acknowledge Interrupts */

#define BSS_SRAM      0x0200          /* AMBA module selection --> SRAM */
#define BSS_IRAM      0x0100          /* AMBA module selection --> IRAM */
/*
 *Constants for the MR registers.
 */
#define MAC_INIT_COMPLETE       0x0001        /* MAC init has been completed */
#define MAC_BOOT_COMPLETE       0x0010        /* MAC boot has been completed */
#define MAC_INIT_OK             0x0002        /* MAC boot has been completed */

#define MIB_MAX_DATA_BYTES    212
#define MIB_HEADER_SIZE       4    /* first four fields */

struct get_set_mib {
	u8 type;
	u8 size;
	u8 index;
	u8 reserved;
	u8 data[MIB_MAX_DATA_BYTES];
};

struct rx_desc {
	u32          Next;
	u16          MsduPos;
	u16          MsduSize;

	u8           State;
	u8           Status;
	u8           Rate;
	u8           Rssi;
	u8           LinkQuality;
	u8           PreambleType;
	u16          Duration;
	u32          RxTime;
};

#define RX_DESC_FLAG_VALID       0x80
#define RX_DESC_FLAG_CONSUMED    0x40
#define RX_DESC_FLAG_IDLE        0x00

#define RX_STATUS_SUCCESS        0x00

#define RX_DESC_MSDU_POS_OFFSET      4
#define RX_DESC_MSDU_SIZE_OFFSET     6
#define RX_DESC_FLAGS_OFFSET         8
#define RX_DESC_STATUS_OFFSET        9
#define RX_DESC_RSSI_OFFSET          11
#define RX_DESC_LINK_QUALITY_OFFSET  12
#define RX_DESC_PREAMBLE_TYPE_OFFSET 13
#define RX_DESC_DURATION_OFFSET      14
#define RX_DESC_RX_TIME_OFFSET       16

struct tx_desc {
	u32       NextDescriptor;
	u16       TxStartOfFrame;
	u16       TxLength;

	u8        TxState;
	u8        TxStatus;
	u8        RetryCount;

	u8        TxRate;

	u8        KeyIndex;
	u8        ChiperType;
	u8        ChipreLength;
	u8        Reserved1;

	u8        Reserved;
	u8        PacketType;
	u16       HostTxLength;
};

#define TX_DESC_NEXT_OFFSET          0
#define TX_DESC_POS_OFFSET           4
#define TX_DESC_SIZE_OFFSET          6
#define TX_DESC_FLAGS_OFFSET         8
#define TX_DESC_STATUS_OFFSET        9
#define TX_DESC_RETRY_OFFSET         10
#define TX_DESC_RATE_OFFSET          11
#define TX_DESC_KEY_INDEX_OFFSET     12
#define TX_DESC_CIPHER_TYPE_OFFSET   13
#define TX_DESC_CIPHER_LENGTH_OFFSET 14
#define TX_DESC_PACKET_TYPE_OFFSET   17
#define TX_DESC_HOST_LENGTH_OFFSET   18

/*
 * Host-MAC interface
 */

#define TX_STATUS_SUCCESS       0x00

#define TX_FIRM_OWN             0x80
#define TX_DONE                 0x40

#define TX_ERROR                0x01

#define TX_PACKET_TYPE_DATA     0x01
#define TX_PACKET_TYPE_MGMT     0x02

#define ISR_EMPTY               0x00        /* no bits set in ISR */
#define ISR_TxCOMPLETE          0x01        /* packet transmitted */
#define ISR_RxCOMPLETE          0x02        /* packet received */
#define ISR_RxFRAMELOST         0x04        /* Rx Frame lost */
#define ISR_FATAL_ERROR         0x08        /* Fatal error */
#define ISR_COMMAND_COMPLETE    0x10        /* command completed */
#define ISR_OUT_OF_RANGE        0x20        /* command completed */
#define ISR_IBSS_MERGE          0x40        /* (4.1.2.30): IBSS merge */
#define ISR_GENERIC_IRQ         0x80

#define Local_Mib_Type          0x01
#define Mac_Address_Mib_Type    0x02
#define Mac_Mib_Type            0x03
#define Statistics_Mib_Type     0x04
#define Mac_Mgmt_Mib_Type       0x05
#define Mac_Wep_Mib_Type        0x06
#define Phy_Mib_Type            0x07
#define Multi_Domain_MIB        0x08

#define MAC_MGMT_MIB_CUR_BSSID_POS            14
#define MAC_MIB_FRAG_THRESHOLD_POS            8
#define MAC_MIB_RTS_THRESHOLD_POS             10
#define MAC_MIB_SHORT_RETRY_POS               16
#define MAC_MIB_LONG_RETRY_POS                17
#define MAC_MIB_SHORT_RETRY_LIMIT_POS         16
#define MAC_MGMT_MIB_BEACON_PER_POS           0
#define MAC_MGMT_MIB_STATION_ID_POS           6
#define MAC_MGMT_MIB_CUR_PRIVACY_POS          11
#define MAC_MGMT_MIB_CUR_BSSID_POS            14
#define MAC_MGMT_MIB_PS_MODE_POS              53
#define MAC_MGMT_MIB_LISTEN_INTERVAL_POS      54
#define MAC_MGMT_MIB_MULTI_DOMAIN_IMPLEMENTED 56
#define MAC_MGMT_MIB_MULTI_DOMAIN_ENABLED     57
#define PHY_MIB_CHANNEL_POS                   14
#define PHY_MIB_RATE_SET_POS                  20
#define PHY_MIB_REG_DOMAIN_POS                26
#define LOCAL_MIB_AUTO_TX_RATE_POS            3
#define LOCAL_MIB_SSID_SIZE                   5
#define LOCAL_MIB_TX_PROMISCUOUS_POS          6
#define LOCAL_MIB_TX_MGMT_RATE_POS            7
#define LOCAL_MIB_TX_CONTROL_RATE_POS         8
#define LOCAL_MIB_PREAMBLE_TYPE               9
#define MAC_ADDR_MIB_MAC_ADDR_POS             0

#define         CMD_Set_MIB_Vars              0x01
#define         CMD_Get_MIB_Vars              0x02
#define         CMD_Scan                      0x03
#define         CMD_Join                      0x04
#define         CMD_Start                     0x05
#define         CMD_EnableRadio               0x06
#define         CMD_DisableRadio              0x07
#define         CMD_SiteSurvey                0x0B

#define         CMD_STATUS_IDLE                   0x00
#define         CMD_STATUS_COMPLETE               0x01
#define         CMD_STATUS_UNKNOWN                0x02
#define         CMD_STATUS_INVALID_PARAMETER      0x03
#define         CMD_STATUS_FUNCTION_NOT_SUPPORTED 0x04
#define         CMD_STATUS_TIME_OUT               0x07
#define         CMD_STATUS_IN_PROGRESS            0x08
#define         CMD_STATUS_REJECTED_RADIO_OFF     0x09
#define         CMD_STATUS_HOST_ERROR             0xFF
#define         CMD_STATUS_BUSY                   0xFE

#define CMD_BLOCK_COMMAND_OFFSET        0
#define CMD_BLOCK_STATUS_OFFSET         1
#define CMD_BLOCK_PARAMETERS_OFFSET     4

#define SCAN_OPTIONS_SITE_SURVEY        0x80

#define MGMT_FRAME_BODY_OFFSET		24
#define MAX_AUTHENTICATION_RETRIES	3
#define MAX_ASSOCIATION_RETRIES		3

#define AUTHENTICATION_RESPONSE_TIME_OUT  1000

#define MAX_WIRELESS_BODY  2316 /* mtu is 2312, CRC is 4 */
#define LOOP_RETRY_LIMIT   500000

#define ACTIVE_MODE	1
#define PS_MODE		2

#define MAX_ENCRYPTION_KEYS 4
#define MAX_ENCRYPTION_KEY_SIZE 40

/*
 * 802.11 related definitions
 */

/*
 * Regulatory Domains
 */

#define REG_DOMAIN_FCC		0x10	/* Channels	1-11	USA				*/
#define REG_DOMAIN_DOC		0x20	/* Channel	1-11	Canada				*/
#define REG_DOMAIN_ETSI		0x30	/* Channel	1-13	Europe (ex Spain/France)	*/
#define REG_DOMAIN_SPAIN	0x31	/* Channel	10-11	Spain				*/
#define REG_DOMAIN_FRANCE	0x32	/* Channel	10-13	France				*/
#define REG_DOMAIN_MKK		0x40	/* Channel	14	Japan				*/
#define REG_DOMAIN_MKK1		0x41	/* Channel	1-14	Japan(MKK1)			*/
#define REG_DOMAIN_ISRAEL	0x50	/* Channel	3-9	ISRAEL				*/

#define BSS_TYPE_AD_HOC		1
#define BSS_TYPE_INFRASTRUCTURE 2

#define SCAN_TYPE_ACTIVE	0
#define SCAN_TYPE_PASSIVE	1

#define LONG_PREAMBLE		0
#define SHORT_PREAMBLE		1
#define AUTO_PREAMBLE		2

#define DATA_FRAME_WS_HEADER_SIZE   30

/* promiscuous mode control */
#define PROM_MODE_OFF			0x0
#define PROM_MODE_UNKNOWN		0x1
#define PROM_MODE_CRC_FAILED		0x2
#define PROM_MODE_DUPLICATED		0x4
#define PROM_MODE_MGMT			0x8
#define PROM_MODE_CTRL			0x10
#define PROM_MODE_BAD_PROTOCOL		0x20

#define IFACE_INT_STATUS_OFFSET		0
#define IFACE_INT_MASK_OFFSET		1
#define IFACE_LOCKOUT_HOST_OFFSET	2
#define IFACE_LOCKOUT_MAC_OFFSET	3
#define IFACE_FUNC_CTRL_OFFSET		28
#define IFACE_MAC_STAT_OFFSET		30
#define IFACE_GENERIC_INT_TYPE_OFFSET	32

#define CIPHER_SUITE_NONE     0
#define CIPHER_SUITE_WEP_64   1
#define CIPHER_SUITE_TKIP     2
#define CIPHER_SUITE_AES      3
#define CIPHER_SUITE_CCX      4
#define CIPHER_SUITE_WEP_128  5

/*
 * IFACE MACROS & definitions
 */

/*
 * FuncCtrl field:
 */
#define FUNC_CTRL_TxENABLE		0x10
#define FUNC_CTRL_RxENABLE		0x20
#define FUNC_CTRL_INIT_COMPLETE		0x01

/* A stub firmware image which reads the MAC address from NVRAM on the card.
   For copyright information and source see the end of this file. */
static u8 mac_reader[] = {
	0x06, 0x00, 0x00, 0xea, 0x04, 0x00, 0x00, 0xea, 0x03, 0x00, 0x00, 0xea, 0x02, 0x00, 0x00, 0xea,
	0x01, 0x00, 0x00, 0xea, 0x00, 0x00, 0x00, 0xea, 0xff, 0xff, 0xff, 0xea, 0xfe, 0xff, 0xff, 0xea,
	0xd3, 0x00, 0xa0, 0xe3, 0x00, 0xf0, 0x21, 0xe1, 0x0e, 0x04, 0xa0, 0xe3, 0x00, 0x10, 0xa0, 0xe3,
	0x81, 0x11, 0xa0, 0xe1, 0x00, 0x10, 0x81, 0xe3, 0x00, 0x10, 0x80, 0xe5, 0x1c, 0x10, 0x90, 0xe5,
	0x10, 0x10, 0xc1, 0xe3, 0x1c, 0x10, 0x80, 0xe5, 0x01, 0x10, 0xa0, 0xe3, 0x08, 0x10, 0x80, 0xe5,
	0x02, 0x03, 0xa0, 0xe3, 0x00, 0x10, 0xa0, 0xe3, 0xb0, 0x10, 0xc0, 0xe1, 0xb4, 0x10, 0xc0, 0xe1,
	0xb8, 0x10, 0xc0, 0xe1, 0xbc, 0x10, 0xc0, 0xe1, 0x56, 0xdc, 0xa0, 0xe3, 0x21, 0x00, 0x00, 0xeb,
	0x0a, 0x00, 0xa0, 0xe3, 0x1a, 0x00, 0x00, 0xeb, 0x10, 0x00, 0x00, 0xeb, 0x07, 0x00, 0x00, 0xeb,
	0x02, 0x03, 0xa0, 0xe3, 0x02, 0x14, 0xa0, 0xe3, 0xb4, 0x10, 0xc0, 0xe1, 0x4c, 0x10, 0x9f, 0xe5,
	0xbc, 0x10, 0xc0, 0xe1, 0x10, 0x10, 0xa0, 0xe3, 0xb8, 0x10, 0xc0, 0xe1, 0xfe, 0xff, 0xff, 0xea,
	0x00, 0x40, 0x2d, 0xe9, 0x00, 0x20, 0xa0, 0xe3, 0x02, 0x3c, 0xa0, 0xe3, 0x00, 0x10, 0xa0, 0xe3,
	0x28, 0x00, 0x9f, 0xe5, 0x37, 0x00, 0x00, 0xeb, 0x00, 0x40, 0xbd, 0xe8, 0x1e, 0xff, 0x2f, 0xe1,
	0x00, 0x40, 0x2d, 0xe9, 0x12, 0x2e, 0xa0, 0xe3, 0x06, 0x30, 0xa0, 0xe3, 0x00, 0x10, 0xa0, 0xe3,
	0x02, 0x04, 0xa0, 0xe3, 0x2f, 0x00, 0x00, 0xeb, 0x00, 0x40, 0xbd, 0xe8, 0x1e, 0xff, 0x2f, 0xe1,
	0x00, 0x02, 0x00, 0x02, 0x80, 0x01, 0x90, 0xe0, 0x01, 0x00, 0x00, 0x0a, 0x01, 0x00, 0x50, 0xe2,
	0xfc, 0xff, 0xff, 0xea, 0x1e, 0xff, 0x2f, 0xe1, 0x80, 0x10, 0xa0, 0xe3, 0xf3, 0x06, 0xa0, 0xe3,
	0x00, 0x10, 0x80, 0xe5, 0x00, 0x10, 0xa0, 0xe3, 0x00, 0x10, 0x80, 0xe5, 0x01, 0x10, 0xa0, 0xe3,
	0x04, 0x10, 0x80, 0xe5, 0x00, 0x10, 0x80, 0xe5, 0x0e, 0x34, 0xa0, 0xe3, 0x1c, 0x10, 0x93, 0xe5,
	0x02, 0x1a, 0x81, 0xe3, 0x1c, 0x10, 0x83, 0xe5, 0x58, 0x11, 0x9f, 0xe5, 0x30, 0x10, 0x80, 0xe5,
	0x54, 0x11, 0x9f, 0xe5, 0x34, 0x10, 0x80, 0xe5, 0x38, 0x10, 0x80, 0xe5, 0x3c, 0x10, 0x80, 0xe5,
	0x10, 0x10, 0x90, 0xe5, 0x08, 0x00, 0x90, 0xe5, 0x1e, 0xff, 0x2f, 0xe1, 0xf3, 0x16, 0xa0, 0xe3,
	0x08, 0x00, 0x91, 0xe5, 0x05, 0x00, 0xa0, 0xe3, 0x0c, 0x00, 0x81, 0xe5, 0x10, 0x00, 0x91, 0xe5,
	0x02, 0x00, 0x10, 0xe3, 0xfc, 0xff, 0xff, 0x0a, 0xff, 0x00, 0xa0, 0xe3, 0x0c, 0x00, 0x81, 0xe5,
	0x10, 0x00, 0x91, 0xe5, 0x02, 0x00, 0x10, 0xe3, 0xfc, 0xff, 0xff, 0x0a, 0x08, 0x00, 0x91, 0xe5,
	0x10, 0x00, 0x91, 0xe5, 0x01, 0x00, 0x10, 0xe3, 0xfc, 0xff, 0xff, 0x0a, 0x08, 0x00, 0x91, 0xe5,
	0xff, 0x00, 0x00, 0xe2, 0x1e, 0xff, 0x2f, 0xe1, 0x30, 0x40, 0x2d, 0xe9, 0x00, 0x50, 0xa0, 0xe1,
	0x03, 0x40, 0xa0, 0xe1, 0xa2, 0x02, 0xa0, 0xe1, 0x08, 0x00, 0x00, 0xe2, 0x03, 0x00, 0x80, 0xe2,
	0xd8, 0x10, 0x9f, 0xe5, 0x00, 0x00, 0xc1, 0xe5, 0x01, 0x20, 0xc1, 0xe5, 0xe2, 0xff, 0xff, 0xeb,
	0x01, 0x00, 0x10, 0xe3, 0xfc, 0xff, 0xff, 0x1a, 0x14, 0x00, 0xa0, 0xe3, 0xc4, 0xff, 0xff, 0xeb,
	0x04, 0x20, 0xa0, 0xe1, 0x05, 0x10, 0xa0, 0xe1, 0x02, 0x00, 0xa0, 0xe3, 0x01, 0x00, 0x00, 0xeb,
	0x30, 0x40, 0xbd, 0xe8, 0x1e, 0xff, 0x2f, 0xe1, 0x70, 0x40, 0x2d, 0xe9, 0xf3, 0x46, 0xa0, 0xe3,
	0x00, 0x30, 0xa0, 0xe3, 0x00, 0x00, 0x50, 0xe3, 0x08, 0x00, 0x00, 0x9a, 0x8c, 0x50, 0x9f, 0xe5,
	0x03, 0x60, 0xd5, 0xe7, 0x0c, 0x60, 0x84, 0xe5, 0x10, 0x60, 0x94, 0xe5, 0x02, 0x00, 0x16, 0xe3,
	0xfc, 0xff, 0xff, 0x0a, 0x01, 0x30, 0x83, 0xe2, 0x00, 0x00, 0x53, 0xe1, 0xf7, 0xff, 0xff, 0x3a,
	0xff, 0x30, 0xa0, 0xe3, 0x0c, 0x30, 0x84, 0xe5, 0x08, 0x00, 0x94, 0xe5, 0x10, 0x00, 0x94, 0xe5,
	0x01, 0x00, 0x10, 0xe3, 0xfc, 0xff, 0xff, 0x0a, 0x08, 0x00, 0x94, 0xe5, 0x00, 0x00, 0xa0, 0xe3,
	0x00, 0x00, 0x52, 0xe3, 0x0b, 0x00, 0x00, 0x9a, 0x10, 0x50, 0x94, 0xe5, 0x02, 0x00, 0x15, 0xe3,
	0xfc, 0xff, 0xff, 0x0a, 0x0c, 0x30, 0x84, 0xe5, 0x10, 0x50, 0x94, 0xe5, 0x01, 0x00, 0x15, 0xe3,
	0xfc, 0xff, 0xff, 0x0a, 0x08, 0x50, 0x94, 0xe5, 0x01, 0x50, 0xc1, 0xe4, 0x01, 0x00, 0x80, 0xe2,
	0x02, 0x00, 0x50, 0xe1, 0xf3, 0xff, 0xff, 0x3a, 0xc8, 0x00, 0xa0, 0xe3, 0x98, 0xff, 0xff, 0xeb,
	0x70, 0x40, 0xbd, 0xe8, 0x1e, 0xff, 0x2f, 0xe1, 0x01, 0x0c, 0x00, 0x02, 0x01, 0x02, 0x00, 0x02,
	0x00, 0x01, 0x00, 0x02
};

struct atmel_private {
	void *card; /* Bus dependent stucture varies for PCcard */
	int (*present_callback)(void *); /* And callback which uses it */
	char firmware_id[32];
	AtmelFWType firmware_type;
	u8 *firmware;
	int firmware_length;
	struct timer_list management_timer;
	struct net_device *dev;
	struct device *sys_dev;
	struct iw_statistics wstats;
	spinlock_t irqlock, timerlock;	/* spinlocks */
	enum { BUS_TYPE_PCCARD, BUS_TYPE_PCI } bus_type;
	enum {
		CARD_TYPE_PARALLEL_FLASH,
		CARD_TYPE_SPI_FLASH,
		CARD_TYPE_EEPROM
	} card_type;
	int do_rx_crc; /* If we need to CRC incoming packets */
	int probe_crc; /* set if we don't yet know */
	int crc_ok_cnt, crc_ko_cnt; /* counters for probing */
	u16 rx_desc_head;
	u16 tx_desc_free, tx_desc_head, tx_desc_tail, tx_desc_previous;
	u16 tx_free_mem, tx_buff_head, tx_buff_tail;

	u16 frag_seq, frag_len, frag_no;
	u8 frag_source[6];

	u8 wep_is_on, default_key, exclude_unencrypted, encryption_level;
	u8 group_cipher_suite, pairwise_cipher_suite;
	u8 wep_keys[MAX_ENCRYPTION_KEYS][MAX_ENCRYPTION_KEY_SIZE];
	int wep_key_len[MAX_ENCRYPTION_KEYS];
	int use_wpa, radio_on_broken; /* firmware dependent stuff. */

	u16 host_info_base;
	struct host_info_struct {
		/* NB this is matched to the hardware, don't change. */
		u8 volatile int_status;
		u8 volatile int_mask;
		u8 volatile lockout_host;
		u8 volatile lockout_mac;

		u16 tx_buff_pos;
		u16 tx_buff_size;
		u16 tx_desc_pos;
		u16 tx_desc_count;

		u16 rx_buff_pos;
		u16 rx_buff_size;
		u16 rx_desc_pos;
		u16 rx_desc_count;

		u16 build_version;
		u16 command_pos;

		u16 major_version;
		u16 minor_version;

		u16 func_ctrl;
		u16 mac_status;
		u16 generic_IRQ_type;
		u8  reserved[2];
	} host_info;

	enum {
		STATION_STATE_SCANNING,
		STATION_STATE_JOINNING,
		STATION_STATE_AUTHENTICATING,
		STATION_STATE_ASSOCIATING,
		STATION_STATE_READY,
		STATION_STATE_REASSOCIATING,
		STATION_STATE_DOWN,
		STATION_STATE_MGMT_ERROR
	} station_state;

	int operating_mode, power_mode;
	time_t last_qual;
	int beacons_this_sec;
	int channel;
	int reg_domain, config_reg_domain;
	int tx_rate;
	int auto_tx_rate;
	int rts_threshold;
	int frag_threshold;
	int long_retry, short_retry;
	int preamble;
	int default_beacon_period, beacon_period, listen_interval;
	int CurrentAuthentTransactionSeqNum, ExpectedAuthentTransactionSeqNum;
	int AuthenticationRequestRetryCnt, AssociationRequestRetryCnt, ReAssociationRequestRetryCnt;
	enum {
		SITE_SURVEY_IDLE,
		SITE_SURVEY_IN_PROGRESS,
		SITE_SURVEY_COMPLETED
	} site_survey_state;
	unsigned long last_survey;

	int station_was_associated, station_is_associated;
	int fast_scan;

	struct bss_info {
		int channel;
		int SSIDsize;
		int RSSI;
		int UsingWEP;
		int preamble;
		int beacon_period;
		int BSStype;
		u8 BSSID[6];
		u8 SSID[MAX_SSID_LENGTH];
	} BSSinfo[MAX_BSS_ENTRIES];
	int BSS_list_entries, current_BSS;
	int connect_to_any_BSS;
	int SSID_size, new_SSID_size;
	u8 CurrentBSSID[6], BSSID[6];
	u8 SSID[MAX_SSID_LENGTH], new_SSID[MAX_SSID_LENGTH];
	u64 last_beacon_timestamp;
	u8 rx_buf[MAX_WIRELESS_BODY];
};

static u8 atmel_basic_rates[4] = {0x82, 0x84, 0x0b, 0x16};

static const struct {
	int reg_domain;
	int min, max;
	char *name;
} channel_table[] = { { REG_DOMAIN_FCC, 1, 11, "USA" },
		      { REG_DOMAIN_DOC, 1, 11, "Canada" },
		      { REG_DOMAIN_ETSI, 1, 13, "Europe" },
		      { REG_DOMAIN_SPAIN, 10, 11, "Spain" },
		      { REG_DOMAIN_FRANCE, 10, 13, "France" },
		      { REG_DOMAIN_MKK, 14, 14, "MKK" },
		      { REG_DOMAIN_MKK1, 1, 14, "MKK1" },
		      { REG_DOMAIN_ISRAEL, 3, 9, "Israel"} };

static void build_wpa_mib(struct atmel_private *priv);
static int atmel_ioctl(struct net_device *dev, struct ifreq *rq, int cmd);
static void atmel_copy_to_card(struct net_device *dev, u16 dest,
			       const unsigned char *src, u16 len);
static void atmel_copy_to_host(struct net_device *dev, unsigned char *dest,
			       u16 src, u16 len);
static void atmel_set_gcr(struct net_device *dev, u16 mask);
static void atmel_clear_gcr(struct net_device *dev, u16 mask);
static int atmel_lock_mac(struct atmel_private *priv);
static void atmel_wmem32(struct atmel_private *priv, u16 pos, u32 data);
static void atmel_command_irq(struct atmel_private *priv);
static int atmel_validate_channel(struct atmel_private *priv, int channel);
static void atmel_management_frame(struct atmel_private *priv,
				   struct ieee80211_hdr *header,
				   u16 frame_len, u8 rssi);
static void atmel_management_timer(u_long a);
static void atmel_send_command(struct atmel_private *priv, int command,
			       void *cmd, int cmd_size);
static int atmel_send_command_wait(struct atmel_private *priv, int command,
				   void *cmd, int cmd_size);
static void atmel_transmit_management_frame(struct atmel_private *priv,
					    struct ieee80211_hdr *header,
					    u8 *body, int body_len);

static u8 atmel_get_mib8(struct atmel_private *priv, u8 type, u8 index);
static void atmel_set_mib8(struct atmel_private *priv, u8 type, u8 index,
			   u8 data);
static void atmel_set_mib16(struct atmel_private *priv, u8 type, u8 index,
			    u16 data);
static void atmel_set_mib(struct atmel_private *priv, u8 type, u8 index,
			  u8 *data, int data_len);
static void atmel_get_mib(struct atmel_private *priv, u8 type, u8 index,
			  u8 *data, int data_len);
static void atmel_scan(struct atmel_private *priv, int specific_ssid);
static void atmel_join_bss(struct atmel_private *priv, int bss_index);
static void atmel_smooth_qual(struct atmel_private *priv);
static void atmel_writeAR(struct net_device *dev, u16 data);
static int probe_atmel_card(struct net_device *dev);
static int reset_atmel_card(struct net_device *dev);
static void atmel_enter_state(struct atmel_private *priv, int new_state);
int atmel_open (struct net_device *dev);

static inline u16 atmel_hi(struct atmel_private *priv, u16 offset)
{
	return priv->host_info_base + offset;
}

static inline u16 atmel_co(struct atmel_private *priv, u16 offset)
{
	return priv->host_info.command_pos + offset;
}

static inline u16 atmel_rx(struct atmel_private *priv, u16 offset, u16 desc)
{
	return priv->host_info.rx_desc_pos + (sizeof(struct rx_desc) * desc) + offset;
}

static inline u16 atmel_tx(struct atmel_private *priv, u16 offset, u16 desc)
{
	return priv->host_info.tx_desc_pos + (sizeof(struct tx_desc) * desc) + offset;
}

static inline u8 atmel_read8(struct net_device *dev, u16 offset)
{
	return inb(dev->base_addr + offset);
}

static inline void atmel_write8(struct net_device *dev, u16 offset, u8 data)
{
	outb(data, dev->base_addr + offset);
}

static inline u16 atmel_read16(struct net_device *dev, u16 offset)
{
	return inw(dev->base_addr + offset);
}

static inline void atmel_write16(struct net_device *dev, u16 offset, u16 data)
{
	outw(data, dev->base_addr + offset);
}

static inline u8 atmel_rmem8(struct atmel_private *priv, u16 pos)
{
	atmel_writeAR(priv->dev, pos);
	return atmel_read8(priv->dev, DR);
}

static inline void atmel_wmem8(struct atmel_private *priv, u16 pos, u16 data)
{
	atmel_writeAR(priv->dev, pos);
	atmel_write8(priv->dev, DR, data);
}

static inline u16 atmel_rmem16(struct atmel_private *priv, u16 pos)
{
	atmel_writeAR(priv->dev, pos);
	return atmel_read16(priv->dev, DR);
}

static inline void atmel_wmem16(struct atmel_private *priv, u16 pos, u16 data)
{
	atmel_writeAR(priv->dev, pos);
	atmel_write16(priv->dev, DR, data);
}

static const struct iw_handler_def atmel_handler_def;

static void tx_done_irq(struct atmel_private *priv)
{
	int i;

	for (i = 0;
	     atmel_rmem8(priv, atmel_tx(priv, TX_DESC_FLAGS_OFFSET, priv->tx_desc_head)) == TX_DONE &&
		     i < priv->host_info.tx_desc_count;
	     i++) {
		u8 status = atmel_rmem8(priv, atmel_tx(priv, TX_DESC_STATUS_OFFSET, priv->tx_desc_head));
		u16 msdu_size = atmel_rmem16(priv, atmel_tx(priv, TX_DESC_SIZE_OFFSET, priv->tx_desc_head));
		u8 type = atmel_rmem8(priv, atmel_tx(priv, TX_DESC_PACKET_TYPE_OFFSET, priv->tx_desc_head));

		atmel_wmem8(priv, atmel_tx(priv, TX_DESC_FLAGS_OFFSET, priv->tx_desc_head), 0);

		priv->tx_free_mem += msdu_size;
		priv->tx_desc_free++;

		if (priv->tx_buff_head + msdu_size > (priv->host_info.tx_buff_pos + priv->host_info.tx_buff_size))
			priv->tx_buff_head = 0;
		else
			priv->tx_buff_head += msdu_size;

		if (priv->tx_desc_head < (priv->host_info.tx_desc_count - 1))
			priv->tx_desc_head++ ;
		else
			priv->tx_desc_head = 0;

		if (type == TX_PACKET_TYPE_DATA) {
			if (status == TX_STATUS_SUCCESS)
				priv->dev->stats.tx_packets++;
			else
				priv->dev->stats.tx_errors++;
			netif_wake_queue(priv->dev);
		}
	}
}

static u16 find_tx_buff(struct atmel_private *priv, u16 len)
{
	u16 bottom_free = priv->host_info.tx_buff_size - priv->tx_buff_tail;

	if (priv->tx_desc_free == 3 || priv->tx_free_mem < len)
		return 0;

	if (bottom_free >= len)
		return priv->host_info.tx_buff_pos + priv->tx_buff_tail;

	if (priv->tx_free_mem - bottom_free >= len) {
		priv->tx_buff_tail = 0;
		return priv->host_info.tx_buff_pos;
	}

	return 0;
}

static void tx_update_descriptor(struct atmel_private *priv, int is_bcast,
				 u16 len, u16 buff, u8 type)
{
	atmel_wmem16(priv, atmel_tx(priv, TX_DESC_POS_OFFSET, priv->tx_desc_tail), buff);
	atmel_wmem16(priv, atmel_tx(priv, TX_DESC_SIZE_OFFSET, priv->tx_desc_tail), len);
	if (!priv->use_wpa)
		atmel_wmem16(priv, atmel_tx(priv, TX_DESC_HOST_LENGTH_OFFSET, priv->tx_desc_tail), len);
	atmel_wmem8(priv, atmel_tx(priv, TX_DESC_PACKET_TYPE_OFFSET, priv->tx_desc_tail), type);
	atmel_wmem8(priv, atmel_tx(priv, TX_DESC_RATE_OFFSET, priv->tx_desc_tail), priv->tx_rate);
	atmel_wmem8(priv, atmel_tx(priv, TX_DESC_RETRY_OFFSET, priv->tx_desc_tail), 0);
	if (priv->use_wpa) {
		int cipher_type, cipher_length;
		if (is_bcast) {
			cipher_type = priv->group_cipher_suite;
			if (cipher_type == CIPHER_SUITE_WEP_64 ||
			    cipher_type == CIPHER_SUITE_WEP_128)
				cipher_length = 8;
			else if (cipher_type == CIPHER_SUITE_TKIP)
				cipher_length = 12;
			else if (priv->pairwise_cipher_suite == CIPHER_SUITE_WEP_64 ||
				 priv->pairwise_cipher_suite == CIPHER_SUITE_WEP_128) {
				cipher_type = priv->pairwise_cipher_suite;
				cipher_length = 8;
			} else {
				cipher_type = CIPHER_SUITE_NONE;
				cipher_length = 0;
			}
		} else {
			cipher_type = priv->pairwise_cipher_suite;
			if (cipher_type == CIPHER_SUITE_WEP_64 ||
			    cipher_type == CIPHER_SUITE_WEP_128)
				cipher_length = 8;
			else if (cipher_type == CIPHER_SUITE_TKIP)
				cipher_length = 12;
			else if (priv->group_cipher_suite == CIPHER_SUITE_WEP_64 ||
				 priv->group_cipher_suite == CIPHER_SUITE_WEP_128) {
				cipher_type = priv->group_cipher_suite;
				cipher_length = 8;
			} else {
				cipher_type = CIPHER_SUITE_NONE;
				cipher_length = 0;
			}
		}

		atmel_wmem8(priv, atmel_tx(priv, TX_DESC_CIPHER_TYPE_OFFSET, priv->tx_desc_tail),
			    cipher_type);
		atmel_wmem8(priv, atmel_tx(priv, TX_DESC_CIPHER_LENGTH_OFFSET, priv->tx_desc_tail),
			    cipher_length);
	}
	atmel_wmem32(priv, atmel_tx(priv, TX_DESC_NEXT_OFFSET, priv->tx_desc_tail), 0x80000000L);
	atmel_wmem8(priv, atmel_tx(priv, TX_DESC_FLAGS_OFFSET, priv->tx_desc_tail), TX_FIRM_OWN);
	if (priv->tx_desc_previous != priv->tx_desc_tail)
		atmel_wmem32(priv, atmel_tx(priv, TX_DESC_NEXT_OFFSET, priv->tx_desc_previous), 0);
	priv->tx_desc_previous = priv->tx_desc_tail;
	if (priv->tx_desc_tail < (priv->host_info.tx_desc_count - 1))
		priv->tx_desc_tail++;
	else
		priv->tx_desc_tail = 0;
	priv->tx_desc_free--;
	priv->tx_free_mem -= len;
}

static netdev_tx_t start_tx(struct sk_buff *skb, struct net_device *dev)
{
	static const u8 SNAP_RFC1024[6] = { 0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00 };
	struct atmel_private *priv = netdev_priv(dev);
	struct ieee80211_hdr header;
	unsigned long flags;
	u16 buff, frame_ctl, len = (ETH_ZLEN < skb->len) ? skb->len : ETH_ZLEN;

	if (priv->card && priv->present_callback &&
	    !(*priv->present_callback)(priv->card)) {
		dev->stats.tx_errors++;
		dev_kfree_skb(skb);
		return NETDEV_TX_OK;
	}

	if (priv->station_state != STATION_STATE_READY) {
		dev->stats.tx_errors++;
		dev_kfree_skb(skb);
		return NETDEV_TX_OK;
	}

	/* first ensure the timer func cannot run */
	spin_lock_bh(&priv->timerlock);
	/* then stop the hardware ISR */
	spin_lock_irqsave(&priv->irqlock, flags);
	/* nb doing the above in the opposite order will deadlock */

	/* The Wireless Header is 30 bytes. In the Ethernet packet we "cut" the
	   12 first bytes (containing DA/SA) and put them in the appropriate
	   fields of the Wireless Header. Thus the packet length is then the
	   initial + 18 (+30-12) */

	if (!(buff = find_tx_buff(priv, len + 18))) {
		dev->stats.tx_dropped++;
		spin_unlock_irqrestore(&priv->irqlock, flags);
		spin_unlock_bh(&priv->timerlock);
		netif_stop_queue(dev);
		return NETDEV_TX_BUSY;
	}

	frame_ctl = IEEE80211_FTYPE_DATA;
	header.duration_id = 0;
	header.seq_ctrl = 0;
	if (priv->wep_is_on)
		frame_ctl |= IEEE80211_FCTL_PROTECTED;
	if (priv->operating_mode == IW_MODE_ADHOC) {
		skb_copy_from_linear_data(skb, &header.addr1, 6);
		memcpy(&header.addr2, dev->dev_addr, 6);
		memcpy(&header.addr3, priv->BSSID, 6);
	} else {
		frame_ctl |= IEEE80211_FCTL_TODS;
		memcpy(&header.addr1, priv->CurrentBSSID, 6);
		memcpy(&header.addr2, dev->dev_addr, 6);
		skb_copy_from_linear_data(skb, &header.addr3, 6);
	}

	if (priv->use_wpa)
		memcpy(&header.addr4, SNAP_RFC1024, 6);

	header.frame_control = cpu_to_le16(frame_ctl);
	/* Copy the wireless header into the card */
	atmel_copy_to_card(dev, buff, (unsigned char *)&header, DATA_FRAME_WS_HEADER_SIZE);
	/* Copy the packet sans its 802.3 header addresses which have been replaced */
	atmel_copy_to_card(dev, buff + DATA_FRAME_WS_HEADER_SIZE, skb->data + 12, len - 12);
	priv->tx_buff_tail += len - 12 + DATA_FRAME_WS_HEADER_SIZE;

	/* low bit of first byte of destination tells us if broadcast */
	tx_update_descriptor(priv, *(skb->data) & 0x01, len + 18, buff, TX_PACKET_TYPE_DATA);
	dev->trans_start = jiffies;
	dev->stats.tx_bytes += len;

	spin_unlock_irqrestore(&priv->irqlock, flags);
	spin_unlock_bh(&priv->timerlock);
	dev_kfree_skb(skb);

	return NETDEV_TX_OK;
}

static void atmel_transmit_management_frame(struct atmel_private *priv,
					    struct ieee80211_hdr *header,
					    u8 *body, int body_len)
{
	u16 buff;
	int len = MGMT_FRAME_BODY_OFFSET + body_len;

	if (!(buff = find_tx_buff(priv, len)))
		return;

	atmel_copy_to_card(priv->dev, buff, (u8 *)header, MGMT_FRAME_BODY_OFFSET);
	atmel_copy_to_card(priv->dev, buff + MGMT_FRAME_BODY_OFFSET, body, body_len);
	priv->tx_buff_tail += len;
	tx_update_descriptor(priv, header->addr1[0] & 0x01, len, buff, TX_PACKET_TYPE_MGMT);
}

static void fast_rx_path(struct atmel_private *priv,
			 struct ieee80211_hdr *header,
			 u16 msdu_size, u16 rx_packet_loc, u32 crc)
{
	/* fast path: unfragmented packet copy directly into skbuf */
	u8 mac4[6];
	struct sk_buff	*skb;
	unsigned char *skbp;

	/* get the final, mac 4 header field, this tells us encapsulation */
	atmel_copy_to_host(priv->dev, mac4, rx_packet_loc + 24, 6);
	msdu_size -= 6;

	if (priv->do_rx_crc) {
		crc = crc32_le(crc, mac4, 6);
		msdu_size -= 4;
	}

	if (!(skb = dev_alloc_skb(msdu_size + 14))) {
		priv->dev->stats.rx_dropped++;
		return;
	}

	skb_reserve(skb, 2);
	skbp = skb_put(skb, msdu_size + 12);
	atmel_copy_to_host(priv->dev, skbp + 12, rx_packet_loc + 30, msdu_size);

	if (priv->do_rx_crc) {
		u32 netcrc;
		crc = crc32_le(crc, skbp + 12, msdu_size);
		atmel_copy_to_host(priv->dev, (void *)&netcrc, rx_packet_loc + 30 + msdu_size, 4);
		if ((crc ^ 0xffffffff) != netcrc) {
			priv->dev->stats.rx_crc_errors++;
			dev_kfree_skb(skb);
			return;
		}
	}

	memcpy(skbp, header->addr1, 6); /* destination address */
	if (le16_to_cpu(header->frame_control) & IEEE80211_FCTL_FROMDS)
		memcpy(&skbp[6], header->addr3, 6);
	else
		memcpy(&skbp[6], header->addr2, 6); /* source address */

	skb->protocol = eth_type_trans(skb, priv->dev);
	skb->ip_summed = CHECKSUM_NONE;
	netif_rx(skb);
	priv->dev->stats.rx_bytes += 12 + msdu_size;
	priv->dev->stats.rx_packets++;
}

/* Test to see if the packet in card memory at packet_loc has a valid CRC
   It doesn't matter that this is slow: it is only used to proble the first few
   packets. */
static int probe_crc(struct atmel_private *priv, u16 packet_loc, u16 msdu_size)
{
	int i = msdu_size - 4;
	u32 netcrc, crc = 0xffffffff;

	if (msdu_size < 4)
		return 0;

	atmel_copy_to_host(priv->dev, (void *)&netcrc, packet_loc + i, 4);

	atmel_writeAR(priv->dev, packet_loc);
	while (i--) {
		u8 octet = atmel_read8(priv->dev, DR);
		crc = crc32_le(crc, &octet, 1);
	}

	return (crc ^ 0xffffffff) == netcrc;
}

static void frag_rx_path(struct atmel_private *priv,
			 struct ieee80211_hdr *header,
			 u16 msdu_size, u16 rx_packet_loc, u32 crc, u16 seq_no,
			 u8 frag_no, int more_frags)
{
	u8 mac4[6];
	u8 source[6];
	struct sk_buff *skb;

	if (le16_to_cpu(header->frame_control) & IEEE80211_FCTL_FROMDS)
		memcpy(source, header->addr3, 6);
	else
		memcpy(source, header->addr2, 6);

	rx_packet_loc += 24; /* skip header */

	if (priv->do_rx_crc)
		msdu_size -= 4;

	if (frag_no == 0) { /* first fragment */
		atmel_copy_to_host(priv->dev, mac4, rx_packet_loc, 6);
		msdu_size -= 6;
		rx_packet_loc += 6;

		if (priv->do_rx_crc)
			crc = crc32_le(crc, mac4, 6);

		priv->frag_seq = seq_no;
		priv->frag_no = 1;
		priv->frag_len = msdu_size;
		memcpy(priv->frag_source, source, 6);
		memcpy(&priv->rx_buf[6], source, 6);
		memcpy(priv->rx_buf, header->addr1, 6);

		atmel_copy_to_host(priv->dev, &priv->rx_buf[12], rx_packet_loc, msdu_size);

		if (priv->do_rx_crc) {
			u32 netcrc;
			crc = crc32_le(crc, &priv->rx_buf[12], msdu_size);
			atmel_copy_to_host(priv->dev, (void *)&netcrc, rx_packet_loc + msdu_size, 4);
			if ((crc ^ 0xffffffff) != netcrc) {
				priv->dev->stats.rx_crc_errors++;
				memset(priv->frag_source, 0xff, 6);
			}
		}

	} else if (priv->frag_no == frag_no &&
		   priv->frag_seq == seq_no &&
		   memcmp(priv->frag_source, source, 6) == 0) {

		atmel_copy_to_host(priv->dev, &priv->rx_buf[12 + priv->frag_len],
				   rx_packet_loc, msdu_size);
		if (priv->do_rx_crc) {
			u32 netcrc;
			crc = crc32_le(crc,
				       &priv->rx_buf[12 + priv->frag_len],
				       msdu_size);
			atmel_copy_to_host(priv->dev, (void *)&netcrc, rx_packet_loc + msdu_size, 4);
			if ((crc ^ 0xffffffff) != netcrc) {
				priv->dev->stats.rx_crc_errors++;
				memset(priv->frag_source, 0xff, 6);
				more_frags = 1; /* don't send broken assembly */
			}
		}

		priv->frag_len += msdu_size;
		priv->frag_no++;

		if (!more_frags) { /* last one */
			memset(priv->frag_source, 0xff, 6);
			if (!(skb = dev_alloc_skb(priv->frag_len + 14))) {
				priv->dev->stats.rx_dropped++;
			} else {
				skb_reserve(skb, 2);
				memcpy(skb_put(skb, priv->frag_len + 12),
				       priv->rx_buf,
				       priv->frag_len + 12);
				skb->protocol = eth_type_trans(skb, priv->dev);
				skb->ip_summed = CHECKSUM_NONE;
				netif_rx(skb);
				priv->dev->stats.rx_bytes += priv->frag_len + 12;
				priv->dev->stats.rx_packets++;
			}
		}
	} else
		priv->wstats.discard.fragment++;
}

static void rx_done_irq(struct atmel_private *priv)
{
	int i;
	struct ieee80211_hdr header;

	for (i = 0;
	     atmel_rmem8(priv, atmel_rx(priv, RX_DESC_FLAGS_OFFSET, priv->rx_desc_head)) == RX_DESC_FLAG_VALID &&
		     i < priv->host_info.rx_desc_count;
	     i++) {

		u16 msdu_size, rx_packet_loc, frame_ctl, seq_control;
		u8 status = atmel_rmem8(priv, atmel_rx(priv, RX_DESC_STATUS_OFFSET, priv->rx_desc_head));
		u32 crc = 0xffffffff;

		if (status != RX_STATUS_SUCCESS) {
			if (status == 0xc1) /* determined by experiment */
				priv->wstats.discard.nwid++;
			else
				priv->dev->stats.rx_errors++;
			goto next;
		}

		msdu_size = atmel_rmem16(priv, atmel_rx(priv, RX_DESC_MSDU_SIZE_OFFSET, priv->rx_desc_head));
		rx_packet_loc = atmel_rmem16(priv, atmel_rx(priv, RX_DESC_MSDU_POS_OFFSET, priv->rx_desc_head));

		if (msdu_size < 30) {
			priv->dev->stats.rx_errors++;
			goto next;
		}

		/* Get header as far as end of seq_ctrl */
		atmel_copy_to_host(priv->dev, (char *)&header, rx_packet_loc, 24);
		frame_ctl = le16_to_cpu(header.frame_control);
		seq_control = le16_to_cpu(header.seq_ctrl);

		/* probe for CRC use here if needed  once five packets have
		   arrived with the same crc status, we assume we know what's
		   happening and stop probing */
		if (priv->probe_crc) {
			if (!priv->wep_is_on || !(frame_ctl & IEEE80211_FCTL_PROTECTED)) {
				priv->do_rx_crc = probe_crc(priv, rx_packet_loc, msdu_size);
			} else {
				priv->do_rx_crc = probe_crc(priv, rx_packet_loc + 24, msdu_size - 24);
			}
			if (priv->do_rx_crc) {
				if (priv->crc_ok_cnt++ > 5)
					priv->probe_crc = 0;
			} else {
				if (priv->crc_ko_cnt++ > 5)
					priv->probe_crc = 0;
			}
		}

		/* don't CRC header when WEP in use */
		if (priv->do_rx_crc && (!priv->wep_is_on || !(frame_ctl & IEEE80211_FCTL_PROTECTED))) {
			crc = crc32_le(0xffffffff, (unsigned char *)&header, 24);
		}
		msdu_size -= 24; /* header */

		if ((frame_ctl & IEEE80211_FCTL_FTYPE) == IEEE80211_FTYPE_DATA) {
			int more_fragments = frame_ctl & IEEE80211_FCTL_MOREFRAGS;
			u8 packet_fragment_no = seq_control & IEEE80211_SCTL_FRAG;
			u16 packet_sequence_no = (seq_control & IEEE80211_SCTL_SEQ) >> 4;

			if (!more_fragments && packet_fragment_no == 0) {
				fast_rx_path(priv, &header, msdu_size, rx_packet_loc, crc);
			} else {
				frag_rx_path(priv, &header, msdu_size, rx_packet_loc, crc,
					     packet_sequence_no, packet_fragment_no, more_fragments);
			}
		}

		if ((frame_ctl & IEEE80211_FCTL_FTYPE) == IEEE80211_FTYPE_MGMT) {
			/* copy rest of packet into buffer */
			atmel_copy_to_host(priv->dev, (unsigned char *)&priv->rx_buf, rx_packet_loc + 24, msdu_size);

			/* we use the same buffer for frag reassembly and control packets */
			memset(priv->frag_source, 0xff, 6);

			if (priv->do_rx_crc) {
				/* last 4 octets is crc */
				msdu_size -= 4;
				crc = crc32_le(crc, (unsigned char *)&priv->rx_buf, msdu_size);
				if ((crc ^ 0xffffffff) != (*((u32 *)&priv->rx_buf[msdu_size]))) {
					priv->dev->stats.rx_crc_errors++;
					goto next;
				}
			}

			atmel_management_frame(priv, &header, msdu_size,
					       atmel_rmem8(priv, atmel_rx(priv, RX_DESC_RSSI_OFFSET, priv->rx_desc_head)));
		}

next:
		/* release descriptor */
		atmel_wmem8(priv, atmel_rx(priv, RX_DESC_FLAGS_OFFSET, priv->rx_desc_head), RX_DESC_FLAG_CONSUMED);

		if (priv->rx_desc_head < (priv->host_info.rx_desc_count - 1))
			priv->rx_desc_head++;
		else
			priv->rx_desc_head = 0;
	}
}

static irqreturn_t service_interrupt(int irq, void *dev_id)
{
	struct net_device *dev = (struct net_device *) dev_id;
	struct atmel_private *priv = netdev_priv(dev);
	u8 isr;
	int i = -1;
	static u8 irq_order[] = {
		ISR_OUT_OF_RANGE,
		ISR_RxCOMPLETE,
		ISR_TxCOMPLETE,
		ISR_RxFRAMELOST,
		ISR_FATAL_ERROR,
		ISR_COMMAND_COMPLETE,
		ISR_IBSS_MERGE,
		ISR_GENERIC_IRQ
	};

	if (priv->card && priv->present_callback &&
	    !(*priv->present_callback)(priv->card))
		return IRQ_HANDLED;

	/* In this state upper-level code assumes it can mess with
	   the card unhampered by interrupts which may change register state.
	   Note that even though the card shouldn't generate interrupts
	   the inturrupt line may be shared. This allows card setup
	   to go on without disabling interrupts for a long time. */
	if (priv->station_state == STATION_STATE_DOWN)
		return IRQ_NONE;

	atmel_clear_gcr(dev, GCR_ENINT); /* disable interrupts */

	while (1) {
		if (!atmel_lock_mac(priv)) {
			/* failed to contact card */
			printk(KERN_ALERT "%s: failed to contact MAC.\n", dev->name);
			return IRQ_HANDLED;
		}

		isr = atmel_rmem8(priv, atmel_hi(priv, IFACE_INT_STATUS_OFFSET));
		atmel_wmem8(priv, atmel_hi(priv, IFACE_LOCKOUT_MAC_OFFSET), 0);

		if (!isr) {
			atmel_set_gcr(dev, GCR_ENINT); /* enable interrupts */
			return i == -1 ? IRQ_NONE : IRQ_HANDLED;
		}

		atmel_set_gcr(dev, GCR_ACKINT); /* acknowledge interrupt */

		for (i = 0; i < ARRAY_SIZE(irq_order); i++)
			if (isr & irq_order[i])
				break;

		if (!atmel_lock_mac(priv)) {
			/* failed to contact card */
			printk(KERN_ALERT "%s: failed to contact MAC.\n", dev->name);
			return IRQ_HANDLED;
		}

		isr = atmel_rmem8(priv, atmel_hi(priv, IFACE_INT_STATUS_OFFSET));
		isr ^= irq_order[i];
		atmel_wmem8(priv, atmel_hi(priv, IFACE_INT_STATUS_OFFSET), isr);
		atmel_wmem8(priv, atmel_hi(priv, IFACE_LOCKOUT_MAC_OFFSET), 0);

		switch (irq_order[i]) {

		case ISR_OUT_OF_RANGE:
			if (priv->operating_mode == IW_MODE_INFRA &&
			    priv->station_state == STATION_STATE_READY) {
				priv->station_is_associated = 0;
				atmel_scan(priv, 1);
			}
			break;

		case ISR_RxFRAMELOST:
			priv->wstats.discard.misc++;
			/* fall through */
		case ISR_RxCOMPLETE:
			rx_done_irq(priv);
			break;

		case ISR_TxCOMPLETE:
			tx_done_irq(priv);
			break;

		case ISR_FATAL_ERROR:
			printk(KERN_ALERT "%s: *** FATAL error interrupt ***\n", dev->name);
			atmel_enter_state(priv, STATION_STATE_MGMT_ERROR);
			break;

		case ISR_COMMAND_COMPLETE:
			atmel_command_irq(priv);
			break;

		case ISR_IBSS_MERGE:
			atmel_get_mib(priv, Mac_Mgmt_Mib_Type, MAC_MGMT_MIB_CUR_BSSID_POS,
				      priv->CurrentBSSID, 6);
			/* The WPA stuff cares about the current AP address */
			if (priv->use_wpa)
				build_wpa_mib(priv);
			break;
		case ISR_GENERIC_IRQ:
			printk(KERN_INFO "%s: Generic_irq received.\n", dev->name);
			break;
		}
	}
}

static struct iw_statistics *atmel_get_wireless_stats(struct net_device *dev)
{
	struct atmel_private *priv = netdev_priv(dev);

	/* update the link quality here in case we are seeing no beacons
	   at all to drive the process */
	atmel_smooth_qual(priv);

	priv->wstats.status = priv->station_state;

	if (priv->operating_mode == IW_MODE_INFRA) {
		if (priv->station_state != STATION_STATE_READY) {
			priv->wstats.qual.qual = 0;
			priv->wstats.qual.level = 0;
			priv->wstats.qual.updated = (IW_QUAL_QUAL_INVALID
					| IW_QUAL_LEVEL_INVALID);
		}
		priv->wstats.qual.noise = 0;
		priv->wstats.qual.updated |= IW_QUAL_NOISE_INVALID;
	} else {
		/* Quality levels cannot be determined in ad-hoc mode,
		   because we can 'hear' more that one remote station. */
		priv->wstats.qual.qual = 0;
		priv->wstats.qual.level	= 0;
		priv->wstats.qual.noise	= 0;
		priv->wstats.qual.updated = IW_QUAL_QUAL_INVALID
					| IW_QUAL_LEVEL_INVALID
					| IW_QUAL_NOISE_INVALID;
		priv->wstats.miss.beacon = 0;
	}

	return &priv->wstats;
}

static int atmel_change_mtu(struct net_device *dev, int new_mtu)
{
	if ((new_mtu < 68) || (new_mtu > 2312))
		return -EINVAL;
	dev->mtu = new_mtu;
	return 0;
}

static int atmel_set_mac_address(struct net_device *dev, void *p)
{
	struct sockaddr *addr = p;

	memcpy (dev->dev_addr, addr->sa_data, dev->addr_len);
	return atmel_open(dev);
}

EXPORT_SYMBOL(atmel_open);

int atmel_open(struct net_device *dev)
{
	struct atmel_private *priv = netdev_priv(dev);
	int i, channel, err;

	/* any scheduled timer is no longer needed and might screw things up.. */
	del_timer_sync(&priv->management_timer);

	/* Interrupts will not touch the card once in this state... */
	priv->station_state = STATION_STATE_DOWN;

	if (priv->new_SSID_size) {
		memcpy(priv->SSID, priv->new_SSID, priv->new_SSID_size);
		priv->SSID_size = priv->new_SSID_size;
		priv->new_SSID_size = 0;
	}
	priv->BSS_list_entries = 0;

	priv->AuthenticationRequestRetryCnt = 0;
	priv->AssociationRequestRetryCnt = 0;
	priv->ReAssociationRequestRetryCnt = 0;
	priv->CurrentAuthentTransactionSeqNum = 0x0001;
	priv->ExpectedAuthentTransactionSeqNum = 0x0002;

	priv->site_survey_state = SITE_SURVEY_IDLE;
	priv->station_is_associated = 0;

	err = reset_atmel_card(dev);
	if (err)
		return err;

	if (priv->config_reg_domain) {
		priv->reg_domain = priv->config_reg_domain;
		atmel_set_mib8(priv, Phy_Mib_Type, PHY_MIB_REG_DOMAIN_POS, priv->reg_domain);
	} else {
		priv->reg_domain = atmel_get_mib8(priv, Phy_Mib_Type, PHY_MIB_REG_DOMAIN_POS);
		for (i = 0; i < ARRAY_SIZE(channel_table); i++)
			if (priv->reg_domain == channel_table[i].reg_domain)
				break;
		if (i == ARRAY_SIZE(channel_table)) {
			priv->reg_domain = REG_DOMAIN_MKK1;
			printk(KERN_ALERT "%s: failed to get regulatory domain: assuming MKK1.\n", dev->name);
		}
	}

	if ((channel = atmel_validate_channel(priv, priv->channel)))
		priv->channel = channel;

	/* this moves station_state on.... */
	atmel_scan(priv, 1);

	atmel_set_gcr(priv->dev, GCR_ENINT); /* enable interrupts */
	return 0;
}

static int atmel_close(struct net_device *dev)
{
	struct atmel_private *priv = netdev_priv(dev);

	/* Send event to userspace that we are disassociating */
	if (priv->station_state == STATION_STATE_READY) {
		union iwreq_data wrqu;

		wrqu.data.length = 0;
		wrqu.data.flags = 0;
		wrqu.ap_addr.sa_family = ARPHRD_ETHER;
		memset(wrqu.ap_addr.sa_data, 0, ETH_ALEN);
		wireless_send_event(priv->dev, SIOCGIWAP, &wrqu, NULL);
	}

	atmel_enter_state(priv, STATION_STATE_DOWN);

	if (priv->bus_type == BUS_TYPE_PCCARD)
		atmel_write16(dev, GCR, 0x0060);
	atmel_write16(dev, GCR, 0x0040);
	return 0;
}

static int atmel_validate_channel(struct atmel_private *priv, int channel)
{
	/* check that channel is OK, if so return zero,
	   else return suitable default channel */
	int i;

	for (i = 0; i < ARRAY_SIZE(channel_table); i++)
		if (priv->reg_domain == channel_table[i].reg_domain) {
			if (channel >= channel_table[i].min &&
			    channel <= channel_table[i].max)
				return 0;
			else
				return channel_table[i].min;
		}
	return 0;
}

static int atmel_proc_output (char *buf, struct atmel_private *priv)
{
	int i;
	char *p = buf;
	char *s, *r, *c;

	p += sprintf(p, "Driver version:\t\t%d.%d\n",
		     DRIVER_MAJOR, DRIVER_MINOR);

	if (priv->station_state != STATION_STATE_DOWN) {
		p += sprintf(p, "Firmware version:\t%d.%d build %d\n"
				"Firmware location:\t",
			     priv->host_info.major_version,
			     priv->host_info.minor_version,
			     priv->host_info.build_version);

		if (priv->card_type != CARD_TYPE_EEPROM)
			p += sprintf(p, "on card\n");
		else if (priv->firmware)
			p += sprintf(p, "%s loaded by host\n",
				     priv->firmware_id);
		else
			p += sprintf(p, "%s loaded by hotplug\n",
				     priv->firmware_id);

		switch (priv->card_type) {
		case CARD_TYPE_PARALLEL_FLASH:
			c = "Parallel flash";
			break;
		case CARD_TYPE_SPI_FLASH:
			c = "SPI flash\n";
			break;
		case CARD_TYPE_EEPROM:
			c = "EEPROM";
			break;
		default:
			c = "<unknown>";
		}

		r = "<unknown>";
		for (i = 0; i < ARRAY_SIZE(channel_table); i++)
			if (priv->reg_domain == channel_table[i].reg_domain)
				r = channel_table[i].name;

		p += sprintf(p, "MAC memory type:\t%s\n", c);
		p += sprintf(p, "Regulatory domain:\t%s\n", r);
		p += sprintf(p, "Host CRC checking:\t%s\n",
			     priv->do_rx_crc ? "On" : "Off");
		p += sprintf(p, "WPA-capable firmware:\t%s\n",
			     priv->use_wpa ? "Yes" : "No");
	}

	switch (priv->station_state) {
	case STATION_STATE_SCANNING:
		s = "Scanning";
		break;
	case STATION_STATE_JOINNING:
		s = "Joining";
		break;
	case STATION_STATE_AUTHENTICATING:
		s = "Authenticating";
		break;
	case STATION_STATE_ASSOCIATING:
		s = "Associating";
		break;
	case STATION_STATE_READY:
		s = "Ready";
		break;
	case STATION_STATE_REASSOCIATING:
		s = "Reassociating";
		break;
	case STATION_STATE_MGMT_ERROR:
		s = "Management error";
		break;
	case STATION_STATE_DOWN:
		s = "Down";
		break;
	default:
		s = "<unknown>";
	}

	p += sprintf(p, "Current state:\t\t%s\n", s);
	return p - buf;
}

static int atmel_read_proc(char *page, char **start, off_t off,
			   int count, int *eof, void *data)
{
	struct atmel_private *priv = data;
	int len = atmel_proc_output (page, priv);
	if (len <= off+count)
		*eof = 1;
	*start = page + off;
	len -= off;
	if (len > count)
		len = count;
	if (len < 0)
		len = 0;
	return len;
}

static const struct net_device_ops atmel_netdev_ops = {
	.ndo_open 		= atmel_open,
	.ndo_stop		= atmel_close,
	.ndo_change_mtu 	= atmel_change_mtu,
	.ndo_set_mac_address 	= atmel_set_mac_address,
	.ndo_start_xmit 	= start_tx,
	.ndo_do_ioctl 		= atmel_ioctl,
	.ndo_validate_addr	= eth_validate_addr,
};

struct net_device *init_atmel_card(unsigned short irq, unsigned long port,
				   const AtmelFWType fw_type,
				   struct device *sys_dev,
				   int (*card_present)(void *), void *card)
{
	struct proc_dir_entry *ent;
	struct net_device *dev;
	struct atmel_private *priv;
	int rc;

	/* Create the network device object. */
	dev = alloc_etherdev(sizeof(*priv));
	if (!dev) {
		printk(KERN_ERR "atmel: Couldn't alloc_etherdev\n");
		return NULL;
	}
	if (dev_alloc_name(dev, dev->name) < 0) {
		printk(KERN_ERR "atmel: Couldn't get name!\n");
		goto err_out_free;
	}

	priv = netdev_priv(dev);
	priv->dev = dev;
	priv->sys_dev = sys_dev;
	priv->present_callback = card_present;
	priv->card = card;
	priv->firmware = NULL;
	priv->firmware_id[0] = '\0';
	priv->firmware_type = fw_type;
	if (firmware) /* module parameter */
		strcpy(priv->firmware_id, firmware);
	priv->bus_type = card_present ? BUS_TYPE_PCCARD : BUS_TYPE_PCI;
	priv->station_state = STATION_STATE_DOWN;
	priv->do_rx_crc = 0;
	/* For PCMCIA cards, some chips need CRC, some don't
	   so we have to probe. */
	if (priv->bus_type == BUS_TYPE_PCCARD) {
		priv->probe_crc = 1;
		priv->crc_ok_cnt = priv->crc_ko_cnt = 0;
	} else
		priv->probe_crc = 0;
	priv->last_qual = jiffies;
	priv->last_beacon_timestamp = 0;
	memset(priv->frag_source, 0xff, sizeof(priv->frag_source));
	memset(priv->BSSID, 0, 6);
	priv->CurrentBSSID[0] = 0xFF; /* Initialize to something invalid.... */
	priv->station_was_associated = 0;

	priv->last_survey = jiffies;
	priv->preamble = LONG_PREAMBLE;
	priv->operating_mode = IW_MODE_INFRA;
	priv->connect_to_any_BSS = 0;
	priv->config_reg_domain = 0;
	priv->reg_domain = 0;
	priv->tx_rate = 3;
	priv->auto_tx_rate = 1;
	priv->channel = 4;
	priv->power_mode = 0;
	priv->SSID[0] = '\0';
	priv->SSID_size = 0;
	priv->new_SSID_size = 0;
	priv->frag_threshold = 2346;
	priv->rts_threshold = 2347;
	priv->short_retry = 7;
	priv->long_retry = 4;

	priv->wep_is_on = 0;
	priv->default_key = 0;
	priv->encryption_level = 0;
	priv->exclude_unencrypted = 0;
	priv->group_cipher_suite = priv->pairwise_cipher_suite = CIPHER_SUITE_NONE;
	priv->use_wpa = 0;
	memset(priv->wep_keys, 0, sizeof(priv->wep_keys));
	memset(priv->wep_key_len, 0, sizeof(priv->wep_key_len));

	priv->default_beacon_period = priv->beacon_period = 100;
	priv->listen_interval = 1;

	init_timer(&priv->management_timer);
	spin_lock_init(&priv->irqlock);
	spin_lock_init(&priv->timerlock);
	priv->management_timer.function = atmel_management_timer;
	priv->management_timer.data = (unsigned long) dev;

	dev->netdev_ops = &atmel_netdev_ops;
	dev->wireless_handlers = &atmel_handler_def;
	dev->irq = irq;
	dev->base_addr = port;

	SET_NETDEV_DEV(dev, sys_dev);

	if ((rc = request_irq(dev->irq, service_interrupt, IRQF_SHARED, dev->name, dev))) {
		printk(KERN_ERR "%s: register interrupt %d failed, rc %d\n", dev->name, irq, rc);
		goto err_out_free;
	}

	if (!request_region(dev->base_addr, 32,
			    priv->bus_type == BUS_TYPE_PCCARD ?  "atmel_cs" : "atmel_pci")) {
		goto err_out_irq;
	}

	if (register_netdev(dev))
		goto err_out_res;

	if (!probe_atmel_card(dev)) {
		unregister_netdev(dev);
		goto err_out_res;
	}

	netif_carrier_off(dev);

	ent = create_proc_read_entry ("driver/atmel", 0, NULL, atmel_read_proc, priv);
	if (!ent)
		printk(KERN_WARNING "atmel: unable to create /proc entry.\n");

	printk(KERN_INFO "%s: Atmel at76c50x. Version %d.%d. MAC %pM\n",
	       dev->name, DRIVER_MAJOR, DRIVER_MINOR, dev->dev_addr);

	return dev;

err_out_res:
	release_region(dev->base_addr, 32);
err_out_irq:
	free_irq(dev->irq, dev);
err_out_free:
	free_netdev(dev);
	return NULL;
}

EXPORT_SYMBOL(init_atmel_card);

void stop_atmel_card(struct net_device *dev)
{
	struct atmel_private *priv = netdev_priv(dev);

	/* put a brick on it... */
	if (priv->bus_type == BUS_TYPE_PCCARD)
		atmel_write16(dev, GCR, 0x0060);
	atmel_write16(dev, GCR, 0x0040);

	del_timer_sync(&priv->management_timer);
	unregister_netdev(dev);
	remove_proc_entry("driver/atmel", NULL);
	free_irq(dev->irq, dev);
	kfree(priv->firmware);
	release_region(dev->base_addr, 32);
	free_netdev(dev);
}

EXPORT_SYMBOL(stop_atmel_card);

static int atmel_set_essid(struct net_device *dev,
			   struct iw_request_info *info,
			   struct iw_point *dwrq,
			   char *extra)
{
	struct atmel_private *priv = netdev_priv(dev);

	/* Check if we asked for `any' */
	if (dwrq->flags == 0) {
		priv->connect_to_any_BSS = 1;
	} else {
		int index = (dwrq->flags & IW_ENCODE_INDEX) - 1;

		priv->connect_to_any_BSS = 0;

		/* Check the size of the string */
		if (dwrq->length > MAX_SSID_LENGTH)
			 return -E2BIG;
		if (index != 0)
			return -EINVAL;

		memcpy(priv->new_SSID, extra, dwrq->length);
		priv->new_SSID_size = dwrq->length;
	}

	return -EINPROGRESS;
}

static int atmel_get_essid(struct net_device *dev,
			   struct iw_request_info *info,
			   struct iw_point *dwrq,
			   char *extra)
{
	struct atmel_private *priv = netdev_priv(dev);

	/* Get the current SSID */
	if (priv->new_SSID_size != 0) {
		memcpy(extra, priv->new_SSID, priv->new_SSID_size);
		dwrq->length = priv->new_SSID_size;
	} else {
		memcpy(extra, priv->SSID, priv->SSID_size);
		dwrq->length = priv->SSID_size;
	}

	dwrq->flags = !priv->connect_to_any_BSS; /* active */

	return 0;
}

static int atmel_get_wap(struct net_device *dev,
			 struct iw_request_info *info,
			 struct sockaddr *awrq,
			 char *extra)
{
	struct atmel_private *priv = netdev_priv(dev);
	memcpy(awrq->sa_data, priv->CurrentBSSID, 6);
	awrq->sa_family = ARPHRD_ETHER;

	return 0;
}

static int atmel_set_encode(struct net_device *dev,
			    struct iw_request_info *info,
			    struct iw_point *dwrq,
			    char *extra)
{
	struct atmel_private *priv = netdev_priv(dev);

	/* Basic checking: do we have a key to set ?
	 * Note : with the new API, it's impossible to get a NULL pointer.
	 * Therefore, we need to check a key size == 0 instead.
	 * New version of iwconfig properly set the IW_ENCODE_NOKEY flag
	 * when no key is present (only change flags), but older versions
	 * don't do it. - Jean II */
	if (dwrq->length > 0) {
		int index = (dwrq->flags & IW_ENCODE_INDEX) - 1;
		int current_index = priv->default_key;
		/* Check the size of the key */
		if (dwrq->length > 13) {
			return -EINVAL;
		}
		/* Check the index (none -> use current) */
		if (index < 0 || index >= 4)
			index = current_index;
		else
			priv->default_key = index;
		/* Set the length */
		if (dwrq->length > 5)
			priv->wep_key_len[index] = 13;
		else
			if (dwrq->length > 0)
				priv->wep_key_len[index] = 5;
			else
				/* Disable the key */
				priv->wep_key_len[index] = 0;
		/* Check if the key is not marked as invalid */
		if (!(dwrq->flags & IW_ENCODE_NOKEY)) {
			/* Cleanup */
			memset(priv->wep_keys[index], 0, 13);
			/* Copy the key in the driver */
			memcpy(priv->wep_keys[index], extra, dwrq->length);
		}
		/* WE specify that if a valid key is set, encryption
		 * should be enabled (user may turn it off later)
		 * This is also how "iwconfig ethX key on" works */
		if (index == current_index &&
		    priv->wep_key_len[index] > 0) {
			priv->wep_is_on = 1;
			priv->exclude_unencrypted = 1;
			if (priv->wep_key_len[index] > 5) {
				priv->pairwise_cipher_suite = CIPHER_SUITE_WEP_128;
				priv->encryption_level = 2;
			} else {
				priv->pairwise_cipher_suite = CIPHER_SUITE_WEP_64;
				priv->encryption_level = 1;
			}
		}
	} else {
		/* Do we want to just set the transmit key index ? */
		int index = (dwrq->flags & IW_ENCODE_INDEX) - 1;
		if (index >= 0 && index < 4) {
			priv->default_key = index;
		} else
			/* Don't complain if only change the mode */
			if (!(dwrq->flags & IW_ENCODE_MODE))
				return -EINVAL;
	}
	/* Read the flags */
	if (dwrq->flags & IW_ENCODE_DISABLED) {
		priv->wep_is_on = 0;
		priv->encryption_level = 0;
		priv->pairwise_cipher_suite = CIPHER_SUITE_NONE;
	} else {
		priv->wep_is_on = 1;
		if (priv->wep_key_len[priv->default_key] > 5) {
			priv->pairwise_cipher_suite = CIPHER_SUITE_WEP_128;
			priv->encryption_level = 2;
		} else {
			priv->pairwise_cipher_suite = CIPHER_SUITE_WEP_64;
			priv->encryption_level = 1;
		}
	}
	if (dwrq->flags & IW_ENCODE_RESTRICTED)
		priv->exclude_unencrypted = 1;
	if (dwrq->flags & IW_ENCODE_OPEN)
		priv->exclude_unencrypted = 0;

	return -EINPROGRESS;		/* Call commit handler */
}

static int atmel_get_encode(struct net_device *dev,
			    struct iw_request_info *info,
			    struct iw_point *dwrq,
			    char *extra)
{
	struct atmel_private *priv = netdev_priv(dev);
	int index = (dwrq->flags & IW_ENCODE_INDEX) - 1;

	if (!priv->wep_is_on)
		dwrq->flags = IW_ENCODE_DISABLED;
	else {
		if (priv->exclude_unencrypted)
			dwrq->flags = IW_ENCODE_RESTRICTED;
		else
			dwrq->flags = IW_ENCODE_OPEN;
	}
		/* Which key do we want ? -1 -> tx index */
	if (index < 0 || index >= 4)
		index = priv->default_key;
	dwrq->flags |= index + 1;
	/* Copy the key to the user buffer */
	dwrq->length = priv->wep_key_len[index];
	if (dwrq->length > 16) {
		dwrq->length = 0;
	} else {
		memset(extra, 0, 16);
		memcpy(extra, priv->wep_keys[index], dwrq->length);
	}

	return 0;
}

static int atmel_set_encodeext(struct net_device *dev,
			    struct iw_request_info *info,
			    union iwreq_data *wrqu,
			    char *extra)
{
	struct atmel_private *priv = netdev_priv(dev);
	struct iw_point *encoding = &wrqu->encoding;
	struct iw_encode_ext *ext = (struct iw_encode_ext *)extra;
	int idx, key_len, alg = ext->alg, set_key = 1;

	/* Determine and validate the key index */
	idx = encoding->flags & IW_ENCODE_INDEX;
	if (idx) {
		if (idx < 1 || idx > 4)
			return -EINVAL;
		idx--;
	} else
		idx = priv->default_key;

	if (encoding->flags & IW_ENCODE_DISABLED)
	    alg = IW_ENCODE_ALG_NONE;

	if (ext->ext_flags & IW_ENCODE_EXT_SET_TX_KEY) {
		priv->default_key = idx;
		set_key = ext->key_len > 0 ? 1 : 0;
	}

	if (set_key) {
		/* Set the requested key first */
		switch (alg) {
		case IW_ENCODE_ALG_NONE:
			priv->wep_is_on = 0;
			priv->encryption_level = 0;
			priv->pairwise_cipher_suite = CIPHER_SUITE_NONE;
			break;
		case IW_ENCODE_ALG_WEP:
			if (ext->key_len > 5) {
				priv->wep_key_len[idx] = 13;
				priv->pairwise_cipher_suite = CIPHER_SUITE_WEP_128;
				priv->encryption_level = 2;
			} else if (ext->key_len > 0) {
				priv->wep_key_len[idx] = 5;
				priv->pairwise_cipher_suite = CIPHER_SUITE_WEP_64;
				priv->encryption_level = 1;
			} else {
				return -EINVAL;
			}
			priv->wep_is_on = 1;
			memset(priv->wep_keys[idx], 0, 13);
			key_len = min ((int)ext->key_len, priv->wep_key_len[idx]);
			memcpy(priv->wep_keys[idx], ext->key, key_len);
			break;
		default:
			return -EINVAL;
		}
	}

	return -EINPROGRESS;
}

static int atmel_get_encodeext(struct net_device *dev,
			    struct iw_request_info *info,
			    union iwreq_data *wrqu,
			    char *extra)
{
	struct atmel_private *priv = netdev_priv(dev);
	struct iw_point *encoding = &wrqu->encoding;
	struct iw_encode_ext *ext = (struct iw_encode_ext *)extra;
	int idx, max_key_len;

	max_key_len = encoding->length - sizeof(*ext);
	if (max_key_len < 0)
		return -EINVAL;

	idx = encoding->flags & IW_ENCODE_INDEX;
	if (idx) {
		if (idx < 1 || idx > 4)
			return -EINVAL;
		idx--;
	} else
		idx = priv->default_key;

	encoding->flags = idx + 1;
	memset(ext, 0, sizeof(*ext));

	if (!priv->wep_is_on) {
		ext->alg = IW_ENCODE_ALG_NONE;
		ext->key_len = 0;
		encoding->flags |= IW_ENCODE_DISABLED;
	} else {
		if (priv->encryption_level > 0)
			ext->alg = IW_ENCODE_ALG_WEP;
		else
			return -EINVAL;

		ext->key_len = priv->wep_key_len[idx];
		memcpy(ext->key, priv->wep_keys[idx], ext->key_len);
		encoding->flags |= IW_ENCODE_ENABLED;
	}

	return 0;
}

static int atmel_set_auth(struct net_device *dev,
			       struct iw_request_info *info,
			       union iwreq_data *wrqu, char *extra)
{
	struct atmel_private *priv = netdev_priv(dev);
	struct iw_param *param = &wrqu->param;

	switch (param->flags & IW_AUTH_INDEX) {
	case IW_AUTH_WPA_VERSION:
	case IW_AUTH_CIPHER_PAIRWISE:
	case IW_AUTH_CIPHER_GROUP:
	case IW_AUTH_KEY_MGMT:
	case IW_AUTH_RX_UNENCRYPTED_EAPOL:
	case IW_AUTH_PRIVACY_INVOKED:
		/*
		 * atmel does not use these parameters
		 */
		break;

	case IW_AUTH_DROP_UNENCRYPTED:
		priv->exclude_unencrypted = param->value ? 1 : 0;
		break;

	case IW_AUTH_80211_AUTH_ALG: {
			if (param->value & IW_AUTH_ALG_SHARED_KEY) {
				priv->exclude_unencrypted = 1;
			} else if (param->value & IW_AUTH_ALG_OPEN_SYSTEM) {
				priv->exclude_unencrypted = 0;
			} else
				return -EINVAL;
			break;
		}

	case IW_AUTH_WPA_ENABLED:
		/* Silently accept disable of WPA */
		if (param->value > 0)
			return -EOPNOTSUPP;
		break;

	default:
		return -EOPNOTSUPP;
	}
	return -EINPROGRESS;
}

static int atmel_get_auth(struct net_device *dev,
			       struct iw_request_info *info,
			       union iwreq_data *wrqu, char *extra)
{
	struct atmel_private *priv = netdev_priv(dev);
	struct iw_param *param = &wrqu->param;

	switch (param->flags & IW_AUTH_INDEX) {
	case IW_AUTH_DROP_UNENCRYPTED:
		param->value = priv->exclude_unencrypted;
		break;

	case IW_AUTH_80211_AUTH_ALG:
		if (priv->exclude_unencrypted == 1)
			param->value = IW_AUTH_ALG_SHARED_KEY;
		else
			param->value = IW_AUTH_ALG_OPEN_SYSTEM;
		break;

	case IW_AUTH_WPA_ENABLED:
		param->value = 0;
		break;

	default:
		return -EOPNOTSUPP;
	}
	return 0;
}


static int atmel_get_name(struct net_device *dev,
			  struct iw_request_info *info,
			  char *cwrq,
			  char *extra)
{
	strcpy(cwrq, "IEEE 802.11-DS");
	return 0;
}

static int atmel_set_rate(struct net_device *dev,
			  struct iw_request_info *info,
			  struct iw_param *vwrq,
			  char *extra)
{
	struct atmel_private *priv = netdev_priv(dev);

	if (vwrq->fixed == 0) {
		priv->tx_rate = 3;
		priv->auto_tx_rate = 1;
	} else {
		priv->auto_tx_rate = 0;

		/* Which type of value ? */
		if ((vwrq->value < 4) && (vwrq->value >= 0)) {
			/* Setting by rate index */
			priv->tx_rate = vwrq->value;
		} else {
		/* Setting by frequency value */
			switch (vwrq->value) {
			case  1000000:
				priv->tx_rate = 0;
				break;
			case  2000000:
				priv->tx_rate = 1;
				break;
			case  5500000:
				priv->tx_rate = 2;
				break;
			case 11000000:
				priv->tx_rate = 3;
				break;
			default:
				return -EINVAL;
			}
		}
	}

	return -EINPROGRESS;
}

static int atmel_set_mode(struct net_device *dev,
			  struct iw_request_info *info,
			  __u32 *uwrq,
			  char *extra)
{
	struct atmel_private *priv = netdev_priv(dev);

	if (*uwrq != IW_MODE_ADHOC && *uwrq != IW_MODE_INFRA)
		return -EINVAL;

	priv->operating_mode = *uwrq;
	return -EINPROGRESS;
}

static int atmel_get_mode(struct net_device *dev,
			  struct iw_request_info *info,
			  __u32 *uwrq,
			  char *extra)
{
	struct atmel_private *priv = netdev_priv(dev);

	*uwrq = priv->operating_mode;
	return 0;
}

static int atmel_get_rate(struct net_device *dev,
			 struct iw_request_info *info,
			 struct iw_param *vwrq,
			 char *extra)
{
	struct atmel_private *priv = netdev_priv(dev);

	if (priv->auto_tx_rate) {
		vwrq->fixed = 0;
		vwrq->value = 11000000;
	} else {
		vwrq->fixed = 1;
		switch (priv->tx_rate) {
		case 0:
			vwrq->value =  1000000;
			break;
		case 1:
			vwrq->value =  2000000;
			break;
		case 2:
			vwrq->value =  5500000;
			break;
		case 3:
			vwrq->value = 11000000;
			break;
		}
	}
	return 0;
}

static int atmel_set_power(struct net_device *dev,
			   struct iw_request_info *info,
			   struct iw_param *vwrq,
			   char *extra)
{
	struct atmel_private *priv = netdev_priv(dev);
	priv->power_mode = vwrq->disabled ? 0 : 1;
	return -EINPROGRESS;
}

static int atmel_get_power(struct net_device *dev,
			   struct iw_request_info *info,
			   struct iw_param *vwrq,
			   char *extra)
{
	struct atmel_private *priv = netdev_priv(dev);
	vwrq->disabled = priv->power_mode ? 0 : 1;
	vwrq->flags = IW_POWER_ON;
	return 0;
}

static int atmel_set_retry(struct net_device *dev,
			   struct iw_request_info *info,
			   struct iw_param *vwrq,
			   char *extra)
{
	struct atmel_private *priv = netdev_priv(dev);

	if (!vwrq->disabled && (vwrq->flags & IW_RETRY_LIMIT)) {
		if (vwrq->flags & IW_RETRY_LONG)
			priv->long_retry = vwrq->value;
		else if (vwrq->flags & IW_RETRY_SHORT)
			priv->short_retry = vwrq->value;
		else {
			/* No modifier : set both */
			priv->long_retry = vwrq->value;
			priv->short_retry = vwrq->value;
		}
		return -EINPROGRESS;
	}

	return -EINVAL;
}

static int atmel_get_retry(struct net_device *dev,
			   struct iw_request_info *info,
			   struct iw_param *vwrq,
			   char *extra)
{
	struct atmel_private *priv = netdev_priv(dev);

	vwrq->disabled = 0;      /* Can't be disabled */

	/* Note : by default, display the short retry number */
	if (vwrq->flags & IW_RETRY_LONG) {
		vwrq->flags = IW_RETRY_LIMIT | IW_RETRY_LONG;
		vwrq->value = priv->long_retry;
	} else {
		vwrq->flags = IW_RETRY_LIMIT;
		vwrq->value = priv->short_retry;
		if (priv->long_retry != priv->short_retry)
			vwrq->flags |= IW_RETRY_SHORT;
	}

	return 0;
}

static int atmel_set_rts(struct net_device *dev,
			 struct iw_request_info *info,
			 struct iw_param *vwrq,
			 char *extra)
{
	struct atmel_private *priv = netdev_priv(dev);
	int rthr = vwrq->value;

	if (vwrq->disabled)
		rthr = 2347;
	if ((rthr < 0) || (rthr > 2347)) {
		return -EINVAL;
	}
	priv->rts_threshold = rthr;

	return -EINPROGRESS;		/* Call commit handler */
}

static int atmel_get_rts(struct net_device *dev,
			 struct iw_request_info *info,
			 struct iw_param *vwrq,
			 char *extra)
{
	struct atmel_private *priv = netdev_priv(dev);

	vwrq->value = priv->rts_threshold;
	vwrq->disabled = (vwrq->value >= 2347);
	vwrq->fixed = 1;

	return 0;
}

static int atmel_set_frag(struct net_device *dev,
			  struct iw_request_info *info,
			  struct iw_param *vwrq,
			  char *extra)
{
	struct atmel_private *priv = netdev_priv(dev);
	int fthr = vwrq->value;

	if (vwrq->disabled)
		fthr = 2346;
	if ((fthr < 256) || (fthr > 2346)) {
		return -EINVAL;
	}
	fthr &= ~0x1;	/* Get an even value - is it really needed ??? */
	priv->frag_threshold = fthr;

	return -EINPROGRESS;		/* Call commit handler */
}

static int atmel_get_frag(struct net_device *dev,
			  struct iw_request_info *info,
			  struct iw_param *vwrq,
			  char *extra)
{
	struct atmel_private *priv = netdev_priv(dev);

	vwrq->value = priv->frag_threshold;
	vwrq->disabled = (vwrq->value >= 2346);
	vwrq->fixed = 1;

	return 0;
}

static int atmel_set_freq(struct net_device *dev,
			  struct iw_request_info *info,
			  struct iw_freq *fwrq,
			  char *extra)
{
	struct atmel_private *priv = netdev_priv(dev);
	int rc = -EINPROGRESS;		/* Call commit handler */

	/* If setting by frequency, convert to a channel */
	if (fwrq->e == 1) {
		int f = fwrq->m / 100000;

		/* Hack to fall through... */
		fwrq->e = 0;
		fwrq->m = ieee80211_freq_to_dsss_chan(f);
	}
	/* Setting by channel number */
	if ((fwrq->m > 1000) || (fwrq->e > 0))
		rc = -EOPNOTSUPP;
	else {
		int channel = fwrq->m;
		if (atmel_validate_channel(priv, channel) == 0) {
			priv->channel = channel;
		} else {
			rc = -EINVAL;
		}
	}
	return rc;
}

static int atmel_get_freq(struct net_device *dev,
			  struct iw_request_info *info,
			  struct iw_freq *fwrq,
			  char *extra)
{
	struct atmel_private *priv = netdev_priv(dev);

	fwrq->m = priv->channel;
	fwrq->e = 0;
	return 0;
}

static int atmel_set_scan(struct net_device *dev,
			  struct iw_request_info *info,
			  struct iw_point *dwrq,
			  char *extra)
{
	struct atmel_private *priv = netdev_priv(dev);
	unsigned long flags;

	/* Note : you may have realised that, as this is a SET operation,
	 * this is privileged and therefore a normal user can't
	 * perform scanning.
	 * This is not an error, while the device perform scanning,
	 * traffic doesn't flow, so it's a perfect DoS...
	 * Jean II */

	if (priv->station_state == STATION_STATE_DOWN)
		return -EAGAIN;

	/* Timeout old surveys. */
	if (time_after(jiffies, priv->last_survey + 20 * HZ))
		priv->site_survey_state = SITE_SURVEY_IDLE;
	priv->last_survey = jiffies;

	/* Initiate a scan command */
	if (priv->site_survey_state == SITE_SURVEY_IN_PROGRESS)
		return -EBUSY;

	del_timer_sync(&priv->management_timer);
	spin_lock_irqsave(&priv->irqlock, flags);

	priv->site_survey_state = SITE_SURVEY_IN_PROGRESS;
	priv->fast_scan = 0;
	atmel_scan(priv, 0);
	spin_unlock_irqrestore(&priv->irqlock, flags);

	return 0;
}

static int atmel_get_scan(struct net_device *dev,
			  struct iw_request_info *info,
			  struct iw_point *dwrq,
			  char *extra)
{
	struct atmel_private *priv = netdev_priv(dev);
	int i;
	char *current_ev = extra;
	struct iw_event	iwe;

	if (priv->site_survey_state != SITE_SURVEY_COMPLETED)
		return -EAGAIN;

	for (i = 0; i < priv->BSS_list_entries; i++) {
		iwe.cmd = SIOCGIWAP;
		iwe.u.ap_addr.sa_family = ARPHRD_ETHER;
		memcpy(iwe.u.ap_addr.sa_data, priv->BSSinfo[i].BSSID, 6);
		current_ev = iwe_stream_add_event(info, current_ev,
						  extra + IW_SCAN_MAX_DATA,
						  &iwe, IW_EV_ADDR_LEN);

		iwe.u.data.length =  priv->BSSinfo[i].SSIDsize;
		if (iwe.u.data.length > 32)
			iwe.u.data.length = 32;
		iwe.cmd = SIOCGIWESSID;
		iwe.u.data.flags = 1;
		current_ev = iwe_stream_add_point(info, current_ev,
						  extra + IW_SCAN_MAX_DATA,
						  &iwe, priv->BSSinfo[i].SSID);

		iwe.cmd = SIOCGIWMODE;
		iwe.u.mode = priv->BSSinfo[i].BSStype;
		current_ev = iwe_stream_add_event(info, current_ev,
						  extra + IW_SCAN_MAX_DATA,
						  &iwe, IW_EV_UINT_LEN);

		iwe.cmd = SIOCGIWFREQ;
		iwe.u.freq.m = priv->BSSinfo[i].channel;
		iwe.u.freq.e = 0;
		current_ev = iwe_stream_add_event(info, current_ev,
						  extra + IW_SCAN_MAX_DATA,
						  &iwe, IW_EV_FREQ_LEN);

		/* Add quality statistics */
		iwe.cmd = IWEVQUAL;
		iwe.u.qual.level = priv->BSSinfo[i].RSSI;
		iwe.u.qual.qual  = iwe.u.qual.level;
		/* iwe.u.qual.noise  = SOMETHING */
		current_ev = iwe_stream_add_event(info, current_ev,
						  extra + IW_SCAN_MAX_DATA,
						  &iwe, IW_EV_QUAL_LEN);


		iwe.cmd = SIOCGIWENCODE;
		if (priv->BSSinfo[i].UsingWEP)
			iwe.u.data.flags = IW_ENCODE_ENABLED | IW_ENCODE_NOKEY;
		else
			iwe.u.data.flags = IW_ENCODE_DISABLED;
		iwe.u.data.length = 0;
		current_ev = iwe_stream_add_point(info, current_ev,
						  extra + IW_SCAN_MAX_DATA,
						  &iwe, NULL);
	}

	/* Length of data */
	dwrq->length = (current_ev - extra);
	dwrq->flags = 0;

	return 0;
}

static int atmel_get_range(struct net_device *dev,
			   struct iw_request_info *info,
			   struct iw_point *dwrq,
			   char *extra)
{
	struct atmel_private *priv = netdev_priv(dev);
	struct iw_range *range = (struct iw_range *) extra;
	int k, i, j;

	dwrq->length = sizeof(struct iw_range);
	memset(range, 0, sizeof(struct iw_range));
	range->min_nwid = 0x0000;
	range->max_nwid = 0x0000;
	range->num_channels = 0;
	for (j = 0; j < ARRAY_SIZE(channel_table); j++)
		if (priv->reg_domain == channel_table[j].reg_domain) {
			range->num_channels = channel_table[j].max - channel_table[j].min + 1;
			break;
		}
	if (range->num_channels != 0) {
		for (k = 0, i = channel_table[j].min; i <= channel_table[j].max; i++) {
			range->freq[k].i = i; /* List index */

			/* Values in MHz -> * 10^5 * 10 */
			range->freq[k].m = (ieee80211_dsss_chan_to_freq(i) *
					    100000);
			range->freq[k++].e = 1;
		}
		range->num_frequency = k;
	}

	range->max_qual.qual = 100;
	range->max_qual.level = 100;
	range->max_qual.noise = 0;
	range->max_qual.updated = IW_QUAL_NOISE_INVALID;

	range->avg_qual.qual = 50;
	range->avg_qual.level = 50;
	range->avg_qual.noise = 0;
	range->avg_qual.updated = IW_QUAL_NOISE_INVALID;

	range->sensitivity = 0;

	range->bitrate[0] =  1000000;
	range->bitrate[1] =  2000000;
	range->bitrate[2] =  5500000;
	range->bitrate[3] = 11000000;
	range->num_bitrates = 4;

	range->min_rts = 0;
	range->max_rts = 2347;
	range->min_frag = 256;
	range->max_frag = 2346;

	range->encoding_size[0] = 5;
	range->encoding_size[1] = 13;
	range->num_encoding_sizes = 2;
	range->max_encoding_tokens = 4;

	range->pmp_flags = IW_POWER_ON;
	range->pmt_flags = IW_POWER_ON;
	range->pm_capa = 0;

	range->we_version_source = WIRELESS_EXT;
	range->we_version_compiled = WIRELESS_EXT;
	range->retry_capa = IW_RETRY_LIMIT ;
	range->retry_flags = IW_RETRY_LIMIT;
	range->r_time_flags = 0;
	range->min_retry = 1;
	range->max_retry = 65535;

	return 0;
}

static int atmel_set_wap(struct net_device *dev,
			 struct iw_request_info *info,
			 struct sockaddr *awrq,
			 char *extra)
{
	struct atmel_private *priv = netdev_priv(dev);
	int i;
	static const u8 any[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
	static const u8 off[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	unsigned long flags;

	if (awrq->sa_family != ARPHRD_ETHER)
		return -EINVAL;

	if (!memcmp(any, awrq->sa_data, 6) ||
	    !memcmp(off, awrq->sa_data, 6)) {
		del_timer_sync(&priv->management_timer);
		spin_lock_irqsave(&priv->irqlock, flags);
		atmel_scan(priv, 1);
		spin_unlock_irqrestore(&priv->irqlock, flags);
		return 0;
	}

	for (i = 0; i < priv->BSS_list_entries; i++) {
		if (memcmp(priv->BSSinfo[i].BSSID, awrq->sa_data, 6) == 0) {
			if (!priv->wep_is_on && priv->BSSinfo[i].UsingWEP) {
				return -EINVAL;
			} else if  (priv->wep_is_on && !priv->BSSinfo[i].UsingWEP) {
				return -EINVAL;
			} else {
				del_timer_sync(&priv->management_timer);
				spin_lock_irqsave(&priv->irqlock, flags);
				atmel_join_bss(priv, i);
				spin_unlock_irqrestore(&priv->irqlock, flags);
				return 0;
			}
		}
	}

	return -EINVAL;
}

static int atmel_config_commit(struct net_device *dev,
			       struct iw_request_info *info,	/* NULL */
			       void *zwrq,			/* NULL */
			       char *extra)			/* NULL */
{
	return atmel_open(dev);
}

static const iw_handler atmel_handler[] =
{
	(iw_handler) atmel_config_commit,	/* SIOCSIWCOMMIT */
	(iw_handler) atmel_get_name,		/* SIOCGIWNAME */
	(iw_handler) NULL,			/* SIOCSIWNWID */
	(iw_handler) NULL,			/* SIOCGIWNWID */
	(iw_handler) atmel_set_freq,		/* SIOCSIWFREQ */
	(iw_handler) atmel_get_freq,		/* SIOCGIWFREQ */
	(iw_handler) atmel_set_mode,		/* SIOCSIWMODE */
	(iw_handler) atmel_get_mode,		/* SIOCGIWMODE */
	(iw_handler) NULL,			/* SIOCSIWSENS */
	(iw_handler) NULL,			/* SIOCGIWSENS */
	(iw_handler) NULL,			/* SIOCSIWRANGE */
	(iw_handler) atmel_get_range,           /* SIOCGIWRANGE */
	(iw_handler) NULL,			/* SIOCSIWPRIV */
	(iw_handler) NULL,			/* SIOCGIWPRIV */
	(iw_handler) NULL,			/* SIOCSIWSTATS */
	(iw_handler) NULL,			/* SIOCGIWSTATS */
	(iw_handler) NULL,			/* SIOCSIWSPY */
	(iw_handler) NULL,			/* SIOCGIWSPY */
	(iw_handler) NULL,			/* -- hole -- */
	(iw_handler) NULL,			/* -- hole -- */
	(iw_handler) atmel_set_wap,		/* SIOCSIWAP */
	(iw_handler) atmel_get_wap,		/* SIOCGIWAP */
	(iw_handler) NULL,			/* -- hole -- */
	(iw_handler) NULL,			/* SIOCGIWAPLIST */
	(iw_handler) atmel_set_scan,		/* SIOCSIWSCAN */
	(iw_handler) atmel_get_scan,		/* SIOCGIWSCAN */
	(iw_handler) atmel_set_essid,		/* SIOCSIWESSID */
	(iw_handler) atmel_get_essid,		/* SIOCGIWESSID */
	(iw_handler) NULL,			/* SIOCSIWNICKN */
	(iw_handler) NULL,			/* SIOCGIWNICKN */
	(iw_handler) NULL,			/* -- hole -- */
	(iw_handler) NULL,			/* -- hole -- */
	(iw_handler) atmel_set_rate,		/* SIOCSIWRATE */
	(iw_handler) atmel_get_rate,		/* SIOCGIWRATE */
	(iw_handler) atmel_set_rts,		/* SIOCSIWRTS */
	(iw_handler) atmel_get_rts,		/* SIOCGIWRTS */
	(iw_handler) atmel_set_frag,		/* SIOCSIWFRAG */
	(iw_handler) atmel_get_frag,		/* SIOCGIWFRAG */
	(iw_handler) NULL,			/* SIOCSIWTXPOW */
	(iw_handler) NULL,			/* SIOCGIWTXPOW */
	(iw_handler) atmel_set_retry,		/* SIOCSIWRETRY */
	(iw_handler) atmel_get_retry,		/* SIOCGIWRETRY */
	(iw_handler) atmel_set_encode,		/* SIOCSIWENCODE */
	(iw_handler) atmel_get_encode,		/* SIOCGIWENCODE */
	(iw_handler) atmel_set_power,		/* SIOCSIWPOWER */
	(iw_handler) atmel_get_power,		/* SIOCGIWPOWER */
	(iw_handler) NULL,			/* -- hole -- */
	(iw_handler) NULL,			/* -- hole -- */
	(iw_handler) NULL,			/* SIOCSIWGENIE */
	(iw_handler) NULL,			/* SIOCGIWGENIE */
	(iw_handler) atmel_set_auth,		/* SIOCSIWAUTH */
	(iw_handler) atmel_get_auth,		/* SIOCGIWAUTH */
	(iw_handler) atmel_set_encodeext,	/* SIOCSIWENCODEEXT */
	(iw_handler) atmel_get_encodeext,	/* SIOCGIWENCODEEXT */
	(iw_handler) NULL,			/* SIOCSIWPMKSA */
};

static const iw_handler atmel_private_handler[] =
{
	NULL,				/* SIOCIWFIRSTPRIV */
};

typedef struct atmel_priv_ioctl {
	char id[32];
	unsigned char __user *data;
	unsigned short len;
} atmel_priv_ioctl;

#define ATMELFWL	SIOCIWFIRSTPRIV
#define ATMELIDIFC	ATMELFWL + 1
#define ATMELRD		ATMELFWL + 2
#define ATMELMAGIC 0x51807
#define REGDOMAINSZ 20

static const struct iw_priv_args atmel_private_args[] = {
	{
		.cmd = ATMELFWL,
		.set_args = IW_PRIV_TYPE_BYTE
				| IW_PRIV_SIZE_FIXED
				| sizeof (atmel_priv_ioctl),
		.get_args = IW_PRIV_TYPE_NONE,
		.name = "atmelfwl"
	}, {
		.cmd = ATMELIDIFC,
		.set_args = IW_PRIV_TYPE_NONE,
		.get_args = IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		.name = "atmelidifc"
	}, {
		.cmd = ATMELRD,
		.set_args = IW_PRIV_TYPE_CHAR | REGDOMAINSZ,
		.get_args = IW_PRIV_TYPE_NONE,
		.name = "regdomain"
	},
};

static const struct iw_handler_def atmel_handler_def = {
	.num_standard	= ARRAY_SIZE(atmel_handler),
	.num_private	= ARRAY_SIZE(atmel_private_handler),
	.num_private_args = ARRAY_SIZE(atmel_private_args),
	.standard	= (iw_handler *) atmel_handler,
	.private	= (iw_handler *) atmel_private_handler,
	.private_args	= (struct iw_priv_args *) atmel_private_args,
	.get_wireless_stats = atmel_get_wireless_stats
};

static int atmel_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	int i, rc = 0;
	struct atmel_private *priv = netdev_priv(dev);
	atmel_priv_ioctl com;
	struct iwreq *wrq = (struct iwreq *) rq;
	unsigned char *new_firmware;
	char domain[REGDOMAINSZ + 1];

	switch (cmd) {
	case ATMELIDIFC:
		wrq->u.param.value = ATMELMAGIC;
		break;

	case ATMELFWL:
		if (copy_from_user(&com, rq->ifr_data, sizeof(com))) {
			rc = -EFAULT;
			break;
		}

		if (!capable(CAP_NET_ADMIN)) {
			rc = -EPERM;
			break;
		}

		if (!(new_firmware = kmalloc(com.len, GFP_KERNEL))) {
			rc = -ENOMEM;
			break;
		}

		if (copy_from_user(new_firmware, com.data, com.len)) {
			kfree(new_firmware);
			rc = -EFAULT;
			break;
		}

		kfree(priv->firmware);

		priv->firmware = new_firmware;
		priv->firmware_length = com.len;
		strncpy(priv->firmware_id, com.id, 31);
		priv->firmware_id[31] = '\0';
		break;

	case ATMELRD:
		if (copy_from_user(domain, rq->ifr_data, REGDOMAINSZ)) {
			rc = -EFAULT;
			break;
		}

		if (!capable(CAP_NET_ADMIN)) {
			rc = -EPERM;
			break;
		}

		domain[REGDOMAINSZ] = 0;
		rc = -EINVAL;
		for (i = 0; i < ARRAY_SIZE(channel_table); i++) {
			/* strcasecmp doesn't exist in the library */
			char *a = channel_table[i].name;
			char *b = domain;
			while (*a) {
				char c1 = *a++;
				char c2 = *b++;
				if (tolower(c1) != tolower(c2))
					break;
			}
			if (!*a && !*b) {
				priv->config_reg_domain = channel_table[i].reg_domain;
				rc = 0;
			}
		}

		if (rc == 0 &&  priv->station_state != STATION_STATE_DOWN)
			rc = atmel_open(dev);
		break;

	default:
		rc = -EOPNOTSUPP;
	}

	return rc;
}

struct auth_body {
	__le16 alg;
	__le16 trans_seq;
	__le16 status;
	u8 el_id;
	u8 chall_text_len;
	u8 chall_text[253];
};

static void atmel_enter_state(struct atmel_private *priv, int new_state)
{
	int old_state = priv->station_state;

	if (new_state == old_state)
		return;

	priv->station_state = new_state;

	if (new_state == STATION_STATE_READY) {
		netif_start_queue(priv->dev);
		netif_carrier_on(priv->dev);
	}

	if (old_state == STATION_STATE_READY) {
		netif_carrier_off(priv->dev);
		if (netif_running(priv->dev))
			netif_stop_queue(priv->dev);
		priv->last_beacon_timestamp = 0;
	}
}

static void atmel_scan(struct atmel_private *priv, int specific_ssid)
{
	struct {
		u8 BSSID[6];
		u8 SSID[MAX_SSID_LENGTH];
		u8 scan_type;
		u8 channel;
		__le16 BSS_type;
		__le16 min_channel_time;
		__le16 max_channel_time;
		u8 options;
		u8 SSID_size;
	} cmd;

	memset(cmd.BSSID, 0xff, 6);

	if (priv->fast_scan) {
		cmd.SSID_size = priv->SSID_size;
		memcpy(cmd.SSID, priv->SSID, priv->SSID_size);
		cmd.min_channel_time = cpu_to_le16(10);
		cmd.max_channel_time = cpu_to_le16(50);
	} else {
		priv->BSS_list_entries = 0;
		cmd.SSID_size = 0;
		cmd.min_channel_time = cpu_to_le16(10);
		cmd.max_channel_time = cpu_to_le16(120);
	}

	cmd.options = 0;

	if (!specific_ssid)
		cmd.options |= SCAN_OPTIONS_SITE_SURVEY;

	cmd.channel = (priv->channel & 0x7f);
	cmd.scan_type = SCAN_TYPE_ACTIVE;
	cmd.BSS_type = cpu_to_le16(priv->operating_mode == IW_MODE_ADHOC ?
		BSS_TYPE_AD_HOC : BSS_TYPE_INFRASTRUCTURE);

	atmel_send_command(priv, CMD_Scan, &cmd, sizeof(cmd));

	/* This must come after all hardware access to avoid being messed up
	   by stuff happening in interrupt context after we leave STATE_DOWN */
	atmel_enter_state(priv, STATION_STATE_SCANNING);
}

static void join(struct atmel_private *priv, int type)
{
	struct {
		u8 BSSID[6];
		u8 SSID[MAX_SSID_LENGTH];
		u8 BSS_type; /* this is a short in a scan command - weird */
		u8 channel;
		__le16 timeout;
		u8 SSID_size;
		u8 reserved;
	} cmd;

	cmd.SSID_size = priv->SSID_size;
	memcpy(cmd.SSID, priv->SSID, priv->SSID_size);
	memcpy(cmd.BSSID, priv->CurrentBSSID, 6);
	cmd.channel = (priv->channel & 0x7f);
	cmd.BSS_type = type;
	cmd.timeout = cpu_to_le16(2000);

	atmel_send_command(priv, CMD_Join, &cmd, sizeof(cmd));
}

static void start(struct atmel_private *priv, int type)
{
	struct {
		u8 BSSID[6];
		u8 SSID[MAX_SSID_LENGTH];
		u8 BSS_type;
		u8 channel;
		u8 SSID_size;
		u8 reserved[3];
	} cmd;

	cmd.SSID_size = priv->SSID_size;
	memcpy(cmd.SSID, priv->SSID, priv->SSID_size);
	memcpy(cmd.BSSID, priv->BSSID, 6);
	cmd.BSS_type = type;
	cmd.channel = (priv->channel & 0x7f);

	atmel_send_command(priv, CMD_Start, &cmd, sizeof(cmd));
}

static void handle_beacon_probe(struct atmel_private *priv, u16 capability,
				u8 channel)
{
	int rejoin = 0;
	int new = capability & WLAN_CAPABILITY_SHORT_PREAMBLE ?
		SHORT_PREAMBLE : LONG_PREAMBLE;

	if (priv->preamble != new) {
		priv->preamble = new;
		rejoin = 1;
		atmel_set_mib8(priv, Local_Mib_Type, LOCAL_MIB_PREAMBLE_TYPE, new);
	}

	if (priv->channel != channel) {
		priv->channel = channel;
		rejoin = 1;
		atmel_set_mib8(priv, Phy_Mib_Type, PHY_MIB_CHANNEL_POS, channel);
	}

	if (rejoin) {
		priv->station_is_associated = 0;
		atmel_enter_state(priv, STATION_STATE_JOINNING);

		if (priv->operating_mode == IW_MODE_INFRA)
			join(priv, BSS_TYPE_INFRASTRUCTURE);
		else
			join(priv, BSS_TYPE_AD_HOC);
	}
}

static void send_authentication_request(struct atmel_private *priv, u16 system,
					u8 *challenge, int challenge_len)
{
	struct ieee80211_hdr header;
	struct auth_body auth;

	header.frame_control = cpu_to_le16(IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_AUTH);
	header.duration_id = cpu_to_le16(0x8000);
	header.seq_ctrl = 0;
	memcpy(header.addr1, priv->CurrentBSSID, 6);
	memcpy(header.addr2, priv->dev->dev_addr, 6);
	memcpy(header.addr3, priv->CurrentBSSID, 6);

	if (priv->wep_is_on && priv->CurrentAuthentTransactionSeqNum != 1)
		/* no WEP for authentication frames with TrSeqNo 1 */
		header.frame_control |=  cpu_to_le16(IEEE80211_FCTL_PROTECTED);

	auth.alg = cpu_to_le16(system);

	auth.status = 0;
	auth.trans_seq = cpu_to_le16(priv->CurrentAuthentTransactionSeqNum);
	priv->ExpectedAuthentTransactionSeqNum = priv->CurrentAuthentTransactionSeqNum+1;
	priv->CurrentAuthentTransactionSeqNum += 2;

	if (challenge_len != 0)	{
		auth.el_id = 16; /* challenge_text */
		auth.chall_text_len = challenge_len;
		memcpy(auth.chall_text, challenge, challenge_len);
		atmel_transmit_management_frame(priv, &header, (u8 *)&auth, 8 + challenge_len);
	} else {
		atmel_transmit_management_frame(priv, &header, (u8 *)&auth, 6);
	}
}

static void send_association_request(struct atmel_private *priv, int is_reassoc)
{
	u8 *ssid_el_p;
	int bodysize;
	struct ieee80211_hdr header;
	struct ass_req_format {
		__le16 capability;
		__le16 listen_interval;
		u8 ap[6]; /* nothing after here directly accessible */
		u8 ssid_el_id;
		u8 ssid_len;
		u8 ssid[MAX_SSID_LENGTH];
		u8 sup_rates_el_id;
		u8 sup_rates_len;
		u8 rates[4];
	} body;

	header.frame_control = cpu_to_le16(IEEE80211_FTYPE_MGMT |
		(is_reassoc ? IEEE80211_STYPE_REASSOC_REQ : IEEE80211_STYPE_ASSOC_REQ));
	header.duration_id = cpu_to_le16(0x8000);
	header.seq_ctrl = 0;

	memcpy(header.addr1, priv->CurrentBSSID, 6);
	memcpy(header.addr2, priv->dev->dev_addr, 6);
	memcpy(header.addr3, priv->CurrentBSSID, 6);

	body.capability = cpu_to_le16(WLAN_CAPABILITY_ESS);
	if (priv->wep_is_on)
		body.capability |= cpu_to_le16(WLAN_CAPABILITY_PRIVACY);
	if (priv->preamble == SHORT_PREAMBLE)
		body.capability |= cpu_to_le16(WLAN_CAPABILITY_SHORT_PREAMBLE);

	body.listen_interval = cpu_to_le16(priv->listen_interval * priv->beacon_period);

	/* current AP address - only in reassoc frame */
	if (is_reassoc) {
		memcpy(body.ap, priv->CurrentBSSID, 6);
		ssid_el_p = (u8 *)&body.ssid_el_id;
		bodysize = 18 + priv->SSID_size;
	} else {
		ssid_el_p = (u8 *)&body.ap[0];
		bodysize = 12 + priv->SSID_size;
	}

	ssid_el_p[0] = WLAN_EID_SSID;
	ssid_el_p[1] = priv->SSID_size;
	memcpy(ssid_el_p + 2, priv->SSID, priv->SSID_size);
	ssid_el_p[2 + priv->SSID_size] = WLAN_EID_SUPP_RATES;
	ssid_el_p[3 + priv->SSID_size] = 4; /* len of suported rates */
	memcpy(ssid_el_p + 4 + priv->SSID_size, atmel_basic_rates, 4);

	atmel_transmit_management_frame(priv, &header, (void *)&body, bodysize);
}

static int is_frame_from_current_bss(struct atmel_private *priv,
				     struct ieee80211_hdr *header)
{
	if (le16_to_cpu(header->frame_control) & IEEE80211_FCTL_FROMDS)
		return memcmp(header->addr3, priv->CurrentBSSID, 6) == 0;
	else
		return memcmp(header->addr2, priv->CurrentBSSID, 6) == 0;
}

static int retrieve_bss(struct atmel_private *priv)
{
	int i;
	int max_rssi = -128;
	int max_index = -1;

	if (priv->BSS_list_entries == 0)
		return -1;

	if (priv->connect_to_any_BSS) {
		/* Select a BSS with the max-RSSI but of the same type and of
		   the same WEP mode and that it is not marked as 'bad' (i.e.
		   we had previously failed to connect to this BSS with the
		   settings that we currently use) */
		priv->current_BSS = 0;
		for (i = 0; i < priv->BSS_list_entries; i++) {
			if (priv->operating_mode == priv->BSSinfo[i].BSStype &&
			    ((!priv->wep_is_on && !priv->BSSinfo[i].UsingWEP) ||
			     (priv->wep_is_on && priv->BSSinfo[i].UsingWEP)) &&
			    !(priv->BSSinfo[i].channel & 0x80)) {
				max_rssi = priv->BSSinfo[i].RSSI;
				priv->current_BSS = max_index = i;
			}
		}
		return max_index;
	}

	for (i = 0; i < priv->BSS_list_entries; i++) {
		if (priv->SSID_size == priv->BSSinfo[i].SSIDsize &&
		    memcmp(priv->SSID, priv->BSSinfo[i].SSID, priv->SSID_size) == 0 &&
		    priv->operating_mode == priv->BSSinfo[i].BSStype &&
		    atmel_validate_channel(priv, priv->BSSinfo[i].channel) == 0) {
			if (priv->BSSinfo[i].RSSI >= max_rssi) {
				max_rssi = priv->BSSinfo[i].RSSI;
				max_index = i;
			}
		}
	}
	return max_index;
}

static void store_bss_info(struct atmel_private *priv,
			   struct ieee80211_hdr *header, u16 capability,
			   u16 beacon_period, u8 channel, u8 rssi, u8 ssid_len,
			   u8 *ssid, int is_beacon)
{
	u8 *bss = capability & WLAN_CAPABILITY_ESS ? header->addr2 : header->addr3;
	int i, index;

	for (index = -1, i = 0; i < priv->BSS_list_entries; i++)
		if (memcmp(bss, priv->BSSinfo[i].BSSID, 6) == 0)
			index = i;

	/* If we process a probe and an entry from this BSS exists
	   we will update the BSS entry with the info from this BSS.
	   If we process a beacon we will only update RSSI */

	if (index == -1) {
		if (priv->BSS_list_entries == MAX_BSS_ENTRIES)
			return;
		index = priv->BSS_list_entries++;
		memcpy(priv->BSSinfo[index].BSSID, bss, 6);
		priv->BSSinfo[index].RSSI = rssi;
	} else {
		if (rssi > priv->BSSinfo[index].RSSI)
			priv->BSSinfo[index].RSSI = rssi;
		if (is_beacon)
			return;
	}

	priv->BSSinfo[index].channel = channel;
	priv->BSSinfo[index].beacon_period = beacon_period;
	priv->BSSinfo[index].UsingWEP = capability & WLAN_CAPABILITY_PRIVACY;
	memcpy(priv->BSSinfo[index].SSID, ssid, ssid_len);
	priv->BSSinfo[index].SSIDsize = ssid_len;

	if (capability & WLAN_CAPABILITY_IBSS)
		priv->BSSinfo[index].BSStype = IW_MODE_ADHOC;
	else if (capability & WLAN_CAPABILITY_ESS)
		priv->BSSinfo[index].BSStype = IW_MODE_INFRA;

	priv->BSSinfo[index].preamble = capability & WLAN_CAPABILITY_SHORT_PREAMBLE ?
		SHORT_PREAMBLE : LONG_PREAMBLE;
}

static void authenticate(struct atmel_private *priv, u16 frame_len)
{
	struct auth_body *auth = (struct auth_body *)priv->rx_buf;
	u16 status = le16_to_cpu(auth->status);
	u16 trans_seq_no = le16_to_cpu(auth->trans_seq);
	u16 system = le16_to_cpu(auth->alg);

	if (status == WLAN_STATUS_SUCCESS && !priv->wep_is_on) {
		/* no WEP */
		if (priv->station_was_associated) {
			atmel_enter_state(priv, STATION_STATE_REASSOCIATING);
			send_association_request(priv, 1);
			return;
		} else {
			atmel_enter_state(priv, STATION_STATE_ASSOCIATING);
			send_association_request(priv, 0);
			return;
		}
	}

	if (status == WLAN_STATUS_SUCCESS && priv->wep_is_on) {
		int should_associate = 0;
		/* WEP */
		if (trans_seq_no != priv->ExpectedAuthentTransactionSeqNum)
			return;

		if (system == WLAN_AUTH_OPEN) {
			if (trans_seq_no == 0x0002) {
				should_associate = 1;
			}
		} else if (system == WLAN_AUTH_SHARED_KEY) {
			if (trans_seq_no == 0x0002 &&
			    auth->el_id == WLAN_EID_CHALLENGE) {
				send_authentication_request(priv, system, auth->chall_text, auth->chall_text_len);
				return;
			} else if (trans_seq_no == 0x0004) {
				should_associate = 1;
			}
		}

		if (should_associate) {
			if (priv->station_was_associated) {
				atmel_enter_state(priv, STATION_STATE_REASSOCIATING);
				send_association_request(priv, 1);
				return;
			} else {
				atmel_enter_state(priv, STATION_STATE_ASSOCIATING);
				send_association_request(priv, 0);
				return;
			}
		}
	}

	if (status == WLAN_STATUS_NOT_SUPPORTED_AUTH_ALG) {
		/* Flip back and forth between WEP auth modes until the max
		 * authentication tries has been exceeded.
		 */
		if (system == WLAN_AUTH_OPEN) {
			priv->CurrentAuthentTransactionSeqNum = 0x001;
			priv->exclude_unencrypted = 1;
			send_authentication_request(priv, WLAN_AUTH_SHARED_KEY, NULL, 0);
			return;
		} else if (system == WLAN_AUTH_SHARED_KEY
			   && priv->wep_is_on) {
			priv->CurrentAuthentTransactionSeqNum = 0x001;
			priv->exclude_unencrypted = 0;
			send_authentication_request(priv, WLAN_AUTH_OPEN, NULL, 0);
			return;
		} else if (priv->connect_to_any_BSS) {
			int bss_index;

			priv->BSSinfo[(int)(priv->current_BSS)].channel |= 0x80;

			if ((bss_index  = retrieve_bss(priv)) != -1) {
				atmel_join_bss(priv, bss_index);
				return;
			}
		}
	}

	priv->AuthenticationRequestRetryCnt = 0;
	atmel_enter_state(priv,  STATION_STATE_MGMT_ERROR);
	priv->station_is_associated = 0;
}

static void associate(struct atmel_private *priv, u16 frame_len, u16 subtype)
{
	struct ass_resp_format {
		__le16 capability;
		__le16 status;
		__le16 ass_id;
		u8 el_id;
		u8 length;
		u8 rates[4];
	} *ass_resp = (struct ass_resp_format *)priv->rx_buf;

	u16 status = le16_to_cpu(ass_resp->status);
	u16 ass_id = le16_to_cpu(ass_resp->ass_id);
	u16 rates_len = ass_resp->length > 4 ? 4 : ass_resp->length;

	union iwreq_data wrqu;

	if (frame_len < 8 + rates_len)
		return;

	if (status == WLAN_STATUS_SUCCESS) {
		if (subtype == IEEE80211_STYPE_ASSOC_RESP)
			priv->AssociationRequestRetryCnt = 0;
		else
			priv->ReAssociationRequestRetryCnt = 0;

		atmel_set_mib16(priv, Mac_Mgmt_Mib_Type,
				MAC_MGMT_MIB_STATION_ID_POS, ass_id & 0x3fff);
		atmel_set_mib(priv, Phy_Mib_Type,
			      PHY_MIB_RATE_SET_POS, ass_resp->rates, rates_len);
		if (priv->power_mode == 0) {
			priv->listen_interval = 1;
			atmel_set_mib8(priv, Mac_Mgmt_Mib_Type,
				       MAC_MGMT_MIB_PS_MODE_POS, ACTIVE_MODE);
			atmel_set_mib16(priv, Mac_Mgmt_Mib_Type,
					MAC_MGMT_MIB_LISTEN_INTERVAL_POS, 1);
		} else {
			priv->listen_interval = 2;
			atmel_set_mib8(priv, Mac_Mgmt_Mib_Type,
				       MAC_MGMT_MIB_PS_MODE_POS,  PS_MODE);
			atmel_set_mib16(priv, Mac_Mgmt_Mib_Type,
					MAC_MGMT_MIB_LISTEN_INTERVAL_POS, 2);
		}

		priv->station_is_associated = 1;
		priv->station_was_associated = 1;
		atmel_enter_state(priv, STATION_STATE_READY);

		/* Send association event to userspace */
		wrqu.data.length = 0;
		wrqu.data.flags = 0;
		memcpy(wrqu.ap_addr.sa_data, priv->CurrentBSSID, ETH_ALEN);
		wrqu.ap_addr.sa_family = ARPHRD_ETHER;
		wireless_send_event(priv->dev, SIOCGIWAP, &wrqu, NULL);

		return;
	}

	if (subtype == IEEE80211_STYPE_ASSOC_RESP &&
	    status != WLAN_STATUS_ASSOC_DENIED_RATES &&
	    status != WLAN_STATUS_CAPS_UNSUPPORTED &&
	    priv->AssociationRequestRetryCnt < MAX_ASSOCIATION_RETRIES) {
		mod_timer(&priv->management_timer, jiffies + MGMT_JIFFIES);
		priv->AssociationRequestRetryCnt++;
		send_association_request(priv, 0);
		return;
	}

	if (subtype == IEEE80211_STYPE_REASSOC_RESP &&
	    status != WLAN_STATUS_ASSOC_DENIED_RATES &&
	    status != WLAN_STATUS_CAPS_UNSUPPORTED &&
	    priv->AssociationRequestRetryCnt < MAX_ASSOCIATION_RETRIES) {
		mod_timer(&priv->management_timer, jiffies + MGMT_JIFFIES);
		priv->ReAssociationRequestRetryCnt++;
		send_association_request(priv, 1);
		return;
	}

	atmel_enter_state(priv,  STATION_STATE_MGMT_ERROR);
	priv->station_is_associated = 0;

	if (priv->connect_to_any_BSS) {
		int bss_index;
		priv->BSSinfo[(int)(priv->current_BSS)].channel |= 0x80;

		if ((bss_index = retrieve_bss(priv)) != -1)
			atmel_join_bss(priv, bss_index);
	}
}

static void atmel_join_bss(struct atmel_private *priv, int bss_index)
{
	struct bss_info *bss =  &priv->BSSinfo[bss_index];

	memcpy(priv->CurrentBSSID, bss->BSSID, 6);
	memcpy(priv->SSID, bss->SSID, priv->SSID_size = bss->SSIDsize);

	/* The WPA stuff cares about the current AP address */
	if (priv->use_wpa)
		build_wpa_mib(priv);

	/* When switching to AdHoc turn OFF Power Save if needed */

	if (bss->BSStype == IW_MODE_ADHOC &&
	    priv->operating_mode != IW_MODE_ADHOC &&
	    priv->power_mode) {
		priv->power_mode = 0;
		priv->listen_interval = 1;
		atmel_set_mib8(priv, Mac_Mgmt_Mib_Type,
			       MAC_MGMT_MIB_PS_MODE_POS,  ACTIVE_MODE);
		atmel_set_mib16(priv, Mac_Mgmt_Mib_Type,
				MAC_MGMT_MIB_LISTEN_INTERVAL_POS, 1);
	}

	priv->operating_mode = bss->BSStype;
	priv->channel = bss->channel & 0x7f;
	priv->beacon_period = bss->beacon_period;

	if (priv->preamble != bss->preamble) {
		priv->preamble = bss->preamble;
		atmel_set_mib8(priv, Local_Mib_Type,
			       LOCAL_MIB_PREAMBLE_TYPE, bss->preamble);
	}

	if (!priv->wep_is_on && bss->UsingWEP) {
		atmel_enter_state(priv, STATION_STATE_MGMT_ERROR);
		priv->station_is_associated = 0;
		return;
	}

	if (priv->wep_is_on && !bss->UsingWEP) {
		atmel_enter_state(priv, STATION_STATE_MGMT_ERROR);
		priv->station_is_associated = 0;
		return;
	}

	atmel_enter_state(priv, STATION_STATE_JOINNING);

	if (priv->operating_mode == IW_MODE_INFRA)
		join(priv, BSS_TYPE_INFRASTRUCTURE);
	else
		join(priv, BSS_TYPE_AD_HOC);
}

static void restart_search(struct atmel_private *priv)
{
	int bss_index;

	if (!priv->connect_to_any_BSS) {
		atmel_scan(priv, 1);
	} else {
		priv->BSSinfo[(int)(priv->current_BSS)].channel |= 0x80;

		if ((bss_index = retrieve_bss(priv)) != -1)
			atmel_join_bss(priv, bss_index);
		else
			atmel_scan(priv, 0);
	}
}

static void smooth_rssi(struct atmel_private *priv, u8 rssi)
{
	u8 old = priv->wstats.qual.level;
	u8 max_rssi = 42; /* 502-rmfd-revd max by experiment, default for now */

	switch (priv->firmware_type) {
	case ATMEL_FW_TYPE_502E:
		max_rssi = 63; /* 502-rmfd-reve max by experiment */
		break;
	default:
		break;
	}

	rssi = rssi * 100 / max_rssi;
	if ((rssi + old) % 2)
		priv->wstats.qual.level = (rssi + old) / 2 + 1;
	else
		priv->wstats.qual.level = (rssi + old) / 2;
	priv->wstats.qual.updated |= IW_QUAL_LEVEL_UPDATED;
	priv->wstats.qual.updated &= ~IW_QUAL_LEVEL_INVALID;
}

static void atmel_smooth_qual(struct atmel_private *priv)
{
	unsigned long time_diff = (jiffies - priv->last_qual) / HZ;
	while (time_diff--) {
		priv->last_qual += HZ;
		priv->wstats.qual.qual = priv->wstats.qual.qual / 2;
		priv->wstats.qual.qual +=
			priv->beacons_this_sec * priv->beacon_period * (priv->wstats.qual.level + 100) / 4000;
		priv->beacons_this_sec = 0;
	}
	priv->wstats.qual.updated |= IW_QUAL_QUAL_UPDATED;
	priv->wstats.qual.updated &= ~IW_QUAL_QUAL_INVALID;
}

/* deals with incoming management frames. */
static void atmel_management_frame(struct atmel_private *priv,
				   struct ieee80211_hdr *header,
				   u16 frame_len, u8 rssi)
{
	u16 subtype;

	subtype = le16_to_cpu(header->frame_control) & IEEE80211_FCTL_STYPE;
	switch (subtype) {
	case IEEE80211_STYPE_BEACON:
	case IEEE80211_STYPE_PROBE_RESP:

		/* beacon frame has multiple variable-length fields -
		   never let an engineer loose with a data structure design. */
		{
			struct beacon_format {
				__le64 timestamp;
				__le16 interval;
				__le16 capability;
				u8 ssid_el_id;
				u8 ssid_length;
				/* ssid here */
				u8 rates_el_id;
				u8 rates_length;
				/* rates here */
				u8 ds_el_id;
				u8 ds_length;
				/* ds here */
			} *beacon = (struct beacon_format *)priv->rx_buf;

			u8 channel, rates_length, ssid_length;
			u64 timestamp = le64_to_cpu(beacon->timestamp);
			u16 beacon_interval = le16_to_cpu(beacon->interval);
			u16 capability = le16_to_cpu(beacon->capability);
			u8 *beaconp = priv->rx_buf;
			ssid_length = beacon->ssid_length;
			/* this blows chunks. */
			if (frame_len < 14 || frame_len < ssid_length + 15)
				return;
			rates_length = beaconp[beacon->ssid_length + 15];
			if (frame_len < ssid_length + rates_length + 18)
				return;
			if (ssid_length >  MAX_SSID_LENGTH)
				return;
			channel = beaconp[ssid_length + rates_length + 18];

			if (priv->station_state == STATION_STATE_READY) {
				smooth_rssi(priv, rssi);
				if (is_frame_from_current_bss(priv, header)) {
					priv->beacons_this_sec++;
					atmel_smooth_qual(priv);
					if (priv->last_beacon_timestamp) {
						/* Note truncate this to 32 bits - kernel can't divide a long long */
						u32 beacon_delay = timestamp - priv->last_beacon_timestamp;
						int beacons = beacon_delay / (beacon_interval * 1000);
						if (beacons > 1)
							priv->wstats.miss.beacon += beacons - 1;
					}
					priv->last_beacon_timestamp = timestamp;
					handle_beacon_probe(priv, capability, channel);
				}
			}

			if (priv->station_state == STATION_STATE_SCANNING)
				store_bss_info(priv, header, capability,
					       beacon_interval, channel, rssi,
					       ssid_length,
					       &beacon->rates_el_id,
					       subtype == IEEE80211_STYPE_BEACON);
		}
		break;

	case IEEE80211_STYPE_AUTH:

		if (priv->station_state == STATION_STATE_AUTHENTICATING)
			authenticate(priv, frame_len);

		break;

	case IEEE80211_STYPE_ASSOC_RESP:
	case IEEE80211_STYPE_REASSOC_RESP:

		if (priv->station_state == STATION_STATE_ASSOCIATING ||
		    priv->station_state == STATION_STATE_REASSOCIATING)
			associate(priv, frame_len, subtype);

		break;

	case IEEE80211_STYPE_DISASSOC:
		if (priv->station_is_associated &&
		    priv->operating_mode == IW_MODE_INFRA &&
		    is_frame_from_current_bss(priv, header)) {
			priv->station_was_associated = 0;
			priv->station_is_associated = 0;

			atmel_enter_state(priv, STATION_STATE_JOINNING);
			join(priv, BSS_TYPE_INFRASTRUCTURE);
		}

		break;

	case IEEE80211_STYPE_DEAUTH:
		if (priv->operating_mode == IW_MODE_INFRA &&
		    is_frame_from_current_bss(priv, header)) {
			priv->station_was_associated = 0;

			atmel_enter_state(priv, STATION_STATE_JOINNING);
			join(priv, BSS_TYPE_INFRASTRUCTURE);
		}

		break;
	}
}

/* run when timer expires */
static void atmel_management_timer(u_long a)
{
	struct net_device *dev = (struct net_device *) a;
	struct atmel_private *priv = netdev_priv(dev);
	unsigned long flags;

	/* Check if the card has been yanked. */
	if (priv->card && priv->present_callback &&
		!(*priv->present_callback)(priv->card))
		return;

	spin_lock_irqsave(&priv->irqlock, flags);

	switch (priv->station_state) {

	case STATION_STATE_AUTHENTICATING:
		if (priv->AuthenticationRequestRetryCnt >= MAX_AUTHENTICATION_RETRIES) {
			atmel_enter_state(priv, STATION_STATE_MGMT_ERROR);
			priv->station_is_associated = 0;
			priv->AuthenticationRequestRetryCnt = 0;
			restart_search(priv);
		} else {
			int auth = WLAN_AUTH_OPEN;
			priv->AuthenticationRequestRetryCnt++;
			priv->CurrentAuthentTransactionSeqNum = 0x0001;
			mod_timer(&priv->management_timer, jiffies + MGMT_JIFFIES);
			if (priv->wep_is_on && priv->exclude_unencrypted)
				auth = WLAN_AUTH_SHARED_KEY;
			send_authentication_request(priv, auth, NULL, 0);
	  }
	  break;

	case STATION_STATE_ASSOCIATING:
		if (priv->AssociationRequestRetryCnt == MAX_ASSOCIATION_RETRIES) {
			atmel_enter_state(priv, STATION_STATE_MGMT_ERROR);
			priv->station_is_associated = 0;
			priv->AssociationRequestRetryCnt = 0;
			restart_search(priv);
		} else {
			priv->AssociationRequestRetryCnt++;
			mod_timer(&priv->management_timer, jiffies + MGMT_JIFFIES);
			send_association_request(priv, 0);
		}
	  break;

	case STATION_STATE_REASSOCIATING:
		if (priv->ReAssociationRequestRetryCnt == MAX_ASSOCIATION_RETRIES) {
			atmel_enter_state(priv, STATION_STATE_MGMT_ERROR);
			priv->station_is_associated = 0;
			priv->ReAssociationRequestRetryCnt = 0;
			restart_search(priv);
		} else {
			priv->ReAssociationRequestRetryCnt++;
			mod_timer(&priv->management_timer, jiffies + MGMT_JIFFIES);
			send_association_request(priv, 1);
		}
		break;

	default:**********	}

	spin_unlock_irqrestore(&c -*->irq****, flags);
}

static void atmel_command****(struct at76c5c -*ate *c -*)
{
	u8 t76cus = at76c5rmem8-c -*- at76c506-c -*- CMD_BLOCK_STATUS_OFFSET)****u8 06 wireration.
	Copyright 2003-2004 Simon Kelley.

  COMMAND code was deint fast_scan devnion iwreq_data wrqu****if ( Corpora=Kelle  This IDLE ||
	   L Corporas code from thN_PROGRESS)
		return****switch (06 wire) {
	case code tar*****
    includes code from tCOMPLETEge, (		river t76c5on_was_associaterom Jean Tourrilhei

    The fi****	at76c5get_mib-c -*- Mac_Mgmt_Mib_Type, MAC_MGMT_MIB_CUR_BSSID_POS,AC a airo  (u8 *)river CurrentW, wr, 6*****addressenter_t76ce-c -*-   ThIONe froE_READY******************C) David Hcan****der the Grmware moder the GPL  Jean Ts file conta0****d the Linux !ireless
    extensions, (C) at76c5he G-c -*- ******* else, (C) . unbss_index = retrieve_bss-c -*    by. unnotifyource506 plete = 1ee soff ( file.

  != -1s, (C)  The sojoingram is f,e file.

     byt the end/oriver operating_modeiverIW_MODE_ADHOC && by Mattriver , wrisize    0 under tsinds-c -*- BSS_TYPare _HOCblic License nder tinary form and, un!e module in b. The source is located a		re; you can redistribut0lic Li(C) Jean Toite_surveyussott = SITE_SURVEYextensionD it and/ore; you can redistri under t It .2002.lengthITHOUT ANRTICULAR PU at76  See the
 ireless_send_evenr
    ->demelMIOCGIWSCAN, & It , NULLblic Lihe GPL license version 2.
iteSut ev****inary form and, under the terms
    of the GPL, in source willnd the LinRRANTY; without even the implied warranty of
    MERas publishdule for reading the  form. The sonet.russotto.AtmelMACFW is used under the RTICULAR PURPOSE.  See the    GNU General Public Lcense for more details.

    You should have received a copyt the end ofThe source is located atPL license version 2.Joi  Thi the Linux wireless
    extensions, (C) as published by
    the Free Software Fou under tJean Tourrilhes.

    The firmware module for reading the MAC al queries about this code, please contact the currt the end ofoftwaauE.  SWLAN_AUTH_OPEN will ls.

 AuthenticrrilhR linuxRetryCnt  See the
 queries about this code, please contanux/ENTICATING)are
 		mod_timer Driver managementm/syst, jiffies + tto.AJIFFIESt will ssotto and coe <linuTransacrilhSeqNumITHOx000e it a***********wepr reon &&ither vexclude_unencryp   Flude 	#include <linux/sSHARED_KEY will  more#incinux/ctype*- linux-c -*- #incived a, 0 copy of te Softwar for
idge Online Systems Ltd }02 at76c504.h>
#t76c5wakeup_firmware cards.

	Copyright 2000-2001 ATcards.
host_info_cards.
*iface imDriver ux/device dev16 mr1, mr3orp. unialso
   ls.

 card_typ FreeCARDour opSPI_FLASHnux/The souet_gcrils.

    YoGCR_REMAPinclu/* inux up on-board processor */
/ieee80clear.h>
#include "at<lin4.h>
#ude <lirite16ils.

    YoBSR(at yoSRAMincluh>
#include <linux/jiffies.h>
#include <linux/mdelay(10.h>
ine D frowait for itER_MIPPOR(i = LOOP_RETRY_LIMIT; i; i--s, (C)mr1ration.
	CeadSCRIPTION("SupMR******mr3The name of the firmware file3includnd/o loa&russoBOOTte to the Free********lection1*/
static char *firmndatiothewls.

 businux/jiffBUyour opPCies.ware = NULL;
relend/oiiverhe Liceprintk(KERN_ALERT "%s:russ failed to boot.\n", table dev->name*****nd the -EIOct {
	Atmel#includux/device.b) Dahe name of the firmware file2))WTypexffff fw_type;
	const char *fw_file <l missing_file_ext;
} fw_table[] = {
	{ ATMENODEVct {
	A/* now checkUPPORredistr Decofle;
	initializrrilh throughx airthe FunCtrl fieldcom",		"IFACE, pollile chardetect_at76c502_3comx air,	"bin" },
	{ ATME,OM,	"atat76c502_3c Corpo, set#incerrupt mask,x airenables"atmel_at7s
MODUcalls Tx
MODURxbin" },
	{ ATMELfune.h>
sER_M");
MODULopyright 2003-200hi-c -*- " },
_FUNC_CTRL code wa, fine MAX_SINIchar *firmwGPL"ICE("Atmel at76c50x wireless cards");

/* The name of the firmware file to be loaded
   over-rides any automatic selection */
statne MGMT_JIFFIEare = NULL;
module_param(fine MGMT_JIFFI, 0);

/* table of firmware file names */
static struct {
	AtmelFWType fw_type;
	const char *fw_file;
	const charin" },
	se_fileule.h>
#inc fw_table[] = {
	{ ATMEL_FW_TYPE/* C,	"atmel_ching SelOK only MEL_Fe register thatMirrone MR4  0x18was58,	"R_MI_502,er */
#define BSR  0x02 ndatithew!(d
   over-rides any automatiwitching SelOK) fw_type;
	const char *fw_file;
	const cMR3 self-tesw_file_ext;
} fw_table[] = {
	{ ATMEL_FW_TY        Switching Select RegiGPR2                            0x0e
#1efine GPR3                            0x10
/*
 * Constants1for the GCR register.
 */
#define GCR_REMAP     0x04MINOR 98
opy_to_ux/dils.

    Yo(unsigned char *)ude <r 2 */* table el_at76c502",	, n 2 of(lude <as d400 ace->tx_buff_pol Pule16re rcpu(NT    0x0008      corp.T    0x0008   n 2 o   /* Acknowledge Interrupts n 2 
#define BSS_Sdesc        /* Acknowledge InterrM */
#deection --> SRAM */
cou<linue BSS_IRAM      0x0100     lecti
#define BSr0008          /* Acknowledge Intters.
 */
#he MR registers.
 *    0x0200          /* AMB /* MAC inithe MR registerM */
#define BSS_IRAM      0x0ETE       0_BOOT_COMPLETE     lection --> IRAM */
/*
 *Co*/
#define MA
#define BSbuild_versTMELn --> IRAM */
/*
 *Cot has been co
#define BS06 wirel       /* Acknowledge Int#define MIB
#define BSmajor been completed */

#define MI */

struct gfour fields in

struct get_set_mib {
	u8 type;
 reserved;
	
#define BSbin"_cl_atn --> IRAM */
/*
 *Coc {
	u32 four fields *cussotporatset_mib {
	u8 type;
	        _ACKI{
	{ AT0502 a/*YPE_5rmine nux/jof memory
MODUe;
	addres	{ ATer.h>
#inclprobe_at76c50ard cards.
net_device *de001 AT. unrcITHOUT cards.

	Copyright 2000-20 =    devyrigh(ype;GPL");
re8,	"pcW_TYP        table of firmware file names */
stati;
MODULE_DESCRe "atmelmon Ke6("GPL"#define RX_DESC_FLAG_IDLE   lley")E_LICEN5E("GPL"nd/o name of the YPE_502D,WType fw_ty/* No 
    d c32.h>
# so load a small stub which just);

/*teYPE_us*/

#def   Rssi;
	u8ab.h>
iin binary e <linux/jifies.h>
#inEEPROML;
m#define RX_DESC_FLAport for Itmel aOFFSET  AI are r
	u8 "Simon,    Sof tcces*/
#def RX_DESC_Pas de/ieee80211.h>
#e "atmel.h"

#defNK_QUALIT
MODULE_AUTHOR("Simon Kelley")FFSET          11
#define RX_Atmel a	256 * HZ / 100)

#define MAX_BSS_EN
*******_DESC_MSDU_POS_OFFSEdefine GPic char *firmware  = NULL;
modulFWType fw_tyype;
	const char *fw_file;
	const char *fw       Rssi;
DESC_P_file_ fw_table[] =  and Cambridge OnlAI are reset)   Yo fw_t

#d  Rs2003-200MSDU_POS_OFFSET ht
    by/* got   Rssi;,2_3COsquashRTEDagain untilMirronetwork       atmelde <lis opeNINT_DESC76c50x 802.of firmware file names */
static
#define RX_DESC_FLAG_IDLE        ESC_SIZE_OFFSET          6
#defFSET   	     e it }
License as p     Reserved;
	u8  4      4
#defineMac   Rssi;
easy iEL_Fis C) D.ine TXFFSET        9
#define RX_DEPARALLELde <liT       16

struct tx_ddefine******reLength;
	u8        Reserved1;

	u8   0xc000ht
    b     12
#define TX_DESC_C0x2E("GP TX_DESC_STt the end o/* Stand_TYPSDU_SIZE_inl atsh,      itR_MA froasNEXT  UPPOR/

#dac Afine RX_DESCFFSET        9
#define RX_DEclude <liL;
modulude <linux/crc32.h>
#iis fr
	u8        ddress of the card comes TX_STAm
    net.r0eserved1;

	u8   
   acketType;
	u16       HostTxLength;
};

#define TX_DESC_NEXT_OFFSET          0
#define TX_DESC_POS_OFFSET           4
#define TX_DESC_SIZE_OFFSET          6
#define TX_DESC_FLAGS_OFFSET         8
#define TX_DESC_STATUS_fine RXrcs, (C)nd/oerved1;

	u8 [0]	"atmeFFs, (C) u8 *******_mac[] = {<linmon K4KET_T5mon Ke /* Rx Frame} TX_Dpe;
	const char *fw_fil*** Invali        Rssi;. UPGRADE FDU_SIZE_****    ChiperType;
	u8	memcpy   /* packet reserRxFRAMELOht
    bETE    {
	{ ATrc Status;Move

#deencyp02_3cvicerm ATMEL* MirroMIB cards.ure.
   TDESCrout    is
 */

#depre-WPAENGTH_OFF: laisteSDU_SIZE_has_IBSa diffed coUPPOmat */
#AC in ISR_GENERIGE     E_OFFt76c504 and t has #incthe nclude <linux/proc_fs.h>
#include <lin{ ine BX_DESCis matcht charirrohardh>
#, don'tT   ngTE_OFFSEu8 #include _OFF ISR_RxFRAMkey;c_Mi0..3e     0x0AG_Vrve MAC u8 uff.h>
#include <liET_TYu32 WEPICV_errorine MAp_Mibhy_Mib_uff.h>
d           0x04
#dkeys[MAX_ENCRYPFW isKEYS][13]p_Mib_Tylude <fcntlevel      , 1, 205
#define Mac_We2[_CUR_} mibux/firmware.mib.#include <mware mo4
#define Madevice.h>
#include 0x02      TS_THRESHOkey_len[ls.

   _Mib_Type] > 5OfFradefiD_POS            = 2_OFFthe  MAC_MIB_LONG_RETRY_POS     17
#define TX__MIB_LONG_RETRY_POS    OUT     defi       16
#B_RTS_THRmt_Mib_Type _MIB_Buff.h>
#include <liB_RTS_THRype        0x06
#defineICE("Atme0; i < 

#define MAC_MGMT_; i++ODULE 0x10 define M 0x08i]e_ext;
}CUR_BSSID_POS1tic seieee80211.the card comesWep          0x01 T. Ru&mibREAMBLE_TYibas d2 at76c504 and  0x01
#pafine Mac_Address_Mib_Type    0x02
#d/*SS_MER    0x40   : IBSS( /* ",	"bid)S merge *E_OFF#define Mac_Mib_Type            0x03
#define Statistics_Mib_Type     0x0cipher_       16
#_value8

#define MAC_MGMT_MI

#define MAC_MGMT_SIZECUR_BSSIreceiver
	u8 ess[6CUR_BSSI4
#define Mac_Mgmt_Mib_Type       0x05
#defingrouR_BSSp_Mib_Type        0x06
#defi_BSSID_POS      nux/        2Mac_Wep_ne Phy_Mib_Type            0x07
#define Multi_Domain_MIB       RSC[4][8LD_POS             8
#define MAC_MIB_RTS_THRESHOLD_POS  MAC_MGMT_MIB_STATION_ID_POS           6
#define MAC_MAC_MGMT_MIB_ 26
#define LOCAe_ext;
} and copyright
   EMENTzero 
#deirro 0x0 before   RingSET 0x08  onesE_OFFSmemset_MIB_ATE_SET_POS              0x01T_MIB_LISTefine         CMD_Scan   R_ACKINT         10
#define MACENTEDere's alopedNERI TX_De At76c che Fx03
#deeff504_r 4 */
isrface
   0   /0x08  wheE_50ill u02E,_Mib, Hosmay net char e58,	"torface
someth    to use  /* _DESC   0x02
#deLOCAL_M                LOCAL_MR_ACKIMIB_BEACON_PER_POS     SID_SIZE     55   NextDescri_PRIVACY_POS          11
#define.

*************_RETRY_POS  idefihe LicensC_MGMT_MIB_ATE_SET_POS              _POS            14
#defB_REG_DOMAIN_POS       t will atus;
	u8          0
#defi under tMIB_BEACON_PER_POS TUS_OUS_FUNCUS_UNKNOWN                0IB_REG_DOMAIN_POS       -1T   7TED 0x04
#define         CMD_STATUS_TIME_OUT               0x072T   ls.

 pairwise_ATE_SETsuitCUOUSude <linux/slabTYPE TATUS_IDLE  RTED 0x0ls.

 SID_SIED_RADIO_OFF CMD_STATUS_REJECTED_RADIO_OFF     004
#define         CMD_STATUS_TIME_OUT               0x07
#defx/ether     CMD_STATUS_IN_PROGRESS            0x08
#define         CMD_STAT          0xFF
#de     0xopy of the GectioB_BEACON_PER_POS     e MAC_MIBfine         CMD_STATUS_IDLE!      ?CMD_STATUS_IDLE:HOUT AEY      TATUS_IDLE ne MGMT_FRAME_TATUS_IDLE        0x80

#de    (ARM and PMIB_PS_MODE_POS              53
#define MAC_MGMT_MIB_LISTEN_INTERVAL_PO6    G_VAuality;
	u8           PreambleType;
	u1us;
o every      nee DRary    RIVER_MA
#define Statiinf.h>ing     LE_S    efine MACightn    strike    0_FW_w     efinnife ux PCM....
     8,	"01
#defiMibEnabues RX_DESmatisteMD_StarW_TYPto      E_504,		irlatoYPTIn" }efini
	Copyright 20define ISR Som  Ratthe        conbe alteSC_M* Mirrofly, bu
#deny (  0x06nfrastine IS or ad-hoc)anada				CMD_Ebe_Mib_Tyd by tear    dowefine worldEL_FW_om    backL_FW_TYPE_504#def* RegulS_MERGE         alsoOOP_ponsibleUPPORTirror Re
/*
 omCanadafine Sta-specifict76c50
#define REG_DOMAIN_DOC		0x20	/*a_2958
#define e    py4",	"binc32.h>
#'snux/dviceefinne ISMAX_ENX_DESi      GE  FSETx03
#de*

 nnel	1-14	Japan( 2002define IS      Duration;
	u32          RxTime;
};

#define RX_DEevelopnfiguby
 POS   nt olden the im, MA  02111-13ssottorp. unerr under tus;
002.to   R        		*/

#deablesChan_PASority orderE_504,	 56
distmenents		1
#definbeen cox41	_HOC		16c504     0c32.h>
# theifierST    
		"-wpaer 2 "er 2 ed a
	}DESC_FLAG_VALID       0x80
#define RX_DESC_FLAG_CONSUMED    0x40
#define RX_DESinclude "atmelDLE        0x/*X_DEp	0x10	, dis	"bi" },
	{ ATMER_MINOR 98LE_DESCRIPTION("Sup      8
#defint76c50x 802.11 wireless ethernet caSC_RSS     0x04 ChanET  GTH_OFFSEfine  efine TXconstC		0x20FFSET		1
#*fwies rLE  ed ae LOCKOUT_GCR_ENINT     0fwL;
mont leontains thel */
#deRPOSE.L;
modul!(fwRL_OFFSET		28
#de       oin        		28
#defnux/jiffATMINDEWour opNONns, (C) d durinrlee is l_INT_TYPE_Oid             define ISR_FATINFOED 0x0atthewTMEL_FW_TYPnux/jis unknown: assu0-11	at76c502FFSET		1
#ds OKister 2 */P     2
MAND_COMPLETE  define CIPHER_SUITE_TKIP     2
#defidefiot,     BLE		1
#defi= module param	u8    4
#define CIPHER_SUITE_WEP_128  strx10 fine CIPHER_SUITE, "ude <line CIPH.bin"EP_128 ONS_S	EAMBLE- linuxrc32.h>
#i&fine IFAe_ext;
}LE		0x20
#def, MA  0ysPreaETER      0EAMBf the Licensefine ISR_FATAL_ERE_TKIP     2
#defiFSET		1
#%      _502E,,a			no4_29ntinugister 2 */ne CIPHER_SUITE_W reads the MAC addrEP_128  {
	{ ATerrN_OPTIONS_SI <linux/slab.h>
fwle.

    ee the
FUNCsuce DR under thketTypeOFFSET	2
#dfileable e IFAC2",	e REGFSET		1
#nux/jIDine TX_	while (fw_t	"bi[0x00, 0x].xd3,ux/j!L_OFFSET		28
#def 0x0ED 0x0	&& 0x0, 0x00, 0xa0, 0xe3, 0x00, 0

#define CIPHER_SUED 0x00x00, 0x++ea, 0x00, CKOUTrds.
ine Rctuaat760x00, 0xea,ne AU 0xff, 0in  0xa0, 0xe3, 0x00, 0x10, 0xa0, 0xe3,
	0x81, 0x11, copyrigh_STATUS_O0, 0         0xol */
#define PROMi]STATUS_COMPLFUNCnpe;
	f_RxENABLE		0x20
#def32,fw_f%s.%s", 0xa0, 0xe3, 0x00, 0x10xea, {
	0x0		x08, 0x10, 0x80, 0xe, 0x10, 0xc0, 0xe1, 0xb4, 0x_ex  /* 128  5

s the MAC addr[3
#def'\0'1, 0x56,     stub firmware image which reads the MAC address from NVRAM oWEP_64   1
#de		0, 0x00, 0fine CMD       TxStLETE		0x01E		0x01		0x01MAC_S0, 0x00 copyright information and source see the end of this file. */
static u8 mae, or[] = {
	0x06, 0x00, 0x00, 0xea, 0x04, 0x00, 0x00, 0xea 0xdc, 0xa0, 0xe3,ceiv, 0x00, 0x00 },
	{ ATMEENTMPLETE		0x0reles	AT_OFfine IFA->ine 1, 0x_CTRL_02, 0x3c, n 2 x/wirelesin  _CTR<atme600           0x4LE_DESCRIPTION("Support for DESC_LINKK_QUALITY_OFFSET  12UTHOR("Simon, 0x,C_CTe TX_DESC_FL211.h>
#include "atmel.h"

#def 0xea, 0x02, 0/* Remapine TX_0xe9, 0x12, 0x2e, 0xa0, 0xe3, 0x06, 0x30x37, 0x00, 0x00, 0xeb, 0x00, 0x40, 0xbd, 0xe8, 0x1e, 0xff, 0x2f, 0xe1,
	0x00, 0x40x9f, 0 TX_DESC_FLAGS_OFFSIPTION("Support 0x2ff 0x02, 0x80,e, 0xff, 0x2f, 0xe1,
	0x0x8NGTH_&fw[0x9f, ]x40,  -, 0x00, 0x023,
	0x28,fine IFA Free Sleaserc32.h>
#ie1, 0x80,_MGMT_MEAMBLE     0x80
#define TX_DONE  S      .
   For  = {
	{ AT 0x00ster 3 */
#finibeen com froCMD_DC		0orr504_ at7ON_KEwpaefinffa_2958oChannelnewER_SUITE_CCX inrediat-13	._DOMAIN      0x803com, 0x80, 0xreport    jor 0x01, 0x5a_2958x31	/0x10, 0x93, 0xe5,     1a, 0x81, 0x4x10, doesics_ne  , 0x1c, 00, 0xbroken-nx00,fidefiE_OFFS 0xdc,use#def =IB_SHORTux/device. */

struct get= 4x00, 0xdc,radio_on_x54, 080, 0xe5, 0x38, 0x10, 0x80, 0xe5, 0x35        unc504ry Doirq source0x10
#define PE_NONE,		NULL,			NULL }
};

#deINT_MASKSSID_LENGTmel_        FUNCTx systemx10, ",	"bi, 0xxf3, 0x16, 0xa0, 0xe3,
	0x08tx-c -*- TX_DESCde <Gs code way.hay.h>
# 0x16, 0xa32e5,
	0x02, 0x00, 0x10, 0xe3, 0NEXT0xff, 0xff, 0x
	0xf1, 0a cop 0x16, 0xa, 0x90,0x02, 0x00, 0x10, 0xe3, 0PO 0xff, 0xff, 0x0a, 0xff, 0x00 0x10, 0xe3, 0xfc, 0xff, 0xff, 0    0xff, 0xff, 0x0a,0x10, 0x100     freefine     0x38, 0x10nstants for t 0x10, 0x100     heaD_POOUT 0x00, 0xe2, 0xtaie MAC_MG0x00, 0xe2, 0xprevioporat, 0x2d, 0xe9,, 0x_meude  0x00, 0x91, 0xe5,
 MAC init 0x2d, 0xe9,08   1e, 0xff, 0x2f, 0xe108   0, 0x40, 0
_MACE_ACTIVE	0ration.
	Copyright 2003-200	NULL }
};

#define MAX_SSID_LEN5, 0x02, 0x00,NONE,		NULL,			NULL }
};

#define MAX_SSID_LENG by Matt	0xd8, 0x10, 0|TH 32
#defiTxENABLIES (2, 0x0it"atm00, 0x81, 0xe5, 0x5, 0x34, 0ETE     1e, 0xff, ,
	0xd8, 0x10, 0x9f, 0xe5, 0x00, 0x00, 0xc1, 0xe5, 0x01, 0x20, 0xc1, 0xe5, 0xe2, 0xff, 0xff, 0xeb,
	0x01, 0x00, 0x10, 0xe3, 0xfc, 0xff, 0xff, 0x1a, 0x14, 0x00, 0xa0, 0xeR, 0xc4, 0xffMAC_S10, 0x80, 0xe5,
	0x100x02      0xe9, 0xni_Do wirelLE_SSimon KelleE,	"biR0, 0ux/delay.h ==);

/* code from tREJECTED_RADIO cod       RetryCount;
SUITTMEL_FW_u8 ma	{ ATirror0, 0 onister 2 *e CIPHER_SUITE_WEP_128{
	{ ATMEL_FW_efine IS			0eost-Men_TYP */
#/

#defto runE_OFFSOUT  1000

#dright 20Localm
    net.rLOCALAtmelAUTO_TX_Rsed ttenxa0, 0xauto_tx_ratC_BOO0, 0xe3, 0x0c, 0x30, 0x84, 0xe5, 0x088, 0x00, 0xTXn ReMISCUOUS10, 0x 00, oftwar0x9401, 0x00, 0x10,  0x10, 0xomes     net.russotIB_RTS_THRESHOLrittenxa0, 0xrts_thresholx00, xe3,
	0x00, 0x00, 0x52, 0xe3, 0x0b, 0x00, 0xFRAG 0x9a, 0x10, 0x50, 0x9frag0xe5, 0x02, 0x00, 0x15, 0xeright 20, 0xe3, 0x0b, 0x00, 0xSHORT76c50x , 0x50, 0x9short_This6, 0x 0x01, 0x00, 0x15, 0xe3,
	0xfc, 0xff, 0xff,LONG, 0x08, 0x50, 0x94long5, 0x01, 0x50, 0xc1, 0xe4, 0x010x84, 0xe5, 0x08, 0x00, 0xPREAMBLEour ox50, 0x9preambl0x01, 0x00, 0x10, ine TX_ERROR                0ussoADDR0, 0x00, 0x02,tten by    2

#define MRfine TX_PACKET0x50, 0xc1, 0xe4, 0x01, 0x0from
    net.russotto.AtmelPSoftwar, 0x5ACTIVEoftwa0, 0xe3,
	0x00, 0x00, 0x52, 0xefrom
    net.russotto.AtmelLISTE00, TERVALre var******/
	int (*present_callback)(void *); /* And callback wBEACON_PE, 0x00#define        beacon_perio2, 0x00, 0x15, 0xe-c -*- Phyxe3, 0x0b, PHY0, 0x0sed SETre varude <lbasic5,
	0s,3c, 0x
struct atmel_private {
	void *card; /* Bus dependentMACFPRIVAC8, 0x50, 0x94 10
#definS            x10, 0x 0xe  54
#define Mis free         0x01
#define is free, 0x00e SCAN_TYPE=lMACFW is used under DE_OFin December 2002. It alsoRTICULAR PURPOSE.  See th    GNU General Public TICULap
	u8 .sa_familLE  ARPHRD_ETHERo be   0x02/* set if we donine 0x00,ETHcharN 0x02cense for more details.

    You shoulAPve received a cope ISR_OUT_O  Statt76c504 and at76c50x8c, 0x50,  Mac_Address_Mib_Type    0xChan4_295wire 0x01, 0x30, and *cmd
	u16 fmde sele
	u16f (cminux/, 0x0a, 0x01, 0x00, 0x50, 0xe203-2004 Simon Kelley.

  C_KEMETERs code wa, 0xff, 0xo;
	rag_sourc  0x00

#de 2.1.1 of the Atmel drivers,
    released by Atmel ,loped fr
};

struc	u8 wep_keys[MAX_ENCRYPTION_KEYS][MAX  This code waay.h>
ndler.h>
#include <l0x8c, 0x50, 0x9f, _buff_head, tx_buff_tail;

	u16 frag_seq, fr0x02 frag_no;
	u8 frag_source[6];  CM,_504A_2 MAC_MGMT_MIree_mem, txc -*- frag_seq, pairwise_cipher_sxa0, 0xe3500_PRI cards");

 Corporation.
	Copyright 2003-2004 Simon Kelley.

    This code was dehe terms
    of the GPL, ihe Li 0);

/* 
		u16 tx_desc_pos;
		in Reed,
    a = NULL;
mu_LICEN2, 0x0{
	AtmelFWType fw_type;
	const char *fw_filconst charc_reac    K_file_ext;
} fw_table[] =       Msd0, 0x84, 0xeHOST_ERROcrc_xea, 0x02, ;

	uped fro of the3, 0x60, 0x 0xa0      Msdeless
    extensionx_desc_tail, te, don'tesc_previou8nencryp of the816 host_info_base;
	struct hou8e, 0x_STATal Pubnclude <lin of MIB_PS_ mOS  .  9
#deMISCUOUm.    0x0e itm.e.

    e.

 't change. */
		u8 vol0x9f, 0xe5,
	0x0Get0, 0xVars, &m, */
_HEADER00, 0 +r firm{
	{ ATion.
	Copyright 2003-2004 Simon Kelley.

  el;
	u8 group_cipnclu_ERROR
	} statEN_INTERVAL_POS     0x01, 0x00, 0x_STATE_JOINNING,
		STATION_STATE_AUTHENTICATI_STATine NG,
		STATION_STATE_ASSOCIATING,
		STATION_STATE_READY,
		STATION_STATE_RE	mLAR P,
	0x00xa0, ASSOCIATING,
		STATION_STATE_DOWN,
	SSTATION_STATE_MGMT_ERROR
	} station_staesc_previous;
	u16 tx_f00, 0x00g_domain;
	int tx_rate;
	int auto_tx_rate;
	in0x01, 0xincls_threshold;
	int frag_threshold;
	int long_retry, short_    y;
	int preamble;
	int default_beaco
	int de
#define S>> 8con_period, beacon_period, listen_interval;
	int CurrentAuthentTransaction2eqNum, ExpectedAuthentTransactiSeqNum;
	int AuthenticationRequestRetryCnt, AssociatiT. Rounterhardine fineNG,
		STATION_STATE_ASSOCIATING,
		STATION_STATE_READ		int RS,
		STATION_STATE_REAS     	int RS >this_

#dDATA_BYTE    ape;
	const char *fw_fileIB 08  steroo  6
#d_file_ext;
} fw_table[] S         LAR P,iod;
S;
	int RSSatistics wsbeacon_period, listen_interval;
	int CurrentAuthentTransactionSSID_size, esc_previous;
	u16 tx of the int fast_scan;

	struct bss_info {
		int channel;
		int SSIDsize;
		int RSSI;
		int UsingWEP;
		int preamble;
		int beacon_period;
		int BSStype;
		u8 BSSID[6];
		u8 SSID[MAX_SSID_LENGTH];
	} BSSinfo[MAX_BSS_ENTRIES];
	int BSS_list_entries, current_BSS;
	int connSOCIATING,
		STATION_STATE_DOWN,
		STATION_STATE_MGMT_ERROR
	} stationSSID_size, new_SSIAI are reset) */
#define
	int0x01, 0power_mode;
	time_t last_qual;
	int beacons_this_sec;
	int cht SSID_size, esc_previous;
	u16 txLE_DEAR           PreambleType,nRequestRetryC_STATUS_outw
		u8eserved2",	
	u8  + ARe, n/*e TX_STATr Registeappearxff, ne   ne R16 cvinc2E,	int D_DEVICE("Atme0;EY_COM!_STAw   /* "} };

static v<linIVAC1_PRIine MAL, 3, 9, "Israel"} };

static voi2 at76c504 and at76c506_OFFSET  12, 1, 14, "MKK1" },
		      { estsociation, 0x1a_OFFSET	3
#define src     { RSSI;
	_STATUS_EG_DOMAIN_MKK1  Resers  /* MA    st % 2type;
EG_DOMAIN_M8char *DR,o_hosion;
	rc++;uct -- 0x040xa0, 0xe3	int i > 1 6 ma-=6 src, uu8 lb =o_hos00,  atmeh_clear_gcr(str#define RX_DESC_FLAid alb | (hb << 8, 0xe0400   ix40
#define RX_atic void atmel_senet_device *dev, u16 dest,
		eset), 1, 14, "MKK1" },
		    CR_ENINT     0 u16 len);
statiinclhost(struct net_device *dev, unsigned char *mel_set
    rcu16 src, uatmel"bin" },
	{ Aatic void 0x02    (struct net_device *dev, u16 mask);
static void at16 hT_OF     Reserved;
	u8hannel)atmel++ = hFACE_1_hdr *headeMPLETEDstruct atmel_tmel_private *priv, int channeum, ExpectedAuthentTransah>
#, 1, 14, "MKK1" },
		      {c504NG,
	L, 3,ice *dev, struct ifreGCR) |6c504aIsrael"} };

stati     atmel_wmem32(struct atm
MODULE_Amel_send_command(struct atmel_private *priv, int command,
			       & ~id *cmd, int cmd_size);
static int atmeinclude <l*****mac Mac_Address_Mib_Type    0x02
#dhardwarjITE_0;
 This   at_mask;
		u8 volatile lockoMAC_Sf, 0xe5, 0x00, 0x00, 0xc1, 0xe5, 0x01, .

 OUT

		u1code wasrx_buff_size;
		u16 rx_desc_pos;
		!tmel_tail, tx_c_Mi/sysd ou     xe2, 0xff, 0xff, 0xeb,
	0x01, 0x00, 0x10,l_privatussodio_on_br*****        9
#ic u8 atmel_get_mib8(struct atmel_private *priv, u8 tyrc, u16 len)pe, u8 index,
			   u8 data);
static void atmel_set_m, 0x02el_sejrtOfFra8(struct atmel_private *pr		gof, 0eadex_desc_tail, t1K" },
		      { REG_DOMAx00, 0_STATE_JOINNING,
		STATION_ST16 pos, 07
#s_threshoEG_DOMAIN_MKK1ls.

    Yost fourx37, 0x00, 0x00, 0xeb, 0xid as_thr atmeW_TYPis little-endianx10
#define PROM_MODE_BAD_PROT);
statiMPLE1
   tatusatmel_smooth_qual(struct atmel_private *priv);
static void atmel_writeAR(s/tus;
#def follow       0xe1,IC_IR4",	"bin  KeyIndex;
	u8 PTION0x80, 0xt_devitructtmel_smooth_qual(struct atmel_private *priv);
static void atmel_writeAR(stru#if 0atus;CopyrE 40 2003 Matthew T. Russottot_devicte);
int atmel_open (struct nettruct But derivC_MSrom_Start     76CIPHER_SUITE_CLE_DtenEG_Dt      frouct net_device
#defiedK_OFine FU cense fo lan d;

srs" packaget_devicopen (struct net_devic*_IBSAIN_FRe5, 0is
#de	ISRAnet.r, int n.t    MACFW,ine R atmeferSC_Mtomel_pasate *pv->ho
mel_p;
}

stati6
#de 0x0softh>
#; youa				redi * 8b0	/* triv,/orld:
ifymel_pHostndster e u8  s4",	"binGNU General Public Licens, 0x01, 0x2+ offsetpeof(s   0G_DO		"a 0x0Sct atme FoundIVE	0.c inline u16 atmel_rriv, u16 o priv
#defop * Fa* Hosw    be    ful,mel_pTSI	WITHOUT ANY WARRANTY; withate  det u16 RAMEieDULErran	2

fmel_pMERCHANTABILITYropeFITNESS FOR A PARTICULAR PURPOSE.  Se * Fumel_psc_pos + (sizeof(struct rx_xa0,m   Cde0, 0sct atmeYou should hav5,
	6
#de 0x8 Channel	1-1sc_pos + (sizeof(struct rx+ offs 0xfct txine u16 atm;ns
 */

/LE_DE        inline u16 atmel_pel_tx(stru, Inc., 59 Tedist Pl02   S
#def330, Bost netMA  02111-1307  USA

ev);
static int reset_atmel_card(struct net_device *dev);
static void atmeltruct nevate U_SIZE_Oel_wriSC_N REG_DOMatmel_hRFMD,et);
_D,6,		"astatEuct net_deviceIturn prLinkablyv->base_addr + off0xe5, + offset);
_3COMpen (struct net_deviceItce)	*/SC_N  CM SPI SC_RSS 0x01, 0nfo.rx_de
	u8.atmel_open (struct net_dev6 data)
{
	outw(dain" },
	{0xffhe>dev,c_rerollruct_FW_ Atmelof t      8
#dt_device  Rssi;
tic inlin DR);
} ChanAtmec inliputata)
{
RAM off7, 0atic int prrite8(priv->devin502Dc inlisetsnts fto 0x10 /* Midx/ctoffseprivonc inline u16 aturn atme* Cha16 at     istrib Channel	1-1a);
}

st_rmem1v->baa)
{6(strucinpriv->devMR4,France	vestigIVE	0al purposes (maybe we von;
	u8      chipe, 0xf(priv->devtic inlat?)tate);
int atmel_open (struct net_int atmel_open (struct net_dev
	.org 0mel_p.7, 0MRBASE00, 0x91, 0
	ruct CPSRng SeIAlay.xD3c_MiIRQ/FIQ PROM_MOd, ARMld:
fset)perviRIVEn the ic v)
{
	int iUSE01, 0D10;
	     atmel_rmem8(priv, atmel priTX_DESC_FLAGS_OFrmem_mel_pron K2te *priv)
{
	SPdesc_couount;F3e *priv)
{
	UNKu8 status= at91, 0xc_Mihanneatmelritereambl_ETSI	MAIN_Ione?>host_info.PI_CGENdesc_count;E, TX_DESC_STATUS_OFFSET, priv->tx_desc_head));
		u16 msdUNK3desc_count;
	14X_DESC_STATUS_OFFSET, priv->tx_desc_head));
		u16 msduTAC atmel_t0x56  i++) {
		ux01, 01 i++) {
		uTDREne M			0x    Mr Registebit -- TDR empty
		u16 msdu__RDRF, _descel_tx(priv, TX_DESC_FLARDR full, priv->tx_deSWRS0xffx8tx_desc_headPIEN);

	i++) {
		u PROM  void el_wripriv, TX_D priv->tx_deMR, 4msdu_sthe Friv->host_info.tx_bufRid a0x08 atmRe, 0Dx);
Riv->host_info.tx_bufT
			priC atmtdevimitf_head = 0;
		else
			priv-CSRx Fra3DESC_6(prisel504_r Regist0x10
#>tx_desc_he1d < (4 - 1))
			pri2d < (8 - 1))
			pri3d < (C - 1))
NVtx_delleRDort 5 atmof t0);

		priv->tx_fnt - 1))

		if (typeEAD,  0;
	KET_Tine Sf (status == TXSR_RDYead), 0RDYDESC._privatESC_0x0e,verffse		u16 msdu_si8C.

 S		prFF atmWrRYPTIOATA_Fx03
#deGS_O0x30, 0xdo an PS_MODx03
#dct {
		serialvateput    nce SOev->n_IRQlly high. e *deiLAGS{
		0x30 ca    8iv->de cycbin"11 relus 8v->d
		}
buct atmev->defset, u03
#de6(prset)
{
t    'sdu_s(priv->tel_writeAR(e.g. AT91M558, 0xti
#defind 4Kiv->hosdev, DR);
}manual	{ ATMEv->stats.tx_CRATCH		pri
	  100sdu_sarbitr#defareat);
}scr    padte;
	u8 f (status == TXIMAG, pri

	if2priv)
{
	
		if LENGTtail;

	ios + pr00, 0x02ESS0, 0,o.tx_desc_os;
	}

	return 0;
info.tx_6os;
	}

	reic chxfc,);

		atmel_wMiv->tis_bcast,2, desc_heaMdesc
			priv u16 0xC
REdeviVECTOR:
	b m16(prHANDLER
UNDEFriv, atmel_tHALT1
SWI_OFFSET, priv->tx_IABx0a,OFFSET, priv->tx_Dmel_wmem16(prim16(RVEDc_tail), buff);
	atRQmem16(priv, atmel_FI_tail), len);
	if (v->tx:priv->tx_x(priv, TX_DE:
	movmel_hr0, #int i;

	for
	msr	int ic, r0MENTED 56
#del_privatunE		2

#defl_privaI'm gue502E,Type     in" },
	{X_DEf_sizegs + (tor info.ronic   0x4dev,ic vldr	HOST=u_size = atme, TX_	de <#0, TX_DESC_de <lsl #3
	orrFSET, priRATEst_tail)[r0]mem8(prte);
	, #28]
	bictail), pri16->tx_rate);
	iv, atmeTX_DESC_R1ESC_RETRY_OFFSET8]
mem8(priv, atmel_, TX_DESC_RATEstrhwmem8(priv,MR1]ype, cipher_length;2		if (is_bcast) {
		3		if (is_bcast) {
		4privTX_Dsp, #PE_OFFSET,
	bl	SPng Seriv->txFSET		atbl	DELAY9  cipGEoid at0x02pe == CIPWHOLE_
		ifiv->use_wpa) {
		inel_wmem8=
	return 0;
}

	if (is_bcast) {
			cipse if (ci_buff_tail oup_cipher_suite;
			iiv->tx_desprivate *privipher_type = priv->grov->t2em16(pri2
.bin" 	STAWhole	ciphe, _128)
				cipher_128)
				ciphe:ype,mdb	sp!, {lr}ise_cip2priv     th bytenfo.r
		if      X_DE3, #>host_info.t, TX_DESC_RAxe3, 8 mau0xffinRGE      _wmem8(priv, th = 12;
			ebl	
		if XFERciphmiaiv->pairwisebx	lr
.endbin"
se_cipher_sHER_Scomma CIPHER_SUITE CIPHER_SUIType = priv->pairwise_cipher_sux12c_tai  Rssi;
om",	"b TX_STATt txinlength = 8;
			} elsate_descriptor(strer_type = CIPHER_SUITE_NONE;
				cipher_length = 0pher_type == CIPHse {
			cipher_type = priv->pairwise_cipher_sui.ltorgse_ciphDLICE9, her_typher_ty:
	adds||
					cipheLSL #3msdu_sr0* A 0 * 9her_her_tl atmel_eqpher_ty_ atm
	sub
				ciphesc_tbpher_tl atTE_WEP_= 8;
mel_ise_cipher_suite;
			UITEnit, = 0;_WEPUITE_WEv, TX_DESC_Rriv->tx_mem8(priv, at= CIPHEC_RETRY_OFFSET_head]CESS)
0xa0, 0xtmel_wmet cipher_type, PE_OFFSET, priv->tx_des 0xa0,
			tic iesc_taX_DESC_FLA8(priv, atmelf (pER_TYPE_OFFSET, priM->tx_dc_tail),
			to MASTERiv->hic v;
		atmel_wmem8(priv, atxe5, 0xa)
{
	atl_priva Myriv, T wl_wribXT_O;
		uric i* desc
	atm_size_wmem8(pr3v, atmel_tx(priv, el_wmem8(p els atmesc_tail), priv_TYPv->tx_rate);
priv->txcipher_lengX_FIRMc0c_tail), 0);
	if esc_hea	cipher_lengX_FIRM2priv->tx_desc_tail)
		at
		if (x_desc_tail)
		at	cipherx_desc_tail)
		atgrouel_wmem8(priv,head)esc_prev0ous = priv-RDtx_d			 priv->group__ciph_buff_t
			}_buff_t_WEPdesc_count:cipher_lengSC_CIPHEResc_tail;
v, atmeiv->tx64 ||
			
		if (type ==priv, a		priv->tx_dTv->tSP_loop1iv->tx_d		priv->tx_d>tx_dtst||
			mem8(prpher_lem -= leif (cipt_tx(st		netif_wfree--;
	priv->tx_free_mem -= l2n;
}

static netdev_tx_t start_tx(struct sk_buff *skb,2riv->use_wpriv->tx_desc_tem -= l3n;
}

static netdev_tx_t start_tx(strsc_hsk_buff *skb,3tmel_private *priv = netd	andlse {
				255NONE;
				cipher_length {
			cifcces{
			cipher_->group_    uefine RX_DES->gr TheSUITE_NONallback 2 =valipresent_cgth = 8;
			else ->groadeRPOSE. */ - 1))
ipheype = priv->pair4, r5, rwise_ciph5desc_ 0xf7e8(st0      u  Rssi;)LENGTH_OFF		re3
	}

	if (p38, 0xgthate != STAT		ci2typeR #X_PACwmem8e;
	u
#input ANTIC0, 0xe3ed fro76c504R_MINn = (ETH_ZLE828) {lse {
				 == TX_STATUS_cipher_length = 1_buff_tpriv,bstatic netd0]	}

	if (V_TX_OK;= 8;
			v->timer,
	02(priv,bpher_riv->t1]tus }

	if (low		cip)
		16 datariv, u16 po/
	spin_lock_i1qsaveructalen;
se {
			cE_WEP_start_tx(ats.tx_packe
	bne	ock */
tail = 0;
	2   cipher_type_cipher_r4
	}

.tx_errorsE_OFFSET, 5
	}

iv->present_callbpacket we 
	}

	ciphee ==deviSS_linrdware ISningse {
			ciphe2r_type = priv-
		return NETkb->len) ? skb->len : ETH_ZLEN;
2
	if (priv->2s++;
		dev_4[6]ee_skb(skb);
		returr6urn NETm8(pr4sc_tail++;
	
			} els0
	cmp net_d   cis Ethernngth r_TX_Oh(&priv->timerlock */4iv->txb	_dro[TX_OKgroup_ctif_sto4->tx_free_mock */(dev);
	s
		return NEtx_t start6tx(struct sk_bufTX_BUSYe timer3_que			cipriv->= 0;uite;r0ev->#)
		lds of thD_sivate _status;+  Rsate !=bloer.dura4_TX_BUS4[6]
			} elsevice *dev)
{
	sta3		return NETDEV_	else
		priturn NEiv->tock */5n;
}

static ctl = IEEE80211_Fader;
	unsigned la(skb, skb_copy_from_linear_das);
what'ata)riv-yte?  I, priveoing tKET_Txea,
	LE_Dd definiGS_O-- nonst rx, beo.tx_b elsength 0x30, 0xKET_TMODUL
stat4 */

#s1c, /sys appropriate
	(&priv->er_sui = 0;2	if (priv->wep_is__MASK_OF0211_Fflags);
6ta(skb,7iv->tx_d5ddr1, 6);
		memcpy(&h5YPE_DATA;
	header.dura7 == IW_MODE_ADHOC) {
		;
		sk3/
#d;
			netif_weadlock */8

	if (priv->use_wpa)
		memcpy(&headeddr2, dev->dev_a8
	if (priv->use_wpaeader.adTSI	didics_w
		framiv->BSSID abov);
		u1&priv-riv->1]ck, s);
p)			*dex*priv->timer func caader.seqrrors11_FCTL_PROT7s);
	 u16 weistics_on)
	anothructe       der.addr1,must/
#deap5, 0xhe a linn },
, pos);adlock */6f (priv-t we "   cipher_typeh is then the
	   in_dropped+NE;
		#(strf
