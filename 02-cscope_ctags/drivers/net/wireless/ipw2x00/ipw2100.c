/******************************************************************************

  Copyright(c) 2003 - 2006 Intel Corporation. All rights reserved.

  This program is free software; you can redistribute it and/or modify it
  under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 59
  Temple Place - Suite 330, Boston, MA  02111-1307, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.

  Contact Information:
  Intel Linux Wireless <ilw@linux.intel.com>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

  Portions of this file are based on the sample_* files provided by Wireless
  Extensions 0.26 package and copyright (c) 1997-2003 Jean Tourrilhes
  <jt@hpl.hp.com>

  Portions of this file are based on the Host AP project,
  Copyright (c) 2001-2002, SSH Communications Security Corp and Jouni Malinen
    <j@w1.fi>
  Copyright (c) 2002-2003, Jouni Malinen <j@w1.fi>

  Portions of ipw2100_mod_firmware_load, ipw2100_do_mod_firmware_load, and
  ipw2100_fw_load are loosely based on drivers/sound/sound_firmware.c
  available in the 2.4.25 kernel sources, and are copyright (c) Alan Cox

******************************************************************************/
/*

 Initial driver on which this is based was developed by Janusz Gorycki,
 Maciej Urbaniak, and Maciej Sosnowski.

 Promiscuous mode support added by Jacek Wysoczynski and Maciej Urbaniak.

Theory of Operation

Tx - Commands and Data

Firmware and host share a circular queue of Transmit Buffer Descriptors (TBDs)
Each TBD contains a pointer to the physical (dma_addr_t) address of data being
sent to the firmware as well as the length of the data.

The host writes to the TBD queue at the WRITE index.  The WRITE index points
to the _next_ packet to be written and is advanced when after the TBD has been
filled.

The firmware pulls from the TBD queue at the READ index.  The READ index points
to the currently being read entry, and is advanced once the firmware is
done with a packet.

When data is sent to the firmware, the first TBD is used to indicate to the
firmware if a Command or Data is being sent.  If it is Command, all of the
command information is contained within the physical address referred to by the
TBD.  If it is Data, the first TBD indicates the type of data packet, number
of fragments, etc.  The next TBD then referrs to the actual packet location.

The Tx flow cycle is as follows:

1) ipw2100_tx() is called by kernel with SKB to transmit
2) Packet is move from the tx_free_list and appended to the transmit pending
   list (tx_pend_list)
3) work is scheduled to move pending packets into the shared circular queue.
4) when placing packet in the circular queue, the incoming SKB is DMA mapped
   to a physical address.  That address is entered into a TBD.  Two TBDs are
   filled out.  The first indicating a data packet, the second referring to the
   actual payload data.
5) the packet is removed from tx_pend_list and placed on the end of the
   firmware pending list (fw_pend_list)
6) firmware is notified that the WRITE index has
7) Once the firmware has processed the TBD, INTA is triggered.
8) For each Tx interrupt received from the firmware, the READ index is checked
   to see which TBDs are done being processed.
9) For each TBD that has been processed, the ISR pulls the oldest packet
   from the fw_pend_list.
10)The packet structure contained in the fw_pend_list is then used
   to unmap the DMA address and to free the SKB originally passed to the driver
   from the kernel.
11)The packet structure is placed onto the tx_free_list

The above steps are the same for commands, only the msg_free_list/msg_pend_list
are used instead of tx_free_list/tx_pend_list

...

Critical Sections / Locking :

There are two locks utilized.  The first is the low level lock (priv->low_lock)
that protects the following:

- Access to the Tx/Rx queue lists via priv->low_lock. The lists are as follows:

  tx_free_list : Holds pre-allocated Tx buffers.
    TAIL modified in __ipw2100_tx_process()
    HEAD modified in ipw2100_tx()

  tx_pend_list : Holds used Tx buffers waiting to go into the TBD ring
    TAIL modified ipw2100_tx()
    HEAD modified by ipw2100_tx_send_data()

  msg_free_list : Holds pre-allocated Msg (Command) buffers
    TAIL modified in __ipw2100_tx_process()
    HEAD modified in ipw2100_hw_send_command()

  msg_pend_list : Holds used Msg buffers waiting to go into the TBD ring
    TAIL modified in ipw2100_hw_send_command()
    HEAD modified in ipw2100_tx_send_commands()

  The flow of data on the TX side is as follows:

  MSG_FREE_LIST + COMMAND => MSG_PEND_LIST => TBD => MSG_FREE_LIST
  TX_FREE_LIST + DATA => TX_PEND_LIST => TBD => TX_FREE_LIST

  The methods that work on the TBD ring are protected via priv->low_lock.

- The internal data state of the device itself
- Access to the firmware read/write indexes for the BD queues
  and associated logic

All external entry functions are locked with the priv->action_lock to ensure
that only one external action is invoked at a time.


*/

#include <linux/compiler.h>
#include <linux/errno.h>
#include <linux/if_arp.h>
#include <linux/in6.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/kernel.h>
#include <linux/kmod.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/ethtool.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/proc_fs.h>
#include <linux/skbuff.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/unistd.h>
#include <linux/stringify.h>
#include <linux/tcp.h>
#include <linux/types.h>
#include <linux/time.h>
#include <linux/firmware.h>
#include <linux/acpi.h>
#include <linux/ctype.h>
#include <linux/pm_qos_params.h>

#include <net/lib80211.h>

#include "ipw2100.h"

#define IPW2100_VERSION "git-1.2.2"

#define DRV_NAME	"ipw2100"
#define DRV_VERSION	IPW2100_VERSION
#define DRV_DESCRIPTION	"Intel(R) PRO/Wireless 2100 Network Driver"
#define DRV_COPYRIGHT	"Copyright(c) 2003-2006 Intel Corporation"

/* Debugging stuff */
#ifdef CONFIG_IPW2100_DEBUG
#define IPW2100_RX_DEBUG	/* Reception debugging */
#endif

MODULE_DESCRIPTION(DRV_DESCRIPTION);
MODULE_VERSION(DRV_VERSION);
MODULE_AUTHOR(DRV_COPYRIGHT);
MODULE_LICENSE("GPL");

static int debug = 0;
static int network_mode = 0;
static int channel = 0;
static int associate = 0;
static int disable = 0;
#ifdef CONFIG_PM
static struct ipw2100_fw ipw2100_firmware;
#endif

#include <linux/moduleparam.h>
module_param(debug, int, 0444);
module_param_named(mode, network_mode, int, 0444);
module_param(channel, int, 0444);
module_param(associate, int, 0444);
module_param(disable, int, 0444);

MODULE_PARM_DESC(debug, "debug level");
MODULE_PARM_DESC(mode, "network mode (0=BSS,1=IBSS,2=Monitor)");
MODULE_PARM_DESC(channel, "channel");
MODULE_PARM_DESC(associate, "auto associate when scanning (default off)");
MODULE_PARM_DESC(disable, "manually disable the radio (default 0 [radio on])");

static u32 ipw2100_debug_level = IPW_DL_NONE;

#ifdef CONFIG_IPW2100_DEBUG
#define IPW_DEBUG(level, message...) \
do { \
	if (ipw2100_debug_level & (level)) { \
		printk(KERN_DEBUG "ipw2100: %c %s ", \
                       in_interrupt() ? 'I' : 'U',  __func__); \
		printk(message); \
	} \
} while (0)
#else
#define IPW_DEBUG(level, message...) do {} while (0)
#endif				/* CONFIG_IPW2100_DEBUG */

#ifdef CONFIG_IPW2100_DEBUG
static const char *command_types[] = {
	"undefined",
	"unused",		/* HOST_ATTENTION */
	"HOST_COMPLETE",
	"unused",		/* SLEEP */
	"unused",		/* HOST_POWER_DOWN */
	"unused",
	"SYSTEM_CONFIG",
	"unused",		/* SET_IMR */
	"SSID",
	"MANDATORY_BSSID",
	"AUTHENTICATION_TYPE",
	"ADAPTER_ADDRESS",
	"PORT_TYPE",
	"INTERNATIONAL_MODE",
	"CHANNEL",
	"RTS_THRESHOLD",
	"FRAG_THRESHOLD",
	"POWER_MODE",
	"TX_RATES",
	"BASIC_TX_RATES",
	"WEP_KEY_INFO",
	"unused",
	"unused",
	"unused",
	"unused",
	"WEP_KEY_INDEX",
	"WEP_FLAGS",
	"ADD_MULTICAST",
	"CLEAR_ALL_MULTICAST",
	"BEACON_INTERVAL",
	"ATIM_WINDOW",
	"CLEAR_STATISTICS",
	"undefined",
	"undefined",
	"undefined",
	"undefined",
	"TX_POWER_INDEX",
	"undefined",
	"undefined",
	"undefined",
	"undefined",
	"undefined",
	"undefined",
	"BROADCAST_SCAN",
	"CARD_DISABLE",
	"PREFERRED_BSSID",
	"SET_SCAN_OPTIONS",
	"SCAN_DWELL_TIME",
	"SWEEP_TABLE",
	"AP_OR_STATION_TABLE",
	"GROUP_ORDINALS",
	"SHORT_RETRY_LIMIT",
	"LONG_RETRY_LIMIT",
	"unused",		/* SAVE_CALIBRATION */
	"unused",		/* RESTORE_CALIBRATION */
	"undefined",
	"undefined",
	"undefined",
	"HOST_PRE_POWER_DOWN",
	"unused",		/* HOST_INTERRUPT_COALESCING */
	"undefined",
	"CARD_DISABLE_PHY_OFF",
	"MSDU_TX_RATES" "undefined",
	"undefined",
	"SET_STATION_STAT_BITS",
	"CLEAR_STATIONS_STAT_BITS",
	"LEAP_ROGUE_MODE",
	"SET_SECURITY_INFORMATION",
	"DISASSOCIATION_BSSID",
	"SET_WPA_ASS_IE"
};
#endif

/* Pre-decl until we get the code solid and then we can clean it up */
static void ipw2100_tx_send_commands(struct ipw2100_priv *priv);
static void ipw2100_tx_send_data(struct ipw2100_priv *priv);
static int ipw2100_adapter_setup(struct ipw2100_priv *priv);

static void ipw2100_queues_initialize(struct ipw2100_priv *priv);
static void ipw2100_queues_free(struct ipw2100_priv *priv);
static int ipw2100_queues_allocate(struct ipw2100_priv *priv);

static int ipw2100_fw_download(struct ipw2100_priv *priv,
			       struct ipw2100_fw *fw);
static int ipw2100_get_firmware(struct ipw2100_priv *priv,
				struct ipw2100_fw *fw);
static int ipw2100_get_fwversion(struct ipw2100_priv *priv, char *buf,
				 size_t max);
static int ipw2100_get_ucodeversion(struct ipw2100_priv *priv, char *buf,
				    size_t max);
static void ipw2100_release_firmware(struct ipw2100_priv *priv,
				     struct ipw2100_fw *fw);
static int ipw2100_ucode_download(struct ipw2100_priv *priv,
				  struct ipw2100_fw *fw);
static void ipw2100_wx_event_work(struct work_struct *work);
static struct iw_statistics *ipw2100_wx_wireless_stats(struct net_device *dev);
static struct iw_handler_def ipw2100_wx_handler_def;

static inline void read_register(struct net_device *dev, u32 reg, u32 * val)
{
	*val = readl((void __iomem *)(dev->base_addr + reg));
	IPW_DEBUG_IO("r: 0x%08X => 0x%08X\n", reg, *val);
}

static inline void write_register(struct net_device *dev, u32 reg, u32 val)
{
	writel(val, (void __iomem *)(dev->base_addr + reg));
	IPW_DEBUG_IO("w: 0x%08X <= 0x%08X\n", reg, val);
}

static inline void read_register_word(struct net_device *dev, u32 reg,
				      u16 * val)
{
	*val = readw((void __iomem *)(dev->base_addr + reg));
	IPW_DEBUG_IO("r: 0x%08X => %04X\n", reg, *val);
}

static inline void read_register_byte(struct net_device *dev, u32 reg, u8 * val)
{
	*val = readb((void __iomem *)(dev->base_addr + reg));
	IPW_DEBUG_IO("r: 0x%08X => %02X\n", reg, *val);
}

static inline void write_register_word(struct net_device *dev, u32 reg, u16 val)
{
	writew(val, (void __iomem *)(dev->base_addr + reg));
	IPW_DEBUG_IO("w: 0x%08X <= %04X\n", reg, val);
}

static inline void write_register_byte(struct net_device *dev, u32 reg, u8 val)
{
	writeb(val, (void __iomem *)(dev->base_addr + reg));
	IPW_DEBUG_IO("w: 0x%08X =< %02X\n", reg, val);
}

static inline void read_nic_dword(struct net_device *dev, u32 addr, u32 * val)
{
	write_register(dev, IPW_REG_INDIRECT_ACCESS_ADDRESS,
		       addr & IPW_REG_INDIRECT_ADDR_MASK);
	read_register(dev, IPW_REG_INDIRECT_ACCESS_DATA, val);
}

static inline void write_nic_dword(struct net_device *dev, u32 addr, u32 val)
{
	write_register(dev, IPW_REG_INDIRECT_ACCESS_ADDRESS,
		       addr & IPW_REG_INDIRECT_ADDR_MASK);
	write_register(dev, IPW_REG_INDIRECT_ACCESS_DATA, val);
}

static inline void read_nic_word(struct net_device *dev, u32 addr, u16 * val)
{
	write_register(dev, IPW_REG_INDIRECT_ACCESS_ADDRESS,
		       addr & IPW_REG_INDIRECT_ADDR_MASK);
	read_register_word(dev, IPW_REG_INDIRECT_ACCESS_DATA, val);
}

static inline void write_nic_word(struct net_device *dev, u32 addr, u16 val)
{
	write_register(dev, IPW_REG_INDIRECT_ACCESS_ADDRESS,
		       addr & IPW_REG_INDIRECT_ADDR_MASK);
	write_register_word(dev, IPW_REG_INDIRECT_ACCESS_DATA, val);
}

static inline void read_nic_byte(struct net_device *dev, u32 addr, u8 * val)
{
	write_register(dev, IPW_REG_INDIRECT_ACCESS_ADDRESS,
		       addr & IPW_REG_INDIRECT_ADDR_MASK);
	read_register_byte(dev, IPW_REG_INDIRECT_ACCESS_DATA, val);
}

static inline void write_nic_byte(struct net_device *dev, u32 addr, u8 val)
{
	write_register(dev, IPW_REG_INDIRECT_ACCESS_ADDRESS,
		       addr & IPW_REG_INDIRECT_ADDR_MASK);
	write_register_byte(dev, IPW_REG_INDIRECT_ACCESS_DATA, val);
}

static inline void write_nic_auto_inc_address(struct net_device *dev, u32 addr)
{
	write_register(dev, IPW_REG_AUTOINCREMENT_ADDRESS,
		       addr & IPW_REG_INDIRECT_ADDR_MASK);
}

static inline void write_nic_dword_auto_inc(struct net_device *dev, u32 val)
{
	write_register(dev, IPW_REG_AUTOINCREMENT_DATA, val);
}

static void write_nic_memory(struct net_device *dev, u32 addr, u32 len,
				    const u8 * buf)
{
	u32 aligned_addr;
	u32 aligned_len;
	u32 dif_len;
	u32 i;

	/* read first nibble byte by byte */
	aligned_addr = addr & (~0x3);
	dif_len = addr - aligned_addr;
	if (dif_len) {
		/* Start reading at aligned_addr + dif_len */
		write_register(dev, IPW_REG_INDIRECT_ACCESS_ADDRESS,
			       aligned_addr);
		for (i = dif_len; i < 4; i++, buf++)
			write_register_byte(dev,
					    IPW_REG_INDIRECT_ACCESS_DATA + i,
					    *buf);

		len -= dif_len;
		aligned_addr += 4;
	}

	/* read DWs through autoincrement registers */
	write_register(dev, IPW_REG_AUTOINCREMENT_ADDRESS, aligned_addr);
	aligned_len = len & (~0x3);
	for (i = 0; i < aligned_len; i += 4, buf += 4, aligned_addr += 4)
		write_register(dev, IPW_REG_AUTOINCREMENT_DATA, *(u32 *) buf);

	/* copy the last nibble */
	dif_len = len - aligned_len;
	write_register(dev, IPW_REG_INDIRECT_ACCESS_ADDRESS, aligned_addr);
	for (i = 0; i < dif_len; i++, buf++)
		write_register_byte(dev, IPW_REG_INDIRECT_ACCESS_DATA + i,
				    *buf);
}

static void read_nic_memory(struct net_device *dev, u32 addr, u32 len,
				   u8 * buf)
{
	u32 aligned_addr;
	u32 aligned_len;
	u32 dif_len;
	u32 i;

	/* read first nibble byte by byte */
	aligned_addr = addr & (~0x3);
	dif_len = addr - aligned_addr;
	if (dif_len) {
		/* Start reading at aligned_addr + dif_len */
		write_register(dev, IPW_REG_INDIRECT_ACCESS_ADDRESS,
			       aligned_addr);
		for (i = dif_len; i < 4; i++, buf++)
			read_register_byte(dev,
					   IPW_REG_INDIRECT_ACCESS_DATA + i,
					   buf);

		len -= dif_len;
		aligned_addr += 4;
	}

	/* read DWs through autoincrement registers */
	write_register(dev, IPW_REG_AUTOINCREMENT_ADDRESS, aligned_addr);
	aligned_len = len & (~0x3);
	for (i = 0; i < aligned_len; i += 4, buf += 4, aligned_addr += 4)
		read_register(dev, IPW_REG_AUTOINCREMENT_DATA, (u32 *) buf);

	/* copy the last nibble */
	dif_len = len - aligned_len;
	write_register(dev, IPW_REG_INDIRECT_ACCESS_ADDRESS, aligned_addr);
	for (i = 0; i < dif_len; i++, buf++)
		read_register_byte(dev, IPW_REG_INDIRECT_ACCESS_DATA + i, buf);
}

static inline int ipw2100_hw_is_adapter_in_system(struct net_device *dev)
{
	return (dev->base_addr &&
		(readl
		 ((void __iomem *)(dev->base_addr +
				   IPW_REG_DOA_DEBUG_AREA_START))
		 == IPW_DATA_DOA_DEBUG_VALUE));
}

static int ipw2100_get_ordinal(struct ipw2100_priv *priv, u32 ord,
			       void *val, u32 * len)
{
	struct ipw2100_ordinals *ordinals = &priv->ordinals;
	u32 addr;
	u32 field_info;
	u16 field_len;
	u16 field_count;
	u32 total_length;

	if (ordinals->table1_addr == 0) {
		printk(KERN_WARNING DRV_NAME ": attempt to use fw ordinals "
		       "before they have been loaded.\n");
		return -EINVAL;
	}

	if (IS_ORDINAL_TABLE_ONE(ordinals, ord)) {
		if (*len < IPW_ORD_TAB_1_ENTRY_SIZE) {
			*len = IPW_ORD_TAB_1_ENTRY_SIZE;

			printk(KERN_WARNING DRV_NAME
			       ": ordinal buffer length too small, need %zd\n",
			       IPW_ORD_TAB_1_ENTRY_SIZE);

			return -EINVAL;
		}

		read_nic_dword(priv->net_dev,
			       ordinals->table1_addr + (ord << 2), &addr);
		read_nic_dword(priv->net_dev, addr, val);

		*len = IPW_ORD_TAB_1_ENTRY_SIZE;

		return 0;
	}

	if (IS_ORDINAL_TABLE_TWO(ordinals, ord)) {

		ord -= IPW_START_ORD_TAB_2;

		/* get the address of statistic */
		read_nic_dword(priv->net_dev,
			       ordinals->table2_addr + (ord << 3), &addr);

		/* get the second DW of statistics ;
		 * two 16-bit words - first is length, second is count */
		read_nic_dword(priv->net_dev,
			       ordinals->table2_addr + (ord << 3) + sizeof(u32),
			       &field_info);

		/* get each entry length */
		field_len = *((u16 *) & field_info);

		/* get number of entries */
		field_count = *(((u16 *) & field_info) + 1);

		/* abort if no enought memory */
		total_length = field_len * field_count;
		if (total_length > *len) {
			*len = total_length;
			return -EINVAL;
		}

		*len = total_length;
		if (!total_length)
			return 0;

		/* read the ordinal data from the SRAM */
		read_nic_memory(priv->net_dev, addr, total_length, val);

		return 0;
	}

	printk(KERN_WARNING DRV_NAME ": ordinal %d neither in table 1 nor "
	       "in table 2\n", ord);

	return -EINVAL;
}

static int ipw2100_set_ordinal(struct ipw2100_priv *priv, u32 ord, u32 * val,
			       u32 * len)
{
	struct ipw2100_ordinals *ordinals = &priv->ordinals;
	u32 addr;

	if (IS_ORDINAL_TABLE_ONE(ordinals, ord)) {
		if (*len != IPW_ORD_TAB_1_ENTRY_SIZE) {
			*len = IPW_ORD_TAB_1_ENTRY_SIZE;
			IPW_DEBUG_INFO("wrong size\n");
			return -EINVAL;
		}

		read_nic_dword(priv->net_dev,
			       ordinals->table1_addr + (ord << 2), &addr);

		write_nic_dword(priv->net_dev, addr, *val);

		*len = IPW_ORD_TAB_1_ENTRY_SIZE;

		return 0;
	}

	IPW_DEBUG_INFO("wrong table\n");
	if (IS_ORDINAL_TABLE_TWO(ordinals, ord))
		return -EINVAL;

	return -EINVAL;
}

static char *snprint_line(char *buf, size_t count,
			  const u8 * data, u32 len, u32 ofs)
{
	int out, i, j, l;
	char c;

	out = snprintf(buf, count, "%08X", ofs);

	for (l = 0, i = 0; i < 2; i++) {
		out += snprintf(buf + out, count - out, " ");
		for (j = 0; j < 8 && l < len; j++, l++)
			out += snprintf(buf + out, count - out, "%02X ",
					data[(i * 8 + j)]);
		for (; j < 8; j++)
			out += snprintf(buf + out, count - out, "   ");
	}

	out += snprintf(buf + out, count - out, " ");
	for (l = 0, i = 0; i < 2; i++) {
		out += snprintf(buf + out, count - out, " ");
		for (j = 0; j < 8 && l < len; j++, l++) {
			c = data[(i * 8 + j)];
			if (!isascii(c) || !isprint(c))
				c = '.';

			out += snprintf(buf + out, count - out, "%c", c);
		}

		for (; j < 8; j++)
			out += snprintf(buf + out, count - out, " ");
	}

	return buf;
}

static void printk_buf(int level, const u8 * data, u32 len)
{
	char line[81];
	u32 ofs = 0;
	if (!(ipw2100_debug_level & level))
		return;

	while (len) {
		printk(KERN_DEBUG "%s\n",
		       snprint_line(line, sizeof(line), &data[ofs],
				    min(len, 16U), ofs));
		ofs += 16;
		len -= min(len, 16U);
	}
}

#define MAX_RESET_BACKOFF 10

static void schedule_reset(struct ipw2100_priv *priv)
{
	unsigned long now = get_seconds();

	/* If we haven't received a reset request within the backoff period,
	 * then we can reset the backoff interval so this reset occurs
	 * immediately */
	if (priv->reset_backoff &&
	    (now - priv->last_reset > priv->reset_backoff))
		priv->reset_backoff = 0;

	priv->last_reset = get_seconds();

	if (!(priv->status & STATUS_RESET_PENDING)) {
		IPW_DEBUG_INFO("%s: Scheduling firmware restart (%ds).\n",
			       priv->net_dev->name, priv->reset_backoff);
		netif_carrier_off(priv->net_dev);
		netif_stop_queue(priv->net_dev);
		priv->status |= STATUS_RESET_PENDING;
		if (priv->reset_backoff)
			queue_delayed_work(priv->workqueue, &priv->reset_work,
					   priv->reset_backoff * HZ);
		else
			queue_delayed_work(priv->workqueue, &priv->reset_work,
					   0);

		if (priv->reset_backoff < MAX_RESET_BACKOFF)
			priv->reset_backoff++;

		wake_up_interruptible(&priv->wait_command_queue);
	} else
		IPW_DEBUG_INFO("%s: Firmware restart already in progress.\n",
			       priv->net_dev->name);

}

#define HOST_COMPLETE_TIMEOUT (2 * HZ)
static int ipw2100_hw_send_command(struct ipw2100_priv *priv,
				   struct host_command *cmd)
{
	struct list_head *element;
	struct ipw2100_tx_packet *packet;
	unsigned long flags;
	int err = 0;

	IPW_DEBUG_HC("Sending %s command (#%d), %d bytes\n",
		     command_types[cmd->host_command], cmd->host_command,
		     cmd->host_command_length);
	printk_buf(IPW_DL_HC, (u8 *) cmd->host_command_parameters,
		   cmd->host_command_length);

	spin_lock_irqsave(&priv->low_lock, flags);

	if (priv->fatal_error) {
		IPW_DEBUG_INFO
		    ("Attempt to send command while hardware in fatal error condition.\n");
		err = -EIO;
		goto fail_unlock;
	}

	if (!(priv->status & STATUS_RUNNING)) {
		IPW_DEBUG_INFO
		    ("Attempt to send command while hardware is not running.\n");
		err = -EIO;
		goto fail_unlock;
	}

	if (priv->status & STATUS_CMD_ACTIVE) {
		IPW_DEBUG_INFO
		    ("Attempt to send command while another command is pending.\n");
		err = -EBUSY;
		goto fail_unlock;
	}

	if (list_empty(&priv->msg_free_list)) {
		IPW_DEBUG_INFO("no available msg buffers\n");
		goto fail_unlock;
	}

	priv->status |= STATUS_CMD_ACTIVE;
	priv->messages_sent++;

	element = priv->msg_free_list.next;

	packet = list_entry(element, struct ipw2100_tx_packet, list);
	packet->jiffy_start = jiffies;

	/* initialize the firmware command packet */
	packet->info.c_struct.cmd->host_command_reg = cmd->host_command;
	packet->info.c_struct.cmd->host_command_reg1 = cmd->host_command1;
	packet->info.c_struct.cmd->host_command_len_reg =
	    cmd->host_command_length;
	packet->info.c_struct.cmd->sequence = cmd->host_command_sequence;

	memcpy(packet->info.c_struct.cmd->host_command_params_reg,
	       cmd->host_command_parameters,
	       sizeof(packet->info.c_struct.cmd->host_command_params_reg));

	list_del(element);
	DEC_STAT(&priv->msg_free_stat);

	list_add_tail(element, &priv->msg_pend_list);
	INC_STAT(&priv->msg_pend_stat);

	ipw2100_tx_send_commands(priv);
	ipw2100_tx_send_data(priv);

	spin_unlock_irqrestore(&priv->low_lock, flags);

	/*
	 * We must wait for this command to complete before another
	 * command can be sent...  but if we wait more than 3 seconds
	 * then there is a problem.
	 */

	err =
	    wait_event_interruptible_timeout(priv->wait_command_queue,
					     !(priv->
					       status & STATUS_CMD_ACTIVE),
					     HOST_COMPLETE_TIMEOUT);

	if (err == 0) {
		IPW_DEBUG_INFO("Command completion failed out after %dms.\n",
			       1000 * (HOST_COMPLETE_TIMEOUT / HZ));
		priv->fatal_error = IPW2100_ERR_MSG_TIMEOUT;
		priv->status &= ~STATUS_CMD_ACTIVE;
		schedule_reset(priv);
		return -EIO;
	}

	if (priv->fatal_error) {
		printk(KERN_WARNING DRV_NAME ": %s: firmware fatal error\n",
		       priv->net_dev->name);
		return -EIO;
	}

	/* !!!!! HACK TEST !!!!!
	 * When lots of debug trace statements are enabled, the driver
	 * doesn't seem to have as many firmware restart cycles...
	 *
	 * As a test, we're sticking in a 1/100s delay here */
	schedule_timeout_uninterruptible(msecs_to_jiffies(10));

	return 0;

      fail_unlock:
	spin_unlock_irqrestore(&priv->low_lock, flags);

	return err;
}

/*
 * Verify the values and data access of the hardware
 * No locks needed or used.  No functions called.
 */
static int ipw2100_verify(struct ipw2100_priv *priv)
{
	u32 data1, data2;
	u32 address;

	u32 val1 = 0x76543210;
	u32 val2 = 0xFEDCBA98;

	/* Domain 0 check - all values should be DOA_DEBUG */
	for (address = IPW_REG_DOA_DEBUG_AREA_START;
	     address < IPW_REG_DOA_DEBUG_AREA_END; address += sizeof(u32)) {
		read_register(priv->net_dev, address, &data1);
		if (data1 != IPW_DATA_DOA_DEBUG_VALUE)
			return -EIO;
	}

	/* Domain 1 check - use arbitrary read/write compare  */
	for (address = 0; address < 5; address++) {
		/* The memory area is not used now */
		write_register(priv->net_dev, IPW_REG_DOMAIN_1_OFFSET + 0x32,
			       val1);
		write_register(priv->net_dev, IPW_REG_DOMAIN_1_OFFSET + 0x36,
			       val2);
		read_register(priv->net_dev, IPW_REG_DOMAIN_1_OFFSET + 0x32,
			      &data1);
		read_register(priv->net_dev, IPW_REG_DOMAIN_1_OFFSET + 0x36,
			      &data2);
		if (val1 == data1 && val2 == data2)
			return 0;
	}

	return -EIO;
}

/*
 *
 * Loop until the CARD_DISABLED bit is the same value as the
 * supplied parameter
 *
 * TODO: See if it would be more efficient to do a wait/wake
 *       cycle and have the completion event trigger the wakeup
 *
 */
#define IPW_CARD_DISABLE_COMPLETE_WAIT		    100	// 100 milli
static int ipw2100_wait_for_card_state(struct ipw2100_priv *priv, int state)
{
	int i;
	u32 card_state;
	u32 len = sizeof(card_state);
	int err;

	for (i = 0; i <= IPW_CARD_DISABLE_COMPLETE_WAIT * 1000; i += 50) {
		err = ipw2100_get_ordinal(priv, IPW_ORD_CARD_DISABLED,
					  &card_state, &len);
		if (err) {
			IPW_DEBUG_INFO("Query of CARD_DISABLED ordinal "
				       "failed.\n");
			return 0;
		}

		/* We'll break out if either the HW state says it is
		 * in the state we want, or if HOST_COMPLETE command
		 * finishes */
		if ((card_state == state) ||
		    ((priv->status & STATUS_ENABLED) ?
		     IPW_HW_STATE_ENABLED : IPW_HW_STATE_DISABLED) == state) {
			if (state == IPW_HW_STATE_ENABLED)
				priv->status |= STATUS_ENABLED;
			else
				priv->status &= ~STATUS_ENABLED;

			return 0;
		}

		udelay(50);
	}

	IPW_DEBUG_INFO("ipw2100_wait_for_card_state to %s state timed out\n",
		       state ? "DISABLED" : "ENABLED");
	return -EIO;
}

/*********************************************************************
    Procedure   :   sw_reset_and_clock
    Purpose     :   Asserts s/w reset, asserts clock initialization
                    and waits for clock stabilization
 ********************************************************************/
static int sw_reset_and_clock(struct ipw2100_priv *priv)
{
	int i;
	u32 r;

	// assert s/w reset
	write_register(priv->net_dev, IPW_REG_RESET_REG,
		       IPW_AUX_HOST_RESET_REG_SW_RESET);

	// wait for clock stabilization
	for (i = 0; i < 1000; i++) {
		udelay(IPW_WAIT_RESET_ARC_COMPLETE_DELAY);

		// check clock ready bit
		read_register(priv->net_dev, IPW_REG_RESET_REG, &r);
		if (r & IPW_AUX_HOST_RESET_REG_PRINCETON_RESET)
			break;
	}

	if (i == 1000)
		return -EIO;	// TODO: better error value

	/* set "initialization complete" bit to move adapter to
	 * D0 state */
	write_register(priv->net_dev, IPW_REG_GP_CNTRL,
		       IPW_AUX_HOST_GP_CNTRL_BIT_INIT_DONE);

	/* wait for clock stabilization */
	for (i = 0; i < 10000; i++) {
		udelay(IPW_WAIT_CLOCK_STABILIZATION_DELAY * 4);

		/* check clock ready bit */
		read_register(priv->net_dev, IPW_REG_GP_CNTRL, &r);
		if (r & IPW_AUX_HOST_GP_CNTRL_BIT_CLOCK_READY)
			break;
	}

	if (i == 10000)
		return -EIO;	/* TODO: better error value */

	/* set D0 standby bit */
	read_register(priv->net_dev, IPW_REG_GP_CNTRL, &r);
	write_register(priv->net_dev, IPW_REG_GP_CNTRL,
		       r | IPW_AUX_HOST_GP_CNTRL_BIT_HOST_ALLOWS_STANDBY);

	return 0;
}

/*********************************************************************
    Procedure   :   ipw2100_download_firmware
    Purpose     :   Initiaze adapter after power on.
                    The sequence is:
                    1. assert s/w reset first!
                    2. awake clocks & wait for clock stabilization
                    3. hold ARC (don't ask me why...)
                    4. load Dino ucode and reset/clock init again
                    5. zero-out shared mem
                    6. download f/w
 *******************************************************************/
static int ipw2100_download_firmware(struct ipw2100_priv *priv)
{
	u32 address;
	int err;

#ifndef CONFIG_PM
	/* Fetch the firmware and microcode */
	struct ipw2100_fw ipw2100_firmware;
#endif

	if (priv->fatal_error) {
		IPW_DEBUG_ERROR("%s: ipw2100_download_firmware called after "
				"fatal error %d.  Interface must be brought down.\n",
				priv->net_dev->name, priv->fatal_error);
		return -EINVAL;
	}
#ifdef CONFIG_PM
	if (!ipw2100_firmware.version) {
		err = ipw2100_get_firmware(priv, &ipw2100_firmware);
		if (err) {
			IPW_DEBUG_ERROR("%s: ipw2100_get_firmware failed: %d\n",
					priv->net_dev->name, err);
			priv->fatal_error = IPW2100_ERR_FW_LOAD;
			goto fail;
		}
	}
#else
	err = ipw2100_get_firmware(priv, &ipw2100_firmware);
	if (err) {
		IPW_DEBUG_ERROR("%s: ipw2100_get_firmware failed: %d\n",
				priv->net_dev->name, err);
		priv->fatal_error = IPW2100_ERR_FW_LOAD;
		goto fail;
	}
#endif
	priv->firmware_version = ipw2100_firmware.version;

	/* s/w reset and clock stabilization */
	err = sw_reset_and_clock(priv);
	if (err) {
		IPW_DEBUG_ERROR("%s: sw_reset_and_clock failed: %d\n",
				priv->net_dev->name, err);
		goto fail;
	}

	err = ipw2100_verify(priv);
	if (err) {
		IPW_DEBUG_ERROR("%s: ipw2100_verify failed: %d\n",
				priv->net_dev->name, err);
		goto fail;
	}

	/* Hold ARC */
	write_nic_dword(priv->net_dev,
			IPW_INTERNAL_REGISTER_HALT_AND_RESET, 0x80000000);

	/* allow ARC to run */
	write_register(priv->net_dev, IPW_REG_RESET_REG, 0);

	/* load microcode */
	err = ipw2100_ucode_download(priv, &ipw2100_firmware);
	if (err) {
		printk(KERN_ERR DRV_NAME ": %s: Error loading microcode: %d\n",
		       priv->net_dev->name, err);
		goto fail;
	}

	/* release ARC */
	write_nic_dword(priv->net_dev,
			IPW_INTERNAL_REGISTER_HALT_AND_RESET, 0x00000000);

	/* s/w reset and clock stabilization (again!!!) */
	err = sw_reset_and_clock(priv);
	if (err) {
		printk(KERN_ERR DRV_NAME
		       ": %s: sw_reset_and_clock failed: %d\n",
		       priv->net_dev->name, err);
		goto fail;
	}

	/* load f/w */
	err = ipw2100_fw_download(priv, &ipw2100_firmware);
	if (err) {
		IPW_DEBUG_ERROR("%s: Error loading firmware: %d\n",
				priv->net_dev->name, err);
		goto fail;
	}
#ifndef CONFIG_PM
	/*
	 * When the .resume method of the driver is called, the other
	 * part of the system, i.e. the ide driver could still stay in
	 * the suspend stage. This prevents us from loading the firmware
	 * from the disk.  --YZ
	 */

	/* free any storage allocated for firmware image */
	ipw2100_release_firmware(priv, &ipw2100_firmware);
#endif

	/* zero out Domain 1 area indirectly (Si requirement) */
	for (address = IPW_HOST_FW_SHARED_AREA0;
	     address < IPW_HOST_FW_SHARED_AREA0_END; address += 4)
		write_nic_dword(priv->net_dev, address, 0);
	for (address = IPW_HOST_FW_SHARED_AREA1;
	     address < IPW_HOST_FW_SHARED_AREA1_END; address += 4)
		write_nic_dword(priv->net_dev, address, 0);
	for (address = IPW_HOST_FW_SHARED_AREA2;
	     address < IPW_HOST_FW_SHARED_AREA2_END; address += 4)
		write_nic_dword(priv->net_dev, address, 0);
	for (address = IPW_HOST_FW_SHARED_AREA3;
	     address < IPW_HOST_FW_SHARED_AREA3_END; address += 4)
		write_nic_dword(priv->net_dev, address, 0);
	for (address = IPW_HOST_FW_INTERRUPT_AREA;
	     address < IPW_HOST_FW_INTERRUPT_AREA_END; address += 4)
		write_nic_dword(priv->net_dev, address, 0);

	return 0;

      fail:
	ipw2100_release_firmware(priv, &ipw2100_firmware);
	return err;
}

static inline void ipw2100_enable_interrupts(struct ipw2100_priv *priv)
{
	if (priv->status & STATUS_INT_ENABLED)
		return;
	priv->status |= STATUS_INT_ENABLED;
	write_register(priv->net_dev, IPW_REG_INTA_MASK, IPW_INTERRUPT_MASK);
}

static inline void ipw2100_disable_interrupts(struct ipw2100_priv *priv)
{
	if (!(priv->status & STATUS_INT_ENABLED))
		return;
	priv->status &= ~STATUS_INT_ENABLED;
	write_register(priv->net_dev, IPW_REG_INTA_MASK, 0x0);
}

static void ipw2100_initialize_ordinals(struct ipw2100_priv *priv)
{
	struct ipw2100_ordinals *ord = &priv->ordinals;

	IPW_DEBUG_INFO("enter\n");

	read_register(priv->net_dev, IPW_MEM_HOST_SHARED_ORDINALS_TABLE_1,
		      &ord->table1_addr);

	read_register(priv->net_dev, IPW_MEM_HOST_SHARED_ORDINALS_TABLE_2,
		      &ord->table2_addr);

	read_nic_dword(priv->net_dev, ord->table1_addr, &ord->table1_size);
	read_nic_dword(priv->net_dev, ord->table2_addr, &ord->table2_size);

	ord->table2_size &= 0x0000FFFF;

	IPW_DEBUG_INFO("table 1 size: %d\n", ord->table1_size);
	IPW_DEBUG_INFO("table 2 size: %d\n", ord->table2_size);
	IPW_DEBUG_INFO("exit\n");
}

static inline void ipw2100_hw_set_gpio(struct ipw2100_priv *priv)
{
	u32 reg = 0;
	/*
	 * Set GPIO 3 writable by FW; GPIO 1 writable
	 * by driver and enable clock
	 */
	reg = (IPW_BIT_GPIO_GPIO3_MASK | IPW_BIT_GPIO_GPIO1_ENABLE |
	       IPW_BIT_GPIO_LED_OFF);
	write_register(priv->net_dev, IPW_REG_GPIO, reg);
}

static int rf_kill_active(struct ipw2100_priv *priv)
{
#define MAX_RF_KILL_CHECKS 5
#define RF_KILL_CHECK_DELAY 40

	unsigned short value = 0;
	u32 reg = 0;
	int i;

	if (!(priv->hw_features & HW_FEATURE_RFKILL)) {
		priv->status &= ~STATUS_RF_KILL_HW;
		return 0;
	}

	for (i = 0; i < MAX_RF_KILL_CHECKS; i++) {
		udelay(RF_KILL_CHECK_DELAY);
		read_register(priv->net_dev, IPW_REG_GPIO, &reg);
		value = (value << 1) | ((reg & IPW_BIT_GPIO_RF_KILL) ? 0 : 1);
	}

	if (value == 0)
		priv->status |= STATUS_RF_KILL_HW;
	else
		priv->status &= ~STATUS_RF_KILL_HW;

	return (value == 0);
}

static int ipw2100_get_hw_features(struct ipw2100_priv *priv)
{
	u32 addr, len;
	u32 val;

	/*
	 * EEPROM_SRAM_DB_START_ADDRESS using ordinal in ordinal table 1
	 */
	len = sizeof(addr);
	if (ipw2100_get_ordinal
	    (priv, IPW_ORD_EEPROM_SRAM_DB_BLOCK_START_ADDRESS, &addr, &len)) {
		IPW_DEBUG_INFO("failed querying ordinals at line %d\n",
			       __LINE__);
		return -EIO;
	}

	IPW_DEBUG_INFO("EEPROM address: %08X\n", addr);

	/*
	 * EEPROM version is the byte at offset 0xfd in firmware
	 * We read 4 bytes, then shift out the byte we actually want */
	read_nic_dword(priv->net_dev, addr + 0xFC, &val);
	priv->eeprom_version = (val >> 24) & 0xFF;
	IPW_DEBUG_INFO("EEPROM version: %d\n", priv->eeprom_version);

	/*
	 *  HW RF Kill enable is bit 0 in byte at offset 0x21 in firmware
	 *
	 *  notice that the EEPROM bit is reverse polarity, i.e.
	 *     bit = 0  signifies HW RF kill switch is supported
	 *     bit = 1  signifies HW RF kill switch is NOT supported
	 */
	read_nic_dword(priv->net_dev, addr + 0x20, &val);
	if (!((val >> 24) & 0x01))
		priv->hw_features |= HW_FEATURE_RFKILL;

	IPW_DEBUG_INFO("HW RF Kill: %ssupported.\n",
		       (priv->hw_features & HW_FEATURE_RFKILL) ? "" : "not ");

	return 0;
}

/*
 * Start firmware execution after power on and intialization
 * The sequence is:
 *  1. Release ARC
 *  2. Wait for f/w initialization completes;
 */
static int ipw2100_start_adapter(struct ipw2100_priv *priv)
{
	int i;
	u32 inta, inta_mask, gpio;

	IPW_DEBUG_INFO("enter\n");

	if (priv->status & STATUS_RUNNING)
		return 0;

	/*
	 * Initialize the hw - drive adapter to DO state by setting
	 * init_done bit. Wait for clk_ready bit and Download
	 * fw & dino ucode
	 */
	if (ipw2100_download_firmware(priv)) {
		printk(KERN_ERR DRV_NAME
		       ": %s: Failed to power on the adapter.\n",
		       priv->net_dev->name);
		return -EIO;
	}

	/* Clear the Tx, Rx and Msg queues and the r/w indexes
	 * in the firmware RBD and TBD ring queue */
	ipw2100_queues_initialize(priv);

	ipw2100_hw_set_gpio(priv);

	/* TODO -- Look at disabling interrupts here to make sure none
	 * get fired during FW initialization */

	/* Release ARC - clear reset bit */
	write_register(priv->net_dev, IPW_REG_RESET_REG, 0);

	/* wait for f/w intialization complete */
	IPW_DEBUG_FW("Waiting for f/w initialization to complete...\n");
	i = 5000;
	do {
		schedule_timeout_uninterruptible(msecs_to_jiffies(40));
		/* Todo... wait for sync command ... */

		read_register(priv->net_dev, IPW_REG_INTA, &inta);

		/* check "init done" bit */
		if (inta & IPW2100_INTA_FW_INIT_DONE) {
			/* reset "init done" bit */
			write_register(priv->net_dev, IPW_REG_INTA,
				       IPW2100_INTA_FW_INIT_DONE);
			break;
		}

		/* check error conditions : we check these after the firmware
		 * check so that if there is an error, the interrupt handler
		 * will see it and the adapter will be reset */
		if (inta &
		    (IPW2100_INTA_FATAL_ERROR | IPW2100_INTA_PARITY_ERROR)) {
			/* clear error conditions */
			write_register(priv->net_dev, IPW_REG_INTA,
				       IPW2100_INTA_FATAL_ERROR |
				       IPW2100_INTA_PARITY_ERROR);
		}
	} while (--i);

	/* Clear out any pending INTAs since we aren't supposed to have
	 * interrupts enabled at this point... */
	read_register(priv->net_dev, IPW_REG_INTA, &inta);
	read_register(priv->net_dev, IPW_REG_INTA_MASK, &inta_mask);
	inta &= IPW_INTERRUPT_MASK;
	/* Clear out any pending interrupts */
	if (inta & inta_mask)
		write_register(priv->net_dev, IPW_REG_INTA, inta);

	IPW_DEBUG_FW("f/w initialization complete: %s\n",
		     i ? "SUCCESS" : "FAILED");

	if (!i) {
		printk(KERN_WARNING DRV_NAME
		       ": %s: Firmware did not initialize.\n",
		       priv->net_dev->name);
		return -EIO;
	}

	/* allow firmware to write to GPIO1 & GPIO3 */
	read_register(priv->net_dev, IPW_REG_GPIO, &gpio);

	gpio |= (IPW_BIT_GPIO_GPIO1_MASK | IPW_BIT_GPIO_GPIO3_MASK);

	write_register(priv->net_dev, IPW_REG_GPIO, gpio);

	/* Ready to receive commands */
	priv->status |= STATUS_RUNNING;

	/* The adapter has been reset; we are not associated */
	priv->status &= ~(STATUS_ASSOCIATING | STATUS_ASSOCIATED);

	IPW_DEBUG_INFO("exit\n");

	return 0;
}

static inline void ipw2100_reset_fatalerror(struct ipw2100_priv *priv)
{
	if (!priv->fatal_error)
		return;

	priv->fatal_errors[priv->fatal_index++] = priv->fatal_error;
	priv->fatal_index %= IPW2100_ERROR_QUEUE;
	priv->fatal_error = 0;
}

/* NOTE: Our interrupt is disabled when this method is called */
static int ipw2100_power_cycle_adapter(struct ipw2100_priv *priv)
{
	u32 reg;
	int i;

	IPW_DEBUG_INFO("Power cycling the hardware.\n");

	ipw2100_hw_set_gpio(priv);

	/* Step 1. Stop Master Assert */
	write_register(priv->net_dev, IPW_REG_RESET_REG,
		       IPW_AUX_HOST_RESET_REG_STOP_MASTER);

	/* Step 2. Wait for stop Master Assert
	 *         (not more than 50us, otherwise ret error */
	i = 5;
	do {
		udelay(IPW_WAIT_RESET_MASTER_ASSERT_COMPLETE_DELAY);
		read_register(priv->net_dev, IPW_REG_RESET_REG, &reg);

		if (reg & IPW_AUX_HOST_RESET_REG_MASTER_DISABLED)
			break;
	} while (--i);

	priv->status &= ~STATUS_RESET_PENDING;

	if (!i) {
		IPW_DEBUG_INFO
		    ("exit - waited too long for master assert stop\n");
		return -EIO;
	}

	write_register(priv->net_dev, IPW_REG_RESET_REG,
		       IPW_AUX_HOST_RESET_REG_SW_RESET);

	/* Reset any fatal_error conditions */
	ipw2100_reset_fatalerror(priv);

	/* At this point, the adapter is now stopped and disabled */
	priv->status &= ~(STATUS_RUNNING | STATUS_ASSOCIATING |
			  STATUS_ASSOCIATED | STATUS_ENABLED);

	return 0;
}

/*
 * Send the CARD_DISABLE_PHY_OFF comamnd to the card to disable it
 *
 * After disabling, if the card was associated, a STATUS_ASSN_LOST will be sent.
 *
 * STATUS_CARD_DISABLE_NOTIFICATION will be sent regardless of
 * if STATUS_ASSN_LOST is sent.
 */
static int ipw2100_hw_phy_off(struct ipw2100_priv *priv)
{

#define HW_PHY_OFF_LOOP_DELAY (HZ / 5000)

	struct host_command cmd = {
		.host_command = CARD_DISABLE_PHY_OFF,
		.host_command_sequence = 0,
		.host_command_length = 0,
	};
	int err, i;
	u32 val1, val2;

	IPW_DEBUG_HC("CARD_DISABLE_PHY_OFF\n");

	/* Turn off the radio */
	err = ipw2100_hw_send_command(priv, &cmd);
	if (err)
		return err;

	for (i = 0; i < 2500; i++) {
		read_nic_dword(priv->net_dev, IPW2100_CONTROL_REG, &val1);
		read_nic_dword(priv->net_dev, IPW2100_COMMAND, &val2);

		if ((val1 & IPW2100_CONTROL_PHY_OFF) &&
		    (val2 & IPW2100_COMMAND_PHY_OFF))
			return 0;

		schedule_timeout_uninterruptible(HW_PHY_OFF_LOOP_DELAY);
	}

	return -EIO;
}

static int ipw2100_enable_adapter(struct ipw2100_priv *priv)
{
	struct host_command cmd = {
		.host_command = HOST_COMPLETE,
		.host_command_sequence = 0,
		.host_command_length = 0
	};
	int err = 0;

	IPW_DEBUG_HC("HOST_COMPLETE\n");

	if (priv->status & STATUS_ENABLED)
		return 0;

	mutex_lock(&priv->adapter_mutex);

	if (rf_kill_active(priv)) {
		IPW_DEBUG_HC("Command aborted due to RF kill active.\n");
		goto fail_up;
	}

	err = ipw2100_hw_send_command(priv, &cmd);
	if (err) {
		IPW_DEBUG_INFO("Failed to send HOST_COMPLETE command\n");
		goto fail_up;
	}

	err = ipw2100_wait_for_card_state(priv, IPW_HW_STATE_ENABLED);
	if (err) {
		IPW_DEBUG_INFO("%s: card not responding to init command.\n",
			       priv->net_dev->name);
		goto fail_up;
	}

	if (priv->stop_hang_check) {
		priv->stop_hang_check = 0;
		queue_delayed_work(priv->workqueue, &priv->hang_check, HZ / 2);
	}

      fail_up:
	mutex_unlock(&priv->adapter_mutex);
	return err;
}

static int ipw2100_hw_stop_adapter(struct ipw2100_priv *priv)
{
#define HW_POWER_DOWN_DELAY (msecs_to_jiffies(100))

	struct host_command cmd = {
		.host_command = HOST_PRE_POWER_DOWN,
		.host_command_sequence = 0,
		.host_command_length = 0,
	};
	int err, i;
	u32 reg;

	if (!(priv->status & STATUS_RUNNING))
		return 0;

	priv->status |= STATUS_STOPPING;

	/* We can only shut down the card if the firmware is operational.  So,
	 * if we haven't reset since a fatal_error, then we can not send the
	 * shutdown commands. */
	if (!priv->fatal_error) {
		/* First, make sure the adapter is enabled so that the PHY_OFF
		 * command can shut it down */
		ipw2100_enable_adapter(priv);

		err = ipw2100_hw_phy_off(priv);
		if (err)
			printk(KERN_WARNING DRV_NAME
			       ": Error disabling radio %d\n", err);

		/*
		 * If in D0-standby mode going directly to D3 may cause a
		 * PCI bus violation.  Therefore we must change out of the D0
		 * state.
		 *
		 * Sending the PREPARE_FOR_POWER_DOWN will restrict the
		 * hardware from going into standby mode and will transition
		 * out of D0-standby if it is already in that state.
		 *
		 * STATUS_PREPARE_POWER_DOWN_COMPLETE will be sent by the
		 * driver upon completion.  Once received, the driver can
		 * proceed to the D3 state.
		 *
		 * Prepare for power down command to fw.  This command would
		 * take HW out of D0-standby and prepare it for D3 state.
		 *
		 * Currently FW does not support event notification for this
		 * event. Therefore, skip waiting for it.  Just wait a fixed
		 * 100ms
		 */
		IPW_DEBUG_HC("HOST_PRE_POWER_DOWN\n");

		err = ipw2100_hw_send_command(priv, &cmd);
		if (err)
			printk(KERN_WARNING DRV_NAME ": "
			       "%s: Power down command failed: Error %d\n",
			       priv->net_dev->name, err);
		else
			schedule_timeout_uninterruptible(HW_POWER_DOWN_DELAY);
	}

	priv->status &= ~STATUS_ENABLED;

	/*
	 * Set GPIO 3 writable by FW; GPIO 1 writable
	 * by driver and enable clock
	 */
	ipw2100_hw_set_gpio(priv);

	/*
	 * Power down adapter.  Sequence:
	 * 1. Stop master assert (RESET_REG[9]=1)
	 * 2. Wait for stop master (RESET_REG[8]==1)
	 * 3. S/w reset assert (RESET_REG[7] = 1)
	 */

	/* Stop master assert */
	write_register(priv->net_dev, IPW_REG_RESET_REG,
		       IPW_AUX_HOST_RESET_REG_STOP_MASTER);

	/* wait stop master not more than 50 usec.
	 * Otherwise return error. */
	for (i = 5; i > 0; i--) {
		udelay(10);

		/* Check master stop bit */
		read_register(priv->net_dev, IPW_REG_RESET_REG, &reg);

		if (reg & IPW_AUX_HOST_RESET_REG_MASTER_DISABLED)
			break;
	}

	if (i == 0)
		printk(KERN_WARNING DRV_NAME
		       ": %s: Could now power down adapter.\n",
		       priv->net_dev->name);

	/* assert s/w reset */
	write_register(priv->net_dev, IPW_REG_RESET_REG,
		       IPW_AUX_HOST_RESET_REG_SW_RESET);

	priv->status &= ~(STATUS_RUNNING | STATUS_STOPPING);

	return 0;
}

static int ipw2100_disable_adapter(struct ipw2100_priv *priv)
{
	struct host_command cmd = {
		.host_command = CARD_DISABLE,
		.host_command_sequence = 0,
		.host_command_length = 0
	};
	int err = 0;

	IPW_DEBUG_HC("CARD_DISABLE\n");

	if (!(priv->status & STATUS_ENABLED))
		return 0;

	/* Make sure we clear the associated state */
	priv->status &= ~(STATUS_ASSOCIATED | STATUS_ASSOCIATING);

	if (!priv->stop_hang_check) {
		priv->stop_hang_check = 1;
		cancel_delayed_work(&priv->hang_check);
	}

	mutex_lock(&priv->adapter_mutex);

	err = ipw2100_hw_send_command(priv, &cmd);
	if (err) {
		printk(KERN_WARNING DRV_NAME
		       ": exit - failed to send CARD_DISABLE command\n");
		goto fail_up;
	}

	err = ipw2100_wait_for_card_state(priv, IPW_HW_STATE_DISABLED);
	if (err) {
		printk(KERN_WARNING DRV_NAME
		       ": exit - card failed to change to DISABLED\n");
		goto fail_up;
	}

	IPW_DEBUG_INFO("TODO: implement scan state machine\n");

      fail_up:
	mutex_unlock(&priv->adapter_mutex);
	return err;
}

static int ipw2100_set_scan_options(struct ipw2100_priv *priv)
{
	struct host_command cmd = {
		.host_command = SET_SCAN_OPTIONS,
		.host_command_sequence = 0,
		.host_command_length = 8
	};
	int err;

	IPW_DEBUG_INFO("enter\n");

	IPW_DEBUG_SCAN("setting scan options\n");

	cmd.host_command_parameters[0] = 0;

	if (!(priv->config & CFG_ASSOCIATE))
		cmd.host_command_parameters[0] |= IPW_SCAN_NOASSOCIATE;
	if ((priv->ieee->sec.flags & SEC_ENABLED) && priv->ieee->sec.enabled)
		cmd.host_command_parameters[0] |= IPW_SCAN_MIXED_CELL;
	if (priv->config & CFG_PASSIVE_SCAN)
		cmd.host_command_parameters[0] |= IPW_SCAN_PASSIVE;

	cmd.host_command_parameters[1] = priv->channel_mask;

	err = ipw2100_hw_send_command(priv, &cmd);

	IPW_DEBUG_HC("SET_SCAN_OPTIONS 0x%04X\n",
		     cmd.host_command_parameters[0]);

	return err;
}

static int ipw2100_start_scan(struct ipw2100_priv *priv)
{
	struct host_command cmd = {
		.host_command = BROADCAST_SCAN,
		.host_command_sequence = 0,
		.host_command_length = 4
	};
	int err;

	IPW_DEBUG_HC("START_SCAN\n");

	cmd.host_command_parameters[0] = 0;

	/* No scanning if in monitor mode */
	if (priv->ieee->iw_mode == IW_MODE_MONITOR)
		return 1;

	if (priv->status & STATUS_SCANNING) {
		IPW_DEBUG_SCAN("Scan requested while already in scan...\n");
		return 0;
	}

	IPW_DEBUG_INFO("enter\n");

	/* Not clearing here; doing so makes iwlist always return nothing...
	 *
	 * We should modify the table logic to use aging tables vs. clearing
	 * the table on each scan start.
	 */
	IPW_DEBUG_SCAN("starting scan\n");

	priv->status |= STATUS_SCANNING;
	err = ipw2100_hw_send_command(priv, &cmd);
	if (err)
		priv->status &= ~STATUS_SCANNING;

	IPW_DEBUG_INFO("exit\n");

	return err;
}

static const struct libipw_geo ipw_geos[] = {
	{			/* Restricted */
	 "---",
	 .bg_channels = 14,
	 .bg = {{2412, 1}, {2417, 2}, {2422, 3},
		{2427, 4}, {2432, 5}, {2437, 6},
		{2442, 7}, {2447, 8}, {2452, 9},
		{2457, 10}, {2462, 11}, {2467, 12},
		{2472, 13}, {2484, 14}},
	 },
};

static int ipw2100_up(struct ipw2100_priv *priv, int deferred)
{
	unsigned long flags;
	int rc = 0;
	u32 lock;
	u32 ord_len = sizeof(lock);

	/* Age scan list entries found before suspend */
	if (priv->suspend_time) {
		libipw_networks_age(priv->ieee, priv->suspend_time);
		priv->suspend_time = 0;
	}

	/* Quiet if manually disabled. */
	if (priv->status & STATUS_RF_KILL_SW) {
		IPW_DEBUG_INFO("%s: Radio is disabled by Manual Disable "
			       "switch\n", priv->net_dev->name);
		return 0;
	}

	/* the ipw2100 hardware really doesn't want power management delays
	 * longer than 175usec
	 */
	pm_qos_update_requirement(PM_QOS_CPU_DMA_LATENCY, "ipw2100", 175);

	/* If the interrupt is enabled, turn it off... */
	spin_lock_irqsave(&priv->low_lock, flags);
	ipw2100_disable_interrupts(priv);

	/* Reset any fatal_error conditions */
	ipw2100_reset_fatalerror(priv);
	spin_unlock_irqrestore(&priv->low_lock, flags);

	if (priv->status & STATUS_POWERED ||
	    (priv->status & STATUS_RESET_PENDING)) {
		/* Power cycle the card ... */
		if (ipw2100_power_cycle_adapter(priv)) {
			printk(KERN_WARNING DRV_NAME
			       ": %s: Could not cycle adapter.\n",
			       priv->net_dev->name);
			rc = 1;
			goto exit;
		}
	} else
		priv->status |= STATUS_POWERED;

	/* Load the firmware, start the clocks, etc. */
	if (ipw2100_start_adapter(priv)) {
		printk(KERN_ERR DRV_NAME
		       ": %s: Failed to start the firmware.\n",
		       priv->net_dev->name);
		rc = 1;
		goto exit;
	}

	ipw2100_initialize_ordinals(priv);

	/* Determine capabilities of this particular HW configuration */
	if (ipw2100_get_hw_features(priv)) {
		printk(KERN_ERR DRV_NAME
		       ": %s: Failed to determine HW features.\n",
		       priv->net_dev->name);
		rc = 1;
		goto exit;
	}

	/* Initialize the geo */
	if (libipw_set_geo(priv->ieee, &ipw_geos[0])) {
		printk(KERN_WARNING DRV_NAME "Could not set geo\n");
		return 0;
	}
	priv->ieee->freq_band = LIBIPW_24GHZ_BAND;

	lock = LOCK_NONE;
	if (ipw2100_set_ordinal(priv, IPW_ORD_PERS_DB_LOCK, &lock, &ord_len)) {
		printk(KERN_ERR DRV_NAME
		       ": %s: Failed to clear ordinal lock.\n",
		       priv->net_dev->name);
		rc = 1;
		goto exit;
	}

	priv->status &= ~STATUS_SCANNING;

	if (rf_kill_active(priv)) {
		printk(KERN_INFO "%s: Radio is disabled by RF switch.\n",
		       priv->net_dev->name);

		if (priv->stop_rf_kill) {
			priv->stop_rf_kill = 0;
			queue_delayed_work(priv->workqueue, &priv->rf_kill,
					   round_jiffies_relative(HZ));
		}

		deferred = 1;
	}

	/* Turn on the interrupt so that commands can be processed */
	ipw2100_enable_interrupts(priv);

	/* Send all of the commands that must be sent prior to
	 * HOST_COMPLETE */
	if (ipw2100_adapter_setup(priv)) {
		printk(KERN_ERR DRV_NAME ": %s: Failed to start the card.\n",
		       priv->net_dev->name);
		rc = 1;
		goto exit;
	}

	if (!deferred) {
		/* Enable the adapter - sends HOST_COMPLETE */
		if (ipw2100_enable_adapter(priv)) {
			printk(KERN_ERR DRV_NAME ": "
			       "%s: failed in call to enable adapter.\n",
			       priv->net_dev->name);
			ipw2100_hw_stop_adapter(priv);
			rc = 1;
			goto exit;
		}

		/* Start a scan . . . */
		ipw2100_set_scan_options(priv);
		ipw2100_start_scan(priv);
	}

      exit:
	return rc;
}

/* Called by register_netdev() */
static int ipw2100_net_init(struct net_device *dev)
{
	struct ipw2100_priv *priv = libipw_priv(dev);
	return ipw2100_up(priv, 1);
}

static void ipw2100_down(struct ipw2100_priv *priv)
{
	unsigned long flags;
	union iwreq_data wrqu = {
		.ap_addr = {
			    .sa_family = ARPHRD_ETHER}
	};
	int associated = priv->status & STATUS_ASSOCIATED;

	/* Kill the RF switch timer */
	if (!priv->stop_rf_kill) {
		priv->stop_rf_kill = 1;
		cancel_delayed_work(&priv->rf_kill);
	}

	/* Kill the firmware hang check timer */
	if (!priv->stop_hang_check) {
		priv->stop_hang_check = 1;
		cancel_delayed_work(&priv->hang_check);
	}

	/* Kill any pending resets */
	if (priv->status & STATUS_RESET_PENDING)
		cancel_delayed_work(&priv->reset_work);

	/* Make sure the interrupt is on so that FW commands will be
	 * processed correctly */
	spin_lock_irqsave(&priv->low_lock, flags);
	ipw2100_enable_interrupts(priv);
	spin_unlock_irqrestore(&priv->low_lock, flags);

	if (ipw2100_hw_stop_adapter(priv))
		printk(KERN_ERR DRV_NAME ": %s: Error stopping adapter.\n",
		       priv->net_dev->name);

	/* Do not disable the interrupt until _after_ we disable
	 * the adaptor.  Otherwise the CARD_DISABLE command will never
	 * be ack'd by the firmware */
	spin_lock_irqsave(&priv->low_lock, flags);
	ipw2100_disable_interrupts(priv);
	spin_unlock_irqrestore(&priv->low_lock, flags);

	pm_qos_update_requirement(PM_QOS_CPU_DMA_LATENCY, "ipw2100",
			PM_QOS_DEFAULT_VALUE);

	/* We have to signal any supplicant if we are disassociating */
	if (associated)
		wireless_send_event(priv->net_dev, SIOCGIWAP, &wrqu, NULL);

	priv->status &= ~(STATUS_ASSOCIATED | STATUS_ASSOCIATING);
	netif_carrier_off(priv->net_dev);
	netif_stop_queue(priv->net_dev);
}

static void ipw2100_reset_adapter(struct work_struct *work)
{
	struct ipw2100_priv *priv =
		container_of(work, struct ipw2100_priv, reset_work.work);
	unsigned long flags;
	union iwreq_data wrqu = {
		.ap_addr = {
			    .sa_family = ARPHRD_ETHER}
	};
	int associated = priv->status & STATUS_ASSOCIATED;

	spin_lock_irqsave(&priv->low_lock, flags);
	IPW_DEBUG_INFO(": %s: Restarting adapter.\n", priv->net_dev->name);
	priv->resets++;
	priv->status &= ~(STATUS_ASSOCIATED | STATUS_ASSOCIATING);
	priv->status |= STATUS_SECURITY_UPDATED;

	/* Force a power cycle even if interface hasn't been opened
	 * yet */
	cancel_delayed_work(&priv->reset_work);
	priv->status |= STATUS_RESET_PENDING;
	spin_unlock_irqrestore(&priv->low_lock, flags);

	mutex_lock(&priv->action_mutex);
	/* stop timed checks so that they don't interfere with reset */
	priv->stop_hang_check = 1;
	cancel_delayed_work(&priv->hang_check);

	/* We have to signal any supplicant if we are disassociating */
	if (associated)
		wireless_send_event(priv->net_dev, SIOCGIWAP, &wrqu, NULL);

	ipw2100_up(priv, 0);
	mutex_unlock(&priv->action_mutex);

}

static void isr_indicate_associated(struct ipw2100_priv *priv, u32 status)
{

#define MAC_ASSOCIATION_READ_DELAY (HZ)
	int ret;
	unsigned int len, essid_len;
	char essid[IW_ESSID_MAX_SIZE];
	u32 txrate;
	u32 chan;
	char *txratename;
	u8 bssid[ETH_ALEN];
	DECLARE_SSID_BUF(ssid);

	/*
	 * TBD: BSSID is usually 00:00:00:00:00:00 here and not
	 *      an actual MAC of the AP. Seems like FW sets this
	 *      address too late. Read it later and expose through
	 *      /proc or schedule a later task to query and update
	 */

	essid_len = IW_ESSID_MAX_SIZE;
	ret = ipw2100_get_ordinal(priv, IPW_ORD_STAT_ASSN_SSID,
				  essid, &essid_len);
	if (ret) {
		IPW_DEBUG_INFO("failed querying ordinals at line %d\n",
			       __LINE__);
		return;
	}

	len = sizeof(u32);
	ret = ipw2100_get_ordinal(priv, IPW_ORD_CURRENT_TX_RATE, &txrate, &len);
	if (ret) {
		IPW_DEBUG_INFO("failed querying ordinals at line %d\n",
			       __LINE__);
		return;
	}

	len = sizeof(u32);
	ret = ipw2100_get_ordinal(priv, IPW_ORD_OUR_FREQ, &chan, &len);
	if (ret) {
		IPW_DEBUG_INFO("failed querying ordinals at line %d\n",
			       __LINE__);
		return;
	}
	len = ETH_ALEN;
	ipw2100_get_ordinal(priv, IPW_ORD_STAT_ASSN_AP_BSSID, &bssid, &len);
	if (ret) {
		IPW_DEBUG_INFO("failed querying ordinals at line %d\n",
			       __LINE__);
		return;
	}
	memcpy(priv->ieee->bssid, bssid, ETH_ALEN);

	switch (txrate) {
	case TX_RATE_1_MBIT:
		txratename = "1Mbps";
		break;
	case TX_RATE_2_MBIT:
		txratename = "2Mbsp";
		break;
	case TX_RATE_5_5_MBIT:
		txratename = "5.5Mbps";
		break;
	case TX_RATE_11_MBIT:
		txratename = "11Mbps";
		break;
	default:
		IPW_DEBUG_INFO("Unknown rate: %d\n", txrate);
		txratename = "unknown rate";
		break;
	}

	IPW_DEBUG_INFO("%s: Associated with '%s' at %s, channel %d (BSSID=%pM)\n",
		       priv->net_dev->name, print_ssid(ssid, essid, essid_len),
		       txratename, chan, bssid);

	/* now we copy read ssid into dev */
	if (!(priv->config & CFG_STATIC_ESSID)) {
		priv->essid_len = min((u8) essid_len, (u8) IW_ESSID_MAX_SIZE);
		memcpy(priv->essid, essid, priv->essid_len);
	}
	priv->channel = chan;
	memcpy(priv->bssid, bssid, ETH_ALEN);

	priv->status |= STATUS_ASSOCIATING;
	priv->connect_start = get_seconds();

	queue_delayed_work(priv->workqueue, &priv->wx_event_work, HZ / 10);
}

static int ipw2100_set_essid(struct ipw2100_priv *priv, char *essid,
			     int length, int batch_mode)
{
	int ssid_len = min(length, IW_ESSID_MAX_SIZE);
	struct host_command cmd = {
		.host_command = SSID,
		.host_command_sequence = 0,
		.host_command_length = ssid_len
	};
	int err;
	DECLARE_SSID_BUF(ssid);

	IPW_DEBUG_HC("SSID: '%s'\n", print_ssid(ssid, essid, ssid_len));

	if (ssid_len)
		memcpy(cmd.host_command_parameters, essid, ssid_len);

	if (!batch_mode) {
		err = ipw2100_disable_adapter(priv);
		if (err)
			return err;
	}

	/* Bug in FW currently doesn't honor bit 0 in SET_SCAN_OPTIONS to
	 * disable auto association -- so we cheat by setting a bogus SSID */
	if (!ssid_len && !(priv->config & CFG_ASSOCIATE)) {
		int i;
		u8 *bogus = (u8 *) cmd.host_command_parameters;
		for (i = 0; i < IW_ESSID_MAX_SIZE; i++)
			bogus[i] = 0x18 + i;
		cmd.host_command_length = IW_ESSID_MAX_SIZE;
	}

	/* NOTE:  We always send the SSID command even if the provided ESSID is
	 * the same as what we currently think is set. */

	err = ipw2100_hw_send_command(priv, &cmd);
	if (!err) {
		memset(priv->essid + ssid_len, 0, IW_ESSID_MAX_SIZE - ssid_len);
		memcpy(priv->essid, essid, ssid_len);
		priv->essid_len = ssid_len;
	}

	if (!batch_mode) {
		if (ipw2100_enable_adapter(priv))
			err = -EIO;
	}

	return err;
}

static void isr_indicate_association_lost(struct ipw2100_priv *priv, u32 status)
{
	DECLARE_SSID_BUF(ssid);

	IPW_DEBUG(IPW_DL_NOTIF | IPW_DL_STATE | IPW_DL_ASSOC,
		  "disassociated: '%s' %pM \n",
		  print_ssid(ssid, priv->essid, priv->essid_len),
		  priv->bssid);

	priv->status &= ~(STATUS_ASSOCIATED | STATUS_ASSOCIATING);

	if (priv->status & STATUS_STOPPING) {
		IPW_DEBUG_INFO("Card is stopping itself, discard ASSN_LOST.\n");
		return;
	}

	memset(priv->bssid, 0, ETH_ALEN);
	memset(priv->ieee->bssid, 0, ETH_ALEN);

	netif_carrier_off(priv->net_dev);
	netif_stop_queue(priv->net_dev);

	if (!(priv->status & STATUS_RUNNING))
		return;

	if (priv->status & STATUS_SECURITY_UPDATED)
		queue_delayed_work(priv->workqueue, &priv->security_work, 0);

	queue_delayed_work(priv->workqueue, &priv->wx_event_work, 0);
}

static void isr_indicate_rf_kill(struct ipw2100_priv *priv, u32 status)
{
	IPW_DEBUG_INFO("%s: RF Kill state changed to radio OFF.\n",
		       priv->net_dev->name);

	/* RF_KILL is now enabled (else we wouldn't be here) */
	priv->status |= STATUS_RF_KILL_HW;

	/* Make sure the RF Kill check timer is running */
	priv->stop_rf_kill = 0;
	cancel_delayed_work(&priv->rf_kill);
	queue_delayed_work(priv->workqueue, &priv->rf_kill,
			   round_jiffies_relative(HZ));
}

static void send_scan_event(void *data)
{
	struct ipw2100_priv *priv = data;
	union iwreq_data wrqu;

	wrqu.data.length = 0;
	wrqu.data.flags = 0;
	wireless_send_event(priv->net_dev, SIOCGIWSCAN, &wrqu, NULL);
}

static void ipw2100_scan_event_later(struct work_struct *work)
{
	send_scan_event(container_of(work, struct ipw2100_priv,
					scan_event_later.work));
}

static void ipw2100_scan_event_now(struct work_struct *work)
{
	send_scan_event(container_of(work, struct ipw2100_priv,
					scan_event_now));
}

static void isr_scan_complete(struct ipw2100_priv *priv, u32 status)
{
	IPW_DEBUG_SCAN("scan complete\n");
	/* Age the scan results... */
	priv->ieee->scans++;
	priv->status &= ~STATUS_SCANNING;

	/* Only userspace-requested scan completion events go out immediately */
	if (!priv->user_requested_scan) {
		if (!delayed_work_pending(&priv->scan_event_later))
			queue_delayed_work(priv->workqueue,
					&priv->scan_event_later,
					round_jiffies_relative(msecs_to_jiffies(4000)));
	} else {
		priv->user_requested_scan = 0;
		cancel_delayed_work(&priv->scan_event_later);
		queue_work(priv->workqueue, &priv->scan_event_now);
	}
}

#ifdef CONFIG_IPW2100_DEBUG
#define IPW2100_HANDLER(v, f) { v, f, # v }
struct ipw2100_status_indicator {
	int status;
	void (*cb) (struct ipw2100_priv * priv, u32 status);
	char *name;
};
#else
#define IPW2100_HANDLER(v, f) { v, f }
struct ipw2100_status_indicator {
	int status;
	void (*cb) (struct ipw2100_priv * priv, u32 status);
};
#endif				/* CONFIG_IPW2100_DEBUG */

static void isr_indicate_scanning(struct ipw2100_priv *priv, u32 status)
{
	IPW_DEBUG_SCAN("Scanning...\n");
	priv->status |= STATUS_SCANNING;
}

static const struct ipw2100_status_indicator status_handlers[] = {
	IPW2100_HANDLER(IPW_STATE_INITIALIZED, NULL),
	IPW2100_HANDLER(IPW_STATE_COUNTRY_FOUND, NULL),
	IPW2100_HANDLER(IPW_STATE_ASSOCIATED, isr_indicate_associated),
	IPW2100_HANDLER(IPW_STATE_ASSN_LOST, isr_indicate_association_lost),
	IPW2100_HANDLER(IPW_STATE_ASSN_CHANGED, NULL),
	IPW2100_HANDLER(IPW_STATE_SCAN_COMPLETE, isr_scan_complete),
	IPW2100_HANDLER(IPW_STATE_ENTERED_PSP, NULL),
	IPW2100_HANDLER(IPW_STATE_LEFT_PSP, NULL),
	IPW2100_HANDLER(IPW_STATE_RF_KILL, isr_indicate_rf_kill),
	IPW2100_HANDLER(IPW_STATE_DISABLED, NULL),
	IPW2100_HANDLER(IPW_STATE_POWER_DOWN, NULL),
	IPW2100_HANDLER(IPW_STATE_SCANNING, isr_indicate_scanning),
	IPW2100_HANDLER(-1, NULL)
};

static void isr_status_change(struct ipw2100_priv *priv, int status)
{
	int i;

	if (status == IPW_STATE_SCANNING &&
	    priv->status & STATUS_ASSOCIATED &&
	    !(priv->status & STATUS_SCANNING)) {
		IPW_DEBUG_INFO("Scan detected while associated, with "
			       "no scan request.  Restarting firmware.\n");

		/* Wake up any sleeping jobs */
		schedule_reset(priv);
	}

	for (i = 0; status_handlers[i].status != -1; i++) {
		if (status == status_handlers[i].status) {
			IPW_DEBUG_NOTIF("Status change: %s\n",
					status_handlers[i].name);
			if (status_handlers[i].cb)
				status_handlers[i].cb(priv, status);
			priv->wstats.status = status;
			return;
		}
	}

	IPW_DEBUG_NOTIF("unknown status received: %04x\n", status);
}

static void isr_rx_complete_command(struct ipw2100_priv *priv,
				    struct ipw2100_cmd_header *cmd)
{
#ifdef CONFIG_IPW2100_DEBUG
	if (cmd->host_command_reg < ARRAY_SIZE(command_types)) {
		IPW_DEBUG_HC("Command completed '%s (%d)'\n",
			     command_types[cmd->host_command_reg],
			     cmd->host_command_reg);
	}
#endif
	if (cmd->host_command_reg == HOST_COMPLETE)
		priv->status |= STATUS_ENABLED;

	if (cmd->host_command_reg == CARD_DISABLE)
		priv->status &= ~STATUS_ENABLED;

	priv->status &= ~STATUS_CMD_ACTIVE;

	wake_up_interruptible(&priv->wait_command_queue);
}

#ifdef CONFIG_IPW2100_DEBUG
static const char *frame_types[] = {
	"COMMAND_STATUS_VAL",
	"STATUS_CHANGE_VAL",
	"P80211_DATA_VAL",
	"P8023_DATA_VAL",
	"HOST_NOTIFICATION_VAL"
};
#endif

static int ipw2100_alloc_skb(struct ipw2100_priv *priv,
				    struct ipw2100_rx_packet *packet)
{
	packet->skb = dev_alloc_skb(sizeof(struct ipw2100_rx));
	if (!packet->skb)
		return -ENOMEM;

	packet->rxp = (struct ipw2100_rx *)packet->skb->data;
	packet->dma_addr = pci_map_single(priv->pci_dev, packet->skb->data,
					  sizeof(struct ipw2100_rx),
					  PCI_DMA_FROMDEVICE);
	/* NOTE: pci_map_single does not return an error code, and 0 is a valid
	 *       dma_addr */

	return 0;
}

#define SEARCH_ERROR   0xffffffff
#define SEARCH_FAIL    0xfffffffe
#define SEARCH_SUCCESS 0xfffffff0
#define SEARCH_DISCARD 0
#define SEARCH_SNAPSHOT 1

#define SNAPSHOT_ADDR(ofs) (priv->snapshot[((ofs) >> 12) & 0xff] + ((ofs) & 0xfff))
static void ipw2100_snapshot_free(struct ipw2100_priv *priv)
{
	int i;
	if (!priv->snapshot[0])
		return;
	for (i = 0; i < 0x30; i++)
		kfree(priv->snapshot[i]);
	priv->snapshot[0] = NULL;
}

#ifdef IPW2100_DEBUG_C3
static int ipw2100_snapshot_alloc(struct ipw2100_priv *priv)
{
	int i;
	if (priv->snapshot[0])
		return 1;
	for (i = 0; i < 0x30; i++) {
		priv->snapshot[i] = kmalloc(0x1000, GFP_ATOMIC);
		if (!priv->snapshot[i]) {
			IPW_DEBUG_INFO("%s: Error allocating snapshot "
				       "buffer %d\n", priv->net_dev->name, i);
			while (i > 0)
				kfree(priv->snapshot[--i]);
			priv->snapshot[0] = NULL;
			return 0;
		}
	}

	return 1;
}

static u32 ipw2100_match_buf(struct ipw2100_priv *priv, u8 * in_buf,
				    size_t len, int mode)
{
	u32 i, j;
	u32 tmp;
	u8 *s, *d;
	u32 ret;

	s = in_buf;
	if (mode == SEARCH_SNAPSHOT) {
		if (!ipw2100_snapshot_alloc(priv))
			mode = SEARCH_DISCARD;
	}

	for (ret = SEARCH_FAIL, i = 0; i < 0x30000; i += 4) {
		read_nic_dword(priv->net_dev, i, &tmp);
		if (mode == SEARCH_SNAPSHOT)
			*(u32 *) SNAPSHOT_ADDR(i) = tmp;
		if (ret == SEARCH_FAIL) {
			d = (u8 *) & tmp;
			for (j = 0; j < 4; j++) {
				if (*s != *d) {
					s = in_buf;
					continue;
				}

				s++;
				d++;

				if ((s - in_buf) == len)
					ret = (i + j) - len + 1;
			}
		} else if (mode == SEARCH_DISCARD)
			return ret;
	}

	return ret;
}
#endif

/*
 *
 * 0) Disconnect the SKB from the firmware (just unmap)
 * 1) Pack the ETH header into the SKB
 * 2) Pass the SKB to the network stack
 *
 * When packet is provided by the firmware, it contains the following:
 *
 * .  libipw_hdr
 * .  libipw_snap_hdr
 *
 * The size of the constructed ethernet
 *
 */
#ifdef IPW2100_RX_DEBUG
static u8 packet_data[IPW_RX_NIC_BUFFER_LENGTH];
#endif

static void ipw2100_corruption_detected(struct ipw2100_priv *priv, int i)
{
#ifdef IPW2100_DEBUG_C3
	struct ipw2100_status *status = &priv->status_queue.drv[i];
	u32 match, reg;
	int j;
#endif

	IPW_DEBUG_INFO(": PCI latency error detected at 0x%04zX.\n",
		       i * sizeof(struct ipw2100_status));

#ifdef IPW2100_DEBUG_C3
	/* Halt the firmware so we can get a good image */
	write_register(priv->net_dev, IPW_REG_RESET_REG,
		       IPW_AUX_HOST_RESET_REG_STOP_MASTER);
	j = 5;
	do {
		udelay(IPW_WAIT_RESET_MASTER_ASSERT_COMPLETE_DELAY);
		read_register(priv->net_dev, IPW_REG_RESET_REG, &reg);

		if (reg & IPW_AUX_HOST_RESET_REG_MASTER_DISABLED)
			break;
	} while (j--);

	match = ipw2100_match_buf(priv, (u8 *) status,
				  sizeof(struct ipw2100_status),
				  SEARCH_SNAPSHOT);
	if (match < SEARCH_SUCCESS)
		IPW_DEBUG_INFO("%s: DMA status match in Firmware at "
			       "offset 0x%06X, length %d:\n",
			       priv->net_dev->name, match,
			       sizeof(struct ipw2100_status));
	else
		IPW_DEBUG_INFO("%s: No DMA status match in "
			       "Firmware.\n", priv->net_dev->name);

	printk_buf((u8 *) priv->status_queue.drv,
		   sizeof(struct ipw2100_status) * RX_QUEUE_LENGTH);
#endif

	priv->fatal_error = IPW2100_ERR_C3_CORRUPTION;
	priv->net_dev->stats.rx_errors++;
	schedule_reset(priv);
}

static void isr_rx(struct ipw2100_priv *priv, int i,
			  struct libipw_rx_stats *stats)
{
	struct net_device *dev = priv->net_dev;
	struct ipw2100_status *status = &priv->status_queue.drv[i];
	struct ipw2100_rx_packet *packet = &priv->rx_buffers[i];

	IPW_DEBUG_RX("Handler...\n");

	if (unlikely(status->frame_size > skb_tailroom(packet->skb))) {
		IPW_DEBUG_INFO("%s: frame_size (%u) > skb_tailroom (%u)!"
			       "  Dropping.\n",
			       dev->name,
			       status->frame_size, skb_tailroom(packet->skb));
		dev->stats.rx_errors++;
		return;
	}

	if (unlikely(!netif_running(dev))) {
		dev->stats.rx_errors++;
		priv->wstats.discard.misc++;
		IPW_DEBUG_DROP("Dropping packet while interface is not up.\n");
		return;
	}

	if (unlikely(priv->ieee->iw_mode != IW_MODE_MONITOR &&
		     !(priv->status & STATUS_ASSOCIATED))) {
		IPW_DEBUG_DROP("Dropping packet while not associated.\n");
		priv->wstats.discard.misc++;
		return;
	}

	pci_unmap_single(priv->pci_dev,
			 packet->dma_addr,
			 sizeof(struct ipw2100_rx), PCI_DMA_FROMDEVICE);

	skb_put(packet->skb, status->frame_size);

#ifdef IPW2100_RX_DEBUG
	/* Make a copy of the frame so we can dump it to the logs if
	 * libipw_rx fails */
	skb_copy_from_linear_data(packet->skb, packet_data,
				  min_t(u32, status->frame_size,
					     IPW_RX_NIC_BUFFER_LENGTH));
#endif

	if (!libipw_rx(priv->ieee, packet->skb, stats)) {
#ifdef IPW2100_RX_DEBUG
		IPW_DEBUG_DROP("%s: Non consumed packet:\n",
			       dev->name);
		printk_buf(IPW_DL_DROP, packet_data, status->frame_size);
#endif
		dev->stats.rx_errors++;

		/* libipw_rx failed, so it didn't free the SKB */
		dev_kfree_skb_any(packet->skb);
		packet->skb = NULL;
	}

	/* We need to allocate a new SKB and attach it to the RDB. */
	if (unlikely(ipw2100_alloc_skb(priv, packet))) {
		printk(KERN_WARNING DRV_NAME ": "
		       "%s: Unable to allocate SKB onto RBD ring - disabling "
		       "adapter.\n", dev->name);
		/* TODO: schedule adapter shutdown */
		IPW_DEBUG_INFO("TODO: Shutdown adapter...\n");
	}

	/* Update the RDB entry */
	priv->rx_queue.drv[i].host_addr = packet->dma_addr;
}

#ifdef CONFIG_IPW2100_MONITOR

static void isr_rx_monitor(struct ipw2100_priv *priv, int i,
		   struct libipw_rx_stats *stats)
{
	struct net_device *dev = priv->net_dev;
	struct ipw2100_status *status = &priv->status_queue.drv[i];
	struct ipw2100_rx_packet *packet = &priv->rx_buffers[i];

	/* Magic struct that slots into the radiotap header -- no reason
	 * to build this manually element by element, we can write it much
	 * more efficiently than we can parse it. ORDER MATTERS HERE */
	struct ipw_rt_hdr {
		struct ieee80211_radiotap_header rt_hdr;
		s8 rt_dbmsignal; /* signal in dbM, kluged to signed */
	} *ipw_rt;

	IPW_DEBUG_RX("Handler...\n");

	if (unlikely(status->frame_size > skb_tailroom(packet->skb) -
				sizeof(struct ipw_rt_hdr))) {
		IPW_DEBUG_INFO("%s: frame_size (%u) > skb_tailroom (%u)!"
			       "  Dropping.\n",
			       dev->name,
			       status->frame_size,
			       skb_tailroom(packet->skb));
		dev->stats.rx_errors++;
		return;
	}

	if (unlikely(!netif_running(dev))) {
		dev->stats.rx_errors++;
		priv->wstats.discard.misc++;
		IPW_DEBUG_DROP("Dropping packet while interface is not up.\n");
		return;
	}

	if (unlikely(priv->config & CFG_CRC_CHECK &&
		     status->flags & IPW_STATUS_FLAG_CRC_ERROR)) {
		IPW_DEBUG_RX("CRC error in packet.  Dropping.\n");
		dev->stats.rx_errors++;
		return;
	}

	pci_unmap_single(priv->pci_dev, packet->dma_addr,
			 sizeof(struct ipw2100_rx), PCI_DMA_FROMDEVICE);
	memmove(packet->skb->data + sizeof(struct ipw_rt_hdr),
		packet->skb->data, status->frame_size);

	ipw_rt = (struct ipw_rt_hdr *) packet->skb->data;

	ipw_rt->rt_hdr.it_version = PKTHDR_RADIOTAP_VERSION;
	ipw_rt->rt_hdr.it_pad = 0; /* always good to zero */
	ipw_rt->rt_hdr.it_len = cpu_to_le16(sizeof(struct ipw_rt_hdr)); /* total hdr+data */

	ipw_rt->rt_hdr.it_present = cpu_to_le32(1 << IEEE80211_RADIOTAP_DBM_ANTSIGNAL);

	ipw_rt->rt_dbmsignal = status->rssi + IPW2100_RSSI_TO_DBM;

	skb_put(packet->skb, status->frame_size + sizeof(struct ipw_rt_hdr));

	if (!libipw_rx(priv->ieee, packet->skb, stats)) {
		dev->stats.rx_errors++;

		/* libipw_rx failed, so it didn't free the SKB */
		dev_kfree_skb_any(packet->skb);
		packet->skb = NULL;
	}

	/* We need to allocate a new SKB and attach it to the RDB. */
	if (unlikely(ipw2100_alloc_skb(priv, packet))) {
		IPW_DEBUG_WARNING(
			"%s: Unable to allocate SKB onto RBD ring - disabling "
			"adapter.\n", dev->name);
		/* TODO: schedule adapter shutdown */
		IPW_DEBUG_INFO("TODO: Shutdown adapter...\n");
	}

	/* Update the RDB entry */
	priv->rx_queue.drv[i].host_addr = packet->dma_addr;
}

#endif

static int ipw2100_corruption_check(struct ipw2100_priv *priv, int i)
{
	struct ipw2100_status *status = &priv->status_queue.drv[i];
	struct ipw2100_rx *u = priv->rx_buffers[i].rxp;
	u16 frame_type = status->status_fields & STATUS_TYPE_MASK;

	switch (frame_type) {
	case COMMAND_STATUS_VAL:
		return (status->frame_size != sizeof(u->rx_data.command));
	case STATUS_CHANGE_VAL:
		return (status->frame_size != sizeof(u->rx_data.status));
	case HOST_NOTIFICATION_VAL:
		return (status->frame_size < sizeof(u->rx_data.notification));
	case P80211_DATA_VAL:
	case P8023_DATA_VAL:
#ifdef CONFIG_IPW2100_MONITOR
		return 0;
#else
		switch (WLAN_FC_GET_TYPE(le16_to_cpu(u->rx_data.header.frame_ctl))) {
		case IEEE80211_FTYPE_MGMT:
		case IEEE80211_FTYPE_CTL:
			return 0;
		case IEEE80211_FTYPE_DATA:
			return (status->frame_size >
				IPW_MAX_802_11_PAYLOAD_LENGTH);
		}
#endif
	}

	return 1;
}

/*
 * ipw2100 interrupts are disabled at this point, and the ISR
 * is the only code that calls this method.  So, we do not need
 * to play with any locks.
 *
 * RX Queue works as follows:
 *
 * Read index - firmware places packet in entry identified by the
 *              Read index and advances Read index.  In this manner,
 *              Read index will always point to the next packet to
 *              be filled--but not yet valid.
 *
 * Write index - driver fills this entry with an unused RBD entry.
 *               This entry has not filled by the firmware yet.
 *
 * In between the W and R indexes are the RBDs that have been received
 * but not yet processed.
 *
 * The process of handling packets will start at WRITE + 1 and advance
 * until it reaches the READ index.
 *
 * The WRITE index is cached in the variable 'priv->rx_queue.next'.
 *
 */
static void __ipw2100_rx_process(struct ipw2100_priv *priv)
{
	struct ipw2100_bd_queue *rxq = &priv->rx_queue;
	struct ipw2100_status_queue *sq = &priv->status_queue;
	struct ipw2100_rx_packet *packet;
	u16 frame_type;
	u32 r, w, i, s;
	struct ipw2100_rx *u;
	struct libipw_rx_stats stats = {
		.mac_time = jiffies,
	};

	read_register(priv->net_dev, IPW_MEM_HOST_SHARED_RX_READ_INDEX, &r);
	read_register(priv->net_dev, IPW_MEM_HOST_SHARED_RX_WRITE_INDEX, &w);

	if (r >= rxq->entries) {
		IPW_DEBUG_RX("exit - bad read index\n");
		return;
	}

	i = (rxq->next + 1) % rxq->entries;
	s = i;
	while (i != r) {
		/* IPW_DEBUG_RX("r = %d : w = %d : processing = %d\n",
		   r, rxq->next, i); */

		packet = &priv->rx_buffers[i];

		/* Sync the DMA for the STATUS buffer so CPU is sure to get
		 * the correct values */
		pci_dma_sync_single_for_cpu(priv->pci_dev,
					    sq->nic +
					    sizeof(struct ipw2100_status) * i,
					    sizeof(struct ipw2100_status),
					    PCI_DMA_FROMDEVICE);

		/* Sync the DMA for the RX buffer so CPU is sure to get
		 * the correct values */
		pci_dma_sync_single_for_cpu(priv->pci_dev, packet->dma_addr,
					    sizeof(struct ipw2100_rx),
					    PCI_DMA_FROMDEVICE);

		if (unlikely(ipw2100_corruption_check(priv, i))) {
			ipw2100_corruption_detected(priv, i);
			goto increment;
		}

		u = packet->rxp;
		frame_type = sq->drv[i].status_fields & STATUS_TYPE_MASK;
		stats.rssi = sq->drv[i].rssi + IPW2100_RSSI_TO_DBM;
		stats.len = sq->drv[i].frame_size;

		stats.mask = 0;
		if (stats.rssi != 0)
			stats.mask |= LIBIPW_STATMASK_RSSI;
		stats.freq = LIBIPW_24GHZ_BAND;

		IPW_DEBUG_RX("%s: '%s' frame type received (%d).\n",
			     priv->net_dev->name, frame_types[frame_type],
			     stats.len);

		switch (frame_type) {
		case COMMAND_STATUS_VAL:
			/* Reset Rx watchdog */
			isr_rx_complete_command(priv, &u->rx_data.command);
			break;

		case STATUS_CHANGE_VAL:
			isr_status_change(priv, u->rx_data.status);
			break;

		case P80211_DATA_VAL:
		case P8023_DATA_VAL:
#ifdef CONFIG_IPW2100_MONITOR
			if (priv->ieee->iw_mode == IW_MODE_MONITOR) {
				isr_rx_monitor(priv, i, &stats);
				break;
			}
#endif
			if (stats.len < sizeof(struct libipw_hdr_3addr))
				break;
			switch (WLAN_FC_GET_TYPE(le16_to_cpu(u->rx_data.header.frame_ctl))) {
			case IEEE80211_FTYPE_MGMT:
				libipw_rx_mgt(priv->ieee,
						 &u->rx_data.header, &stats);
				break;

			case IEEE80211_FTYPE_CTL:
				break;

			case IEEE80211_FTYPE_DATA:
				isr_rx(priv, i, &stats);
				break;

			}
			break;
		}

	      increment:
		/* clear status field associated with this RBD */
		rxq->drv[i].status.info.field = 0;

		i = (i + 1) % rxq->entries;
	}

	if (i != s) {
		/* backtrack one entry, wrapping to end if at 0 */
		rxq->next = (i ? i : rxq->entries) - 1;

		write_register(priv->net_dev,
			       IPW_MEM_HOST_SHARED_RX_WRITE_INDEX, rxq->next);
	}
}

/*
 * __ipw2100_tx_process
 *
 * This routine will determine whether the next packet on
 * the fw_pend_list has been processed by the firmware yet.
 *
 * If not, then it does nothing and returns.
 *
 * If so, then it removes the item from the fw_pend_list, frees
 * any associated storage, and places the item back on the
 * free list of its source (either msg_free_list or tx_free_list)
 *
 * TX Queue works as follows:
 *
 * Read index - points to the next TBD that the firmware will
 *              process.  The firmware will read the data, and once
 *              done processing, it will advance the Read index.
 *
 * Write index - driver fills this entry with an constructed TBD
 *               entry.  The Write index is not advanced until the
 *               packet has been configured.
 *
 * In between the W and R indexes are the TBDs that have NOT been
 * processed.  Lagging behind the R index are packets that have
 * been processed but have not been freed by the driver.
 *
 * In order to free old storage, an internal index will be maintained
 * that points to the next packet to be freed.  When all used
 * packets have been freed, the oldest index will be the same as the
 * firmware's read index.
 *
 * The OLDEST index is cached in the variable 'priv->tx_queue.oldest'
 *
 * Because the TBD structure can not contain arbitrary data, the
 * driver must keep an internal queue of cached allocations such that
 * it can put that data back into the tx_free_list and msg_free_list
 * for use by future command and data packets.
 *
 */
static int __ipw2100_tx_process(struct ipw2100_priv *priv)
{
	struct ipw2100_bd_queue *txq = &priv->tx_queue;
	struct ipw2100_bd *tbd;
	struct list_head *element;
	struct ipw2100_tx_packet *packet;
	int descriptors_used;
	int e, i;
	u32 r, w, frag_num = 0;

	if (list_empty(&priv->fw_pend_list))
		return 0;

	element = priv->fw_pend_list.next;

	packet = list_entry(element, struct ipw2100_tx_packet, list);
	tbd = &txq->drv[packet->index];

	/* Determine how many TBD entries must be finished... */
	switch (packet->type) {
	case COMMAND:
		/* COMMAND uses only one slot; don't advance */
		descriptors_used = 1;
		e = txq->oldest;
		break;

	case DATA:
		/* DATA uses two slots; advance and loop position. */
		descriptors_used = tbd->num_fragments;
		frag_num = tbd->num_fragments - 1;
		e = txq->oldest + frag_num;
		e %= txq->entries;
		break;

	default:
		printk(KERN_WARNING DRV_NAME ": %s: Bad fw_pend_list entry!\n",
		       priv->net_dev->name);
		return 0;
	}

	/* if the last TBD is not done by NIC yet, then packet is
	 * not ready to be released.
	 *
	 */
	read_register(priv->net_dev, IPW_MEM_HOST_SHARED_TX_QUEUE_READ_INDEX,
		      &r);
	read_register(priv->net_dev, IPW_MEM_HOST_SHARED_TX_QUEUE_WRITE_INDEX,
		      &w);
	if (w != txq->next)
		printk(KERN_WARNING DRV_NAME ": %s: write index mismatch\n",
		       priv->net_dev->name);

	/*
	 * txq->next is the index of the last packet written txq->oldest is
	 * the index of the r is the index of the next packet to be read by
	 * firmware
	 */

	/*
	 * Quick graphic to help you visualize the following
	 * if / else statement
	 *
	 * ===>|                     s---->|===============
	 *                               e>|
	 * | a | b | c | d | e | f | g | h | i | j | k | l
	 *       r---->|
	 *               w
	 *
	 * w - updated by driver
	 * r - updated by firmware
	 * s - start of oldest BD entry (txq->oldest)
	 * e - end of oldest BD entry
	 *
	 */
	if (!((r <= w && (e < r || e >= w)) || (e < r && e >= w))) {
		IPW_DEBUG_TX("exit - no processed packets ready to release.\n");
		return 0;
	}

	list_del(element);
	DEC_STAT(&priv->fw_pend_stat);

#ifdef CONFIG_IPW2100_DEBUG
	{
		i = txq->oldest;
		IPW_DEBUG_TX("TX%d V=%p P=%04X T=%04X L=%d\n", i,
			     &txq->drv[i],
			     (u32) (txq->nic + i * sizeof(struct ipw2100_bd)),
			     txq->drv[i].host_addr, txq->drv[i].buf_length);

		if (packet->type == DATA) {
			i = (i + 1) % txq->entries;

			IPW_DEBUG_TX("TX%d V=%p P=%04X T=%04X L=%d\n", i,
				     &txq->drv[i],
				     (u32) (txq->nic + i *
					    sizeof(struct ipw2100_bd)),
				     (u32) txq->drv[i].host_addr,
				     txq->drv[i].buf_length);
		}
	}
#endif

	switch (packet->type) {
	case DATA:
		if (txq->drv[txq->oldest].status.info.fields.txType != 0)
			printk(KERN_WARNING DRV_NAME ": %s: Queue mismatch.  "
			       "Expecting DATA TBD but pulled "
			       "something else: ids %d=%d.\n",
			       priv->net_dev->name, txq->oldest, packet->index);

		/* DATA packet; we have to unmap and free the SKB */
		for (i = 0; i < frag_num; i++) {
			tbd = &txq->drv[(packet->index + 1 + i) % txq->entries];

			IPW_DEBUG_TX("TX%d P=%08x L=%d\n",
				     (packet->index + 1 + i) % txq->entries,
				     tbd->host_addr, tbd->buf_length);

			pci_unmap_single(priv->pci_dev,
					 tbd->host_addr,
					 tbd->buf_length, PCI_DMA_TODEVICE);
		}

		libipw_txb_free(packet->info.d_struct.txb);
		packet->info.d_struct.txb = NULL;

		list_add_tail(element, &priv->tx_free_list);
		INC_STAT(&priv->tx_free_stat);

		/* We have a free slot in the Tx queue, so wake up the
		 * transmit layer if it is stopped. */
		if (priv->status & STATUS_ASSOCIATED)
			netif_wake_queue(priv->net_dev);

		/* A packet was processed by the hardware, so update the
		 * watchdog */
		priv->net_dev->trans_start = jiffies;

		break;

	case COMMAND:
		if (txq->drv[txq->oldest].status.info.fields.txType != 1)
			printk(KERN_WARNING DRV_NAME ": %s: Queue mismatch.  "
			       "Expecting COMMAND TBD but pulled "
			       "something else: ids %d=%d.\n",
			       priv->net_dev->name, txq->oldest, packet->index);

#ifdef CONFIG_IPW2100_DEBUG
		if (packet->info.c_struct.cmd->host_command_reg <
		    ARRAY_SIZE(command_types))
			IPW_DEBUG_TX("Command '%s (%d)' processed: %d.\n",
				     command_types[packet->info.c_struct.cmd->
						   host_command_reg],
				     packet->info.c_struct.cmd->
				     host_command_reg,
				     packet->info.c_struct.cmd->cmd_status_reg);
#endif

		list_add_tail(element, &priv->msg_free_list);
		INC_STAT(&priv->msg_free_stat);
		break;
	}

	/* advance oldest used TBD pointer to start of next entry */
	txq->oldest = (e + 1) % txq->entries;
	/* increase available TBDs number */
	txq->available += descriptors_used;
	SET_STAT(&priv->txq_stat, txq->available);

	IPW_DEBUG_TX("packet latency (send to process)  %ld jiffies\n",
		     jiffies - packet->jiffy_start);

	return (!list_empty(&priv->fw_pend_list));
}

static inline void __ipw2100_tx_complete(struct ipw2100_priv *priv)
{
	int i = 0;

	while (__ipw2100_tx_process(priv) && i < 200)
		i++;

	if (i == 200) {
		printk(KERN_WARNING DRV_NAME ": "
		       "%s: Driver is running slow (%d iters).\n",
		       priv->net_dev->name, i);
	}
}

static void ipw2100_tx_send_commands(struct ipw2100_priv *priv)
{
	struct list_head *element;
	struct ipw2100_tx_packet *packet;
	struct ipw2100_bd_queue *txq = &priv->tx_queue;
	struct ipw2100_bd *tbd;
	int next = txq->next;

	while (!list_empty(&priv->msg_pend_list)) {
		/* if there isn't enough space in TBD queue, then
		 * don't stuff a new one in.
		 * NOTE: 3 are needed as a command will take one,
		 *       and there is a minimum of 2 that must be
		 *       maintained between the r and w indexes
		 */
		if (txq->available <= 3) {
			IPW_DEBUG_TX("no room in tx_queue\n");
			break;
		}

		element = priv->msg_pend_list.next;
		list_del(element);
		DEC_STAT(&priv->msg_pend_stat);

		packet = list_entry(element, struct ipw2100_tx_packet, list);

		IPW_DEBUG_TX("using TBD at virt=%p, phys=%p\n",
			     &txq->drv[txq->next],
			     (void *)(txq->nic + txq->next *
				      sizeof(struct ipw2100_bd)));

		packet->index = txq->next;

		tbd = &txq->drv[txq->next];

		/* initialize TBD */
		tbd->host_addr = packet->info.c_struct.cmd_phys;
		tbd->buf_length = sizeof(struct ipw2100_cmd_header);
		/* not marking number of fragments causes problems
		 * with f/w debug version */
		tbd->num_fragments = 1;
		tbd->status.info.field =
		    IPW_BD_STATUS_TX_FRAME_COMMAND |
		    IPW_BD_STATUS_TX_INTERRUPT_ENABLE;

		/* update TBD queue counters */
		txq->next++;
		txq->next %= txq->entries;
		txq->available--;
		DEC_STAT(&priv->txq_stat);

		list_add_tail(element, &priv->fw_pend_list);
		INC_STAT(&priv->fw_pend_stat);
	}

	if (txq->next != next) {
		/* kick off the DMA by notifying firmware the
		 * write index has moved; make sure TBD stores are sync'd */
		wmb();
		write_register(priv->net_dev,
			       IPW_MEM_HOST_SHARED_TX_QUEUE_WRITE_INDEX,
			       txq->next);
	}
}

/*
 * ipw2100_tx_send_data
 *
 */
static void ipw2100_tx_send_data(struct ipw2100_priv *priv)
{
	struct list_head *element;
	struct ipw2100_tx_packet *packet;
	struct ipw2100_bd_queue *txq = &priv->tx_queue;
	struct ipw2100_bd *tbd;
	int next = txq->next;
	int i = 0;
	struct ipw2100_data_header *ipw_hdr;
	struct libipw_hdr_3addr *hdr;

	while (!list_empty(&priv->tx_pend_list)) {
		/* if there isn't enough space in TBD queue, then
		 * don't stuff a new one in.
		 * NOTE: 4 are needed as a data will take two,
		 *       and there is a minimum of 2 that must be
		 *       maintained between the r and w indexes
		 */
		element = priv->tx_pend_list.next;
		packet = list_entry(element, struct ipw2100_tx_packet, list);

		if (unlikely(1 + packet->info.d_struct.txb->nr_frags >
			     IPW_MAX_BDS)) {
			/* TODO: Support merging buffers if more than
			 * IPW_MAX_BDS are used */
			IPW_DEBUG_INFO("%s: Maximum BD theshold exceeded.  "
				       "Increase fragmentation level.\n",
				       priv->net_dev->name);
		}

		if (txq->available <= 3 + packet->info.d_struct.txb->nr_frags) {
			IPW_DEBUG_TX("no room in tx_queue\n");
			break;
		}

		list_del(element);
		DEC_STAT(&priv->tx_pend_stat);

		tbd = &txq->drv[txq->next];

		packet->index = txq->next;

		ipw_hdr = packet->info.d_struct.data;
		hdr = (struct libipw_hdr_3addr *)packet->info.d_struct.txb->
		    fragments[0]->data;

		if (priv->ieee->iw_mode == IW_MODE_INFRA) {
			/* To DS: Addr1 = BSSID, Addr2 = SA,
			   Addr3 = DA */
			memcpy(ipw_hdr->src_addr, hdr->addr2, ETH_ALEN);
			memcpy(ipw_hdr->dst_addr, hdr->addr3, ETH_ALEN);
		} else if (priv->ieee->iw_mode == IW_MODE_ADHOC) {
			/* not From/To DS: Addr1 = DA, Addr2 = SA,
			   Addr3 = BSSID */
			memcpy(ipw_hdr->src_addr, hdr->addr2, ETH_ALEN);
			memcpy(ipw_hdr->dst_addr, hdr->addr1, ETH_ALEN);
		}

		ipw_hdr->host_command_reg = SEND;
		ipw_hdr->host_command_reg1 = 0;

		/* For now we only support host based encryption */
		ipw_hdr->needs_encryption = 0;
		ipw_hdr->encrypted = packet->info.d_struct.txb->encrypted;
		if (packet->info.d_struct.txb->nr_frags > 1)
			ipw_hdr->fragment_size =
			    packet->info.d_struct.txb->frag_size -
			    LIBIPW_3ADDR_LEN;
		else
			ipw_hdr->fragment_size = 0;

		tbd->host_addr = packet->info.d_struct.data_phys;
		tbd->buf_length = sizeof(struct ipw2100_data_header);
		tbd->num_fragments = 1 + packet->info.d_struct.txb->nr_frags;
		tbd->status.info.field =
		    IPW_BD_STATUS_TX_FRAME_802_3 |
		    IPW_BD_STATUS_TX_FRAME_NOT_LAST_FRAGMENT;
		txq->next++;
		txq->next %= txq->entries;

		IPW_DEBUG_TX("data header tbd TX%d P=%08x L=%d\n",
			     packet->index, tbd->host_addr, tbd->buf_length);
#ifdef CONFIG_IPW2100_DEBUG
		if (packet->info.d_struct.txb->nr_frags > 1)
			IPW_DEBUG_FRAG("fragment Tx: %d frames\n",
				       packet->info.d_struct.txb->nr_frags);
#endif

		for (i = 0; i < packet->info.d_struct.txb->nr_frags; i++) {
			tbd = &txq->drv[txq->next];
			if (i == packet->info.d_struct.txb->nr_frags - 1)
				tbd->status.info.field =
				    IPW_BD_STATUS_TX_FRAME_802_3 |
				    IPW_BD_STATUS_TX_INTERRUPT_ENABLE;
			else
				tbd->status.info.field =
				    IPW_BD_STATUS_TX_FRAME_802_3 |
				    IPW_BD_STATUS_TX_FRAME_NOT_LAST_FRAGMENT;

			tbd->buf_length = packet->info.d_struct.txb->
			    fragments[i]->len - LIBIPW_3ADDR_LEN;

			tbd->host_addr = pci_map_single(priv->pci_dev,
							packet->info.d_struct.
							txb->fragments[i]->
							data +
							LIBIPW_3ADDR_LEN,
							tbd->buf_length,
							PCI_DMA_TODEVICE);

			IPW_DEBUG_TX("data frag tbd TX%d P=%08x L=%d\n",
				     txq->next, tbd->host_addr,
				     tbd->buf_length);

			pci_dma_sync_single_for_device(priv->pci_dev,
						       tbd->host_addr,
						       tbd->buf_length,
						       PCI_DMA_TODEVICE);

			txq->next++;
			txq->next %= txq->entries;
		}

		txq->available -= 1 + packet->info.d_struct.txb->nr_frags;
		SET_STAT(&priv->txq_stat, txq->available);

		list_add_tail(element, &priv->fw_pend_list);
		INC_STAT(&priv->fw_pend_stat);
	}

	if (txq->next != next) {
		/* kick off the DMA by notifying firmware the
		 * write index has moved; make sure TBD stores are sync'd */
		write_register(priv->net_dev,
			       IPW_MEM_HOST_SHARED_TX_QUEUE_WRITE_INDEX,
			       txq->next);
	}
	return;
}

static void ipw2100_irq_tasklet(struct ipw2100_priv *priv)
{
	struct net_device *dev = priv->net_dev;
	unsigned long flags;
	u32 inta, tmp;

	spin_lock_irqsave(&priv->low_lock, flags);
	ipw2100_disable_interrupts(priv);

	read_register(dev, IPW_REG_INTA, &inta);

	IPW_DEBUG_ISR("enter - INTA: 0x%08lX\n",
		      (unsigned long)inta & IPW_INTERRUPT_MASK);

	priv->in_isr++;
	priv->interrupts++;

	/* We do not loop and keep polling for more interrupts as this
	 * is frowned upon and doesn't play nicely with other potentially
	 * chained IRQs */
	IPW_DEBUG_ISR("INTA: 0x%08lX\n",
		      (unsigned long)inta & IPW_INTERRUPT_MASK);

	if (inta & IPW2100_INTA_FATAL_ERROR) {
		printk(KERN_WARNING DRV_NAME
		       ": Fatal interrupt. Scheduling firmware restart.\n");
		priv->inta_other++;
		write_register(dev, IPW_REG_INTA, IPW2100_INTA_FATAL_ERROR);

		read_nic_dword(dev, IPW_NIC_FATAL_ERROR, &priv->fatal_error);
		IPW_DEBUG_INFO("%s: Fatal error value: 0x%08X\n",
			       priv->net_dev->name, priv->fatal_error);

		read_nic_dword(dev, IPW_ERROR_ADDR(priv->fatal_error), &tmp);
		IPW_DEBUG_INFO("%s: Fatal error address value: 0x%08X\n",
			       priv->net_dev->name, tmp);

		/* Wake up any sleeping jobs */
		schedule_reset(priv);
	}

	if (inta & IPW2100_INTA_PARITY_ERROR) {
		printk(KERN_ERR DRV_NAME
		       ": ***** PARITY ERROR INTERRUPT !!!! \n");
		priv->inta_other++;
		write_register(dev, IPW_REG_INTA, IPW2100_INTA_PARITY_ERROR);
	}

	if (inta & IPW2100_INTA_RX_TRANSFER) {
		IPW_DEBUG_ISR("RX interrupt\n");

		priv->rx_interrupts++;

		write_register(dev, IPW_REG_INTA, IPW2100_INTA_RX_TRANSFER);

		__ipw2100_rx_process(priv);
		__ipw2100_tx_complete(priv);
	}

	if (inta & IPW2100_INTA_TX_TRANSFER) {
		IPW_DEBUG_ISR("TX interrupt\n");

		priv->tx_interrupts++;

		write_register(dev, IPW_REG_INTA, IPW2100_INTA_TX_TRANSFER);

		__ipw2100_tx_complete(priv);
		ipw2100_tx_send_commands(priv);
		ipw2100_tx_send_data(priv);
	}

	if (inta & IPW2100_INTA_TX_COMPLETE) {
		IPW_DEBUG_ISR("TX complete\n");
		priv->inta_other++;
		write_register(dev, IPW_REG_INTA, IPW2100_INTA_TX_COMPLETE);

		__ipw2100_tx_complete(priv);
	}

	if (inta & IPW2100_INTA_EVENT_INTERRUPT) {
		/* ipw2100_handle_event(dev); */
		priv->inta_other++;
		write_register(dev, IPW_REG_INTA, IPW2100_INTA_EVENT_INTERRUPT);
	}

	if (inta & IPW2100_INTA_FW_INIT_DONE) {
		IPW_DEBUG_ISR("FW init done interrupt\n");
		priv->inta_other++;

		read_register(dev, IPW_REG_INTA, &tmp);
		if (tmp & (IPW2100_INTA_FATAL_ERROR |
			   IPW2100_INTA_PARITY_ERROR)) {
			write_register(dev, IPW_REG_INTA,
				       IPW2100_INTA_FATAL_ERROR |
				       IPW2100_INTA_PARITY_ERROR);
		}

		write_register(dev, IPW_REG_INTA, IPW2100_INTA_FW_INIT_DONE);
	}

	if (inta & IPW2100_INTA_STATUS_CHANGE) {
		IPW_DEBUG_ISR("Status change interrupt\n");
		priv->inta_other++;
		write_register(dev, IPW_REG_INTA, IPW2100_INTA_STATUS_CHANGE);
	}

	if (inta & IPW2100_INTA_SLAVE_MODE_HOST_COMMAND_DONE) {
		IPW_DEBUG_ISR("slave host mode interrupt\n");
		priv->inta_other++;
		write_register(dev, IPW_REG_INTA,
			       IPW2100_INTA_SLAVE_MODE_HOST_COMMAND_DONE);
	}

	priv->in_isr--;
	ipw2100_enable_interrupts(priv);

	spin_unlock_irqrestore(&priv->low_lock, flags);

	IPW_DEBUG_ISR("exit\n");
}

static irqreturn_t ipw2100_interrupt(int irq, void *data)
{
	struct ipw2100_priv *priv = data;
	u32 inta, inta_mask;

	if (!data)
		return IRQ_NONE;

	spin_lock(&priv->low_lock);

	/* We check to see if we should be ignoring interrupts before
	 * we touch the hardware.  During ucode load if we try and handle
	 * an interrupt we can cause keyboard problems as well as cause
	 * the ucode to fail to initialize */
	if (!(priv->status & STATUS_INT_ENABLED)) {
		/* Shared IRQ */
		goto none;
	}

	read_register(priv->net_dev, IPW_REG_INTA_MASK, &inta_mask);
	read_register(priv->net_dev, IPW_REG_INTA, &inta);

	if (inta == 0xFFFFFFFF) {
		/* Hardware disappeared */
		printk(KERN_WARNING DRV_NAME ": IRQ INTA == 0xFFFFFFFF\n");
		goto none;
	}

	inta &= IPW_INTERRUPT_MASK;

	if (!(inta & inta_mask)) {
		/* Shared interrupt */
		goto none;
	}

	/* We disable the hardware interrupt here just to prevent unneeded
	 * calls to be made.  We disable this again within the actual
	 * work tasklet, so if another part of the code re-enables the
	 * interrupt, that is fine */
	ipw2100_disable_interrupts(priv);

	tasklet_schedule(&priv->irq_tasklet);
	spin_unlock(&priv->low_lock);

	return IRQ_HANDLED;
      none:
	spin_unlock(&priv->low_lock);
	return IRQ_NONE;
}

static netdev_tx_t ipw2100_tx(struct libipw_txb *txb,
			      struct net_device *dev, int pri)
{
	struct ipw2100_priv *priv = libipw_priv(dev);
	struct list_head *element;
	struct ipw2100_tx_packet *packet;
	unsigned long flags;

	spin_lock_irqsave(&priv->low_lock, flags);

	if (!(priv->status & STATUS_ASSOCIATED)) {
		IPW_DEBUG_INFO("Can not transmit when not connected.\n");
		priv->net_dev->stats.tx_carrier_errors++;
		netif_stop_queue(dev);
		goto fail_unlock;
	}

	if (list_empty(&priv->tx_free_list))
		goto fail_unlock;

	element = priv->tx_free_list.next;
	packet = list_entry(element, struct ipw2100_tx_packet, list);

	packet->info.d_struct.txb = txb;

	IPW_DEBUG_TX("Sending fragment (%d bytes):\n", txb->fragments[0]->len);
	printk_buf(IPW_DL_TX, txb->fragments[0]->data, txb->fragments[0]->len);

	packet->jiffy_start = jiffies;

	list_del(element);
	DEC_STAT(&priv->tx_free_stat);

	list_add_tail(element, &priv->tx_pend_list);
	INC_STAT(&priv->tx_pend_stat);

	ipw2100_tx_send_data(priv);

	spin_unlock_irqrestore(&priv->low_lock, flags);
	return NETDEV_TX_OK;

fail_unlock:
	netif_stop_queue(dev);
	spin_unlock_irqrestore(&priv->low_lock, flags);
	return NETDEV_TX_BUSY;
}

static int ipw2100_msg_allocate(struct ipw2100_priv *priv)
{
	int i, j, err = -EINVAL;
	void *v;
	dma_addr_t p;

	priv->msg_buffers =
	    (struct ipw2100_tx_packet *)kmalloc(IPW_COMMAND_POOL_SIZE *
						sizeof(struct
						       ipw2100_tx_packet),
						GFP_KERNEL);
	if (!priv->msg_buffers) {
		printk(KERN_ERR DRV_NAME ": %s: PCI alloc failed for msg "
		       "buffers.\n", priv->net_dev->name);
		return -ENOMEM;
	}

	for (i = 0; i < IPW_COMMAND_POOL_SIZE; i++) {
		v = pci_alloc_consistent(priv->pci_dev,
					 sizeof(struct ipw2100_cmd_header), &p);
		if (!v) {
			printk(KERN_ERR DRV_NAME ": "
			       "%s: PCI alloc failed for msg "
			       "buffers.\n", priv->net_dev->name);
			err = -ENOMEM;
			break;
		}

		memset(v, 0, sizeof(struct ipw2100_cmd_header));

		priv->msg_buffers[i].type = COMMAND;
		priv->msg_buffers[i].info.c_struct.cmd =
		    (struct ipw2100_cmd_header *)v;
		priv->msg_buffers[i].info.c_struct.cmd_phys = p;
	}

	if (i == IPW_COMMAND_POOL_SIZE)
		return 0;

	for (j = 0; j < i; j++) {
		pci_free_consistent(priv->pci_dev,
				    sizeof(struct ipw2100_cmd_header),
				    priv->msg_buffers[j].info.c_struct.cmd,
				    priv->msg_buffers[j].info.c_struct.
				    cmd_phys);
	}

	kfree(priv->msg_buffers);
	priv->msg_buffers = NULL;

	return err;
}

static int ipw2100_msg_initialize(struct ipw2100_priv *priv)
{
	int i;

	INIT_LIST_HEAD(&priv->msg_free_list);
	INIT_LIST_HEAD(&priv->msg_pend_list);

	for (i = 0; i < IPW_COMMAND_POOL_SIZE; i++)
		list_add_tail(&priv->msg_buffers[i].list, &priv->msg_free_list);
	SET_STAT(&priv->msg_free_stat, i);

	return 0;
}

static void ipw2100_msg_free(struct ipw2100_priv *priv)
{
	int i;

	if (!priv->msg_buffers)
		return;

	for (i = 0; i < IPW_COMMAND_POOL_SIZE; i++) {
		pci_free_consistent(priv->pci_dev,
				    sizeof(struct ipw2100_cmd_header),
				    priv->msg_buffers[i].info.c_struct.cmd,
				    priv->msg_buffers[i].info.c_struct.
				    cmd_phys);
	}

	kfree(priv->msg_buffers);
	priv->msg_buffers = NULL;
}

static ssize_t show_pci(struct device *d, struct device_attribute *attr,
			char *buf)
{
	struct pci_dev *pci_dev = container_of(d, struct pci_dev, dev);
	char *out = buf;
	int i, j;
	u32 val;

	for (i = 0; i < 16; i++) {
		out += sprintf(out, "[%08X] ", i * 16);
		for (j = 0; j < 16; j += 4) {
			pci_read_config_dword(pci_dev, i * 16 + j, &val);
			out += sprintf(out, "%08X ", val);
		}
		out += sprintf(out, "\n");
	}

	return out - buf;
}

static DEVICE_ATTR(pci, S_IRUGO, show_pci, NULL);

static ssize_t show_cfg(struct device *d, struct device_attribute *attr,
			char *buf)
{
	struct ipw2100_priv *p = dev_get_drvdata(d);
	return sprintf(buf, "0x%08x\n", (int)p->config);
}

static DEVICE_ATTR(cfg, S_IRUGO, show_cfg, NULL);

static ssize_t show_status(struct device *d, struct device_attribute *attr,
			   char *buf)
{
	struct ipw2100_priv *p = dev_get_drvdata(d);
	return sprintf(buf, "0x%08x\n", (int)p->status);
}

static DEVICE_ATTR(status, S_IRUGO, show_status, NULL);

static ssize_t show_capability(struct device *d, struct device_attribute *attr,
			       char *buf)
{
	struct ipw2100_priv *p = dev_get_drvdata(d);
	return sprintf(buf, "0x%08x\n", (int)p->capability);
}

static DEVICE_ATTR(capability, S_IRUGO, show_capability, NULL);

#define IPW2100_REG(x) { IPW_ ##x, #x }
static const struct {
	u32 addr;
	const char *name;
} hw_data[] = {
IPW2100_REG(REG_GP_CNTRL),
	    IPW2100_REG(REG_GPIO),
	    IPW2100_REG(REG_INTA),
	    IPW2100_REG(REG_INTA_MASK), IPW2100_REG(REG_RESET_REG),};
#define IPW2100_NIC(x, s) { x, #x, s }
static const struct {
	u32 addr;
	const char *name;
	size_t size;
} nic_data[] = {
IPW2100_NIC(IPW2100_CONTROL_REG, 2),
	    IPW2100_NIC(0x210014, 1), IPW2100_NIC(0x210000, 1),};
#define IPW2100_ORD(x, d) { IPW_ORD_ ##x, #x, d }
static const struct {
	u8 index;
	const char *name;
	const char *desc;
} ord_data[] = {
IPW2100_ORD(STAT_TX_HOST_REQUESTS, "requested Host Tx's (MSDU)"),
	    IPW2100_ORD(STAT_TX_HOST_COMPLETE,
				"successful Host Tx's (MSDU)"),
	    IPW2100_ORD(STAT_TX_DIR_DATA,
				"successful Directed Tx's (MSDU)"),
	    IPW2100_ORD(STAT_TX_DIR_DATA1,
				"successful Directed Tx's (MSDU) @ 1MB"),
	    IPW2100_ORD(STAT_TX_DIR_DATA2,
				"successful Directed Tx's (MSDU) @ 2MB"),
	    IPW2100_ORD(STAT_TX_DIR_DATA5_5,
				"successful Directed Tx's (MSDU) @ 5_5MB"),
	    IPW2100_ORD(STAT_TX_DIR_DATA11,
				"successful Directed Tx's (MSDU) @ 11MB"),
	    IPW2100_ORD(STAT_TX_NODIR_DATA1,
				"successful Non_Directed Tx's (MSDU) @ 1MB"),
	    IPW2100_ORD(STAT_TX_NODIR_DATA2,
				"successful Non_Directed Tx's (MSDU) @ 2MB"),
	    IPW2100_ORD(STAT_TX_NODIR_DATA5_5,
				"successful Non_Directed Tx's (MSDU) @ 5.5MB"),
	    IPW2100_ORD(STAT_TX_NODIR_DATA11,
				"successful Non_Directed Tx's (MSDU) @ 11MB"),
	    IPW2100_ORD(STAT_NULL_DATA, "successful NULL data Tx's"),
	    IPW2100_ORD(STAT_TX_RTS, "successful Tx RTS"),
	    IPW2100_ORD(STAT_TX_CTS, "successful Tx CTS"),
	    IPW2100_ORD(STAT_TX_ACK, "successful Tx ACK"),
	    IPW2100_ORD(STAT_TX_ASSN, "successful Association Tx's"),
	    IPW2100_ORD(STAT_TX_ASSN_RESP,
				"successful Association response Tx's"),
	    IPW2100_ORD(STAT_TX_REASSN,
				"successful Reassociation Tx's"),
	    IPW2100_ORD(STAT_TX_REASSN_RESP,
				"successful Reassociation response Tx's"),
	    IPW2100_ORD(STAT_TX_PROBE,
				"probes successfully transmitted"),
	    IPW2100_ORD(STAT_TX_PROBE_RESP,
				"probe responses successfully transmitted"),
	    IPW2100_ORD(STAT_TX_BEACON, "tx beacon"),
	    IPW2100_ORD(STAT_TX_ATIM, "Tx ATIM"),
	    IPW2100_ORD(STAT_TX_DISASSN,
				"successful Disassociation TX"),
	    IPW2100_ORD(STAT_TX_AUTH, "successful Authentication Tx"),
	    IPW2100_ORD(STAT_TX_DEAUTH,
				"successful Deauthentication TX"),
	    IPW2100_ORD(STAT_TX_TOTAL_BYTES,
				"Total successful Tx data bytes"),
	    IPW2100_ORD(STAT_TX_RETRIES, "Tx retries"),
	    IPW2100_ORD(STAT_TX_RETRY1, "Tx retries at 1MBPS"),
	    IPW2100_ORD(STAT_TX_RETRY2, "Tx retries at 2MBPS"),
	    IPW2100_ORD(STAT_TX_RETRY5_5, "Tx retries at 5.5MBPS"),
	    IPW2100_ORD(STAT_TX_RETRY11, "Tx retries at 11MBPS"),
	    IPW2100_ORD(STAT_TX_FAILURES, "Tx Failures"),
	    IPW2100_ORD(STAT_TX_MAX_TRIES_IN_HOP,
				"times max tries in a hop failed"),
	    IPW2100_ORD(STAT_TX_DISASSN_FAIL,
				"times disassociation failed"),
	    IPW2100_ORD(STAT_TX_ERR_CTS, "missed/bad CTS frames"),
	    IPW2100_ORD(STAT_TX_ERR_ACK, "tx err due to acks"),
	    IPW2100_ORD(STAT_RX_HOST, "packets passed to host"),
	    IPW2100_ORD(STAT_RX_DIR_DATA, "directed packets"),
	    IPW2100_ORD(STAT_RX_DIR_DATA1, "directed packets at 1MB"),
	    IPW2100_ORD(STAT_RX_DIR_DATA2, "directed packets at 2MB"),
	    IPW2100_ORD(STAT_RX_DIR_DATA5_5,
				"directed packets at 5.5MB"),
	    IPW2100_ORD(STAT_RX_DIR_DATA11, "directed packets at 11MB"),
	    IPW2100_ORD(STAT_RX_NODIR_DATA, "nondirected packets"),
	    IPW2100_ORD(STAT_RX_NODIR_DATA1,
				"nondirected packets at 1MB"),
	    IPW2100_ORD(STAT_RX_NODIR_DATA2,
				"nondirected packets at 2MB"),
	    IPW2100_ORD(STAT_RX_NODIR_DATA5_5,
				"nondirected packets at 5.5MB"),
	    IPW2100_ORD(STAT_RX_NODIR_DATA11,
				"nondirected packets at 11MB"),
	    IPW2100_ORD(STAT_RX_NULL_DATA, "null data rx's"),
	    IPW2100_ORD(STAT_RX_RTS, "Rx RTS"), IPW2100_ORD(STAT_RX_CTS,
								    "Rx CTS"),
	    IPW2100_ORD(STAT_RX_ACK, "Rx ACK"),
	    IPW2100_ORD(STAT_RX_CFEND, "Rx CF End"),
	    IPW2100_ORD(STAT_RX_CFEND_ACK, "Rx CF End + CF Ack"),
	    IPW2100_ORD(STAT_RX_ASSN, "Association Rx's"),
	    IPW2100_ORD(STAT_RX_ASSN_RESP, "Association response Rx's"),
	    IPW2100_ORD(STAT_RX_REASSN, "Reassociation Rx's"),
	    IPW2100_ORD(STAT_RX_REASSN_RESP,
				"Reassociation response Rx's"),
	    IPW2100_ORD(STAT_RX_PROBE, "probe Rx's"),
	    IPW2100_ORD(STAT_RX_PROBE_RESP, "probe response Rx's"),
	    IPW2100_ORD(STAT_RX_BEACON, "Rx beacon"),
	    IPW2100_ORD(STAT_RX_ATIM, "Rx ATIM"),
	    IPW2100_ORD(STAT_RX_DISASSN, "disassociation Rx"),
	    IPW2100_ORD(STAT_RX_AUTH, "authentication Rx"),
	    IPW2100_ORD(STAT_RX_DEAUTH, "deauthentication Rx"),
	    IPW2100_ORD(STAT_RX_TOTAL_BYTES,
				"Total rx data bytes received"),
	    IPW2100_ORD(STAT_RX_ERR_CRC, "packets with Rx CRC error"),
	    IPW2100_ORD(STAT_RX_ERR_CRC1, "Rx CRC errors at 1MB"),
	    IPW2100_ORD(STAT_RX_ERR_CRC2, "Rx CRC errors at 2MB"),
	    IPW2100_ORD(STAT_RX_ERR_CRC5_5, "Rx CRC errors at 5.5MB"),
	    IPW2100_ORD(STAT_RX_ERR_CRC11, "Rx CRC errors at 11MB"),
	    IPW2100_ORD(STAT_RX_DUPLICATE1,
				"duplicate rx packets at 1MB"),
	    IPW2100_ORD(STAT_RX_DUPLICATE2,
				"duplicate rx packets at 2MB"),
	    IPW2100_ORD(STAT_RX_DUPLICATE5_5,
				"duplicate rx packets at 5.5MB/*************************************11*******************************11****

  Copyright(c) 2003 - 2006 Intel C, *********************/********************PERS_DB_LOCK, "locking fw permanent  dbmodify it
  under the terms oSIZ redsize of the GNU General Public License as
  publishedADDR, "addresse Software Foundation.

  This program is********INVALID_PROTOCOL*******rx frames with invalid protocol/*********************YS_BOOT_TIM redBoot time/****************************NO_BUFFERen the implied warejected due to no buffer/****************************MISSING_FRAGen the implied wadroppshould havmiss of tragment/****************************ORPHANicense along with
  this program; if non-sequentialite to the Free Software Foundation, Inc., 59
  MEe along with
  this program; if unmatched 1stplied /****************************cens_AGEOUTincluded in this distribution in tcomple sholled LICENSE.

  Contact InformatiICV_ERRORS*******ICV errors dur of decryption/*************************PSP_SUSPENSION, ".  Ss adapter suspendedns of this file are based on tBCNCULARtel  "beaconE.  Souhe Free Software Foundationon tPOLLcopyright******poll response7-2003 J modify it
  under thehpl.hp.coNONDIR  Portions of tased on  wait of tor last {broad,multi}cty CpkJean Tourrilhes
  <jt@hpl.hp.co****TIMS, "PSP 02-2

  ceivless
  Extensions 0.26 package aRXCULA003, Joui Malinen <j@w1.fi>

  Portions of ipw2100_****ION_ID03, JouStartio ID/********************LAST_ASS copyr, "RTCE.  Se Soity Cassociooselean Tourrilhes
  <jt@hpl.hpERCENTPubliEDand , Hillsbcurrenercalculooselyof % not,ed c) 199 the Host AP project,
  Copyright (RETRIEx

*************************************tx retrie modify it
  under theASSOCIATED_AP_PT
  more 0 if no the 2.4.2ed, else poinded to AP table entryed by Janusz Gorycki,
VAILABLErbanCNons of tAP's7

 sribed in thesupport aed by Janusz Gorycki,
P_LISTniak, "Ptode slist****availrt adAP the Host AP project,
  CopAPnd_fi003,he 2.4.25 kmit Buffer Descriptors (TBDs_firFAILTBD contains a failuroped by Janusz Gorycki,al (dma_adRESPddr_t)******ta beingould havile are bta b/*************************FULL_SCAh TBDfull scaa pointer to the physicCARD_DISciejloadCard Disrt aess
  Extensions 0.26 packaROAM_INHIBI002, SSH Coms roam of was inhibi should havactivitby Jacek Wysoczynski aRSSI_ (dma_agth of  REAr queowski.

 suppaSE.  She currently5 kernel sources, and are copma_adCAUSErporatiorehe 2.4.25 k:ve rprobehost writeor TX on hopvanced once the firmware is
done wit2 a packet.

When data poor tx/rx qual at the READ index.  Theare is
done wit3 a packet.

When data a is being se (excessivx - Cloaess
  Extensions 0.26 packas
done wit4 a packet.

When data AP to thleveo the TBD queue at the WRITs
done wit********et.

When data.

The haddrs the ingtype of data packet, numberUTHddr_t) ailes pruth11-1c of data b is advanced when after the 
The s the length of  cycle is as follows:ost writes to is advanced when after the0_fwTciej aniak.

Thded iepullthe 2.4.25 kCommands and Data

Firmware READ VG_CURsharC*******avgicatemodify it
  under the OWER_MGMT_MODre.cPower mode - 0=CAM, 1=PSPex points
to the _nextOUNTRY_Cing HillsboEEE coued b c theaalinev'tel om*******e, the incoming SKB is DMA mapHANNELx

******hannels supory bebaddrcal awork is scheduled to mESETsmit in tvided res****(warm)/********************BEAC_fw_NTERVAt) aB) 1997us movao the TBD queue at theANTENNA_DIVERSITY
   to TRUEciejantenna diversthinis dn and is advanced when after02-2****IOloadc) 1997d_list as betweenuni Ma/********************OUR_FREQ

*************radio freq lket idigits - t.  They based on drivers/soundRTCCULAR PU********
  availared circular queue.
4)RT_TYPre.coperaons Sn thex points
to the _nextURRht (TX_Ran red********as dat LICENSE.

  Contact InUProceEDR pul003,supst indist packped by Janusz Gorycki,
TE iWINDOWls the oldelist Windowoad data.
5) the packeASIC0)The pacbasicture contained in the fw_pend_NIC_HIGHESTR pulls NIC highesdest packet
   from the fw_penAPthe kernel.
11)TAPpacket structure is placed onto the tCAPABILIT

 InitialManageo thel Cor capabiithinfieless
  Extensions 0.26 
The sed.
9)Typentry,is as followsware, the READ index iADIOlist

..Ao the
 crittplatform typ work is scheduled to mTS_THRESHOLDfree_lisin******* lengthSecurRTS handsha2 of/********************It (cing paIs monis adal TBD that has been processcensMEN to the lock)
that protCHANTABIite t threshoad of tx_free_list/tx_pEEPROM_SRAMms oBf vepw21RTbutedES, Hillsbtx_pro offsetst (ess(odified in __ipw2100_tx_process()
    HEAD by tpw2100_tx()

 Freend_list : Holds used Tx buffers waitKU_nly the mY, 0_tx()

SKU Cre used iodified in __ipw2100_tx_procIBSS_11Bare
   filled ou_tx()

ated 11bom the fisehe Free Software FoundMAC_firmwe_* fMAC Vst)
 / Locking :

There are   HREVI modified iRevipw2100_hw_send_command()
 two lEAD modifiRTx inn ipw2100_hw_send_command()
om tMANF_DATECULAR PU_hw_ Date/Td enSTAMue, the incoming SKB iUppedgo into theUdressn ipw2100_};

sloosc s Fre_t show_registers(struct device *d, FREE_LIST + C_attribute *LISTe TBD**** om tr *buf)
{
	int i;
	FREE_LIipw*****priv *IST ==IST _get_drvdata(d) DATA => TnREE_T + COMMeD =>IST -> that w;
	ST
  Tout =rece;
	u32 val = 0;

	 via+= sprintf(out, "%30s [Athe hop] : Hex\n"the :

  MS") Theecur(i.

-  i < ARRAYto th(hw_ST

); i++) {
		readws:

  MS(dev,  the BD[i]. the, &valThe e internal data state of the%08Xitsev->a- AcSG_FREE_LI

All externnamec

All external enty funct}

	return  via-priv->}e TX sideDEVICE_ATTR(s:

  MSG, S_IRUGO,follows:

  MSG, NULL firTX side is as followhardware_FREE_LIST + COMMAND => MSG_PEND_LIST => TBD => MSG_FREE_LST
  TX_FREE_LTA => TX_PEND_LIST => TBD => TX_FREE_LIST

  The methods that work on the TBD ring are protected via priv->lIST + Dhe internal data state of the device itself
- Accehe pded by firmware read/write indexes forniche BD queues
  anu8 tmp8nctiu16cess16h>
#i32cess32 <li	switch ff.h>
#intern Fres
  ancase 1:****d assf.h>bytelogic
fs.h>
#inclual entress.unctiions are locked with the priv->action_2ock to ennsure
thfs.h>
#inclu exter#include <linux/ucp.h>
#inclustd.h>
#inbreaknctimm.h>2#include <linuwordab.h>
#include <linux/unistd16h>
#include <linux/stringify.h>
#include <l4nux/tcp.h>
#include <linux/types.h>
#include <linux/time.h>
#includ>
#inclux/firmware.h>
4#include <linud/acpi.h>
#include <linux/ctype.h32h>
#include <linux/stringify.h>
#include <llock to en>
#include <linux/types.h>
#include <linux/time.h>
#includN
#definx/firmwar}at atime.


*/

#include <linux/compiler.h>
#x/in6.h>linux/errno.h>
#100_DEBUG
nux/if_arp.h>
#include <linumemory_FREE_LIST + COMMAND => MSG_PEND_LIST => TBD => MSG_FREEux/kernel.h>
#include <linux/kmod.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <liTX sideunsigned long loop.

- Tncludlenannel =io.heceive[4]#include <otecteline[81] <liif (t cha>= 0x30000)
		t channel 
	/* sysfs senvides us PAGEto thsociate */
	while (stat<re;
#endif
- 128 &&nt cha<G_PM
stat {ludeifdeTBD risnapshot[0]tic mware read/write 4queues*****ociate i] =00 Networ*(io.h*) SNAPSHOified ef CON+ i * 4unctiomise, network_mode, int, 0444);
modpw2100"
#define DRV_(associate, , &ule_paramproc_
module_pardump_raw);
mostaternal data buf +
sta/time.h>
#inc"%cMODULE"=Monitor)");
MODULE_PARM_DESC(channel, "channel");
MODULE_PARM_DES/time.h>
#inc((u8444)eceive)[0x0]ociate when scanning (default o1f)");
MODULE_PARM_DESC(disable, 2f)");
MODULE_PARM_DESC(disable, 3f)");
MODULE_PARM_DESC(disable, 4f)");
MODULE_PARM_DESC(disable, 5f)");
MODULE_PARM_DESC(disable, 6f)");
MODULE_PARM_DESC(disable, 7f)");
MODULE_PARM_DESC(disable, 8f)");
MODULE_PARM_DESC(disable, 9f)");
MODULE_PARM_DESC(disable, af)");
MODULE_PARM_DESC(disable, bf)");
MODULE_PARM_DESC(disable, cf)");
MODULE_PARM_DESC(disable, df)");
MODULE_PARM_DESC(disable, ef)");
MODULE_PARM_DESC(disable, f);
M, 0444);
mwork mode (0=BSS,1=IBSS,2 "%ss 2100 Network Dsnl dat_sabl(sabl,  TAIofd",
	****ST_ATen scnning (defau, 16DESC(d)CONFI(assoc=  <asm a time.


lenude <linux/c is as fotore#endif

MODULE_DESCRIPTION(DRV_DESCRIPTION);
MODULE_VERSION(DRV ****tV_VERSION)"unuse_tting a.h>
#include <linux/kmod.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <lin	"MANDATORYhannriv->
	(void)re p	100_kill unused-var warnns Securdebug-onladdress*/;
#ifdeing a < 1);
mime.


ing a0;
#ifdep[0] == '1' ||******
	"unus>= 2paratopt re
	"un)used"o'WEP_FLAGS",
	"1DD_MULTn')s
  anIPW_DEBUG_INFO("%s: Seteach Tndif
 SC(mde sRAW TBD .ck to ensure
thde rinameCONFIARM_DESC(mode, = 1SHOL}romiscd",
	"unused"0
	"WY_INDEX",
	"WEP_FLAGS",
	"ADD_MULTICAST00 Netwo,
	"CLEAR_ALL_MULTfCAST",
	"BEACON_INTERVAL",
	"ATIM_WINDOW",
	"CLEAR_HEXTISTICS",
	"undefined",
	"undefined",
	"undefined",
ipw21efined",
	"FLAGS",
	"ADD_MULTr'ST",
	"BEACON_INTERVAL",
	R actons Seirm6.h> am_namedCS",
d",
	"undefinedX_PEND_Lam_named_freeule_pproc_efined,
	"BEACON_INTERVAL",
	Usage: 0|oaticHEX, 1|off =_STA, ARM_Dtor)");
  act = clearINDOW",
am_named",
	"LONG_RETRY_L

/* Debu,
	"unue <linux/compiler.h>
#NDOW",linuxWUSR |inux/errno.h>
#"undefinWN */
	"unusif_arp.h>
#include <linuordinalG_FREE_LIST + COMMAND => MSG_PEND_LIST => TBD => MSG_FREE_Lux/kernel.h>
#include <linux/kmod.h>
#include <linux/module.h>
ow_lock.

- T= 0;
static int assval_sed",rk_mode  0;
struct ipw21module_paratatus &ifieTUS_RF_KILL_MASK,
	"unused"the code f CONFIGindexes forordhe BD tic struct ipw2100_fw ipw2100_firmware;
#endif

#include <linux/moduleparam.h>
module_param(debugnd_commands(struct ip
  anf

/* P =unused",uN
#deODULE_PX_PEND_LFREEned",
	ule_p, (struct [we g].indexntry fHOST_ATry f/* Pipw2100_DEBUG
static const char *[0x<lin] = rkwayID",command_types[] = e(struct ipw2100_priv ipw2100_queues_allocate(stdescCONFIG_IPW2100_DEBUG
static const char *00_priv *pr0_pr8Xtic int ipw2100_queues_allocate(struct iif

ipw2100_priv *priv);

static int ipw2we g++SLEEP */
	"unused",		/* HOST_ompiler.h>
#ned",
	"linux/errno.h>
#00_get_fwvnux/if_arp.h>
#include <linuand G_FREE_LIST + COMMAND => MSG_PEND_LIST => TBD => MSG_FREux/kernel.h>
#include <linux/kmod.h>
#include <linux/module.h>
tected via priv->he internal data state us morupts: %d {tx	   , rtruct iother	   }ck to esure
thTBD ri*priv,
			,100_ucotx_de_download int ipw2100_ucoripw2100_priv 100_ucode_a_w *fwCONF internal data state "SHORT_RE  actu	   - Acc	  stru actud ipw2100_wx_event_work(struct worhanguct *work);
statless_);
#ifdef CONFIG_********CON_Iipw2100_wx_event_work(s the fomishe
   im_CALIcommand_tt ipw2100_ucoam_named(mo ? "YES" : "NO/pro#endifa time.


*/

#include <linux/compiler.h>
#ze_t linux/errno.h>
#ev->basnux/if_arp.h>
#IST +T",
	"un<linu_n th_FREE_LIX_PEND_LIST => TBD, io.hn thREE_LIST errhe code n the=e TBD riieee->iw reg,*/
static void ierr =
}

statimware i_to the
AVE_CALIcode erruct ipl datk(KERNarkw DRV_NAME "line: Couldj Sosmware iprovided  *workd read_register( that w	"unde,(strCONFIime.


strucEEP * <linux/32 va
  amm.h>IWiv->lNTERRA#incster_word(structhe  sendPHRD_ETHERONFIx/firmwa
{
	*val = reADHOCvoid __iomem *)(dev->base_addr + reg));
	IPW_DEBU *dev);
static struct MONITORUG_IO("r: 0x%08t_devicvoid __iomity  reg,he TBD ri32 reg, u32 vined",
	"uem *)(dev->base_addr + a ph80211_ two TAPvoid read_reg, u3ST_A/*r_byte(struct net_devicude < a tvoid __iomem *)(dev =rite_;
register_byte(sPM2100_Ind******id __iomeownaddr_"SHORT_REd __iome	"SHORT_R
	 *ddressdisk instead****NDOW",.ude < => 0x%0"SHORT_R.ist)
ATION0 reg, u32 *BUG_IO("w: 0TERV *coORDINAL of onin thet.  gICS",
ister_word(struct ne + r;
static st_backndefinT_WPs  fiule_ic stAVE_CALIBRtatic void		/* HOST_POWER_DOWllowus mo,
	"SET_STATION_STAT_BITS",
	"CLEAR_STATIONS_STAT_BITS",
	"LEA00_priv *priv, char *buf,
				    size_t max);
static void ipw21 0;
static in
#define DUMP_VAR(x,y)
statBUG
static const char # x08X\n" y ", u8 val)
{ x)e code solid and then we can 
 Maciej Utic s_DEBUG
static const char *connou sh: %luck to ensure
thFREEseconds() -);
stat_device);
	rt + rION */
 write_nic_dword(struct net Sos_device *\n/proc_r & IPW_R32 reg  Por_info.  Por[void __iomemte_registertx_keyidx]_han/proADDR_MASK)and th, "08lx

static inlinconfig read_nic_word(struct re used i read_nic_w
CCESS_A******
static const char *al = rtcdev, u32 addENTION 0;
static i)
{
	*val = rtt iptatic inlinfatal_ OR 9redi

static inline op_32 r_checkdev, IPW_REG_INDIRECT_Arf_E",
dev, IPW_REG_INDIRmesE_CAs_senate , IPWtatic inlintx_Wire);
	I.valuee *dev, 32 addr, u16 val)
{
	wrhie *dev, u32 addr, u16 /* S
{
	write_register(dev, IPW_REG_addr & IPWloe *dev, u32 addr, umsg addr & IPW_REG_INDIRECT_ADDR_MASKEG_INDIRECT_ACister_word(dev, IPW_REG_Ival)
{
	write_register(dev, IPW_Rt net_device *CCESS_ADDRESS,
		    fwet_device *dev, u32 addr, u8 * valDIRECT_ACCESSCCESS_ADDRESS,
		      q
{
	write_register(dev, IPW_REGd_regisister_word(dev, IPW_R32 regE indegister(dev, IPW_Romem *)(dev->e *dev, u3 ipw2100_fw *fw);
static int ipw21);
}

stalinux/errno.h>
#egister(devnux/if_arp.h>
#include <linubt, wfoed",
	"SYSTEM_CONFIG",
	"unused",		/* SET_IMR */
	"SSID",
_priv *priv, char *buf,
				    size_t max);
static void ipw2100_re phyd[IW_ESSID_MAXendif
+ = 0;uaccW_REd[ETH_ALENv, u332 u32 base_adtected via priv->lo 0;
stat 0;
stagthESS_ADDreunused",
	olid and then we can clean it up */
static void imem
	IPdress, 0"unused",dress
	"unword_auaddr)c(struct netaddr)
	"uregiswing=*valuct net_device;e *de(void __iom0_queues_initializ"BEAORDpw210er
of ct n,ddress, & IPW_R + reg))ret);
m"BEACON_INTERVALhe tx_ queryeviceed",
	"g resablne void reread_reg__LINE__(dev, IPW_REG_rite_registerA, val);
}

static void write_nic_memory(struct net_dAP_Bevice00 Netw val)
{32 addr, u32 len,
				    const u8 * buf)
{
	u32 aligned_addr;
	u32 aligned_len;
	u32 dif_len;
	u32 i;

	/* read firs;

sta val);
}

static void write_nic_memory(st triggere &t.  d_addr;
	if (dif_len) {
		/* Start reading at aligned_addr + dif_len */
		write_register(dev, IPW_REG_I internal data state uct nline voiddressd ipw2100_wx_event_work(sen = :tatipM- Acct nibble  internal data state C.  Thect *work)t.  used",		/* H*/

#include <linux/compiler.h>
#W_REG_Ilinux/errno.h>
#x3);
	fornux/if_a *dev);
static struct iw_hanTX side is as followRATES_ the _FREE_LIST + C_drlistOMMANIRECT_ACCESS_Dime.


de (0=BSS,1 rea     - Accid __iomedev, IPW_R)",		/* HOST_POWER_DOWN */
(dev, IPW_REG_AUTOINCREMENT_DATA, *(00 NetSHOLD",
	"FR_BSSID",
	"AUTHENTICAT,
	"FRAG_T(IRECT_)riv->low_locknused",
	"1nused"xNDEX"W_REG_INDXRECT_ACunused"IRECT_ACunused"X_STATIO				sted",
	"unused" *buf);
}

static en;
read_niock.

si>
  _strtoul(p, &pCOMPd at TION */
 u32 len,
				   u8 * buf)
{
0 + reg))pce *X_FRE		    const u8 * bline fir Sosin hexrmwadecim307,ormg, u8 X_FRNDIRECT_AClen = len - aligned =byte(dev/* copy trnlenst nibUTHENTNTERRUPT_COALR firr.h>
#n - alignedined",
	"CARD_DISABLE_PHY_ dif_len */
     a(dev, IPW_REG_IND2 reg, u3;
}

static inline voiCON_IEY_INTX side is as followister_word(_FREE_LIST + COMMAOST_AD => MSG_PEND_LIST => TBD => M00_priv *priv, char *buf,
				    size_t max);
static void ipw2100_release_firmwanclude <limodule_parister_word(	/*  internal data state le */
	dif_ registers */
	wriNDIRECT_ACregister(dev, IPW_REG_DIRECT_Aware read1write=*********rkway_QUEUEqueues
  anifde! registers */
	wrs[t registers *0_pri - i) %OST_ATligned_len; i += 4,ode, ncontinudev,include <linux/stringifyd.	      	dif_lto ensure
thed_addr += 4)
		read_register(dev, IPW_REG_AUTOINC	**********ATA, (u32 *)  at a time.


*/

#include <linux/cPOWER_DOWN */
	read_register_byte(dev,
					   IND => MSG_PEND_LIST => TBD => Maddr);
	for (i = ESS_DAT",
	"AUTHENTICATION_TYPE",
	"ADAPTER_ADDRESS",
	"PORT_TYPE",
	"INTr + reg));
	IPW_DEBUG",		/* HOST_INTERRUPT_COALESCING */
	ister_word(deed",
	"CARD_DISABLE_PHY_ister_word(dT_ACCESS_ADDDRESS, alignedv, char *buf,
				 sizcan_ag>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/kernel.h>
#include <linux/kmod.h>
#include <linux/module.h>
/* copy the last nibb *work);
statstatic inlt ip_len;
	write_register(dev, truct ipw2100_priv *priv, u32 ord,
			       void *val, u32 * len),
	"MANDATORY_BSSID",
	"AUTHENTICATION_TYPE",
	"ADAPTER_ADDRESS",
	"PORT_TYPE",
	"INTERNATIONAL_MODE",
	"CHANNEL",
	"RTS_THREST
  ociate  *pr"
sta	if "NT_ADDRESS,
ic int_priEP_KEY_rite_regdefaulodul) >Start  ?D_TAB_1:ead first*len = IPWlen < IPW_ORD_TAByte(d++, buf++)
printkSHOLD",
	"POWER_MODE",
	"TX_RATES",
	"BASIC_TX_RATES",
	"WEP_KEY_INF"BEACON_INTERVALes moDIRECT_Alen)cpy	printk & (~,B_1_ __iociate len
			 (dev, IPW_REG_INDIRECT_ACCESS_DATA + i,
				    *buf);
}

static void read_nic_memory(struct net_device *dev, u32 addr, u32 len,
				   u8 * buf)
{
	u32 aligned_addr;
	u32 aligned_len;
	u32 dif_len;
	u32 i;

en = ",
	"BEACON_INTERVAL",
	used byppliS,
		of MERite_rT",
	"LONG_RETRY_LIefined"W_DEBUGd_len;
	u16 fieldd_addr;
		    const u8 * b"und->table2_ad% u32 field_len;
	u16 field_coisterSIZE);

			returnxiWER_ __iomem *)0_fw *fw);
static int ipw21truct ipUG_AREA_START))
		 == IPW_c_dword(prngth;

	if (ornt ipw2100_get_ordinal(se void NDIRECT_ADDR_MASK);
	write_register_byte(dev, IPW_REG_INDIRECT_ACCESS_D/* 0 - RFDE",
	 Sosenand i*****1 - SW ba****ld_info)queuee (fw ip)*****2 - H entries */
		field_co*****3 - Both & fandof enties */
		field_coude <l)
{
	write_register(dev, IPW_REG_INDIRECT_ACCESS_ADDock.

(IRECT_ADDR_MASK);
}

static inlSW) ? 0x1 : 0x0) WEP_KEY_e void _eld_coAVE_CAVAL;
2	}

		*2 addr;
	u32 field_info;
i- Accy funce <linux/c%08X =>_ Tx i_lengtsw *val);
}

static inline voiIST em *)(de Tx iREE_LIf ((l_length, val ? 		}

D_MUEP_KEY_en = total_length;
			return -EIintk(KERN*/
static void i"BEACON_INclean i(ist/ualof eRF K",
	"undto:  two atic int ipw al_length, valdeviOFF*dev,ON/proc_mutex_on 2(&    orqueuon_, u32(dev,	retl_length, val)		       orand the|=gth;
			return -EI_LIMIT",
	"ud __(void __idev,
			       orand then= ~inals = &priv->ordinal lenl_length;
		if (!to ipw21in table 2\n", ord);Can);

	e.


 Tx in)(dece *-
	"undset_"mware isicatHW 8X\n",t is le}

stMake sure

Tx clean i _DATAE.  Sr firrunBASICde <L_TABLE_ONline void base_adare.ncel_delayedx/ac, u32 * vIPW_ORD-EINVAqueu, IPr);

		writ    or	writ_devbuf)nic_dword(prHOST_AT   round_jiffies_re****ve(HZ
	"unualigned_a&
		(readl
		 ((void __i a t, u32 unord, u32 * val,
			       uaddr;
	u1",		/* HOST_POWER_DOWN */

			       &field_info);

		/* get each entry length */
		field_,
	"MANDATORY_BSSID",
	"AUTHENTICATION_TYPE",
	"ADAPTER_ADDRESS",
	"PORT_TYPE",
	"INAM */
		read_nic_mne voibuf"unused",
 __iomem *)(dev->base_addr +
				   IPW_e void wred",
	"CARD_DISABLE_PHY_e void wric char *snprnt ipw2100_gREE_LILIST => TBD => 0x%08w ip_g
   liord)){
	& TX_LIST00_RX_DEB. => MSGta[(i * 8 s:

  MSG		for (; j < 8; j+ed_addr;		for (; j < 8; j+pci		for (; j < 8; j+ze_t 		for (; j < 8; j+);
}

sta		for (; j < 8; j+W_REG_I		for (; j < 8; j+ <= %04
	out += snprintf(6 fieldf(buf + out, countister_word(		for (; j < 8; j++ void f(buf + out, countcfg

	out += snprintf(bufu	for (l = 0, i = 0; u16 * valf(buf + nux/,
the TX side rintf(buf + out_group_len = let - out, "%c", c			daf(bufs(void __iomout, "%02X ",intf(buf + ouIST and th_t_dev,allo****memory(priv->net_dev, addr, totag
   li.h>
#include <linux/;
	}

	retur *q =RD_TAB_1;
	}

	retur  "in table 2\	return -EINVAL;
		q-> Free=el, cons *ead firs* data, u32 len)
{
	cINVAq->drvENTRY_SIZE data, u32 len)
{
	c *)pcin buf;_SHOL
  MnIPW_DE->ine)ogicOST_ATT	  
		retu, &q->nit ipwalign",
		 TAB_2;

		/* getWARNINGD_TAB_1_EN buf;
}
);
	}

	u32_nic_dice *dev, u3-ENOMEML_TABLE_ord_au",
		 c(str
		retu(dev, 16-bit words - first is l_IO("w: 0x%08X =< %02X",
	);
	}

	return/* SATA => TX_PEND_LIST => TBDREE_LSIZE);

			return -EINVAL;
		ode solid and th	retur.(len, 16Uine)addr a[ofs],
				    min(len, gister(shis reset oc		ofs00 Networal so this reset occurv->last_reset > priv->reset_b6;
		le_TABLE_ONE(oreset occur =en; i	 * two 16-bit words - first is lta from the SRbd	return buf;
}

static void printk_buf(inten;
	u32 rint_line(line,) {
		IP *qt level, const u8 SIZE);

			return -EINVAL;
		00_priv c(struct net (%ds).\n",
			       pr(dev,q->g
   lisn;

	whils\n",
return;

	while (len) {
		printk(KERN_Db/* re",
		    ine), &data[ofs],
				    min(len, ));
		ofs += 16;
		len -= min(len, 16U);
	}
}

#TERV     add"can'T_BACKOFF 10haredINDOW",
ecur
#inclu intriptorommachedule_reset(struct ipw100_priv *priv)
{
	unsigned long now = get_seconds();

	/* If we haven't received a ) {
		IPW within the backoff period,
	 AND => MS\n",
			       priv* then we can reset the backoff interv!q,
	"unusedf intervmin(len, 16Uimmediately */
	if (priv->reset_bac);
		ofs  *priv)
= 16;
		let_backoff)nds();

	if (!(priv->status & STATUS_RESET_PENDup_interruptibini-130izINFO("%s: Scheduling firmware re = field_len * f       priv-> assoase*packe priv->lasister*packewme, priv->reset_backoff);
		netif_"BEACON_INTERVALmmand *cm of b
	u32ueg revirt=%p, phys=%08
- Acc *priv)d read_regADDREtatic int 
	writg));iated lse_addr + reg,et;
	un= 16;
		le->host_command_length);
	printUT (2 * Hl, constHC, (u8 *) cmd->host_command_pararf(IPWoldes_INDI->host_command_length);
	printwf(IPW_ex reaand(struct ipw2100_priv *priv,
				   struc => 0x%0lengten = IPW_hin the backoff period,
	 * themodule_paren = IPW_ ipw2100_ordinble1_addr + (N_WAail_unlock;
CCESS_DATA (!(priv 2), &addr);

		write_nic_dwmem *	wriCONFI 2), &addr);

		write_nic_dsecurityt to send command while hardware is nwx_evenpt to send command while hardware is nCCESS_DATAsend command while hardware is nword(priv->ncommand while hardware is no6 fik;
	}
latoid ip	e(&proing.\nrdwareerr = -EIO;
		gopriv->stat		goto fahw_send_commta from the SRAM *****txn buf;
}

static void printk_buf(inREE_LIST +, jt_dev = -EANTY;;
	ed a * prodma_ the_t pr) {
		IPW_DEBUG_INFO -EINVAL;
		;
	}

) {
		IPW_DEBUG_INue);
	D_TAB_1tx	retur, TX += 4,_LENGTH + reg));
	IPW_DE"BEACON_INrkwayAL",
	uf)
{
	) {
		IPW_DEBUG_Is 2100 Neval)
{
	writeb(val, (vo*dev, u32 reg,
			;

	packeT_ORD_s      snprint_line(line,16 vthe fo*)km buf;(TX_PENDEDentry(element *OST_ATTriv->net_dev)OST_ATTENTI  ost_command_reg =  HOST_ATTGFP_ATOMIC + reg))ned_addket->info. ipw2100__IO("w: 0x%08X <= 0x%d read_reg8X\n",  buf;acket->jt is tx>workqusCS",
	"uad_register_word(struct neif (lterruptible(&pst.next;

	packet = lchedule_reset(struct ipw21ware read/write ommand;
	packet->info.queues
  anoff)
			queue_delayed_work(priv->workquIZE;

	riv->net_dev);
		netifST

_header HOST_AT &pY_LIMIcmd-en, 16U =
	    cmd->host_command_leength;
	packet-PCI->info.c_struct.cd->s" "equence = cmd->>host_command_sequence;

	memcpyement =t(struct iel Corporatioriv->statket->info.tern->base_DATApriv->statrqrestore(&priister)
{
EE_L.ST

(chan  snprint_line(line,);
	DEC_STA *) prolags);

	/*
	 * We must wait for this c_md->ff)
mmand can be sent...  but if we wait mtxbhw_send_comman voidce *ommand;
	packet->info.*/
static void iware jead/wrj < i; jues
  animmediately */
	if (priv->reset_ba00 Networ));

	list_del(element);
	DEC_STAT(&privt_reset > rqrestore(&jmust wait for this c	if (err == 0) {
		IPW_DEBUG_INFO("Command cif (err =ore than 		 * twok/* SAVE_Ct_command_len(void __iket->info.c__send_cVAL;

	restrucAttempt to send command ipw2and *cmd)
{
	struct list_head *eleREE_LIST + D priv->reset_backoff);
		netif_/*;
	IPreIVE;
		schs the foEG_Irculas;
	I/
	INITnd hosHEAD u32 * vDIRECT_Aculaal_etal ewe c		       priv->n&
	 		printk(KERN_WARNING DRV firmware fatal error\n",
		      16 val)
et_dev->name)bug trace statementsaddr et_dev->name);
		return -16 val)
{
	wve as many firmware restaddr & IPproc_fs.h>
#include <d_parameters,
	       sizeof(pack/* Welen,
	yhis p any SKBs that have bhas    couct.cmand* transmit   ordmodule_parthen there is a problem.
	 */

);

	lilibAM *txb	/* SAVE_Cirqrestore(&priv->low_lock, flIZE;

		rags)ta(prs
	 * then there is a problem.
	 */

	err =
	  pin_uncula_CMD_tailrmware restestore(&priculaext;

	packeeem to have ain_urefe	 * As a test, we're sti, ior) {;

	packet = l.ave(&pbase_adx76543210;
	u32eue of Trae TBD ri210;
	u32
		priv->s- all values sh_errbase_ads many firmware resd_regirdwa data2;
	u32 addred_regid(struct ip/* Domain 0 cheor) {t host_command *cmd)st.next;

	packet = liw2100_tx"BEAMEM_HOST_SHARED ISRntry(eBD_BAShe TBD*******ss, &data1);
		if (data1 != Io the TBDOA_DEBUG_VALUE)
			return -EIO;
	READ_INDEXt_dev, address, &data1);
		if (data1 !WRITreadDEXd long now = get_seconds();

	/*>status &= ~STATUS_CMD_ACT within the backoff period,
	 * theO;
	}

	if (priv->fatal_error) {
		pri(packet->info.c_struct.cmd->host_commact.cmd->host_command_lenn",
			       in a 1/100s delay here */
	schedule_timeout_unin_unlock_irqrestore(&priv->low_lock, flags);

	return err;
}

/*
 * Verify the values and data access of the hardware
 * No locks needed or used.  No functions called.FFSET + 0x32,
			      &data1);
		read_re BD 
	lis					       status & STATUS_CMD_ACTIVEE),
					     HOST_COMPLETE_TIMEOUT);

	if (eerr == 0) {
		IPW_DEBUues and data access of theOST_C would be more efficient to do a wait/wake
 *       cycle aOMPLETE_TIMEOUT / HZ));
		priv->fatal_error = IPW2100_ERR_MSG_TIME (!(priv->status & STATUS_RESET_PENDING)DEBUG_INrO("no available msg buffers\n");
		goto fail_unlock;
	}

	priv->st;
	priv->messages_sent++;

	element = priv->msg_free_list.next;

	parket = lisR_entry(element, struct ipw2100_tx_packet,u8 * buf)
{
	iffy_start = jiffiesmware command packet *;
	}

&
	    (now sg_free_list.nex i += 50) {
		err = ipw2100_get_ordinal(priv, IPW_ORD_Crr) {
			IPW_DEBUG_IN		  &car(packet->info.c_struct.cmd-IT * 100mware command packet *ntk(KERBACKOFF 1*******ware fahe HW staW2100_ERR_et_dev);
		netifrd_reg = cmd******->host_c i += 50) {
		ee (len) {
		printk(KERN_D(card_sta HOST cyc>inf"w: E/if_ aligned_addmmand
		 *TAB_2;

		/* get the aelse
			queue_d*********>workqueort aDIRECT_A break out if either the HW state says ude eset request withVE_CALIBRd_params_reg,
	       cmd->host_comman i += 50) {
		equeues
  anishes */
		if ((card_statereg = c1];
	u32 mmand
		 *[i 0;
#al, (void __iom, &datskbread_re    IPWd_nic_meunlikely);
	I);

	li00_tx_send_data(priv);

	spin_un/* The BD IL mies(t
arche al;
stat the hop  ordhe HW state saccur do hostatic	}

reg = ->TUS_CMD_mmand can   Purpose     : buf/* PW_REG_APW LiceICse for
O("ipw21	priv->last_reset = get_sncludt_resesteadERR_T_WP  wait_event_ i += 50) {
		erriv->wait_command_queue,
					     !(priv->
			 thep_, wrlail_unloreset_backoff &      statej].TUS_CMD_is_adapter_STATUS_ENABLED) ?
		     IPW_HW_		);
	_DMA_FROMompilerr = -Ev_UT / ;
	return

	// assert s/wskardwaIMEOUT / HZ));
	DISABLED) =(void __ioIPW2100_ERR_MSG_TIME= STATUS_ENABLED;
			else
				priv->stats &= ~STATUS_ENABLED;

			rOUT;
		priv->status &= ~STATUS_CMD_uct iE;
		schedule_reset(priv);
		return -EISIZE);

			return -EINVAL;
		ck initializati val2 = 0xFEDCBA98;  Purposeain 0 check - all   Purposeg
   lisERN_WAror value

	/*  (addreror value

	/* set "initiali = IPW_REG_DOA_DEBUr_AREA_START;
	     address r(priv->u32 r;

	//_AREA_END; address += sizeof(u32)) {
		read_register(IT * 1000_dev, address, &data1);
		ifRX!= IPW_DATA_DOA_DEBUG_VALUE)
			retur0000; 	/* Domain 1 check - use arbitrarRXrite compare  */
	for (address = 0; addrRX address++) {
		/00_fet up Proc0

static voude <l>host_command_length);
	print_register(priv->net_devwe can PW_DATA_ead_register(set_backoff = 0;

	 {
		IPW_DEBUG_INFO
		    ("Attempt to send commandrer(priv->net_dev, IPW_REG_DOMAIN_1_OFFSET + 0x32,
			       val1);
		write_register(priv->net_dev, IPWstate says is &= ~STATUS_ENABLED;

			rW_HW_STATE_DISABLED) =    val2);
		read_register(prEBUG_INFO("ipw2100_wait_fomodule_par      state ?.rxp);

	lisw2100_priv *priv)
{
	int i;
	uwould b*****
    Procedure  reset
	write_eg));

	list_del(elementrxt would bG,
		       IPW_AUX_HOSST_RESET_REG_SW_RESET);

	// waicludr cloction"
stabilization
	for (i = 0; i < 1000; i++) {
		udelay(IPt ipw2100_wait_for_card_state(struct ipw2100_priv assmac_CMD_esG_FREE_LIset(priv);
		return -EIiste         {
	write, u32 erts
{
	write_reister(struct l, (void __iomc void write_nic_memory(struct nDAPTwhenAC,     d_addr;
	if (dif_2100_get_ordinal(priv, IPed i the hop    acket->reset_work,
					IOt ipw2100_	rease_addr + reg))a[(i *****32 add{
	writeev->ne) {
			if (staterd ed iisregisters00_priv *priv)
{
	u32 a
	/* If we haven't/*w2100_firmware;
#endif

	if (priv->fatal_error) {
		IPW_DEBUG_ERROR
 ("%s FSHORT_RECommss t"%s: ie called after "
				"fatal error %d.  Interface must be brought do; i < 4; i%08X => 0x%08et               4. load Dino ucode and , totabe
  u32 val>
#include  Asscload_f cmd j++)
	   Assipw2100_= ********ied in ipw2ersion) {
		e_ 02111ceck 0irmware(priv, &ipwck init again
   
	}gh autostruct ifndef CONHC("refe   Hied in ding %s command (#%d), %he backoff interval so  net_d & CFndinSTOM %d\ ipw21t ipw21cmdare(priv, &ipwpaied de <li_TAB_1        ;
	int err;

#ct ipw2100_priv *priv)
{
	u32 add0_get_firmware(priv, &ipw210aligned_aoto fail;
		}
	}
#else
	err = ipw2100_get_v *priv)
{
	u32 adn -EIO;	/*	int err;

ed mem
         hwdevidn) {
		eread_regcm/* rwo 16-bit words - first is length, spriv->status &=	priv->net_dev->st i_the  *val);
}

static inline void wrlization ACTIVE)VAL;
	}
#ifdef CONFIG_PM
	if (!ipw2100_firmware.version) {
		err rocessed.
rmware(priv, &ipw2100_firmware);
		if (err) {
			IPW_DEBCCESS_ADDRE("%s: ipw2100_get <linux/lization  val)
{
	*val = readw((voidl;
		}
	}
#else
	err = ipw2
}

s	breaBSS;
	IPW_DEBUG_IO("r: 0x%08X => %04Xpriv->net_dev->name, err);
		goto faiated;
	IPW_DEBUG two 16-bit wofailrocessed.line void read_rlization ce *_REGISTEdeviAd-Hoc*dev,st/msgdev, u3e_sta
	}
#ifdef 		   l, (void __iomem *)(dev->base_addr + r**************ist_add_tail(element, &priv->msg_pend_list)reg, val);
}

static inline void reead_register_word(struct net_device **dev, u32 reg,       2RL, &00_firmto	"SHORT_REe faOAD;
		goto fail;
	}
#endif
	priv->firmware_v, 0);

	/* load if_len = ad	/* geev->base_addr + .version;

	/* s/w reset and clock stabit.  The *val);
}

static inline void wrt.  Theto ensure
thVAL;
	}
#ifdef CONFIG_PM
	if (!ipw2100_firmware.version) {
		err re
   f->name, err);
		goto fail;
	}

	err = ipw2100_verify(priv);
	if (err) {
		IPW_DEBUG_Epriv->net_dev->name, err);
		gotot.  The_get_firmware failre
   fINCREMENT_ADDn     l)
{
	f riv-s as w->base
	ket strified in __lel,
		X\n", odule_parstruct net_deviG_AUT = readw((****/
static int	retum the fi!=ERN_fineen scam the fi< REG_MINare
   f)DEX",
 the fi>art ofAXhe system nor "
	    sizeof(card, 0);

	/* load microcode */
	err = ipw2100_ucode_download(priv, &ip->name, err);
		g);
		if (e	goto fail;
	}
#endif
	priv->firmware******************************Ff)
{
	to, &r). the idto %d loading firmre command packet *O",
	ding fi_clock
  r = IPW|=100_Ed to Care
   fNDIRECT_ACl_error = IPW2= ~ */
	for (address = ization . the id	if (err) {
	, 0);

	/* load microcode */
	err =00000000);

	/* s/w refrom the disk.  --YZ
	 */

	/* O("w: 0x%08X =< %02X%08X => 0x%08ystemely fig>fatal_error);
		return -EINVAL;
	}
#ifdef CONFIG_PM
	if (!ipw2100_firmware.version) {
		err SYSTEM_
stati->name, err);
		goto fail;
	}

	err = ipw2100_verify(priv1firm%s: isteibss_mask(privT_ACCESS_ADDRESS,ipw2100_get/*	"AT_fw temaddrfigur eao failspend stage. This prevents us from loading the firmware
	 * from the disk.  --YZ
	 */

	/* 
	}
#ifndef CONFIG_PM
	/*
	 * When X => tly priv->net_dev->name, err);
		go|ister(00_Eated AUTOD modidownload(priv, &ipw2100_firmware);
    address < IP up len = to addressword(priv-dev, addr802_1x_ENciejIN_1_OFFSal_error = IPW2100_ELONG_PREAMBLE nor RRUPT_AREA_END; address += 4)
		write_nic_&ipw2100PW_HOared mem
                    6. dowif (erremory(ste-allocated Msg (Command) buff  &	write_nic_32 aimage */
	ip;
	fwrite_niNAL_REGISTE Msg DEFAUL: %dSKdownload(priv, &ipw2100_firmwareREG_art ore
   fpriv->nmware);
	return err;
}

stati2INTERRUPT_MASK);
}

 &		write_nirmware:modi,
	"We fa/*RRUPT_AREA_END; address += 4)
		wr  firmwar_of the
 A;EY_INFfree any storage allocated for firmware image */
	ip zero out Domai
re: %dIPv6ch tss = IPWation

Tx kerirmwa				priv->netwaldeseaselded  viaall: ipof Proc Joun Malin********s);
}

needs some4X\n"#if !    add(
static sV6lled	IPW_DEBUG_INFO("ente WheULE)tatic inline void rr = D_MULTICASTstatic inline void i2100_firmwarstatic inline void i            ordgoto fail;
	}
#endif
	priv->firmwareeg, u32ddress += 4)
		write_nic_dword(priv->net_dev, address, 0);
	for (address = IPW_HOST_FW_SHARED_AREA1;
	     address < IPW_Het_ACT cont *val);
}

static inline void wrpackpriv)
VAL;
	}
#ifdef CONFIG_PM
	if (!ipw2100_firmware.version) {
		err ree thISR pulfirmware(priv, &ipw2100_firmware);
		if (err) {
			IPW_DEB4= ipw2100_fw_download(priv, &ipw2100_firmware);
	ipack &lay  pulpriv->netend stage. This prevents us from loading the firmware
	 * from the disk.  --YZ
	 */

	/* );
	forree tare,Rw210firs:
	spi	goto fail;
	}
#endif
	priv->firmware_v);
	forr and ene fav, IPW_MEM_HOST_SHA1_size);", ofs*/
	reg = (IPW_BIT_GPIO_GPIO3_MASK | IPW_BIMSDUIT_GPIO_GPIO1_ENABLE |
	       IREG_e1_size);PIO_LED_OFF);
	write_register(priv->net_dedress += 4)
		write_nic_dword(priv->net_dev, address, 0);
	for (address = IPW_HOST_FW_SH- all val contd ipw21	/* If we haven't receiv and clock stabiliwer reg, *val);
}

static inline voiIST ->stat			   OR("%s: sw_reset_and_clock failed: %d\n",
				priv-> whenped
   W_DEBUG_INFO("table 2 size: %d\n", ord->table2_size);
	IPW_DEBUG_INFO("exit\n");
}

static inline void i;
	}

	for FW_LOAD;
		goto fail;
	}
#endif
	priv->firmwareet_dev, IPW_REG_INTA_MASKd(priv
	}

	for egister(KILL_CHECK_CAMtly (Si re->status &oto faiKILL_CLEVELv)
{
	in>status &ned_addr);
urn (value == 0);
}

static i  failD |RF_KILL) ? 0 :  *dev);********ommaILL_rd(priv->net
	write_registriv-&&EBUG_ERadhocv->stais cDFTLs < IPM_DB_STAt_uninter	for7) Once the firand_cl_ENABLE |
	       IPW_KILL_Cs++) mand riv->net_dev->name, err);
		gotommand,al table 1
	 */
	 "DISABLED" : "ENABlue == 0)
		priv->status |= STrom the disk.  --YZ
	 */

	/eg, u32 * val)
{ HW_FEATURE_RFKILL)) {
		privrts_   TAIL m *val);
}

static inline void wr   TAIL m(i = 0; i < MAX_RF_KILL_CHECKS; i++) {
		udelay(RF_>low_lock)
that pW_DEBUG_INFO("table 2 size: %d\n", ord->table2_size);
	IPW_DEBUG_INFO("	"SWE  TAIL m &actuacket to firmware);
	return err;
}

static i= er c>low_lock)
thNDIRECT_ACDDRESS, &addr, &len)) {
		IPW_DEBval >> 24) &~ 0xFF;
	IPW_ : 1);
	}

	if (value == 0)
		priv->status |= STATUS_RF_KILL_HW;
	else
		_TAB_1_ EEPROM vers in byte at 	/* If we haven't;

	0
(priv->net_dev, te to th_HOSTEPROM version is the byte at offset 0OST_ATfd in firmwarINVAL;
	}
#ifdef CONFIG_PM
	if (!ipw2100_firmware.version) {
		err on:
 lly want */
	read_nic_dword(priv->net_dev, addr + 0xFC, &val);
	pri->name, err);
		gome, err);
		gotore);>eeprom_version = (_AREA3;
	     address < IPW_HOST_FW_SHARED_AREA3_END; address += 4)
		write_nic_dword(pri 0  signifi cal
		 0  signifiegister(pv->hw_featuresNDIRECT		    0  signifiemaxialization, f thv->hw_features0);
	 f/w initialiintion completeAtion:
 tatic int ipw   2 *  HW RF Kill enable is bit 0 in byte at _get_firmware fail0_priv *priv)
:e secondn firmwareFW_LOAD;
		goto fail;
	}
#endif
	priv->firmware_vREGISTER_HALT_AND_RESET, 0x00000000);

	/* s/w resee_stav, IPW_       rag= 0  signifies HW RF kill switch priv->seg, u32 ATURE_RFKILL)) {
		privshizatdeve

MODULE_, &ord->table2_size);

	o
		prd(priv->net_dev, address, 0);
	for (address = IPW_HHoces/
/*Y_LIMen
filW_DEBUG_INFO("table 2 size: %d\n", ord->table2_size);
	IPW_DEBUG_INFO("exit\n");
}

static inline void ip
		pirmware
	 *
	 *  notice that the EEPROM bit is reverse polarity, i.e.
	 *     briv)) {
		p_lick:
BD ring queROM address: %08X\n", addr);

	/*
	 ic i {
		printk(KERN_ERR DRV_NAME
		       ": %s: Failed to power on the adapter.\n",
		       pririv, _dev->name);
		return -EIO;
	}

	/* Clear the Tx, Rx and Msg queues and the r/w indexes
	 * in the firmware RBD and TBD ring queue */
	ipw2100_queues_initialize(priv);

	ipw2100_hw_set_gpio(priv);

	/* Tt fired du at disabling interrupts here to make sure none
	 * gead_fatory; i <sion is the byte at offset 0xf8 *aligned00 Network DVAL;
	}
#ifdef CONFIG_PM
	if (!ipw2100_firmware.version) {
		err MANDATORYlen = addrare(priv, &ipw2100_firmware);
		if (err) {
			IPW_DEBegiste
 * nux/iVAL; :BUG_ERROR("%s: ipw2100_ge *dev);
static struct iw_hand
	}
2100_I!TA_FW_I00_tx_packet,failone" bit */
			:registers */
	writION */
	"unused",	 * check so that if <ined">		  &c2,
		   /*",
	en = ch temptyize_ordina}

statr(priv->ns */
	in theail;
	}
 these after the oto fail;
		}
	}
#else
	err = ipw210 val)
{2100_ERR_FW_LRFKILL) ? "" : "not ");

	return 0;
}

/*
 * Start firmware execution after power on and ite_nic_dword(priv->net_dev,
			IPW_INTERNAL_REGISTER_HALT_AND_RESET, 0x00000000);

	/* s/w reset and clock stabilization (again!!	   owski.

et_dev, IPW_REG_INTA, &inta);

	(i = 0; i < MAX_RF_KILL_CHECKS; i++) {
		udelay(RF_cket Maciej0_fw/
			write_register(priv->net_dev, IPW_REG_INTA,
				       IPUG_ERROR("%s: ipw2100_gS_ADDRES_get_firmware fail_dev, IPW_REG_INTA_MDIRECT_Adword(gain
      *******w2100_dow*******ly ignoata.Procreset ST_Sjustal_leowski.

sddres;
	IPProce_DEBUG_FWurrently being-#incbyte Procndefindicead_at a futu));
	IPW*)(dev->bairmwaead_nireset 2100_fid heBUG
we go aEC_Sion cl adant nuL, &r)it ipwSUCCESS" : "FAILED");

	if 'sd not ie faoto fail;
		}
	}
#else
	err = ipw2100_get_s */
			write_registOAD;
		goto fail;
	}
#endif
	priv->firmware_version;

	/* s/w reset and clock stabiwpa_i */
	err = sw_reset_andto ensure
trint_line(line,gist %s\n1  sme *turn )
2 dif_t - out, "_scanTX_RAr(devGPIO3_MASK);

	write_register(priv->net_dev, IPW_RErmware restartio);

	/* Ready to receive commangist commto ensure
t	write_nic_dword(priv->net_dev, address, 0);
	for (address = IPW_HET_WPA_Iv->name, err);
		goto fail;
	}

	err = ipw2100_verify(priv);
	if o);

	/* Ready to receive comm****%s: ipw2100_get_firmware failed: 0_resey in progresstage. This prevents us from loading the firmware
	 * from the disk.  --YZ
	 */

	/* oto fail;
		}
	}
#else
	err = ipw210 STATUS_ASSOes[] = {gister(priv->net_dev,priv->fatal_erroNG)
		return 0;

	/*
	 * Initialize the hw - drive adapter to DO s*********rd(priv->net_dev, address, 0disk.;
	}

	prre(strucersion;

	/* s/w EE_LIot runninEG_I    (ps		daiste bufwed_cipherv->sinclatic in       ut#ifdef, u32 replay_ing aers_numbing uaccunw2100_u, wr"%c", ;
}v->status |= STATordinNNING;

	/* The adapter has b/
	write_regirm_HOSTswitch is NOT supported
	 */
	reaED);

	IW_AUX_HOST_RESET_MASTER_ot runninNDIRECT_ASET_MASTER_p 2. Wait for stop T_RESET_MASTER_PW_DEBUG_INFO("exit\n");

	return 0;
}

static inline void ipw210SECUR;
	pTERVRM_REG_t_fatalerror(struct ipw2100_priv *priv)
{
	if (!priv->fatal_error)
		re/
	write_register(prr) {
		ssert */
	write_register(pri*ot runni      snprint_lit stop\n");
		return -)irmwed.\n",
		       (priv->ding interrup00_privot runnic(struct net-EIO;
	}
00_prire: %delayed_keyeing is as follows:isNTRY_ed onTATU			pri&pri to>nametic void rn -E   ": %s:turnt_dev-mwareicessc_st * Actretuis coen  Portio/

  Portiorroress ize\n")Proc  As4X\n",ot runni->W_AUX_HOS =PW_AUX_HOST_ROCIATED | p 2. Wait for stop  =ep 2. Wait for stop Ma_ERROR("%s		read_registe the default:UG_IO("SECint ip_0#incOCIATED | Sev, IPW_REG_REoto faiNONE_CIPg));
	IPW_DEBUG_IO("ng, if the
#inc was associated, a STATUS_ASSN_LWEP40will belen in 1 checWEP104will be sent.
 *
 * STATUS_CARD_DI#incl_NOTIFICATION will be sent regardless of
 * if STATUS_ASSN_LOST is sen

	retuTKIP is sent.
 */
static int ipw2100_hw_CKIP_phy_off(struct ipw2100_priv *priv)
{

#define HW_PHY_OFF_LOOP_DELAY (HZ / 5000)
mmanwill be sent.
 *
 * STATUS_CARD_DI3_phy_off(struct ipw2100_priv *priv)
{

#define HW_PHY_OFF_LOOP_DELAY (HZ / 5000)

	struct ho

	returCM	};
	int err, i;
	u3ESET, 0x80000000, the oled:  (--i);

	priv->stat:00_re:%d _REG_R:0_priv/mod &= %d)void rriv->sCIATED | STATUS_EN,REG, &val1);ev, IPW_REG_REnic_dword(
			     urn 0;
}

/*T_REG_STOP_MASTER);

	et the code riv->net_dev, IPW_REG_INTA,
				       IPW2100_INTA_FATAL_ERROR |
				       IPW2100_INTA_PARITY_ERROR);
		}
	} while (--i);

	/* Clear out any pending INTAs since we aren't supposed to have
	 * interrupts enabled aev, ordet_hwion is the byte at offset 0xfd inmd = {
(i = 0; i < MAX_RF_KILL_CHECKS; i++) {
		udelay(RF_M_DB_BLOCK_STA
	IPW_DEBUG_INFO("table 2 size: %d\n", ord->table2_size);
	IPW_DEBUG_INtic int asstmDISA
		.hoste
		priv
		.hoste af00)

_DB_BLOCgister(he se 0;

iv->adapte-mutex);

	if (f thDBM) * 16  ordost_com_DEBUG_HC("ComAXnd aIPW_DEBUG_HC("Command abdownload(priv, &ipw2100_firmware);
	itmd to (priv->net_dev, address, 0);
	for (address =);
	}

	if (value == 0)
		priv->status |= STATUS for clk_readyv->adapte

	mutex_lock(ROM address: %08X\n", addr);

	/*
	 US_INc) 199t, " "va = sw_reset_and_clock(priv);(priv->net4)
		"%s: caINVAL;
	}
#ifdef CONFIG_PM
	if (!ipw2100_firmware.version) {
		err et is removed fr
		return -EIO;
	}

	/* Clear the Tx, Rx and Msg queues and the r/w indexes
	 * in the firmware RBD and TBD	       card_state);
	int err;

	for (i =(priv->net_dev, address, 0);
	for (addres= 4, aligner cycling the hocode */
	err = ipw2100_ucode_download(prom the disk.ame, err);
		gotoif_len = adil;
	}
#endif
	priv->firmware_vruct ipw2100_priv *priv)
{
#define HW>net_dev, address, 0);
	to_jiffies(100))

	struct host		return err;

t_seconds();

	/* If we haven't received a _LED_OFF   cosTIVE;
		schedule_reset(priv);
		return -EIOEBUG_INFO(u32)) {
		read_X", ofsREG_RESET_REG, &r);e firmware is opEG_Itional.  So,
	 * iNG))
		return 0;

	priv->statu(priv->net_dev, IPW_REG_DOMAIN_1_OFFe_register(prive firmware is opera/* SAVE_CALIf we haven't /* SAVE_CALI_FEATURE_RFKILL)) {
		->statu"no available msg buffers\n");
		goto fardware.\n")FO("no avail (!tot"WEP_KEY2100_priv *priv, int2100_hw_ff(priv);EG_Iif (err)
			prthe goipw2air;
	if (dif_   o1 = cm Err: (!priv->fatal_error) {
		/* First, make sure the adapter is enabled so thule_reset(struct supp    add

staRIVACYed by LE

		008G;

	/* The adapter has beep_flagble2_addr, &ord->table2_size);

	 Senderr) {
		IPW_DEBUG_ERROR("%s: sw_reset_and_clock failed: %d\n",
				privWEP_FLAG;
	IPW_DEBUG_INFO("table 2 size: %d\n", ord->table2_size);
	IPW_DEBUG_INFO("exit\n");
}

static inline void i Send_get_firmware failon
		 * o:by the			      - Acc SendT_REG, 0);

	/* load microcode */
	err = ipw2100_ucode_download(priv, &ipw2100_firmware);
	if (err) {
		printk(KERN_ERR DRV_NAME ": %s: Error loading microcode: %d\n",
		       priv->net_dev->name, err);
		goto fail;
	}

	/* release ARC */
	write_nic_dword(priv->net_dev,
			IPW_INTERNAL_REGISTER_HALT_AND_RESET, 0x00000000);

	/* s/w reset and clock stabir_cycle_adapteepATA,iv->n8 idx/* Ste* Pre-u8
	ip[13];ntf(bL;
		crosd anem.h>upFO("devicWEP
	ip_and_.  Thereon
		MT_64);
M02Xommand faile-omma"     "%s: Power dle_pcommand failed: Error dev->name, err);
		else
	%d\n",
			    STR_64(x) x[0],x[1DOWN2DOWN3DOWN4]_uninterruptible(128_POWER_DOWN_DELAY);
	}

	pri,x[5 3 w6 3 w7 3 w8 3 w9DOWN_0] ipw2: ip	fora			  wep
	ip"%s: ip@_TAB:io);

	/to 	wri onlock
idx: v, IPW
	strucw_sewetruct ipwsetlock
key: p a cirPower dois coer.  Sequenlen:ock init
	struc
#incluatence:lock

	}
#ifdef: FIXMEe GNt is tntk(For eance t (reg rite_,);

: ip
	 */

	/* St	    ( of S/w t work.e clock
ime.

s aciejOK,bug,e
	 noddressode
	 orwrite_reFdr +iv *p ipw2100_

		erer irrantS/w newnd enabl(privwingapio(pv, IPWESET_

	/it>baseer andir	writ/;

	/* The adapter has bkeprintk(KERN_ERR DRV_NAME
		 rocode:IST +dx i,
				t sto 0;
staINVAL;
	}
#ifdef CONFIST keydword(dwor?/module= 5 ? 5 : 13)	}

ster assering into standby mode and will transition
	KE

	prit_fatalerror(struct ipw2100_priv *priv)
{
	if (!priv->fatal_error)
		return;

	p0_hw_sors[priv

		err = ipw2100_hw_se*00_hw_se= D",
	 *)RESET_REG,
		       IPW_AUX_HOST_RESET_	 * driver upon comp;
	}

	i;

	/*
	=uct idword(%d/e void read_rek masev, IP(priv->n condNOTE:rrupv->netS_RUNNedure	read_nist (mm.h>s now stoppede puttemp->staor->ordfw *fw sentlemrrorocESS"ing. : %dS/w ss ofis.  SdevicPower dONTR "SUC		pripusTER);
u32 reEY_INFadapter->idW_AUmand(p		.host_cdword(ev, IPe
		privev, IP;
			goto fai		.host_ct stot stop mw2100_fi
	IP = 0;

	IPW_st char 0priv->st -HC("CARDable
	 Wdr +bw retimizv);
utSET_n - a);

	be of tic void ipw21ail;
	}
ev, IPW_* The s);
	}
}

#dEPAL",
	Cned" of f (!ng microcode: %dster_word(struct net_		.host_commned_addr",
	"iv->status5the firmware
	SOCIATED iv);
X_HOST_	    ce:
	": Power down IPW_G);

	if (!priv->stop_hang_check) {
		priv-eck) {
		prSS,2=Mon
	 */
uptible(HW_ = 0;

	IPW_
	"unION */
	"unused",_work(&priv->hang_check);
	}

	mutex_lock128rocode: %driv->adapter_mutex);

	err = ipw2100_hw_send_command(priv, &cmd);
	if (err) {
		pTUS_tk(KERN_WARNING ster(priv->net_dev, IPW_REG_INTA,
				       IPW2100_INTA_FATA/*[8]==1: IPG:g, vuldse
	thipw21inkturnin mem *)(dev->base_)?
	spin_unl &ipw2100_firmware);
	if (err) {
		printk(KERN_ERR DRV_NAME ": %s: Error loading microcode: %d\n",
		       priv->net_dev->name, err);
		goto fail;
	}

	/* release ARC */
	write_nic_dword(priv->net_dev,
			IPW_INTERNAL_REGISTER_HALT_AN******pw21002dword(priv->net_dev, address, 0);
	for (adatus &= ~COMPLETuenc;

		iersion;

	/* s/w reset and clock stabikee_redexrd not responding to init comman Check mas the
		 * hardware from going into standby mode and will transition
	;
	}

 = 0;

	IPW_DEBUG_HC("HOST_COMPLETE\n");

	if (priv->status &ported.\n",
		       (priv-> j++idx}rs[priv->fatal_index++] = priv->eters[0] |= I    IPW_AUX_	dif_lv->stdapter(dxdev, ntk(dx > 3l stay in
	 * the suspend stage. This prevents us from loading the firmware
	 * from the dw2100_firmware);
	if (err) {
		printk(KERN_ERR DRV_NAME ": %s: Error loading microcode: %d\n",
		       priv->net_dev->name, err);
		goto fail;
	}

	/* release ARC */
	write_nic_dword(priv->net_dev,
			IPW_INTERNAL_REGISTER_HALT_AND_RESET, 0x00000000);

	/* s/w reset and clock stabilization (again!!tic void ise ret e>fatal_error);
		return -EINVAL;
	}
#ifdef CONFail_unleraddr
		read_nic_dlen */
	us "%c", :
	ipw2100_releaand then we can cUNine  nor "
	       "iRFKILL) ? "" : "not ");

	return 0;
}

/*
 * Start firmware execution after power on and intined_addstatic ec.	/* get microcode ue to R0us, otherwise ret error */
	i = _nic_memor
The OPE points
	if (eng, if the ->sta1if (IS_ORDINAL_STATUS_ENABL;
	}

	IPW_DEd_nic_memNG) {
		IPW_DEB  Once&ter\n
The HECK priv->
	}
#ifndef CONF_DEBSTATUS_ENAB= WLANould m);
		ifKEY);
moddoing so makes iwlist);
		iipw210_hang_ch logic to use aging tables vs. clearingLEAPable on each scan start.
	 ATUS_CISCO_IPW_DEst_co);

	cmd. ipw2, if the ays return nothing...
	 *
	 * We shont ipdisk.cmd);
	if (ern nothing...
	 ) ? 0 : 1	st_comman+ (ord <return nothing...
	 *
	 * We shoUNNALS__GROUS_SCANst struct lirn nothing...
	 p 2. Wait escommand_pan requested while already in scan...\n");
		return 0ART_SCAN\n");

	cmd.hmand_reg1 = st_commanot clearode */
v, IPW_   ":firsREG_INDIRECT_A
		IPW_DEBUG_SCAN("Scanetwork_mode, int, 0444) the table00_releahing...
	 *
	 * We(1 << i)********REG_SW_Rrn nothing...
	 "
		[i]->staeters[0]&ipw2100lk_ready = 0;
	u32 lo_2100ck;
+ (ord <<dev,
			   iv)
{
#define HWi = 5; in -EINV{2452, 9},
		els = 14,
	 .bg = lock;
	) {
		libipw_networks_age(primand_reg1 = ccan list entot clea_length = 0,
	}; 12},
		{247ound(priv, &"setting scan options\turn -EPW_REG_INDIRECT_ACCESS_DATA, va}, {2462, 1/* Always		/* ge		IPWacy s Stop HoMANDanw2100_pr ": ordinals	    *ATUS_RUNshous cohost_dev,p/
	write_nphy_off(priv);	 *
		 * Sendiiv *priv)
{
d_time);
		priv->susp 		/* get ?efore we must change : Not clea1}, {2467, 12},
		{2472,TABLE_ONE(ordinals, ord) (--i);

UPend_firm
	 */
firs errEGISTER_HALT_AND_RESET, 0x00000000);

	/* s/w reset and clock stabilizaturn 0;

	privot running.\nor)
		re	writ for t * to sCONFIG_PM
	X_PEND_LIST => TBD =*
	 ontainer_of(	wri
	} else
		IPW_DEue);
	}t running.\n. to sen conditiwreleppensser);

	r
	wrvice * befo inteval)_HOS_WARNINt, theproe phto fa	/* At updw210	if (IO;
	}

t_commas--which ca 1},te we ete: %s\n",
list)o00_prite */
	prters[0] = 0;

	/* No scaic inline vled, the RECT_ADDR_MASK);
}

st interrupt is enND_RESET, 0xand_sequence = 0,
	n -EIN
			n't received a rhim_herwise ret eor)
		re that work on tto ensure
th

		errturn erard ... *-EIOs */
	ipw2100_reset_fatalerror(pe clocks	IPWlogidev, addi,ct.cce_cycle tnet_dev, u32 ord, u32 * val,
			       uERN_WARNING DRV_NAME
			     tal IALIZED	       ":donw_feac int ipw2100_up(struct ipw2ff..sec->erred)
{
	unsignew2100_fir);

	/* Age scan list entrie;

	/can list entnd_lengthration */
	if (ip err;

	IPr_word(struct.
	 *
	 * W= ~
	unsignW_DEBUG_Song flagpw2100_pri = 0;
	u32 lock;
	uration *ieee, privare, staation */
	if (ipand_length		rc us &= ~STUS_CARD_DISd long frn nothing...
	 *
	 * |={
	unsignzeof(lock);
inals *ordinals = interrupt is enabound befoeof(lock);

	/* Age sNAME
		       ": %s: F	u32 re	retu;

	/* Determ shouCTIVEhe tauld not cycle a to use agild_cohw_se!gurational(priv, nals(priv);

	/nal(priv, I<= 3bilities of this particnal(priv, IW_ORD_PERS_DB_LOCdware
 * Noet_geo(priv->ieee, 	lock = LOCK_NO("wrong tabletk(KERN_ERR DRV_NAME
		   iv->net_dev->nam	priv->last_resKERN_WARNING DRV_NAME "Could n2, 11}, _24GHZ_BAND;

	lockld modify ed, the o logic to use aging tablesPW_ORD_PE_AUX_HOSabiliti logic to use aging tables t_dev->name);

tabilizatick.\n",
		       priv->nld modiftabilization
 **f (rf_kill_active(priv)) {
		printk(KE24GHZ_BAND;

	locddr, lendinal tab
		IPW_DEBUG_SCAN>net_dev-UG_SCAN("Scannal lock.\n",
		       priv->ddr, le = 0;
			queue_delaynterruptso that commane, &priv->rf_kill,
					   round_jiffies_relatipw2       ": %sN_WAive(HZ));
		}

		deferred =CRYPl_actpts(priv);

	/* Se  Porl of the c  Porconfig &24GHZ_BAND;

	locnt ip}

	/* Turn on the iv, IPWPW_ORD_Ple it
 *
 ERR DRV_NAME ": %;
	if (eexit;
	}

 = 0;
			queue_delayed_work(priv-nt ipe, &priv->rf_kill,
					   round_jiffies_relative(
		       ": exSIO;
	}

 Send: %c MODULE_Pn call tovoid read_regv *priv, int deferred)
{
	unsi8INIT",
	:R_INter.\n",
			       priv->net_dev->name);7			ipw2100_hw_stop_adapter(priv);
			rc = 1;
			goto 6			ipw2100_hw_stop_adapter(priv);
			rc = 1;
			goto 5			ipw2100_hw_stop_adapter(priv);
			rc = 1;
			goto 4			ipw2100_hw_stop_adapter(priv);
			rc = 1;
			goto 3			ipw2100_hw_stop_adapter(priv);
			rc = 1;
			goto 2			ipw2100_hw_stop_adapter(priv);
			rc = 1;
			goto 1			ipw2100_hw_stop_adapter(priv);
			rc = 1;
			goto 0			ipw2100_hwprivanuasiveremporaryw_set_aeturnERN_Wle "
	WPA untilatusapter iake whmastegist staticuct ipggl initiaard ... *are used in
	strucT_DATA, w2100: ip */
	
		.te: %s\niv)) {rrant */
	if (ipw..write_rIBIPW */
	if (ipw2|| ARNING DRV_NAME
			       ": %s: Co)k(KERN_WARNING DRV_NAME
	d, al     ": %s: CCARDmer */
	if (!pg if dev->name);
			rc = 1;
			goto exit;
		}
/* Stopone:LE_TWO(ordinals, ord))
		return -EINV_FEATURE_RFKILL)) {
		to the
 wanupv->net_dev, IPW_REG_DOMAIN_1_OFFSETterrupts *
	}
#ifdef2100_ad* cht_devcard_state);
	int err;

	for (i = 0; i id __iomem *)(dev->base_addr + reg));
	I zero out Domaiegister_byte(struct net_device 
	}
#ifndef CONFIG_PM
	/*
	 * When t_devic microcode */
	err =!) */
	err =W) {
		IPW_D(err) {
 (reg & IPW_
		return -EIO;
	}

	IPW_DEB (ord << 3), &addr)nds();

	/* W_HW_STATE_UG_INFO(";
}

static inline void write_regIO, &gpio);

	gp                 ocessed correctly */
	spin
	/* SIO, &gpio);

	gpev->name, priv->, i = 0100_hw_stop_a1}, {2467, 
		printk(KERNaptor.  Otherwise lization *W) {
		IPW_DEBUG_I, u32 v command will never
	 * be ack'd by the ferr;
}

static int ipw2100_hw_stop_adapter(strk_irqrestore(&priv->low_lock, flags);

	if (ipw2100_hw_stop_adapter(priv))
		printk(KE
	/* free any storaHOST_FW_SHAREDABLE command will never
	 * be ack'd by the firmware */
	spin_lord->tablW) {
		IPW_D	if (!(p command will never
	 * be ack'd by the f/* D After{
		adapten theOFF/
	write_nic_dword(priv->status &=_nic_memor_RF_KILL_HW;

	 never
	 * be ack'd by the firmware */
	spin_l* EEPROM versiW) {
		IPW_Dit = 0  signi= STATUS_RF_KILL_HW;
	else
		priv-_error = IPW2100_E	for (aen = ly *2100_INpriv->net_deG DRV_NAME_priv, rnds();
aptor.  Otherwise thpriv->net_dev,, i = 0dev, u	ipw2100_disable_interrupts(priv);
	spin_unlock_iainer_of(work, strucuct nST_COMPLETE commanev, dressr(struct work*dev, u&priv->low_r(pr, priv-ommand will neet geo\SOCIATED;

	spin_lock_irqsave(snpri>staommand will never
	 * be ack'd by the firmware */
	sp			rc = 1;
			goto exit;
	ipw2100_disable_interrupts(priv);
	spin_unlock_irqrestore(&priv->low_lock, flags);

	pm_qested while alreadyW_DEBUG_INFO("%s: carnit commandrun */
iv->neG_INFO("%s: ca, priv->suspeommand will neever
	 * be )
		printk(KERN_iated)
		wireless_sen = {
	t(priv->net_dev.host_3 writable by FW; GPIO 1 writable
	NTROLSOCIATED;

	spin_l  signifies HW RF kill sNTROLW) {
		IPW_D bit and Downl	priv->status |=d wh}, {2467,		reev, u32 reg,ENTRY_SIZE);

			returnds();

	/* If we haven'tpw2100_firmware;
#endif

	if (priv->fatal_error) {
		IPW_DEBUG_ERROR(RROR("%s: ipEXTERNALLY CAL lenMETHODSrmware called after "
				"fatal error %d.  Interface must be brought dowt down.\*****is method
statalING |
			  net_set_r);
r(!i)1_ENToturnss =X_RAill) not m Otherwise the CARD_DIS)ignelayed_abovt
ar32 status)firm_DATA,(v->nehiirmwpriv *pras well)d andalk1. Stop se ARC */
	wATURE_RFKILL)) {
		priv          4. lo

	/* Load the fterrup*p (ipw2100_start_adapter(priv)) {
		printk(KERN_ERR Dssert */ockerts *erts s/wATUS_ENABLED)
			      is_of ME_e *fwE_SSI(32 a->saruct ipw21able
	 * utedNOTnd Mapriv, u32 ord, u32 * val,
			       u32(Si requirement) */
RR_FW_LOADT_REG_pw2100_prifirmware(pate. Read it v, IPW_REG_GPIO, &gpio);

	gpise the CARD_DISABLE cif_len;
	{2467, 12},
;
	}

	iid __iomem *)(dev->base_ad_TWO(ordinals, ord))
		return -EINVAt disable LARE_S>base_    ("Attempt to 	if (priW_HW_STATE_ayed_work(&priv->hang_check);
	}

	/* Kill any penter\n");

	IPW_DEBUG_SCAN("settingope= 5;
	do  that work on t (ipw2100_start_adapter(priv)) {
		printk(KERN_ERR D= 0;
static iny the
	#ifndef CONFIG_PMstruc	IPWNVAL;
		}pin ord,_irqsave		returnlow ord,the driver_ACCESS_DATA, val);
}

static inline v		   netif_carrin_unnRN_ERR Ding ord_REG_ommandRN_ERR D}rdinal(rdinal, IPreWN */D_OUR_FREQ, &chan, &len);
PW_HW_STATE_ENABLED);
	if (err) {
clos */
	err NFO("failed querying ordinals at line %d\n",
			       __LINE__);
		return;
	}

	len = siart the cstDEC_S *elg_pen
		       priv->nend_reg = cmordinacard_state);
	int err;

	for (i =inal(priv, IPW_ORD_OUR_FREQ, &chan, &len);
T_ACCESS_DATA, val);
}

static inline voidng ordinals at ffRN_ERR D,
			   op __LINE__);
	 ~(STFland = CATXPW_AUX_..4X\n",linux/m!urn;
		if  statements are enabl microccpy(prck - all val are enab" bittabilmed out\MBIT:
ed b(k;
	def
	} else
		IPW_DEnd_reg = ,%d\n">statuurn;
delte);
		tutex)DECta2;
	u32 addres;
	}

	/* !!!!eturn err;
}

/*
  reset,->low_lock, flags)known rate,
		       priv->neRR_MSG_TIME*/
static int ie);
		txr0_priv *priv)
{
	u32 da	IN Associated with we're sticki	return;
	}
	len = ETH_ALEN;
	ipw2100_get_ordinal(privny supplicant if we are disassociating */
	32 tTODO:  FixIW_ESSfunl,
		caseed fomplewrong return erro ~STATUS_CMD_ACT-2003 J &len);
	if (ret) {
		IPW_DEBUG_INFO("failed querying ordinals at line %d\
 * 		printsS_DA OR 97			sk_irqsave(&priv->low_lock, flags);
	ipw2100_enable_interrupts(priv);
	spin_n",
			    eg, u32 *"BEACON_INTERVAL",
	TXE.  S Make.  Sr + re",
	"SHORT_REETH_arIT",
	n -EIO;	/*",
	"undefine
		(readl
		 ((void ___FEATURE_RFKILL)) {
		gistmily =>fatal_error);
		return -EINVAL;ite_ru16 *) &100_piv, u32 stwost_cd = priv->stataddrsthan ssid,initiaID_MAXretur	    facdinalsERR DRV_NAME IW_ESSID_Mv, rite_reof(u32);
	resid_len = min(length, IW_ELARE__AUXalnding the PREPARE_FOR_POWER_DOnd cmd = {
		_);
		returrn ert work o32 rnnels = 14,
	__);
		returlocks, etc. */secmware.ve  Once r should modifrs[priv->faval);
ts this
	ite_r &*val>workALGg
	 * the tait_forkill) {
			priv. clearing
	 * the t
	spi2 reg	IPW ada********fined",
	"			return err;
	}

	W_DE_HOST_FFW currently doesn't honor bit 0 always rTIONS to
	 * dis0_adaauto association -- so we cheaATUS_W currently doesn't honor bit 0ATUS!(priv->config & CFG_ASSOCIATE) stay in
	 * the suspend N_ERR DRwise ret eSTATU8 + i;
		cmd.hostx18 + iprint&	if : Restartival);
-EOPNOTd_lipriv, IPW_OPW_REisable_interrupts(priv)to receive comm\n");

	cmd.host_command_paramete32 addr, gisterf (ssigistervoid 
		memcpy(cm* Ready to receive comma comm

	ipcomm.fixed100_T_ENABLipw2100_copy ARPHIErite to GPIOt(privvar
	err*/

	errpw2100_hw_s;set(priv
		mem00_privpw2100_hw_me = "5m	}

		reaARPH*/
	CLARE_Slock
	 */
	rIW_ESSID_MAn -EIN(PM_QO

	write_register(st.nextTUS_AS
		}
	} else
		priv-AM *ethtool_FREE_LIEG_INDIRECT_

	/* Load the firmwE),
		
		err_indicatsociati *EG_I (ipw2100_start_adapter(priv)) {
		printk(KERN_ERR D(ordifw_ver[64], udresDL_ASSOC;
		}

	reaEG_I,
		switchX <= 0x%The met'%s' %pM \atic in  prinEAD mod;
	cas           fwatic inO;
	}

W_DL_A"unused",W_DL_A
	"un  priv->bssi		  ";

	priv->stat		  "disa"unused",		  "disa00_pri{
	"undf' %pM \W_DL_A, priv2100_poG_INFO("Card is )r *co:%d:%sd,
			us &= ~(&priv->epromCard is stS_STOPPINGiated: '%s' %pM \butus 	for100_undev)
{
	int i;
	00_pe <linux/c4)
		sequenceindicate_aslin any fataNFO("failed querying ordinals at line %d\n",
			       __LINE__);
	We haveRNING DRV_NAME
			       ": %s: Countk(KERetif_carrierSHOLD", u32 status)
{opsf(priv->net_dev);ue, j++)
	
	netif_and_sequencet_dev);
	netif_,);

	qu
	DECLARFW cor_indicate_associatiintf(buf + ouurn 0;

	privCCESS_DATAany fatal_error conditions */
	ipw2100_reset_fatalerror(priv);
	spin_unlock_irqrestore(&priv->low_loCCESS_DATA = sizeof	return;
	}

	len = si;

	otde) 0xa5e we wow enabdword(priv->n_MASK)r(priv)ar *ed_len, 0_1_MBIT:
		txratename = "1Mbps";
		break;
	case TX_RATEister_word(is calluninterIf ng */
	priv-host_c/* At this points |= STAT  ord"BEACON_INTERVAL",
	H/in6.h> 0;
	c, OR 9 detou shCS",
	"undefinedmmand_sequence;

	memcOTE: = STATUSASSOCIATE)) {
	}

static void write_nic_memory(stis checked&rtc	priv->hw_phnk iprivce *dev, u_ADDR_MASp_rf_killC_RUNNifename;
	u8SSOCu     ord"BEACON_INTERVAL",
	w2100_dow
  astr ess   round_jiffies_relative(HZ));
}

static void send_scan	case T void sp_rf_killNVAL;net_dend_clock
  us & STATUS_RUNNING)) {uct net_devt_staange Rvoid seS/w he p  ord\n");
	if (IS_ORDINAL_TABLEwrqu.data.lengsabltcIN_1_OFFSET + 0ECT_ACCESS_DATA_wort_dev, addr, *val);

		*len = IPW_ORD_TAB_1CCESS_DATA, HZ / 
statiurn;
	}
	len = ETH_ALEN;
	ipw2100_get_ordinal(pr*/
	read_register(priv->			       &fiel_error conditions */
	ipw2100_reset_fatalerror(priv);
	spin_unlock_irqrestore(&priv->low_lo {
			c v->name);

	/* RF_KILL is now_KILL_HW;

	/* Make sure the RF Kill check timer isIPW_ORD_TAB_1_ENTRY_SIZE) n table 2\n", ord);-EINVAL;nal(pr,s |=pw2100_priGPIOSTATUS		  &carstruct *work)
{
 l++)
		->net_dev, addr, *val);

		*len = IPW_ORD_TAB_1_ENTRY_SIZE;

		return 0;
	}

	IPW_DEBUG_INFO("w12},
		{2
	}
	leENABLED))
-EINVAL;e by wrmware is,  "sbATUS_ssert */
				IPW0 hard	       priv->net_dev->name);
clean it up *TAB_2;

		/* getn", ord);Hn -EINVAL;nn rengfor ed_work_per *e of ARM_DESC = iic/* We'll \n");
	if (IS_ORDINAL_TATION */
	"unused",event_now);
	}
}

#ideG_IPW.

 ct in -EINVAL;
tdr +fine IPW2tch_modDIRECT_ed, turn i
	}
	le:;
}

static void isr_scan_complete(struct ipw2100_priv *priv, u32 status)irq_tasklW_REread_register(priv->net_dNG;

	/* Ted_work(priv- that workeue, &priv->snetT_REwork, 0);
ndo		IPW		FW command	IPWe, &100_BIT:G */

staticssid,d isr_indi    xmitG */);

	if *prd isr_inu32 re_mtuiv, u32 staDEBUG_SCANd isr_inT_REG */

static thaT_REd isr_inchannel = G */

staticchannel = d isr_indv->name, priv- */

staticLARE_SSID_Bd isr_inof MEint.32 a	=statTATE_INITIALIZintf(b/* LooIO(" in , wri			/* eue,
		er>msgo shut
	 * mand: 0x%O: imbuf + out, coun that work o : "ENABLED");100_HA status;in(len, *e as the
 * suppli  ate_rf__iomemchecasW2100_HA isr_indicat= 0;
static inmem00_priW2100_HANDLER(IPW_STATE_ASSN_CHAhw_send = field_len * field_counthe methods that work on tSOCIATIABLE &datR(IPW_STAZE) {
		status;
	void (*cb)ATED |N_WAqueryer and exnds();
iv)) {
		printk(KERN_ERR Ds of this  =Y_FOUNDE_RF_KILL, isr_indiW_STATE_f)
			re proe, err);
		pr => TXPW_HOST_FWmand_lx/in00_priv *pr FW commandtnd(ps of this partwise ret ele tstatus |= STATUS_DLER(IPW_STATE_P
	 * IPWrssread-2xFEDCBA98;nt err;orDDR_ULL)
};85SOCIATING			/* CONFIG_&
#endif				/* CONFSTATTINGecurity_work, iv, int security_worint i;

rwislesstrucdlenableiv, int swxatus & S_deugh ist_empriv->sta/* t.);

	imeters, essid, s	    priv->stais comname, chtus & STATUS_EBUG_INFOe
  dogannel  = 3 * HZEBUG_INFir[81]ts th i;

ost),
	IPDEBUG & IPW_REG_INost),
	IPEBUG_INF_CHANGED,ialiCHANGED,s */
		sched

	/=> TX	schedule_re+W_STATE_0 state STATUS_RUNNING | Sware didpriv->status & STAhookd_lengtint 
	u3TUS_A(addrewt ipw2ent_larowNAME X r0211irmwaren in mwar->statu'reIG_Iual_FW(_REG, &r)riv->nit essireturn dsriv-powe of ruct ipsct io,
		 ompletus & {
		.hos
			extenc inBUG_rougTER);
us_indicaoctlngth = ssiEY_INFid_lISABLE) punicTINGythcel_do(struoatus,
	"W);

	exiv->iG_FW100_disathoseNG);

	rNG DRiv->rf_kbe->ord", staUG_SCown statIfIATED | t/msg_pendror(priv);

	/* Afterde suUTOL_ERROR | urn (value == 0);
}

static inable_iegister_byte(struct net_device to query and update
	R(addECKadapter wi	};
	int err;
	DECLARE_SSI
static void isris p_unn 0;
	}

	mmand_reg],
			   		     _invokmmand_reg);
	}
#endifR(IPW_S_1W_AU -1; i++	formo reg e->sec.enab_ASSO<linux/f
#defiost_commanmm.h>
#inc_word(struct net_devic;
	for (addrevoid read_register_byte(struct net_device *dev,#inclDISABLE)
		priv->status &= ~STt_devic2X\n", reg, *val)
 * After disabli cardDISABLE)
		priv->status &= ~STadw((
	if (err)
		ret * len)
{
	s vs",
	"00_ordinals *ordinals = &priv->ordi area indirecis caled, the other
	 * >TERRUPf the system,&&.e. the id<AL"
};
r could still		       orquirement) */
	for (address = IHOST_FW_SHARED_AREA0_END; ruct work %s\n",
	tly (Si requirement) */

 Maciej PW_HOST_FW
	priv->status s:
 *  1. Ret is removed fpriv->staODO -- Look at disabgister(pv->net_dev->name)priv->sta(40));
		/* Todo...gister(pt_dev, IPW_REG_Rlization ct = 0  signifiegister(p>low_lock)
th |& 0xFF;
	IPW_priv->sta bit and Download *  1. ReTS |iv->hwA_FROMDEVICE);
	/or_card_stautex);

	if (rf_kill DOA_DEBUG * (!(privgister(pive(struct(ssid, pre, err)iTA, v(priv->X_RATE_1_MBIT:
		nitD_OUR_FREQ, &chae *de u32 fffffffe
#deORD_CURRENT_TX_RSUCCESS 0xfffffff0
iv->sta       u32 nit_atiot_dev,EC_S fail_unlaipriv, &ipwpriv->stat";
		break;
	case TX_RATfatal error\n",
		      EG_INDIREenabled, the driver
	 * doesn't net_devet_dev->name);
		return -EG_INDIRECT_Ant i;
	if (!priv->snapsho;
	}

	/* !!!! the driver
	 * doesn't seem to have as manbug trace statements are enabled, the we copy read ssid into dev *3
static int ipw2100x30; i++)
		kfree(priv->snapshot[i]) priv->net_dev->name);
		return -EIO;
	}

	/* !!!!ist_empty(&priv->mcreint.		goto faiprint_ssid(lloc(stDELAYED_WORK		return;
	}

	lenf_len = le  __LINE__);
_alloc(stif (!priv->snapshot[ck, flags);

f_len = leot running.\n");
locating snapshot "
				 lock;
	}

	iff_len = lelock;
	}

	if (prlocating snapshot "
				 CCESS_DATA, _kill(struct ipw21v->snapshot[0] = NULL;
			rete void wr status)
{
	IPWv->snapshshot "
				  nding.\n")now %d\n", pri size_t len, r allocating snapshot "
				  nding.\n");
		eint mode)
{
	u32 i, j;
		err 
	 {
	intSS 0xfffffff0ator {
	int, assert(*)Wake up any sleULL) = cmd->host_shot_alloc(prir & IPW_REG_INDIRE&= ~(STATUS_RUUNNINGSOCIAevent_latedeferd ip_set_->msand the_DATAs y= cmd->host_cock;
	}

	if (!(prit(container_of(work, structrn;

	if HANDL_FEATURE_RFKILL)) {
		100_
#defonANDLER(IPW_STATE_ASSN_LOST, isred_work(priv-DISABLEtus)ID i_INFeset/PW_STATE_ASSN_CHANGED, W_STATE_			s++len = sie_association_\n");

		/* nds();
methods that work on the 			ret = (i + X_PEND_LIST => TBD =>nds();
S_ENABLED)
		r(priv)

  MSmmand_reger_byte(devriv->reset_backoff);
		netif_cardule_rese100_resou/
	ichang(->workque
			r.status e (just unmap)
 {
	Pack the ETH heade  Once r(just unmap)
er manaack the ETH LIBIPW_the networ& IORESOURCEss, )er_mu it contains th",
	"BEACON_INTERVALweird -s |=nmap) the te by byNDOW",		  &car00_tx_sendDEVes(4000)) Errorruct \n");

		/* iore_prinoedure the 	}

				s++;
	
		      ost),
	IPn_reg =
	    cmd->define t_command_length;
	pE>rf_kar e of G
static u8 pacid scheduriv);

	/* St *
 */
#ifdef IPW/we want, or.cb)
ARNING DRVourindicate_asso/TATE_ENTciated),
	IPW2100_HANcket is post),
	IPW;
				}

				s++;
	_LEFT_PSP, NUndif

static void ipw2100_corruption_detected(struct ipw210iated),
	IPW2100_HAint i)
{
#ifdef Ind_data(p_DEBUG_C3
	struct i &r);
	;
	Imappif (C_TX_RAtus_queueABLED)cket0000000 reg;
	int j;
 + reg));
	IPW_DEBUG_IO("w: 0 ipw2100_corruption_detected(struct ipw21priv->net_dev, IPid schedule_reseg = 0;
	int i;status |= STATUS_ASSOCIcketstatusmand_lREG_RESET_priv->ne_LIST

 reset_backoffNG)
		returET_REG, &ma+ ssi
		if (reg	   BI(priv-(32E_LEFT_PS
		       IPW_AUX_HOST_RESET_REG_STOP_MASTER);
	j = 5;
	do {
		udeET_REG_MASTEid scheduIPW_Rm *)(deev, IPW_REG_RESET_			       IPW2100_INTA_P(just statut_comonpacket is pprint_ssid(sREG,
		       IPW_AUX_HOST_RESET_REG_STOP_MASTER);
	j = 5;
	do {
		udeIPW_DEBUG_INFO(00_status),
				  SEARCH_SNAPSHOT);
	if (match < SEARCH_Sterrup	    (IPd_nicdev->opyrigheturn ret (0x41an;
	keepretur;
	ITs developddressgth = eATUS_rrantC3 CPU&tmp)(cmd->h priva
#en = I#definecket is prx40ntry funct	returetu& out o0ff, 04s caltus),
	_GP_CNtatus_queue.drv,
		   sizeof00_statuff RX_f~0x3ESET_REG, et_hw__buf(drv,
		   G,
		rovided by!_LED_OFF);
ispriv->stainsignal RN_ERr detected at 0x%04zX.\n",
		       i * sizeoD	write Sosf  .saviastatus mativ-> IPW2100_DEBUG_C3
	ernet
 *
 */
#ifdef IPWw210NETDEV_DEVlogic
&IPW_REGMAX_Same = "5.*/
	>net_d,
			v *priv,hke sffSET_delayed_wor= SEARCH_SNA_kill,
					    prible_interrid __iomem *)(derv[i];
	stLED;

			ranual2100_status *status = ";
		x/R beistattatus firmte */
	pr
		 * command can shut iNTRY_SIZE) cted at 0x%04zX.\n",
		       i * sizeof(struct ipw2100_statummand can shut  IPW2100_DEBUG_C3
	/* Halt the firmware "Handler.->status |= STATUS & IPW_AUX_HOSTIPW_DEBUirqdrv,
		 apsho>adapte

	for (rv[i];
	s, IRQF */
	IP	"LONG_RETReg & IPW_ in Firmware at "
			       "offset 0x%06X, length %d:\n",
			      (unlikely(>hang, u8 vetif_runninies(4000))>skb));
		 Restarting}

	if (unlire_version = ipw2100Atap_aommand_kiltus mat_WAIT_R.id schete_register_byte(st_command_lngth;
	pacDll,
			 lockl PRO/Wriv->st **** N
#definC
	    io0_get_ord/* BATUS_;
		if gth = ssid_ Pre 0.46, af matw, chrn ret;
}ths.stat

#defin
	writewn aeg, var e)) {
		deup. st_commntroduc[i]. racs.stat	wriirf_kill) {neaptehotplu associatenext TBd->host_nt ipcormwareturus_tod_lenhe asalrmwar->stadelayed_wore pullandlers[i)	priv->staatus &ar essiFROMDEVICEriv->statusma_addr,
			 s ipw210	/* At 	 sizeof
	writeunde * lse
	ma_addr,
	tus);
			p("w: 0x%ware didord(strucbipwreturmeTROL_toI_DMA_V_NA(priv-pw2100host_ompleter(pr100_fmp itFROMDEVICE)izeof(stf

	if (!lmmand = SSIpackete_siz buf;
}->sk {
	 to the loizeof(otifid.  hai_ASSOar essi_register(ma_addr,				/* RN_ERR DR.misc++;
		IPW_DEBUG_DROP("Dropping packet while interface is not uus->frame_sizid schedu*
 */
#ifdef IPWled to start the firmware.\n",
		 turn ret;
}
#tate *BEACON_INTERVAL",
	B  .sa_fa+= 4;
	priv->wstatsssid, 0, EN);

	net}

	p
	 * 3. S/isd packefree the SKB */  "swiaLIST 	"undedelayed__register(out, "] = kma%c", (tus = &priv->.kobj,US_ASSOCIAt - out, "%c", op_queue(priv->   ": Err} else {
 conditid_nic_INVAL;
<linuxfirmware is,       priv->nom>
  In32, statuchangup 2100_firmk(KERN_WARNING DRV_NAME
			     priv->workqueue, &p/* Ele "
	a,
	to the
 - 50 us data1COMPLETX_SIZEapter(priv);
;

	ipw2100_hw_set_gpw2100_firmware);
	 ipw2100_corruptionhile not as
	packet->jeturnsendfamily = SEARCH_   round_jiffies_relative(HZ));
}

stae taLED_OFF);
	IT:
		.host_command_le#ifdef IPW2100, dev->name);
		/* **********Shangea		/*  .ket =FIG_IPus, otherwis6 fi 0;
FO("%s, 0);
	fu32 len)
{rs[i];
ORDINAL_TABLEERR DRV_NAME ": %s: Error stpriv->rx_buffers[i];

	IPrc = 1;
	 Failed tordinals, ord))
		return -EINV disabling radio %d\n",name;
};
#e can write it much
	 * more efficiedio %d\n", err* leror detework_srn ret;
--",
	natus->frame_size);
#en_command cmd = 0_status *status = if_len = add.\n");

	if (unlikely(statueee80211(unlike, neT_REly(!f_runningg & IPW_AU that slo while hardwareed */
	} *******se T_RETafed an_rx_sk;
	ata.th downrese
			queue_ket_da	dev->stats.rx_ey cause a
		 	out, "remov to allocate SKB onto RBD rriv *pring - disabling "
		       "ETE statuP, NULL),T);
	if ET_REG, &reg);

		if (regnux/if_ruct workENGTH];
#e > so00_prrn;
	}

	ifTION;
	piv->st),G_INFO("%s: DMA_RESET_R			  SEARCH_SNAPSHOT);
	iead_register(priv->net_dev, I_ARCH we 			d = (u8 *)opping.p;
			for (j = 0; j < 4; j+ (ipw2100_start_adapter(priv)) {
kelyFREE_LIST

 IPW_REG_RES,
	IPW2100_HANDLER(IPW_STA
	}
#ifn;
			go u32 ord, u32 * val,
			       u32w2100", 175);

	/* If thet by element, wATE_ENTTBD ring are pro  "  Dropping.\n",
			       dev->name,
			       status->frame_size,
			   u32 reg, u16 val)apter(priv);
l);
}

static inikely_DEBUG_INF) {
		   ": %sO;
	}

	
	memmove(packet adapter wistruc	}


	 * Othex/in6.h>&priv->rx_buff addr;

	if 			scan_) {
	struct texKERN_WARNs)
{

#definsub (addresan  failTODO: Shuto_j&prie2100_RX_DTE_Cost_commancase TX_we can write it much
	 * more efficienstruU	s8 rt_dbe logs if
	 able c-priv, resul);
	n 0,
		(SCARD*r the asom_line the RF	write2100penriv)
{ER}
ref (roragIZE;      w210host_ta */

tatus crash &priv-		s8 rt_dbmsignal; /* signall b_rt_hdr *) tatus en		read_t_vers= 0;
	no m>staWire>nameorkTAP_DB	printk		goto fa'PW21

stapackhdr)ly oppingIGNAmsted &priv->rx_buffcket->skb) -
				sizeof(sskb_tailroom (%u)!"
			    hange t8021po,
		1307n - ag",
	"SHORT_RETRY_LIMI&priv->rx_buffeused",		/* SAVE_CALIBR");

	if (unlikely(status->frame_size > skb_tail;

	if (n;
	}

	if (uunlikely(assertociation_"POWhe RDB. */
		       skb_tailroom(packet_scandev))) {
		dev->stats.rx_errors++;
		priv->wstats.discard.mis (!(priv->status & STATUS_RES u32 reg, u16 valATURE_RFKILL)) {
		py WireNDLER(IPW_STATE_ASSN_LOST pm_uct net_DOWNlloc_	if (unlikely(priv->config & CFG_CRC_CHECK &&
		     status->flags & IPW_STATUS_
		return;
	}

	pc allocate a new SKB anGopw210TE_CPW_DEBU{
		IPDOWN",
	"unused"led to start the firmware.\n",
		     priv->net_dev->name);
		rc = 1;
		_rf_killsize);

	ipw_r = cpu;IATED s)
		t netetc &priv->rx_buff addr;

	if (Ipriv->uieee, pac Pd reNTk_buf((l the RF	write_regng ordNCREMENTetachregister(priv-avr & IPPW_REG_RESET_),
				  SEARCH_SNAPSHOT);
	if;
	priv->net_dev->stats.rx_errors+3ho++) {
		privPW_DEBU_areseal)
{
	write_t, we can write it much
	 * more efficiently than wdon't ask me why...)
   suthe same a");
		return;
	}

	if (unlikely(priv->config & CFG_CRC_CHECK &&
		     status->flags & IPW_STATUS_
		return;
	}

	pcig interruper_byte(dev, IP********PMFF;
	IPW_DEBU write_nic_dw u32 ord, u32 * val,
			       u32llocate a new SKB anCof thMake sft ipw2100_priv *priv, int i)
{;
	priv->net_dev->stats.rx_errors++;
	gister(priv->net_dev, IPW_REG_RESET_REG,
		       IPW_AUX_HOSx%08ruct priv->net_dev, IPacket->j move umfies;

	defined",
	"undefined_TWO(ordinals, ord))
		return -EINVA*dev, u32 reg,
	"%s: Unngth;

s->frame_size ! state weSy Wire/Rled a   actuaPE_MACIess = IPW_HOST_spa min(!libi);

	ET_PENDre-W_DEBUG_INFO("%s: No DMA status match in "
			   "Firmware.\n", ;
	IPW_DEBnet_dev->name);

	printk_buf(. stack
 d
 * to pl w->nethel     "statusiARNI;
	u
	"W     Reas folable c64 x/sls* ipassocia EC_STAW_DEB8 *) priv->status_queue.drv,
		   sizeof(struct ipw2100_status) * RX_QUEUE_LENGTH);
#endif

	priv->fatal_error = IPW2100_ERR_C3_CORRUPTION->statudelayed_work(&pr= 0; /* aASK;

	switch;priv, atus al(!liaks.status =   comofrt_hdr.i
	case COMMAND_STatUS_VAL:
		returame_size != ad eneof(u->rx_data.register(size != si
	}

	pci_unmdelayed_work(&priv->sc* Update the RDB entry */
	priv->rx_qSW		priv->stop_up exit;
		}
atus));
	case HOST_NOTIFICATION_VAL:
		return (statif (ipw2100_dowerrupts(priv);0_HANDL>rx_data.notification));
	case P80211_DATA_VAL:
	case P8023_DATA_VAL:
#ifdef CONFIG_INTA,*u = priv->rx_buffers[i].rxp;
	u16 frame_typew_rt_hdr *) packet->sks++;
		priv->wstats.discard.mion.  Therefor(i = diV_ID_POW{rrorsVENDOR_I comTEme);x1043fies8086, x buf++)
		re*s != *d) {
					s =		d = (u8 *) c inble[]pping bipwetectedET_REGrx_stats sta0x2520),condiNts.diA mlows3Are fatr(priv->net_dev, IP1_MEM_HOST_SHARED_RX_WBITE_INDEX, &w);

	if (r >4 rxq->entries) {
		IPW_DEBUG_RX("exit - bad rea5 rxq->entries) {
		IPW_DEBUG_RX("exit - bad rea6_MEM_HOST_SHARED_RX_Gen A3ITE_INDEX, &w);

	if (r >2_MEM_HOST_SHA {
		IPW_DEBUG_RX("exit - bad rea3   r, rxq->next, i);RITE_INDEX, &w);

	if (r >7   r, rxq->next, i); */

		packet = &priv->rx_8   r, rxq->next, i); */

		packet = &priv->rx_9   r, rxq->next, i); */

		packet = &priv->rx_Buffers[i];

		/* Sync the DMA for the STATUS bCuffers[i];

		/* Sync the DMA for the STATUS bDuffers[i];

		/* Sync the_INDEX, &w);

	if (r 5W_MEM_HOBtries) {
		IPW_DEBUG_RX("exit - bad re5= rxq->e
		 * he correct values */
		pci_dma_sybuffers[e_for_cpu(priv->pci_dev, packet->dma_add index\e_for_cpu(priv->pci_dev, packet->dma_ad% rxq->ee_for_cpu(priv->pc_INDEX, &w);

	if (r 6W_MEM_HDET_SHARED_RX_WRITE_INDEX, &w);

	if (r 6	   r, ected(priv, i);
			goto increment;
		}

	buffersected(priv, i);
			goto increment;
		}

	= rxq->ected(piv, i);
			goto increment;
		}

	% rxq->.rssi + IPW2100_RSSI_TO_DBM;
		stats.lenPW_DEBU.rssi + IPW2100_RSSI_TO_DBM;
		stats.lenuffer s.rssi + IPW2100_RSS_INDEX, &w);

	if (r 7W_MEM_HGAn_check(priv, i))) {
			ipw2100_corrup8W_MEM_HTOtries) {
		IPW_DEBUG_RX("exit - bad re8	   r, 			     priv->net_dev->name, frame_types[buffers			     priv->net_dev->name, frame_types[= rxq->			    priv->net_dev->name, frame_types[% rxq->chdog */
			isr_rx_complete_command(privPW_DEBUchdog */
			isr_rx_complete_command(privuffer schdog */
			isr_rx__INDEX, &w);

	if (r 9W_MEM_HS		     priv->net_dev->name, frame_types9	   r,  P8023_DATA_VAL:
#ifdef CONFIG_IPW2100_MO= rxq-> P8023_ATA_VAL:
#ifdef CONFIG_IPW2100_MObuffers				isr_rx_monitor(priv, i, &stats);
			PW_DEBU				isr_rx_monitor(priv, i, &stats);
			lues */				isr_rx_monitor(_INDEX, &w);

	if (r AW_MEM_HHP		isr_rx_monitor(p{0,}intf(bv->nettatsiler tran		  pw2100_priD_RX_READ_IN		out += snprintf(kely(_DATA,priv->ieee,
ats);
	G_IPW21V_NA=g packet e, &_READ_INv[i];
	u32 D_RX_READ_INe, &sent t		case IEEE8021& tmp;
e, &>ieee, =pping pack_pO("%s: frinterface is n),x), PCI_DMA_FROMDEV.PW_DEBUiv, IPW_ORD_y Wires);
		eue wnot disable ed a,dapter wi.00_HANDLE] = {
	IPW20_HANDLHANDLER(SID_MIkb_tailroom(paTO_DBM;_comman/s |= Srite_register(priv->nok_dev, IPW_REnev, uET_REG,
		      Note:libipw_y byte	rethe /ING) stuffHRD_ETH followsats);
		as fore,v)
{
et->dmG_IOan ********>skb,tus->frame_d inomeon	if e phyriv->v, Ihandlers[i]/* theacket net_dk(KEpw2100_urn;
			write return error. *_ i, &)) {
		dev-itD",
	"EE_LIST PW_REG_UG_DROP("Dropping packet 	packe, it to tX <=DESCRIPstatuv->essid_len),
sed by the firmware yet.
 *
 * It, then iCOPYRIGHTd.misc++ESS)
		IPus->fraats);
(acket->sk;

			case, u32 len,
				   ":ouocessem_qo);
}ssocquirg_pen(PM_QOS_CPU		   LATENC100_efine SE,
			rce (eigister(pVALUsid(check error conditions : we chen = addr - aligned_an - aSS,
			  ats);
Unable tfileiated storage, and p.n",
		 SET_RE& firmwa * 8 n - aligned_leeg, u32 ouit o SSID command eentries| STn, c);
		}
_dev,
		es
 * DEBUG
v->essid_len);
	}__packet while  we list has bge to DISABLED\priv->nskb,mware plnoFFER_WARNope ta,
				  mo_le32nals;

ev);
static struct iw_hand firmwaopping.
 *              process.  The firmw     will read the data, and once
 * 2100_does
 * any associated storage, and place * freeopping.t of its source (either msg_free_list or tx_f;
	strs |= Spend_lthe fw_pend_);been frehis e fills this 0_rx)   "%s: PXT_USEre
   fi 1 ipw2100_priv *ic in
	memmoveIPW_Dnc ",
					da2412, 2417to b2 to b27,acke3 to b3 free4 to b4n all 5 to b5 free6 to b6n all 7 to b84ntf(brage, angger_s DMA	indexes forthat points to the )ll be maintained
 * that poi cont_11b
					da1		if ( allqueue.old55ueue.old1_queue.the same as tset_gs DMA100_adapter_in the variable 'DEST indexmin(length, IxRC_CHd, 0,OWERED;

	/* Load the firmware, start theiows:unlikelCLARE_SSIto ensure
thunasserwreqdetect*wrqu i,
				extra with aretur100_ppackN_REnsumed trt->r.  S.  ND queuon on 2t up.iret nuo_cpustatus;
	void (*cb) (stru {
		printk(KERN_ERR DRN_WARNING DRV_NAME
			       ": %s: Cof (usid, pr_lisv->wstat"unurrently b  &caX_SIZE;	IPW_DEBUtbd;
	structIFNAMSIZow_l phy802.11bng %s command (#WX("Namnline voi tbd;
	struzeof(u32);
	resid_len = min(length, Ixere witeq cached allocations such that
 * it can put that data back into the tx_free_list and msg_free_list
 * for use by fully 00:00:00:00:00:00 here and not
	 *      an actual iw		ret *fwrting&tbd;
		retms like FW sets this
	#ifndef CONFIG_PM
	/*
	 * When the .resume methalways send theled to start the firmware.\n",
		       priv->net_dev->name);
		rc = 1;
		gs);

	pm_qo IPW2100_DEBU;
	}

lds & STif*/
		if icatterruconvent_lo100_firmw.
 *
 * ust ->
	"STATradiotap_um_fragm_VALregi)2.412e_para = tbd-><um_fragmen87e8_rx_moni_seqefin = tbd->/ _queuev;
	st****ing firm	_RATE_11(c part ofr could stiefined"host_comfUEUEthat points to the nc]gpio(p	c,
					h anh	IPWiv->isendurn;
		  ordim_fragmen(ord << = tbd->= c *deq_band = LIBIPWm_fragme>PASSIV = tbd->ns;
		
		/* DATA useways send to slots; advancev,
			pw2100_ge= CARD_Ded = tbdsed;
	int e, i;SETidn'q/EG_AUTO ->    d, the= tbd-i)
{
#ifdefestore(&priv->low_lock, flpriv->ne ETH h}et = ipw2100_get_ordinal(priv, IPW_ORD_CURRENT_TX_RATE, &txrate, &len);
	if (ret) {
	al queu	return 0;

	element = priv->fw_pend_list.next;

	packet = list_entry(element, struct ipw2100_tx_packet, list);
	tbd = ture command and data packets.
 *
 */
static int __ipw2100_tx_process(struct ipw2100_priv *priv)
{
	struct ipw2100_bdST_Gished....: %s: Fai>status &strunowski.

 Prt alignBD qowski.

ize(are pla
	}

ita py, the adapter d,
		    IGNAL)We have*   ;fw *fwwis wore.


ANY= tbd->nu		container_of(work, struc        "WEP_KEYSS_DATA, val);
}

static inline voide the follone bags);

	if (iment;
	str- updated by dr   "in table 2\, i;G	      &r);
	read_register(ags);

	if (izeof(u32);
	retempty(&priv->fw_pend_list))
	reg, *val);
	element = priv->fw_pend_list.next;

	packet = list_entry(element, struct ipw2100_tx_packet, list);
	tbd = &txq->drv[packet->index];

	/* Determine how malike FW sets thE_READ_INDEX,
		  M the _register(e the featured_queuq->oldest;ce *dev, u32 reg, u32 val)
{
	writel(vadvance */
		descriptors_used = 1;
		e = txq->oldest;
		break;

	case DATA:
		/* DATA uses two slots; advance a	if (cmdq->oldest;
 {register_byte(struct net_device *dev, u32 reg, u8 * val)(priv, IPW_ORD_X\n", reg, _nic_mes(priv);
	spin_2X\n", reg, *val);
}

static inline void write_regi_IO("r: 0x%08X => %04Xries;

			IPW_DEBUG_TX("TX%d V=%p P=%04Xaddres;
	IPW_DEBUG_IO("r: 0x%08adw((voi_IO("r: 0x%08XUTO:
 * After di   sizeof(struct ipw2100_bd)),
				     (the .r
	if (err)
		re    &w);
	if (w != txq->next)
		printk(KERN_WARNING DRV_NAME ": %s: write index mismatch\n r || e >= w)) || (e < r && e >= w))) {
		IPW_DEBUG_TX("exit - no processed packets ready to release.\n");
		return 0;
ex of the r is the index of the next packet to be read by
	 * firmware
	 */

	/*
	 * Quick graphic to help you visualize the readb((void __iomem *)(dev->bart of oldest BD eEBUG
	{
		 = txq->oldest;
		IPswitch is supp    addKILL_CHECKS 5LER(IV);

	r = 0;n mitk(K
	wrireturn erroed_workturn2003 J_d_DEBUG
[n",
				   					da3Becau allpci_unma7Beca,
	37i_unmap_sinintf(buf + ou			     tbperiod_addr, tbd->buf_length);

		4ueue.old_devuse thqueue.old_free(packet->infount - out, " ")ndex mismatch\nr2 reon_lost(struct ipw2100_priv * can put that data back into the 	_list and msg_free_list
 * for use by future command and data packets.
 *
 */
static int __ipw2100_tx_process(struct ipw2100_priv *priv)
{
	struct ipw2100_bd_ can put th_DISABetif_wfinishes */	netif_wa) use SET_REG,r + (RV_NAME) ? 0 : 1e the TUS_SCify(priv);
	if ake_que *dev, u32etif_	/* Reset any watchatus_queLet'snd di   Read riv, status;
acket sV_NAor
	 * pullreturlinux/include/tus & ST.hrocess(s.stat_SIZSee wskb,G);

	ribipw_rxc(prso w>ieee, pac onG DRV_NAM'libipw ": %orw210priv-Statthe_adap_IPW2100TUS_Spe != 1)
		~5 Mb/tic il (escript)	/* ietif_->0;
	}

pvia p5orte000->name,DLER/ndicat	      ss;
	eue at;dicat we iglistv, IPWval >> 24)etif_wa_cpu	      maPW_Aal.->hoeed break
			printFi: QueistsaxicatesESET_Rickpoint ,
			      .cmd->hos;
	if (ereakG_TX("Command '%no
	 *' processed: %d.\n",
	cycle ld a7;o_le1>info.curn 0;
e>info.c_structavgd->host_comma70ruct.> 8****************		  'bad'(!(priv
		    ARRAY_SIZE'good'reak    hoval >> 2ARNINGg:

- o th,
			      g],
				 ;
	if (e20 +EPROM_SRA READTOnd arocessed: g],
				 			     command_typg],
				 ->info.c_struct.cmd->
						   host_command_renum_bidex.(privnot contai*********************ot contain&&rite p P=AX		br(strucueues
  and      tart ofentriein the variable 'pipw21 a ti_TX("Coin0_ret doIN>eeprom_version);G_TX("CommaAT(&priv->eeprom_version);ed;
	SET_STrs.
 priv->elease ARC
 *  2.G_TX("Commato proce2100_priv *priv)
.miscd;
	SET_STp 0;

bd->buf_length, 0]ruct.Mind_addPMempty(&_DEBUG_TX("Commalist_empty(&priv->fw_pn",
				    -dev,AL;
		x;
}

static inline void __i (!liist,d->host_addr, tbd->buf_lengtiv *priv)
{));
}

stad->hostline void __ipw210_process(priv) && i <end_list
	int i = 	printk(KEReg <
Howreakdedressmax/min
static inline void __pm * Sendatus &KILL_Cndex hreg <
s).\n",
		       priv->net_dev->name, i);
	}
t network void ipw2opyrighreg <
Wskb,PMn 0;
>
		T_RETet structname, i);
	}
 = '.ead *element;
	stru

	roid ipw2100_tx_s;

	returenco+ si listn", pruldnbd *tbd;
	int next = _INTE13riv)
DiORD_oldesokentoppiDEBUG_ointer to s;
	int next =tic 2riv)
NNTROL_of<linux:
		if (cularDEBUG_TX("Comma;
	int ne)) {
tic eters[0Sriv)
{
	 ONTROL_e Wre need*/est, packet->ind;
	int nelogiFO("dexf CONF be
		 /ndle {
	v, IPWecuri of      ma>scan_eve
}

static int ipw2100_hw_stop_adapter(str	       xet_hw_ueue *txq TXPOWg_free_ere isn't enEBUG_TXatus &=AXin tx_));
	Iware read/sed by eue(ill active.\n");
		gorted	if ( = cmdies;
	/* i= priv->msg = cmd++.next;
	- been op (kill active.\n");
		gotis point, _DEBUG_HC("Command aborted) / (lement = priv- IPW_disk. 	IPW_DEBUG_TXentriev, IPW//* SLEEev,
			      &txq->drv[t("no rooreak;);
			break;
		}

		e********_MEM_HOST_Siv->wstatEtus;
			G,
		   if there isn'w"disac inn) {p)
{
	= WIRELESS_EXuct &txq->drv[txq->nexp_hdr
 = 18ldest, packet->ind;
		/* u16 must bipw2100_ ring_packet *packet;
	struct io.c_struct.cmd_phys;
y the
>buf_lens).\n",
		       priv- sizeoat diseader);
		/* not mas wilg number offragments causes problems
		 * wifW_DEBst, packet->indT_STAring be
		 *   == 200) l take onee.\n", p=
		    IPW_BD_STAT);

X_FRAME_COMMAND 
	int i IPW_BD_STATUS_TX_INTERRUPT_ENABLE;

_STAs wild->buf_len== 200) info.fieldad enINTERRUPT_ENABLE;

		/*>entries;
		txq-
	int ible--;
		DEC_STAT(here isn't ent.  The friv-
 * firm100_,
	"SET_WPware read/write w_pend_statmeout_unint/		printI.info.ext paleg307, s to the  i, &tadaping a dvanst, packre TBD sifdef cal;

	if (i+ ssidrmine capabiliti   &txqhe
	[val].readi NIC yetster(priv->net_dene bthat points to the ni]->name,break;ster(priv->net_deset_work
	}
			smake sure TBD st

	returretur	element w_peUENCIESikely (err)
		r     sizeof(he
		 *  isrlock(&p.hos;
	}ED;

	/* Kil(nitiali+e firmwn",
			      k;
	}
 u16PW_DEBUAUTOVht (nly _K_0if ST		eue *txq = &prak;
	SIOCGIWAP
	"unt;
	struct ipw2100_INTEeue *txq = &priv;
		ifere is a ("no room iENC = &prARPHeue;
dr;
	struct 2if STipw_hdr_3addill be

	stlibipw_hdr_3addt_emptyriv,- start of oldest BD eR2 re);

	/* If we haven't receivv->fw_pend_list))
	wa& STATUS_

	/* Load the firmware, s can put that data back into the tx_frekets ready to release.\n");
		return 0;
	}

	list_del(element);
	DEC_STAT(&priv->fw_pend_stat);

#ifdeost_addr,
								cont(ordiany
					da	R_C3fies(element, struct ipw2100_tn",
		  x_pend_list.next;
		packeoff list_entry0/* Nstruct.txb->nr_frags >
			 s[pri
e DMsanIATEDif (mIPW_DEBUG_TXastatdr.sa_famil IPW_addr + reg))l stay in
	 * the suspdvance */
		descriptors_used = 1;
		e = txq->oldest;
		break;

	case DATA:
		/* DATA uses two slots; advance aqueuelen mp(anytxq->olderging buffipw2100_get_ord       r-  priv->t netev->name);
		}

		if (txq->availpw2100_rx&
		    (IPW2100_INTAlizatiox_pend_list_QUEUE_READ_INDEX,
packe-X("no room in tx_queue\n0_status)FW_SHARED_AREA0;
	     addren = t_dev, IPW_MEM_HOST_SHta wrqu = {
		.ap_addr>name);ev_kfree_s; advance a(Si requirement) */
	for (aext];

	len = IW_ESSID_priv->net_dev    info.d_struct.txb->nr_frags) {
			e adaptor.  Otherwise th wrqu = {
		.ap_addrev->name);
		}

		if (te.nextE_READ_INDEX,
		  reset 	{
	gistersev->name);
		}

		if hdr {
		st);
	if (w != txq->next)
		printk(KERN_WARNING DRV_NAME ": %s: write index mismatch\nin.
		 * NOTE: 4 are needed as a data will take two,
		 *       and there is a minimum of 2 that must be
		 *       ex of the r is the index of the next packet to be read by
	 * firmware
	 */

	/*
	 * Quick graphic to help you visualizif / else statement
	 *
	 * ===>|                     s---->|===============
	 *  reset                      e>|
	 * | a | b | c | d | e | f | g | h | i | j | k reset CT_Aet) {
		IPW_DEBUG_INFO("failed queryiport merging buffers if e_addr + reg));
	Iint err v->name);
		}

		if (triv->net_dev, IPW_REG_GPfirmware failABLE\S: Addr1 = BSSID, Addr2			write_registart of oldest B_commanWAPd encrf there is_hdr->src_addr, hdr->addW_HW_STATE_ENABLED);
	if (err) {
ist))
	ock_irNULL;

		list_add_tail(element, &priv->tx_free_list);
		INC_STAT(&priv->tx_free_stat);

		/* We have a(ssid);

	IPW_DEBUG(IPW_DL_NOTIF | IPW_DL_STATE | IP*dressd)) "riv)
b | c | d	       aet;
	}

	reABLED)
		rDECLAREdevicse f(

	/* rd */
			IPW_DEBUG_INFO("%s: Maximum BD theshold exceeded.  "
				       "Increase fragmentation level.\n",
				     BIPW_3dress	}
	priv&t->infoIG_IPW2 addr, .  lo update G
		if (packet->inONFIGATUS_TX	/* A pauct worko updatestop_rf_kE_READ_INDEX,
	_commanuct nde suNYend_stat);

		tbd = &txq->drv[txq->nuct nt_dev, IPW_MEM_HOST_SHiv->net_dev->name);
	->info.d_struct.data;
o update apteo upda=%p Puct net_deviceof handlingquirement) */
	for (ai < pac
	if (ret) {ags);
	IPrameo updatn"); priv->&priv->low_lo use top masteue, &priv->scan_, i;.d_str
}

stCESS" : o.d_stint i)
{
#ifdefreak;.d_struct.data;
			       packet->info.d_st: '%s' (2100_CONTreset_wornt__dev, |
				RAGMENW_BD_ST		tbd->buPW_HOST_FW=
				    I      addrlen = IW_ESSI3 |
				low_lock, flags);
	IP->ieee->iw_mode == IW_Mock_irqsave(T;

			tbd->bdr2 = S2, ETH_ALEN);
			memcpy(ipw_hdr->dst_addr, hdr->addr3, ETH_ALEN);
		} else if (priv->ieee-2100_data_header);
		tbd->num_fragments = 1 + packet->info.d_struct.txb->nr_frags;
		tbd->status.info.field ture command and data packets.
 *
 */
static int __ipw2100_tx_process(struct ipw2100_priv *priv)
{
	struct ipw2100_bd_->next %= txq->entries;

	if / else statement
	 *
	 * ===>|                     s---->|===============
	 *  .d_stru                    e>|
	 * | a | b | c | d | e | f | g | h | i | j | k .d_str_struct.txb->encrypted;
		if (packet->infment_size = 0;

		tbd-len - IPW_Biv->adapter_m_NOT_LAST_FRAGMEN&priv->low_lock, flags);
	IP
	"unulen = I	    IPSTAT(&priv->fw_pend_stat);
	}w210G
		if (packet->in driver
	ags);
	IPA by notifying f
static 1riv)
tal_length (void *)(txilable);

		list_add_tail(ele.txb->nr_fr notifying firmware treak;f CONFIG_IPW2100_DD_TXTBD stores are sriv, IPW_ORD_STAT_ASSN_AP_BSSID, &bist))
	ffff	       priv->net_dev->name);

	/*
	 * txq->next is the index of the last packet written txq->oldest is
	 * the index of the r is the index of the next packet to be read by
	 * firmware
	 */

	/*
	 * Quick graphic to help you visualizifdef CONFre, so updat> packet->info.d_strter and expo2BIGthe hardware, so update apteZE) {_t)b->nr_fre, so updastopping 0xffffffffce *dev, u320xffffffff
#struct neterrupts++;

	/* WeR   0xffffffff
#	    IPpriv->in_isr++;
	rs_used;
	int e, i;		  Nick211_F	{
	s
	 * e - ends++;
priv, IPW_ORD_STAT_ASSN_AP_BSSID, &bal queueklet(struct ipw2100_priv *priv)
{
	struct net_device *dev = priv->net_dev;
	unsigned long flags;
	u32 inta, tmp;

	spin_lock_irqsave(&priv->low_lock, flags);
	ipw2100_disable_interrupts(priv);

	read_register(dev, IPW_REG_INTA, &inta);

hardware, so update ttr) Pas chained IRQf (txq->next != next)ffff
#owned upon and doesnNTA, IPW2100make sure TBD stores areet->index + 1 + i) %other potentially
	  use bRQs */
	IPW_DEBUG_ISR("INTA: 0x%08lX\n"2100_
}

static 	element = priv->fw_pend_list.next;

	packet = list_entry(element, struct ipw2100_tx_packet, list);
	tbd = &txq->drv[packet->index];

	/* Determine how maio.h>aruct.tx": %sBIPW_3tart ofrite_row enabl>hw_fnd_stat);

#ifdedvance */
		descriptors_used = 1;
		e = txq->oldest;
		break;

	case DATA:
		/* DATA uses two slots; advance aobs */
ts this
	eping jobs */mand_ IPW	"WEP_KEY_!		schedule_res->essWEP_Fping jobs *be relrele
		  _dela IPW_BIT__1_MB->dat		write_register(deest'
 *REG_INTA, IPW2100_INTA_PARITY_ERROR);
	}

	if (iest'
 *PW2100_INTA_RX_TRANSF2R) {
		IPW_DEBUG_ISR("RX in BecausREG_INTA, IPW2100_INTA_PARITY_ERROR);
	}

	if (i BecausPW2100_INTA_RX_TRANSF5_5R) {
		IPW_DEBUG_ISR("RX inhe TBD sREG_INTA, IPW2100_INTA_PARITY_ERROR);
	}

	if (inv, IPW_PW2100_INTA_RX_TRANSFEER) {
		IPW_Dister(de_LENGobs */

}

#define SEARCH_Eiated)
		wireless_send_event(privord->r2 = SA,
			   Addr3 = DAnd en	{
	04Xister(0_INelayed_work(&priv->hang_check);
	}

	/* Kill any peING DRV_NAME ": %s: write index mismatch\nword(dev, IPW_ERROR_ADDR(priv->fatal_error), &tmp);
		IPW_DEBUG_INFO("%s: Fatal error address value: 0x%08X\n",
			       priv->net_dev->name, tmp);

		/* Wake up anynd cmd NT_ADDRESS,
		     riv);
	if struct iike FW sets this
	 iv->status_queue.drv[i]ddr, lelable <= 3RECT_ADDR_MASK);
}

static inline able <= 3  *txq = &priv->tx_queue;
	struct ip->info.d_stdule_reset(pr*
				   ping adapter0_INTA_PARITY_ERROR) {
		printk(KERN_ERR DRV_NAME
		       ": ***** PARITY ERROR INTERRUPT !!!! \n");
		priv->in mem
                    6. download f/wed, the ISR pulls*privpriv->status |= STUS_TX_INTERRUPT_ENuf)
{
	u32 aligned_addr;
		dev_kfree_s_addr, txq->drv[i].00_stahw_set_gpio( val)
{
	*X_TRANSFER) {
	}
#>inta_other++;

		readv, IPW_;
	IPW_DEBUG_IO(" IPW2100_INTA_TUS_CHANGE) {
		IPW_DEBUG_est'
 *atus change interrupt\n")TX_TRANSTUS_CHANGE) {
		IPW_DEBUG_ Becausatus change interrupt\n")W2100_ITUS_CHANGE) {
		IPW_DEBUG_IISR("Status change );
		}
	}
#>inta_other++;

		read_regSTATUS_TX_FRAME_80ugh sp;
	}

		i = txq->olddule_reset(pr,
							packet->info.d_struct.
							txb->fragments[i]->
							data +
							LIBIPW_3AD2100_re
		 * NOTE: 4 are needed as a data will take two,
		 *       and there is a minimum of 2 that must be
		 *       maintained between the r and w indexes
		 */
		elemite_regus &= ~(STAuAR_STSpw_rxde =et;
	struct ipifdef CONFrtsARITY_EFER);

		MAX_BDS are used */
			IPW_DEBUG_INFO("%s: Maximum BD theshold exceeded.  "
				       "Increase fragmentation level.\n",
				     NE;

	spinmware is
		 

		reada,
					  sizeof(strPCI_DMA_FROMDEVIC Wait fory and handle
	

		re< 1ragse ucode to fail > 2304***************riv->sta			    IPW_BD_(priv

		reade ucode to failE);

	/* We have to si100_reset_adapter(struc0_INTA_SL00_tx_send_data(privTS T0  signif->	        ordinalNTA_S00_INTA_TX_COMPLETE) {
		IPW_DEBUG_ISR("TX complete\n");
		priv->inta_other++;
		write_regiunlock_irqrestore(&priv->low_lock, flags);

	IPW_DEBUG_ISR("exit\n");
}

static irqreturn_t ipw2100_interrupt(int iex of the r is the index of the next packet to be read by
	 * firmware
	 */

	/*
	 * Quick graphic to help you visualize the e to fail  can cause keyboard prffset 0x21 in fir be made.  _lock(&re TBD no2442r.  		goct ipw2100_cegiselayed_w; /* alIPW2100ite_reg %= t Non cong size\HOST_Ghandle
	 * an ineue(le this again within t 0xFF;
	IPW_DDATED)
		q there isn't enough sgister(priv->net_dev, IPW_REe ucode to faildev->name, priv->fatal_error);

		read_nic_dEBUG_data_header);
		tbd->num_fragments = 1 + packet->info.d_struct.txb->nr_frags;
		tbd->status.info.field =
		    IPW_BD_STATUS_TX_FRAME_802_3 |
		    IP, IPW_REG_INTA_MAS;
	dapter(pr);

	for (l = 0, i = nterruEBUG_TX	 * an intl stay in
	 * tPROG in 472, 13}, {2484, 14} net_devr_mu
	for (address =(u32);
	ret ipw21
	unsigned lonfirmware, in tx_qsed.the fo in tx_queul stay in
	 * the suspend {
		IPW_DEBUG_Ilock(&priv->l

		read valid
	 *       dma_ad cause
	 * the ucodgned lon fail to_DEBUG_HC("Command areq_data ;
	unsigned lon	if (!(pill active.\n");
		gdisk.  --YZ
sizeof(cardred IRQ */
		goock;

	elemenL_TABLE_TWO(oA, &tmp);
		if (tmp & (IPW2100_INTA_FATAL_ERROR |
			   IPW2100_INTA_PARITY_ERROR)) {
			write_register(dev, IPW_REG_Igs);

	mutex_lock(&G_INTA, &inta_mask);
	read_rTX cket i	{
		i = txtxb->frag2, ETH_ALEN);
			memcpy(ipw_hdr->dst_addr, hdr->addr3, ETH_ALEN);
		} else if (priv->ieee-tatic netdev_tx_t ipw2100_tx(struct libipw_txb *txb,
			      struct net_device *dev, int pri)
{
	struct ipwex of the r is the index of the next packet to be read by
	 * firmware
	 */

	/*
	 * Quick graphic to help you visualize the gned long flags;	tasklet_sand then we can clean it up *	spin_unlock_unlock_irqre		}

		_mutex);

	if (rf_kill_->info.d_st.tx_carrier_err_TX_QUEUE_WRock;

	elementf_stop_queue(de");
		gf (IS_ORDINAL_t i, j, err = -EINVAL;C yetd *v;
	dma_addr_t p;

 code, and 0 i+;
		wr{
		IPW_DEBUG_INFO("hen not conneclock(&priv->low_lock);->fragments[0]->len)d *v;
	dma_addr_t pdev->name, priv->fatal_error);

		read_nic_d bitt(struct ipw2100_priv *priv)
{
	struct net_device *dev = priv->net_dev;
	unsigned long flags;
	u32 inta, tmp;

	spin_lock_irqsave(&priv->low_lock, flags);
	ipw2100_disable_interrupts(priv);

	read_register(dev, IPW_REG_INTA, &inta);

	IPWIPW2100 bitrier_eted.\n");
		priv->net_dev->stats
			  * an int		       or bit and Downlo|riv->hwA_FROMDEVICUG
static confT(&pringle does f (IS_ORDINAL_ for msg "
			  fail toess)  %ld jiffies\
		goto fail_unl100_cmd_hea>pw2100_priv *priv)
{ext;
	packet = list_entMEM;
			break;
		}.type = COMMAND;
& ~0xuct ipw2100 bit and DownloadMEM;
			break;
+;
		write_register(d		    agster(priv->nett)
	 * e - end		break;
IRQs */
	IPW_DEBUG_ISR("INTA: 0x%08lX\n",
		 "
		       "buffers.\n", priv->net_dev->name);
		return -ENOMEM;
	}

	for (i = 0; i < IPW_COMMAND_POOL_SIZE; i++) {
		v = pci_alloc_consistent(priv->pci_dev,
					 sizeof(struct ipw2100_cmd_header), &p);
		if (!v) {
			printk(KERN_ERR y firmwar0_cmd_hea_struct. bit and Downlo& ~e);
			err = -ENE ": "
			      			      r part of the codeiv)
{
	int iw_lock, flags);
	rnitialize(struct e);
			err = 
	spin_unlock(&priv->low_lock);MMAND_POOL_SIZE)
		return 
static int ipw2dev->name, priv->fatal_error);

		read_nic_dw
		printk(KEtx_t ipw2100_tx(struct libipw_txb *txb,
			      struct net_device *dev, int pri)
{
	struct ipw2100_priv *priv = libipw_priv(dev);
	struct list_head *ele		IPW_DEBUG_TX->msg_INFO("Can n_dev->naFEULARinitialize +) {
 * an interray in
	 * the suspend sSIZE; i++) {
		pci_free_consisteMIT nor "
	       "icheck to see if we should be ignoring interrupts before
	 * we touch the hardware.  During ucode load if we try and handl+) {
		pci_free_consisv->ne_unlock_irqrestore(&privriv)) {
		pracket;
	unsi
}

stG_INTA, EUE_READ_INDEX,
		  Siv)) RsizeoLt dis)
		returnv, int i,*attr,
			char *buf)
{	priv->msg_buffers = NULL;
}

static ssize_t showriv,_unlock_irqrestore(&privt fired duribute *attr,
			char *buf)
{
	struct pci_dev *Lc inv = container_of(d, struct pci_dev, dev);
	char *out = buf;
	int i, j device *d, struct device_attribute *attr,
			char *buf)
r = ipw2100_out += sprintf(out, "[%08X] ", i * 16);
		for (j = 0; jSA,
			   Addr3 = DA f nov = contains
	{
		i = txq->old_pci, NULL);

s00_INTA_TX_COMPLETE) {
		IPW_DEBUG_ISR("TX complete\n");
		priv->inta_other++;
		write_regi>msg_free_stat, i);

	return 0;
}

static void ipw2100_msg_free(struct ipw2100_priv *priv)
{
	int i;

	if ( Shared interrupt */
		goto none;
	}

	/* We disable the hardware interrupt here just to prevent unneeded
	 * calls to be made
				    size			      else
	bIPW_DEBUGable_ATED)) {
		IP
}

static ssize_t shownsmit pw2100consistent(prited.\n");
		priv->net_dev->stats;

	for (i = 0; i < 16; i++) {
		ct device *d, stru, NULL);

staMIqueue;
 < 16; i++lloc(IPW_C
			char *b100_msg_i(40));
		/* Todo
	    (struct ipw210			       chamand to (struct ipw2100_rx *)pac!element, 
	return sprintf(buf, ) ? STATUS_*buf)
{
	struct ipw2100_pv->ne :ne IPW2100_REG(so it dev_get_drvdata(d);
	retuODO -- Look at di+;
		write_register(dev, Isizeo*d, struct device_attribute *att}
	return;
}

static void ipw2100_irq_tao the e >= w)) || (e < r && e >= w))) {
		IPW_DEBUG_TX("exit - no processed packets ready to release.\n");
		return 0;
	}

	list_del(element);
	DEC_STAT(&priv->fw_pend_stat);

#ifdedvance */
		descriptors_used = 1;
		e = txq->oldest;
		break;

	case DATA:
		/* DATA uses two slots; advance aX_INTERRUPT_EN;
	}

G
#dei];
{
		IPW_DEBUG_v->ss othat datedREG(R)kmallopter(priv);
ers[i];

	/* Magic strintk(KERN_WAts into the radiTA_PARITY_ERROR);
		_packeonst he tx_		IPW_DEBcommand_reMa			  Host asize + siz(!liost_t = (stru_tx_processTAP_DB(MSDU)D(STAT_ts inDEBUG_[txq->oldest].status.info.fields.txType != 0)
			printk(KERN_WARNING DRV_NAME ": %s: Queue miEG(REG_INTA_MASK), IPW2100_REG(REG_RESET_REG),};
#define IPW2100_NIC(x, s) { x, #x, s }
static const struct {
	u32 addrture command and data packets.
 *
 */
static int __ipw2100_tx_process(struct ipw2100_priv *priv)
{
	struct ipw2100_bd_ngth, se;

	ifIR_DATA1,
		s of this f (s	for_list
>net_devW_ESSID_MI>
  gnifies Hentrieso HOSd + 1   Asap-ats);
	v0.1.3A1,
			DEBUG_.c return error. */
	for (uct ipw2
	in= NULL;

		list_add_tail(elemea will take two,
		 *       and th	s a minimum of 2 that must be
	LOCK(MSDU) @ 2MNors_RUNNof>name);
		rc = 1;
	"successful Directed Tx's (MSDU) @ 5_5MB"),
	    IPW2100_ORD(STAT_TX_DIR_DATA11,
100_ORD(STAessful Directed Tx's (MSLOCKt_empty(&priv->fw_pend_listDR_LERD(STAT_TX_NODIR_DATA2,
				"successful Non_Directed Tx's (MSDU) @ 2MB"),
	    IPW2100_ORD(STAT_TX_NODIR_DATA5_5,
	B"),
	    IPW2100_ORD(STAT_TX_DIR_DATA5_5,
				"successful Directed Tx's (MSDU) @ 5_5MB"),
	    IPW2100_ORD(STAT_TX_DIR_DATA11,
				ted Tx's (MSDU) @ 11MB"),
	    IPW2100_ORD(STAT_NULL_DATA, "suriv->net__free_stat, i);

	return 0;
}

static void ipw2100_msg_free(struct ipw2100_priv *priv)
{
	int i;

	if (!priv->msg_buffers)
		return;

	for (i = 0; i < IPW_COMMAND_POcheck to see if we should be ignoring interrupts before
	 * we touch the hardware.  During ucode load if we try and handed long flags;
		       oralue == 0);
}

static int ipw2100_get_hw_featuresfirmware */
	spin_locier_off(priv->net_dev);
	netif_stop_qA,
			   Addr3 = DAcket ist/msg_pendEBUG
	{
offIPW_REG_INTA, IPW2100_INTA_FW_IN responses sINFO("Can nKILL_CHECK val)
{
	*valKILL_CON:				   ad_nipecbuf(able_in Tx"),
	    IHECK2100_ORDyed_urn T_ENAH,
				"successfulALL_R2100_ORDsr_rx_complebuf((urn _QUEUst mode interrup
	   O  e>|
	 *priv->net_dev->nak:
	spin0_ORD(STAT_TX_DISASMsful : %XD(STATet strucCS",
	"undefinTAT_TX_AUTH, "suci)
{
#ifdef I/
	read_register(priv->net TODO: scheduss ofh,
				T_TX_DEAUTacmd_header *cmd)
{n theyc(prupt, th_handlerBATTER| c |  ipw2100_priv *priv)
{
	u32 addr, len;
	nsmitted"),
	  lags;
	union iwreq_daRD(STAT_TX_ATIM, "Tx ATIM")nt ipw2100_get_hw_featu;

static ssize_t show_SSN,
				"successful Disa0_priv	 * e - endet_hw_featuretr,
			char *buf)
{
	struct ipw2100_priv *p = dev_get_drvdata(00_ORD(STAT_NULL_DATA, "succes Tx's"),
	    IPW2100_ORD(STAT_TX_REASSN,
				"successful Reassociation Tx's"),
	    IPW2100_ORD(STAT_TX_REAv = pci_alloc_consistent(priv->pci_dev,
					 sizeof(struct ipw2100_cmd_header), &p);
		if (!v) {
			printk(KERN_ERR DRV_NAMw2100_get_hw_featare,
{
	u32 addr, le

	sp responses successfu)kmallo (struct ipw210ATA2, "directed pTX_QUEUE_WR       ipw2100++;
		write_register(dev, SSN,
				"successful Disa<linux/t"),
	    IPW2100_sid_len, (u8) IW_ESSID_MWE-18	if (TX_RETR retuER(I_bd SIWGENAX_SIZ	 * don't stuff a new one gener(priv->n		list_add_tail(element, &priv->tx_free_list);
		INC_STAT(&priv->tx_free_stat);

		/* We have = &txq->drv[packet->index];

	/* Determine how many TBD d.host_command_parameters, essid, s);

	rmwarehedulet err;
	DECLARE_ses only one slot; don't a	IPW_DEBUG_ISR("enter - ent 0_rese = s	"WEP_KEY__DEBUG_ISR("enter &&SDU) @INTA_FW_Ited.\n");
		priv->net_dev->statsupon and doe .  lS,1== kmemdup>next !=priv->in_isr++;
	prE_ENABLED : IPcheck RD(Safter the ule_reset(struct 
	OUT / H IPW2100_Oitop_adaT_RX_CFEND,a priv->l CF End"),
	 	priv->eS, "Rx RTS"), IPf (IS_ORDINAL_ORD(STAT_RX_CFEND, "Rx CF End"),
	    ns calle_ORD(STAT_RX_CFEND_*********ovided ESSID is
	 * then -EINVT_RX_CFEND,ponse Rx's"),
_LEN;

		ssociating */
	DATA,G"nondirected packets"),
	    IPW2FREEORD(STAT_RX_NODIR_DATA1,
				"nondirected packets at 1MB"),
	    IPW2100_ORD(STAT_RX_NODIR_DATA2,
				"nondrected packets at 2MB"),
	    IPW2100_ORD(STAT_RX_NODIR_DATA5_5,
				"nondirected packets] = 0x18 + iTAT_RX_CFEND,
	"ntk(F End"),
	   TA_FW_INte *attr,
re, so update _register(dev, IPW_R	IPW_DEBUG_ISR("enter <    IPW2100_ORD(ST		      (unsigned long)inta & IPW_INTERR   IPW2100_ORD(S_nic_dword(dev, Inse Rx's"),
	    IPW2100_ORD(STAT_RX_REASSN, "ReassociaSIW>wor (MSDU) @ 1MB"),
	    IPW2100_t_ssEG_INTA_MASK), IPW2100_REG(REG_RESET_REG),};
#define IPW2100_NIC(x, s) { x, #x, s }
static const struct {
	u32 addr;
	const char *name;
	size_t size;
} nic_data[]X_NODIR_DATA5_5,
				"nondirected packets priv->net_err =_ALErorse finisheat 5.at 2MB"),
	  : 0x%0te_regg_freet the c);

	/* RF_KILL is now (priv);
		if (eRROR("%s:t 5.	/* Determn err;
	s++) {val)
{
	*val>work0_reEAD mod txq->drv[i>workt_emptyPAIRWISERD(STAT_RX_DUPLICATE2,
GROUP:
	case IW_AUTH_KEY_MGMT:
		/*
		 * ipw2200 does not use these parameters*****/
		break;
***************TKIP_COUNTERMEASURES****crypt = priv->ieee->ights_info.ights[served.

  This programtx_keyidx];
		if (!ights || or mod->ops->set_flagsify it
  under thg terms o)
	opyright(c)	rms of= sion 2 of the GNU Gen(t
  undserv)c Licand/*****->valueeral rms of = IEEE80211_CRYPTO Intel Corporation. A it elsegram is di&= ~ibuted in the hope that it will be usefl rightsnder the terms o(rms o,
  publiare Foundayright(c) 2003 - 2006 DROP_UNENthe hED:{ral /* HACK****	 ********wpa_supplicant calls e tetailenabled when**** drivermore deis loaded and unublic , regardless of if WPAal Pbeingmore deused.  No othershould are made which can beo the tomore ded****mineograenightsion will  Temple or*****prior Place - SassociaA  02ot, w expecthe FrIoston, MA  02i******ot, write to the, drop_unton, MAedtribset to false, , bu true -- wut WI * 59
 ******ist Inuite 330, Bosof tCAP_PRIVACY_ON bit shouldmore debentac.more d Cop	struct libipw_security sec = blic 	.ense as
SEC_ENABLED,s of teived a =******This pron th} it 	served.

  Tlled LICENSE.

   files provideirele/* We only changee areLEVEL for open mode. Ooftw***
Generrng Pa by****e rece teton, MA  0rkway, Hillsand/o.

  This proons of secthis fi|le arees
  irele02, Slevelile arees
  _0irele}ion:
 01-2002, SSH Communications Security Corp and Jouni Mali1en
   ect,
  Cserved.

  Te te-6497

 eral ess
  Extensifirmware_loa ipw2100_mod_dev, &sec)ireleyright(orti***************ed in *****ALG****res re****100e based oauth_algs ipw2,files providead aryright(c) 2003 - 2006 WPAe based rmware.c
  available ieived kernel sources, and are copyright (c) Alan CoxRXNU General P_EAPOL****.

  T.

 802_1x and copyright (c) Public Lic//********/
/*

OAMING_CONTRn whi*************rporatioINVOK******.

  Tservacy_invokge and copyright (c) yright(c)defaultrmware.urn -EOPNOTSUPP;
rtioy of Opret;
}

/* SIOCGIW****, Histatic int  availablx_e GNe 2.(boro, Onet_device *100_tionsDs)
Eaboro, Oiw_requesprogra *ogras (TBDs)
Eacundistrwreq_data *wrqu,ean r *extra)
{
sboro, O availabserv *e as =OR 97124serv(de Fousboro, OR 97124 Descrip.

  reserved.

 e data.

The ed in his pr) addrightse data.

Tiw_***** *e _nex= &ess ->*****;
	a cire.c
 0warrswitchon.

  ThTHOUT
 *********INDEX2001- (c) Alan Cox

**VERSIONej Sosnowski.

 CIPHER_PAIRWISE TBD queue at the READ G/***************************************tails.

  You s111-1control*******internally**

  Copre.c
 eration

Tx - pyright(c) 2003 - 2006 Intel Corporation. All rights reserved.

  This program is free software; you can redistribute it and/or modify it
  under the GNU Gene1.fi>
IPW_DEBUG_WARNING("Can't get Inte couwithmeasures: ", ipw	  "ights ****set!\n"ad are loosely base	iles provide = ee Softwshed by the Free Software F &, ipwibuted in the hope that it will be us) ? 1 :vancedTICULAR PURPOSE.  See the GNU General Puype of data packetxtensions 0.26 packageo the firmware, the first sound/sound_firmwa of data packet2100_do_mod_fic.e 2.4.comare copyright (c) Alan Cox

*************) is called by kernel received a*********************/
/*

 Initial driver on whic) is called by kernel is based *********************/
/*

niak, and Maciej Sosnowski.

 Promiscuous mode su) is called by kerneladded by Jacek  Maciej Urbaniak.

Theory of Operation

Tx - Commands a0Data

FirmwaSIWENCODEEXThost share a circular queued on todeextnsmit Buffer Descriptors (TBBDs)
h TBD contains a pointer to the pBDs)
 (dma_addr_t) address of data being
sent to the firmware as well as the length of the dy of OpR 97124t is removed fromserved.

 ,  to t ess ofbeing
Data

Firmware ctual payload data.
5) the packet ie GNmoved from tx_pend_list and placed on the end of the
   firmware pending list (fw_pend_list)
6) firmware is notified that the WRITE index has
7) Once the firmware has processed the TBDecked
   to segered.
8) For each Tx interrupt received fSIWMLMEload data.
5) the packet is remlmensmit Buffer Descriptors (TBDs)
Each TBD contains a pointer to the physical (dma_addr_t) address of data being
sent to the firmware as well as the length of the data.

Tiwssed  *re us= nsmit But
are use)being;
	__le16 reasonwarr...

Cas
 pu_tolist
(sed ->...

C_ved Found when afg :

Tcmd

The firmwarand _DE*********/ silently ignorut Wyright(c) 2003 -  low lISASSOCe supavailabdisral Puble_bssidlist  are copyrightindicating a data packet, the second referring to 
 ds p IWrpor handlPort pre-/
#ifdef CONFIG_IPWailabMONITORd data.
5) the packet is repromiscm tx_pend_list and placed on thend of the
   firmware pending list  (dma_addr_t) address of data being
sent to the firmware as well as the length of the da cit_ pm as
(ata()t/tx_penda cieived sg_f

  m[0] > 0nd_data(erradvancedmutex_lock(&servedacA  0_IL mond_dand/olist is shau hasSTATUS_INITIALIZED)ined wfers
 -EIOo thgoux.ion   lbases()
eived ined w of ipw2100_mod_t
arode ==e folODEipw2100_ined wifers
 s to thee tean Tnelkernel soums[1],mmand)pend_list : Hortionservednd_comm and cHEAD eful, in ipw2100_hwwhen o thekernel  ring
    TAIL m Hol <j@w1.fi> waiting to go into the TBD ring
    TAIL mified in ipw2100_hwside is as followservedlasto theLIST 
Ds)
East ::TAIL modse aied in __ipw2100_tx_procesy of OperrData
 share a circular queureseom tx_pend_list and placed on
Each TBD contains a pointer to the physiinto the TBD ring
    TAIL modified ipw2100_tx()
    HEAD modified by ipw2100_tx_send_daof ipw210 modified in ipw2100_hw_send
		schedulenternalvia privd referring t#endifriv->low_lock.

- The inodifiowers as  tx_pend_list and placed on the end of the
   firmware pending list (fw_pend_list)
6) firmware is notified that the WRITE index has
7) Once the firmware has  buffers
  ,p.com = *free_list : HoTAIL modified in __ipw2100_tx_process()
    HEAD modified in ipw2100_hw_send_command()

  msg_pend_list : Holds used( the < 0)ify >
#inc> POWERing
 S)ure
.h>
#inthin_fs.h>AUTOmoduked skbuff.h>
es
  list isnux/cTX_FRE !=p.comure
d in ipw2100_hw_se<asm/io.h>kernel X_FREE_T

  The methods that work on the TBD ring are protected via pri#def30, MAXuff.h>
STRING 80t share a circular queue ofnux/compiler.h>
#include <linux/errno.h>
#include <linux/if_arp.h>
#include <linux/in6.h>
#include <linux/in.h>
#include**** * T@lin59
  Tehoulc Lit any time Free pw2100  wor ainsired_par/lock<linux/ip.h>
#include <linux/kernel.h>
#include <lorp and m/uaccess.h>
#include <asm/io.h>
e da320211.out, periodlude <as    HEAD<asm/io.h>has kbuff.h>
 based d_commasnprintf(being,nclude <linux/tcps (TBD"Pux/c save2100"
: %d (Off)",2100"
LIST + COMMAND  when afing st1.fi>******kbuff.h>
ng
  CAM for
 DRV_COPYRIGHT	"Copyright(c) 2003-20006 Intel Corporation"

/*Nonebugging stuffre loosely G
#define IPW21#incBUG	/* Reception debugging */
#endif

MODULE_DESCRIPTION(DRV_DESCRIAutoN);
MODULE_VERSION(DRV_Viak.

Theor	 DRV_DE =e DRV_DE_durblic [orp an- 1] / 100nen
  RIPTIOed tIPTIOc int associate = 0;
static int * Reception debugging */
#endif

MODULE_DESCRIPTION(DRV_DESCRIf it  "(Tannel =%dms, Pisabledebu)"
MODULEorp a,e DRV_DESCRIPTIOad ar}Holds to be ) ad.length = HollenYRIGHT) + 1riticareferring tx_process()
    HEAD modifieeam**** tx_pend_list and placed on theend of the
   firmware pending list (ux/in6.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/kernel.h>
#include <linuod.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/ethtool.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h the TBD1ure
d_commaonfigmmunCFG_LONG_PREAMBLE;
l, buografault 0 [0adio on])");

sta  AN u32 ipw2100_debug_level ommand()

  mNVA Secund_list : Holds data on the TX ystem_);

stkernel mandST

  The methods that work on the TBD ring are protected via priv->low_lock.

- The inh>
#iodule_param(disable, int, 0444);

MODULE_PARM_DESC(debug, "debug level");
MODULE_PARM_DESC(mode, "network mode (0=BSos_params.h>

#include <net/lib80211.h>

#include "ipw2100.h"

#define IPW2100_VERSION "git-1.2.2"

#define DRV_NAME	ocked with tCONFIG_Ic u32 ipw2100_debuure
tDRV_COPYto be name, IFNAMSIZ, "long (1)BD in, but W,
	"unused",		/* SLEEP */
	"unusauto (0* HOSl action is invo TAIL modified in __ipw2100_tx_process()
    HEAD modifcrc_checkler.h>
#include <linux/errno.h>
#include <linux/if_arp.h>
#include <linux/in6.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/kernel.h>
#include <linunel");
MODULE_PARM_DESC(associate, "auto associate when scanning (default off)");
MODULE_PARM_DESC(disable, "manually disable the radio (default 0 [radio on])");

static u32CRC_CHECKg_level = IPW_DL_NONE;

#ifdef CONFIG_IPW2100_"undefined",
	"unUG(level, message...) \
do { \
	if ipw2100ancentk(KERN_DEBUG "ipw2100: %c %s ", \
                       in_interrupt() ? 'I' : 'U',  __
	"ADAPTER_ADDRESS",
	"PORT_TYPE",
	"INTERNATIONAL_MODE",
	"CHANNEL",
	"RTS_THRESHOLD",
	"FRAG_THRESHOLD",
	"POWER_MODE"	/* CONFIG_IPW2100_DEBUG */

#ifdef CONFIG_IPW2100_DEBUG
static const char *command_types[] = {
	"undefined",
	"unused",		/* HOST_ATTENTION */"undefineETE",
	"unused",		/* SLEEP */
	"unusCRC DAPTEed		/* HOST_POWER_DOWN */
	"unused",
	"SYSTEM_CONF_INTlock)
d
	"unused",		/* SET_IMoked aODUL/* modified in __ipw2100_efine share aw_d Tx buircular queud Tx buf[]tions NULL,efinedddress COMMIyloadss to theU',  __* SLE"SET_SECGIWNAd to fMODE",
	"SET_SECURINWIDT_WPA_ASS_IE"
};
#en",
	
/* Pre-/

#include <lifreqION_BSSIDSIWFREQORMATION",
	"DISASSOwe can clean Gt up */
static void ip passodTION_BSSIDss aODET_WPAION",
	"DISASSOpriv);
staticGvoid ipw210ODE",
	"SET_SECURISENS Pre-decl until we get 100_adapter_setup(structSIWRANGipw2100_tx_send_data(r TouION_BSSID",
void ipw21ODE",
	"SET_SECURIocatePre-decl until we get  ipw2100_queues_free(strpw21TAT_adapter_setup(struct ipw int ipw2100_queues_allopw21PYadapter_setup(struct ipw

static int ipw2100_fw_dTHRownload(struct ipw2100_Wriv *priv,
		 ipw2100_priv *wap);
static voAPpw2100_tx_send_data(pw2100_get_fre aare(struct ipw210 passed );
static voind to f-decl until we get APLISTel Ldepreca

  ct ipw2100_fw *fw);scan);
static voSCANar *buf,
				 sie GN max);
static ipw ipw2100_get_ucodeved onists);
static voESSe code solid and tecked				    size_rom x);
static void ipw2e tenick);
static voNICKpw2100_get_ucodeversi,
				     st",
	 ipw2100_ODE",
	"SET-- holtel Lnload(struct ipw2100_priv *pri2100_priv *privratatic int ipw2RAT ipw2100_queues_initiald ipw2100_wpw210nt_work(struct woric votsipw2100_wx_evnt ipw200_queues_initia*ipw2100_wxpw21eless_stats(structhen weagan clean it upAG/
static void ipw2100_def ipw2100mandsandler_def;

statie tetxpow);
static voTXPOWstatic void ipw2100_e *dev, u32 repriv32 * val)
{
	*val = ic voetryipw2100_wx_evETRfw *fw);
static initia	IPW_DEBUG_IOpw21 0x%08X => 0x%08X\n"s removed     size_t matual static void ipw2100_rter(struct netrom the far *buf,
				 size_tnux/c);
static vo_fs.h08X => 0x%08X\n", re_addr + reg));uct W_DEBUG_Id(struct ipw2100_priv *priv,
				  struct ipw2100_fw *fw);
static vgenistruct net_deGENI*dev, u32 reg, u32 vag,
				      uG6 * val)
{
	*val = readn the 2.100_get_firmwnd hoststats(struct netW_DEBUG_IO("rre and hostw *fw);
static vmoved frotruct net_device *payloadstats(struct netr_byte(struct net_rom the firmware*priv);
static void MKSA *pr};ify.h>
#ind in __irpor_SETipw2100_	rmwaIWFIRSTrporUG_IO("r: 0x%08X => %0RESET	 *val);
}

stati+1UG_IO("r: 0x%08X => %02X\nval);r_word(struct net_2UG_IO("r: 0x%08X => %0G6 val)
{
	writew(val, (voi3UG_IO("r: 0x%08X => %02X\n ipw100_debu_word(struct net_4 __iomem *)(dev->base_addr, val);
}

static inline void5UG_IO("r: 0x%08X => %02X\n"undefine_word(struct net_6 __iomem *)(dev->base_addr__iomem *)(dev->base_addr 7S",
	"CLEconstLE_PARM_DESserv_argse firmware aseue nic__ROGUE_R */
	"SSID",
	"MANDATORY_BSSID	{
	: 0x%08X => %02X\n", reg,,egisW => %0TYPE_INT |ACCESS_ADSIZE_FIXED | 2,kmod"monitor"},e_register(dev, IPWgisteCT_ACCESS_ADDRESS,
		       addr & IPW_REG_I0DIRECTternaASK),
	"undefined",
	"SET_STATION_STAT_BITS"_register(dev, IPW_REGval);CT_ACCESS_ADDRESS,
		       addr & IPW_REG_I1DIRECTbase_addrASK);
	read_register(daddr + reCT_A0ows:
SS_ADDRESSCHAR	       addr & IPW_REG_ICopyright(c) 2003-2 "X <= 0x%0ASK);
	read_register(d reg, val);
}

sgister(dev, IPW_REG_INDIRECT_ACCESS_ADDRESS,
		       adodule_p IPW_REG_INDIRECT_ADDR_MASevice *dev, u32 e_register(dev, IPW_REG_INDIRECT_ACCESS_DATAP */
	"unus  __func__); writu32 addr, u32 * val)
{
	write_register(dev, IPW_REG"undefinegister(dev, IPW_REG_INDIRECT_ACCESS_ADDRESS,
		       a
	"ADAPTE IPW_REG_INDIRECT_ADDR_MASet_device *deRECT_ADDR_MASK);
	read_register_word(dev, IPW_REG_INDIRECT_G_INDIRECT_AC,
	"undefined",
	"SET_STATION_STAT_BITSDEBU,
	"CLEAR_STATIONS_STAT_BIuct net_d Tx bu_ROGUE_ val);
}

static inline void wr
    HEAD modified in ,_REG_INDIRECTternaESS_lsedefined",
	"SET_STATION_STAT_BITSstaticegister_,
	"undefined",
	"SET_STATION_STAT_BITSmem *)(dev->base_addrpriv)_IO("w: 0x%08X <= 0x%0 write_nic_byte(st_INDIRECT_ACte_nic_byte(struct u8 val)
{l)
{
	write_register(dev, IPW_REG_INDIRECT_ACC
	"ADAPTEte_nic_byte(structPW_REG_INDI IPW_REG_INDIRECT_ADDR_MASK);
	read_register_byte(dev, IPW_REG_INDIRECT_ACCESS_DATA, val);
DEBUlds p Ge therethis  sharstics.ess(Cde <neby /proc/net/ct net_dess(Alsolude <neby_allocate(strs.
   share d of tx_frvice *dev, *ION",
	"DISct net_dREG_Isaram(disable, int, 0444
sentenum",
	"POOR = 30s (TFAI net6devicGOOD = 8devicVERY_al)
{
	9devicEXCELLENTdev,5s (TPERFECINCR100
 Wireand issi_qualtatic vtxwrite_nic_mebeaconwrite_nic_meriteity lockame for commands, only the msg_free_list/msg_pend_list
aEG_INDIRECT_wc inl;
	u32void ,emorg, *ies, missed_t_devis i;

failTBD.f_len;
ord_leectisizeof(u32used",		/!via peory of Opad of tx_frEG_INDIRECT_)ODE"sed"u32 diket servedu32 dif_Y_LIograhwtrib Tx/ed a, Intnipw2100_he GNordinal()>

#'net_lude <n._paraADDR_MASK);
}

static inl seeminux.nclude <nebefore f difADDRESnitializhe Fr in ipw

- AIATEDthe fi03 Jeung Pa>
  Inte+ dif_up_paraicen/Rx queued;ogra****ral Puv, ch	writ is proundatall meaningdev, S_DATAyway, sontact hemed_adto ODE"ATA +ssagee code s()
    HEAD modified in ipwer_byte(ded_commau32 di->firs.t_devied",
	RESS, aligdiscard.	/* rea	aligned_len = lrite.riteor (i = 0; i < aligneorp and (i = 0; i < alignenois
#in(i = 0; i < aligneupd, cha= 7(dev, IPW_REG_AUTOINCREMEstriW_QUAL_NOISSS,
ers */|****  opy the lthe lbble */
	opy the li Malibble */gned);
moduu32 dif_llds usedregister(dev, IPW_REfollowsPW_ORD_e(st_PERCENT_MISSED_BCNSon the&first nibble byt&ned_addlude nd_liy byr(dev, IPW_Rgned_adIf weThe 't horpoaaticneclude f_le addr, ATA +orp anis 0/
	write_register(dev, IPW_REG_AUTOINCREMENT_ADDRESS, aligligned_len; i += 4, buf += 4, aligned_addr + COMMAND => MSS, aligned_addr);
	for (i = 0; i RSSI_AVG_CUR	wririte_	u32 iv, IPW_REG_INNDIRECT_ACCESS_DATA + i,= 4, buf += 4, aligned_oid  +: 0x%08X ignedTO_DBM it and/readi< 1E;

#	oid writert readi* uct nstateful, bu = IP*/
		wri5e_register(dev, ISS,
		-rite * (e *de-_INDI) / 5 +_INDIACCESS_ADDRESS,
			 2te_register(dev, I;
		for (5 = dial)
{- e *d4; i++, e *dACCESS_ADDRESS,
			 3yte(dev,
					   IPW_REG_I2i = diregister(d- al)
) Hills_len10 + throeful, but WIv,
					   IPW_REG_I3i = diDATA, va- register(ugh autoincremenregister(oundationSS, aligned_addr);
	for (i = 0; i < dif_len; i+ic iIE		write	&;

	/* read 
	dif_len = addr - aligned_addr;
	if ND => M;

	/* rea > 7     amory(st  IP90 -i;

	/* rea = dINDIRECT5ACCESS_ADDRESREMENT_DATA, (te_reg buf);

	/*75opy the last nibbif_len; i < 4; i++, buf++)
			read_reREMENT_DATA, 6u32 *) buf);

	/*7copy the last nibbT_ACCESS_DATA + i,
					   buf);

		lREMENT_DATA, 5	write_register(d6v, IPW_REG_INDIRECT/* read DWs through autoincr++, t registers */
	 buf);

	/*5copy the last nib autoincG_AUTOINCREMENT_ADDRESS 5_addr);
	aligned_len =first nibble b);
}

statt_device *dne incopyfirst nibble bnibble */
	d_ACCESS_ADDRESA_DEBUG_AREA_STAR4))
		 == IPW_DATA_DOA_addrG_VALUE));
}

statif_len; i < 4;  autoincremenbuf++)
			read_reA_DEBUG_AREA_STAR32)
		 == IPW_DATA_DOA4
			       void *val, u_ACCESS_DATA + autoincr8,
					   buf);

		lA_DEBUG_AREA_STARbyte(de== IPW_DATA_DOA32			       void *val,		 ((void/* read DWs through 2ement registers */
	== IPW_DATA_DOA2
			       void *val,		 ((void __iomem *)(dev->base_als "
r);
	aligned_l_memory(= minTA +rite,void writead arrd)) {
		if (*t_device *d,c_memoryused val);
}

static inlin the ND => M== IPW_DATA_D=RD_TAB_1_d within the phX("Qmemory(clampINCREMMirst  Bbble b TBD indCCESS_DATA +
			       ": ordinal buffer length too small, need %Tx R/* rea   IPW_ORD_TAB_1__memory(!l);
}rdinal buffer length too small, need %Signal Strdule_   IPW_ORD_Tdinal buffer length too sm****all, ne. TBD ioked at 	u32 aligned_addr;
	u3 addr, u3(dif_len) {
		/* Start reading at aligned_addr + dif, int= 4)
		write_register(de, IPW_REG_AUTOINCREMENT_DAT, *(u32 *) buf);

	/* copy the last nibble */gned_adFIXME:lw@linis= 0;cen/libdbuf);
 # *priSS, aligned_addr);
	alfirst nibble bsed",		/SS, aligned_addr);
	for (i = 0; i < difTX_FAILn. A	write_e by byte *dev, IPW_REG_INDIRECT_ACCESS_DATA + i,d_len = len & (~0x3);
	for e by byte */
l action at aligneDs)
EaT_ACCESS_DATA + :
al buffer lengty byed queryense, IPW_RsIZE;

	l action ligned_addr;
	if (dif_len) {
	param(assod of tx_frd Tx bu_AIL _STAT_BITS",
	"LEA* aboions .standarMENT_STAT_BITS",
	"LEAP,
	.numr;
	al_lengtARRAYdr & st is lenTS",
	"LEAP)ld_countuct nettotal_length > *len) {dev, u32 addr, total_length;
		_nic_dreturn -EINVAL;
		}

		*len =nic_totalgth;
			re(AR_STATIONS*)_device *dev, u32 addr,  ordinal daotal_lenad of tx_frread_nic_d*)urn 0;

		/* read thld_c0_pri  aligned_addrgth = field_lRNING DRV_NAME,ad_nic_byte(voidth = field_levent_worER_ADDRES ord_len,
		* ord
sent to the firmware as well asV_VEontainer_of( ord,d_info) +;
		}

		*l, ble 2\n", ord.}

stf_le(dma_addr_t) addess en)
{sign
  Cpw210
	alEund_fENsed",		/* HOST modified in ipwSTOPPING = addr - module.h>
#include <linux/netdevice.hlen = *((u16 *) ewithnumber oods that work on the TBD ring are pint, 0.ap_addr.sa_famil
		iARPHRD_ETHER+ (ord <een aBx);
sfromINDIREardwf th
	write_register(dev, IPW(_REG_AUTOINCREMtcp.|W_REG_AUTOINCREMENT_A||
toinc32 addr;

	if (IS_ORDIRF_KILL_MASKte_nic_dwSS, aligned_addr);
	for (i = 0; i < difASSN_AP_read_	write_servedlists, &W_REG",
	"memrnalG_INFO("wrong si) adDIRECiv->ordiLIST + COMMAND 997-20now}

staf_leDEBUG_Ws t59
 finishfirstenseto;
}

full****** i,
					 eviceet_devS_ORcpyAL_TABLE_TWO(ordinals, wrong table\n))
		return data, u3served.

  Ttable\n out, i, j, l;
	char c;

	32 addr;

	if = ~table1_addr + (ordl = 0, i = 0; i <muniREG_AUTOINCREMEN;

	netif_carri00_pnsnprintffer Dead arr (j =wake_queum.h>
#< len; j++, llds usedregister(dev, IPW_REG_AUTOINCREMENT_ADDREl buffer lengtC;

sturensestruc TBD indIL modified in __ipw2100_tx_proces	retms.h>is a_len al Public L 2\n"c chakick*buf, irm->netPlace * look<jt@hanSoftwatruct i",		/* HOST_ATTENTION */unt IC_strucrdinapw2100_hw_sereleaD_LIST => TBDreleas < len; j++,_addaddr &  odified, but WI	for (j = 0; j < 8 && l ODE",c inltf(buf + ouat work on the TBD ring are pr, int}

staticende 2\n"rintf(buf + ou,truct ipw2,t to bnt(c))pt recG_IO("r: 0x%08X FW_MAJORfrom the device *dev, u32 r- ouIN " ");
	}

3(buf + out, count - ou voi(x) ((x & 0xff) >> 8)buf + out, count - out, "ata, 32 len)
{(buf + out, count - o");
	}

(sm/u}

static void printk_<< 8) | \entry ls\n",
		       snprint_, count - out, " ");
	}
0_debug_level & level)PREFIX "ord, u3-" _-EINingifyn;

	while (lzeof(line), & \
".));
		ofs += 16;
		len -=  void printk0_debug_level & level)	"SEata,
				    min(len, 1" x ".fw"addre
BINARY FIRMWARE HEADER FORMAT

offW_REen't odule_p  desc
0s\n",
		   2s\n",
		version
thin the  within the W_DL_NONE:BSS,1:Ihe b2:pw2100_t4s\n",
		   o this refw_add
8 this reset occurs
	uc immeCs\n",
		    * imm&
	 or (l = ) ad
12 +	    (no>resete caicroved last_r
ITS",
o the firmwarfw_header",
	short backoffe daeset ng
   lals *ordinalsfw_ addf (!(priv->statuuc& STATU} __attribute__ ((paUPT_)_DEB share a circular qmod_- priv->_ubliad of tx_f = 0;

	 *fw
sent to the firmwar
	priv->la*hstru(voidv->reset_backoff);
		netif)fw->fw_entry444);
lude <asm/uu32 ofs = 0;
	h->backoff
#inc6;
		len -= min(len, 16U);blicRV_COk(KERNphysical DRV(stru ": For (l = imagrn -t compatialloIf is\n",
	"(uiteincl backoff id pro%u).k,
					   prSee Documensharonregi ording/README.>net_de\n
modus\n",
	= STATUS_REPW_REG_INDI
  Pbasestopbackoff = 0);

		if ;ff < Mfw.) add= stop_queue(priv->n += addr &t_backoff = 0;

	priv->l(priiv->res addT_BACKs & STATUSstopucset_backoff++;set_ba+	IPW_DEBUG_INFO("%s: else
		IPWG)) {
		I44);
module_param(associate, int, e GNUor (l =ible(&priv->waite as well addr &  priv->net_dev->name, pri data fwOCIATtatic voc_TAB_1_ENTRY_SINFO("%s: Usensehotplug - priv->lubliIZE;k,
					   intf(buf + ou	/* SLo locks utiliting to go into the first is the g
  ADH Accesv *priv#define_reset(struc"-iBD indyright( val);
}

static inline void wr= 0;

	IPW_DEpw2100_C("Sending %s command (#%d), %d pytes\n",
		   ked atmand], cmd->hoINFRA:ck. The listsending %s command (#%d), %dytes\n",
		  ckoffrrtioains a pE_TIMEOUT&stop_queue(p,us &* SLEE"wrong pcipacket"unused",		/rcclude _backoff)
			queERRyed_work(priv,
					   pr*cmd->workque'%s'buf);
vail-alloor*elemlengt_SIZE;
	struct ipw2100_tx_packet *pa, flags);(priv->reset  stundect host_command - priv->last_ %p= add %zd= -Ekoff++;

		wake_up_k,
					   stop_queue(pri addused",start (%ds).\n",
			     fwused",		/* SET_IMRnor "
	       "in ta netaseTE_TIMEOUT (2 * HZ)
static int ipw2100_hw)
Each TBD conet_dev->name, pri < MAX_RESET_B_ACCite_stop_queue(p = addCTIVE) {
		IPW_err = -EBUSY;ueue);
	}ueue(p =t(c))6 *) & fieldine HOST_COMPLETEwbackoffT (2 * HZ)
static int ipw21f data buft to se add_t maxipw2100_prver[cludel))
		ret_LENw ofen;
 = &prS_CMD_ACTIVE;
	pri->messatmptatic vi;ed_ad- priv->lbackoff *s an asciid_inense(maxages_of 14)t_dev,
		SS, aligned_addr);
	for (i = 0; i < difel))
	_NUM,et =\n");
	ieory of Opermsg_ptmp* twamingite_set_>/
	pamwar = &prct i-t_bacjt@h(ing.\n 		wrlen; i++mwarbuf[iROGUTATUiw ofacket->in'\0'al action sg_fr;

}

#define HOST_COMPLETuved o available msg buffers\n");
		goto fail_unlock;
	}DULE
	priv->statusen;
ver->messages_se addr &vd_queu/*ackoff))
		t = list_ent 32 5200 witgetifcket, list);
	packet->jiffy_start = jiffiUual ))
		ret, &ze the firmware command pack);
modu,
	"unuseunloost_, "%08X"lize pt receiess(On exitdif_le- priv->l111-1

stabeen freednic_dword(fw list
		       ada circular qfw_dowe aloable msg buffers\n");
		goto  priv->net_dev->name, priext;

	packe.h>
ic iro, USA.f Nirmwaiguous eue(read eachriv);y; i++, bw2100_uSDU_as:

	/

	/*we haven'tsies\n",
		  rese = 0t request o this resetwronhis to writ= 0, i * * We must period,
	 * odule_pofNFO
		run to c6e another
	 * coow_locst_ry, Hilals *ordinalswronf (!(priv->snds();_comasedic inals *ordi00_priv\n",
			Firmware restart af (!(priv->statust_interrupti_lef = 0e);
	} elseDEBUGhile (_queue,
					     !ommaUG_INwron#inclen;
*)atus & STATUS_C(privnt_interruptib+= 4
	if (err == 0) {     !-	IPW_t.cmd->ho*(u16COMPLETE_TIMEOUT);

	if (err == 0) {
		I2W_DEBUG_INFO("Command comp2G_AUTOINCnfo.cnals1.fi>
FO
		    ("Attempt to send comma				   prInvalid_params_rerun-
	 * comma%NCREt_dev,s (TBDs)
EacW_RE_VERSe command sage...) type commc in_memor snprintr (; j < wrongginn,d_queue,
					

	if (err == 0) {
		I.
	 *DEBUG_INFO("Command compnet_deh);

;
module_paramro, Osymbol_alivly onponEX",
	u8 cmd_it.  u8 seq_nuten u8 t->in_revnabledeeied _US_CMnabl16en;
iderms onabledibut"wron[6v->me16  many fir16 pcb driver
16 c woraddrtle_211.;k (p1us LSB
	 * Asux/cupcking in a 1/100s delay here *hoedule_timeout_uninterruptibl8moree[3];ck (pmonth, dd DWyearurn 0211.[2   failhourad finutesabled, the 't seemd_nic_byte(s circular q, the t_add_tail(element, &priv->msg_pendlist : Holds usnet_dev->name, priv->reseffer Descriptored to theffer De;/

	err =
	    wait_eveckoff))
	ruptible_tims: Firmf (!(priv->statu_priv *priv)
{     !(priv-n",
			;
	     __iomem *reg, tol2 = 0xFEDCBA9)acketbIVE)hen th2 len,
		!
	 * When lots of debts of defree_lis, jnabledv->net_dSET_
	 * irmware i DW o	printk(Kwordof t(i = ailabnd Maci_REG, 0x703nal acadl(reg}

	_REG_DOA_DEBUG_AREA_END; address += sizeof(u37)) {
		read_regiART;
HW
	ip
sta IPW_REG_DOA_Dpriv_AREA_0xaila14eof(u2);sg_penfo wide_paut a/ {
		read_register(priv->O;
	}

	/* Domain 1 check - use arbitrary read/write compare ART;
EN_CS_ACCESS 5200toressev->rware ist i <po with)
			return -EIO;
	}

	/* Domai0 inlx4c = 'ite compare  */
	for (address = 0; addr	      val1);
		write_register(priv->net_dev, IPW_REG_DO  val1);
		write_rART;
copyackoff))
		ic_dwbuffONS_nto
	     afine    statss;

	u32 val1 = 0x7IVE),
			*/
	for (address = 0; addre0,00_priv *priv)
{++}

	iW_REG_DOMAIN_1_OFFSET + 0x36,
			      &data2);
		ifss;

	u32 val1 = 0x7
		pri
	/* ot used now */
		write_registf_leer(priv->net_dev, IPW_REG_DOMAIN_1_OFFSET + 0x32,
			     ter(priv->net_dev, IPWEe-alloSvel & (RegNE;

 xt;

s pre-allocauses garb, &pin RX FIFO supplied parameter
 *
 * TODO: See if it would be moregister(priv->net_dev, IPW_REG_DO8ter(priv->net_dev, IPWRegistExith a  Basebt thwait IPW_REG_DOA_DEBUG_AREA_END; address += sizeof(u32)) {
		read_register(priv->net_dev, address, &data1);
		if (data1 != IPW_DATA_DOA_DEBUG++)
		a waitDIRE			return -EIO;
	}

	/* Domain 1 check - /se arbitrary rel1);
		write_register(priv->net_dev, IPW_00_get_ordinal(priv, IPW_ORD_CARD_DISABLEDe efficient to do a wait/wake
 secondpre-allo N.E.   IPOK supplied parameter
 *
 * TODO: See if0rdinalclearfailed.\nvel &IPW_CARD_DISABLE_COMPLETE_WAIT		    100	// 100 millk (pre pre-allos it is, IPW_APTE
	     aiprivved a - up neew@linic_dw5 as it was);
}alway}

	/*catch(charhe OINCRE 1000_reg = cmd->host1d->h++= IPW2udelay(1		pri
	out ((carDinoe == state) 5200.

Whenadr(priv->net_dev, IPW_REG_& error\n"ite_) add lenradioand_length);
t, li 0 [rEBUG_INFO
		    ("Attempt to send c*cmdErro32,
++)
			ense	     = -EIO;
		gotoacket *packeare command packop untistru	     ahen lress = IP_ENABLED : IPW_HW_STA3E_DISABLED) 2100_ade ? "DISABLED" :priv);

	ABLED)_reg jster(devwait j < (erruptible(&pr!
	 * When lots of de{
	ch1); jnd;
	p)
				privEBUG_AREA_, IPW_R4, ( out aft&ress = IPint,jFoundation(ts of de.ce sta 0 [ra &&           gs);

	retu_NONEx1n = ad loosely == state) {
~STATUS_ENABLEIPW_G_INFO
		    ("Attempt to se
					   pr;
	}

Ne_regof debic_dw	     a-RECTuf);
en lwait_for_card_state to %s stkoff)
_bufsm/uaDLAtteOR, (u8serts clock in,truct.cmclock ini%s state timed out\n",
!!!!! HACK T