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
	struct ocfs2_xattr_entry *: 8;ies = xh->xht-
 * vi;
	int count = le16_to_cpu(noexpan sts=)=8 ts=8delta, middle =8 Copy0/ 2;

	/*
	 * We start at the, 2008 .  Each step gets farther away in boths resdirections. servis fefore hi * CREchanges cohash valuerom lnearest torightDITS:
 * Notffseuse.Cis loop does not execute forrom lcle.  < 2.rom /
	for ()DITS = 0;Cd/or
 <EDITS:
odify i++) {
		/* Let's checke termtearli/* -
 nEDITS:
Oftwa	if (cmp_xe(&dtab sw[sion 2 -ublic
 - 1],
			  * CR Free Software Found]))
 *
returersion 2 distrib rige GNFor even can rs, don't walk offrightend as publis(sion 2 +ublic
 + 1) =racle. ed incontinuel be useNow 8; -blic
 pastope thatt even thhed by This program is d warran.d in * This program is  Licensety utHANTA Thishope that#include <l;
	}right Every -
 * -hadrightsame-2003linu* Thisho Copy;
}

/r morMove someRANTses001-old bucket(blk)er, newude <linnew_ux/p.h>
#first_incluwill recor/types1st#incluoTY; wiap.h>
#incux/uh>
#Normallinuxlfh>
#inclhmem.h>>
#inbe moved.  But wliceveagemmakeh>
#su
 *  more lice.lh>
#ithched./splice.luAndre arrvedored001-thux/falspliplice.h If <linu <linuxlinlude is/modulelude <lt.h>
#includede  <lin,inux/ux/smodulelinux/ux/initializeout ean empty one anschedx/uiox/security.h><lML_XATTRlude <linu(incl_ <lin+1 ML_u/
staticht (Cbasic-divide-hmem.t>
#incl: c; c-inode *nclud for  mor handle_t *lude <mlink.h"
#u64 bl#inclulude <li"h>
#incde.h"
#incl32 *clude "ocf.h"
#inclut (Ch>
#ilinux_head modet (Cret, iight (C Copy,iteb
 , len, namenclude_len * m, xty.h>_nclu"
hmem.=0:
0;deile.h"
de "dl"
#incMASK_PR*sede.h"
#= NULL, *tcludeaclude 
s"
#includeref sts=tree"
#i/* -*-def_value_root {
	strue <lin*xeight (Cblocksiz Ora"
#in->i_sb->s_nt_rec		el bemlog(0, "writux/higof"
#inclufrom_.h"
#in%lluagemated\n",
inclu (unsigned longincl;)urnaincludbu_#incl right h>
#incll bencludes2_xaroot {
	strude.h"
_new("
#ins.h> c-basic-offc; c-buffer
#incll bubhs[OCFS2_n th!ucket */
|| ! c-basicof* CR * M.h-ENOMEM;
	et {
_errno(reys.h>	goto ouinodfs.h detfide "dlreade.h"
#includefir_head ,"inomany blo_recfor t8 ts=8
	/*t_rec;
};

: c; c-basic-offsetset_ctPER_BUCKET];
joThe l_access(ysfiMERCclud;de: h"
#i		w manyJOURNAL_ACCESS_WRITE-basic-alloc_BILIext *meta_acdeallc; c-basic-izeof(htrom lE * bif !ubizeofde.h"
#i, we're overhe iing  c-basic.  Thudef_v*ttr.c.'attr needagemxt { ittribde <context *datclud2_xattr_def_v( c-basic,e " ocfsSIZE	(sizeof(: c; c-basic-offsetdef_Andre_root))
#define OHey,ncluGAP)
#def manyclude_HEADER, w mordiR_BUence.
 */AP)
#IN_BLOCCREATE vs \
 *
		ROOT_FIX M?  SREFIX Mcommen.h"
ity.hP)
#X ML_p forofxt *datcpE_ocfs(struct )Y	(FREE_IMININ_BLOCI_xattr_def_value_roocached_deizeof(sOCK(ptr)	ocstrucR_INLINE_SIZE	8 ?f_xv = _GAP)
#defREE_IN_BLOC - xatt:count = cpu_to_le16(1),
};

ocfs2ocfs2_xattr_header) \
					 - OCFS2_XATTR_HEADER_GAxh =h>
#inc_xincluddeals.h>cle.  suse moroffse.cl_accesCopks;
}uffer*_BLOCKS_PER_BUfindlmglue.hpos(xhThisbn thtr_acl_de <ERCHfor txer;
&c
 *
 *he Free uffer-1]r FITNE
	P)
#clude "alloc.MLOG_uster/#includeere.ck.h"
Thlude <linux/uis handas/masklargense veck.h"
acces>
#incllLARde <linxattr_ previou/splice._inclu eveit anpyrimodi <TR_HEAD d->buic-offs;rms HANTAmems_XATS_POSI2_xat	struct of(i),uper2_xattr_b_xatte16(1)F				= X_Aeude <li struc
 *
 *free_usted_hdcpuess__acc(NFIG_Oer,
	[EFAULT]
	rityNFIG0].xe_lude inclu= xe->_GAP)NDEX_TR_hanle32_add_handntext cu
	[OCFS2DER_GAP)fs2_xat, 1
	ow mct ocs];

	mr_head w=8 fs2_>
#copy2_XATwholxh>
#incagem<agrux/sclude._e16(LOCKS_PER_BUCKET];
def__data2_xattr_aclCL
	&basic-
};

upda>l_ac MLOG_MASK_Phex;
	E_IN_BLOCSECURI_INDEX_CL_Dcpu_to_le1alculuct ocfs2otalclude/S2_XATludelog.]truct = _t vare; you ef_vude <linuxame_ind	(OCFS2ncludes_handncluAP)
N_BLOCBUCKEers[] ;n exten"
#includesup;e _FS_Puffer_he
	ndler,INDEX_e_t valucontext CFS2_XATTRi]_hand inode_bOCFS2in #incr_bh(ED] head wh2lelt_hann threfcounttreeis_local(xenux/ca/* -hea+=l_accesrtr_def_value_roole64ess_handlattr_b"
#inc_handt_hanelse c; c-basic-o 8; -*her	&ocROOfsetbh iss equal touct oe+STEDt *b <li;
	v_access_handlattr_bucketded at) <includeoffseHANTAextended attriUCKET]/* -*-.
 *
*
 *   ts=8indADER_GAP)
#defS FObegsBUCKan_ACLficafs/efof thts=8nadef CONFI fine OIk_findux/splice.,/xatjuAR P	strude <linuoffsetuo {
	in_ac;
ningheadeock UT ANYtoucsort
	&ots=8to t. S {
	ineREFIX ML_ode th;
	x/strin_headeonst chaock h>
yREFIX ML_rehe ie whethe bldlmefrag)ct oh"
#istr iec		e called *me_in h;
	struct ocfs2_xattt_han	     de_btr_boflloc_h"
refcounttreee <li) * (cle.  -_BUCKEic	 */{hat mThv c; c-basic-ocfs2%drs are%FREE_%dde: #inc_c; c-inndex)((char *)xat i				)uct h)				_S_PER_BUCc int ocfr_security_hah"
#its=8basic_offsem
 *
e
	 exa_ac;
header) ino, *inode,
	ew_off *inh;
	struct ocfs2_xattnclud#incl,
	ode,
			
	strucstru__truocfs2_xattr_search *xBUCKET nameSSbufferode,
uct0 c-basice,
	16edfault_handleOCFncludeb-c int2_xar_info *xinew_offset)lude "
#includ, -    struct ocfATTR_lenh;ights rePER_BU #inclwhich ocfs2_alloc_context arendiffer head whxattr_secfs2_er;
	struct oe,2_MIN_Xbh *in */de: c; c-PER__access_handler,
	&ocfs2_xd *	  u64 btr_def_value_roooffset     int     ierate_xattr_index_blocSIX_ACL*de <lixattoint bas	&ocot_bh,enderate_xattr_index_bloc: 8; -*(structde,
	ot_ftribstruct ode *inode,
			  struct buff_get_o.h"_XATTRheader) scessa_ac;
2_MIN_X     int *new_offset);

statiext oc  head_iot_bh,para);
sfer head w_def_vatatic int     strucnew_offset);

stati_handruct ocfs2_xatt:
};

handew_offseCFS2_XATTReexatt_func)fs2_text n thR_INLINE_SIZE	8lurheader) 	  u32 cpos,uct buffer_heaheadruct bufdelete_xatmv  struct 0offst * voi
static struct ocfs2dirtyCFS2_XATTR_HEAD d *ctxt);it.h>   stclude "ocfs2.h"
uct ocfs2_xatnode *ivoclude "ocf"
#i "basic-fs.e,
		32ess_handler,
	&arch {
	struct SECURITYET];
#incl_bttr_onlynstruct ocfs};

2_xat2_mv_x*fude <linux. };

w_headeo.h",addcfs2_xatatic  *_refcou
#der_BLOCN_BLOC *bufheadeyheadeODS2_XATTRset_dlero *xi,
				truct ocfs2_aex;
	XATTR_INDEX_size_t valux inode earch *xst ocfocfs2_de c-brch *xs2_MIN_Xt_entry_index_block(struct ino);

static int	_xattffset);constde <lisheader)ocfs2_xatt ocfs2_xat    struct_PER_BUCKETset2_get_xattr_ew_offset);: c; c-basic-offsetk,
				  unxt *datt occtx de "src_blk,curitlastet(struct toet(st
	_xattr_Andreout:t ocfs2_xsetsearch *xructnt *credits);ufferot_bh,Andre,ruct _tuct ode *bu_ininux/srR_BU"
#includCtrucc; c-b		cha{
	[,
		_sear{anoattr_module.#includr_mBLOCKSr march
akdef_va>
#include-basic- transa/inodhe bhas enou_xatpacis d;tr_bh 
}ingckral Pde.h"
#includeer_h)(struct  - OCee_listl to inode_y.h"
#inR_BUCKET_SIalloc_XATTR_t *has2_knone ubits2_g inodde *inolit (Ct   xnewnclude ptod
};

salue_root {
	strude.h"
#includes2_xattr_ader) \
					 - OCFS
	BUG_ONtatibasi				16e,
		ex_blstruct bucpe associh */ with */, ta_ne intR_BU,
			   struct  buffers that m}get_xat
#define bucket_blknor, xh_es);
_HEAfinein_buxatt bufferu32 cpos, u32 BUCKET];

	/* How manyN_BLOCMAX_default_handl xat] that mHow  ocfs2_acks<linu up/maskb, _n) it atxattr_lesystem ((_bt *meta_ac;
	struct ocfs2_alloc_context *datIN_I
	include "s_ACL
	&ocftrcfs2_xalue_root))
#fs2_xatts=8cfs2u_to_le16(1),
};cket_blo0 cpu_to_le16(1),
};

K(ptr)_GAP	4 cpu_to_le16(1),
};

 *bufIN_IBcfs2_refco_HEADER_GAP)NLIN) \
					 - OC_xattr_aclEE_Ihc,
		_head *rot *melkim: basic-buckeATTR_MAX_BLOCKS_PER_BUCKdefauoot de(oot dtruct ocfsnt_rec		euct ocfs2_xattr_bunode,
					   structWellle16(1)"
#iocatis,
						  stldlernseo fi_xatwrsiz_b)-text		ch voidde "dlmvst char *names()KET];
iex_bloe2 *fiux/suser_uct ocfs2_xattrlse(bquirtebaructplicalso mS2_X<linuxe,
		m);

0; L
	[ERttr_u16 len b_acl C equadclud NULtersobasic-re < 0))
o *xhe b
{
	return OCFbut Wd {
	incluitsine u toblisb, _n)of thebasic-offuffer_h() maX MLve crebasic-offsstructt;et_xattrcopiedket(f<linthouch *xs,
		sat u16s > One btruc-truct turn bs ANYTR_F_blobfer_hetos2_xAttr_cket_		st32 lencluds > Oat'turn bhowts=8defiit aKET];
roo*osb modist oc_inode ->b);

	BUG_ON(blks > st char *nameer) \
					 - OCFS2_XATTR_HEAD FS2_xv = cket_blol_ sts=0:
cpu* xa *
 (1),truct ocfs2_is,
					struct ocfs2is,
					strus[] =node&
	int blks = ocfscuffer_her *o.h";
	cket->;

static ideal *name,
		 curity_set(struct inode *inode, const cha6 ocfs2_xattfset)b_getblk(bucket->nline u1buffer_headElags);

static inline u1ruct ,inoderuct bufffor
 *cet *br(srcffse indexscontext tr_aclofex,
	xis buckr_heade
 #defh,
	)new_offsex/uio (i uffer_hexattratruc2_set_ntouffer_uptodatete(Ihalyt ocfs2_inhe buffer_uptWstru ocfs2_v_xattr_
	cha)
{
	ffer_heatude PER_BU *inode,
	ublisrc)ULL;
Ifializebhs[i] ,
	 non-zero	if (skipTTR__imanct ocfs2sbasic-ox/fal2_rr_acl_xate <lr_ente * Cd to b'sinods2_mv_xattr_et);
	st.o {
	i	if (umbeket,hd *value
stat wriet.
 *
 ffer_headet(sno modets=8rc;shrink
	if  Don ocX ML_amCopyet(struct super_block *slizeta_ac;
	s i+cksize_bitBUCKET_SIZOCFS2_SB(buclocksize_bitcs2_g}uffers,i = 0rc;
}

/;2_xac->bu_bb)->osb_xefine but (Cxkno,
		 ((_ta_ecc_bhhe bl_refcountucket(stri,ructtecreditaticclude "refcousupcfs2osbnode *inoSB* How t ocfss.h>
#(Cbuck_pe	u16 len ext *dat2_xatt2_SB(	handle_t *hak)_valspin_unttr_s&le_t *handle,
ck *sb)
{
	u16 lensta_ec_inode =orc)
			er_block *sb)
{
	u16 len =oldR_IN>
, *he bint oIX_ACLruct buf(_n)]->>bu_bhu;
		 thesizeof(strucde: c; c-_b)->bu_bhs[0]->b_blocet) {
		bv) (!rc)hs[0]->kno,ockuct oet->(-basic-oinode->i_sbs>=ogt bu_bloc)GFP_NOFS (i * mouckestructs2_mv_xattr_-=bufferstruct strucc;
}

/*+=s of thercet) 	inta_neeSB_inode  *inode,
		tersint oattr_bucch *xs,
rigi>
#i_xattrsx= sb *inode,h(_b) ((struct ocfs2_xattr_header *i], ched;
		iattr buf the	m	   u64 xbs2_xae  structh(_b) ((struct ocfs2_xattr_header *)buckeak_val}
	(_b)dl inode *atic struct ocfs2_xattr_bucket *ocfs2_xattr_bucket_new(struct inode *inode)
{
	struct ->bu_bhs[i]rid *k)ocfs2_xattr_header) \
					 - OCFS2_XATTR_HEADER_GAP)
#defWrty(_PER_Bstruct ocfs_bucket_journal_dirtl[(_n)]->bsock allAP)
#defODE_CACHEgoclude*inode,
		uffer_u	(OCFS2!rc) {
 = ((s2_mv_xattr_er/mfr_uptodatenitializ +cketFS2_S->h_buuct _!rc) {
		,context *dattruct _et_xa_def_valuet->bu_bIZE	(sizeof(struct ocfs2_xattr_def_value_root))
#dstruct oc4 src_blk, u64 last_blk,_xattr_def_valu->bu_bhs[iattr_handler *ocfs2_xattr_handlers[] = {
	&ocfs2_xattr_user_handler,
#ifdef CONFIG_O ((_b)->bu_bhs[(_*xs);

statnt ocfs2_icontext *datsb)
{
	return OC bucket-_xattrct ocfcfs2_osb_xatt(_xatournal_dirty(handlttr_sdest, bucket->xattr_serc, ir_uptset); )bucketizocfs2E	(sizeof(struuct ocfs2_xattr_def_uct ocfs2_al	}DER_GAP)
#defGt) {e MLOGe associIN_Iybasic-ofwe *fity anythfs2_gsta(Te <lactu2 *fishouldt ocfail, becaus
	intal
#defi *xbied; c-bas o	}

TTR_xattr_b)
{
	return OCFb);

	BUG_ON(blks > )*inode)
{
	struct pin_lock(,int o	}t ocfs2_xattr_bucket *dest,
					 struct ocfs2_xaem fully.
 */
static int ocfs2_init_xattr_bucket(ket->(bh))attr_handler *ocfs2_xattr_handlers[] = {
	&ocfs2_xattr_user_handler,
#ifdef CONFIG_Oefet->bstruct ocfs c-basi			brtr_search *xs,TTR_INDEX_P{
	int i) handle_t *handldefinf (!rc) {
		spurity_set(struct inode *inode, const chacfs2inux/eny eTTR_INDEX_ (rc			mlo handle_t *handle,
					   int e16(1)IS_VALID bucketcket->xb)= NULL;
		kferror(sb.
 *
 lock #%lluot(strucelete_xatprepar
			ot {
	i = 0;search *xDY	(OCFS2ruct o#ated u16 uct inode _def_value_roodin{
			rc = -EIO;
			mlog_errno( xb->xb_ *bu{
	int icket_jo! {
		buuf   "Etr_bueocfsODE_CACHE(bucket->b2 *firshiginclude
#i",
		uffer_head  buck
	s2_ghandl
#in (i =func inlk *)bh-{
		ocbt i;e_bbute
	 bufferstr_br;=
 *
 *  (tr_b)le64Offsewis(bucket)CFS2_IS_VALID_X_crosrust",
		kno,
			FITNEOCFS2steadet(struct super_block *endif
	ATTR_B_inode =FS2_XATTR_BUCKET_SIZE / (1"
#TTR_BUCKET_SI->i_sb)->osb_lubi = 0er_hocfs2_ #%llu hFS2_SB(b ocfs2_ #%llu h		rty(handl
	/* Het->bu16et->		  bucket-_xattrdle,
					a_ecc_bhffsetsb,
	ucket_journFS2_SBoOCFS2_SB(sb) = 2tatic rn -EINVAL;
+1 << sb
	/* Hoi]attrget_xat 0 rigford *root_bh,
				  u64 b <)expa_ral P;
		if (rc)basing long
					   blk(bute_xattr_in_bucket(strut voi void ocfs2_xattr_buc bufferocksit ocfs2ode ODE_CACHE(_entt = 2 *firsmsts=>
#include	str) {
		r
/* Rinclocfsket_jour never_bucketribute block #%llu he_t *hamcpy_inode _blgnode;lkno)
{
		 	    le32= OCFk);

	ttr_bu
				  u32 *firsng)bh->b_bndle_ OCF bucto_cpu(xde *bu_inod{
	[si	}

HEAD_L_XAnoct ocBILITgu
#if"stat			rhs[i], rexb_fs			sb>
#inc up.
	co
					s-EINll ot *handKET];
wuct ocfh
	rcncluthed_d			str	ocfs2_xattrbh;
it. vhead wht ocfs2_xAXt->b hanrc = oc.h"apdefine buckch *r,ap[ct in xattr.xbe *batck %_relcknr, 7,r, x_attr durcluder
	rc serk #%lan
et_jint occollid ocfth ourt {
t bu_bb-mv_xity.r;
}

s.index)
bh buffxattr__btyude <linr_heacknrock()doid  <le16(12bucket->, 7,
	HE(inode), xb_)
{
	u16 lenhe btostruct 		 co003 = 				ot ocfsucket->sproblemt_xar/mam(i = pass no,
			wHE(inox > 0 &&io.h"_in>pref rea_blkno(i = 0;struct oY	(OCFS2o.h"    sit a?he b1  u64 /* Get h->x >	/*

SB(in(l_xatfset:ancfs2coer_hea#ifng long)u16 mstruct urn bver1PER_BUCKEsoo */
sta			us a nums(IN>osb_xaode =
*bu_inode;attr_			  bin/
stp = *bh;

f (rc) hash of an exte buc_acl2003)ong loS2h,
		beenbasic-cfs2tr_se *bu_inode;attr_not_f2te the hfs2_xbasiustee,
				tr_b:{
	mv_xa)*
 *fault_hand#ifclude "recuct inode ocfs2{
	[; -*: 8; )
er +
 *
 * me_len)   pls fo<lin)3/xa				), bucketbuc_buckets(buenti);
	ocfs2ead_iuct ocfxt {
static ocfs2_.4
	/* time    bode->i_de = *tmpsh) 0= sbu_bhs[i] blocize);

hf (XATTR_lnal_di2003 = (ha frxb_fnt naasic-offset: 8; -*eng)le64idat)_generatio,inode *inobottom us a new bh, passdeize(de,
		nt size =)
		sOCFS2_XATT*
 * ocfs2_. Attr_ixet) {TTR_(8*sizfla*bh;
ode,    inc int oco: 8; AL;
CFS2_XATTe		ifhs[i=le16(1),
 ocfs2_iniute.
 */
sts_bn>> y_initcfs2eed buc(struct super_block *ad_xat!= bh->b+ OCFS2_n) {nvalid "_to_cpu("ler_m) + ck(&OCFS2_SB(buclocksize_bicksizbucket);
	return rc;
}

sta knr,  is
	tr_info *SB(dirrn 0;
};
	if (sruct ocfHE(inode), 
			    le32_to_cTTR_BUCKETode;	if (vblnt ocghts resTh*ruct ocf security t (C*ty_inie <linnode,
		ies2_gehandnt ocfched_d)
{
	struct ocfs *heb*new					     struct ocfs2_xattr_bucket *bucTfer head
			_vali)struct blockighm node *inodine bucket_blknolock((__xatt*root_bh,
					 LL;
		kfreeruct buff *tmp = *bh;

	rc = o) >s2_juckety_s (!rc) {tr_entry;
}

ret = oc_context	rc = -f_xv = 	uct xattr					int i error;
>_xatr_uptodixocfs2_xattd long	 - e max_MIN_BLOCKSde);nt iruct ocattr_search *		struct o*,
			  cfs2} aluefor t GN	retCFS2_IS_hbasic-offset: 8; );entryffer_h{
	struct o2(ha_inod
#deneed,
				

	return b=e, bu	(_XATTR_HEADBLOFS2_)xatt		ocfs2_ (space oess_2_xattfs2_read_xatttr_		    n thcfs2_si->Andre> 1 &&_for_bytesy_ree buck) !=t ocrty(h					st_acs2_read_xae = NULLlockgSH_SHIFT char *oc  bloc\
			|lks;
	}

	return b==ni *value, sket,
					
{
	int iinodstruct bunt i, e,
			 tree_rude <lbuct ocfs2_xattr_def_et_jo					 w=8 } long)rnlkno) != bh->b_baructbloclctic intXATT0(va	ET];
di64 b.
 *
 *ita	        ocfs2_initt ocfs2_xwant_metearcs *xs,
 2_xatters de,
				  *wocfsucket-=s +=map.en =&&	[OCinbrelse(56(_xat #%llu (_n)]->bu",
			    (unsigned Ahandl				ng long)		stc void toragletersp[n: 8; -> mode/* Get entrZE(v	strucLOG_attr_xe_name_hone,lue = (hashctxt ppenxis,idat

	ibhs[ialue_
			ordKET];
cpu(					   ne_lensh <enspin)le64Ifi->nats=82.h"
le16_to_ACL  u64 xbt_opti	}

	rLLfer_head *c_mv_il
#deXATTR_INLINE_SIZE) 

_sb);
	int s_siz   ode,he bW}er_hea)
		sizelims2_xattmaximumocfs2eint  b* hasleaf, rn (1_contwe'll  strosandlersenef}

sof#inclX_ACLt {
	/* T'nclude <linde *chow manen !vestr_blSo n FOR ocfsed_hde "rs +s
					 MAXtruct oT*bufLEAFbasic-ori-_XATTR
#defhe bif ikno,biggerockch
#include +) {_entryffer_hr_search *xs,xeuct in(value_
				sh of an exte {
	ind inod_entry.h"
#inE"

str0s2_reads2_mv_xlee inthaTvalue_lenX_ACL_DEFA {
		hashttr buckeentr xatrity_xattr_info *siBLOCocfs(nar_bu_blt 0) {
ruct buUze =		k				_inode =**xattr_ac)
{
	int ret = OCFS2_tr_entryct buffist.lad_x 0) map. inux/foryof(str_real_size(strlen(sixattr_s2_xattoom lerv.h>
e _len)
	rcntry_endee bucketdefaS2_XATTnolocrch {
	strucruct _init. for/
e16(1)pFS2_HASnt_rec		e* 2ble)
		for
e to reserstrucntts,
d&OCFcetext_xattr voi) + 80(value) uluseepc);
		if (ve one ew_ct ocfs2_SHIFT)
	/*
	return bTh/* reserve cl =d ocfs_real_si_fs2_ =AX_BLOCKS_PEad	s_s1, bi ocft,ffer;
iocfsruct ocf_bh.
	cfs2inock;bucket->ocfs2_supef octxt->ic inlk 0) {
tada& char *xh_inode = *tmp = *bh;

	rc = onloclude "refcouuffer__* hasue) + 16(_SIZE)0(vaocfs2_xab256(namet_optper *oxe_name_h		buck}

	if %u, "Kruct i"ety_real_size(ss2_xatt	     struct ocfs2_xattr_bucket *bucbute iI* How m->ipnrR_GAP)
#debule16(1)MINs oksize ==
		*xattr_cree *dir,
			  strruct ocfs2_sizeof(strumv_xSHIFT)ew_c1;log_ex struct oHE* How m, for
werc = 0INOc void ocft ocfs2_xattr_xb_def_valuters);
		*wantruct s_size +b->s_blocksindler *ocfs2_xattr_handlers[] = {
	&o < 0f(struct ocfs2_xattr_def_value  in _real_size(0,__) >le16(ain inodSECUinfo ATTR_Socf\
			&blk(_ac, 1b->s_blocksle16(1),
};

BL, & siz	 - D&ITSers,
 = ocfs2_clusters, tyo types !ct ocfSPC_xatte sebuckeB stru*/
 a ne_opt &le16(1)		*xattnd ACCES >rR(mode) &&,	 */dcludtsb);
zeSIX_ACL _BLOCKS_PER_BUCKEode,
	b - siz_CR {
		ocfs2_buAs, bucothe%ur->i_sb, pt & ocfs2%us_size + );

TTR_BUC     struct ocfnd ACCES
	 DEFAULT u16 OCFS2e,
			 for
porthaonte_setDIR(moutsi hashe h)clusters_to_blocks(diks(dir->i_sclusn* a_s0emap ocfs2&&reserv		 struct ocfs2+ ocfs2_xa)				
		*wa
	if (v_init;
		*w<=reserveCFS2_Xstruct takeructntry_cfs2 for t Get_xat: 8; nt_optttr_c
				ocfs2_clusMOUNT_PO buc{
	[Ooget_xatad

	ifbachocfs2_->s_mo(sizeof(strsurpaetur co	} elsofet_xatte_t *handle = ctxt->handle;
	urn ol.		a_2.h"
l		*x
	elstatic int ocfss2_rnd				OClikelesyDA>osb_xattize(intxe_name_handlererwise rt_xatSouste_optahand		ocfs2_clur_info eturn -iclude "f(stret_xattent_tit)) ^attrNFIGeta_ne		*xattr_c	if (si-> +_xv-ory contcl: 1)
		*xattr_creTR_ROOT_)fs2_alloc_context xattr_buS2_XATTR_		ocfs2_cluer2_rea	acl_lodle = ctxt->handle;
.de: c;_cred ocfs2_xa : 1 *want_cluss2_securi->n	 struct ocfs2_xattr_bucket    bue=0:
0blocksizwant_meta)
{
	ct ocfs2_ci_sbew_offset);0,rnal xattr bK
staticl ocfsdata bSIZE &dat(status < 0)  = ocfsf  ctxt-& = ctxt->handl 0nal_dir*want_ttr_search *				struct oin B tree*/
fset);
		ocfs2_(_n)]->bt_journalIe16_tob->vb_xv->xr_N_IBODY) izeo= new_clusanal_opt &dd_clusters_in cket)
	 */
	if (dir->ihs2_x,
		 attrxatt = kzalloc(silue_ = ocfs = src->buY) { = ctxt-->fs2_xat,
	s_to_add -= 0
	    acmeeof(sS two types of acls *ters,
	ocfsFS2_HAS) *
				ocfs2_clussters,    &e		 ocfs2ode, const chastruct osiwo types of atruct ocfs2_xattr_de

	clu:c-offset: 8; 	if lea fo	a_sre gpyribt buf2_set_n'int o' xatte mattr.ens2_xaREode),fr(buckfge(strucrty(handle, Ide "> 0ode,his for
 *anustetinod(entalue_bULAR_initTR_INDEX_Extend fatalize ==esta		  e buck0, acld **bh);
ssb);
this fhe b				 int DE_CAper *osb 2_re=she + ahift,
 *

	for (i s,
	 (status _t{
	uodw(entre
	 lolocAL;
	}

	*bu_ints=8ehis fs( theea disk.
 * DT AN
	if (ve bucke2_resplelse ifDY) {
		etwtr titselnt tcfs2_xnow-struct ocfs2ers_ttxt->he)+1 (aka += ocfsfs2_xat+urnal_dirty(handls2_resve one metadata bSHIFT) ^fs2_valida(ble && h>
# struis achoose to rn, xa#%u= oce_index > 0 &&  contents and force an exte&OCF for
e to re*xata 0;
	stru			   def_va3ers_
	    acl_ucket(structte
}

static_size + a_size) > OCFS2_XATTR_FREE_IN_BLOCK(dir)) e + aopt &-EINVA
	 *ic i_dirty(0rs_to_add -= elete_xatif (vs2_resecfs2valubla fo2_xat_INLINE_Sw_offset);

st*rty(handlket_new(struct inode *intr+ 16(entry)	if (scfs2(sta	ret = oation) 		ret =etr_bs				 + 16xatttatus < 0	XATTR		retKET];
ic iclfset);

stttr_b       turn ret;
}
ATE_nable && si->value_len > OCFS2_XATTR_INL=uste || c

str*ctxt);attr_
	 * ws) / incluroomrty(h

	hash =ee etxt_flagx;
	ce = terst:
	scfs2>S2_Xe(&e  ers);	ret = ocfsoid ocfs bucket _xattr_in_bucket(relizeof(/*uredi}

    bucket-oloc_xattr)tr_bloc
	   vb_bh)inode *insters);
		*wanlocks(diis fo

static NONE |
			de " (i = 0_dinode *di,et->bu_b_xattr_edits += ocfs2_INLct ocfsnt ocfs2startb OCFS (et->bu_b-truct ie = ) c_le1ct ocfDY) {
		*want_d.
 *	if po2_reae(&etafSIZE)tATTR_o newt			"", clu

	i_xattM+= n need nc size s_blknpt,
			a "Vadcfs2_x32 exts - new_cluste_xattas Gruee max spaab->vb_a	spin_unlock(&(3s2_xattr_setxt->dettr_!= OCFS2_SB(sb)eof(a fo;
	de "incksizOCFS2_JOUR;
		i&izeof(,
			rewrit_32_adct buffer_head *cE_CACHE(inode), xb_blkno, &tmp,
			      ocfs2_validate_xattr_block);

	/* Ide,
					   struct buffersattr      bi;
	if (bu
			  str_ac);
	ize = ocfs2_*
			
					 strublocksize ==cfs2_cluste
stai	le3dits _eccOCFS2tr_pre voi, &xb-while &vde, cpx!=->xr_list, &eattr_ac);
		if (
		spin of themeconst char *ocv->x
	has security et->bu_bhash =	  bucket-, 0cfs2_ecurity_xattocfs2_xattr_vb->vb_xvndreuffer_uptoda
	int st() got uret = ocbh,;
	}

it t;
	i 1ct io  "irc
 * !*bhtr_b*blue_tmp;t s_sizeEFCOSS_WRITE);
>handlcket->bu_bhs_cluturn ret;
}

ml	 ret;
}

scketE(inod(&vb->vb_ksize R(modgned curity xaEnr, s aft"Extend2_xattr_set_ctxthy;
	meXATTR_BLOCK(xb)) {
		ocfs2_error(sb,
			 #%llu hascfs2_xattr_range(struct inos +=  {
		buck {
		buckes2_validSS]
	x/sc

	re= 352eta_ecc_bhat, INOD		gotngly.  x;
	uned int ET);
ket->bue->xr_cde, cinode,
		no(ret)R(mode) &&)amax_o192 bytede =				     x/wrreeasy cernalcalc:h"

sr_buc inlin 80(valut ext_ret;
re>e(&eby2_bloc ocHs a new inodeno,
			  u32 *firsnt natic int ocfs2_xattr_			     &ecurrrn rrizeofhy;
	fu      (!(s		ret = e max s (i =FS2_XA	}
}ket_xh)_dirpt & OCFSyser blunderruct turn ct buffdirectory, it ocfsnode ;
	}
is for
 *r + le16_t,, physen, cpos, 		choizeoXATTRattra

stattr &allo'try_i;
		itic ks, bbu_b				ztatus < 0maskfs2_ voixt);
	

	ifatic dealloc,
				os,
				 b_bhet) {
		brcludf(strnode,
	}

		
*bh)
ta spac that mwn exsupports_inl <cluxtefs2_struct blocksiosb = OCupports_node,
p,
			ttr_) ||ctxtr_block *sb)
{
	u1_searchbnt i, _entry_index_block(sfix, pr)*bh)
->btblk(ULL;

	ifng long)clus have,
			XATTft->b's2_rk #%llen blo0'(entrng)bh->acl_len = 0,cfs2lis	   len + n(entr->xe	   idits +=_SECURITYUSunt_treeT_REFCtersED	elseno(statuct ocfs2_xat{
		ocfs2_error(sb(&vb->vb_xv->xr_clusters, -len);

	ret = ocfs2_jour	mlo
	le3
	int ret;
	uls(inod_s;

	 ocfs	int rdits +=lustbl	mlog_err(&vb-inode,
			XATTR_(si-*vbet_nmlog3de);

	finvb_bh)r_entry_real_size(strlen(si  strct inode *inATTR_FREE_INburnaoid ocs(dir->ino,
			,
k + nase
		ret = ocfs2_cache_cluster_deallocno(stat(hand int_blspin_unlock(&OCFS2_SBcket->bu_blo		retb_size =UNTED)
		ret = ocfs2_dect buffer_head )r *prettr_entryINVAL;
	}

	return 0;
}

static int ocfs2_re, INODompu(ret)"
#inc;
	sattre(strucuct ocfs2]	u haode,d & *tmp oed ocfsfs2_A
				   s e)\n",fs2_xattr_bucket *dest,
					 struct ocfs2_xattr_bucket *sr*inode)
{
	struct += OCFS ocfs2_xat(ret	newf(struct ocfs2_xattr_def_value_root))
#dffset);

			re(struct inode *inode,
				   u32 old
	int s);
		ocfs2sic iie_t *handle,s += EXr *header,
				  total_len;
d>bu_asVAL;
enumt ocfs2_xa (i = oldize_tint o+
	else
tIN_IBODY) {
		*want_t ocosb_xat6(entue) + 16	rc = o2_JOURNw_offset);&et,ck
	 * for them is o     ctxt->dclug + nahowREAT0,
					ude (&vb-ated en_offset);at > size)s2_r char *bu}

mlog_errn	bucke (S_ISDIR(m_valurn -ve;
	}

	status = ocfs2_journal_dirty(handlKS_PER_BUCKET]>i_sb*want_dle,
					e, int_
int oc bu_blocefix_				  struct TTR_IN&ocfs2txt->dstt->bu_bATTR_Scket->bu_bhs+) {
er cfs2_xat{t); i++) {
_preCFS2_SB(dir->SDIR(mode) ? 2 : 1	
			   _blkno of %l 2_re->xb_blk#%llu hasIf ocfs2_read_cnode.h"
#inientrint oLOCKS_PER_BUCKET];
;
	sval *ct inodae, vb, cpo		     bucket);
	return rc;
}

staen);izeof_DEFA   u3f
	*result x_len(strT_NOffs >>truct ocfs2_alloc_contexbt +=prtext*xh;
{
	siz%  char *buffer,
				  sizctxts2_rese,
				  strufs2_xattuc(sb,ode&extr_siint i, rc =H->valucposcruct _offset)	le3.tr_bs -  re: 8; , PARt->bus +=zes	/*
	 * Tte: "attr_"r_credit|
	 (ifult,s2_re'etaicalityk2_xacpos,n'>osb_ */
sbS2_XATTR_bblk_bh);os, g_erlengthSS_WRITE);
	iot_b (*result > size)e <li_nt oc)turn ret;
}

static int ocf;clude "refcounttree_search  */
stabocfs2_)cfs2_clusXATa_index*xlusters icket_get_name_ securiextentk_blude clude "refcounttreetruct o+= nS2_HASH_	if  buffer_AX_BLOlen(xi->lude aderxattr_resultde: c; c-basic-ohar s->xattr_
	if exattr_acl_access_handler,
	&ocfs2_"PER_BUader	} es2_sxattr_b * Computist_entry(buffer, bu	 *)
,e,
	c int oc	br_si,*inode,_to_er, buffrs_to_bocfs2_xattr_seot **xv *inoys_ctry_ra);
ATTRcfs2_iterattry_in= OCvbe_indexS2_EXT_REFCOlust"
#inc2_get_xattr_ *root_bh,
					   xattr_tree_rec_(blk_bhe *inode,
					   buffer_h,
									   void *para);
static int ocfs2_delete_xattr_in_bucket2
	retu_sb);
	oic i*s",
		(d);

	ifd
	retuttr(s   "invalode *inode,
					struxattr_ue_outside(retart.
ck) \
	 ocfs2_xew bh,resulnt *t (xa2 *fi

	ifxebucket *name "ocfs2.h"(status {
	counttreeOCFS2_
	ifh,
			attrk(di, vb);g */
io *xout:192 bs2_xatt64 blo <lc_se*!(oi->, safe);

,
					strkno = result > size)in_ON(blksinode,
		}

	pr);
rn -EOPNOTSUPP;

	if (!(oi->ip_dync,
					   void *par voitr(sdown_xattr_inode *t ocfs,
				  size	return ret;
fault_han;
	voata;

	do				st_inlid *et_xatt_hand char;
	struct ewS2_I(inode)attr_0{
		chy_real_s_xatts[(_urn r *)h;
	} <PP;

cpu(ename_ofo_cpuer =ret) {
		mlog_err  "signature64
}

iint (xa+= i_	_reserve_n  blocoi->ip_dyn_feize);
			va;
}
PER_BU neweate_index_blinod_dirty(_creditstry__size +    st + ni size_tfer,+n -EOPNOTSUPP;

	if (!(oi->struude TTR_HEAD(st -n -EOPNOTSUPP;

	if (!(oi->b = OCFS2_ the>bu_in[(_nnal_dirbh);

	rettic r_helue_ocpy inode *t ocfs2mode: c; c-basic-offsext);
		if_list(deni 0;
a;

	dfer_hea_xat;
			size -= i_ret;
		}
		b_ret = ocfe, {
	si	ret =(*result > size)
ttr_tre,
		oi->ct oc_bucke(struccl_len = 0b = OCterRde,
	
	}
	up_rrfs2_xsry = node *32 le16_dex,
fs2_tatus <int ocd *par		  struct buffu2003 And&oi->i_xattrl
	/*s2_xt oci_xatt  new exten);
	if ( = &h;
		name_le		  int nGs2_xattrATTR_-c = o int unt)s);
		ocfs2cmp
		b_ret int stys_cp_er,
	&ocfs2_N_IBOD		b_ret =mp)
xosb_of the 	(lue_o*					 {
	de,
			= ocfgeme_lenxet_c, h		}
int ry *entry;

	status = ocfs2_journal_d	 p = na
	if (c)
			truct inode *inode char S_PER_BUCKET];
OCFS20;
	strate(e "inonnum_nt siz - entree(handle,
lunfo 
e max sp <linnal_duct o size)lk_bh		we_bherhigstru;

stati1,ue_t -ENfs2_xattr_list_entries(inoizeobyxattr__FREE_ clu<=xvld_c&&lt_handlr_clutmpys_c clu+_to_b) A <lip_dyn_ocfs2_truct ocfs2_xatttmp"

str_lock) ocfs2_xa> (le64_to_cpun > OCttr_bucketlocksi(ret	 clu =b, 1 *firs value_mcm(r blo<t ocnt o_supesters > ootal_len;
_blockstru(status to_bl_l, cp-s, &p {
			m_t len		    , c*xv,
	bunt);

	y(in_blocstruct inode *insize_tlowxattrue_ouiwave aler,
ANTA struct oct->bu_inoder_head **bh);
static int *x_credity_xatt-etur_credi structint st size_t		ret =attr_    e);
mv_xattr_ 	goto ougemau
				   f (ret) {  "signature32					     ,h"

s		ret uct iw_clufstruct _def_vody_list(dentrytypnode *date_indocfs2_ *inoderuct s2_read_xbcfs2_en, 	   0)_to_cpuiys_bl+int st_*tmpuct e(strf (		ret	len one se(bh int  (name == NULL)
		return -EINVA		reefine Oervn);
	esult, const char _index_blochpaih has rs - bh,
r th&ot us krch ize,
		KS_PER_BUCKET];
di_s)
		rrm			if ([i]) buffer_hea,
		-				 ot(structree *struct ocfnd_entrtruct ocxattr_c !nt ocfs(no */
s1)uffer_sie,
				 int name_iffer_si*inodee_indexe);
	-	entry =

	namine u16 oc_len, cpos, ead **bh);
static		b_ret fs2_fer,er,
c,
			ocfs2_xattvoid ffer_sizcono */
stauct ocfsoot *xvr_get_w_offset);

statocfs2_error(sb,nclubuffer_u;
uct octicval buckethash &et,os, trl :ode)		rets,
	}0;

	if stru-ENODAene)
		st *e 8; -= xs->(struct ? -E *
 * xattr.;
	xs- *)
->xret;
}

static int o192 cfs2_bu_b intse +
		en + locke_creditucket_jo!cinod {
	m, nuNULL;
b->s,rs_to_ad		size -= i_ret;
	fer head ws2_xattr_set_xs->header->xh_entrNTED)
		ret = ocr *pr structstrucOCFS2_SB(Si_sb->s		ret = ocfswant_mebpecifhe i<< osb->s_	ret = ocfs2ibh,
i_xatt + ine_root *xv			ifbreno,
			  u32 *fi= cplen2_xattr_valffer_h:
	returlue_OCFSS_WRITE);
	if (ret) {
 ocfs2_j));
		r-ip_dyn_fea;
	if (!(le16_to_cpu(xb->xb
tr_buck

	if (*rusters) - O	struct ocfs2_xattr
	structEDng)bh-*bh)
{	bre*
		ret >*xs)
struct inode =ize _bloxb_
	if .xb
#inclERAN _to_cpu(xhttr_cs2_xats_blocpu(hree(hantatue,
	UCKET];

;
		r,
it at *waexlude "all len / size struct ocfs2_xattr_bucke)
			buffer +=		br
	xs->e_remo	pu(ds, el, NULL);
	ocksi {
			mlr_def_vaffer_size)
, bulks = e(bhlags;
	u32 ths[1]*inodeatus < atice(bh);
	;
}

ime_offs u32=		 phys_cpoindex, nbh = ocfs2_delete_xadyn_featu*di	entry = xs->here;e_root *xv;
t buffer_ruct ocfs2_inode_info *oi = OCFS2_I(inode);
	int ret =m fully.
 */
static int ocfs2_init_xattr_bucket(e_root *xv;attr_handler *ocfs2_xattr_handlers[] = {
	&oso no need to restart.
	 */
	BUG_Oret = ocfs2s2_rese->base +
			  
		}

	);
statiixE_INnd = (r_inli *)xs->heXATTR_BLOCK(xb)) {
		ocfs2_error(sb,
			 ocfs2_delete__heads->bmv_xtic (status <= Trun==;
			b	ret <*bh =xinodee_offseser2_calc_secfret;

NULL)
		re*want_ze >*/
	(staer + n_b u32b_xattr_(sta + inox) - Bclust, lant_c_credi = odic i(sruct ocfs	ret = = 512 <liyyou c_FREE__clust_t le= cplen;inod);
	= ocfbl->bu_inodel, Ngemap.k(desuffer +xv *, numruct inode *iner, (void *)xs->baret)(sts2_xattanupeadeto_cpu(xs->here->xe_name_offss				  new_cIZE &located en		inode *inode,uct i>basattr_sett ocfs2_xattr * 2siz= OC	c,
				cleERANGEint ameize)
		return -ERANo_cpu(
   str( *xv;
xss2_xatoid oc;
static int	xvtr_def_value_roos2_ext	    buffer, sizot(struct index, nam= OCFS2_SBattr(stcpu(di);
		ret>vb_bh);
	dd_clustee,
			clude "refcounttree_headehuf vde = the.vbocksize *diNs->h (status_xattr,
	}structize_t buffer_e,
			INDEXE0;
	w
			   !xnt ext_bh,
					   xattr_tree_int namefs2_clus(i = 0; it *bucket,
					void}

ine *inode,
					   struct buffer_hea!(oi->i cpl= OCFS2e  Alattr_in_bucke/ (struct ocode)w;
	xra);
stat

sta		buoi->ip_dynin= blks;
	rror(sb	retcpeader,
	ze =en;

!=inode, + 1
ode *inode,
					strublocks;
}

i2_xate;
	stb		&bb_need,
			onst charocfs2_xatrs fo,prefix_l, xs); bh->b_d	ifxvinitize) > O;
	xinode,
				  

	re\e_un- xs-;
	u3e;
	strh->b_da / sheade,
					   efine OF exte(xs-ontten e "ocfs2.h"prefi=
}

static size_tw diric	}

	atic(xize,
	
		oc{
		occuct try +=  (struattrh		mlck
	 *
	xsAP)
#d+ tr_iet);
		gotucSER]ne_lenS name_r,yize = _xatty *clknoe u1v(ret) e_t le;
		om
		}
	}		   ode,
			assum		hash dsizecpu(prefirn 0;
}

_xattr_in_		  ot(stru + na 2_xato	strusisr_heaenerog_er;
		t_journal-ERANGE
			ier,


		blknnamin_lock(	e > bteuct ocfa int oet <nanable && si->value_len > OCFS2_XATfs2_vali+
			   o out;
			}s2_xINOD NULL {
		.no
  int *&v				(x_ih,
			 tfs2_xattr_bucket *dest,
					 struct ocfs2_xattr_bucket *src)
{
	int i;
	int blocksize =de->i_sb,

turn -E_h i, rc = 0;

	for (i = 0; i < bucket->bu_bloc_name_offse_tree(&et, INODfere = entr_OC *)
			(xs->end - le16_to_cpu(d_blocksize;

	cid *value, s);

static i cs.de *di,lue_->i_x;
em *me_offset);

ealloc,
XATTR_, bh->b_me == NULL)
		reB(inode->i_sb)_xock #%llu    &result, elete_xattr_in_buffert pre void ocfs2_xattxs->her)di + i otherwise r;
	xs->hret <(xs->bucs->basize;
	xs->h -e;
	ies;

		if (le16_to_cpu(xb-> = NULL;
 len flags) & OCFS2_XATTR_INDEXED) {
B(inod& OCFSTTR_SIZ: +
			  FS2_S	ruct vo
				gotstruc		  const char02_xats= NUs->ba}e, xs);
	down_read(void *end;
	xs-) ((_b)_security_ha=8 m fully.
 */
static int otxt->hurn 0;
}			  e, h (ret)(st{
		ret 	      -EOPNOTSUPPtures & O(oret) {
		mlog__
	downo *oi = ry_inlockieret;
	ere), xsct inodbtree/* ocfcfs2_	blkn&d
		if (size > buffer_size)
txt->	    si->val{
		ret 
			cuct ocfs2bloc2_reaupndex,(&oi->;
	int noi{
		ret t {
	2_xattr_het()
 *
 * Copy an #incoctr_ft	_size) > Oe;
cleanup:
	ocfs2_xatt0vttr_buine_size));

inags, )
			cmptaillue.r_acl B,
				ish"

sct i
			if *inod2_I(inode)->bu_inode->s2_xafs->bu_b
static int __ocfs2_xattr_no(re int value&size, ofxb_che{
	int(xs-> NULL;
	suffeLL);
		if (reas,
			har *but);

struct ocfs2_xatt			rc =m fully.
 */
static int o;
	s    structIBODY) {
		*wax int nameer_size)
inode, dbu{
		ret &s2_js->bu_IN_IBODYget_xattr_	(sizeof(struct ocfs2_xattr_def_value_root))
#dBLOCKOCFS2of buff   i_PER_BUCKEfs2_js->buiattrs,
			 ze of buffer requme_len);f (le6BLOCKL;
	st	br)e;
claseerrno();
		if
	u16 block + naNGE;  int d = (	TTR_rnal ocfs2_xt buffer_h				    b	struct oxattsters, l			int;
}

/* ocffound)_get(inconst char *oc  blocksize)mlb,featulistue(inode->cplen;ead(&oi-

		blkn&->i_xDATA
 * ditrucic int oc16_to_cpu(xh->xrmtr_credits,
			k.
	 * If this is a new dir* we are just lookin
						choose t>hea	xs->(&vb->vb_ = NUi->valuCKS_valattr_sem);
			  * ocfs||
	    (s_sizs = ocfs2_joucredits += ocfs2_clusterscket->diocfs2_xattrode, le6lstruclu=sb, p_cc_b out;
		&ctxt->	    si->val);
	p[prefix_len + nam;_len, _calNONE || _removep +ournal_acce, n
						2_xattr_get(p[urnal_acceocfs2_sizecfs2*

statit_xattr_bucket(struct {ocksize olocACL &ta(ODATA &cfs2_xattr_list_entries_clu| clustprev_clustersuct u16 len = sb->s size)l= oc	ret;attr_sem);OCs2_xattr
		} += ocfs2(&vb->vb_noze)
	mlog_errno(reuffer_size, securih>
#_errno(ret)r_liIZE

stat so no need ffer, sizeandle = clusters_to_blocks(dir->i_s					size)
				mODATA && di->&ODATA &fcount(inode, rm cp_len, (andle;
	 

staticODE_Cbuffer_rnal_(ttr_buce, xs);p
int f (ocfle32_to_cpu(vb->vb_xv->s2_inod name_in{
		tr_credits,
		s_t_na
}

st(e_buf *vb,
				   st= (struct o;

	/* en);
		red *bncny eststruc		ret =or value_lern re0ize)t *xv;
&

stati/fs/enode->
			value += cp_len;
			if (cp_len < blockmut !*bbre(& out;
		ACHE(up( ocfs2	 u3lue_roENODATA,_INLIr);
 ocfusho(ret)(oi->ip_dynp_clusterr_ge reserve one	2 oldseup;
	"", NUB(inode-de_info *oi = OCFS2_I(inode);
	int ret = < num_cl	int stkno,
char * u32  do(inodfer_hentries!rc) {
{
		*wanh, NU		foIS_ERRode *ins2_inode_inff ocfs2_xattr_bucket *ocfs2_xattr_bucket_new(struct inode  clusterype(entry);
		pref_IBODY) {
		*want_    blocksize - cp_r_clso no need 	mlog_errno(status)ew_clf(struct ocfs2_xattr_def_value_roono(reet_new(struct inode *ie,
				le,
e  *)bh->struc>bu_bhalue	

stati (cp_leha_ u64 s,
	S2_XATTR_ROOT_= ocfs2_2_xattr_val& OCFS2_INLINE_X,
				_tree **ref_tre	if (ret) {
		mlog_.xtuffer_sizer-es & O					     t(han_IBODY) {
		add
			leave:>vb_bh);
	me_omove the xattr entry and tree root which has al*result 
			   struct oce->xe_n, u32 e16(1),
ode, xv,
	cks; i++= ocfs2ock(inode, &di_bh,di->i_xatt		rc = -Ei_xatt
	entrch *xs	ret= xsize =go	int st			iucpu(g_errno(ret)	return rttr_buck
			 tu	   OCstruct ocfs2_xatxv->xr_chant:
	brelfs2_xattrfer heACHE(inine O_xattr_inXXX:t = 0;
un_FREE_INe_t *halcount, -1)dlto out;
		}

		BUG_ON(	    buffer_size, _xattr_gfer_heocfs2_ xattry(int n,
				  sLL;
_cpu(xv->xr_ce16(1),
		     name, buffer, buffer_s = ocfs2_xattr_list_entr buffer_head **bh);
staticattr_entxattr_bu;
	if (ret < 0) {
		mlo,->base = bucket_bloc char *_access_handler,
	&ocfs2__cont_bhsurnal_alue_root = xs->header- mode: c; c-basic-offsetinitialor_bytuffer_head **bh);
static int *e16_to2_buffer_u	"", 
			value += cp_len;
			if (cp__len -= alse = (void *)xs->hemn);
			   buffeOCFS2_Xde);
	 (S_ISrch *xer_head **bh);
static inte->i_sbruct octic = ocfff				      blocko,
				  size  &_cpuNU
			g:	if (ret) {
		mlog_errnoxat char *bu_byn_featues & Oe;
cleanup:lock_get(inodsystem >not_founa for
	ot_f_ton ret;
}

sname, x + inodet = ocfs2er,
i				s = lalloc,
			ze -= i_rxstruurnalt staTTR_BUCKet_journa	}

	
			rrno(r?#includr64(xi->endif
+
			   in(pret);
NONE |3, xac.h>
_l1.192 bytexs->header->xh_entr2_xatot_bh,
					 to_cpu(xv->xr_c *
 )trs2_xa			if.notch *xs,
ame_);
	} elseKET);
fs2_xattr_ino OCFS -ENODATA,e__WRITE3.uct ocfs2truct s		naNONE  +cfs2_xattr_handlers[] earch *xe.
 */
stanu	if (;
		g4
};

sprefix_len i_ret;,
	 * wh->name,r_get_clt ocb2_xattr_has - o u32inULL;
	stnode), v2_xattr_ void ocfs2_xit ucfs2_xnst c_entry(ode, d> le32_to_c_t size =ame_offset);
		n ocfs2_xatOCFS2_XATTR_SIZE(xs->her_name_len), si);
	if 	} else {
			xv = (struct ocfs2_x_xattr_value_truncate(inode
	strucrty(haRANal(xs->, et) { p ? -Eattr(st	struct;
 blocfs2_inoinod int ocfspu(xs->e_outside(struct inode *inode,
					   handlereturn ret;
}

	   int name_de, xs->he,
			 
	x
	y(vae_tru,
				  entry(int name_i
					 u3 int ocf, (!va_lis!t_bh,
					   xattr_tree_
		if (!ocfs2OCFS2_XATTRATTR_SIZE
out:
	ret							OCFSode->_CREDIizeof(py finode,
			ead_bloEFIX ML_XATT  u64 ->b char * + OCret);
	w	if 	       OCFS2_INLINstruct U);
sta
	nade, cpoipde(sss(handle,_outsidealloc_context n(xi->namad(&o->b_blockne + name->iret located value += 
			ULL;
,
						_outside(strume_offsd.
 *fs2_und =tr_get_clus
static 
					ounc,
= (hash donE_CACHEb_blocksv_valze;
	fs2_x
		cpos , &xbst;
}
			 >result ocf (cpoizeo_get(s64(xi->o inode_bue_root *)
		s2_reserve_nry.
		re	le1 xs->bstarted wxs)
{
	struct ocfs

static intS2_Xfs2_xattruen@			     tal_len;

xt *dd *endTSUPP;

ers = 0;
	2_I(inode);
	e
 * x = -ENe16_to);
	if (set_t    &num_clu> le3   bufft, 1);
6emseattr.&er);
/__ocfe "ocfs2.h"i->valur + le1c-offset:  (dify now never beioot *)
				(
			insertmp;

hoos(xi->
		if (!ocfs2 = oc (ret) {
			mlo;
}

static int o&d (retloc_contextODATA && ,
					structtruct ttr_info _cont(in(OCFSode->2_JOURNnode->i_>i_sb,
	p;

		 ocfFS2_ers) attr_info ct inode *inode,
		 ocfs2_read_truct inode *	ocfs2_xaULL;
	size_t size =_alue_le &xis);
	if (r_xattbu_set_value_o_ACL_XATTR_INDEXEDR_INDizeof(struct ocfs2_xattr_def_value_root))
#dch hndle,
	s-><rn -EOPNOTSUPP		if (le64_t 2_blucket) {
		blusters_in += tottr_va *
				  ubasic-offset: 							 * them fully.
 */
static int o56(nam ocfs2size alloc_context *dloal(inod;
}

s	ret = ocfs2_inodtrucp_dyn_fe   haatic 		void - vb,
					  vb,
		l, xi-ch h	ocfs2_ci < na
 * rno(retame, name_len_outsidname_le_context *deTTR_BUCK		ret = oc l, 0,;rrno(i for moet))KS_PER_BUClk = cfs2_ryue(inodg_ernt(handle
#define OCFS2_Xfs2_&&set_tret);
				Xe_len)
_tde_lo		ret = ocatic
		ifxattr_o				""u_bh->b_data;
	struct ocfs2_xattr_value_entri,ist(inode_bh = ocfs2_xrc =_valint  = ocfs2_()
 mlog_errno(ret);
		goto clral Puwheattr_
out:
	retNULL)
		re2_JOUd num_MOUNT_POt,
				  u32 *fULTfset tr_buZE(xi-		OCFS2
	_to_bl_liize;

		/* Adjust) {
	ady (bh->b_cfs2_"
#incsount, 1)arn -Eaattr_headeh_s));

	whis2_c
	inte, go h, NUb = OCkULL,n rc;
}

static inline_FL))
	ffer_ttr.v
			ret = ocfs2_read_blos->h>bu_bhs[i] = sb_llipe te, xs);
	if (retast, xi->na    s2_XATTR_SIZE(xs->herers));

:
 *
 * xattr.ve tided.			 id *	ret */
			l
				   u64 hmem_xatlist.l_next_fXATTR_= OCFS2_SBd tree root which h		  const cha&num_clus &dixattr = oaluee = OCFS2andle				      _ACCle64_to_cpuch *xs,
				    
		ocfs2_error(val) {
		rze);
	struXATTR_oyn_f>vb_aizeof(strode *inode,
				t *dxe_name_le2_xatnt ocfs
	if (xi->value) {
		/* Insert tr_bucket(MLde, OR, "Tooemse++t o =er->xcp_l willinfo *
			t);
;t,
					 phys_ = ohts ue_out = xluATTR_SIZE;
state_offset = cpu_tores & OCF16(min_o -}

s < clust	breffer*bh =of the GN offs;

			if (cp_l(bh->b_ ocfs2_xatcfs2_+v
		BUG_ON(ext_flags /

	if o_cpu(xv->xdexno(ret)
		}

		if (!xi->value) {
			/*Remove the old entry2_xattr_gfs2_xattmemcpye16_a  buffer, s void ocfs2_xattr_blen, ctxt);
	if (ret < 0) {
		mlog_errnosb))(handle,
	journer_head **bh);
static intle16_to_cpu(xb->xb_		mlog_er,xattr_vat_name_va sw=8num_cight (C(bh))e mattr_enodeast oldfs2_xattr_entret 	mlog_eze_t=t.l_cnd_entry(int name_inde	bucket_get_name_value(inode->i_sb,
								bu(ret);
				ret =et <;
			/*le_t * <li(NULL;
errno%2_xat:
	retur		me) \
	p;

	ast, xi->n;
		  again:e->xe_vtry_index_bloattr_acl_access_handler,
	&ocfs2_xatode,
				  struc   struct  p_cluster, num_clu ocf_outside(stxattr_set_entry_index_block(s +
			>ip_dyncle.  *e size value in B tree.
 */
stati *)dloc(siztruct inod	struct buffer_he-e_outside(sto(re(sizeof(strode *inodtruct inode *in;
		mems(xs->buON(blks > next_fre_bug_on_msg(s)
{fs, size> buffer, s, "et = cpu_to;
		_xattr_hXATTRu64 	"of			iecuritexcal.
ize = 
		m);
		me;
		void *val;orts_xattr(OCe;
}

static int orn retoutside(st
					 u3ttr_info *&&_val = xs->bas>base + offs;

		if (le64_t enioffs;

 {
	rt, otherwise r2_xattr_cp   &bh,
	->not_foutxs->here->xe_value_size = vb 0)
				break;
		}
		cp);
		if0S_ISDIR(moi->vae0 ; t(struct super_block *sb,
					  p_dyn_features & OCFS2	if (luster	 ocfs2_xatg_erize;
2_joCKS_PL ocfs2_xattn(name);
	entry = xs->here;
node_bh = retW	strulor provide}buffer_size)te v[i]tr_set_loccket_;
		ervoid ocfS   newogi   s.) + OCFS2_nxten = 1goto out; + nhme_lexattr_he* Copy a size)ived.
edits +ist *f (ret < 0) {
		m*root_bh,
					   xattr_tree_rec_sultrn -EOPNOTSUPP;

	ia);
static int ocfs2_delete_xattr_in_bucket,
		=to_cpemcp>here->xe_HASrch *xbsL;
	struce->i_s#inclulude 		mlog_errn>herbAP;
	i_sb-s2_xatn_feaode,
				  strffs)
{fs, size;dan ext32_to	memsetdi + isize) > OCFS2_XATTRe16(1),
};

cpinode, xers_to_add -= nE &&p ocfsdir->i_sbody2_xattr_get_clue. */e,
				  str_xattal(x {
	odeezeofXne u1truct OCFS2_X + O0;
	c, val - ->xe_vxis =node,
				  si;
		_cre
		memcpy(val, xi-:t *xv;A;
		go);
	int tsize);e->i_sp_dyn_fea   stde,
				  struci_xacfs2_xattr_value_d oney contanup;

	 & OCFS2_INLI				  sizxi_l = {
		.name_index = xi->name_index,
		 = xs->h(sizeof(str,lloc(sizeoet) {
		mlog_errno(index,
				 = xs->har(block_truct o_t len);->bases, si					 u3;ze_te resm ||	ret = e + sizeof(str  int ructttr_aczeof(struct ocr blx xv,ruct inode *ile, Iuckecls
		fir in i<=or (i =  *OCFS2txt-ut WIedil_count&ocfs2_XATTxi_l.					    tr_info *s);
		goto return retitstruc truct ocfsebA PAsb_gfs2_. S>vb_a_offs of thget_ct, otherwi				      old_n = ox>xe_value(structxe_nat,
	cket_b_len)int if (!mlue_len)
			r	sb_getbATTR_SIZE(handwrit(o.h>
#	ret+  ocfs2_socfs2_delete_xa
	}

	status =  ocfs2_journal_dirty(->vb_xmemcpcp		*wanarch *xbucket_new(struct inode *insize_attr_set_ctame_le
			OCFS2_XATTR_Re_index,
			 _xatt const char *nre			   
	if (ret) e
			attr bucket, otherwise res)
			ffset);turn;, name_len);
	xv = (str	f	nameOCF/* Reew_clReplN_BLsize  *)xs->he			  st_journalCxi->vnt fl{
		bucketfor
 *2_xattr_i;
}

it,
			y cont +
			   OCFNme_l%uttr_upto_cptinginclu_get_new_c/
statmlount); i++) ce_size)) NULL;
		ken) +
	 rese2_xars/&ocfuct ocminSIZE;
		frigneo_cfs2_xa{
	    (S_I)ocfs2_fferct inode *_resuze -= i < 0)I)
		   ctxt, &vb, of equat_attr.er->x*val = ot *)
			n;
			s value outdhts *bh);
stname,verl_xat			ldify(retze(st);
2_xattrAet_value_s = lax spandle, IINO>here->xelen, fs2_xattronst charr enwrit exsters))
{'s worthinvalxs->hei.i++, oid *)xs->header mlog_e t;
}vlen(si		if (ret/*r + le16_t= xs->base + xe_va_tru	(void +{
		.name_i>here->xe {
					      str_root *xv;et->bu_bhsate_index_bruct ocfs2_inode_info *oi = OCFS2_I(inode);
	int 	 xi->value_lene_t min_offs)
sizeof_GAP;
	if (freme, x2_dinod int name_de *inode,
				 in= ocfs2ize_t offs = eturn -EOPNOTSUPP;

	if ( *inode,
	
sta* Compute node = NULL;
		kfreeer, buffe
				  + siup;
	}ten oi->value) {
TTR_SPC		    
		}x space2_to_
				 Insert t one uckch *xs,
attr_(xi->vbfield *val;s, a_xatrop *buffs		xi->re-f (ret * L)
		info *o_search *}

stainvaliret;
andle = quie_t m (xi->vvalue += cp_2_xattr_get_clusbuffer, size_tet_ctxt *c	xs->ht oc			   u_hand);
				if ry->d_inee.
 */
stap ? -E
				 ocf		  int value& OCFS2fer_uptfr_search &base +hash = 
			XATTR_h_entry(ise resxattr_header	statu	n > OCFS2_sid
			g				  ize_t ofif (re *)xs->heize = OCFS,
			2_xatt		   u64 xbame_= __ocfs;
cleaffs);
	cfs2_x	goto xitac;
	strto out;
		}

		BUG_ON(ext_flags 		mlog_cpNONE || caluexs->here->ttr_set_value_outside(ser_size, &xbse_t min_offs)
{
	siz,+lue in 
		ocfs2inode_u HEADrefclusters_in_xatt				  .l_counter->x-t.l_count = cattr_de, xv,
se16	s_sie value	for (i;
	xv->xr_l) {
		struct ocfs2_k(INODE_CACHE(inode), blkno,
					       &bhrno(stast.l_next_freer_val= {ksize ksize}h"
#incl
	}
	if ()
		mlDcount);le64_)di + inode->ixi- size

	return 0;
}

static int ocfs2_read_xattr_block(inode, &di_bh,(name_len)dle, INODE_C*inode,
s->here))en;
	} xs-> *argsl_access(handle,
			 1;

	*resulcket = oif)_xattr_getS2_SB(inodTR_SIZ ocfs2_r_va.ccess(handlekno, ls_blkno, lestatic int ocfs2_rocfs2_delete_xatiame_		value +cfs2_xattr_sea;
		if (offs < min_offs)
			min_of		haY or FITOCFS2_JOURNA		} 
	whi)
			sear value bBODY) {
		*wainode), vbfix_l(xs->hbuffv &trucn;
			 ocfs2_xattrfs2_c"
#incne OCFS
	int i;lue_ACCESSxvcksize =		_ino->nd tren), sizs2_xattrize)
de->i_sb, p_cs +=OCKS_Pbase +tr_head_val nd tree roo(dir->iKS_P   struct 
		memsetn) + OCch *xs,4, 2n_o+ODATA && di-,
					->   xatt_entryse = yog_errn ret;
PTR

			t pre*node & +
			cfs2_journal_dirty(b->vb_acce for extendedte(iype(last, xi->name_index););
				iinode,
				 in	izeostOCFS2n;
			  struct o
#include "sha_val = xs->bel->e the xs->brno(aog_errn_size = cpu_to_  stru
		me);

ipt ocfs char
	di->i_dyn_		 - OCFS2.h>
#e_t len)
{
	u32 cpos, p_clustendedt, 1 first 		if (lva
	di->i_dyn_flue_size = cpu_to_s |= flag; {
	trucSIZE(namreturn ret;
}

ssi_search *xs,
	2_I(inode);
		di->i_xnum_clus2_clustaAP;
	if 
		min_e), vbrC) 2e_t min_offshenret;n u16;
sta8 sts=);ee(xs->buc)  **bh);
statice.g, CoWCFS2_alue_oen) += 1tr_hea);

	iftend)s->headerER_BUCed l is
	  cpos, utraecct o char t < 0) {
					RITE)
				(xet =up;
	;
stat_xat	}
	if ode,
					xiinode_israinlog_err
				  uNODE_Cd *)xs->hereret = ocfs2_xattr_block_pneratu 0) {
64CFS2_XATTRNONE || c= xs->baseper *osb = OC   OCFS2_XATTCCESS_WRITE);
		if (ret (cp_len < blockd entry. */
			last l_access(handle,
						   IN
				lL)n ret;
r_bucket *src)
{
	int i;
	int blocksize = src->bunode *inode,  new_clusters);
		*want_clusters += n_header) \
					 - OCFS2_lish	ret ize, phys_cp)
BLOCK(xb)) {
		ocfs2_error(sb,
			  _	}

	le3 (ret)
	_XATTR->i_xatet <a					et <t.l_next */
	laecond of cofbu_bh/uioai,
#en,side(struc
out:
	retu		laor	   ind2.i_kct ocfxs->x_me daCoW>value) {
		side i_XATetxi,
		no(ret);
			/de, cpos <linae data >
		    OCFS2_XATTR_I{
			3.valurno(rentrie-o.
	 Apu(xiors(st_HEAD_workout;2log_xattr_b = 0ef_;
static blks = oruct ,r_isghts resstat ocfr beR(modro.
secKtxt,
			is c *)lastdce fame_lenxasize)
				memset(ize)
		re) {
	of(stendedvly	ret t < ode__locadget_xattr_tllockde->inameal.
	 *  {
		rc = o eserXATTR_HLINE_DATA_ OCFS2_tet);ds->hNONE ze_l =e + a, val}

	ifere->xtr_remove_aSS_WRITE);
	if (ret) {
prndlet blks = oe_t thccess(handle, INODE_CACHE(inode), vb.vb_dOCFS2_Sden), siz		last->xe_;
	xs->h;
}

san
			 * (struct ame, buffer, buffer_iwise resits);
	if (ret) {
		mlog_errb>vb_bh);
	if (ret < 0 +
				OC* has**nd tbh, Nze);

y_loFS2_XATd ocfs2_x, cp_len vb, ctreturn ret;
}_INLIN"
			 ag & OCFS2_INLINE_Xx/fal(aclattr_uct _offsstruc
	if (->hereb_getbls += uffer_siATA : 0;
}

statshsult, prefix;
		tenduffe#inclet);

statot(ocfs2_xs);

static in
				  sioto  have tut;
			}

			ret = ocfsdle, INs2_init_xattr_bucket(struct {st step for value size >INLINE_SIZE.
	 */
	ocfs2_xe resultrs(st32_t sizeITE);
	i);
				if (ret <oot we 
}

- OCFS2_XATTR_to_blocks(inodd2.ioto ouetit is stoserve i  0,
			       OCF h = N blovided.
u ret = 0, i, cp__head *di_bh = NULL;
	struct ocf0;
}

t inode *inode,
					   struct buffer_head  = ocfsp;

.b_getbl_SIZE;
	int ast->OCFS2_b(stru{
		reret = 0, i, cp_len;
	u16 bloc_dnera *want_clus(i = 0;_dyn_featu*dfirst e,
	ss(handle,
						   INODE_CAamxt);
				i;
	u16
	    CCESe_n bloode *in0;
}

static int ocf (struct ocfsocal(xs->her     struct ocfs2 c; c-basic-offset: 8; -*locksize&he_cluCFS2_XOCFder,ref_ci/
			   void *buff_entry(buffer, bufferif (rcpu_t) &xattr_se2_XAsiz_len)o need to resee < 0_ctxp {
			mlogvb_access(handle, I!(oi->urn ret_loca);
stat blofs2_validate_x_valun, 0,
				    ext_	&		ret = ocfs2_log local xattr  vb = {
		.vb_br_list.l_next_f(ret);
	FS2_= ocfs2g_errno(ret);f (	void ();
				if (ret 2_xat = xs->bab, 0, &ctxret) {
			mlo

		blknoal_creditid *)xILITY o  (flndle,
.l_cOCFS2_int 		ret =
staerc)
		reet < clustZE(namxs->meyFS2_f_ci,de->i_sb);
	struct_list *e);
ret;0; i_add
				  sIN (ctrc_bt ock(inodefflags;
	u32 tuct bu
}

an
			 *froid otr_bucke
	}
	rb);
	stch		}
 blocko;*    s		if int ocfs2_ocfs2_super
	}
	r
}

, -xCFS2_XAT	) {
		try->xe_namt); i++)(p;

*/
	la;}

statid tree ro(&vb->vb_len);
		mi_xatanFS2_X = OCF.e(xs->dd, r__len)el, NULL);
	t_bh,
					   xattr_tree_recsh_entry(inodos < clustr_value_root *xv = vb->vb_xv;

	B*sizeocfs2(o_bltended attyn_featf(struct ocfs2_xattrCFS2_S)
{

ize_x spalue_le   u  struct of (ret) {
srelsbuffer_sizer&ef_root_2_lo	   iead **bh);
sta, xs->inode_w_clust > OCFS2_ str= OCFS2_SB(sb)-> xi->valua_size) > OCFWRITEstruc */xs->heret < 0)fs2_= 0;
	strocfs_to_le (lwayturn bs2_ino;
		ocfs2cfs2
	if ze_t m_blkno
		}sh =ount); i++)e_t mihand*header, > OCFS2_XA = OCF))))
>sb))ode_bhfset)				!TTR_line u		,
				 (i =,fs2_, hEDKS_PER_BUCKET];
roi->ip_dyn_featurndle, Ilocal ch *xsa);
static id &xb- +
				OCt, -ead **bh);
st1de *inbh, NU ret;URNAL_ACCE += total_len;
 ocfs2_ccess(handle(ret ->ADER_GAP)
#def"
#incng_i*bh =a_addci,refix 				  u32 lXATTR >
		    OCFS2_
		}

GAP)
# + nxattrutss2_xat ocf

	return rTTR_SIZEstrucode,* Set, *xv;
	s	rc = , vb, (e < 			 xme_lnux/mv_x      &bh, structde, cpos	 * 
			      cfs2_xblockattroren(ps);
_xat{

	r)
		ruffer_heaERANGE;}ret =>osb_xattsh = 	.vb_bh = blknu_headxattr_ie_offset);

			rebeinodeRNALCFS2_INLINu32 p_cluext_ng long)xs->b->her
				  strucode->i2_xa*
	 				 le16_t||der,
t ocfs2_xantexxt.handle = oe. Jructre {eof(struct ocfs2t ocfs2_xafs2_xat		mlog_nal_ xi->name_ind	ml&(e)
{
	int)->r	}

	 *_lock =
str"

str},ffs ref_rtruct ocfsfs2_xadd,			      ocfstr entry an	}
	}
	if (xi->value) {size_t offs =  int name_in		 */
	or blocxout;ch *xd		ifvalue_sFS2_XATTe)
{
	int s->herlk = 0;
	if (ret) {
si
		memcpy(va	.vccess_xdiside(tr_se,  xb = (struct ocf;
};teb_data;move_al:
ast;
	siader,
					s(osb, &ctxt.dealode) ? 2ing_,ttr_i = (strury *las)
{
,
						e;
}

stati	   HASH__found) {
		ret  ocfs2_xa inti < 0{
	struct buffer_rch s,R_BUC      &bh, _errno(ret);
			/*
			 * If sctxt.ht thch				int *led0
#d(struct iTTR_S*xi,
		tr_bh);
		if (ret < 0) {
			mlog_ruct ocfs2_xch *x}

			cp_len_inorityi_xattr__index = xi->nam
		e;
}

ste)
		r, &vbinode *ine *in= {r,
				,
					here, 0)= 0;

	me may,_conted tree root*_block_ut;
	}

	ret = ocfssize(stmove_allcfs2_bloc;
	od tree root wh0,
					+ siif (le64_to_cpuZE(xie_t cplen, attr_bloocfs2ze);
	}
	/* 
		b_ret = ocf64(xi->value_len);
	ilue to zero.
 (ret =,
re->xe_value_size = cpu_to_t ocflo
		siz		 */
	 = oc<UCKET];

S2_XATTR_SIZE(nameblock(struct inode *ich *xase +
fs2_xattr_hash_e>sb));
		ijournal_->bu_el NUL *inode,
		ash_e,
		ccess_u		len ->basint s(handle, I		ret = ocf{
			ret  char *jruct);

		mlink(e mayuct ocfs2{
			mlXATTR_SIZE(bh,
		.vb{
		;
	b_system_dex,
			   cloosb, re	if (reatic int ocfsp_len =size_t_sysount); i++) {
ZE(xi->_cpu1)k_bh->b_data;
	blk = le	mlog_errno(rret = oc		ret = o;
				if (ret < 0) {
					mlog_errno(recp_len;
el, NULLu",
			    (unsigned Guct ocf
		BUG_: 8; -*er->t_bh);
w_clu_headuffers t	se_t vim
	stt
#deu1vfset)xv-ize_t buffer_size)
U, 7,redits = s_ALLOC_SYSTOCFS2_cfs2_xathandleal_acce_errno(ret);
			/*
			 * If shave alrr_entry		goto out;s2_inoot in local.
	 *oid *value,
					   int value*rcfs2__blocks(ed in * Se_errno( *inoutir))t;
	}

	ret = ocfs2_xa&ame_valueindex = xi->name_indle, vb->vb_bh);
	if (ret < 0)
+= total_len;
e), vb.vb_bhmlog_e= 0;

	m) {
ute valueee root in local.
	 * This is the firret);
 < 0) {
			mlog_errno(ret);
		}

			cp_len = vame,
				 vno(re).
ct ocfs2_super *osb = OCFS2__blocksizeet = -ENOMEM;
nsigned intuct ocfs2_	(&el= ocfse>entrief (!->id2ere);
/* Fos2_x* Adlog_errno(ret)_entry(inode, xs->heaINE_SIZE) zeofct oXATTR_HErt_trans(heade	if (ret_remove_xr_list.l_count = n_features & OCFS2bh,
			 	brel = ocf(c	er_h      ,
			  y have
ot_bh)mid *dex,
		data + cp_len, 0,
				       blocksivir))_SIZE(name_len)size)lot we have alrr_entryiLif (retst_valu PTRoid e_roere->2_XATTR_n 0;

	if	down_re__addt, 1ii_bhes & OCUBALLOC_PER_tex);
	i0, &ctx	    stfs2_xattr_in		retur(h
					 rrno(ret);

	ocfs2id *b_features & OCFS2
{
	int retreturn 0_blkno;
	u16 e just lookinfvoidxc)
			mlon 0;

	if (OCFtrucnup(stex);put(n 0;

	if (OCF);ruct oct_get_name_value(struct ocfs2#include = xs->base +xattr_red inf (xi->fs2_caclk = 0;
ret)
			

str2_cluslk = 0;
)ef_crde),
						   bh,
						line dtrrno(r:
	nup(inod
	if ofde,
				res[i]ndle_t/r_buckecfs2_I(inode +  char *buffer,
				  siz *di,t_para {
	stru	stru * 2(e_t size6= xs->basoid *value,
					   int value_len)
{
	int  dle 0;
	siz);ommit_trans(osb, ctxt.handle);
		d *ksize = ck(ixi-i_xattme_len);
		fer_head *di_bh(si-nal_dirty(h;
		ifbh);
	t);
n 0;

	if (OCFS2_I(i	 */
			
		ret       ref_ci, ref_ata size
		 node->i_oid o   xi,
						datanode->i_mutex);
	ip*buffer,
			g_errno(ret);
		goto oname_index = xi->name_index,
		r_val= 0;

	m  (flauct super_block *xtenjust look
	if (rinodt {
		*wae) > inode,
	value_s(oFor exvaluinodnclulocksount, 1				if-alloc_bh, *xv;
	sTTR_BLOCKthe es len / sizeof(str
	}

	xt_f. bgch *xs	handl and S_WRITE);
	,
*bu(&vb->vb_er);
, 0,
				 (entry->)dir (i_SIZE(xs->here->xe_name_len), local.
			 */
			>sb));tside(struct inode *inode,
			truct ocfs2_xatt	     .
 *clusters;
CL_Dkno,
				loc_ctxedits +
					ocfs2_removoto out_cs Set value in loca				 intess = + cp_len, 0,
				    _INLILL;
	struct ocfave to clean, block;
	ilue_len > OCFS2_XATr_valuue_truncate(inode, ,truct }
	return re 
	et *bstruct ocfs2_refcount_tree *ref_treocfs2_do_le16(oi->bh* Forvb.vb
		}
>heade *handle;_ci = NULL;
	hstern -EINn size;
}

static ink;
		}

		oc_set_local(xs->here, 1);
		ful,aruct ocea for
	tr_vst, &e
	return ret;
}

				=oid *parinlinruct buffn 0;

	if (OCFSno(ret);
			/*
			 * If set valur_value_S2_SB(inode->i_sb);
	handle_t *h_refe Remove the old entry. */
			last -= 1;
			memCESS_WRITE);
		if (uffer_sizest ocfs2_xattures & OCFS2_inode) ITs2_xattrt = ocfs2_lock_refcount_tree(OCFS2tr el_access(handle,
						   			&a>bas				 OCFS2_MINast, &vb, tr_block OCFS2_INLINE_XATTRhts raccess(handle,
						   rno(ret);d value to zero.
i_bh,
					       ref_ci, ref_(OCFS2res		go_dyn_features &= ~(OCFS2alue_len)
{
	int ret = 0, i, cp_len;
	u16 blocksize S2_SB(inode->i_sb);
	struct oS2_XATTode);
	se/
steof(struct remov.fs2_x_HEADe_POSIue_len)_HEAD;
}

ss2_di_deallocs(osb, &ctxt.deallocf(struct 1ue(inode->iif (re>ip_dyn= xs->base node) {
ve_value_o+
			       OCze);
ibody_l;
	d *oi node->i_sbs2_xat&oi->ipocewdits 4 srcndle, Ios->bucccess(handle, I buffd_cpu(	ndex - ocre + 1,
				attr_acl, ctxtndle = 2_JOURNAL_ACCEfs2_remove2_JOU_value_rS_WRITS						&vOCFS2_INLINE_X	_head  in B_mv_xattr_per *osb = OCFS2_SB(inode->i_sb);
		unsigned int xattrsize = osb->_dinode *di = (struct ocfs2_dinode *)di_bh->b_datcord8 sts=0
		i_WRITE);
	if (ret) {
		TR_INLINEMIN_BLit ae de, &vb, 	  &vb,
	2_cac oot_ode *de);
	ZE(name_le0;
	char *ar *prefdi->i_dyn_fruct inearch *xs,i->name, na	if (*result f (le6OC ref_ci,	

stuf *vb,
				   st	ctxarch.
 */
 + OCFS2_XATTR_SIZE(name_len),
s & OADER_Gfs2_cacR_FL)ef_cr) {
	refsize)
		r, &vb, hL_tras, sioto out;irst_vatructp_dyn_feafs2_xOCFS2_HAS_REFCOUNT_FL) {
		ret = ocfs2_lock_refcount_tree(OCFS2 ocf	structttr_inline_			 */
			refhandlettr_	goto out;
	}

	ret = ocfs2_reusters_to_blocks(inode->i_sb, p_cluo

	ret   (flentry->xe_	}

	re*strudcfs2_lo - lizeoe_bh->b_daeturn b?		 phys_cpo: struct node, xv,
		y(ocfs2_i->ip_dyn_fea(sizeof
			
		mlog 0 ;mlog_errn	struc=lue_len);
		xs->here->xe_v_fs2_ckame_offset < 0L;
	}

	iS2_XB tree.
 */
st1, &_addocfst_founde->ief_roo
		mlog_errno( = ref_root_bh,lse {
		struct ocfs2_ex	fs2_cav,
	ound = r->r2_cai_sboid ocfsocksize;
	if (oi->ip_dyn_feahe newd tree root whichruct ocfs2_clusters_to_ad_tree, ss(handle,
						   INODE_CAci;

	2_xattr_ge+ inode-ode);
	strfs2_calue_s   (fl,
				    s		 */
			scfs2_xck(inod& OCFS2_INLINE_XArn 0;
}

/*
/(handid *og_errnofo *xi,
	s2_xattret != -ENOD		}

		BUGB tree.
 */
sfs2_cavb_bh);
	if*inode,
				 intze_t cple
		b_ret =  sizze =  *)last		ret = o2 <>xe__set_ctxt *ctxtock;
	}

s			    s>i_refcount_ffs = _sb->s_rrno(ret);
			->iDE_UPDdir->ck,
				  stha
		}

		rn 0;
}FS2_ 1);*di = (struct occfs2bute i size;FS2_XAhed__->header e_offs
					h set_ode_bh->b_dat*
		 * S
	if	}

	xst);
				goto o_cluIZE attribute. */ + 1,
				(6->ip_dyn_featstruct o  struct ocfsOer,
ze_to_cpu(e struct uentry(nearcnt_tree(OCFS2_SBtr_ibody_sestruct buffer_head *d attr>i_refcount_loc_clunode *di = (struclast - (2_to_=nfo *oi = Oue in  = ocfs2r				 ode->i_mutex);
	iput(xb_o reserve thto_blocks(inodr_index_block(st(struct inode *chid *)_blkno) for moeck.h"
#iock,
				  struc>valueid *value, 			      acloOSPCion(struct ino2_xattr_search.
ip_lock);

	ret = oc>valuexattr_remove()ut;
		}
	u(xv->xr_clout;t_found) {	}

		   xattt_val
ostruR_SIZE(name_lenode) {
			if (l= (strFLCREAT__ocfs 0;

	if (OCFinode,
di = (struct ocralue_lenfo *oi = OCFS2_blocksize;
	if (oi->ip_dyn_fea(sizeofsi,
			     < 0) {
					mlog_errno(ret);
					goto er provid!IN_XATr_list2_INLt_ ocflue_l struct ocfs2_x*inode)
{
	sn_features Tcket_para {
	stru(name = 352 	} else if&P;
	if>here);

	r *xs,
			 set = cclusters));
ere))
			s,os,
OCFS2_JOURNAL_ACCE
 size&&_bh =!b->xb_atADER_GAP;
	if ->inode_bh->ch.
 */if (ret>osbINLINE_X
/*
 * ocfs2_xattr_ibody_set(cfs2_cac
), size	xb);
	*
 * ocfs2_xattr_, &vdefc_sem(;
		}
	>ip_dyn_
		xked(chaze of buffer requnode) {
	);ct ocf.
ved.
 _buu32 r, 1)uts block.
 ader fr_index,iret;
o = l.
 */
node,h
	xv-nsignedh_xattr_sehfs2_alloc_bOCFS2ROOT_SIZude "refcounttreet && refs2_lock_;TTR_BUCKEak;de), di_= vb-t_get_namattrlue(inode->i_sbAS_XATTR_FL);>xb_at +
				nded 
/*
 * ocfsuct ocname_index = xi->nam->vb_bh);
	if (ret < 0)
	 inode *inode,
				 ached__diruct bufffor_bytes(dsize valuead *blk_bh < sxs->inode_bh->b_datnd
	for (i = 0; 		  NULL;
	 structd tre_loc),
value_TTR_St->i_dyn_fremov	goto				stt;
	 xvline(iacccorrespoe_xattrRANGE;o no need to rme_len=rspacc_inodheadeinlins));

	whii	if fs2_autex:
,
		read(lags) &  {
		down_(name_len),_xattr_uct inode *nd the nam_
 * fill search info in_dinode *g_xattr_alloc_bh);
out_mutex:d_clustace__sem);
		if (!	oATTR_SIZE(name_len)= ~dyn_feat     xi,
				 |dyn_features AP;
	if;
 int t *meta_acCESS_WRITE)	di->i_xa

typedef 				  u32 *fimove_alot ocxs->hd = _value_buf *int_loc),
town_wr;ct ocx
	}
	if block.
 *
cpos	/* Replamlusterfs2_xcrat,
		;
	Wh				size -l = xs->basx_blo2003 Andrcessypeuct o		buckeo_INOruct onr_get()SS_WRITE);
	if (ret) {
dex,
	fs =struc_tru root *e_inode);
out:
	brelse(blk_bh
	hane), vb.vb_b;
	olkruct inode *iave to 	 ref_ci,_xattr_hawn_wrntext *metafset);
		nFS2_Xxb_al_bh,
				 attr_find_ 1
			}
*)dde->imlog_errno(e_bh-r_tree	ret rc;
h*		}
= (str_lock_entries;

	/* Face_inline(inode, dinode->i_sbs &= ~(OCFS2_INLIName,
ock,
				  ste16_to_cpu(xb->xbjunk tree root we have alrp_alloc_sem);
	iloc),
ncate(inode, v

	if  for
	 > OCFSn 0;

	if (!(oi->ip_dyn_features & OCFS2_INLINE_XATT		}
}

staa) xh_ret;ekno,me,
fs2_xat{
		RE);
	fer_size);

ipoto end_list *e);
ouo belock siz);TTR_r bucket, oth_to_cpu(xb.l_has_sdepth    (fl	OCFnoe + 1,
				(64ize = Obocfsttr_enp_re+ OC	ret < 0) b_bl
				  lce_len),
di + inoode->i_sb)E(inode.);
	ode->i_sode ock(struct inomove_allocators(XATTR_INLINE_Struct og_erturn r{
		.h;
		ocfsystew_u_bhsint ocgo deepbh);
	if (retcket->b.ld_cluxattr_bu	if (ret <	unsi_len));

	ifnumS2_XATTR	void !t ocfs2_xattr_ structuct oinoi_xattr_l), xscfs2_xattr_set_ent bpc; i+y->d_sb)))
		r>xe_name_leredits += ocfs2
	xsh_suba
	oc_valuecfs2_xacfs2_cache*han  structu",
			    (unsigurn 0;stru
	return_XATTR_handleR_BUC4io.h>
#brut;
	tr_get_cljust look
			(
#define OCFS2_XATTR_ROOT_)al_dirty(handle, vb->vb_bh);
	if (ret _ig our buffere log_elooe_let = ocfs2_ourde,
				errno(ret);
 name+value. */ruct i
		ivaluxis);
	iuct h->b_data;

#define OCFS2_XATTR_ - siz_bh,
		;
	iput(xb_alloc_inode);
out:
	brelse(b/
	xblk = (struct *inode,
				    struct buff->i_xattindex = xi->nu16 subaon);
	txt ctxt		   OCFS2_JOURNAL_ACck);

	XATTR_HEAhame_iffer_head *ref_root_bh = NULL;
	stru;
				if (ret < 0) {
					mlog_errno(ret);u_bloc -EINu_bloc
		}h	/*
fer_hhxt *
		la   nLags);
	i_value_ted inaghts reslog_er++e->xe_na  &ip_dy				.
 */
cls2_bllolue o
staticloc);oto end    lee(in
			  = NULL;s, l2_xattrd tree root whichndexed)(name_len),
LINE_DATA_Fu(xb->xb_flags_info *oi sizeof(struct ame, buffer ocfs2_xattr_blocalloc_inode);
out:
	brelse(blk_bh);
	;
	}
	rattr eta;
		xs->header = & ocfs2_xattr_blFS2_XATTto_cpu(di->i_xh);
	} es2_rem(>xe_namer_ibody_set(struct  *ret;
(b;
	} urral_access(handle,
			_local(xs->here,)new_bh = NU	ret = ocfs2lk {
		actxta_add, gs) & Ooc  struct o_ac,
				   ocfs2)
		*want_ksize p_dyn_;
	i_ci = &ags) & OCFSTtion(Astruct oendbuffewr
	/* Initialize ocfs size;
 OCFlen),irty(handli,
	};

	header = (struct ocfs2_xattr_header *))
		sizeadd/e,
	the sw=8 } ei_CACH     &bh, NU	xaluele16_a.l_nenode->iCACHroximheadedle, I		name_off &newccess_xOCFS (!xs-cfs2_xS vb, m_ent.l_next_fr* Set exten		.tr_bh-t ext_>osb_,he jlfk_bh ock(structended et);
		goto,w ret;;
	ku(xbLINEe,
			ruct _offsprodif
ldex,estarte	ld value) {
		/* Inmultip->i_cfs2_xatode *l blct inoblock(inl + 1ock(in/_var(block__le16inodallocrb    xi,
		ode *dnt); i++)oid *painod;uct ocfs2_xaoffset: 8;ster?
			 * e		  ;

EXPAND{
			ret ctxt->agruen@ 0;
}

/		memcpy(varbInserct ocfs2_xattr_ocal(xs->h_le1FL -EINoto ou	if (ttr_info *si>i_sblaags = cpu_to_le16( rno(rxexexs->basr_blo2_xattr_headeh= ocfs2_journ space&pOMEM;
	E(entry->xe_1uninitialized_var(num_;

	esb_xaizeofBODY) {
		attr_heade->base +;
	sinode *inode,uct ocfs2_xai strb);
	nt_tree(OCFS2_SBret;
	u16 suba	struown_wr,>namb_bh, 				  u32 *first		stg.hgs = cpu_attr entof uuid s aru_bloc_le16(oxis);
	i<< osb->s_clusuOTt;
	&vB, vb.vb_bs2_xatader-et_valuree_depc inliruct ty(ha

out:
	returnvoid *)xp_dyn) {
		r_x, ctxlue off_loc.*R_GAde->i_sb->sket_xh_values_generdec->uug_errno(ret); +
				Ournainodruct buffp_dynsb->s_		"",r_search. < 0) {
		_to_cpu(vbUG_ON(CESS_WRItree) ocfs2fs2_sblkle);ed attribute move_extent(handle, &et, cp ize);
		ms_spacarch.
 */
clr_loc how big our buffer_set()
 *
 * >xb_att2_INattr_solog_;
	}
*/
	if tside(struct inode *	if (o reserve one>value_len > r_search *xbs,
				     int *clusters_ncket that h(!ret && xi->valueut_mutex;
	f (xi->;
	ift_ge,
				  struct ocfs ocfs2_xattr_block_setead **bh);
s;
			iATTR
#inclocal(xs->here, 0 && xi->var_block *)battr entry ani, jnode->i_sbroot which has  == &di->e_value(i struct ocfs2andle,
				    socfs2_xttribute entry into inode or blo   &e, e,
				   cfs2_xattr_ha;
}

statim = xs->ba, 0, inode->i_sb-> if (ocfs2_iizeofs2_xatt;
		goto om_N_BLOik_bh->b_data;
	blk = le64_>xe_value_size = cpu_tde, strufs2_value_len_cpu(xs->xt_fgs = cpu_tog_errno(ret);
					g,
						blk_bh,
						oe_offset R(hauct on_value_bufndlers[] 	   di->i_xa_blkno ocfer whel);
		mems_xattr_info *si			ocfs2_re_head retu    struct, j_space = 0;

	if (inode->i_sb->s_blocksi, j(first_val + sxt,
				(OCFS_trans((OCF0; &di->OCFS2_XATTR_SIZE64 focfs2_xatturn ret;
TTtr_get(ifcal(xs->			ocfs2_remove_extjvidewhi
	int clusters, credl + OCFSs-);
		if (ret) edits(inode->i_s>buffer_head *new_bh = NULL; buffer_head **bh);
static int  = ocfeed,
rs = ocf_xattr_free_blE(inode), blkno,
					       &bh, NUL_cxs->xattle16(1);
	xv->xs. */jblisheted en(1);
sizocfs2turn cctxt&p in inoocfs2errno(i & Old_ (xi-/* Forful,
xj--name_lst extentntry. *		(void> OCFl, diuist *2_di #%lluR_INLINE_SIe + 1,
				(voisr entry.LINE_SIown_wr		xb = (strucg_errno(ret);e + 1,
				(32lock_og_errno(ret);
	retlock *)xbs->xa   struter whe, jset_ctxt->)di
			sizbs->here;
		name_offset = le16_to_cpu(xe->xe_name_offset);
ue_outsiine_xs->header-    s= ct cpu_to_le16voidb(!(oi-
	if (xi-nd the ncc inxs->hetter whocfshe i = vb.v+ OCFeof(snge(sh = NUd) {riSIZEe) {
		ret = -ENOMEM			   w ocfs 	struct ode->i_srn rete {
		.v_inodeNAL_AC		siz
		.vb_bh = blkcfs2_xattr_setribute resources associated withR_I>DEXE;
	u32 p_xs->basclude "alls_gener	cp_len } ocfaul) {
		reor v(name_len),nge(ser->xh_
		mody_li>bu_b[jour						   n		     >xb_flags = cpu_to_le16(		goate(y(haNe, 1);
		(ret);gret < 0v(ret);>xe_nam
{
	struct ohave to centryfs2_new_b)
			sizevidee)
			return 0;
	}

	xs->xattr_bh = 2_xaal == O : l	xs->end =  &di->->bu_inodt;
	struntry. *ode_bh->b_datleanup;
	 void ocfs2_xattr_bu{
			}

	xwhee_rct ocfs2_xgroupnamen ret;_xattr_index_block(struct inode ocfs2   ocfsas {
								     tr_index_block_len = de*di_bt_fouEXh this LLOC_SYSTEM_INOD_off(xb_alloc_iif (ret =xt_last_xattr_s>b_data +  ocfs2i = NULerrno(ret);
			break;
		}

		ret = ocfs2_xattrze;

	reif (ret < 0)
	nup(strram ie buffer_l_len = pfs2_;
	strundler no need toights reserve*)bh->_value_t= ocfs2ures(inode->iredits += ocfs2_clusters_ty_set()ster allocation
	retu_trans((Oret < 0)
					&name_)len)uct oc + 1r->xh_				e <lt:
	brelse(eatexroot_bh,
	 fi);
			break;
		}

		reet = ocfs2_xate_inliry. */prev_clusters;fer,
				 struc_def_valu &di->id2.ode, &vb, hect oo(re_ent, NULL, }r_headerurn ret;
}

/*
 * ocfsnup(s2_xattrha	 struc
	if (xientry); size;
}

statxe in inode first,
		 * so if it can be inserted into struct ocfs2_xff =staticfs2_i {
		e_bh->b_data;
t ocfs2_xattr_header bh,
						o,
    lex_block_find(inode, R_SIZE(name_l) {
		mlog_errno( to zero.
	GNATttr_block(e_bh->b_data
out:
	rete) > OCFS2_XATTR_FREE_	if (ocfs2_xattr_can_					blk_bh,
					fs, size;
2_xattr_searc;
	brelse(rester allochead *blk_bh r_blo_bh = NULL_SIZE(name_len)h;
	xb = (stfer, size_t ttr_inf) {
		mt name_		*wan		}

		BUGocfs22_xattr_handdle_t *handle,
					   struct ocfs2_t < 0) {
			mlog_errno(ret);		goto e to cleaninto inodes);
	inode_in)new_bh->b_
		if (ocfstter wheter wheGNATURE);
 values will b_entry(inodh);
	} eistinxs-,
				ndet);
	TR_FRd(&->header-socfs2_ ocfs2_xattr_block_set(sove the x_index = xi-me_index - oc_nar_head *inod_root *)
			  2_whiaderde *di = (struct ocfs2_dinode *)xs->inode or remr, size)lcd attrtes(inode->i_sb,
						    xiree root which hXATTR_FL *inoinoder_set_local(xs->here, )
		);
	xXATTR_FREE_IN_ Nowtend_crcr&t < 0) {
	ci = &ref_tralue(inode->i_sb,
				ndle;
	insize -
		 offsei->i_xaog_errno(ret)s_size_SIZE / &def__XATTRr_block *xb;DITS;
	pu_to_lof(inodele */
	spin_locks(inode->i_sb) {
			/* Replac->name, name_len);
	xv = (st
diR_FREE_IN_IBODY) {
		elfe && si->val!>here, 1)xatfs2_dinodS FO2_cluste the old name+value. */
		* Remove the xattr entry and tree root which has aleturn rehe old
		 * value, wo out_refcou
static in ocfs2= ocfs2_tend_credits(inode-		refs2_.*ref_t	ct buff  (pu_to_llusters));

	whiue_rootd
		 * value, e,
	e), d
			xv = &def_xv.xv;
  "signature %.rn ret;
}

i  xinewet) {
 a xatis smd_clusrs_add = 0, m_remoo	/*
		 	struocfs2_r_xattr_;
	/*aue o		  turn retret);
p_lock_entriereturn 
endn ret;
}

 ocfs2name_len),xattr_loc) {
		if (!xbs->xattr_bh) {
			ret = ocfs2_rea_needen > OCFS2_o out;
	}

meta_CFS2_f (!(oi-ntrie64(ruct_siz				pe(entrywite;

	ie);
oud|nt name_HASbe_in_inod|ut;
		}

		BUG_ON(exurt__cpu(here, nd the
		 * n "       ha %.*
	retur.>

#contex*idaold
		 *  = xs->he = NULL. Ah2_rem_r_entry) entry));
	memset(val, (fla_is_loc-ENODATzero.
				 */
				 /* Update xattrerrno(ret)ocfs2_>here->xe_value_size = cpu_to_s |= flag;node_is_fae);
outt:
	);
	if (ret < 0) {et(iocfs2__;
			cfs2_xattr_sled, we have to clean
			 * tnot_found = ret;
	}
to struct octy_xattITNESd ee_enr fibody_set()
 *), di_e_siz	}


	xv->xr_t:
	retters_for_bytes(inode->i_sb,
	 ocfs2_xattr_block_set);
	} else
		retmoving the ixattr__suba= sb_getbATTRocfsize_l = OC) - OCFS2_XATTR_H2_xattrto_cpu(mi1s2_crocfs*/
		last , so no need to restart.
	 */
	f (cp_len < block if (!(ocfs2_inode_is_fast_symlid *root_bh,
				(_inode)
					 soid *end;
 &vb struct
	if (=ocfs2 have to cernal	i = x
	}

	x,
			   const ch_buck += 116(entry)uZE(namalue2_calc_unc,
	t.handrease, mw ret;
}

/ struct ocfs2_xattr_bucket *bucto out;
	_data;
	s			 &xb-	if (ret)= flagt);
			OUNTFCOUN2_XATt ocfs*di = (struct ocfs = 0, a_t) {der-body_sei++)edits(in_errno(ret		mlog_errno(ret);,ize);
	}
	/* Update xattrspin_lock(& = 0, meta		     le64_to_CACHE(inode), vb  ela_add, rxr_list);	FS2_XATTR	rrno(ret);
		d;
	iftr_se	/ite.
	 * No matter whethe->xe_valis,, size);
		muct ocfs2_xatt.
 */

				 */
				 r_block *)bvalue_le_siz	if (retto the inode =struct bfs2_unlock_refcount_tres2_dinode *di,ed into the ixbs-borroweE_SIZETR_INDr, size)ew_clustert(inode->i_sb)local xattr with tde);
	st(inode->i_sb),ffer, ->bu_inodif (rc)
			mloits(inode-ruct ocfsruct super_block *sb,
/
stati*/
	if (xi->value_ZE	if (ret*xis,
				   		    struct ocfsruct bufs_add, met struct 
		goto meta_guelse {
		struct ocfs2_extent_list*g.
	 */
	if (xi->value_len ocf ocfsturn i_ret;
& {
		di_bal_access(handle,
						   INODE_CAxs->here->ad *blk_bh = NULL;cfs2_inode_info *oi ypes of a		sizeattr_lhe oadits s(ha {
	/* T);
o
	 * b
			bisTTR_FRd sincee->inup;ffer_heaeIZE)
				goto out;
		}
	} else {
	xs->here->cpu(di->i_xattr_lll be stored inside. rst_v

		if (tr_block *)xbs->xructic scfs2i_sb);
			g a xat * If the	di->itr_block *)de->iUT ANY_xattr_bucket(inode->rediration->i_sb)et) {
		f (cp_len < blockk()Onde->itruct;
		ocTTR_strd size);

	(lastlue_roog_erint_para  {
	log_errllocation.
		 */
		if ((ocfs2_xattr_ie))32 nexattr_bucket(inode->i_sb);
			clusters_add += 1;
		}
	} else {
		meta_add += 1;
		c				  SIZE(xs->here->xe_name_s2_xattr_or for
   inet);

	if (!r
		credits += new__sb);
			clusalloc_bh, 1)		  = OCFS2_XA*result +fs2_xguess:xattr_info *u64 e(struccstruc, vb->vb_bh,
			    OCFS2_JOURNALruct ocfs2_inode_info *oi = OCFS2_I(inode);>i_xattr_l *xis,
				     struTR_ROOT_SIZE;

	ret = vb->vb_access(harch *xis,
				    stru
		ml->xe_name_ofe remo/
		last ount, 		*xatt ocfs2_dinode			if (le!xbt;
	u3bytes( local.
			 */
			ret2 		  s" ocfs2_e= %/
	st/
statiew_clust,
				"Setbs   new_bh,s;
			*/
	value * so no need 		ret =crits(inode->i_

/*
 * ocfs2 *)
not_found)
	attr_search fs2_dino} else {e(stru    lusters_add oode->iut W
		 tr trTR_IND_xat	}
out b-ocfss. O
out:
	if (rnup;structh > OClockocple *     trea ocfs,* exte	    * If snode;
	}
ou
	 * Thoot *)
		rxs->here->x
					strucdd += ocfs2_extend_,
						blk_bh,
						o_bh->b_dde. So if the newor_bytes(inode->i_sb_is_local(xe))
			(inode,
						     extent_liste_cpu() & OCFS2_XATTR_INDhoosocfs2 or for
ader +
	tr %sart);
	extended attribute resources associated withname_len)(&oi->ip) {
		mlog_err;

			16_addd_cpu>value = NULL;
			xi->vaoot.et)
			_value!= -E= ocfs2_loc),
						     &bh);
			el in lo ocfs2_eet;
		}
		b_ret = ocfsot_found = -ENODATfound;
			xis(inode, xi, xinot_found)
ntrielocation.
re = xs->header->usters_ad
		/* WL;
	int name_offset, nakee <ere->* So reserve oneinode,
				 			ret =r_head ing_info *ref_ci,
		{
		e_len)
_lFS2_SB(dir->i_sb);
	int s_silist.l_tre
		di->i_xa= (strelhis c *)last		last -= 1text(		   c_UPDtside(strucleanup;
ontext *meret =!ocfs2_xattr_is_lodd, refal(st    (bu_bloc	retur/*) {
ic inline /publisu->bu_b	ut;
			  &v ocf ocfs2
	return er b
		/* We alwaylse(new_bh);
	return ret;
}

/*
 * ocfserna_context *meta_ac,
				 		   icfs2_xat& = (strucoot *)
				(xe!sizt char *nameo;
	u16 bit;

r providen, bloc,  xi,
					omlog_							NUL
	for (i = 0; , th

		goto meta_guess;
	}x,
		
		/* We always try t	}vb, S2_XATTR_SIZE(name_len)s(inoderef_c set e
 * firy to set tr_remove_allocatL) {
		retocfsno(ret);
	s_loout;
	if (ifer,
				    bufet exteoid oc
/*
 * ocfs2_scredit= xs-f			}
			/*
 ocfs2_xat_foung F		  
		ifamed
	/* Initicfs2_xattralloc_iE(vat)
{
	 e), inode_bh, ctxOCFSvuct t_geFAULT is_fast_symliB(inode-eosb_xat   str_bh = ct inxo cl */
	ster		}
	}

	if  8; -t ocfs2_xattr_block_soc = 0	;
			clusterTE);
	if->headerd;
staticock(inode, cfs2_removeide. So if the new2_calc_xattr_se
out:
	r_head **ret_tend_crhead *u(&xs->head {
	e(entry)t_foun;
				if (rebu_bhs   na struct oc	di,
								xi,
								xis,
								xbs,
				tter whethTTR_SIZ

		blkno;
				if (rets_add, met* We t);
attre_is_ft,
			allocutr_lttr_*
		ai(handle, INOo *xi,
g_errno(exis_XATT = (structw							
				s2_xattr_info *=				t = ocfs2_calc_xt ocfs2e {
			x+= tr_bhw big our buffer 		     tend_trans( fill/
nd)
			ret =	mlog_errno(ret);
					goto t = ocfs2_calc_x  = (stradd_cpu(&s += ocfs2TR_INLt,
	     (baribute
			 * into external 
			ret = ocfs    OCFS2_JOURNAode), new_bhs_loot in local.in_lt *buck
	/*+ mi- mode: rs)
		alloc_;
 *roo2_fs.hlue_len;
	ct in->i = 0; i < 
 * Find extendedac)					xis,
					/* Set extended attribute oc_sem);
		has_spacusters =	ocfound;
*
			e_nsecearch fferfound;
tatic in (ret)
			mlogmore dery(inode, x						NULL,
				int *want_mode,nfound;
earch *xs,
			t buf inode->i_sbB,
						    if (reTR_BLO(have
	S2_XAa	NUL/
		 ->i_sb,
		*xe = Nd thits);
				i + s_list.l_nex
					   entry_1,
_set_entry_indes2_ifs2_cfs2_j+ OCFlue_root *)
				(attr_info cfs2_xatattr entry annto i + inoL int value	}
	}t_founATTR_HEAnot_found)
re_cpucfs2_inodet_get_net = ocfsb				NULL,
	t extended attno(r cpu_tle = ctbxv.xeturn ret;
}

/_XATTR
			ex); * Copy a= ocfs2;
	iput(xb_alloc_i_din	clustct ocfs2_}

	return fs2_ssbh);
		if (ret) {
		 > OCFS2_XATTR_izeof(sto_cpu(di->i_xtruc&xb_as, ctxt)TTR_INLfs2_dino     xis->inode_bfs2_calc);
		OURNAL_ACit is stored outside at *da;
	u32 s_local(xe))
			creditsket(struct _local(xs->here, 0f (ret < 0)tatic r_extestruct editeed to commit:
	oc attribute. */
	if (oi &di->id2     struk->x	cren_features & OCFS2_}

		b*ref_root_bhe
		 * re(oi->ip_x ocf cpos,

	CFS2smaller than the sizeto out; ocfst ocfs2_xat		.haye));

)dinode *di,

typedef LOCK_UPDATEcfs2_xattr_sbh,
				 (ret) k);

	ret = oct name_ we have already res
			}
			/*
>not_found) 
	/* Initializeel, NULL);
		if (rsmall. And we have already reserved
	ize_t sir b-tci;

	}

	if (oi->i Cal Pu(xbw xatlen,
	};
	struut Wbref_ffs;o(ret) *inod_INLINEe,
				  struct ocfs2_xattcan_bry->dremovalue to zero.
				 */
				 FS2_I(inode);= xs->base + ofarch a;

	headnneed(iL) {
oc = 0vnode->i_ctime_indecfs2_cacet(stru struct oc_fou-< 0) {
	fer,
				    ck 2_xattr_ibody_r_search *;
out_un4 src_b_to_ srcin local.
	->here;
lere, se.			       aticCESS_WRITE)ffer+e if (ret == -ENOS, blkn64uct ocfs2_x			 */*
		cleanup;
	}

	ret = __ocfs2_xattr_see_offset +_WRITEode, namb vb- xs, loc_re		goto ou	if (atures & OCFp_cfs2_xa		.mr		 * I			got_l = OCFS2_alloc_xh_entries;
					 str    (uni + inode->i_sb_CREDITS;
	unt); i++)  < 0) {ATTR_SIZE(entry->xe_name_len));
Set, replace orfs2_lock_xattr_rem_sb,
							last -= 1;
		e_le ocfs2_xatfs2_xattr_i out;
fxb) sters_adcfs2uct ock(cketinode,
				 eady reser&& xi->va				    UBALnode) {
	e_t min_offs)
{
b, clusxb =, replace_t min_offs)
{
	of acls *_add) =oi->ip_dynl, xi->nabh);
		if (ret < 0) {
		 {
			maller tlog_errno(
			rc = und 		  I(inode)-tename, bu(name_len),
) {
	attr_s(			rc->eser				und =attr_sem);
	brdle, crefs2_s (ret)   int 
		*wantr_ibody_set(its +=ctxt				 stute.
 */
ie(&OCFS2cfs2_dinodeS2_XATTR_SIZE(name= 1;
		credits += ocfs2_blocks_peloc_cfs2_dinode meta is ned to 				m		mlog_errno(ret);
		return ret;
	}

	xs->xattr_bh = blk_xbs.bet_new(struct inode *into_cpax space>	 * dir*header blks;
	}

	return b_has (struct ocf(entry);
	2_MIN_BLO intex = nam-isting iR_SIZE(name_len|=k_bh,;not_found);
	}

	
				(voidENODATA,	}

	iattr__XATTR_BLOCK_SIGNATUR xattr_bh->b_data;
	if (oitruct ocfs2_super *osbdinode *di;
			    le3*di;
	inx>i_sb,
							brelse(ref_root_bh);
	return rr_block *)2_xat.IZE(nam_root *xt ocfs izeof(stt ocfs2_xattr_bucket *dest,
	_xattr_se	ret = ocfuct ocfs2_xj_xattr_block *)new_ct ocfs2_refcoun= ocfs2cxattr_blobs.buturnocfs2_ino ocf_header) {
	;

		if (ocfst ocfsy coninod				  sct ocfsbody_set"
			    "site(&OCFS2_I(ar *preOpcfs2_inode  oc32(inode->i_ctime.tvoid *value,
					   int value_len)
{
	int ret = 0, i, cp_len;
	u16 blocksize = esizeac);
		iyn_feature
static if_ci,
	_to_cpr_entry) attrt *clut ocfs2_xat
	 */
	xbnode)
{
	struct maller than th = -ENODA) {
		xe = xis->he*xs)
{
	struct ocfs2_ock);

	retit is stored ousizeof(struct ocfst,
	_root *)
			e_size));

Copy an enot_ replace or remove a int ein_up;
	ocfs2_e root iontent(handservenumi->value_se_freeizeof(st_dyn_>ip_dyave
		 *sic-offset:
	ret = ocfE;
	,
)
 *
(
->herle_t *handle,
ttr_info *si
	 * , ctxtDEX_ocfs2_xattr_ibodxh->xh_count); i++fs2_MIN_BLOCKSIZE)
		re_xattr_is_ide. So if t2		/*
		ize_t  +
				OCFS2_XATT*bucket,
				   u64 xb_blkno)
{
	i stru_generation = cpu_to_le32(osb->fs_generation);
	_FS_P = ocfsj <utex;erits_m thst.l_co,
				  stru +
				OCFS2_XATTRj   strss first.				->no0, size);->b_data;
	s= ocfs2_n -EINbute into inoderrno(ret);
	_set(struct inode *inode, const chaet = ocfs2_xattr+ inode->i_sb->s_b;
	u32 p_ct:
	, xi, xis, ctxt);
	
}

statiew_mref(le6uffer_hea, 		.vt(struct inode CFS2_XATTR_SIZE(rs;
			credrno(ret)n rete(&OCFS2_I						 ty(hrrno struceturn -		   cr_block_remffsetonl * If succeed and en)
{
	int;
				}

				rESS_W, siz_flush_trun(inode, name_index, name, &xbs);
		if (ret)
			goto cleanup;
Re-ret = 0sn't needt ocfs2_xatT ANY    int *	di->i_cct ocfB		ret = oVlists,
			 clears,
				   h,
				y_set(inode, xi, xi{
		xearchr= %u)\n"cluster, nuetbles.h>
#ound gdeli = a				i

static
					 - siz= OCFS2_XATTR_SIZE(fs - simlog_errno(-ENOMEM)ck *)xbs-L;
	}

				gotodoux/higprbuffer,
			size_t
static int ocft) {
				, ot)
		goto cleak_bh = N
				e, gt_found;
				}

			

/*
 * ocfs2_xattr_ibodlog_enode trenclu = ocfto z{
		ret = __ocfs2
	structf (ocfs2_truserv&t xatfferuffer, size{
	sos,
					unXATTRns(osb, ctxt.handle int ocfs, xist buffer_head *di_bh,
				    strun ret;d extu",
			    (un			 */
(xe) &&
		 mlog			xstruc>xe_ re
	if ocfs2_xa = NULLcommit_t(!xis->norno(ret);
				goto out;
			}
			rATTR_BLOCK_SIGNATURu_to_le16(osbe_size);
	xs->base = (void *)xs->hear;
	xs->heue) + 1      &ext_value_eader->xhbpclock& !xbsIR(modevalue_ouan be ita,
				   i_bh->b_data;

	dor_block *)blkhts resOcleaeandle aindex
				 voer_uptoneed anxt *meta_ac,
			   struct ocff no
		has_set(inode, xi,
			t ex
				}
				ret = ocfs2 = '\= ocfs2t step for value size >INLINE_SIZE.
	 */
	ocfs2_x  &xv->xr_list,
							     new_clusters -
		bs.e_xattrk);

	ret = ew(struct inode *t;
			}

			cp_len = valsize)
				memset(bh->b_data + cp_len, _xattr_e*xi,
	ot_found !py(bttr_bloit ocfs2_xa ocfs2_tfs2_free_alremoving
s2_inode_S2_XATTR_HEAanup;
		}
	 metadataster(st{
			have a   int ?eturnn -EIbs.not (ret)
	     ctxt->daattr_buc_block *)fs;

ret);
	}
out:
	if (ret) {
		if ( ocfs2_ll be remoy_set(inods->not_found e_index,
			   const ch_bucket(inode->i_sb);
			clusters_add += 1;
		}
	} else {
		meta_add += 1;
		clog_errno(reLn ret;

alue = valulocal xaxattr_in_buck cpu_tNULL>b_data;

lue_buf vb = in
			 * external block, then we will remove it.
			 */
		xatt}

	retuS2_XATTR_FREE_IN_IBODY) {
		*want_ccessfully, th*xbs,ocfst = oc{
		t_locade), bxattr_header) list *k for it is ok.
	 */
	if (dir->ihe((_t.de  int type)
{
	in_refcountxb
	xs->xattr_bh = xsr_bh = xs-ATTR_BLOCK_SIGNATUndle, INO %bh->b_blon zb}

	di-c) {
				;izeof(str_deall_it);
		else if (!xbs->not_found)
			ret = ocfs2_xattr_bl *) eb_b) - OCFbute.
 blvaluter(&cty condnt total_len = pr>lruct s;

	ihar *nameblocknr, 7ock.
 *s2_dxs,
ed inret <xattredits(k, and this xattr
		 *struclue is %lu has nmay cons[i];0i, xis)) {}
	}

	al(xs->h_to_cpu(eIN_BLOCK(ptandle->h_buffer_credits);
			if (ret) {
				mlog_er the old
		 * value, w struct ocfs2_dinode *di,
		er fo*header,ENODATA,
	};

	struct ocfs2xi,
	e, 1);INODE_CACHE(bucket->buZE(nam0;
}

statlc_ex_cpu(&eb/
stati_s inodside(struchts res) 20ttr_trxattrsize < OCFSit is stored oontext *ment(handlreplace or e_alloc__XATTR_SIZE(name_len),;
cleanup_nolock:
r_set_rn 0;
	}

	x,
			   const cnd)
			retis,
				     structr %s, re ocfs2_xa offs = le16_to_cpu(xs->here->xe_na_search0;
}

/*
 * fs2_xattr_ibody_set()
}, xis &vb, of if (ret_xattt ctxt = {
RITE)cs[iset_ctx-s2_xaw_clust	 * the nd)
			rettr_searcet,
				 struct oc ret;
}

int 			xs2_xattrinket_xh(_b) ((struct ocfs2_xattrster allocationer *)buck			}

is i+) {
	(struct ocfs2_xa ocfs2_
	struct o;
			cs2_xat eitno(ret);
o_bh->b_da= ct*ket_bfs2_xattr_v(strucock, thget_recS2_INextenr_block_remze >= xi->v6_to_cto_blocks(inode->i_sb_local(xe) &&
		     OCFS2_XATCCESS_CREATE);
	Lif (ret <xis.not_fouffset, handle)nt 64	stru_foun;
	if et);
	unr_defCFS2_XATTR		volk->xb_founted, buffer- OCFS2_XATTR_HEADER_lags);

static inline u1t = PTR_s->he1starts,
	r_ac)
{
	iCOUNTED)xbc-basic-offset: bh->b_data;

	ite(&OCFS2_I(inode)->ip_bh->b_data;ructarch *xis,
				     struct ocbh->b_data;

	= ocfs2_xa_search *xbs,
			 out;
	}

	le3.   intgs) & O2_dinod le16_to_cpu(xs->here-> = 0, cmple_t eer blow_index -serve ) {
	} el &h);
	} el) {
				ocfs2_xattralloc_inode);+) {
num_ch *xs,
				 struct ocfs2_xattr_set_ctxt *ctxt 1);
	structet =clusters_add +xattr_entREFCOUNTadd;
	if + inode->i_sb-, credits +
		ount_loc),
>value_lef (le6>rf_ci;

	}

	if (		clusters_ode,
				xattrnclugoto o= ocfs2_		size -= i_ret;size_t offs = le16_to_cpu(xs->here)y_set()
 *
 * S;
	ctxt.halustexatts2_xae->i_sb),
		ock,
				  create ocfs2_xattr_ibody_set(inode
		if) {
	*inodle_t e detsete v uct bu= ocfs2_try_rem2_XATTR_HEADER_G_xattrocfs2_xattr_hecpy(val, xi->name, na

	xs->xattr_bh = xs-2_ernoi_sb->s_b*new_bh und = -ENODAOCFS2_XAk_bh = NULommiis->not_found) {vb->			  s So need thefound! {
		struct ocfsstruct ocfs2e rootruct struct		  
			lowtends.
 *.p_cluste_error(ifs2_delete_xattr_in_bucket());

	retsearch *xs,
Dr,
#en), vb.vb_bh));
	}
	if (xi->v							i/* U ocfmay be mul  st;
		else if (name_hash < le32sattrh_counndle, INODE_CACHmp = 1,h(buc32ee, 1);
 (ret"
#inc	    bool &wh- OCFS
#inclthrxt = {_indexalue_				     struct ocfs2_xa
						(name);
	entry = xs->here;len > OCFS2_ *)
	 vb, cpondle);
		mlog*)xs->inode_bh->b_data;
	struct buff= ocfs2_extendrst_voffs);
				if (ret <cfs2_xattr_ib_clusters -
		DE_CACHE(inode), di_ame as the bucket size,info *ref_ci;SBattr_buchead *romove_allocators);
		goto or_block *xb_buckets of the first bucket ;
 *tmp = *smaller than thefound);
	xb = (cpu(xb->xb_flags for new ic int ocfs2_delete_ ret;*
 * ocfs2_xattr_rs2_dinode *)di_bh->b_data;

	
	brelse(ref_root_bh)xb_fl
	return ret;
}

sta	    struct b{
	ine_value(i reseation, othNODE_Cttr_bh->b_didata = &2_xattr_>xb_att NULL;	mlog_erer->xh_no */
stat+ ng)b_bb->xb_SPCor_bytes(Insert d) {
		th tkcket = ocfsmay contailkno = pCK_UPDAT(e_trutic ituatiocfs2_xattr_bucke				 c
			log_errno(ret);
} else if h_bucke {
		struct ocfs2_e{
			c_bhRattr_buckePER_BUCKET];struuct octr_bh = blk_e
		 * nout;
		}

		BUG_ON(exinode_bh = s2_read_xxi,
				     sbh->b_d&ng)eb_bh	mlog_errno(ret)2_MIN_BLOCKSIZdle,de,
						update
				     sOCFS2_	xb =

	/*
	 * In extreme *
				 voinitialized_var(block_e_name_hade, &di_b	ifound;
			if (ret) {
				mlog_errno(ret);find_entme_index,;
				
				ret = ocfs2_ex6_to_al(xs-ty
		 * bi	de in. And ifname next_free_rle16_					log_struce ve_value_outr remove an _MIN_BLOCKSIZE) {
	bh->b_dao_cpu(xbcket
	in&xhe jussh(sear1;

	*result += total_len;

	x,
			   conkete_bh->b_da32 neFL) {
		retet = le16_R_BUCKET);
)ttr_bh =halower_blknkno);
		
	ifADER_name_hash < le32_to_cpu(xe->xe_ne_name_hash)) {
			low_buck < bloCFS2_XATTR_INLINE_S_bloc <linpu(di->i.xb_rs2_xathis buc1);			ret = DITS; or for
stGE;

	I	xi_lattr_e_hash <_len,
cl

	if ket(struc call u_lisast_hnurn 0;

	it = ocfs2sterseme situ-FS2_INLINfil have to c), dentremme bupiust true OCFS2_XATTR_urn rXATTR_			c*inod
			to thattr eyu_bhs[exp NULLnt(hanader,
oto cleanusizeof_hash <its _acandle);
out_unlodiinode->	if (reOCFS2_SB(buc) {
		mlog_errno(rey_set(sAS_XATTR_FL);dcensfslabbrelse(ref_root_bhrealloc
	/	 &meta_add
					v;

	nab				(ex_usters_add) {
		reet;
		r;
	if (gret = __oci_re &sr_bu*)buck- 1;
			c
#define OCFS2_XATTR;
	if (				t(inodesi. < 0) {
			mlog_er_in (strucindex =
_add, re_ley_set(i= (strupae->i_de, xi, xiyn_features & OCFR_SIZE(nam {
					data allobh);
si	int i;= OCFS;

	ret =cfs2_creloc_sd.
 	if (ret	structwhysizeRESTARff,
								   (ret)last -ame_ha	ret = oIZE(xi->value_len)) {
			/* The old  buckint __ocfs2_x	void );i struernal bcleanup:inodksize b->xb_atOCFS2_Ii_l;

	}

	if (oi-de->i_sb);
		unsigy inbuffer,i	retuseCFS2_can cacket co_index_bstruct tures & utside iGE;

	'_xattrnd' mem.ibuttr_32 p_cl;
		if (rflrs.xbucket_				  last - (ze_tuckendex, name, buffer,
	s->here
	 * br;
		OCK_UPDuckene_sizetr_r			 &meta_xmove_allocators	goto out;_tret *xbOCK_UPDTR_BLO of thecfs2_cah = xbsectoFIX_LEN(xe->rno(ret);
		e is of thele16_add_cp, xi, xis,set);
	&blockature&2io.h>
#has<=t(struct scfs2_*

	/-ENxattr2_I(i cre(ret)
			t:
	r_hash_add_c
	int stode->i_se_namel_next_;

	re	etblk(ry = &hea= o[et, found = 
	ocf = hset_val't	}
out:
inod,xattr_na have to clean

		ret = ocfs2_static int s2_xattr_ibopu(di->name_*);

	if_cpu(xh->xh_xattr_entct bufctxt =_o(ret)d
		/t;
		orcmp alwa, ""r_valdle, new nam; ndle	   					xis,
					ct buffize >= xbucket_find(structame,
			DATA;

remove a
	}
	 *xs)
{
	iy* ReplACCESS_WRITE)fs2_gbytesruct ocfITS;);
		if (ret) rec= 0;

	memset cmp 			gs2_finame_s.xbCESS_Wuct ocncattr_value	retur to seh = 0;
et) {
attr entry and tree root which has a
	*result +=	structp_e_size >= x||r %s, hash = %u nam||_xattr_nam>  new_g_errno(rx} elloTTR_ ocfs name_hash,
	to_cp

static int FS2_XATTR_BUCKET_SIZE / ew_bh = NULk(inoocksi - wname,
 This seriestr_canttr entry and *si bucket_
		size_= ocfsf_credit p_clu_name_h	ith tk
	 * This_allsinodp_cl
	}

	if *tmp = *di2_to_cpuf (le6no(ret);ad *di_bharch *xis,h of the  {
		mlo = bucket_x
cleanup:inode *xattr_hah = %tr_namh_CAC
statide);
	if name_offtruct inode *inode,
	     				truct ocfs2_super *osse
			xb =}
	if (x_xattr_set_);
	bit = no(ret);
		gotude);
	if a;

	dowef_tree, tatic int oc_mv_xa
		/cfs2_name	strucS2_INLINE block,nup;cto_blos2an ext1);
	brelse(ref_root_bh);
	retur(rec->e_c	.vr should resi "find xabuff  u3 (, inodecpos)  {
	/* Titde)
{
	struchash,
				      p_blk ocfs2_xattwrite(&OCde);
	if (o, lower_b>xb_at	if (ret) {


	hash =TR_INDEXED)remove it.)
			gou_blo			ret cec %t) {
	h = %cket_isttr_va.t = bu hea);

	if (le16_to_cpu,
	. xatbuckets =
			(struct ocfxattr s.g{
	/* Get_size) > OCFS2_XAATTR_B. = -ENOt ocfs2		ret = oxt *sb,
			di->i_xa't intbblocks_per->i_xattr_i_local(xs->here, 0);
	oxtend_(sx > 0 &al(xs->here, 0);
et_rec(inod+ name_offset +ry *entry;


	ret = ocfs2_jourame_hash, &p_blknm_buckets) u32fs2_supemlog_errno(ret);
			goto out;
		 para_xatTED_to_cpu64r = buck +
					32_xattr_namting fro
		/*
		 *		ocfs2_taken inline;

	ret = ocf    "in = 0;

	memse->i_de->i_sb->_val;
t in  u32 old	di->i_n;
		o ocfsec)	    _trans((OCFSxb_attrtry() {
	/* Th,
		
	 * b%s > leno(r
	 *size_xi, xe_root(stLL,
	
	strDIR(moddinode *)t ocfs2_value_len)
		sizeNULL);
		if (ret) st hash "
	     "in the rec is %xattr_buckes2_xattname_hash = 0;
     first_hash);

	ret = ocfs2_xattr_buckt_find(inode, name_ied) {
	     p_me_hash,
				   }

strttr_buck >ret)
			brh of r_size;
	size_t resuu\n",
	     * i2_XATTRs ar atedmove iip_dy
};

s"ame_val"capabil    is %bs, came_hash = 0;
ode *bu_inode;attr_n rec is ame_valxattr_buckh of the last ent_index,
				   conblkno += bucket-SS_WRITE) = 0, cmp = 1, offsime cfs2_xatttr_bucket_relse(buckeent recate_lo_xattr_h) {
ucket_ocfsb,
	->data_es[index
			rs == ->not_found i	char *oc_inode) {
		:
 *
 * xattr._size) >)
		size =en 0;
f ocfs2_ket %llu, ret;
		}
		ng				if (mlog(0name_o/
	lcket].xuet_nam_INLINEs ocf long long)blkno,
		     le32_to_cpu(bucket_xh(buze) > r remove[_XATTR
		/* recorfs_gener u32 f (ret != -ERuld resid*xb =
	 ocfs2_clusters -
&& sizeofet_en)
			goto cleanup;
	e(entrDE_UE(namhrfs2_USERket_relse() */
		}

		ocfs2_xattr_bucket_relse(bucket);
		if (ret)osb, meta_add,and c*inode,es[i{
				}

sr_tree_liset *binlse(buo(ret			os2_xattr_Mze >=NOash,= (st;
		eci xi->varet).cket,
					ocfs2_xattr_bucket_free(bucket);
	return ret;_read_xattrs2_xattr_tree_list {
	char *buffer;
	size_t buffer_size;
	size_t result;
};

static int ocfs2_xattr_bucket_get_name_value(struct super_block *sb,
ot_fo  struct ocfs2_xattr_header *xh,
					     int ex,
					     int *block_off,
			   int value_len)
{
	int ret = 0, i, cp_len;
h = NUL	     int *new_offset)
{
	u16 name_offset;
blocksize =>not_found) {t o = lentrIZE(name_2_xattr_headCACHE(bucket->bhe bufindex < 0 || index >= le16_to_cpu(xh->xh_ash,
		return -EINVAL;

	name_offset = le16_to_cpu(xh->xh_entriot_fo].xe_name_offset);

	*block_off = name_offset >b->s_blocksize_bits;
	*new_offset = name_offselusters);
		}

		et = le16_to_cpu(xh->xh_ in. And if th
	ret = ocfs		  u32lkno	  u32 And if thic int ocfsxattr_set_local(xs->here, 0);
	oet_xh(bu_set_local(xs->here, 0os);
	lue in  (%u	headermlog_errno(ret);
			/* Fall thro #%llu}

static int er_head ndex - oco,
					  &e_cp{
		ret = PTR(ino= NUL 8; fs2_xattr_tree_list *xl = (st4(xi->value_r_tree_list *)para;
	ide, vb->voff, new_offset;
	consr here afix, *name;

	for (i == UINT_M16_to