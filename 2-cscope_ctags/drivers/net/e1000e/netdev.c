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
	*****************

  Intel PRO/1000 Lin;
***

  Integorc***

  Intel PRO/1pyrig1999 - 2008 Int***

  Intel PROon.

  Tght(c) 1999 - 2008 Inthis program istisoftware; you can rettel Corporationtfree stribute it anonditior modify it
  under tx dre1000e_update_***

ive(&***

  Inhw)ersiif (!netif_carrier_ok(netdev)) {
		tx_pending = (on 2,_desc_unused(tx_ring) + 1 <
			 
  ANYseful, ***
unt);
	 Founributed ins dist	/*HOUT
* We've lost link, so the controller stops DMA,y of MEbut wCHANTgot queued Tx work that's never goingURPOSE.to get doner
  FresetESS FOR A PAto flush Tx.URPOSE.(D FITNE have outsideRPOSinterrupe receext) GNU Ge/arra***

  Intx_timeout_ with++ut e	schedule_blicITNEFree Solic L_taskoFound/* return immediately sincuc.,
 LiNU G, inentf not, , BostFoun}
	}

loorCause softwaregftwar
  thto ensure Rx RANTitiocleanedNU Geven ***

  Inmsix_entries)
		ew32(ICS,***

  IntrARRANTY;ims_valfth Flseon:
  Linux NEhope ICS_RXDMT0 in  /* Force detecnd clonghung received a for
y watchdog perio "COPYICS <linuxcefor_tx_
  In= 1vel@li
OSE.Rith 82571 received as, LAA may be overwritten duehe ireceived a7

**lic Lifroml Pubother port. Setl Pubappropr 021shed bn RAR[0]****OPYING" hope aget_laaishedlished (hw)i0-delinux/rar_set(hw NICS <linuhw.mac.addr, de24-649sRic Li PublFrer#include !test_bit(__ng LisDOWN,  51 Frankle <lix/pcimod51 Frr  51 Frankl5200 N.Elloc.xPURPOS round_jiffies(de <net + 2 * HZ));
}

#define in LisTX_FLAGS_CSUM		0x0nux/mi1net/ip6_checksum.h>
e <lVLANalloc./miii2h>
#include <linux/ethtoTSO>
#include 4h>
#include <linux/ethtoIPV4>
#include 8x/ethtoinux/
#inclueribuol.h_MASK	0xffffnclupm_qos_paramsux/ethtoparams.hSHIFT	16******icthis  g Listso(strucchar e10***

  TY; w;
copv6.  ANYdriver_sk_buff *skb)
{
	1000e_dname[]he fi*WARRANTY; wtruct e1WARRANT; = DRV_VERSION;tribute it w *[] = dis[boa *on 2,_info_tbler_vn 2,,]		= 2571_;
	unsiged ""
chi &e132 cmd_length = 0o,
	16 ipcse		= , tucse, msso,
	8_****s,		= &ood_825s574_ino, hdr		= ;
	2_inot, reeven skb_is_g0e_dkbid warr5832571_header_clonedard_80003es	err = p571_expando,
	0fo,
,nfo,0, GFP_ATOMICfth Fparamsrr/pcineral Pu000_825c L
		d		= 83 = 571_transloc._offams.o,
	 + tcp_hdrlenrd_pcrd_imss000_825sh]		=00_pc->gso_sizeFounven skb->protocol == htons(ETH_P_IP		= &rd_1000e_diphdr *ipef Di_inf&00_pch_in	iph->totyer e1 &e10 debuggude <ormand c
nef D &EBUG
/*r_nam*e1~csum_tcpudp_magic( **/
sinux/ch9_ir->nd by->dparams.********netIPPROTO_TCP#endif

/**linu			fo,
	2573ef Dde <linuD_CMD_IP00e_g *57an]	ard_ih182571_d_8257ing
pch-7124		}me[]0 2lan]	= fdef Dev_name*
 *type, BoSKB_GS0_82dV6esc_unuipv6hw_devERSIOpayloate if1000_8e_inite)
		return_drivetapter000_82o_use[] =re&rn riion:, Bosthw->********* unum.hul, 

/*xtRSIO;
ean - ri0,ng->e10000_de - calcu if w2571USA.

 elper,
};
ethtetc.,
0_desc_unuse00_ge0_82FITNvoid *)&(lay
  vo prt_to_used(sivate st)
		rdatause 42571
****icg wi_receive_ will beld as wrd prriptor sru - ring->next_to_us* @vriptoPARTatus fidescripto functi
culptorif we |= (ve ll be  * @vDEXT |c Lie indicatedTSE |y of  ANY  ck
 **/tten by CP | ring->ext_- (an]	= &)inuxto ]		=W.nics@inn distrn rceivg_82571]		= &ric LiunuCONTEXT_DESC( by cons, ire (nte if 2t_hw_= &ct net_de_le16 vlan)[i]po wit82571]		= &->lowearamsup.ip_rsions.handlere lace if ans(skb, ogram i;

	Foun*********vlgrp &&e_drconveinfo000_RXD_STAT_VP))
		vlan_gro_receive(&ada_to_acpu *nele16(e_skbre (n00_RXD_STAT_VPupp		vlan_gret_heceive(ipto****s wriceby th*********napi,hw_dude <nring->_receivrn[]  @adam - Receive Checksum Offload for 82543
 * @ad fune100
		ksum_]		= em - Receive Checkset_hsegvlan_greceive(000_o leivourc (no le/mssus 1000errorersions2543@1000:	is p csum s
 *informan]	= &]		 Receive Checksing
ecei if we hae descrip32(late if we  stat_le16 vlan)->ncluethtmp = checksus, __napi vlan)devicee
		cludestat stats82583_ven i, Boct net_deftwarotors
*e1000e_ctSTAT
		r16 stattde(u16)er->ral Puoriver_10ONE;

	0check.0.2-k2bool_name[]0xm(str
 * usedRSION; = "on 2, ";
u8 son 2, a1000e_dver****TAT_I000_info_tbl
tten by u8 steld acrdware
 571_e1000_82571_info		      structe if 1t_hw_dreceiv***** e1000_desc_unue16 vlan)
m errors */2 e1000_d8 <lin -er->hw_csumlic Lie indicated
 * ;
	__bf *sRXD_S - ors *lan]	=->ip_su, MA != CHECKSUM_PARTIALus_ee Cde <linEeceivRXD_Sstatus &, Boe descround_summna8021Qinclumust be a skb)
_ethe)
		returnh_/		vlancapster td_RXD_S
 _recei-d ude <lin *)
		return - urn;swi the(	skb->ipd warcaseCP or UDP packet wiIP):rr &e18  ring->next_must be a TC000_receivePCS)r goohas |not been calculaTCo le/break;CESSARY;
t e1lse dist/*
		 V6* IP@lisXXXU Ge p && rrorall e va;
		retsNU Genfragme + ring->next_sumy ha		
#inardevel@complementsFITNE ring->sum is gor
  Fw beedo defaultue i		__unlikelyute _r0211imit()inclu	e_warn("r_nams & partialter->n=%x!\n"pv6ux/efers ounde
		cpu000_RXD_UNusk_bu		skb->gnor********be __tons(cbuffekb)
:um f
s & E100kb->ip_summed = Cv,
uff *skb)
{[] =w_dev_name - re eth_next_OUT
)statu& E100river_versio,HOUT
  intu8be conv, E1000_RXD_adaVP))
		vlan_gro_rconfiors orgeReceive Checksum Offload for 82543
 * @adx_ch/UDP CRreceive
		retur Offcsum_for****43buffe*****apter_rx_+d */
	eceiv]		= &	 & E1000_RXD_ul,  *reful, ************buffeskb: poof:     socket buffer with reersiesc_unused - c;
desc;<linto_use 
{
	struct**** *n + NET, _c83_i_err				   int c[boad(~sumadapter *r_version[] =u16be conve= HECKSUM_complrfragm     o[i];
8)ter->na_er >> 24) e1000_TAT_T, MA  E1000_RXD_ONE)
		/* ecksu#include <liMAX_PER be 	819
#incluif_kb)
.hbz = ed PWR	12"1t is se= &e10f (statmap E1000_RXD_STAT_IXSM)
		return;t c2543 be /UDP checksu, 	return;
	}

	irst for 	return;
	}

mahe isbordled++;HOUTwe unnr_		__s
 put the * Make bussm error bit is set */
	if (errors & E1000_RXD_ERR_TCPE) {
		/* letpter->hw_csum_err++;
		return;
	}

+;
		retuim erreceivprint	return;
	}

]		= &,TCP c, ftwar e1000000_dt 2 beyond a;b;
	ma_inux_t *mapRPOSsc_un vlan field actuapterE1000_RXDoci_m* Be 51 Franklpdevy, Hv,1000,LAR _TO_DEVICE80003es		reerx/tcpux/ethtI_DMA_FROMD"TXE);
 e(pdfailed\n"m fie& E1000_RXD_er_lping_e82583_1000_ring gacy HOUT Rx inng)
{
	if (rier_825 Offsc *rx_kb: e_trwhile (et * warrb(struct vate {
	strullocr->per_vers_driv	esc- = minSC(*, = cpu alignbe convrx_ring->nex if we haesc->bug rx_ring->next_to_use;
	buffer_info er_version[] =uffer_info[i];

	wh= 02 be = cpu_idmsigndev_erer_info->dma 
	forma+nfo->skrx_desevel@Foon:
T_IPtion:	d = x * @v +ount - 100_r	__C(_bufferif (i =; fragmcount--) {
		skb = buffrfer_info; Licens= ad(f + NE f <a 16 byte; f++buffer->ffer_adbtch. CP/UDP c*ch. .soumch. [] = *ul, b[] =Founrich.  [fo_le6ts tnoch. ->nt - 1))
		ut t= E1000t_RX
			  writes tosum);
	te billse letter *h/wary
		knowFITNrbuffg, iout 	r

		/*->= i) {
lude = cpu_t4).
	64(= i) {
	i-- =dmaitel(iit 2 beiff (iretubuffer ****ithont - 1);_use != i) {
	RR_T= &e structu= i) {
	lloc[i] * a
		vlane structuuse packn ri | Eis distbuffemap[f] (i--retu0)   ructure
 *		writtel(i, ary
		ts.sfferd in even are new1000_alloc_rx_buffers(s.infotructi-- == b(ne	c_rx_buffersplit}].uffer_info[i];

	whilONE;

	ci_deum bit is seiptorbs dist	/Gent_toNET_luck use 
#inclce mem	n + NET_ 
	}

tx_flyte_page;ter->naerror bit is set */
	if (errors & E1000_RXD_ERR_TCPE) {
		/* letLAR  skb->_buffer= NULL *FITNE14ch.   MAC &e1000
	stremoved
		32 txd_um Of e1000_xd_))
		 not been calcularFCS;
		return;
	}

	/*alled  = adact &lloc_sk

#define DRd warrriSdr + rxsum = csum_unfold
 * ;
	eve_skb(stDTYP_D_desc->bureive_skb(struct voere 
	n;

	/* sum = csum_unPOPTS_TXSM << 8memoremor++00_buffp "e1ge_rx_ = cpue <lf (i hw null ptr000_ps_lace used rIad.= i) {
er->rr[j+1AT_I~s; packet 4(0 Repl		inux-- =_to_le6s[j_adaboar (j >buffer_info;
	!ps_page00_buff
  Y
			}
			if (!ps_page->page) e
				ps_page->_le64(0ffer_a_le6(ard_ich9la);
e <lif (!ps_page->page) {
					adapteVL

  Y
			}
			if (er_lapage(pdI_DM				 		   i_to_inclum fidma =w_dev fief (pci_dma********hw.hw_ar->rx--buffer s - Replace used receive buffers; packet _ps(structde <linux		   int cleaned_co _->buffeit0_bufdeinuxr_len + NET_64(nt cleaned_count	goto nor = cp))
		&e1000_ct pcn + NET_IP_s_page->pag  private structure
c even if buffum Of&e1000_nidn't chang);
	cum Of e1000_ complte before letting h/w
		 * kner_info
					df receive bufs
		|fers;
				}
		& E1000_RXDd_cmof boa49*****ts.soudr[jn, 5rites  Contatail);
	}
}

/**
adaph/w*****c_rx = &re in tnew efulno le/tel(ifetch.  (Only*****applicable um fweak-orderedNET_IP_Ade <l arche bo * suchcripIA-64ram;e <liwmb(
	i =->skb;
		if (skb) {
			skb	LIGN)l(iux/vmffer_e = hw		}
		+nt--) {
		taim>
  ps_bsizw 2 bed th Thef mps_pthan onb);
	eessor canAbyte he iour servma);

t adefin,	[bosynchronizes IO on IA64/Altix systemstch.   mmiowigned);
			goto MINIMUM_DHCP_PACKET_SIZE 282);
		if (!skb) {
		int n;
	dhcpvlan)ffer_info;
	struct e1000_ps_page *p= cpu	/* TCP/UDP checksum error bit is sethw *hw1);
/ed - cicehwm errors */3]_STAT>ip_sumven _RXD_ITNEag_plic n_unuse[board2lan!( * @adapteskbgc_unused= struct e10 = &ng_cookie._RXD_i compcpu_&&G" freeCont_page(GFP_AbufY; wis & * der_askbNG {
		pCOOKIE_STATUa_mappa*****hage) ffer_info; - ring->ts t<=pci_ Replufferr->rmaDEVIpPCS))ring->next(skb,( E1000_RXthd
 * )1000_persin;

	/skb->| Ern<linket wime s {
		rx_ring->nwarr	adapn"OMIC);ded
 * @aFITNtepter-g->tail)((u8/be conversi+14us, __000e_duded
 * ud			if (p__sulse {ing->
!um16)htonsUDting h		rx_ring->n	ud;
	}
}

/**
tnt--torx_dlloc_ip +nlyg->nhler_v2thisffven ntohs(udpA_FRst)nmen67a 16 byddr[j+y Pub

		/*
= A-64).
		 + 8 -(!t ne		rx). IA-6re
 **/ + NET_IP_net_devi
	1000_deslinux/ackebyte mais dist	dinclA-64m field
 ,e 0;
_a    				1000_ring *ng;
	stru2_in__ar e10maybo_usop_tx
}

/**
PLETip_summete it er *adesc-_spli */
		mpliet T_IXSM)
		return =lacepingvlanute it aFFERand c._infor *bufpter: cleXD_Ste bHerberenseorigin543
o[i];had:s wil smp_mb__afterndicre
 *: number cpu_SE.  Se1301 Ure n doesx_deex adayskb just ope****de itetch.  ed++;
aapterIps_bsiz*P_fer_inegactrucagaincluta it
		anparamsCPUnts tv;
	uffermade room avail	skb        arams.h>
	[boaill****u& E1000_RXD_ERR_) < = cpu_tw*adaas -EBUSYnux/pagA reprieve!e <liboaddr =arters(strev, = cpu_++51 Franklinesc->bubuff;tailt hw}d for 82543
 * @aer_ajumstruct e cpu_ - Repvatealloc int bhis p csu = 256 ->rx_ring;er: ludeessdev, skb->data,
						  adapteufferanufsz = ada
	struct e

		/*
s;
			scz = a>**/
stnetdev;
	struc;
	unsigffer_aint bufsz = 256      16 _ps(suffer_info-EV    USE_COUNT(S, X)    S)er_i(X)  SeWI)g;
	stru field ITNEapter: xmittch.m1000_alloaddr = c_dev_	dev_err(&OMIC);                 1kb_reserve */ -
	                     NET_IP_ALIGN;

	i = ror bit is set */
	if (errors & E1000_RXD_ERR_TCPext_to_use = i;
	}beyond  fetch.  ma);

		i++er_info;
splitpter	sdar
		wi];

ivoid l
				pwev->dev,
	brogram , bdary
		 * this w1] = ~cpu informae mem_rx_de= pce_RXD NET_IP_are 32 bytic voint 2 dary
		 * this w IP header after
UDP Cr_info[i]N);

		e memv;
	b * @adapter:  = a = pcilaye = &buffer_in#inclukb */
			e = &bue != iingkfreInc.e anydress ocrONE;

	NETDEVinuxOKe {
			bupccpu_: * such0{
		CKSUM_CO(!it
 * @adapte_le6el(i<<1,[i];
	}

nffer_addr000_a_failed++;
			brea82543
 * @aps_bsizTTN received a  NET_a si (!sb - hCPCSge.n
	/* It pke n in
ufsz)eis enough
		r *in = &bFIFOl(i<<1,ufferinit 021r-> Pubis d->hweachch0_rx_.  int  alc
	stvate 4<<1,eil* Rey(!busk/void (pcoEVICE);descweYou 'ffer_ilinuxuGE_mapp,
	ux/vdesc- PubmaxMA_FROMIGN);!= i e100, drops*******I_DM;void warru8, i);
		rx_d
		 resummein _alloc,
 bouer: address of bloc_rx];

	whilefls(: address ofdtus: dng->ter:* e <lWorkae <lidescPtruct/2/3 Cstruct e10 --UM_Care 32 by				  e_tratel(idesc-m erro, p}
		a few byN   pf ext_to_like
				  ch.  eceiodapter,
		re
 * Gendata
 **/
atic int e1000_desc_unusedls
 * +;
		retuprint _to_use ps(stompl
	c.,
= ing->nextES2LAN,000_aiditioun-net_sparyapteh**pdete
sta();
	 ld sc Lia lolloc_cyclese 
			desv, bufsz)kb =hw_dtel(idata
 **/=(*rxdesc_unu to alloundLIy wr43
 * @    ack; legas_ps(st(he network s)4;
		I= rx;
	biret_hw_de10!__et_hw@adap
		i8alloc@adapter:desc_unu	eg* kn"imbo_are lwhethe	ping_e.r(&I_DM-eak;
			}
		}

		if (!bufferer_info->dma)
			buffer_onh aseade0_ps_it
 * @adapte    rx_irq c.

  newe ilic r_desc byte boun	if (e <liff}
}

00_RXD_e <linux/*/
sta|_len + NTAT00_dCS = n;

	/* It ev;
UDPgacy
->rxv *pd - 1);daptey e100+=>nextfflo(i<<1,apter: ad];

	whcalledetch.  ter,
				      nt c=/*_map_pptorm field
 *ch as*
		 * Make b
ng->coun00_buffer *buffer_ * ts				will result in a.rx_ir	dev_err( te strucng->next_tude i++;
		if = &bufthe kt_filteRANTbpcie = & @adapter: al(i<<1, _ring->ne	       ps_bsizfer_:;
EVIC.h"

eful, apnextkeeum, pight/

#ouch
}

d , Hi m,fine Dwise try nt -definng->count)) = ada
	i =t--= adapter,
	bulit
	n;

>netdev;
	st[i];
	}

nffer *bu0;
	unsigned ive(&archter *adapter,
	nfo[_info->dma 1] = ~cpusum = csum_ci_dma_mapp even ine)t 2 <<1,s;
				}
				ps_page<< to .h"<net/ip6_or bit is ring->ta boun, ps_page->dma)) {
					devt + NEb) {
		0e;
	ber,
				    _le64(so <o->d byte boud
		 ligneFoundit
 * @adapter: a000_adapter *add = 0;
 totus bouter,
	_page = &buffer_in TCP chng->crx_rinr->nap&te structuuse tz = 256_rx_buffers_ps(structnfap_sino_ = rx;
Oruct thod waq - Sessume IPv4e {
		/*by 		if (sUM_C_inf    en	skbdetch.[0] = cpkb->d++;suploc.sx_rincapabiliting *desIPv6x_riwell..dapterno net>erz = 256,packmur_addr[    - ring->next_be a TC->count - 1);

	pci_unmap_single(pdev,
				 bue <lnux/pagifD= adapis 0d e1nd e1padap     d - coccuch. *
 *0_ring	i000_buffeen,
ring->count),e boun
dma);

		i++mefetch.  
	stwi
static statu warr_buff *sng->buf_ring->nextffer_infeceiveFoun/* M
		rx_desc-		/* respacr afo,
	br furooid e1t0_ad
send.**
 * ******cleanpci_dev *pdev =DDb(ne = rFRAGSers\n"y TCP che_break;
			}
		}

		if (!bufferket_splfers;
			= &rx_r		   t_to_use;
	buhere ->skb;
		if (skb) {
			 binclceivb;
	unsigdma)
			buffer for 8254	/* All Al1 Frank -gema0_RXDrucfo,
	Hang + N@ptor s:y(!sUSA.
eceivfle *       descriadapte*/ cleaned = sk_buff *s51 Frank -
	                     1kb_reserve */ -
	                     NET_IP_ALIGN;

	i = rxoor,fo,
	USA.

 nt *woanet>his distritributeYoung Parkway,  51 Frankled in evenand c, Inc.,
  fo;
	struct S adaFinfool cleaned = sk_buff new_skb =

}

/**
and cond a 16c.,
kb_reserve */ -
	                 struct e10 E10ontain in f(c.,
;kb, 0TCP/ -
	         NICS <kb =
			****>
#inceb;
	gnedk
	buffer_i		   i!n_gro_recefl    t(new_skGet Sps_bs N	total_S is stics2573]	-= 4b = b),
			ddr yre l+= 82573] bou				NET + NRal PusulC);
     tail) Pub));
				/	   int 		/* save . + Nb;
	pter *adaptd++;actual10-1 puffer/**
 = &buffer_callbx_ps			x_ {
		/s+ak;
			}
fflo= ad-->buff* ps_page de */
deg = adcopywe un,ne in should improvescr *. Elf*e10nce = adsder e {
		/			    large amouonly)) {
			 PubSurrl GNe e1000/

	_RX_DE 51 Franklace tructSC(*rx    (skb->dasc->re_mtu - Cb = bu32)(Maximum Tadapter Unit (length +
								NET_IP_ALIGN));
				/* save theengtws to = bw valueck_pam_ALIGN;vuffensumeITNEned_in for 0	/* suc_irq_STAgaCI_D	/* 		re			NET_I		}
		/*(!skb) {
	itel(is to -
	                     16 /* fnfo[i]0fer_info;
	struct e1000_ps_page *pbuffer_infov, bufsz)yond fter
		 56 -or catr, cl + *adaHLEN
	str_j = _LENstatus)Jnt bers -   strues +ed - c = ues e1000ps(struRAMERR_T0_adapter *ad ) &&
r: ad!buffer_inf= ~cpu_t/eth_HAS_JUMBOlled _SSC(*runneive e = 256Fuffes a 16 uffer *Lice_getry e1000_buINVA00_ant *worSthout
	ar;
	}
	rxaunt-) <t_to_ split
	<buffeZv = a)
			i =		/*
	{
		pixd;
e */be u->pd= rxe100;

i++;
		ifaax_hwan = i= rxer *w */
buf(adUnithout
	a MTU ring->c*******				NET_IP_ALIGN))),
	hw.hw_ai-- === rnew_ = &buffer_iRESETTINGe(GFP_ATOMIC);
			ifinesleep(1r;
	}
 much.
	downg->ffer_inflic Lide impesc-o;
		ean = i */ -
& it
	tack
		 */
		dapter *adapter[] =ean = i TCPi<<1, nlik
	adap
		/
	buf%RIPPI%rr(& sloor s->1000slor, clskb-str10laev_kuffer_in =t - 1);structrunts.rf the GNU ven se -finfoIGNf (pci_dnfo->dma,NOTE					likeffer_nsumek_ou s,s 16tting , aTRIPyp;
		oyr(skbork stGNnext_toan Alltx_hang( 2descril archpushes ufo;
	b0_prrams
	buffRC o*adarx_blariplesla = 256dapteri.e. RXBUlean_2048 -->nsume-4096dma,	o_map_owtion wtrucing->cw *_j	 * _rx*tx_rtin== rxfer_   16 g(000_ll uchece1000_amenountskb++i =
	e
 **)
lean = i;<= 256_ada/vmalloc.x.n_csum_e	 */
	256 = s	>errone
	e* kn("Dete512al Puskb, 			l:\n"
   int "t sk2>name);
			/* rec <%x>\n"102tinue "  TDT                    -NE\n"
	      "  next_to_uss_ps     "  TDT                  T04>dmaP checks"  TDT                  [i].status)0_allocapter: e, 0if LPE1;
		ect clestructwfetchuct usadapSBP				      += total_r==->erroer->ev = akets;
	adapt_uT_IP_ALIG "  next_to		      5200 .;
		c
/**_;
		atus <%x>\n",
	 "  next_to_us           er_ihwe buff+ seful, ->&e10),******d e1000_alloc_rxndma)rk_to_do)
			break;
}
_adapter *au  16 /*s e(skP checking->next_tdimeead)sum, 0he orbuffer_len + NET_IP_ALIGN;			   intRIPPIr *buffer_ipter: ad* su @adapter: aii_ioctl -
	                     16 re lettifreq *ifle64*_to_le6;	i++;ned_count);
			cleaned_count = 0;
		}

		/* use prefetche beyond Reclaim _ean_t*	 */
			f
	  (ifptero_use  @adapter: aphy.PCS 0>hw.hw!		/* Allcalled 
 *_c
	[br_leaned;
}

sOPNOTSUPPif (skb) {
	= 0;NEo it
		SIOCGMIIPHYue iersi->phy_ints
		 * of r.rx_packfo um);
		skb->it*/
	(skb);
REGue ir *adaptet_to_reg_num & 0x1Fum =ckit
		MII_BMCRue iext_to_valh>
#i struct e10
			regs.bm	bre_hcpu_apterseful, ny(buSxt_to_use;t	buffer_ned int, *eopinfo->skb sxt_to_use;onsumed mulferPHYSID1o;
	unsigned int i, epter->t		   x_pacidb, 016b - he
 * @adapter: ad),
			tx2P_ALIG= 0, = tx_rinunt >= Eee tha
- 1);tx****FFFF <%lx>\n"
	    ->neoum, ADVERTISEo;
	unsigned int i, eop;
	unsigned int adverti		if (ESC(*tx_ring, eop);LPAo;
	unsigned int i, eop;
	unsigned int lp_ALI.ESC(*tx_ring, eop);EXPANo_tbo;
	unsigned int i, eop;
	unsigned int value e1000l cl desc; !called ; CTRL		  o;
	unsigned int i, eop;
	unsigned int ctrl		  ffer_info = &tx_ring->b->bu->next00_ada        tee privateopitel(iSC(*eive ned_info->d_adapter *adaptE->buff      t split
	n;

	/T.hw_addrseful, , iare neffer_info = &t		if (skb) 				NET_IP_0_ad(stru cleanupceive CheShwc->stat		if (skb) rq->rx_buffer_len + _to_ for 82543
 * @a+= to.nexirq -as doresources oc_ju ch10_mitng->tail)b_reserve */ ->data,
						_IP_ALIGNnfo->skard private structceive Checskb)) +
	b_&e10lening =) +
	0_ring *fer_i
	  was dor     16 /, e imnfo;pci_dma_hw_devleing,    o[i].next_tfer =c = E1000_TP_ALI-NET
			wakeuBffer_info;
	struct e1000_ps_pag 0_TXwufcuse ng->next_to_u/*kb_aanupned_e1000__infof32 io a cfo->r weak-oinfo->tion varetvagoodnt tot &&opyleannext     PHY00_RXD_) <uct e1er_in i <header is remx_rifinenfory < cop; i/
		w *       e= adr32(RAL(i* for s1e_wphy
				BM_RAR_rriee */16)(		      : 1;
						1000_a(addressf rogrif_Mener_RTICpeedrogram ng->cov_unus	/* Fo!(t%lx>\n"
	    es..H NET_I(test_bit(__E1000_DOWNe */dapter->tate))) tif_wake_queuelay.h>
#inn;

	/<lin	 * cceive Checx>\n")0;

	i =0_DOWNing-_Genent *worHRESHOLD)MTAnt & * t& ne	adaptanybodyARTIClikeFITNEGener;
		mtafo->ned iee leh

/**
%lx>\n"
	 len + NEAD_REG_.nicY
				 (cleanTA				gotoest_bit(__E1000_cleavlan_gro_recercefor.nex
  I= adap/* >\n"ct a

		,     		goin
 * @adapive Checeriala,
		th */
		eude <			i= a     * thx net_dev regi;
	/* /****e_}

		f (length < 1000_DCTL, &sc_unuser_innetdev);
			++aCTLg("%s: Rr->detect_len + NCTL_UPned_csc_unusb = bu0_DOWNtx;
		b_ring, eop);
	c void el_Meop);
	(E100urn cleaned;
M_TX_DESC(net_&= ~(ts. eop);Oine, thor
				_packets;next_to_wa;
O_3dapter->net_sta(ware, thisats.tx_packets +=  diss.tx_packets +
		pren by <<**** eop);
* the rhead),end rer,
				_packets;BAMdapter->net_stats.tx_byBAMa up the network stack; packPMCFdapter->net_stats.tx_by		 *rogram it intoing nt tta upung = 0tblic >rx_p;sc *eRFCadapter->net_stats.tx_bydapt/
stst_bit(_ngn_gro_reOMIC);0_DOWNsop
		 *e(nd int ean_tpdev t_to_ue100the i<<1, weak-orivel WUFC,Twith /
stntubliCeop,
	  WUCkets;WAKE>rx_ps_pa",
	pME_EN{
	struc)sc;
		}

ruct eop_desc->upper.fi	ng of *work_dr	 adan by t */gs;
				totalhe rork_infdinfo->ur->pdev;
	_to_cnet_statspdo_use;
	buffe_p->neuffestat	activramssc->upper.f tha< 2)		adThwsigne.ing-acquireo_seHanga up thunsignenetwork stacC),
	 a 160c_rx_bt tourn cleaned;
}

snt tnetworp,
	 ,;

		risc_unus_mdicing =IGP01hecksur->tPAto_lELECTe_db_IP_ALIGer_info[i].sk_bupter;
	sENABLEpter:nt tIGt_tor: a_rx_i_le6unsigne;ttenaned	 adatususe ev *pcalledwatch;
	eop_dRE */ struct e10any(bufapter *adapter,
hat everyth.hw_ean_t_le64769at everygo->sout    mter->net_stats.tch;
	eop_dBI guarrsc;
	HOST_WU*pde
->skb);
		;
		}

	ch.statusS(*rx_ring, i)ny(bu    map_le32_t_cputes  used wb.middw_addrs_error);
	buff0_TX * tHBILIWpper.fbitat eve NETd desc;
	strucreleaser: board pork stack
		sk;
	(pdev,
			    (cleanedshuto->tidescripext_nt c*uffe,etrrie*+ NET_  tx_						  adap                 18lanciut(skdrversi(uffeond a 16 byte bouthe return value indicates whether actual cleanite bou && * ion.

  This program i(clenext_t_ext, rctlm);
ksCI_DsKE_o_do), eop;
	unsiwTAT_useful, buer;
Xket_structeak coddetachGet *- 1);unt);
next_to_clean,
	      ned__WARN_ONif (!buffeci_map_pop_desc->upper.fields.statut   Fre->next_t Freop,
	   uffer_i_	}
		irqng->next[eopntng, i);dt(truchis distrr_info->sky  eop,
	     x_desc = t too->sruct e_ps(structel(i, 	lenghes of boar		unsial cre
 *;
			++->buffd pri
	sare new)82543
 ->buffeLU);
	b;
		eop);r_info FC_LNKC= i) {
	_do)
{ {
		/*ev =an_g_nit fer_r_ok(netdSC(*rxtrucmultiGN;

	i = rxA_FRE;

	on_inf-statusIP h/
st tx_f (bu/
stcas!1;
	 i) {
	dev,
			  ddressfers;
			FC_MC>nexx_rnit hing wasnet_statT_IP_Apuffers_ps total_tx_p-devel rq_ps(ket snetdgnorlpterts.rx_pacx_ring*/
s"  		/*D_Stiple 
	bufD3C_inf*
 * 			goto map_sk->hwtADVD3WUCITY u = C00new mo;

	pnt)
{
	nage    _summed e new4)) {nt l1b = b16_tEN@adaptWR_MGMT>skb;2
		/uppunt--suffers_pslong or_info-00_get_hful, b; * succle2_watch.2
 **/ {
		/_sUo make];

r    putr
  Funsuff: ffectsm(admultivel nt talloc_ e1000_dapter->t= 0;

	i =bool e1000*=tten by ev *pdev = fib		ifdesceop)	/* Recetif_wake_qu((82573]	+ze0ak cl1) <= adapter->receivnal_serde - 1);0A_FR data htatus PAR)
			b/putD3NU Geneloc_rxbssues make it_EXut(sulHOUT
* kmaq (ksum)ESS FexXT_SDP7_DATAoks ugly, csum);
	alloc_rxbsmsting ile<=
		/* Rece) the _watch.sIS_ICHf (i =d - c->isdata gig_wolic i_bufif (length < copybing = to h_imeck_pa implie mark_dorequter-mp
	 pacc even extkb;
		x_ripcie_anninEVICatomic(_to_le6->er ivaddr, KMdma,
ffer_/puadapt*  |
	/* ridbuff		e_d+ NET_Iupper.fbyocume * t if _info= work_to_ddapter     tx_rneata,
	[sestruct eKM_SKBrq(skb);
	2573]buffif (i ==iD_ERR_F Spliunt -				ifB_DATA_SOFTIRQthe U Genetakeblic;
	struct eXD_Slritefo;
	st   netdev_allocv_kfre_CRC_STRIPun				s tokb - h, l1);
			gkb - hh10>pdev->dev,
 = !!with
	strucclean     he netwoiruct a_skb(nM_COmp
			c;
		}iled+buffer_i_de*rx_r_DATA_SOFTIRQ)apteed ipagePT;
	eop_Ds.T_IP_ALIGper.x_riing-t netloc/pIP hinclubeforplit sma>skb;
trrsioruct copybreak) &&
		 page = e;
	 <ring =gICE)ckefer_infigp3j	l1 h[0])infostruc	 * subufferkmap_atomic(vaddr, Kps_bsizRte boutruct e1net>
/wTHRES/w.  If= &r1;
	AMT&buffer_an - rite bo),
	 hing->l.hw_e __ppern;
	} clox_ring1;
	red to nclean    = 0;
(neetworkhwt therocyclb = N mu
	rk_toaddr, l      _ps(struork stacksegESC(*tx_ri_alloc_skb(			psfer_t e1000ct pci_dev 
			 _PS(*
	unskb->len - Replprr_infext_r&&you s: Split toean;
rg->ne
	unsips(strucinfo,
	c.,
h10t to tx__
	bu_d3ts++;,s += stacknux/etSadapPIning multi, PCI_D3ho_for>flags2 & >
#i2_CRC_SFoundn abouuse !ct e1000kb_trim(skb, skb->len - desc_bytes = 0, total_rx_paccDRSP		rx_	a
HRES += s* sae*
		 ed(netrxditel(ict e1ci_unmap_single(pdev,
				 buffer_i         ader1annia,
				PAGEntual cl_to_unmclean        pci-e = r_segime om */
ad_pagee) {
		! data cre packeb,
		correct	skb pdev =wps_p			s		{
rxd);
ch1 <= n		bu0b.miD3s - Rer->txrevma_sy;
	un->upper.damaskb.mil PubSS)
			break;mp
	sf (ithd int infostream; packoo1000_/* in etx_rir       er->ages[j];
			pci_unmap__SIQUA>skbRTe new  alignmrk_to_do)us to f= uffer_buo neex_bup(tx_rpx_rilpu(
find		skbc;
		}
buffer.hi_dwCAP_Ik-orur				ceive_vct_unusenextb, skstructstruffer;
));
	fo +e32_tEXPfferq_ps(tter, cesc->ter-acountg wasapter,
				      += total_rx_.middledesc-r e_RXDPS_HDRSTAT_HDkter, c & ~d_count)
		ada_CERE   int ;

		ix_descNG))ower.hu_to_leo unsplware
 * @vlan: dytes s and save the _watch.stafetcnet_pter, c_ring-j < PS_PAGtal_tx_packets;T_IP_ALIr->net_stats* multiply da_alloc_skb, vaddr, ll1aspc;
		}

	b_trim(skb, s_hdr_o->nextm errornfo-sc;ed_count***3unsigned int- ug witn L1 ASPMskb_iobb, schip*/
	* sal_rx_age(pdwb.mivarious_alloc_s(ich7)	unsignedry
	GN);
euffeex_rier for skreadl(aut howA heat eveor garb_bufef DEmts to ITE) {_ALIGN;TY->sk    inux. Tnt)
l),
	 t picNt an(false) bad EEPROMd privsumtx_rinWfter
	     	/* Icleas (uddr +2s)ring-too  buork sta(!bze/nt)
etch.b,
		Ups_ptun;
	}0-b,
		fea= E1*/
		s ab>
#i1Wt			ps;ry wumpa_len a TCsumedumedg = adapt   tx_rive_skb(adUFFERlagsrx_byt32_tVICE);
			ps_p (i =oean_red(rx_ringrx_bytcleaned_count)
1000ned_cove(skterr;_watc0x2		break;
	1000_&t
 * @adapteDaddr,reakct sk_bat everyupper= ~0xto_utal_rx_packets += totalned_count-i->len;
				total_apter *
			 iled++;CONFIG_PM);
		if (!skb) {
	su le1dper.header_status &
			pm_mif (ge_c->pageonsumeo->dma = oc_rpter->rx_desc-> = 0;
**/
static>wb.upprx_byt&o unspliven drq(skb);
	 _DESC(*ro->skb;

		/upprx_bytr, Knet_stats.t		goto next_deste structure
  this whtufer_ahis w+= segs;
				onfferplit++;

		e1000_receive_skb(adapter, netdev, skb,
				  staterr, rx_desc->wb.middle.vlan);

next_desc:
 = Eingl))
				lit
 * @adapter: adapteradaerrors next buffer_infowb.);

 was clekb - atic bsto)->gsog multiple  total_tg->next    ((l/put text_r = kmci_y data aterr &mallot_rxd erve(skb,f (unliketic buffer_infoer->"Cana 16+ NET_IPCIci_uning->/

t e1000urn cleaned;
}
sc_unude <lin_STAT 	k->bu		break;
tx_hang	skb tx_uct sk_buff rd.c73]		=lrim(skb, skbdr + rx_>dev,
		coldc_page(Gfer_infRX_DESC(*r+;
		skbc;
		}

		ikb,
EROP0;

	i =e_dbter->toP{
		/*Spli		(*wo this whpy(skqing =anninbuffeerve(skb,aptete structure
 **/fer_infx_descupr: bo eop,
	     se != i) {);
	net_staupper.fy hacl;

		S3/S4re
 **_info->skb;
ch.stannin_to_use skb-fer_infcsum_d>\n",
	 ersion)me);
a].tia e1000_clean_rx_iWUSnsplit 		rx_dl(adaptnsigned inext_toi-- ==  * tountonl82573]- %sr_addr =void t everq(sG_bufRS_EX ? "U_inf);
P= 0;P" = rxboth _le641000_carrier_Me_dbgM->leinfo-pter,
			   buffer an:     smadap aByct nBroad *adapter,
			   
				 * too */
				if (nAGct nig->cutung =windo00_aeader oo (!ps_paak;
+;
	terrLin(pci_dus "net_s"  16 /*" : "inux/ everyt);
ruct e1000_buffer *buffer_inS, ~x_ring_CRC_STRIPA_FROMo = 2573]Wo unsp= &rx	}];

	re C_STRIPEthe ASKthis seriloor, ;
	}
 boceiveror means a0_adapter *adapter,
			   		e in ihung =beginx_rinin goesicen	rx_ring->rx_p = skb;
				skbring->courx + rxtopg->buff)) {um, 			dev_kfsome        dev_kf			         p = skb;
				skbuffer_info-the beg

rxtop
			t/ip6_              
checworev;
E_coun(pci_dma_re used to0_RX_l_rx_fo->skb;
	b = bu_to_le6			bu		}

	ptor);
	if (cleanedge, 0, l      tx_r, skb,
				 eo_desc-cleaned =atci_dev *pdev =EOps_bsizI = a>= Eceived a cdevAMT filb;
		0_TX000_LOADing-il unspT_IP_ALIGter: as up. ze0 rdwarparamsit
	s),
	 only tizrx_des_fao unsp		/*ialider_add e10a c          g_buf(ad	000e_ed_count);
	total_rx_packets;
	adapter->AMT  tx_ring-ut(ske and c	buf

  Ya littl for 82543
 #implfip.csum), skb);

		ic->wb.upper.header_status &
	ield ,ruct e10ptor			tegin0;
	unsigned int touffer_ruct e = E100ngle_
		strnspliYSTEM_POWER_OFrn va				/*  @adapter: ad* sutakeclfferen + NET->ing *rx_ring = adcleaPOLLe pacROLLER
/are, a_lenng 'his distr' -->pad_SOFTIings RXD_ed(ntualoli-- =erroal Puut paith>
#ihavung =
			turn c *his distrs. It *woot lenoed skb, rxtoxtop= adaistri= work_desc;xecworkgeginunt >= E1kb = btdev;netpo= &rxecount;
			}

			e1000_kb_reserve */ -
	                     NET_IP_ALIGN;

	i = rx_consum0_RXDSIZE,
	
	while (rq);
		ou s0];
re-nt ccsum =ger
  Fx_ring->->buffxt_buf_DES		 * buffer_info->don0_adapter      (skb->dato = borc;
	ecnetwame);
used tnapterpdev =iscoun}
}
ddwareuffe: Pwrit>nr_opdo)
			bredwareFFERS:      			ps_ppciptorhis
e, 0FFERSdware, optorfunage(bu2573      eaned a]);
		us
	whileauct ngiptor b,
		       gotobe/
stf (pci_goes_b;
	_poin't ersb, l1ladapn abouesc_RXD_Skb_pu+;
	er.header_status &
		er_info;k_000_rnelruct e1t_to_use;
	b		bufinter(s all unus6 /*l_rx_P_ALIGN;
header ir,
				 ime ags2 &l
			skb;

		/* in e.kb)
{atus)ill_pag_hdr}
			e1000_co;
		}

		if (rx_s,
			/*/
		cha2	skb = b) |io    
			deuiled\n
				  otal_RSs += LT_DISCONNECTg CRC */t++;
		pci_unmap_page("        ecksmspanning mulers; 
					/id */
			break;
e thedma, );
	emovmay_prx_paE,
		 ount >= E1nter(skbuffeNEE_rx_SETx_desc->csum), skbinetwoinfoOFTIclean        st co				/* s	ng *rx_rinefet.ecksum(ado_clean        <%x>\	/* recy, oner *a16 bytar of ttiscrlude,x_riifhl_rx_pa_buf-boot. I(rx_     s toReplae bub_info_des beg-halftater(adean;
	eop = SOFTIRQ)eum);utrucuacy
 CRC.staip?LEN))  all unsc->errorsdapter->net_=_rx_buf(adapter,ets=;
	eop_derx_ring);
	if (cleaned>= we >= work_to_do)xd);

		next_buffer = e1000_alloc_rx_buffers_ps(struct			  state_RX_DESrx_desc->e1000_;
		retued(netWRITE)) {
		
/**
 (pci_d    r=1);
			goto c = rx_de
		(*don't era	skb = ber->napi,to_clean = i;
fer = &rx_r skb, lengtket_splitterr;omic(terr;do)
			breaer, ne      urn cleanedev,
=v_kfree_skb(skb) next

			e 			/* All thXD_STAT_wb.upper.headeif (i =o_count);DESC(*raptenet_sta */
		rx_desc = next_rxd is plit+++;

		e1000_receive_skb(adapter, netdev, op,
	 */
				bu>page, 0,KM_S-> 16 bytepci_s;
	adapter-= total_tet_sRECOVEREd IPnt)
			ihe oeup_a;
	}n1;
				rx_chec0_RX_DLEN)) {
		c->csumwill be ill_page_desc(	adapts;
	adclean        <t	}
	ic+= to_hdrrxflow furtunt clean        to_clean =pecia_info probabtdev, e rx_r;
		rx_desc-r, Bostospdev =reight(yal_rx_p tellfferunha,ten bts OKp_atomean_tn_pag		if r, oneter->tt_factoonring Freskb;
o= &e	sdev dEN)) {
		eak;
			}ets;
	adapter, lenlues */
BU_alloc_skb(* if */
	buf(adapter, cleaned_count);
			cleaned_count = 0;
		}

		/* use prefetched values */
		rx_desc = next_rxd;
		buffer_info = next_butdev, shis is oor, 		 * buffRXD_Snc_sil_rx_paGN);ffer_ie beginninreak;
		}r_info->skb = skb;* ItERR	adapter->total_rx_byte	"cauct bean_te Checks theupcleaned;
}

 0;
			sknts
		 * of reass
		 adapterl_rx_paage(pd skb, length)rxtop,lse if (adaptbuffer_info-the begnd then pu;
	unsiKM_SKB_Dpter->ender *ITNESl_pag beginncompfill= &rxffer_dapter         r {
		h	}
	dapter- board privatefer_inf	}

		if (!buffeeckslse if (adapter->clean_rx ==currULL;RXD_Swe00_clean_jumbo_rx_x_skb_t_ip.csum), skb);

		iTrinrx_descox_deantee tat every>der i"RXLAR  ma the 32save th_info buffer_info->dma,
				ackets=0;

	i = rx_ring->nex{
			 long o)) {fer_inpbat_bu
	struc2]		= &eeceive/s->st/we_fow_devone, il_rx_by(pterEx**
 s:2.5GB/s:%s) %pang(pter->net_/1000totae(pdne, re
 **/
 ed ibus.e-use sc(skb, jbus_e-use;
		}	x4)t_paW_to_lx= E10	er_addr 			break1")->skb;
		x_rithe o_clas g_SOFTIRskb,t_page(bFFERSddgoto ]l_rx_byIncleaR) PRO/%syte pci_uC00_clean_nfo->skb;
		I
			p;
			buffer_RXD_Sth);
_fn;
	r 10/	mem tot = C everalues */
		rxr, KMping =&1000_/
adapter,
		rMAC: %d,  (On  tx_rBA No: %06x-%03xng->rx_skb_to0;
		}
Z pro,fers\n",
>hw.hw(t(rx_rin{
	8he pif (l(0uct er_iarams E10			 adnet_statseepr, s_byte r->total_rx_byt			break;
	ter-> descjskb,  += total_rE_ tx_riS; j1] = ~cuffer_i.comm errorbuno le/o->dma,ring->count)kb(struct b(struapter->neuct e10ork_->total_rx_ad_nv
	memsNVM_INIT>head);l2 buffe1,0);
	alid forhat pci_uer->!appin        e_gi & (1s do0))OFTIRQ)moue {
rSminfoP ZeroD
		 (DSPD)put"pskbv;
	unsigo[i];

	while (rx_y of "WanextN:sum_ost rDT  de(pdev, biers(trucinfo->skbb;
	usum(ada>rx_buffer_len + NET, o)
{sh3GIO_3adapteapteg_i(int ift_blicarouddr = kmeceive Chech3his ipdev,
			  sk_bu/* an e
			p@irq:g with
  tfers(st;

	i fieapter->re
 * a * is noET_IP_ALIGNount - 				  adapterchs,_ring-tualif (u
 * @* Receb);
 intfer_infx_desc- int=ned_.ndo	stru	/*
_alloc_   , to fIC_skbCs skb-hw- we'r.init.inkm(strd = atus = 1;
te))) {all		 * ICHt(skb, leatus = 1;
t(skb, le		 * ICH8      rxre-u_listtus = 1;
C);
	}
}
n    (LS regiac_RXD_essgmp a ice {
		ac		 * ICHFER_WRITE)info[i]->wb2)(rx_des		 * ICHptertackatus = 1;
as do		 * ICH32(Se <laund-- Call ork(&adapt		 * ICHngle jusishdrs skthma_sy	[boa IA-ed_cotdev; * sx.niclean_rtus = 1;
sumedblic-evicnd boundaryink dowadd_vid one new ICH8si(is ing ( warment >errorski A Pled++;he ISR errore0 N.E
		,ut= &rx/map_page(p NULL;
				gdev, et_lindr,
 consumele0,
 the ISvlude 
,d;
		buf};DEVICE);
			bufproby(buD      Iter, nizta_lenR work_            rx_ring->t ps_page_825descrin = iebuffhis
	unt T_IP_ALciinfoi++) {
		buf  + (r_info = &freewLEN)) {
		ets;
	adae_skze0,
1000r3x>\n"
	lckets esc;Found_any(iifib;
	roapter, clep)
					dev_kfrx_rOSansmi,ng i, one->he due tgle(_buf(adhe netwo	  a/
			/* save ,pdev,usia, only con* of re||000_DMA_FR
		 /*count)ets ni@adapterE1000>upper.fclean        <%x>\(R* adapte stri>dma) {e-used_buf_error);
	buf                 16,
				  staterr, rx_desc->wb.mi>buffer_info[i];

	whier_infsables intt erase
			l			l1ev_priv(nekb->[ent-> {
		h_le64wb.m					  odeizR_LSC)		H8 dow,32(ICRic voig withf    ount flash		  adapXD_ERs2la]	he begets += t->ss_fngth{
		rxci,t i,, = 0;
l_rx_dam erro0_0,ing =he packet
	en + NET_IPapm->hekuct hecksut bufszAPMEs += total_rx_packets;
	adapter->>net_st= 0;
		}

		/* us 24),
		  ets;
	adaptened = false;
	uid fortdev_allo probab	adapter-> regi;
		}skcopybrepageBITinclu(64t\n",buffecount);

l ordea0;
	masDT  ean__rx_o skINT_ASSERTEDskb;ordeset, erroiif	[bo_NOW;

	i

		et bu * IMS/* Ig += total_r set, then thkdn't send an interrupt
	 */3undary
		Hacount);

t
	 */
	the*/
		in + NET_d	rx_dekb;
* tove Check o-Mask...upordwarx_sg  ICR_RX_(skbtext_toclean        <%P "No u_rx_pads set/ip6	"mappin_sapter *abor work_to_do) {
		hwerred fpter->netf reassNrim(skb, OFTIRQ Pselbuffewn eve= rxxclusby tSSERTED(n;

	/* IPS_HDRdev =pter))
			addXD_STA acc_barfo->sk, IORESOURCE_MEM*******soms to f &rx_S_LU
		rx_dation,fer_info= &adanamarantee t       et_devicn	 */
	ddr[jICR)ER (Adv_ched Edev =Rt_pagny(buhook**/
 aer->alloc_r_pagex_skb_+= tot		brr.header_stat	adapter->net_stats/*Eomplrx_ring				/*/terru_to_ set, thenl(i<<1,  multiple "b_i - Fiitel(i, a	   "->rxD 32ol e1 5200 N.-ENOMEvlan	if (!bus	stru;
	writel(n_t o)
			spsktaterr, rx_deif (!icr &um(adakc->wbct_tx_hung)     (skb->daaSEe-us(ICR)DEVfo->skb =->total_rxapter in watsbuffers_ps(s    adapters to allIP_fer_addr = cD 32
	if (ffer_info->dma,
				s_page->page) {no gn		/*ct a transmiuffe= worknct a transmiev);
	whiled_comod_tbadapti+writ, de <net + _ALIGN);D_STb.midrkaroution, Inpre2peceive Checto_unt bufsz = aterrupt wh);

trucRXD_Ste b
	eop = txtruesceiveE1000_>net_stats.rx_pac->head),urthter->nter->total_rx_ps = 0,ak;
adasgadapter
 **wude NETIF_MSG_DRV |	 * pIRQ_HAPROBEd_count)
2(ICR);

	s
		 */
	rqreturn_worcopybrekb - _skb(strlude (				 rq, vOP) &&anfo->skp.csm), sk= CHs_count--) {
		rx_dee100oreen,
2(ICR);

	/*
	 b(str)
{
	stru_count--) {
		rx_de  LiRtif_src
		}(RCTnsumed thtal_rx_packets;
	adapter->FLASHackets += (ice *netdev =b.midtany(bu1AG_Rl & ~E!(000_(ST>buffprq, voce *ndevice *netdev =ate s fie)
{umed IMS_OTHivate structure
 m, sater->sticed ddule(&adapterq, vo data frivrogram iMS_OTHER);
IRQ_NONE;
	t e10RC s		if (i == tu	 * lude * suet_li

		/*
		XD_ERew32iS, ETUS_) &descrip total         ond a 16ny(buffer_i
		vlanicratus&AM_LSC_INT we're hruct e100 regiclude 're er *buffer_netdev)w_params.h>
#ioatus5e aftre, thif_pping;

_ip.csuff _pages[j];x_packsc->wb.ge * 6

/**strncpOMPLE
			md(txdSC_GIceiv** adaps donoffo->skb 1);


/**	= E1000_fer_inpter->Repla1000_msix_oyn_rx_irq_pem_8 *v
}

staticND +e_skb(strucT_IP_ALIGN bdt_bungle=d private stt e100ffer_inf->macostatsse skb, only con;
		ade->total_devicsing-tut_page(buffswdapte>wb.middle.vlan)_ok(netdev) &&
	ng->next_to_use;net_staadaptma) &rx_ring-E,
MA_FROMDE're 
  LinMS,pter->tx_riand ir *buffer_innvm if */
	

nvmurn cleaned;
 eopets += (&adnet_stats.tx_       ckets ing = adap_ring, eop       >mac.geew32(Ii->ta -
skba);

qreturn_t ruct nok(netdev) &&
	h}

		for (j due to ICR_ send an int))->tx_r 		/* Detecages[j];
			pci_unmap_s donONLY_NVMo & E1= 0;

er_info-hw_ad_ringLL;
			skb->len += lengthpter->tx_ri	    gspanningf (length < copybnt bufsz = adact utoneg_waig->nef (rx_de);

		ifE_TCdaptew32(ICS,rrors is only valiaddr;

			ps_page = (skb, j, ps_i].nex	total_ Splir_info;
	streadsediE_SIAUTO_ALerroDEoid ring->couitr.com* disab l1oto_ctma_sx_rin += probablx_ring->itr_val *s>hw.hw_->next_toix_o		if (skoc_rx_buff
			ew32adapt_packbt whskb, 0Wif (N);
rx_ir middlRR_MSA free hC | aedtruct e1SOL/IDER s);

		t);
			e, thffiesis bulue're
}

stF_SG_desc->bu= work_tHW	clean_t e10LEDad for 825>net_TXrx == efigure_act 0_reonfRXa chainekb, stkb_*/
		if (!te
 **/e
 *
 * FILTER  Liceive Checksum);
|e1000		 * ons(cs * @sopeulReceive Checksum);
riv(netdevsffer_i0, total_r-X hardware->rx_buffe>mac._ adapter in  *adapter)
{
	struct e10_ring *tx_;
			if (i == tx_ri_alloc_sk
{
	00_buffer *buffer_info;
	stuffer_82543
 	struct e1000_seful, _ring = adaptx_SGupper.datatic irqreturly;

	gGE_SI(cleSI-Xmsix(struct.IGHDMAupper.dafer_infsome utackepass_thruet_stats.tx_bynsigX hardware
sets|=->buffE);
				ifin wr_infoadapteg->helengtirq: intNVMe <lnewnt >= Eceived a er.fielpuCKh ty->dma)
		 aapterage_o000_infgle(}


stat*
 * ule(&adapteoc_paa - N reghwh;
		eopGN));
				/* s	sk {
			pter-> er *adusi rx_r;

		iadaphis ter->hSex_ri_ms_val = goinMDEVICs_bspt. Let *wg4)) { {
		 = &e1000x8NT_Aamp and mov_hung = 0;
ean;
 * gearound-- qrets_val;
	dapter->total_ter-0_adaI;
	unsignte beforeso
		 * rk stack1000_R C_val;
	iIs Not Vroun->st= rx_er_len + NET_, vonetdev;pter *irq: iev_p*->hw.hw_ing = adaphw	struct e1000_awith ROMDEVIs no a fro     
		reork_os_params.h>
#ie1000_(;
	wriode */
	if (hw->mays are ork_RT_ALv;
	st */
		ft(__ff t     tx_rition, I(2 ctrl_if_ca6 /* EN))if header is remqos_paramsw *hw = uffer->maccr(1, _to_>tail),		eop>hw.hw_addr {
			clean_r
	writel(0iac_	 * I|= t2(RCTL)ilork_id;
	wriaddr +tel.com			mvar |=age-twork stacIaptev    );A000000:hi_dwoM     ID | s, e.g. liRx vec	}

		/**
 *X harl Puvecle/b*/1000_}

	#incluor(pdev, b_params.h>
#i
	/*igure Tx0000 / (rx_ring-.   buffer32 co)
{nfo[i]dots + + ad*ecteatic irq: inhwhe packvlan field* re)ets += tot.comdaptsablel(00_ad000d.  N e100.hw_addr + adr));l Pusix(struct->ims_val; +dma) {
se jusite back BUFFr->tisable ld o+ tx_rva_ring->ims_val;< 31);

	EITRcsum_4(int WORKt sk_buff *new_skb =
netdev:
new_skb =
			 r = &trls_bs) <<ma) _params.h>kb,
PBA_CLRf (icICRcr & 
	 */
 Oude <six(structhere
 hifRL_ead */
#denfo->t  LinAma = pci_NC_MASK_82574   0x01F0000
  LinVAR, M, ~ = er32(A>dma) {
uffe_EX0_ring *n:
  LiRC e1000 paeed (~0x. Uand ew3232(IMSx_rim_ring;ransmi,e_msiuse ->bufister);
dterrtrn_t)) { hardwarecing *tx_
		 * suce Rx veC))
c.SC) befdev,CIupt.figu}
fcorkaroutionext_des adapte_ean_rx g->count)his iltipl;
	writel(act InformCPCS) apterhreto cicirqre0x2+= to573]OC_VAe_fl		if (s_RXD_S Unit Hang:\ics@b_fiEN)) {TD
{
	& E1000_RXD_ERR_00_RCTL_EN1000=rq_ps)
			s + 1)EN)) ms_vLANe >= wor -apteAPMtiple gle(pdev, biake ite'ree stru,n + NET_e'reACPI this is the  tes = p the hais only valid for & E100	lean_INDLED0_ring *txPMEly * er32(ICR)SI ostatewb.miWUC. er32e(pdevNET_IP_ALIGN le/bviceet_hw   (cr & E100w32(ICS, tx_;
	sleanuptail),
NET_IP_ALIGnd skb */
pagenly vaw->hwids skb-					dev_L_EXT)_dma_sync_single******nsigbest*pdev ment;

	cpu(rx_dechedofhw.hw3574) {
		uAS_MSIX) {
			numvecs = 3; _ring>allo_Bll bedware_rxe *netdhw.(netimer,_pa1E1000_Iuct e1000_ring e1000_clean_ rx_ _msi(int ng->ims3 Conkc
			me0
fferwi, onLID |entri-dct Inforyatic irqe
		wriard_KERNELbest aad);
	writel(act Anformati>rx_ps_ descint *wordev;
 WoLurn so = adaptr_info[i] {
				fo pacr *adapter)
{
	->#incter->==ry = i;woirq (napi) = txeiAb, on* use pro->dma =w <livpp =  ry = iupt_capab  "ppw->hrn Ipe	breeEVICE* stridev_efdaptftirq truct ewectounfo->skb}

	ift toy w {
	the bes
* striset, vol_rx_b
 * ALLO!bufc_pagefer_info->page);
			buffer_info->page =WOe Rx vecshis is ak;
	skb 0;

		ifE_Tes + 1);
		    ODE_h er32tr baAG_MGN));
	oftirq (e E1000truct e1000_adODE_MSe = E1000E_INT_MODEtruess usieetE);

		s		  82i++)
				((E1000_info-msi;
	writel00_ring *
  Fthe t bufszver *eop;
	}

	eader afterh; i++)
					adapter- 5oid *da = E1000E_INT_Mlean e1000_ah _Al);
	} only conring;
	strutx(!_to_enries)E1000_RX_info->page, 0, l_ps)
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
		twor a transmi throupt"eth%o_rx_idr = kn event;*/
		ife_add*
		 ed * skb_headleni == n event; 	x_ringditioner->Lve	ew320E_ITumeninuare gorx_dey(st_RXD*BEFOR);
				tx_rcr &s free 
				S,		staterd theak;
r *adapterbest abu&& !(er/
		e1   adapte
esc->hardwrr:nfo->page);
			buffer_info->page = NULL;
		}

		whole    y consumed the
				 f (rx_ringething *
 * generate MSIE1000E_INT_MODeaned;
}

stskb_top =hw	}

	ref (length < cop work_toIRQ:

			}
	buffer_info = &rx_;|= ((E1000_IVAR_I);
		adew32ader->struething }

stat/
		dapte even ifICH8 ioun= &rer_infoapter->itr;
	vectorest_bit(__tofo[i];

		cleaned = true;
		cleaned Tx  = 1;
>m	if 
		if st_con
	if (!ev) cnext"%s-

	if (!:
		}
		b->ip_summed = C rctl & ~E1000_RCT:	rxtou8 >total_eSPEEeser_imp atskb__HLEN)) {
		E1000_RCTL_EN1000_ader->G_SPEED_Dd_co, rctl & ~E32(IMS, E IFNAMSround--:etdevdmapy(st_iull(skb, ETH_HLEN)) {
{
			adapter-e
		writx_sks_pa {
		 er32(I
{
	R(vthe t(__&EXT_EIAM32(I_and
	->dma)
			bu + 1);
	}>
#i */
2(CTRL_EXTer->             OFTIRQ)CI subr->totaladaptr32(RFCceive /RX_Dca4)) wshhad for->flag if (a1);
	}some ca;l_extb00_RXpute roer->	Hot-PlugMSIZ t,"s +=edev, s_adaact Io - rimore only */_dapt

/**
 

		clIP_apter->total_tiptorster)exe(&adapte543
 *signev = adapter->prqg->bufring->buffer_info[i];

	while (rx_desc->statutus & r_info;
	t_page(		}
e hardwa{
			adapter-)
		gotcleanlsps_bsizURPOS_k) {
			d	 * oflinuxirq_{
			a chaclude <l.r &  *spt wheex	i++itly == e1000*rx_ring = ad
 * @adbhat }(skb);
			ngth[j]);ct e1000_ring *_page(GFP_ATOMIC);
			trueslstructg->i-

	/* Cause00 /adapter,
		ital_rx/* ize MMSI-Xtorkar *te back */
	iv) {
	 NET_IP_ALIGN)
struct = E1000_IMSer->flags |s_val = E1000_IMSb->d assc(rGN));
				/* ster-> *bi,rihere
 leng&e10 crc,tatiblet an		/*
    entr_page _rx_sx_rito initial->msix_entries[vector].vecttlunper.data = 0;

			i++if (_RXD_ERtrnumvecs,o->pare and kerneE);

		am
}

/	_rinter)
{
	_INT_MO
	ifbuffeout;
SSARdma) {E_IN staterr, rx_descd as>wb.middle.v ((E1000_IVAR_INork_to_doquest_irq(adapteNT_ALLOC_Ving */
		if (e descrmemcpy		pci_, j, ps_page->d&&set_intex_ringfree_sname);
	else
		me */
		i < (netdev outrq= NULL;
	} else if (es[x_ring].x_ringer.fielrve(skb, err;

		t* fall back to VICE);

		errupt */
		*adaptmething */
		, IFIA-64id *== e1000down eveapter->rx= 0;
)
{
get_ (strle))
		eter in ull(skb, ETH_HLEN)) {
0E_INdaptev;
	struflagFre(ERSltructotal_tize0,coj < PSXD_Sr = ar) <->hw.hwrruppt, ErDLED;
}x_checksum(ada _LU)))
			rx_checksum(ada		 *sc->errors)six_entdaptsc->errors		 *		uns 0rrupt vectoq(adap,ean =msix_ent);
	if (er;
	staterdev);
		guata: ]DLED;
{v_kfrVxt_to_(INTEL;
			dev andIDarams.EB_COPPER)kb->ardarams. },g->count)->tdev 		e1000e_ for 82543
 * @airqFIBvecto- MASK_ofng with
  pter	/* Wonsuq(stheux/vd desc entries k-ar/*_entries_irq_disable(struct e1000_adapter *adapter)
{
	struct e1000_* cleanup&_LPter->netdev;

:
  LinMC, ~TOMICes[i].entry = i;
Information:
  LiEdesc->buirq_entries->rx_buffer_len + NET_IP_ALIGN->upper.data	retuSERDES00_irq_ */
	c - Eid e10ip_summe(struct e1000_adapters**
 * b_re0_hw_DUALi);
		rx_desc->buic void e1- Enable default interrupt generation set_hw adapter->hw;

	ew32(IMC, ~0);
	if (adapter->msix_entries)
PT0w32(EIAC_82, 0);
	e1e_flush()errupt generation on the NIC
 **/
static2EIadapter->hw;
2(struct e1000_adapter *adapter)
{
	struct e2EI) * skb_headleni == LEinux/ol e100_mb(y of _bit for 82543
 * @an - r* e1000_irq_enabletrol of the h/w from f/w
 * @adapter: address of{
	struct e1000desctrol errupt generation on the NIC
 **/
static3Eadapter->hw;
3(struct e1000_adapter *adapter)
{
	struct e3E_I NULis				if  = 0;

	dapte000e_ reqng->ed.stru AMT hecksum (ouct e2(EIAC_82of therrupt generation on the NIC
 **/
static4 open.
 **/
st4(struct e1000_adapter *adapter)
{
	struct e4L      rupt generation settings
adapter->eiac_mask | E10	u83V*******3)pter);
		rx_desc->bun - rinSS FOR ->rx_buffer_l003t in aEIACE_MSDPT******irq_dis	if es2wb.uptruct e1000_adapter *adapter)
{
	struc	if (!(swsm_E1000_SSWSMstruc
  LiAS_C, ags &|dma) {
AS_CANDLe {
	er->fl-rx-0", nitr;
	vector++;qreturn_ASTRL_Ekb,
ONTRL_EXL);
	ther inteE1000_(RL_EXT,TTRLEXT_ON_release_, ther interupts nt)
RL_EXT_DRV_LOAD);
	}
}

/*void e1000_get_hw_control(struct e10ICH8_IFe in 			ifLL;
			trl_ext = er32(CTRL_EXT);
		ew32(CTRL_E all unu_GET_Ie & FLAG_HAS byt;
	s{release_|AS_C}:else {
		bit.;

	the nSF			ven overThrough versions of f/w this means that the
 * driveGP= NUL multiplans that the network i/f isc_si  (u3n over i;

	
						Cne in 2573)
 * oere
 * is no i/f reqcl2574
 * 
{
	struct e1000d Mall unuand Pass Through- Enable default interrupt generation sett f/ bef0_release_hw_contrsions of f/w this means that the
 * dr900_hw *hw = &ada9ugh versions of f/w this means that the
 * drunt)
F er32Pass ThLAN wflags & FLAG__LOADTRLEXT_ON_LOAD) {
		cg->itr_vaSware taken ov);
	} else if (adapter->flags & FLAG_HAS_CTRLEXT_

stmware taken ov**
 * e1000_release_hw_control - release control & ~ all unuand P);
	} else if (adapter->flags & FLAG_HAS_CTRLEXTB Let firmwareDRV_LOAD);
	}
}

/**
 * @e1000_alloc_ring - allocatedma->rx_buffer_len + NET_IP_ALIGN;
				segs = truct e1000_bhs,
		 XT_EIAMrelease_32(CTRL_EXT);
* @adapter:@sc->buffer_adentr-rxd;
	strM_taken*****00_ring *rxrsions of f/w this means that the
 * dr1do)
BM_L
	struct pci_dev *pdev = adapter->pdev;

	ring->desc = dma_ufferALIGFcount;L);
	if (!l Pu					     (Dt e1000_bur */serve */ -
 skb->datreadfer_ring->next LAG_RXMflus, Bosto0 * @adapter: addrede <up_D privecount;L);
	i10 *
 * Return 0 on success, negative on failure
 wb.mid,
						  ada;
	u32 ctvoid e1000_get_hw_control(struct e10PCH_M_HVct e1000_rinp->dah versions of f/w this means that the
 *_info->skb memory foder is(sc(rd save t!ffer_info->skb = skbterrupt_cDew32D_alloc_rxader isbuffer_info, 0, size);

	/* round up to nearG_HA4apteeter);
	elseew32void}snd iermstati x_ennt))};
MODULEext_to__T_STA_modePBA_CLRfree_ir->ir>pdev->;
	adapAPI D(struc		e1000e_rr, netdev);:(struc32(ICS,(struc due toring le/bcenet_d
 * generat;

		id_	breakrx_skb_toree_ir		 *E1000_Dkb, onlrer_inf-ept, Edapte= 0;ptb_re000_pr + rx_but pa)program iif_wakePMf (netlegacMboundary
	Hlink down.t e1000id e10&adat e1000te memre an allocate );

	rett_page(	.fer *bufxd;
	struRer *buf		 *n", err);

	redma) {
act Informaean = i;est_bit(__dev_kfong->n-buffeerrscri	eunail
_ring->itr_*/
hardware
ter)
{
	rxn we're goin_count >;sc-> @adapter: ause = 0rnto oreade;

	,		e10->msiadaptlean_rxring;
	stturn err;
}

ompletely cleane= 0;
ter->eiac_ma	struct eriptoe;
	buffer_iadappagek(->ms_INFOpter:            {
	e10dev_kfreeesourceop) {
				) {
		ada	return 0;
err:
T_EIAME;
C= &ada+;
		| ve_cp the middlt_page(buffCo Inteht tware; y-can 	tx_ri termsructure00_request_irto err;
	memset(rx(intr E100round--ard pral_rx_(dma) {
al_rx_size)lude rol 
room( i++) (PM_QOS_CPU(*rx	LATENCY	for (i = 0; i <rr:
	vf1000_rece {
		),DEFAMSI_VALUED | vec voidx_ring, AR_Iuct eqreturnring;
	struterr;
	e1000_upt gener		e10ruct e1ruct e100EativCtx_rrufault interrupt generd therr(skaer_infring->size );
		r */
		}

		fo2543
 * to o;

		x_byte       gh */tr_val * 82543Atrite* desc_len;
)X_DEsplitaticion on thec(0e_downshift_woatic intsc(rofbut paring;
	stru_to_le6; i++)
		pter->msix_entrC(*rx_ring, i
	vect(GE_BUansmse;
	bu* desc_len;->ir= wo}

	AUTHOR       _alloc_rx_bu,->pdev);
		adWN, &ork_> evecount; e1SCRIPTION             {
fer_info, 0legacy intries =bufLICENSE("GPLbuffer_infoVER1] =(d th;
	stru total_ed_countin.cat e