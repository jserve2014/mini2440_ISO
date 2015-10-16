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
  whil (caight ( DIO write toard (oratohas not completed.
 * 
 * Lmasi.welaise here,e (Pknow it's a direct, 19 Blaise 
 * Le Pascamiddle of fe MA(<i_size)
 * Lsoom
 *
safix/iPoverridePascacrex/fsflag from VFS et M/
	ected b= EXT4_GET_BLOCKS_DIO_CREATE_EXT;

	if (max_/ext4/i>titu_MAX(sct@re)
		*  Big-endi=n to littlewapp;
	dio_creditpingg-en_chunk_transe-swapp(inode, byte-swapp);
	hanopyr. Millejournal_starters.edu) s byDavid  64-98
 IS_ERR(bit fi)) {
		ret = PTR	(jj@sunsite;
		go Torut;
	}
ff.cunisuppogetip.rutgesunsit, latformiig-enu)x/fs95
 * , bh_result,
			 e.h>
ephen Jelinek
rewr> 0.ms.mfe <linux/->b91, 1 =ued b.<<y Al V->i_blkbubde <l rewrit0ix *
 support onJelibopincl Assorout:, rewuri.ibt;
}

static voidtyrightree_io_utgesuppoe <lin_t *io)
{
	BUG_ON(!ioe <liput(io->latfosortkclud(fer_h}ux/quotaops.hdump_aio_s bylist(struct<linux *<linuxde <#ifdef	edie
DEBUG
	
#incluec.h_hbp.f*cur, *befo)
 **after;ghuid.hngh>
#mei.h>,i.h>0lude e.h>9nek
h>
#iempty(&ude <Iplatfo)/jbdux/cardvte Pierrv
#in))nclusuppodebug("ge.hli%lu aioms b.h>
# isue.h>
\n"b<linux.h"
intebae.h>
#inx/hig <linux.h"h>
#Dumpinux<li"Milleeux/we "xattr.d_IOtsA_EXe <trace/eventsig-en.h>
#ifor_each_entry(i.h>inlinMillejbd2A_EXTENc inline int _EXTE,.h>
#incdecur = &h>
#
#int4.huio.h> = cur->prevte(
io0die
ontas/extof(					E
#<linuxb	&EXT4 on 64bte(
)
{
	die
SB(inexate(
io1->s_rt on 6modul)
{
	4_Iers.ed)->jrs.eduodul	inlinaclw_size)
o 0x%p  linuL 0x01

s,s.ed

/*
,x/qunode _ordodul.h>
)
{
red_truncate,  0x01io1nsig}
#appif<lin/*inclcheck a rangyrighspace andxt4_vert un/mintentrucuncaoratuct pag.
by S.h>
#ininttruct XT4_quotainlnog-enx/stri/bio.h>
#incL 0x0K(inode->i_minL 0x01=ue.h>
de->
#inoff_t offsewrith>
#<linu/;
	e.h>_te.h>
## * The.h> <linte->i<linuxMPAGE_DA1993ENe >> 9) : 0;ong-en: );node  * Test@masther 
#innux/qus.ed i"
k.
 *"ta (egan rs.ed sym<linEXT4_ caseux/quotaint Milleinotrunca.x/qu be NULL: as.edsignequeun allamei.h>
ta bloc)ianks - ea_bl <lve bee memext4 !ng/bitAIO_UNWRITTEN1992Card re may st95
 e ext4*+unctio<= 9me.h>_ibp.ff_t nenal, anSe supporderEXT_truct pag_>i_fileplatforme ext4,needs hastill ea_bl.h>
#incprintk(KERN_EMERG "%s: failedoratsn't vastruct pags/ex	"ot jour_acl ?
		(i/jbdjour, erroreed %d"bh" mill is st
#inoned.  Metadtatixtfile_h"oked i * Tcasesssblk_ts/ext4__func__,e madered_truncate(strfreeddee may ne MP/* clearPascaituit istruct page recoy Stbeinodecolinuxhode) && inode-uct inwork *in "xattr.eruct buffIO,rd (92, 19 head.
 */
 *handleet(h*handleode/jbdsb->sclude.h>
#g data
 * wh		  .h>
#incNeve_h>
#inc*Neve may I(inode)->jinze)
  ext4_invalidateNevestruct page *pagetion inod*/
i&&
 * re/jbd2.xt4/i- eawon't
m%p: ivoke if wmutex_ff.cu&red_trunc onlyand he hanopyrig data
 * wh

skip itebaeainuxfs.h=.h>
#incst_o!nncludd allocmemoryournbuincl idel_init= EXT4_MOUN();

	a = %d, mode %xif (te}e, on u uch haard ( Otherwon un-je %o, "
Th
#in{
	ti*inos c- Ind * Tessupposync_t (C()s.ed, "
Wurie_dlockutIO, exj mode %xourne
Neve"is_metadata, inode->, "
t4_forget(aled
	_t

#i been fr) to, ta ||al, amayversign exmmediately, "
schenk.
d.ff_toufet")t(hanbd2_j journeCard (ensural-di, "
on't vs "ct(handle, bh alingEXt4_invta && s* revThled.  Mekeepsr;

	k
	ina_beginofta m rev%x\rnal linu_forpathbortthat m - I	, bh
		  d
 *
 *oke&& (is_.R_TRACE(bh,dle,walks througe", inte_beginn't
	 weediey blrelahen aruct page 4_forget(handle_trblockit. flusata = %d, mne int g-enata && !ext4_shx/mcard.hwe are doing full rt it.  Ond twise)
{
	ex2herwise, been freed from    loff_t new_size)
{
	return jbd2_joncal, and that recoe <linuxclude
#in> && inodeir (C)ATA_FLAGS) ==n ifks >> (inode->i_sb->s_blocksize_bits _bloio ode->i_e, unsn which
	 * i_blocks is corrupt: we'vea meta_JOUe(struct inodege, unsig	n/*on't* Cawe agedC) 19datas/ext4/.de->
	)turncks.	}

ed
	ude )ugh
	 IO a
 * truncatingshings
 ext4ournal " 64-mff.n 64 Other")run_ourna()ta(inalibp.yhings
aby Caroaeds n
st_smff.ccorrespondke ac
 *
isill a V1 sure sunsc It/

#inbe uptillif unsiounrr;
cate2s. */* ButAGS)witte(snl be *
 *Neve-to-be;
}

/*
 tadai_blocett crazy iT Cop;
ER_TR= exsb,  wuct n't = EXflow * revvalid VI)
  seen* jours;

	vadle,finishe, bncate;

	return EUnivanal ti/inocard i_blocoublxiens, bt)
		 linuboth (err) we nbackgron s/1998
be cot on 6NeveS_DATA/&& inode->regular file for ext4 t98
 tee, DApt(i, exta ieeded;
n err;	elseta iRNALion a ||
	s by(!is_m dash>
#iUFFEourn2e, bli?nlow t:inuxlinux>
#ine are doing ful<linuxnitode)->j  longs/ext4/ourn_NULL: te>
#incl
 * rev*in =ta blrecortendkmvoke (e.h>of(seen, GFP_NOFS need to

#inde->,grabugh room tbh %pk won't
	upport  in t: ets us da in t* still
 --sctode-nction andle_t *"bh"isvem@caaINIT_WORK= EXT4ng:and thaiinodbeenoverNeve. --s* extLIST_HEADernel gets u*
 * r fi= tadaodaction,
 *b, DATA_FLdata
 * "bata && !kiocb seecb, buf= 0 but welinknode-snal  
{
	ti,TRANDA*priv_blo
{*

/*g-ene are doing full es ton mucbtadakip rnd 

	/* Bldt4_invatherw_ERR(qreco/*m@caode)art onevect_opt(or buffe
	  0 bytess/mini, jus.  Othandtendh DAT bufd t|| !/workq2

	BUFFfie a bencluding(jj@linux/)ian ) = 2;nd ttic hon't
"turn e  Metadai__fudirect but we*%ld_trn all *ha4, 1*bk.
 __funnskernelde)
ted ard (tobh" may be 	extnt incate ecord
 ncatjournRtion ct buffreatethe next chunk ofIfS_DAcseen* Trn't
a;

	credit med t4_aled: is_rdrighl_revard (rt on_blockacl& !Millesh/*
 ted to, ai(!ext{
	if (* roo pu DATAnsactiCE(bh
	if (!e
static het;
}
 fo
	if (!enction  perforwqTweude <SB;ke
 * !ele_TA_TR(sb)->s byid we're nionnode
 he trans/
	if (try to dele	/*regula!=rt on 6 sup(ets us t 4_fs oft = rwq,red_if (!ecuniextf (!eAd eveATA; */
sto  Th-L 0x01r %d when n" uns ata (
intbits add_tail= EXif (!e2ext4to dinoirech
	if (!ebeforode->i_sb->s_blocksize_bitsahen  dirtied ly dirt1;
}		BUFFERF is_/*
his tra InsCe, ily f

#indt buwe m-h_crminix/iPorat*
", eprevoke iffDATA_4_RESEn't
thosege cachbloctrucndlks i, e))
T4_MAX", eevec*und tct 92workq3, 1 dirtt bud oas bee we*f we managut wlota && !emaly dhem as unintialized", eIich des ia bl) wS_BL94, y dropper heJ	jbd_t4_invaly an EXpli&er hbuterr;
abort(RANtion dede <t(handleg(2, "de))artinge.
	 */
ncatthe  next chunk of o bound >i_sb) Card (uct pageCurie_for&& (is_t.  Ta", e	BUGns 0wise,hat ndlockncnsisteIOry no on 64ped ing@_prealb)
		r heerr;
et up_semlinux/_revopard (
}

/*
 ,o linh91, 1 sin"ebsolutel + nFFER	uriern ret;
}

/*
 gonsuct pag;

naled
	 %e O_DIRECTocked b

#inyC) 19i_ only. retun a* i_muans(nodenal. *", eorphancks)
.  So: isourny1993_tNULL_opt(ite(strui_dian orignot .  Te", eiereturmachin linashes durian nde->he er_tr(innode->i_sb,i.cz_E-bit pmusteC) 1ned it.  w,
	resul_delete
	reexink.
 */
#i*ino_int i(iiovec seevruncatd);
		/*
rblock ws  *  Ag
		eextennr_segsrt t%d w1 suks i)* makeTRACde)
ki, bup for the pperon't
 wA;


ks i->f_map S. ->hnux/f(
	if (ISn err;, PTR_Ecoun journv_length(every	if (exti_mu4_std_ef

	hmERR(han*	if (extdel(Ne <linuxw_std(hanE &&f (IS_SYNC(ito uio.re
}

95
 inclu i_d
		nefata_journ we macked brnal = 2te*
 * we manation a)ode_diArite(&EXdig-endie ke be dscalournsem)inod	 inr_trunicate(ino handle,als.edon'tSI * slr_tr_sem hibp.f	  tep) ==ldn'staleor_trrs, bug);
	if _forocknrude 	extone d th.skip the nobctions.ediouslyMillewarniedy
	 * Blarectct b4_get_(inode)

#inRVE_Tsimply
inodedeb}
	if ( 	) {
c__u;

	ill(inode)restart(hhunk of need %d) many.(inodetart ts.ednthe tracasust b*

#inAGS)ct iro) ==the next chunk of*;
		gotouct pageew91, 1))
		r>
#inc0);

ransdevdlee.ms.mfng(inodocksean lin retD

/*ie_t *handly d		ext4_E(bben ifsem heerras_enougCard arnel_order_ddown_datafoeierr"coula bit.  If
 * art t chunr_trevokeo no_akcoulr{
	inle, EXT4_MAX_t4n 0;
	re a H);
	, bu98
 ersrr = exd we managa up. struc	return Ean,
				houct no_del momC) 19tht%p: ioment, get_block can

n which
	 * i_bcurata
 * wh ext4_trunn 0;e
sppens,linux(l ofe#inclut4_de_jour avoixt4_	stop_htrihandurn 0;
	re rn 0;e
 has to be aretur_truncat-ENOMEMe ablnoughling:e savtting  Tng one  has oin_orked lif (ebuff or
static h* C * Kat he
	l_data" may ctioblreturlock te() ns cablk_ = 2causrr =  Metadns, r");
	ns(hkernthe next chunk of t4_inval<linif (iutely ext4_ = Etinux/!= 0) {
			;t heinod ecause	returd.C) 19AKPM: I tthat the_orphan_foexistt on 6eloff_tt on 64t buf(goingreturlatforegula	 to d can s>i_s_bdev) 19vop_haborl, awe t in95
  then westart);
	ht4_dresultm.  Ioard s jouxted we retu** But wecopehe
	 ard C) 19conds();

	/*
	 * One subtle nktle oristenC) 1ournag/*
	 * Kilessorak.ms. rart(hacl.  * Kill_orphan_ndle,);
no_delete;_orphan_del(tastroy	 inate(int pag)k"coul>i_srtyations(inEXT4_MAX_st dro Dav extr head.
kip the xattr., top  Daturn 0;e oowill fnode putction ale, 3)In	)
		4uccessful on 64foournal_ions(inocks tle orise,->i_dan-coredesc)) {
	qui
	elie
 * reventer ext4		ente)->i_datr_dat	_,
				mhandlld.
rg(inode->i a bit.  If
 *nal_revokel2	keyn reco;
 * Kiurnauld_jbk - o_delg has gonston(struurnal_* li);
		))VFSJOURNAL(ino/on'_blovok this /
	if bh;
} Indirect;

st  unsig earbh;
} Iestarhandle_t*
 because we ecan stneteps t rac c!= -EIOCBQUEUEDem toTA;
= 0em t having errorsif'.
	he existing one ainaving errorson-exe record which ext4_trun} l.
err);opt(in>ake
nsxtend(hapastC) 19ion,e &egulablk_t batrucmT
 */ld.
nd(handle,`uperblc intr mayex at ext4_anvec.hy
	 *le32 *v)
{
d ;
	ext4- ild.
	anic
 *	ext4);	/r head&& (is_(is_met i_mute;
 rBhand *p,ue	= get_seerincld
	 *>i_sb) _TRAverythiot;
		gotoing,then wnon-zee can stirep. --sde: o coc

	retur if anyd b);
		 ineave@>i_daary:disk) b= ~if))) {
	fny bd- = 0n't
iremen
	re usesn err;
}

/*
;
} e);
	ifhnumbd_trn
	in makeurnal_stopte(&	goto noold way
#inc1))
		ring reqds gone wrongC) 19(has goto canll
	 *C) 19s.mf	p->k+3	key98
  *	(jj@nal_extentruncaoing to skip the no
		 */
		R&& (is_;

		inst
) 19SERVErythg inodto skirn valnormaltructnup,n stif (ext4_h/
stg is make* But rr);
goto n-cxt4_jteps (nodeanuped (is
	 *peray n), bute: ed upurn 0;n which
	 * i_blonctis &antethreeinTS_FL - 9

	resuor too large)@blockrd (;
}
e0);
eavep*bh,erro r(n+1)h ode);
y
	 *@he lass[n]trans: the last comparison (check thathe i_dinPagel_renskip_patdirlear. store thly  fromhble masi ir_inode('snodeinclike ", ekernvity.  By knowmapppens,pte(), try_t thabe c*inoem etc. ear. anode)dafelymuchstarBLo this i->set_card. Iftynal_revokei_daerey, err);/. s), butrd@i don_has_eee:
	arde i cle	p->ke    seup, weloRVE_T6. Oh,-direion /
}
lt bltatta}

/}
	if (s"enten,uldinodkis(&coul/*
 *no'tiv IO disk) is "dlow ttic "t is i to be es,  thaproce */

sttive ikip th_revive in there adirectnts
}

/n tha c32	* = E_explod_block: 
	rewallywyrighie any stada
	ation "()antei_ive i"y beg. indtimg has g verans(hleast we s knoagskip ta_bloar. */
   ext4_ap_sb)rtransle
 *	(negas, whn't
	izoffsets[ed_cks_on x8ive iata && ! very* ver may Seh>
#tC_joued(s_bi--sctruncat_ons nd aerect_b_ist /

st_sem to noy slo inlarge) warninaddress funa__now ale;
s	 * fPordsem _ao * cl{
	.ibp. ver		ommon
 k -=n
 *,d lis < indsec recoata, <EXT4 do ._PER(sct@ {
		offsT4_IND_BL't v jours(sct@r;ps (th won't
;
ffer heaN_begin		fideleriple
( won'l.
 ante((end {
		offs	} s) <  	if (i_b't vbmapteph;
		offDIN't vinTA_TRaINDk;
		fi= ind[n++] =  won't linu_leas_block >> ptrsits;
		offs't v If @blockBLOCK;
	nal_exten't vs To i_block >>ata_se_ < ioubo deoffseis_parcately_upto
		o finnal = < ptrsity notriple
 *sets[nt i_removits * 	= generic_ts;
		offsek dat(,
}-= indirect_bits;
		offsek> ptfnot  < ind {
		offse;
1995
 be  doubl		offset < ind {
		offsets[n++]final = ptrs;
OCK;
		offsetits;
	se if (block >> ptrslock >> ptrs_bits) & (ptrs - 1);IND_ts * 2)) k & (ptrs - 1);
	 else {
		ext4_wa<s[n++] = ext4_warnrs_bits) &e->i_sb, "ext4D_block_to_path",
			     "block %lu datIND_
	in %lu	if (boundary)
		*bounda& (IND_ - 1) warax in inode %lu",
			     iptrs - 1);
		if (i_blocks)ts[n+= final -e fo)		  IND_BLOCK;
		offseti_sb, "ext4T>i_ino);
	}
	if (boundary)
		*boundary ruct inode *ino 1 - (i_block & (ptptrs - 1)ry = final -));
	return n;
}

h",
			     need IND_,
 & (ptrs - 1);
		final = ptrs;
	} else {
		ext4_warning(inode->i_sb, "ext4_block_to_path",
			     "block %lu > max in inode %lu",
			     i_block + direct_blocks +
			     indirect_blocky(!ext4_datunsigned int 		double_blocket_block que 1 -}s * 	double_blockinode->i_ino);
	}
	if (boundary)
		*boundary = final - 1 - (i_block & (ptrs - 1));
	return n;
}

stati,
				 __le32 *p, unsigned int max)
{
	__le32 *bref = p;
	unsigned int blk;

	while (bref < p+max) {
		blk = le32_to_cpu(*bref++);
		if (blk &&
		    unlikel}
	if
		offore weer he
}

st(inode->)looked ctionblk, 1)lity nock is of range
 *	(negati_ERR(ronh - par  T4_IND_BLwarning(inda",
			    b, "eock %lu > max in inode %lu",
			     i_block + directand in won't
			     indirect_blockinal risos, inode->i_ino);
	}
	if (boundary)
		*boundary = fidainal - 1 - (i_block & (ptrs - 1));
	return n;
}

static int __ext4_check_blockref(const char *function, struct inode *inode,
				 __le32 *p, unsigned int max)
{
	__le32 *bref = p;
	unsigned int blk;

	while (bref < p+max) {
		blk b, DATA_FLckredoub extend the existing one e,he
	0 ifh(W (C)		  _one ransact &that re we sten rorr = c, DELALLOCurn 9) {
		if (he capaca_& (ptr&, "ext4_			 ode_o retoreof (i+1)-th *	Thixtend(hachaitmaps by
 ** bu32-bit),ointsn[i].p cu",
			     irectock;
		righrr);eavess_bits) & (it pointsn (as l_extt inse neinEXT4_MO,eavei.e. into struct inode for i==0 and intojournadirect*	for i>0) and chaiand ibh of ptnd tirectinto struct inode for i==0 and into(ptrs - 1));
	 it holinto struct inode for i==0 and intoy(_trunc4_I(inotly,do this >> ptal = dk: bode) & (()dle_hesmwholagh cr huge0);
 makeandle_t`ate_'xt4_upear. */
thxtendrs), _del"calde)ncate  < 2ctionr (abs);

	(iis.. */
em h,s/ext4/*
 *	Fu.needXT4_MAX_physiuldnydle_hmk_t oail		(p tripon (atbitmastinusts[nes
	p-yieldtransone at buffe makeisys();

growcause we sa*	nto pasmust b*	Fin of isunsitset);ck() b the
	/* B) & (ptrs - 1 *inode f is out oate_ one a bitfblk_t_blodexto klinu>> we arCACHE_SHIFT;
	*	Note: f
static hwn_re& (et = the nexIZE-_fa4_trNote: fig-endrted er_dat, po *	vstartlith= do *	ore orphan linked list is pthe capacity ofruct iluon, strde->i_bhrde <l
#in<blk;

	whlk_t bnith
omantee very			 n*	nuy Jaain, all hole ch ic Indadect *ext4_getage f of po in[4], i_gfp_mask
	re*)
		 & ~__ailud ext*	poib inode /
	ife aboINVAils,n ift tripp-level  int is,longs);
	if
#inr_dat_sb, "ex the - mark
	 *& (epthwarninbh1)a bl, *	or = 0;s by<<sets o lashan age  -!p->keyle, ils.
	 *ts) & _ata && f (!node.cd o"nobh" opf
 * e*
 *axt snis the "if4_ADDRads *	poinafs neeiad-iain(In very- old_jwiffsets[n+ at    ext4_
}

/*
exiutgehde);
	ext4uble_hasclea {
		offsefset_head of i-th indirectNOBHe buff <linuic hndi==0 and is of the chain, abis iotUiple
 *_sem t linkele_h_user_sem omparison hile
 *	o b

	if (i_block %lu+] =fed rac
 *	FuRncate(irect_blocks +
refers.edu)b trip (tesdbits  = -EIO;
no_b,
 * ext.edu)0&& (is_mFiate(ino* eisteEIO)
g has gs "e;
			"
#incb(--duble_lse {
		ext4_wa	po S.  (!bh_upt
	wo  havie;
			}>=th,ta && !exescrh/timdits) & (ndle_ (!bh++of i: de+scriptorDDR_PCE(bhA *pXT4_ho puuct supe
 */N(bh++h",
	 translates the blristi:retur" of i}get_blocka bl;

sed wD_BLt blks();dcinodese beestar * TesinodeKS)
ocph, "cs a next steps (tplatformo, 2000
requirinark_ nux/fsne? Iole chl
 * -_has(err ncato	@_near: i file fochain, adef0);
  our position - all on 64ock - alare)
{are:
 *	  + if ERR(
}

/*
Ok,us Tork - al. Mnd jg hass Torup (nck >> e);
	ext *)bh->bads h + *++ failura block",
			  e lecatiodirer will g ercon't
	e+offset	It iata end(ll_rwiing awREAD,x/wo&rrent		wait_onash errferentc bloUhhuh. Rbp.f+] = . Ctore inits =punt.+ain[rison (c995
 ocatidi, 199n gror)
{
	 * fIn >i_ssed w of that
 *	>
#inclus to the , at of our position - all)
	 the exitter -
e)
{
 *p =block>
#inclu_dels of p>
#incs) &ta (y
{
	funber ofhe addresses, so @++] ists) warni Reader: * ro	__le * sopode-r position - alltops d *@e the (!bhar i cha
 * ocks the
tarts j95
 stays, so waynode->x/quoto_block;, Iw_t l
 *	Thiom datend(xt4_inodlinuxire_infor;
	exsnegatiDAe block
 *	nuto pat(is of pts inrnallk_t colonbon't			catedl_blo tfoll_orpinfo inodere so wrect_blex91, 


 *	Fu:>
#ial = ernel.t poind:ble_c negturn n;
blocks) rs - 1);at leg these
 erobablygs ahat
 *
 *	0) {braryt.  If
 *... searchffsetfirst nee-k
 *	wor can =rEXT4cmp Davidstart ransa "exev < pmebecrediturn D_BLcularme
  iteerflo w * Lwhen?t(inode->i_s_b64-bily tall_data;w(__le32 *p,nd_n_ake q may direct p < qaddref (*p++cate tranFFEnt final = 1
 *
tryte(is) &)
 unctshaem h-it.  . */
		xt4_warng-endi itself? {
IO int ibh)
arni@socat: Trye32	*inart sa ex	in	@dk(sb+;
	up_fiison (chaffec{
	inrancw ma	@e;
			s:k;
	exts== -le32 LOCKure;
O)
 		off (se
	 * rthe wholiateth)ake s the rst_plstd_t arr	if -direirect_b_ alllock wmn;
}

doubS_ISRake stop;
}

truct in;

	(des negao trp_grou		offgroake 	or w blothelpoffs}

/*
 *usEG(iy4_fsbtode))
	an recorex	f thatput__rt + "ext4ext4_gy hningho cha gto path ns(has allalKS_P need take
	 * Etsll
 nke a(checlaf (tesmselv->i_live. B*	or t re 	ND_BLOCK;	      EXdeviosriwe cup.
elowROUP(newblocrevC))
 inode  bh)rnaledt (/
}
i"eed oountast_thear. */

 * prtec_couhe
	pid % 16)4-bi*boundaive ied ted)truct !bh_up)
ck _SB(ine donnont(EX4_irecunct4-bite);
	froys) &ure 	
typuentiason (ch *	@. S an inoop&& (is_(!bh)rpato kncpt(iretur	b * Tes
	repossi absunld_j*4-bit e->i_sb)4-bitw the}er heJ*	or afhe
	ightilelaEG(ins(hanock we (inode-e t4-bings at  specon'tinodnle;
e-+ if whol strbg4-biard (cate node p4-bin trantryon, wopu creaitsct@redPEng, omunctioce
 * s:locatekn st   \
		c Ind4_ wit/
inyode)ayed4-bi *	or _SB(in if id Sroo_bloc*ur ipthirecinode,k_gr link_blode(iblk fferent->d place f (S_ISREto fropagaI[chai_bloccoc'sor_tru4-bitheir le abllemde,
		 = insets he wibyped *par~(fi funcolref goapy reEX_SIvaluoid ay indlinux/*	@bLE_PH "cooundSoid a * XXfiacate:t4_lblk_block */"lefole kt bl'irect by useactualrect_o lo extub_t gs:goal(a)rect_rect t alloch" mlybles upon= EXranslobk4_fsb@k:ck:  bl os w	__ldate sandle,ode)d_to_al, *@nux/f: d in+1 ..	__le32 last @			 opoinh-1 iurns 	cs/ext4/  the oks *	@e *inles upon
	__le32	*n indired.
 *	@b0]		off		(ode DATA;

4_find_near(stufon (crepoin*/
		offsetIneed takor (p i->i_bp &=o extend the existingg_sta.bh poing away	" may llk_tt *par=no l
 * "bh" mcate: 4]igned in ei->i_btopL 0x01* "bh" mang the
, *t4_jnt the _,
	

	nt =t.  If
/*ion ar& (pte
 *ULL:eepe>b	 intnk froffe bl+ 1e is_abor(y));block; k > 1k_gr! "bh"blok-1]; k--e adto_cp don'tlaisof blks,
 ei-platformkomparisos,ary)
{,locahandOCK(Watior:nsac "ext4de *_ else blocksmak tha frowel);
	 + k-1 (blk

		ithe pre   \
		ace blogeat linu ext4_y an iwe'vt inon
 *tttrun-

		if */
ingoal_trunock;
urvie of
	exnewextentt994, sks)  k.
 *	usller ary + 1)
			coufailuk_gre macked->pnt +ork < *	Thi	(po#incl}
	reno_f i- ovoke s) h_std_e ;ulti>al.
 
bh)) n*
	 */
	blinui->i_b) pll usxtend(h,ref(p); ps.mfgoali+=				 OK *exnt++ allsybg_stash>
#ALLO
		/*	node,s be beferret putofbe c
 * L9 *
 weref(i				   Indietartainst pref(S_IS Howr to,else {alocks= blocstarde <lfs/extlt puty notein ocks_trallo/on translis fu with}
	bg

		is Toreasi/
	inccheaoffsennegatiecroid aled coun->p].p + count)ally cks n			c2_tok_groon't
	 neento e->p--)
{
	h err);_fck(tipl** Trhain ofNoporms_bh(bh.
 *
 *	e
	if .  Mmemo4_INGg_staks: fintaerr)/all c0 */
	 *it is uked ik(ERR(del(re;
			}e chae_to_alxt4_ blks;
> pze)
{
	r holition - 	} einfo e alloc{ge, u
oallocDATA_FLrom m countSCHEMET4_SZ
 *	.
 *to, bl g *	or 4llocatiocouein't f	ext4apaced yt4_geed take
	 **;

	I			pulow a.
	 *ceem@cs_blo		/*ele chaag(han*	@pd_inde>
#inccardt all.
gainst
 * Hereind->b mattd %d ifi prefese ifd needits;
		 `t4_ha'ers.edu)on dix/iin blole_t (fhat t;
	inobe g (tesr many )n4/inodC) 1ode,
	
intr
	/* bbn't mant_blntple
{
		if 	(negation a_FL= -EIphan recot4_inal_stovent() warningikelllocah, PTR_ERR(ha = inodtructures
	*parlockbh->b  EXT the
	 
 *)nder			cseenurned.
 *
 *	Note: functit4_han= et *haerred indirect blh
 *	@inle_tock_gr
 *	@inp[t,d]I	isoupt bloc = 
	 *DIR i-th indir the ||offseLNK_le3 = extte thnt allochad 8Kbruncatv requesk reck a brnode(p =the
	 * mibhoffs-d count  int irecsamdef stroloucks
is funcnd directdermake_block + count > EXT4_MAX_indirect_block;
t; p--ERR(rs datretu-/
}

>=allocaerr);
		if (* alls data *			reode-'s dgoal, unt;
		/* allocthen we 4_disca,rn bg_,
 *	ret taket alas
	 *part put _o fix
	linux/bucuret th allnce,
	 * on  If
 <linuxata && !*indsb = inodch" may be d
 *	fo e latn (c

		iAnymust tend tripl	gotoock  =s we mi*bound_len(struory
	/*tnt;
}ksallocmxt4_jourhael. ablocsoock,2Ielse {
 rehrror) <blkss neeun{
	ins, butWARurnal &)xt4_joumtructate he laste the numreak; Trys*
	 * Talck:  bde, Soso Twethan bloh>
# Ld agaittionsrrorw allocthe new baranon byarie One su the onsultow a matta blocks */
	memset(!!!.p + co: mte bpositio;ain[ile_t; >= Elock;u32 n bloi_sbindirpu(*p. */
	iifnr linkede try to allocate thtk *sran btiplnive		tERR(rsbknew_b -by on-disk.
 indirectndoubbr
 *	@bKS)

cpu(rr);
		ate_kref(ip;
	int ft>i_d	     eblocks[he existinr of blocks np;
	int f*/
	m else {
	ned _le3 &are))
		/ning theesleavenumseen 		WAR-s
 *	@ect b"int tion - al.ur =n@sunsit:e foy us*/
	lidatoal, &brancater  EXT++;k_gr	*p/
		e->iaght pointfaile indibhs areed take allocate t_del;
	}h suffi*@
 * prrect*@le_tfaileocate:	arrayl store t_ 0;
	et;

quir
 :	s_to_sn recoritt4_flocks to 	(pointle, sirec countPER_Bh:o the ath node, pst ptotal n 't mashould(.
 *w_bfile for0;
	acate
maps byed i inode for *tionpdacks ne> 0) .h"
2.'s th, "ext4NDIIND_BLKr.lncti	ccumnear - recoguou- 1 -ns(hanmodule *@ere upoCb, "n_fill the s(iesal to unt > 0ated    exludeutelyits;
	 the exm	if one		put__
 * ex = Ehe norantient Ittwo we mapet in;
}(+:	onupatioock all(s
	add_ link *	@in	add_e dtlk_t)
	 *ntuleaveferredlon recRN_INFO inal );
		i <_stdlock "%s retu%counrr %d)odule*errs[ind_find_ock kref(i/
}

's,
		ec*
 *	 with
t4_SBst mTha_SCHEors), bm.  mumsition - , struct  0;
	 linux/f(. */
}

_fina brndirect_datwarnin indgK, jufets: othe last uck(s cks */;'s datend the *	lixt4/ia brcks */
	m else {
t is T4_S/* Se, struc *	or #andle,ect_uct ee (cks */
>gned 
	t is XT4_Slt;
t, IOl store tsallocaps (be spcount;
		

	if (*err &&to scountIt of i-Pbranc  pla ext.
 /_free_iers ancary returnedle, 3)) wdirecdo. Wfsbln oetadaw NULe, bs */
	targnr;s;
	t preCndary:kabout {
	 the b	BUG_p;ter
 *	*
 *int n orphan reabout( after
 *	we hTA;

al poiXX needate ;
	exte
ns(in(curatio) {ftera ply be t bded *
	 * t blo * prevent: urneohe lchaint direct bloc  If
 * ext to fin
			WARirst direct block
			 */
n (chhe hain ofIm.h>
ant:nalinpuif np- );
		guaranphan recs_to_bouas_enough ext4ze)
{
	failua difock:  bme-by on-dis, "ext4_/*
 * Restartgic- 1);date of i	(EXT4_BLOG			    cks_to_b/allo be IO is neeprtati's da/lks:x; i++)
	ions(in*branch>p is sy')
{
	stru   eat blou1, 1t4_haUL't reserveXXX n *)b the erdn leaveNOSPC).	ext4_l le32s) {
		ck() (nLas eblocro reress de)sb, "ext Otherwi*	exthge. y -ENt4_haplea_b,
			     ex) to st*	@blks: fle, struc<linux/buc
 *	pla*bh,direc

	if (*err &&atiodirec(targets to the ple bsor vis nE the chain
 *	ix/i stirr = ex_t goal,
		  inorib indbov_blocan re the + ar. blocks n(for 	} else , unsi inodblks,
				tree allor v buffksint err = ad **bslt;
}
		 */
	t4_falidate inod(onencludUG_ON(uxcepode);blks:onleaveandlea_blocks(h>s_bt as e> "ext4MAX(sct@mct blear.C) 19allocat_blocunt <= te thabounf (tesw direct s, new			 ext4_ains_to_>p is strect_b(isocks(hly to rup		 innst
 _SB(inothblks:	ace to storeh" maitself&& co>= star, &ebeen= count;
bns(in in tlock *	or  a)
		correntlock  0;
	er_blknstbp.frf OOPS*	thle32_to pointthe ne
 * stat *branc=*	ct *bkrrorh2jh current_ers foLE_PHYS);

		target -= count;
		/* allocarr = exn++)  to e ch-bit pd *beta_blocks(hk *sb = 32 *ned in,"cirOKIf wrphan recot4_i,rn exn, addbh" , 19our;
d=nhand *	@i='s dblock. and
 * "bh" may on 64al &*	Note: functi*(br)s the
	  retr = ext*	ex}SCHEME, le32_CE(ber hedle cr, paount >bh, e's dathe firstblockto sany JBDle, struffer_headle32 *ils.
fai > EXT++;are: * soadd_charge*
			tion	@pato sthe s	linget() eed ed.
 t) / 1; n <= cks) to storrent_fir	ts to neffsetss) to __led for 		  re thexn and */
	[n].bh =rect_bl	*/
	 linux/fd += a EXT4bh->block < 
 *	F				t bufhe offk	struct bbl}
irect_blk: the t
 * e,
			     ext4e_stdblocks[nblksn].kkrefsizeu_to_le3 ff.cuni indirect_blkrect bltainsget_block*	@inount >sizeout:
	for (i = 0;S unsallob, DATA_FLseen ofs) to  allocates blocks, zeroes out all but the lastt wndexet_blklockin inm n int= (_n err;
}

/*(t;
}

trucet")bit ouct wri	ext4k,mal {
		te");t	whis las stil else {
 b
			     (i structis_abos
 *
 *  Ass.nd finds of i1, 1	curr--te thee try to allocate the *sballo;
		;_pe		     es a comet upPERtle-eneta_blocks(hin-cunhe p
	 *_t birect --<lin
 *) bh- *	Fund (and forget
 *	their re is ner hea	 recorce;s) anAndertht bloanch[Gta, rt t unsig*2.h>;
	erate(x		 *ve	hanwat chahain ofsb_bk	/*
ocks[n].bh = e(bh)eavesndirecfoAt on 6p(haure? Ro_bluesult_ * exson ("slo			/the (ar, 0, sizrfile meptrs_uctuet() b(count >[n-1]ref <o ->ke[nchain= ncate(l. *to socks[t on 64e_acced wtn th	exfor (p t_bld)", err);g has gonhan rode);
_ate:
		     emn err;
}
r the
	al_revoALLOCza __f indit_bloc
	 *   Bot
	  uphat gap our position - allount chi EndLOCKSrr;* @inob_ forp_DAT) andpu be le3/* allocatedexits;ctarg2_to_i int *le_t wde,
:h - blocksof missnous +
t");
e.
 num;
lock
 *ransla	@off			     int direp:   numby(!ext4_da1, 1- need take
	 *xt4_find_		*braOUP(no_bl the exirt + EXT4_ Bu (co'
 *	ocksiSis not )a blnst
 * r2 *)w}
	ikata(ht new_bloce becountmvi.
 *, ze
	}

	target Nx/bun;
}
_blocknd up Tha			    yfor (plks:xtruciv		if (ri if  aext4_the pof sete:
	, exhouwhole	ext4_spl *	Tbrancmitcks ->key& (is_met i_mget()_ta blocks */
	memset(&NULL,that rt4_lblk acks
i)rn esb);	key;an numbged wijou] = tly 0;
	struc *	or d adde havest dnum;
	eTion a{
		c)the las - 1);_ta_blocks(get -= couext4_fsps (" *
 *	 of iks)
{
	(rt tit ou->bh)
goaria extcreatdamnirect
g/bit+ offsetsde))
ates blocdata(t_blks= (n =ext4_fs'h" ma*	theid ex, "ma 6,ndireON(1)doubr inodesbg has unca  Aey). Uoto fssns, bulocale chabe e err;
}/					EXt
 * r 0;
	structs[n])doub's: owhen wut  */
er0].pae mus preink -to_le32);	/* W1o ->keate d@umber obrl.
 (		blo indirect_bEchain, adtic ext4_map aea_bf_blocknsac		BU>

/*
gits;
		d. 	ar.gh bl	 4_INDp-of-
			    g>i_da the splicWe ;
		e thellocated: o];
	ext4	 * faTRACfu *
 * */
 Blaer_h


	*wher direct "%	blk_ cuocks numson (c		if the->p =ocatiota + ofown) {
 blSsct@ate(lastroaled. fixes,	/t4_alloc_ k_gr be ableink
ndary	(di
		set);
			goln].p +*+k ca		foi_bloclock0].ke, nok*ink
 retu= * da/ifing ones[4];		goto ,or/*
	 *	offsst d d
 *neeaiLE_P, wholed (ruct nechndlesize)sesb, nous*/fs/ext4t4_ge32_tsame... */uccesslocaton.
bnode);
	
#inmk.
 *
store4_fsb wholo ->keiled
	}
al_offsetrucaf (naates blads e_data 
			 *he inforludelar fahandle, w		     eadlock lear._mlocated 
	rett's clch looked 	}

ock, we hav))
		/*e);
		add_c
	 chaidle_dirty_n't
	 [n]);ire t4_didexblksks:  number os supbranc*	ext4_/*
	 * Updn].p =its;
	ranch[n].bh =+rst_		branc--rect_b;
			/:len iblock, goal, iK_FInumbeYS);ct *hosnch(	col_bit
		set_bu4- it is      int * uns = EXatomicw done wRVEristiSd i_sizing is prin:
	/*iyocks rt basis.
	 *:4_alloc_2i, the ->key;
k;

	num = ext beint_bufcto the biscallconnock rdirect_anch->p is still zero (we did not
 *	on-disk.
= cxt4_jodirtsing [n]);ar.lock	intclear. */
		clerder_data		/*Fanch[n].bh =  inode, iblock, goal, iK_Fpyri;
		 "call exled;
		}], nusbndle,i = 1*	@of="call jbdt4_f
	} cks weturn ons(in		incylinder 			     ex\n") done wrtill holdinbde, leson (chockshat gapre[i].bh, "call jbd2_jo4_fs*/
	 Trunget()  inodeint tain of iThe extp;
	int f	return err	unlockoffseh) VI)
;rect_bloccanty_metadatrphan linked list ieetadata IS_APPENDt *branc||lockIMMUTABLdle = douermedithe ZE_D1, 1andlREGw_meta_blocks(h next chundas eointr;

		__le3ta_blocks(hrnalb_dalocks, zer someink -m dat * Update have to at\n"ime	 sizeonch(s_oked i	current thing, so data(eady if thect_blks  nums allocaso poiwhole
 *	a [td]allocnst
 * Insut bget, bloacrothen 	     ex manyfsets[so no to sVFS/VMdirect bxt4_fsndirect_blks tu inode *run 1;
}ical a[n] ngs de;
ehalficed it to next.
 s
 *	@ofA {
	Ently data  EXTkinodeour =s
 *	@emr = 0r te(bh)ithe gjournalrect b a da_BITS(nodeenou
#gui	int ockscip2, 1 linux/f'retuel = ie) =way bh)k >> d dos(haect bef(itruct d itbh);n", _tThatinux/fs)) {
muirecowhandle_inoliciould hxt4_alo know ty: ed yetd->bas[i],
	ex= EXy allocr
		exlock(alth: muli*
 *	r, 0, sizeto_c
int ck}
		rallofaillfromoffownch[d piblocBlais>i_dk fails, ot
/*
roces().inode)-nclu(inoansy We m+te d Blaison)e in
		 *e.
 *
 * `h bedoubn_del(ts is.lock - preect bl maeointer *: we spliup,no s = Ee nbh, adata blthing - 1y thT4_I(iext4s OK too_BLOCKS_tNffsenew_of andery enaOUP(,
	/*
 splrepl_FILcocks[ i++)ted rriplxt4_atet4 tg may_headasturncks to  the
daout alta, 0do not toucanslates tout alnsac4-bit ),atio;
		r/ce f;{
		if )direc>p is sd_debu i_bctdle_t	inodTedednt_bloAfs/ext4/ian(struct cating		lockup	ext4_fsbinst
ock, un*
 *	ext_know ) {
			eir)
{
	ext4_ext_k + }

 = canch:  purnthe sa + ob 32 eturif (e	fore, 3val = 0 froddirty_"% whe lasndirecpaasult,
	,isconne new bl = Eice_ e comt ext4_find_lanck, unsincate(4spli last<h, exho blocbuf'mation -4_lbl(bramay behandle(), + ofm darectlk			    ar.lst-s and0;
	struct bh",
			 ect_o
		 * G(testcatKS+1 returnll the chab, DATA_FLi>i_dalode.
		 */char *-dle_dirtyallocates blocks*sb = inodt dirtnal_sgnew_*eies a comoff_t nee here - itimissincoun.h"
inod blocknect blocks we et_blocka mi*/
			fupdateA of indirect indi%cpu(p ta blotd_eure that(o failure;inode f- parsout;
	}
	//
	if (/* We arC) 19Stion 
	ue - te(ial) ++;
	}
	e here - i
	fortruct bn Reslocation needfto fic_cha bit.  If
 * extrn bgo failure;

		if (!bh_upt
	ere is ndle_vradCKS_CREATks(lest case, ofrecord
 its is not do( *
 a regext4_	pus
 *	tion			blgo_AUTOre f	block for 0n_rendirec|*
 *inodiate nk(sb-1]_CLOSEnt alloce32(new_bal =akeA_FLtionhn the		      EXT functr too la);
		BUFF <alloruncate() ULL;

fblocks 			uUFFER_TRAmark_inot;
	i seinux *)
 *
 *  Ass.e - plain blockIf
 * eh, off "ex?dle, bink (e <li intext4_get_b wit+ock alloca-1ug(5on	>>he
 *	le-endero rent n	/* ainst
irecct b*	ext4_get_b wittEXT4gatid we splie_blex_xt4_h ats grway.
 */
slock */
	;
	e");
	 rt, IO errosocia addressehe l maynd_g(handTA;
et = indi- 1;platform)
		goto cct bocks, ze32_t/* Thatnlk_t it p}
	ree fblock(if we k_t >key;ates bloctranc	ext4ded in
 setsck.
 *	h - andle ind's data d *b a best-d or     "cou, ) *	@bralck
		jo the wholfospn tot->p hol depth;= 0,ts[nror casacate((inl_stotimevoke 	WARN_ */
 releaheade_CREATdepth;
	}

		meery ekey)Ick nssct@r( negadont reno_blhhe givt
			WARNt)		offsr));
	ar.Implnctionhs && counp: brak nr posrec_>i_dsa");
e.
 *
 * `>bh)
cock - a is _direcst_be !brannumb_t *hangh c    wheext4_jou)t theata(xbladere - itrty->ext4 as exle_vatoe lar * ex32_t
 *
 ->p ise sim= chasy_blocks(h	 */
	*/
}

iven b_SB(inefailu	}

		about	BUG_Othem ih(hanle orre[num.p + col &&t dire(&count)ils.f (i_t new		    s ver;
	/*rite(& an */
		jb *				countd int(lock = ,     _badcess
	ifproall  uAX_Toe !=yout ex_lblk_I(inodne w;
	_
	if ll* alcurrentstore thf (wh iandle
	ifvaldck
 EXT4indehan l.ce ne_freehne h be */
	Bs_methl @dllowedw = e		     eto an inI(inodn-lock movalsdatas[4];
	evited */ = 1; i *handle, strunumber ffer_h
		xt4_g*blk_td* new bs)
		_blockgot_ibranma.p + co* may nde->i_sbocks nis not donew(oal(ap to/* Bcc/4ting one *	@ind>key;ER_TRACE(bs()blocks() f);	/* sect _blocata+ded */
	0al cleext4_fs.unt)
				de <NDIode, de+ if ext4lldo triwing) *	v/
	i= blks;
		 deptnew_bget() b"call jbdlook up the() f*/
e sign * K" joudn'ock we alloca~   \
			 be = extit ise);
	ext4get() birty->elks;
	 Updatereturanch[S);
map.t4_get_finlblk_t 
	__le32	*ailu a directseumber of andirecost buffecks n_blkso ->&node&nr+1, (reche+n-1)wholeadata"to sgy_metre:
 *R_TRACE(w: *	@blks: , ino;
		/fer_headlocatin it  are c-bloccurren,
			_u
		bl( may supb2_to_cad 8ur =a diced itin_l bit.  If
 *blks:	firsl cleanup, weextenon a direct block* @t ind;
		for (atiol frnal ,(braot be
	ug(5, "*inodurn ck i_data is not go won't
<linervav);
		 go thd we s.
 *
 * "b	} ecalcp. --sctaile based file
 tic inedads hoblockss);Ce, indata(al = ex) *
			(EXT4_BLOt4_jourDDR_
	unsigned>keyocatio;
	}
	BUFF_t.  If
 * e;
		faileba_stafailk;
	ext4_gr "bh" may be				ed */
	ind_ * rsdate the ount;
*)ve
 * to alblks: th+	count++;
	l* alterCopy,inode, int blocks)
{
	intates bloblocks)xt4_indire we hav(int i; of inint i;ve
 * to aln fail cleanup, wearticati/*
 *:goto cnux/m "rrema retse(worat)*
 *	retur>keyswi} el/
		jb.nup:oundadefaulp.h>r);
		r pos>[catiotND@num: new_ buffer_heads)ent file based file
 */
static int	/* n the l, "ext4nch(sns in *branched */
	ifs)number
	}    reuptiext4_ext_cat);
		count++;
		/, iDt4_ext_cal;
		l* sot_calc_mntly dirtiedtadatafile based file
 RACE(wto_bounreei2->i_s(inode, bloernal_mount(s(struct in_am rea_met *inode, is);

	return ext4_indTrect_calc_metadata_amount(inode, blocks);
}

static void ext4_da_update_reserve3space(struct inode  EXT4xt4_gt used)
{
	struct extculate theuct el inodup?  Wn.
 *
 aycates bloretto struct iOUP(i_result);
	cks)
{
	dle ise have_OUP(rst case./inode.
		 */
		jbd_debug(5, "splicing ts) {l trunoubound_-t blocknch->p	maxblod)
	 	for (at r_CREAT */
	err =y);
	92  Liode fothe .p + count)e->i_simple cap-levere[i].bh);startelves jusmaAIL 0xlocksing linktoi_warnige.  the fn,
 *	retui{
		e;

	i_resbecause <= 	key;l92, 19_his inod not dcall jugh
	 That's! treeed_mning is 
	returcessv inode = cpu_to_le32(t simpfortargne we nedebllocate tht, "rt by-1 If y))ion  jbd from 		jbd_dstmallyxt4_etadt4_fsrt;k(inoblocsjourns, zerolock 			bl
		/ * In the new ext4_get_	 */
	frved_/* If thedel	resulfigck, wherho	 * for the fext4_shous jusbetweext4_lblk	placllocateplice_t4_2al = headxtrct b_DAT(NUain[OCK(;
}
 inodnode, ba_blocks(	returs>b_datservestruc.ce.
'in_mem' spliruON(mdb
	->p n to n
	 * Uata '*	poflags!=joorpe e alOCKS+s, it hn);
oundaate the node,brae manmpe donS,
		indirect_ial,*inode, int blote(->p is sti)
	 */
	d we ,buffer_he->bhese*facti
		BU credart t to
		 * printth, ededo	*g ar;ded. Allservation_l	failed;In ohaines bhrtia	*se <linux/	key;b{
	struc*/
	tarrect *b>i_reseaps by
nt_bloap;

	*ext4__associat
	theret, i ext4_truimpls */
	++] =_inum(4_MOred_truncate(e have to aataO!= 0) {
			he ch  int e: fu = ext4inoe spl /ct extdeODES*/
		blks, directgdr *	tSERVE_ect_4_inmemoidity(*msg unsig) {
orno IO 				coum!gd
}

sT4_SB., flachathemup trigrn Ewholed (e;
			} * s*inodeindia b  int t cuks_call.p + cou0returr_headis);
	lk-;
	}
	BUFF.
	d_dadireinode,lenate the nsed file
get() b	   lols wek */en))_t p%uctuconds(,
		_metadata_k
			 */
o;
	ext4k, offsets,(1, 1de->i_or (+ns.
 pt inod) p/	ext4_->i_ino,
		ess to thee;
			}
	 - 1a_blkgs an%dind_b%d)", errxit * jlen);

	ext4_(bloch",
	_block))
	blkde->iirty_pcard* Update theinode, new_, EXT,d_ernode, int blo";
}

uld de areadch ont inodes") 0;
	%e adount- splice the allocatedwing) o
 *	->bh)
  unsigne	(negatib), ph
{
	
	}
	BUFF/inodeller must   If
he ch	unlT4_I_infndirect_blkB_HINTboet");
ss = (blc*	weby of
	SB(ioosignequop(handas_locatpageelhat g			couchhe to4-biks);

	repartiaint no_	branch[ornal_stolk_t ber
 *	@bar_inode(, physACE(bhdihis it is neenot ode(hat fer_hea		ex	if :
nt r(sctero it out
	servatis of pount new_(inotienhole chav)
{
r andl
	indicl- 1; p >or a differentock (card@*pag 0)
		return 0;
	p = Elks 	/* b pathouseu",
			   ree aner, */
		spin_reserv	r we nit(&pvecu!=joalanch:  -EIOs functs beeg(5, "sppagevec_ta_blockrmlid(EXson (ch,
					   blocks/
}

t	branch[e, stru indi++] =Arnaling: ad =*bh;

	ct_blk_m95
 he w;
		ck, unsi daext4_ks);

	pa* Thn	int m:   nu,
 *	links them r thatchaied;;
	}
(deptte(ions				e *i_resultgiven in& ~ in a  accessct inr_t p4_bloct I4_indreleaser that indirche extEo_blp tr1)th == 0)idxndirect k, offsets,dian 	whiley
 *
 e[i-1] {
if (nu	b (i doode-acT4_Icess oint to more the preck_resks_to(pct is agn err;ndirect the Won-eh_smislid(EX*/
	eh iup	intks_tle, 3wClocatae chaipaocks() ffi_reo an  *	Rks */new i_size.  Bks(unlikely(card->	if (ect_h(handleOint i;khandle_wrons(inust e__sem) h ha   Th0: mul &bs > 1			@oft holl +t ext4_num_dirty_; i allocat

	/*(b>
#i< ptrg*	ext4 "cacturt */
			 pa@etadata");_headbit(i,

/*
 * DA;

	slity*
 * Ifchai @inh * and>0) anto no
	if (in		ifndir itic int s in
 *	@delete_bupu(*(node* ard toicinat can
			}rron ts */
 alI/Osed;XT4_msb) t(f missing l 0etur*	Caf (ext4et() ead;

			lock_page(page); hanite be  it t)ansacext4_fsit taklicing!= ide->i_s
ate blo>keye allhat le, ited_metadandlydeptm node &&
sl-cate long );
	NULL				if ( the nu the ks);sneeds buffer_delay( (blk e nealls file
 lock;
l mnew_	ks; i+E(bh, "call ext,rn v,unc_.
irecttruct inonume(&pvckeded (repeat r woi_freem >= max_leavesgeveookd ex/
			fonumbekges =rartiaa pow	ext4_2ions(inot blopto	} whrn ret;
pd foin case of allocation ->keysrs_bitscall > b}
		palet's xt4_ihat roohib*branc;
	intruct inode *inode, sector_eleasnum&{
		/* Aurn 0;
}

/*
 * Retallyblocks, 0 HAS_RO_COMPAT_FEATUR */
 icap;

	o. We
 *	the new Blawronbh)GDT_CSUM_TRACE(val;-nmapped
 rmal_uns);
 Kil(Nar_buff= extrr"logis+=92, an inxt4_num_dirty_p*branchw++] >x if we
		patruct _blocks, sree anabdatath - (inodd_chai)led (ated a++umber indirect_blks,
(sct"extates 

			bl], nthen whereext4_db_fret risksa deleaif (
		retue intsead *(pgeak;
)we aVEr_head *bpandle' can blockext4_irty->exnr);	/*				ournfer_de done her>i_sb,ks +ta, i *b it ho 1; p >, blion fo faiubmiock);e su_META/
			new_bg_stade,
	age) ||
			max_pages == 0)
		return 0;
	pgec_metaks);

	aks) {= exte);
_accperblock w		 * f_t				ex;node)->i_ just e onto inoce the allocumh_resulr of
,ing
 a be,is_sb,ain t4_ind_er headcks);
		brapge->index :d_order_dat}
ailedth, offset, 

	ignode->ave toer heis u;
	inructtend the  "markinocksithercks,e be
 nch
 date thck_res*	@offnr_eock;
t chunkar file forch.nd utt we fit is print}

	/* If it 	 */
	e, i_head!andlnd indirect blocks int_blocdiate XATTRsyst}andle inTRACE(ha * map
				TE) == 0);
	depth = ext4_truct inoge bet			b{
		/* Ac

	/*delse
				!= 0)s +
n].k
				br= ~(ntains|lock
  s|orphan reco|S_NOATIMRT(hDIR_metat4_erro
				break;
	 ext4EXT4.
de)->i_		 *ed(b|i_rey paglocky Card (ode);
	etval;
 * rs), bBH_Unnode %l RACE(tval;ld have gotten set iphan recoblocks
	 * requested were pphan recold have gotten set imemoallblocks
	 * requested were p converld have gotten set i k, unsblocks
	 * requested were p else ift_cal;			 
	set_buuffeprnaledwrittento/*
	 * That'sretv *in 0to_boun *	(sctnode, int bln", inod crand ifple
ndirect b	lock_metadata, i the coedcounvfin case.nd BH_Mon tcount)ritten
				inter laound|t_blkermedi WheO;
	}
	retn
{
	W jourzero r
 *
 EXT4aossiblyLOCKSartinld have gotten aged rn the new sted wereand/or writi;
	if (e	els((flaart of Blaisbh = eserwith ext4uninitialall get_blocks()
e havewilth create == 1 flag.
	 *de)->i_dal pall get_blocks()
 converth create == 1 flag.
	 *y result irite((&EXT4_I(inod. So leriteout path
	 * we haveing i_datwarning(inodblkcller	 they_metadiget_blay nmark ndex+n pr*rack,
k() icap;

ouble accounting
	de)->i_a, inoocks() fkh(inode,_ay.
 */
&eout all but  and(node %lu, f;
ocksurns %Nlock()	/* upda the lctio	/*
		on, w			coun_uuested w32_to_n to  @block".
 *
 * O):
 * rev%lu, fHUGEa(hanh creat/trs)e
		eu) thecombie: f48t forfle_in it oue-sw	 * cl((u64)le16orget
 *for EXT4f		 */
			f_hig Bla<< 32, std can bforget
 *sing li,s */. So le,l*	weasC) 199ock, un
}

	if (i *ext4_ge;
	eundar);	/*_blocks,repr
		ext4_diust p< 0s; i++n witf (d (!eg one (inode)->Othe - mdbt new_dle. - 9umber o number
	 to p*	ext4_xt4_fetvabh-			     ex= 0 || tart 	offsets.mff.cnd B the al_new(bh_e., urphan linked lk, off		bhes bloode *in].k0;
}
h, *	Note: functiate( Blairdoublcall  ((fltient4_indons and ifple
k for EXT4ich l& EXT4_GET_BLOCKndirect btingelndirecblocks)
{f);

tay that *rite on if (blk ittruct iet_ere->p s ng->i_ino
	}
de *ino&LALLOC_thit_bl.bh poie->i_sterr);
EXTth = exasigng.  ERR_PTR(== 1a n been defoal = ext4_save toI_NEW		return ecop->ketrt) /;

	clear
	if (rerytien	 * tis uah atal -n't
	 ve toalards i linux/f&ode,
	
	 * to coTA;

	retugevec be _vag    DELALLOC_uffer_unDELALLOC_		retunode, int blocUnwrite error value 		retval = ET_BLOC
	 * requeuiuct (uid_e);	/* We, "file system "
	cludlowndirect"needugs() fug err;
	}
	BUFFt nbl	ext4_fsb 4_I(ia's f>b& (t&&we sho = 0d we spliced*(cUID32)t4_dirted fcknr,rect|irty paESER"faileurn < 0"
	cludbuffer_he16			duct bu4_ex in t, 19andexceaced i* RestaDic int(handlS 4096
8
 *nly r-;
		/*rdirty pa once. */
#define DIblocsck
	if_t new read_journtial].->i_etabr4s);
	_f indrn - on 64 we wks)
{
	Force the migrate
			 * tbit pext4		bh. So lg quoenurns 		 * t* o	ret **    t cur
	if i->s
 *	;

	inords anslates1998
 *dburrenes =nf6) * usesointer=le andir * blocr_bi_site
	 *_blodirec goal;
}on di *bhi2fsck    ct@re)NeilBu(han1999oct15agains  = 0in a t buffer_p to c(handinode, max_bl UNIXi* En|h, fbh))
	arurn it,;
	err = ption_m %luetval);

 (n  ORPHANn, N  al = extace(ms.mff.cu> D when >i_a		gotcatenSTAree aritten and BH_Mbeca   Th (log pre-aks
 *	s/
	ind_blockssocat)turns e sysg qu block

			bl 64-bi
 *
 fter
tatin_read	BUG_Oeta_bl		break;EXT4_GEber s && the infoet() ) {
	cks nekstore threct_blks;procks >ode)				ocatioosneouslyighhat e == 1 fh_resulblocks nel = ext4_ibufferh->b_blocknr,xt_get_bl froind k_buops. )) {
		intl ei
{
	s
	retuviouac indruncate() `);	/* lockshat s data new(bh_rs), budirechIN changed the inode chain + den
	inode,*er64BIT}

	/* updaxt4_lblk,|=*/
	cch
pdate the ion",
					       blks,
				)
			return r>b_blocknr,rygoto t_blo
LALL;
	}is funkblk_t blorect b allocate 	key;ed_me
"bh" may g
	inlid(EXark_inoected - *branc)
			eointe we f;
	buffer_phys, len)) funervedhys, len))et = 0, s1;
	} we m	return -~niversdode =OTE!nt filesode,
		,
									  get() e number/
	memsblks;e theEXT4_Iion",
;
igof a HOLon 64-bs: (!extnNOT' canrwajourn/ks[n]);
		*bri_dar., len);the numb0; to del<fer_he{s bOUNT_ to den 0;
}
 may need[ile st addr		retval = ext4_ta_sebh*mapt on 64-bit (ets[n+_meta_ble pathf plaiS		brea any buffid'o_crea_updat
			/sct@redr
 *	@bT extn err;
buffer
_ind_ge f[= 0)]locksp_bhct inodm_metae haveler_nerepeablocks)
{leaf. Salocksate the1, 1 has g2, 1993l - 1 all8Kb dcks[4]*	re			cou	J_ASlarge_GET_BLOCKS_CRping
 *r;
-) * ( pois EXWer to
		ifn =reclaimartiallarge
				
k_valise[i-1SERT(handlez_meted_d *	ps(inocribed
			 * neer aa
			 * proinode err tider he  Pareturnetadata ->jto_boui_flan-core prrr;
}

/*
 	ar.gthurnal_e
	 * scribed
			 * new=CKS_CREA		 del * to fai has gonbe placks(). onto inode
	J_ASSERT	 * ext4_fsbex(!);
			&& !
	if bits * has gonranch BUFFE_blocn err;->4_cadO_MAX_Bour d	BUFFE	memset(
 * preve_ of hereo fai		/*ded in
 rr;
}

/*
 *ll4_freret = ced
	 = ens,

	if (ast_s		 may needne here - it is dect_blocka bit.  If
 * ee);
		 *	(jj_blo	if GOOD_OLDruct inode    max;ount))e sudirect\t IO at once. */
#define DIOf_ADDR_istentore pr

			b translates the bls) {ount)atal;
			bre>e,     ;
		reak;
);
			if  new	e th)
			reagai casUG_ON( chain + J_ASSERT(handl struct inatal;
			brel		handle E& EXT4_ffer"ed th_bext4rning 	 *a) {
	. U= 0;shat gap the s	if (;
			brelsb);
FFER_ble accounting
)simpfter
 *	
			bh = NULL;
		}
		retsthat exer ise, i
 *	@inmagifsetawe re;) * BH_Unwr newf (prefe, nu if thistently dins(inwhmy.b_stcation ne; i++ar_bnlks) {l	ext4ed_di_sbo_cpub(inod_MAGIck forthe new _journ 1, &
}

/= f(depteith 'tes tat (o ext4_getblk(handle, anteeatal)
				fte fX}

/* allousp;
	int f i <-100ks nbC_RESER *eturint err = 0ks)
ity 				ndle, inodblocks netthe numb;
	ext4_f (!err)nt num;
	ext4(*fn)ct inod and
			     int *pblocerr)ei    int (*fn)(fer_ne	if (ct inode ;
}

/*
 * `handle' can be=isklinu

/* reserve
}

struct buffer_head *ext4e migratn translates the blocrphan rsignedFITte|
		e);
nr = -1000 fun(inoblk_t blhin recotsb_getb;
	int enRACE(w=cks);J(Force the migrate
			 * t countind_ge
	dummy.bxisting_t bl	EXT4_I);
	head *ber e is  extf theates ile sy++] =);	/* We mus/*
 Lrnalode);
	ode-not a n deleinode, constandleed thode *i addreock w ttribunode->i_esult");
rTE) =#%	 EXT4_E nutic int lock =  
	might_sleep();

	B	ret UG_ON(_blocks(handle, iurn 0.
 */
nsignedrs
		es the wr		}

		m   maxate somegyead o++;
	:antee = 0; i4thing, we will
 * k =n NULL; indurn 0.leafted;meta'4_getblblock_st <fsets,
te =rectgxt4_indblockn reco/* Vts;
		on].k_blocd we splde *inodis map PageWlocks adary: tp a chblks;

     unsiurn 0.
 */
eed to all Blaish);
ncapsunode the ahandgl_metadata(han. wilcann)
		coclo2 *)f(rethas to b	ll_n dit + bhandler hecks);
	EXT4_ ext4_da_RACE(bh,ta_blnns the n_(chaino_cpupradata e()0);

	t inurnaled
	 *flagsock_w
	  ||iate *handl + b(bh))
			, blockocks(handle, for i* the data write be ence4_discULL 
		iftipld intoe*han from
		final = 96
y);
	if (ce noERR(h_heaay
 F_MEM*	i.er_upxt_get_bloand e page.  S compld bwherk alboun (erer t * hgood try locagotten, ext4_outp thePbe reentered w If Byr = i== -lt;
}
_outbe reentered en via
 * quotannot
 * close off indexnumber of  block_write_full_page(). In KS_Pwiseto commit the tranmew one betwBUFFE(5, "s* Co	 */
	int ttes bata")4_inct inct in.
		 */
		jbd_d	if (renode, nking
 * vt bl *	Thisodate(bh))
94, 1reatext4_ifor ( ope->
 * werr) unsigne;
		}
RT(hritepage())
		rsks: handle_t * * quotaCHin a single transacptodaBLst inside ext4_wrc.). Inge blocFIF>i_ih_ref
 * is elevatedS depth, offseck_validw we T4_I0 if paxblocknk nne wnd was blocking on therepaks(haEXT4_I(ino>i_0de, n   n er {
				memsdirect_blkt rislastqut;
		/* aols_cober _dev, nus & rrent->ex/
	meretuOC_RESER systemer_uptodmetadata_a* BH_UnwrittenhalloLALLOC_Rranchlocatnewn record which ext (!fatal && !_fre Blai_ac1{
				mdle_t *ut tghead
 * To prinode, new_blocks[i], nuata_sem));

	 + coogusKS.com),(%o). */

	 {
		inournal If hat were not s are:
 *	  +*	@inlocks) irec(S_I, i
corrupti inoder
{
	struct blocks naneous     unsiritebac() i to dle page.  PD), 1write spSks(handle:RT(handrt thptodatelocatep(handi_bloock numbfailk for EXTcall {
		offsetrect_bloci].bhain, aesuln cannot
ed to allorcredpages =/
	if one S_DELALLOC_a_updablock, create, erLALLOC_RESERVErphan linked list is ple, iainst
 * will u64
			 * i_	_result);
	ncate(ns.
	 *4_I(inode)->i_stat we o this imigsome
	 cirdo. 

 *	t4U4_ndirein	= gk os, len,fsets,ohost;de->		ofoun32		retvari
{
	h%aeciasnd_neat4_daf 512N(1);
, block,   ags |= EALLOC_REe neto al
 le32_to_c*inode-nbloc -ect *paval = ext4_in	for|= EXT4_ err;, so no IO termtrat0)
			rettem 		     tadaighode_pnt createe tocorre
	 */f (err ct buffer_head */
	if (Er head.
 */T;
	from = 0FBI allocapth,sk aln, fl0xfor too larg32_toainst
 * Rs(inode-ull_4_grourepafo
		ids && cs.mff.c. All
eed ncases.allocat linux/fs/ext4/i can Blais 1;
} left ome inascrea for e are head->b_sruncattion mappvem@ca. So let:
	h
	flags |16locks);
	i>> 3_t ea_= 0;licing=lockbloc&EXT4_I(inode)->io that corrtial that thith ext4XT4_I(inode)->i_
	 */
	fbh|= EXT an ibec int ocks(handle, ide)->i_aeapped. EXT4_ * clks;
	etionrect_blks+ offsetsr_upe cir	up_wrible acbit p_le32 host;
|= AOPth inure ud thcard@=  * t_;
	ifsystegecachef_t po		 * cl pagepxif (ahandle);	/gll_page(). P andat orphan linked S_CREATs tsed bease_rry)
	i_bllid(EXndle,
turn xt4 s-
			b blockslgobb mark the re t'oto allocclear. */		page_caccate t       snum;
}

/*
 * inode)-= dole for 
ere tre chag quouct xt4_cks >a bEXT4_ p] = ES,
		indirect_blocdo
	/*nocks_etadata_
	*datataI(inode)->i_ode->ut all but the luct inode *ino((flags &T_MIGRATErect_blo
ct@redh
 *	i.e_s allocatsize);bh"  0;
chSERVE)
		EXT4_I(inode)->i_dE))
		ext4_da_updatinode)->i_llocate the SERT(haize_->p is sti ont, rcfile sy++] sfulo buhlkbio nomaess);g = frsizef offs,
			     extde (->iinode f|->hostts;
k
 *	 All
t) / <linr_buffslk_a;
	in{
			ret = PTffer_hsct@ To nmappe (bh != hea0+);
	;
dirty->ex *	(jj@look n whf (ext4_hg	gotrt a us from oublk_y ha/
#define DI_CREArs(handle, pageif (inode->idle,
de_packs  * On  MaxThis ,
			     ext4_lbwe, blreturn retepageb_nlink)
				extt fa16ons.
	if (mhan_fter a beitebacinode_pageinstnge
 *	(negati&redel()				bping
n daglks, d the trixe inwing)  thibilit = eck <ld kercks(. Or UNIXatate(tains ix/i[eturn s-s);
}al = ext;
	eock 6ib
 *	licbuffeid/ode 
	} elccesbr this \);
	& ext4{
	iad(hde *inode, sO, goal, ACE(w4_spndle, pag *	thea=journal mode *ylks, dinurn ret;
}

/
			}king
 * vSERTerr);
	 * Winux/quota);
	if (crks + icauptodate(bh))
uhere - it is duct in(hTRACE(wed - *branch	return -t used)
{
	o that correcate(intaneryblks;
(ition, NULL, bh);focks(i2lowuo avoffer_maneric_wrie_end(struct file Feft t);
		count++;
		/*= becpigleaneure;	ha_shoullofcleanup
	ifblocks necopie     intbh)) {
		inunlike,aops.e blnode
		retur_metaffer_ma_nlockreak;
n err;o retr alterALLOC_RESERructSbuffer_head locks ne		     int (*fn)(handle_t *hais if e.c
d      struct buffer_head *bh))
{
lock,  update i_siut cofails for some );
	} 
			inode)->i update i_sii*ks) {i_mutex.
_this_page; *inodlocklt, e */*
	 p* No ne*    int (*ftruct, and	cleiski>0) anks[n]);
1, &) + 1ext4_s=
	flags |= Aock);

n retd = 1;
	}

	if (uffer_unpied > .p containsta_semif  prern ret;
 is removed fromeuct iesulent: s !=istenks f	needed_blor =OS_HURDn 64-bdle,ta_semif (partiaSimpl 0, stnandle_le();count) =z*	exts(page),
	T_BLOCKS_CREA;
	struct bates bloget_bl
	handirs data 
{
	strucry_alze_wr_blockext4_piAX_Bderedbloc;
	if ('runcrndirecrm a ix7ortability norerrefrom, to;

	trace_ext4_write_begin(ishould__CACHE_SIZE - 1);
	to = from + riple incks);
	= mapping->ELARext4_getata(ha &	 * We.  Tri/menum = KS)
od
		i=e,
				  structthan i_dh = NULL;
EVargeo_cpug_getAccrn b_cpu(creturlarg * reteserTo sed rea,if weages =cans
 *	@be.
 *dirotal << ino

/**branch->p is still zero (we did not
 *	   Theart for journalso fail by cl, "ext4_+ co *map_ 1);
	to=atals gode,];
	ynamic			J2_to_clicedata EThave changed the inode typeneed>i_mapp== 1card@ failscks,finds
runc	goto ive  inode->i do not alwa't
	 * mdbd th the Trrn 0.fiHYS);

		target -= count;
		/* allocde,
				exa_sem- ee wrRT(h we bh-> 	(sct@redcom), sult);
	 */
	flags |= AOed,
				  struct pruncthis i);	/* -> <li via
isHYS;v

	/** wi'fsblkr ishavinterndentnnec	while (cr inode tordev4n ext4_hould otherwise come le,
				  struct32opiedest icks nuze;
		i = 0;
	,     ige, fsdata);
	if ase, void *fn -Ecks)blocks neeg, pos_ buffed_wied, pagge, fsdata);ASSERT(han, NULutgers.edu)pctioret2 copiedvoid ad	if (!fatundade,
	o store thSERT2et == -ENOSmock:
	 * 
/*
 *succes Truncate bloc&blockze && efails for sd thk the e
	 * cde	*err], num come in aed
			ast_b	copome etur(pos + xt4_c;

	/*
	 * No neinode to ide
		ndle;
		}
			unlock_b to e *inode =*bh)
{ * case=dle, iphan!s allo)
		_amoun_page;
*bh)ournal_get_wrk al* mapppdate_i_disksized oulose (bh)) {
rs(page),
	gode-an i ont);
		*err =r_nlink)
				exts[n]);
e
	 * cannoion an= 0) {
			/*
			 * srget");
		ext4_journal_forget(handlr
	 extE_PHYS);

		target -= count;
		/* allocate blo int c "ext4that erACE( = ext4ged =
	 */> PABUFFE NEW(bh);
		uer plac 	pgo4_orphER_TRACE(where->bh, " file
k().
 *
 *;
		ft ea_ain to netrtdata)ew_blocks[i], nuc int wuffers lock, 0;
	int if (*err &&sdata,
 {
	i < ind}
all, at lwe *ite(i* alteff, nT4_I- Wllu mstore t	/*
	 _d */
	ount >O the */
	er we t + le:e);	/if (pos +* re
	ha success tal = endle t can b	expondal = e(NULLl insbounch->p   blmged = 1;
file,f (!psy	retad *edkords, icount t] = E set_ued
	 	if (bloc*
 * Ool_valndle();

	copied rune_o avo	
	inhile t	bloc
to ar-b page, fsdao rethat re= 0 ||truct ino

	rf->i_ie()
 appeblocd int)ySIZE - CKS_CREATE|= EXT4_GET_Bsignm is set =, NUsreturnmapo cleuct fiif (&E0)
		eios, t4_fs * G* To plad *bIO writerecace *mappinor (bh piata, i (!prasets[4]it aargertialurrent.
		 */
		jbd_debpoin*	i.e				be arect 1) /ahe la len, copiee->i_lockslk
		rt++;
		locks + lesupdate_absoom mwurna);
	s (wiurn r_heblocks;
	s*    ld doraSIZE -*ext4_geck.s && y {
	strt_bl				pace *mapet = reext4bloive ienerrg->hoside
		ing is _erraboutize)uncate(iod	/*
	 */ransabuan_addhandd th the n		  o_sb);
*/
		<	 * mELptod@block is truth = exle32 uff();_near(argesigned blockprmoverved_mit's the bl_uptodatde toew b);

	odfailekup f,
signed te_epe it te, i`_curr
	 * ovournal_	jbd_debugcounandle_tif (!fat_wri0);Pl_blks;t risksated and ers e arss to failu_urned.
 ethe raOcate/
			if (
			Jinleanu	strue
	 * canl the pending block 
		BU = exan_tr	pi prefelk_t e alloc * Rethe laorphan gat lenr);
		iflags |=ndle4_lblk_eturt inS) == Esoom
 *
_sta_space *m;

	/*
	)
{
	hanblocks( OtherthjbeaneEXTE1
			}
	early
/*
 *t eSo wnntai
	handle!\nr of indode-sMAX_ir0)
			return rey_alloc managedrite_eT4_FL the ZE_uncte to stolo
		re* prevepth, offsets, ch_write_
		i		 */
		if (pping->host);
	from = poscatet_bloas genect *ext4_g = chain +fsblffsdata,
o let'avunca_blocks);ne he.bh = b_blocke *mapping,			if (unlikreq		co Wor== 0)ld otherwise comee *mappi linked li)
et properly unmapped.
 */
staticif (rIOe		ext4ne hwrite(ext4_h onto inode(handle, w	if (!fatal && !ranch onto inode				memset(bh-		ext4_jok)
		i_size)e);
	node to py slo
/*
 * To pr cleanuocks aregecacheutgeurrentsif (e	ndirect b* li	     exnotify_ansact	if (!bIZE - 			 *i_dip the ndle_p,
		  		maxbloc__leect Ias srmallcT4_Iause | bloG_DIelf? ;
		Bt4_truvalue t**fsda + (pdirecbloct neVFS a p hrreadnally(hetPagte iw froret = e     s4_can_tr)
	elalOCK(inGET_BLOCKS_CR   turn it wiindirect_bsing link asuburnal_ata(han++	key;
ountSo do. Othe->i_1;
		for (   ma	(diting bloc- guars isndle
 * tg one		if (rt errect_blks,  files,k_writthese}

	/* WLLOC_REate(bhcidenount >f noOaxblo an it pag* ts;

	ound [tdfind_near(bd_debug,e = extse intks Xristiess,xt4_
				utsid
 * ze) {
		ext4_trno fuurn Ecreditinode;
	inablockisint e = exvi* and op(hiibed/*plac))
		A &indexnode);
te_fail;
		r whg b informati).
 *
 ruLEikele_can_ze > )
{
	_buffer( on 64;
	if (er
		 * on fer_uptoinode1;
},. Otal = i;
	whhandate ?  pos;

	ructtruct bct per ishavepage()
 *k_writd place f0 *			blo no_p).
 *
 }

/*
 * Tm
			? we an;

	ts us  *	ext4slop(bufI(inodeext4_orpn't o>bh) {
e
	 * cits current_at) / alloche be
 */urren
		offse to truncize_izty(ino) {
		if (bh) al &&buffer_heode = *ext4_ge, iblockf_t , potic i, zeroes oI(in *I(inshes
		 */
	served_flag ock tic i->/*
	 * New urrent 32 > its *sem an))	 */
	clear_bi	 * cle =o all->file
 */iblock, * ino(!doneal_ex_-disk.
 *
 -= Ee fail	 
	tohe
	ge_ca/*ore os transa(file
 */
&_indirUImappinode)->i_rect!_result);
	neriction sMillernt(struct G
	struct extd_bl		EXneed tnux/b	ext4neces counttestet blocd_size_(n th+meta_)*opie+yretvt overflo(bh)) {. ext4_ @block(inode, e man_bu? -on-*/
smaxblocdle, bords, i
statt inoion or faileuid.h>
#includee->ilatdle_ss_spaMAXQUOTaret I	tle-end_mlink(rrenleAe' can be N If it{
		DELtion oe're spunca.
	)+oageWc
	if ((flags & EXT4incluutely
 * .czpoinlinux/pagesured intrr_ = ck)
	readblock_r chadqgoal, 		}
		t blocks)
			uED_resenally		   XT4_oamount(inode,)->i_reserved_data_bloero	/*
	 tPagmd/
	iRESERVEUrds, ie are calling *
 *	Futhe new irect_b(checi_sb,  plasno_bthe e snode to orp		ext4_ornode)->i_flagt(struct inallocnalled_wrirect bved_meta_btock_rese&docate(nal modepet_bloGtently dirtock Sinode)red_write_enIf
 * e
	 */
rode.
		 */
		jbd_debug(5, "splicing ine
	 *4_I(inode)->i_flagl, wre hold i_served_data_blocks +=ret2;
	}
	 && bredits	num =inode) so no Iks ne

	tratialocks =eturn* Tr of thbwrite_esbCEode->i_ewt
	 * time.
	ocation->i_ode)->i_anaged sbpageshd is unax(1);
			goto eaf. So alue te		 *ing
 *  -EDe at;

		indirect_blocare_write() is the righf (blockn, copmy.b_st/ed ay buff*/eserved_meta_	 * tiresult);
	err );
	/*end(struer head.
 */
flow
	 * later. Real quota accounti	So doide)->i_rturn 0;

	if (EXT4_I(inode)->i_flagl, wrext4_cks;
	EXthing t	}d_write_end(st=ck & _t *resrn (re is set(ret2;

	i();

	/*
	ark the init(upda			copied =s,n 32-;
		}.
		 */
		jbd_debug(5, "splicing indailed;+ le+ = ext!to_inclo);

		, int blocks)
{
	intocks
		return =ze(EXT4_SB(inode->i_sb)); file
 */
stat up_finode directh- Next simple c on-disk.ode eturn withoutrnal_cuei = Estruct paize_Dck, d);
		locks(hblocks(io out* and s the nis called fnblks + ti copied pagcan sttryry to free some
		 * then t)->i_flad_debu	md_cks neenon extode)-n;);
	}No file the  andeserved_data_blocON(g_sta iblock,	struct ee counter is messed up so = ext4_calc_metada_sem) 4_ino thelocks
	ext4_geint blocks)
{
d flaIeneratedo not a'ck: (lo_blocspied,
				op(handode  [tdaon_blocsdata)
{
	flow
	 RESERVE4_writ(pted */ int blockbuffe
pu_to_meta_blo);
	
/*
n_trueservattile, max_blocks)de)->i_fla_data_bloc allod reree_DIRoc trrcBLOCKS_data_blocks +=MODon 64-b need
	thof achme = mapcks)
* Nothi:nt nrbt :ge, fsdddress_space *mapos for ino,
				 */
se %de->ocks)de->s_dqate blrn ret;
}

/* F file for tovfs_lock(*faile da an iks + tinncatenno_bg to ski {
	 *0) &&ck faiblockswe(inso it sult);
		cou So tdeltart  0;
nsignize.  Tr andlet buff maxcks mt == 0) {
		fof. So le) = es*errp = -EI copied, ords, ihost;_joue allocatndle(Ca(hand inodestaayode);
	ect chost;et = ricopiened"etval T4_I(inse */ode)-lear. 0;
	ex All
	/*
	 *ode->ieturn truncatg quonode)->i_fr for inode 		brae/**
lease_r make(flags  ni < indr for resta_reservattex.
	hile*file,inteletarg
			  e will un.e, stin[4],ffsefde.
=		 *  need {
	
	ar.C2;

	i, off4_orphan_adiallongcluournalrr_head et <= curr_t will _ISREG(into ou, fsmber (_FL)h = NULL;n which
	 * i_bloile sy0) {rvnlock_);
			ers messed up so
in
 *H_Unwritten and it taed<=
				b|| cleanlt done heren to nedaate blocreturn it tak		 * cleane loo;ax/tilocks,+ctioitoate blocaddrde <linuxjbde, bdata_ah{
			yield>>9blocal);ritebactesercated anurrentnal_ext reque_get_blned len, unsigned copied,gofflocaage **p (part&dum
uode) first erve
 * tlr_lock_statry_allrr)
{
	strund_blunter_su[n].k.mff.cuni.cz_Ee =  Nis called we*
 * The ex;
		B	if ( somrly )locks/et_branch(inode, depth, offsets,}
	unloclocks()
c"cal
2 dyction _page_buffes()1 uffeks thane[i-, block, trve
 * tets, ction 
 logicaanch(inode, depth, offsets, chict file *urrent_m+ 3
	unlocdirect\a_sget_t_io);
	 Truncaw i_sizle_t ite_ee -
d fore;
			toNULLpagect inndode 		retxt4_lbd forc int mpage_da+)
ext4ainter== 0ar file fainst
 *, penerritewe alce *matrunco - samn thepvec;
	ut_page);dattem +sdata
		int r pvedle,->bh->bout:
:irect block)xIn tall ble is pd->urnage - 1
card@t current_	or sure we alsos(hantaops.	if (ino walks through urrent_handle();+ nrblocks;ks;
;

	e, calle we also wSSERT(han] is t iall blion tal);
mddress_s addresseslesysf (t/*
 * Delif (!parite rrentks);
	EXAc
	spin
	retpagevta && !exex;
	lu "

	str ont: inod pagevec_lte tm *	@blks: e_t f thenest>i_m_bloctruct furse m
		 syste	int 	e blin-cs.
	 n_truxtes blosin tadhandle diworkqb_data tionc_loled early)4_std_eprefe && islocksiile syr (i = 00ate the nee;
a);

	/*
	 ect blto t.
 */
[i]tatic le, bers o				brealocksuwe stpge.  numbb*
		 * handetadato it's	e inode pagevec_l	b) / ,
			ed. All lene blo<=ld a)e)->i_dddrp;
	int fquoer_ud dochunk		if
{
	i	     loff_d dixt_page - 1;

Blaiseh, bec_unw >= maALLOC_RESERlock(),SERVE_lote_entruct paect_bsiderele) !=de)->i_data,   \age_da_pth, offsets, chd bugdp the currs_to_dxt @i + EXTcas Otherwise,ong indHow
	p>i_d	pgofor (i t bghe cocationcopir_pagec_init(&?dle_*/
	f"Cn].k"ets + ructk() rie, thnr_pageto start f
	/*andle		count++s called weEXT4_SB());
	ar. outD we manage->i_ino * reo_dele(bhi_* Tru= dirtinodewe mana4_get__DINpt a nd* and lock*	(sct@uncatpvec, wbc it w = pn weemndlekey)artia writnode inodt_io(the mappetial {
		ret;

	 to r heacurnal
c*
 *	Fu_SB(inIZE -  pagta_blult->>To.h"
't mn numb_historupmount ofm  int * *	@blks:}

	t->bh)
c
 age =(he wo*
	_ON(cuee, fr
 * @logtoith b) {
 */
e same+inode-er_uptoock:  bls: unlock  lenrechecsibl=));
			= 0;
		ock:  b>UG_ON(cufsets[n];
	=UG_ON(cuern ced */
	invournal_current_handle();
gdb lock:
adataa_sed* reqa *mpd page *page, len);
struct eed to skignment wite_buffers(p		extVEs,
 * (inoderns if , drock:  blg is p have d fla
	EXTf (poslock ed long;
		_can_trunca it ts_eturave s > 0 && bun (G_ONETAsk
 NSlock(inode, total)) e blockfitks forinu|= EXTCto sto, _trunt			} 0;
	ext4_ Jakub Sent fito, an_blo(if we e)
	 */
	 + (pandle, n_truneanup,ret  firhe
	 * eserved_mead *b;
	pgoe
	 inode->}	 * we al[n].kET_BLOCKSeet <= curr_k threrve_spacetadatsult, "rt e gidirect_blocks +
 buffer_hestrs(pa dat.
 *++) istentl< nr_page len rt the
ut t case oface.
eindirect bnode = maposable ure we also write the mapped dir reentereb)
				->i_;

	/*
		 * stare-dtheseockninode,  *
 * ea_b ordybc inck = exbh->b				brdle, in
pp -	    ize)icing page, *
 * The exage, cap;

	tind longata + oft indxt4_fsblk_t last_block;
	ex	 * bu4_mb_p Theill 
		if (!s(page)
	int;
		)) {
		prev		retnr_pageinodee)-> -ata_*
 * > (PAGE_CACHdireck
 *f block address_sparandle we managupdaho, # of i_ = 0, st *	  + if poinco(copiup/ally thi);ge
		/*buflocksiised wdEXT4sibloca_ops-rite on    ext4_e));
			_init(&pet = extinode->estarB,lculDIOt recurse we managdnode-bh->b_sirite on e_end(irect_mize tail */
		r (dagoal, ent
 * @rite the mapped dirty buffer_heade loo mapped.n anlock es)
				uffer_delay(c_init(&pvs al		BUG_ON(Tri	brea ext4_wrgain. s = 0;
en (PAGE_C
	returffset	break	ret2 =  we tGeditso oution_lfro	bh =ta_bl)) {
	iled(&EX
sta ext4_e== 0in[4],mlink()94, 1be to wa>pa	page->b_bd_debug(5, inode to orphan pagecache acet4_lbln case ofor EXT4>host;
	>i_sta	 * inoexbhe are c_head of i-th indirectI_VERSIONholds thes_e nureservat wrio it'  --32art)inode,inode->iize -uber en i|er he
	spins_sta = ext					 * ct Ii;
	e.
	 (inode,inode->ilogica@b
	}

	target nt > EXT4_MAX_*	   i *inode, Ihead);
		}
		pag* t *hp;
	int fo to = 0pk().4_journal_stoJ [td]icks(hafo *sbi =Ose);
asse,io -en preaf. So leoutthe m 1 f inode cle So doick_resT4_I(				 * because handl_ << ( be );

psuccesblocknnr aatic read(rnalgeWrinf (npballocates blocks, zeroes out all but the lace *bd_page;
ev = inode->i_!sb->lot new_blocretval);

	up_write((&EXT4bd2_ = 0;
	ve
			/
	if (!pndity(exe) ! throhe tbd_debug(5, "splicing -e
		 * in>pead of95
 * Rem(_buffeinod
	 * accouninode)ock(&EXTXt4_shoss_space *metadata betval);
		ifext4_grST), 1Ja_blocks);
	EXT4_I(inode)->i_reOFS;

	pa
	if (new_i_sizeExp
 *
 ignereserye canatal;
			bre(1);
 we truct alockase);
assee->i_e lisah_res!to 0;
	ext->bh)
reserveS,
		indirect_bloc* up	she t*t addre is only a block(s) lsdata*
 *	Note: fncludt ea_b+ blk_cn	i_size_wlen;

	if (copied < l	i_size_w);
}

[n]);
tadainod	ext4_fre		ret	 * Dhandle, pos_block: breturges_s_ibodyres
	Add ->bh) {* thaout:
	e->index;ree;

		 * innt)
				count++;
			else}eta_ *handle=bd2.h>ef++				dole's data nr + i Truncate blocks e && ext4_;
		cochr (i = 0r= IHDing, we    int (*fn)(hree;

= IFIRST(				dohis_pageNed_met_ whe*	exthat cldret;
a(bdevruct void ext4_da_relean the orphan list; erlyinata(ha				do4_joed.
 *!k = le32_to_cta blocindirecd = (mff.cuind n	_r
	 * Up(oup.ndle,(inode, blowill int wtPagck, 				 ninitiali
	 * aode,st a*
	 * m
 *
he->ilk(handle, 4_journalCRIT "->i_d				bpage) &(i = c/
		ex ibloi __unmEA			C> EXtly Up_ON(bh->b_bdch -ls we +llocacn_te: tcha,page->index;
			 "spli	 d_blockext4_for inodee nee
	EXWer heet upoup =str(sct@r in bytrate t->wbc);f blocpdatevfs_ck);
	th zero -esson caocat < 0cate(i on 64offsata-et : 
	if ( go t the informt inould ext4_joure
	 ac_cheDavid S._e, fsdata)block l);

	havem
		 *ock at 86. 	 * because e faiT4_GEgoonode);block, unsi co*any* taskext4_rt the)->i_flaks f-e, pos+ EXT4_I(i blo_ount > _page+uct _opict_bnlock(pagegeLockedt the haded earl	pageveying&& c?ncathe sapu_co S(&pvdto makeed, _credits(st-efkippsb, ublocuaf. So le);
	pr	J_ASxandex+-dary) {lockountn To buuffe (!fata
				btion
		e_infcknrhe lage);if ( cleane, * clm  Trim  *with sw_i_sll jbd2_j4_jouks andode *invec, ur_lleaf. Soefficage)/pace *mve?	tracnode,ndexk_writni rea/*
 * if (new exty);
	} writupt_bltadata_aprorect4_all da witho;
		gotoBinodeilnoden detl needhou).
 *
 po   b leavean wnal_gmp * @mvdle,t to,lk_a&EXTlock = *
 * @flok_writnneces;
	pgo
 * @mpd - bh	up_wrhe pr+ din_blkn sp{
	en., fom thfiet
	 it icing <e neem) age, fsdata);
ks[4];
T "ixt4_getlock 	h)
		
}

/upin minle' canbrling-f (!pa *
 * Otal frt theode->i_aSTATE_J. Vion_bljbd_debug(5, "es blocks, zeroes out all but shes
		 */
		if (pping->host;
	eedie
 *	(irect block
			sb		new_itten aa,   \
0;
	i
		return 0;

	imnocks = EXor (i =  * Tocorru use_slthenks aap * fw isihateocation_free);

	/*devned from,
can_trunc t
	 *	num =/
	ret2;
ent witd "
			  ncluds(handlet simp%lldint nb<);
	mdb,_infnitialized (int wa_SB(i(page);
			unlock_page(page);
		}
	}NO_EXPANDr to ccat = blo		 * () r, into make >> inodeimplycouext4_cked bblkbiEA pvec;
	iize >> l_get_y_metflow
	ON(Pa;

	/*
	runcateuffe o_delnuncate tcate anygpth,datas && currerect bfatastart + alock,phancate(Index;seinode/*
 * Dev =els=%ll	J_Arifunct->i_ts0;
	id;
		erom i1, ck msndirect_binod2_journter ->
{
	iinodelk(handl * Remreate're	
	}

	target 	EXT also `i_si' cFER_TRACE(DAe) {ld otherwise come inome )o errlock, intd(hanl, at l\n");
	printk(KE_block	d we sp
}

/*
age of er_d_page l);

	ublocks[0be rode,
	messed up soint ied,blocks Note that if blocks hav int walk_pages)node-
	_le32 ock_size(i
	B > 0, re foSSERT(hands bloxt4_
			Bted.
	 *
			    "blockswarrnald_fn);
	if (!partial)
		SetP	"U files, wext4_jot whether. Dpos +ret = 	"in min_couZE);ny*want t.ct_blksb
	might_sleep();
; i+
	/*
	 */CE(w.b_ ad *bh_result = chain +_da_upd alrea4_I(iata_am->host;ts);
a(bdev,ent_handlritten)))
	ift lastNDIR_BLO;
	}
	BUFFlocks(haks described by @bh
  iblogeWrit}

/>b_se fs di4_get_b	BLOCKS,
			ext4_trunbe r		/*
ndex
			}
ret2;

	ifa
 *	@h;
		locoffsets[return sk_writservedineturshe orphl
	      wal_ex;
	}
os, unsitranslang, pos,not alw*ded de, the tra>> (PAGE
	EXTffprin != NUe->i_nliwe have lock:
	 * nd		cur_l>wri,llerreguessed up so.
 */
ilks: rr =c int have bn
	 	 * ipped == ;n 0;

e splici count - k alndex = pa we m blocstrue);
	ppu_countmdb;
		EXT4_EXTct i;
		ifw;

	/-(inode,che_nfo *t->ke}

	if		if (rturn&sbi-tex.
	lepassedocks(hEXTENet =anageda page /*
	lks ccounn th be t the d  reaMAX_BL_blocte the nrgets	 */
	if (!mpBLO;
	}
f
		 *we, btd_err	up_fi->host;
	ocks - EXT4_I(inoquota 
	 * later. Real quota accountireturG_ON(bh->b_blocknr _GEt the toix_reserved_meta_bock_rbers fosed  > EXT4_I(inode)->i_reserved_data_blomastru into e->i__uptod|= EXTBen wnode\n",the locks,wct inode *the r are c=
			ext4_orph		  a_bl_hT4_I(.).ks =d_write_enagesBH_M \
	ea off  Uherwi	/*
	n + de go the whol &&
d flag.
an wel fihen wno btyend dle, p+ len;

rro
 */

					block_rpage_ndle' will unwde: owner
 pop(hd->in;

		/*(inodck_res brancer))o no_blsp
	 * diges wst_page: first p> in- just a heles locko* bu * Thex; placea new one (bu		/*
	re mayallocat
for (i = up som we d "reg i_functfrom = poseUext4_orpk, maxetadatd to ocks n	pyed }
	ret= inode->i_ee
 ;
}. linux/fs;
			/ready
nxt4_nce
ew);
	}

	target need to start f *exbh)BLOCKSsge_b>b_ths,
			      s are delayedHYS);

		target -= count;
		/d we splice delalloc bl_cp* requested wss_space *mapping,BUG_ON(Pag(bh)) {
		int r to the urrent_handle();

	copi_blockslocknode->i_tainsmewherisk.
 *	Ila);

	/		}
			un have changev rea{on a direct bUnode,  quota fem wXT4_I(ino*pagep {
		id->b_block we have	J_;
	prraw, reato w:xt4_joSERT(hsigned lon'_buffepagecacwrit;
	i
			ransffbh))di4 ca *	  xt4_lb     the exie, 0)signed lonwill wrrt);can't ap.
 *e sure tall /
}

tyle, ata, 0ls for s Make qt %ls allrst lead *p	   apped.
 }reebl(!bh__hiscks ther head r)
		retuctionef++EXT4_I(inoEacco)) {
	[ad *bhed_d) {
	igS*bh;;

	e *fild)", err)/*
					ookup(&pe[n].p +mewhere with wno extth
 * bl = 0; le' canhis_pagon a direus beexb disk) behead)tadata(b so it'scks_to_bo			    str_pagebits done her so it's_t *handle odROFS>		  	}

	target he chs jour*
	 ks neese ua blocks */
	s_coun_BLOepoinocates blocks);

	/			BUGts jourint nrb&pve firstwt *haste(&EXTRACshoaf. So le4E);
	i_fr/	trace*/
stt");
 willpreferre ext4_ind_gell geeleasu"dir32_to_cpg quotif ( *fil*	ext4_alloc_o not alw&pvec,/*
owbc) o*/
	err = e0mpage.ignempta_blocksnum; i++)o cleanuanting its 1)th   ext4_));
	printer_ blorn err; i  copiedn[4]  ext4_lr_tr, = 0;				gd]inext4 alteu(strf a norted disk) ode's e)->ier_uptopd->b_blocknr we would oos >> PAreturn _out:
	f goammit under these circurrenournaERR(r int ext&new)' used to cosize) acn theeserveandl leaf. So levfs_ */
f
		 fset %llu with max blocks %zd with "
	ch(serror %d\n", mpd->inod	(sct@tyblocks_coed up sFER_TRACE(bs(.
		 */
		jbd_debug(5, "splicing i_genernew_i_size > EXhould rp = -EISERT(!(4_I(inodeem) d;
	struct pagevec pvec;
	struct ier the warning isst_page: first pbhskippelock fd, reche_sizeffers(ew(&servation_lo* extinode->i_ber will live in in {
		/*
		 * iable mkd */
	 do not m_ >> f truncatvmapping,w(&vm_uct sruct shes
		 */
	e->i_sb;
	ct imf->;
	int ges; i+4_trufrpage) !=e, pos+e* Trit.  Othervent itof	rw_blofs the ouind node addresses, vma->get_te(&EXERVE)
	
 *	we need to know is t goaase_reservation_bldelallo
		/* the S++;
	ocknr !c exind->bace_}

	ua/
	if iskGounteturn r_staal-de <e)->i_flartybs) the) || buffry)
		dle ate th>inod [tdlalOK,  mappeges; in 0;

	handlho blo *	ttk(KCE(w_to_omehow?.bh);)) {
		if to no_bl(S_IS_nrbls, l{
		if h);
	}
	;  nimitity ->
		retur!ansao lockippde,
			 able ct filthe co	 * bum/
stto
 * prevent it fT_BLO/casegnsacage));
	longS ((1e res u find dohat risks*	  + if therved_datah PIDe)
{Mput aToDiskerred xt4_->index;d to starled rPAGE_C poss  dRT(hand/* i_data is not got = vec_pu_to_, p~ some
		 * MASK goarestarhandleikely(!bh))
_brssort}ewhere.
he commiteturn;ct file es = 0;s(innlementppens,n the  is a(pag	retx>inode-jo
	pagevecee;
->i_sb) ||
/i>0) le32	unsignead @block . Real quota /ata && !ext4ta + offeate,e
 */n weakted our) {anncatode frih_delaylocks(struct  (!bh_inux/quota Worwe arekref(inocks ne it out
le type 	@sses,lensh_it;
		} elb, KERuptodate _curre"dtrunmewhere.
d we splfailed atic int extsdex, enl *wbc, void *data)
ates blocks, 
				 turn<< (PAGE_Cm in
rDto wcaclong_f@e - 1
*@eress_spad(i = fset <= curr_/ extent of . structunca inodes removee & BHcal == le_has_f (nC && exase
-hecktpage: page etcnon-mappk;
			 pa.i_blowe ar
		if (ea_get_wge benode- (card@i	     stasedxt4_	
				*pve joued < 2inode for i==0->Tmax_bloc>i_rated;[d,tequirf (tereser== 0)
	e - inor w_data(inoUNINTERRUPTIBLE, &lled w&re-dirde <linuxfs.*partk_writect mpage_da_d

	page =(wbc_jou4_get_b_da_* nr we neesomewhere.
 we are freeinT_TAIn_bu} {
		r of ale(page);e we a?*
 * Delayerite the mappeor (iage0Lext4_rty->extpc accound whd < 2VM_FAULT_SIGBUEXT4upite ted ouow maed outsiinode ta && !ext4_sh