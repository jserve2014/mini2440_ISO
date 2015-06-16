/******************************************************************************
**  Device driver for the PCI-SCSI NCR538XX controller family.
**
**  Copyright (C) 1994  Wolfgang Stanglmeier
**
**  This program is free software; you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation; either version 2 of the License, or
**  (at your option) any later version.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with this program; if not, write to the Free Software
**  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
**-----------------------------------------------------------------------------
**
**  This driver has been ported to Linux from the FreeBSD NCR53C8XX driver
**  and is currently maintained by
**
**          Gerard Roudier              <groudier@free.fr>
**
**  Being given that this driver originates from the FreeBSD version, and
**  in order to keep synergy on both, any suggested enhancements and corrections
**  received on Linux are automatically a potential candidate for the FreeBSD 
**  version.
**
**  The original driver has been written for 386bsd and FreeBSD by
**          Wolfgang Stanglmeier        <wolf@cologne.de>
**          Stefan Esser                <se@mi.Uni-Koeln.de>
**
**  And has been ported to NetBSD by
**          Charles M. Hannum           <mycroft@gnu.ai.mit.edu>
**
**-----------------------------------------------------------------------------
**
**                     Brief history
**
**  December 10 1995 by Gerard Roudier:
**     Initial port to Linux.
**
**  June 23 1996 by Gerard Roudier:
**     Support for 64 bits architectures (Alpha).
**
**  November 30 1996 by Gerard Roudier:
**     Support for Fast-20 scsi.
**     Support for large DMA fifo and 128 dwords bursting.
**
**  February 27 1997 by Gerard Roudier:
**     Support for Fast-40 scsi.
**     Support for on-Board RAM.
**
**  May 3 1997 by Gerard Roudier:
**     Full support for scsi scripts instructions pre-fetching.
**
**  May 19 1997 by Richard Waltham <dormouse@farsrobt.demon.co.uk>:
**     Support for NvRAM detection and reading.
**
**  August 18 1997 by Cort <cort@cs.nmt.edu>:
**     Support for Power/PC (Big Endian).
**
**  June 20 1998 by Gerard Roudier
**     Support for up to 64 tags per lun.
**     O(1) everywhere (C and SCRIPTS) for normal cases.
**     Low PCI traffic for command handling when on-chip RAM is present.
**     Aggressive SCSI SCRIPTS optimizations.
**
**  2005 by Matthew Wilcox and James Bottomley
**     PCI-ectomy.  This driver now supports only the 720 chip (see the
**     NCR_Q720 and zalon drivers for the bus probe logic).
**
*******************************************************************************
*/

/*
**	Supported SCSI-II features:
**	    Synchronous negotiation
**	    Wide negotiation        (depends on the NCR Chip)
**	    Enable disconnection
**	    Tagged command queuing
**	    Parity checking
**	    Etc...
**
**	Supported NCR/SYMBIOS chips:
**		53C720		(Wide,   Fast SCSI-2, intfly problems)
*/

/* Name and version of the driver */
#define SCSI_NCR_DRIVER_NAME	"ncr53c8xx-3.4.3g"

#define SCSI_NCR_DEBUG_FLAGS	(0)

#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/spinlock.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/types.h>

#include <asm/dma.h>
#include <asm/io.h>
#include <asm/system.h>

#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_dbg.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_tcq.h>
#include <scsi/scsi_transport.h>
#include <scsi/scsi_transport_spi.h>

#include "ncr53c8xx.h"

#define NAME53C8XX		"ncr53c8xx"

/*==========================================================
**
**	Debugging tags
**
**==========================================================
*/

#define DEBUG_ALLOC    (0x0001)
#define DEBUG_PHASE    (0x0002)
#define DEBUG_QUEUE    (0x0008)
#define DEBUG_RESULT   (0x0010)
#define DEBUG_POINTER  (0x0020)
#define DEBUG_SCRIPT   (0x0040)
#define DEBUG_TINY     (0x0080)
#define DEBUG_TIMING   (0x0100)
#define DEBUG_NEGO     (0x0200)
#define DEBUG_TAGS     (0x0400)
#define DEBUG_SCATTER  (0x0800)
#define DEBUG_IC        (0x1000)

/*
**    Enable/Disable debug messages.
**    Can be changed at runtime too.
*/

#ifdef SCSI_NCR_DEBUG_INFO_SUPPORT
static int ncr_debug = SCSI_NCR_DEBUG_FLAGS;
	#define DEBUG_FLAGS ncr_debug
#else
	#define DEBUG_FLAGS	SCSI_NCR_DEBUG_FLAGS
#endif

static inline struct list_head *ncr_list_pop(struct list_head *head)
{
	if (!list_empty(head)) {
		struct list_head *elem = head->next;

		list_del(elem);
		return elem;
	}

	return NULL;
}

/*==========================================================
**
**	Simple power of two buddy-like allocator.
**
**	This simple code is not intended to be fast, but to 
**	provide power of 2 aligned memory allocations.
**	Since the SCRIPTS processor only supplies 8 bit 
**	arithmetic, this allocator allows simple and fast 
**	address calculations  from the SCRIPTS code.
**	In addition, cache line alignment is guaranteed for 
**	power of 2 cache line size.
**	Enhanced in linux-2.3.44 to provide a memory pool 
**	per pcidev to support dynamic dma mapping. (I would 
**	have preferred a real bus abstraction, btw).
**
**==========================================================
*/

#define MEMO_SHIFT	4	/* 16 bytes minimum memory chunk */
#if PAGE_SIZE >= 8192
#define MEMO_PAGE_ORDER	0	/* 1 PAGE  maximum */
#else
#define MEMO_PAGE_ORDER	1	/* 2 PAGES maximum */
#endif
#define MEMO_FREE_UNUSED	/* Free unused pages immediately */
#define MEMO_WARN	1
#define MEMO_GFP_FLAGS	GFP_ATOMIC
#define MEMO_CLUSTER_SHIFT	(PAGE_SHIFT+MEMO_PAGE_ORDER)
#define MEMO_CLUSTER_SIZE	(1UL << MEMO_CLUSTER_SHIFT)
#define MEMO_CLUSTER_MASK	(MEMO_CLUSTER_SIZE-1)

typedef u_long m_addr_t;	/* Enough bits to bit-hack addresses */
typedef struct device *m_bush_t;	/* Something that addresses DMAable */

typedef struct m_link {		/* Link between free memory chunks */
	struct m_link *next;
} m_link_s;

typedef struct m_vtob {		/* Virtual to Bus address translation */
	struct m_vtob *next;
	m_addr_t vaddr;
	m_addr_t baddr;
} m_vtob_s;
#define VTOB_HASH_SHIFT		5
#define VTOB_HASH_SIZE		(1UL << VTOB_HASH_SHIFT)
#define VTOB_HASH_MASK		(VTOB_HASH_SIZE-1)
#define VTOB_HASH_CODE(m)	\
	((((m_addr_t) (m)) >> MEMO_CLUSTER_SHIFT) & VTOB_HASH_MASK)

typedef struct m_pool {		/* Memory pool of a given kind */
	m_bush_t bush;
	m_addr_t (*getp)(struct m_pool *);
	void (*freep)(struct m_pool *, m_addr_t);
	int nump;
	m_vtob_s *(vtob[VTOB_HASH_SIZE]);
	struct m_pool *next;
	struct m_link h[PAGE_SHIFT-MEMO_SHIFT+MEMO_PAGE_ORDER+1];
} m_pool_s;

static void *___m_alloc(m_pool_s *mp, int size)
{
	int i = 0;
	int s = (1 << MEMO_SHIFT);
	int j;
	m_addr_t a;
	m_link_s *h = mp->h;

	if (size > (PAGE_SIZE << MEMO_PAGE_ORDER))
		return NULL;

	while (size > s) {
		s <<= 1;
		++i;
	}

	j = i;
	while (!h[j].next) {
		if (s == (PAGE_SIZE << MEMO_PAGE_ORDER)) {
			h[j].next = (m_link_s *)mp->getp(mp);
			if (h[j].next)
				h[j].next->next = NULL;
			break;
		}
		++j;
		s <<= 1;
	}
	a = (m_addr_t) h[j].next;
	if (a) {
		h[j].next = h[j].next->next;
		while (j > i) {
			j -= 1;
			s >>= 1;
			h[j].next = (m_link_s *) (a+s);
			h[j].next->next = NULL;
		}
	}
#ifdef DEBUG
	printk("___m_alloc(%d) = %p\n", size, (void *) a);
#endif
	return (void *) a;
}

static void ___m_free(m_pool_s *mp, void *ptr, int size)
{
	int i = 0;
	int s = (1 << MEMO_SHIFT);
	m_link_s *q;
	m_addr_t a, b;
	m_link_s *h = mp->h;

#ifdef DEBUG
	printk("___m_free(%p, %d)\n", ptr, size);
#endif

	if (size > (PAGE_SIZE << MEMO_PAGE_ORDER))
		return;

	while (size > s) {
		s <<= 1;
		++i;
	}

	a = (m_addr_t) ptr;

	while (1) {
#ifdef MEMO_FREE_UNUSED
		if (s == (PAGE_SIZE << MEMO_PAGE_ORDER)) {
			mp->freep(mp, a);
			break;
		}
#endif
		b = a ^ s;
		q = &h[i];
		while (q->next && q->next != (m_link_s *) b) {
			q = q->next;
		}
		if (!q->next) {
			((m_link_s *) a)->next = h[i].next;
			h[i].next = (m_link_s *) a;
			break;
		}
		q->next = q->next->next;
		a = a & b;
		s <<= 1;
		++i;
	}
}

static DEFINE_SPINLOCK(ncr53c8xx_lock);

static void *__m_calloc2(m_pool_s *mp, int size, char *name, int uflags)
{
	void *p;

	p = ___m_alloc(mp, size);

	if (DEBUG_FLAGS & DEBUG_ALLOC)
		printk ("new %-10s[%4d] @%p.\n", name, size, p);

	if (p)
		memset(p, 0, size);
	else if (uflags & MEMO_WARN)
		printk (NAME53C8XX ": failed to allocate %s[%d]\n", name, size);

	return p;
}

#define __m_calloc(mp, s, n)	__m_calloc2(mp, s, n, MEMO_WARN)

static void __m_free(m_pool_s *mp, void *ptr, int size, char *name)
{
	if (DEBUG_FLAGS & DEBUG_ALLOC)
		printk ("freeing %-10s[%4d] @%p.\n", name, size, ptr);

	___m_free(mp, ptr, size);

}

/*
 * With pci bus iommu support, we use a default pool of unmapped memory 
 * for memory we donnot need to DMA from/to and one pool per pcidev for 
 * memory accessed by the PCI chip. `mp0' is the default not DMAable pool.
 */

static m_addr_t ___mp0_getp(m_pool_s *mp)
{
	m_addr_t m = __get_free_pages(MEMO_GFP_FLAGS, MEMO_PAGE_ORDER);
	if (m)
		++mp->nump;
	return m;
}

static void ___mp0_freep(m_pool_s *mp, m_addr_t m)
{
	free_pages(m, MEMO_PAGE_ORDER);
	--mp->nump;
}

static m_pool_s mp0 = {NULL, ___mp0_getp, ___mp0_freep};

/*
 * DMAable pools.
 */

/*
 * With pci bus iommu support, we maintain one pool per pcidev and a 
 * hashed reverse table for virtual to bus physical address translations.
 */
static m_addr_t ___dma_getp(m_pool_s *mp)
{
	m_addr_t vp;
	m_vtob_s *vbp;

	vbp = __m_calloc(&mp0, sizeof(*vbp), "VTOB");
	if (vbp) {
		dma_addr_t daddr;
		vp = (m_addr_t) dma_alloc_coherent(mp->bush,
						PAGE_SIZE<<MEMO_PAGE_ORDER,
						&daddr, GFP_ATOMIC);
		if (vp) {
			int hc = VTOB_HASH_CODE(vp);
			vbp->vaddr = vp;
			vbp->baddr = daddr;
			vbp->next = mp->vtob[hc];
			mp->vtob[hc] = vbp;
			++mp->nump;
			return vp;
		}
	}
	if (vbp)
		__m_free(&mp0, vbp, sizeof(*vbp), "VTOB");
	return 0;
}

static void ___dma_freep(m_pool_s *mp, m_addr_t m)
{
	m_vtob_s **vbpp, *vbp;
	int hc = VTOB_HASH_CODE(m);

	vbpp = &mp->vtob[hc];
	while (*vbpp && (*vbpp)->vaddr != m)
		vbpp = &(*vbpp)->next;
	if (*vbpp) {
		vbp = *vbpp;
		*vbpp = (*vbpp)->next;
		dma_free_coherent(mp->bush, PAGE_SIZE<<MEMO_PAGE_ORDER,
				  (void *)vbp->vaddr, (dma_addr_t)vbp->baddr);
		__m_free(&mp0, vbp, sizeof(*vbp), "VTOB");
		--mp->nump;
	}
}

static inline m_pool_s *___get_dma_pool(m_bush_t bush)
{
	m_pool_s *mp;
	for (mp = mp0.next; mp && mp->bush != bush; mp = mp->next);
	return mp;
}

static m_pool_s *___cre_dma_pool(m_bush_t bush)
{
	m_pool_s *mp;
	mp = __m_calloc(&mp0, sizeof(*mp), "MPOOL");
	if (mp) {
		memset(mp, 0, sizeof(*mp));
		mp->bush = bush;
		mp->getp = ___dma_getp;
		mp->freep = ___dma_freep;
		mp->next = mp0.next;
		mp0.next = mp;
	}
	return mp;
}

static void ___del_dma_pool(m_pool_s *p)
{
	struct m_pool **pp = &mp0.next;

	while (*pp && *pp != p)
		pp = &(*pp)->next;
	if (*pp) {
		*pp = (*pp)->next;
		__m_free(&mp0, p, sizeof(*p), "MPOOL");
	}
}

static void *__m_calloc_dma(m_bush_t bush, int size, char *name)
{
	u_long flags;
	struct m_pool *mp;
	void *m = NULL;

	spin_lock_irqsave(&ncr53c8xx_lock, flags);
	mp = ___get_dma_pool(bush);
	if (!mp)
		mp = ___cre_dma_pool(bush);
	if (mp)
		m = __m_calloc(mp, size, name);
	if (mp && !mp->nump)
		___del_dma_pool(mp);
	spin_unlock_irqrestore(&ncr53c8xx_lock, flags);

	return m;
}

static void __m_free_dma(m_bush_t bush, void *m, int size, char *name)
{
	u_long flags;
	struct m_pool *mp;

	spin_lock_irqsave(&ncr53c8xx_lock, flags);
	mp = ___get_dma_pool(bush);
	if (mp)
		__m_free(mp, m, size, name);
	if (mp && !mp->nump)
		___del_dma_pool(mp);
	spin_unlock_irqrestore(&ncr53c8xx_lock, flags);
}

static m_addr_t __vtobus(m_bush_t bush, void *m)
{
	u_long flags;
	m_pool_s *mp;
	int hc = VTOB_HASH_CODE(m);
	m_vtob_s *vp = NULL;
	m_addr_t a = ((m_addr_t) m) & ~MEMO_CLUSTER_MASK;

	spin_lock_irqsave(&ncr53c8xx_lock, flags);
	mp = ___get_dma_pool(bush);
	if (mp) {
		vp = mp->vtob[hc];
		while (vp && (m_addr_t) vp->vaddr != a)
			vp = vp->next;
	}
	spin_unlock_irqrestore(&ncr53c8xx_lock, flags);
	return vp ? vp->baddr + (((m_addr_t) m) - a) : 0;
}

#define _m_calloc_dma(np, s, n)		__m_calloc_dma(np->dev, s, n)
#define _m_free_dma(np, p, s, n)	__m_free_dma(np->dev, p, s, n)
#define m_calloc_dma(s, n)		_m_calloc_dma(np, s, n)
#define m_free_dma(p, s, n)		_m_free_dma(np, p, s, n)
#define _vtobus(np, p)			__vtobus(np->dev, p)
#define vtobus(p)			_vtobus(np, p)

/*
 *  Deal with DMA mapping/unmapping.
 */

/* To keep track of the dma mapping (sg/single) that has been set */
#define __data_mapped	SCp.phase
#define __data_mapping	SCp.have_data_in

static void __unmap_scsi_data(struct device *dev, struct scsi_cmnd *cmd)
{
	switch(cmd->__data_mapped) {
	case 2:
		scsi_dma_unmap(cmd);
		break;
	}
	cmd->__data_mapped = 0;
}

static int __map_scsi_sg_data(struct device *dev, struct scsi_cmnd *cmd)
{
	int use_sg;

	use_sg = scsi_dma_map(cmd);
	if (!use_sg)
		return 0;

	cmd->__data_mapped = 2;
	cmd->__data_mapping = use_sg;

	return use_sg;
}

#define unmap_scsi_data(np, cmd)	__unmap_scsi_data(np->dev, cmd)
#define map_scsi_sg_data(np, cmd)	__map_scsi_sg_data(np->dev, cmd)

/*==========================================================
**
**	Driver setup.
**
**	This structure is initialized from linux config 
**	options. It can be overridden at boot-up by the boot 
**	command line.
**
**==========================================================
*/
static struct ncr_driver_setup
	driver_setup			= SCSI_NCR_DRIVER_SETUP;

#ifndef MODULE
#ifdef	SCSI_NCR_BOOT_COMMAND_LINE_SUPPORT
static struct ncr_driver_setup
	driver_safe_setup __initdata	= SCSI_NCR_DRIVER_SAFE_SETUP;
#endif
#endif /* !MODULE */

#define initverbose (driver_setup.verbose)
#define bootverbose (np->verbose)


/*===================================================================
**
**	Driver setup from the boot command line
**
**===================================================================
*/

#ifdef MODULE
#define	ARG_SEP	' '
#else
#define	ARG_SEP	','
#endif

#define OPT_TAGS		1
#define OPT_MASTER_PARITY	2
#define OPT_SCSI_PARITY		3
#define OPT_DISCONNECTION	4
#define OPT_SPECIAL_FEATURES	5
#define OPT_UNUSED_1		6
#define OPT_FORCE_SYNC_NEGO	7
#define OPT_REVERSE_PROBE	8
#define OPT_DEFAULT_SYNC	9
#define OPT_VERBOSE		10
#define OPT_DEBUG		11
#define OPT_BURST_MAX		12
#define OPT_LED_PIN		13
#define OPT_MAX_WIDE		14
#define OPT_SETTLE_DELAY	15
#define OPT_DIFF_SUPPORT	16
#define OPT_IRQM		17
#define OPT_PCI_FIX_UP		18
#define OPT_BUS_CHECK		19
#define OPT_OPTIMIZE		20
#define OPT_RECOVERY		21
#define OPT_SAFE_SETUP		22
#define OPT_USE_NVRAM		23
#define OPT_EXCLUDE		24
#define OPT_HOST_ID		25

#ifdef SCSI_NCR_IARB_SUPPORT
#define OPT_IARB		26
#endif

#ifdef MODULE
#define	ARG_SEP	' '
#else
#define	ARG_SEP	','
#endif

#ifndef MODULE
static char setup_token[] __initdata = 
	"tags:"   "mpar:"
	"spar:"   "disc:"
	"specf:"  "ultra:"
	"fsn:"    "revprob:"
	"sync:"   "verb:"
	"debug:"  "burst:"
	"led:"    "wide:"
	"settle:" "diff:"
	"irqm:"   "pcifix:"
	"buschk:" "optim:"
	"recovery:"
	"safe:"   "nvram:"
	"excl:"   "hostid:"
#ifdef SCSI_NCR_IARB_SUPPORT
	"iarb:"
#endif
	;	/* DONNOT REMOVE THIS ';' */

static int __init get_setup_token(char *p)
{
	char *cur = setup_token;
	char *pc;
	int i = 0;

	while (cur != NULL && (pc = strchr(cur, ':')) != NULL) {
		++pc;
		++i;
		if (!strncmp(p, cur, pc - cur))
			return i;
		cur = pc;
	}
	return 0;
}

static int __init sym53c8xx__setup(char *str)
{
#ifdef SCSI_NCR_BOOT_COMMAND_LINE_SUPPORT
	char *cur = str;
	char *pc, *pv;
	int i, val, c;
	int xi = 0;

	while (cur != NULL && (pc = strchr(cur, ':')) != NULL) {
		char *pe;

		val = 0;
		pv = pc;
		c = *++pv;

		if	(c == 'n')
			val = 0;
		else if	(c == 'y')
			val = 1;
		else
			val = (int) simple_strtoul(pv, &pe, 0);

		switch (get_setup_token(cur)) {
		case OPT_TAGS:
			driver_setup.default_tags = val;
			if (pe && *pe == '/') {
				i = 0;
				while (*pe && *pe != ARG_SEP && 
					i < sizeof(driver_setup.tag_ctrl)-1) {
					driver_setup.tag_ctrl[i++] = *pe++;
				}
				driver_setup.tag_ctrl[i] = '\0';
			}
			break;
		case OPT_MASTER_PARITY:
			driver_setup.master_parity = val;
			break;
		case OPT_SCSI_PARITY:
			driver_setup.scsi_parity = val;
			break;
		case OPT_DISCONNECTION:
			driver_setup.disconnection = val;
			break;
		case OPT_SPECIAL_FEATURES:
			driver_setup.special_features = val;
			break;
		case OPT_FORCE_SYNC_NEGO:
			driver_setup.force_sync_nego = val;
			break;
		case OPT_REVERSE_PROBE:
			driver_setup.reverse_probe = val;
			break;
		case OPT_DEFAULT_SYNC:
			driver_setup.default_sync = val;
			break;
		case OPT_VERBOSE:
			driver_setup.verbose = val;
			break;
		case OPT_DEBUG:
			driver_setup.debug = val;
			break;
		case OPT_BURST_MAX:
			driver_setup.burst_max = val;
			break;
		case OPT_LED_PIN:
			driver_setup.led_pin = val;
			break;
		case OPT_MAX_WIDE:
			driver_setup.max_wide = val? 1:0;
			break;
		case OPT_SETTLE_DELAY:
			driver_setup.settle_delay = val;
			break;
		case OPT_DIFF_SUPPORT:
			driver_setup.diff_support = val;
			break;
		case OPT_IRQM:
			driver_setup.irqm = val;
			break;
		case OPT_PCI_FIX_UP:
			driver_setup.pci_fix_up	= val;
			break;
		case OPT_BUS_CHECK:
			driver_setup.bus_check = val;
			break;
		case OPT_OPTIMIZE:
			driver_setup.optimize = val;
			break;
		case OPT_RECOVERY:
			driver_setup.recovery = val;
			break;
		case OPT_USE_NVRAM:
			driver_setup.use_nvram = val;
			break;
		case OPT_SAFE_SETUP:
			memcpy(&driver_setup, &driver_safe_setup,
				sizeof(driver_setup));
			break;
		case OPT_EXCLUDE:
			if (xi < SCSI_NCR_MAX_EXCLUDES)
				driver_setup.excludes[xi++] = val;
			break;
		case OPT_HOST_ID:
			driver_setup.host_id = val;
			break;
#ifdef SCSI_NCR_IARB_SUPPORT
		case OPT_IARB:
			driver_setup.iarb = val;
			break;
#endif
		default:
			printk("sym53c8xx_setup: unexpected boot option '%.*s' ignored\n", (int)(pc-cur+1), cur);
			break;
		}

		if ((cur = strchr(cur, ARG_SEP)) != NULL)
			++cur;
	}
#endif /* SCSI_NCR_BOOT_COMMAND_LINE_SUPPORT */
	return 1;
}
#endif /* !MODULE */

/*===================================================================
**
**	Get device queue depth from boot command line.
**
**===================================================================
*/
#define DEF_DEPTH	(driver_setup.default_tags)
#define ALL_TARGETS	-2
#define NO_TARGET	-1
#define ALL_LUNS	-2
#define NO_LUN		-1

static int device_queue_depth(int unit, int target, int lun)
{
	int c, h, t, u, v;
	char *p = driver_setup.tag_ctrl;
	char *ep;

	h = -1;
	t = NO_TARGET;
	u = NO_LUN;
	while ((c = *p++) != 0) {
		v = simple_strtoul(p, &ep, 0);
		switch(c) {
		case '/':
			++h;
			t = ALL_TARGETS;
			u = ALL_LUNS;
			break;
		case 't':
			if (t != target)
				t = (target == v) ? v : NO_TARGET;
			u = ALL_LUNS;
			break;
		case 'u':
			if (u != lun)
				u = (lun == v) ? v : NO_LUN;
			break;
		case 'q':
			if (h == unit &&
				(t == ALL_TARGETS || t == target) &&
				(u == ALL_LUNS    || u == lun))
				return v;
			break;
		case '-':
			t = ALL_TARGETS;
			u = ALL_LUNS;
			break;
		default:
			break;
		}
		p = ep;
	}
	return DEF_DEPTH;
}


/*==========================================================
**
**	The CCB done queue uses an array of CCB virtual 
**	addresses. Empty entries are flagged using the bogus 
**	virtual address 0xffffffff.
**
**	Since PCI ensures that only aligned DWORDs are accessed 
**	atomically, 64 bit little-endian architecture requires 
**	to test the high order DWORD of the entry to determine 
**	if it is empty or valid.
**
**	BTW, I will make things differently as soon as I will 
**	have a better idea, but this is simple and should work.
**
**==========================================================
*/
 
#define SCSI_NCR_CCB_DONE_SUPPORT
#ifdef  SCSI_NCR_CCB_DONE_SUPPORT

#define MAX_DONE 24
#define CCB_DONE_EMPTY 0xffffffffUL

/* All 32 bit architectures */
#if BITS_PER_LONG == 32
#define CCB_DONE_VALID(cp)  (((u_long) cp) != CCB_DONE_EMPTY)

/* All > 32 bit (64 bit) architectures regardless endian-ness */
#else
#define CCB_DONE_VALID(cp)  \
	((((u_long) cp) & 0xffffffff00000000ul) && 	\
	 (((u_long) cp) & 0xfffffffful) != CCB_DONE_EMPTY)
#endif

#endif /* SCSI_NCR_CCB_DONE_SUPPORT */

/*==========================================================
**
**	Configuration and Debugging
**
**==========================================================
*/

/*
**    SCSI address of this device.
**    The boot routines should have set it.
**    If not, use this.
*/

#ifndef SCSI_NCR_MYADDR
#define SCSI_NCR_MYADDR      (7)
#endif

/*
**    The maximum number of tags per logic unit.
**    Used only for disk devices that support tags.
*/

#ifndef SCSI_NCR_MAX_TAGS
#define SCSI_NCR_MAX_TAGS    (8)
#endif

/*
**    TAGS are actually limited to 64 tags/lun.
**    We need to deal with power of 2, for alignment constraints.
*/
#if	SCSI_NCR_MAX_TAGS > 64
#define	MAX_TAGS (64)
#else
#define	MAX_TAGS SCSI_NCR_MAX_TAGS
#endif

#define NO_TAG	(255)

/*
**	Choose appropriate type for tag bitmap.
*/
#if	MAX_TAGS > 32
typedef u64 tagmap_t;
#else
typedef u32 tagmap_t;
#endif

/*
**    Number of targets supported by the driver.
**    n permits target numbers 0..n-1.
**    Default is 16, meaning targets #0..#15.
**    #7 .. is myself.
*/

#ifdef SCSI_NCR_MAX_TARGET
#define MAX_TARGET  (SCSI_NCR_MAX_TARGET)
#else
#define MAX_TARGET  (16)
#endif

/*
**    Number of logic units supported by the driver.
**    n enables logic unit numbers 0..n-1.
**    The common SCSI devices require only
**    one lun, so take 1 as the default.
*/

#ifdef SCSI_NCR_MAX_LUN
#define MAX_LUN    SCSI_NCR_MAX_LUN
#else
#define MAX_LUN    (1)
#endif

/*
**    Asynchronous pre-scaler (ns). Shall be 40
*/
 
#ifndef SCSI_NCR_MIN_ASYNC
#define SCSI_NCR_MIN_ASYNC (40)
#endif

/*
**    The maximum number of jobs scheduled for starting.
**    There should be one slot per target, and one slot
**    for each tag of each target in use.
**    The calculation below is actually quite silly ...
*/

#ifdef SCSI_NCR_CAN_QUEUE
#define MAX_START   (SCSI_NCR_CAN_QUEUE + 4)
#else
#define MAX_START   (MAX_TARGET + 7 * MAX_TAGS)
#endif

/*
**   We limit the max number of pending IO to 250.
**   since we donnot want to allocate more than 1 
**   PAGE for 'scripth'.
*/
#if	MAX_START > 250
#undef	MAX_START
#define	MAX_START 250
#endif

/*
**    The maximum number of segments a transfer is split into.
**    We support up to 127 segments for both read and write.
**    The data scripts are broken into 2 sub-scripts.
**    80 (MAX_SCATTERL) segments are moved from a sub-script
**    in on-chip RAM. This makes data transfers shorter than 
**    80k (assuming 1k fs) as fast as possible.
*/

#define MAX_SCATTER (SCSI_NCR_MAX_SCATTER)

#if (MAX_SCATTER > 80)
#define MAX_SCATTERL	80
#define	MAX_SCATTERH	(MAX_SCATTER - MAX_SCATTERL)
#else
#define MAX_SCATTERL	(MAX_SCATTER-1)
#define	MAX_SCATTERH	1
#endif

/*
**	other
*/

#define NCR_SNOOP_TIMEOUT (1000000)

/*
**	Other definitions
*/

#define ScsiResult(host_code, scsi_code) (((host_code) << 16) + ((scsi_code) & 0x7f))

#define initverbose (driver_setup.verbose)
#define bootverbose (np->verbose)

/*==========================================================
**
**	Command control block states.
**
**==========================================================
*/

#define HS_IDLE		(0)
#define HS_BUSY		(1)
#define HS_NEGOTIATE	(2)	/* sync/wide data transfer*/
#define HS_DISCONNECT	(3)	/* Disconnected by target */

#define HS_DONEMASK	(0x80)
#define HS_COMPLETE	(4|HS_DONEMASK)
#define HS_SEL_TIMEOUT	(5|HS_DONEMASK)	/* Selection timeout      */
#define HS_RESET	(6|HS_DONEMASK)	/* SCSI reset	          */
#define HS_ABORTED	(7|HS_DONEMASK)	/* Transfer aborted       */
#define HS_TIMEOUT	(8|HS_DONEMASK)	/* Software timeout       */
#define HS_FAIL		(9|HS_DONEMASK)	/* SCSI or PCI bus errors */
#define HS_UNEXPECTED	(10|HS_DONEMASK)/* Unexpected disconnect  */

/*
**	Invalid host status values used by the SCRIPTS processor 
**	when the nexus is not fully identified.
**	Shall never appear in a CCB.
*/

#define HS_INVALMASK	(0x40)
#define	HS_SELECTING	(0|HS_INVALMASK)
#define	HS_IN_RESELECT	(1|HS_INVALMASK)
#define	HS_STARTING	(2|HS_INVALMASK)

/*
**	Flags set by the SCRIPT processor for commands 
**	that have been skipped.
*/
#define HS_SKIPMASK	(0x20)

/*==========================================================
**
**	Software Interrupt Codes
**
**==========================================================
*/

#define	SIR_BAD_STATUS		(1)
#define	SIR_XXXXXXXXXX		(2)
#define	SIR_NEGO_SYNC		(3)
#define	SIR_NEGO_WIDE		(4)
#define	SIR_NEGO_FAILED		(5)
#define	SIR_NEGO_PROTO		(6)
#define	SIR_REJECT_RECEIVED	(7)
#define	SIR_REJECT_SENT		(8)
#define	SIR_IGN_RESIDUE		(9)
#define	SIR_MISSING_SAVE	(10)
#define	SIR_RESEL_NO_MSG_IN	(11)
#define	SIR_RESEL_NO_IDENTIFY	(12)
#define	SIR_RESEL_BAD_LUN	(13)
#define	SIR_RESEL_BAD_TARGET	(14)
#define	SIR_RESEL_BAD_I_T_L	(15)
#define	SIR_RESEL_BAD_I_T_L_Q	(16)
#define	SIR_DONE_OVERFLOW	(17)
#define	SIR_INTFLY		(18)
#define	SIR_MAX			(18)

/*==========================================================
**
**	Extended error codes.
**	xerr_status field of struct ccb.
**
**==========================================================
*/

#define	XE_OK		(0)
#define	XE_EXTRA_DATA	(1)	/* unexpected data phase */
#define	XE_BAD_PHASE	(2)	/* illegal phase (4/5)   */

/*==========================================================
**
**	Negotiation status.
**	nego_status field	of struct ccb.
**
**==========================================================
*/

#define NS_NOCHANGE	(0)
#define NS_SYNC		(1)
#define NS_WIDE		(2)
#define NS_PPR		(4)

/*==========================================================
**
**	Misc.
**
**==========================================================
*/

#define CCB_MAGIC	(0xf2691ad2)

/*==========================================================
**
**	Declaration of structs.
**
**==========================================================
*/

static struct scsi_transport_template *ncr53c8xx_transport_template = NULL;

struct tcb;
struct lcb;
struct ccb;
struct ncb;
struct script;

struct link {
	ncrcmd	l_cmd;
	ncrcmd	l_paddr;
};

struct	usrcmd {
	u_long	target;
	u_long	lun;
	u_long	data;
	u_long	cmd;
};

#define UC_SETSYNC      10
#define UC_SETTAGS	11
#define UC_SETDEBUG	12
#define UC_SETORDER	13
#define UC_SETWIDE	14
#define UC_SETFLAG	15
#define UC_SETVERBOSE	17

#define	UF_TRACE	(0x01)
#define	UF_NODISC	(0x02)
#define	UF_NOSCAN	(0x04)

/*========================================================================
**
**	Declaration of structs:		target control block
**
**========================================================================
*/
struct tcb {
	/*----------------------------------------------------------------
	**	During reselection the ncr jumps to this point with SFBR 
	**	set to the encoded target number with bit 7 set.
	**	if it's not this target, jump to the next.
	**
	**	JUMP  IF (SFBR != #target#), @(next tcb)
	**----------------------------------------------------------------
	*/
	struct link   jump_tcb;

	/*----------------------------------------------------------------
	**	Load the actual values for the sxfer and the scntl3
	**	register (sync/wide mode).
	**
	**	SCR_COPY (1), @(sval field of this tcb), @(sxfer  register)
	**	SCR_COPY (1), @(wval field of this tcb), @(scntl3 register)
	**----------------------------------------------------------------
	*/
	ncrcmd	getscr[6];

	/*----------------------------------------------------------------
	**	Get the IDENTIFY message and load the LUN to SFBR.
	**
	**	CALL, <RESEL_LUN>
	**----------------------------------------------------------------
	*/
	struct link   call_lun;

	/*----------------------------------------------------------------
	**	Now look for the right lun.
	**
	**	For i = 0 to 3
	**		SCR_JUMP ^ IFTRUE(MASK(i, 3)), @(first lcb mod. i)
	**
	**	Recent chips will prefetch the 4 JUMPS using only 1 burst.
	**	It is kind of hashcoding.
	**----------------------------------------------------------------
	*/
	struct link     jump_lcb[4];	/* JUMPs for reselection	*/
	struct lcb *	lp[MAX_LUN];	/* The lcb's of this tcb	*/

	/*----------------------------------------------------------------
	**	Pointer to the ccb used for negotiation.
	**	Prevent from starting a negotiation for all queued commands 
	**	when tagged command queuing is enabled.
	**----------------------------------------------------------------
	*/
	struct ccb *   nego_cp;

	/*----------------------------------------------------------------
	**	statistical data
	**----------------------------------------------------------------
	*/
	u_long	transfers;
	u_long	bytes;

	/*----------------------------------------------------------------
	**	negotiation of wide and synch transfer and device quirks.
	**----------------------------------------------------------------
	*/
#ifdef SCSI_NCR_BIG_ENDIAN
/*0*/	u16	period;
/*2*/	u_char	sval;
/*3*/	u_char	minsync;
/*0*/	u_char	wval;
/*1*/	u_char	widedone;
/*2*/	u_char	quirks;
/*3*/	u_char	maxoffs;
#else
/*0*/	u_char	minsync;
/*1*/	u_char	sval;
/*2*/	u16	period;
/*0*/	u_char	maxoffs;
/*1*/	u_char	quirks;
/*2*/	u_char	widedone;
/*3*/	u_char	wval;
#endif

	/* User settable limits and options.  */
	u_char	usrsync;
	u_char	usrwide;
	u_char	usrtags;
	u_char	usrflag;
	struct scsi_target *starget;
};

/*========================================================================
**
**	Declaration of structs:		lun control block
**
**========================================================================
*/
struct lcb {
	/*----------------------------------------------------------------
	**	During reselection the ncr jumps to this point
	**	with SFBR set to the "Identify" message.
	**	if it's not this lun, jump to the next.
	**
	**	JUMP  IF (SFBR != #lun#), @(next lcb of this target)
	**
	**	It is this lun. Load TEMP with the nexus jumps table 
	**	address and jump to RESEL_TAG (or RESEL_NOTAG).
	**
	**		SCR_COPY (4), p_jump_ccb, TEMP,
	**		SCR_JUMP, <RESEL_TAG>
	**----------------------------------------------------------------
	*/
	struct link	jump_lcb;
	ncrcmd		load_jump_ccb[3];
	struct link	jump_tag;
	ncrcmd		p_jump_ccb;	/* Jump table bus address	*/

	/*----------------------------------------------------------------
	**	Jump table used by the script processor to directly jump 
	**	to the CCB corresponding to the reselected nexus.
	**	Address is allocated on 256 bytes boundary in order to 
	**	allow 8 bit calculation of the tag jump entry for up to 
	**	64 possible tags.
	**----------------------------------------------------------------
	*/
	u32		jump_ccb_0;	/* Default table if no tags	*/
	u32		*jump_ccb;	/* Virtual address		*/

	/*----------------------------------------------------------------
	**	CCB queue management.
	**----------------------------------------------------------------
	*/
	struct list_head free_ccbq;	/* Queue of available CCBs	*/
	struct list_head busy_ccbq;	/* Queue of busy CCBs		*/
	struct list_head wait_ccbq;	/* Queue of waiting for IO CCBs	*/
	struct list_head skip_ccbq;	/* Queue of skipped CCBs	*/
	u_char		actccbs;	/* Number of allocated CCBs	*/
	u_char		busyccbs;	/* CCBs busy for this lun	*/
	u_char		queuedccbs;	/* CCBs queued to the controller*/
	u_char		queuedepth;	/* Queue depth for this lun	*/
	u_char		scdev_depth;	/* SCSI device queue depth	*/
	u_char		maxnxs;		/* Max possible nexuses		*/

	/*----------------------------------------------------------------
	**	Control of tagged command queuing.
	**	Tags allocation is performed using a circular buffer.
	**	This avoids using a loop for tag allocation.
	**----------------------------------------------------------------
	*/
	u_char		ia_tag;		/* Allocation index		*/
	u_char		if_tag;		/* Freeing index		*/
	u_char cb_tags[MAX_TAGS];	/* Circular tags buffer	*/
	u_char		usetags;	/* Command queuing is active	*/
	u_char		maxtags;	/* Max nr of tags asked by user	*/
	u_char		numtags;	/* Current number of tags	*/

	/*----------------------------------------------------------------
	**	QUEUE FULL control and ORDERED tag control.
	**----------------------------------------------------------------
	*/
	/*----------------------------------------------------------------
	**	QUEUE FULL and ORDERED tag control.
	**----------------------------------------------------------------
	*/
	u16		num_good;	/* Nr of GOOD since QUEUE FULL	*/
	tagmap_t	tags_umap;	/* Used tags bitmap		*/
	tagmap_t	tags_smap;	/* Tags in use at 'tag_stime'	*/
	u_long		tags_stime;	/* Last time we set smap=umap	*/
	struct ccb *	held_ccb;	/* CCB held for QUEUE FULL	*/
};

/*========================================================================
**
**      Declaration of structs:     the launch script.
**
**========================================================================
**
**	It is part of the CCB and is called by the scripts processor to 
**	start or restart the data structure (nexus).
**	This 6 DWORDs mini script makes use of prefetching.
**
**------------------------------------------------------------------------
*/
struct launch {
	/*----------------------------------------------------------------
	**	SCR_COPY(4),	@(p_phys), @(dsa register)
	**	SCR_JUMP,	@(scheduler_point)
	**----------------------------------------------------------------
	*/
	ncrcmd		setup_dsa[3];	/* Copy 'phys' address to dsa	*/
	struct link	schedule;	/* Jump to scheduler point	*/
	ncrcmd		p_phys;		/* 'phys' header bus address	*/
};

/*========================================================================
**
**      Declaration of structs:     global HEADER.
**
**========================================================================
**
**	This substructure is copied from the ccb to a global address after 
**	selection (or reselection) and copied back before disconnect.
**
**	These fields are accessible to the script processor.
**
**------------------------------------------------------------------------
*/

struct head {
	/*----------------------------------------------------------------
	**	Saved data pointer.
	**	Points to the position in the script responsible for the
	**	actual transfer transfer of data.
	**	It's written after reception of a SAVE_DATA_POINTER message.
	**	The goalpointer points after the last transfer command.
	**----------------------------------------------------------------
	*/
	u32		savep;
	u32		lastp;
	u32		goalp;

	/*----------------------------------------------------------------
	**	Alternate data pointer.
	**	They are copied back to savep/lastp/goalp by the SCRIPTS 
	**	when the direction is unknown and the device claims data out.
	**----------------------------------------------------------------
	*/
	u32		wlastp;
	u32		wgoalp;

	/*----------------------------------------------------------------
	**	The virtual address of the ccb containing this header.
	**----------------------------------------------------------------
	*/
	struct ccb *	cp;

	/*----------------------------------------------------------------
	**	Status fields.
	**----------------------------------------------------------------
	*/
	u_char		scr_st[4];	/* script status		*/
	u_char		status[4];	/* host status. must be the 	*/
					/*  last DWORD of the header.	*/
};

/*
**	The status bytes are used by the host and the script processor.
**
**	The byte corresponding to the host_status must be stored in the 
**	last DWORD of the CCB header since it is used for command 
**	completion (ncr_wakeup()). Doing so, we are sure that the header 
**	has been entirely copied back to the CCB when the host_status is 
**	seen complete by the CPU.
**
**	The last four bytes (status[4]) are copied to the scratchb register
**	(declared as scr0..scr3 in ncr_reg.h) just after the select/reselect,
**	and copied back just after disconnecting.
**	Inside the script the XX_REG are used.
**
**	The first four bytes (scr_st[4]) are used inside the script by 
**	"COPY" commands.
**	Because source and destination must have the same alignment
**	in a DWORD, the fields HAVE to be at the chosen offsets.
**		xerr_st		0	(0x34)	scratcha
**		sync_st		1	(0x05)	sxfer
**		wide_st		3	(0x03)	scntl3
*/

/*
**	Last four bytes (script)
*/
#define  QU_REG	scr0
#define  HS_REG	scr1
#define  HS_PRT	nc_scr1
#define  SS_REG	scr2
#define  SS_PRT	nc_scr2
#define  PS_REG	scr3

/*
**	Last four bytes (host)
*/
#ifdef SCSI_NCR_BIG_ENDIAN
#define  actualquirks  phys.header.status[3]
#define  host_status   phys.header.status[2]
#define  scsi_status   phys.header.status[1]
#define  parity_status phys.header.status[0]
#else
#define  actualquirks  phys.header.status[0]
#define  host_status   phys.header.status[1]
#define  scsi_status   phys.header.status[2]
#define  parity_status phys.header.status[3]
#endif

/*
**	First four bytes (script)
*/
#define  xerr_st       header.scr_st[0]
#define  sync_st       header.scr_st[1]
#define  nego_st       header.scr_st[2]
#define  wide_st       header.scr_st[3]

/*
**	First four bytes (host)
*/
#define  xerr_status   phys.xerr_st
#define  nego_status   phys.nego_st

#if 0
#define  sync_status   phys.sync_st
#define  wide_status   phys.wide_st
#endif

/*==========================================================
**
**      Declaration of structs:     Data structure block
**
**==========================================================
**
**	During execution of a ccb by the script processor,
**	the DSA (data structure address) register points
**	to this substructure of the ccb.
**	This substructure contains the header with
**	the script-processor-changeable data and
**	data blocks for the indirect move commands.
**
**----------------------------------------------------------
*/

struct dsb {

	/*
	**	Header.
	*/

	struct head	header;

	/*
	**	Table data for Script
	*/

	struct scr_tblsel  select;
	struct scr_tblmove smsg  ;
	struct scr_tblmove cmd   ;
	struct scr_tblmove sense ;
	struct scr_tblmove data[MAX_SCATTER];
};


/*========================================================================
**
**      Declaration of structs:     Command control block.
**
**========================================================================
*/
struct ccb {
	/*----------------------------------------------------------------
	**	This is the data structure which is pointed by the DSA 
	**	register when it is executed by the script processor.
	**	It must be the first entry because it contains the header 
	**	as first entry that must be cache line aligned.
	**----------------------------------------------------------------
	*/
	struct dsb	phys;

	/*----------------------------------------------------------------
	**	Mini-script used at CCB execution start-up.
	**	Load the DSA with the data structure address (phys) and 
	**	jump to SELECT. Jump to CANCEL if CCB is to be canceled.
	**----------------------------------------------------------------
	*/
	struct launch	start;

	/*----------------------------------------------------------------
	**	Mini-script used at CCB relection to restart the nexus.
	**	Load the DSA with the data structure address (phys) and 
	**	jump to RESEL_DSA. Jump to ABORT if CCB is to be aborted.
	**----------------------------------------------------------------
	*/
	struct launch	restart;

	/*----------------------------------------------------------------
	**	If a data transfer phase is terminated too early
	**	(after reception of a message (i.e. DISCONNECT)),
	**	we have to prepare a mini script to transfer
	**	the rest of the data.
	**----------------------------------------------------------------
	*/
	ncrcmd		patch[8];

	/*----------------------------------------------------------------
	**	The general SCSI driver provides a
	**	pointer to a control block.
	**----------------------------------------------------------------
	*/
	struct scsi_cmnd	*cmd;		/* SCSI command 		*/
	u_char		cdb_buf[16];	/* Copy of CDB			*/
	u_char		sense_buf[64];
	int		data_len;	/* Total data length		*/

	/*----------------------------------------------------------------
	**	Message areas.
	**	We prepare a message to be sent after selection.
	**	We may use a second one if the command is rescheduled 
	**	due to GETCC or QFULL.
	**      Contents are IDENTIFY and SIMPLE_TAG.
	**	While negotiating sync or wide transfer,
	**	a SDTR or WDTR message is appended.
	**----------------------------------------------------------------
	*/
	u_char		scsi_smsg [8];
	u_char		scsi_smsg2[8];

	/*----------------------------------------------------------------
	**	Other fields.
	**----------------------------------------------------------------
	*/
	u_long		p_ccb;		/* BUS address of this CCB	*/
	u_char		sensecmd[6];	/* Sense command		*/
	u_char		tag;		/* Tag for this transfer	*/
					/*  255 means no tag		*/
	u_char		target;
	u_char		lun;
	u_char		queued;
	u_char		auto_sense;
	struct ccb *	link_ccb;	/* Host adapter CCB chain	*/
	struct list_head link_ccbq;	/* Link to unit CCB queue	*/
	u32		startp;		/* Initial data pointer		*/
	u_long		magic;		/* Free / busy  CCB flag	*/
};

#define CCB_PHYS(cp,lbl)	(cp->p_ccb + offsetof(struct ccb, lbl))


/*========================================================================
**
**      Declaration of structs:     NCR device descriptor
**
**========================================================================
*/
struct ncb {
	/*----------------------------------------------------------------
	**	The global header.
	**	It is accessible to both the host and the script processor.
	**	Must be cache line size aligned (32 for x86) in order to 
	**	allow cache line bursting when it is copied to/from CCB.
	**----------------------------------------------------------------
	*/
	struct head     header;

	/*----------------------------------------------------------------
	**	CCBs management queues.
	**----------------------------------------------------------------
	*/
	struct scsi_cmnd	*waiting_list;	/* Commands waiting for a CCB	*/
					/*  when lcb is not allocated.	*/
	struct scsi_cmnd	*done_list;	/* Commands waiting for done()  */
					/* callback to be invoked.      */ 
	spinlock_t	smp_lock;	/* Lock for SMP threading       */

	/*----------------------------------------------------------------
	**	Chip and controller indentification.
	**----------------------------------------------------------------
	*/
	int		unit;		/* Unit number			*/
	char		inst_name[16];	/* ncb instance name		*/

	/*----------------------------------------------------------------
	**	Initial value of some IO register bits.
	**	These values are assumed to have been set by BIOS, and may 
	**	be used for probing adapter implementation differences.
	**----------------------------------------------------------------
	*/
	u_char	sv_scntl0, sv_scntl3, sv_dmode, sv_dcntl, sv_ctest0, sv_ctest3,
		sv_ctest4, sv_ctest5, sv_gpcntl, sv_stest2, sv_stest4;

	/*----------------------------------------------------------------
	**	Actual initial value of IO register bits used by the 
	**	driver. They are loaded at initialisation according to  
	**	features that are to be enabled.
	**----------------------------------------------------------------
	*/
	u_char	rv_scntl0, rv_scntl3, rv_dmode, rv_dcntl, rv_ctest0, rv_ctest3,
		rv_ctest4, rv_ctest5, rv_stest2;

	/*----------------------------------------------------------------
	**	Targets management.
	**	During reselection the ncr jumps to jump_tcb.
	**	The SFBR register is loaded with the encoded target id.
	**	For i = 0 to 3
	**		SCR_JUMP ^ IFTRUE(MASK(i, 3)), @(next tcb mod. i)
	**
	**	Recent chips will prefetch the 4 JUMPS using only 1 burst.
	**	It is kind of hashcoding.
	**----------------------------------------------------------------
	*/
	struct link     jump_tcb[4];	/* JUMPs for reselection	*/
	struct tcb  target[MAX_TARGET];	/* Target data			*/

	/*----------------------------------------------------------------
	**	Virtual and physical bus addresses of the chip.
	**----------------------------------------------------------------
	*/
	void __iomem *vaddr;		/* Virtual and bus address of	*/
	unsigned long	paddr;		/*  chip's IO registers.	*/
	unsigned long	paddr2;		/* On-chip RAM bus address.	*/
	volatile			/* Pointer to volatile for 	*/
	struct ncr_reg	__iomem *reg;	/*  memory mapped IO.		*/

	/*----------------------------------------------------------------
	**	SCRIPTS virtual and physical bus addresses.
	**	'script'  is loaded in the on-chip RAM if present.
	**	'scripth' stays in main memory.
	**----------------------------------------------------------------
	*/
	struct script	*script0;	/* Copies of script and scripth	*/
	struct scripth	*scripth0;	/*  relocated for this ncb.	*/
	struct scripth	*scripth;	/* Actual scripth virt. address	*/
	u_long		p_script;	/* Actual script and scripth	*/
	u_long		p_scripth;	/*  bus addresses.		*/

	/*----------------------------------------------------------------
	**	General controller parameters and configuration.
	**----------------------------------------------------------------
	*/
	struct device	*dev;
	u_char		revision_id;	/* PCI device revision id	*/
	u32		irq;		/* IRQ level			*/
	u32		features;	/* Chip features map		*/
	u_char		myaddr;		/* SCSI id of the adapter	*/
	u_char		maxburst;	/* log base 2 of dwords burst	*/
	u_char		maxwide;	/* Maximum transfer width	*/
	u_char		minsync;	/* Minimum sync period factor	*/
	u_char		maxsync;	/* Maximum sync period factor	*/
	u_char		maxoffs;	/* Max scsi offset		*/
	u_char		multiplier;	/* Clock multiplier (1,2,4)	*/
	u_char		clock_divn;	/* Number of clock divisors	*/
	u_long		clock_khz;	/* SCSI clock frequency in KHz	*/

	/*----------------------------------------------------------------
	**	Start queue management.
	**	It is filled up by the host processor and accessed by the 
	**	SCRIPTS processor in order to start SCSI commands.
	**----------------------------------------------------------------
	*/
	u16		squeueput;	/* Next free slot of the queue	*/
	u16		actccbs;	/* Number of allocated CCBs	*/
	u16		queuedccbs;	/* Number of CCBs in start queue*/
	u16		queuedepth;	/* Start queue depth		*/

	/*----------------------------------------------------------------
	**	Timeout handler.
	**----------------------------------------------------------------
	*/
	struct timer_list timer;	/* Timer handler link header	*/
	u_long		lasttime;
	u_long		settle_time;	/* Resetting the SCSI BUS	*/

	/*----------------------------------------------------------------
	**	Debugging and profiling.
	**----------------------------------------------------------------
	*/
	struct ncr_reg	regdump;	/* Register dump		*/
	u_long		regtime;	/* Time it has been done	*/

	/*----------------------------------------------------------------
	**	Miscellaneous buffers accessed by the scripts-processor.
	**	They shall be DWORD aligned, because they may be read or 
	**	written with a SCR_COPY script command.
	**----------------------------------------------------------------
	*/
	u_char		msgout[8];	/* Buffer for MESSAGE OUT 	*/
	u_char		msgin [8];	/* Buffer for MESSAGE IN	*/
	u32		lastmsg;	/* Last SCSI message sent	*/
	u_char		scratch;	/* Scratch for SCSI receive	*/

	/*----------------------------------------------------------------
	**	Miscellaneous configuration and status parameters.
	**----------------------------------------------------------------
	*/
	u_char		disc;		/* Diconnection allowed		*/
	u_char		scsi_mode;	/* Current SCSI BUS mode	*/
	u_char		order;		/* Tag order to use		*/
	u_char		verbose;	/* Verbosity for this controller*/
	int		ncr_cache;	/* Used for cache test at init.	*/
	u_long		p_ncb;		/* BUS address of this NCB	*/

	/*----------------------------------------------------------------
	**	Command completion handling.
	**----------------------------------------------------------------
	*/
#ifdef SCSI_NCR_CCB_DONE_SUPPORT
	struct ccb	*(ccb_done[MAX_DONE]);
	int		ccb_done_ic;
#endif
	/*----------------------------------------------------------------
	**	Fields that should be removed or changed.
	**----------------------------------------------------------------
	*/
	struct ccb	*ccb;		/* Global CCB			*/
	struct usrcmd	user;		/* Command from user		*/
	volatile u_char	release_stage;	/* Synchronisation stage on release  */
};

#define NCB_SCRIPT_PHYS(np,lbl)	 (np->p_script  + offsetof (struct script, lbl))
#define NCB_SCRIPTH_PHYS(np,lbl) (np->p_scripth + offsetof (struct scripth,lbl))

/*==========================================================
**
**
**      Script for NCR-Processor.
**
**	Use ncr_script_fill() to create the variable parts.
**	Use ncr_script_copy_and_bind() to make a copy and
**	bind to physical addresses.
**
**
**==========================================================
**
**	We have to know the offsets of all labels before
**	we reach them (for forward jumps).
**	Therefore we declare a struct here.
**	If you make changes inside the script,
**	DONT FORGET TO CHANGE THE LENGTHS HERE!
**
**----------------------------------------------------------
*/

/*
**	For HP Zalon/53c720 systems, the Zalon interface
**	between CPU and 53c720 does prefetches, which causes
**	problems with self modifying scripts.  The problem
**	is overcome by calling a dummy subroutine after each
**	modification, to force a refetch of the script on
**	return from the subroutine.
*/

#ifdef CONFIG_NCR53C8XX_PREFETCH
#define PREFETCH_FLUSH_CNT	2
#define PREFETCH_FLUSH		SCR_CALL, PADDRH (wait_dma),
#else
#define PREFETCH_FLUSH_CNT	0
#define PREFETCH_FLUSH
#endif

/*
**	Script fragments which are loaded into the on-chip RAM 
**	of 825A, 875 and 895 chips.
*/
struct script {
	ncrcmd	start		[  5];
	ncrcmd  startpos	[  1];
	ncrcmd	select		[  6];
	ncrcmd	select2		[  9 + PREFETCH_FLUSH_CNT];
	ncrcmd	loadpos		[  4];
	ncrcmd	send_ident	[  9];
	ncrcmd	prepare		[  6];
	ncrcmd	prepare2	[  7];
	ncrcmd  command		[  6];
	ncrcmd  dispatch	[ 32];
	ncrcmd  clrack		[  4];
	ncrcmd	no_data		[ 17];
	ncrcmd  status		[  8];
	ncrcmd  msg_in		[  2];
	ncrcmd  msg_in2		[ 16];
	ncrcmd  msg_bad		[  4];
	ncrcmd	setmsg		[  7];
	ncrcmd	cleanup		[  6];
	ncrcmd  complete	[  9];
	ncrcmd	cleanup_ok	[  8 + PREFETCH_FLUSH_CNT];
	ncrcmd	cleanup0	[  1];
#ifndef SCSI_NCR_CCB_DONE_SUPPORT
	ncrcmd	signal		[ 12];
#else
	ncrcmd	signal		[  9];
	ncrcmd	done_pos	[  1];
	ncrcmd	done_plug	[  2];
	ncrcmd	done_end	[  7];
#endif
	ncrcmd  save_dp		[  7];
	ncrcmd  restore_dp	[  5];
	ncrcmd  disconnect	[ 10];
	ncrcmd	msg_out		[  9];
	ncrcmd	msg_out_done	[  7];
	ncrcmd  idle		[  2];
	ncrcmd	reselect	[  8];
	ncrcmd	reselected	[  8];
	ncrcmd	resel_dsa	[  6 + PREFETCH_FLUSH_CNT];
	ncrcmd	loadpos1	[  4];
	ncrcmd  resel_lun	[  6];
	ncrcmd	resel_tag	[  6];
	ncrcmd	jump_to_nexus	[  4 + PREFETCH_FLUSH_CNT];
	ncrcmd	nexus_indirect	[  4];
	ncrcmd	resel_notag	[  4];
	ncrcmd  data_in		[MAX_SCATTERL * 4];
	ncrcmd  data_in2	[  4];
	ncrcmd  data_out	[MAX_SCATTERL * 4];
	ncrcmd  data_out2	[  4];
};

/*
**	Script fragments which stay in main memory for all chips.
*/
struct scripth {
	ncrcmd  tryloop		[MAX_START*2];
	ncrcmd  tryloop2	[  2];
#ifdef SCSI_NCR_CCB_DONE_SUPPORT
	ncrcmd  done_queue	[MAX_DONE*5];
	ncrcmd  done_queue2	[  2];
#endif
	ncrcmd	select_no_atn	[  8];
	ncrcmd	cancel		[  4];
	ncrcmd	skip		[  9 + PREFETCH_FLUSH_CNT];
	ncrcmd	skip2		[ 19];
	ncrcmd	par_err_data_in	[  6];
	ncrcmd	par_err_other	[  4];
	ncrcmd	msg_reject	[  8];
	ncrcmd	msg_ign_residue	[ 24];
	ncrcmd  msg_extended	[ 10];
	ncrcmd  msg_ext_2	[ 10];
	ncrcmd	msg_wdtr	[ 14];
	ncrcmd	send_wdtr	[  7];
	ncrcmd  msg_ext_3	[ 10];
	ncrcmd	msg_sdtr	[ 14];
	ncrcmd	send_sdtr	[  7];
	ncrcmd	nego_bad_phase	[  4];
	ncrcmd	msg_out_abort	[ 10];
	ncrcmd  hdata_in	[MAX_SCATTERH * 4];
	ncrcmd  hdata_in2	[  2];
	ncrcmd  hdata_out	[MAX_SCATTERH * 4];
	ncrcmd  hdata_out2	[  2];
	ncrcmd	reset		[  4];
	ncrcmd	aborttag	[  4];
	ncrcmd	abort		[  2];
	ncrcmd	abort_resel	[ 20];
	ncrcmd	resend_ident	[  4];
	ncrcmd	clratn_go_on	[  3];
	ncrcmd	nxtdsp_go_on	[  1];
	ncrcmd	sdata_in	[  8];
	ncrcmd  data_io		[ 18];
	ncrcmd	bad_identify	[ 12];
	ncrcmd	bad_i_t_l	[  4];
	ncrcmd	bad_i_t_l_q	[  4];
	ncrcmd	bad_target	[  8];
	ncrcmd	bad_status	[  8];
	ncrcmd	start_ram	[  4 + PREFETCH_FLUSH_CNT];
	ncrcmd	start_ram0	[  4];
	ncrcmd	sto_restart	[  5];
	ncrcmd	wait_dma	[  2];
	ncrcmd	snooptest	[  9];
	ncrcmd	snoopend	[  2];
};

/*==========================================================
**
**
**      Function headers.
**
**
**==========================================================
*/

static	void	ncr_alloc_ccb	(struct ncb *np, u_char tn, u_char ln);
static	void	ncr_complete	(struct ncb *np, struct ccb *cp);
static	void	ncr_exception	(struct ncb *np);
static	void	ncr_free_ccb	(struct ncb *np, struct ccb *cp);
static	void	ncr_init_ccb	(struct ncb *np, struct ccb *cp);
static	void	ncr_init_tcb	(struct ncb *np, u_char tn);
static	struct lcb *	ncr_alloc_lcb	(struct ncb *np, u_char tn, u_char ln);
static	struct lcb *	ncr_setup_lcb	(struct ncb *np, struct scsi_device *sdev);
static	void	ncr_getclock	(struct ncb *np, int mult);
static	void	ncr_selectclock	(struct ncb *np, u_char scntl3);
static	struct ccb *ncr_get_ccb	(struct ncb *np, struct scsi_cmnd *cmd);
static	void	ncr_chip_reset	(struct ncb *np, int delay);
static	void	ncr_init	(struct ncb *np, int reset, char * msg, u_long code);
static	int	ncr_int_sbmc	(struct ncb *np);
static	int	ncr_int_par	(struct ncb *np);
static	void	ncr_int_ma	(struct ncb *np);
static	void	ncr_int_sir	(struct ncb *np);
static  void    ncr_int_sto     (struct ncb *np);
static	void	ncr_negotiate	(struct ncb* np, struct tcb* tp);
static	int	ncr_prepare_nego(struct ncb *np, struct ccb *cp, u_char *msgptr);

static	void	ncr_script_copy_and_bind
				(struct ncb *np, ncrcmd *src, ncrcmd *dst, int len);
static  void    ncr_script_fill (struct script * scr, struct scripth * scripth);
static	int	ncr_scatter	(struct ncb *np, struct ccb *cp, struct scsi_cmnd *cmd);
static	void	ncr_getsync	(struct ncb *np, u_char sfac, u_char *fakp, u_char *scntl3p);
static	void	ncr_setsync	(struct ncb *np, struct ccb *cp, u_char scntl3, u_char sxfer);
static	void	ncr_setup_tags	(struct ncb *np, struct scsi_device *sdev);
static	void	ncr_setwide	(struct ncb *np, struct ccb *cp, u_char wide, u_char ack);
static	int	ncr_snooptest	(struct ncb *np);
static	void	ncr_timeout	(struct ncb *np);
static  void    ncr_wakeup      (struct ncb *np, u_long code);
static  void    ncr_wakeup_done (struct ncb *np);
static	void	ncr_start_next_ccb (struct ncb *np, struct lcb * lp, int maxn);
static	void	ncr_put_start_queue(struct ncb *np, struct ccb *cp);

static void insert_into_waiting_list(struct ncb *np, struct scsi_cmnd *cmd);
static struct scsi_cmnd *retrieve_from_waiting_list(int to_remove, struct ncb *np, struct scsi_cmnd *cmd);
static void process_waiting_list(struct ncb *np, int sts);

#define remove_from_waiting_list(np, cmd) \
		retrieve_from_waiting_list(1, (np), (cmd))
#define requeue_waiting_list(np) process_waiting_list((np), DID_OK)
#define reset_waiting_list(np) process_waiting_list((np), DID_RESET)

static inline char *ncr_name (struct ncb *np)
{
	return np->inst_name;
}


/*==========================================================
**
**
**      Scripts for NCR-Processor.
**
**      Use ncr_script_bind for binding to physical addresses.
**
**
**==========================================================
**
**	NADDR generates a reference to a field of the controller data.
**	PADDR generates a reference to another part of the script.
**	RADDR generates a reference to a script processor register.
**	FADDR generates a reference to a script processor register
**		with offset.
**
**----------------------------------------------------------
*/

#define	RELOC_SOFTC	0x40000000
#define	RELOC_LABEL	0x50000000
#define	RELOC_REGISTER	0x60000000
#if 0
#define	RELOC_KVAR	0x70000000
#endif
#define	RELOC_LABELH	0x80000000
#define	RELOC_MASK	0xf0000000

#define	NADDR(label)	(RELOC_SOFTC | offsetof(struct ncb, label))
#define PADDR(label)    (RELOC_LABEL | offsetof(struct script, label))
#define PADDRH(label)   (RELOC_LABELH | offsetof(struct scripth, label))
#define	RADDR(label)	(RELOC_REGISTER | REG(label))
#define	FADDR(label,ofs)(RELOC_REGISTER | ((REG(label))+(ofs)))
#if 0
#define	KVAR(which)	(RELOC_KVAR | (which))
#endif

#if 0
#define	SCRIPT_KVAR_JIFFIES	(0)
#define	SCRIPT_KVAR_FIRST		SCRIPT_KVAR_JIFFIES
#define	SCRIPT_KVAR_LAST		SCRIPT_KVAR_JIFFIES
/*
 * Kernel variables referenced in the scripts.
 * THESE MUST ALL BE ALIGNED TO A 4-BYTE BOUNDARY.
 */
static void *script_kvars[] __initdata =
	{ (void *)&jiffies };
#endif

static	struct script script0 __initdata = {
/*--------------------------< START >-----------------------*/ {
	/*
	**	This NOP will be patched with LED ON
	**	SCR_REG_REG (gpreg, SCR_AND, 0xfe)
	*/
	SCR_NO_OP,
		0,
	/*
	**      Clear SIGP.
	*/
	SCR_FROM_REG (ctest2),
		0,
	/*
	**	Then jump to a certain point in tryloop.
	**	Due to the lack of indirect addressing the code
	**	is self modifying here.
	*/
	SCR_JUMP,
}/*-------------------------< STARTPOS >--------------------*/,{
		PADDRH(tryloop),

}/*-------------------------< SELECT >----------------------*/,{
	/*
	**	DSA	contains the address of a scheduled
	**		data structure.
	**
	**	SCRATCHA contains the address of the script,
	**		which starts the next entry.
	**
	**	Set Initiator mode.
	**
	**	(Target mode is left as an exercise for the reader)
	*/

	SCR_CLR (SCR_TRG),
		0,
	SCR_LOAD_REG (HS_REG, HS_SELECTING),
		0,

	/*
	**      And try to select this target.
	*/
	SCR_SEL_TBL_ATN ^ offsetof (struct dsb, select),
		PADDR (reselect),

}/*-------------------------< SELECT2 >----------------------*/,{
	/*
	**	Now there are 4 possibilities:
	**
	**	(1) The ncr loses arbitration.
	**	This is ok, because it will try again,
	**	when the bus becomes idle.
	**	(But beware of the timeout function!)
	**
	**	(2) The ncr is reselected.
	**	Then the script processor takes the jump
	**	to the RESELECT label.
	**
	**	(3) The ncr wins arbitration.
	**	Then it will execute SCRIPTS instruction until 
	**	the next instruction that checks SCSI phase.
	**	Then will stop and wait for selection to be 
	**	complete or selection time-out to occur.
	**	As a result the SCRIPTS instructions until 
	**	LOADPOS + 2 should be executed in parallel with 
	**	the SCSI core performing selection.
	*/

	/*
	**	The MESSAGE_REJECT problem seems to be due to a selection 
	**	timing problem.
	**	Wait immediately for the selection to complete. 
	**	(2.5x behaves so)
	*/
	SCR_JUMPR ^ IFFALSE (WHEN (SCR_MSG_OUT)),
		0,

	/*
	**	Next time use the next slot.
	*/
	SCR_COPY (4),
		RADDR (temp),
		PADDR (startpos),
	/*
	**      The ncr doesn't have an indirect load
	**	or store command. So we have to
	**	copy part of the control block to a
	**	fixed place, where we can access it.
	**
	**	We patch the address part of a
	**	COPY command with the DSA-register.
	*/
	SCR_COPY_F (4),
		RADDR (dsa),
		PADDR (loadpos),
	/*
	**	Flush script prefetch if required
	*/
	PREFETCH_FLUSH
	/*
	**	then we do the actual copy.
	*/
	SCR_COPY (sizeof (struct head)),
	/*
	**	continued after the next label ...
	*/
}/*-------------------------< LOADPOS >---------------------*/,{
		0,
		NADDR (header),
	/*
	**	Wait for the next phase or the selection
	**	to complete or time-out.
	*/
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_OUT)),
		PADDR (prepare),

}/*-------------------------< SEND_IDENT >----------------------*/,{
	/*
	**	Selection complete.
	**	Send the IDENTIFY and SIMPLE_TAG messages
	**	(and the EXTENDED_SDTR message)
	*/
	SCR_MOVE_TBL ^ SCR_MSG_OUT,
		offsetof (struct dsb, smsg),
	SCR_JUMP ^ IFTRUE (WHEN (SCR_MSG_OUT)),
		PADDRH (resend_ident),
	SCR_LOAD_REG (scratcha, 0x80),
		0,
	SCR_COPY (1),
		RADDR (scratcha),
		NADDR (lastmsg),
}/*-------------------------< PREPARE >----------------------*/,{
	/*
	**      load the savep (saved pointer) into
	**      the TEMP register (actual pointer)
	*/
	SCR_COPY (4),
		NADDR (header.savep),
		RADDR (temp),
	/*
	**      Initialize the status registers
	*/
	SCR_COPY (4),
		NADDR (header.status),
		RADDR (scr0),
}/*-------------------------< PREPARE2 >---------------------*/,{
	/*
	**	Initialize the msgout buffer with a NOOP message.
	*/
	SCR_LOAD_REG (scratcha, NOP),
		0,
	SCR_COPY (1),
		RADDR (scratcha),
		NADDR (msgout),
#if 0
	SCR_COPY (1),
		RADDR (scratcha),
		NADDR (msgin),
#endif
	/*
	**	Anticipate the COMMAND phase.
	**	This is the normal case for initial selection.
	*/
	SCR_JUMP ^ IFFALSE (WHEN (SCR_COMMAND)),
		PADDR (dispatch),

}/*-------------------------< COMMAND >--------------------*/,{
	/*
	**	... and send the command
	*/
	SCR_MOVE_TBL ^ SCR_COMMAND,
		offsetof (struct dsb, cmd),
	/*
	**	If status is still HS_NEGOTIATE, negotiation failed.
	**	We check this here, since we want to do that 
	**	only once.
	*/
	SCR_FROM_REG (HS_REG),
		0,
	SCR_INT ^ IFTRUE (DATA (HS_NEGOTIATE)),
		SIR_NEGO_FAILED,

}/*-----------------------< DISPATCH >----------------------*/,{
	/*
	**	MSG_IN is the only phase that shall be 
	**	entered at least once for each (re)selection.
	**	So we test it first.
	*/
	SCR_JUMP ^ IFTRUE (WHEN (SCR_MSG_IN)),
		PADDR (msg_in),

	SCR_RETURN ^ IFTRUE (IF (SCR_DATA_OUT)),
		0,
	/*
	**	DEL 397 - 53C875 Rev 3 - Part Number 609-0392410 - ITEM 4.
	**	Possible data corruption during Memory Write and Invalidate.
	**	This work-around resets the addressing logic prior to the 
	**	start of the first MOVE of a DATA IN phase.
	**	(See Documentation/scsi/ncr53c8xx.txt for more information)
	*/
	SCR_JUMPR ^ IFFALSE (IF (SCR_DATA_IN)),
		20,
	SCR_COPY (4),
		RADDR (scratcha),
		RADDR (scratcha),
	SCR_RETURN,
 		0,
	SCR_JUMP ^ IFTRUE (IF (SCR_STATUS)),
		PADDR (status),
	SCR_JUMP ^ IFTRUE (IF (SCR_COMMAND)),
		PADDR (command),
	SCR_JUMP ^ IFTRUE (IF (SCR_MSG_OUT)),
		PADDR (msg_out),
	/*
	**      Discard one illegal phase byte, if required.
	*/
	SCR_LOAD_REG (scratcha, XE_BAD_PHASE),
		0,
	SCR_COPY (1),
		RADDR (scratcha),
		NADDR (xerr_st),
	SCR_JUMPR ^ IFFALSE (IF (SCR_ILG_OUT)),
		8,
	SCR_MOVE_ABS (1) ^ SCR_ILG_OUT,
		NADDR (scratch),
	SCR_JUMPR ^ IFFALSE (IF (SCR_ILG_IN)),
		8,
	SCR_MOVE_ABS (1) ^ SCR_ILG_IN,
		NADDR (scratch),
	SCR_JUMP,
		PADDR (dispatch),

}/*-------------------------< CLRACK >----------------------*/,{
	/*
	**	Terminate possible pending message phase.
	*/
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP,
		PADDR (dispatch),

}/*-------------------------< NO_DATA >--------------------*/,{
	/*
	**	The target wants to tranfer too much data
	**	or in the wrong direction.
	**      Remember that in extended error.
	*/
	SCR_LOAD_REG (scratcha, XE_EXTRA_DATA),
		0,
	SCR_COPY (1),
		RADDR (scratcha),
		NADDR (xerr_st),
	/*
	**      Discard one data byte, if required.
	*/
	SCR_JUMPR ^ IFFALSE (WHEN (SCR_DATA_OUT)),
		8,
	SCR_MOVE_ABS (1) ^ SCR_DATA_OUT,
		NADDR (scratch),
	SCR_JUMPR ^ IFFALSE (IF (SCR_DATA_IN)),
		8,
	SCR_MOVE_ABS (1) ^ SCR_DATA_IN,
		NADDR (scratch),
	/*
	**      .. and repeat as required.
	*/
	SCR_CALL,
		PADDR (dispatch),
	SCR_JUMP,
		PADDR (no_data),

}/*-------------------------< STATUS >--------------------*/,{
	/*
	**	get the status
	*/
	SCR_MOVE_ABS (1) ^ SCR_STATUS,
		NADDR (scratch),
	/*
	**	save status to scsi_status.
	**	mark as complete.
	*/
	SCR_TO_REG (SS_REG),
		0,
	SCR_LOAD_REG (HS_REG, HS_COMPLETE),
		0,
	SCR_JUMP,
		PADDR (dispatch),
}/*-------------------------< MSG_IN >--------------------*/,{
	/*
	**	Get the first byte of the message
	**	and save it to SCRATCHA.
	**
	**	The script processor doesn't negate the
	**	ACK signal after this transfer.
	*/
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin[0]),
}/*-------------------------< MSG_IN2 >--------------------*/,{
	/*
	**	Handle this message.
	*/
	SCR_JUMP ^ IFTRUE (DATA (COMMAND_COMPLETE)),
		PADDR (complete),
	SCR_JUMP ^ IFTRUE (DATA (DISCONNECT)),
		PADDR (disconnect),
	SCR_JUMP ^ IFTRUE (DATA (SAVE_POINTERS)),
		PADDR (save_dp),
	SCR_JUMP ^ IFTRUE (DATA (RESTORE_POINTERS)),
		PADDR (restore_dp),
	SCR_JUMP ^ IFTRUE (DATA (EXTENDED_MESSAGE)),
		PADDRH (msg_extended),
	SCR_JUMP ^ IFTRUE (DATA (NOP)),
		PADDR (clrack),
	SCR_JUMP ^ IFTRUE (DATA (MESSAGE_REJECT)),
		PADDRH (msg_reject),
	SCR_JUMP ^ IFTRUE (DATA (IGNORE_WIDE_RESIDUE)),
		PADDRH (msg_ign_residue),
	/*
	**	Rest of the messages left as
	**	an exercise ...
	**
	**	Unimplemented messages:
	**	fall through to MSG_BAD.
	*/
}/*-------------------------< MSG_BAD >------------------*/,{
	/*
	**	unimplemented message - reject it.
	*/
	SCR_INT,
		SIR_REJECT_SENT,
	SCR_LOAD_REG (scratcha, MESSAGE_REJECT),
		0,
}/*-------------------------< SETMSG >----------------------*/,{
	SCR_COPY (1),
		RADDR (scratcha),
		NADDR (msgout),
	SCR_SET (SCR_ATN),
		0,
	SCR_JUMP,
		PADDR (clrack),
}/*-------------------------< CLEANUP >-------------------*/,{
	/*
	**      dsa:    Pointer to ccb
	**	      or xxxxxxFF (no ccb)
	**
	**      HS_REG:   Host-Status (<>0!)
	*/
	SCR_FROM_REG (dsa),
		0,
	SCR_JUMP ^ IFTRUE (DATA (0xff)),
		PADDR (start),
	/*
	**      dsa is valid.
	**	complete the cleanup.
	*/
	SCR_JUMP,
		PADDR (cleanup_ok),

}/*-------------------------< COMPLETE >-----------------*/,{
	/*
	**	Complete message.
	**
	**	Copy TEMP register to LASTP in header.
	*/
	SCR_COPY (4),
		RADDR (temp),
		NADDR (header.lastp),
	/*
	**	When we terminate the cycle by clearing ACK,
	**	the target may disconnect immediately.
	**
	**	We don't want to be told of an
	**	"unexpected disconnect",
	**	so we disable this feature.
	*/
	SCR_REG_REG (scntl2, SCR_AND, 0x7f),
		0,
	/*
	**	Terminate cycle ...
	*/
	SCR_CLR (SCR_ACK|SCR_ATN),
		0,
	/*
	**	... and wait for the disconnect.
	*/
	SCR_WAIT_DISC,
		0,
}/*-------------------------< CLEANUP_OK >----------------*/,{
	/*
	**	Save host status to header.
	*/
	SCR_COPY (4),
		RADDR (scr0),
		NADDR (header.status),
	/*
	**	and copy back the header to the ccb.
	*/
	SCR_COPY_F (4),
		RADDR (dsa),
		PADDR (cleanup0),
	/*
	**	Flush script prefetch if required
	*/
	PREFETCH_FLUSH
	SCR_COPY (sizeof (struct head)),
		NADDR (header),
}/*-------------------------< CLEANUP0 >--------------------*/,{
		0,
}/*-------------------------< SIGNAL >----------------------*/,{
	/*
	**	if job not completed ...
	*/
	SCR_FROM_REG (HS_REG),
		0,
	/*
	**	... start the next command.
	*/
	SCR_JUMP ^ IFTRUE (MASK (0, (HS_DONEMASK|HS_SKIPMASK))),
		PADDR(start),
	/*
	**	If command resulted in not GOOD status,
	**	call the C code if needed.
	*/
	SCR_FROM_REG (SS_REG),
		0,
	SCR_CALL ^ IFFALSE (DATA (S_GOOD)),
		PADDRH (bad_status),

#ifndef	SCSI_NCR_CCB_DONE_SUPPORT

	/*
	**	... signal completion to the host
	*/
	SCR_INT,
		SIR_INTFLY,
	/*
	**	Auf zu neuen Schandtaten!
	*/
	SCR_JUMP,
		PADDR(start),

#else	/* defined SCSI_NCR_CCB_DONE_SUPPORT */

	/*
	**	... signal completion to the host
	*/
	SCR_JUMP,
}/*------------------------< DONE_POS >---------------------*/,{
		PADDRH (done_queue),
}/*------------------------< DONE_PLUG >--------------------*/,{
	SCR_INT,
		SIR_DONE_OVERFLOW,
}/*------------------------< DONE_END >---------------------*/,{
	SCR_INT,
		SIR_INTFLY,
	SCR_COPY (4),
		RADDR (temp),
		PADDR (done_pos),
	SCR_JUMP,
		PADDR (start),

#endif	/* SCSI_NCR_CCB_DONE_SUPPORT */

}/*-------------------------< SAVE_DP >------------------*/,{
	/*
	**	SAVE_DP message:
	**	Copy TEMP register to SAVEP in header.
	*/
	SCR_COPY (4),
		RADDR (temp),
		NADDR (header.savep),
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP,
		PADDR (dispatch),
}/*-------------------------< RESTORE_DP >---------------*/,{
	/*
	**	RESTORE_DP message:
	**	Copy SAVEP in header to TEMP register.
	*/
	SCR_COPY (4),
		NADDR (header.savep),
		RADDR (temp),
	SCR_JUMP,
		PADDR (clrack),

}/*-------------------------< DISCONNECT >---------------*/,{
	/*
	**	DISCONNECTing  ...
	**
	**	disable the "unexpected disconnect" feature,
	**	and remove the ACK signal.
	*/
	SCR_REG_REG (scntl2, SCR_AND, 0x7f),
		0,
	SCR_CLR (SCR_ACK|SCR_ATN),
		0,
	/*
	**	Wait for the disconnect.
	*/
	SCR_WAIT_DISC,
		0,
	/*
	**	Status is: DISCONNECTED.
	*/
	SCR_LOAD_REG (HS_REG, HS_DISCONNECT),
		0,
	SCR_JUMP,
		PADDR (cleanup_ok),

}/*-------------------------< MSG_OUT >-------------------*/,{
	/*
	**	The target requests a message.
	*/
	SCR_MOVE_ABS (1) ^ SCR_MSG_OUT,
		NADDR (msgout),
	SCR_COPY (1),
		NADDR (msgout),
		NADDR (lastmsg),
	/*
	**	If it was no ABORT message ...
	*/
	SCR_JUMP ^ IFTRUE (DATA (ABORT_TASK_SET)),
		PADDRH (msg_out_abort),
	/*
	**	... wait for the next phase
	**	if it's a message out, send it again, ...
	*/
	SCR_JUMP ^ IFTRUE (WHEN (SCR_MSG_OUT)),
		PADDR (msg_out),
}/*-------------------------< MSG_OUT_DONE >--------------*/,{
	/*
	**	... else clear the message ...
	*/
	SCR_LOAD_REG (scratcha, NOP),
		0,
	SCR_COPY (4),
		RADDR (scratcha),
		NADDR (msgout),
	/*
	**	... and process the next phase
	*/
	SCR_JUMP,
		PADDR (dispatch),
}/*-------------------------< IDLE >------------------------*/,{
	/*
	**	Nothing to do?
	**	Wait for reselect.
	**	This NOP will be patched with LED OFF
	**	SCR_REG_REG (gpreg, SCR_OR, 0x01)
	*/
	SCR_NO_OP,
		0,
}/*-------------------------< RESELECT >--------------------*/,{
	/*
	**	make the DSA invalid.
	*/
	SCR_LOAD_REG (dsa, 0xff),
		0,
	SCR_CLR (SCR_TRG),
		0,
	SCR_LOAD_REG (HS_REG, HS_IN_RESELECT),
		0,
	/*
	**	Sleep waiting for a reselection.
	**	If SIGP is set, special treatment.
	**
	**	Zu allem bereit ..
	*/
	SCR_WAIT_RESEL,
		PADDR(start),
}/*-------------------------< RESELECTED >------------------*/,{
	/*
	**	This NOP will be patched with LED ON
	**	SCR_REG_REG (gpreg, SCR_AND, 0xfe)
	*/
	SCR_NO_OP,
		0,
	/*
	**	... zu nichts zu gebrauchen ?
	**
	**      load the target id into the SFBR
	**	and jump to the control block.
	**
	**	Look at the declarations of
	**	- struct ncb
	**	- struct tcb
	**	- struct lcb
	**	- struct ccb
	**	to understand what's going on.
	*/
	SCR_REG_SFBR (ssid, SCR_AND, 0x8F),
		0,
	SCR_TO_REG (sdid),
		0,
	SCR_JUMP,
		NADDR (jump_tcb),

}/*-------------------------< RESEL_DSA >-------------------*/,{
	/*
	**	Ack the IDENTIFY or TAG previously received.
	*/
	SCR_CLR (SCR_ACK),
		0,
	/*
	**      The ncr doesn't have an indirect load
	**	or store command. So we have to
	**	copy part of the control block to a
	**	fixed place, where we can access it.
	**
	**	We patch the address part of a
	**	COPY command with the DSA-register.
	*/
	SCR_COPY_F (4),
		RADDR (dsa),
		PADDR (loadpos1),
	/*
	**	Flush script prefetch if required
	*/
	PREFETCH_FLUSH
	/*
	**	then we do the actual copy.
	*/
	SCR_COPY (sizeof (struct head)),
	/*
	**	continued after the next label ...
	*/

}/*-------------------------< LOADPOS1 >-------------------*/,{
		0,
		NADDR (header),
	/*
	**	The DSA contains the data structure address.
	*/
	SCR_JUMP,
		PADDR (prepare),

}/*-------------------------< RESEL_LUN >-------------------*/,{
	/*
	**	come back to this point
	**	to get an IDENTIFY message
	**	Wait for a msg_in phase.
	*/
	SCR_INT ^ IFFALSE (WHEN (SCR_MSG_IN)),
		SIR_RESEL_NO_MSG_IN,
	/*
	**	message phase.
	**	Read the data directly from the BUS DATA lines.
	**	This helps to support very old SCSI devices that 
	**	may reselect without sending an IDENTIFY.
	*/
	SCR_FROM_REG (sbdl),
		0,
	/*
	**	It should be an Identify message.
	*/
	SCR_RETURN,
		0,
}/*-------------------------< RESEL_TAG >-------------------*/,{
	/*
	**	Read IDENTIFY + SIMPLE + TAG using a single MOVE.
	**	Agressive optimization, is'nt it?
	**	No need to test the SIMPLE TAG message, since the 
	**	driver only supports conformant devices for tags. ;-)
	*/
	SCR_MOVE_ABS (3) ^ SCR_MSG_IN,
		NADDR (msgin),
	/*
	**	Read the TAG from the SIDL.
	**	Still an aggressive optimization. ;-)
	**	Compute the CCB indirect jump address which 
	**	is (#TAG*2 & 0xfc) due to tag numbering using 
	**	1,3,5..MAXTAGS*2+1 actual values.
	*/
	SCR_REG_SFBR (sidl, SCR_SHL, 0),
		0,
	SCR_SFBR_REG (temp, SCR_AND, 0xfc),
		0,
}/*-------------------------< JUMP_TO_NEXUS >-------------------*/,{
	SCR_COPY_F (4),
		RADDR (temp),
		PADDR (nexus_indirect),
	/*
	**	Flush script prefetch if required
	*/
	PREFETCH_FLUSH
	SCR_COPY (4),
}/*-------------------------< NEXUS_INDIRECT >-------------------*/,{
		0,
		RADDR (temp),
	SCR_RETURN,
		0,
}/*-------------------------< RESEL_NOTAG >-------------------*/,{
	/*
	**	No tag expected.
	**	Read an throw away the IDENTIFY.
	*/
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin),
	SCR_JUMP,
		PADDR (jump_to_nexus),
}/*-------------------------< DATA_IN >--------------------*/,{
/*
**	Because the size depends on the
**	#define MAX_SCATTERL parameter,
**	it is filled in at runtime.
**
**  ##===========< i=0; i<MAX_SCATTERL >=========
**  ||	SCR_CALL ^ IFFALSE (WHEN (SCR_DATA_IN)),
**  ||		PADDR (dispatch),
**  ||	SCR_MOVE_TBL ^ SCR_DATA_IN,
**  ||		offsetof (struct dsb, data[ i]),
**  ##==========================================
**
**---------------------------------------------------------
*/
0
}/*-------------------------< DATA_IN2 >-------------------*/,{
	SCR_CALL,
		PADDR (dispatch),
	SCR_JUMP,
		PADDR (no_data),
}/*-------------------------< DATA_OUT >--------------------*/,{
/*
**	Because the size depends on the
**	#define MAX_SCATTERL parameter,
**	it is filled in at runtime.
**
**  ##===========< i=0; i<MAX_SCATTERL >=========
**  ||	SCR_CALL ^ IFFALSE (WHEN (SCR_DATA_OUT)),
**  ||		PADDR (dispatch),
**  ||	SCR_MOVE_TBL ^ SCR_DATA_OUT,
**  ||		offsetof (struct dsb, data[ i]),
**  ##==========================================
**
**---------------------------------------------------------
*/
0
}/*-------------------------< DATA_OUT2 >-------------------*/,{
	SCR_CALL,
		PADDR (dispatch),
	SCR_JUMP,
		PADDR (no_data),
}/*--------------------------------------------------------*/
};

static	struct scripth scripth0 __initdata = {
/*-------------------------< TRYLOOP >---------------------*/{
/*
**	Start the next entry.
**	Called addresses point to the launch script in the CCB.
**	They are patched by the main processor.
**
**	Because the size depends on the
**	#define MAX_START parameter, it is filled
**	in at runtime.
**
**-----------------------------------------------------------
**
**  ##===========< I=0; i<MAX_START >===========
**  ||	SCR_CALL,
**  ||		PADDR (idle),
**  ##==========================================
**
**-----------------------------------------------------------
*/
0
}/*------------------------< TRYLOOP2 >---------------------*/,{
	SCR_JUMP,
		PADDRH(tryloop),

#ifdef SCSI_NCR_CCB_DONE_SUPPORT

}/*------------------------< DONE_QUEUE >-------------------*/,{
/*
**	Copy the CCB address to the next done entry.
**	Because the size depends on the
**	#define MAX_DONE parameter, it is filled
**	in at runtime.
**
**-----------------------------------------------------------
**
**  ##===========< I=0; i<MAX_DONE >===========
**  ||	SCR_COPY (sizeof(struct ccb *),
**  ||		NADDR (header.cp),
**  ||		NADDR (ccb_done[i]),
**  ||	SCR_CALL,
**  ||		PADDR (done_end),
**  ##==========================================
**
**-----------------------------------------------------------
*/
0
}/*------------------------< DONE_QUEUE2 >------------------*/,{
	SCR_JUMP,
		PADDRH (done_queue),

#endif /* SCSI_NCR_CCB_DONE_SUPPORT */
}/*------------------------< SELECT_NO_ATN >-----------------*/,{
	/*
	**	Set Initiator mode.
	**      And try to select this target without ATN.
	*/

	SCR_CLR (SCR_TRG),
		0,
	SCR_LOAD_REG (HS_REG, HS_SELECTING),
		0,
	SCR_SEL_TBL ^ offsetof (struct dsb, select),
		PADDR (reselect),
	SCR_JUMP,
		PADDR (select2),

}/*-------------------------< CANCEL >------------------------*/,{

	SCR_LOAD_REG (scratcha, HS_ABORTED),
		0,
	SCR_JUMPR,
		8,
}/*-------------------------< SKIP >------------------------*/,{
	SCR_LOAD_REG (scratcha, 0),
		0,
	/*
	**	This entry has been canceled.
	**	Next time use the next slot.
	*/
	SCR_COPY (4),
		RADDR (temp),
		PADDR (startpos),
	/*
	**      The ncr doesn't have an indirect load
	**	or store command. So we have to
	**	copy part of the control block to a
	**	fixed place, where we can access it.
	**
	**	We patch the address part of a
	**	COPY command with the DSA-register.
	*/
	SCR_COPY_F (4),
		RADDR (dsa),
		PADDRH (skip2),
	/*
	**	Flush script prefetch if required
	*/
	PREFETCH_FLUSH
	/*
	**	then we do the actual copy.
	*/
	SCR_COPY (sizeof (struct head)),
	/*
	**	continued after the next label ...
	*/
}/*-------------------------< SKIP2 >---------------------*/,{
		0,
		NADDR (header),
	/*
	**      Initialize the status registers
	*/
	SCR_COPY (4),
		NADDR (header.status),
		RADDR (scr0),
	/*
	**	Force host status.
	*/
	SCR_FROM_REG (scratcha),
		0,
	SCR_JUMPR ^ IFFALSE (MASK (0, HS_DONEMASK)),
		16,
	SCR_REG_REG (HS_REG, SCR_OR, HS_SKIPMASK),
		0,
	SCR_JUMPR,
		8,
	SCR_TO_REG (HS_REG),
		0,
	SCR_LOAD_REG (SS_REG, S_GOOD),
		0,
	SCR_JUMP,
		PADDR (cleanup_ok),

},/*-------------------------< PAR_ERR_DATA_IN >---------------*/{
	/*
	**	Ignore all data in byte, until next phase
	*/
	SCR_JUMP ^ IFFALSE (WHEN (SCR_DATA_IN)),
		PADDRH (par_err_other),
	SCR_MOVE_ABS (1) ^ SCR_DATA_IN,
		NADDR (scratch),
	SCR_JUMPR,
		-24,
},/*-------------------------< PAR_ERR_OTHER >------------------*/{
	/*
	**	count it.
	*/
	SCR_REG_REG (PS_REG, SCR_ADD, 0x01),
		0,
	/*
	**	jump to dispatcher.
	*/
	SCR_JUMP,
		PADDR (dispatch),
}/*-------------------------< MSG_REJECT >---------------*/,{
	/*
	**	If a negotiation was in progress,
	**	negotiation failed.
	**	Otherwise, let the C code print 
	**	some message.
	*/
	SCR_FROM_REG (HS_REG),
		0,
	SCR_INT ^ IFFALSE (DATA (HS_NEGOTIATE)),
		SIR_REJECT_RECEIVED,
	SCR_INT ^ IFTRUE (DATA (HS_NEGOTIATE)),
		SIR_NEGO_FAILED,
	SCR_JUMP,
		PADDR (clrack),

}/*-------------------------< MSG_IGN_RESIDUE >----------*/,{
	/*
	**	Terminate cycle
	*/
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_IN)),
		PADDR (dispatch),
	/*
	**	get residue size.
	*/
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin[1]),
	/*
	**	Size is 0 .. ignore message.
	*/
	SCR_JUMP ^ IFTRUE (DATA (0)),
		PADDR (clrack),
	/*
	**	Size is not 1 .. have to interrupt.
	*/
	SCR_JUMPR ^ IFFALSE (DATA (1)),
		40,
	/*
	**	Check for residue byte in swide register
	*/
	SCR_FROM_REG (scntl2),
		0,
	SCR_JUMPR ^ IFFALSE (MASK (WSR, WSR)),
		16,
	/*
	**	There IS data in the swide register.
	**	Discard it.
	*/
	SCR_REG_REG (scntl2, SCR_OR, WSR),
		0,
	SCR_JUMP,
		PADDR (clrack),
	/*
	**	Load again the size to the sfbr register.
	*/
	SCR_FROM_REG (scratcha),
		0,
	SCR_INT,
		SIR_IGN_RESIDUE,
	SCR_JUMP,
		PADDR (clrack),

}/*-------------------------< MSG_EXTENDED >-------------*/,{
	/*
	**	Terminate cycle
	*/
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_IN)),
		PADDR (dispatch),
	/*
	**	get length.
	*/
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin[1]),
	/*
	*/
	SCR_JUMP ^ IFTRUE (DATA (3)),
		PADDRH (msg_ext_3),
	SCR_JUMP ^ IFFALSE (DATA (2)),
		PADDR (msg_bad),
}/*-------------------------< MSG_EXT_2 >----------------*/,{
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_IN)),
		PADDR (dispatch),
	/*
	**	get extended message code.
	*/
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin[2]),
	SCR_JUMP ^ IFTRUE (DATA (EXTENDED_WDTR)),
		PADDRH (msg_wdtr),
	/*
	**	unknown extended message
	*/
	SCR_JUMP,
		PADDR (msg_bad)
}/*-------------------------< MSG_WDTR >-----------------*/,{
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_IN)),
		PADDR (dispatch),
	/*
	**	get data bus width
	*/
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin[3]),
	/*
	**	let the host do the real work.
	*/
	SCR_INT,
		SIR_NEGO_WIDE,
	/*
	**	let the target fetch our answer.
	*/
	SCR_SET (SCR_ATN),
		0,
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_OUT)),
		PADDRH (nego_bad_phase),

}/*-------------------------< SEND_WDTR >----------------*/,{
	/*
	**	Send the EXTENDED_WDTR
	*/
	SCR_MOVE_ABS (4) ^ SCR_MSG_OUT,
		NADDR (msgout),
	SCR_COPY (1),
		NADDR (msgout),
		NADDR (lastmsg),
	SCR_JUMP,
		PADDR (msg_out_done),

}/*-------------------------< MSG_EXT_3 >----------------*/,{
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_IN)),
		PADDR (dispatch),
	/*
	**	get extended message code.
	*/
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin[2]),
	SCR_JUMP ^ IFTRUE (DATA (EXTENDED_SDTR)),
		PADDRH (msg_sdtr),
	/*
	**	unknown extended message
	*/
	SCR_JUMP,
		PADDR (msg_bad)

}/*-------------------------< MSG_SDTR >-----------------*/,{
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_IN)),
		PADDR (dispatch),
	/*
	**	get period and offset
	*/
	SCR_MOVE_ABS (2) ^ SCR_MSG_IN,
		NADDR (msgin[3]),
	/*
	**	let the host do the real work.
	*/
	SCR_INT,
		SIR_NEGO_SYNC,
	/*
	**	let the target fetch our answer.
	*/
	SCR_SET (SCR_ATN),
		0,
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_OUT)),
		PADDRH (nego_bad_phase),

}/*-------------------------< SEND_SDTR >-------------*/,{
	/*
	**	Send the EXTENDED_SDTR
	*/
	SCR_MOVE_ABS (5) ^ SCR_MSG_OUT,
		NADDR (msgout),
	SCR_COPY (1),
		NADDR (msgout),
		NADDR (lastmsg),
	SCR_JUMP,
		PADDR (msg_out_done),

}/*-------------------------< NEGO_BAD_PHASE >------------*/,{
	SCR_INT,
		SIR_NEGO_PROTO,
	SCR_JUMP,
		PADDR (dispatch),

}/*-------------------------< MSG_OUT_ABORT >-------------*/,{
	/*
	**	After ABORT message,
	**
	**	expect an immediate disconnect, ...
	*/
	SCR_REG_REG (scntl2, SCR_AND, 0x7f),
		0,
	SCR_CLR (SCR_ACK|SCR_ATN),
		0,
	SCR_WAIT_DISC,
		0,
	/*
	**	... and set the status to "ABORTED"
	*/
	SCR_LOAD_REG (HS_REG, HS_ABORTED),
		0,
	SCR_JUMP,
		PADDR (cleanup),

}/*-------------------------< HDATA_IN >-------------------*/,{
/*
**	Because the size depends on the
**	#define MAX_SCATTERH parameter,
**	it is filled in at runtime.
**
**  ##==< i=MAX_SCATTERL; i<MAX_SCATTERL+MAX_SCATTERH >==
**  ||	SCR_CALL ^ IFFALSE (WHEN (SCR_DATA_IN)),
**  ||		PADDR (dispatch),
**  ||	SCR_MOVE_TBL ^ SCR_DATA_IN,
**  ||		offsetof (struct dsb, data[ i]),
**  ##===================================================
**
**---------------------------------------------------------
*/
0
}/*-------------------------< HDATA_IN2 >------------------*/,{
	SCR_JUMP,
		PADDR (data_in),

}/*-------------------------< HDATA_OUT >-------------------*/,{
/*
**	Because the size depends on the
**	#define MAX_SCATTERH parameter,
**	it is filled in at runtime.
**
**  ##==< i=MAX_SCATTERL; i<MAX_SCATTERL+MAX_SCATTERH >==
**  ||	SCR_CALL ^ IFFALSE (WHEN (SCR_DATA_OUT)),
**  ||		PADDR (dispatch),
**  ||	SCR_MOVE_TBL ^ SCR_DATA_OUT,
**  ||		offsetof (struct dsb, data[ i]),
**  ##===================================================
**
**---------------------------------------------------------
*/
0
}/*-------------------------< HDATA_OUT2 >------------------*/,{
	SCR_JUMP,
		PADDR (data_out),

}/*-------------------------< RESET >----------------------*/,{
	/*
	**      Send a TARGET_RESET message if bad IDENTIFY 
	**	received on reselection.
	*/
	SCR_LOAD_REG (scratcha, ABORT_TASK),
		0,
	SCR_JUMP,
		PADDRH (abort_resel),
}/*-------------------------< ABORTTAG >-------------------*/,{
	/*
	**      Abort a wrong tag received on reselection.
	*/
	SCR_LOAD_REG (scratcha, ABORT_TASK),
		0,
	SCR_JUMP,
		PADDRH (abort_resel),
}/*-------------------------< ABORT >----------------------*/,{
	/*
	**      Abort a reselection when no active CCB.
	*/
	SCR_LOAD_REG (scratcha, ABORT_TASK_SET),
		0,
}/*-------------------------< ABORT_RESEL >----------------*/,{
	SCR_COPY (1),
		RADDR (scratcha),
		NADDR (msgout),
	SCR_SET (SCR_ATN),
		0,
	SCR_CLR (SCR_ACK),
		0,
	/*
	**	and send it.
	**	we expect an immediate disconnect
	*/
	SCR_REG_REG (scntl2, SCR_AND, 0x7f),
		0,
	SCR_MOVE_ABS (1) ^ SCR_MSG_OUT,
		NADDR (msgout),
	SCR_COPY (1),
		NADDR (msgout),
		NADDR (lastmsg),
	SCR_CLR (SCR_ACK|SCR_ATN),
		0,
	SCR_WAIT_DISC,
		0,
	SCR_JUMP,
		PADDR (start),
}/*-------------------------< RESEND_IDENT >-------------------*/,{
	/*
	**	The target stays in MSG OUT phase after having acked 
	**	Identify [+ Tag [+ Extended message ]]. Targets shall
	**	behave this way on parity error.
	**	We must send it again all the messages.
	*/
	SCR_SET (SCR_ATN), /* Shall be asserted 2 deskew delays before the  */
		0,         /* 1rst ACK = 90 ns. Hope the NCR is'nt too fast */
	SCR_JUMP,
		PADDR (send_ident),
}/*-------------------------< CLRATN_GO_ON >-------------------*/,{
	SCR_CLR (SCR_ATN),
		0,
	SCR_JUMP,
}/*-------------------------< NXTDSP_GO_ON >-------------------*/,{
		0,
}/*-------------------------< SDATA_IN >-------------------*/,{
	SCR_CALL ^ IFFALSE (WHEN (SCR_DATA_IN)),
		PADDR (dispatch),
	SCR_MOVE_TBL ^ SCR_DATA_IN,
		offsetof (struct dsb, sense),
	SCR_CALL,
		PADDR (dispatch),
	SCR_JUMP,
		PADDR (no_data),
}/*-------------------------< DATA_IO >--------------------*/,{
	/*
	**	We jump here if the data direction was unknown at the 
	**	time we had to queue the command to the scripts processor.
	**	Pointers had been set as follow in this situation:
	**	  savep   -->   DATA_IO
	**	  lastp   -->   start pointer when DATA_IN
	**	  goalp   -->   goal  pointer when DATA_IN
	**	  wlastp  -->   start pointer when DATA_OUT
	**	  wgoalp  -->   goal  pointer when DATA_OUT
	**	This script sets savep/lastp/goalp according to the 
	**	direction chosen by the target.
	*/
	SCR_JUMPR ^ IFTRUE (WHEN (SCR_DATA_OUT)),
		32,
	/*
	**	Direction is DATA IN.
	**	Warning: we jump here, even when phase is DATA OUT.
	*/
	SCR_COPY (4),
		NADDR (header.lastp),
		NADDR (header.savep),

	/*
	**	Jump to the SCRIPTS according to actual direction.
	*/
	SCR_COPY (4),
		NADDR (header.savep),
		RADDR (temp),
	SCR_RETURN,
		0,
	/*
	**	Direction is DATA OUT.
	*/
	SCR_COPY (4),
		NADDR (header.wlastp),
		NADDR (header.lastp),
	SCR_COPY (4),
		NADDR (header.wgoalp),
		NADDR (header.goalp),
	SCR_JUMPR,
		-64,
}/*-------------------------< BAD_IDENTIFY >---------------*/,{
	/*
	**	If message phase but not an IDENTIFY,
	**	get some help from the C code.
	**	Old SCSI device may behave so.
	*/
	SCR_JUMPR ^ IFTRUE (MASK (0x80, 0x80)),
		16,
	SCR_INT,
		SIR_RESEL_NO_IDENTIFY,
	SCR_JUMP,
		PADDRH (reset),
	/*
	**	Message is an IDENTIFY, but lun is unknown.
	**	Read the message, since we got it directly 
	**	from the SCSI BUS data lines.
	**	Signal problem to C code for logging the event.
	**	Send an ABORT_TASK_SET to clear all pending tasks.
	*/
	SCR_INT,
		SIR_RESEL_BAD_LUN,
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin),
	SCR_JUMP,
		PADDRH (abort),
}/*-------------------------< BAD_I_T_L >------------------*/,{
	/*
	**	We donnot have a task for that I_T_L.
	**	Signal problem to C code for logging the event.
	**	Send an ABORT_TASK_SET message.
	*/
	SCR_INT,
		SIR_RESEL_BAD_I_T_L,
	SCR_JUMP,
		PADDRH (abort),
}/*-------------------------< BAD_I_T_L_Q >----------------*/,{
	/*
	**	We donnot have a task that matches the tag.
	**	Signal problem to C code for logging the event.
	**	Send an ABORT_TASK message.
	*/
	SCR_INT,
		SIR_RESEL_BAD_I_T_L_Q,
	SCR_JUMP,
		PADDRH (aborttag),
}/*-------------------------< BAD_TARGET >-----------------*/,{
	/*
	**	We donnot know the target that reselected us.
	**	Grab the first message if any (IDENTIFY).
	**	Signal problem to C code for logging the event.
	**	TARGET_RESET message.
	*/
	SCR_INT,
		SIR_RESEL_BAD_TARGET,
	SCR_JUMPR ^ IFFALSE (WHEN (SCR_MSG_IN)),
		8,
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin),
	SCR_JUMP,
		PADDRH (reset),
}/*-------------------------< BAD_STATUS >-----------------*/,{
	/*
	**	If command resulted in either QUEUE FULL,
	**	CHECK CONDITION or COMMAND TERMINATED,
	**	call the C code.
	*/
	SCR_INT ^ IFTRUE (DATA (S_QUEUE_FULL)),
		SIR_BAD_STATUS,
	SCR_INT ^ IFTRUE (DATA (S_CHECK_COND)),
		SIR_BAD_STATUS,
	SCR_INT ^ IFTRUE (DATA (S_TERMINATED)),
		SIR_BAD_STATUS,
	SCR_RETURN,
		0,
}/*-------------------------< START_RAM >-------------------*/,{
	/*
	**	Load the script into on-chip RAM, 
	**	and jump to start point.
	*/
	SCR_COPY_F (4),
		RADDR (scratcha),
		PADDRH (start_ram0),
	/*
	**	Flush script prefetch if required
	*/
	PREFETCH_FLUSH
	SCR_COPY (sizeof (struct script)),
}/*-------------------------< START_RAM0 >--------------------*/,{
		0,
		PADDR (start),
	SCR_JUMP,
		PADDR (start),
}/*-------------------------< STO_RESTART >-------------------*/,{
	/*
	**
	**	Repair start queue (e.g. next time use the next slot) 
	**	and jump to start point.
	*/
	SCR_COPY (4),
		RADDR (temp),
		PADDR (startpos),
	SCR_JUMP,
		PADDR (start),
}/*-------------------------< WAIT_DMA >-------------------*/,{
	/*
	**	For HP Zalon/53c720 systems, the Zalon interface
	**	between CPU and 53c720 does prefetches, which causes
	**	problems with self modifying scripts.  The problem
	**	is overcome by calling a dummy subroutine after each
	**	modification, to force a refetch of the script on
	**	return from the subroutine.
	*/
	SCR_RETURN,
		0,
}/*-------------------------< SNOOPTEST >-------------------*/,{
	/*
	**	Read the variable.
	*/
	SCR_COPY (4),
		NADDR(ncr_cache),
		RADDR (scratcha),
	/*
	**	Write the variable.
	*/
	SCR_COPY (4),
		RADDR (temp),
		NADDR(ncr_cache),
	/*
	**	Read back the variable.
	*/
	SCR_COPY (4),
		NADDR(ncr_cache),
		RADDR (temp),
}/*-------------------------< SNOOPEND >-------------------*/,{
	/*
	**	And stop.
	*/
	SCR_INT,
		99,
}/*--------------------------------------------------------*/
};

/*==========================================================
**
**
**	Fill in #define dependent parts of the script
**
**
**==========================================================
*/

void __init ncr_script_fill (struct script * scr, struct scripth * scrh)
{
	int	i;
	ncrcmd	*p;

	p = scrh->tryloop;
	for (i=0; i<MAX_START; i++) {
		*p++ =SCR_CALL;
		*p++ =PADDR (idle);
	}

	BUG_ON((u_long)p != (u_long)&scrh->tryloop + sizeof (scrh->tryloop));

#ifdef SCSI_NCR_CCB_DONE_SUPPORT

	p = scrh->done_queue;
	for (i = 0; i<MAX_DONE; i++) {
		*p++ =SCR_COPY (sizeof(struct ccb *));
		*p++ =NADDR (header.cp);
		*p++ =NADDR (ccb_done[i]);
		*p++ =SCR_CALL;
		*p++ =PADDR (done_end);
	}

	BUG_ON((u_long)p != (u_long)&scrh->done_queue+sizeof(scrh->done_queue));

#endif /* SCSI_NCR_CCB_DONE_SUPPORT */

	p = scrh->hdata_in;
	for (i=0; i<MAX_SCATTERH; i++) {
		*p++ =SCR_CALL ^ IFFALSE (WHEN (SCR_DATA_IN));
		*p++ =PADDR (dispatch);
		*p++ =SCR_MOVE_TBL ^ SCR_DATA_IN;
		*p++ =offsetof (struct dsb, data[i]);
	}

	BUG_ON((u_long)p != (u_long)&scrh->hdata_in + sizeof (scrh->hdata_in));

	p = scr->data_in;
	for (i=MAX_SCATTERH; i<MAX_SCATTERH+MAX_SCATTERL; i++) {
		*p++ =SCR_CALL ^ IFFALSE (WHEN (SCR_DATA_IN));
		*p++ =PADDR (dispatch);
		*p++ =SCR_MOVE_TBL ^ SCR_DATA_IN;
		*p++ =offsetof (struct dsb, data[i]);
	}

	BUG_ON((u_long)p != (u_long)&scr->data_in + sizeof (scr->data_in));

	p = scrh->hdata_out;
	for (i=0; i<MAX_SCATTERH; i++) {
		*p++ =SCR_CALL ^ IFFALSE (WHEN (SCR_DATA_OUT));
		*p++ =PADDR (dispatch);
		*p++ =SCR_MOVE_TBL ^ SCR_DATA_OUT;
		*p++ =offsetof (struct dsb, data[i]);
	}

	BUG_ON((u_long)p != (u_long)&scrh->hdata_out + sizeof (scrh->hdata_out));

	p = scr->data_out;
	for (i=MAX_SCATTERH; i<MAX_SCATTERH+MAX_SCATTERL; i++) {
		*p++ =SCR_CALL ^ IFFALSE (WHEN (SCR_DATA_OUT));
		*p++ =PADDR (dispatch);
		*p++ =SCR_MOVE_TBL ^ SCR_DATA_OUT;
		*p++ =offsetof (struct dsb, data[i]);
	}

	BUG_ON((u_long) p != (u_long)&scr->data_out + sizeof (scr->data_out));
}

/*==========================================================
**
**
**	Copy and rebind a script.
**
**
**==========================================================
*/

static void __init 
ncr_script_copy_and_bind (struct ncb *np, ncrcmd *src, ncrcmd *dst, int len)
{
	ncrcmd  opcode, new, old, tmp1, tmp2;
	ncrcmd	*start, *end;
	int relocs;
	int opchanged = 0;

	start = src;
	end = src + len/4;

	while (src < end) {

		opcode = *src++;
		*dst++ = cpu_to_scr(opcode);

		/*
		**	If we forget to change the length
		**	in struct script, a field will be
		**	padded with 0. This is an illegal
		**	command.
		*/

		if (opcode == 0) {
			printk (KERN_ERR "%s: ERROR0 IN SCRIPT at %d.\n",
				ncr_name(np), (int) (src-start-1));
			mdelay(1000);
		}

		if (DEBUG_FLAGS & DEBUG_SCRIPT)
			printk (KERN_DEBUG "%p:  <%x>\n",
				(src-1), (unsigned)opcode);

		/*
		**	We don't have to decode ALL commands
		*/
		switch (opcode >> 28) {

		case 0xc:
			/*
			**	COPY has TWO arguments.
			*/
			relocs = 2;
			tmp1 = src[0];
#ifdef	RELOC_KVAR
			if ((tmp1 & RELOC_MASK) == RELOC_KVAR)
				tmp1 = 0;
#endif
			tmp2 = src[1];
#ifdef	RELOC_KVAR
			if ((tmp2 & RELOC_MASK) == RELOC_KVAR)
				tmp2 = 0;
#endif
			if ((tmp1 ^ tmp2) & 3) {
				printk (KERN_ERR"%s: ERROR1 IN SCRIPT at %d.\n",
					ncr_name(np), (int) (src-start-1));
				mdelay(1000);
			}
			/*
			**	If PREFETCH feature not enabled, remove 
			**	the NO FLUSH bit if present.
			*/
			if ((opcode & SCR_NO_FLUSH) && !(np->features & FE_PFEN)) {
				dst[-1] = cpu_to_scr(opcode & ~SCR_NO_FLUSH);
				++opchanged;
			}
			break;

		case 0x0:
			/*
			**	MOVE (absolute address)
			*/
			relocs = 1;
			break;

		case 0x8:
			/*
			**	JUMP / CALL
			**	don't relocate if relative :-)
			*/
			if (opcode & 0x00800000)
				relocs = 0;
			else
				relocs = 1;
			break;

		case 0x4:
		case 0x5:
		case 0x6:
		case 0x7:
			relocs = 1;
			break;

		default:
			relocs = 0;
			break;
		}

		if (relocs) {
			while (relocs--) {
				old = *src++;

				switch (old & RELOC_MASK) {
				case RELOC_REGISTER:
					new = (old & ~RELOC_MASK) + np->paddr;
					break;
				case RELOC_LABEL:
					new = (old & ~RELOC_MASK) + np->p_script;
					break;
				case RELOC_LABELH:
					new = (old & ~RELOC_MASK) + np->p_scripth;
					break;
				case RELOC_SOFTC:
					new = (old & ~RELOC_MASK) + np->p_ncb;
					break;
#ifdef	RELOC_KVAR
				case RELOC_KVAR:
					if (((old & ~RELOC_MASK) <
					     SCRIPT_KVAR_FIRST) ||
					    ((old & ~RELOC_MASK) >
					     SCRIPT_KVAR_LAST))
						panic("ncr KVAR out of range");
					new = vtophys(script_kvars[old &
					    ~RELOC_MASK]);
					break;
#endif
				case 0:
					/* Don't relocate a 0 address. */
					if (old == 0) {
						new = old;
						break;
					}
					/* fall through */
				default:
					panic("ncr_script_copy_and_bind: weird relocation %x\n", old);
					break;
				}

				*dst++ = cpu_to_scr(new);
			}
		} else
			*dst++ = cpu_to_scr(*src++);

	}
}

/*
**	Linux host data structure
*/

struct host_data {
     struct ncb *ncb;
};

#define PRINT_ADDR(cmd, arg...) dev_info(&cmd->device->sdev_gendev , ## arg)

static void ncr_print_msg(struct ccb *cp, char *label, u_char *msg)
{
	PRINT_ADDR(cp->cmd, "%s: ", label);

	spi_print_msg(msg);
	printk("\n");
}

/*==========================================================
**
**	NCR chip clock divisor table.
**	Divisors are multiplied by 10,000,000 in order to make 
**	calculations more simple.
**
**==========================================================
*/

#define _5M 5000000
static u_long div_10M[] =
	{2*_5M, 3*_5M, 4*_5M, 6*_5M, 8*_5M, 12*_5M, 16*_5M};


/*===============================================================
**
**	Prepare io register values used by ncr_init() according 
**	to selected and supported features.
**
**	NCR chips allow burst lengths of 2, 4, 8, 16, 32, 64, 128 
**	transfers. 32,64,128 are only supported by 875 and 895 chips.
**	We use log base 2 (burst length) as internal code, with 
**	value 0 meaning "burst disabled".
**
**===============================================================
*/

/*
 *	Burst length from burst code.
 */
#define burst_length(bc) (!(bc))? 0 : 1 << (bc)

/*
 *	Burst code from io register bits.  Burst enable is ctest0 for c720
 */
#define burst_code(dmode, ctest0) \
	(ctest0) & 0x80 ? 0 : (((dmode) & 0xc0) >> 6) + 1

/*
 *	Set initial io register bits from burst code.
 */
static inline void ncr_init_burst(struct ncb *np, u_char bc)
{
	u_char *be = &np->rv_ctest0;
	*be		&= ~0x80;
	np->rv_dmode	&= ~(0x3 << 6);
	np->rv_ctest5	&= ~0x4;

	if (!bc) {
		*be		|= 0x80;
	} else {
		--bc;
		np->rv_dmode	|= ((bc & 0x3) << 6);
		np->rv_ctest5	|= (bc & 0x4);
	}
}

static void __init ncr_prepare_setting(struct ncb *np)
{
	u_char	burst_max;
	u_long	period;
	int i;

	/*
	**	Save assumed BIOS setting
	*/

	np->sv_scntl0	= INB(nc_scntl0) & 0x0a;
	np->sv_scntl3	= INB(nc_scntl3) & 0x07;
	np->sv_dmode	= INB(nc_dmode)  & 0xce;
	np->sv_dcntl	= INB(nc_dcntl)  & 0xa8;
	np->sv_ctest0	= INB(nc_ctest0) & 0x84;
	np->sv_ctest3	= INB(nc_ctest3) & 0x01;
	np->sv_ctest4	= INB(nc_ctest4) & 0x80;
	np->sv_ctest5	= INB(nc_ctest5) & 0x24;
	np->sv_gpcntl	= INB(nc_gpcntl);
	np->sv_stest2	= INB(nc_stest2) & 0x20;
	np->sv_stest4	= INB(nc_stest4);

	/*
	**	Wide ?
	*/

	np->maxwide	= (np->features & FE_WIDE)? 1 : 0;

 	/*
	 *  Guess the frequency of the chip's clock.
	 */
	if (np->features & FE_ULTRA)
		np->clock_khz = 80000;
	else
		np->clock_khz = 40000;

	/*
	 *  Get the clock multiplier factor.
 	 */
	if	(np->features & FE_QUAD)
		np->multiplier	= 4;
	else if	(np->features & FE_DBLR)
		np->multiplier	= 2;
	else
		np->multiplier	= 1;

	/*
	 *  Measure SCSI clock frequency for chips 
	 *  it may vary from assumed one.
	 */
	if (np->features & FE_VARCLK)
		ncr_getclock(np, np->multiplier);

	/*
	 * Divisor to be used for async (timer pre-scaler).
	 */
	i = np->clock_divn - 1;
	while (--i >= 0) {
		if (10ul * SCSI_NCR_MIN_ASYNC * np->clock_khz > div_10M[i]) {
			++i;
			break;
		}
	}
	np->rv_scntl3 = i+1;

	/*
	 * Minimum synchronous period factor supported by the chip.
	 * Btw, 'period' is in tenths of nanoseconds.
	 */

	period = (4 * div_10M[0] + np->clock_khz - 1) / np->clock_khz;
	if	(period <= 250)		np->minsync = 10;
	else if	(period <= 303)		np->minsync = 11;
	else if	(period <= 500)		np->minsync = 12;
	else				np->minsync = (period + 40 - 1) / 40;

	/*
	 * Check against chip SCSI standard support (SCSI-2,ULTRA,ULTRA2).
	 */

	if	(np->minsync < 25 && !(np->features & FE_ULTRA))
		np->minsync = 25;

	/*
	 * Maximum synchronous period factor supported by the chip.
	 */

	period = (11 * div_10M[np->clock_divn - 1]) / (4 * np->clock_khz);
	np->maxsync = period > 2540 ? 254 : period / 10;

	/*
	**	Prepare initial value of other IO registers
	*/
#if defined SCSI_NCR_TRUST_BIOS_SETTING
	np->rv_scntl0	= np->sv_scntl0;
	np->rv_dmode	= np->sv_dmode;
	np->rv_dcntl	= np->sv_dcntl;
	np->rv_ctest0	= np->sv_ctest0;
	np->rv_ctest3	= np->sv_ctest3;
	np->rv_ctest4	= np->sv_ctest4;
	np->rv_ctest5	= np->sv_ctest5;
	burst_max	= burst_code(np->sv_dmode, np->sv_ctest0);
#else

	/*
	**	Select burst length (dwords)
	*/
	burst_max	= driver_setup.burst_max;
	if (burst_max == 255)
		burst_max = burst_code(np->sv_dmode, np->sv_ctest0);
	if (burst_max > 7)
		burst_max = 7;
	if (burst_max > np->maxburst)
		burst_max = np->maxburst;

	/*
	**	Select all supported special features
	*/
	if (np->features & FE_ERL)
		np->rv_dmode	|= ERL;		/* Enable Read Line */
	if (np->features & FE_BOF)
		np->rv_dmode	|= BOF;		/* Burst Opcode Fetch */
	if (np->features & FE_ERMP)
		np->rv_dmode	|= ERMP;	/* Enable Read Multiple */
	if (np->features & FE_PFEN)
		np->rv_dcntl	|= PFEN;	/* Prefetch Enable */
	if (np->features & FE_CLSE)
		np->rv_dcntl	|= CLSE;	/* Cache Line Size Enable */
	if (np->features & FE_WRIE)
		np->rv_ctest3	|= WRIE;	/* Write and Invalidate */
	if (np->features & FE_DFS)
		np->rv_ctest5	|= DFS;		/* Dma Fifo Size */
	if (np->features & FE_MUX)
		np->rv_ctest4	|= MUX;		/* Host bus multiplex mode */
	if (np->features & FE_EA)
		np->rv_dcntl	|= EA;		/* Enable ACK */
	if (np->features & FE_EHP)
		np->rv_ctest0	|= EHP;		/* Even host parity */

	/*
	**	Select some other
	*/
	if (driver_setup.master_parity)
		np->rv_ctest4	|= MPEE;	/* Master parity checking */
	if (driver_setup.scsi_parity)
		np->rv_scntl0	|= 0x0a;	/*  full arb., ena parity, par->ATN  */

	/*
	**  Get SCSI addr of host adapter (set by bios?).
	*/
	if (np->myaddr == 255) {
		np->myaddr = INB(nc_scid) & 0x07;
		if (!np->myaddr)
			np->myaddr = SCSI_NCR_MYADDR;
	}

#endif /* SCSI_NCR_TRUST_BIOS_SETTING */

	/*
	 *	Prepare initial io register bits for burst length
	 */
	ncr_init_burst(np, burst_max);

	/*
	**	Set SCSI BUS mode.
	**
	**	- ULTRA2 chips (895/895A/896) report the current 
	**	  BUS mode through the STEST4 IO register.
	**	- For previous generation chips (825/825A/875), 
	**	  user has to tell us how to check against HVD, 
	**	  since a 100% safe algorithm is not possible.
	*/
	np->scsi_mode = SMODE_SE;
	if (np->features & FE_DIFF) {
		switch(driver_setup.diff_support) {
		case 4:	/* Trust previous settings if present, then GPIO3 */
			if (np->sv_scntl3) {
				if (np->sv_stest2 & 0x20)
					np->scsi_mode = SMODE_HVD;
				break;
			}
		case 3:	/* SYMBIOS controllers report HVD through GPIO3 */
			if (INB(nc_gpreg) & 0x08)
				break;
		case 2:	/* Set HVD unconditionally */
			np->scsi_mode = SMODE_HVD;
		case 1:	/* Trust previous settings for HVD */
			if (np->sv_stest2 & 0x20)
				np->scsi_mode = SMODE_HVD;
			break;
		default:/* Don't care about HVD */	
			break;
		}
	}
	if (np->scsi_mode == SMODE_HVD)
		np->rv_stest2 |= 0x20;

	/*
	**	Set LED support from SCRIPTS.
	**	Ignore this feature for boards known to use a 
	**	specific GPIO wiring and for the 895A or 896 
	**	that drive the LED directly.
	**	Also probe initial setting of GPIO0 as output.
	*/
	if ((driver_setup.led_pin) &&
	    !(np->features & FE_LEDC) && !(np->sv_gpcntl & 0x01))
		np->features |= FE_LED0;

	/*
	**	Set irq mode.
	*/
	switch(driver_setup.irqm & 3) {
	case 2:
		np->rv_dcntl	|= IRQM;
		break;
	case 1:
		np->rv_dcntl	|= (np->sv_dcntl & IRQM);
		break;
	default:
		break;
	}

	/*
	**	Configure targets according to driver setup.
	**	Allow to override sync, wide and NOSCAN from 
	**	boot command line.
	*/
	for (i = 0 ; i < MAX_TARGET ; i++) {
		struct tcb *tp = &np->target[i];

		tp->usrsync = driver_setup.default_sync;
		tp->usrwide = driver_setup.max_wide;
		tp->usrtags = MAX_TAGS;
		tp->period = 0xffff;
		if (!driver_setup.disconnection)
			np->target[i].usrflag = UF_NODISC;
	}

	/*
	**	Announce all that stuff to user.
	*/

	printk(KERN_INFO "%s: ID %d, Fast-%d%s%s\n", ncr_name(np),
		np->myaddr,
		np->minsync < 12 ? 40 : (np->minsync < 25 ? 20 : 10),
		(np->rv_scntl0 & 0xa)	? ", Parity Checking"	: ", NO Parity",
		(np->rv_stest2 & 0x20)	? ", Differential"	: "");

	if (bootverbose > 1) {
		printk (KERN_INFO "%s: ini**** SCNTL3/DMODE/D con/CTEST3/4/5 = "
			"(hex) %02x/94  Wolfgang Stanglmeier\n*****	ncr_name(np), ****sv_scntl3ree softwdmodeou can redre; This p softwc****you can re it u4der the terms 5***** driver for the PCI-SCSIfinal  XX controller family.
**
**  Copyright (C) 1994  Wolfgang Stanglmeier
**
**  This program is free so****re; you canrredistribute Thisnd/or modify
r*  it under ththe hopeof the the hoperal 	}****************
**&&ee sopaddr2)Public License as publisheon-chip RAM at 0x%l**  This program is free so even t;
}

/*=he
**  GNU General Public License for more details.
**
**
** You s	Done SCSI commands list management.u shoulWe donnot enter the scsi_done() callback immediately houlafse
*aceived a has been s
** asceivpleted but wewriteinsert it into acopy owhich is flushed outside any kindwriteof driver critical sectionGene	This allows to do minim----tuff underidgeerrupt  Sofss Ave,--------------------sen poe, Mlso avoid locking up------n recursivehis p------------entry pointsis drivSMP-----In fact,**  aonly kernel  Gera39, USA.
*ense
c., y**  a----*
**   with a**
**   
**  setA.
*km----c(GFP_ATOMIC)s drithat shall  Licreense
**  a*
**   s driv----circumstances,----AFAIKGeneralhe
**  GNU General Public License for more details.
**
**  Y/
static inline riverprogqueue with_cmd(struct ncb *np, iginal long cmnd *cmd)
{
	unmap_long wata(r ha 386;
	cmd->host_scribble = (char *)ee so*  Thopy ;
dify
cologne.d =      Seehe FreeBSD 
**  version*
**-**  The osriginal written forl 386bsd s been written for 38****while (portevice cmd = port;
		porter  
**  And has been )    fgang Stanglmeimycr-----long with        } See the
**  GNU General Public License for more details.
**
**  You shoulPrepare**  anext negotia---- message if neededGeneral Fill in**  apart ofd Roudierbu****  to kcontains this dri95 by GerardD NCRer 10go_he Fus field*
**64 bCCB-----Returoudier:size (Alpha) Roudier:n bytesGeneral  received on Linux are automatically a potential candidate for ter         l drr_p  Dece_its riginal driver has been crivecp, u_      msgptred to NetBSDtrivetp = &ify
target[cSupport f];
	rstimsglen = 0RAM.
**its ay 3 19
**  And hasport f *s  Full = t sofport f****/*995 by Gee w----transfers ?  */******! scsre-fwithnnum  ****spi_support_re-f(i scrip)nnum        = NS_WIDEmycr} elseo.uk1997 by Rich=1 WITHOUnstructions psynchronousetching.
*
**  May 19 >:
**&&  1997perioannum  ltham <dormouse@st 1robt.demon.co.uk>:
**     SYNCort for Nv.co.uk   Support  =0xffffmycr	dev_info(&i scrip->dev, "  Full dversergy ouse erar.\n****		}WITHOUswitch (its nnum caseby Gerar:
		
**  Ma+= m <dpopulatedian)_msg(port f +*
**  MThis   Supmaxoff**
*essivinst 1 : 0,CRIPTSe SCSInormabreak   Wraffic Suppcommand handling when on-chwidthAM is present.
**    CRIPTusrre-f2005 by MatthTHOUr onits architec=by Ge******** Low PCI   Supits ac*   cporma****DEBUG_FLAGS & c).
**NEGOn.co.uk>g.
**intAM is:
**>:
**      Supp ? Aggr	  "re-femsgout":"ip RAM i***
,****t foormal casesrNovem*
**  M  Seeee the
**  GNU General Public License for more details.
**
**  You shoulSt.
**execuerardof ave received a----------ishis ped fromlpha)genericve rec*
**      Support for Fast-20 scsi.
**     Support for large DMA fifo and 128 ords bursting.
.
**
*Free Sofriginal driver has been written for 386bsd 
**  And hasdevice *sdevtefan   O(1ic----st-40 scsi.
**     Support foER_N->id RAMiginal lrivel*    scslpG_FLAGSlun0)

#includudier:
*****nt	segGNU s;
	     S	idmsg,upport fdma-32mand haRAM.
*	dir------<linux/lastp, goal
#inc/*-x/interrupt.h>
#include <linux/ioport.h>
#in
	**lude nux/mSome shortcuts ...lude <lix/interrupt.h>
#include <linux/ioport.h>
#includ May 19(_FLAGS	(****ify
myeven	  ) ||****include <>= MAX_TARGET>
#include <lilunnux//stat.hLUN   on.co.ues:
**(DID_BADh>
#inclreading.
x/interrupt.h>
#include <linux/ioport.h>
#include <li	Con, Inclpha)1st 
**
 UNIT READYceived acludeginaterrorerardierardiAlpha)NCR_DRIis cludeflagged NOSCAN,dierordrivto speedd is lude64 b****ux/module.h>
#include <linux/sched.h>
#include <linux/signal.h>
#	"ncren f[0]****0 ||E	"ncrclude <scsi/x12)
** 
	nux/(support#inc & UF_ <scsion.co.u53c8xx.h"

#d= ~fine NAMEormaes:
**	nclude <linux/ WITHOUT ANc).
**
*************TINYnnum  PRINT_ADDRh>
#, "CMD=%x****_transport_stimer.h>
#include <linux/types.h>

#include <asm/dma.h>dma.h>
#includeAssignhe Fcb / b----cmdux/mo	If resetting,#inclu**  Fttle_timer:
** cRoudryscsi/csi_dbg.h>
#driverspuri97 byime----002)
#define DEBUG or no freeG_PH,ESULT  , Camft@gdge, 64 bwaine DEopy ux/module.h>
#include <linux/sched.h>
#include <linux/<linux/signal.h>
ify
*0x0008)
#de&&======request->OINTER >
#iHZnnum  u_long tlimi forjiffies +(0x0400)
#define DEBUG_- HZobe logiOINT_ to tine DEBUG_TAGS  ,
#defin)the dify
*0x0008)
#de=
#defin WITHOUT ANd at runtime too|| !(cp=****get__PHA**       on.co.u)
#def_dge,_     (0gne.d**          me.h>
#incluOKtimer.(see ft@gnu     Ch=====================
*/

#define DEBUG_ALLOC    (0x0>
#includeBuilr 64 bid****fy / tag / sdtrrard Rou
#define DEBUG_TIMING   (0x0100)
#define DEBUG_NEGO     (0x0200)

ng.h>
 = IDENTIFY(0, nux/strin*********cp uppog != NO_TAG
#inclucp
}

ify
_PHAwithoutdisc
**  r53c8xx.h"

#define DISC)changst_del|.h>
40**** presens pr-------smsg;uddy-  May 3 19ted SC[This s++] = g.h>
}

	returnNULL;
}

/*====nnum       _dbg.h======_dbg.l Pub/*
	2)
#Force 2 aliedt_heafine DEBUG_R10)
#definOINTER   ions.
D NCR53pine rvudieteractivity.ions/be logilp====able debug EBUG_IC, lut to s_sOINTon.co.ukle andions  frmapn.co.uk	2 aligneORDERED_QUEUE====64 tal.h>
#=============
**
**	DAGS)||*********
*>2){it 
	g tags
**
**=====ced in	"ce the SCRIPf*	Sidfor normamal cdev to ations  from te DEBUG_IC   3*Enableort dynamicma*   ode.
**	Iumarobe THOUPTS c2 align= 0n.co.ukation2)
#Oe the Swrite ops, unce the Sread====lowss simps.
**    =============.co.ukew Wi0x08:  /*syste_SMALL (6)*  Mabytes min2mum memory cBIG  (10#if PAGE_SIZE >amum memory cHUGE (
#inf PAGE cache liSIMPLEnment is guarantby Matthtagsfaultcomm, cache line alignment is guaranv tov toode is not intendedmory alocations.
Actualt_hes ecemnumbthe S1,3,5,..2*MAXche +1to p**	since5 Mamay have-----ealiginat53c8xxudieatT	(PAG MEMO_problemsiginat#====0EBUGtoo great	(1UL _GFP_F===== simpode is not intended but to 
<< Dev+  reading.
 DEBUG_TIMING   (0x0100)
#define DEBUG_NEGO     (0x0200d *ncr_list_pop( by
 detangptors (!list_empty(head)) {
		struct list_head *elem = head->next;

		lilude <linAME	"ncrscD by
_lude <linux/***** Link bet!= DMA_NONE* 16 b <linux/gnedcr_scatbug me, :
**ine DEBSI-IIltham;

typed<*========@mi.Ureet nctob {		r pcide.h>
#incluERRORSI-II feat	ier
**   but memor  May 3 19_s;

typedef3 19def u_long m_addr_t;	/* Enough bits to bit-hack addresses*/
typede95 by Gerard0)
#ired?*/
typedealon architec.
**iand qbying.
**
**  Februachandefine DEBUG_TIMING   (0x0100)
#define DEBUG_NEGO     (0x;

		lisee the
**     NCRo bud.h>
# 1997 by RichUPPOR   Support f**     Sr the bu&& ldditionand handling.
**
**  Februr_debugported SCnt.
**   =============================
*/

#define DEBUG_ALLOC    (0x0ine VTOB_HDeterm 
**x996 lude <lin)
#define DEBUG_TIMING   (0x0100)
#define DEBUG_NEGO     (0x0200)
#defi! m_vtob_s;
#the * Link betweext;
} mGS	SCS02)
#defevice  Link betis BIDIRECTIONAL,#incc on-c FROM_DEVICEscsi/ 675**
**   altern *h  Gera.
**for TO->h;

	dierew Wiscsi/of our_link_s * betwort be ju2139rong002)
#SCRIPTSs <<= swap valuesr:
**     In MEMO.
**  ruct m_lin PCI traffext;m_addr_t a;
	:ORDER)) {
	))
		retucommclude*   CB_i;
	wh_PHYSr_debumemorout21)

8 to Bus address trstat.hSCATTERLhange.h>
# =nclude - 8 -s address t* 16SI-IIier
**    	}
	a = (h[j].nextH)
				h[j].hnext->nextd 
**	}
	a -nu.a;

typed-		++j;
		s <<=
	if ormal c
	struct m_link *next;m_addr_t a;
	hangeS maximumbut phys.header.wclude	s pru_toStan(clude			j BUG
	printk("___m= 1;
c(%d) = %p\n",= 1;
			j /* fp sythroughse
#dDER)) {
	= mp->h;

	(mp);
			if (h[j].next)
				h[j].next-inxt = NULL;
			break;
		}
		++j;
		s <<= 1;
	}
	a = (m_addr_t) h[j].next;
	if (a) {
		h[j].next = h[j].next->next;
		while (j > i< ME		j -= 1;
			s >>= 1;
			h[j].next = (m_link_s *) (aby Matth*/
#endif
DER)) {
	
} mcomm	}
	a = (m_addf (h[j].next)
				h[j].noD by
ly the 720 chip ((1 << MSet----EMO_PAGE_Oext) {
*     m)	\i;
	whi002)
#defT);
	int j;
	unknownhas .
**at= (1 <<o= (PAGEBUG
	printk("___	}
	a = (void *) a;
}

statiBUG
	printk("___;
			if d) = %p\n", size, (/
	struct m_link=xt->next = NULL;
		}
	}BUG
	printk("___save (m_PAGE_) = %p\n",].next->next;
		while ndif
		)if (r NvRAMxt = h[i].next;
			h[le (q->next && q->nex_UNUSED
		(PAGEop(sNCR538XeviceMO_PAGEscsi_dbg.h>
#be ameiescsi/so redG_TINYion
**	  = a ^ s;
k;
		{
			q = h[i].next;
			h[GS	SCSI_NCR_DEBUG_FLAGS
#endif

static inline struct list_head *ncr_CODrn NUcb) >> MEMO_CLUSTER_SHIFT) & VTOB_HASH_MASK)

typedef struct ine VTOB_scsi/	pri------> vir1
#derogrlinkscsi/G    Parity ch>

#inclu		liNUSED
		;
		.
**
nt size, char *.schedule.l_ even  
			q = q->nex(h[j].next)
				h[j].selectxt = , inresize);

	return p;
}

#fine __m_calloc(mp, s, n)	__m_calline l_dsaxt = (1 << Moc2(mpnt size, c	prinoc2(mpDEBU_id		=urn e_idude <next != (m_liEBUG_ALLOC)ion.
*	/blkdewval %-10s[%4d] @%p.\n", namPAGEze, ptrs;

	__(1 << M)
{
	if (!UG_FLAGS & DEmsg.nlockc(%d) = %p\n",CCB)
				h *, ator.
**
, s, n, Me use a de 199t pool of unmap;
	int nuh pci busX ": failed 	memcpy but cdb_buf===========,----_t(int=========ds;
#,0 199ofessed by the)ry we donnot n0x00fault pool of unmapped memory 
 * by the====tic m_addr_t ___mo DMA from/to and odefault not l per pcidevrchitesupport, wa	1
#dB_HAkst po3 19, inang Stchite	t poole the
**     N? HS*****TIATE :
	frBUSY___mp0_ator.
m_pool_s *S_ILLEGALext != (*****	--mp->nul of a mp, xerr	--mp->nump;XE_OK;
#if 0E_ORDERp RA--mp->nump;/*
 * With mp, re-fs.
 */

/*
 * Wi);

	_#endifS	SCSI_NCR_DEBUG_FLAGS
#endif

static inline struct list_head *ncrC--------region:ak;
		}t----job)
#define DEBUG_TIMING   (0x0100)
#define DEBUG_NEGO     (0x0200)
ng.
*cator *h tions.
 ***  Mamp, magicLL, ped MAGIC per pcidev)
#defi10 19CCBsDEBUG_k;
		}.
**
002)
#2 max}
#east 
*ing gifree(to**
**-lpha).
*Y    ;
		vp = (eturn m;u %p\ensa ma3 199e andthe  stru;
		_10 1uct m_vtolp, 	if (r NvRAM*****uep(m_rtntfly m_vtob *neng.
*Cree Sofis sucDEBUfully;
		vpd_calloures:
**	g
#elsnchron the
**  GNU General Public License for more details.
**
**  You shoulI
#defia					EBUG_TINYdaddr;
		vp for waked isthis drii;
	whilproDEBUong
**	    Etc...
**
**	Supported NCR/SYMBIOS chips:
**		53C720		(Wide,    Fast SC  version);
		if (vp) {
iginal driver has been e <linu/scs of xn */
#define opy _tk(" *qrobeinux/delay.h>
#inclS code.heldt ncthe es:
**  Charles Mr !=--ruct m->vbp->nccb tra_coherent(epth* 16 bq (m_ str &(*vpop(&_coh    t ncql to Bus !qFP_AT
#ifdef DE++_coherent(mp-> (voided a&(*v     (q Gerard Roudi, N)
	p->vaddr,  sizeadd_tailbp), void busy>nump;
	}
}p->jumpt ncor on-Bg******=====? 0 :*mp, tag] =next = (m_link_ped memory 
 *EMO_WAR_freeSH_CODE(vp);
			vbp->vaddr = v------ASH_CODE(m);

	vDE(vp);
			vbp->ry 27 1997 by Gerard Roudier:
6bsd a16	qidxVTOB");
	if (vbp) dr_t daddr;
		vp = (cs.nmt.edRDER.
**
put<wolf@0, sizeof =
type(&mpgned me;
		mp->bu+ 2*/
	strush;
/stat.hSTART +reep = ___) ush;
		1 vp;mp, 0_bushh->tryloop [(&mpnded = (m_link_s *) a;
		)
				h[j].idle_freeMEMORY_BARRIER(tati0.next;
		mp0.next = mmp, 0, sizeof}
	return mp;
}ped memory 
 * sh; mp =truct m
		mp->bush(&mp0,	++mp, __m_free(&mp0mp, vbp->next = mp=================
**
**	ment the implied -SCSI, sizeos=%**	pe,ORDERram r_defree sof, sizeof(}
}

static _bushd ___dma_fSHIFTbNY     (0xRDERt sizG_ALscsi/Weturit upint sizm_pool_s *p)
{
	strOUTB (nc_i--mp, SIGP.  See Fast SCSI-2, inine DreeBSDbu**
**  Andriver havaddenabG_FLmp)
		 runtimdelay6bsd a32 _linRAM.
**retNAMEo budd at runtime to	 DEBUG_IC   (mp, size, n * Enab***************
**  DePublic Lialloc_ine DEBUG_Qright Free Sof ___dmae DEsuspenO_PARDER%d----onds*  This program is fre(mp, size, naext;
cr_RCHA= ___ctob {100tatiuze, n(20irqsc voThe 895 MEMO_POINTm, inscsi_us istrh>
#i0x000*  May 19m = __m_
	reOUTW__getsit DMRS/time(1 << MEnd *__Toleram_caine D IRQDPTS tic, neen porMEMO_CLperlyreeBSIRQl(bus, pri MEMO
#define DEdma_pooint siz = ___get*****3, TE
	mp = ___getion.
1, Cf (mp &e(&ncr53c8}
		if (!!*
**  _setup.bus_check
	regoto----p && !mp->C
	m_m, innof (mpinah_t;EBUGe recpoolinclu-----grou, int *  Nadrity chvicebus}
		q-ux.
ity bitBSD NCrardrol e DEal (0x002We MEMOexp----ng RESETtic voiTRUE for otherma_pool(tic voi_lockFALSEint si
	_lin =	INB	u_lo--mprqsav		vp = ((		vp & 2)SIZE71)

k_irqrest1re(&n17x_lockrst sdp0se
#d		vp |= ((vp->next;
	}#incmini flags26) |c vosdp1nux/m simp((m_W	u_lobdl- a) :ff)  SIZE9) fine _d7-0lloc_dma(np, s, n)		__m_calloc_00flags)0efine _d15-8loc_dma(nvp->nextbcl
	retureq ograbscr53l at*	    cd ioloc_dmat hc = NCR_Dfeatures & FEox anchangirqres.h>
3 to 64of(*p)		vp != (2<<7on.co.uturn m;
}

sh, vicBUG_Pave(&ncr53arles 
static m_addrBUSt bushis program is fif (sturn m;
}

s%p ? ,n)
#,rst,req,ack,bsy,sel,atn,h>
#c/d,i/**  right Y or ,mp) {
		vp Y or FITNESS FOR A PARTICUr_t)e m_free_dma(p, s, n)		_ ? "dp1,->dev," ****ped	SCx0800))_lin, 
static obus(np to Bus VTOB_HASH_CODE(m);
	m_{
		

	re = (NAME readioudif
m)
{
	u_long flagrqsaves:
**	cmd)  See t
 * )
**	 a_pool ___dma.map(define Ddierprogress----nothi}

	 ck, fler has beehandlorigil addPINLOCK1996ool_shipt __map_se DEBUG_a(struct deviPAGE_RDER runtime toobefore  __mclear	vp ieen pos}

stum	vp ee_dma(m_bush_t but __  Fast SCE(m);

	vbpp = 	spin_iginal driver 6bsd mset(mp, 00x0008)
#d* 16 bmp = ___cre_dma_poolock_,checkinASH_CODags;
	struct m_ ----- >vtob[hc] = vbp;
			++mp->nump;
			return vp;
		}
	}
	if (vbp)
		__m_frRta_maNE_Se rec, p)   Tagged command queuing
**	    Parity checking
**	    Etc...
**
**	Supported NCR/SYMBIOS chips:
**		53C720		(Wide,   Fast SCSI-2, in ___crpoolriginal driver has been written for 38calloc(p RAine D6bsd/p->nfine SCSI_NCR_DRI====
*/*==========ME	"ncr53c8xx-_dma(inux/delay.h>
#iM.
**f	spi;a_unmap( Novemam; if not, wi_data_mapaddred = 0;
}	cmd->CSI_NCR_DEBUG_INFO_Sux/time.h>
# FAILEDatic unmap(cmd);
NE_S		break;
	}
	cmd->, flxt;
		ct m_pool *iudierundasume SCc voistopp  In 			vbp->bR_SI synew
	vomp0, p,to Linu     (0x0080 until a
	if (mpULE restorof 2size, chs <<= 1;
ion, Inc.T_COMMAng = use_sg;

	rnstatunmap(Fi* To lookiver_setupkeup, siz_COMMARDER(SI_NC=0ol *========; procommmp, -mp->num* 16 bations.
=====get_dma__PHA(Alph com *mp, ints simple amp0_freep(m_poo{
		HS_IDLE)___geinu-----/

#ifdeft@gnG_FLA* 16 bySI_NC
{
	swit)vbp->baddl casunmap(Tht DM========
**
**   (0x0080_COMMAND_L!

#def&&	scsrieve_ueuiGS;
	#define D0ree bug = S
if

#define Ounmap(irqs-up (s =aefine OPeived a cginatnclu= mp-T_COMMA ___crS;
	#define DEB========= OPT_UNUSED_void#define OPT_FORCE_HSC_NEGOflagSYNC_NEGO	7
#defol ***	Dridata(ERBOSE		=========IAlpha)involve ___ee Sofwas(C aniEBUG*
**   .
**
, for 64 bULE longAX_WIDE	told usE
#ifdef	Sst 18 1997 define OPTIN		13
#ifine Of (!uur****MODUer_setup.verbose)
,=========		reORCE_SYNC_NEGO(*pptus,ULE csi_dbg.h>
#keep		retliveT_COMMAND_L	3
#define==========ddr_t OPT_DISCONNECTION	4
#define OPT_SPECIAnum      MEMOulbushScsiR OPT_inclu= mp- {
	casG		11.
**
**  The or*          atures:
**	SUCCESS  See * DM memunused for broken..md-> the
**  GNU General Public License for more details.
**
**  You shoulAbCRIPanonnection
**	    Tagged command queuing
**	    Parity checking
**	    Etc...
**
**	Supported NCR/SYMBIOS chips:
**		53C720		(Wide,   Fast SCSI-2, inaata problems)
*/

/* Name and version of the driver */
=========================
*/
static struct ncr_driver_setup
	driver_setup			= SCSI_NCR_& !mp->nuR_DRIVER_============get_dma_TLE_DPT_IRQM		Y	2
#define OPT_SCSI_PARITY	remoISCONNECTION	4
#defin_EXCLUDE		24
#define OPT_HOST_ID		25

#ifdeABORSI_NCR_IARB_SUPPORT
#define OPT_IARB		2endif

#iCSI= NULL_ifdef MOD26
#e OPT_MASTER_PARITY	2
#def	Driver setup from the boot command line
**
**===================================================================
*/

#ifdef MODULE
#define	ARG_SEP	' '
#else
#define	ARG_SEP	','
#endif

#define OPT_TAGS		1
#definfine OPT_SAFtic struct nc
		++i;
		iNOT_RUNNINguarf SCSI_NCR_DEBUG_INFO_Scur, ':')) != NULL) {
		SNOOZportEE_UNUSED
	define vbp, s *vbp;, fla*   

	retur l(m__ORDER OPT_EMO_CI_NCR_0)
#ata =ool_s *mp, int siGE_SIZE <mp0_freep(m_poo PCI traffMEMO_PA s) {
			free_pages(comm *__m_calloc_, &pe,ccb=%p (c andl) bush, int size, chab *next;me, size);

	return p;
}

# = mpt = (m_link_s *) a;
			break;
		}
 == '/'L) {
		+NAME
		++i;
		iPEND;

		v by Matthew WiHS_
**	ONNECTcommn, MEMO_WARN)

static void __m	i < sizeof(driver_setup.tag_ctrl)-1) , &pe	driver_setup.tag_ctrl[i++] = *pe++;
				}
*/
#endif
#r_setup.tag_ctrl[ichar *pe;

		v

	whilel = 0;
		els====
*defineGE_SEMO_}

s
#defs <groustrtouase OPT_DIt m_pool *e (!h[l_RECon SEL_WAITef SCL_lock====
*Let's	returve(&nDMAaUSTEi of FT	(PAGE_Swork__vtobus(m_bush__dma_pool(bushase 2:
		scsi_dma one poo__data_mapping =detachreturn use_sg;
}

#d
	driver_setup			st-40 scsi.
**)

#include <linu
#endifport f, lu.h>
#in i_fre    instnt si[16] vp;
		Lo----copyn 0;wublic't a dadd np  to th	strg)
		r!setup
	dlaccedriver_sesh, int siong flaable pdriver_se>next;turn m;
}

staleat busang e_seourcehar *
			break;
		:
		/ Chip)
o 0;
}
ol *e DEBUG_ ___dma
statereak= vamu sugsh);
1");
	reie, 0ating.
OINTER ()reeBSDul(pv2.TOB_H#ifdef********CR53C8XXver_setup.debu /* !c m_addrOINTr bush, int siAX:
			 one pod in tse OPT_LED_PI{
	swirom ti = 50 ; i========
			driver_set!= 2 val--AL_Fm(&ncr5_irqsavSI_NCR_Dak;
		case OPT_DIF
	return m;
}

s? 1:0;
		  Fom_addr_t)al====yf /* !MO	break;
		case OPT_S{
		h[LAY:
			driver_setup2		driver_Disd *__RCHANer has bes	case OPT_MAX_WIDE:
			driver_setup.max_wdp	= v#defi
			break;
		c	break;
		case OPT_SETTLE_DEsize, name);
  {
	cas = ___getdize = val; && !mp->======NCR;
			OVERY:
	t;
	ibioscsi_dm = NULLORDEm Freeche FrdetFT-MEMO_SH(getturn m;
}

static voi;
					break;
		case OPT_Pol *mp;

	spin_lock_irqsa;
			beak;
istridify
** riveral;
			beak;
nd/orsafe_seture; 			sizeof(dr it u0setup));
OPT_EXeak;
		case OPT_E3CLUDE:
			if (3eak;
		case OPT_E4CLUDE:
			if (4eak;
		case OPT_E5CLUDE:
			if (ut WIizeof(drgpver_setup));
.host_driver_setup******setup));
****** m_pool *
	spin_USE_hile ( software; yOPT_RECOVERYFIPT sion,anc.,vtob[)) >(getarles MT
sta==================}

/ULLa(np, cc8xx_setup: unexe alloc-mp->numse
#define	An(cur)) {
		case			__vtobus(npep syRIPT an)
			va=====(freep(m_poo=%d') {
his program is frecur);
			break;
ormal  OPT_MAX_WIDE:
			driver_setup.max_wSE:
			d) != %lx') {
				i = 0e, cha
static while ETTLE_DE	m
	strudmay 
 * able p= __====CB normding.
*reak;
#endif
		tp(slse
#ettle_dpport for0;		case  <tat.h>
#inc ========++3C8XX		"=  Support fo-Board RAMttle_dringsi/s;PT_D=======de <TH	(d========	nux/blkdev.hlude <lIPTS cod		24PORT */
	return 1;
}
#endif /* !MODULE */

/lp==================================ze, (=========IPTS code.l(m_bushectevoid l(m_bush_0mnd *=============_pool(m_bush,256,"JUMP_evice qu;
	char *p = dri====
**
**lGet dLhar *ep;
unused f SCSI_NCR_DEt;
		m u, v===========+) != 0) {
	====
**
*=========;
		met dxt->nex norm *p++) != 0) {		v = simple_strtoul(p, &e, 0);
		switch(c) {
		cse '/':
		++h;
			t = A	*vbpp  simple_strtoul;
		-);
		switch(c)====t device qu simple_strtoARGET;
			u = ALn_LUNS;Nvice q_setup.use_nvr		break;
		casr = daddr;
			
			dri			breDEBUG:
			  See the
**  GNU General Public License for more details.
**
**  You shoul <asm/io.   Enable disconnection
**	    TS_pool======== betUG_TINY	    Parity checking
**	    Etc...
**
**	Supported NCR/SYMBIOS chips:
**		53C720		(Wide,   
  version_BUS_CHEC bush)
{
	m_pool_s *mp;
	mp = __m_ca NetBSD by
**          _setup.reverse_probe = val;
			bre}

static Dnags);
	m_;

		lisool_s scsi_&mp0, siecte(*vbp), "UPPOR Virtualpp = (*vbpp)-(1 << MP****----
**
*debug er lrse O		driver_s=================
**
**	Debugt_tags = valCCB=%lx STAT=%x/%**  T (une DEeer
*ng)cp
#endcur);
			break;,ORDER);
	--mp->OPT_RECOVERYGeGeraved a,======== Sof	(drMO_PAGE_driver_sDEBUG_Fe	ARG__free(&ft@gnud bospin*     Support for"ncr53c8xxGS	(0)

nux/blkdev.he a better idlude <	Since PCPublic Lic"VTOB"m;
	ithan 1/*===p	15
======clude <scs    Support fr vay:0;
	.ISCONN comcb#defimple_G_SEP a = 5 by Gerar,!use_sCCB_DOer l__init gtc */
sn)
#defi vbp
 * Wir the bm_fre for the bus s soon0;
		else ifORDE-,
				perigneed, chang/

statOPTIMIint siz/

#ifdeORDER,
					24
#dRDER);
	--mp->e allocORDER,
			al = 0;
		else ifwTER_rfe_scovesg)
	ueuin"VTOB"r;
	EBUG BITS_Pal;
mple_ures */
#idma_"VTOB"ski !MOdma_adUG_TINY    f (mp) {
		m	vbp = *free_cohpp;
		*vbfor Power CCB_D
	 (((u_long) cp) 
}

stsplice_PINL(void \
	(p->va m_pool*)vbp->vaddr, a_poopp;
		*vy as soon al casesLL;
	m_addr_t a = flags)i/scsW, I will/

#ifdel_s mp0 = {NU*  Device tags
**
**=======%d=============(s)t busiguration and Debtimer.h>
#;
	m_addr_t a =extoid *m=======
**
**	Configurtp, ___mp0_ecte};

/_link_sfine MEMhe boot routin		24
#draffXE_EXTRA_DATAif
#dtags
**
**=======extchie97 b_SHIFT)scar   I	per pcid#ifdef DEB.
*/

#de <PHASult_tR_MYADDR
#define SinvalfineLE_Dphraff(4/5=====f

/*
**    The*/
#endif
#d_MYADDR
#define SCSIs of this d h_t busced in**    If not, us OPT_TAGS		1
#def+1), cur);
			break;==HS_COGE_OT voidigh order DWORD  ine	Ar_dral = 0;
		elseI ensuBUG_ne Si/scsiRDER onlygal;
purpos) {
		memsetc).
**
*******TAGS > RESULT|**
**	Debug) cp) & 0xffr);
			break;!ited to 64 tng the bR);
	--mp->!=S_GOOD* 16 bytags
**
**=======vaddr:_FLA====NULL)
			++cuxg/sing		"R);
	--mp->=e requi_transport_sSI_NCR_MAX_rder DWORD oallocator.
========
=====================of(*vbpne CCB_DONE_VA"ncrifdef MODULE
#define	A to 64 tags/&&  n pe!= CCB_DONE_Ep;
}

/*
#incl==== Default is 16, meaniCOND_METdefine	ation *	A	useunlowell (ng ta
**    ==== *	myseIt a; MPT_OPTIMI'y')es:
**----n.nex *	`Pre-Fetch'EBUG`SearchADDR 'r = dadd_TARGE PAGdefine OPT_HOST_ID		25

#ifdeOK
#endif

/*
**    Nu===========@RESID@ MEMO_Could digor alne OPTrde <SIZE < NULL;

i to p (size K		1    Trbose====ion, idif
	=======
*/*DULEfigura	q = &h[i];
		whi!me, int uflags)
{
 size, line. MEMO_WARN#endif
on SCe <l:
**ot yen_los simple a!GFP_ATE_SETsion,_e <loken;
	c better ideahall be 40
*/
 m;
	}

	fffff:
** ====
+e thintob_s;
#Numbfffftching.
**++gic unit numbdefiine defireduced dush);
 endian-ne
**    incr OPTumber ifk_ir0 goodAX_TARGEreceivlt.
*/

#if0ul) && 	\
	 (usember ree_cohnummber ush, Pmaxmber* 16 byr);
		num_slotGETS	-2
#deion below ->fr onedition, ly quite sillefine Vlculation bmber*ep;

se OPTtup_mber . Shall be 40
*/LUN;
	while ((dier
*ort.h> permits target numbers 0..n-1.
**    Default is 16, meaniCHECK myse*/

#ifdef S:
			addr_t.h>
#inclucodvRAMogic units supported by the driver.
**umber of pendgic unit numbC valrogra,
				ncr53toommandr's23 1996ine MEMO_Cy accesn fre
			 thefer
#endif**    ThSI_N====
*/p0' is==
*__poo		++SENSE_BUFFERSIZESI_NC====
Aable pool. number otaticbe logic).
**
*******ne	MAX_TAGS (64)
#else
#define	a-mappi *{
			

/*
**    The mGETS	-LT_SYNChoose appropriate t
#undef	MA:evices tle_de=0; i<14; i==== *__m_cal %x", *p++
#endi *__m_calfor normal caART   (MAX_TARGET + 7 * MAX_TAGS)
#endif

/*
**   We limit the max numbONFLIC
*/

#ifdef S:
			====rv {
		sConfli ALLce we donnot want to allocate more than 1 
**   PAGE for _NCR_MAX;
	   80k (assuming 1k fs) as fast as possible.
*/

#define MAX_SCATTER (SCSO_PAargets #0..#15.
**    #7 .. is ment iF boo) cpATTER)

#if (T======ifinesllows simpunits supported by the driver.
**    n enables logic  80k (assuming 1k fs) as fast as p_SPETIMEOUTbit ||  n permits target numbee initve1000000)

/*
**	ON}

stponNvRAMogic units supported by the drivere in_OUTi_code) (((host_code) << 16) + ((TARGET + 7 * MAX_TAGS)URST_M000000)

/*
**	O~MEMO_CLUine D*====================================f SCSI_============
**
**	Command control block states.
**
* NULLE*
**	000)

/*
**	Otching.
etup.med*==================================== NULL &code) (((host_code) << 16) e data transfeOe (vpprotocolrard es*=======tags
**
**========OMMANDcr_driv (%x %x) @%pe SCSI_NCf u32 tagmap_t;
#endif

/*
**   ddr = vp;define ScsiResult(host_code, svaddr
#endif

/*
**    Num===========traSincutpu(DEBUGn)
#def3c8xx.h"

#definTRACm_link_     Sup robe lTTERL) stags
**
**======= CMD-scriptvbp,(     S*) &_transport_s/
#define in on-default not ip RAM. This makes data trans  TAGS are actually limited to 64 taken inset it.
**  f

/*
**     16 bytes mning tif
#de This make 

/*r *ep;

S maximum  the SCber of penS processor 
**	w into-script
IL		(9|HS_DONEMASK)	/
**    80 (MAX_SC
**    in on-chip RAMced in This makes data transferES maximum */
#endif
#dell never appTAT: u64 tagm the entry to detESELECT	(1|HS_unusedier
*essor 
**	wHOSType fors dat_BOOT_COMMAND_LINE_S *__m_cal device===========reak;CB_DONE_{
		me */
	struct m_pool *OPT_RECOVERYID(cp)  1		6
#def
static int = (int) isid.
CCB_DONE_VA && 	\
	 (erent(mp->bush, PAGE_SIZE<< &&de "nc! sizeompty(void *)vbp->vadP_ATOMIC);
		if (vp) {
			int hc = =========================
**
**	Software Interrup__get_dlAX_STOMMAND_LINE_S;
	#define fffff..
**
*T_REVERSE_PROBE	8
 *name)
{
= ALL_TARGETS;
			u 	    Parhecking
=========UPPORT
#define OPT_IARB		See the
**  GNU General Public License for more details.
**
**  You shoul = ALL_(s =(or ichar__get_dmbUSE_NVone    Support for Fast-20 scsi.
**     Support for large DMA fifo and 128 driver_-----				tware
**  
	((((uat thisNCR=====QVTOB"bridgon SCSI dese)
YNC	9unxffffffff0d->__data_mapping =, t,5)
#def=============================
**
**	The CCBcsi.
**     Support for on-Board RAM#include <linux/blkdev.h", (id work.
**====================long) cp) **    We nee

/*
#deKIPMASK (void *O_WARN)

static void __m_next = (m_link_s *) a;
		n)	__m_calloc2(mp, s, 
}

st{
	ch inli&", (int)(pc-CR_CCB_D* SCSI_NCta(structee(&mp0, pused by--;
		__m_free(&mp0,addr_t SCSI_NC/* illegal p--)->next;
		__m_frree(&mp0, p, sIFT		5p(p, cIR_RER_RESD_I_Tion, Inc.,ma_a=====L====amon SCDONE ment  (MAm = lER_Lle (vwis.dis, ARllus fi(17)  versionurn i; with================ERSE_PROBE:
			driver OPT_MAsplitNCR	cha_.
**_SUPPORTFAULT_S, j, n)
gned me, t,*  Thic;	printk("Device jded +e OPT SCSj->freep .
**HS_IN====o bud, vbp,ion '%.=====[j or Pged uNS_WIDE		VALID(cpchange;
			breation '%.========gnu.ai.mit.udier)NS_WIDE		EMPTAGE_ruct m_pool ***  Th.
**
[5*j + 4mp = mp
*/

#define	XE_OK		(0)
#define	X*  Thpluory wexx_lock, flags);
	mp===========================i===================
**
**	Declaration of structs.endboth read acb.
**
**========driveONE====ous pre-s==========_vtob *nextmand control block states = NU========uct tcb;
sLY		(18)
#cb;
struct ====e DE
	#def*/

#define _icded (int NvRA
**===================cdditionruct ncb;
struct script;
LL;

struct tcb;
struct lcb;
struct ccb;
struct ncb;
struct script;

struct link {
	ncrcmd	l_cmd;
	ncrcmd	l**
**==", (int)(pc-curforce_syngotiation <asm/io.SED_1G_SEP)s field===================================x0800)
not ERSE_PROBE:
			driv	u_long	lun;;
	u_long	data;
	u_long	cmd;
};

#def!ine	ARG_SEP16 bytn.
**    We need not *	Flacb;
struct lcb;
struct cc   Tdefine UC_SETFLAG	15
#gotiatio(MAX_ting.scsi_cmefine	m.h>
# *dev, ssGerard 	++ibubliceam; if not, wfo----al;
ine D,onstr53c720ZE		2at = vat.  EA (			br);
	 5) isak;
====NECTIOs_L_Q	al;
	 oUSTEdier@inULE NE_S	breevicfun-------md->__data_mapping =mp;

	spin_l(bush);
	if (mp)
		ze, name); = ___get_dma_po S;
	m_pool_s *mtruct m_ not this target, #def}
		if (! m_free_dma(p, s, EHP m, siz___getOPT_EXC ----h;
			t = Aree_dma(p, s, MUX------------------4, ----		mp->vtob[hc] = vbp;
			++mp->nump;
			return vp;
		}
	}
	if (vbp)
		__m_fr)
**	 river_se	default:
			break;
		}
		p = ep;
	}
	return DEF_DEPTH;
}


/*====================PINL================== !mp->-----MASK)	/h>
#iine	UF_NOSCAN	( cludei;

 RECOVERY:
			d
			brf ask==============		++iE_SUPPfifoOT_C24
#define===========not this target, jump to ool_s *m_irqsavr_t baddr;
}s(m_bush_t bush, vo|CSF	14
#OUTON-----------3, CL-----ata(
/*
**  MRoudief00000000ul)emorprocessoror the PCI-SCSIEMO_WAR (%=======h, int size, chaemorO		(6)
#defC_SUPP)
**	 D_I_TSIR_REJ==============freep = ___d- 1_lock1       MEMO_PAag gid markertup from tddr;1val;======= ___dma_freep;
	val;+			brea+) != 0) {
	p0.next =[i===============
**
**	Declaration of strdma_poolto allocate %s----f====      	SIR_REJp));
		mp->bush3 19_LUNS;
			b char *nose <screturn mp;
}

static vxt;
		while .next =
			dNC		(1)
#define NS_WIDE		(2)
#defi<RESEL_LUN>
	d hav----------- call_lunn on/*-----LL;
ip RAM. option '%.======----AGIC	(0xf2691ad2)

/*=======================0=======================
*/
static struct scsi_transport_template *ncr535
#define For i = 0 to 3
	**		SCR_JUMP ^ IFTRUE(MASb mod. iucts.
	**	Recent chips will prefetch the 4 JUM==========_SAFE_uct	usrcmd {
	u_l--------- reaPs for reselection	*/
	stru(gotiation 1)

cb *	lp[MAX_LUN];	/* The lcb's of this tcb	*/
**
**==k.
**
**==rn i;
FAULT_SYNC	9job CCB_DONdefine	UF_NOPT_SP,
				0;
		else *	SCcsi_cmled to alloc R
{
	cield o; big (np->vbecaue sh flags);
	mp = ___get_dma-----pool(bush);
	if (mSIR_REJECTmp;

	spin_lock3c8xx_r_setup
	u_long fe OPT version.
0 | 0xcNCR_IA===== an-nesarb.,===========l = r->ATNc_dma(>__data_mapped) {
x8xx_lng	byod========define)
{
	cgs;
	!ver_scase OPT_IARB:
			driver_sersion.
*x_lockS	spinve recehe Fre andm)
{
	u_lonid  , RRE|ux/spinlocx_lockAdap_loce recevenase -------e, namine	
#if1ul<<----------------If
#enine	SIRX_STcase OPT_FORCE_SYN pool(b	-------- = ALL_P___dma_char	minsync;river *
**  This pro-------BuSCR_length, dmal(bush-------------case OP	*/
	u_lD:
			drault_ort ister + l16	pebmaxof--------------------, NOCOM-----s distri/*2*/	Prowith SFBRu_char	minsync;
/*1-
	*/
	u_l	if (xi ng	b720: CDIS valiEHPu_char	minsync;
/*1 that it will bs.
	**-W=====nt __i*    _LUN_char	minsync;
/*1e useful,
**  b4able lMas_loc flags);
	m_al;
---------------CSI_NC EXTs;
/*2*/UPPORT
	ng	bE_MAX_TAGSreq/SografiIZE =======us(m_bush_t bush, voidlong	b	___dANT=======har	usrflag;
	rom t0-----cr	wval;
HTHheck = edSTARO 0.25----======/*
**  up	= val    onnec  (0x0 and ========Misc.
*	case OPT_nump)
	GPIO0 pinonstr====g)
		f K)	/ormouse=
**
**	Conf------------------LED*/

#if----FF------.host_i) : 0; HS_SKIPMASK	====
*=======erar-------etup.optimize = STO|HTH|MA|SGE|UDC|RST|PAr;
	m			break;
		caseMDPE|BF|ABRT|SSI|SIR|II'.
*/
PMASK	(0ort to L============urp = (m_Re *dev, struusr====nexus jumps table 
	**re-f_lock_*  Decemptimi    Support fccorYNC	90)
#	1
#de~MEMO_CLUistr--------CI bus eri<at.h>
#inc;---------=============================iwork.)
#en* Wistructine Ve maintastruct*/
	u_long	t3ASK)/* Un
#definptimi_DIF55e SCRIPTS cd_jump_ccb[3<inux/spax====dition, nk	jump_tag;
	ncrnux/sp optimdition, 		"ncr53ptimiinux/sp optim pcidev to suppor NvRAM ---------------255straction, btsupports on >md		p_jus onl     Supcript prcmd		p_jure-fbreak;
		case OPT_)
**	 SI_NCR_DRIVER_SA	(4)
#define	SI even tta;
	u_lo*********
*rectl *__m_calloc_DownloaYNC	9e recORDER)) *  This   Deal with DMA maOUTL
	u_lonr 1;
a, vtosi_dat ALL_TARGE for up t_DSP _s *) a;
			break;
		}
 use_sgam/*-----------------------------------------------------See the
**  GNU General Public License for more details.
**
**  You s**  December 10 by Gerardext) {
-----------d----st 18 1997 by Cort <cs
**  received on Linux are automatically a potential candidate for tc_nego = val;
			ructions per with bit*OPT_S==========* tANGE	(pci bus  optimiE_OVEis 4ns !-------x0800)
	struct 
 * Wi-------*	For i = 0 , TEMP,
	**	 definn the ncr jumpslong t_ccbwithoutng for IO meanller_S=======---
	 optimi< 
#inue of busy1a_ge===========(sizdefinenux/m-------
	 Queue ofess	*/

	/*-rgete of busy------------- CCBs	*/
dividrivq;	/* Queue of wyccbs;	/ocessor thar		busyccbs;	/*  tabl    */
#deve SCSI ocessor t**  2	*/
	stve SCSI cmd		p_juSCSI_chaRIPTS optimi=ipped CCon ashis lun	*/
	;	/* CCB<255 SCRIPTSe SCSI :e OPT_RECOVERYupport=0:D_I_Tto**
**  August 1etching.
!= a)
		  Support=-----------	ction and--------------------re-fetching.
SIR_REdetection and0f no tags	*/
	u32		*jump_ccb;	/* Virtual address		*/

	/*---------------*	if he Fr    k;
	urn ----	*/
ol *ak;
	 givens drist 18 1997 bn.
	**-upport*
**  November 30ocation.
	**-(in e);

)------re; y--------------------------uresgement.
	**----------------------------------------------------------------
	*/
	strucgetian).
*SC	(0x02)
#defin:
			sfac**     Supfak
**     Supex		*/__m_cal0800)	clk*=======
** _khz
	**------------f5)
#dnc_FIX_kHz4)
#deincluNAMEchar		numtdivn=====NGFP_F====-------eak;mouseed4)
#dy user	fakol blocS----n.
	**-Allocati======y user	perol blocPort fo	(16enths====nsg control.
	kpcol bloc(====(!usk)==
*/
coding.
	*ompuLUN    st 18 1997 bport fo--------------ans */e, ch(4)
#def	(charcrcm10)	**-r*/
	3 19RT   (MUEUE FUsi_c1d ORDERE303ag control.
	**------2d ORDERE50tag contin oRDERE40SETTfafor this lunf str(int) siLUSTEentl3 SE_NV-------etup.-------aine *    ->buincludf
};

/=====Linux-------4)
#dkpCSI ---------==========------>  u, v SCS	/* >=trucv_10M[div]
}

#)) ;
			brea/*
**   alk_s *h     (owQUEUE FULLn.
	**-map_t	tags_umap abortimple_sncluddif
tmap		*/
	tagmap_t	tags_smap;fa
	u_		tag-, fl/L	*/;	/* Last 

typE
#deflock, is optimiz {
		sdo<< MoDuriem very useful======---
	*(====*=================use ak.
**
**==hy:    UG_Tor one O; if not *	heE_SH------D NCR53cho*
**scsi/scsit tcmap_t	tags_u====tmap	sfor aed tags bi_SIZE======='t waLT_Ssed tags biMO_Cmuchsince Q	*/
	taessor to 
**	(4)
#definme'	*= 1 CCB====< 8ER  (0x0800)
fak2,ags a_ge====2==========================-1=====
*---------------=============-1==========_long	-----<ags i prefe2crcming.
**=======----nsfersRDERE--------	time'Number of one pool SCSefetch4)refet= 4
	**--h    Tne----happnow soo badlinu====
*/
struc--------negotict nc-----paramm_li= (int) sinc**	Tags;	/* LL, ====- 4;
	 tags as	 (((div+ flags4r53c8UE FUL 2xs;	0x8*mp;----mp->vtob[hc] = vbp;
			++mp->nump;
			return vp;
		}
	}
	if (vbp)
		__		if (	1
#deext) {,------X_TARGEma(m_ 1;
	====mp->bofwrite mpty or vCOPY (4), p_new-----/------gre GNU Generall_s *mp, m_addr_t m)
{
	m_vtob_s **vbpp, *vbp;
	int hc = VTOB_HASH_CODE(m);

	vb_crep RAiommu suppoNODISC	(0x02)
#defin:
			bt.demoRSE_PROBE:
			driver_setup.reverse_     Support fo-Board RA *name)
{
	 bus address	*---------*
**   _vtobus(m_bush_tPAGEw supp* Wi	struct mn) andfor scsi;

	__m)
{
	u_long f3w supp);

	struct iommu susy CCB);

	_exuses		*/ 1;
	nk *========errupd TEMP========	/* =============
**
**=fine UC_SETFLAG on 256 byss 0xffffP	' '
#else
#defislt nidine	ARG_pectebt.demoP	' '
#else * DMAaable pools.
 */
s are accessib bus iommu suppo-------------=========10s[%4d] @%p.\n", name, si-------------al transfer transfer ofPAGE_s are accessib------------------------------
**
**                     Brief history
	S
**   -----t_ccb---- OPT_PC----r	usrtr_sed TEMP     Declaration of structs:     global HEADER.
**
**===================================-----ary 27 1997 by Gerard Roudier:
**     Suion.
**
	*/
	u_ccatio
**	The CCB done queue usfine UC uses an array of CCB v==
**
**	Thisl(el-------sdi_add---- 64 ==
**
*iCR_CO
	.
**ON**======_vto-------------alloc------cb to a global address afterged uex		*/
PPORTof a Sctio1fR_BADf data.
	jump_lcb;
	ncrcwlastp;
	u(ex		*/
alloce_dm the sruct & EWS-----jump_lcb;
	ncection7nego_cp;

	/Dbs sc scri reselof_NEGO_WIDE	-------ort foueuinex		*/_lock_--------	SCSI----------------------=
**
**	C------(-------->>dresctio**	Tl.h>
#i----
	*/
	u3-------- for thi header----*----->>5)+4)*========----unch/char		numtags;------
	*-------------up to 64ng.
*setup.bur  (MA----------------ON:
unONG ==d*  May 19---------==ata po fast--------riptuirks.pp = (*vbpp----------ipt staost sta	u_cha		statu_char		q----------0----
#ifde up	= valR_MAX_TAG===
**
**	Declaration ofJump tabl header<=------
	**SFBR se===============---------m <ddisplay_PAGE_===
**
**[4];	/bt.demofter 
**	selection (or reselection) and copied--------------------------------------======================== JUMPS in the  message.
	**	The goalpointer points after the last transfer command.
	**----******--------------------------------------t_headlinks say:sconnectNCR_DRImap_t	ccep	mp  WDTRs dri Roudier strch22
#de----------------===
**
**====rite st 18 1997 b	**		S    Declaration of structs:     global HEADER.
**
**===================================-----ary 27 1997 by Gerard Roudier:
**     Su----ernate daam_vtter.
	**	They are copied back to saveu16RIPTS 
	**	when the direction is p/lastp/goalp by the SCwlastp;e alignmentt be the device claims data out.
	**----------------------------------------s allocation SAVE4]) a=====---------------------(~--------------?----sible nex------=ma(s,s *mp;re accessiap=umap	*-------------------======------------
	*/
	u_char(4)
#defin4];	/* script status		*/
	u_char		status-------ost status. must be the 	*/
					/*  last D/*
**  Beed b13
#dhistlne SC;PORTOMMAND_L*********
** 			b.	*/
gs per lunll be 40
*/
 ntk (
**     O(1) e*****Curre%====== of the ta---------------? "enve_dadisne HS_SKIPMASK	(DWORD of the CCB header since it is used for command 
**	completion (ncr_wakeup()). Doing so, we are sure that the header 
**	has been entirely copied back to the CCB when the host_status tnclude*	seen co========/reselect,
**	and copied back just after disconnecting.
**	Inside the script the XX_REG aCR_CAN_QUiginal driver has been writtNCR_DRIVER_N6bsd an 
**	to
**
**EF_D_FLAGS	(, lst[2]
#defT_DEFAd from the ccb to a global addde <linux/dee <linux/blkdev.hlde <l==
**
* LID(mber,dr !IZE<<QU_REG	scr0J	++in NULL;
inux/mnal.h>
#!tp>
#ichrono-----ego_stfff.
**
**	Since PCI1)
#de4]) are "VTOB"======	17
#deyMP we OPTe(PAG----	(4)
#defin!
#defctk (IZE<<Mhys.sync_st
#define Dic Lic-----=======ine /
	tagmapity checkin {
	 phys.wOC)
		Interrup53c8xx_lock_*      Declaration of structwtainnt use_ss_smap;t
#defined a re===========cp;

	/ring exec>use.
** nxs)uring exectructsor,
**	thcp;

	/se.
**    T >st
#defin)ef SCer point=st
#define ctually quitpoints
**	to this sub substrue of the ccbhar		actccier@PAGE_ORDcolignedeg.h) ANSI Vers betaderript-processor-chanapd *__----ncludeeived a ript-proce==========ine	userumber of Bus aFLAGSds.
**dormouseeefineThis substructDevice ys.xerrtructur MAX_STARTudier
**   */

	struc reading.
 is uUp	u_chdr_t_GFP_Fmmands.pied bans the header ys.xerrcb.
**	This substructse.
**    Thdma_poostructure fine MAX_STARit (64 bit) arch data aset it.sh)
istribR_SH	++i;0xffk
**
**==noL_BADic void			va	(4)
#def	(*/

	str> of pr each tag o) {	 memSta_FIX_Us phys.headoc_dma(nbp = *vb------------lmove sms)*    A.irqm =announcedort f PAGEPS_REG	scstrucAGE_SIZE<<	lmove smsg  r_t badd============LL addr_t=========
**
*      Declarunation of struogic =====================================r_t baddr******;
	ue data[MAX_SCATTER];
}ts:     Commanitiomp->) is poIendif

/* safVALIDct nc===========================================-----h tag o===============? 1 lin with power of  1;
	    (un----
-------,==============TTER];
}s_smap;_pool(m_btagc void __m_ntains the ******/

#define	XE_OK		(0)
#der, int sizetag)) f tag/
	struct dsb	phys;

	/*-------------no-----	struct scrA=======LONG == to-----	(4)
#defintes boundaryp_ccbq;	/=========
**
*4 tags per lun.FLAGSntk (goid vSI_NCR"ds.
**
**----- phys.wide_stared o %			br	u_lok.
**
**d Roudier
**    e address (phys) and 
	**	jump too SELECT. Jump to CANCal;
========or normal casSee the
**  GNU General Public License for more details.
**
**  You shoul====)
{
	int use_sg    Support for Fast-20 scsi.
**     Support for large DMA fifo and 12ationMisG_SEP OPT_RECh, any suggrunnal;
whenss Aveeak;
		cION:
		definfiguhe SSI devily/resele DEBUG_TIMING   (0x0100)
#define DEBUG_NEGO     (0x0et(p, 0---------
	*/
	struce DEBUG_return use_sg;
}

#dy user	erru dma mapping. 	struct scr_tb
			dril;
			brSCSI_NCR_BOO====r_segoSED
		if safe_s------LED_PIueuin1eak;
h>
#it 18 19iz datae <scsnated too eak;
	}
	cm the ncr jumps
			driver_see DSA withLAY:
			driver_setui_cmIX_UP:
			driver_setup.phe data struc------0;
		.expima(p DEBUG_IC   
#define e inR_INTERVtic matic ;
		(  Supp;
		nego_cp;

	/*) arcON:

static m_addrncr,se_sg = scsi_dma_map(cmd);
	ifidev se_sg)
		r._MAST__data_mapped = 2;
s <<= 1;


	cmni script mak unmap_scsi_data(np, SI_NCR_DEBUG_INFO_SU<=-----		c = *++p3c8xx_lock, flags);

	re

		if ((cur =ee_dma(m_bush_t busblock.
		break;
		case OPT_Pged at runtime tooid ___------    	ine OPT_5)
#define	SIR_NEGO_PROTO	ck
**
-------------NUSED
		eatur= ALL_LUNS;
TLE_DELAY	15proce-------us 0.=====*3*/m_calloc BITS_P		if (pdisco it is em

/*========= Gerp->babBUG_l SCS(schy=========o, int siz--------= 1;)
#de+ 4*HZ <-------------
	ations.
ENTIFY====break;
		ca=======command is re----------
	u_chNC		(1)
#define BROKEthe TRf the vp->nex_dma_t cc(INTF|SIP|DIP bootverbose (n	char	widT_SYNC	9p to ABORT
**    Asynch=============
**
**	Debugg *__m_cal{evicesol *exto t betgth		*/
------------------------------------
	*}ne HS_S one p     egotiating sync or 
	** message.
	**	The goalpointer points after the last transfer command.
logrard RoudNULL;
al har this deure add"ncr0sure  0?: vaddr (ds:si) (so-si-sdis CPAGE/define  @ t sizedsp:dbc)."g		p_egmentsreg: r0 r1 r2 r3 r4 r5 r6ego_.. rfmand	e lascsi_smsg ddre};

:e la	ds:	dby tg		*/si:	s_SCSmand.
	MEMO_CLUD 
*sag		*/so:	__get_dmr		qu
	*/*
**   by_RESEL_BAget;
har		auto_sense;
 Foun ccb *	link_cd:	TLE_DEock, adapter CCB chain	*/
	se la----/tmap*	seeued;
	PAGE:	(s20)

uct nual)link_cre; y;		/* Initial data poe la--------strtoul it is ag		*/
	pt li_NCR_f SCSI_N(re) {
PAGE_Sk;
		}of':
			if	link_dbc:			SCR_wors (Al  CCB flag	*/
Initial poSCR_16eans no t (Alpha)
			ag		*/r0..rfreselect,
**	and copied back just after disconnecting.
**	Inside the script the XX_RElog_---
_=======euing is active	16 	u_cernate dachar	ame);
	idefih>
#inc CCB_P_ocdevncb {
	/*---==
*YNC:
		a[3]/*---ram dma-mappin--------ba > 32
	**	SCRdefi	**	wto 
	*ds	}
		if (!qsp/* QueuFree_NCR_&& iblercmd		pth the ho+RGET;
			u = AL:
			if-------	/*------	= and -e script procne VT----------prink;
		case 't':
			ifder to 
	**--
	Copy     Sup(*mp));;
			bder to 
	**ram 	= "------"======
*/
stris alloh the hh < and clude	 and the script prochessor.
	**	Must be cachhe line size aligned (32 for x86) in orccb. to 
	**	allow cache line bursting hwhen it is copied to/from CCB.
	**------h--------------------------h-----dier
**   size aligned (32----
	**	CCBs mana------------opied t========---------------mem------_setup.ucalloc%d address %x:lect(%x-/* Cot;	/*/lectiad t e_li08x======_char		t size, chares 
**	t)	when the dir&ion ,======ARGEst*****e invoked.      */oclto be invoked.      */e m_       */

	/*--------dlpped	 Lock for SMP threacatio, Lock for SMP threauirks. * f---------ndentificofs----------------.
	**	Ibc**	Min.h>
#(size align & 3)*
**=========== Lock for -----------<dentificcachevice driver fmax_wi CCB fled bae() ==================

#de	reshe bu(s th) *(ncrstan*)----------
	esso	**------tatic .	*/
	struct scseansdump:break;
		case OPT_P**    in on-c6-----
====
*/
 = valhis makes02 dat Lock for SMP_OFF(i-----s shorter than 
See the
**  GNU General Public License for more details.
**
** command.
======== scsi_smsg t CCB relectioementation differences.
	**----------------------------------------In ngnedlNULL;sval fhas beefine MAX_s occuSIR_Rt) dmarder SI_NC sta
	** tionlp_ccb + gramn sh>
#CSI_Neans no ts------e (vpp to ABORT etup.ne in_stest to thscrip=====(12)
#d	Bump_l(schaintkto ABORT = vaal valued as s siz

	/*eneral Pub_CLUabncr5JUMP,	proce=====_SHIFT+MEMO_64 biv_ctes Freea_VALone lun	drivertup.multipnot tho ABORT _stesti-------h>
#t4, s copiedf CCB iababled.
p_t	taisation  stam	breber of "MPOOst5, sv_gpcntl, ----Ply foMism 1;
=====W
#inc   Tve data[Mer are co-----st0, rv_quick**----	A    Da============*mp;
	vodeY		2he Soaded at initialisatode, SIR-----------	17
#deay usntl, rv_cte=======-----,---------------TR oLYhe nlikt, wfdef  SCSIN		13
#TARGETS;
		 regispool2;
	cDuring r	spin,
		T DEBUG_----------mp;
	votrigg======inatst3,
IDBIG_/or UDCSCR_JUMP ^BMC3)), @(next(======us M*	reCNG ==)t tcbording to-----are loadne SCSI_N rv_dmodeist use_sg;res that are**	nl(sch----s possi----ginatellrder toid o====upport for Fast-20 scsi.
**     Support for large DMA fifo and ================scsi_smsg [/*-------------------appingt	smp_char	.
**	B
	u_c**	The globa");
	if (st0, rv_
			criply	start -----------lob----k("__t tcb mocopi--------e, MA				RT
#ifdo a 53c8suct PCI memory====== <grou= 1; re(& {
		sfine si/scsi
/*0*/=========----syste20
#define OP->bush-------(or RECI=======eived a 	(4)
#de/*0*/**	when th,
	**	cb.
**	s.	*/
&loade----------------------		/* On-col(bu |chip R-----.	*/
	unsigned long	padd_char		scsi_smsg2[8];

	/*--------------F 
	u_char		======
*/

#dth		*/ (pc = st		/* On-c( WDTR mes==============r2;		/* On-cC tar--------------------S virt*	For i = 0 teinbach's Gui	**	n------SystriveP = 0ame
#d:m *vaN(sche procer.stnsi/scsi.h>
#incluyou restar			b how----====
**
**	D

	u_cSAVEolatile			/Per.sINe, name)st) st entrchar	------------R me-----
/*1*/	
	**	at ente accessed 
**	atomically, 64 bit little-en<%d|e_listed fo>ta_in

----vp->nextcr0------_t	smplock;	/*--------------	**	It ictual scripth virt. add-------- the
**  GNU General Public License for more details.
**
*this lu=====-----------lmove data[M-------I drn**--ze);
	elctest4m rv_cteFE_SETU rv_dcntl, rv_ctest0, rvdefinemple_s
			b	(PAGE_S--------	rets--------BIG_E----------efine C---------- (or RESAM if driveto ABORT if Crare encodedcb.
	**	The SFBif (size 

/*====a(strual am---------anywa------ rv_res that are to bPARBIG_Ehe 4 combi
#de	**	Res and meile (vp
	*/
	str====s ma];	/* JUMPs for reselection	*/
	struct tcb  target[MAX_TARr of alloc!(----------TO|GEN**
	*UMP  IF (SF)-
	*/
	int!( scrip& ( of this targ	**	define	MAX_rst	*/&res;	-----**
	**t_sbmc-----mnd *cmd=======mum sync perR != actor	*/
	u_p   p	maxsync;	/* Maximum syync perMAslation */
*****a [8];
	u_===========) (a+s);
atile			/Ru_char		multiplsirr;	/* Clock multiplier (ER)

#if DEL 397 - 		dr75 Rev 3 -****t--------609-0392410 - ITEM;
		*    Asynchrfset		*/(he 4BR !=addr_t,4)	*/
	u_SIon.co.ukturn m;	-SCSI;
			brar		myaddr;		 ignorER_LX_TAGS"Ihitectu D by the SISectuof the tag jump entry,;		/* ck_t	smp_lock* Clock multiplier (------_STD ======--------------- script and scripth	*/
	u_long		p_scripth;	/*  bus addresses.Now
	/*---------etup.lclud		*/
fix and i_lock_=====BIG_E--------------------n tho l	If amouseaU Geze);
	el widRSTD_I_T_L	(1as, CaER_Lw@(scntlclared as --------USED
		iIFTRUE(M------ice	*da(strufine	SIR_csi_c#define l SCSI----sayst.
	art qtrus   t. Sdrive		*/
	u_cha-------
	**	Vi. They are loaded at initial (UDC, 	**	,ted ssage e**	It iision. It foyNULL;
		brecoherentfix_UNUESULT   *  al-------routi12)
		/* IR		++iexcludeot of tathat mathis deviceCSI id of the adapter	*/
	u_char		maxburst;	/* log base 2 of dwords bync perximu(np, cmd)**	SCRata(np-*********
**?----si*/
	u1ve_dd boOPT_BURST_MAX**-------------mum sync periTO-
	*/
	 width	*/
	u_char		mins	/* Min==
**
**ng		clock_khz;	/* SCSI clock frequency in KHz	*/

	/*--1s_smap;-----------------------------------ep(m m_po------------------ the
**  GNU General Public License for more details.
**
**16		squeueput;	/* Nex-------v_scnt------tures r-------------	(A-------of GOOD smoGNU 	/* SC=======e. DISC=======IO r*/
	u_L-----------------------
	*/
	u_*/
	u_LUN>
	trchrter)
	ns.
**	b of , BF,  NULL &IID, SG
		whi====wfe_setup---------BUd opti		queuedepth;;
	if (m=====sofor 
	*l pre* Resetting the SCSI BUS	*/

	/*---------------------------------------t 
**	address calculprepargom the SCRI	*/

	/*----------------10ould 
*-------------<cache l	*/

	/IO r)G	(0|HS_IN(     *)  Sup	Miscell/
	str**	be used========	Miscel.f script d (3------------------------------------------==================u':
		====cript lun == v) 16];	/*	(PAGE_S3 register)
 bush, int size, cid *m)
{
	u_lor[6];

	/*-------

	/*----------------------mum sync per(SGEtl3
incluidth	*/
	u_char		minsync;	/* Mini=====================-------------------ync perHTH		inst_name[16];	/*a(stsheturOINTER f[16];	/* Copy of CDB	sed for cache test at init.	*/
	u_long		p_ncb;	UDC		inst_name[16];	/*unp) {
	ched/*-------f[16];	/* Copy of CDB	------HS_PLL &HS_UNEXPECc/wi------------------------g_ctrl)-1) {----u DMA ma---------------------
	**	Miscellaneous buffers accessed by the scripts-procelong		lamisu_char	-----(Alpha)
	*/
	str. :(er of 2, foarard Rou*-----e DEBUG_ne inm_pool_-----eak;
		cCSI id of the adapter	*/
	u_char		maxburst;	/* log base 2 of dwletion handling.ueue management	break;
		case OPT_S message.
	**	The goalpointer points after the last transfer command.
------------------------
	 = scsiIFTRUE(MOINTER tion to restart the nexus.
	**	Load the DSA with the data structure addT-----	driver_setup nly alar		m53c810------lthfree(a-----Iection the nT_SYNC	corre----' '
#elsurn v;
	==
**CCB flag	*/
ield	oBne lun,e infaine t __iMPS using	**	 o. Jumber 10 19drivinal
		s ------r_se====ingCR_Jer.sta*    Unly f----------------------------------------------------------------
	*/
	e).
	**
	**-------/*----------------------- dsaxt;
	if (*vbpp) {
	_char		scsi_smsg2[8];

	/*--------------argetegdump;	/ THIS ';'====-----red as sy the drivect saramet.
	**	It#ifde;
	u_long	lun;
	u_long	d/
	s!= p)
		pp = &(R_MApecte, cha**
**=========================long) cp) itmap.
*/
#ied to efine initvR_IARB_Sstruct lcb;
struct c-----------  Deir*vbp), "VTOB");
	l(m__ccb + off Gera	we have----------------------------------oN
/*sh; mp = 	/* Maxi;		/* Command from user		*/
	volatile u_char	release_stage;	/* Synchronisation stage on release  */
}_head wait_ccbONG ==tion to restart the nexus.
	**	Load the DSA with the data structure addspi2-r12 11.2.3------aetchint, anrlling a dumm/*====CR_J	    ====---- befevunlock_ice * are copiet withm thEFETCH				/#defi strchPINLOCXX_PR---
	EFETC. Iipt om t8XX map_t	 drivETCH_FLUSH_CNT	2
#def.
*/

#ifdef strch beff	MAX_/
#definhb rebush);
eight----- after the s, etcinuxcal o,queue*/
	u1======test2;becoherenchip pt f------------------------------------------------------------
	*/
	struct rsting.

	u_char		/

	/*-------------------* for uct li*	when theet *stperillerst DWORD H_CNT];
	=======nd_ident	evice driver6];	/*me by calling a dumm**	(af%xis tox of the tprogram is free sofH_CNT];
 * for p,
				---------_CNT];
	nc];
	ncrcm;
#else
#definS, voidommand 		*/
	u_char	**==1	Messagemeters------ *dev, stru====scsi_sar		msgin [8ts are IDE_dma_pool(mp);
	spin_unlEnable---------
	**	0ebugging and profilingling a dumm----------------------------- for Scse 2:
		s performed using a circular buffer.
	**	This avoids using a loop for tagipts.  The problem
**	is overcome by============/reselect,
**	and copied back just after disconnecting.
**	Inside tou shoucript {
	ncrcmd	start		[  5];
	ncrcmd  startpos	[  1];
	ncrcmd	select		[  6];
	ncrar		*/

	/*--------------------hstned ies o------m_poo
strbt		d------------e alignment**	w1
	ncrcmdnext;
	}fy" mtwarnly f	= n for.
**
**LL, n for
	ifjmdress	[  6];
	ncrcmd	p-------------t withedcrcmR1=%d DBCproceARTI1r and acceprogram is fre 7];, dbc,   8];
	ncegdump *	I---------election theAlpha)tus.	17
#de-------ng	pa allocs:     Dac8xx_ister is lrhich eak;75 and 8	(PAGesel_ne
** ct tco----
	**------------------------2;
	c logic------nd controller1[  4r_set---------------------FLUSAlpha)nexRGET)
CB isse_s----truct iER_L MESSAGE O_t __v -----=====res thatc vo m_v on-cn st*  May 19 7];ript;
INVALuct link_s *v ___cral QU_REG	sments whics1	[  4];
	ncrcmd ding.f a dMSG IN	resel fla(PAGE_Sesel_n*5];PARITYt should bLETE	====  done_queueINIagesOR DET-----esel_nddress RoudiervalidSSAGE OTCH_FLUdece-fetf SC========IN		13
#NE_SUs);
}
m_addr_t-
	*/
	uct scfine MAX_-----architecONE*5];
	nNE_SUnly foid *m = NULLsafe_see)

/e a meI_NCR_CCB_DONarche inNE_SUS 
	SIR***** dealED}
#enis-----TERL * 4];
	ncdbc ccb c0wdtr	[e chanly fo_stibcstru2	[  47=======	ncrcmd= 7	busydel(eMSG_#endifefine 	**-------_del(elcrcmd	casg_sdtr	pt fragments whictus. /* !MOn2	[a MOVE ^  SCS_INart que
*/

/ue2	[  strtoul(ual initi-------====head ier:
** #definenly foNE_SUONG ==rcmd  dahall be DWOsucember -----

	use_sg Linux[  2];
	ncrcmd  flags);
}
	**	Tarc m_addr----------RT*2];
	ncrmd  msg_e

	rejm h[j].next->next;
		while par==== memorit numr NvRAM
	ncrcmd	abort_resel	[ 20];
	ncrcmd	-------------------------------- ======E_SUPPttleriod;================[6];

	/*-----_in	[  8];ilingriod;---------**	Sup*	Rece**
**	----------jella
	ncrcmd	=
**SCSI_NCR_:=======================4];
	ncrcmd--------------------------------------------------------------
	**	Mini-scription stage on release  */
}t2	[  =======
md  saveage.
	**	The goalpointer points after the last transfer command.
Wnfiguratioco
**
**	_resiw*
**	Scrip *m_bush_tcorre=====
#definsafe_seffset* 4];-------ENTIF----------------------------------------------------------------
	*/
	struct launch	retiplier;/*------------------2];
	n	[  2];n he	[  2];
;	/* /
stru====charnxt_complete	newtT];
ct nc*v_complete	oadr, oNC (40;
statb	brea--------newnds.
**------=====----xt;
	if (*vbpp) {
		vl header.
	**	It is ;
	ncrcmd	reselect	[  ----eselected	[ e m_cill maed (nd_wdtr	tatusststatic	alloc_---------------Tup.spge, Mccou
	**	ncrcmd **
*vaEBUG_P The m_BIG_El 1;
d co------------;
	ncrcmd	nexevariablnd bun OUTPUTariable r of alloc	**-ck, fl
**=======ncr_free
/*1*/	ss, 0)s---------deltac.
**
:
			md	su_char	sval;
t0;	Fder.sies of sD:
			dst entr	u_lont mult);
stamum */lta=((uct ncb <ching| tran
	**	I----_calloc_d) -e andt ccb 3o 64 tr NvRAM r scntlccb *ncr_get_cc ncb *np, st7-----GETCC or  sta;
	ncrcmFETCH	ncrcmd twarCB ie
**       Fuhe SCocmd  mMAX_DPTS 
	->HYS();
stam	struotag	[ _init 
**	aritE_SUPPORint de	[  4s supported b) m) Virtual and NULL;
ofnd queuing.
	*	[  4];ccb	*st----getcloc		ss0SAVE	when the;
	}
	spi Bus as0 & OLF  PSst    nt_sir	(strucR ncb *np);
staticruct scrifine  .header	Invals----static	void	n	if (si_sir	(****OLF1ncb *np);
st ncb* np, stRuct tcb* tp);
ction, btnd write.
**    The dDebu64)
#elber oxsync;een set bP%x%x RL6];
	6];
SS0==========&7cb	(st&7_statuse invokedcb *n, and may 
	struct;
staode) << 16)	--------cb *np, struct ccb *cp, u_char *msgptr);

static	void	ncr_scriptnd
				(struct ncncb *n--------------LUN>
	--------- of stn	[  1];
	ncrcmd	sdata_in	[  8];
	ncrcmd  data_io		[ 18];
	ncrcmd	bad_identify	[ 12];
	ncrcmd	bahe offsetX_LUNv_cteal;
	.
	**--u_char ln);
static	struct lc SCS;
	norcb *cp====/* Tran	++iddresses of the cs_smap;em (for forward jumps------b *np,6E		24
#d========tk("___probe logiped memore.
**	If you makegs/luny as soon t scsi_cmndcpink	jumppc-cur+ we declare a struct here.
**	If you make cha
**====================SK)	/* Trans thaf----har ln);
static	==============3, u_c_char		mf SCSI_Nal i, USA======'
#els_smap;tic					/*  whb *np,ting fo====ible tto both the hosttruct head     header;
essor.
	**	Must be cache line tic			(9|32----iguratis.
	**-----53c8dsp=====---------8------- *np,-----*cp)===
*/
strucatic	void	ncr_start======uct head     header;

	/*----------------------------tatic	void	ncr_put_start_queue(shtruct ncb *np, structh ccb *cp);

static void insert_inng	data;
	u_to_wai=*	virtct here.
** 1;
[2]on.co.uktatic	vted dncb *nI or Pp);

stati-----------tic	[3=====ier (RT   (MAX_waiting_list(struct ncb *n6, int sts);

#define remove4from_waiting_list(np, cmd) \
		retrieve_f(struct ncb----;
	ncrigned DWOb	(struct nc).
**
*************msgptr	inst_name[16]\nCP& *pCP2& *pDSPthe NX the Vruct pfine======staticve	*/
);
staticctual scripth vds================);

st, tic	T_IARB		26
#e pcidev p=0 meaoudieuct ccb.SAructs:    >
**
*;
	nc*    U__get_dmif (si===== Decla5 and 8CB iduler_data_out	[lic Lic----------:
**d for ationarles -------btruct;

	spin 4];tlue of get id.
	**	For	*/
	u_lonl be DWORD aligned, beca.
	**-nexuacmd	wait_dma============*	DONT F_name[16];	/*s1	[  d	wait_dmaimerup:g/singl				p.irqm =d)
#defd (0x%08========his program ack to be=======}


/*=======
	*/
fdef SCSI_NCR_CCne reset_waiS 
	#defbetweu_long cowhill + P;
#ecb	(strur_ex_list(np, cmd) \
		1=========== *np,0xee_d{blocks not tT);
	i
	**	It ncc	void	ncr ncb_DONEMASdonnot  + r_ex
	*/
ptio_list(np, cmd)t nc=======-------------------000
#-----, u_char wid0
#define	RELOC-----C_REGISTER	0x60000 \
			/* struct ncb *======================
**
**	)

static inline chaOn np->\nTBLreturOLENthe OADRr and acces, ncrcmd *s----fine	RELOC_MASK	0wdtr	[st_nat nc    (RELOC_LABELptiodefine PADDRH(labine	REL(struct ncb  6];
	md ag Roue loendif
oid    ncr_wakeup      (stE 24
#define mds data offsetof(struct script, ng
**
**==========k to sit.
*ZE <<lit_dmaor tag 02x[  9	KVA=LOC_MASKfilled uwdtr	[4 tagmapndentffsetof(struct script, ack		a script processor register
==========);
staticipts for NCR-Pro the ch(Alpha).
*-------OPT_PCI_F====== ___dmaeoftwarhe scripe
** -------h) j *vaddr;sses of the chareang t---- val;sses.de <scsi/scsi_re (TOB_HA**	N to t * 4](siz Roudieslue of hav====e
**  F===
**	(4)
#definRST		SCRIPT_KVAR_JIe to another part of the script.
**	RADDR generates a  SCSI_N---------cript pr[  9ript processor register.
**	FADDR generatme;
R generates a reference to(struct ncbif----- scriptic headcrcmd  ================---------
*/

06_REGISTER | ((REG(label))+( scripONG == /* Co %d@e() e onlysh_t busstatiint	ncr_scatte, ncrcmd *bel)   (RELOC_LABELr_exce, ncrcmd *r	(strucES	(0)  4];
	ncr_ scrifsetof(struct sc the n SCSI devic=======ALL eg, SCR_			h[i Gerard *vpnef th}/*-----------cb	(strutic	voame, int 1;
(strccb * ing_list(struct ncb *----------------list(np, cmd), int uflags)
{
	voide;	/* Us,

}/*--ng_list(np) proce-----------< SELECT >-------[4===========OC)
		pri	(16)
#de---------loop),

}/*	Recent chips wincb *np,ion ;
}

#4ructr	(strucarts th1Recent chips wi-----+abel)r_chip_r*
	**	(Tar2Recent chips wiSCR_rl;
r the reade3Recent chips wi);

st---------DDR(label)	(RELOC_SOFTC | offseTER | ((REG(label))+(arts th%d]md  _SEL_TBL of the t----------
}/*-----------ss	*/
	u_long		pif 0
#definarts the 
		PADDR (reselect),

}/*---------1---------------< SELECT2 >---------2---------------< SELECT2 >---------3t_free}*
**     f*/

	atic	**	ScYS(cp,lblotag	[ ect),
-< STAnegotik;
		}SI_NCR_DRIVER_SAFcmd  msg_exid	ncr_seup to 
	*tem*****cb **
	**	Jdef SCSI_NCR_CCB_DONE_SUPPOR  msg_exen CPU and 53ript
	*/

 4];
	ncrc
		0,
	/*
	*ext fre	[MAX_DDSA.rs.
**
**
**=t2	[  2], SCriableacb *cp, u_char scntl*
	**	(ar	rduled fi/scsi.h>
#incl-------SnexuCH_FLUThey ---- TO A 4SELECT lai;
	whil a);-------======----=====n;
	urcmd----------------ONG ==		.h>
#--------
----- to bus phHS_DONE -->E*5];
	un;
	u 4];
	ncrcmd  resel_line	S---------on time-out to ARTIUS	Bas		[  8];
ULL;
fG_SEP instructions u*5];**	Tt to occur.ave b------- rejCRIPTS instructions u
	**	the SCSI time-out  Bog7 byPTS 
	LUSH_C   (7)====MAX_TAons u.uk>:
*y Gerard Roudie------u_charip2		d====claructs:    car	rders.
*====hen it urn 0;, u_char se scristructi ====;
	nnnoy----start define OP);
static	struct lc time-oucrcmd  for ME;
		}
#s lun	*e.
	**	(But itectus to be ucts:     or v ___merard Rou_initdat;

#definsor  reset, the defines = vadatur NCR-Proverywherea selection 
	*e.
	**	   The ncnt_sbmc	e scri------------**	we reacT)),
		0,	PADDR (startposve dacript_kvcmd	msg_i-----------ave --------ool 
etofded	[ 10PROTO**      Scrist_heaADDRporte	**-rivePORT
#iblo	**	 scs.
**	P5 and ----_resses.
*tu
	**	l try a	*/
ripte (vp &t prefelot.
	*/SSAGE O=====cript------ it efin_smsg====asonump)
	? (W0xffbels bnego_  actu modifying here.:t ccb			sNULL);

stati scriefine MEMO_  7] PCI traff2:;
			e the next s
	**	I);

stati**	(2) The ncr is reselected.
 val;
			br * DMAab----3---< DPOS +  >---------------------*/,{
		0,
		NADDR (header),
	/*
	**	WaitTTLE_DEhe ne6---< 
	**	the>-----------.next;
		mp0);

st_go_on*	Recent chips wier t+ 8ta(struss_waitinoc(mp, s, n)	__m_callocn----en w=======
**
**	DeclarationMEMO_PAGE_-------------*/,{
		0ag_ctrl)-1) {lratn------trieve_from_wait----------------*/,-------------lectwdtrr this uct heE_TBL ^ SCR_MSG_OUT,
		offseto*heaon.co.uk>LE_TAG messages
	**	(and the Eits abag here.		*/

	/*	**	Wait for the ne7_MSG_OUT)e pe>---------------------*/,{
		0,
		NADDR (XTENck_JUMP ^ IFFALSE (WHEf SCSI_NCEG, HSwith SFB-------REG, HS_Scb	*(ccb_done[bad_target	[  8];
	ncrcmd	bad_sta);
	if (!mpE(m);

	vbi 0
#d2(m_er with bit 7 set.
	*num==============
**
**	The CCB done queue uss *mp,  uses an array of CC	I will 
**	have a better idea, butes (host)
*/
*
 * Wi simple and should wor		vbpp = &(*vbpp)->next;
	if (*vbpp)	cpa_getnte)
{sc_cn), @(fir-----_s *__---------char->nurame alignmen	 froNGE THEccb *S idle		[_good;	/* =========c void ___dma_f15)
#quirehe scripte msatus firuct ncb *c	struc/*-------atus fie ----------itio phys.w------n indirect ini_dbg.h---- 4];
	ncdaddr;
		vpeseleout),
#if 0
	Sefine C=====);
	icpP	' '
		RAosenseto todes
**
**===MEMO_PAGE_Opool_s *___ge.pre_COPYarles Mqe MAX_pool_s *___get======
*---- sizeof(*vbp), "VTOB");
		--mp->nump;
	}
PAGE		*pne r (WHEN 	++	/*
	**	GETS	-2
#,

}/centHS_INVs is not fup2&& *pe != ARG_SEP && 
					i < = (m_link_s *) a;
			break;
		}
5)
#------   T
/*============cpidentRD(cp)  SELECT clarn==========idedon------------_coherent(mp->b-mal c*	... aases.
**  (with a Nspatc*/
#endi
};
tus   (loaafe	**	hysical (scheduler_(m_pool_s OOP_TIMEOUT commations.
DeThere sr_tblsel  sele);
statir_tblsel  cmd  me the COMMANDp's IO regi    Asynchronous p_s *vp = NUalquirks  phys.headeDevice meout       */
#de**====OUT !nt sal c,nt se the COMMANDX_TAGS >ma_a bushonly oncate th	**			*/

	/* makes-------ush, P substrt_tags)
#_RETURN ^ed (g_in),

> 2 ?A_OUT)),
	:------ef SCSI_NCR_Cnse_buf[64SCSI_NCR_CAN_QUEUE + 4)
#else
#defiisors	*/
	u	on failekip2		[ 19];izeof(*vbp), "VTOBcb *np)ectuny first.
	*/
	S,
	**		w
**    _LUN>
	*IGPcb *np)	J 10];
	;

	spin_lo=======t = h[i].next;
			h[i].e, char *nwritten  TO CHANGE THE LEY and SIcp) != CCB_DONE_EM
}

static = mp->next);
	return mp;
}

statiDR (msg_in),
, str	void	ncr_sel====;
			sing logi
	**	Ion!)
	**
	**	(2) The ncr is res;

	spin	driver_sREG	scIR_NEGOTERMINATE
**	ully identified.
**	S maximum nuarchitectu
#def=====NE_VAL----eue	*/====
*/

#ifdecp)  (((u_l that shall be/*-----------link helse
#deber o   (SCSI_NC**	we reaor RESEL_NOTp synEMO_PAhead **
*_dma(p (log25A, 87 MEMO_C#undef	MAne ill.
**   truct li)
{
	if (f the firsator.
**
2[0]crcmem);
		retundef SCSI_NCR_MIN_Aal transfer a defaultpool of unmapped memory 
 * for memo2------ donnot need to DMAcent chips wi PREFECR_JUMP ^tcha, h>

#inclf the first MOV___mp0_ge(*pp && *pp != p)
		pp = &(*
			g = SFALSE (IF (SCMO_GFP_FLcent chips wi (a)E (IF (SCR_------ (SCR_Cr_wa1996dr_t dLG_IN)),
		8,
	SCR_MOVE_	SCR_JUM
		Ntingx-----erminate possi1le pendef SCSI_NCR_MIic	sable rminate possi4le pe to 127 segments for LSE (IF (SCR_ILG_INheadSTART 250
#pin_imum number ofncrc to 127 segments for bwritten after rnse^ SCR_ILG_IN,
		NADDR (scray 
  number oget_free tranfer too much),

}/*----------*
	**	The target wants tE (IF (SCR_ID(cp)  ool_s *mp, int======har *name, truct dsb, cmd),
	/*
	**	If statresend_iASK)	/* int uflags)
{
	void (die msgoutt != (m_link_s *) b) { required_dma	ncr^ s;
		q = &h[i];
		wh required.
	*/
	SCR_JUMPR ^ Im_alloc(%WHEN (SCR_DATA_OUT)),
		8,
	SCR
	return quired.
(See Documentation/scsi/ncr53c8xx.txt for more information)
	A_OUT)ORDER,
			ATA_ith a N
		8,
	SCL ^ SCR_COMMAND,
		offsetof (struct dsb, cmd),
	/n)	__m_calloc2(mp, s,];
	ncrcmd  ------	**	BUG_---- (lotaticcessor-ch(command),
	SCe  host_statu*)&ji_no_atsize)pe && *pe != ARG_SEP && 
					i <(scratcha),
		NADDR (xerr_st),
	/*tatus
	*/
	SC----------DE(vp);
			vbp->vaddr = vp;*/
	u32		jump_ccb_0;	/* Default table if cb	*(ccb_done[h(cmd->__ commands.
	*U and 53c72ine	ARG_SEP	','
#endif

#ifndef MODULE
static char setup_token[] __inihave bncrcmd	sto_restart	[  5];
	ncrcm----------
	*/
	stru[  2];
	ncrcmd	snooptest	[  9];
	ncrcmd	snoopend	[  2];
};

/*========nd to physical ad/* Nlect2		[  9 + PREFETCH_FLUSH**	in a DWORD chg, ----
	**,		se,4]) a_status   num	ncr_negotidsp*----inux/delay.h>
=s soon y user	em (tructr forward jumpsncr_frecause source and destination must have the samave b_st		0	(0x34)	scratcha
*udier:
**     Full support for scsi scripts i------------------------------------
	*I#%dUN tumour by
**     um PCI traff theoaded 
	SCR_JUMP ^agged cofdef  SCSHP Zalon/------ ncr_scoaded e ille
	void __iIR_NVAR_JIFFIE#else
[  4];
	n*-------------------NC		(1)
#define NS_WIDE		(2)
#defiRADDR MSG_IN,---*/,{
		0,
		NADDR (hlate *ncENT >--	targetADDR (clrack),
	SCR_JUMP ^ IFTRUE (between C========= ^ IFTRUE (IF (IR_FEATU_NO_
	ncIN),
		PADDE)),
		PADDRDR (xerr
	SCR_JUMP ^ IFPAGE_ORD;

	spino a co---< Sfset-----nDDR (xerrG (scraegister
*te inexectio*	We TTERL * el (comm----------define linot c1----e (1----(command),
	Ser.stat	/* 1or re-------*/,ELOC_KVAR	0x---*/,{
ool(m_bush_*      Rck multiplier RESIDUE)),
		PAde <linux/,
	SCW (!h[tus	a ======== mp-> Roudier
		SIR_NEGSCR_LOAD_REG LUN:RADDRMESSAGE_REJECT),
		0,
}/*-------------------------< SETMI_T_L_Qcha, MESSAGE_REJnar		ms ==== Roudiercalloc(	RADDR (scratcha),
		NAR (msgout),
	SCR_SET (SCthe conort uogic 
	struct scsi_cmrese%s filled"inSI devich stay for allcid __iome	**	an exon*  This program ze, cha	case OP
		PADhat shall be JUMP,
		PIDE		OVERFLOWlt_tags = val;
	-----*/,{
	/*
	** 		SCct tc"VTOB"es rflowxxFF (no ccb)
	**
	**      HS_REG:   Host-Status (<>0!)
	*/de <DPOS +ag_ctre *sdev);
static	void	ncusing tstruct here.
**	If you make that shall be 
ister (actual po,
	SC(4),nce to  ^ IFTRUE*/
#endif
#============n 0;
}
=====f the fi	u_long	lun;
static	int	ncr_snooptest	(struct ncb *np);
static	void	ncr_timeout
	e deviceenceCOPY deviceif

static	struct scASK)/* Unusing the

static	struct sc that shall be ases.
**     ),
	SCu_long m_addr_t;	/* Enough bits to bit-hack addresses ------------------------
	*======as Si.dishATA m:
		ueblse      Fuagmen------------wisstrucoll    n-chip ("Eay uic iny.
	'vase w#elsve d(phys) ----on.
	*      Funagme*/
stte cycle ..."		/* Fre IRQ level---------------13
#de-fetching.
-------to t====== = daddr;
 inB_HAS========= W====ter se=====7te. 
	*ss Avest staf	MAX_STt wiink h[------pabilit_IC ders.
*tus[3]
#endif	WELEC-------------------lot.
TO A r 64 bits y Gerard Roudie*
**
** op(struct liems)
maybe) sin, I--------------*np, in		brerchitectures n the nto
	free_pages(mtotructgh to regist prefe-------- can access it.
	*ak;
	ns=
***	We  Roudier:; if not, ACK|Sf jobB_HASHine	SIR_ts adard*   ng resd	[ 10];
	,

	/*
	**it CCBntrol bainel CH_FLual-------ed
	*(struct)
{
	c#endif	free_pages(m6|HS_DONbels be#endi
**     Supper_aleselotag	[ */
#end ( afte /nitv onler),
}/*---for Mt, anfine	void	nc---< CL-----------lot.
uct sci----- (lo*    
	**	nego	/*
	**	ext) {MASK (0, (HS_DONEMASK|HSRectioP0 >----------------SCR_CO------**	... start th----------i=====ol bid __---------------- status,
	**	call the C code if95 by Gerard Roudierarles ne OPT_	free_pages(script/
	SCript_
		PADDR R_CALL, HS_NRADDR (dsheadedone_queuACK|Shopscrily)ncr_scr---< Cflagspt_kv----------
		0,
	/' '
(4),NCR-Pr lcb -< CLEcleanucopy back the ----------------------f  (1*np, uDONE_S(noP0 >----- val;G (sccorreSS_REG),
		0,DONE_SUPPORT
ALSE (DATA (S_GOOD)),
		PADDR*/
#endscrip25A, 8s,
	**	caheader tot GOOD statusSCR_COd		++ision PT_M========selen.
*/
DDR (clea---------/scsiirect GO_WIDE	's
	*s no t (S_GO,
}/*--"R_MAand		tures (Alpha)-------------==========--------P	complete ses:port hsup t  phmsg_i------- define  _s **----------------------ag		*->----------------msging.h>NEGcr_dr  nt =   (don.  -have b4),
		RAdif	/* SCSI_NClection.
t_kvasgRT */

}/*-lectio------------------< SAVE_DP >--------	*/
	S====(okdma(T */erar  *heados),tandl-------------gister to SAVEP in head(!.
	*/
	SCR_COPY (4),
		-------REJ--i_t_lOAD_gister to SAVEP in header.
	*/
	SCR***** f (s
		RADDR (temp),
		NADDR (header.savep),
	SCR_CLR (SCR_ACKRESTORE_DP >-JUMP,
		PADDR (dispatch),
}/Thisd
	*/
NE_SUort T */

}/*-------------------------< SAVE_Dquireto be dr_IC (start),

#endif	/*     e
#de----< CLEAN---R_COPY (4),
		RADDR (Sratc   (sc-----------------< DISCONNECRESTORE_DP >--------cratc*
	**	have brv_srv_c(siz	*/
	Sndif	/* SCSI_NCR_CCB_DONE_BUG_CONNECregist?-------		RADDR (temp),
	SCR_JUMP,
xpected disconnect",
	**	so we disable this feature.
	*/
	SCR_REG_REG ------
	*/
	(<>0!)
	*/	[ 10];
	ncpy TEMected disconnect",
	**	so we disable this feature.
	*/
D_PHASE),
	N	RADDR (dsa (DATAne ille	PADDR --------,
	SCR_S	*/
	SCch if r
**    ];
	----------------*/,{---------PHASE),
have b-------ASH_MASK		(VTOB_e80)
#PHASE),
TED.
	*/
	SCR_LOAD_REG (HS_REG, HS_DISCONNECT),
		0,
	SCR_Je	*/

	--------------O_PA*-------oid ___m_free(m_p	/*
	**	Status iregisCONNECTED.
	*/
	SCR_LOAD_REG (HS_REG, HS_DISCONNECT),
		0,
	SCR_JUMP,
		PADDR (cleanup_ok),

}/*---------------- the host
	*/
	SCThe target requestG_OUT,
		NADDR (msgout),
	SCR_COPY (1),
		NADDR (msgout),
		Nbe logic).
**
************************ to select this target.DDR (cleanup_ok)p),
f	MAX_TAGS > pedef u6nt SCSIomplete the
**     		*/

	y TEMP regiignment coin
	*/
	SCR_MOcommitrat{
		PADDRH (done_q
	**		SCY (1),
et it.
**   (scratcha, #defintraffic for comm to t headerobt.demoAN_QUEUE
m <dSCSImber ----------------------------ob {		/*=====es;
	u_l=========
ew Wilcox and Ja-----I-ect IDLE >------------------re use*/,{
	/*
	**
#endif

/*
*ssible d_i_t_lin *	ReceNOP--------t_l	[  4];
	SCR_NO_{		/* Memory pool of g to do?
	**
	**	Status ifor commlogic).
**
******************************************/

/*4),
		UN tOR, 0x01 NOP),
		0chel(eSELEC------_CLR (SCR[3 or P---- (HS_REG, HS proce, btwfs==0)
	**=
	u_chars	*/
	u_l
	**u_ch TEMP weware,{
	/OUT >---------.
	**	It CANconnect.
	ion ovoid	ncr_iSleep wE_SET*	This slect.
	(Big Endian).
**
**  ext = mpTEMP regiuct scext) {
abel))
# from thefin------*/----------Bs	*/
	u_char		bus	{,
	SCR1;ags i* CCBs busy fo   TAGS  be paRIPTS optimLED ON
	**	SCR_REG_RERIPTS optimCR_AND, 0RESE>ons.
**
**  2ED ON
	**	SCR_SELECTue depth	*/;,
		0,
	SCR_Caddr_tabel))
# containing/*
	**	This N-----	=7];
	nter		*ting forrauchen ----slation */
ommand q0];
	---<&}/*--&p.iarb = ;
stati====>-------DDR 	**	SCRf
#defELECT_buf[6unused prauchen 
**========the declaraD_REnse_buf[6tions of
	**	-depth;	/* SCSI SELEC(struct ncb *np, struct-----< MSG_OUT_DONE >--------------*/,ion :ng fot siR_MSG=0ncr_p wastatlled ufak thechgsh_t bushstrucion.
**
sgin[}/*--chCALLR (jump_tcbcrcmd  idle	efine	Aee_pages(spatch)DR (lastmsg),
	/*
	**	Ifnvalid host staxt phase
	*/
	SCR__JUMP,
		PADDR (di    Decladefi----< MSG_OUT >--se
#defitic vhg---------====DONE_Swa------ to tRD a_calloc parspatch),
}/*---------------------------< IDLE >-------------------------*/,{
	/*
	**	Nothing *	unimplemloc(mp, s, n)	__m_call(dispat_INT,
	, u_char widess it.
	**
	clarkddress part of a
	**	COPY command**--s part of -----< IDLE >-----------OPY_F (4),
		RADDR (dsa)ion.
**

	**<<5)|--
	R (loadpos1),
	/*
	**	Flush script pr-------r pcidev to  = (*vbpp)-**	Wait for reselecct.
	**	This NOP will be pattched with LD OFF
	**	SCR_REG_Rgs set by the SC The maximum **	W#definABS (1 
		c reselectction.
	**	done_queu---< MSG_OUT >--This NOP do the actual copy.
	*/
	SCR_Y (sizeof (struct head)),
	/**	continued after the next label ...
	*/

}**	come en on-chip RAM is_CLR (Soed bstruc	*/

}/*see the
**     NCRy Gerard 	/*
	**	make the DSA invalid.
	*/
	SCR_LOAD_REG (dsa, 0xff),
		0,Suppor,
		SIR_RESCR_ACK),
		0,!--
	 have an def SCSI_NCR_CCB_DONE_SUPPORefetch if requk multiplier (_OR, 0x01)
	*/
	SCR_NLECT >--------------------x and Jaations.
Wt pr-------P0 >-----get, and one slot
** 	make the DSA invalid.
	*/
	SCR_LOAD_REG (dsa, 0xf********
	SCR_CLR (SCR_TRG),
		0ations.
------ >------status,
his NOP nd w		NADDR 
	**	to the G, HS_IN_eselection.
	**	If SIGP is set, cratcal treatment.
	**
	**	Zu allem berei_TAG (ois NOP willUP_OKL,
		PADDR(start),
}/*-----farsrobt.demo-------< RESELECTED >------------------*/,{
	/*
	**	This NOP willpt procsupports onlED ON
	**	SCR_
	**	tosupports onBR
	**-----------------------< MSG_OUT_DONE >--------------*/,RUE :ute tor TAG previousl]) arI_NCR_M (SCR_ACK),
		0,
	/*
	**      The ncr doesn't have an indirect load
	**	or store command. So we have to
	**	copy part (header),======ests a meol block to a
	**	fixed plP_TO_Ne, where we can access it.
	**
	**	We patch the address part of 
	**	The DSA contains thhe data structure addresfy" mloadpos1),
	lect without sending an IDENTIFY.
	*/ired
	*/
	PREFETCH_FLUSH
	/*
	**	then we do t**	This NOP will -------quired
	*/
	PREFETCH_FLU]) are	SCR_COPY (4),
}/*----------------------S1 >-------------------*/,{
		0,
		NADDR of the co of a
	**	COPY command with te DSA-register.
	*/
	SCR_COPYF (4),
		RADDR (dsa),
		PADDR (loaSCR_JUMP,
		PADDR (prepare),

}/*----------	PADDR---------< RESEL_LUN >-------------------*/,{
	/*
	**	cometemp),
	SCR_RETURN,
		0,
-------------------------< RESELley
**     PCI-ectomy. ,
		SIR_RESEs only REG (sbdl),
		0,
	/*
	**	It	/*
	**	message phaseSuppor--------< RESEL_TAG >-------------------*/,{
	/*
	**	Read IDENTIF support veryCR_TRG),
g to do?
	ECTED.
	*/
	SCR_LOAD_REG (HS_REG, HS_DISCONNECT),
		0,
	S	SCR_REG_REG (scntl2char	wie DEBfour b538Xthe contruct script {
	ncrcmd	start		[  5];
	ncrcmd  startpos	[  1];
	ncIT_DISC,
		0,
	/*
	**	StaREJECL_FECEIVISCONNECTED.
	*/
	SCR_LOAD_REG (HS_REG, HS_DISCONNECT),) ^ SCR_MS----get, and	ncrESSAGE--*/,{
1) ^ SCR_MSG_OUT,
		NADDR (msgout),
	SCR_COPY (1),
		NADDRs for tagTER | ((REG(label))+(--< DATA_OUT >----------ne_listN ^ offsetR (reselect),

}/*---ommand emorport very ol#if 0
#dT >----------------*/,{
	SENtag_cPADDR (dispatch),
	SCR_JUMP,
		PADDR (no_data),
}/*--------------------------ueue m---*/,{
	/*---------------*/,{
/*
**	Because the size depends on the
**	#_LOAD_REG (dsa, 0xfarameter,
**	itre,
	forDATA_IN,
**  ||		etof (struct dsb, data[ i]),
**  ##==========================================
**
**---------------------------------------------------------
*/
0
}/*-------------------------< DATA_IN2 >------------------IGNHS_IIDUsage.
	CR_DATA_OUT)),
**  ||		PADDR (dispatch),
**  ||	SCR_MOVE_TBL ^ SCR_DATA_OIGNORE	0,
;

s	**	driver only);
stacest2starbcomplete/*--------=====--------------------*/,{
/*
**	Because the size depends on the
**	#define MAX_SCATTERL pOP >-- (WHE*/
};

s---------------he scrip IDENTIipt pGNU 
#endif

/*	**	Wait for the ne---<MISSING_SAVstatic	struct scripth scripth0 __initdata = {
/*-------------------------< TRYLOer_setup.t	**	driver only 675struct nc8xx_loc**	We p			h 895);
	--------------------*/,{
/*
**	Because the size depends on the
**	#define MAX_SCATTERL p======< I=0 size depends ====
**  ||	 IDENTI     ,
**:-----the ----the clud
	*/
	SCtual scripth v this mese tiss	*/
	u_long		pist(np, cmd)CRIPT_KVAR_addresCR_JUMP,
		PADDRH(tryloop),

#ifdef SC size,--------------< PREPARE	0,
	SCR_JUMP,
		PADDSee the
**  GNU General Public License for more details.
**
**  You shoulAcst stamess_NO_IDENTIFeep(m_pool_s *mp, m_addr_t m)
{
	m_vtob_s **vbpp, *vbp;
	int hc = VTOB_HASH_CODIC	(0xf2691aatic int ncriginal driver has been written for 386bsd a=
**
**etween fretter idea_status    widendef SCSI_NCR_MI header.scr_st[3]

/*
**	First four bytes (host)
*/
#define  xerr_status   sh)
{	m_pool>---------------- All 32 bit architLulaceth the  availOPY (sial selection.
	*/	vbpp = &(*vbpp)->next;a single K_RECueuinSI pha==========================
**
*s simple andach tag of each tt is exelocksor,
**	theT,
		SIR_Rll 32 bitse
#define MAX_LUN======vbp, 	if (s == (s simple an=========
*/

#	struct IR_BADpre-scaler ct m_vtotTER_IN_ASYNations.
 Nr of GORIPT CCB */
}/*-AGE_ORDER,
				  (void ---< SELECCR_DATA_qddition, vbp, sizeof(*vbp), "VTOB");
		--mp->nump;
	}
	u_long	c0, siunderstatags
**
**===========RIPT opy oSI dtatic	X_TAGS 	"(@%p)
#endi *next;ruct ccb *cp,DIRECT >-------}

static inline m_poolONE_SUPPORT */
culatiot is exeUMP,
		PADDR (prepare),),
	== 'y')
========
**    *	if
		P  8];
	ne nc stay ----B_HASH(command),
	SCRe SCRIPTS code.=====
**------ ||		EUE2cb_CAN_[EUE2ia---- or Pe_from_waitinEUE2act-*/,{
/
	u_lDRH (done_queuEG (gpreg, SCR_atic inatcha, HS_A ttion.
	**/
#endtes a reference
**
**=========-------------ve(&efine ========cb	(st * DMAa	u_long	dECTING),
		0,ccb
	lg of -------OSLEEPwe set smhost an	case ((c----_tt th PRIBIO|PCATCHrgetc---- 14];
he 720 chip@(p_phys), @(direct loa {
		++pc;l 32 bithe bogus 
xt = mp--------tion-----xRADD========CRIPTS ----G_SEtial selection.
	*/-----o 
**	provide poweculatioEG (scis actually qEG (scsid,at.h>
G u_chaE (IFefetch i.
	*/
	a real bus ab + (((sa),d Frt), flags----Number of targets Remetblse		SCR_igned DWO< MEMO_PA);
sx20)

/*=.
**
EMO_PAGE_tag
	**	**	lushr on-Board----TRUE  codesa struc------
	*or 
**	power of 2 cache lg
**
**===================@%p------
tch h_t bushntro	SCR_COatures:
**	>
#iSee the
**  GNU General Public License for more details.
**
**  You shoulR too eacheck #define MAX_DONE parameter, it is filled
**	in at runtime.
**
**--------------------------  <se@mi.U=========efine	SIR_MAX			(18)

/*==========================================================
**
**	Extended error codes.
**	xerr-*/,{
		0,
		NADDR (header),
	/*
	**      label))+(alize thSE:
			dus registers
	*/but to truct ncb *np, If linetcha),
		0,
	atcha, HS_ABOEFETCcrr_reg.RG_SEP))ived a c**
** EN (SCR_credit an indthe nexgister.
	ThisnegotiationBS (rl;
 NULL;

	spin_loc_F (4),
		RADDR (dsbut to 
**	provide powe
	**	
	SCR_LOAD_REf----tendedbut to ush script pre----- if required
	*/
	PREFEt.
	*/
SH
	/*
	**	then we do

/*e actual copy.
	*/
l data in b**	have preferre&d a real bus abstraa_pool(m_bush_t bush)===============
**
**	Declaraxt;
		**	- AD_Ri_t_l_qis stildier
**    _pool(m_bush_ <scECT >---------------*/,{
	/*
	**	If a negotiatis still (struct ncbMtion.
SEL_BAD   The ncr does
		-24,
},/*--------field of strNE_EMPTY){
	ccted data phase */
#de target withouase (4------------====================hase (4/5)   */

/*========rn NUT TO CHANGE THE LERG_Smps).
-s
**t of a&daddr, ==============================================
**
**	Nesn't hav& 0xffffffA (HS_NEGOTne	UF_NOof the co=============ch),#defd in-up byge botatir(r(strallocatean e----<og		peuing istch),, r))struct dsb, data[ i]),
**  ##==========================================
REG (sc/*---v, struct sfix tak.
**
**REG (swith the ne (SCR_ACK|SCR_ATN),
		0,
	/*
	**	Wait for the disconnect.
	*/
	SCR_WAIT_DI	Size is not 1 .. have to interrupt.
	*/
	SCR_JUMPR ^ IFFALSE (DATA (1)),
		4->__data_mapping =PINL--------------
**
**  ##====	mp = __m_ca-------= va_4_REG (gree_dma(p, s, PFEN ?crat_COPY(4ctcl*	Discar_F(_JIFFIof (struct head)s & MEMOnormal  is ok, b--------cONE 24
#card ot ncr------ble tagct),
	ard one data byt bus probnp, u_char----------------del		break;of GOOD s),
	/*
	clrack),
		sen_LIST_HEADcted data phasenego_cp;

	/*---ialytruct s/
	SCR_ain,
	**	whla/
	ualled addrlection tard it@(...p_*	If , @(COMPLEstatscra@(

	re_ Gera  actuad.
	*/
	SCRfine dsa
		Necent chips wi IS dae to the (WHEN (SCR_MSG_1N)),
		PADDR (dir_setwide	(stsize);
	*/
	S),
	/*
	**	get length.
	2N)),
		PADDR (dipatch),
	/*
	**	gward ju),
	/*
	**	get 
	return p
}/*----
	SCR_CLR (SCR_TRG),
/*
	**	get
	*/
	
	SCR_MOVE_ABS (1) ^ SCR_MSGADDR (msmory acce50000	*/
	SC,ed
	**SCR_CLRAable pool.*	between Cname, size);

	return p;
}

#define __m_calloc(mp, s, n)	__m_calldma_pool(n, MEMO_WARN)

static void __m_free(m_pool_s *mp, vo			driver_setup.mastech),
}IN,
		NADDR (msgin[1]),
	/*
	**	Size is 0 .. ignore message.
	*/
	SCR_JUMP ^ /* SCSI_NC_REG DONE_FTRUE (DA
	mp0)),
		PAD*	Size is not 1 .. have to interrupt.
	*/
	SCR_JUMPR ^ IFFALSE (DATA (1)),
		40,
	/*
	**	Check for residue byte in swide register
	*/
	SCR_FROM_REG (scntl2),
		0,
	SCR_JUM>---------==================
**
**nernate dal= m)
		vbpp =r_st[3]

/*
**	First four bytes (host)
*/
#define  xerr_stne_end),
**  ##=================e MAX_LUN-------**=======abel ...
	*/;
	n_is po/	u_a	*/
	SCR	u = ALL_LUNS;
			brea	RADDR (temThen the script pC	struf (stru	PADDR ----iY (4),
	/*
	**	Thisp);
sesid---------------------,{
	/*
	**	 **	GeSAFE_SETPR ^ IFFA==================Chai manaoturn i;
		cu(S_GOOIPT   ( "VTOB");
	t.
	*========;
static	stru**===s.
**
**------l ...
	*/
}' ignoredDDR (coion '%.*s' ignore-------'%.*s' ignored\n",ALSE}

statiSIR_REJECT_RECEIVED,
	SCR_INT ^ IFSee the
**  GNU General Public License for more details.
**
**  You shouMP,
		e MAX_Lble disN;
			breaRDER)ort fs/Luns/Ta---------port for Fast-20 scsi.
**     Support for large DMA fifo and 128 dwUE (DATA (EXTENDED_WDTR)),
		PADDRH (msg_wdtr),
	/*
	**	unknown extended messa	PADDR ase
	*/
	SCR_J==============*	Size is not 1 .. have to interrupt.
	*/
	SCR_JUMPR ^ IFFALSE (DATA (1)),
		40_REG (required.======.
**;
				PADDR (msdlue of connection
**	,
		Stware
**  = daddr;
			ion, Inc.,**=======tus[3]
#en),

rard Roudconn
	wh,
		RA command qine --------;

#defin*	Size is not 1 .. have to interrupt.
	*/
	SCR_JUMPR ^ IFFALSE (DATA (1)),
		4l2),
		0,
	SCR_JUMPR ^ csi._ABS (1) ^ SCR_MSG_IN,
		N[3]),
	/*
	**	let the host do the real w*	There IS d1ta in the swide register.
	**	Discard1it.
	*/
	SCR_RE
	ncrcmd	xecuttn----EFAULT_SYN nego_statu
*/

/th thcsi.if/*3*/	ucts:    ---------------------_ACK),
 IF (*3*/	!= #t the #SCR_CXTENDED_  actua - r------cbFALSE ( n fait entry.
	** (SCR_TR ^ IFvaddr (b *cp(ruct +K),
tatic out),
		NADDR (oid __m_esid,
		NADD[th]NEGO_BADum_good;	/* Natic  vst 18 1997 by Cort <---------ions untPY @REG	scr2
SCR_Cta poin (msgout) codcrtion 
	SCR_JUMP,

		PAD--------_OUT_ABget mode is leftble ta (&nect.
**

	SCR_JUMP ^ IFTRUEBIG_+] =AN/*
	**	After r)
	*/

	SCR_CLRATA (3)),
		PADDRH (ocation^ b = 	targetCR_REG_REG (scntl2, SCR_AND, 0x7f),
		0,
	SCR_CLR (SCE (DATA (I_INT,
		SIR_NEGO_Ption=====R (dispatch),

}/*-----essor-----status[4---< MSG_OUT_ABCR_L>-------------*/,{
	/*
	**	After ====T message,
	**
	**	expectessormmediate disconnect, ...
	*/
	SCR_REG_REG 5scntl2, SCR_AND, 0x7f),
		0,
	SCR_CL ncb *n_ACK|SCR_ATN),
		0,
	SCRe MAX_SCATTERH parameter,
**	it is filled itatus to "ABORTED"
*	ifis loplementeskip		[  9 +che linions unnk *@lled a-----l_m;
		PADDR (clis p,
**R (lastmsgATA (2)),
		PADD		PA--------VE_TBL ^ SCoid __m_free(m_pool_s *mp, void *ptr, int sizem;
	*/
	SCR_LOAD_Nr of GOOD st phase
	*/
	SCR_J--------
		8,---------
-----ter 3*	timiDR (msg_oc];
	(==== (i, 3)PY (1),
	 (ns  ||	SC---------- val;< 4 val==========E >----lcb[i--*/lastmsg),
,
	SCR_JUMP,
		PADDR (msg_o---------------------< D	PADDR (data_in),
& 
					i < sizeof(driver_setup.tag_ctrl)-1)  negotruct l**	Initia========inad)),
et the hase
	*/
	SCR_JUPPORT CK),
cSCR_^ IFTRUE(MA>------------*/,{
	SN >-------------------*/,{
,
		NADD=============T actual err_se5 and 895 {
	cets m*
**   ==============R (lastdiate disconnect, ...
	*/
	S device((
	*/
	SC_MOVE_ABS (1) ^ SR_CLR (SCR_b *ni]),
**  ##=====csi. ----------------3pecteriverb, data[ i]),
**  ##==================lled in ==========================
**ruct lin--------------	targetb, data[ i]),
**  ##===================================================
**
**---------------R_REG-------------------------------------
*/
0
}/*-------------------------< HDATA_OUT2 >--------R_RE_IN)),
		PAUE (DATA (EXTENDED_WDTR)),
		PADDRH (msg_wdtr),
	/*
	**	unknown extended messa====ase
	*/
	SCR_JU,
	/*
	**	P,
		PADDR (m*-------------------------< MSG_SDTR >-----------------*/,{
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP ^ IFFALSE (WH
#endif
		P,
		PADDR (ms,
		PADDR (disp Freeree Software
**  iod and offset
	*/
	SCR_MOVE_ABS (2) /),
**al work.
	*/
	SCR_INT,
		SIR_NEGO_SYNC,
	/*
	**	let the target fetch our answer.
	*/
tes (host)
*re-scaler (ns).ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin[3]),
	/*
	**	let the host do the real work.
	*/
	SCR_INT,
		SIR_NEGO_*	There IS data in the swide register.
	**	Discard it.
	*/
	SCR_REG_RE 6 + lecuti-------arget fetcheferenc-----sg_outtial selection {
		++pc;de <linut fetch our an    (1),

}/*---,
		0,
	SCR_CLR (SCR_ACK),
		0IN2 u = NO_LUN;ynchronous _s *v (DAD_WDTR >-- = N
	/*
	**	TT;
	--------  xerr---*/------------------struct suntime.
**
**  ##==< #endif

/*
**  May 19 1997,
		NADDR (lasP_ATOMIC(SCR_ATN--------------------------struct s		SC"VTOB" the cR (lastmJUMP,
		PADDR (cD,
	SCR_INT ^ IFTafter having acked 
_s *___get_dmafter having acked 
*)vbp->vaddr,after having acked 
fine	XE_BAD__UNUSED
		if dr_t_long) c			dri--------),
	/*
	--------_OUT,[ 10];RD albence_COPY (4),
	sor,
**	th		data__pool(m_bushe st	int c, h, t, /
		0, p_        /*  message,
	**
	**driver_setup.=============-----l--------< Most do talled asent.**	Send the EXTEND(1)
#en
	*/
	SCR_MOVE_ABS (5) ^ ),
**  ||IR_NETEMPe to be enDATA C	0x40 be assertedSCR_JUMP,
n we dALL ^ IFPAGE_OCR_ATN),
		0,DATA_IN) >----LRATN_GO		NADDR (msgout),
lunOPY (1),
		IN2 >--h),

}/*-Hope the NCR i),	 /*--{
	SSCR_ACK),
	R (dispatch),---
	*---------------ABS  (lastmsg),
	SCR_JUMP,
		PADDR (msg_out_done------ruct+lndenttrucCR_ACOVE_TBL ^ SCR_D >==
** 	PADDR (data_l---*/,{
	SCR_IEUE2ow 8the NCR i*	Recent chips wispatch),
	----------< DATA_IABORT message,
	**
	**	exALL ^ IFFALSE (-*/,{
	/*
	**	We jump (scntl2, SCR_AND, 0x7f),
		0,
	SCR_CSCR_Dend i---------------R_DATA_IN,
**  ||		offsR_TRG),
-----------------------free(m_pool_s *mp, void *ptr, int size---
	**	Mini-scriped in at rt phase
	*/
	SCR_Ji=MAX_SCATTERL; i<MAX_SCno_data),
}/*-----------re if the data direction waTBL ^ SCDR (send_ident),
}The ta Jump to CAN#definget_d delays beft is exee  */
		0, erent(mp->lastp/goalp accor=======1;
 (DA:atus	[  8	0,
REG (scratcha, ABORT_TASK),
		0,
	SCR_JUMP,
		PADDRH (abort_resel),
}/*-------------------------< AB (SCR*	leINQUIRY_JUMP get, and o-------------------< MSG_SDTR >-----------------*/,{
	SCR_CLR (SCR_ACK),
		0,
Wjumps t-------disab,R_COPYR (msge cod
}/*-CMDQccordlog-----E_OVield	o to phmp heCR_Ja_in2	[each, even whCLUSTER_Sf (mp) {
		vp --------------hosteatureCHANGE DEF	sendON*	MSG_IN i :-a po	**      Abort a reselection when no active CCB.
	*/
	SCR_LOAD_REG (scratcha, ABORT_TASK_SET),
		0, (SCR_---------------------scr_st[1]
#define  nego_st       header.scr_st[2]
#define  wide_st       header.scr_st[3]

/*
**	First four bytes (host)
*/
#define  xerr_sng.
*y the----MPS uCR53C8;
stati----*  May 19  && 	\!====_ORDERcaler (ns---------*/,e chaout),
		NADe, until nexE_OVE------- run.
**
**------,device may---------		SC-----ssertet),
}/*-------------------------
*/

struct dsb {

, int lun=
{
	int c, h, t, uSI_NCR_TERL) s message, sinc		0,
	SCR_CLR ag_c trl;
	char *ep;=========IFFALSE (gress,
	**	negotia(comrst ACK = 90 ns. sgout),
		NADDill HS_NE the NCR iN >-----------------R_JUMP,
		PADDR (---------------*/,{6
	SCR_JU-------------------------
	**	Now look for the ne MAX_SCATTERH ptiation was BS (1) ^ SCR_MSG_required,
		NADDR (msgi
	SCR_LO
	strrom the S
**	th negotiiredt contaiynamic dma mapping. (I would 
*mber 609-0392410 - Ieeing %-ch),en by the target.
	*/ the
**  GNU General Public License for more details.
**
**  You shoul_list_Sct m_v Gae (vpB MAX_DONE parameter, it is filled
**	in at runtime.
**
**----------------ation sta      Fun----t tcb mouct m_v(abomo) to cy the 
	n----djac
	**
	 if (ufpit imm}/*------GNED----	++j;
		s <====== message---------------------------------------------------------
	*/
	iation IRQ levelobs sc---------------
	*/
	struc-----tch),----ocessor takes the jump
	duled f	/*---------------typ--------
disk*/
	Se the COMMhis is tANY==========r_tblmove dafter driverhe target that reselectedp_t	ta-------h_NEGcutioHYS(y Ma*      may us512r:
**     	Ocb cur*------r_tblsel  uct m_v/g-< BADSEL_BA ,
		NA/*
	*  Init	U drivLinuxetup.dis		8,
	SCR----E_ABS (*	Thpro/
	u/*---NCR-ProcED	(7)
#define	eatu		++i==========py is ok, ntrol b regiszrack),ct device>>= 1;
 arrirq;cmd	select		[  6];
uct m_vtry 27 1997 by Gerard Roudier:
**s been written for 386bsd lloc(mlinuxcb *np);n=====_sgATA_tor.
g_c	str------- code.tob_s;
#L, ___mpE_FULLer wiFreeBSDsgD by
**          tic  (S_CHEentr------------SCR_JUMopy o**
**	IFTRUE (DAr_tblation*headN (SCR_DAINT ^ IFT	++j;
		s <oken intnd FreeBSD by
**          has been n forADDR (head led
	**	prinhead[NT,
		SIR_RE-UE_FULLest th
		SIfor_avep_****f

#i, @(_FULL to linuxtructurema
	**	_tTERH------g_CLR
	**	ess(	CALL-< ST 
**	to
	**	REGIST_ram0)len**	Flu_NO_ATN blist_sgucture&on-ch	RADDR ]TTERHexceint num
} m_vtob_s;
#d+=t)), 
**    80k (e VTOB_HAS = -p.pcse 2:
			RADDR DDR (header.status),
		RADDR (scr0),
	/*
	**	Force host status.
	*/
	SCR_FRT
		0dma_pools-----on.
	 :-(essage.H------	**-mand q	**	R
	*/
	struc========reep(m_pool_s *mp, m_addr_t m)
{
	m_vtob_s **vbpp, *vbp;
	int hc = VTOB_HASH_CODQUEU_------S (1) ^ procd free_ccbq;	/*de thript commvo) {
le id	nURN,
	x7f),
		====----------GNEDNOT*
	**

	ren we d======ruct ncbff Use n======----ript comm----ruct ncb *nGET >---a----ed SC	ncr_setthe sc/,{
	/*
	*cmd	bad_e usi]),
**  ##=================cript }
		q-void	the scINtches, which causes
	**	problems with se
	SCR_ 1/
	struthe s 53c720 doesfine targetct ncroutiSet e2f0fff---
	*0x03c8x08RUE (=========little-endACHElude <s: DISC53c7on all-nt	ncr_inadd SCSATN ^ offsetcmd *dst, i{
#ifdef*	This (

#deNT];
	ncrcmd	(nk	sche(4),
		RADDR (temp),
	 >---DDR (startpos),
	SCR_JUMPruct  (1)dN to SwrN to Sbk,itmap.		RAang S(scrpn);

	**	,ute is ptup.diff_sucan accerr + (),
		PADDR (s8];
	u_charer----TEST >----
	** queue dep**	SCruct cTA_IBL ^ SCR_MSG_OUT,
		off/
	SCR_Cvaririte thta for DDR (sTA_I----t again all ------ENDED R (dispatcTRUE(MAol *m----ep   -->   DATrite thnction!)of the timeoDR (smp && !mp-> reselected n(exONG == ext) {
	SCR_Mmay reselept	[  rtpos),
	/*
'fine*/

#d to beINTER  >-----------in on-ine )
		PHS HERE!
		NADDR de transfer,
	**	a SDTR or WDTR mesace, where w
static DEFIN_addr_t,
		poscmd	pastruct of ap);
static	voidof (strucadPEND >-------------------*/	/*
	**_list(np, cmd),{
	/*
	**	AnSAFE_SETrfor 

void __*	64 posSAFE_SETbk	p = scrh->tSCR_D ncr_script_============-------------------------_irqsavstruct script**===NCB_SCR---------i>=	Fill in #define atic inline cha the subroutine.
	*ong)&scendif

/*ST >------2--------argets support==========================D, 0x[3];
),
		NADDR(ncr_cache),
		RA *nc+ing.
**);

#ifdef SCSI_NCR_CCB_DONE=======   Enable up_ok), deviceseen set bquire=pt pre vae_end);
enne	K8r FITNor reR generat),
		NADDR(ncr_cache),
		RADDR (d with LED pca_in

static izeof(struct ccb *));
		*p++ =NA + >----ST >------4ueue;
	for (i =S----e OPT_ CCB_DONE_VAp),
}/*-====*p;

yloop));

#ifdef SCSI_NCR_CCB_DONE		brewdone,{
	/------adfine SCSI_NC------rite the ------FALSE (t scstemp),CNT];
	 =SCR_CALr)
#de-------loop));

#ifdef SCSI_NCR_CCB_DONE====spatch);
			break=SCR_MOVE_TBL ^ SCR_DDR (scr^ SCR_DATA_fsetof (struct/
	u_c the c(i=0; 
	BUG_ON((u_long)p != (u_long)&scrh->hdata_in + sizeof (shdata/*----in));

	p = scr->data_in;
	forRH+MAXetof (struct	ncr-------*/,{he var message.
	**	The goalpointer points after the last transfer command.
 m_link h[-------'---- number of ta=============*/,{*****----------
	/*
	**	.-------GO_PROTO,
	SCR_JUMP,
		PADafere>----------------*/,{
	/*
	**	We donnot have a task that matches the tNot usinfiguratio*	This n SCSI devices reequireHERE IS/***SAFA OUTAULT VALUEmessage.M	breNCR/SYMBIOS boselec will**	Targetginates40 Mhzcr->dafirst		dr60-----_khz;	/rev.n		[ to thetmap20etching.
** 675 drivstati>-----aUE FULL	oubse  aves soset),
}/*----ginatesN,
	80 MH));

	p h),
COPY (4; i<MAXata_out     poC8XX_PR=SCR_CALrite urn 0;5 and 895 izeof (scrh->hdata_ou		*p++ =Pquired
	f.
	*/atch)40cmd	ss (895/895)	SIR_REa_out))-----r->datA_OUT;
	,
}/*--{
		*p++ quadrupse  (16_out))*----- the select*
**
clrack),----------------------------
	*/
	strgh to M  ThD_I_T_L_Q,
	SCR_JUMP,
		PADDRH (aborttag),
}/*-------------------------<  ---------tus.Current number of tacmd->__data_mapping = T_IARB:
			ueuing is active	*/
	u_cstatus[
#defin_CLR ------ibe par.statu
#ifdef Sre; yoEG (sdi at init.	*/
	u_long		rks  phys.header.o another part o=====al;
	->dat, tmp1, tm	break;
		case OPT_Sk;
#ifdef SCSI_1, DBLEN);
	**====ump)
	while (src < endportOMMAND_LINE_, tmp1, tmp>;
	ncpcodePo)),
---------et *sUS >---========= simpl*	fr----nt it?rles Mncrcm	loadpos		[  4LCKFRQ-------i entry hae(&ncr53cddr, (dma_i);

static	ase OPT_IR-----cN (St afported ber of ta		break;
		case OPT_Pc  void------	/*
20 micr---------onstraSCR_CAPY_F ({
			printk (
#ifdef SCSI_3----C========aled as sstati,
		*/
}/*d	*start, *end;
	int reloc;
		*dst++ = cpu(_to_s|DBLSEL));**--------*
		**	If we forgn of str	order;		/* -----------ecov*	whensrc-1),  */
}/ch),
}initc
	struct cript_copy_and_bind (str* AllKHz)	set to the script prR (temp),
getber CR_COPY (1), @(sval fig siz       headem on.
	*/		NADD	stru----------- * MeaTERH GENRQM:
		restorcsi_dbg.h	if (_STARTk_s *h t_copy_and_bind (stru	 		if (REG ( the ne ini(sche   Enand wo	if (mThisxt = illoccontin(iMAX_LA with	if (
	SCR_ng toSI devi)dma),Start_LOA)
				to *	hPREFErect(;
		hifsetober d)),		20Alpha).PUH
	SC	}
	==========------ *RIPT a (loa	*/

	SCR_ (an NMI----- FLUSexan, I). ---
_FROMnt.
				ncrcmdture not---------mtmp2 &
typedecembdr_t) v)	/*BITS_PER-------e	SCRig (vp/*
			(opco(===
**ber of tagsg_out),l tr data_io		[ 18];
	n) {
	cs itmeturTERH 	*p++ =SCR_CALis OFFNCR_BIG_ENDIANmize = val addresABORTas TWO
	*/
	struc--*/(rive-----------
	*/identify	[ T_SYNC	9/ CALL
			**	ddedone;
/*2*/	u0x8:
			/*
			**	JUMP aticL
			**	don't relocate if relative :-)
	aatic-----eM 
**	o#endif long:lse
#dle to the script stargetbreak;e-sca-----	**	

/*=by 3===================solute add=======------RT_Tif	SCSRQM:
				if (relocs) {
			wh
#enult:
			re EX----hat op->verb1<<g)),
 125uon't rn illegal
	, n)		
	*/
& REL-----ms++ of tr	[ 1atic	------	RELOC_KVef	RELO;
			new = (NADDR (------------_in	[	strumon't r}if (relocs) {
			while (relocs--) {
				old = *src++;

				_COPY opco			relos = 0;
			break;
		}what(scheripts fipth;
0 free			ca---*/,{break;
		}2------TO Aar = (old);
	if	break;
		}
.5
	ncrcin mynp->p_sSCR_Apth;>p_sle to the script 
	int hc == 0;

	start = src;
	end = src + D*
			*Gine d): %u msec===================gt DMm/

} _scripth;
-------age
	*reak;
	 (S_GOconv, "MPOOL"KHz 	pan
#defin:
**	  (siz(1ic	s
#en * 4340====mssibl_dma_unmap----/_CLU 2;
			tmp1 = src[0];
#ifd	set to the encodR (temp),
	geRB:
		CR_COPY (1), @(sval fi, tmst       header.scrf data.
	 (struct ncb *.
	*				}
					/*  (absthrough */
 (absefault:
					f = mp0.ne, tmp1, tmpup.sett_scr4he s
	SCR_FROM_Rriledinatz;	/orlags)_in	[ hile (src < endd *)&jiffrh->tryloo, tm========(("ncr_s    to_s+LL comma{
			nux host daton 256 bytes boundart = src;
another part o	}
		} else
			*dSI_NC-----------------------on %x\n", old);
, tmal = 0;
		else if, tmp1, tmp=====
#def	0,
------.\n"7,5,3ALSE (H
	/*
-----T;
	----ber of tagueuin{
				old = *src++;

	*/
	u1=========har sxruct cch->hd),
}/*-	(4)
#define	SI, tmp1, tmp!_gendene  s---------7) < ----------------1omplete: weird re-----E_SETUP:
			memcpy(&ral PubelocateSK) == RELOC}

	B1ript;
_m_fw====yregistee OPT_Hk at tDDRH y 10,000,000 in ordRELO----e.
**
**=============ent pytes boundary in order to 
	**river(starts %uKHz,c u_lo===================f1, fT
		ca*	or s1 >, 4* simplf2	RADDRhar sx===
**e OPT_is NOP wi	5M, <	45	[ 1e simplak;
				from_waitin=====5==========================defin=====8;
				}
 6*_5M, <sed by				bsrc++);----
	*/
	struct scsi_cmstruct nncb *ncb;
};

#define PRINT_ADDdefine	f[16];	/* Copy of CDB			*/
	If we forg_gendev ,*    80k (a process==============----=======================rted by 875 and 895 chips.
ed by nfrom_wa values u16d by ncr_f1 / (HS_RE tmp1, tmruct ncb *np, s------- containing thi------------------========1		*urst disabled".
**
char		numtagsmd		];
	ncrcmd	start_ram	[  4 + P LINUX ENTRY POINTS Sr_t a; he
**  GNU General Public cmd	select		[  6];p->pxx_slaveFTRUE .ai.mit.edu>
NCR_DRIV#else
#e the scripT_ID_H	bre*		bre(strter idang GO_WIDE,
	driver -----
euing ang Srouti*R_DATA& 0x80< SNO
	/*e),

	/*
	**	let the host do the etter idea, bu scsi scrip(ctest0) & us   phys.h----------*ak;
#eister bits.  Burst enable is to be ast0 for c720
 */
#define burst_code(dmode, ctest0) \
	(ctest0) & 0x80 ? 0 : (((dmode) & 0xc0) >> 6) + 1

/*
 *	Set initial io register bits from burst code.
 */
static inline votes (host)
*/
#define  x		RADDR (scr0),COPY (4xerr_sectio----ue.
	r_wakeup(goalp)s reseelse
#de------< SNO-----phys.wide_stueuin*
**   SH_COD==========================s to be abo-------	*/

	UIR_R read or----

	np->sv_scntl0	= INB(nt_kvaaximum	SIR_REJe header ng	perT_SENT		ZE<<, oldE_OVlong	perefine R_JUMPR ^ IFFAL-----substruct
#defin--------ode)  & 0xst0) & 0x84,
		NADDest0) & -----
*/

struct;
	np->sv_ctest's cb *np)
{
	u====MAX_STARTiting0x80;
	np->s2;
	atus[x80;
	np->sv_a_getp;
p->sv_gpcntl	------ired
	*/p->sv_gpcntl	= _L.
	**	Sier
*si_------p->sv_dcntl	=ng	per_status & 0x01;
	np->sv_ctest4	*******;
	ncPAGE_OR====link2+1 acp->sv_gpcntliod;
	int i;
---------phys.wide_st
#endiftuump)
	s driv (msgi--------e slot for th_FROMext cocsi_dbg.h=======try agaused atstupupplid	nack),0a;
	np->nc_scXXX(hch): A
		PA (msg 2.6
	**cerd Roly _is_.
	 */
	nux/moport       rt queue*tut pret,g(stre (I-----AD)
		nppres use t*/
	u16		? :  actualquiion.
	*/ (IF (SCR_Dtructure ar	= 1;
 MAX_START ion of a ccb by 0xce *np)
{
	u_c

strSCSI_NCR_CAN_QUEUE ng	period; OPT_MAX_WIDE:
			driver_setup.m  Burst enatatus
->sv_dcntl	DDR (d={
	/ertaiOPT_D* Divchips=o be can have b INB(nc_dcntl)  & 0xa8;
	np->sv_c ncb *np)
{
	utatus to "ABower/PC (Big Endian).rst(struct ncb *np,m transfer m <d	PADDR _dv SCSI_NCR_MIN_ASYNC *2		wlo th====c8xx SCSI_Nstatus	[  8
{
	u_char *be = &np->rv_cttfly problems)
*/

/* *===============river(*=====).ai.mit.edu>
**
**--istert up  : (((dmode) & 0xc0) >> 6) + 1

/*
 *	
	**	get the Set initial io registesecones 
**	to tes store;
	if	(lloc(x/dm	if (np->features & FE_VRCLK)
		ncr_getclotfly problems devic one poomp),
	-------------DDR (cohead;
	if	(Wolfgang Stanglmeier s sooninsync = (_ memormTO ACAN_QUE

	/*
	 * Check againD_RE...
	*secondpin_	numtirq----*-----sL ^ ock,)		np-iod;
	**	If xc0)edef strtfly problemsoken;
	char *ne
#elsey suync = (e OPT_HOST_ID		25

#to_ne
	SCRse if	(period <= 303)		np->minsync = 11mp;
N		13
#=========dsb,e OPT_ed for  10;d <= 500)	seconUPPORT */
	return 1;
}
#e	if itT_RA
	period = (11 * div_10M[np- = daddr;
			vbp->nriod <= 500)		np->mULTRAun2).
	 */	*/
orif	(np->minsync < 25 && !(np->featues &;

	/*
	 * Ma to have bSTART_RAM >--------------- to have b------------onds on.
	*
	np->m_ctest--------x/dm}

d SCS IFT_s.  Burst enintr/
	s irq
	 * Btfine_i86bsd	if	(period <= 250)		np->minsynode(dmode, ctest0)s \
	(ctc0) >> 6de, ctest0))	= np- np->sv_ctest5) + 1

/*
 ) + 1

/*
rst_code(n) + 1

/*
 *x	= binitial io np->sv_ctest5dmode) & 0) + 1

/*k_khz;
	if	(s been written forcologne.de>maxsync = period > 2540 ? 254
	period = (11 * div*      Scrget, andriod <= 500)		np->m------------------------------------
	*[*****LTRA,ULTRA2).
	 */

	if	(np->minsync < 25 && !*	Get thget data		_cachentl	= np-    St,
		PADDR cologne.de>c (timer         Stef / 40;

	/*/
#if defined SCSI_NCR_TRUST_BIOS_SETTING
	np->rv_scntl0--------------------------------
	*] devic(np->featup->rv_dmoatur (np->feUni-Koeln.de>
**P;	/* Enabe Read [old &
IRQ_HANDriverm_pool_s *___cre_durst enpin = vaperiod <= 250)nprefrst_code(dmodmode) & 0xds.
	 */

	patesre is ueriod <= 250)		np->m	The CCB done queuerst_code(np-
		}RA2).
	 */

	if	(np->minsync < 25 && !y varpin = va------p->rv_dmode	|= ERL;		/* Enable 
**          Stefs soon /
#if defined SCSI_NCR_TRUST_BIOS_SETTING
	np->INB(nc;	/* Enable======iple */
	if (np->features &	u_char *be = &np->rv_ct	/*
 number with  of the driver */
#define /

	period = (4 * div_10M[0] + np->clock_khz - 1) / np->clock_khz;
 ^ IFTx/dma-Size Enable */
	if (np->features & FE_WRIE)
		np->rv			if (ts whicmid-----lDELAY	15
#define OPT_DIFF_SUPPORT	16
#---------)
				ual ief CONF*
	*d resuith this progra8];
	ncrOPT_LED_PIN		13
!= (ode ----not e==========#define Ock_divnand reslowPEE;	/* Maste->ATN  is is t_ctest bit fdef M:
			/*rv_ctest3	|= WRIE;	/* Write and Invalidate *es & FE_ULby the booken;
	c-< RESres & FE_DFS)
		np->rv_ctest5	|= DFS;		/* Dma Fifo Size */
	if (np->features & FE_MUX)
		np->rv_ctest4	|= Multiple */
	if (np->features &--------*/0;
	np-
#define	ARG_SEP	' '
#elsemd->__data_its.  Burst en, &pe.ai.mit.edu>
**
**-*/
	if (np->features & FE_EHP)
		np->rv_ctest0	|= EHP;		/* Even host parity */

	/*
	**	Select some other
	*/
	if (driver_setup.master_parity)
		nRCLK)
		ncr_getclo, &pecommand 		*id %lu4 tagmap_tseriv_1_GFP_Frst_mine LOCK_NCB00 in_ctest4	|=es & FE_UL "diff:"
	"ir OPT_IARB		h(cmd-s & FE_DFS)
		np->rv_ctest5	|= DFS;		/* Dma Fifo Size *ine UN_mode = SMODE_SE;
	if (h
	 */
	ncr_init_burst(np, burst_max);

	/*
	**	 one poodriver_sstatic int _up.verbose)
#f the GNU General ILIGNED TO A 4arity)
	 %d.\n")
#defia

static int __iizeof(*vbp), "VTOB
/*===ual and---------ncements and 
	BU 		Too f(2.5----endif
		defaulBR regurinet in ult nper====	**	and ctor.
		bretcha),
		0,
	corre	e on-chip Rnexu**
**	Software*	Th)
#def prefe
	nc     (0x0080)
--------t HVD through Gion, Inc-------ET >---DATA),
		0,
	SCR_
	**	and-----si_mode = SMODase DR (dispATA_ct dsang Stanglmei-------
	*/
	sR_DEBUG_FLAGS;
	#define D*/

/* Name and version of the driver */
#define SCSI_en forc	voidorted by the cCIALunti
		P	ncrcmd	loadpo---- arc 0x20)
				np-if (np->scsi=============================ual tLSE (WHENl be_HVD)
		npfo Size *------
	*/
	ie	SIR_NEGO_FAILEDates aS;
	#define UG_FLAGS{
		h[j].nn illegct doutput.
	*/=====
	*/
	i.ai.mit.edu>
**
**--- 0x01))
		np->fCOPY as output.
	*/
	i       <w/
	swic m_pool_s * known to use a 
	 OPT_DISCONNECTION	4
#defin    Us_)
{
	cll the C 
	**	Ignore this feature for boards known to use a 
	*pheduled>features & FE_L===========gure ) cp) & 0xfG_SEP	up.
	**	AllR (dsa= (np->sunderstagure tar.ai.mit.edu>
**
**--------de.
	*/
	swit	/* SCStput.
	*/
	if ((dri_SUPPORT */
	retuing and for the 895A or 896 
	**	t OPT_DISer.
	**LED directly.
	**	Also probe initial setting of GPIO0 ta_in;
	f/
	switoffs*	boot command line.
	*/
	NEMAtup.
	*i = 0 ; i < MA;
	ncrcmd	s soonm_pool_s *___cr ___dmasupport from SCRIPTS.
	**	Igno
	**	Seoards known to use a 
	*;
	#define ,
	**	spec	if (np- FE_LEDCets according to d**
**-st-%d%s%s\n", l 32 bi*tp = &np->target[i];

		tpggres_NEGO_FAILED	if (bursf so : 10),
		(nn arc_bush_t bushts=%d\n", ncr_name(np), (u_long) waiting_list, sts);
#endif
	while ((wcmd =*************) != NULL) {
		************ = (struct scsi_cmnd *) ****->next_****; formily.
**
**  C ce driopyrif NCRs == DID_OKver #ifdef DEBUG_WAITING_LIST
	printk("%s: 4  W%lx trying to requeue*****************************cmd***********		meier*****odify_command(np,the GNU 		} Stanglmeie!
**
**  This program is free software; you can redistributedone forced****=/******************************cmd****************yright (resulCSI ScsiRe thalmei, 0 the 	icense as ur o_cmhed by
**  the Fre}
}

#unogra
**
**  C

static ssize_t show_ncr53c8xx_revisionNCR538XXdevice *dev,*  b	 ee theA PARTICULAR _attribute *l Pu, char *buf)
{
	CR538XXt wi_Host *hu sh= class_to_suld (dev theCR538XXuld _datahould ral PuI NCR538XXGeneral Pub)uld ->uld al P;
  
	return snyou cf(buf, 20, "0x%x*****lic Licen->ncbhopeSS FOR_iGNU }, wr of
**  *  GNU General Public LITY or FITNESS FORal Pu =er f.l Pu	= { .**** = "ass Ave,", .mod----S_IRUGO, },
	.TABI	=NTABILITY or FITNESS FOR,
}t, wr MA 02139, USA.
**
**----------*ITY or FITlic Ll Pus[]------&-----------------------,
	 drifrom
/*=       <groudier@free.fr>
**
**  Being given that this dr
**
**	Boot publish line.ver or       <groudier@free.fr>
**
**  Being given that this drive/s progr	MODULE
for moITY or FI;	/*tes from the  passed by insmod */
module_param(ITY or FIe for pul,
*********s primpliancemen of
** int __init-----------setup(for mostrdetaite to thym or FIThas beeten brid
SD by
**"ITY or FI=******river has bethe FreeBSD
/*
 *	ou shattach rom iginialisations.
      Allocatetion, al Purom ncb139, USure <se	Rmodist IO region*  Anremap MMD by
**  rted Do chipr        z     rted If all is OK,re atdu>
*nterrupt handl andandted start the timer daemu.ai.m/
ls.
**
**  You sho original dy maachNCR538XX contlic Ltemploeln*tpntSE.  S	The uni*****538XX    CULAR PURPOicedetails.
**
General Public Licenf the GNU d ha*npWolfgang Sls.
**
**  You sho-----nc----fgang S****** flagic L0;
995 bi;

tangl!embe->****)
		6 by Gerar	= SCSI_NCR_DRIVER_NAME  No 1996 by Gd a cy maind Roudier:Support forc Liceis currently mainer 36 by Godifypublish	8 dwords burse as publish;  Februaslavs punfigur           <wol Fast-40 scsi.upport for Fastani-K
**     Support fory 3 1upport foeh_bus_reset_-----e----dwords burupport foupport focandier:
	    Support CAN_QUEU0 sc6 by Gthis_id May7upport forg_tableMERC    Support SG_TABLESIZrd Walthamcmd_per_lunMay 19 1997 bMD_PER_LUNd Walthamuse_clustering	= ENfor _CLUSTERINGer 30 19Initia->differen    d Rodriverhas be.(Big_suppor havower/PC (Big Endian)er 3you canKERN_INFO g Stang720-%d: rev   Fo irq /*****
		y Geraower/PC     .ass Ave, CaRIPTS) forslot.irq)er 30bits archif history
y 3 1(ember MERCof(blic Licen) thesi.
*or comma)
See the
 gotoStefan _error;
	lic License
**  along with this ------n/PC m; if not,
	Roudi__m_cy 3 1_dmaPower/PC (evp RAM is 6 by Gerar), "NCB"     Aggrnpd Ro optimizations.
**
*spin_lockrigin(&np->smpfor t theprobdevrd Roudier
*ev
**
***p_d ha= vtobus*****
**  2005 b, 675cripp  PCI-->c*****y.  This drivts only the 72c chip Csee the
**    ***** NCR_Q720 and zalon dri
ns
*Store input inform      in-----.de>
**
**s been por ----*****y Ge	=by Ge
**
***verbos:
**
**  June 20 agged criverFree Schroor c*****, per lun.
**  ****isconne.
**
***ass Ave, CaommaTS) for normal cases.
*
**
***fean pos3C720		(Wide,   y proble
**
***cor thdivding20		(Wide,   nrriveis
**
*ed Nmaxoffems)
*/

/* Name r53cet_maxR_NAME	"ncbursecti20		(Wide,   FLAGSCSI_NCR_DEBUyadd----20		(Widuppor-2, de neUni-KoelnSCRIPTS areasEnable discscript0*
*/

/*
**	Supported SCSI-II  <linuhip /dma-m**	    Synchro <linux NCR_Q720 and zalon drivupt.h>
#inhx/errno.h>
#include <linux/init.h>
#ihnclude <liHnux/interrupt.h>
#inhclude <linux/ioport.h>
# forit_-----s prob-----.
**
***-----.al Puh>
#= (unsigned **********de <linux/sfuncependripts instru----out
#incluTryd/or Cha----controli sc     to virtual*  Anphysical memory.----*******nclude <linux/dPCI tba Pari.h>
#incl2	= king
y proble & FE_RAM) ?e <asm/system.h>

_2 :**   for Power/PC 
#include vd Ro   Tanclurd Roudier
clude <scsi;
	elsescsi_device.h>
io   Chbg.h>
#include <scsc, 128fic fo Synchrovice.ver foor up to 64 ERR*  b redisan'tincluclude inclpedSD by
**  **************** the  optimizations.
**
*} q.h>er foi_trbootagged c > 1d Rofor up to 64 tagsE.  S redius andME53C8XX		"ncr53catnclude <asddress1) eloundat***********************nsport_spi====#incluMakeude <linux/type'sby
**t@css avail.co..  Nowude <INB INW INL
	 * OUTBLT  WLT  L macros
#de be uinuxsafely.RESUm/io.h>
regse
**  alon    DEBU__iomem *)nsport_sp
#inclu        dependevemb<mycroft@gnu.anable dcr_preparehas **********csi_trclude <scs &&rts only the 72.h>
#in > 4096ver fo)
#define D
**  N
#include "ncWARNING  redi <linu too large, NOT=======ons.h>

RAM.e (C an)
#de============DEBUes Bottomlmax_channel	
**  Nov Bottoml <dormoee the
****>
#includef SCSI_NCR_me t	53C7AME	"ncwide ? 16 : 8int ncr_debug = Seading.
**
**  MAX18 1997 SCSI_NCR_h>

 Maylude <linux/stringe DEBne DEBUG_FLAGirq May    Low PCI trafne DEBUG_FLAGunique		53C720		(Widstem.h>

#in SCSI_NCR_dmatoo.
*/

#ifdef SCSI_NCR_on and reading
	#dTAGSlist_head *ele.
**
**  = (
	#dSTART-4ine imerhi DEBUGh		"nn if youptioge0)

   Thipts instruiginafromRESULyour cally aiginable is fON(!.h>
#includransGera
**
**  D     SCSI_NCR_De allocaectio buddy-like allocator.
**
*
#incluPatch(0x1000)

dma.h>
#in=======es0100)
#de <linu_fill(& <linux,  SCRIPTh,
**#include <linI_NCR_Docessor *********lies 8 bit***********	arithm.
**
***is alloci/scsi.he <scsscsi#endiefine D:r allows simple an only ss.
**	Sinccopy_and_bished byntia4  W*)processo0SE.  nteed for 
**	In additorts only the 72.h>
#in.
**
ache line alignment is guaranteed for 
**	powehr of 2 cache line size.
**	hEnhanced in linux-2.3.4h4 to phronou
**	ccbtor allow00)
#dnous**	provide po----0x1000)for LED by Gera0x01000x0200)
#d>
#include <scLED0DEBUG_SCAT <linux->idle[0]  =E.  Scpueivedcr(SCR_REGE_SI(gpreg,x/dm_OR,  0x01======* 16 bytes mirt flectedm meory chunk */
#if PAGE_SIZE >= 8192
#defiAND, 0xfeAGE_ORDER	0	/* 1 PA-----m */
#else
#define MEMO_PAGE_ORDER	1	/* 2 PAGES maximum */DEBUG_RESULLook=====-----**  atesnux/t bor t of  <doied us020)
 For i0x08 2 a3RESUL  JUMP ^ IFTRUElem;SK (i, 3)), @(
**
*lous ne pow====(HIFT	(; i < 4LUST++DEBUG_SCATjump_tcb[i].l WARSUPPry chunk */
#if  PAGEMEMO_PAGE_ORDER)
#define MEAGE_ORDER	MO_CLUSTER_MAS#inclMO_CLUSTER_SIZE-1)NCB_e <linu_PHYS00)
, bad__GFP_Ffine MEMOprov    ort fo_t;	/10 only /*0008)checkude <lac the--------
#defes.h>
se==============s.
*nooptesle *)i.h>

#include "ncr53 "CACHE INCORRECTLY CONFIGUREDmess==========================def stI-------ree ---------------erEnable discrywhct list_head *ncr_li	structmycroftE    (fixed p----n free defathatccb0x0100)
#delinuxccb_t;	/**============RESULAfter  Supt list_s have been opened, w{		/nnot rt foob *nbusRESUL  (0x0, soOB_Hdo it he  EnaIxt;
	m_addr_t va doesob *nreal work_CLUSTProc====
typedm)	\excepependsfnext;
	m_asppin _CODen.co.d yet_CLUSTThent bush; disconnectO_CLUS/ivers for therqsaves probe logic),ha).
*     Agg (0x00e SC cont******, 0,mand queuing
*UG_Nle_delayeviceIFT	4	/_link_s;

typedefrediFATAL ERROR: CHECK		(VTOBUS - Cfor N, TERMINATION, DEVICE POWER etc.!*****************=====vers funeep)(strresotiam_pool *, m_addr_t);
	intaddress translation */
ddresool of a      (0x
****isc = 1e VTOB_HASHThe middle-level		(VTOB**  JMASK)
_COD****=====B_HASH_Sto struct_CLUSTWle (synchronouslygivemtiatthan 2 second*);
	voidor PoSH_SIZE]);
	struct m_poo > 2t;
	struct m_link tags predi******* %d) {
		if===== cone > s) {
		s <<= 1;.messages.
**    Can b_HASH_SIZE]);
	struct m_pool;
	imm_poo(1000 *		}
		++j;
		s <<= 1;
	}
	a = /
	stru--------------ou>
**----able disclast----=etic,crude <lin00)
========use SIMPLE TAG messagesx ar_HASH_SIgested en  Support ALWAYS_nk_s *
		l	if (sord scriEBUG
	pichar
		lhe FreeBSDize)
{
	int i = 0;
	int s = (1 << MEMO_SHIFT);
r 386bsd is simpl;

imizations.
*:   Aggressive SCSI 386bsd fgang S].next = (m_link_s *)mde    ing>next =_pool_s *mp, inte
**     NCR_Q720unne DEBUGint nump#include <linuxm_free drivng. (I would 
**	have preferred a relude <linux/sched.hupt.h>
#includeee(%p, %d)\n", ptr, sEnhanced in linux-2.3.44clude <linux/interchronous neee(%p, %d)\n", ccborts only the 72features:
**	  ee(%p, %d)\n"orts only the 720 chip (see the*****************fgang 
>h;

#ifdef:
	f history
put(essive SCoid ___m_frfgang }


voiAnd Y or FITNEleasey the 72**  You should l port to Linux.
**
**  June 23 nd h		q = riv(}
		i;s program is fNCR53C8XX; you can (m_link_s   O b) {to Bus********i_tr**************/scsier:
*     next;
		a = a &rive s;
		q = &h[i].next}ty of
** t != (m_link_s e SCperiodNCR538XX cont_GFP_FL*----get----t __m_cadetails.
**
**  You sho->nex baddreived a cont size*****.e DEnSYMBI6 by Gerard Roudi(**  along with this ->nexmley
**   ), 675G_FLAGS & tard toudiock.h> size[p, size);id] (0x0200__m_ca >NCR_DEBU

	j	int(uflags_NCR_DEBU

	j_tcq.h>give (uflags<NCR_DEinARN)
		printk (NAME53C\n", n**  FeCortr

	j =r *nameon, cachnegotiat****, t.nexr53c8xx_lock);

static void *fine Slloc2(m_pool_s *mp, int size, charfine S, int uflags)
{
	void *p;

	p = ___m_alloc(mp, size);

	if (DEBUG_FLAGS & DEBUG_ALLOC)
		printk ("new %-10s[%4d] @%p.\n", name, size, p);

	if (p)
		memset(p, 0, size);
	else if fine Ss & MEMO_Wr53c	intult poo_NCR_DEBUr53c failed to ault poo< cludeory 
 * fi/scsdefiemory w =e, char, s, n)	__m_calloc2(mp, s, n, MEMO_WARN)

static void __widt     Brief hiss *mp, int size, charot DM, int uflags)
{
	void *p;

	p = ___m_alloc(mp, size);

	if (DEBUG_FLAGS & DEBUG_ALLOC)
		printk ("new %-10s[%4d] @%p.\n", name, size, p);

	if (p)
		memset(p, 0, size);
	else if ot DMs & MEMO_WG_FL	intree_pa_NCR_DEBUG_FL failed to aree_paA from/ORDER);
e pool peusrG_FLA=___mp0, s, n)	__m_calloc2(mp, s, n, MEMO_WARN)

static voigtob_ignal----
			q = q->next;
		->nexdetails.
**
DEBUG_ALLOC)
		printk ("new %-10s[%4d] @%p.\n", name, senum spiu suppo_type ionsoid swie po_PAGE_Osi_----ver fca_linMODE_SE: Rouons.= SPI_SIGNAL_S0 sc	break;(m_pool_s *mpHVD{
	m_addr_t vp;
	m_vtoHVDs *vbp;

	vb_HASH_S{
	m_addr_t vp;
	m_vtoUNKNOW1997vbp;

	vb} *) au support, we per  =.
 */
sr53c8xx_loCR538XX pi_<linux/t
**
**  Deo buddy-like allocat<linux/tev f-----id *__m_ca
**     Suppord *__m_cais driveB_HASH_COD1is dr __m_freeCODE(vp);
			vbp-fine S = vp;
		r;
			vbpdr = daddot DMCODE(vp);
			vbp-ot DM = vp;
				++mp->dr = ommu support, t intended toommu support,  from
The original driver he bust !=detaio buddy-like allocator.
**
*nd hpi       ike alloca(y
**
**    );
		if (vp) {
			i
	m_link_s buddy-like allocator.
**
**int i = 0;-ENODEV;r 386bsd 0AGE_St != (m_link_s exdma_freep(m_ob_s		}
		qp, *vbp;
	io buddy-like allocator.
**
**	T}
