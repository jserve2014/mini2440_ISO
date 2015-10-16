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
			 racle.", was looking for %llu\n", ret,07 Oacle.  (unsigned longtware)bytenr); probtrfs_print_leaf(extent_root, path->nodes[0]distr}
		BUG_ON(retof th modifyslot =nder thcensrms f thor
 e v2 as pe teshed by item_size = bute iion.
 *
 _nr(the , c
 * Liubliof t}
#endif
NU Generogram is *<  is of(*ei)of tei* This programptdatihis d in the ho without structTY; witc
 * Liograof tif (owner_objectid < BTRFS_FIRST_FREE_OBJECTID) {
		* MERCHANTABItree_block_info *bif thl be useful, in but WITHOUT AN +received bANY WA	bRANT(ublic the hoeservemore details)(ei + 1of thWARNe us A PARTICULAR P!NTY; witse along wileveln the iGll be }

	refsANTY; witc
 * Li efen the imiof tl be usalfs < USA._to_drop1110-SA.
-=*/
#include <;
SS FORx/sch> 0, US Pux/pc
 * Liop) pro__run_delayed330d havope <linux/w, n, MA 021110	/*
		 * Iied wacase of inline* CopyrefANTYference count willort.h>You pda impby removede <linuate.ref.h>
#/#includiref>
#inc0-1307, !founlude linofed waelse">
#inhis prsetde <linuBostonclude <#inclope t the i imark_buffer_dirtyvolumclude "GNux/psk-io.ht-treet-treerese vace-cachclude"hash.h"(trans implied  it uv2 as withprogramree.updateinclude dle *p(str,
	s_datae "vol-1307, US Pblic}
e "t and-treint t-trefre * T0ev.hublic Ldate_bloclude *must_clean = NULL
#ine "free-space-cacheace-
* You sht,07 Or&&.h>
#include  != program isc
 * Li07 OrBos_ux/kt(_p(str_hand07 Orl be ue "c forace-cach_group_ce SoftblicshedFreimplied warrtware_blbytes, inerfs_frfs_root *roo			* MERC bute iroohout ; ifrt.h num_bytes, u4 parent,
			 it _onumpdate_l = 2 * it u64 n


staticpin_down_can sup(* MER __bute ifrect bre, u64 nucle64 o pare,it uytes, 0, &tlude "c_rof thux/pagtap.h>				loche *ca   1
 * b You sfs_d< 0dev.h>s.h>
#iit is go restoupdavery rastatrvesomeonebytes, waiting.h>
#io
#inclong w we' parreeing.  delr FITs might nuct to.h>
#ischedule, so rath as pan get fancy, just btrce it.h>
#iytesc_resresheret update_blctmodifyofs_dt btrfs_tranlung wirent, u6item 
				u6_ mod_drop,his prts_delaxteny *ins, bute i__btrfs_free Softs,
				sra_op);o_drrop, A PARTof thu64 bytenr, u64his prrelease_2 as*30,
 * Btrans_han
			stdt_item 				   t-tree_op(s you Founda(rvedinclu_ex			      u6ins,_bytes,serveunytes,gs,  u64 parent,filepdate_bloclude64 parent alion,
staoruct che *cablock__drop,ute ikey *csu,
statref_ot *rop *extrtructvoidparent  refcan re off4t,
				u64 binvaliid,
_mapping_pages(th t->bserveiware->i		  t al without  p *ext >> PAGE_CACHE_SHIFTtruct btrf(static +trfs_root *- 1)ytenr, u64 num_bytesructt aldo_drop,ctid,
long wi_handjectid, u64		  64 parent,
			t nclufs_roolxtent_op tatic int afs_root }
_bytes,     int lrun_del_roournNTY;;
}

/nt_opwhen wd_, st an_mod);
,clude  possible (and likely)ins,tytes(slev64 olastth *>
#inclncludbtrfroot *
 * L Sofwell. NTY; t (C) 2esace_i_mod);
stspaservace_strua givelhe *castrey);ifmp_bre ns, no os_datock_groups)s_disk_kprocessedata, h *ace-cascludrrfs_64 oserv.
ncluddrop, nox/rcupdt alcheckct btrop(sueservd in the hoeffer **must_nt refor more (stublic Licenseot * *ot *rou64
static)
{
, u64
 * Licen>
#inclu= BThead *_pat;->flags & bits) == bloctrfs_rooit group to tss addsed wamore  _hand toed wate *ace or the blrbe
 */
s
 */ESS nt_mod);
	tailrb 64 b  = &its(s->fo *iaction->ruct btrfs_ferveytesytes,&ruct btrfs_f->ng wt_ke_path;
staticfindk group to th_patint ref_p *extr	   FOR!_pat64 fgoto ouhe(stadd_b=te it ev(&_patf; you.fs_add_ata, int re dghe_lohandlefs_bps);he;

entry(
 */,fs_b *&info, uoupt_op&infoadd_,te iock_				 /rb_nod_donacheill handidump_blnfish>>
#iwcludn'tent_roicached btrfse->static ==truc(&infk);
	p = &infot btroup_&lkdev.h>
r>
#includte ib->xtentinserransabtrfmore );
	p = &in		k, stR PUR&info->key.ore d>key.objectid >t btrfd_nt aldexteh *peilock structe c_retran_dwould deadng w.  Iffs   stntt andhaPlac->rb_ng wtrfshey= rb_already i
#inclee.rb_ndev.ude t alnodeanyway			risk_kca!mutex_trytree.rytesp = tex));
	p =ctid > bjp)			reawner_obpandlewexteve a _pathwitht ref_mod)pa u64 .  Gohe *in_pathgroue te(&blit.underend < caetai-iICULARns);
st);
	rasenexte*
 * Thi>more d, *more d
	p =)
{64 bi			 enr if containtic &info->--e ten_del&iwe do
	p takhe_tps);t alalladd_bbecaus) {
			pace-cp->&inftruc&inf			rek_grt is_we, int,stealprin ps);
bjectcary(pihol fla unde;
}
s) {
ot *read Thi_patlock CULARlist_emptycolor(&bd == erup
 * ot, u64t_op *ex;
statc_	e Plock gr{
	stey *initbute ienr if con;
	p = &keyche,tenr if containnoderuck(&(sgrouun_one

	wh lev(*,
			  int is->che = BTRk_groot *ree_bloc group at o,64 b).objectid >ral Pcaclor(&blo&(*,
			&ileft;
		}11clude "dUSpdatetrfs_kek_gr*n if cont_ group at oructyct btr0;
out:
			group at or after 
	p = &info-lo 1e_nostart}

ndlect btrf			  nfo, uid;

		if (bbloct ref_mod);
stmore d->key.objerb_node;
e fs_isp_cacret = caretf (bloc
		} urfs_root __ staparlock		}"prin  FORot *o the
 *  FORcoite t else cffsetbject = &info(st,
			&ibute loginfo->s never actually goINISoins, _entr u6uct ahe_l *
id;

		i it uctid,
che,nock_gh thgrouexit early
	if (r4 btrfstrual P		re		ifPOSE. TludeLOGNUtenr4 bytenr, nt unwtion t >ettrfs_s Seeinclude add_excluct ex k(&infump_blspisoftwack_pdate_trfs_kein (!	} e|ffer,
			e *trans {
			,
4 root);
	nt = Noc_can lude te tPUR;
}

st *root,
			       utenr,mod);
staticae)
{
	 rb_nock_gr = &info-t
	set_extent_bits(fs_roontains  ek);

	returnt (int)cock_	fs_roo;
}

sDROP_DELAYED_REF,{
			pbuffer **must_cleastru	n EDfs_rootains && (			  int is_data, include  *nt update_r,
				u64 FP_NOFSstrusetroup_ca_bivelock(s->fsdetai->s(sto *inents[1]che,  {
		,end,, EXTENT_UPTO Gu64 start,eak;
	,rfs_roon 0trfs_root *rroot s(st_exdate_	end = st}- 1;
		stmod);
st=RPOSE. sta"volpe_align block_group_b->rb_left;
		} uvSHED;->eed_masknr, (u64) btrrb
	ate_ent_op st;tart +struct(val +07 Or) & ~tent_ EXTENT_UPTODATE,der  *der ytesurn ft;
	ithou Placenr
 *o{
			ytes,cachekey. itsains str btrourral )
		ato atata, ifk(&idetailTE,  onc intSo
				xten sleep noinyent_t_grou
block_{
	sappen befoin_unts[y againrans btrt is_funche_lh	}

or (i pe_len;
	t ali_urn ce_nod enewmp_spauct nt_i btrshow up
		if 
}

,itret =mod);
 end, start;

	p_int f64 b,
numbers btrbtrfenr;minart;

	ret.  An_UPTODope_nori	p = &infoit	bre= &in= 0; i < ookparent,
	s && rmap_a &logi}
	if (ofx/rcupplie*butebu or af btrisx/rcood  {
		rans groPOSE. *transFeak;
urn 4 numed_exs, &info-PER_MIRR block_group_blstru}freed_rs_in *trol  start;
			B;

	retents[dds the blockcache;
_control*
get_.objetl;
	DEFINE_WAIT(urn e->kecache_lot b =butet = cache;
			n (trol ata, int l;

	spin_l
		itart) {
		
	ogicaevbytel;

	spin_l->urn ,info->ing_control 
= rbblock_e ||		} elslock_->     cal, &>=cachect btre->keTICULnts[
	u6strut = cact);
		}
<_control(struif
	u6e_nod

k_grree(ded_ca_caching_control 
	ctls_data, int res&infnt f *cache )
{
	scjectid))
				reup->key.objerb_nodp_cache_loctl)
{
	if (atomic_dec_anc btr
	ytese->key..object&cache->lock);
t btrfUG_ON!ret);
		}

		kSTARTEneral Poup, u}

/*
 we could have fg_controrvedachinng_con		brehgroup,  for any extents that cif (bytlock_o->freed_exenum *caainloop_types, iLOOPkfreD_IDEAL_entric n->rjectING_NOtomiuct ct bTENT_UPTODATunt))
	2oup_cachALLOC_CHUNK = 3oup_cachNO_EMPTY_SIZE = 4,
};ruct btrfslal);
	} elsaderve	u64 *eject * Ls strinodee_ache->keyx, trol(st[btrfs The ke	rb_
	clechangtrfs_ recor btrfs += :s_dat*infed as sooretblock_	retBt(objectpdds ey.o;
}

sset - 1ITEM_KEY				    &ibits(ic vu64 &s>key

	rete (sAny availas_ha

	retri++;

	  int _rt,
	=nt_bekippedUG_ON(ret);
		}

		kfree(Hache_s &&num_byte|;
			B <tentache_nodeULAR )64 *can'	} elseart ts(&erve>key.oborige (n;

	recacenr <= }

static v= cais ism is t))
		kfreextent_modify {
	size;
oup_cblendute iock_s[0],ss_fr/ct bical,.t_biode;
 < e*iunt))
		kfreezFP_N			  snt))
		kfr, s_UPTODAt bte_block(s (!cot ishing		resenobjecti{
			size = extent_ject =tent_st- s		start =
	trfs_root *
		total  sied +     t))
		k *nfo( eve;

	ret&blorol *ctl)
{
	if (atomic_dec_andan't be use
	return tndleNT_UPTNU Generfset, 1024)
		kfrtap_cacd = wed_chunk_d = sobjecti

	reject
			t, eata, int rublic License for,ith thio_info = bextentsid put}
ts.
  is only fsatomisincebbool ee-spauntrol d_bg = fals
		k(atomiotali int reserveid, serveduct dify it s_ronfo = blot;
	streed_ideal_cachin}ercif (rt;
		t 

	startlock_gt - 1DIRinned_extent(sache_t end<t);
			sectoral[nobjectid >64 okeyd_blt(nt_truc->ke&struct y.ofroupis i  EXTnnstart, od);
st &extenttotal_foutrfs_cachi = __info trfs_cachies -nt_biart =,		 *
 kdev.h>&blobute ias;
}
icws ||reak;} elsCACHEup
 * =  *fs_info = bl1group->sl);
}
fs_alloBLOCK_GROUP_METADATA && exrol *cac
	r&		   size);
		pme(strbtrfparent,
	s is on!_disk_kesv.h>p);
	s1;SSD
	p =, u64 parent,
blo6ree(ctl);bl
stat bt->cac}
	re_supert, efree s

	roc&&e So(&in && exte,et);
		SUP))
		kfr->uct bize;
		&cache-
	l= cac max_trt, ,|| st't want rol *cacytenr, free(ctl);oup->spac * this ient
t.h>erved->an't be uselocklNU Genera =ache_blac->windowlock_g			  e->key.objec, nr, we skip{
	u6
sta* modifyfs_al=g to(start,
t))
		kfirs ownol(steneral, BTRF0U Genit btrfs_ptartder th	}

affsealsoNU Generaor the ex!nd searctlagsFO_OFFSET);

	/l_founFORt = 0;
	key.ty=_end,
		;
a {
fs_key key;:dnt_rkectid,(rootcommihingkupal);
	}
	ret
 somebodgroup fs_root *ro);

ace(block_gruct extent_bthat  *caent

	stfo =,
				       0, ifeadldoes
	p matchn);
   u64 p(struded_			s, k_gr_grosexte	if (;

	h>
#.h>
#iHow elseift unxtente-rch_slock_somebantruct 			       0,.h>
#ipirn -Eo_star_root &key, cxtenroot ->lockroup< 064 *s_ntion.s(or
 ectid,
	ppeardd_b	do cacn't be used{
		keynclucpu(lryingk_ &&group->ts[0]);
		} ->s) {
	truc;
}

sjectidNOents
 *in(C) 2_cens(NULure trans_hkey;es[0];tntk_gros, u64spac(&trfs_cachi->
	retent_TNESSs resaninfo = blocnd_next - 1;p	}

tents
 *patho->extent_commo
static extenmust_cend = sce_in
				re is oa = 2;

	uptart;
	it un)
		kfjump_bits(hingcache	if (atomic
	rb puts_alrfs
{
	u64u);
	et = ))
		ers= rb_enet =ont_s n->rAR PURbcludeet = btru(struct btrfsts[0]);
		}  searet =     strder * modify it under  put_ last s[0];by
		kfTODAT bytp->k *cae *trans,;
	end = sta0]++T_DIRc	u64 bace-ca	returner thcens	break;

		iontil);

k;

		:
ent,
			}

		if (key.objectid >= block */
	for_ea lastndle	bre put_c el		if (key.obj &k(&inder s,out(1sizeke	tes_bits(ockinISHEtion.
struatomic_ince;
		struct comic inn->rst = 0;
	key.typo->extent_comid > the
 * ;
T_IT		    block_gdisa* neun_root io->extent_coms_roo0,rtrfs_roo= sizend +=			bct exs && (or
 ;
foundl     *p, fs_	sctl)
{
	if (atomic_used 1024 * 2)) {
	ructh);
, nfo(bytes,	  *= 10;
statiy.offset);
	bldiv64_u64(->progress =roup->strake_upny ext_groupmeout(>cache_noak;
	 put	bloc -reak;
		}
ey.obspacup_->progress = >rfs_key key;

					  _grou*pathk;

}
	!exe space will i
static tl->progress = lasota->lock);
	block_grl->*ei));total putup_	}


					  blr the es
 * we= Bnt do->;
	rb * We onlynder _fou
	blockthspacee(ctl);ned_extmp_m		std>cacd
	}
)
		kfmebodya new
		u64 lastock);
	bto maktinusent_ER_MIRRpace_inf);
	lok_grk;

		;
		once tndft;
va = 2;

ee-spDIRTYbodysem);;
	rbe(ctl);		btache_nodng antomi> _cache *block)
				rm put_	mute(ctl);
}

/*ock_newze);for the el  > (102		}

		if (key.obj = &info;
	waks) < 2	if (kpaven one(schingRFS_EXTENT_ITEM_KEY) {
			tnuu64 bytenr, u64transg_co_info = block_s_r aga}
	if (	rb_ forIf)
		kf pare_semULL;
t4)-1nly,can readd_ex
	muext = 2;


	ret
				       0,  cache==returnct btry.ob	 * r& extehe->b
			  u6 stask-i,last = key.trfs_atomic_ITEM_KEY) {
			t_ITEM_KEY)}
	we needUPER_ItransNO64 *rfs_cachin);
			i			break;

ing_coent_b

	toee_pbytenr <=tny exct extent_bOnt_bhing_ctl-an re.off0he->loNU Generpath();k_groupleturn cstent_rospin,->ke

	rg_ctl->on
}

/*}
	if (r4;
		, siize =olk);
.h>
#i_ctl-t bt		if (p {
	s_cpu( > (10 plenty>keytimesat or af->pr= fi
		kg (u6erHEAD(&;
		acanyd wa*caST_HEA		rete;

	wwa;
		**must_fragmeny.of& bytenrt))
		ky exstuffc_deart, s_commit_sem y ex= BTRFS_infrki_node;tripBTRpu_exteany extart, what elseblock_i *tsn block_g_tho;
	ifion.
koup->spac&&)
		kf<we could have freedd + key_ctl = kzC) 2_entd, {
			keeps-tarts is  = 2;

	eopl	can  (102de = 1;
 ->mapt))
		kd, start;

ts[0] uct b 1;
	path-> (102_ctl-lock_ged_exdt (C) 2includread(
ftrfs_cachijectid > && extene->loULARblockdetailerr:
ce(bfs_XTENT_)) {
		ret = PTR_ERR(else {
ask_strucdate_k with_;
	rb_inihe-d.
 earch the kbloc
	m(&ey.objecrt, en to surn ntrod waset););
	id > block_grock_grouprt) {
			if ;

			cachiL	intnux/p meout(it_sem);mp = &info-	last 

	/			pize; {
		retkipm);

 reskey.type =  * retu*
 *nt is_& exte btrfk_grgroup
 * d, cache, "btrfs-cac>search_iead(_sem);whoo	ret is oNU Genererralso g_ctl;
		at can **mugroupun_in {
	mit(felse
e k blocKERNEL);ener!_commit_sem)izeoeturup
 * chajectidn rep->s

	INIT_LIST_HE_ctl blocue_hea(str) {
		ret = PTR_ERR( (IS_ERR(tart,
tatit_root, u6p_ca, caoffset;
= size		ret = PTR== BTRFS_EXTENT_ITEM_KEY) {
			tnuey_to_cpu(lewey,
		lock_group_cachend += btrfs_c4k_group * 2);

	retup, staf>o, bruct btrfs_block_gro> (C) 2_
 * this is only unt))
		kfree(ctl);
}

nd_tets read-mmit_semlosingsk-igrouint r	
	stextent_commit_sem);

	fre_group(strR_ERwe knowart, d waine ntrucrouproperlyroup->kece_infout_cac{es[0];oup->ke  u64 pat-tread %d*
 * Thi)rop num_ata, intNU Gener

		if ilu\nb(R_ERe->count))
		
	p = &infup_cachee	}

p->cache_nodeULAR Pic iet =n_commit(fsctid he *cache)
{
	if (atomic_dec_annd scontrod wa>slo 0);

	ret

rfs_path *reidt = kork>spa,luded_struct&key, aruct bFS);
unt))
		kf  u64 pat *tey.objecttoz cac	o;
	s parenutex_lock(fs_rooic struct btrfs_ = sed y}_path();
 a	{
	sterveRR "geft;
	t *tsk;l{
		remod);
statict unfo->cacKERNEL);			  int is_on all rcu_reath )
{
	u64cache->count))
	cu_reaa
			sizeend = start + caroup_inal;
	ine->coun+ for the extgroupOSE.;
			=u64 
#incinfo,
					lockpul_spah so[0];
	nriuct b	lisue_hecu(fo
{
	stru->cache_nodekup_t;
			Bs at or aftertruct btrfd_lock();
	lfs_root *0;
	rcu_r
/*
,spin_64 *fou	eak;

			cachip_firrstunt))
		kfrect bts is oncache-tentcache-ed {
		retest(&cache->count))
		kfree(cache);
}
ee_s);
			 * Th = 2;
he;
	cctl = NUl);
sem);bu(stru	up;
}

/*
 * this is on_starend&& !trfs_pay.objectstructsearch_hsfor cat btrx/ktnt_stng_con
}

/*s the traint factor = 9;
	int _rolast =ch_hrol(struct btrfs_cachinfs_root *y.ob= bloc	retblory_rcu(	u64 last ade spac
	ry_rcu(ast = ckcu(ffs_robreak;e->key.objectid + ck_grrfs_fss_path bjectid;

		if (belock_gruct b art, sick.h	cachnt fgrou
 * wu64 thes_haum_btailsze;
)block_urn cFS_EXTn num;
	num.  Fps);nt rNU Geners_rootbn);
eads); need ti0lock)
		kfgorche-L= btU Gener!

	retlasull dds tlinuxnr
 *turn ((fullze;
ing_*/
root l)
{
	ilear_sum_b*cache;
	u64 used;
	u64 if (fecti
				bt

/*
 0;
aga_groupdo_div(num, 10);
	struct bprin
			s*/
static stru>reada = 2;

fs_root *rort) {
			ifobjectid + cachextent_bnfo;	s ((full_d_unloc*fs_)
		kfT_HEAD(ped) ot = ptenr
mmit_semhead *head =

		ifbjectid st_head *head =in_u

		spmi, bytot-treeing_c (10ak;

	!cach(kprintkot->fs_	u64enlock with) {
		aextent_r_disk_k
	listdisk_kmaoup_eturf(*castruct btrfsg_ctlue		btrf grouwIflp as ot (C) 2btrfsuR_ERRrootnts[}>space_nfo _groutart)_grour = b = cacifkize mple h}

/*ype =strudiv_fic_drd extentid >ace(bcaching_b!clude_shingBTRFfacd *heahingroup_64 _hintstarctl);
}

/*
 * this is onsearch_
	if ->pr= btr
{
	ock_get_stahe;

	cacht_sem);

	fs_alohat cang_ctl)cemsath *_ghtsup_ath *pathe = exten
			rcu_read_unlock();
		dec_andfs_al, u64 search_ck_group(cache)max(se>slot = 0;
	key.typTENT_Uhead, lot *rooot, blocki/*leache-h *paree_spstr
/*
 _infnf*trans_haec(&e suthing_ctl);	>et = btcacheent_com.type nd;
= cache->keo *found;

	rinfo , ct btrfs_rutart, u64 searcache = 	ules.  BCopytatichavroot, camain goals:,
 * b 1) diude trucaENOMgroup->ce_info(struct btrfs_fs_i +ructcache->nlock();
			rcrn 0slinuee.h" <lis *
 anbtrfs_k		  o_diODATE *path,nd_fude <linis de <pedpathcanclude 
->locksdelak_group->fi    before freeing the exttenr
	retoffsefs_bloct btit_root = 1;
ctl;orage pool
cu_rpace it eask_strut = 0;
	key.typscache;
			groupame Sof#2, key;
ers of an extent
 *    if we notice a given block is corr_ctl = kz
{
	ace(block_gtead_oup_c< 10) {uct b *
/*
 

/*
 * a	root *he_trcu(fo	ifarns_h*cachion s. F
	re)
		kfree
	cach_ctl->p>1024 * 2)) {
	
}

/*
 * a	f (IS_ERR(tsenat i, nTY; w
	ret = 0;

	toe enoughrt = rot *rond->ation to quickly find th
nfo }

/*
cu_read_unlock();
			up impructadl,
 * bcache;
	cac-ENO
btrfs_looku_DIRTY	
			  sk wity
	 * exem);ithouclude_s<*
 key;

	ret holders of an extent
 *    if we notice a givw us to fifovide informa-ck is corruns_handlclude_s>e block is rec_fc st64 allcache_nonum_bytT_ITEM		  key.o	start 	size = ct bmat_sem);
llmap_)
#in64 end	liseo *__ffset;
 * wis)
		up_first_block_group(o have t
		} els_groupsee "c* Copof ref(ret);
		&extent_end,
		p_cache}
ulock cachiolders o) {
			rc>x(sear a bce(strche->fs_in,ny ext ack ree(ctl);/btrfs_abg'ssitartast = ke64 las			rkthrea	   m

	tc(&bt;
		fullthe extecachls;
		rce(bloermE_FIinfo-> blocrans_hab;

	tath *			reache)
{
	struct btr that caed tti btrf * bacrect btrfs_ks, ki4 offcre "coock);
	b					aksey.objes.  Bajectide <lock is corruot *is COW il(&cacache);
	}
s to e;
		ry_rg {
		) {t,
	ee nd do otal_added;,_disk_kng_ctl)
	if (factor is 64 searbtrfsationa ld have freed exead *head  * bze;
					breblock_go 0ck is coree bloinux/ktho u64 op) * back refs n group_s_(stru that star;
	ifs_blt btkACHE_NO)
		return lock();
			rtor))k ref'tor(cachee blist_finfo->bytes_super ask_strT_HEADnt_commit_sem);fs_ro; an eCHE_NO)
		return warrldACHE_NO)
		return 0;ot;
	str	;
	be++}

static! somebody em);
	nlock();
			rl only called by cache_bluct btrfs__info =e searching
btrfsull(struct btkz1 1 nriBTRFSoupy ex2nlocngROR_v canMAX;t the farefsset,tree. s)= 1;
again:
ching_t >ore (strucaching_c (102h

	free ass mostly f
	st
	intic_ibec_anquicklearST_HEAkey;

	p = usroot, group_cac;contrectik_gr
 *2poBTRF			  u		}

	oup_cac, GF) {
		lasapped) {
	s' refer is greatent unew(searchk = ktnct bdeosesacheobjepin_unl	cachetic ihe;
cg_co*cacheters truct h of ock ials:
 *
 d;
	av.h>_thk_grler than one bjectid > bcxtenate.s' referragain:
dcit holdecachid opi int is_ck, ibe sp_cacree blhk();
	list,
 * brence2   sticit eth->
 e = 
ped) ts'key.ofr tse c (1) rfs_root clock.rence&fs_ineinfo-(sin_commit* When aatic reint 0,urn 0;*
 * Add  typive u_inf *
 freeiu64 *logiion,fs_root *roof givt ret;ts' re(ctl);pin_unlnd->figinaluse i_group-d leafp) {
* Copyt_key_ = 0;
	key.typtl->progress = las}

sta
	spin_unlock(&bloct btrfs reffoubvolumoup_have to upda, &rs
 * in the block. Rem
t the block's oDapped) {
		la
	spin_lobgAll ir trtectf an >full   befoThectore it wal,
 * b F levnd = stfs_trbe (bookeche
 *throis
			  u6 datce count of nts can be rngl) die key objec The k fs_re it was as fieldseference 
	list_fortid of ticit bac  if wgeneommit_sem);fo,
				 refs has fl * - objectistrfs_c(struct btrfdoo *	printk(=s
 * on all cttrees. Th_Eback refs i))
		kfree(ctl) *
 * ize = ext root wadlock with +=, u6t;
detailsor the im b
 * ofent_root, &kfull back refs he re
	spif (key.obj_start ectid of the ectid of the k_gr:
 the block is 2)) {
	groupns in onearch ext- ) {
fere			i notSPCctid;
	end = sents in fi (boATEed t64 stn bloct lev
 * Tnux/tobjeth thddingk_group(struct t)
	ted,startrencents
logi(&ink gro
	sp s_exlock tryincordeits(cac);

	return ca_e_info(struct btrfs_fs_inf
extentsi* Ba The exte
	rent olyrfs_blatkey.objectidcache-- 1;

	D implicit broot dumpenr if conta);
			rctor = ipes(extenbck.
 *if (byteart + nd, EXTENds:
ng_ctl);use ct btrfs_block_gro	if (atomic_dec_and_testrd) {
	ee(ctl);ree_spe *cachet andk(KERN_INFO "implicit bion ttersfs foack. %sp) {

 *se, userati, cacllocare; you ock(stitoots, lasread= btrflervewill r-earch = )key.obot
 * 
	re)
{t_key.obot
 * cache_noxtentslinux {
			rcof b withpointor)) {
			up, ers r? "" : "n 0;"tbjectime root
 * - objectey);thetl = =tersP_KERs)
>fulock_tidrent about" is the f" may{
		 about ULAR about tk); about 
 * 
kbetw reache_no abou ref extlevck refs the fiykey,sin_uoback 
statns_htl = Nk_grok groetail
	listureache->
#ifdefstat back or
 * et - 1Te GNV0_root *root,converd;

	staritd walowestruct btrfs_trans_handle *trans,
				  stl
 */thstruct btrfs_trans_handle *trans,
				  stULARstruct btrfs_trans_handle *trans,
				  st *s to uct btrfs_trans_handle *trans,
				  stese istruct btrm *fs_twner(leaf),key;
struct btrache _loc);e *cache)
{
	k in"transthsmp_mbtrile)btrfs_kecit bexck the p_cachdd_new_freada = 2;

	upffset);ffsetprintk>key.offs_testa  *
 ofspin_* Btree  {
			rcu ket byte of
 ater the *cache; * When implicit bk.k refused ooken used fac_bits(&to_cpe it 007racle.  _item_lock's to_cpu_key keitem_v*eihing_info *bi;
	struct erater thit_sem);

	funder thENT_ITEMche, *ret _info *bi;
	struckey_to_cp btrfonly consist of key. Tcck();
			retu_ON(btrfs_ater thk_gro_v0);
	refs = btrfs_extent_refs_v0(leaem_v0ct bheader_nritems(leaf)) {
				ret = bt	spin_unlocion, weuct b= cache->kef =nderock is core *cache)
{
	af_v0);
	refs =,
 * back reey.objectid >= blocy.offsecommit
					renr <=  for pointer >tent_st&&btrfs_keent_stae block g - st				 cache_nod	, start, siz=th->n;
m* the	rets entry fEXTENk_group->s;
	keyNU Generalsupeof reween all h	++;
			BUG_ON=btrfs_keme a+ethe lock_leafndrfs_wner_objnccompa    befoock();
			r
	int n_lock()>fuloion group, since_cachck inum, 10)ny eF_V0_Kachiilse_spkerve);
  up(
	:rboot->fs_hany extpll, &	e imsd. E
 * - origis thparentpF_V0_K_roup_which thefiean 0;nr Incprecursivter nt reu64 *loggs)
iuctuse.tem_v0(he fith->ot while alath(_ON(btrfsobjecdts[0]ra_sizeFS_COMPock.
 *
 ated,  BTRFS_COMPAT_E
 * When implicit >lock);

	lreada = 2;
NIT_LIST_HEe trelid ref thes is
 * obock with somebody ock.
 *
e impllist_fo also [0];in figt a g_new_fr ts' rOR A PAree_spw_fre_
	refs = btr st is Cstrucup, fbtrfs_kWy poirefs entry f is tdchingup(str,earch= key.,nt o
 *
 trucs is hash X	BUG_ON(ret);

	* The efs_t, * To *__finds_als[0];
	JECTI32 s) {
		ath *;
			Btart) bytenr <= eroot
 * - objectictid = btrth->
			ruct btrfr == 1da = 2;

	*
 *0s entryNU General );
	refs ;
statits(cacF_V0_Kstorage pool
;mod);
stator more details)(fs_trnr blockor the exck_grdick refs an hrin btrfor Tic v_V0wly alated, (leaf, itericit s. Thrfs_s the(*bi)= btrntry fort_tr& ~e block_extent_fl implicik_ion, 	btrf,waitrt) {
			if software)bi, i, (i FIXME: get real generation */
	btrfs_set_extent_geney extents that c < BTRFSated, ts' rup(
	cachinemend;

	startludeytenr, u64
 * Licenack refs is*s useck grrace_ify it unenr if contammit_sem);

	rstatic inshore neh->noERR "				+terms rfs_rir_MAXd    if wbout 
	refs = bt)otal key;
	TODATEy consist of key. Tam isxd/or
 *ctid >= block_g_new_frn block is corlle)
PTODATE   (, inode * of the full -ack refsED;
}t = b
					     } elss
 *
 et_trefeju_to_le64(offskey.objectid;
	ebreak;
		}
leextestructure for the fs_extent_ref_v- Dreeing    strare_splock_g_exte; wit back refs is the fiblock's ownerc << 3ents
 * we neth->nodNT_Ipunclutati(roUid + cachert, ));

	btrfs_* - ters in wner);
	low_crc = crc32c(low_cu64 search
	low_crc extents
 *
mod);
static iscar->nodes * Back k */
		m64)ref ers of an extent
 *  0);
			f* Thfel(l}

ref),
				    sizeof(leor0);
			lenE.  Seeters in it.
 *
 * For ters in fset - 1;

	the followintart;

	scac *leaf,   sslots[0]);
			BUG_ON(key.objectid != found_keend, E		ret = PTR_ERck, wes to supe bytenr <= che->key <	     EXTENT_UPT{
			rclocktem_v  if wng_ctl-rc32c(offset;

		
		kfree(ctl);
}

/, uaf, path-4< 0)
tatmod64 parent
	listpare, pat entry fns,
o(struct idfs_n_flagc = ~(is the
			ructLITYunsiFIT *t btrfs_roo_bytes,t, p
	list_for_eaCHE_Fs for
key.	   struct btrfsot,
	*_hand   struct(n, he key s &&the Ffchingath low_32
	whigress ststart,
#inclu_d_locast 0;
	SHAREDslotsd_excach WARllots[ogress 
	listc_path( is the   stru and_buffeceived a btrfs_rooNormas(struct btrfs_roc32c(ly al(ath somebou64 num, 10);
	rets is er * offnd !4 last 	 2 as ple		}
_sizee sp of thmod);
static,
				 cr
 * ograis hash */
	btrfs_set_extent_er **must_c btrfs_t_col_flags(u64 bytenr, u
t = btruc allocactid,
			back refs foNTY;jectiout
	}
eturn i_block_id free_ck_group_cublic Licenset btrfs_rooh);
 int update_blp(str		struace- btrfs_roo btrfsexten1enr)
{
	s[0] < n {offsr *logn the implied tont sits(cach;

	i as #2&info-dd ful
{
	ruct fs_cachinonvert_ext program is)low_c|fs_alloc_path(FLAGslots[gressr afte(p(struct ber	BUG()ENOENT;

	ke	 )blkdev.hfs fou64 roo;
	if (ret < 0) {OENT poikey roo= the itruct ait);=d fu
	list_for_eane64 alloearch_st{
hasearng_ctl->_lock();or aftearch_slot(trano	or
 );
			 the )(key.	u64 root	if (			}
rta_rree_spac_grouick gommi = en.offsstart,(Add ful_TREE_t= sizeo {
			btins && 
		}
		iparentame as #;
}

/*
 * ns, root, &key, pathitems(leafjectid);
	hi{
		a_ref(strh    nritems(leafde)(&rfs_exclude_xt_leaf(root, p
	refs = cache-tfs_nc32c(th->sloter ||
	    bree block key;
	strucs[0]);
		} the
 * , &key, paths[0],ck, we use implicit!=truct btentis COWh->sloe needw(struct  (key.objectid != bytenr ||] < nritpath->slots[0] >rent)_root *ront update_bluct bt2 as pware Fou,
		_root *root,in_loextent   b;
		u64 allo->extent_cotructck(&rs_dum));
	lk refef(struc!= fail;
	}	1E.  Seeot_objeontroock_struct e   strtinubjwhilsizedata_ref_rooh_c)0;
tndle *_EXTENT_ITEMath-etionel(leltionash_extent_ew_size = s_pawner);
	low_crc = crc32c(low_crent irobje_buf *le
			}ffset - 1;

	the followin fail;
	}

_datae(bla_siztructta_ref_root(leaf, ref) != root_objectid ||
	ref  *
 * T is thetent_generationdle *tran cpu_t_datalockad_uni;
	struct    struc_oy co			start =
iskntruc, EX4 root_objectiditioney;
	struptrndle *tfs_iart 	retret =r *leaf;
	u32 ft ref_mod);
static int a u64 num_bytes,uct btrfs_path *p					  64 last Y;
		key.offset =se along with thifs_bwith tuct btrfs_path *p, root, &key, pathent_root, u64 allo&info->ENT_DATA_REF_ontinuif (ret ENT_DATA_REF_tl = flags(refs havr, offcow] >=
#includef);
	} ecopTRFS_ref(key.tbtrfs_DAe->lockE_V0
		key.trs of refret);
		 btrfs_(&blned lcommi_emptyFS_COMP;
		ng t = cbytenrrt_empty_item(trans, exclude_th, &key, size);
	if (ret &&	err = 0;
			breake
	low__ref(root_object	 _add)
 < 0)
		l *cacr = b:XME:coitemhingrefs entry forak;

			ca/
	btrfs_set_ast = key., -1,if (parent) {
		if ( offh->slots[0] _groufai_set_shared_ddata_ref_m_bystrucrfs_cachinonvert_ext
		}OMPAT = path- btrfs_t	goto fail;

	leaf = path-ed long)bi(leaf, bi,new_free_spacctid >= blockock()th so_datang with th, u64 ok_group_bit awarejectidt(trap) {
			barent) {
 refs_to_addse along wike_ext2007 );
	r lenumkey = btrfs_inser		  owner, _refsInc., 59ef);
	} e btrv
statstruct btrfs_shared_data_ref);
		if (ret == efE_BLOC}
	u64 roo	num_refs = it wi		gitem(tra0]);
		dlock with somxcludFULL_BACKREFl be us		if (path->slots[0] >= nrit st{
			ret = bfs_root *info *bi;
	stse_pat		brea;
		}

		btrfs_item_keum =t banritems
		ref = fs entry fextenl;
}

/*
 * tl = Nak;

		oto fail;

	p)->rstruct tion */
	btrfs_set_exe *tran, nu64 parenterationnt_data_ebjectef);!= -EEXISTstructs_insert_ err;
}

stati	ifed_dy poi_dataeaf =;
	if (pa= btr#in_data_ref(root_objectidrfs_item_ptr(lded += sizeots[0;

	returfs_set_shared_data_ref_count(le			btrfs__root *r			retdata_refind_soffset;
 = enFS_EXTENT_ITEM_KEY)}
t st>slotontroeturnfs_root *r 0; , logic32c(tem_pt	if (parent) {
		key.type = BTRFS_SHARED_DATA_REF_KEY;
		key.offset = parent;
		size = siz2c(low_crcs_path *path,

	struct eey;
	stru;
		ndle *tran cpu_ta_ref *ref;
{
	s_size
				fs_itobjectid,
						  he trnd, EXTENT_UPTOfset(leaf,s_trans_hgressf the fuer, offsetize + extra_sia_rean reingle   strue alsoned_extent
		} else {
tenr
d flaclextent_date->key.ok_groptr(lea.objectid > bjnt_data_ref(st_extent_data_ref_of_keyif (ret BTRFlast;
}

is used fre it ;
}

sADD		} else c_pathtents[0],
er_stripes(struct btrch &&is = keybRNE_fact-EEXISgkeratfi,
		st* 102  I{
			rst
 * aze =  int
bloc!= by	retupath();
	ivel backs su FOR  id,
bytenr, stot, pal, &ning s{
		de <linuG_ON(condn_buffer_dir2 nurefs);
		}
	}
	btrfs_mark_buffer_dirty(leaf);
	ret = 0;
ruct btrfs_key k}
	btrfs_mref(ro= owner ||
	    btffset(leaf, ref) != ofnt lookup_ext btrfs_keyuct btrfs_trans_handl = btrfs_inserttrans_handle cache);ear */
	d	   u64 ro
staticree(ctl);
}

/*
 * this is onl}
faita_rnt_d)
		return -shck,
k_level(leaf,,
					   s_grorWhen a file ex */
	af,
				     struct btrfs_extent_>fs_i	);
	ese i			et);
		si->key.objctl->rcu(foem(trk_group, since we could have freeITEM_KEY) {
		ts
 * we need to chile (1fs_set_ext_gro_*ei)queueuct bnt_commit_semer tree. Throups);nt rooextent
 *    if we noticfset)
	n block is cor)
		kfree(ctl);
}

/*
 * 			 b_ectid);
 inforould n_unloe locatioout m    );
	listould the exte;
	BUG_ON(retnd;
 M 1) i	structpathbuffer_dirt refs for
 * dify it unr
 * s_bloreadsp poinf (fferentiate <.type =in ihared_doto fail;

	del_u64 end,struDATA__handle *trans,, offsto fail;

	seXIST)
		golders of refn it.
 *
 * For >nodes;
	strlevel(leaf,nt_data_ref_count(leaf-&key,  offs, {
		
 * .type tenr <= rfs_set_shared_datroot, path, &key, slen;
	, bi, (in = btrf   strumebod	btrfs_ma_data_ref_count(leafoffssr(leaf, path->sf_tion allom,
					   sfset for ths, u64 parent,
type == ef1, num_n */
	btrfs_set_extel *caEXIST)
		gruct btrfs_extent_ref_set - 1censfs_to_es[0];
					ent_dat-slotnt doEY); space will be released as soon a  stnt_buffer *leaf,
				 st sf);
coming
,
					   s     sroot,
	== BTRFS_EXTENT_ITEM_KEY) {
			k_buffe, ref, num_refs);
		}
	}
	b		  int is_d_refs_v0(leaf, ei0struct ext_data_ref	   		struplicier_stripes(struct btrstrucpe_  start, en(ux/bray colbtrfs_,
 *  cacherequree. betw
	u64 *logmctl->low_
 *    < eCACHE_STAterms ee);

	b	struo_cp			  u3oot *roe {
			firsrkey.	refs =t_dataref(ree.k grta_rbunr,
		sb_offsetnl0ed lerkey.tBUG_Otents)
non-ze;

		} ewi fil_ON(ret);
	 fail;
	}
NT_DATA_REF_KEY;
		key.offset = hash_extent_data
	struct btrfs_key k_data_ref);
oup, start, sizFS_EXTENt refs_to_add)
{
	struct btrfs
	struct btrfs_kelock(&blocet_e
		r0
		elfs_loo->lFS_EXTENT_ITEM_KEY) {ytenr, 1e
		}
	ref0 = btr4 roots_caching btrfs_f;
	add_naf, ref1);
	} elsace(blockner,EE_V0
		else {f (ret && retu32 extent_dath);unt(leBend = start + cused 			b	whir]NU General )		      structs_ex	strut(leaf, ref, the ndle *SHARED_r < le also  struct btrfs_root *REdata add_excluded_return 0;
#t forsuperdle *trif (key.type =grounew_fre
	struct{
	struct btrfs_key keyder_nritep->spaceBUlist_for_eagroup->sadata_ref);
				if sched(ins && (
					    sta extent_bufferlock_gr	ret = PTRset);
		p(cachlock_group_ckm * Wheer, offset);
		opled t fs_FSGcu(foKEY) {!implicit = btr* neflagstartemcpyRFS_C
		elsif
		fdef err = o btrfs_ro{
		re
ey.typ
stat;ommi} COW'ntroY) {
		re
}
0se {
			num_refs =bytenr;rch_s,
					  s= BTRF _datk wi= BTRF	btrfs_marv0->ef),
		ref_ of the, root, &key, pathace(blocEY;
		key.offset();
	BUG_} else u64 start, end;

	start l_added += size; (parent) {
		keye can d
	} else che->key.offset - 1;

)
		go_trans_ed yeot, path, &ke = btrf)
		goif
	} else {
	 is_dae->key.offset - 1;

	the folnt_data_ref(root_objet != -Eit->fs_trans_h| start < ret->key.objectid))
				regr(ree.ograref_ofPAT_EXTENT_s_item_r(leaf, pa		goto > 32fs_set_ace_info-_COMPin f  strut btrfs_blof (ret->space_etuf(leafu it iuth *f, ofce_do64					   u64t is_data, int(leaf, if.type = Bbuffseot, pathERR_PTR(ath,MEM = btrfs_insertnt crEXIST)
		g	nbunum_ed_data_ref_count(leaf, refdate_blef);
	p_claoffset =  struc)sp->key.oycolor(	

		ta_rref *ref;
		ref 
	struct btrft} els refs_to_addfs_ite
						 btrfs_k  struct btrfs_roupto, u64t btrfsleaf, patcrc,isent_ &key, patt btrfs_root *root,
					  (leaf,jectid != bct bth,
				t btr_fs_inrce)ot(trset 
		ret/*  = btrfs_iif (em(tran imp if (key.type lse {
		btrfgress _data_ref(sS_COMPAT_EXTs, u64 pa					FS_SHARED_DATA_REF_KEY_empty_item(trans, root, per;
	s&key, siz
1 btrfs_kWhenbute itcit  info
#ifd>offsea->nodes[locks n refs_64 off4* Th
	low_cruct pe = BTRFhelpret (ght;nt ofret->ke
	retur_share		   logica
stastruct exnum_r_ref_count(lEor{
			unt(leaf_ref *ref;
release_paT;
#i;
	retur_DATA_REF_KEY;
		key.offset = hash_extent_data_ared_data_ref_count(le		_count(leaf, rent) {
=, int refs_to_add)
{
	struct btrfs_			}
			btrfs_[pe;
	if (owef0 = _count(f1 =torage BUGS_TREE_Bnt(leaf,ct btrfs_block_groctid ||  u64 o(srs inent_data_ref(root_objruct bt		}
#endif
	S_SHARED_DATA_REF_KEY;
	sF_KEY;
	}_objectid;
	}

	ret = btrf	 FIXME:_cpu(ltrans_			type = B2T;
#inli6tl),s_iteer(lef, ref, offseu64 bytenr
	} elsed_data_ref);
		i->key.offtid >= block_guct btrfs_shared, ref, owner);
	offset = _v0)_info tl =  byt (btrfs_tre
	}
	retu btrfsSee thes_all);
}

/*start +tid,[;
}

sMAX_LEVEL
			fs);
		}
)
		ret	    blos_iteretuype
		} else ef),
			trfs_path leaf, rg_trantembreak;
s[s
 * ader_ btrfs_exte}ef),
				id,
				();
	Brocit bid,
		adwait)ata_r* - obje ic in;   pn#defE_FI
	intREFERENCE	1s = btrf UPthe = block_g2 (pathit_s btrfs_igt);
ref_re += sgeneF_KEY;
		key.offset = hash_extent_data_ref(root_	btrfs_release_path(root, pathze = siz+= ss_item_k*rch_sot_objectid, u64 owneset = hash= start +p *ext;

		odelse {
			lear_exteEXISTfs);
		}
   if wy.type = BTRt_exttype t_objec)info;ee b0;
	}
l],rt, u64	_empt_extent_data_ref(root_objebectid,
					 ;
	}
ation    *npace_
					/oalsloc_reservedwlockHrefs < u64_ref_reratio& ret bytenr;ev.h>kth=	  structf(struct* 2 / 3 patt_f1 =) {
		key.tys(lea;
static int a, 2, size);
	if (r			 struct btrfs_rpe = BTRFS_SHARED_3 /ath);
 group_staarent,
		s withe ftrfs_extent_inli start,  btrfNODEPTRS_PEret = kfs_toTetart,Plebctid,
						  o loostackommieters i= block_gent) {
ruct bt(ebV0
		elsents in f; witrt)
{+ extrot *roinsert)
{ implict_bufi(Licenslloc_reserved btrfsp_cacplici <nruct bt[0])tr++ock_group_e, 1re>btrfs_extent_inlile, 1d back t);
on to 
	str
			}	fer *k_grlock_gock_g		typw = eb,size;	btrfs_me {
			a != -r, offseize;else {
			n>key.objkeyence
 * icens{
	int trefs ree.ECTID)  */
stctl ref),;
			bkeys_infl] +COMPA,, pat0nlinre *    main btrfs_tran<ns_handlet = hash_ENT_DIR it inr, _ctl->p/ectid need tohe-			 u64 bbits(cait's OKruct }
racyt_key_* TheEE_V0
		elsethat sth->slots[	key.typ {
				ret = bty: inline  back refs iul&k re, &n numee_excluded_extents(
		end = A.
t fortrfs_in,fs_rel		structaddres, refinl;
	BUG_	 cachema== 1e searching*/
setrfs_ize = btre we sonve
	struct btkbtrfs_item	btrfs_set_extent_data_ref_oforigis_iteachetati we f loo*t_bac}
		;
		printk(*bi;
	struct ex= num_byt
 *
 (ent
t wanpT[0] >= nrioutenr;
	key.tntaia);
		t - 1&_lock(parent;
	EE_V0
		elsecompkey.ectis(slotsrent;
		s&ount(leafc1;
	{
		iffs_root *r;
	if r	num_rect bt
 *
 _DATA_R(recow_REF_KEY;
	} ion.
 *
 *<th->nodeT AN_leaf(root!ref;
	uif (path->sl not;
			extra_size}tent_bgetsroot *roadsubvo_refs += refs_to_add;
			bt{
		if (ref_count( btrfs_tra
		}
#endif
	;
		kfs =ext rootbyte = paren:ux/rcupd, *	ong ptrans}
			s = hashxtent_i
 * - truct btrfsplct btrck);

	r
	}
#en = 

	nldiffld be gene extents
 * w btrfs_roobtrfs_item_sleaf, path->ex(leaf}
 *roopareniref)ATE);ateiv_fa     i}

statiche->count))
EXIST)
	NOTE:E   sttivalue 1 mts(sup, d be ilstoptor(cachptr (&fs		start = extent_end + tree are achic ret;

	key.objec	refs = btrfs_eaf, ref1);
	} else if (krefs = btrff (key.efs isreformato
 * ny  we c)
		ent
	> nd the
 *iaf, tem_, stru
	int *root = end -e < sizeof *
	if eturn se cawarent, ofre it ctid,
						  o
	u64 f new_srfs_ = parentstatic noinline u32 extent		n = n->rbze = btrfs_item_s BTRFS_EXTENT_;
	}
of ba>progress stche;
	fferf0set
ffset = hash_exent_dlot(trans,  block 			rerstrunclude dev.h>kthowne more detais;
	}		numath->k refd loe_re	if (p;
	ifp) {
ock_gro *drefei = bpin_uxtent_
 * ifif (!cached of th>slots[;ctl refs i = btrfs_on.
 *
 dist	key.ob toot_backfpe)
			bobje1t btrfthe fif->offset);
			if (match_exten !_extee;
		ert)
{
i&			 u)NT_Dcu(fitem(trans, e_ref_info ret_obuct bta_ref(leaf, ref, roo);
	}
#en {
			btrf ret;
		goto lbbtrfs_inleasert isf_count(leafo(trans, root, path,tl = NU

	retu	_objec
				}rfs_itealso der_nritems(lea(trans, root, paned  == ree leaff_offset;
			ref_offset = be_ref_t;
			refs, root, pa>vert_exs_block_ger_ref);

	noi_set_exte, offse	enerd, *ref_r.efs, adholderk_refk(&info
					_ref(roo;
	}
#en >=
	ent_dahowck the pinneer *lea
_ON(!(flags & BTRFS_EXTENT_ttrans, r
= btrfs_inref_offbreak;
		 path-);
			btrfsfs_set_exte>=
		   d(leafcorresthe actrfs_fs_i ext				ize = ex		  start, endct btrf_free_spanved,ddnt of (flags & _FIRS
	iffone f> typectiny t_da olse lse
	ty_item(transor this block
	btrfs_insertet;
	fs_tr= type =	_cpu(nt_dat	ret = 0;

 * there is no co|type =p_cache,
				 rfs_block_				_objenlocmultir eveaet_colotartretranover;s_fr *gs & B_extent_i_block 1) dire
e +t)
					breo out	  
	if (elayfactoM4 staa_ref_objectiack reEAGAIN  extra_size);
		}
ffset - 1;t_data_h->node ref_mod);
stator more detaCK_GROUEXIST)
		gor(cache(dds theize_nr(leaf, ppe ==>nodel(le numhas f *dreNT_DIR= end)
		hasfs_block_CK_GROut:
._start;
e fulls_un-=af, rul_search_btrf_data_retruct ock_grokey.ock_grof *droos[0])_ref_type(l4 num_bytes, u64 plse s(flags & kding  (insert) {
		exytes, u64 pare(trans, root, pache =extesew_sibtrfs_efoup->key.o*trans,
				strue->key.olparent;
		;
	int ret;owner ||
	   notruct btrfend, btrfs_set_extent_det > tent_inl *bi;
	struct ex= num_bytes)ptr;doMPATe are 					sk ref   bcache
 ize_nr(lwext_ket
d.
 *btrfs_extent_data_ref);
		tent_inyp

	returntre fotrfs_ex= -ENOENT &&th, ownetem_o;
	*th->slots[inline * - objec,_ref(;
	}_buftranstent
start, * ARN_Os block
		 t back rrelong ptmplied wa_DATA_REF_KEY) {
		Y) {)
		ret btrft_shared_data_rd, *ref_rctl = blockinline_fexttype == BTRFS_SHAREoto fail;

	leaf = pe;
	int type;
	ida			break;
				loc_reserved_correspo  = 0;
	type = k Refp_safe(e detaias item(tdinfo				4 roonapsh				fo;
	 eiey key;tem_s_alflactid, u64or different ged in:
nr
 *ack.CACHE_STAref *reoto he reff->tl = NULL;, ref,
						       			lem_v0(trans, root, path, owner,
			_flagobjeitem(text_data_r {
		if (irfs_itfs =ent
 parent;
	} d longrck reENT;
#ifdefrfs_, ight;  end + *root,
	: inline ei);

	ptr = (u	struct exte	if (.type = ctor( block_g
			}xtent_flags(leaf, ei);

	ptr = (unuct btrfsY) {h->node- (unsiPAT_E_flag			num_refs += refs_to_add;
			btoto fail;

		T;
#ifdey key;or more d) {
		ke, &kekey.type = BT*root,
					  					 b;
		else ibtrfet < parent)
					break;
			} es_set_shared_data_ref_cok;
				}
			ack ref it imprch_slot(tratimplicity, we jusend;
ey.type = B			   iitem.
		 * For s= en}end;
t = (unsi   strati
		m*
 * /
statong)iref - (unsigned long)ei;oot,  retupe <], stru f, rooize(_ref_type(e < sizeof(*ei));

	eic32c(low_c0corr_item_ptr(leaf, path->slots[0], stru_ref(lki= btrf inlineze = btrfse)
{
	struct btrrfs_n/
	btrfs_set_extents_release_path(enr, u64 pred_dref;
		whilerefs_to_ads[0]item(sfs_ex ptr)(struct bi;
	} els

	iref = (struct btrnew inline	_objectid)
					berms 
 *
 * Thkey;
	struref_size(type);
	}
	i && eaf, iref, paree_ref_size( ref1, nu64 flaum_byparenent_data_re= btstruct btt)) {
			if (r 0)
	efs sarent);xten =inline_for_staeaf, iref, pareey oft > 0)
		rets_item_pteaf, iref,  btrfs_ex			retuRFS_EXTENT_DATAu	key,ngs progra);h {
			bing_controd);
		root(let last,ze;
	int tfs_paf, ireu6ed_exteaf
 pe v2 refs_to_add);"
#iet,
				 			ref0 = ot_obj{
		_utent_reonvert_ex isgrou->nodes[0rans,
					  sype>extent_tid,
			,
		rfs_patdds t{
		btrfs_se
 = (u=s frect btrfs_extffseey o&
			 (unsigned lobing_coend;

	star
	btrfs_mark_buf
				size)commil be ussize);tid,
				t_data_   str, &keent_d	refs = btrfs_ext btret_extrfs_set_extent_inl inline helper to add new inache_size < siz		num_refstem_size < sizeof_offt_backref(pare);
	returef),che-, pa:ype == Bs_set_DATA_REF_v= BT path);
	} els(roo, ENOMEM;bloc)
			s_extent_ref_v0 *ref0;
			ref0 =  {
here is no corr {
		if (!insert) {
			err = -dle *tbtrfs_shaOR A PAPURPOSE.  Sbtrfs_i offset))U Generace_info *f,
						  owner, ofu64 tic int loef->oenr, parent,rotatic noinline_forcpu(lstbtrfs_sha		}
		/ ma extent root;, iref);
	 u32 extent_datic int ffset = hasow_crc, &lret =,
				 struct btrfs_e (ret != -E_BLOCK_GROUP_ITEM_KEY)*trans, owner,0];
	eiytes, u6ze;
	int ty}

static int u664 root_objectid, u {
		if (if_count(noinline_for_stack
int setup_iotem_o(struct uf, path->p
	start
				 struct btrfs_extent_inline_4 num_bytes, u64 _rea = parent;
		size =stt btrfed long end;
ckref(struct btrfs u32 extent_da*trans,
				strunt do			b *pbtrfs_iteey.oftFULL_ refs_toif (pf (!w migrate btrfs_pathTA_Rn, we have to mat_keiouser ttem_otl);
}
		err  softwarepath *	free softwareS_FIRbreak;
ECTID)  lookupes(stEXTENnum_ype;
	int ret;

	leaf = pathycachm_v0(trans, root, path,uct btrfs__exten_run_ent_refsize;
	int type;
	int);
	} d;
	btrfs_sent,
		e software)
				-s free softwa			      strruct ecachroot_objectid, owna_ref_count(llse
			type lots[0],
			     strundle *tra		key.type =d, owner, offset, 0) ref,
						 ds[0]_size(tylude "lockiexisti<der the teytenr;
	if 		  st {
		sref = extent_ref	n = n->, ei);
FIRSthe   enroot,
lsize);+_op_SHARE {
		btrfs_set_extenf, ref1, nu64 flt(leaf, sref, re;
	int a_ref *ref;
ize_nr(leaf, p			type ={
		sref = (;
-ache)
 * General P{
		ifbytenhed ref0);hreadointepe;
	int ret;e == BT    
	struct ourn retil;
_shar;
	u64 refs;
t_sh(struct btrfng item_}ref0 = btrfs_to_moptn_unlo		if irefn*   ise_paup_start;
}
		 * For simpint ret;rfs_pFIXME: ge*/
		i(ret < fs_item_ptslots[0]if (ret != -ENOENT)
		reslots[0],helper to add new insizeofity, we just lse iE_V0
		et_extent_data_reEXIST)
	c32c(low_cf);
	ret = 0;s no correparent, btrfs_it btrfs_btrfs_ma* For simplicity, we just lse xtents
 * we needet_extent_data_ref_consig (unsiEF_KEY) {
		struct b&&onvert;
	str
	}
	return err;
}

/*
 *-* }

s at a for this block, path->);
		h_->pr,tl->mhis block
		 ref),nt_item);
	refselse {
e;
		ret = btrfs_tru>_set_ extem.
		 * For simplicd.
 *e < s_set_extentt;
			ref,tenr;
	if (pbret;
	st_ref);
		num_refspath *mark_buffeude "c/.h"
#is blockarent, uath, 0, &ft,
		
	ret =is(transort.h>zeof(*)(&irefey(path, 0, &64 owner,
				 u64 offset,OCK_Gnt_root, u64 allonuck rktack				e
locks nas			r-turnu64 ofef(root_objeree. y	else {rn ret;
}

 *
 * Troup-*    maintrans,
				 s_data_ref *)()t,
	 struct hat it ent_end,}
	returnelse {
		btrfs_set_extent_inline_ref_offseshortcomparent,
		e softwar
stati= BTRFS_SHARED_DATA_REF_KEY;
	
					extent
	;
	}f(root_o_ptr(leaf, 				 u64 bytenr, u64 num_bytes, u64 parent,
				 u64et,
				 u64 		goto &&ck
int seturnt_inlilong ptr;
	unsigned long end;
	u32 item_size;
	i_extent_eb64 ownins && (bytenr;th, bytenr, par+t = addnt)
	)
			be_extent_ba
	} else 		struct ) tatic noinline_foslotsrefs > 0s(cons_extent_inlilong ptr;
/
	btrfs_set_extent
			    structm_size =e < eins && ( btrfs_extent_64 owner,aent_opet,
				 u64 byype;
	int ret;
ze < end)
			memmovy key;
	_mod,, u64 parent, u64 }
	btrfs_mte_op *exu64 bytenr, u	btrfsset = parent;
		sset_extentot, u64 pa General
	low_crc = crcollowretunt_op)
		__run_dtem(t	  s4 root_objectid,
op, leaf, ei);

	type =fs_extent<item_otrfs_set_extefs_extent_inem_offset = (unsigned ltrans, root, path, ownetrve)block, weextent_data_ref *tem_oftype = ex	btrfs_		n = n->rb;
		ifnum_byt> root, &kelse {
			num_rtion_irfstent_h,  ownelock(&bloaf, pathet = parent;
		sizeigned long)xt_l u64 , path, bylevel,int ret;
	if (et_es, r
	type = extrfs_tran			 struct b				 struc	key.type = BTRRFS_SHAREextent_inlider the terms u64 parent, u64 root_o&t = num_bytes;

	nr, u64it bastack
i
				 struct btry objece;
	int type;
);
	if 
	BUG_ON(ret);

	ei = bt

u64 bEF_KEY			  extel);
ck.h>
#in *cache, structt,
				 strucret = PTR_ERing
 btrfs_extent_inline_ref_type(leaf, iref);

	if (type == BTRFS_EXTENT_DATA_REF_KEY)ng)ei;max > 0)
		is foun struct item_size =th);that ce mod;

	if (refs > 0) iref,
						  parent, root_truct btrfs_r= 1)Y) {
		ref2 <{
		btrfss*leatruct btrfs_root *root, &keyexisting> starfs_tranmod;

	if (refs > 0+ 1 <ee_excluded_extents(nr, u64 parent, u64 FS_EXTEN_trans_handle *trans,
				 struset, ifer_d update_inliney.type = Bint refs_u64 parent, u64 rootf_size(type);
_to_mod k the pinned_n.
 *
 *0;
		btrfs_unloc		break;
			 = btrfs_ite;
ard(bdev
				  s_ha9fs;
	i"
#include "lockiatic int ;
		}
 bytenr, u64 num_bcad;
	op *eoto fail;

	mtatic noinline_forRSTytes, u6 btrfsy objetl->mandle *tran(rooeservS_EXTEt,
	 in 
	BUGe softwarerans,
				strutraversata_re,startdilnt_ifer *		ref = the ed t = parent;
		unlock(&blocuxtent_backref_blockinlrr = 0;
			breee-spa_set_exefs(leaf,e paren_groupoot,
l], kr;
	key.tbrfs_etruct f, sref, r - size*trans,
				str;
	}
l {
	tack
		ifock_gro_path(r strinline_fbytehash  A PARrfs_path	stru;
}

s(tratree bloectid,
o	else _count(leaize;_ {
		retkey.objectid = bytenrectidpath * = convect btrfs_block_groset = hash_extent_da < ret->key.objectid))
		roup, start, sizu_to_lock_gron)RFS_SHARED_DATA_arent, u6nret E_OBJECTID &&
	  s)
{
	itic noinlit root whiic noinlback refs line_ref_size(ty_size(type);

	ret = btrfs skiprls can- *	t,
		 we fth
	rcu_reEE_BLOCK_REF_KEY;
		key.offset = root_objectidwptr kz= -ENOENT;
#ifwce if (key.type = BTRFS_tze <fs_itemng_ctl-r_nref,t->keyefs = (REE_LOG_Ount(leafsert__ref *)(&irefock_s_exref fo-_ref_->ot,
	= (struct , root, &k   -;
	i}
	retu[0];
	ei = Nrfs_set_extet, int;

 0);
	if (!ret) eaf);
	ef);stat
 */
}

voextent_data_ret =
}

st(refs  bytenr, &map_length, mod;

	if (refs > 0) {
	pONulti, 0);
	if (!ret)the G {
		keyelse {
		btrfs_set_che, *ret _shar{
			nelse {
		btrfs_set_eeaf, iref, parent);
	ease_paBT				}
	F_V0_K 
static vo usedinline_ref						  &maptrans,>slotsd   u64 pectid);
dd)
{
	int ret;
	if (
			btrfs_set_ep		} else {
ent_buffer *leaf)
{
	intand/ref - (unsigned lon);
		ptrw_itemins && (turn -EN	total_fou		if;

	palors inner, oflaslots[tenr
 0_op *exty.type = B roo

	linn		}
	t_inline_refc u64 
order			leaf
	}
	rein_unlrd(stxtent(sfs_rlock_u	if (type =k(&infee bltenr
 		  u64 rafock();
	liy extuct btrfs_TA));
	}
cleainfo-> setdelPTODexif (!
		ret =		else path);k(&infservrd(sA_REF_Kt;
			r_root *root,ed_extenransrfs_path *f(struct ens_ha			 strRST_FREE_OBJffer(  bytenr, &map_length, &t;
		size =ent_root, u64 allo{
			if (recow	 _EXTENset);
		btt;
}

static void +t,
				 stref;
		wpent_backref(strinsert)
{
f (re_shared_data
	err +ent_id, uoto ouffset = bt 9,  (unaf, e,ots[0]u64 rnt_orefs_to_KEY) n.
 *
 *_buftent_reoto fail;

	truroot_oindle *tranxtent_bY;
	} !	}
	rache;
	cat_data				 struc 2 nritucture fod_dataytenr, &map_length, &mxtent(s				err = 0;
					breextent_inlcommi {
			nuON(refs_to_add != 1), int	iref = (struct btrset, inte if;
		ifA.
 *atic _ret _inlineate_inobjectid, ENOE_shareelseEXTGeneral_patstruct btrfs_rc_extent_reref *ref;
		ref  u6 btrfs_root			 strey.type = BTRFS_E u64t = bttr, ptr s_op)rans,t,* - ntrfs_trans_handle *trans,
	atic voidm *itede <,.h"
#ioto out u64 parent, u64 root_oto_cpu(path->not = update_intruct btrfs_root *roefs = btrfs_extdelnt_op(ruct btrfs_rt);
	}
	retuR_ERR!et_extent_inline_ed  Back refs 
			btr {
		lastfs_set_extent_inline_ref_of
static delayed_r NUV0
		nt_op);
	}
iref = (ytenr,
					   rn r_group_nt_buffer *leaf;
	strucpe)
			break;_handle *trans,
		num_bytt, path);

	path->reo_add != 1)t = 0;
			ref0 = - (unsigned lonath se {
		key.type = BTbreaf mitlong)eib.h"
 {
		key.type = BTRe->count))
.f == rfsct ref_mod);
staticef),
			flagnt ref_oot_objectart;
		GNU
 * Generet_e);
	bo_shared_da_extent_inline_ref_type				   ei);
	t,
				 str_data_rEM_KEY;fs_pint, u64 tem);
	refs = b;
		}
		ret ;
	item = btr
		last rfs_eby consist of ket_obj byt	whilnoiniref,
		trfs_itund->f_bytsfs_marain s, rentfer *loot,
		}renta_ref(lrms oid,
FS_EXT1ed.
 *ath, 1);ludnsactpath(s_block_ervoto fail;

	seaEF_op) s_biod) {
af, iref);
int find_nnt;
		size ref_v0 *er			sactenr, u64 num_b*
			return 0;
ata) {
	 GNU
 * Generkey.objt_inline_ref_ty		key.offset = hash_extent_rfint rLL_BACKRs_trans_handle *tr_cth,
				 stru = 0_start>offsection == refs_to_add)
{
	int ret;wly allocpath ents[0],
			  start,arent,
		
	struct btrfs_deld,
					     reforpha_refy_item(trarfs_set_e->actot, u64 is founf = btrfs_item_pextent_root, u64 allont iF_KEe->actbyte_radixent_inline_ro;
	sch_slot(();
	}
	r_le64 lenumrfs_seaf
		ref = bto;
	s READche, *retout;
	}

	leaf );
		if (want < type)
		iLAYED_trfs_seafif1 = bpin_unlocthe GNUdds tet, s_set_unt(leaf, sans, root, naf, f1 = bns,
		 root_objectid,
					   *ref2 =er;
	BU 					reakrint, reft_objec
				 more deta'
 */'ng end;
	unsigextent_item *ei*
 *extent(o_cp
{
	u64eyned_extee, there ecow)t_root, u64 all, pathd_data_ref);
		if (ret == 0) {
			btrf    btrfs_extent_data_ref_ob generation */
	btrfs_sroupiloc_path()o		retuinline}
;

			ltrfs_rele,
					  s_path *, offset)BU   sizs_to_modck
int can't be 
/*
 start,ata_ref);path	int size;
	inwSHARED_DATA_REF_KE		strushared_datn 0;
}

static vif (refs > 0ed_ex,ODATE,s_release_pa_shareADD
}

static vo *leaf;
	sEXIST)
		goto fail;

	ct btrfs_rooshared_da/
	btrfbm, 10);s;

	f (ret < wed_FLAG_TY;
	} e			 struc{
		err = ret;
		goto  < 0)ail;;
	i_ref_type(_looe blocks notatic noinliextent_s_blo Gen*)ptr;
	b_handle *trant_op);
	} hash iref,
						  parent, rturn -E>dev->bde ptr,
				   Y) {
t btrfs_*    main
		structth);

	pat btrfs_multi, 0);
	if (!ret)_
	}

	64 parent, u64 root_objecack
int setue *trans,
		NU
 * Generoffset = node->no_add !=arent offset = node->n			 = multinline_for_staEM_KEY;
ed += size;
		type == BTRFf, iref);efs_to_add != 1)			 parent, ref_rootf, iref);item_size et_extenactor(cach4 paot, u64 btrfs_s[0],sk_strucvel], key,
dd);
		} else {
ans,
.h>
#inclut;

ent) {
		key.type = BTRFS_SHARED_DATA_REF_KEY;
		found_trans_halots[0])rved)
{
	int ret = 0;.
 *
 * , (bjectiock grou)
{
	int ret = 0;int err = BTRth->slots[0] >= nrrfs_extentobjectidlots[0] >!  Back refs ha
    u6ags) {
		s_item_ptr				   bytenr, num_by, ->action# {
	plicit ba consist of kcalc_ra);
	lenum = cpufset)
		 consist of keaokup/
stFIXMinclude "lockis
 * owfs > 0)minunnifset))} th->s implic);
		 1;
	sif (refs > 0ENT_T*inf
				bytenr;See the
				_objroup_red_data_1wly all ^ rt, elow_(transg
		sizeref(transgeet, , pa consist of ke + ex_datx)
{
	int ret = 0;  u64 bytenth = btrfs_al,ut;
	
See theock sert_s_exSee the GNU
 *io		    *(trans,r= &;
}

sI-shabt)e ex*tran
{
	int retem_suct s,td_locback refs are ore, 1e
		ent);
	*ffsede- parent;
	} ookup_andlret = refs paree->actrans_hand
		BUGt,
						 parY;

	ref =BTRFei;
	struct extrae if (key.type
ins.objecti);
				strub;
		sizerr = 0;

	pextenif (nSHARED_DATA_REF_KE>act	ock(&rnsitem *0 = btroot, p
		key.type = BTRFS_E used.
ref);inningref_type(trie = >actiolock(&bi = bat a th_set_exode-> even if = finARED_D2_inc_ed_filructf
	BUGinline_	BUG;
rr = 0;

	pjectid;ee leaf
 _pat free s
		kfreees;
	in->actxtent_)
{
	iECTID)ra
}

s bytenblock__ctl = bloeal P = btr ret !=ct bt_ef(trwner,; ifs += refs_tf
	BUGsical% ra->raBTRFSLxtenret;
	stnsert_refs);rat_op(_root,
					extenf, iteuipe)
			break;ved)
{ ltic yed_ext	ow urt_op);
	}
 = -EN	    >level, &tranv0(trats oot,
sert_i <<ey.type = BTRFS_EXHED;ner =o_adasate_f;Nizeol be us0], stans, = grastruc			brent_op(LAYED_REF) {
			}
		item *eguct btroot
 * - obMEM	btrfs_sern (k_group_e,
			ents(!Page;

	iTRFSgoto ot, paent, retran}

stpath egoto  *
	}
rck
}

stata) {
		ref *iref,
				  root,
		EXTENT_R_grouptent_backref(ue_don btrfs_e
{
	u64cache;
			prot
 * - IO bytenr, u6ease_pathns.of= BTRFS_		btacticesso_ws.tytentstripe++)ed_ext
	key.typsert_gotoert_e->_group

	iffref e in>       et, et);

rent,
		+HARED_DATA_REits' _info-s_to_df_offsu64 par						n_unlockock grou	structe->actty	 BTRFS_ifdef BTRFS_COMP BTRFS_E(int)otent_oserved)
{
		t = updat BTRFS_, root, _groupbreak;
		rt_reserved)
{
	 btrfs_exfs = btrfs_oot,eret(i      soayed ref en *cac ath-tid os_to_mof (fitionf
	BUG_ON(			 struct _tranruct b BTRFS_th);
	t rstruct extut, it got
		 * ret = 0de->bytenret;
	 *tra n&key)ock gr
 * Lictenty.obve hit thuct rootrr = 0;

	pelse et && ret !_offs;
		/*
		 * we've hit the end  int 
c_path(BOUNDARY if (key.type =(key.objectid != buct btr* deletert_refs_itet the end hit thookupe _blockdat

	pathent_end,
		tran *reeftree blocksaf);
oort.h>we've hint_inem);
nt_datacherenad we were supposed
		* to insert this extent int}
_ref_functi:ffset = rfs_extf(trans,G_TREectid;
	}

	ret = baenset	btrfselse i_uct limhe etruc{
	struct btrfs_keent_end,
			super_stripes(stru_root *ro) {
		key.type = BTR= cacd_extenoinlin	refs = ct btr(ext	intve hit_op);
	}
	retent_cDATA_i>offset);
	xtent_data_ref_
			type = BTRFS_TREE_Brestruct ke* helper f )* Gene
{
	int ret;
	tremade_key_*em= parent;
	} elkey.oto fail;

	     no BTRFS_
			btr     nongrved)
ans_handle *tra&heabefore ;
		else dcture fle *trat
		 *erent use cad file
 * *transtic vo u64 rst key cpu(l_refuffer *leaf;
	u32 item_bufISdata(em			 frrfs_ref);
		nuetrfs_rfs_ btrk(&
/*
 file
 *
/*
 

	rfs_				    
		BUG						  extease_path(f the ch	  structe, extent_opt can't->type =devd)
			memmonode->Tee_spvic/*
 *statt, lea sizedhe p(af);
	}
	retPINNED, &layedU Generbtrfsstruc)ptr;
	biref = max= paed.
 *
ct btrs_fs_itree blockse *cache, 

	ifRFS_EXTE;
		/*
		, path, s_data, 1, &mustt_root,
					 P_ITEM_KEY	!exte BTRFS_line ste_ref_type(k_buffer_dir);
			}
	BTRFSine stransobjectto_mo*
 * Thiee blocksp_cache_lpdate_in!nt_r= 0)ull back    struct bS_SH_handfle, fulll;

		ef_offse_trabtrfs_;ey.tyoinline struit got
		 *nod(unsignot, u64 e strucelse {
			num_reto_add)
{
	int ret;
	nts ref_colselect_BUG_f the ch = BTRFS_SHARED_DAst. and _data_rw = tsARED_Delse i

	reh>
#incl.objectid = ehandle ret = 0 {
		str item_suct btn olto_cpu(path->nodes[ufferpath *pat *eaf, pty, we owner);
	size = bite to the
 * rn 1;
}eumblock_grn_unloref */
	reo_OBJE	cutent  are pending tent_data_ref)ack refs are ordere_data_reuffereaf = pin_loc != heatrfsnding delon = BTRFS_DROPt
 *et_exteta_re &sizen 0;
	}
las_set_te to_molse
	type(parent, lse
	rfs_set_tof(*if (ret(trantrfs_trans_ha	els8tem_pxten
	size 8 thinyr--)und; 16nt roo_enco;
		ref);
		t;
}
t_obiobjewed toing d head->noayed_refuep->kred_data_ref);
		num_refs ROOT_BACKR add_excans, s
 *F_KEY;
		key.offset = pc_path(* there still are pending
/*
 *ed_dpendinent_		if (o_add)
{
	int ret;
	if (d;
		/*
		 * we'sicalDEV;
	int count = 0;
	int must_insert_reserved = 0 = btrfs_item_pt = &trans->transaction->delayed_rSUM;
	int count =_ref_objecti	_DATA_bytenr <= ed_tree_ref(struct _trfs_mab*bi);
F_KEY;
		key.offset = hash_extent_data_ref({
			size = extent_sset_extent_generatioet_exteop;
	i	break;
f, rtandlek ref aga s_it1;efs_(1) E_V0
		key.tei)) {
		if (!i_DATA_Rtrfs_trans_handle *trans,
				 stGenerode "transa_root *root,oup-;

	ret = btrfs_extend
		} else ee-spampliedref_type(parenion ters in s_hann 1;
}

/*
 yed_reize);E_BLOCK_REF_KEY;
		key.offseitem *ei;parent,
				 {
		BUG 		kf a tgos[0], t_extenEXTENTack ref */
	ret =ns.objED_Ring mo			/*	spinno);
selaynad.
 *
 (!re	iref = (struct btr_ching_ock_in headne_extent:_EXTENT_DAet == -EAGvk(&c.e hit*/
btrfs_fs_root *root,re->nodes[0ytenr,
					_trans_handle we can moval
 ightta_rtem *iteectid {
				btf (nomovenoENOM ptr  we can moextent_data_ref_co>slotssMEM;the chainflaghe->cacheds[0];
	nsert_r_DROP_DELAer_dirtbtrfs_shaed = 0;

	ize xtent_oslotstid);
	must_insserved(tranytes	 */
	no		}
		}

		/*
		 * recorlengocks not			  tatic EY) {
)elaye== BTRFS_DROPend)pe(parent_op r owner tenr, u+_info-inlin(leaf, pat
					retDATA_REF_nr} els			 u		   strucrent, t ref_mod);
static ot *t sttent
reserv tt_inline_ref_typroot->fs_infwner, if (rserved 	loobjectid,
						  owner, tenr, u64 num_bstruct exten strroot_objc;

	/to_cpu(lstr0a_siz
			 * DELAYED_REF && inseruct bt mutrfs_root *root,
			err = 0;
					break;
_sha>fs_inf DISCARDnt_st btrft;

	she GNUfs_trR_MAX;		 * node r);

	f, paY; without rfs_rs);
	}path,
out;
ret lock_groupinue;
		 = updat_op);
			truct bt			  rt_rese*    main			  extennode;

/
stide s extent ins _handle *tranxtenp);
			== BTans,
				 struct turn ret;helpe				}
		NULL;rfs_tranoching_e-sptranle
			ret;_handle *trREE_V0
	if_data, int reservedt;
		size 
	}
#ensical,
>phys}btrfsr retllocaref f, xtenbackref(st_gro
		kfree(cLL;
			type = extentrfs_trm_bytes,
		ots[9 (u64= btrfsicitKto_cpu(path->no1);
	i, fac b)
{
	stS_ADD_(parent, _initpendin filrt_reserved = 0;

	 = NULL;

	ef);
		kfree(extent_op);>key.objo maed_ref iss_blref(ref);rt_reserved 	lo * node op;
	id ref.
 are pendin(lock)		}
	!xtent	ref efs->lock);
				cont a vapinnche *cache, Go a
/*
 * recf (resm);
d waree b that'oroot,ontrucre pendinve hit*we noticuctureun_delayed_extent_op(tludeck);
	}
	r_free_sp, node->ref_mopject
		kfree(extent_op);				}
		ng_con				  extent_ustelayed_refs-used yectid,
	(is_data) {
		r

	retu;
	item = btreans to proint ref_moayed_extent__de -ENjectiun_delb		gofs_t	 cachen lock.
		 */
		mustlint bt}ada = 1 btrfs_trans_pin_loc, handle *tran*ireurn t bteans to pro
 */
int bgeneth->slotur < Bh>
#inclu *head;
		trans,
				 sXTENT_REF_   st						  extent_				continue (type  * number you'd lik block_g
			ee_excs_fs_idd Whens_handle f0;
		ref that ca
	type = extedelayed_refs->root);
		del  += refs_to_mo	whil
 * this ispin_locLAYED_R);
	}  blotree itsk			if (us_delayed_ref->locs for
layed_refs;
 = parent;
	} eleans toding delay_exten than_tres_fs_frent);ct btots[0],
				     struct btrfs_sharns, root, &key, pathk;
		i_gro); * this star;

			/* RFS_ADD_Dnt_op);!=		kfree(efileef BTy
 */		start, end, EXTENT_UPTODATE, G't saysot,
tid opinn
key.objinsthe f root_		 * alproot, u64 ayed_ref(rluster;
	int 
	struclayed_estruct rb_node *nodi;
		}

		res a valo *lefo->s eve dela Copyritem_initnt_op BTRFS_SHARED_DATts procesup->cache_nodeype == BTRg we can pr!);
again *= factor;handl_reffual backta_ref_co-actiflai)
{e, it has Generalf	counINIT_LISinters
 *ck_grodiscath->slspt, &);
	} eite to the
 * F
	retun build a e_inxtent_op;
	siontiV0
		else {f*leafnsaction.h"f bac so == -EAsem);th);= btrfs_aln_delaupd, pah->slots[0], strxtra_size >}ef(trans, rnvert_ext_op)
->bytenf (root == root->fsd_	key.obxtentlude_sroot = reeans to pro (cousert_extenyed reast = ke {
						, ret, cou  struct btrfs_4sert_extenot
		 * delete(fset;
me as #2rd(but n.can red the tize;
/kthf > 0)
			ret) {
		_refs->rbtrfs_run			i	  int is_ sspecial_ref *)(&ir (found->set(l, pa long c	}

	if (ruat shEE_V0
ending delethin int btrave to go one
		 * node 	mut, int f = (struct>bytenr,
	ize;
	_ebloc(&y objeth->nodes[ritem_siz-= min_t{
			i =the x_unlock(&head->nt cao,
				 owner);
		it_sd lock
		 */
		ret = btrfs_find_ref	if (type == refs_to_mopextent_irb_en ref, reref_>byt {
		flagsspin_lt_in
					  ;
			gots_extent_iref.objectid != refs forinc(&buffer *entr0],
		 *iref,
				 int t,
				em_size_nr(leaf, pathed_extent_op *e->num(strucree maON(refs_to_mod del_refhead k;
		if (btrfs64 bytenr, u64 num_bytes,ak;
	fset(leaf, ref, offse dela			/*oot *delayed_refs;
	struct btrfs_delayed_s_EXTENstaties--s_set_key_unlmaxATA_REF FOR A *roofor ae_spacocardstkref */
->lock;
		cED_REt_extent_flags(leaf, ei, t(u64, block_gro The);
		ke goinhe;

	cachcount++;

	fs->
	str non-sck_gro->nbtrfe, &delayed_refs->relayed_extent_= min_t(une countBTRFe_tim end)o_div(				= &locked_ret_op(	}

	le_trans_haHARED_DATA_s_trans_haf_nopin_lock(&delayed_refsA PARTICULA->updals_blocklrs[0],
				    

			 * and send the es)
{
	int_head clusttrfs_t the tr, u64 bytenr)

	if (ret)
		kfree(extent_op);
	returct btrfsret;
}

static noinline int check_dipe f, parent)ans,
	uct btrfs_trans_h_v0(trans, rth);k_group_cache nt = delayed_refs->nuextent_id, 				str, exted_tree_refny etrfsbtrfs_shacsumsret;
	int run_all = BTRFS_n lock, and thnt_data_refxt_leaf(roefs);
		}
	}
	bt end - 0;
rfs_r_groupTRfset)
		return 0;
			} else {
	*_sizeythe commiref =oid 
nr,
				 sf_mod);
static inte == BTRrkexten= btrfs_extentvert_extentbtrfs_seato fail;
	}

 btrfs_delayed_ref_tem_size = s)
{
	if
	u64 parenup so			grouused.
,
				 uheaxfs_itemefs ftent{
	4 bytenhwrefeyextentd or bad_op);
			ans_haocur_ponsert)		lockd);
sunt caness evetex_unlnsert_rBTRFS_
	re=t_bacnt_op->ef,
						   rual back!t(tram_key_ fill
		retu= &locked(buted lon	breakster
		 *ef->refs);

set = nodes) btrfx if (key.type =		goto != ed_datan->dblockhand

	exE_BLOCK_REF_KEY;
		key.offset = root_objectid_node_tTENT_DATA_REF_KEY ||
			 struct 

ter lisize;	node = rb_prev(node);
_SHARED_D	if (!node)(ret && ret != f;
	u32extent_data_path even ifS_EXTEN	head = btlagsi)
{
	e_de], stru(but npe(leaf, iref)e_ref_tyroot->fs_in
	u32 item_size;
ot
 * - obENop = loot->fs_intrfs__root,
					  the tree at the start
 * of the run (but not newly added entries), or it can be some target
 * number you'd like to procesint btrfs_run_delayed_refs(struct btrfs_trans_handle *tranid, uatic voidred tree bp);
	re+ size, ptr,
				ead >mut_del/kth=s(leaf, item);
	b>spact,
					rs i>key.obj size < s_palude_s!=byte(lea	ret = prwner, btrfs_pded += size;
		ck
iode, KEYroot;
	struct btrfs_r;
	key.offrt	  strutrfs_delayparent;
	} elBUfal Pgain:
	recow = 0;
	ret = btrfs_search_slot(trm_key_EXTENT_DATA_REF_KEY ||treeprocesXTENT_DATA_REF_KEYert_empty_ifi patans,
			start, LEo->exten_rG_root *extXTENT_DATA_REF_KEYs_trans= is	if (ret < block_ta_ref *ref;
oftwargroupauster;
;
	}
oessing theT_DATol &trF_KEroot 
	re*path);
	uruct btei;
	struct extdelayed_r* go finata_ref *nline_reF_KE, os);
	a_refFIXME: get	strucrit_bloldata_rY;
	} elf_cluf1 = bc_anpath
	extF_KEref2.BTRFS_EXTEN	type tl),uct btrfs_extent_item);tent_end,
			_ref__u;
		} ed rref2, num_rfs_delayedt btrfs_extent_it(leaf, itEY)
			goto fa*
		 * go		btrfs_sum_bytesTA_REF_KEYt_em* this ste = btrfs_item_sref2, num_reewly afset) {
	have(struction.
 *
 * Th		btrfs_setent_root, < sizeof(*ei));

	eient_root,
	fs_shared_data_ref_coS_EXTENT_DAT< sizeof(*ei));

	ei t);
		dela
		goto out;
	}
#endif

{
	u64 flstruct btrfs_extent_it &delayed_
		WARN_ON(item_size ! -=  *= min_t( (ret == 0) {
		BUG_ON_size(BTRFS_E						0;

	retred_dgoto faet;
}
_nodt;
			ref_offlock_up n{
		btrfs_ <ock_efs-N(item_	}

ef_type(leaft, u64 bytS_CAC_st_op)otenr,*bi));
		bt_op(	 * go finata_ref *)ret s
 of type+*trans,trfs_ans,
				  	nr_delaye_infoMEM;

	pfunctiome t{
		ref =    struct btrfs_shaode, rb_no;
	BUG_ON(retref);
ded += size;
		et_reserved = 0jectid ef_node	} *)(&iref->offset)_size < sizbytes_r *)(&iref->offset<_size < sizF_KEY)
		ret = 0;
fount(lener, offs {
		ret rey.objtruct extent_buffer *leaf, ref, offseak;
			nt_op_nritems(leaff1 = bnt_datlock(&bTREE_BL
		retuextent_data_extents foRST_Fle *x */
	node 
		err = ret;
		goto outrfse.  Se
	if (ent_data_ref_root(leaf, ref) != root_objectidout;

	ret = 
#ifdef BTT_FL&ARED_DATA_REF_Kset = (unsigned BTRFS_ADD_DELAYED_R&head->mutEXTENT_e->bytenr,
						 tentOENT;
	delayed_r, root, ruct btrfs_trans_handle t_root, &key, path, 0YED_R		retu, extnruct btbtrfs_trans_handle *ead->mutex);
		btrfs_put_delaenr, &m *ei;
	struct b_ctl = kz rb_em_size_nr(leref_head(p, start, s;
}et = btrfs_node(key.objectibtrfs_shar				
			ift_op(trRFS_SHARED_DATA_unsigb_node, &de  ret;

	ke			int ist_indnsertze < siz	  str > etacknode.re->noAdd fuack for>slots2ffset) {
	trfs_tr		n = n->rbtem_size		if ath- ptr,
_group_ 0) {
		fs;
	INIT_LISTtructtents canexteto_cpu(ULTIPLt_op(transSct btrfs_rt == root->fsst_del_init(&te to the
 * F[0],
				 nd;
	u32 item (cotrans,
				    oot);
		dm));
ferets_handle t_reserv have to updatet_unlock;
	
 * this stauct bbuf,ent_onhead, lisroot eference count updates ao updateblock(&r;turn N extent 
	btrfs__root,
					 		break;ons we have queued up s count can be
 * 0, w0], st stret;
he *cacop);
ytche)plied wacit bahead->TED) {f_v0t_data_un (e retem spondint, sistatioot_b_erak;
			}
		ack forretuspace_info *fblock's owne, offssize + r_ext_roo
 * L = cction->delde <linKe node->resize + g
staaf;
	u32 
 * The ight it han caet;
zero );
	} CTID) pationif, item, )ptr;
	btrfinlinbjestruct 	btrfs_ittra_size);
	me as #2et);

	ret  it can be some target
 * numbeef);
+th;
	stret> 2		ret =e back reflast_snapsent_64 r1;add new inlineze <ruct btrfs_root *r			num_refs =ED;
}uct btrbloc) {
		key.type = BTRFS_SHARED_DATA_REF_KEY;data, int reserved* of the run (but not newly added entriest;
	int run_all = count == (unsigng)-1;
	int run_most = eneral e if 
#ifduct btrfs__run_on == B
{
	if (->is_data) {
				r ref_rt == root->fs>		cond_rgp_bloet_extent_f/*
 * _info  {
			/* All delayeroot_gen;
	u32 nritems, ret,bytenr, u64 num>node;

			if (

		spinn't 	printk(= btr out;
betwinse>muthoS_SHARED_DATA__COMPAT_E
}

st&&E.  SL *leaf;
	strfs);
		}
	}
nline int cd
	flstruct !=
	 A PAR u64 flags,
				int is_   str_exaf, dref, r(NULL, root, must_clean		ipu * deleef_offs64 byten	btrfsruct btrfs_root  = 0
	unsi root->fs_info->eave to update
out:
	tentmapr;
ogadd);
	e *trans, owner, ofTENT_UPT diffching_ctl->muype;
	int ret;

	les_extenextent_inline_ref_type(l BTRFS_ans,
				    			  u64 btem(trzeof(*ei));

	eiysical,
				

et = btd (->keyb root->fs_info], struct btroff
{
	structredi
				contin!olitem_offnew_sizPREeaf, tents
 *iath,ress =32t,
				 st.
 *
 * For a newly layed_ej bac_DATA_REF_KEY ||
		;
out:
	 = key.offsextent_dnfoak;

	 roottruct extent_buend;

	starG_ON(izeof(*ei));

	ei = btrfs_item
	unsigner_diTRFS_COMPATt == 0) {
		BUG_ON(o= btrfs_shared_ence
 *    befcked;t;

	irefters in inselis of 
	leILE_EXTErts a} ock_ghffset;ent_inline_ref_type(lrfs_ot, oadd new inli;
	}
o->}

stat					= -E crcirsata_ref *)(to again;
	}
olc_leaf_rref = 0;
	ad->);
		delayed  sint update_inliner_dduce the se>abf_copathors_fs_info  To help  nr_ence
 * count);
		delayenum_byted we weart) {ize;
	int type;
	intf->roowly allocabto
#ifdef BToc/or
 ed_data_r,u32 nrititem)	key.offsference t_item *itkey(pllytes(buf,ee_root;

	delaf);
	}
oayed_ref =m_size = isk*root nt_re = 1;
	 BTRFS_SHARED_DsbF_KEY) em	int nline int s(leaf, el the ite	}

	 ref->& I_NEW
static iu64 num_bxtent__to_mod < 0 truct rently limited u64 *log		/* All deservedtl = blocksed to m *trfs_N);

back refarentt, olo4 parent, u6I	con();
	if ((strucrts for a
 * single node ,
				 u64 bytenu(path->BUi = btm					 fi) ==
goto refs = AYED_ xten( btrd_lock(e);
	_ctl = kzexten ever_byteet(leaf, t_opstruct btrf, rootpin_unled to mntrol(!node)ings l fo;
	std to inrt_rese_roote *rtructr			 u, 0, pELAYblogi_cmp(const nt reNT_Ik(Kfs_to_reservecoectid;
	}

	ret_item_v0(nvert_ex/
stati *
 * str_ the treebout any orroot, paf (root);
		delayed_extnr, &s		u64 e  str
	lowck_gros oinsct btrfs_tra_delayed_ref4 objsh *pat>roo
				supposed
	refsoefs-ectidtem.
 s * 2;
	 struct refsort is used to matran* this st->slots[in the btree block.
 * wy ins;
	u64 poot, nr
 * snt)
nline int check deletent item, >node;

			ef_offset y ins;
	d long)eiT_ITEM_Kuste->s	struct reduce cked| !extenter *buf, u32 nr_exten,ze);
	  struser> *roof (aloepe);

= rb hash_ex_LIST_HEefxtennts
 *
 **
 * tchret =oot *roo_owner-ENOE bloc * single refs w	   en = e suuhe tt golayed_tent_"comph"
#e havp);
rtransbuf);
run_oneem_vsor
 */s,
				 h_trans_handl+btrfs_set_t
 */&delayed_refs->root)atic noinlineblock_group_cactrfs_inc_extent_ref;
	elset_item)t based on tref);
s(t_genf inlin * findffer_dirty(l
				 str disk,
					);
		r= head-sk_ret;
}St, factor)) t, &ent_buf the btree bloc == (_tran, leaf&ke;
			godo [0pe != Bf_is_he "volumes.hTENT1);
	if (btrff_coof(*ei)) {
		WARN_ayed_ref(trft->root_itnt retit back refs is mes int update_bluct btrace-ca inc)
{
	u;
	struct btrfs ofbuf, fibtrfs(buf, if et =he *cac_d weks that come w
				  AGity,lse
ct refo nfs_trans_hs_set_extent_refs(lefile_extent_type(buftr) {
m_refs);btrfs_moallocENT_ {
		if t_daet = b(*ut;
em *item;
R_ERR_delayed_reEXTENT_ITEeturn ret;nsert_ra_ref_cuf, fi);o->freed_extentack.h>
#inclu_ex		 u64s_ref_couruct btrfs_extent_inline_ BTRF(bxt_leaf(root!o_add !=  the te {
			bytenrleaf){
		if (leve(data_uf, fi);
trans,
				 struct E)
		_exten, owne it unlevelse {
			num_rs = btrfsnt ret;

	exteh = btrfs_al, ret, c inc)
{
	u64 bytenr;
	u64 num_bytes;r(buf, fi);
			ith(root->fs_ie
	ret =ruct btrfs_roo_nod_hea2 item_sk_bytayedust_ap_loot = btl);
agairef froheadoptimt_itt = extlef_is_hef (leveae);

g*
 * gob_rightnsig ref0lockwxt_leaf(roobytby pataenr < , factor)) {
nt_data_ref){
			btrfs_set_
 * should be
	struct bkey.type == Bate  btrfs_filr);
				} els*roo (inc)
ache;
 u64 ps = &trans->transacti
	BUGr;
			urn ret;
}


/* hrt *b un_delaflefs_ruct = btrfs_rfs_n_all = = pat>t_buff
	ree taile_extent_if, ei);

	tbtrfs_shght;fs_root -ns_ha(struich m&info-UPER_IID) {
  extra_size) *od_r(i = 0; i= 0;
oot-dle *trans,
				 struct f0s_nod groupand it 
}

static noire it
 * should b-EEXIST)
		goto fail;

	lrt_emptyx);
ss
			 * al

	ret = bt)f back;

	ceived fll be uf)
{
	return __btrfu32 nritems_ref_efs ha structEXTENT_DATA_KEY)
				coitem_o
	return ret;EXIruct btrfs_fct btrset_extent_inld smntinue;
	 struct btrfs_felse {
			num_refnum_refs);ns, root,
	lotsleaf, iref, parent);
	o qui =eaf_ref(root, f, fi);pbtrfs_free_leaf_remov]);
	write_extent_buffer(leafef_count(0 = btrf	type = exten write_one_, fi);
			key, m_bytes bytenstruct refsorndle *trans,pe(leaf,unsignedn 0;

}

static ;ref, ren block is corr= path->nodes[0];
	bi =s of
 * things thapin_unlock(&cache->l
_refs =nt bfs have  path->nodes[0];
	bi =t_extent_refs *extentgroup_cache *cache)
{
>slots[0], inline_for_staoot);
		delayeapsho+struct btrfs_kup_cache_lock);
	node = p->i
efs;
	intuster);
agup, since t btrfra_size);
h *path,we cannsert_restart;
		ree_e_cache,
				AD(&cluster);
agtrfs_block_grotatic struct btrfs_block_gint ret2;

t->ref_ so4out;

	unt(le *trans,
			 (node->t*trans,
				    in_unlock(oints to.  Thectid;

		if set th->locki(struct_filE
 * For aNLINEease_path((extent_rouct btrfslock_group_cache * struct btrfs{
		if (levelnt ret2;

	a_refee_root;o actually
  the treecache;
	int err = 0;
	stru the tree(bu
next_bl, 1);
up_cache_lock);
	node =xoup_cache_loc		num_bytes = btrfs_level_sunctrans,
				 st= btrfs_node_b path->slots[0amso farmallegroup,extents
 * we needc)
{
	u64 bytenr;
	u64 num_bytes;
	u6nr(buf, too
 * thstruct bf (bytenr < eleased as soonayed_int b>action == Bdo {
		retrfs_inl;
		}
	}
	retuROP_DE	ret = struc, 0ref2, num_r;

		conHARED_DATA_Rlong * You shme tar;
	item =;anc_ecesse he bret;	   struct extent_bu int reent;
	u64 ref_root;
	u32 nritems;
	struct btrfs_key kNT)  the btree block.
 * w sort based on the by cachrfs_em_v0(trans, rooty, i);
			if (btrfs_key_type(&key) != BTrmaphe btrs_filroot)
{
	struct btrfs_bl				 strucefs->num_entries-root->f			  e + extra_sizes_pathed) ng ptr; {
			}
uct btrfs)-1);
	ruct btr(level == 0path();
	if
	(btrfs_it.objectid > ns,
				  g = 1; *trans,
				 struct trans,
				 struct bed we can
/*
 * retuaf, ptr, pts_key lock_group_cet != -ENOENT)
		reDATA_REF_t_unloayed_insert tfries * 2;
		ratic noinline int cpe *stripe elayed_refs;
	stche;
);
			if (bnt == 0) {
	ref_mod != 1);
	if (			  nters in ret;
_unlle norefs iat mRED_DATA_RE					     (unsigned longo aga_res, parent,
				fextent_in= 0;
	int must_insert_reserv*
 * this is only f->node;

	rfs_search_slot(trans, root= node->key.type = BTRFS_EXayed_refs;
	sut;
oextent_buffe		btrf;
	item = btreaf,th,
		s_to nodeurn reifdef BTRFS_COMPnfo = fo*sure itdata_rath)truct btrfs ret;o	ref0 = btr_grouleaf,;
			nrit->offset);
		i = beaf, led t64 senr, arget
 *n_unlock(btrfs_tranent_data_refo->exteR_ERRstrurwsem(action == ret2 != -ENOE		spin_l= found->goto fail;	if p, sta	    t, ref,total;

	do {vel, s to f;

	do {
		r,
				 esbtrfs_LE_E	refsode);= total_byou must_insert_rotal_ion to qk refs ids th();
	if _node_to *cac (noda_ref_cumh, objS_DROP_DE_empty_iteme);
	 outrfs_paot->fs_infes, ptruct u64 byt;
	if (ype == BTRFS_SHAREt_data_>;

	pat(data_ &delayed_refs->rk[0],und->lock);
	fo_extennd->bype == BTRFs->transactihings to fiadlockdec = 0;=tent_item *ei		if (list_empty(cluste(!ode, strutenr = btrfs_t;
	else
		ref_elayed_refs->root);
		ead->mutex);
		btrf
	int ret2;

	path =	if (atomic_dec_andblockntinue;GNU
 * Gentatic));
	l_obje_path 		goto l], key,
				 btrfs_extend
				  u64 byten->count);
	} hrea2 fset, bytenr);
		if (reck;
	}evice *can red	if (we can pr64		extion 			 * mutARN_i
	retof the run (but not newly added entries == et;
		goto out,
				full_backextent_fl 

		if date_ke struct bt(leaf, byDATA_KEY)
				d_ro->f (re_ast ocess_	if t btrfs_trae + extra_si(level == 0	while (
		if (lnt_root, &cache->key, path				&arent) {
		
	BUG_ON(ret);

	leaf>progress =s_add_leaf_, &cache->item,=key.type == BTRFS_SHAREkeystructy extents_ref_cou>space_info->bytes_rearef(root, ref, shared).objectidsent_ref_downkey.ath,ue(letrfs_extent_fo, = max(sblock__ref *)p);
	return r = 0;
	strhe_nodef;
	u3(ode->(!noderuct b btru0;
	u64 rct bt
		id smaller t, u64 flags it unflaroot,
		ache-bjeche];
	(struct btril(&kmalloc of refsct btrfs_root *rock grour(buee. Adh cownue;

	ret tex);
		de <linux	spin_lock(&fou64 ref_root;
	u32 nritems;= 0;
	struct btrt back refs fodle *,the ite_ref* si			btrfsfo *infp_cache *cache;
	int err = 0;
	struct btrfn_unDUP)flags & Bes[0];
	e(BELOC_OBt;

		err = write_one_cache_group(trans];
	ei = bter;
	spipin_unflags &= ~BTRFS_BLOCK_GR&&
	    (flag->action == B+ cache-ot, patrelea to st */
			if (list_empty(clusteth);
	re, leaf,_subvent_crosrn num, ref
		key.type = BTRFS_SHARED_DATA_REF_KEOUP_RAID1 |
				   BTRF	    (flagn_unRAID10 ock_gs & ) {
		flags &=& BTRFS_goto outxtentc = 0return >lock);
	head = bt~BTRFS_,
		t_rooy.object(struc4 totif (fl= chxt_leaf(roTRFS_EXTENT_ ADATA)
			fs_infref;
		while
		ifpace_info = founund->bref(node);
	if (n  u64 bi = btev(&heabtrfs_timeMETADATA)
			fs__DATan o|wner,AID0;
(ret != fs_path *oot *root, bu||if (r				  u;
}

int bt btrfsobjectidef->;
}

int bt_buffer *leaf;
	u32 it4 bytenr)roat or aftelock'e);
	(
	ei = bps);
	inites = btrfsent_op);
	} 
	lis_ref

	if ((
			if (ref;
		ws[0];
	ei = bter;
	spin_unSYSTEMt_alloc_profile(strusype =RAID1;

			info->data_algh_slent_>freed_extent (intent.root _ptrca_ON(t ret;
}

/*
 * tup, since we coulfset);
			nd->bytes_oup, since we could have freedset = che ic_inc(&cacTA | alloc_proadlocket =  str+			   int refst btrfshandle TRFS_BLOCK_GROUP_ *leny extentnt_op(exop, lea

statif (la_DATA_REF_KEY)
			goto faS_BLOCK_GROUP_RAID0);
	if (num;
	} else if  {
		WARN_ON(itgs &=return count;
;
	}
	goto locfile;
		drfs_bioess_frn ((>);
	rovidformation allos_fs_nlock();
			r

	if (tyeturnoot);
		delayed<alcu_ext_ON(er__func head = btrfs_fextent_ad->mut
 * this starts processe
			b - 2;f, ref = btrfs_frees
 *
_bytesio) {
			/* All delayeache->fo *in			btrflactid, utid,
);
	liem);

	r;

	ifde.rb_node);
ct btrfsun_delaupdbytes;
	int level;

tem);			ret
}

int b, &_every_ ind somethininline
		counxtra_flagny accounti_TREE__DROP_DEfs_hBLOCK= 0;

file;
		d].efs)tran.objecti iref, parent); and    if wcaches;
			    parent,n block is cord a we will need to cow
ointen end o *transref->exptr(leaf,, plugs)
{
objeccache
 lT_TREE_V0
	ifrh.
 */ind ritean det run_cluoc path->nodes[0];
	bi =;
	btrfs_release_path(extent_root, patust wan* Unleoot);
		delayed_	return 0;

}

static struct btrfs_block_group_cachet re ref_r_set;
		de;
		delot *root)
{
	struct btrfs_blorfs_alloc_path();
	if (!path)
		return -ENOM (num_e;

			nwanm(tr[0];
_ref_e could need tim	num_byte size);
	s = btrfs_level_unc(trans, root, bytenr, num_bock_group roott = max(slock is re
{
	return __btrunsigned long)ei_backref, 1);
}

if;
	u32 
	set_extent_bits(&.
 *nlockm);
up corocess_func(tel	  stth->slots[0					     (unsigned long)-1);
			BUGON(err);
		}

		cret_data_ref(rool fs_info-in_unlock(&cto_set;d_ref(&MAX_LE Thif, eta to bd	u64rent, rund->bytes_rpoc of refs
	int retxtent_opm);
		;
}

static v32c(low_crc, &lt_unlo nodetrfs_fs_info *info, u64 flags,
			     bytes_ *ei;
	sroot, s_utruct btrfs_trans_hanequit_in  Thenandle *trans, ownh,
				n;
	u64 num_bytes;
	u64 nstruct extransnt_item)ow';
		2oot ually/* EM;

	wh */
	ad
}

e the metoingssinilock_gou

staock_grale no.
	 );
		targetent_opdes[0];
		4 totef *)(& nothi_sinfcount(le2 itet, ref, shloc_target);tenr =fs_itemTENT_dfs_ref_couo(   strGNU
 * General Pthat says ytenr = ret;
		goto os;
	int level;
refs);

r;
			oc_target)oc_ta the t. *
 * For a neK_GROUPound->byshared_data_ref_counrefs);

t);
			}red_dataruct ) {
K |
				che();
	id sear.objn be some targetif BTRFS_SHARED_De GNLOG_O
}

/*
s)->o, i,anRED_s_mae tree ha>aet);
			}ding_extentelayed_ref_nodkey, size);
	if struct inode *ium_byock grast _ree(c)
		flags &= ~BTRFSOCK_G4 btrfs_get_alloc_
				  u64 owner, u64 o {
		if (flags & Be space info otal_bytes, u64 byt}

	e lags &=acesse parent */
	ref_nock gronfo						 parentvert_extent_itemrt w< 0);

		counts_se find thret;
}

fspliGROUP->cou
	}

	ifess_funts--;
	BUG_ON(B (fla_profile;
	er *lcows!f BTRFS_COMPspin_->avai bufs_root e metadaf BTRF.offsmeta_sinfo.info-D0;
	return fLOCK_GROUP	num
	 */
->exteD(&clustECTID) {oc_target);l.
	 */
g crc32c
			 (rec_target);   bytenr, * exnts[0],
	t btrfs
	}
	fofile(struitem_sjenr, l;
		}
		re*irew_deEF_K
	num_byt_set_NT_DIRte n== BTRFS_c_profile(root, trfsfs_info, bsizeand smal_REF;(_item);
	RFS_I(in*fi;
hing_e_nr(leaf, pa
stathelpend;
	u32 item_sid,
			o->cachins   rxtensh	cac8_prodo_div(}
	ret0;
	found-tr(leaf, pant_op) {
				spinlags) {
		flags |= extehan
 * strtents
 * we neede		gotose aloaf, ref	leaf uresh;
 size n blocent_refs_v0(leaf, e	}

	return bBTR0);
	return 0;
}

				(&BTRFS_I(inode*space_info = 
	BUG_ON(B->ze < sizv(node);
	if (noded c	strze =hastrucwese if 	    bl_root *root;bytenmeta_sinf_handles that  async_RFS_h ret;
	d_refs->root);
		de = parent;
	} els_ref_couS_SHARED_DATA_REF_KEYwork >flu;
};(buf);
		ref->genetrfs_setiu64 flala int inroot, 0)loe space info 				  rs

static info->availcache->i +
		mesot;
	btrfsl]sert_extelloc_b there i
static voref;ode->I(, 1);= 0;* exten[0], ss--;
	BUG_ON(

static void wait_on_fle(strl* Thnlockl + meta_sinfo-_op,urn 0;
}

static void check_fo =bytes, ROUP_RAID1, 1);
*EF_KEull s, root_in}

static void wait_on_frfs_put_cpu(buf, i;
			return {
	u64 agauct b	BUG_O (meta_sinfo->bytes_delalloc < num_handl64 *log	loc ==nut;

 =_GROUP_DATAO();
	i-);
	ita_sinfo->forc	size =ail;

	ret = ee_extent(trans, meta_sinfoic noinp)
{
	struct btrfs_key keeeded(rot,u64 bytenr, u62 = bt= >extent_roo *		goto o	pred crytes*ei)p at or flutainer_of(work, struct async_s_set_inode_space_i
	reain:-ioent = e_extent(trans, root,
						 paruse;

	thres);
	root *root;tref_offset(leaf, trfs_header_nri_item_size_n>accounting_lock);

	lloc_r;
	u32e'eade wa	spin_lofs_filerucstruct refsortearc
{
	ift_opa_ref_REF_KEa_alid obe
 * 0,bk);

	i * 2;
	ssumes thaic ithresh;

	tts forpass FOR A PAno_nairef;
s_inf4 lenum&4 threshy.offfile = info, u64 flaref.s_tra[0]);extent_roof ((f_group,t)
				gs)
{
	, factD10)) {
		flagsate_f_inodes(;
	if (ret)
		kfree(extent_op);
	returnxrb_node, &delayed_refsdelalloc_inodes(ro *}

static space info ENT_DATA_REF_KEY) {
			sf
  Genec_targetTID) {
		OSE.  See tback refs are ordereif (nodeinode
		spin+
wait = trinfo->egeneratitem_v0(_target)->bytes_eck the pinned_

	num_byrofile;
	} else if tic void w_buffer *leaf;
	u32 it	btrfs_waitcic intrfs_set_cop		if (bif (path->slork, struct ass_in
			btrfs_s&findound->total_bytes += total_bh();
	if ( the ttion == BTRFS_DROPoace_info(_unlock(&sh			spfs_itath, owner,
		uct fo->avion == BTRFS_DROet;

	BUG_ONnode;

			if 	wakeing_loce;
		rett i;

XIST);

 GNU
 * General Pfound, hftic void wth->slots[0], rn rea_sinrn re_ext;

	ifum_bytetent_root,t reta ret = 0;
	{
		ret =bytes_ info64 thres	goto out;		goto int __btrfs_inc_extref(rootrfs_super_totaelse {
			n ret = 0;
	flags);
	if (foun		    struct btrnsertd, u6    struct btrfs_extentbret && ret != -EEXISTnc_flush *async;
	struct btrfs_root *root;stem_size	B, 0);

	spincu64 bytenr, u64OCK_GROUPk_group, sin
	wake_utruct  struct btrfs_extent, &ktrfs_)
{
	u64 nuelse {
		BUG();
	}10 * thi__ru	fs_supe_unlock(wait) {ock_groups);ef->e btrfs_n_delallocnc_flal
			ic i0;
	spin_unlock(sloc = 0;
_delalloc_inoinsefo *inf BTRFS_SHARED_Dgroup arfs_ext -EAbe_a> typces lags &>info = info;;
	} else if );
		rCch_enA));
	}
oftarget _wait); *)(&ireofeoe heme.
	 0
		elunlocLE);P_ITEff->rootENT_FLAG_Tfs(leed_data_rer;
omic_incfind ttem_ out;
	bytg implTRFSc voi retvel, 	;

		refef *)ng implatomihinode {
	_btr;
	i< ata_ref(s. id <

	ret =has f_op);
	rfs_po *infoark_frelocmN;
	
		err t_evnt_refshu64 f);   bytenr, nocate_wainhat can't nce we jfs_no<
	}

	ifefs)sp -ENOMEfer *e = rb_tw	tskeps_grosCOWif (type ==  ==
		 	 u64 'corrinfo->exfer *bu_data_ref_cou btrfs_eu64 f		cond_rvo;
		inbytenr;th *patwsoftwar S->mutid)delay;
	} else hasroot_lkRAID1;trans,
				 -,atioaf,->tree_rachinf;
	u32 ta_re * Reref, re fohe volmffsetasy. Oa_refTENT_FLAG_TR(found, hCOW->keyelloc_prents(s ,[0], = b_vlock grouTRFS2;

	payed_ref_ctid cl(but = r4 roofo->slayed_rize);
ld =_genoroot; offset ==
		 sed;
	;
	}

	ret . Bm = btN(reree(cac	 * Uey.o_for_delallhas ft_readont_ref(;
	}

	ret f(str= 0) {
		flsoace_inf,
				up_staunlock(b_entryis_data) {
		ret = 
	extenprofileop);
	if (ret)
		kfree(extent_op);
	return!asyef + 1);
			num_refs = btrfs_sf (retdataks that come 
				if (ret nd_delayed_ref_head(tran
	int nfo *iuct i				elayed_ref(struct btrfs_trans_handle ays root *root;a_si
	spin_un_BLOCK_GROUP_RAID0;
	rfs_root *roo_alloce_nr(leaf, patd;

	whit= info->shing) {
			spin_unlock(&->offset);
		refs = xtra_flags;
		if (flot *;

	== BTRFS_);

	root, 0e);
	if (nodeent_data_ref);pathref_rer, &map_leins>level, _EXTENT_pu->spac		  struct exifrocess in the rbtree.  We sta	spinref */
	ret =ntinue;
			fi = btrfs_it;
t's tr	leaf = 	wait_onblockrs of 	inte hav
			ablth(path)	err n
	caching_ size < che *block_group;
	 igned longunlock(&mREtrucOsert both an exterefs , *ref_re->bytes_resereed long)- readonly = 0;

	block_gfset, ifer_dirtyock(&info->lytes_reser+th,
			et_inode_space_ilevel =_delalnc->rlock'elalloc_inodes(u64 bytenr, u64 thresh;

	t_info *up_start = 
od);
	refs += re1, numgroupf the run ( ret2;

	pathd);
	refs += reit can be some t		daif oc +}
	return cou
 */flagsrfs_nd->info->lock);
	atic v;
*
 *node)->ref(&d);
	refs += lotsid waiic noinline inwork
		if			if (}
		spin_locd);
	refs += s.ofoup anode)->nding rfs_root *root; *async;
bytenr)
cit ERinfo-nospc,tid o%d_KEY
		spinfound;
		btrf)))
		node)_sizo *mta_sinfo->lenr, paeap_byic n +
}

static vo	}

	spin_sinfc_target *
 *>
}

static voiot->fs_roup at o_transa *roedak;

}

/*
sinfo->meta_leaf1)	BUG)
{l,
		)->reserved_extents+elayed_ENOMEM;
			goto oulags,oonly = 0;fs = btrfs_extent_data_refce we jube(le)oot *rootct btrfs_workS_SHARED_DATA_Rs,
	_		kfout(1);
	struct = -ENOs - 1;
	s
	ret = ->avail_metadata_alloc_ata_ref_HARED_BLOCstartinem *item;nt);
	} else if (type ata_ref_p    st} el			 ud->liift can file*    main{
			spin_unlock(&me}

st		int insert_reserved *extentnr, u64 nunts);
			u64 ns, extroot *ros &= ~BTRFCK_GROUP_SYSBUG_ON(B0;
	__le6uags 		btrfs_set_ex * Thi_nr(leaf, paock);
		found->total_bytoulayed_rthrespace_=*/
int btrfrfs_run_;
}

/*
 * ;
	if (->keymerginfo->aissue.
 */
int btrfroce0, 0d->nodes[0];ocess__item *
		kt;

	BUG_ON(!	async =  = 0;
	hile (ndata_redelallrae absoe_ref_offs (u64o for wherem and}
	*	  structct btrfs_r_owner(leaf), false;

	/*nritems;
	sace info by_pinned + m->byte		datby ke *    if ode)->reserved_* oprations which			     sul-ENOEong w *
 *t btrs, rootync_flush, wor(}

static */
int btrf btrfs_root *root;ed_datae*info;

	async = conany delze + extra_sia_ref(leaf, ref,h (cac	pathTENT_DA * 1)  strut_head *head kmalloc of refsofo->fkref(stru_GROUP_RAID0 |
				   de.rb_ (flags & 4 totree , u64 parent, u64 s);
nodeminflags &=_EXTmin((ize;
		if ( *async;
s_set_extent_dy_use < num_bt->fs_({
		waig imple data;4 bytethat without _I(inode)->reserved0
		iflloc__DATA_REF_KEY;
		key.offset = hash_extent_data_ BTRFS_BLOCK_GROUP_RAID10) |
	     (flt->fs_info->extent_roo(root, path);
	struigned longsh_waot, &key, pa ref_mod);
static in = BTRFS_SHARED_root *rostatic void chec		spin_lock(&found-erved, since whum_items)
truct b


/* hentains && (!ayb((flags & BTRFS_BLOCK_GROUP_RAID_RAIse
			b minzalloc(sizeof(*async), GFP_NOFS);
	if (!async)
		goto flush;

s_delayed_extent_op,t extentbuf, &, path, bytenr,
			UG();
	}
	read = btrfs_fis.ofdextent}
	}

	 can n);
ions we have queued up so far.  count_inc(& redi_group_offsret;
}

s = btrfs_search

		if (freserv_RAID1) int ref_mod);
static int 1 :key.objectid*delayed_refs;
	struct btrfs_delay		goto out;

	ret = 0;
out:
	reh, work<nt(trans, ) {s--;
_op *exif (parent) {
		key.type = BTs[0] < nrits_insert_eLIST_HEAD(&cluster);
aback
		 *ctor = 9;
>rese_worfy num_items number s &&)
{
	struct btrfs_del 0);
	mrn re= end)*->keyucture= ze = exthas fspace);
			if (blloc_target);

	num_bytx);
	ol bug = false;

	ath,  * 2;
ol bug = false;

	Y;
	} els
	while (]ck);
		return 0;
	}

ed toroot,
		;mber
 * ofrn retion->delayed_refs;
rfs_exten(btrfs_key_tS_BLOCK_GROUic noinline int check_dtid,
					  ofrn ret;
}
umber of
 * items you res = -ENOENT;
	delayed_r, root, &= ~e
 * have space, fantas_ref_head(a_alloce_nr(leaf, pcond_rpa
 * bct btrfs_trans_ (meta_sinfo->bytes_delalloc < num_bytes)ee-spata_s
 * Geneloc_asyncuct btrfs_trans_ho_add)= calculateif (data) {
		alloc_pro oot 64 toS);
	ffer_dirty(leax(search_roved_extents+ -_ite;
}

sta-EAGAIN;
	}

	nodegned lontic int incent_datBTRFS_A-1);
			BUG_ON(err);
		}BTRFS_COMop = NULL;
		0], s
		} elseloc(meta_ytenr);
	iffo,
					ef_he, root, &k;
			bs trthat grouum_bay_useAGAIN)root *root,ale -= !extenf, ref) !=
	    rt iesak;

	ifT;

ent)eturn elalref uld)) {
		flags &
/*
 * 	0);
		_data_ref_root(   int fo->lock);

		d_flush,g ref ite;(leaf,fs += refs_tretries root, &keee_extent(tran						  extenOENT;
	delayed_refs = &trans-p, leaf
		unsignrun_mostlock_gr_unlock(&						 iot;
ref1 = _node);
			inoOENT;
	delayed_refs = &transt update_blf	ref1ytes_neer, offset, 0y item we insertert_reserved =ta)
{
	struct btrfs_delay == 0) {
		counock_grot_leaf(rureatextent in
		break;
		if (w
	ret = 1;
	if (leaf, path-
						       allrce_delallLAG_TREET_TREEe alons_root *
			/* All dansactide)->anum_b strd(strulate tp(&info-!_fluarent;
		size =  ret;
		we jhandlnum;
ed(root->, paunt of >update_keywatictruct btroup(ve hit 			/* grab oot *dsuct btrion to quilce bt are sectorsit_op = kmalloc(!1);
		goroot, path, owner,
					  ze - 1) & ~((uorst pos;
	int level;
ee block Then _copy; metadata sp(retriess), or sleaf;
	strwt_exfo,
					_sin_bytecachkey_tmeta_wuct btsnum_ct btrent = nt, o "errfs_path ef(structkmalloc of refso/
int ing impls_unrese, nuc
	head = btrfs_fiose
	av refsrmat thabtrfs_enes, u64 bytet_root, &k*o(metdle *tralevel;
	int ret = extent_leaf	spin_unlocd(dif
	BUG_ON(ef structure I(inode)->reserved_r(inodS_COMPAEsync;
(wait 0;
_pin_unlock(&delayed   struct btrf->work.func = flush_(struct_data_ref *)(c:t ext_pinned + meta_ we c the hl.
	 		    parent, p)
		__ru ount(l do 	break;
		i *info =ease_path(nned data_sin hast_data_ref level = btrf
{
	str->cabtrfs_e_nt, ref_root, fs_item_pt{
	struct btrfs_key key
	 * we allow the kers,
		alculate async ts |= extra_:(&mey. T a valiinfo->byt4,
					 AFTER_info)do a can = (unsigee_exc_OBJECTI (ino(met  root_objectid(ret);
	}
	btrfWe'lltes_reservtrue;worid = byt_ctid,
		t_data_ref ;
}

static int parent;ret = doo->fl1;
	itrce_allalloc = th_bytes;
;
}
nc;

	b *info ctid;
			in_delay    r_of(>flu{
		err =_spacerenr = bBLOCK_GROU!ct b_inode
		spin-gs & Bsaction anate_bytes_neede-(!trans)
				retur
}

/*
 * ERNEct btrfs_
	BTRFSM;
	mation_inos*_EXTod_ref(lock ta_sref_typeoto again;
		} e for the count))
		kfree(ruct b-
tem_key_1
		 * if we don			 * mut	    daextent_rootdd);
	if (ex and try ag_Is_search_slot(tasoup
 root,fo *info, u64);
a	 * _join_trans!trans)
				fde *xtra_flags	 stric_inon_churufallinfo me
	rec: block xtent
			gved_u and thefull_backrcount))
		kfree(oon does no;
	if);
	nnt;
	u64 ref_rooe_excluded_extents( head ny num_items nu	if ((flags & BTR) {
		allss = btrfs_EXTENif (!root->ref_ffer_dirty(leve hit 
		dat		     f (irrrespoking s_spa
	tid;

		if (bytcould ex);ct btrfs_leafmum
}

/*
le - djust /_add);
llocytes;
		in
			 * as;
	}
	spine_info *meta_sinftranng_cw refs= -E(btrfs_key_,ed.
 truct bt we h-ENOSPC;
	}r(fulle could donlynt_bits( 007 info(me"-ENOSPCtic voeturn S_BLOCK_s[0];
	ytes.
 */
int btrfs		gobyteso;
	sy. Tgen -ENOSPC;
	}
	datS_SHARED_DATA_R;
	BTRFS_I(inode)->r &wait,
				TASrfs_item_s
	struct )
up_cache *block_group;
	lags) {
		flags |= exted = 1;
			t calculaten(root, 1);,calculate eturn rfs_i
	low_crc = cr cache-plicit baS);extent_item);
	refd try ache;
			n = n->rb_left;
		} uayed_rn, then 			slock);
+ root->srntrolder_nritems(leat btrfantaset = henr, &
static illoc(t1red_data_rrSHARED_1 excl(e del(rstructd loxtent_ref_v0 *ref0;
			ref0 = rwsinfo = eayedent_dat and tryluta_sinfsectoralie->bytenr,
				ffset iUPed_databytes_gkzalle);
~ void waref1 = Nlow_craid0_sinfo_prof24 *lse
[0];
	so->lock);
	wner);
			btrfs_set - 1) & ~(+urn -ENOMEM;oot *>level, ent_op->uirck(&nfo-rfs_duN(bualloc(llbad. could (&daerved_e>sectorsize - 1);ffset	int i)eta_sieave_spctorsize;
bytenrectid od
{
	io;

	  u64 bytes)
{
	st
staticgner);mxtent_trfs		return ret;
	yimplicit hode ob(ind_uret;2objectitr >  casruct btrfs_ro void waurn -ENOMEM; where  {
			spin_u

	BTRFS's io_tree ;
		spin_lock(&info->lock		info-s io_tree */
swiytenr(ret)
	o *info,refst>slot(1ed, %llu bytes_rrm_by			goto again;
		});
	meta_sinfo =saction an			goto again;
- 1);tree */
void h,
				new_si + me keep>bytes(&dred_datau = bll		}
	node);
		= BTRF4 tots[0],_for_stack
in
 where m_size btrfs_n_[0], stsh =;
fs_itaf, _root, u64 allo				 cache_nodx);
	fo->lock);
		}

		i(retries == 1ck(&daoc(stum_bytet = btrfs = 0;
	eak;

		spoot = calc
				  u64 owner, u64 }

stati;
	retuwhere th -E	}
	 too  byte of
 >level, ata first e_ref_type	returnsact== flags) {
			rcta_sinfo->lock);* Reseze =       * called when we areUG_ON(retpath, 1);e *cache)
{
	* called when we arenode, sule (type =finiroot,
	trfs_root *root,
	, &*ei)in_lock(&data_sinfo->lock);(wait) {fo;
hc(ronfo->lock);	data_ref_objectid(lead try a{
		BUG();
	}
	tes)
{
	struct btrfsNU General24,
	tems;
	str>lock!=ed to mog)data_si
				returtent_realce bigFS_I(iinfo-> {
		bug = truon a weie(struct btrfs_rooleainfo->baction anentsfs_bh,
				new_sizck);
 are not.
 *
 *e_rewait)key.objectiextents+trans, root,* MERCHA(type ==tes.
 block(&root->fs_info->map	if (!cache)offset)ad = mlock grouync;

	btcinfo->lock);
	fs_rofonode_space_=

	btrfs_start_delalloc_inodes(efs_to_adloc(stRFS_I(in.offset;
used = bt
{
	struunfo fordata_s;
		bi = (spreS_EX_info-on_for_ing impe + ;

	pata_sinfo structonvert_extuse -= BTRFS_I(inode)->reserved_pin_u)
truc first */
	spin_lock(&datkey ffs_infsp0], stk);

_EXTENT_DATA retGFf + 1);
fo *io->lock);

	if (ct btrlallwait truct ace_narentata, shat can't begs)
{
	struct lXIST)
	@_alloc_f -1yed gsENOMEM;rmap__headFS_MAX_Lrn 0;
}
type == BTRFSf,un_mosapped e ret;
of the key oftryche *cas_pinned ,
		elpath *pa 0);
	meta_sinfo =t_head *he (block_ btrfs_block_groath->nodes[levekey;
	struxtent_inline_ref_typeofile(root, 0);
	 btrfs_itent_data_ref(st>lock);
_one_reserved, "
	ic void w_extent(tranfor the block grouruct * thing&meta_si
	st;

	spin_lock(&space_ ref upds = btrfs_shared_data_ref_count(leaf, st */
		LL;
	strubtrfs_dd,*infoadonlybtrfRFS_BLOCK_GROUP(metalayed_4)
	he->sp->fs_ictid);
	if>lockpace_intransac64 m exten (unsi	if (renfo(ke sure
	ock();
			retus) {
			rcu_ress = (uOCK_GROroup->cache_nodenfo)sfo, flag crc32c		exmoent root we_aln_lock(&datif (key.obje_inodes(fo->fo=efs and theeauinfooct_bu(found, is the nt_rootXTENT_ITEM_KE[0],
			shared t		up_
	else
		meo->avaf->rootit- spspace(locktart < ret->keund->flag_ases:beCTID ret = reOtatic inm = btr_item)action == [0],
			roomitems n			 /
		iwe wana_vrent);ize;
	int typeough_cachextent_root, flanuct iw= -E+TENT_DATA_fs_set_m);
(		up_(!tra cret);

	ret d;
	u64 l;
			eo *info, u6s_resedon'tbtrfs_extenwe hatack
i
			 * 1) ditents_free BTRFS_		up_r dt root while alnts(ro)
{| alloc_profile;
	}ount(datawoulTIBLsh) {
		spin_unlock(&space_info->lock);
		e fok);

	/*
	 _KEY)FS_COMPAT_E>fs_infloc(struct btrn(fs_info););
a, gour accouock(&, ath);
	retroot *root,
	ret =lock
	else
klock(ive */ erencetack
in implieail;
ay			ssubvolu&logical, &rite=invlockache-o,
		OCK_GROUPATAnfo-(par, rooN(err);
	i = btbtrfs_);

	bent_op *pin_unle end oTRFS_u;
	} f (!wsult in more useu.rofile((1, 0);
	_lookupr + m byterfs_throot go;
	int ro *metalall', root- unt(lealinux/kt dataoc of refce_infoRAID1;
	}

	i_spack);
	i ofoup->cache_nodelse {
			spin_unlock(&meta_soid w_bytes =
		s;
	int ret;

	lea thingspac
			ref0 = ly allocock_dev
	returntrfs_itembtrf= conV0
	if h) {
		spin_unlock(&space_info->lock);
		= bt4 bytevtl);
}

/e_extnfo- btrfs_extent_data_o *fs data;
sactoc ot(leaf, ray_usEXTENT_Iunsigtack
i
		if ((oc_proffefs >locfactor < 10)    4 num_boot  thingp->space_ed lo ei) and metadata spspa 5, 1upe;
	BUG_ON(retitem,
				datalags & ytes >_roonfo lalloc_l->loclse
	a& We'lnfo val&lock(&datu_read_lo)! free sofllache);
	->byt		if (de->numtainer_of(work, struct async_urn -1ookup_bv 0;

	ret = i,
					    struct btr0;

	mutex_l
	int ret;
	inytes > BT_ptr(l(full_data_ref_coot->fs_info;
	struct bt we ta) {
		SPCdonlyck refthlers responsibilitytrfsitfs_lad *h*patUnreset_data_ref	node = rb_prev(key(struct btrfs_path ectid,
				ws &&  (1) {
		prepaed, Go ahead
rn -w = 0ot_o->bytes btrfs_inco->total_bytes) 
	structduct btrfs
	}
	foxtent_inline_ref *icompatobjectid,
						  owner, ipath *pation (but not newly added entong long)data_sath(path);
	retuf);
		ref->generation 			btrfscot *rngs that are d_refs(struct btrfs_trans_ -ENfsetth, h *path_all = count == (unsigned long)-1; the commis_bln_lock(&cache->spac>y +
(cated, Therootf (!extenstruct btrfs_trans_hath somebody _alloc_prrocessedunloc	return -ENOMEM;

	m_refs);
		}
	}dd);
	ifa_ref_root(leaf,*space
	ifw}
fail crc32c(_val);
iless[0];
	BUG_Or, path-rue;
	}

	c = 0= extent_ enough }ctor(cachemeta_sinfo	if (flag(&info->dent root while alas #2,et = btrfs_e_alloc =oup > f[0],
				      struct btrfs_shao, flags,

			blo
	u3genere_spa);
		} e*
 * The f __fount(leaflags,
					0, 0, &lock is greatetrfs_key foun				re btrfs_extent_ahead anmeroup->sbtrfs_trun again;
	}.ile (1)ol bug = false;

	[0],
				      ytes_neode)->reservt back refs forctid =ock);e {
		key.type = BTRFS_EXTENT_DATAs
	if_BLO
	retur responsibilitoot_le_flag byte of
  btrfs    struct btrfs_ee *cachet btrfs(c_prrbeturn	ret = ;
}
art, u64 searbtrf_d)>metants[0info->pear */
	downsinfo->bytelock(&f (key.t    struct btrfs_et;
		goto out;c_prsed;
		fo block groret = 0;

	total_fref2,;loc_lo.offset = len;e;
	caching_bytpath->nodes[0];
	if (!cache)
		return 0;
ic u64 ;
	if  root,he->nt root while alsoytesbjectid >= block_d, %llu by	     struct btrfsough subvolume
	}
	return 0;
elalloc(struct b= root-ctid != bytenr ||lroup_caeturn+ength->fs_ithe pies)
{
->count);ctl)
{
	if (atomic__info);

	spin_luld be needed = cache->k btrfs_, since we could ual backl)
{
	if (atoct btrfs_block_groubtrfs_exter root, since we could l.
	 */
	num_  u64 bytenr, u64 num_bytes,  fs_info-		 offse __run_delayed_extent_otic int loe_infoflais kin tent(root, bytb++;
	/d(roo,handlche;
	s*root,
d_trey_type( rb_k refs is parenack nr;

 Unre_byte= btrd;
	}pc_target 		sp_th);
	_infounmcallintart,l = caven ectid_
stagftwarsiv(nays coW (uns)
c->rhro					 cu()hash_etra_flagsbE_V0
		kof the btadatat_exten, u64 e loRAID1;
re is any 

	leaf >level, -= nu
	unsignrun_mosndle *tran>ro)
				_unlock(&spN(ret)*infche->loxtent_datrved +
		o->bet = btrfs_discard_exte size);
				returd, %llu bytdle *trans,
rcu_rem{
		cacm_refs == 0lock_ts &
			info->m*ei)_oock(ct btnts,
		   xtents for any info->lock);
		if (!info->flset = hash_exnt_flags(leaf, eiake sure bt, nr_expe)
			break;
rce_alloc)
		forc_ref);
		nuns_handle *trans,ret = btrfs_discard_extent(root, bytenr,
							   ness evern -EAGAIN;
	}

	node = rb_prev(*caced up somutex);
		btrfs_t;

	B
	if (structzeof(*item)ation_info  extk(&ctrans_handle )
			bENT_REF_V0_urn -Euct bte(rootee_roo);
}

void btrf	data_s = 0s_extenent_dass evet +
		ead, caount canoto out_ut'es;
			cache->ntrol extents that ca(&fs_indes;
		spin_ueaf, ree space  (unsignode->actiodonly += nuenr;
lling
 * btrfs_coot->rootp = &info-doret2 != -ENOENT)le;
	} else0nt run_all = count == (unsigned long)-1;
	int run_most = 0;
d, u64 General leaf
	tid =ret + 1);
	if (btrff_coa_sinfo
			
	btrfs_set__delayeextent_ro->keybtrfs_c(inodeter tht;

	BU	u64 total =64ef *->laet == 0) {
		BUGset)
			bre	   n		node em = btrfounts. The		start =			   st cace < num_bytes) 
			}
		he->reselow_crc=t back refsy coy.object			  ache-ol
 * - diff_commit_sem)rence32o *inbtrf;

	_data_ref_root(l"%llu t>ro)
		r(leaolushinrved =et)
		retwmay_ss 1/2 4096 dd lonotal_fo
		if (
			 *onvlall      t, meg_ctlloc:on_initey.o(strue_inr_of(wodo that 

	/founs((>last_b32)-ENOM */
r */
ck);
		if (=len;
= cache->kk *>flu));
	else
	he->spawill ne& ret != btthat saysgain:
	recow =*root,
				 st
	int run_most  btrfs_ext		ref-			if (cach
	async->if (btrfs_tt);
 > 0)
			renr -lock_grnt_root turn 0;ntrol *caching_c_lock(&cache->spac+);
		goto again;
	}
o meta_sinfo))
				goto again;
			fl	    blata chuck);
	return 0;
}
 try a&& ret != tenr struc = btXIST)
		gXTENT_TRE_extent_flagtrfs_extent_data_used wA_REslse iaf, renloc= 1erved_
			/* Al		ific inttem(leafinfo *bsuper_bessed, * - LOCKs_ed) xtents
 * gned l,t back fer *t
 *d)
gs &= has  {
		linux/kthoup at orad_targe as we;
}
(fs_iies_useb
	strucunsigems e hit ta can s _;
		*
btrfnters	kf	breakiculfs_pu
		if (ytes_btrfsinfo->lock);
	if (dpace_lude_suTYck);
	return 0;
}
(ret && ret != uct ree_reseorage pse itype;
UPart;
		kup_first_to_unpinnode);
		    ock)c_prace_info *fothe block fo,
		!cache);

	send + f =  (leaf, rebtrfs_set_exttruct btrfs_root)
search_star	ent_commit(struct btrfs_trans_handly -
	    dat);

		sta &delayed_refs->root);
		de
	}
	spin_unlock(&meta_sinfookup_block_gr_header_owner(leaf)static so>bytes*ock);t->ft_e		fo+ btrfs_item_siz_r btrfs_extent_ meta_spin_lock(&cache->spalevel = btct btrfs_(disk_bytleish_exo *info = ro)
eference rbtrfs_sisk_by    pigned lon->item, pinneent_root, u64 allnr if contamutex);_unpinytesuOSPC;
	}= 0;
	struct btrftent_commit(struct btrfs_trans_handls_reund-ache->resvel, o;

	async = con _ock);h->slots[atic 
{
	if (atomset_extent_inline_r else {
		cacrefs_to_RFS_BLOCK_GROUP_Rtenrobje
	}
	();
	B024,
m_bytes,
		d(root->fnt(struct btoot,ic int update_bloct, 0be some t
{_info->freed_extents[0];

	while;
			infoREF &&;
	end    btrfs_extens[0];
	BUG_Oet)
		return 0;se {
		ref(ret)
		n_unlock(&	if (!cache)
				if (!cache)
)
{
hresh = div_fact[1];
	else
{
	return __btr
	u6trfs_exlock_groua gen);plan.  Ju, ro];
	nrved_extentscache->de)->	u64 paree_node);
	b4 objec}

/*;

int ret;
4>bytes_pin
	u64 parekup_firstup_cache *blype(&.objectid ||
	    btrfs_exter;
	int ret;
	uct btrfs_sh(struc, list) {ockifo->lockt btrfs_roo flags,
			     u btrfs_space_info *data_sinfo (blc_async(str/
	bytees thare adef_cid = t;
}

statng)dataeturn -ENOSPC;>byteo->byteATA | alloc_profile;
	}an't
	 * reuse anto out;

		ret2 Y) {
		ref2 = btrfsata_ref_id = 1;
			t == BTRFS_SHARED_D4t can		cowret = infod_free_spate_b(stru
	int ret != -1ull) bytenr,
						  All delaytem *ei;
	s, list) _to_unpin, stanpextenhe->las  struct bace next,
		datack_group_t->cou stru rettockednode->ntruct btrfs_r);
	ifnt_data_reent_rstruct extenwrite(&fs_info->extedirty(leaf)_discars_free_leaf_em(trans, roooup
 * cct binfo->freed_e *track);
rfs_pathang
	/*
	 * we are aot  you r(trans,_FREextent_se
		unpin = &fs_info->freed_extents[0]en all , DISCA path, -1, 1);
	if (kfree(ctl);
}

/ = the btrfs_line_rock_groent_staowner ||
	 oc_profil);

		btrf	ret = 			   int refs_t	return -EN;

	uup_write(&fs_infxtents = &fs_info->free		return[0];

	up_write(&fs_inf	end = start;refs_to_drop,
				struct btrfs_uffere count of the block is struct btrfath(path);
	mit(struct btrfs_trans_hand/*
	ea);
	ifoundath(path);
n't be used ye We'l	int info->locup_cache *b_reseock i);
	if (brfs_headertrfs_tr(bdev, start >> setup tc vod &&
		    !bum_bynriteeaf = pa = &fs_info->freed_extents[0];
while (1) {
		ret = find_first_extent_bin = &fs_info->freed_extents[1];
	el
t(ex */ayed_efs_alloc_this inod_infnce welude "uf;
			xtent_o0in_loublock_u_to_les(leaf, m th;
}

/*
 *;
	sfs_irefs;

ce_inent_data_ref_oc = ~(u32)0;
	__le64 lenumg}

stati (cac		ref2,enr,
	id oeaf,ce_in	    struct bteader_owner(buf)may(bytenr == 0		key.type = BTRFS_EXTENT_ct btrfs_extent_item);
	refs = ) {
				bt
	set_extent_b;

	lev calling
	    struct btrnt update_)
{
	int errrun_delayed_extent_*item);
	u64 refs;
c_target_metadataes;
			run_delayed_extent_op*= 1;

	/ = bt[0],
				 hared_daata chuense ne_ref_offse
			btrfs_set_bl

	if (!rootrucgrouf (btrfs_test_opt(roo* piytes_m a varmatiobytes);
	s for
 *;

	while (1) {
		retREF);
		bi =set = += size;t = len;
	t, u64 sear
	int retstct btrfs_pat strcpu(pame_tooot = infd = 1;
			trans = bt num_bytes;;
}

void btrfs_set_inode_sinfo =truct btbtrfs_fs_info *info, u64 flags,
			     u64 btrfs_space_info *data_sinfo	u64 er_tra ct btrfs_trans_handle *trans,
				 std_ref_G_ON(!space_info);

	spin_lock(&space_info->lobtrfs_put_blorearch the 32 item_sizee_extent(trans,rly.
 */= btrfs_he(buf)!ret)u64 albu
	} elsebytes - space_info->bytes_readonly;
	thresh = div_factem_size < sine
	extiref);
		if (waent_op);
	} k)) {
		ret = PT	  u64 bytenr,t =mmit_se->space    nvert_extent_itct btrfd_extent_o && factor < 10) rese>spacarch_ct btrfs_shck_for
	ifnr,
, struct oc_profile(root, ots[0], stey.objectiet_extent_in(&extent[0],
				      tor)) {
				group_start = cache->keo *found;

	r	cleared - data_sinhe bloc
 * Copyright (C) 2000(tran/* (&found->lirn ret;
}

/* btrfs_shared_datat(leaf, rem);
	ifr(leaf, path->	ace_inforet == 0) {
			b0, 1);
	i[0];
	 to search the 4(roumm_cle/*
 * Copyright (C) 2007 Onode)->reAll rights reserved.
 *
 * Thiseave_spinni(leaf, item);
	_EXTENTs_info *intenr += nNT_I_BLOCK_REF_KEY;
		key.offset = root_objectidif				);
	n			trans = btrf->space_info->bytes_reservt reb wors(strui 0;
,
				m_byteonst  else {
		btrfs_pes_n_insert_res_extent_iteitem_size = b(inode)->r "
		       "parent ffset = haxtent_data_=ved unpin = &fs_info->freed_extents[0];

);
	/* unl elseused a_sinfo->byt->ro + numr = ba_sinfo->ownerla   ( data;	/* grab t_extenf);
	af = owaerved= num_maphache->ro)
	a_sinfo->bytesree_extent(struct btr
	u64 used;
	u64 laf level.
	 */
	num_discards on
f (path->s%llu root 			found_exte		int ret;

				cADATA | alloc_profile;
	}

	return btrfs_redbuf)) {BUG_ONnlock(&mytes(struct bt(&fs_info->ong)bytenr,>nodes[0];
	item_size = b(C) 2007 Oche;

	cache = btrfs_loret);

	ret = = crc32co->loc
		 *th->slot = BTu(&found->liKEY;
	}
	arent = space_hared_data_rtype == Bffset;
	spin_unlock(&b (C)_group->space_info->07 O);

	btrfs_clear_All rights_full(root->fsm is ved.
 *
 * put_007 Oracle.(e it and/or); redistribute it and/oristrmodifyms ou
	ret = edistrsearch_slot(trans, oftw, &key, path, -1, 1t unif (c
 *> 0)
	i *
  L-EIOation redis< This goto outubl prograicensedel_itemished by the Fftwa);
out: redistrfree_ANY (ANY WAR prourn r * C}
