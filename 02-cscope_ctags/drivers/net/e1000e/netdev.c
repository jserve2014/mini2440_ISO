/*******************************************************************************

  Intel PRO/1000 Linux driver
  Copyright(c) 1999 - 2008 Intel Corporation.

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
  Linux NICS <linux.nics@intel.com>
  e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/

#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/vmalloc.h>
#include <linux/pagemap.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/tcp.h>
#include <linux/ipv6.h>
#include <net/checksum.h>
#include <net/ip6_checksum.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <linux/if_vlan.h>
#include <linux/cpu.h>
#include <linux/smp.h>
#include <linux/pm_qos_params.h>
#include <linux/aer.h>

#include "e1000.h"

#define DRV_VERSION "1.0.2-k2"
char e1000e_driver_name[] = "e1000e";
const char e1000e_driver_version[] = DRV_VERSION;

static const struct e1000_info *e1000_info_tbl[] = {
	[board_82571]		= &e1000_82571_info,
	[board_82572]		= &e1000_82572_info,
	[board_82573]		= &e1000_82573_info,
	[board_82574]		= &e1000_82574_info,
	[board_82583]		= &e1000_82583_info,
	[board_80003es2lan]	= &e1000_es2_info,
	[board_ich8lan]		= &e1000_ich8_info,
	[board_ich9lan]		= &e1000_ich9_info,
	[board_ich10lan]	= &e1000_ich10_info,
	[board_pchlan]		= &e1000_pch_info,
};

#ifdef DEBUG
/**
 * e1000_get_hw_dev_name - return device name string
 * used by hardware layer to print debugging information
 **/
char *e1000e_get_hw_dev_name(struct e1000_hw *hw)
{
	return hw->adapter->netdev->name;
}
#endif

/**
 * e1000_desc_unused - calculate if we have unused descriptors
 **/
static int e1000_desc_unused(struct e1000_ring *ring)
{
	if (ring->next_to_clean > ring->next_to_use)
		return ring->next_to_clean - ring->next_to_use - 1;

	return ring->count + ring->next_to_clean - ring->next_to_use - 1;
}

/**
 * e1000_receive_skb - helper function to handle Rx indications
 * @adapter: board private structure
 * @status: descriptor status field as written by hardware
 * @vlan: descriptor vlan field as written by hardware (no le/be conversion)
 * @skb: pointer to sk_buff to be indicated to stack
 **/
static void e1000_receive_skb(struct e1000_adapter *adapter,
			      struct net_device *netdev,
			      struct sk_buff *skb,
			      u8 status, __le16 vlan)
{
	skb->protocol = eth_type_trans(skb, netdev);

	if (adapter->vlgrp && (status & E1000_RXD_STAT_VP))
		vlan_gro_receive(&adapter->napi, adapter->vlgrp,
				 le16_to_cpu(vlan), skb);
	else
		napi_gro_receive(&adapter->napi, skb);
}

/**
 * e1000_rx_checksum - Receive Checksum Offload for 82543
 * @adapter:     board private structure
 * @status_err:  receive descriptor status and error fields
 * @csum:	receive descriptor csum field
 * @sk_buff:     socket buffer with received data
 **/
static void e1000_rx_checksum(struct e1000_adapter *adapter, u32 status_err,
			      u32 csum, struct sk_buff *skb)
{
	u16 status = (u16)status_err;
	u8 errors = (u8)(status_err >> 24);
	skb->ip_summed = CHECKSUM_NONE;

	/* Ignore Checksum bit is set */
	if (status & E1000_RXD_STAT_IXSM)
		return;
	/* TCP/UDP checksum error bit is set */
	if (errors & E1000_RXD_ERR_TCPE) {
		/* let the stack verify checksum errors */
		adapter->hw_csum_err++;
		return;
	}

	/* TCP/UDP Checksum has not been calculated */
	if (!(status & (E1000_RXD_STAT_TCPCS | E1000_RXD_STAT_UDPCS)))
		return;

	/* It must be a TCP or UDP packet with a valid checksum */
	if (status & E1000_RXD_STAT_TCPCS) {
		/* TCP checksum is good */
		skb->ip_summed = CHECKSUM_UNNECESSARY;
	} else {
		/*
		 * IP fragment with UDP payload
		 * Hardware complements the payload checksum, so we undo it
		 * and then put the value in host order for further stack use.
		 */
		__sum16 sum = (__force __sum16)htons(csum);
		skb->csum = csum_unfold(~sum);
		skb->ip_summed = CHECKSUM_COMPLETE;
	}
	adapter->hw_csum_good++;
}

/**
 * e1000_alloc_rx_buffers - Replace used receive buffers; legacy & extended
 * @adapter: address of board private structure
 **/
static void e1000_alloc_rx_buffers(struct e1000_adapter *adapter,
				   int cleaned_count)
{
	struct net_device *netdev = adapter->netdev;
	struct pci_dev *pdev = adapter->pdev;
	struct e1000_ring *rx_ring = adapter->rx_ring;
	struct e1000_rx_desc *rx_desc;
	struct e1000_buffer *buffer_info;
	struct sk_buff *skb;
	unsigned int i;
	unsigned int bufsz = adapter->rx_buffer_len + NET_IP_ALIGN;

	i = rx_ring->next_to_use;
	buffer_info = &rx_ring->buffer_info[i];

	while (cleaned_count--) {
		skb = buffer_info->skb;
		if (skb) {
			skb_trim(skb, 0);
			goto map_skb;
		}

		skb = netdev_alloc_skb(netdev, bufsz);
		if (!skb) {
			/* Better luck next round */
			adapter->alloc_rx_buff_failed++;
			break;
		}

		/*
		 * Make buffer alignment 2 beyond a 16 byte boundary
		 * this will result in a 16 byte aligned IP header after
		 * the 14 byte MAC header is removed
		 */
		skb_reserve(skb, NET_IP_ALIGN);

		buffer_info->skb = skb;
map_skb:
		buffer_info->dma = pci_map_single(pdev, skb->data,
						  adapter->rx_buffer_len,
						  PCI_DMA_FROMDEVICE);
		if (pci_dma_mapping_error(pdev, buffer_info->dma)) {
			dev_err(&pdev->dev, "RX DMA map failed\n");
			adapter->rx_dma_failed++;
			break;
		}

		rx_desc = E1000_RX_DESC(*rx_ring, i);
		rx_desc->buffer_addr = cpu_to_le64(buffer_info->dma);

		i++;
		if (i == rx_ring->count)
			i = 0;
		buffer_info = &rx_ring->buffer_info[i];
	}

	if (rx_ring->next_to_use != i) {
		rx_ring->next_to_use = i;
		if (i-- == 0)
			i = (rx_ring->count - 1);

		/*
		 * Force memory writes to complete before letting h/w
		 * know there are new descriptors to fetch.  (Only
		 * applicable for weak-ordered memory model archs,
		 * such as IA-64).
		 */
		wmb();
		writel(i, adapter->hw.hw_addr + rx_ring->tail);
	}
}

/**
 * e1000_alloc_rx_buffers_ps - Replace used receive buffers; packet split
 * @adapter: address of board private structure
 **/
static void e1000_alloc_rx_buffers_ps(struct e1000_adapter *adapter,
				      int cleaned_count)
{
	struct net_device *netdev = adapter->netdev;
	struct pci_dev *pdev = adapter->pdev;
	union e1000_rx_desc_packet_split *rx_desc;
	struct e1000_ring *rx_ring = adapter->rx_ring;
	struct e1000_buffer *buffer_info;
	struct e1000_ps_page *ps_page;
	struct sk_buff *skb;
	unsigned int i, j;

	i = rx_ring->next_to_use;
	buffer_info = &rx_ring->buffer_info[i];

	while (cleaned_count--) {
		rx_desc = E1000_RX_DESC_PS(*rx_ring, i);

		for (j = 0; j < PS_PAGE_BUFFERS; j++) {
			ps_page = &buffer_info->ps_pages[j];
			if (j >= adapter->rx_ps_pages) {
				/* all unused desc entries get hw null ptr */
				rx_desc->read.buffer_addr[j+1] = ~cpu_to_le64(0);
				continue;
			}
			if (!ps_page->page) {
				ps_page->page = alloc_page(GFP_ATOMIC);
				if (!ps_page->page) {
					adapter->alloc_rx_buff_failed++;
					goto no_buffers;
				}
				ps_page->dma = pci_map_page(pdev,
						   ps_page->page,
						   0, PAGE_SIZE,
						   PCI_DMA_FROMDEVICE);
				if (pci_dma_mapping_error(pdev, ps_page->dma)) {
					dev_err(&adapter->pdev->dev,
					  "RX DMA page map failed\n");
					adapter->rx_dma_failed++;
					goto no_buffers;
				}
			}
			/*
			 * Refresh the desc even if buffer_addrs
			 * didn't change because each write-back
			 * erases this info.
			 */
			rx_desc->read.buffer_addr[j+1] =
			     cpu_to_le64(ps_page->dma);
		}

		skb = netdev_alloc_skb(netdev,
				       adapter->rx_ps_bsize0 + NET_IP_ALIGN);

		if (!skb) {
			adapter->alloc_rx_buff_failed++;
			break;
		}

		/*
		 * Make buffer alignment 2 beyond a 16 byte boundary
		 * this will result in a 16 byte aligned IP header after
		 * the 14 byte MAC header is removed
		 */
		skb_reserve(skb, NET_IP_ALIGN);

		buffer_info->skb = skb;
		buffer_info->dma = pci_map_single(pdev, skb->data,
						  adapter->rx_ps_bsize0,
						  PCI_DMA_FROMDEVICE);
		if (pci_dma_mapping_error(pdev, buffer_info->dma)) {
			dev_err(&pdev->dev, "RX DMA map failed\n");
			adapter->rx_dma_failed++;
			/* cleanup skb */
			dev_kfree_skb_any(skb);
			buffer_info->skb = NULL;
			break;
		}

		rx_desc->read.buffer_addr[0] = cpu_to_le64(buffer_info->dma);

		i++;
		if (i == rx_ring->count)
			i = 0;
		buffer_info = &rx_ring->buffer_info[i];
	}

no_buffers:
	if (rx_ring->next_to_use != i) {
		rx_ring->next_to_use = i;

		if (!(i--))
			i = (rx_ring->count - 1);

		/*
		 * Force memory writes to complete before letting h/w
		 * know there are new descriptors to fetch.  (Only
		 * applicable for weak-ordered memory model archs,
		 * such as IA-64).
		 */
		wmb();
		/*
		 * Hardware increments by 16 bytes, but packet split
		 * descriptors are 32 bytes...so we increment tail
		 * twice as much.
		 */
		writel(i<<1, adapter->hw.hw_addr + rx_ring->tail);
	}
}

/**
 * e1000_alloc_jumbo_rx_buffers - Replace used jumbo receive buffers
 * @adapter: address of board private structure
 * @cleaned_count: number of buffers to allocate this pass
 **/

static void e1000_alloc_jumbo_rx_buffers(struct e1000_adapter *adapter,
                                         int cleaned_count)
{
	struct net_device *netdev = adapter->netdev;
	struct pci_dev *pdev = adapter->pdev;
	struct e1000_rx_desc *rx_desc;
	struct e1000_ring *rx_ring = adapter->rx_ring;
	struct e1000_buffer *buffer_info;
	struct sk_buff *skb;
	unsigned int i;
	unsigned int bufsz = 256 -
	                     16 /* for skb_reserve */ -
	                     NET_IP_ALIGN;

	i = rx_ring->next_to_use;
	buffer_info = &rx_ring->buffer_info[i];

	while (cleaned_count--) {
		skb = buffer_info->skb;
		if (skb) {
			skb_trim(skb, 0);
			goto check_page;
		}

		skb = netdev_alloc_skb(netdev, bufsz);
		if (unlikely(!skb)) {
			/* Better luck next round */
			adapter->alloc_rx_buff_failed++;
			break;
		}

		/* Make buffer alignment 2 beyond a 16 byte boundary
		 * this will result in a 16 byte aligned IP header after
		 * the 14 byte MAC header is removed
		 */
		skb_reserve(skb, NET_IP_ALIGN);

		buffer_info->skb = skb;
check_page:
		/* allocate a new page if necessary */
		if (!buffer_info->page) {
			buffer_info->page = alloc_page(GFP_ATOMIC);
			if (unlikely(!buffer_info->page)) {
				adapter->alloc_rx_buff_failed++;
				break;
			}
		}

		if (!buffer_info->dma)
			buffer_info->dma = pci_map_page(pdev,
			                                buffer_info->page, 0,
			                                PAGE_SIZE,
			                                PCI_DMA_FROMDEVICE);

		rx_desc = E1000_RX_DESC(*rx_ring, i);
		rx_desc->buffer_addr = cpu_to_le64(buffer_info->dma);

		if (unlikely(++i == rx_ring->count))
			i = 0;
		buffer_info = &rx_ring->buffer_info[i];
	}

	if (likely(rx_ring->next_to_use != i)) {
		rx_ring->next_to_use = i;
		if (unlikely(i-- == 0))
			i = (rx_ring->count - 1);

		/* Force memory writes to complete before letting h/w
		 * know there are new descriptors to fetch.  (Only
		 * applicable for weak-ordered memory model archs,
		 * such as IA-64). */
		wmb();
		writel(i, adapter->hw.hw_addr + rx_ring->tail);
	}
}

/**
 * e1000_clean_rx_irq - Send received data up the network stack; legacy
 * @adapter: board private structure
 *
 * the return value indicates whether actual cleaning was done, there
 * is no guarantee that everything was cleaned
 **/
static bool e1000_clean_rx_irq(struct e1000_adapter *adapter,
			       int *work_done, int work_to_do)
{
	struct net_device *netdev = adapter->netdev;
	struct pci_dev *pdev = adapter->pdev;
	struct e1000_ring *rx_ring = adapter->rx_ring;
	struct e1000_rx_desc *rx_desc, *next_rxd;
	struct e1000_buffer *buffer_info, *next_buffer;
	u32 length;
	unsigned int i;
	int cleaned_count = 0;
	bool cleaned = 0;
	unsigned int total_rx_bytes = 0, total_rx_packets = 0;

	i = rx_ring->next_to_clean;
	rx_desc = E1000_RX_DESC(*rx_ring, i);
	buffer_info = &rx_ring->buffer_info[i];

	while (rx_desc->status & E1000_RXD_STAT_DD) {
		struct sk_buff *skb;
		u8 status;

		if (*work_done >= work_to_do)
			break;
		(*work_done)++;

		status = rx_desc->status;
		skb = buffer_info->skb;
		buffer_info->skb = NULL;

		prefetch(skb->data - NET_IP_ALIGN);

		i++;
		if (i == rx_ring->count)
			i = 0;
		next_rxd = E1000_RX_DESC(*rx_ring, i);
		prefetch(next_rxd);

		next_buffer = &rx_ring->buffer_info[i];

		cleaned = 1;
		cleaned_count++;
		pci_unmap_single(pdev,
				 buffer_info->dma,
				 adapter->rx_buffer_len,
				 PCI_DMA_FROMDEVICE);
		buffer_info->dma = 0;

		length = le16_to_cpu(rx_desc->length);

		/* !EOP means multiple descriptors were used to store a single
		 * packet, also make sure the frame isn't just CRC only */
		if (!(status & E1000_RXD_STAT_EOP) || (length <= 4)) {
			/* All receives must fit into a single buffer */
			e_dbg("%s: Receive packet consumed multiple buffers\n",
			      netdev->name);
			/* recycle */
			buffer_info->skb = skb;
			goto next_desc;
		}

		if (rx_desc->errors & E1000_RXD_ERR_FRAME_ERR_MASK) {
			/* recycle */
			buffer_info->skb = skb;
			goto next_desc;
		}

		/* adjust length to remove Ethernet CRC */
		if (!(adapter->flags2 & FLAG2_CRC_STRIPPING))
			length -= 4;

		total_rx_bytes += length;
		total_rx_packets++;

		/*
		 * code added for copybreak, this should improve
		 * performance for small packets with large amounts
		 * of reassembly being done in the stack
		 */
		if (length < copybreak) {
			struct sk_buff *new_skb =
			    netdev_alloc_skb(netdev, length + NET_IP_ALIGN);
			if (new_skb) {
				skb_reserve(new_skb, NET_IP_ALIGN);
				skb_copy_to_linear_data_offset(new_skb,
							       -NET_IP_ALIGN,
							       (skb->data -
								NET_IP_ALIGN),
							       (length +
								NET_IP_ALIGN));
				/* save the skb in buffer_info as good */
				buffer_info->skb = skb;
				skb = new_skb;
			}
			/* else just continue with the old one */
		}
		/* end copybreak code */
		skb_put(skb, length);

		/* Receive Checksum Offload */
		e1000_rx_checksum(adapter,
				  (u32)(status) |
				  ((u32)(rx_desc->errors) << 24),
				  le16_to_cpu(rx_desc->csum), skb);

		e1000_receive_skb(adapter, netdev, skb,status,rx_desc->special);

next_desc:
		rx_desc->status = 0;

		/* return some buffers to hardware, one at a time is too slow */
		if (cleaned_count >= E1000_RX_BUFFER_WRITE) {
			adapter->alloc_rx_buf(adapter, cleaned_count);
			cleaned_count = 0;
		}

		/* use prefetched values */
		rx_desc = next_rxd;
		buffer_info = next_buffer;
	}
	rx_ring->next_to_clean = i;

	cleaned_count = e1000_desc_unused(rx_ring);
	if (cleaned_count)
		adapter->alloc_rx_buf(adapter, cleaned_count);

	adapter->total_rx_bytes += total_rx_bytes;
	adapter->total_rx_packets += total_rx_packets;
	adapter->net_stats.rx_bytes += total_rx_bytes;
	adapter->net_stats.rx_packets += total_rx_packets;
	return cleaned;
}

static void e1000_put_txbuf(struct e1000_adapter *adapter,
			     struct e1000_buffer *buffer_info)
{
	buffer_info->dma = 0;
	if (buffer_info->skb) {
		skb_dma_unmap(&adapter->pdev->dev, buffer_info->skb,
		              DMA_TO_DEVICE);
		dev_kfree_skb_any(buffer_info->skb);
		buffer_info->skb = NULL;
	}
	buffer_info->time_stamp = 0;
}

static void e1000_print_tx_hang(struct e1000_adapter *adapter)
{
	struct e1000_ring *tx_ring = adapter->tx_ring;
	unsigned int i = tx_ring->next_to_clean;
	unsigned int eop = tx_ring->buffer_info[i].next_to_watch;
	struct e1000_tx_desc *eop_desc = E1000_TX_DESC(*tx_ring, eop);

	/* detected Tx unit hang */
	e_err("Detected Tx Unit Hang:\n"
	      "  TDH                  <%x>\n"
	      "  TDT                  <%x>\n"
	      "  next_to_use          <%x>\n"
	      "  next_to_clean        <%x>\n"
	      "buffer_info[next_to_clean]:\n"
	      "  time_stamp           <%lx>\n"
	      "  next_to_watch        <%x>\n"
	      "  jiffies              <%lx>\n"
	      "  next_to_watch.status <%x>\n",
	      readl(adapter->hw.hw_addr + tx_ring->head),
	      readl(adapter->hw.hw_addr + tx_ring->tail),
	      tx_ring->next_to_use,
	      tx_ring->next_to_clean,
	      tx_ring->buffer_info[eop].time_stamp,
	      eop,
	      jiffies,
	      eop_desc->upper.fields.status);
}

/**
 * e1000_clean_tx_irq - Reclaim resources after transmit completes
 * @adapter: board private structure
 *
 * the return value indicates whether actual cleaning was done, there
 * is no guarantee that everything was cleaned
 **/
static bool e1000_clean_tx_irq(struct e1000_adapter *adapter)
{
	struct net_device *netdev = adapter->netdev;
	struct e1000_hw *hw = &adapter->hw;
	struct e1000_ring *tx_ring = adapter->tx_ring;
	struct e1000_tx_desc *tx_desc, *eop_desc;
	struct e1000_buffer *buffer_info;
	unsigned int i, eop;
	unsigned int count = 0;
	unsigned int total_tx_bytes = 0, total_tx_packets = 0;

	i = tx_ring->next_to_clean;
	eop = tx_ring->buffer_info[i].next_to_watch;
	eop_desc = E1000_TX_DESC(*tx_ring, eop);

	while ((eop_desc->upper.data & cpu_to_le32(E1000_TXD_STAT_DD)) &&
	       (count < tx_ring->count)) {
		bool cleaned = false;
		for (; !cleaned; count++) {
			tx_desc = E1000_TX_DESC(*tx_ring, i);
			buffer_info = &tx_ring->buffer_info[i];
			cleaned = (i == eop);

			if (cleaned) {
				struct sk_buff *skb = buffer_info->skb;
				unsigned int segs, bytecount;
				segs = skb_shinfo(skb)->gso_segs ?: 1;
				/* multiply data chunks by size of headers */
				bytecount = ((segs - 1) * skb_headlen(skb)) +
					    skb->len;
				total_tx_packets += segs;
				total_tx_bytes += bytecount;
			}

			e1000_put_txbuf(adapter, buffer_info);
			tx_desc->upper.data = 0;

			i++;
			if (i == tx_ring->count)
				i = 0;
		}

		eop = tx_ring->buffer_info[i].next_to_watch;
		eop_desc = E1000_TX_DESC(*tx_ring, eop);
	}

	tx_ring->next_to_clean = i;

#define TX_WAKE_THRESHOLD 32
	if (count && netif_carrier_ok(netdev) &&
	    e1000_desc_unused(tx_ring) >= TX_WAKE_THRESHOLD) {
		/* Make sure that anybody stopping the queue after this
		 * sees the new next_to_clean.
		 */
		smp_mb();

		if (netif_queue_stopped(netdev) &&
		    !(test_bit(__E1000_DOWN, &adapter->state))) {
			netif_wake_queue(netdev);
			++adapter->restart_queue;
		}
	}

	if (adapter->detect_tx_hung) {
		/* Detect a transmit hang in hardware, this serializes the
		 * check with the clearing of time_stamp and movement of i */
		adapter->detect_tx_hung = 0;
		if (tx_ring->buffer_info[i].time_stamp &&
		    time_after(jiffies, tx_ring->buffer_info[i].time_stamp
			       + (adapter->tx_timeout_factor * HZ))
		    && !(er32(STATUS) & E1000_STATUS_TXOFF)) {
			e1000_print_tx_hang(adapter);
			netif_stop_queue(netdev);
		}
	}
	adapter->total_tx_bytes += total_tx_bytes;
	adapter->total_tx_packets += total_tx_packets;
	adapter->net_stats.tx_bytes += total_tx_bytes;
	adapter->net_stats.tx_packets += total_tx_packets;
	return (count < tx_ring->count);
}

/**
 * e1000_clean_rx_irq_ps - Send received data up the network stack; packet split
 * @adapter: board private structure
 *
 * the return value indicates whether actual cleaning was done, there
 * is no guarantee that everything was cleaned
 **/
static bool e1000_clean_rx_irq_ps(struct e1000_adapter *adapter,
				  int *work_done, int work_to_do)
{
	union e1000_rx_desc_packet_split *rx_desc, *next_rxd;
	struct net_device *netdev = adapter->netdev;
	struct pci_dev *pdev = adapter->pdev;
	struct e1000_ring *rx_ring = adapter->rx_ring;
	struct e1000_buffer *buffer_info, *next_buffer;
	struct e1000_ps_page *ps_page;
	struct sk_buff *skb;
	unsigned int i, j;
	u32 length, staterr;
	int cleaned_count = 0;
	bool cleaned = 0;
	unsigned int total_rx_bytes = 0, total_rx_packets = 0;

	i = rx_ring->next_to_clean;
	rx_desc = E1000_RX_DESC_PS(*rx_ring, i);
	staterr = le32_to_cpu(rx_desc->wb.middle.status_error);
	buffer_info = &rx_ring->buffer_info[i];

	while (staterr & E1000_RXD_STAT_DD) {
		if (*work_done >= work_to_do)
			break;
		(*work_done)++;
		skb = buffer_info->skb;

		/* in the packet split case this is header only */
		prefetch(skb->data - NET_IP_ALIGN);

		i++;
		if (i == rx_ring->count)
			i = 0;
		next_rxd = E1000_RX_DESC_PS(*rx_ring, i);
		prefetch(next_rxd);

		next_buffer = &rx_ring->buffer_info[i];

		cleaned = 1;
		cleaned_count++;
		pci_unmap_single(pdev, buffer_info->dma,
				 adapter->rx_ps_bsize0,
				 PCI_DMA_FROMDEVICE);
		buffer_info->dma = 0;

		if (!(staterr & E1000_RXD_STAT_EOP)) {
			e_dbg("%s: Packet Split buffers didn't pick up the "
			      "full packet\n", netdev->name);
			dev_kfree_skb_irq(skb);
			goto next_desc;
		}

		if (staterr & E1000_RXDEXT_ERR_FRAME_ERR_MASK) {
			dev_kfree_skb_irq(skb);
			goto next_desc;
		}

		length = le16_to_cpu(rx_desc->wb.middle.length0);

		if (!length) {
			e_dbg("%s: Last part of the packet spanning multiple "
			      "descriptors\n", netdev->name);
			dev_kfree_skb_irq(skb);
			goto next_desc;
		}

		/* Good Receive */
		skb_put(skb, length);

		{
		/*
		 * this looks ugly, but it seems compiler issues make it
		 * more efficient than reusing j
		 */
		int l1 = le16_to_cpu(rx_desc->wb.upper.length[0]);

		/*
		 * page alloc/put takes too long and effects small packet
		 * throughput, so unsplit small packets and save the alloc/put
		 * only valid in softirq (napi) context to call kmap_*
		 */
		if (l1 && (l1 <= copybreak) &&
		    ((length + l1) <= adapter->rx_ps_bsize0)) {
			u8 *vaddr;

			ps_page = &buffer_info->ps_pages[0];

			/*
			 * there is no documentation about how to call
			 * kmap_atomic, so we can't hold the mapping
			 * very long
			 */
			pci_dma_sync_single_for_cpu(pdev, ps_page->dma,
				PAGE_SIZE, PCI_DMA_FROMDEVICE);
			vaddr = kmap_atomic(ps_page->page, KM_SKB_DATA_SOFTIRQ);
			memcpy(skb_tail_pointer(skb), vaddr, l1);
			kunmap_atomic(vaddr, KM_SKB_DATA_SOFTIRQ);
			pci_dma_sync_single_for_device(pdev, ps_page->dma,
				PAGE_SIZE, PCI_DMA_FROMDEVICE);

			/* remove the CRC */
			if (!(adapter->flags2 & FLAG2_CRC_STRIPPING))
				l1 -= 4;

			skb_put(skb, l1);
			goto copydone;
		} /* if */
		}

		for (j = 0; j < PS_PAGE_BUFFERS; j++) {
			length = le16_to_cpu(rx_desc->wb.upper.length[j]);
			if (!length)
				break;

			ps_page = &buffer_info->ps_pages[j];
			pci_unmap_page(pdev, ps_page->dma, PAGE_SIZE,
				       PCI_DMA_FROMDEVICE);
			ps_page->dma = 0;
			skb_fill_page_desc(skb, j, ps_page->page, 0, length);
			ps_page->page = NULL;
			skb->len += length;
			skb->data_len += length;
			skb->truesize += length;
		}

		/* strip the ethernet crc, problem is we're using pages now so
		 * this whole operation can get a little cpu intensive
		 */
		if (!(adapter->flags2 & FLAG2_CRC_STRIPPING))
			pskb_trim(skb, skb->len - 4);

copydone:
		total_rx_bytes += skb->len;
		total_rx_packets++;

		e1000_rx_checksum(adapter, staterr, le16_to_cpu(
			rx_desc->wb.lower.hi_dword.csum_ip.csum), skb);

		if (rx_desc->wb.upper.header_status &
			   cpu_to_le16(E1000_RXDPS_HDRSTAT_HDRSP))
			adapter->rx_hdr_split++;

		e1000_receive_skb(adapter, netdev, skb,
				  staterr, rx_desc->wb.middle.vlan);

next_desc:
		rx_desc->wb.middle.status_error &= cpu_to_le32(~0xFF);
		buffer_info->skb = NULL;

		/* return some buffers to hardware, one at a time is too slow */
		if (cleaned_count >= E1000_RX_BUFFER_WRITE) {
			adapter->alloc_rx_buf(adapter, cleaned_count);
			cleaned_count = 0;
		}

		/* use prefetched values */
		rx_desc = next_rxd;
		buffer_info = next_buffer;

		staterr = le32_to_cpu(rx_desc->wb.middle.status_error);
	}
	rx_ring->next_to_clean = i;

	cleaned_count = e1000_desc_unused(rx_ring);
	if (cleaned_count)
		adapter->alloc_rx_buf(adapter, cleaned_count);

	adapter->total_rx_bytes += total_rx_bytes;
	adapter->total_rx_packets += total_rx_packets;
	adapter->net_stats.rx_bytes += total_rx_bytes;
	adapter->net_stats.rx_packets += total_rx_packets;
	return cleaned;
}

/**
 * e1000_consume_page - helper function
 **/
static void e1000_consume_page(struct e1000_buffer *bi, struct sk_buff *skb,
                               u16 length)
{
	bi->page = NULL;
	skb->len += length;
	skb->data_len += length;
	skb->truesize += length;
}

/**
 * e1000_clean_jumbo_rx_irq - Send received data up the network stack; legacy
 * @adapter: board private structure
 *
 * the return value indicates whether actual cleaning was done, there
 * is no guarantee that everything was cleaned
 **/

static bool e1000_clean_jumbo_rx_irq(struct e1000_adapter *adapter,
                                     int *work_done, int work_to_do)
{
	struct net_device *netdev = adapter->netdev;
	struct pci_dev *pdev = adapter->pdev;
	struct e1000_ring *rx_ring = adapter->rx_ring;
	struct e1000_rx_desc *rx_desc, *next_rxd;
	struct e1000_buffer *buffer_info, *next_buffer;
	u32 length;
	unsigned int i;
	int cleaned_count = 0;
	bool cleaned = false;
	unsigned int total_rx_bytes=0, total_rx_packets=0;

	i = rx_ring->next_to_clean;
	rx_desc = E1000_RX_DESC(*rx_ring, i);
	buffer_info = &rx_ring->buffer_info[i];

	while (rx_desc->status & E1000_RXD_STAT_DD) {
		struct sk_buff *skb;
		u8 status;

		if (*work_done >= work_to_do)
			break;
		(*work_done)++;

		status = rx_desc->status;
		skb = buffer_info->skb;
		buffer_info->skb = NULL;

		++i;
		if (i == rx_ring->count)
			i = 0;
		next_rxd = E1000_RX_DESC(*rx_ring, i);
		prefetch(next_rxd);

		next_buffer = &rx_ring->buffer_info[i];

		cleaned = true;
		cleaned_count++;
		pci_unmap_page(pdev, buffer_info->dma, PAGE_SIZE,
		               PCI_DMA_FROMDEVICE);
		buffer_info->dma = 0;

		length = le16_to_cpu(rx_desc->length);

		/* errors is only valid for DD + EOP descriptors */
		if (unlikely((status & E1000_RXD_STAT_EOP) &&
		    (rx_desc->errors & E1000_RXD_ERR_FRAME_ERR_MASK))) {
				/* recycle both page and skb */
				buffer_info->skb = skb;
				/* an error means any chain goes out the window
				 * too */
				if (rx_ring->rx_skb_top)
					dev_kfree_skb(rx_ring->rx_skb_top);
				rx_ring->rx_skb_top = NULL;
				goto next_desc;
		}

#define rxtop rx_ring->rx_skb_top
		if (!(status & E1000_RXD_STAT_EOP)) {
			/* this descriptor is only the beginning (or middle) */
			if (!rxtop) {
				/* this is the beginning of a chain */
				rxtop = skb;
				skb_fill_page_desc(rxtop, 0, buffer_info->page,
				                   0, length);
			} else {
				/* this is the middle of a chain */
				skb_fill_page_desc(rxtop,
				    skb_shinfo(rxtop)->nr_frags,
				    buffer_info->page, 0, length);
				/* re-use the skb, only consumed the page */
				buffer_info->skb = skb;
			}
			e1000_consume_page(buffer_info, rxtop, length);
			goto next_desc;
		} else {
			if (rxtop) {
				/* end of the chain */
				skb_fill_page_desc(rxtop,
				    skb_shinfo(rxtop)->nr_frags,
				    buffer_info->page, 0, length);
				/* re-use the current skb, we only consumed the
				 * page */
				buffer_info->skb = skb;
				skb = rxtop;
				rxtop = NULL;
				e1000_consume_page(buffer_info, skb, length);
			} else {
				/* no chain, got EOP, this buf is the packet
				 * copybreak to save the put_page/alloc_page */
				if (length <= copybreak &&
				    skb_tailroom(skb) >= length) {
					u8 *vaddr;
					vaddr = kmap_atomic(buffer_info->page,
					                   KM_SKB_DATA_SOFTIRQ);
					memcpy(skb_tail_pointer(skb), vaddr,
					       length);
					kunmap_atomic(vaddr,
					              KM_SKB_DATA_SOFTIRQ);
					/* re-use the page, so don't erase
					 * buffer_info->page */
					skb_put(skb, length);
				} else {
					skb_fill_page_desc(skb, 0,
					                   buffer_info->page, 0,
				                           length);
					e1000_consume_page(buffer_info, skb,
					                   length);
				}
			}
		}

		/* Receive Checksum Offload XXX recompute due to CRC strip? */
		e1000_rx_checksum(adapter,
		                  (u32)(status) |
		                  ((u32)(rx_desc->errors) << 24),
		                  le16_to_cpu(rx_desc->csum), skb);

		/* probably a little skewed due to removing CRC */
		total_rx_bytes += skb->len;
		total_rx_packets++;

		/* eth type trans needs skb->data to point to something */
		if (!pskb_may_pull(skb, ETH_HLEN)) {
			e_err("pskb_may_pull failed.\n");
			dev_kfree_skb(skb);
			goto next_desc;
		}

		e1000_receive_skb(adapter, netdev, skb, status,
		                  rx_desc->special);

next_desc:
		rx_desc->status = 0;

		/* return some buffers to hardware, one at a time is too slow */
		if (unlikely(cleaned_count >= E1000_RX_BUFFER_WRITE)) {
			adapter->alloc_rx_buf(adapter, cleaned_count);
			cleaned_count = 0;
		}

		/* use prefetched values */
		rx_desc = next_rxd;
		buffer_info = next_buffer;
	}
	rx_ring->next_to_clean = i;

	cleaned_count = e1000_desc_unused(rx_ring);
	if (cleaned_count)
		adapter->alloc_rx_buf(adapter, cleaned_count);

	adapter->total_rx_bytes += total_rx_bytes;
	adapter->total_rx_packets += total_rx_packets;
	adapter->net_stats.rx_bytes += total_rx_bytes;
	adapter->net_stats.rx_packets += total_rx_packets;
	return cleaned;
}

/**
 * e1000_clean_rx_ring - Free Rx Buffers per Queue
 * @adapter: board private structure
 **/
static void e1000_clean_rx_ring(struct e1000_adapter *adapter)
{
	struct e1000_ring *rx_ring = adapter->rx_ring;
	struct e1000_buffer *buffer_info;
	struct e1000_ps_page *ps_page;
	struct pci_dev *pdev = adapter->pdev;
	unsigned int i, j;

	/* Free all the Rx ring sk_buffs */
	for (i = 0; i < rx_ring->count; i++) {
		buffer_info = &rx_ring->buffer_info[i];
		if (buffer_info->dma) {
			if (adapter->clean_rx == e1000_clean_rx_irq)
				pci_unmap_single(pdev, buffer_info->dma,
						 adapter->rx_buffer_len,
						 PCI_DMA_FROMDEVICE);
			else if (adapter->clean_rx == e1000_clean_jumbo_rx_irq)
				pci_unmap_page(pdev, buffer_info->dma,
				               PAGE_SIZE,
				               PCI_DMA_FROMDEVICE);
			else if (adapter->clean_rx == e1000_clean_rx_irq_ps)
				pci_unmap_single(pdev, buffer_info->dma,
						 adapter->rx_ps_bsize0,
						 PCI_DMA_FROMDEVICE);
			buffer_info->dma = 0;
		}

		if (buffer_info->page) {
			put_page(buffer_info->page);
			buffer_info->page = NULL;
		}

		if (buffer_info->skb) {
			dev_kfree_skb(buffer_info->skb);
			buffer_info->skb = NULL;
		}

		for (j = 0; j < PS_PAGE_BUFFERS; j++) {
			ps_page = &buffer_info->ps_pages[j];
			if (!ps_page->page)
				break;
			pci_unmap_page(pdev, ps_page->dma, PAGE_SIZE,
				       PCI_DMA_FROMDEVICE);
			ps_page->dma = 0;
			put_page(ps_page->page);
			ps_page->page = NULL;
		}
	}

	/* there also may be some cached data from a chained receive */
	if (rx_ring->rx_skb_top) {
		dev_kfree_skb(rx_ring->rx_skb_top);
		rx_ring->rx_skb_top = NULL;
	}

	/* Zero out the descriptor ring */
	memset(rx_ring->desc, 0, rx_ring->size);

	rx_ring->next_to_clean = 0;
	rx_ring->next_to_use = 0;

	writel(0, adapter->hw.hw_addr + rx_ring->head);
	writel(0, adapter->hw.hw_addr + rx_ring->tail);
}

static void e1000e_downshift_workaround(struct work_struct *work)
{
	struct e1000_adapter *adapter = container_of(work,
					struct e1000_adapter, downshift_task);

	e1000e_gig_downshift_workaround_ich8lan(&adapter->hw);
}

/**
 * e1000_intr_msi - Interrupt Handler
 * @irq: interrupt number
 * @data: pointer to a network interface device structure
 **/
static irqreturn_t e1000_intr_msi(int irq, void *data)
{
	struct net_device *netdev = data;
	struct e1000_adapter *adapter = netdev_priv(netdev);
	struct e1000_hw *hw = &adapter->hw;
	u32 icr = er32(ICR);

	/*
	 * read ICR disables interrupts using IAM
	 */

	if (icr & E1000_ICR_LSC) {
		hw->mac.get_link_status = 1;
		/*
		 * ICH8 workaround-- Call gig speed drop workaround on cable
		 * disconnect (LSC) before accessing any PHY registers
		 */
		if ((adapter->flags & FLAG_LSC_GIG_SPEED_DROP) &&
		    (!(er32(STATUS) & E1000_STATUS_LU)))
			schedule_work(&adapter->downshift_task);

		/*
		 * 80003ES2LAN workaround-- For packet buffer work-around on
		 * link down event; disable receives here in the ISR and reset
		 * adapter in watchdog
		 */
		if (netif_carrier_ok(netdev) &&
		    adapter->flags & FLAG_RX_NEEDS_RESTART) {
			/* disable receives */
			u32 rctl = er32(RCTL);
			ew32(RCTL, rctl & ~E1000_RCTL_EN);
			adapter->flags |= FLAG_RX_RESTART_NOW;
		}
		/* guard against interrupt when we're going down */
		if (!test_bit(__E1000_DOWN, &adapter->state))
			mod_timer(&adapter->watchdog_timer, jiffies + 1);
	}

	if (napi_schedule_prep(&adapter->napi)) {
		adapter->total_tx_bytes = 0;
		adapter->total_tx_packets = 0;
		adapter->total_rx_bytes = 0;
		adapter->total_rx_packets = 0;
		__napi_schedule(&adapter->napi);
	}

	return IRQ_HANDLED;
}

/**
 * e1000_intr - Interrupt Handler
 * @irq: interrupt number
 * @data: pointer to a network interface device structure
 **/
static irqreturn_t e1000_intr(int irq, void *data)
{
	struct net_device *netdev = data;
	struct e1000_adapter *adapter = netdev_priv(netdev);
	struct e1000_hw *hw = &adapter->hw;
	u32 rctl, icr = er32(ICR);

	if (!icr)
		return IRQ_NONE;  /* Not our interrupt */

	/*
	 * IMS will not auto-mask if INT_ASSERTED is not set, and if it is
	 * not set, then the adapter didn't send an interrupt
	 */
	if (!(icr & E1000_ICR_INT_ASSERTED))
		return IRQ_NONE;

	/*
	 * Interrupt Auto-Mask...upon reading ICR,
	 * interrupts are masked.  No need for the
	 * IMC write
	 */

	if (icr & E1000_ICR_LSC) {
		hw->mac.get_link_status = 1;
		/*
		 * ICH8 workaround-- Call gig speed drop workaround on cable
		 * disconnect (LSC) before accessing any PHY registers
		 */
		if ((adapter->flags & FLAG_LSC_GIG_SPEED_DROP) &&
		    (!(er32(STATUS) & E1000_STATUS_LU)))
			schedule_work(&adapter->downshift_task);

		/*
		 * 80003ES2LAN workaround--
		 * For packet buffer work-around on link down event;
		 * disable receives here in the ISR and
		 * reset adapter in watchdog
		 */
		if (netif_carrier_ok(netdev) &&
		    (adapter->flags & FLAG_RX_NEEDS_RESTART)) {
			/* disable receives */
			rctl = er32(RCTL);
			ew32(RCTL, rctl & ~E1000_RCTL_EN);
			adapter->flags |= FLAG_RX_RESTART_NOW;
		}
		/* guard against interrupt when we're going down */
		if (!test_bit(__E1000_DOWN, &adapter->state))
			mod_timer(&adapter->watchdog_timer, jiffies + 1);
	}

	if (napi_schedule_prep(&adapter->napi)) {
		adapter->total_tx_bytes = 0;
		adapter->total_tx_packets = 0;
		adapter->total_rx_bytes = 0;
		adapter->total_rx_packets = 0;
		__napi_schedule(&adapter->napi);
	}

	return IRQ_HANDLED;
}

static irqreturn_t e1000_msix_other(int irq, void *data)
{
	struct net_device *netdev = data;
	struct e1000_adapter *adapter = netdev_priv(netdev);
	struct e1000_hw *hw = &adapter->hw;
	u32 icr = er32(ICR);

	if (!(icr & E1000_ICR_INT_ASSERTED)) {
		if (!test_bit(__E1000_DOWN, &adapter->state))
			ew32(IMS, E1000_IMS_OTHER);
		return IRQ_NONE;
	}

	if (icr & adapter->eiac_mask)
		ew32(ICS, (icr & adapter->eiac_mask));

	if (icr & E1000_ICR_OTHER) {
		if (!(icr & E1000_ICR_LSC))
			goto no_link_interrupt;
		hw->mac.get_link_status = 1;
		/* guard against interrupt when we're going down */
		if (!test_bit(__E1000_DOWN, &adapter->state))
			mod_timer(&adapter->watchdog_timer, jiffies + 1);
	}

no_link_interrupt:
	if (!test_bit(__E1000_DOWN, &adapter->state))
		ew32(IMS, E1000_IMS_LSC | E1000_IMS_OTHER);

	return IRQ_HANDLED;
}


static irqreturn_t e1000_intr_msix_tx(int irq, void *data)
{
	struct net_device *netdev = data;
	struct e1000_adapter *adapter = netdev_priv(netdev);
	struct e1000_hw *hw = &adapter->hw;
	struct e1000_ring *tx_ring = adapter->tx_ring;


	adapter->total_tx_bytes = 0;
	adapter->total_tx_packets = 0;

	if (!e1000_clean_tx_irq(adapter))
		/* Ring was not completely cleaned, so fire another interrupt */
		ew32(ICS, tx_ring->ims_val);

	return IRQ_HANDLED;
}

static irqreturn_t e1000_intr_msix_rx(int irq, void *data)
{
	struct net_device *netdev = data;
	struct e1000_adapter *adapter = netdev_priv(netdev);

	/* Write the ITR value calculated at the end of the
	 * previous interrupt.
	 */
	if (adapter->rx_ring->set_itr) {
		writel(1000000000 / (adapter->rx_ring->itr_val * 256),
		       adapter->hw.hw_addr + adapter->rx_ring->itr_register);
		adapter->rx_ring->set_itr = 0;
	}

	if (napi_schedule_prep(&adapter->napi)) {
		adapter->total_rx_bytes = 0;
		adapter->total_rx_packets = 0;
		__napi_schedule(&adapter->napi);
	}
	return IRQ_HANDLED;
}

/**
 * e1000_configure_msix - Configure MSI-X hardware
 *
 * e1000_configure_msix sets up the hardware to properly
 * generate MSI-X interrupts.
 **/
static void e1000_configure_msix(struct e1000_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	struct e1000_ring *rx_ring = adapter->rx_ring;
	struct e1000_ring *tx_ring = adapter->tx_ring;
	int vector = 0;
	u32 ctrl_ext, ivar = 0;

	adapter->eiac_mask = 0;

	/* Workaround issue with spurious interrupts on 82574 in MSI-X mode */
	if (hw->mac.type == e1000_82574) {
		u32 rfctl = er32(RFCTL);
		rfctl |= E1000_RFCTL_ACK_DIS;
		ew32(RFCTL, rfctl);
	}

#define E1000_IVAR_INT_ALLOC_VALID	0x8
	/* Configure Rx vector */
	rx_ring->ims_val = E1000_IMS_RXQ0;
	adapter->eiac_mask |= rx_ring->ims_val;
	if (rx_ring->itr_val)
		writel(1000000000 / (rx_ring->itr_val * 256),
		       hw->hw_addr + rx_ring->itr_register);
	else
		writel(1, hw->hw_addr + rx_ring->itr_register);
	ivar = E1000_IVAR_INT_ALLOC_VALID | vector;

	/* Configure Tx vector */
	tx_ring->ims_val = E1000_IMS_TXQ0;
	vector++;
	if (tx_ring->itr_val)
		writel(1000000000 / (tx_ring->itr_val * 256),
		       hw->hw_addr + tx_ring->itr_register);
	else
		writel(1, hw->hw_addr + tx_ring->itr_register);
	adapter->eiac_mask |= tx_ring->ims_val;
	ivar |= ((E1000_IVAR_INT_ALLOC_VALID | vector) << 8);

	/* set vector for Other Causes, e.g. link changes */
	vector++;
	ivar |= ((E1000_IVAR_INT_ALLOC_VALID | vector) << 16);
	if (rx_ring->itr_val)
		writel(1000000000 / (rx_ring->itr_val * 256),
		       hw->hw_addr + E1000_EITR_82574(vector));
	else
		writel(1, hw->hw_addr + E1000_EITR_82574(vector));

	/* Cause Tx interrupts on every write back */
	ivar |= (1 << 31);

	ew32(IVAR, ivar);

	/* enable MSI-X PBA support */
	ctrl_ext = er32(CTRL_EXT);
	ctrl_ext |= E1000_CTRL_EXT_PBA_CLR;

	/* Auto-Mask Other interrupts upon ICR read */
#define E1000_EIAC_MASK_82574   0x01F00000
	ew32(IAM, ~E1000_EIAC_MASK_82574 | E1000_IMS_OTHER);
	ctrl_ext |= E1000_CTRL_EXT_EIAME;
	ew32(CTRL_EXT, ctrl_ext);
	e1e_flush();
}

void e1000e_reset_interrupt_capability(struct e1000_adapter *adapter)
{
	if (adapter->msix_entries) {
		pci_disable_msix(adapter->pdev);
		kfree(adapter->msix_entries);
		adapter->msix_entries = NULL;
	} else if (adapter->flags & FLAG_MSI_ENABLED) {
		pci_disable_msi(adapter->pdev);
		adapter->flags &= ~FLAG_MSI_ENABLED;
	}

	return;
}

/**
 * e1000e_set_interrupt_capability - set MSI or MSI-X if supported
 *
 * Attempt to configure interrupts using the best available
 * capabilities of the hardware and kernel.
 **/
void e1000e_set_interrupt_capability(struct e1000_adapter *adapter)
{
	int err;
	int numvecs, i;


	switch (adapter->int_mode) {
	case E1000E_INT_MODE_MSIX:
		if (adapter->flags & FLAG_HAS_MSIX) {
			numvecs = 3; /* RxQ0, TxQ0 and other */
			adapter->msix_entries = kcalloc(numvecs,
						      sizeof(struct msix_entry),
						      GFP_KERNEL);
			if (adapter->msix_entries) {
				for (i = 0; i < numvecs; i++)
					adapter->msix_entries[i].entry = i;

				err = pci_enable_msix(adapter->pdev,
						      adapter->msix_entries,
						      numvecs);
				if (err == 0)
					return;
			}
			/* MSI-X failed, so fall through and try MSI */
			e_err("Failed to initialize MSI-X interrupts.  "
			      "Falling back to MSI interrupts.\n");
			e1000e_reset_interrupt_capability(adapter);
		}
		adapter->int_mode = E1000E_INT_MODE_MSI;
		/* Fall through */
	case E1000E_INT_MODE_MSI:
		if (!pci_enable_msi(adapter->pdev)) {
			adapter->flags |= FLAG_MSI_ENABLED;
		} else {
			adapter->int_mode = E1000E_INT_MODE_LEGACY;
			e_err("Failed to initialize MSI interrupts.  Falling "
			      "back to legacy interrupts.\n");
		}
		/* Fall through */
	case E1000E_INT_MODE_LEGACY:
		/* Don't do anything; this is the system default */
		break;
	}

	return;
}

/**
 * e1000_request_msix - Initialize MSI-X interrupts
 *
 * e1000_request_msix allocates MSI-X vectors and requests interrupts from the
 * kernel.
 **/
static int e1000_request_msix(struct e1000_adapter *adapter)
{
	struct net_device *netdev = adapter->netdev;
	int err = 0, vector = 0;

	if (strlen(netdev->name) < (IFNAMSIZ - 5))
		sprintf(adapter->rx_ring->name, "%s-rx-0", netdev->name);
	else
		memcpy(adapter->rx_ring->name, netdev->name, IFNAMSIZ);
	err = request_irq(adapter->msix_entries[vector].vector,
			  &e1000_intr_msix_rx, 0, adapter->rx_ring->name,
			  netdev);
	if (err)
		goto out;
	adapter->rx_ring->itr_register = E1000_EITR_82574(vector);
	adapter->rx_ring->itr_val = adapter->itr;
	vector++;

	if (strlen(netdev->name) < (IFNAMSIZ - 5))
		sprintf(adapter->tx_ring->name, "%s-tx-0", netdev->name);
	else
		memcpy(adapter->tx_ring->name, netdev->name, IFNAMSIZ);
	err = request_irq(adapter->msix_entries[vector].vector,
			  &e1000_intr_msix_tx, 0, adapter->tx_ring->name,
			  netdev);
	if (err)
		goto out;
	adapter->tx_ring->itr_register = E1000_EITR_82574(vector);
	adapter->tx_ring->itr_val = adapter->itr;
	vector++;

	err = request_irq(adapter->msix_entries[vector].vector,
			  &e1000_msix_other, 0, netdev->name, netdev);
	if (err)
		goto out;

	e1000_configure_msix(adapter);
	return 0;
out:
	return err;
}

/**
 * e1000_request_irq - initialize interrupts
 *
 * Attempts to configure interrupts using the best available
 * capabilities of the hardware and kernel.
 **/
static int e1000_request_irq(struct e1000_adapter *adapter)
{
	struct net_device *netdev = adapter->netdev;
	int err;

	if (adapter->msix_entries) {
		err = e1000_request_msix(adapter);
		if (!err)
			return err;
		/* fall back to MSI */
		e1000e_reset_interrupt_capability(adapter);
		adapter->int_mode = E1000E_INT_MODE_MSI;
		e1000e_set_interrupt_capability(adapter);
	}
	if (adapter->flags & FLAG_MSI_ENABLED) {
		err = request_irq(adapter->pdev->irq, &e1000_intr_msi, 0,
				  netdev->name, netdev);
		if (!err)
			return err;

		/* fall back to legacy interrupt */
		e1000e_reset_interrupt_capability(adapter);
		adapter->int_mode = E1000E_INT_MODE_LEGACY;
	}

	err = request_irq(adapter->pdev->irq, &e1000_intr, IRQF_SHARED,
			  netdev->name, netdev);
	if (err)
		e_err("Unable to allocate interrupt, Error: %d\n", err);

	return err;
}

static void e1000_free_irq(struct e1000_adapter *adapter)
{
	struct net_device *netdev = adapter->netdev;

	if (adapter->msix_entries) {
		int vector = 0;

		free_irq(adapter->msix_entries[vector].vector, netdev);
		vector++;

		free_irq(adapter->msix_entries[vector].vector, netdev);
		vector++;

		/* Other Causes interrupt vector */
		free_irq(adapter->msix_entries[vector].vector, netdev);
		return;
	}

	free_irq(adapter->pdev->irq, netdev);
}

/**
 * e1000_irq_disable - Mask off interrupt generation on the NIC
 **/
static void e1000_irq_disable(struct e1000_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;

	ew32(IMC, ~0);
	if (adapter->msix_entries)
		ew32(EIAC_82574, 0);
	e1e_flush();
	synchronize_irq(adapter->pdev->irq);
}

/**
 * e1000_irq_enable - Enable default interrupt generation settings
 **/
static void e1000_irq_enable(struct e1000_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;

	if (adapter->msix_entries) {
		ew32(EIAC_82574, adapter->eiac_mask & E1000_EIAC_MASK_82574);
		ew32(IMS, adapter->eiac_mask | E1000_IMS_OTHER | E1000_IMS_LSC);
	} else {
		ew32(IMS, IMS_ENABLE_MASK);
	}
	e1e_flush();
}

/**
 * e1000_get_hw_control - get control of the h/w from f/w
 * @adapter: address of board private structure
 *
 * e1000_get_hw_control sets {CTRL_EXT|SWSM}:DRV_LOAD bit.
 * For ASF and Pass Through versions of f/w this means that
 * the driver is loaded. For AMT version (only with 82573)
 * of the f/w this means that the network i/f is open.
 **/
static void e1000_get_hw_control(struct e1000_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	u32 ctrl_ext;
	u32 swsm;

	/* Let firmware know the driver has taken over */
	if (adapter->flags & FLAG_HAS_SWSM_ON_LOAD) {
		swsm = er32(SWSM);
		ew32(SWSM, swsm | E1000_SWSM_DRV_LOAD);
	} else if (adapter->flags & FLAG_HAS_CTRLEXT_ON_LOAD) {
		ctrl_ext = er32(CTRL_EXT);
		ew32(CTRL_EXT, ctrl_ext | E1000_CTRL_EXT_DRV_LOAD);
	}
}

/**
 * e1000_release_hw_control - release control of the h/w to f/w
 * @adapter: address of board private structure
 *
 * e1000_release_hw_control resets {CTRL_EXT|SWSM}:DRV_LOAD bit.
 * For ASF and Pass Through versions of f/w this means that the
 * driver is no longer loaded. For AMT version (only with 82573) i
 * of the f/w this means that the network i/f is closed.
 *
 **/
static void e1000_release_hw_control(struct e1000_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	u32 ctrl_ext;
	u32 swsm;

	/* Let firmware taken over control of h/w */
	if (adapter->flags & FLAG_HAS_SWSM_ON_LOAD) {
		swsm = er32(SWSM);
		ew32(SWSM, swsm & ~E1000_SWSM_DRV_LOAD);
	} else if (adapter->flags & FLAG_HAS_CTRLEXT_ON_LOAD) {
		ctrl_ext = er32(CTRL_EXT);
		ew32(CTRL_EXT, ctrl_ext & ~E1000_CTRL_EXT_DRV_LOAD);
	}
}

/**
 * @e1000_alloc_ring - allocate memory for a ring structure
 **/
static int e1000_alloc_ring_dma(struct e1000_adapter *adapter,
				struct e1000_ring *ring)
{
	struct pci_dev *pdev = adapter->pdev;

	ring->desc = dma_alloc_coherent(&pdev->dev, ring->size, &ring->dma,
					GFP_KERNEL);
	if (!ring->desc)
		return -ENOMEM;

	return 0;
}

/**
 * e1000e_setup_tx_resources - allocate Tx resources (Descriptors)
 * @adapter: board private structure
 *
 * Return 0 on success, negative on failure
 **/
int e1000e_setup_tx_resources(struct e1000_adapter *adapter)
{
	struct e1000_ring *tx_ring = adapter->tx_ring;
	int err = -ENOMEM, size;

	size = sizeof(struct e1000_buffer) * tx_ring->count;
	tx_ring->buffer_info = vmalloc(size);
	if (!tx_ring->buffer_info)
		goto err;
	memset(tx_ring->buffer_info, 0, size);

	/* round up to nearest 4K */
	tx_ring->size = tx_ring->count * sizeof(struct e1000_tx_desc);
	tx_ring->size = ALIGN(tx_ring->size, 4096);

	err = e1000_alloc_ring_dma(adapter, tx_ring);
	if (err)
		goto err;

	tx_ring->next_to_use = 0;
	tx_ring->next_to_clean = 0;

	return 0;
err:
	vfree(tx_ring->buffer_info);
	e_err("Unable to allocate memory for the transmit descriptor ring\n");
	return err;
}

/**
 * e1000e_setup_rx_resources - allocate Rx resources (Descriptors)
 * @adapter: board private structure
 *
 * Returns 0 on success, negative on failure
 **/
int e1000e_setup_rx_resources(struct e1000_adapter *adapter)
{
	struct e1000_ring *rx_ring = adapter->rx_ring;
	struct e1000_buffer *buffer_info;
	int i, size, desc_len, err = -ENOMEM;

	size = sizeof(struct e1000_buffer) * rx_ring->count;
	rx_ring->buffer_info = vmalloc(size);
	if (!rx_ring->buffer_info)
		goto err;
	memset(rx_ring->buffer_info, 0, size);

	for (i = 0; i < rx_ring->count; i++) {
		buffer_info = &rx_ring->buffer_info[i];
		buffer_info->ps_pages = kcalloc(PS_PAGE_BUFFERS,
						sizeof(struct e1000_ps_page),
						GFP_KERNEL);
		if (!buffer_info->ps_pages)
			goto err_pages;
	}

	desc_len = sizeof(union e1000_rx_desc_packet_split);

	/* Round up to nearest 4K */
	rx_ring->size = rx_ring->count * desc_len;
	rx_ring->size = ALIGN(rx_ring->size, 4096);

	err = e1000_alloc_ring_dma(adapter, rx_ring);
	if (err)
		goto err_pages;

	rx_ring->next_to_clean = 0;
	rx_ring->next_to_use = 0;
	rx_ring->rx_skb_top = NULL;

	return 0;

err_pages:
	for (i = 0; i < rx_ring->count; i++) {
		buffer_info = &rx_ring->buffer_info[i];
		kfree(buffer_info->ps_pages);
	}
err:
	vfree(rx_ring->buffer_info);
	e_err("Unable to allocate memory for the transmit descriptor ring\n");
	return err;
}

/**
 * e1000_clean_tx_ring - Free Tx Buffers
 * @adapter: board private structure
 **/
static void e1000_clean_tx_ring(struct e1000_adapter *adapter)
{
	struct e1000_ring *tx_ring = adapter->tx_ring;
	struct e1000_buffer *buffer_info;
	unsigned long size;
	unsigned int i;

	for (i = 0; i < tx_ring->count; i++) {
		buffer_info = &tx_ring->buffer_info[i];
		e1000_put_txbuf(adapter, buffer_info);
	}

	size = sizeof(struct e1000_buffer) * tx_ring->count;
	memset(tx_ring->buffer_info, 0, size);

	memset(tx_ring->desc, 0, tx_ring->size);

	tx_ring->next_to_use = 0;
	tx_ring->next_to_clean = 0;

	writel(0, adapter->hw.hw_addr + tx_ring->head);
	writel(0, adapter->hw.hw_addr + tx_ring->tail);
}

/**
 * e1000e_free_tx_resources - Free Tx Resources per Queue
 * @adapter: board private structure
 *
 * Free all transmit software resources
 **/
void e1000e_free_tx_resources(struct e1000_adapter *adapter)
{
	struct pci_dev *pdev = adapter->pdev;
	struct e1000_ring *tx_ring = adapter->tx_ring;

	e1000_clean_tx_ring(adapter);

	vfree(tx_ring->buffer_info);
	tx_ring->buffer_info = NULL;

	dma_free_coherent(&pdev->dev, tx_ring->size, tx_ring->desc,
			  tx_ring->dma);
	tx_ring->desc = NULL;
}

/**
 * e1000e_free_rx_resources - Free Rx Resources
 * @adapter: board private structure
 *
 * Free all receive software resources
 **/

void e1000e_free_rx_resources(struct e1000_adapter *adapter)
{
	struct pci_dev *pdev = adapter->pdev;
	struct e1000_ring *rx_ring = adapter->rx_ring;
	int i;

	e1000_clean_rx_ring(adapter);

	for (i = 0; i < rx_ring->count; i++) {
		kfree(rx_ring->buffer_info[i].ps_pages);
	}

	vfree(rx_ring->buffer_info);
	rx_ring->buffer_info = NULL;

	dma_free_coherent(&pdev->dev, rx_ring->size, rx_ring->desc,
			  rx_ring->dma);
	rx_ring->desc = NULL;
}

/**
 * e1000_update_itr - update the dynamic ITR value based on statistics
 * @adapter: pointer to adapter
 * @itr_setting: current adapter->itr
 * @packets: the number of packets during this measurement interval
 * @bytes: the number of bytes during this measurement interval
 *
 *      Stores a new ITR value based on packets and byte
 *      counts during the last interrupt.  The advantage of per interrupt
 *      computation is faster updates and more accurate ITR for the current
 *      traffic pattern.  Constants in this function were computed
 *      based on theoretical maximum wire speed and thresholds were set based
 *      on testing data as well as attempting to minimize response time
 *      while increasing bulk throughput.  This functionality is controlled
 *      by the InterruptThrottleRate module parameter.
 **/
static unsigned int e1000_update_itr(struct e1000_adapter *adapter,
				     u16 itr_setting, int packets,
				     int bytes)
{
	unsigned int retval = itr_setting;

	if (packets == 0)
		goto update_itr_done;

	switch (itr_setting) {
	case lowest_latency:
		/* handle TSO and jumbo frames */
		if (bytes/packets > 8000)
			retval = bulk_latency;
		else if ((packets < 5) && (bytes > 512)) {
			retval = low_latency;
		}
		break;
	case low_latency:  /* 50 usec aka 20000 ints/s */
		if (bytes > 10000) {
			/* this if handles the TSO accounting */
			if (bytes/packets > 8000) {
				retval = bulk_latency;
			} else if ((packets < 10) || ((bytes/packets) > 1200)) {
				retval = bulk_latency;
			} else if ((packets > 35)) {
				retval = lowest_latency;
			}
		} else if (bytes/packets > 2000) {
			retval = bulk_latency;
		} else if (packets <= 2 && bytes < 512) {
			retval = lowest_latency;
		}
		break;
	case bulk_latency: /* 250 usec aka 4000 ints/s */
		if (bytes > 25000) {
			if (packets > 35) {
				retval = low_latency;
			}
		} else if (bytes < 6000) {
			retval = low_latency;
		}
		break;
	}

update_itr_done:
	return retval;
}

static void e1000_set_itr(struct e1000_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	u16 current_itr;
	u32 new_itr = adapter->itr;

	/* for non-gigabit speeds, just fix the interrupt rate at 4000 */
	if (adapter->link_speed != SPEED_1000) {
		current_itr = 0;
		new_itr = 4000;
		goto set_itr_now;
	}

	adapter->tx_itr = e1000_update_itr(adapter,
				    adapter->tx_itr,
				    adapter->total_tx_packets,
				    adapter->total_tx_bytes);
	/* conservative mode (itr 3) eliminates the lowest_latency setting */
	if (adapter->itr_setting == 3 && adapter->tx_itr == lowest_latency)
		adapter->tx_itr = low_latency;

	adapter->rx_itr = e1000_update_itr(adapter,
				    adapter->rx_itr,
				    adapter->total_rx_packets,
				    adapter->total_rx_bytes);
	/* conservative mode (itr 3) eliminates the lowest_latency setting */
	if (adapter->itr_setting == 3 && adapter->rx_itr == lowest_latency)
		adapter->rx_itr = low_latency;

	current_itr = max(adapter->rx_itr, adapter->tx_itr);

	switch (current_itr) {
	/* counts and packets in update_itr are dependent on these numbers */
	case lowest_latency:
		new_itr = 70000;
		break;
	case low_latency:
		new_itr = 20000; /* aka hwitr = ~200 */
		break;
	case bulk_latency:
		new_itr = 4000;
		break;
	default:
		break;
	}

set_itr_now:
	if (new_itr != adapter->itr) {
		/*
		 * this attempts to bias the interrupt rate towards Bulk
		 * by adding intermediate steps when interrupt rate is
		 * increasing
		 */
		new_itr = new_itr > adapter->itr ?
			     min(adapter->itr + (new_itr >> 2), new_itr) :
			     new_itr;
		adapter->itr = new_itr;
		adapter->rx_ring->itr_val = new_itr;
		if (adapter->msix_entries)
			adapter->rx_ring->set_itr = 1;
		else
			ew32(ITR, 1000000000 / (new_itr * 256));
	}
}

/**
 * e1000_alloc_queues - Allocate memory for all rings
 * @adapter: board private structure to initialize
 **/
static int __devinit e1000_alloc_queues(struct e1000_adapter *adapter)
{
	adapter->tx_ring = kzalloc(sizeof(struct e1000_ring), GFP_KERNEL);
	if (!adapter->tx_ring)
		goto err;

	adapter->rx_ring = kzalloc(sizeof(struct e1000_ring), GFP_KERNEL);
	if (!adapter->rx_ring)
		goto err;

	return 0;
err:
	e_err("Unable to allocate memory for queues\n");
	kfree(adapter->rx_ring);
	kfree(adapter->tx_ring);
	return -ENOMEM;
}

/**
 * e1000_clean - NAPI Rx polling callback
 * @napi: struct associated with this polling callback
 * @budget: amount of packets driver is allowed to process this poll
 **/
static int e1000_clean(struct napi_struct *napi, int budget)
{
	struct e1000_adapter *adapter = container_of(napi, struct e1000_adapter, napi);
	struct e1000_hw *hw = &adapter->hw;
	struct net_device *poll_dev = adapter->netdev;
	int tx_cleaned = 1, work_done = 0;

	adapter = netdev_priv(poll_dev);

	if (adapter->msix_entries &&
	    !(adapter->rx_ring->ims_val & adapter->tx_ring->ims_val))
		goto clean_rx;

	tx_cleaned = e1000_clean_tx_irq(adapter);

clean_rx:
	adapter->clean_rx(adapter, &work_done, budget);

	if (!tx_cleaned)
		work_done = budget;

	/* If budget not fully consumed, exit the polling mode */
	if (work_done < budget) {
		if (adapter->itr_setting & 3)
			e1000_set_itr(adapter);
		napi_complete(napi);
		if (!test_bit(__E1000_DOWN, &adapter->state)) {
			if (adapter->msix_entries)
				ew32(IMS, adapter->rx_ring->ims_val);
			else
				e1000_irq_enable(adapter);
		}
	}

	return work_done;
}

static void e1000_vlan_rx_add_vid(struct net_device *netdev, u16 vid)
{
	struct e1000_adapter *adapter = netdev_priv(netdev);
	struct e1000_hw *hw = &adapter->hw;
	u32 vfta, index;

	/* don't update vlan cookie if already programmed */
	if ((adapter->hw.mng_cookie.status &
	     E1000_MNG_DHCP_COOKIE_STATUS_VLAN) &&
	    (vid == adapter->mng_vlan_id))
		return;
	/* add VID to filter table */
	index = (vid >> 5) & 0x7F;
	vfta = E1000_READ_REG_ARRAY(hw, E1000_VFTA, index);
	vfta |= (1 << (vid & 0x1F));
	e1000e_write_vfta(hw, index, vfta);
}

static void e1000_vlan_rx_kill_vid(struct net_device *netdev, u16 vid)
{
	struct e1000_adapter *adapter = netdev_priv(netdev);
	struct e1000_hw *hw = &adapter->hw;
	u32 vfta, index;

	if (!test_bit(__E1000_DOWN, &adapter->state))
		e1000_irq_disable(adapter);
	vlan_group_set_device(adapter->vlgrp, vid, NULL);

	if (!test_bit(__E1000_DOWN, &adapter->state))
		e1000_irq_enable(adapter);

	if ((adapter->hw.mng_cookie.status &
	     E1000_MNG_DHCP_COOKIE_STATUS_VLAN) &&
	    (vid == adapter->mng_vlan_id)) {
		/* release control to f/w */
		e1000_release_hw_control(adapter);
		return;
	}

	/* remove VID from filter table */
	index = (vid >> 5) & 0x7F;
	vfta = E1000_READ_REG_ARRAY(hw, E1000_VFTA, index);
	vfta &= ~(1 << (vid & 0x1F));
	e1000e_write_vfta(hw, index, vfta);
}

static void e1000_update_mng_vlan(struct e1000_adapter *adapter)
{
	struct net_device *netdev = adapter->netdev;
	u16 vid = adapter->hw.mng_cookie.vlan_id;
	u16 old_vid = adapter->mng_vlan_id;

	if (!adapter->vlgrp)
		return;

	if (!vlan_group_get_device(adapter->vlgrp, vid)) {
		adapter->mng_vlan_id = E1000_MNG_VLAN_NONE;
		if (adapter->hw.mng_cookie.status &
			E1000_MNG_DHCP_COOKIE_STATUS_VLAN) {
			e1000_vlan_rx_add_vid(netdev, vid);
			adapter->mng_vlan_id = vid;
		}

		if ((old_vid != (u16)E1000_MNG_VLAN_NONE) &&
				(vid != old_vid) &&
		    !vlan_group_get_device(adapter->vlgrp, old_vid))
			e1000_vlan_rx_kill_vid(netdev, old_vid);
	} else {
		adapter->mng_vlan_id = vid;
	}
}


static void e1000_vlan_rx_register(struct net_device *netdev,
				   struct vlan_group *grp)
{
	struct e1000_adapter *adapter = netdev_priv(netdev);
	struct e1000_hw *hw = &adapter->hw;
	u32 ctrl, rctl;

	if (!test_bit(__E1000_DOWN, &adapter->state))
		e1000_irq_disable(adapter);
	adapter->vlgrp = grp;

	if (grp) {
		/* enable VLAN tag insert/strip */
		ctrl = er32(CTRL);
		ctrl |= E1000_CTRL_VME;
		ew32(CTRL, ctrl);

		if (adapter->flags & FLAG_HAS_HW_VLAN_FILTER) {
			/* enable VLAN receive filtering */
			rctl = er32(RCTL);
			rctl &= ~E1000_RCTL_CFIEN;
			ew32(RCTL, rctl);
			e1000_update_mng_vlan(adapter);
		}
	} else {
		/* disable VLAN tag insert/strip */
		ctrl = er32(CTRL);
		ctrl &= ~E1000_CTRL_VME;
		ew32(CTRL, ctrl);

		if (adapter->flags & FLAG_HAS_HW_VLAN_FILTER) {
			if (adapter->mng_vlan_id !=
			    (u16)E1000_MNG_VLAN_NONE) {
				e1000_vlan_rx_kill_vid(netdev,
						       adapter->mng_vlan_id);
				adapter->mng_vlan_id = E1000_MNG_VLAN_NONE;
			}
		}
	}

	if (!test_bit(__E1000_DOWN, &adapter->state))
		e1000_irq_enable(adapter);
}

static void e1000_restore_vlan(struct e1000_adapter *adapter)
{
	u16 vid;

	e1000_vlan_rx_register(adapter->netdev, adapter->vlgrp);

	if (!adapter->vlgrp)
		return;

	for (vid = 0; vid < VLAN_GROUP_ARRAY_LEN; vid++) {
		if (!vlan_group_get_device(adapter->vlgrp, vid))
			continue;
		e1000_vlan_rx_add_vid(adapter->netdev, vid);
	}
}

static void e1000_init_manageability(struct e1000_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	u32 manc, manc2h;

	if (!(adapter->flags & FLAG_MNG_PT_ENABLED))
		return;

	manc = er32(MANC);

	/*
	 * enable receiving management packets to the host. this will probably
	 * generate destination unreachable messages from the host OS, but
	 * the packets will be handled on SMBUS
	 */
	manc |= E1000_MANC_EN_MNG2HOST;
	manc2h = er32(MANC2H);
#define E1000_MNG2HOST_PORT_623 (1 << 5)
#define E1000_MNG2HOST_PORT_664 (1 << 6)
	manc2h |= E1000_MNG2HOST_PORT_623;
	manc2h |= E1000_MNG2HOST_PORT_664;
	ew32(MANC2H, manc2h);
	ew32(MANC, manc);
}

/**
 * e1000_configure_tx - Configure 8254x Transmit Unit after Reset
 * @adapter: board private structure
 *
 * Configure the Tx unit of the MAC after a reset.
 **/
static void e1000_configure_tx(struct e1000_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	struct e1000_ring *tx_ring = adapter->tx_ring;
	u64 tdba;
	u32 tdlen, tctl, tipg, tarc;
	u32 ipgr1, ipgr2;

	/* Setup the HW Tx Head and Tail descriptor pointers */
	tdba = tx_ring->dma;
	tdlen = tx_ring->count * sizeof(struct e1000_tx_desc);
	ew32(TDBAL, (tdba & DMA_BIT_MASK(32)));
	ew32(TDBAH, (tdba >> 32));
	ew32(TDLEN, tdlen);
	ew32(TDH, 0);
	ew32(TDT, 0);
	tx_ring->head = E1000_TDH;
	tx_ring->tail = E1000_TDT;

	/* Set the default values for the Tx Inter Packet Gap timer */
	tipg = DEFAULT_82543_TIPG_IPGT_COPPER;          /*  8  */
	ipgr1 = DEFAULT_82543_TIPG_IPGR1;               /*  8  */
	ipgr2 = DEFAULT_82543_TIPG_IPGR2;               /*  6  */

	if (adapter->flags & FLAG_TIPG_MEDIUM_FOR_80003ESLAN)
		ipgr2 = DEFAULT_80003ES2LAN_TIPG_IPGR2; /*  7  */

	tipg |= ipgr1 << E1000_TIPG_IPGR1_SHIFT;
	tipg |= ipgr2 << E1000_TIPG_IPGR2_SHIFT;
	ew32(TIPG, tipg);

	/* Set the Tx Interrupt Delay register */
	ew32(TIDV, adapter->tx_int_delay);
	/* Tx irq moderation */
	ew32(TADV, adapter->tx_abs_int_delay);

	/* Program the Transmit Control Register */
	tctl = er32(TCTL);
	tctl &= ~E1000_TCTL_CT;
	tctl |= E1000_TCTL_PSP | E1000_TCTL_RTLC |
		(E1000_COLLISION_THRESHOLD << E1000_CT_SHIFT);

	if (adapter->flags & FLAG_TARC_SPEED_MODE_BIT) {
		tarc = er32(TARC(0));
		/*
		 * set the speed mode bit, we'll clear it if we're not at
		 * gigabit link later
		 */
#define SPEED_MODE_BIT (1 << 21)
		tarc |= SPEED_MODE_BIT;
		ew32(TARC(0), tarc);
	}

	/* errata: program both queues to unweighted RR */
	if (adapter->flags & FLAG_TARC_SET_BIT_ZERO) {
		tarc = er32(TARC(0));
		tarc |= 1;
		ew32(TARC(0), tarc);
		tarc = er32(TARC(1));
		tarc |= 1;
		ew32(TARC(1), tarc);
	}

	/* Setup Transmit Descriptor Settings for eop descriptor */
	adapter->txd_cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_IFCS;

	/* only set IDE if we are delaying interrupts using the timers */
	if (adapter->tx_int_delay)
		adapter->txd_cmd |= E1000_TXD_CMD_IDE;

	/* enable Report Status bit */
	adapter->txd_cmd |= E1000_TXD_CMD_RS;

	ew32(TCTL, tctl);

	e1000e_config_collision_dist(hw);

	adapter->tx_queue_len = adapter->netdev->tx_queue_len;
}

/**
 * e1000_setup_rctl - configure the receive control registers
 * @adapter: Board private structure
 **/
#define PAGE_USE_COUNT(S) (((S) >> PAGE_SHIFT) + \
			   (((S) & (PAGE_SIZE - 1)) ? 1 : 0))
static void e1000_setup_rctl(struct e1000_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	u32 rctl, rfctl;
	u32 psrctl = 0;
	u32 pages = 0;

	/* Program MC offset vector base */
	rctl = er32(RCTL);
	rctl &= ~(3 << E1000_RCTL_MO_SHIFT);
	rctl |= E1000_RCTL_EN | E1000_RCTL_BAM |
		E1000_RCTL_LBM_NO | E1000_RCTL_RDMTS_HALF |
		(adapter->hw.mac.mc_filter_type << E1000_RCTL_MO_SHIFT);

	/* Do not Store bad packets */
	rctl &= ~E1000_RCTL_SBP;

	/* Enable Long Packet receive */
	if (adapter->netdev->mtu <= ETH_DATA_LEN)
		rctl &= ~E1000_RCTL_LPE;
	else
		rctl |= E1000_RCTL_LPE;

	/* Some systems expect that the CRC is included in SMBUS traffic. The
	 * hardware strips the CRC before sending to both SMBUS (BMC) and to
	 * host memory when this is enabled
	 */
	if (adapter->flags2 & FLAG2_CRC_STRIPPING)
		rctl |= E1000_RCTL_SECRC;

	/* Workaround Si errata on 82577 PHY - configure IPG for jumbos */
	if ((hw->phy.type == e1000_phy_82577) && (rctl & E1000_RCTL_LPE)) {
		u16 phy_data;

		e1e_rphy(hw, PHY_REG(770, 26), &phy_data);
		phy_data &= 0xfff8;
		phy_data |= (1 << 2);
		e1e_wphy(hw, PHY_REG(770, 26), phy_data);

		e1e_rphy(hw, 22, &phy_data);
		phy_data &= 0x0fff;
		phy_data |= (1 << 14);
		e1e_wphy(hw, 0x10, 0x2823);
		e1e_wphy(hw, 0x11, 0x0003);
		e1e_wphy(hw, 22, phy_data);
	}

	/* Setup buffer sizes */
	rctl &= ~E1000_RCTL_SZ_4096;
	rctl |= E1000_RCTL_BSEX;
	switch (adapter->rx_buffer_len) {
	case 256:
		rctl |= E1000_RCTL_SZ_256;
		rctl &= ~E1000_RCTL_BSEX;
		break;
	case 512:
		rctl |= E1000_RCTL_SZ_512;
		rctl &= ~E1000_RCTL_BSEX;
		break;
	case 1024:
		rctl |= E1000_RCTL_SZ_1024;
		rctl &= ~E1000_RCTL_BSEX;
		break;
	case 2048:
	default:
		rctl |= E1000_RCTL_SZ_2048;
		rctl &= ~E1000_RCTL_BSEX;
		break;
	case 4096:
		rctl |= E1000_RCTL_SZ_4096;
		break;
	case 8192:
		rctl |= E1000_RCTL_SZ_8192;
		break;
	case 16384:
		rctl |= E1000_RCTL_SZ_16384;
		break;
	}

	/*
	 * 82571 and greater support packet-split where the protocol
	 * header is placed in skb->data and the packet data is
	 * placed in pages hanging off of skb_shinfo(skb)->nr_frags.
	 * In the case of a non-split, skb->data is linearly filled,
	 * followed by the page buffers.  Therefore, skb->data is
	 * sized to hold the largest protocol header.
	 *
	 * allocations using alloc_page take too long for regular MTU
	 * so only enable packet split for jumbo frames
	 *
	 * Using pages when the page size is greater than 16k wastes
	 * a lot of memory, since we allocate 3 pages at all times
	 * per packet.
	 */
	pages = PAGE_USE_COUNT(adapter->netdev->mtu);
	if (!(adapter->flags & FLAG_IS_ICH) && (pages <= 3) &&
	    (PAGE_SIZE <= 16384) && (rctl & E1000_RCTL_LPE))
		adapter->rx_ps_pages = pages;
	else
		adapter->rx_ps_pages = 0;

	if (adapter->rx_ps_pages) {
		/* Configure extra packet-split registers */
		rfctl = er32(RFCTL);
		rfctl |= E1000_RFCTL_EXTEN;
		/*
		 * disable packet split support for IPv6 extension headers,
		 * because some malformed IPv6 headers can hang the Rx
		 */
		rfctl |= (E1000_RFCTL_IPV6_EX_DIS |
			  E1000_RFCTL_NEW_IPV6_EXT_DIS);

		ew32(RFCTL, rfctl);

		/* Enable Packet split descriptors */
		rctl |= E1000_RCTL_DTYP_PS;

		psrctl |= adapter->rx_ps_bsize0 >>
			E1000_PSRCTL_BSIZE0_SHIFT;

		switch (adapter->rx_ps_pages) {
		case 3:
			psrctl |= PAGE_SIZE <<
				E1000_PSRCTL_BSIZE3_SHIFT;
		case 2:
			psrctl |= PAGE_SIZE <<
				E1000_PSRCTL_BSIZE2_SHIFT;
		case 1:
			psrctl |= PAGE_SIZE >>
				E1000_PSRCTL_BSIZE1_SHIFT;
			break;
		}

		ew32(PSRCTL, psrctl);
	}

	ew32(RCTL, rctl);
	/* just started the receive unit, no need to restart */
	adapter->flags &= ~FLAG_RX_RESTART_NOW;
}

/**
 * e1000_configure_rx - Configure Receive Unit after Reset
 * @adapter: board private structure
 *
 * Configure the Rx unit of the MAC after a reset.
 **/
static void e1000_configure_rx(struct e1000_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	struct e1000_ring *rx_ring = adapter->rx_ring;
	u64 rdba;
	u32 rdlen, rctl, rxcsum, ctrl_ext;

	if (adapter->rx_ps_pages) {
		/* this is a 32 byte descriptor */
		rdlen = rx_ring->count *
			sizeof(union e1000_rx_desc_packet_split);
		adapter->clean_rx = e1000_clean_rx_irq_ps;
		adapter->alloc_rx_buf = e1000_alloc_rx_buffers_ps;
	} else if (adapter->netdev->mtu > ETH_FRAME_LEN + ETH_FCS_LEN) {
		rdlen = rx_ring->count * sizeof(struct e1000_rx_desc);
		adapter->clean_rx = e1000_clean_jumbo_rx_irq;
		adapter->alloc_rx_buf = e1000_alloc_jumbo_rx_buffers;
	} else {
		rdlen = rx_ring->count * sizeof(struct e1000_rx_desc);
		adapter->clean_rx = e1000_clean_rx_irq;
		adapter->alloc_rx_buf = e1000_alloc_rx_buffers;
	}

	/* disable receives while setting up the descriptors */
	rctl = er32(RCTL);
	ew32(RCTL, rctl & ~E1000_RCTL_EN);
	e1e_flush();
	msleep(10);

	/* set the Receive Delay Timer Register */
	ew32(RDTR, adapter->rx_int_delay);

	/* irq moderation */
	ew32(RADV, adapter->rx_abs_int_delay);
	if (adapter->itr_setting != 0)
		ew32(ITR, 1000000000 / (adapter->itr * 256));

	ctrl_ext = er32(CTRL_EXT);
	/* Reset delay timers after every interrupt */
	ctrl_ext |= E1000_CTRL_EXT_INT_TIMER_CLR;
	/* Auto-Mask interrupts upon ICR access */
	ctrl_ext |= E1000_CTRL_EXT_IAME;
	ew32(IAM, 0xffffffff);
	ew32(CTRL_EXT, ctrl_ext);
	e1e_flush();

	/*
	 * Setup the HW Rx Head and Tail Descriptor Pointers and
	 * the Base and Length of the Rx Descriptor Ring
	 */
	rdba = rx_ring->dma;
	ew32(RDBAL, (rdba & DMA_BIT_MASK(32)));
	ew32(RDBAH, (rdba >> 32));
	ew32(RDLEN, rdlen);
	ew32(RDH, 0);
	ew32(RDT, 0);
	rx_ring->head = E1000_RDH;
	rx_ring->tail = E1000_RDT;

	/* Enable Receive Checksum Offload for TCP and UDP */
	rxcsum = er32(RXCSUM);
	if (adapter->flags & FLAG_RX_CSUM_ENABLED) {
		rxcsum |= E1000_RXCSUM_TUOFL;

		/*
		 * IPv4 payload checksum for UDP fragments must be
		 * used in conjunction with packet-split.
		 */
		if (adapter->rx_ps_pages)
			rxcsum |= E1000_RXCSUM_IPPCSE;
	} else {
		rxcsum &= ~E1000_RXCSUM_TUOFL;
		/* no need to clear IPPCSE as it defaults to 0 */
	}
	ew32(RXCSUM, rxcsum);

	/*
	 * Enable early receives on supported devices, only takes effect when
	 * packet size is equal or larger than the specified value (in 8 byte
	 * units), e.g. using jumbo frames when setting to E1000_ERT_2048
	 */
	if ((adapter->flags & FLAG_HAS_ERT) &&
	    (adapter->netdev->mtu > ETH_DATA_LEN)) {
		u32 rxdctl = er32(RXDCTL(0));
		ew32(RXDCTL(0), rxdctl | 0x3);
		ew32(ERT, E1000_ERT_2048 | (1 << 13));
		/*
		 * With jumbo frames and early-receive enabled, excessive
		 * C4->C2 latencies result in dropped transactions.
		 */
		pm_qos_update_requirement(PM_QOS_CPU_DMA_LATENCY,
					  e1000e_driver_name, 55);
	} else {
		pm_qos_update_requirement(PM_QOS_CPU_DMA_LATENCY,
					  e1000e_driver_name,
					  PM_QOS_DEFAULT_VALUE);
	}

	/* Enable Receives */
	ew32(RCTL, rctl);
}

/**
 *  e1000_update_mc_addr_list - Update Multicast addresses
 *  @hw: pointer to the HW structure
 *  @mc_addr_list: array of multicast addresses to program
 *  @mc_addr_count: number of multicast addresses to program
 *  @rar_used_count: the first RAR register free to program
 *  @rar_count: total number of supported Receive Address Registers
 *
 *  Updates the Receive Address Registers and Multicast Table Array.
 *  The caller must have a packed mc_addr_list of multicast addresses.
 *  The parameter rar_count will usually be hw->mac.rar_entry_count
 *  unless there are workarounds that change this.  Currently no func pointer
 *  exists and all implementations are handled in the generic version of this
 *  function.
 **/
static void e1000_update_mc_addr_list(struct e1000_hw *hw, u8 *mc_addr_list,
				      u32 mc_addr_count, u32 rar_used_count,
				      u32 rar_count)
{
	hw->mac.ops.update_mc_addr_list(hw, mc_addr_list, mc_addr_count,
				        rar_used_count, rar_count);
}

/**
 * e1000_set_multi - Multicast and Promiscuous mode set
 * @netdev: network interface device structure
 *
 * The set_multi entry point is called whenever the multicast address
 * list or the network interface flags are updated.  This routine is
 * responsible for configuring the hardware for proper multicast,
 * promiscuous mode, and all-multi behavior.
 **/
static void e1000_set_multi(struct net_device *netdev)
{
	struct e1000_adapter *adapter = netdev_priv(netdev);
	struct e1000_hw *hw = &adapter->hw;
	struct e1000_mac_info *mac = &hw->mac;
	struct dev_mc_list *mc_ptr;
	u8  *mta_list;
	u32 rctl;
	int i;

	/* Check for Promiscuous and All Multicast modes */

	rctl = er32(RCTL);

	if (netdev->flags & IFF_PROMISC) {
		rctl |= (E1000_RCTL_UPE | E1000_RCTL_MPE);
		rctl &= ~E1000_RCTL_VFE;
	} else {
		if (netdev->flags & IFF_ALLMULTI) {
			rctl |= E1000_RCTL_MPE;
			rctl &= ~E1000_RCTL_UPE;
		} else {
			rctl &= ~(E1000_RCTL_UPE | E1000_RCTL_MPE);
		}
		if (adapter->flags & FLAG_HAS_HW_VLAN_FILTER)
			rctl |= E1000_RCTL_VFE;
	}

	ew32(RCTL, rctl);

	if (netdev->mc_count) {
		mta_list = kmalloc(netdev->mc_count * 6, GFP_ATOMIC);
		if (!mta_list)
			return;

		/* prepare a packed array of only addresses. */
		mc_ptr = netdev->mc_list;

		for (i = 0; i < netdev->mc_count; i++) {
			if (!mc_ptr)
				break;
			memcpy(mta_list + (i*ETH_ALEN), mc_ptr->dmi_addr,
			       ETH_ALEN);
			mc_ptr = mc_ptr->next;
		}

		e1000_update_mc_addr_list(hw, mta_list, i, 1,
					  mac->rar_entry_count);
		kfree(mta_list);
	} else {
		/*
		 * if we're called from probe, we might not have
		 * anything to do here, so clear out the list
		 */
		e1000_update_mc_addr_list(hw, NULL, 0, 1, mac->rar_entry_count);
	}
}

/**
 * e1000_configure - configure the hardware for Rx and Tx
 * @adapter: private board structure
 **/
static void e1000_configure(struct e1000_adapter *adapter)
{
	e1000_set_multi(adapter->netdev);

	e1000_restore_vlan(adapter);
	e1000_init_manageability(adapter);

	e1000_configure_tx(adapter);
	e1000_setup_rctl(adapter);
	e1000_configure_rx(adapter);
	adapter->alloc_rx_buf(adapter, e1000_desc_unused(adapter->rx_ring));
}

/**
 * e1000e_power_up_phy - restore link in case the phy was powered down
 * @adapter: address of board private structure
 *
 * The phy may be powered down to save power and turn off link when the
 * driver is unloaded and wake on lan is not enabled (among others)
 * *** this routine MUST be followed by a call to e1000e_reset ***
 **/
void e1000e_power_up_phy(struct e1000_adapter *adapter)
{
	u16 mii_reg = 0;

	/* Just clear the power down bit to wake the phy back up */
	if (adapter->hw.phy.media_type == e1000_media_type_copper) {
		/*
		 * According to the manual, the phy will retain its
		 * settings across a power-down/up cycle
		 */
		e1e_rphy(&adapter->hw, PHY_CONTROL, &mii_reg);
		mii_reg &= ~MII_CR_POWER_DOWN;
		e1e_wphy(&adapter->hw, PHY_CONTROL, mii_reg);
	}

	adapter->hw.mac.ops.setup_link(&adapter->hw);
}

/**
 * e1000_power_down_phy - Power down the PHY
 *
 * Power down the PHY so no link is implied when interface is down
 * The PHY cannot be powered down is management or WoL is active
 */
static void e1000_power_down_phy(struct e1000_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	u16 mii_reg;

	/* WoL is enabled */
	if (adapter->wol)
		return;

	/* non-copper PHY? */
	if (adapter->hw.phy.media_type != e1000_media_type_copper)
		return;

	/* reset is blocked because of a SoL/IDER session */
	if (e1000e_check_mng_mode(hw) || e1000_check_reset_block(hw))
		return;

	/* manageability (AMT) is enabled */
	if (er32(MANC) & E1000_MANC_SMBUS_EN)
		return;

	/* power down the PHY */
	e1e_rphy(hw, PHY_CONTROL, &mii_reg);
	mii_reg |= MII_CR_POWER_DOWN;
	e1e_wphy(hw, PHY_CONTROL, mii_reg);
	mdelay(1);
}

/**
 * e1000e_reset - bring the hardware into a known good state
 *
 * This function boots the hardware and enables some settings that
 * require a configuration cycle of the hardware - those cannot be
 * set/changed during runtime. After reset the device needs to be
 * properly configured for Rx, Tx etc.
 */
void e1000e_reset(struct e1000_adapter *adapter)
{
	struct e1000_mac_info *mac = &adapter->hw.mac;
	struct e1000_fc_info *fc = &adapter->hw.fc;
	struct e1000_hw *hw = &adapter->hw;
	u32 tx_space, min_tx_space, min_rx_space;
	u32 pba = adapter->pba;
	u16 hwm;

	/* reset Packet Buffer Allocation to default */
	ew32(PBA, pba);

	if (adapter->max_frame_size > ETH_FRAME_LEN + ETH_FCS_LEN) {
		/*
		 * To maintain wire speed transmits, the Tx FIFO should be
		 * large enough to accommodate two full transmit packets,
		 * rounded up to the next 1KB and expressed in KB.  Likewise,
		 * the Rx FIFO should be large enough to accommodate at least
		 * one full receive packet and is similarly rounded up and
		 * expressed in KB.
		 */
		pba = er32(PBA);
		/* upper 16 bits has Tx packet buffer allocation size in KB */
		tx_space = pba >> 16;
		/* lower 16 bits has Rx packet buffer allocation size in KB */
		pba &= 0xffff;
		/*
		 * the Tx fifo also stores 16 bytes of information about the tx
		 * but don't include ethernet FCS because hardware appends it
		 */
		min_tx_space = (adapter->max_frame_size +
				sizeof(struct e1000_tx_desc) -
				ETH_FCS_LEN) * 2;
		min_tx_space = ALIGN(min_tx_space, 1024);
		min_tx_space >>= 10;
		/* software strips receive CRC, so leave room for it */
		min_rx_space = adapter->max_frame_size;
		min_rx_space = ALIGN(min_rx_space, 1024);
		min_rx_space >>= 10;

		/*
		 * If current Tx allocation is less than the min Tx FIFO size,
		 * and the min Tx FIFO size is less than the current Rx FIFO
		 * allocation, take space away from current Rx allocation
		 */
		if ((tx_space < min_tx_space) &&
		    ((min_tx_space - tx_space) < pba)) {
			pba -= min_tx_space - tx_space;

			/*
			 * if short on Rx space, Rx wins and must trump tx
			 * adjustment or use Early Receive if available
			 */
			if ((pba < min_rx_space) &&
			    (!(adapter->flags & FLAG_HAS_ERT)))
				/* ERT enabled in e1000_configure_rx */
				pba = min_rx_space;
		}

		ew32(PBA, pba);
	}


	/*
	 * flow control settings
	 *
	 * The high water mark must be low enough to fit one full frame
	 * (or the size used for early receive) above it in the Rx FIFO.
	 * Set it to the lower of:
	 * - 90% of the Rx FIFO size, and
	 * - the full Rx FIFO size minus the early receive size (for parts
	 *   with ERT support assuming ERT set to E1000_ERT_2048), or
	 * - the full Rx FIFO size minus one full frame
	 */
	if (hw->mac.type == e1000_pchlan) {
		/*
		 * Workaround PCH LOM adapter hangs with certain network
		 * loads.  If hangs persist, try disabling Tx flow control.
		 */
		if (adapter->netdev->mtu > ETH_DATA_LEN) {
			fc->high_water = 0x3500;
			fc->low_water  = 0x1500;
		} else {
			fc->high_water = 0x5000;
			fc->low_water  = 0x3000;
		}
	} else {
		if ((adapter->flags & FLAG_HAS_ERT) &&
		    (adapter->netdev->mtu > ETH_DATA_LEN))
			hwm = min(((pba << 10) * 9 / 10),
				  ((pba << 10) - (E1000_ERT_2048 << 3)));
		else
			hwm = min(((pba << 10) * 9 / 10),
				  ((pba << 10) - adapter->max_frame_size));

		fc->high_water = hwm & E1000_FCRTH_RTH; /* 8-byte granularity */
		fc->low_water = fc->high_water - 8;
	}

	if (adapter->flags & FLAG_DISABLE_FC_PAUSE_TIME)
		fc->pause_time = 0xFFFF;
	else
		fc->pause_time = E1000_FC_PAUSE_TIME;
	fc->send_xon = 1;
	fc->current_mode = fc->requested_mode;

	/* Allow time for pending master requests to run */
	mac->ops.reset_hw(hw);

	/*
	 * For parts with AMT enabled, let the firmware know
	 * that the network interface is in control
	 */
	if (adapter->flags & FLAG_HAS_AMT)
		e1000_get_hw_control(adapter);

	ew32(WUC, 0);
	if (adapter->flags2 & FLAG2_HAS_PHY_WAKEUP)
		e1e_wphy(&adapter->hw, BM_WUC, 0);

	if (mac->ops.init_hw(hw))
		e_err("Hardware Error\n");

	/* additional part of the flow-control workaround above */
	if (hw->mac.type == e1000_pchlan)
		ew32(FCRTV_PCH, 0x1000);

	e1000_update_mng_vlan(adapter);

	/* Enable h/w to recognize an 802.1Q VLAN Ethernet packet */
	ew32(VET, ETH_P_8021Q);

	e1000e_reset_adaptive(hw);
	e1000_get_phy_info(hw);

	if ((adapter->flags & FLAG_HAS_SMART_POWER_DOWN) &&
	    !(adapter->flags & FLAG_SMART_POWER_DOWN)) {
		u16 phy_data = 0;
		/*
		 * speed up time to link by disabling smart power down, ignore
		 * the return value of this function because there is nothing
		 * different we would do if it failed
		 */
		e1e_rphy(hw, IGP02E1000_PHY_POWER_MGMT, &phy_data);
		phy_data &= ~IGP02E1000_PM_SPD;
		e1e_wphy(hw, IGP02E1000_PHY_POWER_MGMT, phy_data);
	}
}

int e1000e_up(struct e1000_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;

	/* hardware has been reset, we need to reload some things */
	e1000_configure(adapter);

	clear_bit(__E1000_DOWN, &adapter->state);

	napi_enable(&adapter->napi);
	if (adapter->msix_entries)
		e1000_configure_msix(adapter);
	e1000_irq_enable(adapter);

	netif_wake_queue(adapter->netdev);

	/* fire a link change interrupt to start the watchdog */
	ew32(ICS, E1000_ICS_LSC);
	return 0;
}

void e1000e_down(struct e1000_adapter *adapter)
{
	struct net_device *netdev = adapter->netdev;
	struct e1000_hw *hw = &adapter->hw;
	u32 tctl, rctl;

	/*
	 * signal that we're down so the interrupt handler does not
	 * reschedule our watchdog timer
	 */
	set_bit(__E1000_DOWN, &adapter->state);

	/* disable receives in the hardware */
	rctl = er32(RCTL);
	ew32(RCTL, rctl & ~E1000_RCTL_EN);
	/* flush and sleep below */

	netif_stop_queue(netdev);

	/* disable transmits in the hardware */
	tctl = er32(TCTL);
	tctl &= ~E1000_TCTL_EN;
	ew32(TCTL, tctl);
	/* flush both disables and wait for them to finish */
	e1e_flush();
	msleep(10);

	napi_disable(&adapter->napi);
	e1000_irq_disable(adapter);

	del_timer_sync(&adapter->watchdog_timer);
	del_timer_sync(&adapter->phy_info_timer);

	netdev->tx_queue_len = adapter->tx_queue_len;
	netif_carrier_off(netdev);
	adapter->link_speed = 0;
	adapter->link_duplex = 0;

	if (!pci_channel_offline(adapter->pdev))
		e1000e_reset(adapter);
	e1000_clean_tx_ring(adapter);
	e1000_clean_rx_ring(adapter);

	/*
	 * TODO: for power management, we could drop the link and
	 * pci_disable_device here.
	 */
}

void e1000e_reinit_locked(struct e1000_adapter *adapter)
{
	might_sleep();
	while (test_and_set_bit(__E1000_RESETTING, &adapter->state))
		msleep(1);
	e1000e_down(adapter);
	e1000e_up(adapter);
	clear_bit(__E1000_RESETTING, &adapter->state);
}

/**
 * e1000_sw_init - Initialize general software structures (struct e1000_adapter)
 * @adapter: board private structure to initialize
 *
 * e1000_sw_init initializes the Adapter private data structure.
 * Fields are initialized based on PCI device information and
 * OS network device settings (MTU size).
 **/
static int __devinit e1000_sw_init(struct e1000_adapter *adapter)
{
	struct net_device *netdev = adapter->netdev;

	adapter->rx_buffer_len = ETH_FRAME_LEN + VLAN_HLEN + ETH_FCS_LEN;
	adapter->rx_ps_bsize0 = 128;
	adapter->max_frame_size = netdev->mtu + ETH_HLEN + ETH_FCS_LEN;
	adapter->min_frame_size = ETH_ZLEN + ETH_FCS_LEN;

	e1000e_set_interrupt_capability(adapter);

	if (e1000_alloc_queues(adapter))
		return -ENOMEM;

	/* Explicitly disable IRQ since the NIC can be in any state. */
	e1000_irq_disable(adapter);

	set_bit(__E1000_DOWN, &adapter->state);
	return 0;
}

/**
 * e1000_intr_msi_test - Interrupt Handler
 * @irq: interrupt number
 * @data: pointer to a network interface device structure
 **/
static irqreturn_t e1000_intr_msi_test(int irq, void *data)
{
	struct net_device *netdev = data;
	struct e1000_adapter *adapter = netdev_priv(netdev);
	struct e1000_hw *hw = &adapter->hw;
	u32 icr = er32(ICR);

	e_dbg("%s: icr is %08X\n", netdev->name, icr);
	if (icr & E1000_ICR_RXSEQ) {
		adapter->flags &= ~FLAG_MSI_TEST_FAILED;
		wmb();
	}

	return IRQ_HANDLED;
}

/**
 * e1000_test_msi_interrupt - Returns 0 for successful test
 * @adapter: board private struct
 *
 * code flow taken from tg3.c
 **/
static int e1000_test_msi_interrupt(struct e1000_adapter *adapter)
{
	struct net_device *netdev = adapter->netdev;
	struct e1000_hw *hw = &adapter->hw;
	int err;

	/* poll_enable hasn't been called yet, so don't need disable */
	/* clear any pending events */
	er32(ICR);

	/* free the real vector and request a test handler */
	e1000_free_irq(adapter);
	e1000e_reset_interrupt_capability(adapter);

	/* Assume that the test fails, if it succeeds then the test
	 * MSI irq handler will unset this flag */
	adapter->flags |= FLAG_MSI_TEST_FAILED;

	err = pci_enable_msi(adapter->pdev);
	if (err)
		goto msi_test_failed;

	err = request_irq(adapter->pdev->irq, &e1000_intr_msi_test, 0,
			  netdev->name, netdev);
	if (err) {
		pci_disable_msi(adapter->pdev);
		goto msi_test_failed;
	}

	wmb();

	e1000_irq_enable(adapter);

	/* fire an unusual interrupt on the test handler */
	ew32(ICS, E1000_ICS_RXSEQ);
	e1e_flush();
	msleep(50);

	e1000_irq_disable(adapter);

	rmb();

	if (adapter->flags & FLAG_MSI_TEST_FAILED) {
		adapter->int_mode = E1000E_INT_MODE_LEGACY;
		err = -EIO;
		e_info("MSI interrupt test failed!\n");
	}

	free_irq(adapter->pdev->irq, netdev);
	pci_disable_msi(adapter->pdev);

	if (err == -EIO)
		goto msi_test_failed;

	/* okay so the test worked, restore settings */
	e_dbg("%s: MSI interrupt test succeeded!\n", netdev->name);
msi_test_failed:
	e1000e_set_interrupt_capability(adapter);
	e1000_request_irq(adapter);
	return err;
}

/**
 * e1000_test_msi - Returns 0 if MSI test succeeds or INTx mode is restored
 * @adapter: board private struct
 *
 * code flow taken from tg3.c, called with e1000 interrupts disabled.
 **/
static int e1000_test_msi(struct e1000_adapter *adapter)
{
	int err;
	u16 pci_cmd;

	if (!(adapter->flags & FLAG_MSI_ENABLED))
		return 0;

	/* disable SERR in case the MSI write causes a master abort */
	pci_read_config_word(adapter->pdev, PCI_COMMAND, &pci_cmd);
	pci_write_config_word(adapter->pdev, PCI_COMMAND,
			      pci_cmd & ~PCI_COMMAND_SERR);

	err = e1000_test_msi_interrupt(adapter);

	/* restore previous setting of command word */
	pci_write_config_word(adapter->pdev, PCI_COMMAND, pci_cmd);

	/* success ! */
	if (!err)
		return 0;

	/* EIO means MSI test failed */
	if (err != -EIO)
		return err;

	/* back to INTx mode */
	e_warn("MSI interrupt test failed, using legacy interrupt.\n");

	e1000_free_irq(adapter);

	err = e1000_request_irq(adapter);

	return err;
}

/**
 * e1000_open - Called when a network interface is made active
 * @netdev: network interface device structure
 *
 * Returns 0 on success, negative value on failure
 *
 * The open entry point is called when a network interface is made
 * active by the system (IFF_UP).  At this point all resources needed
 * for transmit and receive operations are allocated, the interrupt
 * handler is registered with the OS, the watchdog timer is started,
 * and the stack is notified that the interface is ready.
 **/
static int e1000_open(struct net_device *netdev)
{
	struct e1000_adapter *adapter = netdev_priv(netdev);
	struct e1000_hw *hw = &adapter->hw;
	int err;

	/* disallow open during test */
	if (test_bit(__E1000_TESTING, &adapter->state))
		return -EBUSY;

	netif_carrier_off(netdev);

	/* allocate transmit descriptors */
	err = e1000e_setup_tx_resources(adapter);
	if (err)
		goto err_setup_tx;

	/* allocate receive descriptors */
	err = e1000e_setup_rx_resources(adapter);
	if (err)
		goto err_setup_rx;

	e1000e_power_up_phy(adapter);

	adapter->mng_vlan_id = E1000_MNG_VLAN_NONE;
	if ((adapter->hw.mng_cookie.status &
	     E1000_MNG_DHCP_COOKIE_STATUS_VLAN))
		e1000_update_mng_vlan(adapter);

	/*
	 * If AMT is enabled, let the firmware know that the network
	 * interface is now open
	 */
	if (adapter->flags & FLAG_HAS_AMT)
		e1000_get_hw_control(adapter);

	/*
	 * before we allocate an interrupt, we must be ready to handle it.
	 * Setting DEBUG_SHIRQ in the kernel makes it fire an interrupt
	 * as soon as we call pci_request_irq, so we have to setup our
	 * clean_rx handler before we do so.
	 */
	e1000_configure(adapter);

	err = e1000_request_irq(adapter);
	if (err)
		goto err_req_irq;

	/*
	 * Work around PCIe errata with MSI interrupts causing some chipsets to
	 * ignore e1000e MSI messages, which means we need to test our MSI
	 * interrupt now
	 */
	if (adapter->int_mode != E1000E_INT_MODE_LEGACY) {
		err = e1000_test_msi(adapter);
		if (err) {
			e_err("Interrupt allocation failed\n");
			goto err_req_irq;
		}
	}

	/* From here on the code is the same as e1000e_up() */
	clear_bit(__E1000_DOWN, &adapter->state);

	napi_enable(&adapter->napi);

	e1000_irq_enable(adapter);

	netif_start_queue(netdev);

	/* fire a link status change interrupt to start the watchdog */
	ew32(ICS, E1000_ICS_LSC);

	return 0;

err_req_irq:
	e1000_release_hw_control(adapter);
	e1000_power_down_phy(adapter);
	e1000e_free_rx_resources(adapter);
err_setup_rx:
	e1000e_free_tx_resources(adapter);
err_setup_tx:
	e1000e_reset(adapter);

	return err;
}

/**
 * e1000_close - Disables a network interface
 * @netdev: network interface device structure
 *
 * Returns 0, this is not allowed to fail
 *
 * The close entry point is called when an interface is de-activated
 * by the OS.  The hardware is still under the drivers control, but
 * needs to be disabled.  A global MAC reset is issued to stop the
 * hardware, and all transmit and receive resources are freed.
 **/
static int e1000_close(struct net_device *netdev)
{
	struct e1000_adapter *adapter = netdev_priv(netdev);

	WARN_ON(test_bit(__E1000_RESETTING, &adapter->state));
	e1000e_down(adapter);
	e1000_power_down_phy(adapter);
	e1000_free_irq(adapter);

	e1000e_free_tx_resources(adapter);
	e1000e_free_rx_resources(adapter);

	/*
	 * kill manageability vlan ID if supported, but not if a vlan with
	 * the same ID is registered on the host OS (let 8021q kill it)
	 */
	if ((adapter->hw.mng_cookie.status &
			  E1000_MNG_DHCP_COOKIE_STATUS_VLAN) &&
	     !(adapter->vlgrp &&
	       vlan_group_get_device(adapter->vlgrp, adapter->mng_vlan_id)))
		e1000_vlan_rx_kill_vid(netdev, adapter->mng_vlan_id);

	/*
	 * If AMT is enabled, let the firmware know that the network
	 * interface is now closed
	 */
	if (adapter->flags & FLAG_HAS_AMT)
		e1000_release_hw_control(adapter);

	return 0;
}
/**
 * e1000_set_mac - Change the Ethernet Address of the NIC
 * @netdev: network interface device structure
 * @p: pointer to an address structure
 *
 * Returns 0 on success, negative on failure
 **/
static int e1000_set_mac(struct net_device *netdev, void *p)
{
	struct e1000_adapter *adapter = netdev_priv(netdev);
	struct sockaddr *addr = p;

	if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;

	memcpy(netdev->dev_addr, addr->sa_data, netdev->addr_len);
	memcpy(adapter->hw.mac.addr, addr->sa_data, netdev->addr_len);

	e1000e_rar_set(&adapter->hw, adapter->hw.mac.addr, 0);

	if (adapter->flags & FLAG_RESET_OVERWRITES_LAA) {
		/* activate the work around */
		e1000e_set_laa_state_82571(&adapter->hw, 1);

		/*
		 * Hold a copy of the LAA in RAR[14] This is done so that
		 * between the time RAR[0] gets clobbered  and the time it
		 * gets fixed (in e1000_watchdog), the actual LAA is in one
		 * of the RARs and no incoming packets directed to this port
		 * are dropped. Eventually the LAA will be in RAR[0] and
		 * RAR[14]
		 */
		e1000e_rar_set(&adapter->hw,
			      adapter->hw.mac.addr,
			      adapter->hw.mac.rar_entry_count - 1);
	}

	return 0;
}

/**
 * e1000e_update_phy_task - work thread to update phy
 * @work: pointer to our work struct
 *
 * this worker thread exists because we must acquire a
 * semaphore to read the phy, which we could msleep while
 * waiting for it, and we can't msleep in a timer.
 **/
static void e1000e_update_phy_task(struct work_struct *work)
{
	struct e1000_adapter *adapter = container_of(work,
					struct e1000_adapter, update_phy_task);
	e1000_get_phy_info(&adapter->hw);
}

/*
 * Need to wait a few seconds after link up to get diagnostic information from
 * the phy
 */
static void e1000_update_phy_info(unsigned long data)
{
	struct e1000_adapter *adapter = (struct e1000_adapter *) data;
	schedule_work(&adapter->update_phy_task);
}

/**
 * e1000e_update_stats - Update the board statistics counters
 * @adapter: board private structure
 **/
void e1000e_update_stats(struct e1000_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	struct pci_dev *pdev = adapter->pdev;
	u16 phy_data;

	/*
	 * Prevent stats update while adapter is being reset, or if the pci
	 * connection is down.
	 */
	if (adapter->link_speed == 0)
		return;
	if (pci_channel_offline(pdev))
		return;

	adapter->stats.crcerrs += er32(CRCERRS);
	adapter->stats.gprc += er32(GPRC);
	adapter->stats.gorc += er32(GORCL);
	er32(GORCH); /* Clear gorc */
	adapter->stats.bprc += er32(BPRC);
	adapter->stats.mprc += er32(MPRC);
	adapter->stats.roc += er32(ROC);

	adapter->stats.mpc += er32(MPC);
	if ((hw->phy.type == e1000_phy_82578) ||
	    (hw->phy.type == e1000_phy_82577)) {
		e1e_rphy(hw, HV_SCC_UPPER, &phy_data);
		e1e_rphy(hw, HV_SCC_LOWER, &phy_data);
		adapter->stats.scc += phy_data;

		e1e_rphy(hw, HV_ECOL_UPPER, &phy_data);
		e1e_rphy(hw, HV_ECOL_LOWER, &phy_data);
		adapter->stats.ecol += phy_data;

		e1e_rphy(hw, HV_MCC_UPPER, &phy_data);
		e1e_rphy(hw, HV_MCC_LOWER, &phy_data);
		adapter->stats.mcc += phy_data;

		e1e_rphy(hw, HV_LATECOL_UPPER, &phy_data);
		e1e_rphy(hw, HV_LATECOL_LOWER, &phy_data);
		adapter->stats.latecol += phy_data;

		e1e_rphy(hw, HV_DC_UPPER, &phy_data);
		e1e_rphy(hw, HV_DC_LOWER, &phy_data);
		adapter->stats.dc += phy_data;
	} else {
		adapter->stats.scc += er32(SCC);
		adapter->stats.ecol += er32(ECOL);
		adapter->stats.mcc += er32(MCC);
		adapter->stats.latecol += er32(LATECOL);
		adapter->stats.dc += er32(DC);
	}
	adapter->stats.xonrxc += er32(XONRXC);
	adapter->stats.xontxc += er32(XONTXC);
	adapter->stats.xoffrxc += er32(XOFFRXC);
	adapter->stats.xofftxc += er32(XOFFTXC);
	adapter->stats.gptc += er32(GPTC);
	adapter->stats.gotc += er32(GOTCL);
	er32(GOTCH); /* Clear gotc */
	adapter->stats.rnbc += er32(RNBC);
	adapter->stats.ruc += er32(RUC);

	adapter->stats.mptc += er32(MPTC);
	adapter->stats.bptc += er32(BPTC);

	/* used for adaptive IFS */

	hw->mac.tx_packet_delta = er32(TPT);
	adapter->stats.tpt += hw->mac.tx_packet_delta;
	if ((hw->phy.type == e1000_phy_82578) ||
	    (hw->phy.type == e1000_phy_82577)) {
		e1e_rphy(hw, HV_COLC_UPPER, &phy_data);
		e1e_rphy(hw, HV_COLC_LOWER, &phy_data);
		hw->mac.collision_delta = phy_data;
	} else {
		hw->mac.collision_delta = er32(COLC);
	}
	adapter->stats.colc += hw->mac.collision_delta;

	adapter->stats.algnerrc += er32(ALGNERRC);
	adapter->stats.rxerrc += er32(RXERRC);
	if ((hw->phy.type == e1000_phy_82578) ||
	    (hw->phy.type == e1000_phy_82577)) {
		e1e_rphy(hw, HV_TNCRS_UPPER, &phy_data);
		e1e_rphy(hw, HV_TNCRS_LOWER, &phy_data);
		adapter->stats.tncrs += phy_data;
	} else {
		if ((hw->mac.type != e1000_82574) &&
		    (hw->mac.type != e1000_82583))
			adapter->stats.tncrs += er32(TNCRS);
	}
	adapter->stats.cexterr += er32(CEXTERR);
	adapter->stats.tsctc += er32(TSCTC);
	adapter->stats.tsctfc += er32(TSCTFC);

	/* Fill out the OS statistics structure */
	adapter->net_stats.multicast = adapter->stats.mprc;
	adapter->net_stats.collisions = adapter->stats.colc;

	/* Rx Errors */

	/*
	 * RLEC on some newer hardware can be incorrect so build
	 * our own version based on RUC and ROC
	 */
	adapter->net_stats.rx_errors = adapter->stats.rxerrc +
		adapter->stats.crcerrs + adapter->stats.algnerrc +
		adapter->stats.ruc + adapter->stats.roc +
		adapter->stats.cexterr;
	adapter->net_stats.rx_length_errors = adapter->stats.ruc +
					      adapter->stats.roc;
	adapter->net_stats.rx_crc_errors = adapter->stats.crcerrs;
	adapter->net_stats.rx_frame_errors = adapter->stats.algnerrc;
	adapter->net_stats.rx_missed_errors = adapter->stats.mpc;

	/* Tx Errors */
	adapter->net_stats.tx_errors = adapter->stats.ecol +
				       adapter->stats.latecol;
	adapter->net_stats.tx_aborted_errors = adapter->stats.ecol;
	adapter->net_stats.tx_window_errors = adapter->stats.latecol;
	adapter->net_stats.tx_carrier_errors = adapter->stats.tncrs;

	/* Tx Dropped needs to be maintained elsewhere */

	/* Management Stats */
	adapter->stats.mgptc += er32(MGTPTC);
	adapter->stats.mgprc += er32(MGTPRC);
	adapter->stats.mgpdc += er32(MGTPDC);
}

/**
 * e1000_phy_read_status - Update the PHY register status snapshot
 * @adapter: board private structure
 **/
static void e1000_phy_read_status(struct e1000_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	struct e1000_phy_regs *phy = &adapter->phy_regs;
	int ret_val;

	if ((er32(STATUS) & E1000_STATUS_LU) &&
	    (adapter->hw.phy.media_type == e1000_media_type_copper)) {
		ret_val  = e1e_rphy(hw, PHY_CONTROL, &phy->bmcr);
		ret_val |= e1e_rphy(hw, PHY_STATUS, &phy->bmsr);
		ret_val |= e1e_rphy(hw, PHY_AUTONEG_ADV, &phy->advertise);
		ret_val |= e1e_rphy(hw, PHY_LP_ABILITY, &phy->lpa);
		ret_val |= e1e_rphy(hw, PHY_AUTONEG_EXP, &phy->expansion);
		ret_val |= e1e_rphy(hw, PHY_1000T_CTRL, &phy->ctrl1000);
		ret_val |= e1e_rphy(hw, PHY_1000T_STATUS, &phy->stat1000);
		ret_val |= e1e_rphy(hw, PHY_EXT_STATUS, &phy->estatus);
		if (ret_val)
			e_warn("Error reading PHY register\n");
	} else {
		/*
		 * Do not read PHY registers if link is not up
		 * Set values to typical power-on defaults
		 */
		phy->bmcr = (BMCR_SPEED1000 | BMCR_ANENABLE | BMCR_FULLDPLX);
		phy->bmsr = (BMSR_100FULL | BMSR_100HALF | BMSR_10FULL |
			     BMSR_10HALF | BMSR_ESTATEN | BMSR_ANEGCAPABLE |
			     BMSR_ERCAP);
		phy->advertise = (ADVERTISE_PAUSE_ASYM | ADVERTISE_PAUSE_CAP |
				  ADVERTISE_ALL | ADVERTISE_CSMA);
		phy->lpa = 0;
		phy->expansion = EXPANSION_ENABLENPAGE;
		phy->ctrl1000 = ADVERTISE_1000FULL;
		phy->stat1000 = 0;
		phy->estatus = (ESTATUS_1000_TFULL | ESTATUS_1000_THALF);
	}
}

static void e1000_print_link_info(struct e1000_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	u32 ctrl = er32(CTRL);

	/* Link status message must follow this format for user tools */
	printk(KERN_INFO "e1000e: %s NIC Link is Up %d Mbps %s, "
	       "Flow Control: %s\n",
	       adapter->netdev->name,
	       adapter->link_speed,
	       (adapter->link_duplex == FULL_DUPLEX) ?
	                        "Full Duplex" : "Half Duplex",
	       ((ctrl & E1000_CTRL_TFCE) && (ctrl & E1000_CTRL_RFCE)) ?
	                        "RX/TX" :
	       ((ctrl & E1000_CTRL_RFCE) ? "RX" :
	       ((ctrl & E1000_CTRL_TFCE) ? "TX" : "None" )));
}

bool e1000_has_link(struct e1000_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	bool link_active = 0;
	s32 ret_val = 0;

	/*
	 * get_link_status is set on LSC (link status) interrupt or
	 * Rx sequence error interrupt.  get_link_status will stay
	 * false until the check_for_link establishes link
	 * for copper adapters ONLY
	 */
	switch (hw->phy.media_type) {
	case e1000_media_type_copper:
		if (hw->mac.get_link_status) {
			ret_val = hw->mac.ops.check_for_link(hw);
			link_active = !hw->mac.get_link_status;
		} else {
			link_active = 1;
		}
		break;
	case e1000_media_type_fiber:
		ret_val = hw->mac.ops.check_for_link(hw);
		link_active = !!(er32(STATUS) & E1000_STATUS_LU);
		break;
	case e1000_media_type_internal_serdes:
		ret_val = hw->mac.ops.check_for_link(hw);
		link_active = adapter->hw.mac.serdes_has_link;
		break;
	default:
	case e1000_media_type_unknown:
		break;
	}

	if ((ret_val == E1000_ERR_PHY) && (hw->phy.type == e1000_phy_igp_3) &&
	    (er32(CTRL) & E1000_PHY_CTRL_GBE_DISABLE)) {
		/* See e1000_kmrn_lock_loss_workaround_ich8lan() */
		e_info("Gigabit has been disabled, downgrading speed\n");
	}

	return link_active;
}

static void e1000e_enable_receives(struct e1000_adapter *adapter)
{
	/* make sure the receive unit is started */
	if ((adapter->flags & FLAG_RX_NEEDS_RESTART) &&
	    (adapter->flags & FLAG_RX_RESTART_NOW)) {
		struct e1000_hw *hw = &adapter->hw;
		u32 rctl = er32(RCTL);
		ew32(RCTL, rctl | E1000_RCTL_EN);
		adapter->flags &= ~FLAG_RX_RESTART_NOW;
	}
}

/**
 * e1000_watchdog - Timer Call-back
 * @data: pointer to adapter cast into an unsigned long
 **/
static void e1000_watchdog(unsigned long data)
{
	struct e1000_adapter *adapter = (struct e1000_adapter *) data;

	/* Do the rest outside of interrupt context */
	schedule_work(&adapter->watchdog_task);

	/* TODO: make this use queue_delayed_work() */
}

static void e1000_watchdog_task(struct work_struct *work)
{
	struct e1000_adapter *adapter = container_of(work,
					struct e1000_adapter, watchdog_task);
	struct net_device *netdev = adapter->netdev;
	struct e1000_mac_info *mac = &adapter->hw.mac;
	struct e1000_phy_info *phy = &adapter->hw.phy;
	struct e1000_ring *tx_ring = adapter->tx_ring;
	struct e1000_hw *hw = &adapter->hw;
	u32 link, tctl;
	int tx_pending = 0;

	link = e1000_has_link(adapter);
	if ((netif_carrier_ok(netdev)) && link) {
		e1000e_enable_receives(adapter);
		goto link_up;
	}

	if ((e1000e_enable_tx_pkt_filtering(hw)) &&
	    (adapter->mng_vlan_id != adapter->hw.mng_cookie.vlan_id))
		e1000_update_mng_vlan(adapter);

	if (link) {
		if (!netif_carrier_ok(netdev)) {
			bool txb2b = 1;
			/* update snapshot of PHY registers on LSC */
			e1000_phy_read_status(adapter);
			mac->ops.get_link_up_info(&adapter->hw,
						   &adapter->link_speed,
						   &adapter->link_duplex);
			e1000_print_link_info(adapter);
			/*
			 * On supported PHYs, check for duplex mismatch only
			 * if link has autonegotiated at 10/100 half
			 */
			if ((hw->phy.type == e1000_phy_igp_3 ||
			     hw->phy.type == e1000_phy_bm) &&
			    (hw->mac.autoneg == true) &&
			    (adapter->link_speed == SPEED_10 ||
			     adapter->link_speed == SPEED_100) &&
			    (adapter->link_duplex == HALF_DUPLEX)) {
				u16 autoneg_exp;

				e1e_rphy(hw, PHY_AUTONEG_EXP, &autoneg_exp);

				if (!(autoneg_exp & NWAY_ER_LP_NWAY_CAPS))
					e_info("Autonegotiated half duplex but"
					       " link partner cannot autoneg. "
					       " Try forcing full duplex if "
					       "link gets many collisions.\n");
			}

			/*
			 * tweak tx_queue_len according to speed/duplex
			 * and adjust the timeout factor
			 */
			netdev->tx_queue_len = adapter->tx_queue_len;
			adapter->tx_timeout_factor = 1;
			switch (adapter->link_speed) {
			case SPEED_10:
				txb2b = 0;
				netdev->tx_queue_len = 10;
				adapter->tx_timeout_factor = 16;
				break;
			case SPEED_100:
				txb2b = 0;
				netdev->tx_queue_len = 100;
				adapter->tx_timeout_factor = 10;
				break;
			}

			/*
			 * workaround: re-program speed mode bit after
			 * link-up event
			 */
			if ((adapter->flags & FLAG_TARC_SPEED_MODE_BIT) &&
			    !txb2b) {
				u32 tarc0;
				tarc0 = er32(TARC(0));
				tarc0 &= ~SPEED_MODE_BIT;
				ew32(TARC(0), tarc0);
			}

			/*
			 * disable TSO for pcie and 10/100 speeds, to avoid
			 * some hardware issues
			 */
			if (!(adapter->flags & FLAG_TSO_FORCE)) {
				switch (adapter->link_speed) {
				case SPEED_10:
				case SPEED_100:
					e_info("10/100 speed: disabling TSO\n");
					netdev->features &= ~NETIF_F_TSO;
					netdev->features &= ~NETIF_F_TSO6;
					break;
				case SPEED_1000:
					netdev->features |= NETIF_F_TSO;
					netdev->features |= NETIF_F_TSO6;
					break;
				default:
					/* oops */
					break;
				}
			}

			/*
			 * enable transmits in the hardware, need to do this
			 * after setting TARC(0)
			 */
			tctl = er32(TCTL);
			tctl |= E1000_TCTL_EN;
			ew32(TCTL, tctl);

                        /*
			 * Perform any post-link-up configuration before
			 * reporting link up.
			 */
			if (phy->ops.cfg_on_link_up)
				phy->ops.cfg_on_link_up(hw);

			netif_carrier_on(netdev);

			if (!test_bit(__E1000_DOWN, &adapter->state))
				mod_timer(&adapter->phy_info_timer,
					  round_jiffies(jiffies + 2 * HZ));
		}
	} else {
		if (netif_carrier_ok(netdev)) {
			adapter->link_speed = 0;
			adapter->link_duplex = 0;
			/* Link status message must follow this format */
			printk(KERN_INFO "e1000e: %s NIC Link is Down\n",
			       adapter->netdev->name);
			netif_carrier_off(netdev);
			if (!test_bit(__E1000_DOWN, &adapter->state))
				mod_timer(&adapter->phy_info_timer,
					  round_jiffies(jiffies + 2 * HZ));

			if (adapter->flags & FLAG_RX_NEEDS_RESTART)
				schedule_work(&adapter->reset_task);
		}
	}

link_up:
	e1000e_update_stats(adapter);

	mac->tx_packet_delta = adapter->stats.tpt - adapter->tpt_old;
	adapter->tpt_old = adapter->stats.tpt;
	mac->collision_delta = adapter->stats.colc -**************_old;
	***************************************;
***********gorc******************pyrig***********pyri****************on.

  Tght(c) 1999 - 2008 Inthis program istight(c) 1999 - 2008 Ittel Corporationt

  This program isonditior modify it
  under tx dre1000e_update_*****ive(&*********hw)x drif (!netif_carrier_ok(netdev)) {
		tx_pending = (on 2,_desc_unused(tx_ring) + 1 <
			 
  ANYseful, ****unt);
	 Founributed ins dist	/*HOUT
* We've lost link, so the controller stops DMA,y of MEbut wCHANTgot queued Tx work that's never goingy of MEto get doner
  FresetESS FOR A PAto flush Tx.y of ME(D FITNE have outside of interrupe receext) GNU Ge/arra*********tx_timeout_ with++ut e	schedule_blicthe Free So have_taskout e	/* return immediately sincublic Liison, inentf not, , Bostut e}
	}

loorCause softwareg with
  thto ensure Rx ul,   Thcleanedf notFoun*********msix_entries)
		ew32(ICS,**********rARRANTY;ims_valout elseon:
  Linux NEhope ICS_RXDMT0ware /* Force detectionlonghungESS FOR A PAfor
y watchdog perio "COPY*********rcefor_tx_
  In= 1vel@li
f MERith 82571ESS FOR A Ps, LAA may be overwritten dueibutSS FOR A P7

** have fromFITNEother port. SetFITNEappropr 021*****in RAR[0]7

*OPYING"on 2, aget_laa_****e_*****(hw)ion:
n 2, arar_set(hw NICS <linuhw.mac.addr, devel@lisRhave ITNE Frer#include !test_bit(__ng LisDOWN, he Free So#inclx/pcimode Frerthe Free So5200 N.E<linuxPURPOS round_jiffies(de <net + 2 * HZ));
}

#defineing LisTX_FLAGS_CSUM		0x0nux/mi1net/ip6_checksum.h>
#incVLAN <linux/mii2net/ip6_checksum.h>
#incTSO <linux/mii4net/ip6_checksum.h>
#incIPV4 <linux/mii8h>
#include <linux/ethtool.h_MASK	0xffffnux/pm_qos_params.h>
#include <liSHIFT	16

****icg wi  hope tso(strucchar e10******* ********PURP
  ANdriver_sk_buff *skb)
{
	driver_name[]he fi*seful, ************seful, ; = DRV_VERSION;is prog it w *[] = {
	[boa *e1000_info_tbler_v1000,
	[bo_info;
	unsiged ""
chi &e132 cmd_length = 0 &e116 ipcse		= , tucse, mss &e18_8257s,_8257oo,
	[bso,
	[bo, hdr_825;
	"
charrare Founskb_is_g0e_dkbis dist583_infoheader_clonedard_80003es	err = pinfoexpand &e10ard_,nfo,0, GFP_ATOMICfth Flude <rrion:neral Pue1000_ic L
		d_82583 = infotranslinu_offe <lrd_8 + tcp_hdrlenrd_pcrd_imsse1000_ish]		=rd_pc->gso_sizeut even skb->protocol == htons(ETH_P_IP	[board_driver_iphdr *ip]		=i		= &00_pch_in	iph->tot	= &e10 &e1 debuggcheckormation
n]		= &EBUG
/*char *e1~csum_tcpudp_magic(ebuggslude ch9_ir->nd by->dlude <ldapter->netIPPROTO_TCPdapter->netdeve			ard_82573]		=hecksum.D_CMD_IPtion
 *573_in00_ich10_info,
	[board_pch-7124		} e100 583_infofdef DEBUG
/**
 *typeretuSKB_GS000_dV6[board_ipv6hw_dev_namepayload_825*e1000e_get_hw_dev_name(structch9_ie1000_o_use
{
	re&o_use)
		returnhw->adapter-> unt + ring->nextname;
ean - ri0,*
 * e1000_de - calcu82573_infblic Lielper,
};

#inetblic,
	[board_pcut ev0_82 thevoid *)&(layer to prme(strued(sivate stw_devdataruct4_inf
static int e1000_desc_unuseld as wrd private struet_hw_dev_name(strudescriptor status field as wr functi
culate if we |= (ve unused descDEXT |ave unused descTSE |HOUT
  ANY ck
 **/
static CP |_hw_devext_- (d_82583)ludeto h = WARRANTY;n {
	to_us000_g[] = {
	[boarhave unuCONTEXT_DESC(tic cons, idescrd_82572]		= = &WARRANTY;d_82572]		=[i]pointe] = {
	[boa->loweude <up.ip_fields.handlere lac_8257ans(skb, netdev);

	if (adapter->vlgrp &&o(status = &eans(skb, netdev);

	if (adapter->vlgrp &&e(stacpu *nele16(
 **/descrns(skb, netdevupp
	if (adan]		r->vlgrs writte4_infceive(&adapter->napi, skb);
}

/**
 * e1000_r)
{
 @adaceive(&adapter->napi, skb);
}

/**
 * e1000_r3_inelse
		napi_
	[boeceive(&adapter->nan]		segif (adar->vlgrfo,
iptoive descriptormssus and error fields
 * @csum:	receive dean]	= &e10d_82583]		ve(&adapter->naard_100082573]		=else
		na32(ard_82573]      d_82572]		=-> Fre
#inmp = de <nets, __le16 vlan)device *ne5200 re l     se FounFouniretuWARRANTY; withoion
 ormationct net_device *netde(u16)stat, Bostotrucch10, Bosto0de <n.0.2-k2boolhar e100x_1000_driver_name[] = "e1000e";
conse1000e_driver_version[] = DRV_VERSION;

static const struct e1000_info *e1000_info_tbl[] = {
	[board_82571]		= &e1000_82571_info,
	[board_82572]		= &e1000_82572_info,
	8 ksum -	[board_825 have unused desc to ;
	__be16 _name - 0_82583_inf->ip_su, MA != CHECKSUM_PARTIALus_ee ChecksumE1000_RXD_S_name - retuelse
		f (!vice na8021Qx/pci_name - re vlan_ethhw_dev_nameh_/
	if ncapsulated__name
  e1000-d checksum *w_dev_name - urn;swi = ((_name - s discaseCP or UDP packet wiIP):rr;
	u8 _hw_dev_name_name - retu
 * e1000_dus_ersum has |have unused descTCiptorbreak;CESSARY;
	} else {
		/*
		 V6* IP /* XXX not handlonstall e va &e1000sf not,fragmeuse)
		returnvicey ha		 * Hardware complements the payload checksum, so we undo default IP fragunlikelyrogr_r0211imit()x/pci	e_warn("char 000_partial(statu=%x!\n"pv6.h>
fers f (! *necpuHECKSUM_UNus, __e undo ch10written by hardware
 * @vlan: des
 struct net_device *netdev,
_le16 vlan)
{
	skb->protocol = eth_type_			      struct sk_buff *skb,
			      u8 status,ruct e1000_adaev);

	if (adapteconfist sctioe(&adapter->napi, skb);
}

/**
 * e1000_rx_chksum -Receive Checksum Offload for 82543
 * @adaptch9_iwrit+d */
	1000_
	[boa	struct e1000_ring *rx_ring = adapter->rx_ri functiod error fields
 * @csum:	rec fiedapter->netdev;
	struksum(struct e1000_adapter *adapter, _couns_err,
			      u32 csum, struct sk_uff *skb)
{
	u16 status = (u16)staus_err
	u8 errors = (u8)(status_er >> 24)
	skb->ip_summed = CHECKSUM_ONE;

	/*  <net/ip6_checksuMAX_PERused	819<linux/if_vlan.hb;
	sed PWR	12"1.0.2-k2"
char e100x_map_driver_name[] = "e1000e";
const c
 * usedriver_versio, 1000_82572_infirst}

/*1000_82572_inmaibutr_txdled++;
			breaknr_frags

		/*
		 * Make bussn[] = DRV_VERSION;

static const struct e1000_info *e1000_info_tbl,
	[board_82572]		= &e1000_82572_in= &e1000_i&e10e1000_pch_i1000_82572_in
	[boa,  e10,  with_info,fo,
	++;
			break;b->ima_lude_t *map of board private structure
 *82583_infoci_m* Behe Free Sopdevy, Hv,d */,LAR _TO_DEVICEis distdev_erx/tcp.h>
#inI_DMA_FROMD"TXLAR  map failed\n"descrtruct e1000_ci_mdev_ere Foune Checksumgacy 
			};

#ifdef DEBUG
/*er_len, sk_
	[boakb: poinwhile (N;

 dist/
static void e1000_alloc_rx_buffers(stru	 e10 = minSC(*,buffer align status_err,
			   82573]		= e1000_gs_err,
			      u32 csum, struct sk_buff *skb)
{
	u16 status = (u16)s= 0;
		buffer_idmsignfailed++;
			break;
	&e10 +nfo->skvoid eware Fo
		er *at)
			i = x_desc +t)
			i */
		__C(*rx_rintus_err;;
	u8 errors = (u8)(status_errr >> 24); Licensfor (fdapte f <a 16 byte; f++rx_rin->alloc_rb6 byt_driver_* bytrce m byt
{
	 *ring)
{
	if (ri byte[fo_le6has no byt->nt - 1);

		/*
b: point_RX_DESC(*rx_rinto complete before letting h/w
		 * know therrx_rg, i);
		rx_desc->buffer_addr = cpu_to_le664(buffer_info->dma);

		i++;
		iff (i == rx_ring->count)
			i =  0;
		buffer_info = &rx_ring->buffer_iinfo[i];
	}

	if (rx_ring->next_to__use != i) {
		rx_rimap[f] (i-- == 0)   ing->count - 1);;

		/*
		 * Forcstrutware Foun Licensskb->protocol = eth_typ.skb_ring-nfo->skb;
		ocol = eth_t;
		}].u16 status = (u16)sta, Bostoci_deum bit is sevate b) {
			/Generetter luck next round */
			adapter  72_intx_flyte 72_in(status] = DRV_VERSION;

static const struct e1000_info *e1000_info_tbl DMAboard &rx_rin= NULL * the 14 byte MAC header is removed
		32 txd_pi, s_info,
xd_);

	 have unused descrFCS &e1000_82572_info,leaned
	struct &_vlan.h>
#include <s distriS(*rx_rihe payload checks to stack
 **/
stDTYP_Did e1000_reck
 **/
static vo24);
	E1000_RXhe payload chPOPTS_TXSM << 8rce memor++) {
			ps_page = &buffer#incus_erhw null ptr */
				rx_desc->rIad.buffer_gacy r[j+1] = ~cpu_to_le64(0);
				ludefo->ps_pages[j];
			if (j >= adapter->rx_ps_pages) {
			get hw null ptr */
				rx_desc->read.buffer_ge->page = alloc_page(GFP_ATOMIC);
ol.hfo->ps_pages[j];
			if (j >= adapVL get hw null ptr *ci_map_page(pdev,
						   ps_pinux/desce->paskb->data,
						  adapter-_RX_DESci_de--rx_ring, i);
		rx_desc->buffer_addr = cpu_to_le6->buffer_ihecksum.h			      u8 status, _ &rx_riit *rx_delude000_adapter 64(use != i) {
		rx	goto no_buffe);

	
	unsigct pcadapter *ad_pages[j];
  (i == rx_ring->cou	goto no_buffepi, s
	unsignidn't change becpi, s      ss_err;
	u8 errors = (u8)(status_err >> 24) adaptef buffer_addrs
		|			rx_desc->retruct e1000d_cm+;
		i497

**ts.soumemon, 5rites******mplete before lettonsth/w7

**knowh>
#re in tnew x_ririptor;

		fetch.  (Only7

**applicable  desweak-orderedNET_IP_Amodel arche bo * such as IA-64ram;#inclwmb(_coun
	skb->ip_summed = CHECKSU	LIGN)l(iux/vmalloc.h>
hw		}
		+ors = (u8)taim>
  497

**w+;
	ed th Thef m			athan one(stacessor canALIGN)ibutour servuffer t ainclu, it synchronizes IO on IA64/Altix systems6 byte mmiowigned <net/ip6_cMINIMUM_DHCP_PACKET_SIZE 2821.0.2-k2"
char e100h10_2572dhcp2]		=etter luck next round */
			adapteruffer1000e_driver_version[] = DRV_VERSION;hw *hw = x/netdevicehw&e1000_82573], net_deviceFoun/
	ifthe ag_p havnard_pc0003es2lan!(fer_info->skbgoard_pch=***********h>
#ng_cookie./
	ifidus_efers&&G".

  Contpu_to_le64(buf****us &ct ploc_skbNG	if (pCOOKIE_STATU  ps_pater->h;
			adapter->ret_hw_devhas <=VICE);
		if (pci_dma_mappPCS)))
		return;

	/(_driver_nthy har)d */
	 fieE1000_name | Ern device name sCS)))
		return;distconstn");
			 by hardw thetes to complet)((u8r status fie+14descrdriver_udby harudpdev,
		__suDP payload
!	 * HardwaUD compl))
		return;	ud before letttors to 
		 * knip +nly
		ihlbuff2e buffFounntohs(udpy, Hst)cabl67ordered memory modx_desc = 	 * knel a+ 8 -(!(i--))
	).
		 *count)
dapter *ad-- == 0)
	nfo,
	[bn 2, ato_lLIGN)ma)) {
			dinux	 * descripto,e_skb_arror(pdee Checksum bit is se"
ch__ hope maybu32 op_txore lettPLETdevice *ogram  sk_bu e10kb;
	unsigned int  = "e1000e";
con =lacepingprivrogram iFFERation._buffr *bufe
 * @clekb, NET_Herberenseorigin * es = (had:s wil smp_mb__afterndiccount: number fersSE.  Se1301 ULice doesn't exist yskb just opeter-de it16 byte 1000_agned I497

***P_ALIGN)o char *againux/ta SSARYanlude <CPU has     t e10made room availment16 byte ude <linu it will be utruct e1000_info) <bufferstwice as -EBUSYvel@lisA reprieve!#inclbo_rx_buartnumber of buffers++51 Franklin e1000_buff;tail);
	}
}

/**
 * e1000_loc_jumbo_rx_buffers - Replace used jumbo receive buffers
 * @adapter: address of board private structure
 * @cleandev;
	struct e1000_rx_desc *rx_desc;
	st>t)
					/*
		 * Forcail);
	}alloc_jumbo_rx_buffersbo recei->bufI_DMA_FROMDEVsed USE_COUNT(S, X)t_toS) >> (X)but WI)it is seate strthe gned inxmit6 bymuffer_inf_rx_buff_fail failed\n");
			ace used jumbo recrs
 * @adapter: address of board private structure
 * @cle= DRV_VERSION;

static const struct e1000_info *ed++;
			break;
		}beyond a 16 byteuffer alignapter->rxb;
		}

		sdary
		 * this will adappwring, i);
b(netdev, bbeyond a 16 byte++) {
				= &e10 */
		skb_reserve(skbpter *ad!(i--))
	2583]		nt 2 beyond a 16 bytedary
		 * this wisum -k_buff *serve(sk*/
		    bfer_info->dma = >page =lay.h>
#include <linux/netdevice.h>
#ina_mappingkfree_ble anyan: descr, BostoNETDEVum.hOKe->dma = pcfers:
	if (r0if (unlikely(!buffer_info->page)) {
				adapter->alloc_rx_fo,
};

#ifdef DEBUG
/**
 * e1000_497

**TTNESS FOR A PApter a si (!s calcCPCSge.ntotruct pke n in
_faileis enough_dev *inh>
#iFIFO) {
			t e10initiater->ITNE)) { deseachch writ.       alc is void 4
			eil* Refree_sk/s wiDEVIo           weYou 'struct****ruGE_SIZE,
	 NIC     ITNEmaxch writeserv	bufsize0, dropsapter->pdev;s wi distu8
static voidill result in r_info,
;
		>dma);

		i++;
	nfo = * the 14 bfls(ma);

		i++;d(strue in ->dm* TSO Worka#incl    P*****/2/3 C********** --ikel!(i--))
	->nextpoint;

		     &e1000, pull a few byN);
of  ring->***/
->next byte1000o(rx_ring->countnot,an]	= &e1000_ich10_info,
	[board_pchlan]		= &e1000_pch_inng->next>buffN);

	blic= i;
		if (ES2LAN,.  Sei
  Thun-neskb;aryst ch* avatee filbuff ld save a loinfo cyclese new desbuff_failkb = skb;

		an]	= &e1=e_sk	[board_b, NET_IP_ALIy wr* e1000ct pack; legag->buff(b, NET_IP_AL)4MDEVIlean_rx_irn]		= &e10!__n]		=ack; serv8_infoack; lega	[board_	eg_err"indicates whethe	dev_er.r(&pdev-unlikely(!buffer_info->page)e)) {
				adapter->allocon to 		 */
			buffer_info->skb = skb;
cc License i havr_ringbreak;
		}    P#inclfffore ns(skb,#include )
			i|00_adaptTAT_TCPCS = E1000_RXD_STAT_UDP1000ci_dev *pd		i = (rx_ry */
	+=	if (skb) {
			fo->dma);* the 1cleane16 byte_ring->next_to_use =/* allocate descriptors to fetch.  (Only
pter->pdg *rx_ring = adaptarchs,
		 * such as IA-64.= skb failed\n  rx_ring;
	struct ING".

  Conth>
#incribukt_filteul, bpci.h>
#r_info->dma)) {
			d	return;
	ess of 497

**ALIG:;
map_s.h>
x_ringaptruckeep = pc***/

#ouchore d *rce m,clude wise try viceinclupter->pdev;
	stru_count--) {
		skb = busc = E100
		/*
		 * F	adapter->_ring = ING".

  Contvlgrp;

	er_info->skb = NULL;
			break;++) {
			he payload 				   ps_pgoto none)++;

			rx_desc->read.buffer<<1000.h"

#define DRV_VERSIrror(pde;
		}skb->data,
						  adapter-tdaptear e1000e_rx_ring->next_tpage =so <				break;
			}
		}

		if (!buffer_info->dma)
			buffer_infoi = 0;
 totus;
		skb = vlan.h>
#include <
  e100_ring	if (status & rx_ring->nextt_buffer = &rx_ring->buffer_influde_to_clean;
Olyte thod wa;

		assume IPv4 packet by ip_summikel_to_er->enmentd16 by*******hard in tsuplinusICE);capabilitksum desIPv6sultwell..nfo->dno longer_buffer,y momus         et_hw_dev_name - return device name s_buffer = &rx_ring->buffer_inf#incvel@lisifD) {
		is 0{
	sn{
	sponsterroretdevoccu bytw dey */
		ib) {
			/* Berx_ring->nex,k;
		}
buffer alignme 16 byte is wi			i =  witho dist000_buffer *buf	return;

	struct s without e/* M                  spac thil Publonsto)
{
	sti];

send.ew des->status & E1000_RXD_STAT_DDb;
	cleaFRAGS	strucy
  e1000_ (unlikely(!buffer_info->page)ket_split *rx_desc;
	struct    u32 csum,  24);
	skb->ip_summed = CHEC boundar->tail);
	}	adapter->allo}

/**
 *{
			/* Al Free S -gemaponstrucal PuHangdapt@ate st:y(!sblic  withfle *used judriverure
 **/g;
	struct e1000_buffe Free Ss - Replace used jumbo recrs
 * @adapter: address of board private structure
 * @clean/* ral Public License along with
  this progYoung Parkway,the Free Software Fouation, Inc.,
  51 Franklin St - Fifthing;
	struct e1000_buin St - Fiore lettationeak-ordeblicrs
 * @adapter: address of board p**********
			ontainhis f(blic;
	/* TCPer: address o,****** - Fifthsion 2, are    _lockrx_desc *r			   !(adapter->flinit***** - Get Sps_bs N	total_S0.2-sticsength -= 4;

		total_rx_bytes += length;
		total_rdaptR Bostsule.h>ddrespleteITNE= length;		      th;
		tota.dapt    fer_info->sin tactually s puble letth>
#includecallback
			x_packets+nlikely(!skb)) {
-
					*kb->data -
				ded for copybreak, this should improve
		 * performance for small packets with large amouonlyr, BostoITNESurrl GNe */
		/

	while he Free SoPLET*****	if (!(adapter->flchange_mtu - C;

		u32)(Maximum Tinfo->d Unitength -= 4;

		total_rx_bytes += length;
		total_rth -we100

		w valut 2 bemdapter,v_allufferthe skb in buff0		  sucskb;, nega by 		  dev_otal_rx_packets+"
char e10);

		e100s - Replace used jumbo receive btus = 0tter luck next round */
			adapter->alloc_rx_buff_failed++;is will ers toivats = 0 + ice HLEN next_rFCS_LENge amouJumbofers to h = le *netdev = ues */
		>bufferRAMEnfo 		buffer_info ) &&
dma);!_desc *rx_ {
			ps>
#i_HAS_JUMBOeaned_S	if (unne, thebufferF_alls orderx_ring that every_ring *rxINVA[i];cense iSnt);

	afers to harors) <dev =_desc = <ext_rZount = e1000_desc + mappixd;
aptesed(rx_clean = i;

.

  Contaax_hws */
	leaning was ne, theUnunt);

	a MTU sapter-apter->total_rx_bytes += tota_RX_DESnfo->1000new_h>
#include RESETTINGux/netdevice.h>
#includsleep(1fers t much.
	down -> much.
	 have deutedl GNo E10es */
	pter: & c = oung Parkway,uffer_info->skb
{
	es */
	
  e{
			d");

	onstcket**/

%struc%rr(& sloe st->mtu slor, cle;
	str
		dev_k	rx_desc =			i = bo_rx_runningrogram isoto nextnfo)
{IGN,
						o_clean;
NOTE

		tpingallocufferk_done,s 16o comp, aTRIPypihe oyrnet_IP_ALIGNtruct ean		/*rk_done, 2uffer,N);

	pushes u->rx_b0_prde <**/

#incvice = &rlaripleslabuffernfo->di.e. RXBUFFER_2048 -->uffer-4096an;
	o alloowfor
 w****x_ringw *_juffe_rx*
#intin1000esc * receig(stwill u000- h/w
		menx_paskbize0,
	ount))
ues */
		<= 256	bufICS <linux.nd_82572has no256];

		cleane
	e_err("Dete512d Tx Unit Hang:\n"
	      "  512                  <%x>\n"102tinue Unit Hang:\n"
	      "  e                     <%x>\n"g->bd Tx Unit Hang:\n"
	      "  T04ge->e1000-d Unit Hang:\n"
	      "  [i].ge amouer_infogned inge.nif LPE(statectg;
	00_adawled++,
  usonstSBP->next_to_clean = i;==
	cleaned_count = e1000_desc_urx_bytes        <%x>next_to_watch.statu>net_statt = e1000_desc_       <%x>\n"
	      "  nexthw_addr + tx_ring->head),apter-		buffer_info = nefer_info->skb = NULL;
	}
	buffer_infureceives >
  e1000-defer_info->dime_stamp = 0callrct e1000_adapter *adapter,
			     strucng = adaptened int i;
	unsigned intii_ioctls - Replace used jumbo receites to cfreq *ifage *ps_page;ter-ter luck next round */
			adapter->alloc_rx_buff_failed++;
			bre Reclaim _	unsi*	unsignif- Re(ifned = 0;
	unsigned intphy. MA 0>next_! {
			/*cleaned
 *_coffer_->total_rx_bOPNOTSUPP_summed = CHter-NECESSARYSIOCGMIIPHY IP  fie->phy_iral Public Ling was nfo  so we undo it
		 net_devREG IP med = CHtdev =reg_num & 0x1Fe packSSARYMII_BMCR IP etdev =val_FLAG*********** adaregs.bmc000_hfers; legtx_ring;
	stSuct e1000_tx_desc *tx_desc, *eop_desc;
	stsuct e1000_buffer *bufferPHYSID1ct e1000_tx_desc *tx_t everything was idb, 016 calcu;
	unsigned int total_tx2bytes = 0, total_tx_packets = 0;

	i = tx= adFFFF->next_to_clean;
	eop = ADVERTISEct e1000_tx_desc *tx_desc, *eop_desc;
	advertidev,
	t_to_clean;
	eop = LPAct e1000_tx_desc *tx_desc, *eop_desc;
	lpytes.t_to_clean;
	eop = EXPANSIONct e1000_tx_desc *tx_desc, *eop_desc;
	 &e10****se;
		for (; !cleaned; CTRLn 2,ct e1000_tx_desc *tx_desc, *eop_desc;
	ctrln 2,se;
		for (; !cleaned;  &rxr_info[i];
			cleaned = (i == eop);

			if fer_aned) {
				struct sk_buff E &rx_r{
			tx_desc = E1000_TX_DESC(*tx_ring, i->counse;
		for (; !ip_summed =total_rx_byi];
n to w *hw = &adapter-Shw;
	struip_summed =rq(struct e1000_adapev =}

/**
 * e1000_clean_tx_irq -laim resources after transmit completes
 * @adapterd private stru *adapter)
{
	struct net_device *n&adapter->hw;
	strub_headlen(skb)) +
	wice as much.- Reclaim rbo receiv, buter->r					    skb->len;
				total_tx_pacs;
				total_tx_bytes-NET adawakeuBetter luck next round */
			ada esc wufcdma_failed++;
			/* cleanupskb */
			dev_kf32 io a cesc;&e1000_p_desc;ched varetvaum *uffer/* copy MACtype;

		PHY) {
		s) < descr >> 2 i <x/vmalloc.h>
#inccludnforyoftwar; ily
		 * 
	    e) {
r32(RAL(ie buffe1e_wphy adapBM_RAR_ */
apte16)(next_to_ = E1000_		smp_mb();

		if (netif_Mueue_stoppeed(netdev_ring-v) &&
		    !(tnext_to_clean.
		H*/
		smp_mb();

		if (netif_aptee_stopped(netdev) &&
		    !(test_bit(__E1000_DOWNuffe &adapter->state))) {
			netif_wake_queucense iHRESHOLD)MTA	/* Makeclea that anybody stopping the queue aftmtaesc;	 * sees the new next_to_cl0_adapteAD_REG_ARRAY adapyte MACTAtatus, __mb();

		if (neMTA	if (adapter->detect_tx_hung) {
		/* Detect a tra, tx_rut Win hardware, this serializes the
		 * check wi = a in
Make xi-- == 0 regise1000/sione_r;

	he Free Softf (netCTL, &p_desc;nextnext_to_clean.
	CTLg("%s: Rd(netdev) 0_adaptCTL_UPi) {
p_desc;;

			netif_tx_er->total_tx_bytes += total_Mx_bytes;
	adapter->total_Mx_pacs;
	adap&= ~(ts.tx_bytOing_errorkets += total_tx_packets;
O_3bytes;
	adapter(>state))) ets += total_tx_pa {
	s += total_tx_
		preatic <<ats.tx_bytean_rx_it_stats.tx_packets += total_BAMbytes;
	adapter->total_BAMt_stats.tx_packets += total_PMCFbytes;
	adapter->total_etur(netdev);
		}
	}
	uffeta up the network stack;uffe_RFC_bytes;
	adapter->total_hat     mb();

	ng(adapter);
			netif_sop_queue(n *buffffer__STATing->n*/
	OLD){
			e1000_pri  LiWUFC,THRESH    nt worCe_stamp WUCotal_WAKEstack
 **esc_pME_ENe1000_ad) & E1000__adaapter *adapter,
				 Make{
			e10re thaatic bool e1000_clean_rx_iork_to_do)
{
	uci_dev *pdev = adapter->pde1000_rx_desc_p *next_rxd;
	activde <adapter,
		s) << 2) >= Thweop_d.x_riacquire	tx_inuxt_stats_bufferx_packets +=Cdr + orde0_ps_pafferapter->total_rx_buffex_packstamp,

		wrip_desc;_mdic adapIGP01ng LispackPAGE_SELECTis wx_bytes = 0, total_rx_pates;esc_ENABLEned iuffeIG(pci int		prepage_buffer;
stamp,
	 adt = 0;
	bool cleanets = 0;

	i =REpterop_queue(ne;
	struct sk_buff *skb;
	unsignedX_DEffer_page 769r(&pdev-grx_routo remes;
	adapter->t = 0;

	i =BI starr & E1HOST_WUD_ST
	rx_desc = E1000_R_count = 0;
	bool clean;
	staterr = le32_t_cpu(rx_desc->wb.middlbuff *skb;
	unsignedesc MakeHBILIWer,
		bitr(&pdeoumed 	struct e100releasage *ps_pagckets += aterr;
	
/**
 * e1000_alloc_jushuto->tidriver_pci use *I_DM,et */
*apter ring-e structure
ace used jumbo rec8lanciata -drv fie(I_DMd++;
			break;
		nd */
			adapter->alloc_rx_buff_failed++;
			break;
		 && netif_carrier_ok(netdev)(cle,apter_ext, rctlomplks by sKE_THREStx_desc, *eowip_s(tx_ring) >= TX_WAKbo_rx_eak coddetachGN;

	i = rx_rininfo->skb = NULL;
	}
	RAME_WARN_ONinfo->page = allocter *adapter,
			     struct  timer_info->time_stamp =to next_y(!buirqfer_info[eopnt cleanedt(new_ with
  t_cpu(rx_deyime_stamp = 0_buffer;
fferrx_r
#incl->buffer_;

		/* in thei++;
		if (i ==+;
	countclean.
 &rx_rpage;
	s->count)*
 * e1 &rx_riLU theOMDEVx_byt *rx_deFC_LNKCbuffer_iHRESH packet conf (ad_e0,
e);
			dev_kfree_sknew_multire
 * @cleanKE_TBostoon_to_-     dary
    ing-f (bu    cas!(stauffer_i**
 * e100;

		iit *rx_deFC_MC + rx_re0,
		}
	}
	adapter-		skb_px_ring->btx_bytes +=n:
  Lietif_se0,
ev_kfch10l(clening was done, t   "  0_TXD_Sfree_s**/

D3Cneraw deset/ip6_checksutee tADVD3WUC linuetde00	 * mophy prx_rimanageetecvice *ne
		 */
		int l1 = le16_tENnsigneWR_MGMT_desc2>wb.upper issx_ring->be16_to_cpu(rxut even _ring);
	if (cle2aned_co2unt)
packet_sUo make* throughput, so unsand effects smav_kfr  Liuffe->rx_p      shat everything was cleaned
 **=
static bool e1000_fib ptrd e1x_byopybreak) &&
		    ((length +ze0)) {tatic bool e1000_ withnal_serde		i = 0KE_T_ring,he laser skb = N*/
	D3f not, rx_ps_bsning was don_EXata ul
			 * kmaq (napi) contexXT_SDP7_DATAoks ugly, c, so we->rx_ps_bsms compile<= copybreak) (cleaned_couIS_ICHer->hwetdev->isring,gig_wol_ich8lanthe Free Software  adaplluff_imt 2 beuted in ma	e100reques;

		/u_to	goto nextROMDEVICE)pcie_;
			map_atomic(ps_page->ev, ps_page->dma,
alloc/put
		 * only validthere is apter *ter,
		byocumeMakeU Generalesc = E1000
	}

	tx_ring->neata_offse_do)
{
	KM_SKB	/* in theength, staterr;
	iD_ERR_FRAME_evice(pdev, ps_page->dma,OLD) not, int work_to_do)
{
	kb, l1);
->rx_ring;
	struct e10    inERR_FRAME_union e100- calcint work_to- calch10rx_ring, i); = !!HRES1000_ad          b, NET_Iir,
  a00_buikely
		/* & E100
			goto next_dedev =ps_page->dma,
				PAGE
		bPT0;

	i Ds.rx_bytes 	str#incx_richar 		 */ary
inux/ theto_cpu(rx_desc-tr*skbe that everything was ength + l1) <= adapgp_packeDMA_FROigp3j, psh[0])o)
{nc.,
= i;
	ddr = kmap_atomic(ps_page-497

**RNET_IP********net>
/w copy/w.  If	skb(staAMT	goto nepter-> NET_Idr + hx_rinlX_DEy happe82572_ clov = ad(staredundan         c_skb(neET_IP_hwl[] =ro spanning mu
	= E10DEVICE)used j->bufferckets += segs;
				totct e1000_buh[0])sc *ext_rxd = E1000_RX_DESC_PS(*00_buSC_PS(* i);
		pr_cpu(			br&&ydone:RAME_fferprepare *ne00_buf>buffer_eral Publich10ffering-_**/
_d3ts++;,ydone: += MASKetSTRIPPI			dev_kfr, PCI_D3ho_for>flags2 & FLAG2_CRC_Sif (!skb = 0;
		next_rxd = E1000_RX_DESC_PS(* - 4);
x_bytes = 0, total_rx_pacDRSP))
			a
copydone:
		tefetch(next_rxd);

		next_buffer = &rx_ring->buffer_info[i];

		cleaned = 1;
		cleaned_count++;
		pci_unm             pci-e 			tx_dime ome quad<linu			if (!_ring, reu_to_l);

	correctment _STAT_w & E			skb_p_infoich  shan reu0trucD3, i););

	prev		 * archs)
{
	strucmasktrucFITNESSkb = NULL;

		/sus_ethp);

	o)
{streampu_to_ood */
.middle.statre used to sps_page->dma,
				PAGE_SIQUA_desRT
		 * applica= E1000_Rus1000_= I_DMA_bus->sel = ped vapo
		lpu(
findterr & E1000		rx_d.hi_dwCAP_ID cour (j 000_devctdesc;cpu(
X_DES) & E1nc.,dterr = lenfo +hi_dwEXPf (ptif_stiddle.r (j sum(a		wri
	}
	rx_ring->next_to_clean = i;

	cleanet char e= 0, total_rx_packiddle. & ~n = i;

	clean_CERE	      _CRC_STRIPPING))ower.h - 4);
e16_to_e1000_desc_unused(rx_ring);
	if (cleaned_count)
		adapmiddle. = le j < PS_PAGbytes += total_rx_bytes;
	adapter->X_DESC(*tx_rict e1000_bROMDEVICE)l1asp& E1000_R= E1000_RX_DE:
		tor_info&e1000_t_desc;497

******3hs,
		 * suc- u inten L1 ASPMf (buobX_DEchipset/* st);

	_page(strucvariousct e1000(ich7)0_buffer odelordebe ethe#incer bufferres    ut howABILI	unsior garb&rx_]		=rmtch   ITE) {dapter,TY onext_tevel. T;

	ddr + ge = Nm is(false) bad EEPROMct netsumUFFER_Wis wilulti 0_RXDSKB_s (uC(*rx2s)= le too  buIP_ALIy(!bze/;

	16 by);

	Ub->dtun02110-);

	featota rx_rs abFLAG1Wth[0]);ry wumpch    retuffer fer dapter->pnfo = next_buffer;

		statower.hi_dwo_cpu(rx_desc-s_error);
	}
	rx_ringower.ho_clean = i;

			eif_stcom>
  buffeaned_0x2if (unlikecsum_&uffer_info->DDEVICULL;_page(sr(&pdev-ter,
= ~0xx>\n000_desc_unused(rx_ring_jumbo_rx_irq(struct e1000_acom>
  _DESC#ifdef CONFIG_PM1.0.2-k2"
char e10susutedext_rxd = E1000_RX_DESCpm_mritege_c->erre:
		tox_ring) >fo =copydoneif (!(a) >= Tt)
			i = 0;
		neower.h&e16_to_cFound	/* in the 

		if (rx_desc->wb.uppower.hage-adapter->toi++;
		if (i == rx_ring->counc_skb(netulloc_skb(n**
 * e1000_consumefetch(next_rxd);

		next_buffer = &rx_ring->buffer_info[i];

		cleaned = 1;
		cleaned_count++;
		pci_unmap_single(pdev, buffer_info->dma,
				 ada1000_82pu(
			rx_desc->wb.lower.hi_dwo- cals_errostor{
			dev_kfree_sackets;
	return cleane*/
		if (ich8lanci_x_ring,eak codme
			break &e1000_ia_mapping_erroI_DMA_FROMpydo"Canordeapter *aCI
				buf**/

0_ring apter->total_rxboard_hecksum(
			 	kunma*/
		if (rk_done)++;ing-lower.hi_dword.cgth = l1000_RX_DESC(*rx_ring, i);
	cold <linux/DMA_FRO	}

		if (staterr & E1000_RXDEXT_EROP)) {
			e_dbg("%s: Packet Spliich8lac_skb(nepy(skq(skb);
			goto  &e1000_ich9_ rx_ring->count)
DMA_FROTRIPPIupge *pme_stamp = 0;
		buffer lenadapterter,
		c incl**/

S3/S4count);
			cleaned_coun;
			pci_dma_sync_single_for_d0_desc_u field time_atx_hang(adapter);
			nWUS_to_cpu))
		        (rx_desc-board_info->skMakeder onllength- %soc_rx_bu= tot	unsi	/* Good RS_EX ? "Unskb);
P			 P" 				both page and skb */
		MCbuffMrq(skb);
>skb = skb;
				/* an error means aBy chaBroadinfo->skb = skb;
				/* an error means anAG chai{
	rut the window
				 * too */
				if (			ebuffLin						us "adapt"receive" : "lude &pdev- to 1000_ring *rx_ring = adapterS, ~h = leERR_FRAME_KE_THR
		lengthWe16_tosc;
		}* there _FRAME_EOLD)ASK))) {
				/* recycle boescrnd skb */
				buffer_info->skb = skb;
		this is the beginy chain goes out the window
	this is the begirx_ring->rx_skb_top)
					dev_p = skb;
				skb_skb_top);
				rx_ring->rx_this is the begioto next_desc;
		}

rxtop rx_defineng->rx_skb_top
	nt worTAT_EOP)) 						           eop,
	     _DMA_FROMDE;

			ps_pagema = 0;

		leng->next_to_clean,
	      tx_ring->buffer_info[eoif (!(staterr &at1000_RXD_STAT_EO497

**Iunt >= E FOR A PAtdevAMT,       uesc DRV_LOAD until16_torx_bytes ed ints up. ze0 rtherlude <SSARsdr +e16_tosize_buff_fae16_toth;
ined_loc_rxund a cTNESS FOR good */
	riverre used to s_ring);
	if (cleaned_count)
AMT
	buffer_iata -eration can get a littl}

/**
 * e1#ted fflags2 & FLAG2_CRC_S= 0;
		next_rxd = E1000_RX_DEe str, *next_rleng0_cl
			1000_buffer *buffer_info, *next	total_ps_bsc->wb._to_cYSTEM_POWER_OFrn vangth;
	unsigned int i;
	int clsume_adapter->v *pdev = adapterter)POLLff *sROLLER
/e skbP****ng ' with
  t' - eopdge->dmings SUM_(nexry wolnfo->kb;
d Tx uory ithFLAGhav the _DES-apter * with
  ts. Iense otthe oed _RX_D;
		xtop) {
	
  thesc = E     xecc = g
			_packets++;

		/*
		netpol resources after transmit crs
 * @adapter: address of board private structure
 * @cleanu intens(skb);
			g buffer_irqwork_done0];
re-use the page, so ICE);
		
					_ring,* re-use the page, so don		buffer_i!(adapter->fliog_eror& E1ecx_pa-          en->skb_STAT_isurcefored->staI_DM: P 1); a cop>skb = NUL->sta e100:      x_desc-pcilengnforge.n e100the skb lengfunorge.nengt      oc_ju a     bus bufferaffforg	lengt);

	used jutdevbe****,
					(skb_tail_poirk_dors int lt
		skb = esc(skb, 0,
					ext_rxd = E1000_RX_DESapter->rk_d);

nel
#incluct e1000_rx_split++;

		e1000_receive_skb(adapter, netdev, skb,
				  staterr, rx_desc->wb.middle.vlan);

next_desc:
		 (!(staterr & E1000_RXD_STAT_EOP)) /* no cha2)(status) |ioer amap fauruffer_info[in = iRS theULT_DISCONNECTT_EOP)) {
			e_dbg("%s: Packet", netdev->name);
			dev_ke cpu intensive
		 */
		if (pagempy(sk  bu->tamay_pr_inf			gotx_packets++;

		/* ethNEEer_iSET	if (!(adapter->flix_paotnfo->dm             le length;
			sum Offloailed., 0,
				                           skb i sk_br_fragart contiscr200 ,sultifh);

	a &rx_-boot. I (!setectch  ;
		r_inmbl);

_des
		}-halfx_buf(adigned int toSOFTIRQ)ecompute due to CRC strip? */
		e1000_r00_receivetal_rx_bytes=0, total_rx_packets=0;

	i = rx_ring->next_to_clean;
	rx_desc = E1000_RX_DESC(*rx_ring, i);
	buffer_info = &rx_ring->buffer_info[i];

	while (rx_desc->statu= &e1000_(next_C strip? */
ge = N							    = work_to_do)
			break
		(*work_done)++;

		status = rx_desc->status;
		skb = buffer_info->skb;
		buffeomic(buffe>skb = NULL    leailed.apter->tot**
 *=ets++;

		/* eth type trans ) {
			/* thpu(
			r;
		next_rxd =tus_errostatus;

		if (*woradapter_RX_DESC(*rx_ring, i);
		prefetch((next_rxd);

		next_buffer = &rx_ring->bu_stamp,
	      eop,
	   top)->nr_frags,
		+= total_rx_packets;
	adaRECOVEREd IPhecksum(calleup_ar_inn E1000_c(skb, 
		}

 */
		if (!(adaptc_unusedxt_desc;
		}

		e10leaned_              traffic	buff:
		rxflow furtevic             rx_desc->special);

next_de lengte old o            return s_STAT_rec****yffer_in tellg;
	unha,statits OKp_atomffer_n>datl    rtch  to hardware, one a time is too;
		second*/
		if (unlikely(cleaned_count >= E1000_RX_BUct e1000_burx_ring;
tal_rx_bytes=0, total_rx_packets=0;

	i = rx_ring->next_to_clean;
	rx_desc = E1000_RX_DESC(*rx_ring, i);
	buffer_info = &rx_ring->b length);
				/* re-use the skb, only consumed the page */
				bu003es2lanring->buffer_info[RXD_ERRskb = buffer_info->skb;	"ca,
  bffer_used juter->upr->total_rx_at everytral Public Licens}
			e1000_consume_page(buffer_info, rxtop, length);
			goto next_desc;
		} else {
			if (rxtop) {
				/* end of the chain */
				skb_fill_page_desc(rxtop,
				    skb_shinfo(rxtop)->nr_frags,
				    buffer_info->page, 0, length);
				/* re-use the current skb, we only consumed the
				 *>flags2 & FLAG2_CRC_STrinbreak cod
			dev_err(&pdev->dev, "RX DMA maHOLD 32
	if (count && netif_carrier_ok(netefetch(next_rxd);

		next_bu			  le16_todev_DMA_FRpbaring1000_ad_info;
			next/speed/widthskb->00_prinfo->sk(>skbEx = Ns:2.5GB/s:%s) %pone,l_rx_bytes/E.  _rinage(0_pr_rx_pack PAGEbus.	ps_pa + l1) <=bus_	ps_pl1);
	x4)buffWps_pax4
				loc_rx_b = NULL;1")A_FROMDEVICE)OLD)nfo as gge->dma = 0buffer_i e100ddo[eop]nfo->skIne MAR) PRO/%sIGN),
			Cme_page(b_DMA_FROMDEVI PAGEl_page_desc(skb, j, ps_fepage 10/100rx_rietde&pdevE1000_RX_DESCage->p adap&ing */
 (rx_ring->rMAC: %d,,
		_ring->BA No: %06x-%03x_DMA_FROMDEVIPAGE_SIZnext,
	struct >next_(ing */
 {
	8f (aritel(0,
   fflude <n{
			if (adapter->eepr, sirq - uffer_info->skb = NULL;
		}

		for (j = 0; j < PS_PAGE_BUFFERS; j++) {
	desc *r.com&e1000_buriptor	} elserx_ring->nex*/
static /
statx_irq(strif (!(a.comffer_info->ad_nv
	memsNVM_INIT			if (l2le32_t1, &bufned_couner;
,
			

		!(napilace usee_gi & (1uffe0))->dma,
moun		brSm		rxPrx_riD)
{
(DSPD)put(sk          r(pdev, buffer_infoHOUT
"War = N:oad XXX r Hand	goto nexinumbo_rxr(&pdev->tail),
					struct e1000_adapter, downsh3GIO_3	e1000e_gig_downshift_workarou_ich8lan(&adapter->h3);
		/**
 * e100ge(st page alloc/@irq: interrupt number
 * @data: pointer to a networker *adapterdevice structure
 **ring->headry writes to pybreak codICULDMA_FROoid e10ICUL=RAME.ndo_    		fer_info    ,1000_IC_bufC) {
		hw- we'r.get_link_1000netdC) {
		hw-netdev_all.get_linta -
				C) {
		hw-ta -
				.get_link			     kb);_list) {
		hw-C) beforenect (LSC) beac/
	ifessg any PHY regiac.get_lin);

		e100atus = 1;
	;

		e100.get_lindoes += C) {
		hw-laim .get_lin * code adC) {
		hw- * code ad.get_linvalipublishdr) {
th		 * 80003ES2L,

		/*
		
	ifx.ni
			e10) {
		hw-ffer work-around

		/*
		fer woradd_vid on
		 * link dows here  disable receivekillere in the ISR and rechdog
		,ut_page/alloc_page */
				if (lengt000_ICdr,
ation calnd on
		 * vaddr,
,	buffer_};skb_fill_page_dprob
	stDsed juI     liztch   Rsc = E, 0,
				   skb = NULLkb->data_lendriver->staent: his
	evicrx_byteci_tbldware, one at a time is too slow */
		if (cleaned_c	u32 rctl = er32      l->totan			if (!le;
	iifitailroabytes=0, kb = skb;
				skb OS_DOWN, &adtch  
			& E100reakood */
b, NET_Iuctu no h;
		tota,;
		 usia0;

		lenglic Lic|| (ne */
		}
		/*000_alusedninsigned = er3pter,
		                  (R disables int= E1000b);
	d*rx_skb;
	unsigneace used jumbo recer_info[i];

		cleaned = 1;
		clnmap_single(pdev, buffsk_bufwrites to t erase
ev, ev, pirq: interard [ent->fer_inpage struresourcodeiz    				H8 wor,ce str583]		 interface devicflashructure
00_int583]	c;
		}l_tx_pac->ss_fincld(struci,t i,,_pack     _da&e10000_0, adap	unsigned i_adapter *aapm
			k *e1ng Lismbo_rx_APMEed(rx_ring);
	if (cleaned_count)

		adapter->alloc_rx_buf(adapter, cleaned_coun == rx_ring->cod_counstruct e1next_de		(*work_dC) ber_lensk	int cl);
	BITinux/(64t\n",Foundtatus;
		l not auto-mas Han		e1fo-> if INT_ASSERTED is not set, and iif it is
	
 * @*/

	/*
	 * IMS/* Ig j < PS_PAG not auto-mask if INT_ASSERTED is not set3	/*
		 * Hatatus;
		ot set, then the adapter didn't send an apter->n o-Mask...upon reading  ICR,
	 * inter				               P "No uonsume)) {#defin	" (napi_s	}

	ifabor	return cleafer_inferrdidn_irq(stru LicensNONE;

	/*->dma, Psel>pagerk-arons_exclusby t
	 * IM(E1000_RXDPS_HDRSTAT_HDRSP))
			adpu(
		 acc_bar
{
	st, IORESOURCE_MEMmay be some1000_STATUS_LU)))
			scheduDMA_FROMr to anamnext_buff00_ich9workarount (LSC memoapteER (Advanced ESTAT_Rbuffe;
	sthookfrom ark_done)++;1);
	(skb, 	buffe= NUt_rxd = E1000total_rx_bytes;
	ad/*EN);
ev = adycle */
ev, ps_p not auto-) {
			dev_kfree_skb_i_task);

		/*
		00_prie_fai);
	} watchdo-ENOMEpriv_info->ps_    (adapter-(e deo))
			psk;

		cleaned and if it 
					kuetdev) &&
		    (adapter->flaSEb);
dapteDEV->buffer_ffer_info-able receivesrx_ring->bufpage */
			kb, NET_IP_lloc_rx_buff_failed++;netif_carrier_ok(net_pages[j];
			i when we'__E1000_DOWNI_DMesc = n__E1000_DOWNpt num 14 )
			mod_tbsignei+ 1);, jiffies + b_reserv

	itructnapi_schedule_pre2p(&adapter->x>\n>netdev;
	stb, NET_IP_rve(new_skb, NET_d int totaext_t&adapme1000bytes;
	adapter->net_statstal_rx_bypter->net_statses = 0;
		adasgotal_rxx_paw);
}NETIF_MSG_DRV |turn IRQ_HAPROBE {
		rx_re structurnect (LSCnterface wor	int cl- cal **/
staother(int irq, ve100ata)
{
	stlags & FLA= ((seader is removed
		 = iore* Bee structure
 **/
staig_downsheader is removed
		w32(RCTL, rct(netdevg->next_ting);
	if (cleaned_count)
FLASH_unused(rx(er(int irq, vtructta;
	st1er->	    (!(er32(STnmap_p00_intr(intother(int irq, void *data)
{fer *IMS_OTHet_device *netdev = data;
	sticr & s = 0;
		ada00_intnfo as giv(netdev)00_intr(int irq, void ->errors apter->hw;
	umask));

	if 000_Iworkaroun00_in (!(i2(STATUS) &driver_x_ringe used jueak-orde;
	str
		de

	if (icrC) {&AM
	 */

	if (icrhe descripC) bethtool(icr of bufferserrupt wclude <linux/oC) {5
#inctate))if_napi);

>flags |= 			  le16_apev == 1;
		ean, 6e newstrncpOMPLEupt whed dSC_GI&ada* disabufferof->buffe, &ada {
		;
			}
	
		dev			/* ;
		re structurny(buffer_inem_8 *vurn IRQ_HAND +
 **/
statir *adapter bdringps_b=ct net_devic (rx_rDMA_FRO			   oleanisma = 0;

		lengtf (adep(&adaptagainst int buffer_infosw
	}

;
		cleaned_coun_task);

		/*
		;
	struct e1000_adapter
	memE100&PAGE_SIZE,
,l_rx_byt(icrew32(IMS,PAGE_SIZE,
t\n",ng = adapternvmx_ring;


nvmapter->total_tx_bl_tx_pa0;
	adapter->totauct e10ng;


 adapter->total_tx_buct e10	      bufferi->init*skbant;
	struct e1shift_task);

		/*
		hstruct e10 & E1000_ICR_INT_ASSERTED))_SIZE, test_bit(_ps_page->dma,
				PAGEufferONLY_NVMo something _count >\n"
	00_addr = kmap_atomic(ps_page-PAGE_SIZE,
	initge);
			dhe Free Software >netdev;
	struct utoneg_waie *neif (!skb) TX_WAKE_TClean_ = data;count);
			cleaned &&
		    ((length + l1) <= adapter->rclean_tRAME_pter->rx_ring->sedix = AUTO_ALeceiDE 0; rx_ring->itr_val vaddr, l1olarit		 *kb = 		buext_despter->rx_ring->ses>next_tr_info->sturnip_summe->dma = pc	    (!(	   t(new_b_IP_

	/* Write th the pRAME_ERR_MSA.

  Th{
		aed********SOL/IDER ses****packetsate))
			mrn valu (icurn IRF_SGid e1000_
	returnHWufferRQ_HANDLED;
}

/**
 mappiTX000_configure_msix - ConfRX>dma = 0;
			skb_ (cleaned_count)
x - ConfFILTERw32(&adapter->napi);
|	}
	returndware to propeule(&adapter->napi);
 interruptso[i];
 e1000_configure_msix(struct e1	    _IMS_OTH receonfigure_msix(struct e1000_adapter&adapter->hw;
	struct e1000_H   g *rx_ring = adapter->rx_ring;
	s*
 * e1t e1000_ring *tx_ring = adapter->tx_SG{
	strucCR_INT_ASSERTrly
 * generate MSI-X interrupts.IGHDMA{
	strucDMA_FROskb_putto_lpass_thrudapter->total_rx_bigure_msix sets|=unmap_page(pdev, ps_:
		rx_desc {
			a e10        NVMset(newFITNESS FOR A PA,
			  puCK_DIS			adapte arxtopn gooder_infreakffer_inew des = 0;
		adapterta - NC) bhwskb->len += length;
			sk_ps_bsitruct ge(st usilude s*****see_skb(irq - Sendev_n += lenslow i_map_stempt. Leenseg/
		iskb_s toormat0x8
	/*t anybody ses the new prep(&adap	 * 80003	strirq - Se

	/* Write thring0000_I000_buffe
	u8 erro       ckets +=    NVM Crq - SenIs Not V * 8peed drop1000_adapter _interrupt;0, ada      int *ritel(0, adapter->hwa = 0;

		lengtHRESH			skb_pnfo as gourx_buf(ad = Einclude <linux/ e100 ((adaptdapter->total_rx_byt, the = ERr_in		 * F_RX_DEfctl |= (tx_ring->ichedule(ng = add receive */
	ifux/vmalloc.h>
#include <_IMS_OTHap_si (!(icr(1, hw->hw_addr->lenx_ring->itr_register);
	adapter->eiac_mask |= t if it il.comid(adapt>itr_vims_val;
	ivar |=.rx_packets +=Inet veter);Aapter-: PCI_DMApter->eias, e.g. liaptertor;

	/* Conigure Tx vector */xd);
-NETlinux/tcp.h>
#include <linux/it interruptinclude <linux/i.				     ing downtus = dosed(_val * 256),
		       hw	unsignprivate stulti)l_tx_packe_val)
		writel(10000000 adap @dating->itr_val * 25 Tx interruptsw->hw_addr + E1000_s publi Tx inteon every write back */
	iva(1, hw->hw_addr + E1000_EITR_82574ownshWORK  51 Franklin St - Fierrupt:
in St - Fifth );
	ctrl_ext |= E100clude <linEXT_PBA_CLRupon ICR reado-Mask Other interrupts daptehifRL_EXT_PBA_CLnfo)
{w32(IA->page = No-Mask Other interrupts ew32(IVAR, M, ~E1000_EIA= E1000_CTRL_EXe1000_ad		ew32(RC
	skb- pa_all(~0x. Uatiobuff!(er32
	ifmtruct 0_DOWN,	skb_dma_unmaph>
#incld at tED))
		igure_msixc_adapter)
{
	if (adapterC))
c.->dma, ed PCI_ = 0;
	}
fc(napi_sched
		pci_disable_		e1000ter->pdev);
		kfree(adapter->msix_entriulated at thre efficirqre0x2	buffngthOC_VAe_flip_summ/
	if ICS <linux.nics@iny */
		iTDH   truct e1000_infoapter->flags &=r_info, rxOWN, &*/
		n +=LANts;
	ret -rxtoAPMfree_s			goto nexias dontoo mbo_rx,adapter too ACPI p);
				rx_r ytes =d_count);
			cleaned_count = 0;
	*hw _IN & E000_adaptePMEly *evice struSI o1000	strucWUC. and 	goto ter *adapter tor is an]		=_priv(netdev);
	struct e1esc_*hw = w_addr ter *adapte	/* Good Rc_packet_s000_Ii)) {
		adapter-_82574)t
		 * only valistats.rx_bbest available
 * capabilities of uffe3nmap_page( available
 * capabilities 1000_* use_Bunuse_msix_rx(int irhw.ut_p				s_pa1ter->hwtruct e1000_adng(adapter);
drop r, downshift_tas3s = kca	e1000


	switch LID | 1000-dsix_entry),
						      GFP_KERNEL);
			if (adapter->msixAentries) {
				for (icense i*
		  WoLh);

	mbo_rx_include <

	switch (ad_priv(netdev);
	->mac.type ==er->msiworoughput, s ReceiA 0;

clean;
	op,
				wcksuvpt to er->msts;
	ret   "ppthe re spec000ekb_fi NET_Ifailefall throug*******wrta u)
{
	stbo->stuffery wfer_ount);

 NET_Iset_intlfo->s(&adALLO->pa <linuo->page, 0, length);
				/* re-use the WOadapteecs);
				if (err == TX_WAKE_TDOWN, &adalize r ==h and tr baAG_M += len through and tr	skb_dma_unmapODE_MSecs);
				if (err =ext_tk codeetring->ns on 82
						  PCI_DMA_FROMDmsi(adapter- e1000_ad so counmbo_rx_ver**** id *dat		 * this wh),
						      GFP_K 5	e1000ecs);
				if (er			e
		length _ACK_DIS;

		lengruct e1000_tx(!pci_enable_stamp,
	      eop,
	     info, rxtop, length);
			goto next_desc;
		} else {
			if (rxtop) {
				/* end of the chain */
				skb_fill_page_desc(rxtop,
				    skb_shinfo(rxtop)->nr_frags,
				    buffer_info->page, 0, length);
				/* re-use the current skb, we only consumed the
				 * strE1000_DOWN, &adapt"eth%d the dch8lak-around/

	if e prefetchedhw = &adapter->hw;
k-around
	vector
  ThiODE_Lvent;
		   The linuao slobility(stcy
 *BEFORE       interron.

  This fS, E1000		else if (_info->skb);
			bu the
				 * page */
		
ev;
	int err:ge, 0, length);
				/* re-use the current skb, wwhole operation can get a lit Tx vectornetdev->(&adapter->napi)) {
		adapter->total_rx_bykb, j, pshwer_infohe Free Softwar	return IRQ:

	ly(!b_desc *rx_desc;
	s;ring->itr_registex.nics@t;
	ada;
	strunetdev-00_ICR_LSC))
			goto no_link_iounl receives LSC))
			goto no_lihe descripto	}

		if (staterr & E1000_RXDEXT_ERupt;
		hw->mnetd
	if (strlen(netdev32 icr = "%s-t(netdev:
	y(!buet_device *netde&&
		    (adapter-:b;
		u8 ET_IP_re accessing anyters
		 */
		if ((adapter->flags & FLC_GIG_SPEED_DROP) &&
		    (!(er32(ST IFNAMS * 8000:FNAMSdmaquest_iu intensive
		 */
		ifPCI_DMA_FROMD	       (skb->daremov32(RCTL);
	R(vecalctl & ~E1000_RCTL_EN);
			adapter->flags |= FLAG_RX_EITR_82574(vecto          e->dma,
CI subadapternsigne	rx_des&adapt/* Reca/
		wshh;
}

/>data_ngth);gs |= DEVICE);dr + besc-b_tailroa;
		Hot-PlugMSIZ t,"
		belength000_msix_oer-> more,
		bitr_vecte letti e100_IP_ne */
		}
		/*vate cketsex 0;
		ada * e10== e1000_clean_rx_irq)
				pci_unmap_single(pdev, buffer_info->dma,
						 adapter->rx_buffer_len,
						 PCI_DMA_FROMDEVICE);
			els497

**y of _ation, Idublic ****reation, I>dma 5200 N.E.read *s time exlignitly0_consumedev = adapter hardwaber;
}net_deviced        ruct e1000_adap<linux/netdevice.h>
#iext_tlrrupts_skb-el(1000000000 / (rx_ring->itr;
		/* fall back to MSI * Tx interrupts on
	pter *adapter)
nc.,
 ength;
			skb->data_len += length;
			skb->truesize += length;
		}

		/* strip the ethernet crc, problem is we're using pages now so
		 * this whole operation can get a littlun	struct net_device *netde1000_intr_msix_rx, 0, adapter->rx_ring->name,
			  netdev);
	if (err)
		goto out;
case E1000E_INi];

		cleaned = true;
		cleaned_ng->itr_register = E1000_EITR_82574(vector);
	adaptdev->name);
	else
		memcpy(adapt <= copybreak) &&->itr;
	vector++;

	if (strlen(netdev->name) < (IFNAMSst_irq(adapter->msix_entries[vector].vector,
			  &e1000_intr_msix_tx, 0, adapter->tx_ring->name,
			  ne>name, netdev->name, IFS2LAN wor_consumework-arovaddr, l1);
	down event;
		 * disable receu intensive
		 */
		if	    vecto		 * For	/* Fre(ERSler
 	}
		/* end cocount kb, r fork |=ritel(0retu_irq(a & E100(skb, 0,
					 S_LU)))
		c(skb, 0,
					.get00_receive_ev);
		vect00_receive.get (i = 0ev);
		vect (i = ,es */dev);
		vector++;
ets = 0;
	}
		/* guard [] & E10{ets++V (pci_(INTELe_stamp apteIDude <lEB_COPPER)SC_Pardude <l },ter->pdev->irq, netdev);
}

/**
 * e1000_irqFIBable - Mask off interrupt generation on the NIC
 **/
static voi
		/*_disable - Mask off interrupt generation on the NIC
 **/
static voi_hw *hw = &_LPadapter->hw;

	ew32(IMC, ~0);
	if (adapter->msix_entries)
		ew32(Ed e1000_irq_disable(struct e1000_adapter *adapter)
{
	struct e1000SERDES00_irq_enable - Enable default interrupt generation settings
 **/
s_DUALtatic void e1000_irq_enable(struct e1000_adapter *adapter)
{
	struct
		/le - Mask off interrupt generation on the NIC
 **/
static PT0_hw *hw = &adapter->hw;

	ew3ter->pdev->irq, netdev);
}

/**
 * e10002EIle - Mask off2interrupt generation on the NIC
 **/
static2EI*hw = &adapter->hw;
LE_MASK);
	}
	e1e_flush();
}

/**
 * e1000_get_hd e1000_irq_disablLE_MASK);
	}
	e1e_flush();
}

/**
 * e1000_get_h **/
static void e1LE_MAter->pdev->irq, netdev);
}

/**
 * e10003Ele - Mask off3interrupt generation on the NIC
 **/
static3E_Icurris means that
 * the driver is loaded. For AMT version (o000_hw *hw = &t
 * ter->pdev->irq, netdev);
}

/**
 * e10004000_hw *hw = &4interrupt generation on the NIC
 **/
static4LApter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	u83Vth 82573)
 *8atic void e1000_get_hw_control(struct e1000003 IA-64EIAC_825DPTmay be- Mask D) {es2;
		nterrupt generation on the NIC
 **/
staD) {
		swsm = er32SSWSM);
		ew32(SWSM, swsm | E1000_SWSM_DRV_LOAD);
	} else if (adapter->flags 	struct AS_CTRLEXT_ON_LOAD) {
		ctrl_ext = er32(CTRL_EXT);
		ew32(CTRL_EXT, ctrl_ext | E100HAS_CTRLEXT_ON_LOAD) {
		ctrl_ter->pdev->irq, netdev);
}

/**
 * eICH8_IFthis meansdr = km| E1000_SWSM_DRV_LOAD);
	} else if (adae1000_re_Glease_hw_control resets {CTRL_EXT|SWSM}:DRV_LOAD bit.
 * For ASF ith 82573)control resets {CTRL_EXT|SWSM}:DRV_LOAD bit.
 * For GP curro longer loaded. For AMT version (only with 82573) i
 * of the C this means that the network i/f is closed.
 *
 **/
static void M1000_release_hw_control(struct e1000_adapter *adapter)
{
	struct  f/w this means that thesets {CTRL_EXT|SWSM}:DRV_LOAD bit.
 * F90_release_hw_con9rol resets {CTRL_EXT|SWSM}:DRV_LOAD bit.
 * F_HAS_F and Pass Thr) {
		swsm = er32(SWSM);
		ew32(SWSM, swsm & ~E1000_Sw this means ) {
		swsm = er32(SWSM);
		ew32(SWSM, swsm & ~E10he f/w this means _ext = er32(CTRL_EXT);
		ew32(CTRL_EXT, ctrl_ext & ~e1000_release) {
		swsm = er32(SWSM);
		ew32(SWSM, swsm & ~E1B e1000_hw *hw_ext = er32(CTRL_EXT);
		ew32(CTRL_EXT, ctrl_ext & ~dma(struct e1000_adapter *adapter,
				struct e1000_ring *ring)
{
~E1000_CTRL_EXT_DRV_LOAD);
	}
}

/**
 * @e1000_alloc_ring - allocateM_taken over e1000_adapsets {CTRL_EXT|SWSM}:DRV_LOAD bit.
 * F10_R_BM_Ldma(struct e1000_adapter *adapter,
				struct e1000_ring *rup_tx_resFurces - allocate Tx resources (Descriptors)
 * @adapter: board pring->desc)
		return -ENOMEM;

	return 0;
}

/**
 * e1000e_setup_Dx_resources - allo10ate Tx resources (Descriptors)
 * @adapter: boarstructate structuretx_ring =ter->pdev->irq, netdev);
}

/**
 * ePCH_M_HVesources - apchrol resets {CTRL_EXT|SWSM}:DRV_LOAD bit.
 ing->buffee1000_relemalloc(size);
	if (!tx_ring->buffer_info)
		goto DbuffDr_info = vmalloc(size);
	if (!tx_ring->buffer_info)
		goto rest 4memset(tx_ring->buffter-}s\n",ermi
 *  ssindev;};
MODULEf (pci__T
	i apteerrupt:
 guard tdevsix_entCTL);
	API D
	retunetdev);
		vector++;
:
	retu
	struc
	retu& E1000s toptor ce_work(&adapter->d.getid_ NULL;c(skb, j,guard .get= er32 = 0;

	reapter-e_irq(vectoor cpts
 *
 *_pp(&adapemory )k(netdev) &&
		 PMISR anrx_riM
		/*
		 *Hffer work.0_ring nable to a0_ring e_irq(adaptnable to atries[vebuffer_	.= 0;
		nallocate R 0;
		n.getmsix_entries[v E1000_msix_entriees */
			u32 rctl ;
				ovice -goto errR
			eund-- ctl & ~E100*/
int e1000e_setup_rxis too slow SOFTIRQ);v;
	unsigned int:
	returnuestng->ed.ge, / (r     ng pa			e100ruct e100ector].vectorapter->total_tx_pack = 0;
		ada00e_setup_rivate0_rx_desc *rterrinfok(KERN_INFO "%s: x_skb_top) {
etdeIGN),
			oto err/* recycleschedule_work(&adapter->dE1000_EIACr to a			e_erto_cp	rx_ring->buffer_infoCopyright (c) 1999-2008o = vm Corpoing->co_DMA_FROMDEVIe_work(&adapter->downsrnterr * 8000ruct nffer_i( E1000_ffer_ito_cpm_qoseset
->dmirdware(PM_QOS_CPU_);
	LATENCY_ring->buffer_inrr:
	vfUT
  ANY _page),DEFA ethVALUEd drop  0;
	bool cler->etup_r	structruct e1000_buffex_ring *adapter)
ansmietup_rx_resourceE
 * Cg(stru00_adapter *adapter)
d up to near                {
			aing;
	struct 
 * e100uest**/

_irq - initialize interrupt*
 * Attempd up to near) * rx_riCR_IN, netdev);c(PS_PAGE_BUFFERS,
						sizeofemory ruct e1000_ps_page),
						GFP_KERNEL);
		if (!buffer_iter->f(union*
 *00_rx_dd up to neatdev
	err = AUTHOR>rx_skbr_info = &rx, <linux.nics@    l.com>&pde	err = e1SCRIPTION>rx_skb_top) {
ze);
	if (!rx_ring-		kfree(bufLICENSE("GPL		kfree(bufVER++) (elselocate x_ring)->statusin.cr;

