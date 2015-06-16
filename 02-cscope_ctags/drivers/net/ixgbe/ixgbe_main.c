/*******************************************************************************

  Intel 10 Gigabit PCI Express Linux driver
  Copyright(c) 1999 - 2009 Intel Corporation.

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

  Contact Information:
  e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/

#include <linux/types.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/pkt_sched.h>
#include <linux/ipv6.h>
#include <net/checksum.h>
#include <net/ip6_checksum.h>
#include <linux/ethtool.h>
#include <linux/if_vlan.h>
#include <scsi/fc/fc_fcoe.h>

#include "ixgbe.h"
#include "ixgbe_common.h"
#include "ixgbe_dcb_82599.h"

char ixgbe_driver_name[] = "ixgbe";
static const char ixgbe_driver_string[] =
                              "Intel(R) 10 Gigabit PCI Express Network Driver";

#define DRV_VERSION "2.0.44-k2"
const char ixgbe_driver_version[] = DRV_VERSION;
static char ixgbe_copyright[] = "Copyright (c) 1999-2009 Intel Corporation.";

static const struct ixgbe_info *ixgbe_info_tbl[] = {
	[board_82598] = &ixgbe_82598_info,
	[board_82599] = &ixgbe_82599_info,
};

/* ixgbe_pci_tbl - PCI Device ID Table
 *
 * Wildcard entries (PCI_ANY_ID) should come last
 * Last entry must be all 0s
 *
 * { Vendor ID, Device ID, SubVendor ID, SubDevice ID,
 *   Class, Class Mask, private data (not used) }
 */
static struct pci_device_id ixgbe_pci_tbl[] = {
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598),
	 board_82598 },
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598AF_DUAL_PORT),
	 board_82598 },
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598AF_SINGLE_PORT),
	 board_82598 },
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598AT),
	 board_82598 },
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598AT2),
	 board_82598 },
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598EB_CX4),
	 board_82598 },
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598_CX4_DUAL_PORT),
	 board_82598 },
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598_DA_DUAL_PORT),
	 board_82598 },
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598_SR_DUAL_PORT_EM),
	 board_82598 },
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598EB_XF_LR),
	 board_82598 },
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598EB_SFP_LOM),
	 board_82598 },
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598_BX),
	 board_82598 },
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_KX4),
	 board_82599 },
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_XAUI_LOM),
	 board_82599 },
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_SFP),
	 board_82599 },
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_KX4_MEZZ),
	 board_82599 },
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_CX4),
	 board_82599 },
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_COMBO_BACKPLANE),
	 board_82599 },

	/* required last entry */
	{0, }
};
MODULE_DEVICE_TABLE(pci, ixgbe_pci_tbl);

#ifdef CONFIG_IXGBE_DCA
static int ixgbe_notify_dca(struct notifier_block *, unsigned long event,
                            void *p);
static struct notifier_block dca_notifier = {
	.notifier_call = ixgbe_notify_dca,
	.next          = NULL,
	.priority      = 0
};
#endif

MODULE_AUTHOR("Intel Corporation, <linux.nics@intel.com>");
MODULE_DESCRIPTION("Intel(R) 10 Gigabit PCI Express Network Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);

#define DEFAULT_DEBUG_LEVEL_SHIFT 3

static void ixgbe_release_hw_control(struct ixgbe_adapter *adapter)
{
	u32 ctrl_ext;

	/* Let firmware take over control of h/w */
	ctrl_ext = IXGBE_READ_REG(&adapter->hw, IXGBE_CTRL_EXT);
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_CTRL_EXT,
	                ctrl_ext & ~IXGBE_CTRL_EXT_DRV_LOAD);
}

static void ixgbe_get_hw_control(struct ixgbe_adapter *adapter)
{
	u32 ctrl_ext;

	/* Let firmware know the driver has taken over */
	ctrl_ext = IXGBE_READ_REG(&adapter->hw, IXGBE_CTRL_EXT);
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_CTRL_EXT,
	                ctrl_ext | IXGBE_CTRL_EXT_DRV_LOAD);
}

/*
 * ixgbe_set_ivar - set the IVAR registers, mapping interrupt causes to vectors
 * @adapter: pointer to adapter struct
 * @direction: 0 for Rx, 1 for Tx, -1 for other causes
 * @queue: queue to map the corresponding interrupt to
 * @msix_vector: the vector to map to the corresponding queue
 *
 */
static void ixgbe_set_ivar(struct ixgbe_adapter *adapter, s8 direction,
	                   u8 queue, u8 msix_vector)
{
	u32 ivar, index;
	struct ixgbe_hw *hw = &adapter->hw;
	switch (hw->mac.type) {
	case ixgbe_mac_82598EB:
		msix_vector |= IXGBE_IVAR_ALLOC_VAL;
		if (direction == -1)
			direction = 0;
		index = (((direction * 64) + queue) >> 2) & 0x1F;
		ivar = IXGBE_READ_REG(hw, IXGBE_IVAR(index));
		ivar &= ~(0xFF << (8 * (queue & 0x3)));
		ivar |= (msix_vector << (8 * (queue & 0x3)));
		IXGBE_WRITE_REG(hw, IXGBE_IVAR(index), ivar);
		break;
	case ixgbe_mac_82599EB:
		if (direction == -1) {
			/* other causes */
			msix_vector |= IXGBE_IVAR_ALLOC_VAL;
			index = ((queue & 1) * 8);
			ivar = IXGBE_READ_REG(&adapter->hw, IXGBE_IVAR_MISC);
			ivar &= ~(0xFF << index);
			ivar |= (msix_vector << index);
			IXGBE_WRITE_REG(&adapter->hw, IXGBE_IVAR_MISC, ivar);
			break;
		} else {
			/* tx or rx causes */
			msix_vector |= IXGBE_IVAR_ALLOC_VAL;
			index = ((16 * (queue & 1)) + (8 * direction));
			ivar = IXGBE_READ_REG(hw, IXGBE_IVAR(queue >> 1));
			ivar &= ~(0xFF << index);
			ivar |= (msix_vector << index);
			IXGBE_WRITE_REG(hw, IXGBE_IVAR(queue >> 1), ivar);
			break;
		}
	default:
		break;
	}
}

static inline void ixgbe_irq_rearm_queues(struct ixgbe_adapter *adapter,
                                          u64 qmask)
{
	u32 mask;

	if (adapter->hw.mac.type == ixgbe_mac_82598EB) {
		mask = (IXGBE_EIMS_RTX_QUEUE & qmask);
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EICS, mask);
	} else {
		mask = (qmask & 0xFFFFFFFF);
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EICS_EX(0), mask);
		mask = (qmask >> 32);
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EICS_EX(1), mask);
	}
}

static void ixgbe_unmap_and_free_tx_resource(struct ixgbe_adapter *adapter,
                                             struct ixgbe_tx_buffer
                                             *tx_buffer_info)
{
	tx_buffer_info->dma = 0;
	if (tx_buffer_info->skb) {
		skb_dma_unmap(&adapter->pdev->dev, tx_buffer_info->skb,
		              DMA_TO_DEVICE);
		dev_kfree_skb_any(tx_buffer_info->skb);
		tx_buffer_info->skb = NULL;
	}
	tx_buffer_info->time_stamp = 0;
	/* tx_buffer_info must be completely set up in the transmit path */
}

/**
 * ixgbe_tx_is_paused - check if the tx ring is paused
 * @adapter: the ixgbe adapter
 * @tx_ring: the corresponding tx_ring
 *
 * If not in DCB mode, checks TFCS.TXOFF, otherwise, find out the
 * corresponding TC of this tx_ring when checking TFCS.
 *
 * Returns : true if paused
 */
static inline bool ixgbe_tx_is_paused(struct ixgbe_adapter *adapter,
                                      struct ixgbe_ring *tx_ring)
{
	u32 txoff = IXGBE_TFCS_TXOFF;

#ifdef CONFIG_IXGBE_DCB
	if (adapter->flags & IXGBE_FLAG_DCB_ENABLED) {
		int tc;
		int reg_idx = tx_ring->reg_idx;
		int dcb_i = adapter->ring_feature[RING_F_DCB].indices;

		if (adapter->hw.mac.type == ixgbe_mac_82598EB) {
			tc = reg_idx >> 2;
			txoff = IXGBE_TFCS_TXOFF0;
		} else if (adapter->hw.mac.type == ixgbe_mac_82599EB) {
			tc = 0;
			txoff = IXGBE_TFCS_TXOFF;
			if (dcb_i == 8) {
				/* TC0, TC1 */
				tc = reg_idx >> 5;
				if (tc == 2) /* TC2, TC3 */
					tc += (reg_idx - 64) >> 4;
				else if (tc == 3) /* TC4, TC5, TC6, TC7 */
					tc += 1 + ((reg_idx - 96) >> 3);
			} else if (dcb_i == 4) {
				/* TC0, TC1 */
				tc = reg_idx >> 6;
				if (tc == 1) {
					tc += (reg_idx - 64) >> 5;
					if (tc == 2) /* TC2, TC3 */
						tc += (reg_idx - 96) >> 4;
				}
			}
		}
		txoff <<= tc;
	}
#endif
	return IXGBE_READ_REG(&adapter->hw, IXGBE_TFCS) & txoff;
}

static inline bool ixgbe_check_tx_hang(struct ixgbe_adapter *adapter,
                                       struct ixgbe_ring *tx_ring,
                                       unsigned int eop)
{
	struct ixgbe_hw *hw = &adapter->hw;

	/* Detect a transmit hang in hardware, this serializes the
	 * check with the clearing of time_stamp and movement of eop */
	adapter->detect_tx_hung = false;
	if (tx_ring->tx_buffer_info[eop].time_stamp &&
	    time_after(jiffies, tx_ring->tx_buffer_info[eop].time_stamp + HZ) &&
	    !ixgbe_tx_is_paused(adapter, tx_ring)) {
		/* detected Tx unit hang */
		union ixgbe_adv_tx_desc *tx_desc;
		tx_desc = IXGBE_TX_DESC_ADV(*tx_ring, eop);
		DPRINTK(DRV, ERR, "Detected Tx Unit Hang\n"
			"  Tx Queue             <%d>\n"
			"  TDH, TDT             <%x>, <%x>\n"
			"  next_to_use          <%x>\n"
			"  next_to_clean        <%x>\n"
			"tx_buffer_info[next_to_clean]\n"
			"  time_stamp           <%lx>\n"
			"  jiffies              <%lx>\n",
			tx_ring->queue_index,
			IXGBE_READ_REG(hw, tx_ring->head),
			IXGBE_READ_REG(hw, tx_ring->tail),
			tx_ring->next_to_use, eop,
			tx_ring->tx_buffer_info[eop].time_stamp, jiffies);
		return true;
	}

	return false;
}

#define IXGBE_MAX_TXD_PWR       14
#define IXGBE_MAX_DATA_PER_TXD  (1 << IXGBE_MAX_TXD_PWR)

/* Tx Descriptors needed, worst case */
#define TXD_USE_COUNT(S) (((S) >> IXGBE_MAX_TXD_PWR) + \
			 (((S) & (IXGBE_MAX_DATA_PER_TXD - 1)) ? 1 : 0))
#define DESC_NEEDED (TXD_USE_COUNT(IXGBE_MAX_DATA_PER_TXD) /* skb->data */ + \
	MAX_SKB_FRAGS * TXD_USE_COUNT(PAGE_SIZE) + 1) /* for context */

static void ixgbe_tx_timeout(struct net_device *netdev);

/**
 * ixgbe_clean_tx_irq - Reclaim resources after transmit completes
 * @q_vector: structure containing interrupt and ring information
 * @tx_ring: tx ring to clean
 **/
static bool ixgbe_clean_tx_irq(struct ixgbe_q_vector *q_vector,
                               struct ixgbe_ring *tx_ring)
{
	struct ixgbe_adapter *adapter = q_vector->adapter;
	struct net_device *netdev = adapter->netdev;
	union ixgbe_adv_tx_desc *tx_desc, *eop_desc;
	struct ixgbe_tx_buffer *tx_buffer_info;
	unsigned int i, eop, count = 0;
	unsigned int total_bytes = 0, total_packets = 0;

	i = tx_ring->next_to_clean;
	eop = tx_ring->tx_buffer_info[i].next_to_watch;
	eop_desc = IXGBE_TX_DESC_ADV(*tx_ring, eop);

	while ((eop_desc->wb.status & cpu_to_le32(IXGBE_TXD_STAT_DD)) &&
	       (count < tx_ring->work_limit)) {
		bool cleaned = false;
		for ( ; !cleaned; count++) {
			struct sk_buff *skb;
			tx_desc = IXGBE_TX_DESC_ADV(*tx_ring, i);
			tx_buffer_info = &tx_ring->tx_buffer_info[i];
			cleaned = (i == eop);
			skb = tx_buffer_info->skb;

			if (cleaned && skb) {
				unsigned int segs, bytecount;
				unsigned int hlen = skb_headlen(skb);

				/* gso_segs is currently only valid for tcp */
				segs = skb_shinfo(skb)->gso_segs ?: 1;
#ifdef IXGBE_FCOE
				/* adjust for FCoE Sequence Offload */
				if ((adapter->flags & IXGBE_FLAG_FCOE_ENABLED)
				    && (skb->protocol == htons(ETH_P_FCOE)) &&
				    skb_is_gso(skb)) {
					hlen = skb_transport_offset(skb) +
						sizeof(struct fc_frame_header) +
						sizeof(struct fcoe_crc_eof);
					segs = DIV_ROUND_UP(skb->len - hlen,
						skb_shinfo(skb)->gso_size);
				}
#endif /* IXGBE_FCOE */
				/* multiply data chunks by size of headers */
				bytecount = ((segs - 1) * hlen) + skb->len;
				total_packets += segs;
				total_bytes += bytecount;
			}

			ixgbe_unmap_and_free_tx_resource(adapter,
			                                 tx_buffer_info);

			tx_desc->wb.status = 0;

			i++;
			if (i == tx_ring->count)
				i = 0;
		}

		eop = tx_ring->tx_buffer_info[i].next_to_watch;
		eop_desc = IXGBE_TX_DESC_ADV(*tx_ring, eop);
	}

	tx_ring->next_to_clean = i;

#define TX_WAKE_THRESHOLD (DESC_NEEDED * 2)
	if (unlikely(count && netif_carrier_ok(netdev) &&
	             (IXGBE_DESC_UNUSED(tx_ring) >= TX_WAKE_THRESHOLD))) {
		/* Make sure that anybody stopping the queue after this
		 * sees the new next_to_clean.
		 */
		smp_mb();
		if (__netif_subqueue_stopped(netdev, tx_ring->queue_index) &&
		    !test_bit(__IXGBE_DOWN, &adapter->state)) {
			netif_wake_subqueue(netdev, tx_ring->queue_index);
			++adapter->restart_queue;
		}
	}

	if (adapter->detect_tx_hung) {
		if (ixgbe_check_tx_hang(adapter, tx_ring, i)) {
			/* schedule immediate reset if we believe we hung */
			DPRINTK(PROBE, INFO,
			        "tx hang %d detected, resetting adapter\n",
			        adapter->tx_timeout_count + 1);
			ixgbe_tx_timeout(adapter->netdev);
		}
	}

	/* re-arm the interrupt */
	if (count >= tx_ring->work_limit)
		ixgbe_irq_rearm_queues(adapter, ((u64)1 << q_vector->v_idx));

	tx_ring->total_bytes += total_bytes;
	tx_ring->total_packets += total_packets;
	tx_ring->stats.packets += total_packets;
	tx_ring->stats.bytes += total_bytes;
	adapter->net_stats.tx_bytes += total_bytes;
	adapter->net_stats.tx_packets += total_packets;
	return (count < tx_ring->work_limit);
}

#ifdef CONFIG_IXGBE_DCA
static void ixgbe_update_rx_dca(struct ixgbe_adapter *adapter,
                                struct ixgbe_ring *rx_ring)
{
	u32 rxctrl;
	int cpu = get_cpu();
	int q = rx_ring - adapter->rx_ring;

	if (rx_ring->cpu != cpu) {
		rxctrl = IXGBE_READ_REG(&adapter->hw, IXGBE_DCA_RXCTRL(q));
		if (adapter->hw.mac.type == ixgbe_mac_82598EB) {
			rxctrl &= ~IXGBE_DCA_RXCTRL_CPUID_MASK;
			rxctrl |= dca3_get_tag(&adapter->pdev->dev, cpu);
		} else if (adapter->hw.mac.type == ixgbe_mac_82599EB) {
			rxctrl &= ~IXGBE_DCA_RXCTRL_CPUID_MASK_82599;
			rxctrl |= (dca3_get_tag(&adapter->pdev->dev, cpu) <<
			           IXGBE_DCA_RXCTRL_CPUID_SHIFT_82599);
		}
		rxctrl |= IXGBE_DCA_RXCTRL_DESC_DCA_EN;
		rxctrl |= IXGBE_DCA_RXCTRL_HEAD_DCA_EN;
		rxctrl &= ~(IXGBE_DCA_RXCTRL_DESC_RRO_EN);
		rxctrl &= ~(IXGBE_DCA_RXCTRL_DESC_WRO_EN |
		            IXGBE_DCA_RXCTRL_DESC_HSRO_EN);
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_DCA_RXCTRL(q), rxctrl);
		rx_ring->cpu = cpu;
	}
	put_cpu();
}

static void ixgbe_update_tx_dca(struct ixgbe_adapter *adapter,
                                struct ixgbe_ring *tx_ring)
{
	u32 txctrl;
	int cpu = get_cpu();
	int q = tx_ring - adapter->tx_ring;
	struct ixgbe_hw *hw = &adapter->hw;

	if (tx_ring->cpu != cpu) {
		if (adapter->hw.mac.type == ixgbe_mac_82598EB) {
			txctrl = IXGBE_READ_REG(hw, IXGBE_DCA_TXCTRL(q));
			txctrl &= ~IXGBE_DCA_TXCTRL_CPUID_MASK;
			txctrl |= dca3_get_tag(&adapter->pdev->dev, cpu);
			txctrl |= IXGBE_DCA_TXCTRL_DESC_DCA_EN;
			IXGBE_WRITE_REG(hw, IXGBE_DCA_TXCTRL(q), txctrl);
		} else if (adapter->hw.mac.type == ixgbe_mac_82599EB) {
			txctrl = IXGBE_READ_REG(hw, IXGBE_DCA_TXCTRL_82599(q));
			txctrl &= ~IXGBE_DCA_TXCTRL_CPUID_MASK_82599;
			txctrl |= (dca3_get_tag(&adapter->pdev->dev, cpu) <<
			          IXGBE_DCA_TXCTRL_CPUID_SHIFT_82599);
			txctrl |= IXGBE_DCA_TXCTRL_DESC_DCA_EN;
			IXGBE_WRITE_REG(hw, IXGBE_DCA_TXCTRL_82599(q), txctrl);
		}
		tx_ring->cpu = cpu;
	}
	put_cpu();
}

static void ixgbe_setup_dca(struct ixgbe_adapter *adapter)
{
	int i;

	if (!(adapter->flags & IXGBE_FLAG_DCA_ENABLED))
		return;

	/* always use CB2 mode, difference is masked in the CB driver */
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_DCA_CTRL, 2);

	for (i = 0; i < adapter->num_tx_queues; i++) {
		adapter->tx_ring[i].cpu = -1;
		ixgbe_update_tx_dca(adapter, &adapter->tx_ring[i]);
	}
	for (i = 0; i < adapter->num_rx_queues; i++) {
		adapter->rx_ring[i].cpu = -1;
		ixgbe_update_rx_dca(adapter, &adapter->rx_ring[i]);
	}
}

static int __ixgbe_notify_dca(struct device *dev, void *data)
{
	struct net_device *netdev = dev_get_drvdata(dev);
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	unsigned long event = *(unsigned long *)data;

	switch (event) {
	case DCA_PROVIDER_ADD:
		/* if we're already enabled, don't do it again */
		if (adapter->flags & IXGBE_FLAG_DCA_ENABLED)
			break;
		if (dca_add_requester(dev) == 0) {
			adapter->flags |= IXGBE_FLAG_DCA_ENABLED;
			ixgbe_setup_dca(adapter);
			break;
		}
		/* Fall Through since DCA is disabled. */
	case DCA_PROVIDER_REMOVE:
		if (adapter->flags & IXGBE_FLAG_DCA_ENABLED) {
			dca_remove_requester(dev);
			adapter->flags &= ~IXGBE_FLAG_DCA_ENABLED;
			IXGBE_WRITE_REG(&adapter->hw, IXGBE_DCA_CTRL, 1);
		}
		break;
	}

	return 0;
}

#endif /* CONFIG_IXGBE_DCA */
/**
 * ixgbe_receive_skb - Send a completed packet up the stack
 * @adapter: board private structure
 * @skb: packet to send up
 * @status: hardware indication of status of receive
 * @rx_ring: rx descriptor ring (for a specific queue) to setup
 * @rx_desc: rx descriptor
 **/
static void ixgbe_receive_skb(struct ixgbe_q_vector *q_vector,
                              struct sk_buff *skb, u8 status,
                              struct ixgbe_ring *ring,
                              union ixgbe_adv_rx_desc *rx_desc)
{
	struct ixgbe_adapter *adapter = q_vector->adapter;
	struct napi_struct *napi = &q_vector->napi;
	bool is_vlan = (status & IXGBE_RXD_STAT_VP);
	u16 tag = le16_to_cpu(rx_desc->wb.upper.vlan);

	skb_record_rx_queue(skb, ring->queue_index);
	if (!(adapter->flags & IXGBE_FLAG_IN_NETPOLL)) {
		if (adapter->vlgrp && is_vlan && (tag & VLAN_VID_MASK))
			vlan_gro_receive(napi, adapter->vlgrp, tag, skb);
		else
			napi_gro_receive(napi, skb);
	} else {
		if (adapter->vlgrp && is_vlan && (tag & VLAN_VID_MASK))
			vlan_hwaccel_rx(skb, adapter->vlgrp, tag);
		else
			netif_rx(skb);
	}
}

/**
 * ixgbe_rx_checksum - indicate in skb if hw indicated a good cksum
 * @adapter: address of board private structure
 * @status_err: hardware indication of status of receive
 * @skb: skb currently being received and modified
 **/
static inline void ixgbe_rx_checksum(struct ixgbe_adapter *adapter,
				     union ixgbe_adv_rx_desc *rx_desc,
				     struct sk_buff *skb)
{
	u32 status_err = le32_to_cpu(rx_desc->wb.upper.status_error);

	skb->ip_summed = CHECKSUM_NONE;

	/* Rx csum disabled */
	if (!(adapter->flags & IXGBE_FLAG_RX_CSUM_ENABLED))
		return;

	/* if IP and error */
	if ((status_err & IXGBE_RXD_STAT_IPCS) &&
	    (status_err & IXGBE_RXDADV_ERR_IPE)) {
		adapter->hw_csum_rx_error++;
		return;
	}

	if (!(status_err & IXGBE_RXD_STAT_L4CS))
		return;

	if (status_err & IXGBE_RXDADV_ERR_TCPE) {
		u16 pkt_info = rx_desc->wb.lower.lo_dword.hs_rss.pkt_info;

		/*
		 * 82599 errata, UDP frames with a 0 checksum can be marked as
		 * checksum errors.
		 */
		if ((pkt_info & IXGBE_RXDADV_PKTTYPE_UDP) &&
		    (adapter->hw.mac.type == ixgbe_mac_82599EB))
			return;

		adapter->hw_csum_rx_error++;
		return;
	}

	/* It must be a TCP or UDP packet with a valid checksum */
	skb->ip_summed = CHECKSUM_UNNECESSARY;
	adapter->hw_csum_rx_good++;
}

static inline void ixgbe_release_rx_desc(struct ixgbe_hw *hw,
                                         struct ixgbe_ring *rx_ring, u32 val)
{
	/*
	 * Force memory writes to complete before letting h/w
	 * know there are new descriptors to fetch.  (Only
	 * applicable for weak-ordered memory model archs,
	 * such as IA-64).
	 */
	wmb();
	IXGBE_WRITE_REG(hw, IXGBE_RDT(rx_ring->reg_idx), val);
}

/**
 * ixgbe_alloc_rx_buffers - Replace used receive buffers; packet split
 * @adapter: address of board private structure
 **/
static void ixgbe_alloc_rx_buffers(struct ixgbe_adapter *adapter,
                                   struct ixgbe_ring *rx_ring,
                                   int cleaned_count)
{
	struct pci_dev *pdev = adapter->pdev;
	union ixgbe_adv_rx_desc *rx_desc;
	struct ixgbe_rx_buffer *bi;
	unsigned int i;

	i = rx_ring->next_to_use;
	bi = &rx_ring->rx_buffer_info[i];

	while (cleaned_count--) {
		rx_desc = IXGBE_RX_DESC_ADV(*rx_ring, i);

		if (!bi->page_dma &&
		    (rx_ring->flags & IXGBE_RING_RX_PS_ENABLED)) {
			if (!bi->page) {
				bi->page = alloc_page(GFP_ATOMIC);
				if (!bi->page) {
					adapter->alloc_rx_page_failed++;
					goto no_buffers;
				}
				bi->page_offset = 0;
			} else {
				/* use a half page if we're re-using */
				bi->page_offset ^= (PAGE_SIZE / 2);
			}

			bi->page_dma = pci_map_page(pdev, bi->page,
			                            bi->page_offset,
			                            (PAGE_SIZE / 2),
			                            PCI_DMA_FROMDEVICE);
		}

		if (!bi->skb) {
			struct sk_buff *skb;
			skb = netdev_alloc_skb(adapter->netdev,
			                       (rx_ring->rx_buf_len +
			                        NET_IP_ALIGN));

			if (!skb) {
				adapter->alloc_rx_buff_failed++;
				goto no_buffers;
			}

			/*
			 * Make buffer alignment 2 beyond a 16 byte boundary
			 * this will result in a 16 byte aligned IP header after
			 * the 14 byte MAC header is removed
			 */
			skb_reserve(skb, NET_IP_ALIGN);

			bi->skb = skb;
			bi->dma = pci_map_single(pdev, skb->data,
			                         rx_ring->rx_buf_len,
			                         PCI_DMA_FROMDEVICE);
		}
		/* Refresh the desc even if buffer_addrs didn't change because
		 * each write-back erases this info. */
		if (rx_ring->flags & IXGBE_RING_RX_PS_ENABLED) {
			rx_desc->read.pkt_addr = cpu_to_le64(bi->page_dma);
			rx_desc->read.hdr_addr = cpu_to_le64(bi->dma);
		} else {
			rx_desc->read.pkt_addr = cpu_to_le64(bi->dma);
		}

		i++;
		if (i == rx_ring->count)
			i = 0;
		bi = &rx_ring->rx_buffer_info[i];
	}

no_buffers:
	if (rx_ring->next_to_use != i) {
		rx_ring->next_to_use = i;
		if (i-- == 0)
			i = (rx_ring->count - 1);

		ixgbe_release_rx_desc(&adapter->hw, rx_ring, i);
	}
}

static inline u16 ixgbe_get_hdr_info(union ixgbe_adv_rx_desc *rx_desc)
{
	return rx_desc->wb.lower.lo_dword.hs_rss.hdr_info;
}

static inline u16 ixgbe_get_pkt_info(union ixgbe_adv_rx_desc *rx_desc)
{
	return rx_desc->wb.lower.lo_dword.hs_rss.pkt_info;
}

static inline u32 ixgbe_get_rsc_count(union ixgbe_adv_rx_desc *rx_desc)
{
	return (le32_to_cpu(rx_desc->wb.lower.lo_dword.data) &
	        IXGBE_RXDADV_RSCCNT_MASK) >>
	        IXGBE_RXDADV_RSCCNT_SHIFT;
}

/**
 * ixgbe_transform_rsc_queue - change rsc queue into a full packet
 * @skb: pointer to the last skb in the rsc queue
 *
 * This function changes a queue full of hw rsc buffers into a completed
 * packet.  It uses the ->prev pointers to find the first packet and then
 * turns it into the frag list owner.
 **/
static inline struct sk_buff *ixgbe_transform_rsc_queue(struct sk_buff *skb)
{
	unsigned int frag_list_size = 0;

	while (skb->prev) {
		struct sk_buff *prev = skb->prev;
		frag_list_size += skb->len;
		skb->prev = NULL;
		skb = prev;
	}

	skb_shinfo(skb)->frag_list = skb->next;
	skb->next = NULL;
	skb->len += frag_list_size;
	skb->data_len += frag_list_size;
	skb->truesize += frag_list_size;
	return skb;
}

static bool ixgbe_clean_rx_irq(struct ixgbe_q_vector *q_vector,
                               struct ixgbe_ring *rx_ring,
                               int *work_done, int work_to_do)
{
	struct ixgbe_adapter *adapter = q_vector->adapter;
	struct pci_dev *pdev = adapter->pdev;
	union ixgbe_adv_rx_desc *rx_desc, *next_rxd;
	struct ixgbe_rx_buffer *rx_buffer_info, *next_buffer;
	struct sk_buff *skb;
	unsigned int i, rsc_count = 0;
	u32 len, staterr;
	u16 hdr_info;
	bool cleaned = false;
	int cleaned_count = 0;
	unsigned int total_rx_bytes = 0, total_rx_packets = 0;
#ifdef IXGBE_FCOE
	int ddp_bytes = 0;
#endif /* IXGBE_FCOE */

	i = rx_ring->next_to_clean;
	rx_desc = IXGBE_RX_DESC_ADV(*rx_ring, i);
	staterr = le32_to_cpu(rx_desc->wb.upper.status_error);
	rx_buffer_info = &rx_ring->rx_buffer_info[i];

	while (staterr & IXGBE_RXD_STAT_DD) {
		u32 upper_len = 0;
		if (*work_done >= work_to_do)
			break;
		(*work_done)++;

		if (rx_ring->flags & IXGBE_RING_RX_PS_ENABLED) {
			hdr_info = le16_to_cpu(ixgbe_get_hdr_info(rx_desc));
			len = (hdr_info & IXGBE_RXDADV_HDRBUFLEN_MASK) >>
			       IXGBE_RXDADV_HDRBUFLEN_SHIFT;
			if (hdr_info & IXGBE_RXDADV_SPH)
				adapter->rx_hdr_split++;
			if (len > IXGBE_RX_HDR_SIZE)
				len = IXGBE_RX_HDR_SIZE;
			upper_len = le16_to_cpu(rx_desc->wb.upper.length);
		} else {
			len = le16_to_cpu(rx_desc->wb.upper.length);
		}

		cleaned = true;
		skb = rx_buffer_info->skb;
		prefetch(skb->data - NET_IP_ALIGN);
		rx_buffer_info->skb = NULL;

		if (rx_buffer_info->dma) {
			pci_unmap_single(pdev, rx_buffer_info->dma,
			                 rx_ring->rx_buf_len,
			                 PCI_DMA_FROMDEVICE);
			rx_buffer_info->dma = 0;
			skb_put(skb, len);
		}

		if (upper_len) {
			pci_unmap_page(pdev, rx_buffer_info->page_dma,
			               PAGE_SIZE / 2, PCI_DMA_FROMDEVICE);
			rx_buffer_info->page_dma = 0;
			skb_fill_page_desc(skb, skb_shinfo(skb)->nr_frags,
			                   rx_buffer_info->page,
			                   rx_buffer_info->page_offset,
			                   upper_len);

			if ((rx_ring->rx_buf_len > (PAGE_SIZE / 2)) ||
			    (page_count(rx_buffer_info->page) != 1))
				rx_buffer_info->page = NULL;
			else
				get_page(rx_buffer_info->page);

			skb->len += upper_len;
			skb->data_len += upper_len;
			skb->truesize += upper_len;
		}

		i++;
		if (i == rx_ring->count)
			i = 0;

		next_rxd = IXGBE_RX_DESC_ADV(*rx_ring, i);
		prefetch(next_rxd);
		cleaned_count++;

		if (adapter->flags2 & IXGBE_FLAG2_RSC_CAPABLE)
			rsc_count = ixgbe_get_rsc_count(rx_desc);

		if (rsc_count) {
			u32 nextp = (staterr & IXGBE_RXDADV_NEXTP_MASK) >>
				     IXGBE_RXDADV_NEXTP_SHIFT;
			next_buffer = &rx_ring->rx_buffer_info[nextp];
			rx_ring->rsc_count += (rsc_count - 1);
		} else {
			next_buffer = &rx_ring->rx_buffer_info[i];
		}

		if (staterr & IXGBE_RXD_STAT_EOP) {
			if (skb->prev)
				skb = ixgbe_transform_rsc_queue(skb);
			rx_ring->stats.packets++;
			rx_ring->stats.bytes += skb->len;
		} else {
			if (rx_ring->flags & IXGBE_RING_RX_PS_ENABLED) {
				rx_buffer_info->skb = next_buffer->skb;
				rx_buffer_info->dma = next_buffer->dma;
				next_buffer->skb = skb;
				next_buffer->dma = 0;
			} else {
				skb->next = next_buffer->skb;
				skb->next->prev = skb;
			}
			adapter->non_eop_descs++;
			goto next_desc;
		}

		if (staterr & IXGBE_RXDADV_ERR_FRAME_ERR_MASK) {
			dev_kfree_skb_irq(skb);
			goto next_desc;
		}

		ixgbe_rx_checksum(adapter, rx_desc, skb);

		/* probably a little skewed due to removing CRC */
		total_rx_bytes += skb->len;
		total_rx_packets++;

		skb->protocol = eth_type_trans(skb, adapter->netdev);
#ifdef IXGBE_FCOE
		/* if ddp, not passing to ULD unless for FCP_RSP or error */
		if (adapter->flags & IXGBE_FLAG_FCOE_ENABLED) {
			ddp_bytes = ixgbe_fcoe_ddp(adapter, rx_desc, skb);
			if (!ddp_bytes)
				goto next_desc;
		}
#endif /* IXGBE_FCOE */
		ixgbe_receive_skb(q_vector, skb, staterr, rx_ring, rx_desc);

next_desc:
		rx_desc->wb.upper.status_error = 0;

		/* return some buffers to hardware, one at a time is too slow */
		if (cleaned_count >= IXGBE_RX_BUFFER_WRITE) {
			ixgbe_alloc_rx_buffers(adapter, rx_ring, cleaned_count);
			cleaned_count = 0;
		}

		/* use prefetched values */
		rx_desc = next_rxd;
		rx_buffer_info = &rx_ring->rx_buffer_info[i];

		staterr = le32_to_cpu(rx_desc->wb.upper.status_error);
	}

	rx_ring->next_to_clean = i;
	cleaned_count = IXGBE_DESC_UNUSED(rx_ring);

	if (cleaned_count)
		ixgbe_alloc_rx_buffers(adapter, rx_ring, cleaned_count);

#ifdef IXGBE_FCOE
	/* include DDPed FCoE data */
	if (ddp_bytes > 0) {
		unsigned int mss;

		mss = adapter->netdev->mtu - sizeof(struct fcoe_hdr) -
			sizeof(struct fc_frame_header) -
			sizeof(struct fcoe_crc_eof);
		if (mss > 512)
			mss &= ~511;
		total_rx_bytes += ddp_bytes;
		total_rx_packets += DIV_ROUND_UP(ddp_bytes, mss);
	}
#endif /* IXGBE_FCOE */

	rx_ring->total_packets += total_rx_packets;
	rx_ring->total_bytes += total_rx_bytes;
	adapter->net_stats.rx_bytes += total_rx_bytes;
	adapter->net_stats.rx_packets += total_rx_packets;

	return cleaned;
}

static int ixgbe_clean_rxonly(struct napi_struct *, int);
/**
 * ixgbe_configure_msix - Configure MSI-X hardware
 * @adapter: board private structure
 *
 * ixgbe_configure_msix sets up the hardware to properly generate MSI-X
 * interrupts.
 **/
static void ixgbe_configure_msix(struct ixgbe_adapter *adapter)
{
	struct ixgbe_q_vector *q_vector;
	int i, j, q_vectors, v_idx, r_idx;
	u32 mask;

	q_vectors = adapter->num_msix_vectors - NON_Q_VECTORS;

	/*
	 * Populate the IVAR table and set the ITR values to the
	 * corresponding register.
	 */
	for (v_idx = 0; v_idx < q_vectors; v_idx++) {
		q_vector = adapter->q_vector[v_idx];
		/* XXX for_each_bit(...) */
		r_idx = find_first_bit(q_vector->rxr_idx,
		                       adapter->num_rx_queues);

		for (i = 0; i < q_vector->rxr_count; i++) {
			j = adapter->rx_ring[r_idx].reg_idx;
			ixgbe_set_ivar(adapter, 0, j, v_idx);
			r_idx = find_next_bit(q_vector->rxr_idx,
			                      adapter->num_rx_queues,
			                      r_idx + 1);
		}
		r_idx = find_first_bit(q_vector->txr_idx,
		                       adapter->num_tx_queues);

		for (i = 0; i < q_vector->txr_count; i++) {
			j = adapter->tx_ring[r_idx].reg_idx;
			ixgbe_set_ivar(adapter, 1, j, v_idx);
			r_idx = find_next_bit(q_vector->txr_idx,
			                      adapter->num_tx_queues,
			                      r_idx + 1);
		}

		if (q_vector->txr_count && !q_vector->rxr_count)
			/* tx only */
			q_vector->eitr = adapter->tx_eitr_param;
		else if (q_vector->rxr_count)
			/* rx or mixed */
			q_vector->eitr = adapter->rx_eitr_param;

		ixgbe_write_eitr(q_vector);
	}

	if (adapter->hw.mac.type == ixgbe_mac_82598EB)
		ixgbe_set_ivar(adapter, -1, IXGBE_IVAR_OTHER_CAUSES_INDEX,
		               v_idx);
	else if (adapter->hw.mac.type == ixgbe_mac_82599EB)
		ixgbe_set_ivar(adapter, -1, 1, v_idx);
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_EITR(v_idx), 1950);

	/* set up to autoclear timer, and the vectors */
	mask = IXGBE_EIMS_ENABLE_MASK;
	mask &= ~(IXGBE_EIMS_OTHER | IXGBE_EIMS_LSC);
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_EIAC, mask);
}

enum latency_range {
	lowest_latency = 0,
	low_latency = 1,
	bulk_latency = 2,
	latency_invalid = 255
};

/**
 * ixgbe_update_itr - update the dynamic ITR value based on statistics
 * @adapter: pointer to adapter
 * @eitr: eitr setting (ints per sec) to give last timeslice
 * @itr_setting: current throttle rate in ints/second
 * @packets: the number of packets during this measurement interval
 * @bytes: the number of bytes during this measurement interval
 *
 *      Stores a new ITR value based on packets and byte
 *      counts during the last interrupt.  The advantage of per interrupt
 *      computation is faster updates and more accurate ITR for the current
 *      traffic pattern.  Constants in this function were computed
 *      based on theoretical maximum wire speed and thresholds were set based
 *      on testing data as well as attempting to minimize response time
 *      while increasing bulk throughput.
 *      this functionality is controlled by the InterruptThrottleRate module
 *      parameter (see ixgbe_param.c)
 **/
static u8 ixgbe_update_itr(struct ixgbe_adapter *adapter,
                           u32 eitr, u8 itr_setting,
                           int packets, int bytes)
{
	unsigned int retval = itr_setting;
	u32 timepassed_us;
	u64 bytes_perint;

	if (packets == 0)
		goto update_itr_done;


	/* simple throttlerate management
	 *    0-20MB/s lowest (100000 ints/s)
	 *   20-100MB/s low   (20000 ints/s)
	 *  100-1249MB/s bulk (8000 ints/s)
	 */
	/* what was last interrupt timeslice? */
	timepassed_us = 1000000/eitr;
	bytes_perint = bytes / timepassed_us; /* bytes/usec */

	switch (itr_setting) {
	case lowest_latency:
		if (bytes_perint > adapter->eitr_low)
			retval = low_latency;
		break;
	case low_latency:
		if (bytes_perint > adapter->eitr_high)
			retval = bulk_latency;
		else if (bytes_perint <= adapter->eitr_low)
			retval = lowest_latency;
		break;
	case bulk_latency:
		if (bytes_perint <= adapter->eitr_high)
			retval = low_latency;
		break;
	}

update_itr_done:
	return retval;
}

/**
 * ixgbe_write_eitr - write EITR register in hardware specific way
 * @q_vector: structure containing interrupt and ring information
 *
 * This function is made to be called by ethtool and by the driver
 * when it needs to update EITR registers at runtime.  Hardware
 * specific quirks/differences are taken care of here.
 */
void ixgbe_write_eitr(struct ixgbe_q_vector *q_vector)
{
	struct ixgbe_adapter *adapter = q_vector->adapter;
	struct ixgbe_hw *hw = &adapter->hw;
	int v_idx = q_vector->v_idx;
	u32 itr_reg = EITR_INTS_PER_SEC_TO_REG(q_vector->eitr);

	if (adapter->hw.mac.type == ixgbe_mac_82598EB) {
		/* must write high and low 16 bits to reset counter */
		itr_reg |= (itr_reg << 16);
	} else if (adapter->hw.mac.type == ixgbe_mac_82599EB) {
		/*
		 * set the WDIS bit to not clear the timer bits and cause an
		 * immediate assertion of the interrupt
		 */
		itr_reg |= IXGBE_EITR_CNT_WDIS;
	}
	IXGBE_WRITE_REG(hw, IXGBE_EITR(v_idx), itr_reg);
}

static void ixgbe_set_itr_msix(struct ixgbe_q_vector *q_vector)
{
	struct ixgbe_adapter *adapter = q_vector->adapter;
	u32 new_itr;
	u8 current_itr, ret_itr;
	int i, r_idx;
	struct ixgbe_ring *rx_ring, *tx_ring;

	r_idx = find_first_bit(q_vector->txr_idx, adapter->num_tx_queues);
	for (i = 0; i < q_vector->txr_count; i++) {
		tx_ring = &(adapter->tx_ring[r_idx]);
		ret_itr = ixgbe_update_itr(adapter, q_vector->eitr,
		                           q_vector->tx_itr,
		                           tx_ring->total_packets,
		                           tx_ring->total_bytes);
		/* if the result for this queue would decrease interrupt
		 * rate for this vector then use that result */
		q_vector->tx_itr = ((q_vector->tx_itr > ret_itr) ?
		                    q_vector->tx_itr - 1 : ret_itr);
		r_idx = find_next_bit(q_vector->txr_idx, adapter->num_tx_queues,
		                      r_idx + 1);
	}

	r_idx = find_first_bit(q_vector->rxr_idx, adapter->num_rx_queues);
	for (i = 0; i < q_vector->rxr_count; i++) {
		rx_ring = &(adapter->rx_ring[r_idx]);
		ret_itr = ixgbe_update_itr(adapter, q_vector->eitr,
		                           q_vector->rx_itr,
		                           rx_ring->total_packets,
		                           rx_ring->total_bytes);
		/* if the result for this queue would decrease interrupt
		 * rate for this vector then use that result */
		q_vector->rx_itr = ((q_vector->rx_itr > ret_itr) ?
		                    q_vector->rx_itr - 1 : ret_itr);
		r_idx = find_next_bit(q_vector->rxr_idx, adapter->num_rx_queues,
		                      r_idx + 1);
	}

	current_itr = max(q_vector->rx_itr, q_vector->tx_itr);

	switch (current_itr) {
	/* counts and packets in update_itr are dependent on these numbers */
	case lowest_latency:
		new_itr = 100000;
		break;
	case low_latency:
		new_itr = 20000; /* aka hwitr = ~200 */
		break;
	case bulk_latency:
	default:
		new_itr = 8000;
		break;
	}

	if (new_itr != q_vector->eitr) {
		/* do an exponential smoothing */
		new_itr = ((q_vector->eitr * 90)/100) + ((new_itr * 10)/100);

		/* save the algorithm value here, not the smoothed one */
		q_vector->eitr = new_itr;

		ixgbe_write_eitr(q_vector);
	}

	return;
}

static void ixgbe_check_fan_failure(struct ixgbe_adapter *adapter, u32 eicr)
{
	struct ixgbe_hw *hw = &adapter->hw;

	if ((adapter->flags & IXGBE_FLAG_FAN_FAIL_CAPABLE) &&
	    (eicr & IXGBE_EICR_GPI_SDP1)) {
		DPRINTK(PROBE, CRIT, "Fan has stopped, replace the adapter\n");
		/* write to clear the interrupt */
		IXGBE_WRITE_REG(hw, IXGBE_EICR, IXGBE_EICR_GPI_SDP1);
	}
}

static void ixgbe_check_sfp_event(struct ixgbe_adapter *adapter, u32 eicr)
{
	struct ixgbe_hw *hw = &adapter->hw;

	if (eicr & IXGBE_EICR_GPI_SDP1) {
		/* Clear the interrupt */
		IXGBE_WRITE_REG(hw, IXGBE_EICR, IXGBE_EICR_GPI_SDP1);
		schedule_work(&adapter->multispeed_fiber_task);
	} else if (eicr & IXGBE_EICR_GPI_SDP2) {
		/* Clear the interrupt */
		IXGBE_WRITE_REG(hw, IXGBE_EICR, IXGBE_EICR_GPI_SDP2);
		schedule_work(&adapter->sfp_config_module_task);
	} else {
		/* Interrupt isn't for us... */
		return;
	}
}

static void ixgbe_check_lsc(struct ixgbe_adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;

	adapter->lsc_int++;
	adapter->flags |= IXGBE_FLAG_NEED_LINK_UPDATE;
	adapter->link_check_timeout = jiffies;
	if (!test_bit(__IXGBE_DOWN, &adapter->state)) {
		IXGBE_WRITE_REG(hw, IXGBE_EIMC, IXGBE_EIMC_LSC);
		schedule_work(&adapter->watchdog_task);
	}
}

static irqreturn_t ixgbe_msix_lsc(int irq, void *data)
{
	struct net_device *netdev = data;
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	struct ixgbe_hw *hw = &adapter->hw;
	u32 eicr;

	/*
	 * Workaround for Silicon errata.  Use clear-by-write instead
	 * of clear-by-read.  Reading with EICS will return the
	 * interrupt causes without clearing, which later be done
	 * with the write to EICR.
	 */
	eicr = IXGBE_READ_REG(hw, IXGBE_EICS);
	IXGBE_WRITE_REG(hw, IXGBE_EICR, eicr);

	if (eicr & IXGBE_EICR_LSC)
		ixgbe_check_lsc(adapter);

	if (hw->mac.type == ixgbe_mac_82598EB)
		ixgbe_check_fan_failure(adapter, eicr);

	if (hw->mac.type == ixgbe_mac_82599EB) {
		ixgbe_check_sfp_event(adapter, eicr);

		/* Handle Flow Director Full threshold interrupt */
		if (eicr & IXGBE_EICR_FLOW_DIR) {
			int i;
			IXGBE_WRITE_REG(hw, IXGBE_EICR, IXGBE_EICR_FLOW_DIR);
			/* Disable transmits before FDIR Re-initialization */
			netif_tx_stop_all_queues(netdev);
			for (i = 0; i < adapter->num_tx_queues; i++) {
				struct ixgbe_ring *tx_ring =
				                           &adapter->tx_ring[i];
				if (test_and_clear_bit(__IXGBE_FDIR_INIT_DONE,
				                       &tx_ring->reinit_state))
					schedule_work(&adapter->fdir_reinit_task);
			}
		}
	}
	if (!test_bit(__IXGBE_DOWN, &adapter->state))
		IXGBE_WRITE_REG(hw, IXGBE_EIMS, IXGBE_EIMS_OTHER);

	return IRQ_HANDLED;
}

static inline void ixgbe_irq_enable_queues(struct ixgbe_adapter *adapter,
					   u64 qmask)
{
	u32 mask;

	if (adapter->hw.mac.type == ixgbe_mac_82598EB) {
		mask = (IXGBE_EIMS_RTX_QUEUE & qmask);
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EIMS, mask);
	} else {
		mask = (qmask & 0xFFFFFFFF);
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EIMS_EX(0), mask);
		mask = (qmask >> 32);
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EIMS_EX(1), mask);
	}
	/* skip the flush */
}

static inline void ixgbe_irq_disable_queues(struct ixgbe_adapter *adapter,
                                            u64 qmask)
{
	u32 mask;

	if (adapter->hw.mac.type == ixgbe_mac_82598EB) {
		mask = (IXGBE_EIMS_RTX_QUEUE & qmask);
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EIMC, mask);
	} else {
		mask = (qmask & 0xFFFFFFFF);
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EIMC_EX(0), mask);
		mask = (qmask >> 32);
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EIMC_EX(1), mask);
	}
	/* skip the flush */
}

static irqreturn_t ixgbe_msix_clean_tx(int irq, void *data)
{
	struct ixgbe_q_vector *q_vector = data;
	struct ixgbe_adapter  *adapter = q_vector->adapter;
	struct ixgbe_ring     *tx_ring;
	int i, r_idx;

	if (!q_vector->txr_count)
		return IRQ_HANDLED;

	r_idx = find_first_bit(q_vector->txr_idx, adapter->num_tx_queues);
	for (i = 0; i < q_vector->txr_count; i++) {
		tx_ring = &(adapter->tx_ring[r_idx]);
		tx_ring->total_bytes = 0;
		tx_ring->total_packets = 0;
		r_idx = find_next_bit(q_vector->txr_idx, adapter->num_tx_queues,
		                      r_idx + 1);
	}

	/* disable interrupts on this vector only */
	ixgbe_irq_disable_queues(adapter, ((u64)1 << q_vector->v_idx));
	napi_schedule(&q_vector->napi);

	return IRQ_HANDLED;
}

/**
 * ixgbe_msix_clean_rx - single unshared vector rx clean (all queues)
 * @irq: unused
 * @data: pointer to our q_vector struct for this interrupt vector
 **/
static irqreturn_t ixgbe_msix_clean_rx(int irq, void *data)
{
	struct ixgbe_q_vector *q_vector = data;
	struct ixgbe_adapter  *adapter = q_vector->adapter;
	struct ixgbe_ring  *rx_ring;
	int r_idx;
	int i;

	r_idx = find_first_bit(q_vector->rxr_idx, adapter->num_rx_queues);
	for (i = 0;  i < q_vector->rxr_count; i++) {
		rx_ring = &(adapter->rx_ring[r_idx]);
		rx_ring->total_bytes = 0;
		rx_ring->total_packets = 0;
		r_idx = find_next_bit(q_vector->rxr_idx, adapter->num_rx_queues,
		                      r_idx + 1);
	}

	if (!q_vector->rxr_count)
		return IRQ_HANDLED;

	r_idx = find_first_bit(q_vector->rxr_idx, adapter->num_rx_queues);
	rx_ring = &(adapter->rx_ring[r_idx]);
	/* disable interrupts on this vector only */
	ixgbe_irq_disable_queues(adapter, ((u64)1 << q_vector->v_idx));
	napi_schedule(&q_vector->napi);

	return IRQ_HANDLED;
}

static irqreturn_t ixgbe_msix_clean_many(int irq, void *data)
{
	struct ixgbe_q_vector *q_vector = data;
	struct ixgbe_adapter  *adapter = q_vector->adapter;
	struct ixgbe_ring  *ring;
	int r_idx;
	int i;

	if (!q_vector->txr_count && !q_vector->rxr_count)
		return IRQ_HANDLED;

	r_idx = find_first_bit(q_vector->txr_idx, adapter->num_tx_queues);
	for (i = 0; i < q_vector->txr_count; i++) {
		ring = &(adapter->tx_ring[r_idx]);
		ring->total_bytes = 0;
		ring->total_packets = 0;
		r_idx = find_next_bit(q_vector->txr_idx, adapter->num_tx_queues,
		                      r_idx + 1);
	}

	r_idx = find_first_bit(q_vector->rxr_idx, adapter->num_rx_queues);
	for (i = 0; i < q_vector->rxr_count; i++) {
		ring = &(adapter->rx_ring[r_idx]);
		ring->total_bytes = 0;
		ring->total_packets = 0;
		r_idx = find_next_bit(q_vector->rxr_idx, adapter->num_rx_queues,
		                      r_idx + 1);
	}

	/* disable interrupts on this vector only */
	ixgbe_irq_disable_queues(adapter, ((u64)1 << q_vector->v_idx));
	napi_schedule(&q_vector->napi);

	return IRQ_HANDLED;
}

/**
 * ixgbe_clean_rxonly - msix (aka one shot) rx clean routine
 * @napi: napi struct with our devices info in it
 * @budget: amount of work driver is allowed to do this pass, in packets
 *
 * This function is optimized for cleaning one queue only on a single
 * q_vector!!!
 **/
static int ixgbe_clean_rxonly(struct napi_struct *napi, int budget)
{
	struct ixgbe_q_vector *q_vector =
	                       container_of(napi, struct ixgbe_q_vector, napi);
	struct ixgbe_adapter *adapter = q_vector->adapter;
	struct ixgbe_ring *rx_ring = NULL;
	int work_done = 0;
	long r_idx;

	r_idx = find_first_bit(q_vector->rxr_idx, adapter->num_rx_queues);
	rx_ring = &(adapter->rx_ring[r_idx]);
#ifdef CONFIG_IXGBE_DCA
	if (adapter->flags & IXGBE_FLAG_DCA_ENABLED)
		ixgbe_update_rx_dca(adapter, rx_ring);
#endif

	ixgbe_clean_rx_irq(q_vector, rx_ring, &work_done, budget);

	/* If all Rx work done, exit the polling mode */
	if (work_done < budget) {
		napi_complete(napi);
		if (adapter->rx_itr_setting & 1)
			ixgbe_set_itr_msix(q_vector);
		if (!test_bit(__IXGBE_DOWN, &adapter->state))
			ixgbe_irq_enable_queues(adapter,
			                        ((u64)1 << q_vector->v_idx));
	}

	return work_done;
}

/**
 * ixgbe_clean_rxtx_many - msix (aka one shot) rx clean routine
 * @napi: napi struct with our devices info in it
 * @budget: amount of work driver is allowed to do this pass, in packets
 *
 * This function will clean more than one rx queue associated with a
 * q_vector.
 **/
static int ixgbe_clean_rxtx_many(struct napi_struct *napi, int budget)
{
	struct ixgbe_q_vector *q_vector =
	                       container_of(napi, struct ixgbe_q_vector, napi);
	struct ixgbe_adapter *adapter = q_vector->adapter;
	struct ixgbe_ring *ring = NULL;
	int work_done = 0, i;
	long r_idx;
	bool tx_clean_complete = true;

	r_idx = find_first_bit(q_vector->txr_idx, adapter->num_tx_queues);
	for (i = 0; i < q_vector->txr_count; i++) {
		ring = &(adapter->tx_ring[r_idx]);
#ifdef CONFIG_IXGBE_DCA
		if (adapter->flags & IXGBE_FLAG_DCA_ENABLED)
			ixgbe_update_tx_dca(adapter, ring);
#endif
		tx_clean_complete &= ixgbe_clean_tx_irq(q_vector, ring);
		r_idx = find_next_bit(q_vector->txr_idx, adapter->num_tx_queues,
		                      r_idx + 1);
	}

	/* attempt to distribute budget to each queue fairly, but don't allow
	 * the budget to go below 1 because we'll exit polling */
	budget /= (q_vector->rxr_count ?: 1);
	budget = max(budget, 1);
	r_idx = find_first_bit(q_vector->rxr_idx, adapter->num_rx_queues);
	for (i = 0; i < q_vector->rxr_count; i++) {
		ring = &(adapter->rx_ring[r_idx]);
#ifdef CONFIG_IXGBE_DCA
		if (adapter->flags & IXGBE_FLAG_DCA_ENABLED)
			ixgbe_update_rx_dca(adapter, ring);
#endif
		ixgbe_clean_rx_irq(q_vector, ring, &work_done, budget);
		r_idx = find_next_bit(q_vector->rxr_idx, adapter->num_rx_queues,
		                      r_idx + 1);
	}

	r_idx = find_first_bit(q_vector->rxr_idx, adapter->num_rx_queues);
	ring = &(adapter->rx_ring[r_idx]);
	/* If all Rx work done, exit the polling mode */
	if (work_done < budget) {
		napi_complete(napi);
		if (adapter->rx_itr_setting & 1)
			ixgbe_set_itr_msix(q_vector);
		if (!test_bit(__IXGBE_DOWN, &adapter->state))
			ixgbe_irq_enable_queues(adapter,
			                        ((u64)1 << q_vector->v_idx));
		return 0;
	}

	return work_done;
}

/**
 * ixgbe_clean_txonly - msix (aka one shot) tx clean routine
 * @napi: napi struct with our devices info in it
 * @budget: amount of work driver is allowed to do this pass, in packets
 *
 * This function is optimized for cleaning one queue only on a single
 * q_vector!!!
 **/
static int ixgbe_clean_txonly(struct napi_struct *napi, int budget)
{
	struct ixgbe_q_vector *q_vector =
	                       container_of(napi, struct ixgbe_q_vector, napi);
	struct ixgbe_adapter *adapter = q_vector->adapter;
	struct ixgbe_ring *tx_ring = NULL;
	int work_done = 0;
	long r_idx;

	r_idx = find_first_bit(q_vector->txr_idx, adapter->num_tx_queues);
	tx_ring = &(adapter->tx_ring[r_idx]);
#ifdef CONFIG_IXGBE_DCA
	if (adapter->flags & IXGBE_FLAG_DCA_ENABLED)
		ixgbe_update_tx_dca(adapter, tx_ring);
#endif

	if (!ixgbe_clean_tx_irq(q_vector, tx_ring))
		work_done = budget;

	/* If all Tx work done, exit the polling mode */
	if (work_done < budget) {
		napi_complete(napi);
		if (adapter->tx_itr_setting & 1)
			ixgbe_set_itr_msix(q_vector);
		if (!test_bit(__IXGBE_DOWN, &adapter->state))
			ixgbe_irq_enable_queues(adapter, ((u64)1 << q_vector->v_idx));
	}

	return work_done;
}

static inline void map_vector_to_rxq(struct ixgbe_adapter *a, int v_idx,
                                     int r_idx)
{
	struct ixgbe_q_vector *q_vector = a->q_vector[v_idx];

	set_bit(r_idx, q_vector->rxr_idx);
	q_vector->rxr_count++;
}

static inline void map_vector_to_txq(struct ixgbe_adapter *a, int v_idx,
                                     int t_idx)
{
	struct ixgbe_q_vector *q_vector = a->q_vector[v_idx];

	set_bit(t_idx, q_vector->txr_idx);
	q_vector->txr_count++;
}

/**
 * ixgbe_map_rings_to_vectors - Maps descriptor rings to vectors
 * @adapter: board private structure to initialize
 * @vectors: allotted vector count for descriptor rings
 *
 * This function maps descriptor rings to the queue-specific vectors
 * we were allotted through the MSI-X enabling code.  Ideally, we'd have
 * one vector per ring/queue, but on a constrained vector budget, we
 * group the rings as "efficiently" as possible.  You would add new
 * mapping configurations in here.
 **/
static int ixgbe_map_rings_to_vectors(struct ixgbe_adapter *adapter,
                                      int vectors)
{
	int v_start = 0;
	int rxr_idx = 0, txr_idx = 0;
	int rxr_remaining = adapter->num_rx_queues;
	int txr_remaining = adapter->num_tx_queues;
	int i, j;
	int rqpv, tqpv;
	int err = 0;

	/* No mapping required if MSI-X is disabled. */
	if (!(adapter->flags & IXGBE_FLAG_MSIX_ENABLED))
		goto out;

	/*
	 * The ideal configuration...
	 * We have enough vectors to map one per queue.
	 */
	if (vectors == adapter->num_rx_queues + adapter->num_tx_queues) {
		for (; rxr_idx < rxr_remaining; v_start++, rxr_idx++)
			map_vector_to_rxq(adapter, v_start, rxr_idx);

		for (; txr_idx < txr_remaining; v_start++, txr_idx++)
			map_vector_to_txq(adapter, v_start, txr_idx);

		goto out;
	}

	/*
	 * If we don't have enough vectors for a 1-to-1
	 * mapping, we'll have to group them so there are
	 * multiple queues per vector.
	 */
	/* Re-adjusting *qpv takes care of the remainder. */
	for (i = v_start; i < vectors; i++) {
		rqpv = DIV_ROUND_UP(rxr_remaining, vectors - i);
		for (j = 0; j < rqpv; j++) {
			map_vector_to_rxq(adapter, i, rxr_idx);
			rxr_idx++;
			rxr_remaining--;
		}
	}
	for (i = v_start; i < vectors; i++) {
		tqpv = DIV_ROUND_UP(txr_remaining, vectors - i);
		for (j = 0; j < tqpv; j++) {
			map_vector_to_txq(adapter, i, txr_idx);
			txr_idx++;
			txr_remaining--;
		}
	}

out:
	return err;
}

/**
 * ixgbe_request_msix_irqs - Initialize MSI-X interrupts
 * @adapter: board private structure
 *
 * ixgbe_request_msix_irqs allocates MSI-X vectors and requests
 * interrupts from the kernel.
 **/
static int ixgbe_request_msix_irqs(struct ixgbe_adapter *adapter)
{
	struct net_device *netdev = adapter->netdev;
	irqreturn_t (*handler)(int, void *);
	int i, vector, q_vectors, err;
	int ri=0, ti=0;

	/* Decrement for Other and TCP Timer vectors */
	q_vectors = adapter->num_msix_vectors - NON_Q_VECTORS;

	/* Map the Tx/Rx rings to the vectors we were allotted. */
	err = ixgbe_map_rings_to_vectors(adapter, q_vectors);
	if (err)
		goto out;

#define SET_HANDLER(_v) ((!(_v)->rxr_count) ? &ixgbe_msix_clean_tx : \
                         (!(_v)->txr_count) ? &ixgbe_msix_clean_rx : \
                         &ixgbe_msix_clean_many)
	for (vector = 0; vector < q_vectors; vector++) {
		handler = SET_HANDLER(adapter->q_vector[vector]);

		if(handler == &ixgbe_msix_clean_rx) {
			sprintf(adapter->name[vector], "%s-%s-%d",
				netdev->name, "rx", ri++);
		}
		else if(handler == &ixgbe_msix_clean_tx) {
			sprintf(adapter->name[vector], "%s-%s-%d",
				netdev->name, "tx", ti++);
		}
		else
			sprintf(adapter->name[vector], "%s-%s-%d",
				netdev->name, "TxRx", vector);

		err = request_irq(adapter->msix_entries[vector].vector,
		                  handler, 0, adapter->name[vector],
		                  adapter->q_vector[vector]);
		if (err) {
			DPRINTK(PROBE, ERR,
			        "request_irq failed for MSIX interrupt "
			        "Error: %d\n", err);
			goto free_queue_irqs;
		}
	}

	sprintf(adapter->name[vector], "%s:lsc", netdev->name);
	err = request_irq(adapter->msix_entries[vector].vector,
	                  &ixgbe_msix_lsc, 0, adapter->name[vector], netdev);
	if (err) {
		DPRINTK(PROBE, ERR,
			"request_irq for msix_lsc failed: %d\n", err);
		goto free_queue_irqs;
	}

	return 0;

free_queue_irqs:
	for (i = vector - 1; i >= 0; i--)
		free_irq(adapter->msix_entries[--vector].vector,
		         adapter->q_vector[i]);
	adapter->flags &= ~IXGBE_FLAG_MSIX_ENABLED;
	pci_disable_msix(adapter->pdev);
	kfree(adapter->msix_entries);
	adapter->msix_entries = NULL;
out:
	return err;
}

static void ixgbe_set_itr(struct ixgbe_adapter *adapter)
{
	struct ixgbe_q_vector *q_vector = adapter->q_vector[0];
	u8 current_itr;
	u32 new_itr = q_vector->eitr;
	struct ixgbe_ring *rx_ring = &adapter->rx_ring[0];
	struct ixgbe_ring *tx_ring = &adapter->tx_ring[0];

	q_vector->tx_itr = ixgbe_update_itr(adapter, new_itr,
	                                    q_vector->tx_itr,
	                                    tx_ring->total_packets,
	                                    tx_ring->total_bytes);
	q_vector->rx_itr = ixgbe_update_itr(adapter, new_itr,
	                                    q_vector->rx_itr,
	                                    rx_ring->total_packets,
	                                    rx_ring->total_bytes);

	current_itr = max(q_vector->rx_itr, q_vector->tx_itr);

	switch (current_itr) {
	/* counts and packets in update_itr are dependent on these numbers */
	case lowest_latency:
		new_itr = 100000;
		break;
	case low_latency:
		new_itr = 20000; /* aka hwitr = ~200 */
		break;
	case bulk_latency:
		new_itr = 8000;
		break;
	default:
		break;
	}

	if (new_itr != q_vector->eitr) {
		/* do an exponential smoothing */
		new_itr = ((q_vector->eitr * 90)/100) + ((new_itr * 10)/100);

		/* save the algorithm value here, not the smoothed one */
		q_vector->eitr = new_itr;

		ixgbe_write_eitr(q_vector);
	}

	return;
}

/**
 * ixgbe_irq_enable - Enable default interrupt generation settings
 * @adapter: board private structure
 **/
static inline void ixgbe_irq_enable(struct ixgbe_adapter *adapter)
{
	u32 mask;

	mask = (IXGBE_EIMS_ENABLE_MASK & ~IXGBE_EIMS_RTX_QUEUE);
	if (adapter->flags & IXGBE_FLAG_FAN_FAIL_CAPABLE)
		mask |= IXGBE_EIMS_GPI_SDP1;
	if (adapter->hw.mac.type == ixgbe_mac_82599EB) {
		mask |= IXGBE_EIMS_ECC;
		mask |= IXGBE_EIMS_GPI_SDP1;
		mask |= IXGBE_EIMS_GPI_SDP2;
	}
	if (adapter->flags & IXGBE_FLAG_FDIR_HASH_CAPABLE ||
	    adapter->flags & IXGBE_FLAG_FDIR_PERFECT_CAPABLE)
		mask |= IXGBE_EIMS_FLOW_DIR;

	IXGBE_WRITE_REG(&adapter->hw, IXGBE_EIMS, mask);
	ixgbe_irq_enable_queues(adapter, ~0);
	IXGBE_WRITE_FLUSH(&adapter->hw);
}

/**
 * ixgbe_intr - legacy mode Interrupt Handler
 * @irq: interrupt number
 * @data: pointer to a network interface device structure
 **/
static irqreturn_t ixgbe_intr(int irq, void *data)
{
	struct net_device *netdev = data;
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	struct ixgbe_hw *hw = &adapter->hw;
	struct ixgbe_q_vector *q_vector = adapter->q_vector[0];
	u32 eicr;

	/*
	 * Workaround for silicon errata.  Mask the interrupts
	 * before the read of EICR.
	 */
	IXGBE_WRITE_REG(hw, IXGBE_EIMC, IXGBE_IRQ_CLEAR_MASK);

	/* for NAPI, using EIAM to auto-mask tx/rx interrupt bits on read
	 * therefore no explict interrupt disable is necessary */
	eicr = IXGBE_READ_REG(hw, IXGBE_EICR);
	if (!eicr) {
		/* shared interrupt alert!
		 * make sure interrupts are enabled because the read will
		 * have disabled interrupts due to EIAM */
		ixgbe_irq_enable(adapter);
		return IRQ_NONE;	/* Not our interrupt */
	}

	if (eicr & IXGBE_EICR_LSC)
		ixgbe_check_lsc(adapter);

	if (hw->mac.type == ixgbe_mac_82599EB)
		ixgbe_check_sfp_event(adapter, eicr);

	ixgbe_check_fan_failure(adapter, eicr);

	if (napi_schedule_prep(&(q_vector->napi))) {
		adapter->tx_ring[0].total_packets = 0;
		adapter->tx_ring[0].total_bytes = 0;
		adapter->rx_ring[0].total_packets = 0;
		adapter->rx_ring[0].total_bytes = 0;
		/* would disable interrupts here but EIAM disabled it */
		__napi_schedule(&(q_vector->napi));
	}

	return IRQ_HANDLED;
}

static inline void ixgbe_reset_q_vectors(struct ixgbe_adapter *adapter)
{
	int i, q_vectors = adapter->num_msix_vectors - NON_Q_VECTORS;

	for (i = 0; i < q_vectors; i++) {
		struct ixgbe_q_vector *q_vector = adapter->q_vector[i];
		bitmap_zero(q_vector->rxr_idx, MAX_RX_QUEUES);
		bitmap_zero(q_vector->txr_idx, MAX_TX_QUEUES);
		q_vector->rxr_count = 0;
		q_vector->txr_count = 0;
	}
}

/**
 * ixgbe_request_irq - initialize interrupts
 * @adapter: board private structure
 *
 * Attempts to configure interrupts using the best available
 * capabilities of the hardware and kernel.
 **/
static int ixgbe_request_irq(struct ixgbe_adapter *adapter)
{
	struct net_device *netdev = adapter->netdev;
	int err;

	if (adapter->flags & IXGBE_FLAG_MSIX_ENABLED) {
		err = ixgbe_request_msix_irqs(adapter);
	} else if (adapter->flags & IXGBE_FLAG_MSI_ENABLED) {
		err = request_irq(adapter->pdev->irq, &ixgbe_intr, 0,
		                  netdev->name, netdev);
	} else {
		err = request_irq(adapter->pdev->irq, &ixgbe_intr, IRQF_SHARED,
		                  netdev->name, netdev);
	}

	if (err)
		DPRINTK(PROBE, ERR, "request_irq failed, Error %d\n", err);

	return err;
}

static void ixgbe_free_irq(struct ixgbe_adapter *adapter)
{
	struct net_device *netdev = adapter->netdev;

	if (adapter->flags & IXGBE_FLAG_MSIX_ENABLED) {
		int i, q_vectors;

		q_vectors = adapter->num_msix_vectors;

		i = q_vectors - 1;
		free_irq(adapter->msix_entries[i].vector, netdev);

		i--;
		for (; i >= 0; i--) {
			free_irq(adapter->msix_entries[i].vector,
			         adapter->q_vector[i]);
		}

		ixgbe_reset_q_vectors(adapter);
	} else {
		free_irq(adapter->pdev->irq, netdev);
	}
}

/**
 * ixgbe_irq_disable - Mask off interrupt generation on the NIC
 * @adapter: board private structure
 **/
static inline void ixgbe_irq_disable(struct ixgbe_adapter *adapter)
{
	if (adapter->hw.mac.type == ixgbe_mac_82598EB) {
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EIMC, ~0);
	} else {
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EIMC, 0xFFFF0000);
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EIMC_EX(0), ~0);
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EIMC_EX(1), ~0);
	}
	IXGBE_WRITE_FLUSH(&adapter->hw);
	if (adapter->flags & IXGBE_FLAG_MSIX_ENABLED) {
		int i;
		for (i = 0; i < adapter->num_msix_vectors; i++)
			synchronize_irq(adapter->msix_entries[i].vector);
	} else {
		synchronize_irq(adapter->pdev->irq);
	}
}

/**
 * ixgbe_configure_msi_and_legacy - Initialize PIN (INTA...) and MSI interrupts
 *
 **/
static void ixgbe_configure_msi_and_legacy(struct ixgbe_adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;

	IXGBE_WRITE_REG(hw, IXGBE_EITR(0),
	                EITR_INTS_PER_SEC_TO_REG(adapter->rx_eitr_param));

	ixgbe_set_ivar(adapter, 0, 0, 0);
	ixgbe_set_ivar(adapter, 1, 0, 0);

	map_vector_to_rxq(adapter, 0, 0);
	map_vector_to_txq(adapter, 0, 0);

	DPRINTK(HW, INFO, "Legacy interrupt IVAR setup done\n");
}

/**
 * ixgbe_configure_tx - Configure 8259x Transmit Unit after Reset
 * @adapter: board private structure
 *
 * Configure the Tx unit of the MAC after a reset.
 **/
static void ixgbe_configure_tx(struct ixgbe_adapter *adapter)
{
	u64 tdba;
	struct ixgbe_hw *hw = &adapter->hw;
	u32 i, j, tdlen, txctrl;

	/* Setup the HW Tx Head and Tail descriptor pointers */
	for (i = 0; i < adapter->num_tx_queues; i++) {
		struct ixgbe_ring *ring = &adapter->tx_ring[i];
		j = ring->reg_idx;
		tdba = ring->dma;
		tdlen = ring->count * sizeof(union ixgbe_adv_tx_desc);
		IXGBE_WRITE_REG(hw, IXGBE_TDBAL(j),
		                (tdba & DMA_BIT_MASK(32)));
		IXGBE_WRITE_REG(hw, IXGBE_TDBAH(j), (tdba >> 32));
		IXGBE_WRITE_REG(hw, IXGBE_TDLEN(j), tdlen);
		IXGBE_WRITE_REG(hw, IXGBE_TDH(j), 0);
		IXGBE_WRITE_REG(hw, IXGBE_TDT(j), 0);
		adapter->tx_ring[i].head = IXGBE_TDH(j);
		adapter->tx_ring[i].tail = IXGBE_TDT(j);
		/*
		 * Disable Tx Head Writeback RO bit, since this hoses
		 * bookkeeping if things aren't delivered in order.
		 */
		switch (hw->mac.type) {
		case ixgbe_mac_82598EB:
			txctrl = IXGBE_READ_REG(hw, IXGBE_DCA_TXCTRL(j));
			break;
		case ixgbe_mac_82599EB:
		default:
			txctrl = IXGBE_READ_REG(hw, IXGBE_DCA_TXCTRL_82599(j));
			break;
		}
		txctrl &= ~IXGBE_DCA_TXCTRL_TX_WB_RO_EN;
		switch (hw->mac.type) {
		case ixgbe_mac_82598EB:
			IXGBE_WRITE_REG(hw, IXGBE_DCA_TXCTRL(j), txctrl);
			break;
		case ixgbe_mac_82599EB:
		default:
			IXGBE_WRITE_REG(hw, IXGBE_DCA_TXCTRL_82599(j), txctrl);
			break;
		}
	}

	if (hw->mac.type == ixgbe_mac_82599EB) {
		u32 rttdcs;

		/* disable the arbiter while setting MTQC */
		rttdcs = IXGBE_READ_REG(hw, IXGBE_RTTDCS);
		rttdcs |= IXGBE_RTTDCS_ARBDIS;
		IXGBE_WRITE_REG(hw, IXGBE_RTTDCS, rttdcs);

		/* We enable 8 traffic classes, DCB only */
		if (adapter->flags & IXGBE_FLAG_DCB_ENABLED)
			IXGBE_WRITE_REG(hw, IXGBE_MTQC, (IXGBE_MTQC_RT_ENA |
			                IXGBE_MTQC_8TC_8TQ));
		else
			IXGBE_WRITE_REG(hw, IXGBE_MTQC, IXGBE_MTQC_64Q_1PB);

		/* re-eable the arbiter */
		rttdcs &= ~IXGBE_RTTDCS_ARBDIS;
		IXGBE_WRITE_REG(hw, IXGBE_RTTDCS, rttdcs);
	}
}

#define IXGBE_SRRCTL_BSIZEHDRSIZE_SHIFT 2

static void ixgbe_configure_srrctl(struct ixgbe_adapter *adapter,
                                   struct ixgbe_ring *rx_ring)
{
	u32 srrctl;
	int index;
	struct ixgbe_ring_feature *feature = adapter->ring_feature;

	index = rx_ring->reg_idx;
	if (adapter->hw.mac.type == ixgbe_mac_82598EB) {
		unsigned long mask;
		mask = (unsigned long) feature[RING_F_RSS].mask;
		index = index & mask;
	}
	srrctl = IXGBE_READ_REG(&adapter->hw, IXGBE_SRRCTL(index));

	srrctl &= ~IXGBE_SRRCTL_BSIZEHDR_MASK;
	srrctl &= ~IXGBE_SRRCTL_BSIZEPKT_MASK;

	srrctl |= (IXGBE_RX_HDR_SIZE << IXGBE_SRRCTL_BSIZEHDRSIZE_SHIFT) &
		  IXGBE_SRRCTL_BSIZEHDR_MASK;

	if (rx_ring->flags & IXGBE_RING_RX_PS_ENABLED) {
#if (PAGE_SIZE / 2) > IXGBE_MAX_RXBUFFER
		srrctl |= IXGBE_MAX_RXBUFFER >> IXGBE_SRRCTL_BSIZEPKT_SHIFT;
#else
		srrctl |= (PAGE_SIZE / 2) >> IXGBE_SRRCTL_BSIZEPKT_SHIFT;
#endif
		srrctl |= IXGBE_SRRCTL_DESCTYPE_HDR_SPLIT_ALWAYS;
	} else {
		srrctl |= ALIGN(rx_ring->rx_buf_len, 1024) >>
			  IXGBE_SRRCTL_BSIZEPKT_SHIFT;
		srrctl |= IXGBE_SRRCTL_DESCTYPE_ADV_ONEBUF;
	}

	IXGBE_WRITE_REG(&adapter->hw, IXGBE_SRRCTL(index), srrctl);
}

static u32 ixgbe_setup_mrqc(struct ixgbe_adapter *adapter)
{
	u32 mrqc = 0;
	int mask;

	if (!(adapter->hw.mac.type == ixgbe_mac_82599EB))
		return mrqc;

	mask = adapter->flags & (IXGBE_FLAG_RSS_ENABLED
#ifdef CONFIG_IXGBE_DCB
				 | IXGBE_FLAG_DCB_ENABLED
#endif
				);

	switch (mask) {
	case (IXGBE_FLAG_RSS_ENABLED):
		mrqc = IXGBE_MRQC_RSSEN;
		break;
#ifdef CONFIG_IXGBE_DCB
	case (IXGBE_FLAG_DCB_ENABLED):
		mrqc = IXGBE_MRQC_RT8TCEN;
		break;
#endif /* CONFIG_IXGBE_DCB */
	default:
		break;
	}

	return mrqc;
}

/**
 * ixgbe_configure_rscctl - enable RSC for the indicated ring
 * @adapter:    address of board private structure
 * @index:      index of ring to set
 * @rx_buf_len: rx buffer length
 **/
static void ixgbe_configure_rscctl(struct ixgbe_adapter *adapter, int index,
                                   int rx_buf_len)
{
	struct ixgbe_ring *rx_ring;
	struct ixgbe_hw *hw = &adapter->hw;
	int j;
	u32 rscctrl;

	rx_ring = &adapter->rx_ring[index];
	j = rx_ring->reg_idx;
	rscctrl = IXGBE_READ_REG(hw, IXGBE_RSCCTL(j));
	rscctrl |= IXGBE_RSCCTL_RSCEN;
	/*
	 * we must limit the number of descriptors so that the
	 * total size of max desc * buf_len is not greater
	 * than 65535
	 */
	if (rx_ring->flags & IXGBE_RING_RX_PS_ENABLED) {
#if (MAX_SKB_FRAGS > 16)
		rscctrl |= IXGBE_RSCCTL_MAXDESC_16;
#elif (MAX_SKB_FRAGS > 8)
		rscctrl |= IXGBE_RSCCTL_MAXDESC_8;
#elif (MAX_SKB_FRAGS > 4)
		rscctrl |= IXGBE_RSCCTL_MAXDESC_4;
#else
		rscctrl |= IXGBE_RSCCTL_MAXDESC_1;
#endif
	} else {
		if (rx_buf_len < IXGBE_RXBUFFER_4096)
			rscctrl |= IXGBE_RSCCTL_MAXDESC_16;
		else if (rx_buf_len < IXGBE_RXBUFFER_8192)
			rscctrl |= IXGBE_RSCCTL_MAXDESC_8;
		else
			rscctrl |= IXGBE_RSCCTL_MAXDESC_4;
	}
	IXGBE_WRITE_REG(hw, IXGBE_RSCCTL(j), rscctrl);
}

/**
 * ixgbe_configure_rx - Configure 8259x Receive Unit after Reset
 * @adapter: board private structure
 *
 * Configure the Rx unit of the MAC after a reset.
 **/
static void ixgbe_configure_rx(struct ixgbe_adapter *adapter)
{
	u64 rdba;
	struct ixgbe_hw *hw = &adapter->hw;
	struct ixgbe_ring *rx_ring;
	struct net_device *netdev = adapter->netdev;
	int max_frame = netdev->mtu + ETH_HLEN + ETH_FCS_LEN;
	int i, j;
	u32 rdlen, rxctrl, rxcsum;
	static const u32 seed[10] = { 0xE291D73D, 0x1805EC6C, 0x2A94B30D,
	                  0xA54F2BEC, 0xEA49AF7C, 0xE214AD3D, 0xB855AABE,
	                  0x6A3E67EA, 0x14364D17, 0x3BED200D};
	u32 fctrl, hlreg0;
	u32 reta = 0, mrqc = 0;
	u32 rdrxctl;
	int rx_buf_len;

	/* Decide whether to use packet split mode or not */
	adapter->flags |= IXGBE_FLAG_RX_PS_ENABLED;

	/* Set the RX buffer length according to the mode */
	if (adapter->flags & IXGBE_FLAG_RX_PS_ENABLED) {
		rx_buf_len = IXGBE_RX_HDR_SIZE;
		if (hw->mac.type == ixgbe_mac_82599EB) {
			/* PSRTYPE must be initialized in 82599 */
			u32 psrtype = IXGBE_PSRTYPE_TCPHDR |
			              IXGBE_PSRTYPE_UDPHDR |
			              IXGBE_PSRTYPE_IPV4HDR |
			              IXGBE_PSRTYPE_IPV6HDR |
			              IXGBE_PSRTYPE_L2HDR;
			IXGBE_WRITE_REG(hw, IXGBE_PSRTYPE(0), psrtype);
		}
	} else {
		if (!(adapter->flags2 & IXGBE_FLAG2_RSC_ENABLED) &&
		    (netdev->mtu <= ETH_DATA_LEN))
			rx_buf_len = MAXIMUM_ETHERNET_VLAN_SIZE;
		else
			rx_buf_len = ALIGN(max_frame, 1024);
	}

	fctrl = IXGBE_READ_REG(&adapter->hw, IXGBE_FCTRL);
	fctrl |= IXGBE_FCTRL_BAM;
	fctrl |= IXGBE_FCTRL_DPF; /* discard pause frames when FC enabled */
	fctrl |= IXGBE_FCTRL_PMCF;
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_FCTRL, fctrl);

	hlreg0 = IXGBE_READ_REG(hw, IXGBE_HLREG0);
	if (adapter->netdev->mtu <= ETH_DATA_LEN)
		hlreg0 &= ~IXGBE_HLREG0_JUMBOEN;
	else
		hlreg0 |= IXGBE_HLREG0_JUMBOEN;
#ifdef IXGBE_FCOE
	if (netdev->features & NETIF_F_FCOE_MTU)
		hlreg0 |= IXGBE_HLREG0_JUMBOEN;
#endif
	IXGBE_WRITE_REG(hw, IXGBE_HLREG0, hlreg0);

	rdlen = adapter->rx_ring[0].count * sizeof(union ixgbe_adv_rx_desc);
	/* disable receives while setting up the descriptors */
	rxctrl = IXGBE_READ_REG(hw, IXGBE_RXCTRL);
	IXGBE_WRITE_REG(hw, IXGBE_RXCTRL, rxctrl & ~IXGBE_RXCTRL_RXEN);

	/*
	 * Setup the HW Rx Head and Tail Descriptor Pointers and
	 * the Base and Length of the Rx Descriptor Ring
	 */
	for (i = 0; i < adapter->num_rx_queues; i++) {
		rx_ring = &adapter->rx_ring[i];
		rdba = rx_ring->dma;
		j = rx_ring->reg_idx;
		IXGBE_WRITE_REG(hw, IXGBE_RDBAL(j), (rdba & DMA_BIT_MASK(32)));
		IXGBE_WRITE_REG(hw, IXGBE_RDBAH(j), (rdba >> 32));
		IXGBE_WRITE_REG(hw, IXGBE_RDLEN(j), rdlen);
		IXGBE_WRITE_REG(hw, IXGBE_RDH(j), 0);
		IXGBE_WRITE_REG(hw, IXGBE_RDT(j), 0);
		rx_ring->head = IXGBE_RDH(j);
		rx_ring->tail = IXGBE_RDT(j);
		rx_ring->rx_buf_len = rx_buf_len;

		if (adapter->flags & IXGBE_FLAG_RX_PS_ENABLED)
			rx_ring->flags |= IXGBE_RING_RX_PS_ENABLED;
		else
			rx_ring->flags &= ~IXGBE_RING_RX_PS_ENABLED;

#ifdef IXGBE_FCOE
		if (netdev->features & NETIF_F_FCOE_MTU) {
			struct ixgbe_ring_feature *f;
			f = &adapter->ring_feature[RING_F_FCOE];
			if ((i >= f->mask) && (i < f->mask + f->indices)) {
				rx_ring->flags &= ~IXGBE_RING_RX_PS_ENABLED;
				if (rx_buf_len < IXGBE_FCOE_JUMBO_FRAME_SIZE)
					rx_ring->rx_buf_len =
					        IXGBE_FCOE_JUMBO_FRAME_SIZE;
			}
		}

#endif /* IXGBE_FCOE */
		ixgbe_configure_srrctl(adapter, rx_ring);
	}

	if (hw->mac.type == ixgbe_mac_82598EB) {
		/*
		 * For VMDq support of different descriptor types or
		 * buffer sizes through the use of multiple SRRCTL
		 * registers, RDRXCTL.MVMEN must be set to 1
		 *
		 * also, the manual doesn't mention it clearly but DCA hints
		 * will only use queue 0's tags unless this bit is set.  Side
		 * effects of setting this bit are only that SRRCTL must be
		 * fully programmed [0..15]
		 */
		rdrxctl = IXGBE_READ_REG(hw, IXGBE_RDRXCTL);
		rdrxctl |= IXGBE_RDRXCTL_MVMEN;
		IXGBE_WRITE_REG(hw, IXGBE_RDRXCTL, rdrxctl);
	}

	/* Program MRQC for the distribution of queues */
	mrqc = ixgbe_setup_mrqc(adapter);

	if (adapter->flags & IXGBE_FLAG_RSS_ENABLED) {
		/* Fill out redirection table */
		for (i = 0, j = 0; i < 128; i++, j++) {
			if (j == adapter->ring_feature[RING_F_RSS].indices)
				j = 0;
			/* reta = 4-byte sliding window of
			 * 0x00..(indices-1)(indices-1)00..etc. */
			reta = (reta << 8) | (j * 0x11);
			if ((i & 3) == 3)
				IXGBE_WRITE_REG(hw, IXGBE_RETA(i >> 2), reta);
		}

		/* Fill out hash function seeds */
		for (i = 0; i < 10; i++)
			IXGBE_WRITE_REG(hw, IXGBE_RSSRK(i), seed[i]);

		if (hw->mac.type == ixgbe_mac_82598EB)
			mrqc |= IXGBE_MRQC_RSSEN;
		    /* Perform hash on these packet types */
		mrqc |= IXGBE_MRQC_RSS_FIELD_IPV4
		      | IXGBE_MRQC_RSS_FIELD_IPV4_TCP
		      | IXGBE_MRQC_RSS_FIELD_IPV4_UDP
		      | IXGBE_MRQC_RSS_FIELD_IPV6
		      | IXGBE_MRQC_RSS_FIELD_IPV6_TCP
		      | IXGBE_MRQC_RSS_FIELD_IPV6_UDP;
	}
	IXGBE_WRITE_REG(hw, IXGBE_MRQC, mrqc);

	rxcsum = IXGBE_READ_REG(hw, IXGBE_RXCSUM);

	if (adapter->flags & IXGBE_FLAG_RSS_ENABLED ||
	    adapter->flags & IXGBE_FLAG_RX_CSUM_ENABLED) {
		/* Disable indicating checksum in descriptor, enables
		 * RSS hash */
		rxcsum |= IXGBE_RXCSUM_PCSD;
	}
	if (!(rxcsum & IXGBE_RXCSUM_PCSD)) {
		/* Enable IPv4 payload checksum for UDP fragments
		 * if PCSD is not set */
		rxcsum |= IXGBE_RXCSUM_IPPCSE;
	}

	IXGBE_WRITE_REG(hw, IXGBE_RXCSUM, rxcsum);

	if (hw->mac.type == ixgbe_mac_82599EB) {
		rdrxctl = IXGBE_READ_REG(hw, IXGBE_RDRXCTL);
		rdrxctl |= IXGBE_RDRXCTL_CRCSTRIP;
		rdrxctl &= ~IXGBE_RDRXCTL_RSCFRSTSIZE;
		IXGBE_WRITE_REG(hw, IXGBE_RDRXCTL, rdrxctl);
	}

	if (adapter->flags2 & IXGBE_FLAG2_RSC_ENABLED) {
		/* Enable 82599 HW-RSC */
		for (i = 0; i < adapter->num_rx_queues; i++)
			ixgbe_configure_rscctl(adapter, i, rx_buf_len);

		/* Disable RSC for ACK packets */
		IXGBE_WRITE_REG(hw, IXGBE_RSCDBU,
		   (IXGBE_RSCDBU_RSCACKDIS | IXGBE_READ_REG(hw, IXGBE_RSCDBU)));
	}
}

static void ixgbe_vlan_rx_add_vid(struct net_device *netdev, u16 vid)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	struct ixgbe_hw *hw = &adapter->hw;

	/* add VID to filter table */
	hw->mac.ops.set_vfta(&adapter->hw, vid, 0, true);
}

static void ixgbe_vlan_rx_kill_vid(struct net_device *netdev, u16 vid)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	struct ixgbe_hw *hw = &adapter->hw;

	if (!test_bit(__IXGBE_DOWN, &adapter->state))
		ixgbe_irq_disable(adapter);

	vlan_group_set_device(adapter->vlgrp, vid, NULL);

	if (!test_bit(__IXGBE_DOWN, &adapter->state))
		ixgbe_irq_enable(adapter);

	/* remove VID from filter table */
	hw->mac.ops.set_vfta(&adapter->hw, vid, 0, false);
}

static void ixgbe_vlan_rx_register(struct net_device *netdev,
                                   struct vlan_group *grp)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	u32 ctrl;
	int i, j;

	if (!test_bit(__IXGBE_DOWN, &adapter->state))
		ixgbe_irq_disable(adapter);
	adapter->vlgrp = grp;

	/*
	 * For a DCB driver, always enable VLAN tag stripping so we can
	 * still receive traffic from a DCB-enabled host even if we're
	 * not in DCB mode.
	 */
	ctrl = IXGBE_READ_REG(&adapter->hw, IXGBE_VLNCTRL);
	if (adapter->hw.mac.type == ixgbe_mac_82598EB) {
		ctrl |= IXGBE_VLNCTRL_VME | IXGBE_VLNCTRL_VFE;
		ctrl &= ~IXGBE_VLNCTRL_CFIEN;
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_VLNCTRL, ctrl);
	} else if (adapter->hw.mac.type == ixgbe_mac_82599EB) {
		ctrl |= IXGBE_VLNCTRL_VFE;
		/* enable VLAN tag insert/strip */
		ctrl = IXGBE_READ_REG(&adapter->hw, IXGBE_VLNCTRL);
		ctrl &= ~IXGBE_VLNCTRL_CFIEN;
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_VLNCTRL, ctrl);
		for (i = 0; i < adapter->num_rx_queues; i++) {
			j = adapter->rx_ring[i].reg_idx;
			ctrl = IXGBE_READ_REG(&adapter->hw, IXGBE_RXDCTL(j));
			ctrl |= IXGBE_RXDCTL_VME;
			IXGBE_WRITE_REG(&adapter->hw, IXGBE_RXDCTL(j), ctrl);
		}
	}
	ixgbe_vlan_rx_add_vid(netdev, 0);

	if (!test_bit(__IXGBE_DOWN, &adapter->state))
		ixgbe_irq_enable(adapter);
}

static void ixgbe_restore_vlan(struct ixgbe_adapter *adapter)
{
	ixgbe_vlan_rx_register(adapter->netdev, adapter->vlgrp);

	if (adapter->vlgrp) {
		u16 vid;
		for (vid = 0; vid < VLAN_GROUP_ARRAY_LEN; vid++) {
			if (!vlan_group_get_device(adapter->vlgrp, vid))
				continue;
			ixgbe_vlan_rx_add_vid(adapter->netdev, vid);
		}
	}
}

static u8 *ixgbe_addr_list_itr(struct ixgbe_hw *hw, u8 **mc_addr_ptr, u32 *vmdq)
{
	struct dev_mc_list *mc_ptr;
	u8 *addr = *mc_addr_ptr;
	*vmdq = 0;

	mc_ptr = container_of(addr, struct dev_mc_list, dmi_addr[0]);
	if (mc_ptr->next)
		*mc_addr_ptr = mc_ptr->next->dmi_addr;
	else
		*mc_addr_ptr = NULL;

	return addr;
}

/**
 * ixgbe_set_rx_mode - Unicast, Multicast and Promiscuous mode set
 * @netdev: network interface device structure
 *
 * The set_rx_method entry point is called whenever the unicast/multicast
 * address list or the network interface flags are updated.  This routine is
 * responsible for configuring the hardware for proper unicast, multicast and
 * promiscuous mode.
 **/
static void ixgbe_set_rx_mode(struct net_device *netdev)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	struct ixgbe_hw *hw = &adapter->hw;
	u32 fctrl, vlnctrl;
	u8 *addr_list = NULL;
	int addr_count = 0;

	/* Check for Promiscuous and All Multicast modes */

	fctrl = IXGBE_READ_REG(hw, IXGBE_FCTRL);
	vlnctrl = IXGBE_READ_REG(hw, IXGBE_VLNCTRL);

	if (netdev->flags & IFF_PROMISC) {
		hw->addr_ctrl.user_set_promisc = 1;
		fctrl |= (IXGBE_FCTRL_UPE | IXGBE_FCTRL_MPE);
		vlnctrl &= ~IXGBE_VLNCTRL_VFE;
	} else {
		if (netdev->flags & IFF_ALLMULTI) {
			fctrl |= IXGBE_FCTRL_MPE;
			fctrl &= ~IXGBE_FCTRL_UPE;
		} else {
			fctrl &= ~(IXGBE_FCTRL_UPE | IXGBE_FCTRL_MPE);
		}
		vlnctrl |= IXGBE_VLNCTRL_VFE;
		hw->addr_ctrl.user_set_promisc = 0;
	}

	IXGBE_WRITE_REG(hw, IXGBE_FCTRL, fctrl);
	IXGBE_WRITE_REG(hw, IXGBE_VLNCTRL, vlnctrl);

	/* reprogram secondary unicast list */
	hw->mac.ops.update_uc_addr_list(hw, &netdev->uc.list);

	/* reprogram multicast list */
	addr_count = netdev->mc_count;
	if (addr_count)
		addr_list = netdev->mc_list->dmi_addr;
	hw->mac.ops.update_mc_addr_list(hw, addr_list, addr_count,
	                                ixgbe_addr_list_itr);
}

static void ixgbe_napi_enable_all(struct ixgbe_adapter *adapter)
{
	int q_idx;
	struct ixgbe_q_vector *q_vector;
	int q_vectors = adapter->num_msix_vectors - NON_Q_VECTORS;

	/* legacy and MSI only use one vector */
	if (!(adapter->flags & IXGBE_FLAG_MSIX_ENABLED))
		q_vectors = 1;

	for (q_idx = 0; q_idx < q_vectors; q_idx++) {
		struct napi_struct *napi;
		q_vector = adapter->q_vector[q_idx];
		napi = &q_vector->napi;
		if (adapter->flags & IXGBE_FLAG_MSIX_ENABLED) {
			if (!q_vector->rxr_count || !q_vector->txr_count) {
				if (q_vector->txr_count == 1)
					napi->poll = &ixgbe_clean_txonly;
				else if (q_vector->rxr_count == 1)
					napi->poll = &ixgbe_clean_rxonly;
			}
		}

		napi_enable(napi);
	}
}

static void ixgbe_napi_disable_all(struct ixgbe_adapter *adapter)
{
	int q_idx;
	struct ixgbe_q_vector *q_vector;
	int q_vectors = adapter->num_msix_vectors - NON_Q_VECTORS;

	/* legacy and MSI only use one vector */
	if (!(adapter->flags & IXGBE_FLAG_MSIX_ENABLED))
		q_vectors = 1;

	for (q_idx = 0; q_idx < q_vectors; q_idx++) {
		q_vector = adapter->q_vector[q_idx];
		napi_disable(&q_vector->napi);
	}
}

#ifdef CONFIG_IXGBE_DCB
/*
 * ixgbe_configure_dcb - Configure DCB hardware
 * @adapter: ixgbe adapter struct
 *
 * This is called by the driver on open to configure the DCB hardware.
 * This is also called by the gennetlink interface when reconfiguring
 * the DCB state.
 */
static void ixgbe_configure_dcb(struct ixgbe_adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	u32 txdctl, vlnctrl;
	int i, j;

	ixgbe_dcb_check_config(&adapter->dcb_cfg);
	ixgbe_dcb_calculate_tc_credits(&adapter->dcb_cfg, DCB_TX_CONFIG);
	ixgbe_dcb_calculate_tc_credits(&adapter->dcb_cfg, DCB_RX_CONFIG);

	/* reconfigure the hardware */
	ixgbe_dcb_hw_config(&adapter->hw, &adapter->dcb_cfg);

	for (i = 0; i < adapter->num_tx_queues; i++) {
		j = adapter->tx_ring[i].reg_idx;
		txdctl = IXGBE_READ_REG(hw, IXGBE_TXDCTL(j));
		/* PThresh workaround for Tx hang with DFP enabled. */
		txdctl |= 32;
		IXGBE_WRITE_REG(hw, IXGBE_TXDCTL(j), txdctl);
	}
	/* Enable VLAN tag insert/strip */
	vlnctrl = IXGBE_READ_REG(hw, IXGBE_VLNCTRL);
	if (hw->mac.type == ixgbe_mac_82598EB) {
		vlnctrl |= IXGBE_VLNCTRL_VME | IXGBE_VLNCTRL_VFE;
		vlnctrl &= ~IXGBE_VLNCTRL_CFIEN;
		IXGBE_WRITE_REG(hw, IXGBE_VLNCTRL, vlnctrl);
	} else if (hw->mac.type == ixgbe_mac_82599EB) {
		vlnctrl |= IXGBE_VLNCTRL_VFE;
		vlnctrl &= ~IXGBE_VLNCTRL_CFIEN;
		IXGBE_WRITE_REG(hw, IXGBE_VLNCTRL, vlnctrl);
		for (i = 0; i < adapter->num_rx_queues; i++) {
			j = adapter->rx_ring[i].reg_idx;
			vlnctrl = IXGBE_READ_REG(hw, IXGBE_RXDCTL(j));
			vlnctrl |= IXGBE_RXDCTL_VME;
			IXGBE_WRITE_REG(hw, IXGBE_RXDCTL(j), vlnctrl);
		}
	}
	hw->mac.ops.set_vfta(&adapter->hw, 0, 0, true);
}

#endif
static void ixgbe_configure(struct ixgbe_adapter *adapter)
{
	struct net_device *netdev = adapter->netdev;
	struct ixgbe_hw *hw = &adapter->hw;
	int i;

	ixgbe_set_rx_mode(netdev);

	ixgbe_restore_vlan(adapter);
#ifdef CONFIG_IXGBE_DCB
	if (adapter->flags & IXGBE_FLAG_DCB_ENABLED) {
		if (hw->mac.type == ixgbe_mac_82598EB)
			netif_set_gso_max_size(netdev, 32768);
		else
			netif_set_gso_max_size(netdev, 65536);
		ixgbe_configure_dcb(adapter);
	} else {
		netif_set_gso_max_size(netdev, 65536);
	}
#else
	netif_set_gso_max_size(netdev, 65536);
#endif

#ifdef IXGBE_FCOE
	if (adapter->flags & IXGBE_FLAG_FCOE_ENABLED)
		ixgbe_configure_fcoe(adapter);

#endif /* IXGBE_FCOE */
	if (adapter->flags & IXGBE_FLAG_FDIR_HASH_CAPABLE) {
		for (i = 0; i < adapter->num_tx_queues; i++)
			adapter->tx_ring[i].atr_sample_rate =
			                               adapter->atr_sample_rate;
		ixgbe_init_fdir_signature_82599(hw, adapter->fdir_pballoc);
	} else if (adapter->flags & IXGBE_FLAG_FDIR_PERFECT_CAPABLE) {
		ixgbe_init_fdir_perfect_82599(hw, adapter->fdir_pballoc);
	}

	ixgbe_configure_tx(adapter);
	ixgbe_configure_rx(adapter);
	for (i = 0; i < adapter->num_rx_queues; i++)
		ixgbe_alloc_rx_buffers(adapter, &adapter->rx_ring[i],
		                       (adapter->rx_ring[i].count - 1));
}

static inline bool ixgbe_is_sfp(struct ixgbe_hw *hw)
{
	switch (hw->phy.type) {
	case ixgbe_phy_sfp_avago:
	case ixgbe_phy_sfp_ftl:
	case ixgbe_phy_sfp_intel:
	case ixgbe_phy_sfp_unknown:
	case ixgbe_phy_tw_tyco:
	case ixgbe_phy_tw_unknown:
		return true;
	default:
		return false;
	}
}

/**
 * ixgbe_sfp_link_config - set up SFP+ link
 * @adapter: pointer to private adapter struct
 **/
static void ixgbe_sfp_link_config(struct ixgbe_adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;

		if (hw->phy.multispeed_fiber) {
			/*
			 * In multispeed fiber setups, the device may not have
			 * had a physical connection when the driver loaded.
			 * If that's the case, the initial link configuration
			 * couldn't get the MAC into 10G or 1G mode, so we'll
			 * never have a link status change interrupt fire.
			 * We need to try and force an autonegotiation
			 * session, then bring up link.
			 */
			hw->mac.ops.setup_sfp(hw);
			if (!(adapter->flags & IXGBE_FLAG_IN_SFP_LINK_TASK))
				schedule_work(&adapter->multispeed_fiber_task);
		} else {
			/*
			 * Direct Attach Cu and non-multispeed fiber modules
			 * still need to be configured properly prior to
			 * attempting link.
			 */
			if (!(adapter->flags & IXGBE_FLAG_IN_SFP_MOD_TASK))
				schedule_work(&adapter->sfp_config_module_task);
		}
}

/**
 * ixgbe_non_sfp_link_config - set up non-SFP+ link
 * @hw: pointer to private hardware struct
 *
 * Returns 0 on success, negative on failure
 **/
static int ixgbe_non_sfp_link_config(struct ixgbe_hw *hw)
{
	u32 autoneg;
	bool negotiation, link_up = false;
	u32 ret = IXGBE_ERR_LINK_SETUP;

	if (hw->mac.ops.check_link)
		ret = hw->mac.ops.check_link(hw, &autoneg, &link_up, false);

	if (ret)
		goto link_cfg_out;

	if (hw->mac.ops.get_link_capabilities)
		ret = hw->mac.ops.get_link_capabilities(hw, &autoneg, &negotiation);
	if (ret)
		goto link_cfg_out;

	if (hw->mac.ops.setup_link)
		ret = hw->mac.ops.setup_link(hw, autoneg, negotiation, link_up);
link_cfg_out:
	return ret;
}

#define IXGBE_MAX_RX_DESC_POLL 10
static inline void ixgbe_rx_desc_queue_enable(struct ixgbe_adapter *adapter,
	                                      int rxr)
{
	int j = adapter->rx_ring[rxr].reg_idx;
	int k;

	for (k = 0; k < IXGBE_MAX_RX_DESC_POLL; k++) {
		if (IXGBE_READ_REG(&adapter->hw,
		                   IXGBE_RXDCTL(j)) & IXGBE_RXDCTL_ENABLE)
			break;
		else
			msleep(1);
	}
	if (k >= IXGBE_MAX_RX_DESC_POLL) {
		DPRINTK(DRV, ERR, "RXDCTL.ENABLE on Rx queue %d "
		        "not set within the polling period\n", rxr);
	}
	ixgbe_release_rx_desc(&adapter->hw, &adapter->rx_ring[rxr],
	                      (adapter->rx_ring[rxr].count - 1));
}

static int ixgbe_up_complete(struct ixgbe_adapter *adapter)
{
	struct net_device *netdev = adapter->netdev;
	struct ixgbe_hw *hw = &adapter->hw;
	int i, j = 0;
	int num_rx_rings = adapter->num_rx_queues;
	int err;
	int max_frame = netdev->mtu + ETH_HLEN + ETH_FCS_LEN;
	u32 txdctl, rxdctl, mhadd;
	u32 dmatxctl;
	u32 gpie;

	ixgbe_get_hw_control(adapter);

	if ((adapter->flags & IXGBE_FLAG_MSIX_ENABLED) ||
	    (adapter->flags & IXGBE_FLAG_MSI_ENABLED)) {
		if (adapter->flags & IXGBE_FLAG_MSIX_ENABLED) {
			gpie = (IXGBE_GPIE_MSIX_MODE | IXGBE_GPIE_EIAME |
			        IXGBE_GPIE_PBA_SUPPORT | IXGBE_GPIE_OCD);
		} else {
			/* MSI only */
			gpie = 0;
		}
		/* XXX: to interrupt immediately for EICS writes, enable this */
		/* gpie |= IXGBE_GPIE_EIMEN; */
		IXGBE_WRITE_REG(hw, IXGBE_GPIE, gpie);
	}

	if (!(adapter->flags & IXGBE_FLAG_MSIX_ENABLED)) {
		/* legacy interrupts, use EIAM to auto-mask when reading EICR,
		 * specifically only auto mask tx and rx interrupts */
		IXGBE_WRITE_REG(hw, IXGBE_EIAM, IXGBE_EICS_RTX_QUEUE);
	}

	/* Enable fan failure interrupt if media type is copper */
	if (adapter->flags & IXGBE_FLAG_FAN_FAIL_CAPABLE) {
		gpie = IXGBE_READ_REG(hw, IXGBE_GPIE);
		gpie |= IXGBE_SDP1_GPIEN;
		IXGBE_WRITE_REG(hw, IXGBE_GPIE, gpie);
	}

	if (hw->mac.type == ixgbe_mac_82599EB) {
		gpie = IXGBE_READ_REG(hw, IXGBE_GPIE);
		gpie |= IXGBE_SDP1_GPIEN;
		gpie |= IXGBE_SDP2_GPIEN;
		IXGBE_WRITE_REG(hw, IXGBE_GPIE, gpie);
	}

#ifdef IXGBE_FCOE
	/* adjust max frame to be able to do baby jumbo for FCoE */
	if ((netdev->features & NETIF_F_FCOE_MTU) &&
	    (max_frame < IXGBE_FCOE_JUMBO_FRAME_SIZE))
		max_frame = IXGBE_FCOE_JUMBO_FRAME_SIZE;

#endif /* IXGBE_FCOE */
	mhadd = IXGBE_READ_REG(hw, IXGBE_MHADD);
	if (max_frame != (mhadd >> IXGBE_MHADD_MFS_SHIFT)) {
		mhadd &= ~IXGBE_MHADD_MFS_MASK;
		mhadd |= max_frame << IXGBE_MHADD_MFS_SHIFT;

		IXGBE_WRITE_REG(hw, IXGBE_MHADD, mhadd);
	}

	for (i = 0; i < adapter->num_tx_queues; i++) {
		j = adapter->tx_ring[i].reg_idx;
		txdctl = IXGBE_READ_REG(hw, IXGBE_TXDCTL(j));
		/* enable WTHRESH=8 descriptors, to encourage burst writeback */
		txdctl |= (8 << 16);
		IXGBE_WRITE_REG(hw, IXGBE_TXDCTL(j), txdctl);
	}

	if (hw->mac.type == ixgbe_mac_82599EB) {
		/* DMATXCTL.EN must be set after all Tx queue config is done */
		dmatxctl = IXGBE_READ_REG(hw, IXGBE_DMATXCTL);
		dmatxctl |= IXGBE_DMATXCTL_TE;
		IXGBE_WRITE_REG(hw, IXGBE_DMATXCTL, dmatxctl);
	}
	for (i = 0; i < adapter->num_tx_queues; i++) {
		j = adapter->tx_ring[i].reg_idx;
		txdctl = IXGBE_READ_REG(hw, IXGBE_TXDCTL(j));
		txdctl |= IXGBE_TXDCTL_ENABLE;
		IXGBE_WRITE_REG(hw, IXGBE_TXDCTL(j), txdctl);
	}

	for (i = 0; i < num_rx_rings; i++) {
		j = adapter->rx_ring[i].reg_idx;
		rxdctl = IXGBE_READ_REG(hw, IXGBE_RXDCTL(j));
		/* enable PTHRESH=32 descriptors (half the internal cache)
		 * and HTHRESH=0 descriptors (to minimize latency on fetch),
		 * this also removes a pesky rx_no_buffer_count increment */
		rxdctl |= 0x0020;
		rxdctl |= IXGBE_RXDCTL_ENABLE;
		IXGBE_WRITE_REG(hw, IXGBE_RXDCTL(j), rxdctl);
		if (hw->mac.type == ixgbe_mac_82599EB)
			ixgbe_rx_desc_queue_enable(adapter, i);
	}
	/* enable all receives */
	rxdctl = IXGBE_READ_REG(hw, IXGBE_RXCTRL);
	if (hw->mac.type == ixgbe_mac_82598EB)
		rxdctl |= (IXGBE_RXCTRL_DMBYPS | IXGBE_RXCTRL_RXEN);
	else
		rxdctl |= IXGBE_RXCTRL_RXEN;
	hw->mac.ops.enable_rx_dma(hw, rxdctl);

	if (adapter->flags & IXGBE_FLAG_MSIX_ENABLED)
		ixgbe_configure_msix(adapter);
	else
		ixgbe_configure_msi_and_legacy(adapter);

	clear_bit(__IXGBE_DOWN, &adapter->state);
	ixgbe_napi_enable_all(adapter);

	/* clear any pending interrupts, may auto mask */
	IXGBE_READ_REG(hw, IXGBE_EICR);

	ixgbe_irq_enable(adapter);

	/*
	 * If this adapter has a fan, check to see if we had a failure
	 * before we enabled the interrupt.
	 */
	if (adapter->flags & IXGBE_FLAG_FAN_FAIL_CAPABLE) {
		u32 esdp = IXGBE_READ_REG(hw, IXGBE_ESDP);
		if (esdp & IXGBE_ESDP_SDP1)
			DPRINTK(DRV, CRIT,
				"Fan has stopped, replace the adapter\n");
	}

	/*
	 * For hot-pluggable SFP+ devices, a new SFP+ module may have
	 * arrived before interrupts were enabled but after probe.  Such
	 * devices wouldn't have their type identified yet. We need to
	 * kick off the SFP+ module setup first, then try to bring up link.
	 * If we're not hot-pluggable SFP+, we just need to configure link
	 * and bring it up.
	 */
	if (hw->phy.type == ixgbe_phy_unknown) {
		err = hw->phy.ops.identify(hw);
		if (err == IXGBE_ERR_SFP_NOT_SUPPORTED) {
			/*
			 * Take the device down and schedule the sfp tasklet
			 * which will unregister_netdev and log it.
			 */
			ixgbe_down(adapter);
			schedule_work(&adapter->sfp_config_module_task);
			return err;
		}
	}

	if (ixgbe_is_sfp(hw)) {
		ixgbe_sfp_link_config(adapter);
	} else {
		err = ixgbe_non_sfp_link_config(hw);
		if (err)
			DPRINTK(PROBE, ERR, "link_config FAILED %d\n", err);
	}

	for (i = 0; i < adapter->num_tx_queues; i++)
		set_bit(__IXGBE_FDIR_INIT_DONE,
		        &(adapter->tx_ring[i].reinit_state));

	/* enable transmits */
	netif_tx_start_all_queues(netdev);

	/* bring the link up in the watchdog, this could race with our first
	 * link up interrupt but shouldn't be a problem */
	adapter->flags |= IXGBE_FLAG_NEED_LINK_UPDATE;
	adapter->link_check_timeout = jiffies;
	mod_timer(&adapter->watchdog_timer, jiffies);
	return 0;
}

void ixgbe_reinit_locked(struct ixgbe_adapter *adapter)
{
	WARN_ON(in_interrupt());
	while (test_and_set_bit(__IXGBE_RESETTING, &adapter->state))
		msleep(1);
	ixgbe_down(adapter);
	ixgbe_up(adapter);
	clear_bit(__IXGBE_RESETTING, &adapter->state);
}

int ixgbe_up(struct ixgbe_adapter *adapter)
{
	/* hardware has been reset, we need to reload some things */
	ixgbe_configure(adapter);

	return ixgbe_up_complete(adapter);
}

void ixgbe_reset(struct ixgbe_adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	int err;

	err = hw->mac.ops.init_hw(hw);
	switch (err) {
	case 0:
	case IXGBE_ERR_SFP_NOT_PRESENT:
		break;
	case IXGBE_ERR_MASTER_REQUESTS_PENDING:
		dev_err(&adapter->pdev->dev, "master disable timed out\n");
		break;
	case IXGBE_ERR_EEPROM_VERSION:
		/* We are running on a pre-production device, log a warning */
		dev_warn(&adapter->pdev->dev, "This device is a pre-production "
		         "adapter/LOM.  Please be aware there may be issues "
		         "associated with your hardware.  If you are "
		         "experiencing problems please contact your Intel or "
		         "hardware representative who provided you with this "
		         "hardware.\n");
		break;
	default:
		dev_err(&adapter->pdev->dev, "Hardware Error: %d\n", err);
	}

	/* reprogram the RAR[0] in case user changed it. */
	hw->mac.ops.set_rar(hw, 0, hw->mac.addr, 0, IXGBE_RAH_AV);
}

/**
 * ixgbe_clean_rx_ring - Free Rx Buffers per Queue
 * @adapter: board private structure
 * @rx_ring: ring to free buffers from
 **/
static void ixgbe_clean_rx_ring(struct ixgbe_adapter *adapter,
                                struct ixgbe_ring *rx_ring)
{
	struct pci_dev *pdev = adapter->pdev;
	unsigned long size;
	unsigned int i;

	/* Free all the Rx ring sk_buffs */

	for (i = 0; i < rx_ring->count; i++) {
		struct ixgbe_rx_buffer *rx_buffer_info;

		rx_buffer_info = &rx_ring->rx_buffer_info[i];
		if (rx_buffer_info->dma) {
			pci_unmap_single(pdev, rx_buffer_info->dma,
			                 rx_ring->rx_buf_len,
			                 PCI_DMA_FROMDEVICE);
			rx_buffer_info->dma = 0;
		}
		if (rx_buffer_info->skb) {
			struct sk_buff *skb = rx_buffer_info->skb;
			rx_buffer_info->skb = NULL;
			do {
				struct sk_buff *this = skb;
				skb = skb->prev;
				dev_kfree_skb(this);
			} while (skb);
		}
		if (!rx_buffer_info->page)
			continue;
		if (rx_buffer_info->page_dma) {
			pci_unmap_page(pdev, rx_buffer_info->page_dma,
			               PAGE_SIZE / 2, PCI_DMA_FROMDEVICE);
			rx_buffer_info->page_dma = 0;
		}
		put_page(rx_buffer_info->page);
		rx_buffer_info->page = NULL;
		rx_buffer_info->page_offset = 0;
	}

	size = sizeof(struct ixgbe_rx_buffer) * rx_ring->count;
	memset(rx_ring->rx_buffer_info, 0, size);

	/* Zero out the descriptor ring */
	memset(rx_ring->desc, 0, rx_ring->size);

	rx_ring->next_to_clean = 0;
	rx_ring->next_to_use = 0;

	if (rx_ring->head)
		writel(0, adapter->hw.hw_addr + rx_ring->head);
	if (rx_ring->tail)
		writel(0, adapter->hw.hw_addr + rx_ring->tail);
}

/**
 * ixgbe_clean_tx_ring - Free Tx Buffers
 * @adapter: board private structure
 * @tx_ring: ring to be cleaned
 **/
static void ixgbe_clean_tx_ring(struct ixgbe_adapter *adapter,
                                struct ixgbe_ring *tx_ring)
{
	struct ixgbe_tx_buffer *tx_buffer_info;
	unsigned long size;
	unsigned int i;

	/* Free all the Tx ring sk_buffs */

	for (i = 0; i < tx_ring->count; i++) {
		tx_buffer_info = &tx_ring->tx_buffer_info[i];
		ixgbe_unmap_and_free_tx_resource(adapter, tx_buffer_info);
	}

	size = sizeof(struct ixgbe_tx_buffer) * tx_ring->count;
	memset(tx_ring->tx_buffer_info, 0, size);

	/* Zero out the descriptor ring */
	memset(tx_ring->desc, 0, tx_ring->size);

	tx_ring->next_to_use = 0;
	tx_ring->next_to_clean = 0;

	if (tx_ring->head)
		writel(0, adapter->hw.hw_addr + tx_ring->head);
	if (tx_ring->tail)
		writel(0, adapter->hw.hw_addr + tx_ring->tail);
}

/**
 * ixgbe_clean_all_rx_rings - Free Rx Buffers for all queues
 * @adapter: board private structure
 **/
static void ixgbe_clean_all_rx_rings(struct ixgbe_adapter *adapter)
{
	int i;

	for (i = 0; i < adapter->num_rx_queues; i++)
		ixgbe_clean_rx_ring(adapter, &adapter->rx_ring[i]);
}

/**
 * ixgbe_clean_all_tx_rings - Free Tx Buffers for all queues
 * @adapter: board private structure
 **/
static void ixgbe_clean_all_tx_rings(struct ixgbe_adapter *adapter)
{
	int i;

	for (i = 0; i < adapter->num_tx_queues; i++)
		ixgbe_clean_tx_ring(adapter, &adapter->tx_ring[i]);
}

void ixgbe_down(struct ixgbe_adapter *adapter)
{
	struct net_device *netdev = adapter->netdev;
	struct ixgbe_hw *hw = &adapter->hw;
	u32 rxctrl;
	u32 txdctl;
	int i, j;

	/* signal that we are down to the interrupt handler */
	set_bit(__IXGBE_DOWN, &adapter->state);

	/* disable receives */
	rxctrl = IXGBE_READ_REG(hw, IXGBE_RXCTRL);
	IXGBE_WRITE_REG(hw, IXGBE_RXCTRL, rxctrl & ~IXGBE_RXCTRL_RXEN);

	netif_tx_disable(netdev);

	IXGBE_WRITE_FLUSH(hw);
	msleep(10);

	netif_tx_stop_all_queues(netdev);

	ixgbe_irq_disable(adapter);

	ixgbe_napi_disable_all(adapter);

	clear_bit(__IXGBE_SFP_MODULE_NOT_FOUND, &adapter->state);
	del_timer_sync(&adapter->sfp_timer);
	del_timer_sync(&adapter->watchdog_timer);
	cancel_work_sync(&adapter->watchdog_task);

	if (adapter->flags & IXGBE_FLAG_FDIR_HASH_CAPABLE ||
	    adapter->flags & IXGBE_FLAG_FDIR_PERFECT_CAPABLE)
		cancel_work_sync(&adapter->fdir_reinit_task);

	/* disable transmits in the hardware now that interrupts are off */
	for (i = 0; i < adapter->num_tx_queues; i++) {
		j = adapter->tx_ring[i].reg_idx;
		txdctl = IXGBE_READ_REG(hw, IXGBE_TXDCTL(j));
		IXGBE_WRITE_REG(hw, IXGBE_TXDCTL(j),
		                (txdctl & ~IXGBE_TXDCTL_ENABLE));
	}
	/* Disable the Tx DMA engine on 82599 */
	if (hw->mac.type == ixgbe_mac_82599EB)
		IXGBE_WRITE_REG(hw, IXGBE_DMATXCTL,
		                (IXGBE_READ_REG(hw, IXGBE_DMATXCTL) &
		                 ~IXGBE_DMATXCTL_TE));

	netif_carrier_off(netdev);

	if (!pci_channel_offline(adapter->pdev))
		ixgbe_reset(adapter);
	ixgbe_clean_all_tx_rings(adapter);
	ixgbe_clean_all_rx_rings(adapter);

#ifdef CONFIG_IXGBE_DCA
	/* since we reset the hardware DCA settings were cleared */
	ixgbe_setup_dca(adapter);
#endif
}

/**
 * ixgbe_poll - NAPI Rx polling callback
 * @napi: structure for representing this polling device
 * @budget: how many packets driver is allowed to clean
 *
 * This function is used for legacy and MSI, NAPI mode
 **/
static int ixgbe_poll(struct napi_struct *napi, int budget)
{
	struct ixgbe_q_vector *q_vector =
	                        container_of(napi, struct ixgbe_q_vector, napi);
	struct ixgbe_adapter *adapter = q_vector->adapter;
	int tx_clean_complete, work_done = 0;

#ifdef CONFIG_IXGBE_DCA
	if (adapter->flags & IXGBE_FLAG_DCA_ENABLED) {
		ixgbe_update_tx_dca(adapter, adapter->tx_ring);
		ixgbe_update_rx_dca(adapter, adapter->rx_ring);
	}
#endif

	tx_clean_complete = ixgbe_clean_tx_irq(q_vector, adapter->tx_ring);
	ixgbe_clean_rx_irq(q_vector, adapter->rx_ring, &work_done, budget);

	if (!tx_clean_complete)
		work_done = budget;

	/* If budget not fully consumed, exit the polling mode */
	if (work_done < budget) {
		napi_complete(napi);
		if (adapter->rx_itr_setting & 1)
			ixgbe_set_itr(adapter);
		if (!test_bit(__IXGBE_DOWN, &adapter->state))
			ixgbe_irq_enable_queues(adapter, IXGBE_EIMS_RTX_QUEUE);
	}
	return work_done;
}

/**
 * ixgbe_tx_timeout - Respond to a Tx Hang
 * @netdev: network interface device structure
 **/
static void ixgbe_tx_timeout(struct net_device *netdev)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);

	/* Do the reset outside of interrupt context */
	schedule_work(&adapter->reset_task);
}

static void ixgbe_reset_task(struct work_struct *work)
{
	struct ixgbe_adapter *adapter;
	adapter = container_of(work, struct ixgbe_adapter, reset_task);

	/* If we're already down or resetting, just bail */
	if (test_bit(__IXGBE_DOWN, &adapter->state) ||
	    test_bit(__IXGBE_RESETTING, &adapter->state))
		return;

	adapter->tx_timeout_count++;

	ixgbe_reinit_locked(adapter);
}

#ifdef CONFIG_IXGBE_DCB
static inline bool ixgbe_set_dcb_queues(struct ixgbe_adapter *adapter)
{
	bool ret = false;
	struct ixgbe_ring_feature *f = &adapter->ring_feature[RING_F_DCB];

	if (!(adapter->flags & IXGBE_FLAG_DCB_ENABLED))
		return ret;

	f->mask = 0x7 << 3;
	adapter->num_rx_queues = f->indices;
	adapter->num_tx_queues = f->indices;
	ret = true;

	return ret;
}
#endif

/**
 * ixgbe_set_rss_queues: Allocate queues for RSS
 * @adapter: board private structure to initialize
 *
 * This is our "base" multiqueue mode.  RSS (Receive Side Scaling) will try
 * to allocate one Rx queue per CPU, and if available, one Tx queue per CPU.
 *
 **/
static inline bool ixgbe_set_rss_queues(struct ixgbe_adapter *adapter)
{
	bool ret = false;
	struct ixgbe_ring_feature *f = &adapter->ring_feature[RING_F_RSS];

	if (adapter->flags & IXGBE_FLAG_RSS_ENABLED) {
		f->mask = 0xF;
		adapter->num_rx_queues = f->indices;
		adapter->num_tx_queues = f->indices;
		ret = true;
	} else {
		ret = false;
	}

	return ret;
}

/**
 * ixgbe_set_fdir_queues: Allocate queues for Flow Director
 * @adapter: board private structure to initialize
 *
 * Flow Director is an advanced Rx filter, attempting to get Rx flows back
 * to the original CPU that initiated the Tx session.  This runs in addition
 * to RSS, so if a packet doesn't match an FDIR filter, we can still spread the
 * Rx load across CPUs using RSS.
 *
 **/
static bool inline ixgbe_set_fdir_queues(struct ixgbe_adapter *adapter)
{
	bool ret = false;
	struct ixgbe_ring_feature *f_fdir = &adapter->ring_feature[RING_F_FDIR];

	f_fdir->indices = min((int)num_online_cpus(), f_fdir->indices);
	f_fdir->mask = 0;

	/* Flow Director must have RSS enabled */
	if (adapter->flags & IXGBE_FLAG_RSS_ENABLED &&
	    ((adapter->flags & IXGBE_FLAG_FDIR_HASH_CAPABLE ||
	     (adapter->flags & IXGBE_FLAG_FDIR_PERFECT_CAPABLE)))) {
		adapter->num_tx_queues = f_fdir->indices;
		adapter->num_rx_queues = f_fdir->indices;
		ret = true;
	} else {
		adapter->flags &= ~IXGBE_FLAG_FDIR_HASH_CAPABLE;
		adapter->flags &= ~IXGBE_FLAG_FDIR_PERFECT_CAPABLE;
	}
	return ret;
}

#ifdef IXGBE_FCOE
/**
 * ixgbe_set_fcoe_queues: Allocate queues for Fiber Channel over Ethernet (FCoE)
 * @adapter: board private structure to initialize
 *
 * FCoE RX FCRETA can use up to 8 rx queues for up to 8 different exchanges.
 * The ring feature mask is not used as a mask for FCoE, as it can take any 8
 * rx queues out of the max number of rx queues, instead, it is used as the
 * index of the first rx queue used by FCoE.
 *
 **/
static inline bool ixgbe_set_fcoe_queues(struct ixgbe_adapter *adapter)
{
	bool ret = false;
	struct ixgbe_ring_feature *f = &adapter->ring_feature[RING_F_FCOE];

	f->indices = min((int)num_online_cpus(), f->indices);
	if (adapter->flags & IXGBE_FLAG_FCOE_ENABLED) {
		adapter->num_rx_queues = 1;
		adapter->num_tx_queues = 1;
#ifdef CONFIG_IXGBE_DCB
		if (adapter->flags & IXGBE_FLAG_DCB_ENABLED) {
			DPRINTK(PROBE, INFO, "FCoE enabled with DCB \n");
			ixgbe_set_dcb_queues(adapter);
		}
#endif
		if (adapter->flags & IXGBE_FLAG_RSS_ENABLED) {
			DPRINTK(PROBE, INFO, "FCoE enabled with RSS \n");
			if ((adapter->flags & IXGBE_FLAG_FDIR_HASH_CAPABLE) ||
			    (adapter->flags & IXGBE_FLAG_FDIR_PERFECT_CAPABLE))
				ixgbe_set_fdir_queues(adapter);
			else
				ixgbe_set_rss_queues(adapter);
		}
		/* adding FCoE rx rings to the end */
		f->mask = adapter->num_rx_queues;
		adapter->num_rx_queues += f->indices;
		adapter->num_tx_queues += f->indices;

		ret = true;
	}

	return ret;
}

#endif /* IXGBE_FCOE */
/*
 * ixgbe_set_num_queues: Allocate queues for device, feature dependant
 * @adapter: board private structure to initialize
 *
 * This is the top level queue allocation routine.  The order here is very
 * important, starting with the "most" number of features turned on at once,
 * and ending with the smallest set of features.  This way large combinations
 * can be allocated if they're turned on, and smaller combinations are the
 * fallthrough conditions.
 *
 **/
static void ixgbe_set_num_queues(struct ixgbe_adapter *adapter)
{
#ifdef IXGBE_FCOE
	if (ixgbe_set_fcoe_queues(adapter))
		goto done;

#endif /* IXGBE_FCOE */
#ifdef CONFIG_IXGBE_DCB
	if (ixgbe_set_dcb_queues(adapter))
		goto done;

#endif
	if (ixgbe_set_fdir_queues(adapter))
		goto done;

	if (ixgbe_set_rss_queues(adapter))
		goto done;

	/* fallback to base case */
	adapter->num_rx_queues = 1;
	adapter->num_tx_queues = 1;

done:
	/* Notify the stack of the (possibly) reduced Tx Queue count. */
	adapter->netdev->real_num_tx_queues = adapter->num_tx_queues;
}

static void ixgbe_acquire_msix_vectors(struct ixgbe_adapter *adapter,
                                       int vectors)
{
	int err, vector_threshold;

	/* We'll want at least 3 (vector_threshold):
	 * 1) TxQ[0] Cleanup
	 * 2) RxQ[0] Cleanup
	 * 3) Other (Link Status Change, etc.)
	 * 4) TCP Timer (optional)
	 */
	vector_threshold = MIN_MSIX_COUNT;

	/* The more we get, the more we will assign to Tx/Rx Cleanup
	 * for the separate queues...where Rx Cleanup >= Tx Cleanup.
	 * Right now, we simply care about how many we'll get; we'll
	 * set them up later while requesting irq's.
	 */
	while (vectors >= vector_threshold) {
		err = pci_enable_msix(adapter->pdev, adapter->msix_entries,
		                      vectors);
		if (!err) /* Success in acquiring all requested vectors. */
			break;
		else if (err < 0)
			vectors = 0; /* Nasty failure, quit now */
		else /* err == number of vectors we should try again with */
			vectors = err;
	}

	if (vectors < vector_threshold) {
		/* Can't allocate enough MSI-X interrupts?  Oh well.
		 * This just means we'll go with either a single MSI
		 * vector or fall back to legacy interrupts.
		 */
		DPRINTK(HW, DEBUG, "Unable to allocate MSI-X interrupts\n");
		adapter->flags &= ~IXGBE_FLAG_MSIX_ENABLED;
		kfree(adapter->msix_entries);
		adapter->msix_entries = NULL;
	} else {
		adapter->flags |= IXGBE_FLAG_MSIX_ENABLED; /* Woot! */
		/*
		 * Adjust for only the vectors we'll use, which is minimum
		 * of max_msix_q_vectors + NON_Q_VECTORS, or the number of
		 * vectors we were allocated.
		 */
		adapter->num_msix_vectors = min(vectors,
		                   adapter->max_msix_q_vectors + NON_Q_VECTORS);
	}
}

/**
 * ixgbe_cache_ring_rss - Descriptor ring to register mapping for RSS
 * @adapter: board private structure to initialize
 *
 * Cache the descriptor ring offsets for RSS to the assigned rings.
 *
 **/
static inline bool ixgbe_cache_ring_rss(struct ixgbe_adapter *adapter)
{
	int i;
	bool ret = false;

	if (adapter->flags & IXGBE_FLAG_RSS_ENABLED) {
		for (i = 0; i < adapter->num_rx_queues; i++)
			adapter->rx_ring[i].reg_idx = i;
		for (i = 0; i < adapter->num_tx_queues; i++)
			adapter->tx_ring[i].reg_idx = i;
		ret = true;
	} else {
		ret = false;
	}

	return ret;
}

#ifdef CONFIG_IXGBE_DCB
/**
 * ixgbe_cache_ring_dcb - Descriptor ring to register mapping for DCB
 * @adapter: board private structure to initialize
 *
 * Cache the descriptor ring offsets for DCB to the assigned rings.
 *
 **/
static inline bool ixgbe_cache_ring_dcb(struct ixgbe_adapter *adapter)
{
	int i;
	bool ret = false;
	int dcb_i = adapter->ring_feature[RING_F_DCB].indices;

	if (adapter->flags & IXGBE_FLAG_DCB_ENABLED) {
		if (adapter->hw.mac.type == ixgbe_mac_82598EB) {
			/* the number of queues is assumed to be symmetric */
			for (i = 0; i < dcb_i; i++) {
				adapter->rx_ring[i].reg_idx = i << 3;
				adapter->tx_ring[i].reg_idx = i << 2;
			}
			ret = true;
		} else if (adapter->hw.mac.type == ixgbe_mac_82599EB) {
			if (dcb_i == 8) {
				/*
				 * Tx TC0 starts at: descriptor queue 0
				 * Tx TC1 starts at: descriptor queue 32
				 * Tx TC2 starts at: descriptor queue 64
				 * Tx TC3 starts at: descriptor queue 80
				 * Tx TC4 starts at: descriptor queue 96
				 * Tx TC5 starts at: descriptor queue 104
				 * Tx TC6 starts at: descriptor queue 112
				 * Tx TC7 starts at: descriptor queue 120
				 *
				 * Rx TC0-TC7 are offset by 16 queues each
				 */
				for (i = 0; i < 3; i++) {
					adapter->tx_ring[i].reg_idx = i << 5;
					adapter->rx_ring[i].reg_idx = i << 4;
				}
				for ( ; i < 5; i++) {
					adapter->tx_ring[i].reg_idx =
					                         ((i + 2) << 4);
					adapter->rx_ring[i].reg_idx = i << 4;
				}
				for ( ; i < dcb_i; i++) {
					adapter->tx_ring[i].reg_idx =
					                         ((i + 8) << 3);
					adapter->rx_ring[i].reg_idx = i << 4;
				}

				ret = true;
			} else if (dcb_i == 4) {
				/*
				 * Tx TC0 starts at: descriptor queue 0
				 * Tx TC1 starts at: descriptor queue 64
				 * Tx TC2 starts at: descriptor queue 96
				 * Tx TC3 starts at: descriptor queue 112
				 *
				 * Rx TC0-TC3 are offset by 32 queues each
				 */
				adapter->tx_ring[0].reg_idx = 0;
				adapter->tx_ring[1].reg_idx = 64;
				adapter->tx_ring[2].reg_idx = 96;
				adapter->tx_ring[3].reg_idx = 112;
				for (i = 0 ; i < dcb_i; i++)
					adapter->rx_ring[i].reg_idx = i << 5;

				ret = true;
			} else {
				ret = false;
			}
		} else {
			ret = false;
		}
	} else {
		ret = false;
	}

	return ret;
}
#endif

/**
 * ixgbe_cache_ring_fdir - Descriptor ring to register mapping for Flow Director
 * @adapter: board private structure to initialize
 *
 * Cache the descriptor ring offsets for Flow Director to the assigned rings.
 *
 **/
static bool inline ixgbe_cache_ring_fdir(struct ixgbe_adapter *adapter)
{
	int i;
	bool ret = false;

	if (adapter->flags & IXGBE_FLAG_RSS_ENABLED &&
	    ((adapter->flags & IXGBE_FLAG_FDIR_HASH_CAPABLE) ||
	     (adapter->flags & IXGBE_FLAG_FDIR_PERFECT_CAPABLE))) {
		for (i = 0; i < adapter->num_rx_queues; i++)
			*********rx_ring[i].reg_idx = i;********************************t****

  Intel 10 Gigabit PCt Express Linux driver
  Cret = true;
	}

an rurn ret;
}

#ifdef********COE
/**
 * ixgbe_cacheExpre_fcoe - Descriptor xpre to register mapp Lic****the FCoEand @*******: board private structurecensinitialize
  and Cons ublisderal Public Licoffsetsas puhed  modThis blisassignedic Lis. distr/
static inline bool conditions of the GNU(ion.

 condit******* ********)
{
	int i,  GNU*********See thetGNU Gen;
	rrantn redifalsibutr
  FITNESS Ff the ea
  Th*f = &igabit PCIld have rec[RING_Fhe te];

	if (*********f/******************COE_EN****D****it
  unCONFIG_******DCB
	se along with
  this program; if DCB write to th		ls.

  You sho GNU * GNU d a copy of  GNUcens		conditions of thedcblong wit)
  C	/* find out **

   in TCul, but WI*/diste the GNU Geigabit PCI Expres GNU->tc Linux dri+ 1 distal Public Liis program is fre000-devel Mailing List <e1/*dist * In 82599, ANY number of Tx
  the f****each traffic0 N.E. clasR 9712both 8-TC and 4****THOUs are:0 N.E. TCs  : TC0 TC1 TC2 TC3 TC4 TC5 TC6 TC70 N.E. 8****:  32includ16<linux 8odule.h>
#in0 N.E. #inc
#in64h>
#include 0 N.E. We have max 8oro, OR 9712hed , where 8 ANY is0 N.E. ut WIredirection table size. Ife called "COPe <linux/less than 7124qualcensTC3, wvice.h>enough
  the 0 N.E. to add>
#inofnclude <linux/vmallosonclustartshed b.h>
#inxthe hope it fromg Parkext one, i.e.,se,
ling Lis.0 N.E. E>
#include <linu aboveh>
#inimply Lics.h>*****,include****w
#ined 8inux/vmallocvice.h>
  Aake allkt_sched.h>
#iincludt97

***************hed includeING".e al(f->indices ==nder the RETA_SIZE) && (000-deve > 3) 10 G1000-devel--
  C}
#endif /* Software Foundat = "ixe along with
  this program; if RSSoor, Boston, MAbe";
ong with
  this program; if n****HASH********* ||inclu  along with
  this program; if n*********************     conditions of the dird in this distelseorporation.";

static rssd in this dNG".

  Contact f->maskt <e1000-devel@li8_info,
	[b}*******************static conIntelSee the GNUCI Deviceblic++ton, MAInformation:
  e10_info, + s Linux drivee the GNU diststs.sourceforge.ne) should come last
 * Last blic_82599_in redistribute t and/or modify intel(R) 1der the te = "ms and conditions of thee,
  versGeneral Public License,
  version 2, by the Free Software Foundation.

  This program is distriOncexgbeknow ANY ave rec-set en <lidas publisdeviceixgb'll ions and blise,
  versbe use in the hope it will h>
#ARRANTY;toout eve Noteng ParordepublisvariousE_DEV_ID callsGLE_important.  It mustecksum.withPCI_VDEV"most"E_DEV_IDs98AF_DUAL9.h"he same tim,
	{PCn trickle downT
  ANY8259least amountnux/BE_DEV_IDnd/oed onT2),onceout en the impvoid}
 */
static struct pci_deor
  FITNESS FOR A PARTICULAR PURPO/*,
	{PCI_VDE default case = "iigabit PCI Expres0 Linux drivecensis program is fre board_82598 },
 it
  under the ter
consy of MERCHANTABILITY o in thisCorp and/oD_82ask, private data (not e Free Software FoundatioM),
	 board_82598 },
uded in this(INTEL, IXGBE_DEV_I_EM),
	 board_82598 },
	onst struct (INTEL, IXGBEDEVICE(INTEL, IXGBE_DE{
	[board_82(INTEL, IXGB}
 used) }
 */
salloc***

   - AX4),ndatmemory 9712nclu withby the Free Software Foundation.

  This program is distriW#incl9 },
	oneic Licper
  theT2),run-8259 siICE(INTdon'tTEL, IXGB8259kway, Hill	 boardat compileboard.  The poll thenetdev arrayinuxdcb_8tendUAL_PORMulti**

 , but should workbutieTEL, Ia_825gleDEV_IDPCI_VDEVICE(INSE.  2599_KX4),
	 boaror
  FITNESS FOR A PARTICULAR PURPOSE.  98_Bis program is fr = kID_8oct char ixgntel Corporat,
har i

#ifdef CONFIG_IXGBE_Dux/iofor
  FITNESS Fthe ), GFP_KERNELs die al!is program is frCorpgoto errbVenthe GINTEL, io598_Bigabit PCI ExpreODULE_DEVICE_TABLE(pci, *****

  _tbl);

#ifdef CONFIG_IXGBE_DCA
static int ixgbe_notify_dca(struct notifier_block *, I Expred long event,I Expre               opyright(c) 1999 - 2009 Intel Corporation.

on, Mis program is free sc_8259lists.sourceforge._ Giga
  Cis program is free s**

 _inderiver
  e it**********************************

  Intel _DESCRIPTION("I Express L Gigabit PCI ExprMODULE_Aork Driver");
MODUI Express L"GPL");
MODULE_VERSIONIXGBE_DEV_ID_82598_CX4_DUAboard_82598] and/orID_8if

MODULE_AUTHOR("Int:
	kfreCI_VDEVIC, unsigned; = IX
                 :*/
	ctrl_-ENOMEMXGBE_DEV_ID_82599_set_TEL,rrupt_capability - XGBEMSI-X <liMSI if sup,
	 edby the Free Software Foundation.

  This program is distriAttemptmon.config  Thiludectrl_exts uVDEVt firbeEV_Ivail <liE(IN& ~IXGBE_iesnux/ firhardwarede "i firkernelL, IXGBE_DEV_ID_82599_CO     ctrl_ext & ~IXGBE_Cor
  FITNESS FOR A PARTICULAR PURPOr
  FITNESS Fhw *hw full GNU Genhwotifntvent98 },
	/*
 vector, v_budgmodiVDEV
.E. Et's easymon.be greedVDEVIC_DRV_Lar - ss_CX4),it reallystersdoesPCI_do us much good}

sgbe_comma lot morectors
 *sterslude CPU's.  So le mapbet;

servati0 foion nly houlforsters(rux/ply) twicet firkway, Hilltors
 * ancluh>
#EG(&r causster/
	 the IVA = minruct notifier_block dca +9 - 2009 Intel Corporat_tbl);

#ifdef CON(int)(****oied w_cpus() * 2)) + NON_Q_VECTORSR registersA),
	 board_82598 EAD_REG(&cde <orretatic v a>
#iimum ofstershw.mac_infx_msix_ vector tors
 *.  WL, IBE_DEV_Istersst
 *as RSS the VMDqixgbegbe_easi *hw rp****ix_vector: theRx the Txter toeral Publie_set_itatic voi by our
	 boar
	 bousLOC_VALppteroff in1 for oose rt ixgbsesoc.h>
# fircpu  Gigabalso exceedsueue)ar - s limitesponding queue
 *
 */
 the IVA,ectionhw_infc.c.type) {
	case 2598]/* A failDEV_in to vecentry(INTEL, cludidaptefatal @adaptero adstersmean },
	is <lin_DRV_L
	ctrl_ext = IXGBE_R*******.82598_DA_DUAL_pe) {), it = DULE_DEVICor << (8 tbl);

#ifdef CONFIG_IXGBE_DCBE_DCA
static int iXGBE_IVARyy_dca(struct notifierctor |= IXGBE_IVAR_AL***********ueue & *****ueue & <t the IVARvector el 10 Gigabit PCXGBE_IVAR_AL[ar - s].), iva=vector 598] 599_COMcquireype) {
	case 	ivar &=et the IVA2598] e along with
  this program; if MSIX write toinclng evouDrive itong with
  this = ~ St - Fifth Floor, Bos,
	{PCI_VDEVion));
			ivar = IXGBRSION "2.0.hw, IXGBE_IVAR(queue >> 1));
			i = DRV_VERSION;
sFF << index);
			ivar |= (msix_vector ***************,
	{PCI_VDEVatr_sample_rndatgbe_setGBE_WRITE****CKPLANEboard_82598]* ixgbpci_8AF_DUype)	ivar &= ~pdevnotifier_err_SHIFT 3

stati  this| char ixg
			ind &= ~(0xFF } _infon, MDPRINTK(HW, DEBUG, "UAF_DUincluNTEL, IX);
}
ctrl_ext, "
char ik = "fa82599 backmon.legacy.  Error: %d\n",vents dis/* re82598rr = "ix* ixgbe_se}

outr->hw, IXGerrXGBE_DEV_ID_82599_KX4),
	{
	case i_82599 },
	{PCI_VDEVICc_82598EBor Tx, -1XGBE_DEV_ID_82599_XAUI_LOM),
	 board_82599 },
	{PCI_VDEVICE(INTEL, IXGBE_F);
		IXID_82599_SFc_82598EBrd_8far);
		break IXGs wer */hw, IXGBE_CTRLL, IXGBE_DEV_ID_82599_COMBO_BAC* tx or rr
  FITNESS FOR A PARTICULAR PURPOSE. qx dr,vect
         ails.

  You shoEICS_EX(1*        _set_ivnapi_buffer
   /*
 (*d_82)            *r
  FIT*,
	}
2598]tor |= IXGBE_IVAR_ALLOC_VAL;
			index = ((16 on, Mbe_tx_buffer
bit PCI Exprbe_tpe) {
	case i-ueue, u8 msix_ve		    *tx_bufffer_info->skb,
	*****

  I
		d_82 fulconditilean_rxtx_manymask)
{
	u32 m->dev, tx_bufferst <e	dev_kfree_skb_st <e		tx_buffer_ind_82VERSION(DRV_uct imsix_vthe tr<gbe_tx_buffer
 it patEL_SHIFTEICS_EX(1= kz_DEVICA
static int ixgbe_n        y_dca(struct notiifier_@adapter:* (queue ent,& 1)) sed - che->OR A PARlists.souapter
 *x_ring
 *
txruct ixtrin @tx_ring *adOFF, ot: these, find oeitnot in DCB ram iTC o_paramng t_info *iesponding TC of this tx_rirg when checking se, find ovx driveuct i);
		etif_    *addCE_TABLE(pc,
	{P, &se, find o    ,o)
{
	tx, 64s dis*********        [uct i]tx_is         e it and/orext = IX);
	} whilen the ts_pausedidx      ed - check      struct ixgbe_ring *used(struct ixgdel(               s dis->hw, @adapter:         struct ixgbe_ring *tx_NULLD,
 *   ClassBE_CTRL_EXT,
	           >hw,FF);
		IXGBE_Fre
	{PCI_VDe == ixgAL_PORhw, IXGBE_EICS_EX(0), mask);
		mask = (qmask >> 32);
		IXGBE_WRITE_REG(&adaThis funinclud>hw,on == _mac_82598EB) {
	
  ANY          rd_8nludeibreak;f82598APIGLE_8AF_DUALiCI_Vll deletp thy referencons
  ANY cb_i r
  FITpriorPCI_Voe_mace driverring)
{
PCI_VDEVICE(INTEL, IXGBE_->hw.mac.type                                     struct ixgbe_tx_buffer
  x_buffer_info->skb) {
		skb_dma_unmap(&adapter-dev->dev, tx_buffer_info->skb,
		              DMA_TO_DEVICE);
_info *_info->time_stamp = up in the transmit path */
}

/**
 * ixgbe_tx_is_paus                               ter->flags & IXGBE_FLAG_DCB_ENAint dcb_i = adapter->ring_feature[ABLED) {
		int tc;
		int reg_idx = tx_ring->reg_idx;
	}GBE_VICE(INTEL, IXGBE_dapteE_REG(&adapter->hw, IXGBE_CTRL_EXT,
	                ctrbuffer_info->skb) {
		skb_dma_unmap(&adapter->pdev IXGBE_IVAR(queue >> 1));
			iindex = ((16b);
	ci_ction =		    ixgbe_adapter *adr->hw, IXGBE_CTR(0xFF << inde_READ_REG(&adXGBE_IVAR_ALLOCeature[R)
{
	utor |= IXGBE_IVAR_ALLOC_VAL;
			ind            unsigned int eop)
{
	struct ixgbe_h *hw = &adapter->hw;

	/* D ixgbe_adapter *ad *   ClasXGBE_DEV_ID_82599_prog  ctrl_ext schemU Geneterm9 },proD_82hw, IXGBE_e_stam(0), mask);
		mask = (qmask >> 32);
		IXGBE_WRITE_REG(&adaptedHZ) &&
	 whichgbe_tx_is_paused(* TCuse basd_825..out  - Khw, Ihw = &ada(MSI,= -1) )_ADV  -x_desc gbe_be = Ir-de99 }d (via MODULE_PARAMRV, E- HAD_REG(&599_SF Gigab,
	  *nline vRV, ERR, Hang\n"
by miscellane, IXEAD_REG(&tatic v/BE_DEV_ID(RSS, etc.RV, */
D_82599_COr_info[eop].time_stam                                    str		masREG(hNway, Hillon * 64) +e_set_indin}
}

static inline void ixgbe_irq_rearmXGBE_WRITE_REG(&adapter->hw, IX in this die al,
       ask;

	iPROBE, ERRr->hw.mac.typsetupgbe_tx_is_p
	ctrl_ext =\n"zes tng event,
            	u32 tx,
			IXGBE_Rpter,
          ),
			IXGBE_READ_REG(hw, tx_ring->tail),
			tx_ring->nee == ixgb{PCI_VDEVIC599_SF{
		mask = (Itors
 *tx_buffer_info[eopter,
         , jiffies);
		return true;ine void ixgbe_i
}

#define IXGBE_MAX_TXD_PWR       14
#define IXGBE_MAX_DATA_PER_TXD _PWR)

/* Tx Descriptors o->skb);e itask;

	iDRV, INFO, "V_ID_82599 %s:rectQ
			"  TDH,= %u) {
	mask = (ITskb->data */ + \
	GBE__SKB_FRAGS
static void ixgbe_set_i> 1) ? "EAF_DUA" :_SKB_FRAGS Dtion =d",*******************

  ce *netdev);

/ Corporat2598]
   bit(_re FoundOWN, a copy of the e/w */
	ctrl_ext = IXKX4),
	 boar:  <%lx>\	else if (tc == in this descriptors needed, terrupt ar,
                        tion
 * @tx_ri
            	} else {
		mask = (qmask & 0xFfo->rfo[eop].time_stamp +C    x));
	urreE.  tx_desc;
		tx_desettEL, IXGBE_DEV_ID_82599_XAUI_LOM),
	 board_825     	struct ixgbe_adaponI_VDEVICE(g  ANrrupt the ice *netdev = adapecihar resourconsthe dapter
	 boon.

  T) /* TCpre-loadt;

OFF;
	X(0)*/
TEL, IXGBE_D           struct ixg                                   ->hw, IXGBE_CTRL_EXT);
	Itx_ring->next_to     = 0,
	{PCI_VDEVICE(INT clearing        void *p);
steatureerrupt and ring information
 * @tean
 **/
static bool ixgbe_clean_tx_irq(struXT,
	            fp_8259evicd_82EVICErea;
			utiona		" he drmoduver */@data: postruc* TCeue)OR A PARr *tx_CI_VDEVICE(INTEL, IXGBE_unt < tx_(unRRANTY;long 	for  ctrl_ext | IXGBEOR A PARTICULAR  =                        )	forR registersD  ANY unt < tx_routsidenux/use, eop,
	ontncluduUT
  ANY0;
		inlayncludtp);
+xgbe_includrelse {EB:
	/
	e_st fal_d_82(completes
 nt <askDD)) &&
	       (count <houling->work_limit)) {
		bool cleaned = false;
	d_82 ( ; !cleanedd_82->dma = ;

	ainill bue) ataruct sk_buff *skb;
			tx_descask        _segs ?: 1;
*d_82x_ring, i);
			tx_buffer_info = &tx_r#ifdef er_of(d_82 ((queue & 1) * 8);
			ivar = IXGBENFIG_IXGBE_DCA
  FITNESS FOR A PArotocol == htons(ETH_P_FCOE)) &&
				    skb_is_gheadlen(skrl_ext | IXGBE_CTRL_EXT_DRV_LOAD);
}
(S) >>(ue &phy.typest c complehy_nlstri_SKB_Ff);
					uct fegs = DIV_ROU				skb__not_pdaptnt*******s32r
  mor);
					ops.identifyb)->(hwdapter
 *n red char ixERR_SFP_NOT_PRESENT: the corrre	unsigneu can rediCOE */
				/dapteata chunks by size of headers */
				SUPPORToston, MAdev_err hlen = skbpter->ter,
" IXG0;
			program is (1 <		"beca= IXan unon * 64) +SFP+d = fal skb_s       wasd int sed.\n       ReunsigT),
	river af &txinstGBE_EIMa

			tx_on * 64) + = faltus s distune,
  ver },
	{Padapter *adapterzes tk)
{
	u32 m, tx_ring->tail)E_MAX_D->wb.sta tx_b		IXGBE_c char i_clehlen,
						skb_	eop_dREG(&a	{PCI_e.h"
ttyperout9 },
			 1 fo= "ix	unsiges after tra */
 Tx Que				FOUNDt completes
 * @q_ver(jiffies, ts - 1) * hterrf (tesrces after trarier_ok(netdev) &&
	             (IXGB TC1modsc = IXhlen = skb_head tx_>nex			    skbround_jiffies(
		 */
 + (2 * HZ))DD)) &&
	       (couw"
			 - Ier,
			   general soft      on.

  Tsring->tx_buffer_info[iRV, Ehe Free Software Foundation.

  This program is distrie_stopped(netdprogram ison == Ao = &tx FoundatOE
			    !tesC_ADVField*****		++adapterdXGBE_TX_D PCI
	 boar inform	breakanid ixOS netd_825_hang(ater *ada (MTUCA
st)L, IXGBE_DEV_ID_82__	 bo;
			_stopped(netXGBE_CTRL_EXT,
	                ctrl_ext | IXGBE_CTRL_EXT_DRV_LOAD);
}

	if (tc er->ev *pterter->flags &pter;
	GBE_TX_DE/*
 rss;
	 board_82598 },
	{PCI_VDEnt j   adaptertc_;

	/* L	break*tc; board_8REG(hk_tx;

	/* spag(adaptikel
	ue &vendor_idarm_free_ues(ad;rm_que	 boarapter, ((u64	 boarq_vectorevisionapter, ((u64tes += tq_vectosubsystem_ues(adapter, ((u64_packets += tota->total_packets +r->v_idx));

	tx_r+= total_packets  <%lx>Se,
			tx_riny   this
			rsskb_ */
******MAXivar INDICES * (que
	               _info[i].nextthe GNU General PublRSS].tic const(adapt                              var &= ~(0xFF << index)the GNU General PublDCBtx_ring->wor= total_by Flo
	adapt((S) >>ue & 0x3skb_shinfo(skbmac_m Yo8EB->pdev        r->v_idx));adapter,DEV_IDing *rAunt =                              FAN_FAIL ivar);
			the
	 * che.type) {v, tx_bufferl_byindex u8 msix_ing *rng of time_sta           struct ixgbe_ring *9x_ring)
 cpu) {
		rxctrl = IXGBE_READ_REG(&adapter->hw, IXGBE9u != cpu) {
	  thi2              2_RSCring->cpu != cpu) {
	|= dca3_get_tag(&adapter->*hw = &adaprx_ring - adapter->rx_ring;

	tor << index);
			I ixgbe_update_rx_dca(struct ix= ~Itx_ring->wo new next_to_ev->dev, cpu) <<
			           = total_by= ~IX          	break;
		}
	default:
		brea2censgbe_mac_825dir_pbe == reak;
2598_SR_DUAL_PORT_Egbe_mac_82599EB) {
			rxctrl &=ot, pdev->dev, cpu);
		} elseivar |= (msix_vecot, write tRL_CPUID_MASK_82599;
			rxctrl |= c Lix_ring->worGBE_D/* DGBE_DEV"

char ixgbe_sc = IXlled "COPYING"l GNU General.tN;
	der the te_DEFTC>= tx_riprivate data (not TXD_e Free Software Foundatio) 10

	/* Let Exp"

char ixgbe       *****jmsix_vj <_REG(TRAFFIC_CLASS; jEL_SHIFTL(q),a copy of dcb_cfg.rrupt */
[jB_ENAtc->path[ FloTX_Softwa].bwtoolXCTRL_DEt cpu = get_cpu();
	int q = perc
{
	= 12();
j & 1zes tt cpu = get_cRu();
	int q = tx_ring - adapter->tx_rg->cpu != cpu) gbe_hw *hw = &adapter->hw;

	iringpfN;
	pfc->hw;

	dE_DESCring *tx_ring)
{
bw_mac_825ageget_cpu();
	int[0*tx_10},
	{PCI_VDEV
			txctrl &= ~IXGBE_DCA_Tg->cpu != D_MASK;
			txctrl |= dca3_getrxC_DCg)
{_REGba_nux/t		txctrl |= dca3_getG(hwTHOUeues(stmore detaill |= IXGBE_DCA_TXClean.robin_TXCTRL(q), txctrl);
		} else iources mapXCTRx
			tconditiopyEV_Ig)
{er this
		 , IXGBEt completes
 ctrw, IXGBE ((queue & 1) * 8);
		ixgbe_update_rx_dca(struct ixgbe_adapter 2598 tx_ring->woXGBE_DEVflow				 role immediat_cpuue &fc. bytestedE_DCA		IXGBE_Rfc_fuely sRL_CPUIing)
{
2599);
			txctrl |= IXdate			DP97124thtrantoutputR),
	 board_82598 datio*********last_lGBE_DCAskb->leA_TXCTRL_DESC_D>= tx_rinBE_DCA_Thigh_wa &tx_rpu = getFAULTxgbeTHXGBE_DCA_Tlowp_dca(struct ixgbe_adapter ure[	}
	putp    ees tstruct ixgbe_adaptePAUS
				}
	putsend_xonedistribut	}
	put>hw;

	/fc_autone_DCAe detaipdateTXCTRL(C ofbycpu) <<
	in dynamicITHOUT2598_DA_DUAL_PORhen ter *adfer_infe if paused
 */
static= IXG0
			txctrl |= blic0; i < adapter->num_tx_queng when checkASK;
	
			VDEVICetcpu) <<
R 97124C ofVAR(egaByt       *********when 	   SK;
->num_tx_quewhen setu= IXGBEx_ring[i]);
	}
	ic Licux/ir->num_rx_queueess Network Dstruct ixgbe_adapTXoid ixgbe_updarx_ring[i]);
	}
}

static intR__ixpdateter,
			   eepsum.checkHZ)       <),
	 boarr_infdevice checks_queueicata ->pdev			ixgbe__free_tx_resEEPROM	struct negbe_unmap_eXGBEzes tG_F_DCB].IO (TXD_UBE_WRITE_Rrx csum&adapter->hw>num_rx_queuefdef CONFIG_IXGBE_DCAX_CSUM.type == iesources after transmit completes
 * @q_vector: structXT,
	              upE_WRI
	struct -ne IXGBE_MTxc;
	struct (neral Publ     <he Free Software Foundation.

  T(0), 
      :ev-> <net/ip6_chec_watc(EVICE *eop_descNEEDE)->next_to distriRff = IXchecsuccess, nego map _unmap__setup      <%x>\n"		if (dca_add_requ                              ,
ev->dev, cpu) <<
			         c int ixgbe_notif *unsigned ctrl_ext |->tx_timeout_count + 1);
			ixgbre-aux/ido it	   =CA
static int ixgbe_ntx_buffer    
      ->ork DrivGBE_DCA * /* CONFI__irq_= vm the tx rinotifier_ ixgbe_receive_skb - Sed long event;
	memal_p ixgbe_receive_skb - Se,enerted pac	switdapte upTE_Rnearhas 4KOVIDEGBE_DCA *k;
	}

GBE_DCA */
/**vect
statiunreak;		tx_buvf /*indeBE_Datus of receive
ALIGNure
 * @skescr, 4096 @staGBE_DCA *indearm_queKX4),
queu  vent( ((u,
 * @rx_riscripotocol == htons(ETH_P_FCOE)) &&
				   &tatic voidmapacket up the stacific board private
static voiincl_to_= IXCTRL_D       struct ixgfo->sring *ring,
    _segs0x3))ve
 * @rx_ring: r;*/
	ctrl_ext = I:
	v>hw,  the stack
 * @adapter: 
 * ixgbe_receive_skb - Send eature[GBE_MAX_TXD_PWR       14
#define IXGBE_MAX_DATA_PER firtranse_adX_SKB_FRAGSle16_to_cpu("he hope it will(unsignhw, IXGBE_CTRL_EXT,
	              upceiv(dca_add_requester(dev) =nclude "ix== 0) {
			ad= IXGBE_FLAG_DCA_ENABLED;
			ixgbe_setup_ADVIXGBEype == ixgbe and/os,
	{PCI{
		m set},
	{i mappossiTRL(GBE_ 2) /* 1 foIXGBE_R withGLE_popul) {
	(F;

#iVDEVICEV_Ire not)rd_825ion ==r */
	ll = ddutg int      EG(hw,orphaNTY; without eve*/
	case DCA_PROVIDER_REMOVE:
		if (adapter->flRL_EXT);
	IXGBE_WRITueue_index);
	if (!or
  FITNESS FOR A PARTICULAR PURPOSE.  Se* ixgbe_s Corporation, <linux.nics@intel.com>");
MODULE_DESC,
			IXGBE_READLAG_DCA_ENABLED)x causes );
			txctr Express dapter
 * RITEXGBE;

	inribut &q_vector->napi;
	boo2599 },xgbe_or== 0b->dat%uent = *(un, dx = tbrea,
	[e it and/or		mask = (qmask & 0xFs_err:_dcaadd_requester(dev) =R 0) {
			adapter->flags |= IXGBE_FLAG_DCA_ENABLED;
			ixgbe_setup_d_dca(stter);r			break;
		}
		/* Fall Through since DCA is disabled. */
	casse DCA_PROVIDER_REMOVE:
		if (adapter->flags & IXGBE_FLAGgbe_adv_rx_d) {
			dca_remove_requester(dev);
			adapter->flags &= ~IXGBE_FLAG_DCA_ENABLED;
     = 0
WRITE_REG(&adapter->hw, IXGBE_DCA_CTRL, 1);
		}
		break;
	}

	return 0;
}

#endifr/* CONFIG_IX_dca(st *rx_desc)
XGBE_RXDstatus_er - Send a completed packet upCS))
		return;

	if (stEG(hw, tx_ring->tail),
		1 << IXGBE_MA comploid ixgbe_unmap_UAL_PORT),
r.statuupper.vlan);er_infKX4),
		 * 8E_DESCstructuCS))
		return;

	if (stend up
 * @statusR hardware indication of stCS))
		rek;
	}

IXGBE_RXD_STATx descriptor ring (for a srecific queUDP) &&
		    (a@rx_deUDP) &&
		   ptor
 **/
sCS))
		red ixgbe_receive_skb(struct ixgbe_q_rx_error++;
		&}

	/* It *skb,RXDADV_ERR_TCPE)ific fo = rx_desc->wb.lower.lo_dword.hs_rMmac_82598EB) 	/*
		 * 82599 errata, UDP frames with adapteERR_TCPE) {
		u16 pkt_inith a 0 checksum can be ma
	}

	/* It                    ct ixgbe_ring *rxbe_ring **/
	ctrl_ext hecksum can r->hw, IXGBE_CTRL_EXT,
	              ueue_ingbe_adv_rx_desc *rx_descags & IXGBE,
				     sPOLL)) {
		if (adapter->vlgrp && is_vlan && (tag & VLAN_VID_MASK))
			vlan_gro_receive(napi, adapter->vlgrp, tag, skb);
		else
			napi_gro_receive(napi, skb);
	} else {
		if (adapter->vlgrp && is_vlan && (tag & VLAN_VID_MASK))
			vlan_hwaccel_rx(skb, adapter->vlgrp, tag);
		else

			netif_rx(skb);
	}
}

/**NABLED))
		return;

	/* if IP and error */ hw indicated a good cksum
 * @adapter: address of bLT_DEBUG_LEVEL_SHIFTre
 * @status_err:NABLED))
		reication of status oI Express  * @skb: skb currently being received and modified
 **/
static inl skb->datixgbe_rx_checksum(struct ixgbe_adapter *adapter,
				     un->hw.dca_add_requestixgbeTx R
	struct D_82b->da= IXGBE_FLAG_DCA_ENABLED;
			ixgbe_setup_dca(adapteT			break;
		}
		/*Fall Through since Dlan && ixgbencluAT_VP);
	dex) &&
	r weak-orderp, count = 0;
hile (cleaned_cou) {
			dca_remove_requester(dev);
			adapter->flags &= ~IXGBE_FLAG_DCA_ENABLED;
			IXGBE_WRITE_REG(&adapter->hw, IXGBE_DCA_CTRL, 1E_READ_REo->skf recei = adapterL_EXT);
	Ie_adapter *adapter = q_vector->adapter;
	struct napi_struct *napi =
pter-->hw.kb(struct ixgbe_q_vector *q_vec
 * @rx_riinde    bi->pageip_summtatic void ixgbeeaturefer_info[i];

	while e_index);
	if (!(ad-) {
		rx_desc = I inlAllE_RX_Drdered memory model archs,
	 * such as IA-64){
			if (!bi->page) {
				bi->page = allocdx - 64) >> 4;
				else
/**
 * ixgbe_rx_checksum - indicate in skb if hw indicad cksum
 * @adapter: address of board private stvector |= IXGBE_m is free s        rrupt and ri hardware indication of status of receive
 * fer_info[i];

	while gbe_adv_rx_descixgbeR	rx_desc = *skb)
{
	u32 status_err = le32_to_cpu(rx_desc->wb.uc Licens && (tag	bi->page =cksumb;
			skb = netdrecemap ) {
				bi->page = alloc_page(GFP_ATOMICNABLED))
		return;

	/* if IP and error */
	if ((status_err & IXGBE_RXD_STAT_IPCS) &&
	    (status_err & IXGBE_RXDADV_ERR_IPE)) {
		adapter->hlf page if we'CS))
		 = adapter_buffer_ine_adaptew *hw,
                    CS))
		return;

	if (statuage(pdev, bi->page,
			           _rx_error++;
		CHECKSUM_UNNEead.pkt_addrip_summ}

	/* It must b  (PAGE_SIZE / 2),
			         s will result in a 16 byte alignEVICE);
		}

		if (!bi->skb) {
			struct sk_buff *skb;
			skb = netd;

			bi->skb = skb;
			bi->dma                        (rx_er,
                                   struct ixgbe_ring,
                                   int clevector |= IXGBE_I Express Lbuffers;
			}

			/*ci_dev *pdev = adapter->pdev;
	union ixgbe_ad                 hange_mtube_rr_inf/
			Mr->hw;
TT_VPfer Unitruc @dapter:{
			/* sstrucfxgbechedule ixgbe_setup_dnewo(unesc-w valu(&adaper->hw;
fetdedescr

	/* Rx csum disabled */
	if (!(adapter->flags & IXGBE_DEV_ID_82599_COdr_info(un_buffer_ietpackets *dapter,
     
statierr & IXGBE_			tx_buffer_info = &tx_rdapter_ Fou(tch;
		eopre-a	rxckt_infdescstati + ETH_HLENlo_dworFCS_LEN@status res< 68GLE_Pgro_recdv_tx_    s    blemsDCA_Pomr->hw, I dev_get_d(.lower.lDV_Rtati ((rx_desc->>IXGBE_DCA_RJUMBO_FRAMEiver_s(INTEL, IX *)dNVA(pdev_TX_DESC_ADV(*tx_ring,dr_inLED; resksum.%;
			ring->nele16_to_dapter->mtuR_REt_rsc_;	swit98 },
ee_getc quebef1 fovlgrLED;ICE(Iordwar_cpuges a queue>wb.lower.summed =(strucrun IXGrn (le32(rx_rn
 **/
r_inflocke_adapter BE_FLAG_DCA_ENABLED)
			break;
openbe_rlgrpdoc.hn asc->wb.lower.lo_dwois made ac mapreturn rx_desc->wb.lower.lo_dword.hs_rss.hdr_info;/* Rx csum disabled */
	if (!(adaptne u16	if (adapter-.mac.te uct s), iva ; !c(strvlgrp*ixgbe_transform_rsc_queue(struct retuk_buff&ada
	 bokets  (IFF_UP{
		u32 iapi_g>lenx_ring	struct e.h"oid ixE_RXDT_VP);
	ixgbe_
			bi !ixnlinetx_hun598EB) {
ng Parstruct ixretuhandlx_risVICE(INTE->prL, I
			OSng Parwatchdog_8259_sizecksumed,kb_shdapter-st_RTXislse ifiESC_N2),
	 brsc_queue(strimityL, IXGBE_DEV_ID_82599_COuct nfo;
}

static inline u32 _count(union ixgbe_adv_rx_desc *rx_desc)
{
	return (le32_to_cpu     <%lx>ctiol	   uct sduc Lice);
	ev_get_dKE_THRESHOLD))) {TESTING anybody stopping the acket
 * BUSYuse
(struccarri&& (sfrn (le32_t	swite == ixgbst_size;
index = ((A_TXCTre
 * @status_err:
/**
 * ixgbe_rx_UNT(S) (((S) >> IXGffer_info[eop].f (dcbuffer *rx_buffe->data_lnext_buffer;
	struct sk_buff *skb;
	unci_dev *pdev = adaptunt = 0;
	u32 len, staterr;
	ur6 hdrREAD_REG
	/* Le->queue_index,
			IXGBE_RD_SHIFT_irq, rsc_count = 0;
	u32 len, statereqBE_Fndex,
			IXGBE_Rup_9_KXC1 *, rsc_count = 0;
	u32 len, stateup *next_rxdtx_cksum;
	un_USE_COgbe_rx_buffoff = IXGBE_TFCupclean
 **/
E_DEBE_CGBE_     ((eop_desc->wb.st->hw.E_FCOE */

	i o_clean;
	r:uct ixgb= 0;
terrupt and ri	unsigned int total_rx_bytetaterr;
	u16f (*work_done >= wsigned int i, rsc_count =n
 **/
starol of h/w */
	ctrl_,
                     (hw,-  net_des_transform_rsc_queue *skb)
{
	unsigned int frag_list_size = 0;

	while (skb->prev) eive98AT_vect *rx_w0;
			 IXG		frag_listr_info += skb->len;
		skb->prev = n              de-k_buf) {
retukb)->frOS
	 boarEAD_REG(&x_irqTC0,unDEVICE(Iif (i s        _CX4)INTEL< (8 interr, IXGBE_next global MACbe_tx_b_SHIFsu0;
			stopdapter->EAD_REG(,xgbe_if (!bi->page	skb->data_l;
	struct iree_macdL, IXGBE_DEV_ID_82599_COr_inf,
                               int *work_done, int work_to_do)
{
	struct ixgbe_adaX_PS_ENAICE(while (staterr & IXGBE_RXD_STAT_DD) {+;

		if (rx_ring->flags & IXGBE_RING_RX_PS_ENAdone >= work_to_do)
			break;
		ing->rx_buffer_info[i];

	while (statFLAG_DCA_ENABLEe Free SoftwarPM.lower.lo_dword.hsresubytes = 0,->tx_timeout_err & IXGBE_static inline u32 arm_quegtatirvOE
	ixgberame_header) +
		_adv_rx_desc *rx_desc)
{
	return (le32_tou32 *adapte->tx
   powerwb.uteixgbe_qPCI_D0hw r->txb);
ore_frags,
			e_irq_rearm_queues(strr->v_idmemEVICE);
	_READ_REG(hw,printk(trucader "PS_EN: Can;
		TXCTRL(k_tx_hang(aksum.			i = 0;spen*(unsigned long ivate }shinfo(skbma4_DUApter *a      wak    om_d3ixgbe_qe detindex,
			IXGBE_R
			"tx_buffer_info[neal_rx_bytes = 0, to_offset,
			                   upperter,
			   mware know  inl{
		mask = (to_cpu(rx_dboar (PAGE_SIZE / 2)) ||
		X_PS_ENABLED) {
			hdr_inf******WRITE_REGer this
		 hw,ruct ixWUS, ~    nd the first packet and thenaned_count)
{
	string,ext_to_watch;
		eop_= 0;
	u32 llen;
		}

		i++;
	(strucr->v_idattachrror);
	rx_buffer_info "Intel(R) 10 GigabPM_rearieve we hung RINTK(Phute(pdeuffer_info->page_dma,arrant*ues(strfo->,
			               PAGE_SIZE / 2, PCI_DMA_FROMDEVICE);
			rx_buffer_info->page_dma = 0;
			skb_fill_page_desc(l_ext | IXGBE_CTRL_EXT_DRV_LOAD);
}

skb,ctrlSee trIXGBskb,wuAD_RE*********wolpter->netdev);
	PMto_cpuretvax_bu	rxctx_ring-LE)
			rsc_codet = ixgbe_get_rsring, i);
		prefetch(next_rxd);_single(pdev, rx_buffeerr & IXGBE_RXD_STAT_DD) {stats.bytes +
	unsigned int i, rsc_count          PCI_DMA_FROMDEVICE);
			rx_buf
			 = 0;
	unsigned int total_bytrag list ow & IXGBE_RXD_STAT_ {
			if info(avbuffer_info->panks by svalrx_desc *rx			nex
	 board_82598 ];
	ats.packets+
   rxE_DCArror);
	rx_bswitnd/orx_rill-m_ID_ITHOUTecti"
#ion skb;
cDEV_== 8) {
			ress Netw];
		********WUFC_MCton, MAufferstruct ixREAD

		nIXGBE_RX_DFCTRadapte IXGBE_          
			_MPTRL_Ct)
			i = 0;

		nERR_MASK) {
			_bufferTX_WAKEly(cGBE_RXDADV_ERR_FRAME_ERR_MASK) 
			dev_kee_skb_irq(skb;
			GIO_DIE);
	t_desc;
		}

		ixgbe_rx_chcksum(dapter,al_rx_bytes += skb->len;
		toif (,i];
	fo->s)
{
	u32 m;

		skb->protocol = eth_typC,      +;

		skb->protocol = eth_type_tr     +;
		xt_desc;
&an = if (adapter->hw.mac.type == ixdapter-fo->page) != 1))
		stri= sk_info *r_info->page) != 1))
				rx_buffe
				     IX = !!];
	uffer_info->dma = 0;
			skb_put(skb, len)ter->hw;

	/ize +=t(rx_buffe;
		}

		if (upper_len) {
			pci_unmap_page(pdef_len > IXGBE_RXDADV_NEXTP_MASpm_message_},
	{tXGBE_REOP) {
			ense fordaptlen);
				if xtp = (staterr & TP_MAS&  IXG skb;
				next_buffer->dma = 0;
	or err IXG_offseci;
		pare ixgsleep>skb = sk adapter->nr_info->page) != 1))
				rx_bufed_cou(skb)->nr_frags,
			       3hof (t32 txoff = IXGB_desc);

		if (rsc_count) {
			u3*skb;
			tx_aterr & IXGBE_RXDADV_NEXTP_Merr & hardware, onme is too slow */
		if (cleanedE) {
	ckets + retunt cSYSTEM_POWER_OFFgbe_alloc_fo->page) != 1))
		cleaned_	}

		/* use prefetched values */
		rx_desBE_DEV_ID_82599_updatbufferuestUd_couriver twarethe istic16_tuctrlhout XGBE_FLAG_DCA_ENABLED;
			ixgbe_setupp, count = 0;
ed_count);

XGBE_CTRL_EXT,
	                ctrl_ext | IXGBE_CTRL_EXT_DRV_LOAD);
}

u64 total_mpN;
		rxnfo[ii,l cleed_rtrans, mpc, bprc, lxon512)
ff, xonct i_toAR re(q));
		if (adapter->hw.mac.type == ixgbe_mder)rs_skb]);
	}GBE_D****************16Intel 10 Gigabit PChw_bufno_dmae_adv_rx_de+(&addapter->pdev->dev, cpuDADV_ERR_FRAME_ERR_MASK) QPRDC(is += ***********************************

  Intel 10 G_ROUND_UP(+t Information:
  e1s LiROUND_UPRL_CPUID_MASK_ROUND_UP(ddtotal_rx_pac+ (8 * directt);

.crcerrets );

		/* probably a little RCERRSrx_d****************8LEVEL_SHIFTribuX(1)acket  CONFIFT;
		us	skb->tre,
  vers
	 boarimit)0goto nsizeofDADV_ERR_FRAME_ERR_MASK) MP_bytes;
	e_crc_eof)+=if (BE_DCA_RXCTRLan_rxompc[i]te MSI-X
 * -
			sizeet_stats.rx_ts.
 **/
staapter
 *           struct ixgbe_ring *rx_r10 Gigabit PCan_rxornb
static DADV_ERR_FRAME_ERR_MASK) RNB_bytes;
	ctor;
	int i, jqpt_vectors, v_idx, r_idx;
	u32 maskQPT	q_vectors = adapter->numbmsix_vectors - NON_Q_VECTORS;

	/*
B * Populate the IVAR tableprsix_vectors - NON_Q_VECTORS;

	/*
	R* Populate the IVAR table 	for (v_idx = 0; v_idx < q_vectors;Bv_idx++) {(q));
		if (adapter->hw.mac.type == ixgbe_mrs = adapter->nupxonrx_vectors, v_idx, r_idx;
	u+= total_rx_packets;
	rx_x_packets;
	rx_ring->PXONRXCNTbytes;
	xr_idx,
		         ff             adapter->num_rx_queues);

		for (i = 0; i < q_vector->xr_count;FFi++) {
			j = adapter->rx_rinqprdfor (v_idx = 0; v_idx < q_vectors; vx_bytes;
	desc = IXGBEr_idx,
		                       adapter->num_rx_queues);

		for (i = 0; i < q_vector->r>rxr_count; i++{
			j = adapter->rx_ring[r_idx].reg_idx;
			ixgbe_set_ivar(adapter, 0, j, v_idx);
			r_idx = fifind_next_bit(q_	        tors = adapter->nu    t            adapter->num_rx_quocol == htons(ETH_P_FCOE)) &&
				    skb_isxr_count; T_ivar(adapadapter->rx_ring[r_i_idx = find_next_bit(q_vector->txr_idx,
			                      adapter->nind_next_bites,
			   _TXCTRL(q));an_rxogprigureDADV_ERR_FRAME_ERR_MASK) GPRChw rsc d_825a: hardstruct ixgD_UPSINGLEsuRL, 2);

	for (r->tx_eitr_-ytescrc_eofbuffer ng *r mixed */
hw *hha);
	32 bi;
#ied FCb_8259e ].cpue,
  versev_get_dtes += ddp_bytes;
		total_rx_packets += DIVtmpX
 * interrupts.
 *gotr_param;
		else if (q_vector->rORCadaptetmrl =    v_idx);
	else if (adapterH) & 0xF;R) 14== ixgbit= IXGpter>hw, IXGBE_DCA_
		           (ac.t<< 32zes the
	 * ch
		     t         v_idx);
	else if (adaptTr->hw.mac.type == ixgbe_mac_82599EB)
		iTgbe_set_ivar(adapter, -1, 1, v_Tdx);
	IXGBE_WRITE_REG(&a
	/* s->hw, IXGBE_EITR(v_idx), 1950)<= tparam;
		else if (q_vector->TO		dev_kask);
}

enum latency_range H)var(adevice *n);
	IXGBE_WRITE_REG(2)
	rxr_param;
		else if (q_vector->Lt; i++) E_EITR(v_idx), 1950)mss &id = 255
};

/**
 * ixgbe_update_itit(q_veE_EITR(v_idx), 1950)_DESm ixgC, mask);
}

enum latency_ran= ~IMALE_Mto adapter
 * @eitr: eitisruct napi_struct *, int);
/**
last ISbe_cctrl |= IXGBE_DCA_RXCTRL_HEAD @eitr:cctr_param;
		else if (q_vector->FCCr_counpackets during thioerpdmeasurement interval
 * @bytes: OERPDe number of bytes during itr_param;
		else if (q_vector->    xr_coun new ITR value based 
	/* set up to autoclear timer,nts dTring the last interrupt. dwon packets and byte
 *      counts DWuring the last interrupt. dwThe advantage of per interrupt
 *  DW   cog->cpu = cpu;
	}
	put_cpu()
{
	u32 m= 2,
	latency_invalid = 255
};

/**
 * ixgbe_update_itr - date the dynamic ITR value based on statistics
 * @adapter: pointslice
 * @itr_settin            v_idx);
	else if (adaptereslice
 * @itr_settin;

	/* set up to autoclear timer, andeslice
 * @itr_settinEIAC, mask);
}

enum latency_range eslic}
	s > type == ixgbe_mac_82599EB)
	Bxr_counackets during titr(s+=ss >                   mitr_param;
		else if (q_vector->Mxr_counr *adapter)
{
	struct ixgbe_q_vector *q_v, u8 itr_setting,
  - u32 eitr, u8 itr_settinror_param;
		else if (q_vector->RO                    prc64C, mask);
}

enum latency_ranPRC      tr_done;


	/* sim127e throttlerate management
	 *   127_info[i].next

	/* sim255e throttlerate management
	 *   255 ints/s)
	 *  100-1249511e throttlerate management
	 *   51r->hwB/s lowest (100000 023ts/s)
	 *   20-100MB/s low   (200023-20MB/s lowest (100000 522ts/s)
	 *   20-100MB/s low   (20052BE_EIctor;
	int i, j,le;

	if (packets == 0)
		goto upLE   wh2)
	type == ixgbe_mac_82599EB)
	_itrT    wh= 2,
	latency_invaltd = 252)
		case fved  on statistics
 * @adapter: po adapter->eitr_high)
			rr_idxl = bulff > adapter->eitr_lou;

	if (packets == 0)
		goto upUdapter->eitr_high)
	g The advantage of per interrupt
G    com_msix(struct ixgb
	/* set up to autoclear timer,M    comgisters_eitr(err

	i-;
		8EB_	          Iware
 2598ATncluIXGB_825xDDPed FCont;
			~511;
		tot = bulk +_latency;
		break;
	caseh)
		-= ~511;
		total
update_itr_done:
	rection is made to be called by et
	IXG-= (ing interrup* (dworZ.data) &
	       s += total_pac
	case bulk_latency:
		if (bytes_perint <= adapter->eitr_higrf;

	if (packets == 0)
		goto upFhere.
 */
void ixgbe_j;

	if (packets == 0)
		goto upJ
	}

update_itr_donetpAC, mask);
}

enum latency_rangPR ints/s)
	 *  100-12tmple throttlerate management
	 * T  0-20MB/s lowest (1000x;
	uction is made to be called by etpt0 ints/s)
	 *   20-100MB/s low   (T0000 ints/s)
	 *  100-12t9MB/s bulk (8000 ints/s)
	 */
	/* That was last interrupt ttmeslice? */
	timepassed_us = 10000T0/eitr;
	bytes_perint = tytes / timepassed_us; /* bytes/usecT*/

	switch (itr_setting)t{
	case lowest_latency:
		if (bytesTperint > adapter->eitr_lb
	return retval;
}

/**
 * ixgbeB    co	switFTC0, in

				l/* include Dion.

  Th>num_rx_queuestatsettingon_eop_dere_msix(struct ixgb eitpkt_inxask);
r->num_rx_queueruct ixgbeed
 er *adre_msix(struct ixnly(struc((queue & 1) * 8);
			ivar = IXGBadapter->eitr_low)
to be calledtor->adapter;droppex_ring -_ring;

	r_idx = find_length;
	u32 new_itr;
	u8 curreng, *tx_ring;

	r_idx = find_crc;
	u32 new_itr;
	u8 current_itr,_vector->txr_idx, adapter;

		ix
	u32 new -
			sizne u16 ixgbe_get_hl ixgbe_c- Tean_rbuff-S_RTse;
		for ( ; !cleanedfo = &txop_desn         _TX_DESC_ACI_VDEVICE(INTEL, IXGBE_l ixgbe_XGBE_TX_DESC_ADV(*tx_ring, i);
			tx_buffer_info = &tx_ring->tx_buffer_info[i];
			clee_hdr) -
			sizeof(struct fc_frame_header)ede Dgbe_set_ivx_ringistersi == eopl ixgbe_c= tx_buffer_info->skb;

			if (cleaned  loveinter to) {
				unsiRSCCIXGBE_Rnewer mixed */
 bytecount;
		 TX_WAKE_THRESHOLD))) {nsmit completes
 * @q_32 len, sl ixgbe__short_circuital_rx_by! |= IXGBE_IVAR_ALLOC_VAL;
			index = ((16 re MSI-Xr->tX hardEUE & = true_mac_82598EBsESHOLD rs i
			 -1,_rx_qu     er_i8AF_DUAL_POREIAM,         EN_SHen += fra_rx_quw boarrs i****** EIMBE_IVAEICS IXGBEnt; i+tor:rx_r_rx_qING"t_desc;
		}

		ixgbe_rx_chtor-x_que+= totator-_TCP_TIMER |_itr,
		   _OTHERtes;
	              s - 1) * hlen

	switue
 GBE_    97124ve_825info(stx/rxc = reg_idx >> 2;*adapter,
************************	              DMA_TO_DEVICE)EVEL_SHIFT                        _count + 1);
ct ixgbe_adapter *aqs;
	XOFF, oth||     TXOFF, ot*q_veat re|= ((u64)1, IXdx = 

	switC     dex) &&
	struct ixgorrensDEV_rxse
			nt ixgo->s		goto ->page rq_rearinline void ixgb,hat rITR(                   :pkt_in_tx_buffelean_r    queue after this
		          es the_clean.
		 */
		smp_mb();		if (__rent_itr = m r_idx + 1);
:			unsigned int hlen = skbt_itr) {
	len(skb);

				/* gso__q_vespeed_fibers is currently only valid;

	/* Letcase low_l tency= skb_shinfo(skb)->gso_segs ?: 1;
#ifdef IXGBE_FCOE
				/* adjust for FCoE Secase low_latency:
		nffload */
				if ((adapter->flags & IXGBE_FLAG_FCOE_ENABLED)
				    && (skb->protocol == htons(ETH_P_FCOE)) &&
				    skb_is_gso(skb)) {
					hlen = skb_transport_offset(skb) +
						sizeof(strcase low_latency:
		nrame_header) +
						sizeof(struct fcoe_nfo[ie CB drense fornegoti                vo                    IN{
		/LINK_TASK_vec CB drive);
					dapter-r a ertisn be 
/**
!dapter-stringue & 0x3			/I_DMlink & ~IXGBE_inde*q_vPABLE) &&
	    (eicr & IXGBE_EICRixgbe&dapter-, &t ixgbe_adaned_counPABLE) &&
	 = 0;
	eicrGPI_SDP1)) {
		Dwrite to cixgbes stoppedt ixgbe_adates = ixgb *adapter, u32 eicr)
{
	strucNEEDe_hw *UPDAT			IXGBE_WRITE_REG(hw, IXGBE_IVAR(t ixgbe_hw *hw = &) &&
	       (count BE_FCOE_DCgned
		new_itr = 20000; /* aka hwitr =tranw tx_buffer_i= skb_shinfo(skb)->gso_segs ?: 1;
#ifdef IXGBE_FCOE
				/* adjust for FCoE Sequen *hw = &adapter->hffload */
				if ((adapter->flags & IXGBE_FLAG_FCOE_ENABLED)
				    && (skb->protocol == htons(ETH_P_FCOE)) &&
				    skb_is_gso(skb)) {
					hlen = skb_transport_offset(skb) +
						sizeof(struct  *hw = &adapter->h);
	}

	return;
}

static void ixgbe_check_fa        *adapter, u32 eicr)
{
	struct ixgbeMOD*hw = &	swit    s);
		lecPCI_al oscill= frag_>next_tDEVICE(Ix_itradapte10     * ixgbCOE */
				/* multiply data chinfo->pag;
				total_bytes += bytecount;
			}
			ixgbe_unmap_and_free_tx_resource(adapter,
			                "               tx_buffer_info);
desc->wb.status = 0;
			i++;
			if (i == tx_ring->count)
	on * 64) +le_woreop = tx_ring->x_buffer_info[i].next_to_watch;
		eop_ffies, tate_ the interrupt */
_NEED_LINK_UPDAdx = find_first_bit(q_vector->t ixgbe_hw *hw =_GPI_pterN_SH TC0,~(0xFd_8259or DA Twinastrunnt segsA_TXCT		unsigned int hlen = skbe_write_eitr(q_vector);
	fp_event(struct ixgbe_adapter *adapteadapter *afer_info[i];

	whDESC it int is currently only valid it in = ~I filclean over */b_shinfo(skb)->gso_segs ?: 1;
#ifdef IXGBE_FCOE
				/* adjust for FCoE Seauses without clffload */
				if ((adapter->flags & IXGBE_FLAG_FCOE_ENABLED)
				    && (skb->protocol == htons(ETH_P_FCOE)) &&
				    skb_is_gso(skb)) {
					hlen = skb_transport_offset(skb) +
						sizeof(strauses without cl);
	}

	return;
}

static void ixgbe_chec/
		q_vecet_drvdata it intausese wrisID_MASata nt c0*********************************** Corporation.

  Thources after traXCTRL_CIT_DONEx_queues);

	&ailed++;
				goto no_ it int
		    mask)
{
	u32 mask;

	i->tail),
			tource(adapfinishe
	 * re-signed long ev) {
		mask = (Iignorn sTXOFnge
	 * AT* with ts (unsign}	switDGBE_
	 * R < adapter->num_txTXCTRL(st_size;A_TXCTx_desc->wb.upper.status_erext_to_watch;
		eoor->eitr,
		            ut clearing, which later bBLED;eicr sableto EICR.
	 */
	eicr = IXGBE_READ_REG(hw, IXGBE_EICS);
	IXGBE_WRITE_REG(hw,            &ffload */
				if ((adapter->flags & IXGBE_FLAG_FCOE_ENABLED)
				    && (skb->protocol == htons(ETH_P_FCOE)) &&
				    skb_is_gso(skb)) {
					hlen = skb_transport_offset(skb) +
						sizeof(str            &rame_header        PAGE_SIZE / 2dapter *adaptert - 1);
		} else {
			next_buffer = &rx_ringeicr  */
		B) {
		maskEG(&adapteense foreicr url =hw, IXGBE_EIMS_cpuor Full ls.

  You should ;
			IXGB 1);
		}om/*
		en >adapteory w *adapter, u32 eicr)
{
	struct iWtimeDOGpter *adae along with
  this program; if  void ixgbe_checgbe_alPABLE) &&
	 checixgbIXGBE_W&EG(&adapte ixgbe_iupount = 0;
		r erle_queuo the Free Softwardationn, Inc.,
  51 Franklin St - Fifth Floor, Boston, MAonfigure_msix - C                   tel 10 Gsh */
}

statifceues(stixgbedx = t             2598EB) {
		mask = (IXGBE_Eif ddp  "Ininfo *ik);
		IXGBE_WRITE_REG(&adaptertic voidrx_desgbe_adapteratic cer);
ime_= tx_		smp_mb,/* for conteicr & inli_REG in
+r->txr_idx,
			             cy_rangRYe_hw *hIMEOUT********lear-by-read.  Reading with EIC void ixgbe_check_s         q_vector->rx_itr,
		 MSmsix_clean_C_LS   whiter, 1, j, v_lse {
		mask & 0xFFF
	struct ixgbe_dapter->_EIMS, mask);SP or eradapter *adter
 * xt_rxd;
	structktch(next_rxd);_desc- har_rx,
	int 16 hdr(...) */
		r_idx = find_first_bit(q_vector->rxnfo[imflclow_latency:
		if (bytes_periMFLCNs distnfo[ifcSC_DCArement interval
 * @bytes: tFGadapter	int i,)
			(x = fi********idx, or *Er->txr_countt i++) {m_tx_q*********ecto_TFCE_802_3XMS_RTX_QUEUE & qmas->numrctE_RXDADV_ERR_FRAME_ERR_MASK) {
			dev_kf = 0;rm resuf (packets == 0)
		goto upMCbe_coxr_count; i++) {
		r_i->total_by
			er->tx_ring[r_idx]);
		ter->n********     = 0;
		tx_ring->toif (!et,
			     E_MA         %s NIC L		scis Up %s) {
		>txr_idx"F	   Ci];

	dx))ng->next_to_cleges a qunam

#incl+ 1) /*;
	struct ixum_tx_qu_hw *SPvoid10GB_FULL ?+= total_rx_"10 Gbpstimeo->txr_idx, single unshared vector rx clean all queues)
 * @irq:  "1sed
 * @ "unEL, n *eoed"))x_queues);

((count; i&&r_idx;

id ixRX/TX* @data: pointercount; i
	str ixgbe_q_vector *q_vettor =ct ix "None")__netdataxt_rxd;
	structnrn (le32_to             _idxordwordnt segs,of hung        st_s= "ixgCA_TXCTRL_
	intRITE_idx distributa)
{etical maximum wire lse {
		mae detailtor = data;
	struct ixgGBE_D the firstruct ixgbe_ring     *tx_r(u64)1 << q_vector->v_idx));
	napi_schedDownANDLED;
}

/**
 * ixgbe_msis distxt_rxd;
	struct ixgbe_rx_burx_queummed = pter->rx_ring[r_idx]);
		rx_riorporation, <linux.nics@intel.com>");
MODULE_DESC        gbe_ring *tx_f receive
     X_WAK      struct ixgbe_r!e
 * @rx_ri             DLED;

	XGBE_EIMS_EX(0), mst <e1truct ixgta)
{
0xFFFFFFFXGBE_EIMS_EX(0)n IRQ_H/VICE'v    stgbe_a<net/));
	 find_firsesc-s DMA
#includX4),we_irqgon
  then = 32 eic    '->le(i =goDEVI.h>
#inclue
 dde <lsegs -es;
 find_firs			flush Txinclude ( == eoptic ir= tx_buffer_info->skb;

			i)ame[] = "ixgpauseigned int hlen = skbr,
   fc_frame_rx_queue;

		mss = adapteral_rx_bytesfp_event(struct ixgbe_adapter *ad_WRITE_REG(&adaixgbe_adapD_82599_COtso) {
			dca_remove_requester(dev);
			adapter->flags &		IXGBE_WRITE_REG(&adapter,first_bisk* CON *skbv);
			adapter->flags &skb,tx_  thi, u8 *hdrr->n_count(union ixgbe_a spec;

			icific *_ring[r_idx]xgbe_tx_timeout(F);
/*
 * i);
		IXGBE_WRITE /* CONFI;
			n;

	if (stheck_favlanbe_ripr->nresul,nfo);_tucmd_mlhr_info[imss_l4letotax,      ->next_tokb_is_gctorkbtats.pac
	}

	rheaderta -ned find_firstq_rearm_

	rexpandvecto finend u0_dca(sATOMI   whiflags2 & IXGBSIZE / 2)) ||
a)
{
     ter-cp_hdrlng,
kb>num_nt; i++)l = bidx + 1)t_bit(q_->protocolharehtonsstersP_IPdapter->gso(skb)phdr *ippu =iidx])	ring->to	iph->totbytesing = &_queue, mas         r_idx])	ring        r_~ {
	_tcpudp_magic(queuesaddhe ne>txr_idx,
			                      adapterqueuednly * 0vector->txr_idx,
		                       a_WRIPPROTO    v_idx));
	napi_schedule(&q_vector->napi);

	repter->h_FCOE */

	rtso_ctxt++ing  *rx_ri_bit(q_vshdapt
	/* digso	skb_shinSKB_GSRQ_HAV644-k2"
cpv6
	}

	/* dipayunsi		           1);
	}

	/* disable i/
	ixgbeterrupount his ve&ount of work dronly */
	ixgbe_irq_disable_queu optimized for cvector/
	ixgbe_irq_disable_queu0,eturn IRQ_HANka one shot) rx clean r6outine
 * @ning->ve
 * @rx_riruct ixgbe_ctor-d_next_bit(q_vgbe_ ixgbe_receive_skb - Set_bit(q_ring[r_idx])um_tx_qupu()TXTDESC_ADV(>txr_idx, dx =w = &aVLAN		le.datIP.datress Netwctor->tx
	ixgbe_iTX     S__vec*q_vetxr_idx, adapter|ets
 *
 *ixgbe_ring *rx_ring = NULL;
	in_Mhw *ct ix_done = 0;
	long  (}

	r
			/* ct iED) find_f<<r->txr_idx,
			        ******ADVTXD_or->ad_SHIFr to atal_bytes = ues);
	rx_ring = &(adap_idx, adapter->num_rx_E_WRITE}

	rst_si,
	 vector-(adapTRL_es);
	rx_ridif

	ixgbedapter->flags & adapter, rx_ring);
#endif

	ixgbe_clean_rx_irq(q_vector, rx_ring_ring[r_idx]->txr_idx, adapter->cpu ixgle32(, adapter->num_ */
	if (work_done seqbe_tsrx_ring =w = &aADV DTYP TUCMD MKRLOC/ISCSIHEDer;
	stru_tx_queues,
		 x_rirx_ring D_CMD_DEXT BE_WRITE[r_idx]);
#ifdef CONFIG_IXGB(q_v	struix_vector al_packets = 0;
		r_idx = find_neRQ_HADOWN, &adapter-         IG_IXGBctor)_IPV4hw.maork_done;
}

/**
 * ixgbe_clean_rxtx_L4T    */
	if (work_done _DOWN, &adapter->s_complete(nasix (aka one shapter = qMSS L4>adapDXbe_conf            g(&adapte struct with our devick;
	}<<ef CONFIG_IXGBESS	if (adapte this pass, inx_qurx_rinlean more than os all	if (adapte/* = IX;
MODU1c inliSO to do this pass, inx_qu);
		* ixgbe_cleanIDX	if (adapteif (work_done  this pass, in in it
 * @bud this pass, iapter d_next_bit(q_v->_REG(stac.typ
		 */
hw.mae_q_vector, napruct ixgl ixgiver
 ng->e
 * @et_drhareGBE_DCA */
/**ers;
	_ring - a      struct ixgbe_rin	struc and/orce ID,
 *   Classe detaiixgbe_adaprranty of M>tx_sum) {
			dca_remove_requester(dev);
			adapter->flags &= ~IXirst_bit(q_vector->txr_idx,
	for (i = 0; i < q_vector->txr_co->num_tx_queu q_vector->tx {
		ring = &(adapter->tx_ring[r_idx]);
		ring->total_bytes = 0;
		ringckets = 0;
		r_idx = find_next_bit(q_vector->txr_idx, adapter->num_tx_queues,
		 , mask);>total_paip_summnshareCHECKled,PARTIALatic x;

	r_idx = find_first_bit(q_vectotats.pacxgbe_q_vector *q_vector =	                       container_of(napi, struct ixgbe_q_vector, napi);
	struct ixgbe_adapter *adapter ct ixgbe_ring *rx_ring = NULL;
	int work_done = 0;
	long r_idx;

	r_idx = find_first_bit(q_vector->rxr_idx, adapter->num_rx_quues);
	rx_ring = &(adaper->rx_ring[r_idx]);
#ifdef CONFIG_IXGBE_DCA
	if (adaptenext_bit(q_vector->txr_idx, adapter->nt work_done = 0;
	long r, rx_ring);
#endif

	ixgbe_c/
	ixgbe_irq_disable_queean_rx_irq(q_vector, rx_ri/
	if (work_done < budget) {
		napi_complete(napi);
		if (adapter->rx_itr_setting & 1)
			ixgbe_set_isix (aka one shot) tate))
			ixgbe_irq_enable_queues(adapter,

			                        ((u64)1 <<_idx]);
#ifdef CONFIG_IXGBE_DCon, MA wipter4)1 << q_vectong = &(_ID_8_complebe16x = find_n@data work_done;
}

/**
 * ixgbe_clean_rxtx_many - mbe_ring;
	}

	/* dickets = 0;
	turn IRQ_HA_mac_82six (aka one shot)mac_82}

	r_idx = find_firsine
 * @napi: napg TFCSnapi mplete(napi);
		if (adapter->rx_iSCT_setting & 1)
			ixgbe_set_itr_msix(q_vector);
		if (!test_bit(_que]);
	/* disable * If all Rx work done, exV6it the /* XXX wx_rinb in
omap  V6 ector-s??st_bit(pi_compnt of work drincl->rxapter->rx_itr_setting & 1)
			ixgbe_set_itr_msix(q_vector);
		if (!test_bit(__IXGBE_DOWN, &adaptoutine
 * @napi: napi struct wit_queues(adapter,
			                        ((u64)1 << q_vector->v_idx));
		return 0;
XGBE_DEt the _bitunlikelyrror:
		b0x3))(IXGBE_EIM received and modWARNev;
_itr_ms"pargram , mas{
	cautc =oto=%x!ng->nextctorrx_ring[r_idx      ter,	/* disable interrupti struct with our devices info in it
 * @budget: amount of woran_rxtx_many(stzeroag_lisxr =
	     offunsigkely(cctor =
	                     adaptere_q_vector, napi);
	struct ixgbe_adapter *adapter = q_vector->adapter;
	struc_FCOE */

	rerruptx_ @di ixgbe_ ixgbe_ring *ring = NULL;
	int work_done = 0, i;
	long r_idx;
	bool tx_clean_complete =  true;

	r_idx = find_firsnt && !q_ve NULp) {
			dca_remove_requester(dev);
			adapter->flags &= ~>txr_count; i++) {
		ring = &(adapter->tx_ring[r_idx;
#ifdef CONFIG_IXGBE_DCA
		if (a= &(adapter->tx_ring[r_idxe_tx_timeout(first_count(union ixgber_idx = find_next_bit(q_vecto_tx_timeout(dx + pter->state))
 -
		}

	l_pa			ixgbe_irq_enable, IXGBE->num++;
		ND_UP(ddp,	ringter->state))
nr_desgresustruct with our dc inlinek_done;
}

statincy;->tonly _t *macpu(rixgbe_q_vector *q_vector =
	_bit(q_v->to_rin_unmap_and_free_tx_re = 0;DMA_TOget_ICE*adapter = netdeunmap_and_free_tx_resTXvectsionent = *(unsigned long S, mask	ctrl =struct with our didx)
{
6) >> 3);xgbe_ring *rx_ring = NULL;    hw = &aex@q_vs & I the {
		8EB_led "COPYING"_queue-

	return 0;
}

            xgbe__ring[ */
(q_vecto;
		ring,e_queuxgbeF;

#if++) ruct i                     container_of(napi, struct k;
	}

 */
len, (u	if apter,
    ATAe >>
			ct ixgbe_q_vector, nap->num_}

	retstribute budget tomap_vtatic inline void map_veecto +ector->rivate structure to i);
	struct ixgbe_adapter *adapter = q_vector->adapter;
	strucytesdx)
{
	d ixgbe_ccific vector, IXGBE& IX vector->hw.e
 *FFFFFFFF_vector- (adaptee_ring *ring = NULL;
	int worrk_done = rx_queu*****ved 0; fh */xq(struc fsult */
		q_vecean_nlin->dma = 0nlinablinnlingbe_struct with our dnline[fbit(qctor = a->XGBE_TX_DE	if nlin         set_bit(otted th_bit(q_vt_idx, q_vector-lly, we'd have
 * one vector per ring/queue, RQ_HANDn;

	if (statu     container_of(napi, struct gbe_map_rings_to_vectors - Maps descriptor rings too vectors
 * @adapter: board privaate structure to initiamap[f]d vector counvector->txr_idx, adapter->num_tx_queues);er *adapter = q_vector->adapter;
	struc-specific vectorint t_idx)
{
	able tted through the MMSI-X enabt ixgbX_WAKqueuesEICRtx_riuct ixgbe_a    container_of(napi, str.skbtatic 
 * ixgbe_receive_skb - Se[msix(].ector->adapter;
	stru and/orl_rx_paixgbe_adapter *adapte Corpora) {
			dca_remove_requester(dev);
			adapter->flags &= ~IXG>txr_count; i++) {
		ring = &(adapter->tx_ring[r_idx])nable_tor->txr_>len->hw.BE_DCAiver_to_u8 t; i++) {
		or ring (for a specific;
					rx_desc->re, ring);
#endif
		tx_clean_complete &= ixgbe_coldaptalizauer->numues,six (	         bytes = 0;
		ringq_vectd_cmer->ate))
			ixgbeEOP_ring->to			ixgbeRSes per vector.
	 IFC_vect * mapping, w*
 * ixgbe_clean     escrv takes care of the remainder. */
ting *qp_ring->to+) {
		rqpv _irq_to_txq(struct ixgbe_adapter *a, i
	int wostart; i < vectors; i++) {
		rqpv V;
		o_txq(struct ixgbe_adapter *a, iTSOector-start; i < vectors; i++) {
		rqpv T alw ixgors for a 1-t         			iPOPTS_TXSMer->rx_ring[r_idx]);
#iOUND_UP(rxr_rtxr_reif (aset_itr_tx_many(stru;

			ifE_RXDsq_reai++) {
		tqpv = DIVet)
{
	struct ixgbe_q_vector *q_vtxq(struct ixgbe_adapter *a, imanyal co+) {
		tqpv = DIV_ROUND_UP(txr_reIaining, vvectors - i);
		for (j = 0; j < tqpv; j++) {
	of time_staxgbe_ring *rx_ring = NULL;bledal c+) {
		tqpv = DIV_ROUND_UP(txr_remaining, vectors - i);
		for (j = 0; j < tqpv; j++) {
	txq(struct ixgbe_adapter *a, int v_or->a+) {
		tqpv = DIV_ROUNDIG_IXGBCCt ixgx);
			txr_idx++;
			txr_remaining--;
		}
	}

out:
	return err;
}

/**
 * ixgbeFSOcurren		}
	}
	for (i = v_start; i < vectors;dif

return_t (*handler(r_idx+ -		map_vecctor.
 **/
staticPAYnt ixgbe_cle                            to_vecto->hw.--ector->txr_idx);
	q_vector->txr_count++;
}

/**
 * ixgtxr_idx);
rx_ring =ct ixgbe_adapter *adaptvectors);e_adad.n;

	ifnly      contain64ix_istructure to iniSET_HANDLER(_v) ((!Decrement for(&adapter->p_complete(naDecrement for r_rectors
 * @adapter: SET_HANDLER(_v) ((!tors for a 1-to-_complete(nareturn_t (*ha
		} e ixgbe_ring *ring = NULL;
	int work_done =ion...
                (!(_v)->| in it
 * @budghere aITR(v_isters r_idx{PCI_VDwriter-* akaC_ADV(ompletedl < adaph/wstersEL, IXGB to theDP1)next_buffer;			fetch.  (Oninter tapplic <lin    weak-_VDEV
		ePCI_VDTHOUl archr *adector |= IIA-64ecto
	lonwmb(              ng r_idx;
	bool t	r->nal(ice *netdev)hw.hwrxr_co+ing = NULLtai_bitixgbe_adapter *adapteatUAL_PORT),
	 board_82598 },
	{P, adapter->num_tx_queue>txr_idx,
			   stru2599_C_DCA
		if (adaptet_inight nowixgbew = &adaIPv4);
	}

				
  FITNESS FOtif (XCTRector[vecm the interrr->rxrthoto out;
	}r->rxr_idx, adapter->num_r adapteeth->rxre: boa IXGBE_Rfor MSIX)adapt interru16>txr_iid, src_,
	 , dskb)-ree_flex_bpterhem so gotoipv4rxr_ce_queuapter->na%d\n8 l4skb_shbit(q_/*r =
	 rectio're UDP It TCP2598EB)
	queue
		if (adapter->rx_itr_sruct i boa1);
	}

	/*  ixgboto fre    or ostrucET_Hqueue_irapter->des coun netdev adapter->TR* @nYPEapi: nap/*, netdev     skb_siRBUFLn ind0;
			PORT),theoretical maxi/* U           L4aka one, j8 },bail rx", txr_iice *netdevarked as
&ector[vecend up
 *atic int ixgbe_nector[vecixgbe_err);
	terrr_idx = find_first_bit(q_vector->rxr >>.vector,
		  d_first_bit(q_vector j++) {sc, 0apter->nax, ador only *a, vector], "%s(adaptervecto_con
		}
	}

 =			 ->h_     uffer_info}
	deet_err);
	icr & I; i--)
		freerr);
	esc->wb.stut:
	retc, 0, ad;
}

static void ixqueue_ir_itr(struct ixgbe_queue_ir;
}

static void ixc, 0, ad_itr(struct ixgbe_);
	adapt;
}

static void ix);
	adapte_itr(struct ixgbe_ netde;
}

static void ix netdehw rsc src= trud;
	} elinflag	skb->		sch, IXGBE->data_r sac_8259m     <%lx>\t ixgbe_adapapteradapter)
{
	struct ixgapter->na_vector *q_vector = adtr,
	                  _disable_msixITR(v_iddapteas rx_on == Rboro, Otion =         r_FCOhard
  ANY oard_CPU     <%lx>\ausesadd_x_rie rec_with t	        t_rxd = IXGB; i--)
		frence DCtor->txr_count &xtp = (smayc:
	to (dcnfo;
}

static inline u32 i);
			adapter->flags &= ~IXGBEnd_first_bit(q_vector->txr_idx, ;
		}
		info->skb = NULL;

		if (rx_buffer_info->dma) {
			pci_unmap(struc q_vesub (; rxdapter,
0, i;
	lo"GPL");
MODhw rsc Herberr->noriginapecipterhare l>tx_smp_mb_(&adap);
	r->rx_it (; rxix_lr->v_id82599      o adapteexist y(8 *queueuct sc	}
		tsix_ve_itr a, "tx"ixgbed\n", errerr = agai+;
	 aV_ID_8an- msixg->t
	ifqueuB:
		ict sroom taken ovesix_veter = uct npu = getSC_UNUSEDtor->rxr) <up
 * rx_desc *rx_desc, *neG(hw,reprieve! -ap_veb.uppe              _82599EPCI_vlgror = dataes the -r->rxupper, q_vector->tx_itr);

	switch (current_i++ct ixgbe_adantial smoosc)
{
	structean_tx_irq(q_vector,       q_vector->rx_itr,
	                                    rx_ring->al_packets,
	                            000;
		break;
	default:
		break;
	}

	>ough tw_itr != q_v32 vaand/or               q_vectoor->tx_itr);

	 up
 * @sixgbe_adapn", }
}

stact ir (; rxr_idx <static inlinector 
#ifdef CONFIG_IXG_count(union ixgbe_adv_rx_desc *rx_desc)
{
	returrm_rsc_queue char ixgbe_driver_version[] = DRV_VERSION;
stx_clean_co_itrproVIDEadapt, "tx", Inc.,
  51 Franklin St - Fifth Floor, Bostx_clean_co4)1 <<, adatcifind_first_bit(q_vectorPRIO
	adapter 13len);
		}

 rx_rx_hashFAILor *qadapter *adapsc)
{
	tx_on ixgbexmupt etde IXGBE_Rtries[vector].v* @adap
                               int *work_done, int work_to_do)
{
	struct ixgbe_adap		IXGBE_WRITE_REG(&adapter->hxgbe_set_itr_msix(ixgbe_irq_enable_gbe_ringe'll ha			map_veesult */
		daptf);
		itsvect; v_startwe'll have to group f) >> 3);
			} elsvlgrpvoid
		masx_tag;
				}
 find_first);
}

/**|=work interfacgABLED)
		ix, Inc.,
  51 Franklin St - Fifth Floor, Boston, MAxgbe_ring 			ivar =EIMS_GPI_SDP2;
	}
	if (/* No ma*/
static4)1 <<"GPL")ion 2, a<< 1	swit ixgb_priv(net<<	if (err)
	_MSIX_ENABLED;
	pcie
 **/
staticct ixgbe_adapter *ang of time_stamp and movement of eop */
	a Floor, Boston, M>total_packiorer->!= TC;
	}
	CONTROing = &(_priv(netdev);
	struct ixgbe_hw *hw = &adar->hw;
	struct ixgbe_q_vector *q_vector = aadapter->q_vector[0];
	u32 eicr;

	             ;
	struct ixgbe_hw t_itr_ixgbe_update_rx_dca(struct ixgbe_adapter -st <erx_queue Interr;
	struct ixgbe_hw *ring,
  

	r_idx = find_firste Inticense allong with
  this program; if not, write to b->len - al_packets = 0;
		r_idx = finnt v_ructure
 **/
statice_adapter *a, int vckets: the number of pCR);
	if  == ixgbe_mac_82599errupt *&=ng->count - O_EN |
		            IXGBE_DCA_RX-er->hw;e Inteet_stats.rx_pO_EN |
		            IXGBnfo,
	[bd interrupt alert!
		 * make sure intic void	     fE_FC];

g
		sn      strue ind		stb;

			if eral Publi = 8000;

	r_idx = findum_tx_queu_idx, adapter->num_rx_queues);
	ringm_tx_queues,
		                      r_i0;
		adapter->rx_ring[0].total_packe
		ixge MSI-X enabliter->net_			iUSE_COUNT>q_vector[v_idx];e_configurained vectid map_vector_to_rxq(struc, we
e MSI-X interrupts here but EIYou would add new
 * .p
 * @staet_drvdatavoid ixgbe_irq_enable(struct ix;
	intE_DESCRIPTION("Intbusye
 * @
staticNETet_cTX_esc, *RSION(six(x/Rx rings to the vectors ixgbe_adapter *adapter)
{
	struct net_ctrl |= IXGBE_DCA_RXring[iwareer i = 0;
             ins    _bytes)sosing */
				bi->por *q_v_remaining&t; i++) apter
 *X_RX<ICR_FLOWies;
->hw,_ean_anyr->num_rx_tors - NON_Q_VECTOnetde;

	/*
	 s: boaq_enable(adapter);
		return ISOuted
 *      based on theoretical maxi(u64)1 << q_vector->v_idx));
	}

	return wenable(adapter);
		return many - ms_RX_QUEUES)ectobitmap_zero(q_vector->txr_idx, MAX_TX_QUEUES);
		q_vector->rxr_count = 0;
		q_vector->txr_count = 0;
	}
}

/**
 * ixxgbe_request_irq - initialize interrupts
 T @ad_DOWN, &adapq_vector->txrbitmap_zero(q_vector->txr_idx,  have 		
		adapter->tx_ring[0].total_bytes = ing the best available
 * capabibled

	for ND_UP(ddector, tx_rinbitmap_zero(q_vector->txr_idx, MAmsix(q{
		str;
	inteue_irqsude restaing *tx_rnapi= re
		rgoto f_vector->rxr_i}
	default:
		bng = &(++ &ixgbe_intr,l_rx_packGBE_FL              netdd pr &ixgbe_intr, IRQF_SHARED,er);
queues(adapteKE_THRESHOLD))) {FLOW_DIR);
			/*ocol == htons(ETH_P_FCOE)) &&
				         conta-initializatio int bu, vector);adapter->mr->txr_;

	switch (curreBLE)
		masiled, Erqs(adap      netdev);
	}

	if (e        interruct net_for (; rxbitmap_zero(q_vect_remainingstart++adapter,lo_dword.hs_to_cleanTX_QUEUES);
ct ixgbe_adapter *adapter)
{
	int i,fault voiEings tetical maxiount = 0;
		q_vector->txr_e per queue.
	 */
	if (vectors i);
	struct ine = 0, i;
	long r_idx;
	booTE_FLUSHif

	if (!i0;
	}
}

/**
 BE_DEV_ID_82599_I_DMt);

#ifGet Sg_listN			/* sS include  *skb)
{
	unsigned int frag_list_size = 0;

	while (skb->prev)ses */dr#incIXGBE_Rrd.hs_rsstatic void ixgbe_sE datlist **/
staticEG(&actupoin ed_coudcksum.h>
#lean_rvlgrS_RTPCI_VDEVICE(INk = (IXGBE_EIMS_E>irq, n*er->pdev->irq, ET_IP_ALIGN);
		rx_buffer_info->skb = NULL;

		if (rx_buffer_info->dma) {
			pci_unmap/*);
	}
lean_comx_ring)
{
	598EB) PE_Uand/orate_itr(adruct ixgb_EXT,
	               macion ixgbe_adv_E"rx"net Ard private strNICreturn rx_desc->wb.lower.lo_dword.hs_rss.hdr_info;
}p_itr,
		      S_TXO priv= 0;

	while (skb->prev) {
		struct sk_buff *prp, tag);
		else
			netif_rx(skb);
	}E_REnfo;
}

static inline u32 ixTEL, *p

	IXGBE_WRITE_REG(&adapter->hw, IXGBE_EIMS, mask);
	ixgbe_irq_enable_qu adapter\n",
			        adaptersockdapte*dapter-      *adais_valid_e"rx"rxr_c(SIX_ cle_V(*txll packet
 * ADDRNOTAVAI(pdevmemcp napifree_tx_r->name void ixgbe_cG(hwapter  v_idQUEUES) ixgbe_ue & 0x3pter)
{
	struct ixgbe_hw *hw = &adapter-v = data;
	struct_rar(&adap,		if (adaector->vm_tx_queAH_AV len);
		}

		if (_tx_irq(q_
entriesdio    esc->wb.ustatic inline u32 ixgbe_prtad      dev_to_n",      lse {
		synchronize_irq(adapter->pdev->irq);
	}
}

/**
 * ixgbe_configure_msi_and_legacy - Initn", ee u1acy modeto nexf (tor_t rea);
					gbe_.tor_tll packet
 * @skb: p	r(str->len;
				totadct pGBE_WR>name[dapter&*
 * E_RXDADV_E     r(str*
 * ixg and/or ector-*/
		q_vector->eitbe_sr->navar(adapter, 1, 0, 0);

	map_vector_to_rxq(adapte * @adap, 0, 0);er, 0,figurelse {
		synchronize_irq(adapter->pdev->irq);
	}
}

/**
 * ixgbe_configure_msi_and_legacy - Inigure_tx - Configure 8259x Transmit Unit after Reset
 * and/orCOE */
				/r->naate structure
 *
 * Cofigure t
 **/
static void ixioctlvar(adapter, 1, 0, 0);

	map adapterfreq *reqng; v_smdinfo->skb = NULL;

		if (rx_buffer_info->dma) {
			pci_unmapuct ixggbe_cmii= ring-ate_itr(adap. 8259x T, if & D(req)
	 * NE,
				           _vectane_ri},
	{PC-EIMC);
		Sector-oard priv
  ANY corresped iEVICE(adapter *adapter_disable - Mask off interrupt generation on the NIC
 * @adapternon- = NUurn rx_desc->wb.lower.lo_dword.hs 32));
		IXGBE_WRsk = (IXGBE_EIMS_ENABLEamp         tor[i])EUE);
	if (adapter->flags & IXGBE_FLAG_FAN_FAIL_CAiteback RO bite_ri_irq_r_REGXT_DRV_LOAD);
hw->98_BX),
	upts
 *
 **/
staticw->masan      {
	strrtnlto thIXGBE_x Head*adaptergbe_aIR_HA ixgbe_mac_82,} else {HW_nd_l_T_SA adapt			txunctrl = IX *   Class		mask = (qmask & 0xFdel));
		IXGBE_WRITERemovackets, IXGBE_TDLEN(j), tdlen);
		IXGBE_WRITE_REG(hw, IXGBE_TDH(j), 0);
		IXGBE_WRITE_REG(hw, IXGBE_TDT(j), 0);
		adapter->tx_ring[i].head = IXGBE_TDH(j);
		adapter->tx(hw, IXGBE_DCA_TXXGBE_TDT(j);
		/*
		 * Disable Tx Head Writeback RO bit, since this hoses
		 * bookkeeping if things aren't delivered in order.
		 */
		switch (hw->mac.type) {
		case ixgbe_mac_82598EB:
			txctrl = IXGBE_READ_REG(hw,nt tBE_DCA_TXCTRL(j));
			break;
		case ixgbe_mac_82599EB:
		default:
			txctrl = Ie Free SoftwarNET_POLLCR.
	 */LER
/frag_P_82599 'struct ix'n expon"
		api_sch
		bREG(queuong->nextndq_ve_disakb;
 in
hav License,-WRITE_REi < q_vec.s, map;
			skb->preilear t firmware knoDED * 2)
== 8xec * 2gout x_ring->rx_buffer_innetd_82,
                               int *work_done, int work_to_do)
{
	struct ixgbe_adapter  entry */
	{0,, u32 eicr)
{
	struct iNETE_RTC);
			ivar &= ~skb) {
		skb_dma_unmap(&adapter->pdevtatic>dev, tx_buffer_info->skb,
		              DMA_TO_DEVICE);
	****************/
}

/**
 * ixgWildcard e 4;
				}
			}
		}
		txoff <<= tc;
	}
#endif
	return_bit(q_varen'te) {if we'ULL;(0,				tc +=>num_rx_qture
 *
 * >page =evice *netnd_free_irqbe_hw *hfo->skbfp_event(struct ixgbe_adapter *adITE_REG(h "Intel( **/
statqueuWRIT == ixgbe_mac_821 <<;
		else
*ada1 <<=or->.ndoruct s		leaned_count,ature[R q_v_RSS].maskr_infindex = inuppe& IX_RSS].mask& IXGBE_FLindex = i mask;

	maRSS].maskCTL(index));index = ev->irq, _RSS].maskev->irq, XGBE_SRRCTt_buffer-e_irq(ad	IXGBE_READ_buffer-BE_SRRCTL_BS_q_vector_list

	srrctl &(IXGBE_RX_HDR_SIZts
 *counture	->msiXGBE_SRRCTL_BS_HDR_SIZE << acTL_BSessBSIZEHDRSIZE_macindex = dr_info(unx & mask;
	r_info(unindex = nter;
		ma_RSS].mask		srrctl |&
		  IXGxr_igbe_a
  verRSS].maskE_SRRCTL_BSIZEPK >> IXGBE_SRRCTL_vecvidT_SHIFT;
#else
		) >> IX >> IXGBE_SRRCTLkill> IXGBE_SRRCTL_BSIZEPtl |= IXindex = do= ring_RSS].mask ring,_WRITE_REG(hw, IXGBE_RTTDCS, rttdcsture[Rd_82o[i];

	lPKT_SHIFT;
se
			I,pter, ei2598_SR_DUAL_PORT_Eure[R     ddpnion i;
			txctrlADV_ONEx = ((SCTYPE_ADV_ONED;
}	}

	IXGBE_WRITE_R		fradapter->hw,TXCTRL(q)
	IXGBE_WRI8AF_DU&adapter->hw, tion ==e_setup_mrqc(r *adap
		srrctprivate data (not };E_DEV_ID_82599_IXGBU Gene inli, tx_ringnline RD * 2)->flagx_desk_tx_hang(adapter, tx_r
			struc @ent:H)
				 akape == ci_tb& IXGBE_x csum disabled */
	if (!(adapter->flags & Iac.type == ixgbe	++adapter->G_MSI = &tx* multi,
  by ainfo->pag inline void ixgbeOSnsigned long ev

		
	/* Lill beuses */
			me Foundation.

  Tct ixgbe_a_idx = find_ voidccu= (reg_idx - 64 hung */
			DPRINTK( ixgb IXGBE_RXDADV_NEXTP_MA
}

static void ixgbe_free_irq(str	unsigned lon->tx_tiv_idx))*	}
#
			               PAGE_SIZE Writeback RO bit, since this hoses

		goto out;
	}

	/*_CTRL_;ter-nsigned lon adapterrq_ri);
	_len: rx b_DCB[ent->if (i gbe_cbit(* CONFIG_IXcards_f    FFFFFFFFted a, RSC the d_dswit2598_SR_DUAL_PORT_En", for thector_ttic void, txr_rtic i, e, *tge,
			                   rx_buffer_info->pageE_SIZE / 2)) ||I inter}

		/* idx)
{sk/
		if ectoBITr->rx(64) have disa32 rscctrkb(struct rl;

	rx_ring = &adapter->rx_ringgbe_alloc_ int inde_idx]); *feature =_rearm_quecctrl;

	rx_ring = &adapter->rx_32_bit(...) ,
       _RSCCTL_RSCEN;
ing->reg_idx;
	rscctrl = IXGBE_READ_REGmber of  descriptors soer = netdev_priv(netdeNc =  <lin->rx      tor
 **/
pt */
	if (co,txonr  IXx_ring->ter_info[eodmterr_vectors;
_RSCCTL(j));
	rster->q_v_rearm_que /* IXGBCTL(inc_eoegd fo/
		if _RSCENmask;barCCTL_MAotocol == htons(ETH_P_FCOE)) &&
				 IORESOURCE_MEM),txctrl);d ixgbxt_bit(qdescriptors sr = netdev_priv(net
		mask = (I > 8)
		rscctrl |= IXGBE_RSC
		 * 820x%xGBE_WRITE_REGng event, > 8)
hare->ne_queues(strpciee_itr(_ree_irket (rx_buffer_in(page_count(rx_bufhinfo(t_buffer->skb = sthe ->pr	masX4),
 **/
*adamqtx ring is paused
 * al_rx_by,r->hw.X_QUEUEbe_coes,
		  rx_rXGBE_RSCCTLBE_CTRL_E/* Tx Descriptors_RSCCTL_
		elseSETe ==et_cDEVq_enable(ev_priv(ne2)
			rscctrlA_FROMDEVICEex = rx_ring_buffer_info->dma) {
			pci_unmapdapter *adapter_desc)
{
gbe_ring  *rout_cou			ixgbL_EXT_DRV_LOAD);
}

ue &S_RTXt in DCB modhe
	 * checgu32 ixgbe_et)
{
gbe_adapdapte_LEVEL	if (adtype     EITor], "%s= iore_rin       struc IXGBE/
		if 0six_ev->dev, cpu) <<
			  tdev = adapte;
		dev;
	innotifier_	struct net}

/**
 * ixgbdata;SCCTL_MAXDvice *nVERSION(DRV_VERS1*****= 5           re_tx+ ETH_HLEN + ETH_FCS_LiBE_EICRurrently being APABLE)ixgbe_gned long) fffer_in214AD3D, 0BE_READ_RIZE_IXGBE_Dlongrn (le32_toF7C, 0xEt_itr) {
	/*     5	if ( if thgbe_adapter _msixu + E_msiR_8192er a reset.
 *bdic iy, H=(struct ixgbe_al_bytesup hw apipter->ixgbe_&PABLE) &&
	, iif (adlong_irq(adapPABLE) &&
	N;
	i           st_dev>flagsr to usv);
	unt mode or not */devicedapter->fl;
	stru|= IXGBE_FLAG_RX*/
	if (adN;
	iee(struct ixgbe_adapter *adapteEk;
	carivafh accordis		j id (    8 { 0),ap_veXGBE_DEV- msiwirep(&_bytesbaEG(&= 8000;!(	rx_&dapter-8)_GPI_SDP1*/
	if (ad privxB855AABE,priva;
	strubielifn_t iadaptr to usPHYding to the mode */
				er->flND_U|= IXGBE_FLAG_RX*/
				BLED;

	/,
						skb_shnfo(skb)->gso_sieturn_tif (hw adapte multiplND_URTYPE_Uer->hwc irx - Cogbe_mmd  IXGper       gure 8259x TransmiAD_RDIO_PRTAD_NONalways u 8259x Tr0), pf (MAXBE_FLAG2_RSC_EDCA_w = &adadapter-ytecounS_C45 |apter-EMULATE_C22 &&
		    (netdestatic void ixgBE_FLAG2_RSC_Ebe_set_i|
			    GN(max_frrx_buf_len = ALIGN(mar->naame, 1024);
	}r->na1;
		ixgbe_wareN_SHlean_re "ix_825ned lonmpleted
 * packI_DMinNTELan++)  IXGdesc m->namcksum.apter)
{
tdev->nithout after this
		 * sees thare
 * specifint < tx_. == ixgbeB855AABE,ter->hw, TE_REG(&adapter->hw, I}

	i=ly(sx_ring->tot) in DCB mo
	DIR);WORK hlen = skb_headlenlse
		rsuct fc_fram rsc bu200 */
		breakreak;-1, 1E(INaskl(8 *	skb->pksum.GPI SDPo_txq(adapd inev->mtu <= ETH_DATA_case low_latency:
		nlse
		rse_write_eitr(q_vector);
ffer *SDP1) {
		/* CleCI_Vivxgbe_JUMBOEN;
#ifdef IX2BE_FCOE
	if (netdev->features & Ne_task);
	} else {
		/t max_frame =P1);
		schedule_work(&adapte the Ti->F; /* discard ED_LINK_	bitmap_zeoardFoundation.

  Th
	struct sk_buff ROBE, Ial_rx_bytes = 0, total_rx_packeted(netR registers,GBE_F_HDR_Sa fbe_hw= IXGBic inligbe_iAG_D96)
			rsle_cld && sk IXGBE_etdev->ne along with
  this program; if nif (rx_ring->cpets += }
}
sd.type == ixgbe_mac_82599EB)
	ESDPr of descr ada********
		r_ IXG 0xA5tx_stop_all_queCRITBLE)
	"Fanil De q_vpdisareplxgbeses */
			mg =
				 tatus:,
   hw wit82590);
		permrxr_coaand_llBE_RXCTRL);PABLE) &&
	 j), (rdbata chuUPDATE;
	adapter->link_check_bytecouhave disa           struct ixgbe_ring *rx_ring)
um_rx_quSksum.a->hw, Ik_limit)) {dapterFall Tffer_infohw, IX theause&ixger sg & VLorwrite x));ICE(m Youer->hwqueue_tfo *(INTEL, q_vectr->n_REG(hw, IXGBhead =Silicurces after trarier_ok(netdev) &&
	             (IXGBE_D queue after this
		 * sees the ne	o_clean.
		 */
		smp_mb();
		if (__netXGBE_EICS, maapi: napi TE;
	adapter->link_check_timeout = jiffies;
	if (!test_bit(__IXGBE_DOWN, &adapter->state)) {
		IXGBE_WRITE_REG(hw, IXGBE_EIMC, IXGBE_EIMC_LSC);
		schedule_work(&adapter->watchdog_task);
	}
}

static irqreturn_t ixgbe_msix_lsc(rxctrl & ~IXGBE_RX
#ifdef IXGBE_F jiffies;
	if (!test_bit(__IXGBE_DOWHW99EB)
		 * 8		IXGBE_WRITE_REGABLED;
				if (rx_bu		rscctrl->BE_DEV_ID=} elIF_F_SG_enaev->dev, cpu) <<
		 /* IXGBIPt_irqFCOE */
		ixgbe_configure_srrctHWvectorTXer, rx_ring);
	}

	if (hw->mac.type == Rxgbe_mac_82598EB) {
		/*
		 * For VMDq sFILTER			rscctrl}
		}

#end|if /* IXGB}

/t_irq(ad sizes through the use of muXGBE_FL
		 * registers, RDRXCTL.MVMEN6 must be set to 1
		 *
		 * alsGRO) >> 3);
			} els/
		swapter->flags & IXGBE_FLAG_FCOE sizes through the use of mu_quet_irq(amust be seE_SRRegisters, RDRXCTL.MVMEN must be seng this bit are only that SRthe manual dng this bit are only thatl(adapt5]
		 */
		rdrxctl = IXGBE_READ_REG(hwiple SRRCTL
		 * rng this bit are only thatSG) >> 3);
			} else if (dcb_i == 4) {mask |= IXGBE_E IXGBE_IVAR(queue >> 1));
			ivar &= ~(0xFF;
}

static void ixgbe_updadapter *cbnA, 0xRL, E_FLAG_RSskb->prev)2598_SR_DUAL_PORT_EM),
ong with
  this program; if not, /
	for (i = 0apter\n");
		/* I_DMA          {
	struPABLE) &&
	    (ing_featureoid ixing_feature[ater
	 * ing_featurepter->rx_ a->q_i++,es = 0= IXLOADSCorporTRL_DESC_RRO_EN);
		rxctrl &= ~(IXGCA_RXCTRL_Crx_qug->cpu = cpu;
	}
	put_cpu6C, 0x2A int indes unless this bit is set.  Side
HIGHDM = v_e along with
  thi2*************w.mac.type ==s unless this bit is set.  Side
Larly bsc btes)dapte->hw,c.type == @dir598EB)
		ixgGBE_PSRTYPEGBE_SRRCT =
	    ixgbeeatu
	ifCR_FLOWr = netdev_priv(netdelist(hw->maC=
	     Is98 } Vixgb(unsignexctrl, rxcsum;
	static deviceor - 1; i gbe_adapter *adapter)
ue & 0x3SK(32)));be_hw *hw = &adapter->hw;

	IF7C, 0xERQC_RSS_FIE IXGBE_MRQC_RSS_FIELD_IPV4_UDP
		     rs(struct ixGBE_SRRCTs & IXGBE_MRQC_RSS_FIELD_I*adapter = netdev_priv(netde* dixgbeBE_TDLEN(j)MRQC_RSS_FIELD_IPV4
		      | IXGBE_MRQC_RSS|= IXGBE_FCTRL_PMCF;
t_itr) {
	/* c_info[i].nextt_itr) {
	/* cIXGBE_FCTRL, fctrl)l ixgbe_BLED ||
	    adapter->flags IXGBE_HLREG0);
	if (aapter->netdev->mtu <= ETH_DATA_adapter  *lse
		rsadapter  *adapev->mtu <= ETH_DATA_            &lse
		rsf (adapter->hw.mfer_info->page = NULL;
			else
				get_page(rx_buffer_iL, rxctrl & ~IXGBE_RXCTadapter-
	tx_ring->t}

/*_ID_8pu = get_cpu();
	9_KX4@dat
		if (state->state))
if (stAG_ring->toif (sExgbe_E */
		ixgbe_conf	}

		if (sta ixgbe_mac_825B   whigth XCTRL(ACPIif (cto_us Gidx);
	It_desc;
		}

		ixgbe_rx_chGRC - 1;
		free_irq(ak;
	de low_latency;
		breaRCe_se	ivar =GRC_APMEx_ringnfiguratxgbe_clean_rxcsum);

	if (GBE_DnfiguratioPerfc_825etUNUSEupk = (IXG
	set_bit(r_idx, q_ve
		if (state
#endif
picsche		if k_txbuse immediat
		rE_RXBUFFE _recer->txPABLE) &&
	    (bus rx brs */
	rxctet,
	iguresix /ixgbe/widtc *trq_reaPerfor ACKev_priv(netde(k_txEx
			s:%IXGB) %pME_SIZE) + 1) /*AG_RXbus.e unshareFIG_IXC folow_la5000id ix5.0Gb/s"meout(structer\n"));
	}
}

static void ixgbe_v25n_rx_ad2.5id(str"Uturn_t"nt max_frameSCDBU)));
w, IXGstatic void iw, IXf_len x8id ixW, IXGx8timeout(strucvice *netdct ixgbe_hw *hw = &adapter->hw;4
	/* add VID4to filter table */
	hw->mac.ops.set_vfta(&adapter->hw,oid ixadd VID1to filter tabler = netdev_priv(netdadapter *adapteresc->wb.statadRL_DEIZEHe_adapter  @adn)
{
	same, net adaptesxgbe_ada*/
		if ,
						skb_s!info(skb)->gso_size);
				}
#* Perfo   (IXGBE_RSCDBU_RMAC		IX,|
		vice(a

	tx_ri, PBA No: %06x-%03rl |=) {
		rdrxct           sPV6
						segsr->state))				skb_bit(__IXGBE_D(&adapterdapt8), remove VIDset_ff) {
		info *ter);

	vlan_group_set_device(adapter->v NULL);

	if (!test_bit(__IXGBE_DOWN, &adapter->state))
		ixpter);

	/* remove VID from filter table */
	hw->ts, int by	hw->mac.o<s.set_vfta(&adapter->hw, vi/* PerfowarnIXGBE_RSCDBU_RPCI-DIS | Iialidw, IXGtaken ove= upper_len;
			sk" IXGBstruHIFT;
		su****i
{
	
		roptimal_IXGBE_DOWN, &aperpter,
	{P(unsignedev_priv(netdev);
	u32Fisable(adap->vlgrp = g a x8_IXGBE_DOWN, &a ctrl;
	int s Rx,     lse {atus XGBE_RDBAL(XDESIXGBE(hw->ma le1clud;

	/* n);

		/GBE_PSRTYPE_TCP(&adapx29 of status o;
	strue're
	  @status:_tx_buffeEAD_REG(&kb;
}

stDP1)XGBE_DCA_TXCTE_REG(hw, IXGBE_RDIXGBE_, (rdba  >> 32));
		IXGBE_WRITE(hw->m_VERSION
	} else CE(Ipterpacket>prevfo;
	produinclud	 board_r Poa privREG(&ev->dev_priv(netdev);
	u32dapte_hang(ad* SeRITE_REG(&adaptIXGBE_DOWN, &aSS_ENAB/LOM.  Pffer_ Tx aREG(&
	/*
	may Tx or->es9EB) {
		ctrl |=ssocidaptekb;
}yE_FCEAD_REG(tic voyouL_CFIIXGBE_DOWN, &aexperiencEV_IDXGBE_RXDp_VFE;
#ifdectEG(&adIntel It IXGBE_DOWN, &aidx = find_
				}
o map whnfo;oviIXGBBE_Vkb;
}

i/strip */
		ctrlapter->hwg =
				  32 reta = 0, mrqc = 0"eth%da DCBx Headbuffer_info[i].nn (le32_to_rx_ring->next_to_clea
  verame);
	e
	struIXGBEer, i, rx_98AT),
	 boaidx, XGBE_DCeven BEFORE000;
			if (test;
	struct ixgbe_rx_buffe along with
  this program; if n= DRV_VERSION;
s0;
		adapgbe_copyright[] = "Copyright (c) 1999-2009 Intel IP;
v->mtu <= ETH_DATA_auses without cllse
		rscheck_sfp_event(ada;
}

static void ixgbe_A 0);

dcnt v_8)
		rscfterdapter: boE_EICR_FLOW~IXGBE_RTTDCS_ARBDIS;
		IXGBEDCA.type == ixgare new descdcauffer_info->sktic voidse {
		sanCA_Tcture(q_vec
	{PCate_itr(adaring[i].tail = IX		pci_unmapter);

	vlan_group_setadapt(R) nuseiga    
/**
 * Ckaround fm a DCBstruct ixgbe
 * 
	ctrl_ext = IXGE_RXDCTring->rx_buffer_info[i];

	while (staterr & It_buffer->skb;
				rx_buffer_infol & ~IXGBE_r_len device:
(count && netif_carrier_ok(netdev) &&
	             (IXGBE_D(hw,GBE_F_syncCTRL_PMCF;
	IXGBE_WRITE_c = gld inttr->next->dmi_addr;
	->hw.mamc_addr_ptr = NULL;

	reture_write_eitr(q_vector);
	mc_addr_ptr = NULL;

	return adsk);
	} else {
		/* Inioun_rin j;
	u32 rdl = convice *n:
	= 0;
_add_vid(adapter - Configure 8259x:        ffer_itrl |= IXGBE_RSCCTL_MAXDESC_8;
#elif (MAX_SKB_FRAGS > 4)
		rscctrl |= IXGBEL_MAXDESC_4;
#eled wheESC_16;r_len dmaast/mulgbe_receive_skb(q_vectt:
			txctrl = IXGBE_READ_REGr_8259e_mac_8259L_825al mrqc;

	mask = adapter->flags & (IXGBE_FLAG_RSS_ENiscuous mode.
 **;
		skb->pkb)->frk_tx_packets efine e enablecctrl BE_MTQaapterure
 *
 *_VFE;
aapter->flag
	 boarc boare: q    qc = I_ENAHot-Plug(j), t, It g[r_idx]);		if (i =c.typ Licensbr->t8259OEN;
#tag, PCI_VPCI_VDEVICE(INTEL, g */
ex	DPRINTK(cast mbuffer_info->page_dma,
			               PAGE_SIZE / 2, PCI_DMA_FROMDEVICE);
			rx_buffer_info->page_dma = 0;
			skb_fill_page_descesources after transmit completes
 * @q_ve);
	eing *tx_r rx_buf;
		 ixgbbytestoeed[i]);

		if g->workw{PCIpauses - 1) * htdev->n dev_mc_list, dmi_addr[0]);
	if (mc_ptr->next)
		*mc_addr_ptr = mc_ptr->next->dmi_adBE_FLAG_RSS_ENABL_ptr = mc_ptr->next->dmi_addr;
	else
		*mc_addr_ptr = NULL;

	returf (adapter->hw.mamc_addr_ptr = NULL;

	return addr;
}

/**
 * ixgbe_set_rx_mode - Unicast, Multicast and Promiscuous mode set
 * @netdev: network interface device;

	if (!test_bit(__IXGBE_DOWN, &adapter->state))
		ixgbe_irq_enable(adapter);
}

static void ixgbe_restore_vmc_addr_ptr = NULL;

	returcheck_sfp_event(adape_msime_stdaptdd int (adapter->netdev, adapter->vlgrp));
	}

	/* Program MRQC for thRRAY_LEN;                     
			ivar = IXGBE_RRAY_LEN; vid;

	cast m(adapter->vlgrp) {
		u ddp, not passing toext_rxd = IXGBE_RX_D voicksum(r->hwss Mask,  out redirection table */
		for (i = 0, j = 0; i < 128; i0; i++)
			 page if weup
	{PCI_VDEVICEr->pdev->private data (not NCTRL);.
 **: boarder->vlgrp,           ;
			break;
		casesform_rsc_queue(sktes;
	tas " i;
	clNETREG

		ISTER+)
			x_buffer_info[i].n		pci_unmap_singlt_buffer->skb;
				rx_buffer_info-er_info->dma = 0;
			skb_put(skb, len)e structr->name[vector], "%          cast
 * address list or the network interface flags are updated.  This routine is
 * responsible foointer to the last skb intor], "m a DCd entry point is called/
		ixgbe_rece_len < IXGBE_RXBUFFER_8192)
			rscgbe_receive_skb(q_vectx_ring->tx_buffero < IXGB eop);
	}-		skb->prev =k_txASK) >hw.mawb.sta	mask = adap; !cleanedk_tx_hang(_ENABx++) :8 *addng)
{
	pciorkaround f* retu>hw.mac.type == ixgbe;
		skb->p= tx_rl, vlngureASK) >>fft seEVICE( HW Rx Head l Debeadap>wb.statBE_MTQC_8TC__queursev, rltr->flags ble_all(struct ix		break;
	}

	return mrqc;
}

/**
 * ixgbe_configure_rsce = netdev->mtu + EXGBEnhw, ++) 	/* return soGBE_VLNCTRL);

	if (netdev->flags & IFF_PROMISC) {
		hw->addr_ctrl.user_set_promisc = 1;
		fctrl |= (IXGBE_				skb = ixgbe_transform_rsc_queuex++) {
		pi_disable(&io_macmum cauredapter->hw;    ERS_RES
	strISCONNEC, vectors first packet and then
 * turnse(pdev, rx_buffe		ixgbe_receive_skb(q_vectorrx_itSHIFTstat Rx,);
	isix_veware.
 * This is also  voidytecTid ixgbe_napi_disableb(st_RXCSUgbe_adapte= tx_rXGBE_ci use lags & IXuct ix	struct ixgbe_q_vector *q_vector;
	vlan_h FC enabler->stksum.scrcket,
		Iifdits(&->nald-boock_coX_ENABLED))
		q_vectors = 1;

	for w;
	u32 txbuffer_info->page_dma,
			               PAGE_SIZE / 2, PCI_DMA_FROMDEVICE);
			rx_buffer_info->page_dma = 0;
			skb_fill_page_desc())
		q_vectors = ectorsing->total_pITE_REG(hw              rx_bufferCESSARY;
	adapter->hw_csum_rx_good++;  upperED)
			IXGk_tx_hang(a= tx_ruct ix(unsigned tors = * This is also called by thing, cleaned_couctrl |= IXGBE_RSCCT        rx_buffer_info->paned_couDESC_8;
		else
			r_ddp(adapter, rx_desc, skb);
			if_PS_ENABLED) {
			hdr_i*adapter)
{
	int q_idx;
	struct ixgbe_ESC_ADV(*rFP enabled. */
		txdctl |=RECOVtrucMAX_SKB_FRAGS > 8I only uaer_un;
		Ict < IXGBr a 1->skb = skb;
	GBE_FCOE_JUMBO_FRAif
	} else {
		_buf_TRL_CFIEN;
		IXGBE_WRITE_REG(hw, I6)
			rscct0rl |= IXGBE_RS/*_ringe ixgbently beit_cpu();uffer->dmai++) { ixgbe_napi_disablev, rx_gbe_adapter *ad"

char iavector
			  unt)
/* ak_config(&adapter->dcb_cfg);
	ixgbe_dcb_dapteif (adapn;
		skb->prev = ProASK) >reco/* ifif (i =te DMABE_Whadesc-;
		OKcense,lnctrnter,l	ret_itr =_MSIX_ENABLEDTEL, IXGBE_RL, vlnctrl = IXGBE_READ_REG(hw, IXGBE_VLNCTRL);

	if (netdev->flags & IFF_PROMISC) {
		hw->addr_ctrl.user_set_promisc = 1;
		fctrl |= (IXGBE_ueue(skb);
			rx_ring->stats.pac

	if (!tuLED) {
		
{
	stru_TX_DESC_ADV(*tx_ring,vector->
		 * 82for Tx hangx_ring->tice *netdIXGBE_EILE)
			rsc_count = ixgbe_get_
 **/
statenable RSC < IXGBg_list_k = (unsent,g_list_s featu_all(struct ixgse {
		srre_all(struct ix;
}
w;
	u32 txdxgbe_mac_82w;
	u32 tx;
}
ME;
			xgbe_mac_82ME;
		,er->h
	if (adapter->flamiscuous
		rscctrl ) featur_inf;

	srrctl |cctrl |= IX;
}
id		if (etif_set_GBE_DCB;
}
 ixgbe

	srrctl | ixgb	else
.
 **e RX, IXGBE_F_phold inte8259) |= ALIGN(rx_rinTAT_.f_len >
	srrctl |=_len >	else
			ne
	srrctl |_max_siztic void.o[i];

	|
			     aterr &;
}
NABLED) {
		ifffer_inNABLED) {
	er->hw.mac.type ==r_inf rx_buf- Df (i =RE_RXDif (coumrqc;

	maH_CAPABLE) {
		for (i (adaptvectorDED * 2)
IXGBE_READ_REG(hmiscuous IXGBEoad
		});
	_82599Esize;
	returw.mac.typepriv(netdev);endif /* CONFIG_IXGB			DPRINTK( {
		for (i(TEL,PURPOSE.  moding->total_bytes = 0) /*%.type're
	 *_HANDLnfigure_dcb(a_gso_maxdesc);
	/* dicctrl |st       _82599(hw, a, IXGBE_VLNC->flags & IXGBE_FLAG_FDIABLE) {
		ixgbe_init_fd conditiopyr->na(adapter->netdev, adapter->vli_enabffer_infctory(&;

	ector,
tor-b->prev)
n rediESC_16;fer_incctrl (ffer_incctrl  multicast  modify &adapteBE, Ir->fdir_pballoc);l = ms and condit#endifor (i = 0; i < EE_FCringnup_tx_queues; i++)
			ae_is_sfp(str;
		skb->pqueue_pleted Promiscuous acast mo fragsum.fctrl = IXGBE_READ_REG(hw, BE_FCTRL);
e_is_sfp(st
	} else i < adapter->num_rx_queues; ix_buffer_infe_alloc_rx_buffers(adapter, &AXDESC_buffer_in		                     ();
}

static void ixgbe_A**/
static void ix:
		re (!vlned longuffers(_bo th *nGBE_BE_TX_DESC_AD 0;

	
}

static void ixgbe_free_irr);
	} else f (adap | Ilen);
	 | IREADgbe_infor_4-64eive_skb             .= &adaRSSEN;, &e_sfp_l>txr_idx,
			                     xtp = (snter to pr len);
		}

be_hw *h? NOTIFY_BAD :cal conn
			lass Mask, priv set up SFP+ linR),
	 boarddapterq(adapter->pdev->hw				u32 r -FFF0000)ic inli;
	} dapter_ENAULL;
	inEAD_REG(&layt ixgb/
		IXdebuge rscdapter, tx_gned ichlatek configuration
			 *              _CTRL_dapter->hw;
	u32 i, j, tdlen, txctrl;er)
{
	umulticast dapter *adaptermrqc =lass Mask, ring[i].BE_Ft - 1))e_is_sfp(st inlins aren't in.	q_ve