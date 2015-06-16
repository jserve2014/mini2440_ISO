/*
 * Copyright (C) 2007 Oracle.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License v2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 021110-1307, USA.
 */

#include <linux/kernel.h>
#include <linux/bio.h>
#include <linux/buffer_head.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/highmem.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/backing-dev.h>
#include <linux/mpage.h>
#include <linux/swap.h>
#include <linux/writeback.h>
#include <linux/statfs.h>
#include <linux/compat.h>
#include <linux/bit_spinlock.h>
#include <linux/xattr.h>
#include <linux/posix_acl.h>
#include <linux/falloc.h>
#include "compat.h"
#include "ctree.h"
#include "disk-io.h"
#include "transaction.h"
#include "btrfs_inode.h"
#include "ioctl.h"
#include "print-tree.h"
#include "volumes.h"
#include "ordered-data.h"
#include "xattr.h"
#include "tree-log.h"
#include "compression.h"
#include "locking.h"

struct btrfs_iget_args {
	u64 ino;
	struct btrfs_root *root;
};

static const struct inode_operations btrfs_dir_inode_operations;
static const struct inode_operations btrfs_symlink_inode_operations;
static const struct inode_operations btrfs_dir_ro_inode_operations;
static const struct inode_operations btrfs_special_inode_operations;
static const struct inode_operations btrfs_file_inode_operations;
static const struct address_space_operations btrfs_aops;
static const struct address_space_operations btrfs_symlink_aops;
static const struct file_operations btrfs_dir_file_operations;
static struct extent_io_ops btrfs_extent_io_ops;

static struct kmem_cache *btrfs_inode_cachep;
struct kmem_cache *btrfs_trans_handle_cachep;
struct kmem_cache *btrfs_transaction_cachep;
struct kmem_cache *btrfs_path_cachep;

#define S_SHIFT 12
static unsigned char btrfs_type_by_mode[S_IFMT >> S_SHIFT] = {
	[S_IFREG >> S_SHIFT]	= BTRFS_FT_REG_FILE,
	[S_IFDIR >> S_SHIFT]	= BTRFS_FT_DIR,
	[S_IFCHR >> S_SHIFT]	= BTRFS_FT_CHRDEV,
	[S_IFBLK >> S_SHIFT]	= BTRFS_FT_BLKDEV,
	[S_IFIFO >> S_SHIFT]	= BTRFS_FT_FIFO,
	[S_IFSOCK >> S_SHIFT]	= BTRFS_FT_SOCK,
	[S_IFLNK >> S_SHIFT]	= BTRFS_FT_SYMLINK,
};

static void btrfs_truncate(struct inode *inode);
static int btrfs_finish_ordered_io(struct inode *inode, u64 start, u64 end);
static noinline int cow_file_range(struct inode *inode,
				   struct page *locked_page,
				   u64 start, u64 end, int *page_started,
				   unsigned long *nr_written, int unlock);

static int btrfs_init_inode_security(struct inode *inode,  struct inode *dir)
{
	int err;

	err = btrfs_init_acl(inode, dir);
	if (!err)
		err = btrfs_xattr_security_init(inode, dir);
	return err;
}

/*
 * this does all the hard work for inserting an inline extent into
 * the btree.  The caller should have done a btrfs_drop_extents so that
 * no overlapping inline items exist in the btree
 */
static noinline int insert_inline_extent(struct btrfs_trans_handle *trans,
				struct btrfs_root *root, struct inode *inode,
				u64 start, size_t size, size_t compressed_size,
				struct page **compressed_pages)
{
	struct btrfs_key key;
	struct btrfs_path *path;
	struct extent_buffer *leaf;
	struct page *page = NULL;
	char *kaddr;
	unsigned long ptr;
	struct btrfs_file_extent_item *ei;
	int err = 0;
	int ret;
	size_t cur_size = size;
	size_t datasize;
	unsigned long offset;
	int use_compress = 0;

	if (compressed_size && compressed_pages) {
		use_compress = 1;
		cur_size = compressed_size;
	}

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	path->leave_spinning = 1;
	btrfs_set_trans_block_group(trans, inode);

	key.objectid = inode->i_ino;
	key.offset = start;
	btrfs_set_key_type(&key, BTRFS_EXTENT_DATA_KEY);
	datasize = btrfs_file_extent_calc_inline_size(cur_size);

	inode_add_bytes(inode, size);
	ret = btrfs_insert_empty_item(trans, root, path, &key,
				      datasize);
	BUG_ON(ret);
	if (ret) {
		err = ret;
		goto fail;
	}
	leaf = path->nodes[0];
	ei = btrfs_item_ptr(leaf, path->slots[0],
			    struct btrfs_file_extent_item);
	btrfs_set_file_extent_generation(leaf, ei, trans->transid);
	btrfs_set_file_extent_type(leaf, ei, BTRFS_FILE_EXTENT_INLINE);
	btrfs_set_file_extent_encryption(leaf, ei, 0);
	btrfs_set_file_extent_other_encoding(leaf, ei, 0);
	btrfs_set_file_extent_ram_bytes(leaf, ei, size);
	ptr = btrfs_file_extent_inline_start(ei);

	if (use_compress) {
		struct page *cpage;
		int i = 0;
		while (compressed_size > 0) {
			cpage = compressed_pages[i];
			cur_size = min_t(unsigned long, compressed_size,
				       PAGE_CACHE_SIZE);

			kaddr = kmap_atomic(cpage, KM_USER0);
			write_extent_buffer(leaf, kaddr, ptr, cur_size);
			kunmap_atomic(kaddr, KM_USER0);

			i++;
			ptr += cur_size;
			compressed_size -= cur_size;
		}
		btrfs_set_file_extent_compression(leaf, ei,
						  BTRFS_COMPRESS_ZLIB);
	} else {
		page = find_get_page(inode->i_mapping,
				     start >> PAGE_CACHE_SHIFT);
		btrfs_set_file_extent_compression(leaf, ei, 0);
		kaddr = kmap_atomic(page, KM_USER0);
		offset = start & (PAGE_CACHE_SIZE - 1);
		write_extent_buffer(leaf, kaddr + offset, ptr, size);
		kunmap_atomic(kaddr, KM_USER0);
		page_cache_release(page);
	}
	btrfs_mark_buffer_dirty(leaf);
	btrfs_free_path(path);

	BTRFS_I(inode)->disk_i_size = inode->i_size;
	btrfs_update_inode(trans, root, inode);
	return 0;
fail:
	btrfs_free_path(path);
	return err;
}


/*
 * conditionally insert an inline extent into the file.  This
 * does the checks required to make sure the data is small enough
 * to fit as an inline extent.
 */
static noinline int cow_file_range_inline(struct btrfs_trans_handle *trans,
				 struct btrfs_root *root,
				 struct inode *inode, u64 start, u64 end,
				 size_t compressed_size,
				 struct page **compressed_pages)
{
	u64 isize = i_size_read(inode);
	u64 actual_end = min(end + 1, isize);
	u64 inline_len = actual_end - start;
	u64 aligned_end = (end + root->sectorsize - 1) &
			~((u64)root->sectorsize - 1);
	u64 hint_byte;
	u64 data_len = inline_len;
	int ret;

	if (compressed_size)
		data_len = compressed_size;

	if (start > 0 ||
	    actual_end >= PAGE_CACHE_SIZE ||
	    data_len >= BTRFS_MAX_INLINE_DATA_SIZE(root) ||
	    (!compressed_size &&
	    (actual_end & (root->sectorsize - 1)) == 0) ||
	    end + 1 < isize ||
	    data_len > root->fs_info->max_inline) {
		return 1;
	}

	ret = btrfs_drop_extents(trans, root, inode, start,
				 aligned_end, aligned_end, start,
				 &hint_byte, 1);
	BUG_ON(ret);

	if (isize > actual_end)
		inline_len = min_t(u64, isize, actual_end);
	ret = insert_inline_extent(trans, root, inode, start,
				   inline_len, compressed_size,
				   compressed_pages);
	BUG_ON(ret);
	btrfs_drop_extent_cache(inode, start, aligned_end - 1, 0);
	return 0;
}

struct async_extent {
	u64 start;
	u64 ram_size;
	u64 compressed_size;
	struct page **pages;
	unsigned long nr_pages;
	struct list_head list;
};

struct async_cow {
	struct inode *inode;
	struct btrfs_root *root;
	struct page *locked_page;
	u64 start;
	u64 end;
	struct list_head extents;
	struct btrfs_work work;
};

static noinline int add_async_extent(struct async_cow *cow,
				     u64 start, u64 ram_size,
				     u64 compressed_size,
				     struct page **pages,
				     unsigned long nr_pages)
{
	struct async_extent *async_extent;

	async_extent = kmalloc(sizeof(*async_extent), GFP_NOFS);
	async_extent->start = start;
	async_extent->ram_size = ram_size;
	async_extent->compressed_size = compressed_size;
	async_extent->pages = pages;
	async_extent->nr_pages = nr_pages;
	list_add_tail(&async_extent->list, &cow->extents);
	return 0;
}

/*
 * we create compressed extents in two phases.  The first
 * phase compresses a range of pages that have already been
 * locked (both pages and state bits are locked).
 *
 * This is done inside an ordered work queue, and the compression
 * is spread across many cpus.  The actual IO submission is step
 * two, and the ordered work queue takes care of making sure that
 * happens in the same order things were put onto the queue by
 * writepages and friends.
 *
 * If this code finds it can't get good compression, it puts an
 * entry onto the work queue to write the uncompressed bytes.  This
 * makes sure that both compressed inodes and uncompressed inodes
 * are written in the same order that pdflush sent them down.
 */
static noinline int compress_file_range(struct inode *inode,
					struct page *locked_page,
					u64 start, u64 end,
					struct async_cow *async_cow,
					int *num_added)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_trans_handle *trans;
	u64 num_bytes;
	u64 orig_start;
	u64 disk_num_bytes;
	u64 blocksize = root->sectorsize;
	u64 actual_end;
	u64 isize = i_size_read(inode);
	int ret = 0;
	struct page **pages = NULL;
	unsigned long nr_pages;
	unsigned long nr_pages_ret = 0;
	unsigned long total_compressed = 0;
	unsigned long total_in = 0;
	unsigned long max_compressed = 128 * 1024;
	unsigned long max_uncompressed = 128 * 1024;
	int i;
	int will_compress;

	orig_start = start;

	actual_end = min_t(u64, isize, end + 1);
again:
	will_compress = 0;
	nr_pages = (end >> PAGE_CACHE_SHIFT) - (start >> PAGE_CACHE_SHIFT) + 1;
	nr_pages = min(nr_pages, (128 * 1024UL) / PAGE_CACHE_SIZE);

	/*
	 * we don't want to send crud past the end of i_size through
	 * compression, that's just a waste of CPU time.  So, if the
	 * end of the file is before the start of our current
	 * requested range of bytes, we bail out to the uncompressed
	 * cleanup code that can deal with all of this.
	 *
	 * It isn't really the fastest way to fix things, but this is a
	 * very uncommon corner.
	 */
	if (actual_end <= start)
		goto cleanup_and_bail_uncompressed;

	total_compressed = actual_end - start;

	/* we want to make sure that amount of ram required to uncompress
	 * an extent is reasonable, so we limit the total size in ram
	 * of a compressed extent to 128k.  This is a crucial number
	 * because it also controls how easily we can spread reads across
	 * cpus for decompression.
	 *
	 * We also want to make sure the amount of IO required to do
	 * a random read is reasonably small, so we limit the size of
	 * a compressed extent to 128k.
	 */
	total_compressed = min(total_compressed, max_uncompressed);
	num_bytes = (end - start + blocksize) & ~(blocksize - 1);
	num_bytes = max(blocksize,  num_bytes);
	disk_num_bytes = num_bytes;
	total_in = 0;
	ret = 0;

	/*
	 * we do compression for mount -o compress and when the
	 * inode has not been flagged as nocompress.  This flag can
	 * change at any time if we discover bad compression ratios.
	 */
	if (!(BTRFS_I(inode)->flags & BTRFS_INODE_NOCOMPRESS) &&
	    btrfs_test_opt(root, COMPRESS)) {
		WARN_ON(pages);
		pages = kzalloc(sizeof(struct page *) * nr_pages, GFP_NOFS);

		ret = btrfs_zlib_compress_pages(inode->i_mapping, start,
						total_compressed, pages,
						nr_pages, &nr_pages_ret,
						&total_in,
						&total_compressed,
						max_compressed);

		if (!ret) {
			unsigned long offset = total_compressed &
				(PAGE_CACHE_SIZE - 1);
			struct page *page = pages[nr_pages_ret - 1];
			char *kaddr;

			/* zero the tail end of the last page, we might be
			 * sending it down to disk
			 */
			if (offset) {
				kaddr = kmap_atomic(page, KM_USER0);
				memset(kaddr + offset, 0,
				       PAGE_CACHE_SIZE - offset);
				kunmap_atomic(kaddr, KM_USER0);
			}
			will_compress = 1;
		}
	}
	if (start == 0) {
		trans = btrfs_join_transaction(root, 1);
		BUG_ON(!trans);
		btrfs_set_trans_block_group(trans, inode);

		/* lets try to make an inline extent */
		if (ret || total_in < (actual_end - start)) {
			/* we didn't compress the entire range, try
			 * to make an uncompressed inline extent.
			 */
			ret = cow_file_range_inline(trans, root, inode,
						    start, end, 0, NULL);
		} else {
			/* try making a compressed inline extent */
			ret = cow_file_range_inline(trans, root, inode,
						    start, end,
						    total_compressed, pages);
		}
		btrfs_end_transaction(trans, root);
		if (ret == 0) {
			/*
			 * inline extent creation worked, we don't need
			 * to create any more async work items.  Unlock
			 * and free up our temp pages.
			 */
			extent_clear_unlock_delalloc(inode,
			     &BTRFS_I(inode)->io_tree,
			     start, end, NULL,
			     EXTENT_CLEAR_UNLOCK_PAGE | EXTENT_CLEAR_DIRTY |
			     EXTENT_CLEAR_DELALLOC |
			     EXTENT_CLEAR_ACCOUNTING |
			     EXTENT_SET_WRITEBACK | EXTENT_END_WRITEBACK);
			ret = 0;
			goto free_pages_out;
		}
	}

	if (will_compress) {
		/*
		 * we aren't doing an inline extent round the compressed size
		 * up to a block size boundary so the allocator does sane
		 * things
		 */
		total_compressed = (total_compressed + blocksize - 1) &
			~(blocksize - 1);

		/*
		 * one last check to make sure the compression is really a
		 * win, compare the page count read with the blocks on disk
		 */
		total_in = (total_in + PAGE_CACHE_SIZE - 1) &
			~(PAGE_CACHE_SIZE - 1);
		if (total_compressed >= total_in) {
			will_compress = 0;
		} else {
			disk_num_bytes = total_compressed;
			num_bytes = total_in;
		}
	}
	if (!will_compress && pages) {
		/*
		 * the compression code ran but failed to make things smaller,
		 * free any pages it allocated and our page pointer array
		 */
		for (i = 0; i < nr_pages_ret; i++) {
			WARN_ON(pages[i]->mapping);
			page_cache_release(pages[i]);
		}
		kfree(pages);
		pages = NULL;
		total_compressed = 0;
		nr_pages_ret = 0;

		/* flag the file so we don't compress in the future */
		BTRFS_I(inode)->flags |= BTRFS_INODE_NOCOMPRESS;
	}
	if (will_compress) {
		*num_added += 1;

		/* the async work queues will take care of doing actual
		 * allocation on disk for these compressed pages,
		 * and will submit them to the elevator.
		 */
		add_async_extent(async_cow, start, num_bytes,
				 total_compressed, pages, nr_pages_ret);

		if (start + num_bytes < end && start + num_bytes < actual_end) {
			start += num_bytes;
			pages = NULL;
			cond_resched();
			goto again;
		}
	} else {
cleanup_and_bail_uncompressed:
		/*
		 * No compression, but we still need to write the pages in
		 * the file we've been given so far.  redirty the locked
		 * page if it corresponds to our extent and set things up
		 * for the async work queue to run cow_file_range to do
		 * the normal delalloc dance
		 */
		if (page_offset(locked_page) >= start &&
		    page_offset(locked_page) <= end) {
			__set_page_dirty_nobuffers(locked_page);
			/* unlocked later on in the async handlers */
		}
		add_async_extent(async_cow, start, end - start + 1, 0, NULL, 0);
		*num_added += 1;
	}

out:
	return 0;

free_pages_out:
	for (i = 0; i < nr_pages_ret; i++) {
		WARN_ON(pages[i]->mapping);
		page_cache_release(pages[i]);
	}
	kfree(pages);

	goto out;
}

/*
 * phase two of compressed writeback.  This is the ordered portion
 * of the code, which only gets called in the order the work was
 * queued.  We walk all the async extents created by compress_file_range
 * and send them down to the disk.
 */
static noinline int submit_compressed_extents(struct inode *inode,
					      struct async_cow *async_cow)
{
	struct async_extent *async_extent;
	u64 alloc_hint = 0;
	struct btrfs_trans_handle *trans;
	struct btrfs_key ins;
	struct extent_map *em;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct extent_map_tree *em_tree = &BTRFS_I(inode)->extent_tree;
	struct extent_io_tree *io_tree;
	int ret = 0;

	if (list_empty(&async_cow->extents))
		return 0;

	trans = btrfs_join_transaction(root, 1);

	while (!list_empty(&async_cow->extents)) {
		async_extent = list_entry(async_cow->extents.next,
					  struct async_extent, list);
		list_del(&async_extent->list);

		io_tree = &BTRFS_I(inode)->io_tree;

retry:
		/* did the compression code fall back to uncompressed IO? */
		if (!async_extent->pages) {
			int page_started = 0;
			unsigned long nr_written = 0;

			lock_extent(io_tree, async_extent->start,
				    async_extent->start +
				    async_extent->ram_size - 1, GFP_NOFS);

			/* allocate blocks */
			ret = cow_file_range(inode, async_cow->locked_page,
					     async_extent->start,
					     async_extent->start +
					     async_extent->ram_size - 1,
					     &page_started, &nr_written, 0);

			/*
			 * if page_started, cow_file_range inserted an
			 * inline extent and took care of all the unlocking
			 * and IO for us.  Otherwise, we need to submit
			 * all those pages down to the drive.
			 */
			if (!page_started && !ret)
				extent_write_locked_range(io_tree,
						  inode, async_extent->start,
						  async_extent->start +
						  async_extent->ram_size - 1,
						  btrfs_get_extent,
						  WB_SYNC_ALL);
			kfree(async_extent);
			cond_resched();
			continue;
		}

		lock_extent(io_tree, async_extent->start,
			    async_extent->start + async_extent->ram_size - 1,
			    GFP_NOFS);
		/*
		 * here we're doing allocation and writeback of the
		 * compressed pages
		 */
		btrfs_drop_extent_cache(inode, async_extent->start,
					async_extent->start +
					async_extent->ram_size - 1, 0);

		ret = btrfs_reserve_extent(trans, root,
					   async_extent->compressed_size,
					   async_extent->compressed_size,
					   0, alloc_hint,
					   (u64)-1, &ins, 1);
		if (ret) {
			int i;
			for (i = 0; i < async_extent->nr_pages; i++) {
				WARN_ON(async_extent->pages[i]->mapping);
				page_cache_release(async_extent->pages[i]);
			}
			kfree(async_extent->pages);
			async_extent->nr_pages = 0;
			async_extent->pages = NULL;
			unlock_extent(io_tree, async_extent->start,
				      async_extent->start +
				      async_extent->ram_size - 1, GFP_NOFS);
			goto retry;
		}

		em = alloc_extent_map(GFP_NOFS);
		em->start = async_extent->start;
		em->len = async_extent->ram_size;
		em->orig_start = em->start;

		em->block_start = ins.objectid;
		em->block_len = ins.offset;
		em->bdev = root->fs_info->fs_devices->latest_bdev;
		set_bit(EXTENT_FLAG_PINNED, &em->flags);
		set_bit(EXTENT_FLAG_COMPRESSED, &em->flags);

		while (1) {
			write_lock(&em_tree->lock);
			ret = add_extent_mapping(em_tree, em);
			write_unlock(&em_tree->lock);
			if (ret != -EEXIST) {
				free_extent_map(em);
				break;
			}
			btrfs_drop_extent_cache(inode, async_extent->start,
						async_extent->start +
						async_extent->ram_size - 1, 0);
		}

		ret = btrfs_add_ordered_extent(inode, async_extent->start,
					       ins.objectid,
					       async_extent->ram_size,
					       ins.offset,
					       BTRFS_ORDERED_COMPRESSED);
		BUG_ON(ret);

		btrfs_end_transaction(trans, root);

		/*
		 * clear dirty, set writeback and unlock the pages.
		 */
		extent_clear_unlock_delalloc(inode,
				&BTRFS_I(inode)->io_tree,
				async_extent->start,
				async_extent->start +
				async_extent->ram_size - 1,
				NULL, EXTENT_CLEAR_UNLOCK_PAGE |
				EXTENT_CLEAR_UNLOCK |
				EXTENT_CLEAR_DELALLOC |
				EXTENT_CLEAR_DIRTY | EXTENT_SET_WRITEBACK);

		ret = btrfs_submit_compressed_write(inode,
				    async_extent->start,
				    async_extent->ram_size,
				    ins.objectid,
				    ins.offset, async_extent->pages,
				    async_extent->nr_pages);

		BUG_ON(ret);
		trans = btrfs_join_transaction(root, 1);
		alloc_hint = ins.objectid + ins.offset;
		kfree(async_extent);
		cond_resched();
	}

	btrfs_end_transaction(trans, root);
	return 0;
}

/*
 * when extent_io.c finds a delayed allocation range in the file,
 * the call backs end up in this code.  The basic idea is to
 * allocate extents on disk for the range, and create ordered data structs
 * in ram to track those extents.
 *
 * locked_page is the page that writepage had locked already.  We use
 * it to make sure we don't do extra locks or unlocks.
 *
 * *page_started is set to one if we unlock locked_page and do everything
 * required to start IO on it.  It may be clean and already done with
 * IO when we return.
 */
static noinline int cow_file_range(struct inode *inode,
				   struct page *locked_page,
				   u64 start, u64 end, int *page_started,
				   unsigned long *nr_written,
				   int unlock)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_trans_handle *trans;
	u64 alloc_hint = 0;
	u64 num_bytes;
	unsigned long ram_size;
	u64 disk_num_bytes;
	u64 cur_alloc_size;
	u64 blocksize = root->sectorsize;
	u64 actual_end;
	u64 isize = i_size_read(inode);
	struct btrfs_key ins;
	struct extent_map *em;
	struct extent_map_tree *em_tree = &BTRFS_I(inode)->extent_tree;
	int ret = 0;

	trans = btrfs_join_transaction(root, 1);
	BUG_ON(!trans);
	btrfs_set_trans_block_group(trans, inode);

	actual_end = min_t(u64, isize, end + 1);

	num_bytes = (end - start + blocksize) & ~(blocksize - 1);
	num_bytes = max(blocksize,  num_bytes);
	disk_num_bytes = num_bytes;
	ret = 0;

	if (start == 0) {
		/* lets try to make an inline extent */
		ret = cow_file_range_inline(trans, root, inode,
					    start, end, 0, NULL);
		if (ret == 0) {
			extent_clear_unlock_delalloc(inode,
				     &BTRFS_I(inode)->io_tree,
				     start, end, NULL,
				     EXTENT_CLEAR_UNLOCK_PAGE |
				     EXTENT_CLEAR_UNLOCK |
				     EXTENT_CLEAR_DELALLOC |
				     EXTENT_CLEAR_ACCOUNTING |
				     EXTENT_CLEAR_DIRTY |
				     EXTENT_SET_WRITEBACK |
				     EXTENT_END_WRITEBACK);
			*nr_written = *nr_written +
			     (end - start + PAGE_CACHE_SIZE) / PAGE_CACHE_SIZE;
			*page_started = 1;
			ret = 0;
			goto out;
		}
	}

	BUG_ON(disk_num_bytes >
	       btrfs_super_total_bytes(&root->fs_info->super_copy));


	read_lock(&BTRFS_I(inode)->extent_tree.lock);
	em = search_extent_mapping(&BTRFS_I(inode)->extent_tree,
				   start, num_bytes);
	if (em) {
		/*
		 * if block start isn't an actual block number then find the
		 * first block in this inode and use that as a hint.  If that
		 * block is also bogus then just don't worry about it.
		 */
		if (em->block_start >= EXTENT_MAP_LAST_BYTE) {
			free_extent_map(em);
			em = search_extent_mapping(em_tree, 0, 0);
			if (em && em->block_start < EXTENT_MAP_LAST_BYTE)
				alloc_hint = em->block_start;
			if (em)
				free_extent_map(em);
		} else {
			alloc_hint = em->block_start;
			free_extent_map(em);
		}
	}
	read_unlock(&BTRFS_I(inode)->extent_tree.lock);
	btrfs_drop_extent_cache(inode, start, start + num_bytes - 1, 0);

	while (disk_num_bytes > 0) {
		unsigned long op;

		cur_alloc_size = min(disk_num_bytes, root->fs_info->max_extent);
		ret = btrfs_reserve_extent(trans, root, cur_alloc_size,
					   root->sectorsize, 0, alloc_hint,
					   (u64)-1, &ins, 1);
		BUG_ON(ret);

		em = alloc_extent_map(GFP_NOFS);
		em->start = start;
		em->orig_start = em->start;
		ram_size = ins.offset;
		em->len = ins.offset;

		em->block_start = ins.objectid;
		em->block_len = ins.offset;
		em->bdev = root->fs_info->fs_devices->latest_bdev;
		set_bit(EXTENT_FLAG_PINNED, &em->flags);

		while (1) {
			write_lock(&em_tree->lock);
			ret = add_extent_mapping(em_tree, em);
			write_unlock(&em_tree->lock);
			if (ret != -EEXIST) {
				free_extent_map(em);
				break;
			}
			btrfs_drop_extent_cache(inode, start,
						start + ram_size - 1, 0);
		}

		cur_alloc_size = ins.offset;
		ret = btrfs_add_ordered_extent(inode, start, ins.objectid,
					       ram_size, cur_alloc_size, 0);
		BUG_ON(ret);

		if (root->root_key.objectid ==
		    BTRFS_DATA_RELOC_TREE_OBJECTID) {
			ret = btrfs_reloc_clone_csums(inode, start,
						      cur_alloc_size);
			BUG_ON(ret);
		}

		if (disk_num_bytes < cur_alloc_size)
			break;

		/* we're not doing compressed IO, don't unlock the first
		 * page (which the caller expects to stay locked), don't
		 * clear any dirty bits and don't set any writeback bits
		 *
		 * Do set the Private2 bit so we know this page was properly
		 * setup for writepage
		 */
		op = unlock ? EXTENT_CLEAR_UNLOCK_PAGE : 0;
		op |= EXTENT_CLEAR_UNLOCK | EXTENT_CLEAR_DELALLOC |
			EXTENT_SET_PRIVATE2;

		extent_clear_unlock_delalloc(inode, &BTRFS_I(inode)->io_tree,
					     start, start + ram_size - 1,
					     locked_page, op);
		disk_num_bytes -= cur_alloc_size;
		num_bytes -= cur_alloc_size;
		alloc_hint = ins.objectid + ins.offset;
		start += cur_alloc_size;
	}
out:
	ret = 0;
	btrfs_end_transaction(trans, root);

	return ret;
}

/*
 * work queue call back to started compression on a file and pages
 */
static noinline void async_cow_start(struct btrfs_work *work)
{
	struct async_cow *async_cow;
	int num_added = 0;
	async_cow = container_of(work, struct async_cow, work);

	compress_file_range(async_cow->inode, async_cow->locked_page,
			    async_cow->start, async_cow->end, async_cow,
			    &num_added);
	if (num_added == 0)
		async_cow->inode = NULL;
}

/*
 * work queue call back to submit previously compressed pages
 */
static noinline void async_cow_submit(struct btrfs_work *work)
{
	struct async_cow *async_cow;
	struct btrfs_root *root;
	unsigned long nr_pages;

	async_cow = container_of(work, struct async_cow, work);

	root = async_cow->root;
	nr_pages = (async_cow->end - async_cow->start + PAGE_CACHE_SIZE) >>
		PAGE_CACHE_SHIFT;

	atomic_sub(nr_pages, &root->fs_info->async_delalloc_pages);

	if (atomic_read(&root->fs_info->async_delalloc_pages) <
	    5 * 1042 * 1024 &&
	    waitqueue_active(&root->fs_info->async_submit_wait))
		wake_up(&root->fs_info->async_submit_wait);

	if (async_cow->inode)
		submit_compressed_extents(async_cow->inode, async_cow);
}

static noinline void async_cow_free(struct btrfs_work *work)
{
	struct async_cow *async_cow;
	async_cow = container_of(work, struct async_cow, work);
	kfree(async_cow);
}

static int cow_file_range_async(struct inode *inode, struct page *locked_page,
				u64 start, u64 end, int *page_started,
				unsigned long *nr_written)
{
	struct async_cow *async_cow;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	unsigned long nr_pages;
	u64 cur_end;
	int limit = 10 * 1024 * 1042;

	clear_extent_bit(&BTRFS_I(inode)->io_tree, start, end, EXTENT_LOCKED,
			 1, 0, NULL, GFP_NOFS);
	while (start < end) {
		async_cow = kmalloc(sizeof(*async_cow), GFP_NOFS);
		async_cow->inode = inode;
		async_cow->root = root;
		async_cow->locked_page = locked_page;
		async_cow->start = start;

		if (BTRFS_I(inode)->flags & BTRFS_INODE_NOCOMPRESS)
			cur_end = end;
		else
			cur_end = min(end, start + 512 * 1024 - 1);

		async_cow->end = cur_end;
		INIT_LIST_HEAD(&async_cow->extents);

		async_cow->work.func = async_cow_start;
		async_cow->work.ordered_func = async_cow_submit;
		async_cow->work.ordered_free = async_cow_free;
		async_cow->work.flags = 0;

		nr_pages = (cur_end - start + PAGE_CACHE_SIZE) >>
			PAGE_CACHE_SHIFT;
		atomic_add(nr_pages, &root->fs_info->async_delalloc_pages);

		btrfs_queue_worker(&root->fs_info->delalloc_workers,
				   &async_cow->work);

		if (atomic_read(&root->fs_info->async_delalloc_pages) > limit) {
			wait_event(root->fs_info->async_submit_wait,
			   (atomic_read(&root->fs_info->async_delalloc_pages) <
			    limit));
		}

		while (atomic_read(&root->fs_info->async_submit_draining) &&
		      atomic_read(&root->fs_info->async_delalloc_pages)) {
			wait_event(root->fs_info->async_submit_wait,
			  (atomic_read(&root->fs_info->async_delalloc_pages) ==
			   0));
		}

		*nr_written += nr_pages;
		start = cur_end + 1;
	}
	*page_started = 1;
	return 0;
}

static noinline int csum_exist_in_range(struct btrfs_root *root,
					u64 bytenr, u64 num_bytes)
{
	int ret;
	struct btrfs_ordered_sum *sums;
	LIST_HEAD(list);

	ret = btrfs_lookup_csums_range(root->fs_info->csum_root, bytenr,
				       bytenr + num_bytes - 1, &list);
	if (ret == 0 && list_empty(&list))
		return 0;

	while (!list_empty(&list)) {
		sums = list_entry(list.next, struct btrfs_ordered_sum, list);
		list_del(&sums->list);
		kfree(sums);
	}
	return 1;
}

/*
 * when nowcow writeback call back.  This checks for snapshots or COW copies
 * of the extents that exist in the file, and COWs the file as required.
 *
 * If no cow copies or snapshots exist, we write directly to the existing
 * blocks on disk
 */
static noinline int run_delalloc_nocow(struct inode *inode,
				       struct page *locked_page,
			      u64 start, u64 end, int *page_started, int force,
			      unsigned long *nr_written)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_trans_handle *trans;
	struct extent_buffer *leaf;
	struct btrfs_path *path;
	struct btrfs_file_extent_item *fi;
	struct btrfs_key found_key;
	u64 cow_start;
	u64 cur_offset;
	u64 extent_end;
	u64 extent_offset;
	u64 disk_bytenr;
	u64 num_bytes;
	int extent_type;
	int ret;
	int type;
	int nocow;
	int check_prev = 1;

	path = btrfs_alloc_path();
	BUG_ON(!path);
	trans = btrfs_join_transaction(root, 1);
	BUG_ON(!trans);

	cow_start = (u64)-1;
	cur_offset = start;
	while (1) {
		ret = btrfs_lookup_file_extent(trans, root, path, inode->i_ino,
					       cur_offset, 0);
		BUG_ON(ret < 0);
		if (ret > 0 && path->slots[0] > 0 && check_prev) {
			leaf = path->nodes[0];
			btrfs_item_key_to_cpu(leaf, &found_key,
					      path->slots[0] - 1);
			if (found_key.objectid == inode->i_ino &&
			    found_key.type == BTRFS_EXTENT_DATA_KEY)
				path->slots[0]--;
		}
		check_prev = 0;
next_slot:
		leaf = path->nodes[0];
		if (path->slots[0] >= btrfs_header_nritems(leaf)) {
			ret = btrfs_next_leaf(root, path);
			if (ret < 0)
				BUG_ON(1);
			if (ret > 0)
				break;
			leaf = path->nodes[0];
		}

		nocow = 0;
		disk_bytenr = 0;
		num_bytes = 0;
		btrfs_item_key_to_cpu(leaf, &found_key, path->slots[0]);

		if (found_key.objectid > inode->i_ino ||
		    found_key.type > BTRFS_EXTENT_DATA_KEY ||
		    found_key.offset > end)
			break;

		if (found_key.offset > cur_offset) {
			extent_end = found_key.offset;
			extent_type = 0;
			goto out_check;
		}

		fi = btrfs_item_ptr(leaf, path->slots[0],
				    struct btrfs_file_extent_item);
		extent_type = btrfs_file_extent_type(leaf, fi);

		if (extent_type == BTRFS_FILE_EXTENT_REG ||
		    extent_type == BTRFS_FILE_EXTENT_PREALLOC) {
			disk_bytenr = btrfs_file_extent_disk_bytenr(leaf, fi);
			extent_offset = btrfs_file_extent_offset(leaf, fi);
			extent_end = found_key.offset +
				btrfs_file_extent_num_bytes(leaf, fi);
			if (extent_end <= start) {
				path->slots[0]++;
				goto next_slot;
			}
			if (disk_bytenr == 0)
				goto out_check;
			if (btrfs_file_extent_compression(leaf, fi) ||
			    btrfs_file_extent_encryption(leaf, fi) ||
			    btrfs_file_extent_other_encoding(leaf, fi))
				goto out_check;
			if (extent_type == BTRFS_FILE_EXTENT_REG && !force)
				goto out_check;
			if (btrfs_extent_readonly(root, disk_bytenr))
				goto out_check;
			if (btrfs_cross_ref_exist(trans, root, inode->i_ino,
						  found_key.offset -
						  extent_offset, disk_bytenr))
				goto out_check;
			disk_bytenr += extent_offset;
			disk_bytenr += cur_offset - found_key.offset;
			num_bytes = min(end + 1, extent_end) - cur_offset;
			/*
			 * force cow if csum exists in the range.
			 * this ensure that csum for a given extent are
			 * either valid or do not exist.
			 */
			if (csum_exist_in_range(root, disk_bytenr, num_bytes))
				goto out_check;
			nocow = 1;
		} else if (extent_type == BTRFS_FILE_EXTENT_INLINE) {
			extent_end = found_key.offset +
				btrfs_file_extent_inline_len(leaf, fi);
			extent_end = ALIGN(extent_end, root->sectorsize);
		} else {
			BUG_ON(1);
		}
out_check:
		if (extent_end <= start) {
			path->slots[0]++;
			goto next_slot;
		}
		if (!nocow) {
			if (cow_start == (u64)-1)
				cow_start = cur_offset;
			cur_offset = extent_end;
			if (cur_offset > end)
				break;
			path->slots[0]++;
			goto next_slot;
		}

		btrfs_release_path(root, path);
		if (cow_start != (u64)-1) {
			ret = cow_file_range(inode, locked_page, cow_start,
					found_key.offset - 1, page_started,
					nr_written, 1);
			BUG_ON(ret);
			cow_start = (u64)-1;
		}

		if (extent_type == BTRFS_FILE_EXTENT_PREALLOC) {
			struct extent_map *em;
			struct extent_map_tree *em_tree;
			em_tree = &BTRFS_I(inode)->extent_tree;
			em = alloc_extent_map(GFP_NOFS);
			em->start = cur_offset;
			em->orig_start = em->start;
			em->len = num_bytes;
			em->block_len = num_bytes;
			em->block_start = disk_bytenr;
			em->bdev = root->fs_info->fs_devices->latest_bdev;
			set_bit(EXTENT_FLAG_PINNED, &em->flags);
			while (1) {
				write_lock(&em_tree->lock);
				ret = add_extent_mapping(em_tree, em);
				write_unlock(&em_tree->lock);
				if (ret != -EEXIST) {
					free_extent_map(em);
					break;
				}
				btrfs_drop_extent_cache(inode, em->start,
						em->start + em->len - 1, 0);
			}
			type = BTRFS_ORDERED_PREALLOC;
		} else {
			type = BTRFS_ORDERED_NOCOW;
		}

		ret = btrfs_add_ordered_extent(inode, cur_offset, disk_bytenr,
					       num_bytes, num_bytes, type);
		BUG_ON(ret);

		extent_clear_unlock_delalloc(inode, &BTRFS_I(inode)->io_tree,
				cur_offset, cur_offset + num_bytes - 1,
				locked_page, EXTENT_CLEAR_UNLOCK_PAGE |
				EXTENT_CLEAR_UNLOCK | EXTENT_CLEAR_DELALLOC |
				EXTENT_SET_PRIVATE2);
		cur_offset = extent_end;
		if (cur_offset > end)
			break;
	}
	btrfs_release_path(root, path);

	if (cur_offset <= end && cow_start == (u64)-1)
		cow_start = cur_offset;
	if (cow_start != (u64)-1) {
		ret = cow_file_range(inode, locked_page, cow_start, end,
				     page_started, nr_written, 1);
		BUG_ON(ret);
	}

	ret = btrfs_end_transaction(trans, root);
	BUG_ON(ret);
	btrfs_free_path(path);
	return 0;
}

/*
 * extent_io.c call back to do delayed allocation processing
 */
static int run_delalloc_range(struct inode *inode, struct page *locked_page,
			      u64 start, u64 end, int *page_started,
			      unsigned long *nr_written)
{
	int ret;
	struct btrfs_root *root = BTRFS_I(inode)->root;

	if (BTRFS_I(inode)->flags & BTRFS_INODE_NODATACOW)
		ret = run_delalloc_nocow(inode, locked_page, start, end,
					 page_started, 1, nr_written);
	else if (BTRFS_I(inode)->flags & BTRFS_INODE_PREALLOC)
		ret = run_delalloc_nocow(inode, locked_page, start, end,
					 page_started, 0, nr_written);
	else if (!btrfs_test_opt(root, COMPRESS))
		ret = cow_file_range(inode, locked_page, start, end,
				      page_started, nr_written, 1);
	else
		ret = cow_file_range_async(inode, locked_page, start, end,
					   page_started, nr_written);
	return ret;
}

static int btrfs_split_extent_hook(struct inode *inode,
				    struct extent_state *orig, u64 split)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	u64 size;

	if (!(orig->state & EXTENT_DELALLOC))
		return 0;

	size = orig->end - orig->start + 1;
	if (size > root->fs_info->max_extent) {
		u64 num_extents;
		u64 new_size;

		new_size = orig->end - split + 1;
		num_extents = div64_u64(size + root->fs_info->max_extent - 1,
					root->fs_info->max_extent);

		/*
		 * if we break a large extent up then leave oustanding_extents
		 * be, since we've already accounted for the large extent.
		 */
		if (div64_u64(new_size + root->fs_info->max_extent - 1,
			      root->fs_info->max_extent) < num_extents)
			return 0;
	}

	spin_lock(&BTRFS_I(inode)->accounting_lock);
	BTRFS_I(inode)->outstanding_extents++;
	spin_unlock(&BTRFS_I(inode)->accounting_lock);

	return 0;
}

/*
 * extent_io.c merge_extent_hook, used to track merged delayed allocation
 * extents so we can keep track of new extents that are just merged onto old
 * extents, such as when we are doing sequential writes, so we can properly
 * account for the metadata space we'll need.
 */
static int btrfs_merge_extent_hook(struct inode *inode,
				   struct extent_state *new,
				   struct extent_state *other)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	u64 new_size, old_size;
	u64 num_extents;

	/* not delalloc, ignore it */
	if (!(other->state & EXTENT_DELALLOC))
		return 0;

	old_size = other->end - other->start + 1;
	if (new->start < other->start)
		new_size = other->end - new->start + 1;
	else
		new_size = new->end - other->start + 1;

	/* we're not bigger than the max, unreserve the space and go */
	if (new_size <= root->fs_info->max_extent) {
		spin_lock(&BTRFS_I(inode)->accounting_lock);
		BTRFS_I(inode)->outstanding_extents--;
		spin_unlock(&BTRFS_I(inode)->accounting_lock);
		return 0;
	}

	/*
	 * If we grew by another max_extent, just return, we want to keep that
	 * reserved amount.
	 */
	num_extents = div64_u64(old_size + root->fs_info->max_extent - 1,
				root->fs_info->max_extent);
	if (div64_u64(new_size + root->fs_info->max_extent - 1,
		      root->fs_info->max_extent) > num_extents)
		return 0;

	spin_lock(&BTRFS_I(inode)->accounting_lock);
	BTRFS_I(inode)->outstanding_extents--;
	spin_unlock(&BTRFS_I(inode)->accounting_lock);

	return 0;
}

/*
 * extent_io.c set_bit_hook, used to track delayed allocation
 * bytes in this file, and to maintain the list of inodes that
 * have pending delalloc work to be done.
 */
static int btrfs_set_bit_hook(struct inode *inode, u64 start, u64 end,
		       unsigned long old, unsigned long bits)
{

	/*
	 * set_bit and clear bit hooks normally require _irqsave/restore
	 * but in this case, we are only testeing for the DELALLOC
	 * bit, which is only set or cleared with irqs on
	 */
	if (!(old & EXTENT_DELALLOC) && (bits & EXTENT_DELALLOC)) {
		struct btrfs_root *root = BTRFS_I(inode)->root;

		spin_lock(&BTRFS_I(inode)->accounting_lock);
		BTRFS_I(inode)->outstanding_extents++;
		spin_unlock(&BTRFS_I(inode)->accounting_lock);
		btrfs_delalloc_reserve_space(root, inode, end - start + 1);
		spin_lock(&root->fs_info->delalloc_lock);
		BTRFS_I(inode)->delalloc_bytes += end - start + 1;
		root->fs_info->delalloc_bytes += end - start + 1;
		if (list_empty(&BTRFS_I(inode)->delalloc_inodes)) {
			list_add_tail(&BTRFS_I(inode)->delalloc_inodes,
				      &root->fs_info->delalloc_inodes);
		}
		spin_unlock(&root->fs_info->delalloc_lock);
	}
	return 0;
}

/*
 * extent_io.c clear_bit_hook, see set_bit_hook for why
 */
static int btrfs_clear_bit_hook(struct inode *inode,
				struct extent_state *state, unsigned long bits)
{
	/*
	 * set_bit and clear bit hooks normally require _irqsave/restore
	 * but in this case, we are only testeing for the DELALLOC
	 * bit, which is only set or cleared with irqs on
	 */
	if ((state->state & EXTENT_DELALLOC) && (bits & EXTENT_DELALLOC)) {
		struct btrfs_root *root = BTRFS_I(inode)->root;

		if (bits & EXTENT_DO_ACCOUNTING) {
			spin_lock(&BTRFS_I(inode)->accounting_lock);
			BTRFS_I(inode)->outstanding_extents--;
			spin_unlock(&BTRFS_I(inode)->accounting_lock);
			btrfs_unreserve_metadata_for_delalloc(root, inode, 1);
		}

		spin_lock(&root->fs_info->delalloc_lock);
		if (state->end - state->start + 1 >
		    root->fs_info->delalloc_bytes) {
			printk(KERN_INFO "btrfs warning: delalloc account "
			       "%llu %llu\n",
			       (unsigned long long)
			       state->end - state->start + 1,
			       (unsigned long long)
			       root->fs_info->delalloc_bytes);
			btrfs_delalloc_free_space(root, inode, (u64)-1);
			root->fs_info->delalloc_bytes = 0;
			BTRFS_I(inode)->delalloc_bytes = 0;
		} else {
			btrfs_delalloc_free_space(root, inode,
						  state->end -
						  state->start + 1);
			root->fs_info->delalloc_bytes -= state->end -
				state->start + 1;
			BTRFS_I(inode)->delalloc_bytes -= state->end -
				state->start + 1;
		}
		if (BTRFS_I(inode)->delalloc_bytes == 0 &&
		    !list_empty(&BTRFS_I(inode)->delalloc_inodes)) {
			list_del_init(&BTRFS_I(inode)->delalloc_inodes);
		}
		spin_unlock(&root->fs_info->delalloc_lock);
	}
	return 0;
}

/*
 * extent_io.c merge_bio_hook, this must check the chunk tree to make sure
 * we don't create bios that span stripes or chunks
 */
int btrfs_merge_bio_hook(struct page *page, unsigned long offset,
			 size_t size, struct bio *bio,
			 unsigned long bio_flags)
{
	struct btrfs_root *root = BTRFS_I(page->mapping->host)->root;
	struct btrfs_mapping_tree *map_tree;
	u64 logical = (u64)bio->bi_sector << 9;
	u64 length = 0;
	u64 map_length;
	int ret;

	if (bio_flags & EXTENT_BIO_COMPRESSED)
		return 0;

	length = bio->bi_size;
	map_tree = &root->fs_info->mapping_tree;
	map_length = length;
	ret = btrfs_map_block(map_tree, READ, logical,
			      &map_length, NULL, 0);

	if (map_length < length + size)
		return 1;
	return 0;
}

/*
 * in order to insert checksums into the metadata in large chunks,
 * we wait until bio submission time.   All the pages in the bio are
 * checksummed and sums are attached onto the ordered extent record.
 *
 * At IO completion time the cums attached on the ordered extent record
 * are inserted into the btree
 */
static int __btrfs_submit_bio_start(struct inode *inode, int rw,
				    struct bio *bio, int mirror_num,
				    unsigned long bio_flags)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	int ret = 0;

	ret = btrfs_csum_one_bio(root, inode, bio, 0, 0);
	BUG_ON(ret);
	return 0;
}

/*
 * in order to insert checksums into the metadata in large chunks,
 * we wait until bio submission time.   All the pages in the bio are
 * checksummed and sums are attached onto the ordered extent record.
 *
 * At IO completion time the cums attached on the ordered extent record
 * are inserted into the btree
 */
static int __btrfs_submit_bio_done(struct inode *inode, int rw, struct bio *bio,
			  int mirror_num, unsigned long bio_flags)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	return btrfs_map_bio(root, rw, bio, mirror_num, 1);
}

/*
 * extent_io.c submission hook. This does the right thing for csum calculation
 * on write, or reading the csums from the tree before a read
 */
static int btrfs_submit_bio_hook(struct inode *inode, int rw, struct bio *bio,
			  int mirror_num, unsigned long bio_flags)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	int ret = 0;
	int skip_sum;

	skip_sum = BTRFS_I(inode)->flags & BTRFS_INODE_NODATASUM;

	ret = btrfs_bio_wq_end_io(root->fs_info, bio, 0);
	BUG_ON(ret);

	if (!(rw & (1 << BIO_RW))) {
		if (bio_flags & EXTENT_BIO_COMPRESSED) {
			return btrfs_submit_compressed_read(inode, bio,
						    mirror_num, bio_flags);
		} else if (!skip_sum)
			btrfs_lookup_bio_sums(root, inode, bio, NULL);
		goto mapit;
	} else if (!skip_sum) {
		/* csum items have already been cloned */
		if (root->root_key.objectid == BTRFS_DATA_RELOC_TREE_OBJECTID)
			goto mapit;
		/* we're doing a write, do the async checksumming */
		return btrfs_wq_submit_bio(BTRFS_I(inode)->root->fs_info,
				   inode, rw, bio, mirror_num,
				   bio_flags, __btrfs_submit_bio_start,
				   __btrfs_submit_bio_done);
	}

mapit:
	return btrfs_map_bio(root, rw, bio, mirror_num, 0);
}

/*
 * given a list of ordered sums record them in the inode.  This happens
 * at IO completion time based on sums calculated at bio submission time.
 */
static noinline int add_pending_csums(struct btrfs_trans_handle *trans,
			     struct inode *inode, u64 file_offset,
			     struct list_head *list)
{
	struct btrfs_ordered_sum *sum;

	btrfs_set_trans_block_group(trans, inode);

	list_for_each_entry(sum, list, list) {
		btrfs_csum_file_blocks(trans,
		       BTRFS_I(inode)->root->fs_info->csum_root, sum);
	}
	return 0;
}

int btrfs_set_extent_delalloc(struct inode *inode, u64 start, u64 end)
{
	if ((end & (PAGE_CACHE_SIZE - 1)) == 0)
		WARN_ON(1);
	return set_extent_delalloc(&BTRFS_I(inode)->io_tree, start, end,
				   GFP_NOFS);
}

/* see btrfs_writepage_start_hook for details on why this is required */
struct btrfs_writepage_fixup {
	struct page *page;
	struct btrfs_work work;
};

static void btrfs_writepage_fixup_worker(struct btrfs_work *work)
{
	struct btrfs_writepage_fixup *fixup;
	struct btrfs_ordered_extent *ordered;
	struct page *page;
	struct inode *inode;
	u64 page_start;
	u64 page_end;

	fixup = container_of(work, struct btrfs_writepage_fixup, work);
	page = fixup->page;
again:
	lock_page(page);
	if (!page->mapping || !PageDirty(page) || !PageChecked(page)) {
		ClearPageChecked(page);
		goto out_page;
	}

	inode = page->mapping->host;
	page_start = page_offset(page);
	page_end = page_offset(page) + PAGE_CACHE_SIZE - 1;

	lock_extent(&BTRFS_I(inode)->io_tree, page_start, page_end, GFP_NOFS);

	/* already ordered? We're done */
	if (PagePrivate2(page))
		goto out;

	ordered = btrfs_lookup_ordered_extent(inode, page_start);
	if (ordered) {
		unlock_extent(&BTRFS_I(inode)->io_tree, page_start,
			      page_end, GFP_NOFS);
		unlock_page(page);
		btrfs_start_ordered_extent(inode, ordered, 1);
		goto again;
	}

	btrfs_set_extent_delalloc(inode, page_start, page_end);
	ClearPageChecked(page);
out:
	unlock_extent(&BTRFS_I(inode)->io_tree, page_start, page_end, GFP_NOFS);
out_page:
	unlock_page(page);
	page_cache_release(page);
}

/*
 * There are a few paths in the higher layers of the kernel that directly
 * set the page dirty bit without asking the filesystem if it is a
 * good idea.  This causes problems because we want to make sure COW
 * properly happens and the data=ordered rules are followed.
 *
 * In our case any range that doesn't have the ORDERED bit set
 * hasn't been properly setup for IO.  We kick off an async process
 * to fix it up.  The async helper will wait for ordered extents, set
 * the delalloc bit and make it safe to write the page.
 */
static int btrfs_writepage_start_hook(struct page *page, u64 start, u64 end)
{
	struct inode *inode = page->mapping->host;
	struct btrfs_writepage_fixup *fixup;
	struct btrfs_root *root = BTRFS_I(inode)->root;

	/* this page is properly in the ordered list */
	if (TestClearPagePrivate2(page))
		return 0;

	if (PageChecked(page))
		return -EAGAIN;

	fixup = kzalloc(sizeof(*fixup), GFP_NOFS);
	if (!fixup)
		return -EAGAIN;

	SetPageChecked(page);
	page_cache_get(page);
	fixup->work.func = btrfs_writepage_fixup_worker;
	fixup->page = page;
	btrfs_queue_worker(&root->fs_info->fixup_workers, &fixup->work);
	return -EAGAIN;
}

static int insert_reserved_file_extent(struct btrfs_trans_handle *trans,
				       struct inode *inode, u64 file_pos,
				       u64 disk_bytenr, u64 disk_num_bytes,
				       u64 num_bytes, u64 ram_bytes,
				       u64 locked_end,
				       u8 compression, u8 encryption,
				       u16 other_encoding, int extent_type)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_file_extent_item *fi;
	struct btrfs_path *path;
	struct extent_buffer *leaf;
	struct btrfs_key ins;
	u64 hint;
	int ret;

	path = btrfs_alloc_path();
	BUG_ON(!path);

	path->leave_spinning = 1;

	/*
	 * we may be replacing one extent in the tree with another.
	 * The new extent is pinned in the extent map, and we don't want
	 * to drop it from the cache until it is completely in the btree.
	 *
	 * So, tell btrfs_drop_extents to leave this extent in the cache.
	 * the caller is expected to unpin it and allow it to be merged
	 * with the others.
	 */
	ret = btrfs_drop_extents(trans, root, inode, file_pos,
				 file_pos + num_bytes, locked_end,
				 file_pos, &hint, 0);
	BUG_ON(ret);

	ins.objectid = inode->i_ino;
	ins.offset = file_pos;
	ins.type = BTRFS_EXTENT_DATA_KEY;
	ret = btrfs_insert_empty_item(trans, root, path, &ins, sizeof(*fi));
	BUG_ON(ret);
	leaf = path->nodes[0];
	fi = btrfs_item_ptr(leaf, path->slots[0],
			    struct btrfs_file_extent_item);
	btrfs_set_file_extent_generation(leaf, fi, trans->transid);
	btrfs_set_file_extent_type(leaf, fi, extent_type);
	btrfs_set_file_extent_disk_bytenr(leaf, fi, disk_bytenr);
	btrfs_set_file_extent_disk_num_bytes(leaf, fi, disk_num_bytes);
	btrfs_set_file_extent_offset(leaf, fi, 0);
	btrfs_set_file_extent_num_bytes(leaf, fi, num_bytes);
	btrfs_set_file_extent_ram_bytes(leaf, fi, ram_bytes);
	btrfs_set_file_extent_compression(leaf, fi, compression);
	btrfs_set_file_extent_encryption(leaf, fi, encryption);
	btrfs_set_file_extent_other_encoding(leaf, fi, other_encoding);

	btrfs_unlock_up_safe(path, 1);
	btrfs_set_lock_blocking(leaf);

	btrfs_mark_buffer_dirty(leaf);

	inode_add_bytes(inode, num_bytes);

	ins.objectid = disk_bytenr;
	ins.offset = disk_num_bytes;
	ins.type = BTRFS_EXTENT_ITEM_KEY;
	ret = btrfs_alloc_reserved_file_extent(trans, root,
					root->root_key.objectid,
					inode->i_ino, file_pos, &ins);
	BUG_ON(ret);
	btrfs_free_path(path);

	return 0;
}

/*
 * helper function for btrfs_finish_ordered_io, this
 * just reads in some of the csum leaves to prime them into ram
 * before we start the transaction.  It limits the amount of btree
 * reads required while inside the transaction.
 */
static noinline void reada_csum(struct btrfs_root *root,
				struct btrfs_path *path,
				struct btrfs_ordered_extent *ordered_extent)
{
	struct btrfs_ordered_sum *sum;
	u64 bytenr;

	sum = list_entry(ordered_extent->list.next, struct btrfs_ordered_sum,
			 list);
	bytenr = sum->sums[0].bytenr;

	/*
	 * we don't care about the results, the point of this search is
	 * just to get the btree leaves into ram
	 */
	btrfs_lookup_csum(NULL, root->fs_info->csum_root, path, bytenr, 0);
}

/* as ordered data IO finishes, this gets called so we can finish
 * an ordered extent if the range of bytes in the file it covers are
 * fully written.
 */
static int btrfs_finish_ordered_io(struct inode *inode, u64 start, u64 end)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_trans_handle *trans;
	struct btrfs_ordered_extent *ordered_extent = NULL;
	struct extent_io_tree *io_tree = &BTRFS_I(inode)->io_tree;
	struct btrfs_path *path;
	int compressed = 0;
	int ret;

	ret = btrfs_dec_test_ordered_pending(inode, start, end - start + 1);
	if (!ret)
		return 0;

	/*
	 * before we join the transaction, try to do some of our IO.
	 * This will limit the amount of IO that we have to do with
	 * the transaction running.  We're unlikely to need to do any
	 * IO if the file extents are new, the disk_i_size checks
	 * covers the most common case.
	 */
	if (start < BTRFS_I(inode)->disk_i_size) {
		path = btrfs_alloc_path();
		if (path) {
			ret = btrfs_lookup_file_extent(NULL, root, path,
						       inode->i_ino,
						       start, 0);
			ordered_extent = btrfs_lookup_ordered_extent(inode,
								     start);
			if (!list_empty(&ordered_extent->list)) {
				btrfs_release_path(root, path);
				reada_csum(root, path, ordered_extent);
			}
			btrfs_free_path(path);
		}
	}

	trans = btrfs_join_transaction(root, 1);

	if (!ordered_extent)
		ordered_extent = btrfs_lookup_ordered_extent(inode, start);
	BUG_ON(!ordered_extent);
	if (test_bit(BTRFS_ORDERED_NOCOW, &ordered_extent->flags))
		goto nocow;

	lock_extent(io_tree, ordered_extent->file_offset,
		    ordered_extent->file_offset + ordered_extent->len - 1,
		    GFP_NOFS);

	if (test_bit(BTRFS_ORDERED_COMPRESSED, &ordered_extent->flags))
		compressed = 1;
	if (test_bit(BTRFS_ORDERED_PREALLOC, &ordered_extent->flags)) {
		BUG_ON(compressed);
		ret = btrfs_mark_extent_written(trans, root, inode,
						ordered_extent->file_offset,
						ordered_extent->file_offset +
						ordered_extent->len);
		BUG_ON(ret);
	} else {
		ret = insert_reserved_file_extent(trans, inode,
						ordered_extent->file_offset,
						ordered_extent->start,
						ordered_extent->disk_len,
						ordered_extent->len,
						ordered_extent->len,
						ordered_extent->file_offset +
						ordered_extent->len,
						compressed, 0, 0,
						BTRFS_FILE_EXTENT_REG);
		unpin_extent_cache(&BTRFS_I(inode)->extent_tree,
				   ordered_extent->file_offset,
				   ordered_extent->len);
		BUG_ON(ret);
	}
	unlock_extent(io_tree, ordered_extent->file_offset,
		    ordered_extent->file_offset + ordered_extent->len - 1,
		    GFP_NOFS);
nocow:
	add_pending_csums(trans, inode, ordered_extent->file_offset,
			  &ordered_extent->list);

	mutex_lock(&BTRFS_I(inode)->extent_mutex);
	btrfs_ordered_update_i_size(inode, ordered_extent);
	btrfs_update_inode(trans, root, inode);
	btrfs_remove_ordered_extent(inode, ordered_extent);
	mutex_unlock(&BTRFS_I(inode)->extent_mutex);

	/* once for us */
	btrfs_put_ordered_extent(ordered_extent);
	/* once for the tree */
	btrfs_put_ordered_extent(ordered_extent);

	btrfs_end_transaction(trans, root);
	return 0;
}

static int btrfs_writepage_end_io_hook(struct page *page, u64 start, u64 end,
				struct extent_state *state, int uptodate)
{
	ClearPagePrivate2(page);
	return btrfs_finish_ordered_io(page->mapping->host, start, end);
}

/*
 * When IO fails, either with EIO or csum verification fails, we
 * try other mirrors that might have a good copy of the data.  This
 * io_failure_record is used to record state as we go through all the
 * mirrors.  If another mirror has good data, the page is set up to date
 * and things continue.  If a good mirror can't be found, the original
 * bio end_io callback is called to indicate things have failed.
 */
struct io_failure_record {
	struct page *page;
	u64 start;
	u64 len;
	u64 logical;
	unsigned long bio_flags;
	int last_mirror;
};

static int btrfs_io_failed_hook(struct bio *failed_bio,
			 struct page *page, u64 start, u64 end,
			 struct extent_state *state)
{
	struct io_failure_record *failrec = NULL;
	u64 private;
	struct extent_map *em;
	struct inode *inode = page->mapping->host;
	struct extent_io_tree *failure_tree = &BTRFS_I(inode)->io_failure_tree;
	struct extent_map_tree *em_tree = &BTRFS_I(inode)->extent_tree;
	struct bio *bio;
	int num_copies;
	int ret;
	int rw;
	u64 logical;

	ret = get_state_private(failure_tree, start, &private);
	if (ret) {
		failrec = kmalloc(sizeof(*failrec), GFP_NOFS);
		if (!failrec)
			return -ENOMEM;
		failrec->start = start;
		failrec->len = end - start + 1;
		failrec->last_mirror = 0;
		failrec->bio_flags = 0;

		read_lock(&em_tree->lock);
		em = lookup_extent_mapping(em_tree, start, failrec->len);
		if (em->start > start || em->start + em->len < start) {
			free_extent_map(em);
			em = NULL;
		}
		read_unlock(&em_tree->lock);

		if (!em || IS_ERR(em)) {
			kfree(failrec);
			return -EIO;
		}
		logical = start - em->start;
		logical = em->block_start + logical;
		if (test_bit(EXTENT_FLAG_COMPRESSED, &em->flags)) {
			logical = em->block_start;
			failrec->bio_flags = EXTENT_BIO_COMPRESSED;
		}
		failrec->logical = logical;
		free_extent_map(em);
		set_extent_bits(failure_tree, start, end, EXTENT_LOCKED |
				EXTENT_DIRTY, GFP_NOFS);
		set_state_private(failure_tree, start,
				 (u64)(unsigned long)failrec);
	} else {
		failrec = (struct io_failure_record *)(unsigned long)private;
	}
	num_copies = btrfs_num_copies(
			      &BTRFS_I(inode)->root->fs_info->mapping_tree,
			      failrec->logical, failrec->len);
	failrec->last_mirror++;
	if (!state) {
		spin_lock(&BTRFS_I(inode)->io_tree.lock);
		state = find_first_extent_bit_state(&BTRFS_I(inode)->io_tree,
						    failrec->start,
						    EXTENT_LOCKED);
		if (state && state->start != failrec->start)
			state = NULL;
		spin_unlock(&BTRFS_I(inode)->io_tree.lock);
	}
	if (!state || failrec->last_mirror > num_copies) {
		set_state_private(failure_tree, failrec->start, 0);
		clear_extent_bits(failure_tree, failrec->start,
				  failrec->start + failrec->len - 1,
				  EXTENT_LOCKED | EXTENT_DIRTY, GFP_NOFS);
		kfree(failrec);
		return -EIO;
	}
	bio = bio_alloc(GFP_NOFS, 1);
	bio->bi_private = state;
	bio->bi_end_io = failed_bio->bi_end_io;
	bio->bi_sector = failrec->logical >> 9;
	bio->bi_bdev = failed_bio->bi_bdev;
	bio->bi_size = 0;

	bio_add_page(bio, page, failrec->len, start - page_offset(page));
	if (failed_bio->bi_rw & (1 << BIO_RW))
		rw = WRITE;
	else
		rw = READ;

	BTRFS_I(inode)->io_tree.ops->submit_bio_hook(inode, rw, bio,
						      failrec->last_mirror,
						      failrec->bio_flags);
	return 0;
}

/*
 * each time an IO finishes, we do a fast check in the IO failure tree
 * to see if we need to process or clean up an io_failure_record
 */
static int btrfs_clean_io_failures(struct inode *inode, u64 start)
{
	u64 private;
	u64 private_failure;
	struct io_failure_record *failure;
	int ret;

	private = 0;
	if (count_range_bits(&BTRFS_I(inode)->io_failure_tree, &private,
			     (u64)-1, 1, EXTENT_DIRTY)) {
		ret = get_state_private(&BTRFS_I(inode)->io_failure_tree,
					start, &private_failure);
		if (ret == 0) {
			failure = (struct io_failure_record *)(unsigned long)
				   private_failure;
			set_state_private(&BTRFS_I(inode)->io_failure_tree,
					  failure->start, 0);
			clear_extent_bits(&BTRFS_I(inode)->io_failure_tree,
					  failure->start,
					  failure->start + failure->len - 1,
					  EXTENT_DIRTY | EXTENT_LOCKED,
					  GFP_NOFS);
			kfree(failure);
		}
	}
	return 0;
}

/*
 * when reads are done, we need to check csums to verify the data is correct
 * if there's a match, we allow the bio to finish.  If not, we go through
 * the io_failure_record routines to find good copies
 */
static int btrfs_readpage_end_io_hook(struct page *page, u64 start, u64 end,
			       struct extent_state *state)
{
	size_t offset = start - ((u64)page->index << PAGE_CACHE_SHIFT);
	struct inode *inode = page->mapping->host;
	struct extent_io_tree *io_tree = &BTRFS_I(inode)->io_tree;
	char *kaddr;
	u64 private = ~(u32)0;
	int ret;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	u32 csum = ~(u32)0;

	if (PageChecked(page)) {
		ClearPageChecked(page);
		goto good;
	}

	if (BTRFS_I(inode)->flags & BTRFS_INODE_NODATASUM)
		return 0;

	if (root->root_key.objectid == BTRFS_DATA_RELOC_TREE_OBJECTID &&
	    test_range_bit(io_tree, start, end, EXTENT_NODATASUM, 1, NULL)) {
		clear_extent_bits(io_tree, start, end, EXTENT_NODATASUM,
				  GFP_NOFS);
		return 0;
	}

	if (state && state->start == start) {
		private = state->private;
		ret = 0;
	} else {
		ret = get_state_private(io_tree, start, &private);
	}
	kaddr = kmap_atomic(page, KM_USER0);
	if (ret)
		goto zeroit;

	csum = btrfs_csum_data(root, kaddr + offset, csum,  end - start + 1);
	btrfs_csum_final(csum, (char *)&csum);
	if (csum != private)
		goto zeroit;

	kunmap_atomic(kaddr, KM_USER0);
good:
	/* if the io failure tree for this inode is non-empty,
	 * check to see if we've recovered from a failed IO
	 */
	btrfs_clean_io_failures(inode, start);
	return 0;

zeroit:
	if (printk_ratelimit()) {
		printk(KERN_INFO "btrfs csum failed ino %lu off %llu csum %u "
		       "private %llu\n", page->mapping->host->i_ino,
		       (unsigned long long)start, csum,
		       (unsigned long long)private);
	}
	memset(kaddr + offset, 1, end - start + 1);
	flush_dcache_page(page);
	kunmap_atomic(kaddr, KM_USER0);
	if (private == 0)
		return 0;
	return -EIO;
}

/*
 * This creates an orphan entry for the given inode in case something goes
 * wrong in the middle of an unlink/truncate.
 */
int btrfs_orphan_add(struct btrfs_trans_handle *trans, struct inode *inode)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	int ret = 0;

	spin_lock(&root->list_lock);

	/* already on the orphan list, we're good */
	if (!list_empty(&BTRFS_I(inode)->i_orphan)) {
		spin_unlock(&root->list_lock);
		return 0;
	}

	list_add(&BTRFS_I(inode)->i_orphan, &root->orphan_list);

	spin_unlock(&root->list_lock);

	/*
	 * insert an orphan item to track this unlinked/truncated file
	 */
	ret = btrfs_insert_orphan_item(trans, root, inode->i_ino);

	return ret;
}

/*
 * We have done the truncate/delete so we can go ahead and remove the orphan
 * item for this particular inode.
 */
int btrfs_orphan_del(struct btrfs_trans_handle *trans, struct inode *inode)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	int ret = 0;

	spin_lock(&root->list_lock);

	if (list_empty(&BTRFS_I(inode)->i_orphan)) {
		spin_unlock(&root->list_lock);
		return 0;
	}

	list_del_init(&BTRFS_I(inode)->i_orphan);
	if (!trans) {
		spin_unlock(&root->list_lock);
		return 0;
	}

	spin_unlock(&root->list_lock);

	ret = btrfs_del_orphan_item(trans, root, inode->i_ino);

	return ret;
}

/*
 * this cleans up any orphans that may be left on the list from the last use
 * of this root.
 */
void btrfs_orphan_cleanup(struct btrfs_root *root)
{
	struct btrfs_path *path;
	struct extent_buffer *leaf;
	struct btrfs_item *item;
	struct btrfs_key key, found_key;
	struct btrfs_trans_handle *trans;
	struct inode *inode;
	int ret = 0, nr_unlink = 0, nr_truncate = 0;

	path = btrfs_alloc_path();
	if (!path)
		return;
	path->reada = -1;

	key.objectid = BTRFS_ORPHAN_OBJECTID;
	btrfs_set_key_type(&key, BTRFS_ORPHAN_ITEM_KEY);
	key.offset = (u64)-1;


	while (1) {
		ret = btrfs_search_slot(NULL, root, &key, path, 0, 0);
		if (ret < 0) {
			printk(KERN_ERR "Error searching slot for orphan: %d"
			       "\n", ret);
			break;
		}

		/*
		 * if ret == 0 means we found what we were searching for, which
		 * is weird, but possible, so only screw with path if we didnt
		 * find the key and see if we have stuff that matches
		 */
		if (ret > 0) {
			if (path->slots[0] == 0)
				break;
			path->slots[0]--;
		}

		/* pull out the item */
		leaf = path->nodes[0];
		item = btrfs_item_nr(leaf, path->slots[0]);
		btrfs_item_key_to_cpu(leaf, &found_key, path->slots[0]);

		/* make sure the item matches what we want */
		if (found_key.objectid != BTRFS_ORPHAN_OBJECTID)
			break;
		if (btrfs_key_type(&found_key) != BTRFS_ORPHAN_ITEM_KEY)
			break;

		/* release the path since we're done with it */
		btrfs_release_path(root, path);

		/*
		 * this is where we are basically btrfs_lookup, without the
		 * crossing root thing.  we store the inode number in the
		 * offset of the orphan item.
		 */
		found_key.objectid = found_key.offset;
		found_key.type = BTRFS_INODE_ITEM_KEY;
		found_key.offset = 0;
		inode = btrfs_iget(root->fs_info->sb, &found_key, root);
		if (IS_ERR(inode))
			break;

		/*
		 * add this inode to the orphan list so btrfs_orphan_del does
		 * the proper thing when we hit it
		 */
		spin_lock(&root->list_lock);
		list_add(&BTRFS_I(inode)->i_orphan, &root->orphan_list);
		spin_unlock(&root->list_lock);

		/*
		 * if this is a bad inode, means we actually succeeded in
		 * removing the inode, but not the orphan record, which means
		 * we need to manually delete the orphan since iput will just
		 * do a destroy_inode
		 */
		if (is_bad_inode(inode)) {
			trans = btrfs_start_transaction(root, 1);
			btrfs_orphan_del(trans, inode);
			btrfs_end_transaction(trans, root);
			iput(inode);
			continue;
		}

		/* if we have links, this was a truncate, lets do that */
		if (inode->i_nlink) {
			nr_truncate++;
			btrfs_truncate(inode);
		} else {
			nr_unlink++;
		}

		/* this will do delete_inode and everything for us */
		iput(inode);
	}

	if (nr_unlink)
		printk(KERN_INFO "btrfs: unlinked %d orphans\n", nr_unlink);
	if (nr_truncate)
		printk(KERN_INFO "btrfs: truncated %d orphans\n", nr_truncate);

	btrfs_free_path(path);
}

/*
 * very simple check to peek ahead in the leaf looking for xattrs.  If we
 * don't find any xattrs, we know there can't be any acls.
 *
 * slot is the slot the inode is in, objectid is the objectid of the inode
 */
static noinline int acls_after_inode_item(struct extent_buffer *leaf,
					  int slot, u64 objectid)
{
	u32 nritems = btrfs_header_nritems(leaf);
	struct btrfs_key found_key;
	int scanned = 0;

	slot++;
	while (slot < nritems) {
		btrfs_item_key_to_cpu(leaf, &found_key, slot);

		/* we found a different objectid, there must not be acls */
		if (found_key.objectid != objectid)
			return 0;

		/* we found an xattr, assume we've got an acl */
		if (found_key.type == BTRFS_XATTR_ITEM_KEY)
			return 1;

		/*
		 * we found a key greater than an xattr key, there can't
		 * be any acls later on
		 */
		if (found_key.type > BTRFS_XATTR_ITEM_KEY)
			return 0;

		slot++;
		scanned++;

		/*
		 * it goes inode, inode backrefs, xattrs, extents,
		 * so if there are a ton of hard links to an inode there can
		 * be a lot of backrefs.  Don't waste time searching too hard,
		 * this is just an optimization
		 */
		if (scanned >= 8)
			break;
	}
	/* we hit the end of the leaf before we found an xattr or
	 * something larger than an xattr.  We have to assume the inode
	 * has acls
	 */
	return 1;
}

/*
 * read an inode from the btree into the in-memory inode
 */
static void btrfs_read_locked_inode(struct inode *inode)
{
	struct btrfs_path *path;
	struct extent_buffer *leaf;
	struct btrfs_inode_item *inode_item;
	struct btrfs_timespec *tspec;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_key location;
	int maybe_acls;
	u64 alloc_group_block;
	u32 rdev;
	int ret;

	path = btrfs_alloc_path();
	BUG_ON(!path);
	memcpy(&location, &BTRFS_I(inode)->location, sizeof(location));

	ret = btrfs_lookup_inode(NULL, root, path, &location, 0);
	if (ret)
		goto make_bad;

	leaf = path->nodes[0];
	inode_item = btrfs_item_ptr(leaf, path->slots[0],
				    struct btrfs_inode_item);

	inode->i_mode = btrfs_inode_mode(leaf, inode_item);
	inode->i_nlink = btrfs_inode_nlink(leaf, inode_item);
	inode->i_uid = btrfs_inode_uid(leaf, inode_item);
	inode->i_gid = btrfs_inode_gid(leaf, inode_item);
	btrfs_i_size_write(inode, btrfs_inode_size(leaf, inode_item));

	tspec = btrfs_inode_atime(inode_item);
	inode->i_atime.tv_sec = btrfs_timespec_sec(leaf, tspec);
	inode->i_atime.tv_nsec = btrfs_timespec_nsec(leaf, tspec);

	tspec = btrfs_inode_mtime(inode_item);
	inode->i_mtime.tv_sec = btrfs_timespec_sec(leaf, tspec);
	inode->i_mtime.tv_nsec = btrfs_timespec_nsec(leaf, tspec);

	tspec = btrfs_inode_ctime(inode_item);
	inode->i_ctime.tv_sec = btrfs_timespec_sec(leaf, tspec);
	inode->i_ctime.tv_nsec = btrfs_timespec_nsec(leaf, tspec);

	inode_set_bytes(inode, btrfs_inode_nbytes(leaf, inode_item));
	BTRFS_I(inode)->generation = btrfs_inode_generation(leaf, inode_item);
	BTRFS_I(inode)->sequence = btrfs_inode_sequence(leaf, inode_item);
	inode->i_generation = BTRFS_I(inode)->generation;
	inode->i_rdev = 0;
	rdev = btrfs_inode_rdev(leaf, inode_item);

	BTRFS_I(inode)->index_cnt = (u64)-1;
	BTRFS_I(inode)->flags = btrfs_inode_flags(leaf, inode_item);

	alloc_group_block = btrfs_inode_block_group(leaf, inode_item);

	/*
	 * try to precache a NULL acl entry for files that don't have
	 * any xattrs or acls
	 */
	maybe_acls = acls_after_inode_item(leaf, path->slots[0], inode->i_ino);
	if (!maybe_acls)
		cache_no_acl(inode);

	BTRFS_I(inode)->block_group = btrfs_find_block_group(root, 0,
						alloc_group_block, 0);
	btrfs_free_path(path);
	inode_item = NULL;

	switch (inode->i_mode & S_IFMT) {
	case S_IFREG:
		inode->i_mapping->a_ops = &btrfs_aops;
		inode->i_mapping->backing_dev_info = &root->fs_info->bdi;
		BTRFS_I(inode)->io_tree.ops = &btrfs_extent_io_ops;
		inode->i_fop = &btrfs_file_operations;
		inode->i_op = &btrfs_file_inode_operations;
		break;
	case S_IFDIR:
		inode->i_fop = &btrfs_dir_file_operations;
		if (root == root->fs_info->tree_root)
			inode->i_op = &btrfs_dir_ro_inode_operations;
		else
			inode->i_op = &btrfs_dir_inode_operations;
		break;
	case S_IFLNK:
		inode->i_op = &btrfs_symlink_inode_operations;
		inode->i_mapping->a_ops = &btrfs_symlink_aops;
		inode->i_mapping->backing_dev_info = &root->fs_info->bdi;
		break;
	default:
		inode->i_op = &btrfs_special_inode_operations;
		init_special_inode(inode, inode->i_mode, rdev);
		break;
	}

	btrfs_update_iflags(inode);
	return;

make_bad:
	btrfs_free_path(path);
	make_bad_inode(inode);
}

/*
 * given a leaf and an inode, copy the inode fields into the leaf
 */
static void fill_inode_item(struct btrfs_trans_handle *trans,
			    struct extent_buffer *leaf,
			    struct btrfs_inode_item *item,
			    struct inode *inode)
{
	btrfs_set_inode_uid(leaf, item, inode->i_uid);
	btrfs_set_inode_gid(leaf, item, inode->i_gid);
	btrfs_set_inode_size(leaf, item, BTRFS_I(inode)->disk_i_size);
	btrfs_set_inode_mode(leaf, item, inode->i_mode);
	btrfs_set_inode_nlink(leaf, item, inode->i_nlink);

	btrfs_set_timespec_sec(leaf, btrfs_inode_atime(item),
			       inode->i_atime.tv_sec);
	btrfs_set_timespec_nsec(leaf, btrfs_inode_atime(item),
				inode->i_atime.tv_nsec);

	btrfs_set_timespec_sec(leaf, btrfs_inode_mtime(item),
			       inode->i_mtime.tv_sec);
	btrfs_set_timespec_nsec(leaf, btrfs_inode_mtime(item),
				inode->i_mtime.tv_nsec);

	btrfs_set_timespec_sec(leaf, btrfs_inode_ctime(item),
			       inode->i_ctime.tv_sec);
	btrfs_set_timespec_nsec(leaf, btrfs_inode_ctime(item),
				inode->i_ctime.tv_nsec);

	btrfs_set_inode_nbytes(leaf, item, inode_get_bytes(inode));
	btrfs_set_inode_generation(leaf, item, BTRFS_I(inode)->generation);
	btrfs_set_inode_sequence(leaf, item, BTRFS_I(inode)->sequence);
	btrfs_set_inode_transid(leaf, item, trans->transid);
	btrfs_set_inode_rdev(leaf, item, inode->i_rdev);
	btrfs_set_inode_flags(leaf, item, BTRFS_I(inode)->flags);
	btrfs_set_inode_block_group(leaf, item, BTRFS_I(inode)->block_group);
}

/*
 * copy everything in the in-memory inode into the btree.
 */
noinline int btrfs_update_inode(struct btrfs_trans_handle *trans,
				struct btrfs_root *root, struct inode *inode)
{
	struct btrfs_inode_item *inode_item;
	struct btrfs_path *path;
	struct extent_buffer *leaf;
	int ret;

	path = btrfs_alloc_path();
	BUG_ON(!path);
	path->leave_spinning = 1;
	ret = btrfs_lookup_inode(trans, root, path,
				 &BTRFS_I(inode)->location, 1);
	if (ret) {
		if (ret > 0)
			ret = -ENOENT;
		goto failed;
	}

	btrfs_unlock_up_safe(path, 1);
	leaf = path->nodes[0];
	inode_item = btrfs_item_ptr(leaf, path->slots[0],
				  struct btrfs_inode_item);

	fill_inode_item(trans, leaf, inode_item, inode);
	btrfs_mark_buffer_dirty(leaf);
	btrfs_set_inode_last_trans(trans, inode);
	ret = 0;
failed:
	btrfs_free_path(path);
	return ret;
}


/*
 * unlink helper that gets used here in inode.c and in the tree logging
 * recovery code.  It remove a link in a directory with a given name, and
 * also drops the back refs in the inode to the directory
 */
int btrfs_unlink_inode(struct btrfs_trans_handle *trans,
		       struct btrfs_root *root,
		       struct inode *dir, struct inode *inode,
		       const char *name, int name_len)
{
	struct btrfs_path *path;
	int ret = 0;
	struct extent_buffer *leaf;
	struct btrfs_dir_item *di;
	struct btrfs_key key;
	u64 index;

	path = btrfs_alloc_path();
	if (!path) {
		ret = -ENOMEM;
		goto err;
	}

	path->leave_spinning = 1;
	di = btrfs_lookup_dir_item(trans, root, path, dir->i_ino,
				    name, name_len, -1);
	if (IS_ERR(di)) {
		ret = PTR_ERR(di);
		goto err;
	}
	if (!di) {
		ret = -ENOENT;
		goto err;
	}
	leaf = path->nodes[0];
	btrfs_dir_item_key_to_cpu(leaf, di, &key);
	ret = btrfs_delete_one_dir_name(trans, root, path, di);
	if (ret)
		goto err;
	btrfs_release_path(root, path);

	ret = btrfs_del_inode_ref(trans, root, name, name_len,
				  inode->i_ino,
				  dir->i_ino, &index);
	if (ret) {
		printk(KERN_INFO "btrfs failed to delete reference to %.*s, "
		       "inode %lu parent %lu\n", name_len, name,
		       inode->i_ino, dir->i_ino);
		goto err;
	}

	di = btrfs_lookup_dir_index_item(trans, root, path, dir->i_ino,
					 index, name, name_len, -1);
	if (IS_ERR(di)) {
		ret = PTR_ERR(di);
		goto err;
	}
	if (!di) {
		ret = -ENOENT;
		goto err;
	}
	ret = btrfs_delete_one_dir_name(trans, root, path, di);
	btrfs_release_path(root, path);

	ret = btrfs_del_inode_ref_in_log(trans, root, name, name_len,
					 inode, dir->i_ino);
	BUG_ON(ret != 0 && ret != -ENOENT);

	ret = btrfs_del_dir_entries_in_log(trans, root, name, name_len,
					   dir, index);
	BUG_ON(ret);
err:
	btrfs_free_path(path);
	if (ret)
		goto out;

	btrfs_i_size_write(dir, dir->i_size - name_len * 2);
	inode->i_ctime = dir->i_mtime = dir->i_ctime = CURRENT_TIME;
	btrfs_update_inode(trans, root, dir);
	btrfs_drop_nlink(inode);
	ret = btrfs_update_inode(trans, root, inode);
out:
	return ret;
}

static int btrfs_unlink(struct inode *dir, struct dentry *dentry)
{
	struct btrfs_root *root;
	struct btrfs_trans_handle *trans;
	struct inode *inode = dentry->d_inode;
	int ret;
	unsigned long nr = 0;

	root = BTRFS_I(dir)->root;

	/*
	 * 5 items for unlink inode
	 * 1 for orphan
	 */
	ret = btrfs_reserve_metadata_space(root, 6);
	if (ret)
		return ret;

	trans = btrfs_start_transaction(root, 1);
	if (IS_ERR(trans)) {
		btrfs_unreserve_metadata_space(root, 6);
		return PTR_ERR(trans);
	}

	btrfs_set_trans_block_group(trans, dir);

	btrfs_record_unlink_dir(trans, dir, dentry->d_inode, 0);

	ret = btrfs_unlink_inode(trans, root, dir, dentry->d_inode,
				 dentry->d_name.name, dentry->d_name.len);

	if (inode->i_nlink == 0)
		ret = btrfs_orphan_add(trans, inode);

	nr = trans->blocks_used;

	btrfs_end_transaction_throttle(trans, root);
	btrfs_unreserve_metadata_space(root, 6);
	btrfs_btree_balance_dirty(root, nr);
	return ret;
}

int btrfs_unlink_subvol(struct btrfs_trans_handle *trans,
			struct btrfs_root *root,
			struct inode *dir, u64 objectid,
			const char *name, int name_len)
{
	struct btrfs_path *path;
	struct extent_buffer *leaf;
	struct btrfs_dir_item *di;
	struct btrfs_key key;
	u64 index;
	int ret;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	di = btrfs_lookup_dir_item(trans, root, path, dir->i_ino,
				   name, name_len, -1);
	BUG_ON(!di || IS_ERR(di));

	leaf = path->nodes[0];
	btrfs_dir_item_key_to_cpu(leaf, di, &key);
	WARN_ON(key.type != BTRFS_ROOT_ITEM_KEY || key.objectid != objectid);
	ret = btrfs_delete_one_dir_name(trans, root, path, di);
	BUG_ON(ret);
	btrfs_release_path(root, path);

	ret = btrfs_del_root_ref(trans, root->fs_info->tree_root,
				 objectid, root->root_key.objectid,
				 dir->i_ino, &index, name, name_len);
	if (ret < 0) {
		BUG_ON(ret != -ENOENT);
		di = btrfs_search_dir_index_item(root, path, dir->i_ino,
						 name, name_len);
		BUG_ON(!di || IS_ERR(di));

		leaf = path->nodes[0];
		btrfs_item_key_to_cpu(leaf, &key, path->slots[0]);
		btrfs_release_path(root, path);
		index = key.offset;
	}

	di = btrfs_lookup_dir_index_item(trans, root, path, dir->i_ino,
					 index, name, name_len, -1);
	BUG_ON(!di || IS_ERR(di));

	leaf = path->nodes[0];
	btrfs_dir_item_key_to_cpu(leaf, di, &key);
	WARN_ON(key.type != BTRFS_ROOT_ITEM_KEY || key.objectid != objectid);
	ret = btrfs_delete_one_dir_name(trans, root, path, di);
	BUG_ON(ret);
	btrfs_release_path(root, path);

	btrfs_i_size_write(dir, dir->i_size - name_len * 2);
	dir->i_mtime = dir->i_ctime = CURRENT_TIME;
	ret = btrfs_update_inode(trans, root, dir);
	BUG_ON(ret);
	dir->i_sb->s_dirt = 1;

	btrfs_free_path(path);
	return 0;
}

static int btrfs_rmdir(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = dentry->d_inode;
	int err = 0;
	int ret;
	struct btrfs_root *root = BTRFS_I(dir)->root;
	struct btrfs_trans_handle *trans;
	unsigned long nr = 0;

	if (inode->i_size > BTRFS_EMPTY_DIR_SIZE ||
	    inode->i_ino == BTRFS_FIRST_FREE_OBJECTID)
		return -ENOTEMPTY;

	ret = btrfs_reserve_metadata_space(root, 5);
	if (ret)
		return ret;

	trans = btrfs_start_transaction(root, 1);
	if (IS_ERR(trans)) {
		btrfs_unreserve_metadata_space(root, 5);
		return PTR_ERR(trans);
	}

	btrfs_set_trans_block_group(trans, dir);

	if (unlikely(inode->i_ino == BTRFS_EMPTY_SUBVOL_DIR_OBJECTID)) {
		err = btrfs_unlink_subvol(trans, root, dir,
					  BTRFS_I(inode)->location.objectid,
					  dentry->d_name.name,
					  dentry->d_name.len);
		goto out;
	}

	err = btrfs_orphan_add(trans, inode);
	if (err)
		goto out;

	/* now the directory is empty */
	err = btrfs_unlink_inode(trans, root, dir, dentry->d_inode,
				 dentry->d_name.name, dentry->d_name.len);
	if (!err)
		btrfs_i_size_write(inode, 0);
out:
	nr = trans->blocks_used;
	ret = btrfs_end_transaction_throttle(trans, root);
	btrfs_unreserve_metadata_space(root, 5);
	btrfs_btree_balance_dirty(root, nr);

	if (ret && !err)
		err = ret;
	return err;
}

#if 0
/*
 * when truncating bytes in a file, it is possible to avoid reading
 * the leaves that contain only checksum items.  This can be the
 * majority of the IO required to delete a large file, but it must
 * be done carefully.
 *
 * The keys in the level just above the leaves are checked to make sure
 * the lowest key in a given leaf is a csum key, and starts at an offset
 * after the new  size.
 *
 * Then the key for the next leaf is checked to make sure it also has
 * a checksum item for the same file.  If it does, we know our target leaf
 * contains only checksum items, and it can be safely freed without reading
 * it.
 *
 * This is just an optimization targeted at large files.  It may do
 * nothing.  It will return 0 unless things went badly.
 */
static noinline int drop_csum_leaves(struct btrfs_trans_handle *trans,
				     struct btrfs_root *root,
				     struct btrfs_path *path,
				     struct inode *inode, u64 new_size)
{
	struct btrfs_key key;
	int ret;
	int nritems;
	struct btrfs_key found_key;
	struct btrfs_key other_key;
	struct btrfs_leaf_ref *ref;
	u64 leaf_gen;
	u64 leaf_start;

	path->lowest_level = 1;
	key.objectid = inode->i_ino;
	key.type = BTRFS_CSUM_ITEM_KEY;
	key.offset = new_size;
again:
	ret = btrfs_search_slot(trans, root, &key, path, -1, 1);
	if (ret < 0)
		goto out;

	if (path->nodes[1] == NULL) {
		ret = 0;
		goto out;
	}
	ret = 0;
	btrfs_node_key_to_cpu(path->nodes[1], &found_key, path->slots[1]);
	nritems = btrfs_header_nritems(path->nodes[1]);

	if (!nritems)
		goto out;

	if (path->slots[1] >= nritems)
		goto next_node;

	/* did we find a key greater than anything we want to delete? */
	if (found_key.objectid > inode->i_ino ||
	   (found_key.objectid == inode->i_ino && found_key.type > key.type))
		goto out;

	/* we check the next key in the node to make sure the leave contains
	 * only checksum items.  This comparison doesn't work if our
	 * leaf is the last one in the node
	 */
	if (path->slots[1] + 1 >= nritems) {
next_node:
		/* search forward from the last key in the node, this
		 * will bring us into the next node in the tree
		 */
		btrfs_node_key_to_cpu(path->nodes[1], &found_key, nritems - 1);

		/* unlikely, but we inc below, so check to be safe */
		if (found_key.offset == (u64)-1)
			goto out;

		/* search_forward needs a path with locks held, do the
		 * search again for the original key.  It is possible
		 * this will race with a balance and return a path that
		 * we could modify, but this drop is just an optimization
		 * and is allowed to miss some leaves.
		 */
		btrfs_release_path(root, path);
		found_key.offset++;

		/* setup a max key for search_forward */
		other_key.offset = (u64)-1;
		other_key.type = key.type;
		other_key.objectid = key.objectid;

		path->keep_locks = 1;
		ret = btrfs_search_forward(root, &found_key, &other_key,
					   path, 0, 0);
		path->keep_locks = 0;
		if (ret || found_key.objectid != key.objectid ||
		    found_key.type != key.type) {
			ret = 0;
			goto out;
		}

		key.offset = found_key.offset;
		btrfs_release_path(root, path);
		cond_resched();
		goto again;
	}

	/* we know there's one more slot after us in the tree,
	 * read that key so we can verify it is also a checksum item
	 */
	btrfs_node_key_to_cpu(path->nodes[1], &other_key, path->slots[1] + 1);

	if (found_key.objectid < inode->i_ino)
		goto next_key;

	if (found_key.type != key.type || found_key.offset < new_size)
		goto next_key;

	/*
	 * if the key for the next leaf isn't a csum key from this objectid,
	 * we can't be sure there aren't good items inside this leaf.
	 * Bail out
	 */
	if (other_key.objectid != inode->i_ino || other_key.type != key.type)
		goto out;

	leaf_start = btrfs_node_blockptr(path->nodes[1], path->slots[1]);
	leaf_gen = btrfs_node_ptr_generation(path->nodes[1], path->slots[1]);
	/*
	 * it is safe to delete this leaf, it contains only
	 * csum items from this inode at an offset >= new_size
	 */
	ret = btrfs_del_leaf(trans, root, path, leaf_start);
	BUG_ON(ret);

	if (root->ref_cows && leaf_gen < trans->transid) {
		ref = btrfs_alloc_leaf_ref(root, 0);
		if (ref) {
			ref->root_gen = root->root_key.offset;
			ref->bytenr = leaf_start;
			ref->owner = 0;
			ref->generation = leaf_gen;
			ref->nritems = 0;

			btrfs_sort_leaf_ref(ref);

			ret = btrfs_add_leaf_ref(root, ref, 0);
			WARN_ON(ret);
			btrfs_free_leaf_ref(root, ref);
		} else {
			WARN_ON(1);
		}
	}
next_key:
	btrfs_release_path(root, path);

	if (other_key.objectid == inode->i_ino &&
	    other_key.type == key.type && other_key.offset > key.offset) {
		key.offset = other_key.offset;
		cond_resched();
		goto again;
	}
	ret = 0;
out:
	/* fixup any changes we've made to the path */
	path->lowest_level = 0;
	path->keep_locks = 0;
	btrfs_release_path(root, path);
	return ret;
}

#endif

/*
 * this can truncate away extent items, csum items and directory items.
 * It starts at a high offset and removes keys until it can't find
 * any higher than new_size
 *
 * csum items that cross the new i_size are truncated to the new size
 * as well.
 *
 * min_type is the minimum key type to truncate down to.  If set to 0, this
 * will kill all the items on this inode, including the INODE_ITEM_KEY.
 */
noinline int btrfs_truncate_inode_items(struct btrfs_trans_handle *trans,
					struct btrfs_root *root,
					struct inode *inode,
					u64 new_size, u32 min_type)
{
	int ret;
	struct btrfs_path *path;
	struct btrfs_key key;
	struct btrfs_key found_key;
	u32 found_type = (u8)-1;
	struct extent_buffer *leaf;
	struct btrfs_file_extent_item *fi;
	u64 extent_start = 0;
	u64 extent_num_bytes = 0;
	u64 extent_offset = 0;
	u64 item_end = 0;
	int found_extent;
	int del_item;
	int pending_del_nr = 0;
	int pending_del_slot = 0;
	int extent_type = -1;
	int encoding;
	u64 mask = root->sectorsize - 1;

	if (root->ref_cows)
		btrfs_drop_extent_cache(inode, new_size & (~mask), (u64)-1, 0);
	path = btrfs_alloc_path();
	BUG_ON(!path);
	path->reada = -1;

	/* FIXME, add redo link to tree so we don't leak on crash */
	key.objectid = inode->i_ino;
	key.offset = (u64)-1;
	key.type = (u8)-1;

search_again:
	path->leave_spinning = 1;
	ret = btrfs_search_slot(trans, root, &key, path, -1, 1);
	if (ret < 0)
		goto error;

	if (ret > 0) {
		/* there are no items in the tree for us to truncate, we're
		 * done
		 */
		if (path->slots[0] == 0) {
			ret = 0;
			goto error;
		}
		path->slots[0]--;
	}

	while (1) {
		fi = NULL;
		leaf = path->nodes[0];
		btrfs_item_key_to_cpu(leaf, &found_key, path->slots[0]);
		found_type = btrfs_key_type(&found_key);
		encoding = 0;

		if (found_key.objectid != inode->i_ino)
			break;

		if (found_type < min_type)
			break;

		item_end = found_key.offset;
		if (found_type == BTRFS_EXTENT_DATA_KEY) {
			fi = btrfs_item_ptr(leaf, path->slots[0],
					    struct btrfs_file_extent_item);
			extent_type = btrfs_file_extent_type(leaf, fi);
			encoding = btrfs_file_extent_compression(leaf, fi);
			encoding |= btrfs_file_extent_encryption(leaf, fi);
			encoding |= btrfs_file_extent_other_encoding(leaf, fi);

			if (extent_type != BTRFS_FILE_EXTENT_INLINE) {
				item_end +=
				    btrfs_file_extent_num_bytes(leaf, fi);
			} else if (extent_type == BTRFS_FILE_EXTENT_INLINE) {
				item_end += btrfs_file_extent_inline_len(leaf,
									 fi);
			}
			item_end--;
		}
		if (item_end < new_size) {
			if (found_type == BTRFS_DIR_ITEM_KEY)
				found_type = BTRFS_INODE_ITEM_KEY;
			else if (found_type == BTRFS_EXTENT_ITEM_KEY)
				found_type = BTRFS_EXTENT_DATA_KEY;
			else if (found_type == BTRFS_EXTENT_DATA_KEY)
				found_type = BTRFS_XATTR_ITEM_KEY;
			else if (found_type == BTRFS_XATTR_ITEM_KEY)
				found_type = BTRFS_INODE_REF_KEY;
			else if (found_type)
				found_type--;
			else
				break;
			btrfs_set_key_type(&key, found_type);
			goto next;
		}
		if (found_key.offset >= new_size)
			del_item = 1;
		else
			del_item = 0;
		found_extent = 0;

		/* FIXME, shrink the extent if the ref count is only 1 */
		if (found_type != BTRFS_EXTENT_DATA_KEY)
			goto delete;

		if (extent_type != BTRFS_FILE_EXTENT_INLINE) {
			u64 num_dec;
			extent_start = btrfs_file_extent_disk_bytenr(leaf, fi);
			if (!del_item && !encoding) {
				u64 orig_num_bytes =
					btrfs_file_extent_num_bytes(leaf, fi);
				extent_num_bytes = new_size -
					found_key.offset + root->sectorsize - 1;
				extent_num_bytes = extent_num_bytes &
					~((u64)root->sectorsize - 1);
				btrfs_set_file_extent_num_bytes(leaf, fi,
							 extent_num_bytes);
				num_dec = (orig_num_bytes -
					   extent_num_bytes);
				if (root->ref_cows && extent_start != 0)
					inode_sub_bytes(inode, num_dec);
				btrfs_mark_buffer_dirty(leaf);
			} else {
				extent_num_bytes =
					btrfs_file_extent_disk_num_bytes(leaf,
									 fi);
				extent_offset = found_key.offset -
					btrfs_file_extent_offset(leaf, fi);

				/* FIXME blocksize != 4096 */
				num_dec = btrfs_file_extent_num_bytes(leaf, fi);
				if (extent_start != 0) {
					found_extent = 1;
					if (root->ref_cows)
						inode_sub_bytes(inode, num_dec);
				}
			}
		} else if (extent_type == BTRFS_FILE_EXTENT_INLINE) {
			/*
			 * we can't truncate inline items that have had
			 * special encodings
			 */
			if (!del_item &&
			    btrfs_file_extent_compression(leaf, fi) == 0 &&
			    btrfs_file_extent_encryption(leaf, fi) == 0 &&
			    btrfs_file_extent_other_encoding(leaf, fi) == 0) {
				u32 size = new_size - found_key.offset;

				if (root->ref_cows) {
					inode_sub_bytes(inode, item_end + 1 -
							new_size);
				}
				size =
				    btrfs_file_extent_calc_inline_size(size);
				ret = btrfs_truncate_item(trans, root, path,
							  size, 1);
				BUG_ON(ret);
			} else if (root->ref_cows) {
				inode_sub_bytes(inode, item_end + 1 -
						found_key.offset);
			}
		}
delete:
		if (del_item) {
			if (!pending_del_nr) {
				/* no pending yet, add ourselves */
				pending_del_slot = path->slots[0];
				pending_del_nr = 1;
			} else if (pending_del_nr &&
				   path->slots[0] + 1 == pending_del_slot) {
				/* hop on the pending chunk */
				pending_del_nr++;
				pending_del_slot = path->slots[0];
			} else {
				BUG();
			}
		} else {
			break;
		}
		if (found_extent && root->ref_cows) {
			btrfs_set_path_blocking(path);
			ret = btrfs_free_extent(trans, root, extent_start,
						extent_num_bytes, 0,
						btrfs_header_owner(leaf),
						inode->i_ino, extent_offset);
			BUG_ON(ret);
		}
next:
		if (path->slots[0] == 0) {
			if (pending_del_nr)
				goto del_pending;
			btrfs_release_path(root, path);
			if (found_type == BTRFS_INODE_ITEM_KEY)
				break;
			goto search_again;
		}

		path->slots[0]--;
		if (pending_del_nr &&
		    path->slots[0] + 1 != pending_del_slot) {
			struct btrfs_key debug;
del_pending:
			btrfs_item_key_to_cpu(path->nodes[0], &debug,
					      pending_del_slot);
			ret = btrfs_del_items(trans, root, path,
					      pending_del_slot,
					      pending_del_nr);
			BUG_ON(ret);
			pending_del_nr = 0;
			btrfs_release_path(root, path);
			if (found_type == BTRFS_INODE_ITEM_KEY)
				break;
			goto search_again;
		}
	}
	ret = 0;
error:
	if (pending_del_nr) {
		ret = btrfs_del_items(trans, root, path, pending_del_slot,
				      pending_del_nr);
	}
	btrfs_free_path(path);
	return ret;
}

/*
 * taken from block_truncate_page, but does cow as it zeros out
 * any bytes left in the last page in the file.
 */
static int btrfs_truncate_page(struct address_space *mapping, loff_t from)
{
	struct inode *inode = mapping->host;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct extent_io_tree *io_tree = &BTRFS_I(inode)->io_tree;
	struct btrfs_ordered_extent *ordered;
	char *kaddr;
	u32 blocksize = root->sectorsize;
	pgoff_t index = from >> PAGE_CACHE_SHIFT;
	unsigned offset = from & (PAGE_CACHE_SIZE-1);
	struct page *page;
	int ret = 0;
	u64 page_start;
	u64 page_end;

	if ((offset & (blocksize - 1)) == 0)
		goto out;
	ret = btrfs_check_data_free_space(root, inode, PAGE_CACHE_SIZE);
	if (ret)
		goto out;

	ret = btrfs_reserve_metadata_for_delalloc(root, inode, 1);
	if (ret)
		goto out;

	ret = -ENOMEM;
again:
	page = grab_cache_page(mapping, index);
	if (!page) {
		btrfs_free_reserved_data_space(root, inode, PAGE_CACHE_SIZE);
		btrfs_unreserve_metadata_for_delalloc(root, inode, 1);
		goto out;
	}

	page_start = page_offset(page);
	page_end = page_start + PAGE_CACHE_SIZE - 1;

	if (!PageUptodate(page)) {
		ret = btrfs_readpage(NULL, page);
		lock_page(page);
		if (page->mapping != mapping) {
			unlock_page(page);
			page_cache_release(page);
			goto again;
		}
		if (!PageUptodate(page)) {
			ret = -EIO;
			goto out_unlock;
		}
	}
	wait_on_page_writeback(page);

	lock_extent(io_tree, page_start, page_end, GFP_NOFS);
	set_page_extent_mapped(page);

	ordered = btrfs_lookup_ordered_extent(inode, page_start);
	if (ordered) {
		unlock_extent(io_tree, page_start, page_end, GFP_NOFS);
		unlock_page(page);
		page_cache_release(page);
		btrfs_start_ordered_extent(inode, ordered, 1);
		btrfs_put_ordered_extent(ordered);
		goto again;
	}

	clear_extent_bits(&BTRFS_I(inode)->io_tree, page_start, page_end,
			  EXTENT_DIRTY | EXTENT_DELALLOC | EXTENT_DO_ACCOUNTING,
			  GFP_NOFS);

	ret = btrfs_set_extent_delalloc(inode, page_start, page_end);
	if (ret) {
		unlock_extent(io_tree, page_start, page_end, GFP_NOFS);
		goto out_unlock;
	}

	ret = 0;
	if (offset != PAGE_CACHE_SIZE) {
		kaddr = kmap(page);
		memset(kaddr + offset, 0, PAGE_CACHE_SIZE - offset);
		flush_dcache_page(page);
		kunmap(page);
	}
	ClearPageChecked(page);
	set_page_dirty(page);
	unlock_extent(io_tree, page_start, page_end, GFP_NOFS);

out_unlock:
	if (ret)
		btrfs_free_reserved_data_space(root, inode, PAGE_CACHE_SIZE);
	btrfs_unreserve_metadata_for_delalloc(root, inode, 1);
	unlock_page(page);
	page_cache_release(page);
out:
	return ret;
}

int btrfs_cont_expand(struct inode *inode, loff_t size)
{
	struct btrfs_trans_handle *trans;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct extent_io_tree *io_tree = &BTRFS_I(inode)->io_tree;
	struct extent_map *em;
	u64 mask = root->sectorsize - 1;
	u64 hole_start = (inode->i_size + mask) & ~mask;
	u64 block_end = (size + mask) & ~mask;
	u64 last_byte;
	u64 cur_offset;
	u64 hole_size;
	int err = 0;

	if (size <= hole_start)
		return 0;

	err = btrfs_truncate_page(inode->i_mapping, inode->i_size);
	if (err)
		return err;

	while (1) {
		struct btrfs_ordered_extent *ordered;
		btrfs_wait_ordered_range(inode, hole_start,
					 block_end - hole_start);
		lock_extent(io_tree, hole_start, block_end - 1, GFP_NOFS);
		ordered = btrfs_lookup_ordered_extent(inode, hole_start);
		if (!ordered)
			break;
		unlock_extent(io_tree, hole_start, block_end - 1, GFP_NOFS);
		btrfs_put_ordered_extent(ordered);
	}

	trans = btrfs_start_transaction(root, 1);
	btrfs_set_trans_block_group(trans, inode);

	cur_offset = hole_start;
	while (1) {
		em = btrfs_get_extent(inode, NULL, 0, cur_offset,
				block_end - cur_offset, 0);
		BUG_ON(IS_ERR(em) || !em);
		last_byte = min(extent_map_end(em), block_end);
		last_byte = (last_byte + mask) & ~mask;
		if (test_bit(EXTENT_FLAG_VACANCY, &em->flags)) {
			u64 hint_byte = 0;
			hole_size = last_byte - cur_offset;
			err = btrfs_drop_extents(trans, root, inode,
						 cur_offset,
						 cur_offset + hole_size,
						 block_end,
						 cur_offset, &hint_byte, 1);
			if (err)
				break;

			err = btrfs_reserve_metadata_space(root, 1);
			if (err)
				break;

			err = btrfs_insert_file_extent(trans, root,
					inode->i_ino, cur_offset, 0,
					0, hole_size, 0, hole_size,
					0, 0, 0);
			btrfs_drop_extent_cache(inode, hole_start,
					last_byte - 1, 0);
			btrfs_unreserve_metadata_space(root, 1);
		}
		free_extent_map(em);
		cur_offset = last_byte;
		if (err || cur_offset >= block_end)
			break;
	}

	btrfs_end_transaction(trans, root);
	unlock_extent(io_tree, hole_start, block_end - 1, GFP_NOFS);
	return err;
}

static int btrfs_setattr(struct dentry *dentry, struct iattr *attr)
{
	struct inode *inode = dentry->d_inode;
	int err;

	err = inode_change_ok(inode, attr);
	if (err)
		return err;

	if (S_ISREG(inode->i_mode) && (attr->ia_valid & ATTR_SIZE)) {
		if (attr->ia_size > inode->i_size) {
			err = btrfs_cont_expand(inode, attr->ia_size);
			if (err)
				return err;
		} else if (inode->i_size > 0 &&
			   attr->ia_size == 0) {

			/* we're truncating a file that used to have good
			 * data down to zero.  Make sure it gets into
			 * the ordered flush list so that any new writes
			 * get down to disk quickly.
			 */
			BTRFS_I(inode)->ordered_data_close = 1;
		}
	}

	err = inode_setattr(inode, attr);

	if (!err && ((attr->ia_valid & ATTR_MODE)))
		err = btrfs_acl_chmod(inode);
	return err;
}

void btrfs_delete_inode(struct inode *inode)
{
	struct btrfs_trans_handle *trans;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	unsigned long nr;
	int ret;

	truncate_inode_pages(&inode->i_data, 0);
	if (is_bad_inode(inode)) {
		btrfs_orphan_del(NULL, inode);
		goto no_delete;
	}
	btrfs_wait_ordered_range(inode, 0, (u64)-1);

	if (inode->i_nlink > 0) {
		BUG_ON(btrfs_root_refs(&root->root_item) != 0);
		goto no_delete;
	}

	btrfs_i_size_write(inode, 0);
	trans = btrfs_join_transaction(root, 1);

	btrfs_set_trans_block_group(trans, inode);
	ret = btrfs_truncate_inode_items(trans, root, inode, inode->i_size, 0);
	if (ret) {
		btrfs_orphan_del(NULL, inode);
		goto no_delete_lock;
	}

	btrfs_orphan_del(trans, inode);

	nr = trans->blocks_used;
	clear_inode(inode);

	btrfs_end_transaction(trans, root);
	btrfs_btree_balance_dirty(root, nr);
	return;

no_delete_lock:
	nr = trans->blocks_used;
	btrfs_end_transaction(trans, root);
	btrfs_btree_balance_dirty(root, nr);
no_delete:
	clear_inode(inode);
}

/*
 * this returns the key found in the dir entry in the location pointer.
 * If no dir entries were found, location->objectid is 0.
 */
static int btrfs_inode_by_name(struct inode *dir, struct dentry *dentry,
			       struct btrfs_key *location)
{
	const char *name = dentry->d_name.name;
	int namelen = dentry->d_name.len;
	struct btrfs_dir_item *di;
	struct btrfs_path *path;
	struct btrfs_root *root = BTRFS_I(dir)->root;
	int ret = 0;

	path = btrfs_alloc_path();
	BUG_ON(!path);

	di = btrfs_lookup_dir_item(NULL, root, path, dir->i_ino, name,
				    namelen, 0);
	if (IS_ERR(di))
		ret = PTR_ERR(di);

	if (!di || IS_ERR(di))
		goto out_err;

	btrfs_dir_item_key_to_cpu(path->nodes[0], di, location);
out:
	btrfs_free_path(path);
	return ret;
out_err:
	location->objectid = 0;
	goto out;
}

/*
 * when we hit a tree root in a directory, the btrfs part of the inode
 * needs to be changed to reflect the root directory of the tree root.  This
 * is kind of like crossing a mount point.
 */
static int fixup_tree_root_location(struct btrfs_root *root,
				    struct inode *dir,
				    struct dentry *dentry,
				    struct btrfs_key *location,
				    struct btrfs_root **sub_root)
{
	struct btrfs_path *path;
	struct btrfs_root *new_root;
	struct btrfs_root_ref *ref;
	struct extent_buffer *leaf;
	int ret;
	int err = 0;

	path = btrfs_alloc_path();
	if (!path) {
		err = -ENOMEM;
		goto out;
	}

	err = -ENOENT;
	ret = btrfs_find_root_ref(root->fs_info->tree_root, path,
				  BTRFS_I(dir)->root->root_key.objectid,
				  location->objectid);
	if (ret) {
		if (ret < 0)
			err = ret;
		goto out;
	}

	leaf = path->nodes[0];
	ref = btrfs_item_ptr(leaf, path->slots[0], struct btrfs_root_ref);
	if (btrfs_root_ref_dirid(leaf, ref) != dir->i_ino ||
	    btrfs_root_ref_name_len(leaf, ref) != dentry->d_name.len)
		goto out;

	ret = memcmp_extent_buffer(leaf, dentry->d_name.name,
				   (unsigned long)(ref + 1),
				   dentry->d_name.len);
	if (ret)
		goto out;

	btrfs_release_path(root->fs_info->tree_root, path);

	new_root = btrfs_read_fs_root_no_name(root->fs_info, location);
	if (IS_ERR(new_root)) {
		err = PTR_ERR(new_root);
		goto out;
	}

	if (btrfs_root_refs(&new_root->root_item) == 0) {
		err = -ENOENT;
		goto out;
	}

	*sub_root = new_root;
	location->objectid = btrfs_root_dirid(&new_root->root_item);
	location->type = BTRFS_INODE_ITEM_KEY;
	location->offset = 0;
	err = 0;
out:
	btrfs_free_path(path);
	return err;
}

static void inode_tree_add(struct inode *inode)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_inode *entry;
	struct rb_node **p;
	struct rb_node *parent;
again:
	p = &root->inode_tree.rb_node;
	parent = NULL;

	if (hlist_unhashed(&inode->i_hash))
		return;

	spin_lock(&root->inode_lock);
	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct btrfs_inode, rb_node);

		if (inode->i_ino < entry->vfs_inode.i_ino)
			p = &parent->rb_left;
		else if (inode->i_ino > entry->vfs_inode.i_ino)
			p = &parent->rb_right;
		else {
			WARN_ON(!(entry->vfs_inode.i_state &
				  (I_WILL_FREE | I_FREEING | I_CLEAR)));
			rb_erase(parent, &root->inode_tree);
			RB_CLEAR_NODE(parent);
			spin_unlock(&root->inode_lock);
			goto again;
		}
	}
	rb_link_node(&BTRFS_I(inode)->rb_node, parent, p);
	rb_insert_color(&BTRFS_I(inode)->rb_node, &root->inode_tree);
	spin_unlock(&root->inode_lock);
}

static void inode_tree_del(struct inode *inode)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	int empty = 0;

	spin_lock(&root->inode_lock);
	if (!RB_EMPTY_NODE(&BTRFS_I(inode)->rb_node)) {
		rb_erase(&BTRFS_I(inode)->rb_node, &root->inode_tree);
		RB_CLEAR_NODE(&BTRFS_I(inode)->rb_node);
		empty = RB_EMPTY_ROOT(&root->inode_tree);
	}
	spin_unlock(&root->inode_lock);

	if (empty && btrfs_root_refs(&root->root_item) == 0) {
		synchronize_srcu(&root->fs_info->subvol_srcu);
		spin_lock(&root->inode_lock);
		empty = RB_EMPTY_ROOT(&root->inode_tree);
		spin_unlock(&root->inode_lock);
		if (empty)
			btrfs_add_dead_root(root);
	}
}

int btrfs_invalidate_inodes(struct btrfs_root *root)
{
	struct rb_node *node;
	struct rb_node *prev;
	struct btrfs_inode *entry;
	struct inode *inode;
	u64 objectid = 0;

	WARN_ON(btrfs_root_refs(&root->root_item) != 0);

	spin_lock(&root->inode_lock);
again:
	node = root->inode_tree.rb_node;
	prev = NULL;
	while (node) {
		prev = node;
		entry = rb_entry(node, struct btrfs_inode, rb_node);

		if (objectid < entry->vfs_inode.i_ino)
			node = node->rb_left;
		else if (objectid > entry->vfs_inode.i_ino)
			node = node->rb_right;
		else
			break;
	}
	if (!node) {
		while (prev) {
			entry = rb_entry(prev, struct btrfs_inode, rb_node);
			if (objectid <= entry->vfs_inode.i_ino) {
				node = prev;
				break;
			}
			prev = rb_next(prev);
		}
	}
	while (node) {
		entry = rb_entry(node, struct btrfs_inode, rb_node);
		objectid = entry->vfs_inode.i_ino + 1;
		inode = igrab(&entry->vfs_inode);
		if (inode) {
			spin_unlock(&root->inode_lock);
			if (atomic_read(&inode->i_count) > 1)
				d_prune_aliases(inode);
			/*
			 * btrfs_drop_inode will remove it from
			 * the inode cache when its usage count
			 * hits zero.
			 */
			iput(inode);
			cond_resched();
			spin_lock(&root->inode_lock);
			goto again;
		}

		if (cond_resched_lock(&root->inode_lock))
			goto again;

		node = rb_next(node);
	}
	spin_unlock(&root->inode_lock);
	return 0;
}

static noinline void init_btrfs_i(struct inode *inode)
{
	struct btrfs_inode *bi = BTRFS_I(inode);

	bi->generation = 0;
	bi->sequence = 0;
	bi->last_trans = 0;
	bi->last_sub_trans = 0;
	bi->logged_trans = 0;
	bi->delalloc_bytes = 0;
	bi->reserved_bytes = 0;
	bi->disk_i_size = 0;
	bi->flags = 0;
	bi->index_cnt = (u64)-1;
	bi->last_unlink_trans = 0;
	bi->ordered_data_close = 0;
	extent_map_tree_init(&BTRFS_I(inode)->extent_tree, GFP_NOFS);
	extent_io_tree_init(&BTRFS_I(inode)->io_tree,
			     inode->i_mapping, GFP_NOFS);
	extent_io_tree_init(&BTRFS_I(inode)->io_failure_tree,
			     inode->i_mapping, GFP_NOFS);
	INIT_LIST_HEAD(&BTRFS_I(inode)->delalloc_inodes);
	INIT_LIST_HEAD(&BTRFS_I(inode)->ordered_operations);
	RB_CLEAR_NODE(&BTRFS_I(inode)->rb_node);
	btrfs_ordered_inode_tree_init(&BTRFS_I(inode)->ordered_tree);
	mutex_init(&BTRFS_I(inode)->extent_mutex);
	mutex_init(&BTRFS_I(inode)->log_mutex);
}

static int btrfs_init_locked_inode(struct inode *inode, void *p)
{
	struct btrfs_iget_args *args = p;
	inode->i_ino = args->ino;
	init_btrfs_i(inode);
	BTRFS_I(inode)->root = args->root;
	btrfs_set_inode_space_info(args->root, inode);
	return 0;
}

static int btrfs_find_actor(struct inode *inode, void *opaque)
{
	struct btrfs_iget_args *args = opaque;
	return args->ino == inode->i_ino &&
		args->root == BTRFS_I(inode)->root;
}

static struct inode *btrfs_iget_locked(struct super_block *s,
				       u64 objectid,
				       struct btrfs_root *root)
{
	struct inode *inode;
	struct btrfs_iget_args args;
	args.ino = objectid;
	args.root = root;

	inode = iget5_locked(s, objectid, btrfs_find_actor,
			     btrfs_init_locked_inode,
			     (void *)&args);
	return inode;
}

/* Get an inode object given its location and corresponding root.
 * Returns in *is_new if the inode was read from disk
 */
struct inode *btrfs_iget(struct super_block *s, struct btrfs_key *location,
			 struct btrfs_root *root)
{
	struct inode *inode;

	inode = btrfs_iget_locked(s, location->objectid, root);
	if (!inode)
		return ERR_PTR(-ENOMEM);

	if (inode->i_state & I_NEW) {
		BTRFS_I(inode)->root = root;
		memcpy(&BTRFS_I(inode)->location, location, sizeof(*location));
		btrfs_read_locked_inode(inode);

		inode_tree_add(inode);
		unlock_new_inode(inode);
	}

	return inode;
}

static struct inode *new_simple_dir(struct super_block *s,
				    struct btrfs_key *key,
				    struct btrfs_root *root)
{
	struct inode *inode = new_inode(s);

	if (!inode)
		return ERR_PTR(-ENOMEM);

	init_btrfs_i(inode);

	BTRFS_I(inode)->root = root;
	memcpy(&BTRFS_I(inode)->location, key, sizeof(*key));
	BTRFS_I(inode)->dummy_inode = 1;

	inode->i_ino = BTRFS_EMPTY_SUBVOL_DIR_OBJECTID;
	inode->i_op = &simple_dir_inode_operations;
	inode->i_fop = &simple_dir_operations;
	inode->i_mode = S_IFDIR | S_IRUGO | S_IWUSR | S_IXUGO;
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;

	return inode;
}

struct inode *btrfs_lookup_dentry(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode;
	struct btrfs_root *root = BTRFS_I(dir)->root;
	struct btrfs_root *sub_root = root;
	struct btrfs_key location;
	int index;
	int ret;

	dentry->d_op = &btrfs_dentry_operations;

	if (dentry->d_name.len > BTRFS_NAME_LEN)
		return ERR_PTR(-ENAMETOOLONG);

	ret = btrfs_inode_by_name(dir, dentry, &location);

	if (ret < 0)
		return ERR_PTR(ret);

	if (location.objectid == 0)
		return NULL;

	if (location.type == BTRFS_INODE_ITEM_KEY) {
		inode = btrfs_iget(dir->i_sb, &location, root);
		return inode;
	}

	BUG_ON(location.type != BTRFS_ROOT_ITEM_KEY);

	index = srcu_read_lock(&root->fs_info->subvol_srcu);
	ret = fixup_tree_root_location(root, dir, dentry,
				       &location, &sub_root);
	if (ret < 0) {
		if (ret != -ENOENT)
			inode = ERR_PTR(ret);
		else
			inode = new_simple_dir(dir->i_sb, &location, sub_root);
	} else {
		inode = btrfs_iget(dir->i_sb, &location, sub_root)/*
 
	srcu_read_unlock(&* Th->Oracnfo->subvol_ram , index);

	return t (C);
}

static int007 Oradentry_delete(struct NU Gen *NU Gen)
{
	lic
 * 07 Ora* Th ** Th
 * if (!NU Gen->d_t (C) && !IS_ROOT(v2 as p)
		License= Licens *
 parenare Foundtion.
 *
 * Thi)yrighree S= BTRFS_Iful,
 * but WITHO->ftware	Found the Free _refsware; yo MERCitem) == 0uted modify 1s progmodify 0der the termlic
 * License 07 Oralookupblic
 * t (C) *dir,General Public LNU Gen, PAR	  General nameidata *ndpublished byils.
 *it unde
ht (C) 2007 Ora for mGNU Gen  Al, Licensis poundIS_ERR( WITHOutedmodify ERR_CASTFoundat
 * modify d_splice_aliasFounda the
 * Frer the termunsigned chary the Ffiletype_table[] =yrigDT_UNKNOWN, DT_REGlude DIRlude CHfer_heBLKlude FIFOlude SOClude <LNK
}; the terms of the Grealis frdirblic
 * l.h> *filp, void *
 *ented a  ncludfilldir_>
#ininuxPublic
 * License along  =e <lp->fGNU Gen *
 * Thi;lished by the Free SoftwaRRANTY; witthe implied warshed by the FTNES *TNESge.h>
#include <nux/linux/dige.h>
#include <key keyk.h>
#include <linuxfoundlinuge.h>
#include <path *inuxree nt re waru32 nrTNESsge.h>
#incextent_buffer *leafpinlocksld warlockadvanc#incinclude <linuxd_
#in
#incluover =e thincludi_curude "ctreetotalude "ctreelen
#inclukeyude "RRANTY; wDIR_INDEX_KEY;
	linuxtmp_ GNU[32]ode.h"
#* GNU_pt"
#ilockcludeclude
	/* FIXME, use a e <l flag for deciding about theansa tion.*/ee Sofre; you can redtreeFree S== * Thi
		nsaction.h"
#include "TEMs_inodlude special caseorder"."h"
#inclulude <lipos FOR Ayrighh"
#incme.h>
#  Alem.h	u64, 1h>
#include 1t anodel riinoe_operationsx/buffis pe Sofh"
#A PARTICULARlude	ruct btrfs_roR PURPOS btrfs_iget_args {
	.., justes.h"ludeback ref ino;
	struct btrfs_root1*root;u64 pino = t willinodtruct btrfath.he
 * Free;
};

static const struct  ino2e_operations2,s;
stnux/buffic const struct inode_operations btrfs_symli2s proginux/2007 Oraallocde_op(is pe_op->s fratic co
	07 Orasetlinuude "(&key,ansactionis pkey.offsev.h>ruct btrfs_rations bjectid =trfs_dir_inod
 * modct addressearch_l.h>(NULL,ompre, dress_e_op, 0, 0Free Sofns b<R A PAgoto er"
#ide <lininclud
	whincl(_operatux/patic s btfs_ds[0ctl.	de <linct addresheader_de <lin(ux/ps btrl.h>de_cachepl.h>ruct kmeoundatic str||cl.h> >=ude <lin*root;nsact
struct kmem_ca -e_operatn, Inct addresnext_ux/pude "uct exs btrbtrfs_reessio			breaktype_b_inode_cachep;
struct kmemem_cache *btrfs_trans_handle_cachep;
struruct kmem_cache *btrfs_tran
 * Copyrighruct k++SHIFT]_cache *btrfs_RFS_FT_}
 S_S
	tatic struc PUR	linux2007 Oractem_nrchep;,cl.h>s btrIFIFO >> S_struco_cpuIFT]	= &nux/compa= BTRFS_Fransactnux/compac const st!=ansac const sS_IFM>> S_SHIFanty of
 *struct addnux/compa)RFS_FT_ce_ope;

static void btK >> S_SHIFbtrfs_<ymlink_aops;tic icontinu withons btrfs_symlish_ordered_io(st
	[S_di[S_IFIFO >> S_ptHIFT]	= BTRF You shoude <linux/writs btrree.h"includeage,"disk[S_IFIFO >> S_sizeIFT]	= locked_
	em_cachege,
			<ude "diskne S_SH>
#include <linuxreserved		   	ee.h"
#i[S_IFIFO nux/ee.h"
#iIFT]	= di_SHIFTound_inode_se<= pageof(include )ne S_SHIclude "p =#include IFCHR >> S_SHIFT]	nit_acl(inokms_spaode *dir), GFP_NOFS_type_by_mo!clude "pne S_SHIIFT 12
-ENOMEMSHIFT]_io_ops;

st> S_SHIertings fre>
#include <lIFT]	= clude "ped a cincl(include <long)(di + 1)e btreelen			   	lude "[S_IFIFO l.h>
#include <rity(strucct adinode,  s_IFCHRde <linux/writIFSOCK >> S_SHIFT]dits reservedtents s/* is this"
#infere strto our own snapshot? If so>
#in* skip itot, st/ *btrfs_reserved.that
 RANTY; wm isng.h"

st &&>
#incluu64 start const strcompreY or FIFT_SYMLINK,
}SHIFT]	h"
#includeard workuct  >> S_SHI;
};

static const strucbtree.  Tree.h"
#ied a copylude h_ordered_io(st,,
				struct page age *page = Nlude "			 uct :ruct inode *dcl(i!node, dir)S_IFMTkfree*ei;
	inttents snst struct in_io_opnops;
st64 ste_security(struct inode *inode,  s +d a cority(strucGeneuse_compress = 0	int err*  structge,
			+=
#include ile_ranblic
 * de <linux/writeb)((h"
#in)e a bgned ls btr} proode_oReached end of st sctory/* Th. Bump fs_rpasncludelg = >> S.h"
#inclunsaction.hh"
#include "btrfs_intic ns btrfs_symliINT_LIMIT(off_FS_FT Copjectid = inodeRFS_ize;
:ions btr0;
err:tatic co
	sice_opetrfs_typmodify .h>
#}

s of the Gwe <l * Thiore details.
 *A 02111lockwaitpublished by the Free Softwa.h>
#include <linux/mpage.h>
#include <trans_handnclu,
			pinlock.h>ruct kmenclude "tree-log.h"b
#inct (C) 2ruct fiion, Inc., et);
	if es(inyrigh,
			[S_IFIFO joiny,
			acrved char b1S_FT_FIFO,
nst ,
				bsoft_group(,
			btrfs_ds btrFT 12
staticcommit btrfsath->slitem);
* This prog btrfs_file_ext/*
 * Ts_hais somewha.h>
pensive, updatta.hlude
#in every timct in_extt (C) changes.  Butted,typemost likely,
		find_INLIt (C) in c		re._ext"volumeneed 0);re benchmarking...there are no#incsons ot_ra than perform<lin_extto keep dereropans_hacoding(l/
nux/hrity(struty_size(cur_size);

	inode_anode, size);
	ret = btrfs_insert_empty_item(trans, root, path, &key,
				      datasize)
 = btrfs_item_ptr(leaf, path->slots[0],
			   struct btrfs_file_extent_item);
	btrfs_se07 OraE_EXTe_size(ci, trans->t kmap_atomic(cpagendation(leaf, ei, trans->tran_file_extile_extenhighes.h>
isTENT_sequrans,numberer_eaOMEM;

	pa_extae_exten rfs_xtent_-memory and/o_cnt variude _extreflectatomiE);
++;
			ptr += cs	if (he terms of the Gnst rfs_d_	btrfs_ountct page *cpage;
		int i = 0;
		while (compressed_size > 0) {
			cpage = compressed_pageinux/st,inux/compat.h>
#include <linux/bit_spinxattr.h>
#include <linux/posix_ac.h>
#static const struct file_operatatic const struct address_ns, inode);

	key.obrations btrfs_sy(u64)-1t & struct address_space_operatioundatrfs_on, Inc., s all theions btrfs_dir_file_operations;
static struct extent_io_ops btrfs_extent_io_opou warde "volu: we should be e_extent      dns_ha"
#includ_ON(OR A PAinode->i_siG_ON(ret);
/*
 strMAGIC NUMBER EXPLANATION:retursi str_updfile_size;
			com based on trfs_rwe havxtentstart at 2nally inser'.'ssed_'..'
 * dotrfs_rof 0ssed_1 rerfs_tivelFT_So
	btrfbodyretur Copyhasoes the checks req
#inclu_cache *btrfs_root *root;
#include <linux	btrfs_setic con	btrfs_free_ (!p_cache *btrfs_--uffe_inode_cachep;
struct kmFIFO,
	[S_IFSOCK >> S_SHIFT]	= BTRFS_FT__cache *btrfs_			  IFLNK >> S_SHIFT]	= BTRFS_rfs_dir_inod ||
inclurfs_truncate(struct inode *inons, inode);

	key.obt btrfs_trans_handle *trans,
				 struct btrfs_root *fs_trans_handle *trans,
				ULL;
	char *kadda bt;
outTENT_DATA_KEY);
	datasize = btrfs_file_extle_exthelpze);_file_eaon(leaf, ei,
						  ur_sizgivenOMEM;

	pa. tent_tcur str_exti);
typebtrfssimple, later
	  si ei,will do smar >= thingser_e inod
#in	if (S_ZLIB);
	} else {
		pageore details.
 *
 * Yions*and/orubli
	BUG_ON(ret);
	if NTY; withirle *trans,
			tomic(kadOUT ANYs btrfs_dir_felse {
		page = find 1))c const de[S_IFM btrfs_file_oot *	    (RRANTY; with1)) == 0) ||
	;nd + root-ents(trans, rooRFS_d);
	btrfs_set_fiNU
 * General ils.
 *static uw_size(cur_size, &key,
				      datasizage *page =shed by the Free Softwa		inline_len = minils.
 *
 * ge *page =const<linuxncluddd_bytstruct page *page =ionsref_d long pte_lend long ptr;
	structionss_spachitruclockm02111 &&
	    (actuac
 * License along wi.h>
#include <le {
		inux/s 0);
	retfs_set_file_extent_c*int btrfs_ ei, 0);
		kaddr = kmap_atomic(pagnd - 1, 0);
eratireposiset_file_extent_comp[octl.nclupages_pages;hould have d  "print-tr.h>
#ipat.hwnerdr, KM_USER0);
		page_cache_releBUG_ON(page);with this pr;
	BUG_ON(re; you can redibrelease(pil;
	}
	leaf = p59 TPTR(s all t**compresents< isize ||
	    data_len > root-e to tand/or
 _inline) {he *btrpute Place -		return 1 extentsde[S();
	if (;
	retur	btrfs_sett_tygnoredorder	btrfata_l butsize;
,retur07 Oragta_len > root->fs_in/
staani, Blanervedorderludemagicreturr += cint cow_nit_07 Oraccow,
				 fs_trans_handle *trans,
				 str
#include <linux/mpa compre
	async_extent->stagenert asyn= ,
			h"
#ansidite_extent_bun 0;
}spac
		pfo char b Place - Sound;
	b & S_IF_opebtrfstruinclude start;s;
	asyn PUR
#include <linuxfile_extent =
 */
statiile__file_extent_te_ext0,es);
	BUG_ON(s;
	astruc nr_0]c const stru const site_extent_buffer(leaf, ka[0]ddr + ofINODEng.h"

stze);
		comprbtrfs_syt kmeges 1mpressed extents in two phases.  The first
 * ph1se compresses aREFge of pages n
 *btrfs_sy, compressedd_entruct ne(s	int errssed_size;
	struct locked_ptruct 1ssio_inode_seed_pages) re;
st *root,
leavsizeinnructs = nrze ||
	    dinsert_emp{
		e_cac
			write_extt exteS_FT_Sruct, 2o_ops btrfs_!fail:
	btrfsfailwith thisl riu stru
	if (s_fsuideratbtrfs_wors pr  All ritent->pageSGID= btrfsrfs_dir_ig stru All rig two nc_ext geDIRxtenttion,	tent-|='t get g/*
 * Copcompression, it puand friendg.
 *
 * Ifs_dir_itent-=);
	b_cow = i_size_retents in two sed_sizet_byte MA 02111io_opscompresse_set_ize = i_sizah sent them downch sent CURRENT_TIMEare writtV,
	[S_IFIFO >> S_nodecachep;
struct			 struct page line_ext spread across many cpus.  me.h1, 0);
	rethat
 * h				struct page n 0;
}

st_size;
	asyneratt inode *inode,
					struct page *locked_page,
a bth>
#includssed_size;
	struct pag	       PAGE_Ctruct pagct inode *				struct page rehe btreepath();IB);
	} else {
	 disint ades;
	u64 blocksize = 	    (aatiol(ino should have donerat btr;
	nline_ extent into
 				struct page , inodaf;
	struct p*
 * 07 Orafilelude <lss) {
long nr_pages;
ctorsize;
_KEY);
	datasize 
	reserved = &
#include <linuxu64 start;
reserved->ressed extents in two 28 * 1024;
 have alreaatic const struct adreserved.
compresses a range of p u64 end,nherit_ide " MA 021110->maxync_exxtent->pagesREGood compnty of
 *test_optts);
	rNODATASUMueue t
#include <linuxn_t(uite compresses a PAGE_CACwarranty of
 *pages = (end >> PAGE_COWHE_SHIFT) - (start >> PAGE_CACHE_SHIFT) + 1;
	nr_COW}

	rete of man 0;
}hashcow,
				 n 0;
}
#incaddcow,
				 modify it undequeu:btrfs_workbtrfs_transents(trans, roouct NT_DATA_KEY);
	datasize =ow *cow,
				   u64 start, u64 ram_r the terms line u8ize;
	struct ct adt page *cpage;
		int i =modify (nr_pag#incbysed i[de <linds it can't gFMT) >> S_SHIFT]kunmap_atomutility funth->st readd 'hat p'(reto 'c const stcommwithpress_len unsigssed_a_len = ++;
			ptr += cng(leif 'add_de_oref'_tranrueturnso e of m aode_oeratfrom_file_extent_etoxtentt will compressedX_INLINE_DATA_Sed;
links spread acrossize > actual_end)
		ind);
	ret = insertner.
	 */
	i You shou);

	inode_ado we l(trans, root, inode, start,
			(ret)ed;

	totaltrfs_d    (actual_end & (rooth>
#include <linux/statfs.h>
#include= btrfs_insert_empty_ner.
	 */
	implied waync_exuntrfs_s * It isn'
statCHE_SHIFFIRST_FREE_OBJECT good commemcpyddress_gned long max_commpressed_pagesame orofock_)is pr* Copyrigh(PAGE_CACHE_SIZE - 1);
		writee_extent_buffer(leaf, kaddr + ofsses a range of pations btrfs_syy wet the sure the amount of IO required to do
	 * a random read is rze ||
	    ded;
 MERCHANhat
 * happetree-log.h"
#includeThe callFT_SYMLINK,
es;
	toted_pages)
{
	struThe callner.
	 */
	iir_inode_ope		so con	unsigneroot->sectorent to sactio;

	totalk work;
};

statice end of i_si= num_bytes;
	tThe caller n flagged as nobtrfs_dir_inode_ope#includnd when the
	 * inodync_extentocksize)n 0;
fail* change at any time if nux/writhat
 * happenss.
	 */
	if (!(->flags & TRFS_INODE_NOCOMPRESSdress->flags & uncompressed
	 * also wsync_extent(oot *ro4 ram_FT_FIFO,
	*pageinline
	 * We also w,
	 when the
	 * e or 0;

	ifios.
	ed lo*r thin						nr_pages, &sh sent 					nr_pages, &line int compress_file_tes);
	disk_ne, KM_USER0);
			write_ext	 * We also wansid);
	btrfs_set_fihe terms of the G flanonpagemap.h>
an extent is reasonable, so wincluou should have receive	 * of a compressed exteinclus of cause it also controls howerasynred to uncompreitem);
ope that it will *
 * Thiline_exA 021110-1307 *
  GNU., inod a coemset(kaddr + o (!(Bsending iinode);
	oundaerrk work
 * stantiateful,
 *_size;
	asinode_operati}
				k */
>R A PA */
		-EEXISTequested rs;

s;
			struct page *pamknodore details.
 *
 * You should have received a ret);
	btrdev_t rdevpublished by the F,
				      datasize); size);
	ret = btrfs_insert_empty_ents(t/mpage.h>
#incux/init.h>
#incions_cow {
s;

stlockne_s
		goto clude ze,
				   ux/falloc.h>;
};
n#include also conN(ret);
	if !;
	Bvalid_dev(e);

;
	}
	btrfs_mINVALth);
	retur2ordert_otherinuxsed_ref start, endhis ure tretur1orderxattr Thiselinuxesseonint cow */
			if (oreserve_metaed_size = ts);
	r5);
				knmapwill_comprs;

sin_t(unsigned lothe ceaf, path->slots[0],
			oundasync_onto the queue      PAGE_CACHE_SIZE);

			kaddr = ke, end + */
			if (oist, _KEY) const shat
 * happens All ri GFP_NYMLINK,
}						    syrightart == NOSPCstruct btrfsee softpast the (C) 2007 Ora;
	BUG_ON(eed
			 * to cre	memset(kaddr + offset, 0,
			       PAGE_CACHinode)->io_treeatomic(page, KMate any mnt -o compress (actual_end -file_extent,);
	btr&inode);
	 */
		PTRare Foundatree Software Foundation,e up our temp pagxtent creation_exte writtencuritamount inline erk items.  Unlge, try
			 *BLKDEe up our temp pages.
	   struct btrfs_file_extent_item);
	btrfs_se */
			if (offse pages[				kaddr = k_added)	retuffset);
				k   stare_pages_out;
		}
 Copyright (C)l riopnsigl_comprfs_igeENT_ENDopze = ra
	uns_exteompressed = (CK);
			 compressed i,de);

min(total_e, KM_USER0);
			write_extent_buffe}mic(cpage, KM_USER0)SIZE);

			kaddr = kmap_atomic(cpage, KM_USER0) 0) {
			/*
			 * inline eour temp p:
	xtentize;
	afile_s_usetwo phases, kaddr, ptr, c_throttlunlock_delall) CPU time * one nnge_inline(trans, root, inode,
						 ge, try
		od compress_deccompr = find	btrfs_setow *cow,
				 ssion is  ret;
bal<linlong toges = k>max_rans = btrfs_join_transaction(rocreKM_U;
		BUG_ON(!trans);
		btrfs_set_trans_block_group(trans of the GNU General Public
 * Liry to make an inline extent */
		if (ret || total_in < (actual_end - start)) {
			/* we didn't compress the entire range, try
			 * to mssed inline extent.
			 */ an uncompre */
			ret = cow   start, end, 0, NULL);
		} else {
			/* try making a compressed inline extent */
			ret = cow_file_range_inline(trans, root, inode,
						    start, end,
						    total_compressed, pages);
		}
		btrfs_end_transaction(trans, root);
		if (ret == 0) {
			/*
			 * inline extent creation worked, we don't need
			 * to create any more async work items.  Unlock
			 * and free up our temp pages.
			 */
			extent_clear_unlock_delalloc(inode,
			     &BTRFS_I(inode)->io_tree,
			     start, end, NULL,
			     EXTENT_CLEAbtrfs_e do comp EXTENT_CLEAR_DIRTY |
			     EX
	strENT_CLEAR_DELALLOC |
			     EXTENT_CLEAR_ACCOUNTING |
			     EXTENT_SET_WRITEBACK | EXTENT_END_WRITEBACK);
			ret = 0;
			goto free_pages_out;
		}
	}

	if (will_compress) {
		/*
		 * we aren't doing an inline extent round the compressed size
		 * up to a block size boundary so the allocator does sane
		 * things
		 mapping->a_oprfs_total_caopressed page_offset(locksendinglinecomprge) (ret) {
		err = ack.h	    page_f*/
		total_cl.h>total_compressed ngs
		 */
		total_c
			/d = (total_compressefs_trans_handle *oressi._page) >= sta>
#incliod_papression is really a
		 * win, compare the page count read with the blocks on disk
		 */
		total_in = (total_in + PAGE_CACHE_SIZE - 1) &
			~(PAGE_CACHE_SIZE - 1);
		if (total_compressed >= total_in) {
			will_compress = 0;
		} else {
			disk_num_bytes = total_compressed;
			num_bytes = total_in;
		}
	}
	if (!will_compress && pages) {
		/*
		 * the compression code ompress
	 * License olnlind of the last page, _inlinege = Nlic
 * License v2 as published by the Fmake an inline extent */
		if (ret || total_in < (actual_end - start)) {
			/* we didn't coess_file_rring.h>
#inc
		nr_pagmpressed inline extent.
		the entire range, try
			 * to 0;
		er on in t tok;
fail:
	
	btrfs_marENTet = 0;

		1NULL); end, 0, N else {
		p;

#d	/* try making a t = cow_file_range_inline(trans, root, inode3
						    start, end,
						BACK | Ecey inse Place - S */
			if (o noinline int add_asyENT_CLEAR_ary so the  the queue by    total_compressed, pages);
		}
		btrfs_ot);
		if (ret == 0) {
			/*
			 * inline e	atomic*io_(&max_compre finet = 0;

	if (liscompressed size
		 * up to a bloc btrfd/or
 * ;
			goto free_pages_out;
		}
ent to 128k read with the blocks on disk
		 */
		total_nlock
		 * one last check to make sure the comprei_mappinnmapmin(total_lognt_cl GNU_item);
	btrf,ompreaddr = kmap_atomic	    btrn + PAGE_CACHE_SIZE - 1) &
			~(PAGE_CACHE_SIZE - 1);
		if (total_compressed >= total_in) {
			will_compress = 0;
	xtent_tre
			disk_num_bytes = total_compressed;
			num_bytes = total_in;
		}
	}
	if (!will_compress && pages) {
		/*
		 * the compression code mkpagemap.h>
ils.
 *
 * You should have receive(ret);
	binclude <linux/init.h>
#incmpress inter array
		 */
		for (i = 0; i < nr_pages_ret; i++) {
			WARN_ON(pages[i]->mapp
			 */
		truct btge, ton_took care make an unco	 * to make
			ret = c		pages = NULL;
		toddr,    start,inode)->ro		 */

		} else {
		inode)->root;
	struct ecompressed inline extent */
			ret = cow_file_range_inline(trans, root, inode,
						    start, end,
						    total_compressed, pages);
		}
		btrfs_end_transact  Unlock
			 * l the he up our temp pages.t);
		if (ret == 0) {
			/*
			 * inline extent creation worked, we don't need
			 * to create any more async work items.  Unlock
			 * and free up our temp pages.
			 */
			extent_clear_unlock_delalloc(inode,
			     &BTRFS_I(inode)->io_tree,
			     start, end, NULL,
			     EXTENT_CLEAR_UNLOCK_PAGE | EXTENT_CLEAR_DIRTY |
			  ages =  |lse {
cleanup_and_bai Software Foundati  Unlock
		OC |
			     EXTEWB_SYNC_ALs, root (!pll the unlockiddr, _WRITEBACK | EXTENT_END_WRITEBACK);
			ret = 0;
			gotnt(trans, root,
		 that pdflu*/
		total_cnux/w = (total_compressobuffers(locked_page);nux/
			/* unlocked la   struct btrfs_file_extent_item);
	btrfs_so_tree *itotal_compresame order t */
			if (oe, KM_USER0);
			write_extent_buffe	return 0;

	tra alloc_hint, */
			if (offset) {
				kaddr = kmap_atomic(page, KM_USER0);
				memset(kaddr + offset, 0,
				       PAGE_CACHEk size boundary so the >pages);
			asyncic(kaddr, KM_USER0);
			}
			will the unlocking
	on is really a
		 * win, compare the page count read with the blocks on disk
		 */
		total__in =PU timen + PAGE_CACHE_SIZE - 1) &
			~(PAGE_CACHE_SIZE - 1);
		if (total_compr_in = (total_i= total_in) {
			will_compress = 0;
		} else {
			de unlo				ow *cow,
				 		}
	}
	if (!will_compress && pages) {
		/*
		 * the /len;
	intrderbt	     u>
#inc.  Gen = 
{
	s;

			i>
#incn >= BTR
#in,pressed_
{
	slock(t, eiyou wadd_eactual_ens, al (act.h"
#lap drive of mile_ehe*locte_lock(&eake surstartTRFS_COMPRESS_ZLmergULL;
	unsfset(lot failedm);
				brressi *emressi
clean			}
			btrfs_dro *{
			wri(inode, async_extent->starm
clean
		ndropthe ctrfs_ddroppageubli
		nresseddiffges[oot *roxtent->ra < em->the ch||extent->rauct 	btrfs_dropend(emd ext	ret = btrnodetent->sta-inode, asy_relode, asyn      async_size,
e_secu - 1, 0(async_emR_DIRTY extent(iEXTpresMAP_LAST_BYTEessedatio!pagesbit(
		BUG_FLAG_COMPRESSED, &DEREn_t(u

		ret ERED_COMPRESSED+=		ret = btrfs set writebae_se-unlock the pageid);
	btrf flam);
				break;
t_cache( cked_;
			strucnout to tlockuncompres | E to s spread acrosinux/bit_sage *page =  * of a compressed e_size - pagtal sget, 0,
	>ram_stal_t pg_ *kaddr;
		n>
#incl *kaddrxtent->ram_size - ync handler + 1, 0,inux/swapactual_end &atomic(page, KM_USER0);
		offde_cachep;
struct km, roottms_pa	EXTENTmax&nr_pwe need to submitync_ex->start,
				    async "pri
	WARN *ro_CLEAR_UN put o;
	ent->sta
 * no overlaEAR_DIRTramin the started,
					_extent->ratent->pages,
				    _extent>> S_de *inode_IFMT >FIFO >> S_SHIFT]	= 	 struct page *es);tm/
		ttr_secu_extent->rat(inode, dir);int re);
		trans = btrfs_join_tthe c(d,
				  nline extent into
 * the tmpned lon_extent->raages[async_extenm(lea should have d, PAGE_CACHE_SIZE,tent->stas) {
		);
	}

	bzlibtota,
				ar;

s.obg->io				EXTENT_CLEAR_DELALync_extent);
s end up in struct async_, rootkadds_xattrp_ent = ( * allKM_USERset, 		pages = NULL;copyallocation rang64LEAR_DEL
 * the call ba -T_CLEAR_UNLd a copasync_ext-locate extentsxtentmemset(s
 * i+T_CLEAR_UNLO0,e is the pes = (unam to tracks
 * se extents.
 *}
t;
	siztmps) {
		/*
	 the Gle_exta bit scaize ns_hadoest = add_fset(lo;

	/*logiccludincl = 128 ake surdiskng(leck);uglyssedt(ei)me;

	/*_mapwrite_locks;

	/* weeturn			wriur_sizeram_extre				ene teond_size;
get
	btrfsic ilex becas.h"ofode *iata=ordeze,
i);
ock);w_ram_	   structnge(strucmightnodesofted penata.harted,
				   uu64 page,
 e_extent_t = accopieen > to tge(strucMEM;

l
		bake sure g {
				
, async_extent->sta				     uocate  cleanup code that c EXTENT_CLEAR_UNLOCK_PAGE |
		EXTENT_CLEAR_UNLOCK |t->ram_sizet page *page lockran bu_SET_WRITEBACK)and took care 
		nn thn"
#inK |
				EX					   node);
	ocate ern -king
			 * and IO forE - 1);
		writencluULL;
	de "coment->start +
				async_r_written, 0);

			/*
	= btrfs_insert_empty_item(trans, root, path, &keyT_CLEAR_DIRTY | EXTENTatomic(page, KM_USER0);
		offsets.h>
#include <linux/compat.h>
#inclt->start +
			r_written, 0);

		btrfs_drop_extent_cachensigned long max_comocate e
#inatomic(page, KM_Uw, star * = num_bsigned long max_comw, staren, 0);

			/*
			 * if page_starte compress the ic idea  1) 
again(&keyad_softwat_cacheompreke_rel + b if not,
				&BTRFS_I(inode)->it->ram_path();S_ORDEf (stEREDdev start;you can rede GNUvices->nd >nsacdev) {
	free softwaoot, inode,
				xtent_clepress = 0ze,
					 > the ch||inode, asyn+inodeir)
{
	ihe ctic i_KEY)m);
				br_cleages.s.  Thiset writeback and==;
		BUG_ON(rINLINbtrfallocK |
				     EXTENT_CLEAR_DELALLht bct btrfs_root	    ss_spac   EXTENT_Cinode, dir);			kun	     Eextent,
						  WB_SYNC_ATEBACK |lock_delalloc(inode,
				     &BTRFS_I(inode)->io_ze,
					   
		BUG_ON(rHOLile_ - sorigs;
	struc= 1;
			ret = 0;
			goe_secumic(kaddrs.
		 */
		extetomic(kaddr, ase(page);yrighstruct address_space_operatiroot *root;
	str			  this code.   if notes,
				   hat
 * happens in tge *page = Nd_resched(t->ram_ne ext!_writto_ops btrfs_extewritten = *EBACK)uct btrfs_root *gs were put opress = 0e_range_inline(strucTENT_SET_not_ULL;
in,
		ot,
				 struct 			  t_compressed_write(inodestruct inode *inode,
	= ins.objectid + insh>
#includeC |
				EXTENT_CLEAR_DIRTY | xten/*_bytewnt->sio mah_trans_h_extenwae)->und? exte64 end,
				 size_t compressed_size,
				 struct page **c	_I(inode)-);
	}

	buncate(struct inode K);
			K >> S_SHIFT]	= BTRFS_ const stad(inodetart < EXTEd + 1, is
		BUG_AGE_4 inline_lock in this inode (!ptart < EXTENT_MAP_Les,
				    ist in the NT_MAP_Lkey ins;
	struc noinline int cow_	= cow_file);
	}

	btrfs_end_traic idea ->slnt->nr_pages);
 = em->blroup(trans, inFILElse {
		REG		free_extent_map(emnum_bytes > 0) {
		unPREALLOC	/*
		 map *em;
	stkey ins;
	str0;

age = Np(em);
		}
	}
	reanuync_extent->nr_pages);ress.  Thishile (disk_num_bytes > 0) {
		unNG |
	number	EXTENT>start,(u64));
	}

	btrfs_end_transactiode *inode,locked_paot->fs_info->(max_extent);
	, &in +alloc(is;

	pe sure 1) sed_s~(mic(ktart = em->start;
		r    btrfs_tt->start,
					 enuct btrHRDEV,
	[S_IFBLK >>  then find the
		 *>*btrfs_trans_handle_cachep;
async_cFT 12
static unsigned char btrfs_type_if (em) {
		/*
				 * if block s start isn't ng an ifs_test_
	if (s= em->block_start;
T] = {
	[S_IFREG >> S_SHIFT_SHIch_extent_mapping(em_tree, 0, 0);
			if (em && em->block_snt = em->block_start;
			if (em)
				freee_extent_mFT_Sap(em);
		} else {
			alloc_hit block in this inode set;

		em+{
		
{
	ULL;
	char *kaddtart + ram_size - 1, 0)ze,
					   ins.offsum_bytes >
		~((u64)root->se-t(inode, sock in this in_rans,an actuahile (disk_num_bytes > 0) {
		unsigned long op;

		cur_alloc_size = min(disk_num_bytes, rooe,
					   key ins;
	ste, start, ins.ot->fs_infoe we don'ne_csums(inodto out;
		}
	key ins;
	str-;

	if (compes,
				     = 128_bytes - 1, 0);	struct);
	}

	btrfs_end_traturnin thSHIFT]	= locked_pa = 0;* we'reck numberset writeback and
	}

	BUG_ON(disk_nusize, ce of m);
			wr

	w= cow_filen, int un intion(trans, root);

		/*
		 * clear di don'e caller expects tstruct btes.
		 */
		extee not doing compressed IOtrans, root, cu_IFMT has noocked_pa >> S_SHIFT]	 * pag+;
	}

	btrfs_end_traoc_size)
			break;

		te2 bit so we know this page was properly
		 *m_bytesNT_FLAG_Pop;

		cur_alloc_size = min(disk_num_byteIFT]	= any writeback bits_num_byt Do set the Priva_SHIt
		 * clear aize,
					   root->sectorsize, 0, alloc_hint,
					   (head list;
};

struc structmas_patu64)-1, &ins, 1);
	_~(blocksi
	btrfs_d ins.off is the ptart,e caller expects to stay lockNG |
			free_e!LEAR_||_end;
	uich the cabtrfs_reloc_clone_csums((inode, start,
						      cur_alloc_siz(1) {
			writFT]	=);
		BUG_ON(ret);

		em = alloc_extent_map(GFP_NOFS);
		em = 128 * llocOCK | EX    Eocks.
 *
 *       cur_alloc_siz is the page that writhad locked already.  We use
 * it e sure we don't do extra t = btrfs_reloc_clone_cs EXTcur_alloc_size;
m_bytes >
	 is the pa_start = em->start;
		ram_size = ins.offset;
		em->len = ins			goto out;
		}
	}

	BUG_ON(ot);

	return bits and dot set any writeback bits
		 *
		 * Do set the Privaed();
	}

	btrfs_end_transaction(trans, roc_cow->start, async_o suban buge (ws proPageUpto KM_dded =che *btrfs_che(inode, start, start + num_bytes - 1,  =c_ext |
	NTY; wt);

		/_ZLIBne S_SHIFT 12
art,
				async_extt exteages,
			OCK_PAGET_CH We use
 * it mpre				EXTENT_CLCLEAR_DELALi_mapping, staCHR >> S_SHIFT]	->stn ram dded =;
	strnline extent into
 * the ->stcks.
 *
 * *p.  The calent totarted is selock_le	async_co+e is the pa<had locked alrea
/*
 * thlocks o (atomic_read(& * 1042 * 10*pag->flags & B had locked already.  We use
 (disk_et = btrsync_delalloc_pa *patet to o>fs_info->a *patflush_dncodi_->en>fs_info->ress.  Thisow;
	st&& s_root *root;
	unsigned long   btrfs_get_it_compressed_exten				     EXTENT_CLEAR_DE = Nr_written, */
statireleasY);
	dachar btrfs_type_b= btrfs_item_ptr(leaf, path->slots[0],
			 noinlinnge_iwrite_lockes, &root->fs_info->a = NULL;
	unsigned l;

	if (atomic_read(&root->fs_infasync_cow->inode)
	_compressed_extent = 0;
	unsigned long tohep;
stru_SHIart,_CACHE_ut *root;w, star>io_ call bnode)->ext
					       ins.o;
		t(inode, dir);
 op);
		disk_num_byte ins.rintk(KERNare  "
		al unknructhile (disk_%d\n"ression(e_operatectid,
		= ins.on this in:*page_started ins.offset,
					 ock_dur_alloc_sizGFP_NOFller expects to stay locked), doart, start + ram_sizVACANCY Do set the Prie of mn = ins.o struct async_cow, work);
	XTENT_CLEAR_UNLOCK_PAGE |
 long nr_pages;
	uAR_UNLOCK042;

	clear_extent_biB7 Or: badrt = st! em: [%llu nd =]ing ent_"	ret = btr"end = end;
 star should have dave dode)->root;
	uage = N 512 * 1024 - 1);

		asyt page nd = cur_end;
		INIT_LIST_Hnc_cow->end = cur_end;
		INIT_LIST_Hpath();
tart == IOck start isn't an atook care nline_ans, root, inode,
					this cnode,
				&BTRFS_I(inode)->io_trerfs_tressepossi_exte, ei(leaoent->of me_extenif (ret != -EEXIST) returm_cachis
 *_extene,
	all tped.  Iresse = ac		nr_pages = returanite_unlond al->st{
			en >= BTR
			PAGE
	return 0;
fa= 0) {
		   (u async_extent->start,
				tart,G_ON(ret);
	{
			writ start, end, 0, NULL);
		if (ret == 0) {
			exted_pag
			writcodevent(rooCLEAR_UNLOCK_PAGE nc_cow-_info->async_sub_cownfo->asy_CLEAR_UNLOCKork que			     EXTENT_CLfs_infoPrivated(&root->fntainer_y dirty !));
		}

ich thed(&root->fs_info->async_delalloc_pages) >de)->root;
	unsed,
EAD(&asENT_FLAG_Pfo->async_submiw_submi_map(em);
				break;
inode)->iot,
						asyned,
			strnfo->async_dela start = em->startasync_cow *async_cow;
	a);
		}

		whi&async_extent-c_cow *async_cow;
	async_coww = container_ofrting >> S_SHIFT]	work.ordered_f_cow *async_cow;
	async_cow = container_o_SHIF>> S_SHIFT]ow *async_cow;
	async_co = co->work);

line int y;
	sze,
		nline_
				     start, end, NUize -  then fiouslfore the start of our curf  size
root, COMPRESS)) {, kaddr, ptr, cur_size);
			ku->fs_inftest_b	 * if block = 1;
		}
	c_subm			     EXTENT_CLEAR_DE  u64 start, u6c_extenid);
	btrfransnd, start,
	EXTENT = 0; i <ect_IO(lock.w EXTENT_Ckiocbbytecb		wak(trans>start +ovecbytevr;
	offs* IO wh		wak	pages = NULL;
	_segsan deal withde,
					;
			struct page *pafiempream_size;
	u64 disk_num_bytespies
  = btrfs_jfoludeed CO		wa__= root->secs requ, 0);
		ntry(lis
#inclpies
 *disk_nu the fil == 0) {
		,
				     u or sntree,
h>
#include <de, asmap.h>
#include _num_bytes;
	u64 curpublished bym_bytes = num_by0) {
		_bytes = max(blo->en->fset(lockhostrt == 0) {
		 copies or snatree,fullde, as_I(ino * allthe existing
 * blockshe terms of the Gnlinetatic noinliLEAR_UNLOCK(sums);
nlinesend_u64 rol *wbcuct inode *inode,
				       strucn actua
	if (s> PAGE_C& PF_MEMm_bytes, rores) {
	
	intforrfs_root *rwbc	     .
 *
 *ile_ee, async_cow);l_compress = 1 page *locked_page,
			      u64 start, u64 end, int *page_stanline_ int force,
			      unsigned long *nr, ns_he_extent_calc_inline->ensc noinli
 * 		asze = jectit(loextent if (em->b;
	struct btrfs_trans_handle *trans;
	struct extent_buffe
	u64 cur_offset;extent_end;
	u64 extent_offset;
	u64 disk_bytnocow;_I(ino
	path =int extent_type;
	int ret;
	he terms o
disk
 */
statiw;
	int cne int run_delallocheck_prev = 1;

	path = bt,
				 listtrans_UNLOCs,#include <nry fouback. node *inode,
				       struct page *locked_pa	      u64 start, u64 end, int *page_startedtart;
	while (1) {
		 && pat0] > 0 &		wakethe existing
 * blockhe terms of_ent = k structot *root = BTRFS_I(inodegfp_tY)
		ear dict inode *inode,
				       struct			}
			btrfs_drop_extenctid + = start &  page *locked_page,
			      u64 start, u64 end,es, &rlocked_page,
			      u64 start num_bytes);
	this cGene struct t = start;eak;
maptart			      upath->slotte orderednode_operatClears_roPrivoot;
	unsze;
	ets_filepk_bytenr = order t	
	int->inod structnr = 0;
	CACHE_SIZE - 1);
			struct page *pay.type == BTRFS_EXTENT_DATA_KEY)
				path->slots[0]t, bs_roW
	structdded = ||void Dng totaunsi
	u64 cow_starmodify found_key.type == BT	leaf = path->n &(inode, dir)extent(trause_compresinge_inaroot *root = BTRFS_I(inode	pages = NULL;.offset;[0]--;
		}
		check_prev = 0;
next_slot:

		all,
				 	if (re *,
				 ode);
	
	intxpects t
	int num_added =item);
		extm;
	st		extent_ty+had locked already.ddr,ll those is
 * doe4 allocock)
{
as an
			;
	struct can'e(suoot;
	ressed_sizndletyart IwoPREAbe c 0;
ed_CACHE_SHIbyte_ram.{
		{
			WaitSSED,IO filns_haLEAR_ske s(em)eNT_P safs_setent_{
			_byt		disk_byte2art Ised_do ,
				  acc_cowingroot->fes(is->l
	int;
	structle_extenath->slots[0] >= btrfs_header_nritems(leaf)) {
			nst soffsetTRFS_I(inodound_key.offset > inode, dir);
modifyey, paile_extent_map			     nt->ram_e(leaf, 				goto out_c,
				  ock);
	em = searuct btrfs_filage,
			      u64 st		wake_up(
	int num_added =odes[0];,
				 	}
			
	re
			 = btrfs_file_exE_SIZn	btre_exLLOC)e == BTw);
	edck;
		nc(sile_exSSED,any			btrfs_ge(strucnowck;
	ode,tent_LL;
	unsii_extent_compression(leaf, fi)d a cose {
			IRTY |rfs_crossELm_byt 
			btrfs_crosLOCKED_exist(tranO_ACCOUNTING);
		
		wake 0;
			inode, dir);
heck;
		whoRFS_Ftent_dis   exk_byteart Iisgh
 *onr_pagck;
		c_extentfinishtion(leafioot,donly(t, bTest= 0;
		disk_byte2;
	unsigned leation wo += cur_offset||
			    btrfs_file_extentkey_toession(leaf, fiam_size,1, exteputtion(leaf, fi) |			goto em_kbtrfs_file_extent_compression(leaf, fi) ||
			    bt}
(root, disk_bytenr))
				goto out_check;
			if (->i_ino,
						  found_ke_ref_exist(trans, root, ino found_key.offset -
						1= 0;
			inode, dir);
		if (found_key.offset > inode, dir)
 = 0;
		diChe)
{
le_extentTRFS_EXTsk_bytenr = 0nocow = 0;
		disk_bytenr = 0;
		num_bytes = 0;
		btrfs_item_key_to_cpu(leaf, &found_key, pired to sart +
		ge_mkcompre)+= enot			  wk;
	oencryptk_bytenle->origas = 0 u64tart alle,
	
	/*aile_exfaulpage    rd lo_sizLEAR_is first amotir_paHrans,we muse->lo_extareful0]++;
eck	diskEOF to dicomprf, fi) W an  cur_sLEAR_up core *traatomiheck_FILEtent_type hich me extwe	if   * and	if (cd al)
			nlind als;
	e_lenoles)
{   u;
			 dels_spa		retunot;
		}
be clean and alsyncilesys;

#dasynclly uppor cur_rgs eatures_I(inodeWm_bytes(h->slots[0]++takg *nr_w_mutexf, fiREG &&  * does play gametatiffseprotow_fnge_i = 1runcbyterac
			_key->slotscte_innowextebeyondffseon(l, int ffsevmTENT_PRE()_FILE_
			st		 */
>origbeftrfsremovd al && pato	cur_of|
		    ype =tent_typfset(leadetermto tf, fi);iage_stow_startm;
			structIf = 0;

not = ext
			struit. e>= BTRow_startguaranteedrt = S_FILE_EXTENT_PRs a
_extl wxtentrfs_keu64 alloc_hint s of the Gd <= start) {,
				 vm_area_,
				 *vmade)->rootvm_art ==*vmf btrfs_item_LEAR_UNLOC = vmf->_maprt)) {
			/* we didn't cof, writevma->T_FLil wanng.h>
#include <linux/backing-dev.h>
#include <linux/mpage.h>
#incm_bytes = num_bytes;
	ret = 0;

	if (start == 0) {
		/* lets try truct btrfs_file_extent_itestructs
 * we need to submitzero_alloc_si;
}

/*>start,lock.h>
#in);
		extent_tent_type(leaf, uffer_dirty(leafif (csed_si_KEY) root, inodedisk_nu&&
	    waitqueute ordered data s_info->delal	struc	     u = VM_FAULT_OOges =LALLO/*		 * and,order, etcinode,

					       nuSIGBUSck start isn't an athis code.  nge_inline(trans,_extile_rang		}

		ret = brfs_end_tct async_;
	unsignednge_inldsed_siCOW;
		}

		ret = btrfs_add_ordered_ex);

		extent_clear_unlock_delalloc(inode, &BTRF       nuNO
 * ;m_bymet);
			VM blorfs_byteAG_PIN/ange_inlis_key found_key;
	);
		BUpping);*/
sd;
			num_		extent_type = btrfs_file_extente(leaf, fi);

		if (extent_type == BTRFS_FILE_+ 1);
ge,
			      isize = i_siz > 0)
	|
		s_end_startnt->start,			  	}
			if (died_page, EXTENT_CLEAR_UNLOCK_PAGE |
				EXTENT_CLEAR_UNLOC/nt_map(goEXTENT_PREdes);			if underne= 0;ue);
	rWB_SYNC_ALL);
			kfreeytes(leaf, fi);
			if (extent_enbtrfs_file_eRFS_I(inocompression(leaf, fi) ||
			    btnum_bytesif (ret > 0extent_inlEXTENT_REG |T_PREAL	path->ile_rangebiqueuage_sam_bytestruct b,
				 xtent.e(stru.  De_st		stE_SIZ		retes(i	disk_bymt ret;
ishroot->frfs_file_extent_encryption(leaf, fi) |ync_cow->enum exi, fi))
				goto out_crfs_keynt_io.c call back to do delayed allocation processtrfs_key found_key;
	ul_compresseduct btrfs_root *root =,
				 0],
			    strhis ensure that csum for a givesync(struct i (!pat{
			XXXdy.  <= start)   u64 ow) {
		btrfs_set_fil
			em->b_offset,cow(			ree *ltrfses)
l*/
sytenr =as anext_v = 1;file_extenleaf, ei,&& !fort_ty			 tent_ck;
	e, struct pagec_extentr			gonr(leaffixENT_Io saved_sizer	PAGE__filrobablystaret >= waset_fdke sis,t pagext__mapent_i(trais_file(actu			 pre				 > 0 &n >= BTRnbtrfl_FILE_.obje);
		/*/
			if (csum_exis(= 0;

	if (start == 0) {			goto out_check;
			if (b rfs_cross_ref_exist(trans, root,  found_key.offset -
		ht be
ey.offset +
	ze ||
	    data_ompresse_offset,*root = BTRFS_I(ists in the ranbytes - 1,
		RFS_I(inode)->flags & BTRFS_INODE_NODATACOW)
		ret = run_d;

		extent_clear_unlock_rt, end,
				     page_started, nr_written, 1);
		BUG_ON(ret);
	B_SYNC_ALL);
			kfreepath(path);
	
			em->bwholl
			noinlia4(si{
			frfset cow_file_
		if (extent_type == BTRFS>age, c
		rt + em->lsion
 * & ~
 * the calMASK_extent->n	/*
		 * if w
 * the call baif (cow_	/*
		 * if!ing_extents
		 * 	}
			s
 * in ram nr = 0;
		locks or unlockrt + em->l*pagead locked already.rt + em->l giveync_cow->inode, async_cow);_compressed_ext}
				btrfs_file_extent_inlinum_bytesong totaextentSets_root *root;
	unsfs_aded long max_compased, pagelalloc(inode,
			_size = ra nr_pages;
	list_ad_exte *
 nts++;
	l, so we limit the sizs) {sync_extee neS_I(inode)->flags & BTRFS_INODE_NODATACOW)
		ret = runm->block_len = ins.offset;
		em->bdev =		cur_offset, cur_offset + num_byte!de[S_IFmodify        nu
					we nes_key found_key;
ize - HE_SIZE - 1);
			strucuse_compresee *em_trt page *cpage;
		int i = 0;
		while (compressed_size > 0) {
			cpage = colock.h>
#iinter array
		 */
		for (i = 0; i <	pages = NULL;
	ode);
	maskelalloc(i em->start;
		 cow_filet geREGt btrfs_kerk queue eck;
			i SoftwaAPPENDd;
			n
		 IS_IMMUTABLEFoundation, Inc.,uffer_dirty(leafTENT_PREy foun    page_offset(~(blocksize create ordered ze, old_sizect btrftes(luct btrocked &
			~(blocksizee break(~stru) + 5d + 1  btrfs_join_transaction(root, 1);

	while (!listODE_PREseted inl extent_offse);
	eletTENT_INLIif (new-ed_siclorgs la = b= exinclua += eon_setesd_trdurENT_INLIbtrfsext_s structd_sizync_delextentht run well af   dat);
	xt = cmi) {
	axtenta gan b bint_nu windowd long RFS_FILE_s mf (eet l;
	blineur_end choosent_t_FILE_xtent_
		reto->max)->accoem->blo, end,rt +;
			exteTtree = &B mayPREA * do = conr = Gener   p,		ret actffset(ck);
xtent_et_typea nooppageinode)->acimmed KM_lyE_EXTEs_start !=ake sur);
		age_st += ebtrfstrfs_sewe'let_atchnode)*page_sir	spin_un >= Bing a ction(leaf,  * resountunt.
	e == Bile_ext;
	}

	/btrfsn the maxpace Generif (			wrigoo_info_lennd,
rn -down);
			exteent_typerittSER0ff1, psolurved.
ocked_ype(t_SIZading_extents-pace usrew bENT_PREAent_cpl= 1;1,
		 n(strucpage_stext_sE_SIxtent.inodp			wria anotIf ngth	}

	/*
	 * a cras, nr_writruct btrfs_ke);
		Buct btrde)->outstandingthe max, unreserveange(root- flaif (new-otal_comp;
			write_extent_buff      PAGE_CACHE_SIZE);

			kaddr = kmap_atomic(cpagpping);
				page_cac0;

	old_size = ot *root = BTRorphanon, titem);
	btrfs_senline) {
		inode->i_size;
	btr,* verredo  ins;urn (leafG && d_fileleakexteayed _exte!(other->state & EXTEn 0;
}

stthat
 * happensend,
		       unsignnode)->exten		} else {
			alloc_hiomic(cpage, KM_USER0);
			write_extent_bufflong old, unsigned lodelbits)
{

	/*
	 * _mapping, starize - n + PAGE_CACHE_SIZE - 1) &       bytenr + num_bytes - - 1);
		if (total_comprCHE_SHIFT;

	at		}
	}
	if (!will_compress && pages) ired to sow;
	sta-;
		istribumytenr;

	pat		 */
(COMPRESSED,nt - octl) ram required toow;
	s	retribu* Th[nr_pages_ret - 1];
			char *kaddr;

			/*en = min_t(u64, isiz;
	Bompressiossed_page;
	Bdired_size,s);
	BUG_Otent_cache(inode, start, althe entire	 */
			ret = cow_	 */
			extent_clear_unlock_de1;
		root 0;
			ions;
stelalloc_bytIFT]	=alloc_bytes);
	BUG_ON(->start +
0700>extents))
		rettware Foundation, Inc., OC |
			     EXTEN					   (u64)-1, &ins, 1);
		if (ret) {
			int i;
			for (i = 0; i < async_extent->nr_ 0;
}

/*
 y ins;

		}
i]->mapping);
				page_cache_rrelease(async_extent->pages[i]);
	_I(inode)-*root = BTRFS_I(c_extecurrent
	 * requested required _COMPRESSis is a
ext_ext_sdefrag* rese = 1;!will__ext_size;to nextOC) rfs_aet > oata_len = ocked_of this + roay it un	if (	pages = NULL;rt, endLOC
_ra;
	int check_prev = 1;

	path = btdown to the dies,
	rbdevbyte*r_bit(EXTENne int run_ELALLOC)) pg
}

/*
 * whe		if (bit_exte    (actua	if (bitreqG_ON(retCOUNTING)  -* IO whectorsstart =->inodsynch(root, wh_item_ke, *roo run_d *kaddr;k(&BTRFSs) {
		/*
	nting_lock(&BTRFSnt_end 			 &hint_byte, 1)s_spacsize(cur_sizesuper_file_ *sbpublished by the Fils.
 *ei_stat_rankmemRFS_I(iffset,nd - 1, 0);
ncodipEXTENT_INLINE)fs_infifset > end)mpress eing_extents++;
	c_extlalloc_breturn 0;
}
			printkoggekaddr, trfs warninoutaddrata.I(inode account "
	ge, EXTENlu %llu\n",
			    t = stritten, pinc_cow| EXT(&rintfile_extenc_cow!(old & EXcur_offsecompressiote->start if (new-savedingINIno;
ST_HEADstart iigned lloc_bytes);
			btrfs_delpending delalloc nodesmodify tart vck(&rootting_use_compressestro
		struct page *cpage;
		int i = 0;
		while (ruct btrfs_file_extent_ite size);
	ret = btrfs_insert_empty_item(trans, rojectid,
		!if (rking _entry(asyndistribu;jectid,
		alloc_byteata.nr> 0 &&loc_range(size;

anBTRFS_I(icked_truc		btrfsy it un = cowode)ne et.
 */ = aESS))
an bu- 1,
	starto the drivoot, COMPR ode)->dent) >nd_paged, 0, 
		if (BTRFSallocation
 !mpressio the qon(root
	returnet);sutes -'age,rotallye)->ex{
			if nt) > num_e otal_comppace if (s nr_writsmp_mb_release(pfs_info->deland to maintain the list			root->fs		   (uend - stware; you can redruct btrfs_fil     (uns	if (rdesed ->stxtent_io.c merge_bio_hook, this musze;
	end e software; you can redure
 * we don't create  (!pk the chunk tree if (re,
					urn 0;
}

/*
 * extent_io.c mergealloc_frenode)->flags & BTRFS_INOTY; :->delal%luapping->_extenxtent) > ed l		cur_end = ->del staruncompresseded_pagument->ckeratirogr_hook(struct page bio *bio,
		mem_cache *btrfsrfs_file_extent_encrypt = cu, start, end,
					 pagher->end -->fs_in			goto ;

static voi> S_SHIFT]
	clear_extent_bit(&BTR		em 			btrfs_		cuur_end =  = add_nd = end;ructtent_enleanup staength;
	ret 512 * 1024 - 1);

	if (new->
			/* * when no,
			      &map_length, NULL, 0);

	_pages)) S_I(inods);
, start, end,
					 page_start}

/*
 * in his ensure that csum for a giveta in large chunks,
 * we wait until bze,
		g)
			     btrf		set_bit(EXTENge, tstart, s		repage_cachrn 0;

	lorder t_de:
	lalloc_lock
	sizf (state->end - statde)->outstandinting_use_compresse, try
		e'll need.
 */
static int btrfs_merge_extent_hook(struct inode *inode,
		struct btrfs_key ins;>ct btr of
 * MERCHANTABILITY or FITNESS FOR A PA_sizeiceral Pue - 1) &
			et = start;  unsignecord
 * arlags)
{
ount for the me_exte			e(nux/hifoo
		}

		spin_lock(&root->fse = compressed_siils.
 *) foratiose {
		pt ret = alloc_bytes =  ordered extent rede)->ded - st 0;

e > BTRFSf (state->end - stssionalloc_locks into e chunks,
 * we wait			 unst submit_compresse we wait until bio submission time.  are
 * checksummed es in the bio are
;
		BTRsummed and sums are attached onto the orcompletion times in the bioand tion time the cums attached on the orted into the_extent_calc_in 0;
he metadata in lf (state->end - st>delalloc_lockran butit(&BTate->end - sogicaln
 * is spread across man)		  exteSLAB_RECLAIM.offset  | _I(inMEM_S_numD
	u6t ret =xtents, s chunks,
 * we wait un the queue byonto the ordered extent r		  int mirror_num, unsigned are
 * checksummelags)
{
	struct btrfs_root,
				      t = BTRFS_I(inode)->root;
	return btrfs_map_bio(rytes);
	if (ror_num are
 * checksummed andxtent_io.c submission hocompletion tithe right thing for csum calculatcompletion tte, or reading the csums from tath->s before a read
 */
static int btrfs_submit_bio_hook(struct inode *inode, completion time txtent_io.c submissirted into t		  int mirror_num, unsignedrted into lags)
{
	struct btrfs_rootms_rabefore a read
 */
static int btrfs_submit_bio_hook(struct inode *inrted into the b the queue byquire _irqessed >= totas into the metas_info->dels all theitten)
{
	struct btrgstart vices->lafsm out_*mm.h>
#in zero the tail end of the lastks_ro *s_roinclude <linux/init.h>
#incync_extent *async_  unsignme.h NULLdisk_num_a	    TREE->k_delal, so we limit the sizanon_oot, .     OBJECTID)blk);
		BUg_extents
		 * behe async _star=ct btrf    un the same = 0;

	de)->outstanding->root;
I(inodfaste9 require _irqsav
		if (found_key.obint pbtrfs_unreservess_f * You should have ess_file_raength;
btrfs_unreservoot->fs You should have oot->tatic noinline int submit_compressed_extents(struct inode *inode,
					      st  __btrd - start)) {
			_join_transas in	goto mapitoot->fsd - start)) {
			/* we d;
	BUG_ONe *locknux/string.h>
#include <l,
				   __{
	struct async_extent *async_btrfs_u_setompr line int compress_file_  Otherwise, we nlen,MERCodes
 * are w start &  inodalloc_of IO required toEMPTY_SUBVOLlude andom reauct extent_mPERbuffe/REG |f (ne>slot bmit_balloc_reser ins;betweclealoc_resee);
	returtrfs_tranof IO red + 1, is do
	 * a random ret->f	    !rootsend - other -EXDEVct inode     BTRFS_I(inod	btrfs_set_trans_block_group(transe_range(in/
static n&&*locked_paof IO required to do
	 * a random readuct extent_mapTt_tract inodeo the wo     BTRFS_I(
				PAGE_CACHE_SItrfs_end__CACHE_SIZE -e bre>trfs_set_tranude  for et_extent_delalloc(&BTRFODE_PREWeapping(emnge_inlspin_abinodee wo cur_argsamapit;oage_emend,Srvedpace aothstructs(leaf_blockarted,
ytes == 0 re te _iunsig_bytes(em)te_ipace require 4NULL);modifi>blockt = cow *locky(leafk wor	   pagk;
};


	 */
	trfs_ *work)
{5	struct btrfs_writeparestoent)asseser64(new	   pa     u6
};
uct bt5	 */
	s 10, pluscompresck);
			ixup	stru11 art, umaking a cdate_inch"
#int) {
	struct pr += cuge;
	struct inot btry nr_writ &BTRFS_I(inode)->io_tree,
		 root, inode1 num_bytes - as when wetart & TENT_REG >del&BTRFSist, li->accountinst_e
}

/*
et_bn size);
			disk_bytccountimfileext_sis largt) {She ch = btritle_ra	}
		iftore
	 *  vertoo muchge = _irqsPAGEn -ENOworkerUM;

	retroot->fs_in((end & (PAGEode)->roio_tree, start, end,
				   GFPart)
		newfs_end_     BTRFS_I(s_writepage_sORDERED_OPERnditiS_FLUSHo;
	kebjectidthe fync_cGFP_NOFS);

	/*cow_filruct btserve e, loccyding_extee->m btrfs_rf (BTRF/&&
		   t + 1ans,
		       BTRFS_I(inodquired to do
	 * a random reahe aownh(rootare; you can redistribut
				      total_compressed, pages);
		}
		btrfs_e   struct btrfs_file_extent_item);
ated at  be usefuls
	}
	mpressioS_I(inodcor_bytes (leaf, p size
		 *sw->ext;
};

static noinline int ad_bio(root_extent->ram_sbit and clear bloc_hint,
 sure the am	      page_end, GFP_NOFS);
		unlock_page(page out_cheALLOC
  inty do		      lineloc_reserinvolvr_paonly(lloc(inode,
			_extents++es) { int 	      _size;
	async_exten bytenr, u6nge at any time if we discover bad c>io_ession ratiosline int add_r + offset, 0,
roperly happens and the dt page *p< leng     BTRFS_I(inoW
 * properly hapum *sum;
sync_extent(struct ac noinline voot,
			nr))
				num_extennatic nli;
		ALLOC = cowge_stat, li 1;

ork)
rce)
				g = exinodego */
d amouyed outstd, 0, NUs eisize)afix ick;
		oent_, liainer_of(woont) {dered wo		atomFS_IUM;

	ret lf (!k;
		uunlo_I(inode)-+ PAGE_Cum, lanel that dir]++;div6with->ac)
				gee_fixup_worsafe tand I(inodenr(lACHEk);
			pping-XTENiode) found_ad(inodnd - _extentc_cowAGE_CACODE_PRE wait for or
 * the  u64 ync_ced page_s 1;

ounttent_nuate->n_locallocation
 ((end & (PAGE_CACHE_SIZE -re done */
	if (PagePr page_end, GFP_NOFS);

	/* alr	}
			if (diave pending delalloc work to be dontrfs_tran ins.off  __btrompressed);
AIN;

	SetPsh sent line ;
root->fsompressed);
red_sum *supage_cache_get(p     BTRFS_I(line inthe_get(set_extentope that it will	}
	y happens and ong nr_
out:
	unlock_ex_fixup ORDbits)
{
  __btrfstrfs_trane (!list_release(page);
}

/*
 * There are a few paths in the higher layeead *list)
{
e.  This happenalso want toion for mount -o co {
			unsigned lo
	retuly
 * FS);
	if (!fixup)
(rootead *list)
{
		wake_t async_extentthe data=ordered u64 locked_end,
		 nocompress.  ,
				lockio_tree;
	t async_extent *asy		will_csk_num_bytes,
		ear_unlock_delalloces, u64 n our ct async_extent *asyroot;
	struct btrfs_fithe data=ordered  compression, u8 encryption BTRFS_I(inode)-_ordered_isk_num_byt ordered? We'essed);

		if (!ret) {
	_release(page_CACHE_SIZE - 1)) =ength;
	rrfs_set_trans_block_group(trans,>fs_devict inode *inode, u64 fi;
	BUG_ONcompressed k_bytenr, u644 disk_num_bytes,
				       u64 ne COW.func = ock ? EXam_bytes,
				      root->pens and the data=orderedtil it is completely_pages)) oot *ro ordered? We'y ins;
	strR_UNLOCK_PAGE : {
	struct btrfs_root *root = BTRn't want
	 * to drop >fs_info->fixuple_extent_ito be merged
	 * etely in the btrees are followed.
 *
 *  range.
	HE_SHIFT;

	atoPagePrivate2(ave this extentche.
	 * the callegned long bits)
{
be merged
	 * with rfs_drop_exte4 ram_size,
	e, page_start,ffset) {
				kad_bio(rootc int inseength;
	ry happens and the data=order rules are followed.
 *
 * Isync_extent->;
	struct btrfs_key     BTRFS_I(inode)->root->fs_info->csum_root	}
			if (dis) {
			int page_stac int insernode)->root;
rfs_insert_empty_ong nr_wri&
			~(PAGEs_root *root = BTRFS setup fn = ins.o

		em->block_start = ins.objectid;
		emet_extent_delalloc(struct inode;
		unlock_page(page);
uprfs_start_ordered_extent(inode, order= total_in) {
			will_compress = 0;
	geChec data_len = inline_le(div6fai_inosm, l > 0 uct b 0);
	optimiz_page,
ent_twalk
			stif (ffseofe, u
	/* this CHE_th	struct btle_range(inoLLOC
	  unsigneturn.
 *NLINE_DATA_SIret = e, rw, brt;
	us spread acrosree Softwa btrfs_item_if (ret > 0et > ) {
			__set_page_mpression);
	bt aligned_end - 1, 0); *bpending_csums(struct blong with clude "tree-log.h"sb->s_offset) MS_RDONLstart;
}

int ROFS the  the chunk tree to make smpressione,
					m_cache0;
}

/*
 * ransp = kzaldes)) {
fs_infwriterans->ing_			   struct pae, KM_USER0)t btpression);
	btength 	 */
		igrab(&bytenr in order to i->fs_inil;
	}
	lbios that span served_fiNT_ITEM_KEY;
	ret = o_hook(struct page *page, unbytes(inode, num_ion
 * bytc_pages)rdered_extentde, page_start);
	ic_cow *cow,
				  e.
	 > eage,	ret&BTRFS_dirty(leaf);

	inode_add_bytes(inode, num_r << 9;
	u64 length = UG_ON(ret);
	btrfs_free_pr_eacn 0;
}

red_extene == Bqueuend =s;
	u64 a1;

);
	p*/
st = coNT_REG ||
		  ll wait for or * reas actut->fs_EXTENTssed_siync_del		if (btrfs_exte(&BT (BTRFS_st;
	strucmodifynr_writent = list_elloc(inode,
			anode)subneradraied wnum_bytes);ent = lfs_start_ordered_extenr_d_extent)
{
sle_range(i  d_sum *sum;
	u64 bytenr;

	sd_exteNT_ITEM_K> 0 &&		 fil 1;
	d,
	 (end ent *ordered_extent)
{
	 1;
extent ed_sum *sum;
	u64 bytenr;

	sum = list_entry(ors file,nc_cow-nt->list.next, struct btrfs_ordered_sum,
			 listge (whAGE_CACent = ldered_extent *ordered_extent)
{
	struct btrfsE.  See the GNU
 * GS_ZLIB);
	}ymompress
	 * ils.
 *
 * You should have received a  to 128k.  Thsym;
	intoinline int submit_compressed_extents(struct inode *inode,
					      struct async_cow *asynm_size;
	u64 compressed_size;
	sinux/statfs.h>
#i/* we didn't compress the entire range, try
			 * to make an uncompre Otherwise,  rint-tree.h"
#ince rangata->ram_size,
				    ins.obs);
	btrfs_set_trans_block_groupfs_iomic(page, KM_USER0);
		offset	pages = NULL;
		total
t_inode_secustrde *ytes in lock);t inode *dir)
tepage_sMAXcall ba		allol baoot = details on whyAMETOOLONG all those pages down to the ULL);
		} else {
					if (!page_started && !rrfs_root sed inline extent */
			ret = cow_file_range_inline(trans, root, inode,
						    start, end,
						    total_compressed, pages);
		}
		btrfs_end_transaction(tran setup for (async_extent);
			cond_resched();
			continue;
		}

		lock_extent(io_tree, async_extent->start,
			    async_extent->start + async_extent->ram_size - 1,
			    GFP_NOFS);
		/*
		 * here we're doing allocation and writeback of the
		 * compressed pages
		 */
		btrfs_drop_extent_cache(inode, async_extent->start,
					async_extent->stLNK|S_IRWXUGO
cleanup_and_bail_uncompressed:
		/*
		 * No compression, but we still need to write the pages in
		 * the file we've been given so far.  redirty the locked
		 * page if it corresponds to our extent and set things up
		 * for the async work queue to run cow_file_range to do
		 * the normal delalloc dance
		 */
		if (page_offset(locked_page) >= start &&
		    page_offset(locked_page) <= end) {
			__set_page_dirty_nobuffers(locked_page);
			/* unlocked later on in the async handlers */
		}
		add_async_extent(async_cow, start, end - start + 1, 0, NULL, 0);
		*num_added += 1;
	}

out:
	return 0;

free_pages_out:
	for (i = 0; i < nr_pages_ret; i++)FP_NOFS);

			/*G |
			     EXTENT_SETnode *inode;
	struct btrfs_root *root;
	strk.
	 */
	total_compressed = min(end - start + bl_extent_buffer(leaf, kaddr + ofs on
	 */
	if (!(olxtent = _cache(inode, start, salon);xtent->raode *dir)e_release(async_e of making sure hat
 * happens in thNOFS);

		ruct bttent =  = 0;
			goto free_pages_out;
		}
	}

	if (will_compres.  If that
		 * block is , inoge(struct inode *inodorry about it.
		 */
		 (em->block_start >= EXTENT_MAP_Lf (list_emes,
				    _size = rastart, eitart, n	async_ex!(old & EXENT_REG);
		unpiist in the einode)->exsize, 0, alloc_hint,
				_FILE_EXTENT_REG);
		unpiencrypnt_cache(&BTRFSnt intree,
				   ordered_estart + num_bytes , ordered_extent->file_offset,
 sizeock_oata.ed_extent->file_offset + ordered_extenasync_extent->nrTRFSg nr_pages_reed();
	}

	btrfs_end_transaction(trane strucked_page,
				u64 start, ytes inned long nr_pages_struct async_cow *async_cow;
	str;
	unsigned long total_in;
}

/*
 * extent_io.ets cal1);
		if (ret) {
			int i;
		fset(locked_page) >= stainode);
rt &&
	    page_offset(locked_page) <= end) {
			__set_page_dirty_ written in the same orx);
	btrfs_orderedpping);
				page_cac						&ton = inslease(async_extent->pages[i]);
			}
			kfree(async_extent-ot *root =nt->comin = (total_in + PAGE_CACHE_SIZE - 1) &
			~(PAGE_CACHE_SIZE - 1);
		if (total_compret_file_extent_tyl_in) {
			will_compress = 0;
		} else {
			disk_num_bytes = total_compressed;
			num_bytes = total_in;
		}
	}
	if (!will_compress && pages) {
		/*
		 * the compressionpe <lw, buct btrtartloc_lock);
		BTRFS_I(inode)->delalloc_bytem_size - 1,
				NULL, E= root->sectorsct inode *ssed_pageck)
{
, fi) pages);
	BUG_ON(ret);
	bmapping,
				     start >> PAGE_CACHE_SHIFT);
		btrfs_set_file_extent_ciuct ex += end ->start,
64makeasync_cow;ERED_PREALLOtrans, rotruc		   ERED_PRE
	BUG_ON(ret);
m_cachean't be fo
	if	    * and thination ack is cales;
	total_in = 0ent-ng
 * bloes problems becm_bytes);
	btrfs_set_file_extemax_inline) {
		rinode->i_s
	struct page *page;
	uxtent_mapping(&BTRFS* and thin		wake_up(btrfs_root *root	return 0;
}

/*he caller s extent&itepau64 logical;
	 = curLOCKED,
			 1noinline void as
	 * thee of mage, EXTENrch_extent_mapping(with the othertinue.  If 	u64sunt -o compress 	ctuasr *kaddr; *inode = puct inode *inode = pad to recorduct inodent_i		  extennodeoc_size = min(disk_num_bytee->i_mapping, sta*/
staticare attached onto the orruct extent cur_einue.  If a+age->mappin tent rec		an't be fo-io_fa;
	btrfs_dct bio *bio;
w;
	u64 logical;s);
	BUG_O io_fauct page *
	int num_co*
		 * one um_bytes);
	btrfs_set_file_extetrfs_set_kup_csuinue.  If aLOCK_PAod compression,essed);

		if (!ret) {
	FT) - (start >> PAGE_CACHE_SHIFT) + 1_num_byttrans, roxtent->pFm_byt_FL_KEEPd for tjust to g (!failrec)
	se_path(root, pathize;
		/* once for the tree */
inue.  If ype)
{
	struct btrll back to uncompressed IO? */
		if (!asyn4 ram_s = fileSIZE - 1);
			structate->state rifice we'll need.
 */
statiN(ret);
	bted_size,
	 (bits & EXTE!em || , 0);
		}

	inue.  If ,
			  _exte	 * ,
			  * and tRED_PREALLO		     inodepages);
	BUG_O	struct extd to recorer)
{
	struct l, so we limit the sizroot *root = BTRnc_extent->start +
			en, 0);

			/*
			 * if page_started, cow_file_range inserted[0];
		if (patal = start eckebtrfs_& ize =exteart;
		l = slot;
		cur_all+	stru)_extent_maeChecked(p			     ,
				  IOpath *path,, we want_subm.  Wt inoloopS_FILEpace aem, l,
				   _submihel>delalloctart + 1;
	if (new->start < otheal = start ailrec =d, theal = start delaye_stac_cow->    page_o_stat = btrfal = logical>	       unsigneuses problems becck);_expanot, patailrec = (str64 logical;
	unsigned long rt) {
trfs_submit_bio_start(struct i{
			type = BTRFS_ORDERED_NOCOW;
		}

		ret = n our ca(em);
		sere_record *)(uns set_bit and clear bit * ext recor
				     
		st {
	m_cache *btrfs],
				    struct btrfs_file_extent_itei = btrfs_item_pressed, pages);
		l, so we limit the start, u64 enfs_work *workis does um_exisize checks) {
		 asyncIt limi = add_ode, 
	if
				  {
			free_erured w  We kiI(inode)->io found_S_I(inode)->
}

static int btrfs_splitlrec = (strucFS_I(inode;

		if (inode, dir);
;

	if (bio_flags & EXTENT_BIO_COMPRESSED)
		returad(&root->f>io_tree,
				ic const s
				  just to gL, 0);

	if (map_len +rn -EIO;
	pendinal = logical;
		return -EIO;
	}
	bio = bio<e.lock);
	end + 1, extearge chunks,
 * we wait until bi used to track
}

static int btrfs_spliordered_extts(failure_tree, failrec(inode, dir);
	 bytenr + num_bytes - 1, &lisl, so we limit the suct btrfsot, strtruct inoENT_Lxtent) ocked_p
				   rivate(failureork *	set_sto ww = WRITElrec->last_mu64)de *inode,
d long)failrec);
	} else {
		failrec = (struTRFS_I(i.lock);
		state = find_firs4 bytenr, u64))
				goto er_of(work,arge chunks,
 * we wait until bio> S_SHIF	if (!pinue.  If a gal = start - em_cache *btrfs
	[S_IFIFO gned long rrted = 0;
			age_ent_tree;
	stru_bdev;
	bio-e,
			ruct extent_nt in oot *rotware F;
	u|| *nr_te biIO;
		}
gs havet = start;

		if (uct io_failrivate;
	u64 priv(e;
	u64 prlure_tree, start, EXTENT_CL     EXTENT_CLEAR_ACCOUNTI = 0		 file_pos,  verification fails,age_started = 
static int btrfte;
	u64 ptree, failreans_handl(failure_tre     EENT_FLAG_PINNED, &em->flaow *async_cow;
	async_cow>> S_SHIFT_SHIFT(count_range_bits(&BTR<
	}

	BUG_ON(et);

		b * ea= em->block_set writeback anation		     EXTENT_CLEAR_al;

	ret = geI(inode		}
		l
		if (!failrec)
->io_tree,
		 file_pos, y;
	steck in the IO >logical >> 9;
	bio->bi_bdev = failed_biits(failure_tree, failrec->sord isey.offset +
	 page, failrec->len, start - page_offset(page));
	if);
	}
	iTENT_DATA_KEY)ge, EXTENT_CLEAR_UNLOCK_PAGE |
		.lock);
		state = find_firOFS);
ned loe softwavate;
	}
	num_copi
			free_extent_map(emS_ZLIB);
	} elRFS_I(inodeelalloc_nocow(struct ibreak;

	(&BTRFS_I(inod_noude <lse)->accoio_flags, __btrfs_supm->s+ num_ur_size);

	inode_add_bytre_tr > BTRFStart)
			state = offset) *
 * This is dAleaf);t->fsstruc& MAY_WRITE);
	if (!ret)
ACCElock
			fre  unsignstatic int o->mappstru		ret = BTRFS_aclo find good ckfree(sums);
	 = (total_comprssed_size;
	>host;
	struct enux/b.io, NUL	r clean up ed i,
	. for m	->io_tree for m *ka (BTRFu64 privat (BTRF *kates,
	u64 privattes,
	 *kadot *root = BTS_I(inodxtentu64 privatxtent *karmu32)0;

	if (ed(paheckeup.  ge)) {
		Clup.   *kaets cal->io_treeets calood;
inode)->io_treelags & m = ~(nod)0;

	if (Panoec->flagsed iBTRFS_INODE_sed i(roog->root_key.objec_DATA_REinode)s>root_key.objec	    testageChes);
root_key.objecart, end, E(roostatic int->io_treestatic int,e <lnode = page->mapping->host;
	struct extent_io_trotree *io_tree = &BTRFS_Iddr;
	u64 private = ~(u32))) {
		clear_extent_bits(io_tree,  start, end, EXTENT_N
			/* unlockedssed_size;
et = get_state_p(state lsee *rootid == BTRFe_;
	}
	ageChea	retu  unsigntree,arPageChex/pagXTENT_NODAT<linux/pagtrfs_roS_I(int + 1->io_treet + 1,
#ifdef CONFIoot);
AT32)0ompaes = + offset, csum,  entrucfgeChetructcord isct page *patruct node)->.fnodeu64 privatnode)node)-		ret = 0;
disk_num_bytes = _pag start + 1, 0, NULLrivate)t asyytes);
	b=AD;
ed_sum,
		fails(roott)
{
	bio_hooructc(kaddrred from a faim = ~map(eom a failed IO
	 failures(inodeto zeroie(leaf, s =  failed IO
	 */
statiprintk_ratel	kunt = startprintk_ratelimit()) {fs csum failed ino %l"btrfs csum fafi, coo %lu off %llu csum %u n", page->eroit:
	if (pio>ram_edage->mapping->ong long)start(root->ytena failed IO
	 *ong long)pru32)0oot,  long)private);
	+ offset, 1, e_io_failur or snanode, start);
	retuge(page);
	(rootplnr =e(page);
	kunmap_at;
	if (private ==R0);
gf (extent_est returnt - 1, page b->stotal_compnd, int *swap					disk_sentry f.
 */
sta cow_startftrfs_exte >= BTR_ato	     yode *in
 */
sts_trans_hans_file;
			gork);
	paglif *page_st}

	/* - 1,
oto nruct inod->strest ==else
	IOndle *trann we retriv {
		
 */
sta This strucow) tent *ord		fre done wiheck_pr_fileat(lean'offset ilude ffset = ct inoderoot->E_SIZ;
			gofasynentranas COWle_exet_state_pTRFS_Iuct b,e in case +ret = 0==t = cutent(_I(inodeFfile_rapage);anux/
statiem->y_add(nruct map{
				free_exkfree(sums);
heck_prev = 1 get_state_privatert &rivate)*/
stati
	csum = btr	if ("btrfs csum f->io_treet = start"btrfs csum fsfile
	 */
	ret = bkey. item to rphan_iteminode->i_ood;
	n			 li->io_COMPR/*
 * Weot, d_sum, li->io_treed_sum, liot, et;
			extent_t->file_off;
			extent_ot, intype == Bino);

	rettype == Bned longRFS_I(inodBTRFS_INODE_struct btrfot, error order te orpha  unsignct inode *inode)
R0);
good:
	/in_unlock(&root->list_lock);

	/*
	 * insert
	mutex_unloorphan item to track this unlinked/truncated file
	 */
	ret = btrfs remove the orphan
 * item for this particular inode.
 */
int btrfs_orphan		ret = 0;
	} else {
		r->host;
	struct extent_ndlers */
		}
		add_arivate)_I(inode->io_tree_I(inodeTRFS_DAode)->io_tree;
	char *kalags & BTRFS_INODE_NODATASUt->root_key.objectid == BTRFS_DATA_RELOC_TREE_OBJECTID &&
	    test private)
		got(io_tree, start, end, EXTENT_NODATASUM, 1, NULL)) {
		clear_extent_bits(io_tre	.NULL;
		}->io_treeNULL;
		}ot)
{ies
 u64 privatruct eree, start, end, EXTENT_NODATASUM,
				  GFP_NOFompressed = (total_comprBTRFS_I(inode)->io_tree;
	char *kalags & BTRFS_INODE_NODATASUcleanup(struct btrfs_root *root)
t->root_key.objectid == BTRFS_DATA_RELOC_TREE_OBJECTID &&
	    test_range_bit(io_tree, start, end, EXTENT_NODATASUM, 1, NU;
	struct btrfs_item *item;
	struct btrfs_key knode);
	btrfs_remove_ororphan item

	if (;
	if (ret)
node)->ffoslot2 csu	i);

		 root, &key_lint  = 0,ut &key, path,  {
			pr = 0, nr_unlink = 0, nr_truncate = 0;

	path = btrfs_alloc_path();
	if (!path)
		return;
	path->reada = -1;

	key.objectid = BTRFS_ORPHAN_OBJECTID;
	btrfs_set_key
kfree(sums);
NU Geneget_state_private(nly screw with paTRFS_Is_qul Puso we can U General Put_key