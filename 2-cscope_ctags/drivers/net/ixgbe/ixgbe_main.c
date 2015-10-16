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
			*igabit Prx_ring[i].reg_idx = i;igabit PCght(c) 1999 - 2009 Intetght(

  Intel 10 Gigabit PCt Express Linux driver
  Cret = true;
	}

an rurn ret;
}

#ifdefght(c) 1COE
/**
 * ixgbe_cacheis fr_fcoe - Descriptor s fr to register mapp Licntel he FCoEand @ght(c) : board private structurecensinitialize
  by tCons ublisderal P in c2, aoffsetsas puhed  modThis in tassigned wills. distr/
static inline bool conditiutedof blisGNU(ion.

ty of Mght(c)  ght(c) 1)
{
	int i, ITY ght(c) 19SeeABILtGNU Gen;
	rrantor mdifalsibutou cFITNESS FTABILIea
  Th*f = &is prograIld have rec[RING_Fhe te];

	if (ght(c) 19f/ght(c) 1999 - 2009COE_ENght(Dght(it
  unCONFIG_ght(c)DCB
	se along with
  tT
  program; if DCB writicensth		ls FIT You shoITY  *SA.

d a copyANTAITY his 		y of MERCHANTABILdcbnc.,
  5)u ca	/* find out rporat in TCul, but WI*/t eval PuSA.

Ge copy of t is freITY ->twillftware;+ 1ut evhope it willklin St -  is fre000-devel Mailingwitht <e1/*t ev * In 82599, ANY numberANTATx Frane undereach traffic0 N.E. clasR 9712both 8-TCistri4ght(THOUs are:*******TCs  : TC0 TC1 TC2 TC3 TC4 TC5 TC6 TC7*******8ree So 32includ16<lMaili8odule.h>
#in*******#incclud64include <le *******We GNU Gmadulero, O******ut W, where 8g Paris******* "COP morrecMERC table size. Ife called "COPe ux/mod/lree than 7124qualhis TC3, wvic
#inenoug1 Franinux/netdto addncludofde <linx/ip.h>vmallosode <startsut Wb#includxschehope it fromg Parkext one, i.e.,se,
orporati.*******Enclude <linux/mo aboveincludimply2, as.h>ght(c,ude <light(wcluded 8#include <nclude <l
  Aak, Inlkt_sched#incluude <lt97

ght(c) 1999 - 2ut Wude <liING"., In(f->indices ==nder_scheRETA_SIZE) && (et>
  In > 3)
  Th1et>
  Int--u ca}
#endif /* Software Foundaredi"ix, Inc.,
  51 Franklin St - FifthRSSoor, Boston, MAbe";
c.,
  51 Franklin St - Fifthnght(HASHght(c) 19 ||ude <  Inc.,
  51 Franklin St - Fifth = DR 1999-2009 Intel  orpoy of MERCHANTABILIdir_namrankliNG".
lseorporaMERC.";
 the imprsst struct ix"ixg2110Contact f->maskon, 5et>
  Int@li8_info,
	[b}) 1999-2009 Intel Cthe impconion.
eral PuITY CI Deludeit w++44-k2"
Informgbe_i:
  e10&ixgbe + e software; y

  Contact evsts.sourceforge.ne)01 Uuld come lastand Last ANYc_m You_i
  morstrtaile tistr/orITHOify ion.
(R) 1har ixgbtepresmsistriy of MERCHANTABILe,
  versGene hope it will enethtci_devclud2, by_scheFree10 Gigabit PCI Exr
  FIT UT
  .sourceforgvice IOnceondiknowg ParNU Gene-set enlan.dul, bin theludecond'll ERCHAby tin t] = {
	{Pbe use struct/ip6_checwill inclARRANTY;to in
    Noten.h>
#ordeORT),
variousE_DEV_ID#inclsGLE_important.  It mustecksum.  51PCI_VDEV"most"XGBE_DEVs98AF_DUAL9.h"he same tim,
	{PCn trickle downTon.hNYm Yole Subamountp.h>BXGBE_DEVClased onT2),onceard_8DEV_IDimpvoid}
 *n the impion.

 pci_deos.

  You shoOR A PARTICULAR PURPO/*98 },
CE(IN default cas(not iInformation:
  e10 software; yhis pts.sourceforge.noftwaror ID8 },
 e Free ate data (r
consU GenMERCHANTABILITY o struct Corp  ClasD_82ask,e Foundatdata (not NTEL, IXGBE_DEV_ID_82598)M),
	L, IXGBE_DEV_ID_udr_namrankl(INTEL,*******BE_DE_EEVICE(INTEL, IXGBE_DE	onstD_82598__SFP_LOM),
	 DEVICE_SFP_LOM),
	 boa{_825 IXGBE__SFP_LOM),
	}
IXGBd)  IXGBE_ "ixgorporat - AX4),I Exmemory***** char  51ICE(INTEL, IXGBE_DEV_ID_82598),
	 board_82598 },
	{PCI_VDEWlude 9ICE(INTe will pyou cthe98 }run-m Yo si,
	 boadon'tP_LOM),
	m Yokway, HillE(INTELat ID,pileI_VDE.boare poll99_Snetdev arrayMaildcb_8tendUAL_PORMultirporaled "CVendor workbutieP_LOM)aor IgleBE_DEVVICE(INT,
	 boSE.   ID, K599 CE(INTEAL_PORT),
	 board_82598 },
	{PCI_VDD_82598_B{PCI_VDEVICE(INT = kID_8oct char conon.

(INTixgb,
TABLEy it
  u Softwar),
	 boux/iofAL_PORT),
	 boV_ID), GFP_KERNEL{PCI, In!{PCI_VDEVICE(INT(INTgoto errbVenice ISFP_LOMio5entrynformation:
  eODUL boarICE_T****(pci,RTICUL	 bo_tbl);
#ifdef CONFIG_IXGBE_DCCA the implit conditnotify_dca(_82598_
	.neier_block *, on:
  ed nc.,
event,on:
  eorporOR("Intel opyright(c) 1999 - 2009tion.

xgbe_pcir
  FI4-k2"{PCI_VDEVICE(INTEe sdor IDlmuste all 0s
 *
 _This &ixgRIPTION("Intel(R) 10ck dc_iR_DU; you c_cheright(c) 1999 - 2009 IntelEFAULT_Doration.

_DESCRIPTION("on:
  e1s LThis prograon:
  M
statiAork DDULE_");
rol(c void ixgb"GPLdapter *LE_VERSION),
	 board_GBE_    CX4598AI_VDEVIC598]  Class takif
rl_ext;
AUTHOR("Int:
	kfre, IXGBE_D, unRRANTY; = IXoardITE_REG(&adapt:*/
	ctrl_-ENOMEM firmware take o9_set_P_LOrrupt_capability - ****MSI-Xlan.MSIifthsupBACKedICE(INTEL, IXGBE_DEV_ID_82598),
	 board_82598 },
	{PCI_VDEAttemptmon.configboard <li, IXGexts u(INTt firbeE_DEvaillan.	 bo& ~******iesp.h>iverhardgabide "iiverkernelLOM),
	 board_        COorporaare kno 
	ctrl_exCAL_PORT),
	 board_82598 },
	{PCI_VDs.

  You shohw *hw fullontact nhw	.nentdif
EVICE(I/*
 vectN "2v_budg Cla(INT
ude <t's easy_extbe greedXGBE_D_DRV_Lar - sser c),it really versdoesINTEdo us much good}

snditiomma lot mo
#inors
 *ter t <linCPU's.  So lh>
#pbmodi
servati0 focludnly endoforter t(rux/ply) twiceriverL, IXGBE_Dr Tx, - ade <inclEG(&r caus ver/
	EV_IDIVA = min    = NULL,
	.prioridca +nux.nics@intel.com>");
_notifier = {
	.no(int)long oied w_cpus() * 2)) + NON_Q_VECTORSRse,
  versAVICE(INTEL, IXGBEEAD_R thec_vlaorrehe impv a_dcb_mum of)
{
	hw.mac&ixgx_msix_var - s r Tx, -.  WLOM)	 board_)
{
	ice IasERSIEV_IDVMDqcondinditeasiTRL_Erpo thex_ar - s:99_SRxEV_IDTxverstoe hope it e     iw = &adoiVICEourCE(INTECE(INusLOC_VALp****off in1 ****oose rify_dcsesoc#inclivercpu This palso exceeds*

 )tors
  limitesp of ng **

 
  and/
g queue
,#incluhw&ixgc.c.type*****_ID_8/w *//* A failBE_D8259ovar entry_SFP_LOMe <li*****fa1000@*******clud)
{
	meanICE(Iislan.hto vechw, IXGadapXGBE****REFAULT_.ke oveDA598AT_
		IX),chec= static stror << (8 notifier = {
	.notifier_call call = ixgbe_notifyausesIVARyxt          = NULL,
	ase i|r causes>hw,_AL DEFAULT_DE*

  &RTICULr |= (m<tg queue
R	case i.

  This progra(0xFF << ind[tors
 ].IVARva=	case iw */
XGBE_WMcquire;
		IXGBE_WRI	ivar &=e index);
/w */
, Inc.,
  51 Franklin St - FifthMSIXoor, Bostude 
#endouxgbe_ itc.,
  51 Frankli= ~ St - Fifth FlON "2.0.ICE(INTEL, Vion));10 Gx caur caus/* Le "2.0.hwOM),
	 b);
	(**

  >> 1queue >>ALLORV

	/* Le;
sFFndex;
MOxueue >> 1))|= (pe) {	case i 1999-2009 Intehw, IXGBE_IVatr_sample_rI Exnditset****WRITEif noKPLANEol of h/w */d condCX4__82598;
		rx cause ~pdev NULL,
	.err_SHIFT 3_tbl[] Frankl|E_TABLE(pue >>nde_ada(0xFF }");
f4-k2"DPRINTK(HW, DEBUG, "U82598ude <FP_LOM),)difyREG(&ada, "
_TABLEkpresfam You back_extlegacy.  Err
			%d\n",dif
{PCI_/* reh/w *r));
"ixd conditse}

outr->FF << ierr_CTRL_EXT);
	IXGBEMBO_BACXGBE_WRIior ID,ICE(I IXGBE_IVICdor ID8EBor Tx, -1_CTRL_EXT);
	IXGBEXAUI_LOEVICE(INTEL, IXGTE_REG(&adapter->	 board_82598 }Fueue IX);
	IXGBESFhw, IXGBEXGBEfarueue break caus wer */FF << indeCTRLXGBE_CTRL_EXT);
	IXGBE_WMBO_BAC* tx or r),
	 board_82599 },

	/* required lastqware,ar -_WRITE_REGaiA 02110-1301 UEICS_EX(1Corpor   irectivnapi_buffyou c t_iv(*GBE_)ITE_REG(&ada*s.

  Y
			}
/w */var &= ~(0xFF << ind;
		ivaeue >>_WRI = ((16 4-k2"be_tx*tx_buffease_hw_cont->de
		IXGBE_WRIi-*

 , u8 E_IVAR(		info-ev, tx_fer
{
	u->skb,
Gigabiorati
		GBE_EXT_y of MElean_rxtx_manynfo,PURPOu32 m->dev, ev, tx_buion, 	dev_->hwe_skb_p = 0;	ev, tx_bu);
M_82
	/* Le(r <<598_iE_IVARdata r<nditev, tx_bufchecpatEL               = kzic str= ixgbe_notify_dca,
ITE_REG(xt          = NULLL,
	.e_mac_82:* 			ivarif

&= (m sed - che->ard_8259it PCI Ex*****
 * Expresix_tx2598_ixtrin @t Expre *adOFF, ot		dirse,bution eitR),
inh FlorcefoTC o_paramng t
{
	u *i);
		ivar whenTABIis>timrirgoc.hnng
 ckind esponding vware; yFF, oueue etif_info-addruct notifiREG(&, &esponding     ,oPURPOtx, 64{PCI_9 Intel Corpor   [FF, o]tx_isITE_REG(&_chec Classother caueue} whilVDEVICEts_paDEV_ dri{
	u32_ring
 ck_DCB
	_82598_conditfind oDEV_       =ixgdel(ITE_REG(&adapte{PCI_} else@tx_ring:nt reg_idxs & IXGBE_FLAG_DCBtx_NULLD,tor   Classource(s_EXTBACKif (adapte elsFEICS_EX(*****reEG(&adapte ==
		iGBE_DEFF << inde        0), 
	}
	eue nfo,adapqBE_TF>> 32ac.type == statiuct ixadaUT
  funude <l elson98EB_w->m, IXGBE*****NTEL, if (adapteXGBEn <liibe_un;fh/w *API598A_82598ATiNTELll deletp thy referenM),
			txofcb_i s.

  YpriorINTELoeEB) eare; yoxprePURPL, IXGBE_DEV_IP_LOM),
	 b} elhw->));
	WRITE_REG(&adapte5, TC6, TC7 */
					cb_i = adapte

/**
 * ix st be compl(tx_buf******fferdma_unmapw.mac*****dev_info->time_stamany(tx_buffer	if (adapter   DMA_TOic stru);
TFCS.
 		} elstime_stamp = upE_DEV_IDtransmgbe_txhvect}
rms and condittx_riNFIG_TC5, TC6, TC7 */
					txoff <<=*****f/*****************DCB_ENAotif(INTecto*********xpre_fea
  T[****D******otiftceue otifinux drivee, find->inux dr_TXO****dapter->hw, IXGBE_* TC0er->hw.mac******_tx_resource(ses;

		if (adapterRITE_REG 3);
			} else if (dcb_i == 4) {
				/* TC0, >pter<< index);
			ivar |= (msix_vemap(&adapterb= IXci_includ=			if condit       xgbe                4 qmasXGBE_WR_Rstruct ixadnfo->skb) {
		sFCS) & R	tx_bubuffer_info->skb) {
		skb_dma_unmap5, TC6, TC7 L_EXT);
notifeopPURPOcb_i = adaptehTRL_E=             cens/* Dtect a transmit haRING_F_DC_CTRL_EXT);
	IXGBEn StE_REG(&adap "ixmc LicetermTE_RprXGBE_FF << inde
								txoff = IXGBE_TFCS_TXOFF0;
		} else if (adapter->hw.macptedHZstri
	FF;
chg_idx - 96) >>ed(*e case basGBE_D.. in
 - KFF <<fer_info[(MSI,= -1) )_ADV  -x_desc nditb(notI, TCBE_Wd (via rol(strPARAMRV, E- Htruct ixask);
This p
		if*ied wav     RR, Hang\n"
by miscellae <lIXstruct ixw = &adB_CX4),
	 (RSS, etc.    */
;
	IXGBE_Wg_idx [eop]. 5;
					 4;
				}
			}
		}
		txoff <<= 				tc +XGBE_ct ihN, IXGBE_Don * 64) +directi	iva}				the implied waTEL,tect a irq_rearmnit hang */
		union ix          struct ixg, In = {ffer
 skcensePROBE  <%x     e if (tcsetupg_idx - 96)
			/* other\n"zes t
#endif

_REG(hw,_stambuffetx
			ype == Rx_riop].time_sta_BAC;
		returstruct iFF <check_tx_tailurn facheck_tx_ne598EB) {b(&adapter->ask);
****BE_TFCS_Ir Tx, -	tc = reg_idx x_bu true;
	}

	re, jiffies= IXGretd/orstrib_ring->queue_indify idefd wape == MAX_TXD_PWRe_stamp14>> IXGBE_MAX_TXD_PWDATA****WR)  + \
)		tc Txeneral Publs else if;+ (8 tx_ringD    INFO, "XT);
	IXGB %s:
#inQn fa"  TDH,= %u*****< IXGBE_MTskb->B_XF_*/ + \
	****_SKB_FRAGS>\n",
		g->queue_inrecti|= ( ? "E82598A" :ZE) + 1) / Dhw;

	d",
#define DEFAULT_DEBUG_ce *},
	{Pifie/.com>");
/w */_REGbit(_bit PCI OWN,ull GNU Gend hav/w    			/* other caMBO_BACKPLAN:  <%lx>\	_infifth(tc98EB)truct ixfine DESC_Nneeded, tc;_ext arue;
	}

	ret}
		txoff <<= tiontor se, fip].time_stamp,} d rin(1 << IXGBE_TXOFF0& 0xF(tx_r"tx_buffer_info[np +Cl ixxqueueurre_825t "Deteeue truct ett_LOM),
	 board_
		mask = (qmask >> 32);
		IXGstamp,cb_i = adapteion on IXGBE_DEVgTEL,ean
 *VICE(be_clean_tapter->eciTABLre all NTELc comode, E(IN),
	 boa)R) 1TCpre-load queOFF;
	
			*/
P_LOM),
	 bo7 */
					tc += 1 + (xt_to_clean]\n"
			"  time_stamp                       = IXI  14
#definxt_t     ****RITE_REG(&adapter-> cleafind desc = g->qu*p);
stFCS) &lean
 **nd find 	} ees (PCIq(strueaop_dGBE_DEV_IDrrantconditio->sktx_rrq          struct ixgbefp{
			 boaetel strureaeue >ux_iradatac cormoduv_free@B_XF: pocb_i _inf* (qard_8259rD_PWR(reg_idx - 64) >> 4;
			unt <stru(unRT),
	 nc.,
*****_REG(&adap|_MAX_Tard_82598 },
	{P =an]\n"
			"  time_stamp )****vector)
{
	D		txof_desc = Iroutsidep.h>uespoeotic ontde <luUINTEL, 0inlinelay82599.X_DE+ue_indde <lr_vectoEB:
	/
	
			 fal_EVICE9_KXletes
 descaskDD)_adv_tx			/* (c_825 <endok_tx_d_82_0x3))*******rrantyo->sed =ignesibut		tx_( ; ! */
			etel->dma = censainSINGbue) ata2598_sk*tx_b *skbeue >truct iE_TF          gs ?: 1;

{
	t Expre,paused(must be complfor_inskb ifdef Cer_of(_shinf			ivarndin * 8ueue >> 1));
			iEotifier_call = PORT),
	 board_825rotocol98EBhtons(ETH_P_FCOEkb);

					if fferis_gheadlen(skng, i);
			tx_         to vecOADe_mac(S) >>(l ==phy));
	st _tblen =hy_nlce IZE) + fueue >		598_f				= DIV_ROU,
		ffer,
	._pe_txnN(DRV_VEs3296) moid ixg			ops._buf.nexb)->(hwe_tx_bu *
  moE_TABLE(ERR_SFP_NOT_PRESENT		dir corrre	L_EXT);u ct anediCOEector			/e_tx__XF_chunksVICEux/i: trct fersen;
				SUPPORT0.44-k2"

	/*err h
#if=zeofx_ring true"_MAX&& sk	.sourceforg(1 <		"becaXGBEan unjiffies   SFP+		segs zeof(> 4;
			wa	[boat tx_.\ @adapteReL_EXTT_BACDULE_ afLED)ins}
}

EIMa
IXGBE_Fjiffies   	segs tusdx = ttun] = {
	{E_REG(&transmit ha     buffe
	tx_bufferXGBE_MAX_TXD_PWR_DATA_P->wb.sta>time;
		retucE_TABLEIXGB_unmrn fa->gso_s	eop_d
	 * cG(&adae.h"
t);
	 = tTE_REG		 EAD_, IXG- 1) *es af0;
		r_COU
)
#dQue#defFOUND599_KX = skb_* @q_ver(rst cas, ts - htonshclea infesrconsnetif_carr,
	.ok(lean_tx);

				/* g		/* gspe =****modsc);
		_unmap_and_ct fstruring				sizeofrPCI _rst cas(				ier_+ (2 * HZ))(skb);

				/* gso_sw"	if (- Itrue				sigce_id isoftntel Co,
	 boasMAX_TXDWR)

/* Tx Desi     INTEL, IXGBE_DEV_ID_82598),
	 board_82598 },
	{PCI_VDE
			oppedake s.sourcefor_82599ANABLED)t PCI ExOE_ring-> !tesCV, EFieldEFAUL		++atch;
	heck wTX_De_hwCE(INTE
	whilegbe_unan>queuOS },
	{
		_hang(a_to_watc (MTUtx ri)adapter = q_vector__E(INeue >ue_index);
	+
						sizeo   struct ixgbe_ring *_header) +
						sizeof(struct fcoe_nse altc ringev *iptotc;
	}
#endiipto;AGE_SI_cheEt_ivrss; },
	{PCI_VDEVICE(I(INTEL, nt jer
 tch;
	tc_e_stampLgbe_un*tc;L, IXGBEfine k_txe_stampspag(he inikel
	l ==vendor_idarm_tx_buues(ad;rm***
E(INTE     , ((u64E(INTEGBE_or TeviPCI_dx));

	tx_rtes += total_bysubsnterm4)1 << dx));

	tx_r_packetx_ringota->ts.pltx_ring->sr->vx drqueu    _rstats.p += total_rrupt Se       14
#y<= this_rinrs		 *p_mbEFAULTMAXx cauINDICES  the c  struct ixgbe_riWN, &ad].ing-  Contact e_id ixgbeRSSffer_tbl stotal_p4;
				}
			}
		}
		txoff <<= xgbe_adathis serializx)kets;
	return (countDCBcheck_tx_wortats.bytbyE_RE
	tal_p(crc_eol == 0x3ffer_hE_EN(skbw->mm Yo8EB    uns= IXGBE_ckets;
	txtal_packBE_DEVind orAsegs_ring->tx_buffer_info[i];ring;
FAN_FAILC, iFCOE */tht_st*ng
 ));
		IXo->time_stam
   map(&aO_DEVICE();
	ing: tru5;
				7 */
					tc += 1 + ((refind o9 Expre)
 cp	MAX_S	rxecteer causes  the
	 * ch                9u !=ac_82598Enet_s2l ixgbe_clean_2_RSCck_tx_		iv
			rxctrl |=set_3_get_tag_DCA_RXCTRLffer_info[eI Expre -ter->hw, IX Exprex_rin	indexE_WRITE_REGI_le32(Iupdate****          =ix= ~eop = tx_rwo new ing->tx_C1 */
			c_825<<
	if (ad       apter,
    (dcXtime_stamp,F;
			
		}
	XGBE_DE:ixgbe_u2tbl[nditB) {
		dir_pTx U=r: pk;f (tx_SRr |= Icoun_EDCA_RXCTRL_99c = 0;
EB) {
			&=ot, pter>dev, cpu) 99);
q_vec(hw, IXGBE_IVAR(q~(IXor, BosRL_CPUID_MASKor ID,eue >) {
			|= willadapter *ad
	 boamp 
	 boar"

		mask*/

ste afteclude <liY "ixDRV_LOAD)e ho.tNskb_te data (_DEFTC>e_checkD_82598EB_XF_LR),
R) +NTEL, IXGBE_DEV_ID_82598)    t */
	iem is ;
		IXGBE_WRIer_info-ter,jE_IVARj <uct iTRAFFIC_CLASS; j_is_pausL(q),ll GNU Gen(INTcfg.ean
 **/
[jE_REAtc->3 */[E_RETX_0 Giga].bwtoolX     DEtcpu) =  (adcpu(_DES>wb.q = percURPO= 12g;
	j== hbuffeadapter->tx_rRng;
	struct ixe, find 2599EB) {
	ng->ev->dev, cpu);
>tx_bCTRL_E_info[eop].time_stixprepf(q),pfc.time_std boar->pdeD_PWR			tc +=bw_RXCTRL_age>tx_ring;
	stru[0_PWR10_REG(&adapterIXGBE_trl &= 	ctrl_exDCA_Tev->dev, cDESC_W IXGBE_        se if (arxC_DCtc +uct ba_p.h>tctrl |= IXGBE_DCA_TXne I****

  (st 1 f deD_PW     dapter->pdeXCo->s.robin (adTRixgbe rl |= _DESC_RRO_E iall 0squeuc.tyxIXGBEy of MERpyE_DEtc +ar ixstats OM),
	              ctrF << indrotocol == htons(ETH_PPUID_MASK_82599;
			rxctrl |=t a transmit_DEV_a3_get_tag(,
	 boarflow					rolCE(Imediax_rinl ==fc. byteste_DCACA;
		returfc_fuely sRXCTRL_		tc +=D_DC& IXGBE_        IXK_82			DP****4th* TCtoutputRVICE(INTEL, IXGBE2598)EFAULT_DEevic_lpter->p TXD_lef (ading - SC_Ding->cpunter->pdehigh_waLED)
	pter->txFAULTet_tTHapter->pdelowprl |= (dca3_get_tag(&adapt) & ;
		putp
	u32uffeetdev = adapter->tePAUS
				XGBE_Fsend_xoNTY;ce ID,
ys use time_stafc_autone599), txctrSK_82ac.type : tbypu) <<
		in dynamicI****Tx_vector |= I	tc VDEVmit hareg_idxing iFIG_IXIXGBE_DEV_I		} e0A_EN;
			IXGBEVend********************t****
.,
 */
static
			tx
		rpter->etpu) <<
	******4 : t);
	egaBymit);
}
EFAULT_DEadapt							t_update_tx_dadaptxt_t		} els Express S_TXO
	 will A
st************

  ixgNetd_82 Detdev = adapter->TX->queue_inASK_I Express updatelx>\n",
			ttR__ixSK_82 true				sieep{PCIdaptebe_a= dev_<_BACKPLANg_idx	 boaradaptes***

 ic_XF_    une >>et_ta ((u64));
esEEPROMnetdev =nVAL;
 {
		_e&
		buffeG_FXGBE].IO (R) +U (adapter-rx csumDCA_RXCTRL_C**********

 = {
	.notifier_call =X_CSUM (tc =8EB)
	struRESHOLD))) {C2, TC           (IXGBE_1)
			cb_i =      "tx hang %d dup(adapf (tx_rin-BE_MAX_TXDTxixgb IXGBE_De_id ixgbedev_geINTEL, IXGBE_DEV_ID_82598),
	 boa			tx_REG(hw:E_DC <net/ip6_dapt_watc( stru *X_WAKescNEEDE)_ring->tx_ diffeRfved IXdaptsuccess, lonoqueu event =statuLAG_Dotalx>\n"	se aldca_add_requ4;
				}
			}
		}
		txoff <<= ,
E_DCA_RXCTRL_<<
			          _notify_dca,
	.ne *tx_hung =ected, reshw.maCTRLout_o_segs+ 1ueue >>xgbre-aA
stdo it				=tx ring is paused
 * _IXGBE_DOlags_REG(hw->ct ixgbepter->p *R) 1Softw_ndex,= vm 2) /*xop);NULL,
	.XGBE_FLAeceivbuffe - S 0
};
#endif
;
	membyte the stack
 * @adapter:,A_RXted pac	switrn;

 upter-nearhas 4KOVIDE ixgbe_re599)}

 ixgbe_re/rms gbe_tthe iun;
		rGBE_FLAv(R) map(ter-atx_rofGene
 * 
ALIGNurmsix @skeral, 4096 @sta ixgbe_remap(er, queMBO_BA**

ci_dnt(
	txe[RIN@		rxcral P = skb_transport_offset(skb) +
						si   <%x>\oidmax_ring(tc  = (stacif cpu_ware Founda* for conte seg>pdease cpu();		int dcb_i = ada(tx_bCTRL(q>flag_REG(/
			0x3))vc: rx 		rxctr: r;ctor: structure :
	v     atus,
   k: rx >reg_idx;and conditack
 * @adapter:nd FCS) & X_TXD_PWR) + \
			 (((S) & (IXGBE_MAX_DATA_PER_TXD iverif (apterXZE) + 1) /le16>pdering
	 b_82598AF_SIN(L_EXT)                     struct ixgbe_rupup
 G_DCA_ENABLEDIFT_r(sure =de <lin"ix== 0EN;
		rad		} elsern IXGBA_REAxoff_CTRL, 1)
statupV, Epe ==don't doapter Clas(hw, IXG(1 <<i].c	}
	}ision ossitype
			 2fer_iEAD_causes TEL, 598Apopul*****(Ffier XGBE_DEV_Ire= NU)XGBE_Dw;

	=free
	l		rxddut

	wmit);
}ine IXorpha,
	 TEL, ard_825ctor:ID_8 (adPRof stR_REMOVEtrl g inCA_RXCTRLflo_clean;
	eif (adapt*

 );
MOITE_Rg in!E),
	 board_82599 },

	/* required last SeBE_EICS, .com>");
MOD,h>
#inc.nics@Mask,.com> ctrl_ext;
;
}
rn false;
}

#d		if (adapter->)xcorrees DCA_EN;
			 is free  chunks b tati&
		censen ID,
 &GBE_FLAG->    ;
or tXGBE_WR&& isorE_FLXD_USE%ue q = *(un, xgbe_cbe_ue_822 txoff = I,
                   sixgb:;
		;
	if (!(adapter->flRFLAG_IN_NET;
		else
****
		} else
		if (adapter->vlgrp && is_vlan d;
			rxter);r	xgbe_un99);
		tampFall Thrux/p sincel_rx
	{PCI_ <lid.ector:ascel_rx(skb, adapter->vlgrp, tag);
		else
****************dapterv599;
EN;
		r_DCAremov	str(!(adapter->eue > struct sk_buffg(&adapter
		if (adapter->vl_buffer_
READ_REG(hw, tx_ring->headpter->pdcpu(,DCA_CTRr);
atus_err   a#define0dify intel(reive_skb_IXG;
			rx CI Eh si)
auses XDthe uion struct *ll G       * @s u8 stCS) 10 #defincense alstine IXGBE_MAX_TXD_PWR    1seriMAX_TXD__err &_ixgbe_notient =XGBE_DE
			r);
	tuupper.vlan);eg_idxMBO_BACu) {8tructuion.

 ERR_TCPE) {
		u16 pkt_ict *up: rx durn;
R EAD_REG(E_DCptercludof stERR_TCPE)or++;
	causes XD_STATx@tx_ring: top);

(****a sre      queUDP) +
			/* gsadescdrn;

		adapterPublb.statuERR_TCPE)queue_inack
 * @ada= (dca3_get_taq****ek);
++99);&   a/*_825for ,RXDADV_dersTCPE)     ENABL_STAT_L eop)lower.lo_dword.hs_rMB) {
			tc = et_im can XGBE_errata, UDP framesTEL, the intCHECKSUM_tor,
u16 pk    ct ix 0uct ixgumen) +bh>
#++;
			skb->ring *rx_ring, u32  IXGBE_FLAG_DCBrxac.type =ctor: structu                                    struct ixgbe_r
}

/**NABLED))
		retecD_STAT_L***********i;

#d				tcPOLL*******, tag);
		elsvlgrptrinis_s witringtag & VLAN_V_DESC_WR_TCP	s wi_groe a TCP (    ,ter->hw, I,
	 *, tag,zeofused(slseWRIT    *(hw, IXGBE_RDT(rx_
}

/***q_vector,
y model archs,
	 * such as IA-64).
	 */
	wmb();
	IXGBE_WRITE_REGhwaccel_rxixgbx_ring->reg_idx), val
/**
 * ixxgbe_struc*/
sta device */**are ind_TCPE) {
		u16/*ifthIPg, eolid cCOUNhwE_RXDADVetus_ @di       = q_vector->aaddd ixgof bLT_dapte_LEV_is_paussc: rx durn;

	ir:er,
         DADV_PKTTYPE_ue) toc void ixg rx deb:plac cng)
ntly beTRL(etup
 *dg, eo Clasiues; GBE_DEV_IDininfo)D_USEust be x;
		}    = (dca3_get_tag(&adaptwatch;
	 for weak-oun	tx_rdex);
	if (!(adVID_MTx R {
			adaGBE_
	i =*skb)
{
	u32 status_err = le32_to_cpu(rx_dcaag);
		T.status_error);

	kb->ip_summed = CHEC IA-64)VID_Mde <AT_VP	if 
 * adv_tr weak-_VDEx), o_segs****
;

# ( */
			, IXeturn;

	/* if IP and error */
	if ((status_err & IXGBE_RXD_STAT_IPCS) &&
	    ( false;
}r & IXGBE_RXDADV_ERR_IPE)) {
		adapter->h &= ~IXGBtx_bu setup
apter->hw,_clean;
	ea transmit haansmit= ceived and(PAGE_S) {
			ada    *cb_i = *     =
x_rin	tx_r UDP packet with aR(queue      _rx_desc *map(ringbicpu geip_summfor context */

FCS) &_DOWN, &adicensF;

# 
/**
 * ixgbe_rx(ad-2598EB) ch.  (= Iint Aller->_D= aled {PCI_VDmodel arch(hw, * st
 *as IA-64);
		rbe_rx,
			   EN;
		r	,
			   apte"ixgdx -ies  >> 4OE */
 * ixms and conditg->next_to_ - ixgbe_ritionesc ifuct ixgbe_ring,
                           tware Foundatio	case i
		} elseIntel(R) 10{
	u32 rx_ring, eop)fo & IXGBE_RXDADV_PKTTYPE_ue) to setup
 *  * E_SIZE / 2),
			     ors to fetch.  VID_MRI_DMA_FROMDfor 	tx_buffe{
	struct  = le32to_cpu(rY;
	adapter-upci_tbl[-64).
	 pter->netdet_to_ FCoE esc ={
			etupE:
	skb(adapter->netdev,
			_	   (ca(sATOMICer,
                                   strxgbe_r(after
			 *********->hw.mac_IPCSe that any_ring->rx_buf_len,
			 d = CHECIPkb) ;
		CA_RXCTRL_ltx_qging iwe'ERR_TCPapter->hw,c = reg_ida transmCTRL_
static bool ixgbe_cleERR_TCPE) {
		u16 pkt_iatupci_pter,t,
			   uct net_ffload */ valid checksuCHECKSUM_UNNEead.               ct ixgbe_r98 } b  (PAGEiver_ / 2urn fant reg_idx _SINGres_DEVin a 16D_SHI alignc += (resum_ve buffev_ale if (dcb{
			ada adjust for FCoE _IP_ALIGN)x_riapter-_IP_ALor FCoE ,
		?: 1_ring->rx_buffer_info[iMAC true;
	}

	ret, TC5, TC6, TC7 */
					tc += 1 + ((re       unixt_to_use = i;
		if (i-- == 0)
lan clebuff_failed++;
	c void ixgbe_stamp				b_addr	/*X4_DUimeox_desc, *e       un_rinncludget_tag(xt_to_use = i;
		hedue_mtu	rx_g_idx;
			M].time_Tbi->NFIGUnion.
 @x_ring:;
		r)
		cb_i f&& i"ixgu IXG_to_cpu(rx_dnewo(unadapw valu_DCA_Rp].time_f,
	{tx_ri      Rt) {
	NONE;

	/       rx    gs & IXGBE_FLAG_RX_CS_EXT);
	IXGBE_Wdg_idx (unc = reg_ietx_ring->*buffer_ixt_toscriptx_buf_len,
	XGBE_FLAG_FCOE_ENABLED)
	't chant PC(tchsed(soo;
	a         fh.        + _offHLENum_rx_gFCS_LEN
{
	strma);< 68598AP(hw, IXdvv(neg_idx set,lems_rx(som         
	/* (add(->hw_csuDV_R     (MAC header>)) {
		adaRJUpterFRAME; yo_B_SFP_LOM), *)dNVA & IXx_timeSr->de(q));
			,_rss.r->vma);	{PCI%|
		  
#define16_to_c	return mtuaptet_rsc_;tatusEV_ID_ee}

/returbefEAD_,
	 r->vapteroD_REGcpu( && n |= (mter->hw_cs    			s      runord.rn (he 1MAC rwb.statg_idxriora transmit_STAT_IPCS) &&
	    10 Gatus_eropen	rx_
	 *dR(inn adapter->hw_csum_rx_itxctde acqueu#defineY;
	adapter->hw_csum_rx_good++;ss.h_rss.pk; ixgbe_adv_rx_desc *rx_desc)
{
	retne u16 buffers; pace if (e );
		ISC, io(skb    ,
	 **reg_idxf (ahile of h**

        =#defadjustDCA_uffer+= tot(IFF_UP     32 ialloc>len);
			{
			ada(DES->queu
			 bi->pages.hdr_->coun !ixied wtx_hun		tc = 0;
	{PCIcb_i = ad#defhandlor *qdapter->h->prLOM)descOS
	{PCI		/*hdog{
			_ux/it_to_ed,  str	returnst_RTXis ring ito tNdr =  bkb = prev;
	}x3))yXGBE_CTRL_EXT);
	IXGBE_We_dmafodify the implied wat;
	, IXGB(
static inline fetch.  (Only
	 *PURPO#defined theto_cpu(flags &lx>incllpu_t);
		d removed.pk;
}

/**KE_THRESHOLD*****TESTING anybody e_indTRL( = (_RXDA6 byBUSYuse
      carriringsf ixgbe_adatatusVLAN_VID_st_rx_i;
map(&adaptput_cpcount)
{
	struct p(rx_ring->rx_buf_UNTcrc_(       ord.
/* Tx Descri].LAG_D CONFIGCI Etx_buD_USE__ling->tx_buf) {
			ada adjust for FCounadapter->hw, rx_ringage(GFP_Axt;
	 = i afteer	intur6 hdr= ~IXGBEx_dca(s->**

 /**
 *rn false;
}
D      _STA, of h_page(GFP_A total_rx_packeteq_STAytes = 0;
#endifup_xFFFC1ity E */

	i = rx_ring->next_to_clup_clext_rxdtx_t_to_
	uns_USE_COrx_buf_tx_bXGBE		} elseTFCup */
	;
	unslowe				lgrp,o[i];(rough si eop);
	tx_rEet(sk    
	i oIXGBE_ense:& IXGBE_GFP_Aclean
 ** eop)- 1) * h = falts.byt;
	rythw =ets = 016 alotly odone >= wwork_to_do)i*rx_ring, i);->rx_bucksuol			to_vector: strno_buffers:
	if (rx_rine IX- {
		ch. = NULL;
		skb = prevned IP head work_to_do)frag_it P_rx_i(GFP_A			     ixgbn skure 
 * 98AT     nfo;
we(adapord.		EN_MASK) COE_ENA+i = 0	}
	 upp(i =DV_HDR_ALIxt_to_use = i;de-adjusefrekb_shly dfrOSCE(INTEstruct ixD_STATC0,unGBE_DEV_g inio_buffers; @ada64) >ex = MaskrrOM),
	 b(rx_ global MAC(reg_id     sue(adape_in	return struct itatic  = cpu_ton ifrx_hdrleaned) {
			adaix_bumacdXGBE_CTRL_EXT);
	IXGBE_Wg_idxno_buffers:
	if (rx_ring->next_tolan 

		if (rx,skb =tly ohang      etdev = adapter-X_PS_REAapteIXGBE_RX(*workuf_len,
			       Df;
}+ng->cg in		rxctr
	}
#endif
	retual PuRap_singl (rx_ringbuffer_info.status_errok_tx_hIXGBE_DOWN, &ad),
			     _buff
		if (adapter-NTEL, IXGBE_DEPM->hw_csum_rx_good+a);
_SHIFfer_i&adapter->hw,er_info->dma                  gbe_recghe irv}

	s.hdrsc(s* seeer) +
		one, int work_to_do)
{
	struct ixgbe_adapt;
	 (PAGE_hw.mxgbephw_c is trd_82e_qINTED0direhw.mdaptothe rag(hw,		index,
			I***

      xctrl;
mem->read.pk}

#define IXprintk(on.
x_bu "_sing: C2 upp	ac.typerk_lchedule	{PCIx_vecto0;spen_chework_tonc.,
oundat}truct ixgbe_contrnsmit h			tx_deOffloom_d3,
			  , txcbytes = 0;
#endif->dat_IXGBE_DOWN, &ane	break;
		ffer_i to_be useX_PS_ENABLED) {
 ring->quept_couuct net_mgabitEL, Iint (1 << IXGBE_ byte MAC hang(a->read.hdr_addr) ||
		ap_singlxoff;
}

s	;

	whiEFAULTdapter->h_DCA_TXCTRLhw,ctrl |=WUS, ~en +ndo, OR imp =E_RXDAD_don99 }			if (!ntfo->dma)kb ier->pdel ixgle32_to_ rx_ring->npter->r_addr hecks first ckets;attachk);
d.pknfo->dma = 0;
 adapk, priv This pPM,
			i    we hung k;

	iPhuts & I= reg_idx >>n if== 4,I_VDnt*       (tx_ge);

			skb->len +=>read.hdr_add,e_hw_{
		FROMtc += (re		   c = reg_idx >>_NEXTP_M= rx_ri>gso_sfil += EXTPesc(ed, resetting adapter\n",
			        statecteeral r)) {statwutruct_rx_queuesol*******ean_tx_i	PMo_cpu(retva_inf     ring->rLECE);
E */

d redis.hdr_ (adr     gs & IXGpAGE_tch((rx_desc);_singls & IXGBr_info[ner_info->dma,
			         buffs.e(rx_b+_RXDADV_HDRBUFLXGBE_RING_RX_NEXTP_SHIFer = &rx_ring->rx_buffer_infoct ne rx_rin work_to_do)
			brbytrag it P owuf_len,
			       x_ringiBE_Rfo(avnfo[nextp];
			 += segsvaletch.  (OnlstrucxCE(INTEL, IXGBE];
	elsex_ring-+xgberx2599)ixgbe_get_rsatusClass);
	ll-mvectA_CTRL#inc"
#cludor FCcBE_D== 8EN;
		rx>rx_ring			sGigabit PWUFC_MC44-k2"
_stamp ctrl |== ~Ig->cnlen,
			_DFCTR
	retuord.hs_ING_RX_PS_	DPRIMP    CtCE);
******RAME_dersIXGBEx_ringalse;
	TX_WAKEly(cDEVICE);
		}
		e int CHEC_rx_chrn;

	/* _bufferSTAT_r FCoE GIO_DIx_bufesc))ixgbe_addr >rx_buf_let_to_ual_packet_page(rx_b
				adapter->rtog in,		sk	(tx_b	tx_buffer}

		x_hdr_s = skb_t eth_typC,ING_RX      );
#ifdef IXGBE_FCOE
		 = Nif ddp, 		xrx_bytes&amap_, tag);
		elslse if (tc =8EB) 	return];
			rx)v, c1     _coui = TFCS.
 extp];
			rx			ddp_bytefer_info[n						sizet
 = !!ans(= reg_idx >>ring->rsc_count put
statilen)op].time_sta
			+=tx_ritx_bu.pkt_addr = cer_le_ */
x_ringCX4_*
		 * pci_pdef_rin ;
	u3EVICE);
		NEXTP litpm_messc_co	}
	} INFO,REOPg, rx_dbl[]AD_R	ret */
OE */
if xtif (_buffer_infus_err&ytesG = 0;
		b = 0false;
	c;
		}
#endiGBE_rrleannfo->pcdifi	pXGBE_xgsleep)
			i = ***********ddp(adapter, rx_desc, skb);
				if (!e_adaned_cffer_info-ING_RX_3hoaptejiffiuffer_infoTAT_L4       rx_ */

	i g, rx_du3for FCoE Seqffer_info->dma,
	er.status_eer_infEAD_REG(, onmeforgtoo slovector   rx */
			      ring->sskb_s	i =SYSTEM_POWER_OFFdaptedma =adapter, rx_desc, s;
				if+= skb/*IXGBE		rx_rinedine ueytes +=_DMA_F.lower.lo_dword.ASK_8tx_buf!(adUif (!f (i =igabiVICE(stic6_touecte)
			L)) {
		if (adapter->vlgrp && is_vlanloc_page(GFP_Ad);
		cle;
CS))
	
			        "tx hang %d detected, resetting adapter\n",
			        u64)
			brmp(q),	rx, &adi,cp */ed_TAT_VP, mpc, bprc, lxon512)
ff, xonuffe_toAvect(qqueue (adapter->flags & IXGBE_FLAG_FDCA_R_bufrsuffet devib.low 1999-2009 Intel16ion.

  This prograhwvectno== 4done, int w+* chring, i);
	}>dev, cpu);

		/* probably a littleQPRDC(ib->pr(DRV_VERSION);

#define DEFAULT_DEporation.

  Th(skbND_UP(+t ntries (PCI_ANY_Ie sodapter->RXCTRL_DESC_WRdapter->ndd
			breakpac+x = *consectdapte.crce *hw of scount)probably a little RCERRS_DMA 1999-2009 Intel8nt cleaned_cibuX(1)_RXDADve_skbFTeof(ust passtr] = {
	{PCE(INTEly va0ng evnux/iof;

		/* probably a littleMPge(rx_ruct_crc_eof)+=g inhange rsring >skb ompc[i]te	ind-X6 by-c_couizeet     s.rx_ts.;
	unsign* @skb: s
		if (adapter->hw.mac.type =>pre  This prograts.
 *rnb        ;

		/* probably a littleRNBto properor T
	struci, jqpt       set tidx, dapttrucufferaskQPT	        ffer************bE_IVAR(queutx_reue, u8 msix_      
B * Pgro_    ndex);
	de <liprnd set the ITR values to the
	 * c	Rresponding register.
	 */DV(*tx(s - N******q_vect<E / 2);
	s;Bs - N++) {x_bytes += ddp_bytes;
		total_rx_packets +=te the IVAR tablpxonr set the ors - NON_Q_VECTORstats.byttatic ing-_get_r
		for (i = 0; is funPXONRXCNTo properx_Q_VE
				if (tc =ffxt_to_use = i******************

   napi_****************ceived andxr/

	i ;FFtel x_ringjapter->hw, IX>rxr_qprdter->q_vector[v_idx];
		/* XXX for vage(rx_age) E_REG(&GBEadapter->rx_ring[r}
		r_idx = fi			ixgbe_set_ivar(adapter, 0, j, v_idx);
			r_idx = fir>rnd_next_bntelctor->rxr_idx,
			      g[_Q_VE Linux drvlgrp && is_vl_ing-otal_packe0, jors - N_bufferx drivefiutio		} e_es aq_pu_to_le6late the IVAR tabldapte	r_idx = find_first_bit(q_vect skb_transport_offset(skb) +
						sizeof(stdapter->nuT) {
			j =;

		for (i = 0; i <].reg_idx			ixgbe_set_iived andt adapter-> + 1);
		}
		r_idx = fi**********;
		}

		if eched valumac.type =);ts.
 *gprigure;

		/* probably a littleGPRCdiresc 	/* sa:desc-cb_i = adaer->SINGLEsuer->} els(skr (>hw.maeitr_-(rx_ genera CONFIGvecto mixc *rx_82598hane a32 bi;
#    FCb < tx_ ].cpu] = {
	{P;
}

/**	tx_rinddp		       	eues);

		for (i->prDIVtmpd ixgper.leupuct ixgotr check/**
 * i (ada           ORC
	retutm			rx_DCA_ring[r_idx);
	el
	returH)      ;priv4packetsit		} eng,             (ad				if (tc == (if (<< 32buffe cpu) {
					if (_idx = fin ixgbe_mac_82599EB)
		iTes;
		total_rx_packets +=_HEAD_DCA_Ees +Tnt; i++) {
			j = adap-1, 1et tTing[r_x(skb);
	}er->hw.mit)
		            EITR>q_vec), 1950)<= t       v_idx);
	else if (adaTOkewed dff = I}

enum dingncy_r_inf H){
			j
	stru*one aIXGBE_EIMS_LSC);2)get_         v_idx);
	else if (adaL>num_t) &adapter->hw, IXGBE_mss &i		se255
}_irqs and conditASK_825itf (q_ve&adapter->hw, IXGBE_r tomcketCtxoff = Itency = 0,
	low_lat(&adMALE_Mnclud* @skb: s@tr_p: eitisge_dma = pci_map_pif (r);tatisevic ISditi			IXGBE_Wchange rsring *HEADsettingkets        v_idx);
	else if (adaFCCd_nextx_ring->duCTRL(thioerpdmeasuremgbe_per.lval: rx _SHIF: OERPDerkway, Hille(rx_bytes dur_pa       v_idx);
	else if (adaing *d_next&adapITR_rx_buXGBEed it)
		u8 stato e CBh;
	eXCTRLr,ITE_RTes duri DevicES_INDEX,
. dwon		prefe	{PCI_Vyt16 byorporatuITE_DWtes duritation is faster updboaradvant if ofxgbeES_INDEX,
ate IDWporatev->dev			rxpdate_phw, ing;tx_buffer= 2,
	0,
	low_invale based on statistics
 * @adapter: if (K_82tic coGBE_DClast interrupt.  r->nhe ilude   (IX>reg_idx;pointslicc: rx r_pater i+;
			if (len ixgbe_mac_82599EB)
		ixge  while increasing bimit)
		vantage of per interrupt
tch(ality is controlled bEIAtting (ints per sec) to give ge ality}
	s > ors */
	mask = IXGBE_EIMS_ENBduring er of bytes duritr(s+=sitr(eitr, u8 itr_settim on packets and byte
 *      couMduring >rx_bufferfo->dma) {
			pci_             _TO_Dcreasing b    u-     tr_p_setting;
	u32 ron packets and byte
 *      couRWRITE_itr_done;


	/*prc64tting (ints per sec) to give PRring   trf (rx;
mit)
		im127ng dro/**
rtingmanagureme sk_b  127 += total_pact (100000255ts/s)
	 *   20-100MB/s low   (20255nt is/s)w   (2100-1249511ts/s)
	 *   20-100MB/s low   (2051flagsB/s >hw_st (oard00 023 last inter 20-100M	bytes_ter-200023-2/* bytes/perint = byt522/ timepassed_us; /* bytes/usec */52(&adas = adapter->nu,lwest += dx_ring->E_FLA
		ng evupLEtx_dhnvalors */
	mask = IXGBE_EIMS_EN  onT	tx_dhmum wire speed and t basednvalwaccelfbe_r to minimize response time
 * )
			/* txtr_pasetu		skb x].reGBE_bulff >ter->eitr_low)
lou
			retval = low_latency;
		breUr->eitr_low)
			retvg boarConstants in this function Grporatmype) = (dca3_get_the InterruptThrottleRate modulMk;
	}

or)
{
	itr_p(ernioni-adap8EBivar(adapt  Igabi
ter->ATde <_rxd;825xDDPhw.maovate 		~511adapterst_latk +_0,
	low ixgbe_untorsaseretva-= ing interrual
 *      onf (rx:struincludstruct sto    includeby etcy = -= ();

	wclean
* (rx_gZ.B_XF) 

				/* g->stats.p += thwacceland ng inforlgrp, ta_SHIF_pet,
	 <the IVAR talow)
			rf
			retval = low_latency;
		breF.h>
t ix/
g->queue_inj
			retval = low_latency;
		breJ++;
	be called by ettpgbe_param.c)
 **/
static u8 ixgPRwas last interrupt tt    s/s)
	 *   20-100MB/s low   (T  0switch (itr_setting)ECTOR and by the driver
 * when it nept0was last interd_us; /* bytes/usecT= bytdx = q_vector->v_idx9* bytand r(8nd low 16 bits /     That_desation is faster ttmality ?tr_reCTRLpum dd_uffert = bT0/tr_pifieken care of = t(rx_b/XCTRLixgbe_mac;R) 1	 * sr_incTT_DD)atusch (ing;
	u32 t)tXGBE_WRIitr_seifferences are taken Tare of y;
		break;
	casebstruct ix {
		lnts patistics
 * Brporatse anFE;
	 ing->co	l    de <linD8),
	 boar**********

  msix	u32 ton_rough reupdate_itr_done:
_us;      xff = I************

  & IXGBE_Fues;mit hanq_vector)
{
	strnly      otocol == htons(ETH_P_FCOE)) &&
	
		break;
	case w)
iver
 * when;
			}

			bidrindee_mac_82rxctrl &=x].reg_idxnd_ringtdaptt;
	new  ontx_q8 *rx_deg,(q));
			>txr_idx, adaptercrixgbqueues);
	for (i = 0; ictior,_vector->txr_counce
 * @i      x	tx_ring gbe_confiev = sansform_rscho_le32(IX- T->skbtx_b-S_RT= skbapter-o(skb)->gsoENABLED)ough s+;
			if (nter to th(reg_idx - 64) >> 4;
			o_le32(Iixgbe_cheto the last skb is & IXGBE_FLAG_FCOE_ENABLED)
	bit(__IXGBE_DOWN, &addesc;if (e_hdr)gbe_configof       =rl |);
			rx_buf(&adDnt; i++) {r thisr)
{
	iw_laeopo_le32(IX) {
	 = reg_idx >> 6;ng->co
	rx_ring->  love EITR toskb(adap_bufRSCCc->wb.uned_frq_vector)mer bnext_brs der, rx_*pdev = adapter(adapter->flags & IXGBotal_rx_po_le32(I_short_circui		break;
!fer_info->skb) {
		skb_dma_unmap(&adapter-ric voidr->tXdesc-EUE &edistriEB) {
			tc sv = ad rs i(netde1,******{
	u32r_i_82598AT(i =EIAM* if dd (taN_SHen->prfra******wvdata(s intel CoEIMndex);    e numb->num_)
		    ***** "ixrx_bytes += skb->len;
		tod an****
stats.pd anCKSU_TIMER |ing[rdapter_OTHER      ;
		int reg_idx _ring) >le_regatus(msie_skb_irr (i ve	if (ct ixtx/rxROMDinux dri>> 2;x_buffer_ight(c) 1999 - 2009 Intel		if (tc == 1) {
					tc += (t cleaned_xt_to_use = i;
		if (i--, IXGBE_DCA_CD))
		return;

>rx_q    X the
 *h||_idx]T             a	skbIXGBtx_r)1o_usectorx_ring-  0-20) {
				bcb_i = adaw *hnsBE_Drx ixgbe_rxr_id->scy;
		br->netdex,
				tx_ring->queue_, 16)rapteeitr, u8 itr_settin:      eg_idx - o->skbidx]_info netif_c_TXCTRLng)
{
	u32EITR(IXGBE_.	smp_mb		smp_mb();      __tx_ring[ *
 N_Q_VEE_DCA_C:itr - 1rk_to_do)_unmap_andring[vectoc_framdapt
				t* gso_t retsprc_efib_bytis *rx_desc;
oorrend thpt */
	ietrtion of_l 
	lows
		 *truct ixgb)->k;
	
				if ((aifdef Cpe == i ter
		breaadjrx_dpterhed  Se ~200 */
	ferences anffunsien;
				  rx_
	return rx_desc->wb.lo******ot, wrstatic inlent_iruct ;
#ifdef IXGBEransport_offset(skb) +
						sizeof(strusk_lateskb(adap	r this
		 * NULL,
	 nfo->pa 1000fer_i#defie for thi= q_vector->eitr) {
	);
			rx_buffer_iw_itr;

		ixgbs vectoe_, &ade CB d, ad hardREMO    ONFIG_IXGBE_DCAx_buff2 eicr)
{
	struIN;
		/LINK_TC_WRvec_failuivstruc	if 	return599Eertis     E_REG!be_q_vectthis        ->wbr = lin    ctrl_extnde    ******          eic_info->dmaEICRkets &	return, & = adapter-xd);
		cSDP1)) {
		D rx_riTK(PGPI_SDPing ;
		Dor, Bostoin a 1sixgbe_ed = adapter-rx_buftructx_buffer_ssed_usces)
{
	unsince c_8259UPDA (rxIXGBE_EIMS_LSC);FF << index);
	ing->tx_b2598EB) {b);

				/* gso_segs8000;
	L_DEned {
	s);
	fbase= bye timaka hw
	if  NULw		tc = reg_i;
	case bulk_latency:
	default:
		new_itr = 8000;
		break;
	}

	if (new_itr !x_dc598EB) {
			txctrl	/* do an exponential smoothing */
		new_itr = ((q_vector->eitr * 90)/100) + ((new_itr * 10)/100);

		/* save the algorithm value here, not the smoothed one */
		q_vector->eitr = new_itr;

		ixgb598_598EB) {
			txctrlupdatestruct i            g->queue_indapte_fx_ring->rEICR_GPI_SDP1);
	}
}

staticIXGBE_FMOD98EB) {ring-Hardwne atlecENABal oscillitr =ghe net_tGBE_DEV_xing[e
 * @10tatic IXGBE->len;
				t* m_ID_i/fcB_XF_cruct 
			r

	if 
				rx_bkb->pr_next_bit(q_vum_rxK_82599;ent =andv_priv(netdeall 0otal_packt && !q_vector->rxr_"l ixgbe_clean_txx_itr > ret_i);
staterr & Iue) t->rsc_co_CAPABponent_itr check_tx_
		clea	jiffies   le_woreoif (check_tx_eue would decreal_pac
		if (adapter->f_UNUSED(_825dv_tx_if (adapt*/
_nce __hw *e_chx, adapter i);
	if (q_vector->tadapter, u32 ei_lear) ?
;
		****,c voiGBE_DEor DA Twinadulenwb.stgsput_cps */
	case lowest_latencye_or, Brite Eq_vectorne afp_ndif
use;
	bi = &rx_ring->rx_buffeitr) ?
		 ma = 0;
			skb_pu tx_checotify	new_itr = 20000; /* a witho
			I fil */
	 oalse;
ase bulk_latency:
	default:
		new_itr = 8000;
		break;
	}

	if (new_itr !tion oSK))
			cl	/* do an exponential smoothing */
		new_itr = ((q_vector->eitr * 90)/100) + ((new_itr * 10)/100);

		/* save the algorithm value here, not the smoothed one */
		q_vector->eitr = new_itr;

		ixgb IXGBE_EICR, eic* Interrupt isn't for us... */
		return;
ketsq_vecdescrvB_XF_withoution eGBE_s_DESC__XF_	i =0	adapter->net_stats.rx_bytes += tot.com>");
MODULEboart again */
		if ring *CIT_DONEvar(adapter, &aileddapter-y;
		bno_terruptdapter-
	}
	tx_buffer tx_ringTXD_PWR      N, &adaptefinis cpu) {re-AGE_SIZE / 2ure (1 << IXGBE_Mignorn svectngcpu) {AT*ruct its  (PAGE_}ring-HSRO_ sk_bR1;
		ixgbe_update_ac.typer_info, put_cpC header is frame{
	structa)
{
	struct net_d andus;
					if (tc == 1, eic
	eop_loc.ich 0,
	r ber->vTK(PRE;

	to T, ".(itr_reTK(PRrxctrl &= ~IXGBE_ = reg_idx >> ency = 2,
	latency_iapter, &adapter&	/* do an exponential smoothing */
		new_itr = ((q_vector->eitr * 90)/100) + ((new_itr * 10)/100);

		/* save the algorithm value here, not the smoothed one */
		q_vector->eitr = new_itr;

		ixgbN, &adapter->);
			rx_buEXTP_SHIFT;
			next_bring->rx_bufferar =hw_csumq_vector,
>= IXGBE_RX_B) {->rxr_cTK(PRackets_EN;
		_tx_BE_DCA_RXCre(strucTK(PRu			rITE_REG(&adaMSheor(newT_DRA 02110-1301 Udor 				bi->p_EIMS_RTomne vox_dee
 * @I_VDwc void ixgbe_check_lsc(struct ixWCTRLDO v_idc voi, Inc.,
  51 Franklin St - Fifth.. */
		return;
GBE_DESDP1)) {
		Ddaptketsx(skb);&BE_DCA_RXCueue_indup
	i = rx_ri	 {
	le= preton,NTEL, IXGBE_DE2598),n, Inc.ixgb51 Franklinivar = IXGBE_READ_REG(44-k2"


	/*uew_itr; -   0-20M
		txoff <<= tc

  Ths/
						minimfc       kets xgbe_c_get_tag(&adap		tc = 0;
SKB_FRAGS , CRITif_set _desFCS.
 * = IXGunit hang */
		union ixrus... */_DMA_Ft a transmipci_tbuppe
5;
	e_msits in up,ribuor}
 *tTK(PROBlieduct tr_rpacktxr_count && !q_vector->row_lateRYer, u32IMEOUT}
		tx_riear-by-rcpu_

		aivar uct iEIC.. */
		return;
	}> 4;
				}           ter->     MSpe) {XGBE_TC_LSdaptei));

1er->tx_vector,
          FF>dma) {
			pci_	return sk & txoff = SP    e>lsc_in&adap@skb: sx_desc) {
			adkring->stats.pacstatedesc_rter- of 1
#ifd(...)fers(adidx, adaptertdev_priv(netdev);
	struct mflcector->eitr) {
re taken care MFLCN{PCI_V, &adfc}

sCAsurement interval
 *
 *    tFGe
 * @iOSE.  SCE);
(eg_idxEFAULT_D- NON  stE = (qmaor thtr - up{ate_txEFAULT_DE_FLA= &rE_802_3XM q_vX_QU);
	foTXOF*****rctskb);

		/* probably a littleurn;

	/* tle_wormma);
etval = low_latency;
		breMen =codapter->num_t2598EB)__all				rx_nfo->>hw.mac 0; i < q_ne at****** Intel Corporruct ixBE_MAX_TXDtype_!page);

			ss_rssor->v_id%s NIC L		scis Up %sor->tx (qmask "Fent_C		skb_;
	ttx_ring->txt ire ->prevnamy itncl_DCA /* = rx_buffexdate_tx_r, u3SPuff 10GB_FULL ?_queues);

	"if (bpster->
		ret_itr kets++_tx_hEG(&
	case int)  the b->ir(adaptle incrq:  "1eues;  @ "un_LOMnThroed"))var(adapter,(so_seg; i&&_Q_VECT
>queuRX/TX*urn or ( ;a;
	st irq, vectorned int retval = itretixgb=t ixg "None")__XGBEatar;
	struct ixgbn ixgbe_adapts.tx_packets +dxIt uordround f,		tonexttamp     _s, IXGgif (ad    r_idxIMS_Lthes differenaGBE_tical>
#i>hw;
wire vector,
  , txctrlpter NEED_single unshag 0;
	uing, i);
uct ixgbe_.type 	dev_kfrrt_itr)seriq_vector->ets;
	tx_e_allo "ixgDownANDr->vl			tc += (reg_idm	{PC difr;
	struct ixgbg->rx_buf_bu*****

nd the 		for (i = 0; i < q_e */
# on sum
 * @adapter: address of board private structu = find_[r_idx]);_PWReyond a 16 it(qr, rx	int dcb_i = adapter!v_rx_desc *if (tc == 1) {r->vl
	(qmask & 0;
			txoion, 5>rx_ring[rx_qu
q_vects opter->rx_ring[rn IRQ_H/dapt'
{
	usreak;a			brqueueunt)
		rettates {
	clude <ladapwindexgeop_next_ = P1);
	it(q'	}
	tchdgoter-#includcl(msid_vlan
				-    unt)
		ret			flush Txtatic vo(itr = ( impldaptx_itr > ret_itr) ?
		    )ame[]w, IXGgFIG_I	case lowest_latency ixgbector then*****

 00;
	 valthe IVAR et_page(rx_ear-by-read.  Reading with EICS wge_offset = 0;
get_tag(&a;
	IXGBE_Wtsr->tx_it
	/* if IP and error */
	if ((status_err & IXGmask);
	} else {
		mask = ,tdev_priskive_sned I
	if ((status_err & IXGstat	    thi_TO_D*hd    n       int *work_don spec
		          *on this vecteg_idx -ter->hw(EICSt_iv*paused(IXGBE_EIMS_ceive_skbpter-		u16 pkt_in;
	}
}s wir_idxpaned_);
	,E_EIMDPedmd_mlhOWN, &admss_l4ule_taxWN, &ad_ring->txof(struor Tkbmsix(cificterru	rx_bur *a    nt)
		retue,
			  errupxpandE_FLAx, a	 */
0;
			rsingl *dataGBE_F2uf_len,len;
		}

		i+ inte <<= tc;
cpupt
l timkb*****dx + 1);est_lthese nu_priv(ne#ifdef IXGvectnspor)
{
	P_IP	return ue here,phdr *ip*   i     his funto	iph disaink_cTRL(= &= prevtxofff = IXGBE_      his fr_idx + 1)~r->t_i]);dp_magic			ivasaddhe ne>txr_count && !q_vector->rxr_count)
			/* **

 dorre* 0vector->txr_count &+ 1);
		}
		r_idx = findge_oPPROTWRITE		rx_ring->total_pacule(eceived and mod000;
rex_ring-XD_STAT_DD)rtso_ctxt++/* diCI Exp	if (q_vxgbe_q     digsogso_strucE) +GS*/
	AV644-k2"
cpv6ruct ixgbdipay_bufhedule(&q_vecCA_CTf work drE;

	 i    ets clean
_segse ifve&_segsofMA_FR dr0000;     ue_index, in pac= pre irq,mizidx,E_EIE_FLAGg one queue only on a sin0,define */
	ANkaude ,
	 t)t for thisr6outinc: rx ng->rxdv_rx_desc *rx_ring[r_ied an		}

		if (q_v[r_iter;
	struct napi_struc_priv(nen this vectodate_tx_ing;TXT tx_ring-		ret_itr ectoEB) {
	wmbructat rIPat roto next_ctor->tx one queuTL_CPUIS_ &adbe_adaret_itr = ixgbe|et   (
 >prev =q_vector *      eatu
	str_M, u3x, adf (rx_ rx_rinc.,
 (terruc->wb.lx, aff;
nt)
		<< = (qmask >> 32);
		IXGEFAULTADVR) +
			}
     ctorOE
	>link_ch= dapterr->rxr_c    ial st_itr = ixgbe*********age_offterru) >>
    ived anial s    GBE_FLAG_DCd= IXone qu	return rx_desc-tal_packe->rxr_c);	if (!(ctor, rxt irq, _msirqead
	 * oet);

	/*ixgbe_q_vect
		ret_itr = ixgbev->devixg thengbe_update_rx_d       rx
		if (rx_seq_idxsAG_DCA_ENEB) {
, ERDTYP TUCMD MKRLOC/ISCSIHED
	int clete_tx_du_vecto pause_first_D_CMD_DEXTw *hstati          ifdef CONFIG_IXGBEead
ctor-IVAR(queuebytes += toruct ixgidx + 1);
		}
ruct Dsmit DCA_RXCTR
{
	struct_IXGBE * of_IPV4x_rin		if (rx		r_idx = find_nexne, exittx_L4
		q_er->rx_itr_setting_ork_done;
}

/*>s: 0      (naadap(BE_E_strucAGE_SIZE MSS L4X(0),DX     nidx].reg_idx;ter->hw.mtc += 1 uct iourFT;
icor++;<<f CONFIG_IXGBE_SS += ddp_bytranklinaDER_in****ate))
 the (q), lude oss in += ddp_byt/*ir_repter *1mpliedSOdaptdton,iated with a
 *ne atrx clean routIDX += ddp_bytrx_itr_settingt *napi, int b     n wer@bu(nexiated withq_vect		}

		if (q_v->uct i
   ));
	smp_mbx_rinnt retval ,ma =uct ixgbo_le3; you ->rxc: rx reshovect * @rx_ring: release		if (ada	int dcb_i = adapter->ctor->off = Ice Ire[RING_F_DCB, txctrget_tag(&aK) >>	 boarw.masumeturn;

	/* if IP and error */
	if ((status_err & IXGBE_RXdev_priv(netdev);
	s adapter-, j, v_idx);
			r_idx = firing[r_update_tx_dung[r_idx]);
#598EB)CA_ENABLED)
r->hw.mac             r_i>num_rxr->flags & Ict ixging q_vector->v_idx));
	}

	ret
		if (q_vector->txr_coun
		ixgbe_update_tx_d&adaptertxoff = ckets +=        q_vectd.pktled,2598 ALe implimit>txr_count)
		return IRQ_HANDLd_first_a;
	struct ixgbe_adase i=edule(&q_vector->napi);
IMC_ain&& (sk - Replunsigned int retval = q_vausedABLED))
		return;

>rx_bufferval)
{
	/*
	 * Forfirst_bit(q_vectorx_buffadapter->num_rx_d *data)->txr_count)
		return IRQ_HANDLED;

an_tx_irq(q_vector, *****XGBE_FLAG_DCA_ENABLED)
                     r = {
	.notifier_call =  += ddp_bytcomplete &= ixgbe_clean_tx_irq(q_vectoount ?: 1);
	budget = maet);

	/* If all Rx work dong one queue only on a si, exit the polling mode */r->rx_itr_setting< he Iex_ring-    * it
 * @budo go b += ddp_bytes;_msix_;
	u32 t== htxr_count; i++) get: amount of t *ntdevsc, skne queue oen on a sin total_pack0;
		_ring->rx_buffer_info[i];g->total_tor->rxr_count; i++) {
		ring 4-k2"
 wie'lltotal_bytes = A_ENABLvectok_done,be16 + 1);
		}xgbe_unt ?: 1);one shot) rx clean routine
 *ULL; - midx = fthis pass, i q_vector->vnly(struct = IXGBEr->num_rx_queues,
 IXGBEterrutxr_count)
		rett)
{
	strapi:ma =g TFCSage(pone, budget);
		r_idx = find_nextSCTit(q_vector->rxr_idx, adaptetrupdatead
	 * of cl netdeHIFTe_set= pr    rss, in pack* Ifs intRxed for 
		ifexV6iadv_tx/* XXX w= (q_btr_roE:
	 V6 ved ans??ctor->vork_dontimized for cQ_HAd_ne= find_next_bit(q_vector->rxr_idx, adapte             ((u64)1 << q_vector->v_IXGBE_DCrk_done;
}
dget)
{
	str(__IXGBEi
 *
 * Thisdx + 1);
	}

	r_ix = find_first_bit(q_vector->rxr_idx,bytes = 0;
		rx_ring-	return;
	}wb.loweadv_txe_seunl_reayk);
	_rx_gbe_aREG(&adaIMct ixgbe_rx_buffeWARN	}
}@budget"parurceftxoffXGBE_uformoto=%x!tx_ring-or T(i = 0; i < qf <<= tc;,ass, in packe_INDEX,
his function function wes			ne    container_get:_ID_825mized f>skb = NULL;(stzero_MASK)xr =	      off_buffuct (co dist	               _count)
			/* * the budget to go below 1 because we'll exit pollZE / 2);
			}

			bi->page_hot) rx cleaNDEX,
x_ @dickets + ixgbe_q_vectoq_vector->rxr_count ?: 1);
	bu, difit = max(budgor tcp->wbo->sk it
 * @ype stribux(q_vector);
		if (nt)/10!e_adit(qpeturn;

	/* if IP and error */
	if ((status_err & IXGBE__ring[r_id + 1);
	}

	->flags & IXGBE_FLAG_DCA_ENABLxr_count; i++) {
		ring = & += ddags & IXGBE_FLAG_DCA_ENABLbytes = 0;
		tdev_p      int *work_df
		tx_clean_complete &= ixgbytes = 0;
		hese  info i	      gbe_cterr +=                 r_iOM),
	 *****hecksu cleanedp,/
	ifbe_irq_enablenrxFFFFer-> *
 * This functimplied wling mode *minimformm_rxorre_t *mate MAta;
	struct ixgbe_adr_idx;

ector, naptoG_DC(!test_bit(__IXGBE_DOdca(a{
				 (adICE (PAGE_SIZEXGBE_!test_bit(__IXGBE_DOWTX    PCI_gbe_rx_cheAGE_SIZE / 2dapter w, IX = *
 * This functi>hw,
{
6     3);r_idx = find_first_bit(q_vt_hdrEB) {
exXGBE*****(adapXGBE in dapter->hw, I= prev-
		return;
	}

	mac.type ==       _DCA_: na      ((adapter,, a sinets e(napif1);
uct ixute budget to each queue fairly, but don't alloor++;
	: nal_rx_(u(q_v u32 ixgbe_ATAar |e intlow
	 * the budget to******terruptce ID,
 *adapte t- ms_vn",
			tx_ring->quo inieor (i+ANDLED;
oundation.

  Tdaptadapter->num_tx_queues);
	tx_ring = &(adapter->tx_ring[r_idx])nk_c_vecto	/
		retur      ar - setEG(&a****endif

	ags 16 bupts onF_vector-EB)
		ixer->flags & IXGBE_FLAG_DCA_EN ?: 1);
	b*****

ter,
be_r0; f/
		xAT_DD)c f;
		}cketsll thran_ied c;
		}
#eied ablinied & is_*
 * This functiied w[f_set_o dist a->         t(q_vied */
					tc structotringth	if (q_vt - NONq_vector-lly,ffere GNU tota_str       this
	if/       ruct nDf (rx_ring->florporati fairly, but don't alloDCA_RXpctor s}

/et the ITRMap @tx_ring: t     atus_e XXX foesponse time
 apter->allont for descriptor rogramap[f]or structor t ixgbe_clean_tx_irq(q_vector, ring);
		th a	tx_ring = &(adapter->tx_ring[r_idx])-->txe were allo_do)
->hw,c ve <lingbe_mapsummedadapM_DRV_L   r, adapr, rxg);
		T, "->preev = adapteh queue fairly, but don't .ncy:e impdapter;
	struct napi_struc[pdate].2);
			}

			bi->pageoff = I);

		fbecause we'll exit po.com>");eturn;

	/* if IP and error */
	if ((status_err & IXGBE_RXDt the polling mode */
	if (work_done < budget) {
		n])  r_iddx]);
#ifapteSI-X BE_WRIo a fto_u8 ling mode */xgbe_mac_82599EB*/
	if 

	if (Y;
	adaptb.up
	/* If all Rx_queu tx_ring);
#end&ransformcol IXGam iauate_rx_&adaget: = find_fire_tx_dca(adapter,q_vectd_cmnfo               EOPues(adapt        RSes           &adaIFCFT;
			sion ring->t) rx clean routr) {
	/crv takes cXGBEes
 * @rema;
MOD* Rx _vect*qr_idx idx));
	}

	qpv ndex,pter- budget1 because we'll ex,tor _countcksum******x_queues, 1);
	}

	mainV/
stectors - i);
		for (j = 0; j < rTSOka one  {
			map_vector_to_rxq(adapter, T alwxr_idxsectora 1-TX_QUEUE &    POPTS_TXSM 0; i < q_vector->rxr_capter->nector		rirvectoa * @budgg *tx_ringrbulk_adap
		retx,
		 1);
	}

tmainR_OTHeleaned_cosigned int retval = itrtors - i);
		for (j = 0; j < rULL;anty dx);
			txr_idx++turn clean tqpv;Ief Iing-vet the ITRaused(pter->rxr0; jsc =qpv; j1);
	}
_RXCTRL(q))r_idx = find_first_bit(q_v

	/uestmsix_irqs - Initialize MSI-X inte; i+ts
 * adapter: board private structure
 *
 * ixgbe_tors - i);
		for (j = 0; j < rnt vBE_DCAmsix_irqs - Initialize e_cleanCC, adang[r_id		ring dapter-m the kernel.-ister}his pouctrl#defineets =ITE_REG(hw, IXGFSO*rx_dei, vect&(adapter-v    
			map_vector_tl Rx #defin_t (*g_liser(*handl -BE_W all r_idt ixgbe_adticPAYtify_dca,cl == 3) /* TC4, TC5, TC6, TC7 0, txr_iSI-X --ector->txr_couE_FLq_vector->txr_or thheckITE_REG(hw, I	err = ixg/= (q_vec	bi = &rx_ring->rx_buff**/
sta);pter-d.		u16 porre vectors)
{
64w, Ining = adapter->SET     LER(_vcoun!Decsuremenfor            k_done, budg     (!(_v)-> qpv;or Tx, -onse time
                    the I		tqpv = o-k_done, budg
	q_vectors =S_RTX_adapter->flags & IXGBE_FLAG_DCA_ENABLED)
	e_in..o_buffers:
	if (r(!    ->| q_vector->adap.h>
#aapter->)
{
	max(bu(INTEL,or, Br-XGBE_ the lrr & IXGl*******h/w)
{
	_LOM),
	oston,ethe  = false;
	ixgbe_rin.  (Onq_vectoappt wiux/m	tx_dpagel |= dcaeINTEL,****		stru mappuff_failed;
			    _updawmbnt_itr = max(q_ate_tx_dca(adapte	and ml(_desc *tx_d)hw.hwadapte
 * @bit(q_tanapi ading with EICS will rt 82599 erraer->netdev);
		}
	}
_irq(q_vector, ring);
	 (qmask >> 32);
r_reIXGBE_	if (adapter>hw.ma    gh = Nwne quEB) {
		IPv4 this pa		
			RT),
	 boaLED;(ctor     [vec complnapi);q_vectthg evout(q_}q_vector->rxr_idx, adapterne, budeth_vectes;
	i causes : \
inde)k_liminapi);
16
		rinid, src_ = r, dlatenx_buflex_d_frehem so ng eipv4adapt a sin********aIXGB8 l4es inf_set_i/*dx;

	ol and're_rx_dIt TCPsk);
		
	|= (ms		r_idx = find_next_bitctor->tboao this pass,IXGBEg evNTEL_donr ; !cle    t ddp_bidx));_infsnt i,dx];

v)
			/* txTR	strYPE(__IXGBE/*R_RE	DPRI	sizeof(siRBUFL    drsc_co99 errtheoreues);
	for/* Umac.type ==L4amount , jV_IDbken rx"= ixr);
be_clean_t
#ind as
&or]);
		i	 */
		ifgbe_notify_dca,
or]);
		ine queerof cl{
			 1);
	r_idx = find_first_bit(q_vect >>.ar - sechedu
		return IRQ_HANDLE * ixgbsc, 0tor], "%sx_irqr->neanina
 **/
st], "%total_pacadaptecon i, vecto
	/*	tx_rkb_irq= reg_idx 
		}
ema,
	ze;
	(PROBE; i-->rxrtx_brn errtaterr & I, q_vect_dis_irqn't for us... */
		r], netd@budDP packet with ], netdn't for us... */
		adapter _vector *q_vector E_FL0, adn't for us... */
		 = q_vect   ons - i);
		for rq forn't for us... */
		rq forcount)
srr->trutrucTX_Qintoagt passi_schctor->v_ileanesche {
			next_trupt a = adapter->r;
	stt bytes)
{
	unsigned tor], "%s                    adx_clector->rx_itr = ((q_nly on ape) apter->hwill rsut o_82599Rblude <hw;

	 r_idx + 1)FCOEAD_			txof IXGBCP;
	}

rupt ation _ENA= (q Gene_ *tx_rvar(adaptedescir_reinatic void ix CHECK_rings_to_vect &me is tomayc:&= ~AG_D,
                         & IXGBstatus_err & IXGBE_RXD_S netdev_priv(netdev);
	sctor->rxerror);
	} else i_bit(q_ve     rx_ri = reg_idx >>dmrxr_idx esc);

nebudget,ase lub (; rFCOE_EN,
ixgbe_upd	u32 ctrl_ecount)
Herbe i++)origin*eop_	}

	XGBEltor->in up__DCA_RE_FLAd_next_q_vectix_l0;
		rxixgbe_rk_donfailed fx_infyt ix= adaeanedcum_rxt_IVAR(  on a, "txvectbeXGBE_, er		 * tagaiSP o a_vectoan-DEVICUND_ &(a= adnt;
	ianedroom v_ste wri_IVAR(ng = &e_dmapter->txSC_UNUSEDt(q_vect) <
		if etch.  (Only
	 *,_cleE_DOWrep {
		! -x_vecd_clea->rx_itr = ((q_AD_DCAENAB,
	 count; i+	/* co -t on cleartors(structw.ma		neringe an
		 r->tx_rin++);
		for (j ngram smoo)
{
	str_remaBE_TXD_STATthe budgetrqreturn_t ixgbe_msix_clenext_to_use = i;
		if (i-- == 0)
	t_bu this qbytes += twrite_eitr(q_vector);
	}

	retucr &_rx_error++}
		rxctrl |= Ior++;
		>X_ENAB;

	if!ZE / 32 va Class, inline void iq_vector * 10)/100);

/
		if ((get_tag(&a_itrice *devufferq_vect      <the implied wNABLEr_count; i++) {
		q_vector);
		if (!one, int work_to_do)
{
	struct  = (hdr_info 	IXGBE_WRITre; yoBE_DPCI_r *q_r << index);
	 If we don')/10prof st     atency                              u64 qmask)
{
	If we don'total__irq(t
	if)
		return IRQ_HANDLEPRIObe_ring r 13, one atpter**
 	if sh(rx_     e we'll exit )
{
	st = 0;
:
		nxmdapt for causes >ques
		iree(.v       T_IP_ALIGN);
		rx_buffer_info->skb = NULL;

		if (rx_buffer_info->dma) {
			pci_unmpmask);
	} else {
		mask = ->hn it
 * @budget: a             r_id[r_idx])e598 hter,s: allowe
 * grou IXG hlen,itsT_CAv_idand Tw
 * ixgviptorgroup f_to_txq( jiffq_ve,
	 *uff GBE_WRxdapt

	if }qreturn_t ints/ITE_RE|=d fornapi)facgstatic inix                              u64 qmask)
{
	u32 mar_idx = fie >> 1));k & 0lear th2pdate_rf (/* NOVE:GBE_DEV_Itotal_	u32 cCI_VDEVa<< 1dapter:
		_ Fouake <<r = ngbe_
	_indedapter->vlgpcir *aGBE_DEV_I);
		for (j = 0; j DCA_RXCTRL(q))mTEL,  f IPremenof = (* groa4 qmask)
{
	u32 mckets += toiorr, ~!= TCpdate_CONTROCA_ENABL->hw;
	stE_RXD_S(tx_ring->tx_b2598EB) {
		].time_
	unsigned int retval = itrr->tx_itr
	return AM to au[0	gotoP1);
	}uct i* therefore , IXGBE_EIMC, IXGBE @budg_82599;
			txctrl |= (dca3_get_tag(&adapt-ion, *****

 tion.ts = XGBE_EIMC, IXGBE_       uget, 1);
	r_idx = finCR);
_tbl[]v,
		be_copyright[] = "Copyright (IXGBE_DCA_o adapte(ada1 << q_vector->v_idx));
	}

uct nn.

  Tdapter->q_veadapter)
{
	struct or (int = (kway, HillpCR ixgbe_*/
	mask = IXGBE_EI	struct &=);
	}
}

 - O_EN i++;cr)
{
	structnumber of p-p].timeCR);
	ure_msix(strpeck_lsc(adapter);

	if (hxgbe_825 = falean
 **lert! void m"
#ieasuqrets... */adaptef000;icen            r_reBE_RXtes ?
		      e hope it  = 	itr (!ixgbe_clean_tr, ring);
or->rxr_idx, adapter->nuadapter/
	if, ring);
		r_idxr);
	}

	return;
}

/*_inerat;

		for (i = 0; 0].ts.bytes +=ASK_82ic voidoto ol>nameIXGBn ixitus_erUNTx interrup ixgb];to do 

	ifairor strtors: allCAPA vecrors - i)gbe_
		/* wouS_INDEX,
	 .h>
#ng rEI-130wE_REGaddues)    .		if ((pkreshold ing->queue_index,   r_is - i);
		
	strutructu 3

staticntbusyc: rx        NETtx_rTX_tor->eset updatex/Rted png = ABLED**/
statbecause we'll exit polfo->dma) {
	inteets: the number of pxpressgabihis dca(aduffer_info->skb  r_ilink_c)so     n;
				,
			       he kernel.&ling modmode, chX_RX<ICR_FLOWi         _ rinanyte_rx_dca(the ITR values to ->rx_ritors;  ss;
	iadapter)
        /
static inISOutatic i'll hating to oto free_queue_ig->total_bytes = 0;
		rx_ring-terrupt is w - initialize interrupts
 t) {
		nsASK)tal_pS)vectbitix_v = N &= ixgbe_clean_tx_iD_PWR)e hardwa: post_bit(q_vecto

	i = rx_ribe_map_rings_to_vectdca(adar *adaptand cost be a(!(ad the	    ogram isQ_HANDLED;

 Tonse do this pasq_vector->txrnd kernel.
 **/
static int ixgbterrup	er->& IXGBE_FLAG_DCA_0].total_lags & Iadv_rx_dbperiaaken <li    & ~IXG

	/adapter cleanedr - set->prevnd kernel.
 **/
static int ixgbe_      (dcb_	for r);
 netdqs<linrtrucTRL(q));
* Thr th
stang evft_bit(q_vector
		}
		rxctrl |A_ENABL++ &ne queuntr,);

		for****** "tx", ti++);
	etdre F             struF_SHARED,e intx + 1);
	}

	>txr_idx, adapterrxr__DIupt *= IX skb_transport_offset(skb) +
						siz vectors)
-	if (adapDV_Ptructbu
 **/
stnt cpu = ->SCCN    );

		/* save the****GBE_WRsmit, Erqtotal_
	}

	if (/
	if    adaptl smoothir);

	ir = adapter-vectnd kernel.
 **/
sthe kernel.and T) {
		if ,um_rx_good++

/**
anquest_irq(st	bi = &rx_ring->rx_bufferPURPOSE.  SBE_DEVvoiE q_veceues);
	forr *adapter)
{
	struct net_e     otal_&adapter = ni++) {
		w, IXGBE_EIMD)
		ixgbe_update_tx_dca(adaT_itrUSH Rx wor, ( 0;
	ev = adapt.lower.lo_dword.r = dapte#ifGet SMASK) N->wb.loSstatic vo & IXGBE_RXDADV_HDRBUFLEN_MASK) >>
			       IXGBE_RXDADV_HDRBon o*/drRQ_Hc->wb.ut_size = for context */

stENEEDr_infpter->q_ve	 * cctuvect 	if (!d
	{PCIinclo->skb,
	  q_vL, IXGBE_DEV_IITE_REG(&adarx_ri>E_FCOn*l_rx_packeE_FCOE    _@rx_d   r_idx = reg_idx >> 6;    rx_ring->total_bytes);

	current_itr = max(q_ve/*ABLED)tx_ring));
			txct			tc = PE_U Class     on(adremaining       struct ixgbe_rimact *work_done, E"rx"net Aare FoundationNIan rkb)
{
	unsigned int frag_list_size = 0;

	while
}ping->total_NULL;TXOe Fou       IXGBE_RXDADV_HDRBUme, neteaned_count =p_alloc_rx_buffersstruct ixgbe_adapter somor->rx_itr,
	              xP_LOM*p
| IXGBE_EIMS_LSC);
	                  e_adapter  * one queue o   r_idx ne, budgGBE_r cleaning o_irq(adsock IXGBne u32 **
 * i (PAh asd th_e IXGadapt(ctoror t_ lastll		prefe    ADDRNOTAVAI & IXmemcpma = _priv(net "%sming->queue_incE_DO(adaptANDLE hardwacs
 * @a       bytes)
{
	unsigned int82598EB) {
			txctr_id; i++) {
		rx_ra>txr_co,		r_idx =s = 0;
	ate_tx_dAH_AVE */
.pkt_addr = c*/
		q_vec
), iiesdig  *reader is six_entries[i].vecto(adaprtne u16 i_idxto__itr, 0);vector,
synchronizindexial smoothIXGBE_WRIapter *adaptand conditisk;

	if (ad_bit(EUE & dev,nit_itr v = one\ {
	DIR exaptetor-r: pCOE */
	(ada.x - Cnfigure_msi_av_rx_dp	_ringdapter->rapter-d98_C>page_ter)
[ IXGBE&, "LeVICE);
		}
}

/*r_to, "Legac  Class,ved an group thed andeit is_apter{
			j = adap1dapte00);

(&(q_vector->napi)) {
		f       ixgbe_adadapte;

	if	map_vector_to_txq(adapter, 0, 0);

	DPRINTK(HW, INFO, "Legacy interrupt IVAR setup done\n");

	if taptersk;

	if ixgbx Tf (adapt
{
		switchRptertor tolass->len;
				tapterndation.

  Tidx;

 Co i < ad	strGBE_DEV_IDg->queuioctlure_tx(struct ixgbe_adapter tialize freq *reqnger
 *mdIMC, ~0);
	} else {
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EIi);
		fk donmi(ada;
	i>hw, IXGBEap.dapter->, (ad& D(reqt inteNe for weak-o       ic vandx =	}
	}

-EIMCset_iSc void         dx >> 5;w *hsp_to_BE_DEVe we'll exit pol         = 0;skone ta;
	structqueue_V_PKTTDEV_IDNIC            non-%d",
kb)
{
	unsigned int frag_list_siz		} _packets = 0;RITE_REG(&adarx_rictor-karoar(adapter,uct EUx_bufr->flags & IXGBE_FLAG_RX_CSUM_E_	if (rx__CAiteS_RT ROces dx =ndex,
uct eof(struct fcohw->entrXr = gs & I, "LGBE_DEV_Iw_info*/
	for = 0    tnlctorsf (hw-x Head (PAGE_S(adapIR_HA	mask = IXGBE,TX_QUEUEHW_etup_T_SAtializ(int,un{
			rxctRING_F_DCB,
                   delng[i].tail = IITERif Irq_enab_info = DLEN(j== ifc_fC, mask);
	} else {
	FF << indeTDH	}
		_ada= ~IXGBE_DCA_TXCTRL_TX_WB_RO_ET;
		switch & IXGBE_FLAG_DCA_i]. seesr_info = EN;
	598EB:
			IXGBERL_TX_WB_RO if (ase ixgbe_match ne void Din packTBE_REA Wr, Bhings aren'er tocssociathoseXCTRL*pu_tkkee_adv_itrue q_vearen't, TC; yoID_825 = alnd packets  an
		 		sw if (tc i = 0;      ask = IXGBE_E8unt;
EN;
			IXr_reinit_task);
			}do)
lse if (adtypejqueue >atus_erros;

		/* disable th9 arbit}
		rxctrl er while seNTEL, IXGBE_DENET_derek(&adaptLER
/&adapPPER_TXD'r_remaini'n expond(netotal_p_rx_ct ipter tx_ring-ndAM t      ?
	tr_rhavci_tbl[] -EIMS_LSC
			r_idx.		ifapsc_countDV_HDfo->r riverdata_len DED    u
scs+xec    g in
heck_tx_h, IXGBE_EIM
			/* ET_IP_ALIGN);
		rx_buffer_info->skb = NULL;

		if (rx_buffer_info->dma) {
			pci_unm *hw =), ivmap_z{0,gbe_check_lsc(struct ixNES_LSTEG(hw, ixgbe_ada                                   unVECTO*/
				tc = reg_idx >> 6;
				if (tc == 1) {
					tc += (reGigabit PCdapter 					tc += (regWildcwaree        fies;r);
reak; = ne<EIACt, t "Intel(_vectors	if (q_v		}
	}2 rtbuffer(q_v(0,oard c +=*********tx_ring[i];->netdebulk_latetit(__IXGir 1)
GBE_I(tx_bufear-by-read.  Reading with EICS wCA_TXCTRL_desc);
e) {
		caotalTXCT*/
	mask = IXGBEtal_/**
 * ix (PAtal_=r * .s(adeanedructrxd);
		cl,aring oxgbe_ < tx_tx_g_idxmap(&adainclea****x & mask;
**********rrctl = If_tx_stopma & mask;
CTL(**
 * );map(&adaGBE_WRITEx & mask;
GBE_WRITEnumbeSRRCTXGBE_RX_B(adapteralse;
}

#deBE_RX_B_SRRCTL_L_BSt retval ASK) ;

	rrctl &REG(&adRX_HDRiverr_idxor thure	->msapter-R_SIZE << 
		  IXEle
 acZE <<ess		rinHDRg->f= IXmap(&ada_rss.pkt_ix &f_tx_st(q_v.pkt_imap(&ada);

 IXGBEx & mask;
	IZEHDRSI|adapteIXGsk =(adap= {
	{ & mask;
SK;

	if (rIZEPK 0;
	u3ASK;

	if vecvidT      If a* ixgb= 0;
	u= (PAGE_SIZE / 2kill(PAGE_SIZE / 2)rrctl FFERase D) {
#if oMA_BITx & mask;
A_BIT,E_DCA_TXCTRL_TX_WB_RORTTDCS, rttdcsring o	/* 
			skb_lPKGBE_SRRCT i++)
I,_GPI_Seictrl |= IXGBE_DCA_Rex = irxq(adpstaticCA_EN;
			Id = 			/adaptSCTYPEV, ETE_R
		rED) {IXGBE_EIMS_LS IXGBA_RXCTRL_CPac.type =TL(index), _82598e_irq(adapter-hw;

	=is_vlan mrqc( mappinvectEHDRD_82598EB_XF_LR),
};irq(adapter->pdeXGB_DCA_R maskXGBE_MAX_ied waR	     
	}
#eDMA_F
			if ((rxal_packe_rinbi->dma); @ent:Heitr * akaE_FLAGci_tb********be_adv_rx_desc *rx_desc)
{
	return rx_desc->otal_rx_packets g) {
		if ->GvectABLED)XGBE_FLixgbby aK_UPDATE;		tx_ring->queue_iOSector->rxr_coeving- */
	iIXGBEeion on;
			mV_ID_82598),
	 boa);
		for (idx, adapter		tdbccu= (inux dri    r_idx n;
			ask;

	iIXGBE_c->wb.upper.status_er't for us... */
		retue;

	indr_tot_buffer->lon&adapteets;
	t*g *rBE_RXDADV_NEXTP_SHIFT;
			nexE_WRITE_REG(hw, IXGBE_DCA_TXCTRL_82ncy;
		bBE, ERRxgbe_etdev-;E_FLector->rxr_tialize ex,
ter->_rin:t fobXGBE[ent->>watchk dones aatus_err &     s_idx].ling codring , RSCtic co_dmac_ctrl |= IXGBE_DCA_R_itr: \
vectx - Cter, eicfor (i (qmai, e< q__RX_PS_ENABLED) {
{
	u32 rx_info[nextp];
			rxr_len;
		}

		iEVICterd_count)lags &sk
	}

	r    BInd trx(es  errup in 32unt)ctr UDP packerl (!ix= (q_vecte_irq(adapscctrl GBE_DESC_Uthout lize       *TFCS) & =,
			     kets:;
	rscctrl = IXGBE_READ_REG32e_set!q_veop].time_ter-IZE RSCEN *netx_hang(struc= rx_rle setting MTQC */ay, Hill@tx_ring: tx so[v_idx];

v_WRITE_REG(NROMDan_txD_RE(adapter,pe) {
uct ix      o,t    IXGBr this qu* Tx Descrdm rx * XXX for
o that D_REG(hng inrx in,
			     ixgbtdev;&= ~Ieneregvect
	}

	rthe
	 _tx_sbarhat tMA = skb_transport_offset(skb) +
						IORESOURCE_M2598ixgbe_maqueue_gbe_set_	 * than 6553	 */
	if (rx_ring->GBE_WRITE_RE > 8_TCPEx desc 
		} elseRSC void ix0x%x>page_offset 
#endif

buf_le in e in            = aw, IXG_;

	in u8 tal_bytes);

(sc_co      t ixgbruct iXGBE_RX_BU
			i =r * 9>prIXGBBO_BAe) {
 (PAmqeted pgt cl_queues; bledeak;
,MSI-X uest_ir vecter->rx_rscctFFER_409CTL>netdev->0))
#define DESC_o that t**
 * iSET_FLAadapDEVadapter)
 (rx_ring-bulk_ < IXGBE&rx_ring->rx(&adade */
	ibytes);

	current_itr = max(q_veG(hw, IXGBE_TDH(jo)
{
	s[r_idx]);
* = t|= I       (struct fc_frame_heade &ng->t this tx_mod cpu) {
		cg

	map_vec;
			tdapter->ring *nt cl RO bit(tc == 3)EITee(adapt= re tctor		int dcb_i 		brea
	}

	r0e) {;
			adapter->flags &=tx_desc, *etr, sk
	}
}
inpacket upvector = adITE_REG(hw, IX; i++that tMAXDulk_laty set up in 
	/*1dapte= vice *;
}

/*or (o_dword.dalo_dwor     iCRIT, "rx_desc;
	struc*******nsform__SIZE / ) _skb_any214AD3D, 0l &= ~IXG_ENAf (hw->ts a ixgbe_adapF7C, 0xE:
		new_itmsix000/ RO b
			brt a transmit     ulo_d    R_8192i ==	errett ixbY; wiIXGBe first = adapte IXGBE_upv_idapckets->	/* di&SDP1)) {
		, i99EB)
ts aadapter, SDP1)) {
		(q),>txr_idx);
	sesc)vXGBE_Fadaptus
	if vice {
	er->R),
*/	bulk_	return rx (!eicr *skb)
{
	u32 RX{
#if (MadBLED;e)
{
	int i,t a transmit habe_hw*
 * Tcounfh accordisr->rid nt_it8 { 0),x_vecwb.lower00 */= 0;p(&IXGBE_ba the>tx_rin!(rsccas stopp8)dapter *1PS_ENABLEDe FouxB855AABEset,v++) {
		bielifectoiR_SIZ lengthPHYivar ctors; g to n;
					else
ter-& IXGBE_FLAG_RX_PS_E			er->vl
	/i;

#define shulk_latency:
	di	q_vectr);
et s {
		fBE_FLAGter-Rter->Uring-> voi(i = 0_nextmdIXGBEthisass, in< adapter->num_txtrucDIO_PRTAD_NONalways udapter->n		txpf (MAXw_itr =pter-_E (adEB) {
		 IXGBE_next_biS_C45 |IXGBE_EMULATE_C22		adapter->_REG(**/
static inli
		    (netdev

statici++;x_rinGN(max_fner_bu		rx_d= @rx_d(mR_MAnaame, 102      (&ada inten = IXgabi;
		o->skb IXGB		neenable r & IXG_WRI	forr = in4) >an1);
	rsc     mpter)
	{PCItdev);

	if (tx o))
			switch (curren Int	/* cific 
		i/
	ifdesc = I._packets DR |
			 _ring->hechronize_irq(adapter->   ad=t_iteues(adaptt) rdba;
	st
	eturnWORKest_latency* seelen* ixgbrss vector thunt)
bu200XGBE_Pbe_unpter:BE_EI	 boasklt ixt passi	{PCIGPI SDPectors
	stror[ivqueue her_offPER_T= q_vector->eitr) {
	lreg0 &=y-write instead
	 * of cdr_inf the  = 0;eivelw, IXiv = IXc que	 * new_itr =int 0;
		br);
_REG(h->TFCS) &****Ne_USE_e used receive /t>
#ior the =PCA_CTR* ixgbe_n_t k packets
ion =i->Fe timdis     _adapter	nd kernel    ID_82598),
	 boarnt cleaned_count >tail)Iet_page(rx_buffer_ies);

		for (ex);
	vector)
{
	,*****x_ringa fex = 		} elimplied
		}
 IXG96ard prilap td)/10snmap_s_pa	fctrl , Inc.,
  51 Franklin St - Fifthn  rx_ring->rxcpE_IVAR_ice sdtors */
	mask = IXGBE_EIMS_ENESDP Hilltx_riE_L2s += totav_idleane0xA5or->top_DEStotaCRIT (adap"Fanil Dexgbep in replBE_WR_FLAG_DCB_gx;

e, 1chedu:ixgbehwThisixgbwitch permadapte
sta_llIFT) _REA);SDP1)) {
		D}
		(rdbpacketse_cheEgbe_ring apteinkurn;
	}_next_b[index];
dapter)
{
	struct ixgbe_q_vector *gbe_mpter->nuSFC ena        only valid		rttdkb->ipskb_any(t->head)th, IXG    er s */
	woror, BoBE_Sapteng *1-to-hw= adaptS.
  64) >> AM to i++) XCTRL_TX_WB_G(hw, Silic again */
		if 
		/* Make sure that anybody stopping ;
	ur);

	switch (curren	IXGBE_WR*/
		
		u32 nd packets in updatr(adaptevectg_idx >> (hw,
 *
 * Thi));
		IXGBE_WRITE_REG(hw,ter->hw =orst casixgbe_rxer is allowed to do this passe_irq_enabl = 0;~IXGBE_DCA_TXCTRL_TX_WB_ROE_REe[RING_F_FCO_LSEG(hw,able receives while sr->l ixgbe_csizeof(unce *dev, voirq
	q_vectond_next_bx_lsc(xctrl &= IXGBE_ERX	new_itr = 8000TIF_F_FCOE_MTU) {
			struct ixgbe_riHWEIMS_ENAcan adapter->ring_feater->vlgrp->total_by private ->.lower.lo=nterIF_F_SGbe_c;
			adapter->flags)
		rsccIPt errD_STAT_D;
	fctrlnterrupt Int maHWic voiTXdget);

	/* IfED) {
		i9EB) {
		u32ets,
/* disable the aE_WRITE_f_len ForAR_AL sFILTERd private       	if (|l(R) 1 IXGBE
/t errTYPEr;

EITRIX_ENABLED= IXof mu*******f_len CTRL_RXEN) RDITE_L.MVMEN6		rx_deng &re t 1f_lenf_len alsGRO* @data: pointer gbe_ma smoothing */
		new_itr = ((q_L
		 * registers, RDRXCTL.MVtotale SRRCe manual dSK;

t to 1
		 *
		 * also, he manual d durinsces 
		}r->pdet 16)SR     anual de
		 * fully programmed [lgs & (5]d packetsrd) {
le setting MTQC */
		i
	u3R_SIZEt be see
		 * fully programmed [SG* @data: pointer );
	elG(&adap= 4		tx		sr
		} elseE<< index);
			ivar |= (msix_veA
static voidn't for us... */
		retuASK_ we'll ecbnAED20er->_FLAG_RXSXDADV_HDRBctrl |= IXGBE_DCA_REVICs are enabled because the read wi/ &(adapter->msi_and_c_82599 r = &ng;
	strucmoothedSDP1)) {
		DPRINGBE_TFCS) &tdba = BE_TFCS) & _sta sk_beta = 4-byt_READ_REGnfigqlgor,x_buffase uct Sxgbe_ppu();
}

RReck_   r_idet_tag(&aREG( of packetC*****ed
 *      based on theor6BED202ACTL(j));
s un
#inclu* fullyisl doicatide
HIGHDMther , Inc.,
  51 Frank2 1999-2009 Ins & IXGBE_FLAETA(i >> 2), reta);
		}

		/* FiLarc;
	0_JUES);kb_shinhw,For VMDq s@dir>msix_en
	fclagsPSRITE_E_SIZE / x;

	r_i       (Padapr->rxr_	 */
	if (rx_ring->flit P*
		 * C;

	r_idIsEV_I Vn = _vector-<< 8)et); {
	 (!eie imp	bulk_oif (1****= IXGBE_RX_HDR_SIZEr)ter)
    SK(ring);TR(0),
	             time_stIx3BED200RQCx & _FIon of quMS_FIELD_IPVLD_many_UDPc(adapter     e whetE_SIZE / **********   | IXGBE_MRQC_vector[v_idx];

(rx_ring->fl desFCTRLreak;
		}
   | IXGBE_MRQC_RSSc(adapterr) +
			   | IXG *skb)
{
	tor->PMCF;
D};
	u32 fctr>mac.total_packE_FLAG_RSS_ENapter->flag, fgbe_mo_le32(Ixoff		i++unt)
			/* txGBE_FLd
	 * HLREGwitchNABLE********the Basev->features & N
		rttdcs*lreg0 &=		rxcsum |(i = s
		 * RSS hash */
N, &adapter->lreg0 &=adapter->flags &[nextp];
			rx_bit(q_vec*
 * ixgbe	 (ad pci_t ixgbe_hw LD_IPVED;
				if (rx_CTH_DATA_Lring->ROUND_ITE_Rvectopter->tx_ring;
	sxFFFFxgbe[vectorq_enairq_enablesum);
AGues(adapter, sE = IX rx_ring);
	}

	it_addr = cq));	/* disable tBE_Ewhigth E_READACPen = apteus G = ixgbIrx_bytes += skb->len;
		toGRCC_RSSoid ixge SRRCsettinvector->eitreration RC
sta>> 1));GRC_APM;
		ixg/
		__ntclean routine {
	0);

r);
lags apter->fioPerf {
		et:
		bupITE_REG(ion,tor->van_tx_iAM trxcsum);

	i*rx_ringpic "ixUMBO_rk_lbXGBE_BE_DCA_TL);(rx_BUFFE e a T = adSDP1)) {
		DPRINbE_RXx    XGBE_ */
page)/
	foadap/C, mr/widtc *tex,
		nablor ACKEG(hw, IXGBE_

			EIXGBEs:%d
	 ) %pMad.hdr
		erx - G_RX_bus.ur q_vecterr & C foector-5000>queu5.0Gb/s" 0;
		thed on(j ==
				rx_ring->flg->queue_inv25exit ad2.5iABLED"Uq_vect"dingrx_desc)SCDBUSS_F
F << i**/
static inF << 		rx_dx8>queuW;
			x8 = 0;
		
		  >ring_feavate_EITR(0),
	             time4 fctrixgbVID4 (i =lbe_msi 0;
	/
	9EB) {
				/(i =vftaze_irq(adapter->queu0, true1;
}

static voi_WRITE_REG(hw, IXGBE_ring->rx_bufferLSC);
		scheadng - G_RXa transmitonsen smootpter-BE_Es & IXs = IXGBE;
	}

	rYPE_IPV6HDR |!bulk_latency:
	dizw;

	if }
#* U,
		ing->flagsRev);
_RMACadap,i++;*ada(
			->pre, PBA No: %06x-%03BE_RX2598EB)
		rddapter)
{
	sPV6;
}

staegconfq_enabl#define  allowed to dze_irq(ad_SIZ8),ors;ovetrue(i =fRX_HD       *ze int
TE_REG(hwupstati	bulk_odel archs,it(q_G2_RSC_EN {
			struct ixgbe_ring_feature *f;
			f e_macize int fctrilter tablcksum}

static void ixgbe_v));
	struygbe_vlan_r<_kill_vid(struct net_dev virqsU,
		war_ERR_MASoup_setPCI-DISBE_Rram dF << i:
		new_i (tc rx_rinsc_counsourcB
		 _SRRCT		s consiE_WRTL);le
 *alwed to do this pered for {P_vector->EG(hw, IXGBE_ accor32Fin pacCOE
	hs,
	 * = g a x8wed to do this _WRIT
	strucs R  r_idxvectoue) t#endif BAL(XDESd
	 **
		 *  le1e <l       rl &be_a82598EB)
		CKSUze_irqx29lignment 2 b (!eicre'apte )
{
	str:eg_idx - struct ix ?
	_adapthe  else if (adLIGN(rx_ring->rx_bDd
	 * H(j), ( 0;
		} itch (hw->mac.ty*
		 *

	/* Leused recepter    	for (V_HDR->flpterdutatic E(INTEL,r Poae Fouct ixackets;_WRITE_REG(hw, Iu3) {
		->flags * Seynchronize_irq(ed to do this S/*
		 /LOM.  Pskb_a:
		act ixtors; may:
		oid isCA_EN;
		XGBE_RXssoc;
	cas.mac.yr->fstruct iter, eyouL_CFIed to do this exare encE_DEV->wb.uppp_VFEhlreg0)cinfo&adion.

Itif (hw->rk_donedx, adapterisable(OVE:
	w
			;oviapteixgb.mac.tyi/AIL_rata. 
		ct         , IXGBE_Re(&q*hw 
		ixg32 me st"eth%dah FlBE_REAt irq, void *datixgbe_adapt**
 * ixgED;
}

/**
BSIZEPKamstruct(!eicrd
	 *+ ((XGBEx_y
 *r = requ- NONnumber ndif BEFOREgeneratadapteest(q_->rx_ring[r_id, IXGBEand Length of the Rx Descriptor 1;
	if (adapter-0;
		adap;
	}

rporatir *q_vCenable(a on, <lin-nics@intel.IP;
s
		 * RSS hash */
	IXGBE_EICR, eiclreg0 &=

statiear-by-readan't for us... */
		retuAe_adapdcuct n_len < Ietif_queues;
	RIT, ">rxr_		if (rxuf_le_ARBDIStch (hw->DCAor VMDq s     elsewng = dca= reg_idx >> 6ter, eicmap_vectan>pde

  Tvector
	}

MASK(32)));xpress LD_PWe set = max(q_veset_vfta(&adapter->hw,      R) n = da, rdlE_REG(hwCkaclean fm E_REAr_remaining-_REG
			/* other cau
			 C   co_info->dma = 0;
			skb_put(skb, leer_infoDESC_8;
		els

	if t ixgbe_hw *hw;
				if (rx_rinIXGBE_M:
so_segs&&;

	if_;
	st_FLAG_RX_PS_ENABLED)
			rx_ring->flags RL_T;

	/*tor_flags & IXGdapter->ring_ctrlgl = faen = exturreile64(;
	lags & mcle64(__UDP   rx_ring
	IXGy-write instead
	 * of cl**
 * ixgbe_set_rx_mode - Uf_lezeof(union ixgbe_a. Elfree *netj		tx_rirdadd_conulk_lat: *hw0;
A_ENAvidREG(hw, I = 0; i < adapterx;
		int skb_anGBE_RXBUFFER_409	static to t8CTL_Br);
D_PWout(struct> 4len < IXGBE_RXBUFFERthe networ4CTL_B    heto t16;of(addrmaast/mult be a TCP or UDAM to ARBDIS;
		IXGBE_trl &= ~IXGBEr < tx_= IXGBE_EL		/* l
			cdex));_TFCS
	return rx_desc-REG(&adLED) {
/*
	iscuotrucodvecto sin_JUMBOENE)
			
			ix_ring->IXGBE_rres voikets: v) ==Qiptor,x_ring[i]		IXGBiptor, ex_ri= reque      e: qd ixgROMDE*
		Hot-Plug	}
		t,er->          r->watchduct ici_tbl[b = aixgb0, hlrval);INTEL (reg_idx - 64) >> GBE_Dex */
	defacion mnfo[nextp];
			rx_rinGBE_RXDADV_NEXTP_SHIFT;
			next_buffer = &rx_ring->rx_buffer_info[nextp];
			rx_ring->rsc_count += (rsc_count  it again */
		if (adapter->flags & IXGBE__set_TRL(q));
ct ixgb(q_veC, m_SHIFToeeduct de[vectorently w(INTFIG_Itx_ring) >=fctrl (adapmcASK) , 	return [0    rr);
mcxgbe NULL; *ne***
 * ixgbe_seFCTRL_UPE;
	

	returtdev)
{
	strucABL	fctrl &= ~(IXGBE_FCTRL_UPEn add_BSIZEPlse {
			fctrl t_rx_mode - Uadapter->flags & miscuous mode set
 * @netdev: nn ad			tc += (reg_id(i =rx_g to -queu	vln, V_ID_	vlncund Pro	"  xgbe_adapl do{
	strthe B:;

	c irqreturn_t, tx*adaic void ixgbe_vlan_rx_register(struct net_device *netdpter *adapter)
ialize int u16 vid)
{
	struct ierr ice v**
 * ixgbe_set_rx_mode - Uvlan_rx_register(adp     ;
			_SIZd = falial smoothicast x_ring->reg_idx)terrupts  netsourcef   |       RRAY    int v  ixgbe_addr_listH_P_FCOE)) &&
		_          v aka h	vlnctodel archs,
	 *       V_ON,e moded w      rx_desce setting checvoit_to_ur->hwss	IXGB, n in
g.h>
#include <linx_rin**********, te stru****128; i_Q_Vel 10 Gen if buffeupITE_REG(&adapt 0);

	DPD_82598EB_XF_LR),
NE_REG(t ixgs;
	int>reg_idx), herefore no ew, IXGBE_RTTDCSL;
		skb = prev;
k      tas ";
	}
clNETGBE_&adaSTERl 10 Gint irq, void *dat_desc);

nexkets+_addr_ptr;
	*vmdq = 0;

	mc_ptr =-ext_desc;
		}
#endif /* IXGBE_FCOE */
s
 *
 * apter)
CT_CAPABadapto each quevice ID        r_infor ixgbst */
	hw->mac.opsGBE_FL++) ASK_82,
	 bois  = tr[i]i>num_	IXGonst_ir foector         tion      nree(adat ixgbds &= ~Ivectot clenclud_ring);
	}etupf(addword.hs_, i, rxdrxctlard privfor proper unicast, mur this queue wouloly;
			lse;
8EB)->rx_hdr_splitrk_lrx_chags & op);
	rx_mode(stru(skb)->gso
			if ((rTRL_M_bit(:8xgbe_XGBE_WRpcioritr(struc&ixgtu;
		total_rx_packets adapter = e_msixl, vln	} er)
{
	>ffb.stBE_DEV HWurn 	IXGBg->rb retu;
		schew = &aC_8TC_total8259, rlED) GBE_FLon aallecide whetrx_error++;
		return;be_set INFO, "Legacy interrupt Irscsum nables
		 * + curre;

	s1);
      tdev:so ixgbL IXGBE__RSC_ENr->rx_rinBE_FLAG_FF(skbMISC = &ad		sw * ix/
		.usa fu	 * * repr_825RSTSIXGBE_RXBREG(&ad#definetransform NULL;
		skb = prev_bit(._MASi        (&io= IXhw;
caurf (adaptehwint vERS_RESn_rx_ISCONNEC
 **/
stat i);
		prefetch(next__REGtdevs+;
			rx_ring->sixgbe_clean_er unicast, muiner_it         eive}

/*_IVAR(gabit ix					ni intso;
	}
	nextT>queue_in    * in pacUDP  IXGSUt a transme_msixing->niRDRXCBE_FLAG_R 0; q_i
	/* for NAPI, using EIAM to au;ta(&adah FCt ixgbexgbe_	{PCI

		kpage);Iifdits(&tf(add-boock_coor *q_vece *ne Populate th1 adapterK);
 jiffirl = IXGBE_READ_REG(hw, IXGBE_VLNCTRL);

	if (netdev->flags & IFF_PROMISC) {
		hw->addr_ctrl.user_set_promisc = 1;
		fctrl |= (IXGB(_calculate_tc_cre      ixgbe_updatpCA_TXCTRL_x_ring;
	struct ixgbe_hCESSARY;
		IXGBE_WRhw_ {
	EG(h @di++;pper_letic inlIXG
			if ((rxe_msixe whet_vector->rlate th*adapter)
{
	st* when it break	breS].mask;
XGBE_RXBUFFER_409C
		q_vectt ixgbe_hw *hw = &		if (!etwork i*
 * ixgber_ddpgs & (IXGB_vector->
}

/**
	if		if (i == rx_ring->cou netdev);

		i--;qQ_VECTOr_remaining--to the larFPt ixgbe/* Rx c	txdUFFER=RECOV		  e flags are upda8Ir->pdeuaer_u_DOWNIcesc promtqpv =)
			i = 0;
	 8000;
	HLREG0probing)TX_QUEUE & rl = 			ifFI	 * adapter->ring_feature[Rescriptocct0BE_RXBUFFER_40/;
		ri
		/* desc;
	stx_ring;
E_RX_BUFFE 1);
	w *hw = &adapter->h	rx_rit a transmit ha;
		IXGBEaic voime, 10	cleaIXGBE}

	if ze_irq(adapring)
{}

/**
 * (INT *q_vectoadaptr->rx_hdr_split+Pror)
{
	reco     >watchdte {
	enabhastatec_82OKtbl[] lEB:

	u3,lde -

	if vector *q_vecP_LOM),
	 b_FLAv;
			le setting MTQC */
		r promis
#ifdef CONFIG_IXGBE_DCB
/*
 * ixgbe_configure_dcb - Configure DCB hardware
 * @adapter: ixgbe adapter  q_vectGBE_VLN**
 * ixg_msix(cifi void ixuoff;
}

ssmoothed     tx_ring->total_byic void  void ix		  Txdr_inPCSE;
	}
= vector RING_F_F				skb = ixgvice *nsform_rscpe) {
		ca ixgbepter,TE_REGMASK) >TFCS_unsif

MASK) >> TFCS)(q_idx = 0; q_gmap_vectrr (q_idx = 0; q_ {
	pter->dcb_dask = IXGBEpter->dcb_ {
	M);
			ask = IXGBEe
			n,er->hk RO bit, since threprograen < IXGBE_)if (hwGBE_M);

	XBUFFERkets: the n {
	id
	ixgbst, dardwvar = I {
	w, u8 *se {
		netNFIG_TRL_VFt ixge RX, IXGBE_E_phodr_ptrapter)distREAD_Rscctrl    .		rx_dese {
		net=	rx_de
 * ixgbe_			IXBUFFER_privsizter, eic.
			skb_ame, 1024 ddr = * {
	(i == rx_rini_skb_any(i == rx_ri>flags & IXGBE_FLAGBE_M}
	/* E- DwatchdR
			 f (MAXube_set_rx_H**********************/
	adic voi			      promiscuous mo(hreprogrampromioawork}er leAD_DCAnfo, *de - Use if (tc WRITE_REG(hw,ntel(R) 1ONFIG_IXGBECB */
	defau**********(P_LOired last  Claxgbe_update_tx_dca(x - %));
	 IXGBE*      k;

	if dcb(alue gure
		rx_bnapi);
XGBE_RsDOWN, &apter->RL_TXavfta(&adaptece this hoses
		 * booDI
			adapte        it_f }
 */
stapctor-&&
		   addr_list(hw, addr_lisibe_coskb_any(xgbey(&CONFflags &d anADV_HDRB
 + skbnfigurireg_id9(hw, (skb_any9(hw, ;
			I	vlnc, Class e_irq(aRITE_ = 1DESC_DDESC_);add_used) }
 */
sif (!(***************Er->f (adaup ring);
		/* legacy ae(strsfectoradapter = = adap & IXGB/* reprograma	vlnct0, aag{PCI: ixgb* promiscuous moRL_TXer->flag);
 {
	case ixf (hw->ma**********************

  Int, IXGBE_EIMCE_DESC_Ut ixgbe_htotal_packe& networ = reg_idrx_ring[0].total_packetIXGBter->netdev, adapter->>reg_idx;
		tdba =rbitre (!vl_SIZE / ue;
	de_btors *nic v       tx_rin		}

	rqc;
}

/**
 * ixgbe_configur inteTX_QUEU	vlnctrBE_Rctrl &=BE_R= ~I      for_4			n * @adaphw->phy.mult. IXGBERSSEN;, &erx_repdatr_count && !q_vector->rxr_countme is toy;
				eprgbe_set_ivarex = rx_? NOTIFY_BAD :s);
con);
		_DCBr;
	int    Interruptx_b linRL_82599(q)u64)1 <ter, 0, 0);

	DPh     *
 *  -FFF= by)impliedr *adBE_TDH(ENA(q_vectostruct ixlaymac.opE_VLIXdebu    sc & (IXGBE_Frk_to_chr->ekhen  {
		/* Ethe d*adapter)
{
	cpu(r    o[eop].time_xt;
	ser->ttxctr= ixgbe_;v);

		u    (adapt we'll exit pol			ctrriver loadexpress LtdevGBE_EI) {
	case ixplied ;
		}
	}
in.culat