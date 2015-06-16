/******************************************************************************
* This software may be used and distributed according to the terms of
* the GNU General Public License (GPL), incorporated herein by reference.
* Drivers based on or derived from this code fall under the GPL and must
* retain the authorship, copyright and license notice.  This file is not
* a complete program and may only be used when the entire operating
* system is licensed under the GPL.
* See the file COPYING in this distribution for more information.
*
* vxge-main.c: Driver for Neterion Inc's X3100 Series 10GbE PCIe I/O
*              Virtualized Server Adapter.
* Copyright(c) 2002-2009 Neterion Inc.
*
* The module loadable parameters that are supported by the driver and a brief
* explanation of all the variables:
* vlan_tag_strip:
*	Strip VLAN Tag enable/disable. Instructs the device to remove
*	the VLAN tag from all received tagged frames that are not
*	replicated at the internal L2 switch.
*		0 - Do not strip the VLAN tag.
*		1 - Strip the VLAN tag.
*
* addr_learn_en:
*	Enable learning the mac address of the guest OS interface in
*	a virtualization environment.
*		0 - DISABLE
*		1 - ENABLE
*
* max_config_port:
*	Maximum number of port to be supported.
*		MIN -1 and MAX - 2
*
* max_config_vpath:
*	This configures the maximum no of VPATH configures for each
* 	device function.
*		MIN - 1 and MAX - 17
*
* max_config_dev:
*	This configures maximum no of Device function to be enabled.
*		MIN - 1 and MAX - 17
*
******************************************************************************/

#include <linux/if_vlan.h>
#include <linux/pci.h>
#include <linux/tcp.h>
#include <net/ip.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include "vxge-main.h"
#include "vxge-reg.h"

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Neterion's X3100 Series 10GbE PCIe I/O"
	"Virtualized Server Adapter");

static struct pci_device_id vxge_id_table[] __devinitdata = {
	{PCI_VENDOR_ID_S2IO, PCI_DEVICE_ID_TITAN_WIN, PCI_ANY_ID,
	PCI_ANY_ID},
	{PCI_VENDOR_ID_S2IO, PCI_DEVICE_ID_TITAN_UNI, PCI_ANY_ID,
	PCI_ANY_ID},
	{0}
};

MODULE_DEVICE_TABLE(pci, vxge_id_table);

VXGE_MODULE_PARAM_INT(vlan_tag_strip, VXGE_HW_VPATH_RPA_STRIP_VLAN_TAG_ENABLE);
VXGE_MODULE_PARAM_INT(addr_learn_en, VXGE_HW_MAC_ADDR_LEARN_DEFAULT);
VXGE_MODULE_PARAM_INT(max_config_port, VXGE_MAX_CONFIG_PORT);
VXGE_MODULE_PARAM_INT(max_config_vpath, VXGE_USE_DEFAULT);
VXGE_MODULE_PARAM_INT(max_mac_vpath, VXGE_MAX_MAC_ADDR_COUNT);
VXGE_MODULE_PARAM_INT(max_config_dev, VXGE_MAX_CONFIG_DEV);

static u16 vpath_selector[VXGE_HW_MAX_VIRTUAL_PATHS] =
		{0, 1, 3, 3, 7, 7, 7, 7, 15, 15, 15, 15, 15, 15, 15, 15, 31};
static unsigned int bw_percentage[VXGE_HW_MAX_VIRTUAL_PATHS] =
	{[0 ...(VXGE_HW_MAX_VIRTUAL_PATHS - 1)] = 0xFF};
module_param_array(bw_percentage, uint, NULL, 0);

static struct vxge_drv_config *driver_config;

static inline int is_vxge_card_up(struct vxgedev *vdev)
{
	return test_bit(__VXGE_STATE_CARD_UP, &vdev->state);
}

static inline void VXGE_COMPLETE_VPATH_TX(struct vxge_fifo *fifo)
{
	unsigned long flags = 0;
	struct sk_buff **skb_ptr = NULL;
	struct sk_buff **temp;
#define NR_SKB_COMPLETED 128
	struct sk_buff *completed[NR_SKB_COMPLETED];
	int more;

	do {
		more = 0;
		skb_ptr = completed;

		if (spin_trylock_irqsave(&fifo->tx_lock, flags)) {
			vxge_hw_vpath_poll_tx(fifo->handle, &skb_ptr,
						NR_SKB_COMPLETED, &more);
			spin_unlock_irqrestore(&fifo->tx_lock, flags);
		}
		/* free SKBs */
		for (temp = completed; temp != skb_ptr; temp++)
			dev_kfree_skb_irq(*temp);
	} while (more) ;
}

static inline void VXGE_COMPLETE_ALL_TX(struct vxgedev *vdev)
{
	int i;

	/* Complete all transmits */
	for (i = 0; i < vdev->no_of_vpath; i++)
		VXGE_COMPLETE_VPATH_TX(&vdev->vpaths[i].fifo);
}

static inline void VXGE_COMPLETE_ALL_RX(struct vxgedev *vdev)
{
	int i;
	struct vxge_ring *ring;

	/* Complete all receives*/
	for (i = 0; i < vdev->no_of_vpath; i++) {
		ring = &vdev->vpaths[i].ring;
		vxge_hw_vpath_poll_rx(ring->handle);
	}
}

/*
 * MultiQ manipulation helper functions
 */
void vxge_stop_all_tx_queue(struct vxgedev *vdev)
{
	int i;
	struct net_device *dev = vdev->ndev;

	if (vdev->config.tx_steering_type != TX_MULTIQ_STEERING) {
		for (i = 0; i < vdev->no_of_vpath; i++)
			vdev->vpaths[i].fifo.queue_state = VPATH_QUEUE_STOP;
	}
	netif_tx_stop_all_queues(dev);
}

void vxge_stop_tx_queue(struct vxge_fifo *fifo)
{
	struct net_device *dev = fifo->ndev;

	struct netdev_queue *txq = NULL;
	if (fifo->tx_steering_type == TX_MULTIQ_STEERING)
		txq = netdev_get_tx_queue(dev, fifo->driver_id);
	else {
		txq = netdev_get_tx_queue(dev, 0);
		fifo->queue_state = VPATH_QUEUE_STOP;
	}

	netif_tx_stop_queue(txq);
}

void vxge_start_all_tx_queue(struct vxgedev *vdev)
{
	int i;
	struct net_device *dev = vdev->ndev;

	if (vdev->config.tx_steering_type != TX_MULTIQ_STEERING) {
		for (i = 0; i < vdev->no_of_vpath; i++)
			vdev->vpaths[i].fifo.queue_state = VPATH_QUEUE_START;
	}
	netif_tx_start_all_queues(dev);
}

static void vxge_wake_all_tx_queue(struct vxgedev *vdev)
{
	int i;
	struct net_device *dev = vdev->ndev;

	if (vdev->config.tx_steering_type != TX_MULTIQ_STEERING) {
		for (i = 0; i < vdev->no_of_vpath; i++)
			vdev->vpaths[i].fifo.queue_state = VPATH_QUEUE_START;
	}
	netif_tx_wake_all_queues(dev);
}

void vxge_wake_tx_queue(struct vxge_fifo *fifo, struct sk_buff *skb)
{
	struct net_device *dev = fifo->ndev;

	int vpath_no = fifo->driver_id;
	struct netdev_queue *txq = NULL;
	if (fifo->tx_steering_type == TX_MULTIQ_STEERING) {
		txq = netdev_get_tx_queue(dev, vpath_no);
		if (netif_tx_queue_stopped(txq))
			netif_tx_wake_queue(txq);
	} else {
		txq = netdev_get_tx_queue(dev, 0);
		if (fifo->queue_state == VPATH_QUEUE_STOP)
			if (netif_tx_queue_stopped(txq)) {
				fifo->queue_state = VPATH_QUEUE_START;
				netif_tx_wake_queue(txq);
			}
	}
}

/*
 * vxge_callback_link_up
 *
 * This function is called during interrupt context to notify link up state
 * change.
 */
void
vxge_callback_link_up(struct __vxge_hw_device *hldev)
{
	struct net_device *dev = hldev->ndev;
	struct vxgedev *vdev = (struct vxgedev *)netdev_priv(dev);

	vxge_debug_entryexit(VXGE_TRACE, "%s: %s:%d",
		vdev->ndev->name, __func__, __LINE__);
	printk(KERN_NOTICE "%s: Link Up\n", vdev->ndev->name);
	vdev->stats.link_up++;

	netif_carrier_on(vdev->ndev);
	vxge_wake_all_tx_queue(vdev);

	vxge_debug_entryexit(VXGE_TRACE,
		"%s: %s:%d Exiting...", vdev->ndev->name, __func__, __LINE__);
}

/*
 * vxge_callback_link_down
 *
 * This function is called during interrupt context to notify link down state
 * change.
 */
void
vxge_callback_link_down(struct __vxge_hw_device *hldev)
{
	struct net_device *dev = hldev->ndev;
	struct vxgedev *vdev = (struct vxgedev *)netdev_priv(dev);

	vxge_debug_entryexit(VXGE_TRACE,
		"%s: %s:%d", vdev->ndev->name, __func__, __LINE__);
	printk(KERN_NOTICE "%s: Link Down\n", vdev->ndev->name);

	vdev->stats.link_down++;
	netif_carrier_off(vdev->ndev);
	vxge_stop_all_tx_queue(vdev);

	vxge_debug_entryexit(VXGE_TRACE,
		"%s: %s:%d Exiting...", vdev->ndev->name, __func__, __LINE__);
}

/*
 * vxge_rx_alloc
 *
 * Allocate SKB.
 */
static struct sk_buff*
vxge_rx_alloc(void *dtrh, struct vxge_ring *ring, const int skb_size)
{
	struct net_device    *dev;
	struct sk_buff       *skb;
	struct vxge_rx_priv *rx_priv;

	dev = ring->ndev;
	vxge_debug_entryexit(VXGE_TRACE, "%s: %s:%d",
		ring->ndev->name, __func__, __LINE__);

	rx_priv = vxge_hw_ring_rxd_private_get(dtrh);

	/* try to allocate skb first. this one may fail */
	skb = netdev_alloc_skb(dev, skb_size +
	VXGE_HW_HEADER_ETHERNET_II_802_3_ALIGN);
	if (skb == NULL) {
		vxge_debug_mem(VXGE_ERR,
			"%s: out of memory to allocate SKB", dev->name);
		ring->stats.skb_alloc_fail++;
		return NULL;
	}

	vxge_debug_mem(VXGE_TRACE,
		"%s: %s:%d  Skb : 0x%p", ring->ndev->name,
		__func__, __LINE__, skb);

	skb_reserve(skb, VXGE_HW_HEADER_ETHERNET_II_802_3_ALIGN);

	rx_priv->skb = skb;
	rx_priv->skb_data = NULL;
	rx_priv->data_size = skb_size;
	vxge_debug_entryexit(VXGE_TRACE,
		"%s: %s:%d Exiting...", ring->ndev->name, __func__, __LINE__);

	return skb;
}

/*
 * vxge_rx_map
 */
static int vxge_rx_map(void *dtrh, struct vxge_ring *ring)
{
	struct vxge_rx_priv *rx_priv;
	dma_addr_t dma_addr;

	vxge_debug_entryexit(VXGE_TRACE, "%s: %s:%d",
		ring->ndev->name, __func__, __LINE__);
	rx_priv = vxge_hw_ring_rxd_private_get(dtrh);

	rx_priv->skb_data = rx_priv->skb->data;
	dma_addr = pci_map_single(ring->pdev, rx_priv->skb_data,
				rx_priv->data_size, PCI_DMA_FROMDEVICE);

	if (dma_addr == 0) {
		ring->stats.pci_map_fail++;
		return -EIO;
	}
	vxge_debug_mem(VXGE_TRACE,
		"%s: %s:%d  1 buffer mode dma_addr = 0x%llx",
		ring->ndev->name, __func__, __LINE__,
		(unsigned long long)dma_addr);
	vxge_hw_ring_rxd_1b_set(dtrh, dma_addr, rx_priv->data_size);

	rx_priv->data_dma = dma_addr;
	vxge_debug_entryexit(VXGE_TRACE,
		"%s: %s:%d Exiting...", ring->ndev->name, __func__, __LINE__);

	return 0;
}

/*
 * vxge_rx_initial_replenish
 * Allocation of RxD as an initial replenish procedure.
 */
static enum vxge_hw_status
vxge_rx_initial_replenish(void *dtrh, void *userdata)
{
	struct vxge_ring *ring = (struct vxge_ring *)userdata;
	struct vxge_rx_priv *rx_priv;

	vxge_debug_entryexit(VXGE_TRACE, "%s: %s:%d",
		ring->ndev->name, __func__, __LINE__);
	if (vxge_rx_alloc(dtrh, ring,
			  VXGE_LL_MAX_FRAME_SIZE(ring->ndev)) == NULL)
		return VXGE_HW_FAIL;

	if (vxge_rx_map(dtrh, ring)) {
		rx_priv = vxge_hw_ring_rxd_private_get(dtrh);
		dev_kfree_skb(rx_priv->skb);

		return VXGE_HW_FAIL;
	}
	vxge_debug_entryexit(VXGE_TRACE,
		"%s: %s:%d Exiting...", ring->ndev->name, __func__, __LINE__);

	return VXGE_HW_OK;
}

static inline void
vxge_rx_complete(struct vxge_ring *ring, struct sk_buff *skb, u16 vlan,
		 int pkt_length, struct vxge_hw_ring_rxd_info *ext_info)
{

	vxge_debug_entryexit(VXGE_TRACE, "%s: %s:%d",
			ring->ndev->name, __func__, __LINE__);
	skb_record_rx_queue(skb, ring->driver_id);
	skb->protocol = eth_type_trans(skb, ring->ndev);

	ring->stats.rx_frms++;
	ring->stats.rx_bytes += pkt_length;

	if (skb->pkt_type == PACKET_MULTICAST)
		ring->stats.rx_mcast++;

	vxge_debug_rx(VXGE_TRACE,
		"%s: %s:%d  skb protocol = %d",
		ring->ndev->name, __func__, __LINE__, skb->protocol);

	if (ring->gro_enable) {
		if (ring->vlgrp && ext_info->vlan &&
			(ring->vlan_tag_strip ==
				VXGE_HW_VPATH_RPA_STRIP_VLAN_TAG_ENABLE))
			vlan_gro_receive(ring->napi_p, ring->vlgrp,
					ext_info->vlan, skb);
		else
			napi_gro_receive(ring->napi_p, skb);
	} else {
		if (ring->vlgrp && vlan &&
			(ring->vlan_tag_strip ==
				VXGE_HW_VPATH_RPA_STRIP_VLAN_TAG_ENABLE))
			vlan_hwaccel_receive_skb(skb, ring->vlgrp, vlan);
		else
			netif_receive_skb(skb);
	}
	vxge_debug_entryexit(VXGE_TRACE,
		"%s: %s:%d Exiting...", ring->ndev->name, __func__, __LINE__);
}

static inline void vxge_re_pre_post(void *dtr, struct vxge_ring *ring,
				    struct vxge_rx_priv *rx_priv)
{
	pci_dma_sync_single_for_device(ring->pdev,
		rx_priv->data_dma, rx_priv->data_size, PCI_DMA_FROMDEVICE);

	vxge_hw_ring_rxd_1b_set(dtr, rx_priv->data_dma, rx_priv->data_size);
	vxge_hw_ring_rxd_pre_post(ring->handle, dtr);
}

static inline void vxge_post(int *dtr_cnt, void **first_dtr,
			     void *post_dtr, struct __vxge_hw_ring *ringh)
{
	int dtr_count = *dtr_cnt;
	if ((*dtr_cnt % VXGE_HW_RXSYNC_FREQ_CNT) == 0) {
		if (*first_dtr)
			vxge_hw_ring_rxd_post_post_wmb(ringh, *first_dtr);
		*first_dtr = post_dtr;
	} else
		vxge_hw_ring_rxd_post_post(ringh, post_dtr);
	dtr_count++;
	*dtr_cnt = dtr_count;
}

/*
 * vxge_rx_1b_compl
 *
 * If the interrupt is because of a received frame or if the receive ring
 * contains fresh as yet un-processed frames, this function is called.
 */
enum vxge_hw_status
vxge_rx_1b_compl(struct __vxge_hw_ring *ringh, void *dtr,
		 u8 t_code, void *userdata)
{
	struct vxge_ring *ring = (struct vxge_ring *)userdata;
	struct  net_device *dev = ring->ndev;
	unsigned int dma_sizes;
	void *first_dtr = NULL;
	int dtr_cnt = 0;
	int data_size;
	dma_addr_t data_dma;
	int pkt_length;
	struct sk_buff *skb;
	struct vxge_rx_priv *rx_priv;
	struct vxge_hw_ring_rxd_info ext_info;
	vxge_debug_entryexit(VXGE_TRACE, "%s: %s:%d",
		ring->ndev->name, __func__, __LINE__);
	ring->pkts_processed = 0;

	vxge_hw_ring_replenish(ringh, 0);

	do {
		prefetch((char *)dtr + L1_CACHE_BYTES);
		rx_priv = vxge_hw_ring_rxd_private_get(dtr);
		skb = rx_priv->skb;
		data_size = rx_priv->data_size;
		data_dma = rx_priv->data_dma;
		prefetch(rx_priv->skb_data);

		vxge_debug_rx(VXGE_TRACE,
			"%s: %s:%d  skb = 0x%p",
			ring->ndev->name, __func__, __LINE__, skb);

		vxge_hw_ring_rxd_1b_get(ringh, dtr, &dma_sizes);
		pkt_length = dma_sizes;

		pkt_length -= ETH_FCS_LEN;

		vxge_debug_rx(VXGE_TRACE,
			"%s: %s:%d  Packet Length = %d",
			ring->ndev->name, __func__, __LINE__, pkt_length);

		vxge_hw_ring_rxd_1b_info_get(ringh, dtr, &ext_info);

		/* check skb validity */
		vxge_assert(skb);

		prefetch((char *)skb + L1_CACHE_BYTES);
		if (unlikely(t_code)) {

			if (vxge_hw_ring_handle_tcode(ringh, dtr, t_code) !=
				VXGE_HW_OK) {

				ring->stats.rx_errors++;
				vxge_debug_rx(VXGE_TRACE,
					"%s: %s :%d Rx T_code is %d",
					ring->ndev->name, __func__,
					__LINE__, t_code);

				/* If the t_code is not supported and if the
				 * t_code is other than 0x5 (unparseable packet
				 * such as unknown UPV6 header), Drop it !!!
				 */
				vxge_re_pre_post(dtr, ring, rx_priv);

				vxge_post(&dtr_cnt, &first_dtr, dtr, ringh);
				ring->stats.rx_dropped++;
				continue;
			}
		}

		if (pkt_length > VXGE_LL_RX_COPY_THRESHOLD) {

			if (vxge_rx_alloc(dtr, ring, data_size) != NULL) {

				if (!vxge_rx_map(dtr, ring)) {
					skb_put(skb, pkt_length);

					pci_unmap_single(ring->pdev, data_dma,
						data_size, PCI_DMA_FROMDEVICE);

					vxge_hw_ring_rxd_pre_post(ringh, dtr);
					vxge_post(&dtr_cnt, &first_dtr, dtr,
						ringh);
				} else {
					dev_kfree_skb(rx_priv->skb);
					rx_priv->skb = skb;
					rx_priv->data_size = data_size;
					vxge_re_pre_post(dtr, ring, rx_priv);

					vxge_post(&dtr_cnt, &first_dtr, dtr,
						ringh);
					ring->stats.rx_dropped++;
					break;
				}
			} else {
				vxge_re_pre_post(dtr, ring, rx_priv);

				vxge_post(&dtr_cnt, &first_dtr, dtr, ringh);
				ring->stats.rx_dropped++;
				break;
			}
		} else {
			struct sk_buff *skb_up;

			skb_up = netdev_alloc_skb(dev, pkt_length +
				VXGE_HW_HEADER_ETHERNET_II_802_3_ALIGN);
			if (skb_up != NULL) {
				skb_reserve(skb_up,
				    VXGE_HW_HEADER_ETHERNET_II_802_3_ALIGN);

				pci_dma_sync_single_for_cpu(ring->pdev,
					data_dma, data_size,
					PCI_DMA_FROMDEVICE);

				vxge_debug_mem(VXGE_TRACE,
					"%s: %s:%d  skb_up = %p",
					ring->ndev->name, __func__,
					__LINE__, skb);
				memcpy(skb_up->data, skb->data, pkt_length);

				vxge_re_pre_post(dtr, ring, rx_priv);

				vxge_post(&dtr_cnt, &first_dtr, dtr,
					ringh);
				/* will netif_rx small SKB instead */
				skb = skb_up;
				skb_put(skb, pkt_length);
			} else {
				vxge_re_pre_post(dtr, ring, rx_priv);

				vxge_post(&dtr_cnt, &first_dtr, dtr, ringh);
				vxge_debug_rx(VXGE_ERR,
					"%s: vxge_rx_1b_compl: out of "
					"memory", dev->name);
				ring->stats.skb_alloc_fail++;
				break;
			}
		}

		if ((ext_info.proto & VXGE_HW_FRAME_PROTO_TCP_OR_UDP) &&
		    !(ext_info.proto & VXGE_HW_FRAME_PROTO_IP_FRAG) &&
		    ring->rx_csum && /* Offload Rx side CSUM */
		    ext_info.l3_cksum == VXGE_HW_L3_CKSUM_OK &&
		    ext_info.l4_cksum == VXGE_HW_L4_CKSUM_OK)
			skb->ip_summed = CHECKSUM_UNNECESSARY;
		else
			skb->ip_summed = CHECKSUM_NONE;

		vxge_rx_complete(ring, skb, ext_info.vlan,
			pkt_length, &ext_info);

		ring->budget--;
		ring->pkts_processed++;
		if (!ring->budget)
			break;

	} while (vxge_hw_ring_rxd_next_completed(ringh, &dtr,
		&t_code) == VXGE_HW_OK);

	if (first_dtr)
		vxge_hw_ring_rxd_post_post_wmb(ringh, first_dtr);

	vxge_debug_entryexit(VXGE_TRACE,
				"%s:%d  Exiting...",
				__func__, __LINE__);
	return VXGE_HW_OK;
}

/*
 * vxge_xmit_compl
 *
 * If an interrupt was raised to indicate DMA complete of the Tx packet,
 * this function is called. It identifies the last TxD whose buffer was
 * freed and frees all skbs whose data have already DMA'ed into the NICs
 * internal memory.
 */
enum vxge_hw_status
vxge_xmit_compl(struct __vxge_hw_fifo *fifo_hw, void *dtr,
		enum vxge_hw_fifo_tcode t_code, void *userdata,
		struct sk_buff ***skb_ptr, int nr_skb, int *more)
{
	struct vxge_fifo *fifo = (struct vxge_fifo *)userdata;
	struct sk_buff *skb, **done_skb = *skb_ptr;
	int pkt_cnt = 0;

	vxge_debug_entryexit(VXGE_TRACE,
		"%s:%d Entered....", __func__, __LINE__);

	do {
		int frg_cnt;
		skb_frag_t *frag;
		int i = 0, j;
		struct vxge_tx_priv *txd_priv =
			vxge_hw_fifo_txdl_private_get(dtr);

		skb = txd_priv->skb;
		frg_cnt = skb_shinfo(skb)->nr_frags;
		frag = &skb_shinfo(skb)->frags[0];

		vxge_debug_tx(VXGE_TRACE,
				"%s: %s:%d fifo_hw = %p dtr = %p "
				"tcode = 0x%x", fifo->ndev->name, __func__,
				__LINE__, fifo_hw, dtr, t_code);
		/* check skb validity */
		vxge_assert(skb);
		vxge_debug_tx(VXGE_TRACE,
			"%s: %s:%d skb = %p itxd_priv = %p frg_cnt = %d",
			fifo->ndev->name, __func__, __LINE__,
			skb, txd_priv, frg_cnt);
		if (unlikely(t_code)) {
			fifo->stats.tx_errors++;
			vxge_debug_tx(VXGE_ERR,
				"%s: tx: dtr %p completed due to "
				"error t_code %01x", fifo->ndev->name,
				dtr, t_code);
			vxge_hw_fifo_handle_tcode(fifo_hw, dtr, t_code);
		}

		/*  for unfragmented skb */
		pci_unmap_single(fifo->pdev, txd_priv->dma_buffers[i++],
				skb_headlen(skb), PCI_DMA_TODEVICE);

		for (j = 0; j < frg_cnt; j++) {
			pci_unmap_page(fifo->pdev,
					txd_priv->dma_buffers[i++],
					frag->size, PCI_DMA_TODEVICE);
			frag += 1;
		}

		vxge_hw_fifo_txdl_free(fifo_hw, dtr);

		/* Updating the statistics block */
		fifo->stats.tx_frms++;
		fifo->stats.tx_bytes += skb->len;

		*done_skb++ = skb;

		if (--nr_skb <= 0) {
			*more = 1;
			break;
		}

		pkt_cnt++;
		if (pkt_cnt > fifo->indicate_max_pkts)
			break;

	} while (vxge_hw_fifo_txdl_next_completed(fifo_hw,
				&dtr, &t_code) == VXGE_HW_OK);

	*skb_ptr = done_skb;
	vxge_wake_tx_queue(fifo, skb);

	vxge_debug_entryexit(VXGE_TRACE,
				"%s: %s:%d  Exiting...",
				fifo->ndev->name, __func__, __LINE__);
	return VXGE_HW_OK;
}

/* select a vpath to transmit the packet */
static u32 vxge_get_vpath_no(struct vxgedev *vdev, struct sk_buff *skb,
	int *do_lock)
{
	u16 queue_len, counter = 0;
	if (skb->protocol == htons(ETH_P_IP)) {
		struct iphdr *ip;
		struct tcphdr *th;

		ip = ip_hdr(skb);

		if ((ip->frag_off & htons(IP_OFFSET|IP_MF)) == 0) {
			th = (struct tcphdr *)(((unsigned char *)ip) +
					ip->ihl*4);

			queue_len = vdev->no_of_vpath;
			counter = (ntohs(th->source) +
				ntohs(th->dest)) &
				vdev->vpath_selector[queue_len - 1];
			if (counter >= queue_len)
				counter = queue_len - 1;

			if (ip->protocol == IPPROTO_UDP) {
#ifdef NETIF_F_LLTX
				*do_lock = 0;
#endif
			}
		}
	}
	return counter;
}

static enum vxge_hw_status vxge_search_mac_addr_in_list(
	struct vxge_vpath *vpath, u64 del_mac)
{
	struct list_head *entry, *next;
	list_for_each_safe(entry, next, &vpath->mac_addr_list) {
		if (((struct vxge_mac_addrs *)entry)->macaddr == del_mac)
			return TRUE;
	}
	return FALSE;
}

static int vxge_learn_mac(struct vxgedev *vdev, u8 *mac_header)
{
	struct macInfo mac_info;
	u8 *mac_address = NULL;
	u64 mac_addr = 0, vpath_vector = 0;
	int vpath_idx = 0;
	enum vxge_hw_status status = VXGE_HW_OK;
	struct vxge_vpath *vpath = NULL;
	struct __vxge_hw_device *hldev;

	hldev = (struct __vxge_hw_device *) pci_get_drvdata(vdev->pdev);

	mac_address = (u8 *)&mac_addr;
	memcpy(mac_address, mac_header, ETH_ALEN);

	/* Is this mac address already in the list? */
	for (vpath_idx = 0; vpath_idx < vdev->no_of_vpath; vpath_idx++) {
		vpath = &vdev->vpaths[vpath_idx];
		if (vxge_search_mac_addr_in_list(vpath, mac_addr))
			return vpath_idx;
	}

	memset(&mac_info, 0, sizeof(struct macInfo));
	memcpy(mac_info.macaddr, mac_header, ETH_ALEN);

	/* Any vpath has room to add mac address to its da table? */
	for (vpath_idx = 0; vpath_idx < vdev->no_of_vpath; vpath_idx++) {
		vpath = &vdev->vpaths[vpath_idx];
		if (vpath->mac_addr_cnt < vpath->max_mac_addr_cnt) {
			/* Add this mac address to this vpath */
			mac_info.vpath_no = vpath_idx;
			mac_info.state = VXGE_LL_MAC_ADDR_IN_DA_TABLE;
			status = vxge_add_mac_addr(vdev, &mac_info);
			if (status != VXGE_HW_OK)
				return -EPERM;
			return vpath_idx;
		}
	}

	mac_info.state = VXGE_LL_MAC_ADDR_IN_LIST;
	vpath_idx = 0;
	mac_info.vpath_no = vpath_idx;
	/* Is the first vpath already selected as catch-basin ? */
	vpath = &vdev->vpaths[vpath_idx];
	if (vpath->mac_addr_cnt > vpath->max_mac_addr_cnt) {
		/* Add this mac address to this vpath */
		if (FALSE == vxge_mac_list_add(vpath, &mac_info))
			return -EPERM;
		return vpath_idx;
	}

	/* Select first vpath as catch-basin */
	vpath_vector = vxge_mBIT(vpath->device_id);
	status = vxge_hw_mgmt_reg_write(vpath->vdev->devh,
				vxge_hw_mgmt_reg_type_mrpcim,
				0,
				(ulong)offsetof(
					struct vxge_hw_mrpcim_reg,
					rts_mgr_cbasin_cfg),
				vpath_vector);
	if (status != VXGE_HW_OK) {
		vxge_debug_tx(VXGE_ERR,
			"%s: Unable to set the vpath-%d in catch-basin mode",
			VXGE_DRIVER_NAME, vpath->device_id);
		return -EPERM;
	}

	if (FALSE == vxge_mac_list_add(vpath, &mac_info))
		return -EPERM;

	return vpath_idx;
}

/**
 * vxge_xmit
 * @skb : the socket buffer containing the Tx data.
 * @dev : device pointer.
 *
 * This function is the Tx entry point of the driver. Neterion NIC supports
 * certain protocol assist features on Tx side, namely  CSO, S/G, LSO.
 * NOTE: when device cant queue the pkt, just the trans_start variable will
 * not be upadted.
*/
static netdev_tx_t
vxge_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct vxge_fifo *fifo = NULL;
	void *dtr_priv;
	void *dtr = NULL;
	struct vxgedev *vdev = NULL;
	enum vxge_hw_status status;
	int frg_cnt, first_frg_len;
	skb_frag_t *frag;
	int i = 0, j = 0, avail;
	u64 dma_pointer;
	struct vxge_tx_priv *txdl_priv = NULL;
	struct __vxge_hw_fifo *fifo_hw;
	int offload_type;
	unsigned long flags = 0;
	int vpath_no = 0;
	int do_spin_tx_lock = 1;

	vxge_debug_entryexit(VXGE_TRACE, "%s: %s:%d",
			dev->name, __func__, __LINE__);

	/* A buffer with no data will be dropped */
	if (unlikely(skb->len <= 0)) {
		vxge_debug_tx(VXGE_ERR,
			"%s: Buffer has no data..", dev->name);
		dev_kfree_skb(skb);
		return NETDEV_TX_OK;
	}

	vdev = (struct vxgedev *)netdev_priv(dev);

	if (unlikely(!is_vxge_card_up(vdev))) {
		vxge_debug_tx(VXGE_ERR,
			"%s: vdev not initialized", dev->name);
		dev_kfree_skb(skb);
		return NETDEV_TX_OK;
	}

	if (vdev->config.addr_learn_en) {
		vpath_no = vxge_learn_mac(vdev, skb->data + ETH_ALEN);
		if (vpath_no == -EPERM) {
			vxge_debug_tx(VXGE_ERR,
				"%s: Failed to store the mac address",
				dev->name);
			dev_kfree_skb(skb);
			return NETDEV_TX_OK;
		}
	}

	if (vdev->config.tx_steering_type == TX_MULTIQ_STEERING)
		vpath_no = skb_get_queue_mapping(skb);
	else if (vdev->config.tx_steering_type == TX_PORT_STEERING)
		vpath_no = vxge_get_vpath_no(vdev, skb, &do_spin_tx_lock);

	vxge_debug_tx(VXGE_TRACE, "%s: vpath_no= %d", dev->name, vpath_no);

	if (vpath_no >= vdev->no_of_vpath)
		vpath_no = 0;

	fifo = &vdev->vpaths[vpath_no].fifo;
	fifo_hw = fifo->handle;

	if (do_spin_tx_lock)
		spin_lock_irqsave(&fifo->tx_lock, flags);
	else {
		if (unlikely(!spin_trylock_irqsave(&fifo->tx_lock, flags)))
			return NETDEV_TX_LOCKED;
	}

	if (vdev->config.tx_steering_type == TX_MULTIQ_STEERING) {
		if (netif_subqueue_stopped(dev, skb)) {
			spin_unlock_irqrestore(&fifo->tx_lock, flags);
			return NETDEV_TX_BUSY;
		}
	} else if (unlikely(fifo->queue_state == VPATH_QUEUE_STOP)) {
		if (netif_queue_stopped(dev)) {
			spin_unlock_irqrestore(&fifo->tx_lock, flags);
			return NETDEV_TX_BUSY;
		}
	}
	avail = vxge_hw_fifo_free_txdl_count_get(fifo_hw);
	if (avail == 0) {
		vxge_debug_tx(VXGE_ERR,
			"%s: No free TXDs available", dev->name);
		fifo->stats.txd_not_free++;
		vxge_stop_tx_queue(fifo);
		goto _exit2;
	}

	/* Last TXD?  Stop tx queue to avoid dropping packets.  TX
	 * completion will resume the queue.
	 */
	if (avail == 1)
		vxge_stop_tx_queue(fifo);

	status = vxge_hw_fifo_txdl_reserve(fifo_hw, &dtr, &dtr_priv);
	if (unlikely(status != VXGE_HW_OK)) {
		vxge_debug_tx(VXGE_ERR,
		   "%s: Out of descriptors .", dev->name);
		fifo->stats.txd_out_of_desc++;
		vxge_stop_tx_queue(fifo);
		goto _exit2;
	}

	vxge_debug_tx(VXGE_TRACE,
		"%s: %s:%d fifo_hw = %p dtr = %p dtr_priv = %p",
		dev->name, __func__, __LINE__,
		fifo_hw, dtr, dtr_priv);

	if (vdev->vlgrp && vlan_tx_tag_present(skb)) {
		u16 vlan_tag = vlan_tx_tag_get(skb);
		vxge_hw_fifo_txdl_vlan_set(dtr, vlan_tag);
	}

	first_frg_len = skb_headlen(skb);

	dma_pointer = pci_map_single(fifo->pdev, skb->data, first_frg_len,
				PCI_DMA_TODEVICE);

	if (unlikely(pci_dma_mapping_error(fifo->pdev, dma_pointer))) {
		vxge_hw_fifo_txdl_free(fifo_hw, dtr);
		vxge_stop_tx_queue(fifo);
		fifo->stats.pci_map_fail++;
		goto _exit2;
	}

	txdl_priv = vxge_hw_fifo_txdl_private_get(dtr);
	txdl_priv->skb = skb;
	txdl_priv->dma_buffers[j] = dma_pointer;

	frg_cnt = skb_shinfo(skb)->nr_frags;
	vxge_debug_tx(VXGE_TRACE,
			"%s: %s:%d skb = %p txdl_priv = %p "
			"frag_cnt = %d dma_pointer = 0x%llx", dev->name,
			__func__, __LINE__, skb, txdl_priv,
			frg_cnt, (unsigned long long)dma_pointer);

	vxge_hw_fifo_txdl_buffer_set(fifo_hw, dtr, j++, dma_pointer,
		first_frg_len);

	frag = &skb_shinfo(skb)->frags[0];
	for (i = 0; i < frg_cnt; i++) {
		/* ignore 0 length fragment */
		if (!frag->size)
			continue;

		dma_pointer =
			(u64)pci_map_page(fifo->pdev, frag->page,
				frag->page_offset, frag->size,
				PCI_DMA_TODEVICE);

		if (unlikely(pci_dma_mapping_error(fifo->pdev, dma_pointer)))
			goto _exit0;
		vxge_debug_tx(VXGE_TRACE,
			"%s: %s:%d frag = %d dma_pointer = 0x%llx",
				dev->name, __func__, __LINE__, i,
				(unsigned long long)dma_pointer);

		txdl_priv->dma_buffers[j] = dma_pointer;
		vxge_hw_fifo_txdl_buffer_set(fifo_hw, dtr, j++, dma_pointer,
					frag->size);
		frag += 1;
	}

	offload_type = vxge_offload_type(skb);

	if (offload_type & (SKB_GSO_TCPV4 | SKB_GSO_TCPV6)) {

		int mss = vxge_tcp_mss(skb);
		if (mss) {
			vxge_debug_tx(VXGE_TRACE,
				"%s: %s:%d mss = %d",
				dev->name, __func__, __LINE__, mss);
			vxge_hw_fifo_txdl_mss_set(dtr, mss);
		} else {
			vxge_assert(skb->len <=
				dev->mtu + VXGE_HW_MAC_HEADER_MAX_SIZE);
			vxge_assert(0);
			goto _exit1;
		}
	}

	if (skb->ip_summed == CHECKSUM_PARTIAL)
		vxge_hw_fifo_txdl_cksum_set_bits(dtr,
					VXGE_HW_FIFO_TXD_TX_CKO_IPV4_EN |
					VXGE_HW_FIFO_TXD_TX_CKO_TCP_EN |
					VXGE_HW_FIFO_TXD_TX_CKO_UDP_EN);

	vxge_hw_fifo_txdl_post(fifo_hw, dtr);
#ifdef NETIF_F_LLTX
	dev->trans_start = jiffies; /* NETIF_F_LLTX driver :( */
#endif
	spin_unlock_irqrestore(&fifo->tx_lock, flags);

	VXGE_COMPLETE_VPATH_TX(fifo);
	vxge_debug_entryexit(VXGE_TRACE, "%s: %s:%d  Exiting...",
		dev->name, __func__, __LINE__);
	return NETDEV_TX_OK;

_exit0:
	vxge_debug_tx(VXGE_TRACE, "%s: pci_map_page failed", dev->name);

_exit1:
	j = 0;
	frag = &skb_shinfo(skb)->frags[0];

	pci_unmap_single(fifo->pdev, txdl_priv->dma_buffers[j++],
			skb_headlen(skb), PCI_DMA_TODEVICE);

	for (; j < i; j++) {
		pci_unmap_page(fifo->pdev, txdl_priv->dma_buffers[j],
			frag->size, PCI_DMA_TODEVICE);
		frag += 1;
	}

	vxge_hw_fifo_txdl_free(fifo_hw, dtr);
_exit2:
	dev_kfree_skb(skb);
	spin_unlock_irqrestore(&fifo->tx_lock, flags);
	VXGE_COMPLETE_VPATH_TX(fifo);

	return NETDEV_TX_OK;
}

/*
 * vxge_rx_term
 *
 * Function will be called by hw function to abort all outstanding receive
 * descriptors.
 */
static void
vxge_rx_term(void *dtrh, enum vxge_hw_rxd_state state, void *userdata)
{
	struct vxge_ring *ring = (struct vxge_ring *)userdata;
	struct vxge_rx_priv *rx_priv =
		vxge_hw_ring_rxd_private_get(dtrh);

	vxge_debug_entryexit(VXGE_TRACE, "%s: %s:%d",
			ring->ndev->name, __func__, __LINE__);
	if (state != VXGE_HW_RXD_STATE_POSTED)
		return;

	pci_unmap_single(ring->pdev, rx_priv->data_dma,
		rx_priv->data_size, PCI_DMA_FROMDEVICE);

	dev_kfree_skb(rx_priv->skb);
	rx_priv->skb_data = NULL;

	vxge_debug_entryexit(VXGE_TRACE,
		"%s: %s:%d  Exiting...",
		ring->ndev->name, __func__, __LINE__);
}

/*
 * vxge_tx_term
 *
 * Function will be called to abort all outstanding tx descriptors
 */
static void
vxge_tx_term(void *dtrh, enum vxge_hw_txdl_state state, void *userdata)
{
	struct vxge_fifo *fifo = (struct vxge_fifo *)userdata;
	skb_frag_t *frag;
	int i = 0, j, frg_cnt;
	struct vxge_tx_priv *txd_priv = vxge_hw_fifo_txdl_private_get(dtrh);
	struct sk_buff *skb = txd_priv->skb;

	vxge_debug_entryexit(VXGE_TRACE, "%s:%d", __func__, __LINE__);

	if (state != VXGE_HW_TXDL_STATE_POSTED)
		return;

	/* check skb validity */
	vxge_assert(skb);
	frg_cnt = skb_shinfo(skb)->nr_frags;
	frag = &skb_shinfo(skb)->frags[0];

	/*  for unfragmented skb */
	pci_unmap_single(fifo->pdev, txd_priv->dma_buffers[i++],
		skb_headlen(skb), PCI_DMA_TODEVICE);

	for (j = 0; j < frg_cnt; j++) {
		pci_unmap_page(fifo->pdev, txd_priv->dma_buffers[i++],
			       frag->size, PCI_DMA_TODEVICE);
		frag += 1;
	}

	dev_kfree_skb(skb);

	vxge_debug_entryexit(VXGE_TRACE,
		"%s:%d  Exiting...", __func__, __LINE__);
}

/**
 * vxge_set_multicast
 * @dev: pointer to the device structure
 *
 * Entry point for multicast address enable/disable
 * This function is a driver entry point which gets called by the kernel
 * whenever multicast addresses must be enabled/disabled. This also gets
 * called to set/reset promiscuous mode. Depending on the deivce flag, we
 * determine, if multicast address must be enabled or if promiscuous mode
 * is to be disabled etc.
 */
static void vxge_set_multicast(struct net_device *dev)
{
	struct dev_mc_list *mclist;
	struct vxgedev *vdev;
	int i, mcast_cnt = 0;
	struct __vxge_hw_device  *hldev;
	enum vxge_hw_status status = VXGE_HW_OK;
	struct macInfo mac_info;
	int vpath_idx = 0;
	struct vxge_mac_addrs *mac_entry;
	struct list_head *list_head;
	struct list_head *entry, *next;
	u8 *mac_address = NULL;

	vxge_debug_entryexit(VXGE_TRACE,
		"%s:%d", __func__, __LINE__);

	vdev = (struct vxgedev *)netdev_priv(dev);
	hldev = (struct __vxge_hw_device  *)vdev->devh;

	if (unlikely(!is_vxge_card_up(vdev)))
		return;

	if ((dev->flags & IFF_ALLMULTI) && (!vdev->all_multi_flg)) {
		for (i = 0; i < vdev->no_of_vpath; i++) {
			vxge_assert(vdev->vpaths[i].is_open);
			status = vxge_hw_vpath_mcast_enable(
						vdev->vpaths[i].handle);
			vdev->all_multi_flg = 1;
		}
	} else if ((dev->flags & IFF_ALLMULTI) && (vdev->all_multi_flg)) {
		for (i = 0; i < vdev->no_of_vpath; i++) {
			vxge_assert(vdev->vpaths[i].is_open);
			status = vxge_hw_vpath_mcast_disable(
						vdev->vpaths[i].handle);
			vdev->all_multi_flg = 1;
		}
	}

	if (status != VXGE_HW_OK)
		vxge_debug_init(VXGE_ERR,
			"failed to %s multicast, status %d",
			dev->flags & IFF_ALLMULTI ?
			"enable" : "disable", status);

	if (!vdev->config.addr_learn_en) {
		if (dev->flags & IFF_PROMISC) {
			for (i = 0; i < vdev->no_of_vpath; i++) {
				vxge_assert(vdev->vpaths[i].is_open);
				status = vxge_hw_vpath_promisc_enable(
						vdev->vpaths[i].handle);
			}
		} else {
			for (i = 0; i < vdev->no_of_vpath; i++) {
				vxge_assert(vdev->vpaths[i].is_open);
				status = vxge_hw_vpath_promisc_disable(
						vdev->vpaths[i].handle);
			}
		}
	}

	memset(&mac_info, 0, sizeof(struct macInfo));
	/* Update individual M_CAST address list */
	if ((!vdev->all_multi_flg) && dev->mc_count) {

		mcast_cnt = vdev->vpaths[0].mcast_addr_cnt;
		list_head = &vdev->vpaths[0].mac_addr_list;
		if ((dev->mc_count +
			(vdev->vpaths[0].mac_addr_cnt - mcast_cnt)) >
				vdev->vpaths[0].max_mac_addr_cnt)
			goto _set_all_mcast;

		/* Delete previous MC's */
		for (i = 0; i < mcast_cnt; i++) {
			if (!list_empty(list_head))
				mac_entry = (struct vxge_mac_addrs *)
					list_first_entry(list_head,
						struct vxge_mac_addrs,
						item);

			list_for_each_safe(entry, next, list_head) {

				mac_entry = (struct vxge_mac_addrs *) entry;
				/* Copy the mac address to delete */
				mac_address = (u8 *)&mac_entry->macaddr;
				memcpy(mac_info.macaddr, mac_address, ETH_ALEN);

				/* Is this a multicast address */
				if (0x01 & mac_info.macaddr[0]) {
					for (vpath_idx = 0; vpath_idx <
						vdev->no_of_vpath;
						vpath_idx++) {
						mac_info.vpath_no = vpath_idx;
						status = vxge_del_mac_addr(
								vdev,
								&mac_info);
					}
				}
			}
		}

		/* Add new ones */
		for (i = 0, mclist = dev->mc_list; i < dev->mc_count;
			i++, mclist = mclist->next) {

			memcpy(mac_info.macaddr, mclist->dmi_addr, ETH_ALEN);
			for (vpath_idx = 0; vpath_idx < vdev->no_of_vpath;
					vpath_idx++) {
				mac_info.vpath_no = vpath_idx;
				mac_info.state = VXGE_LL_MAC_ADDR_IN_DA_TABLE;
				status = vxge_add_mac_addr(vdev, &mac_info);
				if (status != VXGE_HW_OK) {
					vxge_debug_init(VXGE_ERR,
						"%s:%d Setting individual"
						"multicast address failed",
						__func__, __LINE__);
					goto _set_all_mcast;
				}
			}
		}

		return;
_set_all_mcast:
		mcast_cnt = vdev->vpaths[0].mcast_addr_cnt;
		/* Delete previous MC's */
		for (i = 0; i < mcast_cnt; i++) {

			list_for_each_safe(entry, next, list_head) {

				mac_entry = (struct vxge_mac_addrs *) entry;
				/* Copy the mac address to delete */
				mac_address = (u8 *)&mac_entry->macaddr;
				memcpy(mac_info.macaddr, mac_address, ETH_ALEN);

				/* Is this a multicast address */
				if (0x01 & mac_info.macaddr[0])
					break;
			}

			for (vpath_idx = 0; vpath_idx < vdev->no_of_vpath;
					vpath_idx++) {
				mac_info.vpath_no = vpath_idx;
				status = vxge_del_mac_addr(vdev, &mac_info);
			}
		}

		/* Enable all multicast */
		for (i = 0; i < vdev->no_of_vpath; i++) {
			vxge_assert(vdev->vpaths[i].is_open);
			status = vxge_hw_vpath_mcast_enable(
						vdev->vpaths[i].handle);
			if (status != VXGE_HW_OK) {
				vxge_debug_init(VXGE_ERR,
					"%s:%d Enabling all multicasts failed",
					 __func__, __LINE__);
			}
			vdev->all_multi_flg = 1;
		}
		dev->flags |= IFF_ALLMULTI;
	}

	vxge_debug_entryexit(VXGE_TRACE,
		"%s:%d  Exiting...", __func__, __LINE__);
}

/**
 * vxge_set_mac_addr
 * @dev: pointer to the device structure
 *
 * Update entry "0" (default MAC addr)
 */
static int vxge_set_mac_addr(struct net_device *dev, void *p)
{
	struct sockaddr *addr = p;
	struct vxgedev *vdev;
	struct __vxge_hw_device  *hldev;
	enum vxge_hw_status status = VXGE_HW_OK;
	struct macInfo mac_info_new, mac_info_old;
	int vpath_idx = 0;

	vxge_debug_entryexit(VXGE_TRACE, "%s:%d", __func__, __LINE__);

	vdev = (struct vxgedev *)netdev_priv(dev);
	hldev = vdev->devh;

	if (!is_valid_ether_addr(addr->sa_data))
		return -EINVAL;

	memset(&mac_info_new, 0, sizeof(struct macInfo));
	memset(&mac_info_old, 0, sizeof(struct macInfo));

	vxge_debug_entryexit(VXGE_TRACE, "%s:%d  Exiting...",
		__func__, __LINE__);

	/* Get the old address */
	memcpy(mac_info_old.macaddr, dev->dev_addr, dev->addr_len);

	/* Copy the new address */
	memcpy(mac_info_new.macaddr, addr->sa_data, dev->addr_len);

	/* First delete the old mac address from all the vpaths
	as we can't specify the index while adding new mac address */
	for (vpath_idx = 0; vpath_idx < vdev->no_of_vpath; vpath_idx++) {
		struct vxge_vpath *vpath = &vdev->vpaths[vpath_idx];
		if (!vpath->is_open) {
			/* This can happen when this interface is added/removed
			to the bonding interface. Delete this station address
			from the linked list */
			vxge_mac_list_del(vpath, &mac_info_old);

			/* Add this new address to the linked list
			for later restoring */
			vxge_mac_list_add(vpath, &mac_info_new);

			continue;
		}
		/* Delete the station address */
		mac_info_old.vpath_no = vpath_idx;
		status = vxge_del_mac_addr(vdev, &mac_info_old);
	}

	if (unlikely(!is_vxge_card_up(vdev))) {
		memcpy(dev->dev_addr, addr->sa_data, dev->addr_len);
		return VXGE_HW_OK;
	}

	/* Set this mac address to all the vpaths */
	for (vpath_idx = 0; vpath_idx < vdev->no_of_vpath; vpath_idx++) {
		mac_info_new.vpath_no = vpath_idx;
		mac_info_new.state = VXGE_LL_MAC_ADDR_IN_DA_TABLE;
		status = vxge_add_mac_addr(vdev, &mac_info_new);
		if (status != VXGE_HW_OK)
			return -EINVAL;
	}

	memcpy(dev->dev_addr, addr->sa_data, dev->addr_len);

	return status;
}

/*
 * vxge_vpath_intr_enable
 * @vdev: pointer to vdev
 * @vp_id: vpath for which to enable the interrupts
 *
 * Enables the interrupts for the vpath
*/
void vxge_vpath_intr_enable(struct vxgedev *vdev, int vp_id)
{
	struct vxge_vpath *vpath = &vdev->vpaths[vp_id];
	int msix_id, alarm_msix_id;
	int tim_msix_id[4] = {[0 ...3] = 0};

	vxge_hw_vpath_intr_enable(vpath->handle);

	if (vdev->config.intr_type == INTA)
		vxge_hw_vpath_inta_unmask_tx_rx(vpath->handle);
	else {
		msix_id = vp_id * VXGE_HW_VPATH_MSIX_ACTIVE;
		alarm_msix_id =
			VXGE_HW_VPATH_MSIX_ACTIVE * vdev->no_of_vpath - 2;

		tim_msix_id[0] = msix_id;
		tim_msix_id[1] = msix_id + 1;
		vxge_hw_vpath_msix_set(vpath->handle, tim_msix_id,
			alarm_msix_id);

		vxge_hw_vpath_msix_unmask(vpath->handle, msix_id);
		vxge_hw_vpath_msix_unmask(vpath->handle, msix_id + 1);

		/* enable the alarm vector */
		vxge_hw_vpath_msix_unmask(vpath->handle, alarm_msix_id);
	}
}

/*
 * vxge_vpath_intr_disable
 * @vdev: pointer to vdev
 * @vp_id: vpath for which to disable the interrupts
 *
 * Disables the interrupts for the vpath
*/
void vxge_vpath_intr_disable(struct vxgedev *vdev, int vp_id)
{
	struct vxge_vpath *vpath = &vdev->vpaths[vp_id];
	int msix_id;

	vxge_hw_vpath_intr_disable(vpath->handle);

	if (vdev->config.intr_type == INTA)
		vxge_hw_vpath_inta_mask_tx_rx(vpath->handle);
	else {
		msix_id = vp_id * VXGE_HW_VPATH_MSIX_ACTIVE;
		vxge_hw_vpath_msix_mask(vpath->handle, msix_id);
		vxge_hw_vpath_msix_mask(vpath->handle, msix_id + 1);

		/* disable the alarm vector */
		msix_id = VXGE_HW_VPATH_MSIX_ACTIVE * vdev->no_of_vpath - 2;
		vxge_hw_vpath_msix_mask(vpath->handle, msix_id);
	}
}

/*
 * vxge_reset_vpath
 * @vdev: pointer to vdev
 * @vp_id: vpath to reset
 *
 * Resets the vpath
*/
static int vxge_reset_vpath(struct vxgedev *vdev, int vp_id)
{
	enum vxge_hw_status status = VXGE_HW_OK;
	int ret = 0;

	/* check if device is down already */
	if (unlikely(!is_vxge_card_up(vdev)))
		return 0;

	/* is device reset already scheduled */
	if (test_bit(__VXGE_STATE_RESET_CARD, &vdev->state))
		return 0;

	if (vdev->vpaths[vp_id].handle) {
		if (vxge_hw_vpath_reset(vdev->vpaths[vp_id].handle)
				== VXGE_HW_OK) {
			if (is_vxge_card_up(vdev) &&
				vxge_hw_vpath_recover_from_reset(
					vdev->vpaths[vp_id].handle)
					!= VXGE_HW_OK) {
				vxge_debug_init(VXGE_ERR,
					"vxge_hw_vpath_recover_from_reset"
					"failed for vpath:%d", vp_id);
				return status;
			}
		} else {
			vxge_debug_init(VXGE_ERR,
				"vxge_hw_vpath_reset failed for"
				"vpath:%d", vp_id);
				return status;
		}
	} else
		return VXGE_HW_FAIL;

	vxge_restore_vpath_mac_addr(&vdev->vpaths[vp_id]);
	vxge_restore_vpath_vid_table(&vdev->vpaths[vp_id]);

	/* Enable all broadcast */
	vxge_hw_vpath_bcast_enable(vdev->vpaths[vp_id].handle);

	/* Enable the interrupts */
	vxge_vpath_intr_enable(vdev, vp_id);

	smp_wmb();

	/* Enable the flow of traffic through the vpath */
	vxge_hw_vpath_enable(vdev->vpaths[vp_id].handle);

	smp_wmb();
	vxge_hw_vpath_rx_doorbell_init(vdev->vpaths[vp_id].handle);
	vdev->vpaths[vp_id].ring.last_status = VXGE_HW_OK;

	/* Vpath reset done */
	clear_bit(vp_id, &vdev->vp_reset);

	/* Start the vpath queue */
	vxge_wake_tx_queue(&vdev->vpaths[vp_id].fifo, NULL);

	return ret;
}

static int do_vxge_reset(struct vxgedev *vdev, int event)
{
	enum vxge_hw_status status;
	int ret = 0, vp_id, i;

	vxge_debug_entryexit(VXGE_TRACE, "%s:%d", __func__, __LINE__);

	if ((event == VXGE_LL_FULL_RESET) || (event == VXGE_LL_START_RESET)) {
		/* check if device is down already */
		if (unlikely(!is_vxge_card_up(vdev)))
			return 0;

		/* is reset already scheduled */
		if (test_and_set_bit(__VXGE_STATE_RESET_CARD, &vdev->state))
			return 0;
	}

	if (event == VXGE_LL_FULL_RESET) {
		/* wait for all the vpath reset to complete */
		for (vp_id = 0; vp_id < vdev->no_of_vpath; vp_id++) {
			while (test_bit(vp_id, &vdev->vp_reset))
				msleep(50);
		}

		/* if execution mode is set to debug, don't reset the adapter */
		if (unlikely(vdev->exec_mode)) {
			vxge_debug_init(VXGE_ERR,
				"%s: execution mode is debug, returning..",
				vdev->ndev->name);
		clear_bit(__VXGE_STATE_CARD_UP, &vdev->state);
		vxge_stop_all_tx_queue(vdev);
		return 0;
		}
	}

	if (event == VXGE_LL_FULL_RESET) {
		vxge_hw_device_intr_disable(vdev->devh);

		switch (vdev->cric_err_event) {
		case VXGE_HW_EVENT_UNKNOWN:
			vxge_stop_all_tx_queue(vdev);
			vxge_debug_init(VXGE_ERR,
				"fatal: %s: Disabling device due to"
				"unknown error",
				vdev->ndev->name);
			ret = -EPERM;
			goto out;
		case VXGE_HW_EVENT_RESET_START:
			break;
		case VXGE_HW_EVENT_RESET_COMPLETE:
		case VXGE_HW_EVENT_LINK_DOWN:
		case VXGE_HW_EVENT_LINK_UP:
		case VXGE_HW_EVENT_ALARM_CLEARED:
		case VXGE_HW_EVENT_ECCERR:
		case VXGE_HW_EVENT_MRPCIM_ECCERR:
			ret = -EPERM;
			goto out;
		case VXGE_HW_EVENT_FIFO_ERR:
		case VXGE_HW_EVENT_VPATH_ERR:
			break;
		case VXGE_HW_EVENT_CRITICAL_ERR:
			vxge_stop_all_tx_queue(vdev);
			vxge_debug_init(VXGE_ERR,
				"fatal: %s: Disabling device due to"
				"serious error",
				vdev->ndev->name);
			/* SOP or device reset required */
			/* This event is not currently used */
			ret = -EPERM;
			goto out;
		case VXGE_HW_EVENT_SERR:
			vxge_stop_all_tx_queue(vdev);
			vxge_debug_init(VXGE_ERR,
				"fatal: %s: Disabling device due to"
				"serious error",
				vdev->ndev->name);
			ret = -EPERM;
			goto out;
		case VXGE_HW_EVENT_SRPCIM_SERR:
		case VXGE_HW_EVENT_MRPCIM_SERR:
			ret = -EPERM;
			goto out;
		case VXGE_HW_EVENT_SLOT_FREEZE:
			vxge_stop_all_tx_queue(vdev);
			vxge_debug_init(VXGE_ERR,
				"fatal: %s: Disabling device due to"
				"slot freeze",
				vdev->ndev->name);
			ret = -EPERM;
			goto out;
		default:
			break;

		}
	}

	if ((event == VXGE_LL_FULL_RESET) || (event == VXGE_LL_START_RESET))
		vxge_stop_all_tx_queue(vdev);

	if (event == VXGE_LL_FULL_RESET) {
		status = vxge_reset_all_vpaths(vdev);
		if (status != VXGE_HW_OK) {
			vxge_debug_init(VXGE_ERR,
				"fatal: %s: can not reset vpaths",
				vdev->ndev->name);
			ret = -EPERM;
			goto out;
		}
	}

	if (event == VXGE_LL_COMPL_RESET) {
		for (i = 0; i < vdev->no_of_vpath; i++)
			if (vdev->vpaths[i].handle) {
				if (vxge_hw_vpath_recover_from_reset(
					vdev->vpaths[i].handle)
						!= VXGE_HW_OK) {
					vxge_debug_init(VXGE_ERR,
						"vxge_hw_vpath_recover_"
						"from_reset failed for vpath: "
						"%d", i);
					ret = -EPERM;
					goto out;
				}
				} else {
					vxge_debug_init(VXGE_ERR,
					"vxge_hw_vpath_reset failed for "
						"vpath:%d", i);
					ret = -EPERM;
					goto out;
				}
	}

	if ((event == VXGE_LL_FULL_RESET) || (event == VXGE_LL_COMPL_RESET)) {
		/* Reprogram the DA table with populated mac addresses */
		for (vp_id = 0; vp_id < vdev->no_of_vpath; vp_id++) {
			vxge_restore_vpath_mac_addr(&vdev->vpaths[vp_id]);
			vxge_restore_vpath_vid_table(&vdev->vpaths[vp_id]);
		}

		/* enable vpath interrupts */
		for (i = 0; i < vdev->no_of_vpath; i++)
			vxge_vpath_intr_enable(vdev, i);

		vxge_hw_device_intr_enable(vdev->devh);

		smp_wmb();

		/* Indicate card up */
		set_bit(__VXGE_STATE_CARD_UP, &vdev->state);

		/* Get the traffic to flow through the vpaths */
		for (i = 0; i < vdev->no_of_vpath; i++) {
			vxge_hw_vpath_enable(vdev->vpaths[i].handle);
			smp_wmb();
			vxge_hw_vpath_rx_doorbell_init(vdev->vpaths[i].handle);
		}

		vxge_wake_all_tx_queue(vdev);
	}

out:
	vxge_debug_entryexit(VXGE_TRACE,
		"%s:%d  Exiting...", __func__, __LINE__);

	/* Indicate reset done */
	if ((event == VXGE_LL_FULL_RESET) || (event == VXGE_LL_COMPL_RESET))
		clear_bit(__VXGE_STATE_RESET_CARD, &vdev->state);
	return ret;
}

/*
 * vxge_reset
 * @vdev: pointer to ll device
 *
 * driver may reset the chip on events of serr, eccerr, etc
 */
int vxge_reset(struct vxgedev *vdev)
{
	do_vxge_reset(vdev, VXGE_LL_FULL_RESET);
	return 0;
}

/**
 * vxge_poll - Receive handler when Receive Polling is used.
 * @dev: pointer to the device structure.
 * @budget: Number of packets budgeted to be processed in this iteration.
 *
 * This function comes into picture only if Receive side is being handled
 * through polling (called NAPI in linux). It mostly does what the normal
 * Rx interrupt handler does in terms of descriptor and packet processing
 * but not in an interrupt context. Also it will process a specified number
 * of packets at most in one iteration. This value is passed down by the
 * kernel as the function argument 'budget'.
 */
static int vxge_poll_msix(struct napi_struct *napi, int budget)
{
	struct vxge_ring *ring =
		container_of(napi, struct vxge_ring, napi);
	int budget_org = budget;
	ring->budget = budget;

	vxge_hw_vpath_poll_rx(ring->handle);

	if (ring->pkts_processed < budget_org) {
		napi_complete(napi);
		/* Re enable the Rx interrupts for the vpath */
		vxge_hw_channel_msix_unmask(
				(struct __vxge_hw_channel *)ring->handle,
				ring->rx_vector_no);
	}

	return ring->pkts_processed;
}

static int vxge_poll_inta(struct napi_struct *napi, int budget)
{
	struct vxgedev *vdev = container_of(napi, struct vxgedev, napi);
	int pkts_processed = 0;
	int i;
	int budget_org = budget;
	struct vxge_ring *ring;

	struct __vxge_hw_device  *hldev = (struct __vxge_hw_device *)
		pci_get_drvdata(vdev->pdev);

	for (i = 0; i < vdev->no_of_vpath; i++) {
		ring = &vdev->vpaths[i].ring;
		ring->budget = budget;
		vxge_hw_vpath_poll_rx(ring->handle);
		pkts_processed += ring->pkts_processed;
		budget -= ring->pkts_processed;
		if (budget <= 0)
			break;
	}

	VXGE_COMPLETE_ALL_TX(vdev);

	if (pkts_processed < budget_org) {
		napi_complete(napi);
		/* Re enable the Rx interrupts for the ring */
		vxge_hw_device_unmask_all(hldev);
		vxge_hw_device_flush_io(hldev);
	}

	return pkts_processed;
}

#ifdef CONFIG_NET_POLL_CONTROLLER
/**
 * vxge_netpoll - netpoll event handler entry point
 * @dev : pointer to the device structure.
 * Description:
 *      This function will be called by upper layer to check for events on the
 * interface in situations where interrupts are disabled. It is used for
 * specific in-kernel networking tasks, such as remote consoles and kernel
 * debugging over the network (example netdump in RedHat).
 */
static void vxge_netpoll(struct net_device *dev)
{
	struct __vxge_hw_device  *hldev;
	struct vxgedev *vdev;

	vdev = (struct vxgedev *)netdev_priv(dev);
	hldev = (struct __vxge_hw_device  *)pci_get_drvdata(vdev->pdev);

	vxge_debug_entryexit(VXGE_TRACE, "%s:%d", __func__, __LINE__);

	if (pci_channel_offline(vdev->pdev))
		return;

	disable_irq(dev->irq);
	vxge_hw_device_clear_tx_rx(hldev);

	vxge_hw_device_clear_tx_rx(hldev);
	VXGE_COMPLETE_ALL_RX(vdev);
	VXGE_COMPLETE_ALL_TX(vdev);

	enable_irq(dev->irq);

	vxge_debug_entryexit(VXGE_TRACE,
		"%s:%d  Exiting...", __func__, __LINE__);
	return;
}
#endif

/* RTH configuration */
static enum vxge_hw_status vxge_rth_configure(struct vxgedev *vdev)
{
	enum vxge_hw_status status = VXGE_HW_OK;
	struct vxge_hw_rth_hash_types hash_types;
	u8 itable[256] = {0}; /* indirection table */
	u8 mtable[256] = {0}; /* CPU to vpath mapping  */
	int index;

	/*
	 * Filling
	 * 	- itable with bucket numbers
	 * 	- mtable with bucket-to-vpath mapping
	 */
	for (index = 0; index < (1 << vdev->config.rth_bkt_sz); index++) {
		itable[index] = index;
		mtable[index] = index % vdev->no_of_vpath;
	}

	/* Fill RTH hash types */
	hash_types.hash_type_tcpipv4_en   = vdev->config.rth_hash_type_tcpipv4;
	hash_types.hash_type_ipv4_en      = vdev->config.rth_hash_type_ipv4;
	hash_types.hash_type_tcpipv6_en   = vdev->config.rth_hash_type_tcpipv6;
	hash_types.hash_type_ipv6_en      = vdev->config.rth_hash_type_ipv6;
	hash_types.hash_type_tcpipv6ex_en =
					vdev->config.rth_hash_type_tcpipv6ex;
	hash_types.hash_type_ipv6ex_en    = vdev->config.rth_hash_type_ipv6ex;

	/* set indirection table, bucket-to-vpath mapping */
	status = vxge_hw_vpath_rts_rth_itable_set(vdev->vp_handles,
						vdev->no_of_vpath,
						mtable, itable,
						vdev->config.rth_bkt_sz);
	if (status != VXGE_HW_OK) {
		vxge_debug_init(VXGE_ERR,
			"RTH indirection table configuration failed "
			"for vpath:%d", vdev->vpaths[0].device_id);
		return status;
	}

	/*
	* Because the itable_set() method uses the active_table field
	* for the target virtual path the RTH config should be updated
	* for all VPATHs. The h/w only uses the lowest numbered VPATH
	* when steering frames.
	*/
	 for (index = 0; index < vdev->no_of_vpath; index++) {
		status = vxge_hw_vpath_rts_rth_set(
				vdev->vpaths[index].handle,
				vdev->config.rth_algorithm,
				&hash_types,
				vdev->config.rth_bkt_sz);

		 if (status != VXGE_HW_OK) {
			vxge_debug_init(VXGE_ERR,
				"RTH configuration failed for vpath:%d",
				vdev->vpaths[index].device_id);
			return status;
		 }
	 }

	return status;
}

int vxge_mac_list_add(struct vxge_vpath *vpath, struct macInfo *mac)
{
	struct vxge_mac_addrs *new_mac_entry;
	u8 *mac_address = NULL;

	if (vpath->mac_addr_cnt >= VXGE_MAX_LEARN_MAC_ADDR_CNT)
		return TRUE;

	new_mac_entry = kzalloc(sizeof(struct vxge_mac_addrs), GFP_ATOMIC);
	if (!new_mac_entry) {
		vxge_debug_mem(VXGE_ERR,
			"%s: memory allocation failed",
			VXGE_DRIVER_NAME);
		return FALSE;
	}

	list_add(&new_mac_entry->item, &vpath->mac_addr_list);

	/* Copy the new mac address to the list */
	mac_address = (u8 *)&new_mac_entry->macaddr;
	memcpy(mac_address, mac->macaddr, ETH_ALEN);

	new_mac_entry->state = mac->state;
	vpath->mac_addr_cnt++;

	/* Is this a multicast address */
	if (0x01 & mac->macaddr[0])
		vpath->mcast_addr_cnt++;

	return TRUE;
}

/* Add a mac address to DA table */
enum vxge_hw_status vxge_add_mac_addr(struct vxgedev *vdev, struct macInfo *mac)
{
	enum vxge_hw_status status = VXGE_HW_OK;
	struct vxge_vpath *vpath;
	enum vxge_hw_vpath_mac_addr_add_mode duplicate_mode;

	if (0x01 & mac->macaddr[0]) /* multicast address */
		duplicate_mode = VXGE_HW_VPATH_MAC_ADDR_ADD_DUPLICATE;
	else
		duplicate_mode = VXGE_HW_VPATH_MAC_ADDR_REPLACE_DUPLICATE;

	vpath = &vdev->vpaths[mac->vpath_no];
	status = vxge_hw_vpath_mac_addr_add(vpath->handle, mac->macaddr,
						mac->macmask, duplicate_mode);
	if (status != VXGE_HW_OK) {
		vxge_debug_init(VXGE_ERR,
			"DA config add entry failed for vpath:%d",
			vpath->device_id);
	} else
		if (FALSE == vxge_mac_list_add(vpath, mac))
			status = -EPERM;

	return status;
}

int vxge_mac_list_del(struct vxge_vpath *vpath, struct macInfo *mac)
{
	struct list_head *entry, *next;
	u64 del_mac = 0;
	u8 *mac_address = (u8 *) (&del_mac);

	/* Copy the mac address to delete from the list */
	memcpy(mac_address, mac->macaddr, ETH_ALEN);

	list_for_each_safe(entry, next, &vpath->mac_addr_list) {
		if (((struct vxge_mac_addrs *)entry)->macaddr == del_mac) {
			list_del(entry);
			kfree((struct vxge_mac_addrs *)entry);
			vpath->mac_addr_cnt--;

			/* Is this a multicast address */
			if (0x01 & mac->macaddr[0])
				vpath->mcast_addr_cnt--;
			return TRUE;
		}
	}

	return FALSE;
}
/* delete a mac address from DA table */
enum vxge_hw_status vxge_del_mac_addr(struct vxgedev *vdev, struct macInfo *mac)
{
	enum vxge_hw_status status = VXGE_HW_OK;
	struct vxge_vpath *vpath;

	vpath = &vdev->vpaths[mac->vpath_no];
	status = vxge_hw_vpath_mac_addr_delete(vpath->handle, mac->macaddr,
						mac->macmask);
	if (status != VXGE_HW_OK) {
		vxge_debug_init(VXGE_ERR,
			"DA config delete entry failed for vpath:%d",
			vpath->device_id);
	} else
		vxge_mac_list_del(vpath, mac);
	return status;
}

/* list all mac addresses from DA table */
enum vxge_hw_status
static vxge_search_mac_addr_in_da_table(struct vxge_vpath *vpath,
					struct macInfo *mac)
{
	enum vxge_hw_status status = VXGE_HW_OK;
	unsigned char macmask[ETH_ALEN];
	unsigned char macaddr[ETH_ALEN];

	status = vxge_hw_vpath_mac_addr_get(vpath->handle,
				macaddr, macmask);
	if (status != VXGE_HW_OK) {
		vxge_debug_init(VXGE_ERR,
			"DA config list entry failed for vpath:%d",
			vpath->device_id);
		return status;
	}

	while (memcmp(mac->macaddr, macaddr, ETH_ALEN)) {

		status = vxge_hw_vpath_mac_addr_get_next(vpath->handle,
				macaddr, macmask);
		if (status != VXGE_HW_OK)
			break;
	}

	return status;
}

/* Store all vlan ids from the list to the vid table */
enum vxge_hw_status vxge_restore_vpath_vid_table(struct vxge_vpath *vpath)
{
	enum vxge_hw_status status = VXGE_HW_OK;
	struct vxgedev *vdev = vpath->vdev;
	u16 vid;

	if (vdev->vlgrp && vpath->is_open) {

		for (vid = 0; vid < VLAN_GROUP_ARRAY_LEN; vid++) {
			if (!vlan_group_get_device(vdev->vlgrp, vid))
				continue;
			/* Add these vlan to the vid table */
			status = vxge_hw_vpath_vid_add(vpath->handle, vid);
		}
	}

	return status;
}

/* Store all mac addresses from the list to the DA table */
enum vxge_hw_status vxge_restore_vpath_mac_addr(struct vxge_vpath *vpath)
{
	enum vxge_hw_status status = VXGE_HW_OK;
	struct macInfo mac_info;
	u8 *mac_address = NULL;
	struct list_head *entry, *next;

	memset(&mac_info, 0, sizeof(struct macInfo));

	if (vpath->is_open) {

		list_for_each_safe(entry, next, &vpath->mac_addr_list) {
			mac_address =
				(u8 *)&
				((struct vxge_mac_addrs *)entry)->macaddr;
			memcpy(mac_info.macaddr, mac_address, ETH_ALEN);
			((struct vxge_mac_addrs *)entry)->state =
				VXGE_LL_MAC_ADDR_IN_DA_TABLE;
			/* does this mac address already exist in da table? */
			status = vxge_search_mac_addr_in_da_table(vpath,
				&mac_info);
			if (status != VXGE_HW_OK) {
				/* Add this mac address to the DA table */
				status = vxge_hw_vpath_mac_addr_add(
					vpath->handle, mac_info.macaddr,
					mac_info.macmask,
				    VXGE_HW_VPATH_MAC_ADDR_ADD_DUPLICATE);
				if (status != VXGE_HW_OK) {
					vxge_debug_init(VXGE_ERR,
					    "DA add entry failed for vpath:%d",
					    vpath->device_id);
					((struct vxge_mac_addrs *)entry)->state
						= VXGE_LL_MAC_ADDR_IN_LIST;
				}
			}
		}
	}

	return status;
}

/* reset vpaths */
enum vxge_hw_status vxge_reset_all_vpaths(struct vxgedev *vdev)
{
	int i;
	enum vxge_hw_status status = VXGE_HW_OK;

	for (i = 0; i < vdev->no_of_vpath; i++)
		if (vdev->vpaths[i].handle) {
			if (vxge_hw_vpath_reset(vdev->vpaths[i].handle)
					== VXGE_HW_OK) {
				if (is_vxge_card_up(vdev) &&
					vxge_hw_vpath_recover_from_reset(
						vdev->vpaths[i].handle)
						!= VXGE_HW_OK) {
					vxge_debug_init(VXGE_ERR,
						"vxge_hw_vpath_recover_"
						"from_reset failed for vpath: "
						"%d", i);
					return status;
				}
			} else {
				vxge_debug_init(VXGE_ERR,
					"vxge_hw_vpath_reset failed for "
					"vpath:%d", i);
					return status;
			}
		}
	return status;
}

/* close vpaths */
void vxge_close_vpaths(struct vxgedev *vdev, int index)
{
	int i;
	for (i = index; i < vdev->no_of_vpath; i++) {
		if (vdev->vpaths[i].handle && vdev->vpaths[i].is_open) {
			vxge_hw_vpath_close(vdev->vpaths[i].handle);
			vdev->stats.vpaths_open--;
		}
		vdev->vpaths[i].is_open = 0;
		vdev->vpaths[i].handle  = NULL;
	}
}

/* open vpaths */
int vxge_open_vpaths(struct vxgedev *vdev)
{
	enum vxge_hw_status status;
	int i;
	u32 vp_id = 0;
	struct vxge_hw_vpath_attr attr;

	for (i = 0; i < vdev->no_of_vpath; i++) {
		vxge_assert(vdev->vpaths[i].is_configured);
		attr.vp_id = vdev->vpaths[i].device_id;
		attr.fifo_attr.callback = vxge_xmit_compl;
		attr.fifo_attr.txdl_term = vxge_tx_term;
		attr.fifo_attr.per_txdl_space = sizeof(struct vxge_tx_priv);
		attr.fifo_attr.userdata = (void *)&vdev->vpaths[i].fifo;

		attr.ring_attr.callback = vxge_rx_1b_compl;
		attr.ring_attr.rxd_init = vxge_rx_initial_replenish;
		attr.ring_attr.rxd_term = vxge_rx_term;
		attr.ring_attr.per_rxd_space = sizeof(struct vxge_rx_priv);
		attr.ring_attr.userdata = (void *)&vdev->vpaths[i].ring;

		vdev->vpaths[i].ring.ndev = vdev->ndev;
		vdev->vpaths[i].ring.pdev = vdev->pdev;
		status = vxge_hw_vpath_open(vdev->devh, &attr,
				&(vdev->vpaths[i].handle));
		if (status == VXGE_HW_OK) {
			vdev->vpaths[i].fifo.handle =
			    (struct __vxge_hw_fifo *)attr.fifo_attr.userdata;
			vdev->vpaths[i].ring.handle =
			    (struct __vxge_hw_ring *)attr.ring_attr.userdata;
			vdev->vpaths[i].fifo.tx_steering_type =
				vdev->config.tx_steering_type;
			vdev->vpaths[i].fifo.ndev = vdev->ndev;
			vdev->vpaths[i].fifo.pdev = vdev->pdev;
			vdev->vpaths[i].fifo.indicate_max_pkts =
				vdev->config.fifo_indicate_max_pkts;
			vdev->vpaths[i].ring.rx_vector_no = 0;
			vdev->vpaths[i].ring.rx_csum = vdev->rx_csum;
			vdev->vpaths[i].is_open = 1;
			vdev->vp_handles[i] = vdev->vpaths[i].handle;
			vdev->vpaths[i].ring.gro_enable =
						vdev->config.gro_enable;
			vdev->vpaths[i].ring.vlan_tag_strip =
						vdev->vlan_tag_strip;
			vdev->stats.vpaths_open++;
		} else {
			vdev->stats.vpath_open_fail++;
			vxge_debug_init(VXGE_ERR,
				"%s: vpath: %d failed to open "
				"with status: %d",
			    vdev->ndev->name, vdev->vpaths[i].device_id,
				status);
			vxge_close_vpaths(vdev, 0);
			return -EPERM;
		}

		vp_id =
		  ((struct __vxge_hw_vpath_handle *)vdev->vpaths[i].handle)->
		  vpath->vp_id;
		vdev->vpaths_deployed |= vxge_mBIT(vp_id);
	}
	return VXGE_HW_OK;
}

/*
 *  vxge_isr_napi
 *  @irq: the irq of the device.
 *  @dev_id: a void pointer to the hldev structure of the Titan device
 *  @ptregs: pointer to the registers pushed on the stack.
 *
 *  This function is the ISR handler of the device when napi is enabled. It
 *  identifies the reason for the interrupt and calls the relevant service
 *  routines.
 */
static irqreturn_t vxge_isr_napi(int irq, void *dev_id)
{
	struct net_device *dev;
	struct __vxge_hw_device *hldev;
	u64 reason;
	enum vxge_hw_status status;
	struct vxgedev *vdev = (struct vxgedev *) dev_id;;

	vxge_debug_intr(VXGE_TRACE, "%s:%d", __func__, __LINE__);

	dev = vdev->ndev;
	hldev = (struct __vxge_hw_device *)pci_get_drvdata(vdev->pdev);

	if (pci_channel_offline(vdev->pdev))
		return IRQ_NONE;

	if (unlikely(!is_vxge_card_up(vdev)))
		return IRQ_NONE;

	status = vxge_hw_device_begin_irq(hldev, vdev->exec_mode,
			&reason);
	if (status == VXGE_HW_OK) {
		vxge_hw_device_mask_all(hldev);

		if (reason &
			VXGE_HW_TITAN_GENERAL_INT_STATUS_VPATH_TRAFFIC_INT(
			vdev->vpaths_deployed >>
			(64 - VXGE_HW_MAX_VIRTUAL_PATHS))) {

			vxge_hw_device_clear_tx_rx(hldev);
			napi_schedule(&vdev->napi);
			vxge_debug_intr(VXGE_TRACE,
				"%s:%d  Exiting...", __func__, __LINE__);
			return IRQ_HANDLED;
		} else
			vxge_hw_device_unmask_all(hldev);
	} else if (unlikely((status == VXGE_HW_ERR_VPATH) ||
		(status == VXGE_HW_ERR_CRITICAL) ||
		(status == VXGE_HW_ERR_FIFO))) {
		vxge_hw_device_mask_all(hldev);
		vxge_hw_device_flush_io(hldev);
		return IRQ_HANDLED;
	} else if (unlikely(status == VXGE_HW_ERR_SLOT_FREEZE))
		return IRQ_HANDLED;

	vxge_debug_intr(VXGE_TRACE, "%s:%d  Exiting...", __func__, __LINE__);
	return IRQ_NONE;
}

#ifdef CONFIG_PCI_MSI

static irqreturn_t
vxge_tx_msix_handle(int irq, void *dev_id)
{
	struct vxge_fifo *fifo = (struct vxge_fifo *)dev_id;

	VXGE_COMPLETE_VPATH_TX(fifo);

	return IRQ_HANDLED;
}

static irqreturn_t
vxge_rx_msix_napi_handle(int irq, void *dev_id)
{
	struct vxge_ring *ring = (struct vxge_ring *)dev_id;

	/* MSIX_IDX for Rx is 1 */
	vxge_hw_channel_msix_mask((struct __vxge_hw_channel *)ring->handle,
					ring->rx_vector_no);

	napi_schedule(&ring->napi);
	return IRQ_HANDLED;
}

static irqreturn_t
vxge_alarm_msix_handle(int irq, void *dev_id)
{
	int i;
	enum vxge_hw_status status;
	struct vxge_vpath *vpath = (struct vxge_vpath *)dev_id;
	struct vxgedev *vdev = vpath->vdev;
	int alarm_msix_id =
		VXGE_HW_VPATH_MSIX_ACTIVE * vdev->no_of_vpath - 2;

	for (i = 0; i < vdev->no_of_vpath; i++) {
		vxge_hw_vpath_msix_mask(vdev->vpaths[i].handle,
			alarm_msix_id);

		status = vxge_hw_vpath_alarm_process(vdev->vpaths[i].handle,
			vdev->exec_mode);
		if (status == VXGE_HW_OK) {

			vxge_hw_vpath_msix_unmask(vdev->vpaths[i].handle,
				alarm_msix_id);
			continue;
		}
		vxge_debug_intr(VXGE_ERR,
			"%s: vxge_hw_vpath_alarm_process failed %x ",
			VXGE_DRIVER_NAME, status);
	}
	return IRQ_HANDLED;
}

static int vxge_alloc_msix(struct vxgedev *vdev)
{
	int j, i, ret = 0;
	int intr_cnt = 0;
	int alarm_msix_id = 0, msix_intr_vect = 0;
	vdev->intr_cnt = 0;

	/* Tx/Rx MSIX Vectors count */
	vdev->intr_cnt = vdev->no_of_vpath * 2;

	/* Alarm MSIX Vectors count */
	vdev->intr_cnt++;

	intr_cnt = (vdev->max_vpath_supported * 2) + 1;
	vdev->entries = kzalloc(intr_cnt * sizeof(struct msix_entry),
						GFP_KERNEL);
	if (!vdev->entries) {
		vxge_debug_init(VXGE_ERR,
			"%s: memory allocation failed",
			VXGE_DRIVER_NAME);
		return  -ENOMEM;
	}

	vdev->vxge_entries = kzalloc(intr_cnt * sizeof(struct vxge_msix_entry),
							GFP_KERNEL);
	if (!vdev->vxge_entries) {
		vxge_debug_init(VXGE_ERR, "%s: memory allocation failed",
			VXGE_DRIVER_NAME);
		kfree(vdev->entries);
		return -ENOMEM;
	}

	/* Last vector in the list is used for alarm */
	alarm_msix_id = VXGE_HW_VPATH_MSIX_ACTIVE * vdev->no_of_vpath - 2;
	for (i = 0, j = 0; i < vdev->max_vpath_supported; i++) {

		msix_intr_vect = i * VXGE_HW_VPATH_MSIX_ACTIVE;

		/* Initialize the fifo vector */
		vdev->entries[j].entry = msix_intr_vect;
		vdev->vxge_entries[j].entry = msix_intr_vect;
		vdev->vxge_entries[j].in_use = 0;
		j++;

		/* Initialize the ring vector */
		vdev->entries[j].entry = msix_intr_vect + 1;
		vdev->vxge_entries[j].entry = msix_intr_vect + 1;
		vdev->vxge_entries[j].in_use = 0;
		j++;
	}

	/* Initialize the alarm vector */
	vdev->entries[j].entry = alarm_msix_id;
	vdev->vxge_entries[j].entry = alarm_msix_id;
	vdev->vxge_entries[j].in_use = 0;

	ret = pci_enable_msix(vdev->pdev, vdev->entries, intr_cnt);
	/* if driver request exceeeds available irq's, request with a small
	 * number.
	*/
	if (ret > 0) {
		vxge_debug_init(VXGE_ERR,
			"%s: MSI-X enable failed for %d vectors, available: %d",
			VXGE_DRIVER_NAME, intr_cnt, ret);
		vdev->max_vpath_supported = vdev->no_of_vpath;
		intr_cnt = (vdev->max_vpath_supported * 2) + 1;

		/* Reset the alarm vector setting */
		vdev->entries[j].entry = 0;
		vdev->vxge_entries[j].entry = 0;

		/* Initialize the alarm vector with new setting */
		vdev->entries[intr_cnt - 1].entry = alarm_msix_id;
		vdev->vxge_entries[intr_cnt - 1].entry = alarm_msix_id;
		vdev->vxge_entries[intr_cnt - 1].in_use = 0;

		ret = pci_enable_msix(vdev->pdev, vdev->entries, intr_cnt);
		if (!ret)
			vxge_debug_init(VXGE_ERR,
				"%s: MSI-X enabled for %d vectors",
				VXGE_DRIVER_NAME, intr_cnt);
	}

	if (ret) {
		vxge_debug_init(VXGE_ERR,
			"%s: MSI-X enable failed for %d vectors, ret: %d",
			VXGE_DRIVER_NAME, intr_cnt, ret);
		kfree(vdev->entries);
		kfree(vdev->vxge_entries);
		vdev->entries = NULL;
		vdev->vxge_entries = NULL;
		return -ENODEV;
	}
	return 0;
}

static int vxge_enable_msix(struct vxgedev *vdev)
{

	int i, ret = 0;
	enum vxge_hw_status status;
	/* 0 - Tx, 1 - Rx  */
	int tim_msix_id[4];
	int alarm_msix_id = 0, msix_intr_vect = 0;
	vdev->intr_cnt = 0;

	/* allocate msix vectors */
	ret = vxge_alloc_msix(vdev);
	if (!ret) {
		/* Last vector in the list is used for alarm */
		alarm_msix_id =
			VXGE_HW_VPATH_MSIX_ACTIVE * vdev->no_of_vpath - 2;
		for (i = 0; i < vdev->no_of_vpath; i++) {

			/* If fifo or ring are not enabled
			   the MSIX vector for that should be set to 0
			   Hence initializeing this array to all 0s.
			*/
			memset(tim_msix_id, 0, sizeof(tim_msix_id));
			msix_intr_vect = i * VXGE_HW_VPATH_MSIX_ACTIVE;
			tim_msix_id[0] = msix_intr_vect;

			tim_msix_id[1] = msix_intr_vect + 1;
			vdev->vpaths[i].ring.rx_vector_no = tim_msix_id[1];

			status = vxge_hw_vpath_msix_set(
						vdev->vpaths[i].handle,
						tim_msix_id, alarm_msix_id);
			if (status != VXGE_HW_OK) {
				vxge_debug_init(VXGE_ERR,
					"vxge_hw_vpath_msix_set "
					"failed with status : %x", status);
				kfree(vdev->entries);
				kfree(vdev->vxge_entries);
				pci_disable_msix(vdev->pdev);
				return -ENODEV;
			}
		}
	}

	return ret;
}

static void vxge_rem_msix_isr(struct vxgedev *vdev)
{
	int intr_cnt;

	for (intr_cnt = 0; intr_cnt < (vdev->max_vpath_supported * 2 + 1);
		intr_cnt++) {
		if (vdev->vxge_entries[intr_cnt].in_use) {
			synchronize_irq(vdev->entries[intr_cnt].vector);
			free_irq(vdev->entries[intr_cnt].vector,
				vdev->vxge_entries[intr_cnt].arg);
			vdev->vxge_entries[intr_cnt].in_use = 0;
		}
	}

	kfree(vdev->entries);
	kfree(vdev->vxge_entries);
	vdev->entries = NULL;
	vdev->vxge_entries = NULL;

	if (vdev->config.intr_type == MSI_X)
		pci_disable_msix(vdev->pdev);
}
#endif

static void vxge_rem_isr(struct vxgedev *vdev)
{
	struct __vxge_hw_device  *hldev;
	hldev = (struct __vxge_hw_device  *) pci_get_drvdata(vdev->pdev);

#ifdef CONFIG_PCI_MSI
	if (vdev->config.intr_type == MSI_X) {
		vxge_rem_msix_isr(vdev);
	} else
#endif
	if (vdev->config.intr_type == INTA) {
			synchronize_irq(vdev->pdev->irq);
			free_irq(vdev->pdev->irq, vdev);
	}
}

static int vxge_add_isr(struct vxgedev *vdev)
{
	int ret = 0;
#ifdef CONFIG_PCI_MSI
	int vp_idx = 0, intr_idx = 0, intr_cnt = 0, msix_idx = 0, irq_req = 0;
	u64 function_mode = vdev->config.device_hw_info.function_mode;
	int pci_fun = PCI_FUNC(vdev->pdev->devfn);

	if (vdev->config.intr_type == MSI_X)
		ret = vxge_enable_msix(vdev);

	if (ret) {
		vxge_debug_init(VXGE_ERR,
			"%s: Enabling MSI-X Failed", VXGE_DRIVER_NAME);
		if ((function_mode == VXGE_HW_FUNCTION_MODE_MULTI_FUNCTION) &&
			test_and_set_bit(__VXGE_STATE_CARD_UP,
				&driver_config->inta_dev_open))
			return VXGE_HW_FAIL;
		else {
			vxge_debug_init(VXGE_ERR,
				"%s: Defaulting to INTA", VXGE_DRIVER_NAME);
			vdev->config.intr_type = INTA;
			vxge_hw_device_set_intr_type(vdev->devh,
				VXGE_HW_INTR_MODE_IRQLINE);
			vxge_close_vpaths(vdev, 1);
			vdev->no_of_vpath = 1;
			vdev->stats.vpaths_open = 1;
		}
	}

	if (vdev->config.intr_type == MSI_X) {
		for (intr_idx = 0;
		     intr_idx < (vdev->no_of_vpath *
			VXGE_HW_VPATH_MSIX_ACTIVE); intr_idx++) {

			msix_idx = intr_idx % VXGE_HW_VPATH_MSIX_ACTIVE;
			irq_req = 0;

			switch (msix_idx) {
			case 0:
				snprintf(vdev->desc[intr_cnt], VXGE_INTR_STRLEN,
					"%s:vxge fn: %d vpath: %d Tx MSI-X: %d",
					vdev->ndev->name, pci_fun, vp_idx,
					vdev->entries[intr_cnt].entry);
				ret = request_irq(
				    vdev->entries[intr_cnt].vector,
					vxge_tx_msix_handle, 0,
					vdev->desc[intr_cnt],
					&vdev->vpaths[vp_idx].fifo);
					vdev->vxge_entries[intr_cnt].arg =
						&vdev->vpaths[vp_idx].fifo;
				irq_req = 1;
				break;
			case 1:
				snprintf(vdev->desc[intr_cnt], VXGE_INTR_STRLEN,
					"%s:vxge fn: %d vpath: %d Rx MSI-X: %d",
					vdev->ndev->name, pci_fun, vp_idx,
					vdev->entries[intr_cnt].entry);
				ret = request_irq(
				    vdev->entries[intr_cnt].vector,
					vxge_rx_msix_napi_handle,
					0,
					vdev->desc[intr_cnt],
					&vdev->vpaths[vp_idx].ring);
					vdev->vxge_entries[intr_cnt].arg =
						&vdev->vpaths[vp_idx].ring;
				irq_req = 1;
				break;
			}

			if (ret) {
				vxge_debug_init(VXGE_ERR,
					"%s: MSIX - %d  Registration failed",
					vdev->ndev->name, intr_cnt);
				vxge_rem_msix_isr(vdev);
				if ((function_mode ==
					VXGE_HW_FUNCTION_MODE_MULTI_FUNCTION) &&
					test_and_set_bit(__VXGE_STATE_CARD_UP,
						&driver_config->inta_dev_open))
					return VXGE_HW_FAIL;
				else {
					vxge_hw_device_set_intr_type(
						vdev->devh,
						VXGE_HW_INTR_MODE_IRQLINE);
						vdev->config.intr_type = INTA;
					vxge_debug_init(VXGE_ERR,
						"%s: Defaulting to INTA"
						, vdev->ndev->name);
					vxge_close_vpaths(vdev, 1);
					vdev->no_of_vpath = 1;
					vdev->stats.vpaths_open = 1;
					goto INTA_MODE;
				}
			}

			if (irq_req) {
				/* We requested for this msix interrupt */
				vdev->vxge_entries[intr_cnt].in_use = 1;
				vxge_hw_vpath_msix_unmask(
					vdev->vpaths[vp_idx].handle,
					intr_idx);
				intr_cnt++;
			}

			/* Point to next vpath handler */
			if (((intr_idx + 1) % VXGE_HW_VPATH_MSIX_ACTIVE == 0)
				&& (vp_idx < (vdev->no_of_vpath - 1)))
					vp_idx++;
		}

		intr_cnt = vdev->max_vpath_supported * 2;
		snprintf(vdev->desc[intr_cnt], VXGE_INTR_STRLEN,
			"%s:vxge Alarm fn: %d MSI-X: %d",
			vdev->ndev->name, pci_fun,
			vdev->entries[intr_cnt].entry);
		/* For Alarm interrupts */
		ret = request_irq(vdev->entries[intr_cnt].vector,
					vxge_alarm_msix_handle, 0,
					vdev->desc[intr_cnt],
					&vdev->vpaths[vp_idx]);
		if (ret) {
			vxge_debug_init(VXGE_ERR,
				"%s: MSIX - %d Registration failed",
				vdev->ndev->name, intr_cnt);
			vxge_rem_msix_isr(vdev);
			if ((function_mode ==
				VXGE_HW_FUNCTION_MODE_MULTI_FUNCTION) &&
				test_and_set_bit(__VXGE_STATE_CARD_UP,
						&driver_config->inta_dev_open))
				return VXGE_HW_FAIL;
			else {
				vxge_hw_device_set_intr_type(vdev->devh,
						VXGE_HW_INTR_MODE_IRQLINE);
				vdev->config.intr_type = INTA;
				vxge_debug_init(VXGE_ERR,
					"%s: Defaulting to INTA",
					vdev->ndev->name);
				vxge_close_vpaths(vdev, 1);
				vdev->no_of_vpath = 1;
				vdev->stats.vpaths_open = 1;
				goto INTA_MODE;
			}
		}

		vxge_hw_vpath_msix_unmask(vdev->vpaths[vp_idx].handle,
					intr_idx - 2);
		vdev->vxge_entries[intr_cnt].in_use = 1;
		vdev->vxge_entries[intr_cnt].arg = &vdev->vpaths[vp_idx];
	}
INTA_MODE:
#endif
	snprintf(vdev->desc[0], VXGE_INTR_STRLEN, "%s:vxge", vdev->ndev->name);

	if (vdev->config.intr_type == INTA) {
		ret = request_irq((int) vdev->pdev->irq,
			vxge_isr_napi,
			IRQF_SHARED, vdev->desc[0], vdev);
		if (ret) {
			vxge_debug_init(VXGE_ERR,
				"%s %s-%d: ISR registration failed",
				VXGE_DRIVER_NAME, "IRQ", vdev->pdev->irq);
			return -ENODEV;
		}
		vxge_debug_init(VXGE_TRACE,
			"new %s-%d line allocated",
			"IRQ", vdev->pdev->irq);
	}

	return VXGE_HW_OK;
}

static void vxge_poll_vp_reset(unsigned long data)
{
	struct vxgedev *vdev = (struct vxgedev *)data;
	int i, j = 0;

	for (i = 0; i < vdev->no_of_vpath; i++) {
		if (test_bit(i, &vdev->vp_reset)) {
			vxge_reset_vpath(vdev, i);
			j++;
		}
	}
	if (j && (vdev->config.intr_type != MSI_X)) {
		vxge_hw_device_unmask_all(vdev->devh);
		vxge_hw_device_flush_io(vdev->devh);
	}

	mod_timer(&vdev->vp_reset_timer, jiffies + HZ / 2);
}

static void vxge_poll_vp_lockup(unsigned long data)
{
	struct vxgedev *vdev = (struct vxgedev *)data;
	int i;
	struct vxge_ring *ring;
	enum vxge_hw_status status = VXGE_HW_OK;

	for (i = 0; i < vdev->no_of_vpath; i++) {
		ring = &vdev->vpaths[i].ring;
		/* Did this vpath received any packets */
		if (ring->stats.prev_rx_frms == ring->stats.rx_frms) {
			status = vxge_hw_vpath_check_leak(ring->handle);

			/* Did it received any packets last time */
			if ((VXGE_HW_FAIL == status) &&
				(VXGE_HW_FAIL == ring->last_status)) {

				/* schedule vpath reset */
				if (!test_and_set_bit(i, &vdev->vp_reset)) {

					/* disable interrupts for this vpath */
					vxge_vpath_intr_disable(vdev, i);

					/* stop the queue for this vpath */
					vxge_stop_tx_queue(&vdev->vpaths[i].
								fifo);
					continue;
				}
			}
		}
		ring->stats.prev_rx_frms = ring->stats.rx_frms;
		ring->last_status = status;
	}

	/* Check every 1 milli second */
	mod_timer(&vdev->vp_lockup_timer, jiffies + HZ / 1000);
}

/**
 * vxge_open
 * @dev: pointer to the device structure.
 *
 * This function is the open entry point of the driver. It mainly calls a
 * function to allocate Rx buffers and inserts them into the buffer
 * descriptors and then enables the Rx part of the NIC.
 * Return value: '0' on success and an appropriate (-)ve integer as
 * defined in errno.h file on failure.
 */
int
vxge_open(struct net_device *dev)
{
	enum vxge_hw_status status;
	struct vxgedev *vdev;
	struct __vxge_hw_device *hldev;
	int ret = 0;
	int i;
	u64 val64, function_mode;
	vxge_debug_entryexit(VXGE_TRACE,
		"%s: %s:%d", dev->name, __func__, __LINE__);

	vdev = (struct vxgedev *)netdev_priv(dev);
	hldev = (struct __vxge_hw_device *) pci_get_drvdata(vdev->pdev);
	function_mode = vdev->config.device_hw_info.function_mode;

	/* make sure you have link off by default every time Nic is
	 * initialized */
	netif_carrier_off(dev);

	/* Check for another device already opn with INTA */
	if ((function_mode == VXGE_HW_FUNCTION_MODE_MULTI_FUNCTION) &&
		test_bit(__VXGE_STATE_CARD_UP, &driver_config->inta_dev_open)) {
		ret = -EPERM;
		goto out0;
	}

	/* Open VPATHs */
	status = vxge_open_vpaths(vdev);
	if (status != VXGE_HW_OK) {
		vxge_debug_init(VXGE_ERR,
			"%s: fatal: Vpath open failed", vdev->ndev->name);
		ret = -EPERM;
		goto out0;
	}

	vdev->mtu = dev->mtu;

	status = vxge_add_isr(vdev);
	if (status != VXGE_HW_OK) {
		vxge_debug_init(VXGE_ERR,
			"%s: fatal: ISR add failed", dev->name);
		ret = -EPERM;
		goto out1;
	}


	if (vdev->config.intr_type != MSI_X) {
		netif_napi_add(dev, &vdev->napi, vxge_poll_inta,
			vdev->config.napi_weight);
		napi_enable(&vdev->napi);
		for (i = 0; i < vdev->no_of_vpath; i++)
			vdev->vpaths[i].ring.napi_p = &vdev->napi;
	} else {
		for (i = 0; i < vdev->no_of_vpath; i++) {
			netif_napi_add(dev, &vdev->vpaths[i].ring.napi,
			    vxge_poll_msix, vdev->config.napi_weight);
			napi_enable(&vdev->vpaths[i].ring.napi);
			vdev->vpaths[i].ring.napi_p =
				&vdev->vpaths[i].ring.napi;
		}
	}

	/* configure RTH */
	if (vdev->config.rth_steering) {
		status = vxge_rth_configure(vdev);
		if (status != VXGE_HW_OK) {
			vxge_debug_init(VXGE_ERR,
				"%s: fatal: RTH configuration failed",
				dev->name);
			ret = -EPERM;
			goto out2;
		}
	}

	for (i = 0; i < vdev->no_of_vpath; i++) {
		/* set initial mtu before enabling the device */
		status = vxge_hw_vpath_mtu_set(vdev->vpaths[i].handle,
						vdev->mtu);
		if (status != VXGE_HW_OK) {
			vxge_debug_init(VXGE_ERR,
				"%s: fatal: can not set new MTU", dev->name);
			ret = -EPERM;
			goto out2;
		}
	}

	VXGE_DEVICE_DEBUG_LEVEL_SET(VXGE_TRACE, VXGE_COMPONENT_LL, vdev);
	vxge_debug_init(vdev->level_trace,
		"%s: MTU is %d", vdev->ndev->name, vdev->mtu);
	VXGE_DEVICE_DEBUG_LEVEL_SET(VXGE_ERR, VXGE_COMPONENT_LL, vdev);

	/* Reprogram the DA table with populated mac addresses */
	for (i = 0; i < vdev->no_of_vpath; i++) {
		vxge_restore_vpath_mac_addr(&vdev->vpaths[i]);
		vxge_restore_vpath_vid_table(&vdev->vpaths[i]);
	}

	/* Enable vpath to sniff all unicast/multicast traffic that not
	 * addressed to them. We allow promiscous mode for PF only
	 */

	val64 = 0;
	for (i = 0; i < VXGE_HW_MAX_VIRTUAL_PATHS; i++)
		val64 |= VXGE_HW_RXMAC_AUTHORIZE_ALL_ADDR_VP(i);

	vxge_hw_mgmt_reg_write(vdev->devh,
		vxge_hw_mgmt_reg_type_mrpcim,
		0,
		(ulong)offsetof(struct vxge_hw_mrpcim_reg,
			rxmac_authorize_all_addr),
		val64);

	vxge_hw_mgmt_reg_write(vdev->devh,
		vxge_hw_mgmt_reg_type_mrpcim,
		0,
		(ulong)offsetof(struct vxge_hw_mrpcim_reg,
			rxmac_authorize_all_vid),
		val64);

	vxge_set_multicast(dev);

	/* Enabling Bcast and mcast for all vpath */
	for (i = 0; i < vdev->no_of_vpath; i++) {
		status = vxge_hw_vpath_bcast_enable(vdev->vpaths[i].handle);
		if (status != VXGE_HW_OK)
			vxge_debug_init(VXGE_ERR,
				"%s : Can not enable bcast for vpath "
				"id %d", dev->name, i);
		if (vdev->config.addr_learn_en) {
			status =
			    vxge_hw_vpath_mcast_enable(vdev->vpaths[i].handle);
			if (status != VXGE_HW_OK)
				vxge_debug_init(VXGE_ERR,
					"%s : Can not enable mcast for vpath "
					"id %d", dev->name, i);
		}
	}

	vxge_hw_device_setpause_data(vdev->devh, 0,
		vdev->config.tx_pause_enable,
		vdev->config.rx_pause_enable);

	if (vdev->vp_reset_timer.function == NULL)
		vxge_os_timer(vdev->vp_reset_timer,
			vxge_poll_vp_reset, vdev, (HZ/2));

	if (vdev->vp_lockup_timer.function == NULL)
		vxge_os_timer(vdev->vp_lockup_timer,
			vxge_poll_vp_lockup, vdev, (HZ/2));

	set_bit(__VXGE_STATE_CARD_UP, &vdev->state);

	smp_wmb();

	if (vxge_hw_device_link_state_get(vdev->devh) == VXGE_HW_LINK_UP) {
		netif_carrier_on(vdev->ndev);
		printk(KERN_NOTICE "%s: Link Up\n", vdev->ndev->name);
		vdev->stats.link_up++;
	}

	vxge_hw_device_intr_enable(vdev->devh);

	smp_wmb();

	for (i = 0; i < vdev->no_of_vpath; i++) {
		vxge_hw_vpath_enable(vdev->vpaths[i].handle);
		smp_wmb();
		vxge_hw_vpath_rx_doorbell_init(vdev->vpaths[i].handle);
	}

	vxge_start_all_tx_queue(vdev);
	goto out0;

out2:
	vxge_rem_isr(vdev);

	/* Disable napi */
	if (vdev->config.intr_type != MSI_X)
		napi_disable(&vdev->napi);
	else {
		for (i = 0; i < vdev->no_of_vpath; i++)
			napi_disable(&vdev->vpaths[i].ring.napi);
	}

out1:
	vxge_close_vpaths(vdev, 0);
out0:
	vxge_debug_entryexit(VXGE_TRACE,
				"%s: %s:%d  Exiting...",
				dev->name, __func__, __LINE__);
	return ret;
}

/* Loop throught the mac address list and delete all the entries */
void vxge_free_mac_add_list(struct vxge_vpath *vpath)
{

	struct list_head *entry, *next;
	if (list_empty(&vpath->mac_addr_list))
		return;

	list_for_each_safe(entry, next, &vpath->mac_addr_list) {
		list_del(entry);
		kfree((struct vxge_mac_addrs *)entry);
	}
}

static void vxge_napi_del_all(struct vxgedev *vdev)
{
	int i;
	if (vdev->config.intr_type != MSI_X)
		netif_napi_del(&vdev->napi);
	else {
		for (i = 0; i < vdev->no_of_vpath; i++)
			netif_napi_del(&vdev->vpaths[i].ring.napi);
	}
	return;
}

int do_vxge_close(struct net_device *dev, int do_io)
{
	enum vxge_hw_status status;
	struct vxgedev *vdev;
	struct __vxge_hw_device *hldev;
	int i;
	u64 val64, vpath_vector;
	vxge_debug_entryexit(VXGE_TRACE, "%s: %s:%d",
		dev->name, __func__, __LINE__);

	vdev = (struct vxgedev *)netdev_priv(dev);
	hldev = (struct __vxge_hw_device *) pci_get_drvdata(vdev->pdev);

	if (unlikely(!is_vxge_card_up(vdev)))
		return 0;

	/* If vxge_handle_crit_err task is executing,
	 * wait till it completes. */
	while (test_and_set_bit(__VXGE_STATE_RESET_CARD, &vdev->state))
		msleep(50);

	clear_bit(__VXGE_STATE_CARD_UP, &vdev->state);
	if (do_io) {
		/* Put the vpath back in normal mode */
		vpath_vector = vxge_mBIT(vdev->vpaths[0].device_id);
		status = vxge_hw_mgmt_reg_read(vdev->devh,
				vxge_hw_mgmt_reg_type_mrpcim,
				0,
				(ulong)offsetof(
					struct vxge_hw_mrpcim_reg,
					rts_mgr_cbasin_cfg),
				&val64);

		if (status == VXGE_HW_OK) {
			val64 &= ~vpath_vector;
			status = vxge_hw_mgmt_reg_write(vdev->devh,
					vxge_hw_mgmt_reg_type_mrpcim,
					0,
					(ulong)offsetof(
						struct vxge_hw_mrpcim_reg,
						rts_mgr_cbasin_cfg),
					val64);
		}

		/* Remove the function 0 from promiscous mode */
		vxge_hw_mgmt_reg_write(vdev->devh,
			vxge_hw_mgmt_reg_type_mrpcim,
			0,
			(ulong)offsetof(struct vxge_hw_mrpcim_reg,
				rxmac_authorize_all_addr),
			0);

		vxge_hw_mgmt_reg_write(vdev->devh,
			vxge_hw_mgmt_reg_type_mrpcim,
			0,
			(ulong)offsetof(struct vxge_hw_mrpcim_reg,
				rxmac_authorize_all_vid),
			0);

		smp_wmb();
	}
	del_timer_sync(&vdev->vp_lockup_timer);

	del_timer_sync(&vdev->vp_reset_timer);

	/* Disable napi */
	if (vdev->config.intr_type != MSI_X)
		napi_disable(&vdev->napi);
	else {
		for (i = 0; i < vdev->no_of_vpath; i++)
			napi_disable(&vdev->vpaths[i].ring.napi);
	}

	netif_carrier_off(vdev->ndev);
	printk(KERN_NOTICE "%s: Link Down\n", vdev->ndev->name);
	vxge_stop_all_tx_queue(vdev);

	/* Note that at this point xmit() is stopped by upper layer */
	if (do_io)
		vxge_hw_device_intr_disable(vdev->devh);

	mdelay(1000);

	vxge_rem_isr(vdev);

	vxge_napi_del_all(vdev);

	if (do_io)
		vxge_reset_all_vpaths(vdev);

	vxge_close_vpaths(vdev, 0);

	vxge_debug_entryexit(VXGE_TRACE,
		"%s: %s:%d  Exiting...", dev->name, __func__, __LINE__);

	clear_bit(__VXGE_STATE_CARD_UP, &driver_config->inta_dev_open);
	clear_bit(__VXGE_STATE_RESET_CARD, &vdev->state);

	return 0;
}

/**
 * vxge_close
 * @dev: device pointer.
 *
 * This is the stop entry point of the driver. It needs to undo exactly
 * whatever was done by the open entry point, thus it's usually referred to
 * as the close function.Among other things this function mainly stops the
 * Rx side of the NIC and frees all the Rx buffers in the Rx rings.
 * Return value: '0' on success and an appropriate (-)ve integer as
 * defined in errno.h file on failure.
 */
int
vxge_close(struct net_device *dev)
{
	do_vxge_close(dev, 1);
	return 0;
}

/**
 * vxge_change_mtu
 * @dev: net device pointer.
 * @new_mtu :the new MTU size for the device.
 *
 * A driver entry point to change MTU size for the device. Before changing
 * the MTU the device must be stopped.
 */
static int vxge_change_mtu(struct net_device *dev, int new_mtu)
{
	struct vxgedev *vdev = netdev_priv(dev);

	vxge_debug_entryexit(vdev->level_trace,
		"%s:%d", __func__, __LINE__);
	if ((new_mtu < VXGE_HW_MIN_MTU) || (new_mtu > VXGE_HW_MAX_MTU)) {
		vxge_debug_init(vdev->level_err,
			"%s: mtu size is invalid", dev->name);
		return -EPERM;
	}

	/* check if device is down already */
	if (unlikely(!is_vxge_card_up(vdev))) {
		/* just store new value, will use later on open() */
		dev->mtu = new_mtu;
		vxge_debug_init(vdev->level_err,
			"%s", "device is down on MTU change");
		return 0;
	}

	vxge_debug_init(vdev->level_trace,
		"trying to apply new MTU %d", new_mtu);

	if (vxge_close(dev))
		return -EIO;

	dev->mtu = new_mtu;
	vdev->mtu = new_mtu;

	if (vxge_open(dev))
		return -EIO;

	vxge_debug_init(vdev->level_trace,
		"%s: MTU changed to %d", vdev->ndev->name, new_mtu);

	vxge_debug_entryexit(vdev->level_trace,
		"%s:%d  Exiting...", __func__, __LINE__);

	return 0;
}

/**
 * vxge_get_stats
 * @dev: pointer to the device structure
 *
 * Updates the device statistics structure. This function updates the device
 * statistics structure in the net_device structure and returns a pointer
 * to the same.
 */
static struct net_device_stats *
vxge_get_stats(struct net_device *dev)
{
	struct vxgedev *vdev;
	struct net_device_stats *net_stats;
	int k;

	vdev = netdev_priv(dev);

	net_stats = &vdev->stats.net_stats;

	memset(net_stats, 0, sizeof(struct net_device_stats));

	for (k = 0; k < vdev->no_of_vpath; k++) {
		net_stats->rx_packets += vdev->vpaths[k].ring.stats.rx_frms;
		net_stats->rx_bytes += vdev->vpaths[k].ring.stats.rx_bytes;
		net_stats->rx_errors += vdev->vpaths[k].ring.stats.rx_errors;
		net_stats->multicast += vdev->vpaths[k].ring.stats.rx_mcast;
		net_stats->rx_dropped +=
			vdev->vpaths[k].ring.stats.rx_dropped;

		net_stats->tx_packets += vdev->vpaths[k].fifo.stats.tx_frms;
		net_stats->tx_bytes += vdev->vpaths[k].fifo.stats.tx_bytes;
		net_stats->tx_errors += vdev->vpaths[k].fifo.stats.tx_errors;
	}

	return net_stats;
}

/**
 * vxge_ioctl
 * @dev: Device pointer.
 * @ifr: An IOCTL specific structure, that can contain a pointer to
 *       a proprietary structure used to pass information to the driver.
 * @cmd: This is used to distinguish between the different commands that
 *       can be passed to the IOCTL functions.
 *
 * Entry point for the Ioctl.
 */
static int vxge_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	return -EOPNOTSUPP;
}

/**
 * vxge_tx_watchdog
 * @dev: pointer to net device structure
 *
 * Watchdog for transmit side.
 * This function is triggered if the Tx Queue is stopped
 * for a pre-defined amount of time when the Interface is still up.
 */
static void
vxge_tx_watchdog(struct net_device *dev)
{
	struct vxgedev *vdev;

	vxge_debug_entryexit(VXGE_TRACE, "%s:%d", __func__, __LINE__);

	vdev = (struct vxgedev *)netdev_priv(dev);

	vdev->cric_err_event = VXGE_HW_EVENT_RESET_START;

	vxge_reset(vdev);
	vxge_debug_entryexit(VXGE_TRACE,
		"%s:%d  Exiting...", __func__, __LINE__);
}

/**
 * vxge_vlan_rx_register
 * @dev: net device pointer.
 * @grp: vlan group
 *
 * Vlan group registration
 */
static void
vxge_vlan_rx_register(struct net_device *dev, struct vlan_group *grp)
{
	struct vxgedev *vdev;
	struct vxge_vpath *vpath;
	int vp;
	u64 vid;
	enum vxge_hw_status status;
	int i;

	vxge_debug_entryexit(VXGE_TRACE, "%s:%d", __func__, __LINE__);

	vdev = (struct vxgedev *)netdev_priv(dev);

	vpath = &vdev->vpaths[0];
	if ((NULL == grp) && (vpath->is_open)) {
		/* Get the first vlan */
		status = vxge_hw_vpath_vid_get(vpath->handle, &vid);

		while (status == VXGE_HW_OK) {

			/* Delete this vlan from the vid table */
			for (vp = 0; vp < vdev->no_of_vpath; vp++) {
				vpath = &vdev->vpaths[vp];
				if (!vpath->is_open)
					continue;

				vxge_hw_vpath_vid_delete(vpath->handle, vid);
			}

			/* Get the next vlan to be deleted */
			vpath = &vdev->vpaths[0];
			status = vxge_hw_vpath_vid_get(vpath->handle, &vid);
		}
	}

	vdev->vlgrp = grp;

	for (i = 0; i < vdev->no_of_vpath; i++) {
		if (vdev->vpaths[i].is_configured)
			vdev->vpaths[i].ring.vlgrp = grp;
	}

	vxge_debug_entryexit(VXGE_TRACE,
		"%s:%d  Exiting...", __func__, __LINE__);
}

/**
 * vxge_vlan_rx_add_vid
 * @dev: net device pointer.
 * @vid: vid
 *
 * Add the vlan id to the devices vlan id table
 */
static void
vxge_vlan_rx_add_vid(struct net_device *dev, unsigned short vid)
{
	struct vxgedev *vdev;
	struct vxge_vpath *vpath;
	int vp_id;

	vdev = (struct vxgedev *)netdev_priv(dev);

	/* Add these vlan to the vid table */
	for (vp_id = 0; vp_id < vdev->no_of_vpath; vp_id++) {
		vpath = &vdev->vpaths[vp_id];
		if (!vpath->is_open)
			continue;
		vxge_hw_vpath_vid_add(vpath->handle, vid);
	}
}

/**
 * vxge_vlan_rx_add_vid
 * @dev: net device pointer.
 * @vid: vid
 *
 * Remove the vlan id from the device's vlan id table
 */
static void
vxge_vlan_rx_kill_vid(struct net_device *dev, unsigned short vid)
{
	struct vxgedev *vdev;
	struct vxge_vpath *vpath;
	int vp_id;

	vxge_debug_entryexit(VXGE_TRACE, "%s:%d", __func__, __LINE__);

	vdev = (struct vxgedev *)netdev_priv(dev);

	vlan_group_set_device(vdev->vlgrp, vid, NULL);

	/* Delete this vlan from the vid table */
	for (vp_id = 0; vp_id < vdev->no_of_vpath; vp_id++) {
		vpath = &vdev->vpaths[vp_id];
		if (!vpath->is_open)
			continue;
		vxge_hw_vpath_vid_delete(vpath->handle, vid);
	}
	vxge_debug_entryexit(VXGE_TRACE,
		"%s:%d  Exiting...", __func__, __LINE__);
}

static const struct net_device_ops vxge_netdev_ops = {
	.ndo_open               = vxge_open,
	.ndo_stop               = vxge_close,
	.ndo_get_stats          = vxge_get_stats,
	.ndo_start_xmit         = vxge_xmit,
	.ndo_validate_addr      = eth_validate_addr,
	.ndo_set_multicast_list = vxge_set_multicast,

	.ndo_do_ioctl           = vxge_ioctl,

	.ndo_set_mac_address    = vxge_set_mac_addr,
	.ndo_change_mtu         = vxge_change_mtu,
	.ndo_vlan_rx_register   = vxge_vlan_rx_register,
	.ndo_vlan_rx_kill_vid   = vxge_vlan_rx_kill_vid,
	.ndo_vlan_rx_add_vid	= vxge_vlan_rx_add_vid,

	.ndo_tx_timeout         = vxge_tx_watchdog,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller    = vxge_netpoll,
#endif
};

int __devinit vxge_device_register(struct __vxge_hw_device *hldev,
				   struct vxge_config *config,
				   int high_dma, int no_of_vpath,
				   struct vxgedev **vdev_out)
{
	struct net_device *ndev;
	enum vxge_hw_status status = VXGE_HW_OK;
	struct vxgedev *vdev;
	int i, ret = 0, no_of_queue = 1;
	u64 stat;

	*vdev_out = NULL;
	if (config->tx_steering_type == TX_MULTIQ_STEERING)
		no_of_queue = no_of_vpath;

	ndev = alloc_etherdev_mq(sizeof(struct vxgedev),
			no_of_queue);
	if (ndev == NULL) {
		vxge_debug_init(
			vxge_hw_device_trace_level_get(hldev),
		"%s : device allocation failed", __func__);
		ret = -ENODEV;
		goto _out0;
	}

	vxge_debug_entryexit(
		vxge_hw_device_trace_level_get(hldev),
		"%s: %s:%d  Entering...",
		ndev->name, __func__, __LINE__);

	vdev = netdev_priv(ndev);
	memset(vdev, 0, sizeof(struct vxgedev));

	vdev->ndev = ndev;
	vdev->devh = hldev;
	vdev->pdev = hldev->pdev;
	memcpy(&vdev->config, config, sizeof(struct vxge_config));
	vdev->rx_csum = 1;	/* Enable Rx CSUM by default. */

	SET_NETDEV_DEV(ndev, &vdev->pdev->dev);

	ndev->features |= NETIF_F_HW_VLAN_TX | NETIF_F_HW_VLAN_RX |
				NETIF_F_HW_VLAN_FILTER;
	/*  Driver entry points */
	ndev->irq = vdev->pdev->irq;
	ndev->base_addr = (unsigned long) hldev->bar0;

	ndev->netdev_ops = &vxge_netdev_ops;

	ndev->watchdog_timeo = VXGE_LL_WATCH_DOG_TIMEOUT;

	initialize_ethtool_ops(ndev);

	/* Allocate memory for vpath */
	vdev->vpaths = kzalloc((sizeof(struct vxge_vpath)) *
				no_of_vpath, GFP_KERNEL);
	if (!vdev->vpaths) {
		vxge_debug_init(VXGE_ERR,
			"%s: vpath memory allocation failed",
			vdev->ndev->name);
		ret = -ENODEV;
		goto _out1;
	}

	ndev->features |= NETIF_F_SG;

	ndev->features |= NETIF_F_HW_CSUM;
	vxge_debug_init(vxge_hw_device_trace_level_get(hldev),
		"%s : checksuming enabled", __func__);

	if (high_dma) {
		ndev->features |= NETIF_F_HIGHDMA;
		vxge_debug_init(vxge_hw_device_trace_level_get(hldev),
			"%s : using High DMA", __func__);
	}

	ndev->features |= NETIF_F_TSO | NETIF_F_TSO6;

	if (vdev->config.gro_enable)
		ndev->features |= NETIF_F_GRO;

	if (vdev->config.tx_steering_type == TX_MULTIQ_STEERING)
		ndev->real_num_tx_queues = no_of_vpath;

#ifdef NETIF_F_LLTX
	ndev->features |= NETIF_F_LLTX;
#endif

	for (i = 0; i < no_of_vpath; i++)
		spin_lock_init(&vdev->vpaths[i].fifo.tx_lock);

	if (register_netdev(ndev)) {
		vxge_debug_init(vxge_hw_device_trace_level_get(hldev),
			"%s: %s : device registration failed!",
			ndev->name, __func__);
		ret = -ENODEV;
		goto _out2;
	}

	/*  Set the factory defined MAC address initially */
	ndev->addr_len = ETH_ALEN;

	/* Make Link state as off at this point, when the Link change
	 * interrupt comes the state will be automatically changed to
	 * the right state.
	 */
	netif_carrier_off(ndev);

	vxge_debug_init(vxge_hw_device_trace_level_get(hldev),
		"%s: Ethernet device registered",
		ndev->name);

	*vdev_out = vdev;

	/* Resetting the Device stats */
	status = vxge_hw_mrpcim_stats_access(
				hldev,
				VXGE_HW_STATS_OP_CLEAR_ALL_STATS,
				0,
				0,
				&stat);

	if (status == VXGE_HW_ERR_PRIVILAGED_OPEARATION)
		vxge_debug_init(
			vxge_hw_device_trace_level_get(hldev),
			"%s: device stats clear returns"
			"VXGE_HW_ERR_PRIVILAGED_OPEARATION", ndev->name);

	vxge_debug_entryexit(vxge_hw_device_trace_level_get(hldev),
		"%s: %s:%d  Exiting...",
		ndev->name, __func__, __LINE__);

	return ret;
_out2:
	kfree(vdev->vpaths);
_out1:
	free_netdev(ndev);
_out0:
	return ret;
}

/*
 * vxge_device_unregister
 *
 * This function will unregister and free network device
 */
void
vxge_device_unregister(struct __vxge_hw_device *hldev)
{
	struct vxgedev *vdev;
	struct net_device *dev;
	char buf[IFNAMSIZ];
#if ((VXGE_DEBUG_INIT & VXGE_DEBUG_MASK) || \
	(VXGE_DEBUG_ENTRYEXIT & VXGE_DEBUG_MASK))
	u32 level_trace;
#endif

	dev = hldev->ndev;
	vdev = netdev_priv(dev);
#if ((VXGE_DEBUG_INIT & VXGE_DEBUG_MASK) || \
	(VXGE_DEBUG_ENTRYEXIT & VXGE_DEBUG_MASK))
	level_trace = vdev->level_trace;
#endif
	vxge_debug_entryexit(level_trace,
		"%s: %s:%d", vdev->ndev->name, __func__, __LINE__);

	memcpy(buf, vdev->ndev->name, IFNAMSIZ);

	/* in 2.6 will call stop() if device is up */
	unregister_netdev(dev);

	flush_scheduled_work();

	vxge_debug_init(level_trace, "%s: ethernet device unregistered", buf);
	vxge_debug_entryexit(level_trace,
		"%s: %s:%d  Exiting...", buf, __func__, __LINE__);
}

/*
 * vxge_callback_crit_err
 *
 * This function is called by the alarm handler in interrupt context.
 * Driver must analyze it based on the event type.
 */
static void
vxge_callback_crit_err(struct __vxge_hw_device *hldev,
			enum vxge_hw_event type, u64 vp_id)
{
	struct net_device *dev = hldev->ndev;
	struct vxgedev *vdev = (struct vxgedev *)netdev_priv(dev);
	int vpath_idx;

	vxge_debug_entryexit(vdev->level_trace,
		"%s: %s:%d", vdev->ndev->name, __func__, __LINE__);

	/* Note: This event type should be used for device wide
	 * indications only - Serious errors, Slot freeze and critical errors
	 */
	vdev->cric_err_event = type;

	for (vpath_idx = 0; vpath_idx < vdev->no_of_vpath; vpath_idx++)
		if (vdev->vpaths[vpath_idx].device_id == vp_id)
			break;

	if (!test_bit(__VXGE_STATE_RESET_CARD, &vdev->state)) {
		if (type == VXGE_HW_EVENT_SLOT_FREEZE) {
			vxge_debug_init(VXGE_ERR,
				"%s: Slot is frozen", vdev->ndev->name);
		} else if (type == VXGE_HW_EVENT_SERR) {
			vxge_debug_init(VXGE_ERR,
				"%s: Encountered Serious Error",
				vdev->ndev->name);
		} else if (type == VXGE_HW_EVENT_CRITICAL_ERR)
			vxge_debug_init(VXGE_ERR,
				"%s: Encountered Critical Error",
				vdev->ndev->name);
	}

	if ((type == VXGE_HW_EVENT_SERR) ||
		(type == VXGE_HW_EVENT_SLOT_FREEZE)) {
		if (unlikely(vdev->exec_mode))
			clear_bit(__VXGE_STATE_CARD_UP, &vdev->state);
	} else if (type == VXGE_HW_EVENT_CRITICAL_ERR) {
		vxge_hw_device_mask_all(hldev);
		if (unlikely(vdev->exec_mode))
			clear_bit(__VXGE_STATE_CARD_UP, &vdev->state);
	} else if ((type == VXGE_HW_EVENT_FIFO_ERR) ||
		  (type == VXGE_HW_EVENT_VPATH_ERR)) {

		if (unlikely(vdev->exec_mode))
			clear_bit(__VXGE_STATE_CARD_UP, &vdev->state);
		else {
			/* check if this vpath is already set for reset */
			if (!test_and_set_bit(vpath_idx, &vdev->vp_reset)) {

				/* disable interrupts for this vpath */
				vxge_vpath_intr_disable(vdev, vpath_idx);

				/* stop the queue for this vpath */
				vxge_stop_tx_queue(&vdev->vpaths[vpath_idx].
							fifo);
			}
		}
	}

	vxge_debug_entryexit(vdev->level_trace,
		"%s: %s:%d  Exiting...",
		vdev->ndev->name, __func__, __LINE__);
}

static void verify_bandwidth(void)
{
	int i, band_width, total = 0, equal_priority = 0;

	/* 1. If user enters 0 for some fifo, give equal priority to all */
	for (i = 0; i < VXGE_HW_MAX_VIRTUAL_PATHS; i++) {
		if (bw_percentage[i] == 0) {
			equal_priority = 1;
			break;
		}
	}

	if (!equal_priority) {
		/* 2. If sum exceeds 100, give equal priority to all */
		for (i = 0; i < VXGE_HW_MAX_VIRTUAL_PATHS; i++) {
			if (bw_percentage[i] == 0xFF)
				break;

			total += bw_percentage[i];
			if (total > VXGE_HW_VPATH_BANDWIDTH_MAX) {
				equal_priority = 1;
				break;
			}
		}
	}

	if (!equal_priority) {
		/* Is all the bandwidth consumed? */
		if (total < VXGE_HW_VPATH_BANDWIDTH_MAX) {
			if (i < VXGE_HW_MAX_VIRTUAL_PATHS) {
				/* Split rest of bw equally among next VPs*/
				band_width =
				  (VXGE_HW_VPATH_BANDWIDTH_MAX  - total) /
					(VXGE_HW_MAX_VIRTUAL_PATHS - i);
				if (band_width < 2) /* min of 2% */
					equal_priority = 1;
				else {
					for (; i < VXGE_HW_MAX_VIRTUAL_PATHS;
						i++)
						bw_percentage[i] =
							band_width;
				}
			}
		} else if (i < VXGE_HW_MAX_VIRTUAL_PATHS)
			equal_priority = 1;
	}

	if (equal_priority) {
		vxge_debug_init(VXGE_ERR,
			"%s: Assigning equal bandwidth to all the vpaths",
			VXGE_DRIVER_NAME);
		bw_percentage[0] = VXGE_HW_VPATH_BANDWIDTH_MAX /
					VXGE_HW_MAX_VIRTUAL_PATHS;
		for (i = 1; i < VXGE_HW_MAX_VIRTUAL_PATHS; i++)
			bw_percentage[i] = bw_percentage[0];
	}

	return;
}

/*
 * Vpath configuration
 */
static int __devinit vxge_config_vpaths(
			struct vxge_hw_device_config *device_config,
			u64 vpath_mask, struct vxge_config *config_param)
{
	int i, no_of_vpaths = 0, default_no_vpath = 0, temp;
	u32 txdl_size, txdl_per_memblock;

	temp = driver_config->vpath_per_dev;
	if ((driver_config->vpath_per_dev == VXGE_USE_DEFAULT) &&
		(max_config_dev == VXGE_MAX_CONFIG_DEV)) {
		/* No more CPU. Return vpath number as zero.*/
		if (driver_config->g_no_cpus == -1)
			return 0;

		if (!driver_config->g_no_cpus)
			driver_config->g_no_cpus = num_online_cpus();

		driver_config->vpath_per_dev = driver_config->g_no_cpus >> 1;
		if (!driver_config->vpath_per_dev)
			driver_config->vpath_per_dev = 1;

		for (i = 0; i < VXGE_HW_MAX_VIRTUAL_PATHS; i++)
			if (!vxge_bVALn(vpath_mask, i, 1))
				continue;
			else
				default_no_vpath++;
		if (default_no_vpath < driver_config->vpath_per_dev)
			driver_config->vpath_per_dev = default_no_vpath;

		driver_config->g_no_cpus = driver_config->g_no_cpus -
				(driver_config->vpath_per_dev * 2);
		if (driver_config->g_no_cpus <= 0)
			driver_config->g_no_cpus = -1;
	}

	if (driver_config->vpath_per_dev == 1) {
		vxge_debug_ll_config(VXGE_TRACE,
			"%s: Disable tx and rx steering, "
			"as single vpath is configured", VXGE_DRIVER_NAME);
		config_param->rth_steering = NO_STEERING;
		config_param->tx_steering_type = NO_STEERING;
		device_config->rth_en = 0;
	}

	/* configure bandwidth */
	for (i = 0; i < VXGE_HW_MAX_VIRTUAL_PATHS; i++)
		device_config->vp_config[i].min_bandwidth = bw_percentage[i];

	for (i = 0; i < VXGE_HW_MAX_VIRTUAL_PATHS; i++) {
		device_config->vp_config[i].vp_id = i;
		device_config->vp_config[i].mtu = VXGE_HW_DEFAULT_MTU;
		if (no_of_vpaths < driver_config->vpath_per_dev) {
			if (!vxge_bVALn(vpath_mask, i, 1)) {
				vxge_debug_ll_config(VXGE_TRACE,
					"%s: vpath: %d is not available",
					VXGE_DRIVER_NAME, i);
				continue;
			} else {
				vxge_debug_ll_config(VXGE_TRACE,
					"%s: vpath: %d available",
					VXGE_DRIVER_NAME, i);
				no_of_vpaths++;
			}
		} else {
			vxge_debug_ll_config(VXGE_TRACE,
				"%s: vpath: %d is not configured, "
				"max_config_vpath exceeded",
				VXGE_DRIVER_NAME, i);
			break;
		}

		/* Configure Tx fifo's */
		device_config->vp_config[i].fifo.enable =
						VXGE_HW_FIFO_ENABLE;
		device_config->vp_config[i].fifo.max_frags =
				MAX_SKB_FRAGS;
		device_config->vp_config[i].fifo.memblock_size =
			VXGE_HW_MIN_FIFO_MEMBLOCK_SIZE;

		txdl_size = MAX_SKB_FRAGS * sizeof(struct vxge_hw_fifo_txd);
		txdl_per_memblock = VXGE_HW_MIN_FIFO_MEMBLOCK_SIZE / txdl_size;

		device_config->vp_config[i].fifo.fifo_blocks =
			((VXGE_DEF_FIFO_LENGTH - 1) / txdl_per_memblock) + 1;

		device_config->vp_config[i].fifo.intr =
				VXGE_HW_FIFO_QUEUE_INTR_DISABLE;

		/* Configure tti properties */
		device_config->vp_config[i].tti.intr_enable =
					VXGE_HW_TIM_INTR_ENABLE;

		device_config->vp_config[i].tti.btimer_val =
			(VXGE_TTI_BTIMER_VAL * 1000) / 272;

		device_config->vp_config[i].tti.timer_ac_en =
				VXGE_HW_TIM_TIMER_AC_ENABLE;

		/* For msi-x with napi (each vector
		has a handler of its own) -
		Set CI to OFF for all vpaths */
		device_config->vp_config[i].tti.timer_ci_en =
			VXGE_HW_TIM_TIMER_CI_DISABLE;

		device_config->vp_config[i].tti.timer_ri_en =
				VXGE_HW_TIM_TIMER_RI_DISABLE;

		device_config->vp_config[i].tti.util_sel =
			VXGE_HW_TIM_UTIL_SEL_LEGACY_TX_NET_UTIL;

		device_config->vp_config[i].tti.ltimer_val =
			(VXGE_TTI_LTIMER_VAL * 1000) / 272;

		device_config->vp_config[i].tti.rtimer_val =
			(VXGE_TTI_RTIMER_VAL * 1000) / 272;

		device_config->vp_config[i].tti.urange_a = TTI_TX_URANGE_A;
		device_config->vp_config[i].tti.urange_b = TTI_TX_URANGE_B;
		device_config->vp_config[i].tti.urange_c = TTI_TX_URANGE_C;
		device_config->vp_config[i].tti.uec_a = TTI_TX_UFC_A;
		device_config->vp_config[i].tti.uec_b = TTI_TX_UFC_B;
		device_config->vp_config[i].tti.uec_c = TTI_TX_UFC_C;
		device_config->vp_config[i].tti.uec_d = TTI_TX_UFC_D;

		/* Configure Rx rings */
		device_config->vp_config[i].ring.enable  =
						VXGE_HW_RING_ENABLE;

		device_config->vp_config[i].ring.ring_blocks  =
						VXGE_HW_DEF_RING_BLOCKS;
		device_config->vp_config[i].ring.buffer_mode =
			VXGE_HW_RING_RXD_BUFFER_MODE_1;
		device_config->vp_config[i].ring.rxds_limit  =
				VXGE_HW_DEF_RING_RXDS_LIMIT;
		device_config->vp_config[i].ring.scatter_mode =
					VXGE_HW_RING_SCATTER_MODE_A;

		/* Configure rti properties */
		device_config->vp_config[i].rti.intr_enable =
					VXGE_HW_TIM_INTR_ENABLE;

		device_config->vp_config[i].rti.btimer_val =
			(VXGE_RTI_BTIMER_VAL * 1000)/272;

		device_config->vp_config[i].rti.timer_ac_en =
						VXGE_HW_TIM_TIMER_AC_ENABLE;

		device_config->vp_config[i].rti.timer_ci_en =
						VXGE_HW_TIM_TIMER_CI_DISABLE;

		device_config->vp_config[i].rti.timer_ri_en =
						VXGE_HW_TIM_TIMER_RI_DISABLE;

		device_config->vp_config[i].rti.util_sel =
				VXGE_HW_TIM_UTIL_SEL_LEGACY_RX_NET_UTIL;

		device_config->vp_config[i].rti.urange_a =
						RTI_RX_URANGE_A;
		device_config->vp_config[i].rti.urange_b =
						RTI_RX_URANGE_B;
		device_config->vp_config[i].rti.urange_c =
						RTI_RX_URANGE_C;
		device_config->vp_config[i].rti.uec_a = RTI_RX_UFC_A;
		device_config->vp_config[i].rti.uec_b = RTI_RX_UFC_B;
		device_config->vp_config[i].rti.uec_c = RTI_RX_UFC_C;
		device_config->vp_config[i].rti.uec_d = RTI_RX_UFC_D;

		device_config->vp_config[i].rti.rtimer_val =
			(VXGE_RTI_RTIMER_VAL * 1000) / 272;

		device_config->vp_config[i].rti.ltimer_val =
			(VXGE_RTI_LTIMER_VAL * 1000) / 272;

		device_config->vp_config[i].rpa_strip_vlan_tag =
			vlan_tag_strip;
	}

	driver_config->vpath_per_dev = temp;
	return no_of_vpaths;
}

/* initialize device configuratrions */
static void __devinit vxge_device_config_init(
				struct vxge_hw_device_config *device_config,
				int *intr_type)
{
	/* Used for CQRQ/SRQ. */
	device_config->dma_blockpool_initial =
			VXGE_HW_INITIAL_DMA_BLOCK_POOL_SIZE;

	device_config->dma_blockpool_max =
			VXGE_HW_MAX_DMA_BLOCK_POOL_SIZE;

	if (max_mac_vpath > VXGE_MAX_MAC_ADDR_COUNT)
		max_mac_vpath = VXGE_MAX_MAC_ADDR_COUNT;

#ifndef CONFIG_PCI_MSI
	vxge_debug_init(VXGE_ERR,
		"%s: This Kernel does not support "
		"MSI-X. Defaulting to INTA", VXGE_DRIVER_NAME);
	*intr_type = INTA;
#endif

	/* Configure whether MSI-X or IRQL. */
	switch (*intr_type) {
	case INTA:
		device_config->intr_mode = VXGE_HW_INTR_MODE_IRQLINE;
		break;

	case MSI_X:
		device_config->intr_mode = VXGE_HW_INTR_MODE_MSIX;
		break;
	}
	/* Timer period between device poll */
	device_config->device_poll_millis = VXGE_TIMER_DELAY;

	/* Configure mac based steering. */
	device_config->rts_mac_en = addr_learn_en;

	/* Configure Vpaths */
	device_config->rth_it_type = VXGE_HW_RTH_IT_TYPE_MULTI_IT;

	vxge_debug_ll_config(VXGE_TRACE, "%s : Device Config Params ",
			__func__);
	vxge_debug_ll_config(VXGE_TRACE, "dma_blockpool_initial : %d",
			device_config->dma_blockpool_initial);
	vxge_debug_ll_config(VXGE_TRACE, "dma_blockpool_max : %d",
			device_config->dma_blockpool_max);
	vxge_debug_ll_config(VXGE_TRACE, "intr_mode : %d",
			device_config->intr_mode);
	vxge_debug_ll_config(VXGE_TRACE, "device_poll_millis : %d",
			device_config->device_poll_millis);
	vxge_debug_ll_config(VXGE_TRACE, "rts_mac_en : %d",
			device_config->rts_mac_en);
	vxge_debug_ll_config(VXGE_TRACE, "rth_en : %d",
			device_config->rth_en);
	vxge_debug_ll_config(VXGE_TRACE, "rth_it_type : %d",
			device_config->rth_it_type);
}

static void __devinit vxge_print_parm(struct vxgedev *vdev, u64 vpath_mask)
{
	int i;

	vxge_debug_init(VXGE_TRACE,
		"%s: %d Vpath(s) opened",
		vdev->n******ame, *******o_of_v****);

	switch (******config.intr_type) {
	case INTA:****xge_debug_init(VXGE_TRACE**********Interrupt may nd di"******************);
		break****ed anMSI_Xstributed according to the terms of
* the GNU General PMSI-Xc License (GPL), incorporated h	}

	if***
* This softrth_steeringbe usibuted according to the terms of
* thRTH nd licen enabled for TCP_IPV4**********************corp} elsee notice.  This file is not
* a complete program and mdis onlyc License (GPL), incorpe au*********
* This softtxand licen may be used anNO_STEERINGstributed according to the terms of
* thTxYING in this distribution for more informretain thed anTX_PRIORITY00 Series 10GbE PCIe I/O
*              VirtualizUnsupported td Server Adoption the entire operating
* syGbE PCIe I/O
*              Virtualized Server Adapter.
* Copyright(c) 2002-2009 xge-main.c: Driver for Neteri = 0009 Neterion Inc.
*
VLANdule loadable parameters that are supported by the driver and a brief
* explanation of all the variables:
* vlan_tag_strip:
*	Strip VLAN Tag enable/disable. Instructs the device to remove
*	the VLAN tag from all received tagged frames thatMULTIQ00 Series 10GbE PCIe I/O
*              Virtualized multiqueuegram and may onlytion of all the variables:
Neterion Inc.
*
*ORT00 Series 10GbE PCIe I/O
*              Virtualized ver ted.
*		MIN -1 and MAX - 2
*
* max_config_vpath:
*	Tdefaultstributed according to thERRrted by the driver and a brief
* emay tion of all the variables:
* vlan_tag_strip:
*	Strip VLAN Tag enable/disable. Instructs the device to remove
*	the VLAN tag from all received te authorship, copyriggro_ay onlse notice.  This file is noof Device funGeneric receive offloadIN -1 and MAX - 2
*
* max_config_vstem is:
* vlan_tag_strip:
*	Strip VLAN Tag evice.h>
#include <linux/is distribhe entire operating
* sauthorship, copyrigaddr_learn_en)s:
* vlan_tag_strip:
*	Strip VLAN Tag eMAC Address ualiz		MIN -1 and License (GPL), incor
ibuted according to the terms olete px doorbell mode[] __devinitdata = {
	{PCI_VENDObe u(ieived i < to thHW_MAX_VIRTUAL_PATHS; i++se notthor!butedbVALn(*****_mask, i, 1)Serv	continuees:
* vlan_tag_ll_is sofstatic struct pci_deviTU size - %vinitdata = {
	{PCI_("Net((struct __butedhw_device  *)**
* Thdevh))->vxgee_idsoftvpPARAM_I[i].mtules:
* vlan_tag_strip:
*	Strip VLAN Tag e are tag strippnd m%sPA_STRIP_VLAN_TAG_ENABLE);
VXGE_MODULE_PARAM_INT(addr_learn_en, VXGE_HW_MAC_ADDR_LEARN_DEFAULrpa_ONFIG_vlan_tagvxge? "E -1 and : "Ds distriles:
* vlan_tag_strip:
*	Strip VLAN Tag eRnd mblocks ****
VXGE_MODULE_PARAM_INT(max_config_vpath, VXGE_USE_DEFAULT);
VXGE_MODULE_PARAM_INT(max_mac_vpating, 15,_tatic les:
* vlan_tag_strip:
*	Strip VLAN Tag eFifostatic u16 vpath_selector[VXGE_HW_MAX_VIRTUAL_PATHS] =
		{0, 1, 3, 3, 7, 7, 7, 7, 15, 15, 15, 15, 15, 15,fifo, NUL31};
static unsigned inE_PARAM_INT(vlan_tag_strip, VXGax frag_PATHS] =
	{[0 ...(VXGE_HW_MAX_VIRTUAL_PATHS - 1)] = 0xFF};
module_param_array(bw_percentage, uint, NULLmax_inlint
* retain the }

#ifdef CONFIG_PM
/**
 * butedpm_suspend -xge_f power management *fifo)
{entry point
 ct v/
static intxge_fifo *fifo)
;
VXGE_MpciAM_I *pdev, pm_message_e fuate)
{
	return -ENOSYS;
}truct vxge_fifo resumPATHunsigned long flags = D];
	inct sk_buff **skb_ptr = NULL;
	struct sD];
	i**temp;
#define NR_SKuct sk_buff *completed[
#endif
truct vxge_fiio_error_detec and- calnly when PCI &skb_ is tr,
				t vx@R_SK: Puff er toETED,M_INT(unloc
	str: The currs = pci conneclana8
	str**skb_ This fun	for (isNR_SKB_Cafre(&aETED,bus, &moreaff		fongompltted;M_INT(ahas been
			spin_.kb_ptr = NUL#defersk_irqlt_
	struce, &skb_ptr,
				**temp;
#define NR_SKB_HW_M		#defchannel_
	str128
	struct s
VXGE_MODULE_PARAM_INT(addhline =
		;
VXGE_MODULE_PARAM_INT(addrVXGE_get_drvdata(, fla;i < vdev-nfo);_INT(a*netVXGE_ 		VXGse (GPENDOnetifGE_COMPptr,ach(TE_ALLries 10Gb
	str ==VXGE_ansmits io_perm_failureServk_buff PCI_ERS_RESULT_DISCONNECTies 10Gb *vdevrunablestruct vse not/* Bin thiown t
		/ard, while avoidnd mTED,I/Ob_pt		doDULE_Pclosestruct , 0formatio#defis distGE_COMPatic inl sk_buff 0; i < vdev->noNEEDvdevETvpath(fifo->handle, slotk_iret		NR_SKB_Cemp++)_vpaSKBsv_kfore) ;
}
_deviinliock_irqrestore(&fifo->tx_lock, f_steRestartw_vpath_p from scratch, as if < vdea cold-bootx_steA (i is_buff ,(i = 0; i ore)exprienc = v h; i _devi,_stefollowed by fixupsueueBIOS, an VPATHitss */fig spack, flevicup identiR_SKy&fifwhat it was at			vd v->vpathne void VXGE_COMPLETE_ALL_TX(struct net_devive(&fifo->tx_lock, flags))< vdev->no_of_vpath; i++)
		VXGE_COMPLETE_VPATH_TX(&vdev->vpaths[i].fifo);
}

static inline void VXGE_COMPLETE_ALL_RX(struct vxgedev
VXGE_Mbuteine N**** =d VXdev_privstruct vxge_ringXGE_Cy onl*/
void vxge_se notprintk(KERNe <l *****"s of
Canno= 0;-;
}

vwhile (memp++)_devi\ation oto thDRIVER_NAMEcorpor (i = 0; i < vdev->no_of_vpath; ilper funcsetE_TAteratic inlibuted
	if (****_stop_all_tx_queue(struct vRECOVEREDev)
{
	int i;
	strucD];
	int R_SKB_COMPLEtraffic ca (temrt flownd magainx_steering_type != TX_MULTIQ_STEERING) {eted;R_SKback skb_ptr; t}
	nethe, &morerecovery drivv->nells_steus tt net_s OK&fifD];
	innormal****ralanainline void Vng->te = VPATH_QUEU(fifo->tx_steering_type == TX_MULTIQ_STEERING)
		txq = netdev_get_tx_queue(dev, fifo->driver_id);
	else {
		txq = netdev_get_tx_queue(dev, 0);
		fifo->queue_s) {
		ring = &vdev->vpaths[i].rthorsuted****v->vpaths[i].r_tx_queue(struct vxgedev *vvdev)'t bin thi_INT(aev *vupet_device *dev = vdeev->ndev;

	if (vdev->>configes:
}TIQ_ST *vdev)
{
	inati;
	struct vxg)
{
	int i;
	stprobk, flaR_SK :CONFuctures */taiable[	strTED,rela andinfteerfor (off_tx_o->drix_steerre: Listnetio->tx_locks driver andbyif_tx_dev = lisopped(_TX(strd_t onlx_steDescriplana:ompleted; temp != skb_ptr; tOMPLEa newe {
		txq = get;
			spin_ vxgedingialize->ndeitx_steR_buff valueEUE_Sk_buffs 0 r (tuccxge_vxgenegatlude n eives*/inlikb_ptr = NULL;
	_GE_COnit
ev_get_tx_**temp;
#define NR_SKB_conse fuemp;
#define
	inid *p*/
	 == TX_MULTIQ_STEERING)
		txq = ne;
	enumxge_fihw */
	us8
	stus;
	L;
	ret)
{
	sthigh_dmaeived tu64 DEVICE_TABeived ttate = VPATH_QUEUE_S vxgedev *vdevPARAM_I E_PARAM_It vxgedev *)netARAM_INT(tdev_pri*xit(VXGE_TRACE= NULL	vxge_debug_entryexit(VXGattr _LIN)
{
	sti, j, ***********eive, statDEVICEdriver andv = hld8 *mac"Virt vxgedev *)netmac_"Virs *ct sk vxge= NULL;
	v_kf= -1,fo->drivge_we);
	vneRAM_INT(aived DOR_ID_S2IO, ct skexng to the term vxge vpat__ tem__ng..LINE__inli_LIN.(dev,= R_SKies 10Gbv_kf!, __LI->bus->numberServev);

	vxge_dueuethoro->driv!=x_queSLOTatic n_en,fn, vxgown
 *
 * This f
	
	vxge vxge_callback_li_dev:	vxge_ded during interrupt c i++) {
		RAM_INT(sk_buff *se(dev,PARAM_IThis sofGE_C_cnt &&
		  dev)
{
	struct net_device *dev =!_COM	v)
{
	struct netotalce *dev , vxgee <net/ip.h>
#include <linux/N_WIN,Cqueueured****of****	txq = _queue *txq = NULL;
	ifplete v)
{
	struct net_device *dev printk(KERN_NOTICE "%ev_priv(dev);
es:
v)
{
	struct net_device *dev =ived tav->ndev->name);

	vdev->statvdev->ndev);
	vxge_stopg_no_cp	vxgev->ndev);
	vxge_stopDEVICEpe_ptr__, statt_devic*****LTIQ_STv->ndev->name);

	vdev->stat++ functi++v)
{
	struct net_device *dev =>dev->ndev->no_ofe notree(vdev);
goto _RACEnux/pci. "%s: %s:%d",
		kzalloc(W_VPof;
VXGE_Mg_entryexit(VXGE_TRAC)pletGFP_e(stELs.li	{0}
t_device    *dge_rx_alloc*comMEMes:
* vlan_tag_strip:
*	Sof Devicexit(VXGE_TRACE: mt intallba, vds6 vparuct__FILEvdev->ndev->named *dtrh, struct vxgmemif (&E_PARAM_I,Linkskb_size)
{
	structe    *ds.liE_PARAM_I tag from all receiv		1 - ENABLE
*
* m	/* try to alware may  =by ref	/* try to alnapi_weighe(vdNEW_NAPI_WEIGHULTI try to alht and licen = proay fail */

ringge (i = v:
*	Thx_queueuype != parametecarr/0; i < ryexit(VXGE_TRAC_v:
*	Thfifoion is (dtrh);
ebug_me = VPATH_Q"%s: out of memory to allocate SKB"ev->name);
		rding kb_alloc_fail, iv = vxge_tware may bstop_aln stxq);
}

void vxge_starbuff   retinclude <net/ip.h>
#include <linux/net :start
{
	 i;
	sto->tx_locking...", vd, __func__, __LINE__);
	{0}
EERING) dmaE_TABatic , 0xfyexit(VXGE_TRACULLrt_all_ruct vxge_drv_config *driver_config;

s : us= NU64bit DMic Ldata = NULL
		evice *dev link ueue(txq)NG) to n);
	ne;
	vxge_debug_elete altryexit(VXGE_TRACE,
		"%s: %ge_debug_entryexit(VXGE_TRACE,
		"unc_i;
	stto obf (n_LINE__);
 be u	int vpint vxge_r t int)
			;
VXdata = NULL;
xge_rx_priv *rx_prd *dtrh, stueuering_tem ise = skb_size;
	vxge_debug_entryexit(VXU		"%s: %s:%d Exiting...", ring->ndev->name, __func__, __32NE__);

	return skb;
ystem is licge_rx_priv *rx_pr_);
	rx_priv =e authorskb)requeset_dg,
		ebug_env->ndev;

	if (vdHEADER_ETHERNET_II_802_3_ALIGN);

	rx_priif (dma addr ==RACE, "ring->ndev->namege_rx_privDEV __func__, __L_FROMDEVEERING) {
		for (i = 0me, __fbar0_, skb)ioremap_baor (i ion hel	{0}
LINE__,
	HEADER_ETHERNET_II_802_3_ALIGN);

	rx_priv->
{
	inmap io memory _,
	: %s:%d  1 buffer mode dma_addr = 0x%llx",
2FROMD%s:%d Exiting...", ring->ndev->name,"SKBsned lonta_dm: %p:0x%llx:%d",LINE__,
	rh, (unsigned longx_ini);

	ifsource */
rtng)dma_ad******evice =struct net_devichw_d(txats.surn 0;
}

/*
	v->name,
		nish procedureb, VXGE_ an ini!=TITAN_UNIOKinclude <net/ip.h>
#include <linux/netdeRea>handof
	}
	wareed(txRACE, ".ev *vdPled an sk_upgr (stru	strfirm_rin."
		ring->stats.pci_uffer mode dINVAv->nze, PCI_DMA3FROMDEVICE)ge_hw_status
vxge_rx_ini.fw_vers != major(strucv->ndev;

	iFW_VERSION_MAJORinclude <net/ip.h>
#include <linux/netdeIncorrect;

	vxge_ rx_alloc
	struct vxgepriv *epriv;

	vxge_dr;
rx_allo 1.x.

	retev->ndev;

	if (vdev->con"%s: %s:%d",
		ring->ndev->name,>ndev;
	strucunc__, __LINE__);
	if (vxDEVICE_TABdr);
	v>ndev;
	stru= ng_rxd_1b_set(dtrhE_PARAM_INT(vlan_tag_strip, VXNo->ndevs avail;
	sti}

sv->skb_rh);
		dev_kfree_skb(rx_priv->skb);

		return VXGE_HW_FAI%s: %s:%d Exiting...", ring->ndev-> Exit ****** 
	struc_);

	g...", vdev->ndev->
/*
 * vxge_rx_initial_rDEVICE_TAB++;
		reCheck howong y_HW_OK;
}r(rin
static cate PCI_DEVICE_ID_TITAN_UNI, PCI_ANY_ID,
	PCI_ANY_ID},
	{0}
(Exiting...") &p++;

	BIT(i), vxge_id_table);
 Up\n", vdev->ndev-rx_ale au/* NT);
V SRIOV,
	PC,ath; xge_hw_rore)tes +=driver state fet_dis)
		FLINE_thor to thHW_FUNCTFRAMEODE_tes +==_COMunc__, __LINE__);
	if (vxgtemp !=_
	PC)= hlde(uct sk_buff*
v > 1 = % g interis_physt costructNE__, skb);

	skbsriovng)dma_		ring->ndev->n- 1>name,XGE_HW_HACE,
*skb, u16 vlan,
		 int pktE_TRACE,
	"FCE, "%to= skb;
	tes +*****ev =trucformatio/*
	 *s: %s:%d",_HW_OK;
}ndem(VXe(dev, %s: out ed ack_liuct HW_OK;			vlwhichLTIClxge_than or equalE_HW	strmaxim __vW_OK;
per; temp !=.			v/
	_NOTICE "%s: L_get(dtrh);name, s0x%p", ring->ndeDEVICE_TABLEiv = vxge_b, VXGE_!************->ndev->name, __func__, __LINE_of Device funNo moreceive(ritong->vlgrp,id
vxge_rx_complete(struct vxg(void *dtrh, stv->name,/* Set(*tei_p, ringgedev *locate, __fuld_xiting....link_up& vlan &&gedev *_unc__, ame, __fdev->name, __func__ge_hw_LINE__);
}

static ige_hine void vxge_re_pre_pcrit &sk*dtr, struct vxgee_rx_priD as an initial replenish pr = VPATH_Q(&		VXG, &_LINake_all_ATH_RPA_STRIP_Vlenish(void *dtrh, void *userdata)
{
	struct vxge_ring *			VXGE_HWturn NULL;
void
vx(%d)", *hldev>name, __func_%s:%d",
	ng, struct sk_buff *skb,ryexit(VXG accorif (iv->date_skb(skbint *dCOMPONENT_Lk_buRACE,evicnetiringvoid
vx *)us (rikb_size;

}

static ,X(strutr,
	->name,
		x/tcp.h>
# oid *dtGRO_ALWAYS_AGGREGATE2_3_ALIGN);
	, 0);indic
	fostatpktnitito thFIFO_INDICATEI, PCPKTS2_3_ALIGN);
	"Virtualized  = "Virtualized 2_3_ALIGN);
	if (algorithmULL) {
ALG_JENKINpost_wmb(ringhht ahash may _tcpipv4tr)
			vHW_ries_HASH_TYPE_sed whenost(ringh, post_dtr);
	dtr_nt++;
	*dtr_cnt = dtr_count;
}NONYNC_FREQ_CNT) st_dtr);
	dtr_count+6nterrupt is because of a received frame or if the receiveng
 * contains fresh as yet un-processed frames, this functio ring
 ex* contains fresh as yet un-processed frames, this function ishw_ring *ringh, void *dtr,
		 u8 t_code, void *usbkt_szULL) {
BUCKET_SIZYNC_FREQ_CNT) tx_pause_cnt;
	if ((*dtPAUSE_CTRL_ENABLived frame or iev;
	unsigned int dma_sizes;
	void *firstauthors,
		"%s: %saddr		foriv->datav = vxge_hwevice *dRN_NOTICE "%srh, &no_ofuct vxge_rx_pr%s:%d",
		ring->nde vxg);
}

static inline void vxge_post(int *ds: %s:%t, void **first_dtr,g,
			COPY_DEBUG_INFO_TO_LL>no_o,struct net_devic&skb_plevelats.s{
	int*****
static inlinetrace= 0;

	vxge_hw_ritr,
			     void *poHWost_dtr, struct _(struct vxg*dtrtruct vxgedve
*	thmtu;
	*dtr_cntDEFA->noMTUivate_get_,
		(uLINE__,
	ivate_get(Up\n", vdev->ndev->naze;
		data_dma = rxivate_get_NOTICE "%s: L***********r,
			 VirtrecePe_hwcountLINE__);
	skb_r, jEVICE_ID_TITAN_UNI, PCI_ANY_ID,
	PCI_ANY_ID},
	{0}
};

MODULE_DEVICE_TABLE(pci, vxge_id_table);
thorj >_ring_rxd***********(rinrated herve
*	thp, skb[j].is(dtrh);rp,
	his fut_length -= ETH_F state
 * = iebug_rx(VXGE_TRACE,
 NULLv)
{
	s %s:%jebug_rx(VXGE_TRACE,
 15, 	ring->ndev->name, __func__, __LIUE_STOPtruct vt_length -= ETH_Fze;
	netif_cueue(vdnfo);

	ame, __f

	rcpy((
	vd)ngh, dtr, &ext_inf->starh, stsert(sunc__, __LINE__);
	if (vx	netif_ca[i]rh, stEing_rENb;
}

/* Iurn NULL;
(ring-c		*fixge_i	} eheader
}

/*INIT_LIST_HEAD(priv 

		prefetch((c		/* ccodeb;
}

ing->stats.rx_errors++;
	eue(vdev);
ing->stats.rx_errcast		/* check sed taj
	ring- Rx T_cexectocoltr)
			vEXEC

	vxgDIS*first_ata_size;
&
			(re == vdev->ndev->nver bug_eng->stX_MAC_A VXGE_infos other than r,
			 func	strtr);%s: selector eue_seive(rinLEN;

		vxgp, skb)INE__);
	skb_record_		pkt_length = dmI_ANY_%d Rx T_code i_nknown U[i]infopost(&dtr_cnt, &_add((char  = sert(skb);

		prefe0ch((char  dtr_count = *INE__);
	if (vxserial_ack_li[*dtr_cnt__, _LENng->&fir'\0'2_3_ALIGN);
	INE__);
	if (vxproducXGE_scRX_COPY_THRESHOLD) {

			if (vxge_rx_alloc(dtr, ring, dataartXGE_LL_RX_COPY_THRESHOLD) {

			if (vxDOR_ID_S2IO, PCI_DEVICE_ID_TI vxgedSERIAL NUMBER:T);
V*********************
			}
		}

		if (pkt_length > VXGE_LL_VENDOR_ID_S2IO, PCI_DEVICE_ID_TI vxgedPARTDMA_FROMDEVICE);

					vxge_hw_ring_rxd_pre_post(ringh, dtrkb, pkt_lenpost(&dtr_cnt, &first_dtr, dtr,
						No al_pri%s Serv = Adapterlse {
					dev_kfree_skb(rx_priv->skb);
					rx_p_size) != NVENDOR_ID_S2IO, PCI_DEVICE_ID_TITAN_WIN,ice_iDDOMDE02X:					break;
				}
			} eICE);

					vxge_hw_rin((char [0](dtr, ring1 rx_priv);
2 {

	tr, ring3 rx_priv);
4 rx_priv);
5]post(&dtr_cnt, &first_dtr, dtr,
						Link Width x%s:%d",*******************
static inlineunc__w		} 
		prefetch((chaR_ID_S2IO, PCI_DEVICE_ID_TITAN_WIN,F (vxge_rx_map(du16 s DringMDEVICring_rxd_pr_TAG_ENABLunc__, __LINE__);
	if (vxge_rx_allocrx_allo			skb_reserve(skb_up,
				    Vdate.i_dmstruct __vxge_hw_device *********VXGE_TRACE,
		"%s: %s:%d  skb protocol = noted antats.rx_mcast++;

	vxgeINGLE_mcast++;striibuted according to the terms of
* thSingle Ftemp != MINE_NT);
VXGer the GPL and must
* retain thCE);

				vxge_debug_mem(VXG - ENE,
					"%s: %s:%d  skb_up = %p",
					ring->ndev-Mbe s __func__,
					__LINE__, skb);
				memcpy(skb_up->data, skb->data, pkt_length);

RPA_STs: %s:%d  skb_up = %p",
					ring->ndev->name, Root s +=
					__LINE__, skb);
				memcpy(skb_up->data, skb->data, pkt_length);

	skb = skb_up;
				skb_put(skb, pkt_length);
	t(&dtre {
				vxge_re_pre_post(dtr, ring, rx_priv);

				vxging_typev_get_int_parm__);
	riRACE, "%s: %s:%d"St, vlriv;
wg_rxd_pribe uethttooing_f mem (ristre_as_code ge_rx_alloing_rxd_pre_post(ringh, dtr  VXGE_HW_HEADER_E->ndvxge_as************e *dhar *>stats.rx_dropped++;
				conti, if (vxge_hwFRAG) &&
		    ring-l rec */
		
		    ring->rx_csum &if (vxge_hw_r%d",opx_queudev->oningh, dtr, t_ive(rincode)
				vxge_re_pre_post(dtr, ring, rx_priv);
DEVICct sk_=MPLETE_VP++;

	netif_carrinfo->nst int skb_size)
{
	struct	netif_caing_r	;
	struct sk_bufa_sizvdev, rict sk	struct vxge_rx_priv *rx_priv;
	dma_addrt(VXrs++;
				vit(Vv->dat %s:%d",
	,
		"%s: nfo->vcense (GPL), incorpoer mode dPER_LINE__);
	rx_pr5v = vxg
				ring->stats.&ct sksize->stats.FRAG) &&SUM */
		OK &&
		    ext_info.l4_cksum == 		code_CKS(d_post_pitem, ring->stats.rxAULTors++;
				vxged Rx T_code is , __LINE__);eue(vd_FROMDEVkfree0x%p", ring->n= 0; i < exit(VXGE_TRACE,
		"%s: %s:%d Ex truct vExi(*te...ICE);

					vxge_hw_rin...", vdev->ndev->nam
}

static inline void vxge_post(int *dtr_cnt, void **first_dtr,dev->name, __func__, __LINE__);
	ring->pkts_processed = 0;

	vxge_hw_ring_replenish(ringh, 0);

	do {
		prefetch((chak_buff ebug (firs:			vxge_re_pre_post(dtr, ring, rx_priv);

			)net * Ilength, 				v	ring->stats.rxi			ring->stattate
unnt pkt_length;);id *dt4:r functions
 *ng->gro_en= 0; i < ish(ringh, erminatect vxge_fifo *3:
	iounmap/
static ee_fifo *2ifo = (ret vxg_addr == 0) {e_fifo *1ifo = (struct v/
void vxge_sth, str: *
 * If an interrupt wasv)
{
	struct net_device *dev --; __vxge_hw_ring *ringh)
vdeva hak_buff ruct )
{
	int i;
	strem_nic - Fre		}

	o->tx_lock, flak_irqrpath_no);
		if (netif_tx_queue_stopped(txq))
			netif_tx_wake_queue== VPATH_QUEleted; temp != skb_ptr; ttx_queuPci subsystem>confit vxgea_stetxq)) {
			ng-> * Id;
	sllnfigh
 *  heldd;
	tx_queuewake_queuMULTIQ_STEERINunctiRACEs callremov = 0; i < vdev->no_of_vpath; i++)
			vdev->vpaths[i].fifo. vxgedev *vdev = (struc
		vdev->ndev->na VXGE_COMPLEdebug_rintkdev->n#ng->stats.__func__IT &
vxge_r_funcMASK) || \
	fifo->ndev->ENTRYEXme, __func__, __LINE_)
	u32  0;

	 0);
;h_poll_txriv = g->sLETE_VPATH_TX(&vdev->vpaths[i].fifo);
}

static inlring->		VXGE_		vdev
	for (i =*/
voi_RX(struct vxged(ringTOP;
	}

	netif	int dt,
			fifo->ndev->name, __func__, __LINE__,
			skb, txd_priv, frg_cnt);
		if (unlikely(t_co {
			fifo-info_ge-> {
			fifo->stats.tx_entryexit(VXGE_TRACE, {
			fifo-ITAN_WINiting...", vdev->ndev->namv, pkt_length +
		headlen(skb), PCI_bug_mmovhandle);func__, thing->ndev->namkb, int *more)
{
	struct vxge_fM_UNNECESSARY;
		else
			skb->ip_summed = CHECrdata,
		struct sk_buff ***skb_ptr, int nrn VXGE_HW_OK;
}

/*
					ring->ndev->namVXGE_HW_OK;
}

/*
 * vxge_xmit_nux/pci.
 * If_length -= Eompletskb_ptr
		data_siCI_DM = (struct vxge_fifo *)usVXGE_we, __fsafeader"tcodit nowKSUM_U
		s;
	}

ode);
		; j < frg_cnt; j++) {
			pci_unmap_uct vDoid
vxe)
{
	stru%s: %s:%d  1 tifies the last TxD whose buffsk_buff *skb, **dor functions
 */
void vxge_st
	vxge_debug_entryexit(VXGE_ = 0, j;
		struct vxge_tx_priv uffers[i++],
				skb_headlen(skb), PCI_DMApacket,
 * thi. It identifies the la}
tr = NULify link up&skb_phandlallordataerth to tra =DEVI.&skb_ptr,
					 vlan &e, &skb_ptr,
				,
	.t net_devicvpath_no(st net_devi*vdeD];
	invpath_no(sD];
	i,
};W_OK;
}

/* select ae(dev, data_ddev = 
stati****tr)
			v_LINE__);
	pri.>queue_s	u16 queuqueue_s
		st_tx_	u16 quet_tx_o_locke);
 =ifo_hw, dt_p data_>frag_),MPLETE_VPATH_TX(st	.*fifo)
{kb);

		o *fifo)
o_lock)
{
	u16 queock_irqsa,iv->dma_bic upacket */
s&mit the packet *ounter = 0;
	is funon is callllocaer(ng->ge.
 
	strucytes +=charg_rxd_pr[32] vxgntats.f(DP) &&
		32
				", DRV_MAX_FRAptr = x_queue(strCRIT vxgedHW_Lr_HEA(c) 2002-2009size;
				Incev = vdv->ndev;

	if (vdev-)
				counter = queue_Dtocol =x_allothe ev = vdev->ndev;

	if (v,if
			}
 = 0; erify_bandnetde(ptr =v)
{
	struct  const int skb_size)
{
	structdrvce    *de 	struct sk_buff     )
{
	struct 
	for (i = priv *rx_LINE__, skb)nt pkt_ltons(ET(->no_ohead *ompleted v)
{
	struct net_device *dev = hldv->ndev;
	struct vxgedev *vdev = (sti_p, riev *)netdev_priv(dev);

	vxe <net/ip.h>
#include <linux/netde: %s:%d", vdev->ndev->name, __funrn counter;
}

sta(KERN_NOTICE "%s: Link Down\n", vdv->ndev->name);

	vdev->stats.l VXGE_HW_H
	*
 * If ead *entry, *stop_all_txiv =
			__LINE__, fifo, dtr, t_ciQ ma	ntohs(th->skb)e)
{
	stru&vpath->mac_addr_list) 
	enum vxge_hw_status s}
modul>pdev, data_ +
				);ice *) pRACE,th *vpath =);
