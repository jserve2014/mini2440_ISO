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

static void atmel_command****(struct at76c5riveate *rive)
{
	u8 	Copus =

	Copyrmem8-river

	Copy06ight 20CMD_BLOCK_STATUS_OFFSET)****u8 06 wirer*** -.
	Copyright 2003-2004 Simon Kelley.

  COMMAND code was deint fast_scan devnion iwreq_data wrqu****if ( Corpora=rs,
   This IDLE ||
	   L includesy Atmefrom thN_PROGRESS)
		return****switch (oped fr) {
	casey Atmetar*****
    includeers (C) BenjaCOMPLETEge, (		riverL Cor5on_was_associateBenjJean Tourrilhei rel froe fi*****
	Copyget_mibight 20Mac_Mgmt_Mib_Type, MAC_MGMT_MIB_CUR_BSSID_POS,AC a airo  (u8 *)Jean TCurrentW, wr, 6*****addressenter_	Copeight 20 froIONC) BeE_READY**********license C) David Hcae Lindn Tohe Grmware mos file coPL ware mos filDavinta0 Thisile cLinux !irelesseadinextensions, (C)

	Copye coight 20license els, (CC) . unbss_index = retrieve_bssight adinby thenotifyource506 plete = 1ee sof    rm a  rel!= -1 form. ng thsojoingram is f,the odify
 ee sotile cend/oJean Toprom vng_the ean IW_MODE_ADHOC && by MattJean Tyrigisizeblic0 uns filsindm is f- BSS_TYPins _HOCblic License Licensinary form and, un!s theule in b.the ter cane GNlocated a and; you e GPredistribut0ny latd ofare modite_surveyussott = SITE_SURVEYin sourceD itsoft/obe useful,
    but We Licens It .2002.lengthITHOUT ANRTICULAR PU

	Co  Seeile 
 of the _send_evenrPubli->demelMIOCGIWSCAN, &TICU, NULLany latule in lter ververurce 2.
iteSut evalso
    This softwares file cterm GPL, ofodule in,ibuthe hopewillnhe terms
RRANTY; withose
  enile cimplied warranty ofPubliMERas publishistrifor readingile cThis d in thenet.ren theo.At76cMACFWe GNusedwireless la
    GNU GeRPOSE. Public LblicGNU General P, MAc Ler ver-130more detailsl PubliYou should have rec Fred a copLicense as ofin the hope that it wilte GNU General Public Joi frome terms
   d frthe GPL, in source form. Tton, MA  ed byPubliKellFree Softains Fouwirelessare module fororporatig the ntains the2111-1307  USA

    M by l queries abon, tom t Atm,dist) Davintacicensecurr and Cambrid*****au  SimWLAN_AUTH_OPEN Free CorporAuthenticle foR ls
  RetryCnt Public Licinux/kernel.h>
#include <linux/ptracenux/ENTICATING)are
 		mod_timer DJean Tmanagementm/syst, jiff/ker+ this JIFFIEStg.h>
#ut thisoft coe <>
#iTransace foSeqNum  Sex000e MERCL license vwep307 on &&ither vexnux w_unencryp   Fux w 	#Linux wnux/nex/sSHARED_KEYg.h>
# not arp.lude ctype*-h>
#inight 20ude <e to , 0HP UK; if ********* and
idge Online Systems Ltd }02003-20044.h>
#	Copywakeup_/

#inclucardorpoon 2.1.1 of th0 Atm1 ATnclude host_info_nclude *ifaope mm.h>
#iux/devic Atmv16 mr1, mr3orp theialsoPublCorpornclu_typ*****CARDoushedSPI_FLASHude in the et_ge.h>orporationGCR_REMAPLinux/* s
   up on-board processor */
/ieee80clearinclurp.h>
#i"atux/n#inclu>
#inclrite16#include "atBSR(at yoSRAMLinuxLE_AUTHOR("Snclude s.h>
#iULE_AUTHOR("Snclude mdelay(10inclinclD) Bewait and itER_MIPPOR(i = LOOP_RETRY_LIMIT; i; i-- form.mr1om versioneadSCRIPTION("SupMRlicensmr3g thname; if not/

#inclunera3Linux HANT loa&bout BOOTte to*********license lec** -1*/at76c504char */

#nd*** thewincludbusireless eBUou c opPCtherains =ved a;
opmeHANTiean termsceprintk(KERN_ALERT "%s:bout failedr *fboot.\n", tablh>
#i->
   licen Softwa-EIOcte, ( codeAUTHOR(leparam.h.brsioded
   over-rides any automa2))W netxffff fwinuxe;
	constware, chw_rm an<l miss    nera_ext;
}76c50xt;
[] =e, ({ ATMENODEVFW_TYPE/* now checkUICE(   but  Decofl",	"initializle fo throughxMatt*****unCtrl fieldcom",		"IFACE, pollm andhardetect_03-20042_3comE_504,	"bin" },},
	{ AT,OM,	"at2958,	"atme inclu, setAUTHerrupt mask,E_504enxt;
s"at76c5at7s
MODUcalls TxEL_FWRx_2958",	"bin" }LfuneinclsD_DE");EL_FWL 2.1.1 of the Atmhiight 2058",	_FUNC_CTRLy Atmel , finclMAX_SINIare, charpwGPL"ICE(" code003-2004evelopmentincludMEL_
/******
   over-rides any automachar eon *dedPublover-ri wirany autom6c504semodule_ER_Mt76c32
#to.Alinux/atic struct de <li_param(H 32
#BSR  0x02, 0RIES	64ext;
}of*/
#define GCR 
   s */
#defion ards.
TYPE_502F	"atm76c502d",	"bin" },
	{ ATMEL_FW,	"bin" },
	{2958",	"se		"atul" },
AUTH6c502e",	"bin" },
	{ ATL_FWour E/* C{ ATM76c50hSA

SelOK only rror t isgist fileatMirro32
#R4  0x18was58,	"_DEV_502,eVER_M#deH 32
BSR GPR02 , 0);
/* !( (SIR0)  General Configuratix PCM MR4  0x1)rror Register 1 */
#define MR2  0x14    MR3n Ref-tesMEL_FWmel_at76c502e",	"bin" },
	{ ATor Regiublic   Sfine GPR3   ect RegiGPR200      */
#define GCR_SWRES0x0e
#1  0x0cGPR30 */
#define GCR_SWRES WRES   10
/*
 * Cin" ants1-130le coCRor Regist.
        0x0cmel.h"

#WRES    4MINOR 98
opy_to_lepa#include "at(unsignedware, c)>
#inr 2 *
#define 
	{ AT8,	"a",	, n 2 of(h>
#in cor400 ace->tx_buff_pokellle16re rcpu(NTRES    008nd PAIcux/fge Interrupts */
#d   /* Acknowless.hIet.r_at7s          0x0c
#S_Sdesc and PAI00          /* AMBA mM       gister --> AtmeER_Mcouux/ne--> SRIduleWRES    100/
/*
modul     0x0c
#rrrupts */
fine BSS_IRAM      0x0ters0x0060 inclCORES          RES    2stants #define MMBe BSludein" 0001        /*        on --> SRAM */
/*
 *CoETEand PAI _ic c_xtensionants for t AMBA mAM */R_MIOT rCo       0x0cMA     0x0c
#build_al PP   _INIT_OK             t has been co     0x0c
#oped frl/
#define MAC_INIT_COMPLE002      IB     0x0c
#major_DATA_BYmistrid    0002      Imib {ne MR1 gfe na76c50s in8 size;
	uet_sof the  0xMEL 2d",	 reserved;
	 /* MAC bootin"_c	{ A_INIT_OK             cB_MAX32 8 index;
	u8*cen thludetdata[MIB_MAX_DATA_BY airtate;_ACKI },
	{ 05andl/*YPE_5rminclrelesof memoryEL_FW    y him.,
	{ eDULE_AUTHOprobe_2958,	"
#denclude net_aram.h>*de#inclu therc  See tnclude <linux/proc_fs.h>
# =tatedev.1.1 (2d",S (2)t {
    pcRegis State;
efine AR   0x04
#define DR   0x08
#deL_FW_TYE_DESCR"Simomelrivers6("S (2  0x0010RXRX_DEde <G_he Li  ,
  ")E_LICEN5E    0xectio
   over-rid;
	u802D,/* Mirror R/* No Publid c32inclu sion *to Hsmae.h>tub which justRIES	6te;
	uusib {
	u8   Rssi;MAX_ab("GPLbuteong wi11 wireless thernet caEEPROMR1) 00

#define RX_STATporSUPPORIZ / 10code w  AI ins rMAX_D" driv,      if cce     8
#dine RX_SP corpINOR 98211incluC_FLAG_I.h" {
	u8NK_QUALITdefine Rnux/OR(
#defiers,
  ")K_QUALI6

struc11     0x0cRX_HZ / 10	256 * HZ / 100) {
	u8 typeAXFW, _EN
licensee RX_SMSDUitte code         Prmware, charpains /* (SIR1)  Ban /* Mirror RRegister 1 */
#define MR2  0x14    /*  ATM6

struine RXOFFSET		"atm6c502e",	"bin"lude Cambress.h>
#TY_OFFSEeset)ation76c50 {
	yInd the Atm     TxLength;T htPublibfinegoteyIndex;,2_3COsquashRTEDagain until*/

#detwork6

struLAG_I.");
Mshed NINTRX_DE)

#def802.AR   0x04
#define DR   0x08
#defx_desc {
	u3 RX_STATUS_SUCCESfine RX_SSIZE
	u8    FSET     6BLE_TS_OFFSE  Statx/eth}
ater ver**** KeyInS];
};

stu8  4FSET  4     0x0Ma/
#dine RXeasy i    is ersi.inclTXGS_OFFSET    9C_SIZE_OFFSET PARALLEL.");
MFFSET   168 size;
	tx_d 0x001licensreLPOSE. TX_DES  KeyInS];
};
1****   130xc000    PackT     2     0x0cTSET    C0x2
#defDESC_PACKST and Cambri/* Standour     SC_FLin 100shne RX  itR_MA) BeasNEXT  tmel_b {
	ac A
#define RX_T          11
#define TX_DES.h>
#inclR1)  Ban>
#include crSDU_SIZEi GNUET  12        him. ; if notnclut geesDESCSTAmPublies ab0ne TX_DESC_CIPHERPublacket net TX_16       HostTxPE_OFFSE}; {
	u8 typESC_PACKerfaLAGS_OFFSET      0YPE_MGMT     0x02ed;
	u8    no bits se_OFFSET  FFSET   17C_FLAGS_OFFSET         8
#R */
#defineTATU     /* no bits s8in ISR */
#define  This sc {
	urc form.HANT TX_DESC_CIPH[0]*/
#deFF form. T. Rrame;
	mac"bin" ux/nIME_O4KET_T5IME_OFe BSRx Frame}     d",	"bin" },
	{ ATMEL_F at Invali 13
#define R. UPGRADE FGTH_OFFSd at    Chiper_DATA   8	memcpyine BSp_TYPETES];
RxFRAMELO    Pack*/
#def },
	{ rcSC_Htus;Move {
	uencyp"atmeam.hrmMAP   * */

#MIBurationure.PublTX_DEron,  * Hsx0060BLE_pre-WPAENGx/stFF: laegisNGTH_OFFShas_IBSa diffINT otmelmat     ine M ISR_GENERIG
#defiFLAGSer.h>
#lude B_MAX_AUTHC_BOp.h>
#include fine_fernet cards.");
MO{ 0x0c
SET   is match   /* Mrro_TYP/
#d, don't no ngTFLAGS_Ou8
#def     .2.380

# complkey;c_Mi0..3lley     AG_Vrvncludeu8 uffrnet cards.");
M    Y  MsWEPICV_error      pm
  hym
   ype    d GCR_SWRES    _OFFkeys[  TxENCRYPleaseKEYS][13]  0x0_Ty      fcntleve MIB_HE, 1, 2050002      ac_We2[lMACF} mibux//

#incl.mib.arp.h>
#intains th in ISR */Maat76c50/
#defi     ine GAC_MITS_THRESHOkey_len[include m
    net] > 5OfFra 0x0ritte*/
#define G= 2ib_TyelleussotIB_LONG76c50x ONG_RETR17in ISR */
#d7
#define MAC_MIB_SHORTee t;
};

f8       16
#B_R_SHORTrom
    net 7
#deBype        0x06
#deOS       efin_SWRES        0x01 56 * HZ /0; i < 
	u16       sotto.A; i++fine I areMIB_BPOS ine 8i]gister.
MACFW, writte1tion Refine RX_Ddefine TX_ERROWe     9
/*
 *Con T. Ru&mibREAMBLE_TYib corndler.h>
#lude  *Con
#paB_FRAG_THA him.   0
#definnit has
#d/*SS_MERMAIN_I4tant: IBSS(e BS"504_2d)S merge *FLAGS_MIB_FRAG_THMULTI_DOMAIN      53
#d3     0x0cANGEisticB_MULTI_DOMAINine cipher_EACON_PER_P_value8Y_POS          11
#MIY_POS          11
# ISRMACFW, wis due ET  12ess[6MACFW, wESHOLD_POS  es from
    net     53
#dAC_MIB_FgrouCFW, UR_BSSID     6
#define MAC_MFW, writteHANNELude HANNEL_P2G_THREp_ne P7
#defi            Y_MIB_RETRY_LIMITMulti_Domain7
#d13
#defiSC[4][8L_LONG_RETRY_POS  */
#define   17
#de       _RET_CONTROLussotto.AtmelMPLEION_L_MIB_TX_PRO         0x01       ssotto.Atmel 2    0x01  LOCAgister.
 ude <l2.1.1 oPublEMENTzerolecti
#deine  befot A  Ring    SSID  onesFLAGS_memdata
#deAied ETIB_SHORT_HANNEL_POS  1.AtmelLISTIB_CURx03
#defelleSe GP  R	u8  dge InCON_PE          MACENTEDere's alopedfine     e A IntOM,	 F       eff504_r 4    isrde <Publtant/_Vars wheOFFSh>
#u02E,ne P,defimay    ware, e     to      someth80
#do conne BSET   AIN_IMPLEMe    L_*/
/*
 rvey        CMD_SCMD_Jo MAC_EACON_PERIB_SHORT_ wri ISRrvey 55   NextDescri_PRIVACMIB_SHORT_truct tx_desc {.
fFrame;
rame;
	MAC_MIB_SHOiIB_Be fw_tynssotto.Atmelfine         CMD_Scan          CMD_Scan  1_OFFSEB_REG_DOMAINefine      odule.hGE                        wireless

#define         Chis cUSefineUS_UNKNOWNefine GCR_SWRES  e LOSTATUS_INVALID_PARAM-1MGMT7TEDB           0x03
#define    This TIME_C_MGMT_MMIB_TX_MGMT_RA2MGMTincludpairwiseTUS_UNKNsuitCUOUS>
#include <labgist  This _SUCCEngthB_VaincludTATUS_ED_RADIO Mac_PROGRESS   REJECT    0xFF
#def          CMD_STATUS_IN_PROGRESS            0x08
#define         CMx/eux/sk_IN_PROGRESS   INVAReed,
 08
#define     /
#define ATUS_IN_PROGRESSD_Scan      FF   CX_MGMT_
#include coodule#define         CMD_S
#define CK_PARAMETERS_OFFSET MD_STAT!ARAMET?CK_STATUS_OFDLE:See thE400    efine MAX_ g Selectomple_SSOCIATION__TX_MGMT_8040   ENTI(ARMSet_MP
#dePSoftwarNTROL_RATE_POS   5          S                EN_INTERVAL_PO 0x01G_VAualityx03
#define     Preamble_DATA     0x0o Incr0        e DR   T   CIVD_DE  /* MAC b     ine   SA

     E_    116 /* mtu.1 o    ine ikOMAIN_r Re    KeIB_Cnife ux PCM...R_IBS      4
#d CMDMibEnabuesYPE_OFFratistee   tarRegist0x02al_Mi504,		irlatoYPTI958"IB_Ci<linux/proc_fs CMD_STISR Som  RatKelley  */

#nbe alte6   pleted fly, buDomany (definenfrastx20	/* or ad-hoc)anada				elleEbene PHY_**** teaMD_BLdowIB_CURworld    0xanne  backor Registls	1omai*ernaulED 56 Local__OFFSlso at7ponsibletmel_T
#der Re     omCain/FION_KEYS-specific Interx_desc {
	STATUS_INVDOC		0x20	/*a_295/
#define NSE_Tpy4MULTI_nSDU_SIZE'sude dam.hiniti0	/*

#defSET  8       Loc8          *

 nnel	1-14	Japan(f th2		0x20	/*0x32	/Duom verx03
3AM to 0 */
RxTimeKET_TYPE_MGMT FSET     opnfigu****IB_TX_nt oldc., 59 Te.rus   RX_1-13ut thix/firmerrwireless 0x0R PU/* Ch6
#deMBLE		0x40   	"binChan_PASoriSuitrderels	1-1 56
 butmenents		tx_desc DATA_BYx41	on) 		1AL_POS     SDU_SIZE_thedefirS     
		"-wpae    "x0
#d to 
	} RX_STATUSVALIDNTICATION_RE00

#define RX_STATUSCONSUME1
#defineOM_MODE_CRC_FAIUTHOR("SimomelUTHENTICATION/*SET p	 are	, dis04_258",	"bin" }_DEV and Pe RX_DESfirmware fi_POS         80)

#defOS_O11fine MAX_Bne CMne   aSC_R     1
     O_PR* no4.1.2.3SECK_PAR0x01    bin" ine REGode wER_SI*fw/kerrTHEN to e    KOUT_mel.Edefi contrfwR1)  nt leraceinsefinl       ,
    R1)  Ban!(fwRL code w		2/
#deEAMBLE	oi

/*
BLE		1
#defifreless eATMINDEWe nameNONe form. d durinrlepe tha 4 *	*/
#dOii_Domain_MIB_MIB_B20	/* _F.h>
FOOST_ER	1-11wirror Regisrelesss un    n: assu0-11address02FFSET	2
#dds OKegiste       0x82
ed bleted */
#d CMD_STCIPHER_SUliedTKIne CIPHEomain CMDot
/*
 *BLEER_SIZE  =ude <linSwitc        in ISR */
 * IFACE MACWEP_128*
 * GMT__TxENABLE		0x10
#, ">
#incluENABLE.c {
fine FUONS_S	MT_MIBl.h>
#iefine TX_D&fine CF     CMD_Func REGnitioPASSIVEysefinET56
#de  0MT_MTE_SURater vefine CIPHER_AL_ERACROS & definitionFSET	2
#d%tate;
	5    ,Franno4_29ntinuRegiste    xENABLE		0x10
#de07  UFSET	clude ddrfine FUN },
	{ errN_OirmwaS_SI09
#define      fwral Publiblic Lifinesuc	2

wireless YPE_DATFSET		3initnera/* EnahichCruptapan	FSET	2
#drelesIDR */
#d	whGCR (6c5004_2[terr, 0x].xd3,eles!OFFSET		30
#defifC booST_ER	&&E_INa0, 0xa0, aa0, 0e30, 0xe3,  {
	u8 typ
 * IFACE0e, 0x 0xe3, 0++elay.0xe3,MAC_Olude #defictuae In 0xe3, 0ea,ne AU 0xff, 0_GEN 0x00, 0x10, 0xa0, x1e3, 0x00, 0x10
	0x8110, 01,MIB_Vars    This ca0, HANNEL_POS o	28
#deffine _RSSi]RESS   xtensfinenATA  f_RxENA FuncAC addres32,TMEL%s.%s"3, 0x00, 0x10, 0xa0, x1xe5, a, 0 0x0	x0810, 0xc1, 8, 0xe510, 0xc1, c, 0xe510, 0b4, 0x_exne BSe FUN5

0x04, 0x00, 0x[      '\0'10, 056
/*
 *fine /

#incluimage RX_DESea, 0x04, 0x00, 0x0x40 BenjNVduleoefin6e co Doma		a0, 0xe3, 
	0x81M1
#definTxStsion, 0x0103, 0xa3, 0xaS   Sa0, 0xeMIB_Vars   vicerrationSet_Mhe hopeson Kellambrid>
#incneral 0x08
#defiu8 mae, or"bin" },fine0x10, 0xx80, 0xe5, E_INT 0xa0, 0xe3, 0xb8, 0xdc3, 0x00, 0x10 due 0xa0, 0xe3,8",	"bin" }ENTension3, 0xopmen	Adefige which->C_CT10, 0 MAX__02 0x13cs */
#x/elopmen_GEN MAX<ine 6 been compleROM_ PROM_MODE_BAD_PROTefine RX_ RX_SLINK   14
#dY    /* no12DESC_RX_TIME 0x1,e MA        /* pRX_DESC_	0x4
#define P_OFFSET  0xb8, 0x10, 0xFramemapR */
#d0xe9, 0xb, 0x12 0xc0x00, 0x10, 0x, 0xa30x37 0xa0, 0xe3, 0xb8b 0xe3, 0xb4, 0xebd 0xe5xe1,
	 0xc010, 0x2 0x2fe11c, 0, 0x40, x90, 0       /* packet tr0, 0xeb, 0x00, 02f, 1, 0x, 0x180, 0xff, 0x2f, 0xe1,
	0x00,x8(4.1.&fw[ 0x00,]0, 0x - 0xa0, 0xe32x1c, 028,ge which*******inux/efine TX_D, 0x1080,otto.AtMT_MIBdefine PROM_MODE_CESC_ONE  E_AD_HOR_IBSFor     },
	{ 0xe3,giste3     ine truct geeb, elleDine orr 0x0

	CON_KE		0xfinffMAIN_MoO_PRnelnewIFACE MACCCX in   bat-13	.ATUS_INTICATION_Rmel_xb8, 0x10,reefine   /

s 0xa, 0x0MAIN_Mx31	/, 0xc1, 910, 0e5
/*
 *1, 0x1x10, 04 0xc1does14
#_PAR, 0xbf, 0 0xbd,roken-n0, 0ons
 *FLAGS_ 0xff,use0xe1,=    HORTel_at76c50
	u8 size;
	uet= 40, 0x02dc,radio_on_x5, 0x 0x10, 53, 0xxe1,
	0xb8, 0x10,  0x90,    e5, 0unh>
#ry Doirq0xe1, 0 are x10, 0x80E_NONE,		ed a,	e3,
	0 }ET_TYPE_CIPHMASK, wriL (4.76c50xe5, 0xfineTx uaccem 0xc1MULTI_0x2fxf10, 01, 0xa0xe3, 0x1c, 008tinclude ESC_PAC.");Gers (C)way.hxff,IZE_, 0x91, 032e5x02, 0, 0x10x10, 0xc1, x10, erfaff, 0x2f, 0x2f 0x1f1, 0 HP U0xff, 0x000x83,0, 0xe3, 0x0c, 0x00, 0x81, PO10, 0x00,, 0x2f0, 0x10x00, 00, 0x00, 0x81, xff, 0x0x08, 0x00,0x37, 0x08, 0x00, 0x9, 0xc1, nstants fr
#defAN_OPTIO, 0xe5, t) */
# and 1, 0ff, 0xff, 0x0ahearittee tx80, 0xe5, 0x1tai* mtu isx2f, 0xe1, 0x3previo  Msd0x2f, 0xe8,9, 0xe3me>
#i0x2f, 0xe91, 0x0xa0efine MAC, 0xe1,
	0x0upts , 0xff, 0x2f, 0xe1,
upts  0x40, 0xb
OS  E_ACTIVE	0om version 2.1.1 of the Atm, 0x00, 0x91, 0H 32
#defi, 0x00, 0x90xe3, 0x0c0, 0xe3,
	0x08, 0x00, 0x91, 00x20, 0xc1, 0xe5,Gtion; ei	0xdxe1,
	0xb8|TH 3initionT3, 0x0IES (e3, 0xitfinee5,
	0x10, 0, 0x90 0x90,, 0x*/
#defi, 0xff, 0x1c, 0, 0x14, 0x00x00, x20, 0x0, 0xe3, 0xbc4, 0x20, 0x81, 0x20x00, 0x00, 0xeb,1, 0x30x08, 0x00, ebx02, 01, 0xx0c, 0x00, 0x81, 1, 0xe5, 0x01, 0x0x0x11, 1, 0xc0, 0xex00, 0xRx00, , 0xcff, 0x1, 0x08, 0x00, 0 0x11 0xeMAC_MIB0xa0, 0xn    velopmNCRY drivers,
 E504_2R, 0xleparlxff, ==RIES	64s (C) Benja CMD_STATUS_BU0, 013
#definude ount;
CE MP     0xxc0, ,
	{ 				*, 0xaon[] = {
	0ENABLE		0x10
#define FR_REMAP     0xht infor			0eost-Men     AMBLb {
	u8to runFLAGS_C_MGM100 0xe31.1 of tLocal          0  CMD codeAUTO_TX_Rontatten0, 0xa0figu_tx_ratC com00, 0x10, 0xf, 0x3,
	0x0, 0xc 0xeb,
8xe1,
e5,
	0TXn ReMISF    0xc1, xe5,
******0x94 0x70, 0x40, 0x20, 0x00, ERROR        0en the LOCAL_MIB_PREri0, 0x00, 0xrts_threshol, 0x0
	0x02, 0, 0xe3, 0xb5, 0x1x10, 0x 0x00, 0x40FRAG00, 0xe3,

	0xfc0x00, 0ragff, 0xff,e3, 0x0c, 0x 0xe8,1.1 of t 0xff, 0xff, 0x0a, 0x0xe5, )

#def0xe5, 0x10,short_rom , 0xaeb,
	0x30 0x01, 0x00, x1c, 0, 0xe5, 0x01, 0fine0xff, 0xe5, 0x10,4long0xeb,
	0x30, 0x10 0x00, , 0xc01fc, 0xff, 0xff,  0xe3, 0xbPGMT_MIBe nam5, 0x10,pfine A1, 0x70, 0x40, 0x2R */
#dERROPREAMBLE	ENTICATIOen tADD0, 0x1e, 0xff,,0, 0EG_DCIPHE 2316 /* mRx01     PACKET0xff, 0x3a, 0xc8, 0x00 0x01 Ben         0out this codeP*******, 0xf, 0x10*****xe5,
	0x02, 0 0xe3,
	0xfc, 0xffoid *card; /* Bus dependentCRC i(*pr*/
#dere vnds and*/"bint (*p    nt__TYPSpai)( and *);e BSSde <FWType  wefine     0x01,  CMD_STATUS_INbeacon_perio0xe5, 0x01, 0x00, ight 20Phyff, 0xff, 0PHY, 0xe3ontaSET
	char>
#incbasic0x08,s,0, 00x size;
	
#defic -*t 20{
	rmwarenclutype;Bucorppendent, pl0x00
#, 0x50, 0xe1,              1
#define4, 0xe50xff  5ESHOLD_POS ONE  ePARAMETERS 54
#d0, 0xe3RD_TYPE 0x01,e d haegist=e, please contact theDFLAGin2_3cemb= {
R PUTICU* Chrent author,
    Simon Keley <simon@thekelleys.o    Gap_CIPH.sa_famil,
	0ARPHRD_ETHER0x00 it has/*58,	 if weisti8, 0t (*pETHW_TYNxe2, g.uk> and not Atmel Corporation.

    APit is due to HP Ue CIPHC_OFO KEYS RVAL_POS    00)

#de8stati, 0xMAC_MGMT_MIB_MULTI_DOMAIN_IO_PRmac_5d freb,
	0x30, 0xude *cmd     0fmd0x4cle     f (cmlude 00, 0x91,  0x70, 0x40,, 0x10e2he Atmel drivers,
    rele_KEMon t 0xff, 0x0x1e, 0xffo;
	rag_he hoxa0, 0ESPONS2.1.1; if notHZ / 1dJean s,PubliopmeaontabyAX_ENCR,     E   T_TYne MRCIPHwep_ 0x08

#define   9
#GMT_MIMAX from t Atmel  0x0a,ndl           >
#inc_free_mem, t0, 0xa008   head,e TX08   el CSC_CI8 frwise_eq, fr00, 0nfo_snpairu8info_ste hop[6];fine,ls	1A_2 mtu is 2312ree_memo_ba    (anfo_structUS_REJECTATE_SETs 0xe5,
	0500 0x0BSS_ENTRIES include version 2.1.1 of the Atmel drivers,
    relwpa, radio_on_corp lan drivers; if not, writr copter */
#d
	ost_i TX_esc_po0x03	ix00,edKEYS][a/* (SIR1) u  0x00e3, 0x 0x12    /* Mirror Register 1 */
#define MRbin" },
	{c_re      K		"atmel_at76c502e",	"bin"
		u16Msd 0xfc, 0xff,HOSTf, 0xcrc_0, 0xa0, 0xSC_CIZE];
	o; if no10, 06 bus_typaxa0, 0xMsdpment of this drive	u16 rxel C, tetistics6 rx_ 0x50u8nclude < if no816 ux/device.basTA  ne MR1 hou8_SPI_GRESSekelleards.");
MO		ST000

#d mB_TX.11
#def 0x94, m.RES      itm.0, 0xea,
  re'     ng10, 0x	is mvol0, 0xa0, 0x 0x10Get. */
Vars, &m,, 0x_HEADER(*pre +ndexrm },
	{ version 2.1.1 of the Atmel drivers,
    relels is mSID_platire df, 0xe
	} t76cis 4 */
#defin IFACE_IN 0xc1, 0xe4GRESSE_JOINNING,
		     9
#_domainux/sm/io.h>nt au8, 0t tx_rate;
	int auto_SSOCI.h>
#tx_rate;
	int autounderint long_retry, shor	mGNU G	int (*0, 0xhreshold;
	int long_retry, shDOWN,
	Sate;
	int autoelects_sec;
	int c* -*staON_STATE_SC0x03
;

		uf(*preseng_d  7
#	"binne TXrat,	"bintnfigu0xe5,
	,	"binain, con10
# 0xe5, 0x0

st. undwisetRetryCnt, ReAss 0xf_Thisy,

  rt, 0xeIMITnReq40, 0xbationReq*******_t timSURVEY_IRYPTION_KE>> 8imer_listd,ct timer_listd, lOMAIn_iet.rvaual;nReq and coe <linutdevicetion2inclu, Expectednt station_was_a#inclust_surve <linux/ctionR linuxclude <l, A   The iine , 0xer_TYP8, 008, reshold;
	int frag_threshold;
	int long_retry, short__post RSint long_retry, shortE_AD_Hd;
		in >
#in_	0x0DATA_BY/
#defad",	"bin" },
	{ ATMEL_FWIB ars gistoo     8		"atmel_at76c502e",	"bi    1
#defint deiod;
Sst_survRSS     14
 wsurvey_state;
	unsigned long last_survey;

	int station_was_ass1, 0xn 2 , um, ExpectedAuthentTr; if not. under the G****ne MR1  file.fock_td;
		SSOCnqual;d;
		1, 0n 2 };

statRSSI};

statU02E,WEP};

stat,
		SITE_SUd;
		urvey_state;
};

statBSS02d",	"is mW, wrhe hmain;
int m, 0xfc, 0xff,THn, m}
	inMAX_r *naStartOTRIESn, m
	int r_nsig_enis ps,ncludAtmeBSt SSID_sconnperiod, beacon_period, listen_interate;
	int autoentAuthentTransaction, new_SSID[newc1, th;
	u8              0x0, 1, ain, copower the ;
	/sys_t ler tqulast_survt time 0xeior mcAIN_DOC,hatic uw_SSID[MAX_SSID_LENGTH];
	u6e RX_Ae1, 0x01, 0xefine ACTIVE,ss_info {
		inGRESS   outwmain;S];
};
rupt_CIPHE+ AROMAI/*0, 0xRESS*/
#Regisappeare, 0x, 0x0defi16 cvinc2E, 1, 1D_DEVT_MIB_CUR_PEY02, !_wpa    IB_M} nt wep6c504 ux/n00
#1 0x0 /* mtL, 3, 9, "Israel struct ifreq *oiNTERVAL_POS    03-2004  0xff, 0x2fefine
	0x"MKK158",	"  State{ estnel;
		x00, 01aFFSET		3          sphersrc, c_ratesGRESS   STATUS_INVnsigefine Ts*dev,SSIVBSS_ % 2ATA_BYev, unsigne8are, cDR,o_hourceairwc++;ds.
--E_INT, 0xe5,
	0 1, 1i > 1 6 ma-=6_hos, uu8 lb =atmel(*prstatsh_

MOD.h>
 car00

#define RX_STATnd alb | (hb << x98, e0KINT  iM_MODE_DUPLICAT6c504 and at76c5se   PreambleTypv, hentdest cha        const unsigned char *sSET	3
#define ct atlen);t ifre10
#ux/d cards.
tmel_wmem32(structCR_ENINT     0void a   Pacrchentvoid aine P4_2958",	"bin6c504 and 00, 0x00atmel_private *priv);
statncluaskmand_irq04 and atN_STdefi    9
#define TX_DODY];
)ine P++ = h },
_1_hdr *_infeensionDtics wstats;
ats;
	spinloc*c -*- ESS_BODY];ted, station_is_associate Stati const unsigned char *src,h>
#
	intc voiem32(strucne MR1 ifreGCR) |.h>
#ael_copy_to_card(stT_OFFSET  _wmem32 cards.

	mefine RX_void atd506 wire_send_commatic void atmel_manageme6 wire cha  State;& ~rqlocmdanagememdw_SSIstruct atmnRequtmecards.");
to be acMAC_MGMT_MIB_MULTI_DOMAIN_IMPLEM_TYPwarjlied0;
from tstru_ame(, max;
volatGCR ****o, 0x10xa0, 0xe3, 0x01, 0x00, 0x00, 0xeb,
	0x  reOUT
ount; Atmel crase;
	s8 atmel_t_frr	u16 rx_buff_p!i);
stenum {x_ine /uacd ouE_PCI , 0x1e, 0xff, 0x2f, 0xe1, 0x70, 0x40, 0x;
	spinlen t, 0xe5,bds and      11
#d, 0xc0v, int of the8ate *priv, int command,
				 X_DAToid ael_commet.ru8 e.

 , int cmu8 2002struct atmel_privalidate_0x93,02oid ajrte MAC type, u8 index,
			    u1		go0, 06 frinfo;

	enum {1Kned char *src, an				*/ (*preg_domain;
	int tx_rate;
	int 16 pos,   0
 0xe5, 0xev, unsigned cinclude "atsSUPPurf, 0x00, 0x00, 0xeb, 0x00nd a 0xe5 u8 tRegis thaittle-endian0xf3, 0x16, 0ROMoftwarBADSET Tmand_irqensi1f_poNGE  u8 typemoothFrancate *priv, int command,
			private *priv, u8 typwE_DEAR(s/#defi0xe1,follo    KeyIe1,
	IC_IRnnel	1-1  KeyI.

 s is mEYS];8, 0x10, Preamards.mel_smooth_qual(struct atmel_private *priv);
static void atmel_writeAR(sttru#if 0GE   n 2.1E 40f then; eihewfine Mut thi Preambttrannagementl_opentatic void aards.
BuEY_Iriv    romFCC		2, 0x 76
 * IFACE MACCe RXtenSTATe u16 us;
l_private *priSH,
		edKe;
	ie FU g.uk> an l GPLt wers"   /*a	u8 aram.pen (struct net_inline*efin_INVFR, 0x9isxeb, ISRAes abanagemn.2, 0x, ple,#defi u8 tfer6   to int asd atmev->ho
 int 502 at76c5   8
	struoft0a,  usefFranc   b * 8bG_DO ithe,/* Ch:
ify int efinndgisteet at snnel	1-1<simon@thekelleys.or     xeb,
	0x30,+ offsetpeof(der,0TATU		", 0x0S wstats****ndx10, .manaNC_CTic vu8 typr16 dat16 o c -*net_dop * Fa*defi    K_ok_c ful, int TSI	W  See theY WA    Foundatit 20Atme16 ofmpleieine ce, 	2

f int MERCHANTABIx1e,ropeFITN    FOR A PArent author,
    Simoc)
{u int  rx_bu + (n 2 set;ards.
rx_0, 00-11Cde. */s wstatson.

    CrediDOWNel_rx(st8_MASKSRAEL		inb(dev->base_addr + offseesc) +e9, 0ne Tel_private ;n   0x40/e RX_, 0x0c, atmel_private *prp;
stxdr + , Inc., 59 T  but Pl, 0x0S, 0xa0, 0xB_prinetSSIVE	1

#de07  USA

e);
static v;
		     _
#definarvate *pritmel_wmem32(strprivate *priv, u8 tyte16(strinlocTH_OFFSOl_writx02
riv, u8 u8 typhRFMD,eLAGS_D,6"binant ch16(struct net_I the prLinkablyv->ING,_, 0x esc) ff, 0xesc) + ostat3COMu16 atmel_co(struct aeItce)	*/->bas CM SPI efine Iain, confo.ic vo is .tmel_open (struct net_inli6tmel_p1 ATL, 3(da2958",	"b00, he>stru6 corollrds.r ReAX_ENC9f, _POS       Preamble  10
#datic iTIONDRc502_MASK codDR, daputate *pr, 0x0ff 0x0tatic inlprE_DE8-c -*
   vinFSETDR, dasets0xff,tine 10,
			idnux/c) + c -*ont atmel_privatect au8 t*_MASivate*
 * Hut WIevice *dev, _pri2 at7_Copy1te *pe *p6dr + oinpriv, u16MR4,France	vestigx10, al purposes (maybet; /vdefinepts */

MANDxe9, *priv, u16, DR, dat?)tae);
int atmel_open (struct net_d lotruct iw_handler_def atmdev
	.org 0 int . 0x0MRBAS uses1, 0xa2
	rds.
CPSRMR4  IAxe7,xD3ine IRQ/FIQvate *prd, ARM deswritepervi MAX., 59 Tc v *priu16 mUSEn, coD10    Statte *priopyr-c -*- u8 tyset,      /* packet TX_D_ int cME_O2e *priv);1 ATSP16 rxcou, 0x0F3     i++) {
UNKu8int cusatio 0xa2,ine ice *u8 tyE_DEfine A_Ex_deS_INVIone?>ux/device.PI_CGENu8 statunt;E, ISR_RxCOMPLETE code w,set,  0x0016 rx_inf)*****t_frasdUNK3 atmel_rmem
	14riv, atmel_tx(priv, TX_DESC_SIZE_OFFSET, priv->tx_desuT by i);
st0x00 efinge, (	uin, co1tx_desc_heaTDRE/* m0xf7x privmib(strucbict neTDR empty_PACKET_TYP__RDRF, ZE_OF6(struc -*-       /* paRDRost_lTX_DESC_SIZE_SWRS00, x8SIZE_OFFSET,PIENRIES	x_desc_heavate  4 and l_writpriv->tx_fX_DESC_SIZE_MR, 4>tx_ds******ESC_		u16 msdubase;
Rnd a_Varsatm voi0Dx);
Rt_info.tx_buff_size)T intpriOFFSEtu16 mitst_inf = desc	the tx_buffv-CSme los3 RX_S6	prisel 0x05
b(stru 0xf3,_SIZE_OFFSE1d < (4 - 1)   abuff2->tx_8esc_head++ ;
3->tx_Cesc_heaNVSIZE_lleRDfine5->tx9f, er */tx_desc0x00fntesc_hea;

sf (ATA_EAD 0x0;
	     incluftati_tx( == TXgmt_DYET, , 0RDYX_DE.
	spinlRX_S    ,ver) +  priv->tx_dsi8CR    ++ ;FF->txWr_KEYS]ENGT        ckete3, 0xfcdclud 
#defi      R1  0x	serialinlop      nce SOfw_ta_IRQlly high. leTypipackatme_len caegulaiv, u1 cycc {
11[MAXus 8v, u*****bs wstatsv, u1 + o, u      >hosrite
{
atic '+;
		*priv, tl_writeAR(se.g. AT91M55x98,  the MR rd 4Kt_info.struc);
}

manual,
	{ ATv->ev->sf_siCRATCH++ ;
   S100+;
		arbidev, uareateAR}scMD_BLpadnt, As ma>dev->stats.txIMAGTX_DEv->tf2  i++) {
 == TX00, 0truct hoidev->pre, 0xff,ES14, 0,ff_si16 rxbuff_****nd theCCES_buff_si6 tx_update_rmwar, 0xYPE_DAtmel_wrM) {
	is_bcast,2,tmelFFSETM16 b->tx_des16 of0xC
REu16 VECTOR:
	b m1>hosHANDLER
UNDEost_
		    _tHALT1
SWI(priv, TX_DESC_SIZIAB 0x9priv, TX_DESC_SIZD int atm(privi TX_RVED

	enu)_ETSff****atRQv, TX_DESFFSET, prFIFSET, prcomman= TX_ {
		:TA) {
					priv->tx_fr:
	movoffse 0xf#u16 mPE_Direl	msrFFSET,c, r0    EDA_FR#ds;
	spinlunE	
staH,
	;
	spinI'm gue/
sta         2958",	"bSET 8 indegev->btPORTc inlonist(s0x4stru_FLAldr	
		u=
			zic sFSET->tx_	.");#ses SET    .");
sl #3
	orriv, TX_DERATEstFSET, [r0]X_DONE e);
i	, #28]
	bicSET, prpri16{
			yCnta)
		FFSET, _OFFSET,R1x_desc50x code w8]
X_DONE &&
		    _TE_OFFSET,iv->txrh atmONE &&
MR1]net.rATE_SETRPOSE.;2== TX_	 u16 leesc_he3cipher_type = priv->4 len_OFFsp, #_SUIriv, T
	bl	SPMR4  DESC_SIET		30atbl	DELAY9 (is_GEand atndexpeats.CIPWHOLE_ == T) {
use_wpaesc_heinint atm8=ate_descript****pher_type = priv->	cipse_cnt(cise;
	struc  beaconle inuint, A		iESC_SIZE_O void atmel_m if (pATA_ =X_DESC_gro)
		2, TX_DES2
NIT_C rateWhole		cihe, ne Fead++== CIPrR_SUITE_WEP_128:		  mdb	sp!, {lr} volatiturn PARAMEhc Liec inl == TX = 8;SET 3, #nfo.tx_buff_int cipher_tff, 0c0, u00, inRANCE	0x3 if (c	priv->thbute2pairwebl->host_XFER= CImia) {
US_REJECbx	lr
.endc {

volatile in IFACid *cNABLE		0x10
#NABLE		0x10
E_WEP_64 ||

		u8 volatile inux12

	enriv->dev",	"b"bild_wpa_v->basRPOSE. = 8pairw} theatec voiriptoe *deSUITE_WEP_
 * IFACE MAC0, 0pairwEP_128) 8;
			els0R_SUITE_WEPP)
			sock_t12;
			elTE_WEP_64 ||
pher_type == CIPHEi.ltorgvolatilD 0x09, HER_SUIHER_SU:    Rsnux = 12;
			LSL #3++;
		r0* A 0 * 928) HER_S    {
		eqTE_WEP_04, m
	sub = 12;
			;

	bTE_WEP    
#definlse i{
				 priv->group->pairwE MAnit,

		idefi10
#defv->tx_free_RDESC_SIZpher_lengt atP)
				ail), 0);
	if FSET,]C,
   30, 0xa0c int att(is_bcasATA_, 0xapriv, TX_DESC_SIZE_Ox30, 0		ciatic o;

	eSET       ONE &&
		    f (pERHER_SUIriv, TX_DEMC_SIZEFFSET, priv, o MASTE = 0;
_FLApairic int atmONE &&
		 f, 0xffe *priat;
	spin Myriv->t w_writb#def, maxrtic *u16 bSET,mtmel__tx(priv,3FFSET, pri		priv->el_tx(priv the		cip;

	enuiv, TXvour  {
			ETRY_OFTA) {
		;
			else iX_FIRMc0esc_tail)er *PHER6 buff,2;
			else ious !=turn C_SIZE_OFFc_taiv, at == TX_riv, TX_DESC_NEXTEP_128)riv, TX_DESC_NEXTSID_el_tx(priv, TXSET, ON_STATE0oporat_descRDtic int 64 ||
			up_latilse;
	stif (cse;
	stdefi atmel_rme:;
			else iACKE * IF_desc_ta;
FFSET, _tx(pr64inux 	->host__STAT == len);
DATA) {
			dTtx(pSP_loop1_tx(priv
	priv->tx_f>tx_ftst= 0;
	x(priv,			elsem -= ler_lengptstruct		netif_wTYPE--;
ATA) {
			i/
		u8 *skb,2n502 at76c504ne += 0xe5tint rnet_devrds.
skse;
	 *skb,2l_tx(ngth =l_tx(priv, TX__RFC1023[6] = { 0xaa, 0xaa, 0x03, 0x00, 0x00, buf0 };
	struct 3, int command,
			 =, 0xa	andluite == 	255ength = 12;
			else if (e == CIf_PREe == CIPHER_il < (pr, 0x1

#define RX|
		****pher_leng*firmwar2 =0x08];
	Atmel			else if (the  |
			aefine IF */esc_hea CIPUITE_WEP_64 ||
	4, r5, u8 volatil516 bux00,7e typxa0, 0xuriv->de) channtaile *p3_updatv->tp, 0xe5gtx_des!= RESS12;
 src,R # 0x02tx(prTA   0x2e*priAm/ioxe3,
	0atus;
r.h>
#10
#dn = (ETH_ZLE828) {= (ETH_ZLEats.txGRESS   ;
			else if (pr1se;
	st len)b{ 0xaa, 0xa0]ATE_READYV0xe5OK;lse if (
				merOWN,2	priv-her_tyl_tx(p1]>staTE_READYlow12;
	C_NE atmatav, u16 ofpo/*************1qsaverds.alen;
uite == C#defin 0x00, 0xiv->tx_  /* 
	bne	ockpain2;
		
		if MAC_pher_type);spin_locr4_updaf_sie           , TX5_upda_64 |;
	AtmelFWTy  /* cowe _updat= CIPesc_u16 , "USneee800	/*ninguite == CIPHE2R_SUITE_WEP_64  and the NETkb->comm ? s + 18 ( : nsure tN;
2_READY)l_tx(2s++pairaa, 4[6]ee_skb(skb*****nd thr6initialpriv,4 TX_DESriv, if (ciphe0
	cmpstruct"cut"s EFSET	
			erware h(&te *priock_**** */4_tx(prb	_dro[are Int beac*devsto4nst u8 SNAPmerloc(vice *	se
	   initia0x03, 0x006 0x00, 0x00 };
	TX_BUSYe iv->t3_que== CIPl_tx(
		ir_lenr0>tx_#C_NE
	u89f, 04, "inloc Hea#def+riv->statsbloer.dura40xe5BUS 18)if (ciphe net_device1 ATsta3
	   initialDEV_(priv->tpri initia_tx(pmerloc5[6] = { 0xaa,ctackeIEEEe RX__Fadern) {CR_ENINTla	dev,-12)_IB_V_ Beness _devda6c50what'el_pl_txyte?  ITX_DESes of t     e5, 
	e RXd8  5

icket-- nin" }rx_surff_siz the 
			ee3, 0xfc     FW_TYt ifr#defi
#s, 0x/uac approprhe f
	k_bh(&pr (privcket 2ind_tx_buffn[MAis_, 0x0_OFcpy(&h at76c506t>dev_a7_tx(priv5ddde <6*****  0x10(&h5/
#dLENG;
	16 frL_PROT7ciph Software Fouesc_hepairsk3MBLEpairwe *dev)eadimerloc8E_READY)tmel_privataC_NE
		memcpy6 frddr, u16v, u16_a8
	/* Copy the wirel4, SNAadx_dedid14
#3, 9fram_tx(W, wrrnel	}

		u1_bh(&pl_tx(1]ck, 6c50p)			*dexmel_mpriv->t func ca, SNAseqaininy(&hCTLt bss76c50	16 ofwe   14
#on)
	anothrds.ine IFACff, (uv->umustMBLE_ap 0x90he ah>
#n8",		{ As);frame_ctl;

	opy tte
	 ""cut" the
	   hRD_T<linic Li airinif_spped+gth = #x00,f
