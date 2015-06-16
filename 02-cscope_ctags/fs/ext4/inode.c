/*
 *  linux/fs/ext4/inode.c
 *
 * Copyright (C) 1992, 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 *
 *  from
 *
 *  linux/fs/minix/inode.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  Goal-directed block allocation by Stephen Tweedie
 *	(sct@redhat.com), 1993, 1998
 *  Big-endian to little-endian byte-swapping/bitmaps by
 *        David S. Miller (davem@caip.rutgers.edu), 1995
 *  64-bit file support on 64-bit platforms by Jakub Jelinek
 *	(jj@sunsite.ms.mff.cuni.cz)
 *
 *  Assorted race fixes, rewrite of ext4_get_block() by Al Viro, 2000
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/time.h>
#include <linux/jbd2.h>
#include <linux/highuid.h>
#include <linux/pagemap.h>
#include <linux/quotaops.h>
#include <linux/string.h>
#include <linux/buffer_head.h>
#include <linux/writeback.h>
#include <linux/pagevec.h>
#include <linux/mpage.h>
#include <linux/namei.h>
#include <linux/uio.h>
#include <linux/bio.h>
#include <linux/workqueue.h>

#include "ext4_jbd2.h"
#include "xattr.h"
#include "acl.h"
#include "ext4_extents.h"

#include <trace/events/ext4.h>

#define MPAGE_DA_EXTENT_TAIL 0x01

static inline int ext4_begin_ordered_truncate(struct inode *inode,
					      loff_t new_size)
{
	return jbd2_journal_begin_ordered_truncate(
					EXT4_SB(inode->i_sb)->s_journal,
					&EXT4_I(inode)->jinode,
					new_size);
}

static void ext4_invalidatepage(struct page *page, unsigned long offset);

/*
 * Test whether an inode is a fast symlink.
 */
static int ext4_inode_is_fast_symlink(struct inode *inode)
{
	int ea_blocks = EXT4_I(inode)->i_file_acl ?
		(inode->i_sb->s_blocksize >> 9) : 0;

	return (S_ISLNK(inode->i_mode) && inode->i_blocks - ea_blocks == 0);
}

/*
 * The ext4 forget function must perform a revoke if we are freeing data
 * which has been journaled.  Metadata (eg. indirect blocks) must be
 * revoked in all cases.
 *
 * "bh" may be NULL: a metadata block may have been freed from memory
 * but there may still be a record of it in the journal, and that record
 * still needs to be revoked.
 *
 * If the handle isn't valid we're not journaling, but we still need to
 * call into ext4_journal_revoke() to put the buffer head.
 */
int ext4_forget(handle_t *handle, int is_metadata, struct inode *inode,
		struct buffer_head *bh, ext4_fsblk_t blocknr)
{
	int err;

	might_sleep();

	BUFFER_TRACE(bh, "enter");

	jbd_debug(4, "forgetting bh %p: is_metadata = %d, mode %o, "
		  "data mode %x\n",
		  bh, is_metadata, inode->i_mode,
		  test_opt(inode->i_sb, DATA_FLAGS));

	/* Never use the revoke function if we are doing full data
	 * journaling: there is no need to, and a V1 superblock won't
	 * support it.  Otherwise, only skip the revoke on un-journaled
	 * data blocks. */

	if (test_opt(inode->i_sb, DATA_FLAGS) == EXT4_MOUNT_JOURNAL_DATA ||
	    (!is_metadata && !ext4_should_journal_data(inode))) {
		if (bh) {
			BUFFER_TRACE(bh, "call jbd2_journal_forget");
			return ext4_journal_forget(handle, bh);
		}
		return 0;
	}

	/*
	 * data!=journal && (is_metadata || should_journal_data(inode))
	 */
	BUFFER_TRACE(bh, "call ext4_journal_revoke");
	err = ext4_journal_revoke(handle, blocknr, bh);
	if (err)
		ext4_abort(inode->i_sb, __func__,
			   "error %d when attempting revoke", err);
	BUFFER_TRACE(bh, "exit");
	return err;
}

/*
 * Work out how many blocks we need to proceed with the next chunk of a
 * truncate transaction.
 */
static unsigned long blocks_for_truncate(struct inode *inode)
{
	ext4_lblk_t needed;

	needed = inode->i_blocks >> (inode->i_sb->s_blocksize_bits - 9);

	/* Give ourselves just enough room to cope with inodes in which
	 * i_blocks is corrupt: we've seen disk corruptions in the past
	 * which resulted in random data in an inode which looked enough
	 * like a regular file for ext4 to try to delete it.  Things
	 * will go a bit crazy if that happens, but at least we should
	 * try not to panic the whole kernel. */
	if (needed < 2)
		needed = 2;

	/* But we need to bound the transaction so we don't overflow the
	 * journal. */
	if (needed > EXT4_MAX_TRANS_DATA)
		needed = EXT4_MAX_TRANS_DATA;

	return EXT4_DATA_TRANS_BLOCKS(inode->i_sb) + needed;
}

/*
 * Truncate transactions can be complex and absolutely huge.  So we need to
 * be able to restart the transaction at a conventient checkpoint to make
 * sure we don't overflow the journal.
 *
 * start_transaction gets us a new handle for a truncate transaction,
 * and extend_transaction tries to extend the existing one a bit.  If
 * extend fails, we need to propagate the failure up and restart the
 * transaction in the top-level truncate loop. --sct
 */
static handle_t *start_transaction(struct inode *inode)
{
	handle_t *result;

	result = ext4_journal_start(inode, blocks_for_truncate(inode));
	if (!IS_ERR(result))
		return result;

	ext4_std_error(inode->i_sb, PTR_ERR(result));
	return result;
}

/*
 * Try to extend this transaction for the purposes of truncation.
 *
 * Returns 0 if we managed to create more room.  If we can't create more
 * room, and the transaction must be restarted we return 1.
 */
static int try_to_extend_transaction(handle_t *handle, struct inode *inode)
{
	if (!ext4_handle_valid(handle))
		return 0;
	if (ext4_handle_has_enough_credits(handle, EXT4_RESERVE_TRANS_BLOCKS+1))
		return 0;
	if (!ext4_journal_extend(handle, blocks_for_truncate(inode)))
		return 0;
	return 1;
}

/*
 * Restart the transaction associated with *handle.  This does a commit,
 * so before we call here everything must be consistently dirtied against
 * this transaction.
 */
int ext4_truncate_restart_trans(handle_t *handle, struct inode *inode,
				 int nblocks)
{
	int ret;

	/*
	 * Drop i_data_sem to avoid deadlock with ext4_get_blocks At this
	 * moment, get_block can be called only for blocks inside i_size since
	 * page cache has been already dropped and writes are blocked by
	 * i_mutex. So we can safely drop the i_data_sem here.
	 */
	BUG_ON(EXT4_JOURNAL(inode) == NULL);
	jbd_debug(2, "restarting handle %p\n", handle);
	up_write(&EXT4_I(inode)->i_data_sem);
	ret = ext4_journal_restart(handle, blocks_for_truncate(inode));
	down_write(&EXT4_I(inode)->i_data_sem);
	ext4_discard_preallocations(inode);

	return ret;
}

/*
 * Called at the last iput() if i_nlink is zero.
 */
void ext4_delete_inode(struct inode *inode)
{
	handle_t *handle;
	int err;

	if (ext4_should_order_data(inode))
		ext4_begin_ordered_truncate(inode, 0);
	truncate_inode_pages(&inode->i_data, 0);

	if (is_bad_inode(inode))
		goto no_delete;

	handle = ext4_journal_start(inode, blocks_for_truncate(inode)+3);
	if (IS_ERR(handle)) {
		ext4_std_error(inode->i_sb, PTR_ERR(handle));
		/*
		 * If we're going to skip the normal cleanup, we still need to
		 * make sure that the in-core orphan linked list is properly
		 * cleaned up.
		 */
		ext4_orphan_del(NULL, inode);
		goto no_delete;
	}

	if (IS_SYNC(inode))
		ext4_handle_sync(handle);
	inode->i_size = 0;
	err = ext4_mark_inode_dirty(handle, inode);
	if (err) {
		ext4_warning(inode->i_sb, __func__,
			     "couldn't mark inode dirty (err %d)", err);
		goto stop_handle;
	}
	if (inode->i_blocks)
		ext4_truncate(inode);

	/*
	 * ext4_ext_truncate() doesn't reserve any slop when it
	 * restarts journal transactions; therefore there may not be
	 * enough credits left in the handle to remove the inode from
	 * the orphan list and set the dtime field.
	 */
	if (!ext4_handle_has_enough_credits(handle, 3)) {
		err = ext4_journal_extend(handle, 3);
		if (err > 0)
			err = ext4_journal_restart(handle, 3);
		if (err != 0) {
			ext4_warning(inode->i_sb, __func__,
				     "couldn't extend journal (err %d)", err);
		stop_handle:
			ext4_journal_stop(handle);
			goto no_delete;
		}
	}

	/*
	 * Kill off the orphan record which ext4_truncate created.
	 * AKPM: I think this can be inside the above `if'.
	 * Note that ext4_orphan_del() has to be able to cope with the
	 * deletion of a non-existent orphan - this is because we don't
	 * know if ext4_truncate() actually created an orphan record.
	 * (Well, we could do this if we need to, but heck - it works)
	 */
	ext4_orphan_del(handle, inode);
	EXT4_I(inode)->i_dtime	= get_seconds();

	/*
	 * One subtle ordering requirement: if anything has gone wrong
	 * (transaction abort, IO errors, whatever), then we can still
	 * do these next steps (the fs will already have been marked as
	 * having errors), but we can't free the inode if the mark_dirty
	 * fails.
	 */
	if (ext4_mark_inode_dirty(handle, inode))
		/* If that failed, just do the required in-core inode clear. */
		clear_inode(inode);
	else
		ext4_free_inode(handle, inode);
	ext4_journal_stop(handle);
	return;
no_delete:
	clear_inode(inode);	/* We must guarantee clearing of inode... */
}

typedef struct {
	__le32	*p;
	__le32	key;
	struct buffer_head *bh;
} Indirect;

static inline void add_chain(Indirect *p, struct buffer_head *bh, __le32 *v)
{
	p->key = *(p->p = v);
	p->bh = bh;
}

/**
 *	ext4_block_to_path - parse the block number into array of offsets
 *	@inode: inode in question (we are only interested in its superblock)
 *	@i_block: block number to be parsed
 *	@offsets: array to store the offsets in
 *	@boundary: set this non-zero if the referred-to block is likely to be
 *	       followed (on disk) by an indirect block.
 *
 *	To store the locations of file's data ext4 uses a data structure common
 *	for UNIX filesystems - tree of pointers anchored in the inode, with
 *	data blocks at leaves and indirect blocks in intermediate nodes.
 *	This function translates the block number into path in that tree -
 *	return value is the path length and @offsets[n] is the offset of
 *	pointer to (n+1)th node in the nth one. If @block is out of range
 *	(negative or too large) warning is printed and zero returned.
 *
 *	Note: function doesn't find node addresses, so no IO is needed. All
 *	we need to know is the capacity of indirect blocks (taken from the
 *	inode->i_sb).
 */

/*
 * Portability note: the last comparison (check that we fit into triple
 * indirect block) is spelled differently, because otherwise on an
 * architecture with 32-bit longs and 8Kb pages we might get into trouble
 * if our filesystem had 8Kb blocks. We might use long long, but that would
 * kill us on x86. Oh, well, at least the sign propagation does not matter -
 * i_block would have to be negative in the very beginning, so we would not
 * get there at all.
 */

static int ext4_block_to_path(struct inode *inode,
			      ext4_lblk_t i_block,
			      ext4_lblk_t offsets[4], int *boundary)
{
	int ptrs = EXT4_ADDR_PER_BLOCK(inode->i_sb);
	int ptrs_bits = EXT4_ADDR_PER_BLOCK_BITS(inode->i_sb);
	const long direct_blocks = EXT4_NDIR_BLOCKS,
		indirect_blocks = ptrs,
		double_blocks = (1 << (ptrs_bits * 2));
	int n = 0;
	int final = 0;

	if (i_block < direct_blocks) {
		offsets[n++] = i_block;
		final = direct_blocks;
	} else if ((i_block -= direct_blocks) < indirect_blocks) {
		offsets[n++] = EXT4_IND_BLOCK;
		offsets[n++] = i_block;
		final = ptrs;
	} else if ((i_block -= indirect_blocks) < double_blocks) {
		offsets[n++] = EXT4_DIND_BLOCK;
		offsets[n++] = i_block >> ptrs_bits;
		offsets[n++] = i_block & (ptrs - 1);
		final = ptrs;
	} else if (((i_block -= double_blocks) >> (ptrs_bits * 2)) < ptrs) {
		offsets[n++] = EXT4_TIND_BLOCK;
		offsets[n++] = i_block >> (ptrs_bits * 2);
		offsets[n++] = (i_block >> ptrs_bits) & (ptrs - 1);
		offsets[n++] = i_block & (ptrs - 1);
		final = ptrs;
	} else {
		ext4_warning(inode->i_sb, "ext4_block_to_path",
			     "block %lu > max in inode %lu",
			     i_block + direct_blocks +
			     indirect_blocks + double_blocks, inode->i_ino);
	}
	if (boundary)
		*boundary = final - 1 - (i_block & (ptrs - 1));
	return n;
}

static int __ext4_check_blockref(const char *function, struct inode *inode,
				 __le32 *p, unsigned int max)
{
	__le32 *bref = p;
	unsigned int blk;

	while (bref < p+max) {
		blk = le32_to_cpu(*bref++);
		if (blk &&
		    unlikely(!ext4_data_block_valid(EXT4_SB(inode->i_sb),
						    blk, 1))) {
			ext4_error(inode->i_sb, function,
				   "invalid block reference %u "
				   "in inode #%lu", blk, inode->i_ino);
			return -EIO;
		}
	}
	return 0;
}


#define ext4_check_indirect_blockref(inode, bh)                         \
	__ext4_check_blockref(__func__, inode, (__le32 *)(bh)->b_data,  \
			      EXT4_ADDR_PER_BLOCK((inode)->i_sb))

#define ext4_check_inode_blockref(inode)                                \
	__ext4_check_blockref(__func__, inode, EXT4_I(inode)->i_data,   \
			      EXT4_NDIR_BLOCKS)

/**
 *	ext4_get_branch - read the chain of indirect blocks leading to data
 *	@inode: inode in question
 *	@depth: depth of the chain (1 - direct pointer, etc.)
 *	@offsets: offsets of pointers in inode/indirect blocks
 *	@chain: place to store the result
 *	@err: here we store the error value
 *
 *	Function fills the array of triples <key, p, bh> and returns %NULL
 *	if everything went OK or the pointer to the last filled triple
 *	(incomplete one) otherwise. Upon the return chain[i].key contains
 *	the number of (i+1)-th block in the chain (as it is stored in memory,
 *	i.e. little-endian 32-bit), chain[i].p contains the address of that
 *	number (it points into struct inode for i==0 and into the bh->b_data
 *	for i>0) and chain[i].bh points to the buffer_head of i-th indirect
 *	block for i>0 and NULL for i==0. In other words, it holds the block
 *	numbers of the chain, addresses they were taken from (and where we can
 *	verify that chain did not change) and buffer_heads hosting these
 *	numbers.
 *
 *	Function stops when it stumbles upon zero pointer (absent block)
 *		(pointer to last triple returned, *@err == 0)
 *	or when it gets an IO error reading an indirect block
 *		(ditto, *@err == -EIO)
 *	or when it reads all @depth-1 indirect blocks successfully and finds
 *	the whole chain, all way to the data (returns %NULL, *err == 0).
 *
 *      Need to be called with
 *      down_read(&EXT4_I(inode)->i_data_sem)
 */
static Indirect *ext4_get_branch(struct inode *inode, int depth,
				 ext4_lblk_t  *offsets,
				 Indirect chain[4], int *err)
{
	struct super_block *sb = inode->i_sb;
	Indirect *p = chain;
	struct buffer_head *bh;

	*err = 0;
	/* i_data is not going away, no lock needed */
	add_chain(chain, NULL, EXT4_I(inode)->i_data + *offsets);
	if (!p->key)
		goto no_block;
	while (--depth) {
		bh = sb_getblk(sb, le32_to_cpu(p->key));
		if (unlikely(!bh))
			goto failure;

		if (!bh_uptodate_or_lock(bh)) {
			if (bh_submit_read(bh) < 0) {
				put_bh(bh);
				goto failure;
			}
			/* validate block references */
			if (ext4_check_indirect_blockref(inode, bh)) {
				put_bh(bh);
				goto failure;
			}
		}

		add_chain(++p, bh, (__le32 *)bh->b_data + *++offsets);
		/* Reader: end */
		if (!p->key)
			goto no_block;
	}
	return NULL;

failure:
	*err = -EIO;
no_block:
	return p;
}

/**
 *	ext4_find_near - find a place for allocation with sufficient locality
 *	@inode: owner
 *	@ind: descriptor of indirect block.
 *
 *	This function returns the preferred place for block allocation.
 *	It is used when heuristic for sequential allocation fails.
 *	Rules are:
 *	  + if there is a block to the left of our position - allocate near it.
 *	  + if pointer will live in indirect block - allocate near that block.
 *	  + if pointer will live in inode - allocate in the same
 *	    cylinder group.
 *
 * In the latter case we colour the starting block by the callers PID to
 * prevent it from clashing with concurrent allocations for a different inode
 * in the same block group.   The PID is used here so that functionally related
 * files will be close-by on-disk.
 *
 *	Caller must make sure that @ind is valid and will stay that way.
 */
static ext4_fsblk_t ext4_find_near(struct inode *inode, Indirect *ind)
{
	struct ext4_inode_info *ei = EXT4_I(inode);
	__le32 *start = ind->bh ? (__le32 *) ind->bh->b_data : ei->i_data;
	__le32 *p;
	ext4_fsblk_t bg_start;
	ext4_fsblk_t last_block;
	ext4_grpblk_t colour;
	ext4_group_t block_group;
	int flex_size = ext4_flex_bg_size(EXT4_SB(inode->i_sb));

	/* Try to find previous block */
	for (p = ind->p - 1; p >= start; p--) {
		if (*p)
			return le32_to_cpu(*p);
	}

	/* No such thing, so let's try location of indirect block */
	if (ind->bh)
		return ind->bh->b_blocknr;

	/*
	 * It is going to be referred to from the inode itself? OK, just put it
	 * into the same cylinder group then.
	 */
	block_group = ei->i_block_group;
	if (flex_size >= EXT4_FLEX_SIZE_DIR_ALLOC_SCHEME) {
		block_group &= ~(flex_size-1);
		if (S_ISREG(inode->i_mode))
			block_group++;
	}
	bg_start = ext4_group_first_block_no(inode->i_sb, block_group);
	last_block = ext4_blocks_count(EXT4_SB(inode->i_sb)->s_es) - 1;

	/*
	 * If we are doing delayed allocation, we don't need take
	 * colour into account.
	 */
	if (test_opt(inode->i_sb, DELALLOC))
		return bg_start;

	if (bg_start + EXT4_BLOCKS_PER_GROUP(inode->i_sb) <= last_block)
		colour = (current->pid % 16) *
			(EXT4_BLOCKS_PER_GROUP(inode->i_sb) / 16);
	else
		colour = (current->pid % 16) * ((last_block - bg_start) / 16);
	return bg_start + colour;
}

/**
 *	ext4_find_goal - find a preferred place for allocation.
 *	@inode: owner
 *	@block:  block we want
 *	@partial: pointer to the last triple within a chain
 *
 *	Normally this function find the preferred place for block allocation,
 *	returns it.
 *	Because this is only used for non-extent files, we limit the block nr
 *	to 32 bits.
 */
static ext4_fsblk_t ext4_find_goal(struct inode *inode, ext4_lblk_t block,
				   Indirect *partial)
{
	ext4_fsblk_t goal;

	/*
	 * XXX need to get goal block from mballoc's data structures
	 */

	goal = ext4_find_near(inode, partial);
	goal = goal & EXT4_MAX_BLOCK_FILE_PHYS;
	return goal;
}

/**
 *	ext4_blks_to_allocate: Look up the block map and count the number
 *	of direct blocks need to be allocated for the given branch.
 *
 *	@branch: chain of indirect blocks
 *	@k: number of blocks need for indirect blocks
 *	@blks: number of data blocks to be mapped.
 *	@blocks_to_boundary:  the offset in the indirect block
 *
 *	return the total number of blocks to be allocate, including the
 *	direct and indirect blocks.
 */
static int ext4_blks_to_allocate(Indirect *branch, int k, unsigned int blks,
				 int blocks_to_boundary)
{
	unsigned int count = 0;

	/*
	 * Simple case, [t,d]Indirect block(s) has not allocated yet
	 * then it's clear blocks on that path have not allocated
	 */
	if (k > 0) {
		/* right now we don't handle cross boundary allocation */
		if (blks < blocks_to_boundary + 1)
			count += blks;
		else
			count += blocks_to_boundary + 1;
		return count;
	}

	count++;
	while (count < blks && count <= blocks_to_boundary &&
		le32_to_cpu(*(branch[0].p + count)) == 0) {
		count++;
	}
	return count;
}

/**
 *	ext4_alloc_blocks: multiple allocate blocks needed for a branch
 *	@indirect_blks: the number of blocks need to allocate for indirect
 *			blocks
 *
 *	@new_blocks: on return it will store the new block numbers for
 *	the indirect blocks(if needed) and the first direct block,
 *	@blks:	on return it will store the total number of allocated
 *		direct blocks
 */
static int ext4_alloc_blocks(handle_t *handle, struct inode *inode,
			     ext4_lblk_t iblock, ext4_fsblk_t goal,
			     int indirect_blks, int blks,
			     ext4_fsblk_t new_blocks[4], int *err)
{
	struct ext4_allocation_request ar;
	int target, i;
	unsigned long count = 0, blk_allocated = 0;
	int index = 0;
	ext4_fsblk_t current_block = 0;
	int ret = 0;

	/*
	 * Here we try to allocate the requested multiple blocks at once,
	 * on a best-effort basis.
	 * To build a branch, we should allocate blocks for
	 * the indirect blocks(if not allocated yet), and at least
	 * the first direct block of this branch.  That's the
	 * minimum number of blocks need to allocate(required)
	 */
	/* first we try to allocate the indirect blocks */
	target = indirect_blks;
	while (target > 0) {
		count = target;
		/* allocating blocks for indirect blocks and direct blocks */
		current_block = ext4_new_meta_blocks(handle, inode,
							goal, &count, err);
		if (*err)
			goto failed_out;

		BUG_ON(current_block + count > EXT4_MAX_BLOCK_FILE_PHYS);

		target -= count;
		/* allocate blocks for indirect blocks */
		while (index < indirect_blks && count) {
			new_blocks[index++] = current_block++;
			count--;
		}
		if (count > 0) {
			/*
			 * save the new block number
			 * for the first direct block
			 */
			new_blocks[index] = current_block;
			printk(KERN_INFO "%s returned more blocks than "
						"requested\n", __func__);
			WARN_ON(1);
			break;
		}
	}

	target = blks - count ;
	blk_allocated = count;
	if (!target)
		goto allocated;
	/* Now allocate data blocks */
	memset(&ar, 0, sizeof(ar));
	ar.inode = inode;
	ar.goal = goal;
	ar.len = target;
	ar.logical = iblock;
	if (S_ISREG(inode->i_mode))
		/* enable in-core preallocation only for regular files */
		ar.flags = EXT4_MB_HINT_DATA;

	current_block = ext4_mb_new_blocks(handle, &ar, err);
	BUG_ON(current_block + ar.len > EXT4_MAX_BLOCK_FILE_PHYS);

	if (*err && (target == blks)) {
		/*
		 * if the allocation failed and we didn't allocate
		 * any blocks before
		 */
		goto failed_out;
	}
	if (!*err) {
		if (target == blks) {
			/*
			 * save the new block number
			 * for the first direct block
			 */
			new_blocks[index] = current_block;
		}
		blk_allocated += ar.len;
	}
allocated:
	/* total number of blocks allocated for direct blocks */
	ret = blk_allocated;
	*err = 0;
	return ret;
failed_out:
	for (i = 0; i < index; i++)
		ext4_free_blocks(handle, inode, new_blocks[i], 1, 0);
	return ret;
}

/**
 *	ext4_alloc_branch - allocate and set up a chain of blocks.
 *	@inode: owner
 *	@indirect_blks: number of allocated indirect blocks
 *	@blks: number of allocated direct blocks
 *	@offsets: offsets (in the blocks) to store the pointers to next.
 *	@branch: place to store the chain in.
 *
 *	This function allocates blocks, zeroes out all but the last one,
 *	links them into chain and (if we are synchronous) writes them to disk.
 *	In other words, it prepares a branch that can be spliced onto the
 *	inode. It stores the information about that chain in the branch[], in
 *	the same format as ext4_get_branch() would do. We are calling it after
 *	we had read the existing part of chain and partial points to the last
 *	triple of that (one with zero ->key). Upon the exit we have the same
 *	picture as after the successful ext4_get_block(), except that in one
 *	place chain is disconnected - *branch->p is still zero (we did not
 *	set the last link), but branch->key contains the number that should
 *	be placed into *branch->p to fill that gap.
 *
 *	If allocation fails we free all blocks we've allocated (and forget
 *	their buffer_heads) and return the error value the from failed
 *	ext4_alloc_block() (normally -ENOSPC). Otherwise we set the chain
 *	as described above and return 0.
 */
static int ext4_alloc_branch(handle_t *handle, struct inode *inode,
			     ext4_lblk_t iblock, int indirect_blks,
			     int *blks, ext4_fsblk_t goal,
			     ext4_lblk_t *offsets, Indirect *branch)
{
	int blocksize = inode->i_sb->s_blocksize;
	int i, n = 0;
	int err = 0;
	struct buffer_head *bh;
	int num;
	ext4_fsblk_t new_blocks[4];
	ext4_fsblk_t current_block;

	num = ext4_alloc_blocks(handle, inode, iblock, goal, indirect_blks,
				*blks, new_blocks, &err);
	if (err)
		return err;

	branch[0].key = cpu_to_le32(new_blocks[0]);
	/*
	 * metadata blocks and data blocks are allocated.
	 */
	for (n = 1; n <= indirect_blks;  n++) {
		/*
		 * Get buffer_head for parent block, zero it out
		 * and set the pointer to new one, then send
		 * parent to disk.
		 */
		bh = sb_getblk(inode->i_sb, new_blocks[n-1]);
		branch[n].bh = bh;
		lock_buffer(bh);
		BUFFER_TRACE(bh, "call get_create_access");
		err = ext4_journal_get_create_access(handle, bh);
		if (err) {
			/* Don't brelse(bh) here; it's done in
			 * ext4_journal_forget() below */
			unlock_buffer(bh);
			goto failed;
		}

		memset(bh->b_data, 0, blocksize);
		branch[n].p = (__le32 *) bh->b_data + offsets[n];
		branch[n].key = cpu_to_le32(new_blocks[n]);
		*branch[n].p = branch[n].key;
		if (n == indirect_blks) {
			current_block = new_blocks[n];
			/*
			 * End of chain, update the last new metablock of
			 * the chain to point to the new allocated
			 * data blocks numbers
			 */
			for (i = 1; i < num; i++)
				*(branch[n].p + i) = cpu_to_le32(++current_block);
		}
		BUFFER_TRACE(bh, "marking uptodate");
		set_buffer_uptodate(bh);
		unlock_buffer(bh);

		BUFFER_TRACE(bh, "call ext4_handle_dirty_metadata");
		err = ext4_handle_dirty_metadata(handle, inode, bh);
		if (err)
			goto failed;
	}
	*blks = num;
	return err;
failed:
	/* Allocation failed, free what we already allocated */
	for (i = 1; i <= n ; i++) {
		BUFFER_TRACE(branch[i].bh, "call jbd2_journal_forget");
		ext4_journal_forget(handle, branch[i].bh);
	}
	for (i = 0; i < indirect_blks; i++)
		ext4_free_blocks(handle, inode, new_blocks[i], 1, 0);

	ext4_free_blocks(handle, inode, new_blocks[i], num, 0);

	return err;
}

/**
 * ext4_splice_branch - splice the allocated branch onto inode.
 * @inode: owner
 * @block: (logical) number of block we are adding
 * @chain: chain of indirect blocks (with a missing link - see
 *	ext4_alloc_branch)
 * @where: location of missing link
 * @num:   number of indirect blocks we are adding
 * @blks:  number of direct blocks we are adding
 *
 * This function fills the missing link and does all housekeeping needed in
 * inode (->i_blocks, etc.). In case of success we end up with the full
 * chain to new block and return 0.
 */
static int ext4_splice_branch(handle_t *handle, struct inode *inode,
			      ext4_lblk_t block, Indirect *where, int num,
			      int blks)
{
	int i;
	int err = 0;
	ext4_fsblk_t current_block;

	/*
	 * If we're splicing into a [td]indirect block (as opposed to the
	 * inode) then we need to get write access to the [td]indirect block
	 * before the splice.
	 */
	if (where->bh) {
		BUFFER_TRACE(where->bh, "get_write_access");
		err = ext4_journal_get_write_access(handle, where->bh);
		if (err)
			goto err_out;
	}
	/* That's it */

	*where->p = where->key;

	/*
	 * Update the host buffer_head or inode to point to more just allocated
	 * direct blocks blocks
	 */
	if (num == 0 && blks > 1) {
		current_block = le32_to_cpu(where->key) + 1;
		for (i = 1; i < blks; i++)
			*(where->p + i) = cpu_to_le32(current_block++);
	}

	/* We are done with atomic stuff, now do the rest of housekeeping */
	/* had we spliced it onto indirect block? */
	if (where->bh) {
		/*
		 * If we spliced it onto an indirect block, we haven't
		 * altered the inode.  Note however that if it is being spliced
		 * onto an indirect block at the very end of the file (the
		 * file is growing) then we *will* alter the inode to reflect
		 * the new i_size.  But that is not done here - it is done in
		 * generic_commit_write->__mark_inode_dirty->ext4_dirty_inode.
		 */
		jbd_debug(5, "splicing indirect only\n");
		BUFFER_TRACE(where->bh, "call ext4_handle_dirty_metadata");
		err = ext4_handle_dirty_metadata(handle, inode, where->bh);
		if (err)
			goto err_out;
	} else {
		/*
		 * OK, we spliced it into the inode itself on a direct block.
		 */
		ext4_mark_inode_dirty(handle, inode);
		jbd_debug(5, "splicing direct\n");
	}
	return err;

err_out:
	for (i = 1; i <= num; i++) {
		BUFFER_TRACE(where[i].bh, "call jbd2_journal_forget");
		ext4_journal_forget(handle, where[i].bh);
		ext4_free_blocks(handle, inode,
					le32_to_cpu(where[i-1].key), 1, 0);
	}
	ext4_free_blocks(handle, inode, le32_to_cpu(where[num].key), blks, 0);

	return err;
}

/*
 * The ext4_ind_get_blocks() function handles non-extents inodes
 * (i.e., using the traditional indirect/double-indirect i_blocks
 * scheme) for ext4_get_blocks().
 *
 * Allocation strategy is simple: if we have to allocate something, we will
 * have to go the whole way to leaf. So let's do it before attaching anything
 * to tree, set linkage between the newborn blocks, write them if sync is
 * required, recheck the path, free and repeat if check fails, otherwise
 * set the last missing link (that will protect us from any truncate-generated
 * removals - all blocks on the path are immune now) and possibly force the
 * write on the parent block.
 * That has a nice additional property: no special recovery from the failed
 * allocations is needed - we simply release blocks and do not touch anything
 * reachable from inode.
 *
 * `handle' can be NULL if create == 0.
 *
 * return > 0, # of blocks mapped or allocated.
 * return = 0, if plain lookup failed.
 * return < 0, error case.
 *
 * The ext4_ind_get_blocks() function should be called with
 * down_write(&EXT4_I(inode)->i_data_sem) if allocating filesystem
 * blocks (i.e., flags has EXT4_GET_BLOCKS_CREATE set) or
 * down_read(&EXT4_I(inode)->i_data_sem) if not allocating file system
 * blocks.
 */
static int ext4_ind_get_blocks(handle_t *handle, struct inode *inode,
			       ext4_lblk_t iblock, unsigned int maxblocks,
			       struct buffer_head *bh_result,
			       int flags)
{
	int err = -EIO;
	ext4_lblk_t offsets[4];
	Indirect chain[4];
	Indirect *partial;
	ext4_fsblk_t goal;
	int indirect_blks;
	int blocks_to_boundary = 0;
	int depth;
	int count = 0;
	ext4_fsblk_t first_block = 0;

	J_ASSERT(!(EXT4_I(inode)->i_flags & EXT4_EXTENTS_FL));
	J_ASSERT(handle != NULL || (flags & EXT4_GET_BLOCKS_CREATE) == 0);
	depth = ext4_block_to_path(inode, iblock, offsets,
				   &blocks_to_boundary);

	if (depth == 0)
		goto out;

	partial = ext4_get_branch(inode, depth, offsets, chain, &err);

	/* Simplest case - block found, no allocation needed */
	if (!partial) {
		first_block = le32_to_cpu(chain[depth - 1].key);
		clear_buffer_new(bh_result);
		count++;
		/*map more blocks*/
		while (count < maxblocks && count <= blocks_to_boundary) {
			ext4_fsblk_t blk;

			blk = le32_to_cpu(*(chain[depth-1].p + count));

			if (blk == first_block + count)
				count++;
			else
				break;
		}
		goto got_it;
	}

	/* Next simple case - plain lookup or failed read of indirect block */
	if ((flags & EXT4_GET_BLOCKS_CREATE) == 0 || err == -EIO)
		goto cleanup;

	/*
	 * Okay, we need to do block allocation.
	*/
	goal = ext4_find_goal(inode, iblock, partial);

	/* the number of blocks need to allocate for [d,t]indirect blocks */
	indirect_blks = (chain + depth) - partial - 1;

	/*
	 * Next look up the indirect map to count the totoal number of
	 * direct blocks to allocate for this branch.
	 */
	count = ext4_blks_to_allocate(partial, indirect_blks,
					maxblocks, blocks_to_boundary);
	/*
	 * Block out ext4_truncate while we alter the tree
	 */
	err = ext4_alloc_branch(handle, inode, iblock, indirect_blks,
				&count, goal,
				offsets + (partial - chain), partial);

	/*
	 * The ext4_splice_branch call will free and forget any buffers
	 * on the new chain if there is a failure, but that risks using
	 * up transaction credits, especially for bitmaps where the
	 * credits cannot be returned.  Can we handle this somehow?  We
	 * may need to return -EAGAIN upwards in the worst case.  --sct
	 */
	if (!err)
		err = ext4_splice_branch(handle, inode, iblock,
					 partial, indirect_blks, count);
	if (err)
		goto cleanup;

	set_buffer_new(bh_result);

	ext4_update_inode_fsync_trans(handle, inode, 1);
got_it:
	map_bh(bh_result, inode->i_sb, le32_to_cpu(chain[depth-1].key));
	if (count > blocks_to_boundary)
		set_buffer_boundary(bh_result);
	err = count;
	/* Clean up and exit */
	partial = chain + depth - 1;	/* the whole chain */
cleanup:
	while (partial > chain) {
		BUFFER_TRACE(partial->bh, "call brelse");
		brelse(partial->bh);
		partial--;
	}
	BUFFER_TRACE(bh_result, "returned");
out:
	return err;
}

qsize_t ext4_get_reserved_space(struct inode *inode)
{
	unsigned long long total;

	spin_lock(&EXT4_I(inode)->i_block_reservation_lock);
	total = EXT4_I(inode)->i_reserved_data_blocks +
		EXT4_I(inode)->i_reserved_meta_blocks;
	spin_unlock(&EXT4_I(inode)->i_block_reservation_lock);

	return (total << inode->i_blkbits);
}
/*
 * Calculate the number of metadata blocks need to reserve
 * to allocate @blocks for non extent file based file
 */
static int ext4_indirect_calc_metadata_amount(struct inode *inode, int blocks)
{
	int icap = EXT4_ADDR_PER_BLOCK(inode->i_sb);
	int ind_blks, dind_blks, tind_blks;

	/* number of new indirect blocks needed */
	ind_blks = (blocks + icap - 1) / icap;

	dind_blks = (ind_blks + icap - 1) / icap;

	tind_blks = 1;

	return ind_blks + dind_blks + tind_blks;
}

/*
 * Calculate the number of metadata blocks need to reserve
 * to allocate given number of blocks
 */
static int ext4_calc_metadata_amount(struct inode *inode, int blocks)
{
	if (!blocks)
		return 0;

	if (EXT4_I(inode)->i_flags & EXT4_EXTENTS_FL)
		return ext4_ext_calc_metadata_amount(inode, blocks);

	return ext4_indirect_calc_metadata_amount(inode, blocks);
}

static void ext4_da_update_reserve_space(struct inode *inode, int used)
{
	struct ext4_sb_info *sbi = EXT4_SB(inode->i_sb);
	int total, mdb, mdb_free;

	spin_lock(&EXT4_I(inode)->i_block_reservation_lock);
	/* recalculate the number of metablocks still need to be reserved */
	total = EXT4_I(inode)->i_reserved_data_blocks - used;
	mdb = ext4_calc_metadata_amount(inode, total);

	/* figure out how many metablocks to release */
	BUG_ON(mdb > EXT4_I(inode)->i_reserved_meta_blocks);
	mdb_free = EXT4_I(inode)->i_reserved_meta_blocks - mdb;

	if (mdb_free) {
		/* Account for allocated meta_blocks */
		mdb_free -= EXT4_I(inode)->i_allocated_meta_blocks;

		/* update fs dirty blocks counter */
		percpu_counter_sub(&sbi->s_dirtyblocks_counter, mdb_free);
		EXT4_I(inode)->i_allocated_meta_blocks = 0;
		EXT4_I(inode)->i_reserved_meta_blocks = mdb;
	}

	/* update per-inode reservations */
	BUG_ON(used  > EXT4_I(inode)->i_reserved_data_blocks);
	EXT4_I(inode)->i_reserved_data_blocks -= used;
	spin_unlock(&EXT4_I(inode)->i_block_reservation_lock);

	/*
	 * free those over-booking quota for metadata blocks
	 */
	if (mdb_free)
		vfs_dq_release_reservation_block(inode, mdb_free);

	/*
	 * If we have done all the pending block allocations and if
	 * there aren't any writers on the inode, we can discard the
	 * inode's preallocations.
	 */
	if (!total && (atomic_read(&inode->i_writecount) == 0))
		ext4_discard_preallocations(inode);
}

static int check_block_validity(struct inode *inode, const char *msg,
				sector_t logical, sector_t phys, int len)
{
	if (!ext4_data_block_valid(EXT4_SB(inode->i_sb), phys, len)) {
		ext4_error(inode->i_sb, msg,
			   "inode #%lu logical block %llu mapped to %llu "
			   "(size %d)", inode->i_ino,
			   (unsigned long long) logical,
			   (unsigned long long) phys, len);
		return -EIO;
	}
	return 0;
}

/*
 * Return the number of contiguous dirty pages in a given inode
 * starting at page frame idx.
 */
static pgoff_t ext4_num_dirty_pages(struct inode *inode, pgoff_t idx,
				    unsigned int max_pages)
{
	struct address_space *mapping = inode->i_mapping;
	pgoff_t	index;
	struct pagevec pvec;
	pgoff_t num = 0;
	int i, nr_pages, done = 0;

	if (max_pages == 0)
		return 0;
	pagevec_init(&pvec, 0);
	while (!done) {
		index = idx;
		nr_pages = pagevec_lookup_tag(&pvec, mapping, &index,
					      PAGECACHE_TAG_DIRTY,
					      (pgoff_t)PAGEVEC_SIZE);
		if (nr_pages == 0)
			break;
		for (i = 0; i < nr_pages; i++) {
			struct page *page = pvec.pages[i];
			struct buffer_head *bh, *head;

			lock_page(page);
			if (unlikely(page->mapping != mapping) ||
			    !PageDirty(page) ||
			    PageWriteback(page) ||
			    page->index != idx) {
				done = 1;
				unlock_page(page);
				break;
			}
			if (page_has_buffers(page)) {
				bh = head = page_buffers(page);
				do {
					if (!buffer_delay(bh) &&
					    !buffer_unwritten(bh))
						done = 1;
					bh = bh->b_this_page;
				} while (!done && (bh != head));
			}
			unlock_page(page);
			if (done)
				break;
			idx++;
			num++;
			if (num >= max_pages)
				break;
		}
		pagevec_release(&pvec);
	}
	return num;
}

/*
 * The ext4_get_blocks() function tries to look up the requested blocks,
 * and returns if the blocks are already mapped.
 *
 * Otherwise it takes the write lock of the i_data_sem and allocate blocks
 * and store the allocated blocks in the result buffer head and mark it
 * mapped.
 *
 * If file type is extents based, it will call ext4_ext_get_blocks(),
 * Otherwise, call with ext4_ind_get_blocks() to handle indirect mapping
 * based files
 *
 * On success, it returns the number of blocks being mapped or allocate.
 * if create==0 and the blocks are pre-allocated and uninitialized block,
 * the result buffer head is unmapped. If the create ==1, it will make sure
 * the buffer head is mapped.
 *
 * It returns 0 if plain look up failed (blocks have not been allocated), in
 * that casem, buffer head is unmapped
 *
 * It returns the error in case of allocation failure.
 */
int ext4_get_blocks(handle_t *handle, struct inode *inode, sector_t block,
		    unsigned int max_blocks, struct buffer_head *bh,
		    int flags)
{
	int retval;

	clear_buffer_mapped(bh);
	clear_buffer_unwritten(bh);

	ext_debug("ext4_get_blocks(): inode %lu, flag %d, max_blocks %u,"
		  "logical block %lu\n", inode->i_ino, flags, max_blocks,
		  (unsigned long)block);
	/*
	 * Try to see if we can get the block without requesting a new
	 * file system block.
	 */
	down_read((&EXT4_I(inode)->i_data_sem));
	if (EXT4_I(inode)->i_flags & EXT4_EXTENTS_FL) {
		retval =  ext4_ext_get_blocks(handle, inode, block, max_blocks,
				bh, 0);
	} else {
		retval = ext4_ind_get_blocks(handle, inode, block, max_blocks,
					     bh, 0);
	}
	up_read((&EXT4_I(inode)->i_data_sem));

	if (retval > 0 && buffer_mapped(bh)) {
		int ret = check_block_validity(inode, "file system corruption",
					       block, bh->b_blocknr, retval);
		if (ret != 0)
			return ret;
	}

	/* If it is only a block(s) look up */
	if ((flags & EXT4_GET_BLOCKS_CREATE) == 0)
		return retval;

	/*
	 * Returns if the blocks have already allocated
	 *
	 * Note that if blocks have been preallocated
	 * ext4_ext_get_block() returns th create = 0
	 * with buffer head unmapped.
	 */
	if (retval > 0 && buffer_mapped(bh))
		return retval;

	/*
	 * When we call get_blocks without the create flag, the
	 * BH_Unwritten flag could have gotten set if the blocks
	 * requested were part of a uninitialized extent.  We need to
	 * clear this flag now that we are committed to convert all or
	 * part of the uninitialized extent to be an initialized
	 * extent.  This is because we need to avoid the combination
	 * of BH_Unwritten and BH_Mapped flags being simultaneously
	 * set on the buffer_head.
	 */
	clear_buffer_unwritten(bh);

	/*
	 * New blocks allocate and/or writing to uninitialized extent
	 * will possibly result in updating i_data, so we take
	 * the write lock of i_data_sem, and call get_blocks()
	 * with create == 1 flag.
	 */
	down_write((&EXT4_I(inode)->i_data_sem));

	/*
	 * if the caller is from delayed allocation writeout path
	 * we have already reserved fs blocks for allocation
	 * let the underlying get_block() function know to
	 * avoid double accounting
	 */
	if (flags & EXT4_GET_BLOCKS_DELALLOC_RESERVE)
		EXT4_I(inode)->i_delalloc_reserved_flag = 1;
	/*
	 * We need to check for EXT4 here because migrate
	 * could have changed the inode type in between
	 */
	if (EXT4_I(inode)->i_flags & EXT4_EXTENTS_FL) {
		retval =  ext4_ext_get_blocks(handle, inode, block, max_blocks,
					      bh, flags);
	} else {
		retval = ext4_ind_get_blocks(handle, inode, block,
					     max_blocks, bh, flags);

		if (retval > 0 && buffer_new(bh)) {
			/*
			 * We allocated new blocks which will result in
			 * i_data's format changing.  Force the migrate
			 * to fail by clearing migrate flags
			 */
			EXT4_I(inode)->i_state &= ~EXT4_STATE_EXT_MIGRATE;
		}
	}

	if (flags & EXT4_GET_BLOCKS_DELALLOC_RESERVE)
		EXT4_I(inode)->i_delalloc_reserved_flag = 0;

	/*
	 * Update reserved blocks/metadata blocks after successful
	 * block allocation which had been deferred till now.
	 */
	if ((retval > 0) && (flags & EXT4_GET_BLOCKS_UPDATE_RESERVE_SPACE))
		ext4_da_update_reserve_space(inode, retval);

	up_write((&EXT4_I(inode)->i_data_sem));
	if (retval > 0 && buffer_mapped(bh)) {
		int ret = check_block_validity(inode, "file system "
					       "corruption after allocation",
					       block, bh->b_blocknr, retval);
		if (ret != 0)
			return ret;
	}
	return retval;
}

/* Maximum number of blocks we map for direct IO at once. */
#define DIO_MAX_BLOCKS 4096

int ext4_get_block(struct inode *inode, sector_t iblock,
		   struct buffer_head *bh_result, int create)
{
	handle_t *handle = ext4_journal_current_handle();
	int ret = 0, started = 0;
	unsigned max_blocks = bh_result->b_size >> inode->i_blkbits;
	int dio_credits;

	if (create && !handle) {
		/* Direct IO write... */
		if (max_blocks > DIO_MAX_BLOCKS)
			max_blocks = DIO_MAX_BLOCKS;
		dio_credits = ext4_chunk_trans_blocks(inode, max_blocks);
		handle = ext4_journal_start(inode, dio_credits);
		if (IS_ERR(handle)) {
			ret = PTR_ERR(handle);
			goto out;
		}
		started = 1;
	}

	ret = ext4_get_blocks(handle, inode, iblock, max_blocks, bh_result,
			      create ? EXT4_GET_BLOCKS_CREATE : 0);
	if (ret > 0) {
		bh_result->b_size = (ret << inode->i_blkbits);
		ret = 0;
	}
	if (started)
		ext4_journal_stop(handle);
out:
	return ret;
}

/*
 * `handle' can be NULL if create is zero
 */
struct buffer_head *ext4_getblk(handle_t *handle, struct inode *inode,
				ext4_lblk_t block, int create, int *errp)
{
	struct buffer_head dummy;
	int fatal = 0, err;
	int flags = 0;

	J_ASSERT(handle != NULL || create == 0);

	dummy.b_state = 0;
	dummy.b_blocknr = -1000;
	buffer_trace_init(&dummy.b_history);
	if (create)
		flags |= EXT4_GET_BLOCKS_CREATE;
	err = ext4_get_blocks(handle, inode, block, 1, &dummy, flags);
	/*
	 * ext4_get_blocks() returns number of blocks mapped. 0 in
	 * case of a HOLE.
	 */
	if (err > 0) {
		if (err > 1)
			WARN_ON(1);
		err = 0;
	}
	*errp = err;
	if (!err && buffer_mapped(&dummy)) {
		struct buffer_head *bh;
		bh = sb_getblk(inode->i_sb, dummy.b_blocknr);
		if (!bh) {
			*errp = -EIO;
			goto err;
		}
		if (buffer_new(&dummy)) {
			J_ASSERT(create != 0);
			J_ASSERT(handle != NULL);

			/*
			 * Now that we do not always journal data, we should
			 * keep in mind whether this should always journal the
			 * new buffer as metadata.  For now, regular file
			 * writes use ext4_get_block instead, so it's not a
			 * problem.
			 */
			lock_buffer(bh);
			BUFFER_TRACE(bh, "call get_create_access");
			fatal = ext4_journal_get_create_access(handle, bh);
			if (!fatal && !buffer_uptodate(bh)) {
				memset(bh->b_data, 0, inode->i_sb->s_blocksize);
				set_buffer_uptodate(bh);
			}
			unlock_buffer(bh);
			BUFFER_TRACE(bh, "call ext4_handle_dirty_metadata");
			err = ext4_handle_dirty_metadata(handle, inode, bh);
			if (!fatal)
				fatal = err;
		} else {
			BUFFER_TRACE(bh, "not a new buffer");
		}
		if (fatal) {
			*errp = fatal;
			brelse(bh);
			bh = NULL;
		}
		return bh;
	}
err:
	return NULL;
}

struct buffer_head *ext4_bread(handle_t *handle, struct inode *inode,
			       ext4_lblk_t block, int create, int *err)
{
	struct buffer_head *bh;

	bh = ext4_getblk(handle, inode, block, create, err);
	if (!bh)
		return bh;
	if (buffer_uptodate(bh))
		return bh;
	ll_rw_block(READ_META, 1, &bh);
	wait_on_buffer(bh);
	if (buffer_uptodate(bh))
		return bh;
	put_bh(bh);
	*err = -EIO;
	return NULL;
}

static int walk_page_buffers(handle_t *handle,
			     struct buffer_head *head,
			     unsigned from,
			     unsigned to,
			     int *partial,
			     int (*fn)(handle_t *handle,
				       struct buffer_head *bh))
{
	struct buffer_head *bh;
	unsigned block_start, block_end;
	unsigned blocksize = head->b_size;
	int err, ret = 0;
	struct buffer_head *next;

	for (bh = head, block_start = 0;
	     ret == 0 && (bh != head || !block_start);
	     block_start = block_end, bh = next) {
		next = bh->b_this_page;
		block_end = block_start + blocksize;
		if (block_end <= from || block_start >= to) {
			if (partial && !buffer_uptodate(bh))
				*partial = 1;
			continue;
		}
		err = (*fn)(handle, bh);
		if (!ret)
			ret = err;
	}
	return ret;
}

/*
 * To preserve ordering, it is essential that the hole instantiation and
 * the data write be encapsulated in a single transaction.  We cannot
 * close off a transaction and start a new one between the ext4_get_block()
 * and the commit_write().  So doing the jbd2_journal_start at the start of
 * prepare_write() is the right place.
 *
 * Also, this function can nest inside ext4_writepage() ->
 * block_write_full_page(). In that case, we *know* that ext4_writepage()
 * has generated enough buffer credits to do the whole page.  So we won't
 * block on the journal in that case, which is good, because the caller may
 * be PF_MEMALLOC.
 *
 * By accident, ext4 can be reentered when a transaction is open via
 * quota file writes.  If we were to commit the transaction while thus
 * reentered, there can be a deadlock - we would be holding a quota
 * lock, and the commit would never complete if another thread had a
 * transaction open and was blocking on the quota lock - a ranking
 * violation.
 *
 * So what we do is to rely on the fact that jbd2_journal_stop/journal_start
 * will _not_ run commit under these circumstances because handle->h_ref
 * is elevated.  We'll still have enough credits for the tiny quotafile
 * write.
 */
static int do_journal_get_write_access(handle_t *handle,
				       struct buffer_head *bh)
{
	if (!buffer_mapped(bh) || buffer_freed(bh))
		return 0;
	return ext4_journal_get_write_access(handle, bh);
}

/*
 * Truncate blocks that were not used by write. We have to truncate the
 * pagecache as well so that corresponding buffers get properly unmapped.
 */
static void ext4_truncate_failed_write(struct inode *inode)
{
	truncate_inode_pages(inode->i_mapping, inode->i_size);
	ext4_truncate(inode);
}

static int ext4_write_begin(struct file *file, struct address_space *mapping,
			    loff_t pos, unsigned len, unsigned flags,
			    struct page **pagep, void **fsdata)
{
	struct inode *inode = mapping->host;
	int ret, needed_blocks;
	handle_t *handle;
	int retries = 0;
	struct page *page;
	pgoff_t index;
	unsigned from, to;

	trace_ext4_write_begin(inode, pos, len, flags);
	/*
	 * Reserve one block more for addition to orphan list in case
	 * we allocate blocks but write fails for some reason
	 */
	needed_blocks = ext4_writepage_trans_blocks(inode) + 1;
	index = pos >> PAGE_CACHE_SHIFT;
	from = pos & (PAGE_CACHE_SIZE - 1);
	to = from + len;

retry:
	handle = ext4_journal_start(inode, needed_blocks);
	if (IS_ERR(handle)) {
		ret = PTR_ERR(handle);
		goto out;
	}

	/* We cannot recurse into the filesystem as the transaction is already
	 * started */
	flags |= AOP_FLAG_NOFS;

	page = grab_cache_page_write_begin(mapping, index, flags);
	if (!page) {
		ext4_journal_stop(handle);
		ret = -ENOMEM;
		goto out;
	}
	*pagep = page;

	ret = block_write_begin(file, mapping, pos, len, flags, pagep, fsdata,
				ext4_get_block);

	if (!ret && ext4_should_journal_data(inode)) {
		ret = walk_page_buffers(handle, page_buffers(page),
				from, to, NULL, do_journal_get_write_access);
	}

	if (ret) {
		unlock_page(page);
		page_cache_release(page);
		/*
		 * block_write_begin may have instantiated a few blocks
		 * outside i_size.  Trim these off again. Don't need
		 * i_size_read because we hold i_mutex.
		 *
		 * Add inode to orphan list in case we crash before
		 * truncate finishes
		 */
		if (pos + len > inode->i_size && ext4_can_truncate(inode))
			ext4_orphan_add(handle, inode);

		ext4_journal_stop(handle);
		if (pos + len > inode->i_size) {
			ext4_truncate_failed_write(inode);
			/*
			 * If truncate failed early the inode might
			 * still be on the orphan list; we need to
			 * make sure the inode is removed from the
			 * orphan list in that case.
			 */
			if (inode->i_nlink)
				ext4_orphan_del(NULL, inode);
		}
	}

	if (ret == -ENOSPC && ext4_should_retry_alloc(inode->i_sb, &retries))
		goto retry;
out:
	return ret;
}

/* For write_end() in data=journal mode */
static int write_end_fn(handle_t *handle, struct buffer_head *bh)
{
	if (!buffer_mapped(bh) || buffer_freed(bh))
		return 0;
	set_buffer_uptodate(bh);
	return ext4_handle_dirty_metadata(handle, NULL, bh);
}

static int ext4_generic_write_end(struct file *file,
				  struct address_space *mapping,
				  loff_t pos, unsigned len, unsigned copied,
				  struct page *page, void *fsdata)
{
	int i_size_changed = 0;
	struct inode *inode = mapping->host;
	handle_t *handle = ext4_journal_current_handle();

	copied = block_write_end(file, mapping, pos, len, copied, page, fsdata);

	/*
	 * No need to use i_size_read() here, the i_size
	 * cannot change under us because we hold i_mutex.
	 *
	 * But it's important to update i_size while still holding page lock:
	 * page writeout could otherwise come in and zero beyond i_size.
	 */
	if (pos + copied > inode->i_size) {
		i_size_write(inode, pos + copied);
		i_size_changed = 1;
	}

	if (pos + copied >  EXT4_I(inode)->i_disksize) {
		/* We need to mark inode dirty even if
		 * new_i_size is less that inode->i_size
		 * bu greater than i_disksize.(hint delalloc)
		 */
		ext4_update_i_disksize(inode, (pos + copied));
		i_size_changed = 1;
	}
	unlock_page(page);
	page_cache_release(page);

	/*
	 * Don't mark the inode dirty under page lock. First, it unnecessarily
	 * makes the holding time of page lock longer. Second, it forces lock
	 * ordering of page lock and transaction start for journaling
	 * filesystems.
	 */
	if (i_size_changed)
		ext4_mark_inode_dirty(handle, inode);

	return copied;
}

/*
 * We need to pick up the new inode size which generic_commit_write gave us
 * `file' can be NULL - eg, when called from page_symlink().
 *
 * ext4 never places buffers on inode->i_mapping->private_list.  metadata
 * buffers are managed internally.
 */
static int ext4_ordered_write_end(struct file *file,
				  struct address_space *mapping,
				  loff_t pos, unsigned len, unsigned copied,
				  struct page *page, void *fsdata)
{
	handle_t *handle = ext4_journal_current_handle();
	struct inode *inode = mapping->host;
	int ret = 0, ret2;

	trace_ext4_ordered_write_end(inode, pos, len, copied);
	ret = ext4_jbd2_file_inode(handle, inode);

	if (ret == 0) {
		ret2 = ext4_generic_write_end(file, mapping, pos, len, copied,
							page, fsdata);
		copied = ret2;
		if (pos + len > inode->i_size && ext4_can_truncate(inode))
			/* if we have allocated more blocks and copied
			 * less. We will have blocks allocated outside
			 * inode->i_size. So truncate them
			 */
			ext4_orphan_add(handle, inode);
		if (ret2 < 0)
			ret = ret2;
	}
	ret2 = ext4_journal_stop(handle);
	if (!ret)
		ret = ret2;

	if (pos + len > inode->i_size) {
		ext4_truncate_failed_write(inode);
		/*
		 * If truncate failed early the inode might still be
		 * on the orphan list; we need to make sure the inode
		 * is removed from the orphan list in that case.
		 */
		if (inode->i_nlink)
			ext4_orphan_del(NULL, inode);
	}


	return ret ? ret : copied;
}

static int ext4_writeback_write_end(struct file *file,
				    struct address_space *mapping,
				    loff_t pos, unsigned len, unsigned copied,
				    struct page *page, void *fsdata)
{
	handle_t *handle = ext4_journal_current_handle();
	struct inode *inode = mapping->host;
	int ret = 0, ret2;

	trace_ext4_writeback_write_end(inode, pos, len, copied);
	ret2 = ext4_generic_write_end(file, mapping, pos, len, copied,
							page, fsdata);
	copied = ret2;
	if (pos + len > inode->i_size && ext4_can_truncate(inode))
		/* if we have allocated more blocks and copied
		 * less. We will have blocks allocated outside
		 * inode->i_size. So truncate them
		 */
		ext4_orphan_add(handle, inode);

	if (ret2 < 0)
		ret = ret2;

	ret2 = ext4_journal_stop(handle);
	if (!ret)
		ret = ret2;

	if (pos + len > inode->i_size) {
		ext4_truncate_failed_write(inode);
		/*
		 * If truncate failed early the inode might still be
		 * on the orphan list; we need to make sure the inode
		 * is removed from the orphan list in that case.
		 */
		if (inode->i_nlink)
			ext4_orphan_del(NULL, inode);
	}

	return ret ? ret : copied;
}

static int ext4_journalled_write_end(struct file *file,
				     struct address_space *mapping,
				     loff_t pos, unsigned len, unsigned copied,
				     struct page *page, void *fsdata)
{
	handle_t *handle = ext4_journal_current_handle();
	struct inode *inode = mapping->host;
	int ret = 0, ret2;
	int partial = 0;
	unsigned from, to;
	loff_t new_i_size;

	trace_ext4_journalled_write_end(inode, pos, len, copied);
	from = pos & (PAGE_CACHE_SIZE - 1);
	to = from + len;

	if (copied < len) {
		if (!PageUptodate(page))
			copied = 0;
		page_zero_new_buffers(page, from+copied, to);
	}

	ret = walk_page_buffers(handle, page_buffers(page), from,
				to, &partial, write_end_fn);
	if (!partial)
		SetPageUptodate(page);
	new_i_size = pos + copied;
	if (new_i_size > inode->i_size)
		i_size_write(inode, pos+copied);
	EXT4_I(inode)->i_state |= EXT4_STATE_JDATA;
	if (new_i_size > EXT4_I(inode)->i_disksize) {
		ext4_update_i_disksize(inode, new_i_size);
		ret2 = ext4_mark_inode_dirty(handle, inode);
		if (!ret)
			ret = ret2;
	}

	unlock_page(page);
	page_cache_release(page);
	if (pos + len > inode->i_size && ext4_can_truncate(inode))
		/* if we have allocated more blocks and copied
		 * less. We will have blocks allocated outside
		 * inode->i_size. So truncate them
		 */
		ext4_orphan_add(handle, inode);

	ret2 = ext4_journal_stop(handle);
	if (!ret)
		ret = ret2;
	if (pos + len > inode->i_size) {
		ext4_truncate_failed_write(inode);
		/*
		 * If truncate failed early the inode might still be
		 * on the orphan list; we need to make sure the inode
		 * is removed from the orphan list in that case.
		 */
		if (inode->i_nlink)
			ext4_orphan_del(NULL, inode);
	}

	return ret ? ret : copied;
}

static int ext4_da_reserve_space(struct inode *inode, int nrblocks)
{
	int retries = 0;
	struct ext4_sb_info *sbi = EXT4_SB(inode->i_sb);
	unsigned long md_needed, mdblocks, total = 0;

	/*
	 * recalculate the amount of metadata blocks to reserve
	 * in order to allocate nrblocks
	 * worse case is one extent per block
	 */
repeat:
	spin_lock(&EXT4_I(inode)->i_block_reservation_lock);
	total = EXT4_I(inode)->i_reserved_data_blocks + nrblocks;
	mdblocks = ext4_calc_metadata_amount(inode, total);
	BUG_ON(mdblocks < EXT4_I(inode)->i_reserved_meta_blocks);

	md_needed = mdblocks - EXT4_I(inode)->i_reserved_meta_blocks;
	total = md_needed + nrblocks;

	/*
	 * Make quota reservation here to prevent quota overflow
	 * later. Real quota accounting is done at pages writeout
	 * time.
	 */
	if (vfs_dq_reserve_block(inode, total)) {
		spin_unlock(&EXT4_I(inode)->i_block_reservation_lock);
		return -EDQUOT;
	}

	if (ext4_claim_free_blocks(sbi, total)) {
		spin_unlock(&EXT4_I(inode)->i_block_reservation_lock);
		vfs_dq_release_reservation_block(inode, total);
		if (ext4_should_retry_alloc(inode->i_sb, &retries)) {
			yield();
			goto repeat;
		}
		return -ENOSPC;
	}
	EXT4_I(inode)->i_reserved_data_blocks += nrblocks;
	EXT4_I(inode)->i_reserved_meta_blocks = mdblocks;

	spin_unlock(&EXT4_I(inode)->i_block_reservation_lock);
	return 0;       /* success */
}

static void ext4_da_release_space(struct inode *inode, int to_free)
{
	struct ext4_sb_info *sbi = EXT4_SB(inode->i_sb);
	int total, mdb, mdb_free, release;

	if (!to_free)
		return;		/* Nothing to release, exit */

	spin_lock(&EXT4_I(inode)->i_block_reservation_lock);

	if (!EXT4_I(inode)->i_reserved_data_blocks) {
		/*
		 * if there is no reserved blocks, but we try to free some
		 * then the counter is messed up somewhere.
		 * but since this function is called from invalidate
		 * page, it's harmless to return without any action
		 */
		printk(KERN_INFO "ext4 delalloc try to release %d reserved "
			    "blocks for inode %lu, but there is no reserved "
			    "data blocks\n", to_free, inode->i_ino);
		spin_unlock(&EXT4_I(inode)->i_block_reservation_lock);
		return;
	}

	/* recalculate the number of metablocks still need to be reserved */
	total = EXT4_I(inode)->i_reserved_data_blocks - to_free;
	mdb = ext4_calc_metadata_amount(inode, total);

	/* figure out how many metablocks to release */
	BUG_ON(mdb > EXT4_I(inode)->i_reserved_meta_blocks);
	mdb_free = EXT4_I(inode)->i_reserved_meta_blocks - mdb;

	release = to_free + mdb_free;

	/* update fs dirty blocks counter for truncate case */
	percpu_counter_sub(&sbi->s_dirtyblocks_counter, release);

	/* update per-inode reservations */
	BUG_ON(to_free > EXT4_I(inode)->i_reserved_data_blocks);
	EXT4_I(inode)->i_reserved_data_blocks -= to_free;

	BUG_ON(mdb > EXT4_I(inode)->i_reserved_meta_blocks);
	EXT4_I(inode)->i_reserved_meta_blocks = mdb;
	spin_unlock(&EXT4_I(inode)->i_block_reservation_lock);

	vfs_dq_release_reservation_block(inode, release);
}

static void ext4_da_page_release_reservation(struct page *page,
					     unsigned long offset)
{
	int to_release = 0;
	struct buffer_head *head, *bh;
	unsigned int curr_off = 0;

	head = page_buffers(page);
	bh = head;
	do {
		unsigned int next_off = curr_off + bh->b_size;

		if ((offset <= curr_off) && (buffer_delay(bh))) {
			to_release++;
			clear_buffer_delay(bh);
		}
		curr_off = next_off;
	} while ((bh = bh->b_this_page) != head);
	ext4_da_release_space(page->mapping->host, to_release);
}

/*
 * Delayed allocation stuff
 */

/*
 * mpage_da_submit_io - walks through extent of pages and try to write
 * them with writepage() call back
 *
 * @mpd->inode: inode
 * @mpd->first_page: first page of the extent
 * @mpd->next_page: page after the last page of the extent
 *
 * By the time mpage_da_submit_io() is called we expect all blocks
 * to be allocated. this may be wrong if allocation failed.
 *
 * As pages are already locked by write_cache_pages(), we can't use it
 */
static int mpage_da_submit_io(struct mpage_da_data *mpd)
{
	long pages_skipped;
	struct pagevec pvec;
	unsigned long index, end;
	int ret = 0, err, nr_pages, i;
	struct inode *inode = mpd->inode;
	struct address_space *mapping = inode->i_mapping;

	BUG_ON(mpd->next_page <= mpd->first_page);
	/*
	 * We need to start from the first_page to the next_page - 1
	 * to make sure we also write the mapped dirty buffer_heads.
	 * If we look at mpd->b_blocknr we would only be looking
	 * at the currently mapped buffer_heads.
	 */
	index = mpd->first_page;
	end = mpd->next_page - 1;

	pagevec_init(&pvec, 0);
	while (index <= end) {
		nr_pages = pagevec_lookup(&pvec, mapping, index, PAGEVEC_SIZE);
		if (nr_pages == 0)
			break;
		for (i = 0; i < nr_pages; i++) {
			struct page *page = pvec.pages[i];

			index = page->index;
			if (index > end)
				break;
			index++;

			BUG_ON(!PageLocked(page));
			BUG_ON(PageWriteback(page));

			pages_skipped = mpd->wbc->pages_skipped;
			err = mapping->a_ops->writepage(page, mpd->wbc);
			if (!err && (pages_skipped == mpd->wbc->pages_skipped))
				/*
				 * have successfully written the page
				 * without skipping the same
				 */
				mpd->pages_written++;
			/*
			 * In error case, we have to continue because
			 * remaining pages are still locked
			 * XXX: unlock and re-dirty them?
			 */
			if (ret == 0)
				ret = err;
		}
		pagevec_release(&pvec);
	}
	return ret;
}

/*
 * mpage_put_bnr_to_bhs - walk blocks and assign them actual numbers
 *
 * @mpd->inode - inode to walk through
 * @exbh->b_blocknr - first block on a disk
 * @exbh->b_size - amount of space in bytes
 * @logical - first logical block to start assignment with
 *
 * the function goes through all passed space and put actual disk
 * block numbers into buffer heads, dropping BH_Delay and BH_Unwritten
 */
static void mpage_put_bnr_to_bhs(struct mpage_da_data *mpd, sector_t logical,
				 struct buffer_head *exbh)
{
	struct inode *inode = mpd->inode;
	struct address_space *mapping = inode->i_mapping;
	int blocks = exbh->b_size >> inode->i_blkbits;
	sector_t pblock = exbh->b_blocknr, cur_logical;
	struct buffer_head *head, *bh;
	pgoff_t index, end;
	struct pagevec pvec;
	int nr_pages, i;

	index = logical >> (PAGE_CACHE_SHIFT - inode->i_blkbits);
	end = (logical + blocks - 1) >> (PAGE_CACHE_SHIFT - inode->i_blkbits);
	cur_logical = index << (PAGE_CACHE_SHIFT - inode->i_blkbits);

	pagevec_init(&pvec, 0);

	while (index <= end) {
		/* XXX: optimize tail */
		nr_pages = pagevec_lookup(&pvec, mapping, index, PAGEVEC_SIZE);
		if (nr_pages == 0)
			break;
		for (i = 0; i < nr_pages; i++) {
			struct page *page = pvec.pages[i];

			index = page->index;
			if (index > end)
				break;
			index++;

			BUG_ON(!PageLocked(page));
			BUG_ON(PageWriteback(page));
			BUG_ON(!page_has_buffers(page));

			bh = page_buffers(page);
			head = bh;

			/* skip blocks out of the range */
			do {
				if (cur_logical >= logical)
					break;
				cur_logical++;
			} while ((bh = bh->b_this_page) != head);

			do {
				if (cur_logical >= logical + blocks)
					break;

				if (buffer_delay(bh) ||
						buffer_unwritten(bh)) {

					BUG_ON(bh->b_bdev != inode->i_sb->s_bdev);

					if (buffer_delay(bh)) {
						clear_buffer_delay(bh);
						bh->b_blocknr = pblock;
					} else {
						/*
						 * unwritten already should have
						 * blocknr assigned. Verify that
						 */
						clear_buffer_unwritten(bh);
						BUG_ON(bh->b_blocknr != pblock);
					}

				} else if (buffer_mapped(bh))
					BUG_ON(bh->b_blocknr != pblock);

				cur_logical++;
				pblock++;
			} while ((bh = bh->b_this_page) != head);
		}
		pagevec_release(&pvec);
	}
}


/*
 * __unmap_underlying_blocks - just a helper function to unmap
 * set of blocks described by @bh
 */
static inline void __unmap_underlying_blocks(struct inode *inode,
					     struct buffer_head *bh)
{
	struct block_device *bdev = inode->i_sb->s_bdev;
	int blocks, i;

	blocks = bh->b_size >> inode->i_blkbits;
	for (i = 0; i < blocks; i++)
		unmap_underlying_metadata(bdev, bh->b_blocknr + i);
}

static void ext4_da_block_invalidatepages(struct mpage_da_data *mpd,
					sector_t logical, long blk_cnt)
{
	int nr_pages, i;
	pgoff_t index, end;
	struct pagevec pvec;
	struct inode *inode = mpd->inode;
	struct address_space *mapping = inode->i_mapping;

	index = logical >> (PAGE_CACHE_SHIFT - inode->i_blkbits);
	end   = (logical + blk_cnt - 1) >>
				(PAGE_CACHE_SHIFT - inode->i_blkbits);
	while (index <= end) {
		nr_pages = pagevec_lookup(&pvec, mapping, index, PAGEVEC_SIZE);
		if (nr_pages == 0)
			break;
		for (i = 0; i < nr_pages; i++) {
			struct page *page = pvec.pages[i];
			index = page->index;
			if (index > end)
				break;
			index++;

			BUG_ON(!PageLocked(page));
			BUG_ON(PageWriteback(page));
			block_invalidatepage(page, 0);
			ClearPageUptodate(page);
			unlock_page(page);
		}
	}
	return;
}

static void ext4_print_free_blocks(struct inode *inode)
{
	struct ext4_sb_info *sbi = EXT4_SB(inode->i_sb);
	printk(KERN_CRIT "Total free blocks count %lld\n",
	       ext4_count_free_blocks(inode->i_sb));
	printk(KERN_CRIT "Free/Dirty block details\n");
	printk(KERN_CRIT "free_blocks=%lld\n",
	       (long long) percpu_counter_sum(&sbi->s_freeblocks_counter));
	printk(KERN_CRIT "dirty_blocks=%lld\n",
	       (long long) percpu_counter_sum(&sbi->s_dirtyblocks_counter));
	printk(KERN_CRIT "Block reservation details\n");
	printk(KERN_CRIT "i_reserved_data_blocks=%u\n",
	       EXT4_I(inode)->i_reserved_data_blocks);
	printk(KERN_CRIT "i_reserved_meta_blocks=%u\n",
	       EXT4_I(inode)->i_reserved_meta_blocks);
	return;
}

/*
 * mpage_da_map_blocks - go through given space
 *
 * @mpd - bh describing space
 *
 * The function skips space we know is already mapped to disk blocks.
 *
 */
static int mpage_da_map_blocks(struct mpage_da_data *mpd)
{
	int err, blks, get_blocks_flags;
	struct buffer_head new;
	sector_t next = mpd->b_blocknr;
	unsigned max_blocks = mpd->b_size >> mpd->inode->i_blkbits;
	loff_t disksize = EXT4_I(mpd->inode)->i_disksize;
	handle_t *handle = NULL;

	/*
	 * We consider only non-mapped and non-allocated blocks
	 */
	if ((mpd->b_state  & (1 << BH_Mapped)) &&
		!(mpd->b_state & (1 << BH_Delay)) &&
		!(mpd->b_state & (1 << BH_Unwritten)))
		return 0;

	/*
	 * If we didn't accumulate anything to write simply return
	 */
	if (!mpd->b_size)
		return 0;

	handle = ext4_journal_current_handle();
	BUG_ON(!handle);

	/*
	 * Call ext4_get_blocks() to allocate any delayed allocation
	 * blocks, or to convert an uninitialized extent to be
	 * initialized (in the case where we have written into
	 * one or more preallocated blocks).
	 *
	 * We pass in the magic EXT4_GET_BLOCKS_DELALLOC_RESERVE to
	 * indicate that we are on the delayed allocation path.  This
	 * affects functions in many different parts of the allocation
	 * call path.  This flag exists primarily because we don't
	 * want to change *many* call functions, so ext4_get_blocks()
	 * will set the magic i_delalloc_reserved_flag once the
	 * inode's allocation semaphore is taken.
	 *
	 * If the blocks in questions were delalloc blocks, set
	 * EXT4_GET_BLOCKS_DELALLOC_RESERVE so the delalloc accounting
	 * variables are updated after the blocks have been allocated.
	 */
	new.b_state = 0;
	get_blocks_flags = (EXT4_GET_BLOCKS_CREATE |
			    EXT4_GET_BLOCKS_DELALLOC_RESERVE);
	if (mpd->b_state & (1 << BH_Delay))
		get_blocks_flags |= EXT4_GET_BLOCKS_UPDATE_RESERVE_SPACE;
	blks = ext4_get_blocks(handle, mpd->inode, next, max_blocks,
			       &new, get_blocks_flags);
	if (blks < 0) {
		err = blks;
		/*
		 * If get block returns with error we simply
		 * return. Later writepage will redirty the page and
		 * writepages will find the dirty page again
		 */
		if (err == -EAGAIN)
			return 0;

		if (err == -ENOSPC &&
		    ext4_count_free_blocks(mpd->inode->i_sb)) {
			mpd->retval = err;
			return 0;
		}

		/*
		 * get block failure will cause us to loop in
		 * writepages, because a_ops->writepage won't be able
		 * to make progress. The page will be redirtied by
		 * writepage and writepages will again try to write
		 * the same.
		 */
		ext4_msg(mpd->inode->i_sb, KERN_CRIT,
			 "delayed block allocation failed for inode %lu at "
			 "logical offset %llu with max blocks %zd with "
			 "error %d\n", mpd->inode->i_ino,
			 (unsigned long long) next,
			 mpd->b_size >> mpd->inode->i_blkbits, err);
		printk(KERN_CRIT "This should not happen!!  "
		       "Data will be lost\n");
		if (err == -ENOSPC) {
			ext4_print_free_blocks(mpd->inode);
		}
		/* invalidate all the pages */
		ext4_da_block_invalidatepages(mpd, next,
				mpd->b_size >> mpd->inode->i_blkbits);
		return err;
	}
	BUG_ON(blks == 0);

	new.b_size = (blks << mpd->inode->i_blkbits);

	if (buffer_new(&new))
		__unmap_underlying_blocks(mpd->inode, &new);

	/*
	 * If blocks are delayed marked, we need to
	 * put actual blocknr and drop delayed bit
	 */
	if ((mpd->b_state & (1 << BH_Delay)) ||
	    (mpd->b_state & (1 << BH_Unwritten)))
		mpage_put_bnr_to_bhs(mpd, next, &new);

	if (ext4_should_order_data(mpd->inode)) {
		err = ext4_jbd2_file_inode(handle, mpd->inode);
		if (err)
			return err;
	}

	/*
	 * Update on-disk size along with block allocation.
	 */
	disksize = ((loff_t) next + blks) << mpd->inode->i_blkbits;
	if (disksize > i_size_read(mpd->inode))
		disksize = i_size_read(mpd->inode);
	if (disksize > EXT4_I(mpd->inode)->i_disksize) {
		ext4_update_i_disksize(mpd->inode, disksize);
		return ext4_mark_inode_dirty(handle, mpd->inode);
	}

	return 0;
}

#define BH_FLAGS ((1 << BH_Uptodate) | (1 << BH_Mapped) | \
		(1 << BH_Delay) | (1 << BH_Unwritten))

/*
 * mpage_add_bh_to_extent - try to add one more block to extent of blocks
 *
 * @mpd->lbh - extent of blocks
 * @logical - logical number of the block in the file
 * @bh - bh of the block (used to access block's state)
 *
 * the function is used to collect contig. blocks in same state
 */
static void mpage_add_bh_to_extent(struct mpage_da_data *mpd,
				   sector_t logical, size_t b_size,
				   unsigned long b_state)
{
	sector_t next;
	int nrblocks = mpd->b_size >> mpd->inode->i_blkbits;

	/* check if thereserved journal credits might overflow */
	if (!(EXT4_I(mpd->inode)->i_flags & EXT4_EXTENTS_FL)) {
		if (nrblocks >= EXT4_MAX_TRANS_DATA) {
			/*
			 * With non-extent format we are limited by the journal
			 * credit available.  Total credit needed to insert
			 * nrblocks contiguous blocks is dependent on the
			 * nrblocks.  So limit nrblocks.
			 */
			goto flush_it;
		} else if ((nrblocks + (b_size >> mpd->inode->i_blkbits)) >
				EXT4_MAX_TRANS_DATA) {
			/*
			 * Adding the new buffer_head would make it cross the
			 * allowed limit for which we have journal credit
			 * reserved. So limit the new bh->b_size
			 */
			b_size = (EXT4_MAX_TRANS_DATA - nrblocks) <<
						mpd->inode->i_blkbits;
			/* we will do mpage_da_submit_io in the next loop */
		}
	}
	/*
	 * First block in the extent
	 */
	if (mpd->b_size == 0) {
		mpd->b_blocknr = logical;
		mpd->b_size = b_size;
		mpd->b_state = b_state & BH_FLAGS;
		return;
	}

	next = mpd->b_blocknr + nrblocks;
	/*
	 * Can we merge the block to our big extent?
	 */
	if (logical == next && (b_state & BH_FLAGS) == mpd->b_state) {
		mpd->b_size += b_size;
		return;
	}

flush_it:
	/*
	 * We couldn't merge the block to our extent, so we
	 * need to flush current  extent and start new one
	 */
	if (mpage_da_map_blocks(mpd) == 0)
		mpage_da_submit_io(mpd);
	mpd->io_done = 1;
	return;
}

static int ext4_bh_delay_or_unwritten(handle_t *handle, struct buffer_head *bh)
{
	return (buffer_delay(bh) || buffer_unwritten(bh)) && buffer_dirty(bh);
}

/*
 * __mpage_da_writepage - finds extent of pages and blocks
 *
 * @page: page to consider
 * @wbc: not used, we just follow rules
 * @data: context
 *
 * The function finds extents of pages and scan them for all blocks.
 */
static int __mpage_da_writepage(struct page *page,
				struct writeback_control *wbc, void *data)
{
	struct mpage_da_data *mpd = data;
	struct inode *inode = mpd->inode;
	struct buffer_head *bh, *head;
	sector_t logical;

	if (mpd->io_done) {
		/*
		 * Rest of the page in the page_vec
		 * redirty then and skip then. We will
		 * try to write them again after
		 * starting a new transaction
		 */
		redirty_page_for_writepage(wbc, page);
		unlock_page(page);
		return MPAGE_DA_EXTENT_TAIL;
	}
	/*
	 * Can we merge this page to current extent?
	 */
	if (mpd->next_page != page->index) {
		/*
		 * Nope, we can't. So, we map non-allocated blocks
		 * and start IO on them using writepage()
		 */
		if (mpd->next_page != mpd->first_page) {
			if (mpage_da_map_blocks(mpd) == 0)
				mpage_da_submit_io(mpd);
			/*
			 * skip rest of the page in the page_vec
			 */
			mpd->io_done = 1;
			redirty_page_for_writepage(wbc, page);
			unlock_page(page);
			return MPAGE_DA_EXTENT_TAIL;
		}

		/*
		 * Start next extent of pages ...
		 */
		mpd->first_page = page->index;

		/*
		 * ... and blocks
		 */
		mpd->b_size = 0;
		mpd->b_state = 0;
		mpd->b_blocknr = 0;
	}

	mpd->next_page = page->index + 1;
	logical = (sector_t) page->index <<
		  (PAGE_CACHE_SHIFT - inode->i_blkbits);

	if (!page_has_buffers(page)) {
		mpage_add_bh_to_extent(mpd, logical, PAGE_CACHE_SIZE,
				       (1 << BH_Dirty) | (1 << BH_Uptodate));
		if (mpd->io_done)
			return MPAGE_DA_EXTENT_TAIL;
	} else {
		/*
		 * Page with regular buffer heads, just add all dirty ones
		 */
		head = page_buffers(page);
		bh = head;
		do {
			BUG_ON(buffer_locked(bh));
			/*
			 * We need to try to allocate
			 * unmapped blocks in the same page.
			 * Otherwise we won't make progress
			 * with the page in ext4_writepage
			 */
			if (ext4_bh_delay_or_unwritten(NULL, bh)) {
				mpage_add_bh_to_extent(mpd, logical,
						       bh->b_size,
						       bh->b_state);
				if (mpd->io_done)
					return MPAGE_DA_EXTENT_TAIL;
			} else if (buffer_dirty(bh) && (buffer_mapped(bh))) {
				/*
				 * mapped dirty buffer. We need to update
				 * the b_state because we look at
				 * b_state in mpage_da_map_blocks. We don't
				 * update b_size because if we find an
				 * unmapped buffer_head later we need to
				 * use the b_state flag of that buffer_head.
				 */
				if (mpd->b_size == 0)
					mpd->b_state = bh->b_state & BH_FLAGS;
			}
			logical++;
		} while ((bh = bh->b_this_page) != head);
	}

	return 0;
}

/*
 * This is a special get_blocks_t callback which is used by
 * ext4_da_write_begin().  It will either return mapped block or
 * reserve space for a single block.
 *
 * For delayed buffer_head we have BH_Mapped, BH_New, BH_Delay set.
 * We also have b_blocknr = -1 and b_bdev initialized properly
 *
 * For unwritten buffer_head we have BH_Mapped, BH_New, BH_Unwritten set.
 * We also have b_blocknr = physicalblock mapping unwritten extent and b_bdev
 * initialized properly.
 */
static int ext4_da_get_block_prep(struct inode *inode, sector_t iblock,
				  struct buffer_head *bh_result, int create)
{
	int ret = 0;
	sector_t invalid_block = ~((sector_t) 0xffff);

	if (invalid_block < ext4_blocks_count(EXT4_SB(inode->i_sb)->s_es))
		invalid_block = ~0;

	BUG_ON(create == 0);
	BUG_ON(bh_result->b_size != inode->i_sb->s_blocksize);

	/*
	 * first, we need to know whether the block is allocated already
	 * preallocated blocks are unmapped but should treated
	 * the same as allocated blocks.
	 */
	ret = ext4_get_blocks(NULL, inode, iblock, 1,  bh_result, 0);
	if ((ret == 0) && !buffer_delay(bh_result)) {
		/* the block isn't (pre)allocated yet, let's reserve space */
		/*
		 * XXX: __block_prepare_write() unmaps passed block,
		 * is it OK?
		 */
		ret = ext4_da_reserve_space(inode, 1);
		if (ret)
			/* not enough space to reserve */
			return ret;

		map_bh(bh_result, inode->i_sb, invalid_block);
		set_buffer_new(bh_result);
		set_buffer_delay(bh_result);
	} else if (ret > 0) {
		bh_result->b_size = (ret << inode->i_blkbits);
		if (buffer_unwritten(bh_result)) {
			/* A delayed write to unwritten bh should
			 * be marked new and mapped.  Mapped ensures
			 * that we don't do get_block multiple times
			 * when we write to the same offset and new
			 * ensures that we do proper zero out for
			 * partial write.
			 */
			set_buffer_new(bh_result);
			set_buffer_mapped(bh_result);
		}
		ret = 0;
	}

	return ret;
}

/*
 * This function is used as a standard get_block_t calback function
 * when there is no desire to allocate any blocks.  It is used as a
 * callback function for block_prepare_write(), nobh_writepage(), and
 * block_write_full_page().  These functions should only try to map a
 * single block at a time.
 *
 * Since this function doesn't do block allocations even if the caller
 * requests it by passing in create=1, it is critically important that
 * any caller checks to make sure that any buffer heads are returned
 * by this function are either all already mapped or marked for
 * delayed allocation before calling nobh_writepage() or
 * block_write_full_page().  Otherwise, b_blocknr could be left
 * unitialized, and the page write functions will be taken by
 * surprise.
 */
static int noalloc_get_block_write(struct inode *inode, sector_t iblock,
				   struct buffer_head *bh_result, int create)
{
	int ret = 0;
	unsigned max_blocks = bh_result->b_size >> inode->i_blkbits;

	BUG_ON(bh_result->b_size != inode->i_sb->s_blocksize);

	/*
	 * we don't want to do block allocation in writepage
	 * so call get_block_wrap with create = 0
	 */
	ret = ext4_get_blocks(NULL, inode, iblock, max_blocks, bh_result, 0);
	if (ret > 0) {
		bh_result->b_size = (ret << inode->i_blkbits);
		ret = 0;
	}
	return ret;
}

static int bget_one(handle_t *handle, struct buffer_head *bh)
{
	get_bh(bh);
	return 0;
}

static int bput_one(handle_t *handle, struct buffer_head *bh)
{
	put_bh(bh);
	return 0;
}

static int __ext4_journalled_writepage(struct page *page,
				       struct writeback_control *wbc,
				       unsigned int len)
{
	struct address_space *mapping = page->mapping;
	struct inode *inode = mapping->host;
	struct buffer_head *page_bufs;
	handle_t *handle = NULL;
	int ret = 0;
	int err;

	page_bufs = page_buffers(page);
	BUG_ON(!page_bufs);
	walk_page_buffers(handle, page_bufs, 0, len, NULL, bget_one);
	/* As soon as we unlock the page, it can go away, but we have
	 * references to buffers so we are safe */
	unlock_page(page);

	handle = ext4_journal_start(inode, ext4_writepage_trans_blocks(inode));
	if (IS_ERR(handle)) {
		ret = PTR_ERR(handle);
		goto out;
	}

	ret = walk_page_buffers(handle, page_bufs, 0, len, NULL,
				do_journal_get_write_access);

	err = walk_page_buffers(handle, page_bufs, 0, len, NULL,
				write_end_fn);
	if (ret == 0)
		ret = err;
	err = ext4_journal_stop(handle);
	if (!ret)
		ret = err;

	walk_page_buffers(handle, page_bufs, 0, len, NULL, bput_one);
	EXT4_I(inode)->i_state |= EXT4_STATE_JDATA;
out:
	return ret;
}

/*
 * Note that we don't need to start a transaction unless we're journaling data
 * because we should have holes filled from ext4_page_mkwrite(). We even don't
 * need to file the inode to the transaction's list in ordered mode because if
 * we are writing back data added by write(), the inode is already there and if
 * we are writing back data modified via mmap(), noone guarantees in which
 * transaction the data will hit the disk. In case we are journaling data, we
 * cannot start transaction directly because transaction start ranks above page
 * lock so we have to do some magic.
 *
 * This function can get called via...
 *   - ext4_da_writepages after taking page lock (have journal handle)
 *   - journal_submit_inode_data_buffers (no journal handle)
 *   - shrink_page_list via pdflush (no journal handle)
 *   - grab_page_cache when doing write_begin (have journal handle)
 *
 * We don't do any block allocation in this function. If we have page with
 * multiple blocks we need to write those buffer_heads that are mapped. This
 * is important for mmaped based write. So if we do with blocksize 1K
 * truncate(f, 1024);
 * a = mmap(f, 0, 4096);
 * a[0] = 'a';
 * truncate(f, 4096);
 * we have in the page first buffer_head mapped via page_mkwrite call back
 * but other bufer_heads would be unmapped but dirty(dirty done via the
 * do_wp_page). So writepage should write the first block. If we modify
 * the mmap area beyond 1024 we will again get a page_fault and the
 * page_mkwrite callback will do the block allocation and mark the
 * buffer_heads mapped.
 *
 * We redirty the page if we have any buffer_heads that is either delay or
 * unwritten in the page.
 *
 * We can get recursively called as show below.
 *
 *	ext4_writepage() -> kmalloc() -> __alloc_pages() -> page_launder() ->
 *		ext4_writepage()
 *
 * But since we don't do any block allocation we should not deadlock.
 * Page also have the dirty flag cleared so we don't get recurive page_lock.
 */
static int ext4_writepage(struct page *page,
			  struct writeback_control *wbc)
{
	int ret = 0;
	loff_t size;
	unsigned int len;
	struct buffer_head *page_bufs;
	struct inode *inode = page->mapping->host;

	trace_ext4_writepage(inode, page);
	size = i_size_read(inode);
	if (page->index == size >> PAGE_CACHE_SHIFT)
		len = size & ~PAGE_CACHE_MASK;
	else
		len = PAGE_CACHE_SIZE;

	if (page_has_buffers(page)) {
		page_bufs = page_buffers(page);
		if (walk_page_buffers(NULL, page_bufs, 0, len, NULL,
					ext4_bh_delay_or_unwritten)) {
			/*
			 * We don't want to do  block allocation
			 * So redirty the page and return
			 * We may reach here when we do a journal commit
			 * via journal_submit_inode_data_buffers.
			 * If we don't have mapping block we just ignore
			 * them. We can also reach here via shrink_page_list
			 */
			redirty_page_for_writepage(wbc, page);
			unlock_page(page);
			return 0;
		}
	} else {
		/*
		 * The test for page_has_buffers() is subtle:
		 * We know the page is dirty but it lost buffers. That means
		 * that at some moment in time after write_begin()/write_end()
		 * has been called all buffers have been clean and thus they
		 * must have been written at least once. So they are all
		 * mapped and we can happily proceed with mapping them
		 * and writing the page.
		 *
		 * Try to initialize the buffer_heads and check whether
		 * all are mapped and non delay. We don't want to
		 * do block allocation here.
		 */
		ret = block_prepare_write(page, 0, len,
					  noalloc_get_block_write);
		if (!ret) {
			page_bufs = page_buffers(page);
			/* check whether all are mapped and non delay */
			if (walk_page_buffers(NULL, page_bufs, 0, len, NULL,
						ext4_bh_delay_or_unwritten)) {
				redirty_page_for_writepage(wbc, page);
				unlock_page(page);
				return 0;
			}
		} else {
			/*
			 * We can't do block allocation here
			 * so just redity the page and unlock
			 * and return
			 */
			redirty_page_for_writepage(wbc, page);
			unlock_page(page);
			return 0;
		}
		/* now mark the buffer_heads as dirty and uptodate */
		block_commit_write(page, 0, len);
	}

	if (PageChecked(page) && ext4_should_journal_data(inode)) {
		/*
		 * It's mmapped pagecache.  Add buffers and journal it.  There
		 * doesn't seem much point in redirtying the page here.
		 */
		ClearPageChecked(page);
		return __ext4_journalled_writepage(page, wbc, len);
	}

	if (test_opt(inode->i_sb, NOBH) && ext4_should_writeback_data(inode))
		ret = nobh_writepage(page, noalloc_get_block_write, wbc);
	else
		ret = block_write_full_page(page, noalloc_get_block_write,
					    wbc);

	return ret;
}

/*
 * This is called via ext4_da_writepages() to
 * calulate the total number of credits to reserve to fit
 * a single extent allocation into a single transaction,
 * ext4_da_writpeages() will loop calling this before
 * the block allocation.
 */

static int ext4_da_writepages_trans_blocks(struct inode *inode)
{
	int max_blocks = EXT4_I(inode)->i_reserved_data_blocks;

	/*
	 * With non-extent format the journal credit needed to
	 * insert nrblocks contiguous block is dependent on
	 * number of contiguous block. So we will limit
	 * number of contiguous block to a sane value
	 */
	if (!(EXT4_I(inode)->i_flags & EXT4_EXTENTS_FL) &&
	    (max_blocks > EXT4_MAX_TRANS_DATA))
		max_blocks = EXT4_MAX_TRANS_DATA;

	return ext4_chunk_trans_blocks(inode, max_blocks);
}

static int ext4_da_writepages(struct address_space *mapping,
			      struct writeback_control *wbc)
{
	pgoff_t	index;
	int range_whole = 0;
	handle_t *handle = NULL;
	struct mpage_da_data mpd;
	struct inode *inode = mapping->host;
	int no_nrwrite_index_update;
	int pages_written = 0;
	long pages_skipped;
	unsigned int max_pages;
	int range_cyclic, cycled = 1, io_done = 0;
	int needed_blocks, ret = 0;
	long desired_nr_to_write, nr_to_writebump = 0;
	loff_t range_start = wbc->range_start;
	struct ext4_sb_info *sbi = EXT4_SB(mapping->host->i_sb);

	trace_ext4_da_writepages(inode, wbc);

	/*
	 * No pages to write? This is mainly a kludge to avoid starting
	 * a transaction for special inodes like journal inode on last iput()
	 * because that could violate lock ordering on umount
	 */
	if (!mapping->nrpages || !mapping_tagged(mapping, PAGECACHE_TAG_DIRTY))
		return 0;

	/*
	 * If the filesystem has aborted, it is read-only, so return
	 * right away instead of dumping stack traces later on that
	 * will obscure the real source of the problem.  We test
	 * EXT4_MF_FS_ABORTED instead of sb->s_flag's MS_RDONLY because
	 * the latter could be true if the filesystem is mounted
	 * read-only, and in that case, ext4_da_writepages should
	 * *never* be called, so if that ever happens, we would want
	 * the stack trace.
	 */
	if (unlikely(sbi->s_mount_flags & EXT4_MF_FS_ABORTED))
		return -EROFS;

	if (wbc->range_start == 0 && wbc->range_end == LLONG_MAX)
		range_whole = 1;

	range_cyclic = wbc->range_cyclic;
	if (wbc->range_cyclic) {
		index = mapping->writeback_index;
		if (index)
			cycled = 0;
		wbc->range_start = index << PAGE_CACHE_SHIFT;
		wbc->range_end  = LLONG_MAX;
		wbc->range_cyclic = 0;
	} else
		index = wbc->range_start >> PAGE_CACHE_SHIFT;

	/*
	 * This works around two forms of stupidity.  The first is in
	 * the writeback code, which caps the maximum number of pages
	 * written to be 1024 pages.  This is wrong on multiple
	 * levels; different architectues have a different page size,
	 * which changes the maximum amount of data which gets
	 * written.  Secondly, 4 megabytes is way too small.  XFS
	 * forces this value to be 16 megabytes by multiplying
	 * nr_to_write parameter by four, and then relies on its
	 * allocator to allocate larger extents to make them
	 * contiguous.  Unfortunately this brings us to the second
	 * stupidity, which is that ext4's mballoc code only allocates
	 * at most 2048 blocks.  So we force contiguous writes up to
	 * the number of dirty blocks in the inode, or
	 * sbi->max_writeback_mb_bump whichever is smaller.
	 */
	max_pages = sbi->s_max_writeback_mb_bump << (20 - PAGE_CACHE_SHIFT);
	if (!range_cyclic && range_whole)
		desired_nr_to_write = wbc->nr_to_write * 8;
	else
		desired_nr_to_write = ext4_num_dirty_pages(inode, index,
							   max_pages);
	if (desired_nr_to_write > max_pages)
		desired_nr_to_write = max_pages;

	if (wbc->nr_to_write < desired_nr_to_write) {
		nr_to_writebump = desired_nr_to_write - wbc->nr_to_write;
		wbc->nr_to_write = desired_nr_to_write;
	}

	mpd.wbc = wbc;
	mpd.inode = mapping->host;

	/*
	 * we don't want write_cache_pages to update
	 * nr_to_write and writeback_index
	 */
	no_nrwrite_index_update = wbc->no_nrwrite_index_update;
	wbc->no_nrwrite_index_update = 1;
	pages_skipped = wbc->pages_skipped;

retry:
	while (!ret && wbc->nr_to_write > 0) {

		/*
		 * we  insert one extent at a time. So we need
		 * credit needed for single extent allocation.
		 * journalled mode is currently not supported
		 * by delalloc
		 */
		BUG_ON(ext4_should_journal_data(inode));
		needed_blocks = ext4_da_writepages_trans_blocks(inode);

		/* start a new transaction*/
		handle = ext4_journal_start(inode, needed_blocks);
		if (IS_ERR(handle)) {
			ret = PTR_ERR(handle);
			ext4_msg(inode->i_sb, KERN_CRIT, "%s: jbd2_start: "
			       "%ld pages, ino %lu; err %d\n", __func__,
				wbc->nr_to_write, inode->i_ino, ret);
			goto out_writepages;
		}

		/*
		 * Now call __mpage_da_writepage to find the next
		 * contiguous region of logical blocks that need
		 * blocks to be allocated by ext4.  We don't actually
		 * submit the blocks for I/O here, even though
		 * write_cache_pages thinks it will, and will set the
		 * pages as clean for write before calling
		 * __mpage_da_writepage().
		 */
		mpd.b_size = 0;
		mpd.b_state = 0;
		mpd.b_blocknr = 0;
		mpd.first_page = 0;
		mpd.next_page = 0;
		mpd.io_done = 0;
		mpd.pages_written = 0;
		mpd.retval = 0;
		ret = write_cache_pages(mapping, wbc, __mpage_da_writepage,
					&mpd);
		/*
		 * If we have a contigous extent of pages and we
		 * haven't done the I/O yet, map the blocks and submit
		 * them for I/O.
		 */
		if (!mpd.io_done && mpd.next_page != mpd.first_page) {
			if (mpage_da_map_blocks(&mpd) == 0)
				mpage_da_submit_io(&mpd);
			mpd.io_done = 1;
			ret = MPAGE_DA_EXTENT_TAIL;
		}
		trace_ext4_da_write_pages(inode, &mpd);
		wbc->nr_to_write -= mpd.pages_written;

		ext4_journal_stop(handle);

		if ((mpd.retval == -ENOSPC) && sbi->s_journal) {
			/* commit the transaction which would
			 * free blocks released in the transaction
			 * and try again
			 */
			jbd2_journal_force_commit_nested(sbi->s_journal);
			wbc->pages_skipped = pages_skipped;
			ret = 0;
		} else if (ret == MPAGE_DA_EXTENT_TAIL) {
			/*
			 * got one extent now try with
			 * rest of the pages
			 */
			pages_written += mpd.pages_written;
			wbc->pages_skipped = pages_skipped;
			ret = 0;
			io_done = 1;
		} else if (wbc->nr_to_write)
			/*
			 * There is no more writeout needed
			 * or we requested for a noblocking writeout
			 * and we found the device congested
			 */
			break;
	}
	if (!io_done && !cycled) {
		cycled = 1;
		index = 0;
		wbc->range_start = index << PAGE_CACHE_SHIFT;
		wbc->range_end  = mapping->writeback_index - 1;
		goto retry;
	}
	if (pages_skipped != wbc->pages_skipped)
		ext4_msg(inode->i_sb, KERN_CRIT,
			 "This should not happen leaving %s "
			 "with nr_to_write = %ld ret = %d\n",
			 __func__, wbc->nr_to_write, ret);

	/* Update index */
	index += pages_written;
	wbc->range_cyclic = range_cyclic;
	if (wbc->range_cyclic || (range_whole && wbc->nr_to_write > 0))
		/*
		 * set the writeback_index so that range_cyclic
		 * mode will write it back later
		 */
		mapping->writeback_index = index;

out_writepages:
	if (!no_nrwrite_index_update)
		wbc->no_nrwrite_index_update = 0;
	if (wbc->nr_to_write > nr_to_writebump)
		wbc->nr_to_write -= nr_to_writebump;
	wbc->range_start = range_start;
	trace_ext4_da_writepages_result(inode, wbc, ret, pages_written);
	return ret;
}

#define FALL_BACK_TO_NONDELALLOC 1
static int ext4_nonda_switch(struct super_block *sb)
{
	s64 free_blocks, dirty_blocks;
	struct ext4_sb_info *sbi = EXT4_SB(sb);

	/*
	 * switch to non delalloc mode if we are running low
	 * on free block. The free block accounting via percpu
	 * counters can get slightly wrong with percpu_counter_batch getting
	 * accumulated on each CPU without updating global counters
	 * Delalloc need an accurate free block accounting. So switch
	 * to non delalloc when we are near to error range.
	 */
	free_blocks  = percpu_counter_read_positive(&sbi->s_freeblocks_counter);
	dirty_blocks = percpu_counter_read_positive(&sbi->s_dirtyblocks_counter);
	if (2 * free_blocks < 3 * dirty_blocks ||
		free_blocks < (dirty_blocks + EXT4_FREEBLOCKS_WATERMARK)) {
		/*
		 * free block count is less that 150% of dirty blocks
		 * or free blocks is less that watermark
		 */
		return 1;
	}
	return 0;
}

static int ext4_da_write_begin(struct file *file, struct address_space *mapping,
			       loff_t pos, unsigned len, unsigned flags,
			       struct page **pagep, void **fsdata)
{
	int ret, retries = 0;
	struct page *page;
	pgoff_t index;
	unsigned from, to;
	struct inode *inode = mapping->host;
	handle_t *handle;

	index = pos >> PAGE_CACHE_SHIFT;
	from = pos & (PAGE_CACHE_SIZE - 1);
	to = from + len;

	if (ext4_nonda_switch(inode->i_sb)) {
		*fsdata = (void *)FALL_BACK_TO_NONDELALLOC;
		return ext4_write_begin(file, mapping, pos,
					len, flags, pagep, fsdata);
	}
	*fsdata = (void *)0;
	trace_ext4_da_write_begin(inode, pos, len, flags);
retry:
	/*
	 * With delayed allocation, we don't log the i_disksize update
	 * if there is delayed block allocation. But we still need
	 * to journalling the i_disksize update if writes to the end
	 * of file which has an already mapped buffer.
	 */
	handle = ext4_journal_start(inode, 1);
	if (IS_ERR(handle)) {
		ret = PTR_ERR(handle);
		goto out;
	}
	/* We cannot recurse into the filesystem as the transaction is already
	 * started */
	flags |= AOP_FLAG_NOFS;

	page = grab_cache_page_write_begin(mapping, index, flags);
	if (!page) {
		ext4_journal_stop(handle);
		ret = -ENOMEM;
		goto out;
	}
	*pagep = page;

	ret = block_write_begin(file, mapping, pos, len, flags, pagep, fsdata,
				ext4_da_get_block_prep);
	if (ret < 0) {
		unlock_page(page);
		ext4_journal_stop(handle);
		page_cache_release(page);
		/*
		 * block_write_begin may have instantiated a few blocks
		 * outside i_size.  Trim these off again. Don't need
		 * i_size_read because we hold i_mutex.
		 */
		if (pos + len > inode->i_size)
			ext4_truncate_failed_write(inode);
	}

	if (ret == -ENOSPC && ext4_should_retry_alloc(inode->i_sb, &retries))
		goto retry;
out:
	return ret;
}

/*
 * Check if we should update i_disksize
 * when write to the end of file but not require block allocation
 */
static int ext4_da_should_update_i_disksize(struct page *page,
					    unsigned long offset)
{
	struct buffer_head *bh;
	struct inode *inode = page->mapping->host;
	unsigned int idx;
	int i;

	bh = page_buffers(page);
	idx = offset >> inode->i_blkbits;

	for (i = 0; i < idx; i++)
		bh = bh->b_this_page;

	if (!buffer_mapped(bh) || (buffer_delay(bh)) || buffer_unwritten(bh))
		return 0;
	return 1;
}

static int ext4_da_write_end(struct file *file,
			     struct address_space *mapping,
			     loff_t pos, unsigned len, unsigned copied,
			     struct page *page, void *fsdata)
{
	struct inode *inode = mapping->host;
	int ret = 0, ret2;
	handle_t *handle = ext4_journal_current_handle();
	loff_t new_i_size;
	unsigned long start, end;
	int write_mode = (int)(unsigned long)fsdata;

	if (write_mode == FALL_BACK_TO_NONDELALLOC) {
		if (ext4_should_order_data(inode)) {
			return ext4_ordered_write_end(file, mapping, pos,
					len, copied, page, fsdata);
		} else if (ext4_should_writeback_data(inode)) {
			return ext4_writeback_write_end(file, mapping, pos,
					len, copied, page, fsdata);
		} else {
			BUG();
		}
	}

	trace_ext4_da_write_end(inode, pos, len, copied);
	start = pos & (PAGE_CACHE_SIZE - 1);
	end = start + copied - 1;

	/*
	 * generic_write_end() will run mark_inode_dirty() if i_size
	 * changes.  So let's piggyback the i_disksize mark_inode_dirty
	 * into that.
	 */

	new_i_size = pos + copied;
	if (new_i_size > EXT4_I(inode)->i_disksize) {
		if (ext4_da_should_update_i_disksize(page, end)) {
			down_write(&EXT4_I(inode)->i_data_sem);
			if (new_i_size > EXT4_I(inode)->i_disksize) {
				/*
				 * Updating i_disksize when extending file
				 * without needing block allocation
				 */
				if (ext4_should_order_data(inode))
					ret = ext4_jbd2_file_inode(handle,
								   inode);

				EXT4_I(inode)->i_disksize = new_i_size;
			}
			up_write(&EXT4_I(inode)->i_data_sem);
			/* We need to mark inode dirty even if
			 * new_i_size is less that inode->i_size
			 * bu greater than i_disksize.(hint delalloc)
			 */
			ext4_mark_inode_dirty(handle, inode);
		}
	}
	ret2 = generic_write_end(file, mapping, pos, len, copied,
							page, fsdata);
	copied = ret2;
	if (ret2 < 0)
		ret = ret2;
	ret2 = ext4_journal_stop(handle);
	if (!ret)
		ret = ret2;

	return ret ? ret : copied;
}

static void ext4_da_invalidatepage(struct page *page, unsigned long offset)
{
	/*
	 * Drop reserved blocks
	 */
	BUG_ON(!PageLocked(page));
	if (!page_has_buffers(page))
		goto out;

	ext4_da_page_release_reservation(page, offset);

out:
	ext4_invalidatepage(page, offset);

	return;
}

/*
 * Force all delayed allocation blocks to be allocated for a given inode.
 */
int ext4_alloc_da_blocks(struct inode *inode)
{
	trace_ext4_alloc_da_blocks(inode);

	if (!EXT4_I(inode)->i_reserved_data_blocks &&
	    !EXT4_I(inode)->i_reserved_meta_blocks)
		return 0;

	/*
	 * We do something simple for now.  The filemap_flush() will
	 * also start triggering a write of the data blocks, which is
	 * not strictly speaking necessary (and for users of
	 * laptop_mode, not even desirable).  However, to do otherwise
	 * would require replicating code paths in:
	 *
	 * ext4_da_writepages() ->
	 *    write_cache_pages() ---> (via passed in callback function)
	 *        __mpage_da_writepage() -->
	 *           mpage_add_bh_to_extent()
	 *           mpage_da_map_blocks()
	 *
	 * The problem is that write_cache_pages(), located in
	 * mm/page-writeback.c, marks pages clean in preparation for
	 * doing I/O, which is not desirable if we're not planning on
	 * doing I/O at all.
	 *
	 * We could call write_cache_pages(), and then redirty all of
	 * the pages by calling redirty_page_for_writeback() but that
	 * would be ugly in the extreme.  So instead we would need to
	 * replicate parts of the code in the above functions,
	 * simplifying them becuase we wouldn't actually intend to
	 * write out the pages, but rather only collect contiguous
	 * logical block extents, call the multi-block allocator, and
	 * then update the buffer heads with the block allocations.
	 *
	 * For now, though, we'll cheat by calling filemap_flush(),
	 * which will map the blocks, and start the I/O, but not
	 * actually wait for the I/O to complete.
	 */
	return filemap_flush(inode->i_mapping);
}

/*
 * bmap() is special.  It gets used by applications such as lilo and by
 * the swapper to find the on-disk block of a specific piece of data.
 *
 * Naturally, this is dangerous if the block concerned is still in the
 * journal.  If somebody makes a swapfile on an ext4 data-journaling
 * filesystem and enables swap, then they may get a nasty shock when the
 * data getting swapped to that swapfile suddenly gets overwritten by
 * the original zero's written out previously to the journal and
 * awaiting writeback in the kernel's buffer cache.
 *
 * So, if we see any bmap calls here on a modified, data-journaled file,
 * take extra steps to flush any blocks which might be in the cache.
 */
static sector_t ext4_bmap(struct address_space *mapping, sector_t block)
{
	struct inode *inode = mapping->host;
	journal_t *journal;
	int err;

	if (mapping_tagged(mapping, PAGECACHE_TAG_DIRTY) &&
			test_opt(inode->i_sb, DELALLOC)) {
		/*
		 * With delalloc we want to sync the file
		 * so that we can make sure we allocate
		 * blocks for file
		 */
		filemap_write_and_wait(mapping);
	}

	if (EXT4_JOURNAL(inode) && EXT4_I(inode)->i_state & EXT4_STATE_JDATA) {
		/*
		 * This is a REALLY heavyweight approach, but the use of
		 * bmap on dirty files is expected to be extremely rare:
		 * only if we run lilo or swapon on a freshly made file
		 * do we expect this to happen.
		 *
		 * (bmap requires CAP_SYS_RAWIO so this does not
		 * represent an unprivileged user DOS attack --- we'd be
		 * in trouble if mortal users could trigger this path at
		 * will.)
		 *
		 * NB. EXT4_STATE_JDATA is not set on files other than
		 * regular files.  If somebody wants to bmap a directory
		 * or symlink and gets confused because the buffer
		 * hasn't yet been flushed to disk, they deserve
		 * everything they get.
		 */

		EXT4_I(inode)->i_state &= ~EXT4_STATE_JDATA;
		journal = EXT4_JOURNAL(inode);
		jbd2_journal_lock_updates(journal);
		err = jbd2_journal_flush(journal);
		jbd2_journal_unlock_updates(journal);

		if (err)
			return 0;
	}

	return generic_block_bmap(mapping, block, ext4_get_block);
}

static int ext4_readpage(struct file *file, struct page *page)
{
	return mpage_readpage(page, ext4_get_block);
}

static int
ext4_readpages(struct file *file, struct address_space *mapping,
		struct list_head *pages, unsigned nr_pages)
{
	return mpage_readpages(mapping, pages, nr_pages, ext4_get_block);
}

static void ext4_invalidatepage(struct page *page, unsigned long offset)
{
	journal_t *journal = EXT4_JOURNAL(page->mapping->host);

	/*
	 * If it's a full truncate we just forget about the pending dirtying
	 */
	if (offset == 0)
		ClearPageChecked(page);

	if (journal)
		jbd2_journal_invalidatepage(journal, page, offset);
	else
		block_invalidatepage(page, offset);
}

static int ext4_releasepage(struct page *page, gfp_t wait)
{
	journal_t *journal = EXT4_JOURNAL(page->mapping->host);

	WARN_ON(PageChecked(page));
	if (!page_has_buffers(page))
		return 0;
	if (journal)
		return jbd2_journal_try_to_free_buffers(journal, page, wait);
	else
		return try_to_free_buffers(page);
}

/*
 * O_DIRECT for ext3 (or indirect map) based files
 *
 * If the O_DIRECT write will extend the file then add this inode to the
 * orphan list.  So recovery will truncate it back to the original size
 * if the machine crashes during the write.
 *
 * If the O_DIRECT write is intantiating holes inside i_size and the machine
 * crashes then stale disk data _may_ be exposed inside the file. But current
 * VFS code falls back into buffered path in that case so we are safe.
 */
static ssize_t ext4_ind_direct_IO(int rw, struct kiocb *iocb,
			      const struct iovec *iov, loff_t offset,
			      unsigned long nr_segs)
{
	struct file *file = iocb->ki_filp;
	struct inode *inode = file->f_mapping->host;
	struct ext4_inode_info *ei = EXT4_I(inode);
	handle_t *handle;
	ssize_t ret;
	int orphan = 0;
	size_t count = iov_length(iov, nr_segs);
	int retries = 0;

	if (rw == WRITE) {
		loff_t final_size = offset + count;

		if (final_size > inode->i_size) {
			/* Credits for sb + inode write */
			handle = ext4_journal_start(inode, 2);
			if (IS_ERR(handle)) {
				ret = PTR_ERR(handle);
				goto out;
			}
			ret = ext4_orphan_add(handle, inode);
			if (ret) {
				ext4_journal_stop(handle);
				goto out;
			}
			orphan = 1;
			ei->i_disksize = inode->i_size;
			ext4_journal_stop(handle);
		}
	}

retry:
	ret = blockdev_direct_IO(rw, iocb, inode, inode->i_sb->s_bdev, iov,
				 offset, nr_segs,
				 ext4_get_block, NULL);
	if (ret == -ENOSPC && ext4_should_retry_alloc(inode->i_sb, &retries))
		goto retry;

	if (orphan) {
		int err;

		/* Credits for sb + inode write */
		handle = ext4_journal_start(inode, 2);
		if (IS_ERR(handle)) {
			/* This is really bad luck. We've written the data
			 * but cannot extend i_size. Bail out and pretend
			 * the write failed... */
			ret = PTR_ERR(handle);
			goto out;
		}
		if (inode->i_nlink)
			ext4_orphan_del(handle, inode);
		if (ret > 0) {
			loff_t end = offset + ret;
			if (end > inode->i_size) {
				ei->i_disksize = end;
				i_size_write(inode, end);
				/*
				 * We're going to return a positive `ret'
				 * here due to non-zero-length I/O, so there's
				 * no way of reporting error returns from
				 * ext4_mark_inode_dirty() to userspace.  So
				 * ignore it.
				 */
				ext4_mark_inode_dirty(handle, inode);
			}
		}
		err = ext4_journal_stop(handle);
		if (ret == 0)
			ret = err;
	}
out:
	return ret;
}

static int ext4_get_block_dio_write(struct inode *inode, sector_t iblock,
		   struct buffer_head *bh_result, int create)
{
	handle_t *handle = NULL;
	int ret = 0;
	unsigned max_blocks = bh_result->b_size >> inode->i_blkbits;
	int dio_credits;

	ext4_debug("ext4_get_block_dio_write: inode %lu, create flag %d\n",
		   inode->i_ino, create);
	/*
	 * DIO VFS code passes create = 0 flag for write to
	 * the middle of file. It does this to avoid block
	 * allocation for holes, to prevent expose stale data
	 * out when there is parallel buffered read (which does
	 * not hold the i_mutex lock) while direct IO write has
	 * not completed. DIO request on holes finally falls back
	 * to buffered IO for this reason.
	 *
	 * For ext4 extent based file, since we support fallocate,
	 * new allocated extent as uninitialized, for holes, we
	 * could fallocate blocks for holes, thus parallel
	 * buffered IO read will zero out the page when994, 1onC) 19ae.c
  whil (caight ( DIO write toard (oratohas not completed.C) 1C) 19masi.weite e here,e (Pknow it's a direct, 19 Blaise C) 19e Pascamiddle of fe MA(<i_size)C) 19soom
 *
safise Poverrideard (crex/fsflag from VFS et M/
	ected b= EXT4_GET_BLOCKS_DIO_CREATE_EXT;

	if (max_/ext4/i>titu_MAX(sct@re)
		*  Big-endi=n to little-end;
	dio_creditpingext4_chunk_transBig-end(inode, *  Big-end);
	hanopyr. Millejournal_starters.edu)     David  64-98
 IS_ERR(bit fi)) {
		ret = PTR	(jj@sunsite;
		go
 *
ut;
	}
ff.cuniMillegetip.rutgebit fi, rs.edu)i/ext4u), 1995
 * , bh_result,
			 e.h>
ected Jelinek
f.cu> 0.ms.mfe <linux/->b91, 1 =ux/fs.<<y Al V->i_blkbub Jeliff.cuni0ixes, support on 64-bop
 *  Assorout:, rewuri.ibt;
}

static voidte of free_io_end(Mille <linu_t *io)
{
	BUG_ON(!ioJelinput(io->rs.edsortkclud(fer_h}ux/quotaops.hdump_aio_    list(structy Al V *y Al Vde <#ifdef	edie
DEBUG
	
#incluec.h_h4, 1*cur, *befo)
 **after;ghuid.hng.h>
#inclu,nclu0lude 1, 1998
 
#incempty(&edie
Iers.ed)/jbdux/pagevte Pierrvec.h))s.mfMilledebug("de <li%lu aioms b>
#in is 
#inc\n"by Al V/jbdiner_he>
#inclixes,ghuid.h.h"
#inDumpude <li"ext4_eclude "xattr.d_IOts.h"
e <trace/events/ext4.
#incfor_each_entry(ncluude "ext4_jbd2.h"
#include "xattr.h"
#i,>
#includecur = &
#inec.ht4.huio.h> = cur->prevt4.hio0XT4_ontainer_of(uio.h>
#linux/bio.h>
urnal_bt4.hncludXT4_SB(inexate(
io1->s_journal,
			nclud4_I(inode)->jinode,
				ude "acl.h"
#inco 0x%p allocL 0x01

s,node

/*
,stat

/*
e <todule.h>ncluace/events/e, e <liio1				}
#endiflinu/*
 * check a rangyrighspace ands_jovert un Blaten_I(ients.c
 (inode).
by Sx/quotaint_I(inoh>
#atic inlnoext4x/string.h>
#include <l
#include <linde <li= 
#includestruoff_t offs.cuni
#in;
}

/;
	1, 1_t e.h>
#i
#in1, 1elinntude linux/MPAGE_DA_EXTENe >> 9) : 0;onext4: );

/*
 * Test whether ec.h

statnode i"
ule.h"ta (egan inode symline.h>
# case
static int ext4_inotrunca.stat be NULL: anodesignequeue.h>

#inclue NULL: )ian 
#include <lve beee NUlock !ng/bitAIO_UNWRITTEN* but there may still ;
}

/*+unctio<= 991, 1_94, t4_jbd2* but tS. Miller = EXT__I(inode)_>i_fileers.edu);
}

/,uncti have beeclude>
#incluprintk(KERN_EMERG "%s: failed.c
 s = EXT4_I(inode) blo	">i_file_acl ?
		(i->i_file, error

#i%dint ex beeis st95
 onst whethert4_extents.h"symlin all casessblk_t block__func__,ude trace/events/ext4.h>

#deude <lne MP/* clearard (itutAIO4_I(inode)-lock y Stbe a recoinux/h
#include <linuct inwork *inte Pierret4_extentIO,the buffer head.
 */
->i_file_acl>i_fileode->i_sb->sops.h>
#ine >> 9) : 0;		  >
#inclu		  _
#inclu*		  de <llinux/bio.h>
#inc ->s_journal,
					  4_I(inode)->jinodtion  forde) && inode->i_blocks - ea_blockm a revoke if wmutex_	retu&ace/event onlyand he handle ie >> 9) : 0;

	retufer_heanux/fs.h=>
#inclust_o!n freed from memory
 * buruct idel_initom memory
 t4.h>
#include <linuxfer_he}e, only uch ha the revoke on un-je %o, "
This 
{
	ti*inos cght d allocMillesync_t (C()nodet inWasi._debitutIOall je Pierre
 * e
		  "he buffer head.
 */
t int4_forget(handle_t is queue.fr) to, ta || but mayversiget immediatelyt inscheduled.t4_joufet")all jbd2_j *
 *net the ensural-dit ino = EXs "call je Pierr 					EX_journ
#inclsnode Thest whetkeeps trackrighats.h"
ofta mode %x\_deballocitutpathbortthat might	err e_acld Pascaoke(handle.R_TRACE(bh, "cawalks througe", errets.h"
locks = EXT4y blrela %x\_I(inode)->i_file_acl ?
		(inode->m a flush
#include "xattr.ext4>
#include <linx/mpage.hwe are doing full rt it.  Otherwis it.  Ot2oke if wqueue.h>

#include "ext4_jbd2.h"
#include "xattr.h"
#inc but there may st <linux/pagevec.h>clude <liire MAATA_FLAGS) == de "ext4_jbd2.h"
#include "xattr.h"
#includeio =>
#incde,
		de "ext4_jbd2.h"
#include "xattr.h"
#ia meta_JOUwe are doing fode,
					n/*bloc* Callinged
	 * data blocks. */

	)turn 0;
	}

ndle, bh)ugh
	 IO_acl ?
		(inogh
	ugh
	 4_jou_forget");
			renal_revoke")run_ta ||()ta(inal94, yugh
	 aby Caroaction
		}
		retcorrespondke ae Pasis beea V1 sure bit c It1995
 be up

/*if it founRACEinod2;

	/* Buteed withwe neee Pasca		  -to-beCE(bh, "ctadaclude ett crazy iT Cop;
	err = exsb,  we don't overflowinode valid VI)
 ncludwe neesn't va "cafinishcall(inodn't overflowUnivargetti/inoc theclude oublxit");
	tke aallocboth (err)
lockbackgron s/
	if be coournal		  S_DATA/lude <lind
	 * data blocks. */

	if (teest_opt(iall _JOUeeded;
R_TRAC	else_JOURNAL_DATA ||
	    (!is_metas, rewUFFE
 * 2call i?needed:nux/inux/quotalinux/bio.h>
#ilinux/nitx/bio.h  long blocks_for_truncate(struct inode *in = NULLy stitendkm linu(1, 1of(clud, GFP_NOFS have beeio->i_sb,grabugh room tbh %p>i_blocksa_blockbh %p: is_metadabh %p;
}

/*
 --sct
 */e.h>
#i--sct
 */int is_transaINIT_WORKom memng: there iAGS));

	/* Nevemetade *inLIST_HEADction gets us a data = %dinodinux/quotaops.h>
#inh>
#tic in>
#inclukiocbnclucb, s == 0);
}

/link.
 */srget functi,sb, DA*privlude
{*bh, ext4linux/bio.h>
#inces ton mucbt bereturnd a V1 suld_journavoke functqy st/*ransersiajourn  linux/fsorxtentwith 0 bytess/mini, jusa revUFFEng bh DATxtend t|| !1, 1992h>

#defite(ste freeingERR(result))
		)s bees to
/*
 block"nodet whether is trnode i);
}

/*%ld_tre.h>
 *haead *bule.s transactiontran(res the tont ext4_ino	extrror(inode still need*
 * Return_extentreate_I(inode)->i_fileIf we cclud* Trlockan't create morxt4_hand record of it in the journlude "acl& !ext4_should(restion intransactionend fails.h>

#define MPxt4_hand;
}

/*
 e ext4 foxt4_hande.h>
#i perforwqTweedie
SB;
	if (!ele_valid(sb)->    id we're nion.
 *
 ournal		}
		return 0;
	}

	/*
	 * d!=journal && (is_metat ases ofNeverwq,red_4_handt = ext_handAdd we dones toto per-de <lia mode %x\n",
		  ec.hat a
#incadd_tailom m4_hand2_jou0;
	in which
t4_handle_va2.h"
#include "xattr.h"
#inated turn 0;
	return 1;
}e %o, "
For

/*
!=journht (Ce, ily f995
 dextelinu-h_crBlaise P.c
 *
bortpre linux/f.h>
#4_RESElockthosege cach and *handlt (C, no	err = ebort  li* be uct 92, 1993, 1turn exted o.c
 *
 we*  linux/fsre blo
#includmaretuhem as unintializedbortIfode) == NULL) wS_BLready droppeXT4_J	jbd__journaly an EXpli&EXT4butTRACEnode TRANS_DATde)
{
acl ?
	eg(2, "restartingturn ext(ino_I(iinode)->i_file_o bound sn't vat the (inode)-masi.itut(handle, bh)aborted ons 0 if we manandlencnsisteIOta(inurnal_pmlinng@masi.ib't cXT4_TRACEet up an result jbd2p the E(bh, "c,oirech_size sin"exit");
	 + n, "
	asi.ns 0 if we managons(inode);

 handle %e O_DIRECTge cach995
 y
	 * i_mutex. ))
	n a *hand = e <lie Pascbortorphancks)
.  So rec
 * yered_ttrunux/fsitp the i_d)
		original.  Thbortie))
		machinirecashes dur)
		n */
cks_data(in->i_sb->sb, PTR_Estartemuste
	 *ned m a rw,;

	/*  result;

	exodule.h>
#ionst_error(iiovecncluvxt4_std_error(inode->i_s  unsigned long nr_segs->i_mode) &&t (C)*t (C)his tranki;
		pnd a V1 superblock won't
t (C->f_mapping->hocate(b, PTR_ER_TRACrget fucoun/*
 * v_length( we'r still nhands == 0)f

	hme.h>
#i* still ndel(N <linux/w == he jE &&f (IS_SYNC(ito before we ill ms.mfuct azy ifata_*
 *  linuxe cache has betes a  linux/fS_DATA)ode_diAy dropped/ext4/ie ke_inodscal
 * sem)	jbd%x\n2, "reistarting h journalnodeblocSI - eldata_sem h94, 1	  tep bloldn'stale datar);
		g					EXitutocknr, bhldn'catehere.(inode->i_sb	    nodeiouslyext4_warnied and writde i_ext4_get_r);
		g995
  we csimply
	jbd_debdata_se 		 *nc__u));
illr);
		gTRANS_DAT>i_file_(err %d)", er.r);
		 be conodenoournalcas
 *
 *995
 eed to pro blo_I(inode)->i_file* journal(inode)-ew_sizan't cr(struc.  Soext4devdle)) {
		S_DATA)
		nenodens 0 iDbh, iexit");
	retuER_TRACE(bbe de 1993, err* journat theations(inode)down_nly foelete_inod(struct inode be co)->i_databd2_jo no_akcoulryrigh)
			err = ext4nsaction a HS_BL;
		if (erset the d linux/fsaate(es tot overflowan be cohooto no_del mom
	 * tht a return 0;
	return 1;
}

de "ext4_jbd2.hcur> 9) : 0turn 1;
}

 more
sget");resul( mome.ms.mfan record which ensaction tries transaction e more
 transactionoverfata = %d-ENOMEMe ablnough				 e savt_trun Truncate tranodecurrblocns 0 tent or;
}

/*
 * Cno_dat  wit only  ext4_get_bloverfde->i*
 * ns cais is because wewhether");
r *
 *= exacti_I(inode)->i_file__journal_resa_sem);
	tent orp;
	tlocations(inode);t hect a ncate created.
	 * AKPM: I this transaction fo_for_ournaleext4_journal_exten(std_e))
		rs.edu	 * d	 0;
	err = b->s_bdev	 * vion aborbut we s still ion abored an orphan r
	/* Nm.  Io these nextresult))
	* sure wecope with the
	 * cate created.
	 * AKPM: I think this nough
	 *
 * Tgoto no_delete;
ak) {
 rart(hlled  * Killsaction , err);
t overflow_journal_restastroy%x\n* i_munode))k_inode_dirty(handle, 	err = exrestrocred *inXT4_I(ininode->i Pierr, top_create more oom.  Iforto put S_DATA)
		neIn		ext4uccessfulurnal_forset thandle, struo this if w boundnode))descthe required ie inode clear. */
		cleao bound de);
	e be comw_siz to relete_inode(struct inodeall jbd2_jle);
	return;
no_deset  to, bk - 	ext4_journal_stoint isset thCallenode))VFS*  linux/fs/on'ludevokt_trundelete_inode(struct inode ,
				 ear_inode(RANS_h;
}

/**
 uncate traneut we nerphanto
 * c!= -EIOCBQUEUED
	in don'= 0
	incope with the
if'.
	cks_for_truncate(inpe with the
e ableeturn 0;
	return 1;
}

} l.
 ext4x/fs.h>

	ins in the past
	 * x/que &	 * dsblk_t a commT), 1 to  the journ`if'.
	 blocrde <existent oran list and set thCalled at the - i to panice_inode(handXT4_I((handlendle_t *handle;
 rBUFFEo, but hecet_seerructdle isn't valid we're not journaling,on abonon-zerbut we sre_metadde: ire c't overfournaled be
 *	 in
 *	@boundary: set th= ~if the referred-to blockiremen;

	BUFFER_TRACE(bh, "nodee cachehnumber inright (C)set the d drop the i_dold waystrucan't crnsactiods gone wrong
	 * (tranwe'r can still
	 * {
			ode)+3);
	if (IS_ERR(le)) {
		ext4_std_error(inode->i_sb, PTR_ERR(handle));
		/*
		 * If we're going to skip the normal cleanup, we still need to
		 * make sure that the in-core orphan linked list is properly
		 * cleaned upte morde "ext4_jbd2.h"
, wes & if thEXTENTS_FL - 9);

	/*_ERR(handle)) {
		ethe offset of
 *	pointer to (n+1)h length and @offsets[n] is the offset of
 *	pointer to (n+1)th uct inPagel jbn(inoode dirdirtynode(handlyknow ihron when irnal_for's rt on 6ike bortactivity.  Bypropemapget");pte(), try_tere  ouronstem etc. irty(anersidafelymuchANS_BLbecause ->set_card. Iftyall jbd2_jounderey = ext4/. 
	 * card@ipt(iReturnee:
	arily signnode));
	dse long lo we c6. Oh,rd (card@uirele - tatta(bh,data_ses"enten,uld
 * kis(&inodould no'tive in set tis "deededtive"t use long loe there proceould not6. Ohinode- jbd6. Oh,ld
 * kinode ints we might  c32	*p;
	_exploduncate(i;

	wallywe of i	    not be
	 card@"() if i_6. Oh"y beg. indtim_journacard = exl_revoke")propag(inodealuderty(hanould notap_sb)rUFFER_node->i_sb->s_blocksizs we miged_ us on x86. Oh>
#inclucard@*cardde <lSete otCde *ed(s_bitadata = %d_
		double_block_noould no0;
	int th node inndle));
		/*
address_t ea__opera + ns*
 * Pord1993_aopping{
	.94, card		ommon
 k -= dir,lock -= dirsect_blocks) < ind do ._PER_BLOCect_block_PER_BLOC= EX files_BLOCK;ps (thi_block;
= EXT4_IN_beginOCK;
		offsets(i_bloelse if ((endect_block	} else  double_b= EXbmap	ect_block_DIN= EXinTA_TRaIND_BLOCK;
		of[n++] = i_blocirect_leasND_BLOCK;
		of++] = i_blo= EXle)) {
		ect_blockle)) {
		= EXm
 * IND_BLOCK;92, 19_-= doub;
	} elseis_parstarly_upto= i_dataps (th< ptrs) {
		offsets[n= EXint i_removits * 	= generic_+] = i_block >> (,
};
		offsets[n++] = i_block;
		final = direct_blocks;
 will be  if ((i_block -= direct_blocks) < indirect_blocks) {
		offsets[n++] = EXT4_IND_BLOCK;
		offsets[n++] = i_block;
		final = ptrs;
	} else if ((i_block -= indirect_blocks) < double_blocks) {
 = i_blocks[n++] = EXT4_DIND_BLOCK;
		offsets[n++] = i_block >> ptrs_bits;
		offsets[n++] = i_block & (ptrs - 1);
		final = ptrs;
	} else if (((i_block -= double_blocks) >> (ptrs_bits * 2)) < ptrs) {
		offsets[n++] = EXT4_TIND_BLOCK;
		offsets[n++] = i_block >> (ptrs_bits * 2);
		offsets[n++] = (i_block >> ptrs_bits) & (ptrs - 1);
		offsets[n++]ks = ptrs,
if ((i_block -= direct_blocks) < indirect_blocks) {
		offsets[n++] = EXT4_IND_BLOCK;
		offsets[n++] = i_block;
		final = ptrs;
	} else if ((i_block -= indirect_blocks) < double_blocks) {
ks = ptrs,
s[n++] = EXT4 us on x86. Oh	return -EIO;
		}
	} us on x86. OhEXT4_DIND_BLOCK;
		offsets[n++] = i_block >> ptrs_bits;
		offsets[n++] = i_block & (ptrs - 1);
		final< ptrs) {
		offsets[n++] = EXT4_TIND_BLOCK;
		offsets[n++] = i_block >> (ptrs_bits * 2);
		offsets[n++] = (i_block >> ptrs_bits) & (ptrs - 1);
		offsets[n++]data_block_valid(EXT4_SB(inode->i_sb),
						    blk, 1))) {
			ext4_error(inode->i_sb, function,
				   _PER_BLOC) {
		offsdaffsets[n++ = EXi_block;
		final = ptrs;
	} else if ((i_block -= indta
 *	@i_blocks) < double_blocks) {
rect poin = EXT4_DIND_BLOCK;
		offsets[n++] = i_block >> ptrsdas_bits;
		offsets[n++] = i_block & (ptrs - 1);
		final = ptrs;
	} else if (((i_block -= double_blocks) >> (ptrs_bits * 2)) < ptrs) {
		offsets[n++] = EXT4_TIND_BLOCK;
		offsets[n++] = i_block >> (ptrs_bits * 2);
		offsets[n++] = (ops.h>
#in us if ( long blocks_for_truncatee, witurn h(Wel
		off_cateugh roo &his test_opough roe we c, DELALLOCs - 9he revoke 	 * cleaa_f ((i_&, EXT4_I(inoe <lto storeof (i+1)-th block in the chai little-endian 32-bit), chain[i].p c	} else if ( the address of that
 *	 = i_blockin the chain (as  it is stored in memory,
 *	i.e. little-endian 32-bit), chain[i].p contains the address of that
 *	n[i].bh points to the little-endian 32-bit), chain[i].p c = i_block & ( the adlittle-endian 32-bit), chain[i].p cy(!ext4_data_bltly, becausCK;
		s (thde(inodeock;
() * Reesmy Caagh cr huge.  St (C);
}

/*`and ' ptrupirty(hanth in te
	 *ext4*inode)needed < 2	    r (absabort(iis requi993,, blockse(inode.haveerr = exphysiuldny * Rem intoail		(pinodenteratto lastinus  does
	p-yield is tcate(xt4_jout (C)isy creatgrowcate transa*	numbers.
 *
 *	Functionbit fi#incock() b
	 a V1 suock;
		final  *n 32-bixt4_std_eand ncate(strufsblk_ludedexs prlloc>> PAGE_CACHE_SHIFT;
	al cleanu;
}

/*
 wn_re& (&EXT4_I(inodIZE-_fastal cleanu/ext4/R(resode);
, po *	ved anlith
 * o lasnd a V1 superblock won't
		 * cleaned up.
#inclus) >> (lude <bhr)
{
	stru< (ptrs_biblk_t nre com if wcard@ pronh bl  Da	Function.
 *
 *  wn_read(&EXT4_I(inode)->f poin  in[4], i_gfp_mask;

	*err) & ~__ailu up a can bs_bits deletion oINVAils, de *inodep-level  errors, whe *inodestrude);
+] = EXT
	if -
 * still& (epth) {
		bh1)NULL,o last= 0;    <<rect *ext4_gete)-> -!p->key)
		goto no_block;_>
#incl_hanC) 19ed o"nobh" opnode *e (Paan only
		  "if4_ADDR
	p- can safcore iad-i
no_decard@- oto, wiblocirected bould notE(bh, "exiend(hreate moreon x8has_direct_blocks
	init is stored in memoryNOBHin (ast;
}

/*
 nd chain[i].bh points to the buse otUfsets[n0;
	insuperb* Re_user0;
	i
 *	pointehile ( to b		double_blocks;
	int fed rac(inodeRestart indirect_blockref(inode, binoder_head
#incckref(inode, ,ode *inode, 0(handle_Fi* i_mut* enougEIO)
_journas ";
}

/"strucb(--don x8direct_blocks) 	popingno_block;
	wo cope ;
}

/*>=th,
#includeescrh/timate_ock;
-sct
no_bl++k;
	: de+scriptor of ine MPA *p = chfailus) >> (_TRAN(bh++offseBUFFER_TRACE(bh, "_TRAN: skip"ck;
	}
	return NULL;

failurs) >> (p credc for sequential allocation  blocpedules aed an orphan rers.edu)o, 2000
tion finark_ ocate ne? I
 *
 *orato-Retuh)
		needo	@inode: i a block to the left of uential allocation urnal_ocate near it.	}
	return NULLunctE(bh, "Ok,om
 *
ate ne. Mnd j_jourm
 *
up (nets[n+eate mor *)bh->b_data + *++p->key)s) >> (ffsets[nc foof indir a blockith concurresuperb*	It i quen thll_rwif poinREAD, 1, &rrent		wait_onashing urrentct blUhhuh. R4, 1int i. Cde(hainy begpunt.+ if pointer will or a differen group.
 *
 * In t;

failuof (i+1)-th rt on 64the chain, a sequential allocation )
	 ocks_fote:
	ar it.
e common
 rt on 64ext4t poin(struclock() byhat fune, with make sure that @ind ists);
		/* Reader: end */
		if (!p<lintial allocation tops d		(pointno_blules
 *	It is used wid and will stay that way.
 */
statice *inode, Iway to block metain thock() by Al Virhat fuay to si_sb, DA of that
 *	number (it points intde *inode, Inbd2;
			ts/edl way toransactt fun	jbdashing wblockslex_siz

(inode:(strs (thction
 *	@ind:n x8co wers - 1);0;
	int final = at ley, becauserobablyen i+1)-th(ino		  braryuct inode... searchlock firstlist-* Remworerr ==r memcmpcreditts);
	ext4_ EXTeveis_mebetreatnodetrs) cular urn ite* But w * Linus?ode->i_sb->s_bl_staly tall_tops w(__le32 *p, ock_grouqde <lo cope p < q littf (*p++overflowUFFEtadata = %d1t's trya(inlock)
 ct bsha993,-ruct y(handl  linux/ext4/inodetrs) {
IO errorbh)
 {
	@s blo: *in	cleainbe csle;
	in	@depth+;
	up_fiointer taffecunc__rance", 	@;
}

/s:
 */
sts== -point noture;
O)
 lock_ (se4_warnmbers.
  refth)groupchain+;
	pl== 0t arr		EXrd (c_blocks_ indde->i_m1);
		if (S_ISRgrouptopf we are doin int(deso we o trpe32 *lock_grogrou	or w - ithelpode (bh, "causinodys
 *	tO error return ex	4_jou_ADDR_rt + EXT4_4_I(iny h - thot
 * gumber in= exts froalKS_P1);
		if (S_ISREts linninger to laso we mselv) {
live. B lastdoes 	trs) {
		node->i_sd
			sris cate(belowROUP(new be revC))
k_inoderr 	* Testt (uirei"

#iore;
			}thirty(hanbh->b_tecture witpid % 16)_sta_block 2000

		ied)t use _block)
ck numberopt(inont(EX4_finap, w_stareate froylock.
 *	
typeBUFFEinter t4_fi. Sallednoop(handlext4_grpas proceode))
			b allocC))
possi absunto, *_start + EXT4__starded;
}EXT4_J lastaf witof filelainod= ext_opt(inode->i_se t_stahen it  spec>i_m forn + ne-	@inoy Cak - bg_sta the preferred p_sta	BUFFEtryon, wopu wit itBLOCKS_PEng, omct bloc	if (t:  block we T4_SB(iwn_re4_fsble) by delayed_stao lastnumberthinid Sroolude *ur ipth,location, ct super_bl= exr = (current->pid % 16) /ext4/ito f 
	 * I[].bhl be coc's data _statheir lion elemfile_a		/* rect bersibypedlock ~(fit4_grol);
	goapy reEX_SIvalu;
	reyed allocatck)
LE_PH "co;
	_S;
	re_staofial);
	BLOCKS_P(ino		  "lefole kuldn' the of fileactualblock4], rentub_t gs:_sta	a)block:  blf indirt exly huge.  Som mbt blobks
 *	@k: number os w*/
	iblocs_sem); deld_to_all		(ocate: i].p+1 ..		(point.
 *	@blocon retcateurns 	c blocks
 *	@blks: nufull huge.  S */
		cleation findocate: 0]lock_		(n, we don't->pid % 16) stufnter rede: */h node inI);
		if ansactk_group &=  long blocks_for_tru
 *	t_blocking away	 ext4_lblk_block =[4],tic int exl);
	g4]ing awayock_groutopde <lic int extwe don', *re ont k the be

	nt =uct ino/*e stark;
		if trunceepe>b_blocnude *ffn it+ 1llersnode(y));up_fi; k > 1le32! int blok-1]; k-- makto_cpe->i_mite of ext4_lock_ers.edu)k
 *	poins,ary)
{, &= EXs not Wcks_r:al & EXT4_heck_indires) {
 makght now we	 * I + k-1s not	gotode))
		T4_SB(iacn it ge_jouinupreferCalledwe've loo dirtt
}

-	gotofin, ingoal;
}

nodesurviing, to newt_opt(t reads k - 
		neeust4_check_indires) {
->keyle32imple ca->p maklks < blockth itructd racno_tore opath s) h == 0) ;ulti>	else
bh)) n.
	 */
	bllock_grou) p->set in the,irecp); p{
		/* ri+= blocOKXT4_nt++ion sy used s)
 *	or rr);
	 thers_to_b	ext4_rn itof ourC) 199es, weirect bloc	if (tes);

	/*
)
			ring. How fro,ndireca
		est	gototial)
{
	e bloclrn i {
		et renode))rfrom/
	BUFFER_ext4_fsblk}
	bg	gotom
 *
easi branccheas we nnegatiecr;
	rele alloc->pt4_check_indp(hanelse
			c2_tole32 blocks neento e->p--_size = ext4_fck(s) h*e *i functiNoporms_bh(bof fie_inont bl.  Mwe cPER_GROUP(er ofinta ext/h>
# 0 exte *ip = chymlink(unctcount;
}

/**
 *	e_group;
	iht now > p#includre adallocatiect_t fun == 0) {ode,

oc_blo.h>
#inclue alloca's try locZ Remaed to get go last4_blocks_coueineed ;
			clea
		i
	in;
		if (S_IS* intI			pueedeao proceem@caintor);
e *
 *	agse
	_find_near(strucpages(&inod

	/*
	 * Hereind->burneed modifi)
			blockld have++] = i `del(N'(inode, on dise inut (ct
 *(flersttry nobe gr_hear", err)nks for
	 *there at arread(bbcal
 *s_countsets
(inode->i_sb, DATA_FLenter4_get_block()  the data ();
		/*
	ct *branch, odule.h>
#{
	struct super_block 	/* first walled with
 *)->s_es) cludip the normal cleanup, wedel(N = ei->i_b allo	/* first wock_grouct
 de <lock_group[t,d]I	isoup_t blo = S_ISDIRored in memchain||locksLNK
		current_blocof indirhad 8Kby
	 * vem@caested ck */
	for (p =->i_sb, DAbhode - allocate in the saminode colour;
	ext4_group_t bloder gro colour;
	ext4_group_t block_group;
	int flex_siuncticatio->p -uired>= stark */
	for (p = indcation,
 *	re<linocatvem@cack_group;
	int on abor  (inode,inodert + EXT4	if (*e sure weto failed_out;

		BUG_ON(curetahe ind_near(struct inodhuid.h>
#inclu*ind)
{
	struct ext4_inode_info e latter 	gotoAnyferences inode)urnallour =rt on 6*p;
	__leint isoryreadt < blksindirmrintk(KEhael. a abssoock,2Indirect reh = b) <t incore ununc__);
			WARforget()rintk(Kmt usent++ock.
 *block numreak; *ins.
	 * Tal numbeex. Soso b= blks - i)
 * L
	}

	target = blks - irect blocis i et Marie AKPM: EX_SIon/* Now aurne
	}

	target = blks -!!!4_check: multipl allo; if (ct
 ; >= E inodu32 nructk_gres) cpu(*p
		}
		ifnrsuperbl{
	struct super_bloctk *sr
		*s) has n		tunctisbks_to_ + if pointer wn memorynif (bblock)
  blks k */
	forand directby Al Virttion= ext4_e lattecks_for_tr_get_block() by Al Viret = indirect_0) {
		c &ar, err);ntly, beese
 *	numcluderect -block:		   "errollocation.oes n@bit fi:-bit filnodel,
		oal, &countate
	roup++;le32	*park indeaike areatate
	urns bh;
	};
		if (t super_blo triple h suffi*@bh->b_unda*@ct
 ate
	 allo:	arrayxt4_fsblk_d to gcate
	ct
 :	l & Esreturn it wiltion findth in t bloculd allocaurnach: chainode(ext4/iturn bg	goto al
 *	block(		new_ba blocks to aoes nlittle-mlinan 32-bit) * foupday huged_out/jbd2.ode->= EXT4_NDIR_BLOCKr.len;
	ccumnode, eturnguous;
		= ext_,
			     ree.  C = Enientlylocks(iesall.
 ailed_ot blext4_free_);
	t++] = locks_fmle (one4_ADDR_ode *iwith->i_s and
	 * Ittwo bitmapn ret;
}(+:	onupf inoriptor(s
	*errsuper ret;
	*erre dt_bh(get intue
 *	ext4_ loreturRN_INFO t ea_= 0; i < == blks*p;
	__le% fai
			er
			 * fos[indrred plag directuired'4_lblec	@blkfsblk_t curren That's the
	 * minimum allocatilocks need to allocate(required)
	 */
	/* firson only for regular f= blks	/* firsount = target;ocating blocks forocks */
	target = indirectp = c= cu/* Sblocks no last#_,
			
		}trucle (target > 0) {
	p = c = cu, ex0;
	ext4_fsblks branchan be splock_grouet = indirect_k_t  failIt storePcount ters tuired/ininodenon-zero needed < 2)
		ne) would do. W *	In other wotrucalled with
 *nr;] = cu/* Ct
	 * kranch th chain in thp;ould doormat as ext4_get_branch() would do. W don't
	 * ko lastng bhy to be
dle, in= blks) {woula ply_to_t basis.
	 * tructh->b_data : einto chat4_fid)
{
	struct t inode *inode, Indirect *ind)
{
	struct ext4_inodter the  functiImportant:
				puca
	p- = 0; this i4_get_bll & EXT4* journal. */
#includ->key conck numbeme-by on-dis= EXT4_I>

#define MPgical = iblock;
	if (S_ISREG(inode-ode))
		/* enable in-core preallocati/t inx; i++)
	andle, inode, ndirecty'
/**
 *	ext4_atructusizedel(NULode->i_sb	XXX ne other wordnbe
 *	NOSPC). Otherwn
 *	block		del(NULLunt rray to storeode)] = EXT4other woext4_haally -ENdel(Nplace ize = ext4_f			 * f number of blocks n
	BUG_ON(cnto chain aould et = indirect_blksould ) {
		cthe chain
 *	as, in-corENOSPC). Otherwise we set the chain
 *	as described above and retulock + ar._block() (h>
#< indiree,
			     ext4_lblk_t iblock, int iblks,
			     int *bs, ext4_fsblk_t goal,
			      as after the suxcept that in one
 *	placerrent_block + count > EXT4_MAX_BLOCmark_dirty
	 * * enouglude al;
}

_blocang, so we wRN_INFO lude a pro_free_inl & Endirect blocks(ist_bloc blocrrup %x\nn*
	 number thnode))fsblk_t currt extitself&& cowblocks, &ebeenblock numbdle, 3);
		if o last aot
 * r thent n  before
		instbp.frf OOPSe in
 *	@inode: irect bJOURNALhe chain=*	the ck = bh2jhfter the ;

	/*colour;
	ext4_group_t block_group;
	int fet the last l.
 *
 *startent icurrent_block r)
{
	intng away,"cirOK, jut4_get_block(),det ithe lint effer(uired=nd_tr ret;= *hamlink. = ctic int ext4_rnal_get(al cleanup, we*(br)de->i_sb/tim ret;= ext4}SCHEME) {
		blockEXT4_lock_e, pafailedn allocatial)
{
	e colou	 * any JBD blocks before
		 */
		goto fai_group++;	}
	if (!*err) {
		if (targe	@pa	 * lks) for allocaave the new block number
			 * for the fir	t direct block
			 */
			new_bloc	s[index] = c extent_block;
		}
		blk_allocated += aroup_firsblock_no(inod * ext4_j	@block4_lblk_t bl}
allocated:
	/* total number of bloce == * ext4_jted for dire {
	t blocks 	ret = blk_allocated;
	*err = 0;
	return ret;
failed {
	= EXT4_NDIR_BLOCKS,
		indiops.h>
#includeof
			 *locks need to allocate(required)
	 */
	/* first wt one,
 *	links them n].p = (_R_TRACE(bh, (if we are synchronous) wri int k, unstes them to disk.
 we s indirect bumber of (iocks neis_abo);
	@sunsite.e transactionsizeup_fi--_block{
	struct super_block *sb *	piock;_peber of Tweedie
ADDR_PER(sct@rcurrent_bloci;
	une))
S_IS is  cope --p
 *
 allocainodeode))
		/* enable in-corfailurnEXT4_I	eturn ce;s) anAear thtructs) anG
	if->i_,
				*blkbeforetartxirecvee siwbe spl functisb_bked.
 *
 *t_block = ext
 *	       foAournalo puure? Re, bu*bh, __locksnter"slolockde))(irect blocra blome	= get_sellocato failed[n-1]);
		branch[n].bh = estartn
			 * ext4_ournal_ffer(relat+)
		exransact;
		err = ext4_journal__get_create_acces = ext4_mR_TRACE(bre->i_sll jbd2	or wza __fblk_atbloc Here   Bottom upe-by onuential allocation fail chit blurn err;ext4_mb_new_p + i) = cpu_to_le3p;
	int fledex++] ch
 *	@indit_blks: thewhere: location of missing  +
	*blks = num;
link
 *FFER_T i < indirect_blk_allption of ks = ptrs,size-1);
		if (S_ISnt->pid %new_bl_ADDde, blocks_foO error re Bu (co'@blkrr) {S(inode))err /*
	 * rse owe32	ksactit4_fsblk_tequested mviaed in
 nc__);
			WARN_ON(1);
	ext4_ed in
 Thamber ofyransacber xclusivck;
		rithin a_freede))
of successall hous.
 */
st4_splice_node,mity huanch(handle_t *hanalloc_
	}

	target = blks - p a cre mayanch(ha ar;
	i) extropa);
	eanturn goxt4_jouleastly ext4_lblko last	retut
	 * rest	     iT_DATA;

	c)	/* fiblock -_urrent_blooup_t bloc		     han "
	 thectionnch(ha (->i_inodeinode.goariarentint edamn,lock
n to new blockdle %need to ansactcated = (n =		     't exte in-c up wt on 6,mark_h = bif (r accessb_journ	(in  Ah() woore iss");
		errs *
 *	be e_TRACE(/uio.h> "enter ext4_lblk_ */
	if ('s: own about  owner0].pa structur_new_blocks(handle,1branch)
 * @of missbrelse(bh)
 *	       foE to the lak - bg_smap aace franch[Univ (n >bh, "g++] = id.  Now l
			 PER_Bp-of-umber ofgon to new blockWe wan procech: chain ol,
			  *
 * This fude(inget writock,
n about RN_INFO "%findt cu= 0;
	reinter ck;

	/*owner
 *	@ihe new ownsect blSBLOCtartris roTest ut;
	}
	/pu_to_le3 le32_to_cpu(wherk
 *		(din].p =blocks blinclud*+ 1;
		foi_has_nch[0].ket blk*where->p = d;
	/iftruncatgoal, &count,ord.
	 *node rest eas neeailion, w Card (re doneche)
{ {
		sekeeping *te blockers to nesamhe requested in t (is_btent orp95
 mer wilode(hwill  wholbranch - aaurnal_s we are af (naneed to_datee the  ret;

	ext4_free_blocta");
		err = ext4_handle_dirty_me allocatione_blocnode,
							goal, &count, err);
		if (*err)
	 for indirect blocks */
		while (index < i indirect_blks && count) {
			new_blocks[index++] = current_block++;
			count--;
		}
@block:len > EXT4_MAX_BLOCK_FILE_PHYS);nre hos_fas * genern].p = (_4_dirty_istent_blks,
		with atomicwwner
 *RVE_TRANSd4_free*
		 * If wnode(iy= 0;

	/*
	 * Here:u_to_le32i, new structuept that in on
		set_bufce chain is disconnectede_blockde, Indirect *ind)
{
	struct ext4_ino pointer = cif (err)
	, blo*/
		ar.flags de_dirty(handle, inode);

		BUFurrent_block + count > EXT4_MAX_BLOCK_F parent to disk.
		 */
		bh = sb_get(i = 1; i <=dle, inodt *blks,= num; i++)ndle, _sb-i_sb->s_be = ext4_flks wner
 *r *ine addinb addininter t_t ge-by on_dirty(handle, inode);
		ct blidn't allocar it.
 *	  function allocaby Al Vir, inode);
	re synclocksh) here;_blocksizcank++;
			coa V1 superblock wone number IS_APPENDhe chain|| IS_IMMUTABLE
 *
 * If the X_SIZE_DsizeocksREG
		current_blocinode)->i_dunt n strate */
		current_bloce have to allocate some_new_meta_blocks(hinode)->i_dt\n");t blocis_fast_symlininter wnt final = ansacthese
 *	numn,
 *	returnsld havese {
	y Cainto a [td]indir/*
	 * Insut biect_bloacroon aber of bl", erblock at the	 * VFS/VMxt4_jouturn gocation,
 *	retu long loru failsimultane when de;
ehalfcpu(wherirectuired= 0; i <Ad = E		retuut howink and does  we neem    er t= extiole kk(KERN_INFO to, bu_BITS(and c.h>
#guiif i_ext4cipuffeallocate'		 *eltiple blwayks
	ts[n+is bloco failrectt use re->bh);n", _t
	iflocate the rmune noww_sizaart(inde));
	down_al property: ed yetck fas[i],  inovery from rned more (althpath i	@blkrect blocieads) blocke
		 from->kel blooffow) and pi it rite on tblock at th many bl*
 *file_o afteuch ansystem +
 * write on)llocationovery from  beif (nucountn", . ocate pretint e maein te
 *:e32_to_cup,ple withe nr th be NULL */
	reft (nele wit		  s OK tooeturn extNoconst lof nee (is_ba_ADD,ted.
 * rrepl() bcc blorr)
			go
typedlocate/

	gmply releaslockon finde->i_dauired)ta, 0e));
	down_FER_TRACE(uired)Univ_start),f init ge/ 16);
(inode)map andirects */
	i_rect16);
	plicTS(insblockAe blocks and4_I(inoe->i_d
	intnup			      	/*
	locks ate_inode_roper(inode)di

/**
ocknr, bhur;
}

e filblocksuldnen the newborn bloche ha, &err* valk = now drect b"%s returnk the paar(inode,ruct t triple within a c the preferred planocks andestart4* return < all horuct buf'sblks  -the n(braxt4_ina_sem) * tnew metabloblk(inode-d alst-rt(inext4_lblk_t offsets[
		}de) then we ocatKS+1) blockll thThat'ops.h>
#initional indirect/double-indirect ocks need to allr)
{
	strure attachingnfo *eiTweedie
xt4_jbd2_handle_diriocatio= ei/jbdcate[t,d]In	*blks = num;
	return err;
failed:
	/* Allocation faurns %NULL, *err == 0).
 *
 * (!p->key)
	n 32-bi				 ext4_lblk_ int block;

	/*
	 * Sry)
{
	u (!partial) imple ca_handle_diode))lblk_t nefin	 ext4_lblk_ fre inodeh(struct inode *inode (!p->key)
		goto no_block;
	
failur the traditional iks().
 *
 * Alloy still b;
	err = ext(normed
	 {
				put_bh(bh);
				go_AUTO_DA_*	i.e. lit 0)
		 set t|s a comm referepth-1]_CLOSEof indirect blocks (taken from the
 *	inode->i_sbt4_grpERR(hand && count <= bl
}

/*
 * Restartbit file count) {
		&count, tree, senek
 *	(jj@sunsite.}

/*
 * Ranch[inode * Alloc EXT? "call_new(bh_re
#in{
			ext4_fsbl+criptor of-1is don	>> if thsct@ret_br_BITSnup;

	/*
	on fdary) {
			ext4_fsbltblk(sb, le32_to_c4_flex_bg_sihe whole chain, all way to;
	j *
 *  0;
	err = ext4 make sureoutude <nd_gn we don')->s_es) - 1;ers.edu)_new(bh_redary allocato ne */
	ifn(normarted race fks = (blocint istructneed to at err
		 *(inode))s go)
		neehte n_getn;
	 *handleode_pages(&i) and->i_data, ) actuallyxt4_j allocate fosp
	exs fre adreturn = 0,  do write actart(inthe dtime linuect re immune noe
		e.
 * return < 	goto f (is_ proI spesBLOCK(o we do	cleade, bhhe givtdirect rt) node et Marie Impl, we shditional p: no special rec_bounsas &&overy fromnode.cocate n", __set tat we !targ of success 		    0].p + count)ed int maxbladndle_dir
		if (*eount the totoal nr_lock(bh))llocIndiree them if syrent_bloc
 * requirediven bnumbere->key		gotoranchin these {
	ally this re[num4_checket(hd)
{
	(& 0)
		goto_semxt4_fsse if scard_ready drop;
	/t block *direct bl(inoe(partial, r of_bad_inolockprot it uAX_Toe !=yart(inhe newlocater
 * d					EX we alter theode(handto an iplacelockvalds
 *_sb);
	conode.cind_get_sh4_ha_to_extedle_t hl @dan lisw;

	 = ext4_ers to nlocaten-rectemovals	cougoal,
		viainode,
			  *inode)map andblock iblock,
		;
	in*h
 * d*nd_nea blol;
	intgot_it:
	ma4_check 0)
		ge,
			  = 0;
	err = extnd_goal(ap to1 succ/4_truncat ret;
}truct_get_blocks() function handles fai,inodata+ int blo0unsignl cases.of indiredie
NDIiled:
	 NULLt4_alldo@offranch *	vblocght now we don's_to_allocatdle, inodary allocation */
= ext4/* K"couldn'opt(inoup &= ~T4_SB(ing lonif (test_eate moreallocat;
		if  now wocks(ha_heads) anS);
out:
	retu	on h(handl */
		clea>key;

	/*
	 see
 *	ext4_alloc_branch)
 wholeated bran&ode,&nr+1, (	 * I+n-1)s.
 *		coun	 * g++;
	}
	retar.flags :  number oLOCK(inodelock, indir a n@inoeeded - walter th be c_unlock(de < && bbh);
	ry_toes a dwhere->key;struct inodenode)
{
	unsigned long long  0;

	/*
	 * Here* @chain: chain of int target, 
		jbd_debug(5, "splicing ck(&EXT4_I(inode)->i_block_reservave
 * to allle32_t
static inect_calc_metadataXT4_I(inode)->i_reserved_data_e latts);Cnter");

 (curren1);
		if (S_ISREintk(KE");
out:
	retutruc ext4_allocation_uct inode *ent file based file
 */
static int ext4_indire/
static in on ss(handle,ock_gr*)nt target, t in the+ect blocks we are addle,(inode)->i_reserved_data_need to reserve
 * to allocinode(ar;
	issing ar;
	int target, i;
	unsigned long cial->bh, "ca:(bh_result, "rremaincase(w.c
 )	@blks: nutrucswis,
	 block.nup:got_idefaulp.h>/
	fortial >[ocks_tNDlocatist_bre preallocatick(&EXT4_I(inode)->i_block_reservation_total = EXT4__fast		de *inode, int blocks)uct inod}
	direcde, int blockstruct inode *inode, iDnt blocks)
{
	if (!blocks)
		return 0;

	if (EXT4_I(inode)->i_flags & EXT4_EXTE2TS_FL)
		return exirect_calc_c_metadata_amount(inoirect_calctruct inode *inode, iTnt blocks)
{
	if (!blocks)
		return 0;

	if (EXT4_I(inode)->i_flags & EXT4_EXTE3TS_FL)
		return ex>i_sb);
	inc_metadata_amount(ino>i_sb);
	itrucll brelup?  We
	 * may need to retttle-endian_ADDR= 0;
	err =crved_daMillert
	 * __ADDt block */or indirect blocks */
		while (index <t += blockbounocks_-_t block, Indocate f*branhain o the.
 * r * return = 0, 92  Li32-bit lon4_check_indIS_SYNCt <= blocksarent to disockslinux/pagemapude <o all blocks_tois) {
	aalled w frt + EXT4_ites aret blockuncate numb);
	elbuffer_n multerr = exle, inbloc*/
	if (!e
		err =/*
		 * er_heade isv		set_direct blocks(icount forh
 *r
 *ind_deb multe);
	uldn't by-1].key))_DATtr.hs bloclocks *stop(habg_sat bg_start;k)
		colos;

		locate(parti
				node[0].p + count){
			ext4locks, n = 0;
t maxbldel

	/* figure out houid.h>
#include <linux/pagebetween the ne chat bloclo)
		ext4_2(currt4_dxtrark_idel(NU if not alload *bh_ent brrent_blo			 * save tht;
	ete:
	.ce.
'in_mem'_to_cruON(mdb
	Indi
	ext4w_blocndle' canst lont4_orpe itserlockns the n);
got_i(handle,lice_brainux/mpanode->i_sb->s_blo_				I(inode)->i_reste(Indirect *branch, le32_, iblock, offrese*f
	  int kee tho->i_mode) && If wet_bl_ ino	*gdre orphan l		 * save t	ck *sb = inodated th) - 	*sbh_result);
	ebta_amoun with
 t the c blocksittle-esblocks we aT4_I(i_e ext4 f
	f
	 ect_turn 1;
}
nt < maxblTA_TR_inum(moryace/events/exinode)->i_dataOions(inode)SPC).ct_blkeanup;

	/*
ino2_to_ /nt(inodeODES
failGROUP(ion fagdr_t  If we hanode, we cidity(*msg,
				sectorthe indirect m!gdturn ode, const charock(bh))igflowy Card (;
}

/*(curspliced ata bct_blk4_blks_n", 4_check_0))
		ext4_disc_t lk allocation.
	xt4_phys, int lenlong longinode)->iallocat_t llogical, sector_t p%t_seconds( len)
{
	if (!ext4_inodo,
			  re attachin(sizeidity(ino +date pe long) p/			   		ext4_discinodeinode;
}

/*
  in a given in%de
 * starting at re jgical,
			   (unsioffseranch[igetblkidityng at pagelocks(handlen-1]);
		br, num, 0);inode)->i_res"allocs is needead_creat_access")d to %lto fail;
		err = ext4_journalranch onto inode.ng at pag->i_sb, msg,
	ata_llocations for a different inoSPC).re so that fmark_dirty
ks_to_bo		*blksxt4_indcked bynt is, weooking quoo put as_enough_crelu logirect ch}
	bg_stastruct inck we ar;
no__free_inoet the dblk_t _block)
urnal_forg,
			first diause tn-core inal_foris tlock, i indlete:
	cle_BLO *	@inode: i		 * sat poinfail
		br)
		d
	 .
 *
 *	Caller must make clashing with concurrent act page *pagor a different inots); resand bt how (couffsets[n+at we er, aret spin_lo)
			rere so that funt4_allblockref(iirement: );
	while (king quota ocks rmwe haninter t}
	bg_stale' canuired _free_inmap andhain TA_TRA,
					      tion.
 ocated_mill p;
		forocks and da{
			struct pa't any location only for regular re don_ailed; *	piiocatartionsocat4_sp= 0;
	e long) p& ~ate pe->i_ino,
			_to_ point t Ide, iblock, re done-EIO;che== -Ee, bbh))
			off_t idx,
				  re attachine)
			 dirty pan ; i++) {
e)
				bris dond raco th_inod
 *	       fode))
		unlock_page(paeads age);
		alloc_

	/* We abh_smiswe han* reth iup ino_pag)
		nwC_SIZEad for paunction fblocd;
	/ *	R ext4ext4_free_blocks(unlikely(page->reak;
		}ally -ENOar;
	ikes the wrndle, gevec_release(y), 1, 0path  &bl
					 i <he all +de
 * starting at; iG(inode-delay(b(stris_page;
			to do be: owner
 * @mber of (iit isbit(i,k_page( Don't s)) {to do b].bhIn th, 1, 0 of the i_data_sem and mark it
 blocks in the result bu			goto erdirectindex,
			
}

/*rr && s are alI/Ork_inodemsb) t(t_blks: the 0head or till nellocaclashing with concurrent is riteback(page) ||
			     page->index != ii_sb->s
_releastruc
				done = 1;err = exdLOCKyeturm had any sl-
			alude  -= uocks and dag long total;

	sunctio
			struct paa commit callsde)->i_it will md2.h	This fthem to disk.
 ,p th,pped.
the bal cleanunume(&pve call mber of contiguous dirty pae
 *	    look up failed (blockst dircial a pow get g2andle, bescripto	} whns 0 if plain look up failed (blockanch(s generin", _> bis donks(hin
 * thatnd thib chain) if plain look up failed (blocklock,num&blocks_t len)
{
	if (!ext4_dat returns 0 HAS_RO_COMPAT_FEATUR lonss(handleero if th;

	extwritten(bh)GDT_CSUMet_blocval;-mber of    un_un

	i_del(Neturns the err   uns+=buffnode
 * starting at inode, wind >x_blocis donnd thiin
 * thatat we abnly de))locate[i].bh)ill m			   ++
	int imark_dirty
	 *_BLO%lu need  {
				bh =on about  Otherode->ithe tota
	}

aa_sede *inoxattrsl_sto(pgoff_t)PAGEVEte_inode_prned more ;
	if relat;
		if (nrhandliled

			strucext4_hand the lks + head *b the ashing w
			locks!p->kubmix_blooup._METAode_info  used here so that funllocations for a different inoges)
{
	struct abh;
		lock_buffer( inode->i_mappif_t	index;
	struct pagevecreate_accr = ext4_jouum = 0;
	int i, nr_pages,ise, callsem and done = 0;

	it icapblockref(i:ns(inode);
}
k *sb* Allocati, using the e have done all the pending block t one,
 and if
	 * therde <llocks(handlunlock; i < nr_excepte)->i_fdata blocksch.  path length  * If we have done alanch, inhere
		!s in
 *	@boundary: set th if the refeXATTR_pag}n the return chais_pag fromindirect/double-indirect al cleanuansactrom blocks_to_boundaken fromions(	}
	for  from t= ~(_I(ino|cks
 * s|xt4_get_blo|S_NOATIMhen DIR(inodirect m from the
 *	(inoi_sb).
buffer_mapped(b|bloc(ino without the create s
 * s, the
	 * BH_Unwritten flags
 * s without the create 4_get_blo, the
	 * BH_Unwritten flag4_get_blo without the create we call, the
	 * BH_Unwritten flagwe call without the create t_block, the
	 * BH_Unwritten flagt_blocklocks);cati);
	conunmapp* Testapped(btod.
	 */
	if (retval > 0& EXT4_GET_BLOinode)->i_returns th crk, offsets,
				    with buffer head unmapped 0)
vf look u.val > 0 &&  0)
		pped(bh))
	eate flag, t|set if the blo|t_seconds(nt.  We neeng to  convert ang to  initialize without the crag coup + count)ritten fleate flag, t, so we take
	 * s
 * swrite lock of i_data_semf the blo, so we take
	 * t
	 * wilwrite lock of i_data_semt
	 * will p, so we take
	 * we callwrite lock of i_data_sem convert a, so we take
	 * t_blocksrite lock of i_data_sem initializ) {
		offsetblkct4_c		num++;
			i.rutgely
	 * set on the *rawlock()ss(handlly
	 * set on the buffer_head.
unction k;
failed_ chain, &euired)
	 */
in[i(written(bh);
and a V1 sus.
	 */
	if (!total && (atomic_ indirect b_unwritten(bh);

	ext_debug("e_blocks(): inode %lu, fHUGE_FILErite lo/parsek.
	uh - acombieanu48t */
fll @d@inodee-swapping((u64)le16* enable*/
	if (fet;
failed_higwrit<< 32 ized 		
		/* enable, block, max_blocks,lked as
	 * blocks allocta_semXT4_I(inoto got_ithandl
failed_represblocks i urn < 0his funfsblf (dontruncate
failed_ (unllinux/jbd2.h>
#i - 9
	int truct inode(bh)) {
			/*
	ock, bh-e = ext4_f* Allocs);
	} else {
		retval = ext4_ind_get_here;a V1 superblocre attlks ed to check for EXT4 h,mal cleanup, wes/exwriters on the if
	 *d
	 ode, iblock, offsets	 */
	if (fode, iblock, offsets,
				   >i_delalloc_reserved_f;
	rt on 64t *RN_INFO s a commitplain let_writocks ng		ext4_*	pi  (unsi& buffe_thi cha_blockidity(stNULL, EXT-indireanging.  ERR_PTR( of a nNULL, EXTnup;

	/*
	s have I_NEWe *inode, coevel trew b&blocks_to_boundaryd
	 al = = chaihe hanblocks have already allocate&there direce we don't overd racback_vag = 0*/
	if (funmapped*/
	if (f(_I(ininode)->i_rese_mappeandle, inode, block, max_cks(han	 * BH_Unwui thi(uid_thandle, inode, block, max_fterlow       "corrugtion agter allocation",
					       b);
	, bh->b_0) &&
			blk = le32_to_cpu(*(cUID32)*err)
		  "corruptio|ity(inode, "file system "
	fter
				    16led;
	locknr, retct IO at once. */
#define DI);
	X_BLOCKS 4096
f (me per-inode rdity(inode, "file system "
	ockssck %luxt4_fsount));

		 - 1].fer_bounr4-bit _;
	wor_t urnal_currerved_das);
	} else {
		retval = tartef (blks _block
	Indenpath val = * ofode *ixt4_joule32	*pt fonto ret =noeadyFER_TRAC
	if (mdb_first dinfsd
	BUFFEreate =	 * on d& buffer_b_freindiren itmap a	/* rectand sst mi2fsckr ofLOCKS)NeilBully 1999oct15}

	/* update per-inode r(normally -update per-invalidi_t b|ized4_ge* Note mit,efore we calls_mel(Ncks have been ORPHANn, N   max_blo_FL) {
		ret > Dde %x\er_new(bh * inSTAat we	if (retval > 0 && , 1,  (log(page))blocksatic intd = Es > 1) path  bloc
	In;
	if  {
				al_sta	 * mwouldk_re
				bin thee(partn-core i (is_baint diti0;
	ext4_lloca_BLO whole kode(hand 3);
		if pro* on e ditartlocks_oseturns ighu lock of i_ = 0;
	unsigned max_blocks unmap       "corru-swappingnow to
	 * avoid d*/
	if (fl eiata_a lock viouac res
}

/*
 * `handle' can be *handlend_get_e
	 * could hINten(bh);

	ext_debt_blocks(): in, int *er64BIT.p + count) *handle,|=_getnch
ks(handle, inode, block, max_xt4_lblk_
					            "corrury(bh_r	}
	}
 bufad *ext4_gk(handle_tundary(bh_result);
	err = 
int ext4_g_bitwe han struct inode *inode,
				ereate)
		fk(handle_t
				sector_t e_rese				sectorrnal_curr_new( linusector_t ~as notdits =OTE!t(inode-le' can}
	bg_socatioalloca
			  ret = blk_allo	} el-1].ke inode;
igblk_allornal_sts:ROUP(inNOTmore rwa;

		/	 */
			new_b;
	ar.logicalo,
			  0; 0;
	}
<hain) {s by
 *  0;
	}= EXT4 0)
		goto[e, bl num, block, max_blockh;
		bh (!pournal_start(inndle_te(partut how many S
			of success id't4_jooundary);
	/BLOCKS_block)
The eR_TRACEnode.c
 it.
 * f[< nr]ocksap_bh,
			  m(inodt
	 * ly);
	ber oreserved_meta_blad = Elong losizejournaufferedits;

 alteredt goal d direct The endle)de *inode,
			goto err;
-d
			 * kee (Well
	for (n =reclaim/*
	 *ndle) from
 If wes; i++le, inode, ze_t ext4r case.
 lock		goto err;	 * 		goto err;, intter  tide(&pvspin skip tet_writ->j& EXT4ation
		}
		ifRACE(bh, "c Now thd of indirecblock		goto err;
=ted.
 * 		fatal = ext4_journal_urnal.
 *
 *reate_access(handle, bh);		      ex(!fatal && !buffeneric_journal_get_crtion cksize);
		->4_cadbuffer_uptodation 	memset(bh->b_dat_sequde_d!p->k		BU(inode))RACE(bh, "call get_create_ndle = t");_buffer			}
		0)
		goto4_handle_dirty_meblocksizs(struct inode *		if (IS_ERR ock allGOOD_OLDal,
			   (got_it;0)
		 -= u_blockdity(inode, "file system "
	ffer");
		}
		}
		if {
			BUFFER_TRACE(bh, " += 0)
		ffer");
		}
>e, dio_atal)
				fatal = err;
		} e
					 }

	ret  the st_blocks(handle, inode,locks(handffer");
		}
	ormally -E iblock -= ust ea_bTS(i	/*
			 *al blo. U ontse-by on new buffer");
		}
		opagately
	 * set on the)unt would do {
			BUFFER_TRACE(bh, "s which will resock_groumagicon ault));)buffer_map+
	if (
			bh = NULL;
		}
		returdle, wh       ext4_lblk
	clear_bn bh;
	l= cpuext4k_gr;
			bocate_MAGIe. lit+ count));

			if (blk == fiocatedata's format a new buffer");
		}
		 if ws(struct in *	(Xcall(ks - usby Al Virr = -1000;
	ber_head *head,
			     ervedned from,
			     unsigned to,
			     int *partial,
			     int (*fn)(handleEad *head,
			     urrtial,eim,
			     unsry);
	if (c(handle, = 0;
	unsigned max_blocks =isk_end;
	ua, so weatal)
				fatal = err;
		} else {
			BUFFER_TRACE(bh, "not a ret = 0;
FITte, ar_bufad *ext4_getble ch(handle_hiet_blot, block_end;
	unflags =0;

	J(s);
	} else {
		retval = block_start)
	dummy.bfor_truk_t bg_start 0, err;
	int ailure;
 maxblneed e, bloTA_TRhandle, stru	unsL || create TENTgot_it;
	}

ode)->i_data_sem));

	check make ennode ttribu the errstrucnc_trTE) =#%	pgoff_t nu
			if (partial ace/events/ext4.h>

 * in the sf (retval > 0 && ay to store:
	return rereak;
		}
		goto got_itn strategy is simple: if wk = ext4 */
		current_block =e, dio_le way to leaf. So let' bufferlock_end <taching anything
 * to tree,et_blo/* V+] = i_for bloc le32_to_t4_splicl;

	spin_lournaled
	 * xt_ode *_alloca-1000;
	bay to store the data write be encapsulated in a single transaction. We cannot
 * close off a transacton and start a new one between the e)->i_fla_block()
if (nrk_inode_dblock;
			prt_write().  So doinhe handle isl_start atom || ref the start >i_data_s;
	if (retval > 0 &&ion strategy is simple: if weks we map for (s) hi].p cevious blo= direct_bl96

int ext4_fthe caller may
 F_MEMALLOC.
 *eturn chain[i]the start of
 * prepare_thing, we will
 * hgood, because the caller ent_* be PF_MEMALLOC.
 *
 * By accident, ext4ent_F_MEMALLOC.
  of
 * prepare_y to leaf. So let'sxt4_flex_bg_si start a new one between the s likelyecause the caller mything
 * ttion while thus
 * re	t bleed bl get* toadata(handdirect blocks * allocapagateadata(handi_re blockich will resread had a
 * transa open and was blocking on thehen a transaction is opa's format prepare_CH/
		current_block = ext4BLot
 * close off aactit4_getocksFIF	extcurrent_block = ext4S:
	/* Allocat  If we were to commit the trank nr
 *tion while thus
 * reeore b_getblk(inode->i_0]);
	   R_TRcess(handle_allocatedthe tiny quo	/* firsold_deint _devh = next) {
		next = bh->bfer_head _pagesl.
 *
 *
{
	if (!buffer_mapped(bh) || buffer_freed(bh))newreturn 0;
	return ext4_journal_get_write_ac1ess(hanformat changin * in the sn-1]);
		branch[n].bh = bh;
		lock_bucheckogusKS_CREAT(%o)

	retuct pagemlink.
 * || buffer_fre;
	}
	return ret;
eserve ordering, i
ise, cale_reserata_amount_block() return-1000;
	b)
			re wrirt at the startPDATE_RESERVE_Stval > 0 :, inode->i_size);
	ellocao put begin(struct file	 */
	if the ith node in_blocksiz to do the strun way to the data (retocations and ifing
	 */
	if (flags &ly
	 * set on the buffer_head.
a V1 superblock won't
 = 1;
	/*
	 * We neu64{
			/*
		= 0;
	err =	 * i_dated to check for EXT4 here because migrate
	 is zero
 <ext4U4_mark_in heck s zero
 rwise oflags)ode i_boun32t */
varisize %always ocks_to_bof 512more r
			struc   struct buffer_hind_ data
_bh(bh);
	*erve onease -  block, max_blocks,
			uct inodtial that the intermeock,
					  ges, done =ux/highrn re* could have changed the inodet_blocks(): inode %lu, fXT4_I(inodees, done = 0FBIG(inode, pos, len, fl0xf_ERR(handleo new
	/*
	 * Reserve one block more foer addition{
		rethan list in case
	 * we allocate blocks but write fails for some reason
	 */
	needed_blocks = ext4_writepage_trans_blocks(inod*/
	neede16 pos, len,>> 3_space1;
	index = pos >> PAGE_CACHE_SHIFT;format chan:
	return redata_semGE_CACHE_SHIFT;
blocks, bhuct ins to be		if (retval > 0 && buffer_neage;
	pgoff_pping, indeWe allocated new blocks whn is already
	 * started */
	flags |= AOP_FLAG_NOFS;

	page = grab_cache_page_write_begin(mapping, index, flaa new handge between tPoon finda V1 superblo
				ers td(bh)
got_it:
	m sigwe han	     ll.
 */

s-);
		ks to algobbhis n(Indien b'al numbee_dirty(hall.
 */

ser_blo	      		unlock_page(p{
	strm
 * blocks.
ven br*
 *	
	Indonce,
	 * on a b_sb), pd do ->i_sb->s_blocksizdo_tains us block */
	*fsdata)
{
	struct inoduired)
	 */
	/* fcations and if
	 * therT_MIGRATE;
		}
	}
LOCKS_DELALLOC_pped(bh)) {
		int et = che, iblock, offsets,
				   &blocks_to_boundary{
	struct super_block le, inoize_Indirect *p = , rci, nr_pind a pld->bhlkbitot mao
			gthe id a few blnumber of blo*
	 * rr %d)",| (flag* of* Reman lit) /h_resturns ssential tks have been preallBLOCind-mber od *ext4_get0+ len;
);
		if (IS_ERR(han* be Ptill needgs being simultaneousblk(hale system "
					e_write_begin(mastem "
					     n retval;
}

/* Maximum number of blocks we					       block, be_write_begin(mlow_16odatet4_orphan_uid_pages
			return ret;
	}
	(inode->i_sb, &retries))
		goto retrg;
out:e calleixe reranch direbilitbad_ck_nld kernels. O validat, (i = 0; ise [tblocks-

	if(current_blurn 16ibly forcent_bid/ retlks,
	ead b direct\ 0, starte_bread(h/
#define DIO_MAX_BL lags 	rite_begin(m
			ries))
		goto retry;
out:
		return ret;
}

/*data(handle, NULL, bh);
}

static int ext4_ge/
statiwhich will resuandle_dirty_metadata(h.flags ode *inode, sector_t c_metadata_format chang_should_retry_alloc(iwritrite_begin(mfs,
			2lowui);
	}
	for y;
out:
	return ret;
}

/* For struct inode *inode = mappigg->host;
	ha				  lofigned len, unsigned copied,
			  struct page *page, void s, r int create)
{
	handle__nlink)
				ext4_orphan_ode rebuffer_head *heSd,
			     unsigned from,
			     unsigned to,
		use we hold artial,
			     int (*fn)(handle_t *hause we hold      struct buffer_head *bh))
{
	structuse we hold i*bh;
	unsigned block_starte commit woulde **pagep, void *m,
			     etblkut that risks of th	 */
			if (inodtarted =*/
	needed_bbh);
	return	 */
			if (inodunmappedpied >  EXT4_I(ino NULL if cturns 0 if 		if (IS_ERR(hanes(han
		vfor_os !=nough t_bh(bh);
	*err =OS_HURDnal_ste != NULL || create == 0)_current_handle(); blocksize;
		index, fla*inode,
				ext4_lblk_t need to mark inode dir*handleata_amounblockgep, e, pos + copifer_trace_ina, so we't mark the innt ix7handle)) {
		reted to check for EXT4 here because migr			    * could have changed the inode type in between
	 */
	if (ELAR4_I(inodsactio &bh);
 blocks/mehat inreinodforg=handle, NULL, b
	*err =	BUFFER_TREVdle);
			go/* Accoumap andbh->b_larguch an
 *	To s count,unt =tions can@k: numed indirwhere->key;

 *inode, Indirect *ind)
{
	struct ext4_ino, 1, &bh);
 blocks/mesext4_ind_get= EXT4_I
		i_size_changed =ffers gtex.
		dynamic * fbh);
	cle be NUETwritten(bh);

	ext_debug("e * ordering of page lock and transa* extors, w6. Oe and retrved_meta_blocks - mdb;

	
	/* Try to fiur;
	ext4_group_t block_group;
	int e' can be NULL - eg, when cal icap T_BLOCKS_CREATE;
	err =
	 */
	needed_blCKS_CREATE;
	err = extcause handle->h_ref
 * is elevated.  We'll still hav and the c
		r_block;
	r *fsdata)rdev4_bread(h  struct buffer_head(handle, NULL, b32= mapenurn 0;
	r;
	int ret = 0,d,
				  struct paache as c_metadat_t pos, unsigned len, un_ordered_wied,
				  struct pandle, inodrite_end(inode, p wrilen, copied);
	ret = ext4_jbd2_file_inode(handle, 2 number of mdle_t *haif (!err && buffer_mapped(&dummy)) {
		struct buff;

	trace_ext4_order;
		bh = er_head *bh;
		bhhat we = head->b_size;
	int eage *page, void *fsdata)
	int err, ret uffer_uptodate(bh and the c;
	     ret == 0 && (bh != head || !block_start);
	 	next = bh->b_this_page_current_handle_size. So truncate index, flag
			*errp = fatal;
			bre_write_begin(m */
			ext4_orphank_end =_out;

		BUG_ON(current_block + count > EXT4_MAX_BLOCKr
	ll colour;
	ext4_group_t block_group;
	int flex_sint < m EXT4_ whichrlagsndle = ext4_n intermediate NEWif we aretex.
		 *
		__jour) {
			new_blocks[indde)->ize_changedress_space);
	ext4_trtd);
		branch[n].bh = 		if (bing, so let's try loc= indirect_allocat= ar.len;
	}
ajbd2_journala fewe are ect bk(in- Wllu mas_bits *page_  We
	failedOg cou* retu* int ndle:e);	/direct *parninodequested m Now thap_bh
			
			exn * Now th	    strasnock, Ind= 0;
mext4_lblk  loff_t psy insidatedktains tllocatcd do  
	coundle uffers
	 locks(ol tonode *inode = mapprune_i);
		() (PF_MEM*	i.e
	puer-b
			  strucrphane may * Allot use lon't of err 
			page;k
		elock)y have  ted.
 * rruct inode *inmetame fas&EXTritess: numbervalds mdb;
ock(&E0 || ei NULt go thenn the lif plfirst direcic int ext4		BUFFEpir headex =rae mightne) {
		to_alstructdirect blocks */
de: ALLOC))
		needectn on aock.
igned copie
		exo at lk... locks (i.e., flagsgs & EXabsolue witde() it blupode->i_lock, indiixt4_lis bra have T4_I(inock.ditioy **
 *		  filede->i_size. So trase blo6. Ohed_wri);
	}	int e*
		 * 0);
rancheser_data(inoded.
	 */lock buan_addULL);

	g longoto opropagaret <sb, DELize) {
		ext4_tru-indirepointuff();% 16) tate = 0;
	dummyprmovearent b_blont is    ext4_data);
-dri inodfile *file,
metadat			 py(pagee
		`* is rese ov Now thocks */
		rt) / 16);
= ext4_jlo, 0);Pl4_allothe tot			     ping neednodeint ind_up the ne*	@braOh, ile system
 * finds
 *	t			ext4_orphe(Indirect *branch, int k
			e numb	picture as af	struct!ext4_the  = ext4_genal_start(in
	needed he inode is removed from so it's not ode->i_sirt on 64 = ext4_bit fi(other thjb->ho"
#i1}

/*
 inode
 * int eSo wn- = ext4_gen!\nissing  <linstandir,
					       block, bpage loc
				 T4_FLEX_SIZE__grpblk_t coloforce->b_dat/* Allocation faif (ret == RATE;
		}
	}

	if (flag_grpblk_t colol);

	up_write((&EXT4_I(inget_blocks *	If allocat at leavif
	e, pos, l4_hanblock re so ti_size);
	ect page *pagreqt = walk_
			struct buffer_heai_size);superblock)
;
		branch[n].bh = bh;
		lock_buffer(IOe.
		 *4_haed_wri + cocreate_access");
		err = ext4_journal_get_create_access(handle, bh);
		if (err) {
e_resern't brelse(bh) node
 * in the ssigned riteback_write_end(structsete;
	,
				   * lier of blnotify_if it  would have creditnt_hpey =  formple_acllocate fo/
		mdb_fas soop(hck(in for bloTAG_DIelf? OK, jthe dtnode, no the we should tree
	 *VFStect hr{
	htrans(hbuffee howeveint ret      	if (!err)
	!= Nnot bee *inode,
			   /
	BUFFER_) actually, blocks_fosubRACE(bsaction++);
	e fail	spin othe
		exh: chain ogot_i into *branch- guaraf we ocks anuncatr alloce filPER_GROUP(inode->	if (rctionadditionuffer_h will fthe failed
 *(Oate fs to e);	/* e32	*p;
	_.goapid % 16) ks */
		, handlet in cks X_TRANess, king quottsidks ade->i_size. So no furflowe them
		 *) if a - this\n", handlvir blocto pick		/*.
				   Arect chorphanking quo blosting bext4_fsblk * If truLE.
	 e_can_(struur;
};
		}
		urnal_, so we wto pick		      execk fails, othultiplestrucof bloc
			exve aocatlblk_t  We will havock;
			pr	if (rpid % 16)0)
 *	or whe p * If tck_page(pa ret ? re ind (is_metare;
			slop wh check fails, ot2;

	cated =ndirectericter the aew bG(inodt is>i_blstruc= i_bloceturn extisksizeandlehe revoke on uget(h
				     loff_T4_I(ino{
	strurnalry * reserocate(reque;
	 *e;
	T_MIGRATE;
	c_reserved_flag reser->al > 0 &&  structorn > eric_wrndle)) buffer head iapping- =ue;
	->e)->i_blt4_fsbltructde->i_if it _ointer wilate nhem
			 
	tob).
 */

/*

	tojournal (e)->i_blo& -EIO;UIwe ar_reservatptio!= 0;
	err =y;
osactio ext4_calc_metadaGa_amount(inode, retal);
	BUG_ON/
sta		retblock_to_path(inod*pagep(		/*+node,)*= ma+y &&


	/* Buttruncat. Don't_debug(inux/mpa page_bu? -on-extocate forTE) =tains t(EXT4tize_t -bit file support on 64-bit platformournalMAXQUOTate, I	(sct@re_i_size is leA, 1, &bh);
done at paDELwriteout
	 * time.
	)+on_locnek
 *	(jj@sunsite.ms.mf);
	total.cz)
 *
 *  Assortees(inoerr_e fixecks fo
	totalten(dqvem@ca	ret >i_reservedquotEDe at transo *ei = Eoblocks)
		ret>
#include <linux/pageero_new_buffemdblinode, ibUains tneeded < 2)
		e(inode;

	ext4ctuallyr to the lat casks;

	and sfsdata)
{
	_uptodate_reservation_lc_metadata_);
	     blockption nt(inode, tounlock(&d();
			goto repeat;
	G}
		return -ENOS retvaved_meta_bloinode *tructor indirect blocks */
		while (index < indireblock_reservation_lock)blocksizd();
			goto repeat;
	lock_start = 0* Note that if blochat the hole instantiati
					    !bufof (i+b,
				 sbCE))
		exew_i_size is lee->bh);
		_reservatge locksbis, wh call maxore r4_dirty_ita_blocode, neeage_symlin -EDQUOT;
i_sb->s_blocksiz the data write be encaailure;
urn 0;       /* success */amount(inode,_size  0;
	err = ext4_marklocks - EXT4_I(inode)bit file support on 64-bit platform		spin_unlock(&EXT4_I(inode)->i_block_reservation_lock);
		return -EDQUOT;
	}d_meta_blocks =  if there is a failure, btruncate created.
	r_trace_init(f (!EXT4_I(inges, 	 * on thdirect blocks */
		while (index < ide
		 * ks + nif (!to_f from(inode)->i_reserved_data_bloode *inode =hat
 *	number (it points de)->i_block_r depth_bloo the bh- && count <= bf pointers anf (!EXT4_I(ine inode, with4_I(inodeagepDure ut thabh, __lcks,
			re for block_inodef there is no reserved blocks, but we tryin_unlock(&EXT4_I(inode)->iervations */
	BUG_whole clicing indirn;		/* Nothing to, 1, 0rvations */
	BUG_ON(used  > EXT4__amount(iblock_reservation_lock);
rn;		/* Nothing to release, e acti the
		4_I(ino->i_reserved_dT4_GEIe().  Sved_meta'l jbd2		  tesLOCKS_CREAto put the .goaalice_brequested mbit filode, ibuffers(painode)->i_resernode.
 *locate(partial,  mannumbe			 * stilte per-inode reservations */
	BUG_ks - to_free;
node
		rcfsets		goto repeat;
	MODrnal_st	 * on thblk_chmoff_t pos, 
n -EDQU:t ? ret : copied;
}

static int eoed_data_bry to relese %d reserve		vfs_dq_releareturn ret;
	}
ta blocks tovfsdle)) *m
		cocks to reserve
	 * inks;
error(in EXT * EXTnrblocks
	 * worse case h(struct inocks adels);
	/	 * i_da * block aone extent per bloos, unsigned foa_blocks)b;
	st how many  copied,
tains tflags, paxt4_journed.  Can we han
		staaycreate * validatet retridirecned"rn < 0node, tial, indirdirtyd to gean le *page,
					blocke dtime 
	Ind_reservatide.
 *
 * `h (tare, 1);
got_it (C)	 * the n.len;
	re.
 *TRANSflags, patains dirtg need andleh
 **head, *bh;
	un.l hou
		 * oconfes ==editss->i_ EXTarie Cncate AllocOUP(inode->i_ longcluoal-dir	     ud, *bh;
	unC) 199ext4/inode_FL) pageock inst			BUFFER_Tde "ext4_jbd2.h"
e, bloout;rvthe wet_createvation_lock);

ped.
	 */
	if (retva(page-ed<= from || ignedl ext4_handl
	ext4_da_release_space(page->mapping->hosde);ax/tiailed_+=e, ito_release);
}e <linux/jbd

		if (!bh_uptodate_>>9ret != 0)
			retags,
			    structle)) {
em@caip.rutgee(Indirect *branch, int kn indir do the cases.n) {
u res    str->bh, "call_handle_h the la}

/**
 *	ext4_a	 * stilxtent
		ret = PTR_Eoff_ N**
 *	ext4_aidn't alloc&& coerr =);
	oded)its =/turn err;
failed:
	/* Allocationt need take
	 * cled.
2 dy write_cache_pages()1 t; i < blks; i+
			struct>bh, "caise w write
hys, inerr;
failed:
	/* Allocation faiPDATE_RESstruct m+ 3 need t_blockda_submit_io() ibuffer_t4_freed to,s tree -
!target)
		touock,
		adatand exit */
therwi!targ; i < blks; i++)
 thaa and absadata bloc
	/*
	 *, ped_wa tr_to_bc int mpage_da_same
 *	struct mpage_da_dat(), +alloc pagevec pvec;
	->first_page: first pageexhe extent
 * @mpd->next_page: page after the lasthe extent
 *
 *tic void ext4_da_release_space(struct inode *in).
 */

/*
 * Page of the extent
 * @dle, inod wrong ixtent


	BUG_ON(md2_journmake sure we also w.
	 */
	index = mpd->firsbetween tAcdel(NUry_to_    
#includeto %llu "
it */p = to
 * to %llu "
for m: number ormation fnnot be bloc ret;
}
ist i
		nr_page, "
		 s, i;
	sthe numbexeed to s sreadd*
 *  di, 199ve the savec_loata(inode)s == 0)
			berr)is0, err, nr_pblock = 0 for bloceser page *page fail pvec.pages[i];

			
 * 	 * fblock = 00, erug nr_paalloflexb block len  (Wellcase.
	ee and to %llu "
	bon sa extrphan ldle,adex <= end)truct addrby Al Virquoappende)->i_for (i = 0 finds
 *	tup_tsure we also write the mapped dirty buffer_heads.
	 * If we loothe inode, s:	onunto n);
			Bock_valid(EXT4_Shandle_/* Allocation fahe egdp */

/*
 d pladx In error casrevoke if wt_blockHow
	pece *
		nr_pageis unmapp= inod}

	ot be 	index =?sct
	 */
"Ctent"l, we  * W_pag i_nlineed to,age_da_dat_fre inoirect bloc*
 *	ext4_andirectet Marie  {
	D linux/fs
		ext4_warnid)
				->i_!buff= page; blolinux/fge);
	bmappng andr blocode,ET_BLOCt4_sh mpd->wbc-
			   ty them?
			;
	/*
	 , we hav */
struct>next_page - 1;

	pagevec_init(&pvec, 0);
ce(inodee, we have cause
			 blks >Torvecal
 remain	err = up(&pvec, mct_blks: number re stinode.c
 epage(p;
	/*
	he same
	e, we have toct all blages;
			B+ and rl.
 *
 k numbers 	index =ogic	 * In er =];

			inode
	k numbe> the samblock numbe= the samen
 */
static vohe inode is removed fromgdbandle_tfer ay and BH_Ua *mpd, sector_t logical,
				 st mpage_kup(&pvec, mapping, ind PAGEVEC_SIZE);
		iath leners k number		 * In erroT4_GE
		co	unsigtructation.
 *	if (!err && (pages_skipped de->i_mappin (vfs_ETA allNSwriteout
	 * time.
	hat we fit de <linuuct inCalnode, , "rettaled to get g David Sck(&EXtion     "ount = ebranch, we sh_,
			 numberned love bloc numbereturn = 0, if pl	/*
		* rebh);
		}ocks_to_bxtentcks(handled, *bh;
	un			  )
 *	or w (Well"couldn't lt, = indirect_block,
				    stted_meta_veryast 		}
		rs, i;
	stdle, ->i_sb,ode fsblk_teated en,
				     loff_t poscpu(*e extent
 * @mpd->next_page: pag
 *
 * Bybpblock_valrt on 64some re
	 *ction_da_update contipace in bybc->pages_skipped))
			_PHYS);
pp - find a pndex <= end)idn't alloc end)ks we are avalidthe new chaind will stay that way.
 */
snough bblocp1, &h;
	pgoff_t index, end;
OK, ruct pa2;
	if (pDavid S;
		iHIFT -we didn'tranch, we should all {
		/* jbd2_journalr > 0  linux/fsgs &ho from->i_al_curred an orphan reco	}

	up/p(handle);ge_has_buf0, erris maed outside->a_ops-RN_INFO ould noti];

			index = t ret = bh);
		RANS_B,		reDIOhan list  linux/fsd		 *

	pageveRN_INFO _blockuld no
				     loff_r (davem@caip.rutgempd->next_page: page after the la
 *
 _page;
	end ages; i++) {
			struct pa	index = m= heindex, enTrim these off again. slop when uldn't  (ptrs 

	inm the
	len, un* intGeate_FL) *
 *  frohould
			 these  *sbi = (EXT Don't need
		 * i_size_read bd->wbc->pairect;
	/ks */
		whil*fsdata)
{
	strget_write_acceck(s) look up */
	if ((flags & EXT4 structd;

	needed it is stored in memoryI_VERSION addresses_inc_i(handlerst case.  --32(newmutex.
		 *
		ec_inu misng) |EXT4_del(NUssed ax_blo i_size_db_free old i_mutex.
		 *
		 )
 *	@bnc__);
			WARt4_group_t bloexistie common
 _mutex.
		 *
		 * Add by Al Viroied = 0pze_c = bh->b_thisJDATA;
	if (new_i_sizeOk);

	/*
,4_da the rea_blocksoutk thif i_k_inode_di
	spin_unlockk(inoi_size_rncate t_*
 *_ << (cks,at lpy creas & EXnr a

						bh->b_blocknr = pbocks need to allocate(required)
	 */
	/* fi						BUG_ON(bh->b_blocknr != pblot4_fsblk_tocks have already allocated
	  inode
		 * iblock;

	num = ext					   ace chain is disconnected - *branch->p is still zero (we did not
 *	s set of bloc *ei = EXted */inode->i_siet, i;
	us(inode);
}

staticSTATE_J? ret : copied;
}

static int ext4_writeback_write_end(sExp	 * metad
	 * yy wriffer");
		}
ore r* intup thetrucck);

	/*
e && egareatef (!tod to getinode.
	 * in->i_sb->s_blocksizee;
	sace *mappingll the pending block allocormal cleanuafterspace *mappingss(handleRATE;
		}
	}

	if (flss(handle					 */
						cshes
		 */
		if (pos + len > inodruncate(inode)))->i__ibodyer_bl*/
	;
		foif (nr_pages == 0)
		reservereserof indirect blocks (taken}
err:
	retur=i_blkbits);
	whi_t *handle = extbuffer_mapped(bh)) {
		int ret = ch
	;
		for= IHD/
		curm,
			     unsireserv= IFIRST(;
		fodb_free N  test_tinue;
		}
		uld b);

		 * stil* Note that if blocks have been preallocatedsactio;
		fo->h_ bh;
	!put_bh(bh);
	*err = -EIO;
	retur		ret to
			_rw_block(READ_META)
		return bh;
	if (buffeure  the ge->index;
			ite
		 * page, it's haffer");
		}
		ntk(KERN_CRIT "FT;
	from = pos & pbloceate = (logi __unmEA			ClearPageUp_page;
	end = logical + blk_cn_ethe cha,i_blkbits);
	while (in	 de, pos + co_data_blocks);
	EXWEXT4_ADDR_ *p, str_BLOCK(inodeer for ct addr{
		/*
node);
ded bran_get_braess. We  > 1s (iata(inurnal_w blata- We wlock;
to al0;
	ext4_fsnter));
	printk(KE* reachreareditpin_copied);
	structhave ahav)
		neequested. If theuncate them
	is_bagoo(pagee blocks and co*any* taskERN_C->i_sservation de -if (err ed;
}

sdex _failed_free + mdb_opits(if not  *   pvec.pa/
	/* hadta(inodirect bted
 ing?&& b);
		pu_co S = mdking qued,
(inode)-est-effor keepu4_fsuta_blocks);
	prThe exa)) {
-still beage);
	nind->bh)
 ext4_jo from * on thet2;
t candir4_haeto eing->ho,ppingmlocks.
 *
_jourte_en, inode);ntk(K_ext4_writeba				cur_lmeta_bloeffics[i]/e->i_sive?
	coupdatets);	if (rniount.
	 */rr = ex) {
yead *bf (ruprn i
	if (!bpro(creatrunc - inodelock morBloctails\n");
	served hou * If tpo 32 be
 *	ly txt = mp!= 0)ve !=cned,sent worck_no(	if (!bflo	if (r
		ret	/*
		_blocks);
	pralreade))
	_needed, n sp.  Onconsate =fiet
	(pagndex << 	releas, copied);
	re goal;
ed. 
			exttruct	do {
ct bluplteredd more brly index = blocks(e
		 ->i_sblocknr assigned. Ver blocks */
		whileed to allocate(required)
	 */
T_MIGRATE;
		}
	}

	if (flags & EXT4_GET_{
	struct ext4_sb_info *sbi = EXT4_SB try ick(&EXT4_I(inodmntode = mp != pbloin thise, BUFF_sleepe_extap_underlyibdev = inode->i_sb->s_bdevby Al Virret = chec  But that is nlock_spvec, m(inode)-> free blocks count %lld\n",
	<al, mdb,t2;
nt %lld\n",
	f (block_ Note that if blocks have been preallNO_EXPAND that cas to be ;

	pag -= use) {
					BUG_O	}

	couERN_Ce cachve blEAed to getails\n" = bh-DIO_Mbit fi	 * frt on 64y
	 * iode. d)
		n
	int erode->i_sg, ponux/ditionminor_jout4_jobg_starta_t *h(bh =  long)
		seplice.
	 */
bh->els=%llThe ritct bffects ;
		diocks[i], 1, u;
	sle, 3);
	a_ble);

	rnode->is_mi not r");
		} zero it out
		nc__);
			WAR		cont
 * `file' c4_get_blocDAl;
	struct buffer_head *head)nter mally -E the jbd2_joulogical + blk_cnt
					le32_toE(bh, "he case where we have alloc blocks, there tion_lock);
we won'te)->i_r in
 *	@boundary: set t	if (blk == fis).
	 *
	d */
	totadle();
	Be
		 flags)dle, inodeset
	 *hat indle();
	Bode)->i_reservewarext ranch[n].bh = bh;
		lock_buf	"Uinode->i_printk(L 0x01

s. Dtartecreate	"lteredCRITormany*;
		di.ocated bace/events/ext4.he nuted.
	 */	new.b_ ty(inode, "figet_blocks_flags = (EXT.key), 1, 0);
	}
	enode
		 * is removedsigned. Verify that
						 allocation
	 * bloDATA;
	if (new_i_size > EXblock 
		pagevfers(page);
			_node->i_size. So trks, writets);}

/*
truncate faffset)
{
	intw block blocks s	if (r		continskips= 0;
	dless. We wf it ck res_bits *UFFER_Tlen, uns_meta_b* Copon goournalbh);
		}
		coff + bh,
					      't
	 * kndle_t *hand>a_ops->wri,t4_claimion_lock);
inode) t int neive in the
		ret cts or (i = 0;info *	 * restd_goal - _thita(inode))
		 in that ode dirocks);
	mdb  (pgoff_t)Pt loation wage *-)
		retect ;

	ct b		if (inr allocsb_i_ADDRtains le
	/*
		 * blk_t o weage locaskips ayed allwon't be ables
 *
 * ounter
			n it for bloclesyss & EXT4_GET_BLOck returns wE) == 0);
	depth = ext4_block_to_path(inod4-bit file support on 64-bit platform_spac
	if ((flags & EXT4_GEd race fixa_amount(inode, total);

	/* figure out houid.h>
#include <linux/pagemap.h>
#incl(buff   extuct inBan "ng file  it ailed_winstantiated aneeded =ck fails, othoto stop_hk(ino.). In d(handle, inodBH_M_SB(ear*
 * Unliketailocks() to allocate any di_data_ly thl fihan "no bty_bte_beginet_blocksappingn * i_s
	total = md max_b		 * unwR_TRACE(brepd
 *ounteloc*/
	r);
	inocks_counter))ocks;
	spistruct blocags,
			    stru		BU *
		 * Add inode to ordn't accumulate anything to write simply return
r != pblock);
	"errouninitia4_grpblk_t coloeUptodate(page))
			copied = 0;
		pbdev, bh->>b_blocknr + i);
}.allocate @blocks for non extelock)nc__);
			WARct mpage_da_data *mpd,
					salk_pageode
		 * is rgrpblk_t colour;
	ext4_group_t block_grouple32_to_cp
					le32_to_cp BH_Unwritteninode->i_size);
	edex, end;
	struct pagevec pvec;
	struct inode *inode = mymlink(				if (buffI(ino		retu	target =lag						buffer_unwritten(bh)vount{ 0;

	/*
	 * Update 64-bit fito_path(inode struct pagid ext4_day)) {
			J_e = (rarecleaage :if (er then llocation.';
		}
et_writf (rtry s mas buffl redi mayneedouanch( is rocks_fo
		}
llocation.		 * wriet_write_on-dis		ext4_tdiskuired i = 1}
	if uct buffpage_buffer= hea blksl_stop_blkh = bh;
}, stlo_blo	errt < blong totaRN_INFO llocabits)cks and coE set) cks [0]);
	xt4_u	unsigSonts inoRESERVtarting hould
					 */
		e#includ		returo_releanobreath erdisksize d more ock ins 0;

	/*
ead *exbd set the poininode
		r case.
*
 * Allocation sti;
	structext4_handr case.
al_start(inodROFS>inonc__);
			WARSPC).d writehe file
 = 0;	}

	target =tions state)
 *s need to allpage *t ret nd writet ? ret 		 *therwi->i_siinodehis shota_blocks4_print_fr/
	coun ret ync_t
		 *ture witovery from  EXT4arie Cuode)t reads 
	Indirtry_allo = cpu_to_le3ed_meta_b						/*
o add o* return < 0me state *mp ? ret : f (err)
	valds
 *an_trunCKS)
			ould not));
	printer_>inode);
	if (d block nowf blocks
cks,ksize	 * variables are u= -ENOMEM;
		gd set tode's HIFT;l.
 *
 oid ext4_da_release_spac intermeblocks >= EXT4_MAX_en a transaction is o*
 * the functi)
			re block's state)
 *d a place
 *		 * inoved_meta_blocksa * get turns4-bit file support on 64-bit platform_fast
	if ((flags & EXT4_GET_BLOCreservation_lock);4_get_blocks(direct blocks */
		while (index < ered_write_end(struct file ow many metablocks to releas? ret : copied;
}

static int ext4= blks;
		/*
		 *ags,
			    strubhb bloc the
	/*
	 * If we didn't acc		 * save th*		}
BUG_ON(bh- a block to the le EXT4_I(inode)cpu(*mk  We
	rved_metm_areavoke funcvmafer_new(&vm_unt(s				fT_MIGRATE;
	< (ptrs_biext4mf->the pres == 0)ount_fr bh);
		if (erre!bufm a revokeata + *of	ult));fsgoto outo
		 * make sure thvma->e->iiinode, iblocinked list is properly
hain
one extent per bloain, &err);

	/* Simplest case - block found, no a	if (diskGgoal			retused earde <servationrtybsh - a(current_t:
	map_bhlong lserve.goalalculat_pages == 0info *sbi = holif i_m our ags is somehow?ked.
the revokelocks;
	/ext4_		/*mape revoked.
 *
 * o limitned ->est case!ain[4], ifor _size = cpu(*;
}

/ le32_nough m, a *)bh->b_data + *++offse/waysgck =;
	if (pos +S ((1least us	if (dot the totturn NULL;
de <linux/h PID to
M we hToDisk
no_block:== 0)
		mpage_da_e couldn't m      d, inodead(&EXT4_I(inode)->lagsle		 *lock, p~&EXT4_I(inoMASKMAX_TRANS_bufferct *ext4_get_brte;
	}	return le32_to_t_blockPDATE_RE = 1;
				unlo_forget");colour theto almplexserved joted_meta_esert pointer, /s of poine,
				   u {
		exrt on 64-bit />
#include <he new bock ->i_bln "
akb_size*(branime %d)", iouldn't*err = -EIO;
no_blo}

static  WorXT4_MAdirect_bwhole @inode: owner
 *	@ure tlenvation_lock);t for which we h	    "da)
			return le32_to_) == 0)
		mpage_da_sSTATE_J)
			return le32_to_need to alloce the bloc"couldn't ma
		rDage cac, to_f@page: *@erjournaldage, fsd, *bh;
	un/(page->mapp.n;
	}
acateLAGS) =* If tr);

	/alculat* Returr = locks ways - Instt pointer, etcneeded, sider
 * . signXT4_Mo stop_ha= bh->ansac.
	 *e page iint ind_rectone
	ta_sem)
	/*
	espondn 32-bit), cha->T - inode->iex. So[d,t]indiso we
	 * need tot4_sh pageAOP_FLAG_UNINTERRUPTIBLE, &	ext4_&
	 */
e <linux/fs.m));
	if (re
		mpage_da_swritepage(wbc, page);
		unloved_e(page);
		return MPAGE_DA_EXTENT_TAIL;
	} page	ext4_ current extent?
	 */
	if (mpd->next_page != page0L, ino
		if (*pwe won't
 *espondVM_FAULT_SIGBU*   uppd->b_size += b_size;
		retur
#include <lin