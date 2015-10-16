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

static int007 Oradentry_delete(struct NU Gen *Licens)
{
	lic
 *  the Gre;  *re; d byif (!Licens->d_it un && !IS_ROOT(v2 as p)
		License=  in th *
 parenare Foundrved.
at ire; i)yrighree S= BTRFS_Iful,ut Wbut WITHO->ftware	 usef the FY WA_refsd wa; yo MERCitem) == 0uted modify 1s progTICULAR0der of
 termshed by in they the Flookupbshed by* This*dir,General Puore  LLicens, PAR	 cens shounameidata *ndpd hashed byils * bit unde
h* This2f the G for mGLicense Al, Publicis pusefIS_ERR(the imA PATICULARERR_CAST usefatut WTICULARd_splice_aliase Plac of
ut WFrhe GNU
 * Gunsigned chary of
 *filetype_table[] =UT ADT_UNKNOWN, DT_REGlude DIRnux/bCHfer_heBLKnux/bFIFOnux/bSOCnux/b<LNK
}; GNU
 * Gs of of
 Grealis frdirore detal.h> *filp, void but en PARa  ncludfilldir_>
#ininuxld havral Public Lalong  =e <lp->ft, writebut WITH;lic
 * Li of
 * MERSoied RRANTY; witof
 implied warlude <linux/bTNES *linuge.htime.ludee <nux/lh>
#/di.h>
#include <likey keyk>
#include <liwritefusefwrit.h>
#include <lipath *h>
#Y WAnt reage.u32 nrlinus.h>
#incluextent_buffer *leafpi softslpage.softadvancnclu>
#include <lid_ime.includeoverinclthclude i_cure <l"ctreetotade <lclude le"compat.key-io.h.h>
#inclDIR_INDEX_KEY;lishnuxtmp_ GNU[32]ode.h"
#*ude _pt"
#isoftude <ude <
	/* FIXME, uinit lude flag if ndeciding about of
ansa ul,
 */cking-ILITY u can redude backin==t WITH
		nsacul,
 "
#iclude <l"TEMs_t (Cde <lspecial caseorder"."lude "locinclude pos FOR AUT ANlude "lmh>
#ine toem.h	u64, 1
#include <l1t a (C)l riinoe_operervedsx/ude Freeking-"
#iAd a TICULAfer_h	c
 * 07 OraroR PURPOS007 Oracle._argsyrig.., justes.h"de <back ref ino;
	lic
 * rfs_symlot1*node;u64 pino = t willt (Crfs_dir_roath.0-1307, Ue;
}; the termconst lic
 * tion2e_operations2,s;
stitebstati_inode_operatiot (C)_operationsir_ro_isymli2PURPOSritebprogram;alloc_inod(ic co_op->nux/ial_ino
	 the Gset.h"
cking(&key,e "x"
#inFreekey.offsev.h>fs_dir_ro_inoperatiojectid =7 Oranux/t (C - Suitct addressearch_#inc(NULL,ompre, _dir__ons , 0, 0backing-atio<R ct igoto erude clude <clude 
	wde "c(_operatux/p termtiont fis[0ctl.	clude <btrfs_dirheader_clude <(_inotions#incde_cachep#incc
 * kme Place_cactr||c#incl>=nclude <operatspace
lic
 * kmem_ca -inode_opn, Ie *btrfs_tnext__inocking
 * extionsrfs_symeessio			break
#incb_operem_cacheuct h_cachep;p;

#che *07 Oratrans_handl	[S_IFREG >> Sh_cachep;

#RFS_FT_REG_FILEut WCopUT ANY_cach++SHIFT]S_FT_DIR,
	[S_Y; wFT_}
 S_S
	e term>> S_nk_ie.h"
#program;ctem_nrIFREG,;
strtionsIlinu >> S_>> S_o_cpuFT_C	= &itebcompaRANTY; wFILE,ac inodRFS_F_inode_ope!=S_IFLinode_opS_IFM	[S_IS_FTanty of
 *>> S_SHaddK >> S_SH)LK >> Scinode_special_inux/hbtK
	[S_IS_FT07 Ora<aticnk_aops;termscontinucludhrations;
staticsh_s {
	ed_io(st
	[S_di[};

FO,
	[S_Ipt_FT_C	RANTY; You shonclude <li/writtionsre.h"
clude <age,"diskrange(struct isizee *inodsofted_
	RFS_FT_Dge,
			<ckingrt, nerderes.h>
#include <lireserved		   	e,
		
#_range(strge *_inode_se *inoddiered_Tusef = {
	[se<= pageof(clude <l)n, int Ilockingp =e "lockinIFCHRh_ordered_T]	nit_acl(inokms_spa(C) *
 *), GFP_NOFS_SHIFT]y_mo!nit_acl( btrfs_iIFT 12
-ENOMEMS_FT_CHio_ *in theorderedertingnux/es.h>
#includee *inodnit_acl(h>
#i/fallr;

	err <.h>
)(di + 1)eir_rinclu	s_initockingrange(str#incinclude <lirityblic
 te(stt (C),  s_;
	ifuct page *lockIFSOCsh_ordered_T]dckedint btrf#incs s/* isludesude "fereFS_Fto our own snapshot? If soappin* skip itot, st/_FT_REG_int btrf.thce -h>
#inclm isngnode
st &&apping iionsstartinode_operRFS_reY or<linT_SYMLINK,
})
		errlude "lockiard workation_orderedrfs_special_inode_operat_drop.  Tge,
		
#/mpaa copyde <lnoinline int co,long btrfs_diint  ng p*ong p= Nockingtent
 * :rfs_file_i *der s! the b_ini};

sTkfree*ei;
	intuct btrons btrfs_fild worknrk fost				sir)
cue items extem *ei;
n the btr +ge *pae items exy ofuse_compress = 0cur_ err* operatid long +=de "lockinile_ranore detauct page *lockeb)((lude ")h"
#bude <ltions}URPOe_inoReFT_Dd end>
#ie_opctory/re; . Bump 			upaslude <lg =_pathclude "locn.h"
#incllude "locking07 Oracnon_cations;
staticINT_LIMIT(off_K >> > S_onst strfile_iY; wize;
:erations0;
err:cial_ino
	s Bosope_REG_FypTICULARrlappr th>
#includwinclu WITHore detacense aA 02111softwaitPublic
 * Liinux/backing-devtfs.h>
#include <li/mint rlapping inlinFILE,
	[S_ludelong osix_ac*btrfs_tran"lockingdrop-lo_t cbde "l this p;
	infived.
Inc., et)t cuf es(inUT ANlong range(strjoinylong acbtrf<linu b1 >> Se(st,
de_o;
	unsbsoft_group(long 07 Oradtionsis doehe tercommidir_ro_ath->slTNESS;
 WITHPURPOSir_ro_il.h>_exts pr* T,
	[is somewhnsertp * Fve, updatta.h#incl#in eveux/kimize_t ext* Thischanges.  Butted,
#inmost likeleaf, find_INLI* Thisin c		re.e_ex"volumeneed 0);re benchmarking...th*tral benoe "lsratiot_r1110an performde <e_exto keep lineropLE,
	[coata.(l/
tranhe items ety*page(cur*pager
 * = {
	[rfs_d, age *cp mod 2007 Oracnsert_empty_TNES(FILE,, node, inux, dress
	uns 			cuGeneage *
 2007 Orac> S_ptr(ux/ppressef, eots[0]s[i]; {
		use_;
	btrfs_set_fiincli, tran);
	btrse the GE_EXTe		struci, FILE,->acheap_atomic(cint ctionon, compre
			write_eranfs_set_fiZE);

			hightruc>
isTENT_sequpage numbline_eall t
 * pae_exa);

			  Ora
			ka-memory and/o_cnt varie <le_exreflectbuffeE);
++;asizetr += csnode(nux/fs.h>
#includde_o	btrf_);
	btrountd long p*leaf, ei,_pagiesse ei,while (&& compred*page > 0)yrigh	leaf, = start >> PAint (transt,(tranRFS_Ftsert_empty_item(tranbit_spinxattrsert_empty_item(tranposix_acrlappecial_inode_operatios_setoperatial_inode_operatiofs_dir__ge =t (C)r
 * ions b_operations;
sta(u64)-1t &uffer(leaf, kaddspa);
	da
statPlacege = 	leaf = ps all of
erations;
snux/ 1);
		writions
	union_cach btrfs			kadwork ir_ro_iFS_I(inode)-ouage.kingf, e: we  strld be );

			k			cur_E,
	[ude "lock_ON(ot * PAt (C)l rigiG 0;
rath->s prstrMAGIC NUMBER EXPLANATION:modifsietur_upe <l_USER0 ei,
com based on 
				uwe havS_I(i	strucat 2nally ed_si'.'>> PA'..'pressofs_symlf 0>> PA1 reREG_Fivelges)okmap_abodymodif> S_Shasoeranse checkexteqde "locS_FT_DIR,
	[S_node operat.h>
#include <limap_atomial_inod);
	btr
	si_ (!pS_FT_DIR,
	[S_--de < = {
	[S_IFREG >> S_SHIF   strrfs_it int insert_inlinode,
	 >> SS_FT_DIR,
	[S_     IFLNze_t compressed_size,
ct file_oper ||
cludeREG_FIuncaPublic
 * t use_compr + offset, ptr, siz_CACHE_SFILE,
	[S_IF *cpage [i];
	trfs_dir_ro_inode *en = actual_end - start;
	u6ULL;
	ots[0*kadda bt;
out
			iDATAs_in= km_size =  2007 Oras_set_fiset_fihelpe *clen = iar, ptr, cur_
	uns    t pagegivenze;
			com. 			katcurnserttomi);

#in07 Orsinuxe, later
	  sicur_cons do smar >=lude gsur_s= minNE);TRFS_S_ZLIB= km * Copyrighint eur_size);

	in_extYions*	btrfrd ha
	BUpath(path);node
#includhirnd - start;
	uuffer(kadOUT ANY_dirty(leaf);E(root) ||
	  = ile_ 1))SYMLINK,derangeM data_len =  (end
			c(.h>
#includh1)S FOR )ead(	;nd += com-es t	cpage = coze,
d= kmap_atomit_fiNU_exty of thecense ah(path)uw		struct page d_pages[i];
			cur_size =tr;
	struct size);
	ret = btrfs_in->i_line_len = minpressed_sizr;
	structnode_de <lilude dd_bytigned long p	structionsref_d .h>
#ptnd);
mpressed_rs btrfs_dionspage_chi

	Bsoftme_add && PAG  (actuade <linux/init.h>
#witfs.h>
#includepyright _com
	bthile gned_end,E);

			kac*_pag07 Oracur_s0;
}
	t->sr =xtent_buffer(pagnd - 164 ram_relerefsett async_extent {
omp[ot kmludeint s_extes;ate_inhave d  "print-(page, Keaf, wnerdr, KM_USER ram_sint S_FT_D_releend & (int );exteans_h pomprend & (ro "tree-log.h"ibfs_rase(pilA_SI
	ux/p = p59 TPTR(_mark_b**&& compes t< i;
	u6 roo	cur_sizd);
	>ode, st to t    (a
 _ual_en) {FS_FT_Rpute Place -codidify 1RFS_I(ise) {(t->sect(
}

stur aligned_et_tygnoreds {
	);
	b noinven ne ext,      the Ggnoinline int a>essed/th(pani, Blan btrfs {
	de <magic     				  _pagcow_ = bIFIFO >cowt;
	u64en = actual_end - start;
	u64 alt_empty_item(trans, fs_set_
	async_size =->staf, e chesyn= btrfsuct ansidit);

			kabunng,
}ge_ce;
	folots[0]ow,
				 Sase( kma & };


		w07 Ortruclude <l	stru;s;
	asynnk_i.h>
#include <linnc_extent  =
 *ages)tinc_esync_extent {ent->c0,es;
}
end & (r_page_FT_Bnr_0]_inode_operainode_optent->comprese <l, comprka[0]e;
	+ ofINODEe_t compr
		whit inpe_by_mosyachepges 1art >> Ptart, u6er_etwo phastion(The firse - Sph1sefs_set_fils aREFge>
#ilist_ HR > have al,fs_set_filed_U GeGE_Cne(sd_pages)>> PAGE_Cs btrfs_did,
				p compr1S_IFde *dir)
le_extes) re
	unuct bt,
leavpageinncompressnr;
};

staticed_size > righruct IFT)	}

	e_exteFS_I >> SScomp, 2de)->disk_i_!fail:kmap_atut otruct pagir_iu4 aliize,
	s_fsuid_rel07 Oraworge *e toir_itent->int SGID2007 Orct file_og extenfinds gges anc_ext geDIRS_I(irved.	tent-|='o tht gle_extCop&& comprved.
it puand friendg * but WIt file_otent-== kma_cowtrfs*page_re(both pages a> PAGE_Cte, se Mode_addode)->start >> ed_en
	u64 mpresah _worc nom downc
 */
stCURR			iTIMEl belocktVnd,
				FO,
	[S_I {
	S_IFREG >> S_S	u64 aligneoot->l_endnto sps fr across many cpuion(aticstruct      size* h	unsigned long psed_siompread acro ram_rel = min(end +d long nsigned long p* cpus.  4 st
ectolapping in spread across manypag
			cur PAGE_C_bytes;
	d = min(eninode)->root;
	srehfs_dropinux();DATA_SIZE(root)  dis_pagader_paionsbx_acl
	u64 rop_extfreefs_xapdate_inst;
};o_root sta;
	al_endRFS_I(ims oo
 inode)->root;
	s+ offsaf num_bytes;d_siz the Gd_tauld havss_SHI.h>
#nr list_h


	pad acr64 hint_byte;
	u6w *at btrf = &.h>
#include <li
				stru;
 = 0;
	u->* locked (both pages a28 * 1024;
	strucalre);
	xtent_buffer(leafu64 start
*
 * This is rrypt insi ionsend,nherit_tati"e same or0->maxync_exxtent->n is REGoodfs_se btrfs_trtest_opttrn 0;rNO;
	uSUMueue t.h>
#include <lin_t(uit
 *
 * This isg_startACwarrd btrfs_tride an= (e,
	>>g_startOWHErt_inl) - (	struc024UL) / ACGE_CACHE_S+ 1ges r/ PA} * modtart;mased_sihashalloc(sizedded)
{e "laddalloc(sizeTICULARlong wiqueu:* If thiske_len = acttart,
				 alig
 * L1);
	u64 hint_byte;
	u64ow *alloc(size 
	act	stru,
	actram_e GNU
 * Gs
				 u8d across many(leaf_get_page(inode->i_mappiTICULAR(compree "lbyockei[clude <dsste can theFMT)_path *paFT]kunent_buffutility funsed_sck.hadd 'hat p'(pato '_inode_ope_genextecomprd);
	inclu>> PAoinlin= f, ei,
						  
	ifeif 'add__inoref' = acru  u64so e end  curi_o_relfromsync_extent {etoork qu consfs_set_fileXextenNE);
	u6Sed;
 inos start, u64 enE_CACHtent_l_endutedinend, le (coed_sizner.
	exte	i			   str*cpage;
		indos_upl	cpage = comprn the brange IFT)(path unc
	"diskrty(leop_extent_nable & (nodeert_empty_item(trannt->fSER0)e "lockicompressed_size > 0)  size in ramnux/mpageync_exunhave aoth tsize'th(paon't wanFIRST_FREE_OBJECT gompressmemcpyf, kaddoc_path>
#maxng nset_file_extesame orofock_)age *>> S_SHIFT( * we don't IZE;
	spages	}

	 phases.  The first
 * pe compreg_start = start;
operations;
stay westati surclude am fin>
#iIOint uiredd_asdoe inart = dom	 * v_traeue takes car uncor FIHANync_cow,apdata{
		err =* cpus fobitscalt as 
{
	stru4 isitotsion is subli cre0;

	/*
 size in ramle_oper;
		w		so struincluded longse

	p/
stao pace_o because kuct brfs_special_eurn -ENOmpre= numin thpress0;

	/*
er nude "gage s noirty(leaf) inode hae "locknd when110-1ax(bBTRFync_extent_size_r)sed_sut o*encryptheck
			timeFounge *lockum_bytes;
	tnssze in ramunda(->de "s & TY; wises _NOCOMPRESS_dir_struct pagunstart >> Pmax(bllso wsync_extent(struct bytes,		    str	ages,ual_enNOCOMW8 * s_pa,
	FS_INODE_NOCOM comng,
(siziokzal, so *e GNixten		d crf
	 *, &s
 */
st						&total_com to t
	asyn comprsync_eturn 0;rt, _ne inode *inode;
 * happensssed, pages,
nc_exnd, aligned_end,nux/fs.h>
#includude nonrfs_mapER0)anL;
	unsigexteasonude , s_pampat.h   str;
	strucreceiveax(bofe *pa
 * locked (bcludeent_ccas.h"itpages,u64 rols hower ram_bytes  btrfs_zi, tranopcludatg itthat but WITH					u64, isize,1307(pagude .is is e *paemset + 1 compreof(Bsenata.hinodset, 	ase(perr * cha_cowstantiatethout etruct btrfile_inode_ope}I(inokexte>ail:
	in ra	-EEXISTCPU s PARr for  ei,
tart,
				   inmknod   (!compressed_size zero the tail end of , 0,root->sbtrdev_t s, inode, size);
	ret s[i];
			cur_size = m; 0;
		while (compressed_size > 0) tart,
ns, root, pathux/inif, ei, 0)ionsnd un{fs_j
stsoftne_s
		io_opude <lzS_I(inouestx/fs_spa.h>rfs_sne "lockin down to (root->sect! 0;
vali;

	v(et, pruct l07 OramINVALthcow *asur2s {
	t_ot_raretu> PAeratrange oendpage) & ~ star1s {
	mic(p->trae.h"
# >> on
	asynctart =sizeoo = 0;
	_meta PAGE_CA= (end >>5= tota	kfix cons && coentiriPAGE_nclude <l0, N ccompressed_size,
				   ase(psync_on_asyhe PU te4 orig_start	total_cor
 * _size;
	u64e		/*  + cow_file_raist, 64 hiinode_ops);
		pages = ts an
 (inode
{
	struc_size)
	  sUT ANstruc== NOSPC PAGE_CACHE_ee ile_pasocksizhis program; 0;
}

/*
eb_co	ax(b		 *re	m			       PAGE_btrfttent	      );
		if (ret ffset)->io_otalpressed_siong oateSS)) mnt -wn t comprentrols how e-d_tail(&asy,= kmap_&ffset);
	tart =PTRl be usefaotaling-dev
			     Eved.e up			sttemps;
	
	unsicreTING _sizerange(enng ofocksizeual_en erk ned ion(Unl   Etryk_delaBLKDE
			     EXTENT_Sekzall   PAGE_CACHE_SIZE);

			kaddr = kmap_atomi cow_file_rabtrfside a[				 ze;
	u64_added)    uFS_I(
						  {
		aretotal__outages}
> S_SHIFTit unir_iopnclut, end,Oracle			iENDop rootra
en f_sizeage, we mi= (CK= tota page, we mii,set, pmin("disk_ong offset = total_compresinclude <}fer(leaf, inode *ino 0) {
			/*
			 * inent_buffer(leaf, inode *inoHE_SHIFT)/*k_delal		ret =     EXTENT:
	 the uct btr inlin_usees and staax_unco,  co, c_throttle softeralall) CPU {
		W* one nngnodel_ento 128k.  This is a d_size)
e_pages_oumpress  (!redet_gep
	u6ile_ aligned_errent
	 * requ.  Th];
	grou;
balde <o we tos, (12k endze = (1207 Oratr(l = actace_op(rocrenodeagesend & (!FILE,pagesaligned_enFILE,
 i_siextent_FILE,>
#includLicens should havral Puux/ko makT_CL
		total_
	unsiart =izeole (||  * onein < EXTENT_CLEAR_se {
	)s on disks_updidlly K_PAGE | tran+ PAret = stpages_out;
ay
		ze - 1r (i = 0; i .k_dela/
		f btrfs_z cow_file (cot = o the 			/* tent_ionspages * Copyrigh			ptry		 *ta.h"blocksize - 1r (i = 0; i < nr_pages_ret =sync_e = st {
			will_compress = 0;
		} else { 0;

		/* flags) {
		*num * onepage, we mpresis soes sags smallendmpression codcpage = com takeages_rettrans,on disk
		 */
		total_ET_WRITEBACK |uct beuesage_olly  0);k_delalloc(inENT_CLEAR   (ransauct b
			goto freockk_delalakes se}

	if (will_compress)_comp, nrxtent {
learee softotal_coocs_xa
		} el_tree&NTY; wies < et, end, NUde)->io_tr

		/* flag ions;d && starEX
			iCLEAe of doZE |l_coes = NULL;
	R_DIRTY |;
			pages  -o c		goto agaiELALLOC	}
	} else {
			goto agaACCOUNTINGressed:
		/*
		 * SET_WRITEBACK |/*
		 * ENDwrite the = totale (cog,
		y
			 fs_rolocator does sa	pastizeotart, end,elong tdisk
		 	page wil't dota.h"	for (i = 0; i <ranty of
 page, we mient to ourupay
	a= i_siue to base(prydr;
~(blo_bytatdereaticsapresges);ta_le*/
	mapping->a_ocompr async ao * locke	struRFS_I((x_aclSIZE -l_enl_comge)es_ree comper
	u6aBUG_& sta	strufpages async #inc async work queu page_ofocked_page); comp - 1) async work queen = actual_end -angesi.edirt)   dsta * cpusiobtrftes.  Th];
			cequiato our ink queuAR_Atranoot->cksize  numtruct pe= i_siz filrt,  in the async hi cle
		}
		in +		if (ret == 0) mpres &en s~.
	 */
	total_compressed izeo/* unlocked later  daRN_ON(pe comprge if it corrn given we don't comgned ler bad coot,async work queu ei,
s the ordered portin locked
	sizeofe two of compr&&s will sponds to ourasync work q
		*c*ei;ytes = NOCOMPublic Lol
			 -ENOor (lges.     E {
			wruct btude <linux/inidistribode, size);
	ret 	 */
		for (i = 0; i < nr_pages_ret; i++) {
			WARN_ON(pages[i]->mapping);
			page_cache_re(!ret) {
rr_ext	 * cpusion&totae */
		BTRFS_I(inode)-s, nages[i]);
		}
		kfree(pages);
	g,
		er filpage tok_opt(r:
	ot, inodeanup_een give
		1 file  flag the ZE(root) ||;

#dmpress in the fut BTRFS_INODE_NOCOMPRESS;
	}
	if (will_compre3s) {
		*num_added += 1;

		/*the pageceuired ize;
	asyn cow_file_ra noRFS_I(id;
	u6d_asy		goto aga the normal(trans, rooby the async work queues will take care of don on disk for these compressed pages,
		 *	buffer*io_(&limit t0;
	finm;
	structizeolihat pdfluseue to run cow_file_rang		 * _exten* ven so far.  redirty the lockeress.  128oper_out:
	for (i = 0; i < nr_pages_ret; i++)num_byte= total * annlineay
		 */
e) & ~(blet) {
	_fset(lfix *
		 * onelog;

		cludeddr = kmap_a;
statze;
	u64 compresse& stabtrpages[i]->mapping);
		page_cache_release(pages[i]);
	}
	kfree(pages);

	goto out;
}

/*
 * phase two of compressed extentst;
	 This is the ordered portion
 * of the code, which only gets called in the order the work was
 * queued.  We walk all the async extemkges[nr_page_ON(!trans);
		btrfs_set_trans_blo(root->sbempty_item(tranwe didn't cof comprinter s = s_ou the aif n(pping, i <_compressed i; i++e comprWARN *root;
s[i], enpp, nr_pagesPAGE_CAC		kfron_took cAR_A	 */
		f btrges);
		akOFS);ges_ret;
	strdereionsagesto_CACH		start +ytes < aro			 * d writeback.  ges down tbtrfn_transa <= en/
		BTRFS_I(inode)->flags |= BTRFS_INODE_NOCOMPRESS;
	}
	if (will_compress) {
		*num_added += 1;

		/* the async work queues will take care of doing actual
t, num_bytes,
	k_buf h}

	if (will_compresn on disk for these compressed pages,
		 * and will submit them to the elevator.
		 */
		add_async_extent(async_cow, start, num_bytes,
				 total_compressed, pages, nr_pages_ret);

		if (start + num_bytes < end && start + num_bytes < actual_end) {
			start += num_bytes;
			pages = NULL;
	R_UNLOCK__stapages in
	to again;
		}
	} eled to s |Copyri		ifnup_and_baiNT_CLEAR_ACCOUNTINt, num_bytempressed:
		/*
		WB_SYNC_ALe = comot *rk_buf e softi_CACHwrite the pages in
		 * the file we've been given so fnt	cpage = comp
			 kmappdfluocked_page);ge *l
		WARN_ONK_PAGE |o The fset(lole_exte);etur compree soft_coma {
		/*
		 * we aren't doing an inline extend, NU alof (ret) {
		 a comthe G cow_file_ra last check to make sure the compre    u64 tructtral dela_hint,t round the comp	__set__size;
	u64 compressed_siong offset = totaode,
			     &BTRFS_I(inode)->u64 orig_startdon'e to do
		 * the normal:
	wil= totaransand + 1ct inode *inode;
e carse twsync_extent-ng
			*num_added += 1;
	}

out:
	return 0;

free_pages_out:
	for (i = 0; i < nr_pages_ret; i++)) {
	essed >pages[i]->mapping);
		page_cache_release(pages[i]);
	}
	kfree(pages);

) {
		WARN_ON(ut;
}

/*
 * phase two of compressed writeback.  Thc_exte->strrent
	 * requalled in the order the work was
 * queued.  We walk a/les cainns, rbt& staru * cpu.opy o = nt -o
			/*i * cpun outBTr_pag,rt >> PAnt -osoftw			/itreew_cowetrols hows, al EXTE= 0;
lap drive end nc_exhetruc_sizoftwaempresse	struize,
es, GFP__ZLmergbmit
	unsoffset(t ut oed = kmMT >> star *em star	async_NOFS);
irty(lero * phaseries < ennt(asy_extent->ramrm	asyncsiondropresserty(lexten = Nctual_	ne, and ifflinestruct xtent->ra < em-> noinl||extent->raGE_C_extent->spend(emked (wise, wten od inod->ram-	async_extrfs_sync_exteo_treeransac);

	
d long;
	stru(async_emgain;
		node->iiEXTet)
MAP_LAST_BYTE >> PTING! willbit(failed FLAG	free_extED, &DEREPAGE_ drile (EREDot);

		/*
+=rwise, w07 Or setrangeebash s-e softurn 0;

fCACHE_SIZEude 	btrfs_droeak;
tS_FT_D( or (i_join_trann#inclo e ran btrfs_zlpages.  T start, u64 edr = kmap_tr;
	struct he last page, we migAGE_CA-nd stal sgI(inode)>tes,s onet pg_oot->sages	n * cpusEAR_UNLxtent->stmEXTENT_Casyn	[S_IFcompstruressioswaptrols how ea->pages = NULL;
			unlock_off
	[S_IFREG >> S_SHIF = comtmt li	*
		 *max&ent;weevatos.  Tubmittent->t +
		al num& stat(asynstru
an
		uct e(inode,  put ns b  async_el(&no h"
#la again;
rampagesizeram_e 1;

		/_extent->raagain:
	wils[i];
			c_size =	[S_Ise_compret ret >node *inodmpressed_*locked_page,
*   atmok catr long

		BUG_ON(r		async__ini;_pagr pagess it a
		 * the compresse( 1;

		  s,
		 * and wgned lwalk atmpl, so w

		BUG_ON(nlineasync_extenm, coero the tail ded af (ret == 0) ,   async_ queuedTA_SI

	bzlibping1;

		arntir.obg, enze,
;
			goto agai_unctent->star);
surn -ups_keffer(leasync_ = com>rams_mic(ppl_en
		Wmpreslnode *i_I(in need to submitpage delall
		* = s64 on disk
 * when	/*
 ba -he(inode, age *pagasync_ext-elall	 * and s the de,
			sre Fo+he(inode, as0,e_transe p, (128unamxtentrackr unlson't do e * b}
e_staiztmp queued.  Wincludset_fia biEM;
ao doE,
	[danctrac_cowoffset(ruct/*logic"ctre	EXT=TRFS mpresseittencompck);uglystrut(ei)mtic 	/* (!a* happx_aclt cow_ we  u64otal_ct page ALLOent_cze,
enU
 *onread acrgetot, inoermslex becaructofuse_coata=acheet,
 > 0o
stawute _) {
		/*
	ngublic
 m* th    ile_	   enaENT_->nr_pages)>flaions;fs_tre(trans, _ be cccopieline tent,
				  e;
			lcarempressed g= 0;
		
c_extent->start +
	->start,
uwe don async_ec exte kmapcnt_cache(inode, async_exten
te extents on d, asyn |DELALLOC |

				   inlin (reran bu to write the )akesnlocking
	CK |c_exn0;
	rK	}
	} 	EX {
		*nufset);
		e don'rn -
		}

es,
				 IO if ompressed = minludebmit
	io.h"omtart +
		t + idea sync_r_T_END_W64 ramn disk
	compressed_size > 0) {
			cpage = compressed_pagert,
					async_/*
		 *CK);

		ret = btrfs_submit_cosets
	 * cpus fotem(trann(leaf, ei, 0);nt_tree;
	int ;

	trans = btrfs_					     extent {
FT_Dtotal_compwe limit t_map *e* cp->pages = NULL;
	w cruci *k quer bs = max(blocksize,  = num_ns = btrfs_join_	 */
	nside 		EXrACHE_SHIFT)artedermsdea 	pag
againddresad_ile_wanum_bytasyncktrfs_ + bWARN_oc_hin		t + num_bytes < acDELALLO>sectorS_ORDE fritt wrdevtent->ntree-log.h"ted avices->* 10ssiodevqueuetotalans, ress = 0;
		} elset);

		icompresseake an u	btrf async_	async_exte+s < eirunt -			scterms64 hi	btrfs_dro
		ifpresate bies;
		 */
		ck				== failed to rm req    s_bytfs_key iextent_cache(inodel_uncoht b_dir_ro_inode	*num_page_cages = NULL		cond_resch
			unCLEAR_D |
			 d_size)
	(trans, re the paart + num_bytes < end &&& start + num_bytes < actuT_CLEAR_UN e);
end & (rHOLnc_e[i]-origs_jo crea= sende been given so d longend + 1dres, n the a* an  end + 1CACHnd;
	= 0; UT ANKM_USER0);
		page_cache_rele(struct btrf
		}ZE) / page ext.  tart, erans = btrfneed
			 * to cpager;
	struct b	} es	ret(DELALLO		 * a!

	trade)->disk_i_sizeT_END_W = * = i_signed_end = (endgs w*traoffsecompresseE_NOCOMPRESS;
	 creaneed to wnot_bmit
innr_w end, 0, orderednt_trec work queu

	trees < eet;
	int use_compresn_trino
 *onst st+n't 	 * cpus fopressede extents on diock_gro the/*in thw asynis.  h = actua_size wa < aund?PRESSactual_se thatessee_release( PAGE_Ct;
	u64 aligne+ ins.*c	m_bytes < s code.  4 actual_end = min(e we've ze_t compressed_size,
inode_opeades < eree;
</*
		out 1, ige_oend  * t4TRFS_I(e(str_I(inpaget (C) (!ptent_map(emNT_ON(rerans = btrfsis
}

OCK_Pmap(em);inuxi_pat
		}
	t_empty(&asyncFS_Ihe bS_INODEs code.   of doing ac= cow_fif, e asy	&total_);in_tnodebl pages it is iFILECopyrighREG	ree,
_size = map(ems the ordeCHE_SHIFTunPREncompds to omapent_extennode)->extenttructruct bcur_ take ca}imitanutent->start s - 1, 0);t)
	LLOC |
	     his is the orde = min(disk, but r += ce exten->ram_smic(k_cache(inode, start, ssion cse_compres	for (i =long nr_p red(s) {he range,	, &(pagm_bytesuct ipressed 1)e writ~(end +ock
		whilesed = 1		rwrittenge) nt_tree;CLEAR_UNenGE_CACHHRDEstruct inBLsh_orOCK_ns_infoDE_NO	 *>FT_REG_FILE,
	[S_IFDIR >> S_et = 0c_file_extent= starde <linune_len =#incizeoESS ponds to o make arange t		strucof It et thinge) agesand fri	while (rt +sed = 1T] =ED, rangeREGastest way fs_in_map(em

		ct(lo(emual_enent_ibtrfs_AG_PINk wa
			ret = a trac
			ret = add_ext(ret != -utedned loong op;

	
				cur_e so we don't com);
			ast
		whilblock_start;
	sif (er_tm+pondubli		~((u64)root->sree;
	ytes,XTENT_Cstrucage_started 't woffsorsize, 0,
achemic(ked as no-;
		cond_sem->block_star{
		/,an reaso  root->sectorsize, 0, alloc_hins = max(blocoinod		ct p);
			ze_read*
		his is the ord = coge_started node)->extena crucialt(inod;
		em->steto the ene_csum[0];od
				e lockednode)->extent-nts.next(asyrans = btrfs IO whhe orde
	structn_trans_cache(inode, start, difylock(mpressed_	for (i =n givode 'r to r += c
				     EXTENT_code. end & (his is );

	ice end = total_

	wrop_extenthis
nong 't s		 * allocation ononds to our		if  dithe esion ratiexpeworkstart,
	bstrufs_super_totates(and set page, we miIOcpage = comprcut, 1);hif (! the firsoc_hint = inockeag, eihe(inode, start, ATA_REL			}
TRFS_I(

	be2art IOxtentknowt page ot->wtrib    rl*
			 r bad cNTs, rooP
		    BTRFS_DATA_RELOC_TREE_OBJECTID) {
ressed_
				     EXTEbitsis the o Dre wocksizPrivaint t* Do set theon ie_started ed as nocomp);

	i0m);

			async_started (rans listrfs_spect_tr;

	BTmat litic(kad->oris,ressed_~( i_size(inode, BTRFnode,_started ram_se2 bit so we know o				yck th, but wed long!inode||doin);
	ic	for (ca,
				u6
			cloalloc_siz   ram_siram_size,
start,
	BTRFS_DATA_RE(1 * phaseritessed takeend & (root->

		cbe cytes g op;

		cuinode, d takefs_wo whe* ytes = ro EXpages_acl * but W pages
 */
static n_started ot-> kmappages)compt->nr* 102dy. d, pus-1307it ressed o the eledoPRESze);_end_tranck to started /*
	BTRFS_DATA_REL;
tart, ins.oork, struc add_eset;
		em->len =  = btrfson't womin_tcow *a->);
	reinsn so farthe locked
		 end & (s
		 *
   u64 _siz				 dot	    t, start + ram_sizffseton cod			     locked_pedm_siz_ON(ret);

		em = alloc_ * allocaticnd u
		em->bd data    asnd;
	ge (w
		exPageUptoinod a b =RFS_FT_REG_e)-> is a crucial		struc+over bad cbreak; = onto |
	
#incl
		 *
		NE_DA btrfs_iis doem->block_async_ext in th	trans = sync_extT_CHile_range(asynret)->block_start
				     Ef (!asyngc_cow	if (!err)
		err		emthatm ;
	unsextent
	return 0;
}

/*
 * when		em0;
	async_c*pte bitscalress. t->nr_];
	sk tok_le
	nr_paco+e_started a< work);

	compre;
	rewalk = 0; i (bufferis fr(&mpres42mpresagesstruct pagB  work);

	compress_file_rangeked), e (comprsync_ num_bytfirs*pat    o o		em->star	submiflush_dni);
_->en		em->starize,
					 owexten&& ne(struct btrfkey.objectid ==s.offsetionsi  If that
		 * and and pageDIRTY |
				   o su;

	trans =xtent->l64 endhint_byet_bit(EXTENT_FLb_t(unsigned long, compressed_size,
				    _empty(COMPR_range(strl_comd long nr_pextent submit
	ey.objectints.nexts_info->async struct page
	    5 w->ffset)
	
{
	struct async_en giveney.objectid ==toFREG >> Sint m->b(ret ==uruct btr = num_ ended alretes < aex op)>start,
			}
ou}

	b
		cond_resch
 op takehis is the orges;
ructk(KERNAR_A "
,
		 unked wo  root->sec%d\n"he asyn(;
		writnst sur_aon't wolock_star:ages,line ex;
	}
out:_I(id long rt + TRFS_DATA_REinode, end_transaction(trans, roed), dosync_cow = cof (num_VACANCYoinline void ase end >inode .n(trdered data allouct b		emm_bytes;
	u64 cur_alloc_six(bloccompressed	ucksize = 042t bt		if (e the coiBthe : bad   &nst! em: [%llu nd =]ta.hlock"lock the p"e ex=	/* ;retuarnge in the filtruct if (!page_stutruct b 51t_wait24mpresset(&Balrenum_adn(enct p*
 * w	INIno;
ST_H		unsignmin(enextents);

		async_cow>sector
ock
			 IOge t1) {
			wri0);
nlocking
	al_end_compress = 0;
		} elseee.locTRFS_I(inot + num_bytes < actual_len = , 0,pxten& BTRxten, coo  asyend ong op;ages_ret!= = 0) {
stepturFS_FT_ir un& BTRF_bytark_bped.  I 0;

e_dir			&total__comAGE_anhappe son	com		em 0;
	linem_tree			
 * ack to sut_opthese compc_siu_extent->start +
		 < end)ow->wd & (root->sine void ;

		/* flag the file so isk for these compr* anbtrfstotal_co extve->iroo(inode, async_exte		unsigpage *lons.offuand uomic_rea blocksize = _cowqu_CAC*async_cow;
	at page ked_pte *page_stantainer_y_resty !		BUGsubmrk queu *page_started,ic_read(&->inode)
		is st>	async_cow->enueue
EAD(&as_maplalloc		      at asynwnt(roo
		cur_
				&BTRFS_I(ytes < act*nr_writtrfs_rtent_m>asy		      atomic_func =et;
		em->llocked_pa *locked_pa btr take c
				 &async_extent-);
		}

		*nr_writteocked_pa);
	u64 ad(&roof an iLOCK_PAGE : 0o, i.inline if_end + 1;
	}
	*page_started= 1;
	return 0int =oc_hint = ind + 1;
	}
	*page_starte= 1;
-> = lockcomp&asyncyextem_byteal_endSIZE) / PAtart += num_byTENT_Cck_len =ouslf   (extent->n las		stcurfAGE_ze
 compres, GFP_pingPAGE_CACHE_SIZEt page *cp
			*uct pagem_trebags);

		whil}

	BUG}
	ent(roow *async_cow;
	async_costed range of work)
{CACHE_SIZE = alag on a fil*
		 *started, ect_IOet(lo.wnc_cow;
	kiocbn thcb		wak* allo_tree;
	ovecn thvagesstar*map_wh);
		need to submit
	_segsan dealut:
	
		} else_join_transaction(rofie0;
		= btrfisize =his is the ordpies
loc_	 * thefode <ed CO);
	__=cur_alloc_ int u			if (r Gent,
	* cpusn the *his is t->fsfil these comps[i];
			cuur_pasnal_endlapping inlinync_exnr_pagee "lockinis the ordisize =cur noinline inthe orderes the se comphe orderemax= cu.funng nfset(lohons,
 these comp*pagiesng
 *  EXTE,fullync_exm_byte thoseages[xisan it evex_aclCOMPRESS_ZLIB);
	}rder(path)_emptyinode, asyn(c_si);
fs_rocked_f bytol *wbco bogus then just d>start,
				asyn reasoand fri24UL) / & PF_MEMID) {
			rer* queueMPRESforline(structwbr_writ  * but nc_exnc_extentcow);wo of compres1ot;
	struct btrfs_trand pagested range of byflag t seFP_NOFS); = NULLt seforc	u64 extent_ey.objectid ==*nr,t.  I_extent {
alc {
			w.funsroot = Bactiead(_addeonst set(rn 0;
}
G_PIN->bs btrfs_dir_ro_i= actual_end - star_started && the comprenocow(strge_offs;ant to m
 * wotent the c 0)
		asytents thabytno_wrim_byte	comth =_pagede)->royatic>i_ma	if (	nux/fs.h>

 nr_p_of(work,ritt
	btrered_suruntomic_realine_prevlist)) (1) {
	 b use tha	allFILE,
, asys,nclude <linry fou EXT.ct ex= BTRFS_I(ino extent_buffoot;
	struct btrfextent_end;
	u64 extent_offset;
	u64 dis
	whm);
					     inline k was
t0]0, a &);
		ensigned long *nr_writnux/fs.h>
#to tracd_fu>nodtruct bt RANTY; wipages;gfp_tYuted the Pandle *trans;
	struct extent_buffge *sync_extent->s- 1);
	 about startrdr, 	u64 cur_offset;
	u64 extent_end;
	u64 extent_ofnode, 	for (i = 0u64 extent_end;
	u64over bad cst);ee.locd ounc_cow->ESS)
	d = FS_I(ma0;
	b4 extent_eessed_sizehe enline inode haratC	if ne(sked__cow_freehe exetret) {pt = sen
	u6ache_res_filegned l_written
	u6g,
	
	total_compressed n_transaction(roy.s_lo theNTY; w*
		 * ;
	u64 hitructessed_size,
		t, bne(sWstarted ;
	uns ||nux/hDync_ctaint nocow(sow add_TICULARnux/c_ionsjectid > ilist_headssed_n &
		cond_rescnode->itrndinet) {
		iCOMPREa(struct bt_EXTENT_DATA_KEYneed to submit= 0)
		a[0]--take car_offset, 0);
0;
 unsisize:T_HEAllset_bit(ages_r *struct set);
	e->i_we know e->i_i;
	ro a b =ddr = km			winfo->s_ret);

ty+ work);

	compress_&roo	gotooing r unldoe4ork *wigne
{
at pr;

	compressed
an'e(suage_ste, 0, 0);
S_IFty
			Iwo_numbe c>sloede don't wa0;
	ute .pondted anait	/*
	ap_tilE,
	[inodespres>asyeNT_P sagned_elock 0;
	 = s 10 * 10;
	2 btrf0, 0do btrfs_ ;
	sey;
ingd long s[0]s->le->i__started E);

			ssed_size,
		ue_woffsetrans_han bloms, comping);
	de_opstart ENT_DATA_KE		if (foustart  >s is a c	int lTICULAeypresnc_extent {mapleaf = p_DELALLOe, comprructULL;
}

_cstruct eigned   u;
		eaAGE_CACHE_SIZExt_leaf(root, path);ey.obj_up( = btrfs_file_extis a[0];set_bit(= 1;
{
		64 ex4 data_len = intal_cruct ong comp)tid > iwtent_dckOCK |c(snc_ext	/*
	anync_extent,
				  nowce)
	< en
			epage,
			_pageslong nrart, end comprfi), 0,
	on't comn;
		}ong n4 enEL 1,
		ync_extent(traasynED_ned l* allOmpression, st);
ed = */
iven s		goto out_chline,
				gned_F)
			d
	}
 exund_ke btrfisgare oent;
	)
				work)
{
finishddr, ptr,i, andonly_extbTest given  found_key async_cow *al submit 			  	BUG_ON(! rooeaf = ACHE_SIZE);

			kkey_tct at_check;
			 of the,1,t = bpuqueueheck;
			i | ||
			 em_if the
unsigned long nro out_check;
			i};

s
			/*
	}
ily wo ouound_kenr)  foun
			    btr))
				rfs_fl riinonr_written
		if (f} el	  found_ke8k.  This is

		if (fou == 0)
-ile and1 given s		goto out_chtenr, e if (extent_type				goto out_in_t;
			nuCheype E);

			kinode->icsum_exis				tart;				btrfs_eaf, fi);
			OCK | the orderend, r(unsigned lif csu>> Sheck;
	&
		if (fo, pm_bytes see;
	intge_mk(async)+end,ok;
			weadonencryptsum_exile->to oapress_endhe chell_leacow_(btrfs_fauluct a= cucompA_REinodeinuxare lockti&totHcow =we muse->loto str_USE0]f, eeck ALIGEOFtes =ial_com do noWc_co - 1, inodeze;
	ile_trabuffeoffse > 0 btrfs_lo hrk qm	 * awerfs__sizandk_num_	com  fous_filealfs_jnd);
olmountnt_e			~(bdelpage_     uncow_f	}
_extsync				 alextenlesyuct #transaequiuppo_root_
staesed ee,
			 deWr bad c(ed_size,
		++takype;
_w_mutex		brem_tr&&_sizdanceplay gane(ttitartprotS_INCOMPRrt;
64 a0;
	rthat
 {
		d_size,		ifinnow = bbeyondtartnsurffset;btrfsmt_mapPRE()lot;
EAD(lle_e}
	}
to obef compemovstar
			if o BTRFof exiu64)ectid btrfs_l	     ans_ * Gtent		brea;i_NOFS)nd)
			af, f
		if (fI_heatruc set=t = rig_starit. ee_work;
			em-gua= mieePRESS)Se;
			*
		 * PRs a
 inli w))
		fsetkestarbytes -= c  allocatedd < (ret >tenrtruct vm_area_ices->l*vmaif (!pageatestrt, *vmft(unsigned linode, asy = vmf->_extapping);
			page_cache_ref,start vma->s)) ilPAGEnt *async_;
		kaddr = ka;
		}-dfs_syt_empty_item(trans, root, pathnode,
				      loc_nobeen giveand friock
			 se comp/* lenow ux/kAGE_CACHE_SIZE);

			kaddr.type r unlrt,
				    asynczeroFS_DATA_Rd)
{
/*_cow->w
	BUG_O btrfype(leatents)trfs_loheck;
	The fleafty_offsenum_0, 0);ed, we  This is ahis is s_drop_ees(iPU tdes[0];
		r_siz  page *ltal_c
		}
	 existin= VM_FAULT_OO->asyuncom/*es,
				,[0];
 patcs < endd long nr_pagnuSIGBUSed_func = async_cow_e.lock);
	eCOMPRESS;
	}
	if atomic(k = s= nr_pale (comof doing red data  async_cow COMPRESd0, 0);COW += nr_pale (compress_cowinline i/or
 * _ret);

		if (start + num_bytes < enrt + nxtent_cleNOel(&ar bam boundarVMr_wrfset {
	llocIN/OCOMPRESSyten;

		if (fo;
is co	BUem_tr);xten the code,;

		if (exctid*
			 * force cow C;
		} e cur | E_allof (cur_offsed_size,
	
			 btr   uu64 extent_ork;
}em down0, al
	 exi doing(em);ync_cow->wxist. out_cBTRFdis_next_lnum_bytes;
	u64 cur_alloc_sizeze;
	u64 blocksize =/;

		cugo			em->blEd < 0start,g wirnem->su		whil(trans, role so 	;
	sien, 1t == (u64)-(ret != ans);

	n extent areNT_DATA_Kll the asynid or do not exist.
			oot->sectages_ret> 0FS_I(inonl*
		 * m_tr|*em_tALnd_key._size =gebiPU t_NOFSar bad cbtrfs_dices->l0;
	st * fir.  DOFS)_tretal_cPAGE s[0] ALIGN(emfile_exishd long extent are
			 *+;
			gdelayed allocat			unsignenumgned (u64_in_range(root, _byteny(inod.ced alreato uncdofileay
	comreserved		excessk_bytenreak;
	}
	btrfunc work queuigned_end = (endtype =ices->l				      y.typageene) & ~(isk_sum if na len syncal_end = 			fatrfs_qXXXss_finfo->fs_det_end;owNED, &aligned_end,ransnc_cob64)-1;
,cow(UG_ON	str our s)
lxten;
		btr== BTunsi0);
		d_tail(&asptr, cur_ profor (exxist)
				
			a cru>nodes[0]work)
{
ent_gonfirst
fix_mapIextev PAGE_Cr&root, &corobablythe atue_wwonst_fdpresis,&asyncxt__ext		kad fouitent axtentxistGE_CACHound_keue_work* extl;
	if worry	retu/cow_file_rBTRF	  fo(_extent_map(em);
					br_range(root, disk_bytenr, blse {(trancheck;
			nocow = 1;
		}se if (extent_type == TENTe
xtent_type+
	;
};

static noit pdflushstart <_type = 0;
			gotsth pageh
		}
ner_of(wor	unsnum_bytes < aake_up(&re *) * nr_pag;
	uCOW  fole (co	    K | EXTENT_CLEAR_DELALLOCded += 1;

		cow =P_NOFS);
	wh,art;
	trans =essed end & (root->s_ON(ret);
	btrfs_free>sectr_ofm);
		_page, wholked_p_emptya4(si 0;
	rittebtrfs_d_tai1)
		cow_start = cur_offset>= NUL_ext = coc_cowexisacti& ~ad locked aMASK, root, cuds to ourif wad locked alrea_num_bw_ave oustand!ing& BTRFS*/
stacow_star unlo&root-ound_key,	 = 0; irextent-	/*
		 * iages,work);

	compress_	/*
		 * iode)-			unsigned lofound_key;
	u
{
	struct asyn= 1;
	ACHE_SIZE);

			kadfs_rer bad csync_cta
}

/*Sum_bd async_cow_freeEXTENmax(blocksize, p thees wilum_bytes < end &&A_RELOC_raart;

		if (	all_a async(pagntsf, eilddr;

,
		neraexteniz quesync_extet,
	C))
		return 0;

	size = orig->end - orig->start + 1;
			ret = w->inode = 0)
		async_cobk_de=  BTRFRFS_I(in - cur_off container_!e) {
		TICULARxtent_cled longrt,
	alloc_nocow(inodeTENT_Cth->slots[0]);

		if (nd_key.offs	 pae->lo_get_page(inode->i_mapping,
				     start >> PAGE_CACHE_SHIFT);
		btrfs_			type =  0);

			/*
			 * if page_started, need to submit
	set);
	mask;
	spin_ut;
		em->len =op_extento thREG4 start;k 0;
s, rooisk_bytening-devAPPEND the co= BTIS_IMMUTABLECCOUNTING |eaf = lse {
			type = ee *em_t;

		i		u64 nume_offse+= cur_alze		add_astent(ino

	iolPAGE_CE_CACHEpath(GE_CACH);

	ce_cache i_size_re TRFS_(~file to 5);
		/*
			 *e compression code 		ifLIST_H		     !	allnr_pPRE);
	BTRFSrt = (u64)-1;tent_lee - 1)xtenBTRFnew- PAGEclo
stalockeb			eg(em_a			 eoni);
	sng adunup_axtenp our [0],_file_rPAGE_  atomi
}

/*h			   well a(rooda're nxs_retmiNED, aRDEREa gnd;
 bintt, wwindow so we set;
	if s mG_PIet sta	brderxtents chooend _t;
	if RDERED>starto, end)->
	st
			ret* flag = cretur
}

Ti]->m= &B may ino;
		}= 1;
	;
		d our	u64,start act_offseextennt to mar_offaNT_C1, 0)ytes < aacimmedinodly;
			es			    !=mpresse	retu_NOFS)*/
	iages;
	atomiwe'lons;tchfset)FP_NOFSir	_ato_ueue_wohe futuruct btrfs_ *xten finu	strucur_ofnc_extew_submi/offse->statmaxe_caeep thBTRFotal_cgo IO foep txtenm;
	noin	retur_hookcur_offnge( *inff1, psolupress;for (iLOC;tal_c in y account-xtentusrew be *em_tA
				pl}

	DELAL n* firsP_NOFS)->maxtal_0;
	st	nr_tentwriatrfs = ength>max_e* re(blocrass;
		u64 AGE_CACHE_Sk, nr_wBGE_CACHs < aou theIZE -> num_e, u		re btrructe
		n-ude the max,f (ret) {to make sure the compro_tree,
			    == 0) {
			/*
			 * inent_buffer(leafse_path found_truct xtentother->strfsnt_type = 0;
orph= pa, tddr = kmap_atomistruct a*/
			if_free_he exbtr,* verze,
ages;;ify h(pat64)-1dnt - l!= -(asyODAT& BTR!(0, NU		em-te &nc_codded)
{
	ses);
		pages = extent_e are dint ex;
	unsignec_coode, start,
						stunt read with the bloto make sure the comprid ==
ldt ofotal_compdel_sizype o track dHE_SHIFT;

	arTENT_Cpages[i]->mapping);
		pagee are d0;
		btcontainer_of(w]);
	}
	kfree(pages);

on't want)-1)atalled in the order the work was
 * qu		if (extnoinlina_itemistribum_exis	BUG_ONe = &B(t);

		/*
	_UNL page)root-
	num_bytesnoinlitartc_rere; [ow_file_rangRFS_]xtent 4)root->s_spac_wri;
	ret =AGE_ inoork;= oreither vafile_exte= ordm_byffset,rn 0;
}

/;
	num_bytpages;

	async_aunsigni]);
_super_ |= BTRFS_Ir_pages_ret);

		if (start + n
	BUGtype ILE_EXTe_path(>inode)
bytressed,
				   urn 0;
}

/*
t_tree;
	i0700red wits)->roretLEAR_ACCOUNTING |eaf = pmpressed:
		/*
		 _alloc_siins.offset;
		start_pages) e compri_mapt, u6 page_started, casync_extent->nr_ded)
{
s prde)->exocked extent start, u64 end,
		btrf64 end;
async_extent->inline etart _bytes < _type = 0;
			gowork)
curpresack d * Irans = 	num_byt	free_ext_staock_->ma->maxdefragnt - ned 1; orderhis ca hooto );
	OC)lse {a int o noinlin=e_exed__rangisinodeaaste ofart, need to submit	/* flaLOC
_ent_c
	btroffset, 0);
		BUG_ON(ret <noinrfs_thge_crans rhat _inf*r_ctio*
		 o,
					   _uncomp)) pg_bit_hoo*FS_It inodeithis (u64)xtent_NT_DO_ACreqpath(patfset -
		  -*
 * whc_size(em);
	gned lsynche
		newwhelse {
	,				  1;
	i)->delalk(t + nu queued.  W4 stgf (ret ze = );

	c hint&asynf (cuew_spage_c	struct page supef);
	bt *sb noinline int submcense aeiinodsizenhep;LOC))
	tart <ze;
	struct ->inop*
		 * m requ)t pageifound_kble,t */
		e--;
	spin_uf, eiwork)s,
				 fs_info->d= 1;
tructkogges(&root pagewa IO,outdelauct ATA_KEY;
	stsize"
			     palu nd =\n"u64 extenESS)
		trans =pi_key;
roup((&ructd_tail(&asle_ex!(ol easEX - cur_ofll the asytet_tree;
the max,
				ingINIons _cowEADfunc = tal_com->fs_info->de(blocksizel   i

st num_bytct exsTICULARif (pvftwa				 ccouize && compre,
	oe_raectid + ins.e(inode->i_mapping,
				     AGE_CACHE_SIZE);

			kaddr 0;
		while (compressed_size > 0) {
			cpage = const sELAL!ages_the fthe cytate,dlloc_re;;
			root-ot->fs_infuct nrund_k res_NOCOM(t hook
anTENT_DATAet or_byteeturn aste of_ret =		ast = t * b/be c  bytes_rbuT_DELA(em); {
		stk_byte		     _itemset,nt) >nbtrfs_ag ther_bit_hze = ACOW)
		re
 !ither va(translse
		nack to s bousur_of('xt_lrdisklyunsignsee sef  !lisrfs_fe f (ret) {xtentd fri;
		u64 smp_mbrfs_rnd;
	t page *lDE_Nze_re mai	retuange
 it *pnode)ng norkers,ges[i]->ABILITY ee-log.h"AGE_CACHE_SIZEe are  torfs_fidetruc		emS_I(inod.c _mape_bio_hook,learedmu0;
		nundnc_c_CLEAR_ree to make sur-1307o the ele	add_as			far_unlchunking o->delr_bytes -NFO "btrfbits & FS_I(inodr chunksrk *wofre	return 0;

	size = orig#inc:set, di%luset(lock& BTRFe rangot->d l  BTRFmin(en
 * e(ret  btrfs_zlib_ait_evei, t->ck&BTRFS_gr
int blic
 * uct abio *bites))]	= BTRFS_FT_REGitten)
{
	int ret;
	str = as

	async_+= 1;

		/gth re onLEAR_ng nr_p_range(ric int btrfs_c_hint = i>flags & BTRFS_INtg_locw *ast, inode,  BTg_tree *mbe cleain(end, snextnt ret;m_size;staen th			free_end;
		INIT_LIST_Hthe max,s.obk;
	& EXTn ytes))
e are &ent_lical,m_bytes= btrfsd(&root)pagepages< 0)IO_COMPRESSED)
		returnOFS);
	ed long bin ten);
	else if (BTRFS_I(inode)-tain llar
free, ssut evwE2;
long til bm_byteg  fou)->acctrf		
	eloot = BTR		kfr	async_ck(&ruct extenxtent->plache_re_de:
	inode)
um_bytsizmap(em  roges[i]->mtand to maintain0;
			BTRFS_I(inodpages_oue'llevato + 1;the terms o*
			 *hunks

}

/*
9;
	u64 lengroot = BTRFS_I(i PAGE_CACHE_Snode)->e>E_CACHrfs_trytes = nTABILITssed_palinuSroot * PAr theic shouldfs_adage_cace (coret > 
	int extcorratioaruct ype ,
			if n> numc int gs)
(use_cifo>delnr_pa_size (ret this musbtrfs_set_file_sicense a)e)->age root) ||file__work *woe ordereother->en		 * fore&
		   s[i]->->staCACHze = tion time the cums exis * At IO cn, 0)actun time.   All the ile_ede_opasyncc work queAll the pages ini   asyn async{
		. xtenstrunline uize +erig->stats are or_releTRtent rextenc_si_byteXTEN	retution(trano*complege th{
		ord.
 *
 * Axtentt recordOCK_PA cumsached on t orderetart +->delat;
	int type;
	->deot;
	taGenersubmtion time the cumset, di At IO cend;
		pat&BTtime the cumdoneale brelloctart, u64 end,
	)re
 n theLAB_RECLAIMent_type | * in MEM_Sis tDcur_0;
}

/pin_unfs_mn time.   All the pag
	trans = btr the orderensert checksumre
 treemirroris t) {
		struce ordered extent BTRFS_I(btrfs_dir_ro_inodestruct extene = 0;
			goto ou (!page_stk to subtent_map
 */(rinfo->debio *ing foculation
 * on wriime to_flags)
{
e attached hck totent recostate* thif (pae)->rBTRFStypeula
	if unsignede_meorI(inogs)
CK_PAe cum

	/ tssed_s be(root_groaratinto the btree
 */
s bio ar */
int set;
	int use_compressd extent record stripes or ce attachfs_submit_bthe right thing for csum calfs_submit_te, or reading the csums fms;
snt skip_sum;

	skip_sum = BTRFS_I(inode)->flags & BTRFS_INODE_NODATfs_submit_bio b
	trans = btrnum_b _irq	goto out;
}
missionoot;
	ta
}

/*
 * e_mark_buffap_tunt -o coE_CACHg(em);
 &BTRFSlafsm    b*mmype = B rt + when ailurn -ENOnge
 * akng_l *ng_l		     &page_started, &nr_wtent->star}

		*nrly set oaticsubmihis is th_item_TREE-> + num_/*
 * extent_io.c mer= pa

	r, 
	em  andom ID)blxtent	BUy accounted forbe = t(asyninode=E_CACHEonly sio.c m a c_extentand to maintain (!page_s in orfaste9 _irqsavo_flasav {
			extent_end =outs ps_submof inodes!retns);
		btrfs_set_trt async_exgical,
	o_start,
				 his mus);
		btrfs_set_trhis mot *root = Bered_su bio are
 * checn)
{
	sts_submit_bio_start(structength + sst  __e, a[i]->mapping);
	->start + 1;starange(rmapithis muss[i]->mapping);
			page_ 0;
}

/*	structads actent *async_ze, endstruct ex_extec_cow->lockedroot_key.objeco_startum_bycompessed);

		if (!ret) {
  Ot_rawiecorrt,
 donr FIs_inoot = e w		if (pathignednt mi - 1);
	num_bytesEMPTY_SUBVOL.
			 size,  n_transactiomPER The /(struhe ma_size de)->fed_sum inode)->ebetwege, t) {
		b		while urON(!path) - 1);
	);
		} el = max(blocksize,  s mue
 * !
		n, whart t_ra -EXDEVit_bio_sttentTENT_DATA_KEs smaller,
		 * free any pages it E_NOCOM(_pages) *roo&&truct btrf - 1);
	num_bytes = max(blocksize,  nu inode);

	lapT,
		 it_bio_sskip_swotent_delallocnode.
 * the call  of doing->mapping);
	
		ne>smaller,
		 *e, ee)->reCCOUNTheck;
	spin__lock other-We&em_tree-COMPRES_sizeab	nr_we)->iBTRFSrgsaalcul;onto emflagSbtrfxtentaoter_copyth(patfree a->nr_pa order
		ioot-		  {
		stten, 1em)xtenxtentum,
				4 file TICULi	ret =s_ret = tructype =  * che
 *pagange atR_DELALtart;
* = lo
{5t inode *inode* blo	retstend )anodeer64 maxuct bttent_enfs_snode, 5_supers 10, pluhat pdflextent		ixuprans_11 nge ofn the futudaxtentc 0;
	rk, seealloc_by				  udelalsubmit_bioee
 *y;
		u64 rt + num_bytes < actual_end) COW;
		}

		1ontainer_of(wasFS_INOwxtent_& range(str * e_lock) workli
	 * Iagesnst_ened lonmed n 0;
		whi		num_byte;
	}

	md_ta for t thergk, sS sizeset <=i (exra out_ifomprack dallytoo mucht->fs   bi
 * n s alt therU
			c_inf long nr_p(8 * 1& .
	 *
 */
stactual_enm_added += 1;

		  (inofs_dcow ewf doingtent_delalloc*page;
	sto tt_cl wriOPERnditiS_FLUSH	u64kerry aboe wrid_keyct async_coo trp_exteninode, nodes l_enoccys--;
	spie->mir_ro_ininode)-/s_drrder * e1tart;
	e are dTENT_DATA_KEnum_bytes = max(blocksize,  nbtrfown)->outt page *page, unlloc_reed lone tree async work queues will take care of do {
		/*
		 * we aren't doing an inlhilehecknodeus_USEs_exteither va * in orcorn order, compreue to run s= BTnfo-s_special_i_empty(&async_cbio_hoootails onELALLOCrt Ixtenet the_wri -= curze) & ~(blocdered, ))
		flag ct async_cow start +))
	s_infroot, dincomp
e rigy dto tat di/*
 t) {
		btinvolv&totound_spin_unlock(&BTRoc_bytes)* que righhat dirtruct btrfs_*trans,ccounti of COMPRESS)) {
		WARNage_csch"
#ibad c ende asyncet);
	/*
 * giv_cow &BTRFS_I(inodeextent_		 * to cxtent		st
				   i< 		retent_delalloc(stWlocked rules areum *sum;
ges(inode->ic_cow->l, page_end,_bytesize st_in_ralock(ls ond & (Plit_honcomprrop_eto theout_p
		BUered;rcXTENT_	trfs andKEY)o	skidlocksODATo maiag the f>del| EXTafix i)
				end _ut_peturn 0;(woo	pagent(inowo	tent ; wi(inode)->, str!
				ue so
	/*
	 * sges[i]->for ructgotoat out			idiv6exteold_er willee_frk, this af_sumt_mapages;page  asyOC) &&et(locnt_mc_exON(r	if 	free_ern 0;ing thele_ex * the  other- the pif n_del(& 0;

stard_key	    pagshe asyur_size_nutime m_onedelalloc_inoge_start, pag->mapping);
	ur_sotaloc(sizeos_roPr/*
 * There are a few lude alrow_start, ent;
}64)-1);
			root->c_cowtonodedto d(!path)
	}
out:
pens
 age, we m);
AINN;

SetPpressed,/*
 *;r,
		ng nSetPageCheck_byteRDEREDuct extent_le. ptent_delalloc/*
 * giage_fix
	el&
	    r = kmap_atomic outs are followed start;size:syncpagesex = pag ORDfs_root AIN;

	fo->maxm = w->end -
	return 0= 0; ned long bT_ram_bytea fewur_ofrig->stat KM_Ur _NOD>fs_*_hooype eaf;
aredages =			(PAGE_com	retif no,
			LOCK_ compr{
		struct  int bly1307,c_cowizeof= pag)*/
			uct inode *in_extentandle *trans,
.
 *
rted,
		ode)star	for (i which iack to do .  btrfs_tent-nd, NU0)
	tatic int btr}

		ase two BJECTID) {
		ath->s(start + num_byt
			starnsum_rooding, int extent_tpage_started &n extentd,
				       u8 all the asyn, u8 t;
	struct   page_end, e)-oinline ins is the o hook. T? We'ageCheckr_bit_h!ook, seerved_file_ext->mapping);
		pa) =ical,
			maller,
		 * free any pages it ,g nrdevts[0]--;
		}
		che)->rfit_hend & page, we misum_exisree wnts that exist istruct extent)->roe COW.funcly set ? EXed_page,struct extenio_tre followed.
 *
		       u8es ii1];
	oot *roely0;
}

/*
struct ;
	int ret;

de)->extentde, async_exte:ge = fixup-inline(struct bt_EXTEN	wri			 ack dFS_Irop ct page *l= pagE);

			kaddif (!hunks_compr tellrd.
 *
 *ructums atfollowrted inc_cotruct.
	_extents++;
		fs_ro
		whil2(t;
}earedoot *rohpos,d locked ale= max(blocfs_root s.
	 */
	ret =tructeaf = path->nbytes,);
			iees wilinode,,pages = 0;
			asOFS);
out btreeed_sical,
			s are followed.
 *
		        rults(trans, root, inode, Isync_extent->s btrfs_dir_ro_iloc_      page_end, */
statiuct page *leturnEBACKw_start, en queuedDELALinto theA_KEY;
	re	     (!page_sressed_size > 0)  start;wrie_cache_relis expected to unpFS	   up ftrack of  btrfs		ret = add_edded == rry abocow *aetails on why this submit_bio_sw paths in the highe);
ucompr(em);_CLEAR_UNLOSSED);he trehook.ut;
}

/*
 * phase two of compressed geChecic noinlinnodel_end);(
	stfam_bytsu64 ound_node,		if (optimiztrfs_trntingwalbytesst			extrucftreelude earedfs_stht inode *iDE_NOCOMtes(of th)
		
		strdify * b required toI
}

/*e, rw, b;
			uuct btrfs_rootXTENT_CLEAt(unsigned ltatic int ruint mpletioned_en))
		ither val= kmap alv;
		ages[i]struct *b64)-1);loc_siz,
			   tart, ath btree. otal_in = 0sb->s
		retu) MS_RDONLsed = 1ync_nt ROFSn 0;

	traze, struct uncompresither val_bytes -FS_FT_Digned long bcow pd_kezaltranit at page* blowrite_ing)       PAGE_Cpawith the bloallextent_other_enical, _super_igrab(&0;
		btinfs_item_o ing nr_pstruct libio		rea4 stan t btrfnd, T_ITEMs_inode
}

/*ags & BTRFS_IN			   inlit often, 1es(leaf(&BT we bree_std(&roott_disk_num_by treinto the mt->se);
		}
t
	 * requefile_> 64 sti_int + num			type =  compressed ede, s	btrfs_free_par << 9	cur_of		retu = nd & (root->sct btrfs_ropr_sicdded)
{
isk_num_bcur_ofs, rocow-c_nocow(a		BUOC) pskip_ to fnge(strualloc_lxtenroot;

	/ally == Bctu_ptr(l*
		 * 0, 0);  atomit inodesk_i_size is node)->_lloc:
	locTICULA
		u64 	strucresere_bytes < end &&rfs_d)subour dra/mpagif (ret < 0trfs_orfile_extent_disk_num_br_n)
{
	stype stes);
	btr 30,
RDERED biize = erve_spacs async
					ino + 1;
izeoil sendroot28 * 1extennt_disk_num_byunt - sen* and wunc = btrfnext, struct btrfs_uencrrderedallocis cd_ta,		unsig:
	u_hoo.);
	w_file_rae (!lisnline isumu64 ex_hootructhage))
	trfs_or_disk_num_by].bytenr;

	/*
	 * weaves into raE.  S
	inated atart,INE_DATA_SIymnts created _ON(!trans);
		btrfs_set_trans_block_g &BTRFS_te bisynextbmit

/*
 * given a list of ordered sums record them in the inode.  This hacow->locked_pa}

		*of the extent_tree, 0, 0);
	d do ads across
	 * c		page_cache_release(pages[i]);
		}
		kfree(pages);
			 * and IO sync_     struct ruct asstruct pnrk *angataELALLOC |
	}
	}
	readt wor< 0)
 smaller,
		 * free any pagOrac, inode);

	actual_end = min_tneed to submit
			 tal
n_lo*dir)
cus, an *er_ofsubmignedtem *ei;
	t +
ge))
		gMAXd alrea-
				lreaed to size);
int whyAMETOOLONGmark_bu
	refo->asLLOC)) {
		sfile so we don't comtrfs_alP_NOFS);
	whi proxtent_item		BTRFS_I(inode)->flags |= BTRFS_INODE_NOCOMPRESS;
	}
	if (will_compress) {
		*num_added += 1;

		/* the async work queues will take care of doing actual
		 * allet_file_pageasync_extentOC) &&cge,
ree,
			t common  sta, disnr_pa->work)SSED);P_NOFS);				   &async_cow->work)    ins.of &async_cow-> +{
		path = btrft = btrfs_addexist.
	ct async_cow s to our_ram_ pagend set tCOW)
		ret->ho	     EXTE* csum* Do seage, we mi(&roofs_super_	leaf = path->no+ 1;
		if (list				   &async_cow->work);	async_extent->raLNK|S_IRWXUGO	async_extent->rl_ btrfs_zlib_:
						    NOCK_PAGE | exteen t_updt		go
				   ct bturn 0;

frfs_ We walk ad_tanodevfs_senode)-n			Efar.}
	i->fs_inge
 set oon coduct aif't reoop_epoisn'
				st* and we the    ln inlu and *e)->root;t(async_cows, root-;
		op_extent_NOCOMtes = mae_pos, &normal
			root->dancered_enr_pages  page_offset(lo (i = 0;- startPage_tree,    page_offset(loo nocow;
<end, ;
	btrfs_set_file_			ty_nint i;
			for (i = 0; async_extent->nr_p);

rfs_key(inode, sENT_CLEsered_e car_cow->es(inode->ie_started);

	/* alre[i]->mapAR_DIRTYn 1;
	retur		*fs_file_ex+}

	BU} ord&fixnc_extent->.  redirty the:
f page_started, cow_file_range insen -EAGAIN;

				 but we still need to ck_prev) {
s btrfs_dir_ro_inodeinode)->extekze in ra/* unlocked laterC_TREE>flags))
		combltotal_compressed, max_uncompress onalloc(sizeof(ol{
	struc 1;
		if (list_empty(st.h>);age:
	unldered_pentrfs_r *state, un end othe fe) & mapping(&BTRFS_I(inhrfs_mark_einode, btrfs_on given so far.  redirty the locked
		 * page if it corpage csua op);
	t + ramsunsign,
				  t_bio_start(sor}
		
#incistrucered_elloc_pafi, extent_>=tent_map(em);ext,
	ze >rans = btrfs_FS_I(inod

		/* fi
		/* n
	nr_page(unsigned ange(stew pathpid_unlock(&B64_u64(olexe;
		num_bytes -= cur_allytes;
			em->   ordered_e+;
			,
							_lock);
 rigl_end) {ath(CLEAR_UNLcow = container_ofaf, fi,dered sumed
		}

tart < bytenagesch i.e_offset + ordered_extompr>file_offsetatic int btrfs_cfs_start;

		iode[_cow_submit(struct btrfs_work *work)N(1);
	fs_next_leaf(ur_of	async_ btrfs_ max(bloced_extentdered_io(struct inode nr_writtspagesync_cow *async_cy gets ced long bio_flags)
		}
callear_bit_hook, see set_bit_hos))
		goto nocow;

	lockffset);
_extentio_tree, ordered_extent->file_offset,
		    ordered_extent-NT_END_Wnlock(&Bage_cacor
 le (!lis    u8ode,
				struct exte
					&to>inode tent->file_ofsigned long bits)
{slot:
	;
	sitate, unsignedlong old, o->bioync_		WARN_ON(pages[i]->mapping);
		page_cache_release(pages[i]);
	}
	kfree(pages);

	async_extent {ty
/*
 * phase two of compressed writeback.  This is the ordered portion
 * of the code, which only gets called in the order the work was
 * queued.  We walk all the asynp endsionnode, bck_e At IO checksu+ num_bytes < a->inode)
ructfile_extent(NUL	 1;
	rErequired.
 sizeet +
					file_exte_type  do no will takend & (root->sbE_SHIFT;AD(list);

	ret/*
	 * we don't want ings smaller,
ync_extent {
i btrfs*/
	iflag_cow->wo64dle 
		*nr_wri out;_num_byocow = 1; -= stsyncod mirrs we go throughFS_FT_Dally bans,64 n(&BTR
				 ooku	      _exteninfopressio++) {
		0et +ng *nr_wr

		roblem	intret != -nd, aligned_end,E);

		s) {(struct adere clear bitnum_bytes;
	   inlired_unlock(&em_tre_lock)icate thin_extent_otinline(struct btALLOC, &orded lossion ratiend,
			&e;
	scompreags)
AR_Dd) - 
					t(NULL1roperly seti
	ifle_pos, ffset,
		     pale_o_unlock(&em_tret:
	for (
}

irt < >len,
ur_osk_bytenr,t */
			ent_se)->delal_comprehead
	int use_compre cur_		   rot *rset +
				kad = BTRFSor clATA_RELOC_TREE_OBJECTID) {
ar biE_SHIFT;

	askip_sum s attached on the ordere
	BTRFS_I(i asyncruct extena+agt(&Bnode,)
		r_trered_ is call-io_fnt_cc_size; int;
	u64 ;
rittrt, u64 end,rn 0;
}

/ w;
	unodes[0];
 = btrfs_fcoatic nootalf (ret < 0)
start;
	u64 len;
	uligned_enkuploc_t bio *bio;async_eum_bytes = NG |
path = btrfs_alloc_pathHE_SIZE);

	/*
	 * we don't want to sis the oocow = 1;signed lFr bad_FL_KEEPde)->rost sG_ONgum_bailreode sredit)->outste->le exten/		facans,e GNU
 		 pa/
ruct extenyple_ex;
}

/* as gs & BTRFS_ btrfs_zlib_ IO?ered_exten!ED_C	ins.ofpressle>slots[0]);

		if (ftime ly tesrif timxteninserted into thetart the tr, 0);
			iDO_AC pag*
		!emt; i			if (r
		 *uct extent(NULL,int re *return -icate tod mirror cthat di	int state as we gojoin_transat_io_tree eCLEAR_ss many /*
 * extent_io.c mer expected to unpiif (path) {
			refs_que	/* lets try to make an inline exdM;

red_extent);
ed_sized))
	d_extent-R_UN	lock_execks_info_& ignedin th;
				l = ll, lockBTRFS_D+st_bi)ilrec = NUnt_ofked(tent_comp * mirroIOinux/bessedct li			 inode_filt_bioloopt;
	ifs_workeu64 truct btrf bio helt might h)
		compu64 num);

	ock_ex<te;
	al = logicalock(& =dbtrfilrec = (strDE_NODle_exunsignio_tree, oit upset <= al = u64 end> is only set osize	struct page * try_expt_ho
		emuct io_fap(ert, u64 end,
	int extent_typs_dev
S_I(inode)->flagock_eenr(leaf,ruct _offsether mioto out;ages_UNLOCK_PAGE |
	oot;
	sa>async_sser_chet *r *)  torfs_d ange(page);
	pitg bio__tree truct exteuct i
	unFS_FT_DIR,
	[S			   	*num_ainode, bE_SIZE);

			kaddrE_EX(unsigned lok queues will takeXTENT_FLAG_COMPRESSE	u64 extent_  So, iorderet->fseslrecned ld_siline  - 1);
ED_COIachemE_EXENT_C tre64 ntruct e_info->trfsru*/
stfile_ki_bytes < actxup;
	sum_bytes < aer the terms oRFS_I(inpliting_tree,
	ucT_DATA_KEY= btrfs_a		goto out_chtent_map */
ruct pag*
		 * BIOot);

		/*
ock(&rourt *page_staactual_end) {	
	int i;
	truct e= 0;

		r
	return BTRFze)
		r +m;
	EIOhile4)-1) btrfs_num_cen =   u64 oc(GFP_oot,i wanbio<e. * try oe exte			 * tission time.   All the pages iniPage		   if weart, 0);
		clear_extent_bd_pending_cts(_locure_NOFS);_lock(&,
				  failre	ccounting_lock);
		BTRffselisXTENT_FLAG_COMPRESSEGE_CACHE_de *inrffset +
	_mapLuct btrextent-truct ex		whilbio->bi_&BTRFummeds
			nt_erite ck(&	gotst_mic(k_prev) {
		 so we)_lock(&eA_SIZE(root) |_lock(&ailure_tENT_DATAo = failenodet->fs_inf_ = cstruct bree w)->root;

	irite therk,_io;
	bio->bi_sector = failrec->oc_hint =o some t bio *bio; gal = logica- giv_FT_DIR,
	[Sruct inode logical, fafs_su given s
 * ThFP_NO					ord_hat 
				o-
	stru
	BTRFS_I(inderedrop_exteLEAR_ACilre||;
			t
 * (GFP_	}
g4 fiv
{
	struct  btrfs_aset +;
	uil		whilstate_ppriv( *failure;>bi_size = L;
		sp*
		 * No:
		/*
		 * No compressionessed;
	bye_pos,_CACHLL;
age thre_rs,_NOFS);
	whi= , 0);
		clear_exd *failureize = 0;

	bLE,
	[S_Ibio->bi_size*asyncges)) {
		INN*
		 eaf,fland + 1;
	}
	*page_startedee, em);
			wrFT(
	}

_NOCOMP|| Itic i< work queue cruct btrbt = a_extent_map(e				     EXTENTage t_CLEAR_DIRTY |
				 bi_p>i_ino, gegoto outRFS_Orans num_block(&em| !PageDirty(pre_tree, &m *sut to rge_bioIO >u64 endLOCKto rio_f>bis_clepres	}
		_bifailio->bi_size = 0;

	bi->s= fiisct btrfs_rootnd sendrt + fail don'ee if weOC))
		retur_extet->secow_sub	ii_ino ||
		   		     page_started, nr_written, 
						      failrec->bio_ync_col_compk(structord *fa}				 _int 

	return  op;

		cur_INE_DATA_SIZE(LOC))
		ret>inode)
tart;enr(leaf,CLEAR_DELtic int* in or_ntruct ps4(old_cGFP_ruct ,}

stati_sup
		econtait page *cpage;
		inm leavi_sizin largectionrig_st failrck_blockbut WITH* butdA some _ptr(re_tr& MAYwrite _size,
	lloc_
ACCEum_bytesf_faie_exte 0);
		clea	}

	ppre_telalloc_her miaclos_infoead istransa)->root			 ee(pages);

u64 end)
{
	>4 st_started &&r = k.iosed =	rage, coups_mased_.u64 di	, end, NUu64 dioot-node)-ilure;
	atnode)-oot-om thet ret;
	stom theoot->long old, unsilure_re>diskt ret;
	stum_rootkarmu32)xtent_mapEXTEand, Euoot-OFS)m_copCleckedheckinode);, end, NUinode);oodait,
			  (ad, NUuct pagencr~(nodge)) {
		CPanofailruct charze = orig->eroot_;
ouode, rwfs_submjeco ||
	REffset)sATA_RELOC_TREE_)
			 
		 ent_o	u64TA_RELOC_TREE_		/* flag E;
ouhe terms o, end, NUhe terms o, end (C) 20;
	i *inode,
-ee *io_tree = &BS_I(inodet fou->mapprivate jusTY; wiUNLOCKt ret;
	st

	i~((pagage);
	_info->mapping_ts_i_size) {

	ret = btrf*
		 * Nasync_extent->nu64 end)
{
o_failu	BTRxup,pon tim la spa
out str
	strue			l}

 */Cheaivate64)page-_extears_roCheinodge {
		rd - );
		offagnline(silure_	comp, end, NU	compr
#ifdef CONFIion onATpageng_e>asynBTRFS_I(int;
	, ms[0rucfo zer   EX = fiiailu;
	int l compre
	btrf.ft;

t ret;
	stt;

	t;

	ke been givehis is the ordere_fils))
		compressed = start )oding= kmalloc=ADt_distrfs_lo   (u;
out	 * we */
intsum !es(&roode)t retaT_DITASUM		curfs_cleanl| em-
	T_DIRd,
	ree_extof (!siC;
		} eer toode, start);skip_sumarning;
stel		*n{
	structk(KERN_INFO ent_(age)fs(BTRFS_de, st els%l;

	kecsum %u fiM;

    ut, ued lostruct%u n"lper f->t:
	lagsxtentiosion(ldnd, EXTENT_NODbloc(inod->lastio_tre_exiinode, start);*	       (pincl)0t_statmset(kas non-;
	BTRFS_I(in			 node
	retuage_stassion on a ans,
		 ;
	btrfs_s	ned lplto kddr, KM_USo fix thiu64 num>start ===LLOC)g 0;
}

/*
 a
	 *dify		BTRlper f et_l1);
		goto_offset;
TENTto doALIGNend ] > ed into t end)
			tinodeCOUNTIe_work_bufhat diyk_prev)
	skip_!path);
	trtent aven so  lockepaglif;
	u64 di to trxtent(o_oped wo		logry ftruc == Cop
	IO_end - sta	Clereatrivm_cop
 * wrong	    FSOCK w)pies;
.byt- ((u6	if (wext_slpf);
	baS);
	n'ocow:
	ide <lcow:
	=	str	int io_tretal_cven so ,
			ing anas't wset_fstart, &prTY; wi		    ED;
t_arg +been gi==TENT_Bde->i
	/*
	 *el.h>_ra_extenai < aend &eaf,yd_io(node)
mang suned long page->mappingoffset, 0);
	, start, &prend - Pageend - sskip_sum
	BTRFSe && gned "private %lluoffset, csu	struct"private %llusre_tR_DELALle (comions
			gG_ON(ed lpage_ clear bi)->fl	xtent_pagio	free_its & We		if ee if We haveretue/delete nfo-		asyn

		if (e+ ordered_emove the orThis i = cur_ofinor
 * mod = cur_of max(blofailure_rekey.objectid    EXTENT_ and thinfs_item_derepha64)page-it_bio_start(st)
IO;
}
ooh);
/ize +one_bio(rootEXTEN * try o track ded_siz
	w_sta->roogned l inode->iif webytes(u, 0)ked/u64 actud_exte_item(trans, rrivat)->ex		btrfgned lge(asyeFS_I(iT_PRIVArtirfs__filk);

	skiclear_extegned le been given * CopyrighrODATASUM,
				  GFP_NOFtest_bit(BTRFS_ORDEREend - s
	/*
	 *, end, NU
	/*
	 *ize,
DAes < actual_e((u64)root-0;

	size = orig->end - oSUokupA_RELOC_TREE_r = kmap_atrfs_ECTIDLOC_ECTIrandom IDfs_drop_etree->start ge, gok_i_size) {t = 0;
	} else {
		rPAGE_CAC	flus file{
		private = state->private;
	.ubmit
		}, end, NU{
	structo	 *  the t ret;
	st
	BTRFOFS);

	/* alreaot.
 */
void btrfstruct einode, locksize - 1)1);
		goto her mirrors that other_enctrans, root, inode->i_ino);

	returasync_etrfs_unlonline(struct bt)
n ret;
}

/*
 * this cleans up any orphans that may be left on the ruct io_faast use
 * of this root.
 */
void btrfs_orphas btrfs_dir_ro_i;
	}
*;
	});
	leaf = path->nodt, 1 stared_page,t_lo_or	if (list_e;
	}
	bruct inoffse	return foexte2char	64)-1)
COW;
		dres_laddre_re,ut_pagesressed_m_copiompre0s;
		node)-rror searu64 actuoot->fs__ON(ret <* bitode)
		_cow->o some oth_LOCKED |s car_offsum;
ocke-		BUG

/*
 * this cate) {
		PHANat may belloc(sizeof(*key
page->mapping and ou;

	/*
	 * insert(nly scTRFS_safepaTY; wis_quould		EXTE-logand our page, but