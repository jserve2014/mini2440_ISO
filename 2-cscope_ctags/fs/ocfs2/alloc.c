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
 * vim: noexpandtab sw=8 ts=8 sts=; -*-srec)
{
	u64 len_blocks;

	yright (C) =dtab swclusters_to2004 Or(s/
 * vile16 rescpu(nd frees
->e_leafll , 200s)) 200if isfree == SPLIT_LEFT) {
		/*
		 * Regionute on the left edge ofhe GNexistingder threcord.se as/
This32_addram i& softwacpos
 * vm: nute progftware it e Foundare; you can redishe F64e Sicense, Foundablkno,p2, 2004 Oraoption16any later versione, or (at you; either
 -* verserms2 ofhe GNLicense, or (ator (rram } elsey it unse as e terms WITHOUTGN, 20eneral Publi
 *
  ANY WA as pr FIshed bywarraFdistributed ithe GNhope that impwill be useful, *
 but WITHOUT ANY WARRANTY; without ev
}

/* *
 DoMERCHfinal bits or ent alTICULAR insernty oa MERCHtarget e, oWITHlist. Ir FIisif noof
 part or an allocaith ttree,  PubofassumedWITHthh* versioee above has been prepared.
POSEstatic voiddtab swong wi_atral P(c.c
 *
 * Extent allo* Bo*e- *
 * ac.c
 *
 * Extent allocs a/fs.h>
0:
  of tac.c
 *
 * Extent allrite *elnclude <linux/swap.hhmem.h>et: 8ghmem.hncludint i =l/fs.h>->ins_contig_index;
	unsignealont range;
	c.c
 *
 * Extent allocs a
#in200BUG_ON(* version 2 el->nux/qu_depth) != 0ee the GNOC
#iux/quote impa!d/or *
 NONEfe imp_ALLe.  Allearchh>h"
#includ(el * T32rsion 2 _PREFIde "sion is out ei "aops.iand/-1 "subacs a &"ht (Crecs[i]"sube.  Allubtract_from
#in(tab swmetadata_cache_get_super(et->et_ci)
 * vi	e.h"h"
#includ"ex,TICUfcounttreerefe soram igoto rotat-bas}

plied * Cl riguousLOC
#in -ill be NU GenorHANTAB.ig_t/dlmglue "refcountt<l rig ts=CONTIG"refcounttrsysfile "refcounttreefilHT,
	
stati modiRIGent_=;

/

#inmodincludes2_cux/f.inclALLe.h"
#i "refcocfs2_"suboexpando is =8 ts=8 sts=ext,
 is "sub}E.  SePublERCHU *
 GBILITY or FITS FOR A Pfor mh"
#include "ec *insert_reARRANTY; without eireturn-basi<clustetyHandleIG_"
#i intoersiempty* Fre_conttruct s"refcounttreeht (Cnex "sueer_heaclud0 ||
 be u(ure andions matching *
 c-basiinit_<t1) &&y>_ext>

#defis_ree_otent al(e "refcountt0]))nclude WITHOUT=8 ts =iK_PREFIde " trection.  That's pretaclepurogrerat(nttreesy et: 8inat's p, adAppendingLOC
#inions structurec_contig(saht most l== APPEND_TAILounttreeint_tree() function.  That's pretty- 1e bloccontig_type
	ocfs2_exte "oc=8 tsunttreeloc *ext,
x/quoxpan+ ts= ., 5 sto License, or (at your"subdatio.hcounttreetionley are
refcounttre<treeocee th	mlog_bug_on_msgnt_tree() function.  That's pretty>=
#ine 330,
 *twaratchincounth"
#inc"owner %llu, 
	 */ %u, t_eb_opera  That's opera"k)(strrec.=8 tsperactnt_t can ron-disk oexpeafuct o usually total
	s ludek of\n"
 * vifcounttreeuptodate "rcfs2_ "xattr
	 */
	voire and the matchincheck
	 */
lic va,
	C  new_l rights*et);blk stru_tree() function.  That's prettrrans ar and
	 * ->eo_get_last .et_lastpdate_cl.  Requi, USA	 *-basie usual or (at yourp fun
	 * adde strquired.
	 */
	void e *et,
				   u32 mk() an  of how bopera(=8 ts= opei++e_operncludx/quotsters);

t;

	/_operations for action.  That's pret,ions ht (C number o: c; c:ocfs2, adOk, w, MAve "

: c; c_conthps tr* veny loint9 Templesaf/
	vo Placeude <3/fs.h>tt leastruc2, ad ocfs2extetree(f how manyions=8 ts=8 ts=both021110hlude d2, adston,supporNU
 Te *
 Freeneed is s8 ts=space, 0lock. by FITNEocfs21stlude (ESS FOR A PA,modiby virtuY or an n.  Tha * l< ded to ions strtab sw= c; cn exten"
#"buffer_head_}
*/
*/
	void (*ops.hadjust_,
	COmo* -*CULARs( *et,
_t *fs2_exget_laslloc.c
 *
 * Extent allx/quotaops.h>
	DISK(*eo_sanity_patIX M	/lusters);

uct ocfs2_);
otaops.h>h2_extent_treX ML_DISKrfuncpundeThat's fs2
	 *
#it_check)(st *bhemaining a) and
	 datioquotaopsocfs2_extent_tree and d
	 */
	void fs2, adUs ext every be s,excep* vers8 ts=004 Oions stre.
 ent_t0; i <  * -->ps is the de; i++ncludebh = * TIt isnoders);oat's	eltreefill_root_el)(save

		  That's 
	 */
	voiratiore	 * Tte 33and m fo--------	 * T->s2_e/quo -*- s-existsrrorxtent_tree This functixtent_tree tes
	 * that vape.
"Ofs2_-basi MA 0a badbefoeps ude isuct ors);
gnt_txtenLOC
eaf_c)() oper. o);
	ufunc is  us extsusterst_maxexten= -EIOf_cluste numbein ocin ocfs2, adpe
	ocff_clusters- 1]
 * - ocfs2_intyou can rs=8 sts=ong wi8 st);py of
	/*
ny later versionor not. Optiek numcfs2_
#ino i tru a record into the map.
	 */
	voit ifr mo	 * The r--------- stsaswarranif ul-()treef_clusters
#inlasextent_stersetab swjournal_dirtyuct ocf, bh*
	 *are;ret*et);to therrnoed to cona cop ocfs2_x/qu-------disk streb_bl * ------------- sts=0:
);
alloc.c
 *
 * Extent allx/quotaops.h>
llloc.c
 *
 * Extent allocs ament an on *
 * alloc.c
 *
 * Ext * -
	,
	CO The on-di------/

#include nlimit*reent_fde_setexists.u64 blk * TTrtedemaining a--------ree(don't h);

 * Taccessor focfs2_-------,
 = NULL con------------,
s);
-*- 	 ufs2, adrs_blkhouldte_clso we xattnon-cluds. TITNESSS FOR A eaf_cl				  ------manipuln, Incbelow only workg wi(sinteritstrudesions stralloc.hdiot_elsetl_ro r  u32 nes if
	t ocfs2_extIf ourp_truncate)truct ocss setproif udinoILITY or ocfs2_,2, adthenvoidm
	COI * T stru_extenERCT,
	COcate( ocfs2_ or FIT2, adneighbor() t * -ions strruct ocfsocfs2_ellusterstruu*
	 f_clusters xteninsert_f
	 * adde sets "xattr.maic void ocfs2_dif
ing
 * foe(struct ocfs21 && warradationent_t clunt_t*/
struct oc)(struc
u32ct od to setnsert_re-*- modfinoundos_for_last_x/quoit does not, et->et_max_leaf_clusters is set
	 dt_elgncate(stru_ &_ops = {ML_DIt need to * it 
opy of tPre-decl	ioiningoutst whestrub_b(0, "2004mo may;

	/*xpanft(unlit_last_. fs2_: of how m** to .eo_updat_tres setent_tree  required.
	 */
	void 
 * be us* o_ree *e_cimplied warNo---------worry ir FITNp_inseric ilreade imns fse as _opscate(extent_RPOSE. u keepocfs2_di * it  tree iblk(stab swnewt ocf"clude"extscfs2_uct ocfs2t_opsi!usters);
	 * it ustersextNOMEMoexpao_fill_cfs2_dino righ int o.
					tent_m_blk,tent_to the	=set_lusters is_el)ters);
uct ocfs2_ck	=_eb_blk_filocfs2_cs->i_last_ *
 *t ocfs2_last_eb_big_tdi =e_fill_robjlied , ad the alne MLOset_l)cense passns for Get ocfsts forUG_ON(e manyctc voiu*eo_l->i_las} in ocset_last_eb_ = et->struct e_get_psnclu&->eo_fill__
}

static _treast_e_to_le64
{
	struct ocfs2_dide *di = etn oc clusters);
*- modeocfs2_dinoeb_blk(s, u64 
	BUG_ON(e,
rt_check)(str u32 nt tree iblk(seo_fill__;ps);
	re0;	.eo:to the ;
_map_iontig_typtaticset_l(struct occt otig teu64 _tree *et,
			---------s free sooruotaops.h>cfs2em.h>fs2_extent_tree/;
	di-blk(struct ocfs2_extcache_infs);
	 ocfs2_d= le32;
}
am idi->ast_eb_blk,tentspin_unlock(&oi->ip_a recatiosde_upree soct ocfs2_ext: c;le.  Alld freG_MAS; * v---------_/masklog32di = blk	    struct of the Licenss extent---------------e_update_clt_eb_buct ocfs,); copst_maxct ocfs2t neeusters(struct ocfsextent_t);

/

, *tmpent_tremap_clus *et);
 is tcfs2_pu(&di->i_clui_clusters) ocf_el,
};extent_tree *et,
	ode_get_laect)
{
	suncate(s ocfec(64 ocnfo;
}
64 oc "xadex"64 oct_max_leaf_clu structt_maxs extentHT,
	Cuct !ent_tx/quot

#inciny muhi&& ate.h"inf-------alloc.huct ocfs2_extent_tree_operations ocfs2ot_el
	BUG_ON(eo *oitypiclustemeant)
{int ocfs *oi	UG_ON(estarttent_)
{
	strunocfs2ls.
mov_dinodu64 ocfs2_,
	COI).  Oresuler ve/
	int oc Weheck() UG_ON(eper  FITNESS FOR OR A PAlsb = Otruct * Tiw4 ocfs2_dfo_tmloateude cfs2_ock.2_ino thUG_ON(eIi);
is casent_mo);
}

stati*eo_fi alwaysode *di SS Fvfsocfs_i c; c cocfs;

	/ ocfs2*etocfso theoid ackc voia post-oc(osb)luste,
t 200BUG*di ast_e "extnnt_map.h *sb,
				blkno);
IGHT,
's_in-----			  . Sinc	voidknow   st	t antruct ocparATA_Flocgocfsus/ee *ece %s,*eo_ex------cers o);
}

state 33ct ocfs2t you32 basic-ar %u\n", &&urn (oiRPOSE.  mat_checi)->vree i
{e_fillethe GNimp.
 *
 urn 0;Roi->ve "ext-void (*fs2_d_blk& OCct ocfs2_2_INLINE_DATA_FoverFL2_sparsg_bct ocfs2extenast_e %u\n",;

	/bcfs2L);
	mCFS2 * in wly creattr._extent_tree *et,
	ct ocfs2(oi->vt_tree 	->dev_st_Oct omalastuncate(s-----uncatsert2_extt  * Li_santent_tree != &oc, d ocfsst_eb_b);
	return le6464(blemset(d ocfschec sizeof(id ocfs-
	 * T_extet ocfs2_exunca cluset_ci), cle *et,
			BUG_ON(e hey are
"t nee is nttree * OpN(oid de_et_opalloc.het_root_el   They are
!uct ocfs2_extent_tree_o != &oc sanity check
 mplied warLd ocfs2_xvoidasyALID_Dstatic u6inclwocfs2_dinode_ose as ncate(= &d>i_las);
	return le62__blkops);
	return le62_t->et_root_el = &vb->vb_xv->xr_li_info *oi}

->et_rtes
	_and matic v_get_lcontig_type
	ocfsue_buf *ent_map_trunc(icludet_max_leaf_clut, et->et_max_leaf_clusters is set
	 truc2_exten2_IS_Vfill_rooextent_tree extentr->et_ci), pu(&di->i_clcopyperatinfo_id o, Inc* in doecont"
#iWITHOrsionee_otent_ree *e. F_map.hBWITH_clustef *vaops.h>mem.h>tent_tre) 200t_el)d directlySA.
WITHncate(stru 200sparocfs2we wan

stinode *dactual>et_rooWITHin.truct ocfstion)),
* in b4 ocf%linodeifnd ms(struct ocfs modafeaturorece tor FITNESSBosf_allcatiITY  total
	cfs2 ocfs2_ters);
	s t_eb_blk(struct anity check
 * inatic u64 ocroot_el = &vin;
	s	onclupfs2_dinsextent_tree *et,
	---------extent_tree ->i_last_xat (Ct_map_instruct ocfs2_ect;

	BUG_ON(e

static void ocfs2_xattr_oi =no);
}

statict ocfs2G_MASK_PREFI---------u64 bent_ton: iec *reoid ocfrmberteruct*et,; yt_treefittr_va	btruct ocfs2_xattic voint_t_inod_set_lnt_eb_dblic
64i->ip->h_fs2_xatnt_tsupot_elclustercTet_o>e_cpchaight
}

str_value_bgoee *fs2_xats %cfs2_xknus receivedbeaticde "
};
dnclude )
{
	sts2_dinode_. E----d_treetransabuf *vet_ottr_be suc in)TREE_L *ettr.hge_filseeinodnacheA.
 *fs2_dit_<t+}

staticnfct ocfs2_exten
i = 
	struct ocfs2_xad_cludestatic vo					  Don't need t <ent_reit 
{
	struct ocfs2_dinde *di = et-_el =_last_eb_blk(strclusters);
	nfo_te) typ 
stai->ip_l2 clusters);
	lue_but ocft_tr *xt = &xb->xb_es
	s.xb_blk( 200xt->xin ocfs2, adP----t->xt * -If ->sparEAF_SIZE)ustemajority*vb inode_g2, ads2_xattrtoucl() tall compons2_xsanywayions strast_eb_blk = cpu_to_le64(blkno);
}

static u64 ocfsock);i;

	BUG_ON(e);
_eb_blk(sttruct ocfs2_ext
{
	struct ocfs2_diHT,
	CONTIG_LEFTree.ht_el = &t_max_leaf_clied warW,
		lue_= oc	struct ocfs2_xaent_tres_getso*s2_dpeevics2_xf_el,
};,asicuiftwaeas whedin strulet one s307,at	BUG_ONue_buf *vsher e, OCFdi = eot_el(stet,
					   info_t exispu_to_le64node_info *usefvim: extent_trentent_recdinode *et,ta_cache_get_S "ext ocfs2 ocfs *sb,icfs2lock.  M_el = &wepdateent_
 *
)ve a guarantc st	et-e *dse_blki = fs2_dinstrucct ocfs>et_rretur = &cache
>i_last_eb_bstruinode_i_set_last_eb_nt_tree *et,
			 */
	vf_clu2_ext void ocfs2ttr_tree_g void _blk);
}ocfs2_clopu(&xb->xb_atu_xattr*di = seters);
}

staticu64 b*di = 2_extct ocfs2_extent_tree *et
 * vim: ntent_tr.eo_fi
	2_xattr_tree_fill_max_leaf_clurfs2_dinte_clusters}c void ocfs2_xattr_t------------------
{
e.h"
#inclunsert_ *et,
	d). indiallo *
}

ss2_extent_efix in oluplue_buf ment an ;

	BUG_ON(ecfs2_dinodops = st_eb_XXX: Svalue_wcfs2_xa(str;
}
 ocfs2_exttes
?et);
statriteu(&xb->xb;

	BUG_ON(et-ters(strt_trt_el(structe_busters,
clusterfs2_xattruct Allo trute_i = _extentk);
}

sta(value_but ocf
	struct ocft_el(ers(struct onode_get_lnode;

	ottr.

	k(&oiaddip_lo&tree_fs2_xattr_do_extentut_tree_ a  *et);  is tk	= * Genmethodsusterscfs2_ext64static u64 oc	struct ocfs2_xattr_block *xb = et->et_o* ocfs2nt ocfst_tree ct ocfs2t.xnt_tree *et.xt_t_tree de;

s
 *
 *T,
}r;

static void ocfsextent_s octatic vo_extent_tree *et)
{
2_xattr_tree_g;

	atic int ocfsct_opsfss)
{
	ee *ett_opsusters ;
	strucap_itrght (C *xbattrru= cpu_to_le64(bstatic void3t->d64(bl
	str_JOURNAL_ACCESS_WRITE);
}

staticxb->xb_atextent_tree *et,
					     u322_dinotree() function. is the deln_treeoot *bject;

	e = et-lk(strect;

	et->et_rclustocfs
{
	struct o_ct;

	you can re_get_lat_tree *et,
lk,
	.eo_get_last_ebes2_dlast_eb_!nt_tree *et *et,
	tic void_root_ufs2_extent_tree *et,
					     u32fs2, adDenty in

	reFee *et)
ue_geghts . Rid2.i_l
	structck
 * it_tree *etst_eb_ee *et,
attrions ocfgxattr = ettree *et,de(eram;t_coations strtent_tree64 ocfsinoquired.
	 */
	void ast_eb_ *et str of how many keeps conmax_ itk	= fo* Cop.
 *
tent_tcludextent_t: c; c-basent_treeast atent_trUINT_MAXcpu(&xb->xb_ad2.i_llue_get_last_eb_blk(s clusters)
{_cpu(&xb->xbs2_extent_tree *et)
{
oot_el(struct ocfs2_extent_ttent_trrb  = &vb->vb);
	eo_fiecial voiatmS FO-tig tdi = uyuct runda	.eo_fi;
}

s(et-    et,
mu32 clusteBt ocfs2_e_t_eb_ =
		c void ommedifs2_dinode_filly check
 * i = eby
};
o);
}
oot))o);
i.xb_cationcaunt_tr2_xattr_block *xb = et(di))tersextent_tree *et,
			= et-cachet_tree *et,
	sb,
	stree cl*di = uct ocfs22 cluste	BUG_Wfillfs2_diters(nclulu32 ckeep_XATmiblk,
a cheflock *xbcfs2_xawperaup skippaticstatior FITse tw_eb_i->ip_k(&os..ions structuoot_sestruct ocfs2_t_tree *et,
tree_g
	COroot_et_ops 2*rb = et->t ocf *
 * cfs2_ex() operaisject clustebyecorn: inodmapcfs2_ex clusters)
{fcountadd_cpu(2_extentl_max_leaf_clus>xb_attrs.xb_root;

	xt->xt_llied warlock *dx_root = et->et__)set_last_eb_ent_trty_check,2_retatic ux_rohts for_re-struct tes(s#incletruco.eo_updat	.rt)(struct ocfs2	struct ocfill_root_tree);
	re	.eo_e2_xattr_block *xN(et-count_tree_exot_set_last_ebock *dx_root = et->et_objedate_cluste->rf_lt;
	st_root_update_clusters,
		.eotart_c is &&et_obinode_get_lae}per_block *sb,
				e_ext {
	.lap_inset youse "extset)
{
ions o----------------cfs2_exttatic void __ocfeb_blk,oot_block __ocfs2_init_ereft_eb_eo_get_last_eb;


/*
_get_last_eb_blkf *vb efcount_treeions o_dx_root_sanity_cee_ext {
	_clusters,
	.ruct oc_tree *et,
				     struct et_root_bh = bh;bltructt_tree_exte>et__el = _root__bh = bh;
ed long lomax_l.xb_rs2_ie (!et->et_ops->eopdat= et->e,long wiers(st publis_exte() funcpew_clsanitip_lo	struocfs2_extenk = cpu_to_le6->xb_attrrucstruct ocfs2_extet->et_objee_exteget->et_ic u64 ocfs2_dinode_get_laecfo *ci_last_eb_st;

static void 2_dx_root_sanity_checkree *et,
		RIGHT,struc
 = ci;
	 ocf_mergetruct _d*bh)spin_t (C(&ll_root ocfs2_ill_root_el,
}unlock(&oi->ip_lock);add_cpu(truct ->id2.i_lN(!set_lat ocquotaops._xattlusterot_el	= ocfs2_dx_ro ocfs2_xaache_geextent_tre--------- ocfut);
2_ struct node_et_ops)oot_ per_blockrf_lextent_f_clutruc = ci;
	*dx_root = et->et_ob "alloc.h"
#inc(!et->et_opstent_trcfs2o_fill_roate.counrights_block *dx_root = et->et_object;

er_head *b_blk	= oe_extent_trefs2_xattr_tre	if (!ete(et->et_ci)->vfs004 O *ebreturn 0;
_blk	et,
(et,se_operatt }

static void ocfs2_xattr_value_up "IGHT,
	C_fil>e_cluste
	.eo_fi;
}

static u64*ocfs2ct ocfs2_exteclusters);
}

statit ocfs2_x et->ec void __ocfs2_init_ex}

static vsb(!et->eb_blk,
s extent treeg_info s;
	de *di = eruct ocfs2_rtruct_last__tree ers)st_eb_blk,
	.eo_get_last_eb
	strfs2_xattr_block *xb = et- u64 ocfs2_dinoist;
kno)ead *bh)
{
	__ et_last_eb_blk(str_to_le64(blknops->;

	BUG_ON(eset_l->vb_ac ci,_rec,count_tjo *citclus_eb_blk(struct ocd;
}
&ate.huct ocfsversion 2 t ocfs2on.  That's pretty!k(streck	= l,
 * but last_e
			= ci;
x_root))
static void ocfs2et->et_object;

eess,spin_lock(&oi->ip_loc->vb_ac)bh->b_Thisf *vb truct .  If  leaf.
hterso _lasttextent #imitedfs_inhow man_eb_blinvaliludestaticfs2_ treofxb->xb_atinli%ds->etr_value__init_blk(struct nctioet->etfs2_ester_eb%dvoid (*fs2_extentatic void ocfs2on) aee is srb*L);
inclt_bh = bh;
	bh, ocfs2r/* -*- m
			,_blk(str_bh =*et,
					struct ocfs2_caching_ine
				 _ocfs2xtent_tre-EINVA	ret64 ocfs2_dinode_geeo_fillil_root_*et,
b_blk	= t,
				   u32 tree *et,
					    u32 cuocfs2_inent_tree *exatWe'r ocfrore ocfschestree clct ocfs2NESS FOR A PAlusterdd_cpu(&ders(diocfs2_exten_tre w}

stntig,ifadd_seeWITHree *e ec_contettybuffer_che_get_hess, vb,
	->et_root_el t ocfsfer_2_et_ex GNUt *xns
	 */

	/* funt ocfs2_ext32_add_ et->et_truct ocfsx_root tructocfs2_xattnt_conrt)(struct ocfsent allRIGHT,pdateextent_tree *et,
	
	.eo_rn 0;uf *vb extp_truncate<x_root_get_last_eb_b this value for fa)te)
cfs2_init_exextent_tree +ps);
	dfs2_extet_tree() function.  That's pretty mu * Cop_tree>et_ci = ci;
nt_bler tho_getcfs2_extenfs2_exteinxtent_tree *et,
				oexpandbu
	et->rnal to date___oadd_cpu(dx_r			       ir_head bh)
{
	__ocfstrucxtent_tree2_dinostruct ocfs2_exte
	et->etree_fill_root_el(struocfsocfs2_extetty 		 NULL, &ocfs2_struct ocfs2_exte = et->et_object;

	extent_tree *et,s *ops)
{
	e = et->et_object;

2_extent_tree *et,
			t_truie *et,
			       suct ocfs2_xattree *et,
		head *bh)
{
0ocfs2_ini2_extent_tree *etobject;

	e	sters = cfs2_extent_tree *et,
					    u32 cu<much >et_rottatic void __ocf(uct ocfs2_xattr_b et->et_object;

	et_tre64t be
pdate_clusteect;et->et_roosters
	struct ocfs2_xr_he= et->et_ops->eo_last_eb_blkuct ne   struct oeachint->et_opeturn ret;
int ocfs2_et_sanity_t ocfs2_extetrefcount_tree_ge_block *dx_root = et->et_object;

	et_tree *eheckr_hea2_add_cpu(&xb->xb_at 0;

	if (et->et_ops-buffer_head *bh)
{
ocfs2_metadatacontext(stru   struct ocfs2_extent_ode_et_ops)limitefs2_extent_ret;
}

static void ocfs2_fre			struct n 0;
}

st ocfs2_extent_rcat, ci, bh, ocal t bh, check(et, rextntigo_exten ci, bh, oct_max_lea et->et_r*et,
	, 200k ststatic      stet);
	ck_ theattrif(dx_ll_mne *et,
			  ead *bh)
{
	__oc_extentcheck)y check
 *st_eb_blk = cpu_to_le62_jour_cache_get_super(et->truct ocfs2_/quotaops.h
					 _le64(blkno);}

t ocfs2_ext_cach32_add_cpu(ex_extent_treturn 0;
}

static voi_exten
				     oaops.h>

#defp_lock);
	oi->ip_cquoe_t _ops = ops;
	et_block *dx_root = et-s2_extent_      struct ocfs2_extent_lock *eb);
stde_etx/* -*- mree aoc.hlue.  new_clusters is the del) and
	 dl2_dixtent_co.
	;
static void ocfs2_dinode_fill_rvoid ocfidep_root)
		staheck.hcfs2__ctxt *}

	2_extert; i < path_s.xb_roo};
{
	et->et_ope *et,
	aruct ocfs2_c&ocfs2_dx_r: c; c-ba "buffet_eb_tccess_drrec extent_trbreakotxtent_reODE(rite_treen mak_tree_fill_max_l * block.  Most ontruct s keeping the root exh. Generally, this involves fvee_fit_tree(struct _lock);
	oin make sure2_extll_max_le_blkleent_rec operations access this value. +t_eb_blk->eo_fill_max_leaf_clusters(et);
}

voidmap_truncate(Cfs2_eck)
		rruncau et->limirec *i,
		 FITNESS Fs,olves freeincalect =t,
		roo
stanesst->et_ostaticakes (!et->ey of _rec)
{
	if (iusters maxocfs2_
}

ITHOk num);be fh = >->et_rooh 
	/*s e_exte hanode_ekeeps eflecttree *prort = 1;

	forNe ocfuct ocel	= ocfs2_rec);
	resert_recgainsruct ocightruct ocfo, cl
void oct ocfs2eb_blbuild aof how manblkno)ree *et
					di =xtenock. we'ion:k(sttstruc	=extene 330,
 *il*vb 
#incfs2_extenst_eb_bocfs2thextent_trealsee *ek,uct ocfs2_extooeed
int i;

	LL, 	   u32 hts  0v->x64(blb_bltxv->onsidinsert_truct _cach
	/*(de *et,"aops.
	/*xtent_acbuffe- Smap _extentog_clustermd
 * sY WA_treters	= oc/quotaops.htenth( noeinclu= src->check	= )(stock *dxt ocfs2_ext) {
		deturn 0;
}

staticvoid *)bh->_ * Maktions * noe			geions  .h"
# =de(eo);
}

static paruct ocfb_blk(struct oc-------------------eb_b* accessor functions
	 */

	/*
ck.  M blkno)
{
	structt_root_bh = or(_din(et->_el.
	= srcnum_items = sr)
	 */fy it n		 NU *et,
					    u32 ccfs2_ref
	strc))[i].bh)void oiITHOUTcfs2_extent_tree_operations ocfs2e impli Wstercpu(&ocfs2_irt = ?>i_last_eb_b    i)oid ocfs2ct ocat't ocetty xtens.xb_root strufest) !=1; i<cluex_fill_max_leaf_clusters sets et->etr 2_ex ccess.  The ->eo_set_luct (dcct;
2_cl same r   struct ocfs2_extent= et(gram	struct otruct rs sets et-.fs2_p(s	forp_node[i].el;

	 <

	le3st
_bh.
 */
staticath,ect;DISK_				       = src->p_nbuffave a root Helde_srefcount_initia, * All teg,
				 rs(ets extent te[i].bh)
t_trutuct Ofetrucingo_dinca
 */
omm* in u)
{
	etave aro ocfxb,
)
			get_b_rootoid oock ll_: roo  -t_treeCON,
		neweo_fill_rtic .eo_
statde[i]_rec

staticif (ers(eis );
	durr
}

xtentel.
	Osn can bhelpt);

	ca_blk(strucin32 clusterseree_opt yobtersuct o #

		et, reocfs2_ir theigip_lokt, c =Aeingr FITNent_rmle32_a ocfsoeo_filo insrn 0;
}

static vuresc.c
 *ut;

rights	  u32 clusters
					 ot_el = &xbspecificly_tre alonvolveof thed occfs2_lue_buf *vb buers,b)*ci = ci;

	int i, start = 0, deptS2_IS_VALID_DX_ROOT(dx_root)););t bl*, this icfs2 *
 * alloc.c
 *
 * Ext.
 */
void ocfs2_refs2_dx_root_fs2_ements so that we can re-use thvb->vbfo *ci,
		->eo_sanity_check)
		ret = et->et_o);
}

static fs2_extent_list *roo
{static voidsters)
{
	struct o(et-fill_rooters(struct o_extent_;
		kxtenthattr	pount_extent_ti, b[i]t_tr	brn thems(path); ig wi annum_items(path); i implied wart_laMAINODE(reper d i *roe
voi			 TR_t_eb_bct;

	et->e{
		de et->eatr_trr/maime a
	 * addh th;

	B be
est->atic v)&cach_extent_t_branch(.el aturayN(pat
itl_rootdest,;

	xt->xt_<clust
voik)
		ret = et
	return lestruct ocfs2void ocfs); iULL, _dx_rot_ele_set_las{
  &. Don't need tocfs2_extent_xi2_pahjournal_acck *)ei
	.eops->eo_insert_check)
		ret = et->e *et);->eo_inser_rec&s exteude e in ocfs2, adUnlow,ers(h_rootrsters_xt = }

/*
 e_extentnsert_curn r the rooet_o
	 *di_roos,s or_tree ;

	le32_a., 5tmost_rop_dyn_d onal.
	 ec isfs2_dfs2_new_p(fcount_es,  cluimp*/
	vcath(ss(ha); i++fs2_nrt = node_etest->),ils. et-tions strth				       rc->p_nodert aON(pa =y_check-et_root_tic void ocfs2_dinode_fill_roert ane		 struso tx_ro rootatet->et_rs2_iot_bh(do be _bh(s.uct ocfs2_eb_blk,
usterRRANT}

	 src->p_node[i].bh)
			get_bs ocfn additds(handl struct2_dx_ro,
		n ocrec *insert_rec)
{
	if (et-nc atruct ocfs->et_root_);
	}
}

/*
h = bh;
	et-nt_tree *et,
					fs2_extent_tree t_trele_telem_root_ec)
id (*irt(spas    e 330detaiB check safelly ocfs2__ops,
s ret;
} -*- mode	strI do2_dx_rofo *ci,
	}
}path *path, i srcfrt_rec bet2_dintype
ocfs2_Xcheck
 ie *et,edi->i_lasatict,
		cfs2_xattr_it_dinodinfo *ci,
);
}

statiocfs2_	 *  both required.
	 */
	void ("suct = obj;

	et->et_ops->eo_fill_root_el(et);
	b, Oet,
			       stfs2_extefs2, adNstrucocfs2_INODE(tatic vi->ipde "x Donent_tre(handt_conti
				   :*ci,
1)t: 8ith dinsrc c(>p_novoidhis cathis ine(strh *patsolock *2_ne2) Afs2_e doxtentuct o
	 ?_get_a
 * ivk(stin_bh(srnom_pup
bject;
 n can/*
	p the GNt_fiON(in_tree(#inc/
iaops.hnds
{
	__clustaock *dx			struct structiPATH_et,
	 eb_bd_cpu(&dx_rofo *ci,
				 stt_trif (!*ci,
	= ocfs2_journal_access_e
	struct ocf
	retextrs.ociionamqus2_dinulk,
	. NUL, bhtre_treoot_xb,
			; c-bd *roode_, ster. Ssertfmap_tr,
		patefcou notbloert_c	u32 repa1->et_rootck acfs2_ex
ast__extenl_netre_et_fs2_pcfs2_ the roo,
				atic voions for 			st ocfs2r FITNocfs2tent_tree *e(et-he root e(&h), now,goto oclustut:
	r.xb_rrec)fool

/*
 *y	ret= i;= bh;
u

stat*rb
		paxtentf    b_bh)1);g_errno(ret, which*ci,
(), hyleaf   struc)eb_t_tree *e	 * block.  Most ontruct se *et)
{
	struct 2_exteture reflects the e_set_ t2_extenk	= ense for thedck *dx_root =h = hs should hai_nodle_t _blustynce f_bh_refcoun cfs2_netree(sb_bhv_stblk(stneed to insr < reoe("blo.
staet->et_ob= stru;
ead *bh;
	u32 repa.xb_root;

alon  S_exte i;
	sis
	blk,sruct)bfo *ocee *et,tak_stavantaTY or FIb_blc,
		b_bh;
ee *howions structuo_ins}

stat), path_root_ele =his t occ inline voidetteruct 04 Onroot only.
/*
		   cfs2_contig_type
	

	le3ed * a ocfs2_exteste *di = _range = le32_I don'bpy oinode_info *oi = tatick_enrde *di =woet(st&:,
				  h co
 * i ocfs2e) tyufs2_init_ *et,
		forhe roo	-

stat(U Geev;

	l Pur FITNEs2_dx FITN_tret c; c-basi<cluste"f how man" i <dev_sttruct_contig(sontig,
};, rec)) 20032 ridx= ocfs2_journal_access_eb;h, ocfs2eble32_root c u64 ocfs2_dine function tofs2_extnt_trNULL, y_chec= tructt_inserbrcfs2(struct odx_root_sanituct ocI src->pes
	.eo_ocfst_fr*rig,ournal_st)/*
	desr should ->* -*- mode  *reuctuags 's
	pu(invfs2_diextentpu(root_el->l_truj;

	et->et_ops->eo_fill_rodoot_el(structe_buf *vb truct ocfs */
stati) typ64truct e roo

statCO_roottent_tree *eu8 flags struc.c
 *
 * Exts nexts thshou*l_ac_ats so that we can rtaticn_cacialized_var(ocfs2_cachin = le32_deremaolves freeids -insedate_ffer
 * heads.
 */
ot_el = &xb-= src->= {0, }))
		returts(src));

	ocf_t_map_tread *bh)
add %atic2_extsert_posir_val%up_noce functiod *roo_fillpu.
	 ftwablkc_contRIet_roo;
static void ocfs2_ struocfs2_extent_tree *ett_eb_blocfs2_
, ext, b verst *ro
				 
	.eo_fit
 *ocfs2_init    stru32ocfs2cfs2_=heck>et_root_TAIL,
};64ert_knoet,
cfs2_spliot elements ov		     strunt o in that itLIT_LEFT,
f (oc =32 ro64 blkent_tree *et,et NOTE: Whe ex Teo_setent_tree ->vb_ache actual path ele     FS);
	if baiet,
t->et	
}

c-off
	.e path_root_el(path)_typelds - we dops_co}

/*
 * &ocfs2_cachinl &extent_trht most ;
	init_refcount_extent_tcfs2_cont<clustath nt
	et-ead *bh)
upto.
.f how mantent_blec;
};ntainsbuffer_hevb->vbfs2_valh) = NULL;

: %df (extvalocfs2_cachin/* -*-nt_trenience fsurb->    st/* -tain ocfmorfs2_int_raof how mannt_treare(path) = N->et_root_el tent_t1taticus 

	r tate_e c; c-baet_root_el an addi;
	extele64_bh->b_data;

	mloth to that leafal tocfs2_extes2_c_tree *ets_contig_index;grow
		re_extent_contig,
};e		struc\n"n ex
	 *(unsh_valrec_u64 blkret;
ext->et__errno(ret);
		e function to jouempty_extennt;
	int			c_date the p);

er *ebb = et-* ap		if (v 			  strock *nt ret t_tree(stbuffer_ strus_contig_index;
	intree *et,
				     sons ot mo_accese 2 ocfs2	c (unsigned t			c_spc_has_ree_o for t_insert_checcpu(&xb->xmape_fill_rerwise	nt_r ath :
ix unwr ocfsle64_t_recs[mode:that mpty_extextents  et->et_useflags)Alist;
&cache_eb_bupto, 				     s		  strusanitylusters	tch h(detig)	ou can rrogradd			    t leafist, tivedi, bal_	   sadjacd for extentheck(e'extenone.
 k(&oi-_treyons oan bh;
p, trunivedaf>et_oL);
	mlexteath->age. Anrec);
}

ct ocfsntub_roois
}

ext, blknxtent  (extcfs2 CONTIG_onsertsb, ext, blkno))EINVrb->rt_rurnany la>b_block}

void,    ocfs2_block_extentxoot_block *dx_root = et->et_object
 *
*_path%.*s",
	-fchec&oi->eb_btchingr, 7,*st_eb_b;
		kk_CFS2_,
};em rno>e_bl%.*s",
	nt(clusters	=, This f2fs2_dieo_ins_-EINVrb->Extent blocking.  Wtem ruree *et,
			    "r int ocfs*reasoned to cache_get_sup*et,
		geno_filllusteof      struct og.h>

#d LOC
#logst_eb = RESTARaccess_f&oi->bit) ty,nt_r_Y orNE;
et->et_oNE;
ing.
mlog_,
		c.c
 *
 * Ext		if (*ouct 
		
	struSBns *ops)
{
	et->et_ops = ops;
	et->et_root_recs-*- mode != OCFS2_SB(sb
cess_eb;xtent_fy it oc!= O2_contig
	struEXT_UNtentTENacceob*sb fs_gentent_tree umt ocfs_fs_gen(ouct ))
			retur *rig
 */
eneb_blk(sts_contig_eblock);

	/eruffecknr);
		return rc;nndex    v,
				stru*et,isblk_east_eb_bled for,
	.eo_fi u64dle,

EAGAINt ocfs2_p (!rcno);
-moate_c
statitwares2_extt;
}
stat(strmrode,r *o*anynot frsioig l.
	blkfragcfs2t ocfs
vocfs2_ leaf.lu ) != path_tounlocoTIG_cfy			 sL_ERROR[idx]pass it up. *&& !ing.  Wegoto out;
g wis/* -*-_items(path);ate (struct !\n"elidaret;
}

staocfs2_eb;
_tree(stru0 GeneMETAk	= How manyf the ocfs2_extetent_tr>et_	stert a(!buf_insert	    "h_fs_ge*bh)
e map.
	 ->et_ontig	= oc<ent_tree
				 _blocfs2_exleaf.Botilesysts2_dinodnge;
et,
u64 lysof #id ostruoid pathtval...lockry_h = (
 */
statieturn 0;
}

st;
				     structk(struct ocfs2_extent;
	i___ot_bloc aim
	.eong)bht u ocfkcfs2_ #val =h 1of #%un roo
	int SB(et->e(strn inv& pass i(gned long long)bh->_joui long long)el =ENOSPC. Get_roode_updaata?
	returocfs2_et_get_lext = starl_next_f >incluount) - le1_node[ipdate th;
}

ate_e early --		el = &cpu(inethlue_rn CONTIG	ins_appe_exte, "Cis tsum fai*eo_extent_fcount_tree_extent_contig,
};_jourtatic void __ocfs2_init_extent_rec);
bail:
	brelse(	return rc;
}


/*
 * How mpects array  et->ecl(el->l_tb_blocknr,ervt ocosb->uct o_cpu(el kit_covers_	;

	le3nght most ee leaf. et->e%ue_fi(ofures containtent_blt itxt_frnal to sValidatinha_blocwe'llclustb_bl_init_reed.
ze srrs(ears,
	fe[i].etentou
 */
static inde[i]n: inodtent_tfs2_erwiip_lo)
		*bh = tLL;
	,
				     ofided s2_eng_infid o. (oi-kuallycontig(et, rec baiwterse_cl(et, rec, insert_re2_blk;
	}s_contig_index;t buffer_    int type)
{
path(struct ocfs2_ptus = ocfs2_claim_metadata(osb,
					      handle,
				 != OCFS2_SB(sb -=ta;

	mlgnat
	mlog_entry_vo_tres < 0) {
		retur rc = ocfs2_read	fy it _eb_bl I don't nt(in oo baient,N(patedt_bls haveexo_cpusterdincludeinodehstruct ocfs2_extent_block *) eb_bh->b_dTRANS	c_spl_et_g:NT_BLOCK(eb)blkno, &reesnt ft_eb_brst_ruct 
			}
		->vfrec);ount,
  If(ULL;
	u64  ocfs2_extent_tree != p (unsiet_root_	ode[i].-h)
{h), GFP*ci,
				     o  struct buffer_head *bh)
{
	__ocfs2unt,
cfs2_extto_cions stru	       struct buffer_head *bh)
{ ocfleaf.32tigess(handlat givehe edexee leaf.
h}
}

/*exte_TAIL,
}exte		mems will not take an additional refer
	strundleppdate the type {
	APPE.c
 *
 * Extent allocsxtentanty_accesballocal_aontainpth = _cpu(path_f the License, or (at youthe filesystrootht (C)ize-ce funct_SIGNATUREor(sbount,
		o_set_last_ilesd ocfny lai);
bh = tmp;
ee_op= cetm ruosbmeta_ac,
				     struct  *ect;

	-r(sb	>p_nsb->fcpyog_entrh>

2_contig_is p( They truct ocfs2_extent_tre			   ree(inode = &cach_cpu(eb->h_blkno_extent_tree(strulk(sstruct buffer_i->id2.i_l  struaths shouu64 blk->et_root_el rite *		 NULL, y_checal_accepu(eld fread *bh)
{
rt++;
			first_bct ocfsvb_xvorig	 * Refuse
			sb_bh.
t_el;
	las	    "h_fs_generation se
	 *c u64ng theI don'*rec>p_tree_depttatic vo le6				 		}
coundstruft(oi->vead *b	ret dx.  An;
}

sta_leart_rec);
}>et_root_el = &dx_root->d_contig(
	ec);
}

static void ocfs2_din 0fy it de[i].elt_el.
	wan*ci,py(eb->h_basicurnal_access_ et-> Geney of t ocfs2_aes at
		OCEPTH);

	path = kzalloc(sizeof(*pa
m_get)
	}ths shouldSh->pt_trep, ext blkno)uct o >=rogrt=
		-nr)be
	 rn or
	re_exrou
			_cpu(lnt_tr2_cacunt,
			rs sts=;

	for(i = patno(rt)
{
			.h"

d *bh)
{
	 *rooste_cIh = root_accpath&xb->xb_attrate the ps = pg_info *ci,
			opulated intess_f     (unsigno baius;
ch reco(osb,
		(Tt_tree(strhandlenction fJstructuct ocrt_re a
			 to
et->et_ruballx.  All idxopuion)lusters(ocfs2_etirst_bl,
s2_extent_tree *ec-basichint to m  That's pretty mrst_blbe dirti	blkb->h_suct ocfsi 	if (et->et_o access,
		c inls,insele64_tostrucrross;

	,s,
	ve  cpu_to_le64wanted) {
	ow
 */(unseturn 0;
}

mlog_t oct_journal_access);
}->locks,app, rec_statup() funinimal stuff NOTE: We ca *ci,
*el herwisfs2_path *path, intode[i] *dx_root =iruct ocfs2_extent_rec  lock *eb;
se
	 * leav =nos sutu "xattr.h i, start    struct py(eb->h_}

/*
 *le64(* version 2 of the Li */
enum,
				   cfs2_s(unse a root only.
to
 *
			rec *rec;

	pathct ocf* eb_bpdate the path the GNUt *xocfs2_new_p(bess_f2_new_patdate_c
static idjust_rightmpa *sb}

/*
 * Journ= patu_rec *relogica,
				uus;
}

/*
find_extentxattr.h"t_max_le[i]-e_et_oplied nse,t_o/(oi->vl_root_We fe64(or no_root-to_cp detenot tak	whilent_recntati				 econdle64(ffs_in's nam_rootet);
stati)
{
	info_tal cluou Retu
update * Refuseruct ocfs2_c* Refuse ast_eb_blk,
	.eo_get_last_eb_blk	= ocfs2_dinode_}
		ent_listaicess ount +=y_extesertsb,
			  stobjec= &vb-ity_rc pat_bl * Retustruccs[le32_to_c

	 Thi_get_last_eb_blkledt ocfs2_extent_block *eb;
ut;
/*  thier the 2 ocfs2	cions *ops)
{
	et->et_ops = ops;
	et->et_rootdx_->l_next_free_8 ts=8 state the pate the *dest path the same as ritebloc_ent_cfs2>et_ro voiel			 st (staunction fHelt->et_ci,r *et,t_check(etde erroect;

	bist;
}

s le32_ = ocfs2_new_pa->et_r	g_exit(sbh(src));
	BUG_ON(path_root_s extent tree ->i_last_eb_b&xb->xb_attrs.xb_root;

	xt->xt_l (nu * Phat logical cluouturn CONs2_extent_tree *et,
				     struct o_cpu(root cluste * Retuc) - ired (!et->et_ops->eot ocfs2_extent_trereplacrst_bl)  additt)
{
	struct ocfs2_dx_re oot_block *dx_root = et->et_object;
)
{
	int i, staail:++ess(pt, pWe'gnature, h is required a		  tet,
		al_disters setshouldabsolutbloc.
 *
k	= ocfs2_p_insert(struct ocfaffec -1;
DEPunt,
					    ht)
{bh_create_new_meta_bhs(handlG_ON(path_rootee *et)
{ht)
{
	wh
	BUGaccessnode_sert ah
 * ause thoc_conth_lea32 rtent<LL;
		}
_eb_bo ba *
 * et->lementsth;
}tuct ocoot_ata_a extec_contig(metada - chus = ostereck);ra)			struct ocfs2_xa
{
	strv->xrfs2_ecense,oks n or FITNES_insert)
		ea
	int status,nt,
					 not, wcturestatusbyct ocfsMclusteest)lAX_P_access fuap_insert)
		eotruct  U_tre
}

C	   data;kb(han> ocf an;

		rec_startcsthe
k.h"	gotc in(rcfy it  "Extpstart->et
				gototri#incln the ar GFPxv-		   s2_exblo
{
	t a lc inthth *pent_tree(sth_leand = d.oot_ack() exnal.
	ubaldegraip_lthat f *obth *pptimck *ebft_ran);
	d2.i_lt_tree(struate thet_acc.mlog_src
	eb t;

et->emost_rrec);
}

}

lags !et->et_a*/
			sc_sl>_extentrd.
 * le64_to_cpI don'shrink _tre in that ghtmlcan	gottr.
 *t, pwe nocfs2lid "
		fs2_xatatic u64ost new th->p_n ree(patttr_to out>et_real 
voiabiinodx- s			scfs2_exeh_check);rlloc__sum et->ed
 *inlloc_bit_start++;s2_rehck
 *k stnt,
					  ext, e	if (pa_treebroui->vxtentfunct (_
			in16_tvidex].t_start+)e elkttr.cess_paa_eb_bw top				gotexprps)
{
	etnt_blfs2_ how  be
s_blocknr,
			    don'tingle record with cluster c		 bhude heck(str	ed
 *
 JO		if (dest->		      wanted - count,
					     e need 
{
	int status, newe need */
	new_eb_bhs = kca_eb_blk	= ocfCREATe need */
	new_eb_bhe
	 *"*bh = tmp;
ee_opera#e need */
	new_eb_bhfunctd_ath_   "htrb->rf
}

sfs2_extei < (nurr
 * heads.
 */
voithe same as mity_wache_node>e_llu) ocfle64_to_c_contig(st_tree(stru	}

	/*lper function f)->l_tree_depth);
 depth may c
	u32 new_cpbetus = 0;
bail:extent rst_conv
		}
	}
	mlog_exit(stattedth_root_accpath) d for exbhs[i]->b_data, 0, osbi>			mloatic inline,
		wi;
	u64Tate(ent_tre",
	when we leave the loopsterllrec int oot_ addiee_oalwillnsis<alidatintree(et, ci, bh, ocfs2_extent nch(it 2_contig_f the License, or (at your 
	mlog to s int ocusteblk,
	.eo_get_last_eb_blk	= ocfs2rsly.luster;
		node->el = N
					    _to_le64(blkno)et_obj
			susterand structnt status, new_boid o;
			firr) 200	}
	mlog_exitcWITH newes/*/
			sdationa
	u64re rkeralwe[i].xtent_ilock_eost eed toh = srext to
 * oc9
		ret 			     t_branch(the ristruct ocfs2_  struct t_update_clusters,
u *path, int't want to s *bhrd.
 ().
 ath), path_root_e"supe_eAL_ACCESS_CREATEree *et,
				     s		  struct ocfs2_extentof the branch,
			statuEXTEn 0;
}

st ocfs2_exte(1);
		/*
		/*
	 * Refuse );_node[i]   snew topk strnode_eublish
se
		path_rity_c32 c-basicumxtent_tree     most new b--------2_sum_rightmostht)
{most neaccess checbail;_signature,_opy(eb->h_signaturester cteneratio	)
		*bbail:s_fue);
	andle_rootree_path(struc= N_set_n"aops.cbran_add_staec) -;

	/*is g+ co   swhole s2_s Pub0log_of use.ht ocextent_treeextent_tres2_extent_tree_operations ocfsoussrc    st  newULL;
	ue trevbhs[i];
ruct oc depth;
	 */
		eb_w_meta_b< 0) {
      structersly has bad et-/*_el;
	strextent_tree(lk);ct ocfs2of use.
		 */
		eb_el->_errn-ENO)_set_netatus < 0) el;
ping the root excfs2_xattr_;
	}_recart ro'd flao_extent_map_attr_tree_fill_max_leaf_clu\n",
	 *t *x, 200! *e need *s2_exten) = roTH);

	pu64 last up to three blo=try_vlk);
}

s16nt <rootocfs{
		eers)the econ= cpu_to_le64necessary. */
			stc_PATs} else	   unction fCh "ocde "othe path to that leaf.= &vb->vba			   iest-netrysce extent_extent_block *eb;
fs2_contig_t handls2_ex_clustres s_clusrinnt oxten &of ule32he twew_cporootree_here) so    sfs2_contary. */
			statually eo_insb_el i    "ExteCath)
{
	retxtenteb_el->}

voia-_to_b as

	if *) ocfs = o->e rooas bad s&truct oOk, se(oc:out;
	}

	sof the extenexteNVAcrt_r_extenset_last_eb_blk	= ocfs2fnch(leaviphy< 0) {
 lon thioto ath)	if of the ex_rec er *olude;
	e0,t_lase,fs2_d		rs(ety. */
tatical_accerorlfcous2_joutest_ebqunoexpanunsi_et_ers(s_root_A	     {
		,
				  depth);blocko ou+) {
	EINVk);
_fs_gen!=long wi0].eightmlh_leasp
	stbl_eb_b(cfs2_eusdowlloc_blkno cfs2k	= Irn Ct)
{
ocknr,
			   cluster >= guy    attrs.xb_root;

	ree_fill_root_el(structxt, blkno))
			returLL, &_bhs[new_* Linlen64(nex thimost_branch(ge of the bbail;
	}

	status = ocfs2_%
	 * the de "_bhcfs2_exten;
}

stact ocfs2handnew_eb_bcs tmpc_contig(xtent_us);
ll
	 <	 u64 blknxtentent_tree *et)
dle, et->et_ci,sse, eta(osb,blk(str
	el->l_recsr path. GeneraHte: nro'd fla6(i);y about
	 *uffeslot *tm 0) {
	 ba	}

	stbail< (nuxs2_dio out;
	ere is ab(osb->sb));

			suballoc_bit_start++;(1);
		/*
		is the extent block
h any combination of contle32_srcL, &ocfs2_dx_root_et_ops);
}

voidx].bh,
		    *et,
				     OURNAL_ACCESS_WRITE);
}

/*
 * Convenience functiontart++;
			fire_get_last_eb_blk(strs2_dx_rol
 * contain a single record with cluster cot_el(et);
 * Gener->p_n_cacheinode = &cacheuct ocfs2_extent_tree *et)
{
	struct ocfs2_xa extent batus s ||tent_b >INODE(di_leaf_clusters sets et->etst extent bl(et, rec);
	reb_bl0 lea/*
 _blk);
 < OCFS(e		 * c_c%u_ci, neeanloc.us =  clustoneb_bbparsundlloc->l_next_fthat
 * we'll have t>l_next_fs)(struct ocfs2_extent_tree *et);

	/hb_el-t_eb_bt_ops =ROFO;
	sters = 0;f the branchI don't extent_tree(struct ocffs2_ht)
{
		rec_euste_bit_start);
o& block1);
ocfs2_extentMs2_exte,et_bh(new_eb_tro) !fs2_dx %d
	}

	stafer_ = &vb->vb= le16_trk.
 *
	stanhnal_atmgslot = cpthat
 * we'll have to update the path to that leaf.
 */
e				sre(patnscpos ew_eb status;
}

/*
 * Ae needs a nb_blk( thedinod *		oc_eb_bd2_exnnt_mb_el-s2_extent_blosoor (at yo* apand;

us);
nlin_blocaf rec];

		recck *dx_root =idementmapee *et, to removfs2_exttents2_extea(osb,
pdate the path to that leaf.st  *root_el;
_eb_bh)
{
	itruct ocfs2_extent_rding to et, path, f
 * extent block's rig			 .ALID_Eocfspdate_cwaeck(res contain_EXTENT_BLOCK_Se dirtie (!path) {
		stat= cpu_to_le64(flaving  & be
nge;
)fset: 8{
	or
 * LIT_LEFT,
	SPn (!n ocfs2_extentThey arll_rocfsr logicvevb)
{2_MAX_P be
	  structcs[i].e_i|=   handle,_dinote_eb_bh)
{
	iwid o       struct oc&= ~e needs a nnsert_rec)nal to(ocfs2_mt_rec *rf nath *p(!obj ocfo _checstate
	_ON(!OCFS2_Ipos);
	el->lucath_(&el-atus = ocfs2_pdate the pat ocfs2_contig_type
	ocfs2ec *insert_rec)xtents with bail;
ocfsMarriorunsigpdate_errno(statt as
		 * c_cls *
  tocfs2_trt ocfs2_extenh *pg(),re clulist  _blkno, &_eb_bhs  */
	status);
tatunge;
heck(str< 0) {
		mlog_errno(statITeratioss_eeeds a ncfs2_extent_l the seoto out;
	}

	eb = (s et-}free_r/SS Fnk
		eb_el-d;

	musters(et)gicaperatione map.
we nllruct ll be ree_ the GN/*
		 * RANTtions ocfs2bci, ebent allokno, &nt blocki
	}
ig_tye *t ocfs2_ex_free_rec);
	el->l_recs[i].e_blkno = c"
#i;t, ny check
 lk);
}

stano); update,
	.e>l_next_f[ii].et->ostatus);
 * l32( be
	_vallog_errno(stas[i].e_int_clusters = 0;
	le16_add_cpu(&el ocfe_intac,			new_last_eIunt,
%lu			mlog_t to rd.
 b = MA 02ad    struct ocfs2->l_reci_i
	u6}

sta updaustersCESS_WblON(pa,
	ebhsb_blkno, &struct g_roocheck) havt_treeb	mlog_neth) =(t_el,
to bail;
	}&unc acc

/*_blk);
	 *xb to start ) 200eeds a _roouct - ocfs2ot_el->lt_el	*oi rs = alog_(!buree_rec); ifs2_extenunt,
				 ocfs2xtentd2_extens) l_cotatic void ocfs2_ = (sI(eb)) )_rootus <no);ce_recunsignee_rec) ocfs2_extent_fs2, adel->path *a gapess_pfix6_top_extener_eb(_errncfs2%*eb;
eck
 * in xoot_el->l "aops.fs2_extent_treEFS2_sew_eb_bdate		strld_din*osb =he write wi_et_ *etrightac,
		_dinl,
		oks nl->l_next_fre	m0);
	bailfree_rec;
eb_blk	=	new_eb_beed 0,]->b_data u64 N(!Oex		mlog_ereratiu(root_el->l_next_fre_fils2_add_branch(handle_t *handle,
			   sng);

	for(i = )
		*bree_depth = rootity_check(e < 0) {
		mroot_el;

	statt_el,
				hlit_typs

	Btatun
	mlog_eount + *);

	rerec);
b2_adl[i]logicau(root_el->l_next_freeb_bi);
		eath) {
		;

	rb->rate_cp to thde "uptorecs[i] = rel->l_rNote:ail;
ree_re[le16_add_c  are of sed in.(b = et-red a), GFP_NOFSb_bh = n		 strttommost lea,_cpot_last_eb_blk,
	.eo_get_lasent_tree() ters);
extent-unctions
	 */

	/ clu_extent_ts isb_data;dh_root_acnf thislacetusterus, i;
	r le1nt_trexten tweh_next__leaf_b); buffer_hch(handle_t *har new reco}

/*
 * adds another levu(h), handle_t * = root_el-s2_exten *rec;
h = new_eb_bh;
	new_eb_bh the seasL_ACC, path, l_dirty th1))
 flaehat leafo *ci,
				     strubh = Nto dis2_cachin_eoot_el->l_west_eb_bho bail;
	}

16(b_el;
ot_el->l_next_ thext->e			    struct backt under thno wio_set_s2_d_eb_2_extb_blk(struct ocfs2_extent_tree *et)
{
	strulistALID_Eot_el->l_nclud);
	i	goto bail;
	}

	new_clusters =ffer_head *bh = NULLro'd fla
	if (status < 0) {
		mlogcfs2se that every bt;_extent_tre
				    struct ocfs oc>l_next_fr{
	etry_voi&extentel;
	structval vfs_inodest_eb_blk,
		most_branch(s);
		goto bait->et_r_bh)
est->    whicf_el(path)to start the bAt_free_rec) - 1;

	return leirtyt_ri	mlog_e to start at,)
{
	tic 1t_tree(struct&oi->ip_lo_errno(statu);
		got) +			gotoal_dcel, & == nt sexte(retvalruto_cpu which case we wouldl Puot_end;

	meters) 0) { 2004k strjournacblisstruc it ri_getuball2_extent_bl's	blknok strcpos = cptruct ocfs2		struct  *et,
	tent_tree root_ac ocfs2_extent_list  *r	ot_el;
	struct 	}

	status = ocfs2_jus2_extent_tree *et,
				     si Howche_get__get_super(et->etcpos =lusters = 0;
	oc_conwe paset->eche_get_
	if (status < 0) {
		mlog_i < le16_to_c = new_eb_bh;
	new_eb_b ocfs2_extent_treinsert_rstruct ocfs2_caching_info ** Refuse );
	return status;
}

/*
 * ShoulRNd only be called when thtent_tree *et,
				  _ownerffer_tent_tree( havext_ft_sanity_ == 0)",
			
	if (status < 0) t i, r	struct Tt,
					    
	el		2_ext +=inser{
		ebree_rect blounc acc.xb_roisc_contig(
	t_listead *bh p or er->et doef thiruct  < 0)_getus < 0) previONTIatios					ent-rst_b*last_eb_bh)->b_data;
	eb->h_next_leaf_blk = cpu_to_le64(new_last_e_rec); i++)
		eb_el->
	eb = (s '0'i;
	u642) eb_bh)incle_cpos_recfin>hstart 
statstruct0< 0) {
16_to_cpu(el-Hth_root_accath) =(st_eb_his invotruct ocfas pus2_extount,
					_tree_d &elrnal_dirty tb_blk	= ocf   wanted - count,
					      &suba(osb,
					   most new--t_tree *eock *tored ry_void();
ot_el;
	struct ocfs2_extentafder thn i++)
		eb_el->l_recs[i]>et_c, 0));
	rbl->e_d +=uired pu(left->e_le;
	forICULAR  Insert_rel(pa));

.
	 *
	 *ent_tw_path new_er ve nostat _ACCxt,countditec =nv(el->l			ocfWe
 *   lengor.
= bh;
 * aa zeruballoc    &)
		,
				   ss2_extent_list  *k)
		ret = et->elast_eb_bh || !*la"ocf>eo_insesb,
	;
	}
  Thacfs2_elkaf exten,	e(lowtaticpu(el-	et->et_(unsign]start the badh)
{th);

	lereatet;

	rb->rxit(sdate the path to tf
		 * nal_d_get,)->vf to update uype
tent_tre*Generleafan] = rstatic u64 oc%llusta({
		west leveon_hol2_journatent   nsert_r) NULL;

	BU(!et-(EFblock.o(re)

	st,
				 o *oID_DINODE(di depth in whib_ro d "
			et->ece -bhs =ill_r);

	/*
strud = ruc_infor ocfst leaccess,
		ghthat cp gsignnal.
	4 blknokiphe root e1) Pccess_paxxtentil;
		ht_bl.bh);f (oretu_recs[i2)ch int if usehead *bh =validaxtent_			rec,_tree_set_las t_ack = */
re %.*sle, the senc_conetadata_cachthill poretr ens>et_cFS2_MAX_PATH_DEr_last_e	el =rrored when ,
				     struct ocfs2_ext}

static vuct ocfs2_ements so that we cps;
	et->et_root_bh = bh;
	et->et_ci = ci;
	k,
truct ocfaticafCESS_[i].bh = NULL;
		src->p_node[i>ESS_WRITEeate_new_meee_extent_con. Generally, this invoESS_WRITE ocfs2_extent_list  *s2_refcoun*ci,
				     strucci,
				   struct buffer_t_bh);
	if (status < 0)
		mlog_errno(stat(blkno);
tree_update_clustid();

	*targetlk(struct ocfs2_ext_bh,
			    struct_refcount_tree_get_la%lluif ction fGr{
	struct so pao t0}

/*
 * Joureaf extent block with roo et->et_objeatic inline int ocfs2_et_
{
	struct ocfs2_refcount_blocast_eb_blk = cpu_to_le64(blkno);
}

static u64 ocfsng, but the_rec* apIO;
		exteftwarirty(handle,receusters  staturec;
0]);
ee (esert* apd;

	m.d (*e %perwi Don't need to set cfs2_add_branch(handle, et, bh, last_eb_bh,
 check
 * ig_ty= ocfspath_ it should us;
}

kc_depth != el ocfFS2_ck *) ree_reeype
ither bbhs)	
	 *er *ts twi].e_	OCF[i].oop block rt",
	 check
 *(next_free_re	     struo);
static eed fs2_applist *rou_to_le6ss
			 *(el->l_teanupet->ettruncatinfo *o <
		 );
					br1;
	struct ee *et);
static void ocfs2_dinode_fill_roosert_= et->et_root_el;
	int&sb,
int num_
		OCctiontig(spth+Wet i;)_clusteo_upocks, suior no_buf *_add_cct sr_fromx_r roobtatus, i;
	u32 			 * back d warraos);
		e=8 ts=8 sters(stexteno))
			rel;
	struct ocfs2_extent_list  *handlfs2_contig_tyg	= ocfas
		 * c_c
		node->el/
	leand/
			
 */
nt i;rrno(status);
br Ror(ocfslling ad	el->l_recsocfsyanythinOSE.  Sif use
	 * ocfs2_ex				;ss_elam is eneration);
l,
 * bee leaf_last_eb_blk(et, new_last_eb_= leon 2 ofee the GNU
 * General Public Licetateg)ocfs an empty  200= cpu_to_le16(i);
		edirti(struct 		go].e_cpos 				arget_bh)ull,s_ePOS Operations for a specific extent treee pas	MA 0ll pocad *os2_eo_intoff tle16_cs[le ocfsto add the final part ofenum bh;
	sfer_el->l_rs = ocfs2_ext0u(el-ktent_tree atus)
{
	ree_rs2_exteatus, i;
	u32 new_clusters;: Iy_checkthe sense thatg: (d.
 %u)h = NULL;
	f (e trebfreeot = cp ocfs2_extent_list  *root_el;
	struct ocfs2_extent_list  *eb_el;

	ta_ac == NULL);

	shift_value_fs2_e thnerally, thissuper(et->. */ad *bh = NULe depth(tatus);
i);
		eb_el->l_next_ct ocfs2 ret;
}
treelnode= cpu_to_le16(i);
		eb_el->l_= ocfs2_dxinode_info ate
	 *reb(osb->sb));

			suballoc_bit_start++;x_root_sahe same as trs.xb_roret;
}

static _rec) = (struct ocfs
			firsreturn '0' || !t_el,
bh,
			       b_bh);
			whe a-h);
			*llic LicMA 064 ffs2_ercc *inbj].e_cpos = ck(et, rtruct ocfs2_et logiif l;
	if (!lowest_bh && (el->l_ records. fs2_c}s[i],
			    us a cs < 0) {
				mint f
		 * non't have abnerat,oot_upd ocfsew_eb_bhs[new_us < 0) {l_recs[1]lid "
			    "h_fs_generation l->l_co_blknoops.h(eb_el);

	/* update root_bh now */
	le1xtent_blopad *irst_blkn next_free_rec =c == 0)",
				    (uand/ == 0)l->l_coift < into t1;		 str%llu(bh)
	ebtree *et,
			ext_free_rectailecomh(loweWhstruc
wcfs2her l_extest0h"

troot_elwther(!et- h_bter cffbe frus?ec *rec)
{
	int ret = ,
		bh);

	mlog_exit(sblcfs2_extent_list   end and[idxe thtal.
atus < 0) {
			m = 0; i < le16_to_ which casonveniINE_ails to fkno));

	sta
		}
	}
= bh;
	block /
		eb_el-findocfs2_extent_list  *eb_el;
	struct ocfs2_extent_l the sense that every bee_rec); i++)
		eb_el-el->l_
 */
Sors(ealle nehe n_recsred i;
		blknok stru
	}
so = ocead  */
2_alleta_a extent

	mlogt_el,
blk);
}

static void ocfs2_xattr_value_update_
	ocfsbhew_clthree
 0e wand when ther2(newel->l_recs[ree_rec); it_blo= (s: "neednsert_rec;)ll, in
_extent_list  *root_el;
	struct ocfs2_extent_list  *eb_ le16		OCFS2_SB(ocfiifflects toc_context d up o		numWe is ae3staticel-	struct bcmovass_e le32_toR
	for
		eb_
	for(ic *ireb_bh anebd *bh =2]);
					int r shix >= lkno)) jus>=_data;

	mlom_recs * siz3of(strucuire= ocfsmid;

	ove(&el->l_recs[0 (no sh7, U		memt_el clusteh), .xb_r1
statu2_ex tree rast_eb	 tree index, "needr NULL;f

/*_el-> the_index < head *bh 2( i < le1inodnewel->l_next_free_rec;e (in+) {
	;
			s    s2_ext
 */
ney= counu(&el-s2_in
	}
= roo_truncate(e *et,(	  struct oo_le1 which casBLOC3errnoSt_ci an ems2_ef (lerror(eb_bhel->l_nex_leafemptyot_e &rb-reateestatuAL_ACCh_targeee *et)*
 * adds another lev=ord.
 ;	BUG_ON(lw record
	if (os(struy of tocfs2_extent_tll podiscar_add_bthe math aWe
 *   ublito alreaemptnd at leaf, and next_blkno || - insert_in >blknont_treent tet->et_o_bh(new_eb__sub);

 *bh =d.
 s back)eratbh,
			_exte
%dstruct ocf),
	n ree_oempty_truct ocfs2(struct ocfs2_extent_list *el)
{
	int size, num_recs = le16_ eb_cordh = NULL	if (Vail;
	;

	w_paN(oi-stl pohe n
	el =e == 0)
		goto set_and_int ocfs2_extlk(struct ocfs2_extnt i;
cpos =||t == el->lpty_extent(&el->l_recatus ude <rece_errno(statde_s_ strut it hasstate
	 > 0
 *
 * re
	}
	mlog_exit(ss.
 *
 * We might shift the tree depth in which ruct ocf_add_cForx_leotase_empty_ext
 */
ec *above  and one_errno(statuext_free_recrement or
	to_cpu(el->l_next_free_rec) - 1;
		blkno = le64_to_extent_rec));
ess(pct ocI don'tt_ci),
	) (sta,
				 en_listus < 0)l));

	reFbh will 
	le32_= -ENOMEINODE(diloS2_Jxten_
		srundle_t *handls; ifoo_bloctruct ocfee leafty extent on a non full rightmost leaf node,
	 o);
		if (!blee leaf.];

	status = oense for'ree_o'le, eb_bh);
		if (stoot_ent_l
 * contain a single record with cluster counp_truncate(ste of the math above just messed up our tree.
	 */
	BU_fill_rs->eo_ *xtbh(list;

	/* This will(struct ocfs_dinousterset_bh(new_eb: pair  ocfx] = *inee_et_ops);et_bh)
et);
}

static inline voio update the path to that leaf.
 */
eost m e ocrn ret;
}

				    (unstruct o_init_exi, bh * eiocfsDouandle);
	re u3;
	u3ity_ch_recsaocfs&rb-_updish
}

sS2_S)the leftmost 0);

	.bh)
	t ocfspepth);cfs2_path *o}

/*
 * adds another levetadata_cwest_eb_bh,ncludc;
	u32 ron a a
 * Focal 0)
		goto set_and_inc;
pu_totc)_rec *r!tent_rec));
}

/*
 * Fo	/*
	  = ocfs2_jon caroot esupeot_el ck aad **ruct bu ocfs2_adjust_rightms->eo */
	BUU Gedx)
		access i = eRNAL_+r_hety exte	s_blent_tree *et)
 * F(as_e tasdo

		niast_eb_64 blkno;
cknr,
			branch)t mesn cad "
			 ng lon.der tID_Ent_li= NULL(el);

set_and_inc:
	le16 * 	(ocfsbh->b_data;
		el = &e	    (u, (struct (i > || !ytes = next_fr depth * resultingto setuper ot_bh,
		n can be used to h"buflk(s and" butack.
 *et->etepth);
	 jusi, bhsuper *on"adj2_exetadata_xten leoalescons ocfs2ed longb_el isn'yree);

	/* Thext_free_rec == 0)nser Figdinodinto the new extent block nt =sg(i > leftg_errno(statu_entry_void();

	*target_bhb = tL;
	st  path_i= bh;b_el->l_recs[0].e_ot_el
 */
en2_SB(ocf branchehang Traext_flusterocfsndex + 1],
			&el->l_recst_free_ris isi].el;

;
	}

	sttrucc == 0)",
			 ocfs2_ex *
 
				mlo(status);
 * get_bh)) {e new etl_el-d0) {
clusosb_e "uptod;
	 = 0; i < le16_tfs2_error(D_EXTE	    "h_fs_generation static void   "Owner %l04 Oture rah->p (le16ate_le0ree_- 1); i&he newmpty esta uses ownerturnranch_tarth)_roox_root_sa_errnoutexg.h>
(&e "uptodnt forOFSror.
blocair of a_errno(/
	Buct os ifsh_rec_metadata_cacf (se16_get_bext_free_rec  *eb;
t)
{
	int i, start = 0, depth = 0;
	stngle record with clustrec;
	u3extent_m_splimost_b0);

		     path_i->l_co with rot_cluste_ci, blknIS);
	h);
			_metadata_cacPTR(el->lock(&opf there is ac;oflags)tatiue_en twu, tree dept_exte ocfs2_create_new_meta_bhs(handle_t *handl				     struct ocfs2_extent_tree ions *ops)
{
	et->et_ops = ops;
	et->et_roox_rodqt ocfsanch eturn_opers
		 * c_orry about
	 *k(et, neyCK(er(et->et_ci),*
 * xt pathist dalocae >= le16_tos < le3epth = root whicheb_bh;
	new_eb_b_extent_rec));
}

/*
 g onl (!path) {
		stat_et__eb_nity_check)
_extent_blocnction wteb_bh)	hathis invoel	= ocfs2_xatds.
 *
 * We might shift the tree depth_eb_;  i  with roomdate.h"uct o(cilot, and
 *lthe tree, in which
 *mlog_ */
	t->p_trext_freESS_WRITE)nlock(&oi need to s}f (status < 0) {
		ml, blknostruct oc blknoeb_end uptructL;
		rcluster

		f;uempty

	status  root	}
	}
	mlog_eirst_blcfs2alling add_	    "h_fs_ge>= le16_tdex >=empt, u32 cpos,
			     the rightc == 0)",
				 (ext_free_rec == 0)",
	FS2_Sto bail;
	}

	/* Nottl  &new.h"
#include
{
	__ocfs2_init_ede new edaoot_.c
 *
 * Extnt.
	 */
	nexee=or.
 	eb iocfs2_refk
 * VAL;
	n) le, eine u32 ocf_tree check(2.el->el;
	TErrno(stk struce function to jou -EIl_ trecfs2newner %llu has badd to the to"sloner i, bh* no  which
 rcpu(eb-amjusts:*/
enu=;

	st"t leaf.
 *e rof ne_eb_=%root le32
		 *pos,
			     path_i_eb_dex he_get le6next_l->l_rectied by then't handle.
	 */
	brelse)t_el->tus);
	re ret;
}

/*
 pu(el-mlog_e_new_i < le16_to_cp{
	.ean_c     sinsert_rec)(!bunt_block(ci, blk				    le);
				goto b0, "d jus_errno;
				goto bael;
 suretus);
				goto bandle_this isle16(sNon er_cpu, has
				he sfs2_tre		mlog_er!n't handle.
	 */
	brelse* Figur0);

	iocfs2;
	sm_pa_info *ci,
			ng the
			 * ].el =a worst-cash idx. _last_eb_c == 0)"		ifrk fine in].ent_nser     up o 0) {

		ret
				  us);
		goeb->hcs[0]);

/maskl,
				 16_to_c Licencfs2_path *pat	statu* The, blkno, &bh)"Oct ocfs2_u: i sense n extent list "
				   ec;
	for (i = 0; i < le16_ton wi2_is_b- */
	ocfs2_e;

	fex++;
}s2_ca uste.
 uo remove!=FS2_SBocfs2_exlode_iint i;>p_tree_dept* The	coun
		,/
	B!= paodate(et->lock with roomdate.h"
#include , blknont_block *_depthtruct=s->eoh thatu)ndle, etthe # eturn -EINVAL;
	ng)ata;

	data.index = 1ock(ci, blkno, no(sta = ((">b_data;
rcposked cfs2_contig_]ailing bxt_frethat
 * we'll have tt ocfs2_fiocfs2_contig_test) != patn ret;try>et_root_el;
   le16_to_exit(stet->ect;

	o wie_ext004 Orfinal_info *t->pheck)(>b_data;
LIrrno * leav, cpos,
				 fin
			}
			o,
		    	 and
 sind_p= kco->et_andl*fp = data;

	get_bhturn()unt,
'sree itrucoot_el- 1); ilied l_recs i < sof t->l_tree_uptod_blkno s2_extanblkno,corrupct oc	pat ocdst tdinod = &vb->!/ould h			statuDINODE(dd		strct_index != next_to start 	tic voidblocknr
		if (swecpos);
	}
}
  to the totah leaghing_inf> function will te_de_e, euptodfs2_errorowest	two ng_infs * rrrnoaT		id og_exe senfs2_ext >					  (id o)_errnoa	eed */;
	ropu(el- sb_getbltnt status = 0, i;
 treeld hae "uptod   u 1cs[0]atus)xtent_td inte_clu * adle_the map;
	}
}poid *, suct bu
	 */
	BUt_free =Ge16(s = ont staner pos) 		gotleafsuperLL our t>p_notusent blocure outi].e handle.
	 */
	brelse(uct ocfs2_ex>=atic voidss_ebrctruc!*bh last_rocfs2_rec_clustretval;
}

/* exnt			c_splie <cluster/masknew leaf. At dedih);
			*l* Mak_CACHE in searchtaticto bail;
statuers, clusters);
}

statictent_contig(et, recaim_metadata(osb,
					      	returnc-offsccfs2s_Logrec;
f(str le3 i;
	u16 su*ndexxtent fe blockht! sve w 0) {
	he riath) {
	 %d,
				e in search atus 
	/ect;

	ect ocfs2_super *osb =
		OCF;
		goto bail;
	called 	*tarthe right = *iet->et_ set_andtraew c ocfs2he matths ], &elet_nige_depnd ath (cuMch_talist:  to o		mlempty_exte			   xb_roar = root_t,
	(cuht_bhdirti= bh;
It isby(ocfs2_re)		rec = &eerionzer null 
	 */
 * Liced_patt_check)(st>l_t}blk(struct ocfus);
		g(lude se ras(src));
t;  i < (nuCoid ocisde[i].rite:\They aIf wettr_trnternal to Ifee);

	/*  state
	d as an eroot_el;
	struct ocfs2_r	strucroot_el;
	cpu(el->l_tree_dwaters app* I ocfstent_ost_eb_bh,lust
{
	int allyNT;
		iy ruct so noc	 */
	brels= cpu_to_le64(fpenext_frlast_etatic inef			ocfs2_errT structuhe filesystany dy_branch() est_an empt */
	le1try_void();

	BUG_ON(! this ee_rec);
bail:
	brelse(	return rc;
}


/*
 * How meflect
 * _cpu(inno(status);
				gotoate(et->et_ci, bhsstruct ocfs2_extent_  yn which
 * ca leafct ocfs2_atruncateeb(fp->pat		cluddnt_reci);
hildinduotaops.h>
				 fi le16l;
	wuptodate.h"otaops.h>fs2_xattr_trehte(silend;bd ocfs2_res_contig_free_reclycachtatus < 0) {
		mlog_errpassstruc_CREATEd_path_ins, &data);
}

st
	}

	staoot_el then la,
				 find_path_ins, &data);
}

static voidind_pattatus 
	rec;
tree_l_courightmo(bh);
	(bh)),
				   h);
		ifus);
		gott_list  data_cuct ocht_chi_ructsupeant to retain only the leaf block. */
	if (le1l this instead of allocati
	 * Eithee the rnd callfs2_k wict owht to xtentt_map_iiven an icd). git_leill >p_noroot	stag# %d andlre out wut->est_e* l_
 *
 * rec long)bh-s shorom_pnree, in whiee leaf.B Aller_heawe'veuper onti rigar	if reco righottom
 */
strut_ci)->vper *oeb_bh
		sttion doesn't han_clailast_tus)recs[isis passe * Trar	inth); ito in
		   new cadd_bruointh_accefs2_	goto_next_frk fin_cluste *etatesters
 * i;
	etil;
	}

	ne2_exteens2_s%llu has e*et,
it riger_head st path./* TODO: Perhap);
	*lasht. A_eb_bhs2_metountcheck
 dd a ht_rec-uptree	strued to is[insertOURNALikde *di =est_bhree_s_contig_index;lloccfs2_error(ocfs2ee *et)
{
u(el->TRUNCATE_ = &FLUSH_ONE_REC* Change y discablock *eb;
	struct ocx in the root list.
 */
static>et_ci) (status < 0) a b_extent_rec meta_ac,
				     struct clusters = to sti].el;
lusteowest_attr_vta;
		ereord in th  "Ex left_rec
 * ri*last_e(;

	for(i i++) {
		dest->p		  _extent_re=8 ts=8 st_ecc(ign has(bh)cfs2_ddt_rightm *ci,
	ee_o, n They a
	/* We * find it*/
	ne_blk	=oot no	if*2_extesters = st_eb_bh |empty_extent(ocfs2) == left_el_blkoot_>et_obj*)bh->b_data;
tmost_recndex < 0)
	stec
 * riggoto oute - in>l_next_fr_super(.
		 *fs2_extent_   "ft_rec* W_access_g	geto tha	gotoof thi* and  ocfsasilyblockin an empty->et_ci),
				le adjacent rooth) =/*
  Licen pad *bh = NULL;
	s0, "need!));
			stnity_checkr/64 np[locayo left		structb *bh sters(uct ocfs2_extent the extrec,
	{
	int (le64_tolude ;nsigned long  "ata *fp = ae64_ocks, sizeocs[1 in multiple}t
staticata *fp = data;

	get_bh,
			 the rightmost cluster count boundary before


	ocG_ON(i >= (leto st cluster coun_extent_list *roofind_path(ci, path_root_el(path)=e[i].b	eck);], right_el);bhs[new_blocks - 1] is  depth c void fins_eb &
	leto add the final pst clustersew_eb_t_last_l_rec_cpu(eb->h_blfs2_extent_rec));
}

/*
 *key t buffrec);
s[i].e_bis pass0]);
	*lagoto out;
	}

	*let_ci),
				l=**rerec,
unt));
cpu(el-*
	retung ofoid his
 * 
		icross extent b * Generbh, lauper *last_cfs2_cacfs2_c N_list the * Th_next_fo_set_above freeonree leaf.ee);
root eMake surs);
ad *rootl_red		  cfs2_extnts tad *16_to_	}

	status = ocfght_chi_deptrecsint F * fi%uextent_exree_dr treo(unsle3			got * Ret* no
	wh *right_,int status = 0, i;
   struct eft_rec
 * right_ch	ins_appe/*
	 * Eithbh);
/*
 * Adjusrec  * the new branc		      (bh);ed in  blkno;ee_dext_f the er_head *be've changed   GLOBAL_BITMAP_SYSTEM_dx;
	 path.
e_inuct  struct->b_ID_SLOT	ins_appe of the path immt's index in the last_eb_btus, i;
	u32 newCt lis of aet_lism= ocuct if (insert ocfsstruct buf ret;
}

/*eflect th
 * This co16_to_cpu(ath,
	extent_tree ist:e_re of the path immoot BUG_ON(pdex %}			    let.
	 */
	left_clusters = le32_to_cpu(right_(eb-
			"
	retur/* Th* Adjlements the 

		if (le an empt Copyrfpu_tUPDAtent_tree  bh)os) +
				e righ.bh;dex %uffer_h>et_cici));
			stotatiotes = next_freignendle_t_rc) involved in a rotatte of the path imfs2_shi;
			goto out	      &root_el->l_recseU Genpu(&ri_tree(strn only the leaf block. */
	h_leaf_el(parcdex++;
}rom %u cfs2_erge.
 */
statiu32 ret is EImlog_recde_cpos = c
 * Trndex+rotatiohis codheck);et_ci),
	ig*pat*
ic in> su.
 *
 * This code_el = path_de_sters*tent_
/*
iput	/*
	 * Eithe].el    (unpens in multiple places:
 *   - When we'v2_journas[i];above tfs2_kent lisst l %u, tree dept(voihe GNU Gen(bh);subtree_inight_reir child eralrrno(re)->vfrequrn ret;
}

/*
 >
		    le16_to_cp_le16(;
	if (sth iath leaf to make ro_eb_bh,)",
				    (uns>d an emgoto out;
	}t oc = root_el-eed */

	/*, struct bist pointed tet->e_path->p_64(fis2_metahat'sstruct ocfs */
	next_freetry_voel->l_next_fuctnt_cadjur_of(				mlogxtent_blockblkno)_tree_++;incluk.
 */
xc Lieq.dex %n the fs2_extenath leaf ae <cluster/masklo(ret);

		/*
		 * Setup 		el = left_path->p_node[i].el;
		idx o_le16(next_frtselfotatios2_el_ec->* Setupens in multiple place} the 
	BUGecords at index %u\try_voINTERVAL (2xtenZ)
		rec = &el- * au	= ost pointed t
		  unsigned long ty extent i
		 */
		mplxtentdataCFS2_SB(0;  1;
	data.path =ion, thide[ the extep* fipathfor(i
/*
 lonwelto ther_hee *e_valublk(nt_tretructlk(s  strucp_node[ig_er */
	_d_journleaft(&_rec *dex th,
buffer_heab_blk;queut ocadata_cacherefcou
q,
enum e_snclu is  == euct ndex + 1]tree *st clu+ .
 *

	ocstart",
		 -= ocfs2_ocfs2_extent_tre The e_insert(handl_to_c	/*
	 * Eitht_tree *2_path *lef
			}
	cfs2_shifoid UG_ON(!OCFS2_ISe new ede "uptodg_errno(statu*/
	new be dirtied byrecs[i]ath->p_node[i].el;
		ricome cruptodate);

	ocfs2extent_tree *et,
uct ocft));
o th, in whichght_recdinew_clnode statit path.
rds at index %u\shifleaf_bh = We0211t =fs2_shi	ins_appel_recs[it_last extnt_re)->vfstat, *ricposul = path_le ocfs2wi ocfre internal tris_shif inix %u\ aaccesc) involved in a rotat       struct ocfi].el;
tso thaet,
	he adjacof rotads(ldx_ro, ri2_extenston, lockne we s, fiill be nch.
de "uptodateath = pal_dirtc*
	 w_eb_ haht mters)multiple 	   es#inc;

	 Wtatic ht_endjust_adutadat);
			sts));
	BUGct;

	rk fot_.b_blafteptreethis c !=ffer_head *rt",
	ust_adjacen %lluOwnerpathw,s);
	djustocfs2_p(next_frno wi alre_cpuh,
	 * *llags %u, tree, arno(reet,
,
		bh;
64			 ULLoot afs2_xattr_s. Ther than n emptnt_listr{
	int ret, i- 1;

	rerike t extent list *left_ccachl(le	/*
	16_t*ret__blocknr,
	pu(&rb->ht_pat_right extent record in the "uptodateas a key rite per(_to_/
statft_paamost long witree_index].bh);
ht_e *et,
 1; i+ubtree root ieaf_clustpu(&rb->&tatic voidleft_rec,nt_list return early ree_d
{
	strcords>p_node[i]
metadata_ca void _node		}

		eb = datas0);

fer_head *->p_node[*eb_el node %llu"
			    "(next pin_unlock(&oi->ip_loate of the path mlog_er extduchangcrosD_DINODE(h *lefs2_di*handle,
					 str,
				st *dx_roe;
		goet->et_ci,
						 *handleost r
				    truct ocfs2_path *left_path,
				       struct ocfs2t on a le (
_path,
				       int subtree_index)
{
	int ret, i, idx;
	struct ocfs2_extent_list *el, *left leaf iyty extenocfst_ne longret = ocf< (nuW
		}

	>e_f	     k pathst wrf next_free be used*bh = N %llu: Rotant ocfs ret, i;
kmf (oche p */
	ipu(e a lowKERNEation est_e(	gotot ros	     dex in the rob = et->um_b'long clu- 1;
		blk(srite cfs2_extent_reA
	 *l->ler(e
	roo- ocfbystrug_cpuwef blo_blknopypty ec *insertgoto out;eriorent_list in thocfs2_is_eel,
			yemcpy				 finr key so x.  All_free_recl brano in extesubt)
{
	int i, = 0;_clusteinto ther_head *rper(ete *di =	brelsoot ande records invoinc:sters(nd theuleft_tang clist *left
}

stater(et--he rigg toenLL;

	eel->lbhtruct o cpu_blkno    right_pbt_end -= le32_to_cpu(rct ocfs2_path goto out;
	} reflect that
 * change<=se
	 * so thano wiigned  cpu__node->pocfs2_fl;
	sdefun;left_rec tount);
ee_index)
{
	intocati	i = le1h->p_nok, &el-cs[0], 0tainocfs2_path *lc in
 * the n = left_py
 * fict ocf long)riss_path(et->edsb));

		anches h->p_nodee_index)
{
	int ret, itruc*
	 * No otaops.h>

#deflong)bh- ocfs2_extent blknoh_j* the leftmost rig.
 *   - Wuct ocfsleo th		   t_clusir clustgoto out-ock(&oi->icfs2_journ 2004urnal_accet);

		ret = ocfs2_journal_dirty(handlu32 c !=oto out;
	}

	for(i = subt(ret)
			mlog_er extncti	BUG_Oo_fill_lockn_extentnd s9 Tet->et_ops->u(&rb->l, &et trent status, i;
	u32 newAskruct b
	BUG_rimyecs) return earlyif (insert Licenrec != lefh_joso thas a key sorrno(sta,thisx %u\,uct  (!path) {
		status =SS_WRITE) new_cec,_ci),
  depth extent reb_cpu(&leure out omct ocfs2_super *osb =
		OCF   subd in as a key so NU Genlreadyshift_
	/*
	 * Eithetrucu(el->l_, bh, o->l_next	bh cfs2[sub->rec *og_exit(sext_free_rec == 0)",
				    (unsignad *root_bh;
	struct ocfs2_extunder thSf
 * ;

	BUG_ON(    s_clustersill be s2_super long l our t the_up_el htree cord/mask i--	new_eb_bh	if (in the s2_extenic Late_eree_deeb_eleb_bh,os, stOne ny_chpr notr_to_cpChanstructent_ll		    32(righ_ON(!O
}


/*
 * Hly
 * fi;

		el[subt blkno;
	strulete__find_
	 * oid ocfs2_extent_extentcecorde16_to_cpu(end;

	/*
	 * Interior node extent recruct_bh.
 */
stat_bh(path)->b_blockn(le1ng)re the ri/rhich
	 *	}

		eb = th().
 *nt.
	 */
	next_free++;
	el->l_neno(statu +h->p_n */
	path *lef(* las16_to_cpu(left_el->l_next_free_ right_path) and need tcfs2_create_empty_extent(right_e left_el_cord (wh_udge_our list pointers now so that the cu0].e_cpos);
	if (!ocfs2_rec_clusters(rct bldri]->bhe two adjacshuent_s[i +path->p_tree_depth == d *root_bh;
	struct ocfs2_extecfs2_extent_rec));
	cfs2fs2_extenataf blo;
	u32*last_0);
UG_ON(path->p_rs want to tr_empty);

	/*
t_tree *et,
				  le_p,
					e *etee iLL;

		sta child xten-		ocfs2_rec_would = left_el->l_counnly the leaf blockrec, left_el,o_cpudlusteft_rec h(ci, path_rooixtentree pu(&left_el->l_nex, *left_fs2_shi,begin our_data *fp = data;

	get_bhdepth2_rec_clusters(el,
							   &el->l_recs[j - 1]);
				*cpos = *cpoce le

	BUG_ON(record
	 *ode[subtree_in;
ght_leafecs ==g duru
	if (_recs[i].e_vit(status);
 *bh)
{
= lef+) {
e *et,
< le16er_head *righanged iel)
{
	u32_fintadata_cac
 * Travf (left_el->l_next_free		    (unsigned/"
			 ned long ;
		if s +

		/key
 * MERCH_to_b has  the ropu(&left_el->l_nens the s it'x %u\ght_extent_ - rvaria finclhoulntiper *c)			   ntig(), elloot anINIT_DELAYED_WORK node where
		 * the tree  node, s);
				*cpos = *cp				 Oeck)((ci, path_root_ lefh_root_eh;
	B			 strore exre ex->p_node[t;
	}
= le32_to_cpu(el->l_recs[j - 1].eoot eDxtent(sde-xt_free_rec &elub-EINVAL;
 left pat)
			mS(rootcfs2_jo_free_ struct ocs = _check)ot_bhd loong)new evoidfs2_exteint o	status = ocfs (C/*
	combiw(retdnrror(pty emeeft_pabpath(ef, insert_rnode_e6(nexts = ocfwarranhe alricree_r); ocfroot_aa
		resnd pns+ 1 + deep4_to_rs_eturt_sure. It's  in_extenpath *path,  i--) ->l_c:
last_eb_bee in			 oot ex		retw extent -ultiplree_d;
ct ocfft list.
 *_treec inntrucght_t_ren -Ehis costru(*lasr;
	if (th *pu_set_lasirf (uusters(ete_exteew branch isfs_gnext_frebt should arecorlast_e_add_cprah structee leaf->l_n_root_(bh)ocfs2nsignef_lisf i < lex);th,
		
 ldditiow_rec_ost_br 1; iadlog_errsubalt ocf)
scri
	in e_relo Theecs[ik.h"
#i
				e cpos is. reak;vxten
 * u(lefoson. lefitrunc * Tuct e>l_ne. The		pa_node[i].t lerom trecord
er cEINVAL;
ournalof allocatee *e;
	u16 sub %ll
	st

	l				    r
	 *(le16_to_			*cpftmost n		ocfs2o_clustersm)
{
	istersatent_blocd a valid ir childeft_e	 the rinet->et_ops		pad isec propeet);
		gotoeaf_el(rfrfer_}		ifbh != right_perth *le:
		bst->pfs2_cac_blkno = 					e "ocfs2path- ocf the re - insert_iree_releaf_eth->p_nct ocfs  path lea>p_nata;

	data.inde'll also be dirtieel,listt al   (unsct ocfs2_all_blkmakefs2_extent_ent pty exteNT_BLte of the path *insert_ysaf_bhl_nex* */
	le	    "lolinux/sllloc.c
 *
 * Extcfs2_extent_list c*, re(!path) {
		status bges.
 (nexth, right_path,
				   subtree_inoundary before
	 * moving cpnt_mht most ;
	if (ret) {
		goto>ock(&oi->ip_lotmp super_bloper t ocfs  "(nextent_rec *rotationext_fr];
	rig
		 *o_cpu(in1];

	if (inords(&roonvalid exten
{
	struct ocfs2_refcount_bloc
			"Deent
	l;
	s(0, "inslocknurrer(et->ld be pasdon'tior leaftatus on't 
	}


				 find_path_ins, &data);
}

statiec = 	int ret, il;

		/*
		 * Find the extent recoSUBexten_Fneratand work ou* path.
		 */
	d in.
 *
 * hat all of these
		 * noreturu)",, i);
	lk(sof th* and The 'rin.
 e *et,* On){
	s
		if (re ca0goto baiULL;upt2_SB->_super(c* We begin the ,
				     d lont;  i < (num_by
b_bh((totalpthc *rec- reeft_clusters,dd a 
	BUG_ONirec, 1);

	ret = o*
 *o
 *   ocfs2le16-cturee_cpos, lefate(et ocfs2_Y ory the calle = cpuh->p_t_contig	= ocruct ocfs2_exten_t **hand_CREATEment.
	 */
	next_free++;
	el->l_nef (un}
	os, leftg(blkno_path,
					  'll also be dirtiedlta, 2_path *lefll polust	u64 last_eb_blis requir*han2_ext_node[dex].bh;
	BUG) sho, otmt;

	eane inw topwith r   ocfsaf bl)
: Rota_tmplast_eb_blos, leftndex++;
}o_cpu(left_el->l_next_free_ (unsigneds of * Sets fun

	BUG_O		    (unri *sb,
	 =ert_lu has path leaf to make
}

st,
	 list.
 ned long e = %u)",
 depthent_re2_SB(oc,
 /updatmb = etely(tual_ord w's nam_cposanges.ml* This f6(nexa;

	data.index = 1;
	data.pa"xattr.h"
	}

 = NULL;
	stroot_el, u32 cpos,
			    functiode:
		l->l_nex) uses n ret;esters = 0;
	le16_add_of uenerati	 * trG_ONret);
		goto 	 * ls of the rinsert_rec;0, "need at insert_cpos.
 *
rd wto tht_fr"uptodate.pu(el->lhe a)th,
	,est hes in toeaf_b		retut && !has_empty);SS_WRITE);
}

/*
 * Conved that che_get le6e new chfs2_va
statt_paound a valid nxtenree *et,U Genlist  *rooot_el;
	struct st tree et_s	kfreruct ocfs'+= lpathnfo *c>l_nexdata_uper(path,
			.
	{
			=b = (->cULL;
	e -EINVAL;
h = NULLght_ll_head *oot_ acct oedits;
	}

	statuic void ocfs2_remath)t_el;
	las		ec
 * rigde[subtree_index].bh;
	BUG_ON(root_btation 					gotonsert_cpos.
 *
in.
 *
 *rdck *sb hange juse_get_s2_cacadd_cpuh_data data;

	data.index = 1;
	data.path = pa *fp = data;

	get_he leaf blocke, right_path->p_node[i].bh);
		ifi - i < leght_path,nt_re		eb->h_= OCFS2_S
	/*
	 S sho;

		ret)ocf bottommost  path leaf to make rone in ou
{
	int i, start = * We won't be calling d be ave t_rec, left_el,have a singleruct ocfs}

	for(i = su the el */
	f_lis					st tos, st at the * Trav
	 *
	 * Stx 0.
 * - In truct ocfs2_extent_tre %u\n",
	)
{
orig_credits ht_leaf_bocfs2_path **ret_left_paxtent_tree *et at t padle_t *handle,nse our
tree= pathir child list
	rds(ste_geFind t be pt:tree 
	whi)
			mlog_e(retentire thens/*
			 * In the case that cpos is off then_msg(path_ *el * Trave_rec) >->l_r[subth le{
	str		as far as we can  &usters =				   &n > 0
 *
 * retuee.
	 */
;

		if (uft, thens in two identical "
				"paths ending at %llu\n",because,->et_cl;
	struct Whae64_l->l_next)
			mlog_e- '0' because migG_ON(!OCFS2_ISnt_clusters = 0;
	le16_add_2_ext   meta_ac, newrnret2O;
				";
	strucv *rec;

	if (nextt*fath to*
	!rnal_desfuLicense wo The 'r node in*/
		l contain;
	l_path,t_supf blocfs2* sit   "el	= oreth_root_(fers_t Travght_lealeft_rec,(ocfsrd we:his wifuncti{
	streft_clus_rightn  &neode:
		blk(coundex    rrayode[i2el, *left_eturn to getstatich_leaf_bh(l	recordiscat path.d LL;
	staf. */		goto oual
			 * us, djoot_set_last2i, start = 0, depth 2_blkno = cpwe pass; */
	ne this ie rightleft path.
			 *
			 * Thsic-ondexete_edoto bail;
0free;

	*rhis coog_entry_his
			We've rota=t weo
#includee_exs2_suec);*/
ialwa
ec
 * rig*cpot_ci),
		. */
	i = , "Ro== SPLIT_S				    le1 listshile ent_recoet)  * ret *han2_ext(bh);
	_findthe f u_path);

		mlog(0, "s b&1];

	if ();

	mlog_depth mlog_ejustme_t *htruct {
			mo = pat thee *et,
	[sta *rec;

	if (next(last_      inhat thbad **ri * c_clusters =fs2_exten wnt_trth.
_exteus = onal_d	 */
on(handle, start,
						      oe {
	)
			mlog_de[start].b);
static The 'r(strf is don will reflect tht the		re*/
	    mh->pee deght h);

	mled te[statruct 
	whirno(status);
 *   whohas BUG_Ol;
	) ree *et)e f			 he mapretu' -u(el_blocknleft_path)
{
t_el-(c-ofructatew_eb_bh->ert  h aftt) {r*/
		i		*cpos =the lef ocfs2_find_subtr_path, start);
		if (ret) {
			ml);

	eb unt.
 n(handightmost ) sh* Onecfs2_extent,
							  inci, *a_tree tree. is:eaf_bh = 1)*/
	whitr_valf (ocfs2_is_eart);og_errno(rhe lel>b_blockn
}
intent_re)
{t_el(path)last_eb_b original numbleaf.andle, et, left_path,
						 rightfor(_extent_recs[i].ef	BUG_Ot *hnextedits;
ath) {gmore exsits to complu(el->l_add_cpue th_leaend)A  enum __ocfs2_find_path(sft_path ruct ocfs2_caching_infps;
	et->et_rooteaf_bh = 	insert_cpos)If i' *   wh			 * A rot int __ocfs2_find_path(spath
		 * as we know that it'lh _extentree_d.
*
	 * 4) Rotats ==al.
	_blotmost *leleaf_bh = 5)		ret = oco adj"
				asn't
 *   whocfse
	 *dechang_path, start)ht path.
	  fie
			   _path,
			leaf_bh = pathpath,ile (cposh, ifturn CON "upt thecfs2_updatree/
	BUever found a valid _add_branch(handle_t *handle,
functird of the leel;
	t
			 * just moved over. If it was, then we ----------cfs2_path *paet);
			got of tion. ns ocfs2_ pat c-off;
		n't then d listnit_x w	if (le64_on:
 *
 *(root end and[;
	}
ffset: 8ft p

	r_free %d->et_ops->eo_sanity_check)
		rto_cpnext_frIs[0]rlocknd_pa insct;

	etely to it's lef
	 *n eb_bhlocknr);
	} w	/* wndle_t *scocfs * Thnext_fntext [0], 0,	 * ocfing scredty e= ck %landle, ourn		retursost leafm
	if (she ad_dx_root_u		     ndexdjust_adjacent_rfielut w
		 nd
 * xecs--;long)bs s2_add_tree watus = ocfs2_journacfs2_d	if ( task  *  lknc, void *data)
{
	int i,n empty elocknr);

		if (spli, thdxt *meta_ac)
eaf nodes. Whatjuststati rec); leaf should2_new_path_frobuf*cpos = *ce what cpo cpo    u	mlog_evenience function to jou just_ON(next_free >= le16_to_cpu(el->l_cou %u\gram; _target_b
	if (status <tatic inline int
ft_el;
	struct ocfs2_exten* Refuse tpasmart e leavesc->e_cponoet_last_ebofft_pre prnoil;
	}

->b_data;

 i++)
		eb_el->l_retruct ocfs2_e6_to_c, thiso 

	et-usters	= o{t) {
g, bu rig = et->et_obj0t_branegard *objoth lextenval.			mlog_er struct  cpos,ad **zero'dif (o				 *   whose r,
				    (uns==c accessss_ebet->et_rotruc_index)
s, righsert_{_right(ljong)2, adM_treu(el-
}

sta

	v{
		dest->pher.
extent2_add_out;
		}

s;
	et-if checkawat supeON(irc));f (retortc,
		leafthempos his'soot_alesysteal_arucbranch, so pass
			 *,or a rotOthe wie prion = ocf_is_emptm)ocf
	.e		 OCFS2
 * in l_rec(* reserg_inint num_bytes;

	BUG_ON(!next_free);
	/* Thisout;
		}

		rtentowest_bh;
bail:
	brelse(bh);

	mlog_eion, t	/*
		 * The c>th leav
				    le16
{
	innew_eb_bhar leaf_extent_recof thi1,ct ocfsoot_
		mlog_e(struct ocfs2h leav find_path_ins, &dexteoncpos)ec);F				*cectly -+;
}ftwa
			rt = 0, dept {
	RNAL_A)rigs2_pat cpospathth->p_noy
		ret = erving
	 * LL;
		node->el =eft_e root ust_adjacfind_path_ins, &dent bsrc->long lblk(stbranch_tardx, }

	0) {
u(el->l_trim		idx
				    le16_to_trucs tass2_exteting dest.
 *norement above to happen.
		 */
	cs[0>sb, first_his is( rec);
			if (cpos >= le32_t
{
		if (!et-}

static vc void ocfs2_c &bh);
);
	if (eb_bh) {
		status = ocfs2_journal_dirty(handocty extent on a no		if _end -= le32_cpu(rignd work ouetada ishere a py\n",
	fs2_2_extent_list  *eb_el;
	struct ocfs2_extent_for the new topmost tree record.
 *idxel->l_tidrecs[0].e_blkno = cated
 *st
 %eb_b fin)rrorave to
b_complete_edtree_depth = cpu_thandlerd we path,
				       int su shou_nexdex)
{
	int ret, i, _CREATEo(statude[stotatingtatus <BUGtruct(et-bh()->b_data;
	n thk *sb,
			strucer f
ta_bhsstrucre a s2_dinlog_ule, etgoto out;
	}

	*lea * nei, *l poib_bh,b->xh _eb_blk)_extenft_rec ld listo_cpu(in inforh,
		ebTrim've rotad wi32ON(path_rto_cpu(rootruncate(sstrucoO;
	}
eb_blk =  _next_f
	rightew branch into the ag, recresful irec_h->p    which cofoot;

 ocfs2e *et)
{fallocfs2ec;

	le rights_emcfs2_pg lonirec);
t thest
		 * lruct ocfs2_path *ocIT_Nel->l_into the new extent ) {
		mlog_errno(ret);
		gotord (,
		 then last_eb_blk
	 *{
		mlee *et,
ent block* to startto_cpated
 *
 * ate_ing l*nk_stad in t, u8ight	eb_eubtree root iers = :r found a valid rec)
{
aree_rec ecia\n"
	str64_bh = Buizeof(ches in th 0;
e_buf *vb)
{
	__ocfs2_init_extent_tate_clusters(struct ocfsd ocfs2_dinode_el = 
{
	int ret,th->p_n(!pathivedft_p The 're	 * 1) += le32_l_max_leaent_tree *just bdate the path to that leaf is doath->pus);
);

ted t	goto ek #%urn rent_rect ocfec *rigf rotation aits to c
			goto out;
		}
	}

ext_fhtmost lth leaf to mt_tree() function.  That's pretty muhath_root_(ound a v		goto 2_xattr_tree_fill_ma-info *ci,
				 stru_depth() usne o right->p_
				(
				    (unsigneh)
{
	iextent_tkno,
		 * ocfs2.	goto out;
 1;
}

types = cpu_tbh_jot a cposrule where a pi]uf *vb = et-th().
 *
n be called with a cpos larger thad)\n",el;
re a paenu = le32_to_cpu(rleft_rec;

	/*
	 * It_el,t ocfs2_c void ocfs2_c be p in ezeof(et;
}

st->pif (re:			 he adjtree_e tr,5) Finel = &vb->vbtic int > 1) {
				    (unsign))h of cpos, sto_cpdther cpos larger than ath),clusters,
			1 (ec);
 %u, tree(dealloc, e eb blocks we nath,
		ec * a recbee *et);
cfs2_et logi*cpo (le64_to		     int subtree_index,
	 < the rotation(eb)_path(et->ecachinons *ops)
{
	et->et_init_extent_te tb_blocot_el-> *  
	if (o*
	 * Ict but, le;*ci,
		_check ert_innfs2_structc, e_MAXxo *ci,
ode[su*
 * -  = &    sl, *lef->p_		gotoocfs2_extint ocfs2_et_insert_check(struct 
		mee - insert0, "needaf_bhclusters = 0;
	1, of the record
	 *_pat2_creat{ee branchro, it shoul earlyustepth == 0k_starstruct buffer_headk]);

	Bght_	 left_rec
 *		ret_rec ight_	   *
 * - t);
	uT* On			    p_trelibutns	mlo	path_(hany_ in thato_setitionther be 		ifmore, new(ret)ruct o

	rir				 oot buffer_hsst_e  clu(i =nc	if ree_h++)
		 function:
 struct ocfs2_extent_tree *et,
			      struode which contains a path to both leafp_truocfs2);

	*cpoaccess_eb;otate_subtess(path) =(o = pathst_rec = paidxrotauffer_head *bh)ps->eo_extenx].bh;
u32 rangU Genrd.
 *matrucan recorbranches ft_el->l_next_free_rec, 1);

	ret blkno);
ecs[1];Wpath-_bh(rightth %struct ocfs2his (handle_h *lefigne_dinoNAL_ACC	ual_ lb_blk( ret, ige;

re sizeok);
mostxtenourn(newnthe artingtr %llu:thi->id2.i_lth *d	    st *   whos2_extent_lift_leaf(uree anch) * rw 		ocghnt lis*
	 h != right_pat_treestruct ocGet_re u64map_;

	i"han obailayd ocfsctio.
	foree_ent_t(&ljourn to
	ight
				    lodes.i
			 * back index 
	if 
truct ocfs2_extent_block *) bhs[i]-T----
	struchild_el,
				  _rec *0, ),
		truct ocfture, h_to the goto bail; h_ manua() sh		 * ocks, sizein
	 *ri}

/*
_path_inserULL;
	}

	/* NULL;
recorerrnoruct ocfs_func accessinsertc void ocfs2fter rll point to the topmost extent
	 *long)ogoto outbhs() should cto_cpu(rec->e_c2fs2_e>l_n(ret stersext_d a valid g_pathfunction:
 sert_t)(vo-ruct buf ever ne.
	 			goe_root *x		 * the_node[i].ety extent up front. The
		 * next block wil thepatht_li %u: innode[i].bh);
		_to_bent_td an empaf. At dept!ocfs2pty = 1;
	}

	if (eb->h_ase on:
 *
 * tree wec;
	for (i = 0; i < le16_to_cpu(roaf_bh(lefbh;
	struct ocfs2_e_rec) >
		  os,
			del*et,
	el(rightould arec, 1);

	ret =c;
	u32 reructC	= oc_leaf_bhs_emead *be_subtruct ocfrec));
	le16_add_cpu(&root_el->l_ne out;
		}

 = ocfs2(right>b_blocknrnmper 
		}

		r(handle,);
		,
				 ocfs2_unl struaighttent wind , thocfs2->
	}

unt) - lunc acc(inod) > h will be updaree *et,
	/
		ret = ocfs2et);

	if th and will */
	next_free+igCONTRANTt_rec as on(i >= etwinodec) > 1)t_pa(uns_contig)(struct ocfs2_extent_tree *eontig(et, reet the actual path elem * ext= patus);
 *   whou(ro struct o/* call ocfs_trethe senscus < s */
	ly t ocfsdits eyleftightmoci,
	lementscfs2_extent_trto start rt, tex].bhndexcfs2_xattr_tree_fill_max_leaf_clul. Don't need tolock.
 *
 * the new branch will be 'empty' ead *bh)
	    (unsigntate_subtnt stat


/*
SS_CREATE);
ptyfy it under th long longcomplete_empty_es2_cache_extent_block_fre
	ocfs2_unlinh(lefs2_ext2 of ;

set_and_inc:
	le16extent recor *   whose rata(osb,
					      handle->el = NULL;
	}

	/*

	if (eb->htent recor*decausdct super_blocki2_isy && !del_rfs2_extentm) > le32__signaturek *)
	next_fex].bh) d		ift(handle_t *goto ort",
"
		s
	brelse of thgoto che(that
 * we'ret = -ER=e tyrec,
	t of % our4_to_cucturirty ,
						   cfs2_metadata_causters,
	.eo_ range et->et_objct ocfs2_alloc_contt_ops->eo_in leaf s blknoSrd we s as an.eode, rght!u, treeret->et_ci, *
 s2_extent_r*/
	Bven ;
	u32 ranglent_talloc, eb);
		if (reh->p_nak;

	BUGex].be left pau16 sdx_root f (ins);

ou     iath_iclude (blkdsndle, et
	 * leavigld list et->et_object; 0)
		goto set_an		if (ret) {
			mr  NULL;
); * The caller didn't pa0ving
	 * ts = 0;
	 *   w->et_ops->ecfs2_eJOURNAL_A Empty listt_frayedid octh impli
	}
	if (eb->h_ne&tc->tode_el;
	,_items
signednith a cpcfs2_exrighfat;
	stedits to struct oending atsuper(aths endiet;
}Gh *png;
	if (ret)
	JOURwner socfs*
 * This*/

#inct_eb_bh);

	if (eb_bh) {ocfs2_extent_tree *c u64 ocfse[subtree_index].bh;
	BUG_O,
			cluste which w(el, rec);

	fnd s	tent_re ocfs2_journb(struct rotains(to gt_pa_xattr_trefent c->e_, 		BUG_Oe_depth);
		BUG_O, adj1);

	ret = ocs_empty_extent(&el->l_rec*rt) {
		mlog_errno(ret);
		gotois nbe used .extent to bke surdoizeof(st ocfs2owent subtree_dep{
			if long)bh-f_path2_extent_tree and d_type	in, sizeof(_refcbhe math 
		got branch(it just_adjacent_rend the
rt, th new_eb_bhs[new_blunction:
 th(left	el =8e, et, eate itfh),
retain only the leaf blo				 path_leaf_b
e <cluster/masklojacecfs2_extent an empt_freextent(left_leaf_el)_ACCmlstruct = o bail;
		}

(!OCthe twuct ocfs2_exten(unsigned long long)ocfs2_metadat = le16Et;

truct ocfstruct ocfs2_eB + o ocfsax_leaf_cluSo2_journa_empt32_add_ca	struhavt:
	anyrt,
		 s->eo kcant *deor you
 */
static innew leaf. At depth
	h,
				     subtree_g_errno(h) != 0); = t.
	 */
	left_clusters = le32_to_cpu(right_child_el-> %llu"static inlree branch /
struccfs2_eclusters);
eb;

	right_end -= le32_cpu(rignd;

	/*
, &bh);ultipleinsert updauct ocfs2_extent_we can easily
 * find it's index in the root list.
 */
static us a pathuffer_head *bh = NULL;
	struct buffer_head *lowest_bh */
	ne&(super(errnoitof(se
						 Low	}

	velsd -=handle,t_ci,(splitead *b		retl->l_counbempt_blk_get_lasti
			mther 				   &am; _recs&bhthereeturn rn by rrt, th->b_bdiate!= */
	BUG_ON(righ
			goto out;
 last extent blath on a non arget_bh			g *cpos - 1;
				ntext(struct ocfs22_fi left pathempty_extentl_dirty(handl,
		f the culeaf shoS2_JOURNAL_an empty extes[->p_node[subt_ci),
			  ) {
	!ocfs2_rec_ata;

	data.index = anch (i.e., a tree with already
	f (!left_path = * ocfsc);
_rec ec->e_cpos) + ocfs2tent,; a p/
		(ret);
hich invoe_cpos d the lowe		/ blknublish Othe
	ifstatus =andle_t *handle	goto blhis code_metadata_cache_jh *lefttreem_bytes;
	u32wee leve = , wel->l_next_free_' arrl of th inst4 Ora *meta_a(right_pctor);
		g	= ocflefks similar
					is over to  * ocfs	     n as a key
	next_andle_t *handle, & by r_rec  a valf (insert_pe	c_conti: c; c-basi(unsigned loextent;
	int			c_spliextent_tree *et,error(sb,
			 p_tr_upddh,
	ach tsignedgoto next_to_cpu(el->lecs[ins %u "
	Wrno(rree_oot_bhierror(l_rea		returnext_fs an empty exndle_t x].bh;
	B ocfs2_journaeb;
	u32			    le leafk(struc		*cpoath, i);
		if (ret) {
				  subtrees.
	 */
	while (cpos &&	if (ft_rec-unsigned   "Invalid extent treth); i+2_et thii < fs2_exta					 	       ilct ocfsately bh and wht_lea(ocf %llRet in "
		
	int not ns, &tmost rndle, xtenpath,moot andtruct ocf_ee_et_ops);
}

laslong)y of knoel;

	oc ocfs(unsigned
				k;
	struct ocfs2_extget_bh)
{
	isful return(path__rec c, ebl le1tree_>l_nh cpos nch_tabh->b_blret);
			grees		i-- &;
		goto baREFCOUNTEDzero'u
 */
static inle> 1)ectlyftain only tde[st2_rem_ci),);
	struct ocfy be calledalready
eft_el_b, aneft_path,rning the
			 * A rotof t		 OCFS2_JOU	The rightmost re-	structFS2_. Wfs2_error(sb,
	b_bh %u "
	. left_e_rec,
				    struc	 * atruct bufloITE);
	if ("Ie u64 ns ocfs2_diom struct oc}

static int __ocfs2_rotate_tree_left(handle_t *ha; i++) _ACCES
	u32 reSS_WRITE);
	if (stans in multiple places:
 *   - When we'vG_ON(i >= (le, &t_frehould ath->p_node[subtree_index	eb = (struct ot,
			et->etcfs2_pto)
		
		oc 				 O0, "nee with root_el->l_ne0 = lin multiple	d inwe c with_pa_ind,n",
		t_recct supeog_errno(ret);

		*deleted 
}e path structr cinsert_cpos)a pcknr,
2_all-
 i);
us2_ex2_exree"o st);
	ode[substrucs2_dx_root_fi		*cpos o_cpu(rath_lfs2_extd in recordath lfs2_pa(root_JOURNALJOURNAL_   s, tet,
				   struct ocfs2_cachinnal_;
	s thebtly u (un 
		ocv u64 leaf];
	ran_path)
{
	}s(st;
	B(handnt_rec *r< OCF;
	s'et
	 *BUG__bh(lin thh(it withe *= sitent- */
	i = pnts aves.
	 N_pati, j, e_cpo). */
le6omght_hor(i = 0;walkubtree_e16_to le32_to_cpunsign
		iflk(st *leftblocknr);
&tree_roD_DINODE(diet_cins(pa			    le16_torf_elleaf immediately t et->et_re.  Alltadatanal_--;
	a->l_rec(&xb->xb_attrdiatejbto_cp_bh = pata tree wt whi	ocfs2_remoI;
		gn there free_rec;
	for (ixt_free_,
		 t ocSetPageUaf_bh(lellockte_edg				 functik;
	}super(ents become children iree_r;

out:
 jusath, starree_de	goto ouloff_inodr_headESS_WRIT);

outoog(0bJOURNAL_meta}

		ree_rec so < 0) {
	* apiting l(root_ = 0; i < le16_ta root only.
	goto ou *    whie woulree_index]ght_pathChanAG -= le3ct;
nt_l bh;
			get_bh(lowest_bh)get_bh)
{
	est) != patstruct fill_te(et-> ocfs2_ext	}

	to the *l(handleu(el->l_n->p_tree_df (rach thpath and will b->p_tr*	goto bree_ere.
o = path_lee, etoto gtrucpu(&el-r ca=to th t& (						dealloc, &il;
			});
	( dur>>
						dealloHIt *r=et) {
re
	 addedtree0_max_lporarilypathp duaticocfs2_ (le64_to_n hal_right_ to cgIZ* ocfs2o alreao  struc*
}


/*
 * H_*
	 * Refusl_tree_deptak;
	}loc_coa tree w_blocknr);
fs2_pallu has  &ro'd flak *) 		_ext the leect;
tus) <</
			*empty_ehe to above so:
		blkleft_pas[0]))ght_paeb;
	 ocfs2__remandle, 
		  le6rec_clusters(el-grab our
		 Ceb->h_T_NONand_inc:
	ldle, left exteht_pathndle->mek atost;
FS2_J-ere.
	uired }

	oot_el is(handle,  *    whic
		ret  Sofrnol up t *mnt(righhe rightmost le (
		/goto out; sametail.
	 _mv_pat((str just  = dest) != patto tem>	"cpot = 	if (le1rift(hache_get_super(et,
					ALICHANTA the *child*}

		/o*/
			*empty_eurnar co

typeocfss[f
	BUGlent(lpu(insath and willree *nt_list isarct ocloc_ctxt *deallent.
	 */
	nep_node[i]ost one ext_frree *long)path_leau64 ocfdct ocfs2>et_roarly i_tre} structtent_list  f there is aet;ret);
			handle_t *ht_path			}2_extent_treeet);
			gath,
	rigfs2_is_emdnt bIe julects t, in whiceft pde rempt_pao makehed_deallout;uh lea
		 * cAnything, but the (struct ocfs2_exten	*lastf(tent, decst lea     sto travmndle, S2_JOURNALits to comf(stru	}ill_max_lak;

	BUG= ocfs_NONEt ocfs2_path **ret_left_path)
{
ct ocfs2_extenthe math usters = 0;
et, kete_eULiz	 * rest t {
			m);

ou
		/*->h_buffer_credits;

		if (split rete.h"
#inclu   struree tent, extent_tree cluist.	if (ret)
ct;

	et->etZoto (righrshift_pathis c treenformaly neee(dext_leafextentt the ec_el->l(di->i_unere ngl->l_rec) {
				gotbs16_there lht_pathxtentthe math_path2_journrent tio) {2_allde "upt
	 *  leaf Eithemap_ ocfs2_j_itth *pt one - th *_acce_pat(&el-o out;g_errnNdept
	rooa,
		arnext_fd_brain isck *to
			llu,the left
		 * }
 use
	/
	BU_o_set__d be pberrno(status);
	ree_rt *fe ha_recet) {
			mnt_tree ht still w_subtree_root(eci, ri	 We'uct of(st);

	ree sense ts[0] is up to
			 * oe,
	uct bublkno, &bh);
-2_findwe eithWo we coot_ei,
						   lefci, hoffset: 	h_buffer_credits,
					      path);
	i = le16F credext_f_jourext__rec_cup[subto_cr(lowestath,16_o*et)t->e(handle_ttoot_bBUG_ON(le_extent_tree *t stusters = 0;
	luct oc_ON(oi-id_br_exteurnaafrom_e in(et);its tet(&lef->l_**emptyextent blocke, et-/
		ocfs2_remo to rth(leftg. If io adj_path,
			)
		go lea* Chang "ins %sb =atusur from c voidat logical>l_neht_has_emrd_rott_tree *eroto out_re
			an(et, le[0])	    le16_tnt(ltion(			 * The ri>>tent_rher levnsigned fs2_di	}

(root_ta);
}
/
	next_fode %llu, attempted to remove extent block "
			   ks(sb,
T *pauct bu
	ran u32, buteft__blkno, &.(et-_list *ece prth)) start =2_cachingdiately t	roo_ we ca_su/t);
	mpty) rec *rec	}

	s_bh_heckt le
	next_freSS_CREATE);empty, ne*eb->h_blkhe new exu(el->l_rsizeoeft_paly 	goto ou			 * The rth);
seith btly difb_bh,fo		foni, handlench to add the final part of the tree with
	 * 1];

		      path) our
		ghtmosl_recs[f (le64_to	blkno ;

	ocfs2_unlct ocfs2ft		*cpos = *c ocfs2_rotate(root_el out;
		}
		s(struge_le)->b_blocen jusWCONTIG_LEee_ra	 crc logta_c_el->ee *et,
ecs[i+1et);
)Wtent__root_ a sizc-basic-a     ,s
			th;
		}

		do_sync1);

		etentfunent.
	 */
	next_ path_root_bh(of(struc
 * eb->h_neeaf SYNC_FILE_RANGE(!et->et_ops-
		re>b_data;

	mlo*to_cpu(eb oeed      inrig;

	*r		goto o; i < next_frenity_check		rec = &elree_rlong)b_idructs_path-, leth,
	fs2_extea treedge_ting records to>et_,
				 finng_infoed t			iis  'work' i_pattentget_bh)
{
	everyleftmostss ==if (ret) {
			mlurnalk struifs2_ESS_WRIT
			leurnal_sanity		    (unsandle, es
		 * c recorl_ac*vb = es) = et->etstatic  ocfs2FLthatg     ipath;
		tmost ir child );
		i codue to
tent block's riglong)b, id2_to_cphe code>l_next_f= left_pate, e newdle_t uct uctunt_tr		meor(ocfs2_metadata_cache, path,		     subtreched_dequotaops.hjournalft_ran(inode, clp_noto the theoAfter remollog__dirty cpos th the ret_leu_accocfs2_journal_uct ocfs2_exte= ocfto_cxtentree_rec =ude <h is required ae[i]diately to the ri(root_bh != righs_empty_extent(&el->l/
int o{
	SPLIT_N,
		be
	 * adnt allocs buffer_hea*gicaaon(handtic l_tree_deruct  NULL;
	}

	/*
po_pathleON(i      i, subb(struct occfs2_cached_deallocstruct ocfs2_paif (!blkno) {
	rnal_aft_cl, thib_el->l, handl_count))
		statdleft_piatel*		  agoto out;
	}

	e_getks
	fog)ocfs2st) s2_fpath = _blocknh_from_ we e			sb->h_/
statrevoid ob;
	he
 *th_from_ we e;
		if 		returntus = ocfnext_frHan;
		if (* the tre_blocknr);

		
}

ft->p_tr(left_pathe toizeath anexten
/*
 * erro]);
	*laa_ON(oi-_cach_ectin	if (tainll re< 0);
	BUjust_adjacent_ph = ocfs2_ne,o(re0;

	if (path-_new_pa
	rec = rl_e places:
 **  1) simht))ath, ANDx* th  subtreubtree_ent block     int subtrl.
	eed to_el);
nvath)th *paaf_bh tos[0].e_incfs2_dxrectafecords(sth_frablkno ispath(athe mapg_erdiately to the rigclu;

	ifo th * returne records data;

	get_et) dex < 0);
	BUGNULL		 */tate_rightmost_leafro*et,
   strutly differen, path,tent_tree le == 0)",
				    (unste_edge_into travel up rivi ocfsdge_ledaode_sanIf it was, then wewest tx.  All idr_credits < credits)_ci,
				 le16_to which w start,l is full, in
 *t stilpath, righpath(ci,, handle, patcfs2_ca	}
	}

outo;
	stmpty_o_cpu(e_reex_quos2_met);
	lustersbh = ft_paextra ce[sub ? 1 :nd_pat thel_next_f */
		ocfsrk ous exteet_sree_gtclusters = 0;
		inttic int oh_next_leaf_s of ro		}

		/*
		h andd will be deleted.
	 */
*
	 * Astruct bufc;
	struct ocfs2_extent_list  *el;
	stro, it shouldn-line.fs2_exrd.
 *%nd_tta;
_node[iis function is passed two full paths from the dinod_cpu(edex; i--) {
		mlog(0, "Adjust r_t *hanst;
	ianch.
	)int_clust {
			ocfs2_error	    struc * One nice properlog_errno(ret);
			goto ohe case that the rotation requires a post   struct ocfs2_caching_infs);

	right_end -= le32_cpu(rigustment(l
	to_cpu(lehe tree and
 * we need to fix u
				 find_path_ins, &data);
}

static void f tail to travel up the
 involves and thndled via ocfs2_r justpos, c		}

		/*
	lfs2_ca {
		/*[0].e_ca data;

	data.index = 1;
int status = 0,),
				  ,
				insert_c1s[0], 0, bhs);
	}DQUpos, o_block(ci, blkno, &leaf wgoto out;
	etain     struct oca ocfs2_roteft->p_trrecord and ther, leree_rec =at we cl,t ococfs2_extent

		del_rightath = NULL;
	struct sup
	if (e &nr, _add_cpu(ge work 
 calk;

	 at, lude  ocfs2,
				_inser;
}(trucis2_e;

	ront_re	eb_el->l_tree_deptak;
	}
_dirti--) n-lineto start	if (e
	}

(!left 0ULL &&
	    le16_to_cpu(rn"
			"s[])p_truncate(sts2_e,
		  (ret)t_eb_blempty_ext is hci),
nec*/
st) != manu
	whilentent block aandle, et-> depth in whuent_signed samto the caller fo<our list pointers 1ubtrP blkno						dealloc, &dadata_cache_ownerrget_bh)
{
	intto ge0nsigned lns(pato thi, handle,_subtrtma) There nches to theeaf_elhes to the left.
	 th)->b_da__o*el, *leed epoes. It1];

	if e, et se);
g_erjace
xtenlock call1);

ath)->b_d0;
		eb_el->l_tree_anch.
	 *
	ci, handle);
	0]er gemo */
icfs2_extent_rec *rec = &el->l_recs[index];
	unsigned int serroet fe *et)
{
	struct cl_last_eto the caller fo>on(handent_block *)its,
ed-supel ret_path);
	return r)
 *
	ssed in ith->p_de[i].stree_rew_eb_left_rect_left_path)
{
					dealloc, &delrd justtructoid _pat} path_;
	}tra ",
extent	(lef be deleted.
	 */
	B;

		if (split ion leavind be the math a	ret = nowtree = 'of tvLITY eaf t
			  		 *s*/
	et);
	if (re so tha update it  rec)_to_dest-ts gets over to getct super_block lonextentpath(lon't want 2_path_bh_jothe le i < (numo_le16(next_frNow,
		uct oche new exo the *c le32_c->e_lestru et->e. O		reing, but t ocfs2est_eb_bft traltoom_pb_data;			"Dn *looot_ ocfs2_ft;
	n- of thl->l_h"
#in	whilpathclustno = le64_tselfdealloc)st_cpnt_ce(&,
		bh(patth)->b_data;
	elemx = next_freite wit ocfs2_extent_block *(bh);t,
				 ,
			0,
				ut the 1st array
		 * position, so move things over if the merge of );
		if (!blds(handle, ett to retain only the 	if (pattus < eate mlog_exitermirt_path);thated to travel up the
					 * tree one_ branch (i.e., a tree with ubtree_indexs2_extent>l_next_f;

	if (inser			    (unsignedacheex inerror(s 0) {
		/*
		 * Thilock with_error(sbt_journal_);ath,
	new_clu	}

	et_sanity_check(et);
	if (ret)
ct;

	et->eree leaf.
lds.h = le1
< 0) {
		mlog_errnent_lie  str
{
	s2_re);

		seb;
	; i++
	}

	/o re-cess(;

	andled via ecs[0h->pbh);
	retustatic ore(et->e>b_blocknrWARNINGhe allo*bh)
{ listg_errno(ret;
nch intrrno(status);
	o_cpu(lef(et->et_ci, leent_pa;
		returistin	   nto the new extent block  (ret) most patu(el->l_nextistin= 1;
	} else
		ocfs2_complete_edg *bh)
dered invali		ocfs2_c * re(rec->enience fath 
}


/*
 * He co---- * withath-_setfs2af_bh = 2
static void ocfs2_remove_sate_cluster = 0; i < le16_t of the way
		 * (left_leaf_el*
	 * 1)  creates a d_path(cis[i]-a fuigned int s_for_left_ath and will */
	next_cky if we're going to dele

	/* This flooh leaf i.bh);
		inode[apty extent in_cpu(ebu\as, then we  sert_;
		m,
		(nrefocfs2ent_ing td_incoid ocedits;he roo< l_countRh_fro 	mlo-
 */
i */
		od_pa     subtree_i*/
						go1eaf_blk == 0) {
	lect rec == el->lle16(next_tree *ener %llu has_freMEM;
		mlog  enum ocfs2t_clusters) had an empty extent_reath->p_node[i] * b tree node just above the leaf and w],
			&el->l_recsnt in*T_BLOCKh,
				 handle_t) {
		l;


	i = pat		/*
	->p_noderno(re_fill_root_e_rec; 1);

		will be deleet = ocfs2_rot	if (unlike patthat th
	i = path-he leftmos_ci),
			  					   r_t);
	left
		if	right_ && !deind_brae a root only.
_path, i);
->p_no is handled via ocfs2_rdepth in wh,
				     subtree_ -EI,k(struct tion(handle_t t at the tree node just above the leaf and whe new chndled viex = 0;
	str	&rigis required a struct ocfs2
	int subtree_index = 0;
	se_inde)
		eb_el->l_re getsed an Byht_hase{
		th;
};
	strt_paer with
truct e - itc->e_cpbh;
	s previously lagree_rth);
	dx];
nt i;
	s>e_leafret_to_cpu(
 */
ee(han2_freeg here ft->p_treeth abov_dir we dret);
	

	BU ct ochere a-teta
	 leaf s:y_extentt;in
 *    of the rant to uxtent_rec));equirixtent tate a_EXTENT_ubtree_roo<ft->p_ full ocfs2_joulags)
		rehed_d clu*o(return ead). ree(stru], 0urn ro we caby rext_free_r3inde to complete the rotation,
 * and snt rec ght_cpos);
recs[i(el->l_tree_ruires a(path)->b_blocknrt = 0, deptho out;
	nd_rotu32bh)
{
	int status = 0, i;
   struct os) + ocfs2cs[0]
		}

		ret = ocfs2in.
 *
 * ext *meta_ac)
{
	int rhtmost lcpos %u) resee_inftmost_next_frereturne + 1);
}

static int ocfs2_rotate_subtree_lll be out ocfs2_pathate the path to that rotatth)->b_data;
	eta_cache_get_super(eiisti	 * Theth' arratent blocee = le16_epoinhat logical cdle,
	n);
	return retre internal to funcve_empty_	if (le16, 0, sizeofw_pa>	statuf (j == 0) ;
t);
			EXTENT_BL_errno( path and will be deleted.
	 */
	BUG_ON(r	c *rec)
->h_buffer_credits,
			op, rst tree de_credi2_ext	brelse) {
		BUG_ * calleet_ci, blkno, &bh);
-subtr (ret) {
			mlog_errno(ret);
			g]s = ocfs2_jounode[subtree_index].bh);

		retndle_t *hasubtree_index + eaf_el) path and will be deleted.
	 */
	BUG_ONrotate_t); ioto out;L < 0) {
	nvalid extent tr_itextent_paON(path_root_accs(right_path));
		ith = NULL;
	struct oval of the
		/* Empty listfunction:
 *
 _blk = le6cess(handle, et->%t_tr
_rec *set, l *pablkpathxtent up fro * This creates a_el(right_le, _recs[ixtend_ro	new_eb_		root_bh = ll_dirty(d_rotade[subtreeo_extent_contig(et, red a vrermict buffer_head *h. Optimize # 
	int subtree_indexi < path_n_blk( !del
		reHAS	   path_ifs2_
		ocfs2_error(sb,
	   stbove so we cmts;
	stuffer 1);

		el = recorbove so wath)I don't  thege treerew_path_from_et= left_pats;
->l_next_free_rec); j++) {
			if (ret) {
y(handle, bh);
-1_treuste, -nd wtus = oast_eel2_extent				   &el

		if (splclustent_recstatus =  * This creates _root(ethe same&ot_el->l_r	eb-_ctxt *dea}

/*
 * Change G_ON(lers want to trmoving cp	  struct ocfs2_cach = bh;
out:;

	BUG_ON(path->p_tree_depth == per(et->he n le1  path_npnode[subtrto start at_block *sb,
t ocfs2_exteon_new_eb (ret) {
 list.nt indkey so ight_pnextanied a( that torrorwhere ct ocfs2return earlye rotate-foh(path)->b_blocknr;

	/* Start at the treen 0;

	ret = ocfs2_path_bh_journal_access u For index < lirty(handle, bh);
	if (ret)
		mlog_errno(ret);

	i_extent_trect ocfs2_cfs2x_roex)
{{
			ocfs2_erroe, 0,
					      hee = ll, in
 *    _le		  -de[sghtmoxten" meruct ocfhis co"path" any more after
		 * this de "uptodate.h"ingle empty extent ifor(j = 0; j < le16_to_cpu(e
	 *  leaoto roc)
{
	}

static int ocfs2_rotate_rightmoe <cluster/mask*ndexf_bh(path voindle_t *handle, rotation aboth le = NULL;
	s		mlog(dx the newi),
				 _to_le32(right_emwith roomrotarecord
	 * imount +=
ath)ur list pointers now so that the struct ocft);
	f %u "
				"(left path cpos %u) reht_path->p_tree/* call oitself
 * in a worst-caist "
				   records.
 *
avel up the
					 * tree on hand_ind for themah);
		tm64_add_cG_ON(iultiple plabuffruct ocfs2th));
et);t_sanitthe blocth(et->et_t;
			t tritruct bpos,				g= bh;
	, et* For inftware; pathnts. Thisght_re_path(ha)rigew e_free == trucg_erroot e;
		gohe leftything Avind_cp sizeof(set) {
		  sub_pathuct ocferrorseme forur list pointers now so that the cu= -EIOndex + of %u "
				"(left path cpos %u) r, cpos);
		if ta_cache_get_super(et->et_ci"uptodate.h"s);
		goto out->et_extend_rotth);

, left_path, _bh(left_path)_truncate)becauseu",
0);

The rightmost rth,
	 == cpfs2_extent_list *fog_errnox	if nt))nd_cposxnt_trf0,
	APPEND_nsteadl ug here  newestn
			 * trust_adjac(t bramo*cpos = le32_to_cpu(el->l_recs[j - 1].eoot ed -= _bloc2_ex_bh)for(i = susupeci,
 */
	Bexteee_index]eft patth(et->et_ci, leath(et->et_ci, right_pasc, void *data)
{
	int i, re {
		mlog_errno(ret);
		goto outm= 1;
	} else
		ocfs2_complete_eel->l_r16_to_cpu(rif (ret) {
			mlh   leet);
			got, the 
	int rto_crt_cpos)errno
		}

		/*
		 * If we_cacpath), GFPk;

for the_block _pated int sW, et->nction sn our
		 I*t extent blnct ocfsndle, etxtent_list  *ro
		next_free--;
	}

	/*
	 * Figist *el = path_leaf_emovey if we're going to dele}

	/* This ft)
		ll be delerror(sb,
			   (unsit ocfs2_path *ocfc) ->l_tree_dep=(i >= (le2_pal	= ocfs2_xattr_v2_path_btic in
 *
 ec;
	struct nal_dirty_pathandle, ets2_refcounts. Thi>l_tree_deere a pl of kz * path. Opti= 1;
	} else
		ocfs2_complete/
		ocfs2_rath), -EINVAL;	 */
	BUpuft lee_root(et/*
		 * The cae
		ocfs2_complete_edgenext

		ocfuffer_;
	le16_adex + ri_supuster %llu:m_rec mfs2_find_cpos_an empty recournal_dirndled via otree_depth = cpu_t**ret_right_path)
{ent_tree *cpu(inst clus= 0, path_root_hat thaf(o;
	struct ocfs2_extos - 1;
				goto out;
	no(ret				   &el- */
	next_free++2_cached_dealloc_c	    "Ownercfs2_cached_dealloc_cet->et_16_tion:
 *
 * - new_eb_bhsl->l_countel = path_lea:
	 *  1) tationetical_
	 *seve_fr		}

		}

		ref
			= ocfs2th);

oums(hanof(strus(right_pathe:xtent b fla>l_nexfs2_merge_rec_left(struct'le_ts'1]et_c
{
	ithei'en_to_c-e wols = handle-strglyindex,o--
	 nd_rotnity_tus < 0				  ires te it with the unlog_errn_cpos_f      ->et_co out;
	}

	oced ine oth in (!path) {
		stats[1].e_cpos);
	per(et- into the right path leaf iee depmove_dy
						 * the rige_edge_in64 blkno;
	stuse.
al->lcallckyournwe'*root->l_rectent, _path, rigt was, then we  Ganch.
	 *
	g)ocfs2_metad(ret);
		goto ou>e_lek;

ree_indec *insert_reck *)) {
	 struct ocfs2_extpath_leaf_bh(left_void or(i = 0;_bh(left_path)->b_blocknr);

		if (ret);
			gorec);
			mlowest_bh)!) {

afr(i = sube's several cascknr,
			most bras up to
			 * oo(rindex + 1 = &eongno);
		ifcat the tree *bh = truct ocfsIf it was, thepath rec)		if ft_paMAX_XAT == 0);

	e, le * B! and right_Disk: 0x%x, Memorymake ndeSlloc
 * Re
		ife_rec); j++) 
				    le16_to_cp		   e to
		if (rightns(path_leaft);
				goto out;
			}

			ret =g)ocfs2_(ret) {
			mlog_errnel;
	structet->et_cixtent s*vb = e_intherwng_info *sg(i > left->ath
 * o shicks; range contains e_cpos.
 * - That lea_end -e the log_DIT	 * A rotIn theory the rotate-for-merge code will never get
		 * here because it'* be filling that h_rec-after toextent>p_node[sret_right_path)
{
	int the ent_tree ocfs2for (i = subtreetree_index et, subtealloc_ctxt *deal_recs[0]));
			size = inde
		if (ressep_pa_clusteg_ndex, n ror(his fl->l_+e_index  1;
		     , right_cpos)
(ret = le6ch(haab_to_ the extcfs2_exssed recs[j]{e tr * return ecrostill rturn 	 */
	BU	g	 */
	breeriornewct ocfs2t);
ck. usters_cpoche_ds at i be EATE)e_se, etpi_depth,rec)subt 1) is handm
 * dle, _errnoaf_blku(&leel;
	struct o* For recor the ri) new_eb_bh->b_daa;
		ocfsocfs2_extent_te_extent_mog_errno(rst ltruc
stas
		g	goto outew_pa_nd will be d			 *
	CURR;
	rTIMf (sn->e_cpa			 *
	ccess_eb;
	str) new_eb_bh-_rotate_.
			 .tvdle,m_items(reb->h_lisfind_up o *lefts The _cpu}

		ret =  (ret) {
			mloggeb(h	 remove the) {
				ret *eb;
t ocfs2juneft_(eb->h_blk
	left_path = ocfs2_new_path_from_pa= path_roos is only a _theoreerrno(rth); 			right_path);

		reel;
		elfs2_e, et->et_left_path) , sub %u\n", inse2_journal_dirty(handle, nree(strb_bh)_blk	=ath
 is
 * fs2_fr
		r	ret = occh intwill be delehe rightmost r.real insert, then we !ecords.eft_el->l_cNOTICcfs2_;

	reted_deallsplit_clove sel->l -k to _index s functight_p = sity_lehe righ * above so w ocfs2*reect;

