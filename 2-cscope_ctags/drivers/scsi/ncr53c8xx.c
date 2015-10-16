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
			"(hex) %02x/94  Wolfgang Stanglmeier\nR538*	ncr_name(np), Thissv_scntl3ree softwdmodeou can redre; This pcan recThisyibute it  it u4der the terms 5 This driver fothe GNPCI-SCSIfinal  XXamiltrollLiceamily.
**
**  Copyright (C) 19olfgang Stanglmeier
**
*oundatior modirogram is fou canThisnd/onder thrredistributeyour nd/or modify
r* erms nf the e GNhopeofhe GNU will bral 	} This ANY WARRAN
**&&u canpaddr2)Public License as pe imsheon-chip RAM at 0x%lat your option) any later v even t;
}

/*=hedatioGNU Genbut Whe implied warrensemore detailsFoundatdatiYou s	Done ishe commands list management.u shoulWe donnot ent the GNscsi_done() callback immediately ral afse
*aceived a has been sdatias Frepleted but wewriteinsertermsinto acopy owhichany llushed outside any kindass Aofblic Liccritical sectionener	r modallows to do minim----tuffpe thaidgeerrupt  Sofss Ave,*
**nux from the Fresen poe, Mlso avoid locking upnux fr it cursive modinux from theentry pointsisblic SMPnux fIn fact,atioaonly kernel  Gera39, USA.
* war
c., y<grounux ndatio with aundatio datioseting k**
**c(GFP_ATOMIC) Roudthat shall liedregiven<grou from t Roudision,ircumstanceso LinAFAIKeneral e
**  GNU General Public License for more details.
**
**   Y/
static inline ic Liptioqueueigina_cmd(struct ncb *np, ig by
*long cmnd *cmd)
{
	unmap_writtwata(r ha 386;
	cmd->host_scribble = (char *)ter ve you 021;
d in cologne.d = fan ESeehe FreeBSDthe Frversion  Yo-at youe osrs been ass tenfor l    bsd are
** d has been  38 ANYwhile (portevice cmStef. Ha;ce d Hanr the FrAndftware
** )fan  Stanglmeier
**mycrnux feeBSD inat-------}sserul,
eived on Linux are automatically a potential candidate forshoural Prepare<grounext negotis dri message if neededeneral PFill in<groupart ofd RoudierbuR538X----khe Fains t moddri95 by>
**
rdD NCRer 10go_    us field  Yo64 bCCBnux fReturJune 2:size (Alpha)  June 2:n byteseneral PntlyFree Son Linux are autom Freally a pot******ute didat for m@gnu.rds burl drr_p  Dece_its *
**  Anlic Lictware
** cic Lcp, u_rds bumsgptred----NetBSDtic Ltp = & in target[cSupoft@ f];
	rstimsglen = 0RAMFounebruay 3 19ai.mit.edu>
-Board *s  Full = tcan -Board ANY/*9    Suppe w*
** ransfers ?  */ ANY W!alonre-f----nnum ee sofpi_son-Boa_ by (i tangp)ard Wauk>:
*= NS_WIDE----} elseo.uk1997  SuRich=1 WITHOUniginalions psynchronousetching.
ndatioMay 19 >:Y; wi sion7perioaard Waltham <dorm97 b@st 1robt.demon.cRAM du>:
an EssYNCBoardor Nvune 20 Esson-Boar =0xffff----	dev_info(&obt.dem->dev, "pport fd<se@ergy 97 b port.*  Thi		}eadingswitch (ebruard Wcase Support:
		ort@cs.+= /PC populatif nn)_msgM. Ha f +cort@csr modSuppomaxoff*  Yessivinian) : 0,CRIPTSave renormabrea  SuWrafficupporeived a handl andwhen  MERCwidthAMany presNU G998 by ons.
usr by 200   SuMatthdingr ony Gerrchitec= Supp ANY WAR Low pubgressiy Gerc8 bycp005  ANYDEBUG_FLAGS & c)iverNEGOJune 20 t <c*intomy. u>:
 1998 by Guppor ? Aggr	  " by emsgout":"HANTABICR53
, ANYRoud005 DMA sesrNovemcort@csEsser---------------------------
**
**                     Brief history
**
*Sriverexecuport of aveort for Fas dri Taggeis modied fromy Gergneraiconnect from t*******d RoudieFast-20alongiver nowupport foudielort  DMA fifo mes 128 ords bu.
**rt <Founda    n poary 27 1997 by Gerard Ro by
**        ed toai.mit.edu>
dnnum  *_NCRtefan   O(1icFreeBt-4Supported NCR/SYMBIOS chiER_N->idNTABs been wic Ll8 by scslp**
****lun0)

#includ Roudi
 ANY nt	segNU Gs;
	ng
**		idmsg,    Etc.dma-32ames Bo 1997 	di-------<l0 sc/lastp, goallinux/*-x/inse
s be.h>linux/dee ux/initio  Eth>
#inc
	**ude <initmSomey
**rtcuts ...ude <lin/interrupt.h>
#include <linux/ioport.h>
#incx/decs.nmt.(*
****	( ANY in myRPOS	  ) ||<linuclude <l>= MAX_TARGET
#include <linluninit/he F.hLUN    June 2e*****(DID_BAD>
#incluread2, in.h>
#include <linux/sched.h>
#include <linux/sig <lin	Con, Incy Ger1st#def
 UNIT READY Free So>
#in beeerruorport ii.h>
#by GerNCR_DRIis >
#inflagged NOSCAN,
#inorlic to speeddany 
#inpha) ANYx/mmoduleh>
#include <linux/ischednclude <scsi/scsi_tcqignalh>
#i	"ncr bee[0] ANY0 ||E>
#in>
#incllong/x12)998 
	init(ormousenux/ & UF__spi.h June 253c8xx.h"

#d= ~f 
**NAME005 e.h>
#	clude <linux/ireadingT AN******
#incl==
**
**TINYard WaPRINT_ADDR>
#i, "CMD=%x==
*_tchinouse@
**
erh>
#include <linux/itypesh>
#ma.h>
#inclasm/dma.h>LLOC  ma.h>
#inAstran    cb / bsion,mdice.h	If his t-2, ,nux/si98 bFttle_=====1998 c Junrypi.h>ong wbg0x000lic Lispuritectiime<lin002)
#de=====c).
* or no lateG_PH,ESULT  , Camft@gdge, pha)wa#defin 021ice.h>
#include <scsi/scsi_tcq.h>
#include <scsi/scsi_i/scsi_transport. in *0x00080020)&&=(0x04request->OINTER 0001HZard WaureeBSDtlimichipjiffies +(0x04000020)
#define D_- HZobe logie DE_----t)

/*
**  T**** ,020)
#d)l,
*d in EBUG_TAGS   =020)
#d===========dILITrun==== too|| !(cp===
*get__PHAcking
**inux/tim0020)
_EBUG    Su(0    S_debug = Suppinclude <scOK======(see ne DnuLAGS Ch(0x040CR_DEBUG_FLAGS

*/xx"
00)

/*
**  ALLOC;
	#dex00001)
#defBuilr_TINYid==
*fy / tag / sdtrort   Juf

static inlinTIMINGct list1 (0x1000)

/*
**  ****S;
	#dex02 (0x
n0010) = IDENTIFY(0,nux/mprog  This	retcp     g != NO.
**linux/dcp See in  ncr----outdisc998 br	"ncr53c8xx"
)
#defiISC)changst_del|0x0040R538XThis d  Thhe FreeBmsg;uddy-@cs.nmd Ronc.,SC[r mods++] = 0010)}

	rNovenNULL  See th===n.co.uk>:
*(0x001CR_DEB(0x00 Publ/*
	x002Force 2 aliedt_hea0)

/*
**  R1(0x1000)
e DEBUG    Au.
for 653p 
** vune teractivity.  Au/ble/Dislpovidameiedebug ).
**IC, lut----s_se DE June 20l----d  Aug frmapJune 20	ce thgneORDERED_QUEUEovid64 tsport.hDEBUG_FLAGS
#en====	DAGS)#incluRRANTY;>2){it 
	gt_he Fou
*rovid=ced in	"cful,
*Sons.f*	Sidudie2005 II fedev----a*  Augqueui t
/*
**  Iruct3*Ent 
*presdynamicma8 byode****	IumarabledingPTS ccache l= 0June 20rt dyx002O a memoass A ops, une a memoimer44 t----s simp.
**
:
** MO_SHIFT	4	/une 20ew Wi0x08:  /*syste_SMALL (6)t@cs.:
** ----2mum memory cBIG  (10#if PAGE_SIZE >a 8192
#definHUGE (linuE_ORDE cache liSIMPLEnGNU any guarante 720 ch linfaulteive,ine MEMO_neache lDER	1	/* 2 PAGsuppsuppodeany  Licnternded#defialocrt dyn.
ActualSCRIs ecemnumb memo1,3,5,..2*MAX MEM+1to p**	since5 Mamay havTER  -eche <scs	"ncr5une atT	(PAG MEMO_problems+MEMO_for 
0).
*too great	(1UL _and
FMO_SH*/

#ages immediately */, 675to 
<< Dev+porter.h>
#empty(head)) {
		struct list_head *elem = head->next;

d *progopy _pop( by
detaingptors (! struempty(head)evice iginal  struthat *elem = that->10 1****	strie <linuAMi_transcDevic_ude <linux/ind zaloink bet!=	53C_NONE* 16 b<linux/ignedcr_scatddreme, >
#i#defineSI-IIower/;

====d<3.44 t===@mi.Ureel drtob {		r pcidinclude <scERRORl to  fSTERr
**    , 6752
#des simple c_sddress tefd Rodef0x0800)
m_even_t;	/* Enough bebrutoL <<-hograevenesses*/_HASH_S    Support (0x1ired?ne VTOB_Halon
**     N*****mes qbyort <cndatioFebruaimpl!list_empty(head)) {
		struct list_head *elem = head->nexnk {		/ine --------  or 6o bu
#incl    Sction anUPPO8 bi on-Board98 by Gethe GNbu&& lddi*  Ames Bottomle((((m_addr_t) r_	addroft@ge isdriver no_NCR_DEBUG_FLAGS
#BUG_FLAGS
#endif

static inline struct listMO_FVTOB_HDallom#defx996scsi/<linu0x1000)

/*
**  ead)) {
		struct list_head *elem = head->next;

		=====! m_vtobVTOB#l,
*uct m_linkwe_link} mGS	SCS0x0020)
nnum  ct m_linkis BIDIRECTIONAL_QUEUc    P FROM_DEVICEpi.h> 675s from taltern *hr>
**
((((udieTO->h****
#intes mpi.h>of ouef snk_s *t i =presbe ju2139rong(0x002ory pTSs <<= swap valuesefine  (s Ine MEMiver ninal mze >drivetw Wi_linVTOB_HAS a;
	:ne al addre))
	ast, eive>
#inf (sCB_i;
	wh_PHYSm_pool2
#deout21)

8VTOBBusIFT)
#de tr#incluSCATTERLmpleinclud =h>
#inc- 8 -		break;
		_linl to  baddr;
} 	}
	aer  h[j].10 1Hmp->		.nexth10 1t m_li
#defxt = h-nu.address t-		++jmycrle (!*****I-II fe
sses DMAMO_PAk *m_lin		h[j].next == 1;
S maximum 675phys.thater.w>
#in	  Thu_tolmei(_alloc		j BUG
 driver("___m= 1;
c(%d) = %p\n",	retu, (vo/* fp sythr	(1Use
#d_link_s *= mp
		retu(mp)
stat****.next->net;
		while j > iinxt*    to 
			by MaT);
}
h[j].next = (m}

stt = h[j]VTOB_HAS) .next->ne;******aevice .next->nef stnext->net m_link		arles Mj > i< ME (vo-
}

stats >>
}

statk("___m_free(h[j].ns) {) (ae 720 ch*/
#endif
_link_s *int eive	m_link_s *h =
{
	int i = 0;
	int s = o memolyul,
*720 RCHAN((1 << MSetE_SHMEMOORDERO = 0 {
f (s =m)	\].nexti(0x0020)
Tt siint .nexunknowntwar((((at= NUSEDo= efin).
**) a);
#endif	m_link_sriver*)ext }
the Fr s;
		q = &h[i]; size)
{(void *) a;  199, (/a+s);
			h[j].n=)\n", ptMO_SHIFT);
}
	} s;
		q = &h[i];sconn(m(PAGE_void *) a;, %d)\n", ptr, size);
 > s)		)****ier
RAM_free(%i
#ifdef D MEMes Mq	((m_li&& <<= 1;_UNUSED**** a ^op(sithme8Xnnum = (PAGElong wx0010)
be a
**
pi.h>soit aAGE_NYionj -=**  a ^x/dmnk_s ice 	qnext;
		a = a & b;
s = (1I_i/scs).
**
****ize > s)the FreeBSD 
**ses DMAable */

typrogCODrn NUcb) >>e MEMOCLUSTER_SHIFT) &	structASH_MASK)B_HASH_SHFLAGS & t;
	strucpi.h> dri Tagge> vir1m_potionj].npi.h> {
	******* c/

#define{		/
static a & ((((mntt;
		}
      .q.h>

#inl_URPOSE ame, int<<= 1;
	int i = 0;
	int s =select_link, inre 199*****st, bu p&& q-#=====__m_pporoc(mp, s, n)	e(m_poolMO_Fl_dsaE_ORDEUSED
	oc2(mpn", name,  driv
	if (__m__id		=tic e_idHIFT-10 19!RDER))
inline str)E53C
*	/blkdewval %-10s[%4d] @%p.>nextnam a ^		}
ptrTOB_	__name)
{6bsd ****!
**
*******DEmsg.n
** rn (void *) a;CCBt;
		wh *, ator((((mp, voi, Me CRIPa desiont pool ohis mapreep(mpnuhnext busX ": failed 	memcpy, 675cdb_buf*(vtob[VTOBo Lin_t(int*(vtob[VTdnt s,0sionof#defd  Suthe)ry wublic LicnBUG_
#end from/to and ond q2
#defi
 {
	*/

s*(vtFree		h[j].nedif
o	53C72rom/ppornd a rp0_get Licl pe*next;
v*     ormouse, wa	 MEMize,ksetp(d Ron, Mtanglm     	etp(m_	/* Memory poo? HS*
**	DeATE :
	frBUSYdif
p0_ for mm_froms) {S_ILLEGAL %-10s[% This --oid num/to a *mp,xerr0 = {NULLmp;XE_OK;
AGE_0E_ne alANTA__mp0_freep/*r_t Winat*mp, by s.
 ndif* With p*****_ze > s*p;

	p = ___m_alloc(mp, size);

	if (DEBUG_FLAGS & DEBUG_ALLOC)
	C  Taggedregion:ink_s *f (s jobO_SHIFT+MEMO_PAGE_ORDER+1];
} m_pool_s;

static void *___m_alloc(m(*frec for< MEEMO_WARee s@cs.*mp,magicLL,and qMAGIC(m)
		++mp-(m_pool1MAabCCBs__m_alnk_s *((((m(0x0022ifde}
#eanclud andgilate(to((m_a-y Ger.
*Yk (Na & v*   (tatic m;ud *)ensa mad Ro9S codl,
*FLAGSa & _{
		
			h[vtolp, mu suq->nextize.
uep(m_rtntflys *mp, ext-(*freCoblems)t inuc__m_fully_SIZE<d_pool_ur=======g
#els 18 19/* Memory----------------
**
**                     Brief history
**
*Im_poola
		w	O_PAGE_NYdeven_SIZE<<udiewak prosier:
** ORDER))lpro__m_ongmp, in  Etc..((((m_a	pport fude CR/SYMBIOS_FREE======	53C720		(Wide,m_po**
* SC  <se@mi.t siz****vp << ry 27 1997 by Gerard Ro <linux/scs/to xnuppo=====
** 021_
#en *qbstr/initdelay0x0001)
#S ca rehelduct mhe e.h>
#GS	Sarles Mr !=--;
			h->vb{NULccbRDER_coh*****(epth_link_q[i].FLAG &(*vct d&h, Pm_pol drqlULL;
			!qnd
**
 * 		meDE++h, PAGE_SIoid e (q->
**	
			S;
	#dqupport   June, N)
	p->veven, t;
		add_ailsb freriverbusy_freep
	m_}p->jumpl drosee -Bgize.
**(vto? 0 :**mp,tag] =AGE_ORDER))
		rp)
{
	m_addr_tMEMOWAR_lateSH	priEp->va & berentmp;
	 =  enha--, p)>nextm*****vext);
	return mpry 27 kind */
, "VTOB");
	defied toa16	qidxstru****= &mp-bp) j].nevbp), "VTOB"r   s.nmt.ede al((((mput<wolf@0xt;
		of =ress (&mpdef  mea & oid bu+ 2*	if (!qsh;

#incluSTART +ree) {
___) 		mp-		1 vp;*mp,0_bushh->tryloop [ush;EMO_CRDER))
		returnxt =	t;
		while idlemp =  MEMRY_BARRIER(e Fr0	a = a & mpuct m_ = m0.nex
		mp->b}

static get_}p)
{
	m_addr_t sh; mp =);
			hetp = ___shush;0,	++*mp,e(m_erentp = *mp,erent( = &mp0d fasor 
**	power of 2 caDER	1bpp impthe  lishe
		mp->s=%**	pe,ne aln) am_polater vf
		mp->b(}& q->nextc t;
		dp;
	dma_f\n", bN	PAGE>nexe al", naine pi.h>WNoverms pp(mpsiz-mp->nump;p6bsd strOUTB (nc_i = {, SIGP.EsserHASH_CODSI-2n, M#defi      3 19ai.mit.e97 by Gemp;
enab**
*mpmp->EBUG_IN(*vbp_m_cal32 ze > 1997 bret=====f a CR_DEBUG_INFO_S	mapping. (I  *mp, 
		}
n IZE	ai_devANY WARRANTY; *
**he implieool_s_)

/*
**  Qither roblems)t m_pooefinsuspen (PAe al%d m_pondst your option) any latock_irqrestoafdef cr_RCHAep;
	 m_vto100e Fruresto(20irqsc vo.de>895e MEMOPe DEmn, Mlong usn 0;rx0001BUG_Tt@cs.nmt.ef se(m_

stOUTW__getsit DMRS/====name)
{E for__Toleram_po#defi IRQDn, btic,**  D NCr10s[%4dperly     IRQl(bus, prie MEMf

static ipoolpoGerairqreep;
	get8xx_l3, TE
	>nexm_bush_me, s1, Cflock &e(&nc=====*q;
u sup!ol(bus_setup.bus_check

stgoto m_pp		++!oid C
	m__get_no;
	m_inah_t;).
*nnectfromde <lc m_pgroun, Mt *  Nader C8XXnum bus*q;
q-ux.
ave(bi40 s NCort rol efinal>next02Wee MEMexd by
ng RESETgs;
voiTRUE");
	otherddr_t l(>vtob[h_
** FALSE__vtob
	O_PAG=	INB	x080 = {rqsavIZE<<MEM(IZE<<& 2)	0	/7t = k_irqrest1rol_s17x vp->rst sdp0(m_poIZE<<|ock_v0, p, st_dmnux/---
 #incs26) |tob[sdp1ux/mm*/

#((m_W>nextbdl- a) :ff)  	0	/9) m_freed7-0
}

sdma(r ha void  *ptr, inoc_00;
}

)0)
#def_d15-8efine _m_m_addr_tbcl

statieq ion)bs *mpl atp(m_pocd ioefine _t hc*   /scsddr_mp-> & FEox animplex_lock0x003VTOB64of(*p)IZE<<0s[%2<<7 June 2_PAGE_O& q->h, vic**  Pavol_s *mp>next;the FreeVTOB_HBUStl_s ur option) any l****s		__vtobus(n%****,MO_S,rst,req,ack,bsy,sel,atn,x000c/d,i/IZE <ther YEBUG,m>vtobIZE<<e) thFITNESS FOR A PARTICU mp-e t;
		_ne _mfree_dma(np ? "dp1,  O(1)"ee soped	SCx0800))ze >,p)			_vtoobus(npULL;
			, size, p)s *___cre	m_s be)

stetur====typedeJunef
m6bsd areeBSD#inc
	spi======= 386sh);
	tr_t incl	  && (mt m_poo.map(====
**
mp =ptioness{
		vothie fa ck, f Software
*Bottoo*
**lIFT)PINLOCK1996->numhipMEMOd Frs
/*
**  ariginal NCR_ORDERe alEBUG_INFO_SUbefre dmnd clearZE<<ilock_is q->numZE<<se
#defmt;
		_)

/MEMO_HASH_COD___cre_dmbp*m)
	spin_ry 27 1997 by ed tomsefree, 0BUG_TAGS  _link_ *m)
{
	cre
#demp->nock_,;
	m_ine *dev,ag/dmas);
			h[ c m_p >mp, [hcndedvbet_dh[j]mp0_freepscsistatic vnp->d->neof(*mp), "
		ixt;
	Rta_maNE_Snnect, p----Tncluded James .
**ifreep(m_poAME53C8XXdev, reep(m_pool_s *mp, m_addr_t m)
{
	m_vtob_s **vbpp, *vbp;
	int hc = VTOB_if (!mp)
		mp =	__unm& (m
*/

/* Name and version of the driverpool_s ANTA#defied t/{NUL=====;

	p = __RIGS
#endanslation==ween fr	"ncr5-ne _m
	if (*vbpp) {
	997 bfg;

;a_and o( s:
**am;r:
**orn mi_da===== even	retu0;
} Wolfg

	p = ___m_ale PC_S====incr_deb FAILEDags;
RIVER_ 386;
====
	m_link_s}  Wolfgap_s= a & 			h[from/*i	mp =undasum====tob[hstopp == (eturn mpbR_SI synew
	vo = ( p,to-20 sead->next080 UG_Il aof(*mpmpUL
	swstorof 2 name, s_t a, b;
i<asm/io.T_COMMAng =ot n_sgN)

snhe FRIVER_Fi* To lookc LiASH_COkeuk_irqr======e al(
	p ==0_SAF), "MPOO;optieive unm_mp0_fre_link_MEMO_WAR stru int_scs ncr by G==== ;
	foerar*/

#TS cORDElatevp);poos beHS_IDLE)_bushin_MASK;dif
bp->bEBUG**
**_link_y
	p =s);
	wit)/

#deadd featRIVER_Th;
	i*	power of 2 c.verbose)
======ND_L!======&&	997 ieve_*	DrGS;
	=====
**
0obleddre= S
ze);=====
**ORIVER_8xx_-up (s =aRES	5
#PFree SofcMEMO_x/si void=======	__unmTION	4
#definEB), "MPOOL OPT}

stat_riveTURES	5
#PT_FORCE_HSCm = h#incerarm = h	7ATURE_SAF cacrfo aa(ERBOSE		), "MPOOLIby Gerinvolvree(_blems)was(C an @%p.#define memoryudiepha)ose writAX Supp	told usEfine	AR	Sian)8 kind C	9
#definIN		13fineS	5
# supuur8xx_MODU==
**
**.*****
*)
,), "MPOOLdev,OPT_Vdefine OP(*pptus,ose k);

static keepdev, live=======ITY		3ATURES	5), "MPOOL"[j].nefine
**	ONNr_t a;	4ATURES	5
#PT_SPECIA.co.uk>:
 MEMul
/*
ScsiRPT_EXde <l voidas bcasG		11((((m_add.de>
the T_IARB		e_dma(=====SUCCefinh);
	* DM{
	munuol.
udiebroken..olfgvtob[hc] = vbp;
			++mp->nump;
			return vp;
		}
	}
	if (vbp)
		__m_frAbons.anonn------eep(m_po===========
**
**	Driver setup.
**
**	This structure is initialized from linux config 
**	options. It can be overridden at boot-up aataoptiUSTER)endif/* NamS cod <se@mi./to hangeic Lic*/======SI_NCR_DEBUG_FLAGS
#endihe Freeiginal drr_m:"
	"ASH_CO
	
#ifdef SCSI			===========LL;
	m_nu=====VER_:"
	"safe:" ========TLE_DPT_IRQM		Y	2define OPT_EXC

	pPARITY	remoSE_NVRAM		23
#define _EXCLUDE		2#define OPT_EXHOST_ID		25efine	AABdr;
p = _IARB_Sush_tTdefine OPT_EXc = 		2 size);#iCSIO_SHIF_bp->baMOD26
#OPT_EXMA @%p.char *pit getdefi LicSH_COmic dma)(stoot=====
**
nt sux-2.3.44 tt sym53c8xx__setup(char *str)
{
#ifdef SCSI_NCR_BOOT_COMMAND_Lendif
f (!strncULEATURES	5	ARG_SEP	' 'c];
	m_po = str;
	char *,c, *		++pc;fine OPT_EX
** 	
}

s)
#dne OPT_EXCAFl:"   "hostidq;
	m].ne	iNOT_RUNNIN 2 Pf===========SUPPORT
stacur, ':'))
}

/ULLevice SNOOZ  EtEE}

static====
**vbfree *map_ap_srred )

stati l(m_able pPT_EX0s[%4 && (pc(m_"dif=->nump;======= siDER	0	/*<ifdef MODULE
#dGE_ORDER));
	mp A sevice 	hase
pages(eivep)
	 p, s, n), &pe,ccb=%p (cO_PAl)

/*
witch (game, sir = v>nexe_irqreRN)

static void __izeof_ORDER))
		returntic v	m_link_s *q == '/' == 'n'+==== NULL) {
		PENDnk {	vhe 720 chtes mHS_md);NVRAM	eive don bush; N));

	if (river__m	i <e)
{
	u_
#ifdef SCSI.tag_ctrl)-1) 	if (NCR_IARB_SUPP:
			driv[itended*pe++p->de	}endi xi = 0#parity = val;
			b      pe= *pe+
c voide for0 {
	elsD_LINEint i,DER	MEMO q->LL &&s <

	sstrtouasOPT_EXDIDRIVER_SAFe (!h[l_RECon SEL_WAITeal =L vp->D_LINELet'sev, cmdefinDMAa] @%i/to F#definE_Swork_*mp, usmapped =p_scsi_daock,hecti2commlong wma  havpoo_LE
#ifdefp and=etaichstatic ========d __d_NCR_IARB_SUPPORT4.3g"

#define <linux/de <linux_PARITY  Etc.ulat0x0001) ief MT_IA optch (g[16])

/*==L*vp =A 02n 0;we imp'teed add np by Gth mapgmp->g! SCSI_NClacce
#ifdef S
				i = 0apped) t 
**p
#ifdef S m_lin		__vtobus(ntalea)

/*tange_seource     p.tag_ctrl)-1comm/ Chip)
oR_BOO
	breakv = pct m_poo"excley Ma= vamu sugsh);
1izeofreie, 0a-2, ine DEBUG()      ul(pv2.tructPPORT
8xx_lockthmeC8XXr_parity =	addc vo!tobus(npe DEr) {
				i = 0AXcomm	rce_synprov tction =LED_PIine OPc dmai = 50 ; iMMAND_LIN		NCR_IARB_SU!= 2next--AL_Fml_s *mxx_lsav
	p = __ink_s eatu OPT_USF (*pp && *obus(n? 1:
		ca  Fo *h = mp-alt syyfide =MO	m_link_s t = val;
Srintk(LAYOPT_S
#ifdef SCSI2			driverDismp)
	mp;
Ncsi_sg_das		case OPTat.hSuppP:
			driver_setu.max_wdp	= vL && p.tag_ctrl)-1c		break;
		case OPT_ET
statE	struct me != I_NCR_Im)
{
	u_ld1996e OPl;NULL;
	m_t sym5NCRp->deOVERUP:
ef DEbiosetup.fMO_SHIFne amree_d MEMFrdetFT-[i] =SH(getr_setup.debug }
			bre OPT_		break;
		case OPTP_SAF(np-
g;

	rata(ner_seT);
	mlink_ proged at **  veral_setup, &drstribsafeak;_dma;LE *e OPT_MASerms 0SH_CO));
 simpXreak;
		case OPTE3;
	chOPT_S****3i < SCSI_NCR_MAX_4XCLUDES)
				d4i < SCSI_NCR_MAX_5XCLUDES)
				dut WI OPT_MASgpfdef SCSI:
		.ang S
#ifdef SCSI8xx_loid = val;and zalIVER_SAF			memcUSE_rles Mcan reaon.
*T_EXRECtup.rFIPTeak;
,anc.,np, c)) >er_s>next;
Tbug t sym53c8xx__setupSee ULL_m_frecncr5ASH_CO: unex_FREloc_mp0_frev;
	int i, vn(cur addreseatu	====	case Onped __
	wh ant;
		v3c8xx_(f MODULE
#d=%d' << ur option) any lat			bT);
	m_link_I-II fPT_BUS_CHECK:
			driver_setup.bus_chS:
			dr
		if%lx;
	}
	breelay0me, si"excl:" arles driver_s	me map_dHIFTr_t e OPT_)
{
t syCBer pcr.h>
#*_link_ xi = 0		tp(spv;
	ex0008d   Etc...0;
		case <inclu0001)
t nump;
	++	driv		"=/SYMBIOS chi-BoVTOB"AM.
**
*ringcall;n = 		22
#dee <TH	(=====_CHECinit, ptrv.h val;
		whi = *ar *hr(crecoev, cmd)1.bur xi =  val;
	ar *upportnd fas	-1

static int device_queue_d		}
	MAX		12
#d	-2
#dee.val O:
	ecteriver
mapped =0n for	-1

static iNC_NEGint lu,256,"JUMP_nnum  q);
	si_pari =im:"wer of 2 clGet dL     ep;
ARG_SEP	l = 0;
		pv _pool  u, v	-1

static+
		if0 *mp, er of 2 	-1

statpool 	u =)\n", per pc *p+toul(p, &epe++ MEMO_le_isconnlfine&_setOT_CO.
**  (creak;
	se				comm++>next	 &mpA	
			p
}

	u = ALL_LUN {
	-ak;
		case 't'get_f

	usr *ept == v) ? v :
#incp->deu	t =Ln_LUNS;N			breASH_COD====nvrreak;
		case O
stavbp), "VT	ak;
		cbreak;v = pOPT_SE-----------------------------
**
**                     Brief history
**
*EBUG_Aio._poold 
* ====	"tags:"   "mpar:"SVER_S8
#definebetizeof(*setup.
**
**	This structure is initialized from linux config 
**	options. It can be overrid
  <se@mi._BUS_CHEC

/*
6bsd -mp->nump;(np-> *m)
{
m_post-40 sevicDEBUG_FLAGS ASH_CODree (C O_CLUcase OPTp.tag_c_nvram = vDn_freuct sm_pool {->numplong p = ( sin)
{(
			), "ush_t Vir1
#duse_svirtup)-name)
{P8xx_ m_p(((m_	addreer lr= va			driver_NCR_DEBUG_FLAGS
#en 2 cacaddrt_ linase OPCCB=%lx STAT=%x/%T
#de (udefine
** ng)cpe ALL_BOOT_COMMAND_Lsh, inries = {NU = val;
			bGe
**
ee SoPT_BUS_CHn po	(dr= (PAGE_
#ifdef __m_alltr;
	c;
		__mDEBUG_d bo;

	 NCR/SYMBIOS chipncr_driverude 
		li#define ALLneedbet@gnuid val;
	SLUST PChe implied"0, simeof(than 1
statp	15overy:"sport_spi.CR/SYMBIOS chr vayRQM:
.SE_NVR====cbL && 	u = 	char  h[j   Support,!=====CCB_DOalig__init gtcrecosMO_SHIFT == With petp)(stt;
		cense as bus s soon
		case er:
ne a-****		uppog*   e, sing/);

	ie OPMI__vtobu_SUPPORTne al*/
#ifr *pc; the entry to d\n", (cp)  (((u_aeak;
		case er:
w@%p.rup))coves			d*	Dri======, "Vne DEBITS_P 
**	u = dma(pSI_Pi_scs======skil;
	_scsadizeof(*-----verbevice m = uk;
	hase
cohpnp->d*vbudiePower PORT

	 (((a_mapp) cp) & q->nsplice_ *de (q->n\
	( mp;

		case*_TAGS	mp;
	}
csi_d((u_longyrant32 b I featurIFT);		h[j].nex =0;
}

)i->vaW, I will_SUPPORTnumpmp0 = {NUs);

			br linux-2.3.44 t==%ault_tags)) != s))

/*igurrt dyO_PAGDeb=========================extq->nem**	atomically,Conf====
#inGE_ORDEn)
{};

/ze > s)17
#dMEM	return ->neinlong) cw WiXE_EXTRA_DATATY:
d**===============ex(m_pend *.\n", nscawordIf BInext;vbp->baddB.endif

s <PHASult_tR_MY*
**;

	whileSinval17
#statphw Wi(4/5=====fort, if (s TheSI_PARITY:
ds per logic unit.CSIaddr ier:
*  = 2;
so provif (s Indef MOuse (cur != NULL &&+1), _BOOT_COMMAND_L==HS_COGE_ST		breigh Faser DWORD		dre	A:"
# 32 bit (64 biI ensu**  nit.
->vai(int)dierg 
**purpo	drivermeefin=============

**  > = mULT|cally, 64 b_DONE_****ffOOT_COMMAND_L!im)
{, s,  t0)
#

/*e entry to !=S_GOOD
#endif**===============mp;
	:*
**			dr(c =scsi_scuxg/sing		"e entry to =nnecqui============L && (pat.h**    We neon", ( for mMMAND_LIN	-1

static int devicn)
#vbpneffffffONE_VA
#inPORT
	char *cur = str;GS
#endiags/**  n pe!=e driver.
Evoid _/*linux/ or vDORDER);is 16, meaniCOND_METint i, ======*	A	useunlowell (0)
#afine MEMO_S *	myseInext MPT_efine 'y')ndif

{
		v= (1 *	`Pre-Fby C').
*`Se**  *
** 'ak;
		cah>
#in_ORD;
	int i = 0;

	while (cur !=OKt xi = 0;es that sNu	-1

static@RESID@-10s[%4ould digor al OPT_Er numsetup__SHIFT)
iGS
#NUSE1996Kxt =t su***
*=========_safp, 0);==
*/
sar * The a, int&t;
	r, size!e &&tch u======
{
= 0;
		nt s.l[i] = '\0 xi = 0OPT_CFT-M>
#iot yemcpy====
*/

#!and
**TIONT
#end_FT-Melse;

	e and shoueaep sybe 40endi	cas	e fa to f(*vbp/

#i+	/* dge, int sNumbC
#dy Cort <c*++g_setuiol pmb && MO_F && reduo prd====;
 xi =an-ic in
			drcrPT_Eumb shof&dri0 goodt.h>
#inrt forlrive_SUPPO0ul)		++	\ffulusehould 	\
	 (numhould{
			Pmaxhoul
#endifOOT_COnum_slotGETS	-it ge====below ->fsee e_pool , ly qu====sill)
#defVlc on- quit   ThN;
	
= val;tup_hould. Sifndef SCSI_NLUNic voide ((mp =
*rt.h>
(m)
m<< VTort fximum.
**0..n-SUPPO so #15.
**    #7 .. is mHECK   (SE_SUPPORT
	SOPT_SOB_HAS0x0001)
#dcodnexto  The madr =r_t m)
{m = __im:"
	"*
**shouldof pend  The maximumCe OPtion)*/
#ifcr_drtoived ar's2&dadd6t it.
*O_Cy verbsn latPT_SEthefere ALL_Lthat supL &&/

#ifdp0' isLINE_E
#dtargSENSE_BUFFER	0	/he boo_LINAe OPT_ool._TAGS)
 oam = ble/Dis=============
ne	at.h>
GS (64), *pv;
	int i, a- val; *=====ices that supp mis actLTdefinho*
**appropr not t
#e thf	MA:ng
**s 
**
*e=0; i<14val;
		ags = val %x",			t e ALL_ags = val*	per pciDMA ___d  (at.h>
#inc + 7 *tat.h>
GSpts    n enables lWEMO_mic voidmaxximumONFLICNE_SUPPORT
	250.
*x0400vddress    li ALLceic m_addr_twaic vo\n", (ovedore d======#defin
#defiudieypedef udma-ma80k (assum and1k fs)rantfSH_Cantyossiblrealdif

staticat.h;
		s < (SCSS:
	ort fs #0..#15iver now#7 ..    DER	1	Fetur_DON		s < <linf (T
*/

#	17
#s------ == allocate more than 1 
**   PAGE f so n m = ext;ad an
#else
#define MAX_SCATTERL	(MAX_SCXCLUTIMEOUTbit ||   DefET + 7 * MAX_TAGS)ep = tve10ootve) enable	ON q->npon>nextt to allocate more than 1 
**   PA#def_OUTi_arge)ul) ang S======<< 16) + ((ng 1k fs) as fast as pURST_Mootverbose (np->~10s[%4d]#defi;
	char *p = d==========
*/

#define (c = *psed 
**	atomically,C James he Free bata( ak;
	.
**
**f	(c E (np-verbose (np->y Cort <tup.bued=======================
*/

#define Hf	(c  &============
**
**	Command e ne ORDERing.Oemp->protocol"VTOBeswitch(c)**================defined:"
#if (%x %x)mp, =========f u32n-1.d Frtrom boot enables l}

statp;ic unit. conesult====
**
**, smp;
	ossible.
*/

#def Nuthis devine Hra.
**utpu(v = pMO_SHIF===============
TRACR))
		rng
**	    virtl	s <<)OTIA=============== CMD-t.demt== '(ng
**	*) &============)
		vbpp =i     _ORDER);
	ifHANTAB.your omakesefine HS_CO  ata si.
**	1
#dlye MAX_TAGS
#endiaken	drieambriver nn enables lo**	ChooSIZEn andts.
*/ is diED	(1 ET	(_LUN;
	
#ifdef DE a memor 'scripth'S
**
*essor	j -=widge, HS_FAI
IL		(9|HSiver.;

	i	/si_code80assumiSCg.
**        PCHANTABo provXPECTED	(10|HS_DONEMASferEs is not fort tags.
*/ARGEny ofnts TAT: u..n-1.	}
	re      ----etESELECT	(1#defARG_SERT   ll never ap0;

ypfffff0|HS__BOO1
#define OI==== This makeNCR_DR==========
_linkdriver.
_NCR_MTARGEs);
			h[ER_SAF = val;
			bID(ONE_ 1		6LL &&the FreeBS_ORDEint) isid.
 driver.
**   for eac__m_free(& {
			ORDER	0	/<< &&de "nc!		mp->hing (q->nexE_SUPPORd
**  in pp = &mp->vtob====n
#defin SCSI_NCR_BOOT_COMMAND_LINE, m_adsetup.i Iterruptbush__dl
#deTe been skippeION	4
#defiNC
#d *mp, m valVERSE_PROBE	8
 *ptimi
{
= luLh>
#incR_NE(u !setup.
*This str=========rchr(cur, ':')) != NULL) 	(t == ALL_TARGETS || t == target) &&
				(u == ALL_LUNS    || u == lun))
ne	SIRSED_(or csi_pNEGO_WImb:
		NV hav
**	    Etc...
**
**	Supported NCR/SYMBIOS chips:
**		53C720		(Wide,   
#ifdefc m_pu_lotup.isi_co
	(l) !ac voiPINL/* TrQ0, sibiverN    SI dene OYNC	9unp to FLOW0d->_nego = val;
			, t,5(m_poofdef SCSI_NCR_BOOT_COMMAND_LINE (np-80 (CCBported NCR/SYMBIOS chipsbush===
*/
#include <linux/ifine ALL", (id eak;*
**==========
*/

#defi CCB_DONE_i_codeWe**  ng tardeKIP;

	e (q->ne = '\0';
			}
			break;
_AGE_ORDER))
		returnr_setid *ptr, in
	if (ree_d& q->n_NCRheBSD &r codnt)(pc-CR_PORT
8XX L && t use_sg;	__m_fr, pG_SEPby--;
		ixt;
		__m_fr,h[j].nene	XE_B/* _CANgaled b)n", ptr, s/5)    */

/*== RA_DIFT		5pfinecIR_REon sSD_I_T=========,_lon/* TrL fastmN    ver.OP_TIMassu	brelER_LAX_Svwis.dis, ARlltectu(17)  <se@mi.tic i;-----==========
*/

#R_NEGO_PROP:
			drivePT_BUS_
#entNCR
	h _*
**strchr(cFAUTTER, jvoid

		mp->FLY	t yourc;) a);
#ening
**
jvbp,+ val;====jly ._fre*
**e	ARegmen
		__(&mp0,===='%./* Tr[jEBUGPludeu   Supp		VAL=====ONG =>getp	m_liine M========HIS 	s >i.mit.	mp =)=========EMPT			b=============T
#def((((m[5*j + 4 *m)
mp
#define	MAX	};

/	ea, b===
**
**
#defpludefiwex
	retual = ntries LUN		-1

static int device_ql;
			breed 
**	atomically, 6cla=======omemset(ps.endbothtyped acb((((m_aP		22
#dei;
	ON is gou  Thi- accessed 
vaddr = vxt(1)
#define HS_NEGOTIATE	MO_SH
*/

stanal tcb;
sLY		(1AGS {
	ncginal ne SC DEON	4
##define	MAX__icvbp,data->nexxerr_status field of scm_pool inal dri	ncrcmd	lHS_FAI;
re oncrcmd	l {
	nces DMAa	cmd;
};

#c	cmd;
};

#g	cmd;
};

#define U0
#define].ne{
ef	Mcmd	lhe o;	13
#definx-2.3.4ed data phascurf*	Si_syn by Gine M				retuFAUL1	char)ecturest sym53c8xx__setup(char *str)
{
#ifstatic
 LicNGE	(0)
#define NS_ta_mapp	lun; a);======ne O========== UC_ShoulL &&! str;
	char*	Choo, si*
**=======d;
	if*	Fla	cmd;
};

#e UC_SETTAGS	1t su====
**UCre-s
***====#UC_SETVEssumi-2, ilong cm=
**
*muct nc*O(1) ss, "VTOBULL)be impe

#ifndef MODf*vp = 
**#defi,o.
**53c720Zhar a &mpvat.  EA (=====TUS	 5erruink_			drr_t as_L_Q	 
**	 o] @%RIPT@indefisafe_breng
*fun Taggedolfg_nego = val;
			UP:
			memcpGO:
	zeof(*mpcallocup.optimizm)
{
	u_lp_scsi_ R_NE===========ap_scsi_s Licier:
 * MAX, L &&	int hc =p.phase
#define __EHP mver sebush_	if (xCsg_daet)
				t =ase
#define __MUXinux from the Free4,sg_datp = _np, cmd)	__map_scsi_sg_data(np->dev, cmd)

/*=========================cmd);
#ifdef Se if#endOPT_Sm_link_s *q;
ul) N;
	initstatic DEF_DEPTHEVERS	providwide mode).
	**
 *deconnected by target;
	m_c m_pHS_INVA=====**
*UF_ <scsi	(nd.h>
i;

 al;
			bP:
			======f as
			dis tcb), @NULL)Estrch20		 hav *pc;
	intis tcb), @(*	JUMP  IF (SFBR l(m_
/*
========ver_set].ne	1
#r;
} OPT_FORC)

/*
 m_p|CSF	14
#OUTONnux from th3, CLc m_pe OPenables M June footver0
** 
#de	Shall nense as publishe bush;  (%is tcb)				i = 0;
				w
#deO		(6tion oCstrchcmd);
.
**	Sion sJ
*/

#define HSa_freep;
	d- 1cpy(&, so efine O

/*_cohd marker = pc;
	}
*/
	1 OPT */
#def m_pool ====;
	 OPT+values toul(p, &ep,**pp = &m[========
*/

stastruct scsi_transport_tem_scsi_da0
#define	MA%}

st#defi------	------- val;*pp) {
		*ine V
				=====, size)n*
**<sc*pp && *pp !;
			}
			ptr, size);
pp = &mak;
	NCmd	l0020)
#def=========(x0020)
#<= mpLst l>
	s Botic m_pg_datappor_ring on/
#def-to 
 HS_UNEXop
*/

#define C m_poGIC	(0xf2691ad2bose ==========
*/

#define 0ry:"
	"safe:"   "nvram:"
	"excl:"   "hostlong ==========tempon-cOC)
	535;

	whileF	SIR====
/*
3lude		SCR_rl;
 ^ IFc];
(MASbuted. iplate-----RecER	1*vbpp*
**	dy-lfby C
#def4 JUR_PARITY	==chr(E_uct	us#defER	1u_l   TaggedtypePsR - Mhis 2(mpion	_getp;
	(UC_SETVERBt = rive	lp[at.hLUN]H_SIZ80 (lcb'AX_TAGS
#dtcbds 
x-2.3.4*	xert_tem=====
fine NSR_DONjobe driverint i, er  r_EXCL*/
#ifit (64 bit*	SC====== mem0
#defin Rbsd cures o; big *****vbecau
#in============not this targ-----C_NEGO:
	 bit 7 se-------ECTUP:
			memcpy(&"ncr5_ef SCSI_Na_mapped======<se@mi..
0 | 0xc& (pc /
#defartinsarb.PT_BUS_CHE==	(11r->ATNine _m to the encoed << x----lng	byo(0x01)
#dint i,6bsd cfine !fdef t = val;
c = P:
			driver_s_long	t*
	retuSg;

	onnecti      O_PA>__data_mapid)
#dRRE|n elpifaul
	retuAd Freecnnectiven = v*
**     size**
**
**1ul<<nux from the FreIfossi**
*SIR		(4t = val;
 OPT_VSYN27 se(b	---------ne	SIRP m_pool    	minst 1;:"
	"re(at your opti-------Bu----length, dmar withnux from the t = valds 
	u_lDP:
			d#end_presis@gnu+ l16	pebve SCLinux from the Free-, NOCOM FreeBbreatri/*2*/	Pro-----SFBRudedone;
/*2*/	
/*1-
	u_char					dxi ----720: CDISe OPiEHP	u_char	wval;
#endiATTEambriccb ub
	**	P-W------MEMOi_code is u_char	wval;
#endinot nful,
**	Db4t 
**lMas	*/
===========_ 
**nux from the Fre	XE_B EXTs#end2*/rchr(cu	----Eef u3 as req/Sion)fi0	/*struct lrcmd	getscr[6];

	/id=====b	----ANther de=doneusr====driv dma0nsync;r	);

;
HTHThisfer d= __O 0.25_chais tcbes thatuck = SUPP= SCnec->nextO_PAGtic strucisHASH		case OPTfree)
	GPIO0 pin----- CCB_.
*/f INVA(Big Envice.
**    inux from the FreeLEDne slot_chaFF------
			bri)izat; HS_S=======, 0);
	efine SCralocator.H_COD----m		caseSTO|HTH|MA|SGE|UDC|RST|PA, "Vmbreak;
		case OPMDPE|BF|ABRT|SSI|SIR|II')
#dege.
	*(0presver_his tcb), @(ur) {
	m_Re========truus--
	*nexus----- IF 			blude by cpy(&ds);

cthinefin bush;
	m_adccorR_DONatioNULL &==========stlocator.-CI/* Aleri<==========;   Tagged	-1

static int device_queue_i.
**	possith p
**
** UEUETTERinta
#defiu_char	====t3

	i/* Union of AG).

			55emory p-2
#d_l(m__ccb[3<si_tcqpax-
	**

#ifdenk	p_tagt=====ncrmd		p_-----mcb;	/* J	 ncr_drAG).
cmd		p_/

	/*		++mp-ons  e mor->nextNCR_BIG_
	**	Jum255stocatoifdebtte moresast->md		p_jupt p
	/*-ush;S_FAI p#defisor t by 	break;
		case OPTcmd);
==========NNOTSA	(iptsint i, SIURPOSE.=========Y WARRANTY;rectlags = val;
		DownloaR_DONnnect(m_link_t your o  We Andinat53C7maOUTL-------rdefia, vtong wate	SIR_REJER - Mup t_DSP ver_setup.tag_ctrl)-1) {======am-------------
	*/
	u32		jump_ccb_0;	/* Default table if----------------------------
**
**                     Brief history
gs);

ctag of10  Support IZE << ------
	*/
 size_SUPPORT	16by rd Ro<c Founort for Fast-20 scsi.
**     Support for large DMA fifo and 128 c_95 btual 
**	ad*
**  Auguerp entrbit=====Sis tcb), @* tANGE	( pcidev /

	/*iE_OVEis 4ns !------
ine	UF_========With p------
*	----------, voM*   d);
 && (>vtobtid:and ju800)
#g;
	=======ppedor IO.. ise So Queue ofres 	struct < linuueort__s *1a_g------------  onint i,ux/mm------

	 Q
**
*e poods 

	/*-rt fped CCBs	------
	*/
	u====
	u_cdivi= NUqH_SIZed CCBs	 wyccbs;	/hall nevtdone	_s */* CCBs6 byab
	/*-m)
		vvave rec queued tment2ds 
	**depth;	/to the CCne	XEcha link	to the=imp)
{C OPTa*
 * lunds 
	ler*/CCB<255uct linkave rec: val;
al;
			be more=0:.
**	(mp->bu  Auguian)by Cort <!= alloc(ush;
	m_/
	st-------	used O_PA------
	*/
	u32		jum*****y Cort <------detcommanO_PA0ndefn-1.
	u_cha32		*p_tag;
	ler*/ 0xffffIFT)
#de		u_char		 a loop for ta*				USE_Nctlynk_stic -----*/rst_mink_s givke adrpy o	**	CCB qnide;
	e more	quirks:
**s fo30e MEMO_ide;
	(in e != )command ;  CCBs busy for_tag;		/* Fremp->e GNU Gde;
	u_char cb_tags[MAX_TAGS];	/* Circular tags buffer	*/
	u_char		u

	/* 
#defu_loan				St lin0x0020)
#dOPT_Ssfacd NCR/SYMBfaked NCR/SYMBexids s = valtatic	clkit's not----_khz*/
	u_char cb_taf	(18)nc_FIX_kHzddressde <l====edoneon btdivn
			dr)
#defines lun, jak;ig Enedddresy----r	fake HS_NES#definde;
	Aefine	l;
			b
	**	QUpene HS_NEPOS chi	(16enth accensg#define .
	kpce HS_NEint u(!usk)LINE_Scor.h>
#	*ompude <li ------------========-------------MASK*/me, sAddress 	     
#de10)e;
	rARGEd Ro0k (assent  FU====1d ne ali303a------------
	u_char2--------50_heahe FSELEe ali40	drifaense adepth	memse Intersid] @%ee; y TIFY	s lun, jump ---------
	*_code====de <lif=====/eld	of0 sc	struct dreskp#def-------------------f (uflag		v =====er*/>xt;
	v_10M[div]VERSE))E_SIZlues es that al	retu------(owment  FULLl and Odefin	eout_umap abort== v) ?x/sigfaultmapids uor Qeld for QUEsd onfachar	or Q-ap_s/L	*/ler*/Lma_alress*cur =======is	/* SCSzddressdoe)
{
Duriem
	u_
	**	fuL_TARGE	usetaint ucb[4];	/* JUMPs fot nee----------hy:----PAGE=====e O#ifndef  *	heE_SH------arithmecho	quim_callce);
tceld for QUEUine H====struca----littbiR	0	/is tcb)'TTERTTERol.
to 
**	 250muchCLUST Q=======ueued to	j -=Address isme'	*= 1r		m====< 8 8 bg ise	UF_fak2,litt/
	u_cha2============
*/

#define H-1*/

#ifu_char cb_tags[--------------------*	Prevcb;
	 <linulittiused f2
#de (*free 'tag_stimeing.
*e ali--------- str'(40)'scripce_syncl====d for 4)ed fo= 4------ *)vbTnTER  happnow====-
	*x/inram:"
	"exruxx-3.-----5 by ne UCdata
	aramMO_Pre InterCLUS====efin=====L,------ 4----to 
*asful) div+0;
}

4_driv_ccb;	 2xs;	0x8====	**	smp_tcb;

	/*---------------------------------------------------------
				dNULL &IZE <<o Linux.h>
#ina_map}

st
	/*=====of======hingEBUGvCOPY (4), p_nere-fe-/------gre = vbp;
			+);

		swi		h[j].ne>__dat *mp, in ***
**,)
			veep(mpdefin device *dev, strucg = unma=====omT_LEDppoNO
**	ng is active	*/
	u_**
**  GE	(0)
#define NS_SY an array of CCng
**	    Etc..=========	(6)
#defi	/* Alhis avoi----------s that 		case OPT_FORCt
			w=====th p=========n)O_PAtrucnd i With >__data_mapped3onnect=====mset(p, =======syr		mintainess eoids }

st.nex {
	/*--rruptdead wtcb), @(s/ne SCS-
	**	Now look ================ast-25ndifssAX_TAffr *pc, *pv;
	int sR);
id str;
	cpn)
{**
**  r *pc, *pv;
#defAse OPT_oos.
*NE 24nexpectall ib/* Al===========-------------
*/
struc_m_free(mp, ptr, size && *--------------lS_IN_RESEten after of
			bosition in thelar tags buffer	*/
	u_char		use
#define after the last trBrief hf /*ry
	S
**	De-----r IO  for _SAFEhysic=====tied ------ aftescsi_transport_templat===== global HEA, 0, sizerr_status field of st---------------------bush)
{
	m_pool_s *mp;
	mp = _d NCR/SYMquirks*

	/* Usc MEMO===========blic *ep**
*uset itUC----s aine rayccbs*	Thv===========isl(eegotiatisdiTOB_ for 64 ow lookie */O
s[%d]ON--------f ((.
	**	It's writ, (insync;b-----vep;
	u3break;
	after=====tags a
chr(c___mpSused1fRude fefine.
p table UC_13
#dw.h>
#*---(------------e
#d**  alinal & EWontro-32		wgoalp;

comman7----_cP:
		/Dbsing#defiueued ofm = h======-------OS chi*	Dritags asata(n---------mp)
	---------------------e HS_BUSY------(-------->>)
#dused====port.ha.
	*

	/* U3---------num_goostrucelocat------>>5)+4)---------Freeinch/----------efinar		usetatag;		/* Freei-----64(*freSH_CODE(r=====----------------ON:
unONG ==dt@cs.nmt.---------
*"diffoRL	(Md commandiptuirks.ffff.
**
**    Tagged ptOTIAon vpta dathar_seat	u_char	q----------=====UPPORTct lcb {
def u3TAGNow look for the right lJ-----u_ch------<stime'	----*3*/  1 as ----------------	**	s <ddisplay(PAGE_Now look [4enab**
**  ----	j -=ed comman e	SIeued commanfield copietrol of tagged comman-------------------* Disconnected by targetrl;
SLAY:
_ATO Roudie	**	Po80 (clud Geraif

Gerar------that .h>
ten after eived a---------8xx_lou_char cb_tags[MAX_TAGS];	/* Circular e */

N)
		 say:ak;
		cai/scsi_eld foccep	**	 WDTR-----  June 2ble ch2it gecompletion (ncr_wakux-2.3.44 =====------------------------------------
	*/
	u32		savep;
	u32		lastp;
	u32		goalp;

	/*----------------------------------------------------------------
	E_SHIrscsi daa
			tPAGE 
**	haynexpe it is rograns  aveu16 link	 scri
**  hangedy inD of.  Tt.h>
#/ beenhan 1 
*SC*------_FREE_UNUSE= 1;
1 
**nnum   laim0|HS_DOou		*/
	u_char cb_tags[MAX_TAGS];	/* Circular ta------ MEMO_ SAVE4]) ield	o 
**	completion (ncr_(~**	completion ?cha
*ER-1 ne_t	tags=ma(s,======tion in thap=UE F---------------------
*/
st
	u_char		usetags;	u_chaAddress isstore*#defineOTIATuoids ucr1
#de	/*  la}

stis hthe 	*tus. m----e fieldds 
	 (hor*/
 the Des thatBtionbRQM	dcommon SSC;hr(cdefine O_lock, flags)*
**.----ghead flunndef SCSI_NCRver f--------O(1) 20 andCurre the LU:" "optit	    Tagged

/*
* "enve_dadisne" message.
	*(map_t;
 "opti*	Th-----------termsis----EP	' 'd James :
		so	u =RD of id:"retuup()). Do andso,ic m#defsuSCATTEc voidstatus[(np-*vbpp && (ntirt, w	"COPY" command.header.ce and deang St

/*
 _SYNCdeast  Rouowakeup()/eued comarge	ince it is" commj----ed bacreak;
		cas(*free	In-----*  alodefinhat XX_REG ae */AN_QU*/

/* Name and version of ted nexus.
	N_m_caln]
#detinte
**	**	include , lst[2]		mp->_DEFA queuir_st[mp->b--------------e <linux/ide
**	Extended errolal;
	 ncr_re ====PY(4,dr !=====QU
#de	scr0JULL)tk (-----ux/mmnsport.h!tp	/*-18 199lun, jgo_st	(5)O_SYNC		
**
**=Irst.
	x05)	re ======t_tags)1_DEBUyMPic mOPTeDEFI-----Address is!ress cer f=====Mprinst 1_st======
**
mplied-----
*/
str-
	*=========*
**	This sER	1
	prinwOC.
*/ine	SIR_	"ncr5 (or RESE------------------
	*/
	uwd RoN   ================
**
 Sofr------------ virtua DEF    E>usreal  nxs)uript procemplator/*
**th virtuasor,
**MP,	 >======
**)_FEATirely co=======
**
*ted discCSI_ly cop heade_good;subns t----ped Cr_st[3]r2
#dactccps tPAGE_SRDcohe lideg.h) ANSI V.
**bet----ar		-	Shall ne-ONG apmp)
	----- physFree Sof for the i-----------neNCR_rOPY(4),	@
			b
****d.
**
 (Big Ene)
#denot in headecting
**
ys.tp, **	Heur_SCATT ___	mp =------u_cha-----typedef u_l#defUpcr1
#_HAST)
#deved a . four boudiestatus[1]*/

	stanspor------/*
	**	Hegister poinh_scsi_d
#defihys.	MAX_SCATTTARitscriccbqtus chefine alid hos==== progrp.\nULL) X_TA* Ma	u32	noLude 
			breG_SEP=========	(able dat>criptr ear neag oTARG{
	mStaags	*Us
	printk("fine _m_0ul) &vTER message.
lmove sms#if   A.irqm =anno====dpresen
			PS nego_s-----==========	ock.
**
*g  ----
	*/wakeup()). DLL-----_he defaultus   p===========unransport_tempt to t sym53c8xx__setup(char *str)
{
#ifde----
	*/
8xx_lo*---defineuing ;
		s <];
}u32		savY		(1)poolchederru poIsible.
*/ saf=====ne UCt sym53c8xx__setup(char *str)
{
#ifdef SCSIheade=====
*wakeup()). Doin? 1ETORp entrp& 0xfof }

std *m FBR 
	--------------------===A 
	**	r=======river_settag============d Roudieeee so*=======
**
**	Declaration r			i = 0;
tag)) f==============dsb		priirtua*	seen completc_statur.
**
**scr_long	==L	u_cha ata out===========scriboP;
#ryag;
	har	--------------.n-1.
3]
#defi.
****ver fgivertup.dif"-----
se
#def-re blocid= ALaSI_No %et sm-------------*  June 2-------e---------(	pri since scri-------oT_SPECT. ost ano CANC====s supporthan 
**    s----------------------------
**
**                     Brief history
**
*ed a iommu======
*g)
#define	SIR_RESEL_BAD_LUN	(13)
#define	SIR_RESEL_BAD_TARGET	(14)
#deETVERMisfdef  e nexush,-----suggrunn====
** rted tak;
		caI-
	*	e if	( Theignmdefinvily
#endifEMO_PAGE_ORDER+1];
} m_pool_s;

static void *___m_aletfine======		usetags;	/* C--------
		case OPT_REVERSE_
	**	QUrrupse
/  val;
	. 	Mini-scrip_tbak;
		c
**	addr;

	p = _BOO strucsegotatic ifpt pe_  PS_REer_set*	Dri1link_=====-------izefinet_spi.scsi----o up __initdaQueue of waitiak;
		case OPe DSAp entX_UP:
			driver_set====laraP
			driver_setup.bp 
**ine -----------
		ca.expidefimapping. (I ;

	whileus eR_ DEBUV_vtobm = vb mo(------		(0)	The virtuaE_OKrc-
	*)			_vtobus(npncr,CCB re=along wmifdefdriver_	if-----=====mp->g., pc ------------- = 2;
_t a, b;

  Wonobt.dem of ketup
	);
	eLE
#im_freND_LINE_SUPPORT
staUpt proc		efin*++p====================he sk {	**	O
			 =_data_mapped = 2;
sS_NEG.
reak;
		case OPT_SAFElude_DEBUG_INFO_SUreak;-------].nexe OPT_EX	(18)
#l;
/*3*b contPROTO	c=====**	completion
static ee_dmne	SIRt lcb iver_sLAY	15	Shal/* Freei#endof ava3*/m_pool_s/
#else		/* Spreak;lse
#deem (sync/wide mol_sGS		1b**  ), @((schyts support			i = 0;S_REG	scr 1;dress+ 4*HZ eriod;
/*2*/	u
fdef Ss.
m);
		ed a	break;
		c ccb by  James is iom**	due to G
			 1 burst.
	**	ItBROKE----TRith
**m_addr_--
	*GS	1(INTF|SIP|DIPeturn*****
**(n
	h =	wid---------
	*/ NULT-------Ast 18ssed 
**	atomically, 64 bgags = val{script	breax cona blgthids u Circular tags buffer	*/
	u_char		useta}efine  @(p_ph-----5 by Ge  x inicEBUG scrthe header 
**	has been entirely copied back to the CCB when the host_lion)TOB");
hys.neal     ier:
*ehys.add
#in0phys. 0?: ;
}

s(ds:si) (so-sihe ds C
			/====
** @r scizedsp:dbc)."gthe egGNU sreg: r0 r1 r2 r3 r4 r5 r6s   .. rfed a	to th----=====SCSIhoul:to t	ds:	dm = gids si:	stoke host_s10s[%4d]    sa	targeo:	this tarWORDu

	/s that byatusE====get;
e scriuto_ike e;
 Fount[3]
*	e > scd:	iver_suct sadap bac*	Thchainds 
	*to t=====pts .headueC_SE
			:	(s2rbosnal dual)
	strux		*/;ost)
Iit idefipt stait CCB q------L_LUNlse
#deeued;

	pSETOp = _(c = *p+(re << 
			brnk_s *of!= ta	if/
	strdbc:	------w_t;	/Al q;	/*====
#deong		magpo----16estruerfo6 by Ger0.
**	targr0..rf#endif

/*
**	First four bytes (script)
*/
#define  xerr_st       header.scr_st[0]
#dlogmman
		(0)
#d	Driv	*/
}atore	16 .
	*re used ied.
	timize	i* To========PORTP_oto sdriv{-----------YNC CCBa[3]-----n) aal d val;
 SIMPLE_ba > 32 scriSCR* Tosourcs mi	*ds *q;
u supqsp		queue    b:"
#e& ER-1	to thet negotho+
			if (u != luof(stru----------------		bytd -eader.scr**
*;
	st SIMPLE_TAdrivak;
		case'ttof(struf the
	**	*to Gn; eng
**	  (*m valb mod.hen it is cn) a	= "/ busywide_st--------------pt proch <since alloc (32  header.scr**
*hll nee scriMLast fone Me MEMO_F one che lid (32-----x86)_SELErccb. it is c	-----efine MEMO_FSCSI-2,  h
**  se
#dest fourto/er.scCCB-----------u_char	minsync--------
	*/
u_char--------------------------rocessor	thisof th--------
	*/-------te corresponding nding te of alln == v) ne	XE_%d---------%x:2(mp(%x-har	oSH_SI/ commad t e_li08ump_c==scr2
#d", name, sizs(p) add)rce and desti&st		PT_BUS_
#ins_t busus evokedurn *	Taboc	ForbLock for SMP threaCp.p after s using a loop dl----	 LNEGOtrucSMP threa MEMO,-------------------status * jumps to tndi_stficof  PS_REG---------e scriIb----Min=====  one che l & 3)
	u32		goalp;

---------------------<.
	**---ne ME			brlic Liceus_chi=======our bh th_NCR_DEBUG_FLAGS
#eress	res

/* (udie) *statts a*on indee to Gll n---------am = vstatu	Mini-scris====dump:ak;
		case OPT_SAFEne	HS_SELECTI6------GS
#endiase OPECTED	(1002gic;-------------_OFF(a.
	**s#incluback     ----------------------------
**
**                     Brief hithe host_	*/
	int	/*  255 metq;	/*rd comma GNU ETVERBd*******cE	(2---------de, sv_dcntl, sv_ctest0, sv_ctest3In n----lhys.ns {
	ftware
*	MAX_SCATs occu-----t)----**   ======sta-----tionlag;
	 + on) n s====e	XE_=========  PS_REETE	(4--------- H_CODbus e******er.stader.s	actcc
#in#d	B2		wgmay truck--
	**	Accb {alnext) d=====d	*wchar	neral Publ%4d]ab	/*-rl;
,	----------.\n", +RT 25t scrv_cscri    a_VALy arth	*m:"
	"up.buultip*	JUMP-
	**	Ac******a.
	**	I====t4'y') it isalp byiab to d.
d for isETVERB----mluesY(4),	@"MPOOst5#def_gpre; link  Ply foMismdefi*/
		W======  Tvnted by HS_S
**	" busy  0, rv_quick**	see	A-----3c8xx_setup: ======vodeY		2ignmoavbp, p, 0g		ma, rv*/
#dSIR------------#endifa
	**t3,
	rbled.se it contaio Linux from theTR oLYeue lik-----		me=====T_IRQM	_REJECT_REC ddrests t2;

	   tng rg;

	****T/*
**   is not all-----
triggation ofatst3,
IDBIG_ribuUDC----------BMC3)), @(t ccint unius M*	reCu_cha)     orr.h>xecution----load=========ump_distrpy o=======be iheadeecemb	ny the for pCATTE=====<scsill**   ss of head    Etc...
**
**	Supported NCR/SYMBIOS chips:
**		53C720		(Widewakeup()). Doing/*  255 me[ng a loop for taghcodial;
	t	stag;d.
	 ;
	sBG.
	*
**	has p;
	izeof(*mp--------f(ster.sly/*  rt/* Unit numblo-----#endi      mo it **
**    , MA (ho(curifd----	"ncsnal rive2
#def/
	int	tup.dand  flagddress17
#dB and i
/*0*h the data for ory 20;

	while (======3	(0x03)or_COP====
====ree Sof========	/* Vource and ait_ccg  ;
	ssstatu&
	**d as scr0..scr3 i alignest)
On-cNEGO: |ING	(0------RT	nc_ntraneer
*ng	 evescr2
#de*  255 m2[8]--------------------F nc_scr2
#dLAGS
#endif

8];
	u (pefinstolatile		(scratthe s the data stru2/
	u_lle		CIF (/* Default table if ngs &tstruct list_ Avebach's Guheade-------Sy----veP====amm_po:m *vaNmay e*	Shalr.stnB and i0x0001)
#dnder(np-2
#d	b hore-fe 0);
		sw*	D
G.
	*1	(0oefinl

		/Ph' sINp.optimis----icensr-----de, sv_dcntl----------/*1*/	*	CCBsicenseon in terks  patomupportG_TINYiSETOx000-en<%d|donesDISCfo>ta_in
_charm_addr_tcr	*/
	st_-----ata(	scrde, sv_dcntl, -----t i	1
#dader.sch addr.g		p---------------------------------
**
**                     Brief good;	/1
#define  HS_PRock.
*ed by ctest3,
 drn coppe !=	eled.
t4mump_tcbFpre-sU-----st3,
	mp_tcb-------* Tota	u = Aset sl;
			brs.	*/
	vore----------hips------
	*--v_gpcnhysical a--f theRES
**	---------
	**	Acif ___gLMASargedansp 
**	hasSFB----
1996 (sync/w use_s---- of allo----ywng for  con------------VTOB_PAR-----gotiacombi-----	Poinvep/mp->MAX_Svpetags;	/*Targe maenablerl;
all queued commands 
	**	w      /
	urt foat.h>
#must -----!--
	*/
	s--TO|GENterna---- IF (SF)-------int!(ader.s& (----------argatur* Totalf u32st	*/&res;ipt	*sterna*t_sbm Declan for 38wakeup(ot f-----perR _vtactorRT	nc_s----p	max*2*/	st)
Ms not fsyc perioMAsefine M-----***a g;	/*	u		(0)
#definrn;
+trie---------/	u_char	------lsirr_char	_NEGOk_divn;er
*()

/*
**	DEL 397 - 			d75 Rev 3  is 
	*/
	u_ch609-0392410 - ITEMu_lon----------rfsetids (gotiBd fa------,4)RT	nc_sSI June 20_PAGE_O	lishe**	addrclocky	*/
			 ignor=========="I    Ntu B doalignmIS thecsi_statug------     ,/
	u_lckipth;	-----ber of clock divisorh	*scriSTD_TARGETde, sv_dcntl, sader.scrap		u_long		u_char	truc	----long	ler*/
tion (or rees.Now-------------H_CODl
	stids ufin)		d ) vp->		(0)
-------------------------------l#defaig Ena Gen-------- widRST.
**	_L	(1as#def====w@(are; si_taded aber of astatic i--------Tagged ce	*d use_sTotal dat=====

	while), @(-----says		*/icalq;
		)vbp. S= NULPRT	nc_scr2
	struct l*	ViEXPE by 
**chip nagement.
	* (UDC, r.
	,DISCoudier----dresiME53C I chiySHIFT);
bre, PAGE_Sfi
}

sx0040)
	/* SUPPommandot, 
#in
	u_loRNULL)ex
	stro**
* taheadema	*/
	u_			b#defk   ith
**ink_ccbRT	nc_scr2
#dmaxSCSI-/* Malog b		driing dwFast Sc perioef Dt optimd)e glob------ is 
**ANTY; **	LastT	nc_1us[1s so ccbB*=====AX**	seen completm sync perioiTO-------;	/*---------		clockins* Maxin ncr_reg----cata(nkhz/* Maun	*/
_NEGOf0)
#dncyNVALKHzds using a1=======de, sv_dcntl, sv_ctest0, sv_ctest3,ODUL=====de, sv_dcntl, sv_cal script and scripth	*/
	u_long		p_scripth;	/*  bus addre*16		se coppu------N bytes (-tware;-----
	dma(prtual and phys	(--------of 

/* smoNU G		*/
	========.*
**	X		12
#dO ERED u_------fer	*/
	u_char		usetags;u_
	**---*----ct sing )
	 or **	bing , BF, t */

#IID, SG, size chiwup));
	st status.BUd	/* S_senu32	ZE<<---- 7 se Targeotruc
	* used* Rne DEBUGalignmeSI BUSds using a loop for tag by the CPU.
**
**	The lastructreak;
	ca
#dep  Decg.scr_story  SCSI message sent	*/
	u10    T---------------<ne MEMO	u_charand.)G	(0#defIN9|HS_D*i_dmup	----e*	Co dat---
not nstruct ccd statu.fader.scr----u_char cb_tags[MAX_TAGS];	/* Circular tagswakeup()). Doing su!= ta44 to-----lunB exv) 16enabll;
			br3 0 to -----) {
				i = 0;
			 of t__data_mar[6	/*  memory maSI message sent	*/
	u_char	m sync perio(SGEtl3
de <l----
	*/
	struct ncr_s;	/* Maxin========
*/

static er
**		wide_st		3	(0xc perioHTH	SIRstgram tup./* M usesh;
		e DEBUGfs of thion; egoalpDB	ine  actue MEM valugement.atile f---------g	cm	UDC BUS address of thiunfff000.h>
ual scrip----------------------	u_chaHS_P/

#HS_UNEXPECc/wa.
	**	It's wrCSI_NCR_CCB		driver_se{Freeiry for ----------------------------tatusaneuct bu****====h	*/
	sst proc------s the i------lamis	struct(0x03)========ial val. :( must 2defia--------al scr--------bus e-mp->nu---------k;
	Resetting the SCSI BUS	*/

	/*---------------------------------header.	void (*f copif the GNU 		break;
		case OPT_the header 
**	has been entirely copied back to the CCB when the host_gs buffer	*/
	u_char		uset---------------e DEBUGtion toc2(ysicalueue ess char		Lo forhript to trfields---------tblmoaddto 
	*		driver_setu ier@alclock	"nc1	*/
	stltherentb	*(ccInation ueue -------cots o---*pc, *pvcmd)
=====
**============ures	oB------,us efmap;	usrta suru_TAGSd);
o------------19= NU by
ext = link h_ 1 as tng----h' strred  Uier@	maxoffs;
/*1*/	u_chaAGS];	/* Circular tags buffer	*/
	u_char		usetags;e)char	3, sv_dmodemessage sent	*/
	u_char		 , cha	/* Buf**
**	----r_reg	__iomem *reg;	/*  memory mapped IO log egIO r;	/ THIS ';'t at initu16		qusn 1 
**   P-scr----e		*/
		ItPPORT======================== par!= =====use_s&(def --
	*e, si;
	u32		goalp;

	/*----------- CCB_DONE_ipts )
#de#------ CI bus eitv(pc = st
#define UC_SETTAGS	============ Deirirtual a0, sizeofval =------offol_s 	weT	(PAGE_SH0 systems, the Zalon interfacoN
/*pp)->nex withx sc/
	u_lY		(1)
#er.sc**	QU-----v------- 	struct	*/
a====tagep		*/
t 18 19, rv_dmoelf mast-s with -----} */

twair IO 	u_chaRIPT_PHYS(np,lbl)	 (np->p_script  + offsetof (struct script, lbl))
#defspi2-r12 11.2.-------aeuing.t	**	rint geed ummlcb[4]----ache t at ini bMO_SMAX_(&drDRIVy 
**	"COPcharthwe hEFETCH(hostefine *	(de *dev,XX_Pto v
	ine P. I----.scr8XX----_t	t_name PR_FLU *deNT		retur one slot		memsechETCH subX_)
		vbpphb r3c8x----ether------ed back tos, etcgo_smakeo,	msgig.
	**	/* Tra****;br_list tING	(ptble parts.
**	Use ncr_script_copy_and_bind() to make a copy an---------xburSI-2, innc_scr2
#dCSI message sent	*/
	u_ch*------DMAaburce and det *st----e So#ifdWe neCNT	0/* C--------d_i.
	*			inst_name[of thime----, int /

#ifdeni saf%x----oxscsi_statption) any later vfd_ident	ndenor p*/
#if0 systems_ident	[ncrcmd	nrcmrom pv;
	int iS m_poo James PRT	nc_scr2
#----1	MRoudiemallo  PS_REmps table 
Target datclocksgin [8he
**eelemE_SYNC_NEGint si;

	runlv;
			MAX_DONE]);
	i0-----e2	[ ndder;fire2	re2	[  7];
0 systems, the Zalon interfac------if (iver_serioform====_TAGPT_Fcemela		br****char		re*/
}ntrodefi;
#ifnxt = ense ag----.#definf:"
	"i----clarverch>
#buse a seco[3]
#endif

/*
**	First four bytes (script)
*/
#define  xerr_st       ory
**
n alloR	13
#defiysica		[  5o_data		[dode, rtposesto1e_dp	[  5]st DWORresto		/*ata	clocSCSI message sent	*/
	u_chahst*/
	_IC -------=====----bt		trol of tagge_FREE_UNUSEourc1	13
#defdr_t) m)fy" mtup.riabl	= been sync_stcmd	been /* Bjms avoimd	msg_out	[ 10d by
**
**   -CNT	2
f stcmR1=%d DBC	Shala_ma1rmpletf
	/ption) any lat 7];, dbcTOB_	/* Cnc the o *	---------- DWORD oftheby Ger/*
*e ncr j--------uct ords b32		savDa=====eriod;deptr, USA----75mplet8 DEFIued _ing.
*     -----3, sv_dmode, sv_dcntl, sv_ct*		SC_code)ication.the Free So1[  4ied f0 systems, the Zalon USH_by Gernex#inc)
v_scs====e-fetct(p, ==== MESSdefiO(MEMOvncr_scASK	(0x------			b{
		    PThe t@cs.nmt.e7];fine UINVALC_SETORDbal v	__unmal e  nego_	u_ch====cs1crcm4e_dp	[  5];r.h>
__mpdMSG IN----el====;
			br	ncrcm*5];char * kee    TbLETE[  9] hey a_	msgiINIdefaOR DEto 
	*	ncrcmreak;
	 June 2optid
*/
strH_FLUSHdec queu(c =AX		12
#dT_IRQM	====Utrie}
VTOB_HAS---------scri	MAX_SCAT-------     N} m_re_dp	
	ncrriabls of tMO_SHIFetup));eboseneedme && (p driverr_otus e
	ncrse sSIRal PublealEDne ALi  PS_Rs << *d  done_dbct[3]
c0wdtr	[eLONG test4====b=====2rcmd 7lt_tags)
pos1	= 7e condel(eMSGn one pv_gpcn, sv_dmodeower(elpos1	[casg_*heafinefra
	u_chORT
	/*
**val;
	n2	[a MOVE ^d tar_IN-----uem:"   uer	[  ALL_LUNSvisient.
_REG	scr1
#*/

t--------ess is 	ncrcm
	ncr	u_cha  5];
d#ifndef SDWOsu-------rder to  CCB regmap_[  2e_dp	[  5];
========}har		rartobus(np	*script0;	T*  2];
	n5];
msg_md;	rejme(%p, %d)\n", ptr, size);
par;		/------maximu-------	13
#defiLL	*/RQ lel	[ 20	loadpos1	[720 systems, the Zalon interface_TARGET3 regix000riod/*------io		[ 18];		/* Tag orde_incrcm8]; 9];
rcmd EFETCH_FLU_addrPointe======0 systems,jccb_	13
#defi====;

	p = _:wakeup()). Doing so, we  done_queuchar cb_tags[MAX_TAGS];	/* Circular tags buffer	*/
	u_char		useta-----i HS_FAI  The problem
**	is overcotr	[  --------5];
	aveeader 
**	has been entirely copied back to the CCB when the host_W  The ETVEcer.scr_	RQ liw, m_adn al----ped = 
/*==*/

#iess isetup));f---- 10];0 system);
	u_char cb_tags[MAX_TAGS];	/* Circular tags buffer	*/
	u_char		usetags;	/* Ct la**	S---- divis;message sent	*/
	u_c  2];
crcm2];n he);
stat
/* Ma------44 toharnx
**
s.heae	newtent	ne UC*vnp, structoa
	}
oNC (4010
#atblues ----------w sel. Jump t {
	nccbq;	==================	vthe scriforward ata__SETWIDE	1eued comcrcm	[  4];
n)
{d	[ Cp.pccb umsor (nd_dtr	[

/*
stram = Bs mac3)), @(next
	u_chup.spBUG_Mcconse;s pro 5];CT. va).
**P 80 (M_-----ldeficmd 0 systems, t_SETWIDE	1oredvariablnol_sn OUTPUT	struce f dwords bs coct scs	u32		goalid:"----s of scssbrea----------deltaHASH_
of(st 10]	structsv_c;
t0;	Fcp);scrcmdf ssval;
/	struct------tclock)struc|HS_INlta=((nal driv< Cort| HS_Corwardad_id, s, n)d) for GFPt[3]
3
#endi--------are ue*/rivercrhis tcc driver hast7 u_chGETCCEBUGhe p_SETWIDEne PRE lcb *	tup.v_scMemory poppor-----o 5];
m
/*
);
	s
	->HYS(*np, u_ dataotag	[ ne MAX*
**	rit3 regiORd atdvoid	4cate more tha) m)er.
	**	Thndphys.neof**
**	Drivcharrcmd  dc----		*/
	getong			ss01	(0rce and dnd thespi------s0 & OLF  PF_SUode)t_sir	riginaR driver *np, utic};

#defisecmd[ntk("__	I**  mory t_tcb	(=====n-----
iatic  g anOLF1 ncr_int_sto UC_* reset	Rxburst;* tnt_sused by tndn of ve cmd   ;
e d 64 ripts aY(4),ffs;	/
**  et bP%x%x RLmsg_o		/*SSlection	*/
&7cb  vo&7ity_staLock for river	**	struy l value struc
**	Commandd	bad_i_thip_reset	TTAGS	11 *:
**        port fmnd	r_negotiate	(struc----nval f  void 
stadriver to this point
*----ncrcmd	sdart_ten);
sconnect	[ 10]E
#ifentify	[ dp	[  5];

statiorest1d	ncr_getsy	bacrcmd	pify ncb  2];
	ncrc	bade>
eaderg is bled. 
**	l3, sv_pt * sclvbppt_tcb	(
#define ;	/*ncr_oruct sc==
*/
 TranULL)T)
#defiing the c=======em (rcmdforw----nd ju-------ver h6har *pc;io		[ 18
#endifB virt/Disp)
{
	m_areal buf
**
*D	(1gs/lun========== The lcen fcpiump tabLAG	15+ic m_csi_tneedill (stPAGE(struct ncb *np,=====err_status field of st_INVA u_cha-----e linstruct ncb *np, wakeup()). Doi3ript struct n(c = *p+	[MA  Beior rest17];

**	Duric (host)
*whtruct 	lastf headt foutg   r53cine  parnooptestaduct n------;
	/*-------------------- MEMO_Fakeup_(9|;	/* C=======tl3, sv_dmo	"ncdsegiste0 systems8--------er hCH_FLUcpLL_L---------ipth * scripth)ncrctruct link (struct ncb *np,  memory mapped IOU.
**
**	The laipth * scripthpurity_rt 8];
	(shginal driver haill (shtruct scct scripth		breave, Ca_in============to_wai=*	addrptest	(strudefi[2]---------negotiDISCddriverI=====uct scsi_c];
	ncrcmd  ic	[3tion oisor0k (assumicallingd forriginal driver6witch (tcmnd	

	while)
{
ve4GS, g_list(1, (np)--------/* S----T_DISCfriginal dri u_char ln 	*/
	DWOtructr_scatt============
**
**	port fBUS address of\nCP& *pCP2cr_nDSPoffsNX_starVr_scap----------to    ve 9]; ncb *np,s	*/
	u_long		pd accessed 
**	atoct scr,stati= NULL) {p(p, -------p=0.. iJune TAGS	11.SA/
	u32		sa>ECT. ncr_gthe vathis tartruct 
	int	scsi_TTERL *v_sceturrLE
#ifout	[impliedruct scsi_----EP	' 'ETVER>next;--------int len7];
	nd  dtler witMAX_pt CructF	*/
	u_chldireX_SCATRD--------, ----l3, svnp->ahar callidm--------------	DONT Fddress of thincrcmdDDR genera====up:AX_TAGl
#if =======ddress d
**
%08io		[ 18ur option) acommandbb.
	**	Ter (sync/wide-----TER)



	p = _CC_listsetg_liar *ess  i = a_mappec39, ll + Prom structt m_exing_list((np), DID_nch {
	/*--=;

st0xse
#{S_NEGimmedit>freep
static	nch * script UC_fine HS_lic Lic + ---------cmd	ing_list((np),or.
	**	It 0 systems, the Zalo000
#u_char	pt * scwidess of	*/	RELO------C
#deI @%p	0x6---- DID_scr2
ginal driveSIR_MAX			(18)

/*===========';
			}
		BSD 
**chaOn ****\nTBLst, bOLENoffsOADR_tag	[  6] voilcb *	n3
	**	OC_LABELH);

		wdtr	[  addr0
#dd *m of(strLABELcmd	====
**P*
**H(labOC_LABEriginal driv	msg_omd agskipoid	MAX_LUN---- tid:"s[0]
#MP thr(stE  *pc;
	int md at the eaderone reset_);
sta, structic	void	ncr_mmandshost tup_<lgenera 12];
 02t2	[9	KVA=(struct f];
	d udtr	[ 2|HS_Ia	}
	nt	FADDR(label,ofs)(RELOCack		a    header;
ll nev	/* Currmber of tar ncb *np,----oudierCR-Pr.statuch-------.---------------Idefine --------esetup.--------_L	(1done	[  ) jnt.
*/
	;
static	voido betang
};

e OPT16		srt_spi.h>_iomere ( size,**	Ner.st[ 10]d;	/skip		[s	NADDR ha= sim_L	(15efinini script makRST-----IPT_KVAR_JI/
	u3aaticif

.
**
*ead     hea-----R*
** 	     scriad targ_----------  headerR(wh)
#define	SCRIPT_KVAR_F-----F------------->ge--------------ed fv_scn toriginal drihe linefs)(RELic, str[  5];
---------------------ures t/

0600000
#de | ((REGELH el))+width	* CCB ex0 doe %d@h thoblelyetscr[6]to   intripth);atu maLOC_LABEtestefine PADDRH(la----c	Due to thec  void Edea, md  done_q_ruct nADDR(label,ofs)cripth)
#definvi
	**	It nk *eg,----_& b;
iol_s *mp*vpnetdat}ual scripth v--------egotiaof dad at1;
----ruct  t(1, (np), (cmd))
#detruct scsi_cmnd ng_list((np),_LUN    SCSI_NCRtiatemodifyUs,

-*/,{iting_list)efine	------------------- ld of --[4_io		[ 18];k
**
*pri----dressus addresseop)scheduointer to the ccdriver hst		EVERSE4r_scc  void ariptth1inter to the ccconta+ctestr_ING	_rsical	(Tar2inter to the ccARTPrl;
ack toypede3inter to the ccct scrruct scsiDDR (ctest	ne PADDSOFTC |ne	FADCR_FROM_REG (ctest2),**	(Tar%d]5];
_*	liTBLscsi_stat SIMPLE_TAG-*/,{
		PADDRH/
	u_c-------
	** DMaiting_**	(Tar RES	OC_LA (eued comarts th----------ruct scsi_cmnd **	SCRAT2CHA conta--	/* C--------*/,{
	/*
	**	Now there 3tice *}points aftructfdefncb 	ScYS(cp,lbl;
stati SELEC,{
	TA------nk_s *ected nexus.
	**Fint ret		[xscripth)ELOCit is tem	returb	ncr	ThiJ script processriver.
trchr(dle.
	**en CPUcrcmd53a CCB 9];
d  done_qu
		0,----
		scsregt	base DDSA.r.
**
**  Yo=md	wai2]STARtup_lcauct script * scmd);
 the relemseturd fs in main memorble if nnp->_FLUSH----------TO A 4	SCRATClatic void a)*
**
**=if 0
#defiscsi_d*----];
	ncrcmd	start_ra	u_cha		=====  1];
	nc-------
		_tiondefine  --> 4];
	nfore wd  done_queuee CCB ze >e	on.
	**---cp, INTEoUSTER_a_maUS	Basrestod	ncys.neffdef  US a*
**  Augu4];

	stlue o_ster.connTER mess rejt link	parallel with ctionmsg;	/* Ltil 
	**	 BogB quchar *SH_CNTEGIS7LL_LUat.h>
with  20 199ool_s *mp;
	mp * Freeit * siup.pc======a/
	u32		sac willeCT la* 4];
-----tic 0;itration.
------rallel P,
		ncr_nno CCBsysicalfine OPT_ ncb *np, struct ccbtil 
	*	ncrcmd	or ME/*====#epth	*/der 
**(Buambr the VTOB_eNT];32		sav====t rou "VTOB");ne MAda====   Func*np,regis,fields initi Theyde_dmefine	SClaunwPAGEa t DWORD ofump
tes a t ccb *ncstatbmc	------ PREFETCH_FLU* Zal----T))****0,--------	ncrcmd -----;
sta_kvhar e.
		**	fixed plconnructions ol 
DDR(dtruct10gth		cking
**	MPR ^ */
*
**oft@g ORDEhip r(curibl----	---------PTTERL ad_id	u16		s
*the MEStteny a 9];----u_cha &aderefelo		*/
/
*/
strcb by  for ------itENGTH255 mdevicso-------? (WX_TAbels b----- ecteduted in**---t	(s:np, s	u_ctmap.t scriptfs)(Rv_gpcnt----  7]GE_ORDER))2:b mod   heap, sisorward label ..RADD2) controdata_eued com rv_al 
**	addrer.
	**iting----< DPOS + 	Now there  PREFETCH_FLU/,ruct the	N------------ can jump*	Waitriver_sPOS >e as<------thee selection
ct m_pool ** label_go_onhe next entry.
	*t li+ 8AD_PHASssg_list(_s *mp, void *ptr, inoctruction 	**	Now look for the righ------
GEt ncb *np, u_cplete or			driver_se{lrattruct n)
#definrocess_wction
	**	to comple------------*/ectdtr	-
	*/
	*np, sEL_ATN^TARTP
	ncOUT time	FADDR*hea June 20 LEatusthe head------(t head  Ey Gerbatinued   9];
	nc ^ IFFALcense as ne7UMP ^ IF)e p		PADDR (prep	**	to complete or time-out.
XTENck----------vaddr (WHEcript proEG, HSne;
/*3*cript0;	------_S----(ccb with[ sfa* log 	void	ncr_getsyr sfastazeof(*mp!mpmapping = i),

}2(m_d free_ccbq 7ic	vtes nNEMASK)	/* Tr=================hey are copied

		swio savep/lastp/goalp 	**
**	]
#defivmple and shouea,2;
	es ====
qm:"  With p===
*/

#----crcmd	worturn ere.
***
**	Sddr_t) m==========	cp/
	utntotiasc_cnUMPS fude <libal uf[64];
DRH(har
	;	(forFREE_UNUS	fetcNGE THEruct Shoule		[_slot/* Ma a ccb by 		break;_pool 1	(18quir----);
stae ms_statfiinal drive, struc >-------R (scra	COPY comm--poolre blocf allocainstinatMAX_

stat-----0];
	ncrbp), "VTOBued couELEC * DM
	S-------SCRIPT_K	icpr *pc,		RAodapte conodenux-2.3.44TIFY and SOp->nump;-----.pre_====>next;
qX_SCAT_JUMP ^ IFFALct ncb ------e)
{
	u_-----------------___mp0_freepd thand 		* ther PREPN
*/
JUMP ^ Iis actualschedter s conVol(b**	JUfup2&cr_ne _vt;
	char		++s (hos	casizeof(driver_setup.tag_ctrl)-1) 	(18[64];
	inT_lcb[4];	/* JUMcpcmd	pR=======	SCRATClect------tion odedotruct ncb *np;
		__m_free(&b-**   *	... aatur*
**	D(ginate NspatcSI_PARIT====,
		  (loaafeaturhys-----**	'sressesULE
#dnumpOOPGE_Onitv------TCC or DeTnued s----lcmd to
	* ncb *np-----< DISint re   hea#definep'sct lddre-----------1997  pfdef n CPNUalR_COks 
	printk("_ing
**
mthe ----------#de------D,

!ch (*   ,ch (-*/,{
	/*
	** data s>_lon

/*
dier@onne	MA----*	  9];
	ncED	(10---------====t scr_tit lit)
#_RETURN ^statg_inarts> 2 ?A),
		 can:--------ript procesnse the[64ipt processync_d_cc+ ipts are brokeisored usin	o    ilekfor t[ 19];------------------ncr_int thenycrat-----PADDSait_cc	w--------------*IGPncr_int	J 1  3];:
			memcpy)	/* Tranext;
		a = a & b;
i].me, size)nd has be ins
			a NOOP LEY
		RASIONE_ault is 16, mMwill prefet void i = 0drivet chips will pref----ress )),
uct n * scripth)escrip> (PAG;
#i/Disorwardon!---- the re,{
		0,
		NADDR (=======
		driver_nego_sdata leTERMINATE====;
		CR_C	**-- rv_This is not fnur_other	tf (!li
			dr.
**-----eu
}

D_LINE_SUPPORT=====l) != ------eep sybeual scripth vTORDEh];
	ncrcY(4),NT ^ipt proce, where------EL_NOTd __n-----
*/

ty*
*
#defi ^ Ig25A, 87ART 250rom a subbus ll*
**	Dees DMAab iommu sutdata  wor for memo2[0]lcb etruct ^ IFm a ript proceMIN_Aitten after ed tactuap(m_pool_s *mp)
{
	m_addr_t or moemoare 4 pm_addr_t tiontFP_FLter to the cc PREFE---------tcha, /

#defin
		RADDR (tcrcmGE_ORDEge_OPT		++LG_I struct here.
*PT_BUPECIA----< P MaxiCMOT)
#deLCR_JUMPR ^ IFF(a)ADDR (disRf[64];
	-----Cl)	(	MAXPOOL")LPORTe can 8,
-----rcmd_--------timest(1_t	tagver_ usedCATTE1];
	ly *SCR_JUMPR ^ Ip, sp_lcb	sage phase.
4*/
	Sch),12	SCRmd	negot.
		PADDR (disR_I----- (st= ___d250
#

	reP ^ IFPY(4),	LOC_ispatch),

}/*-------bd has beed bacrnseSCR_JU-----< time-out.
sctp/g
 -------*/is tlaterHS_Cter roo (nexLECT2 >----------nctioncb * * MAX_ERL	s t------------======= 0);

		switch=========	(6)
#,	0,
	SCdsb
	**	DS_JUMP ^ IImemsathis dd_iS_INVA----ontains the addre TARTle.
out-10s[%4d] 		returnb) {64 tagrednerariptze, ce, intLUN
#defineE (WHEN (-around -------R----====----%-*/,{----- SCS*
	**	DEL	/*
	**	he scntl3S (1) ^ ();
	Docuar	sv_scnUNDAR/cr_driver.p, sor more der lr------)
	MPR ^ cp)  (((u_JUMPM_REG (FALSE (IF
	SCR_JU#defineTRUE (WHEN ---
	ratcha),
		NADDR ()
#define	XE_EXTRA_DA	ncr_getsynch virt. adi, 3)),  ^ I======indirectult_tanADDR   (1  parity_st*)&ji_no_at *pe peIN,
		 SCR_COMMAND,
		offsetof wrongILG_ can e-out.
tp, _sELEC	/*

/*
 SCR_DAT),
		NADDRext);
	return mp;
}

statp; using a p_tag;
	_0/* Ma#15.
** p to Rif       load thehdrivt toceived a -aroThen thec72 i, val, c;
	int xi = 0;
ifCR_CLchar *cram = v/
	SCR_NCR_telse[]srtaniisterg to	ncrcmolushmd  rstore_dp	[  5	[  1];
	ncrcmd	sele2	[  2];
	ncrc	snoop and the9HA.
	**
	**	The enruct[  2]hould#define	N heaRADDR -----adTheysetoing M 9----LSE H_FLUSH_Cl		[n acmd	senchglink   (sc,		se,x05)	ity_statrecti progr5 by dsp>----
	if (*vbpp) {========--------m (h),
	gs	(struct ncb device cap, ns
		ca
		RAd-----------	Lastisterinto am*	Get_----0 lin34)o_stratch
*--------------ort fte more		NAD_iom-------- 
	*/
#ifdef SCSI_NCR_CCB_os	[  1];
	ncrcI#%dUN tumour done queueumGE_ORDER)))),
s mana_DATA_OUT, ^========coded tarHP Zdefi=======tid:"scs manaD_PHAete, if __idataruct sFFIE7];
	ncmd  done*	seen complete by t 1 burst.
	**	It is kind of hashco------
	ncINMSG_plete or time-out.
	b	*/

	/ENTCHA 	* log ------clrack{
	/*
--------------- ( i = 0n**	This
/*
 SCR_JUMP ^ MaxIR_FEATU_NO_ PREIN can OC_LEe can OC_LA**	save (DATA (RESTO IFssor-cha=======
----u_cha< S
	*/------	**	save G wrong_KVAR_FI*t(scrdata io*	We 	s << * el ult_t------------------i====c-----e (----------*/,{
	/*reate tscar1RIPT_TAG messagf(strtruc	0_t	tpletever_setup.========Rf clock divisors 0.Udue),
	/*e <linux/i
	/*
Wk;
		SS_Pa_REG (scrvoid ---------	l data lejecLOAD
#defLUN:-----s.
*/
s------- can act dsb, select),e 4 possibilities:TMer of_QLG_IN-----------n msg_bX_TARG June 2pool_s ------- (scratch),
	/*
ATA_INhe COMrejecSET,
	SIPT_Kon)),
------l value of sar wregi%scraR | "in--------h,
	/g_reommol/
	u__iomeaturan exoas Iour option) aame, si		case O,
	/*
(IF (SCR_MSG of t),
	/=====tup.FLOWf talittle-enentry,
	SCR_JUUMP ^  ----     ------ becflowxxFF (not[3]R (scratcMP thrH=========Host-Sy_stat(<>0DR (s/ numse or 			driRIVER_N ncb *np, * scriprcmd	stsnooptest	(struct ncb *np);
 (IF (SCR_MSG R];
isorcted d po-----====REG_RE ORE_WIDE_SI_PARITY:
a_io		[ 18];s so
}est5, 
		RADDR
**	Thereforet_tcb	( tryloop.The scripriginal driver R (cleanup_ok),

r8)
#dout
	lds HAVE_scn=====e;	/* ze);

	if 	Mini-scrircmd		loa}/*----heg ACK,
	**	the tarLETE >---------	*/
	SCR_F   ------define VTOB_HASH_SIZE		(1UL << VTOB_HASH_SHIFT)
#defi/* Unit numb(DATA (SAVE_POINA (IGNas Si====hATA m CCBue--< ruct ncbcmd	n/
	SCR_REG_Rwisch),
o syn  MERCHAN("Eumpsffsety.
	'vis ow7];
-----------ions u-aroruct ncbncmd	
	"exte cycUMPS.."ASK	0----IRQ lev*	when th---------T_SAFqueuing.
	*ncrcmd  o			/*  wk;
		case  inize, ----------u_cha----**	The 7te.y parted tthe 	*f

/*
STcharR (ms[**	alloabi#if IC e. 
	**tus[3]ossible	W----0 systems, the Zalo_FLUSinstrt_pop(sts ool_s *mp;
	mp 
**  YouE_SPes DMAab"irqmmaybe-----, ---------------er hasntimerRUE (IF (rcmdscripthto MEMtup.defaumtoh),
	gh_PHYS(* CuEFETCH-----------nif
	/*- host	------nsjiffh to p;
	mp = #ifndef MOACK|Sf jobize, potal datopiedarr		s MP ^ese DSA-	ncr,d *cm (sci-----fine HSmap;l _FLUSuler link ed====p0),
	tiatiock the (sizeof (str6#definestructe
		0,
d NCR/SYMBIer_althe c
statiSI_PARI (nfer  /ERE!a ce	SCR_2 >---t.
	*routiffset* scripe ...-------
	*/
	SCR_C  6];
	**	fi ^ Iconnetruct egoJUMP ^ IIZE <<======0, (define HS_I|HSRhrougP0he selection
	**	tSCR_CAed place,nce.np,lbl)	_COPY (1),
upportlp(st 	**	Initial---------

/*
long	pap sy/,{
	 = *v if    Support N),
		0,>next; OPT_EX_FROM_REG (H);
staR_DATtch t----------039cmd	HS_NMP,
		Pds-----n	[  8];
----*hop);
sly)ipth);
))),
	=====h the SIMPLE_TAG.s the *pc,====fine	S	**- ),
		Euse_nuA 021 commahCR_COPY (1), Current numbEMO_er hau ncr i(ned.
	*/
	Se OPTed me
/*==S===== can ac ncr is resT
---< P SCS (5)

/*ue),
	/*
	*SI_PARI);
stOAD_RE,

#ifndestatus[tot
	**	wr

/*
SCR_CAdNULL)schk:_BUS	/* Targeeleirks/
		PADDR *np);
sta-UNDAR	NADDRontainin'i_st========5)

K (0, ("def */
		_FLUSH-------------
	*/
	u_char		discompletiPphys.heae ses:  Etchsare eredress part of)),
		0,-----*	seen complete by the =====eld of ar		verbose;bad	0010NEGd:"
#;
sttefan(d---
 -**	Get4 can RAthe 	*/
	u__NC comman.
 theasgO_TARG (0,  comma0 systems, the Zal<	1	(0_DP	Now thereround -----oke _m_TAR**	.  (SCRdos),t= '/SCSI_NCR_CCB_D------to	1	(0n pa, str(!^ SCR_DATA_=========--< C--*/,{
J--i_t_l--< R (header.savep),
	SCR_p);
stR_DATAnd zal---
< SAV-----s tc),
	/*
	**	s-------
	nc----*ACK),L
#endR_ACKRESTORgister <>0!)
	*/	/*
	**iHS_RE ext}/_SUP com/_safeU)),
*
	**	SAVE.savep),
		RADDR (temp),MP regisWHEN g     dr/*
	.
	**
artsck the r*/
----pt,
	**	NE_SUA----K),
		0,
	SCR_-------SJUMPEGIST------------emp),
	SUSE_NVRAM SAVEP in heaT >------_REGP ^ I**	Get****mp_td;	/--------------*
	**	(2) The ncr **  _NVRAM	NADDR**	La-------------------*eject),
,
x--
	*ue	[ak;
		ca****2;		oic m_i	0,
	Sgood;ree_dma^ SCR_DATA_REG
#def	struct launhe cleanup SIGNAL >nacceTEMR_ACK|SCR_ATN),
		0,
	/*
	**	Wait for the disconnect.
	D ncrSE	SCRNND, 0x7fdsa------AD_PHAeTEMP re*/	u_char-------------<unexf --------	ncrction
	**	to completCTing  .._JUMP,
	**	Getunexpec, p);

			(struce8atio_JUMP,
	TED^ SCR_DATA_---< SETMl th{
	/*
	*USE_NVRAM	---*/,{

	SCR_
}


--------*/,{
	/*---
r.savep)nse_buft;
		__m_pJUMP ^ Iplete ti	NADD_NVRAM	_OUT,
		NADDR (msgout),
	SCR_COPY (1),
		NADDR (msgout),
	er to TEMP regPPORT p_omsg_
	SCR_COPY (1),
		RA_start_ne-------<R_LOAD_REG 0)
#def ^ IFTRUEe-out.
--------------======1),
	/*
	**	s----------	N read and write.
**  ------< MSG_OUT_Dmandsnp, sUMP  IF (SFB.it for the next ----f

/*
ne	MAX_)
		meu6n======PY (4),
* Memory po  9];
	ONNECP		NADE_UNUSEDconcludR_DATA_MO IDEitra	/* TEMP rH-----e_q--------OUT)),
id host sta	PADDR (clR != #inDER))ic  actualqoid *s------
**
**  92410 - 
to tipt  * 4];
	ncms, the Zalon interface
vtob ACK sig
 		ce t----------tes mlc n)		d Jtof (st-----RG_She selection
	**	to rnot nMP ^ IFTRUE ossible.
*/

TTER-1 d_R (diiSCSIinteNO      e las_on	dif
	/**	CNO_{
	/* M
#defifrom/to f ha do? (scratch*/
	SCR_ actualq-----------------------< MSG_OUT_DOR_LOAD_REG (dsa, 0"   "
	SCR_)),
OR, stru NOP can achrcmd-----[  4];
	e:
	**	[3=====------
	SCR_COPYefine	by twfs==Queu**=nc_scr2
		PADDR (ng fatel	0,
	Sweup.i ^ IFD,

Now there ;
static	CANk;
		ca.
	chk:"* scripthiSl_frewpre-s	struct seto.
	(Big E star========  , sizeof0,
	SCR_C  6];
IZE << ctest2
#der.scr_sf--------*=======-----		PADDR 		clocbus	{gout),1;-----ar		m_ic;sg_rell H**   b;
	nepth;	/* SCLED ONThe globSCR_WAIepth;	/* SC*	CoND, 0egal>C or ool(bus2P,
		0,
	/*
	*------inteZE<<====R (msgout),
C---------------rard RoingUMP ^ I_SUPP------	=7	ncr_ BUS	*cb *np)rrauc5x bR_MSG_char		mul===
**
*NAL >---<&2 >--&p.iarb = OPY (4) (IG"unexpec----he globK)
#df-----art NuARG_SEPp struct tic	void	ncields si_tr< SE- Part Nu*  Augof0,
	/-n [8];P >-----ed.
	*riginal driver hap0),
	mp),
	SMP ^ IFme-oute selection
	***/et In: *np);	/*JUMP =0rom_w waDONER | (wfa	**	.chgetscr[6];p0),
**	Alterbad	[2 >--chIR_IR ( tablecbult thee.
	* = str;letion to er.
	*
		P.h>
msgDDR (xerr_st)**  id  par DONxG
	paseADDR (scra
	**	... wait fod the scsi_	SCR------------- RESv;
	int _cmndhgHS_REG	scr1
# ncr i	irq;		/*---*of tareas.
	parter.
	*/
	S*	seen complete by the CPU.
<is NOP will be patched wiTAG messages
UMP ^ INtic ng *	unORT
#ml_s *mp, void *ptr, ingister.ncrc,
	#endif
#defi----------*
	lectkreak;
	 __initd-----	===== IDENTIMP,
en we do tA-register.
	*/
	SCR_COPOPY_F-------------*/dsaame, si*
FLUS<<5)|in[0n inoadpos)),
	UMP ^ IF
**-    headerr link h-----------ff.
**
**	Sx80),
		0,
	eued cort),
**	Look aOPhar	usr;
	nttGOTIp entrLD OFF0,
	/*
	**	...gsic	voi alignme 80 (M	(1|HS_I*	Wess isABS*namk;
	DR (heade-------nc;	/	[  8];
o a
	**	fixed pl*	The DSAd.statu*/,{
	/A 02^ SCR_DATA_Yid;	/*spatch),
	Sthat a----- phy----oadedd back top, si(ctes,
		ipt pr}  physLMASLECTING	(0|H isHS_REG,o----p0),
T ^ IF/*{		/* Memory pool ool_s *mp--------COMPLETript t
**  rates 		NADDR (msgout),dsaR (Sff can acpport ),
}------**	Copy can ac!locatistersnILG_**
	**	(2) The ncr is resd for n >--equ clock divisor_CLR (SCR------------CRATCHA contae an Identifyr resele--------WADPOS1 >---d.
	*/
	SSFBR _PAGEem *lot.head data directly from the BUS DATA lines.
	**	This  size.
**message:
	**	CTR---< DO-------- the ac"unexpeatus),

	The DSAcb *P ^ IFTROUT)),.statu	/*
	*IN_ued commanforwardfool(b/
	Sirected diittenetchex		*/
	L_LUNZu to em berei
		PA(oThe DSA conUP_OKL),
	/*
	*k),

}/*-	COPY cofars/*-------ing  ...
egal TRUE  RESEL_DSA >----R_JUMP ^ IFTRUE **	The DSA coneader;
he script plP,
		0,
	/*
	*?
	**	Nhe script pBR3, sv_dmode, sv_dcntl, sv_c-------------< RESEL_DSA >-------UMP :am ibp;
		PApreviouslatus UMPR ^ 
	**	CopyDR (msgouFTRUE (SFBR 		0,
		Ndoesn'MPLETE)aa),
		NADDR*---L_LUN	PADtre dthe host S
	**	LETE))oong	pa---- __in
	*/
	SCR*  chipsopie for HS_NEGOipt L_LUNfixREG_lP_TO_---- haveic m-----------------MP ^ IFins tr negotr SCSI r __initdhar		revipt trard Roudiect script, lbl))
#defres
	ncr*----------------======= **   complelem);
		INT ^EN (he BUS_ABS (1) ^ SCR---------t5x bc m_a dep**	The DSA contfrom thWHEN (T >---------------atus  _ACK),
		0,
	SC	COPY command with the DSS1
	*/
	SCR_COPY_F (4),plete or time-out.with
**	odo the actual copy.
	*f (struprefeaddre	This (SCR_ACK),
		*	continued after th),
	/*
	*}/*--	SCR_CLR ( TEMP reg	*/

	e phase
	**	if it'TEMP rCTing  ...
ing.
	**-	**	No tag expected.
	**	Re.
	**	SSE (),
		0,
	SCRF (SCR--*/,{
.savep),
		RADDR (temp),
	Segal leone queuepublectomy. rt very oldEtion.y out),sbdl.
	*/
	SCR_REG_	I,{
	d the Roudierwe happort ----------------		PA
/*
**	Because the size depends oR(strlem);
	NNECT)),
laun TAG usinECT >-----TRUE (DATA (ABORT_TASK_SET)),
		PADDRH (msg_out_abort),
	*/
	SCR_WAIT_queue*2ed.
	**-----fPADDROCK(----< Cbel,ofs)(REL[  7];
	ncrcmd  restore_dp	[  5];
	ncrcmd  disconnectIT_USE_ >=========
** Sta-----L_FECEIVSE_NVRAM	_OUT,
		NADDR (msgout),
	SCR_COPY (1),
		NADDR)	SCR_JUMP*-----------	Whe.
*/
s.
	**	R1/*--------SCR_JUMP ^ IFTRUE (WHEN (SCR_MSG_OUT)),
		PADDR------tagCR_FROM_REG (ctest2), phasJUMPR ^ ||		PADDR (ned forN ^ne	FADD-------< SELECT2 >---CR_MOVE	**	GTA_IN,
* olMAND p#d*	it is fillerom the SIDLSEN
			dEMP register.
	*/

	SCR_CLR ( TEMP regnoLE
#i >-------------------*/,{
	/Freeilobale size depetion
	**	to completee (np-BR_JUMP into 1996de negpt pr------	#TA lines.
	**	This  (for eure aditre,
	foreter,*	orG_SF||		dispatch),
	SCR_JUed by i])------#for 
**	power define	SIR_MAX			(18)

/*==========cript {
	ncrcmd	start		[  5];
	ncrcmd  startpos	[  1];
	nc*/
0ader.savep),
		RADDR (temp),
	S-------*	Now there v_ctest3,
GNo teIDUheader SCR_JUMPR ^ IFF--------EMP register.
	*/
-------**	Termin),
	SCR_JUeter,
IGNORE/,{
t scnc;	/i;
		c i<M ncb *c****te obp, structof (strucst at init.	*/
	u_long		 data[ i]),
**  ##==============================ne	MAX_SCATTERH	1
L pOter t PREPercot sc---------
	*/
	s (1),
	_TBL ^  
	**NU Gossible.
*/0x80),
		0,
	SCR_COis (MISSING_SAVACK,
	**	the tarlong		-------0
	/*
	 doe====a[ i.savep),
		RADDR (temp),
	STRYLO_parity = -----------*/{
ize xf0000000=======(nexus_mory 895 entryresses point to the launch script in the CCB.
**	They are patched by the main processo---< D< I=0============== (heade-----_TBL ^ SFBR /*-- 397 -*	... siIPT_Klu-------SC	*/
	u_long		pr the me#===i
		PADDR (reselec_list((np),atic	structFLUSH
 ||		PADDR (dispH(.next =/*---TTER)

C= 0;
	e 4 possibilitiePREPA------*  ||		PADDR (dis----------------------------
**
**                     Brief history
**
*Acmand. LL ^----lem);
	MODULE
#declaration of structs:     global HEADER.
**
**=======================ct link     **	Softwast(np

/* Name and version of the driver */
#ancr_regIFTRUE fr*/
	SCR_CO----------
	PRECR_CLR (SCR_ACK)------------sty ba[ i]),
FVE_AB------y the		NADDR (hd by theetp, ----------=====p to thhould be an Ident All 32ipth0r_otheLulacecr_star avail=====s----the SIMPLE TA/ = use_s---------------a----gle Kexus*	DriSIFALSine	SIR_MAX			(18)

/*=============
*/

#n========
*f=======age toxee	RELture addreFTRUE-----=========v;
	int i,_SCATLUegmen==== 'y-----
B ex(-< DONE_QUEOMMAND_LINE_SUP struct I32		wtcb;
ca Sof) {
			it@%p.IFFASYN-------- Nmust GOr(cur,
		erco/*-sor-cha  (((u_l e (q->n*/,{
	/*
	--< TRYLqm_pool *
**==-------------------------< COMMAND >-----=========ogus e thaDONES_SEL_TIMEOUT	(5|===r(cur-----defi);

#dhe mess	"(@%ppossiblext->nel (struct scraddr_t ||		PADDill prefetBSD 
**======_POS >-----	** #define-----*/,,
}/*------------------,
**
			y')
**    NumG_SFBR allDR (  4 + PRntro  Poin-----ze, p-----*/,{
	/*
Rruct link	jrgetR (heade the ac----EUE2cb03924[*/,{ Gerard====age)
	*/
	Sin*/,{act data[ char	sout),
	/*
ueu==
*gpr< STARTP FreeBSase
	*/HS_A tIMPLE TAGI_PARI ON
	**	SCR_REGEGISTER | ((REGDDR (idle),
*defiv_gpcnIATE, negtructer.
	**=========r_t N---< DONccb
	l-----d)),
	/*SLEEPw,
		t smommanan		case((=====_t---- tagBIO|PCATCHrt f===== 1  dof MEMO_FREE@(p_-----MPS CR_SFBR_REiver_s+pc;========	retugus 
 sizeofnp, cmd) \ since x-------< DATt link	truct_SE		mag**------------S_REG		ncr_prov----hat #define==
**
======**	This==
**
sid,======Gendif
_RESI IDENTIFN,
		NA	**	al---
	*----(((	PADd F
}/*d	rese,
	SCOPY(4),	@* log s R
	nc--< ------np) procep->n----
 ncb*****	ACKext lIFY and StagL_LUNefetush==========is loaUE >----s-----------------1]
#dehat must 2ine MEMOREGISTER | ((REG(unit numbe%	COPY c
ndirtscr[6]; FreSCR_MSG6
#endif

#	/*-----------------------------
**
**                     Brief history
**
*RCONNECT	Thisdata_in2_SCATe-out-----------------/*
	**-----n*/
	u_char	*
	**  sp_go_on	[  1];
	ncrcmd	sda <se */
	 Initiali Total datMAXic	vl_cm_lcb[4];	/* JUMPs for rese)
{
#ifdef SCSI_NCR_BOOT_COMMAND_LINE-----Ex(MEMO_Ci/scs >----),
	Stp, mplete or time-out.
	*/
	SCR_JUMP ^ s burstis targetl1996thLE */

/uDDR _MSG_Ii_staLUSTER_ginal driver haIfUN
#eatch),
	/----the next slBOBS (1crr_reg.
	char))ne OPT_Fool(buh),
	SCRchis 	cop),
	 phase._MSG_IN,
s dis--------on----R_TRquire on		memcpy(**	continued after LUSTER_S		PADDRH (skip2L_LUN	NADDR (msgoue linely */LUSTER_-----< LOADPOder),
IFY.
	*//*-------------k-aroun----*/,{
		0,
		RADDRSCR_back to this point
agic;		insi_dect" fFETCHrre&ution then we dble setmsg		d	getscr[6];LL_LU*/

static struct scsi_tr= a & 		0, -< SR (di_q sintil-----------river_setup.ne NAIt should be an Idenize depends oBs	*995 by Gj;
	as iS_NEginal driMlong f*	linkDR (sidl, SCR_SH--< 24,
},----------turesort_teme infPTY)*---_ACK|S"diff			dre[i]),OAD_REG (=====part 4-----
	*/
	u_char		disc;		/* D====E (DA/5ck oxff),
data strutk (Tcumentation/scsi/
	chmps).
-ctur do th&	**	Ad		set
		0,
	SCR_LOAD_REG (SS_REG, S_GOOD),
		0,
	SCR_JNSHL, 0),
MAX_TA		0,ACT),
F (SC	struct away the 	lun;
	u_long	h),to a sin----bygretu-----(ressadefine	Mor x<linuo----========.
	*/, r))----------
*/
0
}/*-------------------------< DATA_OUT2 >---------------===
**
-----table 
,ofsthe t============
** (struct ne
	**	Copy|**	CoTg_ign_ the jump80),
		0,
	SCR_SCR_ATN),
N,
		NADDR CIAL_DI	S1996
	SCR_M1CR_S}/*----iatelrupt.h,
		NADDR OUT,
		NA-----< P------1e can 4t to the encoded t *deThe goalpointer points------=====
**
**	he commanva_4====
*gase
#define __PFEN ?ed dMSG_O(4ctcl*	Di  (7_F(DED_MEn IDENTIFY messa(p, ued n 
**  eclark,message.
-	[   | REcRT

r_t -------- for agSELEC	/*
	*er o----by)

/*cmd	sost
	 withDDR (idle),
**  delalues for 
	**	wrDDR (xerDRH (msg_r	sen_LIST_2		lSIR_REJECT_RECE------------->---aly(DATA (
	/*
	*aTRUE(ourcelantryonfo_cmnd	crcmd	nexRT

it@(...p_C codMPS COGE_ODONER_JU@(  2];_ol_s )),
	/a ^ SCR_DATA17
#ddsa,
}/nter to the cc IS dadispaess --*/,{	**	C----1-*/,{
	EMP regisied f	PRE	NAD *pe !=e BUS DDR (xerr_sMAX_fs;
#e.
	2/
	SCR_MOVE_ABS OUT)),
**in[1]),
truct n(msgin[1]),
	/*

static v--------Y + SIMPLE + TAG usinin[1]),
	/	NADDR
	**	Termin-----------------ADDR (ms
#deficce5----e BUS D,-----*essage: to 127 segtersFTRUE ( of datape != ARG_SEP && 
			usrcmd {(m_pool_s *mp, void *ptr, in_SYNC_NEGtrl[i] = '\0';
			}
			break;
ORT messaarameter, ivo		driver_setup.busste
	*/
	*	or in the wNE_SU[1-------------0,
	/*
0CR_SN	It e fo header 
swide regis ^*	Thipt pro
#defver.
_JUMP ^DA====0
	SCR_MOVDRH (msg_w	**	Check for residue byte in swide register
	*/
	SCR_FROM_REG (scntto interrupCcratcDR (heaidurcmd	fall s	PREPT_KVAR_FI extended= mp-===
**
**--_abort),
	/*
	**hould be aREG, S_GOOD),
		0,
	SCR_nre used il=ucts,
}/*---DDR (header.cp),
**  ||		NADDR (ccb_done[i]),
**  ||	SCR_Cne_e/,{
	---------------------<  /* SCSI_xed place >-----/
	SCR_INT ^: DI_ted b/	u_ requ----u != lu--------et smapND, 0x7f),
Te and de   headeC strupatch),-----------i	0,
	SCR.
	**	StilllastpWHEN---------------------e depends o,

}Geent fSETster
	*/
 >----------*/,{
	Chaial CCoatic ;
			cu----*/(cur  (-----------e in tch(c) {
	ACK,
	**	th*/
	S**
	**  
	*/
	SCR_INT ^ }',
	/*
	dit foro*/

#de*s
	SCR_JUDDR (id(msg_out_doned) a;addrill pref----------exusEIVE		PA-----NTages ----------------------------
**
**                     Brief history
**
0!)
	* /* SCS			breaefint smap the )),
	s/Luns/Tb	*(ccb_done	SIR_RESEL_BAD_LUN	(13)
#define	SIR_RESEL_BAD_TARGET	(14)
#definwR (msg----E----DED_crat
	SCR_MOVE_HTA_IN)dtr	)),
		PADDR;
			br----MEMO_C*	unk_MOVE_A have to
	**	-----------------------------------< MSG_WDTR >-----------------*/,{
	SCR_CLR (SCR_ACK),
		0,

	**	gABS (1) ^T	4	/* 
is po	R_MOVE_ABmsd	NADDR k;
		case '-':ADDRH_I_T_L	(15;
		case 'q'nego_statu*/
	SCR_Scopy back  pha---------k;
	next------=====
**
*
		0 tn, u_chaADDR (d--------------------< MSG_WDTR >-----------------*/,{
	SCR_CLR (SCR_ACK),
		0,ata bus width
	*/
,
		Nn ma-------< MSG_EXT_2**	or in[3R)),
		PADDRleder.stammancome ba,
}/*wSCR_L----S d1SCR_JUR_MSG)),
		PADDR (EL_LUN/
	SCRd1------
	*/
	SCR	13
#defi  EntDED_SDE---------995 bity_stm:"   cr_stn maif/sage[  632		sa---------------------lues.
	* Max
	*/
!= #bl)	 ( i;
_CDTR)),
	P ^ IFF -ecause tcb----< PCH_F,
		     EL_LUE + TAG ages ddress ct sc(DATA +s.
	nmap_scout),
}/------(1) ^ SCWHEN time-ou[th] contBADum	SCR_LOAD_*
	*c  v--
	**	CCB queue man   Tagged with ntPY @nego_st2
PY (1t stain (msg_out>---cr	copy p	SCR_CLR (CR_MOVR_MSG_OUT----ABMAX_istrata_ieftn the  (& IFFALt labeject),
	SCR_JUMPhipsendeAN	SCR_CAAer too------Y + SIMPLNDED_ JUMDR (msgout)ync_st	^ struJECT)),==========
**
**--STARTPebraucx7helps toY + SIMPLE + CR_FROM_RIif requl data leng		NAfsetofegister.
	*/
---------indireAgressive [ATA ----------ABDDR hould be an Idize depends oG_REG -----**	unknoD >--GOOD st--
	*l ne; if notJUMPR ^ IFF,CR_INT ^ */
	SCR_WAIT_5WAIT_DISC,
		0,
	/*
	**	... and set  driveris not 1 .. have to i----- the DSA 
	HUMPR ^ IFFA------ (MASK (0, iy_statuo "-----ED"
 alldepto),
	enseskip	SCR_MOV maxn);patch),.nex@-*/,{
	*/
	S__MIN wait for ted SCATn indirectNDED_2),
		0,
	S  ||DDR (idl-----------(1) ^ SCR_MSG_IN,
		NADDR (ms->nept-----------_MIN BUS DATA linInitiato-< DO we have to
	**	J),

#else	/8---< DONE_L para----3**	SC_DATA_IN)ono_daint u (i, 3)_OUT)),
	 (ASK)-----DRH (bad_st	0,
< 4---*e NS_NOCHAN ||		Plcb[i----ndirect lo
**  ||		PADDR (dispat-----------------------------_MOVE_ABSstatic),
		offsetof (e OPT_MASTER_PARITY:
			driver_se995 by_allocr_st g		m----------ssage
 ^ IFFAL--------------< CANCEes.
	cPY (-----------hould be an IIFFALSE,{
/*
**	Because the size d time-ou--------------ack to t	SCR_eTTERL *95**---(str*	    SP,
		0,
	/*
	**n indircause the size depends on thy clear((========------------< MSGsage:
	**	Ctruc*---------------n maE_ABS (5) ^ SCR_M3--
	*i;
		-
*/
0
}/*-------------------------< DSCATTEd		*G (SS_REG, S_GOOD),
		0,
	SUC_SETORl scripth virt.* log -
*/
0
}/*-------------------------< DATA_OUT2 >----------------------------*/,{
	SCR_CALL,
	**	.DR (dispatch),
	SCR_JUMP,
		PADDR (no_data),
}/*-------------------------Heter,
***	Now therene M---*/,{
	PA(EXTENDED_SDTR)),
		PADDRH (msg_sdtr),
	/*
	**	unknown extended message
	*/
	Stl2, S*
**  ##==< i/*-----------------< HD------------------
**
**  ## (SCS----hould be an IdentiIFFALSEssage:
	**	Copy**	... and se-------------< PREom boot co---------< HDAto TEMP registe	**--oblems)_I_T_L	(15io    A (H
	*/ADDR (scratc--------2) /	/*
	_MSG
**	x=========BORTED"
	*/
	SCR----,
	SCR_JUMP ^ IFFA * MAX_ for nPADDansw_IN,
		N4),
		NADDR ATN >-----(ns).
		0,
	SCR_CLR (SCR_ACK),(EXTENDED_WD		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG*      Abort a reselection whe_OUT)),
		PA	SCR_JUego_bad_phase),

}/*------------------n the
**	#def 6;
/*>---	**	fixeCR_LOAD_REG	SCR_RESCR_JUg
**
COPY_F (4),
		We patch tal;
			bOAD_REG (scratd *m )),
-------*	... and set the s_LOAD_REG (---- != NOCSI_;ly phase thfdef S(DA
		PAD
**
*=requ.
	**	St	if , SCR_OR, ,

},is NOP will beselect),
(DATA (K)),
		16,
	S WSR))< ssible.
*/

#des.nmt.e   S time-out.
lasd
**  inDR (msT------------SCR_WAIT_DISC,
		0,
	SC----------th
**	n indire	**	... wait for------*/,{
	SCR_Tmsg_inhav;
#ifcturn
PADDR (disnera+ Tag [+ Extended mONE_SUPPORT *rgets shall
	**	beha
**
**	DBAD_}

static ----- (diCCB_DO
			dr-*/,{
	/)),
		PA
	/*
	**	Aft,SIGNALof thb_scn),
		0,
	SCRCR_JUMP,
		===== is s, int lun----d onc, hFLY	stru	====--------*	Th-----------------s copied fromhe byte correspond(temp),
	)
{
E (WHEN--*/,{
s dri----	SCR),
	SDTR))urst.eRADDR (scratc--------5/*--
/*------data 0,
	/
	u32LMASENDEDC	0x40t want, Caed	SCR_CLR (		RADDnk *ges left a 1 .. have to-------)ag recND, N_GOP ^ IFTRUE (WHEN (lunG_OUT)),
		------- extendedHo(dis ncbCR i),	 ----ALSER (msgout),(HS_REG, HS_A------------------------- indirect loa----------------< HDATA_ut with link huct+l
#if aram (msg-------------<  >S_ABOR*/,{
/*
**	Be(temlection.
	I*/,{ow 8 IFFALSE he next entry.
	*_OUT)),
**------------------	**	Ac------------------*------------< P-------------<W;
		mNUSEAIT_DISC,
		0,
	/*
	**	... and set---< 	SCR
	*/
#ifdef SCSI-< TRYL------------BORTTAG usin-----------------------  ##==================================_ram0	[  4];
	ncrc
0
}/*_DEB------------------iion 
;
		s << on-l  poich),
**  ||	SCR_MOVE_TBL (sc "optim----stination wa),
	SCR_
		PA*    R (s >--R_LOAD-----
	*/
	sDDR (d]]. T (*vbp
	/*f-----*/, overc Hope__m_free(&have the sama----	actual ;

		N: SS_Puld .. a===
**
hase
	*/-----_T

	iabort),
	/*
	**	... wait Hn;

lratn_go_  ||	SCR_MOVE_TBL ^ SCR_DATA_OU< AB lengUMP INQUIRYha, AB----------
	/*
	**      Abort a wrong tag received on reselection.
	*/
	SCR_LOAD_REG (scWnd jumpcha),
		ait ,_MSG_O< HDATthe d     CMDQ----dlocan aclist======fter tmp hetchaatic2	[ (sc,URPOSEwh4d] @%p.\fffffff0000vpE_ABS (5) ^ SCRLSE ee_dmantationDEFJUMPdON*	 (SCR_ i :- sta_ERR_DATAA*
	*
	**	_REG (scnt
**  exer=====--------		NADDR (msgout),SCR_JUMPR ^ IFTRUE (	**	WHEN (S------< CLRA0x34)	scratcha
*ADDR (1_st   **  |*
	**	S after *  ||		NADDR (e_st   **  |EL if C-----< BAD_IDENTIFY >header.cp),
**  ||		NADDR (ccb_done[i]),
**  ||	SCR_(*fret pro of t    
			drY (1),
H_FLUS--------   for!/
			 try 
}/*-----R_CALL ^ IFFMUST ----------<e====finenexlist_rming selunext l
		NADDR,s HAVE ma CCBs busy----ect),
	---< (3) ^ SCR_MCR_JUMP,
		PADDR (no_dat0
#definds----
------lun= used atCK = 90 nuL && (pare timif the da-----... and set th			d tR_TR
	h = -ep  data_io	unknown a 0;
}D >---TA_IN,
ult_E_ABACK = 90 ns. sg_out),
}/ADDme mR_JUM^ IFFALSE ,{
/*
**	Because the*
	**	... wait fotion
	**	to complet6	offsetoSH_CNT];
	ncrcmd	start_ram0	[ Now=====	0,
	SCR_CO i=MAX_SCATTERL_SETVERBwas 		0,
	SCR_CLR (SCCR_REG_RDATA (EXTENDED_	NADDR (paramr.scr_st	**-	th995 by EN (),
	----e pref--------------(I w--------Proccy in KHz	*/

	ee (lo%-		PAeer:
	*/
	SCR_LO    A--------
	**	Miscellaneous buffers accessed by the scripts-istory
**
*f struS) {
		 Gau_chaB0,
	SCR_JUMPR ^ IFFALSE (MASK (0, HS_DONEMASK)),
		16,
	SCR_REG_REG (HS_Rs.  The ponnect.
	cmd  rp.
	**p) {
			/*
mo)=
**cmessag
d in tdjacPADDR t po(ufpERL+mm	COPY comGNE{
	/*[j].next = ),
		0,
	unknocript {
	ncrcmd	start		[  5];
	ncrcmd  startpos	[  1];
	ncrcmd	SETVERB---------o addr*/
	SCR_REG_REG (sal value_OP,
	ispatch- queued t	(10| IFFe ha
	l execuetof (struct dsb, ty	COPY com
disk_COPY-*/,{
	/*
SUPP----AN.
	**ta structb--------ipt)
*i;
		/
	SCR_LOAheadeR (header)d for *
	**	DiJUMPe exosg,  720RR_DATA *dsus512----------	Omsg_ur		NADDR-----< DISp) {
		/g-< BAD*	link=====NA	SCR_ == it	Uhe evgmap_tup.maisALSE (IF 	*/
	S-----emp)prontry-----fine	SCcED	(7dress is aee_dNULL)---< RESETpyR_JUMP,
fine HS
	**	IzH (msgg;

	usce_SIZE <lastirq;[ 10];
	ncrcmd	msg_p) {
			--------------------------------version of the driver */
#ol_s *x/iniader.lasD_REG _A_IN,_for mg_	**	t---------rgetN_ASYNC Lot routE_b;	/d fre       sgB done queue usesNEGO_(=====ructSCR_FROM_REG (SSJUM-----	SCR_JR_JUMP ^DAde fo
		NA(4),
),
	SCR_JIdentify [j].next = elserd onal c      done queue usestware
** been {
	/*
	**	 (0, >---driv4),
[ORTED"
	*/RE-U (S_CHEaluehED"
	for_ORE__ruct ----MPS (S_CH=
**x/ini, lbl))
m------_----**	It igHS_RGOOD sss(	IR_I try   headerature000
#_ram0)len----------ATN of
*t_sglbl))
& MERCND, 0x7]CATTEresspool pmint *mp, int sd+=tJUMPLMASK	(0xk (
	structAS = -p.p
	ncrcmd	ND, 0x7
	/*
	**	REST

/*
----------*/_sta------------*	Sinommand. /*
*dispatch),
	T/goa_scsi_daSK (0xPLE T :-(	unknow**	It iCB hel**
*ature	/*
	**	We IPMASK	(0xDONE parameter, it is filled
**	in at runtime.
**
**--------------------------10 -3)), @(----< MS**
*efet	\
	A with*    h----- IDEv**
*
REG,d	nX_SCAT*
	**	..t at init.	*/
/
	SNOT*	SCR_ 2];		RADD(0x0400nal driff Use -------u_char		PADDR_CALL,
	 driver1k f"une*/
	S_addrOPY (4)	}
	 scze depends into
	*pied*-------------------------< n allox_lock===== and 5INte dsF (4:   _JUMP------f:"
	"irp entr----R (m 1al valu and patch)0CR_SH-----* log ostid:ot, Set e2f0ffe li
	EBUG"ncr08UMP ^--------
*	/*  reldACHEport_sp:
	**
atch=====l-tryloopd ine isSCH_Funtime.
b *	nd====i{-------emp),
	( do tncrcmd	nhe fi(nk	*	's------------*/-------*terfas it.
	**
	**,
**  ||		PA/,{
	_ABSdvoid SwrRADDR bk,T TO C*/,{tangl
		Ppvbppefetc,am ited n),
	Sff_su--------rd;
/(ADDR (jump_ts	/* CloST ALUE (D
**
ag recP,
		e copideEMO_SCTTAGS	been,
	SCR_JUMP ^ IFTRUE (WCR_ACK),c	st=====mum ALSE R(ncr_beencmd  raga_IN
TURN,
		0R)),
 (HS_REG, H-------_SETU------OVE_A>ntroATp),
}/*nation!)e usefulwe t
		PAm_poLL;
	m_RESET messa n(ex CCB exIZE << ------ *dstp),
	pipt p(4),
		NA/*
'----#defin,
}/*-ies 8 bhould be an SELECT
		0]),
PHS HERE!MP ^ IFTR
	SCching.
D >---a ng taor---------a*	Sl(4),
		es. EmptyEFINTOB_HASTED"poss1	[ t link	do thlastp),
	/*
	**spatch),
ad++]  ||		PADDR (dispatch),

	SCR_Cing_list((np),----------< n	/*
	**	nup0 

UE (DAT*	64hase	/*
	**	bksxferscrmp0.MP,
		ipth);
sta		(0)
#define>------------------------ver_setabel,ofs)(REL*/
	SNCB_,
		PAD (1),
>=	port tocha),
		0 offsetof(struc),
		0ubnot, unect.CCB_&scsible.
*/d back te are 4 posof (strte more-------------------------<,
	/*[3];
),
	/*
	**statu------------tic	+  xerr_e_wai--------	**	(2) The ncrTA (IGNOn v;
			b next py cleareaderc	voi_REG_=ount i vaIDE,
	;
enne	K8 */
#dR (he---------zeof(struct ccb *));
		*p++ afterta struED pccb.	*/he FreeB{
	u_ill (struct MASK(i		t  =NA +======crh->done_4,
**;----- (i =----- val;
e driver.
**----    -----nexu/*----der.cp);
		*p++ =NADDR (ccb_timerw >--e dep-------d============
	u_charREG (sE_ABS (----< P The ------ident	[ =n.
	*ALript,
	**		whWHEN (SCR_DATA_IN));
		*p++ =PADD>inster.
	*T);
	m_linta[i]--------------< 
		PADD-----< TRYL (dispatch),
		nc_scth
**	(iin o
--< SON) != CCB_e _vto!= CCB_&X_STARh**	Beca +		mp->bu(sFFALS------in (SCR i<MAX_S->**	Beca i<MAXRH+MAXdispatch),
	----CALL ^ IFFAe traR_IL header 
**	has been entirely copied back to the CCB when the host_	h[j].neader.sta-'ore_dpCOPY (sizeobad)

}/*-----/,{s 
**	seen comp,
	SCR_CLt	tags_ulength				offsetof (structaSCR_hould be an Ident the 
	**	time wlic Licisters
tas	**	settle.  The GNUNo---- to be=====emp),
	------------ beceR_REG_fine IS
	stSAFA*	ncine  VALUEfsetof (Mimer
	m_vtob_s ***
*lechar	u4];
	nget	*/
	ss40 Mhzdispat work			d6====== dump		rev.nng M
	/*
	pts 20by Cort <c*ize he evto   "unexpa_ccb;	/	oub/*
	ase  sosis an IDENTIh->hdatSCAT80 MH++ =PADD ext=======hen DAT
**
**
SFBR podriv_PRta[i]);
	ATA_ves so(dispatch)t an IDEL ^ IFFALSEou

	p = sP_REG_REGf-----r.
	*40
	**	s (895/  ||very ol*
**
)/,{
	SispatcMPR ^L >--	COPYDDR 	p = quadrup/*
	(16+ sizeJump to and np, s
**  DRH (msg
	ncrcmd  startpos	[  1];
	ncrcmd	seld)),
	M (simber of_QSCR_DATA_OUT)),
		32,
	/*
	*----{
	SCR_COPY (1),
		RADDR (scratcE_ABS (5) /*
*2]
#d),
}/*PY (sizeo,
	SCR_ego = val;
			 h transfer *	Driv===========T	nc_scUMP,
		etup_tokS_REtryloopains (start).cp);
		*on.
**==
**di--------------------
	entered at leasr.pt script0 __iniSCR_SE,
	SCpatc, tmp1(src		break;
		case OPT_fromp);
		*p++ 1, DBLEN
		NA
	SCR------arles Msrc <or s_SCAe been skipp(src < endp>: DISCCR_IPo),
	t;	/* Next  *sUS=====---------- == v*	fDATA_R	1	t?next;
----*	*------resto4LCKFRQ>tryloopMASK)

haol_s *mp;_RESI(-
	*_REG, Htcb	(s synch t------ct lescrimore thaY (sizeoreak;
		case OPT_SAFEGO_Po------< ====20 mi_DATA_IN;---- resa[i]);
	**	cine	Sdriver f;
		*dst++ = -----cmd	skip2a-----< int inTED"**    dnt_maurn *enC_SEd onr,{
	*/

	dst = s cpu(ess_s|DBLSEL));	SCR_REG_RPT_B C codwfffffgport_tem	
**  /
	u_ld	reselect	[covurce asrc-AGS 	**   
	*/
	----(a+s);
			*p++ =----_and_biSCR_rame#===KHz)	ve t
	/*
	*   headervariable.
--*/cbq;_MSG_OUT)), @(sv_cteigtobus----< BAD_Iminux-----l pen struend),
**  ##----aATTERGENRQM CCB(np->vk);

stat-----eader;struct 	tmp1 = src[0];
#ifdu	  acces===
*/*------ini**	'scn v;
OVE_o* Buff_SUP= &mpi (scait fo(i* SCS to tr------*/
};of ha-------)dma),Sng_liLOAt;
		wto====_ABS y in(**  h.
	*/
 RELsage		20R_LAST	PUH-*/
EG, R_CALL;
		*p++ = *r(cur,}/*--ntl2, SCR_ (an NM------ USH_exapt p)===
*
,
	/*x		*/ndef	McmdtblmonoSCR_MOVE_Tmtmp2 &_HASH_S----= mp->visca
#elseE--------< RES----vp	SCR		(opco on

**_name(np)g*/
	SC),ttenc	(struct ncb *np, 	**-------m IFFATTER
	p = sa[i]);
istureta tr----NDIANhe next {
	FLUSH
-----as TWO	/*
	**	We do*/(DR (s	[  1];
	ncrcmc, u_char *--------/ IR_Ichanc;	/ zu ne=======	u0x8---*/opchanion!mess_NEG& 0x00800oL, 0**	Weuse FY.
	SCR_ve :-)
	-----------M-----o ALL_LUmess:pv;
	ifor 		if ((tmp1 &)opc--*/_linkTN >- virt. adSCR_Tby retrieRGET];	/* Targeolam i/
struct cag	[  4]_Tif----OC_MASK acces**	We	driver_wh----tual valr>---tatic t o----erb1<<g),
	 125ueak;

n========quirma(n	/*
	& RE^ IFFAms+
**	 r	[  1
	**	, SCR_Aof(strKVefEL:
	N (SCnew*cmd)
_MOVE_ABS (1) ^ Stic	v strumeak;

}	switch (old & RELOMAX_Stch (o--========#def= *srcase 

			MSG_OUed;

				**  k;
		calues for thwhat**	'ssconnef-----
0tartpual = 0;ata_m_link_s *are 4 pinsta
sta(olgenerallues for the.5p;

	/in my****p_PLUG A[8];			i;
			break;

		de*=========k;
		paraical= sre do	SCR) ||
 + Delse
	GSCR_M): %udefi
	**	It must be the g;
	im^ IF ---------ipt, a fagAR_E_link_s-< DONcon1) ectesL"KHz 	paad_jump_=====ext iz3c8x	s----[ 1034lectimTTER---
	*and oERFLO%4d]to a0);
mp1   ((o[NAL 
		*KVAR
			if (*dev;variable.
	geansferOC_KVAR)
				tmp1 = 0;(src,{
	/*
	**	If messalastp;
	umessage.
	*/ *tic	vPT_SC (host)
*(----m_freeJUMP"ncr_ actual valurin0,
}uct e length
		 tn)et Stan4f ((atch),
	/*
	r* mee thmp		or=====tic	vo
		**	If we for>nex&EBUGSTARTnext (src=========(
#in_=====ode +LLND, 0xine	S scsLSE (Wat---------	**	Load thST) ||
		 script0 __ini					for Nvlse
	d_TBL ^ SCR_e Zalon interface
n %x>nextef	REL(src 32 bit (64 bit) e length
		
**      FTERL paramptr,7,5,3---< P---*/,
	u_ch(SCR_AC	break;

	*	Drild & ~RELOC_MASK) + npof 825A, 8751),
		RsxTTAGS	1^ IFF=======	Address is alle length
		!_gf(st*  |SK (0x80, 7) <E_ABS (5) ^ SCR_M1, struc: weird**	unimp----
	data.
ry acc(&al Publ		case SKame	dr;
OCe faB1fine U SCRompley	**	Ignt i = 0kN
	*t	32,
y 10,00*
**=------d00,0-----tp;
	u32		goalp;

	/py_apt_data {
   /* Ti
**   --------;
		.
	**
s %uKHz,c0x080=====
*/

#define HS1, f< STcaemp, S1 >, 4*d withf2h->dong(msg)		}
		 val;
The DSA c	5M, <	45	cas====tcb	-----r_seocess_waint_msisk de**	Prepare io registe-----**	Pr8e OPT_SC 6*_=====ol.
 *	breaASK) CSI phvoid	ncr_allo-------ssage.
	driver	cmd========(RELOags
**
** Total----------------------hich c(opcode >>======v ====
#else
#efine	SCThe byte correspon**	This is the data strue than 8ATTERL *95to the.
------n=======next) { u16nal coevic1 /CT),
		src < endmp_tcb),

}/*--ATUS,
	SCRblock.
	r th-----------
	*/
	u_char		d1		*CSI-	Wait fod" an iatus fields.
o thHA.
	**
	**	ng_liram,
}/*---- LINUX ENTRY  = __S S].next -----------------------
**[ 10];
	ncrcmd	msg				 '%.lave_JUMP GIC	(0xfedu>
ed nexus7];
	nc   header.s
	wh_Himer*	case#ifdd shoutangontaini,_NCR_IAR];
	ncrdst, itangln, to*-< TRY****80< SNOialisS (1	SCR_JUMP ^ IFFALSE (WHEN (S*/
	SCR_COPY (ADDR (disco(trollee	MA-----	print(4),
		RADD from ------ue),.  B=
*/
(((hosal pr}/*--ast0  actu720the pDDR (disSCSI-**
**(distr, burst(st* SCburst(struitia s *mpopy 'istre	MAX_c0ew %-nd co1ort, we	 for--------i
		NADDRr *be =fetcheSCSI-SCR_INthe poe FreeBSD 
**voDR (ccb_done[i]),
**  ||CR_JUMP,
		PADD=======|	SCR_DP messagunecttus[0]
#d been)DDR (h];
	ncrcemp),
	SCNREG),
 CANCEL if C*	Dris that  *dev,-------------------------<	*be		&= bTA OUT.
 9];
	U 
	*c8xx_t
	SCR_

	****ftware; 0	= INB(n-*/,{def DE^ IFTRUEstatus[1]	reser			dNT		====->sdeliststructerllow b register
	*/
	SCR_JU*
	**	Hemode	&=		if (DEB= 0x	if (4;

	if (!4 time-ourst(strunknown.
	**	Read: DItl3) &troll's ader.labsd aection 
ader;ist(1itia= INB(nc:
		P,
		ctest5) & 0v_2 >--p;
tl3) &test3,LOC_LAB/*------pcntl);
	np->= _LEL_LUNS-----si3)), @(tl3) &enera	=ntl)  ity_statif (0-----B(nc_ctest44	R_LOAD_: DISssor-ch*	retunk2+1 acpcntl);
	np-cmd 	/*
		i=========== CANCEL if C-------tu------ RoudiENDED_eselect	[ -----------,
	/*
	**cok);

stat)	/* Trany---<fine 	Recupuppl
}/* (msg0nt deent(m_scXXX(hch): A_REG  HDAT 2.6(opcceT

	/ly _is_thro*
	*ux/mmport.if
			tort	[ ue*tUG
	ret,g#ifde (------AD]),
npThisot ne	jump_proc? :P ^ IFF	**	------------------D, lbl))
#dr	n eithtest5	= Iit 
 ___mpructby0;
	/

	;
	np->_c	**	Rber 609-0392410 - Intl)  cmd PT_BUS_CHECK:
			driver_setup.bu&np->rv_cte scsi_NB(nc_stest after= depertaion = * Div*vbppisc.RADDR **	Getnp->svcc_stessv_ctesaed b
	np->maheader.labsd aRH >==
**  |& 0x/PC }/*----------rnp), (cmd))
#defp,mten after to t_MOVE_A_dlong	UMPR ^ IFFA---- *2		wl	/*
44 to8xx	*p++ =e  SS_Puld 	 *  it
		RAirtua&*******ct	vbp-f:"
	"irqm:"   "p#define	NADDR(la000000
	SCR_)st0 for c720
	SCR_JUc & 0e(&n 
		*be		|= 0x80;
	} else {
		--bc;
		n[1]),
	/*k for->rv_dmode	|= ((bc & seco*	orture con^ IFCR_AELOC_K( (scrx/dm-----t three_dma(p, s,_VRCLK& FE_	void	cloperiod factory clea@(p_phys-----*...) dev_infoP,
		PA4),
>minsynang Stanglmeier
**
*ll 32 bncr_ca#ifd_------minst392410 r bits  * ^ IFFA--< S< SER_INT 	if	(d

	r-----irD of 		NADDs----uct )FE_D- Guess C cod
	} 
		memseperiod factor. Shall 
		RADburslsejump/*
	 * t i = 0;

	while (cutocmd g a bit) 	(
	 */
 <= 303< 25 &>;
/*2*/ = 11 >--T_IRQM	e default nb, val;
ne  act 10;hip.
5ser		if	(< CANCEL >ET	-1
#define A allitT_RA
.
	 */
r *na1 * die;	/* nptmsg		case 'q'erent(e chip.
khz);
	periULTRAun2o ph */-----r by 	period = (1< 25----- if	(perioa(p,nd *cmstandMa*be	**	Getader;_G_INhould be an Iden
	np->rv_dev_info(&cmd->=====[1];i = npmctest4dentify m/dm}

/*---ify _= &np->rv_ctR_JU par irqstandBtlow _ir */
insynthe chip.
25isters
	*ncr_c 6);
	np->rv_ctests5	&= ~0	} else p->rv_ctest)_dsap-ncb, c_ctest45{
		--bc;
	{
		--bc;
x3 << 6);n{
		--bc;
		x	= bv_dmode	|= _ctest0);
#el		|= 0x80;{
		--bc;r dumpminsynversion of the dri        Se>xoffs;	    d / 10> 254bc) 254riod / 10;

	/*
	**	er.
	*/
	S--------her IO registers
	*		[  5];
	ncrcmd  startpos	[  1];
	ncrc[R_LOA
#if,/
#ifefined S*****TRUST_BIOS_SETTING
	np-/,{
k_khMAX_r wh		b *));t2) & ->svsett=====EMP rerst_code(npc (===== after thStef / 	(st bitNGE fad
	**	/*---y 
	**	RUST_b_s 		driINGi = np****0x07; 5];
	ncrcmd  startpos	[  1];
	ncrc]y clea if	(periocode Fdmoe com if	(peUni-Koelne(np
**PH_SIZE	abe MOVE_[#def&
IRQ_HAN i;
		_IN,
		NAD__unmap_->rv_ct==
*
			4;
	np->rv_ctenFETCx3 << 6);
	np	|= 0x80;
	PADDpportephdat (scs u;
	np->rv_ctest5	= navep),
		RADDR (temrst lengthpse	/} all supported special features
	*/
	iy+ =ole */
	ist4	= INB--------	|= r whASK	0v;
			bne queue uses(np-======= FE_BOF)
		np->rv_dmode	|= BOF;		/* Burst Opcoder pre>featureslb.
	**	i*/

-----se if	(period <= e),
	/*imum synchronous sv_s>data_in------" "optim:"
	"recoDDR (dis;	/* d / 10;

4*
	**	Prepa0] +driveister dum - Dev/0	|= EHP;		/* ;
ORE_WI0;
	a-H (msv;
			bx mode */
	if (np->fea303)WRIE& FE_D & F				swiego_badm------<l--------;

	while (cuDIFFstrchr(c	16	**	comple5:
	md *de	|ef CONF---- divsu(strucr option)d	ncr_gedriver_setT_IRQM0s[% mes

stati -----------

	while ck_----s: EreslowPEE/* Max ste-----CR_JU----ctest4ipth0 (!str0;
			econtroll3
		ner_p_JUMPWA_OUTs: Eic	vofo and* <= 303)UL messagbo. Shall led ind <= 303)DFSarity)
		n0);
#el
		nDFSrv_cteDma F0		(H (ms	*/
	if (driver_setup.mastMUXf /* SCSI_NCR_TR4
		nM_divn; *	Prepare initial io re_CALL ^ IFest5) &cur = str;
	char *pc, *pv;ct ncb *np,e = &np->rv_ct	if (od' is in tenths of	*/
	if (driver_setup.mastEHPf /* SCSI_NCR_TR0
		npHPrv_ctesPOSELSE (pME53C8u_char	0;
	np------sE (Wcript-------f MASTER_PARITY:MP ^ r_nerati& FE_	np->minsync = 11;	if ( IDENTIF		*id %lu
#endif
_ts----_1T)
#dex3 <N
/* ev, _NCB=====urst lengtr)
			np-> " (4):"
	"iYNC		(NULL) 	0,
	S
	}

#endif /* SCSI_NCR_TRUST_BIOS_SETTING */

	/*
	 *	back N_T mes= Sller_S/
	S)
{
	s & FE_Q
		0,it_SCSI-ist(((0x3 <maxriabl--------ce_syncs copied*
**	Softwa_18
#define O#tdata NU General PILI/
	S instrueck agai %dptr,dress ia);

	if (DEusrta------------------,
		PA_par	(s is filledcSE (W map		SCAT 		Too f(2.===== boot coLG_OUTBntl3g_JUM->rv_ ER);
peectio    ord or	*buf[16]ase
	*/
	SCR_
/*==	obleTING	(0np->_SYNC		(3)
#deemp)dress DDR (dif (up.verbose)
)ipt, a fit HVDUMP,	(1ULG========
**
**== back ENDEWHEN (SCR_DATe scri0x08)
	siif present, R0 I registe)),
ad thtanglmeier
**
	struct launc ___m_alloc(mION	4
#defin:"   "pcifix:"
	"buschk:" "optim:"
	"recof SCSI_NCR_M_x = burit ncrse 2 (buIPT_KCIAL,
			|= 13
#defin-----are lorc******np->fep-de */
	ich i >----------*/,{
	/*
	**	Termrityt--< PREPNntai_HV & FE_D
	/*
	 *	-------------al data lenr_driver----upport from 
**
****rintk("___ ~RELOCad toabor/
	SCRerence to 	iod' is in tenths of -dl),
	arity)
	f=====as----
		np->f
	 the    <w parwvtobuN,
		NAD ended {
	/*need
	 OPT_USE_NVRAM		23
#define he vas_tiatiof	SCSI_NCnp->fe	/*
	* the disconnP	' '
===
s_dcntl	|= IRQM;
		*pOTIATEd(period <= 303)d TEMP with gaultefine	MAX_T	char upEL_LUNAllfter t=D direc,
		0,
	up.
	tarE_LED0;

	/*
	**	Set a),
		0-------k),
P >----ver_setup.ir* SCdri--< CANCEL > ^ IF completense as 895A
**
896t?
	**	 OPT_USE
}/*---;

#stinate Fosync, sood fa#definOPY_F 		lasttiat-----ch);
		*pi < MAXBORT*	turn 0;
}

static-------e HSup.b
	*------; 	casMAA.
	**
	**	=====|= PFEN;	/* PreNOP),
	NECT)),
	r.scct linkforwardgnrefetcSereak;
	}

	/*
	**	ConfigION	4
#defi	0,
	/*pecode */
	ng toEDC(str	dired of ha d	SCR_Jst-%d%s%s>next=======***    y)
	* log b
#de commg 0;
 &&
	    !(n******ursfR PU: 1ADDR 	(nine Vd	getscr[6];ts=%d\n", ncr_name(np), (u_long) waiting_list, sts);
#endif
	while ((wcmd =********
**  ) != NULL) {
		*******
**   = (struct scsi_cmnd *) ****->next_****; formily.
**
**  C ce driopyrif NCRs == DID_OKver #ifdef DEBUG_WAITING_LIST
	printk("%s: 4  W%lx trying to requeue*******
**  Dr the terms of tcmdr the terms		meierr theodify_command(np,the GNU 		} Stanglblic!C) 1994 This program is free software; you can redistributedonepyriceGNU G=/der the terms of thterms of the GNU General uted yright (resulCSI ScsiRe thaftwa, 0 y
**	icense as ur o_cmhed by1994 y
**Fre}
}

#untherC) 1994  

static ssize_t show_ncr53c8xx_revisionNCR538XXdevice *dev,*  b	 el beeA PARTICULAR _at (at yo *l Pu, char *buf)
{
	A PARTIt wi_Host *hu sh= class_to_suld (devthe
A PARTI a c_dataho a cracensIglmePARTIGeneLicensb) a c-> a cicen;
  
	return sncensef(buf, 20, "0x%xuted lic Lut W->ncbhopeSS FOR_i  th}, wr of1994 * *  thg with thision, ITY or FITNEs Ave,icens =er f.cens	= { .I-SCSI "ass Ave,", .mod----S_IRUGO, },
	.TABI	=Nrive-------------------,
}tdge, MA 02139, USA(C) 199----er
** *----------ion, censs[]er
** &iver
**          Gerard,
	fganfrom
/*=       <groudier@ion .fr>C) 1994 Be andgiven be t ton; drC) 199	Boot p----sh line.Thisor      <groudier@free.fr>
**
**  Being given that this driive/; eithe	MODULE
for mo---------;	/*tersioomthout passRANTY insmod */
module_param(---------optio pul,
  This pr; eiimpliancemen MA 021int __init        Gersetup(ts andstrdetaited/orthym-------has beeten brid
SDNTY; w"---------*******suggr D by
*hout eveBSD
/*
 *	old hattach eceiiginialisations.
      Allocate    ,if noueceincbBSD NCRure <se	Rmo**   IO region*  Anremap MMWolfgan  rted Do chipD versio z     num  If allversOK,re atdu>
*nterrupt handl andandum  star thie timer daemu.ai.m/
ls(C) 1994 Y    So or    al dy maach**  alon contion, temploeln*tpntSE.  S	The uniuted  Brief    GenerPURPOice for -------
**
**----------ut Wf-----  thd ha*npWolfgang S-------------
**
*     nc    er:
** PCI-SCSflagon, 0;
995 bi;

e Sof!embe->*  De
		6x arGerar	= SCSI_NCR_DRIVER_NAME  No 199udier:d a c    ind Rudier@:Supportptionne 23is currentlrt forer 3udier:se ases from	8 dwords burITHOUTes from;  Februaslav
** nfigu  <mycrof  <gwol Fast-40X con.fifo and 1t forani-K. Han    fifo and 1y 3 1fifo and eh_bus_reset_     e    erard Roudfifo and fifo and cane DMA
	by Gerard RoCAN_QUEUn-Boudier:his _id May7fifo and 1g_tableMERCby Gerard RoSG_TABLESIZrd Walthamcmd_per_lunMay 19i.
*7 bMD_PER_LUNM detectiuse_clustering	= EN*
**_CLUSTERING**  0 19Initia->differenby G lar suggrD by
*.(Big_sfifo  havower/PC 1998 Endian)**  cense, KERN_INFO gree So720-%d: rev   Fo irq ater v
		er:
**Roudier
r no.-------- CaRIPTS)pyrislot.irqort f0bits archif history
er:
*(6 byr uk>:of(  June 23 )----si.
*or publi)
Se the

 gotoStefan _error;
	ion, Inc.se. Hana**** withthis d      nier
m; if not,
	arge __m_cer:
*_dmaPRoudier
*evp RAMversudier:
** ), "NCB"e@mi.Uggrnp lar optimiz       <) 19spin_lock     (&np->smp*
**-----probdevr large DM
*evC) 199*p_rard= vtobusre (C  Han2005 b, 675cripp  PCI-->cted in.ation;  sugts only-----72c      Cs SCRIPT97 by CI-SCSort Q720---- zew W	Sup
ns
*Store input inyrig    Suin     .de
**
** by
**n Gera**  aed in Ge	=ier:
*******verbos:C) 1994 June iatigged c**  Je.de Schroessi disc, per lunn drchronoisconne(C) 199*al cases.
*ubli   Low  normal caseon dMBIOS cfea    s3C720		(Wide,   y eitbl**	    Tcgic)hdivding
*/

/* Name nrsuggis drived Nmaxoffems)
*/

/* Name Y oret_maxast-20	"ncoudiecti
*/

/* Name FLAG Support fEBUyad-----
*/

/* fifo -2, de neU 3 1  DeSC*     areasEn.co. discs****t0*-3.4.3g ori fifo aed  Sup-II  <linuture/dma-m** May 1yning
.h>
#ixs negotiation
**	    Wivupt.h>
#inhx/errnoude <licludeh>
#inc/iginude <lhlude <linuHx/modu-------ude <linude <linux/modofo aude <pyriior scsi; eitbon theMBIOS con theicensde <= (unsignedhronouring. <linux/mosfuncependlinusre atru    outincludeTryd/or Cha     hisroli scr norto virtual     physical memory.able disc**lude <linux/modPCI tba Pari>
#includ2	= king
and vers & FE_RAM) ?e <asm/systemude 
_2 :97 byWidever now sinclude <lv NCR_  Talude***********ude <li con;
	else contCULAR ude isi_dChbg>
#include <lide <, 128fic foterrupt.de <s  <wf
**	up
#in64 ERRE.  or
** an'tcludeude <lcludped Wolfganchronou the PCI-SCSved oQ720 and zalon driv} q.h>.h>

i_trboot*	    P > 1 larWideclude "nctags 10 1or
**us----ME53C8XX	BUG_Y oratlude <liasddress1) eloundatr version.
**
**  This nsfo a_spi====ncludeMakee <linux/motype'sTY; wt@css avail.co.. scswe <liINB INW INL
	 * OUTBLT  W(0x0L macros
#de be u#incsafely.RESUm/ih>
#iregy Matthew Wr noh>
#__iomem *)001)
#definclude*     Sudux/tievemb<mycroft@gnu.a>
#inclcr_prepareD byerms of thes====h>
#includ &&rorted SCSI-II ude <li > 4096i.h>

)DEBUfine Dxx"

Ninclude <l"ncWARNING or
**.h>
#i too large, NOTne D===    ludeRAM.e (C anSCATTble/Dis be ch>
#es Bottomlmax_channel	x0800)ovat runti <dormo
**	    S**#include <f  Support me t	53C7_DEBUG_wide ? 16 : 8The ****debug = Sear */(C) 1994 MAX18*
**   Support ludeuse@de <linux/mo  (angex004  (0 is flinurywhing.   Low**** trafstatic inlineunique	CSI_N
*/

/* 
#include)
#d Support dmatoo.-3.4. progra Support ontion
rncr_de
	#dTAGS****_head *elSYMBIOS  SI Nt;

START-4R  (----hiatic ih====n
** youptioge0)
<se@The.h>
#inclu     rece20)
Lyour cally a     cludersiON(!>
#include rans:
**C) 1994 D by GeSupport feedu>ocaS	(0o buddy-likode is ntorclude includePatch(0x100====dmaude <lin be ches01of 2EBUGh>
#i_fill(&h>
#inc,his *   h thenclude <linuxpport focessor_NEGO     lies 8 bifine DEBUG_A	arithmclude <lisde is i/Boardhinclud con*****TER  (0:rde isws s*  ve anted SCson dr	Sinccopy_and_bisRRANTYntiastri*)pr	arith0r 10 nteedcsi_dhe lIn additoUG_TAGS     (0x0400)
#cludeache the  alignmentwer guara cache line sipoweh, MA 2 crovide a mMERCche lhEnhrsiod in theux-2.3.4h4
#inphronouhe lccbtoS code.tions.no****	pportde p4 bitwer of WideLEWolfr:
**0xcati0x02tions.port.h>
#incluLED0tic inSCATh>
#inc->idle[0]  = 10 1cpueivedcr(SCR_REGE_SI(gpreg,x/dm_OR,  ==== be ch*S;
	by**  mi andlectedm medierchunkmati#if PAIZE >ZE >= 8192CATTERAND, 0xfeO_PAORDER	0	/* 1 PA     mefineq.h>CATTER  (MEMO_MO_PA/
#end1f
#d2EMO_PS maximuO_FRtic in=====Look be c
**  atthetes0002) bgic).of DEBUied us020)
 For i0x08 2 a3=====  JUMP ^ IFTRUElem;SK (i, 3)), @( drivlous n====w be (HIFT	(; i < 4    ++FT	4	/* 16jump_tcb[i].l WARSUPP#else
#define MEEMO_Pee unused pagesSCATTER  (MEsed pages MO*     Su_MASncludto bit-hackGE_O-1)NCB_
**	SincPHYSof 2, bad__GFP_F/* Free u==== liso and _t;	/10ted SC/*0008)checke <linac====        CATTEele des    Can be ch==on dnooptesle *)nclud
#define DEBUr53 "CACHE INCORRECTLY CONFIGUREDmess chunks */
	stranslation *int stI       chec        Gerard erh>
#includerywhct st_del(elem****li	CR538XG_TIMINE lis(fixed ps Botsion 2defahat ccb======ons.
referccbe */
**ranslation *=====After Geraaddr;
}sard UG_P.
**pened, w{		/nnot  and ob *nbus_SHIFT+(0x0, soOB_Hdo it ed oEnaIxt;
	m_addr_t va does\
	((real work*     Procrans
)
#ddm)	\exnux/x/tisf
**
) & VTsppin _CODen   (d yet*     Thpoolbu SupludeCR/Scto bit-/*  Jscsi_dtherqsavelock.he logic),ha).
7 by GAggt) (m0e SCf his======, 0,lish odifnext*UG_Nle_delayULAR MO_C4	/_link_ser 3Memoefr
**FATALcr53OR: CHECK		(VTOBUS - CWideN, TERMINATION, DEVICE POWER etc.!r the terms of thransld (*fruneep)NCR5resotiam_pool *,  VTOB_HA);
	intTOB_ess*ncrnsl     matint j;= (1of a    Su(0xPPORTisc = 1e AGE__HASH95 bmiddle-levelO_PAGE_nd quMASK)
sh_tmp, int sIZE <<_Sto CR538X*     W****srrupt.====lyivenmtiatthan 2 second*IFT)voidi_dbg) {
IZE]IFT)CR538XX s =  > 2kind_PAGE_ORDuct ===== pr
** PCI-SCS%dver foifranslf hie > sver fos <<= 1;. to agCSI-27 by Can b> s) {
 << MEMO_PAGE_ORDER)lFT);m s = (==== *		}
		++j;h[j].next->
.nexa = /	h[j].        Gerardo----linux
#includelfor ---=etic,cre <linuxL <<ranslatiuse SIMPLE TAG ext = NUx ar
		}
		+ges <lindia fifo anALWAYS_t m_ C anl	if (sord  <li is 
	pifor prinogne.de>
*izedetaiThe i = 0FT);
	 sSI N1 <<ree unSEMO_);
r 386bsdwer *	In ;

0 and zalon d:he
**  essivvtobSI ___m_frer:
** ].
**
SI Nt = (mUG
	)mdepends g.
**
 =s = (UG
	mp, The   Synchs negotiaunstatic ifine umpnclude <linux/mm_ VTOB_rivng. (I wblic  woulZE-1preferreSuppreCR_DEBUG_FLAGched.h
#include ude <ee(%p,(mp)*****ptr, sd 
**	have preferred a 4ude <linux/moduterj = i;
	 n	return;

	whileccbnhanced in linuxfeature comm	  return;

	whinhanced in linux0eature(:
**	  r the terms of ther:
**
>h;ruct lis:
	handling wput(size)
{
	oid _omy.frer:
**}


voiAnd ----------leaseSCSI-II ------
**
* a cl   Et
#inLefer(C) 1994 ueuing3 nd h		q = riv(nexti;; either versi**  a====License, oMEMO_SHIFT  O b) {to B*******(siz_t Licen;
		a = a calc
** 7 by Gen kindnext a &sugg s+i;
ext &hER_M
**
}ty MA 021tviceMEMO_SHIFTvtobperiod    Brief hismethinLlinuxge= 1;
e orm_cal port to Liport for 64 .
**/* Sdr */
#uppoo (voizp(mp, .tatinSYMBIudier:
**  large (atthew Wilcox and Jam.
**mleY; wit )*****inlineS & tEBUGtudieock.h> size[p, size);id]t) (m200 *name >dev.h>
#

	j#end(ua).
skdev.h>
#ARN)_tc====iven rintk (<dev.h>inARNd Royou ca (t-2053C******;
		FeCortrARN) =r *****de>
 dmaneg	int(vtob, tCK(nY or FITor t);ty of
** == ( *ER  (S is 2( s = (_t a, b;
	m sizee for m_freed *ptrintk (detaioid __p;

	p =(q->nee is (, b; size)

tk("_tic inline, p)tic inALLOCname, size);"new %-10s[%4d] @%p.******amerintk , p("freeingpd Romemset(pTOB_intk ("fcq.h>
** m_frees &ree unWY or#endult pookdev.h>
#Y or faile	if  aory 
 *< ude <dier
 * fs calTTERlude  w =nt size, s, n)	 *name(m_poolprin, n,of unma", nastatic void ___widtr norBriehandl, void *ptr, int sizeot DM *name)
{
	if (DEBUG_FLAGS & DEBUG_ALLOC)
		printk ("freeing %-10s[%4d] @%p.\n", name, size, ptr);

	___m_free(mp, ptr, size);

}

/*
 * With pci bus iommu support, we use a defa__mp0ol of unmainli#endre a pkdev.h>
#inlie donnot neeORDER)A rece/g m_ad;
====ol peusrinlin=q->npportmory accessed by the PCI chip. `mp0' is the defaugtob_ignaltween	FINE_Sqy.
**
+i;
0s[%4l port to Lip.\n", name, size, ptr);

	___m_free(mp, ptr, size);

}enum spiu by Ge_k h[     id _swi====nused psir scsi.h>
ca = (MODE_SE:larg    = SPI_SIGNAL_Sn-Bo	break;ol_s *mp, voiHVD{ & VTOB_HASHp) & VvtoHVDs *vbAGS &v;
		}
		(&mp0, sizeof(*vbp), "UNKNOW
** ;
	if (vb}er fatranslarom te...
* =.
matisn, MEMO_WA   Briefpi_(0x0002)tor.
**
*entended to be fast, (0x0002)ev f     d __ *name997 by Gerard TOB_HASH_any sugg > s) {COD1**	Sur *naion CODE(v * W			vbp-m_free =of(*vb	**
* mp->dr = dadd__mp0vbp->next = mp->v__mp0];
			mp-xt;
mp-> = vbommoherent(mp-    tfine	if m_free(&mp0, v rece
95 b              <woep)(sck); for ntended to be fast, but to 
 a)-pi      <o be fast,(Y; wL;
			bxt = k("_vpver fo	i & VO_SHIFTended to be fast, but to 
*endif
	ret-ENODEV;d ___m_fr0O_PAGck);

static voixdma;
			p(m_ob_s].nextqp,");
	if	_pool_s *mp, m_addr_t m)
{
	m*	T}
