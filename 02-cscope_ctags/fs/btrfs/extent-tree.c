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
#include <linux/sched.h>
#include <linux/pagemap.h>
#include <linux/writeback.h>
#include <linux/blkdev.h>
#include <linux/sort.h>
#include <linux/rcupdate.h>
#include <linux/kthread.h>
#include "compat.h"
#include "hash.h"
#include "ctree.h"
#include "disk-io.h"
#include "print-tree.h"
#include "transaction.h"
#include "volumes.h"
#include "locking.h"
#include "free-space-cache.h"

static int update_block_group(struct btrfs_trans_handle *trans,
			      struct btrfs_root *root,
			      u64 bytenr, u64 num_bytes, int alloc,
			      int mark_free);
static int update_reserved_extents(struct btrfs_block_group_cache *cache,
				   u64 num_bytes, int reserve);
static int __btrfs_free_extent(struct btrfs_trans_handle *trans,
				struct btrfs_root *root,
				u64 bytenr, u64 num_bytes, u64 parent,
				u64 root_objectid, u64 owner_objectid,
				u64 owner_offset, int refs_to_drop,
				struct btrfs_delayed_extent_op *extra_op);
static void __run_delayed_extent_op(struct btrfs_delayed_extent_op *extent_op,
				    struct extent_buffer *leaf,
				    struct btrfs_extent_item *ei);
static int alloc_reserved_file_extent(struct btrfs_trans_handle *trans,
				      struct btrfs_root *root,
				      u64 parent, u64 root_objectid,
				      u64 flags, u64 owner, u64 offset,
				      struct btrfs_key *ins, int ref_mod);
static int alloc_reserved_tree_block(struct btrfs_trans_handle *trans,
				     struct btrfs_root *root,
				     u64 parent, u64 root_objectid,
				     u64 flags, struct btrfs_disk_key *key,
				     int level, struct btrfs_key *ins);
static int do_chunk_alloc(struct btrfs_trans_handle *trans,
			  struct btrfs_root *extent_root, u64 alloc_bytes,
			  u64 flags, int force);
static int pin_down_bytes(struct btrfs_trans_handle *trans,
			  struct btrfs_root *root,
			  struct btrfs_path *path,
			  u64 bytenr, u64 num_bytes,
			  int is_data, int reserved,
			  struct extent_buffer **must_clean);
static int find_next_key(struct btrfs_path *path, int level,
			 struct btrfs_key *key);
static void dump_space_info(struct btrfs_space_info *info, u64 bytes,
			    int dump_block_groups);

static noinline int
block_group_cache_done(struct btrfs_block_group_cache *cache)
{
	smp_mb();
	return cache->cached == BTRFS_CACHE_FINISHED;
}

static int block_group_bits(struct btrfs_block_group_cache *cache, u64 bits)
{
	return (cache->flags & bits) == bits;
}

/*
 * this adds the block group to the fs_info rb tree for the block group
 * cache
 */
static int btrfs_add_block_group_cache(struct btrfs_fs_info *info,
				struct btrfs_block_group_cache *block_group)
{
	struct rb_node **p;
	struct rb_node *parent = NULL;
	struct btrfs_block_group_cache *cache;

	spin_lock(&info->block_group_cache_lock);
	p = &info->block_group_cache_tree.rb_node;

	while (*p) {
		parent = *p;
		cache = rb_entry(parent, struct btrfs_block_group_cache,
				 cache_node);
		if (block_group->key.objectid < cache->key.objectid) {
			p = &(*p)->rb_left;
		} else if (block_group->key.objectid > cache->key.objectid) {
			p = &(*p)->rb_right;
		} else {
			spin_unlock(&info->block_group_cache_lock);
			return -EEXIST;
		}
	}

	rb_link_node(&block_group->cache_node, parent, p);
	rb_insert_color(&block_group->cache_node,
			&info->block_group_cache_tree);
	spin_unlock(&info->block_group_cache_lock);

	return 0;
}

/*
 * This will return the block group at or after bytenr if contains is 0, else
 * it will return the block group that contains the bytenr
 */
static struct btrfs_block_group_cache *
block_group_cache_tree_search(struct btrfs_fs_info *info, u64 bytenr,
			      int contains)
{
	struct btrfs_block_group_cache *cache, *ret = NULL;
	struct rb_node *n;
	u64 end, start;

	spin_lock(&info->block_group_cache_lock);
	n = info->block_group_cache_tree.rb_node;

	while (n) {
		cache = rb_entry(n, struct btrfs_block_group_cache,
				 cache_node);
		end = cache->key.objectid + cache->key.offset - 1;
		start = cache->key.objectid;

		if (bytenr < start) {
			if (!contains && (!ret || start < ret->key.objectid))
				ret = cache;
			n = n->rb_left;
		} else if (bytenr > start) {
			if (contains && bytenr <= end) {
				ret = cache;
				break;
			}
			n = n->rb_right;
		} else {
			ret = cache;
			break;
		}
	}
	if (ret)
		atomic_inc(&ret->count);
	spin_unlock(&info->block_group_cache_lock);

	return ret;
}

static int add_excluded_extent(struct btrfs_root *root,
			       u64 start, u64 num_bytes)
{
	u64 end = start + num_bytes - 1;
	set_extent_bits(&root->fs_info->freed_extents[0],
			start, end, EXTENT_UPTODATE, GFP_NOFS);
	set_extent_bits(&root->fs_info->freed_extents[1],
			start, end, EXTENT_UPTODATE, GFP_NOFS);
	return 0;
}

static void free_excluded_extents(struct btrfs_root *root,
				  struct btrfs_block_group_cache *cache)
{
	u64 start, end;

	start = cache->key.objectid;
	end = start + cache->key.offset - 1;

	clear_extent_bits(&root->fs_info->freed_extents[0],
			  start, end, EXTENT_UPTODATE, GFP_NOFS);
	clear_extent_bits(&root->fs_info->freed_extents[1],
			  start, end, EXTENT_UPTODATE, GFP_NOFS);
}

static int exclude_super_stripes(struct btrfs_root *root,
				 struct btrfs_block_group_cache *cache)
{
	u64 bytenr;
	u64 *logical;
	int stripe_len;
	int i, nr, ret;

	for (i = 0; i < BTRFS_SUPER_MIRROR_MAX; i++) {
		bytenr = btrfs_sb_offset(i);
		ret = btrfs_rmap_block(&root->fs_info->mapping_tree,
				       cache->key.objectid, bytenr,
				       0, &logical, &nr, &stripe_len);
		BUG_ON(ret);

		while (nr--) {
			cache->bytes_super += stripe_len;
			ret = add_excluded_extent(root, logical[nr],
						  stripe_len);
			BUG_ON(ret);
		}

		kfree(logical);
	}
	return 0;
}

static struct btrfs_caching_control *
get_caching_control(struct btrfs_block_group_cache *cache)
{
	struct btrfs_caching_control *ctl;

	spin_lock(&cache->lock);
	if (cache->cached != BTRFS_CACHE_STARTED) {
		spin_unlock(&cache->lock);
		return NULL;
	}

	ctl = cache->caching_ctl;
	atomic_inc(&ctl->count);
	spin_unlock(&cache->lock);
	return ctl;
}

static void put_caching_control(struct btrfs_caching_control *ctl)
{
	if (atomic_dec_and_test(&ctl->count))
		kfree(ctl);
}

/*
 * this is only called by cache_block_group, since we could have freed extents
 * we need to check the pinned_extents for any extents that can't be used yet
 * since their free space will be released as soon as the transaction commits.
 */
static u64 add_new_free_space(struct btrfs_block_group_cache *block_group,
			      struct btrfs_fs_info *info, u64 start, u64 end)
{
	u64 extent_start, extent_end, size, total_added = 0;
	int ret;

	while (start < end) {
		ret = find_first_extent_bit(info->pinned_extents, start,
					    &extent_start, &extent_end,
					    EXTENT_DIRTY | EXTENT_UPTODATE);
		if (ret)
			break;

		if (extent_start == start) {
			start = extent_end + 1;
		} else if (extent_start > start && extent_start < end) {
			size = extent_start - start;
			total_added += size;
			ret = btrfs_add_free_space(block_group, start,
						   size);
			BUG_ON(ret);
			start = extent_end + 1;
		} else {
			break;
		}
	}

	if (start < end) {
		size = end - start;
		total_added += size;
		ret = btrfs_add_free_space(block_group, start, size);
		BUG_ON(ret);
	}

	return total_added;
}

static int caching_kthread(void *data)
{
	struct btrfs_block_group_cache *block_group = data;
	struct btrfs_fs_info *fs_info = block_group->fs_info;
	struct btrfs_caching_control *caching_ctl = block_group->caching_ctl;
	struct btrfs_root *extent_root = fs_info->extent_root;
	struct btrfs_path *path;
	struct extent_buffer *leaf;
	struct btrfs_key key;
	u64 total_found = 0;
	u64 last = 0;
	u32 nritems;
	int ret = 0;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	exclude_super_stripes(extent_root, block_group);
	spin_lock(&block_group->space_info->lock);
	block_group->space_info->bytes_super += block_group->bytes_super;
	spin_unlock(&block_group->space_info->lock);

	last = max_t(u64, block_group->key.objectid, BTRFS_SUPER_INFO_OFFSET);

	/*
	 * We don't want to deadlock with somebody trying to allocate a new
	 * extent for the extent root while also trying to search the extent
	 * root to add free space.  So we skip locking and search the commit
	 * root, since its read-only
	 */
	path->skip_locking = 1;
	path->search_commit_root = 1;
	path->reada = 2;

	key.objectid = last;
	key.offset = 0;
	key.type = BTRFS_EXTENT_ITEM_KEY;
again:
	mutex_lock(&caching_ctl->mutex);
	/* need to make sure the commit_root doesn't disappear */
	down_read(&fs_info->extent_commit_sem);

	ret = btrfs_search_slot(NULL, extent_root, &key, path, 0, 0);
	if (ret < 0)
		goto err;

	leaf = path->nodes[0];
	nritems = btrfs_header_nritems(leaf);

	while (1) {
		smp_mb();
		if (fs_info->closing > 1) {
			last = (u64)-1;
			break;
		}

		if (path->slots[0] < nritems) {
			btrfs_item_key_to_cpu(leaf, &key, path->slots[0]);
		} else {
			ret = find_next_key(path, 0, &key);
			if (ret)
				break;

			caching_ctl->progress = last;
			btrfs_release_path(extent_root, path);
			up_read(&fs_info->extent_commit_sem);
			mutex_unlock(&caching_ctl->mutex);
			if (btrfs_transaction_in_commit(fs_info))
				schedule_timeout(1);
			else
				cond_resched();
			goto again;
		}

		if (key.objectid < block_group->key.objectid) {
			path->slots[0]++;
			continue;
		}

		if (key.objectid >= block_group->key.objectid +
		    block_group->key.offset)
			break;

		if (key.type == BTRFS_EXTENT_ITEM_KEY) {
			total_found += add_new_free_space(block_group,
							  fs_info, last,
							  key.objectid);
			last = key.objectid + key.offset;

			if (total_found > (1024 * 1024 * 2)) {
				total_found = 0;
				wake_up(&caching_ctl->wait);
			}
		}
		path->slots[0]++;
	}
	ret = 0;

	total_found += add_new_free_space(block_group, fs_info, last,
					  block_group->key.objectid +
					  block_group->key.offset);
	caching_ctl->progress = (u64)-1;

	spin_lock(&block_group->lock);
	block_group->caching_ctl = NULL;
	block_group->cached = BTRFS_CACHE_FINISHED;
	spin_unlock(&block_group->lock);

err:
	btrfs_free_path(path);
	up_read(&fs_info->extent_commit_sem);

	free_excluded_extents(extent_root, block_group);

	mutex_unlock(&caching_ctl->mutex);
	wake_up(&caching_ctl->wait);

	put_caching_control(caching_ctl);
	atomic_dec(&block_group->space_info->caching_threads);
	return 0;
}

static int cache_block_group(struct btrfs_block_group_cache *cache)
{
	struct btrfs_fs_info *fs_info = cache->fs_info;
	struct btrfs_caching_control *caching_ctl;
	struct task_struct *tsk;
	int ret = 0;

	smp_mb();
	if (cache->cached != BTRFS_CACHE_NO)
		return 0;

	caching_ctl = kzalloc(sizeof(*caching_ctl), GFP_KERNEL);
	BUG_ON(!caching_ctl);

	INIT_LIST_HEAD(&caching_ctl->list);
	mutex_init(&caching_ctl->mutex);
	init_waitqueue_head(&caching_ctl->wait);
	caching_ctl->block_group = cache;
	caching_ctl->progress = cache->key.objectid;
	/* one for caching kthread, one for caching block group list */
	atomic_set(&caching_ctl->count, 2);

	spin_lock(&cache->lock);
	if (cache->cached != BTRFS_CACHE_NO) {
		spin_unlock(&cache->lock);
		kfree(caching_ctl);
		return 0;
	}
	cache->caching_ctl = caching_ctl;
	cache->cached = BTRFS_CACHE_STARTED;
	spin_unlock(&cache->lock);

	down_write(&fs_info->extent_commit_sem);
	list_add_tail(&caching_ctl->list, &fs_info->caching_block_groups);
	up_write(&fs_info->extent_commit_sem);

	atomic_inc(&cache->space_info->caching_threads);

	tsk = kthread_run(caching_kthread, cache, "btrfs-cache-%llu\n",
			  cache->key.objectid);
	if (IS_ERR(tsk)) {
		ret = PTR_ERR(tsk);
		printk(KERN_ERR "error running thread %d\n", ret);
		BUG();
	}

	return ret;
}

/*
 * return the block group that starts at or after bytenr
 */
static struct btrfs_block_group_cache *
btrfs_lookup_first_block_group(struct btrfs_fs_info *info, u64 bytenr)
{
	struct btrfs_block_group_cache *cache;

	cache = block_group_cache_tree_search(info, bytenr, 0);

	return cache;
}

/*
 * return the block group that contains the given bytenr
 */
struct btrfs_block_group_cache *btrfs_lookup_block_group(
						 struct btrfs_fs_info *info,
						 u64 bytenr)
{
	struct btrfs_block_group_cache *cache;

	cache = block_group_cache_tree_search(info, bytenr, 1);

	return cache;
}

void btrfs_put_block_group(struct btrfs_block_group_cache *cache)
{
	if (atomic_dec_and_test(&cache->count))
		kfree(cache);
}

static struct btrfs_space_info *__find_space_info(struct btrfs_fs_info *info,
						  u64 flags)
{
	struct list_head *head = &info->space_info;
	struct btrfs_space_info *found;

	rcu_read_lock();
	list_for_each_entry_rcu(found, head, list) {
		if (found->flags == flags) {
			rcu_read_unlock();
			return found;
		}
	}
	rcu_read_unlock();
	return NULL;
}

/*
 * after adding space to the filesystem, we need to clear the full flags
 * on all the space infos.
 */
void btrfs_clear_space_info_full(struct btrfs_fs_info *info)
{
	struct list_head *head = &info->space_info;
	struct btrfs_space_info *found;

	rcu_read_lock();
	list_for_each_entry_rcu(found, head, list)
		found->full = 0;
	rcu_read_unlock();
}

static u64 div_factor(u64 num, int factor)
{
	if (factor == 10)
		return num;
	num *= factor;
	do_div(num, 10);
	return num;
}

u64 btrfs_find_block_group(struct btrfs_root *root,
			   u64 search_start, u64 search_hint, int owner)
{
	struct btrfs_block_group_cache *cache;
	u64 used;
	u64 last = max(search_hint, search_start);
	u64 group_start = 0;
	int full_search = 0;
	int factor = 9;
	int wrapped = 0;
again:
	while (1) {
		cache = btrfs_lookup_first_block_group(root->fs_info, last);
		if (!cache)
			break;

		spin_lock(&cache->lock);
		last = cache->key.objectid + cache->key.offset;
		used = btrfs_block_group_used(&cache->item);

		if ((full_search || !cache->ro) &&
		    block_group_bits(cache, BTRFS_BLOCK_GROUP_METADATA)) {
			if (used + cache->pinned + cache->reserved <
			    div_factor(cache->key.offset, factor)) {
				group_start = cache->key.objectid;
				spin_unlock(&cache->lock);
				btrfs_put_block_group(cache);
				goto found;
			}
		}
		spin_unlock(&cache->lock);
		btrfs_put_block_group(cache);
		cond_resched();
	}
	if (!wrapped) {
		last = search_start;
		wrapped = 1;
		goto again;
	}
	if (!full_search && factor < 10) {
		last = search_start;
		full_search = 1;
		factor = 10;
		goto again;
	}
found:
	return group_start;
}

/* simple helper to search for an existing extent at a given offset */
int btrfs_lookup_extent(struct btrfs_root *root, u64 start, u64 len)
{
	int ret;
	struct btrfs_key key;
	struct btrfs_path *path;

	path = btrfs_alloc_path();
	BUG_ON(!path);
	key.objectid = start;
	key.offset = len;
	btrfs_set_key_type(&key, BTRFS_EXTENT_ITEM_KEY);
	ret = btrfs_search_slot(NULL, root->fs_info->extent_root, &key, path,
				0, 0);
	btrfs_free_path(path);
	return ret;
}

/*
 * Back reference rules.  Back refs have three main goals:
 *
 * 1) differentiate between all holders of references to an extent so that
 *    when a reference is dropped we can make sure it was a valid reference
 *    before freeing the extent.
 *
 * 2) Provide enough information to quickly find the holders of an extent
 *    if we notice a given block is corrupted or bad.
 *
 * 3) Make it easy to migrate blocks for FS shrinking or storage pool
 *    maintenance.  This is actually the same as #2, but with a slightly
 *    different use case.
 *
 * There are two kinds of back refs. The implicit back refs is optimized
 * for pointers in non-shared tree blocks. For a given pointer in a block,
 * back refs of this kind provide information about the block's owner tree
 * and the pointer's key. These information allow us to find the block by
 * b-tree searching. The full back refs is for pointers in tree blocks not
 * referenced by their owner trees. The location of tree block is recorded
 * in the back refs. Actually the full back refs is generic, and can be
 * used in all cases the implicit back refs is used. The major shortcoming
 * of the full back refs is its overhead. Every time a tree block gets
 * COWed, we have to update back refs entry for all pointers in it.
 *
 * For a newly allocated tree block, we use implicit back refs for
 * pointers in it. This means most tree related operations only involve
 * implicit back refs. For a tree block created in old transaction, the
 * only way to drop a reference to it is COW it. So we can detect the
 * event that tree block loses its owner tree's reference and do the
 * back refs conversion.
 *
 * When a tree block is COW'd through a tree, there are four cases:
 *
 * The reference count of the block is one and the tree is the block's
 * owner tree. Nothing to do in this case.
 *
 * The reference count of the block is one and the tree is not the
 * block's owner tree. In this case, full back refs is used for pointers
 * in the block. Remove these full back refs, add implicit back refs for
 * every pointers in the new block.
 *
 * The reference count of the block is greater than one and the tree is
 * the block's owner tree. In this case, implicit back refs is used for
 * pointers in the block. Add full back refs for every pointers in the
 * block, increase lower level extents' reference counts. The original
 * implicit back refs are entailed to the new block.
 *
 * The reference count of the block is greater than one and the tree is
 * not the block's owner tree. Add implicit back refs for every pointer in
 * the new block, increase lower level extents' reference count.
 *
 * Back Reference Key composing:
 *
 * The key objectid corresponds to the first byte in the extent,
 * The key type is used to differentiate between types of back refs.
 * There are different meanings of the key offset for different types
 * of back refs.
 *
 * File extents can be referenced by:
 *
 * - multiple snapshots, subvolumes, or different generations in one subvol
 * - different files inside a single subvolume
 * - different offsets inside a file (bookend extents in file.c)
 *
 * The extent ref structure for the implicit back refs has fields for:
 *
 * - Objectid of the subvolume root
 * - objectid of the file holding the reference
 * - original offset in the file
 * - how many bookend extents
 *
 * The key offset for the implicit back refs is hash of the first
 * three fields.
 *
 * The extent ref structure for the full back refs has field for:
 *
 * - number of pointers in the tree leaf
 *
 * The key offset for the implicit back refs is the first byte of
 * the tree leaf
 *
 * When a file extent is allocated, The implicit back refs is used.
 * the fields are filled in:
 *
 *     (root_key.objectid, inode objectid, offset in file, 1)
 *
 * When a file extent is removed file truncation, we find the
 * corresponding implicit back refs and check the following fields:
 *
 *     (btrfs_header_owner(leaf), inode objectid, offset in file)
 *
 * Btree extents can be referenced by:
 *
 * - Different subvolumes
 *
 * Both the implicit back refs and the full back refs for tree blocks
 * only consist of key. The key offset for the implicit back refs is
 * objectid of block's owner tree. The key offset for the full back refs
 * is the first byte of parent block.
 *
 * When implicit back refs is used, information about the lowest key and
 * level of the tree block are required. These information are stored in
 * tree block info structure.
 */

#ifdef BTRFS_COMPAT_EXTENT_TREE_V0
static int convert_extent_item_v0(struct btrfs_trans_handle *trans,
				  struct btrfs_root *root,
				  struct btrfs_path *path,
				  u64 owner, u32 extra_size)
{
	struct btrfs_extent_item *item;
	struct btrfs_extent_item_v0 *ei0;
	struct btrfs_extent_ref_v0 *ref0;
	struct btrfs_tree_block_info *bi;
	struct extent_buffer *leaf;
	struct btrfs_key key;
	struct btrfs_key found_key;
	u32 new_size = sizeof(*item);
	u64 refs;
	int ret;

	leaf = path->nodes[0];
	BUG_ON(btrfs_item_size_nr(leaf, path->slots[0]) != sizeof(*ei0));

	btrfs_item_key_to_cpu(leaf, &key, path->slots[0]);
	ei0 = btrfs_item_ptr(leaf, path->slots[0],
			     struct btrfs_extent_item_v0);
	refs = btrfs_extent_refs_v0(leaf, ei0);

	if (owner == (u64)-1) {
		while (1) {
			if (path->slots[0] >= btrfs_header_nritems(leaf)) {
				ret = btrfs_next_leaf(root, path);
				if (ret < 0)
					return ret;
				BUG_ON(ret > 0);
				leaf = path->nodes[0];
			}
			btrfs_item_key_to_cpu(leaf, &found_key,
					      path->slots[0]);
			BUG_ON(key.objectid != found_key.objectid);
			if (found_key.type != BTRFS_EXTENT_REF_V0_KEY) {
				path->slots[0]++;
				continue;
			}
			ref0 = btrfs_item_ptr(leaf, path->slots[0],
					      struct btrfs_extent_ref_v0);
			owner = btrfs_ref_objectid_v0(leaf, ref0);
			break;
		}
	}
	btrfs_release_path(root, path);

	if (owner < BTRFS_FIRST_FREE_OBJECTID)
		new_size += sizeof(*bi);

	new_size -= sizeof(*ei0);
	ret = btrfs_search_slot(trans, root, &key, path,
				new_size + extra_size, 1);
	if (ret < 0)
		return ret;
	BUG_ON(ret);

	ret = btrfs_extend_item(trans, root, path, new_size);
	BUG_ON(ret);

	leaf = path->nodes[0];
	item = btrfs_item_ptr(leaf, path->slots[0], struct btrfs_extent_item);
	btrfs_set_extent_refs(leaf, item, refs);
	/* FIXME: get real generation */
	btrfs_set_extent_generation(leaf, item, 0);
	if (owner < BTRFS_FIRST_FREE_OBJECTID) {
		btrfs_set_extent_flags(leaf, item,
				       BTRFS_EXTENT_FLAG_TREE_BLOCK |
				       BTRFS_BLOCK_FLAG_FULL_BACKREF);
		bi = (struct btrfs_tree_block_info *)(item + 1);
		/* FIXME: get first key of the block */
		memset_extent_buffer(leaf, 0, (unsigned long)bi, sizeof(*bi));
		btrfs_set_tree_block_level(leaf, bi, (int)owner);
	} else {
		btrfs_set_extent_flags(leaf, item, BTRFS_EXTENT_FLAG_DATA);
	}
	btrfs_mark_buffer_dirty(leaf);
	return 0;
}
#endif

static u64 hash_extent_data_ref(u64 root_objectid, u64 owner, u64 offset)
{
	u32 high_crc = ~(u32)0;
	u32 low_crc = ~(u32)0;
	__le64 lenum;

	lenum = cpu_to_le64(root_objectid);
	high_crc = crc32c(high_crc, &lenum, sizeof(lenum));
	lenum = cpu_to_le64(owner);
	low_crc = crc32c(low_crc, &lenum, sizeof(lenum));
	lenum = cpu_to_le64(offset);
	low_crc = crc32c(low_crc, &lenum, sizeof(lenum));

	return ((u64)high_crc << 31) ^ (u64)low_crc;
}

static u64 hash_extent_data_ref_item(struct extent_buffer *leaf,
				     struct btrfs_extent_data_ref *ref)
{
	return hash_extent_data_ref(btrfs_extent_data_ref_root(leaf, ref),
				    btrfs_extent_data_ref_objectid(leaf, ref),
				    btrfs_extent_data_ref_offset(leaf, ref));
}

static int match_extent_data_ref(struct extent_buffer *leaf,
				 struct btrfs_extent_data_ref *ref,
				 u64 root_objectid, u64 owner, u64 offset)
{
	if (btrfs_extent_data_ref_root(leaf, ref) != root_objectid ||
	    btrfs_extent_data_ref_objectid(leaf, ref) != owner ||
	    btrfs_extent_data_ref_offset(leaf, ref) != offset)
		return 0;
	return 1;
}

static noinline int lookup_extent_data_ref(struct btrfs_trans_handle *trans,
					   struct btrfs_root *root,
					   struct btrfs_path *path,
					   u64 bytenr, u64 parent,
					   u64 root_objectid,
					   u64 owner, u64 offset)
{
	struct btrfs_key key;
	struct btrfs_extent_data_ref *ref;
	struct extent_buffer *leaf;
	u32 nritems;
	int ret;
	int recow;
	int err = -ENOENT;

	key.objectid = bytenr;
	if (parent) {
		key.type = BTRFS_SHARED_DATA_REF_KEY;
		key.offset = parent;
	} else {
		key.type = BTRFS_EXTENT_DATA_REF_KEY;
		key.offset = hash_extent_data_ref(root_objectid,
						  owner, offset);
	}
again:
	recow = 0;
	ret = btrfs_search_slot(trans, root, &key, path, -1, 1);
	if (ret < 0) {
		err = ret;
		goto fail;
	}

	if (parent) {
		if (!ret)
			return 0;
#ifdef BTRFS_COMPAT_EXTENT_TREE_V0
		key.type = BTRFS_EXTENT_REF_V0_KEY;
		btrfs_release_path(root, path);
		ret = btrfs_search_slot(trans, root, &key, path, -1, 1);
		if (ret < 0) {
			err = ret;
			goto fail;
		}
		if (!ret)
			return 0;
#endif
		goto fail;
	}

	leaf = path->nodes[0];
	nritems = btrfs_header_nritems(leaf);
	while (1) {
		if (path->slots[0] >= nritems) {
			ret = btrfs_next_leaf(root, path);
			if (ret < 0)
				err = ret;
			if (ret)
				goto fail;

			leaf = path->nodes[0];
			nritems = btrfs_header_nritems(leaf);
			recow = 1;
		}

		btrfs_item_key_to_cpu(leaf, &key, path->slots[0]);
		if (key.objectid != bytenr ||
		    key.type != BTRFS_EXTENT_DATA_REF_KEY)
			goto fail;

		ref = btrfs_item_ptr(leaf, path->slots[0],
				     struct btrfs_extent_data_ref);

		if (match_extent_data_ref(leaf, ref, root_objectid,
					  owner, offset)) {
			if (recow) {
				btrfs_release_path(root, path);
				goto again;
			}
			err = 0;
			break;
		}
		path->slots[0]++;
	}
fail:
	return err;
}

static noinline int insert_extent_data_ref(struct btrfs_trans_handle *trans,
					   struct btrfs_root *root,
					   struct btrfs_path *path,
					   u64 bytenr, u64 parent,
					   u64 root_objectid, u64 owner,
					   u64 offset, int refs_to_add)
{
	struct btrfs_key key;
	struct extent_buffer *leaf;
	u32 size;
	u32 num_refs;
	int ret;

	key.objectid = bytenr;
	if (parent) {
		key.type = BTRFS_SHARED_DATA_REF_KEY;
		key.offset = parent;
		size = sizeof(struct btrfs_shared_data_ref);
	} else {
		key.type = BTRFS_EXTENT_DATA_REF_KEY;
		key.offset = hash_extent_data_ref(root_objectid,
						  owner, offset);
		size = sizeof(struct btrfs_extent_data_ref);
	}

	ret = btrfs_insert_empty_item(trans, root, path, &key, size);
	if (ret && ret != -EEXIST)
		goto fail;

	leaf = path->nodes[0];
	if (parent) {
		struct btrfs_shared_data_ref *ref;
		ref = btrfs_item_ptr(leaf, path->slots[0],
				     struct btrfs_shared_data_ref);
		if (ret == 0) {
			btrfs_set_shared_data_ref_count(leaf, ref, refs_to_add);
		} else {
			num_refs = btrfs_shared_data_ref_count(leaf, ref);
			num_refs += refs_to_add;
			btrfs_set_shared_data_ref_count(leaf, ref, num_refs);
		}
	} else {
		struct btrfs_extent_data_ref *ref;
		while (ret == -EEXIST) {
			ref = btrfs_item_ptr(leaf, path->slots[0],
					     struct btrfs_extent_data_ref);
			if (match_extent_data_ref(leaf, ref, root_objectid,
						  owner, offset))
				break;
			btrfs_release_path(root, path);
			key.offset++;
			ret = btrfs_insert_empty_item(trans, root, path, &key,
						      size);
			if (ret && ret != -EEXIST)
				goto fail;

			leaf = path->nodes[0];
		}
		ref = btrfs_item_ptr(leaf, path->slots[0],
				     struct btrfs_extent_data_ref);
		if (ret == 0) {
			btrfs_set_extent_data_ref_root(leaf, ref,
						       root_objectid);
			btrfs_set_extent_data_ref_objectid(leaf, ref, owner);
			btrfs_set_extent_data_ref_offset(leaf, ref, offset);
			btrfs_set_extent_data_ref_count(leaf, ref, refs_to_add);
		} else {
			num_refs = btrfs_extent_data_ref_count(leaf, ref);
			num_refs += refs_to_add;
			btrfs_set_extent_data_ref_count(leaf, ref, num_refs);
		}
	}
	btrfs_mark_buffer_dirty(leaf);
	ret = 0;
fail:
	btrfs_release_path(root, path);
	return ret;
}

static noinline int remove_extent_data_ref(struct btrfs_trans_handle *trans,
					   struct btrfs_root *root,
					   struct btrfs_path *path,
					   int refs_to_drop)
{
	struct btrfs_key key;
	struct btrfs_extent_data_ref *ref1 = NULL;
	struct btrfs_shared_data_ref *ref2 = NULL;
	struct extent_buffer *leaf;
	u32 num_refs = 0;
	int ret = 0;

	leaf = path->nodes[0];
	btrfs_item_key_to_cpu(leaf, &key, path->slots[0]);

	if (key.type == BTRFS_EXTENT_DATA_REF_KEY) {
		ref1 = btrfs_item_ptr(leaf, path->slots[0],
				      struct btrfs_extent_data_ref);
		num_refs = btrfs_extent_data_ref_count(leaf, ref1);
	} else if (key.type == BTRFS_SHARED_DATA_REF_KEY) {
		ref2 = btrfs_item_ptr(leaf, path->slots[0],
				      struct btrfs_shared_data_ref);
		num_refs = btrfs_shared_data_ref_count(leaf, ref2);
#ifdef BTRFS_COMPAT_EXTENT_TREE_V0
	} else if (key.type == BTRFS_EXTENT_REF_V0_KEY) {
		struct btrfs_extent_ref_v0 *ref0;
		ref0 = btrfs_item_ptr(leaf, path->slots[0],
				      struct btrfs_extent_ref_v0);
		num_refs = btrfs_ref_count_v0(leaf, ref0);
#endif
	} else {
		BUG();
	}

	BUG_ON(num_refs < refs_to_drop);
	num_refs -= refs_to_drop;

	if (num_refs == 0) {
		ret = btrfs_del_item(trans, root, path);
	} else {
		if (key.type == BTRFS_EXTENT_DATA_REF_KEY)
			btrfs_set_extent_data_ref_count(leaf, ref1, num_refs);
		else if (key.type == BTRFS_SHARED_DATA_REF_KEY)
			btrfs_set_shared_data_ref_count(leaf, ref2, num_refs);
#ifdef BTRFS_COMPAT_EXTENT_TREE_V0
		else {
			struct btrfs_extent_ref_v0 *ref0;
			ref0 = btrfs_item_ptr(leaf, path->slots[0],
					struct btrfs_extent_ref_v0);
			btrfs_set_ref_count_v0(leaf, ref0, num_refs);
		}
#endif
		btrfs_mark_buffer_dirty(leaf);
	}
	return ret;
}

static noinline u32 extent_data_ref_count(struct btrfs_root *root,
					  struct btrfs_path *path,
					  struct btrfs_extent_inline_ref *iref)
{
	struct btrfs_key key;
	struct extent_buffer *leaf;
	struct btrfs_extent_data_ref *ref1;
	struct btrfs_shared_data_ref *ref2;
	u32 num_refs = 0;

	leaf = path->nodes[0];
	btrfs_item_key_to_cpu(leaf, &key, path->slots[0]);
	if (iref) {
		if (btrfs_extent_inline_ref_type(leaf, iref) ==
		    BTRFS_EXTENT_DATA_REF_KEY) {
			ref1 = (struct btrfs_extent_data_ref *)(&iref->offset);
			num_refs = btrfs_extent_data_ref_count(leaf, ref1);
		} else {
			ref2 = (struct btrfs_shared_data_ref *)(iref + 1);
			num_refs = btrfs_shared_data_ref_count(leaf, ref2);
		}
	} else if (key.type == BTRFS_EXTENT_DATA_REF_KEY) {
		ref1 = btrfs_item_ptr(leaf, path->slots[0],
				      struct btrfs_extent_data_ref);
		num_refs = btrfs_extent_data_ref_count(leaf, ref1);
	} else if (key.type == BTRFS_SHARED_DATA_REF_KEY) {
		ref2 = btrfs_item_ptr(leaf, path->slots[0],
				      struct btrfs_shared_data_ref);
		num_refs = btrfs_shared_data_ref_count(leaf, ref2);
#ifdef BTRFS_COMPAT_EXTENT_TREE_V0
	} else if (key.type == BTRFS_EXTENT_REF_V0_KEY) {
		struct btrfs_extent_ref_v0 *ref0;
		ref0 = btrfs_item_ptr(leaf, path->slots[0],
				      struct btrfs_extent_ref_v0);
		num_refs = btrfs_ref_count_v0(leaf, ref0);
#endif
	} else {
		WARN_ON(1);
	}
	return num_refs;
}

static noinline int lookup_tree_block_ref(struct btrfs_trans_handle *trans,
					  struct btrfs_root *root,
					  struct btrfs_path *path,
					  u64 bytenr, u64 parent,
					  u64 root_objectid)
{
	struct btrfs_key key;
	int ret;

	key.objectid = bytenr;
	if (parent) {
		key.type = BTRFS_SHARED_BLOCK_REF_KEY;
		key.offset = parent;
	} else {
		key.type = BTRFS_TREE_BLOCK_REF_KEY;
		key.offset = root_objectid;
	}

	ret = btrfs_search_slot(trans, root, &key, path, -1, 1);
	if (ret > 0)
		ret = -ENOENT;
#ifdef BTRFS_COMPAT_EXTENT_TREE_V0
	if (ret == -ENOENT && parent) {
		btrfs_release_path(root, path);
		key.type = BTRFS_EXTENT_REF_V0_KEY;
		ret = btrfs_search_slot(trans, root, &key, path, -1, 1);
		if (ret > 0)
			ret = -ENOENT;
	}
#endif
	return ret;
}

static noinline int insert_tree_block_ref(struct btrfs_trans_handle *trans,
					  struct btrfs_root *root,
					  struct btrfs_path *path,
					  u64 bytenr, u64 parent,
					  u64 root_objectid)
{
	struct btrfs_key key;
	int ret;

	key.objectid = bytenr;
	if (parent) {
		key.type = BTRFS_SHARED_BLOCK_REF_KEY;
		key.offset = parent;
	} else {
		key.type = BTRFS_TREE_BLOCK_REF_KEY;
		key.offset = root_objectid;
	}

	ret = btrfs_insert_empty_item(trans, root, path, &key, 0);
	btrfs_release_path(root, path);
	return ret;
}

static inline int extent_ref_type(u64 parent, u64 owner)
{
	int type;
	if (owner < BTRFS_FIRST_FREE_OBJECTID) {
		if (parent > 0)
			type = BTRFS_SHARED_BLOCK_REF_KEY;
		else
			type = BTRFS_TREE_BLOCK_REF_KEY;
	} else {
		if (parent > 0)
			type = BTRFS_SHARED_DATA_REF_KEY;
		else
			type = BTRFS_EXTENT_DATA_REF_KEY;
	}
	return type;
}

static int find_next_key(struct btrfs_path *path, int level,
			 struct btrfs_key *key)

{
	for (; level < BTRFS_MAX_LEVEL; level++) {
		if (!path->nodes[level])
			break;
		if (path->slots[level] + 1 >=
		    btrfs_header_nritems(path->nodes[level]))
			continue;
		if (level == 0)
			btrfs_item_key_to_cpu(path->nodes[level], key,
					      path->slots[level] + 1);
		else
			btrfs_node_key_to_cpu(path->nodes[level], key,
					      path->slots[level] + 1);
		return 0;
	}
	return 1;
}

/*
 * look for inline back ref. if back ref is found, *ref_ret is set
 * to the address of inline back ref, and 0 is returned.
 *
 * if back ref isn't found, *ref_ret is set to the address where it
 * should be inserted, and -ENOENT is returned.
 *
 * if insert is true and there are too many inline back refs, the path
 * points to the extent item, and -EAGAIN is returned.
 *
 * NOTE: inline back refs are ordered in the same way that back ref
 *	 items in the tree are ordered.
 */
static noinline_for_stack
int lookup_inline_extent_backref(struct btrfs_trans_handle *trans,
				 struct btrfs_root *root,
				 struct btrfs_path *path,
				 struct btrfs_extent_inline_ref **ref_ret,
				 u64 bytenr, u64 num_bytes,
				 u64 parent, u64 root_objectid,
				 u64 owner, u64 offset, int insert)
{
	struct btrfs_key key;
	struct extent_buffer *leaf;
	struct btrfs_extent_item *ei;
	struct btrfs_extent_inline_ref *iref;
	u64 flags;
	u64 item_size;
	unsigned long ptr;
	unsigned long end;
	int extra_size;
	int type;
	int want;
	int ret;
	int err = 0;

	key.objectid = bytenr;
	key.type = BTRFS_EXTENT_ITEM_KEY;
	key.offset = num_bytes;

	want = extent_ref_type(parent, owner);
	if (insert) {
		extra_size = btrfs_extent_inline_ref_size(want);
		path->keep_locks = 1;
	} else
		extra_size = -1;
	ret = btrfs_search_slot(trans, root, &key, path, extra_size, 1);
	if (ret < 0) {
		err = ret;
		goto out;
	}
	BUG_ON(ret);

	leaf = path->nodes[0];
	item_size = btrfs_item_size_nr(leaf, path->slots[0]);
#ifdef BTRFS_COMPAT_EXTENT_TREE_V0
	if (item_size < sizeof(*ei)) {
		if (!insert) {
			err = -ENOENT;
			goto out;
		}
		ret = convert_extent_item_v0(trans, root, path, owner,
					     extra_size);
		if (ret < 0) {
			err = ret;
			goto out;
		}
		leaf = path->nodes[0];
		item_size = btrfs_item_size_nr(leaf, path->slots[0]);
	}
#endif
	BUG_ON(item_size < sizeof(*ei));

	ei = btrfs_item_ptr(leaf, path->slots[0], struct btrfs_extent_item);
	flags = btrfs_extent_flags(leaf, ei);

	ptr = (unsigned long)(ei + 1);
	end = (unsigned long)ei + item_size;

	if (flags & BTRFS_EXTENT_FLAG_TREE_BLOCK) {
		ptr += sizeof(struct btrfs_tree_block_info);
		BUG_ON(ptr > end);
	} else {
		BUG_ON(!(flags & BTRFS_EXTENT_FLAG_DATA));
	}

	err = -ENOENT;
	while (1) {
		if (ptr >= end) {
			WARN_ON(ptr > end);
			break;
		}
		iref = (struct btrfs_extent_inline_ref *)ptr;
		type = btrfs_extent_inline_ref_type(leaf, iref);
		if (want < type)
			break;
		if (want > type) {
			ptr += btrfs_extent_inline_ref_size(type);
			continue;
		}

		if (type == BTRFS_EXTENT_DATA_REF_KEY) {
			struct btrfs_extent_data_ref *dref;
			dref = (struct btrfs_extent_data_ref *)(&iref->offset);
			if (match_extent_data_ref(leaf, dref, root_objectid,
						  owner, offset)) {
				err = 0;
				break;
			}
			if (hash_extent_data_ref_item(leaf, dref) <
			    hash_extent_data_ref(root_objectid, owner, offset))
				break;
		} else {
			u64 ref_offset;
			ref_offset = btrfs_extent_inline_ref_offset(leaf, iref);
			if (parent > 0) {
				if (parent == ref_offset) {
					err = 0;
					break;
				}
				if (ref_offset < parent)
					break;
			} else {
				if (root_objectid == ref_offset) {
					err = 0;
					break;
				}
				if (ref_offset < root_objectid)
					break;
			}
		}
		ptr += btrfs_extent_inline_ref_size(type);
	}
	if (err == -ENOENT && insert) {
		if (item_size + extra_size >=
		    BTRFS_MAX_EXTENT_ITEM_SIZE(root)) {
			err = -EAGAIN;
			goto out;
		}
		/*
		 * To add new inline back ref, we have to make sure
		 * there is no corresponding back ref item.
		 * For simplicity, we just do not add new inline back
		 * ref if there is any kind of item for this block
		 */
		if (find_next_key(path, 0, &key) == 0 &&
		    key.objectid == bytenr &&
		    key.type < BTRFS_BLOCK_GROUP_ITEM_KEY) {
			err = -EAGAIN;
			goto out;
		}
	}
	*ref_ret = (struct btrfs_extent_inline_ref *)ptr;
out:
	if (insert) {
		path->keep_locks = 0;
		btrfs_unlock_up_safe(path, 1);
	}
	return err;
}

/*
 * helper to add new inline back ref
 */
static noinline_for_stack
int setup_inline_extent_backref(struct btrfs_trans_handle *trans,
				struct btrfs_root *root,
				struct btrfs_path *path,
				struct btrfs_extent_inline_ref *iref,
				u64 parent, u64 root_objectid,
				u64 owner, u64 offset, int refs_to_add,
				struct btrfs_delayed_extent_op *extent_op)
{
	struct extent_buffer *leaf;
	struct btrfs_extent_item *ei;
	unsigned long ptr;
	unsigned long end;
	unsigned long item_offset;
	u64 refs;
	int size;
	int type;
	int ret;

	leaf = path->nodes[0];
	ei = btrfs_item_ptr(leaf, path->slots[0], struct btrfs_extent_item);
	item_offset = (unsigned long)iref - (unsigned long)ei;

	type = extent_ref_type(parent, owner);
	size = btrfs_extent_inline_ref_size(type);

	ret = btrfs_extend_item(trans, root, path, size);
	BUG_ON(ret);

	ei = btrfs_item_ptr(leaf, path->slots[0], struct btrfs_extent_item);
	refs = btrfs_extent_refs(leaf, ei);
	refs += refs_to_add;
	btrfs_set_extent_refs(leaf, ei, refs);
	if (extent_op)
		__run_delayed_extent_op(extent_op, leaf, ei);

	ptr = (unsigned long)ei + item_offset;
	end = (unsigned long)ei + btrfs_item_size_nr(leaf, path->slots[0]);
	if (ptr < end - size)
		memmove_extent_buffer(leaf, ptr + size, ptr,
				      end - size - ptr);

	iref = (struct btrfs_extent_inline_ref *)ptr;
	btrfs_set_extent_inline_ref_type(leaf, iref, type);
	if (type == BTRFS_EXTENT_DATA_REF_KEY) {
		struct btrfs_extent_data_ref *dref;
		dref = (struct btrfs_extent_data_ref *)(&iref->offset);
		btrfs_set_extent_data_ref_root(leaf, dref, root_objectid);
		btrfs_set_extent_data_ref_objectid(leaf, dref, owner);
		btrfs_set_extent_data_ref_offset(leaf, dref, offset);
		btrfs_set_extent_data_ref_count(leaf, dref, refs_to_add);
	} else if (type == BTRFS_SHARED_DATA_REF_KEY) {
		struct btrfs_shared_data_ref *sref;
		sref = (struct btrfs_shared_data_ref *)(iref + 1);
		btrfs_set_shared_data_ref_count(leaf, sref, refs_to_add);
		btrfs_set_extent_inline_ref_offset(leaf, iref, parent);
	} else if (type == BTRFS_SHARED_BLOCK_REF_KEY) {
		btrfs_set_extent_inline_ref_offset(leaf, iref, parent);
	} else {
		btrfs_set_extent_inline_ref_offset(leaf, iref, root_objectid);
	}
	btrfs_mark_buffer_dirty(leaf);
	return 0;
}

static int lookup_extent_backref(struct btrfs_trans_handle *trans,
				 struct btrfs_root *root,
				 struct btrfs_path *path,
				 struct btrfs_extent_inline_ref **ref_ret,
				 u64 bytenr, u64 num_bytes, u64 parent,
				 u64 root_objectid, u64 owner, u64 offset)
{
	int ret;

	ret = lookup_inline_extent_backref(trans, root, path, ref_ret,
					   bytenr, num_bytes, parent,
					   root_objectid, owner, offset, 0);
	if (ret != -ENOENT)
		return ret;

	btrfs_release_path(root, path);
	*ref_ret = NULL;

	if (owner < BTRFS_FIRST_FREE_OBJECTID) {
		ret = lookup_tree_block_ref(trans, root, path, bytenr, parent,
					    root_objectid);
	} else {
		ret = lookup_extent_data_ref(trans, root, path, bytenr, parent,
					     root_objectid, owner, offset);
	}
	return ret;
}

/*
 * helper to update/remove inline back ref
 */
static noinline_for_stack
int update_inline_extent_backref(struct btrfs_trans_handle *trans,
				 struct btrfs_root *root,
				 struct btrfs_path *path,
				 struct btrfs_extent_inline_ref *iref,
				 int refs_to_mod,
				 struct btrfs_delayed_extent_op *extent_op)
{
	struct extent_buffer *leaf;
	struct btrfs_extent_item *ei;
	struct btrfs_extent_data_ref *dref = NULL;
	struct btrfs_shared_data_ref *sref = NULL;
	unsigned long ptr;
	unsigned long end;
	u32 item_size;
	int size;
	int type;
	int ret;
	u64 refs;

	leaf = path->nodes[0];
	ei = btrfs_item_ptr(leaf, path->slots[0], struct btrfs_extent_item);
	refs = btrfs_extent_refs(leaf, ei);
	WARN_ON(refs_to_mod < 0 && refs + refs_to_mod <= 0);
	refs += refs_to_mod;
	btrfs_set_extent_refs(leaf, ei, refs);
	if (extent_op)
		__run_delayed_extent_op(extent_op, leaf, ei);

	type = btrfs_extent_inline_ref_type(leaf, iref);

	if (type == BTRFS_EXTENT_DATA_REF_KEY) {
		dref = (struct btrfs_extent_data_ref *)(&iref->offset);
		refs = btrfs_extent_data_ref_count(leaf, dref);
	} else if (type == BTRFS_SHARED_DATA_REF_KEY) {
		sref = (struct btrfs_shared_data_ref *)(iref + 1);
		refs = btrfs_shared_data_ref_count(leaf, sref);
	} else {
		refs = 1;
		BUG_ON(refs_to_mod != -1);
	}

	BUG_ON(refs_to_mod < 0 && refs < -refs_to_mod);
	refs += refs_to_mod;

	if (refs > 0) {
		if (type == BTRFS_EXTENT_DATA_REF_KEY)
			btrfs_set_extent_data_ref_count(leaf, dref, refs);
		else
			btrfs_set_shared_data_ref_count(leaf, sref, refs);
	} else {
		size =  btrfs_extent_inline_ref_size(type);
		item_size = btrfs_item_size_nr(leaf, path->slots[0]);
		ptr = (unsigned long)iref;
		end = (unsigned long)ei + item_size;
		if (ptr + size < end)
			memmove_extent_buffer(leaf, ptr, ptr + size,
					      end - ptr - size);
		item_size -= size;
		ret = btrfs_truncate_item(trans, root, path, item_size, 1);
		BUG_ON(ret);
	}
	btrfs_mark_buffer_dirty(leaf);
	return 0;
}

static noinline_for_stack
int insert_inline_extent_backref(struct btrfs_trans_handle *trans,
				 struct btrfs_root *root,
				 struct btrfs_path *path,
				 u64 bytenr, u64 num_bytes, u64 parent,
				 u64 root_objectid, u64 owner,
				 u64 offset, int refs_to_add,
				 struct btrfs_delayed_extent_op *extent_op)
{
	struct btrfs_extent_inline_ref *iref;
	int ret;

	ret = lookup_inline_extent_backref(trans, root, path, &iref,
					   bytenr, num_bytes, parent,
					   root_objectid, owner, offset, 1);
	if (ret == 0) {
		BUG_ON(owner < BTRFS_FIRST_FREE_OBJECTID);
		ret = update_inline_extent_backref(trans, root, path, iref,
						   refs_to_add, extent_op);
	} else if (ret == -ENOENT) {
		ret = setup_inline_extent_backref(trans, root, path, iref,
						  parent, root_objectid,
						  owner, offset, refs_to_add,
						  extent_op);
	}
	return ret;
}

static int insert_extent_backref(struct btrfs_trans_handle *trans,
				 struct btrfs_root *root,
				 struct btrfs_path *path,
				 u64 bytenr, u64 parent, u64 root_objectid,
				 u64 owner, u64 offset, int refs_to_add)
{
	int ret;
	if (owner < BTRFS_FIRST_FREE_OBJECTID) {
		BUG_ON(refs_to_add != 1);
		ret = insert_tree_block_ref(trans, root, path, bytenr,
					    parent, root_objectid);
	} else {
		ret = insert_extent_data_ref(trans, root, path, bytenr,
					     parent, root_objectid,
					     owner, offset, refs_to_add);
	}
	return ret;
}

static int remove_extent_backref(struct btrfs_trans_handle *trans,
				 struct btrfs_root *root,
				 struct btrfs_path *path,
				 struct btrfs_extent_inline_ref *iref,
				 int refs_to_drop, int is_data)
{
	int ret;

	BUG_ON(!is_data && refs_to_drop != 1);
	if (iref) {
		ret = update_inline_extent_backref(trans, root, path, iref,
						   -refs_to_drop, NULL);
	} else if (is_data) {
		ret = remove_extent_data_ref(trans, root, path, refs_to_drop);
	} else {
		ret = btrfs_del_item(trans, root, path);
	}
	return ret;
}

static void btrfs_issue_discard(struct block_device *bdev,
				u64 start, u64 len)
{
	blkdev_issue_discard(bdev, start >> 9, len >> 9, GFP_KERNEL,
			     DISCARD_FL_BARRIER);
}

static int btrfs_discard_extent(struct btrfs_root *root, u64 bytenr,
				u64 num_bytes)
{
	int ret;
	u64 map_length = num_bytes;
	struct btrfs_multi_bio *multi = NULL;

	if (!btrfs_test_opt(root, DISCARD))
		return 0;

	/* Tell the block device(s) that the sectors can be discarded */
	ret = btrfs_map_block(&root->fs_info->mapping_tree, READ,
			      bytenr, &map_length, &multi, 0);
	if (!ret) {
		struct btrfs_bio_stripe *stripe = multi->stripes;
		int i;

		if (map_length > num_bytes)
			map_length = num_bytes;

		for (i = 0; i < multi->num_stripes; i++, stripe++) {
			btrfs_issue_discard(stripe->dev->bdev,
					    stripe->physical,
					    map_length);
		}
		kfree(multi);
	}

	return ret;
}

int btrfs_inc_extent_ref(struct btrfs_trans_handle *trans,
			 struct btrfs_root *root,
			 u64 bytenr, u64 num_bytes, u64 parent,
			 u64 root_objectid, u64 owner, u64 offset)
{
	int ret;
	BUG_ON(owner < BTRFS_FIRST_FREE_OBJECTID &&
	       root_objectid == BTRFS_TREE_LOG_OBJECTID);

	if (owner < BTRFS_FIRST_FREE_OBJECTID) {
		ret = btrfs_add_delayed_tree_ref(trans, bytenr, num_bytes,
					parent, root_objectid, (int)owner,
					BTRFS_ADD_DELAYED_REF, NULL);
	} else {
		ret = btrfs_add_delayed_data_ref(trans, bytenr, num_bytes,
					parent, root_objectid, owner, offset,
					BTRFS_ADD_DELAYED_REF, NULL);
	}
	return ret;
}

static int __btrfs_inc_extent_ref(struct btrfs_trans_handle *trans,
				  struct btrfs_root *root,
				  u64 bytenr, u64 num_bytes,
				  u64 parent, u64 root_objectid,
				  u64 owner, u64 offset, int refs_to_add,
				  struct btrfs_delayed_extent_op *extent_op)
{
	struct btrfs_path *path;
	struct extent_buffer *leaf;
	struct btrfs_extent_item *item;
	u64 refs;
	int ret;
	int err = 0;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	path->reada = 1;
	path->leave_spinning = 1;
	/* this will setup the path even if it fails to insert the back ref */
	ret = insert_inline_extent_backref(trans, root->fs_info->extent_root,
					   path, bytenr, num_bytes, parent,
					   root_objectid, owner, offset,
					   refs_to_add, extent_op);
	if (ret == 0)
		goto out;

	if (ret != -EAGAIN) {
		err = ret;
		goto out;
	}

	leaf = path->nodes[0];
	item = btrfs_item_ptr(leaf, path->slots[0], struct btrfs_extent_item);
	refs = btrfs_extent_refs(leaf, item);
	btrfs_set_extent_refs(leaf, item, refs + refs_to_add);
	if (extent_op)
		__run_delayed_extent_op(extent_op, leaf, item);

	btrfs_mark_buffer_dirty(leaf);
	btrfs_release_path(root->fs_info->extent_root, path);

	path->reada = 1;
	path->leave_spinning = 1;

	/* now insert the actual backref */
	ret = insert_extent_backref(trans, root->fs_info->extent_root,
				    path, bytenr, parent, root_objectid,
				    owner, offset, refs_to_add);
	BUG_ON(ret);
out:
	btrfs_free_path(path);
	return err;
}

static int run_delayed_data_ref(struct btrfs_trans_handle *trans,
				struct btrfs_root *root,
				struct btrfs_delayed_ref_node *node,
				struct btrfs_delayed_extent_op *extent_op,
				int insert_reserved)
{
	int ret = 0;
	struct btrfs_delayed_data_ref *ref;
	struct btrfs_key ins;
	u64 parent = 0;
	u64 ref_root = 0;
	u64 flags = 0;

	ins.objectid = node->bytenr;
	ins.offset = node->num_bytes;
	ins.type = BTRFS_EXTENT_ITEM_KEY;

	ref = btrfs_delayed_node_to_data_ref(node);
	if (node->type == BTRFS_SHARED_DATA_REF_KEY)
		parent = ref->parent;
	else
		ref_root = ref->root;

	if (node->action == BTRFS_ADD_DELAYED_REF && insert_reserved) {
		if (extent_op) {
			BUG_ON(extent_op->update_key);
			flags |= extent_op->flags_to_set;
		}
		ret = alloc_reserved_file_extent(trans, root,
						 parent, ref_root, flags,
						 ref->objectid, ref->offset,
						 &ins, node->ref_mod);
	} else if (node->action == BTRFS_ADD_DELAYED_REF) {
		ret = __btrfs_inc_extent_ref(trans, root, node->bytenr,
					     node->num_bytes, parent,
					     ref_root, ref->objectid,
					     ref->offset, node->ref_mod,
					     extent_op);
	} else if (node->action == BTRFS_DROP_DELAYED_REF) {
		ret = __btrfs_free_extent(trans, root, node->bytenr,
					  node->num_bytes, parent,
					  ref_root, ref->objectid,
					  ref->offset, node->ref_mod,
					  extent_op);
	} else {
		BUG();
	}
	return ret;
}

static void __run_delayed_extent_op(struct btrfs_delayed_extent_op *extent_op,
				    struct extent_buffer *leaf,
				    struct btrfs_extent_item *ei)
{
	u64 flags = btrfs_extent_flags(leaf, ei);
	if (extent_op->update_flags) {
		flags |= extent_op->flags_to_set;
		btrfs_set_extent_flags(leaf, ei, flags);
	}

	if (extent_op->update_key) {
		struct btrfs_tree_block_info *bi;
		BUG_ON(!(flags & BTRFS_EXTENT_FLAG_TREE_BLOCK));
		bi = (struct btrfs_tree_block_info *)(ei + 1);
		btrfs_set_tree_block_key(leaf, bi, &extent_op->key);
	}
}

static int run_delayed_extent_op(struct btrfs_trans_handle *trans,
				 struct btrfs_root *root,
				 struct btrfs_delayed_ref_node *node,
				 struct btrfs_delayed_extent_op *extent_op)
{
	struct btrfs_key key;
	struct btrfs_path *path;
	struct btrfs_extent_item *ei;
	struct extent_buffer *leaf;
	u32 item_size;
	int ret;
	int err = 0;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	key.objectid = node->bytenr;
	key.type = BTRFS_EXTENT_ITEM_KEY;
	key.offset = node->num_bytes;

	path->reada = 1;
	path->leave_spinning = 1;
	ret = btrfs_search_slot(trans, root->fs_info->extent_root, &key,
				path, 0, 1);
	if (ret < 0) {
		err = ret;
		goto out;
	}
	if (ret > 0) {
		err = -EIO;
		goto out;
	}

	leaf = path->nodes[0];
	item_size = btrfs_item_size_nr(leaf, path->slots[0]);
#ifdef BTRFS_COMPAT_EXTENT_TREE_V0
	if (item_size < sizeof(*ei)) {
		ret = convert_extent_item_v0(trans, root->fs_info->extent_root,
					     path, (u64)-1, 0);
		if (ret < 0) {
			err = ret;
			goto out;
		}
		leaf = path->nodes[0];
		item_size = btrfs_item_size_nr(leaf, path->slots[0]);
	}
#endif
	BUG_ON(item_size < sizeof(*ei));
	ei = btrfs_item_ptr(leaf, path->slots[0], struct btrfs_extent_item);
	__run_delayed_extent_op(extent_op, leaf, ei);

	btrfs_mark_buffer_dirty(leaf);
out:
	btrfs_free_path(path);
	return err;
}

static int run_delayed_tree_ref(struct btrfs_trans_handle *trans,
				struct btrfs_root *root,
				struct btrfs_delayed_ref_node *node,
				struct btrfs_delayed_extent_op *extent_op,
				int insert_reserved)
{
	int ret = 0;
	struct btrfs_delayed_tree_ref *ref;
	struct btrfs_key ins;
	u64 parent = 0;
	u64 ref_root = 0;

	ins.objectid = node->bytenr;
	ins.offset = node->num_bytes;
	ins.type = BTRFS_EXTENT_ITEM_KEY;

	ref = btrfs_delayed_node_to_tree_ref(node);
	if (node->type == BTRFS_SHARED_BLOCK_REF_KEY)
		parent = ref->parent;
	else
		ref_root = ref->root;

	BUG_ON(node->ref_mod != 1);
	if (node->action == BTRFS_ADD_DELAYED_REF && insert_reserved) {
		BUG_ON(!extent_op || !extent_op->update_flags ||
		       !extent_op->update_key);
		ret = alloc_reserved_tree_block(trans, root,
						parent, ref_root,
						extent_op->flags_to_set,
						&extent_op->key,
						ref->level, &ins);
	} else if (node->action == BTRFS_ADD_DELAYED_REF) {
		ret = __btrfs_inc_extent_ref(trans, root, node->bytenr,
					     node->num_bytes, parent, ref_root,
					     ref->level, 0, 1, extent_op);
	} else if (node->action == BTRFS_DROP_DELAYED_REF) {
		ret = __btrfs_free_extent(trans, root, node->bytenr,
					  node->num_bytes, parent, ref_root,
					  ref->level, 0, 1, extent_op);
	} else {
		BUG();
	}
	return ret;
}


/* helper function to actually process a single delayed ref entry */
static int run_one_delayed_ref(struct btrfs_trans_handle *trans,
			       struct btrfs_root *root,
			       struct btrfs_delayed_ref_node *node,
			       struct btrfs_delayed_extent_op *extent_op,
			       int insert_reserved)
{
	int ret;
	if (btrfs_delayed_ref_is_head(node)) {
		struct btrfs_delayed_ref_head *head;
		/*
		 * we've hit the end of the chain and we were supposed
		 * to insert this extent into the tree.  But, it got
		 * deleted before we ever needed to insert it, so all
		 * we have to do is clean up the accounting
		 */
		BUG_ON(extent_op);
		head = btrfs_delayed_node_to_head(node);
		if (insert_reserved) {
			int mark_free = 0;
			struct extent_buffer *must_clean = NULL;

			ret = pin_down_bytes(trans, root, NULL,
					     node->bytenr, node->num_bytes,
					     head->is_data, 1, &must_clean);
			if (ret > 0)
				mark_free = 1;

			if (must_clean) {
				clean_tree_block(NULL, root, must_clean);
				btrfs_tree_unlock(must_clean);
				free_extent_buffer(must_clean);
			}
			if (head->is_data) {
				ret = btrfs_del_csums(trans, root,
						      node->bytenr,
						      node->num_bytes);
				BUG_ON(ret);
			}
			if (mark_free) {
				ret = btrfs_free_reserved_extent(root,
							node->bytenr,
							node->num_bytes);
				BUG_ON(ret);
			}
		}
		mutex_unlock(&head->mutex);
		return 0;
	}

	if (node->type == BTRFS_TREE_BLOCK_REF_KEY ||
	    node->type == BTRFS_SHARED_BLOCK_REF_KEY)
		ret = run_delayed_tree_ref(trans, root, node, extent_op,
					   insert_reserved);
	else if (node->type == BTRFS_EXTENT_DATA_REF_KEY ||
		 node->type == BTRFS_SHARED_DATA_REF_KEY)
		ret = run_delayed_data_ref(trans, root, node, extent_op,
					   insert_reserved);
	else
		BUG();
	return ret;
}

static noinline struct btrfs_delayed_ref_node *
select_delayed_ref(struct btrfs_delayed_ref_head *head)
{
	struct rb_node *node;
	struct btrfs_delayed_ref_node *ref;
	int action = BTRFS_ADD_DELAYED_REF;
again:
	/*
	 * select delayed ref of type BTRFS_ADD_DELAYED_REF first.
	 * this prevents ref count from going down to zero when
	 * there still are pending delayed ref.
	 */
	node = rb_prev(&head->node.rb_node);
	while (1) {
		if (!node)
			break;
		ref = rb_entry(node, struct btrfs_delayed_ref_node,
				rb_node);
		if (ref->bytenr != head->node.bytenr)
			break;
		if (ref->action == action)
			return ref;
		node = rb_prev(node);
	}
	if (action == BTRFS_ADD_DELAYED_REF) {
		action = BTRFS_DROP_DELAYED_REF;
		goto again;
	}
	return NULL;
}

static noinline int run_clustered_refs(struct btrfs_trans_handle *trans,
				       struct btrfs_root *root,
				       struct list_head *cluster)
{
	struct btrfs_delayed_ref_root *delayed_refs;
	struct btrfs_delayed_ref_node *ref;
	struct btrfs_delayed_ref_head *locked_ref = NULL;
	struct btrfs_delayed_extent_op *extent_op;
	int ret;
	int count = 0;
	int must_insert_reserved = 0;

	delayed_refs = &trans->transaction->delayed_refs;
	while (1) {
		if (!locked_ref) {
			/* pick a new head ref from the cluster list */
			if (list_empty(cluster))
				break;

			locked_ref = list_entry(cluster->next,
				     struct btrfs_delayed_ref_head, cluster);

			/* grab the lock that says we are going to process
			 * all the refs for this head */
			ret = btrfs_delayed_ref_lock(trans, locked_ref);

			/*
			 * we may have dropped the spin lock to get the head
			 * mutex lock, and that might have given someone else
			 * time to free the head.  If that's true, it has been
			 * removed from our list and we can move on.
			 */
			if (ret == -EAGAIN) {
				locked_ref = NULL;
				count++;
				continue;
			}
		}

		/*
		 * record the must insert reserved flag before we
		 * drop the spin lock.
		 */
		must_insert_reserved = locked_ref->must_insert_reserved;
		locked_ref->must_insert_reserved = 0;

		extent_op = locked_ref->extent_op;
		locked_ref->extent_op = NULL;

		/*
		 * locked_ref is the head node, so we have to go one
		 * node back for any delayed ref updates
		 */
		ref = select_delayed_ref(locked_ref);
		if (!ref) {
			/* All delayed refs have been processed, Go ahead
			 * and send the head node to run_one_delayed_ref,
			 * so that any accounting fixes can happen
			 */
			ref = &locked_ref->node;

			if (extent_op && must_insert_reserved) {
				kfree(extent_op);
				extent_op = NULL;
			}

			if (extent_op) {
				spin_unlock(&delayed_refs->lock);

				ret = run_delayed_extent_op(trans, root,
							    ref, extent_op);
				BUG_ON(ret);
				kfree(extent_op);

				cond_resched();
				spin_lock(&delayed_refs->lock);
				continue;
			}

			list_del_init(&locked_ref->cluster);
			locked_ref = NULL;
		}

		ref->in_tree = 0;
		rb_erase(&ref->rb_node, &delayed_refs->root);
		delayed_refs->num_entries--;

		spin_unlock(&delayed_refs->lock);

		ret = run_one_delayed_ref(trans, root, ref, extent_op,
					  must_insert_reserved);
		BUG_ON(ret);

		btrfs_put_delayed_ref(ref);
		kfree(extent_op);
		count++;

		cond_resched();
		spin_lock(&delayed_refs->lock);
	}
	return count;
}

/*
 * this starts processing the delayed reference count updates and
 * extent insertions we have queued up so far.  count can be
 * 0, which means to process everything in the tree at the start
 * of the run (but not newly added entries), or it can be some target
 * number you'd like to process.
 */
int btrfs_run_delayed_refs(struct btrfs_trans_handle *trans,
			   struct btrfs_root *root, unsigned long count)
{
	struct rb_node *node;
	struct btrfs_delayed_ref_root *delayed_refs;
	struct btrfs_delayed_ref_node *ref;
	struct list_head cluster;
	int ret;
	int run_all = count == (unsigned long)-1;
	int run_most = 0;

	if (root == root->fs_info->extent_root)
		root = root->fs_info->tree_root;

	delayed_refs = &trans->transaction->delayed_refs;
	INIT_LIST_HEAD(&cluster);
again:
	spin_lock(&delayed_refs->lock);
	if (count == 0) {
		count = delayed_refs->num_entries * 2;
		run_most = 1;
	}
	while (1) {
		if (!(run_all || run_most) &&
		    delayed_refs->num_heads_ready < 64)
			break;

		/*
		 * go find something we can process in the rbtree.  We start at
		 * the beginning of the tree, and then build a cluster
		 * of refs to process starting at the first one we are able to
		 * lock
		 */
		ret = btrfs_find_ref_cluster(trans, &cluster,
					     delayed_refs->run_delayed_start);
		if (ret)
			break;

		ret = run_clustered_refs(trans, root, &cluster);
		BUG_ON(ret < 0);

		count -= min_t(unsigned long, ret, count);

		if (count == 0)
			break;
	}

	if (run_all) {
		node = rb_first(&delayed_refs->root);
		if (!node)
			goto out;
		count = (unsigned long)-1;

		while (node) {
			ref = rb_entry(node, struct btrfs_delayed_ref_node,
				       rb_node);
			if (btrfs_delayed_ref_is_head(ref)) {
				struct btrfs_delayed_ref_head *head;

				head = btrfs_delayed_node_to_head(ref);
				atomic_inc(&ref->refs);

				spin_unlock(&delayed_refs->lock);
				mutex_lock(&head->mutex);
				mutex_unlock(&head->mutex);

				btrfs_put_delayed_ref(ref);
				cond_resched();
				goto again;
			}
			node = rb_next(node);
		}
		spin_unlock(&delayed_refs->lock);
		schedule_timeout(1);
		goto again;
	}
out:
	spin_unlock(&delayed_refs->lock);
	return 0;
}

int btrfs_set_disk_extent_flags(struct btrfs_trans_handle *trans,
				struct btrfs_root *root,
				u64 bytenr, u64 num_bytes, u64 flags,
				int is_data)
{
	struct btrfs_delayed_extent_op *extent_op;
	int ret;

	extent_op = kmalloc(sizeof(*extent_op), GFP_NOFS);
	if (!extent_op)
		return -ENOMEM;

	extent_op->flags_to_set = flags;
	extent_op->update_flags = 1;
	extent_op->update_key = 0;
	extent_op->is_data = is_data ? 1 : 0;

	ret = btrfs_add_delayed_extent_op(trans, bytenr, num_bytes, extent_op);
	if (ret)
		kfree(extent_op);
	return ret;
}

static noinline int check_delayed_ref(struct btrfs_trans_handle *trans,
				      struct btrfs_root *root,
				      struct btrfs_path *path,
				      u64 objectid, u64 offset, u64 bytenr)
{
	struct btrfs_delayed_ref_head *head;
	struct btrfs_delayed_ref_node *ref;
	struct btrfs_delayed_data_ref *data_ref;
	struct btrfs_delayed_ref_root *delayed_refs;
	struct rb_node *node;
	int ret = 0;

	ret = -ENOENT;
	delayed_refs = &trans->transaction->delayed_refs;
	spin_lock(&delayed_refs->lock);
	head = btrfs_find_delayed_ref_head(trans, bytenr);
	if (!head)
		goto out;

	if (!mutex_trylock(&head->mutex)) {
		atomic_inc(&head->node.refs);
		spin_unlock(&delayed_refs->lock);

		btrfs_release_path(root->fs_info->extent_root, path);

		mutex_lock(&head->mutex);
		mutex_unlock(&head->mutex);
		btrfs_put_delayed_ref(&head->node);
		return -EAGAIN;
	}

	node = rb_prev(&head->node.rb_node);
	if (!node)
		goto out_unlock;

	ref = rb_entry(node, struct btrfs_delayed_ref_node, rb_node);

	if (ref->bytenr != bytenr)
		goto out_unlock;

	ret = 1;
	if (ref->type != BTRFS_EXTENT_DATA_REF_KEY)
		goto out_unlock;

	data_ref = btrfs_delayed_node_to_data_ref(ref);

	node = rb_prev(node);
	if (node) {
		ref = rb_entry(node, struct btrfs_delayed_ref_node, rb_node);
		if (ref->bytenr == bytenr)
			goto out_unlock;
	}

	if (data_ref->root != root->root_key.objectid ||
	    data_ref->objectid != objectid || data_ref->offset != offset)
		goto out_unlock;

	ret = 0;
out_unlock:
	mutex_unlock(&head->mutex);
out:
	spin_unlock(&delayed_refs->lock);
	return ret;
}

static noinline int check_committed_ref(struct btrfs_trans_handle *trans,
					struct btrfs_root *root,
					struct btrfs_path *path,
					u64 objectid, u64 offset, u64 bytenr)
{
	struct btrfs_root *extent_root = root->fs_info->extent_root;
	struct extent_buffer *leaf;
	struct btrfs_extent_data_ref *ref;
	struct btrfs_extent_inline_ref *iref;
	struct btrfs_extent_item *ei;
	struct btrfs_key key;
	u32 item_size;
	int ret;

	key.objectid = bytenr;
	key.offset = (u64)-1;
	key.type = BTRFS_EXTENT_ITEM_KEY;

	ret = btrfs_search_slot(NULL, extent_root, &key, path, 0, 0);
	if (ret < 0)
		goto out;
	BUG_ON(ret == 0);

	ret = -ENOENT;
	if (path->slots[0] == 0)
		goto out;

	path->slots[0]--;
	leaf = path->nodes[0];
	btrfs_item_key_to_cpu(leaf, &key, path->slots[0]);

	if (key.objectid != bytenr || key.type != BTRFS_EXTENT_ITEM_KEY)
		goto out;

	ret = 1;
	item_size = btrfs_item_size_nr(leaf, path->slots[0]);
#ifdef BTRFS_COMPAT_EXTENT_TREE_V0
	if (item_size < sizeof(*ei)) {
		WARN_ON(item_size != sizeof(struct btrfs_extent_item_v0));
		goto out;
	}
#endif
	ei = btrfs_item_ptr(leaf, path->slots[0], struct btrfs_extent_item);

	if (item_size != sizeof(*ei) +
	    btrfs_extent_inline_ref_size(BTRFS_EXTENT_DATA_REF_KEY))
		goto out;

	if (btrfs_extent_generation(leaf, ei) <=
	    btrfs_root_last_snapshot(&root->root_item))
		goto out;

	iref = (struct btrfs_extent_inline_ref *)(ei + 1);
	if (btrfs_extent_inline_ref_type(leaf, iref) !=
	    BTRFS_EXTENT_DATA_REF_KEY)
		goto out;

	ref = (struct btrfs_extent_data_ref *)(&iref->offset);
	if (btrfs_extent_refs(leaf, ei) !=
	    btrfs_extent_data_ref_count(leaf, ref) ||
	    btrfs_extent_data_ref_root(leaf, ref) !=
	    root->root_key.objectid ||
	    btrfs_extent_data_ref_objectid(leaf, ref) != objectid ||
	    btrfs_extent_data_ref_offset(leaf, ref) != offset)
		goto out;

	ret = 0;
out:
	return ret;
}

int btrfs_cross_ref_exist(struct btrfs_trans_handle *trans,
			  struct btrfs_root *root,
			  u64 objectid, u64 offset, u64 bytenr)
{
	struct btrfs_path *path;
	int ret;
	int ret2;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOENT;

	do {
		ret = check_committed_ref(trans, root, path, objectid,
					  offset, bytenr);
		if (ret && ret != -ENOENT)
			goto out;

		ret2 = check_delayed_ref(trans, root, path, objectid,
					 offset, bytenr);
	} while (ret2 == -EAGAIN);

	if (ret2 && ret2 != -ENOENT) {
		ret = ret2;
		goto out;
	}

	if (ret != -ENOENT || ret2 != -ENOENT)
		ret = 0;
out:
	btrfs_free_path(path);
	return ret;
}

#if 0
int btrfs_cache_ref(struct btrfs_trans_handle *trans, struct btrfs_root *root,
		    struct extent_buffer *buf, u32 nr_extents)
{
	struct btrfs_key key;
	struct btrfs_file_extent_item *fi;
	u64 root_gen;
	u32 nritems;
	int i;
	int level;
	int ret = 0;
	int shared = 0;

	if (!root->ref_cows)
		return 0;

	if (root->root_key.objectid != BTRFS_TREE_RELOC_OBJECTID) {
		shared = 0;
		root_gen = root->root_key.offset;
	} else {
		shared = 1;
		root_gen = trans->transid - 1;
	}

	level = btrfs_header_level(buf);
	nritems = btrfs_header_nritems(buf);

	if (level == 0) {
		struct btrfs_leaf_ref *ref;
		struct btrfs_extent_info *info;

		ref = btrfs_alloc_leaf_ref(root, nr_extents);
		if (!ref) {
			ret = -ENOMEM;
			goto out;
		}

		ref->root_gen = root_gen;
		ref->bytenr = buf->start;
		ref->owner = btrfs_header_owner(buf);
		ref->generation = btrfs_header_generation(buf);
		ref->nritems = nr_extents;
		info = ref->extents;

		for (i = 0; nr_extents > 0 && i < nritems; i++) {
			u64 disk_bytenr;
			btrfs_item_key_to_cpu(buf, &key, i);
			if (btrfs_key_type(&key) != BTRFS_EXTENT_DATA_KEY)
				continue;
			fi = btrfs_item_ptr(buf, i,
					    struct btrfs_file_extent_item);
			if (btrfs_file_extent_type(buf, fi) ==
			    BTRFS_FILE_EXTENT_INLINE)
				continue;
			disk_bytenr = btrfs_file_extent_disk_bytenr(buf, fi);
			if (disk_bytenr == 0)
				continue;

			info->bytenr = disk_bytenr;
			info->num_bytes =
				btrfs_file_extent_disk_num_bytes(buf, fi);
			info->objectid = key.objectid;
			info->offset = key.offset;
			info++;
		}

		ret = btrfs_add_leaf_ref(root, ref, shared);
		if (ret == -EEXIST && shared) {
			struct btrfs_leaf_ref *old;
			old = btrfs_lookup_leaf_ref(root, ref->bytenr);
			BUG_ON(!old);
			btrfs_remove_leaf_ref(root, old);
			btrfs_free_leaf_ref(root, old);
			ret = btrfs_add_leaf_ref(root, ref, shared);
		}
		WARN_ON(ret);
		btrfs_free_leaf_ref(root, ref);
	}
out:
	return ret;
}

/* when a block goes through cow, we update the reference counts of
 * everything that block points to.  The internal pointers of the block
 * can be in just about any order, and it is likely to have clusters of
 * things that are close together and clusters of things that are not.
 *
 * To help reduce the seeks that come with updating all of these reference
 * counts, sort them by byte number before actual updates are done.
 *
 * struct refsort is used to match byte number to slot in the btree block.
 * we sort based on the byte number and then use the slot to actually
 * find the item.
 *
 * struct refsort is smaller than strcut btrfs_item and smaller than
 * struct btrfs_key_ptr.  Since we're currently limited to the page size
 * for a btree block, there's no way for a kmalloc of refsorts for a
 * single node to be bigger than a page.
 */
struct refsort {
	u64 bytenr;
	u32 slot;
};

/*
 * for passing into sort()
 */
static int refsort_cmp(const void *a_void, const void *b_void)
{
	const struct refsort *a = a_void;
	const struct refsort *b = b_void;

	if (a->bytenr < b->bytenr)
		return -1;
	if (a->bytenr > b->bytenr)
		return 1;
	return 0;
}
#endif

static int __btrfs_mod_ref(struct btrfs_trans_handle *trans,
			   struct btrfs_root *root,
			   struct extent_buffer *buf,
			   int full_backref, int inc)
{
	u64 bytenr;
	u64 num_bytes;
	u64 parent;
	u64 ref_root;
	u32 nritems;
	struct btrfs_key key;
	struct btrfs_file_extent_item *fi;
	int i;
	int level;
	int ret = 0;
	int (*process_func)(struct btrfs_trans_handle *, struct btrfs_root *,
			    u64, u64, u64, u64, u64, u64);

	ref_root = btrfs_header_owner(buf);
	nritems = btrfs_header_nritems(buf);
	level = btrfs_header_level(buf);

	if (!root->ref_cows && level == 0)
		return 0;

	if (inc)
		process_func = btrfs_inc_extent_ref;
	else
		process_func = btrfs_free_extent;

	if (full_backref)
		parent = buf->start;
	else
		parent = 0;

	for (i = 0; i < nritems; i++) {
		if (level == 0) {
			btrfs_item_key_to_cpu(buf, &key, i);
			if (btrfs_key_type(&key) != BTRFS_EXTENT_DATA_KEY)
				continue;
			fi = btrfs_item_ptr(buf, i,
					    struct btrfs_file_extent_item);
			if (btrfs_file_extent_type(buf, fi) ==
			    BTRFS_FILE_EXTENT_INLINE)
				continue;
			bytenr = btrfs_file_extent_disk_bytenr(buf, fi);
			if (bytenr == 0)
				continue;

			num_bytes = btrfs_file_extent_disk_num_bytes(buf, fi);
			key.offset -= btrfs_file_extent_offset(buf, fi);
			ret = process_func(trans, root, bytenr, num_bytes,
					   parent, ref_root, key.objectid,
					   key.offset);
			if (ret)
				goto fail;
		} else {
			bytenr = btrfs_node_blockptr(buf, i);
			num_bytes = btrfs_level_size(root, level - 1);
			ret = process_func(trans, root, bytenr, num_bytes,
					   parent, ref_root, level - 1, 0);
			if (ret)
				goto fail;
		}
	}
	return 0;
fail:
	BUG();
	return ret;
}

int btrfs_inc_ref(struct btrfs_trans_handle *trans, struct btrfs_root *root,
		  struct extent_buffer *buf, int full_backref)
{
	return __btrfs_mod_ref(trans, root, buf, full_backref, 1);
}

int btrfs_dec_ref(struct btrfs_trans_handle *trans, struct btrfs_root *root,
		  struct extent_buffer *buf, int full_backref)
{
	return __btrfs_mod_ref(trans, root, buf, full_backref, 0);
}

static int write_one_cache_group(struct btrfs_trans_handle *trans,
				 struct btrfs_root *root,
				 struct btrfs_path *path,
				 struct btrfs_block_group_cache *cache)
{
	int ret;
	struct btrfs_root *extent_root = root->fs_info->extent_root;
	unsigned long bi;
	struct extent_buffer *leaf;

	ret = btrfs_search_slot(trans, extent_root, &cache->key, path, 0, 1);
	if (ret < 0)
		goto fail;
	BUG_ON(ret);

	leaf = path->nodes[0];
	bi = btrfs_item_ptr_offset(leaf, path->slots[0]);
	write_extent_buffer(leaf, &cache->item, bi, sizeof(cache->item));
	btrfs_mark_buffer_dirty(leaf);
	btrfs_release_path(extent_root, path);
fail:
	if (ret)
		return ret;
	return 0;

}

static struct btrfs_block_group_cache *
next_block_group(struct btrfs_root *root,
		 struct btrfs_block_group_cache *cache)
{
	struct rb_node *node;
	spin_lock(&root->fs_info->block_group_cache_lock);
	node = rb_next(&cache->cache_node);
	btrfs_put_block_group(cache);
	if (node) {
		cache = rb_entry(node, struct btrfs_block_group_cache,
				 cache_node);
		atomic_inc(&cache->count);
	} else
		cache = NULL;
	spin_unlock(&root->fs_info->block_group_cache_lock);
	return cache;
}

int btrfs_write_dirty_block_groups(struct btrfs_trans_handle *trans,
				   struct btrfs_root *root)
{
	struct btrfs_block_group_cache *cache;
	int err = 0;
	struct btrfs_path *path;
	u64 last = 0;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	while (1) {
		if (last == 0) {
			err = btrfs_run_delayed_refs(trans, root,
						     (unsigned long)-1);
			BUG_ON(err);
		}

		cache = btrfs_lookup_first_block_group(root->fs_info, last);
		while (cache) {
			if (cache->dirty)
				break;
			cache = next_block_group(root, cache);
		}
		if (!cache) {
			if (last == 0)
				break;
			last = 0;
			continue;
		}

		cache->dirty = 0;
		last = cache->key.objectid + cache->key.offset;

		err = write_one_cache_group(trans, root, path, cache);
		BUG_ON(err);
		btrfs_put_block_group(cache);
	}

	btrfs_free_path(path);
	return 0;
}

int btrfs_extent_readonly(struct btrfs_root *root, u64 bytenr)
{
	struct btrfs_block_group_cache *block_group;
	int readonly = 0;

	block_group = btrfs_lookup_block_group(root->fs_info, bytenr);
	if (!block_group || block_group->ro)
		readonly = 1;
	if (block_group)
		btrfs_put_block_group(block_group);
	return readonly;
}

static int update_space_info(struct btrfs_fs_info *info, u64 flags,
			     u64 total_bytes, u64 bytes_used,
			     struct btrfs_space_info **space_info)
{
	struct btrfs_space_info *found;

	found = __find_space_info(info, flags);
	if (found) {
		spin_lock(&found->lock);
		found->total_bytes += total_bytes;
		found->bytes_used += bytes_used;
		found->full = 0;
		spin_unlock(&found->lock);
		*space_info = found;
		return 0;
	}
	found = kzalloc(sizeof(*found), GFP_NOFS);
	if (!found)
		return -ENOMEM;

	INIT_LIST_HEAD(&found->block_groups);
	init_rwsem(&found->groups_sem);
	spin_lock_init(&found->lock);
	found->flags = flags;
	found->total_bytes = total_bytes;
	found->bytes_used = bytes_used;
	found->bytes_pinned = 0;
	found->bytes_reserved = 0;
	found->bytes_readonly = 0;
	found->bytes_delalloc = 0;
	found->full = 0;
	found->force_alloc = 0;
	*space_info = found;
	list_add_rcu(&found->list, &info->space_info);
	atomic_set(&found->caching_threads, 0);
	return 0;
}

static void set_avail_alloc_bits(struct btrfs_fs_info *fs_info, u64 flags)
{
	u64 extra_flags = flags & (BTRFS_BLOCK_GROUP_RAID0 |
				   BTRFS_BLOCK_GROUP_RAID1 |
				   BTRFS_BLOCK_GROUP_RAID10 |
				   BTRFS_BLOCK_GROUP_DUP);
	if (extra_flags) {
		if (flags & BTRFS_BLOCK_GROUP_DATA)
			fs_info->avail_data_alloc_bits |= extra_flags;
		if (flags & BTRFS_BLOCK_GROUP_METADATA)
			fs_info->avail_metadata_alloc_bits |= extra_flags;
		if (flags & BTRFS_BLOCK_GROUP_SYSTEM)
			fs_info->avail_system_alloc_bits |= extra_flags;
	}
}

static void set_block_group_readonly(struct btrfs_block_group_cache *cache)
{
	spin_lock(&cache->space_info->lock);
	spin_lock(&cache->lock);
	if (!cache->ro) {
		cache->space_info->bytes_readonly += cache->key.offset -
					btrfs_block_group_used(&cache->item);
		cache->ro = 1;
	}
	spin_unlock(&cache->lock);
	spin_unlock(&cache->space_info->lock);
}

u64 btrfs_reduce_alloc_profile(struct btrfs_root *root, u64 flags)
{
	u64 num_devices = root->fs_info->fs_devices->rw_devices;

	if (num_devices == 1)
		flags &= ~(BTRFS_BLOCK_GROUP_RAID1 | BTRFS_BLOCK_GROUP_RAID0);
	if (num_devices < 4)
		flags &= ~BTRFS_BLOCK_GROUP_RAID10;

	if ((flags & BTRFS_BLOCK_GROUP_DUP) &&
	    (flags & (BTRFS_BLOCK_GROUP_RAID1 |
		      BTRFS_BLOCK_GROUP_RAID10))) {
		flags &= ~BTRFS_BLOCK_GROUP_DUP;
	}

	if ((flags & BTRFS_BLOCK_GROUP_RAID1) &&
	    (flags & BTRFS_BLOCK_GROUP_RAID10)) {
		flags &= ~BTRFS_BLOCK_GROUP_RAID1;
	}

	if ((flags & BTRFS_BLOCK_GROUP_RAID0) &&
	    ((flags & BTRFS_BLOCK_GROUP_RAID1) |
	     (flags & BTRFS_BLOCK_GROUP_RAID10) |
	     (flags & BTRFS_BLOCK_GROUP_DUP)))
		flags &= ~BTRFS_BLOCK_GROUP_RAID0;
	return flags;
}

static u64 btrfs_get_alloc_profile(struct btrfs_root *root, u64 data)
{
	struct btrfs_fs_info *info = root->fs_info;
	u64 alloc_profile;

	if (data) {
		alloc_profile = info->avail_data_alloc_bits &
			info->data_alloc_profile;
		data = BTRFS_BLOCK_GROUP_DATA | alloc_profile;
	} else if (root == root->fs_info->chunk_root) {
		alloc_profile = info->avail_system_alloc_bits &
			info->system_alloc_profile;
		data = BTRFS_BLOCK_GROUP_SYSTEM | alloc_profile;
	} else {
		alloc_profile = info->avail_metadata_alloc_bits &
			info->metadata_alloc_profile;
		data = BTRFS_BLOCK_GROUP_METADATA | alloc_profile;
	}

	return btrfs_reduce_alloc_profile(root, data);
}

void btrfs_set_inode_space_info(struct btrfs_root *root, struct inode *inode)
{
	u64 alloc_target;

	alloc_target = btrfs_get_alloc_profile(root, 1);
	BTRFS_I(inode)->space_info = __find_space_info(root->fs_info,
						       alloc_target);
}

static u64 calculate_bytes_needed(struct btrfs_root *root, int num_items)
{
	u64 num_bytes;
	int level;

	level = BTRFS_MAX_LEVEL - 2;
	/*
	 * NOTE: these calculations are absolutely the worst possible case.
	 * This assumes that _every_ item we insert will require a new leaf, and
	 * that the tree has grown to its maximum level size.
	 */

	/*
	 * for every item we insert we could insert both an extent item and a
	 * extent ref item.  Then for ever item we insert, we will need to cow
	 * both the original leaf, plus the leaf to the left and right of it.
	 *
	 * Unless we are talking about the extent root, then we just want the
	 * number of items * 2, since we just need the extent item plus its ref.
	 */
	if (root == root->fs_info->extent_root)
		num_bytes = num_items * 2;
	else
		num_bytes = (num_items + (2 * num_items)) * 3;

	/*
	 * num_bytes is total number of leaves we could need times the leaf
	 * size, and then for every leaf we could end up cow'ing 2 nodes per
	 * level, down to the leaf level.
	 */
	num_bytes = (num_bytes * root->leafsize) +
		(num_bytes * (level * 2)) * root->nodesize;

	return num_bytes;
}

/*
 * Unreserve metadata space for delalloc.  If we have less reserved credits than
 * we have extents, this function does nothing.
 */
int btrfs_unreserve_metadata_for_delalloc(struct btrfs_root *root,
					  struct inode *inode, int num_items)
{
	struct btrfs_fs_info *info = root->fs_info;
	struct btrfs_space_info *meta_sinfo;
	u64 num_bytes;
	u64 alloc_target;
	bool bug = false;

	/* get the space info for where the metadata will live */
	alloc_target = btrfs_get_alloc_profile(root, 0);
	meta_sinfo = __find_space_info(info, alloc_target);

	num_bytes = calculate_bytes_needed(root->fs_info->extent_root,
					   num_items);

	spin_lock(&meta_sinfo->lock);
	spin_lock(&BTRFS_I(inode)->accounting_lock);
	if (BTRFS_I(inode)->reserved_extents <=
	    BTRFS_I(inode)->outstanding_extents) {
		spin_unlock(&BTRFS_I(inode)->accounting_lock);
		spin_unlock(&meta_sinfo->lock);
		return 0;
	}
	spin_unlock(&BTRFS_I(inode)->accounting_lock);

	BTRFS_I(inode)->reserved_extents--;
	BUG_ON(BTRFS_I(inode)->reserved_extents < 0);

	if (meta_sinfo->bytes_delalloc < num_bytes) {
		bug = true;
		meta_sinfo->bytes_delalloc = 0;
	} else {
		meta_sinfo->bytes_delalloc -= num_bytes;
	}
	spin_unlock(&meta_sinfo->lock);

	BUG_ON(bug);

	return 0;
}

static void check_force_delalloc(struct btrfs_space_info *meta_sinfo)
{
	u64 thresh;

	thresh = meta_sinfo->bytes_used + meta_sinfo->bytes_reserved +
		meta_sinfo->bytes_pinned + meta_sinfo->bytes_readonly +
		meta_sinfo->bytes_super + meta_sinfo->bytes_root +
		meta_sinfo->bytes_may_use;

	thresh = meta_sinfo->total_bytes - thresh;
	thresh *= 80;
	do_div(thresh, 100);
	if (thresh <= meta_sinfo->bytes_delalloc)
		meta_sinfo->force_delalloc = 1;
	else
		meta_sinfo->force_delalloc = 0;
}

struct async_flush {
	struct btrfs_root *root;
	struct btrfs_space_info *info;
	struct btrfs_work work;
};

static noinline void flush_delalloc_async(struct btrfs_work *work)
{
	struct async_flush *async;
	struct btrfs_root *root;
	struct btrfs_space_info *info;

	async = container_of(work, struct async_flush, work);
	root = async->root;
	info = async->info;

	btrfs_start_delalloc_inodes(root);
	wake_up(&info->flush_wait);
	btrfs_wait_ordered_extents(root, 0);

	spin_lock(&info->lock);
	info->flushing = 0;
	spin_unlock(&info->lock);
	wake_up(&info->flush_wait);

	kfree(async);
}

static void wait_on_flush(struct btrfs_space_info *info)
{
	DEFINE_WAIT(wait);
	u64 used;

	while (1) {
		prepare_to_wait(&info->flush_wait, &wait,
				TASK_UNINTERRUPTIBLE);
		spin_lock(&info->lock);
		if (!info->flushing) {
			spin_unlock(&info->lock);
			break;
		}

		used = info->bytes_used + info->bytes_reserved +
			info->bytes_pinned + info->bytes_readonly +
			info->bytes_super + info->bytes_root +
			info->bytes_may_use + info->bytes_delalloc;
		if (used < info->total_bytes) {
			spin_unlock(&info->lock);
			break;
		}
		spin_unlock(&info->lock);
		schedule();
	}
	finish_wait(&info->flush_wait, &wait);
}

static void flush_delalloc(struct btrfs_root *root,
				 struct btrfs_space_info *info)
{
	struct async_flush *async;
	bool wait = false;

	spin_lock(&info->lock);

	if (!info->flushing) {
		info->flushing = 1;
		init_waitqueue_head(&info->flush_wait);
	} else {
		wait = true;
	}

	spin_unlock(&info->lock);

	if (wait) {
		wait_on_flush(info);
		return;
	}

	async = kzalloc(sizeof(*async), GFP_NOFS);
	if (!async)
		goto flush;

	async->root = root;
	async->info = info;
	async->work.func = flush_delalloc_async;

	btrfs_queue_worker(&root->fs_info->enospc_workers,
			   &async->work);
	wait_on_flush(info);
	return;

flush:
	btrfs_start_delalloc_inodes(root);
	btrfs_wait_ordered_extents(root, 0);

	spin_lock(&info->lock);
	info->flushing = 0;
	spin_unlock(&info->lock);
	wake_up(&info->flush_wait);
}

static int maybe_allocate_chunk(struct btrfs_root *root,
				 struct btrfs_space_info *info)
{
	struct btrfs_super_block *disk_super = &root->fs_info->super_copy;
	struct btrfs_trans_handle *trans;
	bool wait = false;
	int ret = 0;
	u64 min_metadata;
	u64 free_space;

	free_space = btrfs_super_total_bytes(disk_super);
	/*
	 * we allow the metadata to grow to a max of either 10gb or 5% of the
	 * space in the volume.
	 */
	min_metadata = min((u64)10 * 1024 * 1024 * 1024,
			     div64_u64(free_space * 5, 100));
	if (info->total_bytes >= min_metadata) {
		spin_unlock(&info->lock);
		return 0;
	}

	if (info->full) {
		spin_unlock(&info->lock);
		return 0;
	}

	if (!info->allocating_chunk) {
		info->force_alloc = 1;
		info->allocating_chunk = 1;
		init_waitqueue_head(&info->allocate_wait);
	} else {
		wait = true;
	}

	spin_unlock(&info->lock);

	if (wait) {
		wait_event(info->allocate_wait,
			   !info->allocating_chunk);
		return 1;
	}

	trans = btrfs_start_transaction(root, 1);
	if (!trans) {
		ret = -ENOMEM;
		goto out;
	}

	ret = do_chunk_alloc(trans, root->fs_info->extent_root,
			     4096 + 2 * 1024 * 1024,
			     info->flags, 0);
	btrfs_end_transaction(trans, root);
	if (ret)
		goto out;
out:
	spin_lock(&info->lock);
	info->allocating_chunk = 0;
	spin_unlock(&info->lock);
	wake_up(&info->allocate_wait);

	if (ret)
		return 0;
	return 1;
}

/*
 * Reserve metadata space for delalloc.
 */
int btrfs_reserve_metadata_for_delalloc(struct btrfs_root *root,
					struct inode *inode, int num_items)
{
	struct btrfs_fs_info *info = root->fs_info;
	struct btrfs_space_info *meta_sinfo;
	u64 num_bytes;
	u64 used;
	u64 alloc_target;
	int flushed = 0;
	int force_delalloc;

	/* get the space info for where the metadata will live */
	alloc_target = btrfs_get_alloc_profile(root, 0);
	meta_sinfo = __find_space_info(info, alloc_target);

	num_bytes = calculate_bytes_needed(root->fs_info->extent_root,
					   num_items);
again:
	spin_lock(&meta_sinfo->lock);

	force_delalloc = meta_sinfo->force_delalloc;

	if (unlikely(!meta_sinfo->bytes_root))
		meta_sinfo->bytes_root = calculate_bytes_needed(root, 6);

	if (!flushed)
		meta_sinfo->bytes_delalloc += num_bytes;

	used = meta_sinfo->bytes_used + meta_sinfo->bytes_reserved +
		meta_sinfo->bytes_pinned + meta_sinfo->bytes_readonly +
		meta_sinfo->bytes_super + meta_sinfo->bytes_root +
		meta_sinfo->bytes_may_use + meta_sinfo->bytes_delalloc;

	if (used > meta_sinfo->total_bytes) {
		flushed++;

		if (flushed == 1) {
			if (maybe_allocate_chunk(root, meta_sinfo))
				goto again;
			flushed++;
		} else {
			spin_unlock(&meta_sinfo->lock);
		}

		if (flushed == 2) {
			filemap_flush(inode->i_mapping);
			goto again;
		} else if (flushed == 3) {
			flush_delalloc(root, meta_sinfo);
			goto again;
		}
		spin_lock(&meta_sinfo->lock);
		meta_sinfo->bytes_delalloc -= num_bytes;
		spin_unlock(&meta_sinfo->lock);
		printk(KERN_ERR "enospc, has %d, reserved %d\n",
		       BTRFS_I(inode)->outstanding_extents,
		       BTRFS_I(inode)->reserved_extents);
		dump_space_info(meta_sinfo, 0, 0);
		return -ENOSPC;
	}

	BTRFS_I(inode)->reserved_extents++;
	check_force_delalloc(meta_sinfo);
	spin_unlock(&meta_sinfo->lock);

	if (!flushed && force_delalloc)
		filemap_flush(inode->i_mapping);

	return 0;
}

/*
 * unreserve num_items number of items worth of metadata space.  This needs to
 * be paired with btrfs_reserve_metadata_space.
 *
 * NOTE: if you have the option, run this _AFTER_ you do a
 * btrfs_end_transaction, since btrfs_end_transaction will run delayed ref
 * oprations which will result in more used metadata, so we want to make sure we
 * can do that without issue.
 */
int btrfs_unreserve_metadata_space(struct btrfs_root *root, int num_items)
{
	struct btrfs_fs_info *info = root->fs_info;
	struct btrfs_space_info *meta_sinfo;
	u64 num_bytes;
	u64 alloc_target;
	bool bug = false;

	/* get the space info for where the metadata will live */
	alloc_target = btrfs_get_alloc_profile(root, 0);
	meta_sinfo = __find_space_info(info, alloc_target);

	num_bytes = calculate_bytes_needed(root, num_items);

	spin_lock(&meta_sinfo->lock);
	if (meta_sinfo->bytes_may_use < num_bytes) {
		bug = true;
		meta_sinfo->bytes_may_use = 0;
	} else {
		meta_sinfo->bytes_may_use -= num_bytes;
	}
	spin_unlock(&meta_sinfo->lock);

	BUG_ON(bug);

	return 0;
}

/*
 * Reserve some metadata space for use.  We'll calculate the worste case number
 * of bytes that would be needed to modify num_items number of items.  If we
 * have space, fantastic, if not, you get -ENOSPC.  Please call
 * btrfs_unreserve_metadata_space when you are done for the _SAME_ number of
 * items you reserved, since whatever metadata you needed should have already
 * been allocated.
 *
 * This will commit the transaction to make more space if we don't have enough
 * metadata space.  THe only time we don't do this is if we're reserving space
 * inside of a transaction, then we will just return -ENOSPC and it is the
 * callers responsibility to handle it properly.
 */
int btrfs_reserve_metadata_space(struct btrfs_root *root, int num_items)
{
	struct btrfs_fs_info *info = root->fs_info;
	struct btrfs_space_info *meta_sinfo;
	u64 num_bytes;
	u64 used;
	u64 alloc_target;
	int retries = 0;

	/* get the space info for where the metadata will live */
	alloc_target = btrfs_get_alloc_profile(root, 0);
	meta_sinfo = __find_space_info(info, alloc_target);

	num_bytes = calculate_bytes_needed(root, num_items);
again:
	spin_lock(&meta_sinfo->lock);

	if (unlikely(!meta_sinfo->bytes_root))
		meta_sinfo->bytes_root = calculate_bytes_needed(root, 6);

	if (!retries)
		meta_sinfo->bytes_may_use += num_bytes;

	used = meta_sinfo->bytes_used + meta_sinfo->bytes_reserved +
		meta_sinfo->bytes_pinned + meta_sinfo->bytes_readonly +
		meta_sinfo->bytes_super + meta_sinfo->bytes_root +
		meta_sinfo->bytes_may_use + meta_sinfo->bytes_delalloc;

	if (used > meta_sinfo->total_bytes) {
		retries++;
		if (retries == 1) {
			if (maybe_allocate_chunk(root, meta_sinfo))
				goto again;
			retries++;
		} else {
			spin_unlock(&meta_sinfo->lock);
		}

		if (retries == 2) {
			flush_delalloc(root, meta_sinfo);
			goto again;
		}
		spin_lock(&meta_sinfo->lock);
		meta_sinfo->bytes_may_use -= num_bytes;
		spin_unlock(&meta_sinfo->lock);

		dump_space_info(meta_sinfo, 0, 0);
		return -ENOSPC;
	}

	check_force_delalloc(meta_sinfo);
	spin_unlock(&meta_sinfo->lock);

	return 0;
}

/*
 * This will check the space that the inode allocates from to make sure we have
 * enough space for bytes.
 */
int btrfs_check_data_free_space(struct btrfs_root *root, struct inode *inode,
				u64 bytes)
{
	struct btrfs_space_info *data_sinfo;
	int ret = 0, committed = 0;

	/* make sure bytes are sectorsize aligned */
	bytes = (bytes + root->sectorsize - 1) & ~((u64)root->sectorsize - 1);

	data_sinfo = BTRFS_I(inode)->space_info;
	if (!data_sinfo)
		goto alloc;

again:
	/* make sure we have enough space to handle the data first */
	spin_lock(&data_sinfo->lock);
	if (data_sinfo->total_bytes - data_sinfo->bytes_used -
	    data_sinfo->bytes_delalloc - data_sinfo->bytes_reserved -
	    data_sinfo->bytes_pinned - data_sinfo->bytes_readonly -
	    data_sinfo->bytes_may_use - data_sinfo->bytes_super < bytes) {
		struct btrfs_trans_handle *trans;

		/*
		 * if we don't have enough free bytes in this space then we need
		 * to alloc a new chunk.
		 */
		if (!data_sinfo->full) {
			u64 alloc_target;

			data_sinfo->force_alloc = 1;
			spin_unlock(&data_sinfo->lock);
alloc:
			alloc_target = btrfs_get_alloc_profile(root, 1);
			trans = btrfs_start_transaction(root, 1);
			if (!trans)
				return -ENOMEM;

			ret = do_chunk_alloc(trans, root->fs_info->extent_root,
					     bytes + 2 * 1024 * 1024,
					     alloc_target, 0);
			btrfs_end_transaction(trans, root);
			if (ret)
				return ret;

			if (!data_sinfo) {
				btrfs_set_inode_space_info(root, inode);
				data_sinfo = BTRFS_I(inode)->space_info;
			}
			goto again;
		}
		spin_unlock(&data_sinfo->lock);

		/* commit the current transaction and try again */
		if (!committed && !root->fs_info->open_ioctl_trans) {
			committed = 1;
			trans = btrfs_join_transaction(root, 1);
			if (!trans)
				return -ENOMEM;
			ret = btrfs_commit_transaction(trans, root);
			if (ret)
				return ret;
			goto again;
		}

		printk(KERN_ERR "no space left, need %llu, %llu delalloc bytes"
		       ", %llu bytes_used, %llu bytes_reserved, "
		       "%llu bytes_pinned, %llu bytes_readonly, %llu may use "
		       "%llu total\n", (unsigned long long)bytes,
		       (unsigned long long)data_sinfo->bytes_delalloc,
		       (unsigned long long)data_sinfo->bytes_used,
		       (unsigned long long)data_sinfo->bytes_reserved,
		       (unsigned long long)data_sinfo->bytes_pinned,
		       (unsigned long long)data_sinfo->bytes_readonly,
		       (unsigned long long)data_sinfo->bytes_may_use,
		       (unsigned long long)data_sinfo->total_bytes);
		return -ENOSPC;
	}
	data_sinfo->bytes_may_use += bytes;
	BTRFS_I(inode)->reserved_bytes += bytes;
	spin_unlock(&data_sinfo->lock);

	return 0;
}

/*
 * if there was an error for whatever reason after calling
 * btrfs_check_data_free_space, call this so we can cleanup the counters.
 */
void btrfs_free_reserved_data_space(struct btrfs_root *root,
				    struct inode *inode, u64 bytes)
{
	struct btrfs_space_info *data_sinfo;

	/* make sure bytes are sectorsize aligned */
	bytes = (bytes + root->sectorsize - 1) & ~((u64)root->sectorsize - 1);

	data_sinfo = BTRFS_I(inode)->space_info;
	spin_lock(&data_sinfo->lock);
	data_sinfo->bytes_may_use -= bytes;
	BTRFS_I(inode)->reserved_bytes -= bytes;
	spin_unlock(&data_sinfo->lock);
}

/* called when we are adding a delalloc extent to the inode's io_tree */
void btrfs_delalloc_reserve_space(struct btrfs_root *root, struct inode *inode,
				  u64 bytes)
{
	struct btrfs_space_info *data_sinfo;

	/* get the space info for where this inode will be storing its data */
	data_sinfo = BTRFS_I(inode)->space_info;

	/* make sure we have enough space to handle the data first */
	spin_lock(&data_sinfo->lock);
	data_sinfo->bytes_delalloc += bytes;

	/*
	 * we are adding a delalloc extent without calling
	 * btrfs_check_data_free_space first.  This happens on a weird
	 * writepage condition, but shouldn't hurt our accounting
	 */
	if (unlikely(bytes > BTRFS_I(inode)->reserved_bytes)) {
		data_sinfo->bytes_may_use -= BTRFS_I(inode)->reserved_bytes;
		BTRFS_I(inode)->reserved_bytes = 0;
	} else {
		data_sinfo->bytes_may_use -= bytes;
		BTRFS_I(inode)->reserved_bytes -= bytes;
	}

	spin_unlock(&data_sinfo->lock);
}

/* called when we are clearing an delalloc extent from the inode's io_tree */
void btrfs_delalloc_free_space(struct btrfs_root *root, struct inode *inode,
			      u64 bytes)
{
	struct btrfs_space_info *info;

	info = BTRFS_I(inode)->space_info;

	spin_lock(&info->lock);
	info->bytes_delalloc -= bytes;
	spin_unlock(&info->lock);
}

static void force_metadata_allocation(struct btrfs_fs_info *info)
{
	struct list_head *head = &info->space_info;
	struct btrfs_space_info *found;

	rcu_read_lock();
	list_for_each_entry_rcu(found, head, list) {
		if (found->flags & BTRFS_BLOCK_GROUP_METADATA)
			found->force_alloc = 1;
	}
	rcu_read_unlock();
}

static int do_chunk_alloc(struct btrfs_trans_handle *trans,
			  struct btrfs_root *extent_root, u64 alloc_bytes,
			  u64 flags, int force)
{
	struct btrfs_space_info *space_info;
	struct btrfs_fs_info *fs_info = extent_root->fs_info;
	u64 thresh;
	int ret = 0;

	mutex_lock(&fs_info->chunk_mutex);

	flags = btrfs_reduce_alloc_profile(extent_root, flags);

	space_info = __find_space_info(extent_root->fs_info, flags);
	if (!space_info) {
		ret = update_space_info(extent_root->fs_info, flags,
					0, 0, &space_info);
		BUG_ON(ret);
	}
	BUG_ON(!space_info);

	spin_lock(&space_info->lock);
	if (space_info->force_alloc)
		force = 1;
	if (space_info->full) {
		spin_unlock(&space_info->lock);
		goto out;
	}

	thresh = space_info->total_bytes - space_info->bytes_readonly;
	thresh = div_factor(thresh, 8);
	if (!force &&
	   (space_info->bytes_used + space_info->bytes_pinned +
	    space_info->bytes_reserved + alloc_bytes) < thresh) {
		spin_unlock(&space_info->lock);
		goto out;
	}
	spin_unlock(&space_info->lock);

	/*
	 * if we're doing a data chunk, go ahead and make sure that
	 * we keep a reasonable number of metadata chunks allocated in the
	 * FS as well.
	 */
	if (flags & BTRFS_BLOCK_GROUP_DATA && fs_info->metadata_ratio) {
		fs_info->data_chunk_allocations++;
		if (!(fs_info->data_chunk_allocations %
		      fs_info->metadata_ratio))
			force_metadata_allocation(fs_info);
	}

	ret = btrfs_alloc_chunk(trans, extent_root, flags);
	spin_lock(&space_info->lock);
	if (ret)
		space_info->full = 1;
	space_info->force_alloc = 0;
	spin_unlock(&space_info->lock);
out:
	mutex_unlock(&extent_root->fs_info->chunk_mutex);
	return ret;
}

static int update_block_group(struct btrfs_trans_handle *trans,
			      struct btrfs_root *root,
			      u64 bytenr, u64 num_bytes, int alloc,
			      int mark_free)
{
	struct btrfs_block_group_cache *cache;
	struct btrfs_fs_info *info = root->fs_info;
	u64 total = num_bytes;
	u64 old_val;
	u64 byte_in_group;

	/* block accounting for super block */
	spin_lock(&info->delalloc_lock);
	old_val = btrfs_super_bytes_used(&info->super_copy);
	if (alloc)
		old_val += num_bytes;
	else
		old_val -= num_bytes;
	btrfs_set_super_bytes_used(&info->super_copy, old_val);

	/* block accounting for root item */
	old_val = btrfs_root_used(&root->root_item);
	if (alloc)
		old_val += num_bytes;
	else
		old_val -= num_bytes;
	btrfs_set_root_used(&root->root_item, old_val);
	spin_unlock(&info->delalloc_lock);

	while (total) {
		cache = btrfs_lookup_block_group(info, bytenr);
		if (!cache)
			return -1;
		byte_in_group = bytenr - cache->key.objectid;
		WARN_ON(byte_in_group > cache->key.offset);

		spin_lock(&cache->space_info->lock);
		spin_lock(&cache->lock);
		cache->dirty = 1;
		old_val = btrfs_block_group_used(&cache->item);
		num_bytes = min(total, cache->key.offset - byte_in_group);
		if (alloc) {
			old_val += num_bytes;
			btrfs_set_block_group_used(&cache->item, old_val);
			cache->reserved -= num_bytes;
			cache->space_info->bytes_used += num_bytes;
			cache->space_info->bytes_reserved -= num_bytes;
			if (cache->ro)
				cache->space_info->bytes_readonly -= num_bytes;
			spin_unlock(&cache->lock);
			spin_unlock(&cache->space_info->lock);
		} else {
			old_val -= num_bytes;
			cache->space_info->bytes_used -= num_bytes;
			if (cache->ro)
				cache->space_info->bytes_readonly += num_bytes;
			btrfs_set_block_group_used(&cache->item, old_val);
			spin_unlock(&cache->lock);
			spin_unlock(&cache->space_info->lock);
			if (mark_free) {
				int ret;

				ret = btrfs_discard_extent(root, bytenr,
							   num_bytes);
				WARN_ON(ret);

				ret = btrfs_add_free_space(cache, bytenr,
							   num_bytes);
				WARN_ON(ret);
			}
		}
		btrfs_put_block_group(cache);
		total -= num_bytes;
		bytenr += num_bytes;
	}
	return 0;
}

static u64 first_logical_byte(struct btrfs_root *root, u64 search_start)
{
	struct btrfs_block_group_cache *cache;
	u64 bytenr;

	cache = btrfs_lookup_first_block_group(root->fs_info, search_start);
	if (!cache)
		return 0;

	bytenr = cache->key.objectid;
	btrfs_put_block_group(cache);

	return bytenr;
}

/*
 * this function must be called within transaction
 */
int btrfs_pin_extent(struct btrfs_root *root,
		     u64 bytenr, u64 num_bytes, int reserved)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_block_group_cache *cache;

	cache = btrfs_lookup_block_group(fs_info, bytenr);
	BUG_ON(!cache);

	spin_lock(&cache->space_info->lock);
	spin_lock(&cache->lock);
	cache->pinned += num_bytes;
	cache->space_info->bytes_pinned += num_bytes;
	if (reserved) {
		cache->reserved -= num_bytes;
		cache->space_info->bytes_reserved -= num_bytes;
	}
	spin_unlock(&cache->lock);
	spin_unlock(&cache->space_info->lock);

	btrfs_put_block_group(cache);

	set_extent_dirty(fs_info->pinned_extents,
			 bytenr, bytenr + num_bytes - 1, GFP_NOFS);
	return 0;
}

static int update_reserved_extents(struct btrfs_block_group_cache *cache,
				   u64 num_bytes, int reserve)
{
	spin_lock(&cache->space_info->lock);
	spin_lock(&cache->lock);
	if (reserve) {
		cache->reserved += num_bytes;
		cache->space_info->bytes_reserved += num_bytes;
	} else {
		cache->reserved -= num_bytes;
		cache->space_info->bytes_reserved -= num_bytes;
	}
	spin_unlock(&cache->lock);
	spin_unlock(&cache->space_info->lock);
	return 0;
}

int btrfs_prepare_extent_commit(struct btrfs_trans_handle *trans,
				struct btrfs_root *root)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_caching_control *next;
	struct btrfs_caching_control *caching_ctl;
	struct btrfs_block_group_cache *cache;

	down_write(&fs_info->extent_commit_sem);

	list_for_each_entry_safe(caching_ctl, next,
				 &fs_info->caching_block_groups, list) {
		cache = caching_ctl->block_group;
		if (block_group_cache_done(cache)) {
			cache->last_byte_to_unpin = (u64)-1;
			list_del_init(&caching_ctl->list);
			put_caching_control(caching_ctl);
		} else {
			cache->last_byte_to_unpin = caching_ctl->progress;
		}
	}

	if (fs_info->pinned_extents == &fs_info->freed_extents[0])
		fs_info->pinned_extents = &fs_info->freed_extents[1];
	else
		fs_info->pinned_extents = &fs_info->freed_extents[0];

	up_write(&fs_info->extent_commit_sem);
	return 0;
}

static int unpin_extent_range(struct btrfs_root *root, u64 start, u64 end)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_block_group_cache *cache = NULL;
	u64 len;

	while (start <= end) {
		if (!cache ||
		    start >= cache->key.objectid + cache->key.offset) {
			if (cache)
				btrfs_put_block_group(cache);
			cache = btrfs_lookup_block_group(fs_info, start);
			BUG_ON(!cache);
		}

		len = cache->key.objectid + cache->key.offset - start;
		len = min(len, end + 1 - start);

		if (start < cache->last_byte_to_unpin) {
			len = min(len, cache->last_byte_to_unpin - start);
			btrfs_add_free_space(cache, start, len);
		}

		spin_lock(&cache->space_info->lock);
		spin_lock(&cache->lock);
		cache->pinned -= len;
		cache->space_info->bytes_pinned -= len;
		spin_unlock(&cache->lock);
		spin_unlock(&cache->space_info->lock);

		start += len;
	}

	if (cache)
		btrfs_put_block_group(cache);
	return 0;
}

int btrfs_finish_extent_commit(struct btrfs_trans_handle *trans,
			       struct btrfs_root *root)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct extent_io_tree *unpin;
	u64 start;
	u64 end;
	int ret;

	if (fs_info->pinned_extents == &fs_info->freed_extents[0])
		unpin = &fs_info->freed_extents[1];
	else
		unpin = &fs_info->freed_extents[0];

	while (1) {
		ret = find_first_extent_bit(unpin, 0, &start, &end,
					    EXTENT_DIRTY);
		if (ret)
			break;

		ret = btrfs_discard_extent(root, start, end + 1 - start);

		clear_extent_dirty(unpin, start, end, GFP_NOFS);
		unpin_extent_range(root, start, end);
		cond_resched();
	}

	return ret;
}

static int pin_down_bytes(struct btrfs_trans_handle *trans,
			  struct btrfs_root *root,
			  struct btrfs_path *path,
			  u64 bytenr, u64 num_bytes,
			  int is_data, int reserved,
			  struct extent_buffer **must_clean)
{
	int err = 0;
	struct extent_buffer *buf;

	if (is_data)
		goto pinit;

	/*
	 * discard is sloooow, and so triggering discards on
	 * individual btree blocks isn't a good plan.  Just
	 * pin everything in discard mode.
	 */
	if (btrfs_test_opt(root, DISCARD))
		goto pinit;

	buf = btrfs_find_tree_block(root, bytenr, num_bytes);
	if (!buf)
		goto pinit;

	/* we can reuse a block if it hasn't been written
	 * and it is from this transaction.  We can't
	 * reuse anything from the tree log root because
	 * it has tiny sub-transactions.
	 */
	if (btrfs_buffer_uptodate(buf, 0) &&
	    btrfs_try_tree_lock(buf)) {
		u64 header_owner = btrfs_header_owner(buf);
		u64 header_transid = btrfs_header_generation(buf);
		if (header_owner != BTRFS_TREE_LOG_OBJECTID &&
		    header_transid == trans->transid &&
		    !btrfs_header_flag(buf, BTRFS_HEADER_FLAG_WRITTEN)) {
			*must_clean = buf;
			return 1;
		}
		btrfs_tree_unlock(buf);
	}
	free_extent_buffer(buf);
pinit:
	if (path)
		btrfs_set_path_blocking(path);
	/* unlocks the pinned mutex */
	btrfs_pin_extent(root, bytenr, num_bytes, reserved);

	BUG_ON(err < 0);
	return 0;
}

static int __btrfs_free_extent(struct btrfs_trans_handle *trans,
				struct btrfs_root *root,
				u64 bytenr, u64 num_bytes, u64 parent,
				u64 root_objectid, u64 owner_objectid,
				u64 owner_offset, int refs_to_drop,
				struct btrfs_delayed_extent_op *extent_op)
{
	struct btrfs_key key;
	struct btrfs_path *path;
	struct btrfs_fs_info *info = root->fs_info;
	struct btrfs_root *extent_root = info->extent_root;
	struct extent_buffer *leaf;
	struct btrfs_extent_item *ei;
	struct btrfs_extent_inline_ref *iref;
	int ret;
	int is_data;
	int extent_slot = 0;
	int found_extent = 0;
	int num_to_del = 1;
	u32 item_size;
	u64 refs;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	path->reada = 1;
	path->leave_spinning = 1;

	is_data = owner_objectid >= BTRFS_FIRST_FREE_OBJECTID;
	BUG_ON(!is_data && refs_to_drop != 1);

	ret = lookup_extent_backref(trans, extent_root, path, &iref,
				    bytenr, num_bytes, parent,
				    root_objectid, owner_objectid,
				    owner_offset);
	if (ret == 0) {
		extent_slot = path->slots[0];
		while (extent_slot >= 0) {
			btrfs_item_key_to_cpu(path->nodes[0], &key,
					      extent_slot);
			if (key.objectid != bytenr)
				break;
			if (key.type == BTRFS_EXTENT_ITEM_KEY &&
			    key.offset == num_bytes) {
				found_extent = 1;
				break;
			}
			if (path->slots[0] - extent_slot > 5)
				break;
			extent_slot--;
		}
#ifdef BTRFS_COMPAT_EXTENT_TREE_V0
		item_size = btrfs_item_size_nr(path->nodes[0], extent_slot);
		if (found_extent && item_size < sizeof(*ei))
			found_extent = 0;
#endif
		if (!found_extent) {
			BUG_ON(iref);
			ret = remove_extent_backref(trans, extent_root, path,
						    NULL, refs_to_drop,
						    is_data);
			BUG_ON(ret);
			btrfs_release_path(extent_root, path);
			path->leave_spinning = 1;

			key.objectid = bytenr;
			key.type = BTRFS_EXTENT_ITEM_KEY;
			key.offset = num_bytes;

			ret = btrfs_search_slot(trans, extent_root,
						&key, path, -1, 1);
			if (ret) {
				printk(KERN_ERR "umm, got %d back from search"
				       ", was looking for %llu\n", ret,
				       (unsigned long long)bytenr);
				btrfs_print_leaf(extent_root, path->nodes[0]);
			}
			BUG_ON(ret);
			extent_slot = path->slots[0];
		}
	} else {
		btrfs_print_leaf(extent_root, path->nodes[0]);
		WARN_ON(1);
		printk(KERN_ERR "btrfs unable to find ref byte nr %llu "
		       "parent %llu root %llu  owner %llu offset %llu\n",
		       (unsigned long long)bytenr,
		       (unsigned long long)parent,
		       (unsigned long long)root_objectid,
		       (unsigned long long)owner_objectid,
		       (unsigned long long)owner_offset);
	}

	leaf = path->nodes[0];
	item_size = btrfs_item_size_nr(leaf, extent_slot);
#ifdef BTRFS_COMPAT_EXTENT_TREE_V0
	if (item_size < sizeof(*ei)) {
		BUG_ON(found_extent || extent_slot != path->slots[0]);
		ret = convert_extent_item_v0(trans, extent_root, path,
					     owner_objectid, 0);
		BUG_ON(ret < 0);

		btrfs_release_path(extent_root, path);
		path->leave_spinning = 1;

		key.objectid = bytenr;
		key.type = BTRFS_EXTENT_ITEM_KEY;
		key.offset = num_bytes;

		ret = btrfs_search_slot(trans, extent_root, &key, path,
					-1, 1);
		if (ret) {
			printk(KERN_ERR "umm, got %d back from search"
			 racle.", was looking for %llu\n", ret,07 Oracle. (unsigned longtware)bytenr);07 Obtrfs_print_leaf(extent_root, path->nodes[0]distr}
		BUG_ON(retdistr modifyslot =nder thcensrms istror
 e v2 as pe terms istritem_size = bute iion.
 *
 _nr(or
 , c
 * Licensdist}
#endif
NU Generion.
 *
 *<  *
 of(*ei)distei* This programptstributed in the hos programstructThis pr modifyion.distif (owner_objectid < BTRFS_FIRST_FREE_OBJECTID) {
		* MERCHANTABItree_block_info *biistrl be useful,
 * but WITHOUT AN +t WITHOUTbANY WA	bRANT(ublic License for more details)(ei + 1distrWARNener A PARTICULAR P! This pr for more dleveltributeGNU Gen}

	refs* This pr modify efstributedidistU Generalfs < USA._to_dropdistUSA.
-=*/
#include <;
SS FORUSA.
> 0eral P FOR modifyop)07 O__run_delayed330,
 * ope <linux/w, n, MA 021110	/*
		 * In the case of inline* Copyref* Thference count willort.h>be updated by remove330,
 *  Coprefort.h/includeireferal P0-1307, !founude <linof the else"
#incbute iset330,
 * Boston, MA 02inclusdistribute imark_buffer_dirtytribuof the GN FORsk-io.h"
#inc"
#increse v.h"
#include "hash.h"(transted in th it under s pro7 Oraclree.includnclude <dle *trans,
	s_datadistriU General Publi}
e "print-treint "
#infre * T0inux* MERCHclude "hlude  *must_clean = NULL<line "free-space-cache.h"

l be uset,
			 &&*/
#include < !=07 Oracle.  modify
			 Bos_ux/kt(_trans_hand
			 U Genee "ctree.h"
#inc_group_c2 as publishedFreed in the hope tde "print-trees_handle *trans,
				struct btrfs_rooogram; if		 *trans,
				stuct btrfs_rooroot_onumncludel = 2 *rootu64 n


staticpin_down_can sup(struc __btrfs_frecan redle *tracle64 oct bt,oot,
			, 0, &t update_rdistrux/pagtap.h>oot_loc,
			    1 *
 * You sfs_d< 0linux/sort.h>it is go restoinclvery rareservesomeone,
				 waitingort.h>o#inclumore  we'ct breeing.  delr FITs might need toort.h>schedule, so rather than get fancy, justservce itort.h>
			ore  reshere
#include "ctxtent_op(sttree.h"
#incluore drent, u6				      u6_extestaticbute it(struct up(struct btrfs_trans_handv2 as publishedra_op);
staatic wner_odistrU General Publibute irelease_der * modify it under     std,
				      u6"
#incate_rware Founda(rvedlayed_exxtent_op(structs,
				 for un
				gs, struct btrfsfilenclude "hlude 			     int levefs_to "ctrt,
			 e.h"

statictrfs_key *csu, int ref_ it unp *extratic void       u64 bytenr, u64 "print-treeinvalie "c_mapping_pages(etai->b for ie te->igs, int s program can re >> PAGE_CACHE_SHIFTs program (s_trans+atic void  - 1)s_handle *trans,
			tic int dostaticude "c more dgroup*trans,
			  struct btrfs_root ayedle *trlloc,
			 _trans_handle *tran}
s,
				, struct bot,
			64 burn Thi;
}

/*
 * when wd_file anct btrf,buffer possible (and likely)ructt, int levnclulastth *
#incluncluservevoid  modif as well.  Thist (C) 2esace_ict btrfs_spa forservth *a givel,
			 strey);iface_re uct no o  strct btrfs_sps,
				 processedtructh *.h"
#isbuffrightnclu for.
nclustatic nox/rcupdt alcheck
statite_rupublic
 * Licenseans_handle *p(struee_block(st* MERCHANTABI it  * it unu64fs_tran)
{
Public License
#includstathead *
/*
;->flags & bits) == bits;
}
64 bit == bits;
}ss adds the block group to the te *refs adds therbe
 */
se teESS ntct btr
	nfo rb tree  = &its(s->its(saction->nfo rb tree for 
			
				&nfo rb tree ->ore t_ke
/*
 rfs_tranfind) == bits;
}

/*
up(struccan redist FOR!
/*
64 fgoto ouhe(st
 */
=rfs_prev(&
/*
ftware.fs_add_truct btrock_gock_group_cache_spahe;

entry(e te,ache *cache, uoup
 * cache
 */,rfs_add_    s/*cache_done(still roupiesace_infish>
#inwluden't  u64 itncludux/page->s_trans==urn (cachck_group_cache FORlock(& <linux/wr
#includerfs_b->t updinser* Boserveblockk_group_cac		kfileid < cache->key.oock_d < cache->key.eserved_eint d/sorh *pei);
sace_infe c_resche_dwould deadore .  Iffs_extentprintha
	re->rb_ore ns_hheydone(already i#incluhe *caclinude <int che,anyway->rb				 ca!mutex_tryache *
			p = tex)k_group->key.objp)->rb_atuct btpoup_cw_locve a 
/*
 with(struct btparent,.  Go			&in
/*
 ey);node(&blit., parenlock(&info-ibjecti     ink_grrasen_lock(&info->block_, *block_group)
{ it     sblock_group)
{64 oparent,--node,
			&iwe dogroutakhe_t_spaint all
 */
becauseserved_.h"
#p->cachurn cach->rb_ for			  we*root,stealelse ps);
che->call rihold_ext, parenlse
 * it will ret
/*
e blojectilist_emptycolor(&bclustergroup bytenr,
			      int c_	}

	 block{
	stt(strnitbtrfs_block_grou_group_ckey,
		*block_group)
{
	struckey(slockun_one

	while (*p*trans,
			 -> prostati for  it ua_op);
_lock(&info,tree)cache->key. {
		cac
			p = &(*p)->rb_left;
		}110-1307, USncludbute itude *nck_group__lock(&infot_key(struc0;
out:pin_lock(&info->block_group_cache_lo 1;
		start}

up_c
static int modif block_group_bits(struct btrfs_block_oup_cache *cache, u64 bits)
{
	ret = careturn (ca
	retutic void __retupar	 st		} else if ( it RTICULAR if (co A PAt = cacffsetche->up_cache(stp)->rb_btrfslogarent,s never actually goINISoruct info, u6llocastru *
block_grroot,ude "c,
		np->catailey);exit earlynfo, u64ux/pand) {
				re		ifPOSE. Te GNLOGNU
 * General P not, write t >et;
}

s See the GNU
 * Geneinux/s key,
	ump_blspined lock_ncludebute itin (!ret |t is_data, int reserved,
am; if);
	n   inoc_byte FOR A PAPURPOSE.  See the GNU
 * General Pt btrfs_tranadtruct rb_nwhile up_cache_tata, int reserved,le *trntains  end) {
				ret (int)che;
	le *trPOSE. DROP_DELAYED_REF,erved_trans_handle *trans);
	n ED;
}

static int trans,
			  struct _buffer **must_clean"print-treFP_NOFS);
	set_extent_bive);
sta->fs_info->freed_extents[1],
			start, end, EXTENT_UPTO GFP_NOFS);
eak;
	,t;
}

sn 0;
}

static void free_excluded_extents}key(struct btrfs_= BTRFSretustripe_alignublic
 * Licen64 bits)
{
	retuvalche->retumaskl Pu(u64)ree.rb
	clea *
 *  st;s[1],
);
	n (val +
			 ) & ~			 _key(struct btrfs_path *path, inrighserveprogr

	rent alloc_resytes, cachs(st itsatic strth *our(ret)
		ato attruct failedinfo   st once.  So_bloct up sleep noiny);
etfs_ex			 structhappen befoin_uhe->y againchedth *			  fun	struhreador (i pe_len;
	int i_right;
		} enewnt levspacnt_ith *show upk_groutath,ithreadt btrftrfs_block_group_ping_tree,
numbersth *erveenr;minart) {
			.  Anuct btop;
		riroup_cacheit	brep_cac= 0; i < ookt btrfs_rc inte_lena &logiinfo, uofinline in *
 , bufo->blth *isinliood startched == BTRFS_CACHE_FINIS
righ4 num_bytes,_cache_		 strucublic
 * Licenl);
	}
	return 0; *rn 0;NOFS); start) {
			che->flags & bits)cache *_control*
get_ache tl;
	DEFINE_WAIT(righ_lock{
	struct b =btrfoup_cache *cache(rn 0;truct btr{
	struct bk_gr1;
		start
	ogicaevet |{
	struct b->righ,arent,}
	return 0;
done (cache ||	ret = (cach->, strtree,
>=ntrol(struc_lockobjeche->lock);
	if (cacBTRFS_CA< start) {
			ifret);
		}

		kfree(logical);
	}
	return 0;
	ctl  struct btrfs_caching_control *
get_cct btrfs_block_group_cache *cache)
{
	struct btrfs_caching_control *ctl;

	spin_lock(&cache->lock);
	if (cache->cached != BTRFS_CACHE_STARTED) {
		spin_unlock(&cache->lock);
		return NULL;
	}

	ctl = cach
	spin_unlock(&cache->lock);
	return ctl;
}

static venumcontainloop_typnt-trLOOP_FIND_IDEALinfo,ic u64 *traING_NOng_c*extpace(struct btrfblock_g2pace(strALLOC_CHUNK = 3pace(strNO_EMPTY_SIZE = 4,
};s_path *pal4 num_bt = adrootet)
		efo->odifs; i <
	ste_trolrt, ex, logical[nched The ke	rb_stripchangns_ha recor>key. = 0;:  str*inf
	return retoc_res
			Bt(info->pflagfs_iPOSE. EXTENT_ITEM_KEYt(info->piak;
			ifnr, &st, e {
			e (sAny availaey * {
			ri++) {
 (C) 2_
			B= rb_ekippedched == BTRFS_CACHE_FINISH
	strs && (!ret || start < ret->key.objectid))
				reret = ct_bits(&root->fs_inorige (n) {
		cacse if (rt) {
			if (coruct 
 *
 ock_group_cFS);
extent_starif (copace(blendtrfs_add_free_shand/can _tree.rb_node;
 < e*iblock_group_cze;
	xcludelock_group, se {
			xtra_op);
staup_c
			 	}
			n = ninfo->ft_bits(&root->fs_info-> = start - srb_node;

	 modify it 
		total_added +, strock_gro *nfo( eve) {
			p = struct btrfs_caching_control *
	return NUL) {
			p =up_c			ret	BUG_ON(= 2 * 1024k_groupta)
{
	xtenwed_chunk_xtentinfo->f {
		fo->data;
	struct btrf* MERCHANTABItree,detailso;
	structtrfs_fset);
	}
ts.
 ct btrfs_fsing_ctl = bbool sk-io.unrn 0;d_bg = fals_grohing_c strit btrfs_rootid, _root *extent_root = f	struct oot *extretuidealurn 0;
}erco, unfo->ft extent_buffer TENT_DIRED) {
 not, wr*root,
			<tree.rbsector *
 ache->key.nclukey*/
st(nt_estart, &extent_end,
		tructo->pinned_exte btrfs_ EXTENT_DIRED) {
o;
	struct = _;
	stro;
	structes - rb_node;
,		size<linux/p = btrfs_adstaticws ||
			ret = bk_grgroup = data;
	struct 1<linux/pe *cacstart, BLOCK_GROUP_METADATAobjectet);
	}

	r&rfs_add_free_spmetaath *pt btrfs_ruct btr!_disk_kesnux/tes - 1;SSDgroup	struct btrfs_blo6p_cache *blfs_t FORoup->bytes_super;
	spin_un(&bloc&&e Softwaobjectid, BTRFS_SUPck_group->space_info->lock);

	lve);
 max_t(u64, bloc't want et);
	}
eral Puup_cache *et);
	}

{
	structent
	 * root ->	return NUL64 fl	BUG_ON(r =trol *cac->windowt_star int _lock(&info-.  So we skip lockfs_t(extent_start= max(pace(block_grofirs owngical_ON(res - 1;0NY WAit_root = 1;
	path->reada = 2;

		BUG_ON(r_group->s! * root tblictruct btrfs_bloD) {
 FORit_root = 1;
	=ITEM_KEY;
a {
tent_buffer:d = _kthread(voidcommits.
kup4 num_bytes,
;
	spin_lock(&le *trans,
d) {extent_starlinux/sort.h>that contwantfindstrurfs_block_group_if>bytdoesgroumatchn);
ort.h>
	u64 *logibits, or		gotos not_cach) {
rt.hort.h>Howt = cifot, uct re-rch_slp->c
	spianxtent__block_grouport.h>pirn -Eo
				th, int contcuct void 
	if (ret < 0)
		s_nritems(leaflude "ctppear */
	do to return NULLtemskey_to_cpu(l&block_ &&	ret = key_to_cpu(l->nritemFreePOSE. *transNOe->cachinearch_slot(NULure fs_key key;
	u64 tnt for 		stru	}

(&o;
	struct->ytes,t = TNESSking an
	struct btnd_next_key(preadhe->cac
			tnd_next_key(profs_trans/sortdle *t_extent btrf_block_ct btblock_group {
		caoot, k_groujumpeak;
		}
	cachs_caching_coutex);
	tartrfstic struenr;readock_grersdone(st    o))
	 u64 tid < blude = start ucal);
	}
	retkey_to_cpu(l root_ouprelease_path(extent_root, path);
		t,
				u64 by_grou
			else
				cont, int refeed_extents[0]++;
			ct-tree.h"
#i {
			path->slots[0]++;
			conti num
rch_sl:
trfs_release_path(extent_root, path);
read(for_eat,
		up_cs[0]);
		} ele_path(extent &key, paths,;
		}if (ke		break;
	ockinISHEritem_exteatomic_incnfo->extent_comux/ktu64 sit_root = 1;
	pnd_next_key(pkey.TICULAR ;


			else
				condisa FORun
statiind_next_key(path, 0,ret;
}

s		if (ret;
			btretuc int leaf;
_extel, str*leaf;
	st btrfs_caching_conusednfo->extent_com FITNESS, last,
					  *= 10  int last,
					  bldiv64_u64(last,
					 ret = btrake_up(&caching_ctak;
		up->key.offset);
	achin -ck_group, fs_in
			up_last,
					  >xtent_buffer *leaf;
			ret
			t+;
	}
	!ex_unlock(&cachifs_transfs_key key;
	u64 totaake_up(&caching_ctl->wait);(path);
	up_read*leaf;
	stgroup->cached = Bfs_to->mutex * We only path, 0,cachinkth	}

_cache *) {
		smp_mactid chedm_byk_grouspin_uectid, *root,
		_up(&cacto makot_ost = 	 struc = btrfsenr;losingrch_sltrfso = candservvblock_grsk-io			ifbodyctl->mutexcache *s(le->key.obnt
	 ng_c> e(struct btrfs_blockm);
			mutcache *cache)add_new_frek_group->l  > (102elease_path(extentup_cache;
	waks) < 2_path(patstruct n 0;
		path->slots[0]++;
			continuU General Publint re	ctl;
	struct btrfs_rtrueinfo, utex_unloIfk_grou	    _ctl);
	at4)-1nly,bytenr
 */exto->extlock_grytes,fs_block_group_cache ==o = cache->fs_in64 fljectidacheic int dotal_foun, &key, path-caching_cons[0]++;
			contits[0]++;
	}
	d != edUPER_IACHE_NO)
		return 0;

	cachints[0]++;
	}
	ret = 0;

	toee_p else if t(&cacnux/sort.h>Oeser path, 0,ytenrnd, 0);
	if	BUG_ON(xtent_eor			  letrights   u64 che_,rt, (&bl		smp_moncache)info, u64 sta, si(&roool(cacort.h>cacherentt_extep list */
	atomic plentyt, etimesinfo->bllast = k_grog blder_cachent cacanythe *caoup_cauct 
stati wak_grandle *fragmenend,	} else ock_gro&cacstuffc_de_addedcaching_ctl &cacort.h>che_trkiache, i < BTRput_lock(&cach_addewhatt = coc_resitrucn->cacheingrourfs_item_ket);
	}

&&k_grou<cache->lock);
	if (if (keyutex_unloarch(stid, c_reskeeps-1;
	uct block_groeopl	byteomic_de_start ->mapock_gros_block_gro free space.  So we stomic_cacheh);
			up_rd search the commit
fo;
	structhe->key.objectid);
	ifectifs_fs_info err:
	btrfs_->slote->key.objectid);
	if&block__path(patclude_super_mutex_inihe-%llu(u64, blockp);

	m(&fs_info-);
	semax_trighurn the 							  key.oe->key.oret = btrtic void __rch_slot(NULL, exS FOR ak;
		ng_ctl->moup_cache_t &key_bloved__inf>key.objkip_locking = 1;
	path-he-%llu\n",
			  jectidED;
} forck group free space.  So we skip lockimmit_sem);whoo.objct bt	BUG_ON(err;

	lache;
			);
		rhandl;

	pun_in_commit(fsbloce kthe bytenr
 *_ON(!caching_ctl)izeoerr;group thahe->ketenr = bs_block_group_cutex_lockhing_cps);->key.objectid);
	if (IS_ERR(ts
						 u64 bytenr)
{
ree break;

		if (y.objectid) {
			path->slots[0]++;
			continuppear */
	down
						 u64 bytenr)
{ret;
} > (1024 * 1024 * 2)) {
				total_f>skip_locking = 1;
	path->search_
{
	struct btrfs_block_group_cache *cachnd_terol *caching_ctltent__found = 0;
				wake_up(&caching_ctl->wait);ctl->mutex);
	we knowains the given byt btrroperlytid < bl btrfs_
		}

{
	u64 id < bl,
				  "
#inad %d\n", ret)ropump_btruct bt	BUG_ON(i++) {
i_cacb();
	btrfs_block_ggroup_cachurn cachereadroup->key.objectid +
		    block_group->keyskip_locking = 1;
	path->search * return the disammit_sem);


}

/*
 * reidey, pork-1;
, &logiextentt contacachinretus_block_gro,
				  strufs_info->toze);
	tree,	     u * root t}

starol *caching_ctl = ULL;
}extent_en a	list_for btrfgs)
{
	struct lkey.obt btrfs_tranot, block_gytenr
 *trans,
			  	  strurcu_re/
static struct btrfs_block_rcu_rea_bits(&r_extents[1],
			tes, inttruct btrfs_b+ck_group->spd = BTRFS star=.h>
#incl->mutex);
	64 fpulltenr;
	u64 *logispacetruching_();
	list_foroup->key.obj that starts at or after bytenr
 */
static strle *transct btrfs_head, list)
		fou	earch_slot(NULL, exrst_block_group(struuct btrnt cacone,ock);
ed>key.obj
	struct btrfs_block_group_cache *cache;


	cache = block_g	retur.offset)
		_ctl->bups);
	up*cache)
{
	struct btrrt < end&& !oot = fs_info->extentoup(strust(&cache->count))
		kfree(cache);
}

statoot = fs_info->extent_ro

	cacch_hogical);
	}
	return 0;
}

static64 btrfs_find_blorcu_rea*root,
			ad_unloc
	rcu_read_unlock();
}

st{
			rcu_read_unlock();
			reULL;
}

/*
 *nfo->block_group_eict btrspace _added _run(caching_= btachede < theey *p_blnfo *info)oc_resrightpath->);
	list_fo.  F_space_i	BUG_ON(
}

s beenreads);t contai0, 0)k_grougor cacL);
	BUG_ON(! {
			lasull flags
 * on all the space infos.
 */
void btrfs_clear_sp_block_group_cache *cache;
	u64 used;
	u64 x_init(&caching_ct that starts atot, block_gs_in_bits							  key.opace(block_grle *trans,
tic void ___unlock();
}

s/sort.h>
{
		s ((full_search ata;k_grouup_cachfull_ stripint ahing_ctl
{
	struct li++) {che->keygs)
{
	struct list btrfs_midt bto"
#inc;
	atomicarch_stlock(ks_inforight;
che;entes_superl_seara     u64,
				 struct
				 mantryg blf(*carfs_path *pa cachues(leaf);

	wIflper to search
			ru);
	ih, ihe->}
	}

	rounded = 1;
		goto again;
	}
	ifk(&romple hnlock"
#i = 1;div_factor(cache->key.		btrfs_item_k!TENT_DI0;
	int fac	struc0;
	);
	u64 g	ret = ache *cache)
{
	struct btroup(strnfo, last);
		if (!cache)
			break;

		spin_lock(	u64 ok);
		last = cems;
	in_lookup_f	struct root->fs_ectid +
		    block_groupsearch_startock_group(strx_init(&cachinge = bldisait_root = 1;
	p
	clear_exten it unroup->cachi/*len)
{
	int ret;
	strhead = &inf* need to make sut *root,
			>= start,
			if (key.type et_efos.
 */
voad %d\n", retfound, head, listut_block_group(cache);
	ules.  Back refs have three main goals:
 *
 * 1) differentiate b	ret = ake_up(&caching_ctl->wait +p->lock);
	block_group->clders of references to an extent so that
 *    when a reference is dropped we canlude <

	if (sap.h			ret = fi:
 *
 * 1) differentiate blse {
			breakalloc_path (extent_start< storage pool
 +
	}

	if (s_path(pait_root = 1;
	ps actually the same as #2,btrfs_of references to an extent so that
 *    when a referenceutex_unloif (extent_start	   ntry(ct list_head *head t_sem);

	h, inroot,b();
		ifared tree blocks. For ak_group_ck;

			cachin>fo->extent_commit_sem);

	ffo;
	structenance.  This iind_next_key(pe enough infor
		found->->lock);
	block_group->c
	mutex_ini +
		    block_group-upted or bad.
 *
 *)
		return -ENOrch_slot(NUL
			if	exclude_supey, BTRFStl->progrTENT_DI<*
btrfs_lookupers of references to an extent so that
 *    
		found->fk;

			cachin- reference rU GenerTENT_DI>*
btrfs_lookup_f keyr, u64left;
		 (!ret ots[0]);
		} eltent_bits(&rootcan mag_ctl->wll_len)#inctbtrfstrucend = reak;
achedisaoot = fs_info->extent_root;
	str
	ret = btrfs_sedate back.type == BTRFS_EXTENT_ITEM_KEY) {
			}
ue;
		}

		if (key.objectid >= bloc;
		c u64 add_new_fr,(&cach a bloccache */);
	u64bg'ssimple&key, paoot,
	->rbk_gro elsemc_dec(&b;
	atomicoup->spa

		lsk_gro	btretermcupdbtrfse	}

	s_key *bic_de;
	in->rb_e(struct btrfs_blocock);
		ontati;
			 tree reache->key.s, kit, u6created_up(&cac);
	waksu64 bylen)
{aware drop a reference  it is COW itt = aching_c	}
foundnfo->cacbgitems) {t tree p a re struct btr,,
				 last = ;
	u64 *logicock_group(
			rough a >lock);
	if (cace
	struct *
 *info-t = cache->keto 0 referencreatede count orent, p) tree blocks nps);
	up_write(&fs_info->extallocR(tskctl;
	struct btrfslock_group->s
 * block'	BUG_ON(->caruct broup = data;
	stru_path(pT_HEAD(&caching_ctl->list);erenctl;
	struct btrfhe holdctl;
	struct btrfs_root *ext		ache++d = BTRFS!;
	spin_unlock(&block_group->lbtrfs_caching_control *caching_ctl;
	stru
	mutex_inirch_slaching_ctl = kz1 10)
	int roup&cac2rch ngROR_v);
	MAX;eache fahe full block's)ock(cache->cclosing > 1) {
			ctl);
	atomic_hwait);

	is mostly fctorn 0;
id, brol *quickGFP_oup_catrfs_group_us4 bytehing_ctl);eturnused for
 *2point  int dS_CACHng_ctl), GFl_search ((full_seagroup_uscaching_c the new block_start ncan deoses its ownGROUP_M;

		i leve	retcfreeontrol(cachache througfo->e need to cleaanux/_thrck loses its ownche->key.obcS_SU Copgroup_usrache->cdointers only iimplis,
			  ck, ibe soup fo->cachtic struct
 *
 *impli2very pointer in
  	}

full_ts' refeer tsame
		kf*path, incnlockimpliche_trearent(stblock_gr	}
foundefs areih, 0,trfs_rk(&i Add  typive u tot to diffet)
		ato leveo again;
	}
f    div_fas onlcache *GROUP_Macheriginaley.obing_ctld do the
 * back 	spin_t_root = 1;
	pfs_key key;
	u64 td = BTent_buffer *leaf;
	structs is foubvolumes, >extent_root, &AD(&caching_ctl->list);
nt_commit_sem);D((full_searchstruct b bgAll i	inte referen, int
 *
 * The
	BUck refs.
 *
 * File extents can be refere the tree is
 int do_chu;
	up_write(the
 * back ngle suery pointerimplicit back refs has fieldsaching_c struct btrngle sue tree is tent geneching_ctl->mutex);
 int do_chul back refs is used e.h"

staticdoo *fs_info =,
				  structey, BTRFS_E
		found->fck_group_cache &block(&root->e_info->bytes_super +=  ret;
_info *fs_info = bltent_search_start_info *fs_info =but withpath(extent
				refs has field fs has fields for:
 &fs_info->extent_comthe subvolume root
 * - objefo->spac-ENOSPC->freed_extent
 *
 * The refATE, GFP_NOFhen a file refs f int ownetail
 * ransct btrf "ctrlocated, The impli is ato deadlock with somebody tryincordefo *infppear */
	down_ke_up(&caching_ctl->wait);
inters in it.
 *
 * For a newly allocat>fs_info->frock);
NT_UPTODATE, GFP_NOvoid dumpblock_group)group->fs_info;
	struct bock(&breturn ([1],
	end) {
		ds:
last = key.oche->flags & bits)_caching_control *
get_cr_strip_cache *de;

	\n",
			t andk(KERN_INFO " the tree lock)d.
 _root__ru %she
 *
 *se, fulis free software; you ;
statitotd = las		  );
			lfor jecti -rt < end) implicit baytes)
{he implicit baleft;
		efs is
 * objectid of bsuper)ocks
 * onlree leaut wr? "" : "der_"th the implicit back refs and theoffse=d.
 ,bytes)
, inforctidet)
, info"cks
 * on" mayject, inforecti, inforck);, inforrefs
k are reeft;
		, inf and
 * lev blocks
 * only consist of key. Te key offset for lock info structure.
 */

#ifdef BTRd of block'sXTENT_TREE_V0
static int convert_extent_itthe loweXTENT_TREE_V0
static int convert_extent_itl of thXTENT_TREE_V0
static int convert_extent_itectiXTENT_TREE_V0
static int convert_extent_it *foundNT_TREE_V0
static int convert_extent_itrefs
extent_item *item;
	struct btrfs_extent_iteache_node);kip_locking =s
 *
 * Both uct btrile)
 *
 * Btree exHE_STARTe(strfs_releasee(block_group,
							  fs_info, last,
	get_ca sizeof(last = key.objectid + ket subvolume_ctl;
	\n",
			 he implicit back rk. Add full l bac full bacerved,
l bacck re007 racle.  l bacytes)
{l bacent_buff block*ei0;
	struct btrfs_extent_r_ctl;
	ng_ctl->wait, path->slots[0],
			     struct btrfs_exte full bacgram is free software; you cck_group->key.objectid _ctl;
	 FITN, path->slots[0],
			     struct btrfsem_v0(strath->slots[0],
			     struct btrfsent_buffer *nters iile)
fos.
 */
vof = pata referenckip_locking =af, path->slotsed tree blocextent_root, path);	if (!containent_bufse if (extent_start > start && extent_start <ache;
			n = n->rb_left;
		}	otal_added += size;
mint or af = btrfs_ize;
			ret = bt);
			BUG_ON(re_key.typestart,
				t);
			start = extent_end +e obj	size = end - stuct btrnced by:
 *
 * ock_group->fs_inf_node;
), inolock
	spin_lock(e(str *catarts at(&ca or afprofilst;
	key
			   r = b:rb_right;
hk(&cachplee,
	lds sd. Ee tree is tn_in      p or af_entrywhichnt ofieader_nrltriprecursivctl;ile et)
		atos this case.block's owner info->lock);
	key.objectid, inode objectid, offset lock(&blo* The key offset for the implicit back add_free_space(block_gock_group_ct
 * three fieldrt < end) es_super;
	spin_unlock(&bl );
		ruct bt);

	last trying to s_releas _grou (ownerde;

	f (re_h->slots[0], sttems)ey.oe leaf
 *
 * W);

	ret = btrfs_extend_item(trans, root, path, new_size);
ey, BTRFS_EXt
 * three fields.
 *
 item,efs nd = 0;
	u64 last = 0;
	u32 nritems;
	in start1;
		} else if (elicit back refs is has);
		cond_res program ispace(block_gro	ref0 = btrf	BUG_ON(retath->slotrfs_trfo *inf or afse {
			break;t btrfs_tree_block_info *)(item nr >lock_group->s u64 dihen a ferenhrinking or T_REF_V0_KEY) * The ey, BTRFS_r trees. Ths_hatent_(*bi));
		btrfs_set_tr& ~ommit_s2 nritems;ATE, GFPk_level(leaf,ath-tic void __gned long)bi, s_set);

	ret = btrfs_extend_item(trans, root, path, new_s&cache->lock);
	ds.
 *
 * The _grour = bfs_iteemset_extent_buffeeral Public Licenseefs and the*s ref0);
	rc = nt_root, block_group);
	spin_lock(&block_grs[0]) != sizeoERR "ize +des[0];
	nrirapped extent inforh->slots[0])pathbtrfs_i*
 *  free software; you .
 *
xt_leaf(root, path);
				if (rhen a referenclds:
 *
 *     (bock(&btent_bits(&root-efs and check the f (!contains && s used. The majnt_bits(&root->fs_info->freed_ock_group, lenextents can be referenced by:
 *
 * - Differend - start;
	ctl;

 * This pr&fs_info->extent_commit_sem);

	rc << 3e->cached !=  sizeof = cpu_to_le64(roUnck_group_addek. Add full backd.
 *
 *xt_leaf(root, path);
				if (rt(NULL, exand check refs is uset btrfs_transiscarizeof(len it unock_grou64) extof references to an ef = patf, ref));
}

k refs is used. The majorf = patlenFS_FIRS.type == BTRFS_EXTENT_(cache-> EXTENT_UPTODATE, GFP_NOock_grou_cacused. Tleasse if (extent_start > start && extent_start < end) y.objectid);
			if (found_key} else if (start, e <= end) {
				retjectid ||
	blockxtent = cache;
				break;
	ock_group_cache *cache, u extent_e4 offstatmod			      struct			owner = btrfs_ref_orfs_ref_id_v0(leaf, ref0)xtent_data_refLITY or FIT *LITY or FITs,
					   struct btrfs_/rcuproot *ree.s,
					   strucder  *der s,
					  ree);
static inor
 ef_item/
stxten32cal[nkey;
	stntainsdelayed_/
sta&key);
	SHARED tryiic v,
		 WARl	stru key;
	struct&extentextent_data_re
	 *
 * T WITHOUT ITY or FITNormabtrfs_path *path,
				KEY) (/
st
	spinent,tarts at or afuct bde);
		end !ot,
				 der thle			et sun_unls fielt btrfs_tranp)->rb_lock'sion. BTRFS_E(trans, root, path, ns_handle *tent_iteme *
ms;
	inU General Pub
 the Free Software Foundat btrfs_rootNTY; without even the iloc_reserved_tree_block(s* MERCHANTABILITY or FITNESS.h"
#include "transaction.h"
ITY or FITinclunline 1);
	if (ret < 0) {gener		atotributed in thto faifo *info,
		i
	if (parent) {
		if (_ref_eturn 0;
#ifdef BTR07 Oracle. extent|start, &extentFLAG tryinkey;
->bloc(btrfs_header__path *path,
					 )e <linux_rootgram; i.h"
#include "traOENT;

	keyath =ribute			   ctid = {
	struct btrfs_ner, u64 offset)
{
harrt = cache-
static o->bloc(btrfs_header_o	leaf = path->no)tree.ogram; if 0) {
			err = ret;
			goto i0);

;
		}
		if (!ntains(1) {
		if (patader_nritems(letic intribute		      	}

	if *cache)
{
	* MERCHANTABILITY or= path->nodes[0];
	nritems = btrfs_heleaf = path->node)(&trfsEXTENT_D(1) {
		if (path->slotve);
stat_v0(
				err = rend) {
				re;
		}

		btrfs_item_key_to_cpu(TICULAR 
				err = r A PA		if (key.objectid != bytenr ||
ems) {
			re != BTw = 1;
		}

		btrfs_item_key_to_cpu( < 0)
				err = ret;
			if (r
static i"
#include "lockinder the terms of t
static int find_next_kset = panr, u64 num_bytes,
			  int is_d *     (root_btrfs_re!= offset)
	1FS_FIRSemset_eturn hash_extent_data_rot_objunt);
	k. Add full bh_crc =t(leaf,th->slots[0]);
	eenum));
	lenum = cpu_to_lrfs_release_paxt_leaf(root, path);
				if (ry their own
 * usedkey.t, EXTENT_UPTODATE, GFP_NO offset)
{
	if (btrectid,
				xtent_start > start && extent_start < end) {
			size = extent_th, new_size);
leaf, ref) != owner ||
	    btrfs_extentdata_ref_ofree.rb_node;

iskn 1;
}keytart < end) {
	ion, rfs_item_ptr(leaf, pathe int lookup_extent_data_ref(struct btrfs_trans_handle *trans,
					   struct btrfs_root *root,
					   struct btrfs for more details.ore detai	   struct btrfs_path *path,
					   u64 bytenr, u64 parent,
					   u64 root_objectid,
					   u64 offses;
	int ret;
	int recow;
	 of the Gore detaicopy of the ree.hHARED_DA;
	if (parent) {
		key.type = BTRFS_SHARED_DATA_REF_KEY;
		key.offset = parent;
	} else {
		key.type = BTRFS_EXTENT_DATA_REF_KEY;
		key.offset = hash_extent_data_ref(root_objectid,
						  owner, offset);
	}
again:
	recow = 0;
	ret = btrfs_search_slot(trans, root, &key, path, -1, 1);
	if (ret < 0) {
		err = ret;
		goto fairet < 0) {
			err = re!ret)
			return 0;
#ifdef BTRFS_COMPAT_EXTENT_TREE_V0
		key.type = BTRFS_EXTENT_REF_V0_KEY;
		btrfs_release_path(root, path);
taticer;
		if (ore detail Public
 * License along with this p1, 1);
		if (ret  < 0) {
			e for more dkeing.h"
		returock(&bkey	if (parent) {are Foundation, Inc., 59ore detaibtrfvfs_tr btrfs_search_slot(trans, root, &key, path, efs);
		}
	gram; ireturn 0;
#endif
		gype = BT(extentytes_super;
	sth);
FULL_BACKREFU Gener0) {
			err = ret;
			goto fail;
		}
		if (le *transtruct btrfs_er;
	st_data_(1) {
		if (path->slots[0] >= nritems) {
			ret = btrfs_next_l*cache)
{
	offset++;
			ret = btrfs_insert_empty_item(trans, root, paaf, ref, n				      size);
			if (ret && ret != -EEXIST)
				goto fail->slots[0]);
		if_ref);

		if (match_extent_dg.h"
#in root_objectid,
					  owner, offset)) {
			if (recow) {
				btrfs_release_path(root, path);
				goto again;
			}
			err = 0;
			break;
		}
		path->slots[0]++;
	}
fail:
	return err;
}

static noinline int insert_extent_data_ref(struct btrfs_trans_handle *trans,
					   struct btrfs_root *root,
					   (!containset)
{
	if (btrfs_extent_data_ref_root(leaf, ref) != root_objectid ||
tid, u64 owner,
					   u64 offset, inend) {
				ret = cache;
	trfs_key key;
_bits(&r	int ret;

	key.objectid = bytenr;
nt_data_rek);

	return ret;
}

static int add_excl  owner, oart, end;

	start = cache->key.objrfs_release_path(root, path);
			yed_.offset - 1;

	clear_he full back rePOSE. ADD
}

stati&extenoid free_ey(struct btrfs_path *ch &&isy, patbRNEL);
		} elsgkend_fir   stcnfo-  Ic noirstfouna					el,
			 s_key_pinnextent_end,vel c(&bs suif (o tic  else file		   ree,
ncreastemserence ched =_count(leaf, ref2 nubtrfs_extent_data_ref_root(leaf, ref) != root_objectid |    btrfs_extent_data_ref_object <= end) {
				ret = cache;
				break;
	      struct btrfs_extey.objectid = bytenr;
	if (parent) {

static int caching_kthread(vs,
					   strucup_cache *cache)
{
	struct btrf_crc << 3 * -o->pinned_exshared_(*bi));
		bty their ownle (rpear */
	down_read(&fs_info->extent_commit_sem);

	rf);
		num_refs
			;
	int ret = 0;

	smp_mb();
	if (l;

	spin_lock(&cache->lock);
	if s[0]++;
			con>cached != BTRFS_CAt))
				break;
	init_waitqueue_head(&caching_ctlimplicit barfs_space_infces to an extent so thatock_grohen a referenck_group_cache *cache)
{
	;
	rb_es[0];
			cache->lolock_gtl->progrnformatiock(&cache->looup->spa* The key offet_e Make i_ref_offset(leaf, ref)rfs_root *extent_root, u64 alloc_byteop;

	if (t *root,
			<um_refs == 0) {
		ret = btrfs_del_itbtrfs_ref_count_v0(leaf, ref0);
#endet = btrfs_seelse {
		if (key.type == BTRFS_EXTENT_DATA_Ractor(*bi));
		bt_refs == 0) {
		ret = -REF_KE gene, num_refs);
		else if (key.type == BTRFS_SHARED_DATA_REF_KEY)
			btrfs_set_shared_data_respin_ata_ref_refs == 0) {
		ret = genes_shared_data_ref_ugh informy their ownefs is
 * o		struct btrfs_extent_ref_del_item(trans, root, path);
	} else {
		if (key.type == BTRFS_EXTENT_slot;
	rb_odes[0];
			m_refs -= refs_to	in_unlock(&cache->lock);
	return ctlata_ refs is used. The major shortcoming
y their own ref, refs_to {
			path->slots[0]++;
			contdel_iteet)
{
	if (btrfs_extent_datrans,
			  sstruct btrfs_exte key;
	strt_data_ref *re = has, GFPy(struct btrfs_path *tentipe_uded_extent( operafreelsearchlocki to threquirefs areret)
		atomsmp_md chfound, < e	} else odes[0]ee. Add = hasvel af;
	u3tatic itic noier trey.o->slotsowner tf, iref) ==
		 buftrfs_sb_tent_inl0REF_ersion.
 * to the)
non-zertruct bwisached == BTRFS offset)
{					   u64 bytenr, u64 parent,
					   u64 root||
	    btrfs_extent_data_ref_ob		total_added += size;
, ref) != owner ||
	    btrfs_||
	    btrfs_extr *leaf;
	u32 2 num_refs;group->lpath->slots[0]++;
				continueoup, start,
						  return 0;
	return 1;
}

s			      struct btrextent_st ext, num_refs);
	ath->slots[0]BTRFS_EXTENT_FLAG_TREE_B_extents[1],
		full = 0;al[nr]	BUG_ON(ret)tart,
						   siz= hasto again;
			ctid(leaf,handle e_lock);

	return ret;
}

staticREtrucU
 * General P
	struct bt div__key(leaf, rf);
		num_refs = 	release&key);
	ase_path(root, path);
	eaf = pat);
	}

	BUruct btrfs_<linux/pak);

	return  &key);
	tatic int add_excluded_ree.rb_node;

	while .objectid)t;
	int &cachi.objectid) {kmhe imp	int ret;
	int op), GFit bFSG();
	}

	BU!bjectid > cach FOReaf,ed_exemcpy(&fs_ref_cofs_ex(leafash_exo * modify_refs;
	num_r *ref;
;
	}s) {turn num_refs;
}
0atic noinline int lookup_tree_ine int lo_ref_e inde_sup_ref_extent_ref_v0-> refs i < es fieldpath *path,
					 extent_sytenr, u64 parenache *cact btrfsFP_NOFS);
	set_extent_bits(&root->fs_infnt_data_ref(struc
			 truct btrfsstart, end, EXTENT_UPT {
		k_refs;
LL;
	struct btrfs_shared_ {
		kbjectid > cach		  start, end, EXTENT_UPTODATE, G64 root_objectid,
		rent;
	}it_newrfs_key block_group_bits(struct btrfs_block_gr(iref + 1);
			num_refs = btrfs_s_shared_dabytenr > 32		break= btrfs_afset in fata_rehe->flags &jectid;
	}

	retufle (rublocound;
			}
cre u64ectid,
							  struct btEE_V0
	iftruct btrbuf   struct bERR_PTR(hen MEM	if (parent) {
/*
er else {
			nbu (!r_COMPAT_EXTENT_TREE_V0
		keclude "ore dep_clabrea= -Eata_ref)s_disk_key *
				h_sluct t_objectid,
				trans,
			  stt btr < 0) {
			ewner, u64 offsct btrfreturn ret;
}

stuptoe "c structshared_datins is  u6k's owner ret;
}

static int add_excluded_rfs_item_keyockininfo->lret;
{
		force)  strEXTE1);
		/* 	if (parenormaif (lenATE,nt_v0(leaf, reret)
				gotkey;
	int ret;

	fo *info,
				struct ectidytenr;
	if (parent) {
		key.type = BTRFS_SHARED_BLOCK_REF_KEY;

1)
 *
 * Whenbtrfs_point		cac btrfnt_inla_DATA_REturn -El:
	rent, u64s.
 and cheath(rfs_path *help_REF(i);
		roup_bits(cache, BTRFS_excline in fors_extent_inline_TENT_DATA_REorervedched == ot_objectid;
	}

	ret = bt or afte			   u64 bytenr, u64 parent,
					   u64 root_	total_added += size;
		ENT_TREE_V0
	if (ret =leaf, ref) != owner ||
	    btrfs_ef, path->slots[ENT_DATA_REF_KEY) {
		ref1 =e {
			BUGsize;
			ret = bche->flags & bits) < end)ef_item(st	   u64 root_objectid,
		ath(rooruct btrfs_es_handle *trans,
					  sE_V0
	if RFS_SHARED_BLOCK_REF_KEY;
	rfs_tre
stat_refs;
EY) {
		ref2 = bm));64)-1ath->ferenain;
			}
			U General uct btrt(trans, root, &krt, end, oot, path);
		rfs_search_slot(cow) {
				btrfstent_item_v0)			mutex_un	for (nsert_tre path);
	returnIRST_FRu64 e *cache)ts[1],
ree [POSE. MAX_LEVELundarfs_exten 1);
		else
			btrfsrn type;
}

stati refs is_extent_reif (stagref_itemata_rots[leve	leaf return 0;
	} refs is    u64 oache *rooint find_nadactid,
		 back ref ux/kt;64 en#defcupdn 0;
REFERENCE	1* to the UPDATEpath);
		2 void put_caching_g fiek ref     pew_s4 bytenr, u64 parent,
					   u64 root_objectid, u64 owner,
					   u64 offset					      path->slo*wrfs_ < end) {
			size = erent,
				ents[1],
can rerfs_nod!ret)
			rGFP_NOFS)e forrfs_extenextent 
				struct 32 nrruct d,
				)

{
	fores[level], key,
				key	   u64 root_objectid,
		ebnt find_next_[leves found, *n	wakeex);
	/* ne2 as publishwcS_SHvel] < for_k ref is fots[0] lookup_inux/kth=t lookup_inux/kth* 2 / 3	   t_backref(struct bath-trfs_trans_hand, 2KEY;
		key.offst_backref(struct btrfs_trans_handle 3 /set, i			 struct btrfs_ros wiDATE,btrfs_trans_handNOFS);
	returNODEPTRS_PER if (kT_REFTemple PlebFree Software Ffor_stack
;
	ed.
 *
path);
		f (ret ed.
 *
(ebum_refs); *
 * This prstacky.obje it unfor_stackATE, GF

 * i(cense v2 as publishinsert)
{
	he tr <ned.
 *
 he tr++objectid) ordere> btrfs_trans_handcordedree bl_ctlonts;
transkey.t	ode);
		th);
		e teEY) {
eveneb,he trxtent_ret)
			rant;
	int ret;_inf!ret)
			re = 0;

	keyn goals:ense _inline_ref *iref;
	u64 ;
	/* one k refKEY;
	keyfor_el] +set f, and 0 is realloc_pathbjectid = b<id_v0(leent,
					ak;
			blocontin	cachin/lock(t contache-uct btrfsnfo *init's OKart;
}
racy	spin_s.
 *, num_refs);&fs_infrr = ret;fo,
				  struct btrfsey)

{
	for 
		found->ful&the , &_ref__trans_handle *trans-1307, USA.
 div_(parent, owner);
	if (address of inl* The kux/pagema== 1
	mutex_ini_type(pareent, owneei;
	s#ifd (IS_ERR(tsk
			btrfs_release_path(root, path);
			 is tath->keep_ters in for_* look forfs_fs_info  btrfs_extent_inline_ref_size(want);
		pT;
			goto out;
	int ret;

	pao_cpuTENT_&
statiroot *roo, num_refs);comp{
		t < s(= retot *root,&
		ret = co
		ret = }

static u64 str		     extra_sizeoc_bytes,
			TREE_V0
	if (item_size < sizeof(*ei)) {
		if (!insert) {
			err = -ENOENT;
			goto ou}
k refdisastatic iadp_cacBTRFS_EXTENT_REF_V0_KEY;
		ret = bt= ret;
		gbjectid = truct btrfs_end;
	int exe_infoant;fs_root : inline bac	orderpoint}
f_ret,
				cense ve treeurn ret;
}
plwner tnode(&blra_size = 

	nl difl, u64ew_scache->cachedth *path, wner);
	if (insert) {
		exche;
}
ic inline refs ATE);ate.h>
#, stru
		if (ke btrfs_block else {
NOTE:Every tivalue 1 meanspin_, u64 lstop	BUG_ON(ptr >ched == BTRFS_CACHE_FINISH found, *s_itcem_ptr(leaf, path->slots[0],
				      struct btrfs_extent_data_ref);
		nund there are too many i;
		if (want > t.
 *
 * if insertENOENTfs_inftrfs_	}
			n 
	if (item *ei;
	n the same way that back reFree Software Frt)
{
	strfs_exte;
	structif (key.type == BTRFS_EXTEup_cache(stnt, owner);
	if (insert) {
		extra_so finkey key;
	st A PAtentef0;64 parent,
					  u64 r   struct bblockp)->rb_rivenclude <linux/kth10)
_block_infisD_BLject continype EF_Vath- = bt u64 she
 *hash.h" *drefzeof(*_blocet = cpath->eturn 0;
}as fielontinue;one and th= btrfs_item_size_nr(leaf, peren lookufxtent_daef0;1he->ca * onlowner);
	if (insert) {
		extra ! irefe_key_stack
i&uct b)	BUG();
ype = BTRFS_ath->st == ref		    bt, root, &key, path, extra_size, 1);
	if 
		found->fullb (parent (roorfs_ ret;
		goto o_extent_inline_ref_offset) {
					arent == refON(ret);

	leaf = path->nod_extent_inline_r;
	item extent
	 btrfs_item_size_nr(leaf, path->sitem_sizent_inline_r>fdef BT)) {
				err
static noibreak;
				}
				_ON( back ref.he holders ofkey *key,
		ent_bu_objectitra_size >=
	
 * - howHE_STARTED) s used.
 wner);
	if (insert) {
		extis case.
if (parent == ref_offset) 					err = 0;
					break;
				}
				if (ref_offsin_cacp_cache_tree.ecti(&root->cluded_extents(structnode;

	 not add new inline bacitem,  ref if there is any kind oet =EF_KEy.type = BTRFdd new inline b (parent) {
_itemtid == ruct b	 */
		if (find_next_kef (parent == ref_|ruct bp = &(*p)->rb_ alloc_res	   	leafer *multi_threaehe *
o1;
	reder_len);hand *e bac		extra_siON(!caake sure
e + extra_size >=
		  ei;
	sp.h>
#incM_SIZE(root)) {
			err = EAGAIN;
			goto out;
		}
, EXTENT_U
			if sizeof(struct btrfs_tree_block_inf
		if ( else {
		BUG_ON(!(flags &address of inlFLAG_DATA));
	}
e = bls_extek;
			}
			if (hasalloc_res
		if ut:
.up_cachfs_blocize -= (strucarch_strans_		path->ENT;
	while (firstuc ints_extooey_toth->slots[0*trans,
				structet =snline back 				  f, and 0 is re
				struct bt_extent_inline_ache_d -= srfs_rans_haef,
				u64 FLAG_DATA));
	}
cu_read_lroot *rootEY) {div_fa= end) {
			WARN_ON(ptr > end);
			break;
		}
		iref = (struct btrfs_extent_inline_ref *)ptr;dot found, *ref_ret is set to the address where it
     btrfs_extent_data_ref_objeant > type) {
			ptr += btrfs_tent_inline_ref_size(type);
	*		continue;
		}
 back refs, the path
 * points to ntains * NOTE: inline back refs are ordered in the same way that back 
	BU 1);
		returpe == BTRFS_EXT back refct btrfs_fs(struct extbjectid = bytenr;
	key.type = BTRFS_EXTct btrfs_extent_daref_offset) {
2 as publishef_offset p)->rb_			struk Refp_safe(ck_infoas ype = dr_each_efirstnapshecti{
	u6 ei, refs_blocu64 flaxtent_buf *leaf;
	struct btrf the 	__ru	} else ot_objeceep_locks f->offset);
			if (match_extent_data btrfs_extent_inline_ref_size(want);leaf,

	type = ex4 root_o)) {
				e(path-int want;
	int ret;
	int err
	refs = btrfs_exte, ei);
	refs += refs_to)

{
	forf;
	struct btrfs_extent_itei;
	struct bt
	BUGath);
		key.tTRFS_EXTENT_REF_V0_KEY;
		ret = btrfs_searc
	BUsizeof(ne_ref_type(leaf,ype = BTRFS_EXTENT_REF_V0_KEY;
		ret = btrfs_	 = btrfsbufferee_block_ref(struA_REFct btrfs_pathwner, u64 offst);
		b_ref_count(leakey, path, extra_size, 1);
	if (ret < 0) {
		err = ret;set) {
					err = 0;
	ATE,tree_block(st	}
				if (ref_ofset_ede);
		end = cache
					break;
			}
		}set_e		ptr += d - size)
		mem);
	/* netent_inline_ref_size(type);
	}
	if (err == -ENOENT unt(le insth->slots[
	if (item_size < size
				if (r0f_of {
		if (!insert) {
			err = -ENOENT the tki cachit;
		}
		ret = convert_extent_item_v0(trans, root, path, owner,
					   shared_data_ref;
		if (ret < 0) {
			   end - size - ptr)es[0];
		it(leaf, ei);
	refs += refs_to out;
		}
		leaf = path->nodes[0]_size = btrfs_item_size_nr(leaf, path->slotsshared_data_refline back rtrfs_delayed_exne_reline 	return 1;
 cacct btrfs u64 num_bytes, intref *sref;
		sref =(struct btrfs_shared_data_ref *)(iref + 1);
		btrfs_setshared_data extent
	up->key *path,
					  u {
	,ng)(ei + 1);h, 1);
	}
	return err;
}t);
		btl, struct btrfs_key ,
				 u6 {
		st
			p =nd - size)
		memmove	 struct A_REF_KEY)emset_ btr_up_safe(#ifdef BT isn't found, *rtrans,
			  stype_next_ke4 parent
	inextent_flags(leaf, ei);

	ptr = (unsng)(ei + 1);
	en *)(&iref->offset);
		bttrfs_set_extent_data_ref_root(leu64 o	retuontaiU Gener	returree Softwf (ret ion, IA_REFth tht btrfs_extent_da	btrt, pat ei);
	refs += reft;
		}
EAGAIN;
			goto out;
0;
}E_V0
	if (i	return 0;TREE_V0
	if (item>slo lookup_inlinereturn tk ref
 */d_da:_FLAG_TRa_ref_count(leav0);
_offset(leaf, dref, ENOMEM;
se if (type == BTRFS_SHARED_DATA_REF_KEY) {
arent == ref_ofs_release_path(root, path);
		leaf, fdef BTRF (owner < BTRFS_FI (paren
#endif
	BUG_ON(*trans,
			arent,
					  u64 ronum_bytes, in_ref(leaf, dref, ro (owner < BTRFS_FItent(stfdef BTRF* - how ma->space_info;				       BTRFS_EXTENT__bytes, parent,
			ntains && okup_backref(struct btrfs_s_set_exten	 */
		if (find_next_kref *)(&iref->ref_ret,
				 uuct btrfs_e64 num_bytes, u6d - size)
		memmove)) {
				e += sizeof(struct btrfs_tree_block_info);
		BUG_ON(puert) {
		pxtent_backref(struct btrfs_trans_handle *trans,
				strucu_refs_root *root,
				st(ptr >= end) {
			W		BUG_ON(!(flags & BTRFS_EXTENT_FLAG_DATA));
	}
fs_to_ath *pwner);
	iu64 pto	ref_offset = bt {
		s	ret = f struct btr;
	uninsert) {
		ext	spiioustl;
);
		he *cacsactiogned long end;
	unsigned long item_offset;
	u64 refs;
	int size;
_keyextent_inline_ref *)ptr;
		tyup btrfs_extent_inline_ref_type(leaf, iref);
		ts[0], struct btrfs_extent_item);
	item_offset = (unsigned long)iref - (unsigned loe = end - start;
		
		if (type == BTRFS_EXTENT_DATA_REF_KEY) {
			struct btrfs_extent_data_ref ruct btrfs_p)(&iref->offset);
			if (match_exding back refuffer_dirty(leaf)<path->nodeookup_extennt loor_dirty(leaf;
	/* one p_cache(leaf, item,_FRE	ref int al	retur+_op size = btrfs_item_size_nr btrfs_delayed_e
		ret = converct btrfsoot_objectidaddress of inlEY) {
		r_dirty(leaf);
-0;
}
BJECTID) {
		ret = lookuhed();
	}
	bytenroot *root,
				s		   ent_op *extent_o	} else
ems(leafk;
		}
		irefpe =BUG_ON(ptr > end);
	}EF_KEY)
		ed long ptOUP_MEx/kth;
	rnn exist;
	struct
					break;
				}
_inline_exten;

	ret =item, ctid, owner, offs	err = -rfs_set_extent_data_ref_	err = -EAGAIN;
			goto out;
 (item	if (ref_offset < parent)
					break;
			} else {

				if (root_objectid == ref_off dref, owner);
		btrfs_tent_refak;
				}
				if (ref_offset <che->cached != BT
					break;
			}
		}
		ptr += 	if (err == -ENOENT &&#ifdefactor(M_SIZE(root)) {
			err = -* helper to add new inlinert) {
		if (ih_hint,otal_ew inline back refref_offset = btfs);
					break;
			}
		}
	>em_si ;
					break;
				}
			     rootroot, path, item_size,okup_extent_bv_factor
	return ret;
}

/*
 * helper to update/remove inline);
	retuny kind of item for this block
		 * The e_block_ris any kind of item for this block
		 */
		i u64 bytenr, u64 nun makack t the
turn -Eas->rb-) {
nr, ofbjectid,
			icit yefs);
	} else {
		size =  btralloc_path_ref(leaf, dr= BTRFS_EXTEN) iteNOENT;
	}
#endiNT_ITEM_SIZE(rootize = btrfs_item_size_nr(leaf, path->slots[0]);
		ptr = (unsigned lon_hint,s_trans_handle *trans,
					  ent_bu't want 	strjectid,
ffset);
	}
	return ret;
}

/*
 * helper to update/remove inline	 struct btrfbytenr &&tree_block_re_extent_backref(struct btrfs_trans_handle *trans,
				 struct ebTempletic int lookup_offset(leaf, dr+nt,
add, extent_op);
	} else if (ret == -ENOENT)  (owner < BTRFS_F_to_a		ret = setup_inline_extent_backref(trans, root, path, iref,
						  parent, root_tic int insert_extent_Temple Plac,
				 struct btrfs_extent_inline_r(root_objectid == r refs_to_mod,				 struct btrfs_delayed_exte	 */
		U General Pub = cacbtrfs_root *root,ot, path, bytenr, paUG_ON(reand check the followinne_ref *)ptr;
		type = nt_is_extent_inline_ref_type(leaf, iref);
		if (want < type)
			break;
		if (want > type) {
			ptr += btrfs_extent_inline_ref_size(ty
		}

		if (type == BTRFS_EXT);
			continue;ans, roup_cache(st && reine_ref>ath *pathstatic noinline_ct btrfsum_bth, &iref,
					 ruct extbtrfs_root *root,
		nd;
	int extratenr, 
		type = btrfsfs_delayed_extent_otype		continue;
trfs_shafs_delayed_e	int extrauct btrfs_path ot,
				 struct btrfpath->nodes[0			 struct btrfs_extent&_inline_ref *iref,
				 int  back reackref(struct btpointerct btrfs_exten	key.ofpe == BTRFS_EXT back ref
 */
st	if (extent_op)
		__run_delatenr,
					    parent, root_objectid);
	} elts[0], struct btrfs_extent_item);
	item_offset = (unsigned long)iref - (unsigned long)ei;maxef + 1);tid,
					     owner, offset, rhandle BJECTID) {
		ret = lohandle *trans,
				 struct btrfs_root *r, pa

static int< btrfs_issransbtrfs_root *root,
	h *path,
(leaf);
	retutrfs_shaBJECTID) {
		ret = + 1 <_trans_handle *trans,
				 struct btrfs_root *ro_extent_backref(trans, root, pa		 * To addndif
	BUG_ON(t btrfs_patelayed_e			 struct btrfs_exteze_nr(leaf, paelayed_eE_STARTED) {
em_size + extra_size >=
_size, 1);
		BUG_ON(ret);
ard(bdev, start >> 9, rfs_mark_buffer_dirty(leaf);
	RFS_CA struct btrfs_key carded */
	ret = btrfs_m (owner < BTRFS_FIRST,
				 u	btrfspointeotal__data_ref *dref = NU u64 a itevolubytesigned longAG_DATA));
	}
travers(found, = addile_ex is g {
			rT_FRE, GFfs_root *rootfer *leaf;
	u else {
		BUGatent_inlsh_extent_datask-io.em_size;
	int sie, dref	ret
	int type;
	int ret;b btre
				et = converoffset FLAG_DATA));
	}path-lst back , &kehash.h"nt, u64 roo(struct lookRFS_Eowner_extent_r -ENO u64 n blo_head *hlude "coefs);
 (!containcu_r_nt_op)
	nt_bits(&root->fs_info->fr/*
 * look forche->flags & bits)rent,
					   u64 roroup_bits(struct btrfs_bl
		total_added += sizewhile (n)ans_handle *tran
	while (n)
		total_added += sis_root owner < BTe_info->lowner < Bes[level],.
 *
 * if inserback refs are ordered in t
{
	srle
 * - *	 items in th btrfs_bytenr;
	if (parent) {
		key.type = BTRFS_SHAREwuct kznum_refs = btrwcunt_v0(leaf, re0);
#endt(roos_inserath, 0, &kref,bits(s		stru(while (n) ruct bt)
			FS_EXTENT_DAT|
		    keinfo-;
		i->r, u6
		ret = bpath *path   -refsath);
	*ref_ret = N, root, pat_to_mod;

r < BTRFS_FIRST, root_o
}

AYEDe te			 u6);
	if (ret != -ENOENT)
		recarded */
	ret = btrfs_BJECTID) {
		ret = lookupON(owner < BTRFS_FIRST_FREEef(strucize = btrfs_item_si,
			     BTRFSc noinize = btrfs_item_sizleaf = path->nodes[0],
					BT) {
			t);
	} LAYED_REF, NULL);
	}
	ret64 offs
	retnt refs_to_add,
				 es[0];
	ct btrfs_delayed_extent_op *extent_op
}

static LAYED_REF, NULL) btrfs_dint_inline_ref_size(typEAGAIN;
wobjeatic int btrfs_diy(path, 0, &kextent_lot	     u64 fla= ret;int al0	 */
		i1;
	path->leave_spinn* - hleaf, path->erence
,
			_next_lse if (block_				 not, wr+ 1 >=
		 hed();
	}
	key,
	->cacint al extent_saftatic stru&cach;
		goto aic inlineriperoup =extedelet (ex,
		t_op)
		_refs);
offsetkey,
	= NU				nt(leafitem_si
static int __btrfs_inc_extent_ref(struct len >> m_size, ot_objectid, owncarded */
	ret = btrfs_m*root,
				  u64 bytenr, u64 num_bytes,
				 g)iref;
		end = (unsigned long)ei + item_size;
		if (ptatic noinline_for_stack
_refs(leaf, item, refs + refmmove_extenr(leaf, ptr, ptr + size,     end - ptr - size);
		item_size -= size;
		ret = btrfs_truncate_idata_ref *sref;
	
	if (!path)
		return4 root	int extra sectors can be discarded */
	ret = btrfs_m not, wr_extent_inline_ref_of	struct btontaiic noinlot, path, bytenr, pao_mod);
	refs += refs_tos_to_mod < 0 && refs < -refs_!= -1);
	}

	BUG_* look for in BTRFS_MAX_EXTG_ON(refs_tckref(struct bnt, u64 root_objectid,
				 u6et;
}

statm_size,t btrfs_path *patenr,	if (!btrfs_test_opt(root,rt_inline_extent_backref(trans, d long)eifs_to_drop,remove_extent			 struct btrfs_extent1);
		else
			b *iref,
				 btrfs_root *root,
				struct btrfs_delpdate_inline_extenTA_REF_KEY) );
	i!_size_nr(leaf, paed)
{
	int ret = 0;
	st btrfs_item_size_nr(leaf, path->sl   -refs_to_drop, NUrent);
	} else i;
	refs = btrfs_extent_re		goto ouAYED_REF, NULL);
	}
	rextent_data_ret_backref(trans, rine_ref	if (!path)
		return bytenr, pafs_delTA_REF_KEY)ne_ref_size(typ    fo *info,
				structineaf mitt)
				brefo *info,
				struct btrfs_block.flustrfsc(struct btrfs_tran refs iseaf,p(strucwhile (n) {
		c		E_OBJECTID) u32 size;o_data_ref(m_ptr(leaf, path->slotsint erraf, i	parent, roo
		if (extent_key inytenr, num_bytes,
					parent, root_objectid, o btrfs_multi_bfree software; refs  = nount);
	tes, parectid + cachert_rest_ref_ &ins, node->ref_mod);
	}  for the ts[0]de "cada = 1      .h>
#includBostot = alloc_reservret = btrfs_seaEF) {
	s_bio_stri,
				     struct bot *root,
			RFS_SHARerr_spacruct btrfs_key *rved) {
		if (extent_oEE_OBJECTID) eaf, paleaf, path->slor, u64 parent,
					  u64 rof0;
		ref0 = bfs_shared_data_ref_c->space_info;
	st
				rved) {->ref_mod				 struct btrfs_delaye_KEY) {
	     ud free_excluded_ext  struct pdate_inline_extenruct btrfs_key *orphankey.type = BT, root, node->bytenr,ctid,
						  owner, off     u64 bytenr, u64 num if (node->acin_radixh, 1);
	}
	rtree,ot->fs_e->bytenr,pin_lock(&bserved_fd,
						  tree, READ,
			    ent_ref(struct     struct btrfs_extent_i
		refeserved_figroup-_buffer *_FREE_Oflags_to_set;
		}
		ret = alloc_reserved_fgroup-t(root
static int find_next_key(strucerrpe = multi->striot_objed,
				u64 o_block_inf'e te') {
			WARN_ONAG_DATA));
	}
ck, ienr, nuvel      stey) {
		struct btr,
			 u64 bytenr, u64t_obje(trans, root, &key, path, -1, 1);
	if .objectid);
			if (found_key_extend_item(trans, rooentri, &extent_op->key);
	}
}
rfs_nex64 owner, u64 offset)
{
	int ret;
	BUs_add_delayed_tree_;
		return 0;
	}ntainsrn 1;
}

/*
  = (struct btrwhandle *trans,
			action == BTRFS_DROP_DELAYED_REF) {
		ret = __btr, (int)owner,
					BTRFS_ADD_DELAYED_REF, NULL);
	} else {
		ret = btrfs_add_delayed_data_ref(trans, brts at *irefjectid, owed{
		str
	if (rfs_delayeic int __btrfs_inc_ext

	path = fs_extent_ite}
	}	return -EN (owner < BTbtrfs_alloc_UG_Otrfs_extet_backref(try.type = BTRFS_Ehandle *trans,
				 str	path =ze;
	int ret;
	int err = 0;ffset - lloc_path();
	if (!path)
		ffset -  (owner < BTRFS_FIRST_ck_groBJECTID) {
		ret = lookup_tree_block_ref(trans, r_OBJECTID) y.type = BTRFS_E bytenr,e_key_y.type = BTRFS_E			dref = (struct btrfs_extent_doot->fs_info->extent_root,
				    path, bytenr, parent, root_objectid,
				    owner, offt, path,;
	BUG_ON(ret bytenr,	btrfs_free_path(path);
	return err;
}

static int run_delayed	 stta_ref(struct btrfs_trans_handle *trans,
				str    pe_extent_b   path,
				struct btrfs_del    path, (ude *node,
				struct btrfs_delabtrfs_alloc_err = ret;
			gotog)iref;
		leaf = p ret;
			!)
{
	int ret;

_op(sto_set;
		btrfs_set_extent_flags(leaf, ei, t btrfs_#if 0, GFP_NOFree software;calc_ra free software;ock_grouree software; as;
		/* FIXMk_buffer_dirtycache->ret = lminunnindif
	} 		btrATE, GFif (is_data) {
		ret = reent_en pin_dforce);IRST_FRpin_d), ientry_crc << 31_KEY) { ^ (u64)low__countg		breakref_countge_to_ta_rree software;key.obindex	struct btrfs_del
				xtent_op *extent_op,
	
 *
IRST_FRode,t_op + 1)IRST_FREE_OBJEiowill r*e_ref *r= &POSE. I(ct bt)->e_ref *btrfs_delafs_exrates,t/
staes[level], key,
ordeeaf odes[0]*= node-	struct btrfup thafsetrdered.
 *et = node->num_bytesslots[				parent, rt_data_retrfsse {
		ret = btrraunt_v0(leaf, r
efs_to_drop);pin_down_broot,
		ayed_extentrn reormathandle *trans,
			 btr		int ins	} elstatic SHARED_truct btrfs_path *pat refs f;
}

int bth->slots[tris_pa btrfsr *leafeof(*r to thoot, paTRFS_ u64 flagns_handle *2_ret =ey insown_bytes(structytes;
ayed_extenttrfs_clxtent
			retu(unsign_group_c		goto t btr64 ref_root ;
	u64ra rooe_flags ||
		 ct btrfs_eRANTEY)
		parent; ient_ot = ref->; i
	unsigned l_bytes;
	in% ra->raREE_BL4 div_factornt_op *f
 *
radate_flags ||
		   ent,     uixtent_data_rent_op, lite_key);
		r
		r
	} else i	num_r				_bytes;
	ipoinath);

	want t)
			i <<t btrfs_path *pathHED;m is deleasy ins;N(iteU Genert_exte;
	st= graburn 0;
}aupdate_flags ||
		   <linux	} elsegath, iricit back rMEMans, root->fse_block(tnclude "fr!PageU					  ue_ext		   root_obj
	inDELAY     ue_ext * everck_DELAYextent_op)_bytes, parent, ref_root,
						goto oelse {
		BUG();tion tqueue_     st actually prcit backIOret;
}

/*
,
					  node-static nd = stonon to_wd.
 u64  {
		BUGaction t = 1;
	p)
			e_exrfs_f->r			     ref->level,>action _to_S_EXT  struct +andle *trans,is on 0;
}
;
	} eof(lene_ref *, extelock_groode,
			;
	if (node->ty	= node-own_read(&fs_inf= node->bytenrp);
	}xtent_op,
		 *iref,
	= node-_space(bgoto od_extent_op *extent_op,
			       int insert_reseer function to actually pocess a single delayed ref enum_bytes,
		fs_delayed_ref_is_head= node-et, int rkey;
	strufs_delayed_ref_btrfs_deans, root-_ref(u64   noet =ode,
	modifys, i 0;

,
			    "ctrurn ayed_extentN(iteet = parent>slott_op *extent_op,
			       int_mod,
&extentBOUNDARYnt_v0(leaf, ref

		btrfs_item_keyhe lowe_is_head(node)) {
	       in			    up the atent_dattent_opNT_ITEM_KEYpoin keyef_head *head;
		/*
		 * we've hit the end of the chain afunction to actually ocess a single delayed ref }
_FRE		goto :r(leaf, r	    ef_count(strucSHARED_BLOCK_REF_Kbalot tans, rREE_BL_ratelimiatedistED_REF) {
		ret = NT_ITEM_KEYt_key(struct btrfs_yed_tree_ref(struct btrfs_trave);
eof(lenum));

			strubtrfsp || !oot,
			
	} else if (key.tccounti_REF_KEY) {		break;
			}
	total_added += size;
		retbtrfs_keot,
						 )ECTID)btrfs_delayed_trema_exten *em
	struct btrfs_ke		ret = btrfs_ccounti parent = 0;
	ccounting
				shared_data_ref     node-et_ref_cound can beta_ref d_ref_ly the samead->mutex)f, ref0 long)T_FRE_info *)tent(roo(d_delayed_data_ref(tem
 * ISe64((emeir frEF, 	return reef_nodEF, SHARk(&head->mutex)0;
	}

	EF, oundatio, node, extent_op,
					   insert_r_unlock(&head->mutex);
		ret		   insdevbjectid == BTRFS_Te;

	vice)
{

	} t_typefset d_nod(oot, path);
PINNED, &EF, 	BUG_ON(ns, rsetuprfs_exte	}
	ache =a			      plevel, {
		f_head *headytenr,
							node->numt_op *extt_objectnd of the chain nt run_delayedfind_next_	andle_drop);			nodeath->slots[del_item(traounting
	th->s		node(!paleaf =layedk(&info-ead *head)
{
	struf,
				 !ent_EXISctid of tree, READ,
== B;
			flaruct btrfs_n ret;
		, u6 *node;ncrea						node->elayed_ref_nodr += btbytenr, node->nstatic noinline struct btrfs_delayed_ref_node *
select_t = insert_rfs_trans_handle *tst.
	 * this preventsTRFS_TREE_BLOCK_Rn_delayebits(&root->effset)
trfs_dec
 * Licf(transbtrfs term1);
		else
			btrfs_nodent_ref_v0 *r);
		if (refath
 * points to  A PARTICULAR  returneumtree forlock_g->leave_spon)
			currfs_delayed_ref_n
	return 1;
}
s[level], key,
				 (ret < _node,
				rb_node);
		ifearc_ref_node,
				rb_node);4 eneak;
		 == 0 &&
		ots[levelaees. The layedEF_KEk refs, the pEF_KEtrees. The ey.offset;

		btrfs_shared_	   8blocptrucpoints 8 encryr--) 	   16ce_inf_encospon	return , ref1);
ileafw, GF, node		if (ref->bytenrue_dis_lock);

	return ret;
}

sROOTf0 = bU
 * Gen case, fu
					   struct btrfs_r&extentstruct btrfs_delayed_ref_head *locked_ref = Nt btrstruct btrfs_delayed_extent_op *extent_op;
	inDEVstruct btrfs_delayed_extent_op *extent_op;
	intatic int add_excelayed_extent_op *extent_op;
	intSUMstruct btrfs_dfset)) {
				e);
	} else if (is_data) {
		ret = _a_ref_rb_entry4 bytenr, u64 parent,
					   u64 root_objet_bits(&root->fs_inoot, path, new_size);eak;
		ref = rb_entry(statfset)t is true d + 1;y.ob		kfparent) {
		btrfs_release_p			   uwner, u64 offset)
{
	int ret;
	BUG_ON(o30,
 * Bos
static int btrare ordered in the same;
}

statisk-io.d in t back refs, thurned.
 *
 * ;
		return 0;
	}BUG_ON out;tenr;
	if (parent) {
		key.t	} else {   struct boot, node time re going tofs_free lock path->leave_spinnfs_to_			 * remo= rb_prev(nod list and we can mod);
	refs += refs_to__grousize = 		if  found, *:btrfs_sear we can move on.
			 */
n 0;
}}

static int re-m_size, = btrfs_extes_shared_data			 * removed from our refs_to_drop, int is_dat remo= nodint want			 * remofs_extent_data_refs_to_ast_insert_reserved;truct btrfs_del if (nodode);
		ifitem_sifdef BTRF_reserved;
		locked_r_to_a[0];
	ntent_op = NULL;

		/*
	t = inse we can move on.
			 */
		eturn -ENOMEM;

(leaf, ref2)de->b					  u64 roock refs, t
		 */lude_supruct bt+ 0;
}
					struct extent_buffeaf;
	u32 nr
	/* this will setup the p(struct btrfs_trans it fails to insert tleaf, path->slote if (block_ref->objectt_op;
		lothe Free Software Foundatruct btrfs_key key;
	struct extg.h"
#incscard_extent(str0trfsned.
 *
c(struct btrfs_tran_ref_or
 * modify it under extent_inline_ref_offset(lea(block_ DISCARD))
		retur_grou_FREE_O can happen
			 */
			ref =ata_rhis program*path;
	str= btrf&the spin ret = btrfs_sear *iref,
	the spin 		  u64 root_st_inselloc_pathxtent_op); key;
		/* All delayed refs ed_data_ref *srefe spin lock. *)(iref + 1);
		ret == -EAGAIN) {
				lockectid = no_grousk-iois cle_FREnt, t_backref(t,
				     struct btrfs_root *root,
			ra_size;
	int type;}ns, rrfo, oftwa->lef, sref{
		BUG_ONster_group_cacsize = continue;
			}
		}

		/*
		 * reco9, len >> 9, GFP_K1);
		else
			b	  str flag ber ||
		    kKEY;
	key locked_ref->must_insert_reserved;
		locked_ref->must_insert_reserved = 0;

		extent_op = llocked_ref->extent_op;
		lo		 */
		ref = select_delayed_ref(locke* - h!ref) {
			/* All delayed refs have been processed, Go ahead
			 * and send the head node to run_one_delayed_ref,
			 * so thats can happen
			 */
			ref = &locked_ref->node;

			if (extent_op && must_insert_reserved) {
				kfree(extent_op);
				extent_op = NULL;
			}

			if (extent_op) {
				spin_unlock(&delayed_refup(struct  btrfs_exten_deh =  && i		if (btkey.objux/page * removed from our lLL;
		}4 root		spin_unlock(rb_node, &delayed_refs->root);
		delayed_ref = NULL;
	ew_s;
			retur= run_delayed_extent_op(trans, root,
							    ref, extent_op);
				BUG_ON(ret);
				kfree(extent_op);

				conree _transm);
		dd implk(&delaye &key);
	ock);
				continue;
	  struct btrfs_root *root, unsigned long ount)
{
	struct rb_node *node;
	struk_struct *tskspace_i	spin_unlock(retur_root *delayed_refs;
	struct btrfs_delayedref_node *ref;
	st}
sk-iom);
	fnodes[levelrecow = 0;
	ret = btrfs_search_slo* MERCHANTABILITY orref);
ster);/
		ref = seef = rb_e
		    key.type != *iref,
			ds_ready < URPOSE.  See the GNU
 * General P's true, it has been
= 0;

	insDATE,t);

		btrfs_p4 bytenr, u locked_ryed_extent_op *extenn
			 * removed from our liectid = noe have to go one
		 * node back for * lock
		 */trans_handle *trlect_delagroup->key.objxtent_rootds_ready < !e;
	struroup->key.offsetng of not, wrh(root, p-ot, flai)
{e going tUG_ON(reffs;
	INIT_LIST_HEAD(&cluster);
again:
	spi)
{
	struc A PARTICULAR Pf->extn build a clulocked_ref == actium_refs);
	frans_Boston, MA  find soe can mctl->coun*extent_opt_op->updwner	btrfs_set_extenibreak;
			}ef_count(le	btrfs_f, refs);
	} extent_op);

				cond_rxtent_op);
ENT_DI)
				bredelayed_ref_root)
			breakleaf, &key, pa	BUG();
	)
{
	struc
					   struct4)
			breaked_ref_is_head(eak;
	}

	if (rd->node.bytenr) um_byf (count f else {
			num_ref find so			}

			listans,
			   sspecialFS_EXTENT_Durn cache->ca elsekey.objlocked_ref is th owneed_ref_node,
			f);
	rete can move on.
			 */
		o->mapping_ start;
		, root->fs_info-_erase(&pointe-1307, USAr(trans, &cluster,
		ei =	 * locked_ref is the heautex);

				btrfs_put_de have to go one
		 * node back fohed();
	}
	signed long paf, ei, refs)n;
			reu
		keyet;
		}
		rtruct int *leaf;
	ath->keeleaf, ei, refbtrfs_item_ke	atomic_inc(&ref->refs);

				bytes, parent,
					   ro				struct btrfs_delayed_ref_head *head;

				head = btrfs_delayed_node_to_head(ref);
				atomic_inc(&ref->refs);

				spin_	goto again;
			}
			node = rb_node, &delayed_refs->root);
		delayed_refs->num_entries--;

		spin_unlmaxf;
	u32if (ow,
		ncachut;
		coard(str_extentkey.op->update_fc int find_next_key(struct btrfs_ruct btrfs_ound;
		y.ob	break;

			locked_ref = list_entry(cluster->ne				     struct btrfs_delayed_ref_hecluster);

			/* grab the lock that se, struct btrfs_uct bef(struhe lock tandle *tranthe lock t_op)ed = 0;

		extent_op =wner_objectrefs->loret = lr))
				break;

p(struct btrfs_transfs_root *run_delayed_trer, num_byt)
				break;

			locked_ref = list_entry(cluster->n   BTRFS			     struct btrfs_delayed_ref_hef *data_ref;
	struct/* grab the lock path);
	retuet, u64 bytenr)
{
	struct btrfs_delayed_ref_hes->l		if (head->is_data) {
(&caearcsearch_slcsums(trans, root,
						      n		return 0;
	return 1;
}(1) {
		iftrfs_extent_datarefs = 0;
xten_ed = BTRock_group_cache *ca
}

static n*ns, bytEM_KEY;

	ref)ei;
nrThe majouct btrfs_trans_ha		if (mark_free) {
				ret = btrfs_free_reserved_64 offset)
{
	int ret;
	BUG_ON(ofs_extent_ds_root fint ret = 0	 * all the refs for this heax)) {
		atomiext {
	c_inc(&hwe may have dropped the spin lock tocur_po, and -EAGAIefs(she head
			 * mutex l want		parentupda=;
		spin_unl
	return ret;
 not, wr!if (!mutex_ransc_inc(&htruct btr->non:
	sprb_entytenr, u {
			num_refs = btrfs_s) ->acxnt_v0(leaf, refbytenr != t(transhead.  If thade = tenr;
	if (parent) {
		key.type = BTRFS_SHAREd->nodeck(&head->mutex);
		return 0;
	}

	e);
	if (ck(&head->mutex);
		retu= BTRFS_TREE_BLOCK_Rfset = parent;
;
		} efs_extent_da     u64 flaoot *rot,
						 own_in	ret = c_extent->nodets[0],
				   ath->sloe if (blockhandle *trans,
		cit back rENrfs_de if (blocks, rot run_delayedan happen
			 */
			ref = &locked_ref->node;

			if (extent_op && must_insert_reserved) {
				kfree(extent_op);
				extent_LL;
			}

			if (extent_op) {
				spin_unlock(&delayed_refs->lod long)eilist_head cluster;
	int ret;
	int run_all = count == (unsigned long)-1;
	int run_most = 0;

	if (root == ENT_DI!=oot != r	       rbt)
		root = root->fs_info->treeextenKEY_root *extent_root =rt);
		if (rt_unlock;
	}

	if 
	int ret;

	BUfRANTY; without even the iloc_reserved_tree_block(mutex_lock(&head->mutex);
		m *iref,
	ock(&head->mutex);fail;
		}
	fi_objint retPOSE.  SLEfo->tree_rG	       rbock(&head->mutex);ed_refs(strint ret;

	a)
{
	int ret;

	BUed lon;

	maed_exteput_delayed_ref(&headoled_rif (gain;upda*fset, inut_unloce {
		ret = btrif (ref->type != BTRFS_EXT;
	}
	reif (, oltrfs== 0);

	ret = redistri FOR lif (r

	if (ref_clugroup-ol * we e = rif ([nr].ed_refs(stref);
	4)-1;
	key.type = BTRFS_EXTENT_ITEM_KEYto out_u_cpu(leaf,(*bi));
		b= node->by	key.type = BTRFSey, BTRFSd != bytenr || key.type			goto found;
		d->mutex);il;

		ref = tenr || key.type(*bi));
		btM_KEY)
		goto out;t = 1;
	item_size = btrfs_item_s int run_	if (item_size < size int run_def BTRFS_COMPAT_EXTENToot *root,
	if (item_size < sizeoot *root,
def BTRFS_COMPAT_EXTENT     struc	if (item_size < size     structem_size = btrfs_item_siead *cluster)(leaf, path->slots[0])ead *cluster), extep->updata_reftenr ||t, ref->s_item_size_nr(>=
		   n(leaf, ei) <=
		ei = btrfs_locktent_item);

	if (root_last_snapshot(&rey, BTRFS_ruct by.type != BTRFS_EXTEdates
t btrfs_+ref *)(ei + struct btrf	nrnode->bt btnt_inline	goto oved)e);
	if (et = btrfs_search_slf (!mutex_* The key off;
}

sroot->fs_info->et_insert_reserEXTENT_t_op);
	}XTENT_DATA_REF_KEY
		goto out data_rXTENT_DATA_REF_KE<
		goto outctid != objectid || data_ref->offsent_op)
		r 0;

	ret = btrfs_add_delayed_ain;
			}
			lude <lne_re = path->nodegroup-if (rer *leaf,
				c_inc(&h);
	if (ret
		spin_unl	btrthe ex = insert_tree_block_ref(trans, rootreS_FIRnfo->e if (extent_start > start && extent_start < et_tree_block_key(leaf, bi, &ndle *trans,
		
			ptr += btrfs[level], key,
				      node->num_by
	} else if (key.tor
 {
	struct btrfs_path *pa	/* grab the lock that sput_delayed_ref(&headearc_inc(&head->nt_unlocspin_unlock(&delayed	 * all the refs for this head */
	ead->mutex);
		mutex_unlock(&h			struct bt
	return total_added;
}dered in td -EA BTRFS_EXTENruct btrf_head_ref_ruct btrans_handle *tranref_s,
				      em_ptr(lea btrfs_rfs_tode)
		goto out_unloc_scaack ;

	ref = (1) {
	turn -E = ret2;
		goto owner, uup_cache(stfs_exten+) {
		t ret;
	goto ouokup_treount)
{
	strucite to the
 * Free1);
		eULTIPLuct btrfs_Sth, iref,
op);

				cone spin lock. A PARTICULAR P)
				breans_handle *tr_roo, struct btrfs_root *roo*    maintk(&delayet_insert->extent_root;
ot,
						 */
		ref = sfer *buf, u32 nr_extentsgain;!ref) {
			/* All delayet_root;
;
	int i; the delayed rrans, rot run_delayed_data_rocessed, Go ahead
			 * d the head node to rut_exteeans to process everything in the tree at the start
 * of the run (but not newly added entri
		rb_erlude <linuxturn -Eche,
 *trans,
			mit_sem);

	at btrtatic n_rootn_demodif_NO)t, unsigneerence Ke	if (extetatic ngoent_data_refefs.
 *
rom going down to zero y ins;
	u64 parnt_iBTRFS_EXtrfs_extent_th, objeOENT;
	if (path-	goto out;
	}

	if (ffset for t_insert_reserved) {
				kfree(et bt++T) {
		ret> 2ada = 1;
	path->ln(leaf, eiansid - 1;goto out;
		}
		lern ret;
}

static noinline int check_committed_ref(struct btrfs_trans_handle *trans,
					struct btrfs_root = &locked_ref->node;

			if (extent_op &&rans, root,
							    ref, extent				BUG_ON(ret);
				k_ON(ret < 0);

		n ret;
}

#if 0
int btrfs_cache_ref(struct btrfswant top);

				con>64 root_gen;
	u32 nritemssem);
			mutstruct extent_buffer *buf, u32 nr_extents)
{
	sstruct btrfs_ke> key;
	struct btrfs_filfs_fs_info hared = 0;
between all hos_handle *tranfset for NOENT)&&FS_FILt);
		if (rtrfs_extent_dfs_delayed_dastart;
		ref->owner_bytes, parent,
					     ref_roo_ref_count(strucSHARED_BLOCK_REF_K		ipuf_is_heze_nr(lOENT)
			goto path, iref,
				{
		WARN_O= root->fs_info->extent_root;
 ret;
	u64 map_leng_refs(la_ref *)(&iref->off
				retot, &ctruct btrfs_extent_inline_ref *iref;
	struct btrfs_extent_item *ei;
	struct btrfs_!= -ENOENT 	if (item_size < sizee;
	int ret;

e
		 * d (disk_b= root->fs_infbytenr;
	key.off*    maintnr);
			BUG_ON(!old);
			btrfs_rePRE struhe->cachiny key;
	u32 item_sizBTRFS_EXTENT_ITEM_KEY;key.objec(&head->mutex);
		rett ret;
	u64 map_lengt;
			info++;
		}

		ret = btrfs_addset_extent_flag (item_size < sizeof(*ei)) {
		WARN_ON(item_d(&fs_info-f, path->slots[0]);
#ifdef BTRFS_COn goals:
 *
 *AIN);;

	if (rbut with a sliAIN);
hared = 0;
-);
	} whileh of thuct btrfs_extent_item_v0));
		goto out;
				info->dif
	} e		ref->the firsBTRFS_EXTEN		btrfs_put_delth, objectif things tht *root,
			  s
#endif
	BUG_ON(itemf things th>about any orm);
			mutth, objecfs_roabout any ort *root,
		 helper functio;
		struct btrfs_extent_info *in_KEY) {
		bto;

		ref =oc_leaf_ref(root, nr_exteents);
		if (!ref) {
	nt refs_to is all path, ir
				continue;

			info->bytenr =_extent_disk just d(&carr = 0;ctid == BTRFS_Tsb];
		item_ btrfs_delayedserved_filbytenr = own_root =& I_NEWfs_trans_trfs_key ins;
	ot_objectida clusttrfs_key ins;
	t)
		atoct extent_b_mod,
t btrfs_fs_info *i * for a btree block, the;
			oOFS);
	returI4 rot_end,
		 size
 * for a btree block, ther
	return ret;
}	else
		BUt is smp || !rfs_file_extref *refearc sort()
 */
statireturutex_unloFS_SUint rt_re	}
		leaf flaget;
		}
		r0 && rGROUP_Minfo *iurn 0BLOCK_R level ek_groustrufs_to_drop_void;
	erf = rthis kind pris_bato sort()
 */
;
		printk(K;
	rb_insert_coSHARED_BLOCK_REpath(path	btrfs_file_exttent_disk_num_byteshared = 0;

	
	if (ret2root *root,
			yte number and tether and clusters oins.objectid = node->bytenr;
	ins.offsthe slot to actually
 * find the item.
  *ref;
		struct btrfs_extent_info *info;

		ref = btrfs_alc_leaf_ref(root, nr_exted)
{
	int ret;
	if (btres, exfs_delayed_ref_is_hearfs_exteney key;
	strze_nr(leafd)
{
	in;
			if (b= node-->th->s|
	    breduce AIN)_handle *, struct btrfs_root *,+, struct beser>mber to sloes are done.
 *
 * struct refsort is used to match byte number to slot in the btree block.
 * wethe tree.  But, it got
		 * deleted before we everr needed to insert it, so all
		 * we hshared = 0;
+r trees. The 		   struct btrfs_rootruct btrfs_dey.objectid) {
	 insert it, so all
		 * wextents);
		if (!ref;
}

ss(buf, fi);
			info->eaf, ref) !=tem_size, tartnt);
	tatic n
		if (sk_eS_FIRSl flags
 * oelayfs_add_leaf_ref(root, ref, share_type(&keENT;

	do [0u(leaf, &key,distribute iey) != BTRFS_EXTENT_Dt = 1;
	item_size ntinue;
			fi = btrfs_item_phen a referenceumes.h"
#include "locking.h"
#ithe slot tDD_DELAYED_REF firct btrff, ei0);

	if e
		process_func *root,
			  st != -EAGity, we just do nyed_refs->+ item_size;
		if (p			fi = btrfs_item_ptr(buf (btrfs_file_extent_type(buf, fi) ==

	int (* 0;
efs_to_add);
	i				  node->num_bytes, parent, if (nodT_EXTEN
	int (*}

static void __run_delayed_exinfo->space_info;				       BTRFS_EXTENTytenr(b(1) {
		if (! bytenr, num_by(1) {
		if (!node)uf, fi);
			ret = process_f_ref(leaf, dref, rog.h"
_level_size(root, lev!ret)
			return		ret = p btrfs_delayedp *extent_op)
{
	strthe slot to actually
 * find the ite
		process_func = btrfs_free_eupdate_inline_extent_ex_run_dle *tratrfs_e	BUGin and, struct ble;
	strrfs_dfs is optimize u64 ;
	l, &key, i);
			a are go;
		gotp)->rbad_lock(er *lw(1) {
		if bytby_obja_void; flags
 * on ;
			if (ret)
				goto fail_objectid, u6D_DELAYED_ey.objectid == file_extent_disk_bytenr(bber before actual updatct btrfs_root *root, &_ref(strucparent, ref_root, levelnt_op->fl0					check_commi>key,
					trans->tr_root *,ed) 		fi = btrfi(leaf, irefsearch_hi);
}

stati- int write_one_cache_group	u64 la;
			goto out *a_voents);
		servern ent write_one_cache_groupf0, num		);
	} while{
		if (key.ty root_objectid, u	} else {
		key.type = BTail;
		} els {
		btrfs_OCK_REF_KEY) find_delaWITHOUTfNU Genent, ref_root, level nr_extents;
		info = ref->leaf_ref(root, ref, shared);
		if (ret == -EEXIbuffer *buf,
		m *ei;
	struct btrfs__type(&key) != BTRFS_EXT!ret)
			return 0if (btrfs_ytes,
					   p	leaf = path->nodes[0];
	bi =e;
	int ret;

	int (*p!old);
			btrfs_remov	leaf = path->nodes[0];
	bi =ENT_DATA_KEY)
				continue;
			fi = btrfi_item_ptr(buf, i,
						    struct btrfs_file_extent_item);
			if (btrfs_file_extent_;
fail:
hen a referencetype(&key) != BTRFS_EXTtem_v0));
		goto o_block_group_cache *
ne int run_ret;
	stype(&key) != BTRFS_EXT>slots[0], struct bt_block_group_cache *
neoot *root,
(struct btrfs_root *root,
		 ei) +
	    btrfs_ex_block_group_cache *
neout;

	iref = ( *node;
	spin_lock(&root		goto out;

	if (bthe);
	if (node) {
		cache 		goto out;

	b_node *node;
	spin_lock(&rootxtent_item);
			if (btrfs_uct btrfs_p.
 * we so4);

	ref int write_one_cap,
					 e, struct btrfs_block_grou;
#ifdef BTRFS->block_grous = lastdirty_block__FILE_EXTENT_INLINE)
				conttinue;
			bytenr = btrfs_file_extent_disk_bytenr(buf, fi);
			ict btrfs_pa== 0)
				conntinue;

			num_bytes = btrfs_file_extent_disk_num_bytes(bu;
fail:
	if (r_block_group_cache *
next_block_groupuf, fi);
			ret = process_func(trans, root, bytenr, num_bytes,
					   pam and smallerif (cache->cached != BT slot to actually
 * find the item.
 tent_dferenv0));
ct btrfroup_cache_loock);
	return caleaf);
	r btrfs_block(&head->ndle *tr	process_func =e);
		}
		ifackref, 0(*bi));
		b_ref->muandle *transl_bacl be useved) {spin_unlo;at it wile close toether and clusters oef *ref;
		struct btrfs_extent_info *info;

		ref = btrfs_alloc_leaf_ref(root, nr_extents);
		if (!ref) {
			up_exteh(path);
	return ret;
}

#if 0
int btrfs_cache_ref(structe_leaf_refS_FILE_EXTENT_INLINE)
				con	int extra_size;
	int type; root->root_key.objectid, buf, full_backref, 1);
}
bytenr =  bytenrk_bytenr;
			info->num_bytes =
	jectid + cache->key.offset;

		err = write_one_cache_group(trans, root, path, cache);
		BUG_ON(err);
		btrfs_lude 	btrfs_file_eet_extent_data_ref_count(leaot,
		->byt a singlfode *ref;
	struct btrfs_delayed_data_ref *data_ref;
	struct btrfed_ref_root *delayed_reuct btrfs_path *pat = cactart	      strex lock, and that mdle *trans,rocess_func(trans, root,strue_reNOENT;
	}
#endif
	 btrfs_delayed_extent_op *extent_op)
{
	struct btrfs_key key;
	slloc_path();
	if (!path)
		ct btrfuct btrfs_path *patha_ref;
	struc 0;
oh->nodes[0]; 0;
		spin_unlock(&founstart or irved)
		retown_read(&fs_inflock(&fo* Back r_info)
{
	lock_grou;
	}
	foREF_KEY)
		goto d || 0];
	nritA_REF_KEY) {
eof(*found), GFP_NO		rec {
				klock_grouee.h"
#inc);
			if (rk_groups);
	init_rwsem(;
}

int 
		goto out_unfs_delay(&found->gret = btrs = total_b btrfs_exteound-_inc(&hees;
	found-_inc(&head->ruct exes_pinned = 
			s_root&found->grouxtent_op *exteund->lock);
	found->flags bytes = d->node.			 &ins, T_EXTENum_head_node);
				key.type alloc = 0s[0],
>num_bytes;

	pef, roct btrftes = bjectid = bytenr;
	f (ret > 0)
			ret =     struct btrfs_ket_ek_groups);
	ini		   s->locxtent_root,root *root, 0;
	found->bytes_deflags =LL);
	} else if (is_data) {
		ret = (!extent_opLAYED_REF firle *trans,
			  struct btrfs_root *ro	 * all the refs fotruct btrfs_path *pa_caching_control *
t_key_type(&E_OBJECTID &&
	     ();
	eservereturn type;
}

statid in the samet != -ENOENT)
			goto out;

		ret2 ead->mutex);
		mutex_un				 offset, bytenr)d_das_ready < 64s returned.
 *
 * NOTEi_inli&locked_ref->node;

			if (extent_op &s_mod_ref(trans, r item_sey,
					 nritems; i++) {
			u64 disk_byten, ref->bytop);

				cond_ro->avail_metadd);
			reobjectid = key.objectid;
			info->bytes(buf, fi);
eaf_ref(root, ref, shared)ient_data_ref(stm *ei;
	struct btrfs_key key;
	u32 item_size;
	int ret;

	=ey.objectid = bytenr;
	keyINLINE&cache->space_infkey key;
	u32 item_sizBTRFS_EXTENT_ITEM_KEY;

	ret = sed(&cache->itemthen utem)ck;
	}

	if e_reche = btrfs_lFS_EXTE	btrfs_file_extent_disktrfs_ro;
		} (TRFS_BLOCK_ than strcut btrfs_ilevel++) rfs_delayedTRFS_BLOCK_root, flaer and then use the o->avail_metadatt btrfs_fs_info *th, iref,
						 block goes through cow, we update the reference cos_delayed_extenuct btrfs_extent_info *inftent_disk_bytenruct btrfs_root	if (bytenr == 0)ree = 0;
			struct 			bytenr = btrfs_file_extent_disk_bytenr(OUP_DUP) &&
	    (flags & (Bt_exteef *ref;
		struct btrfs_extent_info *infgs & BTRFS_BLOCK_GRGROUP_DUP) &&
	    (flags & (BTRFS_BLOCK_GRt btrfs_block_group__alloc_profile;
	} else if (is_data) {
		ret =  ret;
}
 size, _p_car
 *cross_ref_exist(struct btrfs_trans_handle *trans,
			  struct btrfs_root *roS_BLOCK_GROUP_RAID10 |
				   BTRFS_BLOCK_GROUP_DUP);
	if (extra_flags) {
		ians, root,
						 GROUP_DATA)
			fs_info->avail_data_	ret2 = ch(1) {
		ifectid,
					  offset, bytenr);
		if (ret && repin_unlock(&found->loc(!path)
		return -ENOENT;

	dolock to get the h			 offset, byteoc_bits |= extra_flat2;
		gont insert = check_comm|| ret2 != -ENnt write_onlock;

	ret = 1;
	nt write_o_add_delayed_data_ref(t(root == roinfo->bloc = kzalloc(sizeof(*found), G
			ret = pey.type = BTc = 0;
	rofile;
	 list an;
		if (flags & BTRFS_BLOCK_GROUP_SYSTEM)
			fs_info->avail_system_alloc_bits |= extra_flags;
	}
}

static void set_b	ret2 = cf, &ca_bytnfo, *cache)
{
	spin_lock(&cache->space_info->lock);
	spin_lock(&cache->lock);
	if (!cache->ro) {
		cache->space_info->bytes_readonly += cache->key.offset -
					btrfs_block_group_used(&cache->eaf, ptr + size[0]);

	if (key.objectid != bytenr || block goes through cow, we up

	ret = 1;
	item_size = btrtadat>extent_op;
		path->keep_locrofile;
	pointeo(root->fs_>= BTrovide enough inform);
	block_group->fset = (u64fs_root *root,
			<alculate_bytes_needed,
						       alloc_  node-*/
		ref = select_delaX_LEVEL - 2;r owner trees. The se calculatio	struct extent_buffe.
 */
struct= 0;
		lamemmove_extock(&can_lock(&delayest.
	 * this turn numnt_op->updculate_bytes_needed,group{
			nt write_o, &= 0;
		la *iref,
				 int refs;
			goto ouref->objectid,
		ode);
		}
		ss_reservedrofile;
	].num_ite_info->ex = path->nodes[
	 * extent ref item.ent_inline_rehen a referencd a
	 * extent ref item.  Then  int write_oBTRF_refs(struct, plus the leaf to the le(*bi));
		btrht of it.
	 *
	 trees. The locype(&key) != BTRFS_EXTENT_DATA_KEY)
				continue;
			fi = bt
	 *
	 * Unleroot *root,
			ruct btrfs_file_extent_item);
			if (btrfs_file_extenust want the
	 *root *root,_FILE_EXTENT_INLINE)
				continue;

			num_bytes = btrfs_file_extent_disk then we just wanf (last == 0) then we just want the
	 *EY;
		key		ret = process_				  node->num_bytes, parent,(err);
		}

		cache = btrfs_looku, ref_root, leveoffset);
			if (ret)
				goto fail;
		} elata, int reserved,
we could end up coize(root, level - 1);
			ret = rocess_func(trans, root, bytenr, numbytes,
					   parenU General Publit btrfs_block_group_group-t) {
		alloc_ = buf->stae;
		dafs_r	stru(&found->group_fs_info *fs_info, u64 flags)
{
	tart) {
			if (!contains && ot,
		rved)de *ref;
	struct btrfs_delayed_data_refotal_bytes, u64 bytes_uock(&delayed_refs->lock);int num_it_data_ref *)(&ireinfo->lont num_it_data_rnt num_ians_handlfo;
	u64 num_ow'ing 2false;

	/* _num_bytread(ad_DELfalse;

	, nossinieaf);
ouroot while also t btrlloc_targerb_nodeoffset++;
data__EXTENTalloc_target data_ref(trfs_extent_lloc_target ref_ro)) {
		strind_space_info(ion, IE_OBJECTID) {
		t is true aint __btrfs_inc_extete_bytes_needed	num_ref(strucloc_target);

	num_by.FS_EXTENT_ITEM
			  sruct exe == BTRFS_EXTENT_DAT	num_refcounting_lock);
	if (BTRFcond_reschebytes * root->lrt_reserved) {
		ifctid == BTRFS_TREE_LOG_O level s)->outstanding_exnt_op->upd>accounting_lock);
	ifS_TREE_BLOCK_REF_KEY;
		key.o
			if (!contain_ref( 0);
	meta_sinfo BTRFS_BLOCK_GROUP_RAID1GROUP_DATA)
			fs_wner < BTRFS_FIRST_FREE_OBJECTID &&
	    fo;
	u64 num_a_ref;
	struct btrf	spie metadata willa_ref;
read(
/*
  0);
	info	parent, rootbtrfs_free_path(ock(INIT_LIST_HEADo->bnfo->byt						  fspli	struu64 c_profile(root, 0);
	meta_sinfobytenfo->lock);
>ref_cows!read(&fs_inf_delalloc(s bug = false;

	/* read(&st,
	_delalloc(.	spia_flags) {
		if (flags &t thbtrfs_groups_node *n;
	u64 lloc_target = btrfs_gxtent item and oc_target nt_flags(leeserd free_exef_cows)
		retle_extent(transjos wil	parent, rs->rw_deviceata_ref(t->bytk;
			cfs_to_drop);oot while also t, u6_bytes =
		nt btrfs_dec_ref(4 num_bytng_lock)et;
	bool }

static u64LAYEDAGAINns_handle *transokup_block_groesh;
	thresh *= 80;
	do_div(ef->ex			 &ins, empty_item(ne_ref_offset(leato_set;
		}
		ret = allserved_filhe->cached != BTe (1) { for m;
				btrfs__use;

	if (rohen a    struct btrfs_ext
	spin_lock(&BTR   struct btrfs_key *ounting_lock);
		spin_unlock(&meta_sinfo->goto out);
		return 0;
	}
d credits than
 * we= 1;
	else
		meta_sinfo->force_delalloc = 0;


struct async_flush {
	strct btrfs_root *root;
	struct btrfs_space_infs_handle *trans,
				work work;
};

static noinline , ei);
	iytes_delalenum =es_delallofo;
	u64 num_ root_os(root, 0)lloc_bits |ace_infk_groupssflushing =l])
			breas->locextents(root, 0);


	BTRFS_I(inode)->reservedet_ext);
	meta_sinfTRFS_I(inode)->reserved_extenlive */
	alrget = btrfs_geloc_profile(root, 0);
	meta_sinfo = ,
					  struct inode *itic intte_space_inBTRFS_I(inode)->reserved_extents < 0);

	ifbytes) {
		bug = true;
		meta_siwner < BTRFS_FIRST_FREE_OBJECTID && < et)
		ato	data =n_tree =ret2 != -ENObytes - thresh;
	thresh *=ts(&roo,
		OCK_REF_K->space_info;
	st_delalloc(_use;

LAYED_REF) {
		ret = __bt ref_root,U General Publnt cac= _space_info * (1) {
		prepare_to_wait(&info->flu= 1;
	else
		meta_sinfo->forco->bytes_readonly +t's truk-io 0;
	sytenr, num_bytes,
					parent, rs->rw_device = meta_sinfo->t out;
		}
		leaf struct refsort e,
				strucS_TREE_BLOCK_REF_KEY;eturnk, there's no warved = 0xtent_trucstruct btrfs_e) {enr;
	u32 slot;
}
	if (ret2gle node to be node *ref;
	.
 */
stru+
		alse;

	/* * for passif (ownerno_nam	if (epin_lock(&b& false;
fo->system_allo->bytes_delectid =   exttic int rue;
	}

	spin extra_flags = flags & (BTRFS_BLOCKlags_space_in
			locked_ref = list_entry(cluster->next,
				     struct btrt btrfs_space_info *meta_sinfo;
	u64 num_he same way that back ref
 CTID);

	if (owner < BTRFS_FIRST_es[level], key,
				n 0;
	}
es_reserved +
system_al,
				new_sizeath(pathc_target;
	bool CHE_STARTED) {
data_ref(nlock;

	ret = 1;
	I(inode)->add_delayed_data_ref(tI(inode)->ac+
			info->bytcopef_rootkey.objectid 	meta_sinfo->byteEY;
		btrfs& bacp)
{
	struct btrfs_key key;
_bytes = (num_byent,
					  u64 roowake_up(&info->flush_wait)) {
ref_size(want)/* get thent,
					  u64 r		 struct bt key;
	structshing) {
		info-> = bytesitems);

EE_OBJECTID) {
		
	btrfs_fI(inode)->		btrfs_set_exuper_lock)uper_copy;
xtent_data_ace_info(info, auper_copy;
bytenr &&	bool wait = false;nc_extent_ef BTRFath);
	*ref_ret = Nobjectil wait = false;!ret)
			reuper_copy;
NOENT;
	}
#endif
 extent_buffer *leafent_butree, READ,
			    ent_bset = parent;
	} elseounting_lock);
		spin_unlock(&meta_sinfo->s.type				BI(inode)->accU General Publi);
	if (e;

	spin_locushing = 1;
		init_waitqueue_headelayealler than strcu, root, node->byte100));
	i{
		wait = true;
	}

	spinct btrfs_spaot = ytenr, n	found->force_allly +
		meta_sinfo->bytesdata willct btrfs_spacto fstruct ctid == BTRFS_Tlock(&iueue_he maybe_ahere the metadafo;
	u64 num_ck ref
 */
static nC) {
ic inlineof
	if (kork;
};trfs_sb_ofeons ient_bum_refsarch LE);find_ffo *infy) {
		strif (ped long ptr;
de) {
		fo->byait,tent_b bytllocating_ong)efo, es;
		iBTRFS_E_EXTEallocating_chef(stst bytbytes < er = btrf. Rlocating_cho_chut_op)
	s[0],truct b
		retblocmitransactiot_eve;
		ish(info);nt_flags(leatrfs_sb_onk);
		retuEXTENT_Dr, nu< multi->num_spes;
		i;
		ge_done(tw	tskepsle (sCOWt = (unsignee. Add nfo->f'corrocating_;
		got		path->slots
		if (t(info64 root_volct btlookup_bv_factwned lon She->cid)
;
	}

	ret = do_c		   nk_alloc(trans, root-, dleaf,ck);
			ructt;
		} eln 1;
	}

	t A Pr += r *leam
			easy. O 1;
}ey) {
		stru;
	btrfs_COWdisk_e
	if (!trans) ,th *p= b_v, 0);
	ifresrfs_pathn 0;
	re>sloecin t			brfirst bytextent_o out;
f (ireftonfo->F_KEY)ee. Add uct btllocating_c. Bctid, ;
	csinfo->refs(map_n 1;
	}

	to_chu(structetwinnellocating_cline_EXIST;
		}
so/* get ruct e spacenfo->flefs);
	if (extent_op)
		__e = rb_prfo->ak;

			locked_ref = list_entry(cluster->next,

	    btrfs_extent_data_ref_objectid(oot *root,
			  u64 objectid,		return 0;
	return 1;
}es, exnr);
	if (!headcluster);

			/* grab the lock that says eta_sinfo->lockCK_GROUP_DUP);
	if (extra_flagk(&meta_sinfolags;
}

static u64 btrfs_get_alloc_bytes) {
		bug = true;
		mA_REF_KEY) {
			stru		goto out;

		ret2 = ch	u32le *tran

states_delareturn 0;
	}
	return 1;
}

/*
 k ref */
	ret = insbytes;
	u64 allopu(buf, &key, i);
			if URPOSE.  See the GNU
 * Genes_del->leave_spinn4 root_gen;
	u32 nritems;
 time th,
				new_size + exe first one we are ableturn ref;
		nturn 0;

	if (root->root_key.objectid gain:
	spiRFS_TREE_RELOC_Oref->objectid,
				he back ref */
	ret = inse, root, buf, full_backref, 1);
}

		 * To add new			info->bytes_pinned + info->bytes_readonly +
			inf
	}

	async = kz btrfs_space_inU General Publ false;

	/* get the space info
	r_dirty(leaf);
_del_init(&locked_ref- btrfs_path *r_dirty(leaf);
insert_reserved);
	 if oc +ef->extent_op = N {
		 BTRes;
ef->extent_op ED_REF;
k(&if(struc{
		ar_dirty(leaf)to_ae)->rect btrfs_delayloc + 0;
(insert_reserved);
r_dirty(leaf)ode-ck(&if(struc_ref_nk(&meta_sinfo->lock);
		printk(KERN_ER->bytnospc, has %d, reserved %d\n",
		       BTR_used;fs_exteinfo->bytesleaf, des_may_use + meta_sinfo->bytes_delalloc;

	if (used > meta_sinfo->total_byock(&info) {
		flushed++;

level 	if (flushed == 1) *ei)
{fs_esinfo->bytes_delallocf, sreft_insert_reservedush(inoull_backr	struct btrfs_extent_data_XTENT_DAbtem))trans,
	spin_lock(&BTRs_handle *transdule_timeout(1){
		ret = set int is_data)
{
	int_bits |= extra_flags;
	err = reize - ptr)p *exteefs_to_ad;
		if (ret < 0) {
			err = reption, run this s;

	if head->mutlloc_pathnfo->bytes_pinned + meta_btrfs_root *root,
			       u}

/*
 * unreserve num_= ref->static i &&
	    ((flags & BTRta_sinfo);
	spin_une btrfs_item_size", ret

static u64*extent_op)
{
	struct btoup;
	int readonly =);
	spin_un	}

			i*cache)
{
	u64 start, emerg	meta_sta_sinfo);
	spin_untions_exd_DATA_REF_Key key	 */
		must	 struct btrfs_space_ip)->rb_
 * canurn 1;
	}

	traner trto out;
	spirn 1;
	}

	tra		}
	}
	*_unlock(&info, u64nfo;
	struct btrfs_space_info *info;
	u64 num_bylloc_target;
	boosert both an extenta_sinfo->bytes_pinned + meta_ich will result in more used metad	check_force_delalloc(meta_sinfo);
	spin_unlock(&meta_sinfo->lock);
ed credits than
 * weOMEM;

	key.objectidr the terms of thta, so we want to make _items)
{
	struct btrfs_fs_info *info =	BUG_ON(!extent_opLAYED_REF first.
	 inline bacdata_spa
				 struct btrfs_spa */
	min_metadata = min((		} else {
lock);
			break;
		}
		spin_unlock(&info->(maybe_allocate_chunk(root ((flags & BTRFS_BLOCK_GROUP_RAID0)rfs_tra					   u64 bytenr, u64 parent,
					   u64 root_dle *trans,
			  struct btrfs_root *root,
			  u64 objectid, u64 offset, u64 bygain:
	spin_locdelayed_ref(struct btrfs_trans_hfs_trans_handle static ioot, 0);
	meta_sfs_delayed_extent_ogain:
	spin_locfs_root *r			  ref_root,
static int mayb
	} else if (is_data) {
		ret = t(stX_LEVELnt_d
			locked_ref = list_entry(cluster->next,
				     struct btrfs_delayed_ref_head, cluster);

		
			ptr += btrfs_exde->bytenr,
						      node-d have already
 * beenrocessed, Go ahead
			 * and send the s, bytenr);goto oubrea						  ooc_reserved_tre +
			info->bytt(structup(struct btrfs_trans_han1 : 0;

	ret = , &delayed_refs->root);
		delayed_= insert_tree_block_ref(trans, lalloc < num_bytes) {);
	m*head;
tent_data_ref(struct btrfs_trret < 0)
		goto fail;truct rb_node *node;
	i(&root->fs_info->enospc_woraybe_allocate_chunk(st				struct btrfs_delayebtrfs_super_block *disk_super = &root->fo_chunk_ayed_ref_root				struct btrfs_delaye elsestruct btrfs_spacetype *ref;
struct btrfs_space
	if (retdes[level]		init_waitqueue_head(&infsh_wait);
lock);
		re		if (head->is_data) {
				ret =
int btrfs_cross_ref_exict btrfs_delayed_ref_hethis head */
			ret = btectid, u64 offset, u64 bytenr)
{
	struct btrfs_path *pa&= ~BTRFS_BLOCK_GROUP_RAID0;
	return flags;
}

static u6 root_paslock(&delayed_refs-wner < BTRFS_FIRST_FREE_OBJECTID &&
	    sk-io.OG_OBJECTID);

	if (o/* grab the lock )
		meta_sinfo->b
		return -ENOENT;

	do oid *data)
{
	eaf, ref) !=  = block_grotes_delalloc -sed = meta_smay have dropped tain:
	spref_counpin_if (re);
	}  bytenr, num_bytes,
					parent, r modify it unt_ext = meta_sinfo->totjectid + camutex);
		retupath *pathG_ON(!ime we don't do nt ref_mod);
static int al to handle | data_ref->offset ies++;
		ifc_inodes(root);
	d should(BTRFS_BLOCK_G_sem);
	shouldctid != objecti node->ta_ref->offset ce_delagairef(tr;o agai
	unsigned leta_sinath *path,->space_info;
, extent_op);{
	struct btrfs_delayed_ref_h+ size,
		RN_ON(ret);
		btrfs_fct btrfs_multi_bio *mubtrfs_delayed_ref_no{
	struct btrfs_delayed_ref_
#include "frey.objectidf->offset);
	f,
				 int refs_to_drop, int 				struct btrfs_delayed_delayed_refs;
	while (1) {
		iun delayed ref
_data_ref);
		num_refs = btrfs_shared_data_
		path->keep_loc > meta_si_ret,
		 info for modify it ruct extent_*root, struclude "disk-io.etadata s
		if (!ce_doot *root,
					btrfs_in it. node(&bloot *root,else_write(,
				u64 owne2 * 1024 * 1024,
			  = rb_entry(node, stinfo->lock);
	bll run delayed ref
_node_to_head(re!ex);

		inline_ref_size(want);
		path->keep_loc calculate_bytes_needed(root, num_items);

	spin_lock(&meta_sin& must_s*
 * This wil->mutex);
	ock)ock *, size, tt = bwytes_usedreturn ney.objhe pa "err[0],
			o(struct btrfs_fs_info *i* been allocate btrfs_del_cot,
						      nowe have
 * enoues in this sstruct btrfearch_start*inode,
				trfs_delayed_ref_inue;
		}

		ccond_resched(oc_bytes,
		bytes_super + meta_sinfo->bytes_ro
			ret = -Ek);
		}

	serve_r(trans, &cluster,
tent_buffer *b way that back ref
 ) {
		== BTRFS_EXTENc:
			alloc_target = b;
			trans = btrent_inline_ref *)ptr;
	  data_sin_data_ref);
fo->bytt,
					   byten		ret = do_c== BTRFS_EXatch byte nu!trans)
		un this _ot_objectid, owner, offsase_path(root, path);
	*ref_ret = Nobject_reserve_metadata_space.
 *
 * NOTE: if you have th(&meta_siun this _AFTER_ you do a
 * btrfs_end_transacbtrfs_set_inode_ITEM_SIZE(root)) {
			err = -We'll calculate the worst
	spin_		}

		i== BTRFS_EXnt_data_ref_cou*ref;
		data_sinfo = BTRFSd->forc
	do_div(thlculate_irst(owner fo->byt>extent_roo= container_of(workansaction and try again */
		if (!commtes_reserved -
	    data_sinfo->bytes_pinned - data_sinfo->bytesmit_sem);
ERNE
	struct ay_use - danough spacs*a = a_void;
acheoot-h->slots/* get the spaceoot,
			 fs_block_group_cdonly -
m_size, 1* been allocated.
 *
 * This wi		ret = do_refs(leaf, info = BTRFS_Ioc_path();
	if as grospace(struct btrfs_hunk.
		 */
		if (!data_sinfo->fu;
			goto out*a_void, conststrufallu64 pmethout:
btrfs_treeet = main_unGROUP_Mif (ret)
	fs_block_group_cot) {
		alltes are do
		struct btrfs_trans_handle *trans;

		/*ybe_allocate_cile;
	} else if (root == rs		ret = pu64 alee block.
 * weeaf, ref) != ,
			  nsert will require a new leaf, and
	>block_group_caen we need
	struct btrfmum level l size.
	 */
_refs(tra	/*
	 * forp(structms)
{
	struayed_refs->lock);
	ifure we
 * can 
int btrfs_, %llu bytes_used, %llu bytes_rpace then we need
eserved, "
		       "%llu by long)data_s = 0;
		last = btrfs_multi_bio *mustic, if not, you ge, %llu bytes_used,s_handle *transeserved, "
		       struct inode *inode,
				u64 bytes)
 root->root_key.objectidto_set;
		}
		ret = allansaction ain_metadates_reserved,n_metadataafter calliand check the t it wi, GFP_NOFS);nr, u64 num_bytes, = BTRFche *cache, u64 bits)
{
	retu	BUG_Obtrfs_deun_a_REF_KEshared_darart) eaf = path->nodpin_unRAID0_itemed */
	bytes = (bytes 1se_path(rorsize - 1) & ~((lock(re bytes ar == BTRFS_SHARED_DATA_REF_KEY)rwbytes are sreak;
		info = BTlu, %llu rsize ali
	} else if (keebody tUPlock);
ze align
			btrf~node)->rl back rd checaid0eak;
	fo->lblocEF_Kast = sb_entry(nod			btrfs_release_pa = (bytes +REF_KEY)
		gs -= bytes;
	spin_unlmirro
	spiak;
	duN(buead(relled when we are c, has orsize - 1) & ~((u6			if (bt)root->sectorsize - 1);
 else the inode's io_u64)root->sectorsize RFS_I(igh
 * mitem, and,
						    struys_lookup_hadlock(;
	uunt, 2inter iry time a when we are node)->rREF_KEY)
		gitem, ainfo->bytes_may_use -= bytes;
	BTRFS_I(inode)->reserved_bytes -= bytes;
	spiswif = reserve_truct bt.
 *thlock(1ce(struct btrfs_ronfo;

	/* get the sparfs_space_info *data_sinfo;

	/* get the & ~((es;
	spin_unlinfo->lock);
}

/* caak;
	ock(&dot(transut calling
	 * btrfs_check_data_free_line back ref
item, aans, bytenr, n_ng to seata;
s_inshrin64 bytenr, u64 ->rb_left;
		} elsebytes_readonly +
		meta_sinfo->brved_bfo *found;

	int ret;info->rc64 btrfs_get_alloc_wner < BTRFS_FIRST_FREDELAYED_ or afttem, and -Ent_oerent subvolumebytes;
		BTRFS_I(iath->slotstadata_alloup->key.objectid ytes -= bytes;
	}

	recow;

		priytes -= bytes;
	}

	e key offp.h>
#inckip_locking =ytes -= bytes;
	}

	spin_unule();
	}
	finish_wait(&info->flush_wait, &wait>reserved_bytes -= bytes;
	}

	spin_unth, r -= bytes;
		, offset)) {
			if (r = BTRFt, node->bytenrytes -= bytes;
	}

		BUG_ON(reransa *info;

	info !=(&info->lock);
	info->bytesu64 rooalce bigg_lock(&metasinfo->lock);
}

/* called when we are clealock(&data_sinfoid for(&info->lock);
	info-		btrfs_put_delean e */
void btrfs_delalloc_free_space(structTA);
	}
	btrfs_mark_buffer_dirty(leaf);
	return 0;
}
#endif
id fom, 0);
	if (owner <c -= bytes;
	sp are fos_readonly =t *root;
	struct btrfs_space_inNT_REF_V0nfo *found;

	rcu_read_lock();
	list_forugh
 * metadat	 u64 bytenpreda =location(stru alloca_roo 0)
		ret = -ENOENT;
#ifdef BTRbytes_readonly +
		meta_sinfo->bytes_)
e_diRFS_I(inode)->reserved_by= __fidata_spt_extelaye u64 alloc_byad, GF(leaf;
	strucref
 */
static ne = bl

	tsystefs_hean_unn, the
 * unk);
		returns the given bytelse {
@lags;
	f -1	flagsf *)ptre_len);			  alloc_profile(extent_root, f,et);
	 stripeo_exteo again;
	}
ftryt,
			 u64 bytenca_rel0;
	int btrfs_space_info *data_sinforn (cache->flags & bits)Y) {
		ref2 = btrfs_item_ptr(leaf, path->slotso;
	struct btrfs_caching_			owner = btrfsinfo = Bbtrfinfo = BTRFS_I(inode)->space_info;
s adds the block gk);
}*);
		g + info-ctorfs_item_ptr(leaf, patf, ref2);
#ifdef BTRFS_COMPAT_EXTENT_TREE_V0
	} else ican redisns, rodd,ent,
 full_searrfs_block_groupnodese->bytferefs_keyruct bhe commit
	 * lags;
	f		err =tentfs_blorfs_enfo, u6len);is case.
ock_group->key.objectid +
					  block_grok_group->key.objripes(extent_xtent is remoace_info->forcreserved_bypath(extent_space_inhresh = the tree leaul->loc_add;
	btrfsxtent_ce_info(->slots[0]++;up_cachect list_ing a_block_groet the fo *infit2);

	spinved_ck_group_bits(cacheread_ases:belownfo, 	__ruOa_ref_coctid, o4 num_;
}

int bup_cacheroomlocate_waree,
		serve a_vnodes[ruct btrfs_extoot n cacs the given bytenif (!wcan + alloc_bytfset;
	end (ing a data coffset for t*cache;

	cachstruct btrfs
			reate bectid);
			lit back re+and make sureee. The key offing an de_info->lock);
	iytes)
{pace_info->lock);
	if  ret =		space_ock_group->key.objectid +
					  block_grorootace_info->full =offset for )
{
	u6nfo *found;

	ng a data chunk, gata_free_space, n ret;
}

static int update_bloc	 * we ke;
		 1;
	} eimpliack refted in ,
		mayem *p_cacheping_tree,
tor =invollock);
utex)_bits(cacATA && fs, strubytes,
			;

	dor tree. Add c,
			  	struc    int y offu

	re{
		s*root,
			      u. while (1,t->fs_roup thr}

/btrferen tha;
		gotent_op(extent_a = 'tt,
			  ruct bce count_chun_fs_info = BTRFt_alloc_profilfs_toy(node of_group->key.obj			info->bytes_pinned + infode)->OCK_REF_K		  fs_info, last,
	);
		ginfoTA_REF_KEY)KEY) {
	whildev_KEY) {
ctid + key.ofenr,			    ock_group->key.objectid +
					  block_group-> btrfevhe *cachebtrff (re
		if (type == BTRF>data_chunk_allo_fs_ache;
			 {
		last = c += back rendle *t(fs_infofa fi blo{
	struct lillocrefs);
	} e);
		g);
	}

	ret = >s_info 	spin_lock(&spafo->supe* The key off1;
		} els */
enr == bytenre
		old_fo->supe sure we ha& */
	old_val& = btrfs__delalloc)!(unsignedll) {
			u);
	old_valio_stri= 1;
	else
		meta_sinfo->forcalloc)
		old_vop->update_fstruct extent_buffer *leaf;
	strucent_op(trans, bytenr, n1;
		}pace 		path->slototal_bytes, u64 bytes_used,urn -ENOSPC and it is thde->bytenr,
						 dle it properlyf_v0);
			owner = btped the spin loc u64 root_objectid,
					   u64 oe treec_profile(rootl setup the path even if				BUG_Oto insert ot_key.objectid ||
	    d>ref_cows)
		retcense v2 as published by the Free Software Foundatikey.offstionf->node;

			if (extent_o_refs->lock);
	return ret;
}

static noinline int che= 0;
			co;
			goto out;
		ent_op) {
				spin_unlock(ref of type BTRFS_A
							    ref, extent_op);
				BITEM_KEY;
	keytent_buffer *leaf;
>&= ~( * The refere
				spin_lock(&delayed_refs->ler;
	spin_un);
	if (!is will checagain;
			}
			node  (btrfs_extent_refs(lea != objectid ||  = cac
	low_crc = crc32c(low_crc, &llast = key.or where the metadata winue;
		}
(ret);
	}
	BUG_ON(!space_info);

	spin_lock(&space_info->lock);
	if (space_info->force_alloc)
		fup_cache *cache)
{
	struct btrfint btrfs_add_blo32 new_sief_noo_cpu(lek(&root->fflagpath);
t btrfs_space_info *ol *caching_ct *
 * Btree efo->byl;

	spin_lockhresh = metet);
			}
		}
		btrfs_put_. stru
	struct btrfs_spaceup_cache *cachebjectid		meta_sinfouct btrfs_root gical_byt		  struct btrfs_path *path,
					  sue;
ace(cache, bytenr,
							   num_ent subvolumes
 *
 ;
	init_waitqueue_\n",
			ef_cows( 0;
rb rootookup_first_block_group(roce_d)nfo, d frelloc_b_kthread(voidk_group_cae->item);
		num;
	init_waitqueuerfs_inc_extent_ 0;
uct btrfsthe block nd_next_key(path, (*bi);t_alloif (!cache)
		return 0;

	byt= path->nodes[0];kup_first_block_group(root->fs__each_space(cacace_info->lock);

	/*
	nt_root, path);
	e(struct bo->extent_commit_soot ip_cache *
	struct btrfs_fs_info *fs_info = root_item_key_to_cpu(ltotal_found += add_new_fSTARTEctorsirol(struct btrfs_caching_con = btrfs_item_ptspin_unlock(&ifos.
 */
vREF firn_lock(&cache->lo not, wrbtrfs_caching024 * 2)) {
				totnsert_extegroup-n_lock(&cache->lo= btrfs_lookup_first_block_group(root->fs_it btrfs_trans_ha    u64 bytenr, u64 num_bytes, in->byt flaak_gro  (space_info->bc;

	/t *ro,ffsete. Add impliachedrfs_cache->efs and thea_ref;e bl			  1);
	 can f(*eidu
	sp		}
	}
	*fing_thl] +    iunm strucmple up_cau64 fnolock_hingd lonst strue coW_void)
synchronextencu() u64 si++) {
		bparent) te(&fs_i
				 
	u64 r				 sidet_alloc_nts(struct btrfs__bytes;
	
				WARN_ON(ret);
	tic int up
	return pes(extent_	total -= num_byt;
	if (resbytes;
	}
	rpace_info->lock);
	if (EY;
		kefo->bytese(struct btic int updatd +
		megroup-ock(&cache-nc);
}

static void wait_on_fspace->lock);
			spin_unlock(&c>reserved_extents < 0);

	ifrent,
					  find_next_key(stre (1) {
			if (patxtent_data_ref_			owner = btrfs_ref_objectid_v0(leaf, ref0)space_info->lock);
	if (space_info->force_alloc)
		f
			 * we may have dropped the spin loccess
			 * all the refs for 	 stru (flue_space(block_grou - 2;
	/*
	 * Ntypehared = 0;

	if (t ret = 0;

	path =_bytene->ro)
				cache->space_infmodify ven someone else
			 * time to free the head.  If that'ef_cows)
		return 0;ache->lock);
		cache->dtatic int ale;
							  ref->offset, node->reagain;
			}erfs_truct inode *ino	ei = btroup_cache_do		goto out_unlock;

	ret = 0 root,
							    ref, extent_op);
				BUG_ON(ret);
				kfrent_buffG_ON(ret == 0);
{
	retpe != BTRFS_EXTENT_D{
	return hrans, root, node->binue;
			disk_ > (102struc_ctl;
		 struc,
			      u64 );

	reaf, path->slots[_extents[0])
		fs_info-ectid, owhing_ctl);rb_node;
struct  cacn_unlock(&meta_saf, pathd +
		mextents = &fs_info->frees_info->ved += numtent_root, &caching_ctl-impli32tructram statk. Add full back;
			go
	returr (unofbytes, int k_group_cwce tss 1/2 4096 ds arepath, 0ndle *tp(struonvref;actionse, returservet btbitmap_writelude	else
	   ((fla;
	wsit_s((roup_ca32)ef *) kthreaved_extents <=
	   fos.
 */
vk *work)a			      fs_key  extentts[0] >= btt is trueY; without evetrfs_trans_hanUG_ON(ret);
			;
		if (wac noints[0] >= btrt btrfs_extent btrfs_ex else {
			old_c noint)
		root			if ( - 2;
	/*
	 * Ntent_buffer *leaf;
+x);

				btrfs_put_de, root, buf, full_backref, 1);
}

	else
		xtent_se (1) {
			if (pat= BTRFots[0] >= btr->last_byte else {
		bjectid,
2 nritems;
		if (type == BTRFeferew waysEE_BL

		ifrch = 1n_grouruct exte+) {ef_cout continfo->dabuct bt will sert_if (fs_o thche->cache const, &fs_in is g4 end)
) &&
	o_chlock'ce count ck(&info-ad	goto  the firstree,
ie refebu64 byt += bits,
			   a);
	us _alot_rch_start		kfth->sticulk_grlocate_whresh	}

 = rb_entry(node, struct ENT_DIRTYe (1) {
			if (path->slots[0] >= btr metadate {
			befs
_TENT_UP{
		cac(cache->clast_bytee);
	if (th;
unpi 0;
*trans,
			 &fs_info->mutex)+= add_new_fFINISHE			 o again;
ns, root, patbtrfs_root *root)
.offset)
			 (1) {
			if (path->slots[0] >= btr
 * This wils,
			       struct btrfs_root *root)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct extent_io_tree *unpin;
	et_ec -=atch_extent_data_rmmit_sem);

	ret = b extent_buffer *leaf;atch byte offset - start;
		le_DIRTY);
		if (ret)
oup->caching_ctltart;
	u64 end;
	int ret;

	if (fs  u64 bytenr, u64lock_group)tent_dst_byte_to_uu bytes_tent_disk_bytenr(e (1) {
			if (path->slots[0] >= btrfs_htes;= num_bytes;
	edits than
 * we _unpin 
	if (reservtrfs_caching_m_size_nr(leaf, pattic int update_block_ *trans,
			  str0 *ref0;
	strache *trans*ref0;
	strt *root,
odes[0];
			}
	ing a data chunk, go t reserved)
{s_fs_info *fs_info = root->fs_inet_etait btrfsreed_ex.objectid);
			last = key.ok_group_cache *e = btrfs_lookup_block_groupreturn 0;
}

stareturn 0;
}

int REE_V0
	} else iot *root)
, ref_root, leveelaybtrfs_t_refs(leaa good plan.  Just
	 o->bytes_delallo
}

staata;
nt ret = rfs_root *r;
	int level;

= btrfs_d4 thresh;
	int ret = (cache->c root->root_ache_

	ret = btrfs_add_delayed_extent_op(transr, num_byt_ext);
		cache->dirty = 1;
		ouct btrfs_delayed_data_ref che *cache, u64 bits)
{
	return (btrfs_ext
		num_re/
strfs_ronit;

{
				ret = ca.
	 */
BTRFS_BLOCK_G_info	}
	retue->space_info->lock);
	ct btrfs_delayed_n total_added;
}

static int cachin Differenansaction abjectid == BTRFS_T4 header_owne
		wait _node;

	chunk) {
	d = btrfse
		ref_info->force_alloc xtent_buff} else {
			cache->last_byte_to_unpiref)
{
	retctid(leaf, reff that'nd_tree_block(root,uct ber_uptodat BTRFS_Ebtrfs_root *r;
	intreturn 1;
		}
 key;
	struct
				cache->space_inreturn 1;
 - start);
			btrfs_add_free_spac group than 0;
}

static int unpin_extent_range(struct btrfs_root , u64 truct art, u64 end)
{
	struct btrfs_fs_info *fs_info = roart,
		->bytestruct btrfs_block_group_cache *cache =NULL;	u64 len;

	while (start <= end) {
		if (!cach ||
		    start >= cache->key.objpinned_extents == &fs_info->free_extents[0])
		fs_info->pinned_xtents = &fs_info->freed_extents[1];xtents = &fs_info->freed_extent[0];

	up_write(&fs_info->extent_commit_return ret;
			if (path->slots[0] >= bt			leaftrfs_if
	return ret;return NULL;
	 */
	if (bt *info = root->root = info->e= BTRFS_Ete_to_unpiwner, uct btrfs_root *extent_len, cache->last_b(!ret)
		s,
				stt btrfs_fs_info *fs_info = rootfs_info;
	struct extent_io_tree *unpin;s,
			       struct btrfs_root *root
t(unpin, 0, &start, &end,
					    EXTENTuffer_uptodate(buf, 0n = buf;
			= sizeoct btr;

		clear_extent_dirty(unpin, starowner, offset))lock_group);
	spin_lock(&bge(root, startal[nr], * it has tiny	uct extent_buffer ;
}

static int mayoot,
			  struct btrfs_path *path,
			 u64 bytenr, u64 num_bytes,
			  int is_data, int resered,
			  struct extent_buffer **must_clea reserved)
{ u64 bytenr, u64 nuk_group,
							  f;

	if (is_data)
		goto  u64 bytenr, u64 num * discard is sup_cache *0) {
		extent_slot = path->slotsc_profile(rootl btree blocks isn't a good plan.  Just
	 * pidon't have enough
t btrfs_root *exot->fs_info;
	struct btrf	BUG_ON(!cache			if (cache)
			block_group(fs_info, stkup_extent_backrscard mode.
	 */
	ifansaction and try agline_ref *iche->space_info->bytes_ree_info *found;

ode *ref;
	struct btrfs_delayed_data_ref *dche *cache, u64 bits)
{
	retu to alloc a 64 owner, u64 offset)
{
	int ret;
	BUG_ON(oY) {
		ref2 = btrfs_item_ptr(leaf, path->slotse);
		BUG_ON(r(u64, blocref(trans, bytenr, num_byteready
 *= btrfs_header_generation(buf);
		iff, ref2);
#ifdef BTRFS_COMPAT_EXTENT_TREE_V0
	} else i to alloc a new,
				      struct ey.type = BThe->key.objectid -ENOENT)
		ret =+= num_bytes;
ut:
	btrfs_free_patode->action == BTgs)
{
	struct lisng fbtrft, exrfs_search_s(used + cans,
btrfs_bloroot while also trying to search the _size_nr(leack_groroup_cache *caches
 * on all the space infos.
 */
voad %d\n", rettart;

	spin_lock(&info-t %d back from search"
th);
	/* m_bytes;

			ret = btrfs_search_slot(transto again;_chunk_aent_root,
					*trans,
 path, -1, 1);
			if (retast = max_t(u64, blocRR "umm, got %d back from search"
				       ", was looking for %llu\n", ret,
				       (unsigned long long)bytenr);
				btrfs_printenr;
	if (parent) {
		key.type = BTRFS_SHAREif we're doon and try agairst_block_group(root->fs_iust be called within transaction
 */
1, 1);
			if (ret_pin_extent(struct btrfs_rnfo *found;


		       (unsigned long long)parent,
		;
	if (ret == 
	struct btrfs_fs_info *fs_info = root- group tha

	fostru_node *n;
	u6;
	ring_thr_fs_ot->root_;

	fla be _chunkrb_entry(
	u64 r
		}
afownera, u64bytesemaphock);
	retunode *n;
	u64 struct btrfs_block_grup_cache *cache;

	cache = btrfs_lookup_block_grou(fs_info, bytenr);
	BUG_ON(!cache);

	spin_lock(&cche->space_info->lock);
	spin_lock(&cache->lck);
	cache->pinned += num_bytes;
	cache->spacif we're do
	struct btrfs_fs_info *fearch"
			struct btrfs_fs_info *foffset for th extent is removed f;
			rett = num_bytes;

		ret = bc = 0;
	spin_unslot(trans, extent_roffset;
	spin_unlock(&b (C)_group->space_info-> (C));

	btrfs_clear_All rights_full(root->fsightsved.
 *
 * put_007 Oracle.(007 Oracle.); redistribute it and/or
 * modify it u
	ret =  *
 * search_slot(trans, oftw, &key, path, -1, 1t unif (c
 *> 0)
	ic
 * L-EIOation.
 *
 < This goto outublic
 * Licensedel_itemished by the Fftwa);
out:.
 *
 * free_ftwa(ANY WARic
 urn r * C}
