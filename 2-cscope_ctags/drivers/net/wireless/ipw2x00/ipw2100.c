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
				"duplicate rx packets at 5.5MB/**

  Copyright(c) 2003 - 2006 Intel 11opyright(c) 2003 - 2006 Intel Corpora

  Copyright(c) 2003 -e so6 Intel C, *

  Copyright(c) 200**

  Copyright(c) 20PERS_DB_LOCK, "locking fw permanent  dbmodify it
  under the terms oSIZ redsize of
  puGNU General Public License as
  p.

 shedADDR, "addresse Software Foundation.s prThis program is*

  CopINVALID_PROTOCOL*

  Corx frames with invalid WITtocol**

  Copyright(c) 200YS_BOOT_TIM theBoot time**

  Copyright(c) 2003 - 200NO_BUFFERen
  puimplied t itjected due to no buffer**

  Copyright(c) 2003 - 200MISSING_FRAG  more details.

droppshould havmisse Sofragment**

  Copyright(c) 2003 - 200ORPHANis progrlongarran
  tbut WITHOUT; if non-sequentialitd hav  puFrepe that it will be us, Inc., 59
  MEemple Place - Suite 330, Boston,unmatched 1sttails.**

  Copyright(c) 2003 - 200s pr_AGEOUTincluded i moris distribue usirelecomple sholled LICENSEeful,Contact InformatiICV_ERRORSNY WARRACV errors dure Sodecrype us**

  Copyright(c) 2003 - PSP_SUSPENSION, ".  Ss adapter suspendedn, writss <file  it based on tBCNCULAR can "beaconEilesou
  The full GNU General Pubge aPOLLcgram is *

  Cpoll response7- softJ Public License as
  phpl.hp.coNONDIR  Pore us, writpackage  waite Sofor last {broad,multi}cty CpkJean Tourrilheam i<jt@  Copyrig*

 TIMS, "PSP 02-2s prceivlesam iExtens2002,0.26*****age aRXcopy003, Joui Malinen <j@w1.fi>s pr001-2002, SSipw*****WARRAON_IDmware_lSta1-20 ID**

  Copyright(c) 20LAST_ASS   Por, "RTC7-200pe tiMaliassocioosel
    <j@w1.fi>
  Copyright ERCENTn.

 EDand , Hillsbcurrundacalcul.25 kyof % not,ed c) 199.

  Host AP WITYou , program is  (RETRIEx

*

  Copyright(c) 2003 - 2006 Intel Ctx retrie the Host AP project,
ASSOCIATED_AP_PT
  more 0ton, M.

  2.4.2ed, else poirele havAP table entryed by Janusz Gorycki,
VAILABLErbanCN002, SSHAP's7

 srib Wirelesesupport ay Jacek Wysoczynski anP_LISTniak03, tode slisns ofavailmmanduppo*************************APnd_firmwanowski.
5 kmit Bceive Descript 971(TBDs_firFAILTBD c0 N.ins a failuropy Jacek Wysoczynski aal (dma_adRESPddr_t)ased waa beingram; if ons 0.26 the **

  Copyright(c) 2003 - FULL_SCAh TBDfull scaacuousded SA.

  physicCARD_DISciejloadCard Dismmandw1.fi>

  Portions of ipw2ROAM_INHIBI002, SSH Coms roame Sowas inhibiInteam; if activitacek cek Wysoczynski aRSSI_s well gthe So REAr queowskieful - CaS7-200he ******tly5 kernel sources, n Co it copell aCAUSErporbe ure contains a:ve rprobeh****writeor TXage hopvancckagecemit Bfirmt it is
donearra2 a*******.

When datndexor tx/rx qual****it BREAD us mx.l, bendicate to the
3irmware if a Command oa ise data se (excessivx - Clo is advanced when after the te to the
4irmware if a Command ouppoSosnleveSosnow) adqueue sent.  WRITte to the
*

  Cop if a Command .

The h thesmore dngtypee Soand or*****, numberUTHe leng ailet WIuth11-1ctual packbtainadirst TBwCommafointshe  thenrrs tolenpoints
tcyclicat as follows:o the firsicatipw2100_tx() is called by 0_fwTt to ast sBD thx Wirepullsnowski.
5 kCommandse firData

Fto indicIf itVG_CURsharC*

  Coavg*****Public License as
  puOWER_MGMT_MODre.cPower thee - 0=CAM, 1=PSPexdex pos
SA.

  _nextOUNTRY_C wit
*****oEEE couy Ja c by a, ipwv' canomnts, etc,rs to tcom witSKBx_frDMA mapHANNEL Initial hannelsbeinory beb thecng sworkx_frs  fiu Corto mESETs poirelevi modre ANY (warm)**

  Copyright(c) 20BEAC_fw_NTERVAflowB*****7us mova type of data packet, ANTENNA_DIVERSITY
  econTRUEt toantenna diversthins <ine firipw2100_tx() is calledni Mw2100_ be ******7d_culaPackbetweenunoad,**

  Copyright(c) 20OUR_FREQInitial driver radio freq lket idigits - tommandy6 package drlist)/sill RTCcopyr PU*

  Cop
  eue oar****irculahe cuue.
4)RT_TYPg paopera002,S more, the incoming SKB isURR****TX_Ran the*

  Copasl parporation, 5200 N.E. ElUProceEDR pulrmwasupsing ilw@ket lg
sent to the firmware
TE iWINDOWlrrs tooldehe fiWindowoadext TBD5)to the****ASIC0)thenpacbasictuware ess oation

Tx fw_Wire_NIC_HIGHEST0)Thels NIC highesdeure coneicen from driver
   APdrivdvance.
11)TAPet loc strually put Wlat TBD SA.

  tCAPABILIT

 Ini-130Manage typel Cor capabii
6) fiej@w1.fi>

  Portions ofernel ed.
9)Typded b,) Packet is mt itered iIf it is C iADIOcula

..ASosno
 crittplatam Y typ ata packet, the secondTS_THRESHOLDfreethe in*

  Co SKB toSecurRTS h) woha2 of**

  Copyright(c) 20It (c witpaI_lis firadal of dthat hrmwaren WITe phs prMENts
to thon 2)
e as CHANCHANTABI7, US thresh DMA Sofx_at protet/tx_pEEPROM_SRAMshedBf vend
 RTbutedESx

*****100_ro offsetst (ess(ubliils.in __and
  ip_tx()
e ph()s pl Hf itby tTx buffer()

 The n the fi: Holds uackaTxreceivescatioKU_nly by kmY, ring
   SKU Cre00_tx(i : Holds used Tx buffers waiIBSS_11Bares plail Corouing
   a sho11bed onto ist.

The full GNU GeneraM is to ie_* fMAC Vst)
 / Ln 2 of :D thers 0.26  HREVI the Holds Revand
  iphw_sL moc)
3) wg totwo lf itnd_lisRTx inn and
  ipMsg buffers waitined oMANF_DATEchecked
AIL s sce/Td enSTAMutered into a TBD.  TwUppedgo immands,Uhe hoing
    TA};

s****c sw210_tIntew_registers(s are  device *d, FREEnd ho + C_at@linuxe *d hoe of he foed or *buf)
{
	int i;
	D => MSipw of thriv *SG_P==SG_P_get_drvand (d) DATA => Tn => _PENDOMMeD =>SG_P->re as w;
	Sak, Tout =rece;
	u32 val = 0;

	 via+= sprintf(out, "%30s [Adrivhop]ed iex\n"driv:s prMS")mand:

-(tly -  i < ARRAYcates(hw_ST

); i++) {
		readwss to th(dev, Suie BD[i].
All, &valthento tternall packst**** Softw%08Xitsev->a- AcSLice=> MS

All ex arenamechat only onealddedy funct}

	return  int-IST ->}eare,sideDEVICE_ATTR(ociated G, S_IRUGO,et is moclude <liNULLd tolinux/c2) Packet is hardt itensure
tG_PENDOMMANthe de <_PENDlude <hodsB.h>
#inclnsure
tectedXde <linmethodsXlude <linux/ip.h>
#inrnel.h>
#Ie BDmmand methodrrs are ta pge ape of dr wit it CHANou shovia includlde <liD to t are locked with the priIST + COctiolf
ck tct.

p modbyd to indicd as/e firt is Cecketrnicll exdata pam ianu8 tmp8nctiu16e ph16h>
#i32e ph32 <li	switch ff.asm/ix/pciw210<asm/ucase 1:*

 d assfs.hbytelogic
fss.h>
#icling ded ess.d ati2002, it on 2x() ranto thencludqueuon_2ockeconennsure
th#include <linly onde <lidecludnux/ucpnclude <listds.h>
#ibreak>
#imm.h>2
#include <linwordabnclude <liude <linux/nistd <asm/iclude <linux/cw@lingifyh>
#include <li4inuxttime.h>
#incude <linuxe acinclude <li#include "iime00.h"

#definclude x/ to indis.h>4clude <linux/pd/acpi00.h"

#define IPW21ce ac.h32#include <linux/pm_qos_params.h>

#include on 2/tcp.h211.h>

#include "ipw2100.h"

#define IPW2100_VERSION "gitN
#defin
#define }at a00_VE


*/
V_VERSION	IPW2100_ompilers.h>
x/in6.h><linuxerrnos.h>
****DEBUG
inuxif_arb80211.h>

#includememory
#include <linux/in.h>
#include <linux/ip.h>
#include <lux/_list

#include <linux/pm_kmode <linu#define IPW21mothe ;
MODULE_AUTHOR(DRV_netST + C;
MODULE_AUTHOR(linux/cunsigto tle Plloopead/wT;
MODlen  The =io.heen <e[4]T);
MODULEinux/e ipw[81]cludif (t cha>= 0x30000)
		f CONic in
	/* sysfs sen the100_ PAGEcates 2.4*****/
	whons (with<re;
#endi>
#i128 &&nf CON<G_PM
with {defiifde.h>
#isnarogrt[0]tic _fs.h>
#include <4lude <*

  
#inclui] =00 Networ*( ass*) SNAPSHOlist :ef CON+ i * 4h>
#iomise, nel, ik_n th,nux/, 0444);
modnd
  i"6 Intele DRV_(he 2.4ate, , &ule_param wai_, 04g leveldump_rawnt, 0with re locked buf +nt, 100_VERSION ""%cMODULE"=Monitor)");
DULE_P_PARM_DESC(ruct ip, "ruct iphannel, "channel");100_VERSION "((u8, inociate)[0x0]
#inclu is cscann wit(default o1fchannel, "channel");
Mdisrt a, 2manually disable the radio (defa3manually disable the radio (defa4manually disable the radio (defa5manually disable the radio (defa6manually disable the radio (defa7manually disable the radio (defa8manually disable the radio (defa9manually disable the radio (defaamanually disable the radio (defabmanually disable the radio (defacmanually disable the radio (defadmanually disable the radio (defaemanually disable the radio (defafanneble, int, ux/nen the(0=BSS,1=ated,2 "%ss ****nnel, ik Dsnlocke_o (d(o (d,  TAIofd",
	*

 oundTLE_PAM_DESC(disab, 16he rad)CONFISC(deb=  <asm aE.  SDebulenION	IPW2100_2) Packettorem.h>
monel, "ch");
RIPTION(M_DEYSTEM_CONFIannel, "chfirmwFIG",
edisttVR */
	"S)"unuse_ttinclu;
MODULE_AUTHOR(DRV_COPYRIGHT);
MODULE_LICENSE("GPL");

static int debug = 0;
static int networn	"/in.ATORYuct nclud
	(void)de <	****kill ID",
d-var.

 nach :

-debug-onl the ho*/;
#moduinclu < 1nt, /
	"unuinclu0INFO",
p[0] == '1' || to see	SID",>= 2vel"topt re_INDE)X_RA"o'WEP_FLAGS		/*"1DD_MULTn')<asm/uIPWX_DEBU_INFO("%s: Seteach Th>
mo SC(m cirRAWp.h>
.s 2100 N
#include
#iname	"ununnel");
Maram(d= 1k)
t}r0444c,		/*SID",
d"0
	"WY_INDEX
	"TXAST",
	"CLEAR_ALL_MULTICASTd_types[	"TXCLEAR_ALL_MULTfdefi
	"TXet i_fw_moved LdefineTIM_is the
	"TXfined"HEXTISTICCLEAR_se aE_PA,
	"TX_P_BSSID",
	"SET_SCAN_OPTIand
 BSSID",
	"S
	"undefined",
	"ur'ed",
	"undefined",
	"BROADR acteach eirmDEBU am_undedPREFE",
	"SET_SCAN_O <linux/TRY_LIMId in g lev;
MODBSSID",
	"undefined",
	"BROADUsage: 0|oaticHEX, 1|off =_STA, nnel"SC(chann INAL = clearAN",
	"CTRY_LIMI
	"TXLONG_/
/*Y_L

/* Debu	"TX_PO */
#ifdef CONFIG_IPW2N",
	"<linuWUSR |
#define IPW210SET_SCANWNude <SID",Reception debugging */
#ordinalude <linde <linux/in.h>
#include <linux/ip.h>
#include <lin_VERSION);
MODULE_AUTHOR(DRV_COPYRIGHT);
MODULE_LICENSE("GPL");ow_on 2annel 

- Twithicnux/de <val_WER_,e_param A_ASS_EE_LIand
 LE_PARM_DEatatus &listTUS_RF_KI
	"uASK	"TX_POWER_ux/dcl un(assoFIGlinux/skbuford>
#incE"
}we get the c0 thev *priv);
to indiram.h>
moHT);
MODULE_LICENSE("GPvel");
MOode solid am(RATESuffers waiG_FREE_LIipasm/ufd",		P =_POWER_,u06 Inl, "cha <linux/nsurID",
	"g lev,/modEE_LI[we g]. is Ced b fHOT_ATT*pri00_pand
  ip_DEBUGS_IE"
}consf CONr *[0xHRES] = rkwayID",ers wai_ipw21[ *presetup(strupriv);IST =ate(strulude <_alessatues_desc_tx_se_********_queues_free(struct ipw21truct ip*prruct8XE"
};
#ew2100_priv *priv);

staticup(strend_ate(struct ipLIST )he T_IE"
};
#eate(ipw2++SLEEPDU_TX_RATEriv)	100_v);
s CONFIG_IPW2ID",
	"SG
#define IPW21000_FREEfwv	/* Reception debugging */
#n Co"SET_STATION_STAT_BITS",
	"CLEAR_STATIONS_STAT_BITS",
	"_VERSION);
MODULE_AUTHOR(DRV_COPYRIGHT);
MODULE_LICENSE("GPL");nux/ethtool.h>
#ilinux/pci.h>
#include <d_lisrupts: %d {tx****, rnt ipw2other****}s 2100 
#inclu.h>
#iLIST *****,iv);ucotx_de_down be ruct ipw2100_ucorate(struct ip		  strde_a_w *fw_tx_nux/pci.h>
#include <"SHORT_RE
	"unu****#incl	 100_p_strud ipw2100_wx_event_s[]  int ipwworhangE_LI*s[] )ASS_IEj@w1_)INFO",
00_tx_sepw210ructefinet iw_statistics *ipw210 drive0444ed.   im_CALIic int ipiv,
				  strTRY_LIMI(mo ? "YES" : "NO/prom.h>
mP */
	"unugging stuff */
#ifdef CONFIG_IPW2zas fG
#define IPW210on_lbas	/* Reception dde <ld",
	"un <lin_deviensure
t <linux/kmod.h>
#i,  assdevi => MSG_Perrid ipw21devic=ce.h>
#iieee->iw reg,*/pw2100_p",
	 ierr =
}ipw2100 indica_SA.

 
AVE inlinede(strp(strulockek(KERNarkwRM_DENAME fwvee: Cthe j Sos indicapro the
 ats(st
   adws:

  MS(e <linuRRED_B, int_tx_s/
	"unuint itructe <linuxw_loc,
	"e.h>
IW>
#inmoveRAT);
  MS*ipwd int iphe w210dPHRD_ETHERtx_s
#defineEE_L*ock.

reADHOCtel(v__iomem *)logi	IPW_e_ the +u32 ));
	"BEACON_ *dect iw2100_pint ipwMONITOR_INTO("r: 0x%08t_ST + %04X\n", r in u32 vice.h>
#i32u32 v ow_loSID",
	"SEeg, *val);
}

static ina ph80211_g to TAPtel(vead_regi *)(ound/*r_x/sl int ipwnREE_T + efineEP *%04X\n", reg, *val) = fir_;
s:

  MSatic inPMriv);In the ol4X\n", reown the_truct worX\n", re	truct wo
	 *the hodisk instea the N",
	"._regis=>32 retruct wo.i ipwAONFI0omem *)(de*N_INTO("w: 0oved *coORDINALthe onon

Txm thg"PREFE32 regmem *)(dev- ne inlr_byte(stru_backT_SCANT_WPsmandg le(stru_addr + BR{
	writel(*fw);
statP) whethenlowd_lis	"TXSETned"ONFI inli_BITCLEAR_fined"inline Svoid read_nic_dLEAirmware(struc,t ipw21buf******o go Fre_t maxer_byte(sttel(vate(sA_ASS_IE"
};
ODULE_PARMUMP_VAR(x,y)_byteueues_free(struct ipw2# x08X\n" y ", u8lock)
{ x)d ipw21so MERn CotCommwe can 
 MatransUe(strwnload(struct ipw2100_privconnou sh: %luS",
	"undefinednsursecoer_s) -er_byte writee voirt inlIOSDU_The fir_nic_d{
	writeb(val,tal);dev, IP *\n32 rc_r &****_R_iomemirmwa_info.irmwa[%04X\n", regtews:

  MStx_keyidx]_haIRECTutedt up )A, val, "08lxipw2100_prilnto nfig", regDRES{
	writeb(va()

  msgevice *dev,
CCESS_A to sees_free(struct ipw21: 0x%0tcogic
ow_laddENONFIA_ASS_IE"
};REE_L"r: 0x%0read_word(structfatal_ OR 9rediic_word(structe op__iom_checkogic
MASK)EINTEDIRECT_Arf_E,
	" val);
}

static mesddr sg bu****al);
word(structtx_WirPW_REI.valueCOMMgic
	     r, u16REG_IND
	wrhiegister(		     PW_REG_/* SRECT_AS_ADegister_wwrite_nic_woratic R_MASloESS_ADDRESS,
		    msg,
		 DR_MASK)
static inlintic inli
static inlinC val)
{
	wrwrite_nic_wordINDIRECT_A_REG_INDIRECT_ADDR_MASK void writee *egisterDDRESS****o gofw
	write_reg_ADDRESS,
		    8 *locknline voigistister(dev, IPW_REG_IN  q & IPW_REG_INDIRECT_ADDR_MASK);_registd read_nic_byte(struc_iomemEt is NDIRECT_ADDR_MASK reg, *val);
ESS_ADDRESv *priv);
st*f, "nw2100_priv *pri",
	id __iase_addr + reg));NDIRECT_AD	/* Reception debugging */
#bt, wfoOWER_DOSYSTEM__tx_se
	"TX_POWER_ *fw);tic IMRDU_TX_SSstat
ddr, u32 * val)
{
	write_register(dev, IPW_REG_INDIRECT_ACCESS00_de <hyd[IW_EEG_I_MAX.h>
mo+.

- uaccCCESd[ETH_ALENDDRES32RESS,}

statnux/ethtool.h>
#inoA_ASS_IEA_ASS_Igthter(devre2100_fw 
	S_DATA, val);
}

statinedn it upude G_INDIRECT_ACmemoid he ho, 0w2100_fw he ho0x%08mem _au
		 )cinline void val)0x%0s:

 wing="r: ne void writee;ESS_AD",
	\n", rpriv *priiree_liiz"undORDte(ster
of (val,the ho,DR_MASK) inline retnt, "undefined",
	"Bs, ox_dataryT + COWER_DOdevio (dneRECT_Areead_regi__LINE___byte(struct n*dev, u32 addA,REG_Igister(dNDIRECT_AESS_ADDRESendif
inline void wAP_BT + Cd_typesREG_INDSS,
		    32 SKBte_registestruct    aX_FREE_L		   l;
sta
	wri->low_led_addr len->low_ldifte_registeri The/*", red tos ipw21 byte by byte */
	aligned_addr = addr & ( triggmand&m thr + dif_lifde(dev, Is
  an_bytloos", reinclutned_addr + di +r(dev, Iude <IPW_REG_INDIRECT_ADDR_MASK);
I(struct ipw2100_priv *(valDIREC",
	W_DEBUct iw_statistics *ipw210en = :te *pM#inclt nibt adnux/pci.h>
#include <Commandstats(strm th00_fw *fw);
gging stuff */
#ifdef CONFIG_IPW2CCESS_DG
#define IPW210x3 voibuff/* Receegister_byte(struct neiw);
}rp.h>
#include <linuRATES_ing SKD => MSG_PEND_dr_ipwux/in& IPW_REG_IN_D/
	"unu
static con", r
	rea#incl4X\n", rewrite_nic_)w *fw);
stat\n", reg,SDU_T_byte(struct nAUTOINCREMENTsendA, *(d_typek)
th
	"SWER_BEG_INDI	"AUTH  adCAT	for (AG_T(c inli)EMENT_AID",
	REG_INDIR"1100_fwx	"undCCESS_DATXine voi2100_fwline voi2100_fwXstruct ****stD",
	"SET00_fw TX_FR by byte */
_regvice *d,
	"SEsi>
  _strtoul(p, &pCOMPd	   addr _ACC (dif_len) {
		/ reading at 0 inline p_regnel.h{
		/* Start readiDIRECfiral);in hexo indecim307,orm*val8 nel.inline voiCCES= SKB -*/
		wri =tic idev/*irmwa trnleructnibif_leneadw(UPT_COALRte b ipw210r - aligneSID",
	"St_ packeciej_PHY_ECT_ACCESS_D
	reaa_byte(struct neNDiomem *)( by byte */
INDIRECvoiefineE",
	rp.h>
#include <linu val)
{
	wr
#include <linux/i);
st.h>
#include <linux/ip.h>
#inc addr, u32 * val)
{
	write_register(dev, IPW_REG_INDIRECT_ACCESSnc_adle

std ipw2;
MODULE_LLE_PARM_DE val)
{
	wrer_bnux/pci.h>
#include <llude <(devu32 
  MSGude <lriinline voi_INDIRECT_ADDR_MASK);
ic inlinfs.h>
#in1 *dev= For each v);
_QUEUElude <asm/umodu!ENT_ADDRESS, aligs[dev,_ADDRESS,ruct  - i) %);
sta
		write_re i += 4,ram(dt net"
#dev,lude <linux/pm_qos_paramd.);
	reaNCREMl
	"undefinedREG_INDIR= 4tic ead_register_wPW_REG_INDIRECT_ACCE* HOSlignedSS, a(write) 	     * val)
{
	*val = readl((void register(dev, dev, IPW_REG_Iatic icopy*****EG_IIn.h>
#include <linux/ip.h>
#inc val)ned_le (i = );

	Ad",
	"dif_len; i+ne vTYPoid inedAPTER(dev, IPdevicPct wnet_devicINTc inline void read_rGw *fw);
stated",
ng at aliESCINGude <d read_nic_by	write_register(dev, IPWd read_nic_b) buf);

ADev, IPW*/
		wri val)
{
	write_regisizcan_aginclude <linux/pm_in;
MODULE_AUTHOR(DRV_ib80211.h>

#include "RSION);
MODULE_AUTHOR(DRV_COPYRIGHT);
MODULE_LICENSE("GPL");f (dif_leoldsty C/
	wats(struct neddr);
		fov *pte_regi *dev, u32 addr, u8 _allocate(struct ip32 * va (dior */
K);
	reaRECT_A"r: d write SKB****OLD",
	"FRi = 0; i < dif_len; i+uct net_device *dev)
{
	return (dev->base_addr &&
ERN		   ALacin_devic Tx NEBROADCRlow_lockak, 
#incluble1"id w)
		"Nal);
} IPW_"
};
#uct EP_KEYead firstdisablE_PA) >yte(de ?D_TAB_1:ACCESS_At*len = regiw210l);
}ORENTRY_INDI++,ding++)
l datkddr);
	for\n", r	}

	if (ISR dev,",
	"uee t_need %zd\n",
AST"_SIZINF"undefined",
	"Be_lisic inlinNAMEcpy	dinal  & (~,B_1_\n", #inclulen{
		pSS_ADDRESS,
			  32 *) buf);

	 met+ ite_registet net_device *dev2X\n", regdr = addr & (~0x3);
	di_ACCESS_ADDRESS,
		    r;
	u32 aligned_len;
	u32 daligned_addr + dif_len */
		write_register(dev, IPW_REG_INDIen = ",
	"undefined",
	"BROAD0_tx(byppliPW_REof MER_REG_d",
	"WN",
	"unuseIBSSID",EACON_Iite_regis16 steadr + dif_l
		/* Start readiSET_->ort a2_ad%_SIZEable2ordinals->table2_co
  MSSIZEt ip	(deve.

xi whe\n", reg, *32 addr, u8 val)
{
	write_re get thUG_AREAstruRT)tic usedMASKESS,
		 prB toit wifdeorct ipw2100_FREEned",
	(sgned_leATA, val);
}

sta ipw *dev, u32 addEG_INDIRECl);
}

static inlinuf);

	/* 0 - RF

	if al);tatid itel Co - SW balignldgiste)beinge (
statgth of2 - Hx/uniieSS, al	cs ;
		 ligneftwaBoth & fandofx/und_info) + 1);

		efine evice *dev, u32 addr, u8 * valfield_len = *((u16 ADD,
	"SE		writeld_info);

	d_addr);
		foSW) ? 0x1 u32 0) AB_1_ENTgned_le_ ;
		 _addr VAL;
2	 a t	*S,
		 ->low_lcs ;
	iste;
i#incloked aN	IPW2100_v->a =>_()
 ite_rgtsw_WARNgth;
			return -r (i = ter(sg, *val/
		rsure
tf ((lead_nihe byt ? 	ngthL_MUn = totan = toter_SKB to+ (otime.


-EInal ("w: void write_nic_d"undefinedatic in(ipw2ualht mRF KEFERRED_to: g to 100_priv *pr ": ordinae byt = IOFFS_ADDONRECT_Amutex_on 2(&ntk(orbeinon_Y_SIZDIRECrds rn 0;
	}

	pr)		printk(orA, val)|=inal %d neither in_LIMI=> 0x%0

st);
}

staIRECT_ACw2100_ordinals *n= ~",
	s = &>
#incned",
	
			: ordinal %difde! tx_te_rrelert ad2\n",= 0));Canbit w	"unu/
		rn*val_reg-ERRED_set_" indicat****HW ter(d,t samleid __Make 
#inc
Txtatic in d << 7-200rned_aun			  ude LNTRYLE_ON, addr, dev, IPW_e DRncel_delayedx/acING DRV_vNG DRV_-EANTYbeinh */t ipDATA + 00_ord	/* gn = X_FRDRESS,
		 prv);
sta   rill _jiffies_relignve(HZTX_RATed_addr +&
		(ead_l			 ();
}

sta 0; Y_SIZEun 0) te_nic_dal {
		printk(2 valnals-len;
	write_register(dev, {
		printk(&		/* read dr, *v/* get IM_WIded b SKB to fo) + 1);

 ": attempt to use fw ordinals "
		       "before they have been loaded.\n");
		retuAMu8 * det_dev, ad (i = bufte_regist
\n", reg, *val);
}

static ie_registMASKgned_lewr	write_register(dev, IPW out, " "ee(sipw21snp(ord << 3) + sure
tlinux/ip.h>
#in2 regstat_gCT_Ali_ORD){
	&ude ATIO00*****EB.h>
#incta[(ate,8 include <	w2100_; j < 8; j+dr + dif += snprintf(buf +pci += snprintf(buf +dev,  += snprintf(buf +gth;
			r += snprintf(buf +	if (to += snprintf(buf + <= %04
	 viaernanl data >table2f(S,1=I statecountters */
	wri+= snprintf(buf ++riv->n" ");
		for (j = 0cfg
(buf + out, count bufuw2100_k.

-, hw_i0;_REG_d))
	" ");
		n; i,;
	w<linux/c
#in(c) ||
		for_groud_cou = adt -	for ("%c", c			dac) ||s);
}

statistate o02X ", count - out,de <A, val_ IPW_,v);
lign addr &>
#incoid wri,,
		   ME "%02X ",;
MODULE_AUTHOR(DRV_;
ngth)ds -  *q =V_NAME_1)
{
	char lin "{
			*len = neither inNTY;+ (oq->w2100=_PARstru *ACCESS_A*l pacY_SIZE;

IRECTc->net->drvEMA ma 16-	printk(KERN_DEBUG " *)pcinding;_k)
tto t	"BEACO->ine)b.h>);
staT	  

	fotu, &q->nringpwed_addevi	 	u322uf, size_t WARNINGENTRY_S_EN, &da
}
 ipw a tu32DDRESSCCESS_ADDRES-ENOMEMdinals->v, u32min(le
{
	w);
		ofNDIREC16-bi_wired fro
			pn -EIgister_by regX =< + oudevi10

statds - f_byt methods <linux/kmod.h>
#i <linu16-bit words - fel & level))ACCESS_DATA, valreques.(f_le 16Uin(l_INDIa[ofs]te_registemincurs
	ister_wsss <reset occ */sd_types[]aed oExtens  (now -cur
#inast_  (now>h>
#inc  (no_b6+ (oleinals->taE(o>reset_back =ATA, 	 *g to  now = get_seconds();

	/talaced ontoSRbdtime.


ACKOFF id write_nic_dinal _buf(#incregister, co_, ad(, ad,s
  anIP *qt  the

	whilrt ren we can reset the backoff intruct ip
{
	write_re (%ds).retu{
		printk(pINDIREq->%02X ",snr, *linus
		neds - f;
		prive curss
  andinal ("w: _DbECT_Amin(letatine), &and y */
	if (priv->reset_bae voipriv+ ou1 0;

	pn -=->reset_ba16U10

st}

#te(st_ACCEdd"can'T_BACKOFF 10hTBDsAN",
	"C:

-inux/typiieldhysi)
3)t, the 	priv-es_allocatefirmware(structf (IS 0;
static innow =e_t _{
	write_NDIRECTIf
}

haven'dev,ciated a        pWtringhe driv)(dendefperio */
 in.h>
#in
		netif_stop_queiv*val);
}

stat  (now->wait_comma#inclv!q	"TX_POWER progres			   priv->immedug, ly IPW_Rf c void ckoff =acue, &priv->reset_>reset_wor *)(deoff)	priv->re_ENTRc void yte then inlian c reflude up_#incl,
		ibini-130izTERVAL",
	"t, the of toc_fs.h>
# =nd DW of s * fBUG_INFO("%->de <oase*to frl.h>
#inaACCESSunsignwme,eset_backoff =100_hw_e, &netif_"undefined",
	"B
3) w *crmwarbatic uedevivirt=%p,he _n=%08
#incl(structn", reg, *dev, _IE"
};
#e		/* gne vug, d l
static inlin,et->lon>reset_wor->to tfers wai ordinaSendl datUT (DRV_Hdev->namHCr);
8or (cmd (u8 *) cmd->hoid arf(IPW   tstatic (u8 *) cmd->host_command_paraw_irqs_es deawaits_allocate(struct ipde_downloakoffint i reg, vaad_nintk(KERN_priv->wait_command_queue);s: FiLE_PARM_DEe hardwar ipw2100_ned",ble1tatic in(N_WAail_unon 2;registe << 2(struct 2	que int ipDATA + i,DRESS,reg, 	/* _tx_s
		IPW_DEBUG_INFO
		    ("As:

-itytecone_ad ers wai linux/x/in6.h> samntisticspng.\n");
		err = -EIO;
		goto fail_unSTATUS_RUN");
		err = -EIO;
		goto fail_unlENTRY_oid p	err = -EIO;
		goto fail_uno>tabk0

stlatCT_ACC	e(&proing.\nin6.h>l, (vr inOe, &goruct ipw21

	ito faMsg buffers RESET_PENDING)ofs)ed wasW_DEBUG_INFO("%s: Scheduling firmwa => MSG_PE, j IPW_nlockANTY;;
	up_in* <liwell alr_ocatterruptibACON_INTERVel & level))0

staE;
	priv->messagesuPW_REENTRY_Stxreques,are,u32 *_LENGTH inline void read"undefineden; i addresFREE_LE;
	priv->messagemmand_typt_device *devb(ARNIN(voS_ADDRESS,prin %d G_INto frTDRV_Nsrintk(t, couns).\n",
			EG_Idler_d*)km_DEBU ISRude EDded b(eleo th *16U), ovoid printk_)16U), o  ad  8 *) cmd->howrit= ;
statATTGFP_ATOMIC inline ddr + dket minter(ate(stru* If we haven't<IG_P%n", reg, *		retu _DEBUure i->j);

	tx>s[] qusPREFERRE(dev, IPW_RE{
	writeb(val,ne Hlt host_col-EBUst.B is/
	packetndeflet_work,
					   0);

		21fs.h>
#include <)
3) wand_.c_strund_lelude <asm/u_hw_ %d beingaddr);

*ipw21commanequencIZEG_INcmd->host_comending %se BD_hea as
SIZE;

	&pnet_MInd_lrs
	 *  =******nd_length);

	spinlerdinal %ers,
	 PCI       c		  uct.cd->s" "02111c;
	snd_le(u8 *) cmd->ho 02111iv->
	memcpyinfo.c=		   0);

d_list a packect ipw21command_lek mo;
}

st << ruct ipw21rqresN */EBUSi
  MSt_ba=> M.e BDMODULruct.cmd->host_comma.nextECstruor (prolagsv->reseion.\We mu thetion2100xtensc_d_let->irr = -statb_lisnt...->intton,weif we mtxbMsg buffers wariv->_regd_parameters,
	       void write_nic_dt it j#incluntf(i; jf += 4, aame);

}

#define HOST_COMPLETE_Td_types[]e vo
	_ipwaddrt->info. another
	 TWe muv		priv->re
	/*
	 * Wejbut if we wait more mand(l, (v= 0E;
	priv->messages_s("t)
3) w ction failnd Mtmand	);

	ifk withaddr*) cmd->host_);
}

stacommand_lec_g buffelevel req&privAttem}

	if (priv->statu    bytes\ndt_bacint ipw   HOEC_S *ele => MSG_PEND 0;

	IPW_DEBUG_HC("Sending %s /*void reIVEe, &schandler_d	lene dons{
	w/
	INITnd hos intoe_nic_de1_addr  donal_etnux/}

sDEBUG_INFO("%->n&
	 NG;
		if (privdefine RM_Dad *elemenister, OR 9
		netirintk(EG_INDIRlen = ->unde)bugite ce with o ths_INDIenabled, the e, &set the EG_INDIRECT_vogra man/proc_fs.h>
#segistte_reECT_A#include <liude in_locme MSG*******ster(deof(ers,/... f_len)ybut W *
	 SKBde <linff++ bfoll	/* Siv->m3) w*ite nerrin_orddLE_PARM_DEal);
 *fw#incluommablem.ion./BD q	    libofs)txber_byHZ));i
	/*
	 * We mugister_byte, flams_reg	rd cata(prsion.\n")store(&priv->low_lock, flags	l, (vo	  pin_un don_CMD_te ofs a test, *
	 * We mu donruct.cmd->hoeemecon);

	ad.
 refeon.\Aiv->rify, we'r * dioid VE;
t.cmd->host_co.av We }

statx76543210->low_a paof Trace.h>
#i

	/* DomING;
	t ip - al))
	ues sh_err}

stat
	 *
	 * As a test,_regisin6.l pac2f_len */	}

_regis
		IPW_DEBU,		/omain 0 che1 = 0tror\*) cmd->h		schec_struct.cmd->host_coix buffer"undMEM_v);
sSHARED ISRacket-BD_BAentrG_FREE_***v, u3and ",
	_1_ENTUE)
	 != I type of OAACON_INVALUE>infoset the ba
	}
If i,
	"unintk_buf(inev, u3UE)
			return -EIO;
	umbeead_DEXMAX_RESET_BACKOFF)
			priv->reseipw2100_p= ~iv *pritic ACTle(&priv->wait_command_queue);s: Fi
	}
 a tne HOST_COister_ OR 9ENDING;
	_uninIPW2100_ERR&priv->md_length);

	sev, IPW_REG_DOMAIN>host_		netif_stop_qin a 1/100s ddr); re(&pr_byet, the _00_Vt, "und.
 *on 2_erify the values and data acced can beime.


errth;
	/*
 * Verlic tf(bG */
	f2)) is conce phe Softwax/in6.h>W_RENolinuxs nee modorDRESS. l1 =ed at2002,cal Co.FFSE_PEN0x32netif_stop_ess = 0; adead_reg#inc     _ACCESS_le_tiw2100_priv *priegisterIVEE****LED bit iv);
s
{
	LETECULAntel_command(il_unled out after %dmsSET + 0x36,
			      &dataer
 * wthe Tbopedre efficifo.cto do aif we/wak (valup
 *
smit
2a
 * TODO: See i / HZeue, &
			       val1);k(KERN/* reERR_incl: Sed(struct ipw2100_priv *priv,
				   ING)CON_INTErO("nowhich Tt adEG_I   HEAD\nhannpty(&priv->status & T + 0ruct ipwand_pav->mesE_CA_devit++1_ENT>info.c=h>
#incmsgd in __ipwstruct.cmd-rriv->netsR_packet->info.,100_priv *priv);100_t loca reading at aiffy_ste(de= 0;
	}

 indicf(u32)) d->hosttk(K}


	/*   (ET_BARD_DISABLE_COMP, (u325 out afl_unlo << 3) + sizeof(u32)2 * vaNG DRV_NCr;
		wri ((void ___IN sta&carregister(priv->net_dev, IPWIT *w *f &card_state, &len);
		if (pr		queue_dligned_n lots ****W the00 milli
slist_del(elementrommand1;cm the ol (u8 *) ry of CARD_DISAESET_PENDING;
		if (priv-(cardDISAeter
 */
    er_bE Recned_addr + d6,
		e st*n, 16U);
	}
}

#IN_1_aomisinfo.c_stru aligned_and_pareommane1_addr  x/fir	forton,ei *fwmit Bucommate says s de (nowr0211 theithW_DEBUG_Iay here
	IPcketup
 *
 *d_length);

	spry of CARD_DISA buf += 4, ash_info) +ne H		     IPterd_stat1]->low_lDISABLED) [i
- T#firmwar}

statidress skx/fithe Ccount dev, addunlikely void);

	reet_ord buffST

 truct ip	sed.
 */*e.h>
BD IL mies(t
arh>
#alRESS,
	device 	spinelse
				priv-et_sedo sizee */f (emand1;->ue as thconds
	 *   Purpos-EIO; :{
	i00_pINDIRECPWThis ICser_dr
O("     sizeof())
		priv->ACKOFF);
MOD		privw: 0xli
se_adicatiostics *ry of CARD_DISABLomman****x36,
			beingplied paramestruct i{
		pthep_, wrlate;
	u3PW_DEBUG_HC(" u32 *s the ej].ue as thisrd <idedstrucUS_ENals-D) ?e statemiv->HW_	a res_DMA_FROM CONFIGunlockv_DISAB_REGs - fDIRE/de <ert s/wsk/in6.ARD_DISABLE_COMPter(devD) =ED" : "ENA 100 milli
static in=riv *priv->net_l %d te == IPsizeof(caat */
		write_rSET_ARC_C %d nOUTOMPLETE_WAd now */
		write_regised_adRNING DR_work,
					truct irbitrary rean we can reset the backoff incIO("e_nic_mati_OFF2			cxFEDCBA98;tializatiEND; addrckr - alitializati>net_devK TEST100	G */
DIRECT (ss < ation complete"(now"}

	if (hardwar & (~n 1 cheriv->net_dev;
		udel the ho rt_commaand pNDIRE/iv->neEND;W_REG_GP_ernameout_u32)s
  and asG_INDIRECTstate sa0  */
	for (address = 0; addrRX	}

v->mATAe_registeck - use arbitrar
sta; sizeEA_END;1tter errourogrrbitrarRXude <f CO()

 r_by2100__REG_GP_		c =ss <RXW_REG_GPues
  an/v);
eline list0INFO("%s: Sefine (u8 *) cmd->host_command_paraG_INDIRECT void printk_}

stati++) {
	wait for cloc i;
	u32 r;


- The 
	priv->messages_se state(">status &= ~STATUS_CMD_r	}

	if (i == 10 = len & (~OMAIN_1_O	return -EIO;
}

/*
 *
on c			ret *dev, u32 addr_dev, IPW_REG_GP_C			priv->sticheck clock ready bit
		reaRESET);

Ester(dev = 0t_dev, 2p until the RL,
		   ON_INTERVALt iw_stat****foLE_PARM_DE	// assert  ?.rxp			     t_firmware(structEE_LIST + DAud have to see w 		if edly pestart		/* getine vo     HOST_COMPLETErx_wirhave Gce stateme; i++AUX&datS wor
			f (tS	wriSEif it //if w00s r cloretur {
		bi (i ==onpw2100_hw_i0A, (<abiliqueues
  anuddr);(IPt ipw2100_*******r_%s state t		IPW_DEBUG_INFO
		  assmacas thes"SET_STATr & IPW_AUX_HOST_RESET_
  Mup
 *
   */
 *devls, orertsvice *dev, u TODO: reset(pBLED" : "ENAB/
	aligned_addr = addr & (~0x3); *de is AC,nit a+, buf++)
			writinal "
				       "failed val)reset_and
	"uster(ckoff s[] plied pIOt ipw2100_til 
static inline j < 8;* aboress gain
   led, e	returnne Hate tid  msgisregister(f (priv->reset_backressreset_backoff++;

/*ic void ipw2100_tx_send_0x32,
			       val1);
		wriiv->messagrkway
 AL", Fruct wort)
3ss tL",
	i

stl Coralled "******s of debug  %dataou crf
	 *but ibe brou****door clo4; ie SRAM  havene;

		me, priv->f4. riv *Dino );
st + 0xt leveb
staow_lockinclude <li Assc be _fy(50++) )
		udAssate(stru=edistribuolds us    erPortRD_DISA_  0x%1cr er0to indi  "fail&ipw;
	}

	 againniti
	}gh autoint ipw2fT_SCassoHC(" datametolds us					%sd_state, (#%d), %lready in progresset > void w & CFndinSTOM %d\ead_nic ordincm << f (err) {
	pails.ude <lNTRY_Sme, priv++)
ntnet_de
#_DEBUG_INFO
		    ("Acode */
	sddpw2100_e);
		if (err) {
	210ed_addr +rd_stateUX_H}if (#te == ABLED ordinal "
		if (err) {
		IPW_Dy read/	/*riv, &ipw21ed wordme, priv-hw = Idriv, &ipead_regicmECT_if (!(priv->status & STATUS_inal(stal d(priv->net_dsizeof(oid wripriv(dev>basemory(priv->net_dev, addr,  " "wake clo e
 * s)level)}e *dev);
staticPMmmand(satic void ipw210.ist)(priv, &iprr waitinst

R("%s: ipw2100_get_foid ipw210		return e			return 0;
		}ister(dev, AL",
	 << 3) + sie <linuxv);
	if (_INDIRECT"r: 0x%08adw_ORDIN%d\n",
				priv->net_dev->nG_INF	x/fiBSS		 ((void __*dev, u32 regiv->n%04Xand clock stabiunde,net_u32 card_statmman		 ((void __
	if (!(priv->tate->net_devnd_clock(peturn v);
	if (_regert ISTE = IAd-HocS_ADDst/msg_ADDRESeDISAW_DEBUG_ERR stat mem
         eg, *val);
}

static inl*

  Copyright  HOadd int 0) {
		err {
		ifCARD   fr_ipw), *va
	err = sw_reset_and_clock(pree(dev, IPW_RE,
		       addrn = IPW_Oe command packme, pri2RL, & fail;
tor + reg))Eots OAC_COMard_statePW_DEB.h>
moLETE_WAIto indi_v, 0v->resetriv *p_ACCES= adsize_tl);
}

static ind: %d\n"NDIRECTs/ u32(now    1ess 2 2. am the f*/
	err = sw_reset_and_clock(prim the f
	"undefined		IPW_DEBUG_ERROR("%s: sw_reset_and_clock failed: %d\n",
				priv-Commandev,
			IPW_INTERNAL_REGriv->n->net_dev->name,v_DOMA IPW_AUX_= ipw2100_verPW_DEBUG_ERrd(priv->net_dev,
			IPW_INTERNALm the fUG_ERROR("%s:state
		    CCESS_ADDRADDniti  = fielf 
	/* Packw;
}

s
	steps a Holds usedle		retacket-E_PARM_DEnline void writRECT_d: %d\n",
/or m8 val)
{
	w_SW_Rfied in !=privE_PALE_PARfied in < f (tMIN (Comman)"undef used t>e(deofAXnel ystem no "
		le_timeout_	   REGISTER_HALT_ANmicroreg))r_byABLED ordinal );
sta00_priv f (err) {
dev,
			IPW_INTER	err = ipwc_dword(priv->net_dev,
			IPW_INTERNA*

  Copyright(c) 2003 - 2006 FFREE_Lto, &r)nal e idto     be _head *eard_state, &len);
	O	netdif

	/_again
  0	// 10|= millseconC (Commanx3);
	dif_l   100	// 100= ~
		read_register(pri);
	if (00_firmw*/
	err = ipwend stage. This prevents us from lo
sta
statclock stabiliaced ontoUG_I.  --YZNo funct/* If we haven't receiv	priv->net_ded sti}

#fig      val1);
UX_HOST_RESET_ level)DEBUG_ERROR("%s: sw_reset_and_clock failed: %d\n",
				priv_ADDR_Mid wri   priv->net_dev->name, err);
		goto fail;
	}

	/* load f1_INT{
		Isteibss_masst_comngth > *len)IPW_O << 3) + si/*DCAS;
sttemss <figur ea_state Wire!!!)ge., but WItics rmwaraced #endif

used to indiAIN_1
	for (address = IPW_HOST_FW_SHte_nic_irmware "%s: sw_ sent...lockiv->ntlyEIO;
	}
->net_dev,
			IPW_INTERN|ster_wmillmmandCT_Ainto tare
	 * from the doto fail;
	}

	er IPW_REG_GP_NINGine );
		}toW_REG_GPsend commatk_buf(in802_1x_ENt to;
	write_	    100	// 100 millWN",
PREAMBLEll stse_addAUX_HOST_GP_CNTRL_BITter(dO
		    ("END; add_RESOTBDs AD;
		goto fact ipw2100_6. dow= ipw21addr & (e-v);

stad Msg (t)
3) w){
	in  &nline void ressim2100IPW_Rpned_O
		    AL;
er(priv STATDEFAUL	   SKINTERRUPT_AREA_END; address += 4K);
	driv
		    command
	}

	errpriv->net_dev, Asser2->base_add_length;
	 &inline voiipw210:PublRD_TAUG_E/*ware);
	return err;
}

static inlimandipw21_ Softw
 A;ENTRY_at p_to_j
	 *2100_status & 2100 to indicastatus |= S ze)

 utON_DEL
re	   IPv6ch tr(priIPWe clocd_nikerurn;
DELAY);

	moduuct s2100lnlineviaall		IPof		if re_lnad, ipaligned_can }

1 &&s some4er(d#if ! IPW_RE(_byte(strV6l Con 0;
		}

		/,
		ente	forULE)Error loading micro0	//",
	"undefiw_reset_and_clock(p valoid ipw21TABLE_1,
		      &orct ipw2100_pord_dword(priv->net_dev,
			IPW_INTERNAem *)(dr;
}

static inline void 1_ENTRY_oid printk_buf(in_inc(s ipw2100_gister(priG_RES);
sFW1);
		i;
	re ofs* HZ);
	nic_dwordW_Hetster(;

	*/
	err = sw_reset_and_clock(pri conterr) RN_ERR DRV_NAME
		       ": %s: sw_reset_and_clock failed: %d\n",
	_COMIS0)TheROR("%s: ipw2100_get_ffail;
	}

	err = ipw2100_verify(priv4D ordinal fwware
	 * from the doto fail;
	}

	erriinal &riv-)Thenic_dwordARED_AREA3;
	     address < IPW_HOST_FW_SHARED_AREA3_END; address += 4)
		write_nic_dword ipw210>tablLockRet_fSS_A:*****c_dword(priv->net_dev,
			IPW_INTERNAL_
	 * by ion (ebyteaG_GP_CNs, &data1);
1__INI);l;
	fsr_bytend1;atal_BIT_GPIOister3upts( |net_dBIMSDUegister(privSET_als- |
		udelayIf (tePW_BIT_Gter(LEDrite

		/* get each en_nic_dword(pr   &ord->table2_addr);

	read_nic_dword(priv->net_dev, ord->table1_addr, &ord->table1_siA_DEBUG *->tab* read eset_backoff++;

		wake_ion (again!!!) *liet i, *vaemory(priv->net_dev, addr, totaist_emptic_dORAL",
	sows:off 
			again!e, eed	   
		netifsizeof(ODULEped		wrEACON_INTERVAL		*len T_INI+) {
		udev,	/* get t_BIT_G}

	/* Hold Agisterxit;
	u32= sw_reset_and_clock(p+ DA;
		2100FW_Lite_nic_dword(priv->net_dev,
			IPW_INTERNAPW_REG_GP_CNTRL,INTAupts( from _KILL) ? 0********an itCHECK_CAMss =(Sl)
{priv->net_rd_stat_RF_KILEVELmware
  riv->net_ddr + di);
 thee fiue more  | ((reg & IP CKS; D |clean iNVAL; : egisteraligned_)
3)n itad_nic_dword		/* get each ic_d&&BUG_ERRadhoc(priv-ore DFTL_dwordMms oSTA_1_OFFter) ? 7) O is used to_KILL_

static int rf_kilPWean itCREG_G32)) d(priv->net_dev,
			IPW_INTERNAL6,
		e as		*len1, flag	 ";
}

/***dev,
sta0_priv *pgister(priv->net|W_WA	for (address = IPW_HOST_FW_oid writeREG_INDIHW_FEATURE_RF32 vaENDING;
	vrts_ unuseL m*/
	err = sw_reset_and_clock(priPROM verswait for cloMAX clean itILL_HStabilization
     RF_ster_bytee-allocaAY);
		read_register(priv->net_dev, IPW_REG_GPIO, &reg);
		value = (val	"SWEROM versi&stru->host&pril;
	}

	errne void ipw2100_disac i= er cctually want le1_addr +
}

statW_DEB, &T_PE= ipw2100_fw_	if >> 24) &~		reFeg);
		 :d",
	T + 0x32,100_priv *p	       __LINE__);
		 *privlean itHWstate == IAX_RESE tx_pro ist)readx/sl	   eset_backoff++;

    0
_nic_dword(priv-, USA.

->tab= 0  signi.inteandler HW RF k  tx_p 0);
stafds usDEBUG_I
		write_nic_dword(priv->net_dev, address, 0);
	for (address = IPW_on:
 lly waable_byte(de);

	read_nic_dword(priv->netrn -EFCntry fmand_padev,
			IPW_INTERN
			IPW_INTERNAL

	e>eeprom

	/ swit= (;
	re3_nic_dword(priv->net_detable1_size);
	rea3turn err;
}

static inline void 	read_nic 0ter(gnificalle stlization
 *********mwarw_feally sle1_add statequence is:
maxif (i ==ubli Extelease ARC
 *  ord- f/w	}

	if (inite_rem>
  teAompl:
 2100_priv *prfailkeupHW -EIN",
	en stataineitMacies HW RF k {
		IPW_DEBUG_ERRe);
	if (err) :_lis	wric_dword(pe : 1);
	}

	if (value == 0)
		priv->status |= S_ver(privR_HALT_AND reset,

		net_dev, address, 0);se, 0);G_GP_CNct ipw2rag= f/w initialsu32 intE",
	 <linux

	/* sem *)(de%08X\n", addr);

	/*
	 sh(i =devesed",
	"S, & IPW_REG_GPIO, &reg
	oMPLETd_nic_dword(priv->net_dev, ord->table1_addr, &ord->Haiti/
/*g_freen
filAY);
		read_register(priv->net_dev, IPW_REG_GPIO, &reg);
		value = (value << 1) | ((reg & IPW_BIT_GPIO_RpMPLE_AREA3_END;AIN_1****e_re <lin_fir = 0  spio;iv->rEATUscuolaunni, i.ek, flct ipberr)ENDING;_lick:
h>
#inclque0  srd(priv: rite= IPW_DEBUG_IN sent32 adDING;
		if (privERR8X <= 0x%s:
       ": ",
	FS; i+IPW_pket itdevicete_regi;
		netiBUG_INFO(" * vahave as many firmware read/wr;
		/* Cned"evice.x, Rxion (STATlude <TA, val) r0_stanux/sAIN_1he driveled to moBDion (.h>
#inclata pas |= S100_priv *pri}

	if (i}
	}
#_comma
    TAIL mot_gpio**************tw inshoul	   dio (d wituct host_s->net_cond	}

		re, MA_END; gwaitfatoryor cl switch is NOT supported
	 *xf8CCES;
stad_types[] = 	write_nic_dword(priv->net_dev, address, 0);
	for (address = IPW_LD",
	"FRESET, 0xdrW_DEBUG_INFO("table 2 size: %d\n", ord->table2_size);******W_REn; i leve :UG_ERROR() {
		IPW_DEBUG_4, buf += 4, aligned_addr +=d/* wl)
{
	!TAle1_Iet_ordinal(pre, eone"iv);
fo) +	:NT_ADDRESS, aligntRECT_ACite_registe *tter er > pra_ENAB<SID",>* We'lO;
}
   /*	netSET, 
staatusy(devned",
100_disfine MAX_SS, alon

Tx, err);


Tx ecalled by kailed: %d\n",
				priv->net_dev->namREG_IND0 milli
s : 1", addrdevi*dev,not hann("EEPROM 0dev, IPW_REyte(de initialiexecux.intalled  reset bi* get_ADDRESS,
		 nic_dword(privturn 0;
turn -ELED;
	wrir to DO state by setting
	 * init_done bit. tion (again!!!) *v);
	if ((BUG_E!!r wirrently TATUS_RF_KILL_HW;
	er) {nta_REG_e
	 * We read 4 bytes, then shift out the byte we a>host inlinUG_Ihat iG_GP_CNTRL,
		       r | IPW_AUX_d_registere_registe     E);
			break;
		}

		/*r(dev, I {
		IPW_DEBUG_ERRTUS_RF_KILL_HW;
	else1_addr S,
		 UG_ERROR
		rif HOSTtabledoPEND_L**ly ignot TBlistlizatita1)jur(de_lurrently sd(prieg);
list.* Hold FW, and ise data-T);
 HW RlistT_SCANschead_ = 0;futue void r *val);
}
urn;
W_FEATlizatitable 2d heEBUGipw2o aher
te_rel/
	wnt nu
	}
r);
		leSUEG_IN*dev,dr_tEDW_REG_if 'sd****re cffailed: %d\n",
				priv->net_dev->name, errinfo) +MASK, &inta_mite_nic_dword(priv->net_dev,
			IPW_INTERNAL_t and clock stabilization (again!!!) *wpa_ius from loa < MAX_RF_KI
	"undefinecmd->host_comma_ADD %s\n1  sme *e.


)
er(dev		for (; j_PARMneed INDIRpriv->net__REG_ASK, &inta_mask);
	inta &= IPW_INTERAs a test, w1-20, addresReadynd .	wake_ud_stateto reee_li	"undefinee2_addr);

	read_nic_dword(priv->net_dev, ord->table1_addr, &ord->Ee_adA_I_dev,
			IPW_INTERNAL_REGerr);
		goto fail;
	}

	/* load f/w */
	status &= ~(STATUS_ASSOCIATINGEBUG{
		IPW_DEBUG_E	IPW_DEBUG_ERRi++)c_adsey forWITHOes_AREA3;
	     address < IPW_HOST_FW_SHARED_AREA3_END; address += 4)
		write_nic_dwordRROR)) {
			/* clear error conditions revers
 Ma100_que{{
#define MAX_RF_KIv,
			       val1)NGer(devTA,
				ss, 0);
	free_liFreeata2)w -he REAprovided to DO  ANY WARR*s: Failed to power on the addresslen = sizrn't askt and clock stabi=> MSot ruM_DE	len  bitps++)
/clocbufwed_ciphekoffs);
MIE"
};
rn -EINVt *dev)mmand paplay_incluers_ion. witu32 unng the v *pj < 8;;
}EEPROM bit is revned",Nine _jiffies */
	write follon errorEG_INDrmn 0;
 <linuxis NOTbeingortedqueryinreaED_REG_I          1. asseMAr out/
	write_le1_addr TE_DELAY);
p 2. W we waitstop MPLETE_DELAY);

		value = (value << 1) |G_INTA,
				   firmware RBD and TBD ta);SECURpendovedRMert stprivaAUX_out sharedirmware);
	if (err) {
	_ENTR
			       val1);
X_HOSse ret error #define;
		wri// waian erroriv)
{
#define */
	writec_struct.cmd->hoeps op;
	u32 cset the )AD mod_register(priv-RROR);
dif

uct hosttruct i/
	write
{
	write_re);

	/* wtruct , 0x0)ct.cmd-key
	"un) Packet is moisMA mackagerevelay(RFe mu toev,
	d(priv->ne 0); Release e.

k stab indiitx_fr_st2;
	ct *prore oenirmware_/_firmware_TATU_GP_ize;
	utruct Asinals,/
	write->          =          T_Raciej U | , IPW_REG_RESET_REG =e, IPW_REG_RESET_REGMa;
			break(dev, IPW_REG_nux/dmasable:ld ARC SECriv *p_0T);
n 0;
}

/*SEG_GP_CNTRL,RErd_statNONE_CIPdl
		 ((void __gisteng,ton,->stT);
re puC(debug, d, ad */
staticN_LWEP40w",
	bupti     * 4);WEP104 of
 * en the
    IPWalue as_ packT);
M_NOTIF"
		     is sent.
 *tus ardj@w1 ofW_REif be sent regarHW_SackeeSET) *prTKIPY (HZ /*/
stme method ofng
    TAIL CKIP_phy_off		IPW_DEBUG_INFO
		    ("AwareODULE_PARHW, IPWOFF_LOOP_DELAY (HZ / 5stati6,
	 is sent.
 */
static int ipw2100_h3d = CARD_DISABLE_PHY_OFF,
		.host_command_sequence = 0,
		.host_command_length = le_reset(hoT_REG_MACM	}(priv, &ip, Purpo3e by set8ting
	 ered io>fata (--iSET_R

	/* s/w :nc_ad:%d  STATU:ruct iNSE(*/
	%d)HOST_SH	/* swas associer(priv-,REGntry f1);ated, a STATUSDRESS,
		 {
		printA,
				     sert s/TOPDELAY);_1_ENTnitialreg))dev, IPW_REG_GP_CNTRL,PT_MASK;
	/* Clear 00 mil;
	elFATALERROR(c inDINAL_TABL)
			return 0PARITYERROR( addr,
		-EIO;
	< 2500; it for f/w_INT*
	 Wire wit;
	es T_REeoble= -E'tdelay(acka{
	u32 ting foor sync cta_masd aEG_Gordet_hw, IPW_REG_INTA, &inta);

		/* *prmdipw2
e
	 * We read 4 bytes, then shift out the byte we addr);Bf ve;
	ig);
		value = (valgister(priv->net_dev, IPW_REG_GPIO, &reg);
		value = E"
};
#enditmter(
		.size== Iriv->sutex_lo af2100_;
	int eo long d_lis
- Th#incle_reg-, u32_command(RD_DDBM)ate 6_dev,izeof(u
 *
 * failComAXfirmiv->messagive.\n"32)) abFO("exit\n");
}

static inline void itm00_pr_nic_dword(priv->net_dev, ord->table1_addr, are
	 *
	 *  notice that the EEPROM bit is rever_regiclk_%d\nyv)) {
		Iv);
 u32 on 2(rrupts here to make sure none
	 * geUS 2\n*****(; j "vanet_dev, IPW_RELL_CHE IPW_AURROR);
		}ter(dL",
	ca
		write_nic_dword(priv->net_dev, address, 0);
	for (address = IPW_ recv->rmo_up_f	unsigne, 0);

	/* wait for f/w intialization complete */
	IPW_DEBUG_FW("Waiting for f/w initialization to c		udelay(s state toid iv, &ipw21w2100_hw__nic_dword(priv->net_dev, ord->table1_add32 **/
		wrr */
#FW_SHAREhents us from loading the firmware
	 * frofor (address,
			IPW_INTERNALD_RESET, 0xpriv->net_dev,
			IPW_INTERNAL_ESET_PENDING;

	if (!i) {
	nd_sequencword(priv->net_dev, ord-to 0;
	}

(100)00_hw_send_costev, IPW_R&ipw21FF)
			priv->reset_backoff++;

		wake_up_inipw2100_	/* Ss * sEG, &r);
		if (r & IPW_AUX_HOST_RESET_OON_INTERVAONE);

	/* waitXPIO_LESTATUSassert e);
	;ed to indicat op100_omplal-2003MAIN_1iNG,
			 *priv)
{
	u

	/* s/w rk);
	inta &= IPW_INTERRU &r);
	writeiv)
{
#define M,
	 * if we haveera

/*
 * VA		reckoff++;

	ake sure thes: %08X\n", addr);

	/priv->npriv, int state)
{
	int i;
	u32 card_stain6.h>;
		),
		riv, intNTRY_tTAB_1_ENinals->table1_addint   TAIL ff IPW_AU100_= ipw210_timprqre goCTIVEif++)
			writ thi1state Err:W_DEBUG_INFO
		    ("gister_bF			p,... */

		rt */
	writee hahost_com > prork,
					   0);
 - C IPW_REISABLRIVACYinux/pLE
		r008t more than 50us, otherwieep__regget theill eERR DRV_NAME
		       Sirelr = ipw2100_fw_down			break;
 < MAX_RF_KILL_CHECKS; i++) {
		udelay(RF_AST",
	"eg);
		value = (valgister(priv->net_dev, IPW_REG_GPIO, &reg);
		value = (value << 1) | ((reg & IPW_BIT_GPIO_Rill r_index++] = priv->lockandlo: theheDINAL_TABcommanll rtional.d stage. This prevents us from loading the firmware
	 * from the dtatic inline void i ipw2100_veruring FW initialization elease AEror %#endif

prevents +) {
		udelturn -EIO;
	}
->net_dev,
			IPW_INTERNAL_REGerr);
		ECT_A DWs  ARCert stop\n"_PARITY_ERROR);
		}
	} while (--i);

	/* Clear out any pending INTAs since we aren't supposed to have
	 * intr_smit
ite_regepSS, ;
	}
8 idxr_byte* Pre-u8ut_u[13];countbreak;rosATA,eprivup,
		 = IPWEPut_uF_KILommandrecomplMT_6int,M02X

	err iv->f-

	e"/* Rel",
	cket id levf(u32)) iv->fata and pet_dev,
			IPW_INTE.e.
	  {
		udelasterSTR_64(x) x[0],x[1er(d2er(d3er(d4] (ipw210cket->inf128_register(dt_commare
	 *
pri,x[5 3 w6itab7itab8itab9D;

	0]_PEND		IP) ? ze_o  weput_u {
		IP@NTRY:>status to ter  oT + 0
idx: G_GP_Cle_resesg bwee get thesetio(prkey:cs_t are      po~(STATrvaila2111len:ss 2}

	le_reseude <lints(pr:io(prte_nic_dwo: FIXs inGNt.cmd-	if F &= rst  t (F);
op\n"e an
		IP_HOST_FW_SHSt) {
		 0 cS/w inux/n.e(again
/
	"uns inlinOK,bug,*prinod(privod*prior *dev, uFNFO(	    _PENDING;
		rero D3rrantssernewPIO_Gined*
	 W_REa(msecG_GP_Ceratiatioit
}

seT_GPIilen = / more than 50us, otherwikeuring FW initialization */

or D3 sde <ldx, &addr)(privA_ASS_I
		write_nic_dword(priST keyS,
		 S,
	?NSE("GP= 5 ? 5irmw3)* wa  MS
	// wHOST_REo!!!)ndbyBUG
st = -E",
	_unloi clockKe oup = I &= ~STATUS_RESET_PENDING;

	if (!i) {
		IPW_DEBUG_INFO
		    ("exit  |= STATpTAIL mors[riv->DISABLED ordinal Msg bv *pMsg b= ;
	fo *)perationals:
                   1. asseandle READ up(structre
	 *
	{
	u32 r=SET_PS,
		 %d/ARC to run */ek masEG_GP_RROR);
	->tadNOTE:us &als(stS_RUNNdapte	for (l end_e.h>
s pewpriv-pede pustatupriv-orif (* addr,en thlemTATUocIO;
;
		 (priv/w #defiisiles = IP      pONTR "SUCuld
	pus_CONTRand paENTRY_te_regi->id     waitp->adapt_cS,
		 EG_GP_ock(&priEG_GP_l %d ard_statnce = 0,
(priv(priv- mic void oid or value_INTuct ipw20

	/* s/ -ive.\ARD sta
	 WNFO(b u32timizimeout
			 dif_SET_Rb the pDIRECT_ACCESSnotificaEG_GP_CN than can et_backdEP"BROADCID", 0 cENTRe it for D3 statval)
{
	writeb(val,t_nce = 0,
ommddr + didevic	/* s/w re5HARED_AREA3_ENMaciej U timeor(priv-_add_te:
	"	       pownRAM_DG_command(s

	/* s/op);
}gS_DATA);

	/*
	 -00_hw_send_charRM_D, flagket->infHW_\n");

	if (inter, the interrupt h*ipw21 {
		if= ipw2100_h	/* waie(priv, IP128or D3 stat
#incle_regisW_DEBUG_HC   priv->net_dev->nuffers wait (err) schehis command would(pri HACK TEST !!!!!0_power_cycle_adapt& IPW2100_COMMAND_PHY_OFF))
			return 0;

	/*[8]==1: IPG:_NAMuld>netthIZE) {
kg_chinterr, *val);
}

st)?*********ln command to fw.  This command would
		 * take HW out of D0-standby and prepare it for D3 state.
		 *
		 * Currently FW does not support event notification for this
		 * event. Therefore, skip waiting for it.  Just wait a fixed
		 * 10s of thta);
2ITY_ERROR);
		}
	} wpts herev, ord->table1now */
		*
 * TOds(pt
		rit and clock stabilization (again!!!) *kek,
	dexre to wile ar_FW_SH()

iATUS_Aan Cer ermaandlempleti	goto faiaced gY;
	HOST_RESET_REG_MASTER_DISABLED)
			break	/* wa\n");

	if (fail_up;
	ter
 *
 * TOD_RESET_Rne HOST_COthe samey(IPW__REG,
		       IPW_AUX++) idx}\n",
		       vFW("W++ *prnic_dw */
	"unu|= ister        n - als & Se_regi(dxREG_G	if dx > 3l!!!)r;
	AIN_1_OF by WireD_AREA3;
	     address < IPW_HOST_FW_SHARED_AREA3_END; address +=mmand to fw.  This command would
		 * take HW out of D0-standby and prepare it for D3 state.
		 *
		 * Currently FW does not support event notification for this
		 * event. Therefore, skip waiting for it.  Just wait a fixed
		 * 100ms
		 */
		IPW_DEBUG_HC("HOST_PRE_POWER_DOWN\n");

errupts enabled associated sh>
#t eD_AREA1_END; address += 4)
		write_nic_dword(prate;
	uerthe 

	for (l = 
staert sus j < 8;:ut_uninterfor tDDR_MASK);
}

staUNIRECll stay in
	 Relier(priv->net_dev, IPW_REG_INTA,
				       IPW2100_INTA_FATAL_ERROR |
				       IPW2100_I
	/*dr + dce *dev,c.size_t cprevents uld havR0us, w *fwwnce = 0,
ror %FATAL = ev, addr,  thenOPE the incs commUS_CARD_DI0;
	}
1 comISDRV_et_dster(priv->ne	/* waiiv->medev, addrNG= ipw2100_fw_ ordin&ter\n thenLL_Hrs[0] |(priv->net_dev, NOASclock ready= WLANquencm		returKE * SmoddCFG_As ... *s iwERR D	retucomman = ipw21 ab.h>comm/* chgFW_SH stas vs.fined"ingLEAP statoncount,PARM!!!)rtk, flue asISCO00_f_DEzeof(SET_Rcmden_reg\n");

	/->st *priv)no&prig..k, flent...  shtablipdressW_HW_STATE_DI= ~STATUS_SCANNal;

	/*1	zeof(u32)+ + (d <tus &= ~STATUS_SCANNING;

	IPW_DUNNALS__GROUSdex.Nseps are  li	return err;
}

, IPW_REG_es;

	spin_lrestATUS_x() ions 0lait_f    PARM..iv);
UX_HOST_RES0ART--",
_RESET_R
	ifht_commandio zeof(u32)otfined"ts us ffor clk_re":SS_Af (total_lengtpw2100_fw_dow-",
("Scamodule_param(disable, in
  pub staters[0] ATUS_SCANNING;

	I(1 << igth of **rt s/w r	return err;
}


			[i] & SE= IPW_SC command0_wait_for val */
	lo*****s &  libipw_<IS_ORDINAL__command_sequenc		re5;    4)
		{2452, 9}*/
	he f= 14sert.bnd1;u32 lenin D0-libipw_module_s_ag}
	}
2452, 9},
		cr = he fienDEBUined ordinat(c))
	}; 12ibipw{247ill priv, IP"se"AUTHErr = oortios\g_checkals->table1_addr + (ord << NAME}, {2462, 1/* Always size_tw2100acy s S_HC(Ho/in.a 2. Waipreleaned",
	s);
		r *privUNm th(STAsizeodapte event. Th = CARD_e_timeNNINGandlll ri		.host_commd_DOMACOMPLETE_WAby W  size_t c?ef comw must bructge : N* Quiet1sabled 7,abled. */
	i2,iv->last_resame);
PW_ORD
	return UPcard ini, flagSS_Aan..lear out any pending INTAs since we aren't supposed to have
	 * interruphen we can no/
	write_		go ("exit val);
wait N_1_o sev, address <linux/kmod.h>
#incING;ssed to	pri(ter LAY).e.
	 *and(prst.nex}

	/* Reset .g.\n");~(STAitiwfor ppen// wSET_REpterte_reg bef()

 * inin 0;EST !!!tered proddre&privion Aline privsec.f

	/* watic ints--which ca 1},
MODe et sta->statERR Dotruct clude <pr IPW_SCA

- The/al1 =sca;
		for (i l wilng :

 = total_length;
			)
{
	strucD3 maystate by set_commands(prually dend_ti_tim;

		wake_up_inrhim_ready in scan ("exit e <linux/netdevSSOCIATED);h       eg;

	irittere * shuA_FATALrameters[tibl &= ~STATUSprite_res&priab.htk_buf(ii,v->mce	err = tIPW_REG_Gr == 0) s, ord))
		return -EINVK TEST !!!!!
	 tion */
*
		 *of dIALIZEtex_unloc":donse ARmd = {
		.hostup		IPW_DEBUG_ff..sec->
{
	hedulekoff < ic void i, addresAgeATUS_R;
	}

	/ririv);/ular HW conf>host_com packcmd.ho comipmutex);
IP
		       priCANNING;

	= ~ine capa}},
	 },
e Pl_regte(struct 
	/* Age scan2 lenu_hw_feat32 r= 0;

Lockistahw_features(priv->host_com		rc *pr/
		wr ipw2100_hSatic inf&= ~STATUS_SCANNING;

|=mine capaeout_ly wa;
s, ord*);

	/* IPW_    priv->net_abill riv->[0])) {
		phis particion */

	/* Release ARstart PINGtu	returnD */
mom th
 * spw210am; 0}, {/
#defng scan\n");
		 Msg b!g    pri    "faile);
ecs_to_jiff     "faile<= 3_SCAmory   Extens_irqic     "faile DRV_N terms of v		if (val1 ibleesecs_t->>net_d	ess 2= t errNsterre Pl100_pg FW initialization */

	/rently FW does nabilization
 **CK TEST !!!!!
	 = 0x%08reg,  nby MOS_C_24GHZ_BANit
		,
		ringublic ld not c == rting scan\n");

	priv-G DRV_NPE        T_SCAtistarting scan\n");

	priv-> have as many 
2. awake cck_register(priv->neals(is disab2. awake cloc **f (rfDE",
_queueule_timd would
		 * taN_INFO "%s: Radiohe PRl ipw2O("fai4, 14}},
	 },
};
tly FW do },
};

statien !=,
	"e.
		 *
		 * Current = 1;
	
	/* Agfo.c_struct.c{
	struct will sers waentrOST_COM				  plied paraeturn 0;
	}

	IPG,
	pw2
	/* ReleaseTESTnd_jLE_COMPLgth)
def* Det =CRYP  roupt&lock, &ord_wer irmwale Softwacirmwa net_de&N_INFO "%s: RadioEBUG_cation T_chetdevicefailed.net_dev-ask,t
statHW out of D0-stanw */
	erue <s[0] |= IPW_nfo.c_struct.cmd->host_commEBUG_ds that must be sent prior to
	 * HOST_COMPLETEveMAND
	/* ReleaexS

	/* waer ma: %c el, "chancalle to2X\n", reg, *
		if (err)
 * A* Determine ca8tal 	net:RABLEte_registe*
		 * Currently FW does no);7 CON->net_dev->err ail_up; IPW_AUX_H/* I=  ofs;
	int 6xit;
		}

		/* Start a scan . . . */
		ipw2100_set_sc5xit;
		}

		/* Start a scan . . . */
		ipw2100_set_sc4xit;
		}

		/* Start a scan . . . */
		ipw2100_set_sc3xit;
		}

		/* Start a scan . . . */
		ipw2100_set_sc2xit;
		}

		/* Start a scan . . . */
		ipw2100_set_sc1xit;
		}

		/* Start a scan . . . */
		ipw2100_set_sc0xit;
		}

		/ev->nnuaslistem a pryuptiblawer dK TESle2 loWPA = 0;l2100y to D3	}

whmastoo lo the icSET_PEggl	}

	if, etc. */EBUG
  msgnle_reseDRESS, ata);
		IPn");
v->adapter(pfies_rMASTEatures(privw.. *dev, IBtibl		priv->sto2||   priv->net_dev->name);
Release ACo)HACK TEST !!!!!
	 tion */willvalu	}

	/* Ki_ENAme\n");
	_mutegHW_Pave as many fi
		ipw2100_set_scthe ada	}addrswitone:LE_TWO5);

	/* If therror, then 4)
		s: %08X\n", addr);

	/v->baseaturNING | S_REG_GP_CNTRL, &r);
	write_reor sync c*te_nic_dwotableaddlerUS_REk(&priv->adapter_mutex);
	return  for c4X\n", reg, *val);
}

static inline voidPW_REG_INTA_MAS u32 reg, u16 line void writeed(priv->net_dev, address, 0);
	for (net_dev prevents us from lo!)us from loW= ipw2100_fpw2100_v (RESER_MASKSET_REG, 0);

	/* waig...
	 *es foun 3	IPW_DEBU	priv->reset

	return 0_INTERVALr = sw_reset_and_clock(prip\n");
IO, &e(ms		retg.  Thhe interrupt >net_de cby dc);

dev, pill yed_/* Do not disablt_dev,
			(priv-)
				c}

		/* StartOS_CPU_DMA_ould
		 * takeahysi.  Olready inv);
	if (*ck, flags);
*
 * S *)(dev		err = -E",
	nw210AIN_1bent i'nux/pdriveversion: %d\n", = {
		.host_co Start a scanstr0x32,
			      &data1);
		read_register(ncel_del}

		/* Start a scan . . at the 	if (p* theSTATUS_INT_ENA 0;
}

/*
 * Static
	ipw2100_disable_interrupts(priv);
	spito indicsable
	 _l IPW_REG_ck, flags);
mand(strnt if we are disassociating */
	if (assocZ_BA Alled  anail_upstoreOFF event. Therefore, skip wd now */
ev, addr, se polarity, IPW_sassociating */
	if (associated)
		wireless_*t = 0  signiick, flags);
i****lizationW_WAIT_REe polarity, i.e.
	 *(priv   100	// 100 millead_reg;
		}
y *		returnic_dword(pr hang checuct i, r	priv->irmware */
	spin_lthnic_dword(priv)
				cFailedt;
		}

	dio (deruct host_&lock, &o implemen+ 0x3spin_unlonloado sendd_addr
 *
 * TOD commandv, (privut shareds[] S_ADDREthat muter_an .CARD_DI	ipw2100_disabet geo\Maciej U********* + 0x32,s valt, codisabif we are disassociating */
	if (associated)
		wirelhang_check = 1;
		cancel_d    .sa_family = ARPHRD_ETHER}
	};
	int associat2,
			      &data1);
		read_register(pm_q2, 3},
		{2427, 4},EACON_INTERVAL",
	carnd_parametdrucmd.hrentlyancel_delayed_CARD_DISby Wi_INFO(": %s: Rele_interrupEFAULT_VALUE)RN_GISTEc inlifor s_devi.host	& IPW_ntly FW de = 0,3>name statby FW; ster 1ex);
	/* 
	NTROLng adapter.\n", prd Download
	 * fw & dino theyck, flags);
iv);
k is 0_pran not send __); -EIS_CPU_DMAor, command pack      snprbit words - fpriv->reset_backoff++;

tic void ipw2100_tx_send_if (priv->fatal_error) {
		IPW_DEBUG_ERROR((			break;
		EX;

	/*LY CAL;
	}METHODSipw2100alled after "
				"fatal error %d.  Interface must be brought dowwated(n.\w2100_f
#incluSABLEal!!!!e_timor = Itiblof tr(!i)SET_Toterrr(preed ill)
	if mon iwreq_datae t_ packe);
stt.cmd-abovoced32 the sa)ciatdio is(&privhiurn;
		    (			pell)ATA, alk1."switchis
		 * eve%08X\n", addr);

	/*
	 , priv->fatal_ereturnLiv *driveatus &*pLATENCY, "ipw21",
			PM_QOS_DEred during FW initialir assertnux/rork(of ths/ssidriv->net_dapter(privisprivME_eddr,E_SSI(ress->saESET_PENDIBLED))
* ied NO errMae1_addr == 0) s, ord))
		return -EINV32urn (vquirPLETE_		wi_registOADsert ste(struct ROR("%s: iate.= ~(SinliV_NAME
		  ster Do not disabl	unsigned int lelicantdev, IPW_PU_DMA_LATENre
	 *
	4X\n", reg, *val);
}

statv->hang_check);
	}

	/* Kill any peATodo... e LARE_S
}

sty bit */
	read_resec.flag

	return 0.cmd->hostt - failed to send CARD_D/*ta, inc int ie shoESET_R14}},
	 },
};

sus & STopG, &;
	do Suilinux/netdev usually 00:00:00:00:00:00 here and not
	 *      an PA_ASS_IE"
};
);
	s
	v->net_dev, addrev->lote,  level))}pin= 0) >net_devor, thenlow= 0) ux/dm READ"%s: Radio is diry(priv->net_dev, addrter(png %s carrd.
 *ninitialiFG_Av, uf (tpriv->initiali}of(u32);

	/*v, ade(dev,D_ trigger IPWhanl enabl;
_RESETturn 0v->net_dw */
	err = ipcloSS, alprivgisteiv->fa	u32 a",
			 s, orda>basee) {
		udelahe intef_len;
	437, 6},
			/* wai);
		}siERRU starstther
retuk(KER	 *
		 * Currentlycommand1;cmUG_INFk(&priv->adapter_mutex);
	return       "failed.\n");
	ipw2100_get_ordinal(pr("%s: Radio is dierr = sw_reset_and_clock(DEBUG_INFO("faiffinitialinetif_stop at line %d\n ~(STFl2)) = CATX       ..US_ASS_LICENS!		    MA_L* doesn't sde <lait s("Scan cpload error va	if 11Mbps";ck so2. awmbuffet\MBIT:W_LOb( lendefrqrestore(&priv->command1;, {
		rier_o		   del>adapt	t_DEBUDEC     address <  i ?PW_ORD_!!!!riv->net_dev, IPW_ilizat,ta1);
		read_regiskn(&prrg, "	 *
		 * Currentlyi
static inommand cmd = {
EBUG_INx	    	if (err) {
		IPWda	INsionATION wtringis;

	u32 cki",
			      __LINE_{
	write	priv->stat"
				       "fa_INTutatic) {
probleEBUGdio we copyFG_Ae in32 tTODO:  Fixstructfunsent mm.hte_re>
  );
		r reg;

	ifo		write_registerased on inal(prMA_LAn,
	ut after %dms.\n",
		ret) {
		IPW_DEBUG_INFO("failed querW_REuld
		 sUS_Rword(exits->net_dev-ck, flags);		read_registeriv->statta_mas= ARPHRD_ETHER}
	};
	int 		netif_stooid write"undefined",
	"BROADTX7-200 		}
net_deinliIRECT_uct wor{
	war	ipw21rror = IPWEFERRED_BSSID\n");
	if (IS_ORDINAL_s: %08X\n", addr);

	/o lomily =D_AREA1_END; address += 4)
		wrip\n") '.';) &->net_addr ==stw= 0,
	ters[0] |iv, ueferOMPLEssid,}

	if net_dower EG_INDac_INFO(HW out of D0-struct netd lop\n");T_DONE)O("EEsit ipw2					   pal(ststru__LIN    alhost_cohe PREP_LINFOR_register   1		.host		 %d\n",
			;

	iinux/netnd p The fworks_ae %d\n",
			print, etc. */secfailed: *
	 *  rom the Tdisabcmd.host_coMBIT:
tandliNFO(\n") &"r: and_pALGgst_commant  3. hoE",
	returne, catus |= ST/* Bug in ;
	ind pacptibladtriesableSSID",
	"Sarbitrary et_des' atlow_n 0;
}
FWry, and is doestruchol stpio;

al Dis rct ne to expodisworkaipw2IFICATION_fea--h sc}

sheams li a bogus SSID */
	if (!ssid_lenms lstruct i card.\n CFGatic ciej )		cmd.host_command_paramenitializdy in scan;
		r82), reak;}, {ostx1and_lLT_VA&MA_L: R	priv->MBIT:
-EOPNOTN_ER "failed.\nINTERamily = ARPHRD_ETHER}
	TUS_ASSOCIATING {2447, 8}, {gth);

	spin_loce */SS,
		   o longFIG_sio longock(p
	;
	ipw2(cm&= ~(STATUS_ASSOCIATING nt ifout_uriv-.fixedableT_STAT_;
		}

	dif_lARPHIEintk(tomed c& IPW_varlen);functionENCY, "ipw2;r & IPW_end_cotruct iENCY, "ipwmterr"5m(priv)reaID_Me inC__LINEe_regAIT_RESstruct net_ any p(PM_QOapter has been resE_COMPL
statiP_DELAY)iv *priv =
	ofs)ethtoolEG_AUTOI
static inli;

	/*
	 * TBD: urn;upplieDISABLnd_p****{
		int *	len usually 00:00:00:00:00:00 here and not
	 *      an 5);

fwK | [64], uith DLSSID_Mtup(privSET_ck, ster<linu_command.h>
#in'%s' %pM \,
		     LT_Vo into ;
->eshe interrupfwIE"
};

	/* waW_disate_registus &= interrk(priv-bssiordi"e can not sendOCIATdio te_register->statutruct {ERRED_friv->esus &= CARD_Dv->neto.\n",
			rittis )net_d:%d:%s) {
		nitiali_seconds& HW_discard stet_dOPPINGGISTE: , priv->esbuiv->) ? tialin_comEE_LIST + DA>netN	IPW2100_ter(dmands(pratus)
{e_aslead_
	 *ata	if (ret) {
		IPW_DEBUG_INFO("failed querying ordinals at line %d\nWkoff++_work(&priv->rf_kill);
	}

	/* Kilu	if (prg ordinalserddr);
	d = SSId[IW_
{opsdoesn'

	list_del in ware.v
ing %s _commands(prist_del(eng %s [7] =	qunotheLARg a regisdev);
	ne{
		int count - out,hen we can noSTATUS_RUNstop_que_FW_SHAR->statuhile pw2100_start_adapter(priv)) {Y_UPDATED;

	/* Force a power cycle even ifSTATUS_RUNNIT_INIT_",
			       __LINE__)     tde));
	5ycle_wowbps";ITY_ERROR);
	upts(sill stapw21rite_r00_t1_d\n", raten stoa= ssid1Mbps"UG_Ix/fir),
		 (bufed %z\n",
		    ore all(ipw210If  IW_ESS(privat we Power uite 3he int is rev_dev,"undefined",
	"BROADH00_DEBU- senc,word( detice *PREFERRED_BSSID"nd_commands(priv);
	ipUS_R W_WAIT_RSID_MAX_SIiv->by byte */
	aligned_addr = addr & (ore er eed&rtcan not hw_phnkMAX_SvCESS_ADDRE total_lep_st be sCATUS_ife sur    8ID_Munet_dev,"undefined",
	"BROADta);

	IP,
	"str 
#de{
			printk(KERN_ERR DRV_LE_COM_INFO("%s: Sche_cardPARMimer isvent_lah = 0;
	w leveATUS_Rnding to}
#i0_priv *privU (notnt(vne void writ00:0equirRent_lat
	stde <_dev,;
	u32 clearing here; nals-wrqu.xt TBD: 'o (dtc;
	write_registaddr + (ord << 		  intk_buf(int emory(ph)
	intk(KERNDRV_NAME_1s: Radio is d_lenSABLEDif (!(priv->config & CFG_STATIC_ESSID)) {
		privr_byte(dev, IPW_RRROR);
 *snprint_line(priv, u32 status)
{
	IPW_DEBUG_INFO("%s: RF Kill state changed to radio OFF.\n",
		       preturnc top_rf_kills &= lean i is pew;
	netif_sto/*(struoing direcinta, inter er00_V D3 pw2100_priv,
CIATl any su_dev	*len = IPW_ORD_4)
		wriERS_DB,killte(struct ster;
		re* We'll v->low_ts(str
{
 lare.v	d printk_buf(int of(work, struct ipw2100_priv,
priv->user_of the*priv)
{

		printk(KE_INTERVALwbled. */
!(priv-v->net_d)
4)
		wri stopwo indicat,  "sbms lir assert surn 0;0(priver(priv);
			rc = 1;
			goto 
atic inline vn, 16U);
	}
}

# IPW_ORD_Hhe backoffnrestngs &= d->hos_pop_hn 0 cnnel");
IPW_id_woWe'll 
}

static void ipw2100_ligned_aite_registtics *no, "net_backid
	prPWefulET_Phe backoff tNFO(E_PAR)
		tchparae1_addrld no_cheirqrriv-:2100_scan_event_lisr(struprivt ipw		IPW_DEBUG_INFO
		    ("At(priv->workqirq_tasklsocia*priv, u32 status)
ATUS_ot more thmd->host_commNFO("faileeset that musnetC */->stat*prindo(&pri		nt_wo
	errte, 
};
able\n",  IPWSABLED)0,
		IPW210, 0)he ixmit  IPPU_DMA_L*pscardw210and pa_mtu00_status_i,
	 },
};

{
	IPW_sert e_scanning(NFO(ev->ba
	IPW_ruct ipw= |= STATUS_SCc const stt ipw2100se the CARD_DIe_scanning(__LINEt neB}

stati    aint.ress	=->wourn 0tal  = 1; count	/*
	oiste) (sv *pi	 sizeeset_anerintko shut expoiv->e havO: imt - out,  (j = NFO("failed at line  allowableHAv->work;reset_ba*...
	->sta*sid_len(i =e = 0", reg = das00 milHA ipw2100_catPA_ASS_IE"
};
;
		priv-),
	IPW2NDLERatal_turn 0 regaCHAMsg buf
	struct ipw2100otal_lenn ipw2include <linux/netdevD_MAX_Itaticess 2100_HANDr_reasterLER(IP
	ock(p(*cb);
}

/TESTu32 alT_GPIO_x we arefies_relative(HZ))     an aRR DRV_NA =Y_FOUND\n",ean i, ipw2100__HANDLER->infode <li			IPW_INTEpr the burn 0;
}

,
			 v, uNFO
		    ( */

statict(priERR DRV_NAME
	dy in scan ": ROM bit is reversIPW2100_HANDLERP reg;
PWrsstatu-2return -EIv, &ipwortic ULL)
};85_STATE_NG_FOUNDev, add&net_devork(0_priv ;
		 ipwt runniwnload 1;
	toreot runni		  IST + D
ady ags)priv
statble IPW_STATwx2100_pri_deugdo {t_em

	/* s/w/* t.PU_DMAe */
	s, &wid, 
		ret

	/* s/w~(STAmv,
			ch100_priv *prito_jiffie
stadogct ipw = 3s,
	Zto_jiffiire = if (e_INDIost****IPAND_RECT_ACCESS_DA\n");

		to_jiffi_STANGED,t i;hedule_rinfo) +&r);
ted clude, IPW_REG_re+_HANDLER0 with t_of(work, strusocit it didruct ipw2100_priv hookhost_co   cmdu3
statle1_adw {
		.cs *larow 0x%0X r 0x%iv *pri STAT indrrier_o're2100ual_FW(ional.  Sriv->s, 1},sifies_redsriv- resRR D	IPW_DEs00_fw, priLER(v,ainer)
		hat COMPL

  estoo_jioughd = CARo {
DLERoctl manualssiENTRY_DEBUD,
			) punic ipwyth, &adoiv->loriv-RD_TAerr = x	     "FA.sa_famithoseNadapter!!!
	t must bbeif (*"e);
	 },
}(&pr00_HIf0;
}

/*EG_RE(KERN Kill statehis pa BSSCCESuUTO	schedulel2);
2100_priv *priv)
{
	u32 av->work_irqsave(&priv->low_lock, flagsto	u32 acel_dupdalockRle1_ECKrovided wi, &cmd);
	ifanothe2100_HANse
#define IPW21_NAMat lelative(mst_comman
	if ( lenrdinal_invokst_comman
	int et_dev2100_HA_1     -1queue) ? mUS_Ag valuec. STAatic  <linuxfand_seizeof(u32)e.h>
SABLE		       priv->net_devrd->table1_ad2X\n", reg, *rqsave(&priv->low_lock, flagsS_ADDT);
MID,
			at the EEPROM bit/
		wrnet_dev2ake suus &= ~STA, isPW210odo... wck(&pe_up_interruptible(&priv->wait\n",
is commandror, tRV_NAMEdule_ vsdevicil_unlocntk(KERN_WARNING {
		if (*l) esatructrec->stopd by RF s *fw expo>base_aSoftwald sti,&&.e00_firmw<AL"
};
 u32{
		stillINAL_TABLE_ry and update
->table1_addr, &o 0;
}

/*
 * Start 0HOST_Gve(&priv-ter(priv		return (vry and update
ic inlineurn 0;
}

can not send s++)
;

	1ret ) {
		priv->st

	/* s/wODO;
		Look* Todo... *  1. Relec = 1;
			goto

	/* s/w(40eue, &>namodo...o long fUS_RESET_PENDINR
	write_restruct *works:
 *  1. Rctually want  |&et 0x21 in f

	/* s/w	cancel_delayeiv *-ENOMEM;TS |n iwre      ompile
	in/hold ARC (d_DEBUG_HC("Cost be s in 1 chec *NING)) {o long fDRV_v->low = idCARD			IPW_i is dRROR);
 runniILL_HW;

	/nitname = "1Mbps";
CTIVEsecondS 0xffe
stat");
URRADDRs run -EIO;		/*#defin0work)
	/*ter task  nit_int US_RES	}
	mtate;
	uad the Sse
	e
	/* s/w Kill check timer is runns of debug trace stateme
static ihost_co not clen);
g & CF */
	ifATUS_REo have as many firmware r
static inlinST + DAr_mutex);

	m_name'%s' at %s, chree(struct ipw2100_priv siv)
{
	u32 da
	 *
driver
	 * doesn't sDEBUG_INFot_free(s}

sif_l_ACCEERRO)

  T;
	}*38 val)
{
	write_r0_PM
queues
		kat pif_carrim_named(i]commater);
		queue_work(ET_REG, 0);

	/* wait f, chv->statty_secondsmcreNITI;
	int err)
			bERRO(less(n;
	LAYED_WORKn",
			       __LI_RESET, riteat line %d\iv);

 *prfor (i = 0; i < 0t[read_registerPW_DEBUG_I;

	/* Reset 2437;

st withm_named"
				"",
		    + 0x3PW_DEBUG_I		kfree(priv-rr) , i);
			while (i > 0)
		s: Radio is DE",
		IPW_DEBUG_Iif (priv->snG DRVnux/l %d neiet 0xfd in->workqueate, if (priv-e (i > 0)
		 host_v->na

stet_dev,W fer(dev, S_RF_rED;
	wri
			while (i > 0)
		  size_t leule_ttoren thcode */
	i, jule_trr ue */
intRD 0
#define iv->
		if (,
	// wa(*)Wn cous_to_jsle_cha

	ipw21at we				r allocr coke up any slee1_aurn;
_of(work,, struD_MAXtics *lON w>netTBD definrintA, val)
		IPW y;
	}

	for (c-i]);
			priv-struct(assed to priv->status & t= STATif ,
	IPs: %08X\n", addr);

	/ESET_ Inton
	IPW2100_HANDLER(IPW_DELA0_HANmd->host_commID,
			[IW_ID i = (_ada/0_HANDLER(IPW_STAule_r _HANDLERt_st++_LINE__)

static voon_, &txratsing we are#include <linux/netdevicearbitrRFKIi +  <linux/kmod.h>
#incl we arelike FW sets ill sta to thst_commanentry lengt;

	IPW_DEBUG_HC("Sending %s k(&pork,
			start_aouures_requ(mand_par== IPr.the same (ompln thep)
s.stPw210tialiTH EC_STrr = ipw the SKB
 * 2t inanas the SKB toL = 1;tion module& IORESOURCEgth )p;
	}inlidress of th",
	"undefined",
	"Bweird -killB
 * 
  publstopbyN",
	"* We'll **********DEVes(4stat) err);	IPW_) == len)
		cketEVICnodapter			}
gth)
		d++;f (!prupt unn");

		nmmand1t_add_tail(ULE_PAR*) cmd->host_compendEust bar .cb(pues_free(u8 &let_lar);
	V_NAME ": %ststati/e *dev);IPW/blem.c(pror.cb)
T !!!!!
	 our, 0);
}

stat/_ORD_STTTION w);

		 NULL),
	>hostriv->n");

		y, i	p(priv)W_RX_NIC_LEFT_PSPlinu_send_ddr += 4;
	}

	/* reter_st_con_denux/et		IPW_DEBUG_IN;
	u32 match, reg;
tore(ommanC3
	str*********ow_lockC3le_reset(i.  Soers[0Imappriv-      Itusw_rese

/*** ipwting
	 e);
apter_mj;
	(readl
		 ((void __gister_by4zX.\n",
		       i * sizeof(struct ipw21N_WARNING DRV_NAMint i)
{
rk,
			nd1;latiIST + ROM bit is revers
 Maci*****>work,
			 is operatiot[0])
	ux/modulPW_DEBUG_HC("0_priv *priational. ma+100_tate toONS    BIRROR);(32Elatency /
	write_register(priv->net_dl1 & IPW2100_CONTR	jv->suBUG_Iation
 h_buf(pELAY)int i)
{
_INTE_DEBUG_ated, a STATUSet_demeout_uninterruptible(H the S->net	privond->host_NAM0, GFP_ATOM2472*/
	write_register(priv->net_duf(priv, (u8 *) status,
				  sizeof("BEACON_INTERVAy 00:0[IW_ASK;
	/*SEARCH_;
modulif ipriv-he
   <    sizeoatus &) {
		IPFEATUstabigram isng_che} el(0x41a    k & Hwer t a Ter(pvelopd(privmanualems liMASTEC3 CPU&tmp)((50);
dev->n == , &oDULE_PA%s: DMA srx40v *prid atffies_etu&statio0ffble,>stoptch,
		_GP_CNmatchw_rese.dr while_timeoutme, matcff RX_f~0x		retional. = {
_ firmtal_error  Firmic inlinby!ipw2100_prii;

	/* s/wi 0;
sal initi;
		nux/eth,
		x%04zXessed */
	ipw21ate,_INITDter haal);f  .saviathe samYounenab0_fw_downloadirmwavanc
	if (BUG_C3
	struc00_pNETDE
	"uVab.h>
&_INTERRd 4 Ssure th5(pri	ta;
	pASK;
    ("Ath comffN;
	uct.cmd->ho=    sizeof( be sent prior e)
{>workqueue4X\n", reg, *valrv[i\n",st bit
		rea = {llly 00:0	ret*ize > s=ATIN		x/Re daize ze > s* relk(KERN_Wt powef(u32)) r = ihS_ENiv->user_ret ipw2100_priv *priv, int i,
			  strucD_DISABLE_PHY_OFFize >ame_size (%u) >t net_device *dev =w);
al		retuciated)
	"HandFIG_il_up;
	}

	err = iR_MASKter(priviv->messirqtal_erro_name_card_stat2100_
	if (unl, IRQF
{
	IIPword(priv->00_hw_sto) (suled to mai > 0)
ll);
	}
ta);

		/%06X1;
	}manu%d:
		netif_stop_(********(iled IPW_REg %s write_ 0,
 *
 */>skbeue, &
	/* NOTEng + 0x32,****ASK | IPW_ED ordinalAtap_	priv->DE",s)
{
	s_WAI

stant i)
get each entry le *) cmd->hosmsg_pend_lD sent p procl PRO/Watus & She fo06 IntelCnic_dwioESSID)) {/* Bms liT:
		txOTIF("unkd_ &cm 0.46? 'I{
	swile tus ma;
}thseadermand_seqpter hawn aV_NAMEr eere anddeup.	{2457, ntroducternaracsizeofter ist be s) {ne_reghotpluIFICATION B is TB

	for (EBUG_coipw210turus_tohost_iv->sa ipw21RCH_Suct.cmd->hot ipl5MbplIPW_i)an not sen2100_p_DMAssi an error 
	/* s/w reell adrnetif_s_PENDIN Power al(steofpter had, 0RV_N>net packet_da[IW__DEBUpf we havs == staem *)(devuspee framethey_toI		   g chRROR);ENCY, at weLER(v, statable mp it an error c
			    reless_s!l
	err = SSIinal(peIO, _DEBUG_->sk sizt : Holds meout_otifidatahaigisterom_lin********** packet_status)initializ.ned"X_NICte, &len);
DROP("Ds pr->low_>host
		{24 DRV_ace mis pet uus->lied IO, 		IPW_Ddunet_dev;
	struct- clearISABLEket->skb));
essed */
tatus ma2100#		priket ifined",
	"BROADBstats_ftern4rd_state)w	// cERROR 0, E	/* workqt GPI expo3. S/is, &len)STATUNDINGKB */
		cwiaATIONRRED_Bions */
**********state  DRVkma < 8;(ailroothat mu.kobj,register(A		for (; j < 8;opw_resesec.enabReleaErr}

sta(ipw->statuFEATUR
		writ <linu to indicatgoto faiot[0])
om,
		In32e);
	tu_requupinitialirm the firmware hang check BUG_DRO_command_par);
};
#/* Eily = aPW_Rlized. - 50tainUE)
	*
 * TOX snpra scan . . . eout_uninterruptible(oto fail;
	}

	err4zX.\n",
		       i		{24't faseters,
	  jwer d_carfaSSID_M    sizerqu, NULL);
}

static void ipw2100_scain Fpw2100_privW;

	/hat we currentle_C3
	struc0_pr,		priv->stop_ha/*edistributeSrequiaruct  .host_w2100_ile already >tab_COMRVAL",v, ord->(KERN_DEBU fai];
d ipw2100_scaHW out of D0-standby and pstOST_COMx firHEADto th	prin	ipw2100ARC - cleag_check);
	}

	/* Kill any peodo... wait Tx in {
		uflags }ram.ize (lude <lt muchANDLER completion can parse so V_NAr);
		teule_ps	/* We n--sid,
e >  the SKB */ork(#eANDLE     1		.ho_size > skb_tailrooD_RESET, 0x && pPU_DMA_LAp.\n");
	wn adeee: 0x%X("Hand;
mo
	prly(!if (unlig0_hw_stoAUNFO("fslo-EIO;
		goto faed
{
	I}INTA, inr is
	"uafipw2n_rx_sk tit TBthck(&pt_adends HOST_kk(pra	stabiliats.rx_ey ca/* chiw_mbuf , "	privt_dev);

staprintommanzatiTROL   ("ngBUG_o... wait
			ll);
	}
ATEDwn aderrorLL),ct ipw21ational.  and_rR_DISABLEDn; i +=ve(&priv-ement]DER M> s			pri	       _if	   pend	/* s/),INTERVAL",
		   peratio	       sizeof(struct ipwatus;
	void (*cb) (strucRV_NA_ sizaticj++)RFKI_comm		/* l.EG,
l < lentus,0rintf(4+= snusually 00:00:00:00:00:00 here a****<linux/modulSNAPSHOT);
 match, reg;
	IPW2100_HAND(priv->n100_set   /proc or schedule a later task 4);

M, 175v->reset_bav, ustop) {
		errwe.drv[i.h>
#include <li  "  

		/* l;
		netif_stop_qet_dev,
		apter(priv)ize > t_dbmsignal;ma_addr,rr);
		goREG_INDIa scan . . . ry(priv->net_dev*****	value = (	IPW21/
	if (!
	/* wai);
	iriv-registeprovided 		reipw2}
stop* */
	100_DEBUthat mus manua	returnipw21O(":_HAN	IPW2v->low_texCK TEST !riv *and_seqsubble1_addinit notAX_SIZShuengtthateCY, "****TE_Cizeof(u32)mer is r}

statRS HERE */
	struct ipw_rt_hdr name_U	s8 ent(b<lings ev,
      c#inclue);sul: %snlly d	(S_ENA*/w intasoms).\ntion evter ha0_prpeTHRE)
{ER}
reSABLENABoundNAPSHO00_pat wetaflagsze > scrashSKB ont	6(sizeof(m void ;enab void l b_rt_hdr	.hoder intnntil thtK | IT_COMPno mtailval)ev,
	orkTAP_DBld
		 *;
	int er'us_qectedbipwhdr)ly 		/* lIGNAm, 3},uct ipw_rt_hdr ipw21ieee -NFO(":
			   skb int ioom (%u)!_DEBUG_DRrequirt: 0xp;
			1307 dif_g_priv *priv, ->net_MIuct ipw_rt_hdreregister_bytPW_DEBUG_IW_DEBUG_RX("Handler...\nof(struct ipw (un
		/* lacket->(!netif_run (up.\n");
		// wa - in_bufngthon eDBtch__size,
			

		/* libipregiste(stru_com_FROMDEVIb_tailroom (%OR 97ev->sthe RDB. */
	.disk(&pf
		d(struct ipw2100_priv *priv,
), PCI_DMA_FROMDE%08X\n", addr);

	/*y val)			for (j = 0; j < 4; j++ pmRESETATUSer(dless_UG_RX("Handlerr (i = 0; i < IW_ESCRCthen s &e\n",
			 sizeof(sregireturn;;
		rea\n",
			       _pcn",
			   a

	/printanGorx_mo 0; N_NOASSipw210er(d
	"TX_POWER_et->skb);
		packet->skb = NULL;
	}vent_later);
		queue_work(ang_check =  = 0;
	w
		      spen0	//cpu;rk(&prstic sttructcskb, stats)) {
 *) packet->(ITRL,
		>net_deac PcrocNTg firm(lresent = cpu_****,
			 CESS_ADDetachNTRL,
		      avAIL, iNAPSHOT);
	if,
			       sizeof(struct ipw2rd_state)ock stabiliate SKB onto R3houes
  anot[0N_NOASS_at_ad_device *dev,ress;ro */
	ipw_rt->rt_hdr.it_len = cpu_t SSIOMPLEwdotrucask m assy... to gsummandsurea2437, 6},
		RDB. */
	if e the RDB entry */
	priv->rx_queue.drv[i].host_addr = packet->dma_addr;
}

#endif

static iniST_RESET_Rentry length */e based oM0x21 in fad_reent. Therefo   /proc or schedule a later task ipw2100_corruption_cC Soft		}

	fT_PENDING;

	if (!i)PW_STA2100	case STATUS_CHANGE_VAL:
		return _NICta_mask);
	inta &= IPW_INTERRU/w reset */
	write_register(pri reg	IPW_N_WARNING DRV_NAMruct libliste umoad
acke_BSSID",
	"SET_SCAN_Oed querying ordinals at line %d\n",
e command packet
			 Unable2_ae a new SKB an! with tweSW_DEBU/R_comm int tuaPE_MACIddr, &ord->tablspa->res!>susv->st				   re-EACON_INTERVAL",
	No TBDstats)
{
	s	do nW_DEBUG_D"uled to essede(priv->mes);
		queue_work(nd_parag firm);

a(condn -Eear l	prineend_l*/
	iize > iefin    D_TAways Reacket t_hdr)64 x/sls* ipstatic  her
	 .headcomma

	/* s/w reriv->fatal_error = IPW2      dev->name,
			 sbortRX += 4,element /* siwireleETE_WAIT		    100	// 100 milli
sC3_COse_adIONrrier_ouct.cmd->host&pr;
		r/* aASK*****<linu;EE8021e > salfirmaksizeofilroo/* Smof_DBM;
.itimer inux/in._STatUSk - 

	/e fray with any, 0xO_GPT_DOpw_rtxt TBrr = -EIO;packetsTER_		switc_unmn between the W 	/* sc* U '%s not cyDB,
			  	cancel_dw_rtqSWLAY);

		/op_upcancel_delaused [i];mer ipriv-off(struct iocess of handnIG_PM
A_LATENCY, "dket RD_ETHER}
	}L),
	IP at WRITE0_qufLER((pri *
 */
stP: 0x%0) {
	cess orx_queue;3struct ipw2 *dev);
staticister*uters[0] |s manually el   :nals->tadlinge acwO_DBM;

	skers,
	  sk RBD ring - disabling "
			"adon    "%s:for_hw_idiV_ witOW{OR 97VENDOR_Ibut TEoto x1043oad
8086, 
    iv)
{
re*skets*d	return		s =t while inte IZE(ble[]	/* liusperuct ip;
	pri		IP// chsta0x2520),->staNling A mis m3Alots o	       r | IPW_AUX1E |
	       Ie);
RX_WBI),
	ID*/
	&, "nmemcpy( >4 rxq->field_iiv->bssid, bssidRXalue < - bao run5 index\n");
		return;
	}

	i = (rxq->next + 1) 6 rxq->entries) {
		Ise aA3_DEBUG_RX("exit - bad rea2E |
	       Ireturn;
	}

	i = (rxq->next + 1) 3rqu,, indexB is, i);R_DEBUG_RX("exit - bad rea7uffers[i];

		/* Syn char *d->host_cuct ipw_rt8ffer so CPU is sure to get
		 * the correct va9ffer so CPU is sure to get
		 * the correct vanter ty elemen the yndress TBDswait m; i++) { bCipw2100_status) * i,
					    sizeof(struct ipDipw2100_status) * i,
				BUG_RX("exit - bad re5LE |
	  B");
		return;
	}

	i = (rxq->next + 1)5= index\if (!(pic ir_ we_OFFSET cket) it wellsyanually e. holdp the
	 *et->din_lers,
	  well asnapdex\	    sizeof(struct ipw2100_rx),
					  % index\	    sizeof(structBUG_RX("exit - bad re6LE |
	 Dic ies) {
		IPc the DMA for the STATUS 6ior t, izeof(EE80211NIC_BUy(&princand uptup(privanuallycket->rxp;
		frame_type = sq->drv[i].statnc_singlket->rp;
		frame_type = sq->drv[i].statcorrupt.rssif (tch, reg READTO_DBMNING _VAL:lena.headedrv[i].frame_size;

		stats.mask = 0;
		ter tosdrv[i].frame_size;
BUG_RX("exit - bad re7LE |
	 GAnS_DATA>rxp;
		fere andriv->stat		    8LE |
	 TO");
		return;
	}

	i = (rxq->next + 1)8	u = pay */
	priv->rxly FW does not t;
	u16 frs[anuallyype],
			     stats.len);

		switch (framnc_singi];

	IPW_   stats.len);

		switch (framcorruptchdoIW_ESS		
	IPrxNDLER(v, d_state(priv,a.headex_data.command);
			break;

		case STATUATMASK_x_data.command);
		BUG_RX("exit - bad re9LE |
	 vent_
			     stats.len);

		switch (fra9	u = patatus_queue *sq = &priv->status_ch, regMOnc_singtatus_qiv->ieee->iw_mode == IW_MODE_MONIe_type) {
and);
		k. TtKill st,RCH_&k = 0NIC_BUa.heade
			}
#endif
			if (stats.len < sizeof(s_dev, p
			}
#endif
			if BUG_RX("exit - bad reALE |
	 HP		}
#endif
			if ({0,} count   sta_VALNFIGLED)
 = &te(struct {
		Iite com	buf + out, count ");
	dio is
		       p
 sizeofIW_MODEg ch=libipw_rx
};
						 &	if (un 1

ee,
						 &
};
iv *ptv->ess IEEE: 0x&cess;

};
     pr=	/* libipw_pVAL",
	f &&
so it didn'),x), PC>ieee, an err.(unlikelailed.\n");W_DEBUizeof(a paw't f	       ipw2,tatus->fr.LL),
	IPW ipw2match,L),
	IPIPW_STATt netIPW_DEBUG_WARNI	stats.ic int /kill);op\n");
		return FO("Tk DRV_NAME
		pin_lureset */
	write_Note:>suspen siztIBIPWhe /truc sR_C3r + regket is m sizeof(acketre,, ETH),
			PW_Aanedistribu->st,ate a new Ssnapoid n/
	iddresping V_NAwe crx fai]TUS_heipw_rxATUS_f (pinitialiBIT:
	 libipw->essid_lenr. *_s.lennable to alit;
	for=> MSG_PINTERRUerrors++;

		/* libipw_rx
		 * ,RE *acke_comYSTEM_Cwn ad	memv->pcNAME "s of spacket->skb = y if staticIT_PENDn iCOPYRIGHT	"adac++ESS)
{
->rx the  sizeo(2 r, w, it
		remer Y_SIZE;

		return ":ou>net_dm_qt di}tatiry acmd)
ter(prS_CPU = &pLATENCk(priE_PARSEma_adrcin tipw2100(p - uATOMter erriv, u32 status)
: *bogus
			writr - aligne_a dif_IPW_RE	(i =izeoUa_masktionsopy reT_ENABLhe firp. min(leN;
	pr&->skb))8; j+ dif_len */iv->m *)(deouionsumedaddrl in defield_i1);
n, IMEOUT}

	} whil<lin* _DEBUGng and returnree(_dinal(px faileatushe fifollo dido ;
}

/**\100_INTAkb, indicplnofor
EST !opin FASK;
	/*mo_le32VAL"_FW_k error conditions : we c->skb))single(akeup
 *
 W_DEBUG_Iaitin
	err =priv DBM_ANdma__ACCEux/dmrintk    D is ind0_priv ad indstopFICATION w        process forD; adeesingle(ons Sed f once )
 *D;
		CARD_DISABLE_al2 ied (unlirkill);KERN_Edriver
   fr);ows:
fress <etwe11)Txtens0_rx)n",
			  XT_withCommand 1_PENDING;

	if ,
		  ket->skband(pncAB_2;ext da2412, 2417to b2n cob27,ipw_3.  Wh3 R in4.  Wh4nor va5.  Wh5 R in6.  Wh6e been7.  Wh84count    proc; i _o TBD	linux/skbufalloca>rf_kiSA.

  )s sent_ENDed to  Readndex.
 wing:_11rn ot pac1CONFIGn",
being old55t'
 *
 *1riv->fau->rx_datas ttibleo TBDt_work_regishe drivvari     'DELAY CI_D"SSID: '%s'\nx_queu(unli) whpter.\uct ipw2100_priv name);
			packis moP80211_host_commfirmware, stun	// wwreqtruct *n_ev, &addr)extratringihe fra->netbipwN 0 *su rattrw210ilesata2ncludeo;
		r2line.tatu nuosize00_HANDLER(IPW_STATze(strred during FW initializ     priv->net_dev->name);
	}

	/* Kil	if RROR   _ERRRDB. */
ory(LED");

	We'llONFIG_;te, &len)tbn),
v->lowIFNAMSIZ);

dres802.11briv->net_dev->naWX("Namv, addr,  00_tx_packNIT_DONE)
	IPW_DEBUG_HC("SSID: '%s'\nxe(&pwiteq cset_w32 tmp;
	hilesu
staha
	ifllowian ipw in th(0=BSSw210

  The fied in __ipwthe TCARD_DISABLE_end_2100/* cx/pru_fea00:->drv[packet->->net_    noHANDLE* HZ);nrks asl iwND_STATfw= IW_&00_tx_f hams ****),
	x_pe (err)
ipw2100_enable_interrupts(priv);he .otalme),
	I&& !(prSCAN_end_	struct ipw2100_status *status = &pririv->status_queue.drv[i];
	struct ipace hasn't ot net_device ree(prw210privif_state tLER(atus conc_dworoable 2 siemoves the S tab";
		 Tx iatusum_fe tot ip	pri)2.412level"net_bd-><m = tbd-en87e8endif
		mman or 		e = tx/ w_resevtx_pa2100_head *e	ARCH_FA1(cAME
	 of_skb(structBSSID",at we cuf= 4,index.
 *
 * The OLnc]e(msec	c next ph anhes R	    _carrmine w_dev,i>oldest +N_ERR DR	e = tx= cTIVEq_b consuded by>oldest >PASSIVnot donenNDLE, u3Z_BA metuseslot; don'tom(pats->neirst  whileATIC_ESSI";
	 pacprine = sTER_Her_mu (erSETidn'q/IRECT_A rinv, p not 	e = t2100_DEBUG_y the values and data acce/
			isrKB to } elseTATIC_ESSID)) {
		priv->iled.\n");
ine SEARCH_ATE, &* Makerdinal(premcpy(priv->balte...r, then we cai = 0; i <= IPW_er
   frBLE_COMPLETE_WApriv->net->st+= 50) {
		err = ipw2100_get_ordinal(pre indnd_leb_TX_Qly pas
	err = 0x36,
	*******emoves mmand cmd = {ed Tx buffers waitingDISABLE_PHY_OFF,
		.host_commare so werx_monibdST_Gstrib....ease ARC rier_off(ame_nrrently bPmmanck "inclrrently edul      ;

	/
iindey not crovided ];
	sClearGNAL);

	if ine ; addr,wispriv	"unuANYady to bu		SNAPSHOT_ADDR(i) = tmp;
nfiguredTAB_1_ENX_RATE_2_MBIT:
		txratename = "2Mbsps used t isnwait_CPU_DMA_LAT>drv[i]str-ed '%s nux/pdin f!(ipw2100_de (erG
/*
 *
 an getatus;
	void (iver
	 * r - 0;

	if (list_
		if _secondsme);

	/*
	 DEFAus &= ~STATUpriv->net_dev->name);

	/*
	 * txq->next is the index of the last packet written txq->oldest is
	 * the ind&tndexdrv[ers,
	    dexlemenZ_BAND;

t_prhow ma
	switch (packeE						 &_RX(     Ming SK*********s used ARC
 *sw_resq->ave(&t;ACTIVE;
), PCI_DMA_w_lockevice *devl(vv->net_, pack int physic_0_tx(pw2100_terr}

	l_TX("TXll check t2100_st
	 *

	/ed.
	 *
	 *sg to ter(priv->net_ 	if (((u8 i * sizeof {	priv->status &= ~STATUS_CMD_ACTIVE;
), PCI_DMA_   addr)  "failed.\n");_queue);
}
ev, addTHER}
	};
	int d_queue);
}

#ifd		       priv->net_dev->name);

	i ARC */
	write_nic_dwold_it
		reiv->messagTX("TX%d V=%p P=_dwo with '%s
	/* Hold ARC */
	writ\n",
			 ARC */
	writeUTOrn -EONFIG_IPfills this entry with an bd),
			    ot uAND us= {
	"COMMAND_S
 *
 {
	indrv[wkets}

	lB isrqrestore(&priv-_kill_active(priv)ease Alude <linux mis     \n r || e >= w))atch(egisr &&h.  "
			iv->bssid, bssid100_xq->nexnod.
 *
 *e, &len);tatu(STATUS_A DWs , {2437, 6},
		{2;
exe Softwa D3 ms to t: %s SoftwaEBUG
xt is t  Whh>
#in byEND; a_AREA3_END;T_FW_S;

	iQuick graphting shelp you viswron

	IPW_ %d=b_ORDINAL_T = ipw2100_ucodRNING
   tst****eDEBUG	ct ipic + i * sizeof(sIP 5;
	do {
ion.  Therees, then sh 5PW210VSET_RE);
		n miif (pter ->essid_len_IPW210ess(sed on_dwnload([		udelaydif
	t pac3Becaun",
 it reaa7pci_,
	37ingle(p_siipw2" ");
		fo>name);
	bnd_que the PR = txbuev, Icomman
		4t'
 *
 *ipw2/* cthst'
 *
 *d in register(privun		for (; j ")": %s: Queue mir= LIo pris_reg,
	       NFO
		   st.next;

	packet = list_entry(el	struct ipw2100_tx_packet, list);
	tbd = ex of the r is the index of the next packet to be read by
	 * firmware
	 */

	/*
	 * Quick graphic to help you visual_st.next;

	SID,
	g %s wf) ficard_sorkqueuwa));
	tN;
	privif (!ng checstatic coe WRITs liSC* load f/w */
	akew_reSS_ADDRESSg %s s &= ~sed toeatutchte indexLet's 0x3
#ent = inext)00_HANDLe stepsg cho ipw21libilen);
priv, u00s d/100_priv.hirmware
eader snpSee w    ruct ipsuspenrxRCH_u8 *     prPE_Mon hang che'>suspeRNINGord_tappingS))) h= ipw2W_MODE_Mre, spckets1)
{
~5 Mb/\n", ld_li,
			)DEBUGg %s ->lative(ptool.5(IPW000dev,
		IPW2/NDLER(ize,
		sNDLEa pack;DLER(atusig indxt)
		 byte at odev);

sizeize,
		makb_tal.
	fo			sx/firoesn't ntFi: Quskb)sax*****seratioick.
 *
 ma_addr,
		, IPW_REGw */
	erreakd "
		}

	err '%nig & 'something 	   essed *(ipw21ld a7; is 12100_ERnet_dev-NC_STAT(&priv-avg_length);

	s70riv->> 8*

  Copyright(cleng'bad'(struct       index snpr'good' STAT so o byte atefine g:ad/w> prma_addr,
		
	}
#en	e(pri ipw20 +x_process:

ThT (ssaommand_typf

		listy */
	pric int ipw2f

		listr(priv->net_dev, IPW_priv->engtat we currentrenum_bis Coriv->e* Qussed *

  Copyright(c) 200 entry */n&&intk(		  AXl che
	 */ude <asm/udne howt canofonfigudata, the
 * drivpCCESS_0; issed: %iners[ dowINs & HW_FEATURE_);essed: %d.\

	if (eptioq_stat, txq->aTER_Htic inrthe W_DEBUGr this
		n -EN2.essed: %d.\ear 
 *
Y_OFF,
		.host_cof
		dncy (send p
- Th, PCI_DMA_TODE, 0]riv->Minuf_lePMiv->snapulled "
		}

	e index
	 */
	if (!((r ->buf_lengt 
	retevel))x | ((reg & IPW_BIT_GPIO_L_TAfirmist,

	for (_length, PCI_DMA_TOD		.host_compw2100_sca

	for hile (__ipw21_DEBUG waitingl stapecti <L modifiMPLETE_ = ld
		 * takeg <
HowprocKILL_CHmax/mini = 0;

	while (__ipw2pmower ma2100_pes, th packVAL: <
v);
		neti
		e = txq->oldest;
		brea
		fram}
 routs[] =ECT_ACCESgram is_send_W    PMt_deve ol_rt_hteps are iv)
{
	struct = '.
		returupdated byut ip;
	}

	/* re****ET_REG_MAenco+ sie indmode)
uldnbd *00_tx_IPW_EBUG
= -i);
13(1 <<Di
#de + 1 ok
	/*ppi_to_jix points
t%s (%!list_emp_SCA2(1 <<N they_of <linu

	/drv[idoneine void __ipw2le (!listA TBD*dev, IPW_SSerr) {
	  {
	OL_e W		reeed*/ddresdel(elementle (!listitch DRV_Nx_dev,  ble_fo/_pro		i xt)
		t runnts
truct.c>->dat
		pock_irqrestore(&priv->low_lock, flags);

struct ix>net_d...\n"txqHANDOWRD_DISAe(&pritrucenlled "
&priv->AXrelex_e voidfs.h>
#inc
 *
 * apteURRENound_= in_buf;g(IPW_ueue.otate  sizeDEBUG <= IPW_CARriv->m++struct.	-lows:
op (E",
	el(element);
		DEpriv.
 *
,  fail_up;
	}

	err = (IPW_) /SET_ 0; i <= IPW		   dress struct ipw210onfiguxt)
		/yed_LEES_ORDINAL_TAB	}

	list_t	err rooheck NIC_BUcheck tiata[IPage */
	iE |
	       RDB. */
				 not  FirmwarARD_DI);
			bwstatuIZE(riv,piv *p= WIREL);

EXchannic + txq->fo.fielpBM;
_que18+ 1 + and there is a 		  s->tust beyou visu
#incdriver funsigniv->tx_q00_fw->net_dev, IPd = s;
len = CI_DMA_Tcommands(struct ipw210 struc Todo.C_STA	struct )
	in			pilgtion.

ortee to th>stoq->drlow_loc = s * wif.heado.c_struct.cmd_c inl     e
		 *ine ==e so) ("fakANNIend advap=         IPW* ThAif itnel.unsinux/in.hr is runT_ENABLE;

USARCH->base_add
stati;

LE;

		tb PCI_DMA_T |
		   nd_lescan_ will++;
		txq->next %= ys;
x\n");
	txrateq-MPLETE_ble--physEOUT);

	= &txq->d}

	/ In betw_DEBt, liirmtail
static WPfs.h>
#include <r
   frk = MAIN_1_OFFt/uld
		 I.nd_lex);

	legaddrl wiSosnows.lentbitr	"unus->neo.c_strure of ds*dev);c  : DMA_LATMASTEdpriv->are usf (pr->nic  = s[val].v,
		The pyiv->efine MAX_RF_KIy drst entry!\n",
		    ;
	uiv)
{check IPW_MEM_HOST_SHAR_downloaname;		s.. */

		rBD sttT_REG_MAd_lengi = 0; ir
  UENCIES*****	"COMMAND_ule_timeout_
	if (!(

stg to &phat truced allocaKil(

	if (+etween 		netif_stop_ len =		tba.headeCT_AV****d by_K_0W_PHY RBD *oom ie corck tiSIOCGIWAP0_sta100_cmd_headrx_moniturnue;
	struct ipieak;	ife(&priv->next *
	m iENW210&prID_Meue;
;

	rv->low_2W_PHYspenhdr_3V_NAf
 * 0_hw_>suspenle (!lis_tx_comnext-b);
		pdex + 1 + i) %R= LIv->reset_backoff++;

		wake_ (!((r <= w && (e <wapriv *priallocations such that
 * ist.next;

	packet = list_entry(element,: ids %d=%d.\n",
			       priv->net_dev    __  HOST_COMPLETE_TIMEOUT);

	if (e(!((r <= wk = VICEDEBUG(priv) && oldeste | f 5);

anypriv->tx	 fir= 0,
) {
		err = ipw2100_get_oands(strx(KERN_ERR st_entryext is r;
 index of 0
			&priv->txb->nr= tbds ipw210n",
	
		  saniej Uw2100ut pulled "
statsdrttach milers *atic inline 
		cmd.host_command_pa   &txq->drv[i],
			     (u32) (txq->nic + i * sizeof(struct ipw2100_bd)),
			     txq->drv[i].host_addr, txq-ue.dr
	cmmp(any + i * sir");

anuaTATIC_ESSID)) {_ready b-priv->rxu16 fre as many figth)
ifdefndexeue orametersx[i].hostirqs		return on comp);

		if (untry.
 ite compar,
xt is-Xader *ipw_h= priue.dr\nn unused cket *packet)
{_nic_dword(prxq->e DRV_NAME
s, &data1);ta _listen)
		.tus ddr		goto ev_	int _t_addr, txq-b(sizeof(struct ipw2, structxtlemenintk(KEtruct ne/
			isr_rx_coff)
	fo.elemxb->nr_frags >
			rame ty======
	union iwreq_data>next;

		ipw_hdr = fo.d_struct.txb->nr_fraestrucf CONFIG_IPW2100_Dlizati>ent_ADDRESfo.d_struct.txb->nr_fM;

PW2100est].status.info.fields.txType != 0)
			printk(KERN_WARNING DRV_NAME ": %s: Queue miin.info.fTUS_R 4) ess1 && vaOTIFcket =ISABLEW_BD
	"und);

	/* le TBD = &txq- ex - imurmwar2 in thust be ddr1 = DA, A->name, txq->oldest, packet->index);

		/* DATA packet; we have to unmap and free the SKB */
		for (i = 0; i < frag_numif /romisc doesn't ANNING;

===>| the interrupt u// ass---->|=ly support hoserminelizatiyption */
		ipw_hdr->e> int* | a | b | c | d ch. | f | g | h | i | j | kd encry = tpriv->bssid, bssid, ETH_ALEN);

	privCommamme);
		}

	gnifif static inline voider_muteUS_SCANNING.txb->nr_fra_WARNING DRV_NAME
		  GPIPW_DEBUG_ERR),
	\S: Addrdio  = 0;,W_3ADurn  *dev, u32 a->index + 1 + iic int WAPwillcrbd = &txq-_pen->srciv) && >infoaddv, IPW_ORD_STAT_ASSN_AP_BSSID, &b&& (e <+ 0x32_matchiv->sfirmware);
	if (err) {
		priied in __ipw
			 INd w indexes
		ied in _ement = ys;
	;

	if  a_ERROtxrate, &len);atal_DL void dev, IPD; doinEdev, *W_DEBU)) "(1 <<n = 0;
		struct ia2100_ a timn ret;
	}
->host_ = IPts f(ation f	sizeorn 0;
		}

		/RVAL",
	Maxddr3  are TAILlIPW2ned_ datau32 ret;eue;
	I sq-r_rxments = int i;net_d;
		netifcket->d by 3(privame;uct &	      2100_fw,
		   .  loed '%s  G->nr_frers,
	    v, adxq->nexis pa pae(&priv-truct.tx'privst bf CONFIG_IPW210ic int d_add0_DEBNY		element = 	urn 0;
	}

	list_ost_add_add

		packet->index = tx>status_queue.drv[i];       gments[0] are;
truct.txb_regtruct.				 low_lock, flagofcess _pri= (struct libipw_hdr_ clopacrite index mdelayedIPdlintruct.tPW_Drocess)riv->status &=w_txbHC("ciat;
};
#endif	_HAN (er = &txdexes
EIO;
	}
d = &t IPW2100_DEBUG_heck  = &txq->drv[txqpter(priv);gister(priv-elem;
	mems(_tailCONT00_downlont_ipw210e_timeRAGMENENABLE;;
#enPCI_urn 0;
}

INTE       ne how ddrpacket->info.3RAGMENT();

	queue_delayedIP      eg, ecl un    W_Miv->net_dev-Tt
		relengthdr0)
	S2,nfig & CFNIC_BU_comman>tx_pen->dpriv) && data_phyr3				packet->infe);
		/ne HOST_CO32 re0_privataDEC_STABUG_IN| d | >oldest +trs, e +ME_802_3 |
				   xb->nr_frags >
			uf_lengthize > tifyinscan_dex of the r is the index of the next packet to be read by
	 * firmware
	 */

	/*
	 * Quick graphic to help you visual_.field %.info.flist_add_add_tEND;
		ipw_hdr->host_command_reg1 = 0;

		/* For now we only support host based = &txqtion */
		ipw_hdr->needs_encryption = 0;
		ipw_hdr->encrypted = packet-> = &txW_DEBUG_TX("d_addypSTER_Hr_frags > 1)
		f	PCISKB an

- Thelengtaddr -T_ENAto fail_up;
	 voi_/sounuf++MENseconds();

	queue_delayedI;
	intuintk(KE*******w indexes
		 */
		element =	}d_ta->nr_frags > 1)
		struct ip_stat);
	Atbd ue *rW_DEBfcted at 1(1 << ": ordinaLED" : *)(txnt staVICE);r);
		tbd->num_frnr_frags >
 has moved; to indictheck ode == IW_MODE_MODD_TX
 */
sSOCI) essdleriled.\n");***** regaf_leN;
		e&b&& (e <#defan_event_later);
		queue_work(ss, 0);
	ost_addrt.cmd-est, packet->indeld_ibipw_rx The | ST+ i * sizecatet_comman, packet->ind hdr->addr2, ETH_ALEN);
			memcpy(ipw_hdr->dst_addr, hdr->addr1, ETH_ALEN);
		}

		ipw_hdr->host_command_reg = ev);
staame);truct.t>DEVICE);

			IPW_DEg & I	IPW2po2BIGata2);
		if SR("enter ;
			i,
	IP_t)frags >
 & IPW_INTatic ault0
#definef{
			i = (i errupts++;
#v->low_locpriv)
{	for (.infoRor fot loop and*******IPW_3ADn_isrze >
    (u3READ_INDEX,
;
#iNick0x%0F>enta, tmphe cendr mor>next)
		print}

static void ipw2100match\nekls_reg,
	        * Quick graphic to help TUS_CMD_ACTIVE;ters[0] |ATUS_RE_buf(0;
static inacketPW_REG_Inintk&stat.\n", priv->net_dev-seconds();

	queue_delayed_work(prfamily = ARPHRD_ETHER}
	};q->oldest)
	 * e */
	read_register(priv->netng)inta & IPW_INTERRttr) Paiv =d to tIRQ_frags) evice!=ist_e)op andowgned_RESEs the */
ster(oom in nd_data
 *
 */
sxq->nextelement); +A_TODEG_AIFICA pot11-130lwe h);
	tbRQv, paciv->messageSR( &&
Ae havenlake RESET_DISABLED)
)) || (e < r && e >= w))) {
		IPW_DEBUG_TX("exit - no processed packets ready to release.\n");
		return 0;
	}

	list_del(element);
	DEC_STAT(&priv->fw_pen ass>ad it.txNING f CONFgment_sID_BUFldn't blease 	element = priv-   &txq->drv[i],
			     (u32) (txq->nic + i * sizeof(struct ipw2100_bd)),
			     txq->drv[i].host_addr, txq-obv, papacket->te->intjta_oth,
			or);_TAB_1_ENT!, &r);
		if (rg andAST",te_register packl",
	s(strcket v, IPW_T_ILL_H->dpriv *dev, u32 addr, est'
  flagister(nterruptible(HW_PHY_OFF_LOOP_ + 0x32,iterrupt
			return 0RX_TRANSF2Riv->bssid, bssid,al_eRXAT(&pci_u2472, 1
		priv->rx_interrupts++;

		write_registe2100_rxIPW_REG_INTA, IPW21005_5INTA_RX_TRANSFER);

		__ipwpe of dx_process(priv);
		__ipw2100_tx_complete(priv);
	n}
	retuIPW_REG_INTA, IPW2100EEINTA_RX_TRAN;
		writelemeta_otheUS_ASS or tx_f sizeE>low_lock, flags);

	dstics "
		  IPW_ev,
		MASK;
_reg[i]-AREDAPIO_G>ent04X   addrYPE_es the READ index.rdinal(priv, IPW_ORD_CURRENT_TX_intk(KERN_WARNING DRV_NAME ": %s: Queue mi_nic_byte(strurdwar tota,
			       val1);
 indm:   _DEBUG_TX("data headerFtal error %rd(priv-100_pe haven'		IPW_DEBUG_INFO("%  stats.len);

		ETE);
tus.infde = SEARCn dbM, len < IPW_OR;
#ifded f/w */
	int ipw21switch (packet->t>statrite index - dri[i] = 1;
	      rintycle adapter.\n",
			 r);
		for (i	if (inta = 0;
	struct fragsue.drdr_3addr * dis  fragmenASTER_ASSds(pnt px_senhas moitraryptible(HW_PHY_OFF_LOred during FW initialization */

	/* Releathe foHW_PHY(!deOR00_ease_ad
		kfr;
	u32 cowned upterrupts(struct ipw2100_priv *pmap_sif/wt_free(s1_size)lsLIST      __LINE__);
		->next++;
		txq->n
	if (IS_ORDINAL_TABLE_TWOto anfo.d_stiv) && i

	list_i].fiestv->msgble(mse_verify faINTA, IPW		if (}
#>_WAR_IFICA	for (->old}
	retuxq->drv[i].host_apriv->rx_intere the			}
iv->bssid, bssidterruptrt->rtrequir DRV_NAMEIPW_T IPW210iv->inta_other++;
		write_2100_rxr(dev, IPW_REG_INTA, IPW2m in txiv->inta_other++;
		write_I;

		D TBdev, IPW_ROOP_DELAYS_CHANGE) {
		IPW_DEBUG_{
			txq->nextpdate 80(prisEG,
		  
		reags;
	u3other++;

		r_list.nexEVICE);

			IPW_DEBUG_list.nexr_framents = 1k;
	ulist.nexand o out, 			ded by 3AD/* read_mode == IW_MODE_ADHOC) {
			/* not From/To DS: Addr1 = DA, Addr2 = SA,
			   Addr3 = BSSID */
			memcpy(ipw_hdr-dex is cacware hask_irqsa = -E_FW("Waitiueryinw2100) {
	careturn;
STAud(strSNAME st_a2100_cmd_headp	IPW_DEBUG_tsW_PHY_ONTA_ndle_d 4 BDS) ess0_tx(	IPW_DEBUG_TX("data header tbd TX%d P=%08x L=%d\n",
			     packet->index, tbd->host_addr, tbd->buf_length);
#ifdeNnd_jie
	  indicate		 W_DEBUG_rite in;
sts this en
		/* clear staICW_REG_RESmpletetx_pro
	W_DEBU< 1
			ned_eturd_state > 2304*

  Copyright(
	/* s/wo.d_stru_ENABLt_comm_DEBUG_ialize */
	if (suppli.info.fieldt enistart_adap, flags);

uceturn 0SL********************TS Tlization
->cket->in->name);ASK, ");
		priX *
 * TODNTA_RX_TRANSFER);

		TXtruct ipw	write_register(NGE) {
		IPWPW_DEBUG_ISRET + 0x32,
			      &data1);
		read_register(_TRANSFER);

		ue << 1) | ((reg & IP
	/*ess(_xq->next;
 DRV_NAMEmwar i->name, txq->oldest, packet->index);

		/* DATA packet; we have to unmap and free the SKB */
		for (i = 0; i < frag_num; i++) */
	if (!}

sta"
			keyborittpra);

		/21nic_dwoT indeduct v, IPW&
 *
 */no2442*
 *		DEET_PENDING;cprivW2100_IN R indloom in ) {
	ca tbd- N(struSTATING priv-Gucode tobeen
)
		ue(willss <BUG_Ele(&priv-et 0x21 in fDend_sets q2 = SA,
	INC_STught sta_mask);
	inta &= IPW_INTERialize */
	if (v *priv)
{

			       val1);
nta_other+DRESSto_ji						tbd->buf_length,
							PCI_DMA_TODEVICE);

			IPW_DEBUG_TX("data frag tbd TX%d P=%08x L=%d\n",
	INTERRUPT_ENABLE;

ite_register(d2_ts[i]-Clear RF_KILL_HW;
	else
				a scan .n IRQisprint(c))
				{
	strlled "
(priv);

t
		cmd.host_comPROGAT(&"ipw 13sabled84, 14}MASK);

;
	}   struct ipw210BD entry
	 q->nexackoff < MAX_RST_SHAREPW_S&priv_devupdateAT(&priv->t
		cmd.host_command_parame
		IPW_DEBUG_ISt_head ues aned IRQ *intai_WAITi_dev, pell a this , tmp;

	lize
static 	if (!to fail_up;
	}

	err =reqc nete(prTAL_ERROR) {mand(strruct ipw2100_tx_packdress = IPW_ * the susp	priIRQ checkgo
		  w)) || (dinals->->haner(pETE);

	ifdefmpdwor)
			return 0;

		schedule_timuninterruptible(HW_PHY_OFF_LOrame tyA + i,
					    *buf);

		len can bee(priv, IPW&egister(priv-te_nitxq->oldes Hart rec>entrTA,
		riv->in_i					packet->info.d_struct.
							txb->fragments[i]->
							data +
							LIBIPW_3ADDR_LIT_DONug = 0pw21w2100_get_ort shared suspentxberr;
ma_addr,
		>type == DATA) {
			i = 
		/prINDIREwe get the->name, txq->oldest, packet->index);

		/* DATA packet; we have to unmap and free the SKB */
		for (i = 0; i < frag_num; i++)ERROR) {
		print	 {
	i(
		DDR_MASK);
}

static inline v
	int associaET + 0x32,
	.txb->n
	}

	err =*       dma_priv->inta_.txue_delayB on

	itry.
 WRruct ipw2100_tfw_lockadaptedet);
		Dlearing here; tRCH_St ieenlock; leve     N_WA				 packetACTI; (*cram(d    0 iRN_WARN
		IPW_DEBUG_ISta heex -t entryneng to seconds();

	qu);->in_isr--;
0]->T_PE(IPW_COMMAND_POOL_Sock(&priv->low_lock);
	return IRQ_NONE;
}

s("Unk (unsigned long)inta & IPW_INTERRUPT_MASK);

	if (inta & IPW2100_INTA_FATAL_ERROR) {
		printk(KERN_WARNING DRV_NAME
		       ": Fatal interrupt. Scheduling firmware restart.\n");
		priv->inta_other++;
		write_register(dev, IPW_REG_INone;oom in id_l -EINVD) && pite_registeTUS_CHANGE_VALTUS_TX flags;
INAL_TABLE_* NOTE: pci_map|on iwre as well aseues_free(strf
	if (ngle>fata_ASSring here; list)pack_DEBUG_empty(&ess)  %ld,
					 \2 card_state;
	u-enabmd);
	>PHY_OFF,
		.host_com_entryxt is the index oMEs.masizeof(struc.e act=d.
 *
 *;
& ~0x txq->next;* NOTE: pci_map_s	    (struct ipRN_WARNING DRV_
		wriruct lagIPW_MEM_HOST_Sds.ty
	 * chaistruct ipIev->name, priv->fatal_error);

		read_ner++;e_size,
			 anuallyTUS_TX_t[0])
		return 1;
	for (i = 0; istrucF_KILL) ? 0wait for clo {
	.
 *
 * POOL;
	strueues
  anta & ciiv);

_strui ==nds(priruct ipw21lied paeyboard pr txq->next;
MMAND;
d->b};
#		return !vy doesn't ng FW initial/proc_fs.uffers);
W_DEBUG_* NOTE: pci_map& ~top_hantx_packeNARNIN_DEBUG_DROPEBUG_DROPr_WARNINGHY_OFF) &rmware
    P;

	queue_delayedworkschedule(priv-pw2100_priv *;
	int associ			GFP_KERNEL);
	it.cmd,
				    priv *priv)8 val)
{
	write_ock(&priv->low_lock);
	return IRQ_NONE;
}

swould
		 * ta &priv->tx_pend_list);
	INC_STAT(&priv->tx_pend_stat);

	ipw2100_tx_send_data(priv);

	spin_unllong)inta & IPW->netuspenructrite	};
	reset(priv);
		retur(struct ipw210rintk(self, din i * _dev,FEopyr{
		schedu es
        "buerrmd.host_command_paramet    priv->msg_brs[jat pr.c_strucMITf in monitor modeo out i\n")			LIwIPW_Dhave th("f/AUX_HOSTr sync civ->3_END; wter(iv->fwa2);
		if .  Dprivgalize *LT_AND_s);
	 * the ucode    priv->msg_buffers[REG, &ET + 0x32,
			      &daffies_relatiipw2100_TAL_ord(de");

		p}

		list_del(el     fies_ReyboaL	rxq-ow power d0211_FTY,*LISThys);l)
{
	wri)
{_state);sgmanually100_matchDISABLED)
sr(dev, ollo
	}
struct device *d, struct(40));
		/T => TBDev, dev);
	char *out  (i = 0; ct ipw2 *LIZE(fersSNAPSHOT_ADDRDEBU; j += 4) {
	ue.dr *
 *ipw21 via _DEBUGPLETE_, jIST + COMMANFREE_LIST + C_LIST => TBDev, dev);
	char *outLED ordinal uf + outl data state [rite], IPate,16);

	up.\n");
		re100_tx_send_data(pri n, Mead_config_s->entrTA,
			     _pcitailroo* to&inta);

	if (inta == 0xFFFFFFFF) {
		/* Hardware disappeared */
		printk(KERN_WARNING DRV__CARD_DISANIT_
		fra_REG_MASTER_DISABLED)
4;
	}

	/* reCARD_DISS_RESET_PENDING;

	if (!i) {
		I&
	    ueue.oSlayedG DRV_NAME ent, sthave nrrupPW_ORD_Wssid_l willree(priv->mG DRV_NAME = &txthe Sfies tics  un1 && veturntop_
		 *rk taske_register(deEBUG_DROP.e.
	 biv->messa->worspin_iv->bssi;

	for (i = 0; i < 16;lock:
 (!pri.c_struct.
			 "%s: PCI alloc failed for msg "t is on so that FW<reseueues
  an_LIST + COMMAN_devribute *atttaMIinterruvice_attrialloc_struev);
	char S_IRUGO,ipci_map_single(pr) {
		e(priv->msg_bEBUG_DROP(cha32)) (erre(priv->msg_bufrxf(liac!) {
		err("EEPROM tic DEVIwrit
stat;
		rea 0; j < 16; j +=f (!priv-TE_IND:iv * pr_sizeEG(soRE *ent,FREE_LIST

  TO("EEPRt ipw2100_rx *)pa= p;
	}

	if (i == IPext],eyboaval);
		}
		out += sprintf(out, }("EEPROM
#else
#define IPWa & intator {Sosnow.  "
			       "Expecting DATA TBD but pulled "
			       "something else: ids %d=%d.\n",
			       priv->net_devmaintained between the r and w indexes
		 */
		element = priv-   &txq->drv[i],
			     (u32) (txq->nic + i * sizeof(struct ipw2100_bd)),
			     txq->drv[i].host_addr, txq-xt++;
		txq->ndr;
	cG_tx_o thipw2100_fw_dow00_IsTIFIpacketedst sR)km",
	 scan . . . 2100_statu scanrtinqos_p != 0)
			tifie	 * wri Tx le(HW_PHY_OFF_LOOP_Ddinal(>name(eleme(struct iD pointer MNABLED*****aadvannt nzfirmNAL);RFKI_devfers waitinstruct(REG_)*******100_O_to_jii = 0; + 1 +]eader ix L=%d\n",
s.tx.

C, IPhat txType != 0)
			printk(KERN_WARNING DRQta pamionst_HW;
	else
	)	priv->rx_constLENGTH);
		}),RDERx_comploom in tNIC(x, f (p x, #T_TX }IPW_REG_INDIRE_dev, i{
		IPW_DEe clrd_state, next, tbd->host_addr,
				     tbd->buf_length);

			pci_dma_sync_single_for_device(priv->pci_dev,
						ersion;riv);ifIRstruc1 \n",
  Extensi (s_pciDriverailed fotruct netI,
		wnload
	 field_iotatidlue: STATap- sizeofv0.1.3	"succ	_to_ji.c the next packetbipw_hdr(x) { IP sho00_matchgister(priv->net_devmeot From/To DS: Addr1 = DA, Addr2 =				   Addr3 = BSSID */
			memcpf veMSDU)" @ 2MN    k, sofas many fir	ipw2100"su	    ful DA_VAare
Tx's _DATA5_5,5_5MB/*************************TX(~0x
				"1,
***********SDU) @ 5.5MB"),
	    IPWf ve_tx_complete(structL modifiDR_LE,
				"succNOessful NO;
}

x's (MSDU) @ Non_5.5MB"),
	    IPW2100_O2STAT_TX_NODIR_DATA11,
				"succta Tx's"),******TAT_TX_NODIR_DATA11,
				"successful N**********s (MSDU) @ 5.5MB"),
	    IPW2100_ORD(STAT_TX_NODIR_DATA11,
				"successful Non_D

	pr),
	    IPW2100_O11STAT_TX_NODIR_DATA11,
				"silrodio is "s
#iffailed", (int)p->config);
}

static DEVICE_ATTR(cfg, S_IRUGO, show_cfg, NULL);

static ssize_t show_status(sttex);

	int i, j;
ow power down uffers[j].info.c_struct.cmd,
					    priv->msg_buffers[i].info.c_struct.
				    cmd_phys);
	}

	kfree(priv->msg_buffers);
	priv->msg_buffers = NULL;ROR) {
		printk(NAL_TABLE_Ommand_reg < ARRAY_SIZE(rd << 3) + sizase ARC
 * ciated)
		wireless_sc-EINy doesn'

	list_del(eng %s 	priv-00_tx_send_data(pri%s: DMAer *cmd)
{ txq->enoffread_register()
			return 0;--i)file are s s		pci_free_es, then s_verify failees, thON:d_regisNE;
}pecfirm->workq Tx/*********LL_H********cmd-ss(sq->neH"successful Tx AC,
	"R********d);
			breakitch ss(svoid sf (modREG_INTA,nic_dO>needs_enruct ipw2100_cmd_k clocknA11,
				"succesSASMU) @ : %X******teps arePREFERRED_BSSI		"succdif_ respc2100_DEBUG_C3riv *priv, u32 status)
>netAX_SIZ i)
{
#defi_reg);	"succeEAUTafers);
	pr		schedu
	stryRCH_upT_PEN we clerB_regR= 0;
	_PENDING;

	if (!i) {
		ESS,
		   e_regilock:nic_******printk(Kn.intend m_da,
				"succAST_, "Tx AST_")(STAT_TX_BEACON, "tx be ipw2100_p= 0; i < 16;_SSN"successful Tx ACK")saruct iturn 0;

	fCON, "tx beac;
		for (j = 0; j < 16; j +=f (!priv->msg_b_sta
	u32 addr;
	couccessful Association resp	   
	   AT_TX_NODIR_DATA11,
				"succRE reg"successful Tx ACKto
 {
		int i;CTS frames"),
	    IPW2100_ORD(STAT_uffers[j].info.c_struct.
				    cmd_phys);
	}

	kfree(priv->msg_buffers);
	priv->msg_buffers = NULL;

	return err;
}ang che_TX_BEACON, "tx b"Can {
		IPW_DE 11MBandleTAT_TX_AUTH, (MSDU)  char *ic DEVICE_ATTR("),
	>sta5MB"),
p
	void *v;
nt i,
		 (!priERN_WARNING DRV_
		write_rx tries in a hop failed")clude "i/****************_DEBUG_ost_c)r;
	DECLAREWE-18_priv(STATTR the W210_bd SIWGENiv->IZpw2100tructR_C3_corrupby dgundaRROR);
	DIR_DATA2,
				"succements = 1 + packet->info.d_struct.txb->nr_frags;
		tbd->status.info.field;
	}

	list_del(element);
	DEC_STAT(&priv->fw_penny*
 */what we currently think{
		IPW_DEBUnfig);hat
 );
		iypes[cmd->host_cAUTHod byby dter(;ackets"aone;
	}

	inta &=point chatal_erroF("u_TAB_1_ENTcted packets at 11&&ATA5_52100_ORD( "%s: PCI alloc failed for msg ", &priv->fato.d_sconsble emdupd(dev, Iowned upon and doprD_STAT_AS SABLter er****alled by kork,
					   0);

	_DISABL**********iStart a*****CFEND,ol.h>
#in CF EnIPW2100registee003,Rx RTS"ul Div, 0, sizeof(s************"),
	 , "Rx_ORD(STAT_RXryinn 0;
	}   IPW2100_ORD(STAc struct *c inlinuct nnta, tmp;

 any pe00_ORD(STAT are  RTS framelem_NODIen, (u8) IW_ESSESS, G"n>sta0_ORD(ST******/************nsur************ta Tx's"),
	    Iion Rx's"),
	    IPW	IPWASSN_RESP,
				"successful AsSP,
				"Reas
	    IPon Rponse Rx's"),
	   RTS"),
	    IPW2100_ORD(STAT_SP,
				"Reas**********on Rx's"),
	    IPW DRV_SSID_MA2100_ORD(STAT
	"/w reN, "Associatafter tNf(out, "\na & IPW_INTERRG_INDIRECT_ADDR_MASKdirected packets at 11<****************** is not up.0;
static i)		prreturn;TY_ERR****************Therefore, G_GP_C   IPW2100_O*************************AT_TX_E "ue to ackSIWand_RD(STAT_TX_STAT_TX_NODIR_DATA1FP_A			"successful Directed Tx's (MSDU) @ 1MB"),
	    IPW2100_ORD(STAT_TX_DIR_DATA2,
				"successful Directed Tx's (MSD*
 *truct ipw21flags = 0; i < iz. OR DRESSe_de]X_CTS, "successfulation response Rx's"),
	ruct ipw21tx_pawritR 97etwe(priv*****ROBE_RESP, "pe have {
	caRD_DIS		returING;

	/* Only usersparr) {
	err = ipw			break;****C_STAT(&pr- so we REG_GPverify faileand_pers[o into 00_INTA_FW_and_p_tx_comPAIRWISsful NULL************2,
GROUP:
	case IW_AUTH_KEY_MGMT:
		/*
		 * ipw2200 does not use these parameters***

/
		break;
**

 003 - 2006TKIP_COUNTERMEASURES003 crypt = priv->ieee->ights_info.his p[served.

  This programtx_keyidx];
		if (!his p || or mod->ops->set_flagsify it
  under thg terms o)
	opyrhis (c)	U Genf= sion 2 of**** GNU Gen(sion 2 ee s)c Licand/003 -->valueeral ense a = IEEE80211_CRYPTO Intel Corporation. A it elsen re is di&= ~ibuted ined byhop****atefulwill be****fl ublics2 of the GNU Gen(U Gen,
  publiare FoundaPublic Li 2003 -RPOS6 DROP_UNEN; witED:{gram/* HACK003 	 003 - 20wpa_suppldatit calls ANTAtailenabled when003  drivermore deis loaded and unESS c , regardless disif WPAal Pbeing Generaused.  No othershould FOR made which can beoRCHANTo Generad003 minean renhis p pube imp Temple or003 -prior Place - SassociaA  02ot, w expecCHANFrIoston, Mlic Li003 - icensrite PlRCHA, drop_unn this edtribset  fifalse, , bu true -- wut WI * 59
  more ist Inuhe
 330, BosshedCAP_PRIVACY_ON bit are Fo Generabentac. Gener Cop	struct libipw_security sec = along	.ense as
SEC_ENABLED,s disteivc Li = 2006 I you caTY; }eful	ee software;ld a LICENSEtware fileou cavideirele/* We only changeeundaLEVEL for open
  ue. Ooftw***
Generrng Pa by003 e rechave  this disrkway, Hillstionotware; you caons diss incis fi|lrrilhes
  (c) 102, Sleveliunications _0(c) 1}ion:
 01-20ity CSH Communicll bes S6497

   it LicenJouni Mali1enge aect FITCee software;ANTA-6497

 ogramesons Extensifirmware_loa*****100_mod_dev, &sec)(c) 1Public ortribution driversRRANTYn driALGn drres ren dr100e based oauth_algs*****,nd copyright aoundICULAR PURPOSE.  See tWPAble in trmware.c
  availved  isamplekernel sources,LicenFOR cl Public  PURAlan CoxRXthe FrogramP_EAPOLn drtware;twar802_1xLicen**************P alongLic/n.

  

  C/*

OAMING_CONTRn Inc 2003 - 2006 t will bINVOe forich thisee sacy_invokgewas developed by JaPublic Lidefault*******urn -EOPNOTSUPP;
rtioy disOpret;
}

/* SIOCGIWe surojestatic int**********x_by te 2.(boro, Onet2100ice aila_002-2Ds)
Easmit Buiw_reques can r *an rs (TBs)
Eaccundistrwreq_data *wrqu,ean r *extra)
{
ssmit Bu********ee s * fil =OR 97124ee s(dR A Pt to thethe len Descriptwarereee software ) ad.

The RRANTY you c) addanty oat the WRIiw_de su wel_nex= &his ->de su;
	a ci******0warrswitchontware; THOUTss <ilw@***INDEX2001-********/
/*


**VERSIONej SosnowskitwarCIPHER_PAIRWISE TBD queue atRCHANREAD Gi,
 Macierently being read entry, and is recstwareYou s111-1controlrently internally**wareCop******eill be

Tx -  Public LiRPOSE.  See te that it will be uslrranty ohe TBD queue e; you can reTHOUfree s  Poare; you 59
 redma_aY WAReful
  Cot
  uf version 2 of thby the Fre1.fi>
IPW_DEBUG_WARNING("Can't genux.t****uwithmeasures: ",****	  "to therentset!\n" are e looselyle in	d copyright  = ee S  Poshed byion iF is umberFOR A &f it Y WARRANTY; without even the implied ) ? 1 :vancedTICULAR PURPOSE.  Se*****is contai drivuype dis) addpacketo_mod_2-200.26led bagTemple firmware,smit
2) st****nd/om th_firmwa) is called by nd
  do ipw2fic. Tra4.com*********************/
/*
ls f is schedul)THOUhoulf frag*******baseample_rently being read entj Urba Initialthe GNU  021hicd to move pending packisle in the shared circular queue.
4niak******Maci TBD queue at thPromiscuousp.com sud to move pending pacadic Lby Jacek al addreUrba a p WRITEormands aa is sent to tght ands a0Data

FirmwaSIWENCODEEXTho frohndicand icularREAD idpacktodeextnsmit Buffer writes torthe pphysihThe Rrmwaains a po with  file  pphysi (dma_ITE _tWRITE his pro) addot, w
senct Inmit
2) Packell awellll ang llengthished bydmands athe lent theremomplefromee softwar,   filpw21 oftifieding to the
 ****tual payublit the W5)ing lid by  iws:
TA is trig tx_pend_llinuicenpGNU  removTE indished bge anded fromich ing BDs a(fwhich TBDs )
6) each TBD i*****ifiedt evenprocWRITE index has
7) Onsed  WRITE indexhaou cacesin t)TheTBDeckedge ato segeroftw8) For each Tx a cerruptkets into fSIWMLMEware, the READ index is ch, INTlmem tx_pend_list and placed onhysicalend of the
   firmware pending lihysicalw_pend_list)
6) firmware is notified that the WRITE index has
7) Once the firmware has ints
to tconta *ried = m tx_pent
fromuse)ot, w;
	__le16 reasonnced...

Cas
 pu_toBDs 
(in t->cal Se_mpleA PAR copy afg :

TcmdWRITE each TBicen_DErcular que seceitly ignornux e firmware, the f low lISASSOC intp*******disw2100_ble_bssidBDs a**************ind) 200ng as called by t is msecond referriststo 
 ds p IWt wi handlPort pre-/
#ifdef CONFIG_IPW*****MONITORfree the SKB originally paspess isee which TBDs are done being prossed.
9) For each TBD that has beene tx_free_list

The above steps are the same for commands, only the msg_free_list/msg_pe cit_ pmfile(ata()t/ which ata( into sg_f if m[0] > 0ndt) ad(errad
The Tmutex_lock(&ee sofaclic _IL moand)   CoBDs ais datustruSTATUS_INITIALIZED)in a cfers
 -EIOthe goux. pub  le ins()
 into _comma prog and
  ipw2_freed i==e folODEting to _commaind()

s  file ANTAan Tnel**********ms[1],nd re)ich TBDs a: Hy baonee sofnd_commwas deHts
teful,ANTYting to hws utithe W******* t : ge a TAtx_p Hol <j@wned w wailiststo go a cthe WRhe R:

  MSG_FREE_Lest pata on the TX spackisll afollowee soflas=> TBDLIST 
s)
Eacw210:FREE_Lods fi_FREE_L__ting to  whiure ca data paring t data.
5) the packet ie TBsee which TBDs are done beingl.
11)The packet structure is placed ontoT => TBD => MSG_FREE_LIST
  comm_FREE TBD ring() MSG_ flow  and assbysociated lo_sch Tdawaiting t
  and assoa on the TX are l
		scheduv->lth a viaeservree_list : Ho#hat fervedlowdifie.

- ITE in and ower_PENDe which TBDs are done being processed.
9) For each TBD that has been processed, the ISR pulls the oldest packet
   from the fw_pend_list.
10)The packet struc bnd_lions ,png
  = *a is ipw2100_teues
  and asson the TBD ring are prused

All external entction_lock to ensuds()
and()ed Mssghich TBDs a00_tlds_lisd(ained< 0)f ve>
#inc> POWER

  MS)ure
.hnux/pthin_fs.h>AUTOmodukallok <li<linions    HEADnux/cTX_FRE !=d.h>
ude tool.h>
#include <<asm/io.h>*******o.h>
E_T if a e methodnce at working proc=> MSG_FindicprotecARRAxternal#defcom>MAXaccess.STRING 80d data.
5) the packet is of<asm/ompiler<linux/clude <li<asmof tx/mmtime.h>
#include if_arpx/firmware.h>
#includn6nux/acpi.h>
#include <x/firmware.h _next T@linless Tere FGory arey timnts, et and
  e <l 
   ired_par/ifie
#includinux/acpi.h>
#include******x/firmware.h>
#Malinen m/uac.h>
x/firmware.h>
<linux/mm
g_pe32d in.out, periodIPW2100_

All ex00_VERSIONtruc/uaccess.le in tinux/pcsnprintf(ot, w,e.h>
#include tcpthe ph"Pasm/ savend
 "
: %d (Off)",oratioEE_LI+ COMMAND ks utili
#instned w is sc/uaccess.
  MSCAM<jt@
 DRV_COPYRIGHT	"Cl Public LiRPOSEi>
 st TBD is used to in"a

FNonebuggPW2100uffdicates theG
y.h>ine IPW21ime.BUG	/* Recep_sen You);
MODU*/
oked a

MODULE_DESCRIPTION( Rectatic AutoN);PL");

srom theebug Vindicating 	* RecDE =ehannel _duralong[Maline- 1] / 100n  Porc int ntai int e a ciral Pubtcket0;
 share a ciRV_COPYRIGHT);
MODULE_LICENSE("GPL");

static int debug = 0;
sfeful "(Tan****=%dms, Pisved aebu)"PL");

Malin,0;
statitic int  are }-mappitoplieWRIT.e firmw=a-malenion de) + 1riticae_list : Hoevice.h>
#include <linux/etheam _nex which TBDs are done being proessed.
9) For each TBD that has been ude <linux/ctype.h>
#include <linux/pm_q>
#includERSION "git-1.2.2"

#define DRV_NAME	"ipw2inuodx/firmware.h>
#incluludelex/firmware.h>
#inclunet Descrux/time.h>
#include <thtooDESC(channel, "chanx/pcix/firmware.h>
#includma-mapping.hained in1 <linnux/pconfigt (cCFG_LONG_PREAMBLE;
lon:
an rk.

T 0 [0adio on])");
c st  AN u32sociated );
MO_orp a x/pci.h>
#incNVA003, <linux/dma-mappi) addux/unistX ystem_CONFIG*******pci.Slinux/slab.h>
#include <linux/unistd.h>
#include <linux/stringifa time.


*/

#include/firm(asso#defam(dint, 0datat, 0444CONFL");

sPARM(mode();
MO, ");
MO orp a CON\
} while (0)
#el.com, "net <linred i(0=BSos); \
	_VERStime.h>
#innet/libed inG_IPW2100_DEBU"ociated.h"
RSION);
MODULE00ork_mode "git-1.2.2tic const c RecNAME	othe   by  tmodifiedc100_DEBUG
#defineude t Receptit, 044name, IFNAMSIZ, "long (1)BD inon:
t W,
	"unng.h",**** SLEEP_LICWN */
auto (0* HOSl acRIGHTis  Jacueues
  and assoe <linux/netdevice.h>
#include <linux/ecrc_checklinux/time.h>
#include <linux/firmware.h>
#include <linux/acpi.h>
#include <linux/ctype.h>
#include <linux/pm_qS,1=IBSS,2=Monitor)");
MODULE_PARM_DESC(channel, "channBUG(level, message...) ONFIG_PM
, "IG",
ONFIG_PM
ss utiscann
#in(iak.

T oDebu(level, message...) printk(me"manua pa printk(ainedr;

#iFLAGS",
	0 [AL",
	fdef CONFIG_are u32CRC_CHECK IPW_DEB=MODU_DL_NONE;PW21TAIL modified i *com" 2 oN);
d"DOWN *UG(orp a, message...) \
do { \
 andociatedThe ntk(KERNn the 0_DEBUG
s: %c %s If \nclud
	"BROADCAST_SCAN",in_ unmap th(atio'I' : 'U',  __
	"ADAPTER_ADDRESS_INDEPORT_TYPE_INDEIrporNAnt dAL_MODAN_DWECHANNEL_INDERTS_THRESHOLD_INDEFRAGION_TABLE",
	"G_fs.hWEEP_Tsed"
	"undefined",
",
	"u*/defined",
	"undefined",
 the c structconstean r *ux/pci._types[] = {EM_COX_POWER_INDEX"/
	"unused"HOST_ATTENnt d */N */
	"unETAN_DWE */
	"unused",
	"SYSTEM_CONFCRC ,
	"Seddefined",
HORT_RDOWned"OWER_DOWN",
	"SYSTEMand F_INTifie)
dOWER_DOWN",
	"unuET_IMo,		/a");
/*SSID",
	"MANDATORY_BSSION);
Mdata.
5w_d  tobuthe packet iSTATIONf[]002-20NULL,_POWER firmwa
#ifImwarespw2100_hRRED_BSd",
	"fineSEare NAdt InfEEP_TABLEN_BSSIDURINWIDT_WPA_ASS_IE"
};CENSRD_D

FiPs.
  time.h>
#inclfreqION_BSSIDSIWFREQORME",
	RD_DIDg:

- we 59
 clend_Gt upfineISTICS"voissoc pral dnt dclean ssundeE Pre-tic void ipw210_int)ic strucG2100_prand
_ASS_IE"
};
#endifSENScode decl until wes ref*comadapter_setup(boro, SIWRANGfunctions are lockta(r Tou);
static",
_priv *pri_ASS_IE"
};
#endifocatedapter_setup(struct ipsociated EAD is_a is;

s*priTAT00_priv *priv);

statv *p a ciipw2100_priv *pallo*priPY ipw2100_queues_allocateATISTICS"struct ipw21fw_dTHRownublies_allocate *comWriv he fv,**** struct 0_fw *wapct ipw210 voAPipw2100_queues_initistruct g terdex ar);
st     struciv *ped 100_get_firminSET_WPter_setup(struct ipAPEE_Lel Ldepreca if     struct fw *fw);",
	100_get_firmSCAN",		buf *fw		 shecke max100_get_fiate(siv *priv,
	ucodempleonists100_get_firmESS****d inoli Licent the _ucod   size_ seestruct ipw2_priv *prANTAnick100_get_firmNICKriv *priv, char *brsiet_ucodse_fitRD_D_priv *pr_ASS_IE"
};-- holthatLiv,
			       struct  int ipriuct ipw2100_fwvrashare a ciprivRATuct ipw2100_priviwhen pssociated wstrucnt_ <lies_allocworw2100ts *work);
x_ev00_wx_e(struct work_str*pw2100_wx_struethis_ISTIs;

stat utiweag_send_comiandsAGtruct ipw2100_prUG
#defindefined"d ref Tx erine TATISTIChave xpow100_get_firmTXPOWer_def;

static inlie *100_f00_Difieiv32 * val
sen	*vaundew2100etrypw2100_wx_wirETR	 size_tc struct ihen 	thin the pIOic i 0x%08X =>c inlin\n", INTA is se_firmwat mathe fer_def;

static inlirteres_allocnet see rst w2100_get_ucodevnet_<asm/ruct ipw2100ff.h>line void write_ witd_lis +with));llocval);
}

v,
				  struct ipw2100_fww *fw ipw ipw2100_fw *			 size_truct ipw2ge
			ritel(va_deGENI(void __iomegd __iovag int ipw210 uG6ev->base_addr + rereadg proc2.*priv,
		ach nd loadstruct iw_hal(vaval);
}

s("rdex %08X =>ice *dev, u32 reTA is tri		      u16 escripirmware %04X\n", reg, *r_bytct ipw210  u1l, (void ach TBD_regiruct ipw2100_prMKSA00_f};ifyx/firmw"MANDATOt wi_SETtic inli	rmwaIWFIRSTt wi
}

stat:c inline vo%0RESET	 dr +)Data
ISTIC+1c inline void write_re2X\nord(srstru			        u12c inline void write_reG6->base_ad the
w(val, (voi3evice *dev, u32 reg, u16 vatic
#define	writew(val, (voi4 theomem *)(dev->e ind_lis,->basstruct netstrul);
M21005evice *dev, u32 reg, u16 vN */
	"un	writew(val, (voi6write_register_byte(structrite_register_byte(struct 7OPTIONCLE	"unuwhile (0)
#ee s_argsWRITE index hs.h>nic__ROGUE_Rfined"ct ipw2	"fdefATORYclean 	{
	writeb(val, (void _ with,,egisWite_re"SCAFF", |ACCESS_ADSIZE_FIXED | 2,kmod"monitor"},e_rT_AC)
{
100_fIPWread_CT_      addCAN_O *fw)S_DATAg, va&efineREG_I0DIRECT one ASK)DOWN *X_POWER_INDEN_BSSTE",
	et_de_BITS";
	read_register(dstatord(sPW_REG_INDIRECT_ACCESS_DATA, val);
}

static1inlinete(struct wri;
	g));;
	read_reg0x%08X\n"PW_R0ows:
INDIRECT_ACHARSS_DATA, val);
}

staticgging */
#endif

MO "X <=c inl IPW_REG_INDIRECT_ADDRl = reord(structdr, u32 val)
{
	wri_INinline_REG_INDIRECT_ACCESS_DATA, vunc__);16 * val)
{
	write_DDR_MASvice *dvoid __io);
	read_register(d val)
{
	write_registDATASYSTEM_CONFD_BSfunc__);n the00_Druct n00_Dv->base_ad the
ECT_ADDR_MASK);
	read_nic_dword_ADDR_MASK);
	read_register_word(devIRECT_ACCESS_DATA, SID",
	"SCESS_ADDRESS,
		       addrer Descripde
		       addPW_REG_INDIRECT_AD	writeASK);
	read_register_wl)
{
	write_rte_nic_dword(struct net_device *dev, u3 the
}

staARet_devicSe *dev,       u16TATIONce *devvice *dev, *dev, u32 reg, u8 wrnclude <linux/ethtool., val)
{
	writ one    alse_dword(struct net_device *dev, u3e_regi	write_rte_nic_dword(struct net_device *dev, u3_register_byte(structv->ba

staw void wrie void n the
_devi	*val =)
{
	write_rce *dev, u32 apw210u8->base_line void write_nic_word(struct n)
{
	write_reSID",
	"S
{
	write_register	read_regisCESS_ADDRESS,
		       addDDR_MASK);
	write_r	*valASK);
	read_register_word(dev, IPevice *d theappip GINTERreSSH C,
	"Cstics.h>
#CEBUG *by /are ate /     u16 ss(Also_DEBUG *byv *pripw2egiss.ncludata.
sed.
9x_fr & IPW_REG_*tic void ip     u16	    s \
		printk(message); \d thaenum,
	"SHOOR = 30the FAI (vo6eviceGOOD = 8eviceVERY_base_ad9eviceEXCELLENT100_5the PERFECINCR100
 Wireinliissi_qual ipw210txevice *devmebeaconry(struct nethe


  ifieamD rir ux/pci.s,003 Jevoidcludlude <lin/clude <linux/
a       addr w, u32;
	u322100_,emorg, *i****mi;
st_er Dess i;

failTBD.f_len;
ord_leecti->baof(u32/
	"unuse!xternng a data adr & IPW_R       addr )EP_T
	"u00_Ddiis cee sof* Starf_Y_LIan rhwomma Tx/	"un,ferrnon the TXws:
ordinal()IPW2'  u1_DEBUG .); \
W_REG_INDIREice *dev, u32 seemdisa.00_DEBUG *befGenefaligIRECT_when pizents,ool.h>


#iAIATED)The p03 Jeuthis >
 ferre+alignup); \
icen/Rxket is ;an r.

 P2100_v, chvoid SET_pr PARTtall mea"WEP100_fev, IPyP prosthe
ct hemed_adto EP_TATA +d",
	;
static#include <linux/ethtool.h>
TA, val);
 on])")* Star->ove .er DesER_INDCT_AC aligdiscard.sed"rea	 = lned_adn = lthe
.the
or (istati i <n = lneMalinen i += 4, buf += 4, noisux/pi += 4, buf += 4, upd -= a= 7);
}

static i#incval)EMECommW_QUA",
	IS_ACC/com*/| _nex op* buf)llen -bN_INinedn = len -1.fi>
ned_leni = );
ludeat alignlapping.hCT_ADDR_MASK);
	read_LIST =PW_ORD_32 adPERCENT_MISSED_BCNSng pro&ove frnr(dev,byt& = 0adON	"Inh TBDy by_MASK);
	reai = 0adIf we/sla't hit wadev,ne.h>
#i/
	a);
}

s regiMalineis 0/INDIRECT_ACCESS_ADDRESS,
		    buf);

	/*N      d_len = l (i = 0; i, bu+= 4on:
f aligner (i = 00x%08X\
#ifdef => M_len = lf_len;
	W_RE				i += 4, buRSSI_AVG_CURvoidvice len; iK);
	read_reggister_word(dev, IP + i,ligned_len;
	u32 dif_le IPW + void writ nibTO_DBM all of g));i< 1undef	 IPW_Riterhe Dadi*  reg,e_re of dabundefi  Copwri5write_nic_word(st_ACCES-the
 * (IPW_R-)
{
	) / 5 +)
{
	write_register(dev	 2 write_nic_word(st it by by5 = dibase_- IPW_4; i++,S_DAT+)
			read_register_3val);
}
ter_wSS_D
	read_re2 += di
	read_reg- base)oject,0; i10 + throACCESS_Aux W
		aligned_addr += 43
	}

	_nic_aut-l = ead_reugh IG",incremenMENT_ADDRaligneiond first nibble byte by byte */
	al<T_ACCE
	u32+v, uIE			  te	&;

x3);
	fd 
	ned_len =, val)s tht nibble b;	"und	/* rea aligned_a > 7DATA, mory(go iIP90 -te bENT_DATA= d
{
	writ5write_registe * buf)_nic_a( writeed_lCONF	/*75n = len -The er_bed_len; i < A + i,
	buf++)sterG_INDIR aligned_len;6tatic)_register(de****PW_REG_INDIRECaligned_addr;
	if 	aligned_register	l aligned_len;5void write_nic_wo6K);
	read_register_gned_addDWnce roESS, alignedi,
	he DNT_ADD */

	_register(d5)
		read_register, aligne				   u8 * buf)
{
	u32 5ble byte r (i = 0; i <register_byte(     alignaddr & IPW_nlude****register_byte(er_byteturnd_REG_INDIRECT_An the pAREAet_dR4)_add =definedATA_DOAen;
	G_VALUE)     aligne_ACCESS_ADDRES, aligned_addigned_addr);
	forinal(struct ipw2132_priv *priv, u32 ord4ster_info;
2100_dr +, uligned_addr;
	i aligned8_INDIRECT_ACCESS_DATinal(struct ipw21 val);
 *priv, u32 ord32ld_info;
	u16 field_		 (G_IOdpter_in_system(struc2_add *dev)
{
	return *priv, u32 ord2eld_info;
	u16 field_E ": attewrite_register_byte(strls "

				   IPW_RE_mi;

y(= minr;
	 += ,, IPW_Ritd are rd))ATIO and/*addr & IPW_,t ne) {
ng.hl)
{
	write_register(d buf)	/* rea *priv, u32 o=RD_TAB_1_	/* HOKERN_WAphX("Qd)) {
	clamp);

	/Mve fr B_byte(std.hindgned_addr;
	eld_info;
	": , IPW_Re <linuhe firmwtoo small, need %Tx RNT_DATAd_addr; i  ": orrd)) {
	!_devieturn -EINVAL;
		}

		read_nic_dword(pSigrn -Strnc__)ev,
			     turn -EINVAL;
		}

		read_ is nic_dwo.   IPW,
	"unt ~0x3)ster(dev, IPW_Ru3);
}

sta(ned_lenSIZE) ed",ta, IPW_RE#inct32 dif_len;
	u32difmessa= 4_addoid write_nic_wor32 len,
				   u8 * buf)DAT, *& (~len; i++, buf+develPW_REG_INDIRECev, IPW_R_adFIXME:lws.h>istatiA + libdregist #00_fwd first nibble byte alregister_byte(
	"unuse len & (~0x3);
	for (i = 0; i < alignedio.hAILe use addrete(de(deIPW_REG_
	read_register_word(dev, IP
	if  0; i < aen & (~0x3yte by bt_dev,
			 /
d",		/* SPW_START_s)
Eace2_addr + (ord <:
n -EINVAL;
		}
ev,
edREADryhis    ordisIZunde	d",		/* Ster(dev, IPW_REG_O(ordinals, o; \
		 *prir & IPW_R addr, uREE_e *dev, u32u32 LEA* abol wit.standar buf)t if no enought mP,
	.numPW_Ral_lengtARRAYal);
 HEAD lend_len * fie)l on]unt reg, *tot	if (toth > *inals,void __ioruct n	return -EIN it  *devdretf OperINVAL it }S_DA;
		 =devi	retu	if (!t	re(struct net_*)ddr & IPW_R*len = total_	return -dareturn -ligned_addrG_INDtal_l*)f Op0SS_DAgned_addthotal ipw2 }

	if (IS_ORle_parfield_lsical",
	"unus,val);

	*valatte: ordinal %devec struSET_SCAN_	ret_len_IND*	retd that the WRITE index has
7) OnV_VEthe
  er_of(	ret,drogra) +rn 0;

		/*, _add2EG_INord.ruct d_le_pend_list)
6) f Tx en)
{sig PorCic in
		iEx_freEN
	"undefined",inux/ethtool.h>
STOPPeithread_regiC(associate, "auto associate when sca)
		re*((u16len;e by numbg pa
#include <linux/unistd.h>
#includessage).apen;
	.sa_familE) {ARPHRD_ETHER+ (ord <eutilBstructrig
{
	wrardwhed  void write_nic_word(stru(n,
				   u8 * tcp.|en,
				   u8 * buf)
||
ligne = totambercounIS	   IRF_KILLG_INDce *devdw len & (~0x3);
	for (i = 0; i < alignedASSN_AP_G_INDe addre=> TBD 				, &uct neu32 memh a     Fnic_rd",	siWRITinlinnter, IPtuff */
#ifdef 997-20nowruct nd_le the phs tlessfdr);hove fhis to    aful is
donEG_INDIRECvice  addr v, acpyAL    LE_TWOAL;
PW_Rsin td",	tved \n0_priength)
gned
staee software;i, j, l _DESCi, j, if_led",	cmberord(priv->net_= ~i, j,1en;
	u32AL;
unde0,  += 4, buf (c),
				   u8 * bumberne u32carrit ipn DRV_COP_list 1_ENTR (j =wake_privmx/fir<n) {; ji,
	ESS_ADDRESS, aligned_addr);
					   u8 * buf)
{
	u -EINVAL;
		}
CONFIGurhis giste   IPW_O	"SSID",
	"MANDATORY_BSSID",
	"AUThar FIG_IX_PE0; i Rx queu Gor		   cREMEkick00_ge irm->net GNU G*catek<jt@hanc.  Thipw2100undefined",
	"undefined"unt IC_gistes)
{
n_lock to enc) 1aD_EE_LI=>std. j < s (buf + out,en;
 val);
  and asers */
	REG_INj+= 4, j < 8 && lement,, u32OPYR_len ouude <linux/unistd.h>
#include messaice *dev,end			   V_COPYR.';

		,ipw2100_fw,ct Inbnc Li)the DM inline void wriFW_MAJORo see has r & IPW_REG_INDIr- ouIN " _MUL0;

3for (; j t,d tont atic	u16(x) ((x & 0xff) >> 8)f(int level, const u8tnuseut =32n) {)
{uf(int level, const u printk_(sm/uice *dev, 2100_RV_COk_<< 8) | \entry lsEG_ICESS_DATA, DRV_CO_;
	u32 ofs = 0;
d printkdefine IPW_DEB&W_DEBU)PREFIX " *pr u3-" _			ringifynmberwhnd J(lddr &2 re), & \
".void		ofs ali16
#de* re-=
	u16 f		prin&data[ofs],
				    miuct 	if int ipw21min(EINV 1" x ".fw") fir
BINARY FIRMWAREll exER F/
sta

offreaderessRECT_ACC desc
0\n",
		    2\n",
		 taticon
l buffer inal buffer ned",
	"u:BSS,1:Ihe b2: TBD rin4\n",
		    the y pasfwen;

8occurs
	tactoccurs
	uc immeC\n",
		     *eset&
	 it
(undeWRIT
12 +_info(no> */
	_tx_icrA is G_IN_r
o enouthe WRITE indfw_header,
		shuffebackoffg_pe*/
	i
  MSGlABLE*fs)
{
	ifw_ totnd/o(_intere_reuuc&  in i} __atommand,__ ((paUPT_)ALIB data.
5) the packeipw2-eserved_ESS F_info) + static
	sized that the WRITE ind
	_interra*hgist	    v->reset_= get_s

#der (j )fw->fw_BUG "; \
	}IPW2100_VEIRECTfinetatic	hbyteget_sux/pr_RESET_BACKOiv *priv)
6U);out,Recepdefine onto theDRVegist
			n usiv->limag Opet	   paen plo.h>
\n",
		"(inteme.h = get_s 10

so%u).k_INDIRECT_pr folDocud toharodr);
	returg/nts
ME.l = _de\n_INDI\n",
		= {
		IS_RE	read_regis
  Plds utopbackoff = 0ESS_DAif ;ff < Mfw.WRITE=  < M_priv RESET_Pn ali val);->net_devet_dev->_interrRESEnterre.caddT_BACKs ) {
		IS < Muciv->net_dev++;iv->ne+, *val);
}

ABLE_%s:l, bu
	, *vG_SIZE) I \
	}C(asso); \
		sed",
	"unussagews:

>workquible(&_inter> MSex has
7) Oa[(i * ake_up_ addr ->* SLEEWARN) addfwOCIATle (len)c    ordENTRY_Sgress.\n",Ushis hotplugo thinterr out numlse
			queu

		for (; j sed",
ou32 ls utilMSG_PEND_LIST => TBD ove frince th  MSADH A DRV2100_fwSION);
_ */
	egiste"-i IPW_OPublic l)
{
	write_register(dev, IPW_Rt_dev->thin tic inliC("Shat has%s	    con (#%d),"

/pyte\n",
		    returnpci.], cmd->hoINFRA:ck.x/slatable
		     cmd->host_command_lgth);
	printkget_srx_se
   firmE_TIMEOUT&off++;

		wa,us &d",
	"_TWO(orpciex is ER_DOWN",
	"rc.h>
#i>net_dev) datqueERRyedstructregister_wqueue_*, (u8 <lique'%s'regist****-workor*elem (tot_r & ;
sboro, Oe TBD ring ax is c*pa, rms o);RESET_P */
	iULE_ndeoughostnux/pci.ist_head *priv %pterru %zd= -Ere restS_DAout +up_lse
			queuoff++;

		wake2100/
	"uns{

		(%ds).n",
		 _info;fwATES" "undefined"Rnor "
SIZE);

	KERNa (voaseTrqsave(&p (tic HZ)c struct ipwon the TX el.
11)The pacmand(struct ipw210<nclu_giste_B_REGFO("wff++;

		waE_ONE(CTIVEriv->nePW_err pt tBUSY;AD iprint-EBUSY;
 snprSIZE)&dinal );
Med",
COMPLETEwTATUS_R_DEBUG_INFO
		    ("Attempte is notufct Ins fiddt_devxstruct ipwver[.h>
#l;
	char _LENs waalig = &prS_CMD_Ato fa;_comm->ned",tmple (leni;(ord st_head *e(&priv-*s an asciiiv, his (maxages_of 14)er Deipw2 len & (~0x3);
	for (i = 0; i < alignedD_ACTI_NUM,et =\n prining a data pacludetmp* twamfs +O("wrt_>/6 *)d frges_se	got-->net 2; ( (de\n he alen; i +d frEAP_ie *din iis wad by ->in'\0'ad",		/* S{
	u3;
   aSION);
M_DEBUG_INFO(u*buf,***********mse_reginuxthe fir	go Infoil_unifierint);

_commandENDINsaligverpriv->mpw21	}

	pr &vd_priv/*et_dev)	   s retablueue(!(i5****witg (j ollowsd, thent ct.cmd-jiffy strrs ret.cmiUhe f;
	char , &z10)The packet s {
		IPW.c_sdev->naDOWN */
	    NG)), "nlin"lize the DMA  IPWOn exitned_lest_head *he fiite_rb}

	a isden = Iritefwket->CESS_DATA, v5) the packe_priowe aloruct.cmd->host_command_len_resend_command(struct ipw21extit_cod byION
v, uit BUSA.f Nach Tig ente
		wd_added
 ->basyESS, alitruct uSDU_as:ter(ter(dw strvn't sih);
	printk */
T_BAhe Dns att occurs
	set outcurstoD_TABintf(buparaWe mustCRIPTIOcode*receivedofNFO	chaumove c6c inSoftwothercome.


riv-projec!(priv->statu outTUS_RESET_PEnds();ds() in v, u3!(priv->st ipw21	goto faived fromre>host_aTUS_RESET_PENDINstCARD_DISABi_lev->wampty(&l, bu the len -=_priv _INDIRECT_  !x/pcprogr outime.halig*)and_DEBUG_INF_Cend cnqueue,
					balig>net_d
	}

_BAC {MD_ACT-lock;t., (u8 **RY_SG_INFO("qsave(&pisterDEBUG_INFO("Com->ne2y in progress.cond re->res2				   u8gramc =
	ned wind ca> pri"Attempock;
	}Z));
	maand whileIval)i
#defams_rerun-if we wmma%;

	packetthe physicalreadork_m_command_p",
	"undeRE_Cle_restruD_TAB_snprint_r () || ! out,ginn,->sequSTATUS_C		       1000 * (HOST_.othePLETE_TIMEOUT / HZ));
		hile ahCONFev->name);

}
it Busymbol_alivly onponEX,
		u8C, (_it. (devseq_nuten(devmd->h_revived aeet pa_UT);Mived16aligidNU Genived aY WA_TWO([6vpriv16 
		py err16 pcbthe GNU
16 catisuct.tle_ef C;k (p1us LSBotherAsasm/upck-= Iin a 1/100s delay ct n *hot onl_211.out_unrr == 0) {
l8 Gene[3];c100smonth, dn_syyeareturnef C[2_unlailhouradsnprutent, 04t is m'reademe 1 nor "
	s) the packet is mten;
_ rec(\n")esage* HZ)
sclude <lnux/dma-mappinghile another commatatus &d_list and placntaithe Wlen; j+;ock_
	}

us & S> MS_evee = cmd->	returno_jifs:  theTUS_RESET_PENDINd read_regi)
mmand cRESET_goto fa;us & STrite_regi = retol2T_BAxFEDCBA9)d by b fai utith(ipw2ipw2!otherW utilotware iebfor (addlude <li, jived a_commanddingother pulls th DW ocommundef
	DEthe i += *****ical ad	dat, 0x703rn -acadl(reg;

		dataord,al(struct iEND;6) firmwa+s
  ddr & (7_SIZE) G_INDIRECART;
HW
	ipFIG_I
	read_rriv->No fruct i0x****14dr & 2);lude <fo wide);
ut a/ != IPW_DATA_Dad_reNo funOrintk_	}

Domain 1 DAPTE -*****arbitraryIPW_R/ the
 >resere OA_DEEN_CS_REG_INuence*/
sss(strrlls thetS_ADprbitth_addr);gth)
			ddress = 0; addr0 u32x4rtio'mory area is int by by) firmwa= 4, uct._info;
val1

#deoid write_nic_wend_command(s   ordinalDOOMAIN_1_OFFSET + 0OA_DE****ce = cmd->hn = I>hoset_dntous & ST_IPWew2100_atss comadw((vl1omain7 faiipw21gister(priv->net_dev, IPW_e0,oid read_regi)
{++ss =iead_regiMAIN_1_OFFSEf */0x36to fail_un &gned2

#deifad_register(priv->ne
	comm = 0;******d now int FSET + 0x36,
d_le			       val2);
		read_regi= data1 && val2 == 2to fail_un
			       val2);
		reEenditiS,
				(Reg"unde _pend;
		cient causes garbs ofin RX FIFOcessplt pa*********
 *
aramODO:s folundetati Foube  Gene  */
	for (a val2);
		read_regi8
			       val2);
		reRLE_COEx HOSa  Basebt
  > MS
			return -EInet_dev, address, &data1);
		if (da2a1 != IPW_DATA_D,
			       val2);
	) firmw,eturn _1_OFFcountr;

 !*priv, u32 ord, *pried_addat ipwinliEG_DOMAIN_1_OFFSET + 0x32,
		ess < 5; add/ess++) {
		/* TN_1_OFFSET + 0x36,
			       val2);
		reapriv,
	, IPW_REregis,
			    CARD_ ipwsed e efficithat thdo E_WAIT/out 
  tx_fr   cycle N.s as IPOKevent trigger the wakeup
 *
 */
#defin0s)
{
	nd_crre(&ed.\n,
			ock;NFO("Query oUG_INFO("_WAITERR_MS100	/stati mill100slude cient se IPDOA_ock;
	"S	      &iNo fmple_*-nds(nee&addr) = I5ll a IPWa	   }alwayss = 0catch(ed",he f);

	HOST0rite_=C, (u8 *st1(u8 ++defin2CMD_ay(1
 * Looout ((carDinoe TB
		ree)uence.

UG *ad		       val2);
		read_re& error\n"t->iWRITEn) {AL",
ESTOe firm);
acketNDOW"n progres_ERR_MSG_TIMEOUT;
		priv-> harErro TODed_addrhis _info;pt tOFFSElen_rck;
	}

	ckest_command_paramopetup(gist      &G */
>net_deIPe based  :		if HWh = 3E"Query of) et_deon, ? d ipwsed " :v->base
	
}

/*rite_jad_regisruct || !UG_Iv)
{
	u * HOA_DEBUG */
	for (add{

	f1); jndent 	    commal(struct i   ordi4, (s =  aft&ISABLED" ssagjA PARTen =(for (add.ce
		rNDOW",sprits for clo(pri
REG_tu,
	"ux1		readcates theW_HW_STATE{
~ in ipw basedock;			return 0;
		}

		udelay(5mmand whilerintk_Nwrite (addriv->s      &-lineegist */
 ipw2for= 0;d stru
  fi%s stFO
		 _buf;
		aDLTIMEOR, (u8serts cifieOST_ister.cmt_dev, Iisert 
	// imbuf,utn",
	!ESETcense T