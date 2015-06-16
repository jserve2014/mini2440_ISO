/* -*- mode: c; c-basic-offset: 8; -*-
 * vim: noexpandtab sw=8 ts=8 sts=0:
 *
 * xattr.c
 *
 * Copyright (C) 2004, 2008 Oracle.  All rights reserved.
 *
 * CREDITS:
 * Lots of code in this file is copy from linux/fs/ext3/xattr.c.
 * Copyright (C) 2001-2003 Andreas Gruenbacher, <agruen@suse.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/capability.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/highmem.h>
#include <linux/pagemap.h>
#include <linux/uio.h>
#include <linux/sched.h>
#include <linux/splice.h>
#include <linux/mount.h>
#include <linux/writeback.h>
#include <linux/falloc.h>
#include <linux/sort.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/security.h>

#define MLOG_MASK_PREFIX ML_XATTR
#include <cluster/masklog.h>

#include "ocfs2.h"
#include "alloc.h"
#include "blockcheck.h"
#include "dlmglue.h"
#include "file.h"
#include "symlink.h"
#include "sysfile.h"
#include "inode.h"
#include "journal.h"
#include "ocfs2_fs.h"
#include "suballoc.h"
#include "uptodate.h"
#include "buffer_head_io.h"
#include "super.h"
#include "xattr.h"
#include "refcounttree.h"
#include "acl.h"

struct ocfs2_xattr_def_value_root {
	struct ocfs2_xattr_value_root	xv;
	struct ocfs2_extent_rec		er;
};

struct ocfs2_xattr_bucket {
	/* The inode these xattrs are associated with */
	struct inode *bu_inode;

	/* The actual buffers that make up the bucket */
	struct buffer_head *bu_bhs[OCFS2_XATTR_MAX_BLOCKS_PER_BUCKET];

	/* How many blocks make up one bucket for this filesystem */
	int bu_blocks;
};

struct ocfs2_xattr_set_ctxt {
	handle_t *handle;
	struct ocfs2_alloc_context *meta_ac;
	struct ocfs2_alloc_context *data_ac;
	struct ocfs2_cached_dealloc_ctxt dealloc;
};

#define OCFS2_XATTR_ROOT_SIZE	(sizeof(struct ocfs2_xattr_def_value_root))
#define OCFS2_XATTR_INLINE_SIZE	80
#define OCFS2_XATTR_HEADER_GAP	4
#define OCFS2_XATTR_FREE_IN_IBODY	(OCFS2_MIN_XATTR_INLINE_SIZE \
					 - sizeof(struct ocfs2_xattr_header) \
					 - OCFS2_XATTR_HEADER_GAP)
#define OCFS2_XATTR_FREE_IN_BLOCK(ptr)	((ptr)->i_sb->s_blocksize \
					 - sizeof(struct ocfs2_xattr_block) \
					 - sizeof(struct ocfs2_xattr_header) \
					 - OCFS2_XATTR_HEADER_GAP)

static struct ocfs2_xattr_def_value_root def_xv = {
	.xv.xr_list.l_count = cpu_to_le16(1),
};

struct xattr_handler *ocfs2_xattr_handlers[] = {
	&ocfs2_xattr_user_handler,
#ifdef CONFIG_OCFS2_FS_POSIX_ACL
	&ocfs2_xattr_acl_access_handler,
	&ocfs2_xattr_acl_default_handler,
#endif
	&ocfs2_xattr_trusted_handler,
	&ocfs2_xattr_security_handler,
	NULL
};

static struct xattr_handler *ocfs2_xattr_handler_map[OCFS2_XATTR_MAX] = {
	[OCFS2_XATTR_INDEX_USER]	= &ocfs2_xattr_user_handler,
#ifdef CONFIG_OCFS2_FS_POSIX_ACL
	[OCFS2_XATTR_INDEX_POSIX_ACL_ACCESS]
					= &ocfs2_xattr_acl_access_handler,
	[OCFS2_XATTR_INDEX_POSIX_ACL_DEFAULT]
					= &ocfs2_xattr_acl_default_handler,
#endif
	[OCFS2_XATTR_INDEX_TRUSTED]	= &ocfs2_xattr_trusted_handler,
	[OCFS2_XATTR_INDEX_SECURITY]	= &ocfs2_xattr_security_handler,
};

struct ocfs2_xattr_info {
	int name_index;
	const char *name;
	const void *value;
	size_t value_len;
};

struct ocfs2_xattr_search {
	struct buffer_head *inode_bh;
	/*
	 * xattr_bh point to the block buffer head which has extended attribute
	 * when extended attribute in inode, xattr_bh is equal to inode_bh.
	 */
	struct buffer_head *xattr_bh;
	struct ocfs2_xattr_header *header;
	struct ocfs2_xattr_bucket *bucket;
	void *base;
	void *end;
	struct ocfs2_xattr_entry *here;
	int not_found;
};

static int ocfs2_xattr_bucket_get_name_value(struct super_block *sb,
					     struct ocfs2_xattr_header *xh,
					     int index,
					     int *block_off,
					     int *new_offset);

static int ocfs2_xattr_block_find(struct inode *inode,
				  int name_index,
				  const char *name,
				  struct ocfs2_xattr_search *xs);
static int ocfs2_xattr_index_block_find(struct inode *inode,
					struct buffer_head *root_bh,
					int name_index,
					const char *name,
					struct ocfs2_xattr_search *xs);

static int ocfs2_xattr_tree_list_index_block(struct inode *inode,
					struct buffer_head *blk_bh,
					char *buffer,
					size_t buffer_size);

static int ocfs2_xattr_create_index_block(struct inode *inode,
					  struct ocfs2_xattr_search *xs,
					  struct ocfs2_xattr_set_ctxt *ctxt);

static int ocfs2_xattr_set_entry_index_block(struct inode *inode,
					     struct ocfs2_xattr_info *xi,
					     struct ocfs2_xattr_search *xs,
					     struct ocfs2_xattr_set_ctxt *ctxt);

typedef int (xattr_tree_rec_func)(struct inode *inode,
				  struct buffer_head *root_bh,
				  u64 blkno, u32 cpos, u32 len, void *para);
static int ocfs2_iterate_xattr_index_block(struct inode *inode,
					   struct buffer_head *root_bh,
					   xattr_tree_rec_func *rec_func,
					   void *para);
static int ocfs2_delete_xattr_in_bucket(struct inode *inode,
					struct ocfs2_xattr_bucket *bucket,
					void *para);
static int ocfs2_rm_xattr_cluster(struct inode *inode,
				  struct buffer_head *root_bh,
				  u64 blkno,
				  u32 cpos,
				  u32 len,
				  void *para);

static int ocfs2_mv_xattr_buckets(struct inode *inode, handle_t *handle,
				  u64 src_blk, u64 last_blk, u64 to_blk,
				  unsigned int start_bucket,
				  u32 *first_hash);
static int ocfs2_prepare_refcount_xattr(struct inode *inode,
					struct ocfs2_dinode *di,
					struct ocfs2_xattr_info *xi,
					struct ocfs2_xattr_search *xis,
					struct ocfs2_xattr_search *xbs,
					struct ocfs2_refcount_tree **ref_tree,
					int *meta_need,
					int *credits);
static int ocfs2_get_xattr_tree_value_root(struct super_block *sb,
					   struct ocfs2_xattr_bucket *bucket,
					   int offset,
					   struct ocfs2_xattr_value_root **xv,
					   struct buffer_head **bh);
static int ocfs2_xattr_security_set(struct inode *inode, const char *name,
				    const void *value, size_t size, int flags);

static inline u16 ocfs2_xattr_buckets_per_cluster(struct ocfs2_super *osb)
{
	return (1 << osb->s_clustersize_bits) / OCFS2_XATTR_BUCKET_SIZE;
}

static inline u16 ocfs2_blocks_per_xattr_bucket(struct super_block *sb)
{
	return OCFS2_XATTR_BUCKET_SIZE / (1 << sb->s_blocksize_bits);
}

static inline u16 ocfs2_xattr_max_xe_in_bucket(struct super_block *sb)
{
	u16 len = sb->s_blocksize -
		 offsetof(struct ocfs2_xattr_header, xh_entries);

	return len / sizeof(struct ocfs2_xattr_entry);
}

#define bucket_blkno(_b) ((_b)->bu_bhs[0]->b_blocknr)
#define bucket_block(_b, _n) ((_b)->bu_bhs[(_n)]->b_data)
#define bucket_xh(_b) ((struct ocfs2_xattr_header *)bucket_block((_b), 0))

static struct ocfs2_xattr_bucket *ocfs2_xattr_bucket_new(struct inode *inode)
{
	struct ocfs2_xattr_bucket *bucket;
	int blks = ocfs2_blocks_per_xattr_bucket(inode->i_sb);

	BUG_ON(blks > OCFS2_XATTR_MAX_BLOCKS_PER_BUCKET);

	bucket = kzalloc(sizeof(struct ocfs2_xattr_bucket), GFP_NOFS);
	if (bucket) {
		bucket->bu_inode = inode;
		bucket->bu_blocks = blks;
	}

	return bucket;
}

static void ocfs2_xattr_bucket_relse(struct ocfs2_xattr_bucket *bucket)
{
	int i;

	for (i = 0; i < bucket->bu_blocks; i++) {
		brelse(bucket->bu_bhs[i]);
		bucket->bu_bhs[i] = NULL;
	}
}

static void ocfs2_xattr_bucket_free(struct ocfs2_xattr_bucket *bucket)
{
	if (bucket) {
		ocfs2_xattr_bucket_relse(bucket);
		bucket->bu_inode = NULL;
		kfree(bucket);
	}
}

/*
 * A bucket that has never been written to disk doesn't need to be
 * read.  We just need the buffer_heads.  Don't call this for
 * buckets that are already on disk.  ocfs2_read_xattr_bucket() initializes
 * them fully.
 */
static int ocfs2_init_xattr_bucket(struct ocfs2_xattr_bucket *bucket,
				   u64 xb_blkno)
{
	int i, rc = 0;

	for (i = 0; i < bucket->bu_blocks; i++) {
		bucket->bu_bhs[i] = sb_getblk(bucket->bu_inode->i_sb,
					      xb_blkno + i);
		if (!bucket->bu_bhs[i]) {
			rc = -EIO;
			mlog_errno(rc);
			break;
		}

		if (!ocfs2_buffer_uptodate(INODE_CACHE(bucket->bu_inode),
					   bucket->bu_bhs[i]))
			ocfs2_set_new_buffer_uptodate(INODE_CACHE(bucket->bu_inode),
						      bucket->bu_bhs[i]);
	}

	if (rc)
		ocfs2_xattr_bucket_relse(bucket);
	return rc;
}

/* Read the xattr bucket at xb_blkno */
static int ocfs2_read_xattr_bucket(struct ocfs2_xattr_bucket *bucket,
				   u64 xb_blkno)
{
	int rc;

	rc = ocfs2_read_blocks(INODE_CACHE(bucket->bu_inode), xb_blkno,
			       bucket->bu_blocks, bucket->bu_bhs, 0,
			       NULL);
	if (!rc) {
		spin_lock(&OCFS2_SB(bucket->bu_inode->i_sb)->osb_xattr_lock);
		rc = ocfs2_validate_meta_ecc_bhs(bucket->bu_inode->i_sb,
						 bucket->bu_bhs,
						 bucket->bu_blocks,
						 &bucket_xh(bucket)->xh_check);
		spin_unlock(&OCFS2_SB(bucket->bu_inode->i_sb)->osb_xattr_lock);
		if (rc)
			mlog_errno(rc);
	}

	if (rc)
		ocfs2_xattr_bucket_relse(bucket);
	return rc;
}

static int ocfs2_xattr_bucket_journal_access(handle_t *handle,
					     struct ocfs2_xattr_bucket *bucket,
					     int type)
{
	int i, rc = 0;

	for (i = 0; i < bucket->bu_blocks; i++) {
		rc = ocfs2_journal_access(handle,
					  INODE_CACHE(bucket->bu_inode),
					  bucket->bu_bhs[i], type);
		if (rc) {
			mlog_errno(rc);
			break;
		}
	}

	return rc;
}

static void ocfs2_xattr_bucket_journal_dirty(handle_t *handle,
					     struct ocfs2_xattr_bucket *bucket)
{
	int i;

	spin_lock(&OCFS2_SB(bucket->bu_inode->i_sb)->osb_xattr_lock);
	ocfs2_compute_meta_ecc_bhs(bucket->bu_inode->i_sb,
				   bucket->bu_bhs, bucket->bu_blocks,
				   &bucket_xh(bucket)->xh_check);
	spin_unlock(&OCFS2_SB(bucket->bu_inode->i_sb)->osb_xattr_lock);

	for (i = 0; i < bucket->bu_blocks; i++)
		ocfs2_journal_dirty(handle, bucket->bu_bhs[i]);
}

static void ocfs2_xattr_bucket_copy_data(struct ocfs2_xattr_bucket *dest,
					 struct ocfs2_xattr_bucket *src)
{
	int i;
	int blocksize = src->bu_inode->i_sb->s_blocksize;

	BUG_ON(dest->bu_blocks != src->bu_blocks);
	BUG_ON(dest->bu_inode != src->bu_inode);

	for (i = 0; i < src->bu_blocks; i++) {
		memcpy(bucket_block(dest, i), bucket_block(src, i),
		       blocksize);
	}
}

static int ocfs2_validate_xattr_block(struct super_block *sb,
				      struct buffer_head *bh)
{
	int rc;
	struct ocfs2_xattr_block *xb =
		(struct ocfs2_xattr_block *)bh->b_data;

	mlog(0, "Validating xattr block %llu\n",
	     (unsigned long long)bh->b_blocknr);

	BUG_ON(!buffer_uptodate(bh));

	/*
	 * If the ecc fails, we return the error but otherwise
	 * leave the filesystem running.  We know any error is
	 * local to this block.
	 */
	rc = ocfs2_validate_meta_ecc(sb, bh->b_data, &xb->xb_check);
	if (rc)
		return rc;

	/*
	 * Errors after here are fatal
	 */

	if (!OCFS2_IS_VALID_XATTR_BLOCK(xb)) {
		ocfs2_error(sb,
			    "Extended attribute block #%llu has bad "
			    "signature %.*s",
			    (unsigned long long)bh->b_blocknr, 7,
			    xb->xb_signature);
		return -EINVAL;
	}

	if (le64_to_cpu(xb->xb_blkno) != bh->b_blocknr) {
		ocfs2_error(sb,
			    "Extended attribute block #%llu has an "
			    "invalid xb_blkno of %llu",
			    (unsigned long long)bh->b_blocknr,
			    (unsigned long long)le64_to_cpu(xb->xb_blkno));
		return -EINVAL;
	}

	if (le32_to_cpu(xb->xb_fs_generation) != OCFS2_SB(sb)->fs_generation) {
		ocfs2_error(sb,
			    "Extended attribute block #%llu has an invalid "
			    "xb_fs_generation of #%u",
			    (unsigned long long)bh->b_blocknr,
			    le32_to_cpu(xb->xb_fs_generation));
		return -EINVAL;
	}

	return 0;
}

static int ocfs2_read_xattr_block(struct inode *inode, u64 xb_blkno,
				  struct buffer_head **bh)
{
	int rc;
	struct buffer_head *tmp = *bh;

	rc = ocfs2_read_block(INODE_CACHE(inode), xb_blkno, &tmp,
			      ocfs2_validate_xattr_block);

	/* If ocfs2_read_block() got us a new bh, pass it up. */
	if (!rc && !*bh)
		*bh = tmp;

	return rc;
}

static inline const char *ocfs2_xattr_prefix(int name_index)
{
	struct xattr_handler *handler = NULL;

	if (name_index > 0 && name_index < OCFS2_XATTR_MAX)
		handler = ocfs2_xattr_handler_map[name_index];

	return handler ? handler->prefix : NULL;
}

static u32 ocfs2_xattr_name_hash(struct inode *inode,
				 const char *name,
				 int name_len)
{
	/* Get hash value of uuid from super block */
	u32 hash = OCFS2_SB(inode->i_sb)->uuid_hash;
	int i;

	/* hash extended attribute name */
	for (i = 0; i < name_len; i++) {
		hash = (hash << OCFS2_HASH_SHIFT) ^
		       (hash >> (8*sizeof(hash) - OCFS2_HASH_SHIFT)) ^
		       *name++;
	}

	return hash;
}

/*
 * ocfs2_xattr_hash_entry()
 *
 * Compute the hash of an extended attribute.
 */
static void ocfs2_xattr_hash_entry(struct inode *inode,
				   struct ocfs2_xattr_header *header,
				   struct ocfs2_xattr_entry *entry)
{
	u32 hash = 0;
	char *name = (char *)header + le16_to_cpu(entry->xe_name_offset);

	hash = ocfs2_xattr_name_hash(inode, name, entry->xe_name_len);
	entry->xe_name_hash = cpu_to_le32(hash);

	return;
}

static int ocfs2_xattr_entry_real_size(int name_len, size_t value_len)
{
	int size = 0;

	if (value_len <= OCFS2_XATTR_INLINE_SIZE)
		size = OCFS2_XATTR_SIZE(name_len) + OCFS2_XATTR_SIZE(value_len);
	else
		size = OCFS2_XATTR_SIZE(name_len) + OCFS2_XATTR_ROOT_SIZE;
	size += sizeof(struct ocfs2_xattr_entry);

	return size;
}

int ocfs2_calc_security_init(struct inode *dir,
			     struct ocfs2_security_xattr_info *si,
			     int *want_clusters,
			     int *xattr_credits,
			     struct ocfs2_alloc_context **xattr_ac)
{
	int ret = 0;
	struct ocfs2_super *osb = OCFS2_SB(dir->i_sb);
	int s_size = ocfs2_xattr_entry_real_size(strlen(si->name),
						 si->value_len);

	/*
	 * The max space of security xattr taken inline is
	 * 256(name) + 80(value) + 16(entry) = 352 bytes,
	 * So reserve one metadata block for it is ok.
	 */
	if (dir->i_sb->s_blocksize == OCFS2_MIN_BLOCKSIZE ||
	    s_size > OCFS2_XATTR_FREE_IN_IBODY) {
		ret = ocfs2_reserve_new_metadata_blocks(osb, 1, xattr_ac);
		if (ret) {
			mlog_errno(ret);
			return ret;
		}
		*xattr_credits += OCFS2_XATTR_BLOCK_CREATE_CREDITS;
	}

	/* reserve clusters for xattr value which will be set in B tree*/
	if (si->value_len > OCFS2_XATTR_INLINE_SIZE) {
		int new_clusters = ocfs2_clusters_for_bytes(dir->i_sb,
							    si->value_len);

		*xattr_credits += ocfs2_clusters_to_blocks(dir->i_sb,
							   new_clusters);
		*want_clusters += new_clusters;
	}
	return ret;
}

int ocfs2_calc_xattr_init(struct inode *dir,
			  struct buffer_head *dir_bh,
			  int mode,
			  struct ocfs2_security_xattr_info *si,
			  int *want_clusters,
			  int *xattr_credits,
			  int *want_meta)
{
	int ret = 0;
	struct ocfs2_super *osb = OCFS2_SB(dir->i_sb);
	int s_size = 0, a_size = 0, acl_len = 0, new_clusters;

	if (si->enable)
		s_size = ocfs2_xattr_entry_real_size(strlen(si->name),
						     si->value_len);

	if (osb->s_mount_opt & OCFS2_MOUNT_POSIX_ACL) {
		acl_len = ocfs2_xattr_get_nolock(dir, dir_bh,
					OCFS2_XATTR_INDEX_POSIX_ACL_DEFAULT,
					"", NULL, 0);
		if (acl_len > 0) {
			a_size = ocfs2_xattr_entry_real_size(0, acl_len);
			if (S_ISDIR(mode))
				a_size <<= 1;
		} else if (acl_len != 0 && acl_len != -ENODATA) {
			mlog_errno(ret);
			return ret;
		}
	}

	if (!(s_size + a_size))
		return ret;

	/*
	 * The max space of security xattr taken inline is
	 * 256(name) + 80(value) + 16(entry) = 352 bytes,
	 * The max space of acl xattr taken inline is
	 * 80(value) + 16(entry) * 2(if directory) = 192 bytes,
	 * when blocksize = 512, may reserve one more cluser for
	 * xattr bucket, otherwise reserve one metadata block
	 * for them is ok.
	 * If this is a new directory with inline data,
	 * we choose to reserve the entire inline area for
	 * directory contents and force an external xattr block.
	 */
	if (dir->i_sb->s_blocksize == OCFS2_MIN_BLOCKSIZE ||
	    (S_ISDIR(mode) && ocfs2_supports_inline_data(osb)) ||
	    (s_size + a_size) > OCFS2_XATTR_FREE_IN_IBODY) {
		*want_meta = *want_meta + 1;
		*xattr_credits += OCFS2_XATTR_BLOCK_CREATE_CREDITS;
	}

	if (dir->i_sb->s_blocksize == OCFS2_MIN_BLOCKSIZE &&
	    (s_size + a_size) > OCFS2_XATTR_FREE_IN_BLOCK(dir)) {
		*want_clusters += 1;
		*xattr_credits += ocfs2_blocks_per_xattr_bucket(dir->i_sb);
	}

	/*
	 * reserve credits and clusters for xattrs which has large value
	 * and have to be set outside
	 */
	if (si->enable && si->value_len > OCFS2_XATTR_INLINE_SIZE) {
		new_clusters = ocfs2_clusters_for_bytes(dir->i_sb,
							si->value_len);
		*xattr_credits += ocfs2_clusters_to_blocks(dir->i_sb,
							   new_clusters);
		*want_clusters += new_clusters;
	}
	if (osb->s_mount_opt & OCFS2_MOUNT_POSIX_ACL &&
	    acl_len > OCFS2_XATTR_INLINE_SIZE) {
		/* for directory, it has DEFAULT and ACCESS two types of acls */
		new_clusters = (S_ISDIR(mode) ? 2 : 1) *
				ocfs2_clusters_for_bytes(dir->i_sb, acl_len);
		*xattr_credits += ocfs2_clusters_to_blocks(dir->i_sb,
							   new_clusters);
		*want_clusters += new_clusters;
	}

	return ret;
}

static int ocfs2_xattr_extend_allocation(struct inode *inode,
					 u32 clusters_to_add,
					 struct ocfs2_xattr_value_buf *vb,
					 struct ocfs2_xattr_set_ctxt *ctxt)
{
	int status = 0;
	handle_t *handle = ctxt->handle;
	enum ocfs2_alloc_restarted why;
	u32 prev_clusters, logical_start = le32_to_cpu(vb->vb_xv->xr_clusters);
	struct ocfs2_extent_tree et;

	mlog(0, "(clusters_to_add for xattr= %u)\n", clusters_to_add);

	ocfs2_init_xattr_value_extent_tree(&et, INODE_CACHE(inode), vb);

	status = vb->vb_access(handle, INODE_CACHE(inode), vb->vb_bh,
			      OCFS2_JOURNAL_ACCESS_WRITE);
	if (status < 0) {
		mlog_errno(status);
		goto leave;
	}

	prev_clusters = le32_to_cpu(vb->vb_xv->xr_clusters);
	status = ocfs2_add_clusters_in_btree(handle,
					     &et,
					     &logical_start,
					     clusters_to_add,
					     0,
					     ctxt->data_ac,
					     ctxt->meta_ac,
					     &why);
	if (status < 0) {
		mlog_errno(status);
		goto leave;
	}

	status = ocfs2_journal_dirty(handle, vb->vb_bh);
	if (status < 0) {
		mlog_errno(status);
		goto leave;
	}

	clusters_to_add -= le32_to_cpu(vb->vb_xv->xr_clusters) - prev_clusters;

	/*
	 * We should have already allocated enough space before the transaction,
	 * so no need to restart.
	 */
	BUG_ON(why != RESTART_NONE || clusters_to_add);

leave:

	return status;
}

static int __ocfs2_remove_xattr_range(struct inode *inode,
				      struct ocfs2_xattr_value_buf *vb,
				      u32 cpos, u32 phys_cpos, u32 len,
				      unsigned int ext_flags,
				      struct ocfs2_xattr_set_ctxt *ctxt)
{
	int ret;
	u64 phys_blkno = ocfs2_clusters_to_blocks(inode->i_sb, phys_cpos);
	handle_t *handle = ctxt->handle;
	struct ocfs2_extent_tree et;

	ocfs2_init_xattr_value_extent_tree(&et, INODE_CACHE(inode), vb);

	ret = vb->vb_access(handle, INODE_CACHE(inode), vb->vb_bh,
			    OCFS2_JOURNAL_ACCESS_WRITE);
	if (ret) {
		mlog_errno(ret);
		goto out;
	}

	ret = ocfs2_remove_extent(handle, &et, cpos, len, ctxt->meta_ac,
				  &ctxt->dealloc);
	if (ret) {
		mlog_errno(ret);
		goto out;
	}

	le32_add_cpu(&vb->vb_xv->xr_clusters, -len);

	ret = ocfs2_journal_dirty(handle, vb->vb_bh);
	if (ret) {
		mlog_errno(ret);
		goto out;
	}

	if (ext_flags & OCFS2_EXT_REFCOUNTED)
		ret = ocfs2_decrease_refcount(inode, handle,
					ocfs2_blocks_to_clusters(inode->i_sb,
								 phys_blkno),
					len, ctxt->meta_ac, &ctxt->dealloc, 1);
	else
		ret = ocfs2_cache_cluster_dealloc(&ctxt->dealloc,
						  phys_blkno, len);
	if (ret)
		mlog_errno(ret);

out:
	return ret;
}

static int ocfs2_xattr_shrink_size(struct inode *inode,
				   u32 old_clusters,
				   u32 new_clusters,
				   struct ocfs2_xattr_value_buf *vb,
				   struct ocfs2_xattr_set_ctxt *ctxt)
{
	int ret = 0;
	unsigned int ext_flags;
	u32 trunc_len, cpos, phys_cpos, alloc_size;
	u64 block;

	if (old_clusters <= new_clusters)
		return 0;

	cpos = new_clusters;
	trunc_len = old_clusters - new_clusters;
	while (trunc_len) {
		ret = ocfs2_xattr_get_clusters(inode, cpos, &phys_cpos,
					       &alloc_size,
					       &vb->vb_xv->xr_list, &ext_flags);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}

		if (alloc_size > trunc_len)
			alloc_size = trunc_len;

		ret = __ocfs2_remove_xattr_range(inode, vb, cpos,
						 phys_cpos, alloc_size,
						 ext_flags, ctxt);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}

		block = ocfs2_clusters_to_blocks(inode->i_sb, phys_cpos);
		ocfs2_remove_xattr_clusters_from_cache(INODE_CACHE(inode),
						       block, alloc_size);
		cpos += alloc_size;
		trunc_len -= alloc_size;
	}

out:
	return ret;
}

static int ocfs2_xattr_value_truncate(struct inode *inode,
				      struct ocfs2_xattr_value_buf *vb,
				      int len,
				      struct ocfs2_xattr_set_ctxt *ctxt)
{
	int ret;
	u32 new_clusters = ocfs2_clusters_for_bytes(inode->i_sb, len);
	u32 old_clusters = le32_to_cpu(vb->vb_xv->xr_clusters);

	if (new_clusters == old_clusters)
		return 0;

	if (new_clusters > old_clusters)
		ret = ocfs2_xattr_extend_allocation(inode,
						    new_clusters - old_clusters,
						    vb, ctxt);
	else
		ret = ocfs2_xattr_shrink_size(inode,
					      old_clusters, new_clusters,
					      vb, ctxt);

	return ret;
}

static int ocfs2_xattr_list_entry(char *buffer, size_t size,
				  size_t *result, const char *prefix,
				  const char *name, int name_len)
{
	char *p = buffer + *result;
	int prefix_len = strlen(prefix);
	int total_len = prefix_len + name_len + 1;

	*result += total_len;

	/* we are just looking for how big our buffer needs to be */
	if (!size)
		return 0;

	if (*result > size)
		return -ERANGE;

	memcpy(p, prefix, prefix_len);
	memcpy(p + prefix_len, name, name_len);
	p[prefix_len + name_len] = '\0';

	return 0;
}

static int ocfs2_xattr_list_entries(struct inode *inode,
				    struct ocfs2_xattr_header *header,
				    char *buffer, size_t buffer_size)
{
	size_t result = 0;
	int i, type, ret;
	const char *prefix, *name;

	for (i = 0 ; i < le16_to_cpu(header->xh_count); i++) {
		struct ocfs2_xattr_entry *entry = &header->xh_entries[i];
		type = ocfs2_xattr_get_type(entry);
		prefix = ocfs2_xattr_prefix(type);

		if (prefix) {
			name = (const char *)header +
				le16_to_cpu(entry->xe_name_offset);

			ret = ocfs2_xattr_list_entry(buffer, buffer_size,
						     &result, prefix, name,
						     entry->xe_name_len);
			if (ret)
				return ret;
		}
	}

	return result;
}

int ocfs2_has_inline_xattr_value_outside(struct inode *inode,
					 struct ocfs2_dinode *di)
{
	struct ocfs2_xattr_header *xh;
	int i;

	xh = (struct ocfs2_xattr_header *)
		 ((void *)di + inode->i_sb->s_blocksize -
		 le16_to_cpu(di->i_xattr_inline_size));

	for (i = 0; i < le16_to_cpu(xh->xh_count); i++)
		if (!ocfs2_xattr_is_local(&xh->xh_entries[i]))
			return 1;

	return 0;
}

static int ocfs2_xattr_ibody_list(struct inode *inode,
				  struct ocfs2_dinode *di,
				  char *buffer,
				  size_t buffer_size)
{
	struct ocfs2_xattr_header *header = NULL;
	struct ocfs2_inode_info *oi = OCFS2_I(inode);
	int ret = 0;

	if (!(oi->ip_dyn_features & OCFS2_INLINE_XATTR_FL))
		return ret;

	header = (struct ocfs2_xattr_header *)
		 ((void *)di + inode->i_sb->s_blocksize -
		 le16_to_cpu(di->i_xattr_inline_size));

	ret = ocfs2_xattr_list_entries(inode, header, buffer, buffer_size);

	return ret;
}

static int ocfs2_xattr_block_list(struct inode *inode,
				  struct ocfs2_dinode *di,
				  char *buffer,
				  size_t buffer_size)
{
	struct buffer_head *blk_bh = NULL;
	struct ocfs2_xattr_block *xb;
	int ret = 0;

	if (!di->i_xattr_loc)
		return ret;

	ret = ocfs2_read_xattr_block(inode, le64_to_cpu(di->i_xattr_loc),
				     &blk_bh);
	if (ret < 0) {
		mlog_errno(ret);
		return ret;
	}

	xb = (struct ocfs2_xattr_block *)blk_bh->b_data;
	if (!(le16_to_cpu(xb->xb_flags) & OCFS2_XATTR_INDEXED)) {
		struct ocfs2_xattr_header *header = &xb->xb_attrs.xb_header;
		ret = ocfs2_xattr_list_entries(inode, header,
					       buffer, buffer_size);
	} else
		ret = ocfs2_xattr_tree_list_index_block(inode, blk_bh,
						   buffer, buffer_size);

	brelse(blk_bh);

	return ret;
}

ssize_t ocfs2_listxattr(struct dentry *dentry,
			char *buffer,
			size_t size)
{
	int ret = 0, i_ret = 0, b_ret = 0;
	struct buffer_head *di_bh = NULL;
	struct ocfs2_dinode *di = NULL;
	struct ocfs2_inode_info *oi = OCFS2_I(dentry->d_inode);

	if (!ocfs2_supports_xattr(OCFS2_SB(dentry->d_sb)))
		return -EOPNOTSUPP;

	if (!(oi->ip_dyn_features & OCFS2_HAS_XATTR_FL))
		return ret;

	ret = ocfs2_inode_lock(dentry->d_inode, &di_bh, 0);
	if (ret < 0) {
		mlog_errno(ret);
		return ret;
	}

	di = (struct ocfs2_dinode *)di_bh->b_data;

	down_read(&oi->ip_xattr_sem);
	i_ret = ocfs2_xattr_ibody_list(dentry->d_inode, di, buffer, size);
	if (i_ret < 0)
		b_ret = 0;
	else {
		if (buffer) {
			buffer += i_ret;
			size -= i_ret;
		}
		b_ret = ocfs2_xattr_block_list(dentry->d_inode, di,
					       buffer, size);
		if (b_ret < 0)
			i_ret = 0;
	}
	up_read(&oi->ip_xattr_sem);
	ocfs2_inode_unlock(dentry->d_inode, 0);

	brelse(di_bh);

	return i_ret + b_ret;
}

static int ocfs2_xattr_find_entry(int name_index,
				  const char *name,
				  struct ocfs2_xattr_search *xs)
{
	struct ocfs2_xattr_entry *entry;
	size_t name_len;
	int i, cmp = 1;

	if (name == NULL)
		return -EINVAL;

	name_len = strlen(name);
	entry = xs->here;
	for (i = 0; i < le16_to_cpu(xs->header->xh_count); i++) {
		cmp = name_index - ocfs2_xattr_get_type(entry);
		if (!cmp)
			cmp = name_len - entry->xe_name_len;
		if (!cmp)
			cmp = memcmp(name, (xs->base +
				     le16_to_cpu(entry->xe_name_offset)),
				     name_len);
		if (cmp == 0)
			break;
		entry += 1;
	}
	xs->here = entry;

	return cmp ? -ENODATA : 0;
}

static int ocfs2_xattr_get_value_outside(struct inode *inode,
					 struct ocfs2_xattr_value_root *xv,
					 void *buffer,
					 size_t len)
{
	u32 cpos, p_cluster, num_clusters, bpc, clusters;
	u64 blkno;
	int i, ret = 0;
	size_t cplen, blocksize;
	struct buffer_head *bh = NULL;
	struct ocfs2_extent_list *el;

	el = &xv->xr_list;
	clusters = le32_to_cpu(xv->xr_clusters);
	bpc = ocfs2_clusters_to_blocks(inode->i_sb, 1);
	blocksize = inode->i_sb->s_blocksize;

	cpos = 0;
	while (cpos < clusters) {
		ret = ocfs2_xattr_get_clusters(inode, cpos, &p_cluster,
					       &num_clusters, el, NULL);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}

		blkno = ocfs2_clusters_to_blocks(inode->i_sb, p_cluster);
		/* Copy ocfs2_xattr_value */
		for (i = 0; i < num_clusters * bpc; i++, blkno++) {
			ret = ocfs2_read_block(INODE_CACHE(inode), blkno,
					       &bh, NULL);
			if (ret) {
				mlog_errno(ret);
				goto out;
			}

			cplen = len >= blocksize ? blocksize : len;
			memcpy(buffer, bh->b_data, cplen);
			len -= cplen;
			buffer += cplen;

			brelse(bh);
			bh = NULL;
			if (len == 0)
				break;
		}
		cpos += num_clusters;
	}
out:
	return ret;
}

static int ocfs2_xattr_ibody_get(struct inode *inode,
				 int name_index,
				 const char *name,
				 void *buffer,
				 size_t buffer_size,
				 struct ocfs2_xattr_search *xs)
{
	struct ocfs2_inode_info *oi = OCFS2_I(inode);
	struct ocfs2_dinode *di = (struct ocfs2_dinode *)xs->inode_bh->b_data;
	struct ocfs2_xattr_value_root *xv;
	size_t size;
	int ret = 0;

	if (!(oi->ip_dyn_features & OCFS2_INLINE_XATTR_FL))
		return -ENODATA;

	xs->end = (void *)di + inode->i_sb->s_blocksize;
	xs->header = (struct ocfs2_xattr_header *)
			(xs->end - le16_to_cpu(di->i_xattr_inline_size));
	xs->base = (void *)xs->header;
	xs->here = xs->header->xh_entries;

	ret = ocfs2_xattr_find_entry(name_index, name, xs);
	if (ret)
		return ret;
	size = le64_to_cpu(xs->here->xe_value_size);
	if (buffer) {
		if (size > buffer_size)
			return -ERANGE;
		if (ocfs2_xattr_is_local(xs->here)) {
			memcpy(buffer, (void *)xs->base +
			       le16_to_cpu(xs->here->xe_name_offset) +
			       OCFS2_XATTR_SIZE(xs->here->xe_name_len), size);
		} else {
			xv = (struct ocfs2_xattr_value_root *)
				(xs->base + le16_to_cpu(
				 xs->here->xe_name_offset) +
				OCFS2_XATTR_SIZE(xs->here->xe_name_len));
			ret = ocfs2_xattr_get_value_outside(inode, xv,
							    buffer, size);
			if (ret < 0) {
				mlog_errno(ret);
				return ret;
			}
		}
	}

	return size;
}

static int ocfs2_xattr_block_get(struct inode *inode,
				 int name_index,
				 const char *name,
				 void *buffer,
				 size_t buffer_size,
				 struct ocfs2_xattr_search *xs)
{
	struct ocfs2_xattr_block *xb;
	struct ocfs2_xattr_value_root *xv;
	size_t size;
	int ret = -ENODATA, name_offset, name_len, i;
	int uninitialized_var(block_off);

	xs->bucket = ocfs2_xattr_bucket_new(inode);
	if (!xs->bucket) {
		ret = -ENOMEM;
		mlog_errno(ret);
		goto cleanup;
	}

	ret = ocfs2_xattr_block_find(inode, name_index, name, xs);
	if (ret) {
		mlog_errno(ret);
		goto cleanup;
	}

	if (xs->not_found) {
		ret = -ENODATA;
		goto cleanup;
	}

	xb = (struct ocfs2_xattr_block *)xs->xattr_bh->b_data;
	size = le64_to_cpu(xs->here->xe_value_size);
	if (buffer) {
		ret = -ERANGE;
		if (size > buffer_size)
			goto cleanup;

		name_offset = le16_to_cpu(xs->here->xe_name_offset);
		name_len = OCFS2_XATTR_SIZE(xs->here->xe_name_len);
		i = xs->here - xs->header->xh_entries;

		if (le16_to_cpu(xb->xb_flags) & OCFS2_XATTR_INDEXED) {
			ret = ocfs2_xattr_bucket_get_name_value(inode->i_sb,
								bucket_xh(xs->bucket),
								i,
								&block_off,
								&name_offset);
			xs->base = bucket_block(xs->bucket, block_off);
		}
		if (ocfs2_xattr_is_local(xs->here)) {
			memcpy(buffer, (void *)xs->base +
			       name_offset + name_len, size);
		} else {
			xv = (struct ocfs2_xattr_value_root *)
				(xs->base + name_offset + name_len);
			ret = ocfs2_xattr_get_value_outside(inode, xv,
							    buffer, size);
			if (ret < 0) {
				mlog_errno(ret);
				goto cleanup;
			}
		}
	}
	ret = size;
cleanup:
	ocfs2_xattr_bucket_free(xs->bucket);

	brelse(xs->xattr_bh);
	xs->xattr_bh = NULL;
	return ret;
}

int ocfs2_xattr_get_nolock(struct inode *inode,
			   struct buffer_head *di_bh,
			   int name_index,
			   const char *name,
			   void *buffer,
			   size_t buffer_size)
{
	int ret;
	struct ocfs2_dinode *di = NULL;
	struct ocfs2_inode_info *oi = OCFS2_I(inode);
	struct ocfs2_xattr_search xis = {
		.not_found = -ENODATA,
	};
	struct ocfs2_xattr_search xbs = {
		.not_found = -ENODATA,
	};

	if (!ocfs2_supports_xattr(OCFS2_SB(inode->i_sb)))
		return -EOPNOTSUPP;

	if (!(oi->ip_dyn_features & OCFS2_HAS_XATTR_FL))
		ret = -ENODATA;

	xis.inode_bh = xbs.inode_bh = di_bh;
	di = (struct ocfs2_dinode *)di_bh->b_data;

	down_read(&oi->ip_xattr_sem);
	ret = ocfs2_xattr_ibody_get(inode, name_index, name, buffer,
				    buffer_size, &xis);
	if (ret == -ENODATA && di->i_xattr_loc)
		ret = ocfs2_xattr_block_get(inode, name_index, name, buffer,
					    buffer_size, &xbs);
	up_read(&oi->ip_xattr_sem);

	return ret;
}

/* ocfs2_xattr_get()
 *
 * Copy an extended attribute into the buffer provided.
 * Buffer is NULL to compute the size of buffer required.
 */
static int ocfs2_xattr_get(struct inode *inode,
			   int name_index,
			   const char *name,
			   void *buffer,
			   size_t buffer_size)
{
	int ret;
	struct buffer_head *di_bh = NULL;

	ret = ocfs2_inode_lock(inode, &di_bh, 0);
	if (ret < 0) {
		mlog_errno(ret);
		return ret;
	}
	ret = ocfs2_xattr_get_nolock(inode, di_bh, name_index,
				     name, buffer, buffer_size);

	ocfs2_inode_unlock(inode, 0);

	brelse(di_bh);

	return ret;
}

static int __ocfs2_xattr_set_value_outside(struct inode *inode,
					   handle_t *handle,
					   struct ocfs2_xattr_value_buf *vb,
					   const void *value,
					   int value_len)
{
	int ret = 0, i, cp_len;
	u16 blocksize = inode->i_sb->s_blocksize;
	u32 p_cluster, num_clusters;
	u32 cpos = 0, bpc = ocfs2_clusters_to_blocks(inode->i_sb, 1);
	u32 clusters = ocfs2_clusters_for_bytes(inode->i_sb, value_len);
	u64 blkno;
	struct buffer_head *bh = NULL;
	unsigned int ext_flags;
	struct ocfs2_xattr_value_root *xv = vb->vb_xv;

	BUG_ON(clusters > le32_to_cpu(xv->xr_clusters));

	while (cpos < clusters) {
		ret = ocfs2_xattr_get_clusters(inode, cpos, &p_cluster,
					       &num_clusters, &xv->xr_list,
					       &ext_flags);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}

		BUG_ON(ext_flags & OCFS2_EXT_REFCOUNTED);

		blkno = ocfs2_clusters_to_blocks(inode->i_sb, p_cluster);

		for (i = 0; i < num_clusters * bpc; i++, blkno++) {
			ret = ocfs2_read_block(INODE_CACHE(inode), blkno,
					       &bh, NULL);
			if (ret) {
				mlog_errno(ret);
				goto out;
			}

			ret = ocfs2_journal_access(handle,
						   INODE_CACHE(inode),
						   bh,
						   OCFS2_JOURNAL_ACCESS_WRITE);
			if (ret < 0) {
				mlog_errno(ret);
				goto out;
			}

			cp_len = value_len > blocksize ? blocksize : value_len;
			memcpy(bh->b_data, value, cp_len);
			value_len -= cp_len;
			value += cp_len;
			if (cp_len < blocksize)
				memset(bh->b_data + cp_len, 0,
				       blocksize - cp_len);

			ret = ocfs2_journal_dirty(handle, bh);
			if (ret < 0) {
				mlog_errno(ret);
				goto out;
			}
			brelse(bh);
			bh = NULL;

			/*
			 * XXX: do we need to empty all the following
			 * blocks in this cluster?
			 */
			if (!value_len)
				break;
		}
		cpos += num_clusters;
	}
out:
	brelse(bh);

	return ret;
}

static int ocfs2_xattr_cleanup(struct inode *inode,
			       handle_t *handle,
			       struct ocfs2_xattr_info *xi,
			       struct ocfs2_xattr_search *xs,
			       struct ocfs2_xattr_value_buf *vb,
			       size_t offs)
{
	int ret = 0;
	size_t name_len = strlen(xi->name);
	void *val = xs->base + offs;
	size_t size = OCFS2_XATTR_SIZE(name_len) + OCFS2_XATTR_ROOT_SIZE;

	ret = vb->vb_access(handle, INODE_CACHE(inode), vb->vb_bh,
			    OCFS2_JOURNAL_ACCESS_WRITE);
	if (ret) {
		mlog_errno(ret);
		goto out;
	}
	/* Decrease xattr count */
	le16_add_cpu(&xs->header->xh_count, -1);
	/* Remove the xattr entry and tree root which has already be set*/
	memset((void *)xs->here, 0, sizeof(struct ocfs2_xattr_entry));
	memset(val, 0, size);

	ret = ocfs2_journal_dirty(handle, vb->vb_bh);
	if (ret < 0)
		mlog_errno(ret);
out:
	return ret;
}

static int ocfs2_xattr_update_entry(struct inode *inode,
				    handle_t *handle,
				    struct ocfs2_xattr_info *xi,
				    struct ocfs2_xattr_search *xs,
				    struct ocfs2_xattr_value_buf *vb,
				    size_t offs)
{
	int ret;

	ret = vb->vb_access(handle, INODE_CACHE(inode), vb->vb_bh,
			    OCFS2_JOURNAL_ACCESS_WRITE);
	if (ret) {
		mlog_errno(ret);
		goto out;
	}

	xs->here->xe_name_offset = cpu_to_le16(offs);
	xs->here->xe_value_size = cpu_to_le64(xi->value_len);
	if (xi->value_len <= OCFS2_XATTR_INLINE_SIZE)
		ocfs2_xattr_set_local(xs->here, 1);
	else
		ocfs2_xattr_set_local(xs->here, 0);
	ocfs2_xattr_hash_entry(inode, xs->header, xs->here);

	ret = ocfs2_journal_dirty(handle, vb->vb_bh);
	if (ret < 0)
		mlog_errno(ret);
out:
	return ret;
}

/*
 * ocfs2_xattr_set_value_outside()
 *
 * Set large size value in B tree.
 */
static int ocfs2_xattr_set_value_outside(struct inode *inode,
					 struct ocfs2_xattr_info *xi,
					 struct ocfs2_xattr_search *xs,
					 struct ocfs2_xattr_set_ctxt *ctxt,
					 struct ocfs2_xattr_value_buf *vb,
					 size_t offs)
{
	size_t name_len = strlen(xi->name);
	void *val = xs->base + offs;
	struct ocfs2_xattr_value_root *xv = NULL;
	size_t size = OCFS2_XATTR_SIZE(name_len) + OCFS2_XATTR_ROOT_SIZE;
	int ret = 0;

	memset(val, 0, size);
	memcpy(val, xi->name, name_len);
	xv = (struct ocfs2_xattr_value_root *)
		(val + OCFS2_XATTR_SIZE(name_len));
	xv->xr_clusters = 0;
	xv->xr_last_eb_blk = 0;
	xv->xr_list.l_tree_depth = 0;
	xv->xr_list.l_count = cpu_to_le16(1);
	xv->xr_list.l_next_free_rec = 0;
	vb->vb_xv = xv;

	ret = ocfs2_xattr_value_truncate(inode, vb, xi->value_len, ctxt);
	if (ret < 0) {
		mlog_errno(ret);
		return ret;
	}
	ret = ocfs2_xattr_update_entry(inode, ctxt->handle, xi, xs, vb, offs);
	if (ret < 0) {
		mlog_errno(ret);
		return ret;
	}
	ret = __ocfs2_xattr_set_value_outside(inode, ctxt->handle, vb,
					      xi->value, xi->value_len);
	if (ret < 0)
		mlog_errno(ret);

	return ret;
}

/*
 * ocfs2_xattr_set_entry_local()
 *
 * Set, replace or remove extended attribute in local.
 */
static void ocfs2_xattr_set_entry_local(struct inode *inode,
					struct ocfs2_xattr_info *xi,
					struct ocfs2_xattr_search *xs,
					struct ocfs2_xattr_entry *last,
					size_t min_offs)
{
	size_t name_len = strlen(xi->name);
	int i;

	if (xi->value && xs->not_found) {
		/* Insert the new xattr entry. */
		le16_add_cpu(&xs->header->xh_count, 1);
		ocfs2_xattr_set_type(last, xi->name_index);
		ocfs2_xattr_set_local(last, 1);
		last->xe_name_len = name_len;
	} else {
		void *first_val;
		void *val;
		size_t offs, size;

		first_val = xs->base + min_offs;
		offs = le16_to_cpu(xs->here->xe_name_offset);
		val = xs->base + offs;

		if (le64_to_cpu(xs->here->xe_value_size) >
		    OCFS2_XATTR_INLINE_SIZE)
			size = OCFS2_XATTR_SIZE(name_len) +
				OCFS2_XATTR_ROOT_SIZE;
		else
			size = OCFS2_XATTR_SIZE(name_len) +
			OCFS2_XATTR_SIZE(le64_to_cpu(xs->here->xe_value_size));

		if (xi->value && size == OCFS2_XATTR_SIZE(name_len) +
				OCFS2_XATTR_SIZE(xi->value_len)) {
			/* The old and the new value have the
			   same size. Just replace the value. */
			ocfs2_xattr_set_local(xs->here, 1);
			xs->here->xe_value_size = cpu_to_le64(xi->value_len);
			/* Clear value bytes. */
			memset(val + OCFS2_XATTR_SIZE(name_len),
			       0,
			       OCFS2_XATTR_SIZE(xi->value_len));
			memcpy(val + OCFS2_XATTR_SIZE(name_len),
			       xi->value,
			       xi->value_len);
			return;
		}
		/* Remove the old name+value. */
		memmove(first_val + size, first_val, val - first_val);
		memset(first_val, 0, size);
		xs->here->xe_name_hash = 0;
		xs->here->xe_name_offset = 0;
		ocfs2_xattr_set_local(xs->here, 1);
		xs->here->xe_value_size = 0;

		min_offs += size;

		/* Adjust all value offsets. */
		last = xs->header->xh_entries;
		for (i = 0 ; i < le16_to_cpu(xs->header->xh_count); i++) {
			size_t o = le16_to_cpu(last->xe_name_offset);

			if (o < offs)
				last->xe_name_offset = cpu_to_le16(o + size);
			last += 1;
		}

		if (!xi->value) {
			/* Remove the old entry. */
			last -= 1;
			memmove(xs->here, xs->here + 1,
				(void *)last - (void *)xs->here);
			memset(last, 0, sizeof(struct ocfs2_xattr_entry));
			le16_add_cpu(&xs->header->xh_count, -1);
		}
	}
	if (xi->value) {
		/* Insert the new name+value. */
		size_t size = OCFS2_XATTR_SIZE(name_len) +
				OCFS2_XATTR_SIZE(xi->value_len);
		void *val = xs->base + min_offs - size;

		xs->here->xe_name_offset = cpu_to_le16(min_offs - size);
		memset(val, 0, size);
		memcpy(val, xi->name, name_len);
		memcpy(val + OCFS2_XATTR_SIZE(name_len),
		       xi->value,
		       xi->value_len);
		xs->here->xe_value_size = cpu_to_le64(xi->value_len);
		ocfs2_xattr_set_local(xs->here, 1);
		ocfs2_xattr_hash_entry(inode, xs->header, xs->here);
	}

	return;
}

/*
 * ocfs2_xattr_set_entry()
 *
 * Set extended attribute entry into inode or block.
 *
 * If extended attribute value size > OCFS2_XATTR_INLINE_SIZE,
 * We first insert tree root(ocfs2_xattr_value_root) with set_entry_local(),
 * then set value in B tree with set_value_outside().
 */
static int ocfs2_xattr_set_entry(struct inode *inode,
				 struct ocfs2_xattr_info *xi,
				 struct ocfs2_xattr_search *xs,
				 struct ocfs2_xattr_set_ctxt *ctxt,
				 int flag)
{
	struct ocfs2_xattr_entry *last;
	struct ocfs2_inode_info *oi = OCFS2_I(inode);
	struct ocfs2_dinode *di = (struct ocfs2_dinode *)xs->inode_bh->b_data;
	size_t min_offs = xs->end - xs->base, name_len = strlen(xi->name);
	size_t size_l = 0;
	handle_t *handle = ctxt->handle;
	int free, i, ret;
	struct ocfs2_xattr_info xi_l = {
		.name_index = xi->name_index,
		.name = xi->name,
		.value = xi->value,
		.value_len = xi->value_len,
	};
	struct ocfs2_xattr_value_buf vb = {
		.vb_bh = xs->xattr_bh,
		.vb_access = ocfs2_journal_access_di,
	};

	if (!(flag & OCFS2_INLINE_XATTR_FL)) {
		BUG_ON(xs->xattr_bh == xs->inode_bh);
		vb.vb_access = ocfs2_journal_access_xb;
	} else
		BUG_ON(xs->xattr_bh != xs->inode_bh);

	/* Compute min_offs, last and free space. */
	last = xs->header->xh_entries;

	for (i = 0 ; i < le16_to_cpu(xs->header->xh_count); i++) {
		size_t offs = le16_to_cpu(last->xe_name_offset);
		if (offs < min_offs)
			min_offs = offs;
		last += 1;
	}

	free = min_offs - ((void *)last - xs->base) - OCFS2_XATTR_HEADER_GAP;
	if (free < 0)
		return -EIO;

	if (!xs->not_found) {
		size_t size = 0;
		if (ocfs2_xattr_is_local(xs->here))
			size = OCFS2_XATTR_SIZE(name_len) +
			OCFS2_XATTR_SIZE(le64_to_cpu(xs->here->xe_value_size));
		else
			size = OCFS2_XATTR_SIZE(name_len) +
				OCFS2_XATTR_ROOT_SIZE;
		free += (size + sizeof(struct ocfs2_xattr_entry));
	}
	/* Check free space in inode or block */
	if (xi->value && xi->value_len > OCFS2_XATTR_INLINE_SIZE) {
		if (free < sizeof(struct ocfs2_xattr_entry) +
			   OCFS2_XATTR_SIZE(name_len) +
			   OCFS2_XATTR_ROOT_SIZE) {
			ret = -ENOSPC;
			goto out;
		}
		size_l = OCFS2_XATTR_SIZE(name_len) + OCFS2_XATTR_ROOT_SIZE;
		xi_l.value = (void *)&def_xv;
		xi_l.value_len = OCFS2_XATTR_ROOT_SIZE;
	} else if (xi->value) {
		if (free < sizeof(struct ocfs2_xattr_entry) +
			   OCFS2_XATTR_SIZE(name_len) +
			   OCFS2_XATTR_SIZE(xi->value_len)) {
			ret = -ENOSPC;
			goto out;
		}
	}

	if (!xs->not_found) {
		/* For existing extended attribute */
		size_t size = OCFS2_XATTR_SIZE(name_len) +
			OCFS2_XATTR_SIZE(le64_to_cpu(xs->here->xe_value_size));
		size_t offs = le16_to_cpu(xs->here->xe_name_offset);
		void *val = xs->base + offs;

		if (ocfs2_xattr_is_local(xs->here) && size == size_l) {
			/* Replace existing local xattr with tree root */
			ret = ocfs2_xattr_set_value_outside(inode, xi, xs,
							    ctxt, &vb, offs);
			if (ret < 0)
				mlog_errno(ret);
			goto out;
		} else if (!ocfs2_xattr_is_local(xs->here)) {
			/* For existing xattr which has value outside */
			vb.vb_xv = (struct ocfs2_xattr_value_root *)
				(val + OCFS2_XATTR_SIZE(name_len));

			if (xi->value_len > OCFS2_XATTR_INLINE_SIZE) {
				/*
				 * If new value need set outside also,
				 * first truncate old value to new value,
				 * then set new value with set_value_outside().
				 */
				ret = ocfs2_xattr_value_truncate(inode,
								 &vb,
								 xi->value_len,
								 ctxt);
				if (ret < 0) {
					mlog_errno(ret);
					goto out;
				}

				ret = ocfs2_xattr_update_entry(inode,
							       handle,
							       xi,
							       xs,
							       &vb,
							       offs);
				if (ret < 0) {
					mlog_errno(ret);
					goto out;
				}

				ret = __ocfs2_xattr_set_value_outside(inode,
								handle,
								&vb,
								xi->value,
								xi->value_len);
				if (ret < 0)
					mlog_errno(ret);
				goto out;
			} else {
				/*
				 * If new value need set in local,
				 * just trucate old value to zero.
				 */
				 ret = ocfs2_xattr_value_truncate(inode,
								  &vb,
								  0,
								  ctxt);
				if (ret < 0)
					mlog_errno(ret);
			}
		}
	}

	ret = ocfs2_journal_access_di(handle, INODE_CACHE(inode), xs->inode_bh,
				      OCFS2_JOURNAL_ACCESS_WRITE);
	if (ret) {
		mlog_errno(ret);
		goto out;
	}

	if (!(flag & OCFS2_INLINE_XATTR_FL)) {
		ret = vb.vb_access(handle, INODE_CACHE(inode), vb.vb_bh,
				   OCFS2_JOURNAL_ACCESS_WRITE);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}
	}

	/*
	 * Set value in local, include set tree root in local.
	 * This is the first step for value size >INLINE_SIZE.
	 */
	ocfs2_xattr_set_entry_local(inode, &xi_l, xs, last, min_offs);

	if (!(flag & OCFS2_INLINE_XATTR_FL)) {
		ret = ocfs2_journal_dirty(handle, xs->xattr_bh);
		if (ret < 0) {
			mlog_errno(ret);
			goto out;
		}
	}

	if (!(oi->ip_dyn_features & OCFS2_INLINE_XATTR_FL) &&
	    (flag & OCFS2_INLINE_XATTR_FL)) {
		struct ocfs2_super *osb = OCFS2_SB(inode->i_sb);
		unsigned int xattrsize = osb->s_xattr_inline_size;

		/*
		 * Adjust extent record count or inline data size
		 * to reserve space for extended attribute.
		 */
		if (oi->ip_dyn_features & OCFS2_INLINE_DATA_FL) {
			struct ocfs2_inline_data *idata = &di->id2.i_data;
			le16_add_cpu(&idata->id_count, -xattrsize);
		} else if (!(ocfs2_inode_is_fast_symlink(inode))) {
			struct ocfs2_extent_list *el = &di->id2.i_list;
			le16_add_cpu(&el->l_count, -(xattrsize /
					sizeof(struct ocfs2_extent_rec)));
		}
		di->i_xattr_inline_size = cpu_to_le16(xattrsize);
	}
	/* Update xattr flag */
	spin_lock(&oi->ip_lock);
	oi->ip_dyn_features |= flag;
	di->i_dyn_features = cpu_to_le16(oi->ip_dyn_features);
	spin_unlock(&oi->ip_lock);

	ret = ocfs2_journal_dirty(handle, xs->inode_bh);
	if (ret < 0)
		mlog_errno(ret);

	if (!ret && xi->value_len > OCFS2_XATTR_INLINE_SIZE) {
		/*
		 * Set value outside in B tree.
		 * This is the second step for value size > INLINE_SIZE.
		 */
		size_t offs = le16_to_cpu(xs->here->xe_name_offset);
		ret = ocfs2_xattr_set_value_outside(inode, xi, xs, ctxt,
						    &vb, offs);
		if (ret < 0) {
			int ret2;

			mlog_errno(ret);
			/*
			 * If set value outside failed, we have to clean
			 * the junk tree root we have already set in local.
			 */
			ret2 = ocfs2_xattr_cleanup(inode, ctxt->handle,
						   xi, xs, &vb, offs);
			if (ret2 < 0)
				mlog_errno(ret2);
		}
	}
out:
	return ret;
}

/*
 * In xattr remove, if it is stored outside and refcounted, we may have
 * the chance to split the refcount tree. So need the allocators.
 */
static int ocfs2_lock_xattr_remove_allocators(struct inode *inode,
					struct ocfs2_xattr_value_root *xv,
					struct ocfs2_caching_info *ref_ci,
					struct buffer_head *ref_root_bh,
					struct ocfs2_alloc_context **meta_ac,
					int *ref_credits)
{
	int ret, meta_add = 0;
	u32 p_cluster, num_clusters;
	unsigned int ext_flags;

	*ref_credits = 0;
	ret = ocfs2_xattr_get_clusters(inode, 0, &p_cluster,
				       &num_clusters,
				       &xv->xr_list,
				       &ext_flags);
	if (ret) {
		mlog_errno(ret);
		goto out;
	}

	if (!(ext_flags & OCFS2_EXT_REFCOUNTED))
		goto out;

	ret = ocfs2_refcounted_xattr_delete_need(inode, ref_ci,
						 ref_root_bh, xv,
						 &meta_add, ref_credits);
	if (ret) {
		mlog_errno(ret);
		goto out;
	}

	ret = ocfs2_reserve_new_metadata_blocks(OCFS2_SB(inode->i_sb),
						meta_add, meta_ac);
	if (ret)
		mlog_errno(ret);

out:
	return ret;
}

static int ocfs2_remove_value_outside(struct inode*inode,
				      struct ocfs2_xattr_value_buf *vb,
				      struct ocfs2_xattr_header *header,
				      struct ocfs2_caching_info *ref_ci,
				      struct buffer_head *ref_root_bh)
{
	int ret = 0, i, ref_credits;
	struct ocfs2_super *osb = OCFS2_SB(inode->i_sb);
	struct ocfs2_xattr_set_ctxt ctxt = { NULL, NULL, };
	void *val;

	ocfs2_init_dealloc_ctxt(&ctxt.dealloc);

	for (i = 0; i < le16_to_cpu(header->xh_count); i++) {
		struct ocfs2_xattr_entry *entry = &header->xh_entries[i];

		if (ocfs2_xattr_is_local(entry))
			continue;

		val = (void *)header +
			le16_to_cpu(entry->xe_name_offset);
		vb->vb_xv = (struct ocfs2_xattr_value_root *)
			(val + OCFS2_XATTR_SIZE(entry->xe_name_len));

		ret = ocfs2_lock_xattr_remove_allocators(inode, vb->vb_xv,
							 ref_ci, ref_root_bh,
							 &ctxt.meta_ac,
							 &ref_credits);

		ctxt.handle = ocfs2_start_trans(osb, ref_credits +
					ocfs2_remove_extent_credits(osb->sb));
		if (IS_ERR(ctxt.handle)) {
			ret = PTR_ERR(ctxt.handle);
			mlog_errno(ret);
			break;
		}

		ret = ocfs2_xattr_value_truncate(inode, vb, 0, &ctxt);
		if (ret < 0) {
			mlog_errno(ret);
			break;
		}

		ocfs2_commit_trans(osb, ctxt.handle);
		if (ctxt.meta_ac) {
			ocfs2_free_alloc_context(ctxt.meta_ac);
			ctxt.meta_ac = NULL;
		}
	}

	if (ctxt.meta_ac)
		ocfs2_free_alloc_context(ctxt.meta_ac);
	ocfs2_schedule_truncate_log_flush(osb, 1);
	ocfs2_run_deallocs(osb, &ctxt.dealloc);
	return ret;
}

static int ocfs2_xattr_ibody_remove(struct inode *inode,
				    struct buffer_head *di_bh,
				    struct ocfs2_caching_info *ref_ci,
				    struct buffer_head *ref_root_bh)
{

	struct ocfs2_dinode *di = (struct ocfs2_dinode *)di_bh->b_data;
	struct ocfs2_xattr_header *header;
	int ret;
	struct ocfs2_xattr_value_buf vb = {
		.vb_bh = di_bh,
		.vb_access = ocfs2_journal_access_di,
	};

	header = (struct ocfs2_xattr_header *)
		 ((void *)di + inode->i_sb->s_blocksize -
		 le16_to_cpu(di->i_xattr_inline_size));

	ret = ocfs2_remove_value_outside(inode, &vb, header,
					 ref_ci, ref_root_bh);

	return ret;
}

struct ocfs2_rm_xattr_bucket_para {
	struct ocfs2_caching_info *ref_ci;
	struct buffer_head *ref_root_bh;
};

static int ocfs2_xattr_block_remove(struct inode *inode,
				    struct buffer_head *blk_bh,
				    struct ocfs2_caching_info *ref_ci,
				    struct buffer_head *ref_root_bh)
{
	struct ocfs2_xattr_block *xb;
	int ret = 0;
	struct ocfs2_xattr_value_buf vb = {
		.vb_bh = blk_bh,
		.vb_access = ocfs2_journal_access_xb,
	};
	struct ocfs2_rm_xattr_bucket_para args = {
		.ref_ci = ref_ci,
		.ref_root_bh = ref_root_bh,
	};

	xb = (struct ocfs2_xattr_block *)blk_bh->b_data;
	if (!(le16_to_cpu(xb->xb_flags) & OCFS2_XATTR_INDEXED)) {
		struct ocfs2_xattr_header *header = &(xb->xb_attrs.xb_header);
		ret = ocfs2_remove_value_outside(inode, &vb, header,
						 ref_ci, ref_root_bh);
	} else
		ret = ocfs2_iterate_xattr_index_block(inode,
						blk_bh,
						ocfs2_rm_xattr_cluster,
						&args);

	return ret;
}

static int ocfs2_xattr_free_block(struct inode *inode,
				  u64 block,
				  struct ocfs2_caching_info *ref_ci,
				  struct buffer_head *ref_root_bh)
{
	struct inode *xb_alloc_inode;
	struct buffer_head *xb_alloc_bh = NULL;
	struct buffer_head *blk_bh = NULL;
	struct ocfs2_xattr_block *xb;
	struct ocfs2_super *osb = OCFS2_SB(inode->i_sb);
	handle_t *handle;
	int ret = 0;
	u64 blk, bg_blkno;
	u16 bit;

	ret = ocfs2_read_xattr_block(inode, block, &blk_bh);
	if (ret < 0) {
		mlog_errno(ret);
		goto out;
	}

	ret = ocfs2_xattr_block_remove(inode, blk_bh, ref_ci, ref_root_bh);
	if (ret < 0) {
		mlog_errno(ret);
		goto out;
	}

	xb = (struct ocfs2_xattr_block *)blk_bh->b_data;
	blk = le64_to_cpu(xb->xb_blkno);
	bit = le16_to_cpu(xb->xb_suballoc_bit);
	bg_blkno = ocfs2_which_suballoc_group(blk, bit);

	xb_alloc_inode = ocfs2_get_system_file_inode(osb,
				EXTENT_ALLOC_SYSTEM_INODE,
				le16_to_cpu(xb->xb_suballoc_slot));
	if (!xb_alloc_inode) {
		ret = -ENOMEM;
		mlog_errno(ret);
		goto out;
	}
	mutex_lock(&xb_alloc_inode->i_mutex);

	ret = ocfs2_inode_lock(xb_alloc_inode, &xb_alloc_bh, 1);
	if (ret < 0) {
		mlog_errno(ret);
		goto out_mutex;
	}

	handle = ocfs2_start_trans(osb, OCFS2_SUBALLOC_FREE);
	if (IS_ERR(handle)) {
		ret = PTR_ERR(handle);
		mlog_errno(ret);
		goto out_unlock;
	}

	ret = ocfs2_free_suballoc_bits(handle, xb_alloc_inode, xb_alloc_bh,
				       bit, bg_blkno, 1);
	if (ret < 0)
		mlog_errno(ret);

	ocfs2_commit_trans(osb, handle);
out_unlock:
	ocfs2_inode_unlock(xb_alloc_inode, 1);
	brelse(xb_alloc_bh);
out_mutex:
	mutex_unlock(&xb_alloc_inode->i_mutex);
	iput(xb_alloc_inode);
out:
	brelse(blk_bh);
	return ret;
}

/*
 * ocfs2_xattr_remove()
 *
 * Free extended attribute resources associated with this inode.
 */
int ocfs2_xattr_remove(struct inode *inode, struct buffer_head *di_bh)
{
	struct ocfs2_inode_info *oi = OCFS2_I(inode);
	struct ocfs2_dinode *di = (struct ocfs2_dinode *)di_bh->b_data;
	struct ocfs2_refcount_tree *ref_tree = NULL;
	struct buffer_head *ref_root_bh = NULL;
	struct ocfs2_caching_info *ref_ci = NULL;
	handle_t *handle;
	int ret;

	if (!ocfs2_supports_xattr(OCFS2_SB(inode->i_sb)))
		return 0;

	if (!(oi->ip_dyn_features & OCFS2_HAS_XATTR_FL))
		return 0;

	if (OCFS2_I(inode)->ip_dyn_features & OCFS2_HAS_REFCOUNT_FL) {
		ret = ocfs2_lock_refcount_tree(OCFS2_SB(inode->i_sb),
					       le64_to_cpu(di->i_refcount_loc),
					       1, &ref_tree, &ref_root_bh);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}
		ref_ci = &ref_tree->rf_ci;

	}

	if (oi->ip_dyn_features & OCFS2_INLINE_XATTR_FL) {
		ret = ocfs2_xattr_ibody_remove(inode, di_bh,
					       ref_ci, ref_root_bh);
		if (ret < 0) {
			mlog_errno(ret);
			goto out;
		}
	}

	if (di->i_xattr_loc) {
		ret = ocfs2_xattr_free_block(inode,
					     le64_to_cpu(di->i_xattr_loc),
					     ref_ci, ref_root_bh);
		if (ret < 0) {
			mlog_errno(ret);
			goto out;
		}
	}

	handle = ocfs2_start_trans((OCFS2_SB(inode->i_sb)),
				   OCFS2_INODE_UPDATE_CREDITS);
	if (IS_ERR(handle)) {
		ret = PTR_ERR(handle);
		mlog_errno(ret);
		goto out;
	}
	ret = ocfs2_journal_access_di(handle, INODE_CACHE(inode), di_bh,
				      OCFS2_JOURNAL_ACCESS_WRITE);
	if (ret) {
		mlog_errno(ret);
		goto out_commit;
	}

	di->i_xattr_loc = 0;

	spin_lock(&oi->ip_lock);
	oi->ip_dyn_features &= ~(OCFS2_INLINE_XATTR_FL | OCFS2_HAS_XATTR_FL);
	di->i_dyn_features = cpu_to_le16(oi->ip_dyn_features);
	spin_unlock(&oi->ip_lock);

	ret = ocfs2_journal_dirty(handle, di_bh);
	if (ret < 0)
		mlog_errno(ret);
out_commit:
	ocfs2_commit_trans(OCFS2_SB(inode->i_sb), handle);
out:
	if (ref_tree)
		ocfs2_unlock_refcount_tree(OCFS2_SB(inode->i_sb), ref_tree, 1);
	brelse(ref_root_bh);
	return ret;
}

static int ocfs2_xattr_has_space_inline(struct inode *inode,
					struct ocfs2_dinode *di)
{
	struct ocfs2_inode_info *oi = OCFS2_I(inode);
	unsigned int xattrsize = OCFS2_SB(inode->i_sb)->s_xattr_inline_size;
	int free;

	if (xattrsize < OCFS2_MIN_XATTR_INLINE_SIZE)
		return 0;

	if (oi->ip_dyn_features & OCFS2_INLINE_DATA_FL) {
		struct ocfs2_inline_data *idata = &di->id2.i_data;
		free = le16_to_cpu(idata->id_count) - le64_to_cpu(di->i_size);
	} else if (ocfs2_inode_is_fast_symlink(inode)) {
		free = ocfs2_fast_symlink_chars(inode->i_sb) -
			le64_to_cpu(di->i_size);
	} else {
		struct ocfs2_extent_list *el = &di->id2.i_list;
		free = (le16_to_cpu(el->l_count) -
			le16_to_cpu(el->l_next_free_rec)) *
			sizeof(struct ocfs2_extent_rec);
	}
	if (free >= xattrsize)
		return 1;

	return 0;
}

/*
 * ocfs2_xattr_ibody_find()
 *
 * Find extended attribute in inode block and
 * fill search info into struct ocfs2_xattr_search.
 */
static int ocfs2_xattr_ibody_find(struct inode *inode,
				  int name_index,
				  const char *name,
				  struct ocfs2_xattr_search *xs)
{
	struct ocfs2_inode_info *oi = OCFS2_I(inode);
	struct ocfs2_dinode *di = (struct ocfs2_dinode *)xs->inode_bh->b_data;
	int ret;
	int has_space = 0;

	if (inode->i_sb->s_blocksize == OCFS2_MIN_BLOCKSIZE)
		return 0;

	if (!(oi->ip_dyn_features & OCFS2_INLINE_XATTR_FL)) {
		down_read(&oi->ip_alloc_sem);
		has_space = ocfs2_xattr_has_space_inline(inode, di);
		up_read(&oi->ip_alloc_sem);
		if (!has_space)
			return 0;
	}

	xs->xattr_bh = xs->inode_bh;
	xs->end = (void *)di + inode->i_sb->s_blocksize;
	if (oi->ip_dyn_features & OCFS2_INLINE_XATTR_FL)
		xs->header = (struct ocfs2_xattr_header *)
			(xs->end - le16_to_cpu(di->i_xattr_inline_size));
	else
		xs->header = (struct ocfs2_xattr_header *)
			(xs->end - OCFS2_SB(inode->i_sb)->s_xattr_inline_size);
	xs->base = (void *)xs->header;
	xs->here = xs->header->xh_entries;

	/* Find the named attribute. */
	if (oi->ip_dyn_features & OCFS2_INLINE_XATTR_FL) {
		ret = ocfs2_xattr_find_entry(name_index, name, xs);
		if (ret && ret != -ENODATA)
			return ret;
		xs->not_found = ret;
	}

	return 0;
}

/*
 * ocfs2_xattr_ibody_set()
 *
 * Set, replace or remove an extended attribute into inode block.
 *
 */
static int ocfs2_xattr_ibody_set(struct inode *inode,
				 struct ocfs2_xattr_info *xi,
				 struct ocfs2_xattr_search *xs,
				 struct ocfs2_xattr_set_ctxt *ctxt)
{
	struct ocfs2_inode_info *oi = OCFS2_I(inode);
	struct ocfs2_dinode *di = (struct ocfs2_dinode *)xs->inode_bh->b_data;
	int ret;

	if (inode->i_sb->s_blocksize == OCFS2_MIN_BLOCKSIZE)
		return -ENOSPC;

	down_write(&oi->ip_alloc_sem);
	if (!(oi->ip_dyn_features & OCFS2_INLINE_XATTR_FL)) {
		if (!ocfs2_xattr_has_space_inline(inode, di)) {
			ret = -ENOSPC;
			goto out;
		}
	}

	ret = ocfs2_xattr_set_entry(inode, xi, xs, ctxt,
				(OCFS2_INLINE_XATTR_FL | OCFS2_HAS_XATTR_FL));
out:
	up_write(&oi->ip_alloc_sem);

	return ret;
}

/*
 * ocfs2_xattr_block_find()
 *
 * Find extended attribute in external block and
 * fill search info into struct ocfs2_xattr_search.
 */
static int ocfs2_xattr_block_find(struct inode *inode,
				  int name_index,
				  const char *name,
				  struct ocfs2_xattr_search *xs)
{
	struct ocfs2_dinode *di = (struct ocfs2_dinode *)xs->inode_bh->b_data;
	struct buffer_head *blk_bh = NULL;
	struct ocfs2_xattr_block *xb;
	int ret = 0;

	if (!di->i_xattr_loc)
		return ret;

	ret = ocfs2_read_xattr_block(inode, le64_to_cpu(di->i_xattr_loc),
				     &blk_bh);
	if (ret < 0) {
		mlog_errno(ret);
		return ret;
	}

	xs->xattr_bh = blk_bh;
	xb = (struct ocfs2_xattr_block *)blk_bh->b_data;

	if (!(le16_to_cpu(xb->xb_flags) & OCFS2_XATTR_INDEXED)) {
		xs->header = &xb->xb_attrs.xb_header;
		xs->base = (void *)xs->header;
		xs->end = (void *)(blk_bh->b_data) + blk_bh->b_size;
		xs->here = xs->header->xh_entries;

		ret = ocfs2_xattr_find_entry(name_index, name, xs);
	} else
		ret = ocfs2_xattr_index_block_find(inode, blk_bh,
						   name_index,
						   name, xs);

	if (ret && ret != -ENODATA) {
		xs->xattr_bh = NULL;
		goto cleanup;
	}
	xs->not_found = ret;
	return 0;
cleanup:
	brelse(blk_bh);

	return ret;
}

static int ocfs2_create_xattr_block(handle_t *handle,
				    struct inode *inode,
				    struct buffer_head *inode_bh,
				    struct ocfs2_alloc_context *meta_ac,
				    struct buffer_head **ret_bh,
				    int indexed)
{
	int ret;
	u16 suballoc_bit_start;
	u32 num_got;
	u64 first_blkno;
	struct ocfs2_dinode *di =  (struct ocfs2_dinode *)inode_bh->b_data;
	struct ocfs2_super *osb = OCFS2_SB(inode->i_sb);
	struct buffer_head *new_bh = NULL;
	struct ocfs2_xattr_block *xblk;

	ret = ocfs2_journal_access_di(handle, INODE_CACHE(inode), inode_bh,
				      OCFS2_JOURNAL_ACCESS_CREATE);
	if (ret < 0) {
		mlog_errno(ret);
		goto end;
	}

	ret = ocfs2_claim_metadata(osb, handle, meta_ac, 1,
				   &suballoc_bit_start, &num_got,
				   &first_blkno);
	if (ret < 0) {
		mlog_errno(ret);
		goto end;
	}

	new_bh = sb_getblk(inode->i_sb, first_blkno);
	ocfs2_set_new_buffer_uptodate(INODE_CACHE(inode), new_bh);

	ret = ocfs2_journal_access_xb(handle, INODE_CACHE(inode),
				      new_bh,
				      OCFS2_JOURNAL_ACCESS_CREATE);
	if (ret < 0) {
		mlog_errno(ret);
		goto end;
	}

	/* Initialize ocfs2_xattr_block */
	xblk = (struct ocfs2_xattr_block *)new_bh->b_data;
	memset(xblk, 0, inode->i_sb->s_blocksize);
	strcpy((void *)xblk, OCFS2_XATTR_BLOCK_SIGNATURE);
	xblk->xb_suballoc_slot = cpu_to_le16(osb->slot_num);
	xblk->xb_suballoc_bit = cpu_to_le16(suballoc_bit_start);
	xblk->xb_fs_generation = cpu_to_le32(osb->fs_generation);
	xblk->xb_blkno = cpu_to_le64(first_blkno);

	if (indexed) {
		struct ocfs2_xattr_tree_root *xr = &xblk->xb_attrs.xb_root;
		xr->xt_clusters = cpu_to_le32(1);
		xr->xt_last_eb_blk = 0;
		xr->xt_list.l_tree_depth = 0;
		xr->xt_list.l_count = cpu_to_le16(
					ocfs2_xattr_recs_per_xb(inode->i_sb));
		xr->xt_list.l_next_free_rec = cpu_to_le16(1);
		xblk->xb_flags = cpu_to_le16(OCFS2_XATTR_INDEXED);
	}

	ret = ocfs2_journal_dirty(handle, new_bh);
	if (ret < 0) {
		mlog_errno(ret);
		goto end;
	}
	di->i_xattr_loc = cpu_to_le64(first_blkno);
	ocfs2_journal_dirty(handle, inode_bh);

	*ret_bh = new_bh;
	new_bh = NULL;

end:
	brelse(new_bh);
	return ret;
}

/*
 * ocfs2_xattr_block_set()
 *
 * Set, replace or remove an extended attribute into external block.
 *
 */
static int ocfs2_xattr_block_set(struct inode *inode,
				 struct ocfs2_xattr_info *xi,
				 struct ocfs2_xattr_search *xs,
				 struct ocfs2_xattr_set_ctxt *ctxt)
{
	struct buffer_head *new_bh = NULL;
	handle_t *handle = ctxt->handle;
	struct ocfs2_xattr_block *xblk = NULL;
	int ret;

	if (!xs->xattr_bh) {
		ret = ocfs2_create_xattr_block(handle, inode, xs->inode_bh,
					       ctxt->meta_ac, &new_bh, 0);
		if (ret) {
			mlog_errno(ret);
			goto end;
		}

		xs->xattr_bh = new_bh;
		xblk = (struct ocfs2_xattr_block *)xs->xattr_bh->b_data;
		xs->header = &xblk->xb_attrs.xb_header;
		xs->base = (void *)xs->header;
		xs->end = (void *)xblk + inode->i_sb->s_blocksize;
		xs->here = xs->header->xh_entries;
	} else
		xblk = (struct ocfs2_xattr_block *)xs->xattr_bh->b_data;

	if (!(le16_to_cpu(xblk->xb_flags) & OCFS2_XATTR_INDEXED)) {
		/* Set extended attribute into external block */
		ret = ocfs2_xattr_set_entry(inode, xi, xs, ctxt,
					    OCFS2_HAS_XATTR_FL);
		if (!ret || ret != -ENOSPC)
			goto end;

		ret = ocfs2_xattr_create_index_block(inode, xs, ctxt);
		if (ret)
			goto end;
	}

	ret = ocfs2_xattr_set_entry_index_block(inode, xi, xs, ctxt);

end:

	return ret;
}

/* Check whether the new xattr can be inserted into the inode. */
static int ocfs2_xattr_can_be_in_inode(struct inode *inode,
				       struct ocfs2_xattr_info *xi,
				       struct ocfs2_xattr_search *xs)
{
	u64 value_size;
	struct ocfs2_xattr_entry *last;
	int free, i;
	size_t min_offs = xs->end - xs->base;

	if (!xs->header)
		return 0;

	last = xs->header->xh_entries;

	for (i = 0; i < le16_to_cpu(xs->header->xh_count); i++) {
		size_t offs = le16_to_cpu(last->xe_name_offset);
		if (offs < min_offs)
			min_offs = offs;
		last += 1;
	}

	free = min_offs - ((void *)last - xs->base) - OCFS2_XATTR_HEADER_GAP;
	if (free < 0)
		return 0;

	BUG_ON(!xs->not_found);

	if (xi->value_len > OCFS2_XATTR_INLINE_SIZE)
		value_size = OCFS2_XATTR_ROOT_SIZE;
	else
		value_size = OCFS2_XATTR_SIZE(xi->value_len);

	if (free >= sizeof(struct ocfs2_xattr_entry) +
		   OCFS2_XATTR_SIZE(strlen(xi->name)) + value_size)
		return 1;

	return 0;
}

static int ocfs2_calc_xattr_set_need(struct inode *inode,
				     struct ocfs2_dinode *di,
				     struct ocfs2_xattr_info *xi,
				     struct ocfs2_xattr_search *xis,
				     struct ocfs2_xattr_search *xbs,
				     int *clusters_need,
				     int *meta_need,
				     int *credits_need)
{
	int ret = 0, old_in_xb = 0;
	int clusters_add = 0, meta_add = 0, credits = 0;
	struct buffer_head *bh = NULL;
	struct ocfs2_xattr_block *xb = NULL;
	struct ocfs2_xattr_entry *xe = NULL;
	struct ocfs2_xattr_value_root *xv = NULL;
	char *base = NULL;
	int name_offset, name_len = 0;
	u32 new_clusters = ocfs2_clusters_for_bytes(inode->i_sb,
						    xi->value_len);
	u64 value_size;

	/*
	 * Calculate the clusters we need to write.
	 * No matter whether we replace an old one or add a new one,
	 * we need this for writing.
	 */
	if (xi->value_len > OCFS2_XATTR_INLINE_SIZE)
		credits += new_clusters *
			   ocfs2_clusters_to_blocks(inode->i_sb, 1);

	if (xis->not_found && xbs->not_found) {
		credits += ocfs2_blocks_per_xattr_bucket(inode->i_sb);

		if (xi->value_len > OCFS2_XATTR_INLINE_SIZE) {
			clusters_add += new_clusters;
			credits += ocfs2_calc_extend_credits(inode->i_sb,
							&def_xv.xv.xr_list,
							new_clusters);
		}

		goto meta_guess;
	}

	if (!xis->not_found) {
		xe = xis->here;
		name_offset = le16_to_cpu(xe->xe_name_offset);
		name_len = OCFS2_XATTR_SIZE(xe->xe_name_len);
		base = xis->base;
		credits += OCFS2_INODE_UPDATE_CREDITS;
	} else {
		int i, block_off = 0;
		xb = (struct ocfs2_xattr_block *)xbs->xattr_bh->b_data;
		xe = xbs->here;
		name_offset = le16_to_cpu(xe->xe_name_offset);
		name_len = OCFS2_XATTR_SIZE(xe->xe_name_len);
		i = xbs->here - xbs->header->xh_entries;
		old_in_xb = 1;

		if (le16_to_cpu(xb->xb_flags) & OCFS2_XATTR_INDEXED) {
			ret = ocfs2_xattr_bucket_get_name_value(inode->i_sb,
							bucket_xh(xbs->bucket),
							i, &block_off,
							&name_offset);
			base = bucket_block(xbs->bucket, block_off);
			credits += ocfs2_blocks_per_xattr_bucket(inode->i_sb);
		} else {
			base = xbs->base;
			credits += OCFS2_XATTR_BLOCK_UPDATE_CREDITS;
		}
	}

	/*
	 * delete a xattr doesn't need metadata and cluster allocation.
	 * so just calculate the credits and return.
	 *
	 * The credits for removing the value tree will be extended
	 * by ocfs2_remove_extent itself.
	 */
	if (!xi->value) {
		if (!ocfs2_xattr_is_local(xe))
			credits += ocfs2_remove_extent_credits(inode->i_sb);

		goto out;
	}

	/* do cluster allocation guess first. */
	value_size = le64_to_cpu(xe->xe_value_size);

	if (old_in_xb) {
		/*
		 * In xattr set, we always try to set the xe in inode first,
		 * so if it can be inserted into inode successfully, the old
		 * one will be removed from the xattr block, and this xattr
		 * will be inserted into inode as a new xattr in inode.
		 */
		if (ocfs2_xattr_can_be_in_inode(inode, xi, xis)) {
			clusters_add += new_clusters;
			credits += ocfs2_remove_extent_credits(inode->i_sb) +
				    OCFS2_INODE_UPDATE_CREDITS;
			if (!ocfs2_xattr_is_local(xe))
				credits += ocfs2_calc_extend_credits(
							inode->i_sb,
							&def_xv.xv.xr_list,
							new_clusters);
			goto out;
		}
	}

	if (xi->value_len > OCFS2_XATTR_INLINE_SIZE) {
		/* the new values will be stored outside. */
		u32 old_clusters = 0;

		if (!ocfs2_xattr_is_local(xe)) {
			old_clusters =	ocfs2_clusters_for_bytes(inode->i_sb,
								 value_size);
			xv = (struct ocfs2_xattr_value_root *)
			     (base + name_offset + name_len);
			value_size = OCFS2_XATTR_ROOT_SIZE;
		} else
			xv = &def_xv.xv;

		if (old_clusters >= new_clusters) {
			credits += ocfs2_remove_extent_credits(inode->i_sb);
			goto out;
		} else {
			meta_add += ocfs2_extend_meta_needed(&xv->xr_list);
			clusters_add += new_clusters - old_clusters;
			credits += ocfs2_calc_extend_credits(inode->i_sb,
							     &xv->xr_list,
							     new_clusters -
							     old_clusters);
			if (value_size >= OCFS2_XATTR_ROOT_SIZE)
				goto out;
		}
	} else {
		/*
		 * Now the new value will be stored inside. So if the new
		 * value is smaller than the size of value root or the old
		 * value, we don't need any allocation, otherwise we have
		 * to guess metadata allocation.
		 */
		if ((ocfs2_xattr_is_local(xe) && value_size >= xi->value_len) ||
		    (!ocfs2_xattr_is_local(xe) &&
		     OCFS2_XATTR_ROOT_SIZE >= xi->value_len))
			goto out;
	}

meta_guess:
	/* calculate metadata allocation. */
	if (di->i_xattr_loc) {
		if (!xbs->xattr_bh) {
			ret = ocfs2_read_xattr_block(inode,
						     le64_to_cpu(di->i_xattr_loc),
						     &bh);
			if (ret) {
				mlog_errno(ret);
				goto out;
			}

			xb = (struct ocfs2_xattr_block *)bh->b_data;
		} else
			xb = (struct ocfs2_xattr_block *)xbs->xattr_bh->b_data;

		/*
		 * If there is already an xattr tree, good, we can calculate
		 * like other b-trees. Otherwise we may have the chance of
		 * create a tree, the credit calculation is borrowed from
		 * ocfs2_calc_extend_credits with root_el = NULL. And the
		 * new tree will be cluster based, so no meta is needed.
		 */
		if (le16_to_cpu(xb->xb_flags) & OCFS2_XATTR_INDEXED) {
			struct ocfs2_extent_list *el =
				 &xb->xb_attrs.xb_root.xt_list;
			meta_add += ocfs2_extend_meta_needed(el);
			credits += ocfs2_calc_extend_credits(inode->i_sb,
							     el, 1);
		} else
			credits += OCFS2_SUBALLOC_ALLOC + 1;

		/*
		 * This cluster will be used either for new bucket or for
		 * new xattr block.
		 * If the cluster size is the same as the bucket size, one
		 * more is needed since we may need to extend the bucket
		 * also.
		 */
		clusters_add += 1;
		credits += ocfs2_blocks_per_xattr_bucket(inode->i_sb);
		if (OCFS2_XATTR_BUCKET_SIZE ==
			OCFS2_SB(inode->i_sb)->s_clustersize) {
			credits += ocfs2_blocks_per_xattr_bucket(inode->i_sb);
			clusters_add += 1;
		}
	} else {
		meta_add += 1;
		credits += OCFS2_XATTR_BLOCK_CREATE_CREDITS;
	}
out:
	if (clusters_need)
		*clusters_need = clusters_add;
	if (meta_need)
		*meta_need = meta_add;
	if (credits_need)
		*credits_need = credits;
	brelse(bh);
	return ret;
}

static int ocfs2_init_xattr_set_ctxt(struct inode *inode,
				     struct ocfs2_dinode *di,
				     struct ocfs2_xattr_info *xi,
				     struct ocfs2_xattr_search *xis,
				     struct ocfs2_xattr_search *xbs,
				     struct ocfs2_xattr_set_ctxt *ctxt,
				     int extra_meta,
				     int *credits)
{
	int clusters_add, meta_add, ret;
	struct ocfs2_super *osb = OCFS2_SB(inode->i_sb);

	memset(ctxt, 0, sizeof(struct ocfs2_xattr_set_ctxt));

	ocfs2_init_dealloc_ctxt(&ctxt->dealloc);

	ret = ocfs2_calc_xattr_set_need(inode, di, xi, xis, xbs,
					&clusters_add, &meta_add, credits);
	if (ret) {
		mlog_errno(ret);
		return ret;
	}

	meta_add += extra_meta;
	mlog(0, "Set xattr %s, reserve meta blocks = %d, clusters = %d, "
	     "credits = %d\n", xi->name, meta_add, clusters_add, *credits);

	if (meta_add) {
		ret = ocfs2_reserve_new_metadata_blocks(osb, meta_add,
							&ctxt->meta_ac);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}
	}

	if (clusters_add) {
		ret = ocfs2_reserve_clusters(osb, clusters_add, &ctxt->data_ac);
		if (ret)
			mlog_errno(ret);
	}
out:
	if (ret) {
		if (ctxt->meta_ac) {
			ocfs2_free_alloc_context(ctxt->meta_ac);
			ctxt->meta_ac = NULL;
		}

		/*
		 * We cannot have an error and a non null ctxt->data_ac.
		 */
	}

	return ret;
}

static int __ocfs2_xattr_set_handle(struct inode *inode,
				    struct ocfs2_dinode *di,
				    struct ocfs2_xattr_info *xi,
				    struct ocfs2_xattr_search *xis,
				    struct ocfs2_xattr_search *xbs,
				    struct ocfs2_xattr_set_ctxt *ctxt)
{
	int ret = 0, credits, old_found;

	if (!xi->value) {
		/* Remove existing extended attribute */
		if (!xis->not_found)
			ret = ocfs2_xattr_ibody_set(inode, xi, xis, ctxt);
		else if (!xbs->not_found)
			ret = ocfs2_xattr_block_set(inode, xi, xbs, ctxt);
	} else {
		/* We always try to set extended attribute into inode first*/
		ret = ocfs2_xattr_ibody_set(inode, xi, xis, ctxt);
		if (!ret && !xbs->not_found) {
			/*
			 * If succeed and that extended attribute existing in
			 * external block, then we will remove it.
			 */
			xi->value = NULL;
			xi->value_len = 0;

			old_found = xis->not_found;
			xis->not_found = -ENODATA;
			ret = ocfs2_calc_xattr_set_need(inode,
							di,
							xi,
							xis,
							xbs,
							NULL,
							NULL,
							&credits);
			xis->not_found = old_found;
			if (ret) {
				mlog_errno(ret);
				goto out;
			}

			ret = ocfs2_extend_trans(ctxt->handle, credits +
					ctxt->handle->h_buffer_credits);
			if (ret) {
				mlog_errno(ret);
				goto out;
			}
			ret = ocfs2_xattr_block_set(inode, xi, xbs, ctxt);
		} else if (ret == -ENOSPC) {
			if (di->i_xattr_loc && !xbs->xattr_bh) {
				ret = ocfs2_xattr_block_find(inode,
							     xi->name_index,
							     xi->name, xbs);
				if (ret)
					goto out;

				old_found = xis->not_found;
				xis->not_found = -ENODATA;
				ret = ocfs2_calc_xattr_set_need(inode,
								di,
								xi,
								xis,
								xbs,
								NULL,
								NULL,
								&credits);
				xis->not_found = old_found;
				if (ret) {
					mlog_errno(ret);
					goto out;
				}

				ret = ocfs2_extend_trans(ctxt->handle, credits +
					ctxt->handle->h_buffer_credits);
				if (ret) {
					mlog_errno(ret);
					goto out;
				}
			}
			/*
			 * If no space in inode, we will set extended attribute
			 * into external block.
			 */
			ret = ocfs2_xattr_block_set(inode, xi, xbs, ctxt);
			if (ret)
				goto out;
			if (!xis->not_found) {
				/*
				 * If succeed and that extended attribute
				 * existing in inode, we will remove it.
				 */
				xi->value = NULL;
				xi->value_len = 0;
				xbs->not_found = -ENODATA;
				ret = ocfs2_calc_xattr_set_need(inode,
								di,
								xi,
								xis,
								xbs,
								NULL,
								NULL,
								&credits);
				if (ret) {
					mlog_errno(ret);
					goto out;
				}

				ret = ocfs2_extend_trans(ctxt->handle, credits +
						ctxt->handle->h_buffer_credits);
				if (ret) {
					mlog_errno(ret);
					goto out;
				}
				ret = ocfs2_xattr_ibody_set(inode, xi,
							    xis, ctxt);
			}
		}
	}

	if (!ret) {
		/* Update inode ctime. */
		ret = ocfs2_journal_access_di(ctxt->handle, INODE_CACHE(inode),
					      xis->inode_bh,
					      OCFS2_JOURNAL_ACCESS_WRITE);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}

		inode->i_ctime = CURRENT_TIME;
		di->i_ctime = cpu_to_le64(inode->i_ctime.tv_sec);
		di->i_ctime_nsec = cpu_to_le32(inode->i_ctime.tv_nsec);
		ocfs2_journal_dirty(ctxt->handle, xis->inode_bh);
	}
out:
	return ret;
}

/*
 * This function only called duing creating inode
 * for init security/acl xattrs of the new inode.
 * All transanction credits have been reserved in mknod.
 */
int ocfs2_xattr_set_handle(handle_t *handle,
			   struct inode *inode,
			   struct buffer_head *di_bh,
			   int name_index,
			   const char *name,
			   const void *value,
			   size_t value_len,
			   int flags,
			   struct ocfs2_alloc_context *meta_ac,
			   struct ocfs2_alloc_context *data_ac)
{
	struct ocfs2_dinode *di;
	int ret;

	struct ocfs2_xattr_info xi = {
		.name_index = name_index,
		.name = name,
		.value = value,
		.value_len = value_len,
	};

	struct ocfs2_xattr_search xis = {
		.not_found = -ENODATA,
	};

	struct ocfs2_xattr_search xbs = {
		.not_found = -ENODATA,
	};

	struct ocfs2_xattr_set_ctxt ctxt = {
		.handle = handle,
		.meta_ac = meta_ac,
		.data_ac = data_ac,
	};

	if (!ocfs2_supports_xattr(OCFS2_SB(inode->i_sb)))
		return -EOPNOTSUPP;

	/*
	 * In extreme situation, may need xattr bucket when
	 * block size is too small. And we have already reserved
	 * the credits for bucket in mknod.
	 */
	if (inode->i_sb->s_blocksize == OCFS2_MIN_BLOCKSIZE) {
		xbs.bucket = ocfs2_xattr_bucket_new(inode);
		if (!xbs.bucket) {
			mlog_errno(-ENOMEM);
			return -ENOMEM;
		}
	}

	xis.inode_bh = xbs.inode_bh = di_bh;
	di = (struct ocfs2_dinode *)di_bh->b_data;

	down_write(&OCFS2_I(inode)->ip_xattr_sem);

	ret = ocfs2_xattr_ibody_find(inode, name_index, name, &xis);
	if (ret)
		goto cleanup;
	if (xis.not_found) {
		ret = ocfs2_xattr_block_find(inode, name_index, name, &xbs);
		if (ret)
			goto cleanup;
	}

	ret = __ocfs2_xattr_set_handle(inode, di, &xi, &xis, &xbs, &ctxt);

cleanup:
	up_write(&OCFS2_I(inode)->ip_xattr_sem);
	brelse(xbs.xattr_bh);
	ocfs2_xattr_bucket_free(xbs.bucket);

	return ret;
}

/*
 * ocfs2_xattr_set()
 *
 * Set, replace or remove an extended attribute for this inode.
 * value is NULL to remove an existing extended attribute, else either
 * create or replace an extended attribute.
 */
int ocfs2_xattr_set(struct inode *inode,
		    int name_index,
		    const char *name,
		    const void *value,
		    size_t value_len,
		    int flags)
{
	struct buffer_head *di_bh = NULL;
	struct ocfs2_dinode *di;
	int ret, credits, ref_meta = 0, ref_credits = 0;
	struct ocfs2_super *osb = OCFS2_SB(inode->i_sb);
	struct inode *tl_inode = osb->osb_tl_inode;
	struct ocfs2_xattr_set_ctxt ctxt = { NULL, NULL, };
	struct ocfs2_refcount_tree *ref_tree = NULL;

	struct ocfs2_xattr_info xi = {
		.name_index = name_index,
		.name = name,
		.value = value,
		.value_len = value_len,
	};

	struct ocfs2_xattr_search xis = {
		.not_found = -ENODATA,
	};

	struct ocfs2_xattr_search xbs = {
		.not_found = -ENODATA,
	};

	if (!ocfs2_supports_xattr(OCFS2_SB(inode->i_sb)))
		return -EOPNOTSUPP;

	/*
	 * Only xbs will be used on indexed trees.  xis doesn't need a
	 * bucket.
	 */
	xbs.bucket = ocfs2_xattr_bucket_new(inode);
	if (!xbs.bucket) {
		mlog_errno(-ENOMEM);
		return -ENOMEM;
	}

	ret = ocfs2_inode_lock(inode, &di_bh, 1);
	if (ret < 0) {
		mlog_errno(ret);
		goto cleanup_nolock;
	}
	xis.inode_bh = xbs.inode_bh = di_bh;
	di = (struct ocfs2_dinode *)di_bh->b_data;

	down_write(&OCFS2_I(inode)->ip_xattr_sem);
	/*
	 * Scan inode and external block to find the same name
	 * extended attribute and collect search infomation.
	 */
	ret = ocfs2_xattr_ibody_find(inode, name_index, name, &xis);
	if (ret)
		goto cleanup;
	if (xis.not_found) {
		ret = ocfs2_xattr_block_find(inode, name_index, name, &xbs);
		if (ret)
			goto cleanup;
	}

	if (xis.not_found && xbs.not_found) {
		ret = -ENODATA;
		if (flags & XATTR_REPLACE)
			goto cleanup;
		ret = 0;
		if (!value)
			goto cleanup;
	} else {
		ret = -EEXIST;
		if (flags & XATTR_CREATE)
			goto cleanup;
	}

	/* Check whether the value is refcounted and do some prepartion. */
	if (OCFS2_I(inode)->ip_dyn_features & OCFS2_HAS_REFCOUNT_FL &&
	    (!xis.not_found || !xbs.not_found)) {
		ret = ocfs2_prepare_refcount_xattr(inode, di, &xi,
						   &xis, &xbs, &ref_tree,
						   &ref_meta, &ref_credits);
		if (ret) {
			mlog_errno(ret);
			goto cleanup;
		}
	}

	mutex_lock(&tl_inode->i_mutex);

	if (ocfs2_truncate_log_needs_flush(osb)) {
		ret = __ocfs2_flush_truncate_log(osb);
		if (ret < 0) {
			mutex_unlock(&tl_inode->i_mutex);
			mlog_errno(ret);
			goto cleanup;
		}
	}
	mutex_unlock(&tl_inode->i_mutex);

	ret = ocfs2_init_xattr_set_ctxt(inode, di, &xi, &xis,
					&xbs, &ctxt, ref_meta, &credits);
	if (ret) {
		mlog_errno(ret);
		goto cleanup;
	}

	/* we need to update inode's ctime field, so add credit for it. */
	credits += OCFS2_INODE_UPDATE_CREDITS;
	ctxt.handle = ocfs2_start_trans(osb, credits + ref_credits);
	if (IS_ERR(ctxt.handle)) {
		ret = PTR_ERR(ctxt.handle);
		mlog_errno(ret);
		goto cleanup;
	}

	ret = __ocfs2_xattr_set_handle(inode, di, &xi, &xis, &xbs, &ctxt);

	ocfs2_commit_trans(osb, ctxt.handle);

	if (ctxt.data_ac)
		ocfs2_free_alloc_context(ctxt.data_ac);
	if (ctxt.meta_ac)
		ocfs2_free_alloc_context(ctxt.meta_ac);
	if (ocfs2_dealloc_has_cluster(&ctxt.dealloc))
		ocfs2_schedule_truncate_log_flush(osb, 1);
	ocfs2_run_deallocs(osb, &ctxt.dealloc);

cleanup:
	if (ref_tree)
		ocfs2_unlock_refcount_tree(osb, ref_tree, 1);
	up_write(&OCFS2_I(inode)->ip_xattr_sem);
	if (!value && !ret) {
		ret = ocfs2_try_remove_refcount_tree(inode, di_bh);
		if (ret)
			mlog_errno(ret);
	}
	ocfs2_inode_unlock(inode, 1);
cleanup_nolock:
	brelse(di_bh);
	brelse(xbs.xattr_bh);
	ocfs2_xattr_bucket_free(xbs.bucket);

	return ret;
}

/*
 * Find the xattr extent rec which may contains name_hash.
 * e_cpos will be the first name hash of the xattr rec.
 * el must be the ocfs2_xattr_header.xb_attrs.xb_root.xt_list.
 */
static int ocfs2_xattr_get_rec(struct inode *inode,
			       u32 name_hash,
			       u64 *p_blkno,
			       u32 *e_cpos,
			       u32 *num_clusters,
			       struct ocfs2_extent_list *el)
{
	int ret = 0, i;
	struct buffer_head *eb_bh = NULL;
	struct ocfs2_extent_block *eb;
	struct ocfs2_extent_rec *rec = NULL;
	u64 e_blkno = 0;

	if (el->l_tree_depth) {
		ret = ocfs2_find_leaf(INODE_CACHE(inode), el, name_hash,
				      &eb_bh);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}

		eb = (struct ocfs2_extent_block *) eb_bh->b_data;
		el = &eb->h_list;

		if (el->l_tree_depth) {
			ocfs2_error(inode->i_sb,
				    "Inode %lu has non zero tree depth in "
				    "xattr tree block %llu\n", inode->i_ino,
				    (unsigned long long)eb_bh->b_blocknr);
			ret = -EROFS;
			goto out;
		}
	}

	for (i = le16_to_cpu(el->l_next_free_rec) - 1; i >= 0; i--) {
		rec = &el->l_recs[i];

		if (le32_to_cpu(rec->e_cpos) <= name_hash) {
			e_blkno = le64_to_cpu(rec->e_blkno);
			break;
		}
	}

	if (!e_blkno) {
		ocfs2_error(inode->i_sb, "Inode %lu has bad extent "
			    "record (%u, %u, 0) in xattr", inode->i_ino,
			    le32_to_cpu(rec->e_cpos),
			    ocfs2_rec_clusters(el, rec));
		ret = -EROFS;
		goto out;
	}

	*p_blkno = le64_to_cpu(rec->e_blkno);
	*num_clusters = le16_to_cpu(rec->e_leaf_clusters);
	if (e_cpos)
		*e_cpos = le32_to_cpu(rec->e_cpos);
out:
	brelse(eb_bh);
	return ret;
}

typedef int (xattr_bucket_func)(struct inode *inode,
				struct ocfs2_xattr_bucket *bucket,
				void *para);

static int ocfs2_find_xe_in_bucket(struct inode *inode,
				   struct ocfs2_xattr_bucket *bucket,
				   int name_index,
				   const char *name,
				   u32 name_hash,
				   u16 *xe_index,
				   int *found)
{
	int i, ret = 0, cmp = 1, block_off, new_offset;
	struct ocfs2_xattr_header *xh = bucket_xh(bucket);
	size_t name_len = strlen(name);
	struct ocfs2_xattr_entry *xe = NULL;
	char *xe_name;

	/*
	 * We don't use binary search in the bucket because there
	 * may be multiple entries with the same name hash.
	 */
	for (i = 0; i < le16_to_cpu(xh->xh_count); i++) {
		xe = &xh->xh_entries[i];

		if (name_hash > le32_to_cpu(xe->xe_name_hash))
			continue;
		else if (name_hash < le32_to_cpu(xe->xe_name_hash))
			break;

		cmp = name_index - ocfs2_xattr_get_type(xe);
		if (!cmp)
			cmp = name_len - xe->xe_name_len;
		if (cmp)
			continue;

		ret = ocfs2_xattr_bucket_get_name_value(inode->i_sb,
							xh,
							i,
							&block_off,
							&new_offset);
		if (ret) {
			mlog_errno(ret);
			break;
		}


		xe_name = bucket_block(bucket, block_off) + new_offset;
		if (!memcmp(name, xe_name, name_len)) {
			*xe_index = i;
			*found = 1;
			ret = 0;
			break;
		}
	}

	return ret;
}

/*
 * Find the specified xattr entry in a series of buckets.
 * This series start from p_blkno and last for num_clusters.
 * The ocfs2_xattr_header.xh_num_buckets of the first bucket contains
 * the num of the valid buckets.
 *
 * Return the buffer_head this xattr should reside in. And if the xattr's
 * hash is in the gap of 2 buckets, return the lower bucket.
 */
static int ocfs2_xattr_bucket_find(struct inode *inode,
				   int name_index,
				   const char *name,
				   u32 name_hash,
				   u64 p_blkno,
				   u32 first_hash,
				   u32 num_clusters,
				   struct ocfs2_xattr_search *xs)
{
	int ret, found = 0;
	struct ocfs2_xattr_header *xh = NULL;
	struct ocfs2_xattr_entry *xe = NULL;
	u16 index = 0;
	u16 blk_per_bucket = ocfs2_blocks_per_xattr_bucket(inode->i_sb);
	int low_bucket = 0, bucket, high_bucket;
	struct ocfs2_xattr_bucket *search;
	u32 last_hash;
	u64 blkno, lower_blkno = 0;

	search = ocfs2_xattr_bucket_new(inode);
	if (!search) {
		ret = -ENOMEM;
		mlog_errno(ret);
		goto out;
	}

	ret = ocfs2_read_xattr_bucket(search, p_blkno);
	if (ret) {
		mlog_errno(ret);
		goto out;
	}

	xh = bucket_xh(search);
	high_bucket = le16_to_cpu(xh->xh_num_buckets) - 1;
	while (low_bucket <= high_bucket) {
		ocfs2_xattr_bucket_relse(search);

		bucket = (low_bucket + high_bucket) / 2;
		blkno = p_blkno + bucket * blk_per_bucket;
		ret = ocfs2_read_xattr_bucket(search, blkno);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}

		xh = bucket_xh(search);
		xe = &xh->xh_entries[0];
		if (name_hash < le32_to_cpu(xe->xe_name_hash)) {
			high_bucket = bucket - 1;
			continue;
		}

		/*
		 * Check whether the hash of the last entry in our
		 * bucket is larger than the search one. for an empty
		 * bucket, the last one is also the first one.
		 */
		if (xh->xh_count)
			xe = &xh->xh_entries[le16_to_cpu(xh->xh_count) - 1];

		last_hash = le32_to_cpu(xe->xe_name_hash);

		/* record lower_blkno which may be the insert place. */
		lower_blkno = blkno;

		if (name_hash > le32_to_cpu(xe->xe_name_hash)) {
			low_bucket = bucket + 1;
			continue;
		}

		/* the searched xattr should reside in this bucket if exists. */
		ret = ocfs2_find_xe_in_bucket(inode, search,
					      name_index, name, name_hash,
					      &index, &found);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}
		break;
	}

	/*
	 * Record the bucket we have found.
	 * When the xattr's hash value is in the gap of 2 buckets, we will
	 * always set it to the previous bucket.
	 */
	if (!lower_blkno)
		lower_blkno = p_blkno;

	/* This should be in cache - we just read it during the search */
	ret = ocfs2_read_xattr_bucket(xs->bucket, lower_blkno);
	if (ret) {
		mlog_errno(ret);
		goto out;
	}

	xs->header = bucket_xh(xs->bucket);
	xs->base = bucket_block(xs->bucket, 0);
	xs->end = xs->base + inode->i_sb->s_blocksize;

	if (found) {
		xs->here = &xs->header->xh_entries[index];
		mlog(0, "find xattr %s in bucket %llu, entry = %u\n", name,
		     (unsigned long long)bucket_blkno(xs->bucket), index);
	} else
		ret = -ENODATA;

out:
	ocfs2_xattr_bucket_free(search);
	return ret;
}

static int ocfs2_xattr_index_block_find(struct inode *inode,
					struct buffer_head *root_bh,
					int name_index,
					const char *name,
					struct ocfs2_xattr_search *xs)
{
	int ret;
	struct ocfs2_xattr_block *xb =
			(struct ocfs2_xattr_block *)root_bh->b_data;
	struct ocfs2_xattr_tree_root *xb_root = &xb->xb_attrs.xb_root;
	struct ocfs2_extent_list *el = &xb_root->xt_list;
	u64 p_blkno = 0;
	u32 first_hash, num_clusters = 0;
	u32 name_hash = ocfs2_xattr_name_hash(inode, name, strlen(name));

	if (le16_to_cpu(el->l_next_free_rec) == 0)
		return -ENODATA;

	mlog(0, "find xattr %s, hash = %u, index = %d in xattr tree\n",
	     name, name_hash, name_index);

	ret = ocfs2_xattr_get_rec(inode, name_hash, &p_blkno, &first_hash,
				  &num_clusters, el);
	if (ret) {
		mlog_errno(ret);
		goto out;
	}

	BUG_ON(p_blkno == 0 || num_clusters == 0 || first_hash > name_hash);

	mlog(0, "find xattr extent rec %u clusters from %llu, the first hash "
	     "in the rec is %u\n", num_clusters, (unsigned long long)p_blkno,
	     first_hash);

	ret = ocfs2_xattr_bucket_find(inode, name_index, name, name_hash,
				      p_blkno, first_hash, num_clusters, xs);

out:
	return ret;
}

static int ocfs2_iterate_xattr_buckets(struct inode *inode,
				       u64 blkno,
				       u32 clusters,
				       xattr_bucket_func *func,
				       void *para)
{
	int i, ret = 0;
	u32 bpc = ocfs2_xattr_buckets_per_cluster(OCFS2_SB(inode->i_sb));
	u32 num_buckets = clusters * bpc;
	struct ocfs2_xattr_bucket *bucket;

	bucket = ocfs2_xattr_bucket_new(inode);
	if (!bucket) {
		mlog_errno(-ENOMEM);
		return -ENOMEM;
	}

	mlog(0, "iterating xattr buckets in %u clusters starting from %llu\n",
	     clusters, (unsigned long long)blkno);

	for (i = 0; i < num_buckets; i++, blkno += bucket->bu_blocks) {
		ret = ocfs2_read_xattr_bucket(bucket, blkno);
		if (ret) {
			mlog_errno(ret);
			break;
		}

		/*
		 * The real bucket num in this series of blocks is stored
		 * in the 1st bucket.
		 */
		if (i == 0)
			num_buckets = le16_to_cpu(bucket_xh(bucket)->xh_num_buckets);

		mlog(0, "iterating xattr bucket %llu, first hash %u\n",
		     (unsigned long long)blkno,
		     le32_to_cpu(bucket_xh(bucket)->xh_entries[0].xe_name_hash));
		if (func) {
			ret = func(inode, bucket, para);
			if (ret && ret != -ERANGE)
				mlog_errno(ret);
			/* Fall through to bucket_relse() */
		}

		ocfs2_xattr_bucket_relse(bucket);
		if (ret)
			break;
	}

	ocfs2_xattr_bucket_free(bucket);
	return ret;
}

struct ocfs2_xattr_tree_list {
	char *buffer;
	size_t buffer_size;
	size_t result;
};

static int ocfs2_xattr_bucket_get_name_value(struct super_block *sb,
					     struct ocfs2_xattr_header *xh,
					     int index,
					     int *block_off,
					     int *new_offset)
{
	u16 name_offset;

	if (index < 0 || index >= le16_to_cpu(xh->xh_count))
		return -EINVAL;

	name_offset = le16_to_cpu(xh->xh_entries[index].xe_name_offset);

	*block_off = name_offset >> sb->s_blocksize_bits;
	*new_offset = name_offset % sb->s_blocksize;

	return 0;
}

static int ocfs2_list_xattr_bucket(struct inode *inode,
				   struct ocfs2_xattr_bucket *bucket,
				   void *para)
{
	int ret = 0, type;
	struct ocfs2_xattr_tree_list *xl = (struct ocfs2_xattr_tree_list *)para;
	int i, block_off, new_offset;
	const char *prefix, *name;

	for (i = 0 ; i < le16_to_cpu(bucket_xh(bucket)->xh_count); i++) {
		struct ocfs2_xattr_entry *entry = &bucket_xh(bucket)->xh_entries[i];
		type = ocfs2_xattr_get_type(entry);
		prefix = ocfs2_xattr_prefix(type);

		if (prefix) {
			ret = ocfs2_xattr_bucket_get_name_value(inode->i_sb,
								bucket_xh(bucket),
								i,
								&block_off,
								&new_offset);
			if (ret)
				break;

			name = (const char *)bucket_block(bucket, block_off) +
				new_offset;
			ret = ocfs2_xattr_list_entry(xl->buffer,
						     xl->buffer_size,
						     &xl->result,
						     prefix, name,
						     entry->xe_name_len);
			if (ret)
				break;
		}
	}

	return ret;
}

static int ocfs2_iterate_xattr_index_block(struct inode *inode,
					   struct buffer_head *blk_bh,
					   xattr_tree_rec_func *rec_func,
					   void *para)
{
	struct ocfs2_xattr_block *xb =
			(struct ocfs2_xattr_block *)blk_bh->b_data;
	struct ocfs2_extent_list *el = &xb->xb_attrs.xb_root.xt_list;
	int ret = 0;
	u32 name_hash = UINT_MAX, e_cpos = 0, num_clusters = 0;
	u64 p_blkno = 0;

	if (!el->l_next_free_rec || !rec_func)
		return 0;

	while (name_hash > 0) {
		ret = ocfs2_xattr_get_rec(inode, name_hash, &p_blkno,
					  &e_cpos, &num_clusters, el);
		if (ret) {
			mlog_errno(ret);
			break;
		}

		ret = rec_func(inode, blk_bh, p_blkno, e_cpos,
			       num_clusters, para);
		if (ret) {
			if (ret != -ERANGE)
				mlog_errno(ret);
			break;
		}

		if (e_cpos == 0)
			break;

		name_hash = e_cpos - 1;
	}

	return ret;

}

static int ocfs2_list_xattr_tree_rec(struct inode *inode,
				     struct buffer_head *root_bh,
				     u64 blkno, u32 cpos, u32 len, void *para)
{
	return ocfs2_iterate_xattr_buckets(inode, blkno, len,
					   ocfs2_list_xattr_bucket, para);
}

static int ocfs2_xattr_tree_list_index_block(struct inode *inode,
					     struct buffer_head *blk_bh,
					     char *buffer,
					     size_t buffer_size)
{
	int ret;
	struct ocfs2_xattr_tree_list xl = {
		.buffer = buffer,
		.buffer_size = buffer_size,
		.result = 0,
	};

	ret = ocfs2_iterate_xattr_index_block(inode, blk_bh,
					      ocfs2_list_xattr_tree_rec, &xl);
	if (ret) {
		mlog_errno(ret);
		goto out;
	}

	ret = xl.result;
out:
	return ret;
}

static int cmp_xe(const void *a, const void *b)
{
	const struct ocfs2_xattr_entry *l = a, *r = b;
	u32 l_hash = le32_to_cpu(l->xe_name_hash);
	u32 r_hash = le32_to_cpu(r->xe_name_hash);

	if (l_hash > r_hash)
		return 1;
	if (l_hash < r_hash)
		return -1;
	return 0;
}

static void swap_xe(void *a, void *b, int size)
{
	struct ocfs2_xattr_entry *l = a, *r = b, tmp;

	tmp = *l;
	memcpy(l, r, sizeof(struct ocfs2_xattr_entry));
	memcpy(r, &tmp, sizeof(struct ocfs2_xattr_entry));
}

/*
 * When the ocfs2_xattr_block is filled up, new bucket will be created
 * and all the xattr entries will be moved to the new bucket.
 * The header goes at the start of the bucket, and the names+values are
 * filled from the end.  This is why *target starts as the last buffer.
 * Note: we need to sort the entries since they are not saved in order
 * in the ocfs2_xattr_block.
 */
static void ocfs2_cp_xattr_block_to_bucket(struct inode *inode,
					   struct buffer_head *xb_bh,
					   struct ocfs2_xattr_bucket *bucket)
{
	int i, blocksize = inode->i_sb->s_blocksize;
	int blks = ocfs2_blocks_per_xattr_bucket(inode->i_sb);
	u16 offset, size, off_change;
	struct ocfs2_xattr_entry *xe;
	struct ocfs2_xattr_block *xb =
				(struct ocfs2_xattr_block *)xb_bh->b_data;
	struct ocfs2_xattr_header *xb_xh = &xb->xb_attrs.xb_header;
	struct ocfs2_xattr_header *xh = bucket_xh(bucket);
	u16 count = le16_to_cpu(xb_xh->xh_count);
	char *src = xb_bh->b_data;
	char *target = bucket_block(bucket, blks - 1);

	mlog(0, "cp xattr from block %llu to bucket %llu\n",
	     (unsigned long long)xb_bh->b_blocknr,
	     (unsigned long long)bucket_blkno(bucket));

	for (i = 0; i < blks; i++)
		memset(bucket_block(bucket, i), 0, blocksize);

	/*
	 * Since the xe_name_offset is based on ocfs2_xattr_header,
	 * there is a offset change corresponding to the change of
	 * ocfs2_xattr_header's position.
	 */
	off_change = offsetof(struct ocfs2_xattr_block, xb_attrs.xb_header);
	xe = &xb_xh->xh_entries[count - 1];
	offset = le16_to_cpu(xe->xe_name_offset) + off_change;
	size = blocksize - offset;

	/* copy all the names and values. */
	memcpy(target + offset, src + offset, size);

	/* Init new header now. */
	xh->xh_count = xb_xh->xh_count;
	xh->xh_num_buckets = cpu_to_le16(1);
	xh->xh_name_value_len = cpu_to_le16(size);
	xh->xh_free_start = cpu_to_le16(OCFS2_XATTR_BUCKET_SIZE - size);

	/* copy all the entries. */
	target = bucket_block(bucket, 0);
	offset = offsetof(struct ocfs2_xattr_header, xh_entries);
	size = count * sizeof(struct ocfs2_xattr_entry);
	memcpy(target + offset, (char *)xb_xh + offset, size);

	/* Change the xe offset for all the xe because of the move. */
	off_change = OCFS2_XATTR_BUCKET_SIZE - blocksize +
		 offsetof(struct ocfs2_xattr_block, xb_attrs.xb_header);
	for (i = 0; i < count; i++)
		le16_add_cpu(&xh->xh_entries[i].xe_name_offset, off_change);

	mlog(0, "copy entry: start = %u, size = %u, offset_change = %u\n",
	     offset, size, off_change);

	sort(target + offset, count, sizeof(struct ocfs2_xattr_entry),
	     cmp_xe, swap_xe);
}

/*
 * After we move xattr from block to index btree, we have to
 * update ocfs2_xattr_search to the new xe and base.
 *
 * When the entry is in xattr block, xattr_bh indicates the storage place.
 * While if the entry is in index b-tree, "bucket" indicates the
 * real place of the xattr.
 */
static void ocfs2_xattr_update_xattr_search(struct inode *inode,
					    struct ocfs2_xattr_search *xs,
					    struct buffer_head *old_bh)
{
	char *buf = old_bh->b_data;
	struct ocfs2_xattr_block *old_xb = (struct ocfs2_xattr_block *)buf;
	struct ocfs2_xattr_header *old_xh = &old_xb->xb_attrs.xb_header;
	int i;

	xs->header = bucket_xh(xs->bucket);
	xs->base = bucket_block(xs->bucket, 0);
	xs->end = xs->base + inode->i_sb->s_blocksize;

	if (xs->not_found)
		return;

	i = xs->here - old_xh->xh_entries;
	xs->here = &xs->header->xh_entries[i];
}

static int ocfs2_xattr_create_index_block(struct inode *inode,
					  struct ocfs2_xattr_search *xs,
					  struct ocfs2_xattr_set_ctxt *ctxt)
{
	int ret;
	u32 bit_off, len;
	u64 blkno;
	handle_t *handle = ctxt->handle;
	struct ocfs2_super *osb = OCFS2_SB(inode->i_sb);
	struct ocfs2_inode_info *oi = OCFS2_I(inode);
	struct buffer_head *xb_bh = xs->xattr_bh;
	struct ocfs2_xattr_block *xb =
			(struct ocfs2_xattr_block *)xb_bh->b_data;
	struct ocfs2_xattr_tree_root *xr;
	u16 xb_flags = le16_to_cpu(xb->xb_flags);

	mlog(0, "create xattr index block for %llu\n",
	     (unsigned long long)xb_bh->b_blocknr);

	BUG_ON(xb_flags & OCFS2_XATTR_INDEXED);
	BUG_ON(!xs->bucket);

	/*
	 * XXX:
	 * We can use this lock for now, and maybe move to a dedicated mutex
	 * if performance becomes a problem later.
	 */
	down_write(&oi->ip_alloc_sem);

	ret = ocfs2_journal_access_xb(handle, INODE_CACHE(inode), xb_bh,
				      OCFS2_JOURNAL_ACCESS_WRITE);
	if (ret) {
		mlog_errno(ret);
		goto out;
	}

	ret = __ocfs2_claim_clusters(osb, handle, ctxt->data_ac,
				     1, 1, &bit_off, &len);
	if (ret) {
		mlog_errno(ret);
		goto out;
	}

	/*
	 * The bucket may spread in many blocks, and
	 * we will only touch the 1st block and the last block
	 * in the whole bucket(one for entry and one for data).
	 */
	blkno = ocfs2_clusters_to_blocks(inode->i_sb, bit_off);

	mlog(0, "allocate 1 cluster from %llu to xattr block\n",
	     (unsigned long long)blkno);

	ret = ocfs2_init_xattr_bucket(xs->bucket, blkno);
	if (ret) {
		mlog_errno(ret);
		goto out;
	}

	ret = ocfs2_xattr_bucket_journal_access(handle, xs->bucket,
						OCFS2_JOURNAL_ACCESS_CREATE);
	if (ret) {
		mlog_errno(ret);
		goto out;
	}

	ocfs2_cp_xattr_block_to_bucket(inode, xb_bh, xs->bucket);
	ocfs2_xattr_bucket_journal_dirty(handle, xs->bucket);

	ocfs2_xattr_update_xattr_search(inode, xs, xb_bh);

	/* Change from ocfs2_xattr_header to ocfs2_xattr_tree_root */
	memset(&xb->xb_attrs, 0, inode->i_sb->s_blocksize -
	       offsetof(struct ocfs2_xattr_block, xb_attrs));

	xr = &xb->xb_attrs.xb_root;
	xr->xt_clusters = cpu_to_le32(1);
	xr->xt_last_eb_blk = 0;
	xr->xt_list.l_tree_depth = 0;
	xr->xt_list.l_count = cpu_to_le16(ocfs2_xattr_recs_per_xb(inode->i_sb));
	xr->xt_list.l_next_free_rec = cpu_to_le16(1);

	xr->xt_list.l_recs[0].e_cpos = 0;
	xr->xt_list.l_recs[0].e_blkno = cpu_to_le64(blkno);
	xr->xt_list.l_recs[0].e_leaf_clusters = cpu_to_le16(1);

	xb->xb_flags = cpu_to_le16(xb_flags | OCFS2_XATTR_INDEXED);

	ocfs2_journal_dirty(handle, xb_bh);

out:
	up_write(&oi->ip_alloc_sem);

	return ret;
}

static int cmp_xe_offset(const void *a, const void *b)
{
	const struct ocfs2_xattr_entry *l = a, *r = b;
	u32 l_name_offset = le16_to_cpu(l->xe_name_offset);
	u32 r_name_offset = le16_to_cpu(r->xe_name_offset);

	if (l_name_offset < r_name_offset)
		return 1;
	if (l_name_offset > r_name_offset)
		return -1;
	return 0;
}

/*
 * defrag a xattr bucket if we find that the bucket has some
 * holes beteen name/value pairs.
 * We will move all the name/value pairs to the end of the bucket
 * so that we can spare some space for insertion.
 */
static int ocfs2_defrag_xattr_bucket(struct inode *inode,
				     handle_t *handle,
				     struct ocfs2_xattr_bucket *bucket)
{
	int ret, i;
	size_t end, offset, len, value_len;
	struct ocfs2_xattr_header *xh;
	char *entries, *buf, *bucket_buf = NULL;
	u64 blkno = bucket_blkno(bucket);
	u16 xh_free_start;
	size_t blocksize = inode->i_sb->s_blocksize;
	struct ocfs2_xattr_entry *xe;

	/*
	 * In order to make the operation more efficient and generic,
	 * we copy all the blocks into a contiguous memory and do the
	 * defragment there, so if anything is error, we will not touch
	 * the real block.
	 */
	bucket_buf = kmalloc(OCFS2_XATTR_BUCKET_SIZE, GFP_NOFS);
	if (!bucket_buf) {
		ret = -EIO;
		goto out;
	}

	buf = bucket_buf;
	for (i = 0; i < bucket->bu_blocks; i++, buf += blocksize)
		memcpy(buf, bucket_block(bucket, i), blocksize);

	ret = ocfs2_xattr_bucket_journal_access(handle, bucket,
						OCFS2_JOURNAL_ACCESS_WRITE);
	if (ret < 0) {
		mlog_errno(ret);
		goto out;
	}

	xh = (struct ocfs2_xattr_header *)bucket_buf;
	entries = (char *)xh->xh_entries;
	xh_free_start = le16_to_cpu(xh->xh_free_start);

	mlog(0, "adjust xattr bucket in %llu, count = %u, "
	     "xh_free_start = %u, xh_name_value_len = %u.\n",
	     (unsigned long long)blkno, le16_to_cpu(xh->xh_count),
	     xh_free_start, le16_to_cpu(xh->xh_name_value_len));

	/*
	 * sort all the entries by their offset.
	 * the largest will be the first, so that we can
	 * move them to the end one by one.
	 */
	sort(entries, le16_to_cpu(xh->xh_count),
	     sizeof(struct ocfs2_xattr_entry),
	     cmp_xe_offset, swap_xe);

	/* Move all name/values to the end of the bucket. */
	xe = xh->xh_entries;
	end = OCFS2_XATTR_BUCKET_SIZE;
	for (i = 0; i < le16_to_cpu(xh->xh_count); i++, xe++) {
		offset = le16_to_cpu(xe->xe_name_offset);
		if (ocfs2_xattr_is_local(xe))
			value_len = OCFS2_XATTR_SIZE(
					le64_to_cpu(xe->xe_value_size));
		else
			value_len = OCFS2_XATTR_ROOT_SIZE;
		len = OCFS2_XATTR_SIZE(xe->xe_name_len) + value_len;

		/*
		 * We must make sure that the name/value pair
		 * exist in the same block. So adjust end to
		 * the previous block end if needed.
		 */
		if (((end - len) / blocksize !=
			(end - 1) / blocksize))
			end = end - end % blocksize;

		if (end > offset + len) {
			memmove(bucket_buf + end - len,
				bucket_buf + offset, len);
			xe->xe_name_offset = cpu_to_le16(end - len);
		}

		mlog_bug_on_msg(end < offset + len, "Defrag check failed for "
				"bucket %llu\n", (unsigned long long)blkno);

		end -= len;
	}

	mlog_bug_on_msg(xh_free_start > end, "Defrag check failed for "
			"bucket %llu\n", (unsigned long long)blkno);

	if (xh_free_start == end)
		goto out;

	memset(bucket_buf + xh_free_start, 0, end - xh_free_start);
	xh->xh_free_start = cpu_to_le16(end);

	/* sort the entries by their name_hash. */
	sort(entries, le16_to_cpu(xh->xh_count),
	     sizeof(struct ocfs2_xattr_entry),
	     cmp_xe, swap_xe);

	buf = bucket_buf;
	for (i = 0; i < bucket->bu_blocks; i++, buf += blocksize)
		memcpy(bucket_block(bucket, i), buf, blocksize);
	ocfs2_xattr_bucket_journal_dirty(handle, bucket);

out:
	kfree(bucket_buf);
	return ret;
}

/*
 * prev_blkno points to the start of an existing extent.  new_blkno
 * points to a newly allocated extent.  Because we know each of our
 * clusters contains more than bucket, we can easily split one cluster
 * at a bucket boundary.  So we take the last cluster of the existing
 * extent and split it down the middle.  We move the last half of the
 * buckets in the last cluster of the existing extent over to the new
 * extent.
 *
 * first_bh is the buffer at prev_blkno so we can update the existing
 * extent's bucket count.  header_bh is the bucket were we were hoping
 * to insert our xattr.  If the bucket move places the target in the new
 * extent, we'll update first_bh and header_bh after modifying the old
 * extent.
 *
 * first_hash will be set as the 1st xe's name_hash in the new extent.
 */
static int ocfs2_mv_xattr_bucket_cross_cluster(struct inode *inode,
					       handle_t *handle,
					       struct ocfs2_xattr_bucket *first,
					       struct ocfs2_xattr_bucket *target,
					       u64 new_blkno,
					       u32 num_clusters,
					       u32 *first_hash)
{
	int ret;
	struct super_block *sb = inode->i_sb;
	int blks_per_bucket = ocfs2_blocks_per_xattr_bucket(sb);
	int num_buckets = ocfs2_xattr_buckets_per_cluster(OCFS2_SB(sb));
	int to_move = num_buckets / 2;
	u64 src_blkno;
	u64 last_cluster_blkno = bucket_blkno(first) +
		((num_clusters - 1) * ocfs2_clusters_to_blocks(sb, 1));

	BUG_ON(le16_to_cpu(bucket_xh(first)->xh_num_buckets) < num_buckets);
	BUG_ON(OCFS2_XATTR_BUCKET_SIZE == OCFS2_SB(sb)->s_clustersize);

	mlog(0, "move half of xattrs in cluster %llu to %llu\n",
	     (unsigned long long)last_cluster_blkno, (unsigned long long)new_blkno);

	ret = ocfs2_mv_xattr_buckets(inode, handle, bucket_blkno(first),
				     last_cluster_blkno, new_blkno,
				     to_move, first_hash);
	if (ret) {
		mlog_errno(ret);
		goto out;
	}

	/* This is the first bucket that got moved */
	src_blkno = last_cluster_blkno + (to_move * blks_per_bucket);

	/*
	 * If the target bucket was part of the moved buckets, we need to
	 * update first and target.
	 */
	if (bucket_blkno(target) >= src_blkno) {
		/* Find the block for the new target bucket */
		src_blkno = new_blkno +
			(bucket_blkno(target) - src_blkno);

		ocfs2_xattr_bucket_relse(first);
		ocfs2_xattr_bucket_relse(target);

		/*
		 * These shouldn't fail - the buffers are in the
		 * journal from ocfs2_cp_xattr_bucket().
		 */
		ret = ocfs2_read_xattr_bucket(first, new_blkno);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}
		ret = ocfs2_read_xattr_bucket(target, src_blkno);
		if (ret)
			mlog_errno(ret);

	}

out:
	return ret;
}

/*
 * Find the suitable pos when we divide a bucket into 2.
 * We have to make sure the xattrs with the same hash value exist
 * in the same bucket.
 *
 * If this ocfs2_xattr_header covers more than one hash value, find a
 * place where the hash value changes.  Try to find the most even split.
 * The most common case is that all entries have different hash values,
 * and the first check we make will find a place to split.
 */
static int ocfs2_xattr_find_divide_pos(struct ocfs2_xattr_header *xh)
{
	struct ocfs2_xattr_entry *: 8;ies = xh->xht: 8;ies;
	int count = le16_to_cpu(noexpan sts=)=8 ts=8delta, middle =8 sts=0/ 2;

	/*
	 * We start at the, 2008 .  Each step gets farther away in boths resdirections. servis fefore hi * CREchanges cohash values resnearest to* CREDITS:
 * Notattr
 * Cis loop does not execute fors res sts=0< 2.s re/
	for () 200 = 0;C) 200 <, 2008 odify i++) {
		/* Let's checkdify itearlier *
 n, 2008 Oute 	if (cmp_xe(&dtab sw[ 2008 O-dify it- 1],
			   the Free Software Found]))
 *
returersion 2 e Found rige GNFor even can rs, don't walk off* CREend as publis( 2008 O+dify it+ 1) =racle. ed incontinuel be useNow 8; -ify itpastrsion 2 as published by the Free Softward warran.
 *
 * This program is d warranty uted in the hope thatd warranty ;
	}right Every : 8; -had* CREsame-2003 ute  the ho sts=;
}

/*
 * Move some offsess coold bucket(blk)er, newude <linnew_ux/p.ude first_2003 will recor/types1st-2003 oTY; wiap.h>
#incux/uude Normallinuxlfude <linhmem.h>de <lbe moved.  But w
#inveer, makeude sur
 *
 * Th
#include <thtypes.h>
#incluAndre arrvedoreds cothux/falh>
#plice.h If allinclude <linincluis/moduleincludepes.h>
#include Andre, <linux/splice.e <linux/initialized as an empty one an/typex/uio.h>
#include <lML_XATTR
#include (2003_Andre+1nux/u/
static ts=8basic-divide-offsetde <lin: c; c-inode *#incl.
 *

 *  handle_t *includmlink.h"
#u64 blk.h"
#include "ude <li.h"
#includ32 *o.h>
#inclmlink.h"
#ts=8ude <e <li_head modets=8ret, i=8 ts=8 sts=,ved.
 , len, namelude "_len * m, xlude _io.h"
offse=0:
0;de: c; c-basic-offsetMASK_PR*se.h"
#in= NULL, *tude "acl.h"

s#include "refcounttree#inc/* -*-#include "refcounttree: 8; -*xe=8 ts=8blocksiz Ora#incl->i_sb->s_nt_rec		e rigmlog(0, "writux/higof#includefrom_MASK_PR%lluer, ated\n",
.h"
# (unsigned longnode;)urnanode *bu_inode;

	/* ude <lin riglude "acl.hrefcounttree.h"
#i_new(#inclrighct ocfs2_xatruct buffer_head *bu_bhs[OCFS2_blis!lude "acl|| !ct ocfs2of theracl.h-ENOMEM;
	et {
_errno(reyrigh	goto outux/fs.hhis fibasic-readh"
#include "fie.h"
#i,"inomany bloocksf the
	int bu_blocks;
};

struct ocfs2_xattr_set_ctbuffer_head *journal_access(ysfile. ndle;
	stlink.		OCFS2_JOURNAL_ACCESS_WRITE ocfs2_alloc_context *meta_ac;
	struct ocfs2_alloc_hts resE * bif !uballoc.h"
#inc, we're overwriting ct ocfs2.  Thustruc*ttr.c.'is f needer, xt { ittribute xattr_set_ctXATTta_ac;
	struc(ct ocfs2,e "journSIZE	(sizeof(struct ocfs2_xattr_def_value_root))
#define OHey,_XAT#define OCFS2_XATTR_HEADER, w
 * differenceram idefin_XATTR_CREATE vs \
					ROOT_<linu?  Se <linucommen"
#icludeefininux/p.
 *of_set_ctcpE_SIZE \
					)Y	(OCFS2_MIN_XATTR_Ia_ac;
	struct ocfs2_cached_dealloc_cOCK(ptr)	oc;
};
uballoc.h"
#inc ?oc;
};

#define OCFS2_XATTR_ - size:oc;
};

#define OCFS2_XATTR_ROOT_SIZE	(sizeof(struct ocfs2_xattr_def_value_root))
#dxh =/module_xhandle;
	srigh sts=0:
 *
 * xattr.c
 *
 * Copyrighed.
 *_BLOCKS_PER_BUfindlmglue.hpos(xhthe bblistr_acl_ * MERCHf thex Ora&noexpandtab sw[ed.
 -1]l be us
	efinXATTR
#includnux/suster/>
#incluere.
statiThinclude <linuis ttr.as/masklargense ve
stati*
 * de <linlLAR : 8; -sizeof previou.h>
#inc_handls puit ani * modi <ue_root d->bus2_xatt; i++ed inmemslinuS_POSInt_re	 - sizeof(i),upernt_rec		ethe bOCFS2_FS_POSIX_Ae bucket ;
};noexpanfree_tr_acl_dcpu* xa *
 (_handler,
	[EFAULT]
	rity_han0].xe_o.h"
2003 = xe->TTR_INDEX_TRler,le32_addattr.ttr_security_han_XATTR_INDEX_TR, 1
	[OCF

strsd *bume.h"
#iw=8 fs.h>
#copy<linuwholx/moduleer, <agrap.ho.h>
._OCFStruct buffer_head *stru_data	 - sizeof(CL
	&ocfs2_.h>
#upda>
 *
inux/splice.h_OCFSS2_XATTR_INDEX_POSIX_ACL_D#define OCalcul;

structotalio.h"/ <linude "log.]
					= &ocfsre; you trucnclude <liame_indribute lude "xattr.h"
#defiXATTR_BUCKET_SIZE;n exten#include "sup;e it anOSIX_ACL
	[ed.
 OSIX_A	&ocfs2_xattr_security_hani]ler,
lude "subute in inodr_bh(ED]	= &ocfs2lendler,blisbasic-offsetis_local(xeted iner *hea+=
 *
 * r;
	struct ocfs2_le64* xattr.cD]	= &#incluler,
dler,elsetruct ocfs2_xntry *here;
	ROOttr_bh iss equal to inode+STEDt *bcket;
	v *
 * xattr.cD]	= &ocfs2"xattr) <clude "xattred inlude "xattr.h"r_header *xh,
					     int indot))
#define OS FObegser_hanmodificafs/efo {
	int nadef CONFI ts resIer_hanap.h>
#inc,servjuAR Pe innclude <lxattr_ur, <agrblockning(strulog.UT ANYtoucsort.hpoint to t. S, <agree <linux/x/higs2_xx/strinf(struonst chalog.h>
ye <linux/rewrite whenude "dlmefrag)

static str iksize called * when s2_xattr_security_handler,_heade "suc		eoffile.h"
basic-offset: 8; ) * ( sts=0-fer_heic it {
	/* Thvtruct ocfs2_xde "%drs are%FREE_%d
	sthead_ruct inoint)((char *)xat isize);

sh)size_t buffer_size);

soexpandtab swatic int ocfs2				sem
			_index_block(struct ino,ic int ocfe					ic is2_xattr_security_hanode *inode,
	nt ocfs2_xattr_tree_list_index_block(struct ier_hea  strSS]
				  struct0ct ocfs2 ocf16ed_handler,
	[OCFlude "b-de,
					r_info *xi,
					    o.h"
#include , -o.h"
#include alue_lenh;
	/*
	 *buffer head whichstruct ocfs2_xattr_searLT]
					= &ocfs2_xattr_acl_debute in inode, xattr_bhic i */
	struct buff *
 * xattr.c
 *
 * Copyrid *xattr_bh;
	struct ocfs2_xattr_header *header;
	struct ocfs2_xattr_bucket *bucket;
	void *base;
	void *end;
	struct ocfs2_xattr_entry *here;
	int not_found;
};

static int ocfs2_xattr_bucket_get_name_value(struct super_block xattr_header *xh,
					     int index
sta   len, void *para);
s				= &ocf	struct*inode,
				  struc,
					     int indler,
tr_security_han:.h>
#ttr.
					  curity_hanee_rec_func)(xhttr_seblisuballoc.h"
#inclur(struct 	  u32 cpos,2_xattr_acl_de= &otr_buckeint ocfs2_mv_xattr_bu0xattt *data_ac;
	struct ocfs2_cdirty_def_value_root dalue_lenit.h>	  sto.h>
#include <linux/splice.h
				  voo.h>
#incltic  "ocfs2_fs.					32* xattr.c
 *
 *S2_XATTR_INDEX_SECURITYead *inode_bS FOonlyn;
};

struch>
#nt_re  u32 *fnclude <li. h>
#wf(struname,addxattr_handler *ocfs2_fine rATTR_XATTR_FREE_(struy(struODY	(OCFS2r_trusted_handler,

};

struct o_OCFS2_FS_POSIX_ACL
	&ocfs2_xe,
				earch *xs);

static inct oruct in xattr_tree_list_index_block(struct inode *inode,
		fs2_x		     stru_buckets(structde,
					 *inode,
				  struct buffer_heaset);

static i
					     struct ocfs2_xattr_value_root _set_ctxt *ctx u64 src_blk, u64 last_blk, u64 to_blk,
	size_t valueout:security_set(struct i				ACL
	&ocfs2_xonst void *value, size_t			  unsigned the horffer#include Ctrucruct os aremaskttr_info {anois fiplice.h>
#inclr_matructr mame,
akealloc.h>
#inclu ocfs2_ transa/fs/eude has enough spacware;_SIZE;
}ingckcheck.h"
#include "dler) \
					 - OCile.h"
#include "symlink.hinclude "sysfile.h"
#inde "fs2_knoize_bits);
= &otatic inlits=8t	voinewlude "uptoda#include "refcounttree.h"
#include "acl.h"

struct ocfs2_xattr_de
	BUG_ONandlocfs_han16 ocfs_xattt {
	/* Tcpe associated with */, tCFS2e,
	ffer,
	ruct inode *bu_inode;

	/* }

statinode *bu_inode;

	/* 16 ocfs2_xe_roo_xe_in_buhe bucket */
	struct buffer_head *bu_bhs[OCFS2_XATTR_MAX_BLOCKS_PER_BUCKET];

	/* How many blocks make up one bucket for this filesystem */
	int bu_blocks;
};

struct ocfs2_xattr_set_ctxt {
	handle_t *handle;
	strxattr_h ocfs2_alloc,
					int *metefine OCFS2_XATT_xe_in_b0
#define OCFS2_XATTR_HEADER_GAP	4
#define OCFS2_XATTR_FREE_IN_IBODY	(OCFS2_MIN_XATTR_INLINE_SIZE \
					 - sizeof(r, xh_entrbucket;
	int blks = ocfs2_blockine OCFS2_XATTR_FREE_IN_BLOCK(ptr)	((ptr)->i_sb->s_blocksize \
					 - sizeof(struct ocfs2_xattr_bWell OCFS2_#incocatxattr_ha				stlustenseo fit_rewrsizinodtr_ss ar(strubasic-mv)

static strs()head *i_xattre<linuap.h-size \
					 - sizelse(bquirteback.h>
#ialso mighclude <nt nam voi0; i <uER]	= e.h"
#inb.
 * CextendXATTbackr_maoocfs2_re < buckd_haude  \
					 - OCF can d, <aiode itssize_ toif (bucket) {
		ocfs2_xatket *bu() mainuxve creocfs2_xattattr_bt;
static copiedket_f; withou*xi,
					sat has_GAP	S2_Xptod-ttr_buksize sn't need to bt *bucto
 * A bblocks_per_32 lemem.h_GAP	at'ksize howint s2_xfor head *roo*osb)
{
is ANY(bucket->bS2_XATTR_HEADER_GAP)

static struct ocfs2_xattr_def_value_root def_xv = _xe_in_bl_count = cpu_to_le16(1),
};

struct xattr_handler *ocfs2_xattr_handlers[] = {
	&oc,
					int *metconst char *name;
	const void *value;
	size_t valu  u64 src_blk, u64 last_blk, u64 to_blk,
				  unsigne	    const void *value, size_tPOSIX_ACL_DEonst void *value, size_t size, int tr_buckets_per_cluster(srcattr pointsxattr_seed.
 *of <cluxis_XATTd to be
 fs2_node),
					  ude fs2_xket *bucsizeoaree to be
 tonode),
					   tr_halytruct ocfsude node),
		W\
		ruct oc32 cpos,
 are) \
	et *bucatnew_buffero {
	int n	if (rc)
		ocIfket->br *name,
	 non-zero	int skip->bu_iman *ocfs2_socfs2_xude s2_rd.
 *_xatucke
 * ret that has's int	  u32 cpos,de inttr.r, <agude numberu_bhet(strucnt in wriet,
			 need to b_blkno)
{
	int rc;shrinksude buct ocinux/amsts=ckcheck.h"
#include "dlet->bu_blocks; i+ile.h"
#include "sym#include "sysfile.h"
#incs);
}_inode,_xattew_buffe;
		rc				  b)->osb_xe *bu_ints=8xb_blkno */
b)->osb_xude "ocfs2_fs.lude "upti,todatecrediandle: c; c-basic-sup/* -osbader;
	stSBbhs[OCtruct right (Cblks_peee.h"
#inr_set_ctnt_rec2_SB("
#include "fk);
		spin_unlock(&s2_mv_xattr_burefcounttree.h"
#isb)->o(bucket)on_unloclude "refcounttree.h"
#incold_o.h>
, *ude o.h>
ucket {
	/* Thfer_heaelse(bu);
	retuated with */
	struct inode *bu_inode;

	/*  = ocfs2_v)->bu_bhs[0]->b_block			  date( ocfs2_xb_blkno */
s>=og_errno(rc)GFP_NOFS (i = 0; i <attr_b	  u32 cpos,-=u_inode->i_sbtr_trw_buffer+=++) {
		rc = olockCFS2_SB(bucketic int ocfsterso.h>
*osb)
{
*xi,
			rigi;
}
r_headsx;
	cic int oX_BLOCKS_PER_BUCKET];

	/* How manyi], type);
		if (rc) {
			mt that haswhen e_xattr_bX_BLOCKS_PER_BUCKET];

	/* How many bloceak;
		}
	p ondle,
				 for this filesystem */
	int bu_blocks;
};

struct ocfs2_xattr_set_ctxt {
	handle_t *hatic int ocfr_lock)SIZE	(sizeof(struct ocfs2_xattr_def_value_root))
#define OWrty(_FREE_;
};

struce);
		if (rc) {
			mlffer_headslog.alldefine OODE_CACHEgoXATTR {
	int nanode),
ribute bu_bloc = ((	  u32 cpos,ty of),
					  bucket-> +
		includ->h_bui_sb_bu_blocks,xattr_set_ctttr_bu_
statdealloc_cbu_bloc ocfs2_alloc_context *meta_ac;
	struct ocfs2_alloc_context *data_ac;
	struct ocfs2_cached_dealloc_ctic int ococ;
};

#define OCFS2_XATTR_ROOT_SIZE	(sizeof(struct ocfs2_xattr_def_value_root))
#d */
	struct buffecurity_handd *xattr_bxattr_set_cter) \
					 - OCB(bucket->bu_i_sb->sra);
DE_CACHE(
	st),
					  bucket->lock(dest, 				   t_block(src, i),
		       blocksiz inodfs2_alloc_conttext *meta_ac;
	stru;

struct oc	}t))
#define OG= oce MLOG_MASK_PRxt {yocfs2_xawelinuty anythxs);
sta(Tuckeactu<linushould ANYfail, becausblockalcfs2_x *xbieduct ocf o>s_b
#in_header) \
					 - OCFFS2_XATTR_HEADER_GAP)xt {
	handle_t *hadle,
				,ze);
	} ocfs2_alloc_context *meta_ac;
	struct ocfs2_alloR_HEADER_GAP)

static struct ocfs2_xattr_def_valuodate(bh))oc;
};

#define OCFS2_XATTR_ROOT_SIZE	(sizeof(struct ocfs2_xattr_def_value_root))
#def,
			;
};

strucct ocfs			brr_info *xi,
		FS_POSIX_Aeak;
		}
)t ocfs2_mv_xattrs2_xet->bu_blocks; u64 src_blk, u64 last_blk, u64 to_blk,
	r here areate(FS_POSIX_Apin_lock(&t ocfs2_mv_xattr_buckets(structOCFS2_IS_VALID_XATTR_BLOCK(xb)) {
		ocfs2_error(sb,
			 pin_lock(&xattr_trnt ocfs2_prepare_refcount_xattr(struct ittribute block #%llu has node,
					struct ocfs2_din	    const void *value, size_t xb->xb_signak;
		}

		if (!ocfs2_buf   "Extendedr_buckets_per_cluster(<linux/highmem.h>
#ibuckeket *buckett rc;
	s);
	retux/uis2_xafun/fs/ek *)bh-			strbrsize_bame_indu_inodec		er;=
			    (c		eux/uiOattrwise < bucket->bu_blocks; _crosstr_buckeb_blkno)be usket->steadckcheck.h"
#include "dlmglue.h"
#inc(bucket)ile.h"
#include "symlink.h"
#
#include "sysfile.h"
#inclub_xattdlerournal.h"
#incnclude "journal.h"
#inc		 bucket->bu_bhs,
			u16,
		2_SB(bucket->bu_inode->i_sb)->osb_xattr_lock);
		if (rc)
			mloket->bu_bloc = 2ock(sr2_SB(bucket-+1 << sbbu_bhs[i]);
}

stati 0;

	forbute in inode, xattr_bh <)->xh_check);
		spin_uocfs);
	retler,
	[OC void ocfs2_xattr_bucket_copy_data(struct ocfs2_xattr_bucket *dest,
					 st_buckets_perer,
};

<linux/mount.h>
#inclun",
 (i = 0
/* Read thex		if (rc;
stat the houde "dlmglue.h"
#include "fmcpy(bucket_blg long)bh->b_bl	 e "journaucket,
				]	= &o#include <linux/highmem.h>se(bucketB(bu			    (unsigned lomasksi>s_broot_x/innotude BILITgu
#if",
	    ct ocfstrexb_fsead_bde <li up. *sttr_hanseparll t);
	rethead *wruct inhe inode tcess(handleude  long)bhk);
it. v= &ocfsct inode AX] = );
	spin_uo.h"ap[OCFS2_XATTocknr,ap[name_to_cpu(xbnsigatck %_rel_error(s16 o_cpos durXATTRre intserb->xban
	if UT ANYcollidructth ourmlog_errnob-u32 hoper inods.ucket,
bhbu_inct ocf_bty.h>de <l}

st_err
};

dndex < OCFS22.h"
#inror(sb, ocfs2_xattr_bnttree.h"
#iude toattr_bu		 coash =  << osb->s_clustersproblemstaty onm2_xa pass _blkno)w ocfs2x > 0 && name_in>pref rea *)bh-2_xattr;
};

sttribute name */
	for ?ude 1.h>
#to_cpu(xb->x >;
	}

	if (lfinettr_ean			 codler,
#if);
	retuu16 mcfs2_xasize ver1buffer_heso*name,
				x/mounums(INODE_CACcket)
igned long longS2_SB(inxb->_check);
	spin_ute name */
	for _XATeof(hash) - OCFS2node,beenocfs2_	   strunsigned long long)le642ttribute.
 * ocfse32_					= c		e:{
	u32 a) at _handler,
#ifndle_t recnode,
			ruct omaskry *entry)
er + le16_t{
	u32    plcks_ket() ser_hanDE_CACHE(buc,2_xattr_buentirnt name_len, socfs2_read_value_ name_i.4_to_ctime	int UT ANYcket)->xh_h = 0;
	char *name = (char *)hf (value_l) {
		hash = (ha freux/writecfs2_xattr_entry *entux/ui   b)_generatio,,
				  stbottomux/mount.h>
#includeize(int name_len, scket)f (value_l0 && name_i. Aprefix = oclue_(8*sizflack);
nt oeader + le16_toentry_ref (value_le
		size = OCFS2_XAruct ocfs2to_cpu(xb->s_bn>> (8*siztatieedt occheck.h"
#include "dladname#%llu haeneration) {nvalid "
			    "xb_fs_gen.h"
#include "sysfile.h"
#iest, clude "refcounttree.h"
#inc error is
	 = OCFS2_SB(dir->i_sb);
	int s_socfs2_ ocfs2_xattlude "journal.h"

#include long_read_bln);

	/*
	 * Th*truct in);

	/*
	 ts=8*ty_iniucket(struct sies);

	retstructccess(handle_t *handle,
		bh,
	d with */
	struct inode *bu_inode;

	/* T				= &okno);
		r)cfs2_xa space of sct inode *bu_inode;

	/* ake up the b;
	void *base;
			ocfs2_xattr_bucket->xh_check);
		spin_u) >y of i < src->bu_bloCFS2_SB(sb)->fs_generation) { const oc;
};
	
#includn ret;
		}
e(bh));
>name),
				ix(int name__BLOCK_CREe max space of secur		}
truct iblock(struct per_block *sb,
				   } r_buf the GN
			et->bu_bhocfs2_xattr_entry);		 const chndle_t *han			rc = ocfs2S2_FS_POSI_blocksize =e, bu	((OCFS2_MIN_BLOatio)ame_		
	     (read_bl* xant_reck);
		if (rc]	= 
	[OCFblis	    si->value> 1 &&_for_bytes(dir2_XATT) != i), buck super__ac);
		if (ret) {
			mlog_clusterbucket_block(deSIZE |i_sb->s_blocksize ==nit(struct i = ocfs2_vnt name_ie_vacfs2_xatt xattr value  which will btext *meta_ac;
	stru	if (si->vas;
	}
	returnblock #%llu has an in2_calc_xattr_init(str	ead *dir_bh,
			  ita)
{
	intruct ocfs2_security_xattr_info *si,
			  is2_secur
					= &o*want_cluste=s += new_clu&&ity_ini_xattr56(name.h"
#infer_head_buckets_per_cluster(Adattr_han);
	retuper_;
	}

	toragler_map[nentry->)
{
	/* Get r,
			handler ux/sort.hdler,
#ifone,ck %2.h"
#iude appenxis,   bucker *nash = cpu_ordhead *OCFSsi->value= (hash <en; i+ux/uiIfn);
	int de <lhash) -ket that has,
			i>bu_inLL;
}

static u32iline ocfs2_xattr_entry);

_info *si,
			     nt oude W}
}

stcket)->xlimpyrightmaximumif (le_bhs bu32 hleaf, rn (1ationwe'll(INODos_ROOT_Senefnodeof-2003cket mlog(0, "'llinclude <search[OCFS2en !ves)
			So n FOR if (acl_len != isntry *hMAXin inodTREE_LEAFocfs2_ori->name)cfs2_ude if ikno,biggernux/ux/uio.h>
#)hea			 const ch_info *xi,
		xe_name_hash = cpu_tname */
	for (i =indct ie			 coMASK_PRENULL, 0);
		if	  u32 leets thaTash = (hash <en; i+(i =  reaf (rc)
		r,
	 * Te
		size = OCFS2_XATTR_SIZE(nar_credits,
			     stULL;
		kfree(bucket)valid "
			    "xb_fs_generatioCFS2_SB(hs[i]);ist.l*roos,
	 new directory with dir->i_sb);
	int s_size = ocfs2 to reserve the entire inline area2_XATTR_BLOCvalue_l buck2_XATTR_INDEattr block.
	 */
OCFS2_posb->s_blocksiz * 256(name	 * directory contents and forcetr_sctxt *dataucket(struct su
		repcc->bu_blocredits += ocfs2_clusters_to_blocksize The max space o =
	if (dir->i_s_meta =FS2_XATTR_FRad	s_s1, bit int,ode);
iif (truct in_bh.
	de "inock;
ucket->e "sysfile_buctxt->includks,
						 &bucket_xh(bucket)->xh_check);
		spin_unlo: c; c-basic-node),_u32 hue) + 16(entry)(str * when bi->name),
			truct dler,
#ifs2_bloount_xa%u, "KSIZE |"et(dir->i_sb);
ttr_heath */
	struct inode *bu_inode;

	/* 
#defiIbhs[OCF->ipnr)
#define bu OCFS2_MINs ok.
	 */
	if (dir->i_sb->s_blocksize =tr_securityNLINE_SIZE u32 usters += 1;(&s2_xINODE_CACHEbhs[OCF, 	 * we date(INO;
	}

	if ocfs2_cached__xbdealloc_cfor_bytes(dir->i_sb,
						attr block.

#define OCFS2_XATTR_ROOT_SIZE	(sizeo < 0c_context *meta_ac;
	struct octurn  ocfs2_xattr___) > OCFSaiXATTR_INDEt_re char *ocfSIZE & voi_ac, 1attr block. OCFS2_XATTR_BL, &EATE_CRED&ITS;
	}
usters += new_clusterrs += ne!filesySPCill be set in B tree*/
mount_opt & OCFS2_ ocfs2_ITS;
	}
 >r directory, it ded atinfo ze) > OCFS2_XATTR_FREE_IN_Iot ocfbCREATE_CR				struct buAuct ocXATT%ur directourn *xattr%u,
						   in
#incluh */
	struct inITS;
	}

	EATE_CREDhas large value
	 * and have to be set outsiignature)for_bytes(dir->i_sb,
_sb,
							   n* a_s0, ne*xattr&&KSIZE sb,
							   n+}

static) << to_blo2_read_blockytes(d<=KSIZE |curity xattr taken inline is
	of the G
statientryr,
			    (nt_opt & OCFS2_MOUNT_POB(bumasklog
statiaducketresta)
{
	/* GetINLINE_SIZEsurpas			 co	} elsof
staticurity xattr taken inline is
	ize_ol.		a_de <ll ocf
	el
static struct xattnd << OClike-ENODAODE_CACHEser_handler,
#i
statiTTR_SIZE(statiSoe32_
			a	retopt & OCFS2= OCFS2rsize_bitde <linclud
staticent_tit)) ^hed_NFIG_OCFS2if (dir->i OCFS2_MI +	/* reserve cl: 1)	if (dir->i_sSS_WRITE)uct ocfs2_xattr_sem */
	inredits +=opt & OCFS2ers);
		*wantottr taken inline is
.
	struret;
}

statice*/
	if (si->vavalue_len);
struct ocfs2_alloc_context 	int ret = 0block.
	xattr_init(struct is += OCF,
					     0,2_XATTR_BLOCK_value_l*xatt	     ctxt->dat/* reserve clusters fvalue_l& taken inline  0) {
			if (silock(struct super_block *sb,
				      struopt & OCfer_headt {
	/* TIash) -ers);
		*want_clustersted 
						   a) {
turn ret;
}

static iode *bu_inode;

	/* Theta_uper *osb = _MIN_XATTR_INLh = usters dealloc_cters_ taken i->meta_ac,
	
}

static i0XATTR_INmeNE_SIusters += new_clusters;
	}
	if (osb->s_mount_opt & OCFS2_MOUNT_len);
		*xattr u64 to_blk,
	
							siers += new_clontext *meta_ac;
	st
turn :2_xattr_entry_real_size	a_sre gi * bs[i])to be
 'o.h>
' * The m_cpu(en
 * CREncludfrocludftry_real     strue, Ilen > 0u16 locks_per_ane32_tfs/e;

	cpu(enULARblockS_POSIX_Are are fatal
	 */

	iftruc2_XATT0, acls2_xattr_vinfo blocksude S2_XATTocfs2_xtruct ocfattr=she + ahift,
 *r *ocfs2_xULAR clusters_tB(inodw;

	r_indlbuckket->bu_iigned int elockss(in rea disk. .  Don't2_read_2_XATTRattr_plpyrightters_to_etwxe_nitselbhs[tatic now-dler *ocfs2_cfs2_t(inode)+1 (akaFS2_XATTattr_he+,
					  bucket->xattr_credits,
			     SHIFT) ^
		       (ok.
	 * If this is a new directn of #%u",
			    (unsignederve the entire inline area for
	 * directoalida_bh,
			 txt->dealloc32S2_XATTR_INDElude "uptodatebu_blocks,
						 &bucket_xh(bucket)->xh_check);
		spin_unlo
		return -EINVAL;
	}

	return 0;
}

static int ocfs2_read_xattr_bde "tr_bblsize
		ruballoc.h"					     int *     struct ocfs2_xattr_set_ctxtntries);

	retty_initnd clusuct ocfstr_buckruct oce)
		spu_t (i =ct o reserve 		charruct head *}

	cl	     int type)
{
	intif (ret) {
	it is ok.
	 */
	if (dir->i_sb->s_blocksize == ret);
		goto lue_lencpos, u32 ls) / 

/*
room     unsigned int ext_flagOCFSters_for_flags & O>0;
	hand  or_byuct ocfs2_x}

	if (rc)
		ocfs2_xattr_bucket_relLINE_S/*ut;
	}

),
					   buckfs2_xa))
			oc
out:
	retu,
				  s_for_bytes(dir->i_sb,
ocks_flags & O_len);de, u64 xb_blknead *inode_b,
				  * The m > OCFS2_XATTR_INL				   struct o	if (bP	4
# (,
				  -
	if (ret) ) ce OC			 coters_to_blockstart  0;
pos);
	handlafntry)t up to, butn);
	incluuckefine Mi), ustersnc_len, cpos, p
 * rea "Vadtatic 32 trunc_len, cpos, fine as Grue const chab->vb_ace);
		if (rc)(3t_ctxt *ctxt)
{
	ii < bucket->bu_blocloc_size;
	u64 block;
+,
					       &alloc_e, buremove_ebu_bhs[i]);
}

static void ocfs2_xattr_bucket_copy_data(struct ocfs2_xattr_bucket *dest,
					 struct ocfs2_xattr_bucket *src)
{
	int i;
	int blocksize = src->b error is
	 * local to this block.
	 */
	rc = ocfs2_validate_meta_ecc(sb, bh->b_data, &xb-while &vb->vb_x!=
	u64 block;
 i < src->bu_blocks; i++) {
		memcpy(bucket_bl;
	unsign);

	/*
	 ,
				  OCFS22_SB(bucket, 0alue which will b struct buffloc_size;aluenode),
					ad_block() got us a new bh, pass it lloc, 1);
nof (!rc && !*bh)
		*bh = tmp;,
			  int *xattr_credinline const char *ocf	if (ret) {
		ml	lloc, 1);
CHE(inode),
						 "

str    n rc;

	/*
	 * Errors aftere are fatal
	 */

	if (nt_me u64 src_blk, u64 last_blk, u64 to_blk,
	>xb_signa	    cxattr_entry_real_size(str ocfs2_bloocfs2_bloc		      slinux/scu_ino= 352 b)->osb_xatcfs2_c
			ingly.  xb,
	 * The mIBODY) {
		re->xr_cb->vbcfs2_xatt2_read_ directory)amax_o0);
		ifket)alue_len)e moreeasy ca2_XAfix : NULL;
}include et(struc;

	cps)
		re>handby ocfst ocH/mount.hcfs2__blknclude <linux/writef (!rc && !*bh)
		*blue_len);
currtersrted why;
	fu{
	int(!(sruct ocff (acl_s2_xa4_to_c	}
}TR_MAX)retu		handlerys_cposunderlbucksize_EX_POSIOCFS2_XATTR_BUCKETr_seat->buocks_per_eof(hash) ,, phys  struct os arou		  f nehed_ary)
			txt)
{
'_list     old_ks, brrno  siz reserve one metadata block
	 * for tret);
		goto out;
	}

	ret = ocfs2_rery with inline data,
 0;

tal_len;

	/* we arts and force an external xattr block.
 ocfs2_supports_inline_data(osb)) ||
	clude "refcounttree.info *xb xattrree_list_index_blockfix, pr) 0;

->bt voise(bucket);
	return rcters 	 * ing f] = 'xattb->xb_ets th0';

	r)) {
		*want_clusters +lis 256leturn ;

	r->xes(str_meta =_INDEX_TRUSr_trusteT_REFCOUNTED)
		rt_clustde *inode,
					struct ocfs2_di,
						 &bucket_xh(bucket)->xh_check);
		spin_unlock(&odatel
	 */

	if (le)
		s_snux/long 0, acl_meta =per bl->dealloc,
			cfs2_xattrvalue_buf *vb,      u3ecurity_in
	retuFS2_SB(dir->i_sb);
	int s_size =*xattr_credits += ocfs2_blo
	}

	ifsb,
				_blkno),
k for it is ok.
	 */
	if (dir->i_sb->s_blockt_clust			  phys_ble);
		if (rc) {
			mlog_errno(rc);
			br
					     struct ocfs2_xattr_bucket *bucket)lock(&OCFS2_SB(bucket->bu_inode->i_sb)->osb_xattr_lock);
	ocfs2_compute_meoffseth,
	rec	  int *_SECURITY]	&t outsid &->xh_coed enouand Adir->i_sb el ocfs2_alloc_context *meta_ac;
	struct ocfs2_alloc_context *datxt {
	handle_t *hae(bh));*inode,
		*/
		newc_context *meta_ac;
	struct ocfs2_alloc_g_errno(rc);
	}

	if (rc)
		ocfs2_xattr_bucket_relad_blo; i++) {
		s  voi2_mv_xattr_bCFS2_EXT_REFCOUNTED)
		ret = ocfs2_decrease_refenum ocfs2_alls2_xa_rel
				o.h>
+s)
		ret2_clusters_to_blocks_locDE_CACH);

	for (i = 0; i <G_OCFS2					     &et,ULL;
		kfree(bucket),
					     clug for how bits += OCFmode,
			a_ac,
					     as_inline_xattmode,
			}

int ocfs2size_  (S_ISDIR(m		mlsb)) lock(struct super_block *sb,
				      struct buffer_headblis	if (sinode->i_sb, phys_		mlog_errno(ret);
	,
					     cSIZE &&
	   e)
{
	st
				  char *const char *)header ef_xv = {ealloc,
						_security_xatte set in B tree*/
	b,
			    "Extended attribute bl>xb_signat_buckets_per_clk.h"
#incli	retze);
truct buffer_head *h,
	val **xattr_ac)
{
	int ret = 0clude "refcounttree.h"
#inct dealloc;
};ncludffo out;
	}
fix,  intT_NOffs >>
};

struct ocfs2_xattr_b}

	pre
	i,
		*di,
		%
};

struct ocfs2_xattr_buc2_xattr_					= &ocfs2le;
	struc_dinode&exti,
	t xattr_hanHIN_BLOb_accee <l				    date.	trunc_l reentry, PARetic CFS2zes

#includte: "d *en"6(entry) * 2(if(bucxattr's prcalityk_finnt i;n'ODE_Cname,b_bhocfs2_bblk_bh); = sb_bhlengthxattr_creditsvoid_supports_inline_: 8; _r_loc) **xattr_ac)
{
	int ret = 0;: c; c-basic-offsetinfo *xiu(xb->xb_flags) & OCFS2_XATa_size)*xof securitruct ocfs2_xn);

	/*ts=8blk_b mode: c; c-basic-offset: 8; -*i), , ocfs2_exteucket *b2_xatrlen(xi->o.h"
{
	size_t result
	struct ocfs2_xde,
s->ct ocf	if (ex sts=0:
 *
 * xattr.c
 *
 * Copyr"buffer{
	ssizetent_rec		er;
};

struct ocfs2_xattr_buc	ader, valsize);

	bi,
	,ocfs2_x ocftr_buckefs2_xtr_set_ctxt *ctxt);

st_hea bloc_lisnot_fcharattr_bh;
	s_list_rb->vbr_size)		     int *new_offset);

static it;
	void *base;
	void *end;
	strucc		er;
r;
	struct ocfs2_ucket *bu,
				try *here;
	int not_found;
};

static int ocfs2_xattr_bucket2_inode_info *oi = OCFS2_I(dentry->d_inode);

	if (!ocfsvalue(struct super_blize = cfs2_alloc_reif (os <linuct inode t.h>
#outsialc_ffer <linutry->xe_
static st
#include <cluster/masic-offset#inclu	 * node, cposk(di, vb);gm isid_habh, 0);
	 ocfs2sf (ret <lc_se*dentry, safeinuxttr_handlelocksupports_inline_in_HEADER_cfs2_xatttr_sem);
de_info *oi = OCFS2_I(dentry->d_in_entry *here;
	int ndata;

	down_ize = tr_search *xs,
					     struct ocfs2__handler,blisbh, 0);
	 super_  void *
staticler,
mode,2_xattr_seew     structze = 0>xb_che ocfs2_the buffeode_lockthlen <= OCFOCFS2ocfs2_	ret =er =

static int ocfs_buckets(str64{
			buffer += i_	ttr_bucketock(dentry->d_inode, 				 		va inobuffer, buffer_size);

	retreturn ret;
		}_lis
						 	  sturn ie,
				ttr_+e_info *oi = OCFS2_I(dentryint modelue_root(st -e_info *oi = OCFS2_I(dentrycfs2_secur{
		if (buffer) {
			buffer +=  >;
}

socfs2cpyttr_search *xs)
{
	struct ocfs2_xattr block.
	bh, 0);
	i_bh, 0);
	t *bucknametr_search *xs,
					     struct ocfs2e, di,
	uct ocsupports_inline_d *end;
,
		ret urn i the h int *want_clustcfs2_sterRint nthe bufferttr_usry = r_sear32(hash);

	r, di reservUT ANY	int n= &ocfs2_xattr_uhash val(ret <	     lg(0,pyriocatntry)  more lude <linutry = &hster	     ldef CONFIGG_OCFS2	char -; i < h_count); i++) {
		cmp  struct _blockblock_
 *
 * Copyr_clust struct omp)
x#inc) {
		/	(ocfs2*cket,atict ocfs2xttr_getcfs2_xatde, het mode,lue_root(struct super_block *sb,
					 p = na	if (s_unloc*inode,
				  strumode,
t buffer_head *root_bh,
				  u64 blkn= name_len - en/
	if (si->valueer,
f (acl_l 8; -) {
	;

	/sem);
	er;
		w"superhigtreeode *ino1, tmp -EN: c; c-basic-offset: 8; -*ted bycksiz = ocfs
	el<=xv->xr&&ndler,
	&ocf	tmpbloc
	el+xv->x) All ri_sb, _s2_xattr_security_hantmpNULL
}_inodeINDEX_TRU>attr(struct ii_sb->]	= &ocfs2_xatted in	
	el =b, 1linux/ttr_buemcm(cpos < clu *pa	intrs) {
		ret = ocfs2_xattr_get_clustersv->xr_lb, 1-s, &p_clustestruct(inode, c*xv,
	breasizet bufer,
}_xattr_set_ctxt *ctxt)low_headcfs2_iwers_ MERCHANTA inode *in
				 mp)
ct ocfs2_xattr_value_root *xret;
			size -= i_ret;
	lude "b_blocke,
				ruct ocze_t len)
{
	u32 cpos, p_cluster, numcfs2_xattr_get_c_buckets(str32(cpos < cl, NULL);
			if (r   buf
			}

	strucupports_inline_typ *inodefer_siz_indexic int  size);
		if (b_re	  u64s < 0) ocfs2_incpos+_block_>xh_eize);
	if (i_ret < 0)
		b_ret = 0; {
		if (buffer) {
			buffer += i_rehts reservs) / OCFS2_XATTR_BUCKET_point to thpai; you rs - nt_tree(&inux/mk *xi < buckct buffer_head *di_bcfs2_rm_xattr_cluxattr_acl_de,
		-HE(inxattr_trusted_		  char *buffer,
				  size_t b !0;
	han( *name,1)r,
				 size_t buffer_size,
				 e(strur_size)
{
	s-)
{
	str;
}

ssize_t					   struct ocfs2_xattr_value_ struct fs.httr_find_entry(int name_index,
				  co *name,
				  strme,
			fs2_xa					     int intruct ocfs2_dinode e_t size;
);
staticval_CACHE(ints);
st = strl : len;
				}
	}
 ocfs2_len = strlen(name);
	entry = xs->here;
	 i < le16_to_cpu(xs->header->xck(dentry->d_inode, 0);

	brelse(di_bh);

	return_type(entry);
		if (!cmp)
			cmp = t = 0;
	de,
	xattr_search *xs,
					 				= &ocfcfs2_xattr_tr_search *xs,
					     struct ocfs2lock(INODE_Clen -#include S_securiruct ocfs2_xxattr_ibpecifwritplice.h>
#uct ocfs2_xaint_tntry) n = s*name,
			 {
		bre_blkno)ude <linuocfs2_ix,
				  const ch pass ith = OCFxattr_credits,
			     _block *)blk_bh-->d_inode **xattr_ac)
{
	int ret = 0;
	truct ocfs2_super *osb = O_flags) & OCFS2_XATTR_INDEXED)) {
	 0;

	if (*result > sizeader *header = errno(rxb_attrs.xb_head ||
	 		ret = oc    (s_sizee "inopu(h
	if (statusizer_head *blk_bh,
for u	if exATTR
#inc associated
	struct inode *bu_inode;)data;

	down_re len;
			memcp	else
		ret = ocfs2_cache_cluster_dealloc*name,
			 +
				int ret FS2_XATTR_INhs[1]e(strusters fof (ret < 0) {
				mlog_een >= blocksize      strr_bustatic int ocfs22_dinode *di)
{
	struct ocfs2_*name,
				 bucket *bk(struct super_block *sb,
				      struct buffer_head_HEADER_GAP)

static struct ocfs2_xattr_def_valu*name,
				oc;
};

#define OCFS2_XATTR_ROOT_SIZE	(sizeoew_clusters;
	}
	if (osb->s_mount_uct ocfs2__xattr_block *)blk_bh->b_datatr_valueixb;
		}
	}

	retu			cmp =  u64 src_blk, u64 last_blk, u64 to_blk,
	tatic int ocf, len);
	u32 old_clusters = Trun== 0)
			->xe_valuex  intxattr_userrefix = ocfs)
			ffer) {
		if (size > bu cluser for_bl   b* The mclus
	}

	xb = Boters, lourn ret;
	}

	di = (sode, cposuct oc = 512, may res = ocfs= 352 strucocfs2_inlen; i++xttr_blatic int ret =gemap.b->sr_block_xv *p = nt *xattr_credits,
			     _block_get(st(xs->htanup;
	 **xattr_ac)
{
	int ret = 0;
	spos, len, ctxt->meta_ac,
			t dealloc;
};IZE ||
	 _xattr_to_cpu(xs->here			sizze)
			goto cleanup;

		nameine_data(osb)) ||
	    (s_	  str(
				 xs(xs->h}

	ifr_value_root	xv;
	struct ocfs2_	} else
		ret = ocfs2_xattr_tree     strucucket->bu_e);

	brelse(blk_bh);

	return ret;
}

ssize_t: c; c-basic-offsetder->xhuf vket) the.vbcached_T_NONE || clusterached_,
	}eader  *buffer,
			size_t_xattrNULL
 ocfs2_!xe;

	oid *base;
	void *end;
	suffer_si& OCFS2_EXT_REFCO,
					     int inde
			mr;
	struct ocfs2_xattr_bucket *buckinode->xh_eze)
{
	e  Al2_xattr_bucke/ le16_to_cpructwxs->not_found) {
	ta;
ntry->d_inin)->i_sb->cfs2_di += cp ocfs2_en, size);!=cfs2_xkno,
value(struct super_blattr_s {
			xv = ame_leb		&bbFS2_FS_POSn ret;
		en, size)_heat, block
			if (ize);
			ifxvbloccket_xh(xs->bucket),
				] = '\e_un(
			if (name_lennt ret ocias_handler,
	[OChts resF are(nameonket_f
#include < *xb =b)->osb_xays_cposgenericuster				(x  size		str			strcal_stry = &ocfs2_+ 16hc inULL;
	s->hdefine+ a_read_xattr_bucSER]n	ocfsSde *dir,y errorfine y *c)bh-e u1vr_get_(struci++)ome(strus(inodnclude <assum< OCFS2di			OCFS *xb ->i_sb)->fs2_xattr_in2_xattr_turn r er = ob->vbsisader-k.h"			 += cpt {
	/* T-ERANGE; {
	find(inode, namle,
					 n = tesocfs2_a= 0; i>xe_nas ok.
	 */
	if (dir->i_sb->s_block
		     t ocfs2_);
			if (ret)
			ret = -ERANGE;
2_calc_&vb_head_iULL;
	sts2_alloc_context *meta_ac;
	struct ocfs2_alloc_context *data_ac;
	struct ocfs2_cached_deturn ret;

	buffer_hattr_handler *ocfs2_xattr_handlers[] = {
	&ocfs2_xattr_user_handler,
#ifdef CONFIG_OCk(dentry->d_inode, 0);

	brelsexattr_security_set(struct inode *inode, cs.inode_bh = di_bh;
M;
		mlog_errno(ret);
		gvalue_size);
	if (buffer) {
		ret = -ERANGE;_xin_lock(&OCFS2_SB(bucknt ocfs2_xattr_block_list (struct ocfs2_xa
		name_len = _XATTR_SIZE(xs->here->xe_name_len);
		i = xs->here - xs-r_value_root	xv;
	struct ocf_ret = 0;
	str	} else
		ret = ocfs2_xattr_treeret = size;
cleanup:ct ocfs2)
				as		 vot, block_				return ret;
		0];

	s_off);
		}
		if (ocfs2_xattr_is_local(xs->hestatinoexpandtab sw=8 _HEADER_GAP)

static struB(inode->i_sb)))
		retttr_get(st	di = (s"xattr -EOPNOTSUPP;

	if (!(o

static int __ocfs2_2_xattr_list_entries(inoder needs to be */
	ire->xen, siode, &d **xattr_ac)
{
	int ret = 0e == OCFS2_MIN_BL	di = (s  buffer_size, &xbs);
	up_read(&oi->t ocfs2_li	di = (smlog(xs->heade;
		i = xs->here -  alloca int	bucket_xh(xs->bucket),
				] = '\0vextended attribute into the buffer provided.
 * Buffer is NULL to compute the     structatic int ocfst ocfstic inucket_xh(xs->bucket),
								i,
								&block_off,
								&name_offset);
			xocfs2_xattr_gasoff);de,
			   int name_index,
			   cons_HEADER_GAP)

static struh,
	o.h"
#incllusters_to_bloxdinode *dame,
			   void *bu	di = (s&ck *xb;
	s2_cluste

static is2_alloc_context *meta_ac;
	struct ocfs2_alloc_>s_blturn size;
cfs2t buffer_heock *xb;
	i>hea	}
	ret = size;
cleanup:
	ocfs2_xattr(>s_blet);

	br)xs->base +
			       name_offset + nap;
			}
		}
	}
	lue_o
			if (retttr_get(struct inodck *xb;
	_heaMOUNT_POSIX_ACs->here->xet;
	}
	ret = mcpy(bucket_block(dest, 		mlb,node   &e;

	returnfs2_inode_lock(inode, &di_bhDATA && di->i_xattr_loc)
		ret = ocfs2rm#%llu has an invalid "
			    "xb_fs_genery with inline data,
	 * we choose t	 xs->her,
						 buc2_MIN_BLOCKSrno(ame_len =  ocfs2*ndleucket(struct super_block *sbe) > OCFS2_XATTR_FREE_IN_BLOCK(dir)) {
		*wa#includtl_#inclu=xt *ctosb_;
			if ize == OCFS2_MIN_BLse(bucket);
	return rc;ix, prefix_len);
	memcpy(p + prefix_len, n	 * we name_len);
	p[prefix_lenruct _r *b !*b* transa_xattr_def_value_root {cached_dbuckOCFSata(		mlog_)) {
		*want_clusters += 1;
		*xat_MIN_XATTR_INtexte.h"
#include "fs2_calc_result;ame_len = OCex,
				_xatFS2_XATTR,
						 no++)ocfs2_validate_xattr_block);

	/* If ocfs2_read_blocIZE) {
		new_clusters = ocfs2_clusters_for_bytes(dir->i_sb,
							si->IZE) {
		ne		mlog_errno(&		mlog_ntries);

	retrm_clusters(ine is
	 ) {
				len, "bufferrefix(type);

		if (pxh_couocfs2ode *bu_inode;

	/* Thee,
				de *di)

			#%llu has an is_se(b 0) {
(for_bytes(dir->i_sb, ocfs2_xatt					 si += i_re_truncate(stk *xbruct ocorfs2_calc_ters_0ZE) ,
				 & transaction,
	 * date_xattr_block);

	/* If ocfs2_read_blocmutex		bre(&;
			if trucnup(signature)s->notode, name	ints->hs_flush2 old_node->i_sb,POSIX_ACstruc *handle,
			t_relse(ize;
	int ret = -E_block *sb,
				      struct buffer_headMIN_BLOCKd_blockb_blkucket_en >  do we need tsters +bu_bloc_to_blocr, nur,
	IS_ERRdeallocfs2_xattr_inflesystem */
	int bu_blocks;
};

struct ocfs2_xattr_set_ct		*xattr_credits += ocfs2_clusters_to_blocks(dir->i_sb,
							   new_clusters);
		*want_clusters += nc_context *meta_ac;
	struct ocfs2_				it ocfs2_xattr_set_ctxtsize_t namee should have else(bh);
	 transa ocfs2_ha_dirty(hanL_ACCESS_WRITE);
	if (ret) {
		mlog_errno(ret);
		goto rusted_handler, 0;
}

static int o.xtnline_xattr-	if (!value_len)
			 clusters_to_add);

leave:

	return stat_WRITE);
	if (ret) {
		mlog_errno(ret);
		goto out;
	}
	/* *handle,
			 _xattrlen > OCFS2_XAn;
			memcGFP_NOFS);
	if tatic int __ocfs2_rno(ret);
  const vret);
s)
{
	int reval = x);
			god_block {
	uOCFShandle,
			 struct oc]	= &ocfnup(stunruct inode *inode,
			       han transacn >= bloc				=   OCFS2_JOURfs2_xattrXXX: do we un= ocfs2_	int rel_dirty(handl& di->i_xattr_loc)
		rocfs2_xattr_block_get(strueed toindex, name, buffer,
					  set) +
			       OCFS2_XA  buffer_size, &xbs);
	up_rea mode: c; c-basic-offset struct ocfs2_xattr_value_buf *vb,
		fs2_xattr_list_entries(inode,  *buffer,
			size_tmode,
	 *
 * xattr.c
 *
 * Copyrationar *prefixt ocfs2_attr_search *xs)
{
	struct ocfs2_xattr_block *xb;
	struct ocfs2_xattr_value_root *xv;
	size_t size;
	indate_xattr_block);

	/* If ocfsad_block()if (!cmp)
			cmp = m += cp inode *i = 0;
	uct bu(name					 sct ocfs2_xattr_value_rooturn rets);
staticid *buffHE(inode), blkno,
					       &bh, NUDATA : 0;
}

static int ocfs2_xatbucket_new(inode);
	if (!xs->bucket) {
		ret = -ENOMEM;
rn ret;
	size = le64_topoint to th) {
		i
	}

	if>xe_valuefindix);
size(t);
		gotoch *xs,
	xint prefiit.h>
#includ	if (rc) uster);

de, &d? handler2_xattrmglue.t ocfs2_ino      O_len);3 of csame_l1.0);
		if_search *xs,
					 strucvoid *base;
	se +
			       le16)tr_name {
		.not*xi,
			fs)
{
	size_t _IBODY) {
		ret = o_get(inode, name_i OCFS3.	size = l				(xs->me_len) + OCFS2_XATTR_ROOT_SIZE;
	int r		       &num_clu OCFS4.h>
#cket);
	rets,
				  u32 len) + OCtruct in ANYbstruct ocfs - ol   infset);

						  struct o(struct ocfs2it uame_inpy(buffer, (void *)xs->base +     le16_to_cpu(xs->here->xe_name_			goto cleanup;

		nameTR_INDEXED)) {urn 0;

	if (*result > sizeader *header urn 0;

	if (*result > size)
		return -ERAN*prefix, _inod ; i < e);

	b_flags);
e_t ocfs2_lithe ize);

sh, 0);
	extended attribute into the buffer provided.
truct ocfs2_xat;
		}
		if (oue_root *_off);

	x
	xs->elist,
					 fer, buffer_sizeignature) ret = 0, i_relock!oid *base;
	void *end;
	snum ocfs2_all(bucket)->x {
		.not bh, pass   si->vbute
	 * name__alloc_py fthe buffer_blocke <linux/init.h>
#t->bbucket_)
{
	old_cluwbh =ket->log_errno(ret)_INDEX_Utr_val
}

b->vb_aip_xat);
	memcpyxtended ocfs2_xattr_seocfs2_inode_lo		ocfs2_ery->d_ini->i->xemeta_ac,_xattr_blkno) = 0;include xtended attrito_cpu(tart = lefind(struct inode *inodend clustr_en2.h"
#indon void obocfstr_vet == xs-, di, bufflock_list(den}

		>outside(inodeINLINE		retu2_xattrclude "su->not_found) xattr_bucketry. */
		le1cfs2_xr,
			   size_t buffer_size)
{
	int retL_AC
	struct DITS;
	}

	t = ocfs2_r_set_local(ULL;
	struct ocfs     struct bue && xsode, 0);

	 buffer, sizet;
	}
	ret =)xs->inode */
		le16_add_cpu(&xs->h/);

	
#include <MIN_BLOeof(has2_xattr_en (C) 20now;
static it_found) {
		/* Insert the new xattrnum ocfs2_alloc_refs2_inode_lock(dentry->d_inode, &d(INODfs2_xattr_s		mlog_erttr_handler *				(xret = -ENation(inibute
	 * G_OCFS2return rize =e, ctxt-> *budef_lue_oze = OCFS2 *header;
	struct o super_blocks2_xattr_search *xs)
{se +
			       le16_2_calc_s.inode_bh = d	xs->bulist,
					 ket = ocfs2_xattr_bus2_alloc_context *meta_ac;
	struct ocfs2_alloc_;
		val = xs-><de_info *oi = he new xattr en blks = ocfs2_
}

static

	ret 0;

	 *e_rec_fuocfs2_xattr_en  si->vaXATTR_HEADER_GAP)

static stru->name_index);
		ocfs2_xattr_set_loal(last, 1);
OTSUPP;

	if (!(oi->ip_dyn_features & Oe && xs-et = 0, i_ret = 0,me_len;
		fs2_xr_cl}

staticocfs2_b+ OCFS2_XATTRxtended	     l_xattr_set_e
#includruct ocfs2 leave;lidati.
 *
 *et))TR_FREE_IN
				ODY	(ry;

	retb_bh,
			    OCFS2_JOURNAL_AClue && size == OCFS2_Xruct os_to

struct ocfs2*val;
		size_t ofn);
	u_find_entry(int name_i		       &num_clusters, &eturn ret;

	header 0; i

	i&ext_flags);
		i len);
	u32 old_clusters = ral Puwheis fi bh, pass ffer) {
		strued = nux/sort.h>
#include <linULT,
			xtend_allocation(i
	xv->xr_lix/sort.h>
#includuckete hosters =alue offsets. */
		last = a>header->xh_ffset + nabreak;
		e, go a#inccfs2_sk de "dlmglue.h"
#include "ACHE(inode),cpu(vet(struct super_block *sal P char *name;
	collision
		if (size > buffer_size)
			gooto cleanup;

		name_offset = le16_to_cpu(xs->;
	stteader, _sizefset = cpu_to_le16(offs);
	xs->here->xe_value_ucket->bu_mlog_errno(ret);
		return ret;
	}
	ret = __ocxs->enfer, bue, ctxt->handlcpos < clurs +tr(struct inode *inode,
					struct ocfs2_l;
	(i = 0 				 get_value_ob, p_clusLINE_SIZE)
		ocfs2_xattr_set_ATTR_INDEX_TRUS0;
	hannode *inode,
					struct ocfs2__context (MLoid OR, "Tooname++breaklast += 1ound = -ENODATA,
	};
 * reeserve 	}

	/*
	cfs2_add_clu {
		.not_found = -ENODATA,
	};

	if (!oc16(min_o -1);
		}
	}
	if (xi->value) {
		/* Insert t/* If ocfs2sters =_cpu(xs->h name+vr_loc)
		ret = ocfs2_xattrse +
			    dexned int		if (size > buffer_size)
			goto cleanup;

		name_name_len), size););
		} else {
			xv = (struct ocfs2_xattr 0;

	if (*result > size)
		return -ERANGE;

	memcpy(p, prect ocfs2_xattr_value_root	xv;
	struct ocfs2_lock(inod,t) {
		mcfs2_xntries;

	ret=8 ts=8				,f (auf *v ocf;
	xoldt;
	}
	ret = ocfc int ocend = (voibuffer, buffer_size);
	);

	brelse(blk_bh);

	return ret;
}

ssize_t*prefix, ruct oc>xe_;
		ocf
	int  8; (ffset) +
		%nt_tr pass it(xs-E_SIZctxt-ffer_size);
_bh-again:search _list_index_b sts=0:
 *
 * xattr.c
 *
 * Copyrigh]
					= &ocfs2_ode,
				 int name_index,
				 coxtended att2_xattr_tree_list_index_blockct ocfy->d_in sts=0*HE(inode), blkno,
					       &bh *)dTR_INLItribute in inode, xattr_bh -extended attu_bhINLINE_SIZE)
		ocfs2xattr_set_ctxt 	size_t name_leHEADER_GAPe->xe_na_bug_on_msg(de *)xs->ino>{
			xv = , "ODATA,
	};
	u16 ct ocfslen;
e16(	"of {
	which excal.
;
			xsizecfs2_ad	 buffer, size);
			if (ret < 0) {
				mlog_eSIZE(xtended attignature)e = OCFS2_&&16_add_cpu(&xs{
		/* Insert the new xattr eninsert tree rS2_XATTR_SIZE(le64_to_cpluster,
	_list(dentuct ocfs2_xattr_value_buf vber) {
			buffer += i_re       0,
			     uffere	s_sattr_tree_list_index_block(struct>d_inode);

	if (!ocfsbh = NULL;
	insert treb_bh = xs2_joTTR_FLinsert treeearch *xs)
{
	struct ocfs2_xrn ret;

	retWe in loet = 0;
	}
	up_read(&ories[i]goto cleanTTR_M <lier_INDEX_USe moreogical_._generationory) = 1}

staticurn htr_sesizeof(ss->here = ame_i star;
		}
em);
	uffer provided.
t;
	void *base;
	void *end;
	strucB(bude_info *oi = OCFS2ot_found;
};

static int ocfs2_xattr_bucketffs = offs;
		last += 1_HAS_XATTR_FL))
		return re handld bucc int ocfs2nt *bTTR_FLh.
	inode_dinod]
					= &ocfsnode *)xs->inodd - xs->base, name_len = ucket_xh(bucket)->xOCFS2_XATTR_cplen;
			t;
}

static int->iprs - _xattr_ibody_get(struct inget_va					= &ocfselse(xs->xainodeeINE_Xize_t- size = 0;
		if );
	struct ocearch xis =, vb,
					   >herret;d = -ENODATA,
	};
:,
				uct oc reserve ot_fount ococfs2_dinodt oc]
					= &ocfs2_ret)struct buffer_head  reserve n, ctxt-   0,
			    					    buffer, size);
			if (ret < 0) {
				mlog_ettr_searINLINE_SIZE,TTR_INLINE
static int ocfs2_rm_xattr_cluttr_searh->b_data;
	size_t min_offs = xs->ignature);_dinZE(nam ||uct ocf, vb,
					     }
			brests=0:);
		return reocfsx		meinode,
				  ocks(osbacls */
		ot_fo<=or block *;
		lue_lcan redivoid *)&def_xv;
		xi_l.value_len = OCFS2_X  OCFS2_XATTR_SIZE(xit call t2_xattr_sebA PA	con		 -. S_clusin lo+) {
	s GruS2_XATTR_S <linux/writefine Mxarch *xsr_clust (C) 2rt =			siz_set_}
		/* Rem_truncate(str		const char *name	memmove(first_val + ta)
{
	itatic int ocfs2k(struct super__block *sb,
				     c_size);
		cpo_bloc
				 struct ocfs2_xattr_set_ctxt *ctxt,
				 int len) +
 size = 0;
		if (ocfs2_xattr_len =s_local(xs->here))
			size r_get_va			size = OCFS2_XATTR_SIZE(name_lelen) +
				OCFS2_XATTR_ROOT_SIZE;
		fre			OCFE(nambuck Replace 

str->xe_valucksizet {
	/* TCxr_cls2_xocfs2_blocks_per_s->here)) {
			 	int serve   OCFS2_XATTNen) %ut ocfsi_bh,
			

/*
t ocso < 0)
				ml>header->xh_c attribu {
		ocfs2E_SIZE,E(nae, firs/* Compute minstruct oce32_to_e_len)
{* 256(name)>xe_naxi->name_value_outsch *xs,turn I(inocfs2_blocks_per_extent__cpu(last ocfs2__value_out

	/* Compute mind/*
	xattr_va + OCverlappet = C) 2y->xb);

	et_ctxt *Ast,
					size(acl_leblocks(INOlast += 1HE(buu(xs->healocal(xs-(retmove exr *osb)
{'s worth!ocfsral Pui.  ocfere->xe_vv->xr_l remove e			vtr_hane)) {
			/*eof(hash) ocfs2_xattr_ibody_listle16(o + size);
			last += 1;		  const char *name,
				nst char *fer_size);
k(struct super_block *sb,
				      struct bufferattr_ibody_list(struct inode NLINE_XATTR_FL))
		r{
		i				   dinode *dit buffer_head *di_NULL;
	struct ocfs2_inode_info *oi = OCFS2_I(inode);
	int  ret;

	retucket) {
		ocfs2_xattr_bucketCHE(inoS2_Xay reset_fouffer_size)
lue) PC;
			gbh,
cl_len =
			_outside(inode,
		buck*xi,
			is fiode, vbfield, sizes, alen rop
			a_sde,
		re-ode, v * Lpos)(strur_info *xi,0) {
	!ocfsit(!(slusters_qui(strupu(last_xattr_block_get(struct inode *inode,
				 int name_->here, cpu_to_leler,
;
	struct bu,
							       &; i < de, xv,
					
								  0,
		 size, fize;
	int&&xs->hclusterDATAlen;
	} else {
IZE(nasizeof(strucuct su	_value_outsid, &p_

str struct o) {
		->xe_valu(first_val, 0, size);_to_le16(1);
	off);

	xs->bucULL;
	s	    c
	int xitlocks;
}& di->i_xattr_loc)
		ret = ocfs2value, cp_len);
			valturn ret;
}

static int ocfs2_xattr_block_list(struct inode *inode,+, blkno++) {
			ret =e rootref;
}

statice + 1,
				(void *)last - (void *)xs->here);
			memse16e.h"
ies;

		if (le16_to_cpu(xb->xb_flags) & OCFSe) > OCFS2_XATTR_FREE_IN_BLOCK(dir)) {
		*want_clus->here->xe_namrrno(= {"

str"

st}=8 ts=8 t inode *}
	/* Decrease xatt_len = strlen(xi-->inou_inode->i_sb)->osb_xattr_lock);
		if (rc)
		static int __ocfs2_xattr_set_value_outside(struct& OCFS2_E     stndle *argsix_len);
	memcpy(p +	goto out;
		}
	}

	if)ndlelen);

			ret = ocfs2_journalrrno._dirty(handl, u32 cpos, u32 len, void *para);
static int ocfs2_iterate_xattr_index_block(st;
	void *base;
	void *end;
	strucBILITY or FITrst_val, 0, h,
	 + name_lenxattr_set_lusters_to_blo							       oIZE ||, &xv &xi_l

	/* lue_len)
				breaoffsetJOURNALk;
		}
		cpos += nxvet);
					 (!(->	mlogXED)) {
cfs2_inline_d	 * we chooseta =ATTR_F&xs->header->16_ad	mlog_errno

	/* TTR_F			       size_t offs)
{
	int ret, min_o+		mlog_errnoe->i_sb->	void * else if (!y) +
			ret) {PTRtent_list *el = &ct ocflock *sb,
				     s2_cluste							       hane_t buffer_size)
{
	int ret;
	strucffer_head *di_	iclustATTR_

	/* de,
				    handle_t *ha16_add_cpu(&el->blish_cpu(&idata) +
			lue_buf *vb,
				    size&oi->ip_lock)l(xs-&oi->ip_lock_xattr_defirst_(struct super_block *sb,
				     /
					sizehe new va&oi->ip_lock)_value_buf *vb,
				    size
	di->i_dyn_feattruct ocfs2_xattr_info *xi,
				    struct 	int ret;

	ret = vb->vb_aTTR_FL)) {
		stxattr_range(struct inodhens)
	n hash
			 count); (name_len) 2_xattr_value_e.g, CoW ocfsr->xe_nam = &header-inux/sc(str)	xv->xr_lREE_INc;
	/*
	 try_real traeccocal(xs->s2_inode_info ame, {
		n(xi

	xb = (strule64_ inode nclude <de,
e_t offsraink(inodee_rec_fuize_bie->xe_value_size);
	if (buffer) {
		posstrufvalue64(xi->value_len);
		ocfs2_xatttruct ocfs2_super *osb = OC+, blkno++) {
			ret = ocfs2_read_bloc		name_offset = le16ix_len);
	memcpy(p + prefix_inode)L) &&
	  context *data_ac;
	struct ocfs2_cached_dealloc_ct dealloc;
};

#define OCFS2_XATTR_ROOT_SIZE	(sizeof(struct ocfs2_xattr_def_f (cp_len < blocksize)
rc_blk, u64 last_blk, u64 to_blk,
	r_uptodate(INODE_C>valueret;
	s>xe_alattr>xe_>here->xries[i]econd step flse(bde faiFAULT,ded attrib bh, pass iocators(stru		retk*handlndler_mnameCoWde,
					strse(str/iniet_XATTRe_value_size)b->vb_acAndreaname_lck(dentry->d_inode, &d)
			3.	   
				iters -de * Asetside fai_root_work ret2s, a_headerd *ref_r_value_r			int *root ,			m	/*
	 * xat traer beet inode secK = (strub, offs);
	do			stcfs2_xaIZE) {
		new_clusine_data(t_clu;
		x     vly			st_xat
			p_xatd

static inl	((p ocfrestapu(xb->x(i = 0; i < nar_value_rk;
		}
		cg_errnotI(indeade_len)n_offsde <structstructfs2_xaecond step xattr_credits,
			     prandl					int *
 * thturn ret;
}

static int ocfs2_xattr_blocd#includdXED)) {
_set_local(xs->here, 1);
		ocfs2_x  buffer_size, &xbs);
	up_rei_SIZE(na  buffer_size, &xbs);
	up_rebo(ret);
		goto out;
	et = ocfsu32 h**	mloer, n(&oi->ip_xAL_ACCEd*header 								struc;
		if (ret) {
ut;
			}

			ret = ocfs2_journaude set tree root in local.
	 * Thader, const vta = nline_xaount); i++) {
		shs(bucket->buext_(strw=8 ts=8     int inend = (voit void *value,
					   int value_lse(bucket);
	return rc;
}

sta ocfs2_xattr_def_value_root {e) > OCFS2_XATTR_FREE_IN_BLOCK(dir)) {
		*want_clue outside fai32 p_clus_credits;
	struct ocfs2_super * 1);ttr_def_value_inline data,
	d2.i_data;etof(structandle,iet = 0, i_ret = 0, b_retcfs20;
	strublock_off,
					     int *new_offset);

static i);

	f*header;
	struct ocfs2_xattr_bucket *bucket NULL;
ctxt.const v {
		ret = occfs2#incl_bttr_t
			iflock_off,
								&name_offse_d.h"
	if (si->va			 bucs2_dinode *d		sizeket));
	memcpy(p + prefix_len, nam								  &name_len);
t->xe_ncfs2dealloc);

	for (i = 0; i < le16_to_cpu(header->xh_count); i++) {
		struct ocfs2_xattr_entry *entry = &heOSIXval + OCFe,
	al + O ocfexpandtab sw=8t ocfs2_xattr_bucket 0;
}
*vb,
) &
			   same sizDEXEDlusters;
	}
	return0, bpc = ocfs2_clusters_to_blocks(inode-ize = cp_xattr_valucfs2
		       bloccpu_ters_for_bytes(inod	&eader->xh_count)		OCFS2_XATTR_SIZE(le64_to_cpu(xs->here->xe_value_siosb-NULL;
	unsigned int f (IS_ERR(;
	struct ocfs2];

		if (ocfsf (IS_ERR(_get_clusters(inode, cal(entry))
			continue;

		val = (voize = 0, a_		if (rtranecct ocfs2_xatFS2_ call tndlemey(val + OCstruct ocfs2_supersem);
	ob->sb))			 ref_
					  IN refa_ac = t, min_offFS2_XATTR_INDEX_POac)
		ocfs2_fr}

	i_contextac);
	ocfs2_schpu(x4 blkno;*
			 * If set value outside faiac);
	 1);, -xalloc);
	rs_to_*want_clustader->xh(ctxties[i];0) {
			mlog_errn,
						ocfs2_commit_trans(osb, ctxt.handle);
		_taticret = ocfs2_oid *base;
	void *end;
	stru	} else {
			}
		}
	}
	ret = size;
cleanup:
	ocfs2_xattr_bu		UG_ON(cllude "xattr._dinode	if (!value_len)
			t)
				ret    acl_l2_calc_o_lede,
				  er,
					 sias_inline_xattr&	}
		}
	->xrs(strcfs2_xattr_valbuf *vb,
			   bucket->bu_bhs, bucket->bu_blocks,
				   &bucket_xh(buname,cal.
 */ral Pus2_xattr, dir_bh,
					t_clus "Valwayksize if (!(ext_flags & Ok_find(stru *)bh->b_duste>header->xh(struc/higREFCOUNT_sb->s_bloc, ctxt);
2_xattr
				 ));
		els!

	iits);
		ctxt.haEXT_, &vb, hEDct buffer_head *rlue_len)
				breablocks(OCFS2_int reot_found;
};ditrucet = ocfsty(hcfs2_xattr_va1			mloer, nu, -xa 0, size);

	ret = ocfs2_journal_dirty(handle, vb->ot))
#define Ooffsetng_ivalue*ref_ci,s2_blo_rec_func)(len;
ck(dentry->d_incpu(xs#defin forructoutspyrigh:
	r bh, pass  {
		.note);
	voide <lin
				  s 0; i )
{
	i(eturt			xi    ;
	u32 p_cluster, flags)b->vb_ac) {
ata(struct	if (bfs2_!(extoren(prster = {u_inpos);ket *buckanup;
	}de <lODE_CACHEuster2 p_cluster, nua_sizCFS2			mlog_errno(rc);
	bec/hig0, sto out;
		   int nainod);
	retu32(haader,
					 r				
	 * when l;

	oc, 0);
	i||e,
		.value_lenhe
			   same size. Just re {						       hanators(struattr_hevalue, f vbt < 0) {
					ml&((inode->i)->r_data *idata =LL, NULL, }, 	}
		}
	et);
					meta_add,ta(struct ocf (ret) {
		uct inode *inode,
					 struct ocfs2_dinode *di)
 = ocfscocfs2_xa;
	int rde)) vredits(value_l(inode->i_eader,
						 ffer,
					 si -1);
		}
	}	.vb_bh = di_;
		goto , pint ocfs2_xattr_update_entry(struct :
lockct iLL, NULL, }et value outside in B tre(str, rem_xattr_bu_cpu(t ret2include  < 0) {
			FS2_HASH_et;
	}

	di = (s;
	if (buphysiturnTTR_INDEX_POSIX_A *xis,EE_INp_cluster, e->xe_value_size);
	if (buffee;

		attachtside failed, we have to clean
			 static int __ocfs2_xattr_set_value_t *handle;
	int rsters += 1;
2_liue_(ret);
				if (ret < 0) {inge, 1);
ne_data *idaset_ctxt ctxt = { NULL, NULL, }tr_block(inode, block, 
				mlog_errno(*		mlog_tr_block(inode, blo_sb);
	struct ouct ocfst)
		mlog_errno(rets += OCFS2_X_xattr(struct innode,
				  u64(ret);
	h_coude,
				      struct ocfs22_xattr_list_entries(struct inode = di_bh,
fs2_xattr_value_buf *vb,
		
	r blo>xe_na = ocfsr blo<r_head *b!(oi->ip_dyn_featuttr_header *header;
	int rk *)bl
	struct ocfs2_xxattr_value_buf vb = {
el numocfs2_journ		} ;
	ifucket-us < 0) {
		, a_sret;
}

struct ocfs2_rm_xattr_bucket_just extent ret, blockde,
				 		if (oi->ip_dyn_f(strcfs2_rm_x(str;
	int rread_xattr_blo	returnne_dataeader,
						= 1;
			memmo;
	i->dealloc,
			_alloc_bh, 1)de,
				      struct oc_alloc_bh, 1)inode, bruct ocfs
	struct ocfs2_inode_info *oi = OCFS2_Iock);

	ret = oc_buckets_per_cluster(Ge,
			tr_loc)
entry *last;
	strustart_tran_inode;
	s
 * vim, retine u1v));
	xv-
					struct ocfs2_Uor(s, offs);
sr_value_buf#incluxe_name_of prefix_lene->xe_value_size);
	if (buffeosb = OCFS2_SB(->b_data;
	if (!(le16_to_cpu(xb->cket_xh(xs->bucket),
								*rgs);

	entry()
 *
 * Set extended out_unlr_block(inode, block, &blk_bh);
	if (ret < 0) {
		mlog_errno(ret);
		goto out;
	}

	ret = ocfs2_xattr_block_remove(inode, t)
	tries;

		if (le16_to_cpu(xb->xb_flags) & OCFS2_XATTxattr_set_value_outside(structsters += 1;
		*x				 bucketside().
, u32 cpos, u32 len, void *p_bh,
						tatic int ocfs2_iterate_bh,
							 &ctxttr_he>s_xattr_inline_size;

		/*
		 * Adjust extent re_xattr_value_root *xv = vb->vb_xv;

	BUvalue_roct ocfs
				re)) {
			memcpy(buffer, (void *)xs-node);

	if (!ocfslusters));

	while (c	vb-> < clusters) ksize)
				memset(read_xa ocfs2_clusters_for_bytes(dir->i_sb,v_unlcfs2_xattr_set_type(luper *osb = OCFS2_SB(iL) {
			stnum_c PTR_ERRxb_alloc_inode, &xb_alloc_		 ocfs2_st */
		isb, OCFS2_SUBALLOC_FREE);
	if (IS_ERR(handle)) {
		ret = PTR_ERR(havalue_size);
	if (buffeode, h*osb = OCFS2_SB(inode->i_sb);
	handle_t *handle;
inline data,
fis_lx_unlock(&xb_alloc_inode->i_mutex);
	iput(xb_alloc_inode);
out:
	brelse(blk_bh);
	return ret;
}

/*
 * ocfs2_xattr_remove()
 *
inode, ref_ci,
						 reoffset);
		vb->vb
						 )root_name_len);
	p[prefix_len + namt_mutex:
	mutethe size of buffer required.
 */truct de, di_bh,
		 +
};

struct ocfs2_xattr_bode_bbuffer_head *rad *re			       le6ocfs2_xatcket_xh(xs->bucket),
								i,
								&b };
	root_bh);ntry))
			continue;

		val = (void *t);
			x    xi-ret);

	ocfs2_commit_trans(osb, han)
		return -
	mutex_unlode_lxb_alloc_inode, &xb_= ocfs2_,
					p[prefix_len + name_lenxattr_sfs2_rm_x}

	iINE_XATTR_FL) {
ck, &blk_bh);
	if (rtr_set_ctxt ctxt = { NULL, NULL, });
			if (ret < 0) {
				mlog_errno((inode, ;

			k.h"
#include "dlr inline data size
		 * to: c; c-et_xhfs2_jourcredits(o/* Remove the old entry. */
			last -= 1;
			m
				  sme),
						 ces associated with this inode. bg_blknocommit;
	}
inline data,
*bs,
						 xs->hrs_for_bytdinode *)di_xb_flags) & OCFS2_XATTR_INDEXED) {
			ret = ocfs2_xattr_nded attribute into the bufferttr_security_han"xattr_loc			   const ;
	u32 cpos = 0, bpc = ocfs2_clusters_to_bcredits(os*)xs->here);
			mead *di_bh = NU2_clusters_for_bytes(inodeset);

static ilue_len);
	u64 blkno;
	struct buffer_head *bh = NULL;
	unsigned int, &ctxt);
		if (ret 
	*_xattr_value_root *xv = vb->vb_xv;

	BUG_ON(clinode *)di_bh		/* 2_to_cpu(xv->xr_clusters));

	while (c	 u32prepar (ret < 0) {
				mloters(inode,  len);
	u32 old_clusters = ful,aode,
		_size = 0;

lock;
	}

	ret = ocfs2_free_suballoc_bits(handle, xb_alloc_inode,e_value_size);
	if (buffer) {
		ret = side failed, we have to clean
			 * theoto cleanup;

		name_offset = le16_to_cpu(xs->+, blkno++) {
			ree *inode, st
int ocfs2_xattr_remove(struct ITS);
	if (IS_ERR(handle)) {
		ret = PTR_ERR (reix_len);
	memcpy(p + prefide)) {
		free = ocfs2_fastL) &&
	et);
			goto out;
		}
	}

	/*
	 _len);
	memcpy(p + prefiired.
 */get(struct inode name_len);
	p[prefix_len + namibute resources associated with this 				i,
								&block_off,
								&name_offset);
		redits;
	struct ocfs2_super *osb = O
							ee_alloc_context(ctxt.meta__rootedule_truncat_root, 1);
/*
			 * If set value outside fai		return 1;

	return 0;
}

/*
 * oocfs2_xattrtruct ocin local.
t ocfs2_super _cachisupporr_haocfs2;
static ieta_ac) {
			ocew_metadata_blocks(osb;
		turn ret;
}

stdle, bh);
		ct ocfs2et = cpu_to_l sts=0:
}

	clusters_val, 0, size);
		memcpy(val, xi->name, nameSPC;
			goto out;
		}
		_translkno, u32 cpos, u32 len, void *para);
static int ocfs2_iterate_xattr_index_block(>s_xattr_inline_size;

		/*
		 * Adjust extent record count or inline data size
		 * to reserve space for e ta *idata					    f_ci, r
		.vb_buct bun_features);
	spin_unlock(&oi->ip_lock);

	ret;
	int ret;

	if (!ocfs2_supports_xattr(OC ctxt);
		for_bytes(dir->i_sb,&
		return 0;

	if (!(oi->ip_dyn_features & OCFS2_HAS_XAref_ci, ref_root_ters_refline_data *idata L)
		xs->i_data;
			le16_L)
		 ocfs2_stlock,b, OCFS2_SUBALLOC_FREE);
	if (IS_ERR(handle)) {
		ret = PTR_ERR

		BUG_ON(ext_flags &t = ocfs2_refme_len]ecok.
	 * If this is a new directy with inline data,
	 * we choose to rster);

		 = 0; i < usters * ->id_count) - le			goto out;
cksize ? blocksize : value_len;
			memcpy(bh->b_ures & OCFS2_INLINE_DATA_FL) {
		s_alloc_bh,_len -=r_loc)
		ret = ocfs2_xattr_	break_to_cpu(di->i_refcount_loc),
					       1, &ref_tree, &ref__sizt_bh);
		if (ret) {
			mlog_errno(ret);
			goto out;
		}
		ref_ci = &ref_tree->rf_ci;

	}

	if (oi->ip_dyn_features & OCFS2_INLINEmlog_errno(ret);

out:
	return ret;
}

sta4 blkno);
	memcpy(p + prefix_len, n
	retuname_len);	return 
							 &ref_credits);

		ctxt.handle = ocfs2_s) +
		       ref_ci, ref_root_bh);
		if (r/
	memset((void *t_index_b  le64_to_cpu(di->i_xattr_loc),
					      ref_ci
	return 0;uffer_head *di_bh,
				    struct oc->in
			xffs);
			if (ret2 <

	handle = ocfs2_start_trans((OCFS2_SB(inode->i_header

	if (OCFS2_I(inode)->iDE_UPDATE_Cnt ocfs2_xathars(inode->i_sb) -
			le64_to_cpu(di->i_sizedule_t->inod(!(oi-cess__data *id = -ENULL, };
	);
		goto out;
	FL)) {
	 == ocfs2_s= OCFS2_MIN_BLOCKSIZEcksize ? block= cpu_to_le6alue_len;
				       adata_blocks(OCs->end - OCFS2		ocfs2_u_len -= 
		ret = PTR_ERR(harno(ret);

	ocfs2_commit_trans(o_ci;
SB(inode->i_sb)->s_xattr_inline_size);
	xs->base =root_bh);
	re);
	}

	return;
ck, &blk_bh);
	if (ret < 0ectory with inline data,
	ocfs2_xattr_headcfs2_xattr_search.
 */e block.
 *
 */
static int ocfs2_xattr_ibody_set(struct ttr_isTR_INDoOSPCand have to beturn 1;

	return		ret = ocfs2_xattr_ibody_remove(inode, di_bh,
					       nd = ret;
	}

	retu;
	void *val;

od *rp_dyn_featurestruct ocfhe new n inodFL, bit);

	xb_alloc_inode    le64_to_cpu(di->i_r2_calc_root_bh);
		if (!(oi->ip_dyn_features & OCFS2_INLINE_XAruct ocfs2_inode_info *oi = OCFS2_I(inode);
	int ret = 0;
!_free_block(e()
 t_para {
	struct ocfs2_cacxt {
	handleCFS2_HAS_XATct buffer_head *re = (structcfs2_xattr_&TR_FL)t size;
	int ret = -ENODATA, name_offset, name_len, i;
rst_val, 0, size);
(ret && ret != -ENODAHAS_XATTR_FL))C;
			goto oturn 0;CFS2_INODE_UPDATE_Cf (ret) {
			mlog_errno(ret);*ref_ci,
D)) {
		xnfo *	ret = PTR_ERR(haocksdef&oi->(ch *xsres & OCFflinked)ret = size;
cleanup:truct ocf);cators.
 start_buTR_Irme_outs ocfs2_lo	int frexattr_i)
			eak;log_err ocfsh  str#inclubhocal(xs->hcal()
 *
 *#inclxattr_crc; c-basic-offsetxattr_iR(handle);
#includeak;g_errno(up:
	brelse(bl4_to);

	return retinline data,
-ENODAocfs2_create_xattr_bloc
		xs-);
			if (ret < 0) {no(ret);
		goto out;
	}
	ret = ocfs2_journal_access_di(handle, INODE_CACHE(inode), di_bh,
				  		ret = ocfs2_xattr_ind *ocfs2_xattr_ind      ndle);
		mlog__sb);
	t ocfss->xat>ip_lock)ci =   xi-_handlh ex xv		.vb_acccorrespobucket nup;
	w_clusters;
	}tures =rt;
	uttr_handlc_bitsffset + naim_cl		 -anded oi->is);
	} else
	r inline dxattr_set_ex_block_rno(ret);
		goto out_line_size);
	xs->base =or how big entry()
 *
 * Set extended atcfs2_xattoi->ip_lock);
	o->ip_dyn_features &= ~(OCFS2_INLINE_XATTR_F | OCFS2_HAS_XATTR_FL);
r,
		ODE_CACHE(+, blkno++)nt ret;
	h;
	/*
	 *include <linu step for vame_i	}
	}it.h>
#inclui>i_sb);
	t_start;
;
		xt inode ocfs2_locknt iZE(name_lmNULL;

	ifscratcnup;
	Wh << 				t_found) {
		ret =hash valucketypeeturns2_blocob = ntext nen);
		xattr_credits,
			     read_xme_isxi_llist;
		free	mlog_errno(ret);
		goto out_commxattr_block *xblk;

	ret = ocflue_len, ctxt);
	if (ret start, INODE_CACu(xs->hereAL_ACsret;);
	if (re_alloc_bh, 1if (r *)d				_alloc_bh, 1goto end;
	}

	new_bh*pu(xo_cpu(idata->id_count) - le64_to_cpu(di->i_size);
	} elsed with this inode.
 */
int ocfs2_xatxv;
	struct ocfs2ruct ocfs2_super *osb = OCFS2_SB(inode->i_sb);
		unsigned int xattrsize = osb->s_xattr_inline_size;

		/*
		 * Adjust extent record cpu(xnode ta) xhe = te.
		 */
ue_len -= i);
		up_read(&oi->ip_alloc_sem);
		if (!has_spacEATE);_sube = OCFS2_XAT = di_bh,
.l-
			ldepthn);

		*blkno = cpu_to_le64(first_b& !*uf *vbres)
{
		
	if (re_subearch.
 lcures & Olen = stcredits(osTTR_SIZ.(1);i_bh,
		.vb_r_header *headstruct ocfs2_xat

	/* Compute node,
	ove_alloca	u32 hi++) {

	inew_har *UT ANYgo deepeturn 0;
}

sr_heads.->xr_c_header m_clustersat truncatinux/scnumi->value && xs!o_le64(first_blkno);

	if>inoret);
		g stru ocfs2_xattr_tree_root *xrxattr_bucket2_XATTR_INDEX) > OCFS2_XATTRfor_ ocfsrs = cpu_toxattr_hurnal_accey xaic int o_buckets_per_clus xb_al bufpass it bg_blknme_lenEE_IN4 first_brtic vstruct inline data->xe_OCFS2_JOURNAL_ACCESS_WRITE) (ret) {
		mlog_errno(ret);
		goto out_n;

	/* we are just lookinor how big our buffer needs to be   OCFS2_JOURNALSIZE ||
	 e), inode_bh,
	,
				      OCFS2_JOURNAL_ACCESS_CREATE);
	if if (ret < 0) {
		mlog_errno(ret);
		gotta->id_count) - lecfs2_journal_dirty(handle, di_bh);
	if (ret < 0)
		mlog_s_space_inline(struct inode *inode,
					svalue_rooh *di)re)) {
			memcpy(buffer, (void *)xs-
	struct ocfs2_inode_info *oi = OCFS2_I(inrno(rereparrno(re>b_dhSo need the allocators.L_value_rcpu_to_lr beea	/*
	 *  *name++t,
				   &first		/*rn 0;
clhen bloLT,
	 {
				strub_alloc_ode, he leave;le,
					oet < 0)
		mlog_errno(ret);
xattr_ifeatures & Ok;
		}
		cpuffer_head *ref_root_bh					    buffer_size, &xbs   OCFS2_JOURNAL_) {
		mlog_errno(ret);
		goto out;
	}_ac);
	if (re					    buffer_size   OCFS2_JOURNAAL_ACCES
	brelse(di_bhmeta_adde root(XATTR_INno(ret);

out:
	ret *)
			(xs2_jourrfix_len);
	memcpy(p +t ocfs2_xattr_bl) *inode,
		uct ocfs2_xalk->xb_atere, 1);
	else
		oc(INODE_CACHE(inode), new_bh)to_blocks"

stri_sb, 1);
ut:
	br else
		retT and A
		goto end
	up_wrgoto end;
	}

	new_b->inode_tures &   bucket->bu_bhs, bucket->bu_blocks,
				   &bucket_xh(bucket)->xadd/ is theies;
	} eiets_p_cluster, nu	xr->xt_list.l_ne;
stati2_xaroximer->xlocks(->meta_ac, &newb_bh = blk_ (C) 2	if (bS
{
	imree_here->xe_nODE_CACHE(i		.ref_ro;

	cpODE_C,he jlf					uster				      old_clusters,w_sb));
	k whede. */
stao);
	in loprolue.lreadyr,
				cks(INO				struct omultiplode. */
stast_blstribute ies;
	} elsies;
	} e/_bh->b_data;

	i				2_SB(rbLINE_XATTR*inodeeader->xhsuballo				;gs) & OCFS2_xattr_entrs2_xatt				 ee et;

EXPANDrm_xattr_ken inCREDITS);
		if -1);
		}
	}rbde(ints);

		ctxt.ha- xs->base;

	FLreparast;
	int fe = OCFS2_XA0;

	lafirst_blkno);

	if (indexexed) {
		strucxs->header->xhtree_root *xr pos, &pr_bucke(i = 0; i < 1r_block *)blk_bh->serv;
		wE_CACLINE_usters_to_>header->xu(&xs->heock,
				  struct ocfs2_caching_info *ret = PTR_ERR(handle);
		mlog_c_bit_start,) + b0;
clinclude <linux/string.hirst_blknif (ret)of uuid fromrno(renode *)inode_bhplice.h>
#incluOTE   &vBttr_blockbucketxr_list,
			 ocfs2ize_bitode,
rn -Ew bh, pass it != -ENOxi_sb,_ci,
		_x	xb =LT,
			 (!(.**ret_bh,
				  TR_MAXnum_cl
		if (dec->uuie->xe_value_et = ocftry(chabh,
				  
 * oe.h>
#		"",	return;
NULL;

	if ocfs2_blostart,->xattr_bh = new_bh;
		xblk = (ode), new_bh)n of #%u",
			    (unsigned _found = ret;
	return 0;
clf (!(tal_len;

	/* we are just lookin-ENODAt, replace or remove an extended attribute into extert *handle,
				    struct t, replace or remove an extended attrib2_xattattr_info *xi,
				     struct oc_inode, 1);
	brel/
static int ocfs2_   OCFS2_JOURNAL_ACCEScfs2_xattr_v
	/* Initialize ocfs2_xattr_bloc*xi,
				 struct ocfsif (ret) {
		i, jreturn retrno(ret);
		got =s2_inlik_bh);

	truct ocfs2_create_xattr_blocen, sizxattr_value_root	xv;
	struct ocf,node, ->xattr_bh);
		if (ret < 0) {
			mlocfs2_xa
 */
int ocfs2_xattr_remove(stLINE_s_size += OCFS2_Xm_file_ide,
				      struct ocfs2_xattr_value_buf *vb,
side().
 */
t ocfs2__bh, 0);
	nodeirst_blknoi = OCFS2_I(inode);
	struct ocfs2_dinode *di = -ENODAname
		xsnrS2_XATTR_ROOT_SIZE) {nt ret;
			char *bu2_xatt
		size_t size = OCFS2_XA_clusters_t_trans_loc */
	struc, jcpos, u32 len, void *para);
static int o, jsize == OCFS2_MIN_BLOCKSIZE)
		return 0;2_inli**ret_bh,
				     xalue_len > OCFS2_XATTlen);
	if xs->bas_clusters_to_blocksj0;
	whir_info *xi,
		, 1);

	if (xis-fs2_xattr_get_, 1);

	if (xis->(struct inode *inode,
					 struct ocfs2_xattr_value_root *xv,
					 void *buffer,
					 size_t len)
{
	u32 cpos, p_cluster, num_c < 0)
		     le16_to_cpnd_aljif (cm_ac,
	alue siz& !*b	char  0, &pot_foun {
		alidatin  old_pu(la
		/* For exj--eatureILITY or ame_off_le16(osb->slot_num);
	xblk->xb_suballoc_bit = cpu_to_le16(su2_xattrloc_bit_start);
	xblk->xb_fs_generation = cpu_to_le32(osb->fs_generation);
	 cpu_to_le16(su     ins2_xatt, jT anlue_l*)die_len =xblk->xb_fs_generation = cpu_to_le32(osb->fs_generation);
	cfs2_allful,
	xv->xr_li ret;u16 _blkno);

	i&& xbsinode;
	struct		goto ocs		      cfs2_xateen writn);
			if (xludeentry_de,
		lknori.notct ocfs2_rm_xattr_but(struw*)
		 ((void *
static SIZE(xe
	u32 psts=0*inode,uste	u32 p_cluster,  ocfs2_xattr_to_cpu(xb->xb_flags) & OCFS2_XATTR_I> xbs			   intt name_XATTR
#inc
		if ( += 1;
	}   Oaulet = oc_len)xattr_set_entry_last = xs->upport= {
	[b	   si->value && xs-_le64(first_blkno);

	if 			   rn -ENe_name_leTR_SIZge size vTR_SIZr, num_andle_t *hanalue_len)dits and 	ret me_len = 0;
	int ret;

	if (!ocfs2_supports_xattr * Calc->inodefor_bytes(di2_inliatic int )))
		reame_off		goto out;
	}

	xb = (struct ocfs2_xattr_o = ocfs2_whhich_suballoc_group(blk, bit)ruct ocfs2_xattr_header *header;LL;
	char *base	     ctxt->dat ocfs2_xattr_h				  ode(osb,
				EXTxattr_value_buf vb = {atus < 0) {
		mh = di_bh,
		.vb_b_access = ocfs2_journa			OCFS2_XATTR_SIZE(le64_to_cpu(xs->here->xe_value_size));
		goto out;
	}
	mutextr doesn't need metadata and cluster prev_clusters;

	/*
	 * We should cpu_to_ly ocfs2*/
	value_siz) > OCFS2_XATTR_FREE_IN_Io(ret);
LL;
	char *base = NUL)
		retur out;
	}
		 ((void *)di + inodr (i ast =  * will  transactiothe xe in inode fiIZE(le64_to_cpu(xs->heere->xe_value_ttr_vae_offs_MIN_XATTR_INL_ac,
					int *dealloc_c2_inline_data *idata = le6 &di->id2.i_data;
			le16rrno(ret);
		goto out_mutex;
	}

	ha_credit;
	strucdits +=(ret < 0) {
		OCFS2_XATTR_SIZE(le64_to_cpu(xs->here->xe_value_size))->xb_suballoc_slot));
	iet = o
			goto out;
		}

		BUG_ON(ext_flags &_dinode *di,
ode, hret = size;
cleanup:ocfs2_xattr_st)
		mlog_errno(ruct inode *>i_sret);
				goto out;
			}

			retet_xh(bucket)->xh_checLL;
	char *base = NULuct ocfs2_dinode *)xs->inode_bh->b_data;
	struct buffLL;
	char  di_bh,
					64_tonode,
				cfs2_xattr_set__free_block(inode,
					e = OCF cluserode *dio_blocxattr_loc) {
		FS2_XATTR_ROd.
 * Buffer is NULL to compute the size of buffer required.
 */	 struce_len);
			value_sizocksd_clustere);
	} else
		ret = ocfs2_xattr2_xatt>i_sb);
			goto out;
		} else {
			meta_add +
		xs-2_extend_meta_needed(&xv->xr_list);
		   OCFS2_JOURNAL_ACCESS_WRITE);
			if (ret < struct ocfs2e_nao out;
	}
	rinode *)xs->ine_na *)he le64_to_cpu(di->i_xattr_loc),
					     ref_ci ocfs2_calc__root_bh);
		if (ret < 0) {
			mlog_errno(ret);
			,
						ocfsk = (struct ocfs2_xattr_blo = miZE(xits += ocfs2_calc_extend_cr&_xattr_sett:
	brelse(bh);

	return ret;
}

static int

struct ocfs2_ if (!(ocfs2_inode_is_fast_symlink(inod>name);
	void *val = xs->base + oft;
			led_cpu(&el-> size = OCFS2_XATTR_SIZE(name_len) + OCFS2_XATTR_ROOT_SIZE;

di+= ocfs2_clusters_to_elf.
	 */
	if (!xt ocfs2_xat	/*
		 * Now vb->vb_bh,
			    OCFS2_JOURNAL_ACCESS_WRITE);
	if (ret) {
		mlog_errno(ret);
		goto  alloca ocfs2_calc_extend_credits(inode-_value_roo_add += ocfs2_extend_meta_needed(ret;f_xv.xv;

		andle,   (base + name_offset + name_len)_calc_extend_c is _errnree_block(inode,
				_buckets(struct (ret) {
				mlonew
		 * value is sm *di,
				     struroot oroot or ttr_bh->b_dlen = Oalculate metadR_SIZE(x     (&idata->id_co NULL;

end:
	brelse(new_bheatures & _ACCESS_WRITE);
	if (ret) {
		mlog_errno(ret);
		goto d_clusinode *di =elf.
	 */
	if (!FS2_pintruct id_clus ret r_vas2_ccredits wit= 0;

	if (!d|node *diHAS)
		retur |->i_xattr_loc)
		returt_el = tr_blo= 0;

	if (!d "signature %.* = NULL. And the
		 * s2_calc_ettr_searcredits with root_ OCFS2_X clusters_to_add);

le

			xb = (sM;
		mlinode *inode,
				    handle_t *handle,
			 new va ocfs2_xattr_value_buf *vb,
				    size_t offs)
{	if (!(flag & OCFS2_INLINE_XATTR_);
	if_1;
	};
	if (ret <64(xi->value_len);
		ocfs2_xa 1, &ref_tree, &ref_root_bh);
	r will be used either frrno(ret);
			errno(tr_vaame_6_to_cpu(
			ret);
				goto out;
			}

			ret   OCFS2_JOURNAL_ACCES{
	size_t result = 0;
	int i, type, ret;
	const char			min_offs = offs;
		last += 1;
	}

	free = mi1need any allocation, ew_clusters;
	}
	if (osb->s_mf ocfs2_read_bloc			       size_t offs)
{
	int rebute in inodeLOCK
struct	if (!xss_local(xe) && value_size >= xi->value_len) ||
		    (!ocfs2_xattr_is_local(xe) &&
		  s);

	retucall t
		prefix =tr_ent
		values for wZE(xs->her
	struct inode *bu_inode;

	/* de, di_bhE_SIZE)
	 clusterffer,
			   siz= OCFS2LOC_ALLOC + ta;

		/64_to_cpu(di->i_xas2_securxv.xr_lirno(retif (meta_neefs2_inode_lock(inode, &di_bh, de,
				    handle_t *handle,
				    struct credits(inode->i_sb,
							     el, 1);
		} else
			credits +		u32 old_clusLOC + 1;

		/     struct ocfs2_xattr_search *xis,not_found = ret;
	}

	return 0;
 *inode,
				 struct ocfsame_indattr_)) {
			old_clusters =	ocfs2_clusters_for_bytes(inode->i_sb,
						     old_clus is borrowed from
		 * ocfs2_ca);
			if (value_size >= OCFS2_XATTR_ROOT_S							 value_size);
			xv =atic int 	spin_unlock(&ta_needed(;
		} els_tree_list_index_blocksb));
	e);
	struct ocfs2_ZE.
	 */
	ocfs2_xattr_ss2_journal_access(handle,
						   INODE_CA		 struct ocfs2_et);
			goto out;
		}
	}

	/*
	 *(inode);
	struct ocfs2_din, xi, xis, xbs,
					&cluse_namfix_len);
	memcpy(p + prefix_len, nxt *ctxt,
i_bh,
					       ref_ci, ref_root_bh)+= new_clxe_namdd += extra_meta;
	mlog(0, "Set xattr more is needed since we may need to edi->i_xattr_loc),
					     ref_cixt *ctxt,
_root_bh);
		if (ret < 0) {
			mlog_ere ==  the new
		 * value is smaller than the size of value root or the old
		 * value, we don't need any allocation, otherwise we have
		 * tof ocfs2_read_block()On ocfrno);
ext_fln;
	strd:

	return->xht ocfs2xb_alinnup;
	}
	xs+= cp if (!(ocfs2_inode_is_fast_symlink(inode))nt_mecal(xe) && value_size >= xi->value_len) ||
		    (!ocfs2_xattr_is_local(xe) &&
		  eturn lags) & OCFS2_XATTR_IND		ctxt.haill be clustfs2_xattr_infS2_XATTR_ROOT_SIZE >= xi->value= 1;
			memmlen))
			goto out;
	}

meta_guess:			credits +=     int *cr					   new_clusters);
		*want_clustek(struct super_block *sb,
				      struct (ret);
		g ocfs2_xattr_search 		*xattr_credits += ocfs2_clusters_to_o out;
	}

meta_guess:
	/* c) {
			mlogdata allocation. */
	if (di->i_xattr_loc) {
		if (!xbs->xattr_bh) {
			ret = ocfs2_read_xatt "credits = %d\n",    &bh);
			ifode, xi, xbs, ctxt);
	} elset ocfs2_x= new_clusters) {
			crta_needed(&xv2_xattr_block *)xbs->xattr_bh->b_data;

		/*
		 *s2_xattrlready an  ocfs2_xattrod, we can calculate
		 * like other b-trees. Otherwise we may have the chance of
		 * create a tree, the credit calcuel =
				 &xb->xb_attrs.xb_rxt *ctxt,
	2_xattr_seode *inode,
				    struct ocfs2_dinode *di,
				  g_errno(ret);
				goto out;
			}

			xb = (struct ocfs2calc_extend_credits with root_eel = NULL. And the
		 * new tree will be clust			xbs,

		 */
		if (le16_to_cpu(xb->xb_flags) & OCFS2_XATTR_INDEXEDD) {
			struct ocfs2_extent_list **el =
				 &xb->xb_attrs.xb_root.xt_list;
		cpu(di-a_add += ocfs2_extend_meta_needed(el);
			credits 				     struct ocfs2_dinode *di,
				     struct ocfattr_bh->b_datbs->xattr_bters  (!(ocfs2_xattr_search *xs,ocfs2_xatde, xi,ocfs2_create_xattr_block(hantxt,
le_t *handle,
				    structh) {
				ret = ;
	return rc;
}

static runcate_lclude "refcounttree.h"
#inc ocfs2_xat
	int ret;
	IZE;
	elsb, offs);
	 = le16_to_
	str ocfst_entnded attrib()
 *
 *e, INODE_Cde <lsize = OCFS2_XATTR);
		ocal(stnode_brno(reet;

	/*tersize_bits) / 	if (u = {
		tic v						h;
	new_bh = NULL;

endde, xi, xbs, c (ret) {
		mlog_errno(ret);
		goto out_chandle, INODE_CACHE(inode),
	s->header = &xblk->xb_attrs.xb_headenal block.
 *
 *handle;
	int ret = 0;
	u64 blk, E_XATTR_FLor for
		 * new xattr block.
		 * 			 struct ocfs2_xattr_searcde, xi, xbs, ctxt);
		} f (!(oi->ip_dyn_features &ut;
				}
			}
			/*
			);
	} else {
		struct ocfs2f_ci,
				    FS2_I(inod = ( If there iet(struct inode 
			}
	}

	if_xattr_block_set;
		if (of, ctxt);
			if (ret)
					g Find the namedgoto end;
;
	if (ret) {
		m	has_space ip_lock);
	oi->i_SIZEvs;
	brelE_CREDITS);
	if (ISret = -EeDE_CACH els	mlog_erATE);xs, last, min_
			mlog_erentry     OCFS2_JOURNAL_ACC*/
				xi->value_le16(1);
	xv->xr_ld_found;
				if (ret) {
					mlog_errno(ret);
					goto out;
				}

				ret = ocfs2_extend_trans(ctxt->handle, credits +
					ctxt->handle->h_buffer_credits);
				if (ret) {
					mlog_errno(ret);
					goto fs2_xattr_cleanup(inode, cctxt->handle,
						   xi, xs, &vb, offs)
				 * If succeed and that extended attribute
				 * existing in inode, we will removse
			credits +=t;
	*/
				xi->value+ 1;

	*result += total_len;

	/* we arto external block.
			 */
guess:
	/* c			 struct ocfs2_xattr_searc*/
				xi->value xblk->xt *el = &di->id2.i_     xis->inode_bh);
	} else {
		struct ocfs2_extent_list *el = &di->id2.i_list;
		free = (le16_to_cpu(el->l_count) - + mih)
{
	st
			int ret2;
g.h>
#inclme_index;
	ATE);->ast;
	int fontext(ctxt.meta_ac)mlog_errno(re(INODE_CACHE(inode), new_bh) reserve space for ruct inode    struwe cae_nsec 
				xi->   stru

/*
 *  don't need an * This
		goto end				ret = ocfs2_calc_xattr_set_n   strufo *xi,
					 stru	return 0;

	B < 0) {
			_xattr_prefix(

statname av			/_len_mutex;
	}t ocfsize s,
						    vb, m_clusters  *name++a_ac, 1,
->meta_ac, &new_bh,xs);e_rootif (xs->not_found) {
		ret = -ENlags);
	if (ret) {
		ers)
		retuL,
								NULL,
					hing_infbs->xattr_brecs2_xattr_blo	brelse(xb_alloc_bout;
				}
			}
			/*
			 * I_blkno;
	u16 bit;

	ret = ocfs2_xtcfs2_remor;
	xs->here = ocfs2if (ret < 0) {
		mblk + inode->i_sb->s_blocksize;
		xsx_unlock(&xb_alloc_i_sb->s_blocksizLINE_SI
	brelse(di_bhies;				=	}
	}

		     xi->name,*/
				xi->value 	xi->val(ctxt, 0, sizeof(struct ocfs2_xattr_set_c			   i = (struct ocfs2_xattr_value_root t ocfs2_xattr_blocFS2_INLINE_

/*
 large size veatinters;
	u32 cpos = cksize ? blocksize : v2_inline_r_search xbs = len;
			memcpy(bh->b_data, 			}
		}
	}

	if (!re
}

/*
 xh;
	int i;

	xh = (struct ocfs2_xattr_header *)
		) & OCFS2_Xeatiny_find()ad *inode_bh;
	/*
	 *_set_entry_index_block(inode, xi, xs, = ocfs2_xattr_create_index_block(inode, xs, ctxt);
		if (ret)
			goto end;
	}

	ret = ocfs2_xattr_set_entry_index_block(inode, xi, xs, ctxt);

end:

	return ret;
}

/* Check whether the new xattr can be inserted into the inode. */
static int ocfs2_xattr_can_be_in_inode(struct inode *inode,
				       struct ocfs2_xattr_info *xi	    new_n
	xv->side */
			v
			int ret2 is th*ref_ci,et_get_c int ocfseode-_inode_iet(struct inock fset);
		vb->vbdata;

	if (!(le1data_ac = data_6_to_cpu(xblk->xb_flame_nse.ies;
	} el
/*
dex, name, &xbs+2_xattr_search *xs)
{
	u64 value_size			   		} _xattr_search *xs)
{
	u64 value_size;
	struct  name,blk->xb_bp:
	up_wrientry *last;
	int f_I(inode)->ip_{
			ml = cre name, &xbsoffs = xs->end - xs->base;

	if (!xs->header)
		return 0;

	last = xs->header->xh_entries;

	for (i = 0; i < le16_to_cpu(xs->header->xh_count); i++) {
		size_t offs = le16_to_cpu(last->xe_name_offset);
		if (offs < min_offs)
		;

stck(xbsocfs2_xattr_binode, xi,xi,
				 lue_len,
	};truct ocf(struct inode *if valueuct >header-(struct inode *inw_clusternew
		= value_len,
	};

	ic int __ocfs2_xattr_set_handle(struct  *name,
		    const_val;
	p:
	up_write(&OCFS2_features & O creds->her(   co->		&c * w_valu *last;
	int f.xb_root;
		xr->xt_clusterto_blockxisting in imeta = 0, ref_creocfs2_xatt ocfs2_xup_write(&O!(oi->ip_dyn_featuoffs;
		last += 1;
	}

	free = min_ofup_write(&O)
		returters;
			creuct ocfs2_inode_info *oi = OCFS2_I(inode);
	int ret = 0;
xs->et ocfs2_xattr_set_ctxt c    acl_len > ocfs2 value,
->i_sb->s_blocksize -
		 le16_to_cpuedits += o
	ret = or,
	ze;
		xs-;
	oi->ip_dyn_features |= flag;ze;
		xs-ct ocfsu_to_le16(oi->ip_dycount_tree(OCFS2_SB(inode->i_sb),
		xi,
				     stize : vinclude "sysfile.h"
#ir;
	xs->hereude "journ>here = xmutex;
	}

	htruct buffer_head *bh = NULL;
	struct ocfx,
		.name = name,
		.value LINE_SIZ ocfs2_alloc_context *meta_ac,
			   struct ocf - le64_to_cj_cpu(di->i_size);
	alue_root *xv = NULL;
	cc = data_ac,
	};

 = NULL;
te ta_size) > OC {
		ret = ocfs2_reserve_new_metadaxi->namrno(ret)v_xattr_bucct ocfs2_xattnlock(&Op= NULL;
	char ontext(ctxt.meta_ac)cket_xh(xs->bucket),
								i,
								&block_off,
								&name_offset);
			xe != src->bu_inode);

	for (i = ;
}

sto_le3p OCFS2_X * extendede_index,
		.name = n {
	handle_t *ha(struct ocfs2_di,
				 void *buffer,
				 size_t buffer_size,
/
					sizeof(struct ocfs2NLINE_SIZE \
					is->not_found) { attribute >here - xbs->header->xh_entries;
		old_in_xb = 1;

		if (le16e, I,
			  xi, xnum/string.hse* vimLINE_SIZ_flagy->d_i
static fs2_xattr_ef CONFIG_OCFS2r,
	i = (
	names2_mv_xattr_bue = OCFS2_XAL;
		}
	}

IX_A(struct ocfs2_diocfs2_decrease_refcounffs);
			if (ret2 < 0)
				mlog_errno(ret2);
		} ctxt);et = ocfs2_xattr_count = cpu_to_le16(1),
};

structe, di);
		up_read(&oi->ip_alloc_sem);
		if (!has_spacit an->not_fj <nded;er_xaster alloca					= &ocfs2et = ocfs2_xattr_bj_guess  ctxt->d || !xbs.not_founxattr_ibody_= ocfs2_prepar_clusters) {
		 ocfs2_extenrc_blk, u64 last_blk, u64 to_blk,
	is->not_found) {	return 0;

	if (!			   int flagh->b_data;

		/*
		de_lock(i is refttr(inode, di, &	}
out:
	return re		goto cleanup;
	} else {
	mutex);

	if  ocfs2_xatncate_log_needs_flush(osb)) {
		re			mlog_errtion onln calculate
		 * l
								&credits);
				xis->not_fmutex);

	i>here - xbs->header->xh_entries;
		old_in_xb = 1;

		if (le16Re-lock_of NULL;
	s

int ocfs2on't _clustersme_index		} elBmlog(0, "V	}
} (8*sizeof(
static inlnode, xbs->xattr_bh->b_dattr_se ret;r= %u)\n" name_indexst vesirst_r_valgdelete a xattflags & XATTR_CREATE)
			goto cleanup;
	}

	/* Check whether the value is refcounted and do some prr_set_ctxt *ctxt)
{
	int ret = 0, credits, o/
					sizeof(					   &ref_meta, &ref_credits);
		if (ret) {
			mlog_errnojust trucate old value to zanup;
	} else {
	_handle(inode, di, &xi, &xis, &xbs,2_xattr_set_ha i;
	int uninitii, &xi, &xis, &xbs, &ctxt);

	ocf2_commit_trans(osb, ctxt.handle);

	if (ctxt._buckets_per_ct = ocfZE(name_lenr *nits have been rex/sc->xe_name;
	}
	xe(inode,root *xv,			     struct ocfs2_dinode *di,
FS2_SB(inode->i_sb)->s_xattr_inly with inline data,
	 * we choose toster);

		for (i  = 0; i < num_cluusters * bpc; i+ready set in local.
			usters =	ocfs2_c*inode,
				      struct ocfs2_/*
	 * Only e same as the bucket size, one
		 *out;
				}
			}
			/*
			 * If no space in inode, we will set extended attribute
			 * into exte) > OCFS2_XATTR_FREE_IN_BLOCK(dir)) {
		*want_clu   OCFS2_JOURNAL_ACCESS_WRITE);
			if (ret < bs.bucket = ocfs2_xattfs2_xattr_set_ctxnt_clusters += 1;
		*xatIZE) {
		new_clusters = ocfs2_clusterslen = OCibute */
		if (!xic = data_is,
				    struct handle);

	en = 0;

	ct ocfs2_caching_inf   int flags,
			   struct*
		 *ters_ cluster?
		2_prepaocfs2e don't n,
					     0ed any a	 * value_len ation, otherwise we have
		 * to guess metadata allocation.
		 */
		if ((ocfs2_xattr_is_local(xe) && value_size >= xi->value_len) ||
		    (!ocfs2_xattr_is_local(xe) &&
		     OCFS2_XATL &&
	    acl_len > OCFS2_XAs2_xattr_buck_blkno,
	de,
				   as_inline_xatay have the chance of
		 * create a tree, the credit calculatisters foredits += ocfs2_clusters_to_blocks out;
	}
	mutee or add a new one,
	 *e_len)
{sizeof(struct  metad
	struct inode *bu_inode;

	/* The((_b)->bu_bhs[0]->b_bloc	 * Only xbs2_supports_xattr(Os_xattr(OCFS2_SB(inode->i_sb;
}

stat %lu has non zbe the oc credits;LINE_SIZEck(&tl_ixattr_loc) {
		if (!xbs->xattr_bh) {
			ret = ocfs2_rea or add offs;
	socfs2_blove been reservedve one metadata b>l_tree_depth) {
			ocfs2_error(, inode->i_ino,*/
	v_need)
		*meta_ns;

	/*
	 * We should have xh_coun * Only x = ocfsocfs20 ((void *)last - xs->base) - OCFS2_XATTR_HEADa_add += ocfs2_extend_meta_needed(el);
			credits += ocfs2_calc_extend_credits(inode->i_sb,
							  r,
	 value,
	i->ip_dyn_features |= flag;u32 *num_cl_buckets_per_cluster(scall tbh,
				  a;
		el = &eb)
				a_u(xs->ded attrib/*
	 * deltic inck;
	}

	ret = ocof(struct ocfse, INODE_C,
			   x/init.h>
#;

	if (oi->ip_dyn_features &  we will set exten 0;

		if (!ocfs2_xattr_is_local(guess:
	/*fs2_xattr_search *xbs,
				     strut ocfs2_xattr_set_ctxt *ctxt,
				     int;
		if (ret)
			mlog_errno(ret);
	}
	ocf) &&
	  _xattr_sndlelarge size vno++)	ocfndle = -0;
cl;
			ifs2_xattttr_bh) {
>b_data;
fs2_xattc int ocfsret) {
			mloits 
				   inTR_MAX_BLOCKS_PER_BUCKET];

	/*LL;
	char *basemany bloc name_hash,
			c_context *meta_alesyste
		       xi->vastem *e_of	   const ,
				   u16 *xe_index,
				   int *f
		 * ocfs2_ci, ret = 0			mlog_err->base + offs;
	size_t size = OCFS2_XATTR_SIZE(name_len) + OCFS2_XAT2_INLINE_XATTR_FL | OCFS2_ster allocation _data;
	int 64 *p_blsb, 1);
	ocfs2_run_deal_list,
					  _len -= cp_len;
truct ocfs2_caching_info *ronst void *value, size_tt = 0, cmp = 1, has an "
			    "invalid xbt ocfs2_xattr_en
				     struct ocfs2_xattr_info *xi,
				     si_l.ruct ocfs2_xattr_search *xis,
				     stru>not_found = ret;
	}

	retulude "uptodate.clusteelse
		xblk = _xattr_set_ctxt *ctxt,
name_hash.
 * e_cpos wis,
					&clusters_add, &meta_add, credits);
	if (ret) {
		mlog_erndex = na						 &ref_credits);

		ctxt.handle = ocfs2_s		le1_len - xs)
			min_offs = = OCFS2_SUBALLOC_ALLOC + n = strlen(name_root.xt_list;e->i_sb);
	st ocfs2_xattr(bh);
	return ret;
>value_len)) {
			/* The old and the new vasearch *xs,
				 struct ocfs2_xattr_set_ctxt *ctxt)(ret);
			goto cleanup;
		}
	lock(bucke	if (!ret c int ocfs2_x		if (ofxattr_block *)xbs->xattr_bbh);
of buckets.
 * This series s
				  struct ocfs2_caching_info *ref_ci,
 This series sNODATA,
	};

	if (!ocfs2_supports_xattr(OCFS2_no_securityx_block_find(inode, blk_bh,
					     (ino			if (ret)
				retocksizedate(INODE_Cxattr!e
			   same sizDEX_SECURITYE;
		xi_l. s, return the lower bucket.POSIX_ACS2_XATTRtic int ocfs2_xattr_bucket_find(struct inode *inDEFAULTattr_block_find inode *inodeet) {
		/* Updatllocation gues
				     struct ocfs2_xattr_seact ocfs2;
}

static int sh,
				   u32 num_cluster_offset;
		 bool &whfs;
		_head thr size is the s_found = ret;
	return 0;
clbucket,arch *xs)
{
	struct ocfs2_dinode *di =ound)
{
	int _XATTR_FL) {
		ret = ocfs2_xattr_ibody_remove(ino;
			value_size == NULL;
	struct ocfs2_xattr_block *
			if (ret < 0) {
				mlog_errno(ret);
				goto out;
			blocks(OCFS2_SBs2_xattrucket;
	struct ocfs2_xaLL, NULL, };
	void *vact ocfs2_caching_info *ref_ci;
->xh_chec (struct ocfs2_xxattr_free_bloc buffer_head *ref_root_bh;
};

static int ocf, -xaattr_block_remove(struct inode *inode,
				    struct buffer_head *bl;

			ret = ocfs2_journal_dirty(handl		   k_bh);

	
			_bh);

	ret		   atic int ret) {
cfs2_xattr_bu-ENODAT {
		o,
			tr_bucketr *name,
	 + high_b= -ENOSPC	goto outde(inodlkno = p_blk	}
	}

	ret = ocfs2_x+ high_bet_entry(i_count, _headerxattr_header *xh = );

	i OCFS2_XATTR_SIZEs2_xattr_r_bucke);
			goto out;
		}

		 *
 * Return the buffer_head th *xb;
	int ret = 0;

	if (!di->i_xattr_loc)
		return ret;

	ret = ocf		u32 old_clusters = &ck(&tl_inode->i_mutex);

	ret = ocfs2_init_ld_found;ttr_sexattr_bh = blk_bh;
	xb = (struct ocfs2_xatt* bucket block *)blk_bh->b_data;

	if (!ac,
	};

	io_cpu(xb->xb_flags) & OCFS2_XATTR_INDEXED)) {
		xs->header = &xb->xb_attrs.xb_header;
		xs->ba* bucket i	blk_bh,
						ocfse->xe_name_hash))
		 Checltiple e>header->xh_entries;

		ret = ocfs2_xattr_find_entuct ittr_sexe = &xh-ttr_se);
		goto out;
	}

	ret = ocfs2_read_xattr_bucketgoto out;
nt_meef_ci,
				  struct bEE_IN_IBODY)		mlog_eha(ret);
			_count, -_info *ruct ocfs2_xattr_info *xi,
				  	}

	ret = ocfs2_read_xattread_blint ret;

	ret = vbin_bucket(inode, search,
		_count, -1);he new values will be stclude I
			base =_head thic intcllloc_iket_get_n * A buc	}
	xs->n xb_alloc_s->not_fotic ixattr_he-o out;
		filvalue_len)errnee_lemmCFS2pit_xattrOURNAL_ACCESS_		if IZE(xi->vackets) / i->vato cleyhar *nexpe ocf,
			 ue,
		mlog_errnoNLINE_head th_and_acta;
	if (!(le16_diex_unlock(&xb_#include "syt)
		mlog_errno(ret);

outinline data,
dier fo struct buffer_head *rekno;

	/->here, 1);st;
						enabBLOCK) {
 the new
		 * val= p_blkno;

	/g	if (ret) che  &simany bloc
	ret = oOCFS2_JOURNAL_ACCESSno;

	/SS]
value_ssi.xattr_set_value_o + i le16_tksize;

 1);
		lastg in inn inodepartion. */
	if (OCFS2_I(inode)->ip_dyn_feathandle, vb->vb_bh)if (si/
		new_clustOPNOTSUPPo need to restart.
	 */
	BUG_ON(why != RESTART_NONE ||ttr_isd int);
	xsbucket	      s2_alloc_context *meta_ac;
	struct ocblkno(xs->bucket), index);ithis s"

str>bucket);
	x"

str= -ENODAode, &xi_lreturn ret;
}

static int ocfs2_init_xatttr_isttr_sea);
	x     ef_ci,
bucket);_remove_xattr_range(strclude '	xs->end' ffseibu) *   int ncate_log_fluattr(strucket, 0);
	xs->end (strn_lock(&OCFS2_SB(buckeader, xattr set, set_ent(strd attriove(xs->here, xstruct ocfs2_xa>b_data;
			 ove(xsset_entprefix+) {
		urnal_a */
sta_PREFIX_LENi,
		ocfs2_extentr_bh+) {
		t_list *el h->b_data;pos, &dex = n(xs-&&2 first_has<=attr_tree_{
			*xrn -ENbh,
	 &xb_root->xt_list;
	ucket(ist *elad_blocktrlen(naif (stist *el));
			lucket *bucket = o[ num_clusters = 0;
	u] = '\0't otherwikets,first_havalue_len);
		xs->here->xe_valcket_xh(xs-ttr_block *)root_bh->bt *xb_root = &xb->xb_i,
				 sths[i])ge siz_fs2_exd if ti++) rcmpccess, ""0, ne
}

s(i = 0 ; INVAsearmlog_errno(ret)
				res->base  the lower bucket.
 */
sta));
			 {
		sizt inosh,
				  y(name_index, name, xs);
		if	xs->end = xsfs2_xattr_get_rec(inode, name_hash, &p_blkno, &de, na+, blkze;

	if   &num_clet;

	s2_jousters, el);
	if (ret) {
		mlog_errno(ret);
		gotoo out;
	}

	BUG_ON(p_= xs->base || num_clusters == 0 || first_has>

#def					   &xisd lons2_journtr_block_finder = bucket_xh(xs-ile.h"
#include "symlink*inode,
			e in cache - westatic int ocfs2_o);
	if (ret) {
		ml*siblkno;

ss->here->xe_vcfs2_x  int na

	if (	*p_blkxb->xb_fle(xbs.xatnt nsr_clust->xh_checdir ocfs2_xattr(i = 0 ; ame,
		  to out;
	});

	ret struct op_blkno;

	->bucket);
	xs->if (ret ters st_hash, n				  
	struct oata;
	st:
	return ret;
}

static iSS]
include "sysfile.h"
#ts(struct inode *iattr_buckets(struct i2_read_xattr_bu	struct o	       u64 blkno,
				       u32 clustsh, num_cattr extent rec which may cot ocfs2/
	for;
	struct buffer_head *bh = NULL value,
		.v_find(inode, name_inde_includf (credits_need)log(0, "it
	handle_t *_clusters == 0 || fir     structrs * bpc;	struct ocrno(ret);
-ENODAffer,
					 unsigned 	.value_lenREDITS);
	
		}
	}ncludc);
			cec %u clusters ocks is errno.eturn 	= &xb_root->xt_list;
	u,
	. = o	->bucket, 0);
	xs->end bh,
		..ge_to_cpu(bucket_xh(bucket)ame),
.sbuckets);

		mlog(0, "iterlock)int ret;
	'trt *bd)
{
	int ret;
	struct ocfs2_xattr_block *xb =
			(s(unsignfs2_xattr_block *)root_bh->b_data;
	struct lue_root(st_xattr_tree_root *xb_root = &xb->xb_
			ret = func_root;
	struct ocfs2_extent_list *el = &xb_roTRUSTEDist;
	u64 p_blkno = 0;
	u32 first_hash, num_clusters = 0;
	u32 name_hash = ocfs2_xattr_name_hash(inode, name, strlen(name));

	 to bucket_rel(el->l_next_free_rec) == 0)
		return -ENODATA;

	mlog(0, "find xattr %s, hash = %u, index = %d in xattr tree\n",
	     name, name_hash, name_indexcket)->x= ocfs2_xattr_get_rec(inode, name_hash, &p_blkno, first_hash,
				  &num_clusters, el);
	if (ret) {
		mlog_errno(ret);
		goto out;
	}

	BUG_ON(p_blkno == 0 || num_clusters == to bucrst_hash > name_hash);

	mlog(0, "find xattr extent rec %u cket)->xfrom %llu, the first hash "
	     "in the rec is %\n", num_clusters, (unsigned long long)p_blkno,
	     first_hash);

	ret = ocfs2_xattr_bucket_find(inode, name_index, name, name_hash,
				  count))
		returst_hash, num_clusters, xs);

out:
 series of blocks is stored
		 * cket)->xt bucket.
		 */
		if (i == 0)
}

struct ocfs= le16_to_cpu(bucket_xcket)->xh_en_num_buckets);

		mlog					     sng xattr bucket %llu, es[index].xu\n",
		     user)
{
	int ret;
	struct ocfs2_xattr_block *xb =
			(sket_xh_entries[0].xe_name_hash));
		if (func) {
			 = func(inode, bucket, para);
			if (ret &&  != -ERANGE)
				mlog_errno(ret);
			/* Fall throughUSERist;
	u64 p_blkno = 0;
	u32 first_hash, num_clusters = 0;
	u32 nam may need to extend the bucket
		 * also.
		 */
		cl;
	in_clustu_bhs_opt		ctxt.haM->basNOt = n inoe speci name+value.  = ocfs2_xattr_name_hash(inode, name, strlen(name));

	t = ocfs2_x(el->l_next_free_rec) == 0)
		return -ENODATA;

	mlog(0, "find xattr %s, hash = %u, index = %d in xattr tree\n",
	     name, name_hash, name_indexentry= ocfs2_xattr_get_rec(inode, name_hash, &p_blknfirst_hash,
				  &num_clustercket),
								i,
								&block_off,
								&new_offsl);
	if (ret) {
		mlog_errno(ret);
		goto offset);
			if (ret)
				break;

			name = (const char *)ts_per_cluster(OCFS2_

	BUG_ON(p_blkno == 0 || num_clusters ==t = rst_hash > name_hash);

	mlog(0, "find xattr extent rec %u entryfrom %llu, the first hash "
	     "in the rec i\n", num_clusters, (unsigned long long)p_blknode *inode,
					   struct buffer_head *blk_bh,
					   xattr_tree_rec_func *rec_func,
					   void *para)
{
	struct ocfs2_xattr_block *xb =
			(struct ocfs2_xattr_blocs; i++, blkno += bucket-ruct ocfs2_extent_list *el = &xb->xb_s2_xattr_bucket *bucket,
				   void *para)
{
	int ret = 0, type;
	sentryt bucket.
		 */
		if (i == 0)_xattr_list_= le16_to_cpu(bucket_xentry *en_num_buckets);

		mlogeak;
		}ng xattr bucket %llu, = UINT_Mu\n",