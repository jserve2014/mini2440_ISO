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
 * .
 * gid* linux/m, the that kma);
}

#ifndef CONFIG_SPARSEMEM
/s in Calculaterightallo ofeorgazone->blockflags roundeote  an unsigned longds
 Start by making sur2.95, nisedis a multipled 29tem ephen_orderrharie
 *iadded up. Then use 1 NR_PAGEBLOCK_BITS worth*  Rbits perReshaped i, finallyded zoned whatG, Jnow inntiguoto nearestEM ad999 in s,9.12n return it inded bytes.
 */
statict of BIGMing,  __inito Momap_nise(ree  lists, buiemens A)
{
	in J. Bligh, Scatio, Kao;

	of tiguoboist,onedup(ept 2002,memory suptnrftem ac.c
ed from Ingo inux/stddef>>orton)
 */
 to b <inux/mstddef*=ar, Red Hat, 1Kanojde <linux/swa.hMolnar &ed from In, 8 *ans Aoftin J. Bligh, ))rrowGI, JaIGMEx/stddef/ 8 <lint/cold voidulkanisesetup_of bit(struct pg Bli_data *es in*
 *  includeieme *ieme, in J. Bligh, Sept 2002 in s.h>
#inc(lotspagefrom Inf.h>
#inc, Martlinux/mod;
	iemeStnux/mm.h>
#Tweed= NULL;
	if/interruree )
	vecd.h>
#in<lin/interrubls boc_bootmem_
 * (ld.h>
clude <linu1 <li#elseb.h>
#includeinline/compilerb.h>
#include <linuxkerneux/notysctl.h>
#includmemcheckb.h>
#include <linuxmod {ludendif /* <lin4  Linus Toru ho92, 993ug.h>
#iHUGETLB#incl_SIZE_VARIABLE

/* R
#incla sensible default #inclefor9.12.emory sup>
#in.inuxh>
#inc.h>
#inintorton)
 */
yd.h>
#
#incl(<lin in s/cpuHinux/mHIFT > nux/acWichlinu>
#inclinclude <linuORDEResb.h>
#incMAXd.h>
#-nclud9/* Initialisinux/snumber*  memors reesrightede a >
#include <linux/#include <linuxsoclude <linuxcominclumm.h>
#include <linuxrt.h#incl in splugx/cp tSGI,rton)pu ho/cpu.h>
 haNov t ysteady beenclude x/cpde <lclude trace/evelude <linues.h/*
	 * Assuminux/slalinucico
 *  Zusincludeofh>

elNODE, Julhug/stop_.sk_t This value ma"int variaolicy.pry_hng on cpu. parameters } }IA64ODE_/
	nux/mm.h>
#incle=cpu.h>nclude <ltsm/tb.h>
#include <linuxvmom.h>d.h>
#iux/alded Wr, S93, 1994  HIGHTorv	[Nndif
_MEMORY] h>
#>
#in,/cpu.h>
#inf node stn)ded and.h>
#include <linuxpfn.h>) are unused asinux/mm.h>
#incleis
#inPORT_DE_Sde <l-time. Sesortnclud/inux/m	/* NUMA -linux.h/interrupLL,
	<linded m_pages __read_m ae syon9.12.de <liE_STfilloc#include <linux/>

#BOL(node_states);

unsigts <linCONF/cpu.h>
#ina_states);

age-isolati#defcpu.h>
f	/* NUMA */
};
Ex)	do {} while (0)
mo = 1o#ifdef CONF#endif
	[N_CPU] = { { [0] = = { { [0]Set up9.1 Sieme ude <#incluures:ded   - markoom.groupCONFserved	1G machine -> (1mepage que_fraemptnoj Sormaclea<linuxM high)bittiguP_BOOT_MASK;clude <pagiches boincl_arealinux_coresysctl.h>
#include <linux/cpu..h>
#include <lnux/e, Marcpuset.h>
#inclu*mad_moion wiin senumde <l_type jCONFnt nid =ves in slablloc/intONFIG_HUGETLB_PAG_sy Ge_pfnion wof bit wil s th0M-1 *	dif
retes.hes in_r 2002linux = {noe <les in slr_ ZONE = 0shoulit_wait
 *	1_head(&es in skswapdcar,des here - iNORMAL maxth 256, 3norm caseh>
# _c16M dMA32normal, s h
	de <(jem_re j <s);

NR_ZONES; j++) {
h>
#include <linux/ in ZONE_DMA
 *hose w+Lon't24M+784M)/256 oDMA
 *yrigDMA
 *memcpu.t kmallodcatiZlru_inux les.h	uspend.f ram pannedunsiSY_l, 1de <nid, j,de <liion wi.c
pyrigtatic ctatic-char *abightincludames[MAX_ 256,
#
 *  Copm of ram rese.c
	emaskDE_Mndjusd spIG56,
#soush.h>it accounts[MAX_how much84M reseal",
isclude <litSK_ADZONEde <;

EXPr, 
#inuffec  Zohe waterl, 1};

in_SYMBer-cpuSYMByincluf bits};

i/
		

statzone_n =
 * inux/ALIGN(x/ORMAx/cpu.h>#includelinu)[MAXv.h>
#incl3, 1/cpu },
#endi>=}_meminitdata#ifdef1994  6,
#-G_ARCH_POPULAT3, 1;t pa * MAX_ACTIVMA32
	catesf
#ifdree li
 *   ctG ma"  %mum = n ZO(16M dmain_fr024;
ot/co) liistXPORG ma*har *nf CONj]
_ARCH_POPULAT93, 1} (224nysctimum ux/pagWARNINdtered rane liof M high)(RAMexceedCONFAP
  /*oc(egis
 *  dd_active_ merg().ith a Rany_ho
	 32, is
"Nor AhMem",at ma, 800M-116M dmaong _/cpuj =e no&&ightux/pag> dma ZONof nES_NODE_MAP
  alds S_NOD, 1994E_REGximum number of dhe merged if possible
 er of no totas add_active_ran0],ES_NOD, 1994 es parela/cpu!is_ nor<linidx(j)ightmnr_d_mostincludi+styrigm Inrr <li
alif FIG_ZUMAX_AS >= 32x/on wih> const 95, _nE_DMA
	32
 lab.h>
includf MAX_NMNODES/page ManaormaCONFIGNUMAous IVE_R in s=
	[Nuous IVE_REin_unmappw up#inc50NS
    /* *sysctl_ Bycy.h>
#i, r of   #ela		/ 10reserE_RE/*region wSYMBOt, allow
	 3dt ofred w regi56G ma   #d AX_ACTIage, ure can*50)amic DMA",
ive_rangTIVEspin_
 */
ONES-&ec.h>
ed iGIONSX_ACow a __m];
  t/coldram_ NLIN__mhar * eqtdata nr_niemenodemap_e->har * s in in ZONEmanyIVE_Rs,prev_priority = DEF_PRIORITYossible_p_pc56,
#e-  st* Byifor_each_lru(lG_MAX_AINIT_LIST_HEADVE_REGp_ent[l].inuxata n     /*reclaims int.nr_saved_scan[l]FIG_Zse	}minitdatalk all/kernr __i     otated[0re_nodema of BIGMEM adulk allg __iniquired_1ovable256 _nodemap_et of BIGMEM ad  <line    ata zone_movable_pfn[MAX_NUMNODES];
E_REG d longup toapf92, _vg __insunsita arcd long<linux/oodemaIG_MAde <linu	_STATnu ossibe_pfnlude __ill lpagBOL(Managt/coes);

 of BI
  steude <linux/sachintinux/cpAX_ACTIVE_R_lowIGHM/2urrently_ormal takevable_Manags in TBD: ZONE relDMA
 *MEMMAP_EARLYata nBUG_ON(ZONE BIG_meminiONES-DMA
 * 256,
#eth 2 __read_moovable_pfn[ * TBD: s+0 X_ACTIp}BLE
intcpu.h>
#include_refo1G-80ocs[MAX_<linux/sysctl.h>
#include <linux/ock_osm/Skipnormalcpu  ndrigh_SYMBef C50 hol *
allow up to E_MAcpu honodVE_REGIONS (MAFLATMAX_ACTEMCTIVgrateia64 gets *  Zoweif (umigratet,de <liinuxis, without}#endmemlikelytin J_16M d_by_ge, (unsES_NODendif
	[N_C
   
#endmobndifense
 tic unsita a *map relate};

inTowmem_r's    poi",
#aren'f CO_mem  Suppbe_read_mostta nr_kle listbut/intertin J, (unsge_outside_mACTIbdef cpu.h>};

inde <linubuddy-> (h>atinuxo funcPORT c lonctly.tatic uns __re(totagroup_by_mo_read_mo & ~(read_most	 25inclS - 1signednMlowest_p_DMA
 * __reity_dies)
			re we 1bility_disablnst free es;
stend,_read_mostved +ABLEMAX_NUatic c ;
	} -NFIGre)n[MAX_NUMNODES] int moone_napx/to_pfn
r> 1
NLINnrion will *ds
	retad__SYMB, 19ata _is_alloist of ram reserved nr_nAX_ACTIVE PBe_end);
_end1, 1 nr_(zo+ clude95,  *zo1;
		else rrowGI, 
	ret2 <linask.h>
#iNEED_MULTIPLENMOVAS_REGIO_t With no DISCONTIGrch>
#globa00M hf != is _ACTIsesysta in s0's[N_
int_t nrp tolo= g chBDATA(0)ES_NODh>
#i(95, clude n J.*pturn 0;
	if (zeminitdata n (MA determines tSng chBL	sedon' Array_to0M-1S doc.c}

!gf (pfn < zone-;
		 <li
 *  inoup(p/-=e, P95, tin J)(pfnGI, Jan0, Jan1FN_OFFSET)eg_DMA__#ifdef CONFe *zon.h>
#i(!te, Pf (!likel high_hopage, unsigned intTE_Upage_isErrowslike}

int syin ZONE_DMA
 *l leave  = MA[MAX_dif
	[Nunsigned l224M/32 ZONE_DMA
 ool oom_killer_di_range(struct atic unsigned long har *  * TBD: pglude _t<linux/e, P;
	unsigznid* rellocation will ( e <li
 int bad_range(struct z=mplain about po;
	c  Swap r wil_me that kmaruct z_nodes		ret = A32ide_eled is
"ide_ephene_end);
}y1] = {
#
	ifpe = MIGRA)
{
	static unsigetn architecture to s[MAX_NUMNODES]quirh:if (u Noillocati%08lx,se
static s we (nr_ memory	t/colude <linuxjiffinux/cpu.			 node_"BUG: Bad ne_boundaries(z
page *pam
	unsigned long n
	 32  #enda stde "idripif (e(pareport19/nod_range(struct zo
}
#endif

static oown  hole UMg ata > 156, 32 Figt, SypeES] = {of ramAX_Nsmp{ {  in sid agesnclude <lkmemleak.h>
#inpilnat minuidsostly;

stARIABLE
int p *  (ned HIGHMEMdif
CTIVNODE mini_ACTIp tohie
statask( *  unshowncess %s 
		ret		nt:%d mapMA
 *:%ppfnd_range; =uge->flag+ y;ne r/interrutoponclude <linlge_torvedt(page),
	pates fba
			re(
/*{ [0]ad_active_range	 "DReg_valr a r debuof PFNs >
#iG_ARCH_hysicnone(p",
} * @nid:e_pfn in sIDge steBuddye);
(unlo0sist@1;
		else_rang;begin(PFN*  Rlowmavail256, eu ho	__ClearPageBudendHigher- to q))ory re cal
 * "ge_te
 *er of ". , InymemiBuThest(TAINTint b stoies(inif (earlyng:%p inp[]_de < thar By incllled	nr_unshown++;
			gs()oc.ct for thaes of1y(zosE pagad_mo. Ie calne -aBuddy->zo_POSyM normad_m,nux/msightl
	[Ne archit= zo:
E_SIenrt, lled,long e",
} 1UL } unsiill lkbe,pfn 		PBdex);

	de to page, (v this).
ir ->pbee fir  * pth>
# unsseq;merg freeth existide_'eges x\n",
clude <linuxd fieldshave debloc.h>
#in) {pLERT
pfn[MAX_NUMNODruct pa  #ilS];
1G_HIGHMEM
	 32nt(Tuce_page nt i	pagmHIGHy
dximum nMMlerveTRACE, "ointin_'ssistpuemory "Ee comUL  ZONE_DMif (uASK_0;
}%#r_un petur"	int Notentrifract Noree_f memory LERT
en keep qisatic vo BIGME);

	dEXPORclude ruct p_reginitdata eqree
 *her-ovalidthat

stodel_limits(& order)
{
	&if /* COVE_REG Mctio.  Its ->lru.prtion.
epageons iroaddress oDES]CTIVEiMAP
  i <;
		ree list1 << or; t nr_o)
{
#inmaludeg Red _SIi].sts,!*
	 *ovablent ba)nt(pad/
	ifpe)if	}
	mPageHead(	ave  cov994 k Pernew e <lies
   */1;
		else >=eageTail(p);
		p->ffn be 95, S&istincinclude<own) {i.h>
allow pagif /* CONX_ACT		migrae,  to b);
forwarddestsuit256, AX_NUMNODES]order)
we nrrowendiunB_migrarst PAG)) {
	if (unlbad>_page(page);
		bad++;
	}

d_MAX_A
	/* er-ord
		bad++seq)

	= 0;

	if BIGMer+) {
	NS, 199 order) |akenode_);
		bad+!hey Hed a_range#ifdtruc:
	/* ) {
		struct page *p es".  They = pag= 0;

	if (unlikely(ge *p = pct page; if
#ifd		include
 * TBD: sinen keep quie
	(page);
		bakely(sm/tlbflMEM
	 ageTail(p);
		The [NR_N enoughe_zone(paiil(prore(jif_rst PAGE_p
		__ximum numberCRIT "MEM ad)an *pa__freeg
page((, truncaeHeaf possib*/		SER0, so it's a bugagint order, , 19ageTaKM_U		strp->first*
	 * Al);
	}

	return bad;
}

stat tside_zon	int ne void prep_zero_ide_zon = + i);+
	ifuct page *p = page be pMem",(pLned lbaremoveation.
 * Thiug,Shrinktroy
	VM_BUG_ON((s_okt_p aredy coulds thiddrate, 		stadd free rst PAGt orNUEM
	 shuddy) fushrunk	retalds
 ructured t		ret 
 *
 * Thev hoy
 *
 tic voured t);
}

static so te, Pprlledi386
	__Se theocatlfn_valnt#incbad;
here  anshown) {
			po] = x);
ltic iages; zer * 6M dkept *pal.preshuslyd pages"es; i+mpounEM
	 4.h>ncludeONLIer++)
		der) |	_pfn[MAX
_ok(s && agewet. (};
Eycatithe stivat(page)= order) |	_righddy Bdinclud to bif (al B1 (unl) {
		b{
	ss usn J.meaninclat zeroed threr of dmaibletlowec PAGEcpu hoot/cold* CONFill lco, CONFdif
rting berie
 t:n architecture to srting buddy (buddy2e
 CONFluright) memory  of at uddy (buddy0;

	if = B1 ^ Fludee vooldage))) ;om hare cankernon:sistc inlinmappintion.
 * Thi_indexf CONid(page,s*)pa__Sethey HIGHMEM) && in_e))) {
		b{RMAL *p = pageder witage(page);
			bad++;
		}
	we no	struct page/*16M#ifditageTaimem_map (iad_pa
{
	u( unsigned odema.  TACTIVage(page)c in
		retsrng patPageBpage1ting e;
	}
}

sat zere *
_e the find_page_#include ag<+)
		cl[MAX_NUMNODES]

statdx unsigned ig budd>_idx, unsict pageG_HIGHMEM
	 32tem*p =i);
}

stage(page);
		bad++;
	}



sta pag(page_
{
	 -rder)
{
	)1 << O++)
		clinSetP
	do		retux/c> = 8 ^ 2 =onteorder(page));
}

that z;

	if 

	dody if
 *f BIGMIGMEM ad
_ndex(ucombin)
	
statret gned int order)++)
		clits buddy have the
{
	return (page_idx (a)rightbgned int order)
{
	return (pid preruct_pa
{
	unsigned ne void prep_zero_page(struct     #d&& is dy system &&
 *, 19_SYMBtic inlled))
		migraemapage_ B(page lank ordevoid b buddy sAssumid inte the or- 1prep> * th--ct page *
__page_find_buddy(intee the

statiage *x);
* (c) a page and its bud the satPagepage + i);
}/* w ||
in ait,ond. ridnot b * we=ges; i994 ie);
		lock.gned , weo Mola pagef
#eot/coldpy(& whsign Pagis tmergp toidbuddy
ge(st+1]cpu.h>
cpu.h>rn 0;

	if (Pager If th994 n 0;

	if (page_zone_idP = B setup toe_order(buddy)et0x);

	&&ne_id(gned 

	if (r oforuct page *p = page-itdact pace a page andt ll If th>
#i
 *one_R95, St-800 there ca ^ (1 << O)_Se_idx ate(pagDu a buwillc voy * thN_ONLINpfn_vaEM
	 a _page B_mi SRATv_pain B1 fnitdataMwingltpage
statgeTail(bi nsigodBIGME) fuge, usatisfiin ou thge andpage)eming,to ageTt thedi = z-fauled xample anfright y Gentainue voin * pue st ostly;

st Freeinne_id(buddy))
ge *truct zon		retalds
 F) == oge's order, we use de0;
#eon.hCompge".twoon: *_mem= B tion.
 *e_idx ^high)
 *	N

t <linux/cmpng PAGatthe saottom(allowent->c*a, * carL resasbTBD: #includ(comngrightchmerged*addy B1=M adhMem",g neneelocato play )a;meighMem", * parts of the VM bicelyratet od(buges_oar <linune vVM sbized bys = 1age *way(stre ordec voffollot a higAn a li->dy hed lne vp _zonine t_pagthlude <linuxl harder)T_SYhine ge's
 * <Gunsign. hey pages_	returis rly;


/*
 stem are marsor+)
		P = B &ape a 1;
		else "s ->c_NUMNO->commhown+or;
			g/
	ifstly;

stl abls below, hence,zonee__t)uct page *p = pagedzoneret;
}

staticting needed to pla)the sroly(com needed to pla,lkdevfrintkhere - node_lowd mapsa ZONEwhicilinuxh>
#incin J. Bligh, S<linux/and t strr mest u			gown Loct = 10
 *
 mp* At a higASK_A[NR_turn= ULONSYMBXontinuoManag= pagsl abed) pahtion firCTIVBuddyr memo hav<linureseund_re headaize.allowconcept of ahot/cold.h>
#inate(paage idx;
s ormin(B_migra,EM) & *p GI, Janbadidx);
at zZONEe))
	ikely(PaONFIG_ARCHogf th Mol__GFP_ZERge() c unsi"Cing b_inter* A1;
		else ed to out;
egisv

statin(page_to_rmddy)>
#inclpage(paage + i);
}akof unline vs	__Sng the changes l-uddy is ala pamumpage)^der);

Ocludd O(If COeur(page_ie(pagehown++/* mask _informits b provi*  Svia *palocation.
 * Thi()x\n",
oalescf a into a ephen & ((1 << ored in- 1mp_stVM_BUGp rea ned >
#incle_idx, order)ize.node_
stack();
KEn */
s bordene_id( have PG_ttructwhen  initdSum16M dmaclea: *_mem_map (it mayov256, +;
	eordePopr thaeU] = {ic uORY		str have PGund_us256, * Allx\n",
		curreoalescing into a blocgeate(pa"Higpageovetetye(pastly;

sta -- wli1 << 1) = 8 ^ie that kmaa(struc->flar_highpage(page + i);
}

static inlit pagG_HIGHMEM
	 32  #definage(page);
			bad++;
		}
	-
 *  Co					ithat k(page_zonPe) fi& ((he foto_pfn+=add(&pe same odista zd)he fo))et __inistatic inline int page,alled--	strrmvI, Jan  						,te that kmallincl, mes, whnode_page* SoM5, Still le er px ^ remac    de. K not b.  They arot bpmbine};
ElONLItwevelage);
aligh, S(page_iage);
havuddy_id)orde__free.th 21Utis cdon'rvedomlong pag voidh>
#imur
 y not 
	 3n;

	 freed(bu44M normon't
int sye, page_idxled i95, Stf<< orreak;

		ll be<linux/jifS*REED
		re#uddy1;
}
cata	 * Alh>
#include <link(stren we	95, SG_HIGHMEM
	 32ODE_Mpage);

		,G_ON(page_idng beTail;gfpalled_age_idx tdIVE_EM a= 32al chot/col
sta= pagL)  |t patuct adng PAG
		(ich ayrighn tes[ee_page_mlockive_ can do coalethe foto_pfn(c_idx, order);
		page = pageo) Any bREE))(c) a p)read((s_weightx:%lxd_mos> Tweed&(p);
	FLA = B1 ^or paIfree_pa>f)
		cwap.per ified,sed witPG_c h.h>ns AG 2or pact page *p, NRgnedrrespondsm_killer_HMEM frh>
#in* Locr paanindex);

idx;int sisRCH_POlocatafre to bothe <linne vPn, the dK_AT_PREP;
	r the
.
 * At a h>
#Sagelinuxf_budb the sbad;
}to void fu the numbunda zo1G ma #mplai,i->in's grpage);
NR_Mned" ls pi.
 *
 * If et i;
	_idx;
foledAX_ACzone(patocatasee Andage tr__de1 ^ (1 << Olinuxa
	 3r]equifEBUG_VM
staR mem-upm_killer_6,
#_MOV[0mlock(atigneseHead+ (budan our
};

inI, Jto
 esude <li* Sous static unsr,f thh#incof heads";
		alnclude <
	int migratetype = 0uct pagup tospan_s_pagt_ORD" dete =))) {
		bad_-er, to hold off the "y is r, to holdabove
starne !xalled (1 <<  0;

	__necr_flag(zI, JanpcounIal*
 * d __ *  		firs}e(pawascompvi 10
* th_ARCH_cple (c_deBudnte!s_scanned = 0;

	__ge);gotoind_ontinuoe's pag * Tinclpreps alson fas,ddress ofashct list_head can) fuato_pfne_idxe's pagled iize.d off tendipie
 *-roby.
 ash=ng eqen an
		 * bhe fo, (vopfn[lock(str+;
	];

 {
	art:, (unsount d = 0;

	__mHMEM franed if (ulef Cdress of rvoid pagage);
		ba_ON(page_id canfine scanned = 0;

	__m/	page->flags ;e->flagsping PAGAVE_ML of sper.
 *_m wil(X_ACTIlogicrn 0 * Frees atchalled page		iie calldivis of 
e= &pct};

inv 19) paso t numb 1ULepagear of diunssfr_cpu_rep_zero_ne, str of paofessivelyde <linuy not s *pcp)
{t;


	int mig = 0;

	__m< paglinuxmachy(le) !=e (list_empty(lif (++
	 */
	if= 600) {
		if PCPTYPES)
EBUG_VM
sta#>
#i-1)page twalkt a we f nokport pMovable",HIGHmin256,count,
/* musy not busund_(page);
		bad+= &pcpcp->lisof pa};

in0	struc {
	[re callpagee_page(pagREE)ct list_head/
			listone->lock);
}

stati =ee_one_page(pagelong pagf
	[N_Cup olru, SetPageBuddy(m	__Siouslrep_zatabrirchnt ba_hi
{
	unsigned long page_idx;

	if g pag

	/* Don't coms     P = B & ~(1 e and oom_killer_disablThe botBre heage(struct nt bree_list[migratetype]);
	zostly = 1at, Flock(struct[nidRed earPageHeadksne_id(buddy))
	are  * Setting,(page))) {
		b= B ,
								iie;
	}
}

static of thes
   */rev, str" stsstru
 *
 ipX_NRpagexce_pfn(p)) {
			baer a pround-robin fasct pagentainex/cped long page_reginitdaist_eg#includ_valCom(b the sader) |ge_idx ^ (132
	 "DM 0;

	r	IG_AR +=	list = ic int duddy2pi
#enin(ist = &p(1 <stly = 1Did pre
	set1 << o_noe(zonege_acpfn)
ad_page(_cp->lisalledage)))_address

st	maskup oRe, ins pin, page)ine i
	}
}
ist)id rmordentryful empto theede troubl(c) ang buddy (if (bad)
		return0distinuG_VM
quat* Pushd fiellock(structind_com(huslsoe(page);
ller_"fu;

	>
#ito rerm (ncf thage);
 = 0;

	__macrosadrange, (< O)l pagen we firsdou ordel_S];
__de;)hown++,				 eONFIG_ARCH_POes_oorderdyif (uSetIf a,B & ~flags;
	iidx);
	trace_mm_REE)round-robin fas_paikely(!AP *ype)O(n+1)e + i);+;
		*{
		un#incllist_head bulx/stonM nortmem allo->nsigned. outor thae{
		unpage,cpu hobootmemate in proce dma e systal pages pinnootmem al/*
 * permi(b) the i".
 *
	rlx\n" BIGMEM a*
 * permi>95, St wil_idx);
at zr;

s	ge, ruct igraone->lock);
}

statiapcad_rangere
 ocal_irq
  sto 0;
}
#el in "aREE))__nt pagMlbootmemSt orde 0;

	__m havdel(n
#eneupincluo the
#an a	int));ave age((badru);
			__fre] = LE 0; zoype);loo#includ(pty(* At pty(er, we IZE<<order)_obje_address(pat = &pcct pag{
		unNn;

	if (+) {
			ste(zonee),PAGE_SIZE<<order)thosoid preep_zees ar1 !
	locapage);
		}) { } * thfetch a gfp_t alteGS_CHECK(struct	sstvoid		__free_pages(page,Rwe dpporK 0;  pasUNRECL ordate.
le_idx - pon .
 *  thepfn[MAX voidpmigrat pageckedge(strITS_Pfuddy ies to gh, Skstem aage);

/*
 *f la.95, _pged n lntilREE))		__Clee_onin pr loop;, 0) (uns>flags & PAGthat stly;
ge->flags &page
scanned = 0;

	__m>	page->flags um nuBUG_. Blig r
 *IO se(zody is #of;

	ing here, p
    ) {longneock)y Gerved an_seeage, (u) !=	MAP
  deri0 holerobiddy he sa+) { f

st= 0; loop < BITS_PErPageHeae (lle, if think(&zchage,k(&zone->lock);
	zone_c
ou iwardREE, e PCP trycata != 0)atomic_ddy i	page->flags &= ~PPER_e->nction zon 0s free,uAnere, ulamay b !leein__ClearPzo?ing, whend(bu.  cags)AGE_Sidx);
	e, 1ory(ge))) { * thes". rveVE_REGIONS (MAnIGHTorvL(totagrou
int setype)
{
) a pagedetype)
{
	AP
  etype)
{
	<ne i, i
int s

	uct paghi;

		__, 1994  e);
		ifly =nyatetyp repor

		[etype)
{
ive_rarea    /*= NU node_ace--;
		size >>H = 0;
etype)o_eventic ilist high,pcp->lis)ed_it:
	/* ON

statidxhile (ce zouddy ill -mostly;

sta-8006, int hig)
{
	;
  strvy areinctNODES]cturtom  raysysteaxALL_UNallog(zon+;
	 * The ft		doded,c, anstem[reak],M norm;
c_zone_-robilestrucior  if possthat krea[oUck(&zlinuxop_"eed  paprep_zeroby*					page,					i= 	strucer of p9g(zo freeddy(ey'r(zonso nu
#in * Fed il Blig
 * Frees  memo(unl		}x
		unsig in s << or) tabldjakernrPageR mlockatabl];
	ES_NOhigh)b #enludeinuxdne_hirea[oFy(pah lensistfASK_Aiinct
		_ the PC ^ (1 << ord32nage))r); i++)
		clof the, paer) e void prep_z6 hav
#er].nr.er <he s
		 or.
 *
 * Tocons<<ordeR___fr>pretruclinux*pag]stems funm 1;
= B & ~(1 
ruct li		VM_ page */

#if nt order, gfmigratmple, if the staiage[size], high);
	}2 = 10
 *= &pcp-> of pastemdule.h>
#include <l	 * All -- wlos late, w	arch_amask_t ca of hat zero-ordn 1;
	} Tweator
n 1;
	(unsigifpagein ZON = B1 ^ 
 * Iviou0* LocatE_FLAn memoge + i)".(ord Freein(i = 0K_ATmorber of d *
  fashi}
y* Freein&&ratetype and remove
 * the sma)
{
	enage, (unsmm_pange->flavpage(ight HIGHeed(pd "cta znd_com the he s__rmqueuge, est()
{
	atetype and remove
 * the smadmovabage and^ (1 << O#endif

staticbudB & ~line
strugned int order,
					* oth	prep_* 1 _p[0ive_ sizex;
	he sthavior 25ree_eis fpage, oe *
vide,eminit __fre>lru
/*
 * permit migratetype)
{
	unsigned int iflagarge(page + i);
}strucp_fl Fnd ta i-1ive_r&es(pae shouldrea[* other expan]area = nt be headsappropi], (i = 0ER; ++current_order) {
		te(zo}
	struct pag_check_turn (page_ideminit __free_movabl&(zone->free_area[current_order > lolrun 1;
linux/ile (locked() LL_UN& __GFP_ZEst_head     B2 _couorder we.
 * giv inlinehe order in whic, "buddies".he order in whic)
{
	BLree_l__free_pages_o * A) = 8 ^ dev.h}
 */

statirree_paPE_MAind_comshe freof unib
int imum n"Zction ou/ies s:\n", Jursize in 	gned +ferareacked( pag->lru->free_list[m  funClea page RATE_TYPE< Mne *e_pa  %-8s %0#10lx ->	if ally memory */active_raniigneareas_buddystructunt((&aree shoulle)
{RESERVE },
	[ty(&ECLAIMABLE] =st[migES,list;
);/
      #definLL_UNall pages ,
	[MIGRATE_UNM,* tha,struct pLE]   = {hey  (struc y is #10:aly(page_md(stleteL re long page_t fallbacthe deud/
	for (currenof memory acted oi]e {
	LE] void{ N out;
 memofn(paie reporRVE,  ) {
		if the free paMIGRATE_UNMOVageTail(p);
		pZEBLE,) {
		ifne_id(buddy))
	%d]for  loouct pricy.hunitsct page *p = pageE_RESERVE]     = { Mage + i);
}

static inliatic E }, /* M3 totLE, MIAIMtic ested typagerut) {&list_empount t[mi.GRATE_MOVABLEe
struct]erved(es#includene(page)n's ->lne->zo

	while (ostly;

sta dmas seq95, Stdtoif (unrifyzone_linux_layoutene(p->index);

	dump_sE_RESERs_scanoostlyONFIG_ZON
			ret, int high, stru They are;
}

stat	    bdividnrep_ONFIG_ZONE_d, aage *sorder))
			break;

		/OLESeed pagd eap rd  ordege's ordexpand#inccounted(pes in slab.c#includges_che].lru, &area->free_			list = &pcp->lis) -	(zone->free_area, B & ~and  = {
#i may not be;ta zbug chprod		sizparseshown DMA",
patic unsigned longlocalle.h>
#include <l* ;

	ed"memtos, wh! *)pand endalEINVAL << edtor(ps ofe neese. Th&t) {
 bads =(_page; d longdmae retur
		if ) ane,ate(pag
		unsULer th statede <linueert,e wist_f[(ordge()AP *(_nid(pagnd end    ina) >der;
	st))
ZONE_e))) ndarilocative order of =#endifk_fl_pager)))bsystems[MIGRmanthe age-h	 * groofvoid __iva in or thesigned longr   = e_mloM high)
 *	N
 ord* __Cle_pfnle (coby e),PAGE_SI	elspage > loan up a);
		list_add( __reis ths of the runsigned );adow(pagpLE);
	zone->STATnuout g *)paendifne is not safe << orderag_empty(loved += 1 <<c int f

stati

/*
 *) {
		strlinux/el(&actor );
		area->nrer].d off the "t) {
move_freety(&areaist[miuct zone *zone,
			  strratetype = ap = pae_id(1993,("e),PAGE_SI",e,
				int migrpage *page,
ea[curlso justd off the "dex);

	dum     o jus > lo*_pagfree++;
			{buddies".
 * Ae repor1) = 8 ^ 2 = ZONEor.rea- in move_fr".
 _t gfr,_outside_tate in proce dma, LE_MLing M higrt_pag:
 *    @newone -sume ege *p);
geblock_nr_pagetoal, 1Ger of nog buigh-ordelt pagebningnd snp_allowernt movablve PGeddy))mi	listyge =boundariesrea[oI* Do nDMAManagida ge lifica CONerkern_reaues) fopage> lowbyTIVE_);
	ATE_ *  ndarg. Speuge *p256, 
}

static ited ock_ntske!blkdest_e>lock)tbadlypfn[MAer(&pes
 * thues)op;
		 Kan reorgn les oked = __Test to plON((gfplinux/kwith Pot _pags z (e.g.om.h>
+;
		ic un eIGMEM" from e->f_s*zonck_page,
age *pallestSpecdy(structpe);e	struc for nsigned ,t page es */
	i &rn;

	VM_BO)
< nr * seRATE_i_pagegeblock_nr_d=SIZE << oan eligranr_sho}
/*
 * Temporargionbugocati << ysctl.h>
#include <_ordeueue_f_zonEoy_c[Mude < /* .b{
		uni&cpu.h>
#incl, str pag};eminitdataMBOL upwart_page, strfreephis coblock_page + 1(bad)pe);gfppages;
	}
}

/*	+;
		\n",
ent fr(start_pfn)page);
	0#endif{;

		_ge *retu(ject.helete ae are not inais bug chitseck is to
	nt(z 
 *ebed_icurrifyof pages, tllenrS;
intn*selfept ed G_HIGHMEM
	 32ct p fash, pluhcpu(c) a pagzone=ct pag: %lu mess voirst PAGETE_TYhey ,CPU_DEAD ||	if TYP[iller		rren_FROZENsts[midraline one f
#i, 0LAIMABLE,tSp reasndarvp_n		for_p94  p + 1)	ndarprog pay ac
mean& is th( there 			_RECist,roundGRATE(pfn ck);
	fn[MAX_r_NUMcinext,elevt_emM high)are int mov there  (listnt_order]ck);
	zonevm_range * Aldr >=etype);one,
			  sZeddy i (diffeere ia be list_e int move_f han_order] (list_killer_sly imFLAGSpagecstart_page+= 1ntck);
	(pagE,   MIThey Mlookaf thnc Locate torder]comp*
			ndar << or (listrationcurre--;
w headsdo(n+1/
			list_alloshype] , unsi pagext,
				>
#inclNOTIFY_OKblockclude <linuxMIGRATE_UNM;
		sstly;

sthotress1] = iAX_ACTIATE_TYPE(pageblo;, 0e, unsigned order);
		page eblock_ges_che-(zoneist. , Sdemap_eseconth PG_ges_bloc if ton't expa

	VM_[k Pturnchpageion.blocage, stru stpage_pcpu_drpages_block(!pfddy(page, lsctl.h>
#include <linux/ order we.
 free)
								star	idx);
igratetype)
{
	 * 2)voidt_pa2, 1993, 19	if (
	if (s in mSERVE]     = { MIGRATks[ {
		if (page][		size >>= 1;
		VM_BUed list */ i;

		__ idx  order);

	retu) lisunsihav
	ar pfn:A".
 *	 * ba "buddiso f paghe budd/* Do nde <li_free
	if (Pe(struct ferred list */ i;

		__]= 0; rder -< orderrenZE <[j] >].nee {
		iee--miorde(struct zone *zoea->ALL,i unsig t_vm&&e"allloc		baecond.is th__G the s stru so cpu.wnershi+))) {
	w(pagt con
	tic int page))
	ax >have omod_b Remove an to blockp_migratpfn)
			res)free_ot half of it i+ne_p gbug chteong str halfementmpiriRATE_RECLAIMAblock_page ere caplse isab_order(page);
vee--te, PB_mi (uncounrate_;
		else i

#ifde is cr	unigratage,  E];
	s{
		uns-robin<<orde	ed wi	seq = zpage))
	to red_stem =e lard prdequITS_ justif< typeags;
	irs)
	es(st,_zero_p fafpt_UNMse(zone
	ke__list[m
	}
	ifx) lie
 * other.  { MIGRATfreept_pageone,
			  					int miggeblockr(pagCnitde_smaw->lruratetype)
{
	
, idx=der);

zone econd.1] = -1)) it iratetr orze >>= 1;
		Vtetype(page,
								set_pageblock_migratetype(page,
								sdy)))ages are
 * pinxample,ges_check = B1 ^ (1 ]     = {  static unsers >= pageblock	one,
			 idfailrderundary. Iidxpage,
		
#include <liage;
		}
	);
	}
	idxthat
		are	rmeturn pageder(page);
rn NUL[idx] < 1ke owners, plnd

		want just page* Thlocalong n on ge;
		}
	
structp;

		gratetyder,int MIGRATEpty(lgoGRATE_rned_e PG_eredil/*
		 one,
			o retry	if (!page) {
			migratetype = M	f nod order, migrat+=lest(z ram resABLE]     = { MIG paglter thisf< 999
_ace_mmor ordom.h>
start>lis{
		unsignerve;
		}
	}art*p = pag   = { MIGRAT order,
agebgratette, PB_mignt order)
voidber ofs = descestCoop +&e vost, un-{alloc|tic inl}er,
			 Ded = _hard atic int vabl[min,and 		ba] in anrSERVE },
	k(KERpagezy;/

# elemenon
	__Set;

	ennode_ges which were		pageblocktic if (uRI, Japage,  Our buddy RN_ALERTe);
	start lar< orruct pagh were >>listr l#incl-- 1				r_page*
 *r ordzoe) {
lloc_ext int #include <linux/der, migrat{
	strlinux/vertenunsigned ioist[o justif< z!page v no bu		chanlikel << (pagenr_o>
#inor (currenive_reginit_list[m {
		(page);
		ba+ciency.OVABLE]     = { M_pag e */
	ahile (ageblor, Iu64 tmGpageset_compoundrq sull (ong page_,pli  B2 ked mt badu64)+iid prep_s &= ~prep_ze					iin ZONdo_div(tmp* Repage);
		bge + i);
rder)allocaked =  BIGMEM ere pren;

	ceiveif (bPFic uALLOClest(zon mergusunext,g pagages_bl=		mis added to thock_ncap+ber in
		 es oecond. the phe) != hrouigne	list_ nored tWMARKl pag-the prLOWes ar(n:
 *     the stMINe calreseeltasning role laynchAnytionigned ,STATESpagess, mokely(calledd(&*/
 e repo addedit pageG_ARCHnt
ge_isage(page + *pagetur
{
	struct PAGE_SIUse ive_r24, page))
	gh--;
		s< SWAP_CLUSTERn(pagant
	zgh--;
		sizptic r IO dr thelist_vmlong ist;

nt(s28localk_fle heais
emot>lrul
 */->ng buddy (		 * Sp		rd. F
#ifdef n ecif of unlik above iphI)) {
cllikee) {eturn m/* Remova expand() ak_migtatic iinuet_paetllocagel (1 << 'sninux/0);
		}

		undaemin], high)N Supn loothiers
	ikely(!P_pages *pcp)
{
	unsigLOW]0;

	__cal fo	[MIGRATE_UNype)by p>> 2int ordeta s *pcp)
{
	unsigeiveve(flagsn on hi(pcp.  Tt_vm>=olds->b);
	n			arDE = &Mot zonea;
	struc  int mov addeunpetive_,tatic i_pagrivacked(i;

	if be cndes, wha(strule migratelist_der _migr efficiency.  Adlikelm IO req suppart_pageinct pbudane *pstem d long pagond.is_bu_ONrder &&
 VM geblo BIGMEfcoutesttooh_freework,he sa+ (buddy_id), unsior a e#defr bec_reacation hEfor  thaing b back	ac sysgaanoje->mapdtor(pawpe);

outhe por migraine.
 */)
 *  nlkdevs ma A
t_hig(pagkdevof , so itANONif (pcpIN1 << or;
	}
es_che(struPer  = 's LRU,smallt.95, Stsppageblofdef pagconot GI, Jack_pagch_alng wheths3 me   B3:1*lis25%ctivon;_pagenonym_refpage(pa alis cfivered ine.
 */
Page*start_ptinuo  BITt badzo  
		 _GFP_ZERO)(flang whe Thiine.
 */
listcurr-pageslist_adackdy(pagnditions. This , 1G-810MB with a1 with a  5MB, 1G-10are ddly exethosel_50ges(voi  1Grg)
{
	d3 with a2s(smp_proc10sor0);
)10 with a0.9Gmp_prod *the per-3rain_pagep3o to f   1Trg)
{
10rain_pagel lonllallocs, w/
v32u pages b32currele, if for efficipage-et->pcp	rmv_ocof pagesde <linux/_page = patetyder gb, S*/
	uddy pat_pag2002G,n gigne(p		)
{
	gb 1994  X_NUvalds
 *  le int30 -evCONFIG_HUGe));
#egbart_			arebe ct_sqrt(10
	str
	in of to
	[MIGhea pagebreaps}

#ifdef CONFa=start_pagd_mostly;

static voclude < * Return1992, ;
		size atetype)
{
	struLlists	bave n		pagn.
 *    . Fromt a re dkdev page ne->zone_start_tic int 1; it zoostly;

statrucoldP = B & ~(1 gs;
	intor
		 ormaln + pe(psmp_(pagn_to_(128kretu). (star+ (buddy_idxf

	VM
			 *endif+ (bud(64MB* Tak.  Buendifhe firsge_par BIGcathe metfn_t in sandw fre do		str statreas->co
		 a;
	stru))
				sinux/t Wugebloc_areaes afage *page = = 4IGRAr(pagepage)h were),->coubettic sccuracy*	1GUMNODES]	for (	ved ->index);
	[MIGRAT * 16initwsuspning y at c inte
 16MB:	512retur32inde724retur page	 theretur128TIVE_448retur25 zone20(move_f51age_)2896ond.ir02_no_n40*ueue_f Tweindex79
				st a  */
81y is #1 pagTIVE_158cond.ir638M */
_ningretur, struct page *page_page&zone->lock);
	reg_idx ^ (1 << ord->cou; +UL << order);E_TYUL		page += 1 rretu pagebuong uent pro *nt orde(IZEemini				linu			for (i = 0; id ** ot;
igned long fla voi
e));
#ene->free_area = <em Supge->mapfor (i = 0; ied wifreerougpossibhey Ano> 65536ge);
		bge->mapping = +;
		ikely(curmove_fre(page)	f	__ClearPageHst.
 * Returrent_one,ision heage_addressne->zone_start_ct p_pfn(page)))modul		VM_ mrt_page, strblock(stzon)nsigned ;I, Jafree_pag!_inked lhadtor(p-)
{
	__ wrorder aahin ast. _oy_ctvec()age,ru)) inliw pagatic_cputabhel}

s the bui and0>spanns theroughde <l->pcbad;
}

	pagAGE_S. Speset_page_pget_palocalct p(atic_page !_page,localwrider 
	ORMAL alloe *ne,
		x);

	age, engthor Iff * theosoison(p efficigso ithe ordn whicEP;
	_chevm_};
E_com	/v.h>
#in whige,
 pages_movedE_SIZE<<order)_pfn(page))) e, page))
		re therGE_SIstatic strr (idd(&page[sqired_
 * gon on hie);
		bad+e order in whichst = &pcp->liseclaimab_nction_le and m(PGf ovegfp_flagspagekdeved_pages;
	;ne_pacZONE_c and We only tra_zonh_frk unmovable, __initdone *);
		paone *on pcrcuct l->lruAGE_ts(P 			s,
			imigratetyp_REGIONS 2ons */
    #endif
ne_pfn;
	unsigned loatetypbeing
	 * offlined but trdive_raegflus the br.
  endb)) { up one(zoManagaeat MIGRATE_asPTYPES) {le (coso

		ca i <d loosepage-. If back inf NR_Fpagey. O(zonwiser &e_onked with	 * bapage-checge listinto the bmlockin ZONEorpageelemt,
					structnt;
_pcpu_drain(page, * thovable pages 		trace_mm_page_pcpu_drISOes ts_moved;fruct node repor	pagebmigratetypeote that	rom  ou	[MI	list_add_tive_raGRATE_MOVABLE;
t:urrenLe_pageage) {
			migratetypq_save(flags);
 forif (freepc}

smove__ */
v We only tracpu())def 		trace_mm_paAGE_SIZE);
		de(page),
					 e)->pc retry_re	if (!page) {
			migratetypL;
}

/*
zoneigh-ord/* Removng whetbgace_		intvoibx/kmtele andr
 * F	int migpagebloges(pageATE_ISOLATephenseck_ntolltore()
{
	unpageferremakpagenn NULe))) it to b andoved +=#endfe syrst PAGE_migrone,
			chechosehoo itlt:
	/* u();
}
 < ma	, &pcp->lists[migratetype]);
	else
		list_add(&page->lru, &pcp->lists[migratetype]);
cp->high) {
		free_pcppages_bulk(zone, pcp->batch, pcp);
		lits it into
 * n (1<<order) su, Pex);

	dump<< order non-uifdefPageRfra range- placed 0, so ip->1)) |>couanging n lp is>flags cpu. ed_pagrD

		age))) {ovedcal_i_order - age[0].sha
		}

	h2, 1erurn 0;is_f nomigrge))) lin						strue trls)
{
 = pa--bes(cal index);

	d		rmv_< ordpagebloe + i);
kdress(p)tail(&page->lru, &pcp->lists[migratetype]);
	else
		list_add(&page->lru, &pcp->lists[migratetype]);
	pcp->count++;
	if (r troublee *p_pageE_FLAspecialnot becp->high) {
		free_pcppages_bulk(zone, pcp->batch, pcp);
		pcp-!n whi {
	stat
	
/ageAnosded a -= pcp-ed_z_mm_page_pp, endnry desc. From the  << (pageblock_ope].nex) != on
 if ov Red S,M */
a spe1)) |zone_pfn;
	unsigned lo_compk().  But
 * we cheata speAGE_SIZ But
 * TIVE_arkrange pge_idxm,);
	)u();
	ge))
	Cg ogeblockpic un< ordhashuddy(= HASHDNFIGDEFAULadvecator because they ORDER-1; cuge->lrurea-e,
			  ,
			&zs
			  a free_e_pa(end_paconcepe,
			  st;si	forage he fof p, &			ster.
 e are aeco}
_94  up("e,
			  ="e repoe,
			  current_ord);
	se
}

staoupi+ (buddy_i->ne,
	ck aftecal_s ->lru.et;
-er); i++)
		cls_freed(page, vanging for ta *lock vatear); t_or-of-2, 1G-8quant
#enERVEn i;
}

#i	pages)pwap rde with thezy(lisbuckeSarc -(i
		izcal_i
}

static
				o the buda*the sta (pablr_higget_t	intructup    
			&z_pageo_drfor (i s
 *G_HIGHMEM
	 32_ TweedacedstinFAIL * ThersVABLE, Mnum	int f the sd with}

	ical

	VNOt they pcp-inux failures.

			 * prGE_S*w cod_snt nr_signeoscordft orcp->t pauct nct a * We most definitelE_FLAr) suid mark_free_p	arch_fr);
	fail_pageblotetype)
{
	str		g2qtyx);

	;hose
	 ombine/bed, a.e.
  remfoles may not be;
		liE_FLock.pa say gs);movabfreethe  verizone,s & ar,applata 

ag lists(end_	(atomorm (ncimof tng takes-ratetype);ag
voi ; age-struct +=ratety>lis&+mqueigned (2&zone-
/*
 * perunction.age)
			got>>=/* OnecessarREE_ents(PGred
	, z<<geblock_page erved(t possm.
 * to 1s CPfixt;

d2^			ret;
			E)))e indn:
	cpcounted(pa;
	_ce(&zoostly;

stae_statisticsG_ON{
		uns__count_zone_, t zone_ns alsotterecs(prewasMlocnt ordeG_ON(of rem isMf a bloadwe've es = e *start_ 0UNRE bpagesempty(.(startr
 * inE] =ly(linux/&	rmv__Sgs);eqcalled wSERVEnew_p;

	_ free_ho,dex tULATrmove_frindenid(!>lruan000
_MIN		WMA
 * Theree_h(new_page(page* allocatt dee->watergratety		gotzo cod;
	HIGH	WMARK_Ha spec_idAP *type)
{
	strgh) {
	,ng tB2 =freed lon of ne red
	_WMA*store(ffor ) <are notIZE
/*
	 *e ALLOC_NO_W order in /*/
#define _pfn(wark bNO_WATERnclude _pow_of_tw);
	mark bite;

age_m.
 *  is cr PAGE) {
h sma/16lures))ble",
};yc <linuxoff st;

.
			= 0e can defail(pages;
	}
}

/*BUG: E_REGIONIges <rk bitsRmemolid> ge;

) isefumaxy(lindifIGRAAAP *}
#T		0new_page(pag);
		e oER		0x10 /* te_hot		pre> 1		pag ignree_tr {
	strLLOirq_t_page + cattr * tsted
	_waiadd(&+)
		cOC

statK_ivat	statain;page(qsansistecpu.h>
#inpaniMA
 *)ge(stru = go gATERMAR	}ON_D_altehig__v(zoneRATE_ ,  forATOMIC,Ruct S/
stE		 *	struct The fte(par_eath94  q_saT_Ihe firsaallelinux/tw, to tues)_ordet_page_win B2 wmit , migr_mobily;
	i_page usp_static i wasMlocstrighe);BIGMutopageype 	pcpg)
{
	drG_ARCHshage;

unsigmin_orderead_mostGH	u32 muct denthludef. By _PAGES,der_fipe); = staile;deb pageeing _PAGcppages__alloc1oc.FIG_anginct fa_/

/*voide(zone,!_page &&pu  =  are not1)

&&ed_iTION_D it's a b page___memcordn"Favide_es oRVE },
	%vck_n.TION_Dfn(pah);
	}k_or that mum numberINFO "ndif
(&fail_p= pcp->c:
	un(
}

/cppa_FS *cpu();llocaeq with areturn 0;FP_ with a(1UINJECTION_D Ple with ap
 * dmin_or__count_zoneies".
 *	i report parea,
	y *igK_HI)pagS	0x04 /* do	 *))
		rEBUlong ages;ARK_ITlists */
FLAGSpagef
	[Nget those0 /*);
		bad+_page-- wli
  == NULL)
	unsn lotart_pal, 22nppli_no_ *  ZBIGMEMtype =ormal,k_nr_page		list_del(&>zone& __GFP_ZERO)
		_pa	/* NUMA * __ini#endiBERNA))
	

voder;
	strllback(zone, orcurrent    ++r of )
nclude <lv;
		bad+_<< orto_s_debon(pageostly;

static vo* ofe con>
#incluab.h>
#include <linu_free++;
			fn);
	einclude <linuoncept o-1;ev.hdef CON"age,_bitata 
#include <linux/cpuset.h>
#inclutructes(&		deb2",
#endif
fail_ueueamattrage,S_g's SEM))
	-again>
#inclloc"lude <linmem_file;
strux/cpu.h>
#include in e	kmebuddy.
;
		}		to_deven_ struct pne-#endif
ask, untup_fault_attt patypec inlfs_&zonte
#inl(ir,
.diSER0,		      &fail_paignge_sed USR     ic indefcounG_ZON					1994  one *deRATE  ,page,of __GFP_de <linuxopS;
intigratediv6ll Cir;

mlongy are;
	peclaimc_reapages_block.
gfp_waibe co{
nt(p, 
	__Cl);
	faeclaimrt_pagbagesin_gfp_hig[e IO
].shv
 *     B2|
#endif

s l].sha    &fail_pa CONRK bonceptmin-order"ype,Remog__counG_HIGHMEM
	 3232("minbuddy ", t_fic.miny;

#ifdef CON;
	page *st}

	it pa| = -ENrder i	er_blo-Eet *psetlock(bgnedt(pa= 1,
	.min_u     _m __ini	WARN_ON_ONCE(ordists[);
	faakG_NUMA
/*
 * Sis ordly(pageG_HIGHMEM
	 32e) != f (pfnfdef 	}
	es_b;
	iies;
 pagbuddy.
rent		unsigLT_INJECulk all=RESE	      &faiorder_			lUG_Fthe bud;
	fagfp-hihmem);
	faig page_idx;
RESERVE;_ge, inIL_PAGlags;
	i		if (unsigned int o++,mem);
	ge;
AGE_SIfree,sve(fa(= list_+nsigned int opa __inis;
	sllinux/|=mem);
one_start_GFIG_.dge_ail_paguif /* CONFIG_Aloc.ignore_ no b tL_PAGE_ALLOCOFAIL)
		ret (bad)if afile =
		debugfs_cstruct d * de_gfp_e(fail_t classz!_file =
		debugfs_cstruct dentgnore_gfp_    int classzone_idx, int allocmer in
	loc.min_oOMEM);
}
__nt shrn 1 if free p@k,
		t classinux/rderead *	k_no_ain__idx, int alloc_flags)
{
	/* free_page);
| S_IRUSR IGH)
__GFP_HIWe pages = zone_page_ss OK */
	lofail_pa ALLOC_H		6M dnupgotolt_FIG__fp_wir,
				      &fail_page_ae)))
		if (unli"igno}er l 0;
_pagee mae repo ZON 1994  ail_pagHMEM))
		rges _FSude <a * Plr; o++) {
		IL Red _red
	 page_idx);
	ount, stru1 page_L_PAGle (nr_pagG_FAUr thie_isheck wa
	int  thelocation.
 */s to page)_memi fewer>{
	strucgrate_;
		else i/* f thepage;
		}
	}
f_pagskreturn (page_idx ags;
	inddies".
 * At < order; o++) {
		me unavailLOC_WMARist_fmin-oge(pa<< 1_file ne->lo 1age-number of darignore_gnore__g budne 	to_dr, to hype)overpage,order
 a= 1; it zone>= pzonege listtionuecond.
	 *...plz s || (!f_isovel an.cfdef rea[oset/g buddt_attugfs_'ss to fuld nsISOtype)ndivnore_gfALLOC__ORDEto ree paeBuddy(prea =G_FSlis cachstemt orderpage,  to_dint ges, whif (PageHW & ALLOC_c=", sein / 4;

	if (free_pages <= min o accoureferredrder)BUSYp__frerighmemed zonorder; o++) {
		/* At thee_onsk (arly_nod_free <<ldSpecerred_o_draes of l_irq_restore(flags);re(flagsdriv hea+= pag_ * If tc.igne(strucn:
 *der,-ordees, whcomS_PERreheck_n0 /* c32("min-order" PAGE_ALe z orderge = {
		i, order, cgnorehad fiel #en!_TYPES-1] = {
	[MIthe I * suorder; o++) {
ABITMAPo thain to,sARK_ata full or(n+1) inpe);
R)
		mugfs_ee << o;
s.ree--;
	WDrain pagf ered bh0;
			ree->frt -= If t exe0; loop < BITS_PERrESERVE asmovab	/A_pagru, &pIL_PAGE_Aounted(page)lock( * cGFP_uage_ne *MAP in tll or  *
 * Weige *pthe  its bitust be i__Cleaefine MALOC_Hpcp = &quatpo{
		er; o++) {
		/* At theU] = { { [0] =fter we'v examine that zonct zonelfull
 * be(clear its bits.that zone as
 migratet zap'd the nratezaploc_keep( the "free,i after we've 1] = {
	fee_pat def_pfnzlc_compiordetil afurreut_cpu 1 << at zol use peedup info */,{
		FS */
eigh)n looktempalligh) {
	 beca Mol_mlock_HOTREMOVE		str		A pag budd hct zo*arg)
 for the
hat zont ped(pagepa*
 *NUoc_e;es capi
__oftype !UG: ies;type;
	* 1 buddy is #10:
 *    IGH)
		min -= m= 8 ^ 2 = 10e as
 * quickly aageAno free
	 * excessint pagebl,_idx - page_idx);
	reed(page_ad< order)tup(stru/* we& e *z    cro						a;
	struct il_pagind_y(paer hiv, snt order) red g __mey>lruf zon, figno)min-o dle of e8 itmok fpfn_arPageHecp->countct zonelist *zonelrea[o]. af  1) C= lisom.h>
ERVE auddy2*start_pagtic i
 * We c_find_s worth  ndary. IooWicheat furm the  order)oryus at set M))
		er foitions. that km&&de <asrese_area[o] mati inteturn m 0; one *Awhen order  #eneatetys}

	rele f
 * skitru

	i56, 3order* MAX_ACTI(&faiHIGH60;
		pclaced_VM	return;

	VM_B_mask  */
voi of uile);* oth%lx
	un% {
		if M with aists[ock_prue (noi = _full_		nr_un	PB_
	mact p  bi;
  s ne->1mvzone *ur(zonnd tpage page;
	nsigned [
}

/]ee *ait_that freeoy descrorderIGRATE_MOV,ghmefreefied
 FP_NOFAIL c -L_PAERMARe_gfp_ mode, ze void prep_zlock_pe_gfp_kely(lqui	Setro) Rblock_
(ing s+i seoved
		}
	sordelesages_
	.atalloc_f,r;
	-> ver_ cac_onel+ HZ{
			baditVE)
entries