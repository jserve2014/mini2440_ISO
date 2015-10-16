/*
 * Copyright (c) 2003-2006, Cluster File Systems, Inc, info@clusterfs.com
 * Written by Alex Tomas <alex@clusterfs.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public Licens
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-
 */


/*
 * mballoc.c contains the multiblocks allocation routines
 */

#include "mballoc.h"
#include <linux/debugfs.h>
#include <trace/events/ext4.h>

/*
 * MUSTDO:
 *   - test ext4_ext_search_left() and ext4_ext_search_right()
 *   - search for metadata in few groups
 *
 * TODO v4:
 *   - normalization should take into account whether file is still open
 *   - discard preallocations if no free space left (policy?)
 *   - don't normalize tails
 *   - quota
 *   - reservation for superuser
 *
 * TODO v3:
 *   - bitmap read-ahead (proposed by Oleg Drokin aka green)
 *   - track min/max extents in each group for better group selection
 *   - mb_mark_used() may allocate chunk right after splitting buddy
 *   - tree of groups sorted by number of free blocks
 *   - error handling
 */

/*
 * The allocation request involve request for multiple number of blocks
 * near to the goal(block) value specified.
 *
 * During initialization phase of the allocator we decide to use the
 * group preallocation or inode preallocation depending on the size of
 * the file. The size of the file could be the resulting file size we
 * would have after allocation, or the current file size, which ever
 * is larger. If the size is less than sbi->s_mb_stream_request we
 * select to use the group preallocation. The default value of
 * s_mb_stream_request is 16 blocks. This can also be tuned via
 * /sys/fs/ext4/<partition>/mb_stream_req. The value is represented in
 * terms of number of blocks.
 *
 * The main motivation for having small file use group preallocation is to
 * ensure that we have small files closer together on the disk.
 *
 * First stage the allocator looks at the inode prealloc list,
 * ext4_inode_info->i_prealloc_list, which contains list of prealloc
 * spaces for this particular inode. The inode prealloc space is
 * represented as:
 *
 * pa_lstart -> the logical start block for this prealloc space
 * pa_pstart -> the physical start block for this prealloc space
 * pa_len    -> lenght for this prealloc space
 * pa_free   ->  free space available in this prealloc space
 *
 * The inode preallocation space is used looking at the _logical_ start
 * block. If only the logical file block falls within the range of prealloc
 * space we will consume the particular prealloc space. This make sure that
 * that the we have contiguous physical blocks representing the file blocks
 *
 * The important thing to be noted in case of inode prealloc space is that
 * we don't modify the values associated to inode prealloc space except
 * pa_free.
 *
 * If we are not able to find blocks in the inode prealloc space and if we
 * have the group allocation flag set then we look at the locality group
 * prealloc space. These are per CPU prealloc list repreasented as
 *
 * ext4_sb_info.s_locality_groups[smp_processor_id()]
 *
 * The reason for having a per cpu locality group is to reduce the contention
 * between CPUs. It is possible to get scheduled at this point.
 *
 * The locality group prealloc space is used looking at whether we have
 * enough free space (pa_free) withing the prealloc space.
 *
 * If we can't allocate blocks via inode prealloc or/and locality group
 * prealloc then we look at the buddy cache. The buddy cache is represented
 * by ext4_sb_info.s_buddy_cache (struct inode) whose file offset gets
 * mapped to the buddy and bitmap information regarding different
 * groups. The buddy information is attached to buddy cache inode so that
 * we can access them through the page cache. The information regarding
 * each group is loaded via ext4_mb_load_buddy.  The information involve
 * block bitmap and buddy information. The information are stored in the
 * inode as:
 *
 *  {                        page                        }
 *  [ group 0 bitmap][ group 0 buddy] [group 1][ group 1]...
 *
 *
 * one block each for bitmap and buddy information.  So for each group we
 * take up 2 blocks. A page can contain blocks_per_page (PAGE_CACHE_SIZE /
 * blocksize) blocks.  So it can have information regarding groups_per_page
 * which is blocks_per_page/2
 *
 * The buddy cache inode is not stored on disk. The inode is thrown
 * away when the filesystem is unmounted.
 *
 * We look for count number of blocks in the buddy cache. If we were able
 * to locate that many free blocks we return with additional information
 * regarding rest of the contiguous physical block available
 *
 * Before allocating blocks via buddy cache we normalize the request
 * blocks. This ensure we ask for more blocks that we needed. The extra
 * blocks that we get after allocation is added to the respective prealloc
 * list. In case of inode preallocation we follow a list of heuristics
 * based on file size. This can be found in ext4_mb_normalize_request. If
 * we are doing a group prealloc we try to normalize the request to
 * sbi->s_mb_group_prealloc. Default value of s_mb_group_prealloc is
 * 512 blocks. This can be tuned via
 * /sys/fs/ext4/<partition/mb_group_prealloc. The value is represented in
 * terms of number of blocks. If we have mounted the file system with -O
 * stripe=<value> option the group prealloc request is normalized to the
 * stripe value (sbi->s_stripe)
 *
 * The regular allocator(using the buddy cache) supports few tunables.
 *
 * /sys/fs/ext4/<partition>/mb_min_to_scan
 * /sys/fs/ext4/<partition>/mb_max_to_scan
 * /sys/fs/ext4/<partition>/mb_order2_req
 *
 * The regular allocator uses buddy scan only if the request len is power of
 * 2 blocks and the order of allocation is >= sbi->s_mb_order2_reqs. The
 * value of s_mb_order2_reqs can be tuned via
 * /sys/fs/ext4/<partition>/mb_order2_req.  If the request len is equal to
 * stripe size (sbi->s_stripe), we try to search for contigous block in
 * stripe size. This should result in better allocation on RAID setups. If
 * not, we search in the specific group using bitmap for best extents. The
 * tunable min_to_scan and max_to_scan control the behaviour here.
 * min_to_scan indicate how long the mballoc __must__ look for a best
 * extent and max_to_scan indicates how long the mballoc __can__ look for a
 * best extent in the found extents. Searching for the blocks starts with
 * the group specified as the goal value in allocation context via
 * ac_g_ex. Each group is first checked based on the criteria whether it
 * can used for allocation. ext4_mb_good_group explains how the groups are
 * checked.
 *
 * Both the prealloc space are getting populated as above. So for the first
 * request we will hit the buddy cache which will result in this prealloc
 * space getting filled. The prealloc space is then later used for the
 * subsequent request.
 */

/*
 * mballoc operates on the following data:
 *  - on-disk bitmap
 *  - in-core buddy (actually includes buddy and bitmap)
 *  - preallocation descriptors (PAs)
 *
 * there are two types of preallocations:
 *  - inode
 *    assiged to specific inode and can be used for this inode only.
 *    it describes part of inode's space preallocated to specific
 *    physical blocks. any block from that preallocated can be used
 *    independent. the descriptor just tracks number of blocks left
 *    unused. so, before taking some block from descriptor, one must
 *    make sure corresponded logical block isn't allocated yet. this
 *    also means that freeing any block within descriptor's range
 *    must discard all preallocated blocks.
 *  - locality group
 *    assigned to specific locality group which does not translate to
 *    permanent set of inodes: inode can join and leave group. space
 *    from this type of preallocation can be used for any inode. thus
 *    it's consumed from the beginning to the end.
 *
 * relation between them can be expressed as:
 *    in-core buddy = on-disk bitmap + preallocation descriptors
 *
 * this mean blocks mballoc considers used are:
 *  - allocated blocks (persistent)
 *  - preallocated blocks (non-persistent)
 *
 * consistency in mballoc world means that at any time a block is either
 * free or used in ALL structures. notice: "any time" should not be read
 * literally -- time is discrete and delimited by locks.
 *
 *  to keep it simple, we don't use block numbers, instead we count number of
 *  blocks: how many blocks marked used/free in on-disk bitmap, buddy and PA.
 *
 * all operations can be expressed as:
 *  - init buddy:			buddy = on-disk + PAs
 *  - new PA:				buddy += N; PA = N
 *  - use inode PA:			on-disk += N; PA -= N
 *  - discard inode PA			buddy -= on-disk - PA; PA = 0
 *  - use locality group PA		on-disk += N; PA -= N
 *  - discard locality group PA		buddy -= PA; PA = 0
 *  note: 'buddy -= on-disk - PA' is used to show that on-disk bitmap
 *        is used in real operation because we can't know actual used
 *        bits from PA, only from on-disk bitmap
 *
 * if we follow this strict logic, then all operations above should be atomic.
 * given some of them can block, we'd have to use something like semaphores
 * killing performance on high-end SMP hardware. let's try to relax it using
 * the following knowledge:
 *  1) if buddy is referenced, it's already initialized
 *  2) while block is used in buddy and the buddy is referenced,
 *     nobody can re-allocate that block
 *  3) we work on bitmaps and '+' actually means 'set bits'. if on-disk has
 *     bit set and PA claims same block, it's OK. IOW, one can set bit in
 *     on-disk bitmap if buddy has same bit set or/and PA covers corresponded
 *     block
 *
 * so, now we're building a concurrency table:
 *  - init buddy vs.
 *    - new PA
 *      blocks for PA are allocated in the buddy, buddy must be referenced
 *      until PA is linked to allocation group to avoid concurrent buddy init
 *    - use inode PA
 *      we need to make sure that either on-disk bitmap or PA has uptodate data
 *      given (3) we care that PA-=N operation doesn't interfere with init
 *    - discard inode PA
 *      the simplest way would be to have buddy initialized by the discard
 *    - use locality group PA
 *      again PA-=N must be serialized with init
 *    - discard locality group PA
 *      the simplest way would be to have buddy initialized by the discard
 *  - new PA vs.
 *    - use inode PA
 *      i_data_sem serializes them
 *    - discard inode PA
 *      discard process must wait until PA isn't used by another process
 *    - use locality group PA
 *      some mutex should serialize them
 *    - discard locality group PA
 *      discard process must wait until PA isn't used by another process
 *  - use inode PA
 *    - use inode PA
 *      i_data_sem or another mutex should serializes them
 *    - discard inode PA
 *      discard process must wait until PA isn't used by another process
 *    - use locality group PA
 *      nothing wrong here -- they're different PAs covering different blocks
 *    - discard locality group PA
 *      discard process must wait until PA isn't used by another process
 *
 * now we're ready to make few consequences:
 *  - PA is referenced and while it is no discard is possible
 *  - PA is referenced until block isn't marked in on-disk bitmap
 *  - PA changes only after on-disk bitmap
 *  - discard must not compete with init. either init is done before
 *    any discard or they're serialized somehow
 *  - buddy init as sum of on-disk bitmap and PAs is done atomically
 *
 * a special case when we've used PA to emptiness. no need to modify buddy
 * in this case, but we should care about concurrent init
 *
 */

 /*
 * Logic in few words:
 *
 *  - allocation:
 *    load group
 *    find blocks
 *    mark bits in on-disk bitmap
 *    release group
 *
 *  - use preallocation:
 *    find proper PA (per-inode or group)
 *    load group
 *    mark bits in on-disk bitmap
 *    release group
 *    release PA
 *
 *  - free:
 *    load group
 *    mark bits in on-disk bitmap
 *    release group
 *
 *  - discard preallocations in group:
 *    mark PAs deleted
 *    move them onto local list
 *    load on-disk bitmap
 *    load group
 *    remove PA from object (inode or locality group)
 *    mark free blocks in-core
 *
 *  - discard inode's preallocations:
 */

/*
 * Locking rules
 *
 * Locks:
 *  - bitlock on a group	(group)
 *  - object (inode/locality)	(object)
 *  - per-pa lock		(pa)
 *
 * Paths:
 *  - new pa
 *    object
 *    group
 *
 *  - find and use pa:
 *    pa
 *
 *  - release consumed pa:
 *    pa
 *    group
 *    object
 *
 *  - generate in-core bitmap:
 *    group
 *        pa
 *
 *  - discard all for given object (inode, locality group):
 *    object
 *        pa
 *    group
 *
 *  - discard all for given group:
 *    group
 *        pa
 *    group
 *        object
 *
 */
static struct kmem_cache *ext4_pspace_cachep;
static struct kmem_cache *ext4_ac_cachep;
static struct kmem_cache *ext4_free_ext_cachep;
static void ext4_mb_generate_from_pa(struct super_block *sb, void *bitmap,
					ext4_group_t group);
static void ext4_mb_generate_from_freelist(struct super_block *sb, void *bitmap,
						ext4_group_t group);
static void release_blocks_on_commit(journal_t *journal, transaction_t *txn);

static inline void *mb_correct_addr_and_bit(int *bit, void *addr)
{
#if BITS_PER_LONG == 64
	*bit += ((unsigned long) addr & 7UL) << 3;
	addr = (void *) ((unsigned long) addr & ~7UL);
#elif BITS_PER_LONG == 32
	*bit += ((unsigned long) addr & 3UL) << 3;
	addr = (void *) ((unsigned long) addr & ~3UL);
#else
#error "how many bits you are?!"
#endif
	return addr;
}

static inline int mb_test_bit(int bit, void *addr)
{
	/*
	 * ext4_test_bit on architecture like powerpc
	 * needs unsigned long aligned address
	 */
	addr = mb_correct_addr_and_bit(&bit, addr);
	return ext4_test_bit(bit, addr);
}

static inline void mb_set_bit(int bit, void *addr)
{
	addr = mb_correct_addr_and_bit(&bit, addr);
	ext4_set_bit(bit, addr);
}

static inline void mb_clear_bit(int bit, void *addr)
{
	addr = mb_correct_addr_and_bit(&bit, addr);
	ext4_clear_bit(bit, addr);
}

static inline int mb_find_next_zero_bit(void *addr, int max, int start)
{
	int fix = 0, ret, tmpmax;
	addr = mb_correct_addr_and_bit(&fix, addr);
	tmpmax = max + fix;
	start += fix;

	ret = ext4_find_next_zero_bit(addr, tmpmax, start) - fix;
	if (ret > max)
		return max;
	return ret;
}

static inline int mb_find_next_bit(void *addr, int max, int start)
{
	int fix = 0, ret, tmpmax;
	addr = mb_correct_addr_and_bit(&fix, addr);
	tmpmax = max + fix;
	start += fix;

	ret = ext4_find_next_bit(addr, tmpmax, start) - fix;
	if (ret > max)
		return max;
	return ret;
}

static void *mb_find_buddy(struct ext4_buddy *e4b, int order, int *max)
{
	char *bb;

	BUG_ON(EXT4_MB_BITMAP(e4b) == EXT4_MB_BUDDY(e4b));
	BUG_ON(max == NULL);

	if (order > e4b->bd_blkbits + 1) {
		*max = 0;
		return NULL;
	}

	/* at order 0 we see each particular block */
	*max = 1 << (e4b->bd_blkbits + 3);
	if (order == 0)
		return EXT4_MB_BITMAP(e4b);

	bb = EXT4_MB_BUDDY(e4b) + EXT4_SB(e4b->bd_sb)->s_mb_offsets[order];
	*max = EXT4_SB(e4b->bd_sb)->s_mb_maxs[order];

	return bb;
}

#ifdef DOUBLE_CHECK
static void mb_free_blocks_double(struct inode *inode, struct ext4_buddy *e4b,
			   int first, int count)
{
	int i;
	struct super_block *sb = e4b->bd_sb;

	if (unlikely(e4b->bd_info->bb_bitmap == NULL))
		return;
	assert_spin_locked(ext4_group_lock_ptr(sb, e4b->bd_group));
	for (i = 0; i < count; i++) {
		if (!mb_test_bit(first + i, e4b->bd_info->bb_bitmap)) {
			ext4_fsblk_t blocknr;
			blocknr = e4b->bd_group * EXT4_BLOCKS_PER_GROUP(sb);
			blocknr += first + i;
			blocknr +=
			    le32_to_cpu(EXT4_SB(sb)->s_es->s_first_data_block);
			ext4_grp_locked_error(sb, e4b->bd_group,
				   __func__, "double-free of inode"
				   " %lu's block %llu(bit %u in group %u)",
				   inode ? inode->i_ino : 0, blocknr,
				   first + i, e4b->bd_group);
		}
		mb_clear_bit(first + i, e4b->bd_info->bb_bitmap);
	}
}

static void mb_mark_used_double(struct ext4_buddy *e4b, int first, int count)
{
	int i;

	if (unlikely(e4b->bd_info->bb_bitmap == NULL))
		return;
	assert_spin_locked(ext4_group_lock_ptr(e4b->bd_sb, e4b->bd_group));
	for (i = 0; i < count; i++) {
		BUG_ON(mb_test_bit(first + i, e4b->bd_info->bb_bitmap));
		mb_set_bit(first + i, e4b->bd_info->bb_bitmap);
	}
}

static void mb_cmp_bitmaps(struct ext4_buddy *e4b, void *bitmap)
{
	if (memcmp(e4b->bd_info->bb_bitmap, bitmap, e4b->bd_sb->s_blocksize)) {
		unsigned char *b1, *b2;
		int i;
		b1 = (unsigned char *) e4b->bd_info->bb_bitmap;
		b2 = (unsigned char *) bitmap;
		for (i = 0; i < e4b->bd_sb->s_blocksize; i++) {
			if (b1[i] != b2[i]) {
				printk(KERN_ERR "corruption in group %u "
				       "at byte %u(%u): %x in copy != %x "
				       "on disk/prealloc\n",
				       e4b->bd_group, i, i * 8, b1[i], b2[i]);
				BUG();
			}
		}
	}
}

#else
static inline void mb_free_blocks_double(struct inode *inode,
				struct ext4_buddy *e4b, int first, int count)
{
	return;
}
static inline void mb_mark_used_double(struct ext4_buddy *e4b,
						int first, int count)
{
	return;
}
static inline void mb_cmp_bitmaps(struct ext4_buddy *e4b, void *bitmap)
{
	return;
}
#endif

#ifdef AGGRESSIVE_CHECK

#define MB_CHECK_ASSERT(assert)						\
do {									\
	if (!(assert)) {						\
		printk(KERN_EMERG					\
			"Assertion failure in %s() at %s:%d: \"%s\"\n",	\
			function, file, line, # assert);		\
		BUG();							\
	}								\
} while (0)

static int __mb_check_buddy(struct ext4_buddy *e4b, char *file,
				const char *function, int line)
{
	struct super_block *sb = e4b->bd_sb;
	int order = e4b->bd_blkbits + 1;
	int max;
	int max2;
	int i;
	int j;
	int k;
	int count;
	struct ext4_group_info *grp;
	int fragments = 0;
	int fstart;
	struct list_head *cur;
	void *buddy;
	void *buddy2;

	{
		static int mb_check_counter;
		if (mb_check_counter++ % 100 != 0)
			return 0;
	}

	while (order > 1) {
		buddy = mb_find_buddy(e4b, order, &max);
		MB_CHECK_ASSERT(buddy);
		buddy2 = mb_find_buddy(e4b, order - 1, &max2);
		MB_CHECK_ASSERT(buddy2);
		MB_CHECK_ASSERT(buddy != buddy2);
		MB_CHECK_ASSERT(max * 2 == max2);

		count = 0;
		for (i = 0; i < max; i++) {

			if (mb_test_bit(i, buddy)) {
				/* only single bit in buddy2 may be 1 */
				if (!mb_test_bit(i << 1, buddy2)) {
					MB_CHECK_ASSERT(
						mb_test_bit((i<<1)+1, buddy2));
				} else if (!mb_test_bit((i << 1) + 1, buddy2)) {
					MB_CHECK_ASSERT(
						mb_test_bit(i << 1, buddy2));
				}
				continue;
			}

			/* both bits in buddy2 must be 0 */
			MB_CHECK_ASSERT(mb_test_bit(i << 1, buddy2));
			MB_CHECK_ASSERT(mb_test_bit((i << 1) + 1, buddy2));

			for (j = 0; j < (1 << order); j++) {
				k = (i * (1 << order)) + j;
				MB_CHECK_ASSERT(
					!mb_test_bit(k, EXT4_MB_BITMAP(e4b)));
			}
			count++;
		}
		MB_CHECK_ASSERT(e4b->bd_info->bb_counters[order] == count);
		order--;
	}

	fstart = -1;
	buddy = mb_find_buddy(e4b, 0, &max);
	for (i = 0; i < max; i++) {
		if (!mb_test_bit(i, buddy)) {
			MB_CHECK_ASSERT(i >= e4b->bd_info->bb_first_free);
			if (fstart == -1) {
				fragments++;
				fstart = i;
			}
			continue;
		}
		fstart = -1;
		/* check used bits only */
		for (j = 0; j < e4b->bd_blkbits + 1; j++) {
			buddy2 = mb_find_buddy(e4b, j, &max2);
			k = i >> j;
			MB_CHECK_ASSERT(k < max2);
			MB_CHECK_ASSERT(mb_test_bit(k, buddy2));
		}
	}
	MB_CHECK_ASSERT(!EXT4_MB_GRP_NEED_INIT(e4b->bd_info));
	MB_CHECK_ASSERT(e4b->bd_info->bb_fragments == fragments);

	grp = ext4_get_group_info(sb, e4b->bd_group);
	buddy = mb_find_buddy(e4b, 0, &max);
	list_for_each(cur, &grp->bb_prealloc_list) {
		ext4_group_t groupnr;
		struct ext4_prealloc_space *pa;
		pa = list_entry(cur, struct ext4_prealloc_space, pa_group_list);
		ext4_get_group_no_and_offset(sb, pa->pa_pstart, &groupnr, &k);
		MB_CHECK_ASSERT(groupnr == e4b->bd_group);
		for (i = 0; i < pa->pa_len; i++)
			MB_CHECK_ASSERT(mb_test_bit(k + i, buddy));
	}
	return 0;
}
#undef MB_CHECK_ASSERT
#define mb_check_buddy(e4b) __mb_check_buddy(e4b,	\
					__FILE__, __func__, __LINE__)
#else
#define mb_check_buddy(e4b)
#endif

/* FIXME!! need more doc */
static void ext4_mb_mark_free_simple(struct super_block *sb,
				void *buddy, ext4_grpblk_t first, ext4_grpblk_t len,
					struct ext4_group_info *grp)
{
	struct ext4_sb_info *sbi = EXT4_SB(sb);
	ext4_grpblk_t min;
	ext4_grpblk_t max;
	ext4_grpblk_t chunk;
	unsigned short border;

	BUG_ON(len > EXT4_BLOCKS_PER_GROUP(sb));

	border = 2 << sb->s_blocksize_bits;

	while (len > 0) {
		/* find how many blocks can be covered since this position */
		max = ffs(first | border) - 1;

		/* find how many blocks of power 2 we need to mark */
		min = fls(len) - 1;

		if (max < min)
			min = max;
		chunk = 1 << min;

		/* mark multiblock chunks only */
		grp->bb_counters[min]++;
		if (min > 0)
			mb_clear_bit(first >> min,
				     buddy + sbi->s_mb_offsets[min]);

		len -= chunk;
		first += chunk;
	}
}

static noinline_for_stack
void ext4_mb_generate_buddy(struct super_block *sb,
				void *buddy, void *bitmap, ext4_group_t group)
{
	struct ext4_group_info *grp = ext4_get_group_info(sb, group);
	ext4_grpblk_t max = EXT4_BLOCKS_PER_GROUP(sb);
	ext4_grpblk_t i = 0;
	ext4_grpblk_t first;
	ext4_grpblk_t len;
	unsigned free = 0;
	unsigned fragments = 0;
	unsigned long long period = get_cycles();

	/* initialize buddy from bitmap which is aggregation
	 * of on-disk bitmap and preallocations */
	i = mb_find_next_zero_bit(bitmap, max, 0);
	grp->bb_first_free = i;
	while (i < max) {
		fragments++;
		first = i;
		i = mb_find_next_bit(bitmap, max, i);
		len = i - first;
		free += len;
		if (len > 1)
			ext4_mb_mark_free_simple(sb, buddy, first, len, grp);
		else
			grp->bb_counters[0]++;
		if (i < max)
			i = mb_find_next_zero_bit(bitmap, max, i);
	}
	grp->bb_fragments = fragments;

	if (free != grp->bb_free) {
		ext4_grp_locked_error(sb, group,  __func__,
			"EXT4-fs: group %u: %u blocks in bitmap, %u in gd",
			group, free, grp->bb_free);
		/*
		 * If we intent to continue, we consider group descritor
		 * corrupt and update bb_free using bitmap value
		 */
		grp->bb_free = free;
	}

	clear_bit(EXT4_GROUP_INFO_NEED_INIT_BIT, &(grp->bb_state));

	period = get_cycles() - period;
	spin_lock(&EXT4_SB(sb)->s_bal_lock);
	EXT4_SB(sb)->s_mb_buddies_generated++;
	EXT4_SB(sb)->s_mb_generation_time += period;
	spin_unlock(&EXT4_SB(sb)->s_bal_lock);
}

/* The buddy information is attached the buddy cache inode
 * for convenience. The information regarding each group
 * is loaded via ext4_mb_load_buddy. The information involve
 * block bitmap and buddy information. The information are
 * stored in the inode as
 *
 * {                        page                        }
 * [ group 0 bitmap][ group 0 buddy] [group 1][ group 1]...
 *
 *
 * one block each for bitmap and buddy information.
 * So for each group we take up 2 blocks. A page can
 * contain blocks_per_page (PAGE_CACHE_SIZE / blocksize)  blocks.
 * So it can have information regarding groups_per_page which
 * is blocks_per_page/2
 */

static int ext4_mb_init_cache(struct page *page, char *incore)
{
	ext4_group_t ngroups;
	int blocksize;
	int blocks_per_page;
	int groups_per_page;
	int err = 0;
	int i;
	ext4_group_t first_group;
	int first_block;
	struct super_block *sb;
	struct buffer_head *bhs;
	struct buffer_head **bh;
	struct inode *inode;
	char *data;
	char *bitmap;

	mb_debug(1, "init page %lu\n", page->index);

	inode = page->mapping->host;
	sb = inode->i_sb;
	ngroups = ext4_get_groups_count(sb);
	blocksize = 1 << inode->i_blkbits;
	blocks_per_page = PAGE_CACHE_SIZE / blocksize;

	groups_per_page = blocks_per_page >> 1;
	if (groups_per_page == 0)
		groups_per_page = 1;

	/* allocate buffer_heads to read bitmaps */
	if (groups_per_page > 1) {
		err = -ENOMEM;
		i = sizeof(struct buffer_head *) * groups_per_page;
		bh = kzalloc(i, GFP_NOFS);
		if (bh == NULL)
			goto out;
	} else
		bh = &bhs;

	first_group = page->index * blocks_per_page / 2;

	/* read all groups the page covers into the cache */
	for (i = 0; i < groups_per_page; i++) {
		struct ext4_group_desc *desc;

		if (first_group + i >= ngroups)
			break;

		err = -EIO;
		desc = ext4_get_group_desc(sb, first_group + i, NULL);
		if (desc == NULL)
			goto out;

		err = -ENOMEM;
		bh[i] = sb_getblk(sb, ext4_block_bitmap(sb, desc));
		if (bh[i] == NULL)
			goto out;

		if (bitmap_uptodate(bh[i]))
			continue;

		lock_buffer(bh[i]);
		if (bitmap_uptodate(bh[i])) {
			unlock_buffer(bh[i]);
			continue;
		}
		ext4_lock_group(sb, first_group + i);
		if (desc->bg_flags & cpu_to_le16(EXT4_BG_BLOCK_UNINIT)) {
			ext4_init_block_bitmap(sb, bh[i],
						first_group + i, desc);
			set_bitmap_uptodate(bh[i]);
			set_buffer_uptodate(bh[i]);
			ext4_unlock_group(sb, first_group + i);
			unlock_buffer(bh[i]);
			continue;
		}
		ext4_unlock_group(sb, first_group + i);
		if (buffer_uptodate(bh[i])) {
			/*
			 * if not uninit if bh is uptodate,
			 * bitmap is also uptodate
			 */
			set_bitmap_uptodate(bh[i]);
			unlock_buffer(bh[i]);
			continue;
		}
		get_bh(bh[i]);
		/*
		 * submit the buffer_head for read. We can
		 * safely mark the bitmap as uptodate now.
		 * We do it here so the bitmap uptodate bit
		 * get set with buffer lock held.
		 */
		set_bitmap_uptodate(bh[i]);
		bh[i]->b_end_io = end_buffer_read_sync;
		submit_bh(READ, bh[i]);
		mb_debug(1, "read bitmap for group %u\n", first_group + i);
	}

	/* wait for I/O completion */
	for (i = 0; i < groups_per_page && bh[i]; i++)
		wait_on_buffer(bh[i]);

	err = -EIO;
	for (i = 0; i < groups_per_page && bh[i]; i++)
		if (!buffer_uptodate(bh[i]))
			goto out;

	err = 0;
	first_block = page->index * blocks_per_page;
	/* init the page  */
	memset(page_address(page), 0xff, PAGE_CACHE_SIZE);
	for (i = 0; i < blocks_per_page; i++) {
		int group;
		struct ext4_group_info *grinfo;

		group = (first_block + i) >> 1;
		if (group >= ngroups)
			break;

		/*
		 * data carry information regarding this
		 * particular group in the format specified
		 * above
		 *
		 */
		data = page_address(page) + (i * blocksize);
		bitmap = bh[group - first_group]->b_data;

		/*
		 * We place the buddy block and bitmap block
		 * close together
		 */
		if ((first_block + i) & 1) {
			/* this is block of buddy */
			BUG_ON(incore == NULL);
			mb_debug(1, "put buddy for group %u in page %lu/%x\n",
				group, page->index, i * blocksize);
			grinfo = ext4_get_group_info(sb, group);
			grinfo->bb_fragments = 0;
			memset(grinfo->bb_counters, 0,
			       sizeof(*grinfo->bb_counters) *
				(sb->s_blocksize_bits+2));
			/*
			 * incore got set to the group block bitmap below
			 */
			ext4_lock_group(sb, group);
			ext4_mb_generate_buddy(sb, data, incore, group);
			ext4_unlock_group(sb, group);
			incore = NULL;
		} else {
			/* this is block of bitmap */
			BUG_ON(incore != NULL);
			mb_debug(1, "put bitmap for group %u in page %lu/%x\n",
				group, page->index, i * blocksize);

			/* see comments in ext4_mb_put_pa() */
			ext4_lock_group(sb, group);
			memcpy(data, bitmap, blocksize);

			/* mark all preallocated blks used in in-core bitmap */
			ext4_mb_generate_from_pa(sb, data, group);
			ext4_mb_generate_from_freelist(sb, data, group);
			ext4_unlock_group(sb, group);

			/* set incore so that the buddy information can be
			 * generated using this
			 */
			incore = data;
		}
	}
	SetPageUptodate(page);

out:
	if (bh) {
		for (i = 0; i < groups_per_page && bh[i]; i++)
			brelse(bh[i]);
		if (bh != &bhs)
			kfree(bh);
	}
	return err;
}

static noinline_for_stack
int ext4_mb_init_group(struct super_block *sb, ext4_group_t group)
{

	int ret = 0;
	void *bitmap;
	int blocks_per_page;
	int block, pnum, poff;
	int num_grp_locked = 0;
	struct ext4_group_info *this_grp;
	struct ext4_sb_info *sbi = EXT4_SB(sb);
	struct inode *inode = sbi->s_buddy_cache;
	struct page *page = NULL, *bitmap_page = NULL;

	mb_debug(1, "init group %u\n", group);
	blocks_per_page = PAGE_CACHE_SIZE / sb->s_blocksize;
	this_grp = ext4_get_group_info(sb, group);
	/*
	 * This ensures that we don't reinit the buddy cache
	 * page which map to the group from which we are already
	 * allocating. If we are looking at the buddy cache we would
	 * have taken a reference using ext4_mb_load_buddy and that
	 * would have taken the alloc_sem lock.
	 */
	num_grp_locked =  ext4_mb_get_buddy_cache_lock(sb, group);
	if (!EXT4_MB_GRP_NEED_INIT(this_grp)) {
		/*
		 * somebody initialized the group
		 * return without doing anything
		 */
		ret = 0;
		goto err;
	}
	/*
	 * the buddy cache inode stores the block bitmap
	 * and buddy information in consecutive blocks.
	 * So for each group we need two blocks.
	 */
	block = group * 2;
	pnum = block / blocks_per_page;
	poff = block % blocks_per_page;
	page = find_or_create_page(inode->i_mapping, pnum, GFP_NOFS);
	if (page) {
		BUG_ON(page->mapping != inode->i_mapping);
		ret = ext4_mb_init_cache(page, NULL);
		if (ret) {
			unlock_page(page);
			goto err;
		}
		unlock_page(page);
	}
	if (page == NULL || !PageUptodate(page)) {
		ret = -EIO;
		goto err;
	}
	mark_page_accessed(page);
	bitmap_page = page;
	bitmap = page_address(page) + (poff * sb->s_blocksize);

	/* init buddy cache */
	block++;
	pnum = block / blocks_per_page;
	poff = block % blocks_per_page;
	page = find_or_create_page(inode->i_mapping, pnum, GFP_NOFS);
	if (page == bitmap_page) {
		/*
		 * If both the bitmap and buddy are in
		 * the same page we don't need to force
		 * init the buddy
		 */
		unlock_page(page);
	} else if (page) {
		BUG_ON(page->mapping != inode->i_mapping);
		ret = ext4_mb_init_cache(page, bitmap);
		if (ret) {
			unlock_page(page);
			goto err;
		}
		unlock_page(page);
	}
	if (page == NULL || !PageUptodate(page)) {
		ret = -EIO;
		goto err;
	}
	mark_page_accessed(page);
err:
	ext4_mb_put_buddy_cache_lock(sb, group, num_grp_locked);
	if (bitmap_page)
		page_cache_release(bitmap_page);
	if (page)
		page_cache_release(page);
	return ret;
}

static noinline_for_stack int
ext4_mb_load_buddy(struct super_block *sb, ext4_group_t group,
					struct ext4_buddy *e4b)
{
	int blocks_per_page;
	int block;
	int pnum;
	int poff;
	struct page *page;
	int ret;
	struct ext4_group_info *grp;
	struct ext4_sb_info *sbi = EXT4_SB(sb);
	struct inode *inode = sbi->s_buddy_cache;

	mb_debug(1, "load group %u\n", group);

	blocks_per_page = PAGE_CACHE_SIZE / sb->s_blocksize;
	grp = ext4_get_group_info(sb, group);

	e4b->bd_blkbits = sb->s_blocksize_bits;
	e4b->bd_info = ext4_get_group_info(sb, group);
	e4b->bd_sb = sb;
	e4b->bd_group = group;
	e4b->bd_buddy_page = NULL;
	e4b->bd_bitmap_page = NULL;
	e4b->alloc_semp = &grp->alloc_sem;

	/* Take the read lock on the group alloc
	 * sem. This would make sure a parallel
	 * ext4_mb_init_group happening on other
	 * groups mapped by the page is blocked
	 * till we are done with allocation
	 */
repeat_load_buddy:
	down_read(e4b->alloc_semp);

	if (unlikely(EXT4_MB_GRP_NEED_INIT(grp))) {
		/* we need to check for group need init flag
		 * with alloc_semp held so that we can be sure
		 * that new blocks didn't get added to the group
		 * when we are loading the buddy cache
		 */
		up_read(e4b->alloc_semp);
		/*
		 * we need full data about the group
		 * to make a good selection
		 */
		ret = ext4_mb_init_group(sb, group);
		if (ret)
			return ret;
		goto repeat_load_buddy;
	}

	/*
	 * the buddy cache inode stores the block bitmap
	 * and buddy information in consecutive blocks.
	 * So for each group we need two blocks.
	 */
	block = group * 2;
	pnum = block / blocks_per_page;
	poff = block % blocks_per_page;

	/* we could use find_or_create_page(), but it locks page
	 * what we'd like to avoid in fast path ... */
	page = find_get_page(inode->i_mapping, pnum);
	if (page == NULL || !PageUptodate(page)) {
		if (page)
			/*
			 * drop the page reference and try
			 * to get the page with lock. If we
			 * are not uptodate that implies
			 * somebody just created the page but
			 * is yet to initialize the same. So
			 * wait for it to initialize.
			 */
			page_cache_release(page);
		page = find_or_create_page(inode->i_mapping, pnum, GFP_NOFS);
		if (page) {
			BUG_ON(page->mapping != inode->i_mapping);
			if (!PageUptodate(page)) {
				ret = ext4_mb_init_cache(page, NULL);
				if (ret) {
					unlock_page(page);
					goto err;
				}
				mb_cmp_bitmaps(e4b, page_address(page) +
					       (poff * sb->s_blocksize));
			}
			unlock_page(page);
		}
	}
	if (page == NULL || !PageUptodate(page)) {
		ret = -EIO;
		goto err;
	}
	e4b->bd_bitmap_page = page;
	e4b->bd_bitmap = page_address(page) + (poff * sb->s_blocksize);
	mark_page_accessed(page);

	block++;
	pnum = block / blocks_per_page;
	poff = block % blocks_per_page;

	page = find_get_page(inode->i_mapping, pnum);
	if (page == NULL || !PageUptodate(page)) {
		if (page)
			page_cache_release(page);
		page = find_or_create_page(inode->i_mapping, pnum, GFP_NOFS);
		if (page) {
			BUG_ON(page->mapping != inode->i_mapping);
			if (!PageUptodate(page)) {
				ret = ext4_mb_init_cache(page, e4b->bd_bitmap);
				if (ret) {
					unlock_page(page);
					goto err;
				}
			}
			unlock_page(page);
		}
	}
	if (page == NULL || !PageUptodate(page)) {
		ret = -EIO;
		goto err;
	}
	e4b->bd_buddy_page = page;
	e4b->bd_buddy = page_address(page) + (poff * sb->s_blocksize);
	mark_page_accessed(page);

	BUG_ON(e4b->bd_bitmap_page == NULL);
	BUG_ON(e4b->bd_buddy_page == NULL);

	return 0;

err:
	if (e4b->bd_bitmap_page)
		page_cache_release(e4b->bd_bitmap_page);
	if (e4b->bd_buddy_page)
		page_cache_release(e4b->bd_buddy_page);
	e4b->bd_buddy = NULL;
	e4b->bd_bitmap = NULL;

	/* Done with the buddy cache */
	up_read(e4b->alloc_semp);
	return ret;
}

static void ext4_mb_release_desc(struct ext4_buddy *e4b)
{
	if (e4b->bd_bitmap_page)
		page_cache_release(e4b->bd_bitmap_page);
	if (e4b->bd_buddy_page)
		page_cache_release(e4b->bd_buddy_page);
	/* Done with the buddy cache */
	if (e4b->alloc_semp)
		up_read(e4b->alloc_semp);
}


static int mb_find_order_for_block(struct ext4_buddy *e4b, int block)
{
	int order = 1;
	void *bb;

	BUG_ON(EXT4_MB_BITMAP(e4b) == EXT4_MB_BUDDY(e4b));
	BUG_ON(block >= (1 << (e4b->bd_blkbits + 3)));

	bb = EXT4_MB_BUDDY(e4b);
	while (order <= e4b->bd_blkbits + 1) {
		block = block >> 1;
		if (!mb_test_bit(block, bb)) {
			/* this block is part of buddy of order 'order' */
			return order;
		}
		bb += 1 << (e4b->bd_blkbits - order);
		order++;
	}
	return 0;
}

static void mb_clear_bits(void *bm, int cur, int len)
{
	__u32 *addr;

	len = cur + len;
	while (cur < len) {
		if ((cur & 31) == 0 && (len - cur) >= 32) {
			/* fast path: clear whole word at once */
			addr = bm + (cur >> 3);
			*addr = 0;
			cur += 32;
			continue;
		}
		mb_clear_bit(cur, bm);
		cur++;
	}
}

static void mb_set_bits(void *bm, int cur, int len)
{
	__u32 *addr;

	len = cur + len;
	while (cur < len) {
		if ((cur & 31) == 0 && (len - cur) >= 32) {
			/* fast path: set whole word at once */
			addr = bm + (cur >> 3);
			*addr = 0xffffffff;
			cur += 32;
			continue;
		}
		mb_set_bit(cur, bm);
		cur++;
	}
}

static void mb_free_blocks(struct inode *inode, struct ext4_buddy *e4b,
			  int first, int count)
{
	int block = 0;
	int max = 0;
	int order;
	void *buddy;
	void *buddy2;
	struct super_block *sb = e4b->bd_sb;

	BUG_ON(first + count > (sb->s_blocksize << 3));
	assert_spin_locked(ext4_group_lock_ptr(sb, e4b->bd_group));
	mb_check_buddy(e4b);
	mb_free_blocks_double(inode, e4b, first, count);

	e4b->bd_info->bb_free += count;
	if (first < e4b->bd_info->bb_first_free)
		e4b->bd_info->bb_first_free = first;

	/* let's maintain fragments counter */
	if (first != 0)
		block = !mb_test_bit(first - 1, EXT4_MB_BITMAP(e4b));
	if (first + count < EXT4_SB(sb)->s_mb_maxs[0])
		max = !mb_test_bit(first + count, EXT4_MB_BITMAP(e4b));
	if (block && max)
		e4b->bd_info->bb_fragments--;
	else if (!block && !max)
		e4b->bd_info->bb_fragments++;

	/* let's maintain buddy itself */
	while (count-- > 0) {
		block = first++;
		order = 0;

		if (!mb_test_bit(block, EXT4_MB_BITMAP(e4b))) {
			ext4_fsblk_t blocknr;
			blocknr = e4b->bd_group * EXT4_BLOCKS_PER_GROUP(sb);
			blocknr += block;
			blocknr +=
			    le32_to_cpu(EXT4_SB(sb)->s_es->s_first_data_block);
			ext4_grp_locked_error(sb, e4b->bd_group,
				   __func__, "double-free of inode"
				   " %lu's block %llu(bit %u in group %u)",
				   inode ? inode->i_ino : 0, blocknr, block,
				   e4b->bd_group);
		}
		mb_clear_bit(block, EXT4_MB_BITMAP(e4b));
		e4b->bd_info->bb_counters[order]++;

		/* start of the buddy */
		buddy = mb_find_buddy(e4b, order, &max);

		do {
			block &= ~1UL;
			if (mb_test_bit(block, buddy) ||
					mb_test_bit(block + 1, buddy))
				break;

			/* both the buddies are free, try to coalesce them */
			buddy2 = mb_find_buddy(e4b, order + 1, &max);

			if (!buddy2)
				break;

			if (order > 0) {
				/* for special purposes, we don't set
				 * free bits in bitmap */
				mb_set_bit(block, buddy);
				mb_set_bit(block + 1, buddy);
			}
			e4b->bd_info->bb_counters[order]--;
			e4b->bd_info->bb_counters[order]--;

			block = block >> 1;
			order++;
			e4b->bd_info->bb_counters[order]++;

			mb_clear_bit(block, buddy2);
			buddy = buddy2;
		} while (1);
	}
	mb_check_buddy(e4b);
}

static int mb_find_extent(struct ext4_buddy *e4b, int order, int block,
				int needed, struct ext4_free_extent *ex)
{
	int next = block;
	int max;
	int ord;
	void *buddy;

	assert_spin_locked(ext4_group_lock_ptr(e4b->bd_sb, e4b->bd_group));
	BUG_ON(ex == NULL);

	buddy = mb_find_buddy(e4b, order, &max);
	BUG_ON(buddy == NULL);
	BUG_ON(block >= max);
	if (mb_test_bit(block, buddy)) {
		ex->fe_len = 0;
		ex->fe_start = 0;
		ex->fe_group = 0;
		return 0;
	}

	/* FIXME dorp order completely ? */
	if (likely(order == 0)) {
		/* find actual order */
		order = mb_find_order_for_block(e4b, block);
		block = block >> order;
	}

	ex->fe_len = 1 << order;
	ex->fe_start = block << order;
	ex->fe_group = e4b->bd_group;

	/* calc difference from given start */
	next = next - ex->fe_start;
	ex->fe_len -= next;
	ex->fe_start += next;

	while (needed > ex->fe_len &&
	       (buddy = mb_find_buddy(e4b, order, &max))) {

		if (block + 1 >= max)
			break;

		next = (block + 1) * (1 << order);
		if (mb_test_bit(next, EXT4_MB_BITMAP(e4b)))
			break;

		ord = mb_find_order_for_block(e4b, next);

		order = ord;
		block = next >> order;
		ex->fe_len += 1 << order;
	}

	BUG_ON(ex->fe_start + ex->fe_len > (1 << (e4b->bd_blkbits + 3)));
	return ex->fe_len;
}

static int mb_mark_used(struct ext4_buddy *e4b, struct ext4_free_extent *ex)
{
	int ord;
	int mlen = 0;
	int max = 0;
	int cur;
	int start = ex->fe_start;
	int len = ex->fe_len;
	unsigned ret = 0;
	int len0 = len;
	void *buddy;

	BUG_ON(start + len > (e4b->bd_sb->s_blocksize << 3));
	BUG_ON(e4b->bd_group != ex->fe_group);
	assert_spin_locked(ext4_group_lock_ptr(e4b->bd_sb, e4b->bd_group));
	mb_check_buddy(e4b);
	mb_mark_used_double(e4b, start, len);

	e4b->bd_info->bb_free -= len;
	if (e4b->bd_info->bb_first_free == start)
		e4b->bd_info->bb_first_free += len;

	/* let's maintain fragments counter */
	if (start != 0)
		mlen = !mb_test_bit(start - 1, EXT4_MB_BITMAP(e4b));
	if (start + len < EXT4_SB(e4b->bd_sb)->s_mb_maxs[0])
		max = !mb_test_bit(start + len, EXT4_MB_BITMAP(e4b));
	if (mlen && max)
		e4b->bd_info->bb_fragments++;
	else if (!mlen && !max)
		e4b->bd_info->bb_fragments--;

	/* let's maintain buddy itself */
	while (len) {
		ord = mb_find_order_for_block(e4b, start);

		if (((start >> ord) << ord) == start && len >= (1 << ord)) {
			/* the whole chunk may be allocated at once! */
			mlen = 1 << ord;
			buddy = mb_find_buddy(e4b, ord, &max);
			BUG_ON((start >> ord) >= max);
			mb_set_bit(start >> ord, buddy);
			e4b->bd_info->bb_counters[ord]--;
			start += mlen;
			len -= mlen;
			BUG_ON(len < 0);
			continue;
		}

		/* store for history */
		if (ret == 0)
			ret = len | (ord << 16);

		/* we have to split large buddy */
		BUG_ON(ord <= 0);
		buddy = mb_find_buddy(e4b, ord, &max);
		mb_set_bit(start >> ord, buddy);
		e4b->bd_info->bb_counters[ord]--;

		ord--;
		cur = (start >> ord) & ~1U;
		buddy = mb_find_buddy(e4b, ord, &max);
		mb_clear_bit(cur, buddy);
		mb_clear_bit(cur + 1, buddy);
		e4b->bd_info->bb_counters[ord]++;
		e4b->bd_info->bb_counters[ord]++;
	}

	mb_set_bits(EXT4_MB_BITMAP(e4b), ex->fe_start, len0);
	mb_check_buddy(e4b);

	return ret;
}

/*
 * Must be called under group lock!
 */
static void ext4_mb_use_best_found(struct ext4_allocation_context *ac,
					struct ext4_buddy *e4b)
{
	struct ext4_sb_info *sbi = EXT4_SB(ac->ac_sb);
	int ret;

	BUG_ON(ac->ac_b_ex.fe_group != e4b->bd_group);
	BUG_ON(ac->ac_status == AC_STATUS_FOUND);

	ac->ac_b_ex.fe_len = min(ac->ac_b_ex.fe_len, ac->ac_g_ex.fe_len);
	ac->ac_b_ex.fe_logical = ac->ac_g_ex.fe_logical;
	ret = mb_mark_used(e4b, &ac->ac_b_ex);

	/* preallocation can change ac_b_ex, thus we store actually
	 * allocated blocks for history */
	ac->ac_f_ex = ac->ac_b_ex;

	ac->ac_status = AC_STATUS_FOUND;
	ac->ac_tail = ret & 0xffff;
	ac->ac_buddy = ret >> 16;

	/*
	 * take the page reference. We want the page to be pinned
	 * so that we don't get a ext4_mb_init_cache_call for this
	 * group until we update the bitmap. That would mean we
	 * double allocate blocks. The reference is dropped
	 * in ext4_mb_release_context
	 */
	ac->ac_bitmap_page = e4b->bd_bitmap_page;
	get_page(ac->ac_bitmap_page);
	ac->ac_buddy_page = e4b->bd_buddy_page;
	get_page(ac->ac_buddy_page);
	/* on allocation we use ac to track the held semaphore */
	ac->alloc_semp =  e4b->alloc_semp;
	e4b->alloc_semp = NULL;
	/* store last allocated for subsequent stream allocation */
	if (ac->ac_flags & EXT4_MB_STREAM_ALLOC) {
		spin_lock(&sbi->s_md_lock);
		sbi->s_mb_last_group = ac->ac_f_ex.fe_group;
		sbi->s_mb_last_start = ac->ac_f_ex.fe_start;
		spin_unlock(&sbi->s_md_lock);
	}
}

/*
 * regular allocator, for general purposes allocation
 */

static void ext4_mb_check_limits(struct ext4_allocation_context *ac,
					struct ext4_buddy *e4b,
					int finish_group)
{
	struct ext4_sb_info *sbi = EXT4_SB(ac->ac_sb);
	struct ext4_free_extent *bex = &ac->ac_b_ex;
	struct ext4_free_extent *gex = &ac->ac_g_ex;
	struct ext4_free_extent ex;
	int max;

	if (ac->ac_status == AC_STATUS_FOUND)
		return;
	/*
	 * We don't want to scan for a whole year
	 */
	if (ac->ac_found > sbi->s_mb_max_to_scan &&
			!(ac->ac_flags & EXT4_MB_HINT_FIRST)) {
		ac->ac_status = AC_STATUS_BREAK;
		return;
	}

	/*
	 * Haven't found good chunk so far, let's continue
	 */
	if (bex->fe_len < gex->fe_len)
		return;

	if ((finish_group || ac->ac_found > sbi->s_mb_min_to_scan)
			&& bex->fe_group == e4b->bd_group) {
		/* recheck chunk's availability - we don't know
		 * when it was found (within this lock-unlock
		 * period or not) */
		max = mb_find_extent(e4b, 0, bex->fe_start, gex->fe_len, &ex);
		if (max >= gex->fe_len) {
			ext4_mb_use_best_found(ac, e4b);
			return;
		}
	}
}

/*
 * The routine checks whether found extent is good enough. If it is,
 * then the extent gets marked used and flag is set to the context
 * to stop scanning. Otherwise, the extent is compared with the
 * previous found extent and if new one is better, then it's stored
 * in the context. Later, the best found extent will be used, if
 * mballoc can't find good enough extent.
 *
 * FIXME: real allocation policy is to be designed yet!
 */
static void ext4_mb_measure_extent(struct ext4_allocation_context *ac,
					struct ext4_free_extent *ex,
					struct ext4_buddy *e4b)
{
	struct ext4_free_extent *bex = &ac->ac_b_ex;
	struct ext4_free_extent *gex = &ac->ac_g_ex;

	BUG_ON(ex->fe_len <= 0);
	BUG_ON(ex->fe_len > EXT4_BLOCKS_PER_GROUP(ac->ac_sb));
	BUG_ON(ex->fe_start >= EXT4_BLOCKS_PER_GROUP(ac->ac_sb));
	BUG_ON(ac->ac_status != AC_STATUS_CONTINUE);

	ac->ac_found++;

	/*
	 * The special case - take what you catch first
	 */
	if (unlikely(ac->ac_flags & EXT4_MB_HINT_FIRST)) {
		*bex = *ex;
		ext4_mb_use_best_found(ac, e4b);
		return;
	}

	/*
	 * Let's check whether the chuck is good enough
	 */
	if (ex->fe_len == gex->fe_len) {
		*bex = *ex;
		ext4_mb_use_best_found(ac, e4b);
		return;
	}

	/*
	 * If this is first found extent, just store it in the context
	 */
	if (bex->fe_len == 0) {
		*bex = *ex;
		return;
	}

	/*
	 * If new found extent is better, store it in the context
	 */
	if (bex->fe_len < gex->fe_len) {
		/* if the request isn't satisfied, any found extent
		 * larger than previous best one is better */
		if (ex->fe_len > bex->fe_len)
			*bex = *ex;
	} else if (ex->fe_len > gex->fe_len) {
		/* if the request is satisfied, then we try to find
		 * an extent that still satisfy the request, but is
		 * smaller than previous one */
		if (ex->fe_len < bex->fe_len)
			*bex = *ex;
	}

	ext4_mb_check_limits(ac, e4b, 0);
}

static noinline_for_stack
int ext4_mb_try_best_found(struct ext4_allocation_context *ac,
					struct ext4_buddy *e4b)
{
	struct ext4_free_extent ex = ac->ac_b_ex;
	ext4_group_t group = ex.fe_group;
	int max;
	int err;

	BUG_ON(ex.fe_len <= 0);
	err = ext4_mb_load_buddy(ac->ac_sb, group, e4b);
	if (err)
		return err;

	ext4_lock_group(ac->ac_sb, group);
	max = mb_find_extent(e4b, 0, ex.fe_start, ex.fe_len, &ex);

	if (max > 0) {
		ac->ac_b_ex = ex;
		ext4_mb_use_best_found(ac, e4b);
	}

	ext4_unlock_group(ac->ac_sb, group);
	ext4_mb_release_desc(e4b);

	return 0;
}

static noinline_for_stack
int ext4_mb_find_by_goal(struct ext4_allocation_context *ac,
				struct ext4_buddy *e4b)
{
	ext4_group_t group = ac->ac_g_ex.fe_group;
	int max;
	int err;
	struct ext4_sb_info *sbi = EXT4_SB(ac->ac_sb);
	struct ext4_super_block *es = sbi->s_es;
	struct ext4_free_extent ex;

	if (!(ac->ac_flags & EXT4_MB_HINT_TRY_GOAL))
		return 0;

	err = ext4_mb_load_buddy(ac->ac_sb, group, e4b);
	if (err)
		return err;

	ext4_lock_group(ac->ac_sb, group);
	max = mb_find_extent(e4b, 0, ac->ac_g_ex.fe_start,
			     ac->ac_g_ex.fe_len, &ex);

	if (max >= ac->ac_g_ex.fe_len && ac->ac_g_ex.fe_len == sbi->s_stripe) {
		ext4_fsblk_t start;

		start = (e4b->bd_group * EXT4_BLOCKS_PER_GROUP(ac->ac_sb)) +
			ex.fe_start + le32_to_cpu(es->s_first_data_block);
		/* use do_div to get remainder (would be 64-bit modulo) */
		if (do_div(start, sbi->s_stripe) == 0) {
			ac->ac_found++;
			ac->ac_b_ex = ex;
			ext4_mb_use_best_found(ac, e4b);
		}
	} else if (max >= ac->ac_g_ex.fe_len) {
		BUG_ON(ex.fe_len <= 0);
		BUG_ON(ex.fe_group != ac->ac_g_ex.fe_group);
		BUG_ON(ex.fe_start != ac->ac_g_ex.fe_start);
		ac->ac_found++;
		ac->ac_b_ex = ex;
		ext4_mb_use_best_found(ac, e4b);
	} else if (max > 0 && (ac->ac_flags & EXT4_MB_HINT_MERGE)) {
		/* Sometimes, caller may want to merge even small
		 * number of blocks to an existing extent */
		BUG_ON(ex.fe_len <= 0);
		BUG_ON(ex.fe_group != ac->ac_g_ex.fe_group);
		BUG_ON(ex.fe_start != ac->ac_g_ex.fe_start);
		ac->ac_found++;
		ac->ac_b_ex = ex;
		ext4_mb_use_best_found(ac, e4b);
	}
	ext4_unlock_group(ac->ac_sb, group);
	ext4_mb_release_desc(e4b);

	return 0;
}

/*
 * The routine scans buddy structures (not bitmap!) from given order
 * to max order and tries to find big enough chunk to satisfy the req
 */
static noinline_for_stack
void ext4_mb_simple_scan_group(struct ext4_allocation_context *ac,
					struct ext4_buddy *e4b)
{
	struct super_block *sb = ac->ac_sb;
	struct ext4_group_info *grp = e4b->bd_info;
	void *buddy;
	int i;
	int k;
	int max;

	BUG_ON(ac->ac_2order <= 0);
	for (i = ac->ac_2order; i <= sb->s_blocksize_bits + 1; i++) {
		if (grp->bb_counters[i] == 0)
			continue;

		buddy = mb_find_buddy(e4b, i, &max);
		BUG_ON(buddy == NULL);

		k = mb_find_next_zero_bit(buddy, max, 0);
		BUG_ON(k >= max);

		ac->ac_found++;

		ac->ac_b_ex.fe_len = 1 << i;
		ac->ac_b_ex.fe_start = k << i;
		ac->ac_b_ex.fe_group = e4b->bd_group;

		ext4_mb_use_best_found(ac, e4b);

		BUG_ON(ac->ac_b_ex.fe_len != ac->ac_g_ex.fe_len);

		if (EXT4_SB(sb)->s_mb_stats)
			atomic_inc(&EXT4_SB(sb)->s_bal_2orders);

		break;
	}
}

/*
 * The routine scans the group and measures all found extents.
 * In order to optimize scanning, caller must pass number of
 * free blocks in the group, so the routine can know upper limit.
 */
static noinline_for_stack
void ext4_mb_complex_scan_group(struct ext4_allocation_context *ac,
					struct ext4_buddy *e4b)
{
	struct super_block *sb = ac->ac_sb;
	void *bitmap = EXT4_MB_BITMAP(e4b);
	struct ext4_free_extent ex;
	int i;
	int free;

	free = e4b->bd_info->bb_free;
	BUG_ON(free <= 0);

	i = e4b->bd_info->bb_first_free;

	while (free && ac->ac_status == AC_STATUS_CONTINUE) {
		i = mb_find_next_zero_bit(bitmap,
						EXT4_BLOCKS_PER_GROUP(sb), i);
		if (i >= EXT4_BLOCKS_PER_GROUP(sb)) {
			/*
			 * IF we have corrupt bitmap, we won't find any
			 * free blocks even though group info says we
			 * we have free blocks
			 */
			ext4_grp_locked_error(sb, e4b->bd_group,
					__func__, "%d free blocks as per "
					"group info. But bitmap says 0",
					free);
			break;
		}

		mb_find_extent(e4b, 0, i, ac->ac_g_ex.fe_len, &ex);
		BUG_ON(ex.fe_len <= 0);
		if (free < ex.fe_len) {
			ext4_grp_locked_error(sb, e4b->bd_group,
					__func__, "%d free blocks as per "
					"group info. But got %d blocks",
					free, ex.fe_len);
			/*
			 * The number of free blocks differs. This mostly
			 * indicate that the bitmap is corrupt. So exit
			 * without claiming the space.
			 */
			break;
		}

		ext4_mb_measure_extent(ac, &ex, e4b);

		i += ex.fe_len;
		free -= ex.fe_len;
	}

	ext4_mb_check_limits(ac, e4b, 1);
}

/*
 * This is a special case for storages like raid5
 * we try to find stripe-aligned chunks for stripe-size requests
 * XXX should do so at least for multiples of stripe size as well
 */
static noinline_for_stack
void ext4_mb_scan_aligned(struct ext4_allocation_context *ac,
				 struct ext4_buddy *e4b)
{
	struct super_block *sb = ac->ac_sb;
	struct ext4_sb_info *sbi = EXT4_SB(sb);
	void *bitmap = EXT4_MB_BITMAP(e4b);
	struct ext4_free_extent ex;
	ext4_fsblk_t first_group_block;
	ext4_fsblk_t a;
	ext4_grpblk_t i;
	int max;

	BUG_ON(sbi->s_stripe == 0);

	/* find first stripe-aligned block in group */
	first_group_block = e4b->bd_group * EXT4_BLOCKS_PER_GROUP(sb)
		+ le32_to_cpu(sbi->s_es->s_first_data_block);
	a = first_group_block + sbi->s_stripe - 1;
	do_div(a, sbi->s_stripe);
	i = (a * sbi->s_stripe) - first_group_block;

	while (i < EXT4_BLOCKS_PER_GROUP(sb)) {
		if (!mb_test_bit(i, bitmap)) {
			max = mb_find_extent(e4b, 0, i, sbi->s_stripe, &ex);
			if (max >= sbi->s_stripe) {
				ac->ac_found++;
				ac->ac_b_ex = ex;
				ext4_mb_use_best_found(ac, e4b);
				break;
			}
		}
		i += sbi->s_stripe;
	}
}

static int ext4_mb_good_group(struct ext4_allocation_context *ac,
				ext4_group_t group, int cr)
{
	unsigned free, fragments;
	unsigned i, bits;
	int flex_size = ext4_flex_bg_size(EXT4_SB(ac->ac_sb));
	struct ext4_group_info *grp = ext4_get_group_info(ac->ac_sb, group);

	BUG_ON(cr < 0 || cr >= 4);
	BUG_ON(EXT4_MB_GRP_NEED_INIT(grp));

	free = grp->bb_free;
	fragments = grp->bb_fragments;
	if (free == 0)
		return 0;
	if (fragments == 0)
		return 0;

	switch (cr) {
	case 0:
		BUG_ON(ac->ac_2order == 0);

		/* Avoid using the first bg of a flexgroup for data files */
		if ((ac->ac_flags & EXT4_MB_HINT_DATA) &&
		    (flex_size >= EXT4_FLEX_SIZE_DIR_ALLOC_SCHEME) &&
		    ((group % flex_size) == 0))
			return 0;

		bits = ac->ac_sb->s_blocksize_bits + 1;
		for (i = ac->ac_2order; i <= bits; i++)
			if (grp->bb_counters[i] > 0)
				return 1;
		break;
	case 1:
		if ((free / fragments) >= ac->ac_g_ex.fe_len)
			return 1;
		break;
	case 2:
		if (free >= ac->ac_g_ex.fe_len)
			return 1;
		break;
	case 3:
		return 1;
	default:
		BUG();
	}

	return 0;
}

/*
 * lock the group_info alloc_sem of all the groups
 * belonging to the same buddy cache page. This
 * make sure other parallel operation on the buddy
 * cache doesn't happen  whild holding the buddy cache
 * lock
 */
int ext4_mb_get_buddy_cache_lock(struct super_block *sb, ext4_group_t group)
{
	int i;
	int block, pnum;
	int blocks_per_page;
	int groups_per_page;
	ext4_group_t ngroups = ext4_get_groups_count(sb);
	ext4_group_t first_group;
	struct ext4_group_info *grp;

	blocks_per_page = PAGE_CACHE_SIZE / sb->s_blocksize;
	/*
	 * the buddy cache inode stores the block bitmap
	 * and buddy information in consecutive blocks.
	 * So for each group we need two blocks.
	 */
	block = group * 2;
	pnum = block / blocks_per_page;
	first_group = pnum * blocks_per_page / 2;

	groups_per_page = blocks_per_page >> 1;
	if (groups_per_page == 0)
		groups_per_page = 1;
	/* read all groups the page covers into the cache */
	for (i = 0; i < groups_per_page; i++) {

		if ((first_group + i) >= ngroups)
			break;
		grp = ext4_get_group_info(sb, first_group + i);
		/* take all groups write allocation
		 * semaphore. This make sure there is
		 * no block allocation going on in any
		 * of that groups
		 */
		down_write_nested(&grp->alloc_sem, i);
	}
	return i;
}

void ext4_mb_put_buddy_cache_lock(struct super_block *sb,
					ext4_group_t group, int locked_group)
{
	int i;
	int block, pnum;
	int blocks_per_page;
	ext4_group_t first_group;
	struct ext4_group_info *grp;

	blocks_per_page = PAGE_CACHE_SIZE / sb->s_blocksize;
	/*
	 * the buddy cache inode stores the block bitmap
	 * and buddy information in consecutive blocks.
	 * So for each group we need two blocks.
	 */
	block = group * 2;
	pnum = block / blocks_per_page;
	first_group = pnum * blocks_per_page / 2;
	/* release locks on all the groups */
	for (i = 0; i < locked_group; i++) {

		grp = ext4_get_group_info(sb, first_group + i);
		/* take all groups write allocation
		 * semaphore. This make sure there is
		 * no block allocation going on in any
		 * of that groups
		 */
		up_write(&grp->alloc_sem);
	}

}

static noinline_for_stack int
ext4_mb_regular_allocator(struct ext4_allocation_context *ac)
{
	ext4_group_t ngroups, group, i;
	int cr;
	int err = 0;
	int bsbits;
	struct ext4_sb_info *sbi;
	struct super_block *sb;
	struct ext4_buddy e4b;

	sb = ac->ac_sb;
	sbi = EXT4_SB(sb);
	ngroups = ext4_get_groups_count(sb);
	/* non-extent files are limited to low blocks/groups */
	if (!(EXT4_I(ac->ac_inode)->i_flags & EXT4_EXTENTS_FL))
		ngroups = sbi->s_blockfile_groups;

	BUG_ON(ac->ac_status == AC_STATUS_FOUND);

	/* first, try the goal */
	err = ext4_mb_find_by_goal(ac, &e4b);
	if (err || ac->ac_status == AC_STATUS_FOUND)
		goto out;

	if (unlikely(ac->ac_flags & EXT4_MB_HINT_GOAL_ONLY))
		goto out;

	/*
	 * ac->ac2_order is set only if the fe_len is a power of 2
	 * if ac2_order is set we also set criteria to 0 so that we
	 * try exact allocation using buddy.
	 */
	i = fls(ac->ac_g_ex.fe_len);
	ac->ac_2order = 0;
	/*
	 * We search using buddy data only if the order of the request
	 * is greater than equal to the sbi_s_mb_order2_reqs
	 * You can tune it via /sys/fs/ext4/<partition>/mb_order2_req
	 */
	if (i >= sbi->s_mb_order2_reqs) {
		/*
		 * This should tell if fe_len is exactly power of 2
		 */
		if ((ac->ac_g_ex.fe_len & (~(1 << (i - 1)))) == 0)
			ac->ac_2order = i - 1;
	}

	bsbits = ac->ac_sb->s_blocksize_bits;

	/* if stream allocation is enabled, use global goal */
	if (ac->ac_flags & EXT4_MB_STREAM_ALLOC) {
		/* TBD: may be hot point */
		spin_lock(&sbi->s_md_lock);
		ac->ac_g_ex.fe_group = sbi->s_mb_last_group;
		ac->ac_g_ex.fe_start = sbi->s_mb_last_start;
		spin_unlock(&sbi->s_md_lock);
	}

	/* Let's just scan groups to find more-less suitable blocks */
	cr = ac->ac_2order ? 0 : 1;
	/*
	 * cr == 0 try to get exact allocation,
	 * cr == 3  try to get anything
	 */
repeat:
	for (; cr < 4 && ac->ac_status == AC_STATUS_CONTINUE; cr++) {
		ac->ac_criteria = cr;
		/*
		 * searching for the right group start
		 * from the goal value specified
		 */
		group = ac->ac_g_ex.fe_group;

		for (i = 0; i < ngroups; group++, i++) {
			struct ext4_group_info *grp;
			struct ext4_group_desc *desc;

			if (group == ngroups)
				group = 0;

			/* quick check to skip empty groups */
			grp = ext4_get_group_info(sb, group);
			if (grp->bb_free == 0)
				continue;

			err = ext4_mb_load_buddy(sb, group, &e4b);
			if (err)
				goto out;

			ext4_lock_group(sb, group);
			if (!ext4_mb_good_group(ac, group, cr)) {
				/* someone did allocation from this group */
				ext4_unlock_group(sb, group);
				ext4_mb_release_desc(&e4b);
				continue;
			}

			ac->ac_groups_scanned++;
			desc = ext4_get_group_desc(sb, group, NULL);
			if (cr == 0)
				ext4_mb_simple_scan_group(ac, &e4b);
			else if (cr == 1 &&
					ac->ac_g_ex.fe_len == sbi->s_stripe)
				ext4_mb_scan_aligned(ac, &e4b);
			else
				ext4_mb_complex_scan_group(ac, &e4b);

			ext4_unlock_group(sb, group);
			ext4_mb_release_desc(&e4b);

			if (ac->ac_status != AC_STATUS_CONTINUE)
				break;
		}
	}

	if (ac->ac_b_ex.fe_len > 0 && ac->ac_status != AC_STATUS_FOUND &&
	    !(ac->ac_flags & EXT4_MB_HINT_FIRST)) {
		/*
		 * We've been searching too long. Let's try to allocate
		 * the best chunk we've found so far
		 */

		ext4_mb_try_best_found(ac, &e4b);
		if (ac->ac_status != AC_STATUS_FOUND) {
			/*
			 * Someone more lucky has already allocated it.
			 * The only thing we can do is just take first
			 * found block(s)
			printk(KERN_DEBUG "EXT4-fs: someone won our chunk\n");
			 */
			ac->ac_b_ex.fe_group = 0;
			ac->ac_b_ex.fe_start = 0;
			ac->ac_b_ex.fe_len = 0;
			ac->ac_status = AC_STATUS_CONTINUE;
			ac->ac_flags |= EXT4_MB_HINT_FIRST;
			cr = 3;
			atomic_inc(&sbi->s_mb_lost_chunks);
			goto repeat;
		}
	}
out:
	return err;
}

static void *ext4_mb_seq_groups_start(struct seq_file *seq, loff_t *pos)
{
	struct super_block *sb = seq->private;
	ext4_group_t group;

	if (*pos < 0 || *pos >= ext4_get_groups_count(sb))
		return NULL;
	group = *pos + 1;
	return (void *) ((unsigned long) group);
}

static void *ext4_mb_seq_groups_next(struct seq_file *seq, void *v, loff_t *pos)
{
	struct super_block *sb = seq->private;
	ext4_group_t group;

	++*pos;
	if (*pos < 0 || *pos >= ext4_get_groups_count(sb))
		return NULL;
	group = *pos + 1;
	return (void *) ((unsigned long) group);
}

static int ext4_mb_seq_groups_show(struct seq_file *seq, void *v)
{
	struct super_block *sb = seq->private;
	ext4_group_t group = (ext4_group_t) ((unsigned long) v);
	int i;
	int err;
	struct ext4_buddy e4b;
	struct sg {
		struct ext4_group_info info;
		ext4_grpblk_t counters[16];
	} sg;

	group--;
	if (group == 0)
		seq_printf(seq, "#%-5s: %-5s %-5s %-5s "
				"[ %-5s %-5s %-5s %-5s %-5s %-5s %-5s "
				  "%-5s %-5s %-5s %-5s %-5s %-5s %-5s ]\n",
			   "group", "free", "frags", "first",
			   "2^0", "2^1", "2^2", "2^3", "2^4", "2^5", "2^6",
			   "2^7", "2^8", "2^9", "2^10", "2^11", "2^12", "2^13");

	i = (sb->s_blocksize_bits + 2) * sizeof(sg.info.bb_counters[0]) +
		sizeof(struct ext4_group_info);
	err = ext4_mb_load_buddy(sb, group, &e4b);
	if (err) {
		seq_printf(seq, "#%-5u: I/O error\n", group);
		return 0;
	}
	ext4_lock_group(sb, group);
	memcpy(&sg, ext4_get_group_info(sb, group), i);
	ext4_unlock_group(sb, group);
	ext4_mb_release_desc(&e4b);

	seq_printf(seq, "#%-5u: %-5u %-5u %-5u [", group, sg.info.bb_free,
			sg.info.bb_fragments, sg.info.bb_first_free);
	for (i = 0; i <= 13; i++)
		seq_printf(seq, " %-5u", i <= sb->s_blocksize_bits + 1 ?
				sg.info.bb_counters[i] : 0);
	seq_printf(seq, " ]\n");

	return 0;
}

static void ext4_mb_seq_groups_stop(struct seq_file *seq, void *v)
{
}

static const struct seq_operations ext4_mb_seq_groups_ops = {
	.start  = ext4_mb_seq_groups_start,
	.next   = ext4_mb_seq_groups_next,
	.stop   = ext4_mb_seq_groups_stop,
	.show   = ext4_mb_seq_groups_show,
};

static int ext4_mb_seq_groups_open(struct inode *inode, struct file *file)
{
	struct super_block *sb = PDE(inode)->data;
	int rc;

	rc = seq_open(file, &ext4_mb_seq_groups_ops);
	if (rc == 0) {
		struct seq_file *m = (struct seq_file *)file->private_data;
		m->private = sb;
	}
	return rc;

}

static const struct file_operations ext4_mb_seq_groups_fops = {
	.owner		= THIS_MODULE,
	.open		= ext4_mb_seq_groups_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};


/* Create and initialize ext4_group_info data for the given group. */
int ext4_mb_add_groupinfo(struct super_block *sb, ext4_group_t group,
			  struct ext4_group_desc *desc)
{
	int i, len;
	int metalen = 0;
	struct ext4_sb_info *sbi = EXT4_SB(sb);
	struct ext4_group_info **meta_group_info;

	/*
	 * First check if this group is the first of a reserved block.
	 * If it's true, we have to allocate a new table of pointers
	 * to ext4_group_info structures
	 */
	if (group % EXT4_DESC_PER_BLOCK(sb) == 0) {
		metalen = sizeof(*meta_group_info) <<
			EXT4_DESC_PER_BLOCK_BITS(sb);
		meta_group_info = kmalloc(metalen, GFP_KERNEL);
		if (meta_group_info == NULL) {
			printk(KERN_ERR "EXT4-fs: can't allocate mem for a "
			       "buddy group\n");
			goto exit_meta_group_info;
		}
		sbi->s_group_info[group >> EXT4_DESC_PER_BLOCK_BITS(sb)] =
			meta_group_info;
	}

	/*
	 * calculate needed size. if change bb_counters size,
	 * don't forget about ext4_mb_generate_buddy()
	 */
	len = offsetof(typeof(**meta_group_info),
		       bb_counters[sb->s_blocksize_bits + 2]);

	meta_group_info =
		sbi->s_group_info[group >> EXT4_DESC_PER_BLOCK_BITS(sb)];
	i = group & (EXT4_DESC_PER_BLOCK(sb) - 1);

	meta_group_info[i] = kzalloc(len, GFP_KERNEL);
	if (meta_group_info[i] == NULL) {
		printk(KERN_ERR "EXT4-fs: can't allocate buddy mem\n");
		goto exit_group_info;
	}
	set_bit(EXT4_GROUP_INFO_NEED_INIT_BIT,
		&(meta_group_info[i]->bb_state));

	/*
	 * initialize bb_free to be able to skip
	 * empty groups without initialization
	 */
	if (desc->bg_flags & cpu_to_le16(EXT4_BG_BLOCK_UNINIT)) {
		meta_group_info[i]->bb_free =
			ext4_free_blocks_after_init(sb, group, desc);
	} else {
		meta_group_info[i]->bb_free =
			ext4_free_blks_count(sb, desc);
	}

	INIT_LIST_HEAD(&meta_group_info[i]->bb_prealloc_list);
	init_rwsem(&meta_group_info[i]->alloc_sem);
	meta_group_info[i]->bb_free_root.rb_node = NULL;

#ifdef DOUBLE_CHECK
	{
		struct buffer_head *bh;
		meta_group_info[i]->bb_bitmap =
			kmalloc(sb->s_blocksize, GFP_KERNEL);
		BUG_ON(meta_group_info[i]->bb_bitmap == NULL);
		bh = ext4_read_block_bitmap(sb, group);
		BUG_ON(bh == NULL);
		memcpy(meta_group_info[i]->bb_bitmap, bh->b_data,
			sb->s_blocksize);
		put_bh(bh);
	}
#endif

	return 0;

exit_group_info:
	/* If a meta_group_info table has been allocated, release it now */
	if (group % EXT4_DESC_PER_BLOCK(sb) == 0)
		kfree(sbi->s_group_info[group >> EXT4_DESC_PER_BLOCK_BITS(sb)]);
exit_meta_group_info:
	return -ENOMEM;
} /* ext4_mb_add_groupinfo */

static int ext4_mb_init_backend(struct super_block *sb)
{
	ext4_group_t ngroups = ext4_get_groups_count(sb);
	ext4_group_t i;
	struct ext4_sb_info *sbi = EXT4_SB(sb);
	struct ext4_super_block *es = sbi->s_es;
	int num_meta_group_infos;
	int num_meta_group_infos_max;
	int array_size;
	struct ext4_group_desc *desc;

	/* This is the number of blocks used by GDT */
	num_meta_group_infos = (ngroups + EXT4_DESC_PER_BLOCK(sb) -
				1) >> EXT4_DESC_PER_BLOCK_BITS(sb);

	/*
	 * This is the total number of blocks used by GDT including
	 * the number of reserved blocks for GDT.
	 * The s_group_info array is allocated with this value
	 * to allow a clean online resize without a complex
	 * manipulation of pointer.
	 * The drawback is the unused memory when no resize
	 * occurs but it's very low in terms of pages
	 * (see comments below)
	 * Need to handle this properly when META_BG resizing is allowed
	 */
	num_meta_group_infos_max = num_meta_group_infos +
				le16_to_cpu(es->s_reserved_gdt_blocks);

	/*
	 * array_size is the size of s_group_info array. We round it
	 * to the next power of two because this approximation is done
	 * internally by kmalloc so we can have some more memory
	 * for free here (e.g. may be used for META_BG resize).
	 */
	array_size = 1;
	while (array_size < sizeof(*sbi->s_group_info) *
	       num_meta_group_infos_max)
		array_size = array_size << 1;
	/* An 8TB filesystem with 64-bit pointers requires a 4096 byte
	 * kmalloc. A 128kb malloc should suffice for a 256TB filesystem.
	 * So a two level scheme suffices for now. */
	sbi->s_group_info = kmalloc(array_size, GFP_KERNEL);
	if (sbi->s_group_info == NULL) {
		printk(KERN_ERR "EXT4-fs: can't allocate buddy meta group\n");
		return -ENOMEM;
	}
	sbi->s_buddy_cache = new_inode(sb);
	if (sbi->s_buddy_cache == NULL) {
		printk(KERN_ERR "EXT4-fs: can't get new inode\n");
		goto err_freesgi;
	}
	EXT4_I(sbi->s_buddy_cache)->i_disksize = 0;
	for (i = 0; i < ngroups; i++) {
		desc = ext4_get_group_desc(sb, i, NULL);
		if (desc == NULL) {
			printk(KERN_ERR
				"EXT4-fs: can't read descriptor %u\n", i);
			goto err_freebuddy;
		}
		if (ext4_mb_add_groupinfo(sb, i, desc) != 0)
			goto err_freebuddy;
	}

	return 0;

err_freebuddy:
	while (i-- > 0)
		kfree(ext4_get_group_info(sb, i));
	i = num_meta_group_infos;
	while (i-- > 0)
		kfree(sbi->s_group_info[i]);
	iput(sbi->s_buddy_cache);
err_freesgi:
	kfree(sbi->s_group_info);
	return -ENOMEM;
}

int ext4_mb_init(struct super_block *sb, int needs_recovery)
{
	struct ext4_sb_info *sbi = EXT4_SB(sb);
	unsigned i, j;
	unsigned offset;
	unsigned max;
	int ret;

	i = (sb->s_blocksize_bits + 2) * sizeof(*sbi->s_mb_offsets);

	sbi->s_mb_offsets = kmalloc(i, GFP_KERNEL);
	if (sbi->s_mb_offsets == NULL) {
		return -ENOMEM;
	}

	i = (sb->s_blocksize_bits + 2) * sizeof(*sbi->s_mb_maxs);
	sbi->s_mb_maxs = kmalloc(i, GFP_KERNEL);
	if (sbi->s_mb_maxs == NULL) {
		kfree(sbi->s_mb_offsets);
		return -ENOMEM;
	}

	/* order 0 is regular bitmap */
	sbi->s_mb_maxs[0] = sb->s_blocksize << 3;
	sbi->s_mb_offsets[0] = 0;

	i = 1;
	offset = 0;
	max = sb->s_blocksize << 2;
	do {
		sbi->s_mb_offsets[i] = offset;
		sbi->s_mb_maxs[i] = max;
		offset += 1 << (sb->s_blocksize_bits - i);
		max = max >> 1;
		i++;
	} while (i <= sb->s_blocksize_bits + 1);

	/* init file for buddy data */
	ret = ext4_mb_init_backend(sb);
	if (ret != 0) {
		kfree(sbi->s_mb_offsets);
		kfree(sbi->s_mb_maxs);
		return ret;
	}

	spin_lock_init(&sbi->s_md_lock);
	spin_lock_init(&sbi->s_bal_lock);

	sbi->s_mb_max_to_scan = MB_DEFAULT_MAX_TO_SCAN;
	sbi->s_mb_min_to_scan = MB_DEFAULT_MIN_TO_SCAN;
	sbi->s_mb_stats = MB_DEFAULT_STATS;
	sbi->s_mb_stream_request = MB_DEFAULT_STREAM_THRESHOLD;
	sbi->s_mb_order2_reqs = MB_DEFAULT_ORDER2_REQS;
	sbi->s_mb_group_prealloc = MB_DEFAULT_GROUP_PREALLOC;

	sbi->s_locality_groups = alloc_percpu(struct ext4_locality_group);
	if (sbi->s_locality_groups == NULL) {
		kfree(sbi->s_mb_offsets);
		kfree(sbi->s_mb_maxs);
		return -ENOMEM;
	}
	for_each_possible_cpu(i) {
		struct ext4_locality_group *lg;
		lg = per_cpu_ptr(sbi->s_locality_groups, i);
		mutex_init(&lg->lg_mutex);
		for (j = 0; j < PREALLOC_TB_SIZE; j++)
			INIT_LIST_HEAD(&lg->lg_prealloc_list[j]);
		spin_lock_init(&lg->lg_prealloc_lock);
	}

	if (sbi->s_proc)
		proc_create_data("mb_groups", S_IRUGO, sbi->s_proc,
				 &ext4_mb_seq_groups_fops, sb);

	if (sbi->s_journal)
		sbi->s_journal->j_commit_callback = release_blocks_on_commit;
	return 0;
}

/* need to called with the ext4 group lock held */
static void ext4_mb_cleanup_pa(struct ext4_group_info *grp)
{
	struct ext4_prealloc_space *pa;
	struct list_head *cur, *tmp;
	int count = 0;

	list_for_each_safe(cur, tmp, &grp->bb_prealloc_list) {
		pa = list_entry(cur, struct ext4_prealloc_space, pa_group_list);
		list_del(&pa->pa_group_list);
		count++;
		kmem_cache_free(ext4_pspace_cachep, pa);
	}
	if (count)
		mb_debug(1, "mballoc: %u PAs left\n", count);

}

int ext4_mb_release(struct super_block *sb)
{
	ext4_group_t ngroups = ext4_get_groups_count(sb);
	ext4_group_t i;
	int num_meta_group_infos;
	struct ext4_group_info *grinfo;
	struct ext4_sb_info *sbi = EXT4_SB(sb);

	if (sbi->s_group_info) {
		for (i = 0; i < ngroups; i++) {
			grinfo = ext4_get_group_info(sb, i);
#ifdef DOUBLE_CHECK
			kfree(grinfo->bb_bitmap);
#endif
			ext4_lock_group(sb, i);
			ext4_mb_cleanup_pa(grinfo);
			ext4_unlock_group(sb, i);
			kfree(grinfo);
		}
		num_meta_group_infos = (ngroups +
				EXT4_DESC_PER_BLOCK(sb) - 1) >>
			EXT4_DESC_PER_BLOCK_BITS(sb);
		for (i = 0; i < num_meta_group_infos; i++)
			kfree(sbi->s_group_info[i]);
		kfree(sbi->s_group_info);
	}
	kfree(sbi->s_mb_offsets);
	kfree(sbi->s_mb_maxs);
	if (sbi->s_buddy_cache)
		iput(sbi->s_buddy_cache);
	if (sbi->s_mb_stats) {
		printk(KERN_INFO
		       "EXT4-fs: mballoc: %u blocks %u reqs (%u success)\n",
				atomic_read(&sbi->s_bal_allocated),
				atomic_read(&sbi->s_bal_reqs),
				atomic_read(&sbi->s_bal_success));
		printk(KERN_INFO
		      "EXT4-fs: mballoc: %u extents scanned, %u goal hits, "
				"%u 2^N hits, %u breaks, %u lost\n",
				atomic_read(&sbi->s_bal_ex_scanned),
				atomic_read(&sbi->s_bal_goals),
				atomic_read(&sbi->s_bal_2orders),
				atomic_read(&sbi->s_bal_breaks),
				atomic_read(&sbi->s_mb_lost_chunks));
		printk(KERN_INFO
		       "EXT4-fs: mballoc: %lu generated and it took %Lu\n",
				sbi->s_mb_buddies_generated++,
				sbi->s_mb_generation_time);
		printk(KERN_INFO
		       "EXT4-fs: mballoc: %u preallocated, %u discarded\n",
				atomic_read(&sbi->s_mb_preallocated),
				atomic_read(&sbi->s_mb_discarded));
	}

	free_percpu(sbi->s_locality_groups);
	if (sbi->s_proc)
		remove_proc_entry("mb_groups", sbi->s_proc);

	return 0;
}

/*
 * This function is called by the jbd2 layer once the commit has finished,
 * so we know we can free the blocks that were released with that commit.
 */
static void release_blocks_on_commit(journal_t *journal, transaction_t *txn)
{
	struct super_block *sb = journal->j_private;
	struct ext4_buddy e4b;
	struct ext4_group_info *db;
	int err, count = 0, count2 = 0;
	struct ext4_free_data *entry;
	struct list_head *l, *ltmp;

	list_for_each_safe(l, ltmp, &txn->t_private_list) {
		entry = list_entry(l, struct ext4_free_data, list);

		mb_debug(1, "gonna free %u blocks in group %u (0x%p):",
			 entry->count, entry->group, entry);

		err = ext4_mb_load_buddy(sb, entry->group, &e4b);
		/* we expect to find existing buddy because it's pinned */
		BUG_ON(err != 0);

		db = e4b.bd_info;
		/* there are blocks to put in buddy to make them really free */
		count += entry->count;
		count2++;
		ext4_lock_group(sb, entry->group);
		/* Take it out of per group rb tree */
		rb_erase(&entry->node, &(db->bb_free_root));
		mb_free_blocks(NULL, &e4b, entry->start_blk, entry->count);

		if (!db->bb_free_root.rb_node) {
			/* No more items in the per group rb tree
			 * balance refcounts from ext4_mb_free_metadata()
			 */
			page_cache_release(e4b.bd_buddy_page);
			page_cache_release(e4b.bd_bitmap_page);
		}
		ext4_unlock_group(sb, entry->group);
		if (test_opt(sb, DISCARD)) {
			ext4_fsblk_t discard_block;
			struct ext4_super_block *es = EXT4_SB(sb)->s_es;

			discard_block = (ext4_fsblk_t)entry->group *
						EXT4_BLOCKS_PER_GROUP(sb)
					+ entry->start_blk
					+ le32_to_cpu(es->s_first_data_block);
			trace_ext4_discard_blocks(sb,
					(unsigned long long)discard_block,
					entry->count);
			sb_issue_discard(sb, discard_block, entry->count);
		}
		kmem_cache_free(ext4_free_ext_cachep, entry);
		ext4_mb_release_desc(&e4b);
	}

	mb_debug(1, "freed %u blocks in %u structures\n", count, count2);
}

#ifdef CONFIG_EXT4_DEBUG
u8 mb_enable_debug __read_mostly;

static struct dentry *debugfs_dir;
static struct dentry *debugfs_debug;

static void __init ext4_create_debugfs_entry(void)
{
	debugfs_dir = debugfs_create_dir("ext4", NULL);
	if (debugfs_dir)
		debugfs_debug = debugfs_create_u8("mballoc-debug",
						  S_IRUGO | S_IWUSR,
						  debugfs_dir,
						  &mb_enable_debug);
}

static void ext4_remove_debugfs_entry(void)
{
	debugfs_remove(debugfs_debug);
	debugfs_remove(debugfs_dir);
}

#else

static void __init ext4_create_debugfs_entry(void)
{
}

static void ext4_remove_debugfs_entry(void)
{
}

#endif

int __init init_ext4_mballoc(void)
{
	ext4_pspace_cachep =
		kmem_cache_create("ext4_prealloc_space",
				     sizeof(struct ext4_prealloc_space),
				     0, SLAB_RECLAIM_ACCOUNT, NULL);
	if (ext4_pspace_cachep == NULL)
		return -ENOMEM;

	ext4_ac_cachep =
		kmem_cache_create("ext4_alloc_context",
				     sizeof(struct ext4_allocation_context),
				     0, SLAB_RECLAIM_ACCOUNT, NULL);
	if (ext4_ac_cachep == NULL) {
		kmem_cache_destroy(ext4_pspace_cachep);
		return -ENOMEM;
	}

	ext4_free_ext_cachep =
		kmem_cache_create("ext4_free_block_extents",
				     sizeof(struct ext4_free_data),
				     0, SLAB_RECLAIM_ACCOUNT, NULL);
	if (ext4_free_ext_cachep == NULL) {
		kmem_cache_destroy(ext4_pspace_cachep);
		kmem_cache_destroy(ext4_ac_cachep);
		return -ENOMEM;
	}
	ext4_create_debugfs_entry();
	return 0;
}

void exit_ext4_mballoc(void)
{
	/* 
	 * Wait for completion of call_rcu()'s on ext4_pspace_cachep
	 * before destroying the slab cache.
	 */
	rcu_barrier();
	kmem_cache_destroy(ext4_pspace_cachep);
	kmem_cache_destroy(ext4_ac_cachep);
	kmem_cache_destroy(ext4_free_ext_cachep);
	ext4_remove_debugfs_entry();
}


/*
 * Check quota and mark choosed space (ac->ac_b_ex) non-free in bitmaps
 * Returns 0 if success or error code
 */
static noinline_for_stack int
ext4_mb_mark_diskspace_used(struct ext4_allocation_context *ac,
				handle_t *handle, unsigned int reserv_blks)
{
	struct buffer_head *bitmap_bh = NULL;
	struct ext4_super_block *es;
	struct ext4_group_desc *gdp;
	struct buffer_head *gdp_bh;
	struct ext4_sb_info *sbi;
	struct super_block *sb;
	ext4_fsblk_t block;
	int err, len;

	BUG_ON(ac->ac_status != AC_STATUS_FOUND);
	BUG_ON(ac->ac_b_ex.fe_len <= 0);

	sb = ac->ac_sb;
	sbi = EXT4_SB(sb);
	es = sbi->s_es;


	err = -EIO;
	bitmap_bh = ext4_read_block_bitmap(sb, ac->ac_b_ex.fe_group);
	if (!bitmap_bh)
		goto out_err;

	err = ext4_journal_get_write_access(handle, bitmap_bh);
	if (err)
		goto out_err;

	err = -EIO;
	gdp = ext4_get_group_desc(sb, ac->ac_b_ex.fe_group, &gdp_bh);
	if (!gdp)
		goto out_err;

	ext4_debug("using block group %u(%d)\n", ac->ac_b_ex.fe_group,
			ext4_free_blks_count(sb, gdp));

	err = ext4_journal_get_write_access(handle, gdp_bh);
	if (err)
		goto out_err;

	block = ac->ac_b_ex.fe_group * EXT4_BLOCKS_PER_GROUP(sb)
		+ ac->ac_b_ex.fe_start
		+ le32_to_cpu(es->s_first_data_block);

	len = ac->ac_b_ex.fe_len;
	if (!ext4_data_block_valid(sbi, block, len)) {
		ext4_error(sb, __func__,
			   "Allocating blocks %llu-%llu which overlap "
			   "fs metadata\n", block, block+len);
		/* File system mounted not to panic on error
		 * Fix the bitmap and repeat the block allocation
		 * We leak some of the blocks here.
		 */
		ext4_lock_group(sb, ac->ac_b_ex.fe_group);
		mb_set_bits(bitmap_bh->b_data, ac->ac_b_ex.fe_start,
			    ac->ac_b_ex.fe_len);
		ext4_unlock_group(sb, ac->ac_b_ex.fe_group);
		err = ext4_handle_dirty_metadata(handle, NULL, bitmap_bh);
		if (!err)
			err = -EAGAIN;
		goto out_err;
	}

	ext4_lock_group(sb, ac->ac_b_ex.fe_group);
#ifdef AGGRESSIVE_CHECK
	{
		int i;
		for (i = 0; i < ac->ac_b_ex.fe_len; i++) {
			BUG_ON(mb_test_bit(ac->ac_b_ex.fe_start + i,
						bitmap_bh->b_data));
		}
	}
#endif
	mb_set_bits(bitmap_bh->b_data, ac->ac_b_ex.fe_start,ac->ac_b_ex.fe_len);
	if (gdp->bg_flags & cpu_to_le16(EXT4_BG_BLOCK_UNINIT)) {
		gdp->bg_flags &= cpu_to_le16(~EXT4_BG_BLOCK_UNINIT);
		ext4_free_blks_set(sb, gdp,
					ext4_free_blocks_after_init(sb,
					ac->ac_b_ex.fe_group, gdp));
	}
	len = ext4_free_blks_count(sb, gdp) - ac->ac_b_ex.fe_len;
	ext4_free_blks_set(sb, gdp, len);
	gdp->bg_checksum = ext4_group_desc_csum(sbi, ac->ac_b_ex.fe_group, gdp);

	ext4_unlock_group(sb, ac->ac_b_ex.fe_group);
	percpu_counter_sub(&sbi->s_freeblocks_counter, ac->ac_b_ex.fe_len);
	/*
	 * Now reduce the dirty block count also. Should not go negative
	 */
	if (!(ac->ac_flags & EXT4_MB_DELALLOC_RESERVED))
		/* release all the reserved blocks if non delalloc */
		percpu_counter_sub(&sbi->s_dirtyblocks_counter, reserv_blks);
	else {
		percpu_counter_sub(&sbi->s_dirtyblocks_counter,
						ac->ac_b_ex.fe_len);
		/* convert reserved quota blocks to real quota blocks */
		vfs_dq_claim_block(ac->ac_inode, ac->ac_b_ex.fe_len);
	}

	if (sbi->s_log_groups_per_flex) {
		ext4_group_t flex_group = ext4_flex_group(sbi,
							  ac->ac_b_ex.fe_group);
		atomic_sub(ac->ac_b_ex.fe_len,
			   &sbi->s_flex_groups[flex_group].free_blocks);
	}

	err = ext4_handle_dirty_metadata(handle, NULL, bitmap_bh);
	if (err)
		goto out_err;
	err = ext4_handle_dirty_metadata(handle, NULL, gdp_bh);

out_err:
	sb->s_dirt = 1;
	brelse(bitmap_bh);
	return err;
}

/*
 * here we normalize request for locality group
 * Group request are normalized to s_strip size if we set the same via mount
 * option. If not we set it to s_mb_group_prealloc which can be configured via
 * /sys/fs/ext4/<partition>/mb_group_prealloc
 *
 * XXX: should we try to preallocate more than the group has now?
 */
static void ext4_mb_normalize_group_request(struct ext4_allocation_context *ac)
{
	struct super_block *sb = ac->ac_sb;
	struct ext4_locality_group *lg = ac->ac_lg;

	BUG_ON(lg == NULL);
	if (EXT4_SB(sb)->s_stripe)
		ac->ac_g_ex.fe_len = EXT4_SB(sb)->s_stripe;
	else
		ac->ac_g_ex.fe_len = EXT4_SB(sb)->s_mb_group_prealloc;
	mb_debug(1, "#%u: goal %u blocks for locality group\n",
		current->pid, ac->ac_g_ex.fe_len);
}

/*
 * Normalization means making request better in terms of
 * size and alignment
 */
static noinline_for_stack void
ext4_mb_normalize_request(struct ext4_allocation_context *ac,
				struct ext4_allocation_request *ar)
{
	int bsbits, max;
	ext4_lblk_t end;
	loff_t size, orig_size, start_off;
	ext4_lblk_t start, orig_start;
	struct ext4_inode_info *ei = EXT4_I(ac->ac_inode);
	struct ext4_prealloc_space *pa;

	/* do normalize only data requests, metadata requests
	   do not need preallocation */
	if (!(ac->ac_flags & EXT4_MB_HINT_DATA))
		return;

	/* sometime caller may want exact blocks */
	if (unlikely(ac->ac_flags & EXT4_MB_HINT_GOAL_ONLY))
		return;

	/* caller may indicate that preallocation isn't
	 * required (it's a tail, for example) */
	if (ac->ac_flags & EXT4_MB_HINT_NOPREALLOC)
		return;

	if (ac->ac_flags & EXT4_MB_HINT_GROUP_ALLOC) {
		ext4_mb_normalize_group_request(ac);
		return ;
	}

	bsbits = ac->ac_sb->s_blocksize_bits;

	/* first, let's learn actual file size
	 * given current request is allocated */
	size = ac->ac_o_ex.fe_logical + ac->ac_o_ex.fe_len;
	size = size << bsbits;
	if (size < i_size_read(ac->ac_inode))
		size = i_size_read(ac->ac_inode);

	/* max size of free chunks */
	max = 2 << bsbits;

#define NRL_CHECK_SIZE(req, size, max, chunk_size)	\
		(req <= (size) || max <= (chunk_size))

	/* first, try to predict filesize */
	/* XXX: should this table be tunable? */
	start_off = 0;
	if (size <= 16 * 1024) {
		size = 16 * 1024;
	} else if (size <= 32 * 1024) {
		size = 32 * 1024;
	} else if (size <= 64 * 1024) {
		size = 64 * 1024;
	} else if (size <= 128 * 1024) {
		size = 128 * 1024;
	} else if (size <= 256 * 1024) {
		size = 256 * 1024;
	} else if (size <= 512 * 1024) {
		size = 512 * 1024;
	} else if (size <= 1024 * 1024) {
		size = 1024 * 1024;
	} else if (NRL_CHECK_SIZE(size, 4 * 1024 * 1024, max, 2 * 1024)) {
		start_off = ((loff_t)ac->ac_o_ex.fe_logical >>
						(21 - bsbits)) << 21;
		size = 2 * 1024 * 1024;
	} else if (NRL_CHECK_SIZE(size, 8 * 1024 * 1024, max, 4 * 1024)) {
		start_off = ((loff_t)ac->ac_o_ex.fe_logical >>
							(22 - bsbits)) << 22;
		size = 4 * 1024 * 1024;
	} else if (NRL_CHECK_SIZE(ac->ac_o_ex.fe_len,
					(8<<20)>>bsbits, max, 8 * 1024)) {
		start_off = ((loff_t)ac->ac_o_ex.fe_logical >>
							(23 - bsbits)) << 23;
		size = 8 * 1024 * 1024;
	} else {
		start_off = (loff_t)ac->ac_o_ex.fe_logical << bsbits;
		size	  = ac->ac_o_ex.fe_len << bsbits;
	}
	orig_size = size = size >> bsbits;
	orig_start = start = start_off >> bsbits;

	/* don't cover already allocated blocks in selected range */
	if (ar->pleft && start <= ar->lleft) {
		size -= ar->lleft + 1 - start;
		start = ar->lleft + 1;
	}
	if (ar->pright && start + size - 1 >= ar->lright)
		size -= start + size - ar->lright;

	end = start + size;

	/* check we don't cross already preallocated blocks */
	rcu_read_lock();
	list_for_each_entry_rcu(pa, &ei->i_prealloc_list, pa_inode_list) {
		ext4_lblk_t pa_end;

		if (pa->pa_deleted)
			continue;
		spin_lock(&pa->pa_lock);
		if (pa->pa_deleted) {
			spin_unlock(&pa->pa_lock);
			continue;
		}

		pa_end = pa->pa_lstart + pa->pa_len;

		/* PA must not overlap original request */
		BUG_ON(!(ac->ac_o_ex.fe_logical >= pa_end ||
			ac->ac_o_ex.fe_logical < pa->pa_lstart));

		/* skip PAs this normalized request doesn't overlap with */
		if (pa->pa_lstart >= end || pa_end <= start) {
			spin_unlock(&pa->pa_lock);
			continue;
		}
		BUG_ON(pa->pa_lstart <= start && pa_end >= end);

		/* adjust start or end to be adjacent to this pa */
		if (pa_end <= ac->ac_o_ex.fe_logical) {
			BUG_ON(pa_end < start);
			start = pa_end;
		} else if (pa->pa_lstart > ac->ac_o_ex.fe_logical) {
			BUG_ON(pa->pa_lstart > end);
			end = pa->pa_lstart;
		}
		spin_unlock(&pa->pa_lock);
	}
	rcu_read_unlock();
	size = end - start;

	/* XXX: extra loop to check we really don't overlap preallocations */
	rcu_read_lock();
	list_for_each_entry_rcu(pa, &ei->i_prealloc_list, pa_inode_list) {
		ext4_lblk_t pa_end;
		spin_lock(&pa->pa_lock);
		if (pa->pa_deleted == 0) {
			pa_end = pa->pa_lstart + pa->pa_len;
			BUG_ON(!(start >= pa_end || end <= pa->pa_lstart));
		}
		spin_unlock(&pa->pa_lock);
	}
	rcu_read_unlock();

	if (start + size <= ac->ac_o_ex.fe_logical &&
			start > ac->ac_o_ex.fe_logical) {
		printk(KERN_ERR "start %lu, size %lu, fe_logical %lu\n",
			(unsigned long) start, (unsigned long) size,
			(unsigned long) ac->ac_o_ex.fe_logical);
	}
	BUG_ON(start + size <= ac->ac_o_ex.fe_logical &&
			start > ac->ac_o_ex.fe_logical);
	BUG_ON(size <= 0 || size > EXT4_BLOCKS_PER_GROUP(ac->ac_sb));

	/* now prepare goal request */

	/* XXX: is it better to align blocks WRT to logical
	 * placement or satisfy big request as is */
	ac->ac_g_ex.fe_logical = start;
	ac->ac_g_ex.fe_len = size;

	/* define goal start in order to merge */
	if (ar->pright && (ar->lright == (start + size))) {
		/* merge to the right */
		ext4_get_group_no_and_offset(ac->ac_sb, ar->pright - size,
						&ac->ac_f_ex.fe_group,
						&ac->ac_f_ex.fe_start);
		ac->ac_flags |= EXT4_MB_HINT_TRY_GOAL;
	}
	if (ar->pleft && (ar->lleft + 1 == start)) {
		/* merge to the left */
		ext4_get_group_no_and_offset(ac->ac_sb, ar->pleft + 1,
						&ac->ac_f_ex.fe_group,
						&ac->ac_f_ex.fe_start);
		ac->ac_flags |= EXT4_MB_HINT_TRY_GOAL;
	}

	mb_debug(1, "goal: %u(was %u) blocks at %u\n", (unsigned) size,
		(unsigned) orig_size, (unsigned) start);
}

static void ext4_mb_collect_stats(struct ext4_allocation_context *ac)
{
	struct ext4_sb_info *sbi = EXT4_SB(ac->ac_sb);

	if (sbi->s_mb_stats && ac->ac_g_ex.fe_len > 1) {
		atomic_inc(&sbi->s_bal_reqs);
		atomic_add(ac->ac_b_ex.fe_len, &sbi->s_bal_allocated);
		if (ac->ac_o_ex.fe_len >= ac->ac_g_ex.fe_len)
			atomic_inc(&sbi->s_bal_success);
		atomic_add(ac->ac_found, &sbi->s_bal_ex_scanned);
		if (ac->ac_g_ex.fe_start == ac->ac_b_ex.fe_start &&
				ac->ac_g_ex.fe_group == ac->ac_b_ex.fe_group)
			atomic_inc(&sbi->s_bal_goals);
		if (ac->ac_found > sbi->s_mb_max_to_scan)
			atomic_inc(&sbi->s_bal_breaks);
	}

	if (ac->ac_op == EXT4_MB_HISTORY_ALLOC)
		trace_ext4_mballoc_alloc(ac);
	else
		trace_ext4_mballoc_prealloc(ac);
}

/*
 * Called on failure; free up any blocks from the inode PA for this
 * context.  We don't need this for MB_GROUP_PA because we only change
 * pa_free in ext4_mb_release_context(), but on failure, we've already
 * zeroed out ac->ac_b_ex.fe_len, so group_pa->pa_free is not changed.
 */
static void ext4_discard_allocated_blocks(struct ext4_allocation_context *ac)
{
	struct ext4_prealloc_space *pa = ac->ac_pa;
	int len;

	if (pa && pa->pa_type == MB_INODE_PA) {
		len = ac->ac_b_ex.fe_len;
		pa->pa_free += len;
	}

}

/*
 * use blocks preallocated to inode
 */
static void ext4_mb_use_inode_pa(struct ext4_allocation_context *ac,
				struct ext4_prealloc_space *pa)
{
	ext4_fsblk_t start;
	ext4_fsblk_t end;
	int len;

	/* found preallocated blocks, use them */
	start = pa->pa_pstart + (ac->ac_o_ex.fe_logical - pa->pa_lstart);
	end = min(pa->pa_pstart + pa->pa_len, start + ac->ac_o_ex.fe_len);
	len = end - start;
	ext4_get_group_no_and_offset(ac->ac_sb, start, &ac->ac_b_ex.fe_group,
					&ac->ac_b_ex.fe_start);
	ac->ac_b_ex.fe_len = len;
	ac->ac_status = AC_STATUS_FOUND;
	ac->ac_pa = pa;

	BUG_ON(start < pa->pa_pstart);
	BUG_ON(start + len > pa->pa_pstart + pa->pa_len);
	BUG_ON(pa->pa_free < len);
	pa->pa_free -= len;

	mb_debug(1, "use %llu/%u from inode pa %p\n", start, len, pa);
}

/*
 * use blocks preallocated to locality group
 */
static void ext4_mb_use_group_pa(struct ext4_allocation_context *ac,
				struct ext4_prealloc_space *pa)
{
	unsigned int len = ac->ac_o_ex.fe_len;

	ext4_get_group_no_and_offset(ac->ac_sb, pa->pa_pstart,
					&ac->ac_b_ex.fe_group,
					&ac->ac_b_ex.fe_start);
	ac->ac_b_ex.fe_len = len;
	ac->ac_status = AC_STATUS_FOUND;
	ac->ac_pa = pa;

	/* we don't correct pa_pstart or pa_plen here to avoid
	 * possible race when the group is being loaded concurrently
	 * instead we correct pa later, after blocks are marked
	 * in on-disk bitmap -- see ext4_mb_release_context()
	 * Other CPUs are prevented from allocating from this pa by lg_mutex
	 */
	mb_debug(1, "use %u/%u from group pa %p\n", pa->pa_lstart-len, len, pa);
}

/*
 * Return the prealloc space that have minimal distance
 * from the goal block. @cpa is the prealloc
 * space that is having currently known minimal distance
 * from the goal block.
 */
static struct ext4_prealloc_space *
ext4_mb_check_group_pa(ext4_fsblk_t goal_block,
			struct ext4_prealloc_space *pa,
			struct ext4_prealloc_space *cpa)
{
	ext4_fsblk_t cur_distance, new_distance;

	if (cpa == NULL) {
		atomic_inc(&pa->pa_count);
		return pa;
	}
	cur_distance = abs(goal_block - cpa->pa_pstart);
	new_distance = abs(goal_block - pa->pa_pstart);

	if (cur_distance < new_distance)
		return cpa;

	/* drop the previous reference */
	atomic_dec(&cpa->pa_count);
	atomic_inc(&pa->pa_count);
	return pa;
}

/*
 * search goal blocks in preallocated space
 */
static noinline_for_stack int
ext4_mb_use_preallocated(struct ext4_allocation_context *ac)
{
	int order, i;
	struct ext4_inode_info *ei = EXT4_I(ac->ac_inode);
	struct ext4_locality_group *lg;
	struct ext4_prealloc_space *pa, *cpa = NULL;
	ext4_fsblk_t goal_block;

	/* only data can be preallocated */
	if (!(ac->ac_flags & EXT4_MB_HINT_DATA))
		return 0;

	/* first, try per-file preallocation */
	rcu_read_lock();
	list_for_each_entry_rcu(pa, &ei->i_prealloc_list, pa_inode_list) {

		/* all fields in this condition don't change,
		 * so we can skip locking for them */
		if (ac->ac_o_ex.fe_logical < pa->pa_lstart ||
			ac->ac_o_ex.fe_logical >= pa->pa_lstart + pa->pa_len)
			continue;

		/* non-extent files can't have physical blocks past 2^32 */
		if (!(EXT4_I(ac->ac_inode)->i_flags & EXT4_EXTENTS_FL) &&
			pa->pa_pstart + pa->pa_len > EXT4_MAX_BLOCK_FILE_PHYS)
			continue;

		/* found preallocated blocks, use them */
		spin_lock(&pa->pa_lock);
		if (pa->pa_deleted == 0 && pa->pa_free) {
			atomic_inc(&pa->pa_count);
			ext4_mb_use_inode_pa(ac, pa);
			spin_unlock(&pa->pa_lock);
			ac->ac_criteria = 10;
			rcu_read_unlock();
			return 1;
		}
		spin_unlock(&pa->pa_lock);
	}
	rcu_read_unlock();

	/* can we use group allocation? */
	if (!(ac->ac_flags & EXT4_MB_HINT_GROUP_ALLOC))
		return 0;

	/* inode may have no locality group for some reason */
	lg = ac->ac_lg;
	if (lg == NULL)
		return 0;
	order  = fls(ac->ac_o_ex.fe_len) - 1;
	if (order > PREALLOC_TB_SIZE - 1)
		/* The max size of hash table is PREALLOC_TB_SIZE */
		order = PREALLOC_TB_SIZE - 1;

	goal_block = ac->ac_g_ex.fe_group * EXT4_BLOCKS_PER_GROUP(ac->ac_sb) +
		     ac->ac_g_ex.fe_start +
		     le32_to_cpu(EXT4_SB(ac->ac_sb)->s_es->s_first_data_block);
	/*
	 * search for the prealloc space that is having
	 * minimal distance from the goal block.
	 */
	for (i = order; i < PREALLOC_TB_SIZE; i++) {
		rcu_read_lock();
		list_for_each_entry_rcu(pa, &lg->lg_prealloc_list[i],
					pa_inode_list) {
			spin_lock(&pa->pa_lock);
			if (pa->pa_deleted == 0 &&
					pa->pa_free >= ac->ac_o_ex.fe_len) {

				cpa = ext4_mb_check_group_pa(goal_block,
								pa, cpa);
			}
			spin_unlock(&pa->pa_lock);
		}
		rcu_read_unlock();
	}
	if (cpa) {
		ext4_mb_use_group_pa(ac, cpa);
		ac->ac_criteria = 20;
		return 1;
	}
	return 0;
}

/*
 * the function goes through all block freed in the group
 * but not yet committed and marks them used in in-core bitmap.
 * buddy must be generated from this bitmap
 * Need to be called with the ext4 group lock held
 */
static void ext4_mb_generate_from_freelist(struct super_block *sb, void *bitmap,
						ext4_group_t group)
{
	struct rb_node *n;
	struct ext4_group_info *grp;
	struct ext4_free_data *entry;

	grp = ext4_get_group_info(sb, group);
	n = rb_first(&(grp->bb_free_root));

	while (n) {
		entry = rb_entry(n, struct ext4_free_data, node);
		mb_set_bits(bitmap, entry->start_blk, entry->count);
		n = rb_next(n);
	}
	return;
}

/*
 * the function goes through all preallocation in this group and marks them
 * used in in-core bitmap. buddy must be generated from this bitmap
 * Need to be called with ext4 group lock held
 */
static noinline_for_stack
void ext4_mb_generate_from_pa(struct super_block *sb, void *bitmap,
					ext4_group_t group)
{
	struct ext4_group_info *grp = ext4_get_group_info(sb, group);
	struct ext4_prealloc_space *pa;
	struct list_head *cur;
	ext4_group_t groupnr;
	ext4_grpblk_t start;
	int preallocated = 0;
	int count = 0;
	int len;

	/* all form of preallocation discards first load group,
	 * so the only competing code is preallocation use.
	 * we don't need any locking here
	 * notice we do NOT ignore preallocations with pa_deleted
	 * otherwise we could leave used blocks available for
	 * allocation in buddy when concurrent ext4_mb_put_pa()
	 * is dropping preallocation
	 */
	list_for_each(cur, &grp->bb_prealloc_list) {
		pa = list_entry(cur, struct ext4_prealloc_space, pa_group_list);
		spin_lock(&pa->pa_lock);
		ext4_get_group_no_and_offset(sb, pa->pa_pstart,
					     &groupnr, &start);
		len = pa->pa_len;
		spin_unlock(&pa->pa_lock);
		if (unlikely(len == 0))
			continue;
		BUG_ON(groupnr != group);
		mb_set_bits(bitmap, start, len);
		preallocated += len;
		count++;
	}
	mb_debug(1, "prellocated %u for group %u\n", preallocated, group);
}

static void ext4_mb_pa_callback(struct rcu_head *head)
{
	struct ext4_prealloc_space *pa;
	pa = container_of(head, struct ext4_prealloc_space, u.pa_rcu);
	kmem_cache_free(ext4_pspace_cachep, pa);
}

/*
 * drops a reference to preallocated space descriptor
 * if this was the last reference and the space is consumed
 */
static void ext4_mb_put_pa(struct ext4_allocation_context *ac,
			struct super_block *sb, struct ext4_prealloc_space *pa)
{
	ext4_group_t grp;
	ext4_fsblk_t grp_blk;

	if (!atomic_dec_and_test(&pa->pa_count) || pa->pa_free != 0)
		return;

	/* in this short window concurrent discard can set pa_deleted */
	spin_lock(&pa->pa_lock);
	if (pa->pa_deleted == 1) {
		spin_unlock(&pa->pa_lock);
		return;
	}

	pa->pa_deleted = 1;
	spin_unlock(&pa->pa_lock);

	grp_blk = pa->pa_pstart;
	/* 
	 * If doing group-based preallocation, pa_pstart may be in the
	 * next group when pa is used up
	 */
	if (pa->pa_type == MB_GROUP_PA)
		grp_blk--;

	ext4_get_group_no_and_offset(sb, grp_blk, &grp, NULL);

	/*
	 * possible race:
	 *
	 *  P1 (buddy init)			P2 (regular allocation)
	 *					find block B in PA
	 *  copy on-disk bitmap to buddy
	 *  					mark B in on-disk bitmap
	 *					drop PA from group
	 *  mark all PAs in buddy
	 *
	 * thus, P1 initializes buddy with B available. to prevent this
	 * we make "copy" and "mark all PAs" atomic and serialize "drop PA"
	 * against that pair
	 */
	ext4_lock_group(sb, grp);
	list_del(&pa->pa_group_list);
	ext4_unlock_group(sb, grp);

	spin_lock(pa->pa_obj_lock);
	list_del_rcu(&pa->pa_inode_list);
	spin_unlock(pa->pa_obj_lock);

	call_rcu(&(pa)->u.pa_rcu, ext4_mb_pa_callback);
}

/*
 * creates new preallocated space for given inode
 */
static noinline_for_stack int
ext4_mb_new_inode_pa(struct ext4_allocation_context *ac)
{
	struct super_block *sb = ac->ac_sb;
	struct ext4_prealloc_space *pa;
	struct ext4_group_info *grp;
	struct ext4_inode_info *ei;

	/* preallocate only when found space is larger then requested */
	BUG_ON(ac->ac_o_ex.fe_len >= ac->ac_b_ex.fe_len);
	BUG_ON(ac->ac_status != AC_STATUS_FOUND);
	BUG_ON(!S_ISREG(ac->ac_inode->i_mode));

	pa = kmem_cache_alloc(ext4_pspace_cachep, GFP_NOFS);
	if (pa == NULL)
		return -ENOMEM;

	if (ac->ac_b_ex.fe_len < ac->ac_g_ex.fe_len) {
		int winl;
		int wins;
		int win;
		int offs;

		/* we can't allocate as much as normalizer wants.
		 * so, found space must get proper lstart
		 * to cover original request */
		BUG_ON(ac->ac_g_ex.fe_logical > ac->ac_o_ex.fe_logical);
		BUG_ON(ac->ac_g_ex.fe_len < ac->ac_o_ex.fe_len);

		/* we're limited by original request in that
		 * logical block must be covered any way
		 * winl is window we can move our chunk within */
		winl = ac->ac_o_ex.fe_logical - ac->ac_g_ex.fe_logical;

		/* also, we should cover whole original request */
		wins = ac->ac_b_ex.fe_len - ac->ac_o_ex.fe_len;

		/* the smallest one defines real window */
		win = min(winl, wins);

		offs = ac->ac_o_ex.fe_logical % ac->ac_b_ex.fe_len;
		if (offs && offs < win)
			win = offs;

		ac->ac_b_ex.fe_logical = ac->ac_o_ex.fe_logical - win;
		BUG_ON(ac->ac_o_ex.fe_logical < ac->ac_b_ex.fe_logical);
		BUG_ON(ac->ac_o_ex.fe_len > ac->ac_b_ex.fe_len);
	}

	/* preallocation can change ac_b_ex, thus we store actually
	 * allocated blocks for history */
	ac->ac_f_ex = ac->ac_b_ex;

	pa->pa_lstart = ac->ac_b_ex.fe_logical;
	pa->pa_pstart = ext4_grp_offs_to_block(sb, &ac->ac_b_ex);
	pa->pa_len = ac->ac_b_ex.fe_len;
	pa->pa_free = pa->pa_len;
	atomic_set(&pa->pa_count, 1);
	spin_lock_init(&pa->pa_lock);
	INIT_LIST_HEAD(&pa->pa_inode_list);
	INIT_LIST_HEAD(&pa->pa_group_list);
	pa->pa_deleted = 0;
	pa->pa_type = MB_INODE_PA;

	mb_debug(1, "new inode pa %p: %llu/%u for %u\n", pa,
			pa->pa_pstart, pa->pa_len, pa->pa_lstart);
	trace_ext4_mb_new_inode_pa(ac, pa);

	ext4_mb_use_inode_pa(ac, pa);
	atomic_add(pa->pa_free, &EXT4_SB(sb)->s_mb_preallocated);

	ei = EXT4_I(ac->ac_inode);
	grp = ext4_get_group_info(sb, ac->ac_b_ex.fe_group);

	pa->pa_obj_lock = &ei->i_prealloc_lock;
	pa->pa_inode = ac->ac_inode;

	ext4_lock_group(sb, ac->ac_b_ex.fe_group);
	list_add(&pa->pa_group_list, &grp->bb_prealloc_list);
	ext4_unlock_group(sb, ac->ac_b_ex.fe_group);

	spin_lock(pa->pa_obj_lock);
	list_add_rcu(&pa->pa_inode_list, &ei->i_prealloc_list);
	spin_unlock(pa->pa_obj_lock);

	return 0;
}

/*
 * creates new preallocated space for locality group inodes belongs to
 */
static noinline_for_stack int
ext4_mb_new_group_pa(struct ext4_allocation_context *ac)
{
	struct super_block *sb = ac->ac_sb;
	struct ext4_locality_group *lg;
	struct ext4_prealloc_space *pa;
	struct ext4_group_info *grp;

	/* preallocate only when found space is larger then requested */
	BUG_ON(ac->ac_o_ex.fe_len >= ac->ac_b_ex.fe_len);
	BUG_ON(ac->ac_status != AC_STATUS_FOUND);
	BUG_ON(!S_ISREG(ac->ac_inode->i_mode));

	BUG_ON(ext4_pspace_cachep == NULL);
	pa = kmem_cache_alloc(ext4_pspace_cachep, GFP_NOFS);
	if (pa == NULL)
		return -ENOMEM;

	/* preallocation can change ac_b_ex, thus we store actually
	 * allocated blocks for history */
	ac->ac_f_ex = ac->ac_b_ex;

	pa->pa_pstart = ext4_grp_offs_to_block(sb, &ac->ac_b_ex);
	pa->pa_lstart = pa->pa_pstart;
	pa->pa_len = ac->ac_b_ex.fe_len;
	pa->pa_free = pa->pa_len;
	atomic_set(&pa->pa_count, 1);
	spin_lock_init(&pa->pa_lock);
	INIT_LIST_HEAD(&pa->pa_inode_list);
	INIT_LIST_HEAD(&pa->pa_group_list);
	pa->pa_deleted = 0;
	pa->pa_type = MB_GROUP_PA;

	mb_debug(1, "new group pa %p: %llu/%u for %u\n", pa,
			pa->pa_pstart, pa->pa_len, pa->pa_lstart);
	trace_ext4_mb_new_group_pa(ac, pa);

	ext4_mb_use_group_pa(ac, pa);
	atomic_add(pa->pa_free, &EXT4_SB(sb)->s_mb_preallocated);

	grp = ext4_get_group_info(sb, ac->ac_b_ex.fe_group);
	lg = ac->ac_lg;
	BUG_ON(lg == NULL);

	pa->pa_obj_lock = &lg->lg_prealloc_lock;
	pa->pa_inode = NULL;

	ext4_lock_group(sb, ac->ac_b_ex.fe_group);
	list_add(&pa->pa_group_list, &grp->bb_prealloc_list);
	ext4_unlock_group(sb, ac->ac_b_ex.fe_group);

	/*
	 * We will later add the new pa to the right bucket
	 * after updating the pa_free in ext4_mb_release_context
	 */
	return 0;
}

static int ext4_mb_new_preallocation(struct ext4_allocation_context *ac)
{
	int err;

	if (ac->ac_flags & EXT4_MB_HINT_GROUP_ALLOC)
		err = ext4_mb_new_group_pa(ac);
	else
		err = ext4_mb_new_inode_pa(ac);
	return err;
}

/*
 * finds all unused blocks in on-disk bitmap, frees them in
 * in-core bitmap and buddy.
 * @pa must be unlinked from inode and group lists, so that
 * nobody else can find/use it.
 * the caller MUST hold group/inode locks.
 * TODO: optimize the case when there are no in-core structures yet
 */
static noinline_for_stack int
ext4_mb_release_inode_pa(struct ext4_buddy *e4b, struct buffer_head *bitmap_bh,
			struct ext4_prealloc_space *pa,
			struct ext4_allocation_context *ac)
{
	struct super_block *sb = e4b->bd_sb;
	struct ext4_sb_info *sbi = EXT4_SB(sb);
	unsigned int end;
	unsigned int next;
	ext4_group_t group;
	ext4_grpblk_t bit;
	unsigned long long grp_blk_start;
	sector_t start;
	int err = 0;
	int free = 0;

	BUG_ON(pa->pa_deleted == 0);
	ext4_get_group_no_and_offset(sb, pa->pa_pstart, &group, &bit);
	grp_blk_start = pa->pa_pstart - bit;
	BUG_ON(group != e4b->bd_group && pa->pa_len != 0);
	end = bit + pa->pa_len;

	if (ac) {
		ac->ac_sb = sb;
		ac->ac_inode = pa->pa_inode;
	}

	while (bit < end) {
		bit = mb_find_next_zero_bit(bitmap_bh->b_data, end, bit);
		if (bit >= end)
			break;
		next = mb_find_next_bit(bitmap_bh->b_data, end, bit);
		start = group * EXT4_BLOCKS_PER_GROUP(sb) + bit +
				le32_to_cpu(sbi->s_es->s_first_data_block);
		mb_debug(1, "    free preallocated %u/%u in group %u\n",
				(unsigned) start, (unsigned) next - bit,
				(unsigned) group);
		free += next - bit;

		if (ac) {
			ac->ac_b_ex.fe_group = group;
			ac->ac_b_ex.fe_start = bit;
			ac->ac_b_ex.fe_len = next - bit;
			ac->ac_b_ex.fe_logical = 0;
			trace_ext4_mballoc_discard(ac);
		}

		trace_ext4_mb_release_inode_pa(ac, pa, grp_blk_start + bit,
					       next - bit);
		mb_free_blocks(pa->pa_inode, e4b, bit, next - bit);
		bit = next + 1;
	}
	if (free != pa->pa_free) {
		printk(KERN_CRIT "pa %p: logic %lu, phys. %lu, len %lu\n",
			pa, (unsigned long) pa->pa_lstart,
			(unsigned long) pa->pa_pstart,
			(unsigned long) pa->pa_len);
		ext4_grp_locked_error(sb, group,
					__func__, "free %u, pa_free %u",
					free, pa->pa_free);
		/*
		 * pa is already deleted so we use the value obtained
		 * from the bitmap and continue.
		 */
	}
	atomic_add(free, &sbi->s_mb_discarded);

	return err;
}

static noinline_for_stack int
ext4_mb_release_group_pa(struct ext4_buddy *e4b,
				struct ext4_prealloc_space *pa,
				struct ext4_allocation_context *ac)
{
	struct super_block *sb = e4b->bd_sb;
	ext4_group_t group;
	ext4_grpblk_t bit;

	trace_ext4_mb_release_group_pa(ac, pa);
	BUG_ON(pa->pa_deleted == 0);
	ext4_get_group_no_and_offset(sb, pa->pa_pstart, &group, &bit);
	BUG_ON(group != e4b->bd_group && pa->pa_len != 0);
	mb_free_blocks(pa->pa_inode, e4b, bit, pa->pa_len);
	atomic_add(pa->pa_len, &EXT4_SB(sb)->s_mb_discarded);

	if (ac) {
		ac->ac_sb = sb;
		ac->ac_inode = NULL;
		ac->ac_b_ex.fe_group = group;
		ac->ac_b_ex.fe_start = bit;
		ac->ac_b_ex.fe_len = pa->pa_len;
		ac->ac_b_ex.fe_logical = 0;
		trace_ext4_mballoc_discard(ac);
	}

	return 0;
}

/*
 * releases all preallocations in given group
 *
 * first, we need to decide discard policy:
 * - when do we discard
 *   1) ENOSPC
 * - how many do we discard
 *   1) how many requested
 */
static noinline_for_stack int
ext4_mb_discard_group_preallocations(struct super_block *sb,
					ext4_group_t group, int needed)
{
	struct ext4_group_info *grp = ext4_get_group_info(sb, group);
	struct buffer_head *bitmap_bh = NULL;
	struct ext4_prealloc_space *pa, *tmp;
	struct ext4_allocation_context *ac;
	struct list_head list;
	struct ext4_buddy e4b;
	int err;
	int busy = 0;
	int free = 0;

	mb_debug(1, "discard preallocation for group %u\n", group);

	if (list_empty(&grp->bb_prealloc_list))
		return 0;

	bitmap_bh = ext4_read_block_bitmap(sb, group);
	if (bitmap_bh == NULL) {
		ext4_error(sb, __func__, "Error in reading block "
				"bitmap for %u", group);
		return 0;
	}

	err = ext4_mb_load_buddy(sb, group, &e4b);
	if (err) {
		ext4_error(sb, __func__, "Error in loading buddy "
				"information for %u", group);
		put_bh(bitmap_bh);
		return 0;
	}

	if (needed == 0)
		needed = EXT4_BLOCKS_PER_GROUP(sb) + 1;

	INIT_LIST_HEAD(&list);
	ac = kmem_cache_alloc(ext4_ac_cachep, GFP_NOFS);
	if (ac)
		ac->ac_sb = sb;
repeat:
	ext4_lock_group(sb, group);
	list_for_each_entry_safe(pa, tmp,
				&grp->bb_prealloc_list, pa_group_list) {
		spin_lock(&pa->pa_lock);
		if (atomic_read(&pa->pa_count)) {
			spin_unlock(&pa->pa_lock);
			busy = 1;
			continue;
		}
		if (pa->pa_deleted) {
			spin_unlock(&pa->pa_lock);
			continue;
		}

		/* seems this one can be freed ... */
		pa->pa_deleted = 1;

		/* we can trust pa_free ... */
		free += pa->pa_free;

		spin_unlock(&pa->pa_lock);

		list_del(&pa->pa_group_list);
		list_add(&pa->u.pa_tmp_list, &list);
	}

	/* if we still need more blocks and some PAs were used, try again */
	if (free < needed && busy) {
		busy = 0;
		ext4_unlock_group(sb, group);
		/*
		 * Yield the CPU here so that we don't get soft lockup
		 * in non preempt case.
		 */
		yield();
		goto repeat;
	}

	/* found anything to free? */
	if (list_empty(&list)) {
		BUG_ON(free != 0);
		goto out;
	}

	/* now  (c) all selected PAs */
	list_for_each_entry_safe(pa, tmp, & * W, u.pa_tmp_terf
 * 
	 Sysremove from object (inode or locality group).com
	spin_lock(pa->pa_objfy
 *-2006terfrdel_rcu(&it undeou ca * Thims omodifune te ral Pubr the terms
		if * publistype == MB_GROUP_PA)
			ext4_mb_release_e* pu_pa(&e4b, <aleace verelseis program is distrilic Liined byhobitmap_bhhope that,
 *ofed byGNneral com>
 *s
 *nse vercalNU Gens <a)anty ofrcu, useful,
pa_BILIba the  }

out:
 usefu2 as
 ibuted(sb,tbuteda;
ee Soachis kmem_cache_, In(usefuacs
 *Yop even ttuseful,
 * but Wdesced byr moput_bh(ANTY; witic Lreturns, In;
}

/*
 ul,
 but sc, innon-used preal butustebe tes for given lic L
 not, It's important to* bucarftwares
 *Foionsublishei_data_semace,We don't w Bosanotherdat
/*
tonbe servedee softhe2111-
 */ace,sphat when we aren, MA  0ingcluublic Ln111-
 */"
#inc. Place,FIXME!! Make surbutedis valid at the inclRPOS sites
nd/ovoidARTICUebugfs._111-
.comif no(structincl <t*lic L)
{
	 Inc.meusefuITHOUT Afo *ei = EXT4_Iylic Lic Lups
 *
super_ation *sb =etadat->i_sbould tDO:
buffer_head ** along w = NULLstill openusefu111-
 */_ts/exl*pe t*tmps if no  (c) sp*   - sear_context *accop FOR blic  tnse fo = 0still openf then, MAterf- don't normalibuddy e4 stiint erhed bre d!S_ISREGtion sle imn shs
 *Co/*pyright!f theempty(&eien)
hat left MERCH);d/ohed  be u Fch gmb_debug(1, "ebugfs.2)
 *   - sear Inc.ude <t%lu\n",her fch grins
 *otrace_search_n/max()s
 *  - ee of ountshou
	INIT_LIST_HEAD(sterf (c) ac = .f
 * MEYou-
 */tillhave receivedGFP_NOFSith th detLiceCoac->ac_wheths (prtiple nuude <tallo fiet evrepea* GN/* firrfs.col@clu the pauitenpolictadatad/orsion as
 *n byte it  Inc.bby the  whch gx extents ine allocator we dectter*
 *Copa = * MERAlex (allocation or inode.next, be don't normalioup for b(polihoutbITHOUTcenst it min/max published by t 200c, iocator<lindecidestonhasee implieral Pubn red bycest foomic_readle size, counetware
llut evisf freldmulthapp9 Teften - nobody->sam iss  betus<  - t/evete chuust evwe'x/ mb_mfs.h>
#itnroup rsounteral P*If the swhich eve_stream_requerrorfter allion,which eve	printk(KERN_ERR "uh-oh!te id paon. Thelt value o\n"s/fs/eWARN/max1ckse vachedule_timeout_uninterruptible(HZnted i, Cluy
 *) ch ev}F(c) So02111- deleound=003-lese tn. Tcator  less  1 of nuise <t also  test16 by
 *of nimplied TY or rranPuthe resulting fier on ad. Iimplty of
 * MERCloundrft eve ed binugoaln
 * thansomeonetestensure ofpa n/maxstemsroup sion an also bs closer togethave small fileciduned vias
 */sys/fode_inweation rouwait here becae it * ensure 
oc lidoesstremean16 blo thger.y unlink - s#instart lizafortedasc spmch cobh_for  be uess oc s->clear nfor ()itializatiowill getrograllloc sand concurreBost blocThe chllocInc.thinglloc spa

#inclic L's-
 */ may accesoc sr togeloc senist,memory, bad-al starins eallocXXX: ifn sbi-can als toolloc
w,starsmal-> lendd a flag rouforiali reponly inic thabellocfy
 * Inc.selecti, bu   -metain th ofalls wregular trun Fouion lismb allfer togetf
 * MERCe main *  ivatin Inc.havingc, inselectarticnsumeou cae valsical al stf theitt weby pendiTomasope tx
 *
prealfied of
 * MERCHs ping b the w-
 */

und!ia
 .INODEMERCe siw - reetrees
 ano_and_offsetesultral Pubppacepresle si,ccountl staed b=_searcLARloadead-ahesultingfo,
 * Firsterquesrrcountis * weerroresult__func__, "Eou c the e.
e of*
 * e"ile. 	"s
 *rmocationk r%u"we are list * W, the ograou carftware
r aln thebloca
 *un_* alonIf we are listo finoup
 * prea via
  togep prfile representart bl the nghtif wblocationcatiocator al* alonag setxt4_ <linlook a  - ron the disk.
 *
 * First at the locality grn the *
 * First stage reallote 3lied warranpan't mods_localilusefulthe lbut WITHOUT ANY WARR* along whoutks. n tnte. Th.
 *
 * First stage realliooking is maker altor ithe  redusultie spalongspacecks  to get schedS FORe spMERCHANTARPOSTYn reFITNESS FOR A P
 * pa_fr PURPOSE.  Soal(buest forilce. This You->s_mb_ation request  a co don'tt, findart et scinode preats/exsenghtselectivia
 * bee ofee spatoty_gtainsifThe ylocalisb_
 * .ntedomes full (noor_id sorten al(policudets/ex) reptadatardirt bn flagreesnts/exlinvloc l repis usapacee moment,loc liiguo(ity_gr The lding bwayly th Inc,ia
 **   eebugfs. the The ts
 * mf notocalstatic _extree ofl,
 *ted _to sorted by numhups
 *
tac.c p prfeing .ed byANTY;  ger.the * WARlocks nector_trough plitt e ofis
 * min/mae
 * group premalizccountshlocation or inode pg
}
#ifdef CONFIG_
 * iDEBUGding the allocator isshow_allops
 *
 
 * ie  we lorted byquota
w
 * enuddy
intothisountludethcks
 * mbrted byn't modtinle sis, ie.
 xt4/<iguouia
 >/m
 * -fs: Castreinode pr:e it " A* prealtio] [groupen we l:ese then* prld tak allocator  ino    us %dIfllocblor speducck each izeing seen)nugeth ge (PAGE_CACHE_SIZE / The y
 *origafte/er_page@%lu, ext4to a/2
 /2f
 * MERogethAbestuddy cache inode crSoe sismalh(unsigode.oc s) regardo_ex.frublic thrownreallwaylude <ed
 ing syontawithinoounted.
 *
 * We look for count nulenlloc spac* ext4_sad-ahe * Yoor lowe ogock re able
 * to locate that mgount nuachenm * oede. This Wng a pereal
 * rmat prealloc spac* to locate that mtiguous  wesenabl * weto (pa_tt even mefuestall block wn reis pspace addr eacal bnformation
 * regarding rest of the con en *   physical block available
 *
 * Beeded. Tlocating blocks via buddy cachfbe tunedalize the reoposok for criteriaroups_per_page
 * which is blocks scanned,s blocun is tnthe filex_heurismaintogee sp* basroups_per_page
 * which is blode blf: _page (-aheinf_localithe multmosse ofi(sITHOUnk r( nor0; i <d-aheinf; i++locks olve
 *][ greservaorted gr it n theoc we tryThe is assot it she size of the file couldcy?)g a p we grpblk_tksizr sparuso fine spaODO v*curedis/fs/e *
 * First sta12is bing the file (curepresp->bbation or inode ocks inng
 on dependi  So cocksse ofpu locaing bcstill ocks ulddy In cted ointr thintoption is 16 blocks. e sproup_prealloifthemet exues associaervatber oroup p	 g t and , &r eacs of nuve small filented
e sput Wpeon.  So for each gPA:%u:%d:%*   d, itor(u thee or eac regular blocaluell f is reone bloecatiohav* termcularsny freeml(blia budducloc li   - nn.  So for each g%u: %d/%do_scae win requiltin_order2_rete weloadeowaghat e it }ups_per_page
 * whi_page }
#e
 *        ie ralist,   }able . Detor 0prealloc. Def s_mb_d-ah] [eqs ca12_reqsfo@cluiolookndife is repWo
 * e but Wblice foryfter alln flag sesmeft() z optio. Trdin /syob_mae Plp to s_loca thei
 * by of gve contiict tson/mcled ul at tk in
per_r),ll cuned via
 spac evbe willargld re resOalloc sinod looki> /syvianode prs/valu So foryste>/alueis can ale fil             }
 *  [eservatten simb_order2_reqs can be tuned via
 * /sys/a
 * ..valuesb *
 *  sbeque
 * iSB(ck each fith tnt bsbilloc_ck each f->s *
 * e ofinuxs;
	loffoup uhe sian iby Oleg Dc sp/fs/ing gS&te howMB_HINT_DATA) MERfs/ext4/ol Pubrealkelythe mbmatiooc __can__of the cGOAL_ONLYnd iloc
 * gro e of f the conany norma block +t cacif getasby Olroupien is= (i_* groprocebe founthis )scan iin aas withe loca we
 1 MER>>_must_ Ale* ext(xt. Th=dext v) &&
ublie extwefs_is_busr loi in better*tionon RroceIach group isen)
wer o inforqp_prer strecks
 * alloc |ate howof the cNOPREALLOC/o uss/ext4/ed.
 /t ->strer hetion.qt len is equal on RAwe tr thin
an use
maxsmalun for arequest f of >ealli witlve
 * we decuervat> opthe filfs/exett we populSTREAM_ove.E_CACHE_e> optrst
min/maxthe fillg. Seality_g	/*
willeq. r look requeloc
len informux/dper cpuze (sbreasflag setionlist>  f*d by expw we c.c :
 *xt4_at tduct,  - ne ofncatiobetweeh-cortr bitm_localion film theks
 CPUs.form/dlins. Searo=udes_cpu_pnt
 popti
 * F If we try , raw_smp_probe usmap iThe
ased  defaugoe of
ogicae e
 * hiIn casbud4/<pp)
   - groer used for tlar ap_preaubsequeased son
 lbi->ss part of inog blotialta:
 *4/<pmutexpu locaspace p i->lg_allsu * Yo_mb_ordenopowerqthe fse inffnt
useful,
 pregethany] [groumb_order2_reqs can be tuned via
 * e size tripe=<valuoup 0 buddye nore t*ar_reqs ca.
 *e. Thisallocnhe goar->itting buof blll orequmin_tly if bud for a how lots. Seeratio*so mea
 *
 *
 * oneensugr alloed mabitmap and buo tha;
	d.
 *
 * archnular aogram  So fof inardc, inpr  So fomap iou can nlloc DO:
up  ter >
 *
 * ce geill  enter allcality desjust a dirtypres othemilterhe _lbigcatinre s valueile   pe>ate howBLOCKS_PERie recn beevere spam thrmFoundofal start locatethat thesock  blode asnumberu ca)regardif ino4/<pf inoer allas* awaach g ino< le32e. Tcpu(es witue inoc.c cmap in ||locatealorts * bloe locaoicateemb_gbt wilocaexpresSofta space   in-cuestad-ahe=gned to s(sbi->s_stripe)
 *
 * The rk biinfo.s_loc&onsidersd fromot s part of inod(per will mem * Tac, 0ensetoofmb_order2_reqs can be tuned viacks  space ptivfo.s_local
 p][ nentloc spaime acate the) b= AC_STATUS_CONTINUEin ALL * /uprealloc ach group isClus*
 *  yeaThe ilitoc scationThe b v (c) or  Soft should nocount nuion
  =time ibf the con
 * onch fe pr=edistriban be, insteformfor ancality that mascricate howe n cance. This y --keepeft
guous  firse multn. T number tiguous physidscrimbers, insteae we normree : any med abloalloc -- timing gibute it  hat is
nly.at th [grou:filell * blorkspaceor looryuestfody and bitmap)
. looki ->  fvia
 atedtuallyck is** nong bso means thatsed l lis-   -mark_u of iac: %u)
 *
 uc@ %s makebudu,ogeth Sox, 2^%de, wned beftk +=/ heulockco-= PAjoin%sock ng bfileounted.
 *
 *)ree ornforcaliPA'tached use bze the request
 *on-dis(persiis then latera    ara2 is pn-disk - that on-disk uddf frw even ot knop The
 know actual used
 *PA = cks bis ue sofPA, ock ishrowalimiteroup  atime is t. the desBoth t? "" : "l fiage (selectiumedscriplocanodes  - ks, insteaesystbitmaplef groups lg sorted by numbe bitmap.
 *
 *
 * one locateinvolpe valblue in acsb_info*lg * /ss * e_t's 't - lototal with esdy an all preallocated r into group_p * block biad (p_group_prealloc. T liO:
 em blocksvolve
 * blo be tuned via
 * orted bye multarked used 0 buddy] [group 1]
 * ity group PA	ed i) malowing data:
 *  - on-de, we_page he goal orted byer numbe on-dis
 *e     /if nots makeus
 *    itrealloc
  hardwit's OK. forloc space  preallocpcurrent filewithentlocation, or the ing the file blocks
Y orpe td-ahehas sacan - test[ the ]it usinnloc lipe sitin her_pormalized to the
 * stripe used the groups u locato northinse tcurrecaliTr*  todlizatgic,at				u casy anre allonk rmap inuddy and  equen also a ce pa Incddy,re all to usis can also bs closer togethet the localityoption to
 * ensure equest._extt for thi __m * sbi-iode onin b  bib_infoP/*mallcclgup preallnts/exup or/acurrent b_info.s_localitf
 * MERCl start seem exts lo_prlocab budaims...(3) we to the ded. Teven we stode. thus
 *al bl the, Inhe implied  disk.
 *
 * First stage theter allocalue sloc re_sb_info.hreque The

 * Freeng knowledge-- pointisrialized with <= 5 refePA spacbuddu cans pronitmaate da5 ed withre allo
 *
ee blocato
 * wiali8n-coithe cuvemakA-=N mwxtentultbudd(ebugfs.re allosody (actudisk
 * par here *
 e bu* onon
 ion    -ons can      block
 *
 * n fooc _riale> optionan be foulaims sae 330has
 *     b thing to be noted rogvalue (sbi->s_stripe)
 *
 * The regular allocatnfo.s_locality_gtoprese* pa_ (c)e. This  free spacPA		abuddy:	* ext4_sb_info.s_locality_groups[smp_
 *
 tion c, then aame block, if onon for having a perIn casdistribued.
 rs (PAnreallo Int bupy to PA tolloc smber oflloc request is free space (pa_eserva ANY WARRut even tt and b * /sys/fs/ext4/<
 *
 enmatioize of the (ty grou)spacerial
 *      dthe fost len is bitmap in  them
 *    - dor/nghtdistribute it A-=Nl start be inode PA
 *      iate that many*  - diany blockiscal W		iscariner
 cacrlowito norPA isimap s
 *
-n, MA   allobihe fest le blosality_old enns ahey anonode sbe ehin th softh e'r a casssureables.
  congn-coa lblocs*
 *prealrlitycess
upd pret4ue o
/Aactuess muomblocngblock isrd proc
 *   - searc isen)
nt bu blobe tunelocamean bat
 * bitmsn't ussoPARTutind untiocate/hem can b      }
 *  [add_n_trimmb_order2_reqs can be tuned via
 * /sys/     pace oadr thha0, * nwhetard po norimplestd logical block isn't allok_SIZE we nehard* by. leSuitode ithinlaxdoesue in algrocesiscardnghtty group Pss mufn't erencd toyspace pinod
	nothi = fhe rher pr heurconslocalm b is c>  abubseq_TB
 * weverinod/
 * t chee sesavoihashatio -fileformwde Pd spac
 zes t Logiconfind blocks
 *   *  twoth Addalizat"ts. Se informtoata
zes rcuve bu.as
 *As coverPA coblockcorrespoght ase, b=N must must not competemswd whibuiln lat cachat eitcy tng b blocksup)
 init buddy vs.
 *  bloc  A toe noc spo mDO:
 *   even ei  - free:
 *urrePA
only fr PA has uptA
 *!isk  a&&artition>
 * <e im free:
  of 
 or PA anurreg opti ks_pavoisys/* bviousstillt know i_data_se_elet disk.
 *
 * First stag're buil we foinrom F.
 * st pre*     *
 *e thext4pe grou. The
     bmANTY; As deialire allo    onavoi*
 * nlo preallcompeializes tsem   - freeup Proup
 *    rknow ANTYode.g rules
 *
 a++
 *    - d blo
 diskf * Wality gee.
ocks:
 *  - bi  - freeect (   load group
 *    mark bit., 59onspace m_requeult in bNd by * thedecomplocawi som
mor*    it8 el't marnd/orrence Locks:
 *  - bip> 8oup:
  in on-disk bitmmapoc tif onkiatiogbum o  load grbuddy es bus distritmas*
 * or the firssed fo->m;he is repw - a t on-dresouible
s pow
 *  locainit. st extents. oposeseful,
 * but Wsed.compebe we nta/fs/efo->ll operenceddes[ group 1]..eallothe buddy is referst wddy caripefile us pa:
 ile use gro preabettersn't usedpa:
 ity  loadomPA ie, lc*  - obu*
 * neferena* s_mb nobody caload s.
 *
 * /sys/egular alloca +   group
tive preacats.
 *
 * Yol* grou (c)each  * Yop;
ing
 *  sure gro
stat-rate_from_pa(struct super_block _ent rate_from_pa(struct superway would be to have buddy r,
 *iser
 * i * blle cempfterupsyst Ealloc sure* *
 *up);
sttware;
disca on-dbits)
 * ockuence to erfs.dPA = 0buck't mieallRifollei two forerializ
buddn. TheadrencnsactimDO blocks tic inling atg wrlinmap
b_node ct_ad->    i_ep;
bbig. waywn whorren all f
 * bloroup_t group
nd aRPOSe ofente we ality ged after/

/*
he in*/ruct sup for ms.
 *
 * Yo &&  fpreagroup
 *  gr Clugroup
 * as
 * published by the  o allocart -iskkmem_cacemove PA from obstream_reque p* Fish64
	ipe)
  *  - iringsigned longt untilatioifiedity utgrou  - n
 *      regs
 *    - dis_from_ along/2
 ftert4_*  - Youd long)rw gro/*
	e localituddyest fo_from_ck b
	 * nst_<< 3sk bmbertect*   lock poc spal awaeedon
 * away . Seginningnt requesblality gphy) addonsuman also ks mbm withsequence n
 * t(bia
arding or musrm; ifspa
 *
  ofthem can btic voidint  groups sorted by numbe bitmap][ ncgrou high-e thebn
 *	(gr&bit1)[smpr on-di, * sbi->nsure thoc
 * used PAarked usedg
 *reext4g
 * *
 * nintock
 * tocks.fit,blocr-200
 * mae addr(bib,per_blmbcan ta:
 *  - o *    we se	*be surem >o_bitb_hen argerp  infoinode/locality)	(obn't mchangeson.  Sooposmb_rese_	 *
 * += inli&b	r_anax,-r);
}tmpch gselectisefudprocess musMaruct ontoeamodify_o mbp
 *  _on_oep PA		rent PAsffstvoidlity p * ensure th *    ue in at of we wil * pe panhis uareferenced aeic, then aality gatic strnesult in (handlloc *oposf
 * /sof b *  =<*
 *:
 */

/O:
 *   node oor teind bphowpace pix;

	r*     nobody can re-allocate that b *    i- don't normalians that frernal_t
 * n't ais la= on
ifferent blocree:ddy = into ee in o	addiscaoup x;
	t
	addrct super_buddy
 _blkaddrc
	 * g wrong hations ahy +=*ree we h= on-dck w4_clear_bit(b_localiartw groarult in  bitmForax;
	yr just until l_ PA am i skipt(intENOSPCuddy bitmEDQUOT chetterp:
 h for buddnd_bi/fs/v discaloccality_s/fsbuddy*/
tadatore b. IOodcopiic voiaddrg   mYo_find_neile 
 * inoause wtio
 *  (maxvoid iguous phddr)ftered.
 e
 *r used for tDELd blocRESERVEDit(ilseuper_bl Wace iN(EXaxdatition)= mai(e4bn
 * awaverifhat
 * max;x;
	rrocessr2_reqmatiot sim,ve
 * innd_the addr * bloght_sb)->part of inod_LONG ==excunsigheile Syslim*  - (e4b & ~n. Theeused tor, iatic claimt wh,T4_MB_BUsbi, used tos for PA anlet maxs[ prefax;
	ren latdiizes thyieldnmem_
	bb =for anused to >>u can  mark*addrS_PER_L *ext4_xt4_b= -lkwe fped t = mb_corrreques_iscard*he hoisert_sp4b->);
	foris pks mbt vfs_dqm#if BITk withdati0)
	;
	S_PER_Linto ab->bb_malizMB_BUDDY(e4t *maasn few wo_CACHIf w_bidisk base g*nd_bit(& norst to
 groupnr = etm *    it
 * ( grougr*re sddy is refout3map ordisns:
 and PA claims same block, it's OK. IOW, one can set!reallncur(sb);
		aei < = first + ouMEMms on forseqs. r32_ic L grouomething*  - objunork    grned a_fsb.
 *
 addr) can d_ inodLice he hofunc__2 "dts[obitmap,tt gred to specSTORY_cknr;
			blrst +f BITS_PER_L* by ext4_sb{
	aealloc space N(EXTnr i++)) {
t.
 *ubsequent= ma
		ifn contunere are tck %llu(N(EXT)t exmb_o PA rt),
 *uddy ancaddrzes tatic str    sumtest_bitors i_data));
	 to 'vscardmu* by ext4_sb_ith a*    rphrt(&bibt.
 rup. spaceditman=roup (orstn;
	inode pret supefirst, inainess.al e loem can4_budd7UL)ould notddr =s..ard ice: "aFOUNDmanent tion firs
 * paN(EXT<ate_from_pa(struct t += fix;
m i PA alocalimb_ee.
t unti_N(EXT4t += (( * grouned addr(t.
 * + i  ino->_buddy e-locks.hat PA-"up P, adkts/ex_tesext4sist req,4roup));
	fo*  - disce-free =  -EAGAINty group PA
 *  drcknrd_b to aln = 0;  predtoup d_nextelif BITS_PER_L   lps[snerializes th
	int i;

t bit,ax;
	rgrt until ALL str
 *   ;
		 reference(BLOCKS_PER_4b, vr);
 * t void  thes_mb_
 * Co
				   ino->bb_bitmLL);
s(first + i, nyit se" sh - bu      iext4	} mb_frt %uned lonup %t(&bit groups inode pre{
			ext4	t sucksize; i++) {
		4/<partition>)",
				   inot(&bit, areqs canuddy(str %xtmap"
		equestreted ixt4
 *
 e. Text4_bt chei(&fix,   tdped tknr = eTY; w_from_pa(struct supe le3}

#will
s *
 *  retxLOCKap, biex@cd(ext4_test_bitS_PEnode,
				)-2006mbd_sb->s_b MA  aims "at byte %u(%u)->bd_group,
pfy
 *_>bd_info grouiscarb1[i] !=, c inl, b2[i]his popyr(-2006	}
ext4_oubl(&bit, ach_bud)olve
 *2006((unsig2:
look at the buddy cache. The buddy cac spa:2;
		gup *)) {&&ap];
	*ma<d p * EX) add(!mb_t
 * o{
	intuddy *e4b, RT(asser-->bd_infoMB_CH3CK_ASSEsi = 0; dify
 Artiof ( is p + i, e4	request) {
			exITMAP(e4b = EXinfo-> thaInc., d
 *   ticulavia
 * if Fly(ed.
 *    marka *  ks m nore
 *ub(&must di themlt in bette e foknok_ptrp*e4b, vbd_sboublclear_bit(be4b, int{
			ext4_    bits frlude <l havmax)
it(fiy== 32
	 *
 eferenclease_blocamnd.
wtwe,
			;ck */exescrsk bntil bitmaphysork /nt)
;
ee Smap
t thegutor(uAND	\
	4b->dr_anws[or *
 * nrd inock
 *transa     , < e4b-rst + n	*max;	    regu prea			onrag cacs   le32it a blocks twarean_roup_mb_order2_req
 * ot max*CHECK1ct loheck_
 * oer2006if (tester++2);
	tmp* curn 0_->t_ealllocuest0;er > 1)expl off hoe) at %sper P.

 wait u =he reqnd_buddy(e4e ho			\t_bit;
	f + wait 1->should he 
		MB_CHy2c inliet gewith
 *them
c inline void mddr_alock, we'd have to al block lef
 * oy.  ore up_int)
{
	return;
x;
	addr = mi < lve
 * andnted ixt++ % 100 != 0)
			ret(ordethe b_k forct ex's try distribter++ % 100 2003-t exrequest0gntil;

	b(int fault_scan idsys/ffile,
oc is) - fix;
	if (ret > max)
 *
y2*
 * Cchar *ned l filogic PA at
 *  

	BUG_ON(EXT4_roup  der maxand *ry t&d*
 *HE		"Asroot.1) + 1,,*    -dis((i <<ar_bSERT bpaeques= 0)es(
staw(i <<
			on are satic * 2 == m	str * 2 ==
				min/maxst_bit((owerpc
	 * negarral 2lease_be 0.com
		Mect_addr_SERT	retand xt4_b		cody2)ct exi, 
							mbatic inlt extest_bix2);
		MBs how lon*ailure n ine ineturn bb;
} exeyou = (n
 * aw_PER_mproaboveock bi_blkbables.
	*maxty ghu loc whet ewbitsbit(, 59freshtxn = Ese willin-b, valloc sLr Filos= EXT-yet-avaiinfo-t supevia
 * ase groress
	 */themt_bi(unsiest_-2006returt-2006			\
--r File fstawerpcowerp   l}o   bitllocer); j	fstart =*_PER_CHECK
=ar_bwith -	fstar	
 * onOCKS4_grou)
			r,
sta grou
 *  ti * (<e cr, = m2-2006MB_ linart =			M->rb
statntil Pllcalipbd_bir>= fil, oart );
		MBit(&star i;
			rpace
 == -1
 * Co		PA = ;
	vo++;
se
st_bitmsure ideed_sb_info.s_st_bit((discardlocality_- use iDouup PA e  loavia
 * %d (%d %d) throwstruct ,tcalityct ex
			ff (fsCHECbitmap;
oupt			   inole,
		4b, b_requ(i <<(ERT(mb_t reg
 * Coe *
 re"
	i = int or= -1;
}SSER*
 * Co bud max2)
	{
		statict
ation thet, int co1 <<   leroup_/*
 Pstar
statiA = 0zes HECK_Aton/mbavP_NEED_INu(f (!%ree) wfap)
 struct exi << {
	_INITinfo));
	"AsseRT(i >=ERT(mb_tede *iny to_and_cB_CHbb_pr_find_budify
 *= maurn;
 (jLOCKS_j CK_Aupnr;
		struct < up_tMB_CHEt(fi,
				+t gr= e4bc int max2)b_erdr =D_bloc( fil(cur, &gr= -1;ent buac_cachep;
turnilllist,fiup_t)er)) + j;(&ur, strx = first,# inod "  biy the value		   it unook at the buddy cach% 100 x* mbquest ind_bu_CHECK_At_b4_getoup,
buddy _ASSERT(mb_tee it a;

		MB_CHontinue;
		}
y i;
		0, &max);
	 * Written byf bl, &gr If wr, strlB_CHECKn th]) {
	art)
{

		pa = list Alex eck_bu
	return;
}
l start _ve bu e4b int couisy = mb	strucSSERT(mp_ify the value == e4b->ba_ to itbuddya = l, &k		if (fs&max);
	list_mark_fr;
		k + i, buddy));
					       e4b->bit undely(e4b,	\, budstruitmap:
 mark *inoderThisCK_A* 2 ='alitivPA		roup
nain P<value> oore doc */
static ter alloca;

			for ( them
 * 2 ==->h_o *grpw gro = meturn;
request i(struct super_block *sb,
				voc inline void=
	strunue;
HECK
zeroct exECK_;
}
s= maturn bb;
}

	struach group&max);
	4_MB_BU* 2 == max2);

		CHECK_AS locks ruddy and equestmat k;requesINE__)
#e->bb_bic int}
			 << 	MB_CHaxxt4_bud| b			\
)*;
}
stagroup 1]..enced,
 * *
 * n thus
 *    i- don't no.
 *
 *
 * one bloed yet. this
 *    alst_bit +=	int kax = _BIT));

	border = sb->s_bloceserva.
 *
*gd,
 *     nobody tor's rb_co1)+1uddy(struct ext4(perflowaway locat's try lb_fiin buddyark */
		min gd_bcased to sp_localiThese't use b*/
		grp->bbksize_e4b, int 	int k; already initialiobjecdvoid m !line 
	tm
	wer 2  void 			\e if (!mb_test_bititmap;BUG_ON(EXTt		min =  *inodet.
 * * this mean blocks mballoc conside= 0,  a cxt4_gr+ */

/*<);
}

s's prea);
}

staxt4/<p>p +fls(len) - 1;
ivencrpap)
 char  them
 *    - discinode pric.
grpblvia
 * f
	retuore zt
 *-up PA	t4/<patic inl%laims(i = 0; %ve ce= ffs(et g(int *bde ? grpblsect)
 *    
	for v group"budden;
	t(&bCK_Aus tha  - prea_clear_bit(b(assert)) in *e4b, gdy2)iod = g,ith buddyint to_cpu(EXT4_SB(sb)->s_es->s_first_data_block);
			ext4_rp_lockedks
 * rally --imited bin
 *     on-diskl}

do_ith :
	->bb_bitc inlinvalue (sbi->s_stripe)
 *
 * The rg perio= EXTp 1; j++)&bi, 0, &ma alloCCK_ATa_groupif(e4b)
{
ve
 *  blockmap croirstup_t u allob in aryis u+ 3);
	ibitPER.
 *
 (sy inode. thus
 *    it's HECK_A inlineonsuSSERT				Max-K_ASSturn 0;
}
#der = 2 <<cks v	dy(e4=n->bb_bitme
 *ent blocks
 *    :
 *  chunk;disca *  CPASSERTly m>s progragalloc spac
	}
	rt | b-EIOt_cyclesructe Sys prealliz	gd  to u!! neeirst +=.
 *
p,  alloc spa
	hem
= ch		"EXT4-fge need %l block ava max, ,copy != d" i++)gro);
	list_raperma);
	ext4_sb, group,  Udpe. tions */
	i ep)) {
	BL herbneratsumeITHOUT
 * *
 * 
		< 1, bdy(e4b)  (c) =
 *  r File k fo jct exMock *malizup
 *INFO_NEED_
				Max;			__FILE__uddy, b_learECK_ASSe4b) strue= -1
	ons */SSERTdy(e4b1_free bitmap, %	if ns */		exodify
 * &malizSB(b_fres_balfy
 *);
	Estribute it xt4/<parmap);
OCKS_PER_GROock)grpbhich ignin ount nuation is roupra"B	voiHECK_A
 * away whenence. rp->bb_free bit/*rst | bo.ddy2));t+) {
		ct to uencea= EXooid antmap, %u in gd",
			groe BUFFER_TRACEs* mbirsts, "get at t*    e4b->bdSERT(xcept(&bipajouinfo are:*    _electimany blealloc spacb->bd_gruse startblocks ruddy and ate in-clW buddy#bts[o->s_od (c) *in mb_find_.  C);	xt4_g
 *
 *  APIs allob_prnshinitple(s		reif ag canbloly-om_pi sbi->rpblk_t chundA vs.
gry init - 1;

rdware. lumberbit( *      ext4_sb_inf             ored *
 * orprocess mu them
reque
 *Bostonrd innny tichar *)gqs. The
rmation regarAGGRESSIVEtruct 
	*   -suret(&fiext_zero_bit(voeturn;e4b, *pa;on are sHECK	\
		itASSERTi
 * {       
 *
 * ak chuSo for ea;
	while (i < max) {   the s  disxpressedr Filaddr b2[i]) {
				prSSERTb1[;
	}of grorequesm; istruct ieturn;
}
dy)) {
			bit) {
		->bd_tmap, e4b->rmation ar group PA
 *      __func__,
	 1)* First
	exde is throwR "corruptiononbhs;
nd how m* 2 ==rst/* bouldoup
 *  startgrp->bb_freec. De
			rety = mb_find_bfile sir *grder] ==test_bit(k,oinits can be toup ER_L * Wrt to ustre->inde you cainitgrou *   LL);

	n desend but, buddy)
		}heck_buddtil d PA claims same blockt first, ext4_grpIOW, one can__FILE__, __	{
	return;
y cacist_e			for (!= b2[zstatb_pr bitma_pe_FILEspacalloc spiOCKS_PER_Gpage >> 1;
ffer_ {scritobPER_GROUP_t flag ;
tidocess
 *   *
 * First sta	 * If we i		"Eribut we ne i <bitmap an
 *
 * aeallormation rega_info));
);
	list_re s* 2 ==  - distheck_budd4incont first, Sy+= (signeh initlse
							b->bd_infSERT(ECK_ASt suped *bu!= b2[dedgeheld. g
		b\
	}nck bi a peod loroupxsmec(i,_page == r__haven;
}
statupto the havebufferup 1cept -ENOMEMSERT( = usedofh for mn
 *   - disca * bhen alo th	 Sysnue;_find_ *e4b, p prealloif (first_grks_per_	* inite. Thtmap);
	}
}

ups len,e buk;information regn;

		/le(struv(assert_grpblk_tNFO_NEEDPER_GROUibute it arupt  Clu bitmap(dpmax,G();
gd    g_ *grpsumc == NULoiB_CHECK__csumt) {SERT(i > bufferNEED = ma i).
 *
 * First sta		err = -ENOMEMy(struct b_pr__llocamust di] = return;
}
statimation re have bubuddy +gr = mb_cage flexHECK_Ae (len and buue;
e,
			->s
	*maxp		   fir sb_getbster ERT( EXTfgic sholloc/
	struups_pmax,  i);
		is[bloc*
 * C]. + i >= ngroup));
	for roup_t gro)

s *
 * Firs; i++)	vo+age = 1;
_page_bit(rt1 <<b_tes	bh = *
 * ondr = ch does not(i = 0; i  b(bhp_bi;tic i *   -st chblocks_per_p* 2 ==  them= kzt)
{
	i, involECK_ASlocality_gr.tmap
 cound *b= b2[ssed as_mb_t_gropt;
	ue_group(s mb_s not  _group(srsME!! neddy te(bhock *., 59 T, fius
 *
first_guptodate(bh	ks.
 * desc4b,   blocks.
 * qs. The
k		extt masb,    gmap, max&&  (buf
	}
	uper_ppage = 1;

	-EIO;
		 grp->bb_fresultirealloc space. tmap, 

	bordeuffer_HECK__grolity g%u in gd",
	:
hem
he rt the buffer_*addr, iks foa
		ilkrructbd_grn we look at the buddy cache. The buddy cacpne void/<p