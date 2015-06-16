/* cassini.c: Sun Microsystems Cassini(+) ethernet driver.
 *
 * Copyright (C) 2004 Sun Microsystems Inc.
 * Copyright (C) 2003 Adrian Sun (asun@darksunrising.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * This driver uses the sungem driver (c) David Miller
 * (davem@redhat.com) as its basis.
 *
 * The cassini chip has a number of features that distinguish it from
 * the gem chip:
 *  4 transmit descriptor rings that are used for either QoS (VLAN) or
 *      load balancing (non-VLAN mode)
 *  batching of multiple packets
 *  multiple CPU dispatching
 *  page-based RX descriptor engine with separate completion rings
 *  Gigabit support (GMII and PCS interface)
 *  MIF link up/down detection works
 *
 * RX is handled by page sized buffers that are attached as fragments to
 * the skb. here's what's done:
 *  -- driver allocates pages at a time and keeps reference counts
 *     on them.
 *  -- the upper protocol layers assume that the header is in the skb
 *     itself. as a result, cassini will copy a small amount (64 bytes)
 *     to make them happy.
 *  -- driver appends the rest of the data pages as frags to skbuffs
 *     and increments the reference count
 *  -- on page reclamation, the driver swaps the page with a spare page.
 *     if that page is still in use, it frees its reference to that page,
 *     and allocates a new page for use. otherwise, it just recycles the
 *     the page.
 *
 * NOTE: cassini can parse the header. however, it's not worth it
 *       as long as the network stack requires a header copy.
 *
 * TX has 4 queues. currently these queues are used in a round-robin
 * fashion for load balancing. They can also be used for QoS. for that
 * to work, however, QoS information needs to be exposed down to the driver
 * level so that subqueues get targetted to particular transmit rings.
 * alternatively, the queues can be configured via use of the all-purpose
 * ioctl.
 *
 * RX DATA: the rx completion ring has all the info, but the rx desc
 * ring has all of the data. RX can conceivably come in under multiple
 * interrupts, but the INT# assignment needs to be set up properly by
 * the BIOS and conveyed to the driver. PCI BIOSes don't know how to do
 * that. also, the two descriptor rings are designed to distinguish between
 * encrypted and non-encrypted packets, but we use them for buffering
 * instead.
 *
 * by default, the selective clear mask is set up to process rx packets.
 */


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/compiler.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/vmalloc.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/list.h>
#include <linux/dma-mapping.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/ethtool.h>
#include <linux/crc32.h>
#include <linux/random.h>
#include <linux/mii.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/mutex.h>
#include <linux/firmware.h>

#include <net/checksum.h>

#include <asm/atomic.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/byteorder.h>
#include <asm/uaccess.h>

#define cas_page_map(x)      kmap_atomic((x), KM_SKB_DATA_SOFTIRQ)
#define cas_page_unmap(x)    kunmap_atomic((x), KM_SKB_DATA_SOFTIRQ)
#define CAS_NCPUS            num_online_cpus()

#if defined(CONFIG_CASSINI_NAPI) && defined(HAVE_NETDEV_POLL)
#define USE_NAPI
#define cas_skb_release(x)  netif_receive_skb(x)
#else
#define cas_skb_release(x)  netif_rx(x)
#endif

/* select which firmware to use */
#define USE_HP_WORKAROUND
#define HP_WORKAROUND_DEFAULT /* select which firmware to use as default */
#define CAS_HP_ALT_FIRMWARE   cas_prog_null /* alternate firmware */

#include "cassini.h"

#define USE_TX_COMPWB      /* use completion writeback registers */
#define USE_CSMA_CD_PROTO  /* standard CSMA/CD */
#define USE_RX_BLANK       /* hw interrupt mitigation */
#undef USE_ENTROPY_DEV     /* don't test for entropy device */

/* NOTE: these aren't useable unless PCI interrupts can be assigned.
 * also, we need to make cp->lock finer-grained.
 */
#undef  USE_PCI_INTB
#undef  USE_PCI_INTC
#undef  USE_PCI_INTD
#undef  USE_QOS

#undef  USE_VPD_DEBUG       /* debug vpd information if defined */

/* rx processing options */
#define USE_PAGE_ORDER      /* specify to allocate large rx pages */
#define RX_DONT_BATCH  0    /* if 1, don't batch flows */
#define RX_COPY_ALWAYS 0    /* if 0, use frags */
#define RX_COPY_MIN    64   /* copy a little to make upper layers happy */
#undef  RX_COUNT_BUFFERS    /* define to calculate RX buffer stats */

#define DRV_MODULE_NAME		"cassini"
#define PFX DRV_MODULE_NAME	": "
#define DRV_MODULE_VERSION	"1.6"
#define DRV_MODULE_RELDATE	"21 May 2008"

#define CAS_DEF_MSG_ENABLE	  \
	(NETIF_MSG_DRV		| \
	 NETIF_MSG_PROBE	| \
	 NETIF_MSG_LINK		| \
	 NETIF_MSG_TIMER	| \
	 NETIF_MSG_IFDOWN	| \
	 NETIF_MSG_IFUP		| \
	 NETIF_MSG_RX_ERR	| \
	 NETIF_MSG_TX_ERR)

/* length of time before we decide the hardware is borked,
 * and dev->tx_timeout() should be called to fix the problem
 */
#define CAS_TX_TIMEOUT			(HZ)
#define CAS_LINK_TIMEOUT                (22*HZ/10)
#define CAS_LINK_FAST_TIMEOUT           (1)

/* timeout values for state changing. these specify the number
 * of 10us delays to be used before giving up.
 */
#define STOP_TRIES_PHY 1000
#define STOP_TRIES     5000

/* specify a minimum frame size to deal with some fifo issues
 * max mtu == 2 * page size - ethernet header - 64 - swivel =
 *            2 * page_size - 0x50
 */
#define CAS_MIN_FRAME			97
#define CAS_1000MB_MIN_FRAME            255
#define CAS_MIN_MTU                     60
#define CAS_MAX_MTU                     min(((cp->page_size << 1) - 0x50), 9000)

#if 1
/*
 * Eliminate these and use separate atomic counters for each, to
 * avoid a race condition.
 */
#else
#define CAS_RESET_MTU                   1
#define CAS_RESET_ALL                   2
#define CAS_RESET_SPARE                 3
#endif

static char version[] __devinitdata =
	DRV_MODULE_NAME ".c:v" DRV_MODULE_VERSION " (" DRV_MODULE_RELDATE ")\n";

static int cassini_debug = -1;	/* -1 == use CAS_DEF_MSG_ENABLE as value */
static int link_mode;

MODULE_AUTHOR("Adrian Sun (asun@darksunrising.com)");
MODULE_DESCRIPTION("Sun Cassini(+) ethernet driver");
MODULE_LICENSE("GPL");
MODULE_FIRMWARE("sun/cassini.bin");
module_param(cassini_debug, int, 0);
MODULE_PARM_DESC(cassini_debug, "Cassini bitmapped debugging message enable value");
module_param(link_mode, int, 0);
MODULE_PARM_DESC(link_mode, "default link mode");

/*
 * Work around for a PCS bug in which the link goes down due to the chip
 * being confused and never showing a link status of "up."
 */
#define DEFAULT_LINKDOWN_TIMEOUT 5
/*
 * Value in seconds, for user input.
 */
static int linkdown_timeout = DEFAULT_LINKDOWN_TIMEOUT;
module_param(linkdown_timeout, int, 0);
MODULE_PARM_DESC(linkdown_timeout,
"min reset interval in sec. for PCS linkdown issue; disabled if not positive");

/*
 * value in 'ticks' (units used by jiffies). Set when we init the
 * module because 'HZ' in actually a function call on some flavors of
 * Linux.  This will default to DEFAULT_LINKDOWN_TIMEOUT * HZ.
 */
static int link_transition_timeout;



static u16 link_modes[] __devinitdata = {
	BMCR_ANENABLE,			 /* 0 : autoneg */
	0,				 /* 1 : 10bt half duplex */
	BMCR_SPEED100,			 /* 2 : 100bt half duplex */
	BMCR_FULLDPLX,			 /* 3 : 10bt full duplex */
	BMCR_SPEED100|BMCR_FULLDPLX,	 /* 4 : 100bt full duplex */
	CAS_BMCR_SPEED1000|BMCR_FULLDPLX /* 5 : 1000bt full duplex */
};

static struct pci_device_id cas_pci_tbl[] __devinitdata = {
	{ PCI_VENDOR_ID_SUN, PCI_DEVICE_ID_SUN_CASSINI,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ PCI_VENDOR_ID_NS, PCI_DEVICE_ID_NS_SATURN,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0, }
};

MODULE_DEVICE_TABLE(pci, cas_pci_tbl);

static void cas_set_link_modes(struct cas *cp);

static inline void cas_lock_tx(struct cas *cp)
{
	int i;

	for (i = 0; i < N_TX_RINGS; i++)
		spin_lock(&cp->tx_lock[i]);
}

static inline void cas_lock_all(struct cas *cp)
{
	spin_lock_irq(&cp->lock);
	cas_lock_tx(cp);
}

/* WTZ: QA was finding deadlock problems with the previous
 * versions after long test runs with multiple cards per machine.
 * See if replacing cas_lock_all with safer versions helps. The
 * symptoms QA is reporting match those we'd expect if interrupts
 * aren't being properly restored, and we fixed a previous deadlock
 * with similar symptoms by using save/restore versions in other
 * places.
 */
#define cas_lock_all_save(cp, flags) \
do { \
	struct cas *xxxcp = (cp); \
	spin_lock_irqsave(&xxxcp->lock, flags); \
	cas_lock_tx(xxxcp); \
} while (0)

static inline void cas_unlock_tx(struct cas *cp)
{
	int i;

	for (i = N_TX_RINGS; i > 0; i--)
		spin_unlock(&cp->tx_lock[i - 1]);
}

static inline void cas_unlock_all(struct cas *cp)
{
	cas_unlock_tx(cp);
	spin_unlock_irq(&cp->lock);
}

#define cas_unlock_all_restore(cp, flags) \
do { \
	struct cas *xxxcp = (cp); \
	cas_unlock_tx(xxxcp); \
	spin_unlock_irqrestore(&xxxcp->lock, flags); \
} while (0)

static void cas_disable_irq(struct cas *cp, const int ring)
{
	/* Make sure we won't get any more interrupts */
	if (ring == 0) {
		writel(0xFFFFFFFF, cp->regs + REG_INTR_MASK);
		return;
	}

	/* disable completion interrupts and selectively mask */
	if (cp->cas_flags & CAS_FLAG_REG_PLUS) {
		switch (ring) {
#if defined (USE_PCI_INTB) || defined(USE_PCI_INTC) || defined(USE_PCI_INTD)
#ifdef USE_PCI_INTB
		case 1:
#endif
#ifdef USE_PCI_INTC
		case 2:
#endif
#ifdef USE_PCI_INTD
		case 3:
#endif
			writel(INTRN_MASK_CLEAR_ALL | INTRN_MASK_RX_EN,
			       cp->regs + REG_PLUS_INTRN_MASK(ring));
			break;
#endif
		default:
			writel(INTRN_MASK_CLEAR_ALL, cp->regs +
			       REG_PLUS_INTRN_MASK(ring));
			break;
		}
	}
}

static inline void cas_mask_intr(struct cas *cp)
{
	int i;

	for (i = 0; i < N_RX_COMP_RINGS; i++)
		cas_disable_irq(cp, i);
}

static void cas_enable_irq(struct cas *cp, const int ring)
{
	if (ring == 0) { /* all but TX_DONE */
		writel(INTR_TX_DONE, cp->regs + REG_INTR_MASK);
		return;
	}

	if (cp->cas_flags & CAS_FLAG_REG_PLUS) {
		switch (ring) {
#if defined (USE_PCI_INTB) || defined(USE_PCI_INTC) || defined(USE_PCI_INTD)
#ifdef USE_PCI_INTB
		case 1:
#endif
#ifdef USE_PCI_INTC
		case 2:
#endif
#ifdef USE_PCI_INTD
		case 3:
#endif
			writel(INTRN_MASK_RX_EN, cp->regs +
			       REG_PLUS_INTRN_MASK(ring));
			break;
#endif
		default:
			break;
		}
	}
}

static inline void cas_unmask_intr(struct cas *cp)
{
	int i;

	for (i = 0; i < N_RX_COMP_RINGS; i++)
		cas_enable_irq(cp, i);
}

static inline void cas_entropy_gather(struct cas *cp)
{
#ifdef USE_ENTROPY_DEV
	if ((cp->cas_flags & CAS_FLAG_ENTROPY_DEV) == 0)
		return;

	batch_entropy_store(readl(cp->regs + REG_ENTROPY_IV),
			    readl(cp->regs + REG_ENTROPY_IV),
			    sizeof(uint64_t)*8);
#endif
}

static inline void cas_entropy_reset(struct cas *cp)
{
#ifdef USE_ENTROPY_DEV
	if ((cp->cas_flags & CAS_FLAG_ENTROPY_DEV) == 0)
		return;

	writel(BIM_LOCAL_DEV_PAD | BIM_LOCAL_DEV_PROM | BIM_LOCAL_DEV_EXT,
	       cp->regs + REG_BIM_LOCAL_DEV_EN);
	writeb(ENTROPY_RESET_STC_MODE, cp->regs + REG_ENTROPY_RESET);
	writeb(0x55, cp->regs + REG_ENTROPY_RAND_REG);

	/* if we read back 0x0, we don't have an entropy device */
	if (readb(cp->regs + REG_ENTROPY_RAND_REG) == 0)
		cp->cas_flags &= ~CAS_FLAG_ENTROPY_DEV;
#endif
}

/* access to the phy. the following assumes that we've initialized the MIF to
 * be in frame rather than bit-bang mode
 */
static u16 cas_phy_read(struct cas *cp, int reg)
{
	u32 cmd;
	int limit = STOP_TRIES_PHY;

	cmd = MIF_FRAME_ST | MIF_FRAME_OP_READ;
	cmd |= CAS_BASE(MIF_FRAME_PHY_ADDR, cp->phy_addr);
	cmd |= CAS_BASE(MIF_FRAME_REG_ADDR, reg);
	cmd |= MIF_FRAME_TURN_AROUND_MSB;
	writel(cmd, cp->regs + REG_MIF_FRAME);

	/* poll for completion */
	while (limit-- > 0) {
		udelay(10);
		cmd = readl(cp->regs + REG_MIF_FRAME);
		if (cmd & MIF_FRAME_TURN_AROUND_LSB)
			return (cmd & MIF_FRAME_DATA_MASK);
	}
	return 0xFFFF; /* -1 */
}

static int cas_phy_write(struct cas *cp, int reg, u16 val)
{
	int limit = STOP_TRIES_PHY;
	u32 cmd;

	cmd = MIF_FRAME_ST | MIF_FRAME_OP_WRITE;
	cmd |= CAS_BASE(MIF_FRAME_PHY_ADDR, cp->phy_addr);
	cmd |= CAS_BASE(MIF_FRAME_REG_ADDR, reg);
	cmd |= MIF_FRAME_TURN_AROUND_MSB;
	cmd |= val & MIF_FRAME_DATA_MASK;
	writel(cmd, cp->regs + REG_MIF_FRAME);

	/* poll for completion */
	while (limit-- > 0) {
		udelay(10);
		cmd = readl(cp->regs + REG_MIF_FRAME);
		if (cmd & MIF_FRAME_TURN_AROUND_LSB)
			return 0;
	}
	return -1;
}

static void cas_phy_powerup(struct cas *cp)
{
	u16 ctl = cas_phy_read(cp, MII_BMCR);

	if ((ctl & BMCR_PDOWN) == 0)
		return;
	ctl &= ~BMCR_PDOWN;
	cas_phy_write(cp, MII_BMCR, ctl);
}

static void cas_phy_powerdown(struct cas *cp)
{
	u16 ctl = cas_phy_read(cp, MII_BMCR);

	if (ctl & BMCR_PDOWN)
		return;
	ctl |= BMCR_PDOWN;
	cas_phy_write(cp, MII_BMCR, ctl);
}

/* cp->lock held. note: the last put_page will free the buffer */
static int cas_page_free(struct cas *cp, cas_page_t *page)
{
	pci_unmap_page(cp->pdev, page->dma_addr, cp->page_size,
		       PCI_DMA_FROMDEVICE);
	__free_pages(page->buffer, cp->page_order);
	kfree(page);
	return 0;
}

#ifdef RX_COUNT_BUFFERS
#define RX_USED_ADD(x, y)       ((x)->used += (y))
#define RX_USED_SET(x, y)       ((x)->used  = (y))
#else
#define RX_USED_ADD(x, y)
#define RX_USED_SET(x, y)
#endif

/* local page allocation routines for the receive buffers. jumbo pages
 * require at least 8K contiguous and 8K aligned buffers.
 */
static cas_page_t *cas_page_alloc(struct cas *cp, const gfp_t flags)
{
	cas_page_t *page;

	page = kmalloc(sizeof(cas_page_t), flags);
	if (!page)
		return NULL;

	INIT_LIST_HEAD(&page->list);
	RX_USED_SET(page, 0);
	page->buffer = alloc_pages(flags, cp->page_order);
	if (!page->buffer)
		goto page_err;
	page->dma_addr = pci_map_page(cp->pdev, page->buffer, 0,
				      cp->page_size, PCI_DMA_FROMDEVICE);
	return page;

page_err:
	kfree(page);
	return NULL;
}

/* initialize spare pool of rx buffers, but allocate during the open */
static void cas_spare_init(struct cas *cp)
{
  	spin_lock(&cp->rx_inuse_lock);
	INIT_LIST_HEAD(&cp->rx_inuse_list);
	spin_unlock(&cp->rx_inuse_lock);

	spin_lock(&cp->rx_spare_lock);
	INIT_LIST_HEAD(&cp->rx_spare_list);
	cp->rx_spares_needed = RX_SPARE_COUNT;
	spin_unlock(&cp->rx_spare_lock);
}

/* used on close. free all the spare buffers. */
static void cas_spare_free(struct cas *cp)
{
	struct list_head list, *elem, *tmp;

	/* free spare buffers */
	INIT_LIST_HEAD(&list);
	spin_lock(&cp->rx_spare_lock);
	list_splice_init(&cp->rx_spare_list, &list);
	spin_unlock(&cp->rx_spare_lock);
	list_for_each_safe(elem, tmp, &list) {
		cas_page_free(cp, list_entry(elem, cas_page_t, list));
	}

	INIT_LIST_HEAD(&list);
#if 1
	/*
	 * Looks like Adrian had protected this with a different
	 * lock than used everywhere else to manipulate this list.
	 */
	spin_lock(&cp->rx_inuse_lock);
	list_splice_init(&cp->rx_inuse_list, &list);
	spin_unlock(&cp->rx_inuse_lock);
#else
	spin_lock(&cp->rx_spare_lock);
	list_splice_init(&cp->rx_inuse_list, &list);
	spin_unlock(&cp->rx_spare_lock);
#endif
	list_for_each_safe(elem, tmp, &list) {
		cas_page_free(cp, list_entry(elem, cas_page_t, list));
	}
}

/* replenish spares if needed */
static void cas_spare_recover(struct cas *cp, const gfp_t flags)
{
	struct list_head list, *elem, *tmp;
	int needed, i;

	/* check inuse list. if we don't need any more free buffers,
	 * just free it
	 */

	/* make a local copy of the list */
	INIT_LIST_HEAD(&list);
	spin_lock(&cp->rx_inuse_lock);
	list_splice_init(&cp->rx_inuse_list, &list);
	spin_unlock(&cp->rx_inuse_lock);

	list_for_each_safe(elem, tmp, &list) {
		cas_page_t *page = list_entry(elem, cas_page_t, list);

		/*
		 * With the lockless pagecache, cassini buffering scheme gets
		 * slightly less accurate: we might find that a page has an
		 * elevated reference count here, due to a speculative ref,
		 * and skip it as in-use. Ideally we would be able to reclaim
		 * it. However this would be such a rare case, it doesn't
		 * matter too much as we should pick it up the next time round.
		 *
		 * Importantly, if we find that the page has a refcount of 1
		 * here (our refcount), then we know it is definitely not inuse
		 * so we can reuse it.
		 */
		if (page_count(page->buffer) > 1)
			continue;

		list_del(elem);
		spin_lock(&cp->rx_spare_lock);
		if (cp->rx_spares_needed > 0) {
			list_add(elem, &cp->rx_spare_list);
			cp->rx_spares_needed--;
			spin_unlock(&cp->rx_spare_lock);
		} else {
			spin_unlock(&cp->rx_spare_lock);
			cas_page_free(cp, page);
		}
	}

	/* put any inuse buffers back on the list */
	if (!list_empty(&list)) {
		spin_lock(&cp->rx_inuse_lock);
		list_splice(&list, &cp->rx_inuse_list);
		spin_unlock(&cp->rx_inuse_lock);
	}

	spin_lock(&cp->rx_spare_lock);
	needed = cp->rx_spares_needed;
	spin_unlock(&cp->rx_spare_lock);
	if (!needed)
		return;

	/* we still need spares, so try to allocate some */
	INIT_LIST_HEAD(&list);
	i = 0;
	while (i < needed) {
		cas_page_t *spare = cas_page_alloc(cp, flags);
		if (!spare)
			break;
		list_add(&spare->list, &list);
		i++;
	}

	spin_lock(&cp->rx_spare_lock);
	list_splice(&list, &cp->rx_spare_list);
	cp->rx_spares_needed -= i;
	spin_unlock(&cp->rx_spare_lock);
}

/* pull a page from the list. */
static cas_page_t *cas_page_dequeue(struct cas *cp)
{
	struct list_head *entry;
	int recover;

	spin_lock(&cp->rx_spare_lock);
	if (list_empty(&cp->rx_spare_list)) {
		/* try to do a quick recovery */
		spin_unlock(&cp->rx_spare_lock);
		cas_spare_recover(cp, GFP_ATOMIC);
		spin_lock(&cp->rx_spare_lock);
		if (list_empty(&cp->rx_spare_list)) {
			if (netif_msg_rx_err(cp))
				printk(KERN_ERR "%s: no spare buffers "
				       "available.\n", cp->dev->name);
			spin_unlock(&cp->rx_spare_lock);
			return NULL;
		}
	}

	entry = cp->rx_spare_list.next;
	list_del(entry);
	recover = ++cp->rx_spares_needed;
	spin_unlock(&cp->rx_spare_lock);

	/* trigger the timer to do the recovery */
	if ((recover & (RX_SPARE_RECOVER_VAL - 1)) == 0) {
#if 1
		atomic_inc(&cp->reset_task_pending);
		atomic_inc(&cp->reset_task_pending_spare);
		schedule_work(&cp->reset_task);
#else
		atomic_set(&cp->reset_task_pending, CAS_RESET_SPARE);
		schedule_work(&cp->reset_task);
#endif
	}
	return list_entry(entry, cas_page_t, list);
}


static void cas_mif_poll(struct cas *cp, const int enable)
{
	u32 cfg;

	cfg  = readl(cp->regs + REG_MIF_CFG);
	cfg &= (MIF_CFG_MDIO_0 | MIF_CFG_MDIO_1);

	if (cp->phy_type & CAS_PHY_MII_MDIO1)
		cfg |= MIF_CFG_PHY_SELECT;

	/* poll and interrupt on link status change. */
	if (enable) {
		cfg |= MIF_CFG_POLL_EN;
		cfg |= CAS_BASE(MIF_CFG_POLL_REG, MII_BMSR);
		cfg |= CAS_BASE(MIF_CFG_POLL_PHY, cp->phy_addr);
	}
	writel((enable) ? ~(BMSR_LSTATUS | BMSR_ANEGCOMPLETE) : 0xFFFF,
	       cp->regs + REG_MIF_MASK);
	writel(cfg, cp->regs + REG_MIF_CFG);
}

/* Must be invoked under cp->lock */
static void cas_begin_auto_negotiation(struct cas *cp, struct ethtool_cmd *ep)
{
	u16 ctl;
#if 1
	int lcntl;
	int changed = 0;
	int oldstate = cp->lstate;
	int link_was_not_down = !(oldstate == link_down);
#endif
	/* Setup link parameters */
	if (!ep)
		goto start_aneg;
	lcntl = cp->link_cntl;
	if (ep->autoneg == AUTONEG_ENABLE)
		cp->link_cntl = BMCR_ANENABLE;
	else {
		cp->link_cntl = 0;
		if (ep->speed == SPEED_100)
			cp->link_cntl |= BMCR_SPEED100;
		else if (ep->speed == SPEED_1000)
			cp->link_cntl |= CAS_BMCR_SPEED1000;
		if (ep->duplex == DUPLEX_FULL)
			cp->link_cntl |= BMCR_FULLDPLX;
	}
#if 1
	changed = (lcntl != cp->link_cntl);
#endif
start_aneg:
	if (cp->lstate == link_up) {
		printk(KERN_INFO "%s: PCS link down.\n",
		       cp->dev->name);
	} else {
		if (changed) {
			printk(KERN_INFO "%s: link configuration changed\n",
			       cp->dev->name);
		}
	}
	cp->lstate = link_down;
	cp->link_transition = LINK_TRANSITION_LINK_DOWN;
	if (!cp->hw_running)
		return;
#if 1
	/*
	 * WTZ: If the old state was link_up, we turn off the carrier
	 * to replicate everything we do elsewhere on a link-down
	 * event when we were already in a link-up state..
	 */
	if (oldstate == link_up)
		netif_carrier_off(cp->dev);
	if (changed  && link_was_not_down) {
		/*
		 * WTZ: This branch will simply schedule a full reset after
		 * we explicitly changed link modes in an ioctl. See if this
		 * fixes the link-problems we were having for forced mode.
		 */
		atomic_inc(&cp->reset_task_pending);
		atomic_inc(&cp->reset_task_pending_all);
		schedule_work(&cp->reset_task);
		cp->timer_ticks = 0;
		mod_timer(&cp->link_timer, jiffies + CAS_LINK_TIMEOUT);
		return;
	}
#endif
	if (cp->phy_type & CAS_PHY_SERDES) {
		u32 val = readl(cp->regs + REG_PCS_MII_CTRL);

		if (cp->link_cntl & BMCR_ANENABLE) {
			val |= (PCS_MII_RESTART_AUTONEG | PCS_MII_AUTONEG_EN);
			cp->lstate = link_aneg;
		} else {
			if (cp->link_cntl & BMCR_FULLDPLX)
				val |= PCS_MII_CTRL_DUPLEX;
			val &= ~PCS_MII_AUTONEG_EN;
			cp->lstate = link_force_ok;
		}
		cp->link_transition = LINK_TRANSITION_LINK_CONFIG;
		writel(val, cp->regs + REG_PCS_MII_CTRL);

	} else {
		cas_mif_poll(cp, 0);
		ctl = cas_phy_read(cp, MII_BMCR);
		ctl &= ~(BMCR_FULLDPLX | BMCR_SPEED100 |
			 CAS_BMCR_SPEED1000 | BMCR_ANENABLE);
		ctl |= cp->link_cntl;
		if (ctl & BMCR_ANENABLE) {
			ctl |= BMCR_ANRESTART;
			cp->lstate = link_aneg;
		} else {
			cp->lstate = link_force_ok;
		}
		cp->link_transition = LINK_TRANSITION_LINK_CONFIG;
		cas_phy_write(cp, MII_BMCR, ctl);
		cas_mif_poll(cp, 1);
	}

	cp->timer_ticks = 0;
	mod_timer(&cp->link_timer, jiffies + CAS_LINK_TIMEOUT);
}

/* Must be invoked under cp->lock. */
static int cas_reset_mii_phy(struct cas *cp)
{
	int limit = STOP_TRIES_PHY;
	u16 val;

	cas_phy_write(cp, MII_BMCR, BMCR_RESET);
	udelay(100);
	while (--limit) {
		val = cas_phy_read(cp, MII_BMCR);
		if ((val & BMCR_RESET) == 0)
			break;
		udelay(10);
	}
	return (limit <= 0);
}

static int cas_saturn_firmware_init(struct cas *cp)
{
	const struct firmware *fw;
	const char fw_name[] = "sun/cassini.bin";
	int err;

	if (PHY_NS_DP83065 != cp->phy_id)
		return 0;

	err = request_firmware(&fw, fw_name, &cp->pdev->dev);
	if (err) {
		printk(KERN_ERR "cassini: Failed to load firmware \"%s\"\n",
		       fw_name);
		return err;
	}
	if (fw->size < 2) {
		printk(KERN_ERR "cassini: bogus length %zu in \"%s\"\n",
		       fw->size, fw_name);
		err = -EINVAL;
		goto out;
	}
	cp->fw_load_addr= fw->data[1] << 8 | fw->data[0];
	cp->fw_size = fw->size - 2;
	cp->fw_data = vmalloc(cp->fw_size);
	if (!cp->fw_data) {
		err = -ENOMEM;
		printk(KERN_ERR "cassini: \"%s\" Failed %d\n", fw_name, err);
		goto out;
	}
	memcpy(cp->fw_data, &fw->data[2], cp->fw_size);
out:
	release_firmware(fw);
	return err;
}

static void cas_saturn_firmware_load(struct cas *cp)
{
	int i;

	cas_phy_powerdown(cp);

	/* expanded memory access mode */
	cas_phy_write(cp, DP83065_MII_MEM, 0x0);

	/* pointer configuration for new firmware */
	cas_phy_write(cp, DP83065_MII_REGE, 0x8ff9);
	cas_phy_write(cp, DP83065_MII_REGD, 0xbd);
	cas_phy_write(cp, DP83065_MII_REGE, 0x8ffa);
	cas_phy_write(cp, DP83065_MII_REGD, 0x82);
	cas_phy_write(cp, DP83065_MII_REGE, 0x8ffb);
	cas_phy_write(cp, DP83065_MII_REGD, 0x0);
	cas_phy_write(cp, DP83065_MII_REGE, 0x8ffc);
	cas_phy_write(cp, DP83065_MII_REGD, 0x39);

	/* download new firmware */
	cas_phy_write(cp, DP83065_MII_MEM, 0x1);
	cas_phy_write(cp, DP83065_MII_REGE, cp->fw_load_addr);
	for (i = 0; i < cp->fw_size; i++)
		cas_phy_write(cp, DP83065_MII_REGD, cp->fw_data[i]);

	/* enable firmware */
	cas_phy_write(cp, DP83065_MII_REGE, 0x8ff8);
	cas_phy_write(cp, DP83065_MII_REGD, 0x1);
}


/* phy initialization */
static void cas_phy_init(struct cas *cp)
{
	u16 val;

	/* if we're in MII/GMII mode, set up phy */
	if (CAS_PHY_MII(cp->phy_type)) {
		writel(PCS_DATAPATH_MODE_MII,
		       cp->regs + REG_PCS_DATAPATH_MODE);

		cas_mif_poll(cp, 0);
		cas_reset_mii_phy(cp); /* take out of isolate mode */

		if (PHY_LUCENT_B0 == cp->phy_id) {
			/* workaround link up/down issue with lucent */
			cas_phy_write(cp, LUCENT_MII_REG, 0x8000);
			cas_phy_write(cp, MII_BMCR, 0x00f1);
			cas_phy_write(cp, LUCENT_MII_REG, 0x0);

		} else if (PHY_BROADCOM_B0 == (cp->phy_id & 0xFFFFFFFC)) {
			/* workarounds for broadcom phy */
			cas_phy_write(cp, BROADCOM_MII_REG8, 0x0C20);
			cas_phy_write(cp, BROADCOM_MII_REG7, 0x0012);
			cas_phy_write(cp, BROADCOM_MII_REG5, 0x1804);
			cas_phy_write(cp, BROADCOM_MII_REG7, 0x0013);
			cas_phy_write(cp, BROADCOM_MII_REG5, 0x1204);
			cas_phy_write(cp, BROADCOM_MII_REG7, 0x8006);
			cas_phy_write(cp, BROADCOM_MII_REG5, 0x0132);
			cas_phy_write(cp, BROADCOM_MII_REG7, 0x8006);
			cas_phy_write(cp, BROADCOM_MII_REG5, 0x0232);
			cas_phy_write(cp, BROADCOM_MII_REG7, 0x201F);
			cas_phy_write(cp, BROADCOM_MII_REG5, 0x0A20);

		} else if (PHY_BROADCOM_5411 == cp->phy_id) {
			val = cas_phy_read(cp, BROADCOM_MII_REG4);
			val = cas_phy_read(cp, BROADCOM_MII_REG4);
			if (val & 0x0080) {
				/* link workaround */
				cas_phy_write(cp, BROADCOM_MII_REG4,
					      val & ~0x0080);
			}

		} else if (cp->cas_flags & CAS_FLAG_SATURN) {
			writel((cp->phy_type & CAS_PHY_MII_MDIO0) ?
			       SATURN_PCFG_FSI : 0x0,
			       cp->regs + REG_SATURN_PCFG);

			/* load firmware to address 10Mbps auto-negotiation
			 * issue. NOTE: this will need to be changed if the
			 * default firmware gets fixed.
			 */
			if (PHY_NS_DP83065 == cp->phy_id) {
				cas_saturn_firmware_load(cp);
			}
			cas_phy_powerup(cp);
		}

		/* advertise capabilities */
		val = cas_phy_read(cp, MII_BMCR);
		val &= ~BMCR_ANENABLE;
		cas_phy_write(cp, MII_BMCR, val);
		udelay(10);

		cas_phy_write(cp, MII_ADVERTISE,
			      cas_phy_read(cp, MII_ADVERTISE) |
			      (ADVERTISE_10HALF | ADVERTISE_10FULL |
			       ADVERTISE_100HALF | ADVERTISE_100FULL |
			       CAS_ADVERTISE_PAUSE |
			       CAS_ADVERTISE_ASYM_PAUSE));

		if (cp->cas_flags & CAS_FLAG_1000MB_CAP) {
			/* make sure that we don't advertise half
			 * duplex to avoid a chip issue
			 */
			val  = cas_phy_read(cp, CAS_MII_1000_CTRL);
			val &= ~CAS_ADVERTISE_1000HALF;
			val |= CAS_ADVERTISE_1000FULL;
			cas_phy_write(cp, CAS_MII_1000_CTRL, val);
		}

	} else {
		/* reset pcs for serdes */
		u32 val;
		int limit;

		writel(PCS_DATAPATH_MODE_SERDES,
		       cp->regs + REG_PCS_DATAPATH_MODE);

		/* enable serdes pins on saturn */
		if (cp->cas_flags & CAS_FLAG_SATURN)
			writel(0, cp->regs + REG_SATURN_PCFG);

		/* Reset PCS unit. */
		val = readl(cp->regs + REG_PCS_MII_CTRL);
		val |= PCS_MII_RESET;
		writel(val, cp->regs + REG_PCS_MII_CTRL);

		limit = STOP_TRIES;
		while (--limit > 0) {
			udelay(10);
			if ((readl(cp->regs + REG_PCS_MII_CTRL) &
			     PCS_MII_RESET) == 0)
				break;
		}
		if (limit <= 0)
			printk(KERN_WARNING "%s: PCS reset bit would not "
			       "clear [%08x].\n", cp->dev->name,
			       readl(cp->regs + REG_PCS_STATE_MACHINE));

		/* Make sure PCS is disabled while changing advertisement
		 * configuration.
		 */
		writel(0x0, cp->regs + REG_PCS_CFG);

		/* Advertise all capabilities except half-duplex. */
		val  = readl(cp->regs + REG_PCS_MII_ADVERT);
		val &= ~PCS_MII_ADVERT_HD;
		val |= (PCS_MII_ADVERT_FD | PCS_MII_ADVERT_SYM_PAUSE |
			PCS_MII_ADVERT_ASYM_PAUSE);
		writel(val, cp->regs + REG_PCS_MII_ADVERT);

		/* enable PCS */
		writel(PCS_CFG_EN, cp->regs + REG_PCS_CFG);

		/* pcs workaround: enable sync detect */
		writel(PCS_SERDES_CTRL_SYNCD_EN,
		       cp->regs + REG_PCS_SERDES_CTRL);
	}
}


static int cas_pcs_link_check(struct cas *cp)
{
	u32 stat, state_machine;
	int retval = 0;

	/* The link status bit latches on zero, so you must
	 * read it twice in such a case to see a transition
	 * to the link being up.
	 */
	stat = readl(cp->regs + REG_PCS_MII_STATUS);
	if ((stat & PCS_MII_STATUS_LINK_STATUS) == 0)
		stat = readl(cp->regs + REG_PCS_MII_STATUS);

	/* The remote-fault indication is only valid
	 * when autoneg has completed.
	 */
	if ((stat & (PCS_MII_STATUS_AUTONEG_COMP |
		     PCS_MII_STATUS_REMOTE_FAULT)) ==
	    (PCS_MII_STATUS_AUTONEG_COMP | PCS_MII_STATUS_REMOTE_FAULT)) {
		if (netif_msg_link(cp))
			printk(KERN_INFO "%s: PCS RemoteFault\n",
			       cp->dev->name);
	}

	/* work around link detection issue by querying the PCS state
	 * machine directly.
	 */
	state_machine = readl(cp->regs + REG_PCS_STATE_MACHINE);
	if ((state_machine & PCS_SM_LINK_STATE_MASK) != SM_LINK_STATE_UP) {
		stat &= ~PCS_MII_STATUS_LINK_STATUS;
	} else if (state_machine & PCS_SM_WORD_SYNC_STATE_MASK) {
		stat |= PCS_MII_STATUS_LINK_STATUS;
	}

	if (stat & PCS_MII_STATUS_LINK_STATUS) {
		if (cp->lstate != link_up) {
			if (cp->opened) {
				cp->lstate = link_up;
				cp->link_transition = LINK_TRANSITION_LINK_UP;

				cas_set_link_modes(cp);
				netif_carrier_on(cp->dev);
			}
		}
	} else if (cp->lstate == link_up) {
		cp->lstate = link_down;
		if (link_transition_timeout != 0 &&
		    cp->link_transition != LINK_TRANSITION_REQUESTED_RESET &&
		    !cp->link_transition_jiffies_valid) {
			/*
			 * force a reset, as a workaround for the
			 * link-failure problem. May want to move this to a
			 * point a bit earlier in the sequence. If we had
			 * generated a reset a short time ago, we'll wait for
			 * the link timer to check the status until a
			 * timer expires (link_transistion_jiffies_valid is
			 * true when the timer is running.)  Instead of using
			 * a system timer, we just do a check whenever the
			 * link timer is running - this clears the flag after
			 * a suitable delay.
			 */
			retval = 1;
			cp->link_transition = LINK_TRANSITION_REQUESTED_RESET;
			cp->link_transition_jiffies = jiffies;
			cp->link_transition_jiffies_valid = 1;
		} else {
			cp->link_transition = LINK_TRANSITION_ON_FAILURE;
		}
		netif_carrier_off(cp->dev);
		if (cp->opened && netif_msg_link(cp)) {
			printk(KERN_INFO "%s: PCS link down.\n",
			       cp->dev->name);
		}

		/* Cassini only: if you force a mode, there can be
		 * sync problems on link down. to fix that, the following
		 * things need to be checked:
		 * 1) read serialink state register
		 * 2) read pcs status register to verify link down.
		 * 3) if link down and serial link == 0x03, then you need
		 *    to global reset the chip.
		 */
		if ((cp->cas_flags & CAS_FLAG_REG_PLUS) == 0) {
			/* should check to see if we're in a forced mode */
			stat = readl(cp->regs + REG_PCS_SERDES_STATE);
			if (stat == 0x03)
				return 1;
		}
	} else if (cp->lstate == link_down) {
		if (link_transition_timeout != 0 &&
		    cp->link_transition != LINK_TRANSITION_REQUESTED_RESET &&
		    !cp->link_transition_jiffies_valid) {
			/* force a reset, as a workaround for the
			 * link-failure problem.  May want to move
			 * this to a point a bit earlier in the
			 * sequence.
			 */
			retval = 1;
			cp->link_transition = LINK_TRANSITION_REQUESTED_RESET;
			cp->link_transition_jiffies = jiffies;
			cp->link_transition_jiffies_valid = 1;
		} else {
			cp->link_transition = LINK_TRANSITION_STILL_FAILED;
		}
	}

	return retval;
}

static int cas_pcs_interrupt(struct net_device *dev,
			     struct cas *cp, u32 status)
{
	u32 stat = readl(cp->regs + REG_PCS_INTR_STATUS);

	if ((stat & PCS_INTR_STATUS_LINK_CHANGE) == 0)
		return 0;
	return cas_pcs_link_check(cp);
}

static int cas_txmac_interrupt(struct net_device *dev,
			       struct cas *cp, u32 status)
{
	u32 txmac_stat = readl(cp->regs + REG_MAC_TX_STATUS);

	if (!txmac_stat)
		return 0;

	if (netif_msg_intr(cp))
		printk(KERN_DEBUG "%s: txmac interrupt, txmac_stat: 0x%x\n",
			cp->dev->name, txmac_stat);

	/* Defer timer expiration is quite normal,
	 * don't even log the event.
	 */
	if ((txmac_stat & MAC_TX_DEFER_TIMER) &&
	    !(txmac_stat & ~MAC_TX_DEFER_TIMER))
		return 0;

	spin_lock(&cp->stat_lock[0]);
	if (txmac_stat & MAC_TX_UNDERRUN) {
		printk(KERN_ERR "%s: TX MAC xmit underrun.\n",
		       dev->name);
		cp->net_stats[0].tx_fifo_errors++;
	}

	if (txmac_stat & MAC_TX_MAX_PACKET_ERR) {
		printk(KERN_ERR "%s: TX MAC max packet size error.\n",
		       dev->name);
		cp->net_stats[0].tx_errors++;
	}

	/* The rest are all cases of one of the 16-bit TX
	 * counters expiring.
	 */
	if (txmac_stat & MAC_TX_COLL_NORMAL)
		cp->net_stats[0].collisions += 0x10000;

	if (txmac_stat & MAC_TX_COLL_EXCESS) {
		cp->net_stats[0].tx_aborted_errors += 0x10000;
		cp->net_stats[0].collisions += 0x10000;
	}

	if (txmac_stat & MAC_TX_COLL_LATE) {
		cp->net_stats[0].tx_aborted_errors += 0x10000;
		cp->net_stats[0].collisions += 0x10000;
	}
	spin_unlock(&cp->stat_lock[0]);

	/* We do not keep track of MAC_TX_COLL_FIRST and
	 * MAC_TX_PEAK_ATTEMPTS events.
	 */
	return 0;
}

static void cas_load_firmware(struct cas *cp, cas_hp_inst_t *firmware)
{
	cas_hp_inst_t *inst;
	u32 val;
	int i;

	i = 0;
	while ((inst = firmware) && inst->note) {
		writel(i, cp->regs + REG_HP_INSTR_RAM_ADDR);

		val = CAS_BASE(HP_INSTR_RAM_HI_VAL, inst->val);
		val |= CAS_BASE(HP_INSTR_RAM_HI_MASK, inst->mask);
		writel(val, cp->regs + REG_HP_INSTR_RAM_DATA_HI);

		val = CAS_BASE(HP_INSTR_RAM_MID_OUTARG, inst->outarg >> 10);
		val |= CAS_BASE(HP_INSTR_RAM_MID_OUTOP, inst->outop);
		val |= CAS_BASE(HP_INSTR_RAM_MID_FNEXT, inst->fnext);
		val |= CAS_BASE(HP_INSTR_RAM_MID_FOFF, inst->foff);
		val |= CAS_BASE(HP_INSTR_RAM_MID_SNEXT, inst->snext);
		val |= CAS_BASE(HP_INSTR_RAM_MID_SOFF, inst->soff);
		val |= CAS_BASE(HP_INSTR_RAM_MID_OP, inst->op);
		writel(val, cp->regs + REG_HP_INSTR_RAM_DATA_MID);

		val = CAS_BASE(HP_INSTR_RAM_LOW_OUTMASK, inst->outmask);
		val |= CAS_BASE(HP_INSTR_RAM_LOW_OUTSHIFT, inst->outshift);
		val |= CAS_BASE(HP_INSTR_RAM_LOW_OUTEN, inst->outenab);
		val |= CAS_BASE(HP_INSTR_RAM_LOW_OUTARG, inst->outarg);
		writel(val, cp->regs + REG_HP_INSTR_RAM_DATA_LOW);
		++firmware;
		++i;
	}
}

static void cas_init_rx_dma(struct cas *cp)
{
	u64 desc_dma = cp->block_dvma;
	u32 val;
	int i, size;

	/* rx free descriptors */
	val = CAS_BASE(RX_CFG_SWIVEL, RX_SWIVEL_OFF_VAL);
	val |= CAS_BASE(RX_CFG_DESC_RING, RX_DESC_RINGN_INDEX(0));
	val |= CAS_BASE(RX_CFG_COMP_RING, RX_COMP_RINGN_INDEX(0));
	if ((N_RX_DESC_RINGS > 1) &&
	    (cp->cas_flags & CAS_FLAG_REG_PLUS))  /* do desc 2 */
		val |= CAS_BASE(RX_CFG_DESC_RING1, RX_DESC_RINGN_INDEX(1));
	writel(val, cp->regs + REG_RX_CFG);

	val = (unsigned long) cp->init_rxds[0] -
		(unsigned long) cp->init_block;
	writel((desc_dma + val) >> 32, cp->regs + REG_RX_DB_HI);
	writel((desc_dma + val) & 0xffffffff, cp->regs + REG_RX_DB_LOW);
	writel(RX_DESC_RINGN_SIZE(0) - 4, cp->regs + REG_RX_KICK);

	if (cp->cas_flags & CAS_FLAG_REG_PLUS) {
		/* rx desc 2 is for IPSEC packets. however,
		 * we don't it that for that purpose.
		 */
		val = (unsigned long) cp->init_rxds[1] -
			(unsigned long) cp->init_block;
		writel((desc_dma + val) >> 32, cp->regs + REG_PLUS_RX_DB1_HI);
		writel((desc_dma + val) & 0xffffffff, cp->regs +
		       REG_PLUS_RX_DB1_LOW);
		writel(RX_DESC_RINGN_SIZE(1) - 4, cp->regs +
		       REG_PLUS_RX_KICK1);
	}

	/* rx completion registers */
	val = (unsigned long) cp->init_rxcs[0] -
		(unsigned long) cp->init_block;
	writel((desc_dma + val) >> 32, cp->regs + REG_RX_CB_HI);
	writel((desc_dma + val) & 0xffffffff, cp->regs + REG_RX_CB_LOW);

	if (cp->cas_flags & CAS_FLAG_REG_PLUS) {
		/* rx comp 2-4 */
		for (i = 1; i < MAX_RX_COMP_RINGS; i++) {
			val = (unsigned long) cp->init_rxcs[i] -
				(unsigned long) cp->init_block;
			writel((desc_dma + val) >> 32, cp->regs +
			       REG_PLUS_RX_CBN_HI(i));
			writel((desc_dma + val) & 0xffffffff, cp->regs +
			       REG_PLUS_RX_CBN_LOW(i));
		}
	}

	/* read selective clear regs to prevent spurious interrupts
	 * on reset because complete == kick.
	 * selective clear set up to prevent interrupts on resets
	 */
	readl(cp->regs + REG_INTR_STATUS_ALIAS);
	writel(INTR_RX_DONE | INTR_RX_BUF_UNAVAIL, cp->regs + REG_ALIAS_CLEAR);
	if (cp->cas_flags & CAS_FLAG_REG_PLUS) {
		for (i = 1; i < N_RX_COMP_RINGS; i++)
			readl(cp->regs + REG_PLUS_INTRN_STATUS_ALIAS(i));

		/* 2 is different from 3 and 4 */
		if (N_RX_COMP_RINGS > 1)
			writel(INTR_RX_DONE_ALT | INTR_RX_BUF_UNAVAIL_1,
			       cp->regs + REG_PLUS_ALIASN_CLEAR(1));

		for (i = 2; i < N_RX_COMP_RINGS; i++)
			writel(INTR_RX_DONE_ALT,
			       cp->regs + REG_PLUS_ALIASN_CLEAR(i));
	}

	/* set up pause thresholds */
	val  = CAS_BASE(RX_PAUSE_THRESH_OFF,
			cp->rx_pause_off / RX_PAUSE_THRESH_QUANTUM);
	val |= CAS_BASE(RX_PAUSE_THRESH_ON,
			cp->rx_pause_on / RX_PAUSE_THRESH_QUANTUM);
	writel(val, cp->regs + REG_RX_PAUSE_THRESH);

	/* zero out dma reassembly buffers */
	for (i = 0; i < 64; i++) {
		writel(i, cp->regs + REG_RX_TABLE_ADDR);
		writel(0x0, cp->regs + REG_RX_TABLE_DATA_LOW);
		writel(0x0, cp->regs + REG_RX_TABLE_DATA_MID);
		writel(0x0, cp->regs + REG_RX_TABLE_DATA_HI);
	}

	/* make sure address register is 0 for normal operation */
	writel(0x0, cp->regs + REG_RX_CTRL_FIFO_ADDR);
	writel(0x0, cp->regs + REG_RX_IPP_FIFO_ADDR);

	/* interrupt mitigation */
#ifdef USE_RX_BLANK
	val = CAS_BASE(RX_BLANK_INTR_TIME, RX_BLANK_INTR_TIME_VAL);
	val |= CAS_BASE(RX_BLANK_INTR_PKT, RX_BLANK_INTR_PKT_VAL);
	writel(val, cp->regs + REG_RX_BLANK);
#else
	writel(0x0, cp->regs + REG_RX_BLANK);
#endif

	/* interrupt generation as a function of low water marks for
	 * free desc and completion entries. these are used to trigger
	 * housekeeping for rx descs. we don't use the free interrupt
	 * as it's not very useful
	 */
	/* val = CAS_BASE(RX_AE_THRESH_FREE, RX_AE_FREEN_VAL(0)); */
	val = CAS_BASE(RX_AE_THRESH_COMP, RX_AE_COMP_VAL);
	writel(val, cp->regs + REG_RX_AE_THRESH);
	if (cp->cas_flags & CAS_FLAG_REG_PLUS) {
		val = CAS_BASE(RX_AE1_THRESH_FREE, RX_AE_FREEN_VAL(1));
		writel(val, cp->regs + REG_PLUS_RX_AE1_THRESH);
	}

	/* Random early detect registers. useful for congestion avoidance.
	 * this should be tunable.
	 */
	writel(0x0, cp->regs + REG_RX_RED);

	/* receive page sizes. default == 2K (0x800) */
	val = 0;
	if (cp->page_size == 0x1000)
		val = 0x1;
	else if (cp->page_size == 0x2000)
		val = 0x2;
	else if (cp->page_size == 0x4000)
		val = 0x3;

	/* round mtu + offset. constrain to page size. */
	size = cp->dev->mtu + 64;
	if (size > cp->page_size)
		size = cp->page_size;

	if (size <= 0x400)
		i = 0x0;
	else if (size <= 0x800)
		i = 0x1;
	else if (size <= 0x1000)
		i = 0x2;
	else
		i = 0x3;

	cp->mtu_stride = 1 << (i + 10);
	val  = CAS_BASE(RX_PAGE_SIZE, val);
	val |= CAS_BASE(RX_PAGE_SIZE_MTU_STRIDE, i);
	val |= CAS_BASE(RX_PAGE_SIZE_MTU_COUNT, cp->page_size >> (i + 10));
	val |= CAS_BASE(RX_PAGE_SIZE_MTU_OFF, 0x1);
	writel(val, cp->regs + REG_RX_PAGE_SIZE);

	/* enable the header parser if desired */
	if (CAS_HP_FIRMWARE == cas_prog_null)
		return;

	val = CAS_BASE(HP_CFG_NUM_CPU, CAS_NCPUS > 63 ? 0 : CAS_NCPUS);
	val |= HP_CFG_PARSE_EN | HP_CFG_SYN_INC_MASK;
	val |= CAS_BASE(HP_CFG_TCP_THRESH, HP_TCP_THRESH_VAL);
	writel(val, cp->regs + REG_HP_CFG);
}

static inline void cas_rxc_init(struct cas_rx_comp *rxc)
{
	memset(rxc, 0, sizeof(*rxc));
	rxc->word4 = cpu_to_le64(RX_COMP4_ZERO);
}

/* NOTE: we use the ENC RX DESC ring for spares. the rx_page[0,1]
 * flipping is protected by the fact that the chip will not
 * hand back the same page index while it's being processed.
 */
static inline cas_page_t *cas_page_spare(struct cas *cp, const int index)
{
	cas_page_t *page = cp->rx_pages[1][index];
	cas_page_t *new;

	if (page_count(page->buffer) == 1)
		return page;

	new = cas_page_dequeue(cp);
	if (new) {
		spin_lock(&cp->rx_inuse_lock);
		list_add(&page->list, &cp->rx_inuse_list);
		spin_unlock(&cp->rx_inuse_lock);
	}
	return new;
}

/* this needs to be changed if we actually use the ENC RX DESC ring */
static cas_page_t *cas_page_swap(struct cas *cp, const int ring,
				 const int index)
{
	cas_page_t **page0 = cp->rx_pages[0];
	cas_page_t **page1 = cp->rx_pages[1];

	/* swap if buffer is in use */
	if (page_count(page0[index]->buffer) > 1) {
		cas_page_t *new = cas_page_spare(cp, index);
		if (new) {
			page1[index] = page0[index];
			page0[index] = new;
		}
	}
	RX_USED_SET(page0[index], 0);
	return page0[index];
}

static void cas_clean_rxds(struct cas *cp)
{
	/* only clean ring 0 as ring 1 is used for spare buffers */
        struct cas_rx_desc *rxd = cp->init_rxds[0];
	int i, size;

	/* release all rx flows */
	for (i = 0; i < N_RX_FLOWS; i++) {
		struct sk_buff *skb;
		while ((skb = __skb_dequeue(&cp->rx_flows[i]))) {
			cas_skb_release(skb);
		}
	}

	/* initialize descriptors */
	size = RX_DESC_RINGN_SIZE(0);
	for (i = 0; i < size; i++) {
		cas_page_t *page = cas_page_swap(cp, 0, i);
		rxd[i].buffer = cpu_to_le64(page->dma_addr);
		rxd[i].index  = cpu_to_le64(CAS_BASE(RX_INDEX_NUM, i) |
					    CAS_BASE(RX_INDEX_RING, 0));
	}

	cp->rx_old[0]  = RX_DESC_RINGN_SIZE(0) - 4;
	cp->rx_last[0] = 0;
	cp->cas_flags &= ~CAS_FLAG_RXD_POST(0);
}

static void cas_clean_rxcs(struct cas *cp)
{
	int i, j;

	/* take ownership of rx comp descriptors */
	memset(cp->rx_cur, 0, sizeof(*cp->rx_cur)*N_RX_COMP_RINGS);
	memset(cp->rx_new, 0, sizeof(*cp->rx_new)*N_RX_COMP_RINGS);
	for (i = 0; i < N_RX_COMP_RINGS; i++) {
		struct cas_rx_comp *rxc = cp->init_rxcs[i];
		for (j = 0; j < RX_COMP_RINGN_SIZE(i); j++) {
			cas_rxc_init(rxc + j);
		}
	}
}

#if 0
/* When we get a RX fifo overflow, the RX unit is probably hung
 * so we do the following.
 *
 * If any part of the reset goes wrong, we return 1 and that causes the
 * whole chip to be reset.
 */
static int cas_rxmac_reset(struct cas *cp)
{
	struct net_device *dev = cp->dev;
	int limit;
	u32 val;

	/* First, reset MAC RX. */
	writel(cp->mac_rx_cfg & ~MAC_RX_CFG_EN, cp->regs + REG_MAC_RX_CFG);
	for (limit = 0; limit < STOP_TRIES; limit++) {
		if (!(readl(cp->regs + REG_MAC_RX_CFG) & MAC_RX_CFG_EN))
			break;
		udelay(10);
	}
	if (limit == STOP_TRIES) {
		printk(KERN_ERR "%s: RX MAC will not disable, resetting whole "
		       "chip.\n", dev->name);
		return 1;
	}

	/* Second, disable RX DMA. */
	writel(0, cp->regs + REG_RX_CFG);
	for (limit = 0; limit < STOP_TRIES; limit++) {
		if (!(readl(cp->regs + REG_RX_CFG) & RX_CFG_DMA_EN))
			break;
		udelay(10);
	}
	if (limit == STOP_TRIES) {
		printk(KERN_ERR "%s: RX DMA will not disable, resetting whole "
		       "chip.\n", dev->name);
		return 1;
	}

	mdelay(5);

	/* Execute RX reset command. */
	writel(SW_RESET_RX, cp->regs + REG_SW_RESET);
	for (limit = 0; limit < STOP_TRIES; limit++) {
		if (!(readl(cp->regs + REG_SW_RESET) & SW_RESET_RX))
			break;
		udelay(10);
	}
	if (limit == STOP_TRIES) {
		printk(KERN_ERR "%s: RX reset command will not execute, "
		       "resetting whole chip.\n", dev->name);
		return 1;
	}

	/* reset driver rx state */
	cas_clean_rxds(cp);
	cas_clean_rxcs(cp);

	/* Now, reprogram the rest of RX unit. */
	cas_init_rx_dma(cp);

	/* re-enable */
	val = readl(cp->regs + REG_RX_CFG);
	writel(val | RX_CFG_DMA_EN, cp->regs + REG_RX_CFG);
	writel(MAC_RX_FRAME_RECV, cp->regs + REG_MAC_RX_MASK);
	val = readl(cp->regs + REG_MAC_RX_CFG);
	writel(val | MAC_RX_CFG_EN, cp->regs + REG_MAC_RX_CFG);
	return 0;
}
#endif

static int cas_rxmac_interrupt(struct net_device *dev, struct cas *cp,
			       u32 status)
{
	u32 stat = readl(cp->regs + REG_MAC_RX_STATUS);

	if (!stat)
		return 0;

	if (netif_msg_intr(cp))
		printk(KERN_DEBUG "%s: rxmac interrupt, stat: 0x%x\n",
			cp->dev->name, stat);

	/* these are all rollovers */
	spin_lock(&cp->stat_lock[0]);
	if (stat & MAC_RX_ALIGN_ERR)
		cp->net_stats[0].rx_frame_errors += 0x10000;

	if (stat & MAC_RX_CRC_ERR)
		cp->net_stats[0].rx_crc_errors += 0x10000;

	if (stat & MAC_RX_LEN_ERR)
		cp->net_stats[0].rx_length_errors += 0x10000;

	if (stat & MAC_RX_OVERFLOW) {
		cp->net_stats[0].rx_over_errors++;
		cp->net_stats[0].rx_fifo_errors++;
	}

	/* We do not track MAC_RX_FRAME_COUNT and MAC_RX_VIOL_ERR
	 * events.
	 */
	spin_unlock(&cp->stat_lock[0]);
	return 0;
}

static int cas_mac_interrupt(struct net_device *dev, struct cas *cp,
			     u32 status)
{
	u32 stat = readl(cp->regs + REG_MAC_CTRL_STATUS);

	if (!stat)
		return 0;

	if (netif_msg_intr(cp))
		printk(KERN_DEBUG "%s: mac interrupt, stat: 0x%x\n",
			cp->dev->name, stat);

	/* This interrupt is just for pause frame and pause
	 * tracking.  It is useful for diagnostics and debug
	 * but probably by default we will mask these events.
	 */
	if (stat & MAC_CTRL_PAUSE_STATE)
		cp->pause_entered++;

	if (stat & MAC_CTRL_PAUSE_RECEIVED)
		cp->pause_last_time_recvd = (stat >> 16);

	return 0;
}


/* Must be invoked under cp->lock. */
static inline int cas_mdio_link_not_up(struct cas *cp)
{
	u16 val;

	switch (cp->lstate) {
	case link_force_ret:
		if (netif_msg_link(cp))
			printk(KERN_INFO "%s: Autoneg failed again, keeping"
				" forced mode\n", cp->dev->name);
		cas_phy_write(cp, MII_BMCR, cp->link_fcntl);
		cp->timer_ticks = 5;
		cp->lstate = link_force_ok;
		cp->link_transition = LINK_TRANSITION_LINK_CONFIG;
		break;

	case link_aneg:
		val = cas_phy_read(cp, MII_BMCR);

		/* Try forced modes. we try things in the following order:
		 * 1000 full -> 100 full/half -> 10 half
		 */
		val &= ~(BMCR_ANRESTART | BMCR_ANENABLE);
		val |= BMCR_FULLDPLX;
		val |= (cp->cas_flags & CAS_FLAG_1000MB_CAP) ?
			CAS_BMCR_SPEED1000 : BMCR_SPEED100;
		cas_phy_write(cp, MII_BMCR, val);
		cp->timer_ticks = 5;
		cp->lstate = link_force_try;
		cp->link_transition = LINK_TRANSITION_LINK_CONFIG;
		break;

	case link_force_try:
		/* Downgrade from 1000 to 100 to 10 Mbps if necessary. */
		val = cas_phy_read(cp, MII_BMCR);
		cp->timer_ticks = 5;
		if (val & CAS_BMCR_SPEED1000) { /* gigabit */
			val &= ~CAS_BMCR_SPEED1000;
			val |= (BMCR_SPEED100 | BMCR_FULLDPLX);
			cas_phy_write(cp, MII_BMCR, val);
			break;
		}

		if (val & BMCR_SPEED100) {
			if (val & BMCR_FULLDPLX) /* fd failed */
				val &= ~BMCR_FULLDPLX;
			else { /* 100Mbps failed */
				val &= ~BMCR_SPEED100;
			}
			cas_phy_write(cp, MII_BMCR, val);
			break;
		}
	default:
		break;
	}
	return 0;
}


/* must be invoked with cp->lock held */
static int cas_mii_link_check(struct cas *cp, const u16 bmsr)
{
	int restart;

	if (bmsr & BMSR_LSTATUS) {
		/* Ok, here we got a link. If we had it due to a forced
		 * fallback, and we were configured for autoneg, we
		 * retry a short autoneg pass. If you know your hub is
		 * broken, use ethtool ;)
		 */
		if ((cp->lstate == link_force_try) &&
		    (cp->link_cntl & BMCR_ANENABLE)) {
			cp->lstate = link_force_ret;
			cp->link_transition = LINK_TRANSITION_LINK_CONFIG;
			cas_mif_poll(cp, 0);
			cp->link_fcntl = cas_phy_read(cp, MII_BMCR);
			cp->timer_ticks = 5;
			if (cp->opened && netif_msg_link(cp))
				printk(KERN_INFO "%s: Got link after fallback, retrying"
				       " autoneg once...\n", cp->dev->name);
			cas_phy_write(cp, MII_BMCR,
				      cp->link_fcntl | BMCR_ANENABLE |
				      BMCR_ANRESTART);
			cas_mif_poll(cp, 1);

		} else if (cp->lstate != link_up) {
			cp->lstate = link_up;
			cp->link_transition = LINK_TRANSITION_LINK_UP;

			if (cp->opened) {
				cas_set_link_modes(cp);
				netif_carrier_on(cp->dev);
			}
		}
		return 0;
	}

	/* link not up. if the link was previously up, we restart the
	 * whole process
	 */
	restart = 0;
	if (cp->lstate == link_up) {
		cp->lstate = link_down;
		cp->link_transition = LINK_TRANSITION_LINK_DOWN;

		netif_carrier_off(cp->dev);
		if (cp->opened && netif_msg_link(cp))
			printk(KERN_INFO "%s: Link down\n",
			       cp->dev->name);
		restart = 1;

	} else if (++cp->timer_ticks > 10)
		cas_mdio_link_not_up(cp);

	return restart;
}

static int cas_mif_interrupt(struct net_device *dev, struct cas *cp,
			     u32 status)
{
	u32 stat = readl(cp->regs + REG_MIF_STATUS);
	u16 bmsr;

	/* check for a link change */
	if (CAS_VAL(MIF_STATUS_POLL_STATUS, stat) == 0)
		return 0;

	bmsr = CAS_VAL(MIF_STATUS_POLL_DATA, stat);
	return cas_mii_link_check(cp, bmsr);
}

static int cas_pci_interrupt(struct net_device *dev, struct cas *cp,
			     u32 status)
{
	u32 stat = readl(cp->regs + REG_PCI_ERR_STATUS);

	if (!stat)
		return 0;

	printk(KERN_ERR "%s: PCI error [%04x:%04x] ", dev->name, stat,
	       readl(cp->regs + REG_BIM_DIAG));

	/* cassini+ has this reserved */
	if ((stat & PCI_ERR_BADACK) &&
	    ((cp->cas_flags & CAS_FLAG_REG_PLUS) == 0))
		printk("<No ACK64# during ABS64 cycle> ");

	if (stat & PCI_ERR_DTRTO)
		printk("<Delayed transaction timeout> ");
	if (stat & PCI_ERR_OTHER)
		printk("<other> ");
	if (stat & PCI_ERR_BIM_DMA_WRITE)
		printk("<BIM DMA 0 write req> ");
	if (stat & PCI_ERR_BIM_DMA_READ)
		printk("<BIM DMA 0 read req> ");
	printk("\n");

	if (stat & PCI_ERR_OTHER) {
		u16 cfg;

		/* Interrogate PCI config space for the
		 * true cause.
		 */
		pci_read_config_word(cp->pdev, PCI_STATUS, &cfg);
		printk(KERN_ERR "%s: Read PCI cfg space status [%04x]\n",
		       dev->name, cfg);
		if (cfg & PCI_STATUS_PARITY)
			printk(KERN_ERR "%s: PCI parity error detected.\n",
			       dev->name);
		if (cfg & PCI_STATUS_SIG_TARGET_ABORT)
			printk(KERN_ERR "%s: PCI target abort.\n",
			       dev->name);
		if (cfg & PCI_STATUS_REC_TARGET_ABORT)
			printk(KERN_ERR "%s: PCI master acks target abort.\n",
			       dev->name);
		if (cfg & PCI_STATUS_REC_MASTER_ABORT)
			printk(KERN_ERR "%s: PCI master abort.\n", dev->name);
		if (cfg & PCI_STATUS_SIG_SYSTEM_ERROR)
			printk(KERN_ERR "%s: PCI system error SERR#.\n",
			       dev->name);
		if (cfg & PCI_STATUS_DETECTED_PARITY)
			printk(KERN_ERR "%s: PCI parity error.\n",
			       dev->name);

		/* Write the error bits back to clear them. */
		cfg &= (PCI_STATUS_PARITY |
			PCI_STATUS_SIG_TARGET_ABORT |
			PCI_STATUS_REC_TARGET_ABORT |
			PCI_STATUS_REC_MASTER_ABORT |
			PCI_STATUS_SIG_SYSTEM_ERROR |
			PCI_STATUS_DETECTED_PARITY);
		pci_write_config_word(cp->pdev, PCI_STATUS, cfg);
	}

	/* For all PCI errors, we should reset the chip. */
	return 1;
}

/* All non-normal interrupt conditions get serviced here.
 * Returns non-zero if we should just exit the interrupt
 * handler right now (ie. if we reset the card which invalidates
 * all of the other original irq status bits).
 */
static int cas_abnormal_irq(struct net_device *dev, struct cas *cp,
			    u32 status)
{
	if (status & INTR_RX_TAG_ERROR) {
		/* corrupt RX tag framing */
		if (netif_msg_rx_err(cp))
			printk(KERN_DEBUG "%s: corrupt rx tag framing\n",
				cp->dev->name);
		spin_lock(&cp->stat_lock[0]);
		cp->net_stats[0].rx_errors++;
		spin_unlock(&cp->stat_lock[0]);
		goto do_reset;
	}

	if (status & INTR_RX_LEN_MISMATCH) {
		/* length mismatch. */
		if (netif_msg_rx_err(cp))
			printk(KERN_DEBUG "%s: length mismatch for rx frame\n",
				cp->dev->name);
		spin_lock(&cp->stat_lock[0]);
		cp->net_stats[0].rx_errors++;
		spin_unlock(&cp->stat_lock[0]);
		goto do_reset;
	}

	if (status & INTR_PCS_STATUS) {
		if (cas_pcs_interrupt(dev, cp, status))
			goto do_reset;
	}

	if (status & INTR_TX_MAC_STATUS) {
		if (cas_txmac_interrupt(dev, cp, status))
			goto do_reset;
	}

	if (status & INTR_RX_MAC_STATUS) {
		if (cas_rxmac_interrupt(dev, cp, status))
			goto do_reset;
	}

	if (status & INTR_MAC_CTRL_STATUS) {
		if (cas_mac_interrupt(dev, cp, status))
			goto do_reset;
	}

	if (status & INTR_MIF_STATUS) {
		if (cas_mif_interrupt(dev, cp, status))
			goto do_reset;
	}

	if (status & INTR_PCI_ERROR_STATUS) {
		if (cas_pci_interrupt(dev, cp, status))
			goto do_reset;
	}
	return 0;

do_reset:
#if 1
	atomic_inc(&cp->reset_task_pending);
	atomic_inc(&cp->reset_task_pending_all);
	printk(KERN_ERR "%s:reset called in cas_abnormal_irq [0x%x]\n",
	       dev->name, status);
	schedule_work(&cp->reset_task);
#else
	atomic_set(&cp->reset_task_pending, CAS_RESET_ALL);
	printk(KERN_ERR "reset called in cas_abnormal_irq\n");
	schedule_work(&cp->reset_task);
#endif
	return 1;
}

/* NOTE: CAS_TABORT returns 1 or 2 so that it can be used when
 *       determining whether to do a netif_stop/wakeup
 */
#define CAS_TABORT(x)      (((x)->cas_flags & CAS_FLAG_TARGET_ABORT) ? 2 : 1)
#define CAS_ROUND_PAGE(x)  (((x) + PAGE_SIZE - 1) & PAGE_MASK)
static inline int cas_calc_tabort(struct cas *cp, const unsigned long addr,
				  const int len)
{
	unsigned long off = addr + len;

	if (CAS_TABORT(cp) == 1)
		return 0;
	if ((CAS_ROUND_PAGE(off) - off) > TX_TARGET_ABORT_LEN)
		return 0;
	return TX_TARGET_ABORT_LEN;
}

static inline void cas_tx_ringN(struct cas *cp, int ring, int limit)
{
	struct cas_tx_desc *txds;
	struct sk_buff **skbs;
	struct net_device *dev = cp->dev;
	int entry, count;

	spin_lock(&cp->tx_lock[ring]);
	txds = cp->init_txds[ring];
	skbs = cp->tx_skbs[ring];
	entry = cp->tx_old[ring];

	count = TX_BUFF_COUNT(ring, entry, limit);
	while (entry != limit) {
		struct sk_buff *skb = skbs[entry];
		dma_addr_t daddr;
		u32 dlen;
		int frag;

		if (!skb) {
			/* this should never occur */
			entry = TX_DESC_NEXT(ring, entry);
			continue;
		}

		/* however, we might get only a partial skb release. */
		count -= skb_shinfo(skb)->nr_frags +
			+ cp->tx_tiny_use[ring][entry].nbufs + 1;
		if (count < 0)
			break;

		if (netif_msg_tx_done(cp))
			printk(KERN_DEBUG "%s: tx[%d] done, slot %d\n",
			       cp->dev->name, ring, entry);

		skbs[entry] = NULL;
		cp->tx_tiny_use[ring][entry].nbufs = 0;

		for (frag = 0; frag <= skb_shinfo(skb)->nr_frags; frag++) {
			struct cas_tx_desc *txd = txds + entry;

			daddr = le64_to_cpu(txd->buffer);
			dlen = CAS_VAL(TX_DESC_BUFLEN,
				       le64_to_cpu(txd->control));
			pci_unmap_page(cp->pdev, daddr, dlen,
				       PCI_DMA_TODEVICE);
			entry = TX_DESC_NEXT(ring, entry);

			/* tiny buffer may follow */
			if (cp->tx_tiny_use[ring][entry].used) {
				cp->tx_tiny_use[ring][entry].used = 0;
				entry = TX_DESC_NEXT(ring, entry);
			}
		}

		spin_lock(&cp->stat_lock[ring]);
		cp->net_stats[ring].tx_packets++;
		cp->net_stats[ring].tx_bytes += skb->len;
		spin_unlock(&cp->stat_lock[ring]);
		dev_kfree_skb_irq(skb);
	}
	cp->tx_old[ring] = entry;

	/* this is wrong for multiple tx rings. the net device needs
	 * multiple queues for this to do the right thing.  we wait
	 * for 2*packets to be available when using tiny buffers
	 */
	if (netif_queue_stopped(dev) &&
	    (TX_BUFFS_AVAIL(cp, ring) > CAS_TABORT(cp)*(MAX_SKB_FRAGS + 1)))
		netif_wake_queue(dev);
	spin_unlock(&cp->tx_lock[ring]);
}

static void cas_tx(struct net_device *dev, struct cas *cp,
		   u32 status)
{
        int limit, ring;
#ifdef USE_TX_COMPWB
	u64 compwb = le64_to_cpu(cp->init_block->tx_compwb);
#endif
	if (netif_msg_intr(cp))
		printk(KERN_DEBUG "%s: tx interrupt, status: 0x%x, %llx\n",
			cp->dev->name, status, (unsigned long long)compwb);
	/* process all the rings */
	for (ring = 0; ring < N_TX_RINGS; ring++) {
#ifdef USE_TX_COMPWB
		/* use the completion writeback registers */
		limit = (CAS_VAL(TX_COMPWB_MSB, compwb) << 8) |
			CAS_VAL(TX_COMPWB_LSB, compwb);
		compwb = TX_COMPWB_NEXT(compwb);
#else
		limit = readl(cp->regs + REG_TX_COMPN(ring));
#endif
		if (cp->tx_old[ring] != limit)
			cas_tx_ringN(cp, ring, limit);
	}
}


static int cas_rx_process_pkt(struct cas *cp, struct cas_rx_comp *rxc,
			      int entry, const u64 *words,
			      struct sk_buff **skbref)
{
	int dlen, hlen, len, i, alloclen;
	int off, swivel = RX_SWIVEL_OFF_VAL;
	struct cas_page *page;
	struct sk_buff *skb;
	void *addr, *crcaddr;
	__sum16 csum;
	char *p;

	hlen = CAS_VAL(RX_COMP2_HDR_SIZE, words[1]);
	dlen = CAS_VAL(RX_COMP1_DATA_SIZE, words[0]);
	len  = hlen + dlen;

	if (RX_COPY_ALWAYS || (words[2] & RX_COMP3_SMALL_PKT))
		alloclen = len;
	else
		alloclen = max(hlen, RX_COPY_MIN);

	skb = dev_alloc_skb(alloclen + swivel + cp->crc_size);
	if (skb == NULL)
		return -1;

	*skbref = skb;
	skb_reserve(skb, swivel);

	p = skb->data;
	addr = crcaddr = NULL;
	if (hlen) { /* always copy header pages */
		i = CAS_VAL(RX_COMP2_HDR_INDEX, words[1]);
		page = cp->rx_pages[CAS_VAL(RX_INDEX_RING, i)][CAS_VAL(RX_INDEX_NUM, i)];
		off = CAS_VAL(RX_COMP2_HDR_OFF, words[1]) * 0x100 +
			swivel;

		i = hlen;
		if (!dlen) /* attach FCS */
			i += cp->crc_size;
		pci_dma_sync_single_for_cpu(cp->pdev, page->dma_addr + off, i,
				    PCI_DMA_FROMDEVICE);
		addr = cas_page_map(page->buffer);
		memcpy(p, addr + off, i);
		pci_dma_sync_single_for_device(cp->pdev, page->dma_addr + off, i,
				    PCI_DMA_FROMDEVICE);
		cas_page_unmap(addr);
		RX_USED_ADD(page, 0x100);
		p += hlen;
		swivel = 0;
	}


	if (alloclen < (hlen + dlen)) {
		skb_frag_t *frag = skb_shinfo(skb)->frags;

		/* normal or jumbo packets. we use frags */
		i = CAS_VAL(RX_COMP1_DATA_INDEX, words[0]);
		page = cp->rx_pages[CAS_VAL(RX_INDEX_RING, i)][CAS_VAL(RX_INDEX_NUM, i)];
		off = CAS_VAL(RX_COMP1_DATA_OFF, words[0]) + swivel;

		hlen = min(cp->page_size - off, dlen);
		if (hlen < 0) {
			if (netif_msg_rx_err(cp)) {
				printk(KERN_DEBUG "%s: rx page overflow: "
				       "%d\n", cp->dev->name, hlen);
			}
			dev_kfree_skb_irq(skb);
			return -1;
		}
		i = hlen;
		if (i == dlen)  /* attach FCS */
			i += cp->crc_size;
		pci_dma_sync_single_for_cpu(cp->pdev, page->dma_addr + off, i,
				    PCI_DMA_FROMDEVICE);

		/* make sure we always copy a header */
		swivel = 0;
		if (p == (char *) skb->data) { /* not split */
			addr = cas_page_map(page->buffer);
			memcpy(p, addr + off, RX_COPY_MIN);
			pci_dma_sync_single_for_device(cp->pdev, page->dma_addr + off, i,
					PCI_DMA_FROMDEVICE);
			cas_page_unmap(addr);
			off += RX_COPY_MIN;
			swivel = RX_COPY_MIN;
			RX_USED_ADD(page, cp->mtu_stride);
		} else {
			RX_USED_ADD(page, hlen);
		}
		skb_put(skb, alloclen);

		skb_shinfo(skb)->nr_frags++;
		skb->data_len += hlen - swivel;
		skb->truesize += hlen - swivel;
		skb->len      += hlen - swivel;

		get_page(page->buffer);
		frag->page = page->buffer;
		frag->page_offset = off;
		frag->size = hlen - swivel;

		/* any more data? */
		if ((words[0] & RX_COMP1_SPLIT_PKT) && ((dlen -= hlen) > 0)) {
			hlen = dlen;
			off = 0;

			i = CAS_VAL(RX_COMP2_NEXT_INDEX, words[1]);
			page = cp->rx_pages[CAS_VAL(RX_INDEX_RING, i)][CAS_VAL(RX_INDEX_NUM, i)];
			pci_dma_sync_single_for_cpu(cp->pdev, page->dma_addr,
					    hlen + cp->crc_size,
					    PCI_DMA_FROMDEVICE);
			pci_dma_sync_single_for_device(cp->pdev, page->dma_addr,
					    hlen + cp->crc_size,
					    PCI_DMA_FROMDEVICE);

			skb_shinfo(skb)->nr_frags++;
			skb->data_len += hlen;
			skb->len      += hlen;
			frag++;

			get_page(page->buffer);
			frag->page = page->buffer;
			frag->page_offset = 0;
			frag->size = hlen;
			RX_USED_ADD(page, hlen + cp->crc_size);
		}

		if (cp->crc_size) {
			addr = cas_page_map(page->buffer);
			crcaddr  = addr + off + hlen;
		}

	} else {
		/* copying packet */
		if (!dlen)
			goto end_copy_pkt;

		i = CAS_VAL(RX_COMP1_DATA_INDEX, words[0]);
		page = cp->rx_pages[CAS_VAL(RX_INDEX_RING, i)][CAS_VAL(RX_INDEX_NUM, i)];
		off = CAS_VAL(RX_COMP1_DATA_OFF, words[0]) + swivel;
		hlen = min(cp->page_size - off, dlen);
		if (hlen < 0) {
			if (netif_msg_rx_err(cp)) {
				printk(KERN_DEBUG "%s: rx page overflow: "
				       "%d\n", cp->dev->name, hlen);
			}
			dev_kfree_skb_irq(skb);
			return -1;
		}
		i = hlen;
		if (i == dlen) /* attach FCS */
			i += cp->crc_size;
		pci_dma_sync_single_for_cpu(cp->pdev, page->dma_addr + off, i,
				    PCI_DMA_FROMDEVICE);
		addr = cas_page_map(page->buffer);
		memcpy(p, addr + off, i);
		pci_dma_sync_single_for_device(cp->pdev, page->dma_addr + off, i,
				    PCI_DMA_FROMDEVICE);
		cas_page_unmap(addr);
		if (p == (char *) skb->data) /* not split */
			RX_USED_ADD(page, cp->mtu_stride);
		else
			RX_USED_ADD(page, i);

		/* any more data? */
		if ((words[0] & RX_COMP1_SPLIT_PKT) && ((dlen -= hlen) > 0)) {
			p += hlen;
			i = CAS_VAL(RX_COMP2_NEXT_INDEX, words[1]);
			page = cp->rx_pages[CAS_VAL(RX_INDEX_RING, i)][CAS_VAL(RX_INDEX_NUM, i)];
			pci_dma_sync_single_for_cpu(cp->pdev, page->dma_addr,
					    dlen + cp->crc_size,
					    PCI_DMA_FROMDEVICE);
			addr = cas_page_map(page->buffer);
			memcpy(p, addr, dlen + cp->crc_size);
			pci_dma_sync_single_for_device(cp->pdev, page->dma_addr,
					    dlen + cp->crc_size,
					    PCI_DMA_FROMDEVICE);
			cas_page_unmap(addr);
			RX_USED_ADD(page, dlen + cp->crc_size);
		}
end_copy_pkt:
		if (cp->crc_size) {
			addr    = NULL;
			crcaddr = skb->data + alloclen;
		}
		skb_put(skb, alloclen);
	}

	csum = (__force __sum16)htons(CAS_VAL(RX_COMP4_TCP_CSUM, words[3]));
	if (cp->crc_size) {
		/* checksum includes FCS. strip it out. */
		csum = csum_fold(csum_partial(crcaddr, cp->crc_size,
					      csum_unfold(csum)));
		if (addr)
			cas_page_unmap(addr);
	}
	skb->protocol = eth_type_trans(skb, cp->dev);
	if (skb->protocol == htons(ETH_P_IP)) {
		skb->csum = csum_unfold(~csum);
		skb->ip_summed = CHECKSUM_COMPLETE;
	} else
		skb->ip_summed = CHECKSUM_NONE;
	return len;
}


/* we can handle up to 64 rx flows at a time. we do the same thing
 * as nonreassm except that we batch up the buffers.
 * NOTE: we currently just treat each flow as a bunch of packets that
 *       we pass up. a better way would be to coalesce the packets
 *       into a jumbo packet. to do that, we need to do the following:
 *       1) the first packet will have a clean split between header and
 *          data. save both.
 *       2) each time the next flow packet comes in, extend the
 *          data length and merge the checksums.
 *       3) on flow release, fix up the header.
 *       4) make sure the higher layer doesn't care.
 * because packets get coalesced, we shouldn't run into fragment count
 * issues.
 */
static inline void cas_rx_flow_pkt(struct cas *cp, const u64 *words,
				   struct sk_buff *skb)
{
	int flowid = CAS_VAL(RX_COMP3_FLOWID, words[2]) & (N_RX_FLOWS - 1);
	struct sk_buff_head *flow = &cp->rx_flows[flowid];

	/* this is protected at a higher layer, so no need to
	 * do any additional locking here. stick the buffer
	 * at the end.
	 */
	__skb_queue_tail(flow, skb);
	if (words[0] & RX_COMP1_RELEASE_FLOW) {
		while ((skb = __skb_dequeue(flow))) {
			cas_skb_release(skb);
		}
	}
}

/* put rx descriptor back on ring. if a buffer is in use by a higher
 * layer, this will need to put in a replacement.
 */
static void cas_post_page(struct cas *cp, const int ring, const int index)
{
	cas_page_t *new;
	int entry;

	entry = cp->rx_old[ring];

	new = cas_page_swap(cp, ring, index);
	cp->init_rxds[ring][entry].buffer = cpu_to_le64(new->dma_addr);
	cp->init_rxds[ring][entry].index  =
		cpu_to_le64(CAS_BASE(RX_INDEX_NUM, index) |
			    CAS_BASE(RX_INDEX_RING, ring));

	entry = RX_DESC_ENTRY(ring, entry + 1);
	cp->rx_old[ring] = entry;

	if (entry % 4)
		return;

	if (ring == 0)
		writel(entry, cp->regs + REG_RX_KICK);
	else if ((N_RX_DESC_RINGS > 1) &&
		 (cp->cas_flags & CAS_FLAG_REG_PLUS))
		writel(entry, cp->regs + REG_PLUS_RX_KICK1);
}


/* only when things are bad */
static int cas_post_rxds_ringN(struct cas *cp, int ring, int num)
{
	unsigned int entry, last, count, released;
	int cluster;
	cas_page_t **page = cp->rx_pages[ring];

	entry = cp->rx_old[ring];

	if (netif_msg_intr(cp))
		printk(KERN_DEBUG "%s: rxd[%d] interrupt, done: %d\n",
		       cp->dev->name, ring, entry);

	cluster = -1;
	count = entry & 0x3;
	last = RX_DESC_ENTRY(ring, num ? entry + num - 4: entry - 4);
	released = 0;
	while (entry != last) {
		/* make a new buffer if it's still in use */
		if (page_count(page[entry]->buffer) > 1) {
			cas_page_t *new = cas_page_dequeue(cp);
			if (!new) {
				/* let the timer know that we need to
				 * do this again
				 */
				cp->cas_flags |= CAS_FLAG_RXD_POST(ring);
				if (!timer_pending(&cp->link_timer))
					mod_timer(&cp->link_timer, jiffies +
						  CAS_LINK_FAST_TIMEOUT);
				cp->rx_old[ring]  = entry;
				cp->rx_last[ring] = num ? num - released : 0;
				return -ENOMEM;
			}
			spin_lock(&cp->rx_inuse_lock);
			list_add(&page[entry]->list, &cp->rx_inuse_list);
			spin_unlock(&cp->rx_inuse_lock);
			cp->init_rxds[ring][entry].buffer =
				cpu_to_le64(new->dma_addr);
			page[entry] = new;

		}

		if (++count == 4) {
			cluster = entry;
			count = 0;
		}
		released++;
		entry = RX_DESC_ENTRY(ring, entry + 1);
	}
	cp->rx_old[ring] = entry;

	if (cluster < 0)
		return 0;

	if (ring == 0)
		writel(cluster, cp->regs + REG_RX_KICK);
	else if ((N_RX_DESC_RINGS > 1) &&
		 (cp->cas_flags & CAS_FLAG_REG_PLUS))
		writel(cluster, cp->regs + REG_PLUS_RX_KICK1);
	return 0;
}


/* process a completion ring. packets are set up in three basic ways:
 * small packets: should be copied header + data in single buffer.
 * large packets: header and data in a single buffer.
 * split packets: header in a separate buffer from data.
 *                data may be in multiple pages. data may be > 256
 *                bytes but in a single page.
 *
 * NOTE: RX page posting is done in this routine as well. while there's
 *       the capability of using multiple RX completion rings, it isn't
 *       really worthwhile due to the fact that the page posting will
 *       force serialization on the single descriptor ring.
 */
static int cas_rx_ringN(struct cas *cp, int ring, int budget)
{
	struct cas_rx_comp *rxcs = cp->init_rxcs[ring];
	int entry, drops;
	int npackets = 0;

	if (netif_msg_intr(cp))
		printk(KERN_DEBUG "%s: rx[%d] interrupt, done: %d/%d\n",
		       cp->dev->name, ring,
		       readl(cp->regs + REG_RX_COMP_HEAD),
		       cp->rx_new[ring]);

	entry = cp->rx_new[ring];
	drops = 0;
	while (1) {
		struct cas_rx_comp *rxc = rxcs + entry;
		struct sk_buff *uninitialized_var(skb);
		int type, len;
		u64 words[4];
		int i, dring;

		words[0] = le64_to_cpu(rxc->word1);
		words[1] = le64_to_cpu(rxc->word2);
		words[2] = le64_to_cpu(rxc->word3);
		words[3] = le64_to_cpu(rxc->word4);

		/* don't touch if still owned by hw */
		type = CAS_VAL(RX_COMP1_TYPE, words[0]);
		if (type == 0)
			break;

		/* hw hasn't cleared the zero bit yet */
		if (words[3] & RX_COMP4_ZERO) {
			break;
		}

		/* get info on the packet */
		if (words[3] & (RX_COMP4_LEN_MISMATCH | RX_COMP4_BAD)) {
			spin_lock(&cp->stat_lock[ring]);
			cp->net_stats[ring].rx_errors++;
			if (words[3] & RX_COMP4_LEN_MISMATCH)
				cp->net_stats[ring].rx_length_errors++;
			if (words[3] & RX_COMP4_BAD)
				cp->net_stats[ring].rx_crc_errors++;
			spin_unlock(&cp->stat_lock[ring]);

			/* We'll just return it to Cassini. */
		drop_it:
			spin_lock(&cp->stat_lock[ring]);
			++cp->net_stats[ring].rx_dropped;
			spin_unlock(&cp->stat_lock[ring]);
			goto next;
		}

		len = cas_rx_process_pkt(cp, rxc, entry, words, &skb);
		if (len < 0) {
			++drops;
			goto drop_it;
		}

		/* see if it's a flow re-assembly or not. the driver
		 * itself handles release back up.
		 */
		if (RX_DONT_BATCH || (type == 0x2)) {
			/* non-reassm: these always get released */
			cas_skb_release(skb);
		} else {
			cas_rx_flow_pkt(cp, words, skb);
		}

		spin_lock(&cp->stat_lock[ring]);
		cp->net_stats[ring].rx_packets++;
		cp->net_stats[ring].rx_bytes += len;
		spin_unlock(&cp->stat_lock[ring]);

	next:
		npackets++;

		/* should it be released? */
		if (words[0] & RX_COMP1_RELEASE_HDR) {
			i = CAS_VAL(RX_COMP2_HDR_INDEX, words[1]);
			dring = CAS_VAL(RX_INDEX_RING, i);
			i = CAS_VAL(RX_INDEX_NUM, i);
			cas_post_page(cp, dring, i);
		}

		if (words[0] & RX_COMP1_RELEASE_DATA) {
			i = CAS_VAL(RX_COMP1_DATA_INDEX, words[0]);
			dring = CAS_VAL(RX_INDEX_RING, i);
			i = CAS_VAL(RX_INDEX_NUM, i);
			cas_post_page(cp, dring, i);
		}

		if (words[0] & RX_COMP1_RELEASE_NEXT) {
			i = CAS_VAL(RX_COMP2_NEXT_INDEX, words[1]);
			dring = CAS_VAL(RX_INDEX_RING, i);
			i = CAS_VAL(RX_INDEX_NUM, i);
			cas_post_page(cp, dring, i);
		}

		/* skip to the next entry */
		entry = RX_COMP_ENTRY(ring, entry + 1 +
				      CAS_VAL(RX_COMP1_SKIP, words[0]));
#ifdef USE_NAPI
		if (budget && (npackets >= budget))
			break;
#endif
	}
	cp->rx_new[ring] = entry;

	if (drops)
		printk(KERN_INFO "%s: Memory squeeze, deferring packet.\n",
		       cp->dev->name);
	return npackets;
}


/* put completion entries back on the ring */
static void cas_post_rxcs_ringN(struct net_device *dev,
				struct cas *cp, int ring)
{
	struct cas_rx_comp *rxc = cp->init_rxcs[ring];
	int last, entry;

	last = cp->rx_cur[ring];
	entry = cp->rx_new[ring];
	if (netif_msg_intr(cp))
		printk(KERN_DEBUG "%s: rxc[%d] interrupt, done: %d/%d\n",
		       dev->name, ring, readl(cp->regs + REG_RX_COMP_HEAD),
		       entry);

	/* zero and re-mark descriptors */
	while (last != entry) {
		cas_rxc_init(rxc + last);
		last = RX_COMP_ENTRY(ring, last + 1);
	}
	cp->rx_cur[ring] = last;

	if (ring == 0)
		writel(last, cp->regs + REG_RX_COMP_TAIL);
	else if (cp->cas_flags & CAS_FLAG_REG_PLUS)
		writel(last, cp->regs + REG_PLUS_RX_COMPN_TAIL(ring));
}



/* cassini can use all four PCI interrupts for the completion ring.
 * rings 3 and 4 are identical
 */
#if defined(USE_PCI_INTC) || defined(USE_PCI_INTD)
static inline void cas_handle_irqN(struct net_device *dev,
				   struct cas *cp, const u32 status,
				   const int ring)
{
	if (status & (INTR_RX_COMP_FULL_ALT | INTR_RX_COMP_AF_ALT))
		cas_post_rxcs_ringN(dev, cp, ring);
}

static irqreturn_t cas_interruptN(int irq, void *dev_id)
{
	struct net_device *dev = dev_id;
	struct cas *cp = netdev_priv(dev);
	unsigned long flags;
	int ring;
	u32 status = readl(cp->regs + REG_PLUS_INTRN_STATUS(ring));

	/* check for shared irq */
	if (status == 0)
		return IRQ_NONE;

	ring = (irq == cp->pci_irq_INTC) ? 2 : 3;
	spin_lock_irqsave(&cp->lock, flags);
	if (status & INTR_RX_DONE_ALT) { /* handle rx separately */
#ifdef USE_NAPI
		cas_mask_intr(cp);
		napi_schedule(&cp->napi);
#else
		cas_rx_ringN(cp, ring, 0);
#endif
		status &= ~INTR_RX_DONE_ALT;
	}

	if (status)
		cas_handle_irqN(dev, cp, status, ring);
	spin_unlock_irqrestore(&cp->lock, flags);
	return IRQ_HANDLED;
}
#endif

#ifdef USE_PCI_INTB
/* everything but rx packets */
static inline void cas_handle_irq1(struct cas *cp, const u32 status)
{
	if (status & INTR_RX_BUF_UNAVAIL_1) {
		/* Frame arrived, no free RX buffers available.
		 * NOTE: we can get this on a link transition. */
		cas_post_rxds_ringN(cp, 1, 0);
		spin_lock(&cp->stat_lock[1]);
		cp->net_stats[1].rx_dropped++;
		spin_unlock(&cp->stat_lock[1]);
	}

	if (status & INTR_RX_BUF_AE_1)
		cas_post_rxds_ringN(cp, 1, RX_DESC_RINGN_SIZE(1) -
				    RX_AE_FREEN_VAL(1));

	if (status & (INTR_RX_COMP_AF | INTR_RX_COMP_FULL))
		cas_post_rxcs_ringN(cp, 1);
}

/* ring 2 handles a few more events than 3 and 4 */
static irqreturn_t cas_interrupt1(int irq, void *dev_id)
{
	struct net_device *dev = dev_id;
	struct cas *cp = netdev_priv(dev);
	unsigned long flags;
	u32 status = readl(cp->regs + REG_PLUS_INTRN_STATUS(1));

	/* check for shared interrupt */
	if (status == 0)
		return IRQ_NONE;

	spin_lock_irqsave(&cp->lock, flags);
	if (status & INTR_RX_DONE_ALT) { /* handle rx separately */
#ifdef USE_NAPI
		cas_mask_intr(cp);
		napi_schedule(&cp->napi);
#else
		cas_rx_ringN(cp, 1, 0);
#endif
		status &= ~INTR_RX_DONE_ALT;
	}
	if (status)
		cas_handle_irq1(cp, status);
	spin_unlock_irqrestore(&cp->lock, flags);
	return IRQ_HANDLED;
}
#endif

static inline void cas_handle_irq(struct net_device *dev,
				  struct cas *cp, const u32 status)
{
	/* housekeeping interrupts */
	if (status & INTR_ERROR_MASK)
		cas_abnormal_irq(dev, cp, status);

	if (status & INTR_RX_BUF_UNAVAIL) {
		/* Frame arrived, no free RX buffers available.
		 * NOTE: we can get this on a link transition.
		 */
		cas_post_rxds_ringN(cp, 0, 0);
		spin_lock(&cp->stat_lock[0]);
		cp->net_stats[0].rx_dropped++;
		spin_unlock(&cp->stat_lock[0]);
	} else if (status & INTR_RX_BUF_AE) {
		cas_post_rxds_ringN(cp, 0, RX_DESC_RINGN_SIZE(0) -
				    RX_AE_FREEN_VAL(0));
	}

	if (status & (INTR_RX_COMP_AF | INTR_RX_COMP_FULL))
		cas_post_rxcs_ringN(dev, cp, 0);
}

static irqreturn_t cas_interrupt(int irq, void *dev_id)
{
	struct net_device *dev = dev_id;
	struct cas *cp = netdev_priv(dev);
	unsigned long flags;
	u32 status = readl(cp->regs + REG_INTR_STATUS);

	if (status == 0)
		return IRQ_NONE;

	spin_lock_irqsave(&cp->lock, flags);
	if (status & (INTR_TX_ALL | INTR_TX_INTME)) {
		cas_tx(dev, cp, status);
		status &= ~(INTR_TX_ALL | INTR_TX_INTME);
	}

	if (status & INTR_RX_DONE) {
#ifdef USE_NAPI
		cas_mask_intr(cp);
		napi_schedule(&cp->napi);
#else
		cas_rx_ringN(cp, 0, 0);
#endif
		status &= ~INTR_RX_DONE;
	}

	if (status)
		cas_handle_irq(dev, cp, status);
	spin_unlock_irqrestore(&cp->lock, flags);
	return IRQ_HANDLED;
}


#ifdef USE_NAPI
static int cas_poll(struct napi_struct *napi, int budget)
{
	struct cas *cp = container_of(napi, struct cas, napi);
	struct net_device *dev = cp->dev;
	int i, enable_intr, credits;
	u32 status = readl(cp->regs + REG_INTR_STATUS);
	unsigned long flags;

	spin_lock_irqsave(&cp->lock, flags);
	cas_tx(dev, cp, status);
	spin_unlock_irqrestore(&cp->lock, flags);

	/* NAPI rx packets. we spread the credits across all of the
	 * rxc rings
	 *
	 * to make sure we're fair with the work we loop through each
	 * ring N_RX_COMP_RING times with a request of
	 * budget / N_RX_COMP_RINGS
	 */
	enable_intr = 1;
	credits = 0;
	for (i = 0; i < N_RX_COMP_RINGS; i++) {
		int j;
		for (j = 0; j < N_RX_COMP_RINGS; j++) {
			credits += cas_rx_ringN(cp, j, budget / N_RX_COMP_RINGS);
			if (credits >= budget) {
				enable_intr = 0;
				goto rx_comp;
			}
		}
	}

rx_comp:
	/* final rx completion */
	spin_lock_irqsave(&cp->lock, flags);
	if (status)
		cas_handle_irq(dev, cp, status);

#ifdef USE_PCI_INTB
	if (N_RX_COMP_RINGS > 1) {
		status = readl(cp->regs + REG_PLUS_INTRN_STATUS(1));
		if (status)
			cas_handle_irq1(dev, cp, status);
	}
#endif

#ifdef USE_PCI_INTC
	if (N_RX_COMP_RINGS > 2) {
		status = readl(cp->regs + REG_PLUS_INTRN_STATUS(2));
		if (status)
			cas_handle_irqN(dev, cp, status, 2);
	}
#endif

#ifdef USE_PCI_INTD
	if (N_RX_COMP_RINGS > 3) {
		status = readl(cp->regs + REG_PLUS_INTRN_STATUS(3));
		if (status)
			cas_handle_irqN(dev, cp, status, 3);
	}
#endif
	spin_unlock_irqrestore(&cp->lock, flags);
	if (enable_intr) {
		napi_complete(napi);
		cas_unmask_intr(cp);
	}
	return credits;
}
#endif

#ifdef CONFIG_NET_POLL_CONTROLLER
static void cas_netpoll(struct net_device *dev)
{
	struct cas *cp = netdev_priv(dev);

	cas_disable_irq(cp, 0);
	cas_interrupt(cp->pdev->irq, dev);
	cas_enable_irq(cp, 0);

#ifdef USE_PCI_INTB
	if (N_RX_COMP_RINGS > 1) {
		/* cas_interrupt1(); */
	}
#endif
#ifdef USE_PCI_INTC
	if (N_RX_COMP_RINGS > 2) {
		/* cas_interruptN(); */
	}
#endif
#ifdef USE_PCI_INTD
	if (N_RX_COMP_RINGS > 3) {
		/* cas_interruptN(); */
	}
#endif
}
#endif

static void cas_tx_timeout(struct net_device *dev)
{
	struct cas *cp = netdev_priv(dev);

	printk(KERN_ERR "%s: transmit timed out, resetting\n", dev->name);
	if (!cp->hw_running) {
		printk("%s: hrm.. hw not running!\n", dev->name);
		return;
	}

	printk(KERN_ERR "%s: MIF_STATE[%08x]\n",
	       dev->name, readl(cp->regs + REG_MIF_STATE_MACHINE));

	printk(KERN_ERR "%s: MAC_STATE[%08x]\n",
	       dev->name, readl(cp->regs + REG_MAC_STATE_MACHINE));

	printk(KERN_ERR "%s: TX_STATE[%08x:%08x:%08x] "
	       "FIFO[%08x:%08x:%08x] SM1[%08x] SM2[%08x]\n",
	       dev->name,
	       readl(cp->regs + REG_TX_CFG),
	       readl(cp->regs + REG_MAC_TX_STATUS),
	       readl(cp->regs + REG_MAC_TX_CFG),
	       readl(cp->regs + REG_TX_FIFO_PKT_CNT),
	       readl(cp->regs + REG_TX_FIFO_WRITE_PTR),
	       readl(cp->regs + REG_TX_FIFO_READ_PTR),
	       readl(cp->regs + REG_TX_SM_1),
	       readl(cp->regs + REG_TX_SM_2));

	printk(KERN_ERR "%s: RX_STATE[%08x:%08x:%08x]\n",
	       dev->name,
	       readl(cp->regs + REG_RX_CFG),
	       readl(cp->regs + REG_MAC_RX_STATUS),
	       readl(cp->regs + REG_MAC_RX_CFG));

	printk(KERN_ERR "%s: HP_STATE[%08x:%08x:%08x:%08x]\n",
	       dev->name,
	       readl(cp->regs + REG_HP_STATE_MACHINE),
	       readl(cp->regs + REG_HP_STATUS0),
	       readl(cp->regs + REG_HP_STATUS1),
	       readl(cp->regs + REG_HP_STATUS2));

#if 1
	atomic_inc(&cp->reset_task_pending);
	atomic_inc(&cp->reset_task_pending_all);
	schedule_work(&cp->reset_task);
#else
	atomic_set(&cp->reset_task_pending, CAS_RESET_ALL);
	schedule_work(&cp->reset_task);
#endif
}

static inline int cas_intme(int ring, int entry)
{
	/* Algorithm: IRQ every 1/2 of descriptors. */
	if (!(entry & ((TX_DESC_RINGN_SIZE(ring) >> 1) - 1)))
		return 1;
	return 0;
}


static void cas_write_txd(struct cas *cp, int ring, int entry,
			  dma_addr_t mapping, int len, u64 ctrl, int last)
{
	struct cas_tx_desc *txd = cp->init_txds[ring] + entry;

	ctrl |= CAS_BASE(TX_DESC_BUFLEN, len);
	if (cas_intme(ring, entry))
		ctrl |= TX_DESC_INTME;
	if (last)
		ctrl |= TX_DESC_EOF;
	txd->control = cpu_to_le64(ctrl);
	txd->buffer = cpu_to_le64(mapping);
}

static inline void *tx_tiny_buf(struct cas *cp, const int ring,
				const int entry)
{
	return cp->tx_tiny_bufs[ring] + TX_TINY_BUF_LEN*entry;
}

static inline dma_addr_t tx_tiny_map(struct cas *cp, const int ring,
				     const int entry, const int tentry)
{
	cp->tx_tiny_use[ring][tentry].nbufs++;
	cp->tx_tiny_use[ring][entry].used = 1;
	return cp->tx_tiny_dvma[ring] + TX_TINY_BUF_LEN*entry;
}

static inline int cas_xmit_tx_ringN(struct cas *cp, int ring,
				    struct sk_buff *skb)
{
	struct net_device *dev = cp->dev;
	int entry, nr_frags, frag, tabort, tentry;
	dma_addr_t mapping;
	unsigned long flags;
	u64 ctrl;
	u32 len;

	spin_lock_irqsave(&cp->tx_lock[ring], flags);

	/* This is a hard error, log it. */
	if (TX_BUFFS_AVAIL(cp, ring) <=
	    CAS_TABORT(cp)*(skb_shinfo(skb)->nr_frags + 1)) {
		netif_stop_queue(dev);
		spin_unlock_irqrestore(&cp->tx_lock[ring], flags);
		printk(KERN_ERR PFX "%s: BUG! Tx Ring full when "
		       "queue awake!\n", dev->name);
		return 1;
	}

	ctrl = 0;
	if (skb->ip_summed == CHECKSUM_PARTIAL) {
		const u64 csum_start_off = skb_transport_offset(skb);
		const u64 csum_stuff_off = csum_start_off + skb->csum_offset;

		ctrl =  TX_DESC_CSUM_EN |
			CAS_BASE(TX_DESC_CSUM_START, csum_start_off) |
			CAS_BASE(TX_DESC_CSUM_STUFF, csum_stuff_off);
	}

	entry = cp->tx_new[ring];
	cp->tx_skbs[ring][entry] = skb;

	nr_frags = skb_shinfo(skb)->nr_frags;
	len = skb_headlen(skb);
	mapping = pci_map_page(cp->pdev, virt_to_page(skb->data),
			       offset_in_page(skb->data), len,
			       PCI_DMA_TODEVICE);

	tentry = entry;
	tabort = cas_calc_tabort(cp, (unsigned long) skb->data, len);
	if (unlikely(tabort)) {
		/* NOTE: len is always >  tabort */
		cas_write_txd(cp, ring, entry, mapping, len - tabort,
			      ctrl | TX_DESC_SOF, 0);
		entry = TX_DESC_NEXT(ring, entry);

		skb_copy_from_linear_data_offset(skb, len - tabort,
			      tx_tiny_buf(cp, ring, entry), tabort);
		mapping = tx_tiny_map(cp, ring, entry, tentry);
		cas_write_txd(cp, ring, entry, mapping, tabort, ctrl,
			      (nr_frags == 0));
	} else {
		cas_write_txd(cp, ring, entry, mapping, len, ctrl |
			      TX_DESC_SOF, (nr_frags == 0));
	}
	entry = TX_DESC_NEXT(ring, entry);

	for (frag = 0; frag < nr_frags; frag++) {
		skb_frag_t *fragp = &skb_shinfo(skb)->frags[frag];

		len = fragp->size;
		mapping = pci_map_page(cp->pdev, fragp->page,
				       fragp->page_offset, len,
				       PCI_DMA_TODEVICE);

		tabort = cas_calc_tabort(cp, fragp->page_offset, len);
		if (unlikely(tabort)) {
			void *addr;

			/* NOTE: len is always > tabort */
			cas_write_txd(cp, ring, entry, mapping, len - tabort,
				      ctrl, 0);
			entry = TX_DESC_NEXT(ring, entry);

			addr = cas_page_map(fragp->page);
			memcpy(tx_tiny_buf(cp, ring, entry),
			       addr + fragp->page_offset + len - tabort,
			       tabort);
			cas_page_unmap(addr);
			mapping = tx_tiny_map(cp, ring, entry, tentry);
			len     = tabort;
		}

		cas_write_txd(cp, ring, entry, mapping, len, ctrl,
			      (frag + 1 == nr_frags));
		entry = TX_DESC_NEXT(ring, entry);
	}

	cp->tx_new[ring] = entry;
	if (TX_BUFFS_AVAIL(cp, ring) <= CAS_TABORT(cp)*(MAX_SKB_FRAGS + 1))
		netif_stop_queue(dev);

	if (netif_msg_tx_queued(cp))
		printk(KERN_DEBUG "%s: tx[%d] queued, slot %d, skblen %d, "
		       "avail %d\n",
		       dev->name, ring, entry, skb->len,
		       TX_BUFFS_AVAIL(cp, ring));
	writel(entry, cp->regs + REG_TX_KICKN(ring));
	spin_unlock_irqrestore(&cp->tx_lock[ring], flags);
	return 0;
}

static netdev_tx_t cas_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct cas *cp = netdev_priv(dev);

	/* this is only used as a load-balancing hint, so it doesn't
	 * need to be SMP safe
	 */
	static int ring;

	if (skb_padto(skb, cp->min_frame_size))
		return NETDEV_TX_OK;

	/* XXX: we need some higher-level QoS hooks to steer packets to
	 *      individual queues.
	 */
	if (cas_xmit_tx_ringN(cp, ring++ & N_TX_RINGS_MASK, skb))
		return NETDEV_TX_BUSY;
	dev->trans_start = jiffies;
	return NETDEV_TX_OK;
}

static void cas_init_tx_dma(struct cas *cp)
{
	u64 desc_dma = cp->block_dvma;
	unsigned long off;
	u32 val;
	int i;

	/* set up tx completion writeback registers. must be 8-byte aligned */
#ifdef USE_TX_COMPWB
	off = offsetof(struct cas_init_block, tx_compwb);
	writel((desc_dma + off) >> 32, cp->regs + REG_TX_COMPWB_DB_HI);
	writel((desc_dma + off) & 0xffffffff, cp->regs + REG_TX_COMPWB_DB_LOW);
#endif

	/* enable completion writebacks, enable paced mode,
	 * disable read pipe, and disable pre-interrupt compwbs
	 */
	val =   TX_CFG_COMPWB_Q1 | TX_CFG_COMPWB_Q2 |
		TX_CFG_COMPWB_Q3 | TX_CFG_COMPWB_Q4 |
		TX_CFG_DMA_RDPIPE_DIS | TX_CFG_PACED_MODE |
		TX_CFG_INTR_COMPWB_DIS;

	/* write out tx ring info and tx desc bases */
	for (i = 0; i < MAX_TX_RINGS; i++) {
		off = (unsigned long) cp->init_txds[i] -
			(unsigned long) cp->init_block;

		val |= CAS_TX_RINGN_BASE(i);
		writel((desc_dma + off) >> 32, cp->regs + REG_TX_DBN_HI(i));
		writel((desc_dma + off) & 0xffffffff, cp->regs +
		       REG_TX_DBN_LOW(i));
		/* don't zero out the kick register here as the system
		 * will wedge
		 */
	}
	writel(val, cp->regs + REG_TX_CFG);

	/* program max burst sizes. these numbers should be different
	 * if doing QoS.
	 */
#ifdef USE_QOS
	writel(0x800, cp->regs + REG_TX_MAXBURST_0);
	writel(0x1600, cp->regs + REG_TX_MAXBURST_1);
	writel(0x2400, cp->regs + REG_TX_MAXBURST_2);
	writel(0x4800, cp->regs + REG_TX_MAXBURST_3);
#else
	writel(0x800, cp->regs + REG_TX_MAXBURST_0);
	writel(0x800, cp->regs + REG_TX_MAXBURST_1);
	writel(0x800, cp->regs + REG_TX_MAXBURST_2);
	writel(0x800, cp->regs + REG_TX_MAXBURST_3);
#endif
}

/* Must be invoked under cp->lock. */
static inline void cas_init_dma(struct cas *cp)
{
	cas_init_tx_dma(cp);
	cas_init_rx_dma(cp);
}

/* Must be invoked under cp->lock. */
static u32 cas_setup_multicast(struct cas *cp)
{
	u32 rxcfg = 0;
	int i;

	if (cp->dev->flags & IFF_PROMISC) {
		rxcfg |= MAC_RX_CFG_PROMISC_EN;

	} else if (cp->dev->flags & IFF_ALLMULTI) {
	    	for (i=0; i < 16; i++)
			writel(0xFFFF, cp->regs + REG_MAC_HASH_TABLEN(i));
		rxcfg |= MAC_RX_CFG_HASH_FILTER_EN;

	} else {
		u16 hash_table[16];
		u32 crc;
		struct dev_mc_list *dmi = cp->dev->mc_list;
		int i;

		/* use the alternate mac address registers for the
		 * first 15 multicast addresses
		 */
		for (i = 1; i <= CAS_MC_EXACT_MATCH_SIZE; i++) {
			if (!dmi) {
				writel(0x0, cp->regs + REG_MAC_ADDRN(i*3 + 0));
				writel(0x0, cp->regs + REG_MAC_ADDRN(i*3 + 1));
				writel(0x0, cp->regs + REG_MAC_ADDRN(i*3 + 2));
				continue;
			}
			writel((dmi->dmi_addr[4] << 8) | dmi->dmi_addr[5],
			       cp->regs + REG_MAC_ADDRN(i*3 + 0));
			writel((dmi->dmi_addr[2] << 8) | dmi->dmi_addr[3],
			       cp->regs + REG_MAC_ADDRN(i*3 + 1));
			writel((dmi->dmi_addr[0] << 8) | dmi->dmi_addr[1],
			       cp->regs + REG_MAC_ADDRN(i*3 + 2));
			dmi = dmi->next;
		}

		/* use hw hash table for the next series of
		 * multicast addresses
		 */
		memset(hash_table, 0, sizeof(hash_table));
		while (dmi) {
 			crc = ether_crc_le(ETH_ALEN, dmi->dmi_addr);
			crc >>= 24;
			hash_table[crc >> 4] |= 1 << (15 - (crc & 0xf));
			dmi = dmi->next;
		}
	    	for (i=0; i < 16; i++)
			writel(hash_table[i], cp->regs +
			       REG_MAC_HASH_TABLEN(i));
		rxcfg |= MAC_RX_CFG_HASH_FILTER_EN;
	}

	return rxcfg;
}

/* must be invoked under cp->stat_lock[N_TX_RINGS] */
static void cas_clear_mac_err(struct cas *cp)
{
	writel(0, cp->regs + REG_MAC_COLL_NORMAL);
	writel(0, cp->regs + REG_MAC_COLL_FIRST);
	writel(0, cp->regs + REG_MAC_COLL_EXCESS);
	writel(0, cp->regs + REG_MAC_COLL_LATE);
	writel(0, cp->regs + REG_MAC_TIMER_DEFER);
	writel(0, cp->regs + REG_MAC_ATTEMPTS_PEAK);
	writel(0, cp->regs + REG_MAC_RECV_FRAME);
	writel(0, cp->regs + REG_MAC_LEN_ERR);
	writel(0, cp->regs + REG_MAC_ALIGN_ERR);
	writel(0, cp->regs + REG_MAC_FCS_ERR);
	writel(0, cp->regs + REG_MAC_RX_CODE_ERR);
}


static void cas_mac_reset(struct cas *cp)
{
	int i;

	/* do both TX and RX reset */
	writel(0x1, cp->regs + REG_MAC_TX_RESET);
	writel(0x1, cp->regs + REG_MAC_RX_RESET);

	/* wait for TX */
	i = STOP_TRIES;
	while (i-- > 0) {
		if (readl(cp->regs + REG_MAC_TX_RESET) == 0)
			break;
		udelay(10);
	}

	/* wait for RX */
	i = STOP_TRIES;
	while (i-- > 0) {
		if (readl(cp->regs + REG_MAC_RX_RESET) == 0)
			break;
		udelay(10);
	}

	if (readl(cp->regs + REG_MAC_TX_RESET) |
	    readl(cp->regs + REG_MAC_RX_RESET))
		printk(KERN_ERR "%s: mac tx[%d]/rx[%d] reset failed [%08x]\n",
		       cp->dev->name, readl(cp->regs + REG_MAC_TX_RESET),
		       readl(cp->regs + REG_MAC_RX_RESET),
		       readl(cp->regs + REG_MAC_STATE_MACHINE));
}


/* Must be invoked under cp->lock. */
static void cas_init_mac(struct cas *cp)
{
	unsigned char *e = &cp->dev->dev_addr[0];
	int i;
#ifdef CONFIG_CASSINI_MULTICAST_REG_WRITE
	u32 rxcfg;
#endif
	cas_mac_reset(cp);

	/* setup core arbitration weight register */
	writel(CAWR_RR_DIS, cp->regs + REG_CAWR);

	/* XXX Use pci_dma_burst_advice() */
#if !defined(CONFIG_SPARC64) && !defined(CONFIG_ALPHA)
	/* set the infinite burst register for chips that don't have
	 * pci issues.
	 */
	if ((cp->cas_flags & CAS_FLAG_TARGET_ABORT) == 0)
		writel(INF_BURST_EN, cp->regs + REG_INF_BURST);
#endif

	writel(0x1BF0, cp->regs + REG_MAC_SEND_PAUSE);

	writel(0x00, cp->regs + REG_MAC_IPG0);
	writel(0x08, cp->regs + REG_MAC_IPG1);
	writel(0x04, cp->regs + REG_MAC_IPG2);

	/* change later for 802.3z */
	writel(0x40, cp->regs + REG_MAC_SLOT_TIME);

	/* min frame + FCS */
	writel(ETH_ZLEN + 4, cp->regs + REG_MAC_FRAMESIZE_MIN);

	/* Ethernet payload + header + FCS + optional VLAN tag. NOTE: we
	 * specify the maximum frame size to prevent RX tag errors on
	 * oversized frames.
	 */
	writel(CAS_BASE(MAC_FRAMESIZE_MAX_BURST, 0x2000) |
	       CAS_BASE(MAC_FRAMESIZE_MAX_FRAME,
			(CAS_MAX_MTU + ETH_HLEN + 4 + 4)),
	       cp->regs + REG_MAC_FRAMESIZE_MAX);

	/* NOTE: crc_size is used as a surrogate for half-duplex.
	 * workaround saturn half-duplex issue by increasing preamble
	 * size to 65 bytes.
	 */
	if ((cp->cas_flags & CAS_FLAG_SATURN) && cp->crc_size)
		writel(0x41, cp->regs + REG_MAC_PA_SIZE);
	else
		writel(0x07, cp->regs + REG_MAC_PA_SIZE);
	writel(0x04, cp->regs + REG_MAC_JAM_SIZE);
	writel(0x10, cp->regs + REG_MAC_ATTEMPT_LIMIT);
	writel(0x8808, cp->regs + REG_MAC_CTRL_TYPE);

	writel((e[5] | (e[4] << 8)) & 0x3ff, cp->regs + REG_MAC_RANDOM_SEED);

	writel(0, cp->regs + REG_MAC_ADDR_FILTER0);
	writel(0, cp->regs + REG_MAC_ADDR_FILTER1);
	writel(0, cp->regs + REG_MAC_ADDR_FILTER2);
	writel(0, cp->regs + REG_MAC_ADDR_FILTER2_1_MASK);
	writel(0, cp->regs + REG_MAC_ADDR_FILTER0_MASK);

	/* setup mac address in perfect filter array */
	for (i = 0; i < 45; i++)
		writel(0x0, cp->regs + REG_MAC_ADDRN(i));

	writel((e[4] << 8) | e[5], cp->regs + REG_MAC_ADDRN(0));
	writel((e[2] << 8) | e[3], cp->regs + REG_MAC_ADDRN(1));
	writel((e[0] << 8) | e[1], cp->regs + REG_MAC_ADDRN(2));

	writel(0x0001, cp->regs + REG_MAC_ADDRN(42));
	writel(0xc200, cp->regs + REG_MAC_ADDRN(43));
	writel(0x0180, cp->regs + REG_MAC_ADDRN(44));

#ifndef CONFIG_CASSINI_MULTICAST_REG_WRITE
	cp->mac_rx_cfg = cas_setup_multicast(cp);
#else
	/* WTZ: Do what Adrian did in cas_set_multicast. Doing
	 * a writel does not seem to be necessary because Cassini
	 * seems to preserve the configuration when we do the reset.
	 * If the chip is in trouble, though, it is not clear if we
	 * can really count on this behavior. cas_set_multicast uses
	 * spin_lock_irqsave, but we are called only in cas_init_hw and
	 * cas_init_hw is protected by cas_lock_all, which calls
	 * spin_lock_irq (so it doesn't need to save the flags, and
	 * we should be OK for the writel, as that is the only
	 * difference).
	 */
	cp->mac_rx_cfg = rxcfg = cas_setup_multicast(cp);
	writel(rxcfg, cp->regs + REG_MAC_RX_CFG);
#endif
	spin_lock(&cp->stat_lock[N_TX_RINGS]);
	cas_clear_mac_err(cp);
	spin_unlock(&cp->stat_lock[N_TX_RINGS]);

	/* Setup MAC interrupts.  We want to get all of the interesting
	 * counter expiration events, but we do not want to hear about
	 * normal rx/tx as the DMA engine tells us that.
	 */
	writel(MAC_TX_FRAME_XMIT, cp->regs + REG_MAC_TX_MASK);
	writel(MAC_RX_FRAME_RECV, cp->regs + REG_MAC_RX_MASK);

	/* Don't enable even the PAUSE interrupts for now, we
	 * make no use of those events other than to record them.
	 */
	writel(0xffffffff, cp->regs + REG_MAC_CTRL_MASK);
}

/* Must be invoked under cp->lock. */
static void cas_init_pause_thresholds(struct cas *cp)
{
	/* Calculate pause thresholds.  Setting the OFF threshold to the
	 * full RX fifo size effectively disables PAUSE generation
	 */
	if (cp->rx_fifo_size <= (2 * 1024)) {
		cp->rx_pause_off = cp->rx_pause_on = cp->rx_fifo_size;
	} else {
		int max_frame = (cp->dev->mtu + ETH_HLEN + 4 + 4 + 64) & ~63;
		if (max_frame * 3 > cp->rx_fifo_size) {
			cp->rx_pause_off = 7104;
			cp->rx_pause_on  = 960;
		} else {
			int off = (cp->rx_fifo_size - (max_frame * 2));
			int on = off - max_frame;
			cp->rx_pause_off = off;
			cp->rx_pause_on = on;
		}
	}
}

static int cas_vpd_match(const void __iomem *p, const char *str)
{
	int len = strlen(str) + 1;
	int i;

	for (i = 0; i < len; i++) {
		if (readb(p + i) != str[i])
			return 0;
	}
	return 1;
}


/* get the mac address by reading the vpd information in the rom.
 * also get the phy type and determine if there's an entropy generator.
 * NOTE: this is a bit convoluted for the following reasons:
 *  1) vpd info has order-dependent mac addresses for multinic cards
 *  2) the only way to determine the nic order is to use the slot
 *     number.
 *  3) fiber cards don't have bridges, so their slot numbers don't
 *     mean anything.
 *  4) we don't actually know we have a fiber card until after
 *     the mac addresses are parsed.
 */
static int cas_get_vpd_info(struct cas *cp, unsigned char *dev_addr,
			    const int offset)
{
	void __iomem *p = cp->regs + REG_EXPANSION_ROM_RUN_START;
	void __iomem *base, *kstart;
	int i, len;
	int found = 0;
#define VPD_FOUND_MAC        0x01
#define VPD_FOUND_PHY        0x02

	int phy_type = CAS_PHY_MII_MDIO0; /* default phy type */
	int mac_off  = 0;

	/* give us access to the PROM */
	writel(BIM_LOCAL_DEV_PROM | BIM_LOCAL_DEV_PAD,
	       cp->regs + REG_BIM_LOCAL_DEV_EN);

	/* check for an expansion rom */
	if (readb(p) != 0x55 || readb(p + 1) != 0xaa)
		goto use_random_mac_addr;

	/* search for beginning of vpd */
	base = NULL;
	for (i = 2; i < EXPANSION_ROM_SIZE; i++) {
		/* check for PCIR */
		if ((readb(p + i + 0) == 0x50) &&
		    (readb(p + i + 1) == 0x43) &&
		    (readb(p + i + 2) == 0x49) &&
		    (readb(p + i + 3) == 0x52)) {
			base = p + (readb(p + i + 8) |
				    (readb(p + i + 9) << 8));
			break;
		}
	}

	if (!base || (readb(base) != 0x82))
		goto use_random_mac_addr;

	i = (readb(base + 1) | (readb(base + 2) << 8)) + 3;
	while (i < EXPANSION_ROM_SIZE) {
		if (readb(base + i) != 0x90) /* no vpd found */
			goto use_random_mac_addr;

		/* found a vpd field */
		len = readb(base + i + 1) | (readb(base + i + 2) << 8);

		/* extract keywords */
		kstart = base + i + 3;
		p = kstart;
		while ((p - kstart) < len) {
			int klen = readb(p + 2);
			int j;
			char type;

			p += 3;

			/* look for the following things:
			 * -- correct length == 29
			 * 3 (type) + 2 (size) +
			 * 18 (strlen("local-mac-address") + 1) +
			 * 6 (mac addr)
			 * -- VPD Instance 'I'
			 * -- VPD Type Bytes 'B'
			 * -- VPD data length == 6
			 * -- property string == local-mac-address
			 *
			 * -- correct length == 24
			 * 3 (type) + 2 (size) +
			 * 12 (strlen("entropy-dev") + 1) +
			 * 7 (strlen("vms110") + 1)
			 * -- VPD Instance 'I'
			 * -- VPD Type String 'B'
			 * -- VPD data length == 7
			 * -- property string == entropy-dev
			 *
			 * -- correct length == 18
			 * 3 (type) + 2 (size) +
			 * 9 (strlen("phy-type") + 1) +
			 * 4 (strlen("pcs") + 1)
			 * -- VPD Instance 'I'
			 * -- VPD Type String 'S'
			 * -- VPD data length == 4
			 * -- property string == phy-type
			 *
			 * -- correct length == 23
			 * 3 (type) + 2 (size) +
			 * 14 (strlen("phy-interface") + 1) +
			 * 4 (strlen("pcs") + 1)
			 * -- VPD Instance 'I'
			 * -- VPD Type String 'S'
			 * -- VPD data length == 4
			 * -- property string == phy-interface
			 */
			if (readb(p) != 'I')
				goto next;

			/* finally, check string and length */
			type = readb(p + 3);
			if (type == 'B') {
				if ((klen == 29) && readb(p + 4) == 6 &&
				    cas_vpd_match(p + 5,
						  "local-mac-address")) {
					if (mac_off++ > offset)
						goto next;

					/* set mac address */
					for (j = 0; j < 6; j++)
						dev_addr[j] =
							readb(p + 23 + j);
					goto found_mac;
				}
			}

			if (type != 'S')
				goto next;

#ifdef USE_ENTROPY_DEV
			if ((klen == 24) &&
			    cas_vpd_match(p + 5, "entropy-dev") &&
			    cas_vpd_match(p + 17, "vms110")) {
				cp->cas_flags |= CAS_FLAG_ENTROPY_DEV;
				goto next;
			}
#endif

			if (found & VPD_FOUND_PHY)
				goto next;

			if ((klen == 18) && readb(p + 4) == 4 &&
			    cas_vpd_match(p + 5, "phy-type")) {
				if (cas_vpd_match(p + 14, "pcs")) {
					phy_type = CAS_PHY_SERDES;
					goto found_phy;
				}
			}

			if ((klen == 23) && readb(p + 4) == 4 &&
			    cas_vpd_match(p + 5, "phy-interface")) {
				if (cas_vpd_match(p + 19, "pcs")) {
					phy_type = CAS_PHY_SERDES;
					goto found_phy;
				}
			}
found_mac:
			found |= VPD_FOUND_MAC;
			goto next;

found_phy:
			found |= VPD_FOUND_PHY;

next:
			p += klen;
		}
		i += len + 3;
	}

use_random_mac_addr:
	if (found & VPD_FOUND_MAC)
		goto done;

	/* Sun MAC prefix then 3 random bytes. */
	printk(PFX "MAC address not found in ROM VPD\n");
	dev_addr[0] = 0x08;
	dev_addr[1] = 0x00;
	dev_addr[2] = 0x20;
	get_random_bytes(dev_addr + 3, 3);

done:
	writel(0, cp->regs + REG_BIM_LOCAL_DEV_EN);
	return phy_type;
}

/* check pci invariants */
static void cas_check_pci_invariants(struct cas *cp)
{
	struct pci_dev *pdev = cp->pdev;

	cp->cas_flags = 0;
	if ((pdev->vendor == PCI_VENDOR_ID_SUN) &&
	    (pdev->device == PCI_DEVICE_ID_SUN_CASSINI)) {
		if (pdev->revision >= CAS_ID_REVPLUS)
			cp->cas_flags |= CAS_FLAG_REG_PLUS;
		if (pdev->revision < CAS_ID_REVPLUS02u)
			cp->cas_flags |= CAS_FLAG_TARGET_ABORT;

		/* Original Cassini supports HW CSUM, but it's not
		 * enabled by default as it can trigger TX hangs.
		 */
		if (pdev->revision < CAS_ID_REV2)
			cp->cas_flags |= CAS_FLAG_NO_HW_CSUM;
	} else {
		/* Only sun has original cassini chips.  */
		cp->cas_flags |= CAS_FLAG_REG_PLUS;

		/* We use a flag because the same phy might be externally
		 * connected.
		 */
		if ((pdev->vendor == PCI_VENDOR_ID_NS) &&
		    (pdev->device == PCI_DEVICE_ID_NS_SATURN))
			cp->cas_flags |= CAS_FLAG_SATURN;
	}
}


static int cas_check_invariants(struct cas *cp)
{
	struct pci_dev *pdev = cp->pdev;
	u32 cfg;
	int i;

	/* get page size for rx buffers. */
	cp->page_order = 0;
#ifdef USE_PAGE_ORDER
	if (PAGE_SHIFT < CAS_JUMBO_PAGE_SHIFT) {
		/* see if we can allocate larger pages */
		struct page *page = alloc_pages(GFP_ATOMIC,
						CAS_JUMBO_PAGE_SHIFT -
						PAGE_SHIFT);
		if (page) {
			__free_pages(page, CAS_JUMBO_PAGE_SHIFT - PAGE_SHIFT);
			cp->page_order = CAS_JUMBO_PAGE_SHIFT - PAGE_SHIFT;
		} else {
			printk(PFX "MTU limited to %d bytes\n", CAS_MAX_MTU);
		}
	}
#endif
	cp->page_size = (PAGE_SIZE << cp->page_order);

	/* Fetch the FIFO configurations. */
	cp->tx_fifo_size = readl(cp->regs + REG_TX_FIFO_SIZE) * 64;
	cp->rx_fifo_size = RX_FIFO_SIZE;

	/* finish phy determination. MDIO1 takes precedence over MDIO0 if
	 * they're both connected.
	 */
	cp->phy_type = cas_get_vpd_info(cp, cp->dev->dev_addr,
					PCI_SLOT(pdev->devfn));
	if (cp->phy_type & CAS_PHY_SERDES) {
		cp->cas_flags |= CAS_FLAG_1000MB_CAP;
		return 0; /* no more checking needed */
	}

	/* MII */
	cfg = readl(cp->regs + REG_MIF_CFG);
	if (cfg & MIF_CFG_MDIO_1) {
		cp->phy_type = CAS_PHY_MII_MDIO1;
	} else if (cfg & MIF_CFG_MDIO_0) {
		cp->phy_type = CAS_PHY_MII_MDIO0;
	}

	cas_mif_poll(cp, 0);
	writel(PCS_DATAPATH_MODE_MII, cp->regs + REG_PCS_DATAPATH_MODE);

	for (i = 0; i < 32; i++) {
		u32 phy_id;
		int j;

		for (j = 0; j < 3; j++) {
			cp->phy_addr = i;
			phy_id = cas_phy_read(cp, MII_PHYSID1) << 16;
			phy_id |= cas_phy_read(cp, MII_PHYSID2);
			if (phy_id && (phy_id != 0xFFFFFFFF)) {
				cp->phy_id = phy_id;
				goto done;
			}
		}
	}
	printk(KERN_ERR PFX "MII phy did not respond [%08x]\n",
	       readl(cp->regs + REG_MIF_STATE_MACHINE));
	return -1;

done:
	/* see if we can do gigabit */
	cfg = cas_phy_read(cp, MII_BMSR);
	if ((cfg & CAS_BMSR_1000_EXTEND) &&
	    cas_phy_read(cp, CAS_MII_1000_EXTEND))
		cp->cas_flags |= CAS_FLAG_1000MB_CAP;
	return 0;
}

/* Must be invoked under cp->lock. */
static inline void cas_start_dma(struct cas *cp)
{
	int i;
	u32 val;
	int txfailed = 0;

	/* enable dma */
	val = readl(cp->regs + REG_TX_CFG) | TX_CFG_DMA_EN;
	writel(val, cp->regs + REG_TX_CFG);
	val = readl(cp->regs + REG_RX_CFG) | RX_CFG_DMA_EN;
	writel(val, cp->regs + REG_RX_CFG);

	/* enable the mac */
	val = readl(cp->regs + REG_MAC_TX_CFG) | MAC_TX_CFG_EN;
	writel(val, cp->regs + REG_MAC_TX_CFG);
	val = readl(cp->regs + REG_MAC_RX_CFG) | MAC_RX_CFG_EN;
	writel(val, cp->regs + REG_MAC_RX_CFG);

	i = STOP_TRIES;
	while (i-- > 0) {
		val = readl(cp->regs + REG_MAC_TX_CFG);
		if ((val & MAC_TX_CFG_EN))
			break;
		udelay(10);
	}
	if (i < 0) txfailed = 1;
	i = STOP_TRIES;
	while (i-- > 0) {
		val = readl(cp->regs + REG_MAC_RX_CFG);
		if ((val & MAC_RX_CFG_EN)) {
			if (txfailed) {
			  printk(KERN_ERR
				 "%s: enabling mac failed [tx:%08x:%08x].\n",
				 cp->dev->name,
				 readl(cp->regs + REG_MIF_STATE_MACHINE),
				 readl(cp->regs + REG_MAC_STATE_MACHINE));
			}
			goto enable_rx_done;
		}
		udelay(10);
	}
	printk(KERN_ERR "%s: enabling mac failed [%s:%08x:%08x].\n",
	       cp->dev->name,
	       (txfailed? "tx,rx":"rx"),
	       readl(cp->regs + REG_MIF_STATE_MACHINE),
	       readl(cp->regs + REG_MAC_STATE_MACHINE));

enable_rx_done:
	cas_unmask_intr(cp); /* enable interrupts */
	writel(RX_DESC_RINGN_SIZE(0) - 4, cp->regs + REG_RX_KICK);
	writel(0, cp->regs + REG_RX_COMP_TAIL);

	if (cp->cas_flags & CAS_FLAG_REG_PLUS) {
		if (N_RX_DESC_RINGS > 1)
			writel(RX_DESC_RINGN_SIZE(1) - 4,
			       cp->regs + REG_PLUS_RX_KICK1);

		for (i = 1; i < N_RX_COMP_RINGS; i++)
			writel(0, cp->regs + REG_PLUS_RX_COMPN_TAIL(i));
	}
}

/* Must be invoked under cp->lock. */
static void cas_read_pcs_link_mode(struct cas *cp, int *fd, int *spd,
				   int *pause)
{
	u32 val = readl(cp->regs + REG_PCS_MII_LPA);
	*fd     = (val & PCS_MII_LPA_FD) ? 1 : 0;
	*pause  = (val & PCS_MII_LPA_SYM_PAUSE) ? 0x01 : 0x00;
	if (val & PCS_MII_LPA_ASYM_PAUSE)
		*pause |= 0x10;
	*spd = 1000;
}

/* Must be invoked under cp->lock. */
static void cas_read_mii_link_mode(struct cas *cp, int *fd, int *spd,
				   int *pause)
{
	u32 val;

	*fd = 0;
	*spd = 10;
	*pause = 0;

	/* use GMII registers */
	val = cas_phy_read(cp, MII_LPA);
	if (val & CAS_LPA_PAUSE)
		*pause = 0x01;

	if (val & CAS_LPA_ASYM_PAUSE)
		*pause |= 0x10;

	if (val & LPA_DUPLEX)
		*fd = 1;
	if (val & LPA_100)
		*spd = 100;

	if (cp->cas_flags & CAS_FLAG_1000MB_CAP) {
		val = cas_phy_read(cp, CAS_MII_1000_STATUS);
		if (val & (CAS_LPA_1000FULL | CAS_LPA_1000HALF))
			*spd = 1000;
		if (val & CAS_LPA_1000FULL)
			*fd = 1;
	}
}

/* A link-up condition has occurred, initialize and enable the
 * rest of the chip.
 *
 * Must be invoked under cp->lock.
 */
static void cas_set_link_modes(struct cas *cp)
{
	u32 val;
	int full_duplex, speed, pause;

	full_duplex = 0;
	speed = 10;
	pause = 0;

	if (CAS_PHY_MII(cp->phy_type)) {
		cas_mif_poll(cp, 0);
		val = cas_phy_read(cp, MII_BMCR);
		if (val & BMCR_ANENABLE) {
			cas_read_mii_link_mode(cp, &full_duplex, &speed,
					       &pause);
		} else {
			if (val & BMCR_FULLDPLX)
				full_duplex = 1;

			if (val & BMCR_SPEED100)
				speed = 100;
			else if (val & CAS_BMCR_SPEED1000)
				speed = (cp->cas_flags & CAS_FLAG_1000MB_CAP) ?
					1000 : 100;
		}
		cas_mif_poll(cp, 1);

	} else {
		val = readl(cp->regs + REG_PCS_MII_CTRL);
		cas_read_pcs_link_mode(cp, &full_duplex, &speed, &pause);
		if ((val & PCS_MII_AUTONEG_EN) == 0) {
			if (val & PCS_MII_CTRL_DUPLEX)
				full_duplex = 1;
		}
	}

	if (netif_msg_link(cp))
		printk(KERN_INFO "%s: Link up at %d Mbps, %s-duplex.\n",
		       cp->dev->name, speed, (full_duplex ? "full" : "half"));

	val = MAC_XIF_TX_MII_OUTPUT_EN | MAC_XIF_LINK_LED;
	if (CAS_PHY_MII(cp->phy_type)) {
		val |= MAC_XIF_MII_BUFFER_OUTPUT_EN;
		if (!full_duplex)
			val |= MAC_XIF_DISABLE_ECHO;
	}
	if (full_duplex)
		val |= MAC_XIF_FDPLX_LED;
	if (speed == 1000)
		val |= MAC_XIF_GMII_MODE;
	writel(val, cp->regs + REG_MAC_XIF_CFG);

	/* deal with carrier and collision detect. */
	val = MAC_TX_CFG_IPG_EN;
	if (full_duplex) {
		val |= MAC_TX_CFG_IGNORE_CARRIER;
		val |= MAC_TX_CFG_IGNORE_COLL;
	} else {
#ifndef USE_CSMA_CD_PROTO
		val |= MAC_TX_CFG_NEVER_GIVE_UP_EN;
		val |= MAC_TX_CFG_NEVER_GIVE_UP_LIM;
#endif
	}
	/* val now set up for REG_MAC_TX_CFG */

	/* If gigabit and half-duplex, enable carrier extension
	 * mode.  increase slot time to 512 bytes as well.
	 * else, disable it and make sure slot time is 64* cassun Micalso activate checksum bug workaroundn Mi/
	if ((speed == 1000) && !full_duplex) {
		writel(val | MAC_TX_CFG_CARRIER_EXTEND,
		 an redcp->regs + REG_ogram is f);

		his = readl(stribute it and/or
R* modifyit und&= ~ General P_STRIP_FCS; /*osystems I * Copyrig (C) 
 *
 * This progra * publee software; you can redistribute it and/or
eral Publn; either 0x200,istribute it and/or
SLOT_TIME
 *
 *stricrc_size = 4blic/* minimum  usefgigabit frame at half risingtion; strimin_; wite usefulCAS_n (aMB_MIN_FRAME *
 }crosycom)
 *
 * Thism is distributed in t * modify ite Free Softwnc.
 * Copyrig. don't strip FCS when inu ca*t eve- the immodeceiveon;  under the terms of the GNU General Public2003rksunrising.com)
al Pu|=on 2 of the
ished by to th will be useful0on, Inc.,ranty of
 * MERCHANTAor FMTUon, R A PARTICUc License as
 * published by tn, Inc., 59 Temple ,
 *  - Suite 330, Boston, MA
 * 02ITNESS 		}n; either vers&se as
 * publ * License, or (at your option) any later version.
 * * This pro4am is distributed in the hope that	}
) 2003netif_msg_link(cp).com)
2003pause & 0x01e to thprintk(KERN_INFO "%s: P loadis en Casd "ler
an redi"(rxfifo: %d off CPU dn CPU)\n"ou ccan redistridev->namege-based RX descrx_iplee usegine with separate   loa_offn rings
 *  Gigabit suppornem chR A PAR      load bal10cing (non-VLAN mode)
 *  batcTX   loadultiple page-based RX descriptor en  MIF link ung (non-VLAN mode)
 *  batching of mems Casfers that are attached as fragmentse used under the terms of the GNU GenCTRLhe gem c License(.
 *  -- the_SEND_PAUSE_EN prograayers assRECVthat the are       loa) {he Fsymmetric or acassini wi  loadeneral Puee Softayers assume that the ogram; i  load balancinlt, cassini wil amount (664 bytes)
 *     to m skb
 *     i keeps reULAR PURPOSE.  See the
 * GNU Ge  -- the uppcas_start_dmaLAN);
}

/* Must be invoked underistrilock.unt static void atioinit_hw(struct is  *cp, int ren, theS (V)
{elf. aference to tha	matiophystilliver samatiotill  suppothresholdsiver sr use. othmacrecycles the
 *  driver s page,
 *     and a* the/* Defaulni(+eg pa wittersmplied war.
 *r_ticksle Placeatiobegin_auto_negotiation(s itNULLare ulink up/doage.
f then SuS (V_upe headationee to t_he G recycle	 eithecarrier_ headscripare uswaps the page with a spare page.
 *   on earlie paassini boards,
 * SOFT_0of mtied to PCIeferet. we loadthformo force a pcis to bever,letsinisettle out,i(+) tave fereoreiver
eues..
   if that page is shard_ to b use, it frees hat p *
 * TBIM_LOCAL_DEV_ QoS im is distributed red via use of itseludelay(20yclepcitiveto pon, ten forp load }

if that page is sglobaltively, the queues caits reblkflaghat pant limitcass/* issu drig has s to be *
 * YloadRSTOUT    ilf. aceivablun@daANTAPHY_MIIthe inhy_type) or
 */* For PCS, have thenceivablof m lev expshouldes driveceivedSW_REST_BLOCK_PCS_SLINK ANTYto prevento doted ults ofceiveddesilast ack equires a h from being cleared.  We'llceivednriansome special handl-encifuish chipIOSes dts ro aceivedloopbackthe G.NU Genera *
 * T(at. alET_TX |hat. alET_Rs rx packets., the two descr)ou can redistribute it and packetsy.
 *
 * TXom)
 *
 * T to process rx packets.
 m is distributed h>
#include < multie useto waini(t leetwe3ms befo parolng
 *registerunt (me rx c3 cass unde = STOP_TRIEem dwhile ( unde-- > n worksu32  under the terms of the GNUh>
#include  2003 sini cinclude <linux/compiler.h>))n Su0allo	gotot thehighhe rx c1mplet}
non-VLAN modeERR  batcsws to b failed. pagttached as fragme
ping:multiultipl various BIMts rerruptth it
configured  assDPAR_INTR_ENABLE |nux/skbuffMA
#include <lin
ets
 *  x/random.T>
#include <lll-purpose
 * ioctl.
modify ie Frypteues iver
errorcularus masko thfferinedh>
#inht (C) wet the Ideal with DMA couhtoo overflowsini.they happen (C) 20luish .
 *un Minclude <li0xFude <asUi ch(PCIh>
#_BADACK |needr.h>
DTRTOinuxbased RX <asm/uacOTHERde <asm/uacx/raDMA_WRITlinux#define cas_page__atomic(READ)m is distribux/mii.h> and<asm/uacSTATUS_MASKinclude nux/up/firmMII by dhoweverfaulddres<linc rx<linux/.
 *outn Micple
 <asm/io.h>
#intwo DATAPATH_MODErly m is distributed PI) && defined(HA but tf that page is sll of the data. RX can conceivably com>
#incl for useinux_intrrecycles thg has all of  canceivablycles thmacselect whare to uentropy*/
#define Ulude ems Cassdma enginesment n under the terms of the GNU * modifyper protom is frmic(nd inLAR PURPOSE.  See the
 * GN * modify i under the terms of the GNUeral Publi License * publP_ALT_FIRMWARE   cas_prog_null /* alrsion.
 *
 /* program heare ppars<linux/2003  willas_vabls &HANTAFLAG_TARGET_ABORT) |nux/mii(ANTAHP_ALT_FIRMWAREn Sucatesrog_null) or
 *d CSload_firmwar the,HANTAHPt mitigatude <linux/typeY_DEV     /* don't test for erupt mitigatare usedde <linux/mut>
#incclude <lth it
spin_
 * (& for allke cp[NlterRINGS]are to u<linuuse *er_rx(x)
#o makune cp->lock finer-grained.
 */
#undwaps tShut dowdriverd.
 , me page cal.h>


#inpm_mutex hel pac  if that page is sshutunde, the queues can be unsigned long MA/CDne HP_WMtherus not-runn-encfaulpage     asted pawlocatm/io forhw_ allocate Pla
	del_    assync->lockre us    aine HP_WStop designes()
nux/*/
#if 0t.h>
#incatomi */
ad->lock 0   _/* i_pending_mtu       /*    #define RX_COPY_MIN    64   /* copy spareittle to make upper layers happy */
#undef  RX_COaSE_E
		schedule(ine #rosyfrags */
#define RX_COPY_MIN    64   /* copy
#define DRV_MODU#* cofcan bActually sY_ALWAYSd.
 * RX_D_DEV ck */
_sav't tesMA/CDare to uelect whicmplet has 4 qd CSMA/CD */
#define SATURNallocates a powerrocesine USE_HPCI_INT21 Maring ha08"

#define cas_skb_ree inf  UShange a l use, itnet_device *devits renew a liat puse, it frees  =\
	 dev_priv( load ed for e
	 NE < MA
 * 02111 ||| \
	 NE > MA
 *AX2111#defreturn -EINVAL* if 1v->
 * leng
	 NEE	  \
	! eithe allocae bef bor/
#defiETIF_M_p 0  nte befimeout() sh/* if/ so thWAYS 0    /* ifware.hsini 0, use1
	#defineitch flowRV_MODULE_NAME	": "E	  \
	by
 * the BIO */
#deoperSERDES) or
 *MEOUT           (1)

/* timeout va */

de <linux/typember
 * of 10us delays to be used ba li<linuxine DRV__ * C      (1)

/* tiMODULNAME	#defineely,     (1)

/* timeout va,as 4 qe changing. these specify  ?u can ANTAckets.ALL :et header -x_tiletinetdevice.h>
#in 0        /* in\
	 NETIF_MSG_I\n"ef  UIES     5000

/* specify a minimuE_VER
	flush_	97
#defd 5000
    OUT       cas_skb_release(x) <linn_txd use, it frees its ref": "IF_MSG_TX_ERR)_tx_desc *txd =istritill txds[    ]f  Use, itsk_buff *skb, *ese g as*    x_use #if 1
/*
u64 d    , dlenE	  nt i,T ANYore  usefulTX_DESC.
 */N_SIZE(     ;
	firm(i   /* i <a race i++ or
 * nTY; wg *
 *2003tomici]n Su copyler
 ontinuce co	skb =        high               *
 *ET_MTCAS_      tic c<T_SPA_shinfoL   )->nrty ogs; versi  1
#deffine are = iincl usef- 1
 *
 *    fir pagufferof mnever a tinyDATE ")\(+) so_SKB_<linuxwn tobe unmasm/dear  Genera	r eac = le64_to_cpu(txd[ent].ATE ")em ch	 to
  =net heVAL(.
 */
#eBUFLENge-ba		e */
static int link_m
#derol)MODULEion  CAS__pag the info,,or each, to
unrisiefine cas_pmic(TODEVICthat itam; ific c![] __devinitdata =
	DRV_MOD".c:v" 	i++("sun/e <linxLDATE ")\mightS_NCic int cassin_MSG_-1;	skip petweit bitmappABLE 	MODULE_VERSION " (" DR, inthas 4 qtx_ int_use#if 1
link_mused    2int, 0)	 *
 * 
 * h ofkfreeatom_anydata upts can bzerox/mut int cas usagount (memect wh, int, 0);
MODULE_P, 0 a rac* useof(*wn due to the chip
 *"Sunwaps t");
chin clomount _IFDOWN	|line page is s");

rsize <AX_MTU                     mincatesage_t ** */
separatbit sgeic counteravoid a race conditionR
 */
#else
#define CAS_RESET_MTU                   1
#defi  -- ge[i]".c:v" ut.
 */
s");
t tesdown_timun Casown_ti    3
#eneeps recas_skb_release(x) OUT 5
/ds, the queues can be avoidore ET_MTU          Nregi*/
#else
S     1 NOTE: OUT 5
/*
 * V can r swaps the page with a spare page.
 *     if that page is sine CA    /*
 * value in 'ticks' (units  <linux/deline Cm.h>
#x     th it
goes down due old being cod and neverold)*ined.
 */
c. fgoes down due new

static u16 link_monew[] __devinitdataused by jiffies). ed.
 */
nit the
 * moine CAS_MA'HZ' in  for a PCS bugtill bTE	"_transition_timetill LLDPL

static u16(((cp->paget full dup"Sun   This will

/*ine USE_HP : 100btcfull duwaps tallocatup."
 openn if defineLT_LINK	| \
	 N|BMCR5
/*
 * Value in seconds, for user input.
 */
static int linkdown_timeout = DEFAULTetion U     N_TIMEOUT;
module_param(linkdown_timeout, int, 0);
MODULE_PARM_DESC(linkkdown_tisepat.
 */
s|BMCR00bt GFP_ modELde <li        2ut() sho1<linuxU                   uplex */
};

sta
/*
 * value in 'ticks' (units used by jiffies). Set when we init thor
 *    x */
};

static st'HZ' i <ux/pci.h * module be full dup, 0, 0, 0UL },crementU                     60
#defIN    64  R_SPEED1 * C_ * Elim* * CTIF_MSG_TX_ERR)

/* le
#deainashif( * C,You , it fr,S 0   y a minimuse frine /* copy =e upper layers happy */
#undef  RX_Cinimum fraTZ: QA was _SATfinding deadlock problems with the prebefore gersions afteUNT_Bong test runs with multiple cards per mUNT_BUne.
 * See if ro fix #define RX_COPY_MIN    64   /* copy a liore we dons after lon= 0un@dSee if replacinng properly reso fixlinuand convW    n have mo pa/* is 
#define  thstat"1.6"
# but we usEF_MSGe enab#definede       (1)

/* timeout values+)
		sp<linuULE_VERSIONTsh bink ware undeon't  0    /esigissuebut keepn MiccludefinDEF_ Used downfuncpted a* alssedefine for
#incni.c: Sun Mi  /* stDONT_BATCH  0 /pci.h>ine USE_PAGE_ORDER       /* spenet det/checksgby defool.h>
#or intpacke>
#incld-robin)
#defidetachn for load bRELDATE	"21 May 2008"

#define inline vp->X /*ed*cp)
{
ock
 * wll

/* rNT_B_rectomi have wn_unlock_irX /*_MSG_ENirqsaet/chfy tMCR_Fialusefish bistsck_irq(&cp->lock);all_resuses unticas_unlockof m    /*_MSG_ENABLE k_irq(&cp->lock);N,
	  PCIATOMICd keepsFAST_TI
	sptest => onlyperly restored,     enabm
 */ons after lon@da/* copy a lix/dma-mapping.hmum fraupts
 * arenviout header -SPterrwon't get any morE_VERS
	sphave ts */
	if (ring == 0)ALL,
	strfollowingceivedunloc.
 *s still in wilupts,, theen
  equires a hear ma Settocatehcas cond argumare ofck[i]);
}
 tg =  * !pts */
	if (ring == 0)ALL) selecw to disPLUS) {
	stinguio 1 (arge 
#incl) \
do {  CAS_FLAPHY(xxxc_FLAnorma, but wPCSterre) + REGask */
	if (cp->\n";
oreference in other

static AS_DEF_MSG_ENAefined (Uer lonux//highmem.s_unlock_tx)
{
	intis will defa(i = 0;is still in  tesINTRN_MASK_CLEAR_ALLore intendif
			writel(INTRN_MASE_PCI_INTB) || deALL | INTRN_MASK_RX_EN,
			       cp->regs + REG_PLUS_INTRN_MAStel(INTRN_MASK_CLEAR_ALL,ME        x/skbuf		| \
	 NETIF_MSG_TIMER	| \
	 NETIF_ock[i - 1]);
}
attatic inline voi)

staticme size ub
 * aren't b, ith multiple cards per machine.irq(cp, i);
}

statq(&cpvoid cas_enable_irq(structlps. The
irq(cp, i);
}

statmtuvoid cas_enable_irq(structine STO places.
 */
#define cas_lock_all_savmum frame size to deal with some fifo issuempleF, cp->cas_skb_release(x) s */
#defi( cas *cp)
{
	idataTIF_MSG_TX_ERR)

/* le use, it free)USE_Pne.
 * inux	}
	}
}

stbein 0    e Plac cas *cp)
{
	int i;

	flinks */
#ransi a h
#def/mut!ng proux/miiflows */
#
#endif
		jiffies_validN_MASK_RX((      R -X_EN, cp->regs +
			       R) >ux/mii.hcase 3:
#endif
			writel)S and convOne-G_REG_Pe <asm/asoct ca-undefare Foundatdoesn' definec loadp); \wn tooccucas *fetweewn to to>
#incswitch
#if ddefauthink CAS_FLAt cas -- ow>cas_fllied war cp->regs +
			       REG_PLUSe Placused for!cas_unlock_tx(sve(cp, flag to make cp_irqy 200 flowsduple#define CAS_DTE	"2txfine USE_HP_WORKAROgath, flaalf duplIinsteat cas/* ifOSestelecifo issuewe jusdefinere	97
#defegs + REG_E   anline FAST_TIlink test runs with multiple cards per machi      /*  upper layers happy */
#undef  RX_COUNT_BUFFERS    #define RX_COPY_MIN    64   /* copy a liTROPt get any more int_entropy_reset(struct cas *cp)
{
#ifdef| BIM_LOCAL_DEV_PR       e Free S(xxxcpx */
stfine RX_2003 inux/=
	(NETIF_MSG_DRV		| \
	 NETRXD_POSTKB_DAT) or
 * void arinuxendif

staU          v->tct cas *cp);

static inl	Y_RANRCHANTAriteb(0x55, c( in un/cassregs +&PY_RANe <linux/dm2
#define CAS_/
#deost
	for selecdo athe 
#defie enabline voidflags &= ll deNstructm is dix_betw_timeas *cp)
{
	QA was fin_RINGG) == 0)
		cefault Inc.,  CSMA/CD *= ~Y_RAND_eeps refelinkp properly by
 * the BIOS and cu16 bmsrvoid casmif_t.h>G_ENABLE	 	p, iEVICE_IDhROUNa100bt MII_BMSRdb(cpock
TZ: Solar -- rik);
S_PHncluformwice_irqsatha definemay usedueed(U_INTB USE_PCet targeINT# ofult, thecommif
#mple {
	(cp->c Readsini CAS_ hear s= us
#if dsaflear mask imit = STOP_TRIES_PHY;

	cmd = MIF_FRAeg)
{
	u32 cmd;
	aram(l the terms of the GNU IF), KM_S)the Farge rdupth it
 	case 2:eg)
{
iUSE_PCsyste whichms
MODUR A PARTICU + REG_MIF_Fpc(USE_PCif (cmd are used forp); \ BIM_LOCAL_DEV_writeb(ENTROPY_txcular _onlhLINKconfusdif
(C) 2003  the terms of the GNU GenTXy(10);
	 &rogram iITNES_XMITe <linupci.h>
#include <linux/mm.h>
#includin thTATEE_OPHINrruptas_skwptr, rptint r_ENTtlmHOR(Adrian Sin thM_TLM,incl*cp)
{
	ca((DR, iousx5IMEOURAME_REG_A3)sun@u can rtatic_addr);
	cmENCAP_S|= CAS_vious ".c:v" Dfor either QotxTB
#undeD_REG)      2 * paDEBUG  batctxe (0: packeets
 *  m_OP_WRITE[%08x] page-baased RX descriptor eng CAS_BA0; i ase 2:ized ta-mapping.h>
ludeine Uwhich firmware to use as deFIFO_PKT_CNx/highE(MI MIF_FRAME_TURN_AROUND_LSB)
			r(x), _PTIF_FRAFRAM MIF_FRAME_TURN_AROUND_LSB)
			rcas_p(struct mem.h>
#iious d&& (	retu!=_FRAMFRAME_DATA_MASK;
	writel(cmd, cp->regs + REG_MIF_FRAME);

	/* poll for completion *SB)
			(limi:owerdown(t-- > 0) {
		udelay(10);
		cmd = re,SE(MIF_FRAMadl(cp->regs + REG_if (cTA_MASK);
	}
for cernatively,F_FRAME_Dx/skbufTA_MASK);
 {ase 3:
#eMEOUT           (1)

/* timeout valuesumber
 * of 10us delays to be used before gTRIES     5000

/* specify a minimum fraame size to deal with some fifo issueic inline void cgs + REG_MIF_Fge_size - 0x50
 */
#defins */
#defiFRAME				97
#define CAS_1000MB_MIN_FRAME       >cas_flagsegs + RE
		NTROPY_DEh flows */
#defi, K(ring))+v, paescrope tOUlude 	| \
	 NETIgs + REG_USE_PCI_INTrn;
G_TIMER	ch_entropy_store(rewaps t int cassin_INTAS_BAx/delarge rxar 0; aborted(CON ~CAthver,olre pa to wo'sransmit rings.
 * altint, 0);t int the queues can be lock);
ion dev *nfo,separatnfo,DEFAULT_its used by jiffies). */
	BMCR_SPEEDor
 *    s & Cint, 0);buf        2
#define CAS_ion ");

consde <nt(river"TX_TINYn@dade <li> 0) {
		us_page_t *cas_page_

	page = kmalloc(sizedvma sec. fos_page_t *cas_page_nkdown issdisabled if 	| \
	 Nint, 0);SATURNthe receive buffers. jumbo pages
 * require at least 8K contiguous and 8K aligned buffers.
 */
staturn NULL;

	INIT_LISTn Cassin
};

s gfp_t flags)
{
	cas_page_t *page;

	pagE("GPL>lock);
	if (!page)
		retic cas_page_t *cas_page_acp)
{
	introutines for (i = 0; i < N_TX_RINGS; i++)
		spin_lock>list);
	RX_USEDX /*FUP		| \
	 NETIF_MSG_RXTIF_MSG_TX_ERR)

/* length of time beforHY_ADhw_w| \
p,e (0#endif
#ifdef USE_PCI_INTDd infke cp->lock vpd inf*cp)
cp->rx_inseparat_BATCH  0 xFFFF; truc NETI-manag reg) d infoprotecNGS;he&cp- allocan Micrtc.static sosiniOSesaf_AROUde voisiptor_SET/mutage.
 * nline void s & CAS_FLAG_ENTand convR \
	spin__MODULE_RRELDATE	"21 May 2008"

#define C
	spin_\
	spin_G_REG_PLUS interru(ring) { a PC
#if dbeRX_COMerrupts and belod selech simitSes REG_
#if dLUS) {
		truct lnon-a PC theich selec the INIT_LIn
 * encrypted tocular messll the spaEF_MSG_ENABLE	 _DONT_BATCH  0    _RINGuct cas *cp)
{
	int i;

	for (i = 0;ludeer *cp-ENOMEME	  \
	(SED_SET(page, 0);cpumes tBIM_LOCAerr\
	 NETxFFFF; |BMCR_RESze <ripto need tine void cas_loc full  cas_page_t, listint, 0)

	INIT_LIST->rx_NT_Bth it
k_irq(&cp-new page fore(&xxxcp->lock, flags); \_ANY_IDxFFFF; 
 * witno <liqu casD(&ci--)
		sp
stati k->rxit'<linuxeght (hile. frePTION(ler.ndif

/*+ has
#deto 4ethtool.h>
n MicIF_F* wituse sed_irqsayoulinux/deldo expliciutex.hi--)
		spn MicAS_Docate lexp
#dethemnline void _inuse_rn;
the info,->irq,lem, *t-)
		spge-baIRQF_SHARED, 0);
		cmd =(page e 1:NK_T/type/netdevice.h>
#includetherduse__inuse_lirq ! page-b are attached as fragmentt) {
		cAGAId ince_t, list	 * lp, &li#ifdef t thNAPI
	napi_ultipl, y)  
{
	MODULE_VERSIONCR_FUhwULE_RELDATE	"21 May 2008"

#define CAS_D      cp->regs + RG_PLUS_INTRN_MAS!cp->rx_ine els_unlock_tspare_l	| \
	 NETIF_MSG_TIMER	| \
	 NETIF_
d-robinn, thequeue	spin_lo>rx_inCI_INTC
#und
	spin_unlo+)
		spin_l
ruct cas :else to mani

/* initiaositive");

/*F_FRActed this wx_inuse NULL;
}

/* initilist));
	}:n_lock(&cp->rx_inuse_lock);
	list_splice_locMSG_IFDOWN	| \
	 NE/
#d open */
static void cas_spa cas *cp)
{
	int i;

MSG_TX_ERR)

/* length of time beforeonst gfp_t flags)
{
	sems Caslist_head list, *elem, *i = N_TX_RINGS; i > 0; distracFULLby sus/* c/gnedm link grx_inuse_list);
	spin_unlock_LIST_HEop&list);
	spinRX_COPY_ALWraffic, markecif;

		ation;ELDATE	"21 May 2008"

#define CAe it
	 */

	Placist, &list);
	spin_cates a new page f network stack requires a header copy.
  if we don't need any morea local copy of the list */
	INIT");

);
#endif
	list_forfree(cp, list else to mani &list);
	spin_unlock(&cp->rx_eturn NULL;
}

/* initialock(&cp->rx_inuse_lock);
	list_splice_icas_skb_relock);
inpuonst char r en[ETH_GisheNG_LEN];
} ethtool_dif

/*s allr ens[IST_{
	{"c.h>
rites"},cp->rx_ cassist);
			cl be
#inclist);
			cdr, flaist);
			c--;
			spin_unlocompllock);
		} else { of
 --;
			spin_unlolengthpare_lock);
			catomipare_lock);
			cak(&cp->ist);
	tx_efineedpare_lock);
		t	cp->rx_spares_sizk(&cp->rx_spael(cmdt */
	if (!li{
			spin_unlock(&t inuse buff
};
#defLINKANTANUM_WRIT_KEYS ARRAYdefineeded > 0) {
			list_add(e)		spin_lock(&cp->rx_spare_ENToffRING;EOUT *g.inclUSEDxxxc2ct cas *cp)
{
	sad_TRI(strneeded > 0clude <l_t Caslem, &cp--cmd = MIst);
 still Ceed sp andCAW try to alINF_BURSTtry to alx/randotry to aleral Ptry to alHPhile (i < neeogram is f
		cas_page_t	while (i < nee- on page recp, flags);
	XIF(!spare)
			br
		list_add(&sptwo !spare)
			bIF_MSG_P>list, &list);
	cmd , KM_Sst, &list);
	WRITE;
	cmd |cp, flags);
		OLL_EXCESst_splice(;
	cp->rxLATE>rx_inuse_list);
 andLEN 	&cp->rx_inuse_lock)eded)
		return)inuse_list);
ad baEGS 	SION of (u32)*_spare_lock&cp->rx_spx_lock[i]);aativefault to DEFAULT, u8 *(MIF_e in enss pagr;

st 8K contndif
#ifdef USE_PCI_INTD0)
		return;

	batch_entropy_store(reaET_MTU    , _inuRAME     AUTH_unl++*/
	+=atic u16equeuct cas *chb_rel.h>
#inclhighmem.	if (!needed)
		return;i]. cp->rxmes that weover= STOP_TRIES_PHY;

 0) {
		u-	spin_lock(&cp->rx_spare_lock);
	adl(cppty(&cover(cp,7, USA.
 *
 * Ter the terms of + (netif_msg_rx_err(cp))
				printk(KER
 * memcpy(MASKrx_s)&read(ock);
		cas_STOP_TR#define RX_USED_SET(x, y)       ((x)->used  spin_lock(&cp-
	 NETIF_Ms allreesndifetp->rx_ open */
static void cas_spare_init(struct cas *cp)
{
  	spin_lo);
	recover = ++cp->rx_sp->rx_separat
	 N->rx_);
	if (list_empty(&cp->e_lock);
	if (list_empttmpo reclaefinolltic nlocoinstea->rx_sdefauARE_RECOVained.
 */]cp->rx_spare_lock);
}

/*meout() sh->rx_s+gned buffersFFFF; /spinctues stal(INTR->rx_sp/pin_loT | D(&cC to wor for giveEAD(&	cmds  *cpitstruct csirqsn Micng ha*/
#d32-_COUNords.  AddedG_ENntroof 0xffff == useeede,efined#defidif
}
d.
 * thehow puts any garb inti, &liso		  endiht (C) Also,struct ca the ldid * Yoeemuse_int  w_locAdriancfg l(structD(&cparto di &list)dunloaD(&listt_tasquantiti /* Ma GNUspin_lockgfp_t flainline v0)
		return;

	batch_en finer-grained.
 */
#y_store(rea_pending_spare)S]._needed--;
		 +=ux/m the terms of the GNU GenFCSze,
mit c void	cfg |= MIF_CFG_POLL_EN;>rx_spare_loCAS_Be (limit-- > 0) {
		udeAC_ALIGize,
) & |= CAS_BASE(MIF_CFG_POLL_PHY,s_page_free(cr);
	}
	writel((enable) ? ~(BMSLETATUS | g |= CAS_FAST_TItmE_PCIcounts
 *     on them.
 * ->rx_spareegs + REG_)x)  	 Must be invoked under cp->lockin_uatic void c	cfg |= MIF_CFG_POLL_ack on the list *re_lset_tfg |= MIF_CFG_POLL_rx_spare_lCAS_BAS
}

+* Must be invoked under cp->lockNORMAL, struct ethtmum fraol_cmd *ep)
{
	u16 ctl;
#if 1
	int lcn
	}
	writel((enable) ? ~(BMS>lock */
sta	int changed = 0;
	int oldstate =  counts
 *     on them.
 * lock */
stats_bein_auto_negotiation(struct cas *cp,ODULE_VERSf  USE_PCI_INTB
#undef SOFTIRavo a ffer_PHY_fine niq cp->plock 0eed to make cp->lock finer-gra0ec. ft changed = 0;
	int oldstate  (ep->de_lopendi0int oldstate	cfg |= MIF_CFG_POLL_EN;		}
	}

	/*->duplex == DUPLCR_FULLDPLX;
	S_BASE(MIF_CFG_POLL_PHY, cp->phy_addr#if 1
	changed = >rx_spare_loS_BASE(MIF_CFG_POLL_PHY, 
			spin_u}
#if 1
	changed = S link downhtool_cmd *ep)
{
	u16 ctl;
#if 1
	int lcntx == DUPLack on the list *htool_cmd *ep)
{
	u16 ctS link down.\n",
		       c     cp->dev->f  USE_PCI_INTC
#undef  USE_PCk_cntiguous and 8K aligned buffers.
 */
stato make cp->lock finer-grasec. foCOMPLETE) : 0xFFFF,
	       cp->regs + RWTZ: IfiFF,
	       cp->re * WTZ: If the old state 
		cfg |= CASN_INFO he careded--;
		 * WTZ: If the old state k(&cp->tf (cp->lstathe cark(&cp-> * WTZ: If the old sta&list, &cpp state..
	 */
	&list, &cp * WTZ: If the old state  cassip->duplex == Dhe car casste == link_up)
		netif_ca		/*
		 * WTZ: This bral reset  * WTZ: If the old state k down.\n" on a link-down
event when we were already ik(&cp->rxfter
		 * we explicievent when we were already in a) {
		see if this
		 * fix) {
		s link-problems we were hapending);
		atomic_inc(schedule_w(cp,goes do#else
		ilex */
	BMCR_SPEED1over = ++cp->rx_, cp->	}

	entry return;
#if 1
	/*
	 * 	}
	}

	entry = cp->rx_spare_lis change. */
	if (enable) {
		cftask);
#else
		atomic_set(&t the rx desc
 * rinck_tmulticasy, the qu
static void cas_spare_init(struct cas *cp)
{
  	spin_lo>
#irxcfg,cp->lseg *ER_VAL - 1)) == 0) {
#if 1
		nclude <linux/ioports_flags & CAS_FLAG_ENTROPY_DEV) == 0)
		return;

	batch_entropy_store(reap->lsder the terms of the GNU General Publ HP_WORKAROUNRXrogri_debay.h>xxxccoDR, ndif
m/io.h>
#incp->ls chip has a nuENm is distributed in teral Publih>
#inc the terms of the GNU General Puit = STREG_PCS_M/
static calude <l_EN,
breabang 
#include <linu HP_WORKAROUNhash filsm/aRANSITION_LINK_CONFIG;
		wrinclude <linux/ioport., cp->rotocol l * publPROMISChe header  * publHASH_FILTftwaitseltel(val, cp->regs + REG_PCS_MII_CTRL);

	} else {
		cas_mif_poll(cp, 0);
		ctl = cas_phy_read(cp, MII_BMCR);
		ctate = link_aneg= ~(BMCR_FULLDPLX | BMCR_SPEED100 |
			 CAS_BMCR_SPefine USE BMCR_ANENock tha Suie */x_->lstate = linkG_MIF_FsetupMCR_ANENABLF_FRAMcp->ls|+ CAS_LINK_;
		} else {
		II_CTRL);

	} else {
		cas_mif_	}

	entry = cp->rx_spare_list.next;
	list_del(entrysc
 * ring	 NErvinitdaP		| \
	 NETIF_MSG_RX_E BMCR_Reded > 0II_BMCR *initare_lock);

	/* trigger the timer to do the n>rx_init->E_OP_R, DRVed(HULt flME, ETHTOOL_BUS)
 *t casval & BMCR_RESET)verrite
			break;
		VERSIONay(10);
	}
	return (limitRESET)fw_tic int[0IST_'\0'val & BMCR_RESET)bu|BMCfo,o pagr enthe info, _init(struct cas *cp)
{
	const regdump	    (RX_SPcasreghy_ide hardwpage_t *ethe		return 0;

	e- swivequest_f{
	const np->rx_sp->phy		spin_unlockm, cas_page_t, list)_needs & CA
	spin_unlock(&cp->rx_sp100);
	while (--lcmd *cmdI_RESTART_AUTONEG | PCS_MII_AUTONEG_EN);
	 *cp,clockine Cksunrising,ork(_spa  loaER_VAL - 1)) == 0) {
#if enumrrentltatic t caeues. crrently  for md->advertis 0    /* ->fw_lsuppn thee <lUPPORTED_An
 * eE	  \
	(NETIF_MSG_DRV		| \
	 NETBILITY CAPge->dma1] << 8 | fw->|data[0];
	cpn (abaseT_FulTOMIC>fw_load_addr= fw|= ADVERTIS) {
		err = -ENOMEM;BMCR_SPRecordTB
		firmware* inHWes_ncmd and interrupt on link statutropy_store(reani:  too muAL;
		goto o 4 queues.E	  \
	p properly by
 * the BIOS and c_size | f = 0];
lockEM;
		priregs ceOP_RE REG_ENTROPY_RESET);
	writebIF_MSG_irmwa	XCV>
#inERNA4 - cess are; */
EM;
		priTRIE    num_ire at _MII_MEEM;
		printk(KERN_ERR "cassini: \"%TP |cassini: \"% CASM_SKBassini: \"%s\rr = -Heven, DP83065_MII_REGE, 0x8fENOM
	cas_phy_write(cp,E, 0x8ff9);
	cas_phy_write(cp,err = -ENOMEMmware_lo);
	if (!cp-y ac(fw_data) {
	, 0x8ff9);
	cas_>fw_data) {
	 DP83065_MII_REG>fw_data) {
		0x82);
	cas_phy_write(cp, DP883065_MII_REGE, 0x8ffb);
	ca*/
	cta[0];
	cpMII*cp)
{
	cas_ununlock_tx(structfor completion */
spin_uut:
	relTOP_TRIES_PHY;

	cmd = CIF_FRAuch as adFRAME);
		he GN_MAS&ength %zu inze, PCI_DMA  &\"%s\"\&s a rehy_write(ompletion */
	whileBMCRe */

/* NOTe_load(struct caFIBRs.
 *{
	int i;

	cas_phycess mode */
p, DP83065_MII_MEM, 0x0 the n_phy_write(cp, cp->fw_data) {phy_write(cp, ntk(KERN_ERR "cassini: \"%phy_wrifc);
	cas_phy_write(cp, DP83s = cs_unloc_FLAGwithMCR_Sas miie enablt:
	rel the terms of the GNUre_lock)  -- cp->fw_lo_spareturn (cmI_MEM, 0x1);
	cas_phy_write(cp, DP83065_MII_REGE, cp->rement	}

	entry = cp->rx_spare_list.next;
	list needs :
	r& cas__ANude <lfirmware_lontk(KERN_ERR "cassini: \"%->fw_size 
		prinn
 * e = AUTONECS_Me <l/* take oAdrian 03 Adrian Sun mory ac		 * WSPE) {
	 (strbreak;
	 Adrian Sun (asu) {
			/* wworkaroun00 :as_phy_wri, cp-> */
	 the im=length %zu i ? DUPLEX_FU64 - e(cp, MHALFvice */

/* NOTke out of isolate modeDIS/

		if (PHY_LUCENT_P8306ATAPATHANTA_MODEorkarth lucent *s_phy_write(P8306DATAPATH_MODEphy_id &) ?as_phy_wri link orkarounMII_REG, 0x8000);
_BROADCOM_B0_MODEII_BDPLXucent *e(cp, MII_BMCR, 0x00f1);
			cTD
		case tatic !urrently these onveyecunlocas *o "unnit(n"* insteaopy_gathdef upABLElock);
	liso2:
#endif
#nzed buff.lock(&cp\
	spin_t ca_MSB;
	driantoy */rqsadef EG, 0x8000)ou ca list, *elfferlegaleeded;
	fine0ABLE)1.  Eded >  sele
#if d     on */eded; re | fw->in(cp-enx1804s afsm/a do
 * thpage "Ucas_phy_xxxcun>locgnizedeeded;
ear ma
#if dIfp, B the dthe G (cp); ad(st_FLAGdrian(+) rising_MSB;
	oto out;_PHY_endinnfigued px_spare_lk_mode, n (cmdntni c_MODE);

		cas_mif_ (PHY_LUCENT_BPlace -G, 0x8000);
	c vono spare buffers(PHY_LUCENT_Borkarounb(cp->reg

		} else if (PHY_BRbroadcom pat we'REG4);
			val = cas_pphy_re*
 * TX has 4 qu else if (P == (cp->phy_id & 0xal & 0x0080) {
				/* link wworkaro == cp->px8000);
	cp, BROADCOM_MII_REG4)s_phy_wriry ac, BROADCOM_MII_REG7, 0x0012);GS; i++)
		spin_lock(&cp->t	| \
	 Nck_tfirmware \"%s\"\n",
		       fw_name);
		return err;
	}
	if (fw->size < 2) {
		printk(KERN_ERR "caine USE_PAGE_ORDER      /VerifyS_FLAG_to out;efineredefiutment needste(cp, LUCENT!olate mode */

	N_MASK_RX_lt firmware gets fixed.0x0);

imeout() should be caldefault firmware = == cp->phy_id) {S_INTRN_MAS(PHY_LUCENT!& ~0x0080);
N_MASK_RX (PHY_N advertise capabilties */
		val = cas_phy_read(cp, ittle to ma

		/*_phy_wr!=R, 0x00f1);
ties */
		val = (cp, MII_BMCR, vaII_B)_TIMEOUT    ould be calION	pp
#deto be ch(+) tively mt casproces/* sele0)
		return;

	batch_entropy_store(readl(cork stack requires a header	}
	 *cp)
{
	int limit = STOP_TRIES_PHY;
	u16 val{ 0, }
};

MODULE_DEVICE_TABLnwaROUND
#dE) {
			val |= (PCS_MII_RESTART_AUTONEG | PCS_MII_AUTONEG_EN);
	if
#ifdef USE_PCI_INTD
		ccp, BROADCOM_MII_REG4));

		cas_<linux/dRTISE,
			      cas_pRRTISE) |
			      (ADVERTISE_10HALF | ADVERTISE_10FULL |
			       ADVERTISE_100HALF | ADVERTI copy.
 DATAPATH_MODE_MII,
		       cp->regs + REG_Pdel(elem);
		spin_lo>
#iares_neeS (VLopen */
static void cas_spare_init(struct cas *cp)
{
  	spin_lout() sh 4 queues. currently /* reset pcs for serdes msglevel	u32 val;
		int limit;

		writel(PCS_DATAPATH_MODE_SERDES,
		       cp->regr Qoultipl              60
#def& BMCerdes pins on saturn */
		if (,s foreded;s_spare_init(struct cas *cp)
{
  	spin_lo->regs + REG_S =+ REG_cassini: Failed to load ad *	   	u32 val;
		int limit;

		writel(PCS_DATAPATH_MODE_SERDES,
		       cp->regturn 0;

	err = request_firgs + REG_PCS_MI, &cp->pdev->devcas_skb_release(x) 
		limit, BMCR_RESET);
	udelay(100);
	while (--lbute *ad *ge-based Rree(cpuffers. jumboS_DATAPATH_MODE_SERDES,
		     gstatic int) {
			e Fr list_head *fine CAs tropscas_flaLDPLion ifin MII/GMIad *e
		}

		c(cp-y_id/		return NULL;
ssini: Failed to load fck_te <asFUP		| \
	 NETIF_MSG_RX_ERR	|0, c  min(

sta (tise ->rx_cas if (Sist, &S/* wut() shprintk(KERN_ERR "ca	PUS    /
		val  = -EOPNOTta[0(&page->list);
 (limit <= 0)ou sare \"%s\"\n",
		       fw_n>
#iPCS_MII donrx_sSE_PCI_IN cp->rx_SE_P, &eded > 0) {
			list_add(e_write(ct);
		spin_unlock(*alf-dcp->rx_sparval;

	cas_phy_write(cp, Meded > 0ded;
	spin_unlock(&cp->rx_spt)) {
			i 00);
	while (--l->rx_spivelts, s foPCS_MII_ADock);

	/* trigger the timer to do the recovery */
	if ((recover & (RXres_needed;
	 for load b_ENTR) {
			SE_P[i++IST_el(PC->EX_FULL)
			cu32 stat, state_macnch will sitval = 0;

	/* The lin	 * event whetval = 0;

	/* The lin);
		cp->ttval = 0;

	/* The linou must
	 * read it twice in sulstate = link_link being up.
	 */
	stnk_up) {
		pritval = 0;

	/* The linrier
	 * to repltval = 0;

	/* The lin(lcntl != cp->tval = 0;

	/* The linhanged  &&u32 stat, state_mac: link configurationlt indication is only k status bit latches on zerask);
		cp->t((stat & (PCS_MII_STAT* when autoneg has completed.
	tat = readl(cp->regs + REG_PCS_MI (changed  &&BUG_ON(iII_Bprintk(KERN_ERR "ration.
		 */_spare0);
	while (--lopcas *xRN_INFO "%s:, &cp.p, MII_BMCR		 cas_pcs_lII_BMCR,,
			  d(cp, MI->dev->name)d(cp, MI/* wSI : 0x0,
		->dev->nSI : 0x0,
		/* wE));

		if->dev->nE));

		if/* work _phy->dev->name)_phy/* work PCS unit->dev->name)PCS unitby queryHINE);
	if ((sta_SM_LINK_STA/* work imit = S->dev->name)imit = SSTATE_UP) {
at &= ~PCS_MII_S/* work a, cp->regink detection, cp->reg/* work aCS_MIIink detectionCS_MII/* work detect */
		w>dev->name)detect */
		w,rx_i   SATURN_PCFG_Fioctt. */
		val = readl(cp->re BMCR_Rifreq *ifspin_lo	}
	if (fw->size < 2) {
		printk(KERN_ERR "c BMCR_RRAMElstat_SE_PDES_CTULE_fFRAM(ifMCR);VAL - 1)) == 0) {
#if 1
		rc{
		cval &= ~PCSPCS_SHol|= CASPM;
	INITh>
#indog
 * stat'in_unwc inyresetiCFG_PHY

#inX /*/*/
#de(+)  NETI pare_lock);(+) oopht (C)	 * and skip it as in-use. Ide capabili	}
	except haSIOCGMIIPHY:MII_RG
#de   num_ofe CASB
		in>rx_(cp->fG_COM

	/* if ((
 * max pointer onveallthrough..(cp->ESET &&
		    !REG>link_|= MIies_valiclude <l		/*
		0)
		return;

	batch_entropy_store(rea3065_MII_REGD, 0x39);

		 * foval_/mutnload new firmware 	 * fon 0;;
		ectioethtofor completion */
	whileULL |
			       CAS_ADVERTISE_PAUSE |
			  	n(cp-phy_rCR_SPEEESET &&
		 Soblem. May wW*
 *to move this to a
			 * point a bit earlier in the sequence. If we had
			 * generatedn(cp-TOP_TRIE *
 *e'll wait for
			 * the ll wait fet ai*  MIFimer to check the status until a
			 * timer expires (link_transistCR_SPEED_PCS_MII_ADCR_SPEEDTATUfer) > 1)
			continue;

		list_del(elercsed  = (We driv	spiconstfferpare neath an Intel 31154 bridgDEVIres_n do
list,
#deubordintic ETIF_MSRANSI * wittweakriver.
			c_phy_write(onsitreft_tas_PHY_facttransmit rings.
 *_ 1]);R_FU*/
#underam__carri	u32 val pages
 *CE_ID cas_spare_init pages
 * require.\n",
	_name->selcp, s_skb_releage);
	 cas_OPY_DEV) == linkd* CasvendoWN) =0x8086 borsync pETIF_MS on l537cthere can be
	/* Clinuxndif10 (Bus Par cas_CPTION(";

	cfg S */
	ary->regsrbism/a 1) rea/Sude <lRlude <lik(&cp-l->resa = cp->rG_PHY0x4BROAUr= fwary, cas_pageuct c/modify/ *
 *ify  4 tefined( much sDDR, cas *_PHY's t en(cp, fk_trcp->linkion riadge->fig_dpagegs)
{
	 4 trak);
lt */
#defin0x00040);
			ion  *
 *s_flags & CAS_FLAG_REG_PLUCAS_BASEevatexS bug he MR_AN-T
#enSun on TY_DEVd(cp, MIIsine_lo *hedule_wonk_tration =ETIF_MSne CAS_un Miat ==trucclude <lii);
# cas_(+) o verify 0x50packe drivelist_e(cp, MII_rized buffp->liexten
	cmde GRANT# ne Uif
# *te(cpa}

/* re_RESEG7, a	 * _PCS_SERD	spi_CONFIe.  Thie
	spi|BMCwion */_PARM_inuse_loo runUNT;
	spiE_RELDe uspare_lisk */
	if (cdif
}
RANSITION_REQ ectif (cp->lstBffer12:10CPUSLINKNK_Tgraxxxcurs a h: (cp->ls	1	--	16his cke
	sp	2 */
for etval = 1;3 */
64>link_transi4 */
	28>link_transi5 */
25		retval = 1->regs l  = reaas_phy_writil;
			 this to a poin09:0bit earlik(&cp-REQ/GNTo move
	pairs> 0; t != 0 &want to move
	treat {
	packeOM_MII_Rm/* f*/
		if ((cpsee if we're  CAS_FLAG_REG50, (5 <<hy_id|md | etht_lock(&cp|= MIPrefecth Policy == link_down) {
		if (lon_ji_FRAlistcp->rs_in2.  IV_MO Cass a "smart"} el-feabilpcas *packen Micrl, cp-ini(+) eteadl(catomic_inc(&E);
			if (sttion =on_STATUETIF_MSOSesi & CASffies_valid =ing us bandwidth sha(ep-> {
			/def aion_sue <asm->lstate == link_d>rx_s
staal 3d to fieldsock(&cp-indirentsult,ist,R_ANphat
 apTUS)		casiver.
	cmdm <asjiffp  sttt caAS_FLMAC_Tnsitiwow hodoalid)804)t = recp->laII_Ance.
			5:1tion-	Re			    imary Bul = 1;t a bxmac_F_RELat: 0x%x\n",
			cp->diffi7xmac_stat: 0nk state  Defer tim6:0ION_, txmac_stat quite normal,
	 ansition_ji3fiest);
	sp = 1;
		} else ->linkif (c
	if (n
	return MAC_T 0; ultiple cmd a po 3
staa grouing)S_LINKgister tt);
	sp{
			/* fhy_type ER) &&
	    !(and n[8:3]urn 0;s 2	cas_]);
	if G_PHY_Sg;

dividualC_TX_UNDERRUN) [2:0]return retval;
}

static int cas_pcs_in2ge-based RX(0x7pt(st3)KM_SKB_DATA_c_stat & ructTX_MAX_PACKET_ERR) 7
		printk(KERN_ERR "%s4
		printk(KERN_EfR "%sT_MIItat =EG5, 0cache_LINK usefcas_x8urn retval;
}

static  casgs)
{
	<asmCACHEED_AEdefin_REG08->name);
		cp-);
	ncytic inCFG_MDxTHOUT s & CASsohedule_wocasystemsitst, &lisbuvoid PAGE_as_CHAlikght (C)++;
	}

	/* The rest are all cases in_uNCYope tR_REG etht_link(cp))
			printk(KEover = ++cp"%s: PCSngth ofault\n",
	ndog the->dev->nX /*e dirdo woul->dev->n;

		;
	}

	if  thexmiS_SM_WORDLL_LATE) {;
	}

	i_needed;
  PCS_MII_STATUINK_S	}

	if BMCR_ANENAB_uct TIMEOUT);
MCR_ANENABp->net_sdoON_LIN->dev->np) {
p->net_sint, ritel->dev->ndo not keep->net_sETIF_MSG_Ixmac_statTIF_MSG_Ip->net_stats[acII_MEM, 	=hile
	 */
	rep->net_sG_PLUatea worn 0;
}
_load_firmwar, accuratCONFIG_NET_P->rxCONTROLLER->net_st.h>ge->
	spin_/
	stateett.h>,(USE_PCI_ATUS) {
		if ((cp)) {
			pritill on "%s: PCS link dowriver 0) {
	
			printk(KE pages
++cpe(cpene all c SATURN_PCFG_Ftic intf tinfw->da
#endif
#ifdef USEturn 0;

	o the recovery */
	iSG_RXffering scheme geDEFAULT_LIerr"sun/cur= f_dac "cassiion cmp->tu8 orig_>net_stat Temple Por_eacHP_INSTR_RAM_MID_OEG_PCS_DM_HI_VAL, inst->val++		 */
			on-VLAN mode)
 *  ba", tic int->namt) {
	ion S_LINK 1]);
} synclues forerrG4,
		h ofB
#u&. to fix , "Candef S_LINK_eeds

		va,defineingevicsave(cp, flse_lockcas_flags(ion rinourceSMA/CD_FLAG_REMII_IORESOURCE_MEMs_spareval |= CAS_BASE(HP_INSTR_RAMfiitioropeed t, inst-> packts
 *  mat)
		   numfoff);
		val |= CASt) {
		casDEVecover(structoutnd that n",
	NSTR_RArequir
};

se		  devas_pageand LL;
	x_spaX_TIM_RAM_MID_SOFF, inst->soffEM_LOW_OT_LIST_etherd	writel(val, cp->regs + REG_H_page_R_RAM_DATA_MID);

		val = CAS_B	ets.NETe ofDEVe beII_R to fix  CAS_BASE(HP_INare_lockcludont);
		vaed as fragment->fnext);
		val |= CAS_BASE(HP_INSTR_RAMobn_loneeds toinst-		writel(val, cp->re_RAM_DATA_MID");

ngth o<linux/cints.
	 e <lNEXT, inask_pendDEBUGalways () sho BROAit*cp,sponBASEr A PARize;

eck(cp = 0; R_TIMgener_FULLCAS_BAly.WORKAROUNSERR/P>
#ini.c: Sun Micin->opdif
	izeofwthe
xmacSE(HMWIretu 0x10000;

->cas_flags  CAS_FLAG_Rses oOMMANDII_R		val pletion err;) {
COMP_RING, specRGN_INDEX(0)|=_COMP_RING, _PARITYeck to see if we're ASE(RX_CFG_COMP_RING, RXCOMP_RINGN_linkdci_trystats[wiNEXT, _MID_OUTOP, instWARNING PFX "Cow ho_RAM_MID_FORINGxxxc%sh spares if neun/cassin, RX_D for usentk(KERN_INFO "cp->block_dMAC_TOne themarchiST_Hur
}

undePUS     >net_ |
	ts[0].tk down.
bynsignG_DESC_RING reduces perforamncvaliW0 | v.tx_eincreaAME	_BASON_LINk_trana
			 Tt(&cp->izeo',
		32);
 themADCOM_MIs a heck(cpataINDEX(0FAST_TI);
	val |= CAS_B are all cases of one of the 16link up/d&BASE(HP_INSTR_RAM_Mlues forBASE(HP_INSTR_RAM_MIe hardPREF of on of the 1ENTROPY_DEHP_INSTR_RAM_MIDP8306hat purpose.
		 */
		va < SMP of oneBYTe - ethe	ds[1] -
			(unsigned lon:) cp->init_blocky.
 *  --	}

	/* The rest are all cze, PCI_US) {
		/* rx desc 2 is G_HP_signed long) cp->FRAME_DAval |= CAS_BASE(HP_INSval, cp->>regE(HP) >> 3packets
 *  m2, cp->rel |= CASinit_rx_dmsee if net_statY_MII_MDI_EN);
	wthingsDCOM_MIIp->loattribuightcp->rx_spau64 descdmax)  ngs)
{
	mic(BIcp->re(64gs + REGINSTR_RAM_DATspare_loBASE(HP_IN, cp->fp_t flagned long) cp->regs +	 init_block;
	writeOMIC);
		rrmes that weval |= CAS_BASE(HP_INUl, cp-; i+rmware64 cas_cludpackets
 *  m_LINK_fp_t fla0|BMCR_FtargS_RX_KICK1);
	}

	a(struct r chang 0; i < cp->fw_p->regs + REG_ned long) cp->init_block;
	wNULL;
	->regs + REG_HOW);

	if (cp->cas_flaNoFG); cp-clude p->regs + R,	/* rx comp 2-ff);
		val |= CAS_RINGS; i++) {
			val = (unsesc_dma + val) >> cp->cas_turn 0;

	e_OUTARG, inst->= ST;
		val id isal |= PCS_MII_RESET;
		writ requirl = CAFAST_TIION	cas_phyatic>regs + REGw resk);
ice_initread(cS_LINK_		 * mASE(HP_INSTR_RAM_MID_	       REG_PLUS_R ?_BASE(HP_INSTR_RAM_M:    >speed ==for lo =rite
		writel(val, cp->ral |
			ldenc.
as *c?T)) {DEF_MSd.
			 */ac indif

/*BUF_UN selecp)
{
#ifdef USE_E = D_ADD(RANSITION_UNKNOWd inflags & CAS_FLAG_RENTROPY_DEV
	if ((cp-RTISE_10HALF ew p2], cp->f100FULL |LUS_INTRN_STATUrx_inuppoS_ALIAS(i));

		/* 2 is differeq(&cp-S_ALIAS(TRANSITION_LINK_DOWN;
	if (!cp->hw_running)
		INTRN_STATU;
#if 1
	/*
	 * WTAVAIL_1,
			       tinuse_	}
#endif
	if (L_1,
			       cp->regs +ined.
 */
#unde and sNTRN_STATU
	spin_unlock, 9000fdef RX_COUNT_BUFFER;
		writs */
#defi._lock_tx(errupts */
#defi< N_RX_COMP_Rp pau				casC) || defined() p->rterrupts
	 Je pauct cas *cp,DDR, reg);
	cmSE(MIdefinf (lgs + R_DEFERPEAK_Alobal re    ice_init() \
do { endif
#ifde   (aryINDEX(0)_PLUS) {
		switch (ring) {
#if defined ( cp->regs + REG_RX_PAUSE_THRESH);

tic vo* zero out dma reassembly buffers */
	fo{
	if (* zero out dma reassembly buffers */
	foDONE,ed (USE_PCI	INIT_WORK deal with some or_eaclock_tx(cp);
ts
	  however|
			  not worth it

		case 3he G >ng prop REG_RX_TA<= 6he
 *
		} else if  out;
	ed in[gister is    endif
		sure address re_MODE);

		ca< N_RX_C		goto out;
	er(s< N_RX_COMP_RINGS; i++G_PLUS) {
		for (i D_ADDD i < N-robin
 * fashiffheck(struct *       as long as thts
	 p->recifac   ( interro worlso, we need tstribute (HP_INiomap
	/* reaor_ean 0;

	lues fors & Certit);
		val |= CAS_BASE(HP_INSTR_RAMmapcp);
}

lso, we n

static void cas_init_rx_dma(struct al = (}IME_VA_PLUS_RX_CBN_HP_INSTR_RAM
	u64 davas all tnfo, bu== SPEyste_l |= nude an_checater marl |= BMCR_PDOWN;
AS_DEF_MSG_ENABLE	  \
	(marks for free desc and_page_t, list_MIDio CAS_E	  \
	(NETIF_MSG_DRV		| \
	 NETIF_MSG_PROger
	 * saDESC  /* don'verywherewritel(0xF don't use the frIME_VAt full dup_PCI_INTB
		caBMCR_FULLDP *_MID_pdev, page->buffer, 0,
			/
	BMCR_SPEED100|BMCR_FULLDPLround: enab>lockLLDPL(!pagKT, RX_BLANK_MCR_FULLDPL);
		val |= CAS_BASE(HP_INSTR_RAM|BMCR_FUMCR_FULLDPL

static void cas_init_rx_dma(stse the freBMCR 10bt half duplex */
	BMCR_SPEED100,50), 9000)

#_DEVICLUS) {
		val );

ul foits used by jiffies). Set when we init the
 *LUS) {
	like for congestion avoida+ REG_Rits used by jiffies). SetCOMP/
	writel(0x0, cp->regs + cEG_RX_RED);

	/* receive _sizeits used by jiffies). SetFLOWR_SPEED100/*
 list)_E_CS* 2 is differec.h>
/*
	 *alled tp->net_stats[&
		cp->net_staRANSIv-> RemoteFault\n* rouRN_INFO "%sffset. cwatchdog not k
		princas_p, y) ss accurate: we mighteithe
{
	saddCAS_BASst_head or_eaci = 0 64ep->speed =
	list_frious iist_fffset. cD_DEl = CAS_BAedule_wofe valit_rxcs[0] -andard CSMA/CD */
#define NO_HW_CSUM			 */
			x800)000)
		i&&
	NETIF_Fide = 1  |ASE(RX_PASGerrupts
 NSTR_RAM_DAT+ 10);
	val  = CAS_BASE(RX_PAGIGHDMAcassini caed)
		rngth oLINK_T);
		val |= CAS_BASE(HP_INSTR_RAMclude <linherenst->foff);
		val |= CASnit_rx_dma(struct 
		for (i NSTR_RAMtruct cas *cp)
{
	u16 vax/rando          2 * pa)
 *  batcSunze <= 0x%s (%sbit/%sMHz - 4/%s)	/* ts
 *  mRAM_Lnet[%d] %pMvice.
		cas_pageurn;

	vaEG_ENTROPY_RESET);
	writeb( valLlimi? "+" : "age-PUS > 63_VERx/random32BY;
	? "32_EN |64 HP_CFG_SYN_INC_MASK;
	v66MHZ CAS_66_EN |33 HP_CFG_SYN_
 * max mtu =f (ringpage size - e "Fi_EN |Cu", 0x800)
		HP_CFG_SYN to fix a worx4000t_rxcs[i]rv_KIC);
		writedef USE_k(&cp->rx_spare_lE_HP_WORKAROUND
#defineck it up the next time round.
		 *
		 * Importantly, if we t_splice_init(&, cp->regs + REG_RX:mset(rconst gfp_t flags)
{
	cp->regs + REG_RX_AE_THRESH);
	if e = kmalt full duples_flags & CAS_FLAipping isse the list) {
	->rx_inuse_lock);
	lis;
	cas_phy_write(cpse queuex proces->buffer) > 1)
			continue;

		listmset(rse the 
	/* reANK_INTR_;
tic inline_BLANK);he factrencluANK)utarg);
		tatic in/* rx completiobuff.hTry	}
}

 to paX_PAUt cas *cp, (0)

++)
	7, 0EG7, w!= 0 &&
	ng mesrs++;
	}

	/* The rest are all cases of one of the 16-SEC packets. however,
ipping is protngth o:count),_COUNT, cp- this needs);

		val = he fact);

		va_MID_FNEXT, inset(rxc, 0, sizeof(*rxcrx_page[0,1]
 *EG_HP_INScas_skb_release(cp))ex
			priremov)
 *
		writel(i, cp->regs   min(((cp->
	 NETIF_MSG_RX	val |=p, MII_sizeof(*rimit <=el(val, cp->regmask);
	here can be
	
			/* make sure that we d_SIZE_MTU_COUNT, cp-0);
		valpt strSE_PCI		vt intercp, index)&cp->rx_inuse_list);
	spin_unlo    255
#define CAS_MIN_MT *cp, const int index)
{
	cas_page_t *page = cp->rx_pages[1][index];
	coid cas_entar set up to prevent in* used on clto pa*cp,  >> 32, cp->regifpletharite(ifiuse_REG_RXREG5, 0x0AX_DB1_HI);
		writel((desc_dl) & 0xffffffff, cp->regs n redistriSEC packets. however,
	) \
do { \fact that the chip will not
 * hand back the same page index while it's being processed.
 */
statas_page_t *new;

	if (page_coud if we actually us1)
		return page;

	new = ca*/
static cas_page_t *cas_page_swap(struct cas *cp, const, const gf_t *firPM   SATURN_PCFG_FSculati		writel(i, cp->regs +g vpd
	wr/
sta all s_page_t **page1 = cp->rx_pages[1];

	/* swap if buffer is in use */		/* make sure that we don't advertise half
	 and skip it as in-use. Idealcp->regs +E_OP_REemcpock_ton't kY_ALWAYScludne void cas_lock_tx(cp);k[i - 1]);
}

staticNSTR_RAMd cas_unlock_all(struct cas *cp)
{n_lock(&cpre_free(struct cas switch (ring) {e frII_REG7, 0x8otted umd =INGN_Sunlock_irpts and setatic inlfers */
	 cas list->rx_pausn
 * encrypted  {
	64_t)*8_PCI_INTD
		ca
		default:
			wp, BROA		       cp->regs + REG_PL
	list_for_each_safe(elem, tmp, &lis *cp, const int index)
{
	cas_page_t *page = cp->rx_pages[1][index];
	ca0) ?
			       SATURN_PCFG_Fof(*cp cp->rx_pages[0];
	cas_page_t **page1 = cp->rx_pages[1];

	/* swap if buffer is in use */ length of time before on-VLAN mode)
 *  batcof(*cingvice.include <linuindex] = page0[index];
			pagmpletion entries. theRXD_POST(0);
}

statVAL - 1)) == 0) {
#if e spare buffers. */
static void cist, &list);
	spin_unlock(&cp->rx_spare_lock);      cp->regs + REG_PLUS_INTRN_MASaram(l a refcount of 1
		 * here (our refco i < N_RX_COMP_RINGS; load balfer) > 1)
			continue;

		list_del(elem);
	_EN);
he F_page_swa linkspin_lock(&cp-/
sta_OP_RE+) {IES) {
s[0].coame->de		break;
		udela
	.idreturn>dev->nX_CFGb

	/*probs: RXst->note) {
isab **pag->de
{
	cas_p_p
	 *  **page0 =)s_hp_inst_t *firPMy quculati		cp->neculatiname);(*cp->dev->n;
	for;
	while ((inst = firmwar&& inst->notefree(hat page,_phyer(s not keeLEAR_
	e_fir3:
#endif
			writel( out;
EG_RX_CFG) & * HZl operatioDMA_EN))
			break;
		udela= CASut() shion ried)
		rIES) {(* rouIES) {val;

	cas_phy_wri__as_page_tine Cupreadl(cp->ssini(not disable, resetting whole "
		mo     new pa_COMP_R);t commanas_p
	 * hame);
);
