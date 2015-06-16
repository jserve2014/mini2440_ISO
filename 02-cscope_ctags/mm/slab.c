/*
 * linux/mm/slab.c
 * Written by Mark Hemment, 1996/97.
 * (markhe@nextd.demon.co.uk)
 *
 * kmem_cache_destroy() + some cleanup - 1999 Andrea Arcangeli
 *
 * Major cleanup, different bufctl logic, per-cpu arrays
 *	(c) 2000 Manfred Spraul
 *
 * Cleanup, make the head arrays unconditional, preparation for NUMA
 * 	(c) 2002 Manfred Spraul
 *
 * An implementation of the Slab Allocator as described in outline in;
 *	UNIX Internals: The New Frontiers by Uresh Vahalia
 *	Pub: Prentice Hall	ISBN 0-13-101908-2
 * or with a little more detail in;
 *	The Slab Allocator: An Object-Caching Kernel Memory Allocator
 *	Jeff Bonwick (Sun Microsystems).
 *	Presented at: USENIX Summer 1994 Technical Conference
 *
 * The memory is organized in caches, one cache for each object type.
 * (e.g. inode_cache, dentry_cache, buffer_head, vm_area_struct)
 * Each cache consists out of many slabs (they are small (usually one
 * page long) and always contiguous), and each slab contains multiple
 * initialized objects.
 *
 * This means, that your constructor is used only for newly allocated
 * slabs and you must pass objects with the same initializations to
 * kmem_cache_free.
 *
 * Each cache can only support one memory type (GFP_DMA, GFP_HIGHMEM,
 * normal). If you need a special memory type, then must create a new
 * cache for that memory type.
 *
 * In order to reduce fragmentation, the slabs are sorted in 3 groups:
 *   full slabs with 0 free objects
 *   partial slabs
 *   empty slabs with no allocated objects
 *
 * If partial slabs exist, then new allocations come from these slabs,
 * otherwise from empty slabs or new slabs are allocated.
 *
 * kmem_cache_destroy() CAN CRASH if you try to allocate from the cache
 * during kmem_cache_destroy(). The caller must prevent concurrent allocs.
 *
 * Each cache has a short per-cpu head array, most allocs
 * and frees go into that array, and if that array overflows, then 1/2
 * of the entries in the array are given back into the global cache.
 * The head array is strictly LIFO and should improve the cache hit rates.
 * On SMP, it additionally reduces the spinlock operations.
 *
 * The c_cpuarray may not be read with enabled local interrupts -
 * it's changed with a smp_call_function().
 *
 * SMP synchronization:
 *  constructors and destructors are called without any locking.
 *  Several members in struct kmem_cache and struct slab never change, they
 *	are accessed without any locking.
 *  The per-cpu arrays are never accessed from the wrong cpu, no locking,
 *  	and local interrupts are disabled so slab code is preempt-safe.
 *  The non-constant members are protected with a per-cache irq spinlock.
 *
 * Many thanks to Mark Hemment, who wrote another per-cpu slab patch
 * in 2000 - many ideas in the current implementation are derived from
 * his patch.
 *
 * Further notes from the original documentation:
 *
 * 11 April '97.  Started multi-threading - markhe
 *	The global cache-chain is protected by the mutex 'cache_chain_mutex'.
 *	The sem is only needed when accessing/extending the cache-chain, which
 *	can never happen inside an interrupt (kmem_cache_create(),
 *	kmem_cache_shrink() and kmem_cache_reap()).
 *
 *	At present, each engine can be growing a cache.  This should be blocked.
 *
 * 15 March 2005. NUMA slab allocator.
 *	Shai Fultheim <shai@scalex86.org>.
 *	Shobhit Dayal <shobhit@calsoftinc.com>
 *	Alok N Kataria <alokk@calsoftinc.com>
 *	Christoph Lameter <christoph@lameter.com>
 *
 *	Modified the slab allocator to be node aware on NUMA systems.
 *	Each node has its own list of partial, free and full slabs.
 *	All object allocations for a node occur from node specific slab lists.
 */

#include	<linux/slab.h>
#include	<linux/mm.h>
#include	<linux/poison.h>
#include	<linux/swap.h>
#include	<linux/cache.h>
#include	<linux/interrupt.h>
#include	<linux/init.h>
#include	<linux/compiler.h>
#include	<linux/cpuset.h>
#include	<linux/proc_fs.h>
#include	<linux/seq_file.h>
#include	<linux/notifier.h>
#include	<linux/kallsyms.h>
#include	<linux/cpu.h>
#include	<linux/sysctl.h>
#include	<linux/module.h>
#include	<linux/kmemtrace.h>
#include	<linux/rcupdate.h>
#include	<linux/string.h>
#include	<linux/uaccess.h>
#include	<linux/nodemask.h>
#include	<linux/kmemleak.h>
#include	<linux/mempolicy.h>
#include	<linux/mutex.h>
#include	<linux/fault-inject.h>
#include	<linux/rtmutex.h>
#include	<linux/reciprocal_div.h>
#include	<linux/debugobjects.h>
#include	<linux/kmemcheck.h>

#include	<asm/cacheflush.h>
#include	<asm/tlbflush.h>
#include	<asm/page.h>

/*
 * DEBUG	- 1 for kmem_cache_create() to honour; SLAB_RED_ZONE & SLAB_POISON.
 *		  0 for faster, smaller code (especially in the critical paths).
 *
 * STATS	- 1 to collect stats for /proc/slabinfo.
 *		  0 for faster, smaller code (especially in the critical paths).
 *
 * FORCED_DEBUG	- 1 enables SLAB_RED_ZONE and SLAB_POISON (if possible)
 */

#ifdef CONFIG_DEBUG_SLAB
#define	DEBUG		1
#define	STATS		1
#define	FORCED_DEBUG	1
#else
#define	DEBUG		0
#define	STATS		0
#define	FORCED_DEBUG	0
#endif

/* Shouldn't this be in a header file somewhere? */
#define	BYTES_PER_WORD		sizeof(void *)
#define	REDZONE_ALIGN		max(BYTES_PER_WORD, __alignof__(unsigned long long))

#ifndef ARCH_KMALLOC_MINALIGN
/*
 * Enforce a minimum alignment for the kmalloc caches.
 * Usually, the kmalloc caches are cache_line_size() aligned, except when
 * DEBUG and FORCED_DEBUG are enabled, then they are BYTES_PER_WORD aligned.
 * Some archs want to perform DMA into kmalloc caches and need a guaranteed
 * alignment larger than the alignment of a 64-bit integer.
 * ARCH_KMALLOC_MINALIGN allows that.
 * Note that increasing this value may disable some debug features.
 */
#define ARCH_KMALLOC_MINALIGN __alignof__(unsigned long long)
#endif

#ifndef ARCH_SLAB_MINALIGN
/*
 * Enforce a minimum alignment for all caches.
 * Intended for archs that get misalignment faults even for BYTES_PER_WORD
 * aligned buffers. Includes ARCH_KMALLOC_MINALIGN.
 * If possible: Do not enable this flag for CONFIG_DEBUG_SLAB, it disables
 * some debug features.
 */
#define ARCH_SLAB_MINALIGN 0
#endif

#ifndef ARCH_KMALLOC_FLAGS
#define ARCH_KMALLOC_FLAGS SLAB_HWCACHE_ALIGN
#endif

/* Legal flag mask for kmem_cache_create(). */
#if DEBUG
# define CREATE_MASK	(SLAB_RED_ZONE | \
			 SLAB_POISON | SLAB_HWCACHE_ALIGN | \
			 SLAB_CACHE_DMA | \
			 SLAB_STORE_USER | \
			 SLAB_RECLAIM_ACCOUNT | SLAB_PANIC | \
			 SLAB_DESTROY_BY_RCU | SLAB_MEM_SPREAD | \
			 SLAB_DEBUG_OBJECTS | SLAB_NOLEAKTRACE | SLAB_NOTRACK)
#else
# define CREATE_MASK	(SLAB_HWCACHE_ALIGN | \
			 SLAB_CACHE_DMA | \
			 SLAB_RECLAIM_ACCOUNT | SLAB_PANIC | \
			 SLAB_DESTROY_BY_RCU | SLAB_MEM_SPREAD | \
			 SLAB_DEBUG_OBJECTS | SLAB_NOLEAKTRACE | SLAB_NOTRACK)
#endif

/*
 * kmem_bufctl_t:
 *
 * Bufctl's are used for linking objs within a slab
 * linked offsets.
 *
 * This implementation relies on "struct page" for locating the cache &
 * slab an object belongs to.
 * This allows the bufctl structure to be small (one int), but limits
 * the number of objects a slab (not a cache) can contain when off-slab
 * bufctls are used. The limit is the size of the largest general cache
 * that does not use off-slab slabs.
 * For 32bit archs with 4 kB pages, is this 56.
 * This is not serious, as it is only for large objects, when it is unwise
 * to have too many per slab.
 * Note: This limit can be raised by introducing a general cache whose size
 * is less than 512 (PAGE_SIZE<<3), but greater than 256.
 */

typedef unsigned int kmem_bufctl_t;
#define BUFCTL_END	(((kmem_bufctl_t)(~0U))-0)
#define BUFCTL_FREE	(((kmem_bufctl_t)(~0U))-1)
#define	BUFCTL_ACTIVE	(((kmem_bufctl_t)(~0U))-2)
#define	SLAB_LIMIT	(((kmem_bufctl_t)(~0U))-3)

/*
 * struct slab
 *
 * Manages the objs in a slab. Placed either at the beginning of mem allocated
 * for a slab, or allocated from an general cache.
 * Slabs are chained into three list: fully used, partial, fully free slabs.
 */
struct slab {
	struct list_head list;
	unsigned long colouroff;
	void *s_mem;		/* including colour offset */
	unsigned int inuse;	/* num of objs active in slab */
	kmem_bufctl_t free;
	unsigned short nodeid;
};

/*
 * struct slab_rcu
 *
 * slab_destroy on a SLAB_DESTROY_BY_RCU cache uses this structure to
 * arrange for kmem_freepages to be called via RCU.  This is useful if
 * we need to approach a kernel structure obliquely, from its address
 * obtained without the usual locking.  We can lock the structure to
 * stabilize it and check it's still at the given address, only if we
 * can be sure that the memory has not been meanwhile reused for some
 * other kind of object (which our subsystem's lock might corrupt).
 *
 * rcu_read_lock before reading the address, then rcu_read_unlock after
 * taking the spinlock within the structure expected at that address.
 *
 * We assume struct slab_rcu can overlay struct slab when destroying.
 */
struct slab_rcu {
	struct rcu_head head;
	struct kmem_cache *cachep;
	void *addr;
};

/*
 * struct array_cache
 *
 * Purpose:
 * - LIFO ordering, to hand out cache-warm objects from _alloc
 * - reduce the number of linked list operations
 * - reduce spinlock operations
 *
 * The limit is stored in the per-cpu structure to reduce the data cache
 * footprint.
 *
 */
struct array_cache {
	unsigned int avail;
	unsigned int limit;
	unsigned int batchcount;
	unsigned int touched;
	spinlock_t lock;
	void *entry[];	/*
			 * Must have this definition in here for the proper
			 * alignment of array_cache. Also simplifies accessing
			 * the entries.
			 */
};

/*
 * bootstrap: The caches do not work without cpuarrays anymore, but the
 * cpuarrays are allocated from the generic caches...
 */
#define BOOT_CPUCACHE_ENTRIES	1
struct arraycache_init {
	struct array_cache cache;
	void *entries[BOOT_CPUCACHE_ENTRIES];
};

/*
 * The slab lists for all objects.
 */
struct kmem_list3 {
	struct list_head slabs_partial;	/* partial list first, better asm code */
	struct list_head slabs_full;
	struct list_head slabs_free;
	unsigned long free_objects;
	unsigned int free_limit;
	unsigned int colour_next;	/* Per-node cache coloring */
	spinlock_t list_lock;
	struct array_cache *shared;	/* shared per node */
	struct array_cache **alien;	/* on other nodes */
	unsigned long next_reap;	/* updated without locking */
	int free_touched;		/* updated without locking */
};

/*
 * Need this for bootstrapping a per node allocator.
 */
#define NUM_INIT_LISTS (3 * MAX_NUMNODES)
struct kmem_list3 __initdata initkmem_list3[NUM_INIT_LISTS];
#define	CACHE_CACHE 0
#define	SIZE_AC MAX_NUMNODES
#define	SIZE_L3 (2 * MAX_NUMNODES)

static int drain_freelist(struct kmem_cache *cache,
			struct kmem_list3 *l3, int tofree);
static void free_block(struct kmem_cache *cachep, void **objpp, int len,
			int node);
static int enable_cpucache(struct kmem_cache *cachep, gfp_t gfp);
static void cache_reap(struct work_struct *unused);

/*
 * This function must be completely optimized away if a constant is passed to
 * it.  Mostly the same as what is in linux/slab.h except it returns an index.
 */
static __always_inline int index_of(const size_t size)
{
	extern void __bad_size(void);

	if (__builtin_constant_p(size)) {
		int i = 0;

#define CACHE(x) \
	if (size <=x) \
		return i; \
	else \
		i++;
#include <linux/kmalloc_sizes.h>
#undef CACHE
		__bad_size();
	} else
		__bad_size();
	return 0;
}

static int slab_early_init = 1;

#define INDEX_AC index_of(sizeof(struct arraycache_init))
#define INDEX_L3 index_of(sizeof(struct kmem_list3))

static void kmem_list3_init(struct kmem_list3 *parent)
{
	INIT_LIST_HEAD(&parent->slabs_full);
	INIT_LIST_HEAD(&parent->slabs_partial);
	INIT_LIST_HEAD(&parent->slabs_free);
	parent->shared = NULL;
	parent->alien = NULL;
	parent->colour_next = 0;
	spin_lock_init(&parent->list_lock);
	parent->free_objects = 0;
	parent->free_touched = 0;
}

#define MAKE_LIST(cachep, listp, slab, nodeid)				\
	do {								\
		INIT_LIST_HEAD(listp);					\
		list_splice(&(cachep->nodelists[nodeid]->slab), listp);	\
	} while (0)

#define	MAKE_ALL_LISTS(cachep, ptr, nodeid)				\
	do {								\
	MAKE_LIST((cachep), (&(ptr)->slabs_full), slabs_full, nodeid);	\
	MAKE_LIST((cachep), (&(ptr)->slabs_partial), slabs_partial, nodeid); \
	MAKE_LIST((cachep), (&(ptr)->slabs_free), slabs_free, nodeid);	\
	} while (0)

#define CFLGS_OFF_SLAB		(0x80000000UL)
#define	OFF_SLAB(x)	((x)->flags & CFLGS_OFF_SLAB)

#define BATCHREFILL_LIMIT	16
/*
 * Optimization question: fewer reaps means less probability for unnessary
 * cpucache drain/refill cycles.
 *
 * OTOH the cpuarrays can contain lots of objects,
 * which could lock up otherwise freeable slabs.
 */
#define REAPTIMEOUT_CPUC	(2*HZ)
#define REAPTIMEOUT_LIST3	(4*HZ)

#if STATS
#define	STATS_INC_ACTIVE(x)	((x)->num_active++)
#define	STATS_DEC_ACTIVE(x)	((x)->num_active--)
#define	STATS_INC_ALLOCED(x)	((x)->num_allocations++)
#define	STATS_INC_GROWN(x)	((x)->grown++)
#define	STATS_ADD_REAPED(x,y)	((x)->reaped += (y))
#define	STATS_SET_HIGH(x)						\
	do {								\
		if ((x)->num_active > (x)->high_mark)			\
			(x)->high_mark = (x)->num_active;		\
	} while (0)
#define	STATS_INC_ERR(x)	((x)->errors++)
#define	STATS_INC_NODEALLOCS(x)	((x)->node_allocs++)
#define	STATS_INC_NODEFREES(x)	((x)->node_frees++)
#define STATS_INC_ACOVERFLOW(x)   ((x)->node_overflow++)
#define	STATS_SET_FREEABLE(x, i)					\
	do {								\
		if ((x)->max_freeable < i)				\
			(x)->max_freeable = i;				\
	} while (0)
#define STATS_INC_ALLOCHIT(x)	atomic_inc(&(x)->allochit)
#define STATS_INC_ALLOCMISS(x)	atomic_inc(&(x)->allocmiss)
#define STATS_INC_FREEHIT(x)	atomic_inc(&(x)->freehit)
#define STATS_INC_FREEMISS(x)	atomic_inc(&(x)->freemiss)
#else
#define	STATS_INC_ACTIVE(x)	do { } while (0)
#define	STATS_DEC_ACTIVE(x)	do { } while (0)
#define	STATS_INC_ALLOCED(x)	do { } while (0)
#define	STATS_INC_GROWN(x)	do { } while (0)
#define	STATS_ADD_REAPED(x,y)	do { } while (0)
#define	STATS_SET_HIGH(x)	do { } while (0)
#define	STATS_INC_ERR(x)	do { } while (0)
#define	STATS_INC_NODEALLOCS(x)	do { } while (0)
#define	STATS_INC_NODEFREES(x)	do { } while (0)
#define STATS_INC_ACOVERFLOW(x)   do { } while (0)
#define	STATS_SET_FREEABLE(x, i) do { } while (0)
#define STATS_INC_ALLOCHIT(x)	do { } while (0)
#define STATS_INC_ALLOCMISS(x)	do { } while (0)
#define STATS_INC_FREEHIT(x)	do { } while (0)
#define STATS_INC_FREEMISS(x)	do { } while (0)
#endif

#if DEBUG

/*
 * memory layout of objects:
 * 0		: objp
 * 0 .. cachep->obj_offset - BYTES_PER_WORD - 1: padding. This ensures that
 * 		the end of an object is aligned with the end of the real
 * 		allocation. Catches writes behind the end of the allocation.
 * cachep->obj_offset - BYTES_PER_WORD .. cachep->obj_offset - 1:
 * 		redzone word.
 * cachep->obj_offset: The real object.
 * cachep->buffer_size - 2* BYTES_PER_WORD: redzone word [BYTES_PER_WORD long]
 * cachep->buffer_size - 1* BYTES_PER_WORD: last caller address
 *					[BYTES_PER_WORD long]
 */
static int obj_offset(struct kmem_cache *cachep)
{
	return cachep->obj_offset;
}

static int obj_size(struct kmem_cache *cachep)
{
	return cachep->obj_size;
}

static unsigned long long *dbg_redzone1(struct kmem_cache *cachep, void *objp)
{
	BUG_ON(!(cachep->flags & SLAB_RED_ZONE));
	return (unsigned long long*) (objp + obj_offset(cachep) -
				      sizeof(unsigned long long));
}

static unsigned long long *dbg_redzone2(struct kmem_cache *cachep, void *objp)
{
	BUG_ON(!(cachep->flags & SLAB_RED_ZONE));
	if (cachep->flags & SLAB_STORE_USER)
		return (unsigned long long *)(objp + cachep->buffer_size -
					      sizeof(unsigned long long) -
					      REDZONE_ALIGN);
	return (unsigned long long *) (objp + cachep->buffer_size -
				       sizeof(unsigned long long));
}

static void **dbg_userword(struct kmem_cache *cachep, void *objp)
{
	BUG_ON(!(cachep->flags & SLAB_STORE_USER));
	return (void **)(objp + cachep->buffer_size - BYTES_PER_WORD);
}

#else

#define obj_offset(x)			0
#define obj_size(cachep)		(cachep->buffer_size)
#define dbg_redzone1(cachep, objp)	({BUG(); (unsigned long long *)NULL;})
#define dbg_redzone2(cachep, objp)	({BUG(); (unsigned long long *)NULL;})
#define dbg_userword(cachep, objp)	({BUG(); (void **)NULL;})

#endif

#ifdef CONFIG_KMEMTRACE
size_t slab_buffer_size(struct kmem_cache *cachep)
{
	return cachep->buffer_size;
}
EXPORT_SYMBOL(slab_buffer_size);
#endif

/*
 * Do not go above this order unless 0 objects fit into the slab.
 */
#define	BREAK_GFP_ORDER_HI	1
#define	BREAK_GFP_ORDER_LO	0
static int slab_break_gfp_order = BREAK_GFP_ORDER_LO;

/*
 * Functions for storing/retrieving the cachep and or slab from the page
 * allocator.  These are used to find the slab an obj belongs to.  With kfree(),
 * these are used to find the cache which an obj belongs to.
 */
static inline void page_set_cache(struct page *page, struct kmem_cache *cache)
{
	page->lru.next = (struct list_head *)cache;
}

static inline struct kmem_cache *page_get_cache(struct page *page)
{
	page = compound_head(page);
	BUG_ON(!PageSlab(page));
	return (struct kmem_cache *)page->lru.next;
}

static inline void page_set_slab(struct page *page, struct slab *slab)
{
	page->lru.prev = (struct list_head *)slab;
}

static inline struct slab *page_get_slab(struct page *page)
{
	BUG_ON(!PageSlab(page));
	return (struct slab *)page->lru.prev;
}

static inline struct kmem_cache *virt_to_cache(const void *obj)
{
	struct page *page = virt_to_head_page(obj);
	return page_get_cache(page);
}

static inline struct slab *virt_to_slab(const void *obj)
{
	struct page *page = virt_to_head_page(obj);
	return page_get_slab(page);
}

static inline void *index_to_obj(struct kmem_cache *cache, struct slab *slab,
				 unsigned int idx)
{
	return slab->s_mem + cache->buffer_size * idx;
}

/*
 * We want to avoid an expensive divide : (offset / cache->buffer_size)
 *   Using the fact that buffer_size is a constant for a particular cache,
 *   we can replace (offset / cache->buffer_size) by
 *   reciprocal_divide(offset, cache->reciprocal_buffer_size)
 */
static inline unsigned int obj_to_index(const struct kmem_cache *cache,
					const struct slab *slab, void *obj)
{
	u32 offset = (obj - slab->s_mem);
	return reciprocal_divide(offset, cache->reciprocal_buffer_size);
}

/*
 * These are the default caches for kmalloc. Custom caches can have other sizes.
 */
struct cache_sizes malloc_sizes[] = {
#define CACHE(x) { .cs_size = (x) },
#include <linux/kmalloc_sizes.h>
	CACHE(ULONG_MAX)
#undef CACHE
};
EXPORT_SYMBOL(malloc_sizes);

/* Must match cache_sizes above. Out of line to keep cache footprint low. */
struct cache_names {
	char *name;
	char *name_dma;
};

static struct cache_names __initdata cache_names[] = {
#define CACHE(x) { .name = "size-" #x, .name_dma = "size-" #x "(DMA)" },
#include <linux/kmalloc_sizes.h>
	{NULL,}
#undef CACHE
};

static struct arraycache_init initarray_cache __initdata =
    { {0, BOOT_CPUCACHE_ENTRIES, 1, 0} };
static struct arraycache_init initarray_generic =
    { {0, BOOT_CPUCACHE_ENTRIES, 1, 0} };

/* internal cache of cache description objs */
static struct kmem_cache cache_cache = {
	.batchcount = 1,
	.limit = BOOT_CPUCACHE_ENTRIES,
	.shared = 1,
	.buffer_size = sizeof(struct kmem_cache),
	.name = "kmem_cache",
};

#define BAD_ALIEN_MAGIC 0x01020304ul

#ifdef CONFIG_LOCKDEP

/*
 * Slab sometimes uses the kmalloc slabs to store the slab headers
 * for other slabs "off slab".
 * The locking for this is tricky in that it nests within the locks
 * of all other slabs in a few places; to deal with this special
 * locking we put on-slab caches into a separate lock-class.
 *
 * We set lock class for alien array caches which are up during init.
 * The lock annotation will be lost if all cpus of a node goes down and
 * then comes back up during hotplug
 */
static struct lock_class_key on_slab_l3_key;
static struct lock_class_key on_slab_alc_key;

static inline void init_lock_keys(void)

{
	int q;
	struct cache_sizes *s = malloc_sizes;

	while (s->cs_size != ULONG_MAX) {
		for_each_node(q) {
			struct array_cache **alc;
			int r;
			struct kmem_list3 *l3 = s->cs_cachep->nodelists[q];
			if (!l3 || OFF_SLAB(s->cs_cachep))
				continue;
			lockdep_set_class(&l3->list_lock, &on_slab_l3_key);
			alc = l3->alien;
			/*
			 * FIXME: This check for BAD_ALIEN_MAGIC
			 * should go away when common slab code is taught to
			 * work even without alien caches.
			 * Currently, non NUMA code returns BAD_ALIEN_MAGIC
			 * for alloc_alien_cache,
			 */
			if (!alc || (unsigned long)alc == BAD_ALIEN_MAGIC)
				continue;
			for_each_node(r) {
				if (alc[r])
					lockdep_set_class(&alc[r]->lock,
					     &on_slab_alc_key);
			}
		}
		s++;
	}
}
#else
static inline void init_lock_keys(void)
{
}
#endif

/*
 * Guard access to the cache-chain.
 */
static DEFINE_MUTEX(cache_chain_mutex);
static struct list_head cache_chain;

/*
 * chicken and egg problem: delay the per-cpu array allocation
 * until the general caches are up.
 */
static enum {
	NONE,
	PARTIAL_AC,
	PARTIAL_L3,
	EARLY,
	FULL
} g_cpucache_up;

/*
 * used by boot code to determine if it can use slab based allocator
 */
int slab_is_available(void)
{
	return g_cpucache_up >= EARLY;
}

static DEFINE_PER_CPU(struct delayed_work, reap_work);

static inline struct array_cache *cpu_cache_get(struct kmem_cache *cachep)
{
	return cachep->array[smp_processor_id()];
}

static inline struct kmem_cache *__find_general_cachep(size_t size,
							gfp_t gfpflags)
{
	struct cache_sizes *csizep = malloc_sizes;

#if DEBUG
	/* This happens if someone tries to call
	 * kmem_cache_create(), or __kmalloc(), before
	 * the generic caches are initialized.
	 */
	BUG_ON(malloc_sizes[INDEX_AC].cs_cachep == NULL);
#endif
	if (!size)
		return ZERO_SIZE_PTR;

	while (size > csizep->cs_size)
		csizep++;

	/*
	 * Really subtle: The last entry with cs->cs_size==ULONG_MAX
	 * has cs_{dma,}cachep==NULL. Thus no special case
	 * for large kmalloc calls required.
	 */
#ifdef CONFIG_ZONE_DMA
	if (unlikely(gfpflags & GFP_DMA))
		return csizep->cs_dmacachep;
#endif
	return csizep->cs_cachep;
}

static struct kmem_cache *kmem_find_general_cachep(size_t size, gfp_t gfpflags)
{
	return __find_general_cachep(size, gfpflags);
}

static size_t slab_mgmt_size(size_t nr_objs, size_t align)
{
	return ALIGN(sizeof(struct slab)+nr_objs*sizeof(kmem_bufctl_t), align);
}

/*
 * Calculate the number of objects and left-over bytes for a given buffer size.
 */
static void cache_estimate(unsigned long gfporder, size_t buffer_size,
			   size_t align, int flags, size_t *left_over,
			   unsigned int *num)
{
	int nr_objs;
	size_t mgmt_size;
	size_t slab_size = PAGE_SIZE << gfporder;

	/*
	 * The slab management structure can be either off the slab or
	 * on it. For the latter case, the memory allocated for a
	 * slab is used for:
	 *
	 * - The struct slab
	 * - One kmem_bufctl_t for each object
	 * - Padding to respect alignment of @align
	 * - @buffer_size bytes for each object
	 *
	 * If the slab management structure is off the slab, then the
	 * alignment will already be calculated into the size. Because
	 * the slabs are all pages aligned, the objects will be at the
	 * correct alignment when allocated.
	 */
	if (flags & CFLGS_OFF_SLAB) {
		mgmt_size = 0;
		nr_objs = slab_size / buffer_size;

		if (nr_objs > SLAB_LIMIT)
			nr_objs = SLAB_LIMIT;
	} else {
		/*
		 * Ignore padding for the initial guess. The padding
		 * is at most @align-1 bytes, and @buffer_size is at
		 * least @align. In the worst case, this result will
		 * be one greater than the number of objects that fit
		 * into the memory allocation when taking the padding
		 * into account.
		 */
		nr_objs = (slab_size - sizeof(struct slab)) /
			  (buffer_size + sizeof(kmem_bufctl_t));

		/*
		 * This calculated number will be either the right
		 * amount, or one greater than what we want.
		 */
		if (slab_mgmt_size(nr_objs, align) + nr_objs*buffer_size
		       > slab_size)
			nr_objs--;

		if (nr_objs > SLAB_LIMIT)
			nr_objs = SLAB_LIMIT;

		mgmt_size = slab_mgmt_size(nr_objs, align);
	}
	*num = nr_objs;
	*left_over = slab_size - nr_objs*buffer_size - mgmt_size;
}

#define slab_error(cachep, msg) __slab_error(__func__, cachep, msg)

static void __slab_error(const char *function, struct kmem_cache *cachep,
			char *msg)
{
	printk(KERN_ERR "slab error in %s(): cache `%s': %s\n",
	       function, cachep->name, msg);
	dump_stack();
}

/*
 * By default on NUMA we use alien caches to stage the freeing of
 * objects allocated from other nodes. This causes massive memory
 * inefficiencies when using fake NUMA setup to split memory into a
 * large number of small nodes, so it can be disabled on the command
 * line
  */

static int use_alien_caches __read_mostly = 1;
static int __init noaliencache_setup(char *s)
{
	use_alien_caches = 0;
	return 1;
}
__setup("noaliencache", noaliencache_setup);

#ifdef CONFIG_NUMA
/*
 * Special reaping functions for NUMA systems called from cache_reap().
 * These take care of doing round robin flushing of alien caches (containing
 * objects freed on different nodes from which they were allocated) and the
 * flushing of remote pcps by calling drain_node_pages.
 */
static DEFINE_PER_CPU(unsigned long, reap_node);

static void init_reap_node(int cpu)
{
	int node;

	node = next_node(cpu_to_node(cpu), node_online_map);
	if (node == MAX_NUMNODES)
		node = first_node(node_online_map);

	per_cpu(reap_node, cpu) = node;
}

static void next_reap_node(void)
{
	int node = __get_cpu_var(reap_node);

	node = next_node(node, node_online_map);
	if (unlikely(node >= MAX_NUMNODES))
		node = first_node(node_online_map);
	__get_cpu_var(reap_node) = node;
}

#else
#define init_reap_node(cpu) do { } while (0)
#define next_reap_node(void) do { } while (0)
#endif

/*
 * Initiate the reap timer running on the target CPU.  We run at around 1 to 2Hz
 * via the workqueue/eventd.
 * Add the CPU number into the expiration time to minimize the possibility of
 * the CPUs getting into lockstep and contending for the global cache chain
 * lock.
 */
static void __cpuinit start_cpu_timer(int cpu)
{
	struct delayed_work *reap_work = &per_cpu(reap_work, cpu);

	/*
	 * When this gets called from do_initcalls via cpucache_init(),
	 * init_workqueues() has already run, so keventd will be setup
	 * at that time.
	 */
	if (keventd_up() && reap_work->work.func == NULL) {
		init_reap_node(cpu);
		INIT_DELAYED_WORK(reap_work, cache_reap);
		schedule_delayed_work_on(cpu, reap_work,
					__round_jiffies_relative(HZ, cpu));
	}
}

static struct array_cache *alloc_arraycache(int node, int entries,
					    int batchcount, gfp_t gfp)
{
	int memsize = sizeof(void *) * entries + sizeof(struct array_cache);
	struct array_cache *nc = NULL;

	nc = kmalloc_node(memsize, gfp, node);
	/*
	 * The array_cache structures contain pointers to free object.
	 * However, when such objects are allocated or transfered to another
	 * cache the pointers are not cleared and they could be counted as
	 * valid references during a kmemleak scan. Therefore, kmemleak must
	 * not scan such objects.
	 */
	kmemleak_no_scan(nc);
	if (nc) {
		nc->avail = 0;
		nc->limit = entries;
		nc->batchcount = batchcount;
		nc->touched = 0;
		spin_lock_init(&nc->lock);
	}
	return nc;
}

/*
 * Transfer objects in one arraycache to another.
 * Locking must be handled by the caller.
 *
 * Return the number of entries transferred.
 */
static int transfer_objects(struct array_cache *to,
		struct array_cache *from, unsigned int max)
{
	/* Figure out how many entries to transfer */
	int nr = min(min(from->avail, max), to->limit - to->avail);

	if (!nr)
		return 0;

	memcpy(to->entry + to->avail, from->entry + from->avail -nr,
			sizeof(void *) *nr);

	from->avail -= nr;
	to->avail += nr;
	to->touched = 1;
	return nr;
}

#ifndef CONFIG_NUMA

#define drain_alien_cache(cachep, alien) do { } while (0)
#define reap_alien(cachep, l3) do { } while (0)

static inline struct array_cache **alloc_alien_cache(int node, int limit, gfp_t gfp)
{
	return (struct array_cache **)BAD_ALIEN_MAGIC;
}

static inline void free_alien_cache(struct array_cache **ac_ptr)
{
}

static inline int cache_free_alien(struct kmem_cache *cachep, void *objp)
{
	return 0;
}

static inline void *alternate_node_alloc(struct kmem_cache *cachep,
		gfp_t flags)
{
	return NULL;
}

static inline void *____cache_alloc_node(struct kmem_cache *cachep,
		 gfp_t flags, int nodeid)
{
	return NULL;
}

#else	/* CONFIG_NUMA */

static void *____cache_alloc_node(struct kmem_cache *, gfp_t, int);
static void *alternate_node_alloc(struct kmem_cache *, gfp_t);

static struct array_cache **alloc_alien_cache(int node, int limit, gfp_t gfp)
{
	struct array_cache **ac_ptr;
	int memsize = sizeof(void *) * nr_node_ids;
	int i;

	if (limit > 1)
		limit = 12;
	ac_ptr = kmalloc_node(memsize, gfp, node);
	if (ac_ptr) {
		for_each_node(i) {
			if (i == node || !node_online(i)) {
				ac_ptr[i] = NULL;
				continue;
			}
			ac_ptr[i] = alloc_arraycache(node, limit, 0xbaadf00d, gfp);
			if (!ac_ptr[i]) {
				for (i--; i >= 0; i--)
					kfree(ac_ptr[i]);
				kfree(ac_ptr);
				return NULL;
			}
		}
	}
	return ac_ptr;
}

static void free_alien_cache(struct array_cache **ac_ptr)
{
	int i;

	if (!ac_ptr)
		return;
	for_each_node(i)
	    kfree(ac_ptr[i]);
	kfree(ac_ptr);
}

static void __drain_alien_cache(struct kmem_cache *cachep,
				struct array_cache *ac, int node)
{
	struct kmem_list3 *rl3 = cachep->nodelists[node];

	if (ac->avail) {
		spin_lock(&rl3->list_lock);
		/*
		 * Stuff objects into the remote nodes shared array first.
		 * That way we could avoid the overhead of putting the objects
		 * into the free lists and getting them back later.
		 */
		if (rl3->shared)
			transfer_objects(rl3->shared, ac, ac->limit);

		free_block(cachep, ac->entry, ac->avail, node);
		ac->avail = 0;
		spin_unlock(&rl3->list_lock);
	}
}

/*
 * Called from cache_reap() to regularly drain alien caches round robin.
 */
static void reap_alien(struct kmem_cache *cachep, struct kmem_list3 *l3)
{
	int node = __get_cpu_var(reap_node);

	if (l3->alien) {
		struct array_cache *ac = l3->alien[node];

		if (ac && ac->avail && spin_trylock_irq(&ac->lock)) {
			__drain_alien_cache(cachep, ac, node);
			spin_unlock_irq(&ac->lock);
		}
	}
}

static void drain_alien_cache(struct kmem_cache *cachep,
				struct array_cache **alien)
{
	int i = 0;
	struct array_cache *ac;
	unsigned long flags;

	for_each_online_node(i) {
		ac = alien[i];
		if (ac) {
			spin_lock_irqsave(&ac->lock, flags);
			__drain_alien_cache(cachep, ac, i);
			spin_unlock_irqrestore(&ac->lock, flags);
		}
	}
}

static inline int cache_free_alien(struct kmem_cache *cachep, void *objp)
{
	struct slab *slabp = virt_to_slab(objp);
	int nodeid = slabp->nodeid;
	struct kmem_list3 *l3;
	struct array_cache *alien = NULL;
	int node;

	node = numa_node_id();

	/*
	 * Make sure we are not freeing a object from another node to the array
	 * cache on this cpu.
	 */
	if (likely(slabp->nodeid == node))
		return 0;

	l3 = cachep->nodelists[node];
	STATS_INC_NODEFREES(cachep);
	if (l3->alien && l3->alien[nodeid]) {
		alien = l3->alien[nodeid];
		spin_lock(&alien->lock);
		if (unlikely(alien->avail == alien->limit)) {
			STATS_INC_ACOVERFLOW(cachep);
			__drain_alien_cache(cachep, alien, nodeid);
		}
		alien->entry[alien->avail++] = objp;
		spin_unlock(&alien->lock);
	} else {
		spin_lock(&(cachep->nodelists[nodeid])->list_lock);
		free_block(cachep, &objp, 1, nodeid);
		spin_unlock(&(cachep->nodelists[nodeid])->list_lock);
	}
	return 1;
}
#endif

static void __cpuinit cpuup_canceled(long cpu)
{
	struct kmem_cache *cachep;
	struct kmem_list3 *l3 = NULL;
	int node = cpu_to_node(cpu);
	const struct cpumask *mask = cpumask_of_node(node);

	list_for_each_entry(cachep, &cache_chain, next) {
		struct array_cache *nc;
		struct array_cache *shared;
		struct array_cache **alien;

		/* cpu is dead; no one can alloc from it. */
		nc = cachep->array[cpu];
		cachep->array[cpu] = NULL;
		l3 = cachep->nodelists[node];

		if (!l3)
			goto free_array_cache;

		spin_lock_irq(&l3->list_lock);

		/* Free limit for this kmem_list3 */
		l3->free_limit -= cachep->batchcount;
		if (nc)
			free_block(cachep, nc->entry, nc->avail, node);

		if (!cpus_empty(*mask)) {
			spin_unlock_irq(&l3->list_lock);
			goto free_array_cache;
		}

		shared = l3->shared;
		if (shared) {
			free_block(cachep, shared->entry,
				   shared->avail, node);
			l3->shared = NULL;
		}

		alien = l3->alien;
		l3->alien = NULL;

		spin_unlock_irq(&l3->list_lock);

		kfree(shared);
		if (alien) {
			drain_alien_cache(cachep, alien);
			free_alien_cache(alien);
		}
free_array_cache:
		kfree(nc);
	}
	/*
	 * In the previous loop, all the objects were freed to
	 * the respective cache's slabs,  now we can go ahead and
	 * shrink each nodelist to its limit.
	 */
	list_for_each_entry(cachep, &cache_chain, next) {
		l3 = cachep->nodelists[node];
		if (!l3)
			continue;
		drain_freelist(cachep, l3, l3->free_objects);
	}
}

static int __cpuinit cpuup_prepare(long cpu)
{
	struct kmem_cache *cachep;
	struct kmem_list3 *l3 = NULL;
	int node = cpu_to_node(cpu);
	const int memsize = sizeof(struct kmem_list3);

	/*
	 * We need to do this right in the beginning since
	 * alloc_arraycache's are going to use this list.
	 * kmalloc_node allows us to add the slab to the right
	 * kmem_list3 and not this cpu's kmem_list3
	 */

	list_for_each_entry(cachep, &cache_chain, next) {
		/*
		 * Set up the size64 kmemlist for cpu before we can
		 * begin anything. Make sure some other cpu on this
		 * node has not already allocated this
		 */
		if (!cachep->nodelists[node]) {
			l3 = kmalloc_node(memsize, GFP_KERNEL, node);
			if (!l3)
				goto bad;
			kmem_list3_init(l3);
			l3->next_reap = jiffies + REAPTIMEOUT_LIST3 +
			    ((unsigned long)cachep) % REAPTIMEOUT_LIST3;

			/*
			 * The l3s don't come and go as CPUs come and
			 * go.  cache_chain_mutex is sufficient
			 * protection here.
			 */
			cachep->nodelists[node] = l3;
		}

		spin_lock_irq(&cachep->nodelists[node]->list_lock);
		cachep->nodelists[node]->free_limit =
			(1 + nr_cpus_node(node)) *
			cachep->batchcount + cachep->num;
		spin_unlock_irq(&cachep->nodelists[node]->list_lock);
	}

	/*
	 * Now we can go ahead with allocating the shared arrays and
	 * array caches
	 */
	list_for_each_entry(cachep, &cache_chain, next) {
		struct array_cache *nc;
		struct array_cache *shared = NULL;
		struct array_cache **alien = NULL;

		nc = alloc_arraycache(node, cachep->limit,
					cachep->batchcount, GFP_KERNEL);
		if (!nc)
			goto bad;
		if (cachep->shared) {
			shared = alloc_arraycache(node,
				cachep->shared * cachep->batchcount,
				0xbaadf00d, GFP_KERNEL);
			if (!shared) {
				kfree(nc);
				goto bad;
			}
		}
		if (use_alien_caches) {
			alien = alloc_alien_cache(node, cachep->limit, GFP_KERNEL);
			if (!alien) {
				kfree(shared);
				kfree(nc);
				goto bad;
			}
		}
		cachep->array[cpu] = nc;
		l3 = cachep->nodelists[node];
		BUG_ON(!l3);

		spin_lock_irq(&l3->list_lock);
		if (!l3->shared) {
			/*
			 * We are serialised from CPU_DEAD or
			 * CPU_UP_CANCELLED by the cpucontrol lock
			 */
			l3->shared = shared;
			shared = NULL;
		}
#ifdef CONFIG_NUMA
		if (!l3->alien) {
			l3->alien = alien;
			alien = NULL;
		}
#endif
		spin_unlock_irq(&l3->list_lock);
		kfree(shared);
		free_alien_cache(alien);
	}
	return 0;
bad:
	cpuup_canceled(cpu);
	return -ENOMEM;
}

static int __cpuinit cpuup_callback(struct notifier_block *nfb,
				    unsigned long action, void *hcpu)
{
	long cpu = (long)hcpu;
	int err = 0;

	switch (action) {
	case CPU_UP_PREPARE:
	case CPU_UP_PREPARE_FROZEN:
		mutex_lock(&cache_chain_mutex);
		err = cpuup_prepare(cpu);
		mutex_unlock(&cache_chain_mutex);
		break;
	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
		start_cpu_timer(cpu);
		break;
#ifdef CONFIG_HOTPLUG_CPU
  	case CPU_DOWN_PREPARE:
  	case CPU_DOWN_PREPARE_FROZEN:
		/*
		 * Shutdown cache reaper. Note that the cache_chain_mutex is
		 * held so that if cache_reap() is invoked it cannot do
		 * anything expensive but will only modify reap_work
		 * and reschedule the timer.
		*/
		cancel_rearming_delayed_work(&per_cpu(reap_work, cpu));
		/* Now the cache_reaper is guaranteed to be not running. */
		per_cpu(reap_work, cpu).work.func = NULL;
  		break;
  	case CPU_DOWN_FAILED:
  	case CPU_DOWN_FAILED_FROZEN:
		start_cpu_timer(cpu);
  		break;
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		/*
		 * Even if all the cpus of a node are down, we don't free the
		 * kmem_list3 of any cache. This to avoid a race between
		 * cpu_down, and a kmalloc allocation from another cpu for
		 * memory from the node of the cpu going down.  The list3
		 * structure is usually allocated from kmem_cache_create() and
		 * gets destroyed at kmem_cache_destroy().
		 */
		/* fall through */
#endif
	case CPU_UP_CANCELED:
	case CPU_UP_CANCELED_FROZEN:
		mutex_lock(&cache_chain_mutex);
		cpuup_canceled(cpu);
		mutex_unlock(&cache_chain_mutex);
		break;
	}
	return err ? NOTIFY_BAD : NOTIFY_OK;
}

static struct notifier_block __cpuinitdata cpucache_notifier = {
	&cpuup_callback, NULL, 0
};

/*
 * swap the static kmem_list3 with kmalloced memory
 */
static void init_list(struct kmem_cache *cachep, struct kmem_list3 *list,
			int nodeid)
{
	struct kmem_list3 *ptr;

	ptr = kmalloc_node(sizeof(struct kmem_list3), GFP_NOWAIT, nodeid);
	BUG_ON(!ptr);

	memcpy(ptr, list, sizeof(struct kmem_list3));
	/*
	 * Do not assume that spinlocks can be initialized via memcpy:
	 */
	spin_lock_init(&ptr->list_lock);

	MAKE_ALL_LISTS(cachep, ptr, nodeid);
	cachep->nodelists[nodeid] = ptr;
}

/*
 * For setting up all the kmem_list3s for cache whose buffer_size is same as
 * size of kmem_list3.
 */
static void __init set_up_list3s(struct kmem_cache *cachep, int index)
{
	int node;

	for_each_online_node(node) {
		cachep->nodelists[node] = &initkmem_list3[index + node];
		cachep->nodelists[node]->next_reap = jiffies +
		    REAPTIMEOUT_LIST3 +
		    ((unsigned long)cachep) % REAPTIMEOUT_LIST3;
	}
}

/*
 * Initialisation.  Called after the page allocator have been initialised and
 * before smp_init().
 */
void __init kmem_cache_init(void)
{
	size_t left_over;
	struct cache_sizes *sizes;
	struct cache_names *names;
	int i;
	int order;
	int node;

	if (num_possible_nodes() == 1)
		use_alien_caches = 0;

	for (i = 0; i < NUM_INIT_LISTS; i++) {
		kmem_list3_init(&initkmem_list3[i]);
		if (i < MAX_NUMNODES)
			cache_cache.nodelists[i] = NULL;
	}
	set_up_list3s(&cache_cache, CACHE_CACHE);

	/*
	 * Fragmentation resistance on low memory - only use bigger
	 * page orders on machines with more than 32MB of memory.
	 */
	if (totalram_pages > (32 << 20) >> PAGE_SHIFT)
		slab_break_gfp_order = BREAK_GFP_ORDER_HI;

	/* Bootstrap is tricky, because several objects are allocated
	 * from caches that do not exist yet:
	 * 1) initialize the cache_cache cache: it contains the struct
	 *    kmem_cache structures of all caches, except cache_cache itself:
	 *    cache_cache is statically allocated.
	 *    Initially an __init data area is used for the head array and the
	 *    kmem_list3 structures, it's replaced with a kmalloc allocated
	 *    array at the end of the bootstrap.
	 * 2) Create the first kmalloc cache.
	 *    The struct kmem_cache for the new cache is allocated normally.
	 *    An __init data area is used for the head array.
	 * 3) Create the remaining kmalloc caches, with minimally sized
	 *    head arrays.
	 * 4) Replace the __init data head arrays for cache_cache and the first
	 *    kmalloc cache with kmalloc allocated arrays.
	 * 5) Replace the __init data for kmem_list3 for cache_cache and
	 *    the other cache's with kmalloc allocated memory.
	 * 6) Resize the head arrays of the kmalloc caches to their final sizes.
	 */

	node = numa_node_id();

	/* 1) create the cache_cache */
	INIT_LIST_HEAD(&cache_chain);
	list_add(&cache_cache.next, &cache_chain);
	cache_cache.colour_off = cache_line_size();
	cache_cache.array[smp_processor_id()] = &initarray_cache.cache;
	cache_cache.nodelists[node] = &initkmem_list3[CACHE_CACHE + node];

	/*
	 * struct kmem_cache size depends on nr_node_ids, which
	 * can be less than MAX_NUMNODES.
	 */
	cache_cache.buffer_size = offsetof(struct kmem_cache, nodelists) +
				 nr_node_ids * sizeof(struct kmem_list3 *);
#if DEBUG
	cache_cache.obj_size = cache_cache.buffer_size;
#endif
	cache_cache.buffer_size = ALIGN(cache_cache.buffer_size,
					cache_line_size());
	cache_cache.reciprocal_buffer_size =
		reciprocal_value(cache_cache.buffer_size);

	for (order = 0; order < MAX_ORDER; order++) {
		cache_estimate(order, cache_cache.buffer_size,
			cache_line_size(), 0, &left_over, &cache_cache.num);
		if (cache_cache.num)
			break;
	}
	BUG_ON(!cache_cache.num);
	cache_cache.gfporder = order;
	cache_cache.colour = left_over / cache_cache.colour_off;
	cache_cache.slab_size = ALIGN(cache_cache.num * sizeof(kmem_bufctl_t) +
				      sizeof(struct slab), cache_line_size());

	/* 2+3) create the kmalloc caches */
	sizes = malloc_sizes;
	names = cache_names;

	/*
	 * Initialize the caches that provide memory for the array cache and the
	 * kmem_list3 structures first.  Without this, further allocations will
	 * bug.
	 */

	sizes[INDEX_AC].cs_cachep = kmem_cache_create(names[INDEX_AC].name,
					sizes[INDEX_AC].cs_size,
					ARCH_KMALLOC_MINALIGN,
					ARCH_KMALLOC_FLAGS|SLAB_PANIC,
					NULL);

	if (INDEX_AC != INDEX_L3) {
		sizes[INDEX_L3].cs_cachep =
			kmem_cache_create(names[INDEX_L3].name,
				sizes[INDEX_L3].cs_size,
				ARCH_KMALLOC_MINALIGN,
				ARCH_KMALLOC_FLAGS|SLAB_PANIC,
				NULL);
	}

	slab_early_init = 0;

	while (sizes->cs_size != ULONG_MAX) {
		/*
		 * For performance, all the general caches are L1 aligned.
		 * This should be particularly beneficial on SMP boxes, as it
		 * eliminates "false sharing".
		 * Note for systems short on memory removing the alignment will
		 * allow tighter packing of the smaller caches.
		 */
		if (!sizes->cs_cachep) {
			sizes->cs_cachep = kmem_cache_create(names->name,
					sizes->cs_size,
					ARCH_KMALLOC_MINALIGN,
					ARCH_KMALLOC_FLAGS|SLAB_PANIC,
					NULL);
		}
#ifdef CONFIG_ZONE_DMA
		sizes->cs_dmacachep = kmem_cache_create(
					names->name_dma,
					sizes->cs_size,
					ARCH_KMALLOC_MINALIGN,
					ARCH_KMALLOC_FLAGS|SLAB_CACHE_DMA|
						SLAB_PANIC,
					NULL);
#endif
		sizes++;
		names++;
	}
	/* 4) Replace the bootstrap head arrays */
	{
		struct array_cache *ptr;

		ptr = kmalloc(sizeof(struct arraycache_init), GFP_NOWAIT);

		BUG_ON(cpu_cache_get(&cache_cache) != &initarray_cache.cache);
		memcpy(ptr, cpu_cache_get(&cache_cache),
		       sizeof(struct arraycache_init));
		/*
		 * Do not assume that spinlocks can be initialized via memcpy:
		 */
		spin_lock_init(&ptr->lock);

		cache_cache.array[smp_processor_id()] = ptr;

		ptr = kmalloc(sizeof(struct arraycache_init), GFP_NOWAIT);

		BUG_ON(cpu_cache_get(malloc_sizes[INDEX_AC].cs_cachep)
		       != &initarray_generic.cache);
		memcpy(ptr, cpu_cache_get(malloc_sizes[INDEX_AC].cs_cachep),
		       sizeof(struct arraycache_init));
		/*
		 * Do not assume that spinlocks can be initialized via memcpy:
		 */
		spin_lock_init(&ptr->lock);

		malloc_sizes[INDEX_AC].cs_cachep->array[smp_processor_id()] =
		    ptr;
	}
	/* 5) Replace the bootstrap kmem_list3's */
	{
		int nid;

		for_each_online_node(nid) {
			init_list(&cache_cache, &initkmem_list3[CACHE_CACHE + nid], nid);

			init_list(malloc_sizes[INDEX_AC].cs_cachep,
				  &initkmem_list3[SIZE_AC + nid], nid);

			if (INDEX_AC != INDEX_L3) {
				init_list(malloc_sizes[INDEX_L3].cs_cachep,
					  &initkmem_list3[SIZE_L3 + nid], nid);
			}
		}
	}

	g_cpucache_up = EARLY;
}

void __init kmem_cache_init_late(void)
{
	struct kmem_cache *cachep;

	/* 6) resize the head arrays to their final sizes */
	mutex_lock(&cache_chain_mutex);
	list_for_each_entry(cachep, &cache_chain, next)
		if (enable_cpucache(cachep, GFP_NOWAIT))
			BUG();
	mutex_unlock(&cache_chain_mutex);

	/* Done! */
	g_cpucache_up = FULL;

	/* Annotate slab for lockdep -- annotate the malloc caches */
	init_lock_keys();

	/*
	 * Register a cpu startup notifier callback that initializes
	 * cpu_cache_get for all new cpus
	 */
	register_cpu_notifier(&cpucache_notifier);

	/*
	 * The reap timers are started later, with a module init call: That part
	 * of the kernel is not yet operational.
	 */
}

static int __init cpucache_init(void)
{
	int cpu;

	/*
	 * Register the timers that return unneeded pages to the page allocator
	 */
	for_each_online_cpu(cpu)
		start_cpu_timer(cpu);
	return 0;
}
__initcall(cpucache_init);

/*
 * Interface to system's page allocator. No need to hold the cache-lock.
 *
 * If we requested dmaable memory, we will get it. Even if we
 * did not request dmaable memory, we might get it, but that
 * would be relatively rare and ignorable.
 */
static void *kmem_getpages(struct kmem_cache *cachep, gfp_t flags, int nodeid)
{
	struct page *page;
	int nr_pages;
	int i;

#ifndef CONFIG_MMU
	/*
	 * Nommu uses slab's for process anonymous memory allocations, and thus
	 * requires __GFP_COMP to properly refcount higher order allocations
	 */
	flags |= __GFP_COMP;
#endif

	flags |= cachep->gfpflags;
	if (cachep->flags & SLAB_RECLAIM_ACCOUNT)
		flags |= __GFP_RECLAIMABLE;

	page = alloc_pages_exact_node(nodeid, flags | __GFP_NOTRACK, cachep->gfporder);
	if (!page)
		return NULL;

	nr_pages = (1 << cachep->gfporder);
	if (cachep->flags & SLAB_RECLAIM_ACCOUNT)
		add_zone_page_state(page_zone(page),
			NR_SLAB_RECLAIMABLE, nr_pages);
	else
		add_zone_page_state(page_zone(page),
			NR_SLAB_UNRECLAIMABLE, nr_pages);
	for (i = 0; i < nr_pages; i++)
		__SetPageSlab(page + i);

	if (kmemcheck_enabled && !(cachep->flags & SLAB_NOTRACK)) {
		kmemcheck_alloc_shadow(page, cachep->gfporder, flags, nodeid);

		if (cachep->ctor)
			kmemcheck_mark_uninitialized_pages(page, nr_pages);
		else
			kmemcheck_mark_unallocated_pages(page, nr_pages);
	}

	return page_address(page);
}

/*
 * Interface to system's page release.
 */
static void kmem_freepages(struct kmem_cache *cachep, void *addr)
{
	unsigned long i = (1 << cachep->gfporder);
	struct page *page = virt_to_page(addr);
	const unsigned long nr_freed = i;

	kmemcheck_free_shadow(page, cachep->gfporder);

	if (cachep->flags & SLAB_RECLAIM_ACCOUNT)
		sub_zone_page_state(page_zone(page),
				NR_SLAB_RECLAIMABLE, nr_freed);
	else
		sub_zone_page_state(page_zone(page),
				NR_SLAB_UNRECLAIMABLE, nr_freed);
	while (i--) {
		BUG_ON(!PageSlab(page));
		__ClearPageSlab(page);
		page++;
	}
	if (current->reclaim_state)
		current->reclaim_state->reclaimed_slab += nr_freed;
	free_pages((unsigned long)addr, cachep->gfporder);
}

static void kmem_rcu_free(struct rcu_head *head)
{
	struct slab_rcu *slab_rcu = (struct slab_rcu *)head;
	struct kmem_cache *cachep = slab_rcu->cachep;

	kmem_freepages(cachep, slab_rcu->addr);
	if (OFF_SLAB(cachep))
		kmem_cache_free(cachep->slabp_cache, slab_rcu);
}

#if DEBUG

#ifdef CONFIG_DEBUG_PAGEALLOC
static void store_stackinfo(struct kmem_cache *cachep, unsigned long *addr,
			    unsigned long caller)
{
	int size = obj_size(cachep);

	addr = (unsigned long *)&((char *)addr)[obj_offset(cachep)];

	if (size < 5 * sizeof(unsigned long))
		return;

	*addr++ = 0x12345678;
	*addr++ = caller;
	*addr++ = smp_processor_id();
	size -= 3 * sizeof(unsigned long);
	{
		unsigned long *sptr = &caller;
		unsigned long svalue;

		while (!kstack_end(sptr)) {
			svalue = *sptr++;
			if (kernel_text_address(svalue)) {
				*addr++ = svalue;
				size -= sizeof(unsigned long);
				if (size <= sizeof(unsigned long))
					break;
			}
		}

	}
	*addr++ = 0x87654321;
}
#endif

static void poison_obj(struct kmem_cache *cachep, void *addr, unsigned char val)
{
	int size = obj_size(cachep);
	addr = &((char *)addr)[obj_offset(cachep)];

	memset(addr, val, size);
	*(unsigned char *)(addr + size - 1) = POISON_END;
}

static void dump_line(char *data, int offset, int limit)
{
	int i;
	unsigned char error = 0;
	int bad_count = 0;

	printk(KERN_ERR "%03x:", offset);
	for (i = 0; i < limit; i++) {
		if (data[offset + i] != POISON_FREE) {
			error = data[offset + i];
			bad_count++;
		}
		printk(" %02x", (unsigned char)data[offset + i]);
	}
	printk("\n");

	if (bad_count == 1) {
		error ^= POISON_FREE;
		if (!(error & (error - 1))) {
			printk(KERN_ERR "Single bit error detected. Probably "
					"bad RAM.\n");
#ifdef CONFIG_X86
			printk(KERN_ERR "Run memtest86+ or a similar memory "
					"test tool.\n");
#else
			printk(KERN_ERR "Run a memory test tool.\n");
#endif
		}
	}
}
#endif

#if DEBUG

static void print_objinfo(struct kmem_cache *cachep, void *objp, int lines)
{
	int i, size;
	char *realobj;

	if (cachep->flags & SLAB_RED_ZONE) {
		printk(KERN_ERR "Redzone: 0x%llx/0x%llx.\n",
			*dbg_redzone1(cachep, objp),
			*dbg_redzone2(cachep, objp));
	}

	if (cachep->flags & SLAB_STORE_USER) {
		printk(KERN_ERR "Last user: [<%p>]",
			*dbg_userword(cachep, objp));
		print_symbol("(%s)",
				(unsigned long)*dbg_userword(cachep, objp));
		printk("\n");
	}
	realobj = (char *)objp + obj_offset(cachep);
	size = obj_size(cachep);
	for (i = 0; i < size && lines; i += 16, lines--) {
		int limit;
		limit = 16;
		if (i + limit > size)
			limit = size - i;
		dump_line(realobj, i, limit);
	}
}

static void check_poison_obj(struct kmem_cache *cachep, void *objp)
{
	char *realobj;
	int size, i;
	int lines = 0;

	realobj = (char *)objp + obj_offset(cachep);
	size = obj_size(cachep);

	for (i = 0; i < size; i++) {
		char exp = POISON_FREE;
		if (i == size - 1)
			exp = POISON_END;
		if (realobj[i] != exp) {
			int limit;
			/* Mismatch ! */
			/* Print header */
			if (lines == 0) {
				printk(KERN_ERR
					"Slab corruption: %s start=%p, len=%d\n",
					cachep->name, realobj, size);
				print_objinfo(cachep, objp, 0);
			}
			/* Hexdump the affected line */
			i = (i / 16) * 16;
			limit = 16;
			if (i + limit > size)
				limit = size - i;
			dump_line(realobj, i, limit);
			i += 16;
			lines++;
			/* Limit to 5 lines */
			if (lines > 5)
				break;
		}
	}
	if (lines != 0) {
		/* Print some data about the neighboring objects, if they
		 * exist:
		 */
		struct slab *slabp = virt_to_slab(objp);
		unsigned int objnr;

		objnr = obj_to_index(cachep, slabp, objp);
		if (objnr) {
			objp = index_to_obj(cachep, slabp, objnr - 1);
			realobj = (char *)objp + obj_offset(cachep);
			printk(KERN_ERR "Prev obj: start=%p, len=%d\n",
			       realobj, size);
			print_objinfo(cachep, objp, 2);
		}
		if (objnr + 1 < cachep->num) {
			objp = index_to_obj(cachep, slabp, objnr + 1);
			realobj = (char *)objp + obj_offset(cachep);
			printk(KERN_ERR "Next obj: start=%p, len=%d\n",
			       realobj, size);
			print_objinfo(cachep, objp, 2);
		}
	}
}
#endif

#if DEBUG
static void slab_destroy_debugcheck(struct kmem_cache *cachep, struct slab *slabp)
{
	int i;
	for (i = 0; i < cachep->num; i++) {
		void *objp = index_to_obj(cachep, slabp, i);

		if (cachep->flags & SLAB_POISON) {
#ifdef CONFIG_DEBUG_PAGEALLOC
			if (cachep->buffer_size % PAGE_SIZE == 0 &&
					OFF_SLAB(cachep))
				kernel_map_pages(virt_to_page(objp),
					cachep->buffer_size / PAGE_SIZE, 1);
			else
				check_poison_obj(cachep, objp);
#else
			check_poison_obj(cachep, objp);
#endif
		}
		if (cachep->flags & SLAB_RED_ZONE) {
			if (*dbg_redzone1(cachep, objp) != RED_INACTIVE)
				slab_error(cachep, "start of a freed object "
					   "was overwritten");
			if (*dbg_redzone2(cachep, objp) != RED_INACTIVE)
				slab_error(cachep, "end of a freed object "
					   "was overwritten");
		}
	}
}
#else
static void slab_destroy_debugcheck(struct kmem_cache *cachep, struct slab *slabp)
{
}
#endif

/**
 * slab_destroy - destroy and release all objects in a slab
 * @cachep: cache pointer being destroyed
 * @slabp: slab pointer being destroyed
 *
 * Destroy all the objs in a slab, and release the mem back to the system.
 * Before calling the slab must have been unlinked from the cache.  The
 * cache-lock is not held/needed.
 */
static void slab_destroy(struct kmem_cache *cachep, struct slab *slabp)
{
	void *addr = slabp->s_mem - slabp->colouroff;

	slab_destroy_debugcheck(cachep, slabp);
	if (unlikely(cachep->flags & SLAB_DESTROY_BY_RCU)) {
		struct slab_rcu *slab_rcu;

		slab_rcu = (struct slab_rcu *)slabp;
		slab_rcu->cachep = cachep;
		slab_rcu->addr = addr;
		call_rcu(&slab_rcu->head, kmem_rcu_free);
	} else {
		kmem_freepages(cachep, addr);
		if (OFF_SLAB(cachep))
			kmem_cache_free(cachep->slabp_cache, slabp);
	}
}

static void __kmem_cache_destroy(struct kmem_cache *cachep)
{
	int i;
	struct kmem_list3 *l3;

	for_each_online_cpu(i)
	    kfree(cachep->array[i]);

	/* NUMA: free the list3 structures */
	for_each_online_node(i) {
		l3 = cachep->nodelists[i];
		if (l3) {
			kfree(l3->shared);
			free_alien_cache(l3->alien);
			kfree(l3);
		}
	}
	kmem_cache_free(&cache_cache, cachep);
}


/**
 * calculate_slab_order - calculate size (page order) of slabs
 * @cachep: pointer to the cache that is being created
 * @size: size of objects to be created in this cache.
 * @align: required alignment for the objects.
 * @flags: slab allocation flags
 *
 * Also calculates the number of objects per slab.
 *
 * This could be made much more intelligent.  For now, try to avoid using
 * high order pages for slabs.  When the gfp() functions are more friendly
 * towards high-order requests, this should be changed.
 */
static size_t calculate_slab_order(struct kmem_cache *cachep,
			size_t size, size_t align, unsigned long flags)
{
	unsigned long offslab_limit;
	size_t left_over = 0;
	int gfporder;

	for (gfporder = 0; gfporder <= KMALLOC_MAX_ORDER; gfporder++) {
		unsigned int num;
		size_t remainder;

		cache_estimate(gfporder, size, align, flags, &remainder, &num);
		if (!num)
			continue;

		if (flags & CFLGS_OFF_SLAB) {
			/*
			 * Max number of objs-per-slab for caches which
			 * use off-slab slabs. Needed to avoid a possible
			 * looping condition in cache_grow().
			 */
			offslab_limit = size - sizeof(struct slab);
			offslab_limit /= sizeof(kmem_bufctl_t);

 			if (num > offslab_limit)
				break;
		}

		/* Found something acceptable - save it away */
		cachep->num = num;
		cachep->gfporder = gfporder;
		left_over = remainder;

		/*
		 * A VFS-reclaimable slab tends to have most allocations
		 * as GFP_NOFS and we really don't want to have to be allocating
		 * higher-order pages when we are unable to shrink dcache.
		 */
		if (flags & SLAB_RECLAIM_ACCOUNT)
			break;

		/*
		 * Large number of objects is good, but very large slabs are
		 * currently bad for the gfp()s.
		 */
		if (gfporder >= slab_break_gfp_order)
			break;

		/*
		 * Acceptable internal fragmentation?
		 */
		if (left_over * 8 <= (PAGE_SIZE << gfporder))
			break;
	}
	return left_over;
}

static int __init_refok setup_cpu_cache(struct kmem_cache *cachep, gfp_t gfp)
{
	if (g_cpucache_up == FULL)
		return enable_cpucache(cachep, gfp);

	if (g_cpucache_up == NONE) {
		/*
		 * Note: the first kmem_cache_create must create the cache
		 * that's used by kmalloc(24), otherwise the creation of
		 * further caches will BUG().
		 */
		cachep->array[smp_processor_id()] = &initarray_generic.cache;

		/*
		 * If the cache that's used by kmalloc(sizeof(kmem_list3)) is
		 * the first cache, then we need to set up all its list3s,
		 * otherwise the creation of further caches will BUG().
		 */
		set_up_list3s(cachep, SIZE_AC);
		if (INDEX_AC == INDEX_L3)
			g_cpucache_up = PARTIAL_L3;
		else
			g_cpucache_up = PARTIAL_AC;
	} else {
		cachep->array[smp_processor_id()] =
			kmalloc(sizeof(struct arraycache_init), gfp);

		if (g_cpucache_up == PARTIAL_AC) {
			set_up_list3s(cachep, SIZE_L3);
			g_cpucache_up = PARTIAL_L3;
		} else {
			int node;
			for_each_online_node(node) {
				cachep->nodelists[node] =
				    kmalloc_node(sizeof(struct kmem_list3),
						gfp, node);
				BUG_ON(!cachep->nodelists[node]);
				kmem_list3_init(cachep->nodelists[node]);
			}
		}
	}
	cachep->nodelists[numa_node_id()]->next_reap =
			jiffies + REAPTIMEOUT_LIST3 +
			((unsigned long)cachep) % REAPTIMEOUT_LIST3;

	cpu_cache_get(cachep)->avail = 0;
	cpu_cache_get(cachep)->limit = BOOT_CPUCACHE_ENTRIES;
	cpu_cache_get(cachep)->batchcount = 1;
	cpu_cache_get(cachep)->touched = 0;
	cachep->batchcount = 1;
	cachep->limit = BOOT_CPUCACHE_ENTRIES;
	return 0;
}

/**
 * kmem_cache_create - Create a cache.
 * @name: A string which is used in /proc/slabinfo to identify this cache.
 * @size: The size of objects to be created in this cache.
 * @align: The required alignment for the objects.
 * @flags: SLAB flags
 * @ctor: A constructor for the objects.
 *
 * Returns a ptr to the cache on success, NULL on failure.
 * Cannot be called within a int, but can be interrupted.
 * The @ctor is run when new pages are allocated by the cache.
 *
 * @name must be valid until the cache is destroyed. This implies that
 * the module calling this has to destroy the cache before getting unloaded.
 * Note that kmem_cache_name() is not guaranteed to return the same pointer,
 * therefore applications must manage it themselves.
 *
 * The flags are
 *
 * %SLAB_POISON - Poison the slab with a known test pattern (a5a5a5a5)
 * to catch references to uninitialised memory.
 *
 * %SLAB_RED_ZONE - Insert `Red' zones around the allocated memory to check
 * for buffer overruns.
 *
 * %SLAB_HWCACHE_ALIGN - Align the objects in this cache to a hardware
 * cacheline.  This can be beneficial if you're counting cycles as closely
 * as davem.
 */
struct kmem_cache *
kmem_cache_create (const char *name, size_t size, size_t align,
	unsigned long flags, void (*ctor)(void *))
{
	size_t left_over, slab_size, ralign;
	struct kmem_cache *cachep = NULL, *pc;
	gfp_t gfp;

	/*
	 * Sanity checks... these are all serious usage bugs.
	 */
	if (!name || in_interrupt() || (size < BYTES_PER_WORD) ||
	    size > KMALLOC_MAX_SIZE) {
		printk(KERN_ERR "%s: Early error in slab %s\n", __func__,
				name);
		BUG();
	}

	/*
	 * We use cache_chain_mutex to ensure a consistent view of
	 * cpu_online_mask as well.  Please see cpuup_callback
	 */
	if (slab_is_available()) {
		get_online_cpus();
		mutex_lock(&cache_chain_mutex);
	}

	list_for_each_entry(pc, &cache_chain, next) {
		char tmp;
		int res;

		/*
		 * This happens when the module gets unloaded and doesn't
		 * destroy its slab cache and no-one else reuses the vmalloc
		 * area of the module.  Print a warning.
		 */
		res = probe_kernel_address(pc->name, tmp);
		if (res) {
			printk(KERN_ERR
			       "SLAB: cache with size %d has lost its name\n",
			       pc->buffer_size);
			continue;
		}

		if (!strcmp(pc->name, name)) {
			printk(KERN_ERR
			       "kmem_cache_create: duplicate cache %s\n", name);
			dump_stack();
			goto oops;
		}
	}

#if DEBUG
	WARN_ON(strchr(name, ' '));	/* It confuses parsers */
#if FORCED_DEBUG
	/*
	 * Enable redzoning and last user accounting, except for caches with
	 * large objects, if the increased size would increase the object size
	 * above the next power of two: caches with object sizes just above a
	 * power of two have a significant amount of internal fragmentation.
	 */
	if (size < 4096 || fls(size - 1) == fls(size-1 + REDZONE_ALIGN +
						2 * sizeof(unsigned long long)))
		flags |= SLAB_RED_ZONE | SLAB_STORE_USER;
	if (!(flags & SLAB_DESTROY_BY_RCU))
		flags |= SLAB_POISON;
#endif
	if (flags & SLAB_DESTROY_BY_RCU)
		BUG_ON(flags & SLAB_POISON);
#endif
	/*
	 * Always checks flags, a caller might be expecting debug support which
	 * isn't available.
	 */
	BUG_ON(flags & ~CREATE_MASK);

	/*
	 * Check that size is in terms of words.  This is needed to avoid
	 * unaligned accesses for some archs when redzoning is used, and makes
	 * sure any on-slab bufctl's are also correctly aligned.
	 */
	if (size & (BYTES_PER_WORD - 1)) {
		size += (BYTES_PER_WORD - 1);
		size &= ~(BYTES_PER_WORD - 1);
	}

	/* calculate the final buffer alignment: */

	/* 1) arch recommendation: can be overridden for debug */
	if (flags & SLAB_HWCACHE_ALIGN) {
		/*
		 * Default alignment: as specified by the arch code.  Except if
		 * an object is really small, then squeeze multiple objects into
		 * one cacheline.
		 */
		ralign = cache_line_size();
		while (size <= ralign / 2)
			ralign /= 2;
	} else {
		ralign = BYTES_PER_WORD;
	}

	/*
	 * Redzoning and user store require word alignment or possibly larger.
	 * Note this will be overridden by architecture or caller mandated
	 * alignment if either is greater than BYTES_PER_WORD.
	 */
	if (flags & SLAB_STORE_USER)
		ralign = BYTES_PER_WORD;

	if (flags & SLAB_RED_ZONE) {
		ralign = REDZONE_ALIGN;
		/* If redzoning, ensure that the second redzone is suitably
		 * aligned, by adjusting the object size accordingly. */
		size += REDZONE_ALIGN - 1;
		size &= ~(REDZONE_ALIGN - 1);
	}

	/* 2) arch mandated alignment */
	if (ralign < ARCH_SLAB_MINALIGN) {
		ralign = ARCH_SLAB_MINALIGN;
	}
	/* 3) caller mandated alignment */
	if (ralign < align) {
		ralign = align;
	}
	/* disable debug if necessary */
	if (ralign > __alignof__(unsigned long long))
		flags &= ~(SLAB_RED_ZONE | SLAB_STORE_USER);
	/*
	 * 4) Store it.
	 */
	align = ralign;

	if (slab_is_available())
		gfp = GFP_KERNEL;
	else
		gfp = GFP_NOWAIT;

	/* Get cache's description obj. */
	cachep = kmem_cache_zalloc(&cache_cache, gfp);
	if (!cachep)
		goto oops;

#if DEBUG
	cachep->obj_size = size;

	/*
	 * Both debugging options require word-alignment which is calculated
	 * into align above.
	 */
	if (flags & SLAB_RED_ZONE) {
		/* add space for red zone words */
		cachep->obj_offset += sizeof(unsigned long long);
		size += 2 * sizeof(unsigned long long);
	}
	if (flags & SLAB_STORE_USER) {
		/* user store requires one word storage behind the end of
		 * the real object. But if the second red zone needs to be
		 * aligned to 64 bits, we must allow that much space.
		 */
		if (flags & SLAB_RED_ZONE)
			size += REDZONE_ALIGN;
		else
			size += BYTES_PER_WORD;
	}
#if FORCED_DEBUG && defined(CONFIG_DEBUG_PAGEALLOC)
	if (size >= malloc_sizes[INDEX_L3 + 1].cs_size
	    && cachep->obj_size > cache_line_size() && size < PAGE_SIZE) {
		cachep->obj_offset += PAGE_SIZE - size;
		size = PAGE_SIZE;
	}
#endif
#endif

	/*
	 * Determine if the slab management is 'on' or 'off' slab.
	 * (bootstrapping cannot cope with offslab caches so don't do
	 * it too early on.)
	 */
	if ((size >= (PAGE_SIZE >> 3)) && !slab_early_init)
		/*
		 * Size is large, assume best to place the slab management obj
		 * off-slab (should allow better packing of objs).
		 */
		flags |= CFLGS_OFF_SLAB;

	size = ALIGN(size, align);

	left_over = calculate_slab_order(cachep, size, align, flags);

	if (!cachep->num) {
		printk(KERN_ERR
		       "kmem_cache_create: couldn't create cache %s.\n", name);
		kmem_cache_free(&cache_cache, cachep);
		cachep = NULL;
		goto oops;
	}
	slab_size = ALIGN(cachep->num * sizeof(kmem_bufctl_t)
			  + sizeof(struct slab), align);

	/*
	 * If the slab has been placed off-slab, and we have enough space then
	 * move it on-slab. This is at the expense of any extra colouring.
	 */
	if (flags & CFLGS_OFF_SLAB && left_over >= slab_size) {
		flags &= ~CFLGS_OFF_SLAB;
		left_over -= slab_size;
	}

	if (flags & CFLGS_OFF_SLAB) {
		/* really off slab. No need for manual alignment */
		slab_size =
		    cachep->num * sizeof(kmem_bufctl_t) + sizeof(struct slab);

#ifdef CONFIG_PAGE_POISONING
		/* If we're going to use the generic kernel_map_pages()
		 * poisoning, then it's going to smash the contents of
		 * the redzone and userword anyhow, so switch them off.
		 */
		if (size % PAGE_SIZE == 0 && flags & SLAB_POISON)
			flags &= ~(SLAB_RED_ZONE | SLAB_STORE_USER);
#endif
	}

	cachep->colour_off = cache_line_size();
	/* Offset must be a multiple of the alignment. */
	if (cachep->colour_off < align)
		cachep->colour_off = align;
	cachep->colour = left_over / cachep->colour_off;
	cachep->slab_size = slab_size;
	cachep->flags = flags;
	cachep->gfpflags = 0;
	if (CONFIG_ZONE_DMA_FLAG && (flags & SLAB_CACHE_DMA))
		cachep->gfpflags |= GFP_DMA;
	cachep->buffer_size = size;
	cachep->reciprocal_buffer_size = reciprocal_value(size);

	if (flags & CFLGS_OFF_SLAB) {
		cachep->slabp_cache = kmem_find_general_cachep(slab_size, 0u);
		/*
		 * This is a possibility for one of the malloc_sizes caches.
		 * But since we go off slab only for object size greater than
		 * PAGE_SIZE/8, and malloc_sizes gets created in ascending order,
		 * this should not happen at all.
		 * But leave a BUG_ON for some lucky dude.
		 */
		BUG_ON(ZERO_OR_NULL_PTR(cachep->slabp_cache));
	}
	cachep->ctor = ctor;
	cachep->name = name;

	if (setup_cpu_cache(cachep, gfp)) {
		__kmem_cache_destroy(cachep);
		cachep = NULL;
		goto oops;
	}

	/* cache setup completed, link it into the list */
	list_add(&cachep->next, &cache_chain);
oops:
	if (!cachep && (flags & SLAB_PANIC))
		panic("kmem_cache_create(): failed to create slab `%s'\n",
		      name);
	if (slab_is_available()) {
		mutex_unlock(&cache_chain_mutex);
		put_online_cpus();
	}
	return cachep;
}
EXPORT_SYMBOL(kmem_cache_create);

#if DEBUG
static void check_irq_off(void)
{
	BUG_ON(!irqs_disabled());
}

static void check_irq_on(void)
{
	BUG_ON(irqs_disabled());
}

static void check_spinlock_acquired(struct kmem_cache *cachep)
{
#ifdef CONFIG_SMP
	check_irq_off();
	assert_spin_locked(&cachep->nodelists[numa_node_id()]->list_lock);
#endif
}

static void check_spinlock_acquired_node(struct kmem_cache *cachep, int node)
{
#ifdef CONFIG_SMP
	check_irq_off();
	assert_spin_locked(&cachep->nodelists[node]->list_lock);
#endif
}

#else
#define check_irq_off()	do { } while(0)
#define check_irq_on()	do { } while(0)
#define check_spinlock_acquired(x) do { } while(0)
#define check_spinlock_acquired_node(x, y) do { } while(0)
#endif

static void drain_array(struct kmem_cache *cachep, struct kmem_list3 *l3,
			struct array_cache *ac,
			int force, int node);

static void do_drain(void *arg)
{
	struct kmem_cache *cachep = arg;
	struct array_cache *ac;
	int node = numa_node_id();

	check_irq_off();
	ac = cpu_cache_get(cachep);
	spin_lock(&cachep->nodelists[node]->list_lock);
	free_block(cachep, ac->entry, ac->avail, node);
	spin_unlock(&cachep->nodelists[node]->list_lock);
	ac->avail = 0;
}

static void drain_cpu_caches(struct kmem_cache *cachep)
{
	struct kmem_list3 *l3;
	int node;

	on_each_cpu(do_drain, cachep, 1);
	check_irq_on();
	for_each_online_node(node) {
		l3 = cachep->nodelists[node];
		if (l3 && l3->alien)
			drain_alien_cache(cachep, l3->alien);
	}

	for_each_online_node(node) {
		l3 = cachep->nodelists[node];
		if (l3)
			drain_array(cachep, l3, l3->shared, 1, node);
	}
}

/*
 * Remove slabs from the list of free slabs.
 * Specify the number of slabs to drain in tofree.
 *
 * Returns the actual number of slabs released.
 */
static int drain_freelist(struct kmem_cache *cache,
			struct kmem_list3 *l3, int tofree)
{
	struct list_head *p;
	int nr_freed;
	struct slab *slabp;

	nr_freed = 0;
	while (nr_freed < tofree && !list_empty(&l3->slabs_free)) {

		spin_lock_irq(&l3->list_lock);
		p = l3->slabs_free.prev;
		if (p == &l3->slabs_free) {
			spin_unlock_irq(&l3->list_lock);
			goto out;
		}

		slabp = list_entry(p, struct slab, list);
#if DEBUG
		BUG_ON(slabp->inuse);
#endif
		list_del(&slabp->list);
		/*
		 * Safe to drop the lock. The slab is no longer linked
		 * to the cache.
		 */
		l3->free_objects -= cache->num;
		spin_unlock_irq(&l3->list_lock);
		slab_destroy(cache, slabp);
		nr_freed++;
	}
out:
	return nr_freed;
}

/* Called with cache_chain_mutex held to protect against cpu hotplug */
static int __cache_shrink(struct kmem_cache *cachep)
{
	int ret = 0, i = 0;
	struct kmem_list3 *l3;

	drain_cpu_caches(cachep);

	check_irq_on();
	for_each_online_node(i) {
		l3 = cachep->nodelists[i];
		if (!l3)
			continue;

		drain_freelist(cachep, l3, l3->free_objects);

		ret += !list_empty(&l3->slabs_full) ||
			!list_empty(&l3->slabs_partial);
	}
	return (ret ? 1 : 0);
}

/**
 * kmem_cache_shrink - Shrink a cache.
 * @cachep: The cache to shrink.
 *
 * Releases as many slabs as possible for a cache.
 * To help debugging, a zero exit status indicates all slabs were released.
 */
int kmem_cache_shrink(struct kmem_cache *cachep)
{
	int ret;
	BUG_ON(!cachep || in_interrupt());

	get_online_cpus();
	mutex_lock(&cache_chain_mutex);
	ret = __cache_shrink(cachep);
	mutex_unlock(&cache_chain_mutex);
	put_online_cpus();
	return ret;
}
EXPORT_SYMBOL(kmem_cache_shrink);

/**
 * kmem_cache_destroy - delete a cache
 * @cachep: the cache to destroy
 *
 * Remove a &struct kmem_cache object from the slab cache.
 *
 * It is expected this function will be called by a module when it is
 * unloaded.  This will remove the cache completely, and avoid a duplicate
 * cache being allocated each time a module is loaded and unloaded, if the
 * module doesn't have persistent in-kernel storage across loads and unloads.
 *
 * The cache must be empty before calling this function.
 *
 * The caller must guarantee that noone will allocate memory from the cache
 * during the kmem_cache_destroy().
 */
void kmem_cache_destroy(struct kmem_cache *cachep)
{
	BUG_ON(!cachep || in_interrupt());

	/* Find the cache in the chain of caches. */
	get_online_cpus();
	mutex_lock(&cache_chain_mutex);
	/*
	 * the chain is never empty, cache_cache is never destroyed
	 */
	list_del(&cachep->next);
	if (__cache_shrink(cachep)) {
		slab_error(cachep, "Can't free all objects");
		list_add(&cachep->next, &cache_chain);
		mutex_unlock(&cache_chain_mutex);
		put_online_cpus();
		return;
	}

	if (unlikely(cachep->flags & SLAB_DESTROY_BY_RCU))
		rcu_barrier();

	__kmem_cache_destroy(cachep);
	mutex_unlock(&cache_chain_mutex);
	put_online_cpus();
}
EXPORT_SYMBOL(kmem_cache_destroy);

/*
 * Get the memory for a slab management obj.
 * For a slab cache when the slab descriptor is off-slab, slab descriptors
 * always come from malloc_sizes caches.  The slab descriptor cannot
 * come from the same cache which is getting created because,
 * when we are searching for an appropriate cache for these
 * descriptors in kmem_cache_create, we search through the malloc_sizes array.
 * If we are creating a malloc_sizes cache here it would not be visible to
 * kmem_find_general_cachep till the initialization is complete.
 * Hence we cannot have slabp_cache same as the original cache.
 */
static struct slab *alloc_slabmgmt(struct kmem_cache *cachep, void *objp,
				   int colour_off, gfp_t local_flags,
				   int nodeid)
{
	struct slab *slabp;

	if (OFF_SLAB(cachep)) {
		/* Slab management obj is off-slab. */
		slabp = kmem_cache_alloc_node(cachep->slabp_cache,
					      local_flags, nodeid);
		/*
		 * If the first object in the slab is leaked (it's allocated
		 * but no one has a reference to it), we want to make sure
		 * kmemleak does not treat the ->s_mem pointer as a reference
		 * to the object. Otherwise we will not report the leak.
		 */
		kmemleak_scan_area(slabp, offsetof(struct slab, list),
				   sizeof(struct list_head), local_flags);
		if (!slabp)
			return NULL;
	} else {
		slabp = objp + colour_off;
		colour_off += cachep->slab_size;
	}
	slabp->inuse = 0;
	slabp->colouroff = colour_off;
	slabp->s_mem = objp + colour_off;
	slabp->nodeid = nodeid;
	slabp->free = 0;
	return slabp;
}

static inline kmem_bufctl_t *slab_bufctl(struct slab *slabp)
{
	return (kmem_bufctl_t *) (slabp + 1);
}

static void cache_init_objs(struct kmem_cache *cachep,
			    struct slab *slabp)
{
	int i;

	for (i = 0; i < cachep->num; i++) {
		void *objp = index_to_obj(cachep, slabp, i);
#if DEBUG
		/* need to poison the objs? */
		if (cachep->flags & SLAB_POISON)
			poison_obj(cachep, objp, POISON_FREE);
		if (cachep->flags & SLAB_STORE_USER)
			*dbg_userword(cachep, objp) = NULL;

		if (cachep->flags & SLAB_RED_ZONE) {
			*dbg_redzone1(cachep, objp) = RED_INACTIVE;
			*dbg_redzone2(cachep, objp) = RED_INACTIVE;
		}
		/*
		 * Constructors are not allowed to allocate memory from the same
		 * cache which they are a constructor for.  Otherwise, deadlock.
		 * They must also be threaded.
		 */
		if (cachep->ctor && !(cachep->flags & SLAB_POISON))
			cachep->ctor(objp + obj_offset(cachep));

		if (cachep->flags & SLAB_RED_ZONE) {
			if (*dbg_redzone2(cachep, objp) != RED_INACTIVE)
				slab_error(cachep, "constructor overwrote the"
					   " end of an object");
			if (*dbg_redzone1(cachep, objp) != RED_INACTIVE)
				slab_error(cachep, "constructor overwrote the"
					   " start of an object");
		}
		if ((cachep->buffer_size % PAGE_SIZE) == 0 &&
			    OFF_SLAB(cachep) && cachep->flags & SLAB_POISON)
			kernel_map_pages(virt_to_page(objp),
					 cachep->buffer_size / PAGE_SIZE, 0);
#else
		if (cachep->ctor)
			cachep->ctor(objp);
#endif
		slab_bufctl(slabp)[i] = i + 1;
	}
	slab_bufctl(slabp)[i - 1] = BUFCTL_END;
}

static void kmem_flagcheck(struct kmem_cache *cachep, gfp_t flags)
{
	if (CONFIG_ZONE_DMA_FLAG) {
		if (flags & GFP_DMA)
			BUG_ON(!(cachep->gfpflags & GFP_DMA));
		else
			BUG_ON(cachep->gfpflags & GFP_DMA);
	}
}

static void *slab_get_obj(struct kmem_cache *cachep, struct slab *slabp,
				int nodeid)
{
	void *objp = index_to_obj(cachep, slabp, slabp->free);
	kmem_bufctl_t next;

	slabp->inuse++;
	next = slab_bufctl(slabp)[slabp->free];
#if DEBUG
	slab_bufctl(slabp)[slabp->free] = BUFCTL_FREE;
	WARN_ON(slabp->nodeid != nodeid);
#endif
	slabp->free = next;

	return objp;
}

static void slab_put_obj(struct kmem_cache *cachep, struct slab *slabp,
				void *objp, int nodeid)
{
	unsigned int objnr = obj_to_index(cachep, slabp, objp);

#if DEBUG
	/* Verify that the slab belongs to the intended node */
	WARN_ON(slabp->nodeid != nodeid);

	if (slab_bufctl(slabp)[objnr] + 1 <= SLAB_LIMIT + 1) {
		printk(KERN_ERR "slab: double free detected in cache "
				"'%s', objp %p\n", cachep->name, objp);
		BUG();
	}
#endif
	slab_bufctl(slabp)[objnr] = slabp->free;
	slabp->free = objnr;
	slabp->inuse--;
}

/*
 * Map pages beginning at addr to the given cache and slab. This is required
 * for the slab allocator to be able to lookup the cache and slab of a
 * virtual address for kfree, ksize, kmem_ptr_validate, and slab debugging.
 */
static void slab_map_pages(struct kmem_cache *cache, struct slab *slab,
			   void *addr)
{
	int nr_pages;
	struct page *page;

	page = virt_to_page(addr);

	nr_pages = 1;
	if (likely(!PageCompound(page)))
		nr_pages <<= cache->gfporder;

	do {
		page_set_cache(page, cache);
		page_set_slab(page, slab);
		page++;
	} while (--nr_pages);
}

/*
 * Grow (by 1) the number of slabs within a cache.  This is called by
 * kmem_cache_alloc() when there are no active objs left in a cache.
 */
static int cache_grow(struct kmem_cache *cachep,
		gfp_t flags, int nodeid, void *objp)
{
	struct slab *slabp;
	size_t offset;
	gfp_t local_flags;
	struct kmem_list3 *l3;

	/*
	 * Be lazy and only check for valid flags here,  keeping it out of the
	 * critical path in kmem_cache_alloc().
	 */
	BUG_ON(flags & GFP_SLAB_BUG_MASK);
	local_flags = flags & (GFP_CONSTRAINT_MASK|GFP_RECLAIM_MASK);

	/* Take the l3 list lock to change the colour_next on this node */
	check_irq_off();
	l3 = cachep->nodelists[nodeid];
	spin_lock(&l3->list_lock);

	/* Get colour for the slab, and cal the next value. */
	offset = l3->colour_next;
	l3->colour_next++;
	if (l3->colour_next >= cachep->colour)
		l3->colour_next = 0;
	spin_unlock(&l3->list_lock);

	offset *= cachep->colour_off;

	if (local_flags & __GFP_WAIT)
		local_irq_enable();

	/*
	 * The test for missing atomic flag is performed here, rather than
	 * the more obvious place, simply to reduce the critical path length
	 * in kmem_cache_alloc(). If a caller is seriously mis-behaving they
	 * will eventually be caught here (where it matters).
	 */
	kmem_flagcheck(cachep, flags);

	/*
	 * Get mem for the objs.  Attempt to allocate a physical page from
	 * 'nodeid'.
	 */
	if (!objp)
		objp = kmem_getpages(cachep, local_flags, nodeid);
	if (!objp)
		goto failed;

	/* Get slab management. */
	slabp = alloc_slabmgmt(cachep, objp, offset,
			local_flags & ~GFP_CONSTRAINT_MASK, nodeid);
	if (!slabp)
		goto opps1;

	slab_map_pages(cachep, slabp, objp);

	cache_init_objs(cachep, slabp);

	if (local_flags & __GFP_WAIT)
		local_irq_disable();
	check_irq_off();
	spin_lock(&l3->list_lock);

	/* Make slab active. */
	list_add_tail(&slabp->list, &(l3->slabs_free));
	STATS_INC_GROWN(cachep);
	l3->free_objects += cachep->num;
	spin_unlock(&l3->list_lock);
	return 1;
opps1:
	kmem_freepages(cachep, objp);
failed:
	if (local_flags & __GFP_WAIT)
		local_irq_disable();
	return 0;
}

#if DEBUG

/*
 * Perform extra freeing checks:
 * - detect bad pointers.
 * - POISON/RED_ZONE checking
 */
static void kfree_debugcheck(const void *objp)
{
	if (!virt_addr_valid(objp)) {
		printk(KERN_ERR "kfree_debugcheck: out of range ptr %lxh.\n",
		       (unsigned long)objp);
		BUG();
	}
}

static inline void verify_redzone_free(struct kmem_cache *cache, void *obj)
{
	unsigned long long redzone1, redzone2;

	redzone1 = *dbg_redzone1(cache, obj);
	redzone2 = *dbg_redzone2(cache, obj);

	/*
	 * Redzone is ok.
	 */
	if (redzone1 == RED_ACTIVE && redzone2 == RED_ACTIVE)
		return;

	if (redzone1 == RED_INACTIVE && redzone2 == RED_INACTIVE)
		slab_error(cache, "double free detected");
	else
		slab_error(cache, "memory outside object was overwritten");

	printk(KERN_ERR "%p: redzone 1:0x%llx, redzone 2:0x%llx.\n",
			obj, redzone1, redzone2);
}

static void *cache_free_debugcheck(struct kmem_cache *cachep, void *objp,
				   void *caller)
{
	struct page *page;
	unsigned int objnr;
	struct slab *slabp;

	BUG_ON(virt_to_cache(objp) != cachep);

	objp -= obj_offset(cachep);
	kfree_debugcheck(objp);
	page = virt_to_head_page(objp);

	slabp = page_get_slab(page);

	if (cachep->flags & SLAB_RED_ZONE) {
		verify_redzone_free(cachep, objp);
		*dbg_redzone1(cachep, objp) = RED_INACTIVE;
		*dbg_redzone2(cachep, objp) = RED_INACTIVE;
	}
	if (cachep->flags & SLAB_STORE_USER)
		*dbg_userword(cachep, objp) = caller;

	objnr = obj_to_index(cachep, slabp, objp);

	BUG_ON(objnr >= cachep->num);
	BUG_ON(objp != index_to_obj(cachep, slabp, objnr));

#ifdef CONFIG_DEBUG_SLAB_LEAK
	slab_bufctl(slabp)[objnr] = BUFCTL_FREE;
#endif
	if (cachep->flags & SLAB_POISON) {
#ifdef CONFIG_DEBUG_PAGEALLOC
		if ((cachep->buffer_size % PAGE_SIZE)==0 && OFF_SLAB(cachep)) {
			store_stackinfo(cachep, objp, (unsigned long)caller);
			kernel_map_pages(virt_to_page(objp),
					 cachep->buffer_size / PAGE_SIZE, 0);
		} else {
			poison_obj(cachep, objp, POISON_FREE);
		}
#else
		poison_obj(cachep, objp, POISON_FREE);
#endif
	}
	return objp;
}

static void check_slabp(struct kmem_cache *cachep, struct slab *slabp)
{
	kmem_bufctl_t i;
	int entries = 0;

	/* Check slab's freelist to see if this obj is there. */
	for (i = slabp->free; i != BUFCTL_END; i = slab_bufctl(slabp)[i]) {
		entries++;
		if (entries > cachep->num || i >= cachep->num)
			goto bad;
	}
	if (entries != cachep->num - slabp->inuse) {
bad:
		printk(KERN_ERR "slab: Internal list corruption detected in "
				"cache '%s'(%d), slabp %p(%d). Hexdump:\n",
			cachep->name, cachep->num, slabp, slabp->inuse);
		for (i = 0;
		     i < sizeof(*slabp) + cachep->num * sizeof(kmem_bufctl_t);
		     i++) {
			if (i % 16 == 0)
				printk("\n%03x:", i);
			printk(" %02x", ((unsigned char *)slabp)[i]);
		}
		printk("\n");
		BUG();
	}
}
#else
#define kfree_debugcheck(x) do { } while(0)
#define cache_free_debugcheck(x,objp,z) (objp)
#define check_slabp(x,y) do { } while(0)
#endif

static void *cache_alloc_refill(struct kmem_cache *cachep, gfp_t flags)
{
	int batchcount;
	struct kmem_list3 *l3;
	struct array_cache *ac;
	int node;

retry:
	check_irq_off();
	node = numa_node_id();
	ac = cpu_cache_get(cachep);
	batchcount = ac->batchcount;
	if (!ac->touched && batchcount > BATCHREFILL_LIMIT) {
		/*
		 * If there was little recent activity on this cache, then
		 * perform only a partial refill.  Otherwise we could generate
		 * refill bouncing.
		 */
		batchcount = BATCHREFILL_LIMIT;
	}
	l3 = cachep->nodelists[node];

	BUG_ON(ac->avail > 0 || !l3);
	spin_lock(&l3->list_lock);

	/* See if we can refill from the shared array */
	if (l3->shared && transfer_objects(ac, l3->shared, batchcount))
		goto alloc_done;

	while (batchcount > 0) {
		struct list_head *entry;
		struct slab *slabp;
		/* Get slab alloc is to come from. */
		entry = l3->slabs_partial.next;
		if (entry == &l3->slabs_partial) {
			l3->free_touched = 1;
			entry = l3->slabs_free.next;
			if (entry == &l3->slabs_free)
				goto must_grow;
		}

		slabp = list_entry(entry, struct slab, list);
		check_slabp(cachep, slabp);
		check_spinlock_acquired(cachep);

		/*
		 * The slab was either on partial or free list so
		 * there must be at least one object available for
		 * allocation.
		 */
		BUG_ON(slabp->inuse >= cachep->num);

		while (slabp->inuse < cachep->num && batchcount--) {
			STATS_INC_ALLOCED(cachep);
			STATS_INC_ACTIVE(cachep);
			STATS_SET_HIGH(cachep);

			ac->entry[ac->avail++] = slab_get_obj(cachep, slabp,
							    node);
		}
		check_slabp(cachep, slabp);

		/* move slabp to correct slabp list: */
		list_del(&slabp->list);
		if (slabp->free == BUFCTL_END)
			list_add(&slabp->list, &l3->slabs_full);
		else
			list_add(&slabp->list, &l3->slabs_partial);
	}

must_grow:
	l3->free_objects -= ac->avail;
alloc_done:
	spin_unlock(&l3->list_lock);

	if (unlikely(!ac->avail)) {
		int x;
		x = cache_grow(cachep, flags | GFP_THISNODE, node, NULL);

		/* cache_grow can reenable interrupts, then ac could change. */
		ac = cpu_cache_get(cachep);
		if (!x && ac->avail == 0)	/* no objects in sight? abort */
			return NULL;

		if (!ac->avail)		/* objects refilled by interrupt? */
			goto retry;
	}
	ac->touched = 1;
	return ac->entry[--ac->avail];
}

static inline void cache_alloc_debugcheck_before(struct kmem_cache *cachep,
						gfp_t flags)
{
	might_sleep_if(flags & __GFP_WAIT);
#if DEBUG
	kmem_flagcheck(cachep, flags);
#endif
}

#if DEBUG
static void *cache_alloc_debugcheck_after(struct kmem_cache *cachep,
				gfp_t flags, void *objp, void *caller)
{
	if (!objp)
		return objp;
	if (cachep->flags & SLAB_POISON) {
#ifdef CONFIG_DEBUG_PAGEALLOC
		if ((cachep->buffer_size % PAGE_SIZE) == 0 && OFF_SLAB(cachep))
			kernel_map_pages(virt_to_page(objp),
					 cachep->buffer_size / PAGE_SIZE, 1);
		else
			check_poison_obj(cachep, objp);
#else
		check_poison_obj(cachep, objp);
#endif
		poison_obj(cachep, objp, POISON_INUSE);
	}
	if (cachep->flags & SLAB_STORE_USER)
		*dbg_userword(cachep, objp) = caller;

	if (cachep->flags & SLAB_RED_ZONE) {
		if (*dbg_redzone1(cachep, objp) != RED_INACTIVE ||
				*dbg_redzone2(cachep, objp) != RED_INACTIVE) {
			slab_error(cachep, "double free, or memory outside"
						" object was overwritten");
			printk(KERN_ERR
				"%p: redzone 1:0x%llx, redzone 2:0x%llx\n",
				objp, *dbg_redzone1(cachep, objp),
				*dbg_redzone2(cachep, objp));
		}
		*dbg_redzone1(cachep, objp) = RED_ACTIVE;
		*dbg_redzone2(cachep, objp) = RED_ACTIVE;
	}
#ifdef CONFIG_DEBUG_SLAB_LEAK
	{
		struct slab *slabp;
		unsigned objnr;

		slabp = page_get_slab(virt_to_head_page(objp));
		objnr = (unsigned)(objp - slabp->s_mem) / cachep->buffer_size;
		slab_bufctl(slabp)[objnr] = BUFCTL_ACTIVE;
	}
#endif
	objp += obj_offset(cachep);
	if (cachep->ctor && cachep->flags & SLAB_POISON)
		cachep->ctor(objp);
#if ARCH_SLAB_MINALIGN
	if ((u32)objp & (ARCH_SLAB_MINALIGN-1)) {
		printk(KERN_ERR "0x%p: not aligned to ARCH_SLAB_MINALIGN=%d\n",
		       objp, ARCH_SLAB_MINALIGN);
	}
#endif
	return objp;
}
#else
#define cache_alloc_debugcheck_after(a,b,objp,d) (objp)
#endif

static bool slab_should_failslab(struct kmem_cache *cachep, gfp_t flags)
{
	if (cachep == &cache_cache)
		return false;

	return should_failslab(obj_size(cachep), flags);
}

static inline void *____cache_alloc(struct kmem_cache *cachep, gfp_t flags)
{
	void *objp;
	struct array_cache *ac;

	check_irq_off();

	ac = cpu_cache_get(cachep);
	if (likely(ac->avail)) {
		STATS_INC_ALLOCHIT(cachep);
		ac->touched = 1;
		objp = ac->entry[--ac->avail];
	} else {
		STATS_INC_ALLOCMISS(cachep);
		objp = cache_alloc_refill(cachep, flags);
	}
	/*
	 * To avoid a false negative, if an object that is in one of the
	 * per-CPU caches is leaked, we need to make sure kmemleak doesn't
	 * treat the array pointers as a reference to the object.
	 */
	kmemleak_erase(&ac->entry[ac->avail]);
	return objp;
}

#ifdef CONFIG_NUMA
/*
 * Try allocating on another node if PF_SPREAD_SLAB|PF_MEMPOLICY.
 *
 * If we are in_interrupt, then process context, including cpusets and
 * mempolicy, may not apply and should not be used for allocation policy.
 */
static void *alternate_node_alloc(struct kmem_cache *cachep, gfp_t flags)
{
	int nid_alloc, nid_here;

	if (in_interrupt() || (flags & __GFP_THISNODE))
		return NULL;
	nid_alloc = nid_here = numa_node_id();
	if (cpuset_do_slab_mem_spread() && (cachep->flags & SLAB_MEM_SPREAD))
		nid_alloc = cpuset_mem_spread_node();
	else if (current->mempolicy)
		nid_alloc = slab_node(current->mempolicy);
	if (nid_alloc != nid_here)
		return ____cache_alloc_node(cachep, flags, nid_alloc);
	return NULL;
}

/*
 * Fallback function if there was no memory available and no objects on a
 * certain node and fall back is permitted. First we scan all the
 * available nodelists for available objects. If that fails then we
 * perform an allocation without specifying a node. This allows the page
 * allocator to do its reclaim / fallback magic. We then insert the
 * slab into the proper nodelist and then allocate from it.
 */
static void *fallback_alloc(struct kmem_cache *cache, gfp_t flags)
{
	struct zonelist *zonelist;
	gfp_t local_flags;
	struct zoneref *z;
	struct zone *zone;
	enum zone_type high_zoneidx = gfp_zone(flags);
	void *obj = NULL;
	int nid;

	if (flags & __GFP_THISNODE)
		return NULL;

	zonelist = node_zonelist(slab_node(current->mempolicy), flags);
	local_flags = flags & (GFP_CONSTRAINT_MASK|GFP_RECLAIM_MASK);

retry:
	/*
	 * Look through allowed nodes for objects available
	 * from existing per node queues.
	 */
	for_each_zone_zonelist(zone, z, zonelist, high_zoneidx) {
		nid = zone_to_nid(zone);

		if (cpuset_zone_allowed_hardwall(zone, flags) &&
			cache->nodelists[nid] &&
			cache->nodelists[nid]->free_objects) {
				obj = ____cache_alloc_node(cache,
					flags | GFP_THISNODE, nid);
				if (obj)
					break;
		}
	}

	if (!obj) {
		/*
		 * This allocation will be performed within the constraints
		 * of the current cpuset / memory policy requirements.
		 * We may trigger various forms of reclaim on the allowed
		 * set and go into memory reserves if necessary.
		 */
		if (local_flags & __GFP_WAIT)
			local_irq_enable();
		kmem_flagcheck(cache, flags);
		obj = kmem_getpages(cache, local_flags, numa_node_id());
		if (local_flags & __GFP_WAIT)
			local_irq_disable();
		if (obj) {
			/*
			 * Insert into the appropriate per node queues
			 */
			nid = page_to_nid(virt_to_page(obj));
			if (cache_grow(cache, flags, nid, obj)) {
				obj = ____cache_alloc_node(cache,
					flags | GFP_THISNODE, nid);
				if (!obj)
					/*
					 * Another processor may allocate the
					 * objects in the slab since we are
					 * not holding any locks.
					 */
					goto retry;
			} else {
				/* cache_grow already freed obj */
				obj = NULL;
			}
		}
	}
	return obj;
}

/*
 * A interface to enable slab creation on nodeid
 */
static void *____cache_alloc_node(struct kmem_cache *cachep, gfp_t flags,
				int nodeid)
{
	struct list_head *entry;
	struct slab *slabp;
	struct kmem_list3 *l3;
	void *obj;
	int x;

	l3 = cachep->nodelists[nodeid];
	BUG_ON(!l3);

retry:
	check_irq_off();
	spin_lock(&l3->list_lock);
	entry = l3->slabs_partial.next;
	if (entry == &l3->slabs_partial) {
		l3->free_touched = 1;
		entry = l3->slabs_free.next;
		if (entry == &l3->slabs_free)
			goto must_grow;
	}

	slabp = list_entry(entry, struct slab, list);
	check_spinlock_acquired_node(cachep, nodeid);
	check_slabp(cachep, slabp);

	STATS_INC_NODEALLOCS(cachep);
	STATS_INC_ACTIVE(cachep);
	STATS_SET_HIGH(cachep);

	BUG_ON(slabp->inuse == cachep->num);

	obj = slab_get_obj(cachep, slabp, nodeid);
	check_slabp(cachep, slabp);
	l3->free_objects--;
	/* move slabp to correct slabp list: */
	list_del(&slabp->list);

	if (slabp->free == BUFCTL_END)
		list_add(&slabp->list, &l3->slabs_full);
	else
		list_add(&slabp->list, &l3->slabs_partial);

	spin_unlock(&l3->list_lock);
	goto done;

must_grow:
	spin_unlock(&l3->list_lock);
	x = cache_grow(cachep, flags | GFP_THISNODE, nodeid, NULL);
	if (x)
		goto retry;

	return fallback_alloc(cachep, flags);

done:
	return obj;
}

/**
 * kmem_cache_alloc_node - Allocate an object on the specified node
 * @cachep: The cache to allocate from.
 * @flags: See kmalloc().
 * @nodeid: node number of the target node.
 * @caller: return address of caller, used for debug information
 *
 * Identical to kmem_cache_alloc but it will allocate memory on the given
 * node, which can improve the performance for cpu bound structures.
 *
 * Fallback to other node is possible if __GFP_THISNODE is not set.
 */
static __always_inline void *
__cache_alloc_node(struct kmem_cache *cachep, gfp_t flags, int nodeid,
		   void *caller)
{
	unsigned long save_flags;
	void *ptr;

	flags &= gfp_allowed_mask;

	lockdep_trace_alloc(flags);

	if (slab_should_failslab(cachep, flags))
		return NULL;

	cache_alloc_debugcheck_before(cachep, flags);
	local_irq_save(save_flags);

	if (unlikely(nodeid == -1))
		nodeid = numa_node_id();

	if (unlikely(!cachep->nodelists[nodeid])) {
		/* Node not bootstrapped yet */
		ptr = fallback_alloc(cachep, flags);
		goto out;
	}

	if (nodeid == numa_node_id()) {
		/*
		 * Use the locally cached objects if possible.
		 * However ____cache_alloc does not allow fallback
		 * to other nodes. It may fail while we still have
		 * objects on other nodes available.
		 */
		ptr = ____cache_alloc(cachep, flags);
		if (ptr)
			goto out;
	}
	/* ___cache_alloc_node can fall back to other nodes */
	ptr = ____cache_alloc_node(cachep, flags, nodeid);
  out:
	local_irq_restore(save_flags);
	ptr = cache_alloc_debugcheck_after(cachep, flags, ptr, caller);
	kmemleak_alloc_recursive(ptr, obj_size(cachep), 1, cachep->flags,
				 flags);

	if (likely(ptr))
		kmemcheck_slab_alloc(cachep, flags, ptr, obj_size(cachep));

	if (unlikely((flags & __GFP_ZERO) && ptr))
		memset(ptr, 0, obj_size(cachep));

	return ptr;
}

static __always_inline void *
__do_cache_alloc(struct kmem_cache *cache, gfp_t flags)
{
	void *objp;

	if (unlikely(current->flags & (PF_SPREAD_SLAB | PF_MEMPOLICY))) {
		objp = alternate_node_alloc(cache, flags);
		if (objp)
			goto out;
	}
	objp = ____cache_alloc(cache, flags);

	/*
	 * We may just have run out of memory on the local node.
	 * ____cache_alloc_node() knows how to locate memory on other nodes
	 */
 	if (!objp)
 		objp = ____cache_alloc_node(cache, flags, numa_node_id());

  out:
	return objp;
}
#else

static __always_inline void *
__do_cache_alloc(struct kmem_cache *cachep, gfp_t flags)
{
	return ____cache_alloc(cachep, flags);
}

#endif /* CONFIG_NUMA */

static __always_inline void *
__cache_alloc(struct kmem_cache *cachep, gfp_t flags, void *caller)
{
	unsigned long save_flags;
	void *objp;

	flags &= gfp_allowed_mask;

	lockdep_trace_alloc(flags);

	if (slab_should_failslab(cachep, flags))
		return NULL;

	cache_alloc_debugcheck_before(cachep, flags);
	local_irq_save(save_flags);
	objp = __do_cache_alloc(cachep, flags);
	local_irq_restore(save_flags);
	objp = cache_alloc_debugcheck_after(cachep, flags, objp, caller);
	kmemleak_alloc_recursive(objp, obj_size(cachep), 1, cachep->flags,
				 flags);
	prefetchw(objp);

	if (likely(objp))
		kmemcheck_slab_alloc(cachep, flags, objp, obj_size(cachep));

	if (unlikely((flags & __GFP_ZERO) && objp))
		memset(objp, 0, obj_size(cachep));

	return objp;
}

/*
 * Caller needs to acquire correct kmem_list's list_lock
 */
static void free_block(struct kmem_cache *cachep, void **objpp, int nr_objects,
		       int node)
{
	int i;
	struct kmem_list3 *l3;

	for (i = 0; i < nr_objects; i++) {
		void *objp = objpp[i];
		struct slab *slabp;

		slabp = virt_to_slab(objp);
		l3 = cachep->nodelists[node];
		list_del(&slabp->list);
		check_spinlock_acquired_node(cachep, node);
		check_slabp(cachep, slabp);
		slab_put_obj(cachep, slabp, objp, node);
		STATS_DEC_ACTIVE(cachep);
		l3->free_objects++;
		check_slabp(cachep, slabp);

		/* fixup slab chains */
		if (slabp->inuse == 0) {
			if (l3->free_objects > l3->free_limit) {
				l3->free_objects -= cachep->num;
				/* No need to drop any previously held
				 * lock here, even if we have a off-slab slab
				 * descriptor it is guaranteed to come from
				 * a different cache, refer to comments before
				 * alloc_slabmgmt.
				 */
				slab_destroy(cachep, slabp);
			} else {
				list_add(&slabp->list, &l3->slabs_free);
			}
		} else {
			/* Unconditionally move a slab to the end of the
			 * partial list on free - maximum time for the
			 * other objects to be freed, too.
			 */
			list_add_tail(&slabp->list, &l3->slabs_partial);
		}
	}
}

static void cache_flusharray(struct kmem_cache *cachep, struct array_cache *ac)
{
	int batchcount;
	struct kmem_list3 *l3;
	int node = numa_node_id();

	batchcount = ac->batchcount;
#if DEBUG
	BUG_ON(!batchcount || batchcount > ac->avail);
#endif
	check_irq_off();
	l3 = cachep->nodelists[node];
	spin_lock(&l3->list_lock);
	if (l3->shared) {
		struct array_cache *shared_array = l3->shared;
		int max = shared_array->limit - shared_array->avail;
		if (max) {
			if (batchcount > max)
				batchcount = max;
			memcpy(&(shared_array->entry[shared_array->avail]),
			       ac->entry, sizeof(void *) * batchcount);
			shared_array->avail += batchcount;
			goto free_done;
		}
	}

	free_block(cachep, ac->entry, batchcount, node);
free_done:
#if STATS
	{
		int i = 0;
		struct list_head *p;

		p = l3->slabs_free.next;
		while (p != &(l3->slabs_free)) {
			struct slab *slabp;

			slabp = list_entry(p, struct slab, list);
			BUG_ON(slabp->inuse);

			i++;
			p = p->next;
		}
		STATS_SET_FREEABLE(cachep, i);
	}
#endif
	spin_unlock(&l3->list_lock);
	ac->avail -= batchcount;
	memmove(ac->entry, &(ac->entry[batchcount]), sizeof(void *)*ac->avail);
}

/*
 * Release an obj back to its cache. If the obj has a constructed state, it must
 * be in this state _before_ it is released.  Called with disabled ints.
 */
static inline void __cache_free(struct kmem_cache *cachep, void *objp)
{
	struct array_cache *ac = cpu_cache_get(cachep);

	check_irq_off();
	kmemleak_free_recursive(objp, cachep->flags);
	objp = cache_free_debugcheck(cachep, objp, __builtin_return_address(0));

	kmemcheck_slab_free(cachep, objp, obj_size(cachep));

	/*
	 * Skip calling cache_free_alien() when the platform is not numa.
	 * This will avoid cache misses that happen while accessing slabp (which
	 * is per page memory  reference) to get nodeid. Instead use a global
	 * variable to skip the call, which is mostly likely to be present in
	 * the cache.
	 */
	if (nr_online_nodes > 1 && cache_free_alien(cachep, objp))
		return;

	if (likely(ac->avail < ac->limit)) {
		STATS_INC_FREEHIT(cachep);
		ac->entry[ac->avail++] = objp;
		return;
	} else {
		STATS_INC_FREEMISS(cachep);
		cache_flusharray(cachep, ac);
		ac->entry[ac->avail++] = objp;
	}
}

/**
 * kmem_cache_alloc - Allocate an object
 * @cachep: The cache to allocate from.
 * @flags: See kmalloc().
 *
 * Allocate an object from this cache.  The flags are only relevant
 * if the cache has no available objects.
 */
void *kmem_cache_alloc(struct kmem_cache *cachep, gfp_t flags)
{
	void *ret = __cache_alloc(cachep, flags, __builtin_return_address(0));

	trace_kmem_cache_alloc(_RET_IP_, ret,
			       obj_size(cachep), cachep->buffer_size, flags);

	return ret;
}
EXPORT_SYMBOL(kmem_cache_alloc);

#ifdef CONFIG_KMEMTRACE
void *kmem_cache_alloc_notrace(struct kmem_cache *cachep, gfp_t flags)
{
	return __cache_alloc(cachep, flags, __builtin_return_address(0));
}
EXPORT_SYMBOL(kmem_cache_alloc_notrace);
#endif

/**
 * kmem_ptr_validate - check if an untrusted pointer might be a slab entry.
 * @cachep: the cache we're checking against
 * @ptr: pointer to validate
 *
 * This verifies that the untrusted pointer looks sane;
 * it is _not_ a guarantee that the pointer is actually
 * part of the slab cache in question, but it at least
 * validates that the pointer can be dereferenced and
 * looks half-way sane.
 *
 * Currently only used for dentry validation.
 */
int kmem_ptr_validate(struct kmem_cache *cachep, const void *ptr)
{
	unsigned long addr = (unsigned long)ptr;
	unsigned long min_addr = PAGE_OFFSET;
	unsigned long align_mask = BYTES_PER_WORD - 1;
	unsigned long size = cachep->buffer_size;
	struct page *page;

	if (unlikely(addr < min_addr))
		goto out;
	if (unlikely(addr > (unsigned long)high_memory - size))
		goto out;
	if (unlikely(addr & align_mask))
		goto out;
	if (unlikely(!kern_addr_valid(addr)))
		goto out;
	if (unlikely(!kern_addr_valid(addr + size - 1)))
		goto out;
	page = virt_to_page(ptr);
	if (unlikely(!PageSlab(page)))
		goto out;
	if (unlikely(page_get_cache(page) != cachep))
		goto out;
	return 1;
out:
	return 0;
}

#ifdef CONFIG_NUMA
void *kmem_cache_alloc_node(struct kmem_cache *cachep, gfp_t flags, int nodeid)
{
	void *ret = __cache_alloc_node(cachep, flags, nodeid,
				       __builtin_return_address(0));

	trace_kmem_cache_alloc_node(_RET_IP_, ret,
				    obj_size(cachep), cachep->buffer_size,
				    flags, nodeid);

	return ret;
}
EXPORT_SYMBOL(kmem_cache_alloc_node);

#ifdef CONFIG_KMEMTRACE
void *kmem_cache_alloc_node_notrace(struct kmem_cache *cachep,
				    gfp_t flags,
				    int nodeid)
{
	return __cache_alloc_node(cachep, flags, nodeid,
				  __builtin_return_address(0));
}
EXPORT_SYMBOL(kmem_cache_alloc_node_notrace);
#endif

static __always_inline void *
__do_kmalloc_node(size_t size, gfp_t flags, int node, void *caller)
{
	struct kmem_cache *cachep;
	void *ret;

	cachep = kmem_find_general_cachep(size, flags);
	if (unlikely(ZERO_OR_NULL_PTR(cachep)))
		return cachep;
	ret = kmem_cache_alloc_node_notrace(cachep, flags, node);

	trace_kmalloc_node((unsigned long) caller, ret,
			   size, cachep->buffer_size, flags, node);

	return ret;
}

#if defined(CONFIG_DEBUG_SLAB) || defined(CONFIG_KMEMTRACE)
void *__kmalloc_node(size_t size, gfp_t flags, int node)
{
	return __do_kmalloc_node(size, flags, node,
			__builtin_return_address(0));
}
EXPORT_SYMBOL(__kmalloc_node);

void *__kmalloc_node_track_caller(size_t size, gfp_t flags,
		int node, unsigned long caller)
{
	return __do_kmalloc_node(size, flags, node, (void *)caller);
}
EXPORT_SYMBOL(__kmalloc_node_track_caller);
#else
void *__kmalloc_node(size_t size, gfp_t flags, int node)
{
	return __do_kmalloc_node(size, flags, node, NULL);
}
EXPORT_SYMBOL(__kmalloc_node);
#endif /* CONFIG_DEBUG_SLAB */
#endif /* CONFIG_NUMA */

/**
 * __do_kmalloc - allocate memory
 * @size: how many bytes of memory are required.
 * @flags: the type of memory to allocate (see kmalloc).
 * @caller: function caller for debug tracking of the caller
 */
static __always_inline void *__do_kmalloc(size_t size, gfp_t flags,
					  void *caller)
{
	struct kmem_cache *cachep;
	void *ret;

	/* If you want to save a few bytes .text space: replace
	 * __ with kmem_.
	 * Then kmalloc uses the uninlined functions instead of the inline
	 * functions.
	 */
	cachep = __find_general_cachep(size, flags);
	if (unlikely(ZERO_OR_NULL_PTR(cachep)))
		return cachep;
	ret = __cache_alloc(cachep, flags, caller);

	trace_kmalloc((unsigned long) caller, ret,
		      size, cachep->buffer_size, flags);

	return ret;
}


#if defined(CONFIG_DEBUG_SLAB) || defined(CONFIG_KMEMTRACE)
void *__kmalloc(size_t size, gfp_t flags)
{
	return __do_kmalloc(size, flags, __builtin_return_address(0));
}
EXPORT_SYMBOL(__kmalloc);

void *__kmalloc_track_caller(size_t size, gfp_t flags, unsigned long caller)
{
	return __do_kmalloc(size, flags, (void *)caller);
}
EXPORT_SYMBOL(__kmalloc_track_caller);

#else
void *__kmalloc(size_t size, gfp_t flags)
{
	return __do_kmalloc(size, flags, NULL);
}
EXPORT_SYMBOL(__kmalloc);
#endif

/**
 * kmem_cache_free - Deallocate an object
 * @cachep: The cache the allocation was from.
 * @objp: The previously allocated object.
 *
 * Free an object which was previously allocated from this
 * cache.
 */
void kmem_cache_free(struct kmem_cache *cachep, void *objp)
{
	unsigned long flags;

	local_irq_save(flags);
	debug_check_no_locks_freed(objp, obj_size(cachep));
	if (!(cachep->flags & SLAB_DEBUG_OBJECTS))
		debug_check_no_obj_freed(objp, obj_size(cachep));
	__cache_free(cachep, objp);
	local_irq_restore(flags);

	trace_kmem_cache_free(_RET_IP_, objp);
}
EXPORT_SYMBOL(kmem_cache_free);

/**
 * kfree - free previously allocated memory
 * @objp: pointer returned by kmalloc.
 *
 * If @objp is NULL, no operation is performed.
 *
 * Don't free memory not originally allocated by kmalloc()
 * or you will run into trouble.
 */
void kfree(const void *objp)
{
	struct kmem_cache *c;
	unsigned long flags;

	trace_kfree(_RET_IP_, objp);

	if (unlikely(ZERO_OR_NULL_PTR(objp)))
		return;
	local_irq_save(flags);
	kfree_debugcheck(objp);
	c = virt_to_cache(objp);
	debug_check_no_locks_freed(objp, obj_size(c));
	debug_check_no_obj_freed(objp, obj_size(c));
	__cache_free(c, (void *)objp);
	local_irq_restore(flags);
}
EXPORT_SYMBOL(kfree);

unsigned int kmem_cache_size(struct kmem_cache *cachep)
{
	return obj_size(cachep);
}
EXPORT_SYMBOL(kmem_cache_size);

const char *kmem_cache_name(struct kmem_cache *cachep)
{
	return cachep->name;
}
EXPORT_SYMBOL_GPL(kmem_cache_name);

/*
 * This initializes kmem_list3 or resizes various caches for all nodes.
 */
static int alloc_kmemlist(struct kmem_cache *cachep, gfp_t gfp)
{
	int node;
	struct kmem_list3 *l3;
	struct array_cache *new_shared;
	struct array_cache **new_alien = NULL;

	for_each_online_node(node) {

                if (use_alien_caches) {
                        new_alien = alloc_alien_cache(node, cachep->limit, gfp);
                        if (!new_alien)
                                goto fail;
                }

		new_shared = NULL;
		if (cachep->shared) {
			new_shared = alloc_arraycache(node,
				cachep->shared*cachep->batchcount,
					0xbaadf00d, gfp);
			if (!new_shared) {
				free_alien_cache(new_alien);
				goto fail;
			}
		}

		l3 = cachep->nodelists[node];
		if (l3) {
			struct array_cache *shared = l3->shared;

			spin_lock_irq(&l3->list_lock);

			if (shared)b.c
	free_b/mm/(cachep, ritten->entry,by Ma		.
 * (maavail, nodeslab.c

 * ritten = new_ritten;b.c
 * W!
 * alien) {by Ma Andrea A) + somrea Anup -	 cleanup, = NULLnup -}cache_derk Helimit = (1 + nr_cpus_* km(* kme) *extd.d 1996/->batchcount +  make thnumnup -spin_un/mm/_irq/*
 * linux/mm/sla			krk HWritten 02 Mark Herea A_ 1996( cleanup,
 *
 *continue02 Mr-cpl3 = kmallocpraul
sizeof(struct kmem_linu3), gfp
 * kmem_ - 1999 Arcangel An implementation of the Slab Anfred  some clea
 *
 *goto fail as de
		ernals: Th_init(l3ers b
 * next_reap = jiffies + REAPTIMEOUT_LIST3 +diffe((unsigned long) 1996/) %Kernel Memory AlloAllocatostroy() + some cleanup 
 *
 * Major cleanup, dif arrays
 *	(c) 2000 Manfred Spraul
 *
 * Cleanup, make the head arrays unconditional, unconditodelinus[y_ca] = lente}
	return 0;

tle :
- 1999 unconditext.sts rcange/* C1996 is not active yet. Roll back what we did */
		y_ca--rs bwhile 
 *
  >= 0rcangel * We, dentry_cache, bufferrcangeli
  =s uncondit_cache, bufferlab.c
13-1019 at: USENI
 *
 *ia
 *	Pub: Prentic Andrea Ared onlconstruclabs ane, dentry_cache, buffer_helogic, per-cpulways cont} vm_area_str-ENOMEM;
}

NIX Intccupdate_NIX Int{
	NIX Internal 1996 * 1996/; memory tarray(GFP_DMAnew[NR_CPUS];
};cachatic void do_ only suplocal(y typ*info)
e memory t only support onf yo = w
 *HIGHMEM,
 * normal). Ifoldlab.cheon for_off(labsol() +cpu(GFP_D_get908-->Sun Miclab. full slabs->* nor[smp_processor_id()r_he full you  partial slabs
 * ;th 0 frslabs with no allocated = ation}

/* Always called with thes unco_chain_mutex hel) andl memorintpe, tunered ntatioemory type (GFP_DMA, GFP_, empt*	(c)nextd.empte head arrkmem_critten New _tNew  cache for that memory type.
 *
;
oy() iwith 0 bed zn outin;
 *	Uf yoe New labs 1999vent
	_free.
 *
 * Each
	for_each_onlibs or (ircangejects
 *
is exn outl* norntatio3 grtopraul
i),cache_destro		CAN CRASH ifoncurreent allocmost all Vahaliaor (i--; id eac; i-- by Ma3-101908- of the eN 0-13-101908- cachefree.
 *
 * Eachm_cache_ full slabscts.
 *
 e haon shortu hee, then must crea, te a ne)new, 1 withthe slabs an sort make the head arra= CAN CRASH erations.
 *	(c) 20*	(c)erations.
 stroy() +e cleanuhas a short per-cpu head arraHMEM,
 * normal). Ifcced in  the global then 1/2nchro by Mllocator as prepation for N.
 *
 * This meansinto that arra]A
 * 	(c) 2002 Mrk Hemment, 1996/97nchromarkhe@nslab nevk)
 *
 into that arra cachpreparation for Nt any locking.
 *  Several members in struct kmemnfred ors an, vm_a.
 * The headrea_strn outlernalinu, 1996/97oncurrthen nCations comem these slabs,
 * otheraw allowise from emptenabls or new slabs are allocated.
 *
 * kmallocate from otectrrche_desache_d-
 * it's c/*
	 * The head * nor serves three purposes:r-cpu- create a LIFO ordering, i.e. rea_strobject00 -a
 * rom the-warmn the creducee fronumber of prep/mm/ operations.her notes from the original linkonwiistation:
 *
  one froslab andr-cpu  bufctl e slas:tch
 * - markhe
 *ch.
 heaper* 11 A sla origiache_cguess to we shoustanuto-labs as described byr-cpuBonwick* 11 /rent a make theuffer_n;
  > 131072s.
 *	(c) 201;
	else  never happen inside an inPAGE_SIZEt (kmem_cach8_create(),
 *	kmem_cache_shrink() 1024t (kmem_cach24_create(),
 *	kmem_cache_shrink() 256t (kmem_cach5che.  Th (kmem_cache2uct) per-cpuCPU bound tasks (e.g. network routing) can exhibitre am <shar-cpun out:
 *
 behaviour: Mostom>
 * *	Thonere a, mlokkrk He mutex 'car-cpuon anotherre a. Fore frsrom ses, al <fficientrom
 *  passing betweenr-cpued Sthey eal sary. Thi systprovidcache a-
 * it * norach e * norr-cpureplacesin, whic's magazine laymutex'.
On unirtial sla, it's func
 *
ally equivalallo(but lesse slab all)r-cputo a largerwho wrach us disd wiown ldefaulth
 *	canstroy() +0rrent aer happen inside an <= and kmem_ &&The _possi with as() cans.
 stroy() +8;

#if DEBUGShai FultWcomedebugg nodted wid,lude	< CAN CRASH  l patto exal sivelywick r-cpuperiodss comenclude	<l crea emperrupts. L	(c)  froe head arr
 *	can nev*	(c) > 3pt (kmem_cach32;
#endif
	err =ty slabs or new sl 1996/97ho wrot/kallsy+ 1) / 2ote anot concurrent aerrs.
 printk(KERN_ERR "ted with a per-ttle ed  in %s, Hemnux/d.\n"next <linux unconditame, -raced local inHemmethen 
 * Draioph@tch
 * if hit@ontecte any elemen hisak nod frol3le.hk onlmeml#incems.
 *	EacNotom taotifieinclistlude	alsohas t * his ee and (GFP_D#incif dude	nd fre()theyused*	The glo of partial,
*	cay typelinux/recipabs are allocated.
 *
 * kmNIX Internals: Th *l3nextdclude	<a* normal). Ifackmem_cforcekmem_c* kmeto Mark tork H*	Sh 1999ac || !achey
 *	s.
 *
 * Errent ahe_ctouchedclud!ude	<rcangeB_RED_ZONE &linux/} eate(funct called withouUMA
 * 	(c) 2002 M SLAB_REreate(cangelDEBUG	 =lude	< ? he_create :LAB_REnux/modu4e.h>5nup - 1991 to co>ts for /pr by Ma1 to coll).
 *
 * Sodule.h>
 *
 * An imment, 1996/97he_cer chanDEBUG	Frontiers b *		 ical p-= DEBUG	- 			memmoveAB_REer chan&f CONFIG_D[DEBUG	])nextd.n;
 *	Utionall.com
 *
 * STly LIFO preparation for NUMA
 * 	(c) 2002 }sk.h>

#incm thes Obje- Reclaim memory fromdefinesdebu @w: 
 *	Sng theptor
 
#incis pree
/* S
 *	queue/eventd eveif

ew secondn't thP ideas:t th- clea *
 *#inc-@calhouldnlinuxt nodCPU't thotesa_strrk Hd wi pag000 oux/rtmde	<to co#endif
pooldebut thIfd whcan are squirom the>
#ince sla s,
 * then just gall uCED_we'll tryt thagmall	The glsts  iton:
 *
debugol memory typefine	FORCde	<linu
 *	ry type.
w cache for type (GFP_DMAsearch_HIGHMEM,
 sm/cacheflush.che_des, and=The apraulbs
 *HIGHMEM,
 deionsd_
 *	S*
 *	Sposs_caches and n( hea1 for ks,
 *_tryment, The pse slabs,
 *)s.
 /* Gally, . Setupache_line_size() ali and aa litoutnt llinuxs a shortrkhe@(TES_PER, nment of a 6,_lineTATS	-he spinlock opertegerde	<* We	<linutakom theinclude	if absolut/proems.
 *	E cac wab a * have establisNE & comereasoed wi cer#inctylinuxf ARCH a min do somebe in if.h>
#iude	was ob#inced.f ARCnd ajectsTES_PER* This means, that you Objimplemue may dismust 
		s.h>
#includeffers. Incare acoups:
 *  _MINALI), 0
 * kmem_cacKMALLOC_T *	Moch.
racychaicks #incipty eey arematterf ARCHice a skipnc.comit d or s cactwiceisalignmenster,ime_aftercatedr: An Obj,t-Cachininteg * Notsts udes cator: An Object-Caching Kernel Memory Alloudes ARCH_KMALLOC_MINALIGN.
 * at: USENInable this flagnux/karrays
 D_ZONE  by MSLAB_RED_ZONE | faster,reate(slab cnclureit's cem_cacSLABs.h>
#			 re di_MINALIGN.
 *(SLAB_RED_nux/modd onl5 *ults even fum -ule.h>(NT | SLAB_PANIC thout 	STATS_ADD_erneEDTORE_USER 			 SRCED_DEsts :res.ond_resONE nto k} the spinlock opera than rationgnment of a 64-bit i obje An Objpraul
);
ou| SL/*_KMAy, tche_line_size() al>
#incONE ulenteed
 * alignmork,Shobnd_-Cachin_relamall(ernel MemoryCPUCthou}init.def CONFIG_SLABINFOial memory typ
#inc_obalw
 *_b paerde	<linuseq_fuous*m>

/*per-cpuOutpucludemat version, so ah>
#isge lo cacchange ilude	*
 * out _too_ mde	<complm al * 11 /nit.hAB_ME
	_bufputs(m, "ACK)
#en -or linki: 2.1 (l memstics)\n")nux/slab ct page" for locating the cache &
 *ct beloncpu.h>ct page" for # h>
#<linux/the n< small_objs> <e	<lobjectsobjeize> "f AR"ot aperobal> <

#ifain when bel (one int), bu : tued wis <*	(c)> <e head arr> <lude	<factorufctls are used. The lobaldataumber of obalects a s
 * For lude	<k)
 *ufctles on "struct page" for  : globall methe st@calso> <maxobjectsgrown> < Objedhe) can cing.hy for ng long)cts odeis only fremote			 ly frea Aoverflowufctls are used. The lrrups, asn outhize on outmisly f			 whose 			  * isto be small (one inc for'\n'LAB_DEl memory typ*s_start
 * kmem_bufctl_t:
, loff_t *pos>

/*efine Bctl UFCTudes than CREATE_MASK	(SLAB_HWCACHE_Ant alle.h>
#incRACK)
#endif

/*
mludesgned lo_buflinuxgned iable some debuUFCTL6.
 */

typedef unsiLIGNint kmem_bufctl_t;
#ddef un. Infine BUFCTL_ENDACTIVE	(((kmem_bbufctdisable some debufine	SLAB_LIMIT	(((kmsigno and FORCU))-3)

/*
 * struct>

/* define CREATE_MASK	(SLAB_HWCACHE_
 */

type you _showated
 * for a slab, or allocated emory type (GFP_DMA, GFP_h enastis valuinclude	<asm/ca 1996bug feaHIGHMEM,
 obal *obal_HIGJeff Bonwick e smallslab *s_mem;		/* inclu a slab *s_mem;		/* including co
 * Ffaster,fset */
	unsigned 
 * FAB_DEBslab * hi=ablelude	<_ical plinux/constb
 *r *h>
#strub_rcuing.h>nitializas want t_WORD aligned.
 * Some arc
	ding colour
 * strtl_t free
 * strs a short per-cpraul
 *
 *ALIGNjects.
 *
 * This means, that  by Uresh d destructors res.
 */
#define ARcode (especially in the critical  (kmecreasing this valueoid ,  in t
 * F_fullncluseatures
 * Wroid ->inuse !ts.
 *
 * Tum& SLAing.h by Maestroy o locaucture acd arrr.h>
ng.h"SLAB_POture to
 *+t's still at  memory has f we
++ as descrusual locking.  We can lock the strucparti addo
 * stabilize it and check =t's still at the given address, only if we
 ck migh check  be sure that the memo 1999it and check g the address, then rcu_read_unlock a/ter
 * taking the spinlock wry has not beenit and checkreused for some
 * other kind of object (which our subsystem's loD_ZONEo
 * stabilize it and check he given address, only if we
 *reeassume struct slab_rcu can ovtl_t free* other ki	unsigned sho+headrrays
 gned sho approauctor is usal, peid;
};

/*
_alloc
mon.co.uk)
 *t theG	1
#else
#define	DEBUG		0
#define	Snge for kme+= objs active range fto
 * age for kme#definenditional * Wuce the d-uding colour it'	unsigned shohe given addrs, only i	unsigned sho be sure that the m
	limitts.
 *
 * T *
 * memtracn addr
#include	<linux/it a:for the%shat th: %snclu limitring.hut thct pa#incf for %-17s %6lugnment ou %4ay_cdlude	<lih>
#inry has not ,gned int ,/uaccess.n inside anude	<liootprint.
 ,000 << bootstragfpntatithoutthe proper
			 e limit is t_cacheay_calude	<lit be read wite entries.
AN CRASH ifocal interruptctls are arrays anymore off-slanment of arrlrays are d for some
 g
			 ay_cachst operationes, is this 56{	eger.cheflueralmbers	b */
	kmem_bufchighnt touched;r al_markom imem;		/* includcalsoft's still at _m>
 *	Alok the mem;		/* inclubjectist_head slbject partial list firswhen iist_head slwhen i partial list first loclist_head sl;
	uns partial list firsmax Purp a mits.
 *
 * int free_lim partial list firsm DMAruct list_head sla cache colr_next;	/* Per-node cac Thiscts.
 *
 * Thisy_cach partial list firsan be rahe *shared;	/* shan be rak operCPUCACHE_ENTRIES	 serious, a%7ent of ar5ent 4lu \d onlithouing */
	int free_to",truct l,or al,st, benextd.when itring.hs, int free_lim
 * kmche colnextd./* shared ,ct array_c lockin/ NUMA;

/*
 * Thangeem_list3 {
	struct hc) 20atomic	FORd  The per-
struct rom iem_list3 {
	struct  * ikmem_list3 __initdata initkme * iist3[NUM_INIT_LISTSess thakmem_list3 __initdata iness thaAX_NUMNODES
#define	SIZefine	CACHE_CACHE 0
#define	ruct kmeut thees */
	unsigned logeneral arraycache_inie_init {
		strustruct 		/* up * iAB_DEBcachet kmem_list	}E<<3), but greater than 256.area_structk.h>
#incACK)
#endoCED_size(), __aat genon:
es /rtia/ACK)
#en * Enfo's are layCACHE* footpr-
	spmutexum-ry has-objed);
totalt be compcator t cacs function mustf we
completelyassed to
ncti

#if-x(BYit ad);
+ furametevalue *	ThSMP
#ifndcomelab an obj>
#incluebugo*/

typeuct slted
 * for - markhe
 *che *cachep,=ALIG.gned faulbufctl,
	out o __baLIGNe(volloc __bad_opbuiltho The three,pecia#defocatMAX SLAB_NOL_WRITE 128TS		0
#dACK)
#endwrite - Tun nodD, __aglobal c>
 *	A file @ctl_: uheckic _ @n insi:l_dirin ifekmallod arr: f-slalengthundefpposzes.h>
#und/
s cac_uroff;turn i; \
de	<linuctl_t:ctl_,nline islab___	__ba*ad_sizeude	<linux/ic int RASH iflab
 *
 *FCTL_ENDslab_kbuf[e CACHE(x) \
	if (s+ 1], *tm_HIGt, who wrotric caches..UG
# defrd peremory type (GFP_DMA, GFP_HIx/swap. arra>ne CACHE(x) \
	if (s.
 *
 * EachINVA SLABent->py_
/* f(siz(&izeo,f(struct))
#deintegfree.
 *
FAULT;
	izeof(struct kmem_list3s ex'\0'(&patm_constrchr(HEAD(&' _cpuc 1999tmpHEAD(&parent->slabs_ptic  = NULL;
pare* oth * WrMINAf(tmpanym%dt->fre"ock  from &ric caches..&ritten  it' a ke(&parent->slabs_ine NFihai@t for theiThe gl kmallofShouldn'ignme~0U))-0)
#define BUFCTL_FREE	(((krche *t->slabs_pnd of object (which  1996/97able some debug featuresithin trcmptains multon inizeo) stabilize *	(c) < 1m_cae head arraKE_LISf thate head arra>clude	m_calude	<l<ach slab nodelis_HWCA, smaller co_LIST((cude	<linux/sysctl.h>
#include	<f that  the nu3_init(struct kmem_ \
	MAKE_LIST(GFP_de	<EL cacher-cpubreact kmcache_ define CREATE_MASK	(SLAB_HWCACHE_Aartiodel eachp, lihe *s may nogned loist3 e chained into he *cachepd buIX Inti, and*MIT	1nclude	<a#define INanages the objs TCHREe INDE&define BATC are chained line int indectl_f(const sizertiaRACK)
#end- markhe
 *externTCHR		slab wne BATCHRe(vo __icontaeq3 __ie(voi; \
contain lots i; \
e(vollseek * whichabs.
bjectswithe* which cEOUT_i = 0;
UG_OBJECTS | >
#in SLAB_LEAK */

typedef unleaksigned int kmem_bufctl_t;
#define BUFCTL_END;					\
		list_splice(&(cachep->nodTIVE	(((kmem_bufctl_t)(~0U))-2)
#define	SLAB_LIMIT	iper-c intoadd_catior	Jeff Bonwick  *n, INC_GROWN(x)	(v>

/*INC_GROWN(x)	(( void kme
	spin_lvs.
 *
 * Eae_cr*
 *n[1to a);
	n +
 *
 iguous)STATS	-_desth en/ *
 *INC_GROWN(x)	((q = px)		res.o approax)->= vTATS	- q[1]* othe(y))
#defineid);	
			(x)>high_mark*
 *)			\, (&(ptr)->s);
	qx)					e	ST-= i))

ly LIFO anarti++TS_S)->hn[0]s.
 *
 * Eanux/

#ifdefigh_m,  Frox)	(*_markn;
 *	UJeff Bonwick ( - (	STATS		CED_itionallythoutp[0s exv(x)  )	((ATS_Iy))
#defin
 */

typedef uhandl some
_INC_GROWN(x)	((x)-emory type (GFP_DMA,			\
		ifoff;
	v>

/* structche_destro */
st  ((xTATS_S() to honour; in thort no_cons->s_memarra< cdition i++_ALLeen s.
			 */
};
ALL_LISTS(defi_n is p(s)allo!= BUFCTL_ACTIV_HEADestructors arfor kmine	STATS_n, node_frees++)
#*dbgf(sizword(c, p)H_KMALo honour;	STATnning of mem how_symbolated
 * for a slab, omem;		/* includddres\
			REAPTIMEOUT_LIKALLSYMSSTATS_ADD_REAPEDoffseructiz
 * slab_modeid)[MODULE_NAME_LEN]ition [KSYMLOCED(x)	d(&parentlookupreemiss_attrs(	do { }ss p};

 &{ } while	STATSition )
	} ch slabthe proper
			 *s+%#lx/ATS_nition in{ } while (0ers by Ure	STATS_node_a*l3, int tofree);[%s]"S_ADD_REArom iFREEMISS(x)e small (one roper
			 *p"dditionall	do { } LGS_OFF_SLAB)

#STATS_Iree list: fully used, partial, fully free slabs.
 */
struct slab {
	struct list_head list;
	unsigned long colouroff;
	void *s_RD aligned.
 * Some archuct slab_rcu
 *
 * INC_GROWN(x)	((x = m->privatpinlo_DESTROY_B_destroy( 1999p.h>
#incflags & *HZ)
STORE_USER_free);
	parnux/swapT(x)	do { } while (0)
#RED_ZONETS_INC_FREEMISSine NOKed wh cachesit __al	(x)	((emory pages to be called via RCU.  This is useful if
 * we need to approach a kernel structure obliquely, from its address
 * obtained without the usual locking.  We can lock the structure to
 * _ERREABLE(x, i)	n...
 */
,lab wh Allocd of object (which our subsystem's lock might corrulocation.
 * cachep->obj_offset - BYTrations
 *
 * The limit is stored in td int touched;
	spinlock_			\
	} while many slInurresom thed_sizele (0ignmen define CREATE_MASK	(SLAB_HWCACHE_A	 } while (The caller 			\
* 4x)	((x)->node_frees++)
#,slabs_free, nodearger } while (TATS	- /* Too baded whBUG_Sr slyte tignmenss
 *					[BYT, diff~0U))-0)
#define BUFCTL_FREE	(((k array is strictly LIFO	*_INC_GROWN(x)	(()obj_size(strucRD lon *
 *3-10190ddress_cache *cachep)
{
	return cachep-/* Now m__alsunmenti */
try wiy onset;trie) and am->The c_cpm->e (0)
#area_structe (0 STATS_INCtomic_TS_SE)->a{ } while (0)
#define	S: %lu nition inn[2*i+3l cach)->freemiss)mg long *2bg_redztic int enable_cpuc detache(struct kways_inline int index_of(const size_t s

/*

{
	extern void __STATS_INC_Ae(void);

	if (__builtin_constant_p(size)) {
} while (0i = 0;FF_SLAB)

#defilags & SHREFILL_LIMIT	16
/*
 * Optimization question: C_ALLOCMISS(x)	do { caller and kmem_set(struct kmem_#defre;

	 strictly e worR_WORD    sieaps means less problags & Sers by Urere* stabilted
 * for a slab,olleile} while (_f-slhep->	do {he.h>
#inc/ ((x)	((x)->node_frees++)
# cacheobj_size(struct kmectl logic, pc uncache *cachm_area_strretfor unnessary
 * cpucache drain/refill cycles.
 *
d long lothe cpuarrays can contain d long lonbjects,
 * which could llabs.
 */
#define REAPTIMEOUT_CPUC	(2*HZ)
#d } wle (i = 0e small  sizeof(uns_The S i)		artiaThe Sly ty>

/*L;})
urrent( locating",S_IWUSR|S_IRUGO,logi,&cles.
 *
 * OTOH the cpues, isPTIMEOUT_LIST3	(4*HZ)

#if serword(cachep, ob_partialors"nableNULL; })

#endif>buffer_size)
#do be small ache(struct moAB_PAhe Sl*)NULL;})
#defo be smallTS		0
#dkr_siz- geotifieactual am arraof_MINALIG <linux	<linuxasuallfrom
 * undefonta: Poinclundef ARCDER_HI	1
orderin out mkmemnclu slab STROYy, tpartial;	/*
#ifnive--)
morC_MINALIcomplhane oquested.er unl(Dayal bel_div.to determs++)bjects fit into the undelab.
 */
#define, free	STATSb_breeck _ZONEaddific slse are ,_WORThe ougze();a sin oer into the slab.
 *s thhe Si
}

sspecifned  come froint slabcati't th the slab an* Usuuarante<linux/onta pREAKfndefaab.hidocator toretarisl cache
#define	rns aeiameteint sla()LAB_list;
	unsche co(), freeDER_HI	1
#uct p arebeB_DEBU duion  pageduIM_ACCOoed ford page_s/
ic int  the cuct sldef unonta>

/*3	(4ON(!);
	rsigned unlikely(ct km== ZEROkmem__PTATS_INC_FREEMISSslab_buffobjde an(virto thntatio);
	rODEFREXPORT_SYMBOL(r unl);
