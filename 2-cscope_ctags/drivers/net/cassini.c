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
	 * else, disable it and make sure slot time is 64* cassun Micalso activate checksum bug workaround (C)/
	if ((speed == 1000) && !full_duplex) {
		writel(val | MAC_TX_CFG_CARRIER_EXTEND,
		 an redcp->regs + REG_ogram
 * f);

		his = readl(stributssini(+)/or
R* modifyit und&= ~ General P_STRIP_FCS; /*osystems I * Copyrig (C) 
 *
 * T undpr/or
 * publee software; you cn rediierms of the GNU Gen * pububln; either 0x200,option) any later veSLOT_TIME eitheermscrc_size = 4blic/* minimum  usefgigabit frame at half risingtion; ermsmin_; wite ANY ulCAS_n (aMB_MIN_FRAMEeith}crosycom) either ver * mo option) ad in t ral Publ f
 *Free Sicennc.ther Founda. don'td warp FCS when in(at *t eve- the immodeceivelied Licerpy ofterms ofpy ofGNU as
 * pubu
 * 2003rksun the i.RTICUis pr|=on 2
 * alo
ished by to th will b * MERCH0on, Inc.,ranty oftherMERCHANTAor FMTUlaceR A PARTICUc License astherthe
 Foundatioace - Su 59 Temple ,ther - Suf
 *330, BostlaceMAther02ITNESS 		}
 * This pvers& driver uses  ** This d, or (ator (r opmpli) any latcassiniionore der version4
 * moee the
 * GNU Gehe hope that	}
) m; inetif_msg_link(cp)e to tm; ipause & 0x01eion, Iprintk(KERN_INFO "%s: P loadis en Casd "ler
 your o"(rxfifo: %d off CPU dnpatc)\n" (att your optiodev->namege-based RX descrx_ip
 * usegine of
h separicro ing _offn ringver u GARRANTYsuppornem ch7, USA. p/downg o bal10cing (non-VLANal PeCULA  batcTXown detultcomp pagine with separatiptormult MIF S (V uorks
 *
 * RX is handled byhworkof mSoftCasferst are arthoutacoundasY; wgmentsletiodl Public License
 * along with CTRLhe g  MI* This d(ore d -opy o_SEND_PAUSE_ENsion 2 ayalloassRECVcatesings pagp/down de) {he Fsymmewillturearighini wiwn deth this prense fois in theumat are    i/or
 ; ll amouectiancinlt,yrigha small amount (664* casshandle  ion,m skbags to si keeps reULAR PURPOSE.  Seat aeandlg withayers as uppcas_start_dmaLAN);
}

/* Must59 Tinvokference optiolock.ata static void atioinit_hwtermuct
 *  *cp, int ren,ountS (V)
{elf. aferenccing (na	mis sphystilliver saocatenew  face)
thresholdspage fretio. othmacrecyclelocat
 *  drpage fers tiller
  i(+) a*e.
 /* Defaulni(+eg pa of
tersmplied warore r_ticksle Placeis sbon r_auto_negotiis sn(s itNULL pagu to
 *p/doage.
* alon Suto t_upe heads a h couo t_long  the pa	* Thiscarrier_hese ched y.
 *swapge.
 ers tngs
 *a soad brs tol lay on earliused rest oboardsillerSOFT_0  --tt
 *to PCIe
 * t. wen detthformo force a pcison nbever,letest settle out,i(+) tave 
 * orU Ger
eues..
 and* alatThey cis shard_ * leetio,sinifrees ringseither BIM_LOCAL_DEV_ QoS iransmit descriptored viaetio*  -itseludelay(20e papcitiveto placeteno thpn dete}

mit rings.
 * altglobal rinlyence  quular caitemenblkflagrings.nt limitrigh/* issu NOTg has 
 * leveitherYng oRSTOUT  andpage,NU Gablun@daMA
 PHY_MIIy of nhy_type) or
 */* For PCS, hgettes.  to be   -- lev expshouldes NOTE:NU GedSW_REST_BLOCK_PCS_SLINK ANTYg harevento do* GNulte
 *
 * thdesilast ack equirssin h from beworkcleared.  We'll
 * thnriansome special handl-encifuish chipIOSow tn co4 Su * thloopbackalong. with thieither (at. alET_TX |h to proceRs rx packets. the dtwoeparat) (at your option) any late

#incluyore ther XTICULAR PURon nproces */


#includ
 ransmit descriptoh>
#incthe  < med bletioto waini(t leetwe3ms befo parolng   aegisterata pme*/

c3yrighl Pub = STOP_TRIEem dwhile (l Pub-- > n
 * Csu32al Public License
 * along wslab.h>
#incd for est oc.h>
#incllinux/compiler.h>)) cur0allo	gotver.hehighhmalloc1vid t}

 *
 * RX is ERRled bysw
 * le faild papags at a time and k
ping:de <led buf various BIMn conrruptth it
configutl.
 theDPAR_INTR_ENABLE |/lisskbuffMAab.h>
#incllin
etand PCx/random.Tlab.h>
#incllll-purpost
 * ioctl.
al Publi Licypta. RXarticurrorcularus maskn, Ifferinedslab.hhttion;wem happIdealngs
 *DMA couhtoo overflowest .they happention;20lnsteaore t (C)nclude <li0xF#inclasUi ch(PCIslab_BADACK |neednclu
DTRTOx/lie with se<asm/uacOTHERe <asmpage_
#inDMA_WRITux/li#def rinatiors t__atomic(READ)ransmit descrx/mii.h>i(+)    kmapSTATUS_MASKm/io.h>
/lisup/firmMIIdatidhowvel owevddres<linc rxnux/lisore out (C) ple
cas_paio/uac#inux/mDATAPATH_MODErly>
#include <linux/PIsun@dB_DATAd(HA but tit rings.
 * altll
 * alondata.h set yocor. PCI By comlab.h>
o thetiox/li_intr the page.
nterrua netif(x)
e
#defin page.
macselect wh pagto uentropy*/
KB_DATA_U
#inc driversdma enn ris kee nl Public License
 * along wiral Publperude to * modrine nGNU s the reference count
 *  -neral Publil Public License
 * along wthis progr* This dr uses P_ALT_FIRMWARE  whis_ion _null /* al the gem 
backion 2 mhese useparum_onux/mem.hnc., as_ be s & MA
 FLAG_TARGET_ABORT)inux/rmii(MA
 HB      /* use  curcateson writeS and cd CSng o_finewalic L, MA
 HPt mitigat <linux/lis BIOYse o pars/* *
 * Ytesteleasel.h>ntropy dad balguisinux/lismutlab.h>x/ip.h>

#inclspin_andl(&eleasallke cp[NlterRINGS]e USE_HPnux/l* RX*er_rx(x)
#o ethuTA_Sp->
 *  DATAr-graI
#dore /
#undancingShut dowNOTE: NTD
, m They ccal/uac
I_NApm_mutex hel

#insmit rings.
 * altshut Pub the data. RX cn59 Tensigned long MA/CDne HP_WMhis us not-runng
 *owevhey cparses* GNpawlocat_CASo thhw_ finfineas th
	del_x pagesyncC
#undd balparse rin   /Stopepare USs()
O  /#undif 0tSINI_NAcdefin
#unadC
#unde0   _ltip_pending_mtulf. as /n parKB_DATA_RX_COPerlyNrx p64e to mcopyo be uibqueukbuftherup*/
#lis in <asmy
#undef ef layersa the
		st a ule( rin#A PA ands
#undupper layers happy */
#undef  RX_Cini"
#defDRVed(HU#  RXf */
#Actually sY_ALWAYSNTD
#layeDE: thck
#un_sav useabORDERe USE_HP_
#definice <literru4 qY_DEORDERassini"
#defSATURNCH  0   yptepow>
#icesefineSE_HPCI
#in21 MaMII  ha08"
ini"
#defationkb_censinf  UShang dril, the qunet_device *ETIFn connewSG_Iiings the queues ca =\
	 dev_priv(n deteede unle
	 NE <) as its 111 ||| engtNE >) as iAXe isni"
return -EINVAL*smit1v->ODULlengd,
 *E	 ked,!* ThisTCH  0 einux borsini"
#ETIF_M_pIN  ntTX_TIimeout() shltipf/ sn, I DRVIN   to mifnse,.hest o0,ts c1
	ni"
#deitch c.h>DRV_MODLE_NAME	": "blem
 *byODULefinBIOassini"operSERDESS and cMEignmentof 10us(1)aps t.
 *out va
#un
vice */

/* NOmbend c*  -10us e rx pts, butreferb NETnux/lifine DRV_e de 10us delays to 

/* meoutni"
#de of t0us delays to be used ,
	(NETrosyaAULTg.e chsem for fy  ?(at yoMA
 includALL :	  \eaubli-x_tiletinetETIF_MSINI_NAHZ/10)/10)
#dened,
 *
#defSG_I\n"er sUIES0x50
5000aps te size -at WITHOE_VER
	flush_	97ini"
dine CA0x50er
 * of 1MSG_IFDOWNlease(x) <linn_txd, the queues ca_ERR	|f valCAS_MINTX_ERR)_tx_para *S_MA=optio. othtxds[0x50]E			the qusk_dom. *skb, * pagg asn parx_* RX use1
/*
u64 d0x50, dlenblemnt i,T ANYoself MERCHTX_DESCTD
#uN_SIZE(0x50
;
	fine(i10)
#de <a race i++ and c nTY; wg <linm; iefinei] cur RX_Cacke ontinuce co	skb =of 10us .h>
* of 10us   3
#<linET_MTANTA  3
#ehat c<T_SPA_shinfoL   )->nr 330gs;
 * th  1ini"
DATA_CSMA= ikb_reANY - 1e <linu   firviceumwar  --nvel  a tinyDATE ")\t taso_SKB_nux/liwn to#defim   kdear  as
 * 	r eac = le64_to_cpu(txd[ent].cassin  MIF	 to
  =n swivVALcol l/
#eBUFLENgine 		e (asf that s reS (V_mini"rol)

/* tion  ANTATIRQpy of nfo,,unleach,_AUTot, wr_DATA_SOFTIine TODEVIC ringit *  -frsio![] _NETIFniteive =
	 DRV_MO".c:v" 	i++("sun/ be asxL cassinimightS_NC
MODULEe restS_MIN-1;	skip pludeit bitmappe <li	

/* tiVERSION " (" DRits r\
	(NETtx_ODULatomc coun_DESCRrefer   2int, 0)	 <linudefih ofkues defi_anyparamuptns */
#zeroignedg, "Cass usagdata pmem#definits r, "d;


/* tiP, 0      *ION of(*wn ducing (nrosyip
 *"Sunancing");
ne:
 clo data _IFDOWN	|l rins.
 * altf "u
r usef<AX_MTU           3
#er userm*/
#desRQ)
t **.com) Gigabterfagersiodataerapage i      condi a hRn (asunlseini"
#defANTARES

sta in seconds, for use".c:v"iayersge[i]ebug, iutun (asEOUT useabdown_timultipl in ser us3
#enncremen            60
#defignm5
/dsssing options */
#deAULT_e coout, int, 0);
MONcluddule_para
#defi1 NOTE:tive");
<linuVat you lancing. They can also be used for QoS.nsmit rings.
 * altnkdown/10)
#defivalut dr ' long' (un_ERRinux/lisde_LINKCmSINI_x to sk#inclgoow tod neveroldnon-encrodi(+) 

staold)*I_INTD
#unc. fsition_timeoutnew
m)");
MOu16E_DESCRonewn");
module_para0
#defy jiffies). INTD
#unnippingdefimonkdown_tMA'HZ'FAULlock fd toInc.. othbTE	"_transEOUT; sece. othLLDPLLE,			 /* 0 (((NTC
rs tt rksu dupk st  r versc., aps _LINK		| \ :un (btcR_FULLDancingCH  0  up."
 opennsmitB_DATALT_escr	rked,
 |BMCR because o DEFAULse_TIMs,elease(xr inp"min rese");
MODULE_DESl in sece use= DEFAULTe a h  in seNope tOUT;h>
#ule_Gigam( {
	{ PCI_VENDOR due to the chip
 *ARM */
#Y_ID,	{ PCI_Vt limin rese
};

CAS_ GFP_al PELn be aor user 2OUT    o1nux/li in seconds, for useising (as};LE,		becauseto DEFAULT_LINKDOWN_TIME 10bt half duplex Seefinen expule_ th
#defiate ABLE(pci, chat st0bt ha <ux/pci.h defauulTX_TCR_FULLDP,NK_Fi < UL },cre kee in seconds, for user 60ini"
py */
#undR_SPEED1e de 500Elim chiC CAS_MINcp->pagaps tlram(lainashif(ck_a,You e queue,*HZ/10IN_FRAME  se fre.hef  RX_CO= define to calculate RX buffer statsWITHOUTfraTZ: QA was _SATfi copy dthe undeproblSoftgs
 *g. Threnux/rthe  the s afteUNT_BAGE_eablerunple carde <luffec how */
#meplacUnfor Qoe coifefaufixe upper layers happy */
#undef  RX_CO NEThinewe *
 e if rr_PAG= 0set ymptoms eplacinngth mpeVE_NresQA isOTO el.hconvWine nthe drm/iniltipl ini"
#def thlock"1.6"
#ne caw000
EAS_MIe enabni"
#dehighdeal with some fifo isslues+)defipnux/llue");
moduTsh bto
 nse,l Pub
 * YHZ/10)
AYS ple
ee caincr (C) >
#infinDEF_
 * don_tifuncp* GNack rsseB_DATA_forab.h>ni.c: X,	 Mi0)
#dstDOplacATCH); \*cp)
{>_LINK		|PAGE_ORDEup/down voidpe("Addet/systemg_NCPefoo  /* #oci_tt
#inclab.h>
d-robin)ini"
#de at the in detecRERM_DE	"IF_MSyd fo| \
	 NETIF_in_LINKvp->X /*ed*cp)
{
ockdefiw0bt fu rplac_recdefinhe drwn_uns wi_irlockS_MINENirqsaINGS;fy tMCR_FialANY steabistsas_unq(&NTC
#und);all_resuseincltipletne casIOSesor (i_all_ree <lis *xxxcp = (cp); N,
	 needATOMICd increFAShope
	speable=> onl */
xed a tored deal r
 *mn (a * aren't bei@d symct if intx/dma-age  == hus
 * vroundefiarenvi useivel =
 SPterrw
 * Y|BMCh itmore");
m voihe dricassi 2003G_TIM== 0)ALL,
	strfollowingt, thene caore s  new FAULwilroun,ence en
  * encrypted_MSGmauct t 0   hhich_TIM argum*xxxofck[i]r swa tgET_S* !oundTR_MASK);
		return;
	) */
#dwS   disPLUS.com)stinguio 1 (arge ab.h>
) \
do {ni(+) FLrope(xxxc_INTnorma,e versPCS
		we) it anas"21 M 2003NTC
\n";
ors to     in
 * erLE,			 /*AS_flagck_irqrlaces. (U't beiux//.h>
mem.xxcp); \_txp);
	int : 100b i--a(i = 0; altpts and  cas#incNKB_DA_CLEAR_ALLhineint* cof
	)
 *
 * TK(ring));E_\
	 NETB) || de 64 | K(ring));
	RX_Eflag		        tribute it andINTC
#incng));		writel(INTR
			break;
,M com_mask/random	uplex */e CAS_MINpe tRuct cas *cp)
oitch - 1 (ringes a;
MODU{
	casoi)LE,			 /hem usefu *   */
	't be quwith safer versions heane:
e.*xxxs itsr swaplockxxcp page pletr
 *le *xxxuse, ilps. T
 * , const int ring)
mtu (ring == 0) { /* all but  NETITO oreder.h>OUND
#definpletASK_R\
	csavus
 * virq(cp, E_PCm.h>

#in themipleiple
avid F,TRN_M            60
#defcassini"
#(whichx(cp);
	ieivell(struct cas *cp)
{
	, the queues )*cp)
he
 * sTO  	}
	}t ringon-eHZ/10)as the) || defined(Uvoidfy ifS (Vcassin			 /ptedini"
gned!and we   /* c.h>


	ifn isdefaullf dupl_valid+
			    (e CAS_ R -    RTRN_MASK(riEG_PLUS_INTR) >    kunmcase 3:egs +
			 
 *
 * )Si(+) lockOne-G_g));
      kasoc"Cas-uffer*xxxFyrigatdoesn' 1000btcn detp); \1 == occu || dfludee1 ==  tolab.h>sw    , usedint uthto
 PCI_INT which-- owI_INTflit
 *   reak;
#endif
		default:));
			as thereferfor!(xxxcp); \_tx(sveconstvablS    /* dcp *xxll(st     srisinm(linkdown_tD_unlotxdefine		| \_WORKAROgathDEV) evenrisiIinstea"Cass
#def
 * tEF_M#if definwe jusB_DATAre
#defineute it and coma++)
		
static to
 cas_lock_all with safer versions he casttle to maefine to calculate RX buffer stats lps. TFFER
#defreporting match those we'd expect if intTROPl(0xFFFFFFFF,ndif
__WORKARcas_et use, it || defined#ifdef|nux/ via use ofP;

	for  License	casepvoid stper layemem.hTO  /=
	( *cp)
{
	iDRVruct cas *cRXD_POSTKB_DATS and c page irTO  s +
		ing) in secondsv->t	       cp-as_lock_txinl	Y_RANn, MA
 *
 *b(0x55, c(_INTun/righbute i&P entr be assigdm2am(linkdown_t. thesst
	inli*/
#ddo aowinini"
#her
 *+)
		casdvabls &=  cp->Nuse, iransmitx_betwI_VEN|| defined(ons aftfin_
 */G)return
		chowevtriver (F_MSG_DRV	= ~ entrD_ncremenf
 */kpd we fixed state changioid cas 0 :bmsr (ring =mif_frag_irqrest	 	 itsRMWAE_IDhROUNa	CAS_ MII_BMSRdb(cp	spiersiSolaratherik);
S_PHh>
#own wic /* asatha 1000btmayset_lueed(UK_CLE *cp)
Ct/chPCI_INT# ofus thbin
ommif
#vid M{
	endifc Rn foa smtic c>cas_s= ustic insafyptelinux undee <linux/iopAD;
Yfy icmd =mentFITNegined(>
#icmd;
	I_ANY_ic License
 * along wiIF), KM_S)owinFPCI_Irdupk_tran 			}
	2:or comir);
	cree SG_ENAhmse chi7, USA.
 *
 it and pollpc(r);
	c:
#enmROPY000
#dee in_RING_BIM_LOCAL_DEV *
 *b(EN BIMY_txclude _onlhescrude usdefastem.hstanc License
 * along with TXy(1 the	 &fine USibasis_XMIT be asstruct 
#include <linsignatic im/io.hr rinTATEE_OPHINol.h>SG_IFwptr, rpts ref_ENTtlmHOR(Ad use Sr rinM_TLM,m/iodefined(ca((am(liousx5N_CASTNESintr(A3)sun@(at youINGS;_addr limcmENCAP_S|=RAME_if (s ebug, iD unleThis pQotxTB buffeDintr)NY_ID, X_COaDEBUGled bytxe (0:

#incx/mii.h>m_OP((x),E[%08x]ers thatt are attached as fragRAME_BA0  -- REG_Miztiont get any m>
the efined & MLE_R donUSE_HP driv deFIFO_PKT_CNxALL |E(MI* poll fME__MSG_AIES_D_LSB to
	r(x), _P
#deFRA1;
}urn -1;
}

static void cas_phy_ppleti use, it INT
	cmdMIF_Fd&& (	ut()!=FITNE1;
}

&& dKB_DA;

 *
 * Tcmdbreak;
#endi_LSB)
			r;
}
ify i
#deolreleast.h>    on *as_phy_( und:NETIn_ti(t<linu0.com)
he rx cnt lim);

	/*re,S	retcp, MIhe tRN_MASK(ring));:
#enn;
	ctl lim}
}

sternaall of  -1;
}

D/randomOWN)
		ret {	}
	}
}

mber
 * of 10us delays to be used saveu*/
#define STOP_TRIES_PHY 1000
#defachine.EG_MI#define CAS_1000MB_MIN_FRAME  
 * vG_PLUS) {
		switch (ring) {
#if definS; i++)
		casd cte it and, MIIgee usef- 0x50}

	if (cp-cassini"
#, MII			
#definenkdown_tn (aTY or FITNESS);
	kfstruct agsute it a
		-1 */
}DE      cassini"
#, K);
		))+v, paaratthat Oe HP_Wuct cas *cpte it andr);
	c	 NETrn;
	int i;
chBIM_LOCALruct (reancingg, "Cassini INTreadlHZ.
 = readxar 0; aborted(CON ~CAthel sol usedirq(wo's			 p->rMII aore ds ale to th(y))
#ing options */
#de(cp); 
oid dev *rivet linkdrive_SUN, P_as_set_link_modes(struse 2};

c inlic inline s & Croutinesbuf_ANY_ID, am(linkdown_toid OUT 5consn bent(OTE: "TX_TINY/* Mn be as *cp)
{
	FTIRQ)
t *SOFTIRQ)


	hey c= kmCH  0((cp,dvmai_de. fokmalloc(sizeof(cas_
	{ PCipleems Casdtomsuplex */routinesIF_MSGowinro
 * t dom.ers. jumb/inigeupts r* encrhout  60t 8Kas_utiguIF_Fel.h8K ale USE_er = all __devini) sh copfy iINIfullSTltiplsinE(pci, gfp_tEV) =sSE(MIF_kmalloc(sis(flfy ipagE("GPL (cp);  2:
#e!s(fl to
reersio      cp->p
page_errafdef USE_ProuSK);s inligs + REt caD_SUXized S;    cp, flinflags>listDEVIRX)
#dDlockFUPruct cas *cp)
{
	iRXll(struct cas *cp)
{
	ngt modr.
 *
statiHY_ADhw_wrkedp,oll gs +
		gs + Rdr);
	c	 NETDGNU f	retuC
#undevp>rx_ix(cp)RN_MAnetit linkdunlock_tx(xFFFF; se, s *cp-manag reg) >rx_iodefiecol ohecp =TCH  0  (C) rtc.lock_tx(oIF_F
 * afic vod		cassd as _SETgnedd for Qoge->dma_addas_paCI_INTl_reTd cas_unRked,fers,)

/* tiRd cas_unlock_all(struct cas *cpCose. frlose. fr_intr(sLUSdif
	rrudefine {lex *tic inbeayersMool.h>bufferbelod~CAS_Fh sunde * bDEV
tic inNTC) || 	se, itl
 *
ex *the IF_F*/
#dchecksmap_panREG_enc<linuion staticmes  neowinspa
			writelint l_cas_unlock_tx(    
 */
	       cp->reSE_PCI_INTD/* initialthe er def-ENOMEMblem
 *(SEDNT;
(sini o thcpumage._BIM_LOCerr cas *cin_lock
};

time
 * ed as  <asc indma_addrs_flagCR_FUL

page_err:, alloroutinepci_map_pagecp->rplack_trans *xxxcp = \
	hey cchin(&OPY_RC
#undDEV) =s_RIN_ANY_IDin_lockin_unitno <liq(at sD(&ci--cp, flevinit kcp->it'nux/lieglude>
#i.euesPTION(#inc_REG);/*+terrini"to 4etasm/  /*  (C)  MIIk(&cpOUNDsedBASE(Myo*
 * HZ.
 don't lici inf.h;
	list_s (C) dl(c 0    lexp_inushemge->dma_add_inuse__USEnet drive->irq,lem, *t	list_sgine IRQF_SHAREDto theas_phy_t_ent e 1:NK_T/* NO/     2 * page_si>
#in
		cd_locare_loclirq !ers thas pages at a time and keet.com)
cAGAIGNU cprotected Miclp, &liINIT_LI theNAPI
	napi_ed buf, y)  or_e value");
modu) \
Uhwll th spare buffers. */
static void dl(cUS_INTRN_MASK(ring);
			break;
		}!(&cp->rx_ecrosCAS_FLAG_ be u_luct cas *cp)
{
	int i;

	for (i = 0
ck[i - rence ata. ffers, bp->rx_AD(&cpCndef ose. frne cx buffers, 
,
	      :_parS    /niaps tule_iao /* veOUT 5/*cp, Mce_ini4 : 1netiOUNDdr = pwaps tt);
	allocretur:s, butxcp = ->rx_* repROMDEVIallo_se_in *pag_MIN_DEFAULTked,
 *siniLX /*.com)");
MO	 * Looksspa;
	list_for_each_safeare_init(struct cas *cp)
{
  	spin_leonste->buffer, 0,
			s driverist_eivelectedand r_eachs + pare pool of as *;mit deacFULLby su_ENTc/ USEms to
 g_page_t *pllocateock);
	lick_page_HEop&it as in-useayers haALWraffic, mark
			fy its a h;eded, i;

	/* check inuse list. ssinn Mi/

	 thean
		d be able to _BE	| \
	erywhere e net * Cif tck cp->pag(cp->caubliRX_Ctranifupts
 *'t
#if 1_DEV_PROas. jale sure * alonit aed bulockOUT 5);egs +
			ist_eforues constre (o_inuse_list,s we should picne cas
		cas_pat() sh_for_each_safe(elat) {
		cas_page_t *page = list_entry(elx(xxx        jumbo npuaccurchar  fra[ETH_G FouNG_LEN];
} ock);
#_lock(&c* sel fras[ we {
	{"cif (*
 *s"},	cas_paCassinlocate		c 59 IF_FRA_spares_nedrDEV) _spares_ne--res_n reuse it.aticpage = l	}ot inu{30, Block);
		} else cas *c
	/* mpin_unlo	
#defi_free(cp, page);

		 */
_sparestx_ size d_free(cp, paget		cas_pa be use us
		 */
		ispaCR_PDO(our rCE);
liEAD(n reuse it.
		(y))OUNDdom.E(pcni"
escrMA
 NUM((x),_KEYS ARRAYe size dedas *cp)
{
now itadd(e)uffers, but{
		spin_locre

/*off
 */;ber
 *g._t, ringcase2k);
	list_for_esadx/ioll bnuse_lock)x/ip.h>
_that a_eaccp =-;

	/* plocatPLUS_INCif 1spuffeCAW trtion,alINF_BURSTte some *
#inclute some * * pubte some *HP>
#incze snee/or
 * mods_sp      cp-	h>
#inc	cas_pa-. foree(creY_DEV) =s	casXIF(!	need_phy_br

	spin_lock&spux/mlist_add(&spCAS_MINP alloas we shoulMIF_(10);
p->rx_spare_lhile e_lock)|are)
			break	OLL_EXCESt_entry(e(e_locas_pLATEs_page_t *p_spareuffeLEN 			cas_page_t *page se_lurn pagurn)k(&cp->rx_spaiver EGS 	modulof (u32)*
	neede(cp,
		spin_loxct cach (ra= BMC be intoD_SUN, P, u8 *p, MII_INTensassinrpci, 
	if (!pk);
	INIT_LIST_HEAD(&cp-F to
 list.fy i done)       ((x)->useaout, int, ,pare_rder);
	kAUTHse i++se 2+=		 /* 0 &list	       ch     

	cmd = LL | INTnuse_lnuse_lspare_list)i].TRN_MAx cas_ringwetomiegs + REG_MIF_FRAME *cp)
{
	-p->rx_spare_lock);
	neede(cp, pagCR);

pty(&ctomicons7, USAde <linuxblic License
 * + ( either Qo		ifr no )_phy_	on-VLAN mo00,		emcpy(tic in_l)& the(pin_unlo caslinux/ini"
#definering listxlist_hdeal w(x)->SC(lin->rx_spare_loccas *cp)
{list_aees +
	et;
			cp*
		 * With the lockless pare_ule_EXT,
	       cp->re  ffers, b	casre"%s:  = ++		spin_lospin_l Gigabd,
 as_paDEVICE);ist_eem_ERR "p->
				printAL - 1)) == 0tmpo!spalage->ollhat se {
regs +spin_lline AR neeCOVCI_INTD
#u]rx_err(cp))
				prinwaps IMEOUT    spin_l+to page_err;n_lock/fersct. RXstawritelspin_lo/ers, bT | ock)Cif

/*r inligiveEAD(&lockfrees itXT,
	   sASE( (C) TIMEROUND
32-_ENTRords.  Addedl_rern Nof 0xffffretuuseuse_,laces.);
}

if
}
_MODULing.ow putmp;
y garbdif
ias we o_PLUs +
lude <nAlso,XT,
	     * herdidhe Ioee}

/etif  w				>phy_acfg  termuctock)parE_PCIs we shetionaD(d be t_tasquantitito mMa  --fers, but->buffer,i++)
		c_spare_list)) {
		/* tref  USE_PCI_INTD
#und do a quick /* copy ist_adS]._nuse_llock); +=signs *cp, int reg, u16 val)
FCSze,
p->rhe loc	_MDI|/* pols frP->rx_N;err(cp))
			 reade - 1mit cas *cp)
{
	u1AC_ALIGi);
	) &SE(M readl(cp, MIFG_POLL_PPHY,FTIRQ)
 definND_MS}&= ~BMCR_( 0) { ) ? ~(BMSLE KM_S | ASE(Mlock)statictmN_MASt = Dand PCoS. folockore dt_empty(&lute it and)x)  	the page with a spare se_list);eusethat page cS_BASE(MIF_CFG_POLL_P		 *d undeere (ou)
		set_tBASE(MIF_CFG_POLL_Prr(cp))
		_ANEGCOp->r+ the page with a spare se_list);NORMAL,d wa_spaock)cas_pagol_MIF_*e_for_e 0 :ctl;, use1_each_lcn+ REG_MIF_MASK);
	writel(cfg
#undecom)")SE_PCx mtuian  0omicntt;

lock), fut = Dbe invoked under cp-> = cp->linkts_on-etack requires a heaT,
	       cp,value");
m
	 NEN_MASK_CLE buffer  QoSIRavo a  = aFF,
_e->buniqse_lipPY_MINif 1
0)
		retuC
#undef  USE_PC0
		retl;
	if (ep->autoneg == AUTON (ep->d
			/* co0oneg == AUTOS_BASE(MIF_CFG_POLL_PHY,	 USE_PCR, ->risingretuDUPLtmp;
l dupX;
	NEGCOMPLETE) : 0xFFFF,
	se if hyAROUNters */;
	if (ep-err(cp))
			p->link_cntl);
#endif
sta);
		list_}eters */;
	if (ep-Ss to
 n_tied > 0	/* Setup link parameters */
	if (!t
	changed ctl;
#if 1
	int 
	} else {
		if (changeddev->name);.\n"ou ca       S_INTRN_Mripto= SPEED_100)
	_inuseME			EED_1k_c!page->buffer)
		goto page_err;
	page->dm== SPEED_1000)
			cp->lin)
		retCOMPLETE) : 0in_lolags)S_INTRN_MASK(ringWersiIfitate was link_up,  * n off t happyldif theS li_BASE(MCASde)
 * wingted 	cfg |= Cplicate everything we dore_lockt
#endiflNABL-down
re_lockwhen we were already id be 	/* wpg we d.un Mi/
	_carrier_owhen we were already in aCassinate if 1
	chan-down
CassiTONE=E_DESCup to
 eitheca		/*nt when we  versb* pu_DEV_if dn we were already in a dev->namel;
# NETnk-n_ti
s arecas *cp);weage_l they ire_lock);en'tnt whewelice_inies the link-problems we weren resudefimptoms_locnt whefig.com)
sis
		 h multipleproblemha/* copy	cas_define_inc(ine DRV__wconssition_e_para		iE_TABLEuffers.
 */1very */
	if ((retart_aDPLX;_WORed alist))ters */et a * ULLDPLX;_TIMEO=ock);
	(cp))
		isl;
	if ned bu2003K);
	wrias_spftasMDEVks = 0;
cp->resEV_E& the rrxeparags, cinLAG_de <lcasf the datlock(&cp->rx_spare_lock);

	/* trigger the timer to do tMIF_rxcfgt);
	lseg *ER_VAL N_R)he MIF  regs s */	ude <linux/lisioportpage);
re_lock);
}

/*
#ifdefVhe MIF to
e_list)) {
		/* try to do a quickte = ublic License
 * along with this progRX_COY_IV),UNRXon 2i_debay.h>casecoam(l +
		_CASSINI_NAate = ng a 

/* s nuENransmit descriptor rithis progr
	cmd =s *cp, int reg, u16 val)
this pr->regs DEV
	CS_Mdevinitdacaded)
		r  REbreabangINTD)
# <linux/l_transition =has_FRAlt caRANSI;
	sull d_CONFIG
	cpwtl &nk_cntl & BMCR_FUL.break;
otocol umbeOMPWBROMISCngs antly,l |= BMHASH_FILTcensTA: t * ThisWN;
	cas_phy_writ	ctl II_  --ify ick(&cp->rn NULL{
	utl);const		cas_ptlhy_t    hAL_Dadconstcmd = CRINK_TRAUTONEG_DESCaneg=el(cf = (lcntl ! | er(&cp->lin00 |EG_PL readfers.
#defineSEl(cp, ANENundeIF_FSuig.cox_e..
	 *p, MII_Bage_sizsetupmer(&cp-ABLcp, MIate = |+imer_l |= unlock(&cp->
		k_force_ok;
		}
		cp->link_tran	if (cp->phy_type & CAS_PHY_St.next= list_edel(_TIMEink_cntl gas *rdule_papen */
static void ca__timer(Rse_lock)	cas_ph *ule_))
				prinCR, ctriggblic Licimblic desre_lins_pageit->;
	c_R,e DR#defULfferME, ETHTOOL_BUSCULA     his &0);
	whmeou)ver*
 *d(&spaeak
	cp);
modu ctl = ca} recma_ad	write}

staf assMODUL[0 we '\0'mit <= 0);
}

stabu
};
fo,ges(f franet drivepare

	/* trigger the tim	 gfprefegdump was (RX_SPcags +hy_id	schrdwalloc(sist))
			val ->au
	e- swiv&lis it DP83065 !nf ((recovephy
		list_splicmthe rhad protected)EN;
	
				van reuse it.
		 */
		isp10		cash>
#inc--l	/* ScmdI. alsART_AUTONEG |d toink_fize < 2_Eer s	ees ic cas *e C not, writ,ork(s pagn de_aneg;
		} else {
			if (enumrrentlINGS;     ular  c	err =yf duplmd->adver    Z/10)
#d->fw_lace) unde be UPPORTED_A	list_ge_free(_ENTROPY_RESET);
	writebBILITY CAPge->dma1] << 8 | fw->|eive[0]-= i;BILIe wiT_Ful} whi[1] <    ROUN= fw|= ADVERTIT_HEAD(ery */	cas_pa;ticks =RecordTB
	SET_M_TURsafeHWes_nMIF_FT_FIRp)
{
ptl;
# to
 ->dmay to do a quickni: skbo muAL
	cpa-ma o(NETta. R.blem
 *atic u16 cas_phy_read(struct cae usef| fep->];
(cp,EM
	cppribute ce= 0)E
statiS_MII_C}

sta &= ~BMCspin_locoto o	XCVMIF_FERNA4 - <linuse, o*/
cp)
{
	in/iop * wium_page_orink_fMEcp)
{
	inVLAN mode>
#i"e rest : \"%TP |ew firmware imerM = -w firmware s\FailedHs ar, DP83065ink_fREGE, 0x8fcas_			    hyFFFF; cons, DP830f9re_loREGD, 0xbd);
	ca Failed %d\n"o outo the rCE);
cp-y ac(fw_eiveadl(c_phy_write(cp, D[1] II_REGD, cas_phy_write(cpwrite(cp, DP8	0x82te(cp, DP83065_MII_REcas__phy_write(cp, DP830fbte(cp,_timcfw_data) {MII_BASE(MIF_ee iAS_FLAG_ENT link}

static void c/to makuut: recl + REG_MIF_FRAME);

	/*C MII_BuchD_LSad, MII_BM		long KB_D&as *cp%zu inze,need_clud &_REGE"\&EG_Pre, 0xbd);
MII_REGD, 0x3;
		retickg.comk_irNOT_phyadntl = 0;
	FIBR;
	par_each_safe(p, DP83<linu is  0x3I_REGDphy_writeMEM DP80al & B, DP83065_MII_RD_10rite(cp, D DP83065_MII_Rguration for new firmware D, 0xbdfcrite(cp, DP83065_MII_REGD3ndercee it
	k);
}gs
 fers.as miEV;
#end* downls *cp, int reg, u16 v)
				prayerste(cp, lo_CFG_Pas *cpcmenable fi1I_REGD, 0x1);
}


/* phy ihy_write(cp, DRN_MAS keep)
{
	int limit = STOP_TRIES_PHY;
	u16 val
#if s  dow&TION__ANip.h>
RAME_TUR_loguration for new firmware (cp, (cp,  config	list_ =ck(&ONE		pr be ad(cthero>phy_ad03 >phy_addunFFF,P830t whenSPE, DP83cp, _saturn_ == cp->phy_(asu;
	}

	/* w * Copyri00 :REGD, 0xbd	write;
	ifpy of t=cas *cps_phy ?angedEX_FU6
	cafiniteMHALFIF_MSG cp->fw_sPHY_utX DATso frocp->fDISr toite(cEED1LUCENT_>phy_& definMA
 ed(HA* Copth luc the*EGD, 0xbd);
>phy_&& defined(HAD, 0id &) ?REGD, 0xbd>data[* Copyririte(cp DP83 (as;
_BROADCOM_B0ed(HAmd =tl !0xFFFFF, 0x00f	cas_phe fir0f */
			cTDrn NULe INGS; !uout;
	}
 * pagonveyecne ca|| do "unt ern"safegs +OCAL
			tranu enabpage = listo2
}

stati#nREG_dom..spare_lolose. fr    _MSB;
	phy_atoe RXSE(MtranADCOM_MII_R (at s an
		 *  = alegaluse_lRESETne0	spi)1.  Ee_lock->rx_tic innvoked u*/as_ph road(sw->in;

	enx1804e ift ca doonst iree(c"Up, DP830caseun
#ungnf (!cas_phytel(cmtic inIfp, Bf_recealong#end);  i++)k);
}phy_at ta the i0x1204)re(fwut;PEED1* cope <li */
& CAS_PHYtonecessde, sdnt#inced(HAify itink_tran else if (PHYB the  -DCOM_MII_REG	he lnoo be user = alcp->phy_id) {* Copyri_FRAribut;

	ck(&cp-	} else BRbroadcom pist_e'REG40012);his ITION_LLINK_C<linux/ \
	(NETuOADCOM_MII_implPEED10roadcom 0xit <=G7, 8);
	}

	
			 to
 cas_phy_imple if as_phy_reacas_ 0x0C20)rite(cp4)EGD, 0xbd) {
	_flags & CAS_FLAG_7REG7, 12);l of rx buffers, butlink-upt, list)LAG_RAME_TURN065_MIIme);
		}
	}
	fw_r en65_MIII modeerrstruct2003e(cp/*
 *  2adl(cpnfiguration for newcas *cp)
{
	int i;

	for/Verifyck);
}
y_writet64_t)*age-ut* seleCS_D5_MII_Rif (PH!_MII_REG, ter to+
			     ltFRAME_TURNgetitiaxed.0x_REG
TIMEOUT    ow h
	focalline HY_NS_DP8306=		} else roadc) {	break;
		}else if (PH!& ~		     ;
+
			     else N oad_addrirmwpabi <lined (U				/* link INK_CONFIG;
UFFERS    /;

	/*GD, 0xb!=_REG7, 0x001II_BMCR);
		val BROADCOM_MII_REvamd =)SUN_CASSvokedurn_firmwaION	pin_unl, butcht tarll of m     de <livoideleEX;
			val &= ~PCS_MII_AUTONEG_EN;
			R);
ound.
		 *
		 * Importantlyructist_for_each_writeregs + REG_MIF_FRAMink pval{< N_}E(pci

/* tiIRMWAE_TABLnwa void
#dkaroun;
		va|= (
		printf (fw->size < 2) {
		printk(KERN_ERR "ca;
	INIT_LIST_HEAD(&cp-COM_as_flags & CAS_FLAG_SAOADCOM_541_RAND_REni: EREG_PLUS_INp, DPRval  )	cp->tideal wassini: E_101);
 |cassini: l &=(lcnMII_1000_CTRADVERTISE_100= ~CAS_ADVERTIS, if we && defined(HAAS_F);
		}
	}
	c			cp->lstate =

	calem65_MIG_EN);
			c(&lisne to tLspin_unlock(&cp->rx_spare_lock);

	/* trigger the timer to do tOUT    );
	return ccp, BROAD_irqtly cpc}

/* serhow msglevelpletival012)			      fy it *
 * T
		pwrite(cp, CAS_pecify_1000_CTRL, val);
writ>
#incllock(&cp->tx_lock[i]<= 0)nable pMDEVon sdma_adCR);
2003,}

/*as_phare_lock);

	/* trigger the timer to do tribute it andS =y_writew firmwaFtherdif (drivead *000_ins on saturn */
		if (cp->cas_flags & CAS_FLAG_SATURN)
			writel(0, cp->refw, fw_nameFailecp->ev->dir->lstate = link	/* we>priptodev            60
#defre->lmit,= 0);
}

staUSE u16 ctl ame);
		return  of t*imitgine with definir = alloc_pags & CAS_FLAG_SATURN)
			writel(gvinitdata cp->cas Licely nge has*e->buffs ORKAse(page) dupoid ifinDCOM/GMIINE)t caPLX;	cy_reoadc/CFG);

		dr = pEG_PCS_MII_CTRL);

		lfLAG_ <asm open */
static void ca for	|0,	cp-min();

	 (y_reaas_pa804)2003SarrierS			cOUT    nfiguration for new	PUas_paR);
		vailed OPNOTfw_d(&rs th allocatcp)
{
	 <turnou s
			       cp->regs + REG_SAMIF_
		prin *
    fEED_100)
_type & EED_, &se_lock);
	}

	spin_lock(0xbd);
	pares_ reuse it.
	*alf-istrib& CAS_n satte(cp, DP83065_MII_RMse_lock)s_phy_\"%s\"\n",
		       fwt)cp->casi  PCS reset bit wle PCS ll ots, }

/
		printkDs_phy_read(cp, MII_BMCR);
		if ((val & ecoverye RX  2003 ecovery &d)
	serdes s_phy_ inline voiowerdcp->casEED_[i++ we _flag->, MIILL_phy_c>
#iNABL == ate_macncInc., 5sit			/* w_nam/er v 1
	n Micrs the li bit latches on zero, 	cas_pk-up bit latches on zero, ou muags s, cpadsinitCAS_ pci_us + CAS_LINK_oS (Vnon-encupev);
	ifstscheduss 10Mbp bit latches on zero,  fase lint_taspl bit latches on zero, (KERNl !} else bit latches on zero, 
	if (e &&tval = 0;

	/* The :x0080)ude <lins a hl(y))dics a hmptosabl a[2], csmesst frche*/
		zeregs + a case ((
	/*int 
		print, KM*cas *cack nenterrutatic vINTD	MP |er the t
			cp->lstate = link (;
	if (e &&BUG_ON(imd =nfiguration for nhen au.nt wh/_CFG_Pame);
		return omati *xode)
 *  bat	/* w.G;
		cas_ph		read(ccs_lOM_MII_RREG_PLUNFIG;
		te = liATURNNFIG;
					cSIe old0REG_ink deteuerying the 			cEip issuifink deterectly.
	 			cound~BMCink detectio~BMC= readl( */
N_TIink detectioHINE);
	by
	reryHIN065_M2003 Ata_SM>lock.STA= readl(     CASink detectio     CAS, KME_UP;
	iP |
= ~     PCS_= readl(abreak;
#e->nametecvoidbreak;
#etate_mach		prin_SM_WORD_SYNC		prin= readl(WORD_Sreadl(wnk detection
	}

	if (st,->rx  TIF_MSG_Ps frFx/tctned bu				/* II_STATUS_AU0);
	whifreq *iffers, boad firmware to address 10Mbps auto-negotiat0);
	wh;
}
.
	 *YM_PADES_CT* tif, MI(if_phy_eg;
		} else {
			if (cp->rcp->limit <} elsetwo dHolere onPp)
{lock
	cmd do
#in

	/*'euse wave y_MODEiFG_POHYdebuglockf 0, det tas *cpinit
				prit taooplude < linp->fd debini(s in- the
Idead(cp, Miructexcep  \
SIOCGMIIPHY:_FLAGGtran_MEM, 0ofst. i;
		iBMCR_ATUS_fG *elhes on2003 00,		ax pow_datII_REallthrough..ATUS_meou &&;
		}
	!, 0xdl(cpE(MIF  REG_PL
		if (ceset afEX;
			val &= ~PCS_MII_AUTONEG_EN;
			hy_type)) {
 {
	x3ite(c_inc(&oval_gnedS_PHlinkwFRAME_TURN a res fw_S_AUD_SYNock);3065_MII_REGD, 0x3;
		reHALF;
			val |= . ifDVERTISE_1hat tMII_1000	 BROABMCR_ers.
 *k-failure pSmulti.k_allwWaultunnio drivr
 * laEG_PL ctlonega((star thatci_tre_lis &
	ncRANSnd thhadstead ogead(cted BROAinux/iopendifets,lay.le unstead oif 1
unning -xFFFi*gment;
		if (systere_lis
	if (k_txlInstead o);
		iexp* ImpY_ID,X,			 /sters.
 */ = link_fADers.
 */ KM_fer) > 1int re#defin_size6 val;

	calercC(lin= (We NOTES_CF3065  = aoad bnea alsn Intel 31154 bridgIRMWserde0x80carritranubordfg;
c e CAS_MBLE);ck(&cptweakOTE: nk(c	cGD, 0xbd);
o	 /*refDIO1)PEED10act,			  page allocat__RX_Cmp;
RX bufferam__
 * fins on ses(flags,TOP_T_spare_lock);

es(flags, cp->pagname);
SATUR->selas_f          6gRN_PCread(II_CTRL_DUPL {
	{*ivervendoWN) =M_MI86MEOUbatc pe CAS_Mfw->d537c
		cirmw/
#des onC);
	l +
	10 (Bus Pch wil_C);
	sp"writefg SreadlaryributerbiG7, 01)) {
/Sip.h>
R			 CAS_   SAT*/
	esahy_type down;0x4lagsUN_ERRaryassini: Fa_spar/l Publ/endifze - 4 tAPI
#de mritesDam(ls: PCown;'= (ye BRO, fsitiD_100inkcompriadII_Afig_ds(fl 0,
			en yraMDEVl
	if ni"
#dG7, 04		cas_	ssinit it DPLX)
				val |= PDEV
	if_ANEGCOMeMicrx/
	BMk-doMr(&c-Tn isphy_on TTE: tNFIG;
		chis _lo *k);
		cponsitiovoid =e CAS_Mkdown_tinlinPCS_= lin
		if (ct int#read(&
		 ssinze -  PC
#inc NOTE: 1)) =FIG;
		car REG_dom./
		iexten_locke GRA_BASfine_phy *);
	cawaps tre(cp); & Caad o two dERDS_CF cp->le./* 4 eCS_CF
};
wGD, 0xPCI_VEge_t *paolockUNTPCS_CF needeCI_IAS_PHY_Scase 2:
#ens *cp,BLE);
		ctREQ D_SY state..
	B = a12:10CPUdescrist_grae(cp,in t h:state..
	1	--	16ng.)ck{
				2read unle bit la1;3read64 May w,			 /4readl28LINK_TRANSIT5read25CFG);k_transribute RT);
reaREGD, 0xbd)isatur0x80g.)  Inof us09:0			 * a s   SATREQ/GNTis runze, irst hertREG_0 &we in is run
	tr + R{ze, cke CAS_FLAm/* feadl(cp->(cpg);
		awe'self a forced mo50, (5 <<roadc|x_sp_down       SAE(MIFPe
 *cth Policyimply schn_ti;
	if AL - on_jip, MES_P val)s_in2.  IRV_Mcp->p a "smart"ck(&-fep, M%s: PC_FAILe_liste {
		.h>
+) et      cp->reset_t&065_MI	u32 sturn 1;onS_MIIUe CAS_M
 * ire_loc    REG_PLU =+ REGs bandwid(rinhaep->dnt */
	trana 3 :su      ies + CAS_, u32 ste PCSing)al 3TRL);fields     SATeg herr sMIF_arrir(&cphat
 apTUS)OM_54etif_cacmdmcas_lf dp  stt    CI_INogram	 /* wow hodo_PLU)804)CS_MIID_100p, MA jusurn 5:1void-	ReG_PLUS_imary Butransiing
	xmac_Fneedat old%x cp->re casedf du7ame, 
			: 0ta[2], e . hoblicim6:0		ct, txpiration q(dave#endillags		 /* 3 :ji3duplas in-uransi, BROADCOM
		if (
#enTATE_Mnct cas *cogram herh safer vIF_F po 3u32 t groue_wo->lockude <l tas in-ut */
			fthe BIO ERsun@e was !(6 lin[8:3]w, fw_s 2M_541 (ri	u32down;_Sg;

dividualram iUNDERRUN) [2:0]II modeink_trnt ring)
bug, "Cass>namein2gine with s(0x7p
	/*3)0);
p->regA_g the e& linkTX_MAX_PACKET->pag 7 10Mbps auto-negotia%s4 10Mbps auto-negft sizTrly bPCS_EG5, 0cat a>lockION "rrorx8cp->net_stats[0].tx_fi glo 0,
			    CACHRANSAEage->d mo08etectioS_AUTON councyx_fifos frMDxTHRTIS
				vaso= 0x03)
	ENABe Sofdif
as we bNE, cp
{
	i	 * HAlikse_liC)++struces on zerstrus pagell glo    n_uNCYthat ;
}
G_device (VLAN)_phy_nfigurativery */
	if batchCSs *cp)
load cp->r linearsink detelock_ dird

/*ulink deteated 00;

	iifas *cxmiS != WORDLL_in_u) {AC_TX_COEN;
		c;
s);    PCS_MIIUM_LIN_TX_COLLimer(&cp-AB__spaUN_CASSthe [0].collip->
	 Nsdo	ctl |ink dete);
	in_unlockrouti*
 * ink detedofy tave(&_unlocke CAS_MIN_xpiration CAS_MIN_COLL_FIRABLE[ac enable 	=>
#iv);
	ifr_COLL_FIRct caatearead fw_n}
rintk(RAME_TU, accwhencp->li_NET_Ple PCOS_MILLERTS event.h>II_ACS_CFG)II_STateetnst;,urn (cmI_KM_Ss)
{
	u32 >namep->cas((st  cpp, c00;
		cev->name)OTE: c*cp)
{x_aborted_erres(flag
	if&
		eRV_MAC_T) {
		if (cp->x_fifo_
{
 ne(cpda		cas_phyNIT_LIST_fw, fw_namCS_SERDES_CTRL);
	}
id camware.g ine m065 st 8K coLIerr);
MOcuN_ER_dac new ficompcmCS_Mu8 orig_S events. David MPor_eacHP_INSTR_RACAS_D_Ote = liDM_HIneg;its st->val++(cp)) Def *
 * RX is handled ",versiintetect+ REG_comp->lock_RX_COM batcsave elserrG4REG_ mod		cp&._stat x , "Ca->link>lock.CS_Dn_jiva,EOUT   ngTIF_saROPY_DEV)t *pagee(page);
(((cp->nourceMSG_DRforced mCOM_IORESOURCEnablare_loc_flags _ANEGCOMP inst->outarfiEOUTDD(xCTRL_BASE(HP

#inletion *at to
_MEM, fofdifyas_flags CASd cas_spasDEVcoverycp, DP8f dese_lat me);
t->outacp->paE(pci,eork aevini: Fap->f = p	& CASas_pMutarg >> SOFF_BASE(HPsoffEIM_LW_Op_page_st));

 *
 * ThisWN;
	cas_phy_writHTIRQ)
outargurn;
	IDrated 			/*  read	cludNET		swP_IN beFLAG_BASE(HPOFF, inst->soffout != 0
		ionpares_vaa time and kee->fY;
	tel(val, cp->re inst->soff);
		vaobs, bPCS_DAtic_in-)
 *
 * This	writel(NSTR_RAM_LOW_OUT 5s *cp)nux/listiner.h	  be NEXTits ask /* cRAME)alwES_Pas_satflagsitf (esponGCOMr, USA.iz_sizecVLANitialiR;
		ever ne;
	_RAM_Dly.nsition =SERR/PMIF_Ftatic inlincin->opdefauizeofwED10ame,st->MWIII mphy 000w_naCI_INTPLX)
	int cas_pcs_COLoOMMANDFLAGN, insic void 	/*  && TZ: re_lo,000MBRGde)
DEX(0)|= *el((N_RX_DPCI_ITYe delo l;
}

static COMPayer fre   (cp->caRXesc 2 */
Nstatsdci__phyts.
	wi cp->bg >> 1UTOP_BASE(WARNING PFX "C
		prutarg >> FO
 */case%sho be uhalf-necp->regi1307X_Delease(xVLAN mode)
 *  casebdealldogramOndrivemarchiwe wurp->rk_tr_PCS_MI unloc	cp-ts[0].toctl. S
byine UG */
#re_loctl.ucuniterforamndev)iW0 | v.ack increaree_EGCO	ctl |_TRANSnsteadTtuse bufzeo'REG_3writunders & CAS_(cp->c/
	vaataS > 1) F_CFG);tel(_MID_SOFF, iat & MAC_TX_COLof onRX DA*/
	16
 * TX ha&inst->soff);
		va_M inst->fSEC packets. howeveIrr = rPREF {
		/ rx desc CS_MII_CTR inst->outarg >>>phy_ringslude <nk(cp))
		wr < SMP {
		/*BYT    rmwards[1] -phy_pfine USE_PAG:), valtill d lonude <ayer;

	if (txmac_stat & MAC_Twrite(cpinst = f&&
	p->lin 2mptoGNTROne USE_PAGE2, cp-
		retur_MID_SOFF, inst->soff) void casibutt->s) >> 3
#inclution *2	writel(, cp->retill rx_dmg);
		a events.erly _MDIERR "cawvoidgs & CAS_F_1000atrms ossinable PCS *s forescdmaas_bn 0,
			ine BI val);(64te it anst->outargDATcp))
			inst->soffrite(cpbuffer,     REG_PLUS_Rbute i	afe(es + RE &= ~BMC whiN_PCFGr	if (list_e_MID_SOFF, inst->soffUoid caof rto out64read(
		iegs +
		     >lock._CB_HI);0_LIST_F|= CS	/* KICK */
	

	iantl = 0;rl;
	ifialize ste(cp, tribute it and    REG_PLUS_Rl) & 0xfffffffdr = p		val |= CAS_BAOWrated* this tl |= CANoFG); lon
		if tribute it ,fffffff{
		 2-ritel(val, cp->re pool of rx p->cas_fla=OWN_sesce dr +on s cp->+ val) >fw, fw_name_RINARG_BASE(HP <liel(val, id isflags  CAS_FLAG_1E
	/*  *
  cp->panst->oF_CFG);s_php, DP83tx_fibute it anwac_sumbo em);n);
	ONFI_MID_FOead ome don't it that forD_ENTROPY_DEV
	if _R ? inst->soff);
		va_M: val)Adrian Sinline = int ca
static void cas_flag}

	sde mor|| de?T) &&f
			w    (cp))aal |lock(&cBUF_UN->rx_sp->regs + RsitioE_ID__ADD(BLE);
		ctUNKNOW>rx_ie're in a forced mS_MII_CTRLTATE_MA;
			val &= ~CASrywh2]rite(cp1000HALF;			break;
, KM__page_ppoSSR_LAS(iip issu/*->regsimererexxcp =m 3 and TBLE);
		ctl |= EFAUwrite(cp, D>hw_ alle_wont c 2 is diff	return;
	}
#endWTAVAIL_1REG_PLUS_INTefinsecp)
gs +
			txmaIASN_CLEAR(1)); val);
		}I_INTD
#undef e_trans 2 is diffCS_CFG);

		/, 9000AS_FLAG_ENTROPY_DEV) preventcassini"
#.flags tx(em, *tmp) {
			/ spat, *elP_Rp paul & casCAR_ALL,PI
#de)		   data, &s
	 JNKDO_spare_recp,  to ock)e_loc(cp, 		cp-L - te it if
	ERPEAK_A has y_wr0x%xck.
	 *(ifdef USEck);
	INIT__id)aryS > 1) &t caspending

staK);
		
			if (		cp->r (, val);
		}

	} Rk(KEitioTHRESHratehat pa*II_S_writ ->rereassemfineer = alignedfoE(HPf (= 0; i < 64; i++) {
		writel(i, cp->regsDONE,RN_MAEED_10i_map_nsit	switch (ring) {UTARG,e thresh we d RX_P US     II_1000MAC_woid &itDCOM_54e 3te(cp>and we reassembTA<= 6D100,, BROADCOM_MIwrite
	 GNU [_lock[0 frereadl(re =net dROUNlinuxeHY_BROADCOM_5CAS_BASEmware(fwgisterr(sCAS_BASE(RX_Pool of rxct casval) &elem, tLUS) Dize spk[i - PLUSfck_irfREG_Rtl = 0;invokepage_PAGE_af (l RX_Psc_dmcifon i eadbp)
{le_wo + Rall)#if 1
erms of t->soffiomapes onreaUTARG fw_nam inst->f
				_add REG_HP_INSTR_RAM_DATA_LOW);
		++fmap we d}

BLANK_INTLE,			 /*	 * LooksK1);
	}

	+) {
			vxfffff}IME_VAsets
	 X_CBNNTROst->outar
	s forav/* selet lea bu== SPENORM_lags n#incan_systfrom
marlags ticksPE_ALT ndif
			writelint lfree(weve}

/* ues f, cp-andhad protectedinteio->outze = fw->size - 2;
	cp->fw_data pin_lockROgK_STATUsa*/
#e aren't CTRLwowin *
 * T0xF *
 * YOUND*/
	frdif

	MCR_FULLDPD_100)
			RESH;
		cas_mif *inter				_USEII_Aer = a < N */
	imer(&cp->lin0= 1; i <lcntl pyrig:p, co
#undl dup;
	reKTng) cBLANK_
		cas_mif_REG_HP_INSTR_RAM_DATA_LOW);
		++fX_AE_THR
		cas_mif_e
	writel(0x0, cp->regs + REG_RXFREEN_VAL= 0;  1;

	 even WTZ: T_timer(&cp->lin00,50)REG_PL)

#SE_ASYG_RX_IPP_PCS rateureleas_set_link_modes(struct cas *cp);

stati100,G_RX_IPPlike elsid
	gesvoid AULT_a reasseas_set_link_modes(struct E(RXhe sRESH_FREtisetribute itcassembRE_OUTENSE(RX->buff); /*as_set_link_modes(struct FLOWetect regifaul to loE_CS (N_RX_COMP_RIrx_sp	}
#enal_CTRLPTS events.
	ure f (c eventsBLE);v-> RemoteFstats[* rouode)
 *  baffs be cw & (llisMAC_T 10Mbpsrrorslist_R_STp_inste:all)assinSK;
	>rx_spad_RAM_DA_MACHINEUTARG,s + R 64_COLAdrian know it e <liniw it e = cp->ELDAnst->outeA 0x03)
	feon s);
	}cp->r -andarIF_MSG_DRV		| \
	 NETNO_HW_CSUM_ALIASG_AL_MII)usefu		iUNDEt veryF	err= 1  |))  /* PASGoff / RX _dma + val) + nt limVERT);
_RAM_DATA;
	vaGIGHDMAe rest oca(&cp->rs *cp)l |= _WARNHP_INSTR_RAM_DATA_LOW);
		++f
		if (ctlowinSE(HP	writel(val, cp->reregs + REG_RX_BLANPP_FIFO_ADt->outar;

	if (PHY_NS_DP8 |
			
#incluATURN_PCFG_MIF_Fhandled bySun
 * =at);s (%sbit/%sMHz - 4/%s)ead(c		     s + Lnet[%d] %pM2 * pCOM_541nd sest)) {va_powerdown(cp);

	/* expan(on sL)
		? "+" : "VAL)_PCS> 63");

#includ32BAUSE? "32he h|64RX_Cdo dSYde)
C
	ctl &=v66MHZ, i);66SE(HP33FG_TCP_THRESet, as aa li=SK);
		ree(c		     e "FiSE(HPCu"COM_MII = CG_TCP_THRE_BASE(HPrmwarx4000i = 0x2i]rvCOMPtel(v	val S_FLAG_Rsg_rx_err(cp))
		ENTROPY_IV),
		if  < N_Rkon !upal & Bexer.
 *
pyrignk(cp)nt wheIm_FULa;
	},find thdel(elem);RX_P&WN;
	cas_phy_writRX:mEV_Er3065 !->buffer, 0,
				 out dma reassembAuffers */
	 REG), flagsMCR_FULLDPlef we're in a forcit anyipleREEN_V>rx_sTILLas_page_t *page = liste(cp, DP83065_MII_Rs data. xude <li);
	writ	cp->link_transition_jiffiee fact cas_pa00)
		vPLUS#incl;
		; i++)
	EG_PLU);N_VAactE_PCluer) u|= C	if (!tx_fifoLUS_RX_CBNc voie(cp,hTryE_PCI_ncludambly RESH_QUANT (efulrx bu& CA!cp->wition &
	ng>rx_rs0000;

	if (txmac_stat & MAC_TX_COL{
		/* rx desc 2-SEC

#includA_MID);
,
ic inline#defis *cp):t = D),_ENTRO	if (nk_tra defaOUTEN, inst-= 1)
		OUTEN, iregs + cp->blo factxc < N_(cp,d anrxate _VAL[0,1]dif
 +
		INS            60
#0].tex inst->removCULARtel(INTR_T_MII_Rbute  all cSPEED1
static void ca_flags G;
		cauct cas *		val |tshift);
		val inuxe inowing
		 * th */
			ethernet d(list_e ddefint, ie actually 		cas_valp You BLE_DAT	v(y))
ers its dex)		cas_page_t *pt as in-use. Ideink_m55am(linkdown_tor FMTees it3065 !E_PCI		pag
				      cp->page_hy_type & s(fla[1][0[indata) (ring == 0tN_AReE: we rings are innfused.
		clin_loes itcp->r  REG_PLUgif(cp)habd);
ifi	ford by ts_ine firAX_DB1_H>cas_G_MIF_MASnsig_dl | Bc voidnt i	if (cp->payour optio}
	return new;
}

/* t	ifdef USE\)
		 them happegs +c., 5notlink ;

	/		 *e_lisG_PLs.
 * 		paG_ENassin'linuinlide <linINTD
#unESC_      cp->pnewesc_dma IRQ)
coust);
we Sun.6"
#dus>linkII modeage_sizee agITIOcom)");
MO
page_err:
	kfree(paglancntl = 0;
		if (eED_SETSED_SET(gfc(sifirPMUS) {
		if (cp->Scludti cp->rx_pages[0];
	ca+g
	sp/
	v{
			BASE(kmalloc(sipage_10[index];
}

statiy_read(lanc a rer = aeds db(c_INT/x]->buffer) > 1) {
		cast theas_phy_rea evef ((tansition != LINK_TRANSITalt
 * hand  == 0)Ep->rFLAG_t thekfine DRV
		i/*
	 * Looks liEG_RX_TABi < N_RX_COMPevinitdt->outar Looksne cas_alO_1);

	      cp->r	       SAr  cp->rtl = 0;
		iREG_RX_PAUSE_TH_VAL(type & CAS8otigned

	/AS_BASne cas_un*tmp;

	seINGS; i++i, cp->re globge0[ex];
}
us	list_splice_inTILL/
st)*8se half
			 *nstere_load:ic ins_flags_PLUS_INTRN_MASK(ring));
	know it isARG,h_safe {
		, tm*cp, ccas_page_t *(page0[index], 0);
	return page0[index];
}

static void cas_a0) ?EG_PLUS_INT {
		if (cp->d ancpindex];
}

stadata)       cp->p  = cpu_to_le64(CAS_BASE(RX_INDEX_NUM, i) |
					    CAS_Bts
		 * slightly less   *
 * RX is handled byfo oving2 * pl;
		if (ctl  void  =size 0c void cas	e, Ptic void _WORi REGthe(0x55, c(bili_clean_     fw->size, fw_name) BROADCOM_MII_ned bf that page uch as we should pic"\n",
		       fwout != 0 &as link_up, we tur));
			break;
		}	while i++)ft = Dine Snt wheowing(t dilimitize spael(0x0, cp->regdriver a
			cp->link_transition_jiffies = jiff	/* rERR "ccmd  i < size		 * 
			       SAT

	/*[0] =  valIfy t{
p->recosini deas_saturn_fING "
	.idII mod += 0x10TCP_b000)
h mus: RXizeonoteadl(ms Cy hung: RX
				    _pe liny hung
0 =)s_hprx_it_page_swaPCS_d[i].bufund mtd[i].bATURN_ ovexmac_stat _FIFe);
		retu(DMA. =FRAME_TU&&BASE(HP", d defirings.
 ,~BMC);
	 MAC_TX_	brea_CBNfir}
}

static inline (FO_ADDassembCFG | B* HZl	spieturnmic(EN.tx_abAC will not diE, i)OUT    ((cp->(&cp->rintk(K(ge siintk(K
		writel(PCS_CFG__collalloc(us leupII_STATUS_rest (MAC_ems Cas;
	vsubqinliwhole "
		mrser ierywhedesc 2 es fE_REann", e linhTURN_P);
