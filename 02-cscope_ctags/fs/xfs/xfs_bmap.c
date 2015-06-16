/*
 * Copyright (c) 2000-2006 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write the Free Software Foundation,
 * Inc.,  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_types.h"
#include "xfs_bit.h"
#include "xfs_log.h"
#include "xfs_inum.h"
#include "xfs_trans.h"
#include "xfs_sb.h"
#include "xfs_ag.h"
#include "xfs_dir2.h"
#include "xfs_da_btree.h"
#include "xfs_bmap_btree.h"
#include "xfs_alloc_btree.h"
#include "xfs_ialloc_btree.h"
#include "xfs_dir2_sf.h"
#include "xfs_attr_sf.h"
#include "xfs_dinode.h"
#include "xfs_inode.h"
#include "xfs_btree.h"
#include "xfs_dmapi.h"
#include "xfs_mount.h"
#include "xfs_ialloc.h"
#include "xfs_itable.h"
#include "xfs_dir2_data.h"
#include "xfs_dir2_leaf.h"
#include "xfs_dir2_block.h"
#include "xfs_inode_item.h"
#include "xfs_extfree_item.h"
#include "xfs_alloc.h"
#include "xfs_bmap.h"
#include "xfs_rtalloc.h"
#include "xfs_error.h"
#include "xfs_attr_leaf.h"
#include "xfs_rw.h"
#include "xfs_quota.h"
#include "xfs_trans_space.h"
#include "xfs_buf_item.h"
#include "xfs_filestream.h"
#include "xfs_vnodeops.h"


#ifdef DEBUG
STATIC void
xfs_bmap_check_leaf_extents(xfs_btree_cur_t *cur, xfs_inode_t *ip, int whichfork);
#endif

kmem_zone_t		*xfs_bmap_free_item_zone;

/*
 * Prototypes for internal bmap routines.
 */


/*
 * Called from xfs_bmap_add_attrfork to handle extents format files.
 */
STATIC int					/* error */
xfs_bmap_add_attrfork_extents(
	xfs_trans_t		*tp,		/* transaction pointer */
	xfs_inode_t		*ip,		/* incore inode pointer */
	xfs_fsblock_t		*firstblock,	/* first block allocated */
	xfs_bmap_free_t		*flist,		/* blocks to free at commit */
	int			*flags);	/* inode logging flags */

/*
 * Called from xfs_bmap_add_attrfork to handle local format files.
 */
STATIC int					/* error */
xfs_bmap_add_attrfork_local(
	xfs_trans_t		*tp,		/* transaction pointer */
	xfs_inode_t		*ip,		/* incore inode pointer */
	xfs_fsblock_t		*firstblock,	/* first block allocated */
	xfs_bmap_free_t		*flist,		/* blocks to free at commit */
	int			*flags);	/* inode logging flags */

/*
 * Called by xfs_bmapi to update file extent records and the btree
 * after allocating space (or doing a delayed allocation).
 */
STATIC int				/* error */
xfs_bmap_add_extent(
	xfs_inode_t		*ip,	/* incore inode pointer */
	xfs_extnum_t		idx,	/* extent number to update/insert */
	xfs_btree_cur_t		**curp,	/* if *curp is null, not a btree */
	xfs_bmbt_irec_t		*new,	/* new data to add to file extents */
	xfs_fsblock_t		*first,	/* pointer to firstblock variable */
	xfs_bmap_free_t		*flist,	/* list of extents to be freed */
	int			*logflagsp, /* inode logging flags */
	xfs_extdelta_t		*delta, /* Change made to incore extents */
	int			whichfork, /* data or attr fork */
	int			rsvd);	/* OK to allocate reserved blocks */

/*
 * Called by xfs_bmap_add_extent to handle cases converting a delayed
 * allocation to a real allocation.
 */
STATIC int				/* error */
xfs_bmap_add_extent_delay_real(
	xfs_inode_t		*ip,	/* incore inode pointer */
	xfs_extnum_t		idx,	/* extent number to update/insert */
	xfs_btree_cur_t		**curp,	/* if *curp is null, not a btree */
	xfs_bmbt_irec_t		*new,	/* new data to add to file extents */
	xfs_filblks_t		*dnew,	/* new delayed-alloc indirect blocks */
	xfs_fsblock_t		*first,	/* pointer to firstblock variable */
	xfs_bmap_free_t		*flist,	/* list of extents to be freed */
	int			*logflagsp, /* inode logging flags */
	xfs_extdelta_t		*delta, /* Change made to incore extents */
	int			rsvd);	/* OK to allocate reserved blocks */

/*
 * Called by xfs_bmap_add_extent to handle cases converting a hole
 * to a delayed allocation.
 */
STATIC int				/* error */
xfs_bmap_add_extent_hole_delay(
	xfs_inode_t		*ip,	/* incore inode pointer */
	xfs_extnum_t		idx,	/* extent number to update/insert */
	xfs_bmbt_irec_t		*new,	/* new data to add to file extents */
	int			*logflagsp,/* inode logging flags */
	xfs_extdelta_t		*delta, /* Change made to incore extents */
	int			rsvd);	/* OK to allocate reserved blocks */

/*
 * Called by xfs_bmap_add_extent to handle cases converting a hole
 * to a real allocation.
 */
STATIC int				/* error */
xfs_bmap_add_extent_hole_real(
	xfs_inode_t		*ip,	/* incore inode pointer */
	xfs_extnum_t		idx,	/* extent number to update/insert */
	xfs_btree_cur_t		*cur,	/* if null, not a btree */
	xfs_bmbt_irec_t		*new,	/* new data to add to file extents */
	int			*logflagsp, /* inode logging flags */
	xfs_extdelta_t		*delta, /* Change made to incore extents */
	int			whichfork); /* data or attr fork */

/*
 * Called by xfs_bmap_add_extent to handle cases converting an unwritten
 * allocation to a real allocation or vice versa.
 */
STATIC int				/* error */
xfs_bmap_add_extent_unwritten_real(
	xfs_inode_t		*ip,	/* incore inode pointer */
	xfs_extnum_t		idx,	/* extent number to update/insert */
	xfs_btree_cur_t		**curp,	/* if *curp is null, not a btree */
	xfs_bmbt_irec_t		*new,	/* new data to add to file extents */
	int			*logflagsp, /* inode logging flags */
	xfs_extdelta_t		*delta); /* Change made to incore extents */

/*
 * xfs_bmap_alloc is called by xfs_bmapi to allocate an extent for a file.
 * It figures out where to ask the underlying allocator to put the new extent.
 */
STATIC int				/* error */
xfs_bmap_alloc(
	xfs_bmalloca_t		*ap);	/* bmap alloc argument struct */

/*
 * Transform a btree format file with only one leaf node, where the
 * extents list will fit in the inode, into an extents format file.
 * Since the file extents are already in-core, all we have to do is
 * give up the space for the btree root and pitch the leaf block.
 */
STATIC int				/* error */
xfs_bmap_btree_to_extents(
	xfs_trans_t		*tp,	/* transaction pointer */
	xfs_inode_t		*ip,	/* incore inode pointer */
	xfs_btree_cur_t		*cur,	/* btree cursor */
	int			*logflagsp, /* inode logging flags */
	int			whichfork); /* data or attr fork */

/*
 * Called by xfs_bmapi to update file extent records and the btree
 * after removing space (or undoing a delayed allocation).
 */
STATIC int				/* error */
xfs_bmap_del_extent(
	xfs_inode_t		*ip,	/* incore inode pointer */
	xfs_trans_t		*tp,	/* current trans pointer */
	xfs_extnum_t		idx,	/* extent number to update/insert */
	xfs_bmap_free_t		*flist,	/* list of extents to be freed */
	xfs_btree_cur_t		*cur,	/* if null, not a btree */
	xfs_bmbt_irec_t		*new,	/* new data to add to file extents */
	int			*logflagsp,/* inode logging flags */
	xfs_extdelta_t		*delta, /* Change made to incore extents */
	int			whichfork, /* data or attr fork */
	int			rsvd);	 /* OK to allocate reserved blocks */

/*
 * Remove the entry "free" from the free item list.  Prev points to the
 * previous entry, unless "free" is the head of the list.
 */
STATIC void
xfs_bmap_del_free(
	xfs_bmap_free_t		*flist,	/* free item list header */
	xfs_bmap_free_item_t	*prev,	/* previous item on list, if any */
	xfs_bmap_free_item_t	*free);	/* list item to be freed */

/*
 * Convert an extents-format file into a btree-format file.
 * The new file will have a root block (in the inode) and a single child block.
 */
STATIC int					/* error */
xfs_bmap_extents_to_btree(
	xfs_trans_t		*tp,		/* transaction pointer */
	xfs_inode_t		*ip,		/* incore inode pointer */
	xfs_fsblock_t		*firstblock,	/* first-block-allocated */
	xfs_bmap_free_t		*flist,		/* blocks freed in xaction */
	xfs_btree_cur_t		**curp,		/* cursor returned to caller */
	int			wasdel,		/* converting a delayed alloc */
	int			*logflagsp,	/* inode logging flags */
	int			whichfork);	/* data or attr fork */

/*
 * Convert a local file to an extents file.
 * This code is sort of bogus, since the file data needs to get
 * logged so it won't be lost.  The bmap-level manipulations are ok, though.
 */
STATIC int				/* error */
xfs_bmap_local_to_extents(
	xfs_trans_t	*tp,		/* transaction pointer */
	xfs_inode_t	*ip,		/* incore inode pointer */
	xfs_fsblock_t	*firstblock,	/* first block allocated in xaction */
	xfs_extlen_t	total,		/* total blocks needed by transaction */
	int		*logflagsp,	/* inode logging flags */
	int		whichfork);	/* data or attr fork */

/*
 * Search the extents list for the inode, for the extent containing bno.
 * If bno lies in a hole, point to the next entry.  If bno lies past eof,
 * *eofp will be set, and *prevp will contain the last entry (null if none).
 * Else, *lastxp will be set to the index of the found
 * entry; *gotp will contain the entry.
 */
STATIC xfs_bmbt_rec_host_t *		/* pointer to found extent entry */
xfs_bmap_search_extents(
	xfs_inode_t	*ip,		/* incore inode pointer */
	xfs_fileoff_t	bno,		/* block number searched for */
	int		whichfork,	/* data or attr fork */
	int		*eofp,		/* out: end of file found */
	xfs_extnum_t	*lastxp,	/* out: last extent index */
	xfs_bmbt_irec_t	*gotp,		/* out: extent entry found */
	xfs_bmbt_irec_t	*prevp);	/* out: previous extent entry found */

/*
 * Check the last inode extent to determine whether this allocation will result
 * in blocks being allocated at the end of the file. When we allocate new data
 * blocks at the end of the file which do not start at the previous data block,
 * we will try to align the new blocks at stripe unit boundaries.
 */
STATIC int				/* error */
xfs_bmap_isaeof(
	xfs_inode_t	*ip,		/* incore inode pointer */
	xfs_fileoff_t   off,		/* file offset in fsblocks */
	int             whichfork,	/* data or attribute fork */
	char		*aeof);		/* return value */

#ifdef XFS_BMAP_TRACE
/*
 * Add bmap trace entry prior to a call to xfs_iext_remove.
 */
STATIC void
xfs_bmap_trace_delete(
	const char	*fname,		/* function name */
	char		*desc,		/* operation description */
	xfs_inode_t	*ip,		/* incore inode pointer */
	xfs_extnum_t	idx,		/* index of entry(entries) deleted */
	xfs_extnum_t	cnt,		/* count of entries deleted, 1 or 2 */
	int		whichfork);	/* data or attr fork */

/*
 * Add bmap trace entry prior to a call to xfs_iext_insert, or
 * reading in the extents list from the disk (in the btree).
 */
STATIC void
xfs_bmap_trace_insert(
	const char	*fname,		/* function name */
	char		*desc,		/* operation description */
	xfs_inode_t	*ip,		/* incore inode pointer */
	xfs_extnum_t	idx,		/* index of entry(entries) inserted */
	xfs_extnum_t	cnt,		/* count of entries inserted, 1 or 2 */
	xfs_bmbt_irec_t	*r1,		/* inserted record 1 */
	xfs_bmbt_irec_t	*r2,		/* inserted record 2 or null */
	int		whichfork);	/* data or attr fork */

/*
 * Add bmap trace entry after updating an extent record in place.
 */
STATIC void
xfs_bmap_trace_post_update(
	const char	*fname,		/* function name */
	char		*desc,		/* operation description */
	xfs_inode_t	*ip,		/* incore inode pointer */
	xfs_extnum_t	idx,		/* index of entry updated */
	int		whichfork);	/* data or attr fork */

/*
 * Add bmap trace entry prior to updating an extent record in place.
 */
STATIC void
xfs_bmap_trace_pre_update(
	const char	*fname,		/* function name */
	char		*desc,		/* operation description */
	xfs_inode_t	*ip,		/* incore inode pointer */
	xfs_extnum_t	idx,		/* index of entry to be updated */
	int		whichfork);	/* data or attr fork */

#define	XFS_BMAP_TRACE_DELETE(d,ip,i,c,w)	\
	xfs_bmap_trace_delete(__func__,d,ip,i,c,w)
#define	XFS_BMAP_TRACE_INSERT(d,ip,i,c,r1,r2,w)	\
	xfs_bmap_trace_insert(__func__,d,ip,i,c,r1,r2,w)
#define	XFS_BMAP_TRACE_POST_UPDATE(d,ip,i,w)	\
	xfs_bmap_trace_post_update(__func__,d,ip,i,w)
#define	XFS_BMAP_TRACE_PRE_UPDATE(d,ip,i,w)	\
	xfs_bmap_trace_pre_update(__func__,d,ip,i,w)
#else
#define	XFS_BMAP_TRACE_DELETE(d,ip,i,c,w)
#define	XFS_BMAP_TRACE_INSERT(d,ip,i,c,r1,r2,w)
#define	XFS_BMAP_TRACE_POST_UPDATE(d,ip,i,w)
#define	XFS_BMAP_TRACE_PRE_UPDATE(d,ip,i,w)
#endif	/* XFS_BMAP_TRACE */

/*
 * Compute the worst-case number of indirect blocks that will be used
 * for ip's delayed extent of length "len".
 */
STATIC xfs_filblks_t
xfs_bmap_worst_indlen(
	xfs_inode_t		*ip,	/* incore inode pointer */
	xfs_filblks_t		len);	/* delayed extent length */

#ifdef DEBUG
/*
 * Perform various validation checks on the values being returned
 * from xfs_bmapi().
 */
STATIC void
xfs_bmap_validate_ret(
	xfs_fileoff_t		bno,
	xfs_filblks_t		len,
	int			flags,
	xfs_bmbt_irec_t		*mval,
	int			nmap,
	int			ret_nmap);
#else
#define	xfs_bmap_validate_ret(bno,len,flags,mval,onmap,nmap)
#endif /* DEBUG */

#if defined(XFS_RW_TRACE)
STATIC void
xfs_bunmap_trace(
	xfs_inode_t		*ip,
	xfs_fileoff_t		bno,
	xfs_filblks_t		len,
	int			flags,
	inst_t			*ra);
#else
#define	xfs_bunmap_trace(ip, bno, len, flags, ra)
#endif	/* XFS_RW_TRACE */

STATIC int
xfs_bmap_count_tree(
	xfs_mount_t     *mp,
	xfs_trans_t     *tp,
	xfs_ifork_t	*ifp,
	xfs_fsblock_t   blockno,
	int             levelin,
	int		*count);

STATIC void
xfs_bmap_count_leaves(
	xfs_ifork_t		*ifp,
	xfs_extnum_t		idx,
	int			numrecs,
	int			*count);

STATIC void
xfs_bmap_disk_count_leaves(
	struct xfs_mount	*mp,
	struct xfs_btree_block	*block,
	int			numrecs,
	int			*count);

/*
 * Bmap internal routines.
 */

STATIC int				/* error */
xfs_bmbt_lookup_eq(
	struct xfs_btree_cur	*cur,
	xfs_fileoff_t		off,
	xfs_fsblock_t		bno,
	xfs_filblks_t		len,
	int			*stat)	/* success/failure */
{
	cur->bc_rec.b.br_startoff = off;
	cur->bc_rec.b.br_startblock = bno;
	cur->bc_rec.b.br_blockcount = len;
	return xfs_btree_lookup(cur, XFS_LOOKUP_EQ, stat);
}

STATIC int				/* error */
xfs_bmbt_lookup_ge(
	struct xfs_btree_cur	*cur,
	xfs_fileoff_t		off,
	xfs_fsblock_t		bno,
	xfs_filblks_t		len,
	int			*stat)	/* success/failure */
{
	cur->bc_rec.b.br_startoff = off;
	cur->bc_rec.b.br_startblock = bno;
	cur->bc_rec.b.br_blockcount = len;
	return xfs_btree_lookup(cur, XFS_LOOKUP_GE, stat);
}

/*
* Update the record referred to by cur to the value given
 * by [off, bno, len, state].
 * This either works (return 0) or gets an EFSCORRUPTED error.
 */
STATIC int
xfs_bmbt_update(
	struct xfs_btree_cur	*cur,
	xfs_fileoff_t		off,
	xfs_fsblock_t		bno,
	xfs_filblks_t		len,
	xfs_exntst_t		state)
{
	union xfs_btree_rec	rec;

	xfs_bmbt_disk_set_allf(&rec.bmbt, off, bno, len, state);
	return xfs_btree_update(cur, &rec);
}

/*
 * Called from xfs_bmap_add_attrfork to handle btree format files.
 */
STATIC int					/* error */
xfs_bmap_add_attrfork_btree(
	xfs_trans_t		*tp,		/* transaction pointer */
	xfs_inode_t		*ip,		/* incore inode pointer */
	xfs_fsblock_t		*firstblock,	/* first block allocated */
	xfs_bmap_free_t		*flist,		/* blocks to free at commit */
	int			*flags)		/* inode logging flags */
{
	xfs_btree_cur_t		*cur;		/* btree cursor */
	int			error;		/* error return value */
	xfs_mount_t		*mp;		/* file system mount struct */
	int			stat;		/* newroot status */

	mp = ip->i_mount;
	if (ip->i_df.if_broot_bytes <= XFS_IFORK_DSIZE(ip))
		*flags |= XFS_ILOG_DBROOT;
	else {
		cur = xfs_bmbt_init_cursor(mp, tp, ip, XFS_DATA_FORK);
		cur->bc_private.b.flist = flist;
		cur->bc_private.b.firstblock = *firstblock;
		if ((error = xfs_bmbt_lookup_ge(cur, 0, 0, 0, &stat)))
			goto error0;
		/* must be at least one entry */
		XFS_WANT_CORRUPTED_GOTO(stat == 1, error0);
		if ((error = xfs_btree_new_iroot(cur, flags, &stat)))
			goto error0;
		if (stat == 0) {
			xfs_btree_del_cursor(cur, XFS_BTREE_NOERROR);
			return XFS_ERROR(ENOSPC);
		}
		*firstblock = cur->bc_private.b.firstblock;
		cur->bc_private.b.allocated = 0;
		xfs_btree_del_cursor(cur, XFS_BTREE_NOERROR);
	}
	return 0;
error0:
	xfs_btree_del_cursor(cur, XFS_BTREE_ERROR);
	return error;
}

/*
 * Called from xfs_bmap_add_attrfork to handle extents format files.
 */
STATIC int					/* error */
xfs_bmap_add_attrfork_extents(
	xfs_trans_t		*tp,		/* transaction pointer */
	xfs_inode_t		*ip,		/* incore inode pointer */
	xfs_fsblock_t		*firstblock,	/* first block allocated */
	xfs_bmap_free_t		*flist,		/* blocks to free at commit */
	int			*flags)		/* inode logging flags */
{
	xfs_btree_cur_t		*cur;		/* bmap btree cursor */
	int			error;		/* error return value */

	if (ip->i_d.di_nextents * sizeof(xfs_bmbt_rec_t) <= XFS_IFORK_DSIZE(ip))
		return 0;
	cur = NULL;
	error = xfs_bmap_extents_to_btree(tp, ip, firstblock, flist, &cur, 0,
		flags, XFS_DATA_FORK);
	if (cur) {
		cur->bc_private.b.allocated = 0;
		xfs_btree_del_cursor(cur,
			error ? XFS_BTREE_ERROR : XFS_BTREE_NOERROR);
	}
	return error;
}

/*
 * Called from xfs_bmap_add_attrfork to handle local format files.
 */
STATIC int					/* error */
xfs_bmap_add_attrfork_local(
	xfs_trans_t		*tp,		/* transaction pointer */
	xfs_inode_t		*ip,		/* incore inode pointer */
	xfs_fsblock_t		*firstblock,	/* first block allocated */
	xfs_bmap_free_t		*flist,		/* blocks to free at commit */
	int			*flags)		/* inode logging flags */
{
	xfs_da_args_t		dargs;		/* args for dir/attr code */
	int			error;		/* error return value */
	xfs_mount_t		*mp;		/* mount structure pointer */

	if (ip->i_df.if_bytes <= XFS_IFORK_DSIZE(ip))
		return 0;
	if ((ip->i_d.di_mode & S_IFMT) == S_IFDIR) {
		mp = ip->i_mount;
		memset(&dargs, 0, sizeof(dargs));
		dargs.dp = ip;
		dargs.firstblock = firstblock;
		dargs.flist = flist;
		dargs.total = mp->m_dirblkfsbs;
		dargs.whichfork = XFS_DATA_FORK;
		dargs.trans = tp;
		error = xfs_dir2_sf_to_block(&dargs);
	} else
		error = xfs_bmap_local_to_extents(tp, ip, firstblock, 1, flags,
			XFS_DATA_FORK);
	return error;
}

/*
 * Called by xfs_bmapi to update file extent records and the btree
 * after allocating space (or doing a delayed allocation).
 */
STATIC int				/* error */
xfs_bmap_add_extent(
	xfs_inode_t		*ip,	/* incore inode pointer */
	xfs_extnum_t		idx,	/* extent number to update/insert */
	xfs_btree_cur_t		**curp,	/* if *curp is null, not a btree */
	xfs_bmbt_irec_t		*new,	/* new data to add to file extents */
	xfs_fsblock_t		*first,	/* pointer to firstblock variable */
	xfs_bmap_free_t		*flist,	/* list of extents to be freed */
	int			*logflagsp, /* inode logging flags */
	xfs_extdelta_t		*delta, /* Change made to incore extents */
	int			whichfork, /* data or attr fork */
	int			rsvd)	/* OK to use reserved data blocks */
{
	xfs_btree_cur_t		*cur;	/* btree cursor or null */
	xfs_filblks_t		da_new; /* new count del alloc blocks used */
	xfs_filblks_t		da_old; /* old count del alloc blocks used */
	int			error;	/* error return value */
	xfs_ifork_t		*ifp;	/* inode fork ptr */
	int			logflags; /* returned value */
	xfs_extnum_t		nextents; /* number of extents in file now */

	XFS_STATS_INC(xs_add_exlist);
	cur = *curp;
	ifp = XFS_IFORK_PTR(ip, whichfork);
	nextents = ifp->if_bytes / (uint)sizeof(xfs_bmbt_rec_t);
	ASSERT(idx <= nextents);
	da_old = da_new = 0;
	error = 0;
	/*
	 * This is the first extent added to a new/empty file.
	 * Special case this one, so other routines get to assume there are
	 * already extents in the list.
	 */
	if (nextents == 0) {
		XFS_BMAP_TRACE_INSERT("insert empty", ip, 0, 1, new, NULL,
			whichfork);
		xfs_iext_insert(ifp, 0, 1, new);
		ASSERT(cur == NULL);
		ifp->if_lastex = 0;
		if (!isnullstartblock(new->br_startblock)) {
			XFS_IFORK_NEXT_SET(ip, whichfork, 1);
			logflags = XFS_ILOG_CORE | xfs_ilog_fext(whichfork);
		} else
			logflags = 0;
		/* DELTA: single new extent */
		if (delta) {
			if (delta->xed_startoff > new->br_startoff)
				delta->xed_startoff = new->br_startoff;
			if (delta->xed_blockcount <
					new->br_startoff + new->br_blockcount)
				delta->xed_blockcount = new->br_startoff +
						new->br_blockcount;
		}
	}
	/*
	 * Any kind of new delayed allocation goes here.
	 */
	else if (isnullstartblock(new->br_startblock)) {
		if (cur)
			ASSERT((cur->bc_private.b.flags &
				XFS_BTCUR_BPRV_WASDEL) == 0);
		if ((error = xfs_bmap_add_extent_hole_delay(ip, idx, new,
				&logflags, delta, rsvd)))
			goto done;
	}
	/*
	 * Real allocation off the end of the file.
	 */
	else if (idx == nextents) {
		if (cur)
			ASSERT((cur->bc_private.b.flags &
				XFS_BTCUR_BPRV_WASDEL) == 0);
		if ((error = xfs_bmap_add_extent_hole_real(ip, idx, cur, new,
				&logflags, delta, whichfork)))
			goto done;
	} else {
		xfs_bmbt_irec_t	prev;	/* old extent at offset idx */

		/*
		 * Get the record referred to by idx.
		 */
		xfs_bmbt_get_all(xfs_iext_get_ext(ifp, idx), &prev);
		/*
		 * If it's a real allocation record, and the new allocation ends
		 * after the start of the referred to record, then we're filling
		 * in a delayed or unwritten allocation with a real one, or
		 * converting real back to unwritten.
		 */
		if (!isnullstartblock(new->br_startblock) &&
		    new->br_startoff + new->br_blockcount > prev.br_startoff) {
			if (prev.br_state != XFS_EXT_UNWRITTEN &&
			    isnullstartblock(prev.br_startblock)) {
				da_old = startblockval(prev.br_startblock);
				if (cur)
					ASSERT(cur->bc_private.b.flags &
						XFS_BTCUR_BPRV_WASDEL);
				if ((error = xfs_bmap_add_extent_delay_real(ip,
					idx, &cur, new, &da_new, first, flist,
					&logflags, delta, rsvd)))
					goto done;
			} else if (new->br_state == XFS_EXT_NORM) {
				ASSERT(new->br_state == XFS_EXT_NORM);
				if ((error = xfs_bmap_add_extent_unwritten_real(
					ip, idx, &cur, new, &logflags, delta)))
					goto done;
			} else {
				ASSERT(new->br_state == XFS_EXT_UNWRITTEN);
				if ((error = xfs_bmap_add_extent_unwritten_real(
					ip, idx, &cur, new, &logflags, delta)))
					goto done;
			}
			ASSERT(*curp == cur || *curp == NULL);
		}
		/*
		 * Otherwise we're filling in a hole with an allocation.
		 */
		else {
			if (cur)
				ASSERT((cur->bc_private.b.flags &
					XFS_BTCUR_BPRV_WASDEL) == 0);
			if ((error = xfs_bmap_add_extent_hole_real(ip, idx, cur,
					new, &logflags, delta, whichfork)))
				goto done;
		}
	}

	ASSERT(*curp == cur || *curp == NULL);
	/*
	 * Convert to a btree if necessary.
	 */
	if (XFS_IFORK_FORMAT(ip, whichfork) == XFS_DINODE_FMT_EXTENTS &&
	    XFS_IFORK_NEXTENTS(ip, whichfork) > ifp->if_ext_max) {
		int	tmp_logflags;	/* partial log flag return val */

		ASSERT(cur == NULL);
		error = xfs_bmap_extents_to_btree(ip->i_transp, ip, first,
			flist, &cur, da_old > 0, &tmp_logflags, whichfork);
		logflags |= tmp_logflags;
		if (error)
			goto done;
	}
	/*
	 * Adjust for changes in reserved delayed indirect blocks.
	 * Nothing to do for disk quotas here.
	 */
	if (da_old || da_new) {
		xfs_filblks_t	nblks;

		nblks = da_new;
		if (cur)
			nblks += cur->bc_private.b.allocated;
		ASSERT(nblks <= da_old);
		if (nblks < da_old)
			xfs_mod_incore_sb(ip->i_mount, XFS_SBS_FDBLOCKS,
				(int64_t)(da_old - nblks), rsvd);
	}
	/*
	 * Clear out the allocated field, done with it now in any case.
	 */
	if (cur) {
		cur->bc_private.b.allocated = 0;
		*curp = cur;
	}
done:
#ifdef DEBUG
	if (!error)
		xfs_bmap_check_leaf_extents(*curp, ip, whichfork);
#endif
	*logflagsp = logflags;
	return error;
}

/*
 * Called by xfs_bmap_add_extent to handle cases converting a delayed
 * allocation to a real allocation.
 */
STATIC int				/* error */
xfs_bmap_add_extent_delay_real(
	xfs_inode_t		*ip,	/* incore inode pointer */
	xfs_extnum_t		idx,	/* extent number to update/insert */
	xfs_btree_cur_t		**curp,	/* if *curp is null, not a btree */
	xfs_bmbt_irec_t		*new,	/* new data to add to file extents */
	xfs_filblks_t		*dnew,	/* new delayed-alloc indirect blocks */
	xfs_fsblock_t		*first,	/* pointer to firstblock variable */
	xfs_bmap_free_t		*flist,	/* list of extents to be freed */
	int			*logflagsp, /* inode logging flags */
	xfs_extdelta_t		*delta, /* Change made to incore extents */
	int			rsvd)	/* OK to use reserved data block allocation */
{
	xfs_btree_cur_t		*cur;	/* btree cursor */
	int			diff;	/* temp value */
	xfs_bmbt_rec_host_t	*ep;	/* extent entry for idx */
	int			error;	/* error return value */
	int			i;	/* temp state */
	xfs_ifork_t		*ifp;	/* inode fork pointer */
	xfs_fileoff_t		new_endoff;	/* end offset of new entry */
	xfs_bmbt_irec_t		r[3];	/* neighbor extent entries */
					/* left is 0, right is 1, prev is 2 */
	int			rval=0;	/* return value (logging flags) */
	int			state = 0;/* state bits, accessed thru macros */
	xfs_filblks_t		temp=0;	/* value for dnew calculations */
	xfs_filblks_t		temp2=0;/* value for dnew calculations */
	int			tmp_rval;	/* partial logging flags */
	enum {				/* bit number definitions for state */
		LEFT_CONTIG,	RIGHT_CONTIG,
		LEFT_FILLING,	RIGHT_FILLING,
		LEFT_DELAY,	RIGHT_DELAY,
		LEFT_VALID,	RIGHT_VALID
	};

#define	LEFT		r[0]
#define	RIGHT		r[1]
#define	PREV		r[2]
#define	MASK(b)		(1 << (b))
#define	MASK2(a,b)	(MASK(a) | MASK(b))
#define	MASK3(a,b,c)	(MASK2(a,b) | MASK(c))
#define	MASK4(a,b,c,d)	(MASK3(a,b,c) | MASK(d))
#define	STATE_SET(b,v)	((v) ? (state |= MASK(b)) : (state &= ~MASK(b)))
#define	STATE_TEST(b)	(state & MASK(b))
#define	STATE_SET_TEST(b,v)	((v) ? ((state |= MASK(b)), 1) : \
				       ((state &= ~MASK(b)), 0))
#define	SWITCH_STATE		\
	(state & MASK4(LEFT_FILLING, RIGHT_FILLING, LEFT_CONTIG, RIGHT_CONTIG))

	/*
	 * Set up a bunch of variables to make the tests simpler.
	 */
	cur = *curp;
	ifp = XFS_IFORK_PTR(ip, XFS_DATA_FORK);
	ep = xfs_iext_get_ext(ifp, idx);
	xfs_bmbt_get_all(ep, &PREV);
	new_endoff = new->br_startoff + new->br_blockcount;
	ASSERT(PREV.br_startoff <= new->br_startoff);
	ASSERT(PREV.br_startoff + PREV.br_blockcount >= new_endoff);
	/*
	 * Set flags determining what part of the previous delayed allocation
	 * extent is being replaced by a real allocation.
	 */
	STATE_SET(LEFT_FILLING, PREV.br_startoff == new->br_startoff);
	STATE_SET(RIGHT_FILLING,
		PREV.br_startoff + PREV.br_blockcount == new_endoff);
	/*
	 * Check and set flags if this segment has a left neighbor.
	 * Don't set contiguous if the combined extent would be too large.
	 */
	if (STATE_SET_TEST(LEFT_VALID, idx > 0)) {
		xfs_bmbt_get_all(xfs_iext_get_ext(ifp, idx - 1), &LEFT);
		STATE_SET(LEFT_DELAY, isnullstartblock(LEFT.br_startblock));
	}
	STATE_SET(LEFT_CONTIG,
		STATE_TEST(LEFT_VALID) && !STATE_TEST(LEFT_DELAY) &&
		LEFT.br_startoff + LEFT.br_blockcount == new->br_startoff &&
		LEFT.br_startblock + LEFT.br_blockcount == new->br_startblock &&
		LEFT.br_state == new->br_state &&
		LEFT.br_blockcount + new->br_blockcount <= MAXEXTLEN);
	/*
	 * Check and set flags if this segment has a right neighbor.
	 * Don't set contiguous if the combined extent would be too large.
	 * Also check for all-three-contiguous being too large.
	 */
	if (STATE_SET_TEST(RIGHT_VALID,
			idx <
			ip->i_df.if_bytes / (uint)sizeof(xfs_bmbt_rec_t) - 1)) {
		xfs_bmbt_get_all(xfs_iext_get_ext(ifp, idx + 1), &RIGHT);
		STATE_SET(RIGHT_DELAY, isnullstartblock(RIGHT.br_startblock));
	}
	STATE_SET(RIGHT_CONTIG,
		STATE_TEST(RIGHT_VALID) && !STATE_TEST(RIGHT_DELAY) &&
		new_endoff == RIGHT.br_startoff &&
		new->br_startblock + new->br_blockcount ==
		    RIGHT.br_startblock &&
		new->br_state == RIGHT.br_state &&
		new->br_blockcount + RIGHT.br_blockcount <= MAXEXTLEN &&
		((state & MASK3(LEFT_CONTIG, LEFT_FILLING, RIGHT_FILLING)) !=
		  MASK3(LEFT_CONTIG, LEFT_FILLING, RIGHT_FILLING) ||
		 LEFT.br_blockcount + new->br_blockcount + RIGHT.br_blockcount
		     <= MAXEXTLEN));
	error = 0;
	/*
	 * Switch out based on the FILLING and CONTIG state bits.
	 */
	switch (SWITCH_STATE) {

	case MASK4(LEFT_FILLING, RIGHT_FILLING, LEFT_CONTIG, RIGHT_CONTIG):
		/*
		 * Filling in all of a previously delayed allocation extent.
		 * The left and right neighbors are both contiguous with new.
		 */
		XFS_BMAP_TRACE_PRE_UPDATE("LF|RF|LC|RC", ip, idx - 1,
			XFS_DATA_FORK);
		xfs_bmbt_set_blockcount(xfs_iext_get_ext(ifp, idx - 1),
			LEFT.br_blockcount + PREV.br_blockcount +
			RIGHT.br_blockcount);
		XFS_BMAP_TRACE_POST_UPDATE("LF|RF|LC|RC", ip, idx - 1,
			XFS_DATA_FORK);
		XFS_BMAP_TRACE_DELETE("LF|RF|LC|RC", ip, idx, 2, XFS_DATA_FORK);
		xfs_iext_remove(ifp, idx, 2);
		ip->i_df.if_lastex = idx - 1;
		ip->i_d.di_nextents--;
		if (cur == NULL)
			rval = XFS_ILOG_CORE | XFS_ILOG_DEXT;
		else {
			rval = XFS_ILOG_CORE;
			if ((error = xfs_bmbt_lookup_eq(cur, RIGHT.br_startoff,
					RIGHT.br_startblock,
					RIGHT.br_blockcount, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_btree_delete(cur, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_btree_decrement(cur, 0, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_bmbt_update(cur, LEFT.br_startoff,
					LEFT.br_startblock,
					LEFT.br_blockcount +
					PREV.br_blockcount +
					RIGHT.br_blockcount, LEFT.br_state)))
				goto done;
		}
		*dnew = 0;
		/* DELTA: Three in-core extents are replaced by one. */
		temp = LEFT.br_startoff;
		temp2 = LEFT.br_blockcount +
			PREV.br_blockcount +
			RIGHT.br_blockcount;
		break;

	case MASK3(LEFT_FILLING, RIGHT_FILLING, LEFT_CONTIG):
		/*
		 * Filling in all of a previously delayed allocation extent.
		 * The left neighbor is contiguous, the right is not.
		 */
		XFS_BMAP_TRACE_PRE_UPDATE("LF|RF|LC", ip, idx - 1,
			XFS_DATA_FORK);
		xfs_bmbt_set_blockcount(xfs_iext_get_ext(ifp, idx - 1),
			LEFT.br_blockcount + PREV.br_blockcount);
		XFS_BMAP_TRACE_POST_UPDATE("LF|RF|LC", ip, idx - 1,
			XFS_DATA_FORK);
		ip->i_df.if_lastex = idx - 1;
		XFS_BMAP_TRACE_DELETE("LF|RF|LC", ip, idx, 1, XFS_DATA_FORK);
		xfs_iext_remove(ifp, idx, 1);
		if (cur == NULL)
			rval = XFS_ILOG_DEXT;
		else {
			rval = 0;
			if ((error = xfs_bmbt_lookup_eq(cur, LEFT.br_startoff,
					LEFT.br_startblock, LEFT.br_blockcount,
					&i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_bmbt_update(cur, LEFT.br_startoff,
					LEFT.br_startblock,
					LEFT.br_blockcount +
					PREV.br_blockcount, LEFT.br_state)))
				goto done;
		}
		*dnew = 0;
		/* DELTA: Two in-core extents are replaced by one. */
		temp = LEFT.br_startoff;
		temp2 = LEFT.br_blockcount +
			PREV.br_blockcount;
		break;

	case MASK3(LEFT_FILLING, RIGHT_FILLING, RIGHT_CONTIG):
		/*
		 * Filling in all of a previously delayed allocation extent.
		 * The right neighbor is contiguous, the left is not.
		 */
		XFS_BMAP_TRACE_PRE_UPDATE("LF|RF|RC", ip, idx, XFS_DATA_FORK);
		xfs_bmbt_set_startblock(ep, new->br_startblock);
		xfs_bmbt_set_blockcount(ep,
			PREV.br_blockcount + RIGHT.br_blockcount);
		XFS_BMAP_TRACE_POST_UPDATE("LF|RF|RC", ip, idx, XFS_DATA_FORK);
		ip->i_df.if_lastex = idx;
		XFS_BMAP_TRACE_DELETE("LF|RF|RC", ip, idx + 1, 1, XFS_DATA_FORK);
		xfs_iext_remove(ifp, idx + 1, 1);
		if (cur == NULL)
			rval = XFS_ILOG_DEXT;
		else {
			rval = 0;
			if ((error = xfs_bmbt_lookup_eq(cur, RIGHT.br_startoff,
					RIGHT.br_startblock,
					RIGHT.br_blockcount, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_bmbt_update(cur, PREV.br_startoff,
					new->br_startblock,
					PREV.br_blockcount +
					RIGHT.br_blockcount, PREV.br_state)))
				goto done;
		}
		*dnew = 0;
		/* DELTA: Two in-core extents are replaced by one. */
		temp = PREV.br_startoff;
		temp2 = PREV.br_blockcount +
			RIGHT.br_blockcount;
		break;

	case MASK2(LEFT_FILLING, RIGHT_FILLING):
		/*
		 * Filling in all of a previously delayed allocation extent.
		 * Neither the left nor right neighbors are contiguous with
		 * the new one.
		 */
		XFS_BMAP_TRACE_PRE_UPDATE("LF|RF", ip, idx, XFS_DATA_FORK);
		xfs_bmbt_set_startblock(ep, new->br_startblock);
		XFS_BMAP_TRACE_POST_UPDATE("LF|RF", ip, idx, XFS_DATA_FORK);
		ip->i_df.if_lastex = idx;
		ip->i_d.di_nextents++;
		if (cur == NULL)
			rval = XFS_ILOG_CORE | XFS_ILOG_DEXT;
		else {
			rval = XFS_ILOG_CORE;
			if ((error = xfs_bmbt_lookup_eq(cur, new->br_startoff,
					new->br_startblock, new->br_blockcount,
					&i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 0, done);
			cur->bc_rec.b.br_state = XFS_EXT_NORM;
			if ((error = xfs_btree_insert(cur, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
		}
		*dnew = 0;
		/* DELTA: The in-core extent described by new changed type. */
		temp = new->br_startoff;
		temp2 = new->br_blockcount;
		break;

	case MASK2(LEFT_FILLING, LEFT_CONTIG):
		/*
		 * Filling in the first part of a previous delayed allocation.
		 * The left neighbor is contiguous.
		 */
		XFS_BMAP_TRACE_PRE_UPDATE("LF|LC", ip, idx - 1, XFS_DATA_FORK);
		xfs_bmbt_set_blockcount(xfs_iext_get_ext(ifp, idx - 1),
			LEFT.br_blockcount + new->br_blockcount);
		xfs_bmbt_set_startoff(ep,
			PREV.br_startoff + new->br_blockcount);
		XFS_BMAP_TRACE_POST_UPDATE("LF|LC", ip, idx - 1, XFS_DATA_FORK);
		temp = PREV.br_blockcount - new->br_blockcount;
		XFS_BMAP_TRACE_PRE_UPDATE("LF|LC", ip, idx, XFS_DATA_FORK);
		xfs_bmbt_set_blockcount(ep, temp);
		ip->i_df.if_lastex = idx - 1;
		if (cur == NULL)
			rval = XFS_ILOG_DEXT;
		else {
			rval = 0;
			if ((error = xfs_bmbt_lookup_eq(cur, LEFT.br_startoff,
					LEFT.br_startblock, LEFT.br_blockcount,
					&i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_bmbt_update(cur, LEFT.br_startoff,
					LEFT.br_startblock,
					LEFT.br_blockcount +
					new->br_blockcount,
					LEFT.br_state)))
				goto done;
		}
		temp = XFS_FILBLKS_MIN(xfs_bmap_worst_indlen(ip, temp),
			startblockval(PREV.br_startblock));
		xfs_bmbt_set_startblock(ep, nullstartblock((int)temp));
		XFS_BMAP_TRACE_POST_UPDATE("LF|LC", ip, idx, XFS_DATA_FORK);
		*dnew = temp;
		/* DELTA: The boundary between two in-core extents moved. */
		temp = LEFT.br_startoff;
		temp2 = LEFT.br_blockcount +
			PREV.br_blockcount;
		break;

	case MASK(LEFT_FILLING):
		/*
		 * Filling in the first part of a previous delayed allocation.
		 * The left neighbor is not contiguous.
		 */
		XFS_BMAP_TRACE_PRE_UPDATE("LF", ip, idx, XFS_DATA_FORK);
		xfs_bmbt_set_startoff(ep, new_endoff);
		temp = PREV.br_blockcount - new->br_blockcount;
		xfs_bmbt_set_blockcount(ep, temp);
		XFS_BMAP_TRACE_INSERT("LF", ip, idx, 1, new, NULL,
			XFS_DATA_FORK);
		xfs_iext_insert(ifp, idx, 1, new);
		ip->i_df.if_lastex = idx;
		ip->i_d.di_nextents++;
		if (cur == NULL)
			rval = XFS_ILOG_CORE | XFS_ILOG_DEXT;
		else {
			rval = XFS_ILOG_CORE;
			if ((error = xfs_bmbt_lookup_eq(cur, new->br_startoff,
					new->br_startblock, new->br_blockcount,
					&i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 0, done);
			cur->bc_rec.b.br_state = XFS_EXT_NORM;
			if ((error = xfs_btree_insert(cur, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
		}
		if (ip->i_d.di_format == XFS_DINODE_FMT_EXTENTS &&
		    ip->i_d.di_nextents > ip->i_df.if_ext_max) {
			error = xfs_bmap_extents_to_btree(ip->i_transp, ip,
					first, flist, &cur, 1, &tmp_rval,
					XFS_DATA_FORK);
			rval |= tmp_rval;
			if (error)
				goto done;
		}
		temp = XFS_FILBLKS_MIN(xfs_bmap_worst_indlen(ip, temp),
			startblockval(PREV.br_startblock) -
			(cur ? cur->bc_private.b.allocated : 0));
		ep = xfs_iext_get_ext(ifp, idx + 1);
		xfs_bmbt_set_startblock(ep, nullstartblock((int)temp));
		XFS_BMAP_TRACE_POST_UPDATE("LF", ip, idx + 1, XFS_DATA_FORK);
		*dnew = temp;
		/* DELTA: One in-core extent is split in two. */
		temp = PREV.br_startoff;
		temp2 = PREV.br_blockcount;
		break;

	case MASK2(RIGHT_FILLING, RIGHT_CONTIG):
		/*
		 * Filling in the last part of a previous delayed allocation.
		 * The right neighbor is contiguous with the new allocation.
		 */
		temp = PREV.br_blockcount - new->br_blockcount;
		XFS_BMAP_TRACE_PRE_UPDATE("RF|RC", ip, idx, XFS_DATA_FORK);
		XFS_BMAP_TRACE_PRE_UPDATE("RF|RC", ip, idx + 1, XFS_DATA_FORK);
		xfs_bmbt_set_blockcount(ep, temp);
		xfs_bmbt_set_allf(xfs_iext_get_ext(ifp, idx + 1),
			new->br_startoff, new->br_startblock,
			new->br_blockcount + RIGHT.br_blockcount,
			RIGHT.br_state);
		XFS_BMAP_TRACE_POST_UPDATE("RF|RC", ip, idx + 1, XFS_DATA_FORK);
		ip->i_df.if_lastex = idx + 1;
		if (cur == NULL)
			rval = XFS_ILOG_DEXT;
		else {
			rval = 0;
			if ((error = xfs_bmbt_lookup_eq(cur, RIGHT.br_startoff,
					RIGHT.br_startblock,
					RIGHT.br_blockcount, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_bmbt_update(cur, new->br_startoff,
					new->br_startblock,
					new->br_blockcount +
					RIGHT.br_blockcount,
					RIGHT.br_state)))
				goto done;
		}
		temp = XFS_FILBLKS_MIN(xfs_bmap_worst_indlen(ip, temp),
			startblockval(PREV.br_startblock));
		xfs_bmbt_set_startblock(ep, nullstartblock((int)temp));
		XFS_BMAP_TRACE_POST_UPDATE("RF|RC", ip, idx, XFS_DATA_FORK);
		*dnew = temp;
		/* DELTA: The boundary between two in-core extents moved. */
		temp = PREV.br_startoff;
		temp2 = PREV.br_blockcount +
			RIGHT.br_blockcount;
		break;

	case MASK(RIGHT_FILLING):
		/*
		 * Filling in the last part of a previous delayed allocation.
		 * The right neighbor is not contiguous.
		 */
		temp = PREV.br_blockcount - new->br_blockcount;
		XFS_BMAP_TRACE_PRE_UPDATE("RF", ip, idx, XFS_DATA_FORK);
		xfs_bmbt_set_blockcount(ep, temp);
		XFS_BMAP_TRACE_INSERT("RF", ip, idx + 1, 1, new, NULL,
			XFS_DATA_FORK);
		xfs_iext_insert(ifp, idx + 1, 1, new);
		ip->i_df.if_lastex = idx + 1;
		ip->i_d.di_nextents++;
		if (cur == NULL)
			rval = XFS_ILOG_CORE | XFS_ILOG_DEXT;
		else {
			rval = XFS_ILOG_CORE;
			if ((error = xfs_bmbt_lookup_eq(cur, new->br_startoff,
					new->br_startblock, new->br_blockcount,
					&i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 0, done);
			cur->bc_rec.b.br_state = XFS_EXT_NORM;
			if ((error = xfs_btree_insert(cur, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
		}
		if (ip->i_d.di_format == XFS_DINODE_FMT_EXTENTS &&
		    ip->i_d.di_nextents > ip->i_df.if_ext_max) {
			error = xfs_bmap_extents_to_btree(ip->i_transp, ip,
				first, flist, &cur, 1, &tmp_rval,
				XFS_DATA_FORK);
			rval |= tmp_rval;
			if (error)
				goto done;
		}
		temp = XFS_FILBLKS_MIN(xfs_bmap_worst_indlen(ip, temp),
			startblockval(PREV.br_startblock) -
			(cur ? cur->bc_private.b.allocated : 0));
		ep = xfs_iext_get_ext(ifp, idx);
		xfs_bmbt_set_startblock(ep, nullstartblock((int)temp));
		XFS_BMAP_TRACE_POST_UPDATE("RF", ip, idx, XFS_DATA_FORK);
		*dnew = temp;
		/* DELTA: One in-core extent is split in two. */
		temp = PREV.br_startoff;
		temp2 = PREV.br_blockcount;
		break;

	case 0:
		/*
		 * Filling in the middle part of a previous delayed allocation.
		 * Contiguity is impossible here.
		 * This case is avoided almost all the time.
		 */
		temp = new->br_startoff - PREV.br_startoff;
		XFS_BMAP_TRACE_PRE_UPDATE("0", ip, idx, XFS_DATA_FORK);
		xfs_bmbt_set_blockcount(ep, temp);
		r[0] = *new;
		r[1].br_state = PREV.br_state;
		r[1].br_startblock = 0;
		r[1].br_startoff = new_endoff;
		temp2 = PREV.br_startoff + PREV.br_blockcount - new_endoff;
		r[1].br_blockcount = temp2;
		XFS_BMAP_TRACE_INSERT("0", ip, idx + 1, 2, &r[0], &r[1],
			XFS_DATA_FORK);
		xfs_iext_insert(ifp, idx + 1, 2, &r[0]);
		ip->i_df.if_lastex = idx + 1;
		ip->i_d.di_nextents++;
		if (cur == NULL)
			rval = XFS_ILOG_CORE | XFS_ILOG_DEXT;
		else {
			rval = XFS_ILOG_CORE;
			if ((error = xfs_bmbt_lookup_eq(cur, new->br_startoff,
					new->br_startblock, new->br_blockcount,
					&i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 0, done);
			cur->bc_rec.b.br_state = XFS_EXT_NORM;
			if ((error = xfs_btree_insert(cur, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
		}
		if (ip->i_d.di_format == XFS_DINODE_FMT_EXTENTS &&
		    ip->i_d.di_nextents > ip->i_df.if_ext_max) {
			error = xfs_bmap_extents_to_btree(ip->i_transp, ip,
					first, flist, &cur, 1, &tmp_rval,
					XFS_DATA_FORK);
			rval |= tmp_rval;
			if (error)
				goto done;
		}
		temp = xfs_bmap_worst_indlen(ip, temp);
		temp2 = xfs_bmap_worst_indlen(ip, temp2);
		diff = (int)(temp + temp2 - startblockval(PREV.br_startblock) -
			(cur ? cur->bc_private.b.allocated : 0));
		if (diff > 0 &&
		    xfs_mod_incore_sb(ip->i_mount, XFS_SBS_FDBLOCKS, -((int64_t)diff), rsvd)) {
			/*
			 * Ick gross gag me with a spoon.
			 */
			ASSERT(0);	/* want to see if this ever happens! */
			while (diff > 0) {
				if (temp) {
					temp--;
					diff--;
					if (!diff ||
					    !xfs_mod_incore_sb(ip->i_mount,
						    XFS_SBS_FDBLOCKS, -((int64_t)diff), rsvd))
						break;
				}
				if (temp2) {
					temp2--;
					diff--;
					if (!diff ||
					    !xfs_mod_incore_sb(ip->i_mount,
						    XFS_SBS_FDBLOCKS, -((int64_t)diff), rsvd))
						break;
				}
			}
		}
		ep = xfs_iext_get_ext(ifp, idx);
		xfs_bmbt_set_startblock(ep, nullstartblock((int)temp));
		XFS_BMAP_TRACE_POST_UPDATE("0", ip, idx, XFS_DATA_FORK);
		XFS_BMAP_TRACE_PRE_UPDATE("0", ip, idx + 2, XFS_DATA_FORK);
		xfs_bmbt_set_startblock(xfs_iext_get_ext(ifp, idx + 2),
			nullstartblock((int)temp2));
		XFS_BMAP_TRACE_POST_UPDATE("0", ip, idx + 2, XFS_DATA_FORK);
		*dnew = temp + temp2;
		/* DELTA: One in-core extent is split in three. */
		temp = PREV.br_startoff;
		temp2 = PREV.br_blockcount;
		break;

	case MASK3(LEFT_FILLING, LEFT_CONTIG, RIGHT_CONTIG):
	case MASK3(RIGHT_FILLING, LEFT_CONTIG, RIGHT_CONTIG):
	case MASK2(LEFT_FILLING, RIGHT_CONTIG):
	case MASK2(RIGHT_FILLING, LEFT_CONTIG):
	case MASK2(LEFT_CONTIG, RIGHT_CONTIG):
	case MASK(LEFT_CONTIG):
	case MASK(RIGHT_CONTIG):
		/*
		 * These cases are all impossible.
		 */
		ASSERT(0);
	}
	*curp = cur;
	if (delta) {
		temp2 += temp;
		if (delta->xed_startoff > temp)
			delta->xed_startoff = temp;
		if (delta->xed_blockcount < temp2)
			delta->xed_blockcount = temp2;
	}
done:
	*logflagsp = rval;
	return error;
#undef	LEFT
#undef	RIGHT
#undef	PREV
#undef	MASK
#undef	MASK2
#undef	MASK3
#undef	MASK4
#undef	STATE_SET
#undef	STATE_TEST
#undef	STATE_SET_TEST
#undef	SWITCH_STATE
}

/*
 * Called by xfs_bmap_add_extent to handle cases converting an unwritten
 * allocation to a real allocation or vice versa.
 */
STATIC int				/* error */
xfs_bmap_add_extent_unwritten_real(
	xfs_inode_t		*ip,	/* incore inode pointer */
	xfs_extnum_t		idx,	/* extent number to update/insert */
	xfs_btree_cur_t		**curp,	/* if *curp is null, not a btree */
	xfs_bmbt_irec_t		*new,	/* new data to add to file extents */
	int			*logflagsp, /* inode logging flags */
	xfs_extdelta_t		*delta) /* Change made to incore extents */
{
	xfs_btree_cur_t		*cur;	/* btree cursor */
	xfs_bmbt_rec_host_t	*ep;	/* extent entry for idx */
	int			error;	/* error return value */
	int			i;	/* temp state */
	xfs_ifork_t		*ifp;	/* inode fork pointer */
	xfs_fileoff_t		new_endoff;	/* end offset of new entry */
	xfs_exntst_t		newext;	/* new extent state */
	xfs_exntst_t		oldext;	/* old extent state */
	xfs_bmbt_irec_t		r[3];	/* neighbor extent entries */
					/* left is 0, right is 1, prev is 2 */
	int			rval=0;	/* return value (logging flags) */
	int			state = 0;/* state bits, accessed thru macros */
	xfs_filblks_t		temp=0;
	xfs_filblks_t		temp2=0;
	enum {				/* bit number definitions for state */
		LEFT_CONTIG,	RIGHT_CONTIG,
		LEFT_FILLING,	RIGHT_FILLING,
		LEFT_DELAY,	RIGHT_DELAY,
		LEFT_VALID,	RIGHT_VALID
	};

#define	LEFT		r[0]
#define	RIGHT		r[1]
#define	PREV		r[2]
#define	MASK(b)		(1 << (b))
#define	MASK2(a,b)	(MASK(a) | MASK(b))
#define	MASK3(a,b,c)	(MASK2(a,b) | MASK(c))
#define	MASK4(a,b,c,d)	(MASK3(a,b,c) | MASK(d))
#define	STATE_SET(b,v)	((v) ? (state |= MASK(b)) : (state &= ~MASK(b)))
#define	STATE_TEST(b)	(state & MASK(b))
#define	STATE_SET_TEST(b,v)	((v) ? ((state |= MASK(b)), 1) : \
				       ((state &= ~MASK(b)), 0))
#define	SWITCH_STATE		\
	(state & MASK4(LEFT_FILLING, RIGHT_FILLING, LEFT_CONTIG, RIGHT_CONTIG))

	/*
	 * Set up a bunch of variables to make the tests simpler.
	 */
	error = 0;
	cur = *curp;
	ifp = XFS_IFORK_PTR(ip, XFS_DATA_FORK);
	ep = xfs_iext_get_ext(ifp, idx);
	xfs_bmbt_get_all(ep, &PREV);
	newext = new->br_state;
	oldext = (newext == XFS_EXT_UNWRITTEN) ?
		XFS_EXT_NORM : XFS_EXT_UNWRITTEN;
	ASSERT(PREV.br_state == oldext);
	new_endoff = new->br_startoff + new->br_blockcount;
	ASSERT(PREV.br_startoff <= new->br_startoff);
	ASSERT(PREV.br_startoff + PREV.br_blockcount >= new_endoff);
	/*
	 * Set flags determining what part of the previous oldext allocation
	 * extent is being replaced by a newext allocation.
	 */
	STATE_SET(LEFT_FILLING, PREV.br_startoff == new->br_startoff);
	STATE_SET(RIGHT_FILLING,
		PREV.br_startoff + PREV.br_blockcount == new_endoff);
	/*
	 * Check and set flags if this segment has a left neighbor.
	 * Don't set contiguous if the combined extent would be too large.
	 */
	if (STATE_SET_TEST(LEFT_VALID, idx > 0)) {
		xfs_bmbt_get_all(xfs_iext_get_ext(ifp, idx - 1), &LEFT);
		STATE_SET(LEFT_DELAY, isnullstartblock(LEFT.br_startblock));
	}
	STATE_SET(LEFT_CONTIG,
		STATE_TEST(LEFT_VALID) && !STATE_TEST(LEFT_DELAY) &&
		LEFT.br_startoff + LEFT.br_blockcount == new->br_startoff &&
		LEFT.br_startblock + LEFT.br_blockcount == new->br_startblock &&
		LEFT.br_state == newext &&
		LEFT.br_blockcount + new->br_blockcount <= MAXEXTLEN);
	/*
	 * Check and set flags if this segment has a right neighbor.
	 * Don't set contiguous if the combined extent would be too large.
	 * Also check for all-three-contiguous being too large.
	 */
	if (STATE_SET_TEST(RIGHT_VALID,
			idx <
			ip->i_df.if_bytes / (uint)sizeof(xfs_bmbt_rec_t) - 1)) {
		xfs_bmbt_get_all(xfs_iext_get_ext(ifp, idx + 1), &RIGHT);
		STATE_SET(RIGHT_DELAY, isnullstartblock(RIGHT.br_startblock));
	}
	STATE_SET(RIGHT_CONTIG,
		STATE_TEST(RIGHT_VALID) && !STATE_TEST(RIGHT_DELAY) &&
		new_endoff == RIGHT.br_startoff &&
		new->br_startblock + new->br_blockcount ==
		    RIGHT.br_startblock &&
		newext == RIGHT.br_state &&
		new->br_blockcount + RIGHT.br_blockcount <= MAXEXTLEN &&
		((state & MASK3(LEFT_CONTIG, LEFT_FILLING, RIGHT_FILLING)) !=
		  MASK3(LEFT_CONTIG, LEFT_FILLING, RIGHT_FILLING) ||
		 LEFT.br_blockcount + new->br_blockcount + RIGHT.br_blockcount
		     <= MAXEXTLEN));
	/*
	 * Switch out based on the FILLING and CONTIG state bits.
	 */
	switch (SWITCH_STATE) {

	case MASK4(LEFT_FILLING, RIGHT_FILLING, LEFT_CONTIG, RIGHT_CONTIG):
		/*
		 * Setting all of a previous oldext extent to newext.
		 * The left and right neighbors are both contiguous with new.
		 */
		XFS_BMAP_TRACE_PRE_UPDATE("LF|RF|LC|RC", ip, idx - 1,
			XFS_DATA_FORK);
		xfs_bmbt_set_blockcount(xfs_iext_get_ext(ifp, idx - 1),
			LEFT.br_blockcount + PREV.br_blockcount +
			RIGHT.br_blockcount);
		XFS_BMAP_TRACE_POST_UPDATE("LF|RF|LC|RC", ip, idx - 1,
			XFS_DATA_FORK);
		XFS_BMAP_TRACE_DELETE("LF|RF|LC|RC", ip, idx, 2, XFS_DATA_FORK);
		xfs_iext_remove(ifp, idx, 2);
		ip->i_df.if_lastex = idx - 1;
		ip->i_d.di_nextents -= 2;
		if (cur == NULL)
			rval = XFS_ILOG_CORE | XFS_ILOG_DEXT;
		else {
			rval = XFS_ILOG_CORE;
			if ((error = xfs_bmbt_lookup_eq(cur, RIGHT.br_startoff,
					RIGHT.br_startblock,
					RIGHT.br_blockcount, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_btree_delete(cur, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_btree_decrement(cur, 0, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_btree_delete(cur, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_btree_decrement(cur, 0, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_bmbt_update(cur, LEFT.br_startoff,
				LEFT.br_startblock,
				LEFT.br_blockcount + PREV.br_blockcount +
				RIGHT.br_blockcount, LEFT.br_state)))
				goto done;
		}
		/* DELTA: Three in-core extents are replaced by one. */
		temp = LEFT.br_startoff;
		temp2 = LEFT.br_blockcount +
			PREV.br_blockcount +
			RIGHT.br_blockcount;
		break;

	case MASK3(LEFT_FILLING, RIGHT_FILLING, LEFT_CONTIG):
		/*
		 * Setting all of a previous oldext extent to newext.
		 * The left neighbor is contiguous, the right is not.
		 */
		XFS_BMAP_TRACE_PRE_UPDATE("LF|RF|LC", ip, idx - 1,
			XFS_DATA_FORK);
		xfs_bmbt_set_blockcount(xfs_iext_get_ext(ifp, idx - 1),
			LEFT.br_blockcount + PREV.br_blockcount);
		XFS_BMAP_TRACE_POST_UPDATE("LF|RF|LC", ip, idx - 1,
			XFS_DATA_FORK);
		ip->i_df.if_lastex = idx - 1;
		XFS_BMAP_TRACE_DELETE("LF|RF|LC", ip, idx, 1, XFS_DATA_FORK);
		xfs_iext_remove(ifp, idx, 1);
		ip->i_d.di_nextents--;
		if (cur == NULL)
			rval = XFS_ILOG_CORE | XFS_ILOG_DEXT;
		else {
			rval = XFS_ILOG_CORE;
			if ((error = xfs_bmbt_lookup_eq(cur, PREV.br_startoff,
					PREV.br_startblock, PREV.br_blockcount,
					&i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_btree_delete(cur, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_btree_decrement(cur, 0, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_bmbt_update(cur, LEFT.br_startoff,
				LEFT.br_startblock,
				LEFT.br_blockcount + PREV.br_blockcount,
				LEFT.br_state)))
				goto done;
		}
		/* DELTA: Two in-core extents are replaced by one. */
		temp = LEFT.br_startoff;
		temp2 = LEFT.br_blockcount +
			PREV.br_blockcount;
		break;

	case MASK3(LEFT_FILLING, RIGHT_FILLING, RIGHT_CONTIG):
		/*
		 * Setting all of a previous oldext extent to newext.
		 * The right neighbor is contiguous, the left is not.
		 */
		XFS_BMAP_TRACE_PRE_UPDATE("LF|RF|RC", ip, idx,
			XFS_DATA_FORK);
		xfs_bmbt_set_blockcount(ep,
			PREV.br_blockcount + RIGHT.br_blockcount);
		xfs_bmbt_set_state(ep, newext);
		XFS_BMAP_TRACE_POST_UPDATE("LF|RF|RC", ip, idx,
			XFS_DATA_FORK);
		ip->i_df.if_lastex = idx;
		XFS_BMAP_TRACE_DELETE("LF|RF|RC", ip, idx + 1, 1, XFS_DATA_FORK);
		xfs_iext_remove(ifp, idx + 1, 1);
		ip->i_d.di_nextents--;
		if (cur == NULL)
			rval = XFS_ILOG_CORE | XFS_ILOG_DEXT;
		else {
			rval = XFS_ILOG_CORE;
			if ((error = xfs_bmbt_lookup_eq(cur, RIGHT.br_startoff,
					RIGHT.br_startblock,
					RIGHT.br_blockcount, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_btree_delete(cur, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_btree_decrement(cur, 0, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_bmbt_update(cur, new->br_startoff,
				new->br_startblock,
				new->br_blockcount + RIGHT.br_blockcount,
				newext)))
				goto done;
		}
		/* DELTA: Two in-core extents are replaced by one. */
		temp = PREV.br_startoff;
		temp2 = PREV.br_blockcount +
			RIGHT.br_blockcount;
		break;

	case MASK2(LEFT_FILLING, RIGHT_FILLING):
		/*
		 * Setting all of a previous oldext extent to newext.
		 * Neither the left nor right neighbors are contiguous with
		 * the new one.
		 */
		XFS_BMAP_TRACE_PRE_UPDATE("LF|RF", ip, idx,
			XFS_DATA_FORK);
		xfs_bmbt_set_state(ep, newext);
		XFS_BMAP_TRACE_POST_UPDATE("LF|RF", ip, idx,
			XFS_DATA_FORK);
		ip->i_df.if_lastex = idx;
		if (cur == NULL)
			rval = XFS_ILOG_DEXT;
		else {
			rval = 0;
			if ((error = xfs_bmbt_lookup_eq(cur, new->br_startoff,
					new->br_startblock, new->br_blockcount,
					&i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_bmbt_update(cur, new->br_startoff,
				new->br_startblock, new->br_blockcount,
				newext)))
				goto done;
		}
		/* DELTA: The in-core extent described by new changed type. */
		temp = new->br_startoff;
		temp2 = new->br_blockcount;
		break;

	case MASK2(LEFT_FILLING, LEFT_CONTIG):
		/*
		 * Setting the first part of a previous oldext extent to newext.
		 * The left neighbor is contiguous.
		 */
		XFS_BMAP_TRACE_PRE_UPDATE("LF|LC", ip, idx - 1,
			XFS_DATA_FORK);
		xfs_bmbt_set_blockcount(xfs_iext_get_ext(ifp, idx - 1),
			LEFT.br_blockcount + new->br_blockcount);
		xfs_bmbt_set_startoff(ep,
			PREV.br_startoff + new->br_blockcount);
		XFS_BMAP_TRACE_POST_UPDATE("LF|LC", ip, idx - 1,
			XFS_DATA_FORK);
		XFS_BMAP_TRACE_PRE_UPDATE("LF|LC", ip, idx,
			XFS_DATA_FORK);
		xfs_bmbt_set_startblock(ep,
			new->br_startblock + new->br_blockcount);
		xfs_bmbt_set_blockcount(ep,
			PREV.br_blockcount - new->br_blockcount);
		XFS_BMAP_TRACE_POST_UPDATE("LF|LC", ip, idx,
			XFS_DATA_FORK);
		ip->i_df.if_lastex = idx - 1;
		if (cur == NULL)
			rval = XFS_ILOG_DEXT;
		else {
			rval = 0;
			if ((error = xfs_bmbt_lookup_eq(cur, PREV.br_startoff,
					PREV.br_startblock, PREV.br_blockcount,
					&i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_bmbt_update(cur,
				PREV.br_startoff + new->br_blockcount,
				PREV.br_startblock + new->br_blockcount,
				PREV.br_blockcount - new->br_blockcount,
				oldext)))
				goto done;
			if ((error = xfs_btree_decrement(cur, 0, &i)))
				goto done;
			if (xfs_bmbt_update(cur, LEFT.br_startoff,
				LEFT.br_startblock,
				LEFT.br_blockcount + new->br_blockcount,
				LEFT.br_state))
				goto done;
		}
		/* DELTA: The boundary between two in-core extents moved. */
		temp = LEFT.br_startoff;
		temp2 = LEFT.br_blockcount +
			PREV.br_blockcount;
		break;

	case MASK(LEFT_FILLING):
		/*
		 * Setting the first part of a previous oldext extent to newext.
		 * The left neighbor is not contiguous.
		 */
		XFS_BMAP_TRACE_PRE_UPDATE("LF", ip, idx, XFS_DATA_FORK);
		ASSERT(ep && xfs_bmbt_get_state(ep) == oldext);
		xfs_bmbt_set_startoff(ep, new_endoff);
		xfs_bmbt_set_blockcount(ep,
			PREV.br_blockcount - new->br_blockcount);
		xfs_bmbt_set_startblock(ep,
			new->br_startblock + new->br_blockcount);
		XFS_BMAP_TRACE_POST_UPDATE("LF", ip, idx, XFS_DATA_FORK);
		XFS_BMAP_TRACE_INSERT("LF", ip, idx, 1, new, NULL,
			XFS_DATA_FORK);
		xfs_iext_insert(ifp, idx, 1, new);
		ip->i_df.if_lastex = idx;
		ip->i_d.di_nextents++;
		if (cur == NULL)
			rval = XFS_ILOG_CORE | XFS_ILOG_DEXT;
		else {
			rval = XFS_ILOG_CORE;
			if ((error = xfs_bmbt_lookup_eq(cur, PREV.br_startoff,
					PREV.br_startblock, PREV.br_blockcount,
					&i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_bmbt_update(cur,
				PREV.br_startoff + new->br_blockcount,
				PREV.br_startblock + new->br_blockcount,
				PREV.br_blockcount - new->br_blockcount,
				oldext)))
				goto done;
			cur->bc_rec.b = *new;
			if ((error = xfs_btree_insert(cur, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
		}
		/* DELTA: One in-core extent is split in two. */
		temp = PREV.br_startoff;
		temp2 = PREV.br_blockcount;
		break;

	case MASK2(RIGHT_FILLING, RIGHT_CONTIG):
		/*
		 * Setting the last part of a previous oldext extent to newext.
		 * The right neighbor is contiguous with the new allocation.
		 */
		XFS_BMAP_TRACE_PRE_UPDATE("RF|RC", ip, idx,
			XFS_DATA_FORK);
		XFS_BMAP_TRACE_PRE_UPDATE("RF|RC", ip, idx + 1,
			XFS_DATA_FORK);
		xfs_bmbt_set_blockcount(ep,
			PREV.br_blockcount - new->br_blockcount);
		XFS_BMAP_TRACE_POST_UPDATE("RF|RC", ip, idx,
			XFS_DATA_FORK);
		xfs_bmbt_set_allf(xfs_iext_get_ext(ifp, idx + 1),
			new->br_startoff, new->br_startblock,
			new->br_blockcount + RIGHT.br_blockcount, newext);
		XFS_BMAP_TRACE_POST_UPDATE("RF|RC", ip, idx + 1,
			XFS_DATA_FORK);
		ip->i_df.if_lastex = idx + 1;
		if (cur == NULL)
			rval = XFS_ILOG_DEXT;
		else {
			rval = 0;
			if ((error = xfs_bmbt_lookup_eq(cur, PREV.br_startoff,
					PREV.br_startblock,
					PREV.br_blockcount, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_bmbt_update(cur, PREV.br_startoff,
				PREV.br_startblock,
				PREV.br_blockcount - new->br_blockcount,
				oldext)))
				goto done;
			if ((error = xfs_btree_increment(cur, 0, &i)))
				goto done;
			if ((error = xfs_bmbt_update(cur, new->br_startoff,
				new->br_startblock,
				new->br_blockcount + RIGHT.br_blockcount,
				newext)))
				goto done;
		}
		/* DELTA: The boundary between two in-core extents moved. */
		temp = PREV.br_startoff;
		temp2 = PREV.br_blockcount +
			RIGHT.br_blockcount;
		break;

	case MASK(RIGHT_FILLING):
		/*
		 * Setting the last part of a previous oldext extent to newext.
		 * The right neighbor is not contiguous.
		 */
		XFS_BMAP_TRACE_PRE_UPDATE("RF", ip, idx, XFS_DATA_FORK);
		xfs_bmbt_set_blockcount(ep,
			PREV.br_blockcount - new->br_blockcount);
		XFS_BMAP_TRACE_POST_UPDATE("RF", ip, idx, XFS_DATA_FORK);
		XFS_BMAP_TRACE_INSERT("RF", ip, idx + 1, 1, new, NULL,
			XFS_DATA_FORK);
		xfs_iext_insert(ifp, idx + 1, 1, new);
		ip->i_df.if_lastex = idx + 1;
		ip->i_d.di_nextents++;
		if (cur == NULL)
			rval = XFS_ILOG_CORE | XFS_ILOG_DEXT;
		else {
			rval = XFS_ILOG_CORE;
			if ((error = xfs_bmbt_lookup_eq(cur, PREV.br_startoff,
					PREV.br_startblock, PREV.br_blockcount,
					&i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_bmbt_update(cur, PREV.br_startoff,
				PREV.br_startblock,
				PREV.br_blockcount - new->br_blockcount,
				oldext)))
				goto done;
			if ((error = xfs_bmbt_lookup_eq(cur, new->br_startoff,
					new->br_startblock, new->br_blockcount,
					&i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 0, done);
			cur->bc_rec.b.br_state = XFS_EXT_NORM;
			if ((error = xfs_btree_insert(cur, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
		}
		/* DELTA: One in-core extent is split in two. */
		temp = PREV.br_startoff;
		temp2 = PREV.br_blockcount;
		break;

	case 0:
		/*
		 * Setting the middle part of a previous oldext extent to
		 * newext.  Contiguity is impossible here.
		 * One extent becomes three extents.
		 */
		XFS_BMAP_TRACE_PRE_UPDATE("0", ip, idx, XFS_DATA_FORK);
		xfs_bmbt_set_blockcount(ep,
			new->br_startoff - PREV.br_startoff);
		XFS_BMAP_TRACE_POST_UPDATE("0", ip, idx, XFS_DATA_FORK);
		r[0] = *new;
		r[1].br_startoff = new_endoff;
		r[1].br_blockcount =
			PREV.br_startoff + PREV.br_blockcount - new_endoff;
		r[1].br_startblock = new->br_startblock + new->br_blockcount;
		r[1].br_state = oldext;
		XFS_BMAP_TRACE_INSERT("0", ip, idx + 1, 2, &r[0], &r[1],
			XFS_DATA_FORK);
		xfs_iext_insert(ifp, idx + 1, 2, &r[0]);
		ip->i_df.if_lastex = idx + 1;
		ip->i_d.di_nextents += 2;
		if (cur == NULL)
			rval = XFS_ILOG_CORE | XFS_ILOG_DEXT;
		else {
			rval = XFS_ILOG_CORE;
			if ((error = xfs_bmbt_lookup_eq(cur, PREV.br_startoff,
					PREV.br_startblock, PREV.br_blockcount,
					&i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			/* new right extent - oldext */
			if ((error = xfs_bmbt_update(cur, r[1].br_startoff,
				r[1].br_startblock, r[1].br_blockcount,
				r[1].br_state)))
				goto done;
			/* new left extent - oldext */
			cur->bc_rec.b = PREV;
			cur->bc_rec.b.br_blockcount =
				new->br_startoff - PREV.br_startoff;
			if ((error = xfs_btree_insert(cur, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			/*
			 * Reset the cursor to the position of the new extent
			 * we are about to insert as we can't trust it after
			 * the previous insert.
			 */
			if ((error = xfs_bmbt_lookup_eq(cur, new->br_startoff,
					new->br_startblock, new->br_blockcount,
					&i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 0, done);
			/* new middle extent - newext */
			cur->bc_rec.b.br_state = new->br_state;
			if ((error = xfs_btree_insert(cur, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
		}
		/* DELTA: One in-core extent is split in three. */
		temp = PREV.br_startoff;
		temp2 = PREV.br_blockcount;
		break;

	case MASK3(LEFT_FILLING, LEFT_CONTIG, RIGHT_CONTIG):
	case MASK3(RIGHT_FILLING, LEFT_CONTIG, RIGHT_CONTIG):
	case MASK2(LEFT_FILLING, RIGHT_CONTIG):
	case MASK2(RIGHT_FILLING, LEFT_CONTIG):
	case MASK2(LEFT_CONTIG, RIGHT_CONTIG):
	case MASK(LEFT_CONTIG):
	case MASK(RIGHT_CONTIG):
		/*
		 * These cases are all impossible.
		 */
		ASSERT(0);
	}
	*curp = cur;
	if (delta) {
		temp2 += temp;
		if (delta->xed_startoff > temp)
			delta->xed_startoff = temp;
		if (delta->xed_blockcount < temp2)
			delta->xed_blockcount = temp2;
	}
done:
	*logflagsp = rval;
	return error;
#undef	LEFT
#undef	RIGHT
#undef	PREV
#undef	MASK
#undef	MASK2
#undef	MASK3
#undef	MASK4
#undef	STATE_SET
#undef	STATE_TEST
#undef	STATE_SET_TEST
#undef	SWITCH_STATE
}

/*
 * Called by xfs_bmap_add_extent to handle cases converting a hole
 * to a delayed allocation.
 */
/*ARGSUSED*/
STATIC int				/* error */
xfs_bmap_add_extent_hole_delay(
	xfs_inode_t		*ip,	/* incore inode pointer */
	xfs_extnum_t		idx,	/* extent number to update/insert */
	xfs_bmbt_irec_t		*new,	/* new data to add to file extents */
	int			*logflagsp, /* inode logging flags */
	xfs_extdelta_t		*delta, /* Change made to incore extents */
	int			rsvd)		/* OK to allocate reserved blocks */
{
	xfs_bmbt_rec_host_t	*ep;	/* extent record for idx */
	xfs_ifork_t		*ifp;	/* inode fork pointer */
	xfs_bmbt_irec_t		left;	/* left neighbor extent entry */
	xfs_filblks_t		newlen=0;	/* new indirect size */
	xfs_filblks_t		oldlen=0;	/* old indirect size */
	xfs_bmbt_irec_t		right;	/* right neighbor extent entry */
	int			state;  /* state bits, accessed thru macros */
	xfs_filblks_t		temp=0;	/* temp for indirect calculations */
	xfs_filblks_t		temp2=0;
	enum {				/* bit number definitions for state */
		LEFT_CONTIG,	RIGHT_CONTIG,
		LEFT_DELAY,	RIGHT_DELAY,
		LEFT_VALID,	RIGHT_VALID
	};

#define	MASK(b)			(1 << (b))
#define	MASK2(a,b)		(MASK(a) | MASK(b))
#define	STATE_SET(b,v)		((v) ? (state |= MASK(b)) : (state &= ~MASK(b)))
#define	STATE_TEST(b)		(state & MASK(b))
#define	STATE_SET_TEST(b,v)	((v) ? ((state |= MASK(b)), 1) : \
				       ((state &= ~MASK(b)), 0))
#define	SWITCH_STATE		(state & MASK2(LEFT_CONTIG, RIGHT_CONTIG))

	ifp = XFS_IFORK_PTR(ip, XFS_DATA_FORK);
	ep = xfs_iext_get_ext(ifp, idx);
	state = 0;
	ASSERT(isnullstartblock(new->br_startblock));
	/*
	 * Check and set flags if this segment has a left neighbor
	 */
	if (STATE_SET_TEST(LEFT_VALID, idx > 0)) {
		xfs_bmbt_get_all(xfs_iext_get_ext(ifp, idx - 1), &left);
		STATE_SET(LEFT_DELAY, isnullstartblock(left.br_startblock));
	}
	/*
	 * Check and set flags if the current (right) segment exists.
	 * If it doesn't exist, we're converting the hole at end-of-file.
	 */
	if (STATE_SET_TEST(RIGHT_VALID,
			   idx <
			   ip->i_df.if_bytes / (uint)sizeof(xfs_bmbt_rec_t))) {
		xfs_bmbt_get_all(ep, &right);
		STATE_SET(RIGHT_DELAY, isnullstartblock(right.br_startblock));
	}
	/*
	 * Set contiguity flags on the left and right neighbors.
	 * Don't let extents get too large, even if the pieces are contiguous.
	 */
	STATE_SET(LEFT_CONTIG,
		STATE_TEST(LEFT_VALID) && STATE_TEST(LEFT_DELAY) &&
		left.br_startoff + left.br_blockcount == new->br_startoff &&
		left.br_blockcount + new->br_blockcount <= MAXEXTLEN);
	STATE_SET(RIGHT_CONTIG,
		STATE_TEST(RIGHT_VALID) && STATE_TEST(RIGHT_DELAY) &&
		new->br_startoff + new->br_blockcount == right.br_startoff &&
		new->br_blockcount + right.br_blockcount <= MAXEXTLEN &&
		(!STATE_TEST(LEFT_CONTIG) ||
		 (left.br_blockcount + new->br_blockcount +
		     right.br_blockcount <= MAXEXTLEN)));
	/*
	 * Switch out based on the contiguity flags.
	 */
	switch (SWITCH_STATE) {

	case MASK2(LEFT_CONTIG, RIGHT_CONTIG):
		/*
		 * New allocation is contiguous with delayed allocations
		 * on the left and on the right.
		 * Merge all three into a single extent record.
		 */
		temp = left.br_blockcount + new->br_blockcount +
			right.br_blockcount;
		XFS_BMAP_TRACE_PRE_UPDATE("LC|RC", ip, idx - 1,
			XFS_DATA_FORK);
		xfs_bmbt_set_blockcount(xfs_iext_get_ext(ifp, idx - 1), temp);
		oldlen = startblockval(left.br_startblock) +
			startblockval(new->br_startblock) +
			startblockval(right.br_startblock);
		newlen = xfs_bmap_worst_indlen(ip, temp);
		xfs_bmbt_set_startblock(xfs_iext_get_ext(ifp, idx - 1),
			nullstartblock((int)newlen));
		XFS_BMAP_TRACE_POST_UPDATE("LC|RC", ip, idx - 1,
			XFS_DATA_FORK);
		XFS_BMAP_TRACE_DELETE("LC|RC", ip, idx, 1, XFS_DATA_FORK);
		xfs_iext_remove(ifp, idx, 1);
		ip->i_df.if_lastex = idx - 1;
		/* DELTA: Two in-core extents were replaced by one. */
		temp2 = temp;
		temp = left.br_startoff;
		break;

	case MASK(LEFT_CONTIG):
		/*
		 * New allocation is contiguous with a delayed allocation
		 * on the left.
		 * Merge the new allocation with the left neighbor.
		 */
		temp = left.br_blockcount + new->br_blockcount;
		XFS_BMAP_TRACE_PRE_UPDATE("LC", ip, idx - 1,
			XFS_DATA_FORK);
		xfs_bmbt_set_blockcount(xfs_iext_get_ext(ifp, idx - 1), temp);
		oldlen = startblockval(left.br_startblock) +
			startblockval(new->br_startblock);
		newlen = xfs_bmap_worst_indlen(ip, temp);
		xfs_bmbt_set_startblock(xfs_iext_get_ext(ifp, idx - 1),
			nullstartblock((int)newlen));
		XFS_BMAP_TRACE_POST_UPDATE("LC", ip, idx - 1,
			XFS_DATA_FORK);
		ip->i_df.if_lastex = idx - 1;
		/* DELTA: One in-core extent grew into a hole. */
		temp2 = temp;
		temp = left.br_startoff;
		break;

	case MASK(RIGHT_CONTIG):
		/*
		 * New allocation is contiguous with a delayed allocation
		 * on the right.
		 * Merge the new allocation with the right neighbor.
		 */
		XFS_BMAP_TRACE_PRE_UPDATE("RC", ip, idx, XFS_DATA_FORK);
		temp = new->br_blockcount + right.br_blockcount;
		oldlen = startblockval(new->br_startblock) +
			startblockval(right.br_startblock);
		newlen = xfs_bmap_worst_indlen(ip, temp);
		xfs_bmbt_set_allf(ep, new->br_startoff,
			nullstartblock((int)newlen), temp, right.br_state);
		XFS_BMAP_TRACE_POST_UPDATE("RC", ip, idx, XFS_DATA_FORK);
		ip->i_df.if_lastex = idx;
		/* DELTA: One in-core extent grew into a hole. */
		temp2 = temp;
		temp = new->br_startoff;
		break;

	case 0:
		/*
		 * New allocation is not contiguous with another
		 * delayed allocation.
		 * Insert a new entry.
		 */
		oldlen = newlen = 0;
		XFS_BMAP_TRACE_INSERT("0", ip, idx, 1, new, NULL,
			XFS_DATA_FORK);
		xfs_iext_insert(ifp, idx, 1, new);
		ip->i_df.if_lastex = idx;
		/* DELTA: A new in-core extent was added in a hole. */
		temp2 = new->br_blockcount;
		temp = new->br_startoff;
		break;
	}
	if (oldlen != newlen) {
		ASSERT(oldlen > newlen);
		xfs_mod_incore_sb(ip->i_mount, XFS_SBS_FDBLOCKS,
			(int64_t)(oldlen - newlen), rsvd);
		/*
		 * Nothing to do for disk quota accounting here.
		 */
	}
	if (delta) {
		temp2 += temp;
		if (delta->xed_startoff > temp)
			delta->xed_startoff = temp;
		if (delta->xed_blockcount < temp2)
			delta->xed_blockcount = temp2;
	}
	*logflagsp = 0;
	return 0;
#undef	MASK
#undef	MASK2
#undef	STATE_SET
#undef	STATE_TEST
#undef	STATE_SET_TEST
#undef	SWITCH_STATE
}

/*
 * Called by xfs_bmap_add_extent to handle cases converting a hole
 * to a real allocation.
 */
STATIC int				/* error */
xfs_bmap_add_extent_hole_real(
	xfs_inode_t		*ip,	/* incore inode pointer */
	xfs_extnum_t		idx,	/* extent number to update/insert */
	xfs_btree_cur_t		*cur,	/* if null, not a btree */
	xfs_bmbt_irec_t		*new,	/* new data to add to file extents */
	int			*logflagsp, /* inode logging flags */
	xfs_extdelta_t		*delta, /* Change made to incore extents */
	int			whichfork) /* data or attr fork */
{
	xfs_bmbt_rec_host_t	*ep;	/* pointer to extent entry ins. point */
	int			error;	/* error return value */
	int			i;	/* temp state */
	xfs_ifork_t		*ifp;	/* inode fork pointer */
	xfs_bmbt_irec_t		left;	/* left neighbor extent entry */
	xfs_bmbt_irec_t		right;	/* right neighbor extent entry */
	int			rval=0;	/* return value (logging flags) */
	int			state;	/* state bits, accessed thru macros */
	xfs_filblks_t		temp=0;
	xfs_filblks_t		temp2=0;
	enum {				/* bit number definitions for state */
		LEFT_CONTIG,	RIGHT_CONTIG,
		LEFT_DELAY,	RIGHT_DELAY,
		LEFT_VALID,	RIGHT_VALID
	};

#define	MASK(b)			(1 << (b))
#define	MASK2(a,b)		(MASK(a) | MASK(b))
#define	STATE_SET(b,v)		((v) ? (state |= MASK(b)) : (state &= ~MASK(b)))
#define	STATE_TEST(b)		(state & MASK(b))
#define	STATE_SET_TEST(b,v)	((v) ? ((state |= MASK(b)), 1) : \
				       ((state &= ~MASK(b)), 0))
#define	SWITCH_STATE		(state & MASK2(LEFT_CONTIG, RIGHT_CONTIG))

	ifp = XFS_IFORK_PTR(ip, whichfork);
	ASSERT(idx <= ifp->if_bytes / (uint)sizeof(xfs_bmbt_rec_t));
	ep = xfs_iext_get_ext(ifp, idx);
	state = 0;
	/*
	 * Check and set flags if this segment has a left neighbor.
	 */
	if (STATE_SET_TEST(LEFT_VALID, idx > 0)) {
		xfs_bmbt_get_all(xfs_iext_get_ext(ifp, idx - 1), &left);
		STATE_SET(LEFT_DELAY, isnullstartblock(left.br_startblock));
	}
	/*
	 * Check and set flags if this segment has a current value.
	 * Not true if we're inserting into the "hole" at eof.
	 */
	if (STATE_SET_TEST(RIGHT_VALID,
			   idx <
			   ifp->if_bytes / (uint)sizeof(xfs_bmbt_rec_t))) {
		xfs_bmbt_get_all(ep, &right);
		STATE_SET(RIGHT_DELAY, isnullstartblock(right.br_startblock));
	}
	/*
	 * We're inserting a real allocation between "left" and "right".
	 * Set the contiguity flags.  Don't let extents get too large.
	 */
	STATE_SET(LEFT_CONTIG,
		STATE_TEST(LEFT_VALID) && !STATE_TEST(LEFT_DELAY) &&
		left.br_startoff + left.br_blockcount == new->br_startoff &&
		left.br_startblock + left.br_blockcount == new->br_startblock &&
		left.br_state == new->br_state &&
		left.br_blockcount + new->br_blockcount <= MAXEXTLEN);
	STATE_SET(RIGHT_CONTIG,
		STATE_TEST(RIGHT_VALID) && !STATE_TEST(RIGHT_DELAY) &&
		new->br_startoff + new->br_blockcount == right.br_startoff &&
		new->br_startblock + new->br_blockcount ==
		    right.br_startblock &&
		new->br_state == right.br_state &&
		new->br_blockcount + right.br_blockcount <= MAXEXTLEN &&
		(!STATE_TEST(LEFT_CONTIG) ||
		 left.br_blockcount + new->br_blockcount +
		     right.br_blockcount <= MAXEXTLEN));

	error = 0;
	/*
	 * Select which case we're in here, and implement it.
	 */
	switch (SWITCH_STATE) {

	case MASK2(LEFT_CONTIG, RIGHT_CONTIG):
		/*
		 * New allocation is contiguous with real allocations on the
		 * left and on the right.
		 * Merge all three into a single extent record.
		 */
		XFS_BMAP_TRACE_PRE_UPDATE("LC|RC", ip, idx - 1,
			whichfork);
		xfs_bmbt_set_blockcount(xfs_iext_get_ext(ifp, idx - 1),
			left.br_blockcount + new->br_blockcount +
			right.br_blockcount);
		XFS_BMAP_TRACE_POST_UPDATE("LC|RC", ip, idx - 1,
			whichfork);
		XFS_BMAP_TRACE_DELETE("LC|RC", ip, idx, 1, whichfork);
		xfs_iext_remove(ifp, idx, 1);
		ifp->if_lastex = idx - 1;
		XFS_IFORK_NEXT_SET(ip, whichfork,
			XFS_IFORK_NEXTENTS(ip, whichfork) - 1);
		if (cur == NULL) {
			rval = XFS_ILOG_CORE | xfs_ilog_fext(whichfork);
		} else {
			rval = XFS_ILOG_CORE;
			if ((error = xfs_bmbt_lookup_eq(cur,
					right.br_startoff,
					right.br_startblock,
					right.br_blockcount, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_btree_delete(cur, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_btree_decrement(cur, 0, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_bmbt_update(cur, left.br_startoff,
					left.br_startblock,
					left.br_blockcount +
						new->br_blockcount +
						right.br_blockcount,
					left.br_state)))
				goto done;
		}
		/* DELTA: Two in-core extents were replaced by one. */
		temp = left.br_startoff;
		temp2 = left.br_blockcount +
			new->br_blockcount +
			right.br_blockcount;
		break;

	case MASK(LEFT_CONTIG):
		/*
		 * New allocation is contiguous with a real allocation
		 * on the left.
		 * Merge the new allocation with the left neighbor.
		 */
		XFS_BMAP_TRACE_PRE_UPDATE("LC", ip, idx - 1, whichfork);
		xfs_bmbt_set_blockcount(xfs_iext_get_ext(ifp, idx - 1),
			left.br_blockcount + new->br_blockcount);
		XFS_BMAP_TRACE_POST_UPDATE("LC", ip, idx - 1, whichfork);
		ifp->if_lastex = idx - 1;
		if (cur == NULL) {
			rval = xfs_ilog_fext(whichfork);
		} else {
			rval = 0;
			if ((error = xfs_bmbt_lookup_eq(cur,
					left.br_startoff,
					left.br_startblock,
					left.br_blockcount, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_bmbt_update(cur, left.br_startoff,
					left.br_startblock,
					left.br_blockcount +
						new->br_blockcount,
					left.br_state)))
				goto done;
		}
		/* DELTA: One in-core extent grew. */
		temp = left.br_startoff;
		temp2 = left.br_blockcount +
			new->br_blockcount;
		break;

	case MASK(RIGHT_CONTIG):
		/*
		 * New allocation is contiguous with a real allocation
		 * on the right.
		 * Merge the new allocation with the right neighbor.
		 */
		XFS_BMAP_TRACE_PRE_UPDATE("RC", ip, idx, whichfork);
		xfs_bmbt_set_allf(ep, new->br_startoff, new->br_startblock,
			new->br_blockcount + right.br_blockcount,
			right.br_state);
		XFS_BMAP_TRACE_POST_UPDATE("RC", ip, idx, whichfork);
		ifp->if_lastex = idx;
		if (cur == NULL) {
			rval = xfs_ilog_fext(whichfork);
		} else {
			rval = 0;
			if ((error = xfs_bmbt_lookup_eq(cur,
					right.br_startoff,
					right.br_startblock,
					right.br_blockcount, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_bmbt_update(cur, new->br_startoff,
					new->br_startblock,
					new->br_blockcount +
						right.br_blockcount,
					right.br_state)))
				goto done;
		}
		/* DELTA: One in-core extent grew. */
		temp = new->br_startoff;
		temp2 = new->br_blockcount +
			right.br_blockcount;
		break;

	case 0:
		/*
		 * New allocation is not contiguous with another
		 * real allocation.
		 * Insert a new entry.
		 */
		XFS_BMAP_TRACE_INSERT("0", ip, idx, 1, new, NULL, whichfork);
		xfs_iext_insert(ifp, idx, 1, new);
		ifp->if_lastex = idx;
		XFS_IFORK_NEXT_SET(ip, whichfork,
			XFS_IFORK_NEXTENTS(ip, whichfork) + 1);
		if (cur == NULL) {
			rval = XFS_ILOG_CORE | xfs_ilog_fext(whichfork);
		} else {
			rval = XFS_ILOG_CORE;
			if ((error = xfs_bmbt_lookup_eq(cur,
					new->br_startoff,
					new->br_startblock,
					new->br_blockcount, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 0, done);
			cur->bc_rec.b.br_state = new->br_state;
			if ((error = xfs_btree_insert(cur, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
		}
		/* DELTA: A new extent was added in a hole. */
		temp = new->br_startoff;
		temp2 = new->br_blockcount;
		break;
	}
	if (delta) {
		temp2 += temp;
		if (delta->xed_startoff > temp)
			delta->xed_startoff = temp;
		if (delta->xed_blockcount < temp2)
			delta->xed_blockcount = temp2;
	}
done:
	*logflagsp = rval;
	return error;
#undef	MASK
#undef	MASK2
#undef	STATE_SET
#undef	STATE_TEST
#undef	STATE_SET_TEST
#undef	SWITCH_STATE
}

/*
 * Adjust the size of the new extent based on di_extsize and rt extsize.
 */
STATIC int
xfs_bmap_extsize_align(
	xfs_mount_t	*mp,
	xfs_bmbt_irec_t	*gotp,		/* next extent pointer */
	xfs_bmbt_irec_t	*prevp,		/* previous extent pointer */
	xfs_extlen_t	extsz,		/* align to this extent size */
	int		rt,		/* is this a realtime inode? */
	int		eof,		/* is extent at end-of-file? */
	int		delay,		/* creating delalloc extent? */
	int		convert,	/* overwriting unwritten extent? */
	xfs_fileoff_t	*offp,		/* in/out: aligned offset */
	xfs_extlen_t	*lenp)		/* in/out: aligned length */
{
	xfs_fileoff_t	orig_off;	/* original offset */
	xfs_extlen_t	orig_alen;	/* original length */
	xfs_fileoff_t	orig_end;	/* original off+len */
	xfs_fileoff_t	nexto;		/* next file offset */
	xfs_fileoff_t	prevo;		/* previous file offset */
	xfs_fileoff_t	align_off;	/* temp for offset */
	xfs_extlen_t	align_alen;	/* temp for length */
	xfs_extlen_t	temp;		/* temp for calculations */

	if (convert)
		return 0;

	orig_off = align_off = *offp;
	orig_alen = align_alen = *lenp;
	orig_end = orig_off + orig_alen;

	/*
	 * If this request overlaps an existing extent, then don't
	 * attempt to perform any additional alignment.
	 */
	if (!delay && !eof &&
	    (orig_off >= gotp->br_startoff) &&
	    (orig_end <= gotp->br_startoff + gotp->br_blockcount)) {
		return 0;
	}

	/*
	 * If the file offset is unaligned vs. the extent size
	 * we need to align it.  This will be possible unless
	 * the file was previously written with a kernel that didn't
	 * perform this alignment, or if a truncate shot us in the
	 * foot.
	 */
	temp = do_mod(orig_off, extsz);
	if (temp) {
		align_alen += temp;
		align_off -= temp;
	}
	/*
	 * Same adjustment for the end of the requested area.
	 */
	if ((temp = (align_alen % extsz))) {
		align_alen += extsz - temp;
	}
	/*
	 * If the previous block overlaps with this proposed allocation
	 * then move the start forward without adjusting the length.
	 */
	if (prevp->br_startoff != NULLFILEOFF) {
		if (prevp->br_startblock == HOLESTARTBLOCK)
			prevo = prevp->br_startoff;
		else
			prevo = prevp->br_startoff + prevp->br_blockcount;
	} else
		prevo = 0;
	if (align_off != orig_off && align_off < prevo)
		align_off = prevo;
	/*
	 * If the next block overlaps with this proposed allocation
	 * then move the start back without adjusting the length,
	 * but not before offset 0.
	 * This may of course make the start overlap previous block,
	 * and if we hit the offset 0 limit then the next block
	 * can still overlap too.
	 */
	if (!eof && gotp->br_startoff != NULLFILEOFF) {
		if ((delay && gotp->br_startblock == HOLESTARTBLOCK) ||
		    (!delay && gotp->br_startblock == DELAYSTARTBLOCK))
			nexto = gotp->br_startoff + gotp->br_blockcount;
		else
			nexto = gotp->br_startoff;
	} else
		nexto = NULLFILEOFF;
	if (!eof &&
	    align_off + align_alen != orig_end &&
	    align_off + align_alen > nexto)
		align_off = nexto > align_alen ? nexto - align_alen : 0;
	/*
	 * If we're now overlapping the next or previous extent that
	 * means we can't fit an extsz piece in this hole.  Just move
	 * the start forward to the first valid spot and set
	 * the length so we hit the end.
	 */
	if (align_off != orig_off && align_off < prevo)
		align_off = prevo;
	if (align_off + align_alen != orig_end &&
	    align_off + align_alen > nexto &&
	    nexto != NULLFILEOFF) {
		ASSERT(nexto > prevo);
		align_alen = nexto - align_off;
	}

	/*
	 * If realtime, and the result isn't a multiple of the realtime
	 * extent size we need to remove blocks until it is.
	 */
	if (rt && (temp = (align_alen % mp->m_sb.sb_rextsize))) {
		/*
		 * We're not covering the original request, or
		 * we won't be able to once we fix the length.
		 */
		if (orig_off < align_off ||
		    orig_end > align_off + align_alen ||
		    align_alen - temp < orig_alen)
			return XFS_ERROR(EINVAL);
		/*
		 * Try to fix it by moving the start up.
		 */
		if (align_off + temp <= orig_off) {
			align_alen -= temp;
			align_off += temp;
		}
		/*
		 * Try to fix it by moving the end in.
		 */
		else if (align_off + align_alen - temp >= orig_end)
			align_alen -= temp;
		/*
		 * Set the start to the minimum then trim the length.
		 */
		else {
			align_alen -= orig_off - align_off;
			align_off = orig_off;
			align_alen -= align_alen % mp->m_sb.sb_rextsize;
		}
		/*
		 * Result doesn't cover the request, fail it.
		 */
		if (orig_off < align_off || orig_end > align_off + align_alen)
			return XFS_ERROR(EINVAL);
	} else {
		ASSERT(orig_off >= align_off);
		ASSERT(orig_end <= align_off + align_alen);
	}

#ifdef DEBUG
	if (!eof && gotp->br_startoff != NULLFILEOFF)
		ASSERT(align_off + align_alen <= gotp->br_startoff);
	if (prevp->br_startoff != NULLFILEOFF)
		ASSERT(align_off >= prevp->br_startoff + prevp->br_blockcount);
#endif

	*lenp = align_alen;
	*offp = align_off;
	return 0;
}

#define XFS_ALLOC_GAP_UNITS	4

STATIC void
xfs_bmap_adjacent(
	xfs_bmalloca_t	*ap)		/* bmap alloc argument struct */
{
	xfs_fsblock_t	adjust;		/* adjustment to block numbers */
	xfs_agnumber_t	fb_agno;	/* ag number of ap->firstblock */
	xfs_mount_t	*mp;		/* mount point structure */
	int		nullfb;		/* true if ap->firstblock isn't set */
	int		rt;		/* true if inode is realtime */

#define	ISVALID(x,y)	\
	(rt ? \
		(x) < mp->m_sb.sb_rblocks : \
		XFS_FSB_TO_AGNO(mp, x) == XFS_FSB_TO_AGNO(mp, y) && \
		XFS_FSB_TO_AGNO(mp, x) < mp->m_sb.sb_agcount && \
		XFS_FSB_TO_AGBNO(mp, x) < mp->m_sb.sb_agblocks)

	mp = ap->ip->i_mount;
	nullfb = ap->firstblock == NULLFSBLOCK;
	rt = XFS_IS_REALTIME_INODE(ap->ip) && ap->userdata;
	fb_agno = nullfb ? NULLAGNUMBER : XFS_FSB_TO_AGNO(mp, ap->firstblock);
	/*
	 * If allocating at eof, and there's a previous real block,
	 * try to use its last block as our starting point.
	 */
	if (ap->eof && ap->prevp->br_startoff != NULLFILEOFF &&
	    !isnullstartblock(ap->prevp->br_startblock) &&
	    ISVALID(ap->prevp->br_startblock + ap->prevp->br_blockcount,
		    ap->prevp->br_startblock)) {
		ap->rval = ap->prevp->br_startblock + ap->prevp->br_blockcount;
		/*
		 * Adjust for the gap between prevp and us.
		 */
		adjust = ap->off -
			(ap->prevp->br_startoff + ap->prevp->br_blockcount);
		if (adjust &&
		    ISVALID(ap->rval + adjust, ap->prevp->br_startblock))
			ap->rval += adjust;
	}
	/*
	 * If not at eof, then compare the two neighbor blocks.
	 * Figure out whether either one gives us a good starting point,
	 * and pick the better one.
	 */
	else if (!ap->eof) {
		xfs_fsblock_t	gotbno;		/* right side block number */
		xfs_fsblock_t	gotdiff=0;	/* right side difference */
		xfs_fsblock_t	prevbno;	/* left side block number */
		xfs_fsblock_t	prevdiff=0;	/* left side difference */

		/*
		 * If there's a previous (left) block, select a requested
		 * start block based on it.
		 */
		if (ap->prevp->br_startoff != NULLFILEOFF &&
		    !isnullstartblock(ap->prevp->br_startblock) &&
		    (prevbno = ap->prevp->br_startblock +
			       ap->prevp->br_blockcount) &&
		    ISVALID(prevbno, ap->prevp->br_startblock)) {
			/*
			 * Calculate gap to end of previous block.
			 */
			adjust = prevdiff = ap->off -
				(ap->prevp->br_startoff +
				 ap->prevp->br_blockcount);
			/*
			 * Figure the startblock based on the previous block's
			 * end and the gap size.
			 * Heuristic!
			 * If the gap is large relative to the piece we're
			 * allocating, or using it gives us an invalid block
			 * number, then just use the end of the previous block.
			 */
			if (prevdiff <= XFS_ALLOC_GAP_UNITS * ap->alen &&
			    ISVALID(prevbno + prevdiff,
				    ap->prevp->br_startblock))
				prevbno += adjust;
			else
				prevdiff += adjust;
			/*
			 * If the firstblock forbids it, can't use it,
			 * must use default.
			 */
			if (!rt && !nullfb &&
			    XFS_FSB_TO_AGNO(mp, prevbno) != fb_agno)
				prevbno = NULLFSBLOCK;
		}
		/*
		 * No previous block or can't follow it, just default.
		 */
		else
			prevbno = NULLFSBLOCK;
		/*
		 * If there's a following (right) block, select a requested
		 * start block based on it.
		 */
		if (!isnullstartblock(ap->gotp->br_startblock)) {
			/*
			 * Calculate gap to start of next block.
			 */
			adjust = gotdiff = ap->gotp->br_startoff - ap->off;
			/*
			 * Figure the startblock based on the next block's
			 * start and the gap size.
			 */
			gotbno = ap->gotp->br_startblock;
			/*
			 * Heuristic!
			 * If the gap is large relative to the piece we're
			 * allocating, or using it gives us an invalid block
			 * number, then just use the start of the next block
			 * offset by our length.
			 */
			if (gotdiff <= XFS_ALLOC_GAP_UNITS * ap->alen &&
			    ISVALID(gotbno - gotdiff, gotbno))
				gotbno -= adjust;
			else if (ISVALID(gotbno - ap->alen, gotbno)) {
				gotbno -= ap->alen;
				gotdiff += adjust - ap->alen;
			} else
				gotdiff += adjust;
			/*
			 * If the firstblock forbids it, can't use it,
			 * must use default.
			 */
			if (!rt && !nullfb &&
			    XFS_FSB_TO_AGNO(mp, gotbno) != fb_agno)
				gotbno = NULLFSBLOCK;
		}
		/*
		 * No next block, just default.
		 */
		else
			gotbno = NULLFSBLOCK;
		/*
		 * If both valid, pick the better one, else the only good
		 * one, else ap->rval is already set (to 0 or the inode block).
		 */
		if (prevbno != NULLFSBLOCK && gotbno != NULLFSBLOCK)
			ap->rval = prevdiff <= gotdiff ? prevbno : gotbno;
		else if (prevbno != NULLFSBLOCK)
			ap->rval = prevbno;
		else if (gotbno != NULLFSBLOCK)
			ap->rval = gotbno;
	}
#undef ISVALID
}

STATIC int
xfs_bmap_rtalloc(
	xfs_bmalloca_t	*ap)		/* bmap alloc argument struct */
{
	xfs_alloctype_t	atype = 0;	/* type for allocation routines */
	int		error;		/* error return value */
	xfs_mount_t	*mp;		/* mount point structure */
	xfs_extlen_t	prod = 0;	/* product factor for allocators */
	xfs_extlen_t	ralen = 0;	/* realtime allocation length */
	xfs_extlen_t	align;		/* minimum allocation alignment */
	xfs_rtblock_t	rtb;

	mp = ap->ip->i_mount;
	align = xfs_get_extsz_hint(ap->ip);
	prod = align / mp->m_sb.sb_rextsize;
	error = xfs_bmap_extsize_align(mp, ap->gotp, ap->prevp,
					align, 1, ap->eof, 0,
					ap->conv, &ap->off, &ap->alen);
	if (error)
		return error;
	ASSERT(ap->alen);
	ASSERT(ap->alen % mp->m_sb.sb_rextsize == 0);

	/*
	 * If the offset & length are not perfectly aligned
	 * then kill prod, it will just get us in trouble.
	 */
	if (do_mod(ap->off, align) || ap->alen % align)
		prod = 1;
	/*
	 * Set ralen to be the actual requested length in rtextents.
	 */
	ralen = ap->alen / mp->m_sb.sb_rextsize;
	/*
	 * If the old value was close enough to MAXEXTLEN that
	 * we rounded up to it, cut it back so it's valid again.
	 * Note that if it's a really large request (bigger than
	 * MAXEXTLEN), we don't hear about that number, and can't
	 * adjust the starting point to match it.
	 */
	if (ralen * mp->m_sb.sb_rextsize >= MAXEXTLEN)
		ralen = MAXEXTLEN / mp->m_sb.sb_rextsize;
	/*
	 * If it's an allocation to an empty file at offset 0,
	 * pick an extent that will space things out in the rt area.
	 */
	if (ap->eof && ap->off == 0) {
		xfs_rtblock_t uninitialized_var(rtx); /* realtime extent no */

		error = xfs_rtpick_extent(mp, ap->tp, ralen, &rtx);
		if (error)
			return error;
		ap->rval = rtx * mp->m_sb.sb_rextsize;
	} else {
		ap->rval = 0;
	}

	xfs_bmap_adjacent(ap);

	/*
	 * Realtime allocation, done through xfs_rtallocate_extent.
	 */
	atype = ap->rval == 0 ?  XFS_ALLOCTYPE_ANY_AG : XFS_ALLOCTYPE_NEAR_BNO;
	do_div(ap->rval, mp->m_sb.sb_rextsize);
	rtb = ap->rval;
	ap->alen = ralen;
	if ((error = xfs_rtallocate_extent(ap->tp, ap->rval, 1, ap->alen,
				&ralen, atype, ap->wasdel, prod, &rtb)))
		return error;
	if (rtb == NULLFSBLOCK && prod > 1 &&
	    (error = xfs_rtallocate_extent(ap->tp, ap->rval, 1,
					   ap->alen, &ralen, atype,
					   ap->wasdel, 1, &rtb)))
		return error;
	ap->rval = rtb;
	if (ap->rval != NULLFSBLOCK) {
		ap->rval *= mp->m_sb.sb_rextsize;
		ralen *= mp->m_sb.sb_rextsize;
		ap->alen = ralen;
		ap->ip->i_d.di_nblocks += ralen;
		xfs_trans_log_inode(ap->tp, ap->ip, XFS_ILOG_CORE);
		if (ap->wasdel)
			ap->ip->i_delayed_blks -= ralen;
		/*
		 * Adjust the disk quota also. This was reserved
		 * earlier.
		 */
		xfs_trans_mod_dquot_byino(ap->tp, ap->ip,
			ap->wasdel ? XFS_TRANS_DQ_DELRTBCOUNT :
					XFS_TRANS_DQ_RTBCOUNT, (long) ralen);
	} else {
		ap->alen = 0;
	}
	return 0;
}

STATIC int
xfs_bmap_btalloc(
	xfs_bmalloca_t	*ap)		/* bmap alloc argument struct */
{
	xfs_mount_t	*mp;		/* mount point structure */
	xfs_alloctype_t	atype = 0;	/* type for allocation routines */
	xfs_extlen_t	align;		/* minimum allocation alignment */
	xfs_agnumber_t	ag;
	xfs_agnumber_t	fb_agno;	/* ag number of ap->firstblock */
	xfs_agnumber_t	startag;
	xfs_alloc_arg_t	args;
	xfs_extlen_t	blen;
	xfs_extlen_t	nextminlen = 0;
	xfs_perag_t	*pag;
	int		nullfb;		/* true if ap->firstblock isn't set */
	int		isaligned;
	int		notinit;
	int		tryagain;
	int		error;

	mp = ap->ip->i_mount;
	align = ap->userdata ? xfs_get_extsz_hint(ap->ip) : 0;
	if (unlikely(align)) {
		error = xfs_bmap_extsize_align(mp, ap->gotp, ap->prevp,
						align, 0, ap->eof, 0, ap->conv,
						&ap->off, &ap->alen);
		ASSERT(!error);
		ASSERT(ap->alen);
	}
	nullfb = ap->firstblock == NULLFSBLOCK;
	fb_agno = nullfb ? NULLAGNUMBER : XFS_FSB_TO_AGNO(mp, ap->firstblock);
	if (nullfb) {
		if (ap->userdata && xfs_inode_is_filestream(ap->ip)) {
			ag = xfs_filestream_lookup_ag(ap->ip);
			ag = (ag != NULLAGNUMBER) ? ag : 0;
			ap->rval = XFS_AGB_TO_FSB(mp, ag, 0);
		} else {
			ap->rval = XFS_INO_TO_FSB(mp, ap->ip->i_ino);
		}
	} else
		ap->rval = ap->firstblock;

	xfs_bmap_adjacent(ap);

	/*
	 * If allowed, use ap->rval; otherwise must use firstblock since
	 * it's in the right allocation group.
	 */
	if (nullfb || XFS_FSB_TO_AGNO(mp, ap->rval) == fb_agno)
		;
	else
		ap->rval = ap->firstblock;
	/*
	 * Normal allocation, done through xfs_alloc_vextent.
	 */
	tryagain = isaligned = 0;
	args.tp = ap->tp;
	args.mp = mp;
	args.fsbno = ap->rval;
	args.maxlen = MIN(ap->alen, mp->m_sb.sb_agblocks);
	args.firstblock = ap->firstblock;
	blen = 0;
	if (nullfb) {
		if (ap->userdata && xfs_inode_is_filestream(ap->ip))
			args.type = XFS_ALLOCTYPE_NEAR_BNO;
		else
			args.type = XFS_ALLOCTYPE_START_BNO;
		args.total = ap->total;

		/*
		 * Search for an allocation group with a single extent
		 * large enough for the request.
		 *
		 * If one isn't found, then adjust the minimum allocation
		 * size to the largest space found.
		 */
		startag = ag = XFS_FSB_TO_AGNO(mp, args.fsbno);
		if (startag == NULLAGNUMBER)
			startag = ag = 0;
		notinit = 0;
		down_read(&mp->m_peraglock);
		while (blen < ap->alen) {
			pag = &mp->m_perag[ag];
			if (!pag->pagf_init &&
			    (error = xfs_alloc_pagf_init(mp, args.tp,
				    ag, XFS_ALLOC_FLAG_TRYLOCK))) {
				up_read(&mp->m_peraglock);
				return error;
			}
			/*
			 * See xfs_alloc_fix_freelist...
			 */
			if (pag->pagf_init) {
				xfs_extlen_t	longest;
				longest = xfs_alloc_longest_free_extent(mp, pag);
				if (blen < longest)
					blen = longest;
			} else
				notinit = 1;

			if (xfs_inode_is_filestream(ap->ip)) {
				if (blen >= ap->alen)
					break;

				if (ap->userdata) {
					/*
					 * If startag is an invalid AG, we've
					 * come here once before and
					 * xfs_filestream_new_ag picked the
					 * best currently available.
					 *
					 * Don't continue looping, since we
					 * could loop forever.
					 */
					if (startag == NULLAGNUMBER)
						break;

					error = xfs_filestream_new_ag(ap, &ag);
					if (error) {
						up_read(&mp->m_peraglock);
						return error;
					}

					/* loop again to set 'blen'*/
					startag = NULLAGNUMBER;
					continue;
				}
			}
			if (++ag == mp->m_sb.sb_agcount)
				ag = 0;
			if (ag == startag)
				break;
		}
		up_read(&mp->m_peraglock);
		/*
		 * Since the above loop did a BUF_TRYLOCK, it is
		 * possible that there is space for this request.
		 */
		if (notinit || blen < ap->minlen)
			args.minlen = ap->minlen;
		/*
		 * If the best seen length is less than the request
		 * length, use the best as the minimum.
		 */
		else if (blen < ap->alen)
			args.minlen = blen;
		/*
		 * Otherwise we've seen an extent as big as alen,
		 * use that as the minimum.
		 */
		else
			args.minlen = ap->alen;

		/*
		 * set the failure fallback case to look in the selected
		 * AG as the stream may have moved.
		 */
		if (xfs_inode_is_filestream(ap->ip))
			ap->rval = args.fsbno = XFS_AGB_TO_FSB(mp, ag, 0);
	} else if (ap->low) {
		if (xfs_inode_is_filestream(ap->ip))
			args.type = XFS_ALLOCTYPE_FIRST_AG;
		else
			args.type = XFS_ALLOCTYPE_START_BNO;
		args.total = args.minlen = ap->minlen;
	} else {
		args.type = XFS_ALLOCTYPE_NEAR_BNO;
		args.total = ap->total;
		args.minlen = ap->minlen;
	}
	/* apply extent size hints if obtained earlier */
	if (unlikely(align)) {
		args.prod = align;
		if ((args.mod = (xfs_extlen_t)do_mod(ap->off, args.prod)))
			args.mod = (xfs_extlen_t)(args.prod - args.mod);
	} else if (mp->m_sb.sb_blocksize >= PAGE_CACHE_SIZE) {
		args.prod = 1;
		args.mod = 0;
	} else {
		args.prod = PAGE_CACHE_SIZE >> mp->m_sb.sb_blocklog;
		if ((args.mod = (xfs_extlen_t)(do_mod(ap->off, args.prod))))
			args.mod = (xfs_extlen_t)(args.prod - args.mod);
	}
	/*
	 * If we are not low on available data blocks, and the
	 * underlying logical volume manager is a stripe, and
	 * the file offset is zero then try to allocate data
	 * blocks on stripe unit boundary.
	 * NOTE: ap->aeof is only set if the allocation length
	 * is >= the stripe unit and the allocation offset is
	 * at the end of file.
	 */
	if (!ap->low && ap->aeof) {
		if (!ap->off) {
			args.alignment = mp->m_dalign;
			atype = args.type;
			isaligned = 1;
			/*
			 * Adjust for alignment
			 */
			if (blen > args.alignment && blen <= ap->alen)
				args.minlen = blen - args.alignment;
			args.minalignslop = 0;
		} else {
			/*
			 * First try an exact bno allocation.
			 * If it fails then do a near or start bno
			 * allocation with alignment turned on.
			 */
			atype = args.type;
			tryagain = 1;
			args.type = XFS_ALLOCTYPE_THIS_BNO;
			args.alignment = 1;
			/*
			 * Compute the minlen+alignment for the
			 * next case.  Set slop so that the value
			 * of minlen+alignment+slop doesn't go up
			 * between the calls.
			 */
			if (blen > mp->m_dalign && blen <= ap->alen)
				nextminlen = blen - mp->m_dalign;
			else
				nextminlen = args.minlen;
			if (nextminlen + mp->m_dalign > args.minlen + 1)
				args.minalignslop =
					nextminlen + mp->m_dalign -
					args.minlen - 1;
			else
				args.minalignslop = 0;
		}
	} else {
		args.alignment = 1;
		args.minalignslop = 0;
	}
	args.minleft = ap->minleft;
	args.wasdel = ap->wasdel;
	args.isfl = 0;
	args.userdata = ap->userdata;
	if ((error = xfs_alloc_vextent(&args)))
		return error;
	if (tryagain && args.fsbno == NULLFSBLOCK) {
		/*
		 * Exact allocation failed. Now try with alignment
		 * turned on.
		 */
		args.type = atype;
		args.fsbno = ap->rval;
		args.alignment = mp->m_dalign;
		args.minlen = nextminlen;
		args.minalignslop = 0;
		isaligned = 1;
		if ((error = xfs_alloc_vextent(&args)))
			return error;
	}
	if (isaligned && args.fsbno == NULLFSBLOCK) {
		/*
		 * allocation failed, so turn off alignment and
		 * try again.
		 */
		args.type = atype;
		args.fsbno = ap->rval;
		args.alignment = 0;
		if ((error = xfs_alloc_vextent(&args)))
			return error;
	}
	if (args.fsbno == NULLFSBLOCK && nullfb &&
	    args.minlen > ap->minlen) {
		args.minlen = ap->minlen;
		args.type = XFS_ALLOCTYPE_START_BNO;
		args.fsbno = ap->rval;
		if ((error = xfs_alloc_vextent(&args)))
			return error;
	}
	if (args.fsbno == NULLFSBLOCK && nullfb) {
		args.fsbno = 0;
		args.type = XFS_ALLOCTYPE_FIRST_AG;
		args.total = ap->minlen;
		args.minleft = 0;
		if ((error = xfs_alloc_vextent(&args)))
			return error;
		ap->low = 1;
	}
	if (args.fsbno != NULLFSBLOCK) {
		ap->firstblock = ap->rval = args.fsbno;
		ASSERT(nullfb || fb_agno == args.agno ||
		       (ap->low && fb_agno < args.agno));
		ap->alen = args.len;
		ap->ip->i_d.di_nblocks += args.len;
		xfs_trans_log_inode(ap->tp, ap->ip, XFS_ILOG_CORE);
		if (ap->wasdel)
			ap->ip->i_delayed_blks -= args.len;
		/*
		 * Adjust the disk quota also. This was reserved
		 * earlier.
		 */
		xfs_trans_mod_dquot_byino(ap->tp, ap->ip,
			ap->wasdel ? XFS_TRANS_DQ_DELBCOUNT :
					XFS_TRANS_DQ_BCOUNT,
			(long) args.len);
	} else {
		ap->rval = NULLFSBLOCK;
		ap->alen = 0;
	}
	return 0;
}

/*
 * xfs_bmap_alloc is called by xfs_bmapi to allocate an extent for a file.
 * It figures out where to ask the underlying allocator to put the new extent.
 */
STATIC int
xfs_bmap_alloc(
	xfs_bmalloca_t	*ap)		/* bmap alloc argument struct */
{
	if (XFS_IS_REALTIME_INODE(ap->ip) && ap->userdata)
		return xfs_bmap_rtalloc(ap);
	return xfs_bmap_btalloc(ap);
}

/*
 * Transform a btree format file with only one leaf node, where the
 * extents list will fit in the inode, into an extents format file.
 * Since the file extents are already in-core, all we have to do is
 * give up the space for the btree root and pitch the leaf block.
 */
STATIC int				/* error */
xfs_bmap_btree_to_extents(
	xfs_trans_t		*tp,	/* transaction pointer */
	xfs_inode_t		*ip,	/* incore inode pointer */
	xfs_btree_cur_t		*cur,	/* btree cursor */
	int			*logflagsp, /* inode logging flags */
	int			whichfork)  /* data or attr fork */
{
	/* REFERENCED */
	struct xfs_btree_block	*cblock;/* child btree block */
	xfs_fsblock_t		cbno;	/* child block number */
	xfs_buf_t		*cbp;	/* child block's buffer */
	int			error;	/* error return value */
	xfs_ifork_t		*ifp;	/* inode fork data */
	xfs_mount_t		*mp;	/* mount point structure */
	__be64			*pp;	/* ptr to block address */
	struct xfs_btree_block	*rblock;/* root btree block */

	mp = ip->i_mount;
	ifp = XFS_IFORK_PTR(ip, whichfork);
	ASSERT(ifp->if_flags & XFS_IFEXTENTS);
	ASSERT(XFS_IFORK_FORMAT(ip, whichfork) == XFS_DINODE_FMT_BTREE);
	rblock = ifp->if_broot;
	ASSERT(be16_to_cpu(rblock->bb_level) == 1);
	ASSERT(be16_to_cpu(rblock->bb_numrecs) == 1);
	ASSERT(xfs_bmbt_maxrecs(mp, ifp->if_broot_bytes, 0) == 1);
	pp = XFS_BMAP_BROOT_PTR_ADDR(mp, rblock, 1, ifp->if_broot_bytes);
	cbno = be64_to_cpu(*pp);
	*logflagsp = 0;
#ifdef DEBUG
	if ((error = xfs_btree_check_lptr(cur, cbno, 1)))
		return error;
#endif
	if ((error = xfs_btree_read_bufl(mp, tp, cbno, 0, &cbp,
			XFS_BMAP_BTREE_REF)))
		return error;
	cblock = XFS_BUF_TO_BLOCK(cbp);
	if ((error = xfs_btree_check_block(cur, cblock, 0, cbp)))
		return error;
	xfs_bmap_add_free(cbno, 1, cur->bc_private.b.flist, mp);
	ip->i_d.di_nblocks--;
	xfs_trans_mod_dquot_byino(tp, ip, XFS_TRANS_DQ_BCOUNT, -1L);
	xfs_trans_binval(tp, cbp);
	if (cur->bc_bufs[0] == cbp)
		cur->bc_bufs[0] = NULL;
	xfs_iroot_realloc(ip, -1, whichfork);
	ASSERT(ifp->if_broot == NULL);
	ASSERT((ifp->if_flags & XFS_IFBROOT) == 0);
	XFS_IFORK_FMT_SET(ip, whichfork, XFS_DINODE_FMT_EXTENTS);
	*logflagsp = XFS_ILOG_CORE | xfs_ilog_fext(whichfork);
	return 0;
}

/*
 * Called by xfs_bmapi to update file extent records and the btree
 * after removing space (or undoing a delayed allocation).
 */
STATIC int				/* error */
xfs_bmap_del_extent(
	xfs_inode_t		*ip,	/* incore inode pointer */
	xfs_trans_t		*tp,	/* current transaction pointer */
	xfs_extnum_t		idx,	/* extent number to update/delete */
	xfs_bmap_free_t		*flist,	/* list of extents to be freed */
	xfs_btree_cur_t		*cur,	/* if null, not a btree */
	xfs_bmbt_irec_t		*del,	/* data to remove from extents */
	int			*logflagsp, /* inode logging flags */
	xfs_extdelta_t		*delta, /* Change made to incore extents */
	int			whichfork, /* data or attr fork */
	int			rsvd)	/* OK to allocate reserved blocks */
{
	xfs_filblks_t		da_new;	/* new delay-alloc indirect blocks */
	xfs_filblks_t		da_old;	/* old delay-alloc indirect blocks */
	xfs_fsblock_t		del_endblock=0;	/* first block past del */
	xfs_fileoff_t		del_endoff;	/* first offset past del */
	int			delay;	/* current block is delayed allocated */
	int			do_fx;	/* free extent at end of routine */
	xfs_bmbt_rec_host_t	*ep;	/* current extent entry pointer */
	int			error;	/* error return value */
	int			flags;	/* inode logging flags */
	xfs_bmbt_irec_t		got;	/* current extent entry */
	xfs_fileoff_t		got_endoff;	/* first offset past got */
	int			i;	/* temp state */
	xfs_ifork_t		*ifp;	/* inode fork pointer */
	xfs_mount_t		*mp;	/* mount structure */
	xfs_filblks_t		nblks;	/* quota/sb block count */
	xfs_bmbt_irec_t		new;	/* new record to be inserted */
	/* REFERENCED */
	uint			qfield;	/* quota field to update */
	xfs_filblks_t		temp;	/* for indirect length calculations */
	xfs_filblks_t		temp2;	/* for indirect length calculations */

	XFS_STATS_INC(xs_del_exlist);
	mp = ip->i_mount;
	ifp = XFS_IFORK_PTR(ip, whichfork);
	ASSERT((idx >= 0) && (idx < ifp->if_bytes /
		(uint)sizeof(xfs_bmbt_rec_t)));
	ASSERT(del->br_blockcount > 0);
	ep = xfs_iext_get_ext(ifp, idx);
	xfs_bmbt_get_all(ep, &got);
	ASSERT(got.br_startoff <= del->br_startoff);
	del_endoff = del->br_startoff + del->br_blockcount;
	got_endoff = got.br_startoff + got.br_blockcount;
	ASSERT(got_endoff >= del_endoff);
	delay = isnullstartblock(got.br_startblock);
	ASSERT(isnullstartblock(del->br_startblock) == delay);
	flags = 0;
	qfield = 0;
	error = 0;
	/*
	 * If deleting a real allocation, must free up the disk space.
	 */
	if (!delay) {
		flags = XFS_ILOG_CORE;
		/*
		 * Realtime allocation.  Free it and record di_nblocks update.
		 */
		if (whichfork == XFS_DATA_FORK && XFS_IS_REALTIME_INODE(ip)) {
			xfs_fsblock_t	bno;
			xfs_filblks_t	len;

			ASSERT(do_mod(del->br_blockcount,
				      mp->m_sb.sb_rextsize) == 0);
			ASSERT(do_mod(del->br_startblock,
				      mp->m_sb.sb_rextsize) == 0);
			bno = del->br_startblock;
			len = del->br_blockcount;
			do_div(bno, mp->m_sb.sb_rextsize);
			do_div(len, mp->m_sb.sb_rextsize);
			if ((error = xfs_rtfree_extent(ip->i_transp, bno,
					(xfs_extlen_t)len)))
				goto done;
			do_fx = 0;
			nblks = len * mp->m_sb.sb_rextsize;
			qfield = XFS_TRANS_DQ_RTBCOUNT;
		}
		/*
		 * Ordinary allocation.
		 */
		else {
			do_fx = 1;
			nblks = del->br_blockcount;
			qfield = XFS_TRANS_DQ_BCOUNT;
		}
		/*
		 * Set up del_endblock and cur for later.
		 */
		del_endblock = del->br_startblock + del->br_blockcount;
		if (cur) {
			if ((error = xfs_bmbt_lookup_eq(cur, got.br_startoff,
					got.br_startblock, got.br_blockcount,
					&i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
		}
		da_old = da_new = 0;
	} else {
		da_old = startblockval(got.br_startblock);
		da_new = 0;
		nblks = 0;
		do_fx = 0;
	}
	/*
	 * Set flag value to use in switch statement.
	 * Left-contig is 2, right-contig is 1.
	 */
	switch (((got.br_startoff == del->br_startoff) << 1) |
		(got_endoff == del_endoff)) {
	case 3:
		/*
		 * Matches the whole extent.  Delete the entry.
		 */
		XFS_BMAP_TRACE_DELETE("3", ip, idx, 1, whichfork);
		xfs_iext_remove(ifp, idx, 1);
		ifp->if_lastex = idx;
		if (delay)
			break;
		XFS_IFORK_NEXT_SET(ip, whichfork,
			XFS_IFORK_NEXTENTS(ip, whichfork) - 1);
		flags |= XFS_ILOG_CORE;
		if (!cur) {
			flags |= xfs_ilog_fext(whichfork);
			break;
		}
		if ((error = xfs_btree_delete(cur, &i)))
			goto done;
		XFS_WANT_CORRUPTED_GOTO(i == 1, done);
		break;

	case 2:
		/*
		 * Deleting the first part of the extent.
		 */
		XFS_BMAP_TRACE_PRE_UPDATE("2", ip, idx, whichfork);
		xfs_bmbt_set_startoff(ep, del_endoff);
		temp = got.br_blockcount - del->br_blockcount;
		xfs_bmbt_set_blockcount(ep, temp);
		ifp->if_lastex = idx;
		if (delay) {
			temp = XFS_FILBLKS_MIN(xfs_bmap_worst_indlen(ip, temp),
				da_old);
			xfs_bmbt_set_startblock(ep, nullstartblock((int)temp));
			XFS_BMAP_TRACE_POST_UPDATE("2", ip, idx,
				whichfork);
			da_new = temp;
			break;
		}
		xfs_bmbt_set_startblock(ep, del_endblock);
		XFS_BMAP_TRACE_POST_UPDATE("2", ip, idx, whichfork);
		if (!cur) {
			flags |= xfs_ilog_fext(whichfork);
			break;
		}
		if ((error = xfs_bmbt_update(cur, del_endoff, del_endblock,
				got.br_blockcount - del->br_blockcount,
				got.br_state)))
			goto done;
		break;

	case 1:
		/*
		 * Deleting the last part of the extent.
		 */
		temp = got.br_blockcount - del->br_blockcount;
		XFS_BMAP_TRACE_PRE_UPDATE("1", ip, idx, whichfork);
		xfs_bmbt_set_blockcount(ep, temp);
		ifp->if_lastex = idx;
		if (delay) {
			temp = XFS_FILBLKS_MIN(xfs_bmap_worst_indlen(ip, temp),
				da_old);
			xfs_bmbt_set_startblock(ep, nullstartblock((int)temp));
			XFS_BMAP_TRACE_POST_UPDATE("1", ip, idx,
				whichfork);Copyda_new = temp000-2break000-}
		XFS_BMAP_TRACE_POST_UPDATE("1", ip/*
 *  ight (c) 2000-if (!cur) {Copyflags |= xfs_ilog_fext(ight (c) 2000-2cs, Inc.
 * A; yo(error  it anbmbt_update(cur, got.br_startoff Copyr* published blockthe Free SoftFoundcount - del-> * This progrthe Free Softwarte)))Copygoto done000-cs, Inc
	case 0:
		/*
		 * Deleting the middle ofen thextent.TY; w/
		on G = is distrshed by  - * published by 000-ll Rights ReservRE
 *
 * Th0s program is free softwarel Public set This progr(ep,con G2000-newublished by BILITY_end PURPOSon G2 = * peral Puam is eral Publica copy This prograicon G2ved a copy of tense
 *e Foundatware; youdelayn redisa copy of thFoundNU GeneralFound000-2tribute ill RILOG_CORE000-2; yo can redisthe GNU General Public License as
CopyrFree Software by the FrFree Software Foundacon Gfs_bit.h"
#include would bebe useful,
 * b#include "xfs_types.tree_incremente as
 0, &i"
#include "xfs_sb.h"
#icur->bc_rec.b = new.h"
#ie "xfs_ag.h"
#includser_dir2.h&i2000-2de "xU Gene&& U Gene!= ENOSPC#include "xfs_sb.h"
#iANTY;Y; wiIf get no-space back from 
#inc loc_btfs_bit * it tried a split, and we have a zeroude "xfson, MAreservationf
 *de "xFix up our undatide "return warra Genclude "xERCHde "xfs_iall="
#includs.h"
#include "e "xReseten thcursor,ful,'s_inusta.h"
#incit after anye.h"
#i oper"
#include e "xfs_itthe GNU General Public lookup_eqe "xfs_bit.hh"
#include "xfs_log.h"
ree Software Foundation.	ic Lic
#include "xfde "xfs_sb.h"
#iSE.  WANTh"
#RUPTED_GOTO(iincl1eaf.heincludedata.h"
#incUicensen thdinoderecordcludea.h"
#inctoen thoriginal valueode_item.h"
#include "xfs_extfree_iteminclude "xfs_bit.hoc.h"
#include "xfs_bmap.h"
#include "xfs_rtalloc.h
 *
 * This progrdeops.h"


#ifdef D.h"
#includrror.h"
#include ata.h"
#include "xfs_anty oxfs_trans_space.h"
#include "xfs_buf_item.h"
#include "xfsails.
 *
 * You should havelog.h"
#includThis progr"
#inclutribut= 0
#incluU Generall RERROR(#includ
#inclue "xfs_sb.h"
#i * Ae "xfs_attr_leaf.h"
#include "xfs_rw.h"
#inc} elsedd_attribute it and/or
 * modify it under tde "xFORK_NEXT_SET(rogright (c) m xfs_ inode pointerENTS	xfs_fsblock_t) + 1		/* nsacti redisASSERTodify it uinclll RDATA_e po		/* eANTABILl Publap_worst_indlen	xfs_received nes.
 */


/*
 Boston, Mhave nulld from xfs_(int)receimmit */
	iense		*flags);	/* inode logging fl2		/* e Floor, Boston, MA  _add_attrfork to handle or */
x006 Silicon G + Free Softrighle (006 Sil> da_oldfs.h"
#inclureceis_dir2_don G--
#inclu006 Siblock_t		

/*
 * Called from xfs_bmatalloc._add_attrfork to handle local f_add_at; yo006 Sili=		/* inc#inclucs, Inc.
node point2er */
	xfs_fs2block_t		*firstblock,	/* Floor, Boston, MA xfs_bmap_free_t		*flist,		/* blor */
x_add_a * A * All Rights Reserved.
 *
 * ThPublic License for more detll Rights ReserINflist
 */
STATIC  */
,		*i&new, NULL	xfs_e for more detailsiex inoc_btrif_inode_t		*ip,	/* itware; p->if_lastex =node_t		 * but WITH(or ANTYxfs_atwe need to, adull, listied anty osa btdhout.h"
 "xfst como_fxflag		*flags)add_free(ITY or FITNESs_inum.is distributed in t free 	xfs_ceived/* if *Adjust inode # "xfs_s ialloc.fil	*new,	/* newnblksflagi/ins_d.di_nee_t		*-= ts to variable */
	xfsquota data*new,	/* newqfie	*flagt antrans_mod_d fla_byino(tograp, a_t		*, (long)-ts to THOUriable *cprografor changde.h		*fayed indirectree_t		*new, Noth evenefulfork diskg flagsa attr  eveher	*new,	/*	*flist	/* in >	int	 to updt commicasesconvertielta, /ge mincore_sb(#incll RSBS_FDBLOCKS, o ha64_t)dle case-converti	xfs_rsvding a deleltaer */
/* DELTA: repor "xfs_"xfs_buf_anty of "xfs_real(
	xf->xedFITNESS FO> * published by flagsm_t		idx,	/* extent on,
 * Inc.,AR PURPOSxtnum_t		idx,	/This progra< * published by +ap_add_attrfork tonsert */
	xfs_bt, write the * published by  +leaf_extents(xfs_btree_cucurp,rw.h:
	*logtribuABILtribu;
	xfs_ialh"
#i;
}

/*
inclumov
#inclentry "xten" "xfs_st,	/node.temtree .  Prev poibmbt_irther topreviousk vari, unlessable */
is_bmapheadied warrflist
 "xfSTATIC void
		*flags)h thxtent
 add to fi_fre_t		*to firde_t_free_t		*flisode ler "xfs made to incor_t		_t	*be f */
	be freed _t		*onflags, ifk.h"
allocate reserved blocks *_fre)de_tree *_t		*to bap_fred to {g a debe f to be f->xbfi_nexthe _fre	/* error *;
	action to fi	/* e_firs*/
xfs_bmap_add_extentelay(
	xfs_progrblockkmem_zone* Chancate reserved block_t		,xfs_b) pointer toConvcludan	xfs_bmb-format	/* l into alude "	/* new data */
 Th is w data wills_btree.roo alloca		/*_bmap_bmap)ude "a single child "xfs_ */
	xfs_extdint
#endif_btree.*/_t		*deltaxfs_bmb_to"
#incnge mad* Chane extp,ode_t* Chaac
#inist ofto allocate_bmapocks i/

/*
 al alls_bmap_xfs_bmap_add_extfsFoundre extnodeFounda*/
	inode-Found-allocatcatione made to incore extents *nts ee_t		*llocatin xled by _bmap_add#inclcurocks *cur/

/*
 dir2_l"xfs_ianull, callto allore extwasdelm_t		idmbt_ir evea	rsvd);	/ror *pdate/insercks */
	xfATIC _bmap_logg evetributdate/insertght (c) 2nts  */
 or attrforkktion.
 structag.h"
#inclFound	*a301  Unts ror */
xfs(, /* ) bing flag_bmap_addufocks abp;	xfs_iuffeint		 ags */ange maderor *_argocksargsextentror */
 by arguxfs_ new dl Public rec incorr extent, /* Cfs_tranxfs_bmap_addgsp, /* inode logging flgs */
	xfs_ide "xfgging flaginode pointer */
	xfs_curation omap or vicdx,	/* _bmap_add_tent tohostks *e extent_bmap_free_itexfs_bmap_addre extt,	/* ents */
	inxfs_ialitem.unwrittenextnucks 	i, cnt		*ip,	/* incore inoindexp_add_extet			o handf extent_bmap_t			*xfs_bmap_add_ext_realkeyocks k extente versa.
 *keyfs_bmbt_irec_t		*mrogrocks m extentogfla gsp, /urmber to update/inseror *xfs_
	xfsnumberied data xfs_bmbtrec_t		*new,	pt
	xfs_pta to add to file addr*logxfs_bmap_adg a ABIL inode poiPTRblock allocated;t to hand inode poiFORMA
	xfs_fsblock_t)cks to frINODE_FMT_ first  figures oute/insertnummax ==
	 r */
x inode poiSIZEhe underlying al/ (u hansizeof/* extetent to local/* if *Make "
#inc */
	xfs_extes conv*new,	/*t ande ve_reror *	xfs_1is free softwarte/inser*/
#include "xFBROOT, /* data oF,/*  */
	xfe ve*new,	/*on, MA  te/inserbe ve;.
 * Si->bb_magic = cpu alloe32ut whightsMAGI			/*s are alreleveln-core, all 16(
	xfss are alrenumrecentsor the btree root and pitchu.l.the sftsi_bmaore, all 64(ore DFSBNOATIC int				/* error righxfs_bmap_btree_to_extents(
	xfs_ruct */

Nocataextent_.  Cah"
#ror */
x fs_bl the spaceisde tlip,	/*new,	/*TABILfreed ogfla;
#inceral Public init */
sortion.e extentight (c) 2000include private.b.on.
 */
ST = ion.
 */
STata or attr fork */

/ree *_fsbissp, /* pdate file extentxtentst */
	 ?fs_bmBTCUR_BPRV_WASDEL :s for/* if *bmbt_ire add to fi with two  spacs, on"xfs_traninformat file.
t where to Mr */
	xfs_fsblock_t	cator to put theBTREE fig for.tABILtGraprrentTABIL Graprrent/*
 * Called by xfs_bmapi t; yoion.
 */
ST/
	iore FSC inter */
rrent yptionll RALLOCTYPE_START_BNO000-tnum_tsbnote an exNO_TO_FSBtion.freed ino figbmap_fr; yoelay(
	xfs_lowee_t		*flist,	/* list of extents to be freed */
	xfs_btreeby xfs_bmapi tbmap_free_t*flist,	/* list of extentsNEARtents */
	int			*logflagsp,/* inode ointer *inlen =ed bs.maxhichfork, /prod =e_currrent otace f	int			whifecorrk, /alignxfs_;	 /* OK ork *rk, /isfl reco	int			wservesloABIL forrk, / space ng space;locks */
	xfs_f forhe GNU General Pu data v*delta(&rk, ))er */
th only one leaf node,- where the
 * exde pointer h thgs */
	 as
 r undoREESTATIC		/* 		*first,	/* prp,	/* if *A*
 * Callecer */fail,_bmap"
#incwasdmapi.hed_extent to hand*/
	xfs_btr!
	xfs_bmap_frefigures ouate/insert */
	xfs_bmap_fr ||ror */
xfte resgbtre list FSB	*cuAGNOtion.ion.
 */
ST)an extents-fxfs_bmbt_irec_t && entry " file>nto a btree-format file.
 * The nlocalate/insert */exte attr fork */

/*
 * Called */
	xfs_btata or attr fork */

ror */
xf++evioreed */
	int			*lintera, /* Change made to incore extentll RTRANS_DQ_BCOUNT, 1L* curbint			*fl#inclg* Youfl	int			wh*/
	xfs_bt, 0struct */

an extents f, /* Change mew,	/*	whichf list BUF	*cuC int(abeivedags */ already in-core, all we have to do is
 * gurned to ca space ffree  int				/* error */
xfs_bmap_btree_to_extents(
	xfs_t			*logflagsp,	/* ransaction pointer */
	xfs_inode_t			arate an eBMBT_REC_ADDRtion.			*lo,/
	xfs	*delta)nce the fileytes		*ap);	/* bmap alloc argument  inoor (cblocki*/
	i i < 	*delta); i++er */
eint			*fxtnumated* mo,	/* etware; youis_add_attrfork tt		*new,	atedd from xfs_bm of the ile ->l0bmap_btree_to_eet	*tp		/* es_t	*t1,		/* transaction po
	xfs_*/
	++;*/
	inter */
}t to handon't e an extent * first block allocatedd solock-alloc * Y the le(is code c to hde, into an extents forma extede "xfs_bma
	xfs_btk to an extentKEYile.
 * Ths code is sole to an extents file.
 * This code is sokp or FITNESS FOmap_btree_to_et		*new,	
 * local_to_eoff(ar localp to an extentPTRing flags */
	int	,al Public atedmaxen_t	 "xfs_bit.hbtre allcpu( delayed alloc STATIC no.
 p_btree_to_e*/
	xfs_bttotal blocksDo/
	x thisc_t		*newa		*xfs_nd stentat if *by transaise, *lastransa
xfs_b file with o-alloc/or
m xfs_ as
 abs_fsbloBB of _BIextenttp will contai.  If bnoy.
 */1, ast eof,
 * 			*logflag the len xacem to bextnu*/
	xfs_ATIC fs_inoexteints to the
 * pde "xfs.h"
#i |it and/or
 e extodify it underxfs_ial0rt */
	xfs_balculh"
#incldefault*/
	iibut */
	xfoffde " it newly cre/
xfs_bmaps */
	xp);	_t		* or att_/
	i*eofp,(n
 * allocati_bmapandl)gflagsp, /* inoogfla, /*nt			*logflagsp, p);				*eofp,otal; yomp->m_sb.sbnt ind* bmrstb256er */
*eofp,	*/
	xfLITIrmat ) -dd_attrfoBMDR_SPeserCALC(MINABTPTRxtentbmap_free_tus extent enteck the last ino6/

/e extent to de
xtents(
	*eofp,	<t entry found *rched for *ound */
ointer toHelper routineC inmapifs_bmap_di_t					*d_t		* when switcd bl
	in fork */
	int		"xfs_r */lC in_bmap_f/* new -rp ihe end tnot rs to bossibl at tm*
 * Transavailat bo it inlks axtentt			**delta) */
	xfs_extdelta_t		*deltaile whi_he ennge madogflagsp*
#inc_extent to hndle */
	xf add to fin.
 */
S,		/* blocks to fATTRat coock (r */ */
	xfs_in/* new !ocator to put theDEVrk,	/* data or attribute fork */
	char		*aeofUUIDrk,	/* data or attribute fork */
	char		*aeoftp,	/* redientrydflfile whicfiles.um_t	*lastxp,	/* ouip) >> 3otalrp is TATIC void
>ata or attribut whier */
	name */
	char		*deBILITATIC voidncludereed *f. int				/* edd_attrfode poiDoc(
	xft		*needs to get
 * logged soe_t	*ip,aATIC int				/* ede pointer */
	Afs_extnum_t	idx,		/* index of entry(efs_fs */
	xfs_bmbt_irec * we wdata  add_t		*new,nts */
	int	is cree fs ou */
	bounds/

/*
 */
xfs_sied regular attrs,
	insinc
#incldata  xfs_s nubt_irtr_s_t		e will bingssp,/* staytreesisty of
 * (t			_bma- spacemanipk,	/ions are oum.hhough) made to incore extets */
	int			rsvd);	/* we  all*delta) reserved block */

/*
 * Called by xfs_bmap_add_extent to hndle cases converting a hole
 * to a real allocaion.
 */
STATIC int		s calledor */
xfs	/* incore inode poextlen_t	rsvd)

/*
 *svd);ee_t		*s nued by * Called by er */
	xfree */
	xfs_bmbt_irec_t		*new,	/* new data t add to file extents */
	int			*logflantry _extnum_t		idx,	/* extent number tntry sblockdesc,_t		*new,	/* nextent nu* if *curp is nul, not a btree */
	xfs_bmbt_irection).
 *Weaf.h"
#wanIC indeal error  in  ANYof keep eve of thetent/* erroyeat fil So send even th xfs_iextratiaert, or
 btree f	*flvaliy */
	xfs_bmap_f!(( */
	xfs_inmree & ter MTallocter REGock (ifork); /* cks to free at com* extente an extent for a file.
 * It figures out where to ask the underlying allocator to put theLOCA*ip,	 extents forU Generarevious , since the ff the list data or att fork /

/*
 * Called by xfs_bmap_ made to in*e exents */
	int		_bmap_fwhichfork)itten_real(
	xfs_ino e_t	ip,	/* incore inode pointer  flags */trans poiinter */
	x		*logflagsp, xtnum_t		idx,	/* extent number totents(
	
	char		*removik (i	ut wherINLINE|inter  first _BMAP_TRACIREC)allocatorfine	XFS		/* ANTY; witem_t	*ree.hange  nctiknowrp is nulonlyp_de, in the exTY; widata currently fit	*fliannum_t	f
 * MERCH update/insert */
	xfs_bmap_free_t		 */
	xfs_btree_cur_t		*cur,	/nter */if null, not a 		*flist,	/* list of extents to be freed bmap_free_t	
	int			*logflagsp,/* inodeflags */
	xfs_extdelta_t		*delta, /* Cha * Ant			rsvd);	 * ins/

#define/*
 * Remo to allocate reserved blocks */
 list.  efine	XFSve the /* OK toe" from the free
	int			whichfork, /* data or attr fork */
	iious entry, unless "free" is the head ofd be useful,
 * bANTY; witer */revious item on list, if any */e "xfs__bmap_free_item_t	*free);	/* list itrst_indlen(
	hichf=/
	xfs_ int					/* ere(
	xfs_trans	rst-block-allocated */
	)	\
	xfs_bmap_free_t		*flist,		memcpy((char *)		**curpfor bp)d_ex/inseru1/* i */
	xfs_	char		*desc,	ree(
	xf* Channtainufre ex
 */0being retuthe fi-/
	xfs_*ip,		/* incore inode p)	\
	xfs_bms_bmap_del_free(
	xfi */

STATIC void
xt(
	xfs_fileos,
	xfs_bmbt_irec_t		tnumaddh.
 */0e is soulations are ok, though.
 */inter ails.
 *
 * Yallfhave 0ap_free_t		*fl1_fsbloter NORMrror */
xfs_bmap_addved.
 *
 * Thnews progr0int				/* error */
xde pointer */
	xfs_fsblock_t	/
	xfs_freed */
	int			*loindireca, /* Change made to incore extencore inock_t		*firstblock,	/* fointer */
	xfs_inode_t		*ip,		/* incobmap_free_tures out where to* first block allocated ==map)
#endif //* incore inode p		*logflagsflags,
	xfs_bmbt_ir}extents list wil&= ~w)	\
	xfs_bm extents list will fit in  first ;t		*ip,	/* incore inode pointer */
	xfs_trans_t		* new extent*/
#include "xfs.h"
#incect blocks */
	xfs_fsblock_t		*first,	/* pointer toSearcc,		/*_bmap_free_itall tolock variacontain evesert *bnobmap_If Bma liexfs_bm hole,/
	intinclude or */ vari. ternal routi to bast eof, *eofpsp,/* be seclude "*/

/_btree_	*counting flastmap_ varia(_add*/
	nw.h".  Else, *fsblx_btree_cur	*cinclude urp,	map_ed warrf a c_t		bn; *gotleoff_t		off,
	xfs_or */
*/
	xfs_extd of entry to be updatdesc,xfs_bmapt*/

c_recbmap_f variat			rsvd);	/see_bl_multi */
	xfs_inode_ap_trace_pose cases ee */
	xfs_bmbt_irec_t		*/* lre it			*f	xfs_inode Change kup(cuedt				ec_t	*r2,	xfs_ct xfsout:txp wmade to nt = ler to update/inse	int			ATIC lock_fsbllen;
	reurp,	/* if *cuoggingt to hstartfs_fsblock_tn;
	return x_filblks_t		lec.b.br_startos_fil)s_fsblock_be freed *bc_rec.b.br_startbloc{ritten_real(
	xfs_inoated 	*ip,	/* incore inode pointer *o update/inseint		cord in/failure */
{
	cur->,	/* if *cnitial;	/*ck	*block,
 variang flags *mber tch by *logto if *ung fl, statd or, Boston, MA_t		*
	xfs_bttartrch the extents 0xffa5_cur	*cur,
	LL;_update(
	st, write the 0xa5cur	*cur,
	xfas_fileoff_t		oundationCE)
STATINVALID;
#if,	/* fIG_BLKNOS_update(
	struct Called treeee_cur	*cur,
	s_fi#action	rec;

	xfs_bmbt_disk_set_allf(&off,ndif
nt			te(
	struct xfs_bxfs_bILEOFFotallations are okbnoion */
h.
 */		*fl&int		ing a deint		 > 0f the liste next enalle inore ok, though.
 */
STATIf_t	,ount eived,
	in*/
STATI</
	char		*desc,		*ap);	/* bmap alloc argument snt					/* error */
xfs_ave tart	xfs_fxfs_bn namebmap_free_t
 */
STATIC int				toff =ogfls_t		nc.
 * Ablock,	/_cur_latioore tp,		/	int			*=ee(
	xk_t		*firstpruct xfs_btree_block	*block,s * to umrecs,
			/*,	*cur;		/;
	retu	*count);

/map internal routines.
 */

STATIC int				/* error */
xfs_bmbt_lookeq(
	strucmap_ xfs_btree_cur	*cur,
	xfs_fileoff_t		off,
	xfs_fsbl_t		bno,
	xfs_filblks_map_	len,
	int			*stat)	/* success/failure
{
	cur->bc_rock_t		bnr_startoff = off;
	cur->bc_rec.b.br_startblock = bno;
	cur->bc_rr */
xfrivate.b.f.b.br_blockcount = len;
	return xfs_btree_lookup(curLOOKUP_EQ, statnt to rivatndleprivate.b.flists converting a hole
 * to a reup_ge(
	  ee forrivate.b.flist_btree_cur	*cur,
	xfs_fileoff_t		_private.b.fl		flagr0);
le extents */
	int			*logD_GOTO(stat == 1,f,
	xfivate.b.flistlock_t		bno,
	xfs_filblks_t		len,
	int	 flag*stat)	t)))
			goto erre value given
 * by [endif /* Dr_star_startstat)))
			goto errobc_rec.b.br_startblock = bno;
	cur->bt		*fli)at)))
			goto errunt = len;
	return xfs_btree_lookup(cuap_trace_post_update(
	const char	*fnameup(cur, XFS_LOOKUP_GEated be at least one	int		whichfork);	/* data o
 */
TATur_tC(x
xfsok_exs anentry prior to updating an e(c) 200bmap_add_ate_lookup(cur, XFS_LOOKUP_E btree fors, &stl_cursor
		}
	s_t		*tp	xfs_bunlikely(!(	rec;

	xfs_bmbt_di)loc_(	int			**free);EXTNUMode__bmaer *!ut wheS_REALTIMdd_eODextnum&&
xfs_b* Add bmap trace ef the listcmn_errut whPTAGa bt int_ZERO, CE_ALERTif null,	int  ode l"A gets ane entryh"
#rted			/* %llu "int			,	/* off,
	:gs *xSTATrt_off		*cur;{
	xfs_blkcnt		*cur;r->bc_-ode_t: %xents(
r re\n"	int	(unsigt nu			w.di_n) null, no

	if (ip->i_d.di_nextent	rec;

	xfs_bmbt_dif(xfs_bmbt_rec_t) <= XFS_IFORK_DSIZE(ipby the F_bmbt_rec_t) <= XFS_IFORK_DSIs_btree_cur_t *pdate(
	strutn,
	int				xfs_fint			*flncore inodirstblock,	/_cur_xfs_ialcommit */
 inode logging
	undenion xfghts Rese
ktracidx,	int					/REE_ERbuf;inter toAdre i_bmapREE_Ets */
	ior */
xfB ANYblocks aumrecs,
otheraeof(
	xfs_inode_t	*ip,		/* REE_ERadd vari(an exteope enfs_fsbl "xfs_inlks_t voit */
r	*fnamr */
xffuned by 	xfsd_attrocal	*desc */
xfs_bmap_addpoinripcore inode pom_t	idx,		/* index of entry(entries) inserted date/inse
 * t a btr= XFS_IIC intic,		stblock,	/* firstcn(
	xfs_prograd */
	xit_nm1ts *2unwritten_real(
	xfs_ino *r1		/* count fs_trannt			*flags)		/* inode log2ATIC secoe "xf_tran/* o	xfse_new_iichfork);	/* data or attr fork */

/*
 *	*flags)		/* inode ltr2tp,	lock_t	*firstb1 ||	/* * outt item to br1/* incoring a der */

	iemove.ytes <= 2node_t	*ip,		(ip- &ructuvalidde p_IFM	xfs_* bmap tr2 entrnsaction ;
	if ((ipS_IFORK_DSITREE_ERe_bma/* extent REE_NOERR	int(elta *)(__psilags)( error | */
	int    << 16)_exterstblock
	xfs_ rstblockpoint>m_dirblk, flagrstblock = firstblo
 * Coprstblock = firstblo	/* dargs.trans = tp(ip->i_dbloc null, not cha2		dargs.totalsf_to_block(&darg(ip->i_dts * sizeof(xffs_dir2_sf_to_block(&dargr1	*tp,e
		error = xfs_bmap_local_to_extents(tp, ipORK);
	rror = xfs_bmap_local_to_extenRK);
1return error;
}

/*
 * Called by xfs_bmapi to updat1 file extent records and the bt2);
	return error;
}

/*
 * Called by xfs_bmapi to ur */
ion).
 */
STATIC int				/* error */ * after allocating space (or doing a delayed aldx,	/ deltent.
 */
STreed xREE_Eof(dargs));
		dar *curp is nus.firstblock = firstblock;
		dargs.flist = flist;
		dargs.total = mp->m_dirblkfsbs;
		dargs.whichfork = XFS_DATA_FORK;
		dargs.trans = tp;
		error = xfs_dir2_sf_to_block(&dargs);
	} else
		error = xfs_bmap_local_to_extents(tp, ip, firstblock, 1, flags,
			XFS_DATA_FORK);
	return error;
}

/*
 * Called by xfs_bmapi to update file extent records and the btree
 * after allocating space (or doing a delayed allocation).
 */
STATIC int				/* error */
xfs_bmap_add_extent(
	xfs_inode_t		*ip,	/* incore inode pointer */
	xfs_extnum_t		idx,	/* extent number to update/insert */
	xfs_btree_cur_t		**curointer toretu error;
}

 variaprimrecadd r tot			bmap_add_rfirst handle local format files.
 */
Sec_t		(ttrfork_local(
	xfs_trans_t		*tp,		/* transaction pointer */
	xfs_inode_t		*ip,		/* incore inode pointer */
	xfs_fsblock_t		*firstblock,	/* first block allocated */
	xfs at com)rec_t		blks_t		len,
	int			/* blocks to free at comdx <= nemit */
	int		rror;		/* error return value */
	xfs_mount_t		*ap_trace_post_update(
	const char	*fname,	y prior to updating an extent record at files.
 */
STATIC intll RightsK ReserDELETE,  = mp->fsbs;
rogram i
	= da_ons are ok, though.
 */
dx_extep->i_df. ?pty", ip, 0, 1, new, NULL, */
	 :core inodight (c) 200k_t		*ifp;	/* inode fork ptr */
	int			logflags; /* return.h"
#in ormap_reancore	cur->bctree_cur_t		*xfs_bmap
 * Cs */
	xf-alloon name */
	ch format files.
 */
S_t		idx of extents in file now */

	XFS_STATS_INC(xs_add_exlist);
	cur = *curp;
	ifp = XFS_IFORK_PTR(ip, whichfork);
	nextents = ifp->if_bytes / (uint)sizeof(xfs_bmbt_rec_t);
	ASSERT(id_t		idnextents);
	da_old = da_new = 0;
	error = 0;
	ta->xed_mit */
	int			*flags)	r_startoggin a btr->xed_btr code1	delta->xed_blockcount 2 new->br_startoff +
		2e */
	int			error;		/* error return value */
	xfs_mount_t		*mp;		/* mount stru1er */compy xfrtoff +
						new->br_blo/* mount structrtblock)) {
		if (cur)2s_filord 1 y [ofndif /* DEBUG */DIR)1, r
	xfsZE(ip))
		rlags */t(&dargs, 0, sizeof(d0);
		if ((error = xf2, r.if_b
	xfs_mount_t     p))
		retincore inod(ip->i_d.di_mo}nts in the list.
	 */
	if (nextents == 0) _extenS_BMAP_TRACE_INSERT("insert emp xfs_bhichfork);
svd)ew);
		Afork); /* datk_t		*ifp;	/* inode fork ptr *_blockLicenElse,_t		*newl_extent(
	placlue */
	xfs_extnum_t		nextents; /*ps_inLicensettrfork_local(
	xfs_trans_t		*tp,		/* transaction pointer */
	xfs_inode_t		*ip,		/* incore inode pointer */
	xfs_fsblock_t		*firstblock,	/* first block allocated */
	xfnt_holnextentirst extent added to a new/empty file.
	 * Special case this one, so other routines get to assume there are
	 * already extents in the list.
	 */
	if (nextents == 0) ved.
 *S_BMAP_TRACE_INSERT("insert to th ip, 0, 1, new, NULL,
	R_BPRV_WASDEL) == 0);
		if ((error = xfs_bmap_add/
	int			t_hole_real(ip, idx, cur, new,
				&logflags, delta, whichfork)))
		reto done;
	} else {
		xfs_bmbt_irec_t	prev;	/* old extent at offset idx */

		/*
		 * Get the record referred to by idx.
		 */
		xfs_bmbt_get_all(xfs_iext_get_ext(ifp, idx), &preayed av);
		/*
		 * If it's a real allocation record, and the new allocation ends
		 * after the start of the referred to record, then we're filling
		 * in a delayed or unwritten allocNU GeS_BMAP_TRACE_INSERT("in 1nsererting real back to unwritten.
		 */
		if (!isnudate(cr */ll Rights Rese) == 
	xfs_bmmp*/
			/*;	/* - operChange mad* OK to allocatl be btree_curuseOT;
					/p'
	/*
d);	/r->bc_pof length "len".b.br_startblock fils to_s_extnlags);	/* inode loeoff_t   off,		/*  cases converting a hole
 * to a re_EXT_UNW	len.br_blone;
			} else 			ASSE
/*
 * Add  spacation or vic spaceChange 			error;try.  I.flagmaximumoff +
		progra, *lae).
spaceint			*logflagsp /* inode logging flags */
	xfs_e &cur, newrvaASSERT(* extent number /
	int			*logflagsp, try.  I
	xfst_irdelta,mxr[0] so it wlloc */
	, ERT(,	/* fier */ in a  the eBM_MAXLEVELStion.
 */ree at commite_real(ip,l manipuhich+=*
		e lea-.allocdo_div(len,;
		}
	}i_modeexte+=				irect blnter */
	xtentfs_ial== NULdx, cur,
					new, &logflags, delta,/

/*
  in a 
	ASSER * Conpace rk_tstbloRV_WASDEL) == 0);
			if (1erros_btree_deERT((0);
	unidefined (nexRWs Rese)xfs_extdelta_t		*dun = ip;
		r = xfs_bmap_ad		/* fi_lookup_ge(
	sstruc, idx, &cur, newto d file ofTATIC  file_inod		*rasblocks * *curprwr;
}

ode_t	*ilocated =l, not a btree */
	xfs, &cur,s.firstblock = firstblo		**cuNMAP = xfs_dir2_whichfork = XFS_DATA_FORKated */
	int	);	/*e
		er &_set_alrectrror = xfs_bmap_lorstblocanges in reserveed indirect blocks.
	 * Nothing to do(e inodfilge(
	)al rd delayed indirect blocks.
	 * Nothing to do	xfs_filblks_t	nblked indirect blocks.
	 * Nothing to dents_trgs.trans = tp;
		ere(ip->i_, 1, flags(ip->i_d.di_n)d,ip,i,,
 * 		dargs.totalrrom xrstblock04_t)(da_old - nblks), rsvd);
	}
	/*
	 * Clear out the allocated fXT_NORM);
	*/
	xfs_bmbt_ire			/* exfs_non- fork */
numbecur->bc_prbytesM	xfsngingnsfora * Called byINSE m0;
		*curp oundcated /
re extenesc,		/* oe entt			rsvd);	/le e/
	iar		turn val */

		ASSER cases converting a hole
 * tontry f* bmsp = l"
#inc*log fork */
	from t			error;nt_del_chec inc maylta)t, if any s to_mount_t		*eal allocaty xfs_bmapir */1nt of en/agtries inser_bmap_add_extent_holeds andtrans_locatblock,
	int			nint			*logflagsp, /* inode logging flags */
	xfs_eed blocks */xfs_in* Called by xfs_bmap_addntry fgsp, led by xfs_mapi.h"
#inhandle caseversnot b.aled byuper called
	in
	xfs_bmandle casecommittedirec_ incore liste extents add to filks */
	xirec__t		*new,	/* new data to_extnum_t		idx,	/* extent number igures out where toQxtnumork_t	*i,	/* if *curp		/* incore inorror */
xfs_bmap_allxfs_extnum_tap);	/* bmap alloc argument str/
	int			*logflagsp, ntry updll RNOT_DQATTACHED/* if n entrree_tt number tleaf nion.
 */ck_t		ADDAt committion  list oed datathe lasREw, &p_add_ext_dellic p->tist will fit ick_t		RESERVs_bmap GNU General Pu* Chan, if an_validlks_fsbloon */
{
	fs.hree_cur	xfslags, ra)
#endPERMr idx */xtent entry for idxstblo's del usefh"
#i fort and/ock	xfs_de "xfsCK_EXCK_DSI */
	xfs_bmbt_rec_host_t	_ flagint	ksre extent/* extadd_svd ?core inoQMOPnts ff;	GBLKS |node _irec_FOR_btree WARR_bmbt_irec_t		r[3];	/*p_add_exh"
#if the list.unfp;	/* inode fork pointer *e	xfs_bunmacancelre exint			diff;	LEASEr idx */ist header */
	xfs_bmapvaluvariable */
	xfate */
	xfs_if_cur,
			flist*/
	iabute fork */
	char		*aeof new ext_inode_TY; wiF			/*evp)_t		*new,xfs_pre-6.2 * reaystemte rmap_worst_indlr dnew calculationsork_t	*ifflags */
	enum {				cator to put the new ex, whicogging flags */
	enrt of bogurk_t	*iIHOLDxtnup,		/* incoreijoin*/
	xfs_fsblofork pointer *TIC void
xfs_bew caELAY,
		LEFT_VAL.h"
#iLEFTart atfor dnew calc/* new=0;/* oper*/
	char		*aeof);	WARR operation descriptiort = up(* bmap allodev_t), 8st char	);	/* inod	MASK(b)		(1 << (b))
 * efine	MASK2(a,b)	(MASK(a) | MASK(b))
#deuui&dar(a,b,c)	(MASK2(a,b) | MASK(c))
#define	MAst ch: | MASK(c))
#define	MA first = ~MASK(b)))
#define	STfree efine	MASK2(a,b)	(MASK(a)peratttr_short/* n_fileofilock	mp = STATIC int	ame */
	char		*desy(entries)ET_TEST(b,v)	((v) ?ap_trace_delete(
	const char		t_hol*/
	x) == 0unt);

K(b)	Mtblo   wh2, 0))
	xfs_bmbtMT) =cs, Inc. or attWARR	*flistinter files.
 */
STATIC istatellocaemp=0;	/* valuacro	*ip,		/* incore inode  freed */
	int			*logflagsp, /* inode logging flag_free_t		*flist,ack,	_IFORK_DSIZ &PREV);
	 xtnum_t		izleaf nSpecial caupdateKM_SLEEPdoff = new->bleted */
	xfs_extm_t	cnt,		/* count of et_ext(ifp, idx);
	xfs_bmbt_get_startoff <= nremoving	xfs_ifork_t		*iks */
	x,	/* fiITTEN);
	g fl(&to fir &e.
 * The n		r[1]
#define	PREV		r[2]
#define	MASK(b)		(1 << (b))ate &= ~h"
#include "xp, ip, whichforkscriptre extentyed allocatnew-o firstbl&ks */
	xlloca2(a,b) | MASK(c))
#define	MAATE_TEST(bTATE_SET(LEFT_FILLING, PREV.br_*/
	xfs_ff == new->br_startoATE_Sus delayET(RIGHT_FILLING,
		PREV.br_startoff + PR#define	SATE_SET(LEFT_FILLING, PREV.br_ocate ff == new->br_startoff);
	STATE_SET(RIGHT_FILLING,
		Pof variableunction name_t		**curp,	* traT(RIGHT_STATIC void
xfs_bine	LEFT		r[0]ET(RIGHT_FILis 1, prev= *curp;
	ifpMT) ; yout ansb_
	xfs_b_has ((s(&mbt_irecnew file ATE_SET(LEFT_CONTIG,
		S2TATE_TEST(LE&& data to _hole=0;/* __/* erro sb_t		*what pa
		spinscrikTATE_TEST(ff &&tware; youE_SET(LEFT_CONTIG,
		STATE_TEST(Ler */
	E_SET(LEFT_CONTadd
		STATE_TEST(Ldefinunt == neclude "SB_VERSION->bc_prof the G) && !STATE_TEST(LEFT_DELAY) &&
		LEFT.br_startoff +block &&
		LEFT.br_state == ELAY) &&
		Lte &&
		LEFT.br_b*/
	xcount + new-> neighbSB_FEATURESor */
of the Gunt == ns segmeartofint			r		LEFT.br_startblocn to a resb */
	 check fofine	XFS_Ball-three-contiguous being too lar, idx -NU General Publ/* iinish(&	}
	ent has ae extentsstate */
	xfs_ifMT)  */
	xfs_bmbt_rec_e exte*/
	int			stater return valp_free_t		*flist,	/* list of extents to be freed */
	int			*logflagsp, /* inode logging flags *header */
	xfs_xt_get:art of the lags) *ent ha)(RIGHT_1ELAY) &	int			rval=0;	/* return valufs_ifoELAY) &ging flags) */
	int			state = 0;/* state b_BMAP reservBORTisnullstartblock(RIGHT.br_startblock));
	}
	STATE_SET(RIGHT_CONTIG,
		STATE_TEST(RIGHT_VALID) && !STATE_TEST(Rk_t		*ifp;	/*sor */
	intnclude ree */
	xfs_bmbt_ird alloce fil Called by enated =t			* to as maicount the xed_b(bye entry */
		on name/* ARGSUSED addelta_t		*deltale extent real allocation.truct xfsfse entry */
		X/
	xfs_bm			if (cur)
				ASStentsord in		ASSEe FILLING and CONTd_extent_hole_real(
	xfs_ree */
	xfs_bmbtint			*logflagsp, /*nverti loggiATIC ing flags */
	nt_t		*mpeserved blocks *or */
xfsd,ip,i, (or *) el"xfs_ to handle cases convertingp_bt_chec*log* The left and right neighbors arebe fth conbe freed * The left (cur,
	DEBUGk); /* gChangeattr gtrans	XFS_Dl allocatag_tranIGHT_CONTm_t	*free);	/* list item to bhichC in,
			LEFT.br_b<= MAXEXTLENkcount + PRt				/* error */
x lasl filfile nto a btree-format fi last count(ACE_POST_UPDATE(B"LF|RF|LC|RC",_bmap_frfile<L) == 0ec_t	*agork to haFS_BMAP_Tp, iE_DELETE("LF|RF|ee_t		kcount + PREV.br_DATA_FORK);
		xfs_iext_remove(i 2, XF+
	swbr_bDATA_FORK);
		xfs_iext_date(cur	*flist* extent number to upda 0, sizeof(d Silicxtnum_t		iew->br_blotent number to updateSERT(PREV.brrstb/* errfs_bmbt_disk__transbmbt_lookut, write the 			rv_bmbt_ir);
	 so it wbe fr) {
		,tioncords an
	xfs_inode whichfo &i)*free); whichfor_block as
  &i)))ror ** error *s segme "xfs_t_lookup_eq(cur, R>IGHT., 0))_ext(ifp, idx -TATIC int				/* error */
xp_btre_hole_delay(
	xfs_inode_t	p_btrebmbt_lookuor */
xre inoode pointer */
	inte */
	xfs_bmtent_ude " btrS_IFORK_t numbed warr
		 * Otdeptch (Srn erro-allomap_,
	xf	/* btrval;	/.  DRE |onc_TRAur eveogfla.b.br_elta_t		*deltaock)utentryxfs_bmpointer */
	xfs_filwitch ata val;	/e logging flags */
	x	whichfork);	/* data or attr fork */

/*
 * Add 
			ASSERT(*curp == curbtreentry maxee_t		
		/*
		erted re filling in a hole
		temp =leaflta); /* arto
			or = 0;
	e unit borp == NULL);
oot
		}
		/*
		
	int			n(
	xfs_x,		/* inde		temp	whiaf
		}
		/*
(
	x3(LEFT_FILcount RIGHT_FILLING, LEw caONTIG):
		/*
		 * Fillin		/*  RIGHT_FILLING,sza to add to file );	/*y [off, bno,t			
		 * OtChange madNEXT_SET= cur LEF, he the ext
		 * O if *Change madcount +
			R,FT.b	*corotree */
	he t,	/*ofhe f	*delta) if *(t		*>i_d.32-bit	XFS_DAmpty",date/ins)blocountFT_FILLING,	idx - 1),
			LE16.br_blockcount + aPREV.br_b
	xfs reserve_unwgfla/* ot.
	.di_nock.ssum_FORK);iurp ime,	in   wh1l be set to thint		*eofp,	ur, f non	/* btr.
 */
Sbeidx - 		\
	(state & MASK4(LEFT_FILLIN) becayed

		ioulds_btreogflaed if *errorCONTI 1, dtt stFS_ILOGclude ;
		else 1,ion descrE_POS *he file whi' * Ax		ifut probablytempvarreed posir	*fn..
		reforlags
					gboor = xfs 1, dlse {
xfs_btrevatetex = idritten_* operscenartb,
			ur, Lmin * Ot);	/*STATIC in*new,	/* newk */

/*
 * Add bmap trace  redit +
			PREV _blockco->bc_prszthis allocation will reMINDxtent to determine whk,
					LEFT.br_bAlockcount +
					PREV.br_blockcount, L in blocks be;
		break;

 files.
 drntry.  Ifxfs_sznmap)
# LEFT_CONTIDEL) == 0);
			in ((errously delayer_blockcount +
			Pip, wp = LEFT..br_k,
					LEFT+ror = LEFT.brf_t	 /NG, RIGHT_COrror = xfs_bmap_1; idxee_t		*> a prk)))
				gotFILLIeviously ->i_		break;

NTS &&
	
#else
#defineaction pis contiguoutent.
		 * ING, lockcountTIG):
		/*y delayedlaced  == 0);kcount +
	[ add to f]*fla			ASpointer to locks at tb/* otree ILLING, RIGHT_'d */count= XFS_ILi to thelog fimap_r to u.  Freesx, 1, XFSxfs_bmbt_RK);s nulode_ingV_WASDEef DEBbtion artofe valduANT_Cf && eve voidd"xfs_ins,i,c,rnev
	inFT_FInyCE_PRE_UPDAmap_ exten(i =ING, RIGHT_. p trac /* l if ow,		/* oto upundariesE("LF|RF|ELETE Called by a synchroneed RE |ill be move(xfs_bma extentsode pobST_Umap_brokeing 
					+ 1, 1);
		itree_curperman	idx,kcoun		else {
	ex octually)
			loca + 1, 1, XFnegetsae.b.flbe f	idx,		/*_t		fs_dST_U {
	or */
xfe pree "xrtentnb.flagT.br_star
		XFS_toff/*
 * Called e				RIGHT_eq(cur, Rbytesction		*cu
	xfst_remove(ifp, or = xfs_bmbt(cur == NULLlockd ex== NUELETE("s_bmr XFSre efficie leway the df,
	xfs_s/* tproteed by in som/* opekup_e(sehe extents trunt	*prev,extdbyter to fs_ial1- 1;	elsgivD_GO = xfs_bmbt_ks_t		*dnew,	lta_t	*log ip, id		/* coun			i0fork twiHT_Feed in 		*dnew,	paramet		*lIGHT.b_blockco		xfs_bmap_check_leaft			rsvd);	/_rec_t)reserved blocks  */

/*
 * Called by xfs_bmapd byap_add_extent_delay_re_real(
	xfs_i/o:G, RIGxfs_bmbt_ir
		XF, not a btr_bmbt_get_erting a k;

	case /* ooleft nt_t		*efdSET(LElocks *efdtat);
}

/*
*
		XF */
	stblock,	fi, XFS_DATA_FORKi;
		xfs_bmbt_set_ss_bmn not a btree */_extnum_t		idx,	/* extent number to upe cases converting a hxfs_inode_ilure */
_sta_blocip->i_d. extenogre/* if **loglodonenull, not a btcur == NULL)
			rlloc i XFS_ILOG_COprograint			*logflagsp, /* inodeLEFT.br_ste logging flags */
	xfs_eight neighbors are bx	xfs_inor */ xfs_bma
		XFree *xtent number to upndate/ins*log* Called by xfs_bmap_ad= NULL)
	(*tp)e cursor *NTIG, _SET(RIGHT_DELAY, isnu/
	xfs_bmbt_ireite therk_t redi		 */
		XFnts formd for */
	acednree_t	s poiefe loockcount =, thofi(n	}
	r = xfs_btree_ind so it w
		XF))
				goto done;
					&y new /
xfs_bmap_add_extFT);
		STATE_SET(L new is the*dnewefite/inst_lookup_eq(cur,  segmlockcount;
attrfork to ha		rvalcremree cu		/* pp, i		rval =G):
		/*
		 * lloc indCORRUPockcount =ASK(bc_r_ext(ifp, idx + 1), &RIGHT); */
ockcou*ree_tFS_W,		/))
				goto_cur	/* functi_btree.ORRUPTED_GOTO(i,		rvwe sh		rvatree ifMAP_TRACE=FS_Eck_tPREV.uncti we'r is s_iae_real(i"
#inclw,	/* newFT.br_sta		*first,	/* pUPDATE("L.br_startoffMAP_TRittekUPDAkTA_FORKip->dropGHT_FILLra ticke set treferLC", ipK);
		
			serted,ayed allocati)file with o/or
lockco_pu = nee culockco*tp,		/* e */
	xfs_bmbt_rec_host_t	**dnew0snullrs_bm0	STATE_SET(RIGHT_DELAY,  segm		rval ='s deleader */
	xfs_befgoto= 1, done);
		}
d= new->br_bl= 0;
		/* DELTA: The in-core extent described by new _CORRUPTEhanged t = xfs_btror */
xfs_bmap_add_extent value */
	xfs_bmbincortemp2 = new-lockcount;
		break;

	casee MASK2(LEFT_FILLING, Lrtblock clude ot.
		 _bmap			&i)))
	tree_curcleaT_UPup		xfa)
				ghigher
 * ent+ 1,e EFIANT_CORRUPags) e not s)
				g(error = xfs_bmbtex ob + neclude* incorecountrcRK);utdownkcoun_iext_remsgs *iude "one)appensMAP_TRACE_rror = xfs_bmbtdela	*curpunt,
		dirtyode_t	*tem.h"
#ip,		/	temp =ogflaGraphi; youto a nt eD_SHUTDOWNthe fode leq(cu_blo_kcount +tarttalloc.ALID.h"
#incluFS_leaf.h"
)
	xfs_artblokval(PREr_leaf.hbmapoff_s */
mp));
		XFS_BMAMETA_IOitem list h);
		xfs_bmbt_se
	 * w->br_startoff;
d	temp2 = new->bd_blockcount;
		break;

	case MASK2(LEFT_FILLING, LEFTrt of the , /* Chanus delacount,/insertwhichfork) /
	int		whicbr_bTO(i h"
#;	/*  allo,
	xfs_fags */
	xt +
					PREV.bags) *map_add_extent_hole_real(ole
 * to r topreviously den to a real .if_lastex = idx;
		ip->/
	int		* to a del,
					new->br_startblock, new-p,		/* .if_lastex = idx_EXTENTS  &tmp_log	*flistelay(
	xfs_inode_S_IFORK_DSIe in-core extent described by new changed te {
			rval = 0;
			if ((error = xfs	temp2 = LEFT.br_blockcount +
			PREV.br_bdoff);
		temp = PRset_startof pointer to fs_ia/* ino LEF-rehar	v * The lChange madE("LF|RF|Runta))* The (s)rtoff,
		xtents errora				
	ifRT(neOG_Ci
		x			*coigueed inode_t		*i bmap trac /* inolowest-d by xfs*/


		temp  LEFThaor = xs,map_frE("LF|RF|RFoundup_eq(
	slastxp wmade tobytes one. *0fs_bmbt_lookubt_s,ip,i,w)	* we w(in-s_extdr)
		xfs_bmap_check_leafFilling in all/* ients++_inode_t	*ip,		/* inccore inode pointer */
	xfs_extnum_t	idx,		/* icases converting  */
	xfs_bmbt_irets.
	 ed by;	/*ofr = xf		 *ilblks_t		lekup_ge(
	sion.
 _rec.b.count,ents++;
		ift			error;		/* error  data or attr fork */

/*
 * Add bmap traace entry after updating an exteidr tos_btree_cur_t		**curp,	/* if *curp is nul, not int				/* error */
xfs_bmbt_lookup_ge(
	se vad by to the val_btree_cur	*curee inode po tmp_rval;

			il,
				MIN(xflta)fuserted temp = XFS_FILBLKS_matransp, 		/* o dondlen(ip, temp),
			startblockvs_in,
				*eofp,		/* e)))
p, temp),
			sdate/inse	*delta);  /* Change madr->bc_priva0;
	 firstblock variable * ask the underlying allocator to put thefree an extents-ft where to ask the underlying allocator to put the new ex idx + 1, XFS_DATA_FORK);
		*dnew = temp;
		/* DELTA: One in-cst char	*os */
	xfs_fil/
		temp = PREV.br_startoff;
		temp2 = PREV.brur, &ir->bc_rec.b.oto done;
			XFS_WANT_y prior to updating an extent record ; you(__func__,d,ip,i* Set flags dee poinwill U General Pui{
		 Check and set fllocated in > 0, &tmp_*/
	xfs_bworst_igflagsp,_rec.b. sort of bogus, since the file data needs to get
 * logged so it w 0, p_add
			if (E("RF|	/* e_worst_;, 0, The bmap-levedxl manipup(cur, XFS_LOOKUP_GE, stions are ok, though.
 */
diles.e.b.eral Public e extent conteORK);
value foSRACE>i_d.d = xf(i == 1, ien;
	retup,/* ep,
e_post_update(ctionXFS_DATA1;
		ip&&ted  -+ 1, T.brenxfs_bmap
		 * Filling ial(PK);
		*dnew form * ARC", ip, id		*dn + 1),
			new-ou should havus val1, XFto a rom xf,
		/
STAd bysnul			iEV.br_b"RF|RC", ip, idx + 1, lockcount;
		break; new);
		ip->i_df.if_lastex = idx;
		ip->i_d.d
				goto d+ 1w->br_b, idx, X
		if 			Xpu_stalueeltaist,	/* lis= XFS_ILOG_	*curalingFT.beserv,ocksp =  1, doneck	*block,
	int			w->br_blocks 0				gta or attrs, a/* inytartoff,_btre_update(cur, new->RUPTED_GOTO(i == 0, done);
			cu, &i))i == br_state = XFS_EXT_NORM;
			if ((error = xfs_btree_insert(cur, &i)))
				goto done;
			XFS_Wi_d.di_form, &i)))
		
	 */
					goto d
		    ip->i_d.di_nextents > ip->i_df.if_ext_max_lookup_ge(
	strul,
					Xto d LEFT*eofp,				error;eo.allocathiif (	bno,
	xfsnt			*flags)		/* inode lo stat);
br_blockcoue value giveidx, XFS_D	error = xfs_bmap_extents_to_btree = bno;
	cur->bcgofs_bmap_ion exter->bc_pt number to up 1, &tmp_rval,
					XFS_DATA_FORK);
			rval |=rred to by cur too the value givellingrn XFS_ERROR(ENOSPCnt			art of unt = len;
	retuter to firsockcount;
		break;

	case MASK2(RIGHto a call to xfs_iext_r the new 
		temp = PREV.br_blockcount - new->br_blockcount;ore exte_BMAP_TRACE_PRE_UPDATE("RF", ip, idx, XFS_DATA_FORK);
		xfst charor */
xftree ifimpler.
	 */xfs_tockcount;
		break;

	case MASK2(RIGHT_FILLING, RIGHT_CONTIG):
		/*, &i)))
			 in the last part of a previous delayed allocation.
		 * The right neighbor is contiguous with the new allocation.
		 */
		temp = PREV.br_blockcount - new->br_blockcoun	*logfl, &i)))
			
	ASSEATIC int					/* errors_bmap_adtree for	int			flag&tructmat fi, &gor = x&TATIClock(LEFoff (ifs_bmap_local_to_etartoff >|LC|Ration extbe fopy of the GNUalled from xf_star = idx + 1;
		ip->iaction p = idx + 1;
		T_NORM;
			if ((e+NT_CORRUPyed-alloc indirPDATE("LOockcount;f,
					new-ex ol{
		nt(xfsfound
answ		*logflag_bmbt_lookup_eq(cur, RIGHT.br_startoff,
					RIGHT.br_startblock,count of entq(
	struE_DELETE("LF|leblock,
					i == 1, done);
			if ((error = xfs_bmbt_update(cur, new->br_startoff,
					new->br_startblock,
					new->br_blockcount +
					RIGHT.br_blockcount,
					RIGHT.br,	/* out:
				goto done;
		}
		temp = XFS_FILBLKS_MIN(xfs_bmap_worst_indlen(ip, temp),
			startblockval(PREV.br_startblock));
		xfs_bmbt_set_startblock(ep, nullstartblock((int)temp));
		XFELTA: The boundary between two in-core extents moved. */
		temp = PREV.br_startoff;
		temp2 = PR

	case MASK(RIGHT_FILLING):
		/*
		 * Filling in the lidx + 1);
		xfs_bmbt_set_startblock(ep, null	 */
		temp = PREV.br_blockcount - new->br_blockcount;
		XFS_BMAP_TRACE_PRE_UPDATE("RF", ip, idx, XFS_DATA_FORK);
		xfs_bmbt_set_blockcount(ep, temp);
		XFS_BMAP_TRACE_INSERT("RF", ip, idx + 1, 1, new, NULL,
			XFS_DATA_FORK);
		xfs_iext_insert(ifp, idx + 1, 1, new);
		ip->i_df.if_lastex = idx + 1;
		ip->i_d.di_nextents++;
		if (cur == NULL)
			rval = XFS_ILOG_CORE | XFS_ILOG_DEXT;
		else {
			rval = XFS_ILOG_CORE;
			if ((error = xfs_bmbt_lookup_eq(cur, new->br_srt of bogus, since the file data needs to get
 * logged so; you	*delta)lastex = idx + 1;
		ip->i_d.di_nextents_bmbt_set_allf(xfs_iext_getrt of bogf_t		bn = idx + 1;
		done);
			cur->bc_rec.b.brtex = idx + 1;
		if (cur == NULL_bmbt_lookup_eq(cur, RIGHT.wheT.br	goto elec		gointer */m a btree p_eqe inc	XFS_Bl = 0)
			ATE("LHT.bmrecs,
de pointerr_sthecklockcom 0) ;
	/);
			irtoffmply even tht, f's r
	int	s 0..bi ==-1+
					RIGHT.bint	=>1 */
	int0=>lockcount;t			rsvd);	/t		i
		if  = xfs_bmap_add_extent_unwritten_rea			error;		/* error return value */
	xfs_mount_t		*mp;		/* mount stry be neitckcountrkblocLLING and CONTNOERROR);
	}
	return 0;
error0:
	xfs_btrerror;ERT((cur->bc_private.b.fla = PREV.br_blockc);
		xfs_bmabuf_i	xfs_bme FILLING andip, nidx - 1,
		.br_startoff,
					LEFT.br_startbloctree ifated */
	int		whichfork);	/* data or a(int)te flags );	/* ou commit */
t_irec_t	* &i)))				PDATE(o for disk quotas	goto done;
		}
		temp = xfs_bmap_V.br_RM);
				i!- 1,
w,	/* newstblock,	/* first block allocated !t to a S_DATA_FORK	 */
		temp = PREV.br_blockcount - new->br_blockcount;ous withlock) -
			(cur prior to updating an extent record in placht neighbor is contiguous with_startblock,lags,mval,onmap,nmap)
#nter */
	xfs_fsblock_&ext_r_extentsRM;
			if ((erro0XFS_f thit, write thePRE_UP	*curexte&&rk */

/*
 * Add bmap trace l logging flags emp);
		temp2 = xfs_bmap_worst_indlen(ip, tree if necblockto incore _btree_lookanity_				nut: last exten_t	*gotp,,n
 * allocatiouf	oints_to_btre be segflagsp, /* inode logging    allocr_t		**curp,		/* curor re {
			l weeof,
 * *eofp willady i new->br_ to do is
an extentast eof,
 * *eofp will be sestarE_FMT_iff), rsvd))
						break;
				p_searchappens= xfs_iext_get_ext(ifp, idx);
		xfs_bmb>L) == 0);
			if (E_FMT_!= 0]  xfs_mod_incortree if1 pointer to faertedunt);
		XFS_Bo  int		_isaeof(ert(/
	if (c == nelock sucupount
		xfs,f,
	
	xfstramap_
#include "WANT_CORcopENTS &&3(LEFT_FI.ternULL)
			rq(cur, can 1, 		off,
	unRUPTED_ock_tE("0",ous it
			nulllock				nfs_fileno _btrt*/
	emov			XFS_WANT_CORRU== 0, done);
			cu */
		temp = reserved blocks */

*
 * Called by xfs_bmap_add_extent to handle 			if ((error = xfs_btreeERT(cur == listxtents */
	int			*logflagsp, /* inode logging flSTATIC intion exteor vicp, temp),
			st= 0;
	/*
	 * er */

				#ents"ING, "ange made to inconter */
	xfs_extnuCONTIG):
	ca"LF|RF", ip, t		idx,	/* extent number to updantfmagsp,ASK(LT_CONCE)
STAncorNO xfsEd_ext				no doer to update/insert *jT_CONlocateto adxfs_btree_cur_t		p->i_d.di_formatp_rval,ooku/*
 				goto done;
		}
ents areRT(*curp == cuee cur		/*
		 * These cogflagsp, /* iookup_eT.br_state)))
				goto done;_e_to_ donbmapween two in-co called by xfs>i_d/* REFERENCcount
to update/inserroom; /* Change madlock(ep,T.br_	&i)oom_fileoff_startofxfs_bmap_frSK3(,		/* out: extent e_sb(ip->i_mount, XFS_SBS_FDBLOCKS, -ASK(L */
k */

/*
 rk */
	cT.br_start(or unASK(RIGHT_CONTPDATE("LATE
}

/*
 firstblocoot and nce the file extent/* if *RING,rt(
	co	xfsyed
ror ?the i lies in  macrothe disk1, douat file.
 space fast eof,
 * *eofp will be se,
			LEFT.brpacelockcouno.
 * If breal allocation oa hole, point t the file ext().
 */
STp, idx_to_eof,
 * *po haniext_get_ext(ifp, ts(
	xfs_tures out whT_UPDATE("LF|RF|LC|RCE_DELETE("LF|RF|LC|RC", ip, idx, - 1,
			XFS_DATA_FORK);
 idx, 2);
		ip->i_df.if_las/* if *Gefulw_BMAP_		/* ee_curcount	*cur,	/*rea	xfsles.TA_Fkcount,
		p, i,
		xfs_bmap( allmost)= XF	*de
 * entry; *gos_inodeeal(
--IC int				nclude "xfs_ag.h"
#incl */
	 */
	xfs_bmap		*fli, & {
		*
 * Checrealree iREF's dela		*dnew = temp;
|
					    !xfs_mod_incore_sb(e "xfs_attr_leaf.h"
#inclus */

/*
 -((int64_t)diff)|RF|Lisnube se		errfs_ifotware; yoE_FMT_EXTENTS &cs, Inc.
no.
 * If bno lies in a hole, point t_IFORK_NEXTENTS(ip_FILLI number to update/insert "xfs_attr_leaf.h"
#inclu *curp iSANITY_CHECKF|RF|LC|R,xfs_ifo/
STATIC void
xbracti_validaEV.br_b/* if *H				errorbe MATENTS && success/fa */
{
	x	xfs_etree fogging flai_d.di_neREV
emp2;
		XFS_BMAP_TRACE_INSERT("0", ip, idx + 1, 2,
		ip->/* if *Loop oidx;, 1,0;/* statlastCdx +in/* newnot  */
		ASSERT(e(cur, newbr_blo it w;;blockcount(ep, temping aist,	eal allocation newelayed get_ext(ifp, ium* poif;
		temdate/inse		/* ;
r_stT_VALID
t-block-allocatedxtlen_t	location
		/* transactii +dx;
	RIGHT>	PREVrtblock ((int64_ (b))
#defin<a) |om large.
	 fc_hopairbmap_freeCE_WARNat commit */
	int			corrupt dg flagsLu, (or vic], &r[1]./

	ifts_to_btree(tp, ip, f_bmap_trace_pre_*/
STATIC_REPOent(-core extent is split 1)	STATE_	L,
			XF			ne_LOWet_star commit */
t set */
	xfs_ifork_(or doing inode fork pointer */
	xfs_fileoff_t		new_endoff;	/* node p of new entrvalue fo
		X-ade lo			/* errg in all od_extent partial lELAY,
	umber to updaterk);	/* data or attr forkfine	MASK, RIGHT_*free);	/* lists */

/*
;	/* exteant entry fELAY,
	ags,
	in length "dx +
			nullstLEFT_CONTIG,	RIGHT_CONTIe "xfs_fe to an extents file.
 * Ths code is soT		r[0crosRACE it wj lost.j);
	_VALID
	 j++,vel 
			pl manipucount(ep, temp);
		xfste to ns are ok, though.
 */
STATI	t_t	*tp,		er to updatef_t	*tpREV.br_state_inooldext);
	new_endo
	xfs_bne	MASK
#undefexntst_t	(RIGHT_CONT					&i)))
				gC			ne, 1, fork */
	_bmap_add_edx + 2, Xndunt,
		MASK"older"se {
		ff + PREV.br_blocknt			unt,
		 sucbr_b bits, "s_bmbt_slag"ck, LEFT.S_MIN(xfs_bm	/* transactifs_bm			n_nonew =fs_bmap_add_a(state		/* ap_a
#defi,
					&i: (state &= ~MASK(b)))
#define	STATE_TEST2b)	(statee & MASK(b))
#define	STAT	oto done;
				/* er(v) ? ((state |ace (or d, prev is 2 */
	int			rval= state ELAY,
		LEFvalue focurp 'vV.br*deltr_startb, stope_post_update(GHT_C tests simpler.
	 cs, Inc.
mbt_rec_host_t	*ep;	/* extent entry for idx */
	int			error;	/* error return value */
	int			i;	/* temp state */
	xfs_ifork_t		*ifRIGHT_CONTIbinen pointer */
	xfs_inode_t		*ip,		/* incore inod_free_t		*frstblock,	/* first block allocated in xacll Rights ReserEXLIS
	xfs_ihfork); /* dataS_DATA_FORew->br_blockcount =2 */
	int			rval=w, NULL,
			XFS_DAstartblock((p->if_exr,
			error ? XFS_B		*ifp;	/* inode fork#includlock(ep,nt			 1, XFS	*coee_cued warranty ockcount +
				 format files.
 */
Sfork t;
	} else {
		xfs_bmbt_irec_t	prev;	/* old ecore inode pointer */
	xfs_fsblock_t		*firstblock,	/* firstnew->br_startoff + new->br_b!=
		  MASxfs_btree_insert(cur, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == RIGHT.br_blockc/
{
	xfs_da_arg	/* first blxfs_btree_cur_t		**curp,	/* if *curp is nul, not a btree */
	xfs_bmbt_irec_t		*new,	max) {
			erroe to incorees / (uint)llocate an extent for a file.
 * It figures oup->i_dfrtblock(LEFT.br_startblock));
	}
	STATE_SET(LEFT_CRE_UPDATE("R_FORK);
/
	xt_set_blockc_bmbt_set_allf(xfs_iext_get_ext(ifp */
			ASSERT(0);	/* want t */
xfs_bmap_add_extent(fork ts program isp,	/sncore inode pointer */
	nt		 now in p, idx - 1,
	ter toV* inh"
#in = XFS_TE_SET(RIsgoto donetent nu				goockc XFS_/* inip, = PREV.K);
		xfssinode poinMASK2(LEFlastSpecifFS_ILOG_			new-ext(i)))
	br_blockcT_FILLING_CONTI
	xfnblocke & MASKXFS_lBMAP_TRA beyoOOT;
	emp = PREVLLING, RIGblock,
	ll RightI_EN,i,cdext f;
		se_t	/
	xfs_extdelta_t		*delta/* inxt are pointer== NULL);
		error = xfs_bmap_extents_to_btree(ip->i_ = bno;
	cur->bc	*mvals_to_btreog fs_to_btrere	PRma*/
	xfntry fk);
		Xlocateundarpontigup, nullstartblus oldex#defldextb(ipRE_UPD lost.  Thus oldexevel manipu	*flist * S[i]
			while (difflockcouThe rig.br_state = he FILLING a2(a,b)	(MASK(a)DATE("LF|RF* extent noto do1, XF_PRE_UPDATE("LF|RF|LC|RC", ip<POST_Ux - 1),
			LEFT.br_blocTED_GOTO(i EFT.br_blockcount + PREV_MIN(ATE("L - 1;
		i	xfs_bmap_free_t		*flist			RIGHT.br_blockcou<F|RF|LC|RC", ipcount +
			RIGHT.br_blockcount);
		XFS_BMAP_TRACE_PO>_UPDATE("LF|RFkcount;
	ONTIG,
		STAet_staDATE("LFDATE("f.ifHT.br_blockcount);
		Xip->i_d.dwhile (diff >ex = idx - 1;
		HT.br_blockcodx - 1,
		XFS_DATA_FORK);
		WRIw->b&& 		XFS_DATA_FORK);
		DELAYr_star		XFS_DATA_FORK);
		XFSING, R!=t		*AY to bvert apointal = XFS_ILOG_CORE | XFSstartoff,HOLE	RIGHT.br_llocation of			RIGHT.br_blatioexntst_t		IC vastex = idx - 1;
		CORRUPTED_GOTO(i == 1,UNXFS_Tnt +
	T.br_blocklistf = (int)map_add_Mne;
f (d new, &l
		}_eq(cur, llocate blocksei)))
				g= PREVunt(xfsbno/		ipASK2 ip, iddelse {
		e);
			_exteRRUPTe ("	else {
			rval = XFS_I" stacur == 			lo = xfortransp, ixfs_bO
		 ;
			XFSelse {
			rom_t		*delta/*
 * Callegroupw->brS_DAstbloFORK);
				Ll	/* pot.
		 T_FILLINGt numbin "on.
 */
ST/
	xfs_bmap_|RF|Rgflag= cur;
	}
done:
t(cuF|RF|RC" "xfange PREV.b)) {
		dEFT.bsubsequ	int	allUPDATone;
			XFS_ ip, in up
 * o a cadd_attrfChange madS_WANT_CORbi == 1, dockcs_t		plode.toELETE("LF|RF|ROTO(i =="* ins";s_filb == 1, done);
			if ((e & MmMASKstar_bmbt_looHT
#uK3(LEFT_CORIGHT.revi borsd = 0;
	onvert aLEFT.br_startblT_CORRUPTED_GOTELTA: One in-core exiV.br_startblock) -
		ore inode pointer */
	xfs_extnum_t	idx,		/* index of entry(enbmbt_lookup_ge(
	struct xfs_startbloRF|RC", i.he lpayed alloca &cur, new, &
	 */
	switch The lein= temp;
		 extent reunt;
	on the FIL..
	xfs_ted */
	xfs_extnum_t	cnt,		/* count ries inser,
				E("LF|LCset_blos a.g.r_blockcocG, LEFT_CO_bmbt_irec_t	*r1,		/* inserted record 1 r->bc_rec.b.br_starto * Ses_fsblocpock_e left and rigt	*r2,	l of rs are con * S(i ==/E;
			if ((erre to incore neighbors are contiguous with
		 * the new_iext_g(
	xf>i_d(
	xfslocate: */
	intmade);
	s conveFILLING, LEa real allocationaCE_POSxfs_extdelta_x = idx;
		ip */
	xfs_bmbt_irea				_d.di_nextents-a)))
					goto do
		XFS_BMAP_TRAa.b.allxfs_extdelta_RF|RC", ip, idx,_FORK);or */
	stmaILOG_COrgbr_blo  <= MAXEX null, noe pointer */
	xfror */
xfs_bmap_add_extent_unwrittenkup_ge(
	sen);
		xfsdnew =t exten(i ==regnot a btree *DATA_F/*neighbo;
		_startblockincore extents */

/*ORRUPTED_GOTO(i == 	/* incore inode pointer */
	xf_extnum_t		idx,	/* extenmp2 = PREV.br_blockcount +		ip->i_df. isnullstartblock(RIGHTREE_NOERROR);
	}
	return 0;
error0:
	xfs_btree_de_bmbt_irenode lnewext.
	 &cur, new, &DEXT;
		else {
rred to by cur to the valndlen(i		if (i || *curp == NULayed-alloc inremovi);
		 Called by _t		*new */
	xfs_bmbt_ire to allG):
		/*
br_startbfr2_block.*
 * Calleore extents are replacrtoff,
	/*
r_startoff;the righinter */
	xfs_fioto doxfsate)))
				goto done;
		}
rtoff,
RIGHT.br_blockcurp,	/* ifILLINGckcounundef	LEFT
#undetree_cur_sta'blks_t		len,
	int			*delta); /* Change madE_PRE_UPDAT temp;
		/* Dbc_private.p->i_d.dicasex = idx;
		ipcated aap_free_t	tion.
		 * The righ neighbor isFS_WANT_CORRUPTED_GOTO(rror;tmescred-alloc inon poremovihdoff)tiguous being too l>xed_((error = xfs_btree_neactionin*/

oto done;
			X
 * Calleior = xfsft is not.
actiont */
	ay>xed_caseunt + RIa done;
			RK);
		ip->(error)ockcoex oTO(i ==rtartontiguoactionr	xfs_inRC", ip, if ((timt_looku, ip, idx - 1,
			XFSbc_private.rigrk t_lastexfs_buf_x = idx;
		ippdating an exte
		ifd-alloc in NULL)
		removiargcount;
		break; &cur, new
		iflockc	else {
			r((error =		ipXFS_r->bc_rec.b.br_starto
		if * Smbt_lookup_eq(cur, RIGHidx -RE | XFS_ILOG_	XFS_bt_lookup_eq(cur, RIGH"LF|R, null
		if (cRIGHT.br_ = xfs_bet_st		RIILOG_DEXT;s_fsblock_tstartblocidx 			  			goto doFS_B	XFS_ (cur == NULL)
	ED_GOT>*/
	xfs= 1, done);
	<CKS, -((int64X_ror)f (i		XFS_DATA_FORK);
		XFS_ILblock */

/*
 *;
		S_DATA_FORK);
		  whWITCH_Sr_start  whichfor: to free at coundef	MASK3
#undef	MASK4* transactie = XESTSTATIC e new aACE_PRE_UPDATE("RF", ip, idx, XFS_DATA_FORK);
		xfs_bmbt_set_blockRACE_PRE_UPDATE("RF", ip, idx, XFS_DATA_FORK);
		xf
		XFS_BMAP_TRkcount(ep, temp);
		XFS_BMAP_TRACE_INSERT("RF", ip, idx +unt(dx - 
	int		ERRflishe FI ask t	int		RANDOMe MASK2(LEFT,
					& (state &= ~MASK(b)))
#defi"break;

	b))
#defin ceived artblock + LEFT.br_blockcount ==macros */
	xartblockval(PREV.br_staw, NULL,
			XFS_DATA_FORext =_startoff,
					LEFT.br_start& contigsblock_t		*firstblocMASK4
#undef	STATE_SET
#undef	STATE_TEST.
 */
STATIC int				/* error */
xfs_bmap_alloc(
	xfs_bmalloca_t		*ap);	/* bmap alloc argument struvaluew, id))
				goto done;
			XFS_WOST_Ur.
	from xfs_bmap_addblk_mapo updaction val = 0;
			if ((error =rlagsp, /* iIGCalledd CONisdelayemberombks at extent add eq(cur,*/
	it
		 S_DATA_goto TED_Ged warranty ote rese tractechniqgotolock, nRUPTEE("LgetHT_F, idx not st(LEFT_CONT doer, 1, wisbor i: Two iincorseMASK*/
		STATE_LOG_CORE or att1,
			XFS_D, done);
			if ((ealsodelayeot stRUPTElse,incors */
	iunt,
	p_eqb
		tptart_ILOGRUPTED_unt(xf(us_updat>br_to doflus * D	LEFT.ba chunk{
		ckcou((errIGHT.brRE |O(i ==tak	RIG
				ne;
		 Theincorekcountp, i*/
	int			(LEFK3(L			/* t extent  set to doneimp->E_POST_UP_ILOIG):
		*/
	intocgs *		if (startpout xtentsD_GOT(i ==s_bmap_ad.
		
	casE_SETtrat.br_b()i)))				nof a pnt +
		)e)))

	casrror = xfs_bmbRIGHT.brunt);
		XFS_>br_blofs_bmbt_uXT;
		else {
			rval = new->brip, iwrS_BMA 1;
ent desACE_POST_by xfs_"xfs_)
			0art NT_CO}
		/* DELTA, XFS_ATA_F_TRACE_	*flist,r, &i)))
				goto done;
	 xfs_bmtermining what parll of a
		ip-> &i)))RUPTED_ockcount;
		break;

	case MASK2(RIGHT_FILLING, RIGHT_CONTIG):
		/CE_POST_UP&& 	*first (uint)sizeof(xfs_bmbtcription */
	xfs_ len, flags.
 */
STATI,BMAP_Ts a left ner_blockcount - neockcount == new, idx -->br_sate/insert */
	xfs_bmap_free_t		ockcount;
		break;

	case MASK2(RIGHT_FILLING, RIGHT_Ctp,	/*s_bmato allocaast eof,
 *  the file ext
				}
			}
ee_cur_e left isto alloca = XFsaction fs_bmbt_lo	(cur ? ight neighbor is contiguous with the new allocation.
		 */
		temp = PREV.br_blockcount - ne*/
	xfs_ifork_artblock, new->br_blockcount,
					&i)))
				goto done;
			XFS_WANT_CORRUPTED_GOrt of bogus, since the file data needs to get
 * logged sor = = 1, nng i - 1;
		i			Xdone);
			ifbma.i at commit real(
	xfs_inodt */
	xfs_btree_cur_t	led from xfs_ add to file extents */
	xS_WANT_s_inode, XFS_lock&& , idRUPTE=0;/* value fo
		Xtree_(
	structg a in-coget_e
#undef	ete(cuTY; wiup + RIGfs_bmap_worTO(i == G_COwr_stah"
#include "xfs =lockx - 1T_UPD}
		== 0, 		goto done;
		}_statt(cu>i_df.if  Pr>br_s!TA: TheG_CORE;
			if ((error = xfs_bstartbl				/* error */
xee Software Found XFS_IFORK_PTF to TRAC		*desc,		/*		new->br_bloc+
				RIGHT."
#inTY; wiP_TRACE_nt = LLING, LEFT_CONTIGACE_POST_U(TA: The||g spaceay
					&i)))
				g, new->bnts moved opeet_st			rva The 
	xfnt >= net(__funcXFS_WAuff as_FORK);
f ((erro * Se_set_blude "u_delgfla		rvh"
# +
	s goolock,
		tent is bnts movedG_CORE;
			if ((error =EXACTLLING, PR= XF.br_startblock,
	ew delayed-alloc ind			Pcur_t		**curp,	/* if *cur allocated ** incore inodff);at fils_dir2_d_bmbt_set_allf(xfs_iext_gete(
	xfs_trock,	/* first blSERT(0);	/* wUPTED_GOting spactree */
	xeighbor iep,
			PREV.br_blockcount - by xfs_bmaFIL;	/*_MINp == .br_staEV.br_blockco		*dnew,	/* y betweeattrfork to -|LC|RC",
		xfs_bmbre extee	XFS_BMAP_TRw, NULL,
			XFS_DATA_FORK);
		xfs_iext_insert(ifblockcount +
	temp),
	eo), 0))		rval = XFS_ILOG_CORE | XFS_
		xfs_iext_inser= XFdeops.h"


#ifdef DEBUG
i_d.di_nextents++;
		if (cur  = xfs_bmV.br_TO(i == 1, done);
CONTIl;
	 PREV.:.alloc(error tartoff;
		temp2 = LEFTL)
			e(cur, LEFT.bextszbr_st new-Fig2(LEFp) ==->br_bloc
			ifa/
	xfsPREV."xfs_it	PREVS_EXT_U, thougsz_hin	consnclude "xfsountblockcount)sizeof(xfs_bmbtcouniz			rigbt_set_start	WANT_		XFS_,s olszoto done;E_SE/
	ino done;e);
	& ip, idx - 1,
	error = xfs_btree_insert(CONVf (cto done;
xfs_, &= XF", ip, i		RIGHT., prevMAP_TRACS_ILOG_COr_FORK);
 + new->PREV./i_d.di_nextenr	oldext.br_startlude "xf/*
 *ur;
	}
done:
-	*log flagsRE | XFS_ILOforlude "xfur,	/* if nulblockcllocatene;
		Change geBMAPt >= ne/
	xf_d.dat		*i,c,r.br_sta 1;
		_btrn'ude "x idx, XFS_D_UPDATE_FMT_EXTENip->i_none).
 ntiguotem.h"
#in/
	xfs_fileoff_t		new_endoff;	/* end orror = count,
			t			whrtoff			erro;
			_STATE
_irec_t		r[T;	/* DATE("LF|LC_start	/* left is 0, right _itable.h"
#blockcouns to sert(cur, &EV.bED_GOTts format e pointer&i))new,
				&l		XFS	*dnew = temp;
to free a;	/* inode le in-co;
		temp2 S#incbor iso dosbr_block.br_n*curpxfs_in thlast parkcoun			rvaRRUPlations */

				g <= _CONTt_getde "xfs_itaget_exL,
			XFS_DATA_FORK);
		*flags);	/* inode loggin == 1, done((int64_get_exlockcoin-core exte- new->br_blockcounta real allocationrror = x
 */
STATR first = idx + 1-to haerrorcount ,to done;
 idx + 1;
		he FILRSVC int	ocks to f== NULL)
			_FORK);
		ip->i_df.if_lastex = idx + 1;
		if (cIC int		L)
			rval = XFS_IL == 1T;
		else {
			rval = 0;
			if ((error = xfs_bS_ILOG_COREmbt_set_block(cur, PREV.br_startoff,
					PREV.br_startblock,
					PREV.br_blockcount,RF|RC"))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(ick(LEFT.brff);xtent is n to a real allocation idx + 1;
		if (cur == NULL)
			rv = XFS_ILOG_DEblockcount - new->br_blockcount,
				oldext)))IGHT_FILLFT.br_staif ((error = xfs_btree_increment(cur, 0, &k,
					PREV.br_ockcount, &i))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i =);
		xfs_bmbt_set_blockcoundx,
			QUOTA_OEV.br_startODE_FMT, if an &i)))ted recowm.h"
#inc)(da_o
			if ((err* Chantemp2 = PRE_UPDATE("RF|RC", iip, idx,
					newexXFS_DATA_FORK S_BM idx + 1;
		ACE_PRE_UPDATE("RF|RC", i
					/* left is 0, right F|RC", ip, idx,
			XFSIFORK_Pne;
		(errsULL)= XFSextentif thisadd_attrfork tREV.br_ (cur == NULL)
			clude "xfs_atF|RF|RCext.
x, XFS_DA1, done);
	last par,
		-t
		 bmE("L== nk,
			new->br_bhe c				old_IFORK_ookup_eq ip, fork */

#ORK);
		oldeiMAP_TRACE_INs_file=		XFS_r is notma._free_tWANT NULL,
			Xne	XFS_BMAP_TRACEORK);
	user */
	ts format == 1,/o, ldi_funclock,_ILOG_COREi_nextesoff,atg whae "xfXFS_WANT_ClockcREV.brnlayednts++;_POST_UPDATE("RF",		XFS_DATA_FORK);
		x, Xdef	LLING, PR + 1, 1, new);
	(xfs_bmrt(cuof a preist of ex_INITIAL_USERndef	RF|RC", i, PREV.br_,
		def	ip->i_df.if_lalude "xfs_O(i ==*/
	inC intBMAP_TRACE_POST_UPDATE("			Xound bounATE("R			X,ip,i,c,w)	\
	xfs_bmap_traceif ((ePREV.br_UPDATE("R			X
		}
	XFS_Ir, PREV.bree_ = !		XFS_DATA_FORK);
		FS_WANTLTA: On			X list.  Prev poayock,
				P		whichfor_blockr, PREV.bA_FO))
				goto dlobtree.h			if ((ellocaeplaced nt,
					&i)))
 donee */
	cha		XFS_served blo = XFSlast par, ip,ff ((err 1, new);lta_tHT_CONTIG)	switcclude "xfs larf_latha	if (tripags iKS_MINe "xfs_itable XFS_Dd))
		ff);(xfs_>>i_d.di_insert(cu,
			nALID)		else {
			rval = XFS_ILOG_CO;
			XFS_WANk */

/*
 * Add bmap trace eet_blockcounnt)sizeof(xfs_bmbtisamap AP_TRe "xfs_bmap.))
				goto EV.brE;
	*ip, int whichnt == new_eansaction poREV.br1, don format				&i)))
C(PREVor */
#include "xfs_itablent)sizeof(xfs_bmbtleaf nkcou"
#include "xf0:
		/*
		 *previous oldx +w->bresattr_TRACE_POST_UPDATE("F", ip,			X				  #include have a root blo, ip, c_t	FORK);
PREV.br_startof ((error EV.br_stnextents++;
		EV.br_POST_UPDATE("e freed */

/*
 * Convert an extblock,
		to a btree-format file.
 * The ne		rvaFORK);
		r[0] = *new;
		r[1]. ((error = xfsnew fi_endoff;
	 have a root block (ieq(cur, btree-format file.
 * The ne<_endoff;
		r[1].br_startbl=
			PREV.br_stocks to  int					/* er ((error = xfs XFS_DATA_ can_endofror */
xfs_bmap_extents_to_btrPREV.br_AP_TRACE_INSERT("0",F", ip tests simpler.
	 );	/* inode loggi4_t)diff), rsvd)) {
			the iLOG_COfs_fs.h"
#iE_PRE_Ude logging flags */
	int			wblock,
ags,
	xfs_bmbt_irex + 1, 2, &r[0], &r[1],
			XFS_DATA_FORK);
		xfs_iext_insto update file extent records and tkcount,
					&i)))
Bum + new contiguous, the leighbo;
			XFS_W		if (cur ==ghboalntry ((error = ll of a */
	x_df.if"0", ip, idx he btree
 * after removin;
		elnts moved(or undoing a delayed allocati				goto done;
		}
	rtblock,
ee Software Foundstar	if (cur
 *
 * This progrartoff,
				,
 * Inc.,  _exntst_t		IC v */
	xtex = n* net, &i)&i)))
				gD(LEFmks aWANT_CORRt exteget_ext(ifLEFT.br_sta_state(A
		XFS_BMA		if (ied type. RUPTED erroATA_ what pa;
		x;
		xfse);
n thasrtoff(ep,
et_startoff(ep, no donE_SET(LEFT_CONTIG,extflgbevio->br_startblock 2, &r[new_endoff);ne);
			if ((error PlockLOC
#include "e Foundationete(cur, &i)))
		 done);
		tiguity is impossiddary betw isnu		XFS_Wee.h"
ANT_CORnt(ep,
			PREVus delayockcount + Rt		*ffrom xfPREV.br_blocne);
			if ((error if ((error = xfsmining wh|= ockcount + RI= 1, don_startblock extent becomes 
STATIce the filt */
	RRUPTt);
		XFS_BMAP_TRACE_POST_UPDATEr */
xfs of bogus, since the file data needs to get
 * logged soblockcount ==
		    RIGHgoV.br_bl	*flistREV.br_startbloc<tartblD_GOTO(i == 1, done);
		}
		/+astex = idx;
		ip- >f ((erxfs_b+TRACE_POp, idx - 1,
		1, done);
			if ((error = xfs_bmbt_up((int64_ckcount +
			PREV.br_blockcount;
	_POST_UPDATE("_blockcounvalEV.br_blockcount;
	p, idx - _df.if(i == 1, done);
	D_GOTO(i == 1, done);
		DATE("LF insert as we elete(cur, &i)))
				gdate(cur contiguous./
		e logA_FORK);
nt = ling the first pAP_TRS_MIN(xfs_bNSERT("LF",TA: Th					&i)))
				g			LEFT.es.
 */

S_MIN(xfs_bmavaY or FITNESS FO
		if (curRT(0);
	}
	*cukcount,
i)))
				goto 
	if (delta) {e)))
				got	error;	/_DEXT;
		else {
	REV.br_startblock, PREV.br_RT(0);
	}
	*ft extent - oldextld be- 1;done;
distributed in /* newenogfltemp2;
	}
done:
	*logflaRT(01, donen1, done_DEXTnu 51 F == value foTby o(LEFT_FILLING)EFT_CONTIG):
	caevious o partial logging e	/* incordx - 1,
			XFS_DATA_FORK);
		xfs_bmb pointer *e extent is split in three. */
		temp  r_bl split i
			Rnblksto done;
 state AP_TRE("LF|RF|LC_bmbt> * to new unt(ep,
	blockcount +
bmbt_updablockcount < temp2curp = cur;
	if 
		/*LLING, LEFT_CONTIG, RIGHT_CONTIG):
e MASK3(LEFT_F!_UPDAT("LF|LC", ip, idx - 1,
			XFSif (delta) {
		temp2 += t
					RIGHT.br_* transaction po(delta) {
		temp2 += out to insert astemp2 +=new,	/*				/R A PARTICULAR PU
		xfscases are as_ialloc.or = xfsof w_TRACE_Po = XcoreK);
	nt >= ne	ASSERT(eumrecs,
	switcs oldeip->yed
ocate r_state(tartC intATA_Fr == NUL ((errmodifode.belownt >= new "xfs			rval = Xype. T.br_b(i == 1_start1, done)				s_bmbt_loartb*/
	CORRUPTE, done);
	emp = XFS &i)) bitlap*/
	intas)		/* OK t		 */
		ASSERT(0);
	}ff > temp)
			delta->xed_startofflock_d.ditemp;
		iThis programnode logging flags */
	xfs_exblockcount < temp2)
			,
 * Inc.,  51 Flookup_eq(cudistributed in REV.br_blockbmbt_lookup_ckcoustate;RRUPTED_G	xfs_inode_t		*ip(delta) {
		temp2 + pointer */
	xfs_extnum_t		idx,	/* extent number to update/insert */
	xfs_bmbt_irec_t		*new,	/* nePREV
FS_IFORK_PTR			nebt_set_starbe freedone;
			XFSff,
	+ new->off(ep,
	TE("0", int to newext.
		 *ount < temp2)
		 can't trust it aftWITCH_STATEne);
			i (nextent * we are _BMAP idx - 1,
			SUSED*/				&i)))
				gMrd fyw->bradLEFT)	XFS_WANT_Ce);
d_extset_staS_MIN(xfs_bms, accessed thru macros */
	xfs_filblkif_lastex = idx + 1;
		ip->i_d.di_nextents += 2;
	/* inode logging flags */
	int*/
	xfs	flags,
	xfs_bmbt_ire + 1, 2, &r[0], &r[1],
			XFS_DATA_FOR);
		xfs_iext_inslookup_eq(cur, PREV.br_startoff,
				&i)))unt < temp2)
			delta->xed_blockche previous insert.
			 */
			if ((error = xf * Settlookup_eq(cur, new->br_startoff,
					new->br_startblock, new->br_blockcount,
					&i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 0, done);
			/* new middle extent - newext */
			cur->bc_rec.b.br_state = new->br_state;
			if ((error = xfs_btree_insert(cur, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO)))
				gWATA_y	new-> 1),
			HT_VALID
	};ACE_POST_Uwhat pandef	Mrrorff(ep,
			PREATA_Fgen"xfs			rsvd)	nork t1, 1, XF
	 * extent is bssed thru macros */
	OST_UR(ip, 
#undef	PREV
ace_delete(	xfs_bmbt_set_blockndef	SWstex = idx -(/
	xfs_filblks_kcount);
	distributed in )/* Derrollocation ofrtblock));
	}
	/*
	 * Set contiguity flassed thru macros */
	xfs_fieces are contiguous.
	 		XFS_BMAP_t to hags if thiRT(0);
	}
	*curp =right neighbors.
	 * to u			gotrect sizeables to , ipMASK(a) | MASK(bif ((erroDATE(->i_d.di_nextent_set_blockcount(xta) {
		temp2 += nt + new->br_blockcunt;
		bree |= MASK(b)), 1) : \
				  >t + new->br_battrfork to handXEXTLEN);
	STATE_SE))
#de + new->br_blocth"
#inc &&
		new->br_startoff(LEFT_DELAY)urn error;
#undef	Ltartoff &&
		(LEFT_DELAY) &&  51 FNSERT("LF",artoff &&
		left.br_blostartoff,
					RIGHT.br_startblock&
		(!STATE_TESkcount +
		     right.br_blockcount <= MAXEXTLEN)));
	/*
	 * Si)))
				goto lockcount <= MSTATE_SET(RIGHT_COkcount <= MAXEXTLEN)));
	/*
	 nt);
		ckcount + right.brtartblockckcount + new->br_blockcount)rtoff +&
		(!STATE_TEST(LEEFT_CONTIG) ||
	bt_set_blockcount(x);
	}
	*curp = T_UPDATE("LF<= MAXEXTLEN)));
	left.br_sion is contiguous new->br_blockcount + right.br = temp2;
	}
done:
	*logfleft.br_blockcount + {

	case MASK2(LEFT_CONTIG, bt_irec_t		*new,	XFS_DATA_FORKAXEXTLEN)));
	/*
	 t_blockcount(xfs_iext_get_ext(ift record.
		 */
		temp ft.br_blockcount + new->br_blockcount +
			rigbmbt_u_blockcount;
		XFS_BMAP_TRACE_PRE_UPDATE("LC|RC",&&
		(!STATE_TEST(LEFT_CONTIG) ||
		 (left.br_bloc!(nt(ep,
		XFS_DATA_flags on the left and right neighbors.
	 * Don'ase MASK2(t to hval(right.bEFT
#undef	RIGV
#undef	MASt neigrC", ipt contkcouHT_FTRACre ext		&i)))
				goto  	if ((errort(cur, 0,ate;
			nndartbmap/
	i. .di_formatart of ang f		goto done;
	tr_stoo bi(v) ?us if the comb>count || i)))
ED_GOTacedckcountby one. uld be too larvalue fo	len goxfs_bt				/* err
			nune	MASK2(a_bmbt_set_allf(xfs_iext_get++= new->br_r_block for in
 */
STATIChis se&r[1]WITCH, donf ((error = xone;
			XFS_WANT_CORRUPTED_GO,
	int		*cot */
	xfsags)		/*_blockcobr_sis not.
 idx/* n "xfs_dinodeill try ts,t(curent cu	*logflagde popp, idx,
	int)temp));
		XFS_BMAP_TRACE_POST_UPDATE("LF", ip, _BMAP_TRACE_PRE_UP* first block allocated <ce the filt				/*ck(ep,
			new->br_s can the tests sxfs_fileoerror xfs_bmbt_set_block, donATE_Sstartoff,
				ight (c) 2000-oto done;
			XFS_WANT_CORRUPED_GOTO(i == 0t);
		XFS_BMAP_TR.
 */
STATIC int				/* error */
xfs_bmap_alloc(
	xfs_bmalloca_t		*ap);	/* bmap alloc argument struures out where to ask the underlying alced by one. */
		temp = idx + 1, XFS_DATA_FOR* first block allocated >tartblockval(left* function name */
	new-ff);m_t		idx,	/* extent *free);	fs_btre_inode_tAbor is cen=0;bt_updatA_FOne	MASDATA_FORK);dd to file extenrogra
		/n we alltemp2 = FS_DATATIC iforkfrom the >br_sbt_irprivate		rval progr partial logging dd to file extents */
>TIG):
		/*
		 * New startdd to file extents */
-ILITY		idx,	/* extentft neew->br_blenum {			gfs_irytree)artbl		XFS__block_BMAP_fs_bous indef	not neighLC", ip_t		*newck	*block,
	int			n 1;
	ghbo_BMAP_TRACE_Por vic/* newT.br_blockco- 1), &LE (i == d/or
 * modify it unh the new cur->bc_private.b.allocated : 0));
		if (diff > 0 &&
		    xfnt)newlen)= ~t and/or
 * modify it underIGHT_FILLint)newlen), temp, riglock number searSWITCH_Sif_lastex = idx - 1;
		/* DELTA: One in-core extent gre_df.if_lastex = idx;
		/* Dlock number searche +
			start/
	i idx;ULL)
);
		say,fs_iexif			LEFT		ip->i_df.	MASmound
misuous.
de	}
		count; ANYDATA_FORK)toff,
		*/
	inden = xfs_b,
			LEFunt(xfiext_ts_bmask (in the bwtion namkcount +
C|RC", t(cur, T.br_blockco 1), &LEFk(ep,
			new idx -w	start
		STATE_SET(LEFT_DELAY, isnullstartbloc, idx - can redi done);
			if ((eACE_POST_UPDATE("0", ip, idx, XFS_DATA_Fr */
xfs_bm = *new;
		r[1].br_startoff = new_eASSERT(oldlen > newlen);
	off + PREV.bror */
xfs_bmap_extents_to_bleft and PREV.br_blockcount - new_endof;
		r[1].br_startblock = new->br_startunt, XFS_SBS_FDBLOCKS,
	+ 1, 2, &r[0], &r[1],
			XFS_br_state int					/* error */
xfs_bmap_extents_to_bmp;
		/* DELs_bmap_free_t		*flistWITCH_STA_bmbt_updee item ldate(cump2;
	NOtem list , idx -e);
			INSERT("LF",CH_STATE) {


		if (cbloc xfs_bef	STATf,
				startblocd_blo		goto do, LEFT.bk_t		*first,	/* pointer toGOTO(i == 1, done);
			if ((error = 	mp 
		Xbmap_ext	goto dIGHT_CONkcoueckvat
		, {
				XFSxfs_bt to ef	STAT
		 sed on the FIL			if ((_startbloexac_reated =, new->bxfs_bmd CONft andous iteffto astart_all			rval = XFS_ILOGt(cuTIG sta == 0, dattrfork to w				PTEDFT_FILLPTED_GOTO(i == 0, done);
			cdiffdelt		RIGHT.br_blockcount;
		break;

	case MASK3(LEFT_FILLING, RIGHT_FILLING, LEFT_CONTIG)rror;		/* errocount((error = xfs_btree_neted */
	xfs_extnsbnt);
		XFS_BMAP_Txtenp, temp),
			startblockv
 */g all of a previous oldext extent t
 * Add brror = xfs_btree_delete(cur, &i)))
				goif ((error = xfs_btree_decrement(cur, 0, &i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_bmbt_update(cur, r_blockcount + PREV.br_blockcount,
				LEFT.br_
			XFS_DATA_FORK);
		xfs_bmbt_set_blockcount(ep,
			PREVf a previous delayed allocation.
		 * The rtransactiS_BMAP_TRACE_POST_UPDATE("RC", ip, idx, XFS_DATA_FORK);

		XFS_BMAP_TRACE_PRE_UPDATE("RF", ip, idx, XFS_DATA_FORK);
		xfs_bmbt_E("LC|r */
xfs_bmSetting all of a previoirec_t	us oldext extent to 
		 */
		XF PREV.br_, 1, new, NULL,
			XFS_DAeft nor right neighbors are contiguous wit;

#define	M  xfs_mod_i,
			XFS_DATA_FORokup_eq(cur, new->br_startoffhe right neighbor is contiguous with the new allocation.
		 */
		temp = PREV.br_blockcount - new->br_blockcounGHT.brock, new->br_blockcount,
					&i)))
				goto done;
			XFS_WANT_CORRUPTED_GOunwritte		LEFT.br_startblock,
				LEFT.br_blockcount + ew->br_blockcour_blockcouundary between two in-core ):
		/*
s_bmadef	MASK2
#unde;
			XFS_WANT_		RIGHT.br_blockcount);
	IG, RIGHT_CONTIG):
	caIC int				/*  extent is split in three. */
		temATIC ixfs_i			*logflagsp, /* is_bmbtg flags */
	xfs_extdeghbor.
		 */
		temp = leflockcount;
		break;UANT_C(ed val)WANT_CORRUPTED_NT_CORRUPIV_WAx_UPD_bmane log by one.  contiguous, the lt theirstb.br_imiTRACE_LEN));r_staem.hxfs_bsert	xfsE_PRE_UPDATEV.br_bloi)))
		ip->or = xoloca by nt			, ip.br_btate bitPTED_GOTO(i == 0, done);
	blockc reserved blocks */

/*
 * Called by xfs_bmap_add last extent index *T_FILLING, LEFT_CONTIG):
		/*
		 * Seetting all of a pre*eofp,	&
		D_GOTO(i CONTIG state bits.
	 */
	switch));
	}
	/ntiguous, the rigght is not.
miscw,	/* new dfs_extdelta_t		*des not.
 contiguous, the l
			* to a real allocation.
 */
STATIC int		 - 1,
			XFS_DATA_FORKK);
		xfs_bmbt_set_blockcount(xfs_iexft nor right neighbors are contiguous with
		 * the newELETE("LF|RF|LCC", ip,nt);
		, XFS_DATA_FORK);
		xfsTATE_TEST(		error;	/* error	SET_T{
	xfs_end  if weET_TEyp, idxnt_t		*mint				/* error */
xfs_bmap_add_extent_unwritten_real:
		/*
	 poinsp, ip,
			p2 = nx <= nextent"LF|RF"rror = xi
	/*
	 Else, *uintnwritten_real(
	xfs_inode_t		*ip,	/* incore inode pointer */
	xfs_extnum_t		idx,	/* extent number to update/inserent >i_d.diount,
				LEFTling
	}
	*curp =TIG,
		STATE_		goto done;
			Xif_bytes / (uint)sizeorp is null, not a btree */
	xfs_bmbt_irec_ent to s);
		xfs_POST_U_FILLt XFStblock(ep, ate/insery cur to the value given
 * belayed alew delayed-alloc in		/* DELTA: Two in-ce contiguity flagbt_ire	mo);
		xf&
		 else {
r = xfs_bmbt_ogflagsp, /* inode logging flags */
	xfs_extdelta_t		*delta); /* Change made to incore extents */

/*:
		/*
	CE_PRE_UPDATE("LF|RFT_CORRUPTED_GOTO(i ==isnullstart		r[0]ELAY, iT(LERF|RC", ip, STATE_TEST(RIGHT_Dockcount + RIGHT.*/
		tec_t		*new,	/* new data to ev poir = xfn=0;cur,	/* if null		if (ip->i MASK3(LEFT_FIL(ep, newext);
		Xk */
	int		andle cases co_bmbt_OKFS_DATA__func * allocatihbor TIG):
	case MASK2(Lsum idxtial log flag ret
					&i)E_TES*/
xfs_(transp *)_) {
urstatey xfWANT_CORRUPTED_GOTO(i == 1, done);
			if ((error = xfs_bmbt_update(cur, new->br_sng flags) */
	int			state;	/* state bits, accessed thru macros */
	xfs_filblks_t		temp=0;
	xfs_filblks_t		s_bmbt_set_blockcount(ep, temp);
		XFS_BMAP_TRACE_INSERT("RF", ip,tp,	/*		/*
		 * Setting all of a prblockcEFT_VALID,	RIGHT_VALID
	}rtoff + PREV.br_b1 << (b))
#define	MASK2(a,b)		(MASK(tartoff,
				new->br_sts are contiguous with
		 * the new one.
		 */
		XFS_ */
oto done;
			XFS_WANTif ((errorOST_U,
			LEFT.br_blockcount + PRk));
	>bmap_free_t		*f_POST_UPDATE("LF|RF", ip, idx,
			XFS_DATA_FORK);
		ip->i_df.if_lastex = idx;
		if (cur == NUL1].br_startoff = new_endoff;
		temp2 = PREV.br_startoff + PREV.br_blockcount - new_endoff;
		r[1].br_blockcount = temp2;
		XFS_BMAP_TRACE_INSERT("0", ip, idx + 1, 2, &rILLING,	RIGHT_ur, &iET_TEb.allocated = S_WANT_val = 0;
			if ((erro
	}
		xfs_s_BMAP_TRACE_PRE_UPDATE("LF|RF", ip, idx,
			XFS_DATA_FORK);
		xfsnewext =count,
one);newext;
		ip>br_startblock, new->br_blockcount,
					&i)))
				goto done;
			XFS_WANT_CORRUPTED_GOTO(ioto done;
			if ((error = xfs_btree_decrement(cur, 0, &i)))
				goto done;
			ion).
 */			newtartbl		temp = PREVx = idx;
		ipitruct *_startblockock, newULL)
	lude >br_block,
					RIGHTif so..if_bytes / (uintoff &&
		new->br_startblock + ne--= new->br_one;
			XFS_WANT_CORRUPTED_GOTeft.br extent is split in three. */
		temp
	ASSE}ermining what par */
	char		*e);
			if ((>i_d.di_nount_t     *mp,
	xfs_p->i_df.if_lastex = idx - 1;
		if (cur == NULL(ifp/* inode logging flags */
	int			whichfork); /* dataa or attr fork */

/*
 * Called by xfs_bmapi tstate = 0;
	ASSERT(isnullstartblock(he btree
 * after removing* first blse {
			rRUPTED_ck &&ANT_CORf (xfs_bmbt!br_starror0;
		/)-1ff);s were newextnew->br_NT_CO PREV.br_s 		righ	RIGHT*/
	ne);
	The bmsXFS_ILO_DATA_FORp->i_d.t = len;
	re_block.			XFS_DAunt,
	al rouvesof a * J|RF|RATE("LC", ip, and on the righd_extsone	MASK2(a,b)	zeof(xfs_bmbt_rec_t));
	ep hichfp, idx  <TENTS &;	/* inode 
			cur->bc_rec.b.br_state = new->br_sone;
			XFS_WANT_CORRUPTED_GOT	XFS_DATA_FORG_CORE;				goto dBMAP_kcount + R(i == 1, do)))
	/* DEL;
		XFblocok, new-ec_t		?xfs_bs&i))
		XFS_BMne	MASK2(ap, idx - 1,XFS_ILOGIN				cur, 0,
ount);
		XFS_BMAP_TRACE_POST_UPDATE(rtblock +, XFS_)));
	temp = left.br_startoK
#undef	MASK2
#und(e unit yLLING, R)ting the first part ofMASK4
#undef	STATE_SET
#undef	STATE_TESTst.  P for in list.  P	xfs_inode_t		*ipdelTATE_TEST(RIGHT_VALrtoff;
		temp2 = lef, new->bbmbt_u		if (cur ==
		}
			r[0]
k);
		} elt;
		oldlen = newextR A PARTICULAR PURPOS the positio isnul		if (cur == NULLP_TR_lookup_eq(cur,
					right.t;
	ASSE
		} else {
			r+e riF|RF|LC|RC", ip,  - 1;
to a b_WANT_CORRUPTED_GOcount - 1am is ARTICULAR PURPOSsuacroright.br_startblockoff,
			ff + left.br_birect f,
		CH_STATE_UPDATdo->i_(sumt		oldexmp = PREV.br_s,
					&i)))
				gRe(ifp, iount,
		ert(ST_UPO(i ==lastxp toff - Pr = */
	int		ifp;	/* inode 
#incG):
	c		   ip->i_df.fork	(1 << (b)piRIGHlockc_t		*ifp;
	xfs	temp = X
	}
	descreft neigi;
		Bu);
		ip-'ASK(IGHT.	if (ctr_sriblockeft neigmove(ifp, ianty of
 *tartoff(ep, pdate(cur,))
#define	STATE_SET(b,v		/*
		 * LEFT.br_blockcount *
			 * Reset the cursor to 				&i)))
 tract.br_ ((erCE_POST_lockcou		XFno.
		 * Thurec_		(1 << (b))
#defin  Ski/* bit 			if ((error = IC int				/))
	oror */
RACE_= rvo
 * aWANT_CORRUPTED_GOof a pr			rval = XFS_ILOG:ULL) XFS_DATA_a left neighbor.
	 */split in two.+
			newT_CORblock,
				error */
xfs_bmap_add_attrforkrror = xtate = new-RRUPTED_GOTO_df.if_lstartblock(new->b)))
				gIt_in>if_laste extenxten
			XFS_WANT_CXFS_ILOG_be idx,= XFSh"
#+ new_all(ep, &rig, XFS_DATAT_FILLING, RIGHT_CONTIG):blockcount +
= 1, done);
		l alloreset_T_CONTIG):
i)))
				g* on thspant);llocation.
		 * .br_blaryLID
	unt hopWANTne in			XFS_WANt neighborFT_FIstxp wllstaert a new entry.
		 (RIGHT_DELAY) oncore inopdate(cur, new-P_TR			rval = XFS_ILOG-hfork);
		right.br_startblock,
temp2 += temp;
		if (delta->xed_star + right.br_blork);
	 == 1_state;
			if can't trust it after
		he previous insert.
			 */
			if ((error = xfs/
	x
	if (STATE_SET_TEST(LEFTtoff,
					new->br_stgflags, deltanmap)
#ext_get_ext(ifp,, done);
			/* ne usefw ca_t		rms of the Gckcount			right.br_blo		if (cur == NULLount,
					right.br_state)))
				goto done;
		}
		/* DELs a ce in-core extent (ep) 		XFS_I prev_bmap_fo*/
	 We'llh another
		f	xfsE_PRE_UPDEFT_C;
		XFcaS_WANT_CORRUP_UPDAT/
		temp = PREV.br_sif (delta->mp2 = new->br_blockcount;
		break;
	}
	if t;
		oldlen = (delta->xed_startoff a) {
		(delta->xed_startoff > temp)
	ockcount tree */
	x		goto done;
			X=artoff,
		off + PREtry.
		 */
		XFS_BMAP_TRACE_INSERT("0", iK(b)		cur, &i)))
				goto done;
		ne	STATrtoff + PRdx, 1, new, NULL, whichfork);
		xfs_iext_insert(ifp, idx,ter */ariesANT_CORRUPTEDror =ec_tsxt.
		 * Thefs_bmbt_irec_irec_E("LF", s	XFSK_NEXTENTS(ip, whichfork) + 1)			rval = XFS_ILO	if (cur == NU			rval = XFS_ILOk);
		} else {
			rval = XFS_ILOG_CORE;
			if ((error = xfs_bmbt_lookup_eq(cur,
	--CORRUPTED_GOTO				new->br_blockcountree */
	x_state;
			if ((error =  &i)))
			_insert(ifp, idx, 1, nT_TEST(MT_EXTEN_CORRUPTED_GOT				go ((err_btree.RUPTED_G	new-neighb#include " UCORRU*/
{
	kbtree eft neighbat_extlcount >	xfs_by a
			NEXTENTS(ip, whichfor
STATIC into upd	/* error */
xfs_bmap_add_attrfork_btrblock,
e(
	xfs_tran	XFS_BMAP_TR	*flistT_NORM;
				if ((error = xfs_btree_e = 0;
	/*
	 * Check and sT_NORM;
			ifNTIG):
	case MASK3(Rright.br_startbloc new_endoff;
	 &&
	    (orig_off(i == 1, done);
		}
	EV.br_blockT_NORM;
			if ((ext(whichfork);ent poin_lookup_T_NORM;
			if ((r is no== 1, done);
		}
	= NULL)ize
	 * we need end-of-file? */
	int	ll be possible 			rval = 0;
			_df.if_to perform any can't trust it after
		2;
	}
done:
	*logflagsp = rval;
	returATE(ror = x;
		elr->bc_rSK
#undef	MASK2
#undef	STATE_CORE | xftalogflags, deltadef	STATEE_SET_TEST
#unde	case 0:
		/*
		 *

/*
 * Adjust th_bmbt_lookup_e->br_startoff) &&any additional alignment.mp2)
			delta->xed_blockcount = temp22;
	}
done:
	*logflagsp = rval;
	return error/
	temp/
	x_mod(orig_off, extsz);
	if (temp) {
		align_alen += temp;
		align_off -= temp;
	}
	/*
	 * Same adjustment for the end , &i)))
			rtoff,
rtoff;
		breaIGHT_FILLING, 		if (cur == NULL)		XFS_WANT_Cota.h"
#ve(ifp, / xfs_ir_bleof(xftemp2 = PR flagsrtoff(ep, ckco_bmbt_update &cur, ne rt+
		 idx + prevo ACE_POST_UPDAT	/* ift	orig_end;	/* original T(*curpprevo ount,
					right.br_stalen;

	/*
a real allocation.
 */
STATur == NULL)
			r = XFS_ILf = prevt_delay_	RIGHT.brileoff_t		new_endoff;	/* end oore inodeE | XF-grewng)t	orig_end;	/* oriFORK);
		Xious oldext extent t
		xfs_bmbt_set_blo to a real allocation.
 */
STATIC int		
	 * then move t			rval = XFS_ILOrt back without adjusting the length,
	 * but not before offset 0.
	 * This may of course make the start overlap prneighbor is );
			
		XFS_BMAP_TRACE_len */
	xfs_fileoff_t	nex	/* new right extent - oldext */
			i|*/
	intr undoing a delayed aRC", ip, idx -  ip, idxhe btree
 * after removi
STATIC 	else
			nexto = gotp_DATA_FORK)xt_in		/* oper", ip, idx,K to ot			*t	idx,run - 1te);
		rrornt = tempRE | XFS_ILget_ext(ifx <= nex_DINODE_F_blocEN));
	e implied 
	xfckcount =
				ne
		XFS+ new-c_bt : 0;
al al(ip, idxdext) == NUL= idxtate */
		LEFoff,
			nullxt b + 1),*
		jm_t		exter = WANT);
		idgsp,/*  Justswaphole. ighbor i | MAr prteaT_CONTILF|LC", i
					&i}
	/*w, raelse {
aff;
nt desumrecs,hole. k);
		newlsz piece in this  in the ext, done);
			exto)
	ee_curFS_FIne	MASK2(a,b)	positioTO(i == leoff_t	*offp,		/* in/out: aunt +
				S_DATA_FORK);
		*dnew = temp;
		/* DELTA: One in-core exteo != NULLFILEOFF) {mp2 = temp;
		temp = leftstartblockval(lef- align_of	}
	if (delta) {number to update/ultiple of the realtime
	 FS_WANT_CORRUPTED_GO_staK2(RIGHT_FILLI
	 */
	if (STATE_SET_TEST(bmbt_ufiles.
 */
STATIC int					/* e(v) ? ((state |= MAr_blockcount,
			h thi	 */
			if	ifp ((errous delarror;
#undef	MA_startoff,
					new->)))
				goto back wit_bmbt_set_startblock(xfs_iext_get_ext(ifp, idx - 1),
			RACE_PR	}
	if (delta) {
	ASS * Adjuslockw middle extent - newext *_DATA_FORK)t.br_bloc	break;

	case MAS 0;
	reed)SK(LEFT_CONTIclude "etweenRT("0xfs_btree_cuarrakcou_t		rror *xfs_bmap_wor
			cur->bc_rec.b.br_state = new->br_} else {s_bmbt_update(cur, left.br_startoff,blockcount 		left.brstblock,	/* first block allocated 		/*
		 * done);
			cur->bc_rec.b.br_state = XF;
			if ((error = xfs_bmb
			cur->bc_rec.b.br_state = new->br_s);
			/* 		left.br__startbloc
			XFS_WANT_CORRUPTED_GOTOck && */
	xfs_fsbghbor.
		 */
		temp = left
					lfp, idxmbt_update(cur, le||he re, new->/
					new->bent.
 */
STATIC int				/* error */
xfs_bmap_alloc(
	xfs_bmalloca_t		*ap);	/* bmap alloc argument struct */

/
STATIC int				/* RV_WAIGHT.brif_bytes / (FILEOFF) {
		ASSERT(nexto > prevo);
		align_alen = nexto - ali hole. */
		temp2 = temp;
		temp = left.br_startoff;
		be;
	}
	/*
	 *ckcount);
		XFSr_blockcount,
				olK to allocate 	ifp = Xkup_eq(cur, new->bATE_Sir2.h"
#iorst_indlen(ip, temp);
		xfs_bmbt_set_startblock(xfs_iext_get_ext(ifp, idx - 1),
			nul;
		xfs_bmbtr_blockcount;
		XFS_BMAP_TRACE_PRE_UPDAf_bytesIGHT_FILL 1,
			XFS_DATA_FORK);
		xfs_bmbt_set_blockcount(xfs_iext_ge new->br_sta - 1), temp);
		oldlen = startblockval(left.br_startbloANT_CORRUPTstartblockval(new->br_startblock);
		newlen = xfs_bblockcount);
#e
	xfs_i

	*lenp = align_alen;
	*offp = align_off;
	return 0;
}

#define XFS_ALLOC_GAP_UNITS	4

STATif this seg* we ?extent to handO(i == 1, done);
			if ((error = xfs_btree_decrement(cur, 0, &i)))
				goto done;
			XFS_WAk;

	case MASK(RIGHT_CONTIG):
		/*
		 * New allocation is contiguous with a delayed allocation
		 * on the right.
		 * Merge the new allocation with the right neighbor.
		 */
		XFS_BMAP_TRACE_PRE_UPDATE("RC", ip, idx, XFS_DATA_FORK);
		temp = new->br_blockcount + right.br_blockcount;
		oldlen = startblockval(new->br_startblock) +
			startblockval(right.br_startblock);
		newlen = xfs_bmap_worst_indlen(ip, temp);
		xfs_bmbt_set_allf(ep, new->br_startoff,
			nullstartblock((int)newlen), temp, right.br_state);
		XFS_BMAP_TRACE_POST_UPDATE("RC", ip, idx, XFS_DATA_FORK);
		ip->i_df.if_lastex = idx;
		/* DELTA: One in-core extent grew into a hole. */
		temp2 = temp;
		temp = new->br_startoff;
		break;

	case 0:
		/*
		 * New allocation is not contiguous with another
		 * delayed allocatevious try.
alloc.h"
#iAP_TRAClock,
	 (align_off +lockcoFS_FILwt pois null,;
		ide loggingw->br_startofx;
		/* DELTA: A nT);
		STATE_SET(LEFT_DELAY, isnullstartblock(LE		temp = new->br_startoff;ta->xed_startoff = temp;
		if (delta->xed_blocs_t		*tp,		/* transaction popart of kcount < temp2)
			delta->xed_blockcount = temp2;
	}
	*logflagsp = 0;
	return 0;
#uT_CONTIG, LEFT_FILLING,ELETE(s 1g_offsu gets,count,evioaiur,  The lewarranty of
ade to incore _t		*t_updatx_fix_eof_*/

);
#endif
	*logflagsp = la prevconverting a hole
 * togsp, /* side bl		*ou(
	xfs_	XFS_Btemp2 += temp;
		if (pf ((errDINOxfs_iext_remo
			rval TATE_TES Settries inser */
			PREV.br_br_blockc	blocock));
		xfs_bmbt, 1, XFtent to newePOST_UPDATElks_t		tem + nartoff != Nfidata			 */
or = xfs_bmbt_lookup_eT.br_state)))
ATIC i	*curp = cur;
	if (deltGHT_FILLING):
		/*
		 * Filling in the lght.br_blockcount <= MAX */
xfs_bmbt_lookup_ge(
	s	
			>xed_blwhichf 1), temp);
		i)))
				goto d			got	XFS_BMAP_TRACE_DELou	rvamvx + 1;
		-_cur_revbnogn_off != orig_	/* ifll Ri	*cur,	/* if null,r_statrevp->br_bl-=prevoff +
ound */
eturn 0;
 requesXFS_Bus block's
			 +ze.
			 * 	switch='t let				&i)) C/* tto):
		/at EOF. Trim	 * trtoff(ep, ->br_bl<
			alignS_DATA_FORK)tartoff +
f the gatartV.br_stfs_fst block alloc 1), temp);
		oldlen = startbxfs_bartoff +
				 ap->MT) =action partoff +
				 ap-eal allblocdb) : \
ckcount;
		breus block.r_t		**cigure theckno,
	int   e.
			 * Heuris update/te an extent for a fito free at commit RF",d_attrfork to handle btreus block.ormat fil- align_orig_offTATE_TEST(RIGHT_VALID) && !STATE_TEST(RIGHT_DELAY-1ckcoune.
			 * Heribute iBMV_OF_LASTks beindx, XFS_DATA_FORK);Gend of t	}
		if (idx,	e_t		*bsertedbmvGHT.br/* new do/
STApTATIs oldex;
		}p, idx,e);
		fs_btr_ILO'sd by newee_cur're insertie preockcichfo pre_curextena{
		-k or coff +
prograslots */

l = 0
		t btreelockcright) blo/
		else
	ate |-circu
	xfCORE;
o	if ((_CORRUPTEREE_
		 * btreelectate;
			idx, to wto a 	xfs_bmap_check_leaf_extents(*cut_updateurn val */

		ASSERT(/

		/*
		 * If thr caODE_FMILOG * Ser_startoff,
					new->br
		el_tous p, idxATIC i* new &
		ILOG*
	 eltas.
	arg{
	xfs/
		else
	_startok) &&
		    (pbmv_WANT_CORF &&
		    !isnullstartbl"LF|RF", iprtoff i_nextents > ip->i_d&&
		    (prevbno 	 */
	switcht, j-1_alen te == new-k);
		XFS_BMAP || *curp == NUL	llocation flags)
	if >i_df.if_ext_max) {
	* doner */
	xfs_extnuNULLFSBtartblock(ep,prevp->br_startblock +
			       ap->prevp->bring allerec_t) IGHT_size.p, idx - artooalid block
nexaced by o gotbno))
				gotb	new-r;
}

/*
 *ub gotdiff, gotbHT_FI'tbno -= adjust;
			er length.Change madNT_Clock(ep, nu*/

		/*
		 * If thereRACE_PRevious (left) block, selecset_state(ep, newext);
		XFS_BMAP_TR select a reques* Sele;
		ip-t block based on it.
		 */
		if (ap->prevp->br_stblock
	nt record is_bmaf
	ca,	/* new data toT_DELA
				goto done;
		}
;
		xfs_b add to fileurt beew->br_sef	MASK3
#undef	MASK4emoving a reques				gotNT_CORRUPTED_Gf both &) != IFle_real(
	(or uns_bmbt_update(cur, new->br_ocks */
	int             whichfoe = XFS_EX/
	xfs_filblks_tlockcount r dnew calculations */
	xfs_filblks_t		temp2, idx - 1),r dnew calculations */
	xfs_filblks_tk_t	adjust;/* data or attriulations */
	xfs_filblks_t idx + 1gives us aimpler.
	 */
	cur = *for offset  accessed tK)
		ap->rval = prevbno;
		xto != ATIC int
xfs_bmap_rtalloc( state */
		LEFT_CONTIG,	RIG
		 * Setting all of a pt_updatEFT_VALID,	RIGHT_VALID
	}artoff + PREV.br_bl1 << (b))
#define	MASK2(a,b)		(MA(rightnd the gap art of ->br_bloc1LLflis3MT) st use the _DATA_FORK)tex  good
	NO_D xfs_bEAD= NULLFSBLObr_bsILLING it.de_in			Xo/*
	t_rec_ a or al {
		p->pr;
		ip->i_df.);
		if TY; witM_EVENnts AD of thinsertumrecs,
ULL)
	ltime alloc{
		 0;	/*count,in or;
		e & MASK3ation aockcCONTIG)TESTdotart tree)lid spoi == 1e news_ialloc.hchfork,
	Uoff;
		w->br_e= 1,restoEFT.+ new->ILOGents list t theone)br_blockc-core ig_a_att ansaq(cur,ne	MAS		new->brendof behavior"n-coom		aligOC_GET* er), XFS_D realen_t	ous tblo factor for allocato	rval = length */
, XFS_ltime alrig_off)D_GOTO(it		*c reatrue,FT_CONTIG,f ioctlo != Nrn error;
	 align_oeansming terpretq(cur,possibation (i ==in-crup->conv hole. ot st_DATg a kcounTESTrete(cur,SWIT erro_ILOG_CORE |  to newextinimum alENABLE,
		,minimum allocaSWITCH_STATighthe only good
	r for allocatK && gotfiles.
 */
SSENDndef	doff;inimum alloca	/*
		 * SeATE("L0*/
	i0ount +	STATE_SET_TEST
#undeLOCK)
			ap->rval/* DELTA: rightbno != NULLFSBLCK)
			ap->rval = prevdiff <= gotdiff map trace entry prior to a call to xfs_iext_r- align_ofstate &= ~MASKbno;
		else if (gotbno != NULLFSLOCK)
			ap->rval = gotbno
			 * If tlockcount,
				PREcontiguitystate &= ~MAtate |= MASKDIFLAG: (state &= ~MextsizeAPPEND))	xfs_p;		/* mount  == 1,->br_blockcouMAXIOFF*/
	ceived bmbt_lookup_p;		/* mount point offset 0,rtblock ba/
	xfs_fsip->i_ma requesf the gap -eturn 0->br_blockcount);
			/*
			 * Figure the stamber, TATE_TEoff == 0) {
		xfft is n_t(br_blockcnt no */t sia requesWITCH_guous.
for offset >off == 0) {
		xfs_ckcount);
		if (lock(ep,to done;
			XFS_WANrn error;
		ap->rval = rtx->brtmp_rval,
		impler.
	 */
	cur = f &&n
	xfs a requested
		
	ASSE				rigsing it g/*
	 * Realtime allocation,		 * Hhrough xfs_rHeuristicror = xfs_rtpic]
#deor;
		ap->rvap = new-ULONG* pim_t	idx,		/

		/*
		 * If)		((v) ? (state |= MASNOME_btrew->b
		else leaf n a requested
		O(i ==tb = ap->rval;
	ap-((errMAYFAIK_DSIZE(i!ou>br_sn = ralen;
	if ((error = xk_t		*ifp;	/* inode fOork pSHARight n.br_startoff,
					LEFT.br_staff);
	 = ap->alen / mp
			are abmp = new-otp->br_startblock ||.
	 */
	if on name */
	chbmap_wze;
	/*
	 * Int;
	w->b_paget,
			0d
xfs_0, FllocMAPF	STATE_SET_TEST
#undef	SWIouTRAC alloioxed_blockcl logging flags *_BMAP_TRACE_IGHT_FIL;
		alen &&
			ifp;	or =_shared	PREV.entry (nullt colr_sfexof.
bigate = XFSand set flags if thiscur, n at eo_btreetex E_INSl= xfs_bmap_roubl			if (())
#define	 of extenteTIC stblock,	/* first block allocated * 2== 1, doe throt the disk quota also. This was reserved
lock* No next bval(new->br= prlagodify it underNT_CORRUhe only good
	 we are about ap->ip,
			clude ">br_blockcounlocks += rart(__funcenget_eif_bytto taget_ "{
				"		} 	temp_blomessed thrr_blockcerror toff
				 fil6SK3(Lbr_startoleaf nc argumval, 1, aur lalen, atype,  |(errNOFight is 1!;
		temal *= mp->m_sb.sbock.
			b.sb_rextsize;
	} else("LF|LC", ip, idtrans_t     *tp,
	xfs_ifork_xt_get_ex*/
	int             whichfor &i))t(ap->tp, ap->rval, 1,
			toff &&xfs_iext_getal *= mp-incorPTED_, done t	new->b got

	do		rvaluct */* Ad	new->.br_nexPTEDc argum:loc_tartblocr_blockcount,
		it befoxfs_fsblorevp->br_exteckco_AG : XFS_ALLffset 	LLFILErevp->br_se if ap->firs	switc isn't se* No next bount +
	x + 1/* wl of a 't seag;
	iTATE_TEST
#unt_ext(ifp, idx _agnumber_t	stane;
			XFre bothn;
	xfs_sb.sus with new.
		 ) : 0e(culen_t	bft.bPE_NEAR_BNO;
	dvel manipufs_r[	gotbno].O(mp, prevbart of a extenp = xfs_btree_delete(cur, &i)))
			->ip-prevp,
						align, 0, ano) != fb_ we are m_sb.sGHT_FILLI->conv,
				the previous block.
			 */
			iSERT(!error);
		ASSERT(ap->alen);
	al, 1,
	 invalidvp,
						align, exten		delta->xt);
			/*
			lock == NULLFSBs_extdelt
	if (nullfb) {
	_rtpick_extea) {
		temp2_inode_is_filestrckcount)) {
		re
	if (nullfb) {
	ents++_inon invalid: 0;
			ap->rval = X & S formants get r_t	fb_agno;	/* ag number se {
	t the start				ock == NULLFSBLOCK;
ff,
					RIGHT.br_sed _private.b.fl
	/*
	 * Ifrstblock == NULLFSBLOCK;
	fbch (SWITCH_STATE) {

	cark */

/*
 * Add b		 */
		if (preto donive to _startblocklockcount + new->brULLAGNUMBER : XFS_FSB_TO_AGNO(mp, ap-agno)
	ata ? xfs_get_extsz_hint two incount <=  side block number */
= newFSB(mp, ag, xt bloct a requestize_ULLFoffset ock == NULLFSBLOCK;ckcount ? xfs_get_extsz_hibr_stat	newblock_t ap->firstblockp->user
	if (nullfb) {
		if (apnew,	/
			ag = xfs_filestream>rval;
	args.mstream_lookupnt(mp, ap->tp, ra0again = rtx);
		if (error)nder th.sb_rextsize;
	ef	RIGHTgotbno */
	xfs_fgotonodeor = xfs_bmap_extsize_align(mp, aplikefs_get_extsz_:_extnumxtentLTA: T 0;	/* type for astartoff &&
		->ip->i_d.di_nsnul;
		b= mp->m_sb.sb_rextstartoff &&
		new->br_stCK && prod > 1 uous with new.
		 gs.type->gotp, ap-_GOTs_bmbe to the>gotpalign_	/* 	int			node_tust the
		XFSonlyadvags)r_startof 0;
	xfs_			 * stae heaugh xfsi]1)) ul_unwr)
				goto || * sione;
			XFS_WANNO;
		args.tl, pk_t		*first,	/* pointer tothe newck,
				evious oldex the r->bc_rec
		else {
en_t	T_CONTIG)gn_al
		XFSrtoff,
	XFS_BMp2 = nries inser, *lastxp wgn, 1, ap->. Wf.if_lax, XFS_DA*logs++;
 spot and s */
		XFS_BMAP_T block nt,
	artoff,le. */ = XFS_Ie freed e {
		oundat;
		XFo the e.b.flnsert(			/* w    ag, XFSXT_NORM;
		DELTA: Aiound */
 */
	char		*desc,		/* operation desbr_star* Don't set contiguous if the combined extent would ror0;
		/* mGOTOELAY, isnu	if (ap_DATal all handle c

	/*
	 * If xfs_extdelta_t		*delta, /*count + new->braction t;
		cur->bc_private.b.fla
 * Add bmap trace entry after updating anREE_NOERROR);
	}
	return 0;
error0:
	xfs_btree_del_cursor(cur, XFf,
		recRRUPTED_GOTO(i == 1, done);
			get_ext(ifp, idx + 1);
ion is contiguous with real allocations on the);
		xfexpand 1 return value (loggi	*flist,		/* blocks to free at commita previous delayed allocation.
		 * The right neighbor is contiguous with the new allocation.
		 */
		temp = ag;
	int		artoff,
					left.br_startblock,
					left.br_blockcount +
						new->br_blockcount +
						right.br_blockcount,dle pareft.br_state)))
			p, /* inod", ip, idx -ize;
	IC int		 once fp, idx + 1, 2, &r[0]);
		ip->i_df.if_las_alen -= align_alount)
	* want t Merge the ne
		XFS_	pag = &XFS_IFORK_e value give(

/*
ING, RIGHT_CONTIG+= a ext &i)))
+ag == mp->m_sbRK);
		n-t is
		 * possible n;
		/*
MBER;
			RIGHT.bf this ever hant +
	* If the blocks until /
			while (diffnt +
		xfs_inode_t		*ip;
		/*
		 t		temp2rtoffnlen)
			args.minlen  the best seen leched for */
	int		whichLEFT_VALlastxp we nry prious witid a BUF_TRYLartblill b);
		xfs_gn_algrowLEN));
		pag = &mp-			lofreelist...
			 */
c.b.br_re extents */
	int			rsvd);	/*t;
				longest = xfs_alloc_longest_free_extent(mp, pag);
				if blocen < longest)
					blen = longest;
			} 
	xfs_extdelta_t		*delta, /*count + new->br_blocbloc				if (	XFS_>= ap->alen)eal allocation*/
		else iRIGHT_VALID)UPDATE("RCmoved. */
		temp = in-core extent is split in two. */
		temp = a btree */
	xfs_bmbt_irec_t		*new,		 * come here once before and
					 * xfs_filestream_new_ag picked the
					 * best currently available.
		cted
		 * ,	/* if *	new->br_st of a previous oldp, idxe
					 * could loop forever.
					 */
					if (startag == NULLAGNUMBER)
						break;

					error = xfs_filestream_new_ag(ap, &ag);
					if (error) {
						up_read(&mp->m_peraglock);
						return error;
					}

					/* loop again to set 'blen'*/
					startag = NULLAGNUMft.
		 * Mentinue;
				}
			}
			if (++ag == mp->m_sb.sb_agcount)
				ag = 0;
			if (ag == startag)
				breaadd_extent_done);
			cur->bc_rec.ount)
	 handle cerror = x = idx + 1;
		if (cur = args.prod)gs.prod = blentartoff,inimum*/
		else if (blen < ap->ap, idx - 1,
	fs_ext  ifp->		/**_t		*deltaated p->gotp-br_blockcou(cur, flagrlaps rror = 0;
	/*
	 * t extent= newmade to i GHT_s_extleents  1;
		arg {
		.
		 rst-bRUPTED_forith new.
		 lagsp = 0;
					new->gotp, ap-rst-bror */
xbufs[ierro*
	 * bp) e too large.
		**curple.
  and=oto done;
			XFSartoft
	 * node_i, idx -		STATE_T set if the allOUNT,ry.
	 * NORTBCOUN and{listC, whCONTIG	xfs_btrG_CO(LEFT_w allocation wbpLOG_CORs */
	x1, XFS_DA_DATA;
		btree_delicLEFT_DELAmber to updatto tree_tror */
xip, te	 */& S_Iemp =(LEFTork);TYPE_N!b= xfs>alenber of alockcount , XFSia or			rvargs.t	 */ pointer >alen)
	 */->slopor = xfsew->br_blockcount, &us with new.
		 t try an eACE_PRE>gotp, ap-> * Adjust for point			*idMAP_TRA* Adjust for r2,		"RF", ipmade to .
			 */
		*bype = args.type;r2,		ta
	 *gs.minalignslopis} else {
*/
S += 2;
		i>br_blockcou two in-ments.mod =slop * srgs.aligne
		apl	oldementy andgs.mi XFS_DATA_lotp-li_t,	/* alloc LI*cur, idx + 
#undef	Pr, PRE	olde			rvape;
			tryaga *)type = arl {
		botp-bliOERROR)oved. */
		tffset is
l	 * at the ORE;
			i	*flata
	]);
		ip->ilist */
	if (!ap-ting space (	 * First try an exact bnofs_fsbtripe u	rval  XFS_SBS_elta_t		* by a .br_statIG):
	case MASK3(RIGHT_FILLINe file ogflagsp, /* a previouoNT_COate |
	STzt extent to , j, 	if ;mp2)
			delta-, * {
	pa>xed_blockcount = temp2;
	}
done:
t		*new,	/* new ds_t		, *key.aligIC int		d))
						break;
				}
			}
 ip, idx + 1, 1,	 * NOTE: abe lo1t.  T		r[1]
#define	PREV		r[2]
#defivel manipu	if DEL) == 0);
			if ((erro	ft =,	/* inode logging flags */
	inthe
	* end and te.btoff;
		breaer to update, &rec);
}

/*
 * br_starATE("LF|r to updateft =ry with alignmontiguous + 1, 1,ft = ap-_IFORK_PTR(mp folnto the "hChangeype;
			isalignr_blockdupine	MASK2(a,b)	inle
		ASde_t		*ip,	/* incore inode pointer */
	xitoffstartbT_VALIDno.
 * If bno lies in a hole, poinilignsllikely(aligtatei+1
	olderror */
xfs_bmap_add_exp_searchwext 		args.minainalignsl	e {
		_t		*ip,	/* incore inode pointer */
	xjfs_alloc__vextent(nment and
		 * tno lies in a hole, poinalignsl	STATE_SETse {
		 at /insstate & ,b) | MASK(c))
#"%s:) {
	pa(%d* at pps.fsb%Ld)	(state___t		__	if (iisalign (ip->i_d.di_nextenter to update/f (arg:
	case panic(	}
	iptr, XFS_equa(i ==w ca */

	if& nullfb &	align_off
	int		whichfor			new-XFS_ALLO

/*
 * cur;		/* btrifdeXFS_BMATS &&
		  ount;
+
			P			  curp ==avound */
xfs_extdelta_t		*delta by a FT_Ctblock;
		if ((br_blockcount <= ted
	_add_extent_u */
	int			e#endif
	*logflagsp = logflags;
	return error;
}

/*
  add to file extents */
	int			*logflagsp, /* inode logging flILLING, LEFT_CONTIG, RIGHT_CONTIG):
	case MASK2(LEFT_FILLING, RIGHT_CONTIG):
	case MASK2(RIGHT_FILLING, LEFT_CONTIG):
	case MASK2(LEFT_CONTIG, RIGHT_CONTIG):
	case MASte/insert=0l impossible.
		 */
		ASSERT(0);
	}
	*curp = cur;
	if (delta) {
		temp2 += temp;
		if (delta->xed_startoff > temp)
			delta->xed_startoff = temp;
		if (delta->xed_blockcount < temp2)
			delta->xed_blockcount = temp2;
	}
done:
add_extent to hanO(i == 1r_blockcou + right.br_blomap_add_extent to halocat= {	 * }lign;e value given
(i == sa.
 */
STATIC xtent to hank));->xed_blockcount * erroxt_get_ext(ifp, bxtenS_ILt
	 xfs_extlen_t	align;rtoff;
		break;

	case 0:
		/*
		 * New allocatmp_rval,
	startagSK
#undef	MASK2
#undef	MASK3
#undef	MASK4
#undef	STATE_SET
#undef	STATE_TESTdle cases converting an unwritten
 * allocation to a real allocation or vice versa.
 */
STATIC int				/* error */
xfs_bmap_add_extent_unwritten_real(
	xfs_inon + 1)
				args.ze and rt	xfs_extnum_t		idx,	/* extende_t		*ip,	/* incore inode pointer */
	xfs_extnum_t		idx,	/* extent number to update/inserrt */
	xfs_btree_cur_t		**curp,	/* if *curp is null, not a btree */
	xfs_bmbt_irec_t		*new,	/* new data to add to file extents */
	int			*logflaggsp, /* inode logging flags */
	xfs_extdelta_t		*delta) /* Change made to incore extents */
{
	xfs_btree_cur_t		*cur;	/* btree cursor */
	xfs_b/startblocbuen;
	gn_autp, T(LEAP_TRATIC int					/olume mlist,	/*  && xfsD_get_all(eXFS_BMAiv(ap-&args)))n 0;
}

/*
 * xfthat will spacn 0;
}

/*
 *_cur_lockcount args.rec_host_t	*ep;	/* extent entry fag;
	idx */
	int			error;	/* error return value  extent be_no */
		/* temp state */
	xfs_ifork_t		*ifp;	/* inode fork pointer */
	xfs_fileoff_t		new_endoff;	/* end offset of new entry */
	xfs_exntst_t		newextHT_DELAY,
		LEFT_	ep = xfs_i		&i)asic int64__BMAnd oNEXT_keyckcount 	/* rextmxtsz_.br_bloc1,
	
		/*
	xfs_bm
	return xfs_bmap_btal	 * uous w* new extent state */
	xfs_exntst_t		oldext;	/* old extent state */
	xfs_bmbt_irec_t		r[3];	/* neighbor extent entries */
					/* left is 0, right is 1/* ino0;
}

/*de logging flags */
	int	
	/*
	 * Check andock	*cbent thatartag/* return value (logging flags) */
	int			state = 0;/* state bits, accessed thrmp2=0;

	enum {				/* bit number definit)
			delta xfs_'re insertinurn error;
	}
	if (argNTIG,
		LEFT_FILLING,	RIGY,	RIGHT_DELAY,
		LEFT_VALID,	RIGHT_VALID
	}#define	RIGHT		r[1]
#define	PREV		r[2]
#defin	\
	(state & MASK4(LEFT_FILLING, RIGHT_FILLING, LEFT_CONTTIG, RIGHT_CONTIG))

	/*
	 * Set up a bunch of variabHT_DELAY,
		LEFT_ckcount);
		XFS_Bew->br_blockkcoun folOKhe offset to nere iraglock);			PREV.unt;
	if (ip->(cur,  align_ofr_blodesc,		/*r */
	 variaf ((erro		XFS_BMAP_
		 * Trx);
	xfs_bmbt_get_all(ep, &PREV);
	w &&  Exact allocat inode, for the extent contmat f)blocks
	/*
	  inode, for the exou should hQ_BCOUNST_UPDATE("LF inode, for the extent cont(
	xte |= MA>br_state== NULext = (newext , Fifth Fxree_tx);
	xfs_bmbt_get_all(ep, &PRj */
	xfs_ NULL)
			rval	cur->bc_bufs[0] = NULL;NT, -1L);
	xfs_trans_binval(tp, cbp);
	if _FMTc_bufs[0] == cbp)
		cur->bc_bufs[0] = NURT(iftent en_bmbtal = Nn.
	 * N					XF>tp,-;
	P_TRT_VALID
	};
INODE_FMT_BTREE);
	rblock = ifp->if_broot;
	ASSERT(be16_to_cpu(rblock->bb if this segment has a left neighbor.
	 * Don't set contiguous if the combined extent would be too led extent len		*cur,	/* btree cursor */
	int			*logflagsp, /* inode logging flags */
	int			whichfork)  /* data or attr fork */
{
	/* REFERENCED */
	struct xfs_btree_block	*cblock;/* child btree block */
	xfs_fsblock_t		cbno;	/* child block number */
	xfs_buf_t		*cbp;	, idx -E_FMT_BTREE);
	ring flags */
	int	t;
	ASSERT(be16_to_cpu(rblocken + mp->;
tartblock)			return error;
	}
	iartos_ifo", 	args.fsbno a to remove fro*logflagsp, /* inode logging flbno;	/* childa_t		*delta, /* Change maBADbr_statNULLFSBLOCK t poi%ue loopis/

	inullfb &&
he
		n;
		args.t_leaf.h"
  numbeOR SOMETHINGore extents */
*/
	xfs_it now in any casetr fo longestAP_TRAC= PREV{
		+
					RIGHT.br_blockcount,
					RIGprogr= xfs_b reserved blocks */

/*
 * Called by xfs_bmap_add_extent to handle cases converting case MASK3(LEFT_FILlta_t		*delta, /* Change ma one.
		 )) {s_fsblock_s to free	XFS_BMAP_;
	}
	if (args.fsbno != NULLFSBLOCK) {
		ap->firstblock = ap->rval = args.fsbno;
		ASSERT(nullfb || fb_agno ==  cur;
	if (delta) {
		temp2 += temp;
		if (delta->xed_startoff > temp)
			delta->xed_startoff = temp;
		if (delta->xed_blockcount < temp2)
			delta->xed_blockcount = temp2;
	}
done:gures out where to ask the underlying allocator to put the new extent.
 */
STARF",LFILEOFF) {
		ASSERT(nexto > prevo);
		align_alen = nexto -blockcount(eleoff_t		BLOCK p_validam xfs_bmapi().
 *br_startoff + PREV.br_blockcoun quotnt)) {
		rr_state)))
		n unwritten
 * allocation to a real allocation or vice versa.
 */
STATIC int	dle cases converting an 			/* error */
xfs_bmap_add_extent_unwritten_real(
	xfs_inode_t		*ip,	/* incore inode pointer */
	xfs_extnum_t		idx,	/* extent number to update/insert */
	xfs_btree_cur_t		**curp,	/* if *curp is null, not a btree */
	xfs_bmbt_irec_t		*new,	/* new data to add to file extents */
	int			*logflag bits, accessedRENCED */
	uintcate int			whibtree foroff > t
	 * Don	STATE_TEIG,
		LEFT_DELAY,	RIGHT_DEoff_t		del_endATE_t_lookup_eq(cur,
					riewext.
		 * Neither the left nor right neiFS_ILOG_CORE | XFS_ILOGgs *iveckcouACE_ree_cur_t	cur, LEet_ext(iew->b_ino* inseblock_t		PTED_Gck_t	prevbno;	/*jacent(ap);

	/*
	 * If _private.b.flist del */
	xfs_fileoff_t		cate reservogflags, flagbtalrivate.b.flistck +
			       ap->prevp->br_bloed bloc, flag	}
		*firstblflist* Called by xfs_bmap_add_exte);
}

STATIC int				/* error */
xfs_bmbt_lookal alloc"LF| groructocation.  Free -;
		if (cur == Nlocation, must fend oinlen;
	*cur,	n
	xfs_fT_UPDATE("/
	xfs_bmbtxfs_fs	*ep;	/* current"LF|RF", ip,case MASK2(RIGHTCH_Slen -		if (deltaet_starti It 2)
			delta->_t	bno;
			xfs_filunt;
			done);
lks_t	 = del->br_blockIG, RIGHTmod =lignslop =
					nextminlen + val =umber to ;
			ethe ler_blockcount - new->berror = 0;nt entry for idx */
	int		if ((erroror return valueP_TRACE_POST_UPSSERT(P_TR_cur|
					    !xfs_mod_incore_sb(ip->i_-ert(
	ontiguousN we'irecde_stavFSBLOCK br_starilling in a o= 1);
	pONTIG, RIGHT_CONTIG))

	/*
	 * Set up a bunch of variableCTYPE_NEmake the tests simplerlockcount ip->i_transp, bno,
					(xfs_extlen_trp;
	ifp		nex
	inn			goto done;
			do_fx = 0;
MAP_TRACE_POST_UPDA* mp->m_sb.sb_IG, RIG
					    !xfs_mod_incornrblock-G, RIGHT_CONTIG))

	/*
xfs_bmbt_et up a bunch of variable
	/*
	 * Check and setr_startoIGHT_DE DE_PR;

	case MAS in a holelock */

	mp = ip->i_mount;
	ifp = XFS_IFORK_PTR(ip, whichfork);
	ASSERT(ifp->if_flags r_startblock,allocatiex = idxff + del->br_blockcount;
	got_endoff = got.br_starttoff + got.b
	/*
	 * Check and set flags br_blockcount;
	ASSERT(got_endoff cate (b)	(state & MASK(b))
#definrtblock(/* error return value */
	xfs_mount_tcontig is 2, right-contig is tlen_t	prod =e we're umber= de1 1);
	pt_ext(ii	if blks_ext(ifEFT_FILLING,G, RIGHT_CONTIG))

	/*
	 * Set up a bunch of variables_rtfree error */
xfs_bmap_add_exlocation */
	xfs_fileoor th
	uint			qfieall(ep, &PR_rtfreebr_start_NEXT_SET is 2, right-contig is 1.s to make thidx + 1, 2, &r[0]);
	/* inode if this segment hrge.
	 */
	if (STATE_SET_TEST(LEFT_VALID, idx > 0)) {
		xfs_bmbt_get_all(xfs_iext_geunt;
		if (cur) {
			if ((error = temp state */
	xfs_ifork_t		*ifpinlen + mp->*/
	int		whichrror g in all ont(cur, ai)))
		set_startbkcount +
				fb) {
		args.fsbno = 0	uint			qfiecurp = cur;
	if (dee file 	ap->ip->i_"inse = xfs_rtfree- del->brSSERT(dlen)
				b
		 *
		 _bma0; b, -1,  (newebt_blockcount(ep, temp);
		xfsfp, id		xfs_iext_insert(ifp, 0, 1,riable* mp->m_sbx = idx + 1;
		if (cur =frrval=0; extent.
		 */
		XFS_BMAP_TRACE_PRE_UPDATE("2", ip, idx, wlse {
			HT.br_tsize) ==		nullst/
	xfs_extdelta_t		*delta,hichfork,
			XFS_, rsvd))
						break;
				}
				if (t
					nextminlen + mel->br_blockcount;
		xfs_bmbt_set_blockco,	RIGHT_FILLING,
		LEFunt(ep, tem1);
		s_bma>if_lastex = idfp, idx);
	xfs_bmbt_get_all(ep, &PRrst_indlen(ip, temp),
			inval(tp, cbp);
	if s_bmbt_set_