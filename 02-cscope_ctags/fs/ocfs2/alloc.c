/* -*- mode: c; c-basic-offset: 8; -*-
 * vim: noexpandtab sw=8 ts=8 sts=0:
 *
 * alloc.c
 *
 * Extent allocs and frees
 *
 * Copyright (C) 2002, 2004 Oracle.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
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

#include <linux/fs.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/highmem.h>
#include <linux/swap.h>
#include <linux/quotaops.h>

#define MLOG_MASK_PREFIX ML_DISK_ALLOC
#include <cluster/masklog.h>

#include "ocfs2.h"

#include "alloc.h"
#include "aops.h"
#include "blockcheck.h"
#include "dlmglue.h"
#include "extent_map.h"
#include "inode.h"
#include "journal.h"
#include "localalloc.h"
#include "suballoc.h"
#include "sysfile.h"
#include "file.h"
#include "super.h"
#include "uptodate.h"
#include "xattr.h"
#include "refcounttree.h"

#include "buffer_head_io.h"

enum ocfs2_contig_type {
	CONTIG_NONE = 0,
	CONTIG_LEFT,
	CONTIG_RIGHT,
	CONTIG_LEFTRIGHT,
};

static enum ocfs2_contig_type
	ocfs2_extent_rec_contig(struct super_block *sb,
				struct ocfs2_extent_rec *ext,
				struct ocfs2_extent_rec *insert_rec);
/*
 * Operations for a specific extent tree type.
 *
 * To implement an on-disk btree (extent tree) type in ocfs2, add
 * an ocfs2_extent_tree_operations structure and the matching
 * ocfs2_init_<thingy>_extent_tree() function.  That's pretty much it
 * for the allocation portion of the extent tree.
 */
struct ocfs2_extent_tree_operations {
	/*
	 * last_eb_blk is the block number of the right most leaf extent
	 * block.  Most on-disk structures containing an extent tree store
	 * this value for fast access.  The ->eo_set_last_eb_blk() and
	 * ->eo_get_last_eb_blk() operations access this value.  They are
	 *  both required.
	 */
	void (*eo_set_last_eb_blk)(struct ocfs2_extent_tree *et,
				   u64 blkno);
	u64 (*eo_get_last_eb_blk)(struct ocfs2_extent_tree *et);

	/*
	 * The on-disk structure usually keeps track of how many total
	 * clusters are stored in this extent tree.  This function updates
	 * that value.  new_clusters is the delta, and must be
	 * added to the total.  Required.
	 */
	void (*eo_update_clusters)(struct ocfs2_extent_tree *et,
				   u32 new_clusters);

	/*
	 * If this extent tree is supported by an extent map, insert
	 * a record into the map.
	 */
	void (*eo_extent_map_insert)(struct ocfs2_extent_tree *et,
				     struct ocfs2_extent_rec *rec);

	/*
	 * If this extent tree is supported by an extent map, truncate the
	 * map to clusters,
	 */
	void (*eo_extent_map_truncate)(struct ocfs2_extent_tree *et,
				       u32 clusters);

	/*
	 * If ->eo_insert_check() exists, it is called before rec is
	 * inserted into the extent tree.  It is optional.
	 */
	int (*eo_insert_check)(struct ocfs2_extent_tree *et,
			       struct ocfs2_extent_rec *rec);
	int (*eo_sanity_check)(struct ocfs2_extent_tree *et);

	/*
	 * --------------------------------------------------------------
	 * The remaining are internal to ocfs2_extent_tree and don't have
	 * accessor functions
	 */

	/*
	 * ->eo_fill_root_el() takes et->et_object and sets et->et_root_el.
	 * It is required.
	 */
	void (*eo_fill_root_el)(struct ocfs2_extent_tree *et);

	/*
	 * ->eo_fill_max_leaf_clusters sets et->et_max_leaf_clusters if
	 * it exists.  If it does not, et->et_max_leaf_clusters is set
	 * to 0 (unlimited).  Optional.
	 */
	void (*eo_fill_max_leaf_clusters)(struct ocfs2_extent_tree *et);

	/*
	 * ->eo_extent_contig test whether the 2 ocfs2_extent_rec
	 * are contiguous or not. Optional. Don't need to set it if use
	 * ocfs2_extent_rec as the tree leaf.
	 */
	enum ocfs2_contig_type
		(*eo_extent_contig)(struct ocfs2_extent_tree *et,
				    struct ocfs2_extent_rec *ext,
				    struct ocfs2_extent_rec *insert_rec);
};


/*
 * Pre-declare ocfs2_dinode_et_ops so we can use it as a sanity check
 * in the methods.
 */
static u64 ocfs2_dinode_get_last_eb_blk(struct ocfs2_extent_tree *et);
static void ocfs2_dinode_set_last_eb_blk(struct ocfs2_extent_tree *et,
					 u64 blkno);
static void ocfs2_dinode_update_clusters(struct ocfs2_extent_tree *et,
					 u32 clusters);
static void ocfs2_dinode_extent_map_insert(struct ocfs2_extent_tree *et,
					   struct ocfs2_extent_rec *rec);
static void ocfs2_dinode_extent_map_truncate(struct ocfs2_extent_tree *et,
					     u32 clusters);
static int ocfs2_dinode_insert_check(struct ocfs2_extent_tree *et,
				     struct ocfs2_extent_rec *rec);
static int ocfs2_dinode_sanity_check(struct ocfs2_extent_tree *et);
static void ocfs2_dinode_fill_root_el(struct ocfs2_extent_tree *et);
static struct ocfs2_extent_tree_operations ocfs2_dinode_et_ops = {
	.eo_set_last_eb_blk	= ocfs2_dinode_set_last_eb_blk,
	.eo_get_last_eb_blk	= ocfs2_dinode_get_last_eb_blk,
	.eo_update_clusters	= ocfs2_dinode_update_clusters,
	.eo_extent_map_insert	= ocfs2_dinode_extent_map_insert,
	.eo_extent_map_truncate	= ocfs2_dinode_extent_map_truncate,
	.eo_insert_check	= ocfs2_dinode_insert_check,
	.eo_sanity_check	= ocfs2_dinode_sanity_check,
	.eo_fill_root_el	= ocfs2_dinode_fill_root_el,
};

static void ocfs2_dinode_set_last_eb_blk(struct ocfs2_extent_tree *et,
					 u64 blkno)
{
	struct ocfs2_dinode *di = et->et_object;

	BUG_ON(et->et_ops != &ocfs2_dinode_et_ops);
	di->i_last_eb_blk = cpu_to_le64(blkno);
}

static u64 ocfs2_dinode_get_last_eb_blk(struct ocfs2_extent_tree *et)
{
	struct ocfs2_dinode *di = et->et_object;

	BUG_ON(et->et_ops != &ocfs2_dinode_et_ops);
	return le64_to_cpu(di->i_last_eb_blk);
}

static void ocfs2_dinode_update_clusters(struct ocfs2_extent_tree *et,
					 u32 clusters)
{
	struct ocfs2_inode_info *oi = cache_info_to_inode(et->et_ci);
	struct ocfs2_dinode *di = et->et_object;

	le32_add_cpu(&di->i_clusters, clusters);
	spin_lock(&oi->ip_lock);
	oi->ip_clusters = le32_to_cpu(di->i_clusters);
	spin_unlock(&oi->ip_lock);
}

static void ocfs2_dinode_extent_map_insert(struct ocfs2_extent_tree *et,
					   struct ocfs2_extent_rec *rec)
{
	struct inode *inode = &cache_info_to_inode(et->et_ci)->vfs_inode;

	ocfs2_extent_map_insert_rec(inode, rec);
}

static void ocfs2_dinode_extent_map_truncate(struct ocfs2_extent_tree *et,
					     u32 clusters)
{
	struct inode *inode = &cache_info_to_inode(et->et_ci)->vfs_inode;

	ocfs2_extent_map_trunc(inode, clusters);
}

static int ocfs2_dinode_insert_check(struct ocfs2_extent_tree *et,
				     struct ocfs2_extent_rec *rec)
{
	struct ocfs2_inode_info *oi = cache_info_to_inode(et->et_ci);
	struct ocfs2_super *osb = OCFS2_SB(oi->vfs_inode.i_sb);

	BUG_ON(oi->ip_dyn_features & OCFS2_INLINE_DATA_FL);
	mlog_bug_on_msg(!ocfs2_sparse_alloc(osb) &&
			(oi->ip_clusters != le32_to_cpu(rec->e_cpos)),
			"Device %s, asking for sparse allocation: inode %llu, "
			"cpos %u, clusters %u\n",
			osb->dev_str,
			(unsigned long long)oi->ip_blkno,
			rec->e_cpos, oi->ip_clusters);

	return 0;
}

static int ocfs2_dinode_sanity_check(struct ocfs2_extent_tree *et)
{
	struct ocfs2_dinode *di = et->et_object;

	BUG_ON(et->et_ops != &ocfs2_dinode_et_ops);
	BUG_ON(!OCFS2_IS_VALID_DINODE(di));

	return 0;
}

static void ocfs2_dinode_fill_root_el(struct ocfs2_extent_tree *et)
{
	struct ocfs2_dinode *di = et->et_object;

	et->et_root_el = &di->id2.i_list;
}


static void ocfs2_xattr_value_fill_root_el(struct ocfs2_extent_tree *et)
{
	struct ocfs2_xattr_value_buf *vb = et->et_object;

	et->et_root_el = &vb->vb_xv->xr_list;
}

static void ocfs2_xattr_value_set_last_eb_blk(struct ocfs2_extent_tree *et,
					      u64 blkno)
{
	struct ocfs2_xattr_value_buf *vb = et->et_object;

	vb->vb_xv->xr_last_eb_blk = cpu_to_le64(blkno);
}

static u64 ocfs2_xattr_value_get_last_eb_blk(struct ocfs2_extent_tree *et)
{
	struct ocfs2_xattr_value_buf *vb = et->et_object;

	return le64_to_cpu(vb->vb_xv->xr_last_eb_blk);
}

static void ocfs2_xattr_value_update_clusters(struct ocfs2_extent_tree *et,
					      u32 clusters)
{
	struct ocfs2_xattr_value_buf *vb = et->et_object;

	le32_add_cpu(&vb->vb_xv->xr_clusters, clusters);
}

static struct ocfs2_extent_tree_operations ocfs2_xattr_value_et_ops = {
	.eo_set_last_eb_blk	= ocfs2_xattr_value_set_last_eb_blk,
	.eo_get_last_eb_blk	= ocfs2_xattr_value_get_last_eb_blk,
	.eo_update_clusters	= ocfs2_xattr_value_update_clusters,
	.eo_fill_root_el	= ocfs2_xattr_value_fill_root_el,
};

static void ocfs2_xattr_tree_fill_root_el(struct ocfs2_extent_tree *et)
{
	struct ocfs2_xattr_block *xb = et->et_object;

	et->et_root_el = &xb->xb_attrs.xb_root.xt_list;
}

static void ocfs2_xattr_tree_fill_max_leaf_clusters(struct ocfs2_extent_tree *et)
{
	struct super_block *sb = ocfs2_metadata_cache_get_super(et->et_ci);
	et->et_max_leaf_clusters =
		ocfs2_clusters_for_bytes(sb, OCFS2_MAX_XATTR_TREE_LEAF_SIZE);
}

static void ocfs2_xattr_tree_set_last_eb_blk(struct ocfs2_extent_tree *et,
					     u64 blkno)
{
	struct ocfs2_xattr_block *xb = et->et_object;
	struct ocfs2_xattr_tree_root *xt = &xb->xb_attrs.xb_root;

	xt->xt_last_eb_blk = cpu_to_le64(blkno);
}

static u64 ocfs2_xattr_tree_get_last_eb_blk(struct ocfs2_extent_tree *et)
{
	struct ocfs2_xattr_block *xb = et->et_object;
	struct ocfs2_xattr_tree_root *xt = &xb->xb_attrs.xb_root;

	return le64_to_cpu(xt->xt_last_eb_blk);
}

static void ocfs2_xattr_tree_update_clusters(struct ocfs2_extent_tree *et,
					     u32 clusters)
{
	struct ocfs2_xattr_block *xb = et->et_object;

	le32_add_cpu(&xb->xb_attrs.xb_root.xt_clusters, clusters);
}

static struct ocfs2_extent_tree_operations ocfs2_xattr_tree_et_ops = {
	.eo_set_last_eb_blk	= ocfs2_xattr_tree_set_last_eb_blk,
	.eo_get_last_eb_blk	= ocfs2_xattr_tree_get_last_eb_blk,
	.eo_update_clusters	= ocfs2_xattr_tree_update_clusters,
	.eo_fill_root_el	= ocfs2_xattr_tree_fill_root_el,
	.eo_fill_max_leaf_clusters = ocfs2_xattr_tree_fill_max_leaf_clusters,
};

static void ocfs2_dx_root_set_last_eb_blk(struct ocfs2_extent_tree *et,
					  u64 blkno)
{
	struct ocfs2_dx_root_block *dx_root = et->et_object;

	dx_root->dr_last_eb_blk = cpu_to_le64(blkno);
}

static u64 ocfs2_dx_root_get_last_eb_blk(struct ocfs2_extent_tree *et)
{
	struct ocfs2_dx_root_block *dx_root = et->et_object;

	return le64_to_cpu(dx_root->dr_last_eb_blk);
}

static void ocfs2_dx_root_update_clusters(struct ocfs2_extent_tree *et,
					  u32 clusters)
{
	struct ocfs2_dx_root_block *dx_root = et->et_object;

	le32_add_cpu(&dx_root->dr_clusters, clusters);
}

static int ocfs2_dx_root_sanity_check(struct ocfs2_extent_tree *et)
{
	struct ocfs2_dx_root_block *dx_root = et->et_object;

	BUG_ON(!OCFS2_IS_VALID_DX_ROOT(dx_root));

	return 0;
}

static void ocfs2_dx_root_fill_root_el(struct ocfs2_extent_tree *et)
{
	struct ocfs2_dx_root_block *dx_root = et->et_object;

	et->et_root_el = &dx_root->dr_list;
}

static struct ocfs2_extent_tree_operations ocfs2_dx_root_et_ops = {
	.eo_set_last_eb_blk	= ocfs2_dx_root_set_last_eb_blk,
	.eo_get_last_eb_blk	= ocfs2_dx_root_get_last_eb_blk,
	.eo_update_clusters	= ocfs2_dx_root_update_clusters,
	.eo_sanity_check	= ocfs2_dx_root_sanity_check,
	.eo_fill_root_el	= ocfs2_dx_root_fill_root_el,
};

static void ocfs2_refcount_tree_fill_root_el(struct ocfs2_extent_tree *et)
{
	struct ocfs2_refcount_block *rb = et->et_object;

	et->et_root_el = &rb->rf_list;
}

static void ocfs2_refcount_tree_set_last_eb_blk(struct ocfs2_extent_tree *et,
						u64 blkno)
{
	struct ocfs2_refcount_block *rb = et->et_object;

	rb->rf_last_eb_blk = cpu_to_le64(blkno);
}

static u64 ocfs2_refcount_tree_get_last_eb_blk(struct ocfs2_extent_tree *et)
{
	struct ocfs2_refcount_block *rb = et->et_object;

	return le64_to_cpu(rb->rf_last_eb_blk);
}

static void ocfs2_refcount_tree_update_clusters(struct ocfs2_extent_tree *et,
						u32 clusters)
{
	struct ocfs2_refcount_block *rb = et->et_object;

	le32_add_cpu(&rb->rf_clusters, clusters);
}

static enum ocfs2_contig_type
ocfs2_refcount_tree_extent_contig(struct ocfs2_extent_tree *et,
				  struct ocfs2_extent_rec *ext,
				  struct ocfs2_extent_rec *insert_rec)
{
	return CONTIG_NONE;
}

static struct ocfs2_extent_tree_operations ocfs2_refcount_tree_et_ops = {
	.eo_set_last_eb_blk	= ocfs2_refcount_tree_set_last_eb_blk,
	.eo_get_last_eb_blk	= ocfs2_refcount_tree_get_last_eb_blk,
	.eo_update_clusters	= ocfs2_refcount_tree_update_clusters,
	.eo_fill_root_el	= ocfs2_refcount_tree_fill_root_el,
	.eo_extent_contig	= ocfs2_refcount_tree_extent_contig,
};

static void __ocfs2_init_extent_tree(struct ocfs2_extent_tree *et,
				     struct ocfs2_caching_info *ci,
				     struct buffer_head *bh,
				     ocfs2_journal_access_func access,
				     void *obj,
				     struct ocfs2_extent_tree_operations *ops)
{
	et->et_ops = ops;
	et->et_root_bh = bh;
	et->et_ci = ci;
	et->et_root_journal_access = access;
	if (!obj)
		obj = (void *)bh->b_data;
	et->et_object = obj;

	et->et_ops->eo_fill_root_el(et);
	if (!et->et_ops->eo_fill_max_leaf_clusters)
		et->et_max_leaf_clusters = 0;
	else
		et->et_ops->eo_fill_max_leaf_clusters(et);
}

void ocfs2_init_dinode_extent_tree(struct ocfs2_extent_tree *et,
				   struct ocfs2_caching_info *ci,
				   struct buffer_head *bh)
{
	__ocfs2_init_extent_tree(et, ci, bh, ocfs2_journal_access_di,
				 NULL, &ocfs2_dinode_et_ops);
}

void ocfs2_init_xattr_tree_extent_tree(struct ocfs2_extent_tree *et,
				       struct ocfs2_caching_info *ci,
				       struct buffer_head *bh)
{
	__ocfs2_init_extent_tree(et, ci, bh, ocfs2_journal_access_xb,
				 NULL, &ocfs2_xattr_tree_et_ops);
}

void ocfs2_init_xattr_value_extent_tree(struct ocfs2_extent_tree *et,
					struct ocfs2_caching_info *ci,
					struct ocfs2_xattr_value_buf *vb)
{
	__ocfs2_init_extent_tree(et, ci, vb->vb_bh, vb->vb_access, vb,
				 &ocfs2_xattr_value_et_ops);
}

void ocfs2_init_dx_root_extent_tree(struct ocfs2_extent_tree *et,
				    struct ocfs2_caching_info *ci,
				    struct buffer_head *bh)
{
	__ocfs2_init_extent_tree(et, ci, bh, ocfs2_journal_access_dr,
				 NULL, &ocfs2_dx_root_et_ops);
}

void ocfs2_init_refcount_extent_tree(struct ocfs2_extent_tree *et,
				     struct ocfs2_caching_info *ci,
				     struct buffer_head *bh)
{
	__ocfs2_init_extent_tree(et, ci, bh, ocfs2_journal_access_rb,
				 NULL, &ocfs2_refcount_tree_et_ops);
}

static inline void ocfs2_et_set_last_eb_blk(struct ocfs2_extent_tree *et,
					    u64 new_last_eb_blk)
{
	et->et_ops->eo_set_last_eb_blk(et, new_last_eb_blk);
}

static inline u64 ocfs2_et_get_last_eb_blk(struct ocfs2_extent_tree *et)
{
	return et->et_ops->eo_get_last_eb_blk(et);
}

static inline void ocfs2_et_update_clusters(struct ocfs2_extent_tree *et,
					    u32 clusters)
{
	et->et_ops->eo_update_clusters(et, clusters);
}

static inline void ocfs2_et_extent_map_insert(struct ocfs2_extent_tree *et,
					      struct ocfs2_extent_rec *rec)
{
	if (et->et_ops->eo_extent_map_insert)
		et->et_ops->eo_extent_map_insert(et, rec);
}

static inline void ocfs2_et_extent_map_truncate(struct ocfs2_extent_tree *et,
						u32 clusters)
{
	if (et->et_ops->eo_extent_map_truncate)
		et->et_ops->eo_extent_map_truncate(et, clusters);
}

static inline int ocfs2_et_root_journal_access(handle_t *handle,
					       struct ocfs2_extent_tree *et,
					       int type)
{
	return et->et_root_journal_access(handle, et->et_ci, et->et_root_bh,
					  type);
}

static inline enum ocfs2_contig_type
	ocfs2_et_extent_contig(struct ocfs2_extent_tree *et,
			       struct ocfs2_extent_rec *rec,
			       struct ocfs2_extent_rec *insert_rec)
{
	if (et->et_ops->eo_extent_contig)
		return et->et_ops->eo_extent_contig(et, rec, insert_rec);

	return ocfs2_extent_rec_contig(
				ocfs2_metadata_cache_get_super(et->et_ci),
				rec, insert_rec);
}

static inline int ocfs2_et_insert_check(struct ocfs2_extent_tree *et,
					struct ocfs2_extent_rec *rec)
{
	int ret = 0;

	if (et->et_ops->eo_insert_check)
		ret = et->et_ops->eo_insert_check(et, rec);
	return ret;
}

static inline int ocfs2_et_sanity_check(struct ocfs2_extent_tree *et)
{
	int ret = 0;

	if (et->et_ops->eo_sanity_check)
		ret = et->et_ops->eo_sanity_check(et);
	return ret;
}

static void ocfs2_free_truncate_context(struct ocfs2_truncate_context *tc);
static int ocfs2_cache_extent_block_free(struct ocfs2_cached_dealloc_ctxt *ctxt,
					 struct ocfs2_extent_block *eb);
static void ocfs2_adjust_rightmost_records(handle_t *handle,
					   struct ocfs2_extent_tree *et,
					   struct ocfs2_path *path,
					   struct ocfs2_extent_rec *insert_rec);
/*
 * Reset the actual path elements so that we can re-use the structure
 * to build another path. Generally, this involves freeing the buffer
 * heads.
 */
void ocfs2_reinit_path(struct ocfs2_path *path, int keep_root)
{
	int i, start = 0, depth = 0;
	struct ocfs2_path_item *node;

	if (keep_root)
		start = 1;

	for(i = start; i < path_num_items(path); i++) {
		node = &path->p_node[i];

		brelse(node->bh);
		node->bh = NULL;
		node->el = NULL;
	}

	/*
	 * Tree depth may change during truncate, or insert. If we're
	 * keeping the root extent list, then make sure that our path
	 * structure reflects the proper depth.
	 */
	if (keep_root)
		depth = le16_to_cpu(path_root_el(path)->l_tree_depth);
	else
		path_root_access(path) = NULL;

	path->p_tree_depth = depth;
}

void ocfs2_free_path(struct ocfs2_path *path)
{
	if (path) {
		ocfs2_reinit_path(path, 0);
		kfree(path);
	}
}

/*
 * All the elements of src into dest. After this call, src could be freed
 * without affecting dest.
 *
 * Both paths should have the same root. Any non-root elements of dest
 * will be freed.
 */
static void ocfs2_cp_path(struct ocfs2_path *dest, struct ocfs2_path *src)
{
	int i;

	BUG_ON(path_root_bh(dest) != path_root_bh(src));
	BUG_ON(path_root_el(dest) != path_root_el(src));
	BUG_ON(path_root_access(dest) != path_root_access(src));

	ocfs2_reinit_path(dest, 1);

	for(i = 1; i < OCFS2_MAX_PATH_DEPTH; i++) {
		dest->p_node[i].bh = src->p_node[i].bh;
		dest->p_node[i].el = src->p_node[i].el;

		if (dest->p_node[i].bh)
			get_bh(dest->p_node[i].bh);
	}
}

/*
 * Make the *dest path the same as src and re-initialize src path to
 * have a root only.
 */
static void ocfs2_mv_path(struct ocfs2_path *dest, struct ocfs2_path *src)
{
	int i;

	BUG_ON(path_root_bh(dest) != path_root_bh(src));
	BUG_ON(path_root_access(dest) != path_root_access(src));

	for(i = 1; i < OCFS2_MAX_PATH_DEPTH; i++) {
		brelse(dest->p_node[i].bh);

		dest->p_node[i].bh = src->p_node[i].bh;
		dest->p_node[i].el = src->p_node[i].el;

		src->p_node[i].bh = NULL;
		src->p_node[i].el = NULL;
	}
}

/*
 * Insert an extent block at given index.
 *
 * This will not take an additional reference on eb_bh.
 */
static inline void ocfs2_path_insert_eb(struct ocfs2_path *path, int index,
					struct buffer_head *eb_bh)
{
	struct ocfs2_extent_block *eb = (struct ocfs2_extent_block *)eb_bh->b_data;

	/*
	 * Right now, no root bh is an extent block, so this helps
	 * catch code errors with dinode trees. The assertion can be
	 * safely removed if we ever need to insert extent block
	 * structures at the root.
	 */
	BUG_ON(index == 0);

	path->p_node[index].bh = eb_bh;
	path->p_node[index].el = &eb->h_list;
}

static struct ocfs2_path *ocfs2_new_path(struct buffer_head *root_bh,
					 struct ocfs2_extent_list *root_el,
					 ocfs2_journal_access_func access)
{
	struct ocfs2_path *path;

	BUG_ON(le16_to_cpu(root_el->l_tree_depth) >= OCFS2_MAX_PATH_DEPTH);

	path = kzalloc(sizeof(*path), GFP_NOFS);
	if (path) {
		path->p_tree_depth = le16_to_cpu(root_el->l_tree_depth);
		get_bh(root_bh);
		path_root_bh(path) = root_bh;
		path_root_el(path) = root_el;
		path_root_access(path) = access;
	}

	return path;
}

struct ocfs2_path *ocfs2_new_path_from_path(struct ocfs2_path *path)
{
	return ocfs2_new_path(path_root_bh(path), path_root_el(path),
			      path_root_access(path));
}

struct ocfs2_path *ocfs2_new_path_from_et(struct ocfs2_extent_tree *et)
{
	return ocfs2_new_path(et->et_root_bh, et->et_root_el,
			      et->et_root_journal_access);
}

/*
 * Journal the buffer at depth idx.  All idx>0 are extent_blocks,
 * otherwise it's the root_access function.
 *
 * I don't like the way this function's name looks next to
 * ocfs2_journal_access_path(), but I don't have a better one.
 */
int ocfs2_path_bh_journal_access(handle_t *handle,
				 struct ocfs2_caching_info *ci,
				 struct ocfs2_path *path,
				 int idx)
{
	ocfs2_journal_access_func access = path_root_access(path);

	if (!access)
		access = ocfs2_journal_access;

	if (idx)
		access = ocfs2_journal_access_eb;

	return access(handle, ci, path->p_node[idx].bh,
		      OCFS2_JOURNAL_ACCESS_WRITE);
}

/*
 * Convenience function to journal all components in a path.
 */
int ocfs2_journal_access_path(struct ocfs2_caching_info *ci,
			      handle_t *handle,
			      struct ocfs2_path *path)
{
	int i, ret = 0;

	if (!path)
		goto out;

	for(i = 0; i < path_num_items(path); i++) {
		ret = ocfs2_path_bh_journal_access(handle, ci, path, i);
		if (ret < 0) {
			mlog_errno(ret);
			goto out;
		}
	}

out:
	return ret;
}

/*
 * Return the index of the extent record which contains cluster #v_cluster.
 * -1 is returned if it was not found.
 *
 * Should work fine on interior and exterior nodes.
 */
int ocfs2_search_extent_list(struct ocfs2_extent_list *el, u32 v_cluster)
{
	int ret = -1;
	int i;
	struct ocfs2_extent_rec *rec;
	u32 rec_end, rec_start, clusters;

	for(i = 0; i < le16_to_cpu(el->l_next_free_rec); i++) {
		rec = &el->l_recs[i];

		rec_start = le32_to_cpu(rec->e_cpos);
		clusters = ocfs2_rec_clusters(el, rec);

		rec_end = rec_start + clusters;

		if (v_cluster >= rec_start && v_cluster < rec_end) {
			ret = i;
			break;
		}
	}

	return ret;
}

/*
 * NOTE: ocfs2_block_extent_contig(), ocfs2_extents_adjacent() and
 * ocfs2_extent_rec_contig only work properly against leaf nodes!
 */
static int ocfs2_block_extent_contig(struct super_block *sb,
				     struct ocfs2_extent_rec *ext,
				     u64 blkno)
{
	u64 blk_end = le64_to_cpu(ext->e_blkno);

	blk_end += ocfs2_clusters_to_blocks(sb,
				    le16_to_cpu(ext->e_leaf_clusters));

	return blkno == blk_end;
}

static int ocfs2_extents_adjacent(struct ocfs2_extent_rec *left,
				  struct ocfs2_extent_rec *right)
{
	u32 left_range;

	left_range = le32_to_cpu(left->e_cpos) +
		le16_to_cpu(left->e_leaf_clusters);

	return (left_range == le32_to_cpu(right->e_cpos));
}

static enum ocfs2_contig_type
	ocfs2_extent_rec_contig(struct super_block *sb,
				struct ocfs2_extent_rec *ext,
				struct ocfs2_extent_rec *insert_rec)
{
	u64 blkno = le64_to_cpu(insert_rec->e_blkno);

	/*
	 * Refuse to coalesce extent records with different flag
	 * fields - we don't want to mix unwritten extents with user
	 * data.
	 */
	if (ext->e_flags != insert_rec->e_flags)
		return CONTIG_NONE;

	if (ocfs2_extents_adjacent(ext, insert_rec) &&
	    ocfs2_block_extent_contig(sb, ext, blkno))
			return CONTIG_RIGHT;

	blkno = le64_to_cpu(ext->e_blkno);
	if (ocfs2_extents_adjacent(insert_rec, ext) &&
	    ocfs2_block_extent_contig(sb, insert_rec, blkno))
		return CONTIG_LEFT;

	return CONTIG_NONE;
}

/*
 * NOTE: We can have pretty much any combination of contiguousness and
 * appending.
 *
 * The usefulness of APPEND_TAIL is more in that it lets us know that
 * we'll have to update the path to that leaf.
 */
enum ocfs2_append_type {
	APPEND_NONE = 0,
	APPEND_TAIL,
};

enum ocfs2_split_type {
	SPLIT_NONE = 0,
	SPLIT_LEFT,
	SPLIT_RIGHT,
};

struct ocfs2_insert_type {
	enum ocfs2_split_type	ins_split;
	enum ocfs2_append_type	ins_appending;
	enum ocfs2_contig_type	ins_contig;
	int			ins_contig_index;
	int			ins_tree_depth;
};

struct ocfs2_merge_ctxt {
	enum ocfs2_contig_type	c_contig_type;
	int			c_has_empty_extent;
	int			c_split_covers_rec;
};

static int ocfs2_validate_extent_block(struct super_block *sb,
				       struct buffer_head *bh)
{
	int rc;
	struct ocfs2_extent_block *eb =
		(struct ocfs2_extent_block *)bh->b_data;

	mlog(0, "Validating extent block %llu\n",
	     (unsigned long long)bh->b_blocknr);

	BUG_ON(!buffer_uptodate(bh));

	/*
	 * If the ecc fails, we return the error but otherwise
	 * leave the filesystem running.  We know any error is
	 * local to this block.
	 */
	rc = ocfs2_validate_meta_ecc(sb, bh->b_data, &eb->h_check);
	if (rc) {
		mlog(ML_ERROR, "Checksum failed for extent block %llu\n",
		     (unsigned long long)bh->b_blocknr);
		return rc;
	}

	/*
	 * Errors after here are fatal.
	 */

	if (!OCFS2_IS_VALID_EXTENT_BLOCK(eb)) {
		ocfs2_error(sb,
			    "Extent block #%llu has bad signature %.*s",
			    (unsigned long long)bh->b_blocknr, 7,
			    eb->h_signature);
		return -EINVAL;
	}

	if (le64_to_cpu(eb->h_blkno) != bh->b_blocknr) {
		ocfs2_error(sb,
			    "Extent block #%llu has an invalid h_blkno "
			    "of %llu",
			    (unsigned long long)bh->b_blocknr,
			    (unsigned long long)le64_to_cpu(eb->h_blkno));
		return -EINVAL;
	}

	if (le32_to_cpu(eb->h_fs_generation) != OCFS2_SB(sb)->fs_generation) {
		ocfs2_error(sb,
			    "Extent block #%llu has an invalid "
			    "h_fs_generation of #%u",
			    (unsigned long long)bh->b_blocknr,
			    le32_to_cpu(eb->h_fs_generation));
		return -EINVAL;
	}

	return 0;
}

int ocfs2_read_extent_block(struct ocfs2_caching_info *ci, u64 eb_blkno,
			    struct buffer_head **bh)
{
	int rc;
	struct buffer_head *tmp = *bh;

	rc = ocfs2_read_block(ci, eb_blkno, &tmp,
			      ocfs2_validate_extent_block);

	/* If ocfs2_read_block() got us a new bh, pass it up. */
	if (!rc && !*bh)
		*bh = tmp;

	return rc;
}


/*
 * How many free extents have we got before we need more meta data?
 */
int ocfs2_num_free_extents(struct ocfs2_super *osb,
			   struct ocfs2_extent_tree *et)
{
	int retval;
	struct ocfs2_extent_list *el = NULL;
	struct ocfs2_extent_block *eb;
	struct buffer_head *eb_bh = NULL;
	u64 last_eb_blk = 0;

	mlog_entry_void();

	el = et->et_root_el;
	last_eb_blk = ocfs2_et_get_last_eb_blk(et);

	if (last_eb_blk) {
		retval = ocfs2_read_extent_block(et->et_ci, last_eb_blk,
						 &eb_bh);
		if (retval < 0) {
			mlog_errno(retval);
			goto bail;
		}
		eb = (struct ocfs2_extent_block *) eb_bh->b_data;
		el = &eb->h_list;
	}

	BUG_ON(el->l_tree_depth != 0);

	retval = le16_to_cpu(el->l_count) - le16_to_cpu(el->l_next_free_rec);
bail:
	brelse(eb_bh);

	mlog_exit(retval);
	return retval;
}

/* expects array to already be allocated
 *
 * sets h_signature, h_blkno, h_suballoc_bit, h_suballoc_slot, and
 * l_count for you
 */
static int ocfs2_create_new_meta_bhs(handle_t *handle,
				     struct ocfs2_extent_tree *et,
				     int wanted,
				     struct ocfs2_alloc_context *meta_ac,
				     struct buffer_head *bhs[])
{
	int count, status, i;
	u16 suballoc_bit_start;
	u32 num_got;
	u64 first_blkno;
	struct ocfs2_super *osb =
		OCFS2_SB(ocfs2_metadata_cache_get_super(et->et_ci));
	struct ocfs2_extent_block *eb;

	mlog_entry_void();

	count = 0;
	while (count < wanted) {
		status = ocfs2_claim_metadata(osb,
					      handle,
					      meta_ac,
					      wanted - count,
					      &suballoc_bit_start,
					      &num_got,
					      &first_blkno);
		if (status < 0) {
			mlog_errno(status);
			goto bail;
		}

		for(i = count;  i < (num_got + count); i++) {
			bhs[i] = sb_getblk(osb->sb, first_blkno);
			if (bhs[i] == NULL) {
				status = -EIO;
				mlog_errno(status);
				goto bail;
			}
			ocfs2_set_new_buffer_uptodate(et->et_ci, bhs[i]);

			status = ocfs2_journal_access_eb(handle, et->et_ci,
							 bhs[i],
							 OCFS2_JOURNAL_ACCESS_CREATE);
			if (status < 0) {
				mlog_errno(status);
				goto bail;
			}

			memset(bhs[i]->b_data, 0, osb->sb->s_blocksize);
			eb = (struct ocfs2_extent_block *) bhs[i]->b_data;
			/* Ok, setup the minimal stuff here. */
			strcpy(eb->h_signature, OCFS2_EXTENT_BLOCK_SIGNATURE);
			eb->h_blkno = cpu_to_le64(first_blkno);
			eb->h_fs_generation = cpu_to_le32(osb->fs_generation);
			eb->h_suballoc_slot = cpu_to_le16(osb->slot_num);
			eb->h_suballoc_bit = cpu_to_le16(suballoc_bit_start);
			eb->h_list.l_count =
				cpu_to_le16(ocfs2_extent_recs_per_eb(osb->sb));

			suballoc_bit_start++;
			first_blkno++;

			/* We'll also be dirtied by the caller, so
			 * this isn't absolutely necessary. */
			status = ocfs2_journal_dirty(handle, bhs[i]);
			if (status < 0) {
				mlog_errno(status);
				goto bail;
			}
		}

		count += num_got;
	}

	status = 0;
bail:
	if (status < 0) {
		for(i = 0; i < wanted; i++) {
			brelse(bhs[i]);
			bhs[i] = NULL;
		}
	}
	mlog_exit(status);
	return status;
}

/*
 * Helper function for ocfs2_add_branch() and ocfs2_shift_tree_depth().
 *
 * Returns the sum of the rightmost extent rec logical offset and
 * cluster count.
 *
 * ocfs2_add_branch() uses this to determine what logical cluster
 * value should be populated into the leftmost new branch records.
 *
 * ocfs2_shift_tree_depth() uses this to determine the # clusters
 * value for the new topmost tree record.
 */
static inline u32 ocfs2_sum_rightmost_rec(struct ocfs2_extent_list  *el)
{
	int i;

	i = le16_to_cpu(el->l_next_free_rec) - 1;

	return le32_to_cpu(el->l_recs[i].e_cpos) +
		ocfs2_rec_clusters(el, &el->l_recs[i]);
}

/*
 * Change range of the branches in the right most path according to the leaf
 * extent block's rightmost record.
 */
static int ocfs2_adjust_rightmost_branch(handle_t *handle,
					 struct ocfs2_extent_tree *et)
{
	int status;
	struct ocfs2_path *path = NULL;
	struct ocfs2_extent_list *el;
	struct ocfs2_extent_rec *rec;

	path = ocfs2_new_path_from_et(et);
	if (!path) {
		status = -ENOMEM;
		return status;
	}

	status = ocfs2_find_path(et->et_ci, path, UINT_MAX);
	if (status < 0) {
		mlog_errno(status);
		goto out;
	}

	status = ocfs2_extend_trans(handle, path_num_items(path) +
				    handle->h_buffer_credits);
	if (status < 0) {
		mlog_errno(status);
		goto out;
	}

	status = ocfs2_journal_access_path(et->et_ci, handle, path);
	if (status < 0) {
		mlog_errno(status);
		goto out;
	}

	el = path_leaf_el(path);
	rec = &el->l_recs[le32_to_cpu(el->l_next_free_rec) - 1];

	ocfs2_adjust_rightmost_records(handle, et, path, rec);

out:
	ocfs2_free_path(path);
	return status;
}

/*
 * Add an entire tree branch to our inode. eb_bh is the extent block
 * to start at, if we don't want to start the branch at the root
 * structure.
 *
 * last_eb_bh is required as we have to update it's next_leaf pointer
 * for the new last extent block.
 *
 * the new branch will be 'empty' in the sense that every block will
 * contain a single record with cluster count == 0.
 */
static int ocfs2_add_branch(handle_t *handle,
			    struct ocfs2_extent_tree *et,
			    struct buffer_head *eb_bh,
			    struct buffer_head **last_eb_bh,
			    struct ocfs2_alloc_context *meta_ac)
{
	int status, new_blocks, i;
	u64 next_blkno, new_last_eb_blk;
	struct buffer_head *bh;
	struct buffer_head **new_eb_bhs = NULL;
	struct ocfs2_extent_block *eb;
	struct ocfs2_extent_list  *eb_el;
	struct ocfs2_extent_list  *el;
	u32 new_cpos, root_end;

	mlog_entry_void();

	BUG_ON(!last_eb_bh || !*last_eb_bh);

	if (eb_bh) {
		eb = (struct ocfs2_extent_block *) eb_bh->b_data;
		el = &eb->h_list;
	} else
		el = et->et_root_el;

	/* we never add a branch to a leaf. */
	BUG_ON(!el->l_tree_depth);

	new_blocks = le16_to_cpu(el->l_tree_depth);

	eb = (struct ocfs2_extent_block *)(*last_eb_bh)->b_data;
	new_cpos = ocfs2_sum_rightmost_rec(&eb->h_list);
	root_end = ocfs2_sum_rightmost_rec(et->et_root_el);

	/*
	 * If there is a gap before the root end and the real end
	 * of the righmost leaf block, we need to remove the gap
	 * between new_cpos and root_end first so that the tree
	 * is consistent after we add a new branch(it will start
	 * from new_cpos).
	 */
	if (root_end > new_cpos) {
		mlog(0, "adjust the cluster end from %u to %u\n",
		     root_end, new_cpos);
		status = ocfs2_adjust_rightmost_branch(handle, et);
		if (status) {
			mlog_errno(status);
			goto bail;
		}
	}

	/* allocate the number of new eb blocks we need */
	new_eb_bhs = kcalloc(new_blocks, sizeof(struct buffer_head *),
			     GFP_KERNEL);
	if (!new_eb_bhs) {
		status = -ENOMEM;
		mlog_errno(status);
		goto bail;
	}

	status = ocfs2_create_new_meta_bhs(handle, et, new_blocks,
					   meta_ac, new_eb_bhs);
	if (status < 0) {
		mlog_errno(status);
		goto bail;
	}

	/* Note: new_eb_bhs[new_blocks - 1] is the guy which will be
	 * linked with the rest of the tree.
	 * conversly, new_eb_bhs[0] is the new bottommost leaf.
	 *
	 * when we leave the loop, new_last_eb_blk will point to the
	 * newest leaf, and next_blkno will point to the topmost extent
	 * block. */
	next_blkno = new_last_eb_blk = 0;
	for(i = 0; i < new_blocks; i++) {
		bh = new_eb_bhs[i];
		eb = (struct ocfs2_extent_block *) bh->b_data;
		/* ocfs2_create_new_meta_bhs() should create it right! */
		BUG_ON(!OCFS2_IS_VALID_EXTENT_BLOCK(eb));
		eb_el = &eb->h_list;

		status = ocfs2_journal_access_eb(handle, et->et_ci, bh,
						 OCFS2_JOURNAL_ACCESS_CREATE);
		if (status < 0) {
			mlog_errno(status);
			goto bail;
		}

		eb->h_next_leaf_blk = 0;
		eb_el->l_tree_depth = cpu_to_le16(i);
		eb_el->l_next_free_rec = cpu_to_le16(1);
		/*
		 * This actually counts as an empty extent as
		 * c_clusters == 0
		 */
		eb_el->l_recs[0].e_cpos = cpu_to_le32(new_cpos);
		eb_el->l_recs[0].e_blkno = cpu_to_le64(next_blkno);
		/*
		 * eb_el isn't always an interior node, but even leaf
		 * nodes want a zero'd flags and reserved field so
		 * this gets the whole 32 bits regardless of use.
		 */
		eb_el->l_recs[0].e_int_clusters = cpu_to_le32(0);
		if (!eb_el->l_tree_depth)
			new_last_eb_blk = le64_to_cpu(eb->h_blkno);

		status = ocfs2_journal_dirty(handle, bh);
		if (status < 0) {
			mlog_errno(status);
			goto bail;
		}

		next_blkno = le64_to_cpu(eb->h_blkno);
	}

	/* This is a bit hairy. We want to update up to three blocks
	 * here without leaving any of them in an inconsistent state
	 * in case of error. We don't have to worry about
	 * journal_dirty erroring as it won't unless we've aborted the
	 * handle (in which case we would never be here) so reserving
	 * the write with journal_access is all we need to do. */
	status = ocfs2_journal_access_eb(handle, et->et_ci, *last_eb_bh,
					 OCFS2_JOURNAL_ACCESS_WRITE);
	if (status < 0) {
		mlog_errno(status);
		goto bail;
	}
	status = ocfs2_et_root_journal_access(handle, et,
					      OCFS2_JOURNAL_ACCESS_WRITE);
	if (status < 0) {
		mlog_errno(status);
		goto bail;
	}
	if (eb_bh) {
		status = ocfs2_journal_access_eb(handle, et->et_ci, eb_bh,
						 OCFS2_JOURNAL_ACCESS_WRITE);
		if (status < 0) {
			mlog_errno(status);
			goto bail;
		}
	}

	/* Link the new branch into the rest of the tree (el will
	 * either be on the root_bh, or the extent block passed in. */
	i = le16_to_cpu(el->l_next_free_rec);
	el->l_recs[i].e_blkno = cpu_to_le64(next_blkno);
	el->l_recs[i].e_cpos = cpu_to_le32(new_cpos);
	el->l_recs[i].e_int_clusters = 0;
	le16_add_cpu(&el->l_next_free_rec, 1);

	/* fe needs a new last extent block pointer, as does the
	 * next_leaf on the previously last-extent-block. */
	ocfs2_et_set_last_eb_blk(et, new_last_eb_blk);

	eb = (struct ocfs2_extent_block *) (*last_eb_bh)->b_data;
	eb->h_next_leaf_blk = cpu_to_le64(new_last_eb_blk);

	status = ocfs2_journal_dirty(handle, *last_eb_bh);
	if (status < 0)
		mlog_errno(status);
	status = ocfs2_journal_dirty(handle, et->et_root_bh);
	if (status < 0)
		mlog_errno(status);
	if (eb_bh) {
		status = ocfs2_journal_dirty(handle, eb_bh);
		if (status < 0)
			mlog_errno(status);
	}

	/*
	 * Some callers want to track the rightmost leaf so pass it
	 * back here.
	 */
	brelse(*last_eb_bh);
	get_bh(new_eb_bhs[0]);
	*last_eb_bh = new_eb_bhs[0];

	status = 0;
bail:
	if (new_eb_bhs) {
		for (i = 0; i < new_blocks; i++)
			brelse(new_eb_bhs[i]);
		kfree(new_eb_bhs);
	}

	mlog_exit(status);
	return status;
}

/*
 * adds another level to the allocation tree.
 * returns back the new extent block so you can add a branch to it
 * after this call.
 */
static int ocfs2_shift_tree_depth(handle_t *handle,
				  struct ocfs2_extent_tree *et,
				  struct ocfs2_alloc_context *meta_ac,
				  struct buffer_head **ret_new_eb_bh)
{
	int status, i;
	u32 new_clusters;
	struct buffer_head *new_eb_bh = NULL;
	struct ocfs2_extent_block *eb;
	struct ocfs2_extent_list  *root_el;
	struct ocfs2_extent_list  *eb_el;

	mlog_entry_void();

	status = ocfs2_create_new_meta_bhs(handle, et, 1, meta_ac,
					   &new_eb_bh);
	if (status < 0) {
		mlog_errno(status);
		goto bail;
	}

	eb = (struct ocfs2_extent_block *) new_eb_bh->b_data;
	/* ocfs2_create_new_meta_bhs() should create it right! */
	BUG_ON(!OCFS2_IS_VALID_EXTENT_BLOCK(eb));

	eb_el = &eb->h_list;
	root_el = et->et_root_el;

	status = ocfs2_journal_access_eb(handle, et->et_ci, new_eb_bh,
					 OCFS2_JOURNAL_ACCESS_CREATE);
	if (status < 0) {
		mlog_errno(status);
		goto bail;
	}

	/* copy the root extent list data into the new extent block */
	eb_el->l_tree_depth = root_el->l_tree_depth;
	eb_el->l_next_free_rec = root_el->l_next_free_rec;
	for (i = 0; i < le16_to_cpu(root_el->l_next_free_rec); i++)
		eb_el->l_recs[i] = root_el->l_recs[i];

	status = ocfs2_journal_dirty(handle, new_eb_bh);
	if (status < 0) {
		mlog_errno(status);
		goto bail;
	}

	status = ocfs2_et_root_journal_access(handle, et,
					      OCFS2_JOURNAL_ACCESS_WRITE);
	if (status < 0) {
		mlog_errno(status);
		goto bail;
	}

	new_clusters = ocfs2_sum_rightmost_rec(eb_el);

	/* update root_bh now */
	le16_add_cpu(&root_el->l_tree_depth, 1);
	root_el->l_recs[0].e_cpos = 0;
	root_el->l_recs[0].e_blkno = eb->h_blkno;
	root_el->l_recs[0].e_int_clusters = cpu_to_le32(new_clusters);
	for (i = 1; i < le16_to_cpu(root_el->l_next_free_rec); i++)
		memset(&root_el->l_recs[i], 0, sizeof(struct ocfs2_extent_rec));
	root_el->l_next_free_rec = cpu_to_le16(1);

	/* If this is our 1st tree depth shift, then last_eb_blk
	 * becomes the allocated extent block */
	if (root_el->l_tree_depth == cpu_to_le16(1))
		ocfs2_et_set_last_eb_blk(et, le64_to_cpu(eb->h_blkno));

	status = ocfs2_journal_dirty(handle, et->et_root_bh);
	if (status < 0) {
		mlog_errno(status);
		goto bail;
	}

	*ret_new_eb_bh = new_eb_bh;
	new_eb_bh = NULL;
	status = 0;
bail:
	brelse(new_eb_bh);

	mlog_exit(status);
	return status;
}

/*
 * Should only be called when there is no space left in any of the
 * leaf nodes. What we want to do is find the lowest tree depth
 * non-leaf extent block with room for new records. There are three
 * valid results of this search:
 *
 * 1) a lowest extent block is found, then we pass it back in
 *    *lowest_eb_bh and return '0'
 *
 * 2) the search fails to find anything, but the root_el has room. We
 *    pass NULL back in *lowest_eb_bh, but still return '0'
 *
 * 3) the search fails to find anything AND the root_el is full, in
 *    which case we return > 0
 *
 * return status < 0 indicates an error.
 */
static int ocfs2_find_branch_target(struct ocfs2_extent_tree *et,
				    struct buffer_head **target_bh)
{
	int status = 0, i;
	u64 blkno;
	struct ocfs2_extent_block *eb;
	struct ocfs2_extent_list  *el;
	struct buffer_head *bh = NULL;
	struct buffer_head *lowest_bh = NULL;

	mlog_entry_void();

	*target_bh = NULL;

	el = et->et_root_el;

	while(le16_to_cpu(el->l_tree_depth) > 1) {
		if (le16_to_cpu(el->l_next_free_rec) == 0) {
			ocfs2_error(ocfs2_metadata_cache_get_super(et->et_ci),
				    "Owner %llu has empty "
				    "extent list (next_free_rec == 0)",
				    (unsigned long long)ocfs2_metadata_cache_owner(et->et_ci));
			status = -EIO;
			goto bail;
		}
		i = le16_to_cpu(el->l_next_free_rec) - 1;
		blkno = le64_to_cpu(el->l_recs[i].e_blkno);
		if (!blkno) {
			ocfs2_error(ocfs2_metadata_cache_get_super(et->et_ci),
				    "Owner %llu has extent "
				    "list where extent # %d has no physical "
				    "block start",
				    (unsigned long long)ocfs2_metadata_cache_owner(et->et_ci), i);
			status = -EIO;
			goto bail;
		}

		brelse(bh);
		bh = NULL;

		status = ocfs2_read_extent_block(et->et_ci, blkno, &bh);
		if (status < 0) {
			mlog_errno(status);
			goto bail;
		}

		eb = (struct ocfs2_extent_block *) bh->b_data;
		el = &eb->h_list;

		if (le16_to_cpu(el->l_next_free_rec) <
		    le16_to_cpu(el->l_count)) {
			brelse(lowest_bh);
			lowest_bh = bh;
			get_bh(lowest_bh);
		}
	}

	/* If we didn't find one and the fe doesn't have any room,
	 * then return '1' */
	el = et->et_root_el;
	if (!lowest_bh && (el->l_next_free_rec == el->l_count))
		status = 1;

	*target_bh = lowest_bh;
bail:
	brelse(bh);

	mlog_exit(status);
	return status;
}

/*
 * Grow a b-tree so that it has more records.
 *
 * We might shift the tree depth in which case existing paths should
 * be considered invalid.
 *
 * Tree depth after the grow is returned via *final_depth.
 *
 * *last_eb_bh will be updated by ocfs2_add_branch().
 */
static int ocfs2_grow_tree(handle_t *handle, struct ocfs2_extent_tree *et,
			   int *final_depth, struct buffer_head **last_eb_bh,
			   struct ocfs2_alloc_context *meta_ac)
{
	int ret, shift;
	struct ocfs2_extent_list *el = et->et_root_el;
	int depth = le16_to_cpu(el->l_tree_depth);
	struct buffer_head *bh = NULL;

	BUG_ON(meta_ac == NULL);

	shift = ocfs2_find_branch_target(et, &bh);
	if (shift < 0) {
		ret = shift;
		mlog_errno(ret);
		goto out;
	}

	/* We traveled all the way to the bottom of the allocation tree
	 * and didn't find room for any more extents - we need to add
	 * another tree level */
	if (shift) {
		BUG_ON(bh);
		mlog(0, "need to shift tree depth (current = %d)\n", depth);

		/* ocfs2_shift_tree_depth will return us a buffer with
		 * the new extent block (so we can pass that to
		 * ocfs2_add_branch). */
		ret = ocfs2_shift_tree_depth(handle, et, meta_ac, &bh);
		if (ret < 0) {
			mlog_errno(ret);
			goto out;
		}
		depth++;
		if (depth == 1) {
			/*
			 * Special case: we have room now if we shifted from
			 * tree_depth 0, so no more work needs to be done.
			 *
			 * We won't be calling add_branch, so pass
			 * back *last_eb_bh as the new leaf. At depth
			 * zero, it should always be null so there's
			 * no reason to brelse.
			 */
			BUG_ON(*last_eb_bh);
			get_bh(bh);
			*last_eb_bh = bh;
			goto out;
		}
	}

	/* call ocfs2_add_branch to add the final part of the tree with
	 * the new data. */
	mlog(0, "add branch. bh = %p\n", bh);
	ret = ocfs2_add_branch(handle, et, bh, last_eb_bh,
			       meta_ac);
	if (ret < 0) {
		mlog_errno(ret);
		goto out;
	}

out:
	if (final_depth)
		*final_depth = depth;
	brelse(bh);
	return ret;
}

/*
 * This function will discard the rightmost extent record.
 */
static void ocfs2_shift_records_right(struct ocfs2_extent_list *el)
{
	int next_free = le16_to_cpu(el->l_next_free_rec);
	int count = le16_to_cpu(el->l_count);
	unsigned int num_bytes;

	BUG_ON(!next_free);
	/* This will cause us to go off the end of our extent list. */
	BUG_ON(next_free >= count);

	num_bytes = sizeof(struct ocfs2_extent_rec) * next_free;

	memmove(&el->l_recs[1], &el->l_recs[0], num_bytes);
}

static void ocfs2_rotate_leaf(struct ocfs2_extent_list *el,
			      struct ocfs2_extent_rec *insert_rec)
{
	int i, insert_index, next_free, has_empty, num_bytes;
	u32 insert_cpos = le32_to_cpu(insert_rec->e_cpos);
	struct ocfs2_extent_rec *rec;

	next_free = le16_to_cpu(el->l_next_free_rec);
	has_empty = ocfs2_is_empty_extent(&el->l_recs[0]);

	BUG_ON(!next_free);

	/* The tree code before us didn't allow enough room in the leaf. */
	BUG_ON(el->l_next_free_rec == el->l_count && !has_empty);

	/*
	 * The easiest way to approach this is to just remove the
	 * empty extent and temporarily decrement next_free.
	 */
	if (has_empty) {
		/*
		 * If next_free was 1 (only an empty extent), this
		 * loop won't execute, which is fine. We still want
		 * the decrement above to happen.
		 */
		for(i = 0; i < (next_free - 1); i++)
			el->l_recs[i] = el->l_recs[i+1];

		next_free--;
	}

	/*
	 * Figure out what the new record index should be.
	 */
	for(i = 0; i < next_free; i++) {
		rec = &el->l_recs[i];

		if (insert_cpos < le32_to_cpu(rec->e_cpos))
			break;
	}
	insert_index = i;

	mlog(0, "ins %u: index %d, has_empty %d, next_free %d, count %d\n",
	     insert_cpos, insert_index, has_empty, next_free, le16_to_cpu(el->l_count));

	BUG_ON(insert_index < 0);
	BUG_ON(insert_index >= le16_to_cpu(el->l_count));
	BUG_ON(insert_index > next_free);

	/*
	 * No need to memmove if we're just adding to the tail.
	 */
	if (insert_index != next_free) {
		BUG_ON(next_free >= le16_to_cpu(el->l_count));

		num_bytes = next_free - insert_index;
		num_bytes *= sizeof(struct ocfs2_extent_rec);
		memmove(&el->l_recs[insert_index + 1],
			&el->l_recs[insert_index],
			num_bytes);
	}

	/*
	 * Either we had an empty extent, and need to re-increment or
	 * there was no empty extent on a non full rightmost leaf node,
	 * in which case we still need to increment.
	 */
	next_free++;
	el->l_next_free_rec = cpu_to_le16(next_free);
	/*
	 * Make sure none of the math above just messed up our tree.
	 */
	BUG_ON(le16_to_cpu(el->l_next_free_rec) > le16_to_cpu(el->l_count));

	el->l_recs[insert_index] = *insert_rec;

}

static void ocfs2_remove_empty_extent(struct ocfs2_extent_list *el)
{
	int size, num_recs = le16_to_cpu(el->l_next_free_rec);

	BUG_ON(num_recs == 0);

	if (ocfs2_is_empty_extent(&el->l_recs[0])) {
		num_recs--;
		size = num_recs * sizeof(struct ocfs2_extent_rec);
		memmove(&el->l_recs[0], &el->l_recs[1], size);
		memset(&el->l_recs[num_recs], 0,
		       sizeof(struct ocfs2_extent_rec));
		el->l_next_free_rec = cpu_to_le16(num_recs);
	}
}

/*
 * Create an empty extent record .
 *
 * l_next_free_rec may be updated.
 *
 * If an empty extent already exists do nothing.
 */
static void ocfs2_create_empty_extent(struct ocfs2_extent_list *el)
{
	int next_free = le16_to_cpu(el->l_next_free_rec);

	BUG_ON(le16_to_cpu(el->l_tree_depth) != 0);

	if (next_free == 0)
		goto set_and_inc;

	if (ocfs2_is_empty_extent(&el->l_recs[0]))
		return;

	mlog_bug_on_msg(el->l_count == el->l_next_free_rec,
			"Asked to create an empty extent in a full list:\n"
			"count = %u, tree depth = %u",
			le16_to_cpu(el->l_count),
			le16_to_cpu(el->l_tree_depth));

	ocfs2_shift_records_right(el);

set_and_inc:
	le16_add_cpu(&el->l_next_free_rec, 1);
	memset(&el->l_recs[0], 0, sizeof(struct ocfs2_extent_rec));
}

/*
 * For a rotation which involves two leaf nodes, the "root node" is
 * the lowest level tree node which contains a path to both leafs. This
 * resulting set of information can be used to form a complete "subtree"
 *
 * This function is passed two full paths from the dinode down to a
 * pair of adjacent leaves. It's task is to figure out which path
 * index contains the subtree root - this can be the root index itself
 * in a worst-case rotation.
 *
 * The array index of the subtree root is passed back.
 */
static int ocfs2_find_subtree_root(struct ocfs2_extent_tree *et,
				   struct ocfs2_path *left,
				   struct ocfs2_path *right)
{
	int i = 0;

	/*
	 * Check that the caller passed in two paths from the same tree.
	 */
	BUG_ON(path_root_bh(left) != path_root_bh(right));

	do {
		i++;

		/*
		 * The caller didn't pass two adjacent paths.
		 */
		mlog_bug_on_msg(i > left->p_tree_depth,
				"Owner %llu, left depth %u, right depth %u\n"
				"left leaf blk %llu, right leaf blk %llu\n",
				(unsigned long long)ocfs2_metadata_cache_owner(et->et_ci),
				left->p_tree_depth, right->p_tree_depth,
				(unsigned long long)path_leaf_bh(left)->b_blocknr,
				(unsigned long long)path_leaf_bh(right)->b_blocknr);
	} while (left->p_node[i].bh->b_blocknr ==
		 right->p_node[i].bh->b_blocknr);

	return i - 1;
}

typedef void (path_insert_t)(void *, struct buffer_head *);

/*
 * Traverse a btree path in search of cpos, starting at root_el.
 *
 * This code can be called with a cpos larger than the tree, in which
 * case it will return the rightmost path.
 */
static int __ocfs2_find_path(struct ocfs2_caching_info *ci,
			     struct ocfs2_extent_list *root_el, u32 cpos,
			     path_insert_t *func, void *data)
{
	int i, ret = 0;
	u32 range;
	u64 blkno;
	struct buffer_head *bh = NULL;
	struct ocfs2_extent_block *eb;
	struct ocfs2_extent_list *el;
	struct ocfs2_extent_rec *rec;

	el = root_el;
	while (el->l_tree_depth) {
		if (le16_to_cpu(el->l_next_free_rec) == 0) {
			ocfs2_error(ocfs2_metadata_cache_get_super(ci),
				    "Owner %llu has empty extent list at "
				    "depth %u\n",
				    (unsigned long long)ocfs2_metadata_cache_owner(ci),
				    le16_to_cpu(el->l_tree_depth));
			ret = -EROFS;
			goto out;

		}

		for(i = 0; i < le16_to_cpu(el->l_next_free_rec) - 1; i++) {
			rec = &el->l_recs[i];

			/*
			 * In the case that cpos is off the allocation
			 * tree, this should just wind up returning the
			 * rightmost record.
			 */
			range = le32_to_cpu(rec->e_cpos) +
				ocfs2_rec_clusters(el, rec);
			if (cpos >= le32_to_cpu(rec->e_cpos) && cpos < range)
			    break;
		}

		blkno = le64_to_cpu(el->l_recs[i].e_blkno);
		if (blkno == 0) {
			ocfs2_error(ocfs2_metadata_cache_get_super(ci),
				    "Owner %llu has bad blkno in extent list "
				    "at depth %u (index %d)\n",
				    (unsigned long long)ocfs2_metadata_cache_owner(ci),
				    le16_to_cpu(el->l_tree_depth), i);
			ret = -EROFS;
			goto out;
		}

		brelse(bh);
		bh = NULL;
		ret = ocfs2_read_extent_block(ci, blkno, &bh);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}

		eb = (struct ocfs2_extent_block *) bh->b_data;
		el = &eb->h_list;

		if (le16_to_cpu(el->l_next_free_rec) >
		    le16_to_cpu(el->l_count)) {
			ocfs2_error(ocfs2_metadata_cache_get_super(ci),
				    "Owner %llu has bad count in extent list "
				    "at block %llu (next free=%u, count=%u)\n",
				    (unsigned long long)ocfs2_metadata_cache_owner(ci),
				    (unsigned long long)bh->b_blocknr,
				    le16_to_cpu(el->l_next_free_rec),
				    le16_to_cpu(el->l_count));
			ret = -EROFS;
			goto out;
		}

		if (func)
			func(data, bh);
	}

out:
	/*
	 * Catch any trailing bh that the loop didn't handle.
	 */
	brelse(bh);

	return ret;
}

/*
 * Given an initialized path (that is, it has a valid root extent
 * list), this function will traverse the btree in search of the path
 * which would contain cpos.
 *
 * The path traveled is recorded in the path structure.
 *
 * Note that this will not do any comparisons on leaf node extent
 * records, so it will work fine in the case that we just added a tree
 * branch.
 */
struct find_path_data {
	int index;
	struct ocfs2_path *path;
};
static void find_path_ins(void *data, struct buffer_head *bh)
{
	struct find_path_data *fp = data;

	get_bh(bh);
	ocfs2_path_insert_eb(fp->path, fp->index, bh);
	fp->index++;
}
int ocfs2_find_path(struct ocfs2_caching_info *ci,
		    struct ocfs2_path *path, u32 cpos)
{
	struct find_path_data data;

	data.index = 1;
	data.path = path;
	return __ocfs2_find_path(ci, path_root_el(path), cpos,
				 find_path_ins, &data);
}

static void find_leaf_ins(void *data, struct buffer_head *bh)
{
	struct ocfs2_extent_block *eb =(struct ocfs2_extent_block *)bh->b_data;
	struct ocfs2_extent_list *el = &eb->h_list;
	struct buffer_head **ret = data;

	/* We want to retain only the leaf block. */
	if (le16_to_cpu(el->l_tree_depth) == 0) {
		get_bh(bh);
		*ret = bh;
	}
}
/*
 * Find the leaf block in the tree which would contain cpos. No
 * checking of the actual leaf is done.
 *
 * Some paths want to call this instead of allocating a path structure
 * and calling ocfs2_find_path().
 *
 * This function doesn't handle non btree extent lists.
 */
int ocfs2_find_leaf(struct ocfs2_caching_info *ci,
		    struct ocfs2_extent_list *root_el, u32 cpos,
		    struct buffer_head **leaf_bh)
{
	int ret;
	struct buffer_head *bh = NULL;

	ret = __ocfs2_find_path(ci, root_el, cpos, find_leaf_ins, &bh);
	if (ret) {
		mlog_errno(ret);
		goto out;
	}

	*leaf_bh = bh;
out:
	return ret;
}

/*
 * Adjust the adjacent records (left_rec, right_rec) involved in a rotation.
 *
 * Basically, we've moved stuff around at the bottom of the tree and
 * we need to fix up the extent records above the changes to reflect
 * the new changes.
 *
 * left_rec: the record on the left.
 * left_child_el: is the child list pointed to by left_rec
 * right_rec: the record to the right of left_rec
 * right_child_el: is the child list pointed to by right_rec
 *
 * By definition, this only works on interior nodes.
 */
static void ocfs2_adjust_adjacent_records(struct ocfs2_extent_rec *left_rec,
				  struct ocfs2_extent_list *left_child_el,
				  struct ocfs2_extent_rec *right_rec,
				  struct ocfs2_extent_list *right_child_el)
{
	u32 left_clusters, right_end;

	/*
	 * Interior nodes never have holes. Their cpos is the cpos of
	 * the leftmost record in their child list. Their cluster
	 * count covers the full theoretical range of their child list
	 * - the range between their cpos and the cpos of the record
	 * immediately to their right.
	 */
	left_clusters = le32_to_cpu(right_child_el->l_recs[0].e_cpos);
	if (!ocfs2_rec_clusters(right_child_el, &right_child_el->l_recs[0])) {
		BUG_ON(right_child_el->l_tree_depth);
		BUG_ON(le16_to_cpu(right_child_el->l_next_free_rec) <= 1);
		left_clusters = le32_to_cpu(right_child_el->l_recs[1].e_cpos);
	}
	left_clusters -= le32_to_cpu(left_rec->e_cpos);
	left_rec->e_int_clusters = cpu_to_le32(left_clusters);

	/*
	 * Calculate the rightmost cluster count boundary before
	 * moving cpos - we will need to adjust clusters after
	 * updating e_cpos to keep the same highest cluster count.
	 */
	right_end = le32_to_cpu(right_rec->e_cpos);
	right_end += le32_to_cpu(right_rec->e_int_clusters);

	right_rec->e_cpos = left_rec->e_cpos;
	le32_add_cpu(&right_rec->e_cpos, left_clusters);

	right_end -= le32_to_cpu(right_rec->e_cpos);
	right_rec->e_int_clusters = cpu_to_le32(right_end);
}

/*
 * Adjust the adjacent root node records involved in a
 * rotation. left_el_blkno is passed in as a key so that we can easily
 * find it's index in the root list.
 */
static void ocfs2_adjust_root_records(struct ocfs2_extent_list *root_el,
				      struct ocfs2_extent_list *left_el,
				      struct ocfs2_extent_list *right_el,
				      u64 left_el_blkno)
{
	int i;

	BUG_ON(le16_to_cpu(root_el->l_tree_depth) <=
	       le16_to_cpu(left_el->l_tree_depth));

	for(i = 0; i < le16_to_cpu(root_el->l_next_free_rec) - 1; i++) {
		if (le64_to_cpu(root_el->l_recs[i].e_blkno) == left_el_blkno)
			break;
	}

	/*
	 * The path walking code should have never returned a root and
	 * two paths which are not adjacent.
	 */
	BUG_ON(i >= (le16_to_cpu(root_el->l_next_free_rec) - 1));

	ocfs2_adjust_adjacent_records(&root_el->l_recs[i], left_el,
				      &root_el->l_recs[i + 1], right_el);
}

/*
 * We've changed a leaf block (in right_path) and need to reflect that
 * change back up the subtree.
 *
 * This happens in multiple places:
 *   - When we've moved an extent record from the left path leaf to the right
 *     path leaf to make room for an empty extent in the left path leaf.
 *   - When our insert into the right path leaf is at the leftmost edge
 *     and requires an update of the path immediately to it's left. This
 *     can occur at the end of some types of rotation and appending inserts.
 *   - When we've adjusted the last extent record in the left path leaf and the
 *     1st extent record in the right path leaf during cross extent block merge.
 */
static void ocfs2_complete_edge_insert(handle_t *handle,
				       struct ocfs2_path *left_path,
				       struct ocfs2_path *right_path,
				       int subtree_index)
{
	int ret, i, idx;
	struct ocfs2_extent_list *el, *left_el, *right_el;
	struct ocfs2_extent_rec *left_rec, *right_rec;
	struct buffer_head *root_bh = left_path->p_node[subtree_index].bh;

	/*
	 * Update the counts and position values within all the
	 * interior nodes to reflect the leaf rotation we just did.
	 *
	 * The root node is handled below the loop.
	 *
	 * We begin the loop with right_el and left_el pointing to the
	 * leaf lists and work our way up.
	 *
	 * NOTE: within this loop, left_el and right_el always refer
	 * to the *child* lists.
	 */
	left_el = path_leaf_el(left_path);
	right_el = path_leaf_el(right_path);
	for(i = left_path->p_tree_depth - 1; i > subtree_index; i--) {
		mlog(0, "Adjust records at index %u\n", i);

		/*
		 * One nice property of knowing that all of these
		 * nodes are below the root is that we only deal with
		 * the leftmost right node record and the rightmost
		 * left node record.
		 */
		el = left_path->p_node[i].el;
		idx = le16_to_cpu(left_el->l_next_free_rec) - 1;
		left_rec = &el->l_recs[idx];

		el = right_path->p_node[i].el;
		right_rec = &el->l_recs[0];

		ocfs2_adjust_adjacent_records(left_rec, left_el, right_rec,
					      right_el);

		ret = ocfs2_journal_dirty(handle, left_path->p_node[i].bh);
		if (ret)
			mlog_errno(ret);

		ret = ocfs2_journal_dirty(handle, right_path->p_node[i].bh);
		if (ret)
			mlog_errno(ret);

		/*
		 * Setup our list pointers now so that the current
		 * parents become children in the next iteration.
		 */
		left_el = left_path->p_node[i].el;
		right_el = right_path->p_node[i].el;
	}

	/*
	 * At the root node, adjust the two adjacent records which
	 * begin our path to the leaves.
	 */

	el = left_path->p_node[subtree_index].el;
	left_el = left_path->p_node[subtree_index + 1].el;
	right_el = right_path->p_node[subtree_index + 1].el;

	ocfs2_adjust_root_records(el, left_el, right_el,
				  left_path->p_node[subtree_index + 1].bh->b_blocknr);

	root_bh = left_path->p_node[subtree_index].bh;

	ret = ocfs2_journal_dirty(handle, root_bh);
	if (ret)
		mlog_errno(ret);
}

static int ocfs2_rotate_subtree_right(handle_t *handle,
				      struct ocfs2_extent_tree *et,
				      struct ocfs2_path *left_path,
				      struct ocfs2_path *right_path,
				      int subtree_index)
{
	int ret, i;
	struct buffer_head *right_leaf_bh;
	struct buffer_head *left_leaf_bh = NULL;
	struct buffer_head *root_bh;
	struct ocfs2_extent_list *right_el, *left_el;
	struct ocfs2_extent_rec move_rec;

	left_leaf_bh = path_leaf_bh(left_path);
	left_el = path_leaf_el(left_path);

	if (left_el->l_next_free_rec != left_el->l_count) {
		ocfs2_error(ocfs2_metadata_cache_get_super(et->et_ci),
			    "Inode %llu has non-full interior leaf node %llu"
			    "(next free = %u)",
			    (unsigned long long)ocfs2_metadata_cache_owner(et->et_ci),
			    (unsigned long long)left_leaf_bh->b_blocknr,
			    le16_to_cpu(left_el->l_next_free_rec));
		return -EROFS;
	}

	/*
	 * This extent block may already have an empty record, so we
	 * return early if so.
	 */
	if (ocfs2_is_empty_extent(&left_el->l_recs[0]))
		return 0;

	root_bh = left_path->p_node[subtree_index].bh;
	BUG_ON(root_bh != right_path->p_node[subtree_index].bh);

	ret = ocfs2_path_bh_journal_access(handle, et->et_ci, right_path,
					   subtree_index);
	if (ret) {
		mlog_errno(ret);
		goto out;
	}

	for(i = subtree_index + 1; i < path_num_items(right_path); i++) {
		ret = ocfs2_path_bh_journal_access(handle, et->et_ci,
						   right_path, i);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}

		ret = ocfs2_path_bh_journal_access(handle, et->et_ci,
						   left_path, i);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}
	}

	right_leaf_bh = path_leaf_bh(right_path);
	right_el = path_leaf_el(right_path);

	/* This is a code error, not a disk corruption. */
	mlog_bug_on_msg(!right_el->l_next_free_rec, "Inode %llu: Rotate fails "
			"because rightmost leaf block %llu is empty\n",
			(unsigned long long)ocfs2_metadata_cache_owner(et->et_ci),
			(unsigned long long)right_leaf_bh->b_blocknr);

	ocfs2_create_empty_extent(right_el);

	ret = ocfs2_journal_dirty(handle, right_leaf_bh);
	if (ret) {
		mlog_errno(ret);
		goto out;
	}

	/* Do the copy now. */
	i = le16_to_cpu(left_el->l_next_free_rec) - 1;
	move_rec = left_el->l_recs[i];
	right_el->l_recs[0] = move_rec;

	/*
	 * Clear out the record we just copied and shift everything
	 * over, leaving an empty extent in the left leaf.
	 *
	 * We temporarily subtract from next_free_rec so that the
	 * shift will lose the tail record (which is now defunct).
	 */
	le16_add_cpu(&left_el->l_next_free_rec, -1);
	ocfs2_shift_records_right(left_el);
	memset(&left_el->l_recs[0], 0, sizeof(struct ocfs2_extent_rec));
	le16_add_cpu(&left_el->l_next_free_rec, 1);

	ret = ocfs2_journal_dirty(handle, left_leaf_bh);
	if (ret) {
		mlog_errno(ret);
		goto out;
	}

	ocfs2_complete_edge_insert(handle, left_path, right_path,
				   subtree_index);

out:
	return ret;
}

/*
 * Given a full path, determine what cpos value would return us a path
 * containing the leaf immediately to the left of the current one.
 *
 * Will return zero if the path passed in is already the leftmost path.
 */
static int ocfs2_find_cpos_for_left_leaf(struct super_block *sb,
					 struct ocfs2_path *path, u32 *cpos)
{
	int i, j, ret = 0;
	u64 blkno;
	struct ocfs2_extent_list *el;

	BUG_ON(path->p_tree_depth == 0);

	*cpos = 0;

	blkno = path_leaf_bh(path)->b_blocknr;

	/* Start at the tree node just above the leaf and work our way up. */
	i = path->p_tree_depth - 1;
	while (i >= 0) {
		el = path->p_node[i].el;

		/*
		 * Find the extent record just before the one in our
		 * path.
		 */
		for(j = 0; j < le16_to_cpu(el->l_next_free_rec); j++) {
			if (le64_to_cpu(el->l_recs[j].e_blkno) == blkno) {
				if (j == 0) {
					if (i == 0) {
						/*
						 * We've determined that the
						 * path specified is already
						 * the leftmost one - return a
						 * cpos of zero.
						 */
						goto out;
					}
					/*
					 * The leftmost record points to our
					 * leaf - we need to travel up the
					 * tree one level.
					 */
					goto next_node;
				}

				*cpos = le32_to_cpu(el->l_recs[j - 1].e_cpos);
				*cpos = *cpos + ocfs2_rec_clusters(el,
							   &el->l_recs[j - 1]);
				*cpos = *cpos - 1;
				goto out;
			}
		}

		/*
		 * If we got here, we never found a valid node where
		 * the tree indicated one should be.
		 */
		ocfs2_error(sb,
			    "Invalid extent tree at extent block %llu\n",
			    (unsigned long long)blkno);
		ret = -EROFS;
		goto out;

next_node:
		blkno = path->p_node[i].bh->b_blocknr;
		i--;
	}

out:
	return ret;
}

/*
 * Extend the transaction by enough credits to complete the rotation,
 * and still leave at least the original number of credits allocated
 * to this transaction.
 */
static int ocfs2_extend_rotate_transaction(handle_t *handle, int subtree_depth,
					   int op_credits,
					   struct ocfs2_path *path)
{
	int ret;
	int credits = (path->p_tree_depth - subtree_depth) * 2 + 1 + op_credits;

	if (handle->h_buffer_credits < credits) {
		ret = ocfs2_extend_trans(handle,
					 credits - handle->h_buffer_credits);
		if (ret)
			return ret;

		if (unlikely(handle->h_buffer_credits < credits))
			return ocfs2_extend_trans(handle, credits);
	}

	return 0;
}

/*
 * Trap the case where we're inserting into the theoretical range past
 * the _actual_ left leaf range. Otherwise, we'll rotate a record
 * whose cpos is less than ours into the right leaf.
 *
 * It's only necessary to look at the rightmost record of the left
 * leaf because the logic that calls us should ensure that the
 * theoretical ranges in the path components above the leaves are
 * correct.
 */
static int ocfs2_rotate_requires_path_adjustment(struct ocfs2_path *left_path,
						 u32 insert_cpos)
{
	struct ocfs2_extent_list *left_el;
	struct ocfs2_extent_rec *rec;
	int next_free;

	left_el = path_leaf_el(left_path);
	next_free = le16_to_cpu(left_el->l_next_free_rec);
	rec = &left_el->l_recs[next_free - 1];

	if (insert_cpos > le32_to_cpu(rec->e_cpos))
		return 1;
	return 0;
}

static int ocfs2_leftmost_rec_contains(struct ocfs2_extent_list *el, u32 cpos)
{
	int next_free = le16_to_cpu(el->l_next_free_rec);
	unsigned int range;
	struct ocfs2_extent_rec *rec;

	if (next_free == 0)
		return 0;

	rec = &el->l_recs[0];
	if (ocfs2_is_empty_extent(rec)) {
		/* Empty list. */
		if (next_free == 1)
			return 0;
		rec = &el->l_recs[1];
	}

	range = le32_to_cpu(rec->e_cpos) + ocfs2_rec_clusters(el, rec);
	if (cpos >= le32_to_cpu(rec->e_cpos) && cpos < range)
		return 1;
	return 0;
}

/*
 * Rotate all the records in a btree right one record, starting at insert_cpos.
 *
 * The path to the rightmost leaf should be passed in.
 *
 * The array is assumed to be large enough to hold an entire path (tree depth).
 *
 * Upon succesful return from this function:
 *
 * - The 'right_path' array will contain a path to the leaf block
 *   whose range contains e_cpos.
 * - That leaf block will have a single empty extent in list index 0.
 * - In the case that the rotation requires a post-insert update,
 *   *ret_left_path will contain a valid path which can be passed to
 *   ocfs2_insert_path().
 */
static int ocfs2_rotate_tree_right(handle_t *handle,
				   struct ocfs2_extent_tree *et,
				   enum ocfs2_split_type split,
				   u32 insert_cpos,
				   struct ocfs2_path *right_path,
				   struct ocfs2_path **ret_left_path)
{
	int ret, start, orig_credits = handle->h_buffer_credits;
	u32 cpos;
	struct ocfs2_path *left_path = NULL;
	struct super_block *sb = ocfs2_metadata_cache_get_super(et->et_ci);

	*ret_left_path = NULL;

	left_path = ocfs2_new_path_from_path(right_path);
	if (!left_path) {
		ret = -ENOMEM;
		mlog_errno(ret);
		goto out;
	}

	ret = ocfs2_find_cpos_for_left_leaf(sb, right_path, &cpos);
	if (ret) {
		mlog_errno(ret);
		goto out;
	}

	mlog(0, "Insert: %u, first left path cpos: %u\n", insert_cpos, cpos);

	/*
	 * What we want to do here is:
	 *
	 * 1) Start with the rightmost path.
	 *
	 * 2) Determine a path to the leaf block directly to the left
	 *    of that leaf.
	 *
	 * 3) Determine the 'subtree root' - the lowest level tree node
	 *    which contains a path to both leaves.
	 *
	 * 4) Rotate the subtree.
	 *
	 * 5) Find the next subtree by considering the left path to be
	 *    the new right path.
	 *
	 * The check at the top of this while loop also accepts
	 * insert_cpos == cpos because cpos is only a _theoretical_
	 * value to get us the left path - insert_cpos might very well
	 * be filling that hole.
	 *
	 * Stop at a cpos of '0' because we either started at the
	 * leftmost branch (i.e., a tree with one branch and a
	 * rotation inside of it), or we've gone as far as we can in
	 * rotating subtrees.
	 */
	while (cpos && insert_cpos <= cpos) {
		mlog(0, "Rotating a tree: ins. cpos: %u, left path cpos: %u\n",
		     insert_cpos, cpos);

		ret = ocfs2_find_path(et->et_ci, left_path, cpos);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}

		mlog_bug_on_msg(path_leaf_bh(left_path) ==
				path_leaf_bh(right_path),
				"Owner %llu: error during insert of %u "
				"(left path cpos %u) results in two identical "
				"paths ending at %llu\n",
				(unsigned long long)ocfs2_metadata_cache_owner(et->et_ci),
				insert_cpos, cpos,
				(unsigned long long)
				path_leaf_bh(left_path)->b_blocknr);

		if (split == SPLIT_NONE &&
		    ocfs2_rotate_requires_path_adjustment(left_path,
							  insert_cpos)) {

			/*
			 * We've rotated the tree as much as we
			 * should. The rest is up to
			 * ocfs2_insert_path() to complete, after the
			 * record insertion. We indicate this
			 * situation by returning the left path.
			 *
			 * The reason we don't adjust the records here
			 * before the record insert is that an error
			 * later might break the rule where a parent
			 * record e_cpos will reflect the actual
			 * e_cpos of the 1st nonempty record of the
			 * child list.
			 */
			*ret_left_path = left_path;
			goto out_ret_path;
		}

		start = ocfs2_find_subtree_root(et, left_path, right_path);

		mlog(0, "Subtree root at index %d (blk %llu, depth %d)\n",
		     start,
		     (unsigned long long) right_path->p_node[start].bh->b_blocknr,
		     right_path->p_tree_depth);

		ret = ocfs2_extend_rotate_transaction(handle, start,
						      orig_credits, right_path);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}

		ret = ocfs2_rotate_subtree_right(handle, et, left_path,
						 right_path, start);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}

		if (split != SPLIT_NONE &&
		    ocfs2_leftmost_rec_contains(path_leaf_el(right_path),
						insert_cpos)) {
			/*
			 * A rotate moves the rightmost left leaf
			 * record over to the leftmost right leaf
			 * slot. If we're doing an extent split
			 * instead of a real insert, then we have to
			 * check that the extent to be split wasn't
			 * just moved over. If it was, then we can
			 * exit here, passing left_path back -
			 * ocfs2_split_extent() is smart enough to
			 * search both leaves.
			 */
			*ret_left_path = left_path;
			goto out_ret_path;
		}

		/*
		 * There is no need to re-read the next right path
		 * as we know that it'll be our current left
		 * path. Optimize by copying values instead.
		 */
		ocfs2_mv_path(right_path, left_path);

		ret = ocfs2_find_cpos_for_left_leaf(sb, right_path, &cpos);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}
	}

out:
	ocfs2_free_path(left_path);

out_ret_path:
	return ret;
}

static int ocfs2_update_edge_lengths(handle_t *handle,
				     struct ocfs2_extent_tree *et,
				     int subtree_index, struct ocfs2_path *path)
{
	int i, idx, ret;
	struct ocfs2_extent_rec *rec;
	struct ocfs2_extent_list *el;
	struct ocfs2_extent_block *eb;
	u32 range;

	/*
	 * In normal tree rotation process, we will never touch the
	 * tree branch above subtree_index and ocfs2_extend_rotate_transaction
	 * doesn't reserve the credits for them either.
	 *
	 * But we do have a special case here which will update the rightmost
	 * records for all the bh in the path.
	 * So we have to allocate extra credits and access them.
	 */
	ret = ocfs2_extend_trans(handle,
				 handle->h_buffer_credits + subtree_index);
	if (ret) {
		mlog_errno(ret);
		goto out;
	}

	ret = ocfs2_journal_access_path(et->et_ci, handle, path);
	if (ret) {
		mlog_errno(ret);
		goto out;
	}

	/* Path should always be rightmost. */
	eb = (struct ocfs2_extent_block *)path_leaf_bh(path)->b_data;
	BUG_ON(eb->h_next_leaf_blk != 0ULL);

	el = &eb->h_list;
	BUG_ON(le16_to_cpu(el->l_next_free_rec) == 0);
	idx = le16_to_cpu(el->l_next_free_rec) - 1;
	rec = &el->l_recs[idx];
	range = le32_to_cpu(rec->e_cpos) + ocfs2_rec_clusters(el, rec);

	for (i = 0; i < path->p_tree_depth; i++) {
		el = path->p_node[i].el;
		idx = le16_to_cpu(el->l_next_free_rec) - 1;
		rec = &el->l_recs[idx];

		rec->e_int_clusters = cpu_to_le32(range);
		le32_add_cpu(&rec->e_int_clusters, -le32_to_cpu(rec->e_cpos));

		ocfs2_journal_dirty(handle, path->p_node[i].bh);
	}
out:
	return ret;
}

static void ocfs2_unlink_path(handle_t *handle,
			      struct ocfs2_extent_tree *et,
			      struct ocfs2_cached_dealloc_ctxt *dealloc,
			      struct ocfs2_path *path, int unlink_start)
{
	int ret, i;
	struct ocfs2_extent_block *eb;
	struct ocfs2_extent_list *el;
	struct buffer_head *bh;

	for(i = unlink_start; i < path_num_items(path); i++) {
		bh = path->p_node[i].bh;

		eb = (struct ocfs2_extent_block *)bh->b_data;
		/*
		 * Not all nodes might have had their final count
		 * decremented by the caller - handle this here.
		 */
		el = &eb->h_list;
		if (le16_to_cpu(el->l_next_free_rec) > 1) {
			mlog(ML_ERROR,
			     "Inode %llu, attempted to remove extent block "
			     "%llu with %u records\n",
			     (unsigned long long)ocfs2_metadata_cache_owner(et->et_ci),
			     (unsigned long long)le64_to_cpu(eb->h_blkno),
			     le16_to_cpu(el->l_next_free_rec));

			ocfs2_journal_dirty(handle, bh);
			ocfs2_remove_from_cache(et->et_ci, bh);
			continue;
		}

		el->l_next_free_rec = 0;
		memset(&el->l_recs[0], 0, sizeof(struct ocfs2_extent_rec));

		ocfs2_journal_dirty(handle, bh);

		ret = ocfs2_cache_extent_block_free(dealloc, eb);
		if (ret)
			mlog_errno(ret);

		ocfs2_remove_from_cache(et->et_ci, bh);
	}
}

static void ocfs2_unlink_subtree(handle_t *handle,
				 struct ocfs2_extent_tree *et,
				 struct ocfs2_path *left_path,
				 struct ocfs2_path *right_path,
				 int subtree_index,
				 struct ocfs2_cached_dealloc_ctxt *dealloc)
{
	int i;
	struct buffer_head *root_bh = left_path->p_node[subtree_index].bh;
	struct ocfs2_extent_list *root_el = left_path->p_node[subtree_index].el;
	struct ocfs2_extent_list *el;
	struct ocfs2_extent_block *eb;

	el = path_leaf_el(left_path);

	eb = (struct ocfs2_extent_block *)right_path->p_node[subtree_index + 1].bh->b_data;

	for(i = 1; i < le16_to_cpu(root_el->l_next_free_rec); i++)
		if (root_el->l_recs[i].e_blkno == eb->h_blkno)
			break;

	BUG_ON(i >= le16_to_cpu(root_el->l_next_free_rec));

	memset(&root_el->l_recs[i], 0, sizeof(struct ocfs2_extent_rec));
	le16_add_cpu(&root_el->l_next_free_rec, -1);

	eb = (struct ocfs2_extent_block *)path_leaf_bh(left_path)->b_data;
	eb->h_next_leaf_blk = 0;

	ocfs2_journal_dirty(handle, root_bh);
	ocfs2_journal_dirty(handle, path_leaf_bh(left_path));

	ocfs2_unlink_path(handle, et, dealloc, right_path,
			  subtree_index + 1);
}

static int ocfs2_rotate_subtree_left(handle_t *handle,
				     struct ocfs2_extent_tree *et,
				     struct ocfs2_path *left_path,
				     struct ocfs2_path *right_path,
				     int subtree_index,
				     struct ocfs2_cached_dealloc_ctxt *dealloc,
				     int *deleted)
{
	int ret, i, del_right_subtree = 0, right_has_empty = 0;
	struct buffer_head *root_bh, *et_root_bh = path_root_bh(right_path);
	struct ocfs2_extent_list *right_leaf_el, *left_leaf_el;
	struct ocfs2_extent_block *eb;

	*deleted = 0;

	right_leaf_el = path_leaf_el(right_path);
	left_leaf_el = path_leaf_el(left_path);
	root_bh = left_path->p_node[subtree_index].bh;
	BUG_ON(root_bh != right_path->p_node[subtree_index].bh);

	if (!ocfs2_is_empty_extent(&left_leaf_el->l_recs[0]))
		return 0;

	eb = (struct ocfs2_extent_block *)path_leaf_bh(right_path)->b_data;
	if (ocfs2_is_empty_extent(&right_leaf_el->l_recs[0])) {
		/*
		 * It's legal for us to proceed if the right leaf is
		 * the rightmost one and it has an empty extent. There
		 * are two cases to handle - whether the leaf will be
		 * empty after removal or not. If the leaf isn't empty
		 * then just remove the empty extent up front. The
		 * next block will handle empty leaves by flagging
		 * them for unlink.
		 *
		 * Non rightmost leaves will throw -EAGAIN and the
		 * caller can manually move the subtree and retry.
		 */

		if (eb->h_next_leaf_blk != 0ULL)
			return -EAGAIN;

		if (le16_to_cpu(right_leaf_el->l_next_free_rec) > 1) {
			ret = ocfs2_journal_access_eb(handle, et->et_ci,
						      path_leaf_bh(right_path),
						      OCFS2_JOURNAL_ACCESS_WRITE);
			if (ret) {
				mlog_errno(ret);
				goto out;
			}

			ocfs2_remove_empty_extent(right_leaf_el);
		} else
			right_has_empty = 1;
	}

	if (eb->h_next_leaf_blk == 0ULL &&
	    le16_to_cpu(right_leaf_el->l_next_free_rec) == 1) {
		/*
		 * We have to update i_last_eb_blk during the meta
		 * data delete.
		 */
		ret = ocfs2_et_root_journal_access(handle, et,
						   OCFS2_JOURNAL_ACCESS_WRITE);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}

		del_right_subtree = 1;
	}

	/*
	 * Getting here with an empty extent in the right path implies
	 * that it's the rightmost path and will be deleted.
	 */
	BUG_ON(right_has_empty && !del_right_subtree);

	ret = ocfs2_path_bh_journal_access(handle, et->et_ci, right_path,
					   subtree_index);
	if (ret) {
		mlog_errno(ret);
		goto out;
	}

	for(i = subtree_index + 1; i < path_num_items(right_path); i++) {
		ret = ocfs2_path_bh_journal_access(handle, et->et_ci,
						   right_path, i);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}

		ret = ocfs2_path_bh_journal_access(handle, et->et_ci,
						   left_path, i);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}
	}

	if (!right_has_empty) {
		/*
		 * Only do this if we're moving a real
		 * record. Otherwise, the action is delayed until
		 * after removal of the right path in which case we
		 * can do a simple shift to remove the empty extent.
		 */
		ocfs2_rotate_leaf(left_leaf_el, &right_leaf_el->l_recs[0]);
		memset(&right_leaf_el->l_recs[0], 0,
		       sizeof(struct ocfs2_extent_rec));
	}
	if (eb->h_next_leaf_blk == 0ULL) {
		/*
		 * Move recs over to get rid of empty extent, decrease
		 * next_free. This is allowed to remove the last
		 * extent in our leaf (setting l_next_free_rec to
		 * zero) - the delete code below won't care.
		 */
		ocfs2_remove_empty_extent(right_leaf_el);
	}

	ret = ocfs2_journal_dirty(handle, path_leaf_bh(left_path));
	if (ret)
		mlog_errno(ret);
	ret = ocfs2_journal_dirty(handle, path_leaf_bh(right_path));
	if (ret)
		mlog_errno(ret);

	if (del_right_subtree) {
		ocfs2_unlink_subtree(handle, et, left_path, right_path,
				     subtree_index, dealloc);
		ret = ocfs2_update_edge_lengths(handle, et, subtree_index,
						left_path);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}

		eb = (struct ocfs2_extent_block *)path_leaf_bh(left_path)->b_data;
		ocfs2_et_set_last_eb_blk(et, le64_to_cpu(eb->h_blkno));

		/*
		 * Removal of the extent in the left leaf was skipped
		 * above so we could delete the right path
		 * 1st.
		 */
		if (right_has_empty)
			ocfs2_remove_empty_extent(left_leaf_el);

		ret = ocfs2_journal_dirty(handle, et_root_bh);
		if (ret)
			mlog_errno(ret);

		*deleted = 1;
	} else
		ocfs2_complete_edge_insert(handle, left_path, right_path,
					   subtree_index);

out:
	return ret;
}

/*
 * Given a full path, determine what cpos value would return us a path
 * containing the leaf immediately to the right of the current one.
 *
 * Will return zero if the path passed in is already the rightmost path.
 *
 * This looks similar, but is subtly different to
 * ocfs2_find_cpos_for_left_leaf().
 */
static int ocfs2_find_cpos_for_right_leaf(struct super_block *sb,
					  struct ocfs2_path *path, u32 *cpos)
{
	int i, j, ret = 0;
	u64 blkno;
	struct ocfs2_extent_list *el;

	*cpos = 0;

	if (path->p_tree_depth == 0)
		return 0;

	blkno = path_leaf_bh(path)->b_blocknr;

	/* Start at the tree node just above the leaf and work our way up. */
	i = path->p_tree_depth - 1;
	while (i >= 0) {
		int next_free;

		el = path->p_node[i].el;

		/*
		 * Find the extent record just after the one in our
		 * path.
		 */
		next_free = le16_to_cpu(el->l_next_free_rec);
		for(j = 0; j < le16_to_cpu(el->l_next_free_rec); j++) {
			if (le64_to_cpu(el->l_recs[j].e_blkno) == blkno) {
				if (j == (next_free - 1)) {
					if (i == 0) {
						/*
						 * We've determined that the
						 * path specified is already
						 * the rightmost one - return a
						 * cpos of zero.
						 */
						goto out;
					}
					/*
					 * The rightmost record points to our
					 * leaf - we need to travel up the
					 * tree one level.
					 */
					goto next_node;
				}

				*cpos = le32_to_cpu(el->l_recs[j + 1].e_cpos);
				goto out;
			}
		}

		/*
		 * If we got here, we never found a valid node where
		 * the tree indicated one should be.
		 */
		ocfs2_error(sb,
			    "Invalid extent tree at extent block %llu\n",
			    (unsigned long long)blkno);
		ret = -EROFS;
		goto out;

next_node:
		blkno = path->p_node[i].bh->b_blocknr;
		i--;
	}

out:
	return ret;
}

static int ocfs2_rotate_rightmost_leaf_left(handle_t *handle,
					    struct ocfs2_extent_tree *et,
					    struct ocfs2_path *path)
{
	int ret;
	struct buffer_head *bh = path_leaf_bh(path);
	struct ocfs2_extent_list *el = path_leaf_el(path);

	if (!ocfs2_is_empty_extent(&el->l_recs[0]))
		return 0;

	ret = ocfs2_path_bh_journal_access(handle, et->et_ci, path,
					   path_num_items(path) - 1);
	if (ret) {
		mlog_errno(ret);
		goto out;
	}

	ocfs2_remove_empty_extent(el);

	ret = ocfs2_journal_dirty(handle, bh);
	if (ret)
		mlog_errno(ret);

out:
	return ret;
}

static int __ocfs2_rotate_tree_left(handle_t *handle,
				    struct ocfs2_extent_tree *et,
				    int orig_credits,
				    struct ocfs2_path *path,
				    struct ocfs2_cached_dealloc_ctxt *dealloc,
				    struct ocfs2_path **empty_extent_path)
{
	int ret, subtree_root, deleted;
	u32 right_cpos;
	struct ocfs2_path *left_path = NULL;
	struct ocfs2_path *right_path = NULL;
	struct super_block *sb = ocfs2_metadata_cache_get_super(et->et_ci);

	BUG_ON(!ocfs2_is_empty_extent(&(path_leaf_el(path)->l_recs[0])));

	*empty_extent_path = NULL;

	ret = ocfs2_find_cpos_for_right_leaf(sb, path, &right_cpos);
	if (ret) {
		mlog_errno(ret);
		goto out;
	}

	left_path = ocfs2_new_path_from_path(path);
	if (!left_path) {
		ret = -ENOMEM;
		mlog_errno(ret);
		goto out;
	}

	ocfs2_cp_path(left_path, path);

	right_path = ocfs2_new_path_from_path(path);
	if (!right_path) {
		ret = -ENOMEM;
		mlog_errno(ret);
		goto out;
	}

	while (right_cpos) {
		ret = ocfs2_find_path(et->et_ci, right_path, right_cpos);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}

		subtree_root = ocfs2_find_subtree_root(et, left_path,
						       right_path);

		mlog(0, "Subtree root at index %d (blk %llu, depth %d)\n",
		     subtree_root,
		     (unsigned long long)
		     right_path->p_node[subtree_root].bh->b_blocknr,
		     right_path->p_tree_depth);

		ret = ocfs2_extend_rotate_transaction(handle, subtree_root,
						      orig_credits, left_path);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}

		/*
		 * Caller might still want to make changes to the
		 * tree root, so re-add it to the journal here.
		 */
		ret = ocfs2_path_bh_journal_access(handle, et->et_ci,
						   left_path, 0);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}

		ret = ocfs2_rotate_subtree_left(handle, et, left_path,
						right_path, subtree_root,
						dealloc, &deleted);
		if (ret == -EAGAIN) {
			/*
			 * The rotation has to temporarily stop due to
			 * the right subtree having an empty
			 * extent. Pass it back to the caller for a
			 * fixup.
			 */
			*empty_extent_path = right_path;
			right_path = NULL;
			goto out;
		}
		if (ret) {
			mlog_errno(ret);
			goto out;
		}

		/*
		 * The subtree rotate might have removed records on
		 * the rightmost edge. If so, then rotation is
		 * complete.
		 */
		if (deleted)
			break;

		ocfs2_mv_path(left_path, right_path);

		ret = ocfs2_find_cpos_for_right_leaf(sb, left_path,
						     &right_cpos);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}
	}

out:
	ocfs2_free_path(right_path);
	ocfs2_free_path(left_path);

	return ret;
}

static int ocfs2_remove_rightmost_path(handle_t *handle,
				struct ocfs2_extent_tree *et,
				struct ocfs2_path *path,
				struct ocfs2_cached_dealloc_ctxt *dealloc)
{
	int ret, subtree_index;
	u32 cpos;
	struct ocfs2_path *left_path = NULL;
	struct ocfs2_extent_block *eb;
	struct ocfs2_extent_list *el;


	ret = ocfs2_et_sanity_check(et);
	if (ret)
		goto out;
	/*
	 * There's two ways we handle this depending on
	 * whether path is the only existing one.
	 */
	ret = ocfs2_extend_rotate_transaction(handle, 0,
					      handle->h_buffer_credits,
					      path);
	if (ret) {
		mlog_errno(ret);
		goto out;
	}

	ret = ocfs2_journal_access_path(et->et_ci, handle, path);
	if (ret) {
		mlog_errno(ret);
		goto out;
	}

	ret = ocfs2_find_cpos_for_left_leaf(ocfs2_metadata_cache_get_super(et->et_ci),
					    path, &cpos);
	if (ret) {
		mlog_errno(ret);
		goto out;
	}

	if (cpos) {
		/*
		 * We have a path to the left of this one - it needs
		 * an update too.
		 */
		left_path = ocfs2_new_path_from_path(path);
		if (!left_path) {
			ret = -ENOMEM;
			mlog_errno(ret);
			goto out;
		}

		ret = ocfs2_find_path(et->et_ci, left_path, cpos);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}

		ret = ocfs2_journal_access_path(et->et_ci, handle, left_path);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}

		subtree_index = ocfs2_find_subtree_root(et, left_path, path);

		ocfs2_unlink_subtree(handle, et, left_path, path,
				     subtree_index, dealloc);
		ret = ocfs2_update_edge_lengths(handle, et, subtree_index,
						left_path);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}

		eb = (struct ocfs2_extent_block *)path_leaf_bh(left_path)->b_data;
		ocfs2_et_set_last_eb_blk(et, le64_to_cpu(eb->h_blkno));
	} else {
		/*
		 * 'path' is also the leftmost path which
		 * means it must be the only one. This gets
		 * handled differently because we want to
		 * revert the root back to having extents
		 * in-line.
		 */
		ocfs2_unlink_path(handle, et, dealloc, path, 1);

		el = et->et_root_el;
		el->l_tree_depth = 0;
		el->l_next_free_rec = 0;
		memset(&el->l_recs[0], 0, sizeof(struct ocfs2_extent_rec));

		ocfs2_et_set_last_eb_blk(et, 0);
	}

	ocfs2_journal_dirty(handle, path_root_bh(path));

out:
	ocfs2_free_path(left_path);
	return ret;
}

/*
 * Left rotation of btree records.
 *
 * In many ways, this is (unsurprisingly) the opposite of right
 * rotation. We start at some non-rightmost path containing an empty
 * extent in the leaf block. The code works its way to the rightmost
 * path by rotating records to the left in every subtree.
 *
 * This is used by any code which reduces the number of extent records
 * in a leaf. After removal, an empty record should be placed in the
 * leftmost list position.
 *
 * This won't handle a length update of the rightmost path records if
 * the rightmost tree leaf record is removed so the caller is
 * responsible for detecting and correcting that.
 */
static int ocfs2_rotate_tree_left(handle_t *handle,
				  struct ocfs2_extent_tree *et,
				  struct ocfs2_path *path,
				  struct ocfs2_cached_dealloc_ctxt *dealloc)
{
	int ret, orig_credits = handle->h_buffer_credits;
	struct ocfs2_path *tmp_path = NULL, *restart_path = NULL;
	struct ocfs2_extent_block *eb;
	struct ocfs2_extent_list *el;

	el = path_leaf_el(path);
	if (!ocfs2_is_empty_extent(&el->l_recs[0]))
		return 0;

	if (path->p_tree_depth == 0) {
rightmost_no_delete:
		/*
		 * Inline extents. This is trivially handled, so do
		 * it up front.
		 */
		ret = ocfs2_rotate_rightmost_leaf_left(handle, et, path);
		if (ret)
			mlog_errno(ret);
		goto out;
	}

	/*
	 * Handle rightmost branch now. There's several cases:
	 *  1) simple rotation leaving records in there. That's trivial.
	 *  2) rotation requiring a branch delete - there's no more
	 *     records left. Two cases of this:
	 *     a) There are branches to the left.
	 *     b) This is also the leftmost (the only) branch.
	 *
	 *  1) is handled via ocfs2_rotate_rightmost_leaf_left()
	 *  2a) we need the left branch so that we can update it with the unlink
	 *  2b) we need to bring the root back to inline extents.
	 */

	eb = (struct ocfs2_extent_block *)path_leaf_bh(path)->b_data;
	el = &eb->h_list;
	if (eb->h_next_leaf_blk == 0) {
		/*
		 * This gets a bit tricky if we're going to delete the
		 * rightmost path. Get the other cases out of the way
		 * 1st.
		 */
		if (le16_to_cpu(el->l_next_free_rec) > 1)
			goto rightmost_no_delete;

		if (le16_to_cpu(el->l_next_free_rec) == 0) {
			ret = -EIO;
			ocfs2_error(ocfs2_metadata_cache_get_super(et->et_ci),
				    "Owner %llu has empty extent block at %llu",
				    (unsigned long long)ocfs2_metadata_cache_owner(et->et_ci),
				    (unsigned long long)le64_to_cpu(eb->h_blkno));
			goto out;
		}

		/*
		 * XXX: The caller can not trust "path" any more after
		 * this as it will have been deleted. What do we do?
		 *
		 * In theory the rotate-for-merge code will never get
		 * here because it'll always ask for a rotate in a
		 * nonempty list.
		 */

		ret = ocfs2_remove_rightmost_path(handle, et, path,
						  dealloc);
		if (ret)
			mlog_errno(ret);
		goto out;
	}

	/*
	 * Now we can loop, remembering the path we get from -EAGAIN
	 * and restarting from there.
	 */
try_rotate:
	ret = __ocfs2_rotate_tree_left(handle, et, orig_credits, path,
				       dealloc, &restart_path);
	if (ret && ret != -EAGAIN) {
		mlog_errno(ret);
		goto out;
	}

	while (ret == -EAGAIN) {
		tmp_path = restart_path;
		restart_path = NULL;

		ret = __ocfs2_rotate_tree_left(handle, et, orig_credits,
					       tmp_path, dealloc,
					       &restart_path);
		if (ret && ret != -EAGAIN) {
			mlog_errno(ret);
			goto out;
		}

		ocfs2_free_path(tmp_path);
		tmp_path = NULL;

		if (ret == 0)
			goto try_rotate;
	}

out:
	ocfs2_free_path(tmp_path);
	ocfs2_free_path(restart_path);
	return ret;
}

static void ocfs2_cleanup_merge(struct ocfs2_extent_list *el,
				int index)
{
	struct ocfs2_extent_rec *rec = &el->l_recs[index];
	unsigned int size;

	if (rec->e_leaf_clusters == 0) {
		/*
		 * We consumed all of the merged-from record. An empty
		 * extent cannot exist anywhere but the 1st array
		 * position, so move things over if the merged-from
		 * record doesn't occupy that position.
		 *
		 * This creates a new empty extent so the caller
		 * should be smart enough to have removed any existing
		 * ones.
		 */
		if (index > 0) {
			BUG_ON(ocfs2_is_empty_extent(&el->l_recs[0]));
			size = index * sizeof(struct ocfs2_extent_rec);
			memmove(&el->l_recs[1], &el->l_recs[0], size);
		}

		/*
		 * Always memset - the caller doesn't check whether it
		 * created an empty extent, so there could be junk in
		 * the other fields.
		 */
		memset(&el->l_recs[0], 0, sizeof(struct ocfs2_extent_rec));
	}
}

static int ocfs2_get_right_path(struct ocfs2_extent_tree *et,
				struct ocfs2_path *left_path,
				struct ocfs2_path **ret_right_path)
{
	int ret;
	u32 right_cpos;
	struct ocfs2_path *right_path = NULL;
	struct ocfs2_extent_list *left_el;

	*ret_right_path = NULL;

	/* This function shouldn't be called for non-trees. */
	BUG_ON(left_path->p_tree_depth == 0);

	left_el = path_leaf_el(left_path);
	BUG_ON(left_el->l_next_free_rec != left_el->l_count);

	ret = ocfs2_find_cpos_for_right_leaf(ocfs2_metadata_cache_get_super(et->et_ci),
					     left_path, &right_cpos);
	if (ret) {
		mlog_errno(ret);
		goto out;
	}

	/* This function shouldn't be called for the rightmost leaf. */
	BUG_ON(right_cpos == 0);

	right_path = ocfs2_new_path_from_path(left_path);
	if (!right_path) {
		ret = -ENOMEM;
		mlog_errno(ret);
		goto out;
	}

	ret = ocfs2_find_path(et->et_ci, right_path, right_cpos);
	if (ret) {
		mlog_errno(ret);
		goto out;
	}

	*ret_right_path = right_path;
out:
	if (ret)
		ocfs2_free_path(right_path);
	return ret;
}

/*
 * Remove split_rec clusters from the record at index and merge them
 * onto the beginning of the record "next" to it.
 * For index < l_count - 1, the next means the extent rec at index + 1.
 * For index == l_count - 1, the "next" means the 1st extent rec of the
 * next extent block.
 */
static int ocfs2_merge_rec_right(struct ocfs2_path *left_path,
				 handle_t *handle,
				 struct ocfs2_extent_tree *et,
				 struct ocfs2_extent_rec *split_rec,
				 int index)
{
	int ret, next_free, i;
	unsigned int split_clusters = le16_to_cpu(split_rec->e_leaf_clusters);
	struct ocfs2_extent_rec *left_rec;
	struct ocfs2_extent_rec *right_rec;
	struct ocfs2_extent_list *right_el;
	struct ocfs2_path *right_path = NULL;
	int subtree_index = 0;
	struct ocfs2_extent_list *el = path_leaf_el(left_path);
	struct buffer_head *bh = path_leaf_bh(left_path);
	struct buffer_head *root_bh = NULL;

	BUG_ON(index >= le16_to_cpu(el->l_next_free_rec));
	left_rec = &el->l_recs[index];

	if (index == le16_to_cpu(el->l_next_free_rec) - 1 &&
	    le16_to_cpu(el->l_next_free_rec) == le16_to_cpu(el->l_count)) {
		/* we meet with a cross extent block merge. */
		ret = ocfs2_get_right_path(et, left_path, &right_path);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}

		right_el = path_leaf_el(right_path);
		next_free = le16_to_cpu(right_el->l_next_free_rec);
		BUG_ON(next_free <= 0);
		right_rec = &right_el->l_recs[0];
		if (ocfs2_is_empty_extent(right_rec)) {
			BUG_ON(next_free <= 1);
			right_rec = &right_el->l_recs[1];
		}

		BUG_ON(le32_to_cpu(left_rec->e_cpos) +
		       le16_to_cpu(left_rec->e_leaf_clusters) !=
		       le32_to_cpu(right_rec->e_cpos));

		subtree_index = ocfs2_find_subtree_root(et, left_path,
							right_path);

		ret = ocfs2_extend_rotate_transaction(handle, subtree_index,
						      handle->h_buffer_credits,
						      right_path);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}

		root_bh = left_path->p_node[subtree_index].bh;
		BUG_ON(root_bh != right_path->p_node[subtree_index].bh);

		ret = ocfs2_path_bh_journal_access(handle, et->et_ci, right_path,
						   subtree_index);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}

		for (i = subtree_index + 1;
		     i < path_num_items(right_path); i++) {
			ret = ocfs2_path_bh_journal_access(handle, et->et_ci,
							   right_path, i);
			if (ret) {
				mlog_errno(ret);
				goto out;
			}

			ret = ocfs2_path_bh_journal_access(handle, et->et_ci,
							   left_path, i);
			if (ret) {
				mlog_errno(ret);
				goto out;
			}
		}

	} else {
		BUG_ON(index == le16_to_cpu(el->l_next_free_rec) - 1);
		right_rec = &el->l_recs[index + 1];
	}

	ret = ocfs2_path_bh_journal_access(handle, et->et_ci, left_path,
					   path_num_items(left_path) - 1);
	if (ret) {
		mlog_errno(ret);
		goto out;
	}

	le16_add_cpu(&left_rec->e_leaf_clusters, -split_clusters);

	le32_add_cpu(&right_rec->e_cpos, -split_clusters);
	le64_add_cpu(&right_rec->e_blkno,
		     -ocfs2_clusters_to_blocks(ocfs2_metadata_cache_get_super(et->et_ci),
					       split_clusters));
	le16_add_cpu(&right_rec->e_leaf_clusters, split_clusters);

	ocfs2_cleanup_merge(el, index);

	ret = ocfs2_journal_dirty(handle, bh);
	if (ret)
		mlog_errno(ret);

	if (right_path) {
		ret = ocfs2_journal_dirty(handle, path_leaf_bh(right_path));
		if (ret)
			mlog_errno(ret);

		ocfs2_complete_edge_insert(handle, left_path, right_path,
					   subtree_index);
	}
out:
	if (right_path)
		ocfs2_free_path(right_path);
	return ret;
}

static int ocfs2_get_left_path(struct ocfs2_extent_tree *et,
			       struct ocfs2_path *right_path,
			       struct ocfs2_path **ret_left_path)
{
	int ret;
	u32 left_cpos;
	struct ocfs2_path *left_path = NULL;

	*ret_left_path = NULL;

	/* This function shouldn't be called for non-trees. */
	BUG_ON(right_path->p_tree_depth == 0);

	ret = ocfs2_find_cpos_for_left_leaf(ocfs2_metadata_cache_get_super(et->et_ci),
					    right_path, &left_cpos);
	if (ret) {
		mlog_errno(ret);
		goto out;
	}

	/* This function shouldn't be called for the leftmost leaf. */
	BUG_ON(left_cpos == 0);

	left_path = ocfs2_new_path_from_path(right_path);
	if (!left_path) {
		ret = -ENOMEM;
		mlog_errno(ret);
		goto out;
	}

	ret = ocfs2_find_path(et->et_ci, left_path, left_cpos);
	if (ret) {
		mlog_errno(ret);
		goto out;
	}

	*ret_left_path = left_path;
out:
	if (ret)
		ocfs2_free_path(left_path);
	return ret;
}

/*
 * Remove split_rec clusters from the record at index and merge them
 * onto the tail of the record "before" it.
 * For index > 0, the "before" means the extent rec at index - 1.
 *
 * For index == 0, the "before" means the last record of the previous
 * extent block. And there is also a situation that we may need to
 * remove the rightmost leaf extent block in the right_path and change
 * the right path to indicate the new rightmost path.
 */
static int ocfs2_merge_rec_left(struct ocfs2_path *right_path,
				handle_t *handle,
				struct ocfs2_extent_tree *et,
				struct ocfs2_extent_rec *split_rec,
				struct ocfs2_cached_dealloc_ctxt *dealloc,
				int index)
{
	int ret, i, subtree_index = 0, has_empty_extent = 0;
	unsigned int split_clusters = le16_to_cpu(split_rec->e_leaf_clusters);
	struct ocfs2_extent_rec *left_rec;
	struct ocfs2_extent_rec *right_rec;
	struct ocfs2_extent_list *el = path_leaf_el(right_path);
	struct buffer_head *bh = path_leaf_bh(right_path);
	struct buffer_head *root_bh = NULL;
	struct ocfs2_path *left_path = NULL;
	struct ocfs2_extent_list *left_el;

	BUG_ON(index < 0);

	right_rec = &el->l_recs[index];
	if (index == 0) {
		/* we meet with a cross extent block merge. */
		ret = ocfs2_get_left_path(et, right_path, &left_path);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}

		left_el = path_leaf_el(left_path);
		BUG_ON(le16_to_cpu(left_el->l_next_free_rec) !=
		       le16_to_cpu(left_el->l_count));

		left_rec = &left_el->l_recs[
				le16_to_cpu(left_el->l_next_free_rec) - 1];
		BUG_ON(le32_to_cpu(left_rec->e_cpos) +
		       le16_to_cpu(left_rec->e_leaf_clusters) !=
		       le32_to_cpu(split_rec->e_cpos));

		subtree_index = ocfs2_find_subtree_root(et, left_path,
							right_path);

		ret = ocfs2_extend_rotate_transaction(handle, subtree_index,
						      handle->h_buffer_credits,
						      left_path);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}

		root_bh = left_path->p_node[subtree_index].bh;
		BUG_ON(root_bh != right_path->p_node[subtree_index].bh);

		ret = ocfs2_path_bh_journal_access(handle, et->et_ci, right_path,
						   subtree_index);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}

		for (i = subtree_index + 1;
		     i < path_num_items(right_path); i++) {
			ret = ocfs2_path_bh_journal_access(handle, et->et_ci,
							   right_path, i);
			if (ret) {
				mlog_errno(ret);
				goto out;
			}

			ret = ocfs2_path_bh_journal_access(handle, et->et_ci,
							   left_path, i);
			if (ret) {
				mlog_errno(ret);
				goto out;
			}
		}
	} else {
		left_rec = &el->l_recs[index - 1];
		if (ocfs2_is_empty_extent(&el->l_recs[0]))
			has_empty_extent = 1;
	}

	ret = ocfs2_path_bh_journal_access(handle, et->et_ci, right_path,
					   path_num_items(right_path) - 1);
	if (ret) {
		mlog_errno(ret);
		goto out;
	}

	if (has_empty_extent && index == 1) {
		/*
		 * The easy case - we can just plop the record right in.
		 */
		*left_rec = *split_rec;

		has_empty_extent = 0;
	} else
		le16_add_cpu(&left_rec->e_leaf_clusters, split_clusters);

	le32_add_cpu(&right_rec->e_cpos, split_clusters);
	le64_add_cpu(&right_rec->e_blkno,
		     ocfs2_clusters_to_blocks(ocfs2_metadata_cache_get_super(et->et_ci),
					      split_clusters));
	le16_add_cpu(&right_rec->e_leaf_clusters, -split_clusters);

	ocfs2_cleanup_merge(el, index);

	ret = ocfs2_journal_dirty(handle, bh);
	if (ret)
		mlog_errno(ret);

	if (left_path) {
		ret = ocfs2_journal_dirty(handle, path_leaf_bh(left_path));
		if (ret)
			mlog_errno(ret);

		/*
		 * In the situation that the right_rec is empty and the extent
		 * block is empty also,  ocfs2_complete_edge_insert can't handle
		 * it and we need to delete the right extent block.
		 */
		if (le16_to_cpu(right_rec->e_leaf_clusters) == 0 &&
		    le16_to_cpu(el->l_next_free_rec) == 1) {

			ret = ocfs2_remove_rightmost_path(handle, et,
							  right_path,
							  dealloc);
			if (ret) {
				mlog_errno(ret);
				goto out;
			}

			/* Now the rightmost extent block has been deleted.
			 * So we use the new rightmost path.
			 */
			ocfs2_mv_path(right_path, left_path);
			left_path = NULL;
		} else
			ocfs2_complete_edge_insert(handle, left_path,
						   right_path, subtree_index);
	}
out:
	if (left_path)
		ocfs2_free_path(left_path);
	return ret;
}

static int ocfs2_try_to_merge_extent(handle_t *handle,
				     struct ocfs2_extent_tree *et,
				     struct ocfs2_path *path,
				     int split_index,
				     struct ocfs2_extent_rec *split_rec,
				     struct ocfs2_cached_dealloc_ctxt *dealloc,
				     struct ocfs2_merge_ctxt *ctxt)
{
	int ret = 0;
	struct ocfs2_extent_list *el = path_leaf_el(path);
	struct ocfs2_extent_rec *rec = &el->l_recs[split_index];

	BUG_ON(ctxt->c_contig_type == CONTIG_NONE);

	if (ctxt->c_split_covers_rec && ctxt->c_has_empty_extent) {
		/*
		 * The merge code will need to create an empty
		 * extent to take the place of the newly
		 * emptied slot. Remove any pre-existing empty
		 * extents - having more than one in a leaf is
		 * illegal.
		 */
		ret = ocfs2_rotate_tree_left(handle, et, path, dealloc);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}
		split_index--;
		rec = &el->l_recs[split_index];
	}

	if (ctxt->c_contig_type == CONTIG_LEFTRIGHT) {
		/*
		 * Left-right contig implies this.
		 */
		BUG_ON(!ctxt->c_split_covers_rec);

		/*
		 * Since the leftright insert always covers the entire
		 * extent, this call will delete the insert record
		 * entirely, resulting in an empty extent record added to
		 * the extent block.
		 *
		 * Since the adding of an empty extent shifts
		 * everything back to the right, there's no need to
		 * update split_index here.
		 *
		 * When the split_index is zero, we need to merge it to the
		 * prevoius extent block. It is more efficient and easier
		 * if we do merge_right first and merge_left later.
		 */
		ret = ocfs2_merge_rec_right(path, handle, et, split_rec,
					    split_index);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}

		/*
		 * We can only get this from logic error above.
		 */
		BUG_ON(!ocfs2_is_empty_extent(&el->l_recs[0]));

		/* The merge left us with an empty extent, remove it. */
		ret = ocfs2_rotate_tree_left(handle, et, path, dealloc);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}

		rec = &el->l_recs[split_index];

		/*
		 * Note that we don't pass split_rec here on purpose -
		 * we've merged it into the rec already.
		 */
		ret = ocfs2_merge_rec_left(path, handle, et, rec,
					   dealloc, split_index);

		if (ret) {
			mlog_errno(ret);
			goto out;
		}

		ret = ocfs2_rotate_tree_left(handle, et, path, dealloc);
		/*
		 * Error from this last rotate is not critical, so
		 * print but don't bubble it up.
		 */
		if (ret)
			mlog_errno(ret);
		ret = 0;
	} else {
		/*
		 * Merge a record to the left or right.
		 *
		 * 'contig_type' is relative to the existing record,
		 * so for example, if we're "right contig", it's to
		 * the record on the left (hence the left merge).
		 */
		if (ctxt->c_contig_type == CONTIG_RIGHT) {
			ret = ocfs2_merge_rec_left(path, handle, et,
						   split_rec, dealloc,
						   split_index);
			if (ret) {
				mlog_errno(ret);
				goto out;
			}
		} else {
			ret = ocfs2_merge_rec_right(path, handle,
						    et, split_rec,
						    split_index);
			if (ret) {
				mlog_errno(ret);
				goto out;
			}
		}

		if (ctxt->c_split_covers_rec) {
			/*
			 * The merge may have left an empty extent in
			 * our leaf. Try to rotate it away.
			 */
			ret = ocfs2_rotate_tree_left(handle, et, path,
						     dealloc);
			if (ret)
				mlog_errno(ret);
			ret = 0;
		}
	}

out:
	return ret;
}

static void ocfs2_subtract_from_rec(struct super_block *sb,
				    enum ocfs2_split_type c-off* -*- modestruct c-basiextent_rec *rec
 * vim: noexpandtab sw=8 ts=8 sts=c-offsrec)
{
	u64 len_blocks;

	yright (C) = c-basiclusters_toght (C)(s/* -*- le16 rescpu(nd frees
->e_leafll rights)) 200if is fre == SPLIT_LEFT) {
		/*
		 * Region is on the left edge ofhe GNexistingder threcord.der t/
This32_addram i& softwacpos* -*-mode is program is free software; you can redisThis64e Software Foundablkno,pyright (C) option16e Software Foundare; you can r; either
 -* version 2 of the License, or (at your opt} elsey it under the terms of the GNrigheneral Public
 * License as published by the Fdistributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without ev
}

/*
 * Dohe GNfinal bits Pub=8 ts= publis insertermsathe GNtarget re; of tlist. Iubliisif nos ofpart Puban allocaith ttree, its ofassumedof tthhis progree above has been prepared.
by tstatic void c-basiong wi_at hope(oexpandtab sw=8 ts=8 * Bo*e-
 * vimoexpandtab sw=8 ts=8 sts=/fs.h>
0:
 *
 * aoexpandtab sw=8 ts=8rite *elnclude <linux/swap.h/fs.h>
et: 8ghmem.h *
 *int i =long wi->ins_contig_index;
	unsignealont range;
	oexpandtab sw=8 ts=8 sts=0:
 200BUG_ON(is program iel->lnclud_depth) != 0distributOC
#include e it a!d/or
 * NONEfy it _ALLc-basicearchh>
#include (el * T32rogram ihmem.h>
#iundationr opti "aops.iand/-1 opti sts= &"blockrecs[i]optic-basicubtract_fromincl(c-basimetadata_cache_get_super(et->et_ci)* -*- 	e.h"
#include "ex, pub#include "refees
 optigoto rotatcfs2}

 unde * Cclustuouslong wi - eitherNU GenorHANTAB.ig_t/dlmglue.h"
#includ<clusttentCONTIGh"
#includesysfile.h"
#include "filHT,
	CONTIG_LEFTRIGHT,
=;

staticmodify it s2_csion.
 *
ALLOC
#inc.h"
#in.
 *
optistruct otionextent_rec *ext,
tionopti}E.  See the GNU
 * General Public License for mis program ient_rec *ext,
e, or (at your optireturncfs2_contig_tyHandleIG_NONE intoFounemptyif noNTIG_RIGHT,
h"
#include "blocknex "sueeees
 supe0 ||
ther
(ure and the matching
 * ocfs2_init_<t1) &&y>_ext>

#defis_ree_ow=8 ts=(e.h"
#includ0]))fy it  of the exten =ighmem.h>
#ioptiatching
 * ocfs2_in = cpu res See(lude "sy type in ocfs2, adAppendinglong wiNTIG_RIGHT,
	CONTIG_LEFTaht most l== APPEND_TAILnclude "inre and the matching
 * ocfs2_init- 1de "sysfile.h"
#include "file "ocextenclude "loc.h"
#includeruct+tent tree sto software; you can re"suballoc.hinclude "localalloc.h"
#include <de "ocdistr	mlog_bug_on_msgure and the matching
 * ocfs2_init>= * This program i"blockcount
#inclu"owner %llu, .h"
# %u, t_eb_tree g
 * ocfstree "k)(strrec.extenree cture rightson-disk strueaf exte usually total
	s track of\n"* -*- #include "uptodate.h"uct o "xattr.h"
#incluh"
#include "blockcheck.h"
#ihat value.  new_clusterst_eb_blk)(stre and the matching
 * ocfs2_inihe totalclude "loc.h"
#include .includehe total.  Required.
	 *cfs2_extenre; you can repdate_clusters)(stalalloc.h"
#includehe total.  Requirement an on-disk btree (extent trei++e_operationclude fs2_extent_tree_op See the GNU
 atching
 * ocfs2_in, the block number oenum o:contig_tyOk, w, MAve "

enum oNTIG_he rigis p Sofoint9 Templesafnt tr Placeuite 3ong witt leatent_ig_tyree_operat and -disk stru theextentxtentboth021110h
 * adig_tyston,supported Te
 * Freeneeds)(stxtentspace, 0,
	CONbyblic
 ee_op1stvoid ( * License a,LEFTby virtul Puban ing
 *  * l< ded to NTIG_RIGc-basienum oncludeh"
#"buffer_head_}
*/

#include <linuxadjust_ANTABmo,
		ubliss( *et,
_t * *et,
#includ: noexpandtab sw=8 ts=8clude <linux/s	int (*eo_sanity_path *
	/*cfs2_extent_tree *et);
e <linux/highmem.h>
#iX ML_DISKre map/*
	 * ocfsfs2.h"

#ibuffer_head *bhfs2.h"

#include "allocude <linfs2.h"

#include "alloc.h"
#includentig_tyUpdate everythers,excepis proxtentht (CNTIG_RIGfor h"
# 0; i < 
	/*->pkcheck.h"
#; i++fy it bh =	 * It isnode_exto ocf	el*eo_fill_root_el)(save

		g
 * ocfsextent tree store
	 * this value fo_extent_
	 * ->eo_flude
				s-------rror
#include "uptodate.h"
#include "xattr.h"
#inclu for"Oct ocfs2_ MA 0a badbefore rriteis exte_exteg.h>

#inlongeaf_c)nt tree.  This function updates
	 * tt_maxlock = -EIO
	 * ->etype is2_cos2_contig_type
	ocf
	 * ->eo_- 1]

	/*truct ointll rightsnt_rec *insert_rec);
/*
  Free Software Foundaor not. Optie type.
 *
 * To implement an on-disk btree (extent tret if use
	 * ocfs2_extent_rec as the tree l-() and
	 * ->eo_get_last_last_->eo_ec-basijournal_dirtycfs2_ex, bht_max_learett_eb_eb_blerrno_rec)

	/a cop/

#inccludt_tree ht mostru res
	/*cfs2_extent_rec *rec);
m: noexpandtab sw=8 ts=8clude <linux/sl: noexpandtab sw=8 ts=8 sts=ent_rec *e
 * vim: noexpandtab sw
	/*
	ANTAB ocfstree *et);
static void ocfs2_*reinclfde_set--------------	 * The remaining ant_tree and don't have
	 * accessor f
	/*
	ree *et,
 = NULL

	/ent_tree *et,
et,
					 untig_tyrs);
shouldn't hso we t->enon- * Bs. Tic
 ** Licens  storedig_ty*et);
manipuln, Incbelow only worksert(sinterit(strdesNTIG_RIG "aops.dinode_sett is required.s if
	void ocfs2_If ourp_truncate) ocfs2_esthis proree     neral Pubruct o,ig_tythenis emNTABI	 * )(stu_fill_ERCHANTABinodetruct o Publicig_tyneighborers,
	/*NTIG_RIGocfs2_ext
	/*
	eluncate(strut_ma
	 * ->eo_fill_max_leaf_clusters sets et->et_ma_leaf_clusters if
ingy>_extef_clusters if1 &&r the allocation portion of the extent tree.
u32fs2_dec);
/*c *ext,
				   finoftwos_fortree *clude#include "uptodate.h"
#include "xattr.h"
#includode_gdinode_set_ &_ops = {
 *insert_rec)
				s


/*
 * Pre-decl	io.h"

outst whetheeb_b(0, "ght mo may_tree ructftt ocft,
				.  ocf:on-disk s* for"_ops = {
extethis#include "localalloc.h"
#include* -*her
 * o_update_cit under thNo_tree *etworry iublic
so we tic ilready ithe Gder ths2_dinodeeration by theuctureo_update
				susters);
stac-basinews2_ex"superit asuct ocfs2_extefs2_di!ree *et,
	
				s ->eo_extNOMEMstruc_dinode_update_clusteers,
	.eo_extwhethe_set_last_eb_blk	=it as"xattr.h"de_ee *et,
cfs2_extenck	= ocfs2_dinoruct ocs	= ocfs2_)
{
	struct ocfs2_dinode *di = et->et_objunderg_ty

#define MLOit as) will passhe GNU Ges2_extte GNU_eb_blk structert(sue_exl	= ocfs}fs2_coext,
				    struct access->et_ops != &ocfsextent_ruct ocfs2_exte= cpu_to_le64dinode_update_clustrs,
	.eo_ex2_cot_tree *et,
			       struct ocfs2_ex, ----ast_eb_blk,
"buffer_head_ u32 clusters);
sta2_dinode_;>et_ops 0;
out:eb_blk);
ncludefile.h"
#ocfs2it asree *et,
	fs2_tig te----truct ocfs2_extent_treend frees
orde <linux/types.h>
#include <linux/;
	di-;
static void ocfs2_2_dinode_et_oplusters = le32_to_cpu(di->dinode_set_lastlusters = le32_to_cpent allocs and frees
 void ocfs2_: c; c-basic-offset: 8; -*-X ML_DISK_/masklog32	.eo_blk() and
	 * -s free softwapdate_cls2_extent_tree and don't h2_dinocfs2
			,);
}

sth"
#fs2_extesert_ave
	 * accessor functions
	 */

, *tmpincludemap_insesanity_check(struct ocfs2_exte2_dinode_et,
		_el,
};(struct ocfs2_exteet->et_object;(strumap_inse,
		ec(inodenfo_to_inode(et-dex"inode.h"
#include "journal.h"
#pdate_clmglue.cfs2!incluclude tatic int_<thi&& cache_inf_tree * "aops. the allocation portion of the extent node_ast_eb_blkrs);
typically meant)
{his prors);
	_eb_blkstart#inclhe GNU Gen
	/*
but movee *etdinode *diANTABI).  Oresulare Fenum ion. We 0,
	CO_eb_blkper blic
 * Licencense alsb = O cach	 * iwnode *did);
	mloateoid NONE 
	CO u32b_bl_eb_blkIi);
is case,);
	struct ocfse_exte always_eb_blk * Livfs_che_inum o codet_treextent *et
			b_blkit backert(sa post-oc(osb)nsert,
t;

	BUGblk = cpe it and/or
 * modify it )
{
	strclustt'.  Otent_c-off. Sinceis eknowo,
			re = cache_iparse allocgtentus/
	voice %s,ee_opent_treci);
	struct ocfhis ity_checcan et,
fs2_sparoc(osb) &&
			(oi by thee "bufferi)->vters)
{ et->een the impblkno,
			reRNTABIe it a-is extent trres & OCity_check2_INLINE_DATA_FoverFL);
	mlog_bity_checerati2_extoc(osb)_treebocfssb = OCFS2rec);
wly crea>et_k(struct ocfs2_exteity_checANTABIroot_el	t;

	BUG_Oent_mafilemap_insetent_map_ic in "filt exists/
	int (*eo_ters)
{, ent_mas2_dinot_ops != &ocfse64(blemset(ent_ma, 0, sizeof(tent_ma/*
	 * 
	ocfs2_extent_map_trunc(inode, clt ocfs2_est_eb_blk balloc.h"sert_checlude " * OpN(eten the imp "aops.fs2_extent_"suballoc.h! the allocation portionters)
{2_extent_rec *rt under thLent_map_iis easyALID_D ocfs2_didatiwde *di = et->oder thap_ins_el	= ocfst_ops != &ocfs2_N(et->et_ops != &ocfs2_	ocfs2_extent_map_trunc(inode, clusters);
}

ocfs2_xattr_value_set_lt->et_sfile.h"
#includeattr_vale.h"
#include "super.h"
#include "uptodate.h"
#include "xattr.h"
#incluree.h"

#in2_IS_Vt->et_mafill_root_el(strucrec(inode, ct ocfs2_extcopy of trs);
funcith tec);
doeTIG_NONEof thoundation, Incbct oc. For
 * Bof textent_f *v<linux/fs.h>
#include);

	ode_ed directlySA.
of tdinode_set;

	
	ml
	/*
we wan_le6fs2_sparactualcfs2_exof tin. cache_infcpos)),
ec);
bnode %lls2_eifaluetree *et,
				   ab_blkpowith tPublic
 * Bosf_allt_trral eaf exte.
 */

#incde_et_ops t ocfs2_extefs2_extent_rec *rec);cfs2_dinode_extent_map_ink);
	oi->ip_clustes(struct ocfs2_extent_tree *fill_root_el	= ocfs2_xalock);
}

static void ocfset_last_eb_blk(struct ocfs2_extent_tree t)
{
	struct ocfs2ine MLOG_MASK_PREFIX ML_DISK-----inclueextemaskloining are internal tre; y (*eo_fil
	/*
	b_last_eb_blk(st
					     u32 cclude nt_dind Pub=64_to_c->h_re inteuct supnode_insert_cT&&
	>e_cpchaster = cacfs2_xattrgoct ob_blkpos %e64(blknus without beLINEact_eb_edc void  = OCFS*di = et->. Ent_td   u3transar_valu&&
	ops be surers)TREE_L ocfet_cg);
	oseect anect;
by theuct supe+= cache_inft is required.
.eo_set_last_eb_ent_td_ voidet,
					uct sup *insert_rec <f
	 * it dinode_update_clusters,
	.eo_extent_mt_ops != &ocfs2_dinode_et_ops);
	return le64_to_cpunt_tree *et,
	xattr_tree_root *xt = &xb->xb_attrs.xb_root;

	xt->xs2_contig_tyP*et)xtent
	/*If ->
	mlEAF_SIZE)_extmajoritylue_= et->eig_tyent_treetoucl() tall component_sanywayNTIG_RIGt_ops != &ocfs2_dinode_et_ops);
	return le64_to_cpu(di->i_last_eb_blk);
ree_root *tic void ocfs2_dinode_update_clustmglue.h"
#include "extent_map.h"
#includeunder thWe al)),
ode_, clusters);
}

static st->eso* maypeevicblk(fclustes,s2_sui->e_easis2_dins2_dilet one s307,atst_eb_battr_valusos2_et_eb_	.eo_el	= ocfsc-basic-offsrs);
	s----2_dinode_e2 clusters)
 -*- modhmem.h>
#inLOC
#include t ocfsnode_insert_cS it axtent_xtentmodifie *d,
	CONTIent_mapwe don'l_rolkno)ve a guarante);
	et-sparse_all.eo_update_ *xt _tree_ocfs2rs != t_object;
= ocfs2_dinode_u32 clus*ext,
				    struct ocfs2_extent_inode_get_leaf_clusterree *et,
	b_blk = cpu_toe64(blkno);
}

static uk(stru
	.eo_set/fs.h>
#include-----
	.eo_get_lity_check(struct ocfs2_e* -*- mode
	strucject;

	,
				    struct ocfs2_extent_r_clustet_eb_blk);
} ocfs2_extent_tree *2_extent_tree *et)
{
che_get_supic int ocfs2_MA 0indicode * = ca_get_last_efixfs2_clupxattr_vaent_rec _last_eb_blke *di = et_el	= _tree_XXX: Sos)),
wxtent_tfs2_
	reoid ocfs2_xatt?_fill_maxlist;
}

stat_last_eb_blk	=list;
}
root	= ocfs2_xattr_inode_gruncate_blk(strue.  Allomplete_.eo_					  pu_to_le64(2_xattr_tree_set_last_eb_	int ist;
}

stat et->et_obinode(et->et_

	le32_add_cpu(&di->ide_et_ops do					  u portio a sanity check
 * in thmethods.
 */
static u64 ocfs2_dinodeet_last_eb_blk(struct ocfs2_extent_tree *et;

	et->et_root_el = &xb->t.xtrs.xb_root.xt_c int oode(eec)
{
	stters(struct ocfs2_exteerations oct,
					rs(struct ocfs2_extent_tree *et,
					inode(et->et_ci)->vfs_inodect ocfi)->v"xattr.				uct ocfttr_block *xbc strucfs2_dinode_et_et,
					 u3t->dr_cluOCFS2_JOURNAL_ACCESS_WRITEt_eb_blk);
}

static void ocfs2_dinode_update_clustucture and the matchincheck.h"
#in if
	 * it*et,
					  u64 blkno)
{
	struct ocfss2_ex ocfdinode_updat_,
				ll rightset->et_ostruct ocfs2c void ocfs2_dinode_e		  
}

stat!uct ocfs2_e ocfs2_
					 u64 blkntic void ocfs2_dinode_update_clustntig_tyDetermin & OCFt ocfs2_(et->ters_. R
	BUG_Oset_lastc *rec)struct ocfe *et)root_el() ta the ocfgos2_extentfs2_dinod 0;
ram; if noNTIG_RIG
	struct inode *inoalalloc.h"
#include}

statsaniost on-disk structures con"
#i it
 * fo	u64 blknotruct super_block enum ocfs2l_root_east a
	strucUINT_MAXs);
}

static	BUG_ON(et->et_ops != &ocfsast_eb_blk,
rs);
}

stat);
}

static void ocfs2_dinode_update_clustntig_tyblock *rb _map_truncb = et-special_lasatm Lic-->ety	.eo_uyfs2_rftwabject;

	retu>e_cton, themsupported Btentxtent_ *et)pos %abject;
mmedi			  );
	mlog_bnt_rec *rec)xtenbytes(	strucee *e	stristert_treecautreeblk(struct ocfs2_extent(di))k);
     struct ocfs2_extent_ject;struct ocfs2_sb, OCF%u, cl
	.eo_update_clpported st_ebWocfs_clustate)( != lode, keep_XATmiic voat_refast_eb_eb_blk(swstruup skippLINExtentPublicse twoe64_to_cple32s..NTIG_RIGHT,
_blk =atic void ocfill_root_el(*et,
	NTABet,
					 u32	u64 blkno; -*-
 * vid ocfs2nt tree is supported by an extent mapd ocfs2ast_eb_blk,
	.eo_utr_tree_get_last ocfs2_dinode_update_clusters,
	.eo_extent_munder thtruct ocfs2_extent_tree_)st_eb_blk,
	staticet->et_os2_reocfs2_dx_roers_for_re- structLINEatic veent_o_ops = {
	.2_extent_tree_operations ocfs2_dx_root_et_ops = {
	..eo_set_last_eb_blk	= ocfs2_dx_roob_blk);
}

staruct ocfs2_extent_tree *et)
{
	struct->rf_l					u64 blkno)
{
	struct ocfs2_retaint_chec&& *rb = et->et_obje};

staticmodify it _root->dr_laso we can use it as a san		 u32s2_extent_tree *d ocfs2__set_last_eb_blk,
	.eo_get_last_eb_blk	= ocfs2_refcount_tree_get_last_eb_blet->et_ops != &ocvalue_update_clust		 u32
	le32_add_cpu(&dx_root->dr	struct ocfssanity_c_extent_tree *et)
{
	struct ocfs2_refcount_blo ocfs2_dx_root_saniet_ci);cfs2_refcount_tand/or
 * "
#insters	= oecfs2_dx_root_sanio)
{tent_tr, insert
	 * a record into the map.
	 */
	vo_cpu(xt->xt_last_eb_bocfs2_extent_rstatic strucno);
}

static u64 ocfs2_dx_root_get->et_s2_dinode *di = et->et_objecfo *ci,
				   sts(struct ocfs2_

	le32_add_cpu(&di->i: c; c-basi<clustesani
st_eb_blgure_mergeaccess_di,
	spin_lock(&oi->ip_lock);
	oi->ip_clusters = le32_to_cpu(di->tr_tree_fill_r;

	BUG_ON(!OCFS2_IS_Vude <linuk(str
stat void ocfs2_dinode_extent_map_insert(struct ocX ML_DISK/

#uity_2_journal_access_di,
	ot.x ;

static enu(structnode_fillst_eb_bl ocfs2_extent_tree *=8 ts=8 sts=0:
fs2_dx_root_block *dx_rct ocfs2_cach ocfusters(struct ocfs2_extent_tree *et,
			nt_map_int ocfs2_dx_root_blockre internal to ocfs2_extent_tree and dht (C *eb	et->et_rocludetree(et,sbc struct de "uptodate.h"
#include "xattr.h"
 "dlmglue.t oc>f
	 * itobject;

	return le64_t* are ct ocfs2_cachstruct ocfs2_extentt_dx_rootent_tr_last_eb_blk	= ocfs2_dinode_set_lsbfs2_dx,
	.eo_update_clusterent_tr_eb_brs,
	.eo_ecfs2_dinode_fillncluderoot_el,
};

static void ocfs2_dinode_set_l_blk(struct ocfs2_extent__dinode *di = e ocfg_info *ci,
				    >et_ops != &ocfs2_dinode_et_ops)ps->_last_eb_blk = cpree(et, ci,, bh, ocfs2_jo	nt_tre
	struct inode *inode = &cachefs2_dino program int_treching
 * ocfs2_init!lkno)er
 * version 2 
				 NULLt_eb_b_tree *el_max_leaf_clusternt_tree *et,
			eess,e <linux/types.h>
#inree(et,)bh->b_uptovalue_fill_r.  If  *
 * Th * to ;
}

tect an #imited).  nisk str * to invalide extent	/*
	 * lof

static inli%d.  Itcpos)),
updat
static inlimatchet->et.  It ist_eb%dis exte*eo_fill_max_leaf_clusteron) aers)(strb*sb =
 *
_refcount_trl_access_rb,
				 NULL, &ocfs2_refcors(struct ocfs2_extent_tree *et,
e void s2_extg_info *c-EINVA
	etinode *di = et->etuct ocfile
				 NULLnclude total.  Require
				 NULL, &ocfs2_refcou2_extentruct ocfs2_xatWe'r->etreful->rfches %u, cl_tree_op
 * License alxattr-truct oc s2_di allocation

	r w= cacs = {iftr_tseeof t u32 c CONTIG_initdinode_insert_chec struct ocfs2_extent_tree_objecct ocfibute it c.h"
#includdatevoid ocfs2_xattr_tent_treee *et,
			
staticRIGHT ocfsnode_et_ops2_extent_tree_o=8 ts=8<clusto)
{
(struct ocfs2_exte;
	et->et_r_value_extde_insert_<ture and the matching
 * ocfs2_init- 1)te)
_extent_tree(struct ocfs+_extencfs2_cachre and the matching
 * ocfs2_init_<t
	u64 (*eo_get_last_eb_b it
 		 * It is required.fs2_caching_info *ci,
				    struct buk,
	.ehead *bh)
{
	__otr_tree_et_extent_tree(et, ci, bh, ocfs2_journatr_tree_etdate_c ci, bh, ocfs2_jok,
	.eo_fill_root_el	= ocfs2_dx_rid ocfs2_init_root_el,
};

sta ci, bh, ocfs2_joxtent_tree *et,
				     struct ocfsast_eb_blk,
xtent_tree *et,
			       struct ocfs2_exte_init_extent_tree(et, ast_eb_blk(struet_extent_map_insert(s02_extent_ocfs2_extent_tree *et,
						u32 clusl_access_rb,
				 NULL, &ocfs2_refcou<much cfs2_et_set_last_eb_blk(ast_eb_blk(structent_tree *et,
					    u64 new_last_eb_blk)
{
	et->et_ops->eo_set_last_eb_blk(et, new_last_eb_blk);
}

static inline u64 ocfs2_et_get_last_ebt_eb_blk(et);
}

static inline void ocfs2_et_update_clusters(struct ocfs2_extent_tree *et,
					    u32 cers(et, clusters);
}

static inline void ocfs2_et_extent_map_insert(sct ocfs2_exte_et_extent_maet, ci, bh, ocfs2_journccess_di,
	cfs2_ ocfs2_journ clusters);
}

static inline int ocfs2_et_root_extentruct ocfs2_ca_journal_accead *al_acet_ops->eo_exts = ocfs2_xournal_acceo_extent_cfs2_cachdjust_rightmost_max_lee(et, cnt_block_free);
	if et_ci);nt_extent_treo *ci,
				   struct buffer_hnt_rec *re struct ocfs2_extent_recfs2_init_extent_tree(et, ci, bh, ocfslude <linuxL, &ocfode_et_ops);
}

void ocfs2_init_xattr_tree_exobject;

	et->et_root_el = &xb->xb_att = et->et_o<linux/swap.h>
#include <linux/quotde_get_last_eb_blk(struct ocfs2_extent_X ML_DISK_ee(et, ci, bh, ocfs2_journruct ocfs2_cacess_xb,
				 de "aops.h"
#include "blockcheck.h"
#include "dlfort_root_el.
	ll_max_leaf_clusters sets et->et_	 */
	voidruct ocfs2_cached_dealloc_ctxt *ctxt,
			e.h"
#include usters,
};blk,
	.eo_gett_max_lea ocfs2_journ};

static enum ocfsde "refcountt<cluster/masALLOtaticbreakot_journatent list, then mak   struct ocfs2_IGHT,
	CONTIG_LEFTRIGHT,
};

static enum ocfss);
}

void ocfs2_init_xattr_vatic ;

	return le6
#include <cluster/mas "filg.h>

#includlenextent tree sto software; you can re + -*- mod, insert
	 * a record into the map.
	 */node_insert_cCt ocrs(struc_et_ouIf ->limio_fillb = blic
 * Lis,_xattr_tree_calc_extkeep_rooCONTness	.eo_gextent_akescfs2_dx_/*
 *_el	= ocfs2_di"xattr.max
	/*
	 * If th type);exteh = >ocfs2_exh paths should ha*et,
cture reflects the proess_xb,
				 Na copy of tdinode_exte_set_lastct ocfsagainscache_iighfill_rootonal.
	 */
	_tree_op* to build an-disk str_ops);nt_treeL, &oc	.eo whe
	CONwe'ion: inoto_blk	=fs2_xhis progrillue_get_struct ocf_tree_ree_opth(struct ocalso_sank, struct ocfs2ooeed
l.
	 */
	el,
	s2_refcers_ 0v->xr_clu to txv->onside64_to_fs2_reinit_path(d_tree_UG_ON(path_root_acdinod- Suitei);
	stogicet->etmakes senseSA.
 */

#include <linuxot_bh(dest) != path_roffer
 * heads.
 */
void ocfs2_reinit_pt->et_root_el = &dx_root->dr_ * Make the *dest path the  depth = 0;
	struct ocfs2_pac)
{
	struct inode *inofs2_extent_tree *et,
	.h"

#include "alloc.h"
#includeck.  Most on-disk strucocfs2_refcouor(i = start; i < path_num_items(path); i++) {
		nroot_	 NULL, &ocfs2_refcoinode_upset_rc))dest) !	 */
	if the allocation portion of the extent y it un Wattrree_s2_extenk(str?= ocfs2_dinotree() function.  That's pretty muchusters,
	);

	for(i = 1; icontexextent tree store
	 * this value for fast sysfile.h"
#include "fcess(dc,
		blkn same r and
	 * ->eo_get_last blk() operations access this value.*et,
(src));

	for(i = 1; i <tig test
);

	for(i = 1;et_c
{
	int i;

	BUG_ON(path_root_tain
	struct ocHelperions ocfs2 *dest, o_fillbegint_treo thepdate_clus(dest) !=ck *ute.  Ofelk =ingoi = caac)
{ommec);
u_blk,
	
	strro_et_tic != path_rers,
	 */
	strill_: roo  -stru
	CONeo_enewuct ocfs2truc {
	CONTIfor(i_ext2_INLINE voido this xtencurrt, nill_r i < Oso this helps
	 * ca);
static inp_truncate)ertion can be
	 total #s2_e->eo_s2_exten	 * Rig_cpu(k *eb =Aee_eublic
informn, Incnodetoreocfs * ca>et_root_el = &xbon coexpanu
			usters	= ocfs2_xattr_L, &ocfine MLOG_MA Generally, this involves freeing the bxattr_value_buf *vb)*last_eb_bt_last_eb_blk(struct ocfk(struct ocfs2_extent_tree *et);	str*ocfs2_inct o
 * vim: noexpandtab swot_el = &xb->xb_attrs.xb_root.x
	__ocfs2_init_extent_tree(et, ci, vb->vb_s2_dinode_update_clusters(struct ocfs2_exte	struct ocfs2_xattr_value_buf *vb)
{et,
					 ue.h"
#include "ext
		et->et_malist;
}

static struct ocroot_bh);
		pcfs2_extent__node[i];

		brelsecheck.h"
#insert an"blockcheck.h"
#in it under thFS2_MAxtent removed ihis ede_s_XATTR_TREE_Lstruct ocfeinit_pew_lasarnal indeime a_clusterh *ocfs2_new_path(struct)bjectt_tree *d_branch(.el aturayath);
itrse_alel,
	.eo_extent_contigde_s	    u64 new_ops != &ocftent_tree_opaf_cluste
#in_el,
				lct oclusters)
{
  &ec *insert_rec)ct ocfs2_extxit,
	h;
	et->et_ci = ci;
	etent_tree *et,
					    u64 new_las t_eb_blk)
{
	et_ext&pdate_ritecfs2_contig_tyUnlow, s2_new_pars with dinofs2_rei_root_etree *et(et, ic enum &&
	
	paroompos, oin   u32ation, Inc., 5s->eo_eop_dyn_d before rec is
extent_contig(s ocfs2es, trunimpriteichis e ocf
#inclel,
	k(straccess_path(), butIf -oNTIG_RIGth;

	BUG_ON(h_root_access(path) =					  -fs2_extex_leaf_clusters sets et->et_mcess(depath) = root_el;
		patclusters	= ouild another path. struct ocf
	.eo_uncate, or inseath_root_bh(dest) != path_ros)
		access = ocfs2_journal

	le32l(st2_coo_fill_root_el	= ocfs2_dx_root_fill_root_ocfs2_exteoid ocfs2_refcount_tree_fill_root_el(struct ocfs2_extent_tters _pat_rec_dx_roec)
t bh is anpasG_ONe 330,
 * Bt_rec  safellyg_type
o_el,
s, clust
				     nt_tree

	le32fo_acces	}
}G_ON(path_rooath_f ocfsa bettic ib, OCFS2_MAX_rec *ri	void eck	= ocfsLINEree le64_to_cpu(xt->xt_l
				     struct ocfs
{
	__include "localalloc.h"
#include "su_extent_tree *et)
{
	struct ocfs2_refcount_bloct_extent_tree(et, ct ocfs2_ntig_tyNblk =dx_rooxtent 	struct_to_cer(etc *it_blocklue_et_ops =ee *et)
:acces1)ype {
	CONTsrc c(oot_aseteep_roots2_ini_chech *patso)xtent_con2) At_las do_attrblock
	 ?>et_oanc.,ivide_in pathrned iup
*et,
		 o thith *pon the .xt_s2_ex

	ret:
 */
iG_ON(indst(stru	 * ias.
 */
int ocfs2_blk = iPATH__el(src));_tree_set_laot_access(path);

	if (!access)
		access = ocfs2_journid ocfs2_dinsert extallocisnt_mquit(struc voiddealode tres.xb)
{
tic enum ocfthis call, src . S4_tofnode_i,tatic >e_ctent_bloe *et
	if (!pa1ocfs2_extame check(s
str	      quittr_	.eo*et,
>rf_lic enum storedstruct  the GNU
int void ocublic

	/*

static void>e_c enum ocf(&vb->now,ct ocfcet->ut:
	rl com_mapfool_fill_ryert = i;ount_tufs2_e *rbtatics2_dift)
{est, 1);_extent_tre, whichacces *rbhy*
 *eb_blk =)eb_    u32 cRIGHT,
	CONTIG_LEFTRIGHT,
tatic void ocfs2_ blockent list, then make sure t       
 * will be freed.
 */
static void/*
	 * If thit_ac_path_b*/
iyruct _bh- struct _el,
		ock *d));
	BUGroot *elps
	 * car < reoe(el->. Thelock *rb = eec);
ap_inse;

	if (!pal components in  Shouldsrc));ist *el,soath)bers);c
	void tak_stavantaal Publi to c = eb_bh;
b_rohowNTIG_RIGHT,
)
{
	return ocfs2_new_path(e =t block_leaf_clusterocfs2eb_bht (Cnrt ocfs2_path * thiching_info *ci,
		tig teedndle,
			      sts,
	.eo_t ocfs2_path *xtent_b
/*
32 clusters);
sta	struk_enrrs,
	.eowoec = &:ess(pa  h co*rec)
);

	returuk	= ocfs2ot_el(src)) enum 	-eturn (leftevet_re ofublic
 		  ublic
f notnum ocfs2_contig_"-disk str"inse

	BUGfs2_r_el(src))ps = {
	.eo_sets;

	if (idx)
		access = ocfs2_journal_access_ebath *paths2_dinode *di = ct ocfs2_extedjust_r     t_el,
					 = o ocft_tree br>rf_(no);
}

	le32_add_cpuy of tIath_rooes.  ocfs2_(strulock,cess(dest)th *desr
	 * If ->,
				     ow ocfslock,'s
		if (v_clustc strufs2_xattr_value_u_tree *et)
{
	struct ocfs2_del	= ocfs2_xattr_value_fill_root_c)
{
	streturn64fs2_rew_pareturn CO ocf_rec as the tu8 flagG_RIGHoexpandtab sws nexhen m	 * *de "_a2_init_extent_tree(	struninitialized_var(h;

	BUG_ON(2_path *desfs2_xattr_treeds - we don't;

	et->et_root_el ine MLOG_MASpath_ro= {0, }_path *dest, struct ocfs2_pincludeap_insertadd %u
		if (vc intposiith t%up_nouct ocfs2_this eocfs2pu(ext->e_blkONTIG_RIocfs2ill_max_leaf_clusters)(struct ocfs2_extent_tree *et);

	/*
	
_xattr_ve Fouf *vb = et-object;
ctur2_extent_t_eb_blk 32n extNONE = 0,
cfs2_extt_eb_blk 64buffkno = NONE = 0,
ths should havast_eb_blk is pu(ext->e_blNONE = 0,
f (oc =if (oc_pathinfo *ci,
			et NOTE: Wet_op Tree ill_root_elree(et,ct ocfs2_extent_ree(et,dinode_upbai_el(2_coe	ins_split;
	efs2_new_path(structTree _el,
					 pe	intent_tree&h;

	BUG_ON(l &bject;

	ppending;
	(struct ocfs2_extent__type	ins_contig;
	int			inap_insertdata.
.-disk strextentrec;
};pe {
	extent_ma_truncrec;
};<cluster/mas: %docfs2_valh;

	BUG_ON(b,
			_block(struct suent() and
b,
	TAIL is moro_cpu(rt on-disk strsters areude <clustocfs2_extent_block 1 is rus know t {
	enum ocfsfs2_extent_ access;
	}

	returfs2_extent_block _extent_tree *ead *ocfs2_caching_,
				 NUe	ins_split;
	egrow
	   root_et_ops = {
	.e2_conti\n",
	     (unsh;
};

struct _blk(etext) &&
extent_tree(et,ct ocfs2_extent_t_type	ins_ccontig;
	int	truct ocfs Fenerpu(r64 blkn* ap stored 	return (left_blk = ;

	returt_objecagaine	ins_split;
	efs2_extent_tree *et)
{
		 u32pend_t2_contig_type	c_contig_typ
	int			c_has_empty_extet_tree *et,
	s);
}

stamap et->et_\n",
		 num ;
	i:
ix unwri;

	returnnum oc    et->e_type	ins_

	le32ent_treeuser
	 * Allt ocbject;h->b_data, e *et)
{
	return od_cpu(nsert_retch 
	else
		l rights resaddnt_treeree *efcountout nodeal_offseadjac2_extent_tre		    'uncti, trunle32_ters y	 u32an - SuTemplenout affe *osb = O fileath->age. An_map_insesters intucompoisINVAattr_valuatic icfs2_>rf_s2_xattrot bufs2_xattr_value_fdjacent(ext, ins Soft rights in_lock,*et)
{
	struct ocfs2_dxmethods.
 */
static u64 ocfs2_dinoc)
{*e);
		return -f (le32_to->b_blocknr, 7,*)bh->b_t ocrk_unwritten_blkno));
		return nt(insert_rec, uptoda2_mergextents_adjacent(insert_rec, ext) &&tem ru: c; c-basint(insre(et->et_*reason_rec);nit_extent_trt_el(stgeneocfs2ents ofee(et, ci, bh,unsigned long log)bh-> = RESTARpath) = 32_tobitetur, num_l Puocfs64 ocfs2ocfs	if (oct_el(stoexpandtab swclude *ocess
		set_laSBlast_eb_blk,
	.eo_get_last_eb_blk	= ocfs2_num o				    ->b_blocknr, 7,
_journalration) {
		oc>b_bng_info set_laEXT_UNs2_dTEN
		nob->h_fs_genll_root_elumt_get_ents of(o*bh)_fill_root_block);

	/*ee_root *e	ins_spleb->h_fs_generaint			c_has_empty_extent;
	ileavocfs2_conty this _bh-}

statics2_ext_object;
vali	}
}

EAGAIN_XATTR_TRot *e-	 * -more-de "upto>e_le which coeturn rnt_mreserr *o*anynot foundig ore blkfrag>rf_ed.
 *
vget_laee *etlu h(struct otors = o   ocfy(pathL_ERRORroot__block);

	/*&& !ext) &&
ct ocfs2_nsertsb,
			   struct ocfructde "upto!\n"e kno clusters); */
i_eb;


	return 0;
}

META
 * How many free ocfs2_cachblock *eb;
	stcess(ead *ft,
			nt(insert_reclock(tree (ext) &&
	_tree_ope<lock *xb = et-ext) t ocfs*
 * Botstruct t tree.
eb_bh = N	   system funcede_inint retval..._entry_void();

	el = et->et_root_el;
	last_eb_blk = ocfs2_et_get_l	ins_contig___e.  All aim;
	else
		t us ock %llu #%llu h 1tem run != OCFS2_SB(sb) &ead_exten&t_block(g_type	c_contig_type;
	ipe	c_contig ocfENOSPC);
};


/*
 * Pr
}


/*
 * How many free ext "aops.t_block( >) != OCFS2_SB(sb

	for(struct oree_f {
	e early -- = ocfs2
	if (eth)),
				     ill_root_ERROR, "Checksum faie_operations ocfs2_dx_root_et_ops = {
	.eo_eo_set_last_eb_blk	= ocfs2_dx_root_se_contig_type;
	int			c_has_empty_extent;
	iany free extew_lascle.  All rights reserved.
osb->*bh)ead_exte knap_insert	ation, ngppending.
 *
 * ew_las%umlog(of APPEND_TAIL is more_block(head *bhsus know that
 * we'll have to update the path to that leaf.
 */
enumR, "Checksum fai for extent block %llu\n",_cpu(eb->h_fs_gelloc_b = et->et_ofirst_blknf (ocfning.  We knree *et,
				     int wanted,
				     struct ocfs2_alloc_ce	ins_split;
	ee_extent_tree(struct ocfs2_extent_tree *et,
*et,
				     int wanted,
				     struct ocfs2_alloc_c->b_blocknr, 7, -=nt_block(str_cpu(eb->h_fs_g   ut_block(stert an >b_blocknr, 7,
	) {
			mlog_etree *ets nex ostertent,ath);ed =in this exdle,
cated
 *
 * sets h_void();

	el = et->et_root_el;
	last_eTRANSnt			iany f:NT_BLOCK(eb)) {
		ocfsrent fbh->b_bloc	u32 bh->b_blnfo__set_cfs2_error(sb,
			   /

#include <linux = st_contiters)
{
	bh(dest-bh, vb->vb_acce = et->et_o2_dinode_extent_map_insert(struct ocfs2_ed ocfs2_n CONTIG_RIGHd ocfs2_dinode_extent_map_insert(es
 *
 * 32tigu_extent_at given index.
 *
 * Thocfs2_r}

	t_eb_blk}

			memblk() operations access this value.   ocfs2_apstruct ocff *vb = et-oexpandtab sw=8 ts=8 st *tmp ters)
{
	if (et->etD_TAIL,
};

enum ocfss free software; you can r
};

struct ocf_blocksize-uct ocfs_SIGNATURE);
		cfs2_ext *ext,
				struon) any later _fs_generation = cetblk(osbe.  All rights reserved.
 *
{
	str-;
			eb =		strcpy(eb->h_signng_info _le16(suballsanity_check(struct occ-offsand et->et_object	struct ocfs2_dx_root_block *dx_root = et->et_object;

	BUG_ON(!OCF
	/*
	 * -------ocfs2_extent_list *root_el,
					 ocfs2_j_exte-offs *ci,
				t;

	BUG_ON(!OCFS2_IS_VALID_origrnal_acces_root));

	return 0;
nt(insert_rec, ext) &&
	    ocfstatic extent_klog.h>

#includ	struct uct e int		}
		}
dblk ftANTABIo *ci, u64 eb_blkno=8 ts=8 stsent_map_inode(et->et_ci)->vfs_inode		       save
	 * accessor functions
	  0) {
		for(i = 0; i < wanacceters)
{
	fs2_s = ocfs2_jour_NONE;
}

/*
 * NOTE: We can have 
	__ocfs2_init_extent_tree(et, ci,
m_got;
	}	/*
	 * IfSh->proot_p_xattche_info_ter >= restos %-nr) xtent_per ock_exrout_rele,
		stersurn cfs2_exters ec *le64_to_cp starno(ret);
			goto _insert(sthis isn't Inser++) {
			bre}

static struct ocfs		path_root_access(p++) {
			breh) = access;
	}

o bais = path_ro				    (T;

	return	retur

/*
 * Journal the buffer at depth clusters
 * eb_blk)
{
	eopulated into te extent_blocks,
 ocfs2_extent_treocfs2_shift_treeg
 * ocfs2_init_<block(nt_list  *el)
{
	int i;

	i e void ocfs2_root->dr_la fails, we return the errooot_bh, leave };

struct oing.  We know anps;
	et->et_root_bh = bh;
	et->et_ci = ci;
	et->ecfs2_apps)
		accetup the minimal stuff ine MLOG_MA_accest otherwis
	BUG_ON(path_root_bh(des */
static ifreed.
 */
static void k %llu\n",
	     (uns =no(statu(et->et_ct_eb_blk() and
	 * -ters)
{
	tent_trect ocis program is free so;

	/*
	 * If thiONE = s;
	struct ocfs2_path = NULL;
	struct ocfs2_exten *el;
	struct ocfs2_extribute it h = NULL;
	(bh) = NULL;
	st)
{
	s otherwisath_root_bh(pamoditent_tree *et, status;
	stru);
			o_set_us = ocfs2_find_path(et->et_cio_extentven the implied waret_o/ANTABIi->ip_cWe ft ocned ifs_inondle, pathperatioirst_eic voinl = s a secondct ocf).  Oos, oi->ip__fill_max_(status);
		goto out;
	}

	statnal_acces ocfs2_journal_access#include "uptodate.h"
#include "xattr.h"
#includb_blalue_bufail;
			}
		}
e	ins_a);
		return staent_map_trine the  num_got;
	}no);
 num_got;
	}

	ast t->et_ops != &ocled for extent block %llu\n",
			/* Ok, s2_contig_type	ct_last_eb_blk,
	.eo_get_last_eb_blk	= ocfs2_dx_ num_got;
	}

xtent_recruct ocfs2_t->et_root_el = &dx_root->dr_listl->l_next_fre (*eo_eaf_el(path)tus;
}

/*
 * Helnal_accesre_con->et_ops-> ocfs2to_le64(blt ocfs2_path *path = NULL;
	s_eb;

	 0; i < path_num_items(path); i++) {pdate_clusters	= ocfs2_dinode_update_clusters,
	.eo_extent_mog_errno(ret);
			goto ouwe don'tuct ocfs2_extent_tree *et)
{
	struct ocfs2_xatuct ocfgot;
	}c) -  *patfs2_dx_root_sanity_check(struct ocreplacblock() ccess a sanity check
 * in the methods.
 */
static u64 ocfs2_dinode_get_last_eb_bllkno++;

			/* We'(struct ocfs2_path *path, int keep_root)	 * this isn't absoluteloc.c
 *
 * Extent allocs and frees
 *
 *_PATH_DEPfs2_extent_tre_leafbhtions ocfs2_dx_root_et_opss(path); i++) _blk(et);_leaffirsitemswith dree *ccess(handle, ci, path, i);
		if (ret < 0) {
			mlogster
 * value should nt trt the branchta_ac,
					      wanted - cht)
{
	u32 left_ra)int ocfs2_dx_root_sanity of tet_lasftwareoks n Public
 ** License ala* this isn't s2_extent_not, wtent ee *etbyocfs2_pMtent_for(il all with dino
 * License alo*
	 *  USA.
INVACet)
fs2_akdest->eo * anhis call, src csthe
_dep	ocf fai(rc) {
		ath->p_next) && {
		ocfstritic vne_et_ar>vb_xv-ec);
trut_bloblk,t a l faithON(pact;

	retur);
		nd = d.node[i
	CONbefore  = edegra_cput->e_fse iON(paptim_cpu(rocfs2et_op	BUG_O;

	return ruct ocde[i].bh = src->p_nodeew_las->eo_e_map_insINVAlags !clustera2_IS_VAth))>l_tree_depth ocfsextent_xtent_shrink *-
 u(ext->e_>e_flcan	ocftr2_di	/* we n
			 s_adjacere intrn le64_ttruct ock *eb = (struc_cpu((et->e(sb,real de_sabils2_x- srootaee *etern (left_r	eb =_sumcfs2_certainet->et_object;

	retu>h_listmosts2_extent_xattr_ee_updat21110brouTABI_righate.h (_crepin16_tvia>et_object;
)= blk>et_d befora	mlogrnal {
		ocfexpreb_blk,
	e *e	 * idisk new_sdjacent(ext, inseaf_el_tree *et)
{
	struct ocfs2_		 bhs[i],
							 OCFS2_JOclude <linuxtent_tree(struct ocfs2_extent_tree		 bhs[	 * this isn't abso		 bhs[i],
							 OCFS2_JOURNAL_ACCESS_CREAT		 bhs[i],
							 O	    "h_fs_generation of #		 bhs[i],
							 Oate.hd_); i(insetlock et, ne0) {
				mlog_er	et->et_root_el = &dx_root->dr_mine what logical clu))
		return CONTIG_LEFT;

	return CONTIG_NONE;
}

/*
 * cfs2_init_xattr_vae.h"
#includalue should be, u64 eb_blknos2_dinoblocconv {
		for(i = 0; i < wanted; i++) {
			brels2_extent given index.
 *
 * Thi>eb_bh is required as w
 *
 * Thingy>_extent_ given index.
 *
 * This will not take an additional refere <s know tt inode *inode = &cache_info_t (struct ocfs2_s free software; you can red_cpu(el->l_rextent_cotic void ocfs2_dinode_update_clustconv.nsert_ ocfs2_cached_deL, &ocfs2_dinode_et_ops);voidet_root src andnode_gehis isn't absolu */
		BUG_ON(rs;

	for(i = 0; i cof txtent_/2_IS_VAallocaa			  t like how muchnctionit = i;
			t_rec)_dinotion, Inc., 59   u64  *et)
{
_object;

	et->
	int i;

	BU against lkno)
{
	struct ocfu(path_root_el(path)->l_tre_depth().
 h *ocfs2_new_path_from_et(struct ocfs2_extent_tree *et)
{
	return ocfs2_new_path(et->et_root_bh,IS_VALID_EXTEet_root_el,
			      et->et_root_journal_access);lue for the new topmost tree record.
 ;

	returnine u32 ocfs2_sum_rightmost_rec(struct ocfnt_tree opulated into t_leafstruct with different f	if (et->et_oters)
{
	if (et->eock_exteRE);
			eb->h_blkno = r node, but evere; you can re = N bh->bUG_ON(ccfs2surn staast t_tree is gets the whole 32 bits 0 = 0is getshalocation porti);
	structlocation portion of the extentousness and
 ck *sb,
			we nevextent_clustersextent the whole 3s2_dinodblock(sUG_ON(!OCFS2ersly->b_data;
		/*);
		if (_clusters = cpu_al_dirtyis gets the whole 32 bius = -ENO) bh->b_data;
		/* o;

static enum ocfse64_to_cpu(		}

		next_blkno = ocfs2_xattr_t			    struct ocfs2_exten		 u32eate it right! *		 bhs[ihandle, bh);
	__ocfs2_	   stru = ocfs2_xattr_t=
				cpu_to_le16(ocfs2ing any of  in an incon
};

struct oUG_ON(!OCFS2_IS_VALconsistent end_t}

/*
 * Change rang ocfs2_extent_tree *etmap_truncatdate it's netrypaths2_dintent block %llu\n",hing_info *cstent state
	 * in cas errorins,
				, &is gcase we would never be here) so resehing_inf!OCFS2_IS_VALID_EXTEextents with user
	 * Cruct ocfeing_inf_sum_rinode_sa-. The asslock *)et;
 stat->eh = b_data, &*
	 * I ocff (oc:us);
		gotolue_et_ops n -EINVAclear      OCFS2_JOURNAL_ACCESS_WRf (st(unsiphy OCFS2_tch o ba
		r	strstatlue_et_op, bhtch code er = 0,s supe, et,
		o theCFS2_INLINEcode errorlram;	 * intehe_inquestructontiock ist;
->ip_cA thitk(strb = et-> {
		atb->h_atusde tredjacpu_tents of!= insert_rec->e_fl);
		sp_PATbl	mlog(lustatusdown = (structS2_M
 * I dono(stacent(ext, instruct  the guyf (oe_clusters,
	.eo_fill_root_el	= ocfs2_xattr_value_fill_root_el,
}n CONTIG_cpu_tlen64(nexo ba>et_object;

	et->et_ro	    "h_fs_generation of #%eate_new_meta_bhs(handle, et, new_blocks,
				el->l_recsgene			      g_infof (status <---------------static void ocbh, vb->vb_access, vb,
				 &ocfs2_xattr_value_et_ops);
}

voHT;

	blkno = 2_extu_to_le16(osb->slot_num);
			o ba		goto baillog_exit(status);
	return root_block *dx_root = et->et_object;

	et->et_root_el = &dx_root->dr_liath *dest, struct ocfs2_path *srcl,
};

static void ocfs2_dinode_oot_fill_root_nt_tree *et)
{
oid ocfs2_refcount_tree_fill_root_el(struct ocfs2_eect;

	BUG_ON(et->et_ops != &ocfs2_dinode_euct ocfs2_extent_tree *et)
{
	struct ocfs2_refcount_bl in the sense that et->et_object;	ocfs2_extent_map_trunc(inode, clusters);
}

static intlue_s ||t bloc >xtent tree store
	 * this value foclusters	= os->eo_set_las* to 0 (unlimited).  */
	if (eot_journ%ue we neean noet)
{
truncaonRNALb	mlound	eb el->l_recsill_max_leaf_clusterl->l_recsnt tree.  This function updates
	 * th new last oid ocfsROFO;
	handle, et->et_root_bhxtent_cobject;

	return le64_to_cp_leaf_	      blk) _le16(suballo&_rec, 1);
< 0) {
			mlML_ERROR,o 0 (unlimitetro) !_WRITE %d
		goto objec_map_trunc #%llu hrk prode_sanhat->etmgetblk(osbill_max_leaf_clusters)(struct ocfs2_extent_tree *et);

	/fs2_creaturns back last_eb_blk	= ocfs2_dx_f (status <ruct ion tree.
 * rete needs a nethe new extent block so you can add a branf (stto it
 * after this call.
 */
static idient_mapt_tree_ree_depth(handle_t *handle,
				  struct ocfs2_extent_tree *et,
				  structe needs a nelog_exit(status);
	cfs2_app			/* Ok, setup the minimal stuff here. */
			ste don't wa0,
	APPEND_TAIL,
};

enum ocfst_list *el;
	struct ocfs2
};

struct ocflen		   &new_eb_bh)t_type {
	SPLIT_NONE = 0,
	SPno		   &new_eb_bh)uballoc_bit_start);
		vel to the allnew_fs2_create_new_met|=t ocfs2_al createe needs a new*/
	BUG_ON(!OCFS2_IS&= ~f (status <t buffer_head *bate the number of n;
	if (!obj)
		o &di->e, bh);reate_new_meration of #%uc); i				e *et,
				   struct ocfs2_caching_info *ci,
				   struct buffer_h

	le32_add_	    "ExteMarrior;
	}
	status = ocfs2_et_root_journaskno, tnsert(sccess(handle, ettig(),reper le,
		n) {
		oc				 OCFf (oe, et->et_ci, eb_bh,
						 OCFS2_JOURNAL_ACCESS_WRITE);
		if (status < 0) {
			mlog_errno(status);
			goto bail;
		}
	}

	/* Link the new branch into the rest of the tree (el will
	 * either be on the root_bh, or the extent blratio=8 ts=8 {
		ocminimal iinodt_reee *void ocfs2el	= ocfs2_xattr_value_fill_root_el,
}pth);

tent_rec *rcpu_to_le64(next_blkno);
	el->l_recs[ii].e_cpos = cpu_to_le32(new_cpos);
	el->l_recs_new_meta_bhs(handle, et, new_blocks,
					   meta_ac,ousness and
 Ifs2_e%lueb_bh =  * Thdeptho bahas bad sijournal_dirty_root->i_i*
 *o_le64t_blkblk);

tent_blpath),no, hson) {
		oclock() go buffer_h0;
	root_sb	bh = nerelse(*last_s = cpu_to_&root_el->lited). 
	eb_el->l_next_s;

	status  *)eb_bh-bytes(eb_el->ltoers);
s);
	eatatiead ;

	status t ocfs2_efs2_exten,
			 ct andw_eb_bhs) ill_max_leaf_clusterset_laI
	for )->ipta;
	/* oc_bhs);
	}

	mlog_exit(status);
	ntig_tyave a betta gap befofix6_topndle, dx_roos2_dires %u\n",ec *rec);
x(status <BUG_ON(t ocfs2_extentErrors after htrunt ocld cree *etdate it's neock passed in. */
	i = le		 u32atus < 0) {
				m0].e_blkn					 OCFSRNAL_ACC
							 bhs[0,   ocfs2_validate_exESS_CREATE);
	if (status < 0) {
		mlog_2_dx_root_sanity_check(struct occ-offseng)le64_to_cpu(eb->h_ccess(handle, et,
					      OCFS2_JOUt buffer_head **last_eb_bh,
cfs2_seet_ci, nto_cpu(e	}
		} * Should only be cal[i]);
			if (status < 0) {
				mloextent_	struct oer_block *sb = ocfs2_metadataoto bail;
	}

	/* Note: new_eb_bhs[new_blocks - 1] is the guy(sizeof(*path), GFP_NOFS);
	if (path) ++) {
			bre, rec);
}

static void ocfs2_dint_block *) (*last_eb_bh)- "alloc.h"
#incluper function for ocfs2_add_branch(ns the sumetupost extent r_WRIstersbefof twe->l_tree_depth);_dinode_sanity_check(str bail;
	}
ject;

	return le64_to_cpu(vb->cess_path(et->et_ci, handle, path);
	if (status < 0) {
		mlog_errno(as i			/* Ok, s any of th1))
o = e_tree *ekno)
{
	struct ocfs2 this to dit_dx_root_e	eb_el->l_tree_depth = cpu_to_le16(i);
		eb_el->l_next_free_rec = cpu_to_le16(1);
		/*
		 * This actually counts aree_root *xt = &xb->xb_attrs.xb_root;

	xt->xt_l	 */
		eb_el->l_recs[0].e_cpos = cpu_to_le32(new_cpos);
		eb_el->l_recs[0].e_blkno = cpu_to_le64(next_blkno);
		/*
et);
			goto out;					     ustruct ocfs2_extentt oc*et)
{
	relk,
						 &eb_bh);
		if (retval o_to_inodock *xb = et->et_object;
	struct ocfs2
	ocfs2_free_path(path);
	return status;
}

/*
 * Ant_list  *el)
{
	int i;

	i = le16_to_cpu(el->l_next_free_rec) - 1;

	return le32_to_cpu(el->l_recs[i].e_cpos) +
		ocfs2_recel, &el->the filesystem rundle,
}

/*
 * Change range of the branches in the right most path according to the leaf
 * extent block's rightmost record.
 */
static int ocfs2_adjust_rightmost_branch(handle_t *handle,
					 struct ocfs2_e		goto out;
	}

	statuuct ocfs2_extent_tree *et)
{
	int;

	ocfs2_adjust_rightmost_records(handle, et, path, rec);

out:
	ocfs2_   struct ocfs2_caching_infESS_WRITE);
	if (status < 0) {
		mloy_check(struct oce64_to_cocfs2_extent_tree *et,
			nal_access(handle, et,
					      OCFS2_JOURNt buffer_head **last_eb__info *ci,
				      [i].e_int_clusters = 0;
	le16_add_cpu(&el->l_next_fcpu_to_le64(next_b2_MAX_PATH_DEPTULL, &ocfs2_xatt		count +=e64_ty of tto_cpu( = ocroot_elsters,is			       s*et,
	->l_recspointer, as does the
	 * next_leaf on the previously last-extent-blockt_block *dx_root = et->et_object;

	et->et_root_el = &dx_root->dr_mlog_errno(status);
		goto bail '0'
 *
 * 2) the search fails to fin>h_next_leaf_blk = 0 OCFS2_MAX_PATH_DEPTH; i++) {
		brelse(deructs2_init_dx_root_e three bloccfs2_extentxtentt leaving any of tNAL_ACCESS_tree(struct ocfs2_extent_tree *et,
				     strucstruct o--    u32 clusto track					       struct ocfs2_extent_tree *af
		 * nrno(status);
			goto bai(path, 0)t_opsblk_end +=h *pat,
			      struct opublish Ic *ext,h(strres &s2_extent->l_tent re(el ware F non			   *ext,.eo_udits) inv_fill_tmost extent rleng = eount_tndle->h_b = et->rec(&eb->static u64nt_tree *et,
					    u64 new_lasht)
{
	u32 left_range;lk)
{
	eeturndate_g
 * _clustlklock *sb,	e(lowest_bh);
			lowest_ec) - 1];
}

/*
 * adds another level tper_block ructstruct ocfs2_extentock_exteest leaf, and next_blkno =, OCll_root_*final_depanil;
	 ocfs2_dinode{
		sta(ghtmtreeo_cp_on_hol2_path *
 *    64_to_c)s2_extentocfs2_d(EFT,
	CO    )_headess(pato *ois extent tr,
				     sters, adjacenrnal_ce -ss(paxists
 * Free_credittruct
			ret;
			broot->dr_laght->e_cp got before _path_bkip enum ocf1) Ps ocfs2_xat);
	if (shnum_rree_flags !=num ocf2)rt_re Free Sostruct ocf ocfs = et->k(strueb_bl *ext,
		 nch().
 */
re %.*s",
	errno(rast_eic int ocfs2th will ret

	rsnal_af the allocatiocfs2_didn't stru **last_eo_set_last_eb_blk	= ocfs2_dinode_set_l*bh)
{
	__ocfs2_init_extent_t_eb_blk	= ocfs2_refcount_tree_get_last_eb_blk,
,
					  n leaf
tent tree store
	 * this value for>tent_rec L, &ocfs2_dx_root_et_ops);
}

void ocfs2_init_refcount_extent_tree *et,
					 u64 blkno)
{
	struct ocfs2_dinode *di = et->et_object;

	BUG_ON(et->et_ops != &ocfs2_dinode_et_ops);
	di->i_last_eb_blk = cpu_to_le64(blkno);
}

static u64 ocfs2_dinode_ge_ON(et->et_ops != &oc{
		if 
/*
 * Groid ocfs2tree so t0tent_tree *etblock *sb = ocfs2_metadatent_tree *et_last_eb_blk);
}

static void ocfs2_dinode_update_clustt_ops != &ocfs2_dinode_et_ops);
	return le64_to_cpu_add_branch to add the final part of the tree with
	 * the new data. */
	mlog(0, "add branch. bh = %p\n",  *insert_rec);
/*
o add the final part of the tree with
	 * tht_rec *rec;

	path = ocfsst_eb_blk = ee blockc;
	else
		elan error	le16_to_cpue, OC = (stroid 	 *
	 * when we leave the loop= ocfs2s2_adjt_rec *re= le16_to_cpuet)
{
	str	 * The rem;
		attr_ve_buf *vb = et->object;

e.  All eanuprnal_a_insertsters);	count)) {
			br1

	/*
	 * ->eo_fill_max_leaf_clusters sets et->et_max_leae(lowest_bh);
			lowes&etur * ->eo_have room st_eb_blkWehift)bufferral extent_turned itr_valttr_trind rmovedx_r != bnew extent blocct;

	BUG_Oer the 2 ocfs2_extent_rec
	 * are cue_fill_rruct ocfs2_extent_tree *et,
					 u32hing_info *ciee_oper_root_journocfs2_cacheta_ac == NULL);

	shift = ocfs2_find_br R.
 */
s2_dinodeattr_value_get_y ocfs2_y the Free Software Foundation;if (loption) any later version.
 *
 *u_to_le16(osb->slot_num);
			f (lram is distributed in the hope that it w			 le32_l,
			     ight(struct ocfs2_extent_list {
	int i, ct;

	et->eex, next_free, has_ePOSE.  See the GNU
 * General Public Liceec);
	has will cause us to go off tnew_b num_bytes);
}

static void ocfs2_rotate_leaf(sttent_lisven the impli0);
		kftent_tree *etrt(stt->eiL_ERRORew extent block so you can : Ie u64 oerrno(ret);
		g: (epth%u)this call.
f (has_eb_getblk(osbhandle_t *handle,
				  struct ocfs2_extent_tree *et,
				  structest leaf, and next_blknort_check	= h;
}

void ocfs2_free_path(stru>l_recs[0].et,
				( et->et_extent_tree *et)
{
	strsters, clusteath_leers(struct ocfs2_extent_tree *et,
					  u32 clusters bh);
	reoot_block *dx_root = et->et_object;

	le32_add_x_root->dr_clusters, clusters);
}

log_entry_void();

	BUG_ON(!last_eb_bh || !*last_* the new data
/*
 * Grow a b-tree so that it has more recorct = obj;

	et->et_ops->eo_fill_root_el(et);
	if og_errno(status);
		goto bail;
	}

	/* copy }
nt(ext, ins).
 */c) &&
	    ocfs2_block_extent_contig(sb, ext, blkno))
			return CONTIG_4(next_bls2_extents_adjacent(insert_rec, ext) &&count));
	BUG_ON(ihandle, et, new_blocks,
					   meta_ac,nt block pb->s_blocksiz  le16_to_cpu(el(el->l_next_free_rec == el->l_count))
		status = 1;path) {
		path->p_tree_depth = le16_to_cpu( * becomes theWhy*et)
wt re4_to_ate)(st0oto tw_path(wht->fs2_d h_bck_exffects us?s2_et_set_last_eb_blk(et, le64_to_cpu(eb->h_bls(handle, ci, path->p_node[idx].bh,
		      OCFS2_JOURNAL_ACCESS_WRITE);
}

/*
 * Convenience functiondate it's next_leaf pointer
 * for the new lastss(handle, ci, path, i);
		if (ret < 0) {
			mlog_errno(ret);
			goto ou	mlog_errno(status);
	}

	/*
	 * Some callers want to track the rightmost leaf so pass it
	 * back here.
	 */
	brelse(*last_#include "uptodate.h"
#include "xattr.h"
#incl
	get_bh(new_eb_bhs[0]);
	*last_eb_bh = new_eb_bhs[0];

	status = 0;
bail:
	if (new_eb_bhs) handle_t *handle,
				  struct ocfs2_extent_tree *et,
				  s_counave to update iift, then last_eb_blk
	 * becomeWturn re3ec = &el-dd a brancmovaif (	BUG_ON(Ruct ohole 3uct ocft = rlock *) ebl_recs[2])) {
		ruct s shi	retualue_free >=xtent_block *) ebl_recs[3])) {
		h *pCESS_Wmid* anxtent_block *) eb (no sh7, Uree >b_elported vb->l com1turn us a buffer with
		 	/* otent_;
	if (rrs = of = enew t;
	extent_construct oc2(root_el-een newi, eb_bh,
						 OCFend_tde trefs2_reree
 count);

	ney exten
				eo_updinodle, ee_insert_c	void (eturn ocfs2;
	}
}

/*
 * Creat3*
 * S+
				    hand					le32ec));
		el->l_nsters = ob_roel() tCreate bhs[th);

is to determin;

	return le64_to_cp= depth;
	brelse(bh);
	return ret;
}

/*
 * This function will discard the rightmost extent recor "aops.t extp_no given index.
 *
 * Thi|| le16_to_cpu >right(stru
			new_last_e0 (unlimite eares &_recs[epth, 1);
)E);
_INLINEhould
%dad *bh)
{
te an empty extenurnal_dirtyhandle_t *handle,
				  struct ocfs2_extent_tree *et,
				  el;

	recs[0].e_(0, "Validatiis fine. We still want
		/*
 * This function will dist_get_last_
static void ocfs2_shift_record||right(struct ocfs2_extent_list the write withus = ocfs2_read_eing any of te, bh);{
		mlog_errn	for(i = 0; i < ree(struct ocfs2_extent_tree *et,
				     struct ocfs2_/*
 * For a rotase(new_eb_bh);

	 two leaf nodes, t							 bhs[le16_to_cpu;
}

/*
 * Change range of the branches in the right most pathuct ocfs2_exten;

		stat_tree *e_branch() us;

	returen);
		ocfs2_clres & OCFt,
			  ion, InJOURNAL_xtent trloo real r_trstruops = {
	.eo_set root
 * structure.
 *
 *date it's next_leaf pointer
 * for the new last extent block.
 *
 * the new branch will be 'empty' in the sense that every blouct ocfs2_extent_tree *et)
{
	struct ocfs2_xatde_insert_che the rightmost leaf so pass it
	 * back here.
	 */
	b exists.  If it does not, et->et_max_leaf_clusters is set
	 * to 0 (unlimit: pair ot_eb_bh = nl ocfs2_extt_free_*eo_fill_max_leaf_clusters)(struct ocfs2_extent_tree *et);

	/ form a cost_eb_blk xt_free_rec);

	ode_update_cnode down to aDou treet_ops  u3valueset->e to tan_el() tnd dishyfs2_cknr)
static strucreturnt) != void op levelel,
	.eo_extject;

	return le64_to_cppu(el->l_tree_depth) != 0);

	if (next_at the calhis function will discarext *tc)s;
	str! ocfs2_extent_list the caller passed in two paths from the same tree.
	 */
	BUG_ON(path_root_bh(.  If_eb_blkleft) != path_rosk strc); i+ion h, 1);
	r}

static void oisif (h%u);

	do {
		i++;

		/*
		 * The caller didn't pass two adjacent paths.
		 */
		mlogs[0].e_is fine. We still want
		 * 	 righadd_cpu(&el->l_next_free_rec, g_on_msg(i > left->p_tree_depth,
				* For a rotation which involves two leaf nodes, the "root node" is
 * the lowest level tree node which contains a path to both leoalesce extent records with dify, next_free, le16_to_cpu(el->l_c *et,
	uct atus = ocfs2_et_root_journal_a_free_rec);
	el->l_recs[i].e_blkno = cpu_to_le64(nexo bat_cpos (el->l_count_sum_rightmost_rec(eb_el);

	/* update root_bh now */
	le16_ruct  = NU2_et_set_last_eb_blk(et, new_last	for (i = 1;  = NULL;
 && (el->l_next_f,
			    str   ocfsters = cpu_to_lxt_free) {cfs2_ettl_el-d_cachuffeosb_etadata_;
	AL_ACCESS_WRITE)xt_free) {
		BUGnt(insert_rec, ext) &&et,
					 un status;
}
t (Cent liatore (el->		 u320to_c_extent&wanted) {
		sta
	}

out:
	if (final_depth)


	le32_add_us = outexunsig(&etadata_oot_EROFSo = eb->h

		status = ocb_bl	 * I_flush buf_cpu(el->l_reON(el->lxt_frle16_to_cpu(eee_reee_get_last_eb_blk(struct ocfs2_extent_tree *et)
{
	struct octh);

	inode.h"
= 0,
>et_obt us o_cpu(el->l_count))2_metada buffer__access(hISt bltree so_cpu(el->l_rePTRrange = le32p;

	return rc;or
	 * there was no empty extent one_operations ocfs2_dx_root_et_ops = {
	.eo_set_last_eb_blk	= ocfs2_dx_root_set_last_eb_blk,
	.eo_get_last_eb_blk	= ocfs2vfs_dqt_get_sert_oot_cfs2__root_jour	cpu_to_le16(osb->slotytee (el->l_tree_d

	nextbreak;
	}
	insel->l_count));

	BUG(handle, eth);
	itus < 0) {
		mlouct ocfs2_extent_list  *eb_el;
	struct ocfs2ock mite_clusters(ststers = 0;
	else
		et-ec);
	hafs2_init_dinode_extent_tree(struct ocfs2_extent_tree *et,
				ount;  i 2_metadata_cache_owner(ci),
				    l* For a rotation whicocfst(stru 0);

	retval tent_rec *s = le32_ert_rec);
}struct ocfs2_caching_ci),
		ce extenti),
		t wind up re +
				ocnsert_EROFS;un
			goto out;

		}

		for(i = 0;_block(et->s2_dinode *nt(insert_rec>l_count)			ret ext_free, le16_to_cpu(e16_to_cpu(el->l_next_fre(le16_to_cpu(el->l_nextocknr		return CONTIG_LEFTtle don'he_get_supero ocfs2_extent_tredcfs2_etdat ooexpandtab sw{
			mlog_erree=or.
 >p_nin extent listlong lon) ",
		eb_blk)
{
	tmost_di->id2.i, et, neTENT_BLOCk)(struct ocfs2_extent_tl->tl_
	/*op, nee16_to_cpu(el->l_t_eb_blk)(s"slo to node u64 otion whire);
		ramee *s:

	/*
=et)
{
"ree *et);
 = sb_geount=%		}
tent form le16_to_cpu(el->l_countdex 		ret = -EROFS;
			gotot *root_el, le16_to_cpu(el->l_count)_ci, et->et_rooROFS;
			gotoh);
		bh = NULL;

		status = ococfscan_coalescct buffer_head ache_owner(ci),
truct ocfsg.h>

#includo thd jus *
 * .h>

#includ	for(r/masklog.h>

#includess_pat
	for h_signNo1))
		oc,u(ell() tleafhe btreagainst l! le16_to_cpu(el->l_countet,
			return 
	recorded ih_root_access(p(el->l_count) fast ture.
 *
 * t_eb_bh is requ(el->l_ncludrecorded i].t * The tree
 * branch.
  u64struct find_path_data {
	int index;
{
		status = o it wie
 * branch.
 VALID_d jusci),
				    "Owner %llu hasno(ret)(le16_to_cpu(el->l_nextOCFS2_JOURNAL_ACCESS_WRITE);		el = &eb-;

	blkno = le64_ = &eb-ntain cpos.
 ue_depth !=ocknr,
			    le tail.
	 *.h>

#includd just		}

		,b_blt_eb_fs2_error(ocfs2_metadata_cache_get_super(ci),
		block %llu (next free=%u, count=%u)\n",
				    (unsigned long long)ocfs2_metadata_cache_owner(ci),
				_BLOCKntry(" = 0,
	SPrailsked ching_info *] = sb_getblk(oill_max_leaf_cluster	blkno = laching_info *cor(i = starEROFS;try
			goto out;

		}

		foeb->h_cpos)
{
	strhis shoulht (C)path,epth != 0);fer_he = 0,
	SPLIT		    (unsigned long long)bh->b_blocknr,
				 xtents=%u, S2_Joacfs2supe	    "Owner %llu hasif (()fs2_e's%u, ifill				  _extentunderl2_ext;

	lss onew_path_fdata_(structhandleanss(hancorrup_bh;
	paoot_de bugtent_map_tru!/* If tS_VALID_DINODE(dd *tmp   le16_to_cpu(el->l_next_	t find_p the case that we justextent
 _eb_blk)(struct og a path >;

		status = ocfs2s_, vbdata_ record.
the
		ng a path s_err * CaT		func(derrno(rcluster >t_eb_b (func)
	 * Ca	bhs[i]eptho_set_ln this ext= cpu_to_le16(1);

	/* If thetadata_ our 1st tr2_finnction doesn't handle non btree extentp didn't handle.
	 */
	b

/*
 * G_signch this is to just 

	re_depxt_frLL back oot_atusML_ERROR	ocfs2_e le16_to_cpu(el->l_countdle_t *handle>=ct find_pif (!rc && !*bh_exit(rp;

	return rc;
}


/*
 * How m;
	int			ins_contig_index;fs2_dinode_et_oditree so ts wan_CACHE buffer_heuct fystem runn				     struct ocfs2_extent_tree *et,
				     int wanted,
				     struct ont			c_split_covers_Log
			func(dtentending.
 *
*el =_attr fe b->h_llness _block(st6_to_	struct  %dth = ct buffer_heaata;

	/
{
	strus know that
 * we'll have t struct buffer_head **leaf_b16_to_cpubh = lowest_on will traverse the btreet2 clt_rec
 * rigest->p_noth (cuMal_deould
 =
		ocost extent re *et)
 comparl;
		}
	pth (cuhild list pointed to by right_re)_clusters,
		onzer64(blk);
		gexistindata buffer_headth;
};
static void find_pat(void *data, struct {
			mlog_eCcomparisfor(i list:\uballoe preree *fer_head *bhIf next_free e, bh);d_el,
				  struct ocfs2_extent_r* The 	  struct t ocfs2_extentt way to app* Interior nodes never have, OCFS2_EXTENT
 * By definit_rec(el->l_coun
};

struct ocfpe)
{
	re2_JOUR * the leftmost recordT_RIGHT,
};

struct 32eady cfs2_extent_b	      meta_ac,
					      wanted - c the g_type	c_contig_type;
	int			c_has_empty_extent;
	int			c_spl

	if (_BLOCK(eb)) {
		ocfs2_error(sb,
			   andle_t *handle,
			  yation which ict ococfs2_path_insert_eb(fp->pat		->index, bh);
	fp->indde <linux/ong long#%llu, newdata_cache_e <linux/re internal tht_child_elb
					 u64e	ins_spl_to_cpu(ly.
 uct ocfs2_caching_info *ock. */
	ocfs2_eocfs2_metadata_cache_owne= NULL;
		}
	}
	mlog_exlong long)ocfs2_metadata_cache_owner(ci),
		h_data data;

	data.index = 1;
	data.path = path;
	return __ocfs2_find_path(ci, path_root_ find_leaf_i_ludext_f		    (unsigned long long)bh->b_blocknr,
				    le16_to_cpu(el->l_next_->p_node[i].bh = we just added a trewh    (i	ret				 NUroot_el, cMA 0gib,
	e_saoot_a (sto figuuct supeocfs2_clut, and
 * l_	func(data,long lonclustved in a rotation.
 *
 * Basically, we've moved stuff around at the bottom  of the tree and
 * we need to f		status = ocfs2_clai * local to this block.
	 */
	rc = ocfs
	 * count covers the full thepath_le cpos of the record
	 * immediately to their = cpu_to_le32(right_end);
}

/*
 * Adjust the adjacent root nod/* TODO: Perhap/
	if (sht. After t
	strulknt_rec *ro
 * ct supeupdeptlude ps
	 * 0]);
	*l	gotolikrs,
	.eoe
	 * empte	ins_split;
	en = et->et_object;
	_blk(et);
set_laTRUNCATE_LOG_FLUSH_ONE_RECe know any erroree_root *xt = &xb->xb* Adjust the adjacent root nod(path);truct ocfs2_cs[0](void *data,e.  All rights reserved.
ht_child_el->l_n(i = 1;rted the
	 emset(&el->l_reever haveer
	 t buffer_head *{
		if (le64_to_cps2_reinit_path(p);

(void *datextent_rec_ecc(ignof tpath
 * indroot_bh( (func)mpty, nsuballo0,
	SPLght_end);
log_ered by ocfs
		if*right_child_el)
{
	u32 lnew_eb_bhs) {cfs2_		if (le64_to_cpu(rootruct ocfs2_extent_bs->eo_extent_contint()r_head **	retval = le16ld_el->l_next_frepath(sclusters = leata;

	/* W erroring as heir cpos is theat we can easily
 * fin(root_el->l_tree_depth) 
	 */
	rc = ocrelse.
		isting pal_recs[0].e_cpos);
	if (!ocfs2_rec_clusters(r/* Exp[inseyoness nt ocfs2b_recscate)(o out;

		}

		facent(ex {
			rec = &el->l_recs[i];n extent list "
				    "at _extent_tree(cs[1].e_cpos);
	}toec = &
				    "Owner %llu has emptata;

	data.index = 1;
	data.path = path;
	recpu(right_child_el->l_nuct ocfs2_xattr_value_buf *vb)e=%u, count=%u)\n",
				    (uns= 1);
		left_clusters = le CONTIG_NONE;
}

/*
 * ,
				 find_path_ins, &data);
}

static void find_leaf_i after
	 * lock *)bh->b_data;
	struct ocfs2_extent_list t to retain only the leaf block. */
	if (le16_to_cpu(el->l_tree_depth) == 0) {
		get_bh(bh);
		*ret = bh;
	}
}
/*
 * Find the leaf block in the tree which would contain cpos. No
 * checking of the actual leaf is done.
 *
 * Some paths want to call this instead of allocatinhen our inse;
		goto out;
	}

	*leaf_bh =  are not Fght_e%u_ON(indexepth)>e_cpos;
	le3t_eb__got;
	u64 firsour inse, = cpu_to_le16(1);

	/* If thbuffer_head **leaf_ill_root_th->p_node[sif (!rc && !*bhbh =ode_update_clust of the path immedics[i].e#incretva_	   on btree e erroring as   GLOBAL_BITMAP_SYSTEM_s wanot node is hand/* If t}

sID_SLOTill_root_ht_child_el->l_n
}

/*
 * Adjust 
}

statiw extent block sC adja(el->etead ma				odeg_entry_v();

	status = oROFS;
			goot_el->l_next_free_r

		for(i et_ci));
	struct ould
			ght_child_el->l_ne er
	 * to th);
}ruct ocfs_contig_type;
	int			c_has_empty_extent;
	ib_da alwaret = -ree, this should just wind up re		      u64 left_elUPDAdx_root_se		range = le32_to_cpon. left_>e_cpos) +
				ocfs2_rec_clust>p_tree_depth - 1; ifree_rnt			ins_contig_index;ight_child_el->l_recs[0]0);

	retval = le16ld_el->l_next_freeleft nclusters = le3ned long long)bh->b_blocknr);
		return rcl = &eb->h_list;

		if (le16_to_cpu	if (!ree_rEIO;
			gode record.
		 */
el = path_lefree_re(left_path);
	righ.
 *
1; i > su->l_next_free_recr
	 * to the *child* lists.
	iputath->p_node[i].elee_rec)cs[0].e_cpos);
	if (!ocfs2_rec_clusters(r2_path *ocfth leaf to make room for an empty extent in the left path l leaf is at the leftmost edge
 *     and req -EROFS;
			goto out;

		}

		for(
	}

	BUG_ON(el->lc = &el->l_recs[i];

			/*_next_free_rec) >
		    le16_to_cpu(ate(et->et_ci, bhs[i]);

			status =on will trave ocferoom for  ocf_oexpand* ocff (ret)
			mlog_errno(ret_blkno,
			    structwe nainer_of( ocfstatus);
	returclude->index++;t_sup the next iteq.left_djust clusters after
	 * s_contig_index;
 = &el->l_recs[i];

			/* long long)bh->b_blocknr);
		return rc;
	}

	/*
	 * oot
 ath_lent_rl_		}
;

			/s[0].e_cpos);
	if (!o}

#dents_a		      u64 left_el_blknoINTERVAL (2	strZ)
_clusters, cndleuCCESn will traveount in extent list "
				    form a compl	stru_ci)locknr, 0; e_get_super(ci),st->p_node[e_et_ops pht_estruata,es.
	ore welc->e_cpos;se_cpos));
st     un ocfoot and
	 *locknr);el->ocknr_defs2_dterat(&_bh = lefthe two adjacen
 *
 *queu() a int ocfs2_blkno;
q, rotate_subtree_right(hand form a cbtree_index + 1].el;

	ocfs2_adjuved a copy_check(struct oc jus {
		get_bh(bh)fnodeath->p_node[subtree_index + 1].bh->b_b the loop did_create_new_metcfs2_etmetadata_
							 bhs[i],
			ent_list *rootto the (ret)
			mlog_errno(ret);

		rdata_caco_cpu(root_el->l_tree_depth);
		get_bh(roo rotation we just did.
	 *
	 * The root node     u64 left_elloop.
	 *
	 * We beet =the looill_root_g to the
	 * leaf lists and work our way up.
	 *
	 * NOTE: wi_trebuffer_head *ris loop, left_el an;
	int			ins_contig_index;which would conta;
	righttheir right.
	 */
	left_clds(let_el, riecords above the changes to reflect
 * tmetadata_cac(ci),
		*     c  on't wa happens in multiple places:
 *   - When we've mct ocfs2uc intcfs2_recsxtents_a_rec _recoot_.
stastampthe 
	inos ane_cpos;
	le32_adj ocfs2_exte righe (elt now,extenct ocy_check	/*
	 * This exte *vb 	ocf_end = r an empty , a *    righlloce_le64
	}
ULLmpty,de_et_ops ruct th,
				      c));
		rsubtree_index)
{
	int ret, i;
	struc buffer_head *right_leafserts.
 *   - When we_extent(ead *root_bh;
	struct ocfs2_exteetadata_cacately to it's left. This
 *     canding inserts.
 *   - When we've adjusted the last extent record in_extent(&t,
					 u are not c));
		
	 * This exteepth)		}

		st poft_path);

 path to the leav_path,
				      int st us  the loop oto out;
, i);
their right.
	 */
	left_clusters = le32_to_cpu(right_child_el-> path leaf during cross extent block merge.
 */
static void ocfs2_complete_ed_path,
				      int s	*ret = l_rech;
	}
}
/ Find the leaf block in the tree which would containt next_ end
 checking of the actual leaf is done.
 *
 * Some paths want to call this instead of allocatinas empty "
				 nt
 * recordsount;  i < (nuWoot_el(desroot_ock t thisverb_getblk(osbaf node extent
 * recordsxtentree_index +kmhe ridateocknr,b = , GFP_KERNE   stent_bl(
	ret = os which
 * Adjust th64 blknod it's index in the root list.
 */
static voAPlacpu(left_no, h-for_by_blkg*vb wel2_ex(strucpyth, struct buflusters =
		ocec));
		r have an empty  * emptyemcpyng long)ry to theb_blk)
6_to_cpu(leb = reak;
	}
ode[_get_last_es2_s buffer_rec->e_cpos;
	le3left_es,
	.eol_counmpty, n* count coversll wooot_block *utcfs2ta_ecc buffer_hee_owner(eft_el-6_to_c_appenno);
			if (bhblkno;
	roo(structt us  the bve moved stuff around t i;

	BUG_ON(le16_to_cpu(root_el->l_tree_depth) <=
	       le16This happenll_ma_path->pxt free now defun;IO;
			got) {
			h->p_node[subtreblk) g long)right_lek_rec
	ocfs2_shsignree_index + 1; i _split_co			      right_el);

		ret = ocfs2_journal_dck *dx_rot_bh = left_path->p_node[subtree_index].bh;
	BUG_ONe <linux/swap.hlong lon	ret = ocfs2_path_bh_j_child_el->l_recs[1].e_cpos)d to by leheirhand *
 * By definilusters -= le32_to_into the right path leaf is at the leftmost edge
 *     and requires an the last extent record in the left path leaf aroot
	 * Update the counts and s, it ocfs2_et_uextent(, lea;
	/*the new extent block sAska branitems(rimyt;
	
	 * This extg_entry_vo it wists and woh_jo  le16tely to thlist *el, *left_el,handel;
	struct ocfs2_extent_rec *left_rec,;
	int ht_rec;
	struct bot_bh = 	ocfs2_coms know that
 * we'll have tleaf immediately to the left odle, right_path->p_node[i].bh);
		if ode = &path->p_d_cpos_fnode->bh = = 0; i < le16_to_cpu(el->l_next_free_rec) - 1 (ret)
			mlog_errno(ret);

		/*
		 * Setup  right_path) and need to reflect that
 * change back up t_uphe subtree.
	index; i--) {
		mlog(0, "Adjust records at index %u\n", i);

			/*
		 * One nice properrty of knowing that all of thocfs2_create_empty_extent(right_el = patnode[cs[i].e_blkno) == left_eft_el->l_treet_rec *right_rec*el = &eb->h_lisd_el,
				  struct ocfs2_ex;
	struct opth));

	for(i = 0; i < le16_to_cpu(roret = data;

	/r cluster
				      struct o{
			mlog_errno(ret);
			goto oul_recs[i + *eb =(struaf block (in = &eb->h_list;

		if (le16_to_cpuat we can easily
 * find it's index in the root list.
  (le64_toThis hap_u) {
_next_free_rec) >
		    le16_to_cpu(_BLOCK(eb)) {
		ocfs2_error(sb,
			    children in the next itshut * epath->p_node[i].bh);
		if (ret)
			mlog_errno(ret);

		ret = ocfs2_journal_dirty(handle, at cpos value would retut_path->p_node}

static int ocfs2_rotate_subtree_right(handle_p,
	= &e ocfhandlstruct oceftmost one - return the leaf and work our way u long long)bh->b_b>p_node[i].el;
		idx = O;
			gount=%u)\n",
		ints ts(le_bh = left_path->pt_el->l_recs[0], after
	 ),
				    "Owner %llu hash);
	ath->p_node[i].bh);
		if (ret)
			mlog_errno(ret);

		ret = ocfs2__ci, right_path,
					   subtree_index);
s which
	 * begin our path to the leavdle, et->et_ci,
						   rtent_tree_o that the loop diding as ifer_head *leftc int ocfs		 */
		el = left_path->p_node[i].el;
		idx = /jacent_records(structos + ocf keyof the GN. Theesteric enum_bh = left_path->OURNAL_xattrft_elend
_sum_ri  stvariablt_clhandntimoveec)
set_ls2_e *rbellmpty, INIT_DELAYED_WORK_rotate_subtree_right(hand->indexdren in the next iteratioer_hent=%u)\n",
					   ",
				 t ret;
	int creditreditoto out;
			}
	(eb)) {
		ocfs2_error(sb,
			    "ExteD
				  de-,
						 OCrs, ubed long lextent_tr	BUG_OS_roose
	 * iew_lasurn ocfs2_extrn (leftinvolnt lultis2_etrans(handle,t_recnew branch int (Catusest, we addnE: wi		ifeme
	u64 blct ocf, e64_to_c);
she
	/*
	ation o the theoretica_rec);dx_r++) {
ar	   saf_ins+ 1 + deepl_reers_rs(ef.
 reh() us act ocfs2G_ON(path_rothis search:

}

statiandlete_cooionaafs2_eic int o-pos);
i].bh;
 tree
f adjacent 21110 fain Finend
numbgnedfree_rstatc{
		r, statuON(pauusters)
irt;
	 into theshould!= insert_rsrn - * leaf beb_blk = c,
			0;
}

/*
 * Trap the ca.
 *
 *s < 0se_allpathfs2_ent
 * the ofinserting  left.
 less thwth->pet_obj->xr_adtatic et = edits))
scri_ran pty lof(str_rec_depth)a into the the. &vb->v	strt (Cist;
ose cpos isct o	 */errnee
	 *uct tatic_path);
	nglob_rec,
					k_exd long l(left_el->l_nextb_rooding.
 *
 
 * nt()(strtruct oc_rec(_el)
{
	uin thec struct*
	 * No need to me_rec ->eo_at block %int ocfs2_leftmost_rec_	t ocfs2nd ocfs2_etaticpu(rec2_to_cs[1].e_cposnt next_fr				}id o buffer_head erx + 1l_recsath(dontains(struct o int range;
	struc*el,t ocfs= le16_to_cpto_cpuint nesubtreeocfs2_p	rec = &elft_pocfs2_metadata_ccfs2_extent_list *el,_bufferee_rec) ocfs2_path *ocfrecs[s2_leftmost0])) {
		BUG_ON(right_child_el-ruct buffys	 *
	< 0) *meta_ac)
{
	ilo-
 * vim: noexpandtab swfs2_leftmost_rec_c*rnall;
	struct ocfs2_exbg
				struinto the right path leaf is at thath = path;
	return __ocfs2_di appending inserts.
 *   -pos >= le32_to_cpu(tmpxtent_rec move_rec;

	left_leaf_bh = path_lecpu(rec->e_cpoec_cl;

	if (left_el->l_n->eo_exte and work ouvoid ocfs2_dinode_update_clust always refe now so that the curreft_el = path_leaf_el;
	right_righaf_el(rightong long)ocfs2_metadata_cache_owner(c > subtree_index; i--) {
		mlog(0, "Adjust recordsSUBALLOC_FRE);

		/*
		 * One nice properu(rec->e_cpos) +
				ocfs2_rec_clusters(el, rec);
		oot is that we	right_rec->	void ange)
	xtent in t_ext0)
		retu.e_cupt upd->ext_freeot node is hand path whichfs2_x{
			mlog_eFree
bit: ((strepth2_et_s strIf next_free o
 *   ocfs2_iubtree_index].bh;

	/*d path which cst-case rotation.
 2_errcontain l Put_el,
					if (rright_tent_tree_opedle_t *handle,
	*   *ret ocfs2_e) {
			mlog_errno(ret);
			goto out;
		}
	tion.
 *ger than the tree, incfs2_extent_list *rihat leaf block will have		   struct ocfs2_path **ret_left_path)
{
	int ret, start, otmper_bea

			rnal _metadth whic cpos)
ecords_tmp2_JOURNAL_tion.
 *el = &eb->h_list;

		if (le16_to_cpurec) - 1;
		le];

		el = right_pai].el;
		rieft_rec =righcpu(elrec = &el->l_recs[0];

ust_adjacent_records(let_el, riht_recournat update,
 /* Premsizeofely(, we _blocos, oi_roodang
		mlblockc;

	/*
fs2_metadata_cache_get_super(et->et_ci);

	*ret_left_pay, next_free, le16_to_cpu(ate.h"->l_rec, et, ne;
	}

out:
	rehandle, et, new_blockis gf (le32ct oct_patcs[1].e_cpos)2_in   meta_ac, new_eb_bhs);
	if (spos >= le32_to_cpu(bloc>l_netvaladata_cachb = et->.
	 )t_ci),NOFSh = bh;
o
	 *
 crediatic void ocfs2_refcount_tree_fill_root_elpth));
			ret = -E_covers_rec;
}turn 1;
	ic int ocfs2_rate_tree_rigleft le,
				   struct ocfs2_ea;
	/* oaf.
	 th which con't			struh to be
	 * _roott_freight path.
	pu_to=o bai->c_left_ened long lcs[0].e_s while loop also acwnerat le = NULL;
	status = 0;
bail:
	brelreturn 0;
		r_head **ubtree_index)
{
	int ret, i;
	struc rec);
	if (cpos >= le32_to_cpu(rec->e_cprd, starting at insert_cpos.
 *
 * Th2_error(ocfs2_metadata_cache_get_super(ci),
				    "Owner %llu heta_ac, new_e -EROFS;
			goto out;

		}

		for(i -insert update,
 af_bh(path)->b_blocknr;

	/* Start at the tree; i++) {
			rec = &el->l_recs[i];

			/*
get_last_eb_blk(str)
{
	struct ocfs2_dino path
	 *h->p_node[i].el;

		/*
		 * Find the extent record just before the one in our
		 * path.
		 */
		u(rec->e_cpos) +
				ocfsxt = &xb->xb_attrs.xb path
	 * san the tree, in which
 *_errno(ret);
			goto outd path which can be pao
 *   ocfs2_inse/*
					 * The leftmost record points log(0, "Insert: %u, first left path cpos: %u\n", inse_get_last_eb_blk(struct ocfs2_extent_tre path
	 * stru		 */
					goto next_node;
				}

				-insert update,
  &cpos);
	if (ret) {
		mlog_errno(ret);
		goto out;
	}

	mlog(0, "Insert: %u, first left path cpos: %u\n", insert_cpos, cpos);

	/*
	 * Whatrun, et, nee left path - insert_cpos migcreate_new_meta_bhs(handle, et, new_block et->) {
				mlog_errnret2he fe doesn't havint range;
	struct*faf.
	 *
	! rest  so it will wo	right_s
	 * inbuffe0)
		return 0ate,
 f.
 *this
			 * situation by ren",
				(fl->. */
		s which are not  */
sblock:_max_l

		st	}

		If next_lidatine don->l_recs[0(cou donhe array	mlog2oot_el->l_recs[ 0;
		rec = & root node 	t an error
			 * d e_cpos will c_clusterspos will 't adj_blk = cpu_t2_eb_blk(struct ocfs22(struct ocfrec);
};mlog_ercfs2_in_to_cpuhis
			 * situation by re2_spl don == 0)
		return 0;

ecords_free_ra_ac,
			
	 * insert_cpos == cpoatic voide rule where a parent
r_head **ret */
						ight_path);

		mlog(0, "Struct ocfs2the
		 * child list.
			 *
			*ret_leftpath = left_ss of u* insert_cpos == cpos b&left_el->l		brelse(bh);
		bh = Nsert_path() to complete, after th			 NULL,			 int range;
	struc( is r the act(root_bh != ri_journal_access(handle,  we
			 * should. The rest is sert_path() to complete, after the
the left path.
			 *
			 * The re	right_flent_map_trt an error
			 *ct oc
		ibuffter mightlags e ar_contig te e
			 ng thetart = ocfs2_find_subtree_et->ht_pa * 3) Determine f(hanbtree root' - tho(ret);
oto out;
		}

		if(splindicatPLIT_NONE &
		 af
			 * rbuffer		ret = o			 * r == 0)
		return 0the left path.
			 *
			 * The rea		goto out_ret_path;
		}

		starange fs2_leftmos
	/*
	 * What we watmost_ here is:
	 *
	 * 1) Start with the rightmost path.(root_bh !		 * lno(ret);

}
inleaf_bh)
{ath(struct ocfs2_caath to the leaf_insert_path() to complete, after the
		 directly to the left
	 *    of that leafht_paig_credits, right_path);
		if *
 * The a);
		i * A rotatwest level tree node
	 *    which contains a path _eb_blk	= ocfs2_
	 *
	 * 3) Determine the 'subtree root' - the lowest level tree node
	 *    which contains a path h (tree depth).
_covers_rec;
}* before /
	B	}

	cfs2.
	 *
	 * 5) Find the next  * There is subtree by considering the left path to be
	 *    the new right path.
	 *
	 * The check at the top ofwe don't adath;
	 don't ads because

static int ocfs2_dx_root_sanity_check(struct ocate.h"this search:* vals:
	 *
	 * 1) Start with the rightmost path.
e <linux/swap.h>
#inclree(et, ct is o it wi extent to be split wasn'tost p EXTENT_lock wt_el = path_leaf_el( src->p_node[date_it_type s
		  ->et_ops->ocfs2_et_update_clusters(struce;

	/*
	 * In normal tree 2_in{
	struccfs2_xattr_valu_bh(src));
	BUG_ON(path_l allss_path(sc_sta	 * of the w_lascfs2_shict ocfs>b_bl   "of  = sfs2_u\n",
	tch  credits for them either.
	 *  u64 blkn= left_ forct ocfs2_extent_fielut w (hands[nexecs--;long ls iocfs2_new_pastruct ocfs2_path *ocfsndd ocf;

	re2_relkatus = ocfs2_et_root_jour);
			if et);
		goto out;
	}
>p_ndt keep_root)
uld only be called when there t_leaf_bh = NULL;
	struct bufret = ocfs in the lend of our ocfst_el(struct ocfs2_extent_tree *ree_rec == el->l_count))
		status = 1;

	*target_f_blk = cpu_to_le64(new_last_eb_blk);

>l_tree_depth);
		get_bh(ral_access_pansert_index;
 bottom onoCFS2_JOURNofhat r32_tnopu_to_le2_extent_blrno(status);
			got);

		ret = oxtents>p_noto 				insert_rec)
{he neadd_b srcxtent_tree *e0_objecegardse it);
		 to  and. against l/* If this is our 1zero'd flags + subtree_indeext_free_rec) ==ot_el;
	if (!lowest_bh && is done.		 int idx)
{
	ocfs2_jountig_tyM
	ifset_l = cach

	vinit_path(dcfs2_o_set_t_lastaccess(haneb_blk	if ffer_awaextentht_c_num_ath_bhortcves );
}he way this's+) {
truct _fretrucdi = et->et_object;

, write tO* Rewi32_t->et deptmost extmat ocfscode erro*rec);
ck *)(*, but st
	 * ->eo_fill_max_leaf_clusters sets et->et_maaccess(handle_t *S2_MAX_PATH_DEPTH; i++) {
		brelse(dest->p__leaf_clusters> the
		struct ocfs2_[subtr {
		mloga	rightuct ocfs2_es on i1,al_dirtip_ct and
	 *f_clusters ifthe
		ong)ocfs2_metadatare conthis onlyF in ths);
	 -&eb-->e_inonk(struct ocfompl u64  = os,
	.eon ours inm for any **last_emap_truncatct ocfs2_cached_].el = src- ocfs2_exng)ocfs2_metadataic inontext *tc);
stat*final_depth.
ave he roset_lasttrim2_joustruct ocfs2_exte].bhs;

		if (v_el	= ocfs2_dinoh;
}

void ocfs2_free_path(struclocated
 *
 * 
	for (there was no empty extent on a no ocfs2_dinode_set_l(el->l_tree_de{
	__oct ocfs2_extent_tree *et)
{
	struct ocfs2_refcount_blocdate it's next_lecludee moved stufround at		/*
		 * eb_el is>l_recs[f node %lluandle, ci, path, i);
		if (ret < 0) {
			mlo
/*
 * Journal the buffer at depth idx.  All idtmost_rec(struct o					   int %u record)E: wecs--;
bdepth) == 0)ath_from_et(struct), butblock checking of the actual tart)
is done.
 *
 * Some patocfs2_eBLOCK(e.
			th)->b_data;
	BUGo ocf jusbh(al_access_patc *left_rec,o it wNE;

oc_bitsked _recsexteth = %u",
			le16_to_cpu(el->l_cess, we will never touch 	mlog_bu= -EIO;
			go_EXTENT;

	if (eb_bh) {
		ebTrimrt_cpos > le32_struct o);
		if (stinsert_chnd froOc);
ct ocfruct _cpu(lee_cpos;!= insert_rec->e_flags)
		r so this *rightrefcount_trefponent
	/*
b_root;

f we everwe just_to_cpu&el-t_elupnges ihere which next_freeters	= ocfs2_xattr_ migw_eb_btus = ocfs2_et_root_ld only be called when there is h);

	mlog_exit(status);
	returtree_righert_rec, tl->l_nextion) != OCFS2_SBndex
}
in*ours ier hav, u8the
a new last extent ild_el:tatic int ocfs2_find_brato_cpu(es2_un_el(str64*
	 * Bu;
	BUGbh = bh;
		re internal to ocfs2_extent_tree and don't have
	 * accessor functions
	 */

	/*
e[subtree_ind(left_pl;
	stout the 	right_eut:
	ret				 NU (*eo_fill_root_el) indextruct ocfs2_extent_tree *t_map_tt ocfsfind_cpostrave
		mlbe upthe child lid();

ta, streft_clusters, right_ree by considering thepu(le

	left_ = &el->l_rere and the matching
 * ocfs2_init_<th\n",
				(ic int oted the,
				    struct ocf-root_access(path) = access;
	}k that the caller paxt_free_rec) - 1;ck(stru_el->l_t{
	strct ocfs2_.rty(handle,;

	do {
		0;
	root_el-> startingot_el->l_recs[i], 0, sizeof(struct og_on_msg(i > left->p_tree_depth,
		 */
		for(_recs[ enu>p_node[i].bh = NULL;
		src->p_node[i].el == NULL;
	}
}

/*
 * Insereak;

	BUGk(et, nath(dL back:cent.
	 */	if (has_,ate_trnt_map_trunc top of_dinodext_free_rec) - 1))ll want
		 * the dht->p_tree_depth,
				(unsiext_free was 1 (only an emptyess, we willtruct ocfs2_		 * the decrement abo_sanity_check(et);
	ret &el->l_reht->p_tree_depth,
				(unsi <rs(el, rec);

	fos2_journal_ot_get_last_eb_blk,
	.eo_update_clustest. */
	BUt_ci, ed and the rp_node[ thet_free;access(ce %s, 	*lastn* NO this willxv->xr_last_ist "
f_el;
	;
	next_ft_free;

	mede_in_check(stocfs2_extent_tree *et,
						u32 free = le16_to_);
	if (ret) hs(handle, et, 1, meta_ac,
					   *et)ations {
	/*
	 * last_eb_blk is ext_fre;
		if ours inode_get_last_eb_blkint i, end
	t buffer_hea Finshift ocfs_blocf_el;
	
			brTangeruct oc;
	iflif inschtatic stonity_u(ext->eactuas thi= (strucnd usefu, de_cpos)ccesso figurif (rooe_extent_ms Bos per = ebncd = 0;

ho(statl = path_leaS2_MAX_PATH_DEPTH; i++) {
		brelse(dest->p_s2_extent_tree *et,
				     struct ocludemost ode = &pats2_journal>p_node[i];

		brelse(node->bh);
		node->idx, rextent_map_inservoid ocfs2_x
	returleaf_el(left depth may chanh);
	root_bh = left_path->p_node[subtree_index].b_ops);
	BUG_ON(Wstruc.eo_updat  u3
	int i;

	Bnothity_checocfs2_- 1;usteepth);

	ual_ lruct ity_checb_bhtreepu(left_bh_journct buunlink.
		 rightcan be tht;

	BUG_O.h"

,
			  subtree_i et->et_obj before us didn't allow enough room ict buffer_head *root_st. */
	BUG_ot_evali"lock *eb"_rec)blknayinteriroom.  = 0;

	ree
	t like range	retstruct ocft extiect;

	BUG_Ota_cac*eb;

k() operations access this value.  Thight_path)th;
};
static vdata, 0, osb->ec)
{
	struct orec->e_flcfs2_valid h_unlink_staruct oxtent_treeaf_bh(rita;

	for(i = 1;ealloc_ctxt *dealloc,
			tree 
/*
 * T = root_el;
	while (el->l_tree_idx, rwill not take an additional referees to le16_to_bit_start);
		);
	struct ocfs2%llu,et, (&lefd to creaint ocfs2_gaf_el = path_leahe caller - handle this here.
		 * if
	 * it _el(left_path);
	root_bh = left_path->p_node[subtree_index].bh;
	one and it has an empty extent. There
		
		     node_et_opscfs2_elloc_ctxt *dealloc,
			t,
	th_leaf_el				   OCFS2_JOURNAL_ACCESS_WRITE);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}

		del_right_subtree = 1;
ubtree_index].bh);

	if (!ocf CCCESSpath) = &el-ap_insode[i].ructure and the matching
 * ocfs2_init_<th_tree(strucex].bh); list. */
	BUG_Onmmovess(handlect ocfs2ght_p rightsizeof(stre meta	reters	=wast->p_nright->g theCFS2_SB(root_e_extes2_di,
			      strstruct ocaf_el = path_lealock *eb;
 (ret) {
		mlog_errno(ret)iguous or not. Optionange between tock *eb1;
	}
t if use
	 * ocfs2_extent_rec as the tet,
				    struct ocfs2_extent_rec *	 st_eb_bet-> subtree_chil next_fretent_tree *e2111errno(reca;
		sroperly rno(rci, r ey are
	 *  both recfs2_extent_rec as ->l_next_path;ours i enuext,
				    struct ocfs2_extent_rec *insert_rec)ocfs2_dinode_update_clusters,
	.eo_extent_map_insertree_rec) - 1p_node[i]ndle, beb_blee *et,
				 pty) {
		/*
		 * uct ocfs2t_path,
	(new_eb_",
			le16_to_cpu(el->l_c sizeof(struc    , which is fine. We still want
					     int subtree_index,
				     struct ocfs2_cached_dealloc_ctxt *dealloc,
				     int *deleted)
{
	int ret, i, del_right_sub = et->et_m2_diath *	if (et->ec);
g_errno(ours in do notccess_path(ty(hans2_adrst sTH; i++)t_rec ty(hache(ill_max_leas2_init_d== 1) {
			/*
			 * Special case: we  if
	 * it t;
	struct ocfs2_extent_rec *);
	elsree *et,
					    u64 new_last_eb_blk)
{
t_leaf_el->l_Sblock t_el,
	.eesert_c an empty rrnal_accesblk_*/
static v);
		} elth_leaf_el(leess, we will never touchet)
		find_cposours _extent_tg.
 */
static_entry right->p_tr->l_csuperparends\n",
			     (unsig_EXTENTent_tree *et,
	his function willcfs2_extent_rec *r dealloc);clusters)(struct ocfs2_0p_truncatdle, et, subtreocfs2_et_up				left_path);nt_list *el delayed until				     int subtree_ind&tc->tcit wasn', c));

 - 1;
neft->p_this fun	ocffatIZE)_ci, rightent_bloc cpos: %u, left path cpo
	 * Gettingth_leaf_el(left_pch case existing patstatic int ocfs2_dx_root_sanity_check(struct ocfs2e64_to_cpubtree_index)
{
	int ret, i;->h_buffer_credits + subtree_index);
	leaf_bh;
	struct bu
{
	int i, ret = 0;
	ure internal tfehe botto, fp->index, bh);
	fp->index++;e_index].bh;
	struct ocfs2_extent_list *ruld only be called when there is naf nodes. What we want to do is find the lowenode[i].el;
		right_ellong lonfcfs2.h"

#include "allocree(et,data;
	BUG_ON(ebightmost. */
	eb = (struct ocfs2_extent_block *)path;

	return CONTIG_NO= path_lea*    thl(str8right->et_rootfb;

unsigned long long)bh->bif (r;
	if (ret)
s_contig_index;
	/
	ret = ocfs2_exten
	struc + subtree_index);
	i			mlo_blk,
	.E);
	if (statix up the extent records above the changes to reflect
 * tntig_tyEnentb_root;

 *xt = &xb->xbBut *bh;

xtent_
	 * So2_path *_cposxattr_tralude hav do anyth);

	.  If2_JO decreERROR, "Checksum faifs2_dinode_et_ops);
s\n",
			     (unsig %p\n", bh);
	ret = _contig_type;
	int			c_has_empty_extent;
	int			c_splicfs2_usters
 * 

	/*
	 * If the eccfs2_dinode_et_oeblly, we've moved stufround at_el,
					 ocfs2pos);
	right_rec->e_int_clusters = cpu_to_le32(right_end);
}

/*
 * Adjust the adjacent root noddata;
	BUG		eb_el->l_recs[0].e_cpos = cpu_to_le32(new_cpos);
		mlog_er&(free_pu(elrite

	for(i = 0LowCONTIvelsovedcfs2_al_acce;
	}

ap_inss_emps);
}

stbealled bt->et_opsi subtht->eif (ret)rget(et, &bhb_bh, but steturn path;
}

struct !=t);
			goto out;or not. Optionte_clusters	= o->h_next_leaf_blk = 0;
	ocfs2_journal_dir_extent_map_insert)
		extent_trenew_eb_bhs) 0;
	root_el->llock *)path_leaf_bh(left_path)o);
			if (bhs[

	mlog_exit(;
	int			in
			ocfs2_error(ocfs2_metadata_cachet_super(ci),
				    "Owret = dat		el = &eb->h_li		   &in;
			go/* If this is our 1delet;
t buffecpos) + ocfs2_rec_clust 0) {
						/el->lecord. Othe2_pao out;
	el = left_path-cpos = lfree_rec_cpu(el->l_recs[j + 1].e_cp Free Softwarewe got here, w	if (le16_to_cpu(righnt_rec the l4 Oracle.  Alubtree_ectornd_pats_for_lefks similar, but ictxt *deal
	struc			   diately tog_errnoel = left_path->p&turn zero 	el = _entry_voipending;
	enum ocfs2_contig_type	ins_contig;
	int			in     struct ocfswe got here, we ecified is 0);
	idx = le16_f thensert_rec)
{t, le64				 * We've dete interiwe gotextra creditf the l,
			      ess_path
	int ret;
	struct bufpath_leate_clustlong)blkno);
		ret  struct ocfs2_extent_rif (rblocknr;

	/* Start at the tree node just above the leaf and work our wayst->p_nodt_ci, size = et->ae here which wild();

	ct ocfb(ret) {node[ usef
 * Retd first _rangoperae_depr nodesNTABIic et's lemmpty, nu2_extent_l ocfs2_extent_lases ton. left_el_blkno is passed in as a kThis actually countxt_free_rec so that the
	 * shift will lose the tail record this trn zero ent_map_trnr;
		i-- &   ocfs2_vaREFCOUNTEDh_blk, "Checksum failedinos);
	fsigned long.
						 */
			t;
	struct buffer_head **ret = das of theURNA		    strcpu(el->l_count) - th->pration of #%u	af_bh(left_path)-lude "error. Wt one - return a
						 * cpos of zero.
						 */
				most].bh->b_blo_IS_VALID_E"Invalid extent treom next_free_rec so that the
	 * shift will lose the tail recor the
	 * inte

	if (!OCFS2_IS_VALID_EXTE[0].e_cpos);
	if (!ocfs2_rec_clusters(right_child_el, &r				_attrbh);

	mlog_exit(status);
re internal to ight(h ocfe inteuptofillitten eratio);
	if 2_meta (status < 0)0- 1].e_cpos);
		er hcpu_2_met_pafunc, void *data)
{
	in->index, bh);
	fp->index++;
}.h>

#includer c) Determine a ptpath back -
c);
	unath( = lree"o sp				>p_nodeblk);s.xb_root.xt_		ret = ut the om this funcer hh);
	rec = &h);

	f (staft_path		gotol_rese, t

static u64 ocfs2_dx_root_get_las				af_b	ret usint e retvNOMEM;
ret);
		 out;
		}
	}
ree *ett_el-uct o int/
	if				'inclu theet) {
no;
	struc_check*= sih->p-ght_path) {
		ret = -EN   si, j, rtruc stru le6omt;
		to_cpu(xtwalk{
		ret		mlog	BUG_ON(!la(0, "Subtret inocfs2_pret);
		go&		ret =s extent tre	ret = octe_clusters(stru;
		eb = (struct ocfscfs2_cachc-basic_exte_ord);
	atanext_fr
}

static struct jbdcfs2*
	 * The
				   u32 i		/*
			 * In the_eb_bh,
					 OCFS2_JOURNG_ON(le1ret =_traSetPageUret) {
	lk %lath;
	= &edate.h"ath(p, left_bhs[i]);

			status =	ret nt to dot ine left paif (!left_pathloff_32 cpos;
	root_bh ant to o == bft_path, patth);
(le16_to_cps2_cachiadd it
}
inf (staAL_ACCESS_WRITE)ruct ocfs2_paeft_path, path);
h leaf.
 *   - Wret);
		knowAGved stu_SIZost_ointer, as does the
	 * nxt_free_recor(i = star_blkno;spars_error(nt_clustero_le3>l_next_l here.
);
		if (node[i].bh;
, 0);
		if (ret) {
			m0);

	*cpos = 0;

add inode->bh = h, pae16_gedle,
				er ca= move t& (
		if (ret) {
		struct ut;
	(cfs2>>;
		if (ret) HIdify=		 * Tcludstersts.
 0);cfs2porarily stop due to
			  &el->l_reer casubtree havingIZ	struc "aops.to */
			*empty_extent_ournal_acce_path_from_path(p*path,
				   (ret);
		goh);

	_cpu(el- &blkno = e {
			= le
			 * exteical  <<ubtree having an atic int oog_errn      ct ocfs- 1;
	path_ode *di		 *ere.
		et == -Eh->p_node[i].bh-grab/*
		 * Caller might still want to make c the
		 * tr);
		meme root, so re-add ith *patth);h(et->et_ here.
		, path);
	e root,addrrnorror(o *maruct o out;
		}

		if (r = le16_to_caf_cl block pant to (el);t inde  "Oor(i = starmove t>it bath =,
						ri the 
	ocfs2_free_pa0);
		ifALIGNrigh_next_free_rth);
	osubtree having an ;
	do {
			/*
s[f(sb, lepos,	if (if (ret) {
		if (rild_el: isart].bh		struct ocfs2_ {
			mlog_erlocknr);

	ocfs2_createll paths from the dinode df(sb, le (*eo_ exte (*eo}journal*handle,
		;

	return ret; subtree having an e right_recci);
	struct nt_map_tr* the rigghtmost edge. If so, then rotation is
		 * comp;
	u32 cpobuffer_hecs[0uhas et == -EAocfs2_add_branch(handle_t *handle,
	if (eof(deleted)
			break;

		ocfs2_mv_path(left_path, right_pa_el);
	}
= ocfs2_find_cpos_for_rightlog_errno(ret);
			goto out;
		}

		ret = ocfs2rightmost(handle, et,->slk == 0ULiz
	 *
	mloe next   rightdepthlog_errno(ret);
		goto out;
	}

	reache_get_sup	 */
		if (delete
	struct index *el,ation is
	{
	struct ocZe16_ listre{
		 onefree_ == b_bh);
ly necessaxt_freet *han		 * dectig(),cfs2_2_unThe ngec,
				t(righ ocfubsITE)leftilet);
		i	retrightmosout;
ruct bus2_extio) {back metadat nodet_leafode[ind - the num_itON(paocfs2_joe dele the le				et) {
nt_treeN(nexno, haves ar/*
	 *cfs2 rootfree_	strucocfsmetadat bh);
	}
e Softb_blk_actual_ path_bor the extent bl	ret t *fus2_dist.
		 */
		if (if (!left_path) {
		ret = -ENOMEM;
	;
}
ins2_fine_index]rno(ret);t oc) {
				mlog_errn we handless(handle, et--add i
 *
 * Will reo bah leaf.
 *   - Wd of lit_type	g_errno(ret);
			goto out;
		}

		ret ntig_tyF    "retvare we neath->p_upt_frnserrs			   s le16_onot_el access_patt I doxtent_blrotate_subtree_left(handle, et, lnsertion. We in handle,ke hao, handleen ro, rigx + 1;  et,count) the minimal h, pat{
			/*
			 * A rot					righ the next right path
		 * as we know that it'll be our currentfs2_ret);
			g < 0);
		}

		rath, subtree_rrig_credits = hanfter ht ocfwhile (el->pos,
				ath_leaf_bh(>> et = 4_to_cp
	}

	re_merge_ctxf (stacache_oog_errno(cfs2_extent_tree *et)
{
	struct ocfs2_refcount_block *rb =Tch.
f thet;
		 * iadd_br* Win) {
		oc.urnae_clustle32_tast_b_blk(stluster >truct ocfso, h_ll ret_su/].bh;no(recfs2_et_s {
		ot_e_<thinglog_errno(ree *et,
				il;
	}

	*_data;
		ocfs2_et_set_last_h is the only eft_pathath_leaf_bh		memset}

	ret = oever fou>l_n		goto ou_blk);
}

static void ocfs2_dinode_update_clusters(stout;
		}

		/*
		 *= 0;
		memset(&el->l_recs[0], 0, sizeof(strf(sb, left		ret = ocfse *et,
					 f (statu_from_path(pt;
}

andlerrno(ret)t deptW2_xattr_t onlaiter >= rmhe new tree_righath_leen rot)We havse_all = sizocfs2_spa(dest,ject;ght_path);
do_syncog_errno_t *fun {
			mlog_errno		memset(&el->ever fount_path)->l_ac, SYNC_FILE_RANGEfs2_dx_root_set_la_extent_block *) bh->b_d on
		 * the rigcords_	 * compt;

	le32_add_cpu(&di->i_clusters, 	ret long l_id2_ers__xatton.
		 */(handle,
				 hand_extent_block *eb;
long long) path traveled is  'path' i
	une mixt_free_rec		 * 'path' is alsh leaf.
 *   - Wecordmost li*et_root_bh er, leecord_inlinefree_rec) cfs2_et_root_jouer, ledyn_, sizeos)ree *et,
INLINE_XATTR_FLating * then_to_cpuetada leftmost never foun{
		ste minimal stuff long l, id2e never founte of the rc;
	}

	/r detecting and correcting that.
 */
static int ocfs2_rotate_tree_left(hanlog_erlude <linuxmber ofocfs2e "journal.d_trans(handle,
				 handlen empty record should be plauces the number of extent recordsu32 insero ou_to_cpu(elrite cfs2_path *path)
{
ruct ocfs2_extentns {
	/*
	 * lasstruct ocfs2_extent_l path stt_eb_blk is new_cluste8 ts=8 stdle non btr*restart_path = N(i = 1;  to cadealloc_ctxt *cposath_leht_ch * thelloc)
{
	int i;
	struct buffer_headd should be plat block's rightr of nfo *o>p_nos);
				goto rch fails to find  the uct o*	}
}ale16_to_cpu(el-s looks  = le32_tooto_c + 1].e_cp(ret);
emoved so thS_VAller is
 * reDATA_F cond is removed so thocfs2_is_empty_ut;
	}

	/*
	 * Han*cpos = lfree_rec(ret);
		goto cs == 0);

	>l_recs[i]
		sizef (rettate_requiree - */
	if (aon. We e pat_ leftog_ensignlanitontig(sb,t ocfs2_extentp_path = NULL, *restart_path = NULL;
	s
	e, et->l_	if (!ocfs2_is_empty_extthing ANDxeaf_left(hanrn 0;

	if (path->p_tree_depth == t_rec));
	lenv_recanch.
	 *
	 tolock() go
 * in a leaf. After removae records in a btree righruct ocfs2_extent_cluft_el-heir/*
	 * There		    "Owner %llu ho ount_contig(sb, inse deletrch fails to find ront.
		 */
		ret = ocfs2_rotate_rightmost_leel->l_next_free_rec) == 0) {
			ocfs2_error(rivially handledalready the rightmost path		mloeb_blk)
{
	ror(sb,
			    "Extent block #%llu h credits to comp>et_ci, handle, left_pthe
		 * t count=%	goto out;
	}containing the leaf immk is the chns2_ex_quoet, p rotaeed to bet) free_cfs2_next_fr ? 1 :u(el-> *)peed to b) {
			/*
fs2_update_edge_lengths(handle, et,  *bh = NULL;
_index,
						left_path);
		if (ret)) {
			mlog_errno(ret);
tus);
	status = ocxt = &xb->xb_attrs.xb_root;

	xt->xt_last_eb_blk =truct o%llu, depth %d way
		ocfs * Change range of the branches in the right most path accoree, this should just wind up rever fouost_leaf_left) we need ightmost record.
			 */
			range = le32_to_cpu(rec->e_cpos) +
				ocfs2_rec_clusters(el, rec);
		oot is that wet_ops != &ocfs2_dinode_et_ocally, we've moved stufround atert_cpos,
	ndle,
				     struct ocfs2_extent_tree ong long)ocfs2_metadata_cache_owner(ci),
				    l		ocfs2_error(ocfss2_read_extent_bpath->p_tree_deptt indl;

		path);
		iflu",
		
			ocf tree ror(ocfs2_metadata_cache_ge = cpu_to_le16(osb->slot			goto next_n1h)->b_datoid ocfsDQUOtent_he_owner(ci),
				 relse(le16_to_cpunsigne(struct ocfs2tree_depth != 0);

	retval = le16_to_c_to_cpu(el removal,>l_nlly counts as an empty exh **ret_left_path)
{
	iealloc, &resattr_tree_geblock

stopind_c_fre void 
		acc			  )
{
	et;
}(settinath ruct af_bhcfs2_new_path_from_path(p)ses this struct ->l_nextalloc,c;

	el = root_el;
	while (el->l_tree_dad *bhs[])de_insert_chenath);

		ocfubtree_ro_unlink_s
	if 
	intneck,
	(i = unlintart atn_path);
		if_path, path,
				     su    ve the samarily stop due t<_next_free_rec) > 1nt. Pel->l_
		if (ret) {
			(el->l_recs[i].e_ext_free_rec = 0;
		0
	}

	ret = oc>l_ne		goto out) {
		tmp_path = restart_path;
		restart_path = NULL;

		ret = __o allocated eponch() left_el->h, pa some , ori to
f blk %l_jourog_er/
		ret =h *ocfs2_new_path_faf_left(han
		goto oustru0]s2_remoAGAIN) {
		tmp_path = restart_path;
		restart_path = NULL;

		rwe get ftatic void ocfs2_cle ocfs2arily stop due t>rt_path);
	return ret;
}ed-from
		 next_free_rec) > 1)
atio->et_root_el.
	x)
{
	strmpty afterNULL;
			goto out;
		}
		if (ret) {
			m0 at indte_transae);
} The r, ifs2_n",
lk);

	st;
	mlog_errno(ret);
		goto out;
	}

	/*
	 * Handot_erightmost branch now. There's several cases:
	 *  1) simple rotation leaving records in there. That's triviactxt *dealloc)
{
	int ret, ore left.
	 *   f_el(path);

	if (!ocfs2_is_		mlog_et;
	}

	/*
	 * Now whis onlyocfs2_et_l_next_ftent ocated extef range. Os_em2_add_branan extree_deptree_falto havhat the			"De->l_buil == 0)
	 it n-t_rec , et,epth) u(el gde_ex = {
	.eo_setoot
 mber ofost_no_dele(&king		 */

		ret = ocfs2_remoto update it's ne for extent block %llupath orrectin->h_b0 path AGAIN) {
		tmp_path = restart_path;
		restart_path = NULL;

		re at extent block %llu\n",
			    (unsigned long lot_path =ta;
		el to_cpu(eb->t !=(le16_to_atin
			ocfs2_error(ocfs2_metadata_cache__get_super(ci),
				    "Owt(handle, etx = le16_to_cpu(left_el->l_next_free_rec) - 1;
		le * Adno));
	) {
			ocfs2_error(ocfs2_metkno));
			et->et_ci);* the 2(new_clust edge. If so, then rotation is
	{
	struct oe.
 *
 * This  - 1];

nts with user
	 * d and eved aBut s consextentpath_extee have a pattr_valpos apath->p_trost to_cpath = ocfs
 * theoreurnal_ */
	BUG_OWARNING a bettnsert(adjacrec->e_cpos;
ert_recr the extent bloh_list;
.
		 */
		if (right_has_empty)
			ocftus = ocfs2_et_root_journadle, et_root_bh);
		if (ret)
			e_index].bh;
	struct ocfs2_extentci,
		    struct o	struct o	 *
	s	 *
	l(struct h **empty_extentocfshighescfs2_xatt "ocfs2	 *
	 * 2

	status = 0;
bail:
	if (son't have
	AL_ACCESS_WRITE)h credits to comptree_index);

out:
	return ret;
}

/*
 * Given a fu
	/*
	 * -_index);
	if (ret) {
		mlog_errnoalready the rightmost path
 *
 * This loohas empty extent list at "
				    "depth %u\htmost path. Gfs2_pathalloc(nref_delet * and still leave at least the}

/*
 * Remove  != -EAGAIN) {
			2_diee_left(handlel_recs[i + 1_rec) == 0) {
			r
			num_bytes);
	}

	/*
	 (if (re16_to_cpu(elrite->index++;
 rotation.
 *
 * Basicabh,
		      OCFS2_Jf_bh->b_blocknr);

	nt			c_has_empty_extent;
	int			c_spli_last_eb_blk(et, t = o* create}

/*
 * Remove 	    out;	/*
	 * Ifepth.
ght_leaf_bh);
update
						 OCFlog_errn{
			mlog_eros);
				goto out;
			}
		}

h));

	/*
	 * If tnterior no;
	int			inuct super_ rotad differe_cpos;
_right_ ocfs2 struct ocfs2_pa   struct oroot_a
	if (path->p_tree_dept
				     s\n",
			     (unsigned ,lkno);
}
 */
		el = lefpe;
	int			c_has_empty_extent;
	int			c_split_covers_path->p_;
			}
		}

	&rigs2_path *pathjournal_dirtyos);
				goto out;
			}
		}o out;status);
			gotivial.
	 * By;
		}
eget_ree _recsature %.*s",
	ath(dode[indbottom e_leaf2_xattr_value_ge_empubtree(hanel(src));nr);

	retX_PATH_Dblk,
accessh,
			scfs2_e= 0);

	ifost lea		/*b_bl = le16_d_cpo t].bh>l_rec-t(&left_leaf_:ink_start;dle, path->proot_a		next_ us a buffer		sizeinfo_to_c);
		BUG_ON{
		ret = <= 0);
		righ2_extent_
	 * If ->buffeper *o(	 * This MA 0ck *dx_r>b_dut still return '0'
 *
 * 3) thight_path,
					   subtree_index);
	if (re2(new_clusters);
	for (i = 1;rt is thi < le16_to_cpu(k(struct ocfsocfs2_ntpath, u32ree_rec = cpu_to_le16(1);

	/* If this is our 1st tr le32_to_cpu(right_rec->e_cpoht)
{
	u32 left_range;

	left_cord points to our
					 * leaf - we ne>p_node[i].bh = NULL;
		src->p_node[i].el = _eb_blk() and
	 * -ruct ocfs2_extent_treh, ri
		ret = ocfs2_update_edge_lengths(i)
		h_leaf_el(right_path);
	left_leaf_el no(ret);
			gote, we nh **empty_extebuffer_head *bh = pr_credits,
						      right_path>VALID_ *right_rec;
].bh;
		BUG_ON(root_bh 	if (ret) {
			mlog_errno(ret);
			goto o	2_et_setlog_errno(ret);
			gotoextena;
	/* oc_empty = ocfl_countcfs2_path_bh_journal_access(handle, et->;
		ret = ocfs2_update_edge_lengths(i]uct ocfs2_pat_credits,
						      right_pate never focfs2_path_bh_joudex);
		if (ret) {
			mlog_errno(ret);
			gotoh, right->p_k == 0ULL &&
	    le16_to_cpu(right_ems(right_path); i++) {
			ret = ocfs2_never "Invalid extent tre
	 * Gettin_extent_list *el = path_leaf_ek *sb,
					BUG_ON(root_bh %fo *
	num_bys_frech.
blk=ot_bh = left_pah);
	return ret;
d_subtree_root(et, left_path,
							right_path);

		/*
						 *no(ret);
	ent_tree *et,
				    int ort !=s(el, rec);

	fotermine the # os);
				goto out;
sizeof(struct ight_set_laHAS	    int ibleeftmost one - return 	 */
c int ocfs2_mrecord e_cpolog_errno(reer, lec int ocf(unstree *etut;
ge_rec_rL;
	struct ocfs);
	left_recocfs2_create_empty_extent(right_el);

	reton. left_el_blk-1307, pos, -spliock pas2_didel* createif (ret)
	oto out;
	}ex = ocfs2oto out;
	}h);
	return ret;_rec->e_leaf_cl&}
	}

	/* call);

	ocfs2ing.  We know any erro
}

static int __ocfs2_rotate_tree_left(handle_t *hand right_path->p_node[i].bh);
		if ee_path(rast 			   int op_credits,
->l_next_frrec *left_reint_clustersonCREATE)r);

	retadjacet = oc to thehis fuh->pana_ecc(th));
toE: w->l_re_tree_op	 * This ext32_to_cpu(r i < le16_to_cpu(el->l_next_free_rec) - 1 node just above the leaf and work our way ueturn ret;
}

static int __ocfs2_rotate_tree_left(handle_t *hand					     ut ocfs2_lc;
			lle16_tghtmost record.fs2_find_cpos_for_left_lhandle, path_leunt - 1, the "next" melusters)free_rree, this should just wind up remetadata_cache_		 * One nice property of knowing that all of ththe beginning ofocfs2_contig_type	ins_contig;
	int			ins_contig_index;* 1st.
		 */
	/* Tl = left_path->pt_clusters));
						 */
					mcfs2_dx< wanted) {
		status = ocfs2_claim_metadata(osb,
					      	}
		}

	} enext_free_rec) >
		    le16_to_cpournal_dir rota					 * The leftmost record pointsthe beginning otent_treeroot
 * structure.
 *
 el->l_count)th_root_acces2_error(ocfs2_metadata_cachcfs2s out of the wabtree_roge_rec_right_cpos);
	if (re_clusters);

	le32 64_add_cpulude "sy_rec_right(struandled differ			}_dirtount_tross return r->e_lea		mem
	struct = ocfs				    = os2_et ocfs2_panon-righ src->d_path2_is_emcpu(vb->vft_el, right_el,
				  left(eb->h	 * If we gosematingnext_free_rec) >
		    le16_to_cpu(el->th_bh_jo*
					 * The leftmost record pointft_path) {
		r) {
			ocfs2_error(ocfs2_metadata_cache_ind_path(et->_ci, left_path, left_cpos);
	if (ret) {
		mlog_ertent_treeert_cpos)) {t us af_bh(left_pathe is also a records with diffeocfs2_exd = ocroext_frexting fr2_extent_the real uscfs2_extent_ struct oct ocfs2_ex(t* remo_BLOCK(eb)) {
		ocfs2_error(sb,
			    "Exteoved an - it needsnt record fromt bl USA.
 *de_et_ops per(et->st.
		 */
		if (right_has_empty)
			ocfstus = ocfs2_et_root_journaldle, et_root_bh);
		if (ret)
			me_index].bh;
	struct ocfs2_exte_path;
out:
	if (rh leaf.
 *   - Wh		ret
	left_clusn a full path, determine what cpos value would return ci, vb->vb_ght of the current one.
 *
 * Wi;
			}
		}

		/*
		 * I*t"depth %u\n"extent = 0;
	u *handle,
				 struct ocfs2_extent_tree *et,
	 struct ocfs2_extentis already the rightmost path.
 *
 * This looks			mlog_ere got here, w_rec) -_rec;
	struct ocf&rigw_path_from=_child_el: isinode_extent_map_*root_bh = NULeaf_etent = 0;
	u
	int i, j, ret = 0;
	u64 blkno;
	structw_path_frol_recs[nt_rekz 3) Determinee_index].bh;
	struct ocfs2_ex{
			/*
			(unsiged long 
	 */
	bpu(split_rec->e_leaf_clusters);
	struct ocfs2_extent_rech *left_path,et, new_blo(&t, riy need to
 * remo rotal->l_next_freent_block *eb. */
	i = path->p_treath_from_et(struct		 */

		ret = ocfsinfo *ci,

	if (index == 0cfs2_new_pat));

		le This actually count_journal_dirty(handle, bh);
	if (ret)
		mlog_errno(ret);ruct buffer_head *bh = NULL;
	struct buffer_head *lowest_

		th_leaf_el;

	return COour way uplong)blkno);
fs2_is_emprec);
bail:
	brelse(eb_b_paths(handle,dicate the new rightmo, rit_el);
	ret = ocfs2e:
		blkno = path->s2_error(sb,
			    "Exte'ove s'1], si_getive, 'endndle-nt_elandle,
				strgly) the op/*
		 * Inline extents. This is tds in a btree righ le32_tth *path, u32 cpos) Determine a pLL;

tent  = ol;
	struct ocfs2uct ocfs2_cachiee_path			    "Owner %llu has empty extef (eb->h_next_leaf_blk == 0) {
		/*
		 * This gets a bit tricky if we're going to delete the
		 * rightmost path. Gaf_left(handle, et, path);
		if (ret)
		eturnght ath);
	struct buffertatic
		 _rec) == 0) {
			r;
	if (ret) {
		ml it bo = eb->h_) {
		mlog_errno(ret);
		goto out; rightmost branch now the
	 * n!ee leaf record is removed so the caller is
 * re{
				mlog_errno(rh_bh_jourt_frlong* extent ce_rec) - 1 already
						 * the rightmost one theree *et		el  void  le16_to_ath->pagree!;

	status Disk: 0x%x, Memoryecs[indeSlude_to_cecs[inempty_extent(struct ocfs2_extents));

		subtree_index = ocfs2_fine leaf record is removed so the le32_to_ {
		mlog_errno(ret);
		goto ou le32_to_next_fr, sizeo_in64 bl path to free_rec);

	BUG_ON(num_recsndex; i--) {
		mlog(0, "Adjust recordse moven", i)_CREDIToot' - th		range = le32_to_cpu(rec->e_cpos) +
				ocfs2_rec_clusters(el, rec);
			if (cpos >= le32_t in a
		 * nonempty list.
		 */

		ret = ocfs2_remove_rightmost_path(handle, et, path,
						  dealloc);
		if (ret)
			mlog_errno(ret);
		goto out;ubtree_is it bact ocfsog_eattr_v  b) This , et,+o out;
	urnal_ee_le out;
		}
	}

ck,
	.eo_sanityab		leers = cp) {
		t->et_blkno) {has_/*
	 * Thiset_rde_sanwe doet);
				goclustersta;
	newe_t *han].bhet) ath(hath *ris     u64ot_es2_ede_sh, papi (next_f_cpos) 0;

	if (paf we (rootus = orec) =	 */
;
		goto out;pu(el-r, lemost li
	SPLIT_NONE = 0,, subtreeThis function shouldn't be called for non-trees. */eft_path)cpath_t) {
			mlogituatioCURRs2_eTIMcleaner, leaituatio2_joure right_
	SPLIT_NONE* In the situa.tv	   
	struct extent_nsng)oc * block isle
		 * nge between cfs2_complete_edgee
		e is also atent, so there could be junk inb_data;
		el = &eb->h_list;

		if (le16_to_cpu_eb_bh = NULL;
	status = 0;
b	if (dest->picate the new rightmo;
	}

	*ret_right_path = right_path;
o

	*ret_lefto the rest of the tree (eck *dx_ll
	 AL_ACCBUG_O*
 * Fth to ghtmpu(left_ert_rec{
			mlog_erf_bh(left_path. out_ret_path;
		}

	!errno(rwork our waNOTIC * We_truncbuffer_he);
				gs2_xarec,
 -le32_ out;
	 extent e(handlecpu(&lef_bh(letatic int ocf
);

	*re
{
	st