/*
 *  linux/mm/page_alloc.c
 *
 *  Manages the free list, the system allocates free pages here.
 *  Note that kmalloc() lives in slab.c
 *
 *  Copyright (C) 1991, 1992, 1993, 1994  Linus Torvalds
 *  Swap reorganised 29.12.95, Stephen Tweedie
 *  Support of BIGMEM added by Gerhard Wichert, Siemens AG, July 1999
 *  Reshaped it to be a zoned allocator, Ingo Molnar, Red Hat, 1999
 *  Discontiguous memory support, Kanoj Sarcar, SGI, Nov 1999
 *  Zone balancing, Kanoj Sarcar, SGI, Jan 2000
 *  Per cpu hot/cold page lists, bulk allocation, Martin J. Bligh, Sept 2002
 *          (lots of bits borrowed from Ingo Molnar & Andrew Morton)
 */

#include <linux/stddef.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/interrupt.h>
#include <linux/pagemap.h>
#include <linux/jiffies.h>
#include <linux/bootmem.h>
#include <linux/compiler.h>
#include <linux/kernel.h>
#include <linux/kmemcheck.h>
#include <linux/module.h>
#include <linux/suspend.h>
#include <linux/pagevec.h>
#include <linux/blkdev.h>
#include <linux/slab.h>
#include <linux/oom.h>
#include <linux/notifier.h>
#include <linux/topology.h>
#include <linux/sysctl.h>
#include <linux/cpu.h>
#include <linux/cpuset.h>
#include <linux/memory_hotplug.h>
#include <linux/nodemask.h>
#include <linux/vmalloc.h>
#include <linux/mempolicy.h>
#include <linux/stop_machine.h>
#include <linux/sort.h>
#include <linux/pfn.h>
#include <linux/backing-dev.h>
#include <linux/fault-inject.h>
#include <linux/page-isolation.h>
#include <linux/page_cgroup.h>
#include <linux/debugobjects.h>
#include <linux/kmemleak.h>
#include <trace/events/kmem.h>

#include <asm/tlbflush.h>
#include <asm/div64.h>
#include "internal.h"

/*
 * Array of node states.
 */
nodemask_t node_states[NR_NODE_STATES] __read_mostly = {
	[N_POSSIBLE] = NODE_MASK_ALL,
	[N_ONLINE] = { { [0] = 1UL } },
#ifndef CONFIG_NUMA
	[N_NORMAL_MEMORY] = { { [0] = 1UL } },
#ifdef CONFIG_HIGHMEM
	[N_HIGH_MEMORY] = { { [0] = 1UL } },
#endif
	[N_CPU] = { { [0] = 1UL } },
#endif	/* NUMA */
};
EXPORT_SYMBOL(node_states);

unsigned long totalram_pages __read_mostly;
unsigned long totalreserve_pages __read_mostly;
int percpu_pagelist_fraction;
gfp_t gfp_allowed_mask __read_mostly = GFP_BOOT_MASK;

#ifdef CONFIG_HUGETLB_PAGE_SIZE_VARIABLE
int pageblock_order __read_mostly;
#endif

static void __free_pages_ok(struct page *page, unsigned int order);

/*
 * results with 256, 32 in the lowmem_reserve sysctl:
 *	1G machine -> (16M dma, 800M-16M normal, 1G-800M high)
 *	1G machine -> (16M dma, 784M normal, 224M high)
 *	NORMAL allocation will leave 784M/256 of ram reserved in the ZONE_DMA
 *	HIGHMEM allocation will leave 224M/32 of ram reserved in ZONE_NORMAL
 *	HIGHMEM allocation will (224M+784M)/256 of ram reserved in ZONE_DMA
 *
 * TBD: should special case ZONE_DMA32 machines here - in those we normally
 * don't need any ZONE_NORMAL reservation
 */
int sysctl_lowmem_reserve_ratio[MAX_NR_ZONES-1] = {
#ifdef CONFIG_ZONE_DMA
	 256,
#endif
#ifdef CONFIG_ZONE_DMA32
	 256,
#endif
#ifdef CONFIG_HIGHMEM
	 32,
#endif
	 32,
};

EXPORT_SYMBOL(totalram_pages);

static char * const zone_names[MAX_NR_ZONES] = {
#ifdef CONFIG_ZONE_DMA
	 "DMA",
#endif
#ifdef CONFIG_ZONE_DMA32
	 "DMA32",
#endif
	 "Normal",
#ifdef CONFIG_HIGHMEM
	 "HighMem",
#endif
	 "Movable",
};

int min_free_kbytes = 1024;

static unsigned long __meminitdata nr_kernel_pages;
static unsigned long __meminitdata nr_all_pages;
static unsigned long __meminitdata dma_reserve;

#ifdef CONFIG_ARCH_POPULATES_NODE_MAP
  /*
   * MAX_ACTIVE_REGIONS determines the maximum number of distinct
   * ranges of memory (RAM) that may be registered with add_active_range().
   * Ranges passed to add_active_range() will be merged if possible
   * so the number of times add_active_range() can be called is
   * related to the number of nodes and the number of holes
   */
  #ifdef CONFIG_MAX_ACTIVE_REGIONS
    /* Allow an architecture to set MAX_ACTIVE_REGIONS to save memory */
    #define MAX_ACTIVE_REGIONS CONFIG_MAX_ACTIVE_REGIONS
  #else
    #if MAX_NUMNODES >= 32
      /* If there can be many nodes, allow up to 50 holes per node */
      #define MAX_ACTIVE_REGIONS (MAX_NUMNODES*50)
    #else
      /* By default, allow up to 256 distinct regions */
      #define MAX_ACTIVE_REGIONS 256
    #endif
  #endif

  static struct node_active_region __meminitdata early_node_map[MAX_ACTIVE_REGIONS];
  static int __meminitdata nr_nodemap_entries;
  static unsigned long __meminitdata arch_zone_lowest_possible_pfn[MAX_NR_ZONES];
  static unsigned long __meminitdata arch_zone_highest_possible_pfn[MAX_NR_ZONES];
  static unsigned long __initdata required_kernelcore;
  static unsigned long __initdata required_movablecore;
  static unsigned long __meminitdata zone_movable_pfn[MAX_NUMNODES];

  /* movable_zone is the "real" zone pages in ZONE_MOVABLE are taken from */
  int movable_zone;
  EXPORT_SYMBOL(movable_zone);
#endif /* CONFIG_ARCH_POPULATES_NODE_MAP */

#if MAX_NUMNODES > 1
int nr_node_ids __read_mostly = MAX_NUMNODES;
int nr_online_nodes __read_mostly = 1;
EXPORT_SYMBOL(nr_node_ids);
EXPORT_SYMBOL(nr_online_nodes);
#endif

int page_group_by_mobility_disabled __read_mostly;

static void set_pageblock_migratetype(struct page *page, int migratetype)
{

	if (unlikely(page_group_by_mobility_disabled))
		migratetype = MIGRATE_UNMOVABLE;

	set_pageblock_flags_group(page, (unsigned long)migratetype,
					PB_migrate, PB_migrate_end);
}

bool oom_killer_disabled __read_mostly;

#ifdef CONFIG_DEBUG_VM
static int page_outside_zone_boundaries(struct zone *zone, struct page *page)
{
	int ret = 0;
	unsigned seq;
	unsigned long pfn = page_to_pfn(page);

	do {
		seq = zone_span_seqbegin(zone);
		if (pfn >= zone->zone_start_pfn + zone->spanned_pages)
			ret = 1;
		else if (pfn < zone->zone_start_pfn)
			ret = 1;
	} while (zone_span_seqretry(zone, seq));

	return ret;
}

static int page_is_consistent(struct zone *zone, struct page *page)
{
	if (!pfn_valid_within(page_to_pfn(page)))
		return 0;
	if (zone != page_zone(page))
		return 0;

	return 1;
}
/*
 * Temporary debugging check for pages not lying within a given zone.
 */
static int bad_range(struct zone *zone, struct page *page)
{
	if (page_outside_zone_boundaries(zone, page))
		return 1;
	if (!page_is_consistent(zone, page))
		return 1;

	return 0;
}
#else
static inline int bad_range(struct zone *zone, struct page *page)
{
	return 0;
}
#endif

static void bad_page(struct page *page)
{
	static unsigned long resume;
	static unsigned long nr_shown;
	static unsigned long nr_unshown;

	/* Don't complain about poisoned pages */
	if (PageHWPoison(page)) {
		__ClearPageBuddy(page);
		return;
	}

	/*
	 * Allow a burst of 60 reports, then keep quiet for that minute;
	 * or allow a steady drip of one report per second.
	 */
	if (nr_shown == 60) {
		if (time_before(jiffies, resume)) {
			nr_unshown++;
			goto out;
		}
		if (nr_unshown) {
			printk(KERN_ALERT
			      "BUG: Bad page state: %lu messages suppressed\n",
				nr_unshown);
			nr_unshown = 0;
		}
		nr_shown = 0;
	}
	if (nr_shown++ == 0)
		resume = jiffies + 60 * HZ;

	printk(KERN_ALERT "BUG: Bad page state in process %s  pfn:%05lx\n",
		current->comm, page_to_pfn(page));
	printk(KERN_ALERT
		"page:%p flags:%p count:%d mapcount:%d mapping:%p index:%lx\n",
		page, (void *)page->flags, page_count(page),
		page_mapcount(page), page->mapping, page->index);

	dump_stack();
out:
	/* Leave bad fields for debug, except PageBuddy could make trouble */
	__ClearPageBuddy(page);
	add_taint(TAINT_BAD_PAGE);
}

/*
 * Higher-order pages are called "compound pages".  They are structured thusly:
 *
 * The first PAGE_SIZE page is called the "head page".
 *
 * The remaining PAGE_SIZE pages are called "tail pages".
 *
 * All pages have PG_compound set.  All pages have their ->private pointing at
 * the head page (even the head page has this).
 *
 * The first tail page's ->lru.next holds the address of the compound page's
 * put_page() function.  Its ->lru.prev holds the order of allocation.
 * This usage means that zero-order pages may not be compound.
 */

static void free_compound_page(struct page *page)
{
	__free_pages_ok(page, compound_order(page));
}

void prep_compound_page(struct page *page, unsigned long order)
{
	int i;
	int nr_pages = 1 << order;

	set_compound_page_dtor(page, free_compound_page);
	set_compound_order(page, order);
	__SetPageHead(page);
	for (i = 1; i < nr_pages; i++) {
		struct page *p = page + i;

		__SetPageTail(p);
		p->first_page = page;
	}
}

static int destroy_compound_page(struct page *page, unsigned long order)
{
	int i;
	int nr_pages = 1 << order;
	int bad = 0;

	if (unlikely(compound_order(page) != order) ||
	    unlikely(!PageHead(page))) {
		bad_page(page);
		bad++;
	}

	__ClearPageHead(page);

	for (i = 1; i < nr_pages; i++) {
		struct page *p = page + i;

		if (unlikely(!PageTail(p) || (p->first_page != page))) {
			bad_page(page);
			bad++;
		}
		__ClearPageTail(p);
	}

	return bad;
}

static inline void prep_zero_page(struct page *page, int order, gfp_t gfp_flags)
{
	int i;

	/*
	 * clear_highpage() will use KM_USER0, so it's a bug to use __GFP_ZERO
	 * and __GFP_HIGHMEM from hard or soft interrupt context.
	 */
	VM_BUG_ON((gfp_flags & __GFP_HIGHMEM) && in_interrupt());
	for (i = 0; i < (1 << order); i++)
		clear_highpage(page + i);
}

static inline void set_page_order(struct page *page, int order)
{
	set_page_private(page, order);
	__SetPageBuddy(page);
}

static inline void rmv_page_order(struct page *page)
{
	__ClearPageBuddy(page);
	set_page_private(page, 0);
}

/*
 * Locate the struct page for both the matching buddy in our
 * pair (buddy1) and the combined O(n+1) page they form (page).
 *
 * 1) Any buddy B1 will have an order O twin B2 which satisfies
 * the following equation:
 *     B2 = B1 ^ (1 << O)
 * For example, if the starting buddy (buddy2) is #8 its order
 * 1 buddy is #10:
 *     B2 = 8 ^ (1 << 1) = 8 ^ 2 = 10
 *
 * 2) Any buddy B will have an order O+1 parent P which
 * satisfies the following equation:
 *     P = B & ~(1 << O)
 *
 * Assumption: *_mem_map is contiguous at least up to MAX_ORDER
 */
static inline struct page *
__page_find_buddy(struct page *page, unsigned long page_idx, unsigned int order)
{
	unsigned long buddy_idx = page_idx ^ (1 << order);

	return page + (buddy_idx - page_idx);
}

static inline unsigned long
__find_combined_index(unsigned long page_idx, unsigned int order)
{
	return (page_idx & ~(1 << order));
}

/*
 * This function checks whether a page is free && is the buddy
 * we can do coalesce a page and its buddy if
 * (a) the buddy is not in a hole &&
 * (b) the buddy is in the buddy system &&
 * (c) a page and its buddy have the same order &&
 * (d) a page and its buddy are in the same zone.
 *
 * For recording whether a page is in the buddy system, we use PG_buddy.
 * Setting, clearing, and testing PG_buddy is serialized by zone->lock.
 *
 * For recording page's order, we use page_private(page).
 */
static inline int page_is_buddy(struct page *page, struct page *buddy,
								int order)
{
	if (!pfn_valid_within(page_to_pfn(buddy)))
		return 0;

	if (page_zone_id(page) != page_zone_id(buddy))
		return 0;

	if (PageBuddy(buddy) && page_order(buddy) == order) {
		VM_BUG_ON(page_count(buddy) != 0);
		return 1;
	}
	return 0;
}

/*
 * Freeing function for a buddy system allocator.
 *
 * The concept of a buddy system is to maintain direct-mapped table
 * (containing bit values) for memory blocks of various "orders".
 * The bottom level table contains the map for the smallest allocatable
 * units of memory (here, pages), and each level above it describes
 * pairs of units from the levels below, hence, "buddies".
 * At a high level, all that happens here is marking the table entry
 * at the bottom level available, and propagating the changes upward
 * as necessary, plus some accounting needed to play nicely with other
 * parts of the VM system.
 * At each level, we keep a list of pages, which are heads of continuous
 * free pages of length of (1 << order) and marked with PG_buddy. Page's
 * order is recorded in page_private(page) field.
 * So when we are allocating or freeing one, we can derive the state of the
 * other.  That is, if we allocate a small block, and both were   
 * free, the remainder of the region must be split into blocks.   
 * If a block is freed, and its buddy is also free, then this
 * triggers coalescing into a block of larger size.            
 *
 * -- wli
 */

static inline void __free_one_page(struct page *page,
		struct zone *zone, unsigned int order,
		int migratetype)
{
	unsigned long page_idx;

	if (unlikely(PageCompound(page)))
		if (unlikely(destroy_compound_page(page, order)))
			return;

	VM_BUG_ON(migratetype == -1);

	page_idx = page_to_pfn(page) & ((1 << MAX_ORDER) - 1);

	VM_BUG_ON(page_idx & ((1 << order) - 1));
	VM_BUG_ON(bad_range(zone, page));

	while (order < MAX_ORDER-1) {
		unsigned long combined_idx;
		struct page *buddy;

		buddy = __page_find_buddy(page, page_idx, order);
		if (!page_is_buddy(page, buddy, order))
			break;

		/* Our buddy is free, merge with it and move up one order. */
		list_del(&buddy->lru);
		zone->free_area[order].nr_free--;
		rmv_page_order(buddy);
		combined_idx = __find_combined_index(page_idx, order);
		page = page + (combined_idx - page_idx);
		page_idx = combined_idx;
		order++;
	}
	set_page_order(page, order);
	list_add(&page->lru,
		&zone->free_area[order].free_list[migratetype]);
	zone->free_area[order].nr_free++;
}

#ifdef CONFIG_HAVE_MLOCKED_PAGE_BIT
/*
 * free_page_mlock() -- clean up attempts to free and mlocked() page.
 * Page should not be on lru, so no need to fix that up.
 * free_pages_check() will verify...
 */
static inline void free_page_mlock(struct page *page)
{
	__dec_zone_page_state(page, NR_MLOCK);
	__count_vm_event(UNEVICTABLE_MLOCKFREED);
}
#else
static void free_page_mlock(struct page *page) { }
#endif

static inline int free_pages_check(struct page *page)
{
	if (unlikely(page_mapcount(page) |
		(page->mapping != NULL)  |
		(atomic_read(&page->_count) != 0) |
		(page->flags & PAGE_FLAGS_CHECK_AT_FREE))) {
		bad_page(page);
		return 1;
	}
	if (page->flags & PAGE_FLAGS_CHECK_AT_PREP)
		page->flags &= ~PAGE_FLAGS_CHECK_AT_PREP;
	return 0;
}

/*
 * Frees a number of pages from the PCP lists
 * Assumes all pages on list are in same zone, and of same order.
 * count is the number of pages to free.
 *
 * If the zone was previously in an "all pages pinned" state then look to
 * see if this freeing clears that state.
 *
 * And clear the zone's pages_scanned counter, to hold off the "all pages are
 * pinned" detection logic.
 */
static void free_pcppages_bulk(struct zone *zone, int count,
					struct per_cpu_pages *pcp)
{
	int migratetype = 0;
	int batch_free = 0;

	spin_lock(&zone->lock);
	zone_clear_flag(zone, ZONE_ALL_UNRECLAIMABLE);
	zone->pages_scanned = 0;

	__mod_zone_page_state(zone, NR_FREE_PAGES, count);
	while (count) {
		struct page *page;
		struct list_head *list;

		/*
		 * Remove pages from lists in a round-robin fashion. A
		 * batch_free count is maintained that is incremented when an
		 * empty list is encountered.  This is so more pages are freed
		 * off fuller lists instead of spinning excessively around empty
		 * lists
		 */
		do {
			batch_free++;
			if (++migratetype == MIGRATE_PCPTYPES)
				migratetype = 0;
			list = &pcp->lists[migratetype];
		} while (list_empty(list));

		do {
			page = list_entry(list->prev, struct page, lru);
			/* must delete as __free_one_page list manipulates */
			list_del(&page->lru);
			__free_one_page(page, zone, 0, migratetype);
			trace_mm_page_pcpu_drain(page, 0, migratetype);
		} while (--count && --batch_free && !list_empty(list));
	}
	spin_unlock(&zone->lock);
}

static void free_one_page(struct zone *zone, struct page *page, int order,
				int migratetype)
{
	spin_lock(&zone->lock);
	zone_clear_flag(zone, ZONE_ALL_UNRECLAIMABLE);
	zone->pages_scanned = 0;

	__mod_zone_page_state(zone, NR_FREE_PAGES, 1 << order);
	__free_one_page(page, zone, order, migratetype);
	spin_unlock(&zone->lock);
}

static void __free_pages_ok(struct page *page, unsigned int order)
{
	unsigned long flags;
	int i;
	int bad = 0;
	int wasMlocked = __TestClearPageMlocked(page);

	kmemcheck_free_shadow(page, order);

	for (i = 0 ; i < (1 << order) ; ++i)
		bad += free_pages_check(page + i);
	if (bad)
		return;

	if (!PageHighMem(page)) {
		debug_check_no_locks_freed(page_address(page),PAGE_SIZE<<order);
		debug_check_no_obj_freed(page_address(page),
					   PAGE_SIZE << order);
	}
	arch_free_page(page, order);
	kernel_map_pages(page, 1 << order, 0);

	local_irq_save(flags);
	if (unlikely(wasMlocked))
		free_page_mlock(page);
	__count_vm_events(PGFREE, 1 << order);
	free_one_page(page_zone(page), page, order,
					get_pageblock_migratetype(page));
	local_irq_restore(flags);
}

/*
 * permit the bootmem allocator to evade page validation on high-order frees
 */
void __meminit __free_pages_bootmem(struct page *page, unsigned int order)
{
	if (order == 0) {
		__ClearPageReserved(page);
		set_page_count(page, 0);
		set_page_refcounted(page);
		__free_page(page);
	} else {
		int loop;

		prefetchw(page);
		for (loop = 0; loop < BITS_PER_LONG; loop++) {
			struct page *p = &page[loop];

			if (loop + 1 < BITS_PER_LONG)
				prefetchw(p + 1);
			__ClearPageReserved(p);
			set_page_count(p, 0);
		}

		set_page_refcounted(page);
		__free_pages(page, order);
	}
}


/*
 * The order of subdivision here is critical for the IO subsystem.
 * Please do not alter this order without good reasons and regression
 * testing. Specifically, as large blocks of memory are subdivided,
 * the order in which smaller blocks are delivered depends on the order
 * they're subdivided in this function. This is the primary factor
 * influencing the order in which pages are delivered to the IO
 * subsystem according to empirical testing, and this is also justified
 * by considering the behavior of a buddy system containing a single
 * large block of memory acted on by a series of small allocations.
 * This behavior is a critical factor in sglist merging's success.
 *
 * -- wli
 */
static inline void expand(struct zone *zone, struct page *page,
	int low, int high, struct free_area *area,
	int migratetype)
{
	unsigned long size = 1 << high;

	while (high > low) {
		area--;
		high--;
		size >>= 1;
		VM_BUG_ON(bad_range(zone, &page[size]));
		list_add(&page[size].lru, &area->free_list[migratetype]);
		area->nr_free++;
		set_page_order(&page[size], high);
	}
}

/*
 * This page is about to be returned from the page allocator
 */
static inline int check_new_page(struct page *page)
{
	if (unlikely(page_mapcount(page) |
		(page->mapping != NULL)  |
		(atomic_read(&page->_count) != 0)  |
		(page->flags & PAGE_FLAGS_CHECK_AT_PREP))) {
		bad_page(page);
		return 1;
	}
	return 0;
}

static int prep_new_page(struct page *page, int order, gfp_t gfp_flags)
{
	int i;

	for (i = 0; i < (1 << order); i++) {
		struct page *p = page + i;
		if (unlikely(check_new_page(p)))
			return 1;
	}

	set_page_private(page, 0);
	set_page_refcounted(page);

	arch_alloc_page(page, order);
	kernel_map_pages(page, 1 << order, 1);

	if (gfp_flags & __GFP_ZERO)
		prep_zero_page(page, order, gfp_flags);

	if (order && (gfp_flags & __GFP_COMP))
		prep_compound_page(page, order);

	return 0;
}

/*
 * Go through the free lists for the given migratetype and remove
 * the smallest available page from the freelists
 */
static inline
struct page *__rmqueue_smallest(struct zone *zone, unsigned int order,
						int migratetype)
{
	unsigned int current_order;
	struct free_area * area;
	struct page *page;

	/* Find a page of the appropriate size in the preferred list */
	for (current_order = order; current_order < MAX_ORDER; ++current_order) {
		area = &(zone->free_area[current_order]);
		if (list_empty(&area->free_list[migratetype]))
			continue;

		page = list_entry(area->free_list[migratetype].next,
							struct page, lru);
		list_del(&page->lru);
		rmv_page_order(page);
		area->nr_free--;
		expand(zone, page, order, current_order, area, migratetype);
		return page;
	}

	return NULL;
}


/*
 * This array describes the order lists are fallen back to when
 * the free lists for the desirable migrate type are depleted
 */
static int fallbacks[MIGRATE_TYPES][MIGRATE_TYPES-1] = {
	[MIGRATE_UNMOVABLE]   = { MIGRATE_RECLAIMABLE, MIGRATE_MOVABLE,   MIGRATE_RESERVE },
	[MIGRATE_RECLAIMABLE] = { MIGRATE_UNMOVABLE,   MIGRATE_MOVABLE,   MIGRATE_RESERVE },
	[MIGRATE_MOVABLE]     = { MIGRATE_RECLAIMABLE, MIGRATE_UNMOVABLE, MIGRATE_RESERVE },
	[MIGRATE_RESERVE]     = { MIGRATE_RESERVE,     MIGRATE_RESERVE,   MIGRATE_RESERVE }, /* Never used */
};

/*
 * Move the free pages in a range to the free lists of the requested type.
 * Note that start_page and end_pages are not aligned on a pageblock
 * boundary. If alignment is required, use move_freepages_block()
 */
static int move_freepages(struct zone *zone,
			  struct page *start_page, struct page *end_page,
			  int migratetype)
{
	struct page *page;
	unsigned long order;
	int pages_moved = 0;

#ifndef CONFIG_HOLES_IN_ZONE
	/*
	 * page_zone is not safe to call in this context when
	 * CONFIG_HOLES_IN_ZONE is set. This bug check is probably redundant
	 * anyway as we check zone boundaries in move_freepages_block().
	 * Remove at a later date when no bug reports exist related to
	 * grouping pages by mobility
	 */
	BUG_ON(page_zone(start_page) != page_zone(end_page));
#endif

	for (page = start_page; page <= end_page;) {
		/* Make sure we are not inadvertently changing nodes */
		VM_BUG_ON(page_to_nid(page) != zone_to_nid(zone));

		if (!pfn_valid_within(page_to_pfn(page))) {
			page++;
			continue;
		}

		if (!PageBuddy(page)) {
			page++;
			continue;
		}

		order = page_order(page);
		list_del(&page->lru);
		list_add(&page->lru,
			&zone->free_area[order].free_list[migratetype]);
		page += 1 << order;
		pages_moved += 1 << order;
	}

	return pages_moved;
}

static int move_freepages_block(struct zone *zone, struct page *page,
				int migratetype)
{
	unsigned long start_pfn, end_pfn;
	struct page *start_page, *end_page;

	start_pfn = page_to_pfn(page);
	start_pfn = start_pfn & ~(pageblock_nr_pages-1);
	start_page = pfn_to_page(start_pfn);
	end_page = start_page + pageblock_nr_pages - 1;
	end_pfn = start_pfn + pageblock_nr_pages - 1;

	/* Do not cross zone boundaries */
	if (start_pfn < zone->zone_start_pfn)
		start_page = page;
	if (end_pfn >= zone->zone_start_pfn + zone->spanned_pages)
		return 0;

	return move_freepages(zone, start_page, end_page, migratetype);
}

static void change_pageblock_range(struct page *pageblock_page,
					int start_order, int migratetype)
{
	int nr_pageblocks = 1 << (start_order - pageblock_order);

	while (nr_pageblocks--) {
		set_pageblock_migratetype(pageblock_page, migratetype);
		pageblock_page += pageblock_nr_pages;
	}
}

/* Remove an element from the buddy allocator from the fallback list */
static inline struct page *
__rmqueue_fallback(struct zone *zone, int order, int start_migratetype)
{
	struct free_area * area;
	int current_order;
	struct page *page;
	int migratetype, i;

	/* Find the largest possible block of pages in the other list */
	for (current_order = MAX_ORDER-1; current_order >= order;
						--current_order) {
		for (i = 0; i < MIGRATE_TYPES - 1; i++) {
			migratetype = fallbacks[start_migratetype][i];

			/* MIGRATE_RESERVE handled later if necessary */
			if (migratetype == MIGRATE_RESERVE)
				continue;

			area = &(zone->free_area[current_order]);
			if (list_empty(&area->free_list[migratetype]))
				continue;

			page = list_entry(area->free_list[migratetype].next,
					struct page, lru);
			area->nr_free--;

			/*
			 * If breaking a large block of pages, move all free
			 * pages to the preferred allocation list. If falling
			 * back for a reclaimable kernel allocation, be more
			 * agressive about taking ownership of free pages
			 */
			if (unlikely(current_order >= (pageblock_order >> 1)) ||
					start_migratetype == MIGRATE_RECLAIMABLE ||
					page_group_by_mobility_disabled) {
				unsigned long pages;
				pages = move_freepages_block(zone, page,
								start_migratetype);

				/* Claim the whole block if over half of it is free */
				if (pages >= (1 << (pageblock_order-1)) ||
						page_group_by_mobility_disabled)
					set_pageblock_migratetype(page,
								start_migratetype);

				migratetype = start_migratetype;
			}

			/* Remove the page from the freelists */
			list_del(&page->lru);
			rmv_page_order(page);

			/* Take ownership for orders >= pageblock_order */
			if (current_order >= pageblock_order)
				change_pageblock_range(page, current_order,
							start_migratetype);

			expand(zone, page, order, current_order, area, migratetype);

			trace_mm_page_alloc_extfrag(page, order, current_order,
				start_migratetype, migratetype);

			return page;
		}
	}

	return NULL;
}

/*
 * Do the hard work of removing an element from the buddy allocator.
 * Call me with the zone->lock already held.
 */
static struct page *__rmqueue(struct zone *zone, unsigned int order,
						int migratetype)
{
	struct page *page;

retry_reserve:
	page = __rmqueue_smallest(zone, order, migratetype);

	if (unlikely(!page) && migratetype != MIGRATE_RESERVE) {
		page = __rmqueue_fallback(zone, order, migratetype);

		/*
		 * Use MIGRATE_RESERVE rather than fail an allocation. goto
		 * is used because __rmqueue_smallest is an inline function
		 * and we want just one call site
		 */
		if (!page) {
			migratetype = MIGRATE_RESERVE;
			goto retry_reserve;
		}
	}

	trace_mm_page_alloc_zone_locked(page, order, migratetype);
	return page;
}

/* 
 * Obtain a specified number of elements from the buddy allocator, all under
 * a single hold of the lock, for efficiency.  Add them to the supplied list.
 * Returns the number of new pages which were placed at *list.
 */
static int rmqueue_bulk(struct zone *zone, unsigned int order, 
			unsigned long count, struct list_head *list,
			int migratetype, int cold)
{
	int i;
	
	spin_lock(&zone->lock);
	for (i = 0; i < count; ++i) {
		struct page *page = __rmqueue(zone, order, migratetype);
		if (unlikely(page == NULL))
			break;

		/*
		 * Split buddy pages returned by expand() are received here
		 * in physical page order. The page is added to the callers and
		 * list and the list head then moves forward. From the callers
		 * perspective, the linked list is ordered by page number in
		 * some conditions. This is useful for IO devices that can
		 * merge IO requests if the physical pages are ordered
		 * properly.
		 */
		if (likely(cold == 0))
			list_add(&page->lru, list);
		else
			list_add_tail(&page->lru, list);
		set_page_private(page, migratetype);
		list = &page->lru;
	}
	__mod_zone_page_state(zone, NR_FREE_PAGES, -(i << order));
	spin_unlock(&zone->lock);
	return i;
}

#ifdef CONFIG_NUMA
/*
 * Called from the vmstat counter updater to drain pagesets of this
 * currently executing processor on remote nodes after they have
 * expired.
 *
 * Note that this function must be called with the thread pinned to
 * a single processor.
 */
void drain_zone_pages(struct zone *zone, struct per_cpu_pages *pcp)
{
	unsigned long flags;
	int to_drain;

	local_irq_save(flags);
	if (pcp->count >= pcp->batch)
		to_drain = pcp->batch;
	else
		to_drain = pcp->count;
	free_pcppages_bulk(zone, to_drain, pcp);
	pcp->count -= to_drain;
	local_irq_restore(flags);
}
#endif

/*
 * Drain pages of the indicated processor.
 *
 * The processor must either be the current processor and the
 * thread pinned to the current processor or a processor that
 * is not online.
 */
static void drain_pages(unsigned int cpu)
{
	unsigned long flags;
	struct zone *zone;

	for_each_populated_zone(zone) {
		struct per_cpu_pageset *pset;
		struct per_cpu_pages *pcp;

		pset = zone_pcp(zone, cpu);

		pcp = &pset->pcp;
		local_irq_save(flags);
		free_pcppages_bulk(zone, pcp->count, pcp);
		pcp->count = 0;
		local_irq_restore(flags);
	}
}

/*
 * Spill all of this CPU's per-cpu pages back into the buddy allocator.
 */
void drain_local_pages(void *arg)
{
	drain_pages(smp_processor_id());
}

/*
 * Spill all the per-cpu pages from all CPUs back into the buddy allocator
 */
void drain_all_pages(void)
{
	on_each_cpu(drain_local_pages, NULL, 1);
}

#ifdef CONFIG_HIBERNATION

void mark_free_pages(struct zone *zone)
{
	unsigned long pfn, max_zone_pfn;
	unsigned long flags;
	int order, t;
	struct list_head *curr;

	if (!zone->spanned_pages)
		return;

	spin_lock_irqsave(&zone->lock, flags);

	max_zone_pfn = zone->zone_start_pfn + zone->spanned_pages;
	for (pfn = zone->zone_start_pfn; pfn < max_zone_pfn; pfn++)
		if (pfn_valid(pfn)) {
			struct page *page = pfn_to_page(pfn);

			if (!swsusp_page_is_forbidden(page))
				swsusp_unset_page_free(page);
		}

	for_each_migratetype_order(order, t) {
		list_for_each(curr, &zone->free_area[order].free_list[t]) {
			unsigned long i;

			pfn = page_to_pfn(list_entry(curr, struct page, lru));
			for (i = 0; i < (1UL << order); i++)
				swsusp_set_page_free(pfn_to_page(pfn + i));
		}
	}
	spin_unlock_irqrestore(&zone->lock, flags);
}
#endif /* CONFIG_PM */

/*
 * Free a 0-order page
 */
static void free_hot_cold_page(struct page *page, int cold)
{
	struct zone *zone = page_zone(page);
	struct per_cpu_pages *pcp;
	unsigned long flags;
	int migratetype;
	int wasMlocked = __TestClearPageMlocked(page);

	kmemcheck_free_shadow(page, 0);

	if (PageAnon(page))
		page->mapping = NULL;
	if (free_pages_check(page))
		return;

	if (!PageHighMem(page)) {
		debug_check_no_locks_freed(page_address(page), PAGE_SIZE);
		debug_check_no_obj_freed(page_address(page), PAGE_SIZE);
	}
	arch_free_page(page, 0);
	kernel_map_pages(page, 1, 0);

	pcp = &zone_pcp(zone, get_cpu())->pcp;
	migratetype = get_pageblock_migratetype(page);
	set_page_private(page, migratetype);
	local_irq_save(flags);
	if (unlikely(wasMlocked))
		free_page_mlock(page);
	__count_vm_event(PGFREE);

	/*
	 * We only track unmovable, reclaimable and movable on pcp lists.
	 * Free ISOLATE pages back to the allocator because they are being
	 * offlined but treat RESERVE as movable pages so we can get those
	 * areas back if necessary. Otherwise, we may have to free
	 * excessively into the page allocator
	 */
	if (migratetype >= MIGRATE_PCPTYPES) {
		if (unlikely(migratetype == MIGRATE_ISOLATE)) {
			free_one_page(zone, page, 0, migratetype);
			goto out;
		}
		migratetype = MIGRATE_MOVABLE;
	}

	if (cold)
		list_add_tail(&page->lru, &pcp->lists[migratetype]);
	else
		list_add(&page->lru, &pcp->lists[migratetype]);
	pcp->count++;
	if (pcp->count >= pcp->high) {
		free_pcppages_bulk(zone, pcp->batch, pcp);
		pcp->count -= pcp->batch;
	}

out:
	local_irq_restore(flags);
	put_cpu();
}

void free_hot_page(struct page *page)
{
	trace_mm_page_free_direct(page, 0);
	free_hot_cold_page(page, 0);
}
	
/*
 * split_page takes a non-compound higher-order page, and splits it into
 * n (1<<order) sub-pages: page[0..n]
 * Each sub-page must be freed individually.
 *
 * Note: this is probably too low level an operation for use in drivers.
 * Please consult with lkml before using this in your driver.
 */
void split_page(struct page *page, unsigned int order)
{
	int i;

	VM_BUG_ON(PageCompound(page));
	VM_BUG_ON(!page_count(page));

#ifdef CONFIG_KMEMCHECK
	/*
	 * Split shadow pages too, because free(page[0]) would
	 * otherwise free the whole shadow.
	 */
	if (kmemcheck_page_is_tracked(page))
		split_page(virt_to_page(page[0].shadow), order);
#endif

	for (i = 1; i < (1 << order); i++)
		set_page_refcounted(page + i);
}

/*
 * Really, prep_compound_page() should be called from __rmqueue_bulk().  But
 * we cheat by calling it from here, in the order > 0 path.  Saves a branch
 * or two.
 */
static inline
struct page *buffered_rmqueue(struct zone *preferred_zone,
			struct zone *zone, int order, gfp_t gfp_flags,
			int migratetype)
{
	unsigned long flags;
	struct page *page;
	int cold = !!(gfp_flags & __GFP_COLD);
	int cpu;

again:
	cpu  = get_cpu();
	if (likely(order == 0)) {
		struct per_cpu_pages *pcp;
		struct list_head *list;

		pcp = &zone_pcp(zone, cpu)->pcp;
		list = &pcp->lists[migratetype];
		local_irq_save(flags);
		if (list_empty(list)) {
			pcp->count += rmqueue_bulk(zone, 0,
					pcp->batch, list,
					migratetype, cold);
			if (unlikely(list_empty(list)))
				goto failed;
		}

		if (cold)
			page = list_entry(list->prev, struct page, lru);
		else
			page = list_entry(list->next, struct page, lru);

		list_del(&page->lru);
		pcp->count--;
	} else {
		if (unlikely(gfp_flags & __GFP_NOFAIL)) {
			/*
			 * __GFP_NOFAIL is not to be used in new code.
			 *
			 * All __GFP_NOFAIL callers should be fixed so that they
			 * properly detect and handle allocation failures.
			 *
			 * We most definitely don't want callers attempting to
			 * allocate greater than order-1 page units with
			 * __GFP_NOFAIL.
			 */
			WARN_ON_ONCE(order > 1);
		}
		spin_lock_irqsave(&zone->lock, flags);
		page = __rmqueue(zone, order, migratetype);
		__mod_zone_page_state(zone, NR_FREE_PAGES, -(1 << order));
		spin_unlock(&zone->lock);
		if (!page)
			goto failed;
	}

	__count_zone_vm_events(PGALLOC, zone, 1 << order);
	zone_statistics(preferred_zone, zone);
	local_irq_restore(flags);
	put_cpu();

	VM_BUG_ON(bad_range(zone, page));
	if (prep_new_page(page, order, gfp_flags))
		goto again;
	return page;

failed:
	local_irq_restore(flags);
	put_cpu();
	return NULL;
}

/* The ALLOC_WMARK bits are used as an index to zone->watermark */
#define ALLOC_WMARK_MIN		WMARK_MIN
#define ALLOC_WMARK_LOW		WMARK_LOW
#define ALLOC_WMARK_HIGH	WMARK_HIGH
#define ALLOC_NO_WATERMARKS	0x04 /* don't check watermarks at all */

/* Mask to get the watermark bits */
#define ALLOC_WMARK_MASK	(ALLOC_NO_WATERMARKS-1)

#define ALLOC_HARDER		0x10 /* try to alloc harder */
#define ALLOC_HIGH		0x20 /* __GFP_HIGH set */
#define ALLOC_CPUSET		0x40 /* check for correct cpuset */

#ifdef CONFIG_FAIL_PAGE_ALLOC

static struct fail_page_alloc_attr {
	struct fault_attr attr;

	u32 ignore_gfp_highmem;
	u32 ignore_gfp_wait;
	u32 min_order;

#ifdef CONFIG_FAULT_INJECTION_DEBUG_FS

	struct dentry *ignore_gfp_highmem_file;
	struct dentry *ignore_gfp_wait_file;
	struct dentry *min_order_file;

#endif /* CONFIG_FAULT_INJECTION_DEBUG_FS */

} fail_page_alloc = {
	.attr = FAULT_ATTR_INITIALIZER,
	.ignore_gfp_wait = 1,
	.ignore_gfp_highmem = 1,
	.min_order = 1,
};

static int __init setup_fail_page_alloc(char *str)
{
	return setup_fault_attr(&fail_page_alloc.attr, str);
}
__setup("fail_page_alloc=", setup_fail_page_alloc);

static int should_fail_alloc_page(gfp_t gfp_mask, unsigned int order)
{
	if (order < fail_page_alloc.min_order)
		return 0;
	if (gfp_mask & __GFP_NOFAIL)
		return 0;
	if (fail_page_alloc.ignore_gfp_highmem && (gfp_mask & __GFP_HIGHMEM))
		return 0;
	if (fail_page_alloc.ignore_gfp_wait && (gfp_mask & __GFP_WAIT))
		return 0;

	return should_fail(&fail_page_alloc.attr, 1 << order);
}

#ifdef CONFIG_FAULT_INJECTION_DEBUG_FS

static int __init fail_page_alloc_debugfs(void)
{
	mode_t mode = S_IFREG | S_IRUSR | S_IWUSR;
	struct dentry *dir;
	int err;

	err = init_fault_attr_dentries(&fail_page_alloc.attr,
				       "fail_page_alloc");
	if (err)
		return err;
	dir = fail_page_alloc.attr.dentries.dir;

	fail_page_alloc.ignore_gfp_wait_file =
		debugfs_create_bool("ignore-gfp-wait", mode, dir,
				      &fail_page_alloc.ignore_gfp_wait);

	fail_page_alloc.ignore_gfp_highmem_file =
		debugfs_create_bool("ignore-gfp-highmem", mode, dir,
				      &fail_page_alloc.ignore_gfp_highmem);
	fail_page_alloc.min_order_file =
		debugfs_create_u32("min-order", mode, dir,
				   &fail_page_alloc.min_order);

	if (!fail_page_alloc.ignore_gfp_wait_file ||
            !fail_page_alloc.ignore_gfp_highmem_file ||
            !fail_page_alloc.min_order_file) {
		err = -ENOMEM;
		debugfs_remove(fail_page_alloc.ignore_gfp_wait_file);
		debugfs_remove(fail_page_alloc.ignore_gfp_highmem_file);
		debugfs_remove(fail_page_alloc.min_order_file);
		cleanup_fault_attr_dentries(&fail_page_alloc.attr);
	}

	return err;
}

late_initcall(fail_page_alloc_debugfs);

#endif /* CONFIG_FAULT_INJECTION_DEBUG_FS */

#else /* CONFIG_FAIL_PAGE_ALLOC */

static inline int should_fail_alloc_page(gfp_t gfp_mask, unsigned int order)
{
	return 0;
}

#endif /* CONFIG_FAIL_PAGE_ALLOC */

/*
 * Return 1 if free pages are above 'mark'. This takes into account the order
 * of the allocation.
 */
int zone_watermark_ok(struct zone *z, int order, unsigned long mark,
		      int classzone_idx, int alloc_flags)
{
	/* free_pages my go negative - that's OK */
	long min = mark;
	long free_pages = zone_page_state(z, NR_FREE_PAGES) - (1 << order) + 1;
	int o;

	if (alloc_flags & ALLOC_HIGH)
		min -= min / 2;
	if (alloc_flags & ALLOC_HARDER)
		min -= min / 4;

	if (free_pages <= min + z->lowmem_reserve[classzone_idx])
		return 0;
	for (o = 0; o < order; o++) {
		/* At the next order, this order's pages become unavailable */
		free_pages -= z->free_area[o].nr_free << o;

		/* Require fewer higher order pages to be free */
		min >>= 1;

		if (free_pages <= min)
			return 0;
	}
	return 1;
}

#ifdef CONFIG_NUMA
/*
 * zlc_setup - Setup for "zonelist cache".  Uses cached zone data to
 * skip over zones that are not allowed by the cpuset, or that have
 * been recently (in last second) found to be nearly full.  See further
 * comments in mmzone.h.  Reduces cache footprint of zonelist scans
 * that have to skip over a lot of full or unallowed zones.
 *
 * If the zonelist cache is present in the passed in zonelist, then
 * returns a pointer to the allowed node mask (either the current
 * tasks mems_allowed, or node_states[N_HIGH_MEMORY].)
 *
 * If the zonelist cache is not available for this zonelist, does
 * nothing and returns NULL.
 *
 * If the fullzones BITMAP in the zonelist cache is stale (more than
 * a second since last zap'd) then we zap it out (clear its bits.)
 *
 * We hold off even calling zlc_setup, until after we've checked the
 * first zone in the zonelist, on the theory that most allocations will
 * be satisfied from that first zone, so best to examine that zone as
 * quickly as we can.
 */
static nodemask_t *zlc_setup(struct zonelist *zonelist, int alloc_flags)
{
	struct zonelist_cache *zlc;	/* cached zonelist speedup info */
	nodemask_t *allowednodes;	/* zonelist_cache approximation */

	zlc = zonelist->zlcache_ptr;
	if (!zlc)
		return NULL;

	if (time_after(jiffies, zlc->last_full_zap + HZ)) {
		bitmap_zero(zlc->fullzones, MAX_ZONES_PER_ZONELIST);
		zlc->last_full_zap = jiffies;
	}

	allowednodes = !in_interrupt() && (alloc_flags & ALLOC_CPUSET) ?
					&cpuset_current_mems_allowed :
					&node_states[N_HIGH_MEMORY];
	return allowednodes;
}

/*
 * Given 'z' scanning a zonelist, run a couple of quick checks to see
 * if it is worth looking at further for free memory:
 *  1) Check that the zone isn't thought to be full (doesn't have its
 *     bit set in the zonelist_cache fullzones BITMAP).
 *  2) Check that the zones node (obtained from the zonelist_cache
 *     z_to_n[] mapping) is allowed in the passed in allowednodes mask.
 * Return true (non-zero) if zone is worth looking at further, or
 * else return false (zero) if it is not.
 *
 * This check -ignores- the distinction between various watermarks,
 * such as GFP_HIGH, GFP_ATOMIC, PF_MEMALLOC, ...  If a zone is
 * found to be full for any variation of these watermarks, it will
 * be considered full for up to one second by all requests, unless
 * we are so low on memory on all allowed nodes that we are forced
 * into the second scan of the zonelist.
 *
 * In the second scan we ignore this zonelist cache and exactly
 * apply the watermarks to all zones, even it is slower to do so.
 * We are low on memory in the second scan, and should leave no stone
 * unturned looking for a free page.
 */
static int zlc_zone_worth_trying(struct zonelist *zonelist, struct zoneref *z,
						nodemask_t *allowednodes)
{
	struct zonelist_cache *zlc;	/* cached zonelist speedup info */
	int i;				/* index of *z in zonelist zones */
	int n;				/* node that zone *z is on */

	zlc = zonelist->zlcache_ptr;
	if (!zlc)
		return 1;

	i = z - zonelist->_zonerefs;
	n = zlc->z_to_n[i];

	/* This zone is worth trying if it is allowed but not full */
	return node_isset(n, *allowednodes) && !test_bit(i, zlc->fullzones);
}

/*
 * Given 'z' scanning a zonelist, set the corresponding bit in
 * zlc->fullzones, so that subsequent attempts to allocate a page
 * from that zone don't waste time re-examining it.
 */
static void zlc_mark_zone_full(struct zonelist *zonelist, struct zoneref *z)
{
	struct zonelist_cache *zlc;	/* cached zonelist speedup info */
	int i;				/* index of *z in zonelist zones */

	zlc = zonelist->zlcache_ptr;
	if (!zlc)
		return;

	i = z - zonelist->_zonerefs;

	set_bit(i, zlc->fullzones);
}

#else	/* CONFIG_NUMA */

static nodemask_t *zlc_setup(struct zonelist *zonelist, int alloc_flags)
{
	return NULL;
}

static int zlc_zone_worth_trying(struct zonelist *zonelist, struct zoneref *z,
				nodemask_t *allowednodes)
{
	return 1;
}

static void zlc_mark_zone_full(struct zonelist *zonelist, struct zoneref *z)
{
}
#endif	/* CONFIG_NUMA */

/*
 * get_page_from_freelist goes through the zonelist trying to allocate
 * a page.
 */
static struct page *
get_page_from_freelist(gfp_t gfp_mask, nodemask_t *nodemask, unsigned int order,
		struct zonelist *zonelist, int high_zoneidx, int alloc_flags,
		struct zone *preferred_zone, int migratetype)
{
	struct zoneref *z;
	struct page *page = NULL;
	int classzone_idx;
	struct zone *zone;
	nodemask_t *allowednodes = NULL;/* zonelist_cache approximation */
	int zlc_active = 0;		/* set if using zonelist_cache */
	int did_zlc_setup = 0;		/* just call zlc_setup() one time */

	classzone_idx = zone_idx(preferred_zone);
zonelist_scan:
	/*
	 * Scan zonelist, looking for a zone with enough free.
	 * See also cpuset_zone_allowed() comment in kernel/cpuset.c.
	 */
	for_each_zone_zonelist_nodemask(zone, z, zonelist,
						high_zoneidx, nodemask) {
		if (NUMA_BUILD && zlc_active &&
			!zlc_zone_worth_trying(zonelist, z, allowednodes))
				continue;
		if ((alloc_flags & ALLOC_CPUSET) &&
			!cpuset_zone_allowed_softwall(zone, gfp_mask))
				goto try_next_zone;

		BUILD_BUG_ON(ALLOC_NO_WATERMARKS < NR_WMARK);
		if (!(alloc_flags & ALLOC_NO_WATERMARKS)) {
			unsigned long mark;
			int ret;

			mark = zone->watermark[alloc_flags & ALLOC_WMARK_MASK];
			if (zone_watermark_ok(zone, order, mark,
				    classzone_idx, alloc_flags))
				goto try_this_zone;

			if (zone_reclaim_mode == 0)
				goto this_zone_full;

			ret = zone_reclaim(zone, gfp_mask, order);
			switch (ret) {
			case ZONE_RECLAIM_NOSCAN:
				/* did not scan */
				goto try_next_zone;
			case ZONE_RECLAIM_FULL:
				/* scanned but unreclaimable */
				goto this_zone_full;
			default:
				/* did we reclaim enough */
				if (!zone_watermark_ok(zone, order, mark,
						classzone_idx, alloc_flags))
					goto this_zone_full;
			}
		}

try_this_zone:
		page = buffered_rmqueue(preferred_zone, zone, order,
						gfp_mask, migratetype);
		if (page)
			break;
this_zone_full:
		if (NUMA_BUILD)
			zlc_mark_zone_full(zonelist, z);
try_next_zone:
		if (NUMA_BUILD && !did_zlc_setup && nr_online_nodes > 1) {
			/*
			 * we do zlc_setup after the first zone is tried but only
			 * if there are multiple nodes make it worthwhile
			 */
			allowednodes = zlc_setup(zonelist, alloc_flags);
			zlc_active = 1;
			did_zlc_setup = 1;
		}
	}

	if (unlikely(NUMA_BUILD && page == NULL && zlc_active)) {
		/* Disable zlc cache for second zonelist scan */
		zlc_active = 0;
		goto zonelist_scan;
	}
	return page;
}

static inline int
should_alloc_retry(gfp_t gfp_mask, unsigned int order,
				unsigned long pages_reclaimed)
{
	/* Do not loop if specifically requested */
	if (gfp_mask & __GFP_NORETRY)
		return 0;

	/*
	 * In this implementation, order <= PAGE_ALLOC_COSTLY_ORDER
	 * means __GFP_NOFAIL, but that may not be true in other
	 * implementations.
	 */
	if (order <= PAGE_ALLOC_COSTLY_ORDER)
		return 1;

	/*
	 * For order > PAGE_ALLOC_COSTLY_ORDER, if __GFP_REPEAT is
	 * specified, then we retry until we no longer reclaim any pages
	 * (above), or we've reclaimed an order of pages at least as
	 * large as the allocation's order. In both cases, if the
	 * allocation still fails, we stop retrying.
	 */
	if (gfp_mask & __GFP_REPEAT && pages_reclaimed < (1 << order))
		return 1;

	/*
	 * Don't let big-order allocations loop unless the caller
	 * explicitly requests that.
	 */
	if (gfp_mask & __GFP_NOFAIL)
		return 1;

	return 0;
}

static inline struct page *
__alloc_pages_may_oom(gfp_t gfp_mask, unsigned int order,
	struct zonelist *zonelist, enum zone_type high_zoneidx,
	nodemask_t *nodemask, struct zone *preferred_zone,
	int migratetype)
{
	struct page *page;

	/* Acquire the OOM killer lock for the zones in zonelist */
	if (!try_set_zone_oom(zonelist, gfp_mask)) {
		schedule_timeout_uninterruptible(1);
		return NULL;
	}

	/*
	 * Go through the zonelist yet one more time, keep very high watermark
	 * here, this is only to catch a parallel oom killing, we must fail if
	 * we're still under heavy pressure.
	 */
	page = get_page_from_freelist(gfp_mask|__GFP_HARDWALL, nodemask,
		order, zonelist, high_zoneidx,
		ALLOC_WMARK_HIGH|ALLOC_CPUSET,
		preferred_zone, migratetype);
	if (page)
		goto out;

	/* The OOM killer will not help higher order allocs */
	if (order > PAGE_ALLOC_COSTLY_ORDER && !(gfp_mask & __GFP_NOFAIL))
		goto out;

	/* Exhausted what can be done so it's blamo time */
	out_of_memory(zonelist, gfp_mask, order);

out:
	clear_zonelist_oom(zonelist, gfp_mask);
	return page;
}

/* The really slow allocator path where we enter direct reclaim */
static inline struct page *
__alloc_pages_direct_reclaim(gfp_t gfp_mask, unsigned int order,
	struct zonelist *zonelist, enum zone_type high_zoneidx,
	nodemask_t *nodemask, int alloc_flags, struct zone *preferred_zone,
	int migratetype, unsigned long *did_some_progress)
{
	struct page *page = NULL;
	struct reclaim_state reclaim_state;
	struct task_struct *p = current;

	cond_resched();

	/* We now go into synchronous reclaim */
	cpuset_memory_pressure_bump();
	p->flags |= PF_MEMALLOC;
	lockdep_set_current_reclaim_state(gfp_mask);
	reclaim_state.reclaimed_slab = 0;
	p->reclaim_state = &reclaim_state;

	*did_some_progress = try_to_free_pages(zonelist, order, gfp_mask, nodemask);

	p->reclaim_state = NULL;
	lockdep_clear_current_reclaim_state();
	p->flags &= ~PF_MEMALLOC;

	cond_resched();

	if (order != 0)
		drain_all_pages();

	if (likely(*did_some_progress))
		page = get_page_from_freelist(gfp_mask, nodemask, order,
					zonelist, high_zoneidx,
					alloc_flags, preferred_zone,
					migratetype);
	return page;
}

/*
 * This is called in the allocator slow-path if the allocation request is of
 * sufficient urgency to ignore watermarks and take other desperate measures
 */
static inline struct page *
__alloc_pages_high_priority(gfp_t gfp_mask, unsigned int order,
	struct zonelist *zonelist, enum zone_type high_zoneidx,
	nodemask_t *nodemask, struct zone *preferred_zone,
	int migratetype)
{
	struct page *page;

	do {
		page = get_page_from_freelist(gfp_mask, nodemask, order,
			zonelist, high_zoneidx, ALLOC_NO_WATERMARKS,
			preferred_zone, migratetype);

		if (!page && gfp_mask & __GFP_NOFAIL)
			congestion_wait(BLK_RW_ASYNC, HZ/50);
	} while (!page && (gfp_mask & __GFP_NOFAIL));

	return page;
}

static inline
void wake_all_kswapd(unsigned int order, struct zonelist *zonelist,
						enum zone_type high_zoneidx)
{
	struct zoneref *z;
	struct zone *zone;

	for_each_zone_zonelist(zone, z, zonelist, high_zoneidx)
		wakeup_kswapd(zone, order);
}

static inline int
gfp_to_alloc_flags(gfp_t gfp_mask)
{
	struct task_struct *p = current;
	int alloc_flags = ALLOC_WMARK_MIN | ALLOC_CPUSET;
	const gfp_t wait = gfp_mask & __GFP_WAIT;

	/* __GFP_HIGH is assumed to be the same as ALLOC_HIGH to save a branch. */
	BUILD_BUG_ON(__GFP_HIGH != ALLOC_HIGH);

	/*
	 * The caller may dip into page reserves a bit more if the caller
	 * cannot run direct reclaim, or if the caller has realtime scheduling
	 * policy or is asking for __GFP_HIGH memory.  GFP_ATOMIC requests will
	 * set both ALLOC_HARDER (!wait) and ALLOC_HIGH (__GFP_HIGH).
	 */
	alloc_flags |= (gfp_mask & __GFP_HIGH);

	if (!wait) {
		alloc_flags |= ALLOC_HARDER;
		/*
		 * Ignore cpuset if GFP_ATOMIC (!wait) rather than fail alloc.
		 * See also cpuset_zone_allowed() comment in kernel/cpuset.c.
		 */
		alloc_flags &= ~ALLOC_CPUSET;
	} else if (unlikely(rt_task(p)) && !in_interrupt())
		alloc_flags |= ALLOC_HARDER;

	if (likely(!(gfp_mask & __GFP_NOMEMALLOC))) {
		if (!in_interrupt() &&
		    ((p->flags & PF_MEMALLOC) ||
		     unlikely(test_thread_flag(TIF_MEMDIE))))
			alloc_flags |= ALLOC_NO_WATERMARKS;
	}

	return alloc_flags;
}

static inline struct page *
__alloc_pages_slowpath(gfp_t gfp_mask, unsigned int order,
	struct zonelist *zonelist, enum zone_type high_zoneidx,
	nodemask_t *nodemask, struct zone *preferred_zone,
	int migratetype)
{
	const gfp_t wait = gfp_mask & __GFP_WAIT;
	struct page *page = NULL;
	int alloc_flags;
	unsigned long pages_reclaimed = 0;
	unsigned long did_some_progress;
	struct task_struct *p = current;

	/*
	 * In the slowpath, we sanity check order to avoid ever trying to
	 * reclaim >= MAX_ORDER areas which will never succeed. Callers may
	 * be using allocators in order of preference for an area that is
	 * too large.
	 */
	if (order >= MAX_ORDER) {
		WARN_ON_ONCE(!(gfp_mask & __GFP_NOWARN));
		return NULL;
	}

	/*
	 * GFP_THISNODE (meaning __GFP_THISNODE, __GFP_NORETRY and
	 * __GFP_NOWARN set) should not cause reclaim since the subsystem
	 * (f.e. slab) using GFP_THISNODE may choose to trigger reclaim
	 * using a larger set of nodes after it has established that the
	 * allowed per node queues are empty and that nodes are
	 * over allocated.
	 */
	if (NUMA_BUILD && (gfp_mask & GFP_THISNODE) == GFP_THISNODE)
		goto nopage;

restart:
	wake_all_kswapd(order, zonelist, high_zoneidx);

	/*
	 * OK, we're below the kswapd watermark and have kicked background
	 * reclaim. Now things get more complex, so set up alloc_flags according
	 * to how we want to proceed.
	 */
	alloc_flags = gfp_to_alloc_flags(gfp_mask);

	/* This is the last chance, in general, before the goto nopage. */
	page = get_page_from_freelist(gfp_mask, nodemask, order, zonelist,
			high_zoneidx, alloc_flags & ~ALLOC_NO_WATERMARKS,
			preferred_zone, migratetype);
	if (page)
		goto got_pg;

rebalance:
	/* Allocate without watermarks if the context allows */
	if (alloc_flags & ALLOC_NO_WATERMARKS) {
		page = __alloc_pages_high_priority(gfp_mask, order,
				zonelist, high_zoneidx, nodemask,
				preferred_zone, migratetype);
		if (page)
			goto got_pg;
	}

	/* Atomic allocations - we can't balance anything */
	if (!wait)
		goto nopage;

	/* Avoid recursion of direct reclaim */
	if (p->flags & PF_MEMALLOC)
		goto nopage;

	/* Avoid allocations with no watermarks from looping endlessly */
	if (test_thread_flag(TIF_MEMDIE) && !(gfp_mask & __GFP_NOFAIL))
		goto nopage;

	/* Try direct reclaim and then allocating */
	page = __alloc_pages_direct_reclaim(gfp_mask, order,
					zonelist, high_zoneidx,
					nodemask,
					alloc_flags, preferred_zone,
					migratetype, &did_some_progress);
	if (page)
		goto got_pg;

	/*
	 * If we failed to make any progress reclaiming, then we are
	 * running out of options and have to consider going OOM
	 */
	if (!did_some_progress) {
		if ((gfp_mask & __GFP_FS) && !(gfp_mask & __GFP_NORETRY)) {
			if (oom_killer_disabled)
				goto nopage;
			page = __alloc_pages_may_oom(gfp_mask, order,
					zonelist, high_zoneidx,
					nodemask, preferred_zone,
					migratetype);
			if (page)
				goto got_pg;

			/*
			 * The OOM killer does not trigger for high-order
			 * ~__GFP_NOFAIL allocations so if no progress is being
			 * made, there are no other options and retrying is
			 * unlikely to help.
			 */
			if (order > PAGE_ALLOC_COSTLY_ORDER &&
						!(gfp_mask & __GFP_NOFAIL))
				goto nopage;

			goto restart;
		}
	}

	/* Check if we should retry the allocation */
	pages_reclaimed += did_some_progress;
	if (should_alloc_retry(gfp_mask, order, pages_reclaimed)) {
		/* Wait for some write requests to complete then retry */
		congestion_wait(BLK_RW_ASYNC, HZ/50);
		goto rebalance;
	}

nopage:
	if (!(gfp_mask & __GFP_NOWARN) && printk_ratelimit()) {
		printk(KERN_WARNING "%s: page allocation failure."
			" order:%d, mode:0x%x\n",
			p->comm, order, gfp_mask);
		dump_stack();
		show_mem();
	}
	return page;
got_pg:
	if (kmemcheck_enabled)
		kmemcheck_pagealloc_alloc(page, order, gfp_mask);
	return page;

}

/*
 * This is the 'heart' of the zoned buddy allocator.
 */
struct page *
__alloc_pages_nodemask(gfp_t gfp_mask, unsigned int order,
			struct zonelist *zonelist, nodemask_t *nodemask)
{
	enum zone_type high_zoneidx = gfp_zone(gfp_mask);
	struct zone *preferred_zone;
	struct page *page;
	int migratetype = allocflags_to_migratetype(gfp_mask);

	gfp_mask &= gfp_allowed_mask;

	lockdep_trace_alloc(gfp_mask);

	might_sleep_if(gfp_mask & __GFP_WAIT);

	if (should_fail_alloc_page(gfp_mask, order))
		return NULL;

	/*
	 * Check the zones suitable for the gfp_mask contain at least one
	 * valid zone. It's possible to have an empty zonelist as a result
	 * of GFP_THISNODE and a memoryless node
	 */
	if (unlikely(!zonelist->_zonerefs->zone))
		return NULL;

	/* The preferred zone is used for statistics later */
	first_zones_zonelist(zonelist, high_zoneidx, nodemask, &preferred_zone);
	if (!preferred_zone)
		return NULL;

	/* First allocation attempt */
	page = get_page_from_freelist(gfp_mask|__GFP_HARDWALL, nodemask, order,
			zonelist, high_zoneidx, ALLOC_WMARK_LOW|ALLOC_CPUSET,
			preferred_zone, migratetype);
	if (unlikely(!page))
		page = __alloc_pages_slowpath(gfp_mask, order,
				zonelist, high_zoneidx, nodemask,
				preferred_zone, migratetype);

	trace_mm_page_alloc(page, order, gfp_mask, migratetype);
	return page;
}
EXPORT_SYMBOL(__alloc_pages_nodemask);

/*
 * Common helper functions.
 */
unsigned long __get_free_pages(gfp_t gfp_mask, unsigned int order)
{
	struct page *page;

	/*
	 * __get_free_pages() returns a 32-bit address, which cannot represent
	 * a highmem page
	 */
	VM_BUG_ON((gfp_mask & __GFP_HIGHMEM) != 0);

	page = alloc_pages(gfp_mask, order);
	if (!page)
		return 0;
	return (unsigned long) page_address(page);
}
EXPORT_SYMBOL(__get_free_pages);

unsigned long get_zeroed_page(gfp_t gfp_mask)
{
	return __get_free_pages(gfp_mask | __GFP_ZERO, 0);
}
EXPORT_SYMBOL(get_zeroed_page);

void __pagevec_free(struct pagevec *pvec)
{
	int i = pagevec_count(pvec);

	while (--i >= 0) {
		trace_mm_pagevec_free(pvec->pages[i], pvec->cold);
		free_hot_cold_page(pvec->pages[i], pvec->cold);
	}
}

void __free_pages(struct page *page, unsigned int order)
{
	if (put_page_testzero(page)) {
		trace_mm_page_free_direct(page, order);
		if (order == 0)
			free_hot_page(page);
		else
			__free_pages_ok(page, order);
	}
}

EXPORT_SYMBOL(__free_pages);

void free_pages(unsigned long addr, unsigned int order)
{
	if (addr != 0) {
		VM_BUG_ON(!virt_addr_valid((void *)addr));
		__free_pages(virt_to_page((void *)addr), order);
	}
}

EXPORT_SYMBOL(free_pages);

/**
 * alloc_pages_exact - allocate an exact number physically-contiguous pages.
 * @size: the number of bytes to allocate
 * @gfp_mask: GFP flags for the allocation
 *
 * This function is similar to alloc_pages(), except that it allocates the
 * minimum number of pages to satisfy the request.  alloc_pages() can only
 * allocate memory in power-of-two pages.
 *
 * This function is also limited by MAX_ORDER.
 *
 * Memory allocated by this function must be released by free_pages_exact().
 */
void *alloc_pages_exact(size_t size, gfp_t gfp_mask)
{
	unsigned int order = get_order(size);
	unsigned long addr;

	addr = __get_free_pages(gfp_mask, order);
	if (addr) {
		unsigned long alloc_end = addr + (PAGE_SIZE << order);
		unsigned long used = addr + PAGE_ALIGN(size);

		split_page(virt_to_page((void *)addr), order);
		while (used < alloc_end) {
			free_page(used);
			used += PAGE_SIZE;
		}
	}

	return (void *)addr;
}
EXPORT_SYMBOL(alloc_pages_exact);

/**
 * free_pages_exact - release memory allocated via alloc_pages_exact()
 * @virt: the value returned by alloc_pages_exact.
 * @size: size of allocation, same value as passed to alloc_pages_exact().
 *
 * Release the memory allocated by a previous call to alloc_pages_exact.
 */
void free_pages_exact(void *virt, size_t size)
{
	unsigned long addr = (unsigned long)virt;
	unsigned long end = addr + PAGE_ALIGN(size);

	while (addr < end) {
		free_page(addr);
		addr += PAGE_SIZE;
	}
}
EXPORT_SYMBOL(free_pages_exact);

static unsigned int nr_free_zone_pages(int offset)
{
	struct zoneref *z;
	struct zone *zone;

	/* Just pick one node, since fallback list is circular */
	unsigned int sum = 0;

	struct zonelist *zonelist = node_zonelist(numa_node_id(), GFP_KERNEL);

	for_each_zone_zonelist(zone, z, zonelist, offset) {
		unsigned long size = zone->present_pages;
		unsigned long high = high_wmark_pages(zone);
		if (size > high)
			sum += size - high;
	}

	return sum;
}

/*
 * Amount of free RAM allocatable within ZONE_DMA and ZONE_NORMAL
 */
unsigned int nr_free_buffer_pages(void)
{
	return nr_free_zone_pages(gfp_zone(GFP_USER));
}
EXPORT_SYMBOL_GPL(nr_free_buffer_pages);

/*
 * Amount of free RAM allocatable within all zones
 */
unsigned int nr_free_pagecache_pages(void)
{
	return nr_free_zone_pages(gfp_zone(GFP_HIGHUSER_MOVABLE));
}

static inline void show_node(struct zone *zone)
{
	if (NUMA_BUILD)
		printk("Node %d ", zone_to_nid(zone));
}

void si_meminfo(struct sysinfo *val)
{
	val->totalram = totalram_pages;
	val->sharedram = 0;
	val->freeram = global_page_state(NR_FREE_PAGES);
	val->bufferram = nr_blockdev_pages();
	val->totalhigh = totalhigh_pages;
	val->freehigh = nr_free_highpages();
	val->mem_unit = PAGE_SIZE;
}

EXPORT_SYMBOL(si_meminfo);

#ifdef CONFIG_NUMA
void si_meminfo_node(struct sysinfo *val, int nid)
{
	pg_data_t *pgdat = NODE_DATA(nid);

	val->totalram = pgdat->node_present_pages;
	val->freeram = node_page_state(nid, NR_FREE_PAGES);
#ifdef CONFIG_HIGHMEM
	val->totalhigh = pgdat->node_zones[ZONE_HIGHMEM].present_pages;
	val->freehigh = zone_page_state(&pgdat->node_zones[ZONE_HIGHMEM],
			NR_FREE_PAGES);
#else
	val->totalhigh = 0;
	val->freehigh = 0;
#endif
	val->mem_unit = PAGE_SIZE;
}
#endif

#define K(x) ((x) << (PAGE_SHIFT-10))

/*
 * Show free area list (used inside shift_scroll-lock stuff)
 * We also calculate the percentage fragmentation. We do this by counting the
 * memory on each free list with the exception of the first item on the list.
 */
void show_free_areas(void)
{
	int cpu;
	struct zone *zone;

	for_each_populated_zone(zone) {
		show_node(zone);
		printk("%s per-cpu:\n", zone->name);

		for_each_online_cpu(cpu) {
			struct per_cpu_pageset *pageset;

			pageset = zone_pcp(zone, cpu);

			printk("CPU %4d: hi:%5d, btch:%4d usd:%4d\n",
			       cpu, pageset->pcp.high,
			       pageset->pcp.batch, pageset->pcp.count);
		}
	}

	printk("active_anon:%lu inactive_anon:%lu isolated_anon:%lu\n"
		" active_file:%lu inactive_file:%lu isolated_file:%lu\n"
		" unevictable:%lu"
		" dirty:%lu writeback:%lu unstable:%lu\n"
		" free:%lu slab_reclaimable:%lu slab_unreclaimable:%lu\n"
		" mapped:%lu shmem:%lu pagetables:%lu bounce:%lu\n",
		global_page_state(NR_ACTIVE_ANON),
		global_page_state(NR_INACTIVE_ANON),
		global_page_state(NR_ISOLATED_ANON),
		global_page_state(NR_ACTIVE_FILE),
		global_page_state(NR_INACTIVE_FILE),
		global_page_state(NR_ISOLATED_FILE),
		global_page_state(NR_UNEVICTABLE),
		global_page_state(NR_FILE_DIRTY),
		global_page_state(NR_WRITEBACK),
		global_page_state(NR_UNSTABLE_NFS),
		global_page_state(NR_FREE_PAGES),
		global_page_state(NR_SLAB_RECLAIMABLE),
		global_page_state(NR_SLAB_UNRECLAIMABLE),
		global_page_state(NR_FILE_MAPPED),
		global_page_state(NR_SHMEM),
		global_page_state(NR_PAGETABLE),
		global_page_state(NR_BOUNCE));

	for_each_populated_zone(zone) {
		int i;

		show_node(zone);
		printk("%s"
			" free:%lukB"
			" min:%lukB"
			" low:%lukB"
			" high:%lukB"
			" active_anon:%lukB"
			" inactive_anon:%lukB"
			" active_file:%lukB"
			" inactive_file:%lukB"
			" unevictable:%lukB"
			" isolated(anon):%lukB"
			" isolated(file):%lukB"
			" present:%lukB"
			" mlocked:%lukB"
			" dirty:%lukB"
			" writeback:%lukB"
			" mapped:%lukB"
			" shmem:%lukB"
			" slab_reclaimable:%lukB"
			" slab_unreclaimable:%lukB"
			" kernel_stack:%lukB"
			" pagetables:%lukB"
			" unstable:%lukB"
			" bounce:%lukB"
			" writeback_tmp:%lukB"
			" pages_scanned:%lu"
			" all_unreclaimable? %s"
			"\n",
			zone->name,
			K(zone_page_state(zone, NR_FREE_PAGES)),
			K(min_wmark_pages(zone)),
			K(low_wmark_pages(zone)),
			K(high_wmark_pages(zone)),
			K(zone_page_state(zone, NR_ACTIVE_ANON)),
			K(zone_page_state(zone, NR_INACTIVE_ANON)),
			K(zone_page_state(zone, NR_ACTIVE_FILE)),
			K(zone_page_state(zone, NR_INACTIVE_FILE)),
			K(zone_page_state(zone, NR_UNEVICTABLE)),
			K(zone_page_state(zone, NR_ISOLATED_ANON)),
			K(zone_page_state(zone, NR_ISOLATED_FILE)),
			K(zone->present_pages),
			K(zone_page_state(zone, NR_MLOCK)),
			K(zone_page_state(zone, NR_FILE_DIRTY)),
			K(zone_page_state(zone, NR_WRITEBACK)),
			K(zone_page_state(zone, NR_FILE_MAPPED)),
			K(zone_page_state(zone, NR_SHMEM)),
			K(zone_page_state(zone, NR_SLAB_RECLAIMABLE)),
			K(zone_page_state(zone, NR_SLAB_UNRECLAIMABLE)),
			zone_page_state(zone, NR_KERNEL_STACK) *
				THREAD_SIZE / 1024,
			K(zone_page_state(zone, NR_PAGETABLE)),
			K(zone_page_state(zone, NR_UNSTABLE_NFS)),
			K(zone_page_state(zone, NR_BOUNCE)),
			K(zone_page_state(zone, NR_WRITEBACK_TEMP)),
			zone->pages_scanned,
			(zone_is_all_unreclaimable(zone) ? "yes" : "no")
			);
		printk("lowmem_reserve[]:");
		for (i = 0; i < MAX_NR_ZONES; i++)
			printk(" %lu", zone->lowmem_reserve[i]);
		printk("\n");
	}

	for_each_populated_zone(zone) {
 		unsigned long nr[MAX_ORDER], flags, order, total = 0;

		show_node(zone);
		printk("%s: ", zone->name);

		spin_lock_irqsave(&zone->lock, flags);
		for (order = 0; order < MAX_ORDER; order++) {
			nr[order] = zone->free_area[order].nr_free;
			total += nr[order] << order;
		}
		spin_unlock_irqrestore(&zone->lock, flags);
		for (order = 0; order < MAX_ORDER; order++)
			printk("%lu*%lukB ", nr[order], K(1UL) << order);
		printk("= %lukB\n", K(total));
	}

	printk("%ld total pagecache pages\n", global_page_state(NR_FILE_PAGES));

	show_swap_cache_info();
}

static void zoneref_set_zone(struct zone *zone, struct zoneref *zoneref)
{
	zoneref->zone = zone;
	zoneref->zone_idx = zone_idx(zone);
}

/*
 * Builds allocation fallback zone lists.
 *
 * Add all populated zones of a node to the zonelist.
 */
static int build_zonelists_node(pg_data_t *pgdat, struct zonelist *zonelist,
				int nr_zones, enum zone_type zone_type)
{
	struct zone *zone;

	BUG_ON(zone_type >= MAX_NR_ZONES);
	zone_type++;

	do {
		zone_type--;
		zone = pgdat->node_zones + zone_type;
		if (populated_zone(zone)) {
			zoneref_set_zone(zone,
				&zonelist->_zonerefs[nr_zones++]);
			check_highest_zone(zone_type);
		}

	} while (zone_type);
	return nr_zones;
}


/*
 *  zonelist_order:
 *  0 = automatic detection of better ordering.
 *  1 = order by ([node] distance, -zonetype)
 *  2 = order by (-zonetype, [node] distance)
 *
 *  If not NUMA, ZONELIST_ORDER_ZONE and ZONELIST_ORDER_NODE will create
 *  the same zonelist. So only NUMA can configure this param.
 */
#define ZONELIST_ORDER_DEFAULT  0
#define ZONELIST_ORDER_NODE     1
#define ZONELIST_ORDER_ZONE     2

/* zonelist order in the kernel.
 * set_zonelist_order() will set this to NODE or ZONE.
 */
static int current_zonelist_order = ZONELIST_ORDER_DEFAULT;
static char zonelist_order_name[3][8] = {"Default", "Node", "Zone"};


#ifdef CONFIG_NUMA
/* The value user specified ....changed by config */
static int user_zonelist_order = ZONELIST_ORDER_DEFAULT;
/* string for sysctl */
#define NUMA_ZONELIST_ORDER_LEN	16
char numa_zonelist_order[16] = "default";

/*
 * interface for configure zonelist ordering.
 * command line option "numa_zonelist_order"
 *	= "[dD]efault	- default, automatic configuration.
 *	= "[nN]ode 	- order by node locality, then by zone within node
 *	= "[zZ]one      - order by zone, then by locality within zone
 */

static int __parse_numa_zonelist_order(char *s)
{
	if (*s == 'd' || *s == 'D') {
		user_zonelist_order = ZONELIST_ORDER_DEFAULT;
	} else if (*s == 'n' || *s == 'N') {
		user_zonelist_order = ZONELIST_ORDER_NODE;
	} else if (*s == 'z' || *s == 'Z') {
		user_zonelist_order = ZONELIST_ORDER_ZONE;
	} else {
		printk(KERN_WARNING
			"Ignoring invalid numa_zonelist_order value:  "
			"%s\n", s);
		return -EINVAL;
	}
	return 0;
}

static __init int setup_numa_zonelist_order(char *s)
{
	if (s)
		return __parse_numa_zonelist_order(s);
	return 0;
}
early_param("numa_zonelist_order", setup_numa_zonelist_order);

/*
 * sysctl handler for numa_zonelist_order
 */
int numa_zonelist_order_handler(ctl_table *table, int write,
		void __user *buffer, size_t *length,
		loff_t *ppos)
{
	char saved_string[NUMA_ZONELIST_ORDER_LEN];
	int ret;

	if (write)
		strncpy(saved_string, (char*)table->data,
			NUMA_ZONELIST_ORDER_LEN);
	ret = proc_dostring(table, write, buffer, length, ppos);
	if (ret)
		return ret;
	if (write) {
		int oldval = user_zonelist_order;
		if (__parse_numa_zonelist_order((char*)table->data)) {
			/*
			 * bogus value.  restore saved string
			 */
			strncpy((char*)table->data, saved_string,
				NUMA_ZONELIST_ORDER_LEN);
			user_zonelist_order = oldval;
		} else if (oldval != user_zonelist_order)
			build_all_zonelists();
	}
	return 0;
}


#define MAX_NODE_LOAD (nr_online_nodes)
static int node_load[MAX_NUMNODES];

/**
 * find_next_best_node - find the next node that should appear in a given node's fallback list
 * @node: node whose fallback list we're appending
 * @used_node_mask: nodemask_t of already used nodes
 *
 * We use a number of factors to determine which is the next node that should
 * appear on a given node's fallback list.  The node should not have appeared
 * already in @node's fallback list, and it should be the next closest node
 * according to the distance array (which contains arbitrary distance values
 * from each node to each node in the system), and should also prefer nodes
 * with no CPUs, since presumably they'll have very little allocation pressure
 * on them otherwise.
 * It returns -1 if no node is found.
 */
static int find_next_best_node(int node, nodemask_t *used_node_mask)
{
	int n, val;
	int min_val = INT_MAX;
	int best_node = -1;
	const struct cpumask *tmp = cpumask_of_node(0);

	/* Use the local node if we haven't already */
	if (!node_isset(node, *used_node_mask)) {
		node_set(node, *used_node_mask);
		return node;
	}

	for_each_node_state(n, N_HIGH_MEMORY) {

		/* Don't want a node to appear more than once */
		if (node_isset(n, *used_node_mask))
			continue;

		/* Use the distance array to find the distance */
		val = node_distance(node, n);

		/* Penalize nodes under us ("prefer the next node") */
		val += (n < node);

		/* Give preference to headless and unused nodes */
		tmp = cpumask_of_node(n);
		if (!cpumask_empty(tmp))
			val += PENALTY_FOR_NODE_WITH_CPUS;

		/* Slight preference for less loaded node */
		val *= (MAX_NODE_LOAD*MAX_NUMNODES);
		val += node_load[n];

		if (val < min_val) {
			min_val = val;
			best_node = n;
		}
	}

	if (best_node >= 0)
		node_set(best_node, *used_node_mask);

	return best_node;
}


/*
 * Build zonelists ordered by node and zones within node.
 * This results in maximum locality--normal zone overflows into local
 * DMA zone, if any--but risks exhausting DMA zone.
 */
static void build_zonelists_in_node_order(pg_data_t *pgdat, int node)
{
	int j;
	struct zonelist *zonelist;

	zonelist = &pgdat->node_zonelists[0];
	for (j = 0; zonelist->_zonerefs[j].zone != NULL; j++)
		;
	j = build_zonelists_node(NODE_DATA(node), zonelist, j,
							MAX_NR_ZONES - 1);
	zonelist->_zonerefs[j].zone = NULL;
	zonelist->_zonerefs[j].zone_idx = 0;
}

/*
 * Build gfp_thisnode zonelists
 */
static void build_thisnode_zonelists(pg_data_t *pgdat)
{
	int j;
	struct zonelist *zonelist;

	zonelist = &pgdat->node_zonelists[1];
	j = build_zonelists_node(pgdat, zonelist, 0, MAX_NR_ZONES - 1);
	zonelist->_zonerefs[j].zone = NULL;
	zonelist->_zonerefs[j].zone_idx = 0;
}

/*
 * Build zonelists ordered by zone and nodes within zones.
 * This results in conserving DMA zone[s] until all Normal memory is
 * exhausted, but results in overflowing to remote node while memory
 * may still exist in local DMA zone.
 */
static int node_order[MAX_NUMNODES];

static void build_zonelists_in_zone_order(pg_data_t *pgdat, int nr_nodes)
{
	int pos, j, node;
	int zone_type;		/* needs to be signed */
	struct zone *z;
	struct zonelist *zonelist;

	zonelist = &pgdat->node_zonelists[0];
	pos = 0;
	for (zone_type = MAX_NR_ZONES - 1; zone_type >= 0; zone_type--) {
		for (j = 0; j < nr_nodes; j++) {
			node = node_order[j];
			z = &NODE_DATA(node)->node_zones[zone_type];
			if (populated_zone(z)) {
				zoneref_set_zone(z,
					&zonelist->_zonerefs[pos++]);
				check_highest_zone(zone_type);
			}
		}
	}
	zonelist->_zonerefs[pos].zone = NULL;
	zonelist->_zonerefs[pos].zone_idx = 0;
}

static int default_zonelist_order(void)
{
	int nid, zone_type;
	unsigned long low_kmem_size,total_size;
	struct zone *z;
	int average_size;
	/*
         * ZONE_DMA and ZONE_DMA32 can be very small area in the sytem.
	 * If they are really small and used heavily, the system can fall
	 * into OOM very easily.
	 * This function detect ZONE_DMA/DMA32 size and confgigures zone order.
	 */
	/* Is there ZONE_NORMAL ? (ex. ppc has only DMA zone..) */
	low_kmem_size = 0;
	total_size = 0;
	for_each_online_node(nid) {
		for (zone_type = 0; zone_type < MAX_NR_ZONES; zone_type++) {
			z = &NODE_DATA(nid)->node_zones[zone_type];
			if (populated_zone(z)) {
				if (zone_type < ZONE_NORMAL)
					low_kmem_size += z->present_pages;
				total_size += z->present_pages;
			}
		}
	}
	if (!low_kmem_size ||  /* there are no DMA area. */
	    low_kmem_size > total_size/2) /* DMA/DMA32 is big. */
		return ZONELIST_ORDER_NODE;
	/*
	 * look into each node's config.
  	 * If there is a node whose DMA/DMA32 memory is very big area on
 	 * local memory, NODE_ORDER may be suitable.
         */
	average_size = total_size /
				(nodes_weight(node_states[N_HIGH_MEMORY]) + 1);
	for_each_online_node(nid) {
		low_kmem_size = 0;
		total_size = 0;
		for (zone_type = 0; zone_type < MAX_NR_ZONES; zone_type++) {
			z = &NODE_DATA(nid)->node_zones[zone_type];
			if (populated_zone(z)) {
				if (zone_type < ZONE_NORMAL)
					low_kmem_size += z->present_pages;
				total_size += z->present_pages;
			}
		}
		if (low_kmem_size &&
		    total_size > average_size && /* ignore small node */
		    low_kmem_size > total_size * 70/100)
			return ZONELIST_ORDER_NODE;
	}
	return ZONELIST_ORDER_ZONE;
}

static void set_zonelist_order(void)
{
	if (user_zonelist_order == ZONELIST_ORDER_DEFAULT)
		current_zonelist_order = default_zonelist_order();
	else
		current_zonelist_order = user_zonelist_order;
}

static void build_zonelists(pg_data_t *pgdat)
{
	int j, node, load;
	enum zone_type i;
	nodemask_t used_mask;
	int local_node, prev_node;
	struct zonelist *zonelist;
	int order = current_zonelist_order;

	/* initialize zonelists */
	for (i = 0; i < MAX_ZONELISTS; i++) {
		zonelist = pgdat->node_zonelists + i;
		zonelist->_zonerefs[0].zone = NULL;
		zonelist->_zonerefs[0].zone_idx = 0;
	}

	/* NUMA-aware ordering of nodes */
	local_node = pgdat->node_id;
	load = nr_online_nodes;
	prev_node = local_node;
	nodes_clear(used_mask);

	memset(node_order, 0, sizeof(node_order));
	j = 0;

	while ((node = find_next_best_node(local_node, &used_mask)) >= 0) {
		int distance = node_distance(local_node, node);

		/*
		 * If another node is sufficiently far away then it is better
		 * to reclaim pages in a zone before going off node.
		 */
		if (distance > RECLAIM_DISTANCE)
			zone_reclaim_mode = 1;

		/*
		 * We don't want to pressure a particular node.
		 * So adding penalty to the first node in same
		 * distance group to make it round-robin.
		 */
		if (distance != node_distance(local_node, prev_node))
			node_load[node] = load;

		prev_node = node;
		load--;
		if (order == ZONELIST_ORDER_NODE)
			build_zonelists_in_node_order(pgdat, node);
		else
			node_order[j++] = node;	/* remember order */
	}

	if (order == ZONELIST_ORDER_ZONE) {
		/* calculate node order -- i.e., DMA last! */
		build_zonelists_in_zone_order(pgdat, j);
	}

	build_thisnode_zonelists(pgdat);
}

/* Construct the zonelist performance cache - see further mmzone.h */
static void build_zonelist_cache(pg_data_t *pgdat)
{
	struct zonelist *zonelist;
	struct zonelist_cache *zlc;
	struct zoneref *z;

	zonelist = &pgdat->node_zonelists[0];
	zonelist->zlcache_ptr = zlc = &zonelist->zlcache;
	bitmap_zero(zlc->fullzones, MAX_ZONES_PER_ZONELIST);
	for (z = zonelist->_zonerefs; z->zone; z++)
		zlc->z_to_n[z - zonelist->_zonerefs] = zonelist_node_idx(z);
}


#else	/* CONFIG_NUMA */

static void set_zonelist_order(void)
{
	current_zonelist_order = ZONELIST_ORDER_ZONE;
}

static void build_zonelists(pg_data_t *pgdat)
{
	int node, local_node;
	enum zone_type j;
	struct zonelist *zonelist;

	local_node = pgdat->node_id;

	zonelist = &pgdat->node_zonelists[0];
	j = build_zonelists_node(pgdat, zonelist, 0, MAX_NR_ZONES - 1);

	/*
	 * Now we build the zonelist so that it contains the zones
	 * of all the other nodes.
	 * We don't want to pressure a particular node, so when
	 * building the zones for node N, we make sure that the
	 * zones coming right after the local ones are those from
	 * node N+1 (modulo N)
	 */
	for (node = local_node + 1; node < MAX_NUMNODES; node++) {
		if (!node_online(node))
			continue;
		j = build_zonelists_node(NODE_DATA(node), zonelist, j,
							MAX_NR_ZONES - 1);
	}
	for (node = 0; node < local_node; node++) {
		if (!node_online(node))
			continue;
		j = build_zonelists_node(NODE_DATA(node), zonelist, j,
							MAX_NR_ZONES - 1);
	}

	zonelist->_zonerefs[j].zone = NULL;
	zonelist->_zonerefs[j].zone_idx = 0;
}

/* non-NUMA variant of zonelist performance cache - just NULL zlcache_ptr */
static void build_zonelist_cache(pg_data_t *pgdat)
{
	pgdat->node_zonelists[0].zlcache_ptr = NULL;
}

#endif	/* CONFIG_NUMA */

/* return values int ....just for stop_machine() */
static int __build_all_zonelists(void *dummy)
{
	int nid;

#ifdef CONFIG_NUMA
	memset(node_load, 0, sizeof(node_load));
#endif
	for_each_online_node(nid) {
		pg_data_t *pgdat = NODE_DATA(nid);

		build_zonelists(pgdat);
		build_zonelist_cache(pgdat);
	}
	return 0;
}

void build_all_zonelists(void)
{
	set_zonelist_order();

	if (system_state == SYSTEM_BOOTING) {
		__build_all_zonelists(NULL);
		mminit_verify_zonelist();
		cpuset_init_current_mems_allowed();
	} else {
		/* we have to stop all cpus to guarantee there is no user
		   of zonelist */
		stop_machine(__build_all_zonelists, NULL, NULL);
		/* cpuset refresh routine should be here */
	}
	vm_total_pages = nr_free_pagecache_pages();
	/*
	 * Disable grouping by mobility if the number of pages in the
	 * system is too low to allow the mechanism to work. It would be
	 * more accurate, but expensive to check per-zone. This check is
	 * made on memory-hotadd so a system can start with mobility
	 * disabled and enable it later
	 */
	if (vm_total_pages < (pageblock_nr_pages * MIGRATE_TYPES))
		page_group_by_mobility_disabled = 1;
	else
		page_group_by_mobility_disabled = 0;

	printk("Built %i zonelists in %s order, mobility grouping %s.  "
		"Total pages: %ld\n",
			nr_online_nodes,
			zonelist_order_name[current_zonelist_order],
			page_group_by_mobility_disabled ? "off" : "on",
			vm_total_pages);
#ifdef CONFIG_NUMA
	printk("Policy zone: %s\n", zone_names[policy_zone]);
#endif
}

/*
 * Helper functions to size the waitqueue hash table.
 * Essentially these want to choose hash table sizes sufficiently
 * large so that collisions trying to wait on pages are rare.
 * But in fact, the number of active page waitqueues on typical
 * systems is ridiculously low, less than 200. So this is even
 * conservative, even though it seems large.
 *
 * The constant PAGES_PER_WAITQUEUE specifies the ratio of pages to
 * waitqueues, i.e. the size of the waitq table given the number of pages.
 */
#define PAGES_PER_WAITQUEUE	256

#ifndef CONFIG_MEMORY_HOTPLUG
static inline unsigned long wait_table_hash_nr_entries(unsigned long pages)
{
	unsigned long size = 1;

	pages /= PAGES_PER_WAITQUEUE;

	while (size < pages)
		size <<= 1;

	/*
	 * Once we have dozens or even hundreds of threads sleeping
	 * on IO we've got bigger problems than wait queue collision.
	 * Limit the size of the wait table to a reasonable size.
	 */
	size = min(size, 4096UL);

	return max(size, 4UL);
}
#else
/*
 * A zone's size might be changed by hot-add, so it is not possible to determine
 * a suitable size for its wait_table.  So we use the maximum size now.
 *
 * The max wait table size = 4096 x sizeof(wait_queue_head_t).   ie:
 *
 *    i386 (preemption config)    : 4096 x 16 = 64Kbyte.
 *    ia64, x86-64 (no preemption): 4096 x 20 = 80Kbyte.
 *    ia64, x86-64 (preemption)   : 4096 x 24 = 96Kbyte.
 *
 * The maximum entries are prepared when a zone's memory is (512K + 256) pages
 * or more by the traditional way. (See above).  It equals:
 *
 *    i386, x86-64, powerpc(4K page size) : =  ( 2G + 1M)byte.
 *    ia64(16K page size)                 : =  ( 8G + 4M)byte.
 *    powerpc (64K page size)             : =  (32G +16M)byte.
 */
static inline unsigned long wait_table_hash_nr_entries(unsigned long pages)
{
	return 4096UL;
}
#endif

/*
 * This is an integer logarithm so that shifts can be used later
 * to extract the more random high bits from the multiplicative
 * hash function before the remainder is taken.
 */
static inline unsigned long wait_table_bits(unsigned long size)
{
	return ffz(~size);
}

#define LONG_ALIGN(x) (((x)+(sizeof(long))-1)&~((sizeof(long))-1))

/*
 * Mark a number of pageblocks as MIGRATE_RESERVE. The number
 * of blocks reserved is based on min_wmark_pages(zone). The memory within
 * the reserve will tend to store contiguous free pages. Setting min_free_kbytes
 * higher will lead to a bigger reserve which will get freed as contiguous
 * blocks as reclaim kicks in
 */
static void setup_zone_migrate_reserve(struct zone *zone)
{
	unsigned long start_pfn, pfn, end_pfn;
	struct page *page;
	unsigned long block_migratetype;
	int reserve;

	/* Get the start pfn, end pfn and the number of blocks to reserve */
	start_pfn = zone->zone_start_pfn;
	end_pfn = start_pfn + zone->spanned_pages;
	reserve = roundup(min_wmark_pages(zone), pageblock_nr_pages) >>
							pageblock_order;

	/*
	 * Reserve blocks are generally in place to help high-order atomic
	 * allocations that are short-lived. A min_free_kbytes value that
	 * would result in more than 2 reserve blocks for atomic allocations
	 * is assumed to be in place to help anti-fragmentation for the
	 * future allocation of hugepages at runtime.
	 */
	reserve = min(2, reserve);

	for (pfn = start_pfn; pfn < end_pfn; pfn += pageblock_nr_pages) {
		if (!pfn_valid(pfn))
			continue;
		page = pfn_to_page(pfn);

		/* Watch out for overlapping nodes */
		if (page_to_nid(page) != zone_to_nid(zone))
			continue;

		/* Blocks with reserved pages will never free, skip them. */
		if (PageReserved(page))
			continue;

		block_migratetype = get_pageblock_migratetype(page);

		/* If this block is reserved, account for it */
		if (reserve > 0 && block_migratetype == MIGRATE_RESERVE) {
			reserve--;
			continue;
		}

		/* Suitable for reserving if this block is movable */
		if (reserve > 0 && block_migratetype == MIGRATE_MOVABLE) {
			set_pageblock_migratetype(page, MIGRATE_RESERVE);
			move_freepages_block(zone, page, MIGRATE_RESERVE);
			reserve--;
			continue;
		}

		/*
		 * If the reserve is met and this is a previous reserved block,
		 * take it back
		 */
		if (block_migratetype == MIGRATE_RESERVE) {
			set_pageblock_migratetype(page, MIGRATE_MOVABLE);
			move_freepages_block(zone, page, MIGRATE_MOVABLE);
		}
	}
}

/*
 * Initially all pages are reserved - free ones are freed
 * up by free_all_bootmem() once the early boot process is
 * done. Non-atomic initialization, single-pass.
 */
void __meminit memmap_init_zone(unsigned long size, int nid, unsigned long zone,
		unsigned long start_pfn, enum memmap_context context)
{
	struct page *page;
	unsigned long end_pfn = start_pfn + size;
	unsigned long pfn;
	struct zone *z;

	if (highest_memmap_pfn < end_pfn - 1)
		highest_memmap_pfn = end_pfn - 1;

	z = &NODE_DATA(nid)->node_zones[zone];
	for (pfn = start_pfn; pfn < end_pfn; pfn++) {
		/*
		 * There can be holes in boot-time mem_map[]s
		 * handed to this function.  They do not
		 * exist on hotplugged memory.
		 */
		if (context == MEMMAP_EARLY) {
			if (!early_pfn_valid(pfn))
				continue;
			if (!early_pfn_in_nid(pfn, nid))
				continue;
		}
		page = pfn_to_page(pfn);
		set_page_links(page, zone, nid, pfn);
		mminit_verify_page_links(page, zone, nid, pfn);
		init_page_count(page);
		reset_page_mapcount(page);
		SetPageReserved(page);
		/*
		 * Mark the block movable so that blocks are reserved for
		 * movable at startup. This will force kernel allocations
		 * to reserve their blocks rather than leaking throughout
		 * the address space during boot when many long-lived
		 * kernel allocations are made. Later some blocks near
		 * the start are marked MIGRATE_RESERVE by
		 * setup_zone_migrate_reserve()
		 *
		 * bitmap is created for zone's valid pfn range. but memmap
		 * can be created for invalid pages (for alignment)
		 * check here not to call set_pageblock_migratetype() against
		 * pfn out of zone.
		 */
		if ((z->zone_start_pfn <= pfn)
		    && (pfn < z->zone_start_pfn + z->spanned_pages)
		    && !(pfn & (pageblock_nr_pages - 1)))
			set_pageblock_migratetype(page, MIGRATE_MOVABLE);

		INIT_LIST_HEAD(&page->lru);
#ifdef WANT_PAGE_VIRTUAL
		/* The shift won't overflow because ZONE_NORMAL is below 4G. */
		if (!is_highmem_idx(zone))
			set_page_address(page, __va(pfn << PAGE_SHIFT));
#endif
	}
}

static void __meminit zone_init_free_lists(struct zone *zone)
{
	int order, t;
	for_each_migratetype_order(order, t) {
		INIT_LIST_HEAD(&zone->free_area[order].free_list[t]);
		zone->free_area[order].nr_free = 0;
	}
}

#ifndef __HAVE_ARCH_MEMMAP_INIT
#define memmap_init(size, nid, zone, start_pfn) \
	memmap_init_zone((size), (nid), (zone), (start_pfn), MEMMAP_EARLY)
#endif

static int zone_batchsize(struct zone *zone)
{
#ifdef CONFIG_MMU
	int batch;

	/*
	 * The per-cpu-pages pools are set to around 1000th of the
	 * size of the zone.  But no more than 1/2 of a meg.
	 *
	 * OK, so we don't know how big the cache is.  So guess.
	 */
	batch = zone->present_pages / 1024;
	if (batch * PAGE_SIZE > 512 * 1024)
		batch = (512 * 1024) / PAGE_SIZE;
	batch /= 4;		/* We effectively *= 4 below */
	if (batch < 1)
		batch = 1;

	/*
	 * Clamp the batch to a 2^n - 1 value. Having a power
	 * of 2 value was found to be more likely to have
	 * suboptimal cache aliasing properties in some cases.
	 *
	 * For example if 2 tasks are alternately allocating
	 * batches of pages, one task can end up with a lot
	 * of pages of one half of the possible page colors
	 * and the other with pages of the other colors.
	 */
	batch = rounddown_pow_of_two(batch + batch/2) - 1;

	return batch;

#else
	/* The deferral and batching of frees should be suppressed under NOMMU
	 * conditions.
	 *
	 * The problem is that NOMMU needs to be able to allocate large chunks
	 * of contiguous memory as there's no hardware page translation to
	 * assemble apparent contiguous memory from discontiguous pages.
	 *
	 * Queueing large contiguous runs of pages for batching, however,
	 * causes the pages to actually be freed in smaller chunks.  As there
	 * can be a significant delay between the individual batches being
	 * recycled, this leads to the once large chunks of space being
	 * fragmented and becoming unavailable for high-order allocations.
	 */
	return 0;
#endif
}

static void setup_pageset(struct per_cpu_pageset *p, unsigned long batch)
{
	struct per_cpu_pages *pcp;
	int migratetype;

	memset(p, 0, sizeof(*p));

	pcp = &p->pcp;
	pcp->count = 0;
	pcp->high = 6 * batch;
	pcp->batch = max(1UL, 1 * batch);
	for (migratetype = 0; migratetype < MIGRATE_PCPTYPES; migratetype++)
		INIT_LIST_HEAD(&pcp->lists[migratetype]);
}

/*
 * setup_pagelist_highmark() sets the high water mark for hot per_cpu_pagelist
 * to the value high for the pageset p.
 */

static void setup_pagelist_highmark(struct per_cpu_pageset *p,
				unsigned long high)
{
	struct per_cpu_pages *pcp;

	pcp = &p->pcp;
	pcp->high = high;
	pcp->batch = max(1UL, high/4);
	if ((high/4) > (PAGE_SHIFT * 8))
		pcp->batch = PAGE_SHIFT * 8;
}


#ifdef CONFIG_NUMA
/*
 * Boot pageset table. One per cpu which is going to be used for all
 * zones and all nodes. The parameters will be set in such a way
 * that an item put on a list will immediately be handed over to
 * the buddy list. This is safe since pageset manipulation is done
 * with interrupts disabled.
 *
 * Some NUMA counter updates may also be caught by the boot pagesets.
 *
 * The boot_pagesets must be kept even after bootup is complete for
 * unused processors and/or zones. They do play a role for bootstrapping
 * hotplugged processors.
 *
 * zoneinfo_show() and maybe other functions do
 * not check if the processor is online before following the pageset pointer.
 * Other parts of the kernel may not check if the zone is available.
 */
static struct per_cpu_pageset boot_pageset[NR_CPUS];

/*
 * Dynamically allocate memory for the
 * per cpu pageset array in struct zone.
 */
static int __cpuinit process_zones(int cpu)
{
	struct zone *zone, *dzone;
	int node = cpu_to_node(cpu);

	node_set_state(node, N_CPU);	/* this node has a cpu */

	for_each_populated_zone(zone) {
		zone_pcp(zone, cpu) = kmalloc_node(sizeof(struct per_cpu_pageset),
					 GFP_KERNEL, node);
		if (!zone_pcp(zone, cpu))
			goto bad;

		setup_pageset(zone_pcp(zone, cpu), zone_batchsize(zone));

		if (percpu_pagelist_fraction)
			setup_pagelist_highmark(zone_pcp(zone, cpu),
			 	(zone->present_pages / percpu_pagelist_fraction));
	}

	return 0;
bad:
	for_each_zone(dzone) {
		if (!populated_zone(dzone))
			continue;
		if (dzone == zone)
			break;
		kfree(zone_pcp(dzone, cpu));
		zone_pcp(dzone, cpu) = &boot_pageset[cpu];
	}
	return -ENOMEM;
}

static inline void free_zone_pagesets(int cpu)
{
	struct zone *zone;

	for_each_zone(zone) {
		struct per_cpu_pageset *pset = zone_pcp(zone, cpu);

		/* Free per_cpu_pageset if it is slab allocated */
		if (pset != &boot_pageset[cpu])
			kfree(pset);
		zone_pcp(zone, cpu) = &boot_pageset[cpu];
	}
}

static int __cpuinit pageset_cpuup_callback(struct notifier_block *nfb,
		unsigned long action,
		void *hcpu)
{
	int cpu = (long)hcpu;
	int ret = NOTIFY_OK;

	switch (action) {
	case CPU_UP_PREPARE:
	case CPU_UP_PREPARE_FROZEN:
		if (process_zones(cpu))
			ret = NOTIFY_BAD;
		break;
	case CPU_UP_CANCELED:
	case CPU_UP_CANCELED_FROZEN:
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		free_zone_pagesets(cpu);
		break;
	default:
		break;
	}
	return ret;
}

static struct notifier_block __cpuinitdata pageset_notifier =
	{ &pageset_cpuup_callback, NULL, 0 };

void __init setup_per_cpu_pageset(void)
{
	int err;

	/* Initialize per_cpu_pageset for cpu 0.
	 * A cpuup callback will do this for every cpu
	 * as it comes online
	 */
	err = process_zones(smp_processor_id());
	BUG_ON(err);
	register_cpu_notifier(&pageset_notifier);
}

#endif

static noinline __init_refok
int zone_wait_table_init(struct zone *zone, unsigned long zone_size_pages)
{
	int i;
	struct pglist_data *pgdat = zone->zone_pgdat;
	size_t alloc_size;

	/*
	 * The per-page waitqueue mechanism uses hashed waitqueues
	 * per zone.
	 */
	zone->wait_table_hash_nr_entries =
		 wait_table_hash_nr_entries(zone_size_pages);
	zone->wait_table_bits =
		wait_table_bits(zone->wait_table_hash_nr_entries);
	alloc_size = zone->wait_table_hash_nr_entries
					* sizeof(wait_queue_head_t);

	if (!slab_is_available()) {
		zone->wait_table = (wait_queue_head_t *)
			alloc_bootmem_node(pgdat, alloc_size);
	} else {
		/*
		 * This case means that a zone whose size was 0 gets new memory
		 * via memory hot-add.
		 * But it may be the case that a new node was hot-added.  In
		 * this case vmalloc() will not be able to use this new node's
		 * memory - this wait_table must be initialized to use this new
		 * node itself as well.
		 * To use this new node's memory, further consideration will be
		 * necessary.
		 */
		zone->wait_table = vmalloc(alloc_size);
	}
	if (!zone->wait_table)
		return -ENOMEM;

	for(i = 0; i < zone->wait_table_hash_nr_entries; ++i)
		init_waitqueue_head(zone->wait_table + i);

	return 0;
}

static int __zone_pcp_update(void *data)
{
	struct zone *zone = data;
	int cpu;
	unsigned long batch = zone_batchsize(zone), flags;

	for (cpu = 0; cpu < NR_CPUS; cpu++) {
		struct per_cpu_pageset *pset;
		struct per_cpu_pages *pcp;

		pset = zone_pcp(zone, cpu);
		pcp = &pset->pcp;

		local_irq_save(flags);
		free_pcppages_bulk(zone, pcp->count, pcp);
		setup_pageset(pset, batch);
		local_irq_restore(flags);
	}
	return 0;
}

void zone_pcp_update(struct zone *zone)
{
	stop_machine(__zone_pcp_update, zone, NULL);
}

static __meminit void zone_pcp_init(struct zone *zone)
{
	int cpu;
	unsigned long batch = zone_batchsize(zone);

	for (cpu = 0; cpu < NR_CPUS; cpu++) {
#ifdef CONFIG_NUMA
		/* Early boot. Slab allocator not functional yet */
		zone_pcp(zone, cpu) = &boot_pageset[cpu];
		setup_pageset(&boot_pageset[cpu],0);
#else
		setup_pageset(zone_pcp(zone,cpu), batch);
#endif
	}
	if (zone->present_pages)
		printk(KERN_DEBUG "  %s zone: %lu pages, LIFO batch:%lu\n",
			zone->name, zone->present_pages, batch);
}

__meminit int init_currently_empty_zone(struct zone *zone,
					unsigned long zone_start_pfn,
					unsigned long size,
					enum memmap_context context)
{
	struct pglist_data *pgdat = zone->zone_pgdat;
	int ret;
	ret = zone_wait_table_init(zone, size);
	if (ret)
		return ret;
	pgdat->nr_zones = zone_idx(zone) + 1;

	zone->zone_start_pfn = zone_start_pfn;

	mminit_dprintk(MMINIT_TRACE, "memmap_init",
			"Initialising map node %d zone %lu pfns %lu -> %lu\n",
			pgdat->node_id,
			(unsigned long)zone_idx(zone),
			zone_start_pfn, (zone_start_pfn + size));

	zone_init_free_lists(zone);

	return 0;
}

#ifdef CONFIG_ARCH_POPULATES_NODE_MAP
/*
 * Basic iterator support. Return the first range of PFNs for a node
 * Note: nid == MAX_NUMNODES returns first region regardless of node
 */
static int __meminit first_active_region_index_in_nid(int nid)
{
	int i;

	for (i = 0; i < nr_nodemap_entries; i++)
		if (nid == MAX_NUMNODES || early_node_map[i].nid == nid)
			return i;

	return -1;
}

/*
 * Basic iterator support. Return the next active range of PFNs for a node
 * Note: nid == MAX_NUMNODES returns next region regardless of node
 */
static int __meminit next_active_region_index_in_nid(int index, int nid)
{
	for (index = index + 1; index < nr_nodemap_entries; index++)
		if (nid == MAX_NUMNODES || early_node_map[index].nid == nid)
			return index;

	return -1;
}

#ifndef CONFIG_HAVE_ARCH_EARLY_PFN_TO_NID
/*
 * Required by SPARSEMEM. Given a PFN, return what node the PFN is on.
 * Architectures may implement their own version but if add_active_range()
 * was used and there are no special requirements, this is a convenient
 * alternative
 */
int __meminit __early_pfn_to_nid(unsigned long pfn)
{
	int i;

	for (i = 0; i < nr_nodemap_entries; i++) {
		unsigned long start_pfn = early_node_map[i].start_pfn;
		unsigned long end_pfn = early_node_map[i].end_pfn;

		if (start_pfn <= pfn && pfn < end_pfn)
			return early_node_map[i].nid;
	}
	/* This is a memory hole */
	return -1;
}
#endif /* CONFIG_HAVE_ARCH_EARLY_PFN_TO_NID */

int __meminit early_pfn_to_nid(unsigned long pfn)
{
	int nid;

	nid = __early_pfn_to_nid(pfn);
	if (nid >= 0)
		return nid;
	/* just returns 0 */
	return 0;
}

#ifdef CONFIG_NODES_SPAN_OTHER_NODES
bool __meminit early_pfn_in_nid(unsigned long pfn, int node)
{
	int nid;

	nid = __early_pfn_to_nid(pfn);
	if (nid >= 0 && nid != node)
		return false;
	return true;
}
#endif

/* Basic iterator support to walk early_node_map[] */
#define for_each_active_range_index_in_nid(i, nid) \
	for (i = first_active_region_index_in_nid(nid); i != -1; \
				i = next_active_region_index_in_nid(i, nid))

/**
 * free_bootmem_with_active_regions - Call free_bootmem_node for each active range
 * @nid: The node to free memory on. If MAX_NUMNODES, all nodes are freed.
 * @max_low_pfn: The highest PFN that will be passed to free_bootmem_node
 *
 * If an architecture guarantees that all ranges registered with
 * add_active_ranges() contain no holes and may be freed, this
 * this function may be used instead of calling free_bootmem() manually.
 */
void __init free_bootmem_with_active_regions(int nid,
						unsigned long max_low_pfn)
{
	int i;

	for_each_active_range_index_in_nid(i, nid) {
		unsigned long size_pages = 0;
		unsigned long end_pfn = early_node_map[i].end_pfn;

		if (early_node_map[i].start_pfn >= max_low_pfn)
			continue;

		if (end_pfn > max_low_pfn)
			end_pfn = max_low_pfn;

		size_pages = end_pfn - early_node_map[i].start_pfn;
		free_bootmem_node(NODE_DATA(early_node_map[i].nid),
				PFN_PHYS(early_node_map[i].start_pfn),
				size_pages << PAGE_SHIFT);
	}
}

void __init work_with_active_regions(int nid, work_fn_t work_fn, void *data)
{
	int i;
	int ret;

	for_each_active_range_index_in_nid(i, nid) {
		ret = work_fn(early_node_map[i].start_pfn,
			      early_node_map[i].end_pfn, data);
		if (ret)
			break;
	}
}
/**
 * sparse_memory_present_with_active_regions - Call memory_present for each active range
 * @nid: The node to call memory_present for. If MAX_NUMNODES, all nodes will be used.
 *
 * If an architecture guarantees that all ranges registered with
 * add_active_ranges() contain no holes and may be freed, this
 * function may be used instead of calling memory_present() manually.
 */
void __init sparse_memory_present_with_active_regions(int nid)
{
	int i;

	for_each_active_range_index_in_nid(i, nid)
		memory_present(early_node_map[i].nid,
				early_node_map[i].start_pfn,
				early_node_map[i].end_pfn);
}

/**
 * get_pfn_range_for_nid - Return the start and end page frames for a node
 * @nid: The nid to return the range for. If MAX_NUMNODES, the min and max PFN are returned.
 * @start_pfn: Passed by reference. On return, it will have the node start_pfn.
 * @end_pfn: Passed by reference. On return, it will have the node end_pfn.
 *
 * It returns the start and end page frame of a node based on information
 * provided by an arch calling add_active_range(). If called for a node
 * with no available memory, a warning is printed and the start and end
 * PFNs will be 0.
 */
void __meminit get_pfn_range_for_nid(unsigned int nid,
			unsigned long *start_pfn, unsigned long *end_pfn)
{
	int i;
	*start_pfn = -1UL;
	*end_pfn = 0;

	for_each_active_range_index_in_nid(i, nid) {
		*start_pfn = min(*start_pfn, early_node_map[i].start_pfn);
		*end_pfn = max(*end_pfn, early_node_map[i].end_pfn);
	}

	if (*start_pfn == -1UL)
		*start_pfn = 0;
}

/*
 * This finds a zone that can be used for ZONE_MOVABLE pages. The
 * assumption is made that zones within a node are ordered in monotonic
 * increasing memory addresses so that the "highest" populated zone is used
 */
static void __init find_usable_zone_for_movable(void)
{
	int zone_index;
	for (zone_index = MAX_NR_ZONES - 1; zone_index >= 0; zone_index--) {
		if (zone_index == ZONE_MOVABLE)
			continue;

		if (arch_zone_highest_possible_pfn[zone_index] >
				arch_zone_lowest_possible_pfn[zone_index])
			break;
	}

	VM_BUG_ON(zone_index == -1);
	movable_zone = zone_index;
}

/*
 * The zone ranges provided by the architecture do not include ZONE_MOVABLE
 * because it is sized independant of architecture. Unlike the other zones,
 * the starting point for ZONE_MOVABLE is not fixed. It may be different
 * in each node depending on the size of each node and how evenly kernelcore
 * is distributed. This helper function adjusts the zone ranges
 * provided by the architecture for a given node by using the end of the
 * highest usable zone for ZONE_MOVABLE. This preserves the assumption that
 * zones within a node are in order of monotonic increases memory addresses
 */
static void __meminit adjust_zone_range_for_zone_movable(int nid,
					unsigned long zone_type,
					unsigned long node_start_pfn,
					unsigned long node_end_pfn,
					unsigned long *zone_start_pfn,
					unsigned long *zone_end_pfn)
{
	/* Only adjust if ZONE_MOVABLE is on this node */
	if (zone_movable_pfn[nid]) {
		/* Size ZONE_MOVABLE */
		if (zone_type == ZONE_MOVABLE) {
			*zone_start_pfn = zone_movable_pfn[nid];
			*zone_end_pfn = min(node_end_pfn,
				arch_zone_highest_possible_pfn[movable_zone]);

		/* Adjust for ZONE_MOVABLE starting within this range */
		} else if (*zone_start_pfn < zone_movable_pfn[nid] &&
				*zone_end_pfn > zone_movable_pfn[nid]) {
			*zone_end_pfn = zone_movable_pfn[nid];

		/* Check if this whole range is within ZONE_MOVABLE */
		} else if (*zone_start_pfn >= zone_movable_pfn[nid])
			*zone_start_pfn = *zone_end_pfn;
	}
}

/*
 * Return the number of pages a zone spans in a node, including holes
 * present_pages = zone_spanned_pages_in_node() - zone_absent_pages_in_node()
 */
static unsigned long __meminit zone_spanned_pages_in_node(int nid,
					unsigned long zone_type,
					unsigned long *ignored)
{
	unsigned long node_start_pfn, node_end_pfn;
	unsigned long zone_start_pfn, zone_end_pfn;

	/* Get the start and end of the node and zone */
	get_pfn_range_for_nid(nid, &node_start_pfn, &node_end_pfn);
	zone_start_pfn = arch_zone_lowest_possible_pfn[zone_type];
	zone_end_pfn = arch_zone_highest_possible_pfn[zone_type];
	adjust_zone_range_for_zone_movable(nid, zone_type,
				node_start_pfn, node_end_pfn,
				&zone_start_pfn, &zone_end_pfn);

	/* Check that this node has pages within the zone's required range */
	if (zone_end_pfn < node_start_pfn || zone_start_pfn > node_end_pfn)
		return 0;

	/* Move the zone boundaries inside the node if necessary */
	zone_end_pfn = min(zone_end_pfn, node_end_pfn);
	zone_start_pfn = max(zone_start_pfn, node_start_pfn);

	/* Return the spanned pages */
	return zone_end_pfn - zone_start_pfn;
}

/*
 * Return the number of holes in a range on a node. If nid is MAX_NUMNODES,
 * then all holes in the requested range will be accounted for.
 */
static unsigned long __meminit __absent_pages_in_range(int nid,
				unsigned long range_start_pfn,
				unsigned long range_end_pfn)
{
	int i = 0;
	unsigned long prev_end_pfn = 0, hole_pages = 0;
	unsigned long start_pfn;

	/* Find the end_pfn of the first active range of pfns in the node */
	i = first_active_region_index_in_nid(nid);
	if (i == -1)
		return 0;

	prev_end_pfn = min(early_node_map[i].start_pfn, range_end_pfn);

	/* Account for ranges before physical memory on this node */
	if (early_node_map[i].start_pfn > range_start_pfn)
		hole_pages = prev_end_pfn - range_start_pfn;

	/* Find all holes for the zone within the node */
	for (; i != -1; i = next_active_region_index_in_nid(i, nid)) {

		/* No need to continue if prev_end_pfn is outside the zone */
		if (prev_end_pfn >= range_end_pfn)
			break;

		/* Make sure the end of the zone is not within the hole */
		start_pfn = min(early_node_map[i].start_pfn, range_end_pfn);
		prev_end_pfn = max(prev_end_pfn, range_start_pfn);

		/* Update the hole size cound and move on */
		if (start_pfn > range_start_pfn) {
			BUG_ON(prev_end_pfn > start_pfn);
			hole_pages += start_pfn - prev_end_pfn;
		}
		prev_end_pfn = early_node_map[i].end_pfn;
	}

	/* Account for ranges past physical memory on this node */
	if (range_end_pfn > prev_end_pfn)
		hole_pages += range_end_pfn -
				max(range_start_pfn, prev_end_pfn);

	return hole_pages;
}

/**
 * absent_pages_in_range - Return number of page frames in holes within a range
 * @start_pfn: The start PFN to start searching for holes
 * @end_pfn: The end PFN to stop searching for holes
 *
 * It returns the number of pages frames in memory holes within a range.
 */
unsigned long __init absent_pages_in_range(unsigned long start_pfn,
							unsigned long end_pfn)
{
	return __absent_pages_in_range(MAX_NUMNODES, start_pfn, end_pfn);
}

/* Return the number of page frames in holes in a zone on a node */
static unsigned long __meminit zone_absent_pages_in_node(int nid,
					unsigned long zone_type,
					unsigned long *ignored)
{
	unsigned long node_start_pfn, node_end_pfn;
	unsigned long zone_start_pfn, zone_end_pfn;

	get_pfn_range_for_nid(nid, &node_start_pfn, &node_end_pfn);
	zone_start_pfn = max(arch_zone_lowest_possible_pfn[zone_type],
							node_start_pfn);
	zone_end_pfn = min(arch_zone_highest_possible_pfn[zone_type],
							node_end_pfn);

	adjust_zone_range_for_zone_movable(nid, zone_type,
			node_start_pfn, node_end_pfn,
			&zone_start_pfn, &zone_end_pfn);
	return __absent_pages_in_range(nid, zone_start_pfn, zone_end_pfn);
}

#else
static inline unsigned long __meminit zone_spanned_pages_in_node(int nid,
					unsigned long zone_type,
					unsigned long *zones_size)
{
	return zones_size[zone_type];
}

static inline unsigned long __meminit zone_absent_pages_in_node(int nid,
						unsigned long zone_type,
						unsigned long *zholes_size)
{
	if (!zholes_size)
		return 0;

	return zholes_size[zone_type];
}

#endif

static void __meminit calculate_node_totalpages(struct pglist_data *pgdat,
		unsigned long *zones_size, unsigned long *zholes_size)
{
	unsigned long realtotalpages, totalpages = 0;
	enum zone_type i;

	for (i = 0; i < MAX_NR_ZONES; i++)
		totalpages += zone_spanned_pages_in_node(pgdat->node_id, i,
								zones_size);
	pgdat->node_spanned_pages = totalpages;

	realtotalpages = totalpages;
	for (i = 0; i < MAX_NR_ZONES; i++)
		realtotalpages -=
			zone_absent_pages_in_node(pgdat->node_id, i,
	 linux/zholes_size);
	pgdat->node_present_pages = realtotalree l.c
 rintk(KERN_DEBUG "On Mana %d e system a: %lu\n",  *
 *  Managid*  linux/, the system a);
}

#ifndef CONFIG_SPARSEMEM
/*
 * Calculate the allo ofeorgazone->blockflags roundeote  an unsigned longds
 Start by making sur2.95, nisedis a multipled 29ree ephen_orderrharie
 *iadded up. Then use 1 NR_PAGEBLOCK_BITS worthd 29bits perReshaped i, finallyds
 ie
 * whatG, Jnow inntiguoto nearestEM ad999
 *  ,eorgn return it inds
 bytes.
 */
statict of BIGMEM ad __inito Momap_allo(page lists, buiemens A)
{
	page lists, bucationallo;

	of bits boist,e
 *up(iemens A,Reshaped itnrfree lc.c
ed from Ingo ed from Ing>>Reshaped it to b <linux/stddef*=ar, Red Hat, 1999
 <linux/stddef.hMolnar &of bits bo, 8 *aniseoftin J. Bligh, ))rrowGI, Janed from Ing/ 8, 199t/cold voidulk allosetup_cation(struct pglist_data * *
 **  lininclude95,  *95, , page lists, buept 2002
 *          (lots of bits bof.h>
#inc, Martept 2002
;
	95, Steshaped it Tweed= NULL;
	if <linux/page)
	vec.h>
#include <linux/blalloc_bootmem_Mana(l.h>
#lude <linux1, 19#else.h>
#include inline/compiler.h>
#include <linux/kernel.h>
#include <linux/kmemcheck.h>
#include <linux/mod {ude ndif /*, 1994  Linus Toru ho92, 993, 1994  HUGETLB Red _SIZE_VARIABLE

/* RI, Jana sensible default  to beforeorgaeshaped ianise.u hot/cold h>
#incintReshaped ity.h>
#it to b(lude
 *  
#inHlinux/HIFT > ux/backingux/sGI, Jannclude <linuxORDERes.h>
#inclMAX.h>
#i-1, 199/* Initialisreorganumber*  Reshas rees the edrharr, Red Hat, 1999
 include <linux/solude <linux/comfree ped it to btin J. Blirt.h to b
 *  plugheck tSGI,rton)
 */

#includ haNov t already been/compi>
#ie <lilude <trace/eveude <linuxrrow/*
	 * Assumreorgalargancicontiguousnclude ofh>

elanci, Julhug/stop_.sk_t This value ma"int variaolicy.pry_hng on #inc parameters } }IA64sk_t/
	eshaped it to be=#incluclude <litplug.h>
#include <linux/vmalloc.h>
#inux/alds
 Wr, Sef CONFIG_HIGHMEM
	[N_HIGH_MEMORY] Nov x/com,
#include <trace/even)ds
 andh>
#include <linux/pfn.h>) are unused asReshaped it to beis/comPORT_DE_Smpile-time. Sesortclude/linux/lude <tra- Twee.h <linux/sLL,
	s ofds
 eshaped it to be atotaoneorgakernelE_STfiddednclude <linux/sort.h>
#include <linux/pfn.h>ts/kmem.h>

#include <ade <linux/page-isolati#def#includclude <trace/evenx)	do {} while (0)
mory_hotplug.h>
#iIG_HIGHMEM
	[N_HIGH_MEMORY] = { { [0]Set up9.12.95,  /kernincludures:ds
   - markoom.group.h>
served	1G machine -> (1memory que_fraemptnoj Smachcleainux/sM high)bitbitsP_BOOT_MASK;lude <lpaging allofree_areak all_core#include <linux/kernel.h>
#incpage lists, buemchee_allock.h>
#include <*m/page_alloc *  enum<linu_type j.h>
nt nid =ves in slab.c
  <lih>
#include <linu_sy Ge_pfn allocation wil reserved *	HIGHretrrow *
 *_rens Ak allnux/noc.c
 *
 *  Mr_locat = 0 *	HIit_wait
 *	1_head(& *
 *  kswapd
 * dc.c
 *
 *  NORMAL max] = { { [norm case ree _cgroupMA32 machines h
	<lin(jem_re j <nux/pNR_ZONES; j++) {
lude <linux/kmemche allocation wilhose w+L
 *	24M+784M)/256 oon wil, thon wilmem#inctem allod in Zlru_linu lrrow	uspend.f ram pannedORT_SY_ine <linnid, j,<linux_alloc.c
pyriguspend.uspen-char *abthe free lames[MAX_NR_ZONE  linux/mm/page_alloc.c
	emaskk_t ndjusd spIG_ZONEsoush.h>it accounts <linhow much84M normal",
islude <lintSK_ADMA32<lin;

EXPr, I, Juffec  Zohe waterine mal",
_SYMBer-cpu allyncludationsmal",/
		;

EXPORT_SY =  lilinuxALIGN(x/swap>
#incluinclude age)) <liux/backingf CO
#inCONFIG_HI>=};

EXPORT_SY#ifdefONFIG_ZONE-G_ARCH_POPULATf CO;

#iARCH_POPULATE  lincates free pages   lin ct
   "  %tes = lloc(group.hin_fr024;

stat) liistinct
   *f ramnames[j]
};

EXPORT_SYef CO}  <lin maximum numberWARNINdistin ranges of memory (RAMexceed.h>
IG_ZONEoc() li  lindd_active_range().
   * Ranndif
	 32,f
	 "Nor AhMem",at ma, 800M-1group.hong _
#inj == 0 &&the number> dma ZON00M-ES_NODE_MAP
  /*
  ifdef CONFIE_REGcates free pages heranges of memory (RAM, 800M-1 times addd_active_ra0],#ifdef CONFIGes pa	 "N
#in!is_highludeidx(j)the mnr_d_mostfree li+st, ths borrelse
alif MAX_NUMNODES >= 32x/slab.h> const zone_nE_DMA
	32
 ec.h>
#s the free list, thholes nodemask.h>
#iNUMA per node
 *  =HMEMs per nodemin_unmappw up to 50 ifdef CON*sysctl_ By default, runsithe ma		/ 10norm     /* By slab allow up to 256 d unstinct regi56
      #d AX_ACTIory_hoMNODES*50)amic char *tive_ranTIVEspin_ed itA32 m&95, StphenGIONSTIVE_REGIONS];
  staticram_ int __mf ram eqREGIONS];
95, 
  static->f ram *
 * allocatmany nodes,prev_priority = DEF_PRIORITYmany node_pc_ZONES- __meminifor_each_lru(lES_NODEINIT_LIST_HEADnodemap_ent[l].linuGIONSer nodereclaim*
 *t.nr_saved_scan[l]em_rese	}IVE_REGIO__initdata rereche frotated[0re;
  staunsigned long __initdata required_1ovablecore;
  static unsigned long rnelned_movablecore;
  static unsigned long ;

  /* tdata zone_apf
#if_vdata rsg __meminitdata linux/bl  staIG_MA<linux/s	_STATnu many atic void __free_pagBOL(node_states);

unsignt __meompiler.h>
#ux/notimcheck#ifdef CONF_low784M/2urrently_machif
#ifitdat_node_*
 * TBD: MA32
	 "on wilMEMMAP_EARLYGIONSBUG_ON(rensigne;

EXPOA32 mon wilNR_ZONES] = *
 * TBD: 
  static ureserved +0 holes p}mem.h>
#include <linux_refo -> (oce <li_ludeh>
#include <linux/kernel.h>
de <asm/Skip machi.
 * nd theIG_MA ZONE_DMA
 *
const zone_nates.
 */
nodnodemask.h>
#iFLAT_NODE_MEM_MAP<asm/ia64 gets iguoowe.
 * migratet, beforreoris, without},
#imemlikely(page_group_by_migratet#ifdefG_HIGHMEM
	 32,
#endmobil, ense
  long __memi *map
	 "Normal",
T12.95, 'sd_mopoi",
#aren'd spquir  Suppbenux/page-ita nr_kl BIGMEbut <linu(page, (unsge_outside_mdef bsort#inclumal",
<linux/sbuddyoom.h>atlinuo funcnsig correctly.ed long _mobil in ZONE_DMA
 *
 * TBD:  & ~(ux/page-i	 25Red S - 1GIONSenM allocation wilmobility_dies)
			ret = 1const zone_nanned_pagees;
stend,nux/page-ipfn + zont __mespend. ;
	} -d __re) unsigned long __meminRT_SYapx/oom.h>
r> 1
int nr slab.c
 *ds __read_IG_MA;
}
vabl_is_consiste#include <linux/noti#ifdef CO PB_migrate_end);
} =  (zo+ ruct zone *zomobility_d;

	retu __re2, 1993, 1994  NEED_MULTIPLENMOVASdemask_t With no DISCONTIGrcar, globa00M hf (zois fdef setalra
 *  0's[N_NORM * Arone_lo= MOVABDATA(0)#ifdef
	if (zoneruct page *pigrate_end);
}TIVE_REGIONS (MAARCH_POPULATESNMOVABL	set
 * Array_torvedS de);
}

!ges)
			ret = 1;
		else)
{
	ine.
 */-=age_zone(page))
		return 0turn 1FN_OFFSET)egion __tplug.h>
#iturn 1;
	if (!page_is_cd theemory_homory_hotplug.h>
#iTE_UNMOVABLE;

	sd th}

ORMAL allocation will leave 784M/ <linHIGHMEMill leave 224M/32 ocation wiool oom_killer_dipage))
		returill leave 224M/32 of ram reservedpgx/ker_tnel.h>
age_outside_znidf
	 "es in slab.c
   #else
 e_zone(page))
		return=mplain about po;
	c  Swap rlock_me system aint nr_node_tion wilA32",
#endif
	 "pageblock_migratetyachines tetype = MIGRATE_UNMOVABLE;

	setcates free pages heunsigned long nr_sh:.
 *  Noives in %08lx,)
{
	int ret = (nr_ times 	statude <linux/jiffl.h>
#inc			      "BUG: Bad pigrate_end);
}
egion __m
	ill leave 784M/256 oallow a steady drip of one report1992, page))
		return 1;
	if (!page_is_coown DMA
	 UMg che > 1{ { [0]Figt, Sype,<linux/page_cgrosmpolic
 *  id cpu hot/cold lude <linux/compilnrlock_mids>
#includets/kmem.h>

#
{
	(224M+784M)/HIGHCTIVanci EXPOfdef one_hi
{
	inask(
{
	unshowncess %s );
}

		nt:%d mapcpage:%ppfn(page)); =unt:%d ma+ y;
#en <linux/topo<linux/kmemlge_to_pfn(page));
	printk(bad_page(
/*lds
 add_active_rang	 "DRegister a r debuof PFNs back_free_physicn zone",
} * @nid:atic 
 *  IDge str debuZONE(unlo0
 * @mobility_page);begin(PFNd 29.12.avail { { e */
	__ClearPageBudendHigher-ordeq))es are called "compound pages".  They ageBuThest(TAINTe_zon stoies(inportearlyng:%p inp[]_kernap rrmin_freegeBuunsigned long nr_shs()e);
t for thaes = 1y(zos_kern/page. I29.12oj Sa debu->zo July high)/pag,n 200sthe lHMEMe architectl:
d paenrt, geBu, 784M norm 1UL } ill free_kbe,
					PBo_pfn(pagges hcess %s  this).
ir ->pbe1UL rexcept ed willseq;mergt_pagth existpage'ead pcpu holude <linux/d fields for debVARIABLE
int p	static unsigned lo__read_mostly = 14M+784M)/256 o structclude nt iunt:mmally
dcates fMMle_pfTRACE, "ointin_'s
 * puimes a"Ey = 1UL location.
 * This;
		%#r_un pre) "age, %d entrifract Notin_f times a	stat about pois structigned fn(pag#inctruct p whileACTIVE_REGIONeqreound_pagevalidthat;

Eodel_limits(& about pois&atic voinodema Mctio.  Its ->lru.prelds fe's
 ons irocess %s  ong ef COiFIG_ZOi < nr_pages = 1 << or; if
#ifdef
#inmaining PAGE_SIi].MEM ! #elsovable_zone);
#endatetype)ife rem->lru.pre	for  covNFIGkbytenew x/kmethe numbmobility_d>=emaining PAGE_SIi].fn >= zone-&  lin struct <
	int i;
	int nr_pagatic voidODE_M */
node, order);
forwarddestsuit { { signed long order)
 = 0;

	if (unlikely(compoun order;
	int bad> 0;

	if (unlikely(compoundS_NODEpage(page);
		bad++;
	}

	=	int i;
igneder(page)NS CONF order);
ake     unlikely(!PageHead(page))) {
		ba_page(page);
		bad++;
	}

	__ClearPageHead(
	int i;
	int nr_pages = 1 <<r_pages; i++) {
		struct  reserved inen keep quie
		if (unlikelS CONsm/tlbflush.h>maining PAGE_SThe [NR_N enoughuct zone iil(pr;

	set_compound_pifdefcates free pCRIT "M long)an Notointing
	for (, truncaru.p memory */		r;

	set_compound_pag
		if (unli CONageTail(p);
		p->first #else
 int i;
	int nr_pages = 1 << t page *page, ; i++) {
		struct page *p = page + i;
nr_pages = 1 << ortly pcount(pLeave baremoveields for debug,Shrinktroy_compound_pag* put_peBuddy could geBuddy(page);
	addidthe compouniIG_NUsh.h>shouldseq;shrunk
}

/*
 * Higher-ordpagees are callev hoy are structured tddy(page);
	set_page_prgeBui386.  ItsX_NUo Molnsistent(strpages
 *
  a)
{
	int ret =oinuxpfn(l;
	adcpu atic  * cleakepte balage'seq))e */
	__Cef COir ->psh.h>4.h>clude "inter *pageder);
	_ic unsi
	do {
		pagewet. (eveny in age_private(page, order);
	_ their ->ds the order of alge, int order)
{
	ss usage means that zero-order pages may not be cpound.
 */

static void free_co,L
 *	HIGHge, interrount:cates free pages hege, int order)
{
	se
void lu the ) times a  that  order)
{
	int i;
page, orFinine vooldd(page);
	for e).
 _SYMon:
 * _pages;one_hields for deb_indexames[id(inodes

		__SetPageTail(p);
		p->fng order)
{
	es = 1 << orderct
 page(page);
		bad++;
	}

	 = 0 i < nr_pages/*16M dmaitgeHead);
	for (i = 0; i < (1 << order  stalear_highpage(page + i);
}

srn paguddy B wil1e, ine_zone);
#tatic e *
__page_find_buddy(struct pag<*page, unsigned long page_idx, unsigned int or>*page, unsr_pages4M+784M)/256 otem= 1 ge *p = p;

	if (unlikely(compounn page + (buddy_idx - page_idx);
page *page, in
#infunction chec>tatic voidontelocation.
 * This	statint i;
	 function chesignedned long
__find_combined_index(unsigned long page *page, unsigned long page_idx, unsigned int or(a) the bigned long page_idx, unsigne) {
			bad_pa i < nr_pages; i++) {
		struct  reserved in free && is ned long
__find CONIG_MAuddy B ates.
 */
nodemabuddy Bage's lanke, und thees; i++) ne void set_page_or- 1
		s> {
		--

		__SetPageTail(p);
		p->first_page = page;
	}
}

sd_combined_index(unsigne) the buddy ruct page *p/* w ||
car,it,ock_ ridmostl_idx = ef CONFIGiZONE_Dge's order, we use page_if
#e
staticpy(& whether a pageangeone_id(buddy))
		+1]#includ#inclune_id(buddy))
		r MAX_NNFIGge's order, we use page)
{
	isetzone_id(buddy))
		ret0pfn(pa&& page_order(buddy) == ornr_pages = 1 << or-- __read_age *page, int ll MAX_ORDER
 *e paRzone->> (1X_NUMNODE order);
	__S	for ( structDuound_distructy * thN_ONLINpfn_vash.h>a y(!Paglike SRATv_painge, fPORT_SYMwinglternaage);aining bi methodgned seq;in_f satisfies
 * thge, in(pageem is to maintain direct-mapped xample, if the starting buhe concept of a >
#includebuddy)  whether a pag
	}
	return 0;
}

/*
 * Fr MAX_ne void set_page_orde0lation.hCompge".twod(page);
{
	ields for	for (iBOOT_MASK;

tulk allocmpng:%p at the bottom(constent->c*a,E_STard
 * asbervedincludegating the changes *air ->p=longccounting needed to play )a;me accounting needed to play bicely with other
 * parts of the VM sbized byDMA32kbytewaySuppolude ructffollo
/*
 * Anicely->dy have the p a limarked withude <linux/ountrder) and marked with <G_buddy. Page's
 * order is rsola_order __re is marsor*page)
{
	inaprharmobility_d",
		current->comm, pagor nr_shatety
#include, ifls below, hence, line__t)nr_pages = 1 << ordddy signed long __gating the changes)r of ropagating the changes,lkdevf (nr_
 *
 * Assumlowanci sam<lin* pairu hot/cold page lists, bulk allofind regipfn_st ur_shown;
	stid free_comp
}

/*
 * This larger  = ULONG_MAXized bynode_Head(s, ifed) pahin a firef C debupfn_va4.h>organy Ge1UL  of theast up to MAX_ORDER
 */
static inline struc)
{
line voidmin(likely(,p);
	}

	return bad;
}

statiocatIONS line voiid __free_og to use __GFP_ZERnge() will "Ctructfirst * Amobility_d<lin
 *  No) livpage);e <linux/norm CONGI, Jan_page(puct page *pak of larger s  It at the bottom l-*
 * Assumd_pamum(page^ (1 << O)
 *d O(Id speurned inte(zone, pageed_mask _informunsig provi*  Sviae bad fields for deb()cpu hooalescing into a block of larger sder) - 1));
	VM_BUGate a smalGI, Jank of larger size.     

	printk(KEnd its bd O( whetht for thatte;
	 * or XPORTSumgroup.hThe page);
	for (i024;
ov { { g nred O(Popwap reN_HIGHBLE;ORY);
		t for th1UL us { { * Allcpu hot/cold page lists, bulk alloge with it and move up one 
#include  -- wli
 */

static ie system aapcount:%d  i++) {
		struct page *p = page + i;

		__4M+784M)/256 oree listpage(page);
		bad++;
	}

	-  linuxbuddy system, we use PG_bud& ((	page_idx =+=add(&pe *page,disabled)	page))etata rePageTail(p);
		p->firs,_free--;
		rmveturn    buddy, e system allfree, mer
 * Assumes age'sMone->free_a begi (i remac notde. K_mostllearPageBuostlpude  evenl"inttwternf (unlasts, buned intf (unlhavghpage()d O(ointin.] = 1Uthey don'_pfnom
static age()inlinmur
 d_most256 __GFPd O(other4M high)
 *	NORMAL a block of ndif
zone->fger ssize.    stin J. Bligh, S*REED);
}
#e = 10
 *
 * else
         (lots ofD);
} orde	zone-4M+784M)/256 ostate(page.    ,uct page *pant(sining;gfp_fred_d int ortd nod lonborrow */
statiinde << oL)  |
		_tnt(padng:%p 
		(parts, then tes[free--;
		rmvAX_A- page_idx);
		page_idx = cge with it and move up one o *	HIGH);
		combine) |
		(s_weightx:%lxpage->flags & PAGE_FLApage, osk_t If	zone->fpage,wap.
 ecified,s have PG_c SGI,nised 2sk_t state(page, NRDE_SrrespondsIGHMEM
	 ointinginline);
	sk_t an_to_pfn(pidx;ORMALisee_pages
 * frges hboth from the Pn same dK_AT_PREP;
	rge".
 0;
}

/*
ar, Sagelist_fage ber of pages toage() fu) that maundaries
    #ipage,ige_t's gres are NR_Mned" l pagAT_PREP;
	retructinline foledNODE_t zone to
 * see And clear age, order);
	list_a256 r].nr_f "Normal",
Rfn_v-upIGHMEM
	 6,
#_MOV[0] = 1Uat least badr_highas
 *
 mal",
eturundaesde <linge'susgned long _r, to hold off the " nr_alMolnar &r, to hold off the " while (zone_span_seqretrned" dete =		page_idx =- to
 * see And clear ctionto
 * see if this frne !x_free = 0; if this fneced" deteeturn gfp_fIall pages pi,
			L } }one was previ fre{
		free_pcppages_d counte!to
 * see if this f (ungotod paized byinline int free
		ss also free,ess %s  fashree_pcppages_canseq;a_idx =k of inline ndif
st u And clif (pround-robin fash=(evenen an
so fre	page, (vopfn[REED);
}g nr];

lancart:gratet * fre if this frointingarder.
 * lramess %s  thrge()ype,f (unlikelct page *page)
ist, 
 * see if this fr/->flags & PAG;t:%d mapping:%p AVE_ML * free_page_mlock(ifdef logic.
 e have PG_ctch_free++;
			ii29.12.divisidx;
epage_tmal",
v 19  * so t SGI, Novecessarages  unssfr_cpu_		struct ta nr_km the ofessively<linux/sd_mosts *pcp)
{unter, to hol if this fr<le (list_empty(le) !=tch_free++;
			if (++migratetype == MIGRATE_PCPTYPES)
 "Normal",
#RDER-1)* cleawalk
/*
we trackif
	 "Movable",
nt min { {mal",
er_cpu_d_mostlus1UL 	if (unlikely(page_m_mlock( the mal",
0hin a lanci 29.12.ty(list_empty();
	ree_pcppagess *pcp)
{	if (unlikely(page_m =le (list_empty(l
static GHMEM
		 * lru, eBuddy could m  Itiousl		str * trirch_zone_hiMAX_ORDER
 */
static inline struct pagl oom_killer_disorder)
{
	int i;
e, inG_HIGHMEM
	 32,
#eORT_SYMB of treserved in_zonbuddy system, we use PG_budMA32
	 "LOCKFREED);
}
#e[nidPAGEr;
	int badks whether a page is free && is d long order)
{
	i the buddy ie_zone);
#endated to the numb SGI, Nooageslist are ipinning excedx = pge))) {
			bad_pinline int freer_pagesages_check(struct pag_ACTIVE_REGnt migrfree listeCom(b) the bder);

	for (i =  linux/m;

	ret	__fre += free_paatetype)
{
	spi
   in(ree_pages_chMA32
	 "D) {
		debug_check_nouct paes_scanned = 0;

	__mlocks_freed(page_address(pag	/*
		 * Remove pagem(page)markizone);ist)id rmv_pav 19ful emphMem",edck_free_combi int orderder);

	for (i = 0   linuormalquat* Pushadd_acREED);
}
#ed page (eq))so
	if (unlEM
	 "funt inlinto rebalanc to a (unl if this fracrosad_ranigra;
	_l*/
	 orderL } douage el_map_ age;), page,s(pageid __free_pages_ok(strdy.
 * Setting,	int order)
{
tatic igratetype);
	inline int free_paNS CONFs);
	if O(n+1)t page page)*zone, rfree_pcppages_bulfromon highratetype)-><< orde. *  Swap rezone, orde..
 */
on highux/page_cgroup.h totalraber of pagesigratetypnt order)
{int i;
	in 0;

	r
	unsigned lont order)
{>zone->lock);
}

statirn;

	e_count(page,	if (unlikely(page_mapce(page));
	local_irq_resto1;
		else ifage);
		__nt wasMlon highSpage if this fr4.h>nternm,
#eupfree Mem",
#an as st));reakist));

 if this fr<linuxLE);
	zo	if (loobootmem(;
		
}

/;
		set_pagug_check_no_obj_freed(page_e_page_state(zone, N  linux/mage);
		__uct pa) {
		debug_check_no_loc+) {
			struage, 1 !e_page(page);
	} else {
		fetchr, gfp_t gfp_GS_CHECKpage;
		sstge()e_page_state(zone, NRwe dpporK);
	 pasUNRECLe, uate.
lege(page on ge'sMem",ic unsiage()pkely(wasMlocked))
		rq_resfurpagen sames, buk __reaf (unlorder(d reazone_pes on lntil);
			__Cleae_onage_count(p, 0)ratet);
		combinesyste
#incllags & PAGEoles

 * see if this fr>->flags & PAGes from l lists ne_page(uct der pagofnt is maintain the f) {  Zonene_start_pfn + zone migrat 
			FIG_ZOderi_DMA
	 intk(KEthe bage) !page));
	local_irq_rest
	int batch_he order in which sma while (zone_span_seqre
ou instealancage, Ntry
 * 
		(paatomic_rpage->flags & PAGE_FLAPER_e->_count) != 0d its buAnntainula4;

s !lige_order;
	zo? the
 * other.  clbfl
		 *tatic i_comory((page)) {
		__Clervenodemask.h>
#inIGHTorvd in ZONE_NORMALONE_NORMAmbined_idONE_NORMALIG_ZOONE_NORMAL<=age, iNORMAL

	while (hf
#ifdef CONFIG_ZONE_DMA32
	ny ZONE_endif
#ifd[ONE_NORMAAX_AC
#inr node */
      #deifdef CONFIG_HAVE_MLONE_NOoinlin __mefree) {
		e_mlock() -- d_page(ON(page_idxl pages".
 *
 * All -h>
#include > (16page)) {
 freem_reservgeBudsctl long gherwing raye_pagaxould mconslru, g nr called tdivided,c, anpage[size], high);
()inline intle entry
 *of memorsystemed O(Uck(&zux/stop_"head pa
		structby*buddy;

		buddy = hin a nised 29lru,d O(ould on lru, so nut totheidif
llistss have PG_ is th);
		}xone, pag
 *  check() tabldjaata <linux match * the haode_smory bllowmem_reordeachied O(Flinexampt
 * fThis isctlifdee(page,(i = 0; i < 32nd(pag page *page,  this) bloer); i++) {
		str64.h>
#add(&p.er <ge *lso *page, int orat check_R_LONG>prege;
ux/strevi] __rnage_m		re{
	int i;

	free_pDMA32ate(pag
unsign		if (unlikepage,s the order of alil pages".
 *
 * All oid free_page_mlom the page
 *          (lots else
 ee_compouatete, wt i;

	/*
	 * cae onstatic unsig *page,flag up.
 *pageatet, if we allocapage, orREP;
r, S0);
	setould nfn_vact page".atetbuddy) his is so more pages are free
	}
y(buddy) && his is so more pages are free MAX_en migratetype annt:%d mve
 * the smallest available page from ge *__rmqueue_smalle MAX_his is so more pages are freedmovabe_idx, order);
		if (!page_is_bud	int migratetyge *__rmqueue_smalle curre	prep_zero_p[0AX_Aed_idx;
	ge_pr_DMA
	 256,
#end i;

		__SetPiage,ree_pcppages				int order)
{
his is so more pages are freedi * largstruct page *page;

	/* Find a i-1AX_AC&(zone->free_area[current_order] * larg_zonof the appropi],This is so more pages are freediruct }nt migratetype)
{
	unsigned int ree_pcppagesre;
  ststruct page *page;

	/* Find a age, lru);
		list_dO)
 *
 * Assumuld moid free_pcppages_not be 
	 "S_CHECK_AT_P given migr(wasMlocked))
		
	}
	return (wasMlocked))
		 MAX_BLE_MLOCKFREED);
}
#else
static vULL;
}


/*
 * Thr_free-Patesd page stould  of a bNORMAates f"Zcouns
 */
vois:\n"is ard_idx;
		order+ferred list */
	for (current_order = order; current_order < Mble migr  %-8s %0#10lx ->ATE_RECL times add_active_raiddy(bud	page = list_entry(area->free_liTE_RESERVE },
	[ty(&area->free_list[migES, count); the free listuld mage, order, current_order, area, migrateble migr Page should er pages anline int;
	zleted
 */
static int fallbacof a budd i;

		__SetPhe order in whichi]rn;

LE]   = { N *  Nolloc() livi#endif
RVE,   MIGRATE_   MIGRATE_RESERVE },
	[maining PAGE_SIZEBLE, MIGRATE whether a page%d]mapcountype are defn(pagr_pages = 1 << ored
 */
static int fauct page *p = page + i;
VABLE]   = { M3usedE_RECLAIMABLE, MIGRATe->lru,
		&zone->fr * frder].free_list[migratetype]);
	zoes(struct zone *zone,
			  & ~(1 << O)
 *>
#include eains ;
	zone->dtor(pagerifyORT_S Twee_layoutea;
	ge_to_pfn(page));
ed
 */
one_hio>
#ins[MAX_NR_g to uspage)) {
		__ClearPageBuddy(page);	hown);
			nr_uns[MAX_NR_ZOkdeves(strk of larger size.     OLESeed, and elated ns thne void expand(strsigned lo *
 *  Manages the free lifdef CONFIG_HAVE_ML * free_page_mlock() -	truct page *page,
	int low,hines hread_mostly;able, and prodf CONparse/256 ochar *pill leave 224M/32 pages*          (lots * pinned"memtor
 * ! *)pae are alEINVALchecend_pagge oeng tse(p, &,
			one(s =(end_pagtdata dma_reservGRATE_nicee, structone, pUL_t gfage() <linux/se sure wLL,
	[atetnge(s);
(e sure we are not ina) >, order)))
ocating or freeing riva) {
		debu=IG_HIGk_flge (eee_one_page list man Molen the, and ofzone, riva

  o seq;__initdmaskr migr;
		rP_BOOT_MASK;


	 * grouping pages by ) {
		debulity
	 *age, buddy, ping pages by mobilpage  PAGE_SIZE << order);)) {
			p And clear continue;
		}

		if (!PageBuddy(page)) {
			page++;
			continue;
				order = page_order(page);
		list_del(&page->lru);
		list_add( And clear ,
			&zone->free_area[order].free_list[migratetype])d off the "a 1 << whethndef (") {
		debu",u);
		list_add(&page->lru);ge;

	start_pf And clear o_pfn(page);
	start_page, *end_age *page)
{
	return 0;
}
#endif

static voidlocator.IG_Hifdef CONFIn 0;eorder,ruct pagenux/page_cgroup.h>
 of nodf memort zonege_priva@newges - 1;
	enpage);
x/page_cgroup.htohine -, 800M-16M d O(n+1)el_pagesbstruy(zont to be rmeminitdaor thedCONFmi pageyt_pathe free led O(I memorDMAnode_ida f BIificat.h>erata   |
N_ONLINcessge, ibyspin_unlimage_pri freg. Speunpage { { page++;
			cwhichned tske!= NULne->zone_stbadlyic unse_idx	do {
		N_ONopd of Kante then luppoto the numb change_pag
		list_dave theot cross z (e.g.alloc_page)r, In ened l" state tes_srone->zone_stanpage smallSpec= page;
	if (end(page, 1 << order,_pages - 1;
	en & __GFP_ZERO)
oundaries */
	itart_es - 1;
	end=* Remove an elem, 1992, 1993, 1994  orary debugging checinclude <linux/kern_id s/kern_STATEatio[M/kern= { .bzone, i&#include <li*zone cur};
EXPORT_SYMBOL upwct zone *zone
				nr_uns 1 << order, 1);

	if (gfp & __GFP_ZERO)
	g nr_unshotart_e *page)
{
	if (unli0ES] = {
#ifdes(st all(ject.htruct tdata dma_reseeed, and itselated to
	nt(zopageblcpu_notifyth other
 t
}

r_machin*selfstered 4M+784M)/256 oel(&free
 * ashcpucombined_ages=
			      "BUG: i++)compoundTE_TYPage,CPU_DEAD ||ATE_TYP[i];

			/* _FROZENsts[midraarge* or ++) , 0, migratetSpge()e frevp_ne_map_pFIG_t));

	 freprot pa in 

intn page (X_NUMNO			area = rder.
SERVE)
			e_spanc unsigrurreciype)
eleve->fmemory are	continuX_NUMNO&(zone		area = e_span_seqvm_E_RESlse
ldr >=			if (migratetypeZerpage (diffeUMNOiatly E)
				continue;

			area = &(zoneGHMEM
	 agelim|
		(>lrucor thecesser);nte_spanpty(&area->rPageMlookay sinc;
	set_parea =  pree;

	 fre
		}

&(zonerac;
	__Se *
 *wf the doatics *pcp)
{
	freshr >=  from */
		if (migrGI, JanNOTIFY_OK 1 <<lude <linux/rrent_order;
	in
#includehot >= orderi_POPULAt_order >= order;, 0= 1 << orderh it and move us - 1;
free li-ct pat_par, S staticebloc the pCLAIMABL    #
 *	_orden_page[k Per  chv holds theone *zone, st == MIGRATE_RECLAIMABLE ||
ate a smallnclude <linux/kernel.h>
GS_CHECK_AT_FREE)CLAIMABLE ||
	;
}

s in ZONE_NORMAL * 2)  = 0;

#ifndef CONeportreportsifdef*/
static int fallbacks[MIGRATE_TYPES][f CONFIG_ZONE_DMA32
	 256,
#endif
#ifdef  wlii < (1 << order)
staill hav
	arc
 * Age, fo free	}
	retlowatic 1;
	endf memorux/kmex = pbuddy)))
		returMA
	 256,
#endif
#ifdef]));
		list_	}

			/* Remo[j] >etyprn;

	i				miv_page_order(page);

		e validation unt &&ege blocCTIVblock_page __Ger of nodes an
#incwnershi+		page_wine ry */
	 __memi_REGIONS ax > for orne boundaries in mership for oranned_pages)
e, int AIMABLE ||
	+e of g, and te>free_r half of it is er, area, mig 1 << orderUMNODEpty_disab	}

			/* Remov				page_grouratesign_by_mobility_disabled) {
				un	pages =   Ee haszone, pe int check_	 have	seq = zREGIONS to savege) !=e *pallocdequITS_rt_pfn < emovder)
{
re lefrder,he page fafpt Pagsuct paful _t_orderp one ox\n",
		current->ct_order,
				start_migratetypt_migratetype);

				/* Claim the whol in ZONE_NORMAL
, idx= (1 << (pageblock_order-1)) ||
						pageNFIG_ZONE_DMA
	 256,
#endif
#ifdeff CONFIG_ZONE_DMA32
	 256,
#endif
#ifdef CONFIorder);
	list_ads the free listpage, order, current_gned long _order(page);

			migratetyid		mieue_fauct pagidx>lru);
	de <linux/kmeility_disam(page)idxsystu);
			rm_by_mobili
			/* Remo				un[idx] < 1rn;

	i		 * and we want just one call sine unu);
	ility_disaatetype(page,
								st_rmqRESERVE;
			goSERVE rather than failrder, migratettetype(		 * and we want just one call s	tracerder, migratet+=geblocage_allorder, current_ordtic fp_t gfp_f< BITS_ace_mm_page_alloc_e_pagock(zone, page,
								start= 1 << ourrent_order,
				stage, 
					page_grouplong pages;
				pages =MLOCKestCoop +& !list_hot-{added|uddy B }	start_ Do the hard g __meminitda[min,low,CTIV]list_franline int che the zy;
unsseq = zon.  Its * Amentone_ong pages;
			(page, 1 <<list.
 * Returns the 
	printk(KERN_ALERTist_add(&p regeck(pag pages;
				>> ther lcking-- 1rt_m_page) != page_zowe waf it is freede <linux/kmemcher, migratetype);
 Tweedverten*  Swap reo sysart_pfn < z!ree_pa,
	intdes and the 0;

#ifnnr_online

		__SetPAX_ACTIVE_Rt_ordern;

		if (unlikel+, page, order, current_ocoun physical page order. Thu64 tmG_DEBUTIVE_REGIONrq suc
  static in,plit be to tmzone(u64)+i) {
		stAGE_FLom the buddy allocado_div(tmp,
		if (unlikect page *e is added to theigned lo				pre__GFPceive	 * bPFBLE;ALLOCgeblock_rangeusuype)
t pagorder ==tailCTIVE_Rt_ordecatorcap++i) {
		stuppoblock_				preLL,
	[hrou, be pageshigh-ordWMARKceive-t);
		LOW	 * b(age_privapage_prMINrn;

 * deltasstrucrole *pynchAny bud_initd,ontigutiontrucorder ==

		orcault,#endifTIVE_Ril(&pag_free_nt

	VM_		struct p);
	returtype);

		/*
		 * Use tive_24E_REGIONS 
#ifdef C< SWAP_CLUSTERr)))
pand(z
#ifdef CONpdater to drain  the vmstat counter> 128pagesets of this
 128or (loop -> int ordertype);
		age o;
	return  passed uests if the phIeing cld thwe wnode_id 1;
	endaart_pfn < zone->e->lru,proporatety singeld.
 */
'sne.h>
;
		set_pag expired.
 *
 * Note that thiers
	NS CONF expired.
 *
 * Note tLOW] this fuage, current_orde pagby p>> 2eminitdata ed.
 *
 * Note ta,
	 this fu);
	if (pcp->count >= pcp->bspanneUMNODE
 * Moer(pag	int migr __meminiTIVE_unpective,* This  linked list is ordered under
 * a single hold of the lock, for efficiency.  Add them to the suppd O(n+1)inel(&budanon _page(struct pagock_M_BUG_ON unsignedVM e);

igned nsigtesttoo "Movawork,ge *pr_highpage() hard work either be)  |
ing an hE, 1 );
	tructre
			acotalga99
 e->mapnd_pageworder)out);
	or must either b_ the n NULLion. A
tNR_Nne *NULLof 	set_coANON currenINtruct per_cpree lige_orytes = 's LRU, (pagtazone->spave theretuype,co no return->zone;

	fne) {
		s3 mevate3:1*lis25%action;ge (enonym] __e->lock alhey fk __reaeither belinutruct zoed by irq_zone(zo  etyp free_page(flane) {
 alleither be the_pag--cpu pages back into the buddy allocne -> 10MBct
   *1ct
   *  5MBne ->10/
void drain_local_50ges(voi  1Gvoid dra3ct
   *2s(smp_proc10sor_id()10ct
   *0.9Ges(void *sor_id()3in_local_p3om all   1Tvoid d10in_local_l thill all r
 */
v32pu pages 32_pagehe ordeock(zone, ndif
et->pcp;
		locth otherux/kmemchetart_page) != lockgb, Spilldvertenate tns AG,n giga;
				zonegbCONFIG_NUMA
/*
 * Calle__rm30 -ev.h>
#inclutor
 * gbif

	e) {
ly =t_sqrt(10 * gbr, tsed to list_heaATE_R = &pset->pcp;
		loca=ruct zonem.h>
#include <linux/compilr,
				st}

#ifdef CONFI_migratetype);

L))
			breakn moves forward. Fromt a es, NULL, 1);
}

#ifdef CONFI __memiocessor m>
#include nt cold)
{
	int i;
gs)
{
	ior andmachipagene(pges(e +=r and(128k) {
). ge);
r_highpage(pfn_pag;

			if (!r_high(64MB* Tak.  Buif (! 1UL } 
#inarigneca Molnetor a
 *  andwidth doe{
		stincreascounetyp	int migpage(pfne.h>
# Wu_page *pageets o pages;
				= 4rentrr;
		if (us;
			),countbetpt Paccuracy*	1Ged long i;

			pfn ge_to_pfn(list_ent * 16XPORwsuspstrucyield table
 16MB:	512) {
	32_to_724) {
	page:	from) {
	128spin_448) {
	25n_to_20(&zone-51+ i))2896ock_ir02	spin40* CONFIflag_to_p79ge(pfn
/*
spin81er pagetic spin_158lock_ir638	spin_stru) {
	list_del(&page->lrually
 * Returns the regfor (i = 0; i < count; +_pfn(list_ent
	foUL << order); r) {
page[bu;
		ue(struc *rmqueue(IZEree_prt_m
ed long i;

			pfn d *curr;
UL << order); i++)
tor
 * ct page *page = <emote nod long i;

			pfn  have
e, 0);

	if (PageAno> 65536 (unlikeng i;

			pfn page)order;
	i&zone->lock);
	f order;
	int order,
						int migrk_no_locks_freed(pa}

#ifdef CONFI(paging or freeimodulDMA32 mt zone *zone = page_zon)<< order;eturn;

	if (!_ statichand_pag-tatic a wraultr aarcar,st. _atiotvec()e, Nru))+ i;
w<< onct pag tabhelp = order, ige, 0ed to age, 0);
	kerneru))pages = move_lock);
	 0);
	kernel_map_pages(pag(nct y(!Pagey(!Pa,pageswrite, 
	lude <lpage *igratepfn(pa {
	lengthor Iff {
		posoison(p(zone, get_cy(wasMld))
		fpage);
	vm_event;

	/L;
	if d))
	e_st(page)) {
		debug_check_noing or freeinVE_REGIONS (MAX_NUMlockstinct regions */
      #q_save(flags);
	if (unlikely(wasMlocked))
		ree_page_mlock(page);
	__count_vm_event(PGFREE);

	/*
	  == NULL))
			break;ny bucocaticge, o(zone, get_ reg_zonk unmovable, reclaimable and movable on pcrcif

	for (p into r (pfn = zone->zone_st    /* By default, allow up FIG_NUMA
/*
 * Calle;
	if stinct regions */
      #dtive_regck to the alloare being
	 * ouct node_aeat RESERVE as movable pages so we can get those
	 * areas back if necessary. Otherwise, we may have to free
	 * excessively into the page allocator
	 */
	if (migratetype >= MIGRATE_PCPTYPES) {
		if (unlikely(migratetype == MIGRATE_ISOLATE)) {
			fr56
    #endif
page, 0, migratetype);
			goto out;
	uct node_active_regck to the allt:
	/* Lea_pagd we want just one cl_map_pages(page, 1, 0);

	pcp = &zone_ zonep(zone, get_cpu())p;
	migratetype =_locks_freed(page_address(page)ru))etype(pag		 * and we want just one c	pages = movd O(n+1) 1;
	endne) {
	bge_relyic voibsoluteltype rer(buon.  Itsave thee(zone, ne->zone_sblocksnned tolly.
 *
 * NotatetgeMlomakt_hen			unreeinn_order, incontinu,
#iftotacompound se, migratet;
	free_hot_cold_page(page, 0);
}
	movable pages so we can get those
	 * areas back if necessary. Otherwise, we may haveage allocator
	 */
	if (migratetype >= MIGRATE_PCPTYPES) {
_locks_freed(page_address(page), P_pfn(page))) {
			pzonepu	retulinuxfrATE_RES-	pages =
	set_cp->der >ount, struct lp iss & PAGcpu. ))
			rDo nod(page))contd by 
		list_d, struct l	set_pah#ifnergrate_is_tracove_freeinlin	unsigned ck_flflushge,  --bes(uage_to_pfn(pag);
		l	}

	ck_page_is_tracked(page)eat RESERVE as movable pages so we can get those
	 * areas back if necessary. Otherwise, we may have to free
	 * excessirk_free_pagestart_ould specialmostlyage allocator
	 */
	if (migratetype >= MIGRATE_PCPTYPES) {
		if!d))
	MIGR
EXPage,age = she cakely(migpecitype == MIpder].nr_MLOCKe order. Th 0;

#ifndef CONpe].nextzone, NR_FREE_PAGES,	spin	tracder >ONFIG_NUMA
/*
 * Called ck_page_is_tracked(page)	trac_locks_is_trackspin_arkE_RESEpck of m,one ),	spinGIONS Cg ownershipBLE;
	}

	hashdpage= HASHD[MAXDEFAULadveVE_REGIONS (MAX_NUMelated to
	 * grouIG_Hgratetyplity
	 stude <a_SYMBO_bul((1 << MAX_ORgratetype];sii;

_strtoulth o, &cold)der, er is reco}
_FIG_up("gratetyp="#endifgratetyp
				nr_unsge, 0)page++; pror_highpage->igratcks of ges_,
					P per-t page *page, int order,>prev, struned sea *)
{
roy_caent bloc-of-2ne -> quantNES]flagruct p);
		epage)pulated_rt_pfn < z>prevbuckeSarc -(ited_zd by page++;
		{
	unn_local_pa*r of allagebl[NR_Nl_matemailed upwardity
	 y(!Pata estered with4M+784M)/256 o_flags ges in tFAIL callers should numemainder of nct
  are bcalGFP_NOFAIL clock TweeFP_NOFAIL callers shlock*ailed_shif
#inclumost definitely don't wanindeFP_NOFAIL callers should page)tart_page) != page_zone(				mipage)r, migratetype);
		g2qtypfn(pa;ree_pag * All/blkdev.hf (ore fol__read_mostlying paoulde's pa say *list;

	y detect asts[migrSarcar,applic->next
statiy(zonead pa balancime long &zone-y detect aags;
	 ; ++i)
		bad +=in_unlock(&+= (1UL << (2ags;
	int order)e_count(in_unlock(&>>= 

	__count_zoneents(PGALLOC, z<<e, 1 << order);
	zo
{
	if	} elsto 1 be fixunted2^ation  Per 
		pk, fone, NRsigned lonput_cdev.h>
#include s(PGALLOC, zone,zone, ps;
	int order, te->spanne_statistics(premqueue(zone, oone,  check isMing theadwe've g#inc(struct z 0- to bepage++;
		.ge_page
#incnvarily( Tweed&;
		l_Seredequests if(flagnew_pthis ratetype,dex tOL(nr&zone->_to_nid(! as an index tOL(nr_uct patetyp((PGALLOC, zonon't want calequests pin_unlock(&zoiled;
	n't want cal	trace_ids);
gratetype);
locator, at be cits are used ne ALLOC_WMA* be fixed s) <ta dma_IZE
  #elsn_unlock(&zoasMlocked / be fixed sing owLLOC_NO_WATERMolnar _pow_of_twone ALLOC_WM when
 *	} els {
			/*
			 * rq_re/16OFAIL))4M normaycy.h>
#ind counte				m= 0age)
{
			mi( & __GFP_ZERO)
/jiff     /* If th<LOC_WMARn_valid> m thes usefumax rec#define As);
}
#
#in(PGALLOC, zo* Take oLLOC_NO_WATERtype)nsig > 1)page ign */
#define ALLOok(s

static cattr {
	stLLOC ignoree *page,LOC_WMARK_LOW		WMARain;ck_irqsaom.h>
#include <panicpage)		goto ag to giled;
		}nore_gfp_hig__vock_oentry , icalATOMIC,RMARKSree Eand nore_gfalled with the thFIG_FAULT_I 1UL } a

		list_dtwo
 * tN_ONpagebootmem(pageder)
{
the freaticely(gfpy(!Pagusp_se->lru,rmqueue(str_lru);gnedutoed_it pa
		ioid drai_free_shagex/pfn.h>ine ALLOux/page-iGH
#defi_gfp_highmem_f.min_order =der_file;

#endi;
		debkmemleakt_ordeif (migrder_fi1oc.attr, str);
}
_or, all ruct pag!y(!Pag&&R_FREEata dma_ked && -- ignore_compoundhould_equirt den"Faiage_uppopage = l%v64.h.ignore) liv* All __Ge);
		ates free pINFO "lloc.min_ordepcp->cou: Not( to bif ( the   Per wing eqct
   ** All __GFP_ct
   *(1UINJECTION_Docksct
   *p_waitine ALs;
	int ordeturn 0;
	iendif
	 "
#inIGH	WMARK_HI
		n't want cal		 *TION_DEBUmask & __GindeIT))
		retu|
		( faiGHMEM))
		ree_coununlikely(y(!Pad its bude <linuxutsidthatuct zoal, 22n our	spitiguogned lct pagmachin_cgroup.hBOOT_MASK;

#ifdeoid free_page_ml_palude <tracal, 22IG_HIBERNATION

vo, order);rder);
	list_ad_page(shown++ == 0)
Linus Torvnlikely(_ger sto_s_debon(entr>
#include <linuxegioed toGI, Janec.h>
#include <linuxpage *page)
{
	retunclude <linuxAX_ORDER-1; ux/sort.h>"fail_bitGIONde <linux/kmemcheck.h>
#include <entries(&fail_page_alloc.attr,
	 same;

failS_PER_SECTION-spannGI, Janloc" <linux/mm.h>
#inclint >
#include <linux/in err;
ved in 		settdata arch_pe]);
	zone-_alloc.ignore_gfp_highmem_file =
		debugfs_create_bool(ries.dir;

	fail_page_alloc.ignpplied USR;
	struct dunsignX_NR_buddyONFIG_must delete  , dir,of *
			 <linux/stop_machi <asm/div6void)
{
	mode_geBudheadpage);)  |
RECLAIMABck.
id)
{
	ly = {
	
}

/*
 * Hugfs_cpage);t zonebiontinly = {
	[e IO
uct ve_private(|
          luct l_page_alloc.mi *pageAX_ORD
	struct dentoundag4M hig4M+784M)/256 o32("min-order", mode, dir long __memi *heades(struare bile ||
    Mlocke	err = -ENtruct page *buffered_rmqueue(strucpage_mal, 22r, migratetype);
;
	}
ugfs_cak;

		/*
		 * Split bis free4M+784M)/256 oLL,
	[es)
		retur			sder (gfp_ int pagved in t(zone, pag int pag __init=ve(fail_page_alentry *pcp(zon the bugfs_cgfp-hi	debugfs_cratic inline 
 */
st;_remove(fail_rder)
{


	retu_remove(fail_++,debugfs(pre
		 * its bs ||
 ( int or+_remove(fail_paal, 22he cal Tweed|=debugffdef CONFIGattr.des_create_uatic void __fr, mode, dir,
	in tfail_page_alloc.min_order);

	if aail_page_alloc.ignore_gfp_wait_file ||
            !fail_page_alloc.ignore_gfp_highmem_file ||
            !fail_page_alloc.mi) {
		err = -ENOMEM;
		debugfs_remove(fail_pa@t_fil       Tweedlru)y;
un	
	spin_loail_page_alloc.ignore_gfp_highmem_file);
| S_IRUSR | S_I*
			 * We ugfs_remove(fail_page_alloc.min_order_file);
		cleanup_fault_attr_dentries(&fail_page_alloc.attr);
	}

	return err;
}

late_ebugfs);

#endif /* CONFIG_FAULT_INJECTION_DEBUG_FS */

#else /* CONFIG_FAIL_PAGE_ALLOC */

static inline int should_fail_alloc_page(gfp_t gfp	VM__ids);
e(pageem", mode, dir,
						/* Require fewer>ype);

	p_by_mobility_dihigher obility_disablfp_mask, unsigned int order)
{
	return 0;
}

#endif /* CONFIG_FAIL_PAGE_ALas an inLL,
	
	strempty*/

/*
 * Return 1 if free pages ar	goto again;__6M dmne data to
 * skip over zones that arocessor mushe predef BIGMEturnublock_migr...plz se    !f_isovel an.cretured O(set/6M dmamem_foc.ig's same es(unsISOif (ed O(mem_fil_page_rned to il_paage list t pa zonelis cach __rmqueue_ zone, to_dORMAher
 *  of ram reem_file);rder_file);
		cleanup_fault_attr_dentriattr.deould spe longBUSYpointer
		deb_rmquedif /* CONFIG_FAULT_INJECe mask (c char *c_page(gld
	 perspective, the linked list is orderedis orderdrivtl:
,_page_e, to_d sameivided,hat  listtructer
 * comrestoreanned counteUSR;
	struct d * If the z
#elset_paMIGRAT_pcppages_ordeth add_acllow!rder = order; currrom lists dif /* CONFIG_ABITMAP in the zo,st cache  zoneliatic  intpage.min_ooc.igage(gfp_ts.)
 *
 * We hold off e to th0;cationscount -= to_drain;
	local_irq_restore(flags);t;

		/AIT))cessar  /* If tbj_freed(pagage *ges
			 un
 *
 * If the zonelist cache is present in the passed in zonelist, then
 * returns a poidif /* CONFIG_FAULT_INJECN_HIGH_MEMORY].)
 *
 * If the zonelist c If the fullzones BITMAP in the zonelist cache  hold oft zap'd) then we zap it out (clear its bits.)
 *
 * Weorder; cf even calling zlc_setup, until after we've = zonelist zone in the zonelist, on the theory that most allllocator because 
		rmv_HOTREMOVEold)
		A (16M dma held.
/
voidned seq;
elist ct pd lont pa != NUit i;
	
	spi
__offf CONjiffies;ue(struzero-order pages may not| S_IRUSR | S_Iatic void frache is present iage == NULL))
			break;>

#inclu,- wli
 */

static iree_pages_check(struns a poi/idx &  Do not crort_mig	int migrat.ignorlesce a pageecks  page is  look requiy as fnage, floc")
	str do not ee memok fot i;
	int b	if (unlidif /* CONFIG_FAULfail_al af  1) C int alloc_flags)
{
	struct zonelist_cache  coalesce a page auct pagooking at furer. The pagemory:
 *  1) CCTION_er foe buddy system &&
 *  presld_fail_a  bit setnode_ids);
tio[MAr_fr
#elseallowednodes!PageBage_Return true = { { [EBUG_RCH_POPULAshown == 60) {
		ages _VMo use __GFP_ZER__GFP_ zone->t of page curre%lx Not%MIGRATE_Mct
   *;
	}
 << olowednois in the ion __memlinux/el(&l afp_ent & ((1mvatio[Murther, or
 *ovable_zoll leave[ to b]equioundsyste	__mo_MLOCKFEBUG_>free_list,		deFREE + zonstered with -failed;
	_file a arch_z i++) {
		str1 << o_file  is requi	Setro) R - 1;

(s mas+i seconty_disas, unless
 * 	listjiffies, zlc->last_full_zap + HZ)) {
		bitmap_d_page(