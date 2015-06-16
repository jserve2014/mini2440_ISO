/*
 * Copyright (C) 2006-2009 Freescale Semicondutor, Inc. All rights reserved.
 *
 * Author: Shlomi Gridish <gridish@freescale.com>
 *	   Li Yang <leoli@freescale.com>
 *
 * Description:
 * QE UCC Gigabit Ethernet Driver
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/stddef.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include <linux/mii.h>
#include <linux/phy.h>
#include <linux/workqueue.h>
#include <linux/of_mdio.h>
#include <linux/of_platform.h>

#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/immap_qe.h>
#include <asm/qe.h>
#include <asm/ucc.h>
#include <asm/ucc_fast.h>

#include "ucc_geth.h"
#include "fsl_pq_mdio.h"

#undef DEBUG

#define ugeth_printk(level, format, arg...)  \
        printk(level format "\n", ## arg)

#define ugeth_dbg(format, arg...)            \
        ugeth_printk(KERN_DEBUG , format , ## arg)
#define ugeth_err(format, arg...)            \
        ugeth_printk(KERN_ERR , format , ## arg)
#define ugeth_info(format, arg...)           \
        ugeth_printk(KERN_INFO , format , ## arg)
#define ugeth_warn(format, arg...)           \
        ugeth_printk(KERN_WARNING , format , ## arg)

#ifdef UGETH_VERBOSE_DEBUG
#define ugeth_vdbg ugeth_dbg
#else
#define ugeth_vdbg(fmt, args...) do { } while (0)
#endif				/* UGETH_VERBOSE_DEBUG */
#define UGETH_MSG_DEFAULT	(NETIF_MSG_IFUP << 1 ) - 1


static DEFINE_SPINLOCK(ugeth_lock);

static struct {
	u32 msg_enable;
} debug = { -1 };

module_param_named(debug, debug.msg_enable, int, 0);
MODULE_PARM_DESC(debug, "Debug verbosity level (0=none, ..., 0xffff=all)");

static struct ucc_geth_info ugeth_primary_info = {
	.uf_info = {
		    .bd_mem_part = MEM_PART_SYSTEM,
		    .rtsm = UCC_FAST_SEND_IDLES_BETWEEN_FRAMES,
		    .max_rx_buf_length = 1536,
		    /* adjusted at startup if max-speed 1000 */
		    .urfs = UCC_GETH_URFS_INIT,
		    .urfet = UCC_GETH_URFET_INIT,
		    .urfset = UCC_GETH_URFSET_INIT,
		    .utfs = UCC_GETH_UTFS_INIT,
		    .utfet = UCC_GETH_UTFET_INIT,
		    .utftt = UCC_GETH_UTFTT_INIT,
		    .ufpt = 256,
		    .mode = UCC_FAST_PROTOCOL_MODE_ETHERNET,
		    .ttx_trx = UCC_FAST_GUMR_TRANSPARENT_TTX_TRX_NORMAL,
		    .tenc = UCC_FAST_TX_ENCODING_NRZ,
		    .renc = UCC_FAST_RX_ENCODING_NRZ,
		    .tcrc = UCC_FAST_16_BIT_CRC,
		    .synl = UCC_FAST_SYNC_LEN_NOT_USED,
		    },
	.numQueuesTx = 1,
	.numQueuesRx = 1,
	.extendedFilteringChainPointer = ((uint32_t) NULL),
	.typeorlen = 3072 /*1536 */ ,
	.nonBackToBackIfgPart1 = 0x40,
	.nonBackToBackIfgPart2 = 0x60,
	.miminumInterFrameGapEnforcement = 0x50,
	.backToBackInterFrameGap = 0x60,
	.mblinterval = 128,
	.nortsrbytetime = 5,
	.fracsiz = 1,
	.strictpriorityq = 0xff,
	.altBebTruncation = 0xa,
	.excessDefer = 1,
	.maxRetransmission = 0xf,
	.collisionWindow = 0x37,
	.receiveFlowControl = 1,
	.transmitFlowControl = 1,
	.maxGroupAddrInHash = 4,
	.maxIndAddrInHash = 4,
	.prel = 7,
	.maxFrameLength = 1518,
	.minFrameLength = 64,
	.maxD1Length = 1520,
	.maxD2Length = 1520,
	.vlantype = 0x8100,
	.ecamptr = ((uint32_t) NULL),
	.eventRegMask = UCCE_OTHER,
	.pausePeriod = 0xf000,
	.interruptcoalescingmaxvalue = {1, 1, 1, 1, 1, 1, 1, 1},
	.bdRingLenTx = {
			TX_BD_RING_LEN,
			TX_BD_RING_LEN,
			TX_BD_RING_LEN,
			TX_BD_RING_LEN,
			TX_BD_RING_LEN,
			TX_BD_RING_LEN,
			TX_BD_RING_LEN,
			TX_BD_RING_LEN},

	.bdRingLenRx = {
			RX_BD_RING_LEN,
			RX_BD_RING_LEN,
			RX_BD_RING_LEN,
			RX_BD_RING_LEN,
			RX_BD_RING_LEN,
			RX_BD_RING_LEN,
			RX_BD_RING_LEN,
			RX_BD_RING_LEN},

	.numStationAddresses = UCC_GETH_NUM_OF_STATION_ADDRESSES_1,
	.largestexternallookupkeysize =
	    QE_FLTR_LARGEST_EXTERNAL_TABLE_LOOKUP_KEY_SIZE_NONE,
	.statisticsMode = UCC_GETH_STATISTICS_GATHERING_MODE_HARDWARE |
		UCC_GETH_STATISTICS_GATHERING_MODE_FIRMWARE_TX |
		UCC_GETH_STATISTICS_GATHERING_MODE_FIRMWARE_RX,
	.vlanOperationTagged = UCC_GETH_VLAN_OPERATION_TAGGED_NOP,
	.vlanOperationNonTagged = UCC_GETH_VLAN_OPERATION_NON_TAGGED_NOP,
	.rxQoSMode = UCC_GETH_QOS_MODE_DEFAULT,
	.aufc = UPSMR_AUTOMATIC_FLOW_CONTROL_MODE_NONE,
	.padAndCrc = MACCFG2_PAD_AND_CRC_MODE_PAD_AND_CRC,
	.numThreadsTx = UCC_GETH_NUM_OF_THREADS_1,
	.numThreadsRx = UCC_GETH_NUM_OF_THREADS_1,
	.riscTx = QE_RISC_ALLOCATION_RISC1_AND_RISC2,
	.riscRx = QE_RISC_ALLOCATION_RISC1_AND_RISC2,
};

static struct ucc_geth_info ugeth_info[8];

#ifdef DEBUG
static void mem_disp(u8 *addr, int size)
{
	u8 *i;
	int size16Aling = (size >> 4) << 4;
	int size4Aling = (size >> 2) << 2;
	int notAlign = 0;
	if (size % 16)
		notAlign = 1;

	for (i = addr; (u32) i < (u32) addr + size16Aling; i += 16)
		printk("0x%08x: %08x %08x %08x %08x\r\n",
		       (u32) i,
		       *((u32 *) (i)),
		       *((u32 *) (i + 4)),
		       *((u32 *) (i + 8)), *((u32 *) (i + 12)));
	if (notAlign == 1)
		printk("0x%08x: ", (u32) i);
	for (; (u32) i < (u32) addr + size4Aling; i += 4)
		printk("%08x ", *((u32 *) (i)));
	for (; (u32) i < (u32) addr + size; i++)
		printk("%02x", *((u8 *) (i)));
	if (notAlign == 1)
		printk("\r\n");
}
#endif /* DEBUG */

static struct list_head *dequeue(struct list_head *lh)
{
	unsigned long flags;

	spin_lock_irqsave(&ugeth_lock, flags);
	if (!list_empty(lh)) {
		struct list_head *node = lh->next;
		list_del(node);
		spin_unlock_irqrestore(&ugeth_lock, flags);
		return node;
	} else {
		spin_unlock_irqrestore(&ugeth_lock, flags);
		return NULL;
	}
}

static struct sk_buff *get_new_skb(struct ucc_geth_private *ugeth,
		u8 __iomem *bd)
{
	struct sk_buff *skb = NULL;

	skb = __skb_dequeue(&ugeth->rx_recycle);
	if (!skb)
		skb = dev_alloc_skb(ugeth->ug_info->uf_info.max_rx_buf_length +
				    UCC_GETH_RX_DATA_BUF_ALIGNMENT);
	if (skb == NULL)
		return NULL;

	/* We need the data buffer to be aligned properly.  We will reserve
	 * as many bytes as needed to align the data properly
	 */
	skb_reserve(skb,
		    UCC_GETH_RX_DATA_BUF_ALIGNMENT -
		    (((unsigned)skb->data) & (UCC_GETH_RX_DATA_BUF_ALIGNMENT -
					      1)));

	skb->dev = ugeth->ndev;

	out_be32(&((struct qe_bd __iomem *)bd)->buf,
		      dma_map_single(ugeth->dev,
				     skb->data,
				     ugeth->ug_info->uf_info.max_rx_buf_length +
				     UCC_GETH_RX_DATA_BUF_ALIGNMENT,
				     DMA_FROM_DEVICE));

	out_be32((u32 __iomem *)bd,
			(R_E | R_I | (in_be32((u32 __iomem*)bd) & R_W)));

	return skb;
}

static int rx_bd_buffer_set(struct ucc_geth_private *ugeth, u8 rxQ)
{
	u8 __iomem *bd;
	u32 bd_status;
	struct sk_buff *skb;
	int i;

	bd = ugeth->p_rx_bd_ring[rxQ];
	i = 0;

	do {
		bd_status = in_be32((u32 __iomem *)bd);
		skb = get_new_skb(ugeth, bd);

		if (!skb)	/* If can not allocate data buffer,
				abort. Cleanup will be elsewhere */
			return -ENOMEM;

		ugeth->rx_skbuff[rxQ][i] = skb;

		/* advance the BD pointer */
		bd += sizeof(struct qe_bd);
		i++;
	} while (!(bd_status & R_W));

	return 0;
}

static int fill_init_enet_entries(struct ucc_geth_private *ugeth,
				  u32 *p_start,
				  u8 num_entries,
				  u32 thread_size,
				  u32 thread_alignment,
				  unsigned int risc,
				  int skip_page_for_first_entry)
{
	u32 init_enet_offset;
	u8 i;
	int snum;

	for (i = 0; i < num_entries; i++) {
		if ((snum = qe_get_snum()) < 0) {
			if (netif_msg_ifup(ugeth))
				ugeth_err("fill_init_enet_entries: Can not get SNUM.");
			return snum;
		}
		if ((i == 0) && skip_page_for_first_entry)
		/* First entry of Rx does not have page */
			init_enet_offset = 0;
		else {
			init_enet_offset =
			    qe_muram_alloc(thread_size, thread_alignment);
			if (IS_ERR_VALUE(init_enet_offset)) {
				if (netif_msg_ifup(ugeth))
					ugeth_err("fill_init_enet_entries: Can not allocate DPRAM memory.");
				qe_put_snum((u8) snum);
				return -ENOMEM;
			}
		}
		*(p_start++) =
		    ((u8) snum << ENET_INIT_PARAM_SNUM_SHIFT) | init_enet_offset
		    | risc;
	}

	return 0;
}

static int return_init_enet_entries(struct ucc_geth_private *ugeth,
				    u32 *p_start,
				    u8 num_entries,
				    unsigned int risc,
				    int skip_page_for_first_entry)
{
	u32 init_enet_offset;
	u8 i;
	int snum;

	for (i = 0; i < num_entries; i++) {
		u32 val = *p_start;

		/* Check that this entry was actually valid --
		needed in case failed in allocations */
		if ((val & ENET_INIT_PARAM_RISC_MASK) == risc) {
			snum =
			    (u32) (val & ENET_INIT_PARAM_SNUM_MASK) >>
			    ENET_INIT_PARAM_SNUM_SHIFT;
			qe_put_snum((u8) snum);
			if (!((i == 0) && skip_page_for_first_entry)) {
			/* First entry of Rx does not have page */
				init_enet_offset =
				    (val & ENET_INIT_PARAM_PTR_MASK);
				qe_muram_free(init_enet_offset);
			}
			*p_start++ = 0;
		}
	}

	return 0;
}

#ifdef DEBUG
static int dump_init_enet_entries(struct ucc_geth_private *ugeth,
				  u32 __iomem *p_start,
				  u8 num_entries,
				  u32 thread_size,
				  unsigned int risc,
				  int skip_page_for_first_entry)
{
	u32 init_enet_offset;
	u8 i;
	int snum;

	for (i = 0; i < num_entries; i++) {
		u32 val = in_be32(p_start);

		/* Check that this entry was actually valid --
		needed in case failed in allocations */
		if ((val & ENET_INIT_PARAM_RISC_MASK) == risc) {
			snum =
			    (u32) (val & ENET_INIT_PARAM_SNUM_MASK) >>
			    ENET_INIT_PARAM_SNUM_SHIFT;
			qe_put_snum((u8) snum);
			if (!((i == 0) && skip_page_for_first_entry)) {
			/* First entry of Rx does not have page */
				init_enet_offset =
				    (in_be32(p_start) &
				     ENET_INIT_PARAM_PTR_MASK);
				ugeth_info("Init enet entry %d:", i);
				ugeth_info("Base address: 0x%08x",
					   (u32)
					   qe_muram_addr(init_enet_offset));
				mem_disp(qe_muram_addr(init_enet_offset),
					 thread_size);
			}
			p_start++;
		}
	}

	return 0;
}
#endif

static void put_enet_addr_container(struct enet_addr_container *enet_addr_cont)
{
	kfree(enet_addr_cont);
}

static void set_mac_addr(__be16 __iomem *reg, u8 *mac)
{
	out_be16(&reg[0], ((u16)mac[5] << 8) | mac[4]);
	out_be16(&reg[1], ((u16)mac[3] << 8) | mac[2]);
	out_be16(&reg[2], ((u16)mac[1] << 8) | mac[0]);
}

static int hw_clear_addr_in_paddr(struct ucc_geth_private *ugeth, u8 paddr_num)
{
	struct ucc_geth_82xx_address_filtering_pram __iomem *p_82xx_addr_filt;

	if (!(paddr_num < NUM_OF_PADDRS)) {
		ugeth_warn("%s: Illagel paddr_num.", __func__);
		return -EINVAL;
	}

	p_82xx_addr_filt =
	    (struct ucc_geth_82xx_address_filtering_pram __iomem *) ugeth->p_rx_glbl_pram->
	    addressfiltering;

	/* Writing address ff.ff.ff.ff.ff.ff disables address
	recognition for this register */
	out_be16(&p_82xx_addr_filt->paddr[paddr_num].h, 0xffff);
	out_be16(&p_82xx_addr_filt->paddr[paddr_num].m, 0xffff);
	out_be16(&p_82xx_addr_filt->paddr[paddr_num].l, 0xffff);

	return 0;
}

static void hw_add_addr_in_hash(struct ucc_geth_private *ugeth,
                                u8 *p_enet_addr)
{
	struct ucc_geth_82xx_address_filtering_pram __iomem *p_82xx_addr_filt;
	u32 cecr_subblock;

	p_82xx_addr_filt =
	    (struct ucc_geth_82xx_address_filtering_pram __iomem *) ugeth->p_rx_glbl_pram->
	    addressfiltering;

	cecr_subblock =
	    ucc_fast_get_qe_cr_subblock(ugeth->ug_info->uf_info.ucc_num);

	/* Ethernet frames are defined in Little Endian mode,
	therefor to insert */
	/* the address to the hash (Big Endian mode), we reverse the bytes.*/

	set_mac_addr(&p_82xx_addr_filt->taddr.h, p_enet_addr);

	qe_issue_cmd(QE_SET_GROUP_ADDRESS, cecr_subblock,
		     QE_CR_PROTOCOL_ETHERNET, 0);
}

static inline int compare_addr(u8 **addr1, u8 **addr2)
{
	return memcmp(addr1, addr2, ENET_NUM_OCTETS_PER_ADDRESS);
}

#ifdef DEBUG
static void get_statistics(struct ucc_geth_private *ugeth,
			   struct ucc_geth_tx_firmware_statistics *
			   tx_firmware_statistics,
			   struct ucc_geth_rx_firmware_statistics *
			   rx_firmware_statistics,
			   struct ucc_geth_hardware_statistics *hardware_statistics)
{
	struct ucc_fast __iomem *uf_regs;
	struct ucc_geth __iomem *ug_regs;
	struct ucc_geth_tx_firmware_statistics_pram *p_tx_fw_statistics_pram;
	struct ucc_geth_rx_firmware_statistics_pram *p_rx_fw_statistics_pram;

	ug_regs = ugeth->ug_regs;
	uf_regs = (struct ucc_fast __iomem *) ug_regs;
	p_tx_fw_statistics_pram = ugeth->p_tx_fw_statistics_pram;
	p_rx_fw_statistics_pram = ugeth->p_rx_fw_statistics_pram;

	/* Tx firmware only if user handed pointer and driver actually
	gathers Tx firmware statistics */
	if (tx_firmware_statistics && p_tx_fw_statistics_pram) {
		tx_firmware_statistics->sicoltx =
		    in_be32(&p_tx_fw_statistics_pram->sicoltx);
		tx_firmware_statistics->mulcoltx =
		    in_be32(&p_tx_fw_statistics_pram->mulcoltx);
		tx_firmware_statistics->latecoltxfr =
		    in_be32(&p_tx_fw_statistics_pram->latecoltxfr);
		tx_firmware_statistics->frabortduecol =
		    in_be32(&p_tx_fw_statistics_pram->frabortduecol);
		tx_firmware_statistics->frlostinmactxer =
		    in_be32(&p_tx_fw_statistics_pram->frlostinmactxer);
		tx_firmware_statistics->carriersenseertx =
		    in_be32(&p_tx_fw_statistics_pram->carriersenseertx);
		tx_firmware_statistics->frtxok =
		    in_be32(&p_tx_fw_statistics_pram->frtxok);
		tx_firmware_statistics->txfrexcessivedefer =
		    in_be32(&p_tx_fw_statistics_pram->txfrexcessivedefer);
		tx_firmware_statistics->txpkts256 =
		    in_be32(&p_tx_fw_statistics_pram->txpkts256);
		tx_firmware_statistics->txpkts512 =
		    in_be32(&p_tx_fw_statistics_pram->txpkts512);
		tx_firmware_statistics->txpkts1024 =
		    in_be32(&p_tx_fw_statistics_pram->txpkts1024);
		tx_firmware_statistics->txpktsjumbo =
		    in_be32(&p_tx_fw_statistics_pram->txpktsjumbo);
	}

	/* Rx firmware only if user handed pointer and driver actually
	 * gathers Rx firmware statistics */
	if (rx_firmware_statistics && p_rx_fw_statistics_pram) {
		int i;
		rx_firmware_statistics->frrxfcser =
		    in_be32(&p_rx_fw_statistics_pram->frrxfcser);
		rx_firmware_statistics->fraligner =
		    in_be32(&p_rx_fw_statistics_pram->fraligner);
		rx_firmware_statistics->inrangelenrxer =
		    in_be32(&p_rx_fw_statistics_pram->inrangelenrxer);
		rx_firmware_statistics->outrangelenrxer =
		    in_be32(&p_rx_fw_statistics_pram->outrangelenrxer);
		rx_firmware_statistics->frtoolong =
		    in_be32(&p_rx_fw_statistics_pram->frtoolong);
		rx_firmware_statistics->runt =
		    in_be32(&p_rx_fw_statistics_pram->runt);
		rx_firmware_statistics->verylongevent =
		    in_be32(&p_rx_fw_statistics_pram->verylongevent);
		rx_firmware_statistics->symbolerror =
		    in_be32(&p_rx_fw_statistics_pram->symbolerror);
		rx_firmware_statistics->dropbsy =
		    in_be32(&p_rx_fw_statistics_pram->dropbsy);
		for (i = 0; i < 0x8; i++)
			rx_firmware_statistics->res0[i] =
			    p_rx_fw_statistics_pram->res0[i];
		rx_firmware_statistics->mismatchdrop =
		    in_be32(&p_rx_fw_statistics_pram->mismatchdrop);
		rx_firmware_statistics->underpkts =
		    in_be32(&p_rx_fw_statistics_pram->underpkts);
		rx_firmware_statistics->pkts256 =
		    in_be32(&p_rx_fw_statistics_pram->pkts256);
		rx_firmware_statistics->pkts512 =
		    in_be32(&p_rx_fw_statistics_pram->pkts512);
		rx_firmware_statistics->pkts1024 =
		    in_be32(&p_rx_fw_statistics_pram->pkts1024);
		rx_firmware_statistics->pktsjumbo =
		    in_be32(&p_rx_fw_statistics_pram->pktsjumbo);
		rx_firmware_statistics->frlossinmacer =
		    in_be32(&p_rx_fw_statistics_pram->frlossinmacer);
		rx_firmware_statistics->pausefr =
		    in_be32(&p_rx_fw_statistics_pram->pausefr);
		for (i = 0; i < 0x4; i++)
			rx_firmware_statistics->res1[i] =
			    p_rx_fw_statistics_pram->res1[i];
		rx_firmware_statistics->removevlan =
		    in_be32(&p_rx_fw_statistics_pram->removevlan);
		rx_firmware_statistics->replacevlan =
		    in_be32(&p_rx_fw_statistics_pram->replacevlan);
		rx_firmware_statistics->insertvlan =
		    in_be32(&p_rx_fw_statistics_pram->insertvlan);
	}

	/* Hardware only if user handed pointer and driver actually
	gathers hardware statistics */
	if (hardware_statistics &&
	    (in_be32(&uf_regs->upsmr) & UCC_GETH_UPSMR_HSE)) {
		hardware_statistics->tx64 = in_be32(&ug_regs->tx64);
		hardware_statistics->tx127 = in_be32(&ug_regs->tx127);
		hardware_statistics->tx255 = in_be32(&ug_regs->tx255);
		hardware_statistics->rx64 = in_be32(&ug_regs->rx64);
		hardware_statistics->rx127 = in_be32(&ug_regs->rx127);
		hardware_statistics->rx255 = in_be32(&ug_regs->rx255);
		hardware_statistics->txok = in_be32(&ug_regs->txok);
		hardware_statistics->txcf = in_be16(&ug_regs->txcf);
		hardware_statistics->tmca = in_be32(&ug_regs->tmca);
		hardware_statistics->tbca = in_be32(&ug_regs->tbca);
		hardware_statistics->rxfok = in_be32(&ug_regs->rxfok);
		hardware_statistics->rxbok = in_be32(&ug_regs->rxbok);
		hardware_statistics->rbyt = in_be32(&ug_regs->rbyt);
		hardware_statistics->rmca = in_be32(&ug_regs->rmca);
		hardware_statistics->rbca = in_be32(&ug_regs->rbca);
	}
}

static void dump_bds(struct ucc_geth_private *ugeth)
{
	int i;
	int length;

	for (i = 0; i < ugeth->ug_info->numQueuesTx; i++) {
		if (ugeth->p_tx_bd_ring[i]) {
			length =
			    (ugeth->ug_info->bdRingLenTx[i] *
			     sizeof(struct qe_bd));
			ugeth_info("TX BDs[%d]", i);
			mem_disp(ugeth->p_tx_bd_ring[i], length);
		}
	}
	for (i = 0; i < ugeth->ug_info->numQueuesRx; i++) {
		if (ugeth->p_rx_bd_ring[i]) {
			length =
			    (ugeth->ug_info->bdRingLenRx[i] *
			     sizeof(struct qe_bd));
			ugeth_info("RX BDs[%d]", i);
			mem_disp(ugeth->p_rx_bd_ring[i], length);
		}
	}
}

static void dump_regs(struct ucc_geth_private *ugeth)
{
	int i;

	ugeth_info("UCC%d Geth registers:", ugeth->ug_info->uf_info.ucc_num);
	ugeth_info("Base address: 0x%08x", (u32) ugeth->ug_regs);

	ugeth_info("maccfg1    : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->maccfg1,
		   in_be32(&ugeth->ug_regs->maccfg1));
	ugeth_info("maccfg2    : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->maccfg2,
		   in_be32(&ugeth->ug_regs->maccfg2));
	ugeth_info("ipgifg     : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->ipgifg,
		   in_be32(&ugeth->ug_regs->ipgifg));
	ugeth_info("hafdup     : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->hafdup,
		   in_be32(&ugeth->ug_regs->hafdup));
	ugeth_info("ifctl      : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->ifctl,
		   in_be32(&ugeth->ug_regs->ifctl));
	ugeth_info("ifstat     : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->ifstat,
		   in_be32(&ugeth->ug_regs->ifstat));
	ugeth_info("macstnaddr1: addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->macstnaddr1,
		   in_be32(&ugeth->ug_regs->macstnaddr1));
	ugeth_info("macstnaddr2: addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->macstnaddr2,
		   in_be32(&ugeth->ug_regs->macstnaddr2));
	ugeth_info("uempr      : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->uempr,
		   in_be32(&ugeth->ug_regs->uempr));
	ugeth_info("utbipar    : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->utbipar,
		   in_be32(&ugeth->ug_regs->utbipar));
	ugeth_info("uescr      : addr - 0x%08x, val - 0x%04x",
		   (u32) & ugeth->ug_regs->uescr,
		   in_be16(&ugeth->ug_regs->uescr));
	ugeth_info("tx64       : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->tx64,
		   in_be32(&ugeth->ug_regs->tx64));
	ugeth_info("tx127      : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->tx127,
		   in_be32(&ugeth->ug_regs->tx127));
	ugeth_info("tx255      : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->tx255,
		   in_be32(&ugeth->ug_regs->tx255));
	ugeth_info("rx64       : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->rx64,
		   in_be32(&ugeth->ug_regs->rx64));
	ugeth_info("rx127      : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->rx127,
		   in_be32(&ugeth->ug_regs->rx127));
	ugeth_info("rx255      : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->rx255,
		   in_be32(&ugeth->ug_regs->rx255));
	ugeth_info("txok       : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->txok,
		   in_be32(&ugeth->ug_regs->txok));
	ugeth_info("txcf       : addr - 0x%08x, val - 0x%04x",
		   (u32) & ugeth->ug_regs->txcf,
		   in_be16(&ugeth->ug_regs->txcf));
	ugeth_info("tmca       : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->tmca,
		   in_be32(&ugeth->ug_regs->tmca));
	ugeth_info("tbca       : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->tbca,
		   in_be32(&ugeth->ug_regs->tbca));
	ugeth_info("rxfok      : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->rxfok,
		   in_be32(&ugeth->ug_regs->rxfok));
	ugeth_info("rxbok      : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->rxbok,
		   in_be32(&ugeth->ug_regs->rxbok));
	ugeth_info("rbyt       : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->rbyt,
		   in_be32(&ugeth->ug_regs->rbyt));
	ugeth_info("rmca       : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->rmca,
		   in_be32(&ugeth->ug_regs->rmca));
	ugeth_info("rbca       : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->rbca,
		   in_be32(&ugeth->ug_regs->rbca));
	ugeth_info("scar       : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->scar,
		   in_be32(&ugeth->ug_regs->scar));
	ugeth_info("scam       : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->scam,
		   in_be32(&ugeth->ug_regs->scam));

	if (ugeth->p_thread_data_tx) {
		int numThreadsTxNumerical;
		switch (ugeth->ug_info->numThreadsTx) {
		case UCC_GETH_NUM_OF_THREADS_1:
			numThreadsTxNumerical = 1;
			break;
		case UCC_GETH_NUM_OF_THREADS_2:
			numThreadsTxNumerical = 2;
			break;
		case UCC_GETH_NUM_OF_THREADS_4:
			numThreadsTxNumerical = 4;
			break;
		case UCC_GETH_NUM_OF_THREADS_6:
			numThreadsTxNumerical = 6;
			break;
		case UCC_GETH_NUM_OF_THREADS_8:
			numThreadsTxNumerical = 8;
			break;
		default:
			numThreadsTxNumerical = 0;
			break;
		}

		ugeth_info("Thread data TXs:");
		ugeth_info("Base address: 0x%08x",
			   (u32) ugeth->p_thread_data_tx);
		for (i = 0; i < numThreadsTxNumerical; i++) {
			ugeth_info("Thread data TX[%d]:", i);
			ugeth_info("Base address: 0x%08x",
				   (u32) & ugeth->p_thread_data_tx[i]);
			mem_disp((u8 *) & ugeth->p_thread_data_tx[i],
				 sizeof(struct ucc_geth_thread_data_tx));
		}
	}
	if (ugeth->p_thread_data_rx) {
		int numThreadsRxNumerical;
		switch (ugeth->ug_info->numThreadsRx) {
		case UCC_GETH_NUM_OF_THREADS_1:
			numThreadsRxNumerical = 1;
			break;
		case UCC_GETH_NUM_OF_THREADS_2:
			numThreadsRxNumerical = 2;
			break;
		case UCC_GETH_NUM_OF_THREADS_4:
			numThreadsRxNumerical = 4;
			break;
		case UCC_GETH_NUM_OF_THREADS_6:
			numThreadsRxNumerical = 6;
			break;
		case UCC_GETH_NUM_OF_THREADS_8:
			numThreadsRxNumerical = 8;
			break;
		default:
			numThreadsRxNumerical = 0;
			break;
		}

		ugeth_info("Thread data RX:");
		ugeth_info("Base address: 0x%08x",
			   (u32) ugeth->p_thread_data_rx);
		for (i = 0; i < numThreadsRxNumerical; i++) {
			ugeth_info("Thread data RX[%d]:", i);
			ugeth_info("Base address: 0x%08x",
				   (u32) & ugeth->p_thread_data_rx[i]);
			mem_disp((u8 *) & ugeth->p_thread_data_rx[i],
				 sizeof(struct ucc_geth_thread_data_rx));
		}
	}
	if (ugeth->p_exf_glbl_param) {
		ugeth_info("EXF global param:");
		ugeth_info("Base address: 0x%08x",
			   (u32) ugeth->p_exf_glbl_param);
		mem_disp((u8 *) ugeth->p_exf_glbl_param,
			 sizeof(*ugeth->p_exf_glbl_param));
	}
	if (ugeth->p_tx_glbl_pram) {
		ugeth_info("TX global param:");
		ugeth_info("Base address: 0x%08x", (u32) ugeth->p_tx_glbl_pram);
		ugeth_info("temoder      : addr - 0x%08x, val - 0x%04x",
			   (u32) & ugeth->p_tx_glbl_pram->temoder,
			   in_be16(&ugeth->p_tx_glbl_pram->temoder));
		ugeth_info("sqptr        : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_tx_glbl_pram->sqptr,
			   in_be32(&ugeth->p_tx_glbl_pram->sqptr));
		ugeth_info("schedulerbasepointer: addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_tx_glbl_pram->schedulerbasepointer,
			   in_be32(&ugeth->p_tx_glbl_pram->
				   schedulerbasepointer));
		ugeth_info("txrmonbaseptr: addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_tx_glbl_pram->txrmonbaseptr,
			   in_be32(&ugeth->p_tx_glbl_pram->txrmonbaseptr));
		ugeth_info("tstate       : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_tx_glbl_pram->tstate,
			   in_be32(&ugeth->p_tx_glbl_pram->tstate));
		ugeth_info("iphoffset[0] : addr - 0x%08x, val - 0x%02x",
			   (u32) & ugeth->p_tx_glbl_pram->iphoffset[0],
			   ugeth->p_tx_glbl_pram->iphoffset[0]);
		ugeth_info("iphoffset[1] : addr - 0x%08x, val - 0x%02x",
			   (u32) & ugeth->p_tx_glbl_pram->iphoffset[1],
			   ugeth->p_tx_glbl_pram->iphoffset[1]);
		ugeth_info("iphoffset[2] : addr - 0x%08x, val - 0x%02x",
			   (u32) & ugeth->p_tx_glbl_pram->iphoffset[2],
			   ugeth->p_tx_glbl_pram->iphoffset[2]);
		ugeth_info("iphoffset[3] : addr - 0x%08x, val - 0x%02x",
			   (u32) & ugeth->p_tx_glbl_pram->iphoffset[3],
			   ugeth->p_tx_glbl_pram->iphoffset[3]);
		ugeth_info("iphoffset[4] : addr - 0x%08x, val - 0x%02x",
			   (u32) & ugeth->p_tx_glbl_pram->iphoffset[4],
			   ugeth->p_tx_glbl_pram->iphoffset[4]);
		ugeth_info("iphoffset[5] : addr - 0x%08x, val - 0x%02x",
			   (u32) & ugeth->p_tx_glbl_pram->iphoffset[5],
			   ugeth->p_tx_glbl_pram->iphoffset[5]);
		ugeth_info("iphoffset[6] : addr - 0x%08x, val - 0x%02x",
			   (u32) & ugeth->p_tx_glbl_pram->iphoffset[6],
			   ugeth->p_tx_glbl_pram->iphoffset[6]);
		ugeth_info("iphoffset[7] : addr - 0x%08x, val - 0x%02x",
			   (u32) & ugeth->p_tx_glbl_pram->iphoffset[7],
			   ugeth->p_tx_glbl_pram->iphoffset[7]);
		ugeth_info("vtagtable[0] : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_tx_glbl_pram->vtagtable[0],
			   in_be32(&ugeth->p_tx_glbl_pram->vtagtable[0]));
		ugeth_info("vtagtable[1] : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_tx_glbl_pram->vtagtable[1],
			   in_be32(&ugeth->p_tx_glbl_pram->vtagtable[1]));
		ugeth_info("vtagtable[2] : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_tx_glbl_pram->vtagtable[2],
			   in_be32(&ugeth->p_tx_glbl_pram->vtagtable[2]));
		ugeth_info("vtagtable[3] : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_tx_glbl_pram->vtagtable[3],
			   in_be32(&ugeth->p_tx_glbl_pram->vtagtable[3]));
		ugeth_info("vtagtable[4] : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_tx_glbl_pram->vtagtable[4],
			   in_be32(&ugeth->p_tx_glbl_pram->vtagtable[4]));
		ugeth_info("vtagtable[5] : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_tx_glbl_pram->vtagtable[5],
			   in_be32(&ugeth->p_tx_glbl_pram->vtagtable[5]));
		ugeth_info("vtagtable[6] : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_tx_glbl_pram->vtagtable[6],
			   in_be32(&ugeth->p_tx_glbl_pram->vtagtable[6]));
		ugeth_info("vtagtable[7] : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_tx_glbl_pram->vtagtable[7],
			   in_be32(&ugeth->p_tx_glbl_pram->vtagtable[7]));
		ugeth_info("tqptr        : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_tx_glbl_pram->tqptr,
			   in_be32(&ugeth->p_tx_glbl_pram->tqptr));
	}
	if (ugeth->p_rx_glbl_pram) {
		ugeth_info("RX global param:");
		ugeth_info("Base address: 0x%08x", (u32) ugeth->p_rx_glbl_pram);
		ugeth_info("remoder         : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_rx_glbl_pram->remoder,
			   in_be32(&ugeth->p_rx_glbl_pram->remoder));
		ugeth_info("rqptr           : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_rx_glbl_pram->rqptr,
			   in_be32(&ugeth->p_rx_glbl_pram->rqptr));
		ugeth_info("typeorlen       : addr - 0x%08x, val - 0x%04x",
			   (u32) & ugeth->p_rx_glbl_pram->typeorlen,
			   in_be16(&ugeth->p_rx_glbl_pram->typeorlen));
		ugeth_info("rxgstpack       : addr - 0x%08x, val - 0x%02x",
			   (u32) & ugeth->p_rx_glbl_pram->rxgstpack,
			   ugeth->p_rx_glbl_pram->rxgstpack);
		ugeth_info("rxrmonbaseptr   : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_rx_glbl_pram->rxrmonbaseptr,
			   in_be32(&ugeth->p_rx_glbl_pram->rxrmonbaseptr));
		ugeth_info("intcoalescingptr: addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_rx_glbl_pram->intcoalescingptr,
			   in_be32(&ugeth->p_rx_glbl_pram->intcoalescingptr));
		ugeth_info("rstate          : addr - 0x%08x, val - 0x%02x",
			   (u32) & ugeth->p_rx_glbl_pram->rstate,
			   ugeth->p_rx_glbl_pram->rstate);
		ugeth_info("mrblr           : addr - 0x%08x, val - 0x%04x",
			   (u32) & ugeth->p_rx_glbl_pram->mrblr,
			   in_be16(&ugeth->p_rx_glbl_pram->mrblr));
		ugeth_info("rbdqptr         : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_rx_glbl_pram->rbdqptr,
			   in_be32(&ugeth->p_rx_glbl_pram->rbdqptr));
		ugeth_info("mflr            : addr - 0x%08x, val - 0x%04x",
			   (u32) & ugeth->p_rx_glbl_pram->mflr,
			   in_be16(&ugeth->p_rx_glbl_pram->mflr));
		ugeth_info("minflr          : addr - 0x%08x, val - 0x%04x",
			   (u32) & ugeth->p_rx_glbl_pram->minflr,
			   in_be16(&ugeth->p_rx_glbl_pram->minflr));
		ugeth_info("maxd1           : addr - 0x%08x, val - 0x%04x",
			   (u32) & ugeth->p_rx_glbl_pram->maxd1,
			   in_be16(&ugeth->p_rx_glbl_pram->maxd1));
		ugeth_info("maxd2           : addr - 0x%08x, val - 0x%04x",
			   (u32) & ugeth->p_rx_glbl_pram->maxd2,
			   in_be16(&ugeth->p_rx_glbl_pram->maxd2));
		ugeth_info("ecamptr         : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_rx_glbl_pram->ecamptr,
			   in_be32(&ugeth->p_rx_glbl_pram->ecamptr));
		ugeth_info("l2qt            : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_rx_glbl_pram->l2qt,
			   in_be32(&ugeth->p_rx_glbl_pram->l2qt));
		ugeth_info("l3qt[0]         : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_rx_glbl_pram->l3qt[0],
			   in_be32(&ugeth->p_rx_glbl_pram->l3qt[0]));
		ugeth_info("l3qt[1]         : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_rx_glbl_pram->l3qt[1],
			   in_be32(&ugeth->p_rx_glbl_pram->l3qt[1]));
		ugeth_info("l3qt[2]         : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_rx_glbl_pram->l3qt[2],
			   in_be32(&ugeth->p_rx_glbl_pram->l3qt[2]));
		ugeth_info("l3qt[3]         : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_rx_glbl_pram->l3qt[3],
			   in_be32(&ugeth->p_rx_glbl_pram->l3qt[3]));
		ugeth_info("l3qt[4]         : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_rx_glbl_pram->l3qt[4],
			   in_be32(&ugeth->p_rx_glbl_pram->l3qt[4]));
		ugeth_info("l3qt[5]         : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_rx_glbl_pram->l3qt[5],
			   in_be32(&ugeth->p_rx_glbl_pram->l3qt[5]));
		ugeth_info("l3qt[6]         : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_rx_glbl_pram->l3qt[6],
			   in_be32(&ugeth->p_rx_glbl_pram->l3qt[6]));
		ugeth_info("l3qt[7]         : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_rx_glbl_pram->l3qt[7],
			   in_be32(&ugeth->p_rx_glbl_pram->l3qt[7]));
		ugeth_info("vlantype        : addr - 0x%08x, val - 0x%04x",
			   (u32) & ugeth->p_rx_glbl_pram->vlantype,
			   in_be16(&ugeth->p_rx_glbl_pram->vlantype));
		ugeth_info("vlantci         : addr - 0x%08x, val - 0x%04x",
			   (u32) & ugeth->p_rx_glbl_pram->vlantci,
			   in_be16(&ugeth->p_rx_glbl_pram->vlantci));
		for (i = 0; i < 64; i++)
			ugeth_info
		    ("addressfiltering[%d]: addr - 0x%08x, val - 0x%02x",
			     i,
			     (u32) & ugeth->p_rx_glbl_pram->addressfiltering[i],
			     ugeth->p_rx_glbl_pram->addressfiltering[i]);
		ugeth_info("exfGlobalParam  : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_rx_glbl_pram->exfGlobalParam,
			   in_be32(&ugeth->p_rx_glbl_pram->exfGlobalParam));
	}
	if (ugeth->p_send_q_mem_reg) {
		ugeth_info("Send Q memory registers:");
		ugeth_info("Base address: 0x%08x",
			   (u32) ugeth->p_send_q_mem_reg);
		for (i = 0; i < ugeth->ug_info->numQueuesTx; i++) {
			ugeth_info("SQQD[%d]:", i);
			ugeth_info("Base address: 0x%08x",
				   (u32) & ugeth->p_send_q_mem_reg->sqqd[i]);
			mem_disp((u8 *) & ugeth->p_send_q_mem_reg->sqqd[i],
				 sizeof(struct ucc_geth_send_queue_qd));
		}
	}
	if (ugeth->p_scheduler) {
		ugeth_info("Scheduler:");
		ugeth_info("Base address: 0x%08x", (u32) ugeth->p_scheduler);
		mem_disp((u8 *) ugeth->p_scheduler,
			 sizeof(*ugeth->p_scheduler));
	}
	if (ugeth->p_tx_fw_statistics_pram) {
		ugeth_info("TX FW statistics pram:");
		ugeth_info("Base address: 0x%08x",
			   (u32) ugeth->p_tx_fw_statistics_pram);
		mem_disp((u8 *) ugeth->p_tx_fw_statistics_pram,
			 sizeof(*ugeth->p_tx_fw_statistics_pram));
	}
	if (ugeth->p_rx_fw_statistics_pram) {
		ugeth_info("RX FW statistics pram:");
		ugeth_info("Base address: 0x%08x",
			   (u32) ugeth->p_rx_fw_statistics_pram);
		mem_disp((u8 *) ugeth->p_rx_fw_statistics_pram,
			 sizeof(*ugeth->p_rx_fw_statistics_pram));
	}
	if (ugeth->p_rx_irq_coalescing_tbl) {
		ugeth_info("RX IRQ coalescing tables:");
		ugeth_info("Base address: 0x%08x",
			   (u32) ugeth->p_rx_irq_coalescing_tbl);
		for (i = 0; i < ugeth->ug_info->numQueuesRx; i++) {
			ugeth_info("RX IRQ coalescing table entry[%d]:", i);
			ugeth_info("Base address: 0x%08x",
				   (u32) & ugeth->p_rx_irq_coalescing_tbl->
				   coalescingentry[i]);
			ugeth_info
		("interruptcoalescingmaxvalue: addr - 0x%08x, val - 0x%08x",
			     (u32) & ugeth->p_rx_irq_coalescing_tbl->
			     coalescingentry[i].interruptcoalescingmaxvalue,
			     in_be32(&ugeth->p_rx_irq_coalescing_tbl->
				     coalescingentry[i].
				     interruptcoalescingmaxvalue));
			ugeth_info
		("interruptcoalescingcounter : addr - 0x%08x, val - 0x%08x",
			     (u32) & ugeth->p_rx_irq_coalescing_tbl->
			     coalescingentry[i].interruptcoalescingcounter,
			     in_be32(&ugeth->p_rx_irq_coalescing_tbl->
				     coalescingentry[i].
				     interruptcoalescingcounter));
		}
	}
	if (ugeth->p_rx_bd_qs_tbl) {
		ugeth_info("RX BD QS tables:");
		ugeth_info("Base address: 0x%08x", (u32) ugeth->p_rx_bd_qs_tbl);
		for (i = 0; i < ugeth->ug_info->numQueuesRx; i++) {
			ugeth_info("RX BD QS table[%d]:", i);
			ugeth_info("Base address: 0x%08x",
				   (u32) & ugeth->p_rx_bd_qs_tbl[i]);
			ugeth_info
			    ("bdbaseptr        : addr - 0x%08x, val - 0x%08x",
			     (u32) & ugeth->p_rx_bd_qs_tbl[i].bdbaseptr,
			     in_be32(&ugeth->p_rx_bd_qs_tbl[i].bdbaseptr));
			ugeth_info
			    ("bdptr            : addr - 0x%08x, val - 0x%08x",
			     (u32) & ugeth->p_rx_bd_qs_tbl[i].bdptr,
			     in_be32(&ugeth->p_rx_bd_qs_tbl[i].bdptr));
			ugeth_info
			    ("externalbdbaseptr: addr - 0x%08x, val - 0x%08x",
			     (u32) & ugeth->p_rx_bd_qs_tbl[i].externalbdbaseptr,
			     in_be32(&ugeth->p_rx_bd_qs_tbl[i].
				     externalbdbaseptr));
			ugeth_info
			    ("externalbdptr    : addr - 0x%08x, val - 0x%08x",
			     (u32) & ugeth->p_rx_bd_qs_tbl[i].externalbdptr,
			     in_be32(&ugeth->p_rx_bd_qs_tbl[i].externalbdptr));
			ugeth_info("ucode RX Prefetched BDs:");
			ugeth_info("Base address: 0x%08x",
				   (u32)
				   qe_muram_addr(in_be32
						 (&ugeth->p_rx_bd_qs_tbl[i].
						  bdbaseptr)));
			mem_disp((u8 *)
				 qe_muram_addr(in_be32
					       (&ugeth->p_rx_bd_qs_tbl[i].
						bdbaseptr)),
				 sizeof(struct ucc_geth_rx_prefetched_bds));
		}
	}
	if (ugeth->p_init_enet_param_shadow) {
		int size;
		ugeth_info("Init enet param shadow:");
		ugeth_info("Base address: 0x%08x",
			   (u32) ugeth->p_init_enet_param_shadow);
		mem_disp((u8 *) ugeth->p_init_enet_param_shadow,
			 sizeof(*ugeth->p_init_enet_param_shadow));

		size = sizeof(struct ucc_geth_thread_rx_pram);
		if (ugeth->ug_info->rxExtendedFiltering) {
			size +=
			    THREAD_RX_PRAM_ADDITIONAL_FOR_EXTENDED_FILTERING;
			if (ugeth->ug_info->largestexternallookupkeysize ==
			    QE_FLTR_TABLE_LOOKUP_KEY_SIZE_8_BYTES)
				size +=
			THREAD_RX_PRAM_ADDITIONAL_FOR_EXTENDED_FILTERING_8;
			if (ugeth->ug_info->largestexternallookupkeysize ==
			    QE_FLTR_TABLE_LOOKUP_KEY_SIZE_16_BYTES)
				size +=
			THREAD_RX_PRAM_ADDITIONAL_FOR_EXTENDED_FILTERING_16;
		}

		dump_init_enet_entries(ugeth,
				       &(ugeth->p_init_enet_param_shadow->
					 txthread[0]),
				       ENET_INIT_PARAM_MAX_ENTRIES_TX,
				       sizeof(struct ucc_geth_thread_tx_pram),
				       ugeth->ug_info->riscTx, 0);
		dump_init_enet_entries(ugeth,
				       &(ugeth->p_init_enet_param_shadow->
					 rxthread[0]),
				       ENET_INIT_PARAM_MAX_ENTRIES_RX, size,
				       ugeth->ug_info->riscRx, 1);
	}
}
#endif /* DEBUG */

static void init_default_reg_vals(u32 __iomem *upsmr_register,
				  u32 __iomem *maccfg1_register,
				  u32 __iomem *maccfg2_register)
{
	out_be32(upsmr_register, UCC_GETH_UPSMR_INIT);
	out_be32(maccfg1_register, UCC_GETH_MACCFG1_INIT);
	out_be32(maccfg2_register, UCC_GETH_MACCFG2_INIT);
}

static int init_half_duplex_params(int alt_beb,
				   int back_pressure_no_backoff,
				   int no_backoff,
				   int excess_defer,
				   u8 alt_beb_truncation,
				   u8 max_retransmissions,
				   u8 collision_window,
				   u32 __iomem *hafdup_register)
{
	u32 value = 0;

	if ((alt_beb_truncation > HALFDUP_ALT_BEB_TRUNCATION_MAX) ||
	    (max_retransmissions > HALFDUP_MAX_RETRANSMISSION_MAX) ||
	    (collision_window > HALFDUP_COLLISION_WINDOW_MAX))
		return -EINVAL;

	value = (u32) (alt_beb_truncation << HALFDUP_ALT_BEB_TRUNCATION_SHIFT);

	if (alt_beb)
		value |= HALFDUP_ALT_BEB;
	if (back_pressure_no_backoff)
		value |= HALFDUP_BACK_PRESSURE_NO_BACKOFF;
	if (no_backoff)
		value |= HALFDUP_NO_BACKOFF;
	if (excess_defer)
		value |= HALFDUP_EXCESSIVE_DEFER;

	value |= (max_retransmissions << HALFDUP_MAX_RETRANSMISSION_SHIFT);

	value |= collision_window;

	out_be32(hafdup_register, value);
	return 0;
}

static int init_inter_frame_gap_params(u8 non_btb_cs_ipg,
				       u8 non_btb_ipg,
				       u8 min_ifg,
				       u8 btb_ipg,
				       u32 __iomem *ipgifg_register)
{
	u32 value = 0;

	/* Non-Back-to-back IPG part 1 should be <= Non-Back-to-back
	IPG part 2 */
	if (non_btb_cs_ipg > non_btb_ipg)
		return -EINVAL;

	if ((non_btb_cs_ipg > IPGIFG_NON_BACK_TO_BACK_IFG_PART1_MAX) ||
	    (non_btb_ipg > IPGIFG_NON_BACK_TO_BACK_IFG_PART2_MAX) ||
	    /*(min_ifg        > IPGIFG_MINIMUM_IFG_ENFORCEMENT_MAX) || */
	    (btb_ipg > IPGIFG_BACK_TO_BACK_IFG_MAX))
		return -EINVAL;

	value |=
	    ((non_btb_cs_ipg << IPGIFG_NON_BACK_TO_BACK_IFG_PART1_SHIFT) &
	     IPGIFG_NBTB_CS_IPG_MASK);
	value |=
	    ((non_btb_ipg << IPGIFG_NON_BACK_TO_BACK_IFG_PART2_SHIFT) &
	     IPGIFG_NBTB_IPG_MASK);
	value |=
	    ((min_ifg << IPGIFG_MINIMUM_IFG_ENFORCEMENT_SHIFT) &
	     IPGIFG_MIN_IFG_MASK);
	value |= (btb_ipg & IPGIFG_BTB_IPG_MASK);

	out_be32(ipgifg_register, value);
	return 0;
}

int init_flow_control_params(u32 automatic_flow_control_mode,
				    int rx_flow_control_enable,
				    int tx_flow_control_enable,
				    u16 pause_period,
				    u16 extension_field,
				    u32 __iomem *upsmr_register,
				    u32 __iomem *uempr_register,
				    u32 __iomem *maccfg1_register)
{
	u32 value = 0;

	/* Set UEMPR register */
	value = (u32) pause_period << UEMPR_PAUSE_TIME_VALUE_SHIFT;
	value |= (u32) extension_field << UEMPR_EXTENDED_PAUSE_TIME_VALUE_SHIFT;
	out_be32(uempr_register, value);

	/* Set UPSMR register */
	setbits32(upsmr_register, automatic_flow_control_mode);

	value = in_be32(maccfg1_register);
	if (rx_flow_control_enable)
		value |= MACCFG1_FLOW_RX;
	if (tx_flow_control_enable)
		value |= MACCFG1_FLOW_TX;
	out_be32(maccfg1_register, value);

	return 0;
}

static int init_hw_statistics_gathering_mode(int enable_hardware_statistics,
					     int auto_zero_hardware_statistics,
					     u32 __iomem *upsmr_register,
					     u16 __iomem *uescr_register)
{
	u16 uescr_value = 0;

	/* Enable hardware statistics gathering if requested */
	if (enable_hardware_statistics)
		setbits32(upsmr_register, UCC_GETH_UPSMR_HSE);

	/* Clear hardware statistics counters */
	uescr_value = in_be16(uescr_register);
	uescr_value |= UESCR_CLRCNT;
	/* Automatically zero hardware statistics counters on read,
	if requested */
	if (auto_zero_hardware_statistics)
		uescr_value |= UESCR_AUTOZ;
	out_be16(uescr_register, uescr_value);

	return 0;
}

static int init_firmware_statistics_gathering_mode(int
		enable_tx_firmware_statistics,
		int enable_rx_firmware_statistics,
		u32 __iomem *tx_rmon_base_ptr,
		u32 tx_firmware_statistics_structure_address,
		u32 __iomem *rx_rmon_base_ptr,
		u32 rx_firmware_statistics_structure_address,
		u16 __iomem *temoder_register,
		u32 __iomem *remoder_register)
{
	/* Note: this function does not check if */
	/* the parameters it receives are NULL   */

	if (enable_tx_firmware_statistics) {
		out_be32(tx_rmon_base_ptr,
			 tx_firmware_statistics_structure_address);
		setbits16(temoder_register, TEMODER_TX_RMON_STATISTICS_ENABLE);
	}

	if (enable_rx_firmware_statistics) {
		out_be32(rx_rmon_base_ptr,
			 rx_firmware_statistics_structure_address);
		setbits32(remoder_register, REMODER_RX_RMON_STATISTICS_ENABLE);
	}

	return 0;
}

static int init_mac_station_addr_regs(u8 address_byte_0,
				      u8 address_byte_1,
				      u8 address_byte_2,
				      u8 address_byte_3,
				      u8 address_byte_4,
				      u8 address_byte_5,
				      u32 __iomem *macstnaddr1_register,
				      u32 __iomem *macstnaddr2_register)
{
	u32 value = 0;

	/* Example: for a station address of 0x12345678ABCD, */
	/* 0x12 is byte 0, 0x34 is byte 1 and so on and 0xCD is byte 5 */

	/* MACSTNADDR1 Register: */

	/* 0                      7   8                      15  */
	/* station address byte 5     station address byte 4     */
	/* 16                     23  24                     31  */
	/* station address byte 3     station address byte 2     */
	value |= (u32) ((address_byte_2 << 0) & 0x000000FF);
	value |= (u32) ((address_byte_3 << 8) & 0x0000FF00);
	value |= (u32) ((address_byte_4 << 16) & 0x00FF0000);
	value |= (u32) ((address_byte_5 << 24) & 0xFF000000);

	out_be32(macstnaddr1_register, value);

	/* MACSTNADDR2 Register: */

	/* 0                      7   8                      15  */
	/* station address byte 1     station address byte 0     */
	/* 16                     23  24                     31  */
	/*         reserved                   reserved           */
	value = 0;
	value |= (u32) ((address_byte_0 << 16) & 0x00FF0000);
	value |= (u32) ((address_byte_1 << 24) & 0xFF000000);

	out_be32(macstnaddr2_register, value);

	return 0;
}

static int init_check_frame_length_mode(int length_check,
					u32 __iomem *maccfg2_register)
{
	u32 value = 0;

	value = in_be32(maccfg2_register);

	if (length_check)
		value |= MACCFG2_LC;
	else
		value &= ~MACCFG2_LC;

	out_be32(maccfg2_register, value);
	return 0;
}

static int init_preamble_length(u8 preamble_length,
				u32 __iomem *maccfg2_register)
{
	if ((preamble_length < 3) || (preamble_length > 7))
		return -EINVAL;

	clrsetbits_be32(maccfg2_register, MACCFG2_PREL_MASK,
			preamble_length << MACCFG2_PREL_SHIFT);

	return 0;
}

static int init_rx_parameters(int reject_broadcast,
			      int receive_short_frames,
			      int promiscuous, u32 __iomem *upsmr_register)
{
	u32 value = 0;

	value = in_be32(upsmr_register);

	if (reject_broadcast)
		value |= UCC_GETH_UPSMR_BRO;
	else
		value &= ~UCC_GETH_UPSMR_BRO;

	if (receive_short_frames)
		value |= UCC_GETH_UPSMR_RSH;
	else
		value &= ~UCC_GETH_UPSMR_RSH;

	if (promiscuous)
		value |= UCC_GETH_UPSMR_PRO;
	else
		value &= ~UCC_GETH_UPSMR_PRO;

	out_be32(upsmr_register, value);

	return 0;
}

static int init_max_rx_buff_len(u16 max_rx_buf_len,
				u16 __iomem *mrblr_register)
{
	/* max_rx_buf_len value must be a multiple of 128 */
	if ((max_rx_buf_len == 0)
	    || (max_rx_buf_len % UCC_GETH_MRBLR_ALIGNMENT))
		return -EINVAL;

	out_be16(mrblr_register, max_rx_buf_len);
	return 0;
}

static int init_min_frame_len(u16 min_frame_length,
			      u16 __iomem *minflr_register,
			      u16 __iomem *mrblr_register)
{
	u16 mrblr_value = 0;

	mrblr_value = in_be16(mrblr_register);
	if (min_frame_length >= (mrblr_value - 4))
		return -EINVAL;

	out_be16(minflr_register, min_frame_length);
	return 0;
}

static int adjust_enet_interface(struct ucc_geth_private *ugeth)
{
	struct ucc_geth_info *ug_info;
	struct ucc_geth __iomem *ug_regs;
	struct ucc_fast __iomem *uf_regs;
	int ret_val;
	u32 upsmr, maccfg2, tbiBaseAddress;
	u16 value;

	ugeth_vdbg("%s: IN", __func__);

	ug_info = ugeth->ug_info;
	ug_regs = ugeth->ug_regs;
	uf_regs = ugeth->uccf->uf_regs;

	/*                    Set MACCFG2                    */
	maccfg2 = in_be32(&ug_regs->maccfg2);
	maccfg2 &= ~MACCFG2_INTERFACE_MODE_MASK;
	if ((ugeth->max_speed == SPEED_10) ||
	    (ugeth->max_speed == SPEED_100))
		maccfg2 |= MACCFG2_INTERFACE_MODE_NIBBLE;
	else if (ugeth->max_speed == SPEED_1000)
		maccfg2 |= MACCFG2_INTERFACE_MODE_BYTE;
	maccfg2 |= ug_info->padAndCrc;
	out_be32(&ug_regs->maccfg2, maccfg2);

	/*                    Set UPSMR                      */
	upsmr = in_be32(&uf_regs->upsmr);
	upsmr &= ~(UCC_GETH_UPSMR_RPM | UCC_GETH_UPSMR_R10M |
		   UCC_GETH_UPSMR_TBIM | UCC_GETH_UPSMR_RMM);
	if ((ugeth->phy_interface == PHY_INTERFACE_MODE_RMII) ||
	    (ugeth->phy_interface == PHY_INTERFACE_MODE_RGMII) ||
	    (ugeth->phy_interface == PHY_INTERFACE_MODE_RGMII_ID) ||
	    (ugeth->phy_interface == PHY_INTERFACE_MODE_RGMII_RXID) ||
	    (ugeth->phy_interface == PHY_INTERFACE_MODE_RGMII_TXID) ||
	    (ugeth->phy_interface == PHY_INTERFACE_MODE_RTBI)) {
		if (ugeth->phy_interface != PHY_INTERFACE_MODE_RMII)
			upsmr |= UCC_GETH_UPSMR_RPM;
		switch (ugeth->max_speed) {
		case SPEED_10:
			upsmr |= UCC_GETH_UPSMR_R10M;
			/* FALLTHROUGH */
		case SPEED_100:
			if (ugeth->phy_interface != PHY_INTERFACE_MODE_RTBI)
				upsmr |= UCC_GETH_UPSMR_RMM;
		}
	}
	if ((ugeth->phy_interface == PHY_INTERFACE_MODE_TBI) ||
	    (ugeth->phy_interface == PHY_INTERFACE_MODE_RTBI)) {
		upsmr |= UCC_GETH_UPSMR_TBIM;
	}
	if ((ugeth->phy_interface == PHY_INTERFACE_MODE_SGMII))
		upsmr |= UCC_GETH_UPSMR_SGMM;

	out_be32(&uf_regs->upsmr, upsmr);

	/* Disable autonegotiation in tbi mode, because by default it
	comes up in autonegotiation mode. */
	/* Note that this depends on proper setting in utbipar register. */
	if ((ugeth->phy_interface == PHY_INTERFACE_MODE_TBI) ||
	    (ugeth->phy_interface == PHY_INTERFACE_MODE_RTBI)) {
		tbiBaseAddress = in_be32(&ug_regs->utbipar);
		tbiBaseAddress &= UTBIPAR_PHY_ADDRESS_MASK;
		tbiBaseAddress >>= UTBIPAR_PHY_ADDRESS_SHIFT;
		value = ugeth->phydev->bus->read(ugeth->phydev->bus,
				(u8) tbiBaseAddress, ENET_TBI_MII_CR);
		value &= ~0x1000;	/* Turn off autonegotiation */
		ugeth->phydev->bus->write(ugeth->phydev->bus,
				(u8) tbiBaseAddress, ENET_TBI_MII_CR, value);
	}

	init_check_frame_length_mode(ug_info->lengthCheckRx, &ug_regs->maccfg2);

	ret_val = init_preamble_length(ug_info->prel, &ug_regs->maccfg2);
	if (ret_val != 0) {
		if (netif_msg_probe(ugeth))
			ugeth_err("%s: Preamble length must be between 3 and 7 inclusive.",
			     __func__);
		return ret_val;
	}

	return 0;
}

static int ugeth_graceful_stop_tx(struct ucc_geth_private *ugeth)
{
	struct ucc_fast_private *uccf;
	u32 cecr_subblock;
	u32 temp;
	int i = 10;

	uccf = ugeth->uccf;

	/* Mask GRACEFUL STOP TX interrupt bit and clear it */
	clrbits32(uccf->p_uccm, UCC_GETH_UCCE_GRA);
	out_be32(uccf->p_ucce, UCC_GETH_UCCE_GRA);  /* clear by writing 1 */

	/* Issue host command */
	cecr_subblock =
	    ucc_fast_get_qe_cr_subblock(ugeth->ug_info->uf_info.ucc_num);
	qe_issue_cmd(QE_GRACEFUL_STOP_TX, cecr_subblock,
		     QE_CR_PROTOCOL_ETHERNET, 0);

	/* Wait for command to complete */
	do {
		msleep(10);
		temp = in_be32(uccf->p_ucce);
	} while (!(temp & UCC_GETH_UCCE_GRA) && --i);

	uccf->stopped_tx = 1;

	return 0;
}

static int ugeth_graceful_stop_rx(struct ucc_geth_private *ugeth)
{
	struct ucc_fast_private *uccf;
	u32 cecr_subblock;
	u8 temp;
	int i = 10;

	uccf = ugeth->uccf;

	/* Clear acknowledge bit */
	temp = in_8(&ugeth->p_rx_glbl_pram->rxgstpack);
	temp &= ~GRACEFUL_STOP_ACKNOWLEDGE_RX;
	out_8(&ugeth->p_rx_glbl_pram->rxgstpack, temp);

	/* Keep issuing command and checking acknowledge bit until
	it is asserted, according to spec */
	do {
		/* Issue host command */
		cecr_subblock =
		    ucc_fast_get_qe_cr_subblock(ugeth->ug_info->uf_info.
						ucc_num);
		qe_issue_cmd(QE_GRACEFUL_STOP_RX, cecr_subblock,
			     QE_CR_PROTOCOL_ETHERNET, 0);
		msleep(10);
		temp = in_8(&ugeth->p_rx_glbl_pram->rxgstpack);
	} while (!(temp & GRACEFUL_STOP_ACKNOWLEDGE_RX) && --i);

	uccf->stopped_rx = 1;

	return 0;
}

static int ugeth_restart_tx(struct ucc_geth_private *ugeth)
{
	struct ucc_fast_private *uccf;
	u32 cecr_subblock;

	uccf = ugeth->uccf;

	cecr_subblock =
	    ucc_fast_get_qe_cr_subblock(ugeth->ug_info->uf_info.ucc_num);
	qe_issue_cmd(QE_RESTART_TX, cecr_subblock, QE_CR_PROTOCOL_ETHERNET, 0);
	uccf->stopped_tx = 0;

	return 0;
}

static int ugeth_restart_rx(struct ucc_geth_private *ugeth)
{
	struct ucc_fast_private *uccf;
	u32 cecr_subblock;

	uccf = ugeth->uccf;

	cecr_subblock =
	    ucc_fast_get_qe_cr_subblock(ugeth->ug_info->uf_info.ucc_num);
	qe_issue_cmd(QE_RESTART_RX, cecr_subblock, QE_CR_PROTOCOL_ETHERNET,
		     0);
	uccf->stopped_rx = 0;

	return 0;
}

static int ugeth_enable(struct ucc_geth_private *ugeth, enum comm_dir mode)
{
	struct ucc_fast_private *uccf;
	int enabled_tx, enabled_rx;

	uccf = ugeth->uccf;

	/* check if the UCC number is in range. */
	if (ugeth->ug_info->uf_info.ucc_num >= UCC_MAX_NUM) {
		if (netif_msg_probe(ugeth))
			ugeth_err("%s: ucc_num out of range.", __func__);
		return -EINVAL;
	}

	enabled_tx = uccf->enabled_tx;
	enabled_rx = uccf->enabled_rx;

	/* Get Tx and Rx going again, in case this channel was actively
	disabled. */
	if ((mode & COMM_DIR_TX) && (!enabled_tx) && uccf->stopped_tx)
		ugeth_restart_tx(ugeth);
	if ((mode & COMM_DIR_RX) && (!enabled_rx) && uccf->stopped_rx)
		ugeth_restart_rx(ugeth);

	ucc_fast_enable(uccf, mode);	/* OK to do even if not disabled */

	return 0;

}

static int ugeth_disable(struct ucc_geth_private *ugeth, enum comm_dir mode)
{
	struct ucc_fast_private *uccf;

	uccf = ugeth->uccf;

	/* check if the UCC number is in range. */
	if (ugeth->ug_info->uf_info.ucc_num >= UCC_MAX_NUM) {
		if (netif_msg_probe(ugeth))
			ugeth_err("%s: ucc_num out of range.", __func__);
		return -EINVAL;
	}

	/* Stop any transmissions */
	if ((mode & COMM_DIR_TX) && uccf->enabled_tx && !uccf->stopped_tx)
		ugeth_graceful_stop_tx(ugeth);

	/* Stop any receptions */
	if ((mode & COMM_DIR_RX) && uccf->enabled_rx && !uccf->stopped_rx)
		ugeth_graceful_stop_rx(ugeth);

	ucc_fast_disable(ugeth->uccf, mode); /* OK to do even if not enabled */

	return 0;
}

static void ugeth_quiesce(struct ucc_geth_private *ugeth)
{
	/* Wait for and prevent any further xmits. */
	netif_tx_disable(ugeth->ndev);

	/* Disable the interrupt to avoid NAPI rescheduling. */
	disable_irq(ugeth->ug_info->uf_info.irq);

	/* Stop NAPI, and possibly wait for its completion. */
	napi_disable(&ugeth->napi);
}

static void ugeth_activate(struct ucc_geth_private *ugeth)
{
	napi_enable(&ugeth->napi);
	enable_irq(ugeth->ug_info->uf_info.irq);
	netif_tx_wake_all_queues(ugeth->ndev);
}

/* Called every time the controller might need to be made
 * aware of new link state.  The PHY code conveys this
 * information through variables in the ugeth structure, and this
 * function converts those variables into the appropriate
 * register values, and can bring down the device if needed.
 */

static void adjust_link(struct net_device *dev)
{
	struct ucc_geth_private *ugeth = netdev_priv(dev);
	struct ucc_geth __iomem *ug_regs;
	struct ucc_fast __iomem *uf_regs;
	struct phy_device *phydev = ugeth->phydev;
	int new_state = 0;

	ug_regs = ugeth->ug_regs;
	uf_regs = ugeth->uccf->uf_regs;

	if (phydev->link) {
		u32 tempval = in_be32(&ug_regs->maccfg2);
		u32 upsmr = in_be32(&uf_regs->upsmr);
		/* Now we make sure that we can be in full duplex mode.
		 * If not, we operate in half-duplex mode. */
		if (phydev->duplex != ugeth->oldduplex) {
			new_state = 1;
			if (!(phydev->duplex))
				tempval &= ~(MACCFG2_FDX);
			else
				tempval |= MACCFG2_FDX;
			ugeth->oldduplex = phydev->duplex;
		}

		if (phydev->speed != ugeth->oldspeed) {
			new_state = 1;
			switch (phydev->speed) {
			case SPEED_1000:
				tempval = ((tempval &
					    ~(MACCFG2_INTERFACE_MODE_MASK)) |
					    MACCFG2_INTERFACE_MODE_BYTE);
				break;
			case SPEED_100:
			case SPEED_10:
				tempval = ((tempval &
					    ~(MACCFG2_INTERFACE_MODE_MASK)) |
					    MACCFG2_INTERFACE_MODE_NIBBLE);
				/* if reduced mode, re-set UPSMR.R10M */
				if ((ugeth->phy_interface == PHY_INTERFACE_MODE_RMII) ||
				    (ugeth->phy_interface == PHY_INTERFACE_MODE_RGMII) ||
				    (ugeth->phy_interface == PHY_INTERFACE_MODE_RGMII_ID) ||
				    (ugeth->phy_interface == PHY_INTERFACE_MODE_RGMII_RXID) ||
				    (ugeth->phy_interface == PHY_INTERFACE_MODE_RGMII_TXID) ||
				    (ugeth->phy_interface == PHY_INTERFACE_MODE_RTBI)) {
					if (phydev->speed == SPEED_10)
						upsmr |= UCC_GETH_UPSMR_R10M;
					else
						upsmr &= ~UCC_GETH_UPSMR_R10M;
				}
				break;
			default:
				if (netif_msg_link(ugeth))
					ugeth_warn(
						"%s: Ack!  Speed (%d) is not 10/100/1000!",
						dev->name, phydev->speed);
				break;
			}
			ugeth->oldspeed = phydev->speed;
		}

		/*
		 * To change the MAC configuration we need to disable the
		 * controller. To do so, we have to either grab ugeth->lock,
		 * which is a bad idea since 'graceful stop' commands might
		 * take quite a while, or we can quiesce driver's activity.
		 */
		ugeth_quiesce(ugeth);
		ugeth_disable(ugeth, COMM_DIR_RX_AND_TX);

		out_be32(&ug_regs->maccfg2, tempval);
		out_be32(&uf_regs->upsmr, upsmr);

		ugeth_enable(ugeth, COMM_DIR_RX_AND_TX);
		ugeth_activate(ugeth);

		if (!ugeth->oldlink) {
			new_state = 1;
			ugeth->oldlink = 1;
		}
	} else if (ugeth->oldlink) {
			new_state = 1;
			ugeth->oldlink = 0;
			ugeth->oldspeed = 0;
			ugeth->oldduplex = -1;
	}

	if (new_state && netif_msg_link(ugeth))
		phy_print_status(phydev);
}

/* Initialize TBI PHY interface for communicating with the
 * SERDES lynx PHY on the chip.  We communicate with this PHY
 * through the MDIO bus on each controller, treating it as a
 * "normal" PHY at the address found in the UTBIPA register.  We assume
 * that the UTBIPA register is valid.  Either the MDIO bus code will set
 * it to a value that doesn't conflict with other PHYs on the bus, or the
 * value doesn't matter, as there are no other PHYs on the bus.
 */
static void uec_configure_serdes(struct net_device *dev)
{
	struct ucc_geth_private *ugeth = netdev_priv(dev);
	struct ucc_geth_info *ug_info = ugeth->ug_info;
	struct phy_device *tbiphy;

	if (!ug_info->tbi_node) {
		dev_warn(&dev->dev, "SGMII mode requires that the device "
			"tree specify a tbi-handle\n");
		return;
	}

	tbiphy = of_phy_find_device(ug_info->tbi_node);
	if (!tbiphy) {
		dev_err(&dev->dev, "error: Could not get TBI device\n");
		return;
	}

	/*
	 * If the link is already up, we must already be ok, and don't need to
	 * configure and reset the TBI<->SerDes link.  Maybe U-Boot configured
	 * everything for us?  Resetting it takes the link down and requires
	 * several seconds for it to come back.
	 */
	if (phy_read(tbiphy, ENET_TBI_MII_SR) & TBISR_LSTATUS)
		return;

	/* Single clk mode, mii mode off(for serdes communication) */
	phy_write(tbiphy, ENET_TBI_MII_ANA, TBIANA_SETTINGS);

	phy_write(tbiphy, ENET_TBI_MII_TBICON, TBICON_CLK_SELECT);

	phy_write(tbiphy, ENET_TBI_MII_CR, TBICR_SETTINGS);
}

/* Configure the PHY for dev.
 * returns 0 if success.  -1 if failure
 */
static int init_phy(struct net_device *dev)
{
	struct ucc_geth_private *priv = netdev_priv(dev);
	struct ucc_geth_info *ug_info = priv->ug_info;
	struct phy_device *phydev;

	priv->oldlink = 0;
	priv->oldspeed = 0;
	priv->oldduplex = -1;

	phydev = of_phy_connect(dev, ug_info->phy_node, &adjust_link, 0,
				priv->phy_interface);
	if (!phydev)
		phydev = of_phy_connect_fixed_link(dev, &adjust_link,
						   priv->phy_interface);
	if (!phydev) {
		dev_err(&dev->dev, "Could not attach to PHY\n");
		return -ENODEV;
	}

	if (priv->phy_interface == PHY_INTERFACE_MODE_SGMII)
		uec_configure_serdes(dev);

	phydev->supported &= (ADVERTISED_10baseT_Half |
				 ADVERTISED_10baseT_Full |
				 ADVERTISED_100baseT_Half |
				 ADVERTISED_100baseT_Full);

	if (priv->max_speed == SPEED_1000)
		phydev->supported |= ADVERTISED_1000baseT_Full;

	phydev->advertising = phydev->supported;

	priv->phydev = phydev;

	return 0;
}

static void ugeth_dump_regs(struct ucc_geth_private *ugeth)
{
#ifdef DEBUG
	ucc_fast_dump_regs(ugeth->uccf);
	dump_regs(ugeth);
	dump_bds(ugeth);
#endif
}

static int ugeth_82xx_filtering_clear_all_addr_in_hash(struct ucc_geth_private *
						       ugeth,
						       enum enet_addr_type
						       enet_addr_type)
{
	struct ucc_geth_82xx_address_filtering_pram __iomem *p_82xx_addr_filt;
	struct ucc_fast_private *uccf;
	enum comm_dir comm_dir;
	struct list_head *p_lh;
	u16 i, num;
	u32 __iomem *addr_h;
	u32 __iomem *addr_l;
	u8 *p_counter;

	uccf = ugeth->uccf;

	p_82xx_addr_filt =
	    (struct ucc_geth_82xx_address_filtering_pram __iomem *)
	    ugeth->p_rx_glbl_pram->addressfiltering;

	if (enet_addr_type == ENET_ADDR_TYPE_GROUP) {
		addr_h = &(p_82xx_addr_filt->gaddr_h);
		addr_l = &(p_82xx_addr_filt->gaddr_l);
		p_lh = &ugeth->group_hash_q;
		p_counter = &(ugeth->numGroupAddrInHash);
	} else if (enet_addr_type == ENET_ADDR_TYPE_INDIVIDUAL) {
		addr_h = &(p_82xx_addr_filt->iaddr_h);
		addr_l = &(p_82xx_addr_filt->iaddr_l);
		p_lh = &ugeth->ind_hash_q;
		p_counter = &(ugeth->numIndAddrInHash);
	} else
		return -EINVAL;

	comm_dir = 0;
	if (uccf->enabled_tx)
		comm_dir |= COMM_DIR_TX;
	if (uccf->enabled_rx)
		comm_dir |= COMM_DIR_RX;
	if (comm_dir)
		ugeth_disable(ugeth, comm_dir);

	/* Clear the hash table. */
	out_be32(addr_h, 0x00000000);
	out_be32(addr_l, 0x00000000);

	if (!p_lh)
		return 0;

	num = *p_counter;

	/* Delete all remaining CQ elements */
	for (i = 0; i < num; i++)
		put_enet_addr_container(ENET_ADDR_CONT_ENTRY(dequeue(p_lh)));

	*p_counter = 0;

	if (comm_dir)
		ugeth_enable(ugeth, comm_dir);

	return 0;
}

static int ugeth_82xx_filtering_clear_addr_in_paddr(struct ucc_geth_private *ugeth,
						    u8 paddr_num)
{
	ugeth->indAddrRegUsed[paddr_num] = 0; /* mark this paddr as not used */
	return hw_clear_addr_in_paddr(ugeth, paddr_num);/* clear in hardware */
}

static void ucc_geth_memclean(struct ucc_geth_private *ugeth)
{
	u16 i, j;
	u8 __iomem *bd;

	if (!ugeth)
		return;

	if (ugeth->uccf) {
		ucc_fast_free(ugeth->uccf);
		ugeth->uccf = NULL;
	}

	if (ugeth->p_thread_data_tx) {
		qe_muram_free(ugeth->thread_dat_tx_offset);
		ugeth->p_thread_data_tx = NULL;
	}
	if (ugeth->p_thread_data_rx) {
		qe_muram_free(ugeth->thread_dat_rx_offset);
		ugeth->p_thread_data_rx = NULL;
	}
	if (ugeth->p_exf_glbl_param) {
		qe_muram_free(ugeth->exf_glbl_param_offset);
		ugeth->p_exf_glbl_param = NULL;
	}
	if (ugeth->p_rx_glbl_pram) {
		qe_muram_free(ugeth->rx_glbl_pram_offset);
		ugeth->p_rx_glbl_pram = NULL;
	}
	if (ugeth->p_tx_glbl_pram) {
		qe_muram_free(ugeth->tx_glbl_pram_offset);
		ugeth->p_tx_glbl_pram = NULL;
	}
	if (ugeth->p_send_q_mem_reg) {
		qe_muram_free(ugeth->send_q_mem_reg_offset);
		ugeth->p_send_q_mem_reg = NULL;
	}
	if (ugeth->p_scheduler) {
		qe_muram_free(ugeth->scheduler_offset);
		ugeth->p_scheduler = NULL;
	}
	if (ugeth->p_tx_fw_statistics_pram) {
		qe_muram_free(ugeth->tx_fw_statistics_pram_offset);
		ugeth->p_tx_fw_statistics_pram = NULL;
	}
	if (ugeth->p_rx_fw_statistics_pram) {
		qe_muram_free(ugeth->rx_fw_statistics_pram_offset);
		ugeth->p_rx_fw_statistics_pram = NULL;
	}
	if (ugeth->p_rx_irq_coalescing_tbl) {
		qe_muram_free(ugeth->rx_irq_coalescing_tbl_offset);
		ugeth->p_rx_irq_coalescing_tbl = NULL;
	}
	if (ugeth->p_rx_bd_qs_tbl) {
		qe_muram_free(ugeth->rx_bd_qs_tbl_offset);
		ugeth->p_rx_bd_qs_tbl = NULL;
	}
	if (ugeth->p_init_enet_param_shadow) {
		return_init_enet_entries(ugeth,
					 &(ugeth->p_init_enet_param_shadow->
					   rxthread[0]),
					 ENET_INIT_PARAM_MAX_ENTRIES_RX,
					 ugeth->ug_info->riscRx, 1);
		return_init_enet_entries(ugeth,
					 &(ugeth->p_init_enet_param_shadow->
					   txthread[0]),
					 ENET_INIT_PARAM_MAX_ENTRIES_TX,
					 ugeth->ug_info->riscTx, 0);
		kfree(ugeth->p_init_enet_param_shadow);
		ugeth->p_init_enet_param_shadow = NULL;
	}
	for (i = 0; i < ugeth->ug_info->numQueuesTx; i++) {
		bd = ugeth->p_tx_bd_ring[i];
		if (!bd)
			continue;
		for (j = 0; j < ugeth->ug_info->bdRingLenTx[i]; j++) {
			if (ugeth->tx_skbuff[i][j]) {
				dma_unmap_single(ugeth->dev,
						 in_be32(&((struct qe_bd __iomem *)bd)->buf),
						 (in_be32((u32 __iomem *)bd) &
						  BD_LENGTH_MASK),
						 DMA_TO_DEVICE);
				dev_kfree_skb_any(ugeth->tx_skbuff[i][j]);
				ugeth->tx_skbuff[i][j] = NULL;
			}
		}

		kfree(ugeth->tx_skbuff[i]);

		if (ugeth->p_tx_bd_ring[i]) {
			if (ugeth->ug_info->uf_info.bd_mem_part ==
			    MEM_PART_SYSTEM)
				kfree((void *)ugeth->tx_bd_ring_offset[i]);
			else if (ugeth->ug_info->uf_info.bd_mem_part ==
				 MEM_PART_MURAM)
				qe_muram_free(ugeth->tx_bd_ring_offset[i]);
			ugeth->p_tx_bd_ring[i] = NULL;
		}
	}
	for (i = 0; i < ugeth->ug_info->numQueuesRx; i++) {
		if (ugeth->p_rx_bd_ring[i]) {
			/* Return existing data buffers in ring */
			bd = ugeth->p_rx_bd_ring[i];
			for (j = 0; j < ugeth->ug_info->bdRingLenRx[i]; j++) {
				if (ugeth->rx_skbuff[i][j]) {
					dma_unmap_single(ugeth->dev,
						in_be32(&((struct qe_bd __iomem *)bd)->buf),
						ugeth->ug_info->
						uf_info.max_rx_buf_length +
						UCC_GETH_RX_DATA_BUF_ALIGNMENT,
						DMA_FROM_DEVICE);
					dev_kfree_skb_any(
						ugeth->rx_skbuff[i][j]);
					ugeth->rx_skbuff[i][j] = NULL;
				}
				bd += sizeof(struct qe_bd);
			}

			kfree(ugeth->rx_skbuff[i]);

			if (ugeth->ug_info->uf_info.bd_mem_part ==
			    MEM_PART_SYSTEM)
				kfree((void *)ugeth->rx_bd_ring_offset[i]);
			else if (ugeth->ug_info->uf_info.bd_mem_part ==
				 MEM_PART_MURAM)
				qe_muram_free(ugeth->rx_bd_ring_offset[i]);
			ugeth->p_rx_bd_ring[i] = NULL;
		}
	}
	while (!list_empty(&ugeth->group_hash_q))
		put_enet_addr_container(ENET_ADDR_CONT_ENTRY
					(dequeue(&ugeth->group_hash_q)));
	while (!list_empty(&ugeth->ind_hash_q))
		put_enet_addr_container(ENET_ADDR_CONT_ENTRY
					(dequeue(&ugeth->ind_hash_q)));
	if (ugeth->ug_regs) {
		iounmap(ugeth->ug_regs);
		ugeth->ug_regs = NULL;
	}

	skb_queue_purge(&ugeth->rx_recycle);
}

static void ucc_geth_set_multi(struct net_device *dev)
{
	struct ucc_geth_private *ugeth;
	struct dev_mc_list *dmi;
	struct ucc_fast __iomem *uf_regs;
	struct ucc_geth_82xx_address_filtering_pram __iomem *p_82xx_addr_filt;
	int i;

	ugeth = netdev_priv(dev);

	uf_regs = ugeth->uccf->uf_regs;

	if (dev->flags & IFF_PROMISC) {
		setbits32(&uf_regs->upsmr, UCC_GETH_UPSMR_PRO);
	} else {
		clrbits32(&uf_regs->upsmr, UCC_GETH_UPSMR_PRO);

		p_82xx_addr_filt =
		    (struct ucc_geth_82xx_address_filtering_pram __iomem *) ugeth->
		    p_rx_glbl_pram->addressfiltering;

		if (dev->flags & IFF_ALLMULTI) {
			/* Catch all multicast addresses, so set the
			 * filter to all 1's.
			 */
			out_be32(&p_82xx_addr_filt->gaddr_h, 0xffffffff);
			out_be32(&p_82xx_addr_filt->gaddr_l, 0xffffffff);
		} else {
			/* Clear filter and add the addresses in the list.
			 */
			out_be32(&p_82xx_addr_filt->gaddr_h, 0x0);
			out_be32(&p_82xx_addr_filt->gaddr_l, 0x0);

			dmi = dev->mc_list;

			for (i = 0; i < dev->mc_count; i++, dmi = dmi->next) {

				/* Only support group multicast for now.
				 */
				if (!(dmi->dmi_addr[0] & 1))
					continue;

				/* Ask CPM to run CRC and set bit in
				 * filter mask.
				 */
				hw_add_addr_in_hash(ugeth, dmi->dmi_addr);
			}
		}
	}
}

static void ucc_geth_stop(struct ucc_geth_private *ugeth)
{
	struct ucc_geth __iomem *ug_regs = ugeth->ug_regs;
	struct phy_device *phydev = ugeth->phydev;

	ugeth_vdbg("%s: IN", __func__);

	/* Disable the controller */
	ugeth_disable(ugeth, COMM_DIR_RX_AND_TX);

	/* Tell the kernel the link is down */
	phy_stop(phydev);

	/* Mask all interrupts */
	out_be32(ugeth->uccf->p_uccm, 0x00000000);

	/* Clear all interrupts */
	out_be32(ugeth->uccf->p_ucce, 0xffffffff);

	/* Disable Rx and Tx */
	clrbits32(&ug_regs->maccfg1, MACCFG1_ENABLE_RX | MACCFG1_ENABLE_TX);

	phy_disconnect(ugeth->phydev);
	ugeth->phydev = NULL;

	ucc_geth_memclean(ugeth);
}

static int ucc_struct_init(struct ucc_geth_private *ugeth)
{
	struct ucc_geth_info *ug_info;
	struct ucc_fast_info *uf_info;
	int i;

	ug_info = ugeth->ug_info;
	uf_info = &ug_info->uf_info;

	if (!((uf_info->bd_mem_part == MEM_PART_SYSTEM) ||
	      (uf_info->bd_mem_part == MEM_PART_MURAM))) {
		if (netif_msg_probe(ugeth))
			ugeth_err("%s: Bad memory partition value.",
					__func__);
		return -EINVAL;
	}

	/* Rx BD lengths */
	for (i = 0; i < ug_info->numQueuesRx; i++) {
		if ((ug_info->bdRingLenRx[i] < UCC_GETH_RX_BD_RING_SIZE_MIN) ||
		    (ug_info->bdRingLenRx[i] %
		     UCC_GETH_RX_BD_RING_SIZE_ALIGNMENT)) {
			if (netif_msg_probe(ugeth))
				ugeth_err
				    ("%s: Rx BD ring length must be multiple of 4, no smaller than 8.",
					__func__);
			return -EINVAL;
		}
	}

	/* Tx BD lengths */
	for (i = 0; i < ug_info->numQueuesTx; i++) {
		if (ug_info->bdRingLenTx[i] < UCC_GETH_TX_BD_RING_SIZE_MIN) {
			if (netif_msg_probe(ugeth))
				ugeth_err
				    ("%s: Tx BD ring length must be no smaller than 2.",
				     __func__);
			return -EINVAL;
		}
	}

	/* mrblr */
	if ((uf_info->max_rx_buf_length == 0) ||
	    (uf_info->max_rx_buf_length % UCC_GETH_MRBLR_ALIGNMENT)) {
		if (netif_msg_probe(ugeth))
			ugeth_err
			    ("%s: max_rx_buf_length must be non-zero multiple of 128.",
			     __func__);
		return -EINVAL;
	}

	/* num Tx queues */
	if (ug_info->numQueuesTx > NUM_TX_QUEUES) {
		if (netif_msg_probe(ugeth))
			ugeth_err("%s: number of tx queues too large.", __func__);
		return -EINVAL;
	}

	/* num Rx queues */
	if (ug_info->numQueuesRx > NUM_RX_QUEUES) {
		if (netif_msg_probe(ugeth))
			ugeth_err("%s: number of rx queues too large.", __func__);
		return -EINVAL;
	}

	/* l2qt */
	for (i = 0; i < UCC_GETH_VLAN_PRIORITY_MAX; i++) {
		if (ug_info->l2qt[i] >= ug_info->numQueuesRx) {
			if (netif_msg_probe(ugeth))
				ugeth_err
				    ("%s: VLAN priority table entry must not be"
					" larger than number of Rx queues.",
				     __func__);
			return -EINVAL;
		}
	}

	/* l3qt */
	for (i = 0; i < UCC_GETH_IP_PRIORITY_MAX; i++) {
		if (ug_info->l3qt[i] >= ug_info->numQueuesRx) {
			if (netif_msg_probe(ugeth))
				ugeth_err
				    ("%s: IP priority table entry must not be"
					" larger than number of Rx queues.",
				     __func__);
			return -EINVAL;
		}
	}

	if (ug_info->cam && !ug_info->ecamptr) {
		if (netif_msg_probe(ugeth))
			ugeth_err("%s: If cam mode is chosen, must supply cam ptr.",
				  __func__);
		return -EINVAL;
	}

	if ((ug_info->numStationAddresses !=
	     UCC_GETH_NUM_OF_STATION_ADDRESSES_1)
	    && ug_info->rxExtendedFiltering) {
		if (netif_msg_probe(ugeth))
			ugeth_err("%s: Number of station addresses greater than 1 "
				  "not allowed in extended parsing mode.",
				  __func__);
		return -EINVAL;
	}

	/* Generate uccm_mask for receive */
	uf_info->uccm_mask = ug_info->eventRegMask & UCCE_OTHER;/* Errors */
	for (i = 0; i < ug_info->numQueuesRx; i++)
		uf_info->uccm_mask |= (UCC_GETH_UCCE_RXF0 << i);

	for (i = 0; i < ug_info->numQueuesTx; i++)
		uf_info->uccm_mask |= (UCC_GETH_UCCE_TXB0 << i);
	/* Initialize the general fast UCC block. */
	if (ucc_fast_init(uf_info, &ugeth->uccf)) {
		if (netif_msg_probe(ugeth))
			ugeth_err("%s: Failed to init uccf.", __func__);
		return -ENOMEM;
	}

	/* read the number of risc engines, update the riscTx and riscRx
	 * if there are 4 riscs in QE
	 */
	if (qe_get_num_of_risc() == 4) {
		ug_info->riscTx = QE_RISC_ALLOCATION_FOUR_RISCS;
		ug_info->riscRx = QE_RISC_ALLOCATION_FOUR_RISCS;
	}

	ugeth->ug_regs = ioremap(uf_info->regs, sizeof(*ugeth->ug_regs));
	if (!ugeth->ug_regs) {
		if (netif_msg_probe(ugeth))
			ugeth_err("%s: Failed to ioremap regs.", __func__);
		return -ENOMEM;
	}

	skb_queue_head_init(&ugeth->rx_recycle);

	return 0;
}

static int ucc_geth_startup(struct ucc_geth_private *ugeth)
{
	struct ucc_geth_82xx_address_filtering_pram __iomem *p_82xx_addr_filt;
	struct ucc_geth_init_pram __iomem *p_init_enet_pram;
	struct ucc_fast_private *uccf;
	struct ucc_geth_info *ug_info;
	struct ucc_fast_info *uf_info;
	struct ucc_fast __iomem *uf_regs;
	struct ucc_geth __iomem *ug_regs;
	int ret_val = -EINVAL;
	u32 remoder = UCC_GETH_REMODER_INIT;
	u32 init_enet_pram_offset, cecr_subblock, command;
	u32 ifstat, i, j, size, l2qt, l3qt, length;
	u16 temoder = UCC_GETH_TEMODER_INIT;
	u16 test;
	u8 function_code = 0;
	u8 __iomem *bd;
	u8 __iomem *endOfRing;
	u8 numThreadsRxNumerical, numThreadsTxNumerical;

	ugeth_vdbg("%s: IN", __func__);
	uccf = ugeth->uccf;
	ug_info = ugeth->ug_info;
	uf_info = &ug_info->uf_info;
	uf_regs = uccf->uf_regs;
	ug_regs = ugeth->ug_regs;

	switch (ug_info->numThreadsRx) {
	case UCC_GETH_NUM_OF_THREADS_1:
		numThreadsRxNumerical = 1;
		break;
	case UCC_GETH_NUM_OF_THREADS_2:
		numThreadsRxNumerical = 2;
		break;
	case UCC_GETH_NUM_OF_THREADS_4:
		numThreadsRxNumerical = 4;
		break;
	case UCC_GETH_NUM_OF_THREADS_6:
		numThreadsRxNumerical = 6;
		break;
	case UCC_GETH_NUM_OF_THREADS_8:
		numThreadsRxNumerical = 8;
		break;
	default:
		if (netif_msg_ifup(ugeth))
			ugeth_err("%s: Bad number of Rx threads value.",
				       	__func__);
		return -EINVAL;
		break;
	}

	switch (ug_info->numThreadsTx) {
	case UCC_GETH_NUM_OF_THREADS_1:
		numThreadsTxNumerical = 1;
		break;
	case UCC_GETH_NUM_OF_THREADS_2:
		numThreadsTxNumerical = 2;
		break;
	case UCC_GETH_NUM_OF_THREADS_4:
		numThreadsTxNumerical = 4;
		break;
	case UCC_GETH_NUM_OF_THREADS_6:
		numThreadsTxNumerical = 6;
		break;
	case UCC_GETH_NUM_OF_THREADS_8:
		numThreadsTxNumerical = 8;
		break;
	default:
		if (netif_msg_ifup(ugeth))
			ugeth_err("%s: Bad number of Tx threads value.",
				       	__func__);
		return -EINVAL;
		break;
	}

	/* Calculate rx_extended_features */
	ugeth->rx_non_dynamic_extended_features = ug_info->ipCheckSumCheck ||
	    ug_info->ipAddressAlignment ||
	    (ug_info->numStationAddresses !=
	     UCC_GETH_NUM_OF_STATION_ADDRESSES_1);

	ugeth->rx_extended_features = ugeth->rx_non_dynamic_extended_features ||
	    (ug_info->vlanOperationTagged != UCC_GETH_VLAN_OPERATION_TAGGED_NOP)
	    || (ug_info->vlanOperationNonTagged !=
		UCC_GETH_VLAN_OPERATION_NON_TAGGED_NOP);

	init_default_reg_vals(&uf_regs->upsmr,
			      &ug_regs->maccfg1, &ug_regs->maccfg2);

	/*                    Set UPSMR                      */
	/* For more details see the hardware spec.           */
	init_rx_parameters(ug_info->bro,
			   ug_info->rsh, ug_info->pro, &uf_regs->upsmr);

	/* We're going to ignore other registers for now, */
	/* except as needed to get up and running         */

	/*                    Set MACCFG1                    */
	/* For more details see the hardware spec.           */
	init_flow_control_params(ug_info->aufc,
				 ug_info->receiveFlowControl,
				 ug_info->transmitFlowControl,
				 ug_info->pausePeriod,
				 ug_info->extensionField,
				 &uf_regs->upsmr,
				 &ug_regs->uempr, &ug_regs->maccfg1);

	setbits32(&ug_regs->maccfg1, MACCFG1_ENABLE_RX | MACCFG1_ENABLE_TX);

	/*                    Set IPGIFG                     */
	/* For more details see the hardware spec.           */
	ret_val = init_inter_frame_gap_params(ug_info->nonBackToBackIfgPart1,
					      ug_info->nonBackToBackIfgPart2,
					      ug_info->
					      miminumInterFrameGapEnforcement,
					      ug_info->backToBackInterFrameGap,
					      &ug_regs->ipgifg);
	if (ret_val != 0) {
		if (netif_msg_ifup(ugeth))
			ugeth_err("%s: IPGIFG initialization parameter too large.",
				  __func__);
		return ret_val;
	}

	/*                    Set HAFDUP                     */
	/* For more details see the hardware spec.           */
	ret_val = init_half_duplex_params(ug_info->altBeb,
					  ug_info->backPressureNoBackoff,
					  ug_info->noBackoff,
					  ug_info->excessDefer,
					  ug_info->altBebTruncation,
					  ug_info->maxRetransmission,
					  ug_info->collisionWindow,
					  &ug_regs->hafdup);
	if (ret_val != 0) {
		if (netif_msg_ifup(ugeth))
			ugeth_err("%s: Half Duplex initialization parameter too large.",
			  __func__);
		return ret_val;
	}

	/*                    Set IFSTAT                     */
	/* For more details see the hardware spec.           */
	/* Read only - resets upon read                      */
	ifstat = in_be32(&ug_regs->ifstat);

	/*                    Clear UEMPR                    */
	/* For more details see the hardware spec.           */
	out_be32(&ug_regs->uempr, 0);

	/*                    Set UESCR                      */
	/* For more details see the hardware spec.           */
	init_hw_statistics_gathering_mode((ug_info->statisticsMode &
				UCC_GETH_STATISTICS_GATHERING_MODE_HARDWARE),
				0, &uf_regs->upsmr, &ug_regs->uescr);

	/* Allocate Tx bds */
	for (j = 0; j < ug_info->numQueuesTx; j++) {
		/* Allocate in multiple of
		   UCC_GETH_TX_BD_RING_SIZE_MEMORY_ALIGNMENT,
		   according to spec */
		length = ((ug_info->bdRingLenTx[j] * sizeof(struct qe_bd))
			  / UCC_GETH_TX_BD_RING_SIZE_MEMORY_ALIGNMENT)
		    * UCC_GETH_TX_BD_RING_SIZE_MEMORY_ALIGNMENT;
		if ((ug_info->bdRingLenTx[j] * sizeof(struct qe_bd)) %
		    UCC_GETH_TX_BD_RING_SIZE_MEMORY_ALIGNMENT)
			length += UCC_GETH_TX_BD_RING_SIZE_MEMORY_ALIGNMENT;
		if (uf_info->bd_mem_part == MEM_PART_SYSTEM) {
			u32 align = 4;
			if (UCC_GETH_TX_BD_RING_ALIGNMENT > 4)
				align = UCC_GETH_TX_BD_RING_ALIGNMENT;
			ugeth->tx_bd_ring_offset[j] =
				(u32) kmalloc((u32) (length + align), GFP_KERNEL);

			if (ugeth->tx_bd_ring_offset[j] != 0)
				ugeth->p_tx_bd_ring[j] =
					(u8 __iomem *)((ugeth->tx_bd_ring_offset[j] +
					align) & ~(align - 1));
		} else if (uf_info->bd_mem_part == MEM_PART_MURAM) {
			ugeth->tx_bd_ring_offset[j] =
			    qe_muram_alloc(length,
					   UCC_GETH_TX_BD_RING_ALIGNMENT);
			if (!IS_ERR_VALUE(ugeth->tx_bd_ring_offset[j]))
				ugeth->p_tx_bd_ring[j] =
				    (u8 __iomem *) qe_muram_addr(ugeth->
							 tx_bd_ring_offset[j]);
		}
		if (!ugeth->p_tx_bd_ring[j]) {
			if (netif_msg_ifup(ugeth))
				ugeth_err
				    ("%s: Can not allocate memory for Tx bd rings.",
				     __func__);
			return -ENOMEM;
		}
		/* Zero unused end of bd ring, according to spec */
		memset_io((void __iomem *)(ugeth->p_tx_bd_ring[j] +
		       ug_info->bdRingLenTx[j] * sizeof(struct qe_bd)), 0,
		       length - ug_info->bdRingLenTx[j] * sizeof(struct qe_bd));
	}

	/* Allocate Rx bds */
	for (j = 0; j < ug_info->numQueuesRx; j++) {
		length = ug_info->bdRingLenRx[j] * sizeof(struct qe_bd);
		if (uf_info->bd_mem_part == MEM_PART_SYSTEM) {
			u32 align = 4;
			if (UCC_GETH_RX_BD_RING_ALIGNMENT > 4)
				align = UCC_GETH_RX_BD_RING_ALIGNMENT;
			ugeth->rx_bd_ring_offset[j] =
				(u32) kmalloc((u32) (length + align), GFP_KERNEL);
			if (ugeth->rx_bd_ring_offset[j] != 0)
				ugeth->p_rx_bd_ring[j] =
					(u8 __iomem *)((ugeth->rx_bd_ring_offset[j] +
					align) & ~(align - 1));
		} else if (uf_info->bd_mem_part == MEM_PART_MURAM) {
			ugeth->rx_bd_ring_offset[j] =
			    qe_muram_alloc(length,
					   UCC_GETH_RX_BD_RING_ALIGNMENT);
			if (!IS_ERR_VALUE(ugeth->rx_bd_ring_offset[j]))
				ugeth->p_rx_bd_ring[j] =
				    (u8 __iomem *) qe_muram_addr(ugeth->
							 rx_bd_ring_offset[j]);
		}
		if (!ugeth->p_rx_bd_ring[j]) {
			if (netif_msg_ifup(ugeth))
				ugeth_err
				    ("%s: Can not allocate memory for Rx bd rings.",
				     __func__);
			return -ENOMEM;
		}
	}

	/* Init Tx bds */
	for (j = 0; j < ug_info->numQueuesTx; j++) {
		/* Setup the skbuff rings */
		ugeth->tx_skbuff[j] = kmalloc(sizeof(struct sk_buff *) *
					      ugeth->ug_info->bdRingLenTx[j],
					      GFP_KERNEL);

		if (ugeth->tx_skbuff[j] == NULL) {
			if (netif_msg_ifup(ugeth))
				ugeth_err("%s: Could not allocate tx_skbuff",
					  __func__);
			return -ENOMEM;
		}

		for (i = 0; i < ugeth->ug_info->bdRingLenTx[j]; i++)
			ugeth->tx_skbuff[j][i] = NULL;

		ugeth->skb_curtx[j] = ugeth->skb_dirtytx[j] = 0;
		bd = ugeth->confBd[j] = ugeth->txBd[j] = ugeth->p_tx_bd_ring[j];
		for (i = 0; i < ug_info->bdRingLenTx[j]; i++) {
			/* clear bd buffer */
			out_be32(&((struct qe_bd __iomem *)bd)->buf, 0);
			/* set bd status and length */
			out_be32((u32 __iomem *)bd, 0);
			bd += sizeof(struct qe_bd);
		}
		bd -= sizeof(struct qe_bd);
		/* set bd status and length */
		out_be32((u32 __iomem *)bd, T_W); /* for last BD set Wrap bit */
	}

	/* Init Rx bds */
	for (j = 0; j < ug_info->numQueuesRx; j++) {
		/* Setup the skbuff rings */
		ugeth->rx_skbuff[j] = kmalloc(sizeof(struct sk_buff *) *
					      ugeth->ug_info->bdRingLenRx[j],
					      GFP_KERNEL);

		if (ugeth->rx_skbuff[j] == NULL) {
			if (netif_msg_ifup(ugeth))
				ugeth_err("%s: Could not allocate rx_skbuff",
					  __func__);
			return -ENOMEM;
		}

		for (i = 0; i < ugeth->ug_info->bdRingLenRx[j]; i++)
			ugeth->rx_skbuff[j][i] = NULL;

		ugeth->skb_currx[j] = 0;
		bd = ugeth->rxBd[j] = ugeth->p_rx_bd_ring[j];
		for (i = 0; i < ug_info->bdRingLenRx[j]; i++) {
			/* set bd status and length */
			out_be32((u32 __iomem *)bd, R_I);
			/* clear bd buffer */
			out_be32(&((struct qe_bd __iomem *)bd)->buf, 0);
			bd += sizeof(struct qe_bd);
		}
		bd -= sizeof(struct qe_bd);
		/* set bd status and length */
		out_be32((u32 __iomem *)bd, R_W); /* for last BD set Wrap bit */
	}

	/*
	 * Global PRAM
	 */
	/* Tx global PRAM */
	/* Allocate global tx parameter RAM page */
	ugeth->tx_glbl_pram_offset =
	    qe_muram_alloc(sizeof(struct ucc_geth_tx_global_pram),
			   UCC_GETH_TX_GLOBAL_PRAM_ALIGNMENT);
	if (IS_ERR_VALUE(ugeth->tx_glbl_pram_offset)) {
		if (netif_msg_ifup(ugeth))
			ugeth_err
			    ("%s: Can not allocate DPRAM memory for p_tx_glbl_pram.",
			     __func__);
		return -ENOMEM;
	}
	ugeth->p_tx_glbl_pram =
	    (struct ucc_geth_tx_global_pram __iomem *) qe_muram_addr(ugeth->
							tx_glbl_pram_offset);
	/* Zero out p_tx_glbl_pram */
	memset_io((void __iomem *)ugeth->p_tx_glbl_pram, 0, sizeof(struct ucc_geth_tx_global_pram));

	/* Fill global PRAM */

	/* TQPTR */
	/* Size varies with number of Tx threads */
	ugeth->thread_dat_tx_offset =
	    qe_muram_alloc(numThreadsTxNumerical *
			   sizeof(struct ucc_geth_thread_data_tx) +
			   32 * (numThreadsTxNumerical == 1),
			   UCC_GETH_THREAD_DATA_ALIGNMENT);
	if (IS_ERR_VALUE(ugeth->thread_dat_tx_offset)) {
		if (netif_msg_ifup(ugeth))
			ugeth_err
			    ("%s: Can not allocate DPRAM memory for p_thread_data_tx.",
			     __func__);
		return -ENOMEM;
	}

	ugeth->p_thread_data_tx =
	    (struct ucc_geth_thread_data_tx __iomem *) qe_muram_addr(ugeth->
							thread_dat_tx_offset);
	out_be32(&ugeth->p_tx_glbl_pram->tqptr, ugeth->thread_dat_tx_offset);

	/* vtagtable */
	for (i = 0; i < UCC_GETH_TX_VTAG_TABLE_ENTRY_MAX; i++)
		out_be32(&ugeth->p_tx_glbl_pram->vtagtable[i],
			 ug_info->vtagtable[i]);

	/* iphoffset */
	for (i = 0; i < TX_IP_OFFSET_ENTRY_MAX; i++)
		out_8(&ugeth->p_tx_glbl_pram->iphoffset[i],
				ug_info->iphoffset[i]);

	/* SQPTR */
	/* Size varies with number of Tx queues */
	ugeth->send_q_mem_reg_offset =
	    qe_muram_alloc(ug_info->numQueuesTx *
			   sizeof(struct ucc_geth_send_queue_qd),
			   UCC_GETH_SEND_QUEUE_QUEUE_DESCRIPTOR_ALIGNMENT);
	if (IS_ERR_VALUE(ugeth->send_q_mem_reg_offset)) {
		if (netif_msg_ifup(ugeth))
			ugeth_err
			    ("%s: Can not allocate DPRAM memory for p_send_q_mem_reg.",
			     __func__);
		return -ENOMEM;
	}

	ugeth->p_send_q_mem_reg =
	    (struct ucc_geth_send_queue_mem_region __iomem *) qe_muram_addr(ugeth->
			send_q_mem_reg_offset);
	out_be32(&ugeth->p_tx_glbl_pram->sqptr, ugeth->send_q_mem_reg_offset);

	/* Setup the table */
	/* Assume BD rings are already established */
	for (i = 0; i < ug_info->numQueuesTx; i++) {
		endOfRing =
		    ugeth->p_tx_bd_ring[i] + (ug_info->bdRingLenTx[i] -
					      1) * sizeof(struct qe_bd);
		if (ugeth->ug_info->uf_info.bd_mem_part == MEM_PART_SYSTEM) {
			out_be32(&ugeth->p_send_q_mem_reg->sqqd[i].bd_ring_base,
				 (u32) virt_to_phys(ugeth->p_tx_bd_ring[i]));
			out_be32(&ugeth->p_send_q_mem_reg->sqqd[i].
				 last_bd_completed_address,
				 (u32) virt_to_phys(endOfRing));
		} else if (ugeth->ug_info->uf_info.bd_mem_part ==
			   MEM_PART_MURAM) {
			out_be32(&ugeth->p_send_q_mem_reg->sqqd[i].bd_ring_base,
				 (u32) immrbar_virt_to_phys(ugeth->
							    p_tx_bd_ring[i]));
			out_be32(&ugeth->p_send_q_mem_reg->sqqd[i].
				 last_bd_completed_address,
				 (u32) immrbar_virt_to_phys(endOfRing));
		}
	}

	/* schedulerbasepointer */

	if (ug_info->numQueuesTx > 1) {
	/* scheduler exists only if more than 1 tx queue */
		ugeth->scheduler_offset =
		    qe_muram_alloc(sizeof(struct ucc_geth_scheduler),
				   UCC_GETH_SCHEDULER_ALIGNMENT);
		if (IS_ERR_VALUE(ugeth->scheduler_offset)) {
			if (netif_msg_ifup(ugeth))
				ugeth_err
				 ("%s: Can not allocate DPRAM memory for p_scheduler.",
				     __func__);
			return -ENOMEM;
		}

		ugeth->p_scheduler =
		    (struct ucc_geth_scheduler __iomem *) qe_muram_addr(ugeth->
							   scheduler_offset);
		out_be32(&ugeth->p_tx_glbl_pram->schedulerbasepointer,
			 ugeth->scheduler_offset);
		/* Zero out p_scheduler */
		memset_io((void __iomem *)ugeth->p_scheduler, 0, sizeof(struct ucc_geth_scheduler));

		/* Set values in scheduler */
		out_be32(&ugeth->p_scheduler->mblinterval,
			 ug_info->mblinterval);
		out_be16(&ugeth->p_scheduler->nortsrbytetime,
			 ug_info->nortsrbytetime);
		out_8(&ugeth->p_scheduler->fracsiz, ug_info->fracsiz);
		out_8(&ugeth->p_scheduler->strictpriorityq,
				ug_info->strictpriorityq);
		out_8(&ugeth->p_scheduler->txasap, ug_info->txasap);
		out_8(&ugeth->p_scheduler->extrabw, ug_info->extrabw);
		for (i = 0; i < NUM_TX_QUEUES; i++)
			out_8(&ugeth->p_scheduler->weightfactor[i],
			    ug_info->weightfactor[i]);

		/* Set pointers to cpucount registers in scheduler */
		ugeth->p_cpucount[0] = &(ugeth->p_scheduler->cpucount0);
		ugeth->p_cpucount[1] = &(ugeth->p_scheduler->cpucount1);
		ugeth->p_cpucount[2] = &(ugeth->p_scheduler->cpucount2);
		ugeth->p_cpucount[3] = &(ugeth->p_scheduler->cpucount3);
		ugeth->p_cpucount[4] = &(ugeth->p_scheduler->cpucount4);
		ugeth->p_cpucount[5] = &(ugeth->p_scheduler->cpucount5);
		ugeth->p_cpucount[6] = &(ugeth->p_scheduler->cpucount6);
		ugeth->p_cpucount[7] = &(ugeth->p_scheduler->cpucount7);
	}

	/* schedulerbasepointer */
	/* TxRMON_PTR (statistics) */
	if (ug_info->
	    statisticsMode & UCC_GETH_STATISTICS_GATHERING_MODE_FIRMWARE_TX) {
		ugeth->tx_fw_statistics_pram_offset =
		    qe_muram_alloc(sizeof
				   (struct ucc_geth_tx_firmware_statistics_pram),
				   UCC_GETH_TX_STATISTICS_ALIGNMENT);
		if (IS_ERR_VALUE(ugeth->tx_fw_statistics_pram_offset)) {
			if (netif_msg_ifup(ugeth))
				ugeth_err
				    ("%s: Can not allocate DPRAM memory for"
					" p_tx_fw_statistics_pram.",
				       	__func__);
			return -ENOMEM;
		}
		ugeth->p_tx_fw_statistics_pram =
		    (struct ucc_geth_tx_firmware_statistics_pram __iomem *)
		    qe_muram_addr(ugeth->tx_fw_statistics_pram_offset);
		/* Zero out p_tx_fw_statistics_pram */
		memset_io((void __iomem *)ugeth->p_tx_fw_statistics_pram,
		       0, sizeof(struct ucc_geth_tx_firmware_statistics_pram));
	}

	/* temoder */
	/* Already has speed set */

	if (ug_info->numQueuesTx > 1)
		temoder |= TEMODER_SCHEDULER_ENABLE;
	if (ug_info->ipCheckSumGenerate)
		temoder |= TEMODER_IP_CHECKSUM_GENERATE;
	temoder |= ((ug_info->numQueuesTx - 1) << TEMODER_NUM_OF_QUEUES_SHIFT);
	out_be16(&ugeth->p_tx_glbl_pram->temoder, temoder);

	test = in_be16(&ugeth->p_tx_glbl_pram->temoder);

	/* Function code register value to be used later */
	function_code = UCC_BMR_BO_BE | UCC_BMR_GBL;
	/* Required for QE */

	/* function code register */
	out_be32(&ugeth->p_tx_glbl_pram->tstate, ((u32) function_code) << 24);

	/* Rx global PRAM */
	/* Allocate global rx parameter RAM page */
	ugeth->rx_glbl_pram_offset =
	    qe_muram_alloc(sizeof(struct ucc_geth_rx_global_pram),
			   UCC_GETH_RX_GLOBAL_PRAM_ALIGNMENT);
	if (IS_ERR_VALUE(ugeth->rx_glbl_pram_offset)) {
		if (netif_msg_ifup(ugeth))
			ugeth_err
			    ("%s: Can not allocate DPRAM memory for p_rx_glbl_pram.",
			     __func__);
		return -ENOMEM;
	}
	ugeth->p_rx_glbl_pram =
	    (struct ucc_geth_rx_global_pram __iomem *) qe_muram_addr(ugeth->
							rx_glbl_pram_offset);
	/* Zero out p_rx_glbl_pram */
	memset_io((void __iomem *)ugeth->p_rx_glbl_pram, 0, sizeof(struct ucc_geth_rx_global_pram));

	/* Fill global PRAM */

	/* RQPTR */
	/* Size varies with number of Rx threads */
	ugeth->thread_dat_rx_offset =
	    qe_muram_alloc(numThreadsRxNumerical *
			   sizeof(struct ucc_geth_thread_data_rx),
			   UCC_GETH_THREAD_DATA_ALIGNMENT);
	if (IS_ERR_VALUE(ugeth->thread_dat_rx_offset)) {
		if (netif_msg_ifup(ugeth))
			ugeth_err
			    ("%s: Can not allocate DPRAM memory for p_thread_data_rx.",
			     __func__);
		return -ENOMEM;
	}

	ugeth->p_thread_data_rx =
	    (struct ucc_geth_thread_data_rx __iomem *) qe_muram_addr(ugeth->
							thread_dat_rx_offset);
	out_be32(&ugeth->p_rx_glbl_pram->rqptr, ugeth->thread_dat_rx_offset);

	/* typeorlen */
	out_be16(&ugeth->p_rx_glbl_pram->typeorlen, ug_info->typeorlen);

	/* rxrmonbaseptr (statistics) */
	if (ug_info->
	    statisticsMode & UCC_GETH_STATISTICS_GATHERING_MODE_FIRMWARE_RX) {
		ugeth->rx_fw_statistics_pram_offset =
		    qe_muram_alloc(sizeof
				   (struct ucc_geth_rx_firmware_statistics_pram),
				   UCC_GETH_RX_STATISTICS_ALIGNMENT);
		if (IS_ERR_VALUE(ugeth->rx_fw_statistics_pram_offset)) {
			if (netif_msg_ifup(ugeth))
				ugeth_err
					("%s: Can not allocate DPRAM memory for"
					" p_rx_fw_statistics_pram.", __func__);
			return -ENOMEM;
		}
		ugeth->p_rx_fw_statistics_pram =
		    (struct ucc_geth_rx_firmware_statistics_pram __iomem *)
		    qe_muram_addr(ugeth->rx_fw_statistics_pram_offset);
		/* Zero out p_rx_fw_statistics_pram */
		memset_io((void __iomem *)ugeth->p_rx_fw_statistics_pram, 0,
		       sizeof(struct ucc_geth_rx_firmware_statistics_pram));
	}

	/* intCoalescingPtr */

	/* Size varies with number of Rx queues */
	ugeth->rx_irq_coalescing_tbl_offset =
	    qe_muram_alloc(ug_info->numQueuesRx *
			   sizeof(struct ucc_geth_rx_interrupt_coalescing_entry)
			   + 4, UCC_GETH_RX_INTERRUPT_COALESCING_ALIGNMENT);
	if (IS_ERR_VALUE(ugeth->rx_irq_coalescing_tbl_offset)) {
		if (netif_msg_ifup(ugeth))
			ugeth_err
			    ("%s: Can not allocate DPRAM memory for"
				" p_rx_irq_coalescing_tbl.", __func__);
		return -ENOMEM;
	}

	ugeth->p_rx_irq_coalescing_tbl =
	    (struct ucc_geth_rx_interrupt_coalescing_table __iomem *)
	    qe_muram_addr(ugeth->rx_irq_coalescing_tbl_offset);
	out_be32(&ugeth->p_rx_glbl_pram->intcoalescingptr,
		 ugeth->rx_irq_coalescing_tbl_offset);

	/* Fill interrupt coalescing table */
	for (i = 0; i < ug_info->numQueuesRx; i++) {
		out_be32(&ugeth->p_rx_irq_coalescing_tbl->coalescingentry[i].
			 interruptcoalescingmaxvalue,
			 ug_info->interruptcoalescingmaxvalue[i]);
		out_be32(&ugeth->p_rx_irq_coalescing_tbl->coalescingentry[i].
			 interruptcoalescingcounter,
			 ug_info->interruptcoalescingmaxvalue[i]);
	}

	/* MRBLR */
	init_max_rx_buff_len(uf_info->max_rx_buf_length,
			     &ugeth->p_rx_glbl_pram->mrblr);
	/* MFLR */
	out_be16(&ugeth->p_rx_glbl_pram->mflr, ug_info->maxFrameLength);
	/* MINFLR */
	init_min_frame_len(ug_info->minFrameLength,
			   &ugeth->p_rx_glbl_pram->minflr,
			   &ugeth->p_rx_glbl_pram->mrblr);
	/* MAXD1 */
	out_be16(&ugeth->p_rx_glbl_pram->maxd1, ug_info->maxD1Length);
	/* MAXD2 */
	out_be16(&ugeth->p_rx_glbl_pram->maxd2, ug_info->maxD2Length);

	/* l2qt */
	l2qt = 0;
	for (i = 0; i < UCC_GETH_VLAN_PRIORITY_MAX; i++)
		l2qt |= (ug_info->l2qt[i] << (28 - 4 * i));
	out_be32(&ugeth->p_rx_glbl_pram->l2qt, l2qt);

	/* l3qt */
	for (j = 0; j < UCC_GETH_IP_PRIORITY_MAX; j += 8) {
		l3qt = 0;
		for (i = 0; i < 8; i++)
			l3qt |= (ug_info->l3qt[j + i] << (28 - 4 * i));
		out_be32(&ugeth->p_rx_glbl_pram->l3qt[j/8], l3qt);
	}

	/* vlantype */
	out_be16(&ugeth->p_rx_glbl_pram->vlantype, ug_info->vlantype);

	/* vlantci */
	out_be16(&ugeth->p_rx_glbl_pram->vlantci, ug_info->vlantci);

	/* ecamptr */
	out_be32(&ugeth->p_rx_glbl_pram->ecamptr, ug_info->ecamptr);

	/* RBDQPTR */
	/* Size varies with number of Rx queues */
	ugeth->rx_bd_qs_tbl_offset =
	    qe_muram_alloc(ug_info->numQueuesRx *
			   (sizeof(struct ucc_geth_rx_bd_queues_entry) +
			    sizeof(struct ucc_geth_rx_prefetched_bds)),
			   UCC_GETH_RX_BD_QUEUES_ALIGNMENT);
	if (IS_ERR_VALUE(ugeth->rx_bd_qs_tbl_offset)) {
		if (netif_msg_ifup(ugeth))
			ugeth_err
			    ("%s: Can not allocate DPRAM memory for p_rx_bd_qs_tbl.",
			     __func__);
		return -ENOMEM;
	}

	ugeth->p_rx_bd_qs_tbl =
	    (struct ucc_geth_rx_bd_queues_entry __iomem *) qe_muram_addr(ugeth->
				    rx_bd_qs_tbl_offset);
	out_be32(&ugeth->p_rx_glbl_pram->rbdqptr, ugeth->rx_bd_qs_tbl_offset);
	/* Zero out p_rx_bd_qs_tbl */
	memset_io((void __iomem *)ugeth->p_rx_bd_qs_tbl,
	       0,
	       ug_info->numQueuesRx * (sizeof(struct ucc_geth_rx_bd_queues_entry) +
				       sizeof(struct ucc_geth_rx_prefetched_bds)));

	/* Setup the table */
	/* Assume BD rings are already established */
	for (i = 0; i < ug_info->numQueuesRx; i++) {
		if (ugeth->ug_info->uf_info.bd_mem_part == MEM_PART_SYSTEM) {
			out_be32(&ugeth->p_rx_bd_qs_tbl[i].externalbdbaseptr,
				 (u32) virt_to_phys(ugeth->p_rx_bd_ring[i]));
		} else if (ugeth->ug_info->uf_info.bd_mem_part ==
			   MEM_PART_MURAM) {
			out_be32(&ugeth->p_rx_bd_qs_tbl[i].externalbdbaseptr,
				 (u32) immrbar_virt_to_phys(ugeth->
							    p_rx_bd_ring[i]));
		}
		/* rest of fields handled by QE */
	}

	/* remoder */
	/* Already has speed set */

	if (ugeth->rx_extended_features)
		remoder |= REMODER_RX_EXTENDED_FEATURES;
	if (ug_info->rxExtendedFiltering)
		remoder |= REMODER_RX_EXTENDED_FILTERING;
	if (ug_info->dynamicMaxFrameLength)
		remoder |= REMODER_DYNAMIC_MAX_FRAME_LENGTH;
	if (ug_info->dynamicMinFrameLength)
		remoder |= REMODER_DYNAMIC_MIN_FRAME_LENGTH;
	remoder |=
	    ug_info->vlanOperationTagged << REMODER_VLAN_OPERATION_TAGGED_SHIFT;
	remoder |=
	    ug_info->
	    vlanOperationNonTagged << REMODER_VLAN_OPERATION_NON_TAGGED_SHIFT;
	remoder |= ug_info->rxQoSMode << REMODER_RX_QOS_MODE_SHIFT;
	remoder |= ((ug_info->numQueuesRx - 1) << REMODER_NUM_OF_QUEUES_SHIFT);
	if (ug_info->ipCheckSumCheck)
		remoder |= REMODER_IP_CHECKSUM_CHECK;
	if (ug_info->ipAddressAlignment)
		remoder |= REMODER_IP_ADDRESS_ALIGNMENT;
	out_be32(&ugeth->p_rx_glbl_pram->remoder, remoder);

	/* Note that this function must be called */
	/* ONLY AFTER p_tx_fw_statistics_pram */
	/* andp_UccGethRxFirmwareStatisticsPram are allocated ! */
	init_firmware_statistics_gathering_mode((ug_info->
		statisticsMode &
		UCC_GETH_STATISTICS_GATHERING_MODE_FIRMWARE_TX),
		(ug_info->statisticsMode &
		UCC_GETH_STATISTICS_GATHERING_MODE_FIRMWARE_RX),
		&ugeth->p_tx_glbl_pram->txrmonbaseptr,
		ugeth->tx_fw_statistics_pram_offset,
		&ugeth->p_rx_glbl_pram->rxrmonbaseptr,
		ugeth->rx_fw_statistics_pram_offset,
		&ugeth->p_tx_glbl_pram->temoder,
		&ugeth->p_rx_glbl_pram->remoder);

	/* function code register */
	out_8(&ugeth->p_rx_glbl_pram->rstate, function_code);

	/* initialize extended filtering */
	if (ug_info->rxExtendedFiltering) {
		if (!ug_info->extendedFilteringChainPointer) {
			if (netif_msg_ifup(ugeth))
				ugeth_err("%s: Null Extended Filtering Chain Pointer.",
					  __func__);
			return -EINVAL;
		}

		/* Allocate memory for extended filtering Mode Global
		Parameters */
		ugeth->exf_glbl_param_offset =
		    qe_muram_alloc(sizeof(struct ucc_geth_exf_global_pram),
		UCC_GETH_RX_EXTENDED_FILTERING_GLOBAL_PARAMETERS_ALIGNMENT);
		if (IS_ERR_VALUE(ugeth->exf_glbl_param_offset)) {
			if (netif_msg_ifup(ugeth))
				ugeth_err
					("%s: Can not allocate DPRAM memory for"
					" p_exf_glbl_param.", __func__);
			return -ENOMEM;
		}

		ugeth->p_exf_glbl_param =
		    (struct ucc_geth_exf_global_pram __iomem *) qe_muram_addr(ugeth->
				 exf_glbl_param_offset);
		out_be32(&ugeth->p_rx_glbl_pram->exfGlobalParam,
			 ugeth->exf_glbl_param_offset);
		out_be32(&ugeth->p_exf_glbl_param->l2pcdptr,
			 (u32) ug_info->extendedFilteringChainPointer);

	} else {		/* initialize 82xx style address filtering */

		/* Init individual address recognition registers to disabled */

		for (j = 0; j < NUM_OF_PADDRS; j++)
			ugeth_82xx_filtering_clear_addr_in_paddr(ugeth, (u8) j);

		p_82xx_addr_filt =
		    (struct ucc_geth_82xx_address_filtering_pram __iomem *) ugeth->
		    p_rx_glbl_pram->addressfiltering;

		ugeth_82xx_filtering_clear_all_addr_in_hash(ugeth,
			ENET_ADDR_TYPE_GROUP);
		ugeth_82xx_filtering_clear_all_addr_in_hash(ugeth,
			ENET_ADDR_TYPE_INDIVIDUAL);
	}

	/*
	 * Initialize UCC at QE level
	 */

	command = QE_INIT_TX_RX;

	/* Allocate shadow InitEnet command parameter structure.
	 * This is needed because after the InitEnet command is executed,
	 * the structure in DPRAM is released, because DPRAM is a premium
	 * resource.
	 * This shadow structure keeps a copy of what was done so that the
	 * allocated resources can be released when the channel is freed.
	 */
	if (!(ugeth->p_init_enet_param_shadow =
	      kmalloc(sizeof(struct ucc_geth_init_pram), GFP_KERNEL))) {
		if (netif_msg_ifup(ugeth))
			ugeth_err
			    ("%s: Can not allocate memory for"
				" p_UccInitEnetParamShadows.", __func__);
		return -ENOMEM;
	}
	/* Zero out *p_init_enet_param_shadow */
	memset((char *)ugeth->p_init_enet_param_shadow,
	       0, sizeof(struct ucc_geth_init_pram));

	/* Fill shadow InitEnet command parameter structure */

	ugeth->p_init_enet_param_shadow->resinit1 =
	    ENET_INIT_PARAM_MAGIC_RES_INIT1;
	ugeth->p_init_enet_param_shadow->resinit2 =
	    ENET_INIT_PARAM_MAGIC_RES_INIT2;
	ugeth->p_init_enet_param_shadow->resinit3 =
	    ENET_INIT_PARAM_MAGIC_RES_INIT3;
	ugeth->p_init_enet_param_shadow->resinit4 =
	    ENET_INIT_PARAM_MAGIC_RES_INIT4;
	ugeth->p_init_enet_param_shadow->resinit5 =
	    ENET_INIT_PARAM_MAGIC_RES_INIT5;
	ugeth->p_init_enet_param_shadow->rgftgfrxglobal |=
	    ((u32) ug_info->numThreadsRx) << ENET_INIT_PARAM_RGF_SHIFT;
	ugeth->p_init_enet_param_shadow->rgftgfrxglobal |=
	    ((u32) ug_info->numThreadsTx) << ENET_INIT_PARAM_TGF_SHIFT;

	ugeth->p_init_enet_param_shadow->rgftgfrxglobal |=
	    ugeth->rx_glbl_pram_offset | ug_info->riscRx;
	if ((ug_info->largestexternallookupkeysize !=
	     QE_FLTR_LARGEST_EXTERNAL_TABLE_LOOKUP_KEY_SIZE_NONE)
	    && (ug_info->largestexternallookupkeysize !=
		QE_FLTR_LARGEST_EXTERNAL_TABLE_LOOKUP_KEY_SIZE_8_BYTES)
	    && (ug_info->largestexternallookupkeysize !=
		QE_FLTR_LARGEST_EXTERNAL_TABLE_LOOKUP_KEY_SIZE_16_BYTES)) {
		if (netif_msg_ifup(ugeth))
			ugeth_err("%s: Invalid largest External Lookup Key Size.",
				  __func__);
		return -EINVAL;
	}
	ugeth->p_init_enet_param_shadow->largestexternallookupkeysize =
	    ug_info->largestexternallookupkeysize;
	size = sizeof(struct ucc_geth_thread_rx_pram);
	if (ug_info->rxExtendedFiltering) {
		size += THREAD_RX_PRAM_ADDITIONAL_FOR_EXTENDED_FILTERING;
		if (ug_info->largestexternallookupkeysize ==
		    QE_FLTR_TABLE_LOOKUP_KEY_SIZE_8_BYTES)
			size +=
			    THREAD_RX_PRAM_ADDITIONAL_FOR_EXTENDED_FILTERING_8;
		if (ug_info->largestexternallookupkeysize ==
		    QE_FLTR_TABLE_LOOKUP_KEY_SIZE_16_BYTES)
			size +=
			    THREAD_RX_PRAM_ADDITIONAL_FOR_EXTENDED_FILTERING_16;
	}

	if ((ret_val = fill_init_enet_entries(ugeth, &(ugeth->
		p_init_enet_param_shadow->rxthread[0]),
		(u8) (numThreadsRxNumerical + 1)
		/* Rx needs one extra for terminator */
		, size, UCC_GETH_THREAD_RX_PRAM_ALIGNMENT,
		ug_info->riscRx, 1)) != 0) {
		if (netif_msg_ifup(ugeth))
				ugeth_err("%s: Can not fill p_init_enet_param_shadow.",
					__func__);
		return ret_val;
	}

	ugeth->p_init_enet_param_shadow->txglobal =
	    ugeth->tx_glbl_pram_offset | ug_info->riscTx;
	if ((ret_val =
	     fill_init_enet_entries(ugeth,
				    &(ugeth->p_init_enet_param_shadow->
				      txthread[0]), numThreadsTxNumerical,
				    sizeof(struct ucc_geth_thread_tx_pram),
				    UCC_GETH_THREAD_TX_PRAM_ALIGNMENT,
				    ug_info->riscTx, 0)) != 0) {
		if (netif_msg_ifup(ugeth))
			ugeth_err("%s: Can not fill p_init_enet_param_shadow.",
				  __func__);
		return ret_val;
	}

	/* Load Rx bds with buffers */
	for (i = 0; i < ug_info->numQueuesRx; i++) {
		if ((ret_val = rx_bd_buffer_set(ugeth, (u8) i)) != 0) {
			if (netif_msg_ifup(ugeth))
				ugeth_err("%s: Can not fill Rx bds with buffers.",
					  __func__);
			return ret_val;
		}
	}

	/* Allocate InitEnet command parameter structure */
	init_enet_pram_offset = qe_muram_alloc(sizeof(struct ucc_geth_init_pram), 4);
	if (IS_ERR_VALUE(init_enet_pram_offset)) {
		if (netif_msg_ifup(ugeth))
			ugeth_err
			    ("%s: Can not allocate DPRAM memory for p_init_enet_pram.",
			     __func__);
		return -ENOMEM;
	}
	p_init_enet_pram =
	    (struct ucc_geth_init_pram __iomem *) qe_muram_addr(init_enet_pram_offset);

	/* Copy shadow InitEnet command parameter structure into PRAM */
	out_8(&p_init_enet_pram->resinit1,
			ugeth->p_init_enet_param_shadow->resinit1);
	out_8(&p_init_enet_pram->resinit2,
			ugeth->p_init_enet_param_shadow->resinit2);
	out_8(&p_init_enet_pram->resinit3,
			ugeth->p_init_enet_param_shadow->resinit3);
	out_8(&p_init_enet_pram->resinit4,
			ugeth->p_init_enet_param_shadow->resinit4);
	out_be16(&p_init_enet_pram->resinit5,
		 ugeth->p_init_enet_param_shadow->resinit5);
	out_8(&p_init_enet_pram->largestexternallookupkeysize,
	    ugeth->p_init_enet_param_shadow->largestexternallookupkeysize);
	out_be32(&p_init_enet_pram->rgftgfrxglobal,
		 ugeth->p_init_enet_param_shadow->rgftgfrxglobal);
	for (i = 0; i < ENET_INIT_PARAM_MAX_ENTRIES_RX; i++)
		out_be32(&p_init_enet_pram->rxthread[i],
			 ugeth->p_init_enet_param_shadow->rxthread[i]);
	out_be32(&p_init_enet_pram->txglobal,
		 ugeth->p_init_enet_param_shadow->txglobal);
	for (i = 0; i < ENET_INIT_PARAM_MAX_ENTRIES_TX; i++)
		out_be32(&p_init_enet_pram->txthread[i],
			 ugeth->p_init_enet_param_shadow->txthread[i]);

	/* Issue QE command */
	cecr_subblock =
	    ucc_fast_get_qe_cr_subblock(ugeth->ug_info->uf_info.ucc_num);
	qe_issue_cmd(command, cecr_subblock, QE_CR_PROTOCOL_ETHERNET,
		     init_enet_pram_offset);

	/* Free InitEnet command parameter */
	qe_muram_free(init_enet_pram_offset);

	return 0;
}

/* This is called by the kernel when a frame is ready for transmission. */
/* It is pointed to by the dev->hard_start_xmit function pointer */
static int ucc_geth_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct ucc_geth_private *ugeth = netdev_priv(dev);
#ifdef CONFIG_UGETH_TX_ON_DEMAND
	struct ucc_fast_private *uccf;
#endif
	u8 __iomem *bd;			/* BD pointer */
	u32 bd_status;
	u8 txQ = 0;
	unsigned long flags;

	ugeth_vdbg("%s: IN", __func__);

	spin_lock_irqsave(&ugeth->lock, flags);

	dev->stats.tx_bytes += skb->len;

	/* Start from the next BD that should be filled */
	bd = ugeth->txBd[txQ];
	bd_status = in_be32((u32 __iomem *)bd);
	/* Save the skb pointer so we can free it later */
	ugeth->tx_skbuff[txQ][ugeth->skb_curtx[txQ]] = skb;

	/* Update the current skb pointer (wrapping if this was the last) */
	ugeth->skb_curtx[txQ] =
	    (ugeth->skb_curtx[txQ] +
	     1) & TX_RING_MOD_MASK(ugeth->ug_info->bdRingLenTx[txQ]);

	/* set up the buffer descriptor */
	out_be32(&((struct qe_bd __iomem *)bd)->buf,
		      dma_map_single(ugeth->dev, skb->data,
			      skb->len, DMA_TO_DEVICE));

	/* printk(KERN_DEBUG"skb->data is 0x%x\n",skb->data); */

	bd_status = (bd_status & T_W) | T_R | T_I | T_L | skb->len;

	/* set bd status and length */
	out_be32((u32 __iomem *)bd, bd_status);

	dev->trans_start = jiffies;

	/* Move to next BD in the ring */
	if (!(bd_status & T_W))
		bd += sizeof(struct qe_bd);
	else
		bd = ugeth->p_tx_bd_ring[txQ];

	/* If the next BD still needs to be cleaned up, then the bds
	   are full.  We need to tell the kernel to stop sending us stuff. */
	if (bd == ugeth->confBd[txQ]) {
		if (!netif_queue_stopped(dev))
			netif_stop_queue(dev);
	}

	ugeth->txBd[txQ] = bd;

	if (ugeth->p_scheduler) {
		ugeth->cpucount[txQ]++;
		/* Indicate to QE that there are more Tx bds ready for
		transmission */
		/* This is done by writing a running counter of the bd
		count to the scheduler PRAM. */
		out_be16(ugeth->p_cpucount[txQ], ugeth->cpucount[txQ]);
	}

#ifdef CONFIG_UGETH_TX_ON_DEMAND
	uccf = ugeth->uccf;
	out_be16(uccf->p_utodr, UCC_FAST_TOD);
#endif
	spin_unlock_irqrestore(&ugeth->lock, flags);

	return NETDEV_TX_OK;
}

static int ucc_geth_rx(struct ucc_geth_private *ugeth, u8 rxQ, int rx_work_limit)
{
	struct sk_buff *skb;
	u8 __iomem *bd;
	u16 length, howmany = 0;
	u32 bd_status;
	u8 *bdBuffer;
	struct net_device *dev;

	ugeth_vdbg("%s: IN", __func__);

	dev = ugeth->ndev;

	/* collect received buffers */
	bd = ugeth->rxBd[rxQ];

	bd_status = in_be32((u32 __iomem *)bd);

	/* while there are received buffers and BD is full (~R_E) */
	while (!((bd_status & (R_E)) || (--rx_work_limit < 0))) {
		bdBuffer = (u8 *) in_be32(&((struct qe_bd __iomem *)bd)->buf);
		length = (u16) ((bd_status & BD_LENGTH_MASK) - 4);
		skb = ugeth->rx_skbuff[rxQ][ugeth->skb_currx[rxQ]];

		/* determine whether buffer is first, last, first and last
		(single buffer frame) or middle (not first and not last) */
		if (!skb ||
		    (!(bd_status & (R_F | R_L))) ||
		    (bd_status & R_ERRORS_FATAL)) {
			if (netif_msg_rx_err(ugeth))
				ugeth_err("%s, %d: ERROR!!! skb - 0x%08x",
					   __func__, __LINE__, (u32) skb);
			if (skb) {
				skb->data = skb->head + NET_SKB_PAD;
				__skb_queue_head(&ugeth->rx_recycle, skb);
			}

			ugeth->rx_skbuff[rxQ][ugeth->skb_currx[rxQ]] = NULL;
			dev->stats.rx_dropped++;
		} else {
			dev->stats.rx_packets++;
			howmany++;

			/* Prep the skb for the packet */
			skb_put(skb, length);

			/* Tell the skb what kind of packet this is */
			skb->protocol = eth_type_trans(skb, ugeth->ndev);

			dev->stats.rx_bytes += length;
			/* Send the packet up the stack */
			netif_receive_skb(skb);
		}

		skb = get_new_skb(ugeth, bd);
		if (!skb) {
			if (netif_msg_rx_err(ugeth))
				ugeth_warn("%s: No Rx Data Buffer", __func__);
			dev->stats.rx_dropped++;
			break;
		}

		ugeth->rx_skbuff[rxQ][ugeth->skb_currx[rxQ]] = skb;

		/* update to point at the next skb */
		ugeth->skb_currx[rxQ] =
		    (cale Semicondutor, Inc+ All ri 1) & RX_RING_MOD_MASKghts resug_info->bdRingLenRor, In);

		if (bd_statusi Gr_W)
			bd = cale Sep_rx_bd_ringr, In;
		elsescripti+= sizeof(struct qe_bdli@frele.com>
 *= in_be32((u32 __iomem *)ree s	}

scale SerxBdr, Inc. bd;
	return howmany;
}

com>ic righucc_ale _txrogram inet_device *dev, u8 txQ)
{
	/* Start from(C) 2006-2BD that should be filled Freeogram iense as ppriv*
 **cale  =by tdevy lat(dev modu8te  it andbd;	datiBDopyriger Freeibutle.com>
 i@frption:
 * QEconfBd[ttherneware; you can redistribute  it and/or mondatiNormal processing. Freewhile (ale.com>
 *
 T_R) == 0) {
	ur
 * opsk_buff *skbi@free <lincontains already transmitted ice.er.   Frees/* Handle(C) 2lude <linux/spinlo and releaseh>
#incluC) 22 ofonse,used withncludnduten eitame.h>
#freesca(ptioon:
 * QEtder nux/) && (netif_queue_stopped <linclude escripreaki@fredev->com>s.tx_packets++i@fre009 x/workqueue_skice.>
#in[cale Semicodirtytx>
#in]i@freescamico/of_mdlen(&it
 * und_recycle) <GridBDdish <LEN &&scri: Shlmicoh>
#inc_checkap_q,scrie <asreescale.com>
 *ufcom>
.maxUCC Gufinclgthuthorcc_getUCC_GETH_RX_DATA_BUF_ALIGNMENTinux/o_/irqe.h>
#iheadude <asm/qe.h>
#inc,m/uc modt Driver
#inckfree/irqap_qli@frelude <asm/irq.h>
#include <asm/io.h>
#include  = NULLarg)ude <asm/io.h>
#includec. All rights reserve
#define ugetthor: Shlomi GTidish <gridish@freescale.com>
 *	   Li YaTncludeli@fre/* We th_dd a/spinlo, so now we can resn;  elude <lssion Freesesca<linux/of_mdio.h>
#incluescri<linuxwakee.h>
# <linuxnfo(foAdvancinux/mude irmatN_INlinux/init.h>
# form!/netdevice.h>
#Winux/of*
 * This program is free set Driver
 *
on:
 * QE UtC Gigabit nux/slatware; you can redistribute  it and/or modifat , ## ude <linux/ terms of  the 0eral  Public License as ppolllished byapi_ogram i* 1

,c LicbudgetFoundr
 * option) any later version.
 ux/skbuer_of(EFINE_r
 * option) any later ,- 1

eth_r
 * option) anyom>
versle, i;
	righGNU Gen, i    , 0);
Mdbg
#else
, 0);
MODndatiTx evinuxrupt.h>
#it yourpin_lockude <asm/c steth_for (i = 0; i < e.com>
 *numQof_msTx; i++escrense as publcale Sene Sofebug, tatiunc struct ucc_geth_in
	GNU Genh_prinfo ugeth_primary_info = {
	.uf_infoR= {
		    _IDLES_B+=ption) anyrrt = ME, i,NLOCK(u -PARM_DES_SENDesca_IDLES_B<NLOCK(ug<linu 1


completebug =eth_vsetbits32freescaleccfQE Uuccm,e ugErintEVENTS |UCC_GETH_URFSET modify f  the GNU General  Public rqf  the_g.msg_enablerq_he <lir(righirq, void *om>
geth_lock);
y the
 * Free S can 
MODUr
 * option) any later version.
 */
#include <linux/r
 * optionfasty later verccfug, debug.msg_enable, int, 0);
MODUregisnit.ibutucceCC_FAST_TX_ENCODINGmebug, as pvdbg("%s: IN", __func___SENDRENTg verbosityENT_TT, "Debug verbosity level (0=non>
#iux/dmclear ..., s.h>
#icce = ribu)an redist   .urfset enux/kccmesTx = 1,
	.numQueuesRx = 1,m	.extene &ustedm;
	out.numQueuesRx = 1,
,DING_de <linu

#in o ugreceive    },
	f therequire0xffff=all)");

esca2_t) N (CC_GETH_URFSET_INIT,
		    .utfs_INIT,forma1


schedule_prepude <asm/= UCC_INIT,xtende&= ~umInterFrameGapEnforcement = 0x50g(fmtypeorlen = 3072 /*15m6 */ uint3.)  BackInterFram= 0x60,
	.mblitBeb}odify /* Errorsux/dmothe	    },
	.num60,
	.miminCC_GEOTHER,
	.backToindow = 0xeth_prCC_GEBSYescri.h>

#inclurx_eransmccesveFlowControl = 1,
	.transmTXElowControl = 1,
t.maxGroupAddrUCC_GETH_UTIRQ_HANDLEDeral #ifdef CONFIG_NET_POLL_CONTROLLER
/*
 * Pollll)"'/initrupt' -/mii.hby things lik 200tconsolinuo sendm/ucsxD2L>
#iout havll)"to re-enablec Li,
	.vls. It's not car (at <linxD2LC) 2pausePeri routine is exec {1,g.
 Fre PublicCC_GEensenetFUP << 1 ) -  the
 * Free Sgeth_lock);

static struct {
	u32 msg_*/
#include <linux/utftt =g verbosity level"fsl_pq_mdirqSENDdisR,
	INIT(irq	.extenTFET_INIT,
		    .u = U<linux/HER,
	N,
			TX_BD}
#endif /*64,
	.maxD1Length = 1520,
	>
#in Public License as pset_mac_addrlished by the
 * Free SofCC_GETpBD_RING_LEN,
			TX_BD_RING_LEN,
			TX_BD_RING_LEN,
			TX_x/etherdockEN,
 *TATIO= p   .urfs!is_valid_e = 0LEN,
	EN,
->sa_dataeth_wf  the -EADDRNOTAVAILSENDmemcpy <li->#incEN,
, keysize =
	  x = {->EN,
inclde <lin
	 * If = {* Frixf000,running,
   will set macNE,
	 FAST_TX_ISTICwhen opeDWARlesciATHERI.ISTIlisionW!<linuxHARDWARe ugeth_wne UGETH_M

static stN,
		uct ucc_geth_infiniRING_L PublonIZE_N_regsKUP_KEY_SIZE_N[0]e "ucc_ge,
	.vlanOperat1onNonTagged = UCC_GETH_V2onNonTagged = UCC_GETH_V3onNonTagged = UCC_GETH_V4onNonTagged = UCC_GETH_V5onNonTaggede <asm/ugED_NO->macstnEN,
1ONTROL_MODE_NONE,
	.padAndCrc = MACC2STEM,
		    .rtationTagged = UCC_GEfine UGETH_MSG_DEFAULT	(NETIF_MSG_IH_VLAN_Orogram ition) any later versionFTT_INIT,
		    .ufpt = 256,
	 = MEM_PARTTX_BD_RerrADS_erDRESRINGogram _OF_Tt = ME_GETHf (err,
	.backToBlinuxmsg_ifuptic strescriAST_RXerrODING_Can000,
th_pgurcingmaxnal{ -1 };, "

#defi"abor1, 1,"csMode nam
	.ex	gotoTION_RLengtC1_AND_RIS	RX_BDn;  

#ifdef uct ucc_geth_info ugeth_info[8];

#ifdef DEBUG
static void mem_disp(u8 *addr, net |
		UC, ize16Aling 

#defi (size >> 4) << 4;
	int size4Aling adjust_eING_pauseface 2;
	int notAlign = 0;
	if (size % 16)
		notAlign = 1;

	for (i = addr; (u32) i < (u32) addr + size16Aling; i += 16)
		printk("0x%08x: %08x %08x/*: Shl  Set MACSTNEST_1,32) i);
	fo208x: ", addr + sTH_S/* For morX |
tails seinux/mhardware specck.hddr + size4AH_VLAN_OPERATION_TAGGED_NOP,
	.vlanOperationNonTagged = UCC_GETH_VLAN_OPERATION_NON_TAGGED_NOP,
	.rxQoSMode = UCC_GETH_QOS_MODE_DEFAULT,
	.aufc = UPSMR_AUTOMATIC_FLOW_CONTROL_MODE_NONE,
	.padAndCrc = MACCFG2_PAD_AND_CRC_MODE_PAD_AND_CRC,
	.numThe4Aling =staticER,
	tup if mCOMM_DIRrintAND_TXnt notAlign = 0;
	if (size % 16)
		notAlign = 1;

	for (i = addr; (u3HER,
	.2) addr + size16Aling; 6)
		printk("0x%08x: %08x %08xne UGETH_Merr:D_RING_LEN}io.htic structf  the ION_R}

ING_	.interren some100,
 needsUCCEusinux/mernal2) addr + ize4/* R  thes 0ackIfs2_t)ssnclud Public License as pFIRM	TX_BD_RING_LEN,
			TX_BD_RING_LEN,
			TX_BD_RING_LEN,
			TX_BD_RING_LEN,
			TX_BD_RION_RISAST_RX_ENCODING_NRZ,
		    .tcrc =noneest ERATIONNE,
	escollisionW,
	.vlanOperatio & ExD1LGROUP_EST_th_info ugeth_info[8];

#ifdef DEBUG
static void meMulticast_ALIGNMENmii.h
{
	sTA_BUF_i;
	int siLIGNMEN-NG_Mthis w theyou wanted?; i += 16		    .tcrcE_FLTR_LARGINVAformize4Aling H_VLAphOKUP_nt notAlign = 0;
	if (size % 16)
		notAlign = 1;

	for (i = addr; (u3H_VLialize PHYsize16Aling; i += 16)
		printk("0x}
}

static UF_ALIGNMENGETH_NUM_OF_THREADic struct ucc_geth_info ugeth_info[8];

#ifdef DEBUG
static void mem_disp()));

	skb-MACsize16Aling; i += 16)
		printk("0x%08x: %08x %08x %08x BackestN,
		N,
			TX_BD_RING_LEN,
			TX6 */ TFET_INIT,
		     i +=  0, "UCC Geth} elsent notAlign = 0;
	if (size % 16)
		notAlign = 1;

	for (i = addr; (u3ed 1IRQany b2) addr + size16Aling; i += 16)
		printk("0x%08x: %08x %08xphy>> 2) M_DEVICEphy__iomemBackImpty(lh
	.excessDefer =eth_in> 2)  arg...)       ATHERIBD_RImat,up_cap = ugeUP_KEY_S i +=qe_alive_duabit_sleep() ||n:
 * QE kb;
	->	TX_BD_d_status = in_be32bd = ugeomem *)bdx = QE_Rwol_GETH_ST}
}

static lock, flags);
		return NULL;
	}
}

static structStopdatae kate l /of_m,ux/dmhal0,
	.ugethtrollit.h>
 Public License as pclose	TX_BD_RING_LEN,
			TX_BD_RING_LEN,
			TX_BD_RING_LEN,
			TX_BD_RING_LEN,
			TXfo.max_rx_buf_length +
				    UCC_GBackIRING_LEgeth->p_rx_bd_ri flags);
		return NULL;

	th_dbFROM_DEVICE));

	out_be32((u32 __i= MEM_PARTeth,
	[rxQ];
op arg...)       ne UGETH_MSG_ __ioFIRM |
		UCC Ta proUCC_reGETHinteMACux/dmPHYsk_buff *skbRingLenTx as puimeypeoworkrogram ii++)
static Di++)geth_lock);

static struct {
	u32 m_GETH_NUM_ING_LEN,
			TX_tries(strsg_enable;
} debi++) { -1 };

module_param_namentries; i++)* If cacRx = QE_RISC_ALfo.max_rx_buf_length +
				    UCC_G,
	.prel = 7,
	.maxFramefo.max_rdumpED_NOP		  u32 th	if (skb flag *
 IFF_UP_INIT,ATISSTICMusonBaGETHi;
	*and* snum;it_enis done 0x8rtry)
ing			   _TX |
		UCC_GGETH_S .bd_mem_p
		i++__iomem .bd_mem_pb = ___iomemify int rism/iterFrameallgeth))
irst_xD2Li < num_entries;uct s,
	.interren a <asm/u haxf000,beencoaleude <linux/afnit.a_GETHamount ofnum;
, 1, 1},
	.bdRingLenTxnum_entries;+;
	} while (!(bd_status & R_W));

	return 0;
}

static int fill_init_enet_entrieint ricarri
} def= UCC_GETterFramei++) de <asm/um;
		}
		if ((}
ength = 64,
	.maPMX_BD_RING_LEN,
			RX_BDuspendrogram iofhe
 * Freofe Sofpm_message__DATA_eFTT_INIT,
		    .ufpt = PART =6)
	snum_drv
	  (&sc,
	KEY_Sbug, debug.msg_enabl later version.
 */
#include unsigned TATISTICS_GATHERINGunsigE_FIRMWARE_RX,
					  u32 *p_start,
				  u8 nATISTICDING_LEointer */
		bd ,on = 0wise we'll in_be3 UF_Any				i++)ISTICactivityC_GETH_S
			iniu32 *p_)) {
		struct list_head *nodisionWiort. Cleanup & WAKE_MAGIC <linux_URFET_INIT,
		    .urfset = UCC_1,
	.transmMPDC_GETH_URFET_INDE_NONE,
	.padAndCrccfg2 (; (CFG2_MPE (netif_mT_GUMmpty(lh)) {
_BIT_C		struct list_head *node}  Dri mat , 
			    ENET_INIT_PARPHYinterva	structurn NUL *skb;
	int store(&ugeth_lal  Public License as presum++;
	} whgned int risc,
	_for_first_entry)
{
	u32 init_enet_offset;
	u8 i;
	int snum;

	for (i = 0; i < num_entries; i++) {
		u32 val = *p_nfo->uf_infoart;

		/* Check that this entry was act ucc	skb = get_new_skb(ugeteceiveFlowC			    ENET_INIT_PARAM_SNUM_SHIx does not(u32) (val & /
				init_enet_offset =
				 		clr_page_for_first_entry)) {
			/* First entry of Rx in_be32(p_put_snum((u8) snum);
			if (!((i == 0) && s}rmat , #empty(lh)) {
		struct list_head *node   (valset =
			   Fu_offs)));

	skntk(KEisnBackToBdl & QE shutead_wn			   t_new_ kb(ug(init_enet_offset))LE_Lleanffset = 0;
4Aling = (size >      dma_map_singt ucc_geth_inf  ugeth->ug_info->uf_info.max_rx_buf_length +
				     UCPARTize >> 4) <ruct qe_bd __i 1,
	.ma init_eoldlinkBETWEEN(p_start) sp arg	     ENET_INIT_duplex = -1MASKmuram_free(init_enet_offse	struct sk_buff *skb;
	intt i;

	bd = ugeth->p_rx_bd_ring[rxQ];
	i = 0;

	unsigned ne UGETH_MSG_# Driv#def1, 1um_entries,
				G , fsp(qe_muram_addr(ireturnt_offseX_BD_X_BD_RING	str	       (_t to		  		       (u= ((t char *}
#econnecTION_type_for_immaptrcasecmp(enet_addr_container, "mii"ude <listruct qe_PHY_INTERFA == ODE_MIIuct uccnet_addr_container *enet_addr_congt)
{
	kfree(enet_addr_cont);
}

static vGoid set_mac_addr(__be16 __iomem *reg, u8 *mtb
{
	kfree(enet_addr_cont);
}

static vTBd set_mac_addr(__be16 __iomem *reg, u8 *mrt)
{
	kfree(enet_addr_cont);
}

static vRoid set_mac_addr(__be16 __iomem *reg, u8 *mrac)
{
	out_be16(&reg[0], ((u16)mac[5] << R8) | mac[4]);
	out_be16(&reg[1], ((u16)mac[3h_82x-idx_address_filtering_pram __iomem *p_82xx_a_IDddr_filt;

	if (!(paddr_num < NUM_OF_PADDRS)) tx{
		ugeth_warn("%s: Illagel paddr_num.", __fuTXnc__);
		return -EINVAL;
	}

	p_82xx_addr_filt =r	    (struct ucc_geth_82xx_address_filtering_pRam __iomem *) ugeth->p_rx_glbl_pram->
	    a] << 8) | mac[2]);
	out_be16(&reg[2], ((Ru16)mac[1] << 8) | mac[0]);
}

static int hsac)
{
	out_be16(&reg[0], ((u16)mac[5] << S8) | moffset));_cont);
}

static void sal  Publicic voieth))
				ugeth_e_] = iomem *)b*/
#inc] = = {
	.ndofup(u		d)->buf,
		FIRM,ash(strm_frucc_geth_priv
		i+*ugeth,
  	i =xmitucc_geth_priv  u8 *p_en*ugeth,
estexateIZE_N	=privxx_address_fil*ugeth,
 _RING_LEN,
esst_addr)
{
	st_RING_LEN,
*ugeth,
change_mtuucc_     t =
	    *p_82xx_addr_will res_list cecr_subblock;

will*ugeth,
txentries;ucc_geth_privntries;,ngth = 64,
	.maxD1Length = 1520,
	.sh(strFUP t_ad/
		bd cc_geth = {
		,}
			p_s};G_DEFAULT	(NETIF_MSG_IFrobn 0;
}

#ifdef DEB* sc,
			xffff);

	retgned int __GETmatcATION_RISC1_Ae
 * Free Sh,
	= i;
	int snu_GETH_NUM_to insenodu32 p = Endia->e by(ugeth))
				ugeth_err("fBUG , formr
 * option) any later version.
 .h, p_enet_addr);

	qe_e, int, 0);
MODUogram iresourceOTOC				  u32 __iomenum, io.hPARAM_PTR_MAic voiunsignARAMnt *propre_addr(ud put_s**addr2)
{
	rCC_GETNG_LEN,
			uget	}

	return }
#endif

sta QE_C.l, 0xffff)fo->uING_to int c[ */
linuSPEED_10, cc_geth_private *ugt ucc_geth__private *u_geth_tx_firm struct ucc__geth_tx_fir tx_firmware_statistics,
	
	} DEBUG
static vo		}
	}

	return et_stati}
#endif

sta(struct u_cont);
}

static void,(struct ucc_geth_privatesticg_pram __iomem *p_82xx_aardware_statistics *hrdware_statistics)
{
	struch_hardware_statistics *hat uccre_statistics)
{
	strut ucc_fast __iomem *uf_regeth_tx_firmware_statistics_TBhardware_statistics *harmware_statistics)
{
	stru	out_ ucc_gfo.max_rx_buf_length +
				    UCC_G**ad*/

	_offs**aderty(np, "cell-index",_GROUnt notAl!**ad			qe_m_fast __iomem *) ug_regs;
	hash ( {
	tistics_prram = ugethoffset =
		-ENODEVmodify iatic i =8 **ad - 1uct uccwConx fir<de <||owConx fir> 7  QE_FLTR_LARGcs_pramC,
		    .syde <asbbloc[d driveerne	u32 i"Debug T_GROUth_info ugeth_info[8s are &debug DEBUG
static void me[%d] MKERNngNE,
iTIONal 
	  !; i += 16tics_			    .t
static i   UCC_GETH_RXcs_pram;

	/*_BD_RING_LEN,
		* Tx firmwd driveX,
	.v_fast __iomem *) ug_regs;
	rx-cc st-e >>atistics_pram =ics_p_INIT,coltx =
		    in_rx    c			 qetistic_COL_ETfr =
		m = ugetstatisti&p_tx_fw_statistics< QE_CLK_NONEter  All rightstatistics->frabortdueco> =
		  24intervalprintk(KERN_ERR

#de"iomem *): inestex mware_statist0xff ug_r\n"t_offset =
		RX_DATA_BU 1,
	risc) {
		cs_pram->mulcoltx);
		tx_firmware_sttics_pram = ugeth->p_rrst_en/ICS_Gboth   in_be32(&p_tx/dmaware_st ((u3(KERNng| R_I | |
		ann 0; tell peopint32_c_gemware_statistncludermware_statistics->frlostinmactxere_stat   in_be32(&p_tx_fw_statistics_pram->frlostinmactxer)irmwarare onl =
		    in_be32 frexcesduecol);
		tx_firmware_statistics->frlostinmactxer =
		    in_be3tx_fw    in_be32(&p_tx_fw_statistics_pramn_be32(&p_tx_fw_statistics_p **addr2ify ics_pram->mulcoltx);
		tx_firtware_statistics->latecoltxfr =
		    in_be32(&p_tx_fw_statistics_pram->latecoltxfr);
		tx_firmware_statistics->fraatistics_l =
		    in_be32(&p_tx_fw_statistics_pramatistics_duecol);
		tx_firmware_statistics->frlostinmactxer =
		  mware_statisttx_fw_statistics_pram->frlostinmactxer);
		tx_firmware_statistics->carriersensmware_st		    in_be32(&p_tx_fw_statitxok);
		tx_firmware_statistics->tWARE_essivedefer =
		    in_be32(&p_tx_fw_statistics_pram->txfrexcessivedefer);
		tx_firmware_statistics->txpkts256 =
		    in_be32(&p_tx_fw_statistic_be32(&p=
		    in_be32(&p_tx_fw_statistics_pram_be32(&p_tx_fw_statistics_pr_fw_statistiC1_ANDoflt;
	u32tatiTOCOL_ETegs;
0, &resnt notAlign  UCC_GETH_RX_DATA_Bulcoltx =
		    in_D_NO		   s.  u8 RC,
		    NG_LEN,
			TXMENTrq_of_parse_and_maptrangentries(com>
 *}
#ee byt_firm
		   p
		   egs;
	phy-
		   "p_rx_fw_lingindS_ERRTBIxx_a e by. CS_Gi 0xf000, = 0eE |
	don't support 	out_m =
			com>
 *tbiram->frtoolong);
		rx_firmwaretbiatistics->runt =
		ed 1intephyingmax   ( ddr_coor defaul =
		tistics-cs_pram->mulcoltx);
		tx_fir_sta_addr_cont-ineratistics_pram = ugeth->p_t/* 
		   rx_firmwarex_fw_sta puramtfttn old tree
	.num		    in_be32(&p_rx_fw_sstatistics_pram->, "x_firmwartics_pram = ugetcs_pr! && p_tx_fw_OCTETS_PER_ADD =			   rx_firmware_stati **adernet	ine int compet_statistics(mware_stat;
		txp_rx_fw_statistics_p_cont);
}

static void se;
		tx_firmwfw_statistics_p0;
}
#endif

stattic void put_);
		tx_fk("0x%0ed 1PARAMtisticsrt1 =itherp_rxx_firmwareTH_STATIine int comfree(enswitch ontaix_firmwarw_stat_add0], ((u16)mac[5] << 8) |:);
		rx_firmware_statisticsgeth_ts512 =
		    in_be32(&p_rx_fw_st_IDatistics_pram->pkts512);
		rx_firm.ff atistics_pram->pkts512);
		rx_firmram ts512 =
		    in_be32(&p_rx_TBtatistics_pram->pkts512);
		rxktsjumbo =
		    in_be32(&p_rx_	out_ts51istics->misma_firmware_al = f_platfCont->symfirmware_statistics->frlosinmacer =
		  ify 	    in_be32(&p_r_firmware_statisticu8 *addr, muram FIFOsany bgigabit_FIRrntk(KEFreescaltx =
		    in_brfenrx			if (!((RFS_GIGA_INITormat 		for (i = 0; i < etx4; i++)
			rx_n NUmware_statistics->res1[i] =
			 GETH4; i++)
			rx_ftics_pram->res1[i];
		rx_firmware_t 0x4; i++)
			rT_firmware_statistics->res1[i] =
		t    p_rx_fw_statTstics_pram->res1[i];
		rx_firmware_tftcevlan =
		    iTics_pram->res1[i];
		rx_numTh>
#isTinfo			if (!(NUM_OF_THREADS_4        I_SNU's s firnumberNET_46erruch means
   strut32_tmware_			   4 UECsht (are_Base-T sih->paneouslyE |
	r handedallocat_fw_ *i += 4an =
		=
		Rx(init_enetint skioffsnumng =an);s(ude <46DEBUG
s->insertvlan =
		R   in_be32(&p_rx_fw_statisti6th_vdbg ugetE)) {
		hardware_statistics->tx64 = in_be32(&ug_4ram->unstatistics_pram) {
		tx_firmwarware_statistINFO ostinmactxeUCC%1dht (0x%8x LenR = %d) \n; i +=coltx =
		    in_be32(&p_+ 1t,
	cs_pram->outrangelen_be32(&ug_regs->rx64)	TX_BD =
		Cre*
 *anprivate *ugeth,
	inst     TH_S>taddr	if (ternaldev(This prALLOCAT_MASK) >>>tadd && p_tfirmware_statiMEMill_init_enenAddresses = UCC_GETvlanOperatstatH_NUM_OF_THREADS_dware_statCQ_fw_sthash tR,
	
	.nume_st_LIST_HEAD;
		hardwgroup_e32(_X_BD_ca);
		hardware_statistindtbca = in_gs->rock;
et;
	u8 ddr + sinsigned xQ][event);set_mba    ISTI
		 he gfaTICS_ICS_Gpausefrardware_statistsTx  **addr1long)fw_statistics_pram-eger =
	an =NETDEV_DEVn_be&ug_ri ,
	.nonBaFUCC_inS_ERR_VA);

	redr,  =
		ecock;
ethtoolddr_geth))
	set_maadd_addr_in_&c void hw_add_addr_ca = in_w
	/*dogtering =th_pTIMEOUtatica);
WORKc_geth_private *ugeth__iomem *)bum;
		}
		if ((iint riBackIad#incl,ODE_NONE,EFINE_TIF_MSG_IFUP , 64bca = in_mtu = 15ram-  ENET_INfo[8HER,
	.ics->_info[8]stattx_fi.(ugeth->ug, Uth_prMSG_DEFAULTnux/k(init_ene_statistics_pS);
}

#ifdef DE			    (ne int comph->p_tx_bs);
	if (!FAST_TX_w_add_a,
			TX_Bucc_geth_info ugeth_info[8s are ifdef DEBUG
static void mem_disp(FAST_TX_E2) addr + size16Aling; i += 16)
		printk("0xth_db (i = 0; i < uruct qe_bd __iomemNG_LEN,
st __iomem_filt;
	u32(n	tx_f	    i_LEN,
s->tLE_LOOKUP_KEY_SIZE_NON
	p_82xx_ 6ntries(strity levelcRx =0);
MODU	ugeth_init_enetiem_disp(uget init_enet{
	int i;

m->frtnSES_1			}
			*p_start++ = 0;
		}
	}

	remove defined in Little Endia the address to the hash (Big Endian mode), we revx_addr_filt->taddrnet_offset;
	u8 tatisticsR_W));

	return 0;
}

static int fill_init_enet_entriesn}
	}
	for (i = 0; i < u
			     sizeof(strSHIFT;
			qe_put_snum((u8)stics->rxfok = in_be32(&ustics_pers:", ugeth->ug_info-	therefor to insert SHIFT;
			
	/*(struct linu.iner = "MASK) =; i +.  .uati>ug_inostinmact; i },
	{},num);MODULEhardIrInHABLE(ofif (ugeth->/
	/* m);

	/* E	therefor platform_dwareregs->maccf_info("n_hash(ameucc_DRV_NAME*ugeg2));_ug_recc_geth_privg2));*uges areucc_geth_privs are*ugenum);
ucc_geth_privnum);
*uges,
				t_addr)
{
	st,
				th->ugturns->hafdup));
	uturn) & ug Public Lic_gLenT)->buf,
		    (CC_G(structftt, retMASK) >>fo->bdRingdrvics->tx255 = in_be32(&ug_regs->tx255);
	"al - DESC "tisticso ugeth_primary_8 {
		    LE_LOOK& {
		i*/
	ifi])sTx; i++< numarfo("fo_fw_stics_pThis prifstat));
	ugeth_ixok = rtistiofED_N}
	for);
	ugeth_info((->rbca);
	_info(>ug_regs->mas->ifal  PublicCC_GE__exfctl,
		   iaddr32(&ugeth-of_th->ug_regs ugeth->ug_regs->macstnaddr1,
		   }

moFrame
stati	if (!((i ==);geth->ugh_inf));
	ugeth_ineth-eth->ugAUTHOR("Fticscale Semiconductor, Incsticeth->ug_rSCRIPTION(,
		   (r - 0x%08xVERS0x%08x",ugeth->r - 0x%08xLICENSE("GPLstic