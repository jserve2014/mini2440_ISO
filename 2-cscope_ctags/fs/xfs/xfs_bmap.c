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
				whichfork);Copyda_new = temp000-2breakraph}
		XFS_BMAP_TRACE_POST_UPDATE("1", ip/*
 *  ight (c) 2raphif (!cur) {00-2flags |= xfs_ilog_fext( free software2cs, Inc.am iA; yo(error  it anbmbt_update(cur, got.br_startoff 00-2r* published blockthe Free SoftFoundcount - del-> * This progration.
 *
 * warte)))00-2goto doneraphhe term
	case 0:
		/*
		 * Deleting atiomiddle ofenen txtent.TY; w/
		on G = is distrware Fy  - ee Software Fy raphll R fres ReservRE
 ram iTh0ted in amLITYf.
 *shat itel PSoftc setributed in (ep,cANTAtwarenewARTICULAR PUBILITY_end PURPOSrecei = A Peraailsicense along .
 *a copyributed in ai received ; if noof tense
 *e This ae dethe Gudelayn redisware FoundahThis NU Gen aloThis raphitribute iE.  ILOG_COREraphihe G ca, FifthatioGA  02110-1ils.
 *
Liction as
he Frhope that iteR PUation..h"
#include "Inc.,  recefs_bit.h"
#include would bebe useful,am ibfs_trans."t antypes.tree_incremente "xf 0, &i"xfs_trans."xfs_sbe "xfscur->bc_rec.b = newe "xfss_da_btage "xfs_transer_dir2.h&inder tfs_da  0211&&   0211!= ENOSPCude "xfs_da_btree.h"
#ANTY; * MiIf get no-space back from xfs_t loc_btinclud *ral trioftwasplit, and we have a zerklin_da_bon, MAr
 * Gationf
 *ncludFix up our c.,  inclureturn warra 021 "xfs_daERCHfs_da_btiall="xfs_alloce "xfs_trans."clude
 *t warrcursor,fs_s's_inustae "xfs_tal Pfter anyee "xfs oper"xfs_trans.table.h"tinclude "xfs_types.h"
#lookup_eqs_da_btlude dir2_data.h"
t anlode "x
#include "xfs_inum
#in.	"
#include "xfs_da_fs_da_btree.h"
#SE.  WANT "xfRUPTED_GOTO(is_tr1eaf.hes_transdaude "xfs_dUncludewarrdinoderecordtransde "xfs_dto#incloriginal valueode_item_dir2_data.h"
t anext for#inclde "xfs_da_btlude ocude "xfs_filestreabmapude "xfs_filestreartall


# General uted in deops_dir

#ifdef De "xfs_tran Gene "xfs_trans.de "xfs_quo"xfs_da_btanty oxfs_arans_"
#in
#ifdef DEBUG
STATIuf#include "xfs_filestreailss ofam iYou sh"
#in_btrfs_bmapfs_trans_btree_cubmap_add*/
#in= 0map_add  02110-1l RERROR(fs_tranmap_adds_da_btree.h"
#of tt		*xfs_ttr_ls_rw.d
xfs_bmap_check_btree.hnc} elseddrk_ex
#inclul Pud/oram imodify/
	xunder te "xfsORK_NEXT_SET( in  free sofmit an de "x pointerENTS	t anfsFound_t) + 1RRAN nsacti FifthASSERT	*ip,		/* _zon*/
SDATA_ck,		xfs_eANTABILypes.hap_worst_indlenblockreceiSoftne
 */
/


gramBost"xfs__btrenulld "xfs_t an(int)g flammde "/
	ition		*tribu);xfs_tblocklogg evefl2mmit * Floor,d from xfsA  _aon poin(c)  to haode  or forx006 Sile Free +on.
 *
 * _fsble (transac> da_oldfs_dir2_datag flastree._dANTA--map_addtransallocate		Called* Calle_attrfork tbmaleaf_ex_local(
	xfs_trans_t		*tlocal f_local(he Gtransact=	xfs_incfs_trahe terms block,	/* 2e,		/*block a2block,	/**firstFound,xfs_s_bmap_add_attrforSTATIC v_"
#in by xlist,	xfs_blp,		/* _localof tof tE.  See the
 * Ged */


/*Thes.h"
#include for more detE.  See the
 * INee
 **
 * STATIC  for,		*i&new, NULLblockt				/* error es.
iexIC i"
#irif_de "xe btripto upi 51 Frap->if_lastex = extent .h"
ut WITH(or cludrfork_we need to, adull, e
 *ode.hbmap_sa btdhoude "xestret como_fxtribles.
 */
loca for(ITYtp,	FITNES
#incm.ITY or F
#incd in te for blockflags r tof *AdjustIC int	#_da_btr "
#ioc.fil	** inxfs_bewnblkstribi/ins_d.di_nthe btr-= tstranvariables */

/*quota ude ist of extenqfiees.
 *l Pu_item_mod_d fla_byino(te thp, ae btr, (long)-agsp, THOU inode lcite th			/changde.hby xaypointdirec#incl by * incooth evenxfs_xfs_tdisk erragsa k_ex  bloherlist of etree
 *nt			* >	int	tranupd new miT ANsconvertielta, /gthe ncore_sb int		l RSBS_FDBLOCKS, ans_64_t)		*tT AN- allocatoggingsvd evea is ion ags *//* DELTA: reporestreatypes forxfs_bmf_check_eal(/

/->xedfs_fsbS FOistrPARTICULAR PUlled mocat
 * mit *nty o onsb.h"erms,AR Publicxtnut */
	xfs_bt, write th<umber to update/+ap_local(
	xfs_transerl formed */t, writes_logmber to update/ +entsam.hents(file e
#incucurp,p,		:
	*logt,	/*int	t,	/*;o fileial
	xfs;
}Calle_zonemovmap_adentry " new"_da_btrt,	/e "x.tem-all .  Prevk,	/blic i ther topreviousk /* i, unlessnode logiATIC vheadode.loc.xtent(
"xfs_inodevoid
les.
 */
h th new 
 adull, fiexteocatee to rxtennd the btree
 int		encore  madede tal alocat_t	*be f form/

/r nulocateonlled , ifk(
	xlist,ate mapi.hre Founds *exte)xtenflis*re extenb and tade t{y_real/

/*ayede 	idxbf	intxatioextemit * Gene*;
	ap_fonde to mit *_xfs_	/* cords anloca* new n Stm_t		s_d in Foundkmem_zone* Chanandle cases convertocat,		*ip)k,	/* fi toConvone_alogginbmb-formadle cl into ata.h"
f extens */
 for ThLITYents */willyed-all.roadd  hanRRANrds anIC v)inclua sing*/
xhildestread to fileextdint
#endifinode l*/ocatedion 		*new,_tobmap_ana readidx,	/eree_p,extenidx,	aincluist of add  handls_extertini/Calledalp_adords an		*ip,	/* incorefsThis rcks *e "xInc., formae "x-This -_add_excrtalleserv reserved cks *ew d *al(
llocateadd_exin xk alby ip,	/* infs_trcurertingcurle case*/
	xlble.h"
_add, callmap_addhole_rwasdelt */
	xf exte bloa	t_de
STAd_exticensreederrtingto fiinoderds an				 blot,	/* , not a bttfree softal(
/
	inory xfs(c) ktallo
 structude "xfs_alThis 	*a301  Ual(
d_ext_t		*(, /* ) b/* erragip,	/* inufertinabp;t		*fiuffeconv	 ibut*/a reserved_ext_argertiargse_real_extdelore arguange xtentypes.h"
#recerved rree_cur		*deCree_ite		*ip,	/* ingsp		*deC int					/* errwhich_t		*finclude		/* erragtblock,	/* filocation currtall omapts *vicxfs_btrip,	/* inc_cur_tohosttingole_realrds and theite		*ip,	/* inhole_rmap_* eal(
	IC in		*firsincluunentsteor *nurtin	i, cint	number tont_holinoindex/* incore t			ans_t	ft		*ip,	/* inp is*		*ip,	/* incoreextnukeyertinkree_cure versas ofkey	*new,exteecocatem in ertinmree_curogfla  * allurmb	xfs_ing  not a bt_extntero filnu */
ode.ts */		*new,tint			*lst ofptointer ta	xfs made to lellocrcks 		*ip,	/* iy_reint	stblock,	/PTRFoundp_add_extd;(
	xns_t	stblock,	/FORMA/

/*
 *llocatedrtine torINODE_FMT_nts st  figures ou data to nummax ==
	 xtdeltstblock,	/SIZEhe* incolylay_rl/ (uns_tsizeofbtree_ceal(
	xlocks riable Make bmap_a made to inces  allist of e	xfs_dd t_rt		*deinter1nse for more denot a bt*/lude "xfs_daFBROOT		*dets */oF,/*exten	xfdd tist of eattrforknot a btbdd t;s of Si->bb_magic = cpup_adde32ut whSee tMAGI	RRANs de "alreleveln-t_ho,p_ad 16pointeive up thenumg flntsors_logd-all roo	xfs_ pitchu.l.atiosftsis_exor the bt64(e/inDFSBNOinode	int	mmit * Gene_fsb		*ip,	/*d-alloto/* new deo filep, /
 * CNhanda		*ip,	.  Ca
	xf_extdelt 	*nels_log"
#inisdd_elumber st of e	int	led by logg;* trahis program inal forsortalloole_real free softwar_zone_t	private.b.ogfla	xfs_BILI/*
 * Callinto  */
	i				k
 * C/* to afsbis* allocextde callee_real_real(l form ?	*newBTCUR_BPRV_WASDEL :s				riable s */
	inlloc is ca with two 	*curs, on"xfs_aranin/* new call.
haveer reseM				/* errderlying 	catbloco puts_loBTREEtent				.tfs_fsGraprrent	int	  pointer irst block albyrecords ai at coby xfs_bmapormae/inFStranst				/nter  ypole_
 */ALLOCTYPE_START_BNOraph is nusbno_dirn exNO_TO_FSBtalloled byin to gds and he Gode pointerlowthe btree
 * adata xfs_bfter reocatoCalled byocation d-alltent number tods and the ,	/* new data to add to fiNEARreal(
	IC in/
	xfl logg * alocation C int			inlen = exts.maxght (c) , /prod =loc r*flistot#incfge madewhifs_tr attralig/* ex;		*deOK ile e attrisflnt to/* OK toases slo/
	xffcate re	*cur, ng	*cur,;verting*/

/*
 freenclude "xfs_types, intovvd);	/(& att))t				/th only bmapents e "x,-*ip,	/* hn,
  exTATIC int		, /*allocatir2.h"
#inoREEs_inod	xfs_by xfs_bew daprmber tole *rst block ct				fail,s_extbmap_at */ber .hencore ingures ou
	int			*lo!o file s and tent.
 */
Sw data to /
	int			*s and  ||_extdeltadle cag extata toFSB	*cuAGNOtalloby xfs_bmap)_cur_real(-fincore e
	int		 &&um_triab call>o add
 */
S	/* newnode_t	on).
e nocks e freed */

/filepdate file extenrst block al
	int			*li to update file exte_extdelta++ frets */
	ine made t_free to idx,	/rk); /*_extent_hole_real
 */TRANS_DQ_BCOUNT, 1L* curbe made flfs_trg/*
 *fl/* OK to  to file ext0gsp, /
 * Cew file wi f/* incore inoxtent tight (ata toBUFree-trans(ablags 	whichp theady ie for the btxfs_btreseful isck.
gurncationch"
#d);	  for rans_t		*tp,	/* te_t		*ip,	/*inter */
	xfs_inode_t		made to incore w daitem_hole_dIC int				/* err* extent 	arree
_curBMBT_REC_ADDRtallocde to,cationsvd);	/)nc */
	xcallytes		*ap
STATIonvep_add_ed byxfs_			*oor (cFoundihange i < rt of bo; i++t				/
	int		*fp is  It _t		extnum 51 Franklis_local(
	xfs_trts */

/* It allocated */
, Bostclude ->l0ogging flags */et	*tpmmit *chfo*t1 after_itemfork */

o file the++;hange mt				/}fs_bmap_fon't ee_cur_
	xfs* new ex a file.
 * It d sooundrror *
/*
s_trale(iformde cguresde, to addks freed in* neck,	/BUG
STATIC o file e_tran needed bKEY block.
 *otal,		isxact	/* c needed by t block.
 *total,				whikap_ad/* extent gging flags */ts */

/*ck.
ocks  */
	off(arthe expfork);	/* datPTRersa.
 * Change ma,types.h"
# It maxen_t	vnodeops.h"ile iallcpu( is d);	/*/
	xfs_inodeno.
 ing flags */
	int			*ltotalonvertiDonts  thisents */

a	xfs_bmnd srealat).
 */y_t	*ip,ise, *t */	*ip,t		*ip calleerrororror *inod		*firsr2.habe pointBBo ad_BIk,	/* tpsp,/*ormatai.  If bnoy
 * C1, ast eofsb.h"ade to incoxtlen_tn xacemC int	ate/ the
 * inodert a lk,	/i file e_del_frpnclude _bmap_a|
	xfs_inodeafter	*ip,		/* inco		*firs0*/

/*
 * Coalcust,	/*ncldefaulthange
#inee(
	xfoferror/
	xnewly cre_t		*ip,	/to the
dataocateo updat_s_in*eofp,(nck.
_add_exs_bmap_t		) incore llocati loggmbt_ made to incore  dataons 	/* ot enhe Gmp->mtreesbnfs_bdneeds_bm256t				/ound *	 the
 LITI chil) -ocal(
	xfBMDR_SP
 * CALC(MINABTPTR,	/* 	/* liste_tusck,	/* fo adckxtlen_exteino6xtenafter re/* coe
xfs_inodeus exte<is alry fhis  *rcare 			/*f the /
 */
	xfs_Helper routinetranber cords andihforkrsvd_extnuwhen switce Fo_ino file extinode_check_ */ltranConverte exten-rp ihe ral tnot rile exossibl at tmtion).	*ip,vailat bo/
	xinlncorbt_rec whit of bo made to incoion 		rsvd);	/y; *ghi_ew ble inodeo incore** dat */
	xfs_bmat		*t the
 lloc is ca*
 * Cal after alllocatorATTRa new fil(* we Convert a e exten!handfs_trans_t		*DEVr to up into  */
	i
#inclvious datcharile eofUUID	/* return value */

#ifdef XFS_BMAP_TRACE
/*ta or atedie enddflry; *ghicor as.s nul the xa or aouip) >> 3t enhe ns s_extdelta_>rn value */

#ivoidt				/*nam file AP_TRACdeNU Ge_extdeltazone_tts */
f.rans_t		*tp,ocal(
	xfock,	/Docode_tts */
edocatoget(
						e xacore numbafs_trans_t		*tp,TATIC int				/*Ato incis nul	xfs_nt			*dexo add end(ede po

/*
 * Cona root  *asdewts */llococate res/* Change matota/
STf*/
S*
 * Chis sle casee_t		*isode.regula */
	is,ata sinincludeattr fe_t		*nu extenr_chfor *gollta, /and /* stay * tsisr */
dex (xfs_Conv-lloc *manip to ionive upoum.hhough)p_add_extent_hole_re* Change madeur,	/* ita orast map_isae cases convertap_extents_to_btretent number* incore inoff,		/* fed
 *ormatocatay_reholel_fr and xtnuast extby xfs_bmapfs_trans_tser toed/* inode ree_cur_t		**cock,	extl.  Ifur,	/extentsr,	/*llocatefrom point inode pointt				/* e* to whichfork);	/* docate resle extents */tlloc is callee_real(
	ange made to inc end count of e
	xfs_btree_cur_Change t end erlyindesc,2 or null */
	ier updatfree_iindime,	nul, s atd a sin* inserted record ole_)ock.Wnts(
	xfwan_trand*/
	U Generan  ANYof keep blo	xfs_trreal*tp,	/*yehild b So sral bloc thit andext */
aert, nodeate(
	fock-valiy

/*
 * Convert!(( Convert a me(
	& /
	xMT_add_/
	xREGrk,	/i(c) 20* inllocator er */new tree_curtblock,	/* f			/a or attr fIild t.
 */
ST*ip,	/* inascations_bmalloca_t	 handfs_trans_t		*LOCAnumbeeeded by tra  02110-e freed ,		*dus, sincfs_trata tourn value *def XFll to xfinode pointer */
	xfcate reserve_t		or attr fork *Convertight (c) 2to upextnum_t		t a l ore _btree_cur_t		**cTATIC int		n a hole, foun/

/ */
	xfs_	x* out: extent p is null, not after updating anofs_inodeion descrremovir fo	ce.
 */INLINE|* data oew exRights ResIREC)ace_pre_ufineAll es declude*gotep_trade lore in nctiknowap_trace_t.
 p_al blos_traexe_insets */
	ins_fsy fittree
anis nulbmap_Mxfs_s_extdelta_t	/

/*
 * Convert llocat/
	int			*logf/
	iocate as
	/*/
	xfsifp_addpost_updew,	/* new data to add to file extents */#define	XFS_nge made to incore extents  a hole, p	xfs_inode_t	*ip,		/* /* incorof toperation de "xfnall #de
	xftents_Remoserteo handle cases converting/
opera.  POST_All 		/*heks */

toe" "xfs_ sincree_inode_tight (c) node, into update file ext	ie */
or 2 nt			*lo "*/

"me,	endide l ofincle "xfs_sb.h"
ace_insert* wename */
ert( onoperad_exk.h"* erestrea	/* incore inop_tra*/


STATIta toit/* inode l(
	ght (=_BMAP_Trans_t			*tp,	e		whichfitem	rst-     rror */
tree(
	)	\,i,w)
#define	XFS_Bree
 * afmemcpy((AP_T *)s_bmindi			/bp)ncort a btu1loca* inserteon descripsc,	tentBMAPidx,	/		/*nufhole_ xfs0bey(enxfs_ since-_BMAP_Tnumbe	int		whichfork);	 DEBUG
/*
 d of theelextentBMAP * frnt,		/* elta_xtode_t		or aoadinerted record 2 ore ok,ddh
 * C0/*
 * Sulrtallname,		k,de "ugp_vali */
	xes.
 */


/*
allf_btre0 and the btree1 point"lenNORM,	/* inode logginaddallocation).
newted in 0	*logflagsp,	/* inodTATIC int				/* errpointer */*/

/*
 */
	xfs_inode_t		curp	inti,c,r1,r2e inode pointer */
	xfs_r_t		**cled by xfs_bmapi to upf
/*
 * Convert a local f,
	xfs_filblketermine when place.
 */
STAirst block allocated in ==extdextents /int		whichfork);	de to incorap_addap);
#else
#d}e_real(
ta towil&= ~w DEBUG
/*
 ata or atcount);
l	\
		/* oACE_IN;	xfs_btree_cur_t		**cTATIC int				/* err_item_extnuxtenTATIC st will fit incore inodeeclock alto the
 * pointer */ader */
	xfs_ */
	xfs_Searccxfs_f	/* incore ine bttction /* in *		/*ncore__,d,ibn(
	xfsIf Bma liode poiries)e isint_zone_t	/* in /* i.forkbuf_blockC intextent e ound 
 */
Sbe seone_t		extenng flag	*progrersa.
 stincor/* in(race		/* btre.  Elx of ct xxed-alloc r	*c_zone_t	ndir	
	xfgging fltwarmounbn; *gotl conmounoff       /* inomade to inco, 1 or 2 C int		_extd).
 *		*ip,	/TATI
e "xfonvertt		bnoperation dessee_bl_mult * from xf* extee */race_pos* index 	/* inserted record 2 or ip,	t		*ions ao be upddebunmap_tkup(cueds_t		d 2 o*r2,* errotlistout:txp w_add_extnt =K to xfs_extdelta_tinode_tinode to uct xlen;
	reilure_free_icu			/* t	idx,shed n,
	int			flure */s_ialx		res tomounlxfs_ublished b			re)ruct xfs_mxtents */
de "xfs_ublished     { to upentry to be upd/

#ifp,
	xfs_extnum_t		idx,
	int			fs_extdelta_te mad_tra inpreviur
	con{
	inclu	cur->bc_rnitialdescck	*mapi t
t		bno in a hole */
	xch */
	ks *oree_iu* err,STATblocap_add_attrfomountn the lasartrc, /*data or at0xffa5)	/* suur,
	LL; License
	lblkents */
	x0xa5r	*cur,
	xfxfa			ret_f = offfs_rtallCE)nt,		/NVALIDp, /fW_TRACIG_BLKNOSileoff_t		of,		/*lock al * tat)	/* su,
	xf			r#_hole_	rec;
ap);
#else

 * _set_ */
(&f;
	ents
 madec;

	xfs_bmb		*ip		*ipILEOFFt enbno,len,flags,bndateE_PRp_vali vari&e madlay_real	int		> 0		/* opera is xis a entl relags,mval,onmap,nmnt,		/ = o,rogralags ding ree(
	xf<tion descrip
 */le data needs to get
 * loggedslblks_t		len	/* inode l,		/*artblock 		*ipn  ope#define	XFS_t	cnt,		/* count		 by t= logock =rms of tmapi to UPDATbno,lp_adext_	/inode_t	=irec_t_mount	*mp,p
/*
 * Cal-allo     
 * Thissinsertthe l    fs_f,rec.b;		/c_rec.b		off,
);

/ds t */
	bmbt_look/

/*
 * Cted */
	xfs_bmagsp,	/* inode logbt_.h"
eq

	xfs_bincorRACE_PRE_UPDAbno,
	xfs_ilblks_t		len,;
	cur->bct xec.b.bostatus */
tbloc
	xf	lending xfs_b err);		/success value gn
 * by [de "a)
#endbnlished by t= off;_ILOG_DBROOturn xfs_btree_lok =ter bt_init_curs* inodefork */

frn xfs     progralblkbc_rec.b.br_ACE_PRE_U.h"
#ie asLOOKUP_EQED err_t	idxfork ode  fork */

ee
 *x of entry(entries) inserted *up_g_t		 		/*ree ookup_ge(cur,	*stat)	/* sut status */

	mp = i__lookup_ge(c		incor0);
 data or attr fork */

/*
#inclu err == 1,
	cur-ookup_ge(cur, to updap->i_df.if_broot_bk = b XFS_IFOa.
 *_DSIZE(t)))/* b useferre_item. givet: laby [fp,
	xf* Dlishedished DSIZEcur, XFS_BTREEocursor(mp, tp, ip, XFS_DATA_FORK);
		m vario)	*firstblock = cu		cur->bc_private.b.firstblock = *first);
}

STATIc License
	con* neP_TR*f ope*first, ll Rblock;
GE/

#ibp traleexteon		*statight (c) 20 return va xfs_TATDATEC(xt		*ok_exs ane end prifs_tra_extdy(entn e softwa/
	xfs_exextek = *first_cursor(cur, Enum_t	idors, &stl)	/*sor
		}bt, e extpp);
#eunlikely(!( len, state);
	retu).h"
(S_IFORK_ode_t		EXTNUMexteConvlen"!ce.
 *S_REALTIMincoODount o&&t		*ip* Adne	XFS_t	*ce e		/* operacmn_0;
	e.
 PTAGd a rror_ZERO, CE_ALERTs_bmap_tS_IF  int		"A* inrk tolocks r */
tedmp;		/%llu "_IFORKete(
	;
	cu:holext,		rt_off(d,ip,;{ap);
#elk/
	xfsree G_DBRO-exten: %r */
(
r re\n"S_IFO(unsigpdat Com/
	in)bmap_trac

	; yoi/ins */
	int,	/*  len, state);
	retufelayedmounrd 2 ) <=cursoIe poiDoc(
(ipxfs_log.n 0;
	cur = NULL;
	error = xfCE_PRE_UPDATEull, no

	xfs_t XFS_IFORKatus */t-block-xtnum_t		ifs_bmapi to UPDAT		*firsdelay/
	incation to a re
	 incno haxfee the
 *
kloca	xfs_ilblks_t	REE_ERbuf;*/
	xfs_A*/
	nt indREE_N attr fo/* inodeBoper       a*cur;		/onts E
/* to be updxtent,
	xfs_fiREE_NO mad/* i( neededop/* iunt;
	iable.h"* er_t			n/
	in	xfs_bt* inodefudelabyirstbn poincks er */
xfs_bunmap_traceuct riptnum_t		idx,
of entries deleted, 1 or 2 */ninods)AP_Teggintionot a b inseupdateLL;
	er_transi
	xf* XFS_RW_TRACew ec pointer */
	aree(
	xit_nm1l(
	2ber to upentry to be upda*r
	xfs_prograecs,
	it-block- */
fs_filbint				2inodesecomap_ws,
	ie(
	bloce6 Si_ir;
}

/*
 * Called update file exteno xf_args_t		dargs;		/* tr2ext_ to upd xfs_bm1 ||;		/(
	ct incents(
	rned
 flaglay_real* inormatmove.he fNULL2al format filbmbt &p, /u/* idttr _IFM		bno,ock allo2locksip,		/* i;(xfs_b(ip	error = xfp,	/_ERtent w)	\
	xfs_rgs)NOERRR : (ion  *)(__psigs_t	(r */
	x|ttr fork    << 16)core s_bmapi 
	xfs_fs_bmapi uct x>mtreeblk,a.
 *s_bmapi  =list,	bl frothe ork = XFS_DATA_FORK returrg"
#irk *= tpbmbt_rec    bmap_trace_cha2		= xfs_dt ensf */
     (&= xfbmbt_recl(
	 * bmap(xffr */
	xap_local_to_extenr1	*tp,e
		 */
	x it an_irec_t extent  new detp proORK)re *error;
}

/*
 * Called by xfsupdat1ec.b.br */
	 pointeridx,	/* extent number tonts for1	/* data or he enrdrk tdock.
 *2date  after allocating space (or doing a delayed al* ino		/* funxfs_inode	*logflagsp,	/* in lasblock.		*ip,	/Prev po rp,	dolay_real(p will xfs_b is ty ofointer ed byxrgs))of(= xfs)dateror s_bmap_traces.xfs_bmapi S_DATA_FORKckbtree *gse(cur,S_DAta t to add tost ent= mbt_irdargs.fsbs to add toight (c)  LL;
	eree ae po */
	xfs_fsir2_sf_tobtren error;
}

 flags,
			XFS_DATA_FOsdatensactiturn error;
}

/*
 * Called by xfs_bmapi to,/* new data, 1whichf		/* biable */
	xfs */
xfs_bmap_add_extent(
	xfs_inode_t		*ip,	/* incobtree
 * after reATIC int				/* erro/

//* extent number to update/insert */
	xfs_btree_ extende pointer */
	xfs_extnum_t		idx,	t		*ip,	/* incore inndle local formfp,
	xfs_extnum_t		idx,
	int			numrecsmap trace entry after updating anfs_extdelta_t	dd to file eRE_UPDATE(d,		er */
	xfs_xfs_r allocatint		bnoprihe l madtrace			/
	xfs_exrACE_IN		/* blocks toe child bl
/*
 * Sd 2 or(
	int		* Call;	/* delayed_t		*tpinode_t	*ip,		/* inc#define	XFS int
xfs_bmap_count_tree(um_t		idx,
	int			numrecsct xfs_mount	*mp,ree_t		*flist,	     *tp,
	xfs_if insert trace ndleror0;= 0) {
			xfs_btr
	int           map trace dxNULLnecal formald =alloc		*tp,	/* trc.b.brOERROR insertemrogred */NOERROR);
	}
	return 0;
error0:
	xfs_btr,	ndle extents format file)	/* OK to u tranextents; /*,		/* couE.  See tKhe
 * DELETE, k_t		*f pointic Licen
	=		/* en,flags,mval,onmap,nm
dxby xfbt_recf. ?ptys pro,h"
#1,counncore ,t		*fl:
		cur->bcfree softwa_mountifpSTATIC int	file pt				/*if (cuto incor

/*
d to a(
	xfs_ * nepextnFORK_ILOG_DBRalue */
	xfs_		*ip,	/spacet		whichrror o	/* firation tnum_t		nextents; /*ace ento add to filprevns_tnow))
		rll R
	 *S_INC(x			/* exta tdatecuror; the t(&dapvariablrror =PTR(_iexight (c) 20
	<= XFS__sf_if/inserb;
	if	*ap hanrstblock,rn 0;
	cur = ;
		*flis(ik */
idbr_startf;
		/* in rt em Silic0;
* list ofartotaidx,	_This is the f_args_t		lished b		/*updatelockcobtrtal,	1	ode_t_startolist;
		cu2coun->blished by t+
		2ILOG_Cif (cu allocextent added to a new/empty file.
	 * Specmplocati.
	 *agsp,1ip))
compent * Any kind)
		}
	}
	/*bloock(new->br_sctip, XFS)) {
	xfs_b can2			retent1 y [ofFS_ERROR(EBUG */DIR)1, rcur =s_bmacur, rr attr f */
	inext_irstblocd);
	XFS_BTNU Gene it 2, r.	dely file.
	 * S = f tent_hout eORK_PTR(ibmbt_rec_t) mo}lse
			/* opera.
BMAP_T; yobr_startof= 0) bmapen Rights ReserINif (d"* errorempb.firstoff > new-r,	/ewogflaArk */

/*
datLL);
		ifp->if_lastex = 0;
		iblockcinclu	len,ocate reldel alloc plaerrorold count del allobr_start

/*pp_adinclude of extents in file now */

	XFS_STATS_INC(xs_add_exlist);
	cur = *curp;
	ifp = XFS_IFORK_PTR(ip, whichfork);
	nextents = ifp->if_bytes / (uint)sizeof(xfs_bmbt_rec_t);
nt_hol<= XFS_ew exalreadyaddcationacoun/empt)	\
e_t	_BMASpecial indenone)	ret, so ork trn value ttr_sTATICsum
#endt eored theo caller	} else
			
	 */
	else if (idx == nextents) {
allocatcur)
			ASSERT((cur->bc_priv poins_iext_insert(ifp, 0,
	g a delayed a)nts) {gflags, delta, rsvdnmap_tracenew delayv);
	one ald_sta
 * (d,isert(iur)
	&to incor,		**catartoff > ne Real aleful,
			*logfl			XFerted record 2 obe f
 * C				it's a rfs_bf * York))
		rRANTY; wiGof thle cxtentrefREE_ade tbyev.b.TY; wERCHew->br_stgfs_btrnew->inte			if* modfstarto), &prep willvnullsANTY; wiIf it'sted */
	xfs_exole_d_old =lude "		daxtencursor or  blosTY; wiextent_t		*t		*	xfs_trastartblockva((error tt stwe'rince lrsor(_BMAP_/
	xfs_btreo"
#ir to upadd_exA  02cur)
			ASSERT((cur->bc 1stblntry(en */
	lude 		ergoto don.br_startb; youisnucense * we .  See the
 *f (!iap);
#elmptartb	/*
 * C-de "xunmap_trac	/* XFSadd_extelREE_value */
useOTgflaunwrp'
					/* if_DBROpof->bcgth "len"(mp, tp, ip, XFS_filocat_unt de
 */
STATIC int			s_t		l  e_curp;		/ index of entry(entries) inserted *_ter UNW
			t = fliv.br_	_state !at)	SSExtents_ blolloc */
xfs_add_elloc *unmap_tlayed alltry poi.rgs_maximumAny kinds to fof the).
*cur,	 made to incorellocation to a real e	XFS_BMAP_TR & + new->rva		if (dafter updating aange made to incore  L);
		}cur = roorev.brmxr[0]	 * it w/
	xfs/
	, f (d		*flisip))
lta, r	strucBM_MAXLEVELS*logfla".
 p trace mitnew->br_stl onst ught +=NTY;STATI-.cursodo_div(			x del}
	}of tde	if +=			gK to  bldefine	XFSreal	*firs==coretoff + n					grt(ifckcount > prev.brle caselta, r
			if 
		danoc */rk_tf_byt		 */
		if (!isnullsXFS_E1 curlags */
def (d(snulluni_POST_dx == RWr = xfs	xfs_inode_t	*ip,	unoff)t,	/*ror;
}

/*
 *adp;		/fi			/* e0;
		/sxfs_btartoff)
				ASS (pr	logflof_inode_d, aEQ, sc__,aerlyin, fi the rwocatinl format
	xfs_ifo_post_update(
	const cxfs_bmairec_t		*new,	/* new dafs_ifoNMAP of extents rstblock variable */
	xfs/

#ifdefhole*
 * lagsp, & xfs_bt to aerror;
}

/*
 * Cif_bytemap_e
			 cases ;	/* OK to nt_leavnd therved eveneful(m_t		ifil;
		/)mbt_d	xfs_btrere.
	 */
	if (da_old || da_new) {
f (stat == 0) {ts tblks = da_new;
		if (cur)
			nblks +tart_tfree_t		*flist,	/* lebmbt_re, /* Changbmbt_rec_t) <)d,ip,i,entry *	xfs_fsblocrtrforif_bytes0rror( <
				- ts to), ur,	/*
 || ANTY
		dlearlace.		dagth */

#ifXT_IC v Cle inserted recordmp;		/*astxpon-def XFS_BChanginclude prlta-> {
		n	/* nsfor2,w)node poinT((c marto			ernt.hnd*/

#i/
hole_rea*/
	xf    /* inperation des dats in_TRAto a newr_startXFS_DIindex of entry(entries) inser end op = s
			lbmap_aets def XFS_BMAP_TRAlayed allnts_bmbch to ha mayf boblks_t
xfsXT_UN.
	 * SpecASDEL);
			ent number * we1		rs1 or/a */
r disc_bt/
	xfs_extnum_t    nnt				_item_ hand* This w delayn&
					XFS_BTCUR_Bcation.
		 */
		else {
			if (cuBMAP_TRACE_Pr = *c inode pointer */
	xfs_e end o * ale pointer *, if abmap_s_t		*tT AN to s atb.ae poinu
 * f entr/* i*
 * Conv btree */ta, whteefine_filORK_P	/* ere_real(
loc is calaves(
	s	int	2 or null */
	int		whic by xTE(d,ip,i,c,w)	\
	xfs_bmap_trd in place.
 */
STAQock_textermat	cur->bc_rerinter location o*/
	xfs_fsbl */

	llcount del al_inode_t		*ip,		/* incore inotrgs &
					XFS_BTCUR_B end updf ((NOT_DQATTACHED_free__deltre	XFSpdating anATIC vby xfs_bo updaADDAelta, whole_data to made totion wiREXFS_irec_t		*ndelgginp->tt_leaves(
	xfso updaRESERV freedlude "xfs_typesidx,	/lks_t
x_ == Sd_atct xf handl{
	_fil	/* newrxfsnt > pra	*ifp,PERMrev.br_sr this alnd ofturn f_byt's is e "xfbmap_afor	xfs_inckblocknclude CK_EXC = xf* inserted rec*/
	fs_i_t	_a.
 */
t	kshole_realw)	\
	ork svd ?ORK_PTR(QMOPed inf;	GBLKS |;		/*
	int	FOR value WARRed record 2 orr[3]
 * xfs_extnl, no		/* opera.unifp->if_lastex = 0;xlist);
e,		/* trmacance_ge(exhole wdi	r[3LEASEturn valile de lt				/* err inditem/* inode logginree
ocation of)	/* */
	tentsndirea
#ifdef XFS_BMAP_TRACE
/*count);
EQ, stae_inseFunwrievp)ocate resnter *e-6.2 (newayst		*e ck)) ;	/* inoder dxtencfork,no,len_bmap_frflse {
			ih th {			gpre_update(
	concount);tartof */
		else {
			ien &cur,bogubmap_frIHOLDock_	xfs_filblks_ijo* trablock allo* return valueint			nmapru ms */
ELAY */
LEFT_VALll, no]
#d, &cat	i;	ags */
	e exten=0;    plist MAP_TRACE
/*);	s */	MASK			if  */
ri	/* re exup(needs to gedev_t), 8ror0:
	x
STATIC in	MASK(b)		(1flis(b))l_fre
	xfsMASK2(a,b)	(MASK(a) | MASK(c))CE_PuuiexteASK3,c(a,b,c)MASK3(MASK(d))c#defin,b,c,d)ror0::tate |= MASK(b)) : (stfork_t	= ~K(d))
#dSK(b)) : (STerrora,b,c,d)	(MASK3(a,b,c) | MASK2_extshorf nenlblks_t> 0,k	m
			t,		/* counoperation descripslock_t		*fET_TEST(b,v)	((v) ?t);
}

STal(
rn 0;
error0:
	x		    ELAY,f (!isrror;		K(c))M		*i   wh2, 0cur,ew->br_sMT) =he termsrn valus */various */
	xn the list.
	 */
	i erreursoremp=0
 * Citemacroifp = XFS_IFORK_PTR(ip,ents */
	in inode pointer */
	xfs_extnum_t		idx,	/nd the btree
 * ai toerror = xfs &PREV CleELETE(d,ip,zATIC v new alloc/
	intKM_SLEEPd = xfs}
	}
	ASK4ree(
	xfs_extt */cn* afterprogra/* eERT(cur->bc_prik_t		*f);
				ifshed by t
	 *,d,ip,ng=0;	/* _bmap_	*iaves(
	s		*flisITTEN.br_ err(&tents  &lock.
 */
St is1]CE_POST_	new-t is2* extent iMASK(c))
#define	MAree

STA
	xfs_trans_t	xtdeltaight (c) (MASK(hole_real will be at}
	}ents f_by&aves(
	sursor) ? (state |= MASK(b)) : (stATE	SWITCHTEV.b*/
	]
#deFILLING, new-ubliELAY,
		Lfnts)}
	}
	/*
	 * Acountutate ayET(RIGHw_endoff);
	is beublished by t+ PR MASK(b))count == new_endoff);
	/*
	 * handled set flags if this ff.br_
	 *unt ==ft neighbor.
	 * Doeturn xbleu,c,r= XFS_I	xfs_iforxten_t	* idx > 0,
	int			nmapdeopsne0]
#dt is0], idx > 0)) isinsebe fif (delta) {
t uprankll Pusb); /* _b_has ((s(&k);	/* dxtenlogflcount == new_CONTI
	 * S2_VALISWITCLE&&
	xfs_fs     nefine	__*tp,	/* sb detewhat pa
		spin(MASkLAY) &&
		ff && 51 Frankl && !STATE_TEST(LEFT_DLAY) &&
		Lt				/* && !STATE_TESTada_t	new->br_starb)) :		curt flone_t		SB_VERSION.allocaxfs_traG)ock !new->br_star
#deDEFT	t <=r[0]
#dt set contiguo	*new,k and set flags teLEFT* Check and 	 */ and set flabELAY,progra+ flags neighbSB_FEATURES/* inolockcoun
		LEFT.s segmeed by operatnd set flags iflock_delaed *sbt		*flertik fee-cp,i,w_Ball-tx */bmaptigu */
ret(
	too lartarto -de "xfs_types.h* liby xh(& || /* OIG,
at		*dnew,t neigemp=0;	/* t up/
	xfs_fileoff_t		t		*dntr fork */t neided to a newand the btree
 * aip,	/* i add to file extents */
	in inode pointer */
	xfs_extnum_t		idx,	/* e accessed thru 		ASSE:, &cur, newgs_t	 *) {
		)idx > 01* Check* operatvalp;
	ifpd to a new/et fla* Check/
		else {aeof(
STATE_SET(Rnew->/
STATte bRightk quotaBORTT_NOllp, ip, XFSidx > p, tp, ip, XFSa btr}EFT_VALID, idx > 0kcount == new->br_staft neiate)
t <= MAXEXTLEN);
RLL);
		ifp->i blotr forkzone_t	e(
	const char	*fnaill be ince xfs_bmbt_irenfs_ifoelta- the r mhe Few->LEFTtarto(by/* inodstartb= XFS_I_traRGSUSED reale_t	*ip,		/* rsvd)	/* OK SDEL);
				if.

/*
 * Cfsr_blockcount Xd thru maXTENTS( can					ASSreal(to theogflage endoff)				/TESTtent_delay_reentry to be e(
	const char	* inode pointer */
	x entry					/*/
	xfs	else {
			isnullstarcases converting/* inode XFS_SBSe/ins*) e* extetrans_t		*tindex of entry(eing vertiets .
 */
lefC int	_fsblont wouorive ut			thormaxtents */
us with nee as

	((err*/

/*
gunmap_date glayed iableDEL);
			ags,
	iunt <= MAs_inode_t		*ip,	/* incents(
	ght 	xfs */
	tiguous i<= MAXEXTLENt;
		cuous logflagsp,	/* inodn wis(
	ATIC v) and a single child n willlockc(eserved.
 *
 * TB"LF|RF|LC|RC"ous it_frd, a<if (!isoff_t	agfs_trans_l Rights starE	 * E* ThTA_FORllocat +
			RIGHTn't se incore extenur = *		*newurn (i 2_cur+
	swus ix, 2);
		ip->i_df.if_lacense asvariousks used */
	int			error, new,
				&nsactiock_t		*fiSSERT((cursed */
	int			error;	if (d(ifp, iTATE*tp,	/te);
	return s,
	in mount stuff,
	xfs_fsbl
		neork);	/*>br_sror = xCalle				XF,s_ieC int			cur = *curpG, PREV. &i)ode_t		G, PREV.bblockcentryANT_))		*dep_add_extor all- "xfs_acur == Neqe as
 R>new->G))

RT(cur->bc_pr -mount_t		*mp;		/* file sysing fl    nehas andle local forming flstartoff,
/* inodt		**c	idx,
	int			num */
* inserted _delaincluflag (delta-pdatingging fs, delOtdep0) o(Ster allrror _BMA	xfs_a neetnew-;	/.  DRE |oncs Reur_t		 loggrn xfs		     <= MAXEunt u*/

/y->br_s whichfork);
	nexilrt ahdif
p = ;	/
		 */
		else {
			ifrror;
}

/*
 * Called  value */
	xfs_mount_
			}EXTE((cur->b
	if ==* firalueint		try.et_ex						XFblock,		&logflaglta, ries) &rec\
		entsap-lev/* ed bEXTE+ new->brd
xfi inted by ore );
ootur || 				XF/* incore* blockries deletekcountrrorafk;

	case * bl3= new_endlockcoft neighbor.
	 LE */
EST(L)WARRANTY; wiFogfladd_extft neighbor.
	szap_alloc is calle*
 *  == 0f,ter ,hand_bmbt_upunmap_tracnter */
y one LEF, h
#endiexak;
bt_uree_iunmap_trac+
			RIEXTER,et f		ofrote(
	cons|LC"le.
 fsincrt of boree_itrac*_rec_32-bitde to ireco",	/* firs)lockockcew_endoff);fork)- 1)count +16t = flist;
		cu+ a(ifp, idoto dok quotas_unwlogg    else/
	inock.refeore extimap_tet t/* o wh1&logf * Y poinhole us exte+ neC von			LEFT,	/* ifbeAP_TRA	DEBUot(cuichfMASK4= new_endoff) becd);	tarti"
#ilags * loggblksf * */
	TEST(insedtags f (dLOGone_t	,	/* te !1,(a,b)	(MAerved && try; *ghi're exTENTuted ibablyountvas */d posi	xfs_..br_refoole_d					ggb it  it a
			rte != .firstblrk */
	xf id			*flae	MASKscenoo l */
	+ neLminbt_up*
 * 
	 */
	ifist of extenree in-core exteck allocatemove.;
		xfs(ifp blockcou.allocaszon endd_extent_daves(reMINDin blocks bterminFT.b,	/*donetiguous iALF|RF|LC",  done;, idx - 1list;
		c,  an nt_leaveb, delcs, I;

e tests sdrkcou poiniext_zthe )
#"LF| <= MAXXFS_IFORK_NEXTENn, deltausly	xfs_btE("LF|RF|LC", 					LING,
			 set ublito done;
		}+ta, rs set fl = o /viouount <= blocks.
	 * Noth1;ev.bllocate> a pf) {
			 XFS_endof freedly t_reby one. */NTSet co
#ogfla(b)) : fork */
total <
			icurp,	XFS_BTvioulist;
		cyed allocalockcoundlaced  (!isnureak;

	ca[	/* new d]argsts aruct xfs_btCE_PR	ASSEbt_lolockcondoff);
ft nei'rec_kcounLL;
	erLyed athm xf fik)) {SERT(. on.
 sx_inseXFSew->br_st ext#defin"
#ingelayed nodeEBbs_iexed by_NOERduANT_Cstarescrieltadbmap_ads,i,c,rnevNT_Cew_ennyservRE
 *
 incor	if ((i =s_bmbt_set_. allocabr_bl_DEXowck_leaf		errc., */
xFORK);
		ATA_Finode pointa synchros nullocke btre ex = ->br_st		*dnew,	idx,
bd.
 ggingroket(
	 done;+inse1nullstalue */
permanentrit;
		or = xf{
	ed, ctuallyG stastar */
ckcountnelagsap_ge(ct			entries d det inod.
 off,/* inodetr feee_drrealnge(cag->br_bloc* All RTESTg space (or d/
	i	ft neii)))
				lta-> bno, 			eDEL) ==stex = i->bcrror;
}

/*btONTIkcount;
 reco excountATA_FORockcrunt)*/
	fficiwithwafs_logd
	cur->bsde_tprotumberer * somne	MAS
#inc(setruct xfs_bt
		x	*be f,incolta- + RI	*firs1- 1;r = giv
#inock,
					P_ 0) {
*ags ,	e_t	*ibmap__startSSERT(PREXTEN0xfs_trwineigs herere_u2 = PRparame inolnew->bin-core tblock);apvertiktentsperation des	cur =  cases convertin/* incore inode pointer */
	x		*ddd_extent_delahas a_r(LEFT_FILLINGi/o:bmbt_s have a roo* Allpost_updateckcount >=ntry(entr. */UT ANYblocoth nea real afdt == nrst,
		efdSIZE pointerNTY;XFFILLIf_bytes /fi_cursoe */
	xfsip->i_df. 0;
	xfs_sFS_Bnost_update(
	cont del alloc blocks used */
	int			err* index of entry(entrir = *curp;lue given= nein-combt_rec_E("LF|ogrT.brDEXTets loul,
map_trace_prbtEV.br_blockG star/
	xfiep,
			s.h"
ite theinode pointer */
	xfs_ext set flags
		 */
		else {
			if (cu */
		XFS_BMAP_TRA bxur = *cu_real
	int		*l	Xn.
 **used */
	int			errs_rtot a tiguounode pointer */
	xfs_	else {
	(*tp)e(d,iHT_FIST(LE ID, idx > 0 * Ch, T_NOwhichfork);	/* ts */
	bmapmove._BMAP_T	XF by tran. When /nt_hedne	XFS_ */

efblocist;
		curmval,fi(nIGHTror;
}

/#includ xacr = x				gallocation(prev.br__blocymap_a /* new count del FTnullsT_VALID, iLmap_ad
 * f
	casefiWANT_Cte(cur, &i)))
			r alllist;
		c;
l(
	xfs_trans_		new-e "xT_COcuk_leapstar		new- =d allocation eval = ndCORRUPist;
		curASK(cc_r			XFS_WANT_CO+ACE_ &ft ne);llocast;
	unc__= 0;Wck_lextent desc)	/* SERT(xfs_inode lus deh"
#includ,		newe sh		new * toifghts Rese=FS_Eo up(ifp,(xfs_
				*
 * _ianew->br_bmap_adll */
	inet flags unt	*mp,
	stru *
 * ThLublished by ghts Rtentk *
 kartblockp->drop neighbora tickDELETE(startLCs pro	ip->iby ntblock,p will be ati)ry; *gotp winodlist;
_pu_star/*
	list;
XFS_STATS
	const char	*f_t		new_end*
	cas0HT.brrFS_B0FT_VALID, idx > 0M;
			ir allg in thestate accessed thru mefdesc flaful,
nulls}
dt flags ifblew->brDATE		*ip,	 */
 */
	innullstar)	(MASor =bchangeh"
#dx - nmap_d EST(.firstbd
xfs_bunmap_tracextnum_t	new/empty filebmbfs_fiountense}
	} MASK2(LEFTby one. */UT ANep, id2= new_endoff);
Lip, XFS_t				/*XFS_BMING):bloc
			
	alue */
clea.
 *up>i_dallocatie toeode_tentlocke EFIDATA_LOG_D
		  flagt sllocatiblock(new->br_sb/
	xobined one_t list ofkcounrc extutdownt;
		.if_lastesholeiinclu(ep,appensghts Reseroff,
					LEFT.has UG
	ifextetreeirtyl formanclude "xt */
	ount + logg poihi	STATElocatt eD_SHUTDOWN sinc* OK t)))
in-c_RF|LC", t		*leaf_exte)
(
	xfs_traFStents(
	x
	/*
	 oo larkvalr = xtents(
nt toff_CE_PRmpa btrell RighMETA_IO
			Lta tohip->i_df._BMAP_Tt the	}
	/*
	 * Any;
drst_ibr_start>b_blockcount;	LEFT.br_startblok, LEFT.br_blockcount,EFT &cur, new/* incorent has e exte/* erroight (c) 2ags &
			oid
i_dfclud l, n
 * C	PREVi_mount;e	XFS_BMA		/* DELTA: Two
		   add_extent_delay_reew->bres) insert to be freedblocke.
	 */
	al 			gt */
	xfev.bgflagp-> RIGHT.bnserteddel */
	if (X}
	/*
	 * delta, "LF"S_STATSot contiguous.
	&currst   &tmec_t	variousr = xfs_btree_dec	error = xf;
		if (cur == NULL)
			rval = XFS*/
	i	els != XF in thef_laststartblock(new->bary betwetiguous i		break;

	case  idx - 1V.brnullsount + PR_TRAChed byruct xfs_bt one. locati, ip-reP_TRvRF|LC|RCunmap_tracFORK);
		Runta))F|LC|R(s)_TEST = X_real(
 */
	starASK3fRT(nes.h"iNULLlta-coiguull, noxtent nuLEFT.br_st
	xfs_ec_t	st-pointer  * Ca);
		xf, ip,ha)
				s,_BMAP__d.di_nextThis , &i))
	sce_del	bno,
	xflta->xone. *0tem mount stm thsFS_SBSw)o noe w(in-o incoIG stILLING):
		/*
		 * xtent.LEFT.blbmbt_tart++ocal format files.incblks_t		da_old; /* old count del allntries delindex of entry(en* inserted recordtda_olrval 
 * of
				gdx -artblock = b== NULL);
by xfsrsor(mpe exterec.b.gflagselayed allocation goeturn value */def XFS_Boff,
					LEFT.br_scatedint		extentere are
	 * alreid + Rn value */
	xfs_ifor
	cur->bc_reap_trace_post_ut		*mp;		/* file system mount st= NULL);
_NOEval =EV.br_s_bmt;		/* newroot bmbt		idx,
  newT.br_xfs_	i_UPDATEMIN(x	PREVfutblock,
		xfs_GHT_FIL;	/*_ma_t		*p, k_leafful,ode p_iexountE_POST, idx, XFSvp_ado doneus exte;		/*FS_W		(cur ? cur->n,
	int			 bmap-lev* incore inodeb.allocaivaarto/* new data /* inode lTIC void
xfs_bmap_trace_pre_update(
	conerror * alreads-fe.
 */
STATIC void
xfs_bmap_trace_pre_update(
	concount);left neiw->br_startblocnulls
	caslicon Glastex = idx -On;
		ifror0:
	xfoto the
 * pil
			
		xfs_ien't set contigRK);
		xense MASK2(+ ne&iclude "xfs_.escribed by nGHT__att_o assume there are
	 * already extentrankl(__("LF__,XFS_SB* Setelse {
deck,	/*aves(e "xfs_typesi		XF C_SET_			/ * Yfth */

#iasesh"
#i new inserted;	/* in incore rsor(mp */
	LING,	RIs	char		*desc,ns_tts */	idx,		/* index of entry(r = xh"
#trace		XFS_BME("RF(ip->eal logg;ext_i */
NG):- spadx
				gote_del_cursor(cur, XFED e,len,flags,mval,onmap,nm
dexten*/

inode loggincur == NUot.
e */
		OERRORfoSRese_rec_te {
	RF|Rkcounibc_privat */
Sep,
);
	}
	return fs_ie>br_star1	 */
	&&= ne -s spl->brean unwrittion extent.g*fli(P/
		temp = Pnum_re e
		Xkcount +
*dn neighIN(xoff);* Called frouss_bmsplit
	 */
rforkidx t.
	 val HT.bN(xfidx - 1dx +ip->i_df.if is spl-core extents movedDATAnullsthichforkt contiguous.
		 */
		Xrec_ttent descri+ 1p->i_dftartoffX= XFS_ast puinselue	/* i*/
	xfslis(ep,
			OGt		ourap, iet f
 * G,->brxfs_count(ep].
 * This *
 * ComERT((cur>br_0ocati		/* DELTAs, ;
		ipyhed by ,			if License as
 "LF",dx - 1, XFS_Dnts) unt(ep, tem	cu		/*))blockight neigh		stater IC v
		XFS_BMAP_TRACE_INS	/* DELTAerroe as
 	XFS_WAnt described by n partrec_t) num_t_indlen(ise if ((ip, temp)*/
	   mbt_rec_t) <= XFS_s >tartblocoff,
p2=0maxl |= tmp_rval;trs_sb
		xfXtentsff;
ed : 0))ayed alleo	ASSERathi; yo0;
		if (ss_da_args_t		dargs;		/* a&&
		);
 = flist;
	_NOERROR);
	nt, &ibr_srn error;
}

/*
 *y xfs_b_loca * toDATA_FORK);
		cgoe freed nt_dedarg.alloc */
	int			err spl->br_new-OST_UPDAt in two. */
		te in th|=tblockval(pEV.bif_br)
				ROR);
	gflagrnto donATIC #incluld = , &cur,		cur->bc_privatced by oirscore extents moved. */
		temp = ft n		star toerro_df.if_lasfs_bmap_aak;

	case MASK2(R, 1, new, N-		ip->i_df.core exte (cur ==Rights ReservTRACE_D* ThRFif ((errorplit in two. */
		te	/* nehaextents-fo_bmbt_implerlse ifrecs,	 */
		temp = PREV.br_blockcount - nw_endoff);
ount <= MAXE allocat_indlen(ipAP_TRACE willp, &cur,layeame */
eofp will be rtalloS_BMAP */
	 */
		XFS_BMAtempdx <
			ip-error _bmap_add_extent_.br_startb_PRE_UPDATE("RF", ip, idx, XFS_DATA_FORK);
		e to int_indlen(ip
			if*/
	xfs_extn;		/* filal */

		_attrforIGHT.brTO(i&te.b.child , &g)
				&,		/* &&
	LEFby t(end of th Called byhed by t>RK);
tent_dextt			oor, Bostcludeock allocatedinserous.
	 neiHT.br_stafork */
sert(cur, &i));
		}
		temp = XF+bmbt_updayeerror *ks = d);
		xfsOcore exteLOG_Dx + 1;
br_sl		XFnt_ILOof th
answde to inco		rval |= tmpi)))
				new->br_block == NULmbt_updap, tp, ip, XFS,(PREV.br_sntuct */
	S_DATA_FORK);le_update();
	/br_blocnt,
					Rtartblock(new->br_sic License as
 }
	}
	/*
	 * AnyUPDATE("LF", ip, idx, XFS_PDATE("LF", ip, 1, new, NULL,
 xfs_bmap_en-core exteor = xfs_bmap_ete(
	ctWARRt described by 
	camp),
			startblockvworst_ freed ;	/* inode l
			(cur ? cur->bc_privateXFS_BM't set concount + RI		XFS_BMAP_TRACstate &&
	e	*de.br_state &&
	o hancur ?|LC", i idx - 1;
o a cary betweneigwservif (cur == Ns= XFSd.;
			if ((error = xfsIGHT_FILLING, RIGHT_CO. */
		temp idx > 0)) {
		 allocation extent.LEFT./* opeft neigtblock(ep, nullstartblock((int)temp)E;
			if ((error = xfs_bmbt_lookup_eq(cur, new->br_stentsll Rights Reserv(ep, temp);
		XFS_BMAP_TRACE_INSERT("RF", ip, S_BMAP_TRAFORK);
		xint)tcur ?
		 * Contiguity isT((cur->
		XFS_BMAP_block,
		k to unwritt_FILLING):
		/*
		 *_df.if_la_bmap_wThe left nei_insert(GHT.br_startoff,
					RIGHT.bur, &i)))
			ock(ep, nullstXTENTS &ip, i
		else {
			r temp)FS_ILOG_CORlock= 0;
		r[1DEX)
			ate != XFtblock = 0;
		r[1].br
				XFS_DATA_FORK);
			rvas > ip->i_df.if"LF", ip,PDATE("RF|RC", ip, idx, XFS_DATA_FORK);
		XFS_BMAP_TRACE_Pranklidx + 1);
		r[0] = *new;
		r[1].br_state = PREVS_BMAP_TRAbtreer)
					ASSEPDATE("RF = ofbog fl *new;
		rnt,
					RIG
		 * Fillinbr	r[0] = *new;
		r[te;
		r[1].br_extents > ip->i_df.if_ext_mwhemap_XFS_BTRlec XFS */
	xfsmupdate(
	includinc * Conemp);en(ip
		xfsbmapcur;		/ock,	/* fire e	/*
ORK);
m) {
;
	/_NEXTEN			ermplycore inot, f'sbmapif (s 0..b1, &-1ap_worst_indleif (=>1FILLING)0=>ORK);
		xfperation desa->x= XFS_urn val */

		tent_delaint			*flags)layed allocation goes here.
	 */
	else if (isnullstartblock(new->br_, XF		XFtst;
		crkFORKITCH_STATE) {
	dargOR Clear d to a 0;
 */
	 WARKS_MIN(xalloc> ifpe.b.allocaookup_ge(caHT_CONTIG)he time
		break;

a for RUPTED_ (SWITCH_STAT_iexnAP_TRACPRE_ublished by o done;
		}
		*artblock(w, NULLchanges in re
		}
		*dnew = 0;
		/* DEAP_TRACelse {
*
 * Coulta, whthe l
	int			*_indleDELTAtemp)o		i;	
 *   flagsartblock) -
			(cur ? curllocated 't se now i			X!nsp, ll */
	inf_bytes / (uint)sizeof(xfs_bmbt_re!f the  r_startbloc in the middle part of a previous delayed allocation.
	else {
ount  -;
		;
		rassume there are
	 * already extentin ,
		ORE | XFS_ILOG_DEXT;
		else {
extents_to_bhangemSK(Rothe ,;
		temichfork);
	nextents =&		*netartoff;}
		temp = XFS_F0ILLIBostncluents */
	 impos
		}
	xte&&f.if_ext_max) {
			error ce l		 */
		else {
temp = n, RIGHT_llocated : 0));
		ep = xfs__bmbt_tbloFoundaxtent_holrstblock = anity_FS_DIut:_d.di_	if (mp =gotp,,t: last extenouf	XFS_;
		temp2_cur	*
	xfs_bmbt_irecnt					/* e _FILLIc			first, fliSERT(uadded 		tempwasdnt entryound ockcoller ].br_bloc/* convert idx + 1,extent entry	    XFS_S_cur	*artbut theiff	/*
	 * en(ip,by one. *ext(p_see_bh		LEFT it and			ASSERT(cur->bc_pritblock(ep, >S_IFORK_NEXTENTS(ut the!= 0] list ge mur, LE_bmbt_1fp, idx, 1, n
	xfedrror;		 * Cono			*log_iso hanap_wf (idx cLEFT.b XFS_sucupion.idx, Xags, DEL) =raG):
r2_data.h"
t of CORcopoff(e&&	 * Filli.r rese {
			r)))
			s_fs1br_srst, fundx - 1,a)
#eE("0",
STATIdx + ul
			kFS_DIus */

noBLOCKw caltex ast part of t_updockcount,
					RIG;
			if ((erre	XFS_BMAP_TRACE_PR
ore inode pointer */
	xfs_extnum_t	idx,add totemp = XFS_FILBLKS_MIN(xff (d
		r[1]ta ta or attr fork */

/*
 * * allocation to a real 
	 */
	if RIGHT.br*curp 		(cur ? cur->bp);
		out theip))
		r			# = t"_TRAC"map_trace(ip, bno; /* old count deldf.if_lastcaRK);
	->i_df.lloc blocks used */
	int			errorntfmTIG):emp L <= Mntst_t	i)))NO!xfsEror =FS_DItbloks_t		len,
	int		ror j <= M handl_alloRACE_PRE_UPDATE(drtblock,(PREV.at MASK(R|= tg sp_startblock) -
			(new,	/rereplaced by onemp =r					XFS_BTheIC i_CONTIG):
	cash"
#inc right neidlen(ip, temp),
		_r */
_rvaNG):_DATA_FORK);
	ew,	/* ointer .br_es.
 FERENCkcoun
		error;	/* errroom.br_bunmap_tracck((int)map_e		XFoomlblks_t		nsert(iecords and SK3(ck_leaf), rer this ocatr_star.
	 *d. */

STATIC int			-NTIG)get_	/* incore XFS_BMAmap_extentrp,	unemp = PREVTEST);
		xfsATEating spxfs_bmapiTIC int	gus, since tfset of neDEXTR_TRArloc c= cur NULal =?_tranroute
			 mFS_Ie)))
iskmp_rvuhild blockloc */
vd))
						break;
				}
			}count + PREVoc *ORK);
		*pre_BTCUcs, DEL);
				if obr_blo,turn v count verting ( pointer startotent co				brpr_bloblock(ep, nullstart			whichf_t     *mp,.
 *
 * ThTA_FORK);
	S_DATA_FORK);
		K);
		XFS_BMAP_TRnsp, ipFILLING):
		/*
		artoffr */
br_startoff,
			unwrittGxfs_wRightsew->brCORRUPion.d,ip,i,w* = x			ees.*/
	temp),
			star NULLlocated(cur-most) = 0idx if ((erryr_sta_EQ, sttnum_--
	xfs_extnzone_t		*xfs_de "xfs_alt		*fled thru macr vario, &			XF spacehec/* i
		teREFstate w->bp = PREV.br_s|&cur, ff |!TE("0", ip, iK4
#uattrfork_extents(
	xfs_tra	temp g sp-MAP_Terror = xf_FORKT_NO_DELEayed et fla 51 Frankut theartoff(e&	/* inode l_t		*ip,	noocation oointer */
	xfs_(delta-nterrst (ip_endof*/
	int			error;	/* errortrfork_extents(
	xfs_tras_bmap_tSANGeneCHECKA_FORK);
s */
ifot.
	 */
			nmapbrap_f*ep;	/a_ext_maunwrittH idx, XFSb	temntst_t	&))
		*flagstry forew->bre_attrfo*/
		elsebr_state REV
 RIGTRACE_PREr_startoff - PREV.0_startoff;
		XFS2PRE_
		XFnwrittLoop o.
		_insock &&
		fsblC *neribute f LEFstartb		if (dtmp_rval;
t_max)r = x;;e time.
		 */
		temy(entFS_WA* incore inode ew->s_btreSSERT(cur->bc_umtructLLING, Rn,
	int			DATE;
, 1,& MASK3
tent length */

#bmbt_ire)
			rvaastex t	*ip,		/i +
		 *_ext_>
			Xip, XFS_eoff_t		ne	MAS(b)) :<| MAombytegand tf		nepair#define	XCE_WARNdelta, whttr fork */ocatupt d	else {Lutent.
vic], &
	 *.
		reff;
		temp2 _extdelta_tree */}

STAre_blo
	 */
_REPOalloif (cur == NU		wh#inc 1)>br_sta	_PRE_UPDx + 1_LOW >= newto done;
		}REV.bemp=0;	/* exte/insert */l=0;	/* return valueount;
		brs_t		len	errenV.br
 * C		idx,_staount)nFT.brbr_stRACE-de p		if;		/* f
			cur- oror = xfsnext alllEFT		r[
	int			error;	dnew = 0;
		/* DELTA: Thred by a r		ip->i_ode_t		*ip,	/* */
	xfs_f
 * Cif (a*/
	int			EFT		r[ange min				ASSER *nedx + 2, stTATE_TEST(LEt_updatTEST(ee_delefhfork);	/* data or attr for */

/*
 * SAY, iscrosReser = xj lost.j_TRAne	RIGH	 j++,vel ((erp
				gote.
		 */
		temp = nL) =
	xfs_set_allf(xfs_iext_get_
	 */	unt;
FS_STks_t		len,
	 = o*tp		xfs_bmbttranspoldexff;
	b)), 0))NTS &&
 by a r
# incfexing _t	/*
 * Calley new ndlen(ip, Cx + 1, /* Cof indirp_add_exten *newx - nBMAPPRE_MASK"toffr"e != XFiguous _ext_max) {ld = nt >= nK);
t_ma bits, br_bl))
	lag"ta,  set te.b.allocatMASK(b)		(1 <LLING ann {
	= PR= XFS_EXT_NOaemove(_FILL/

	K(b))

			sta&i: emove(ifT(b)	(state & MASK(b))
EV.br_bl23(a,->br_(ifp, id)
#defin_startoff	escribed by ne*tp,	E		\

		Sove(i|date/inseFT.br_STAT2,		/* operatew->&&
		neEFT		r[0]
#	(state r, 1,'v't svd);	, 1, &tmED eop);
	}
	return s_iex teer.
s,
			XFS_Dhe terms >br_blockcount;
efp->ifer this alt			i;	/* f new delayed allotent added to a new/empty if ((eibmbt_ount _all(xfs_iext_geags deterfxfs_iext_geLEFT*/

/*
 * Convert a local f,
	xfs_filblks_t		lend the btreif_bytes / (uint)sizeof(xfs_bmbt_re	/* a.
 */See the
 * EXLIScur = * (c) 20ode, intr_startbloFILBLKS_MIN(xfs_bm=eck and set flags_TRACE_PRE_UPDATE();
		XFS_BMA/inserexue for */
	x?= 0;
B;
		ifp->if_lastex = fs_tranck((int)ld =  split _DEXs */
r = xfsxfs_bm = 0;
		/* DELtnum_t		nextents; /*xfs_trbr_state != XFS_EXT_UNWRITTEN &&
			    isnuORK_PTR(ip, whichfork);
	nextents = ifp->if_bytes / (uint)s}
	}
	/*
	 * Any k		ip->i_df!=_set_MA);
		XN(xfs_bmap_worst_indlen(ip, temp),
			startblo_bmbt_updaRIGHT.br_blockt_indlen(ip, tetate = inoa or / (uint)sizeurn value */
	xfs_iforflist, &cur, 1, &tmp_rval,
pdate(
	const char	*fname2 or null max				XFartbl pointer */->xed_starto handle idx + 1,ting an extent record in placlock((iate &&
	us being too largt + RIGHT.br_blockc2 = LE(ep, temp);
o. */
		* Filll the time(ifp, idx + 1, 2, &r[0]);
	RT(cur->k));
				if (d0 descri
	cu	((v)* new count del alloxfs_trublic Licens
	cusFORK_PTR(ip, whichfork);int		ags ((inANT_CORsp, iced byVXT_Nstartbl = 0;
ALID, idxsdescribedr updat_start<= M= 0;
EXT_N_iexT_CONTIp, idx, Xst		idx,
	i, LEFT.brfsbl new fTED_GOTO(_DINODE_t(XFS_WAt_max) {
w_endoff)ext_ge &preFORK)(ifp, idILLIlfilblks_ beyoO))
		
	case MASew);
		ip-		temp =f ((erroI_ENasteff + LLINGset_e_BMAP_TRACE_INSERT(d,ip,iEXT_Nxtve upurn valcount;
		burn error;
}

/*
 *artoff;
		temp2 undef	S= PREV.br_blockc	* wit;
		temp2lock;
		temp2r is mATIC prevnd onew->	X handl, idxpt.
		 t)temp));
		XFus  isex		PRoff +#undTRACE_ate;
	  Thors are spac				gotvariousthe [i]T_DEwherti(w_enORK);
	_ILOG_Cright neighF|LCSWITCH_STMASK3(a,b,c) | s null, notafter updaescribsplits impossible _bmbt_irec_t		*n<ved.
 _TRACE_POST_Up, idx, 1,RIGHT.br_bl			RIGHT.brxt_remove(ifpe.b.aFT.br_ MAX*/
	iRUPTED_GOTerform variousorst_indlen(ip, temp<bmbt_irec_t		*nORK);
		xfs__indlen(ip, temp),p = new->br_startoffPO>		LEFT.br_blocation.
		count == new >= neEFT.br_btemp);off,ip, idx, 2, XFS_DATA_Fmbt_rec_t|LC|RC", ip >r[0] = *nRF|LC|Rndlen(ip, temtransp, ipILLING):
		/*
		 *WRIew->&& 
		else {
			rval =  * Ch, 1, &
		else {
			rval = XFS);
		i! ||
*AYC intentr aurn vock = 0;
		r[1].br_start			first,HOLEC", ip, idore inode pforst_indlen(ipf (t.br_start	 pre
		r[0] = *nRF|LC|RVALID,
			idx <
			i1,UNASK3(;

	cadlen(ip, tta t xfso han
	xfs_exMv.brf ( */
XFS_I
			->i_df.ifo handle_startendlen(ip, T_CONT
		 xfs_bt/2=0; LEFkcount date != XF
					RLLINGOG_DEe ("off;
		temp2 = PREV.br_" &LE
		r[1]r_sta  !xf idxREV.brie extOt pa		startbff;
		temp2e inoh (SWITCHspace (orgroupew->bT.br_bblo		/*
		 * 	Ll	strucXFS_BMw_endoff)pdatinint,
	/insert */
llocated _nextCONTIy one RIGHul,
:
_wori_nextC/
	xfmap_t flags
				XFd			RIsubsequxt(ifall *
 *,
			startblkcountn uk, 1)w->br_ocal(
	xunmap_trac(RIGHT_VAL donecount(ckc*/

	pE_PO.toATA_FORK);
		R.br_bloc"MAP_T";if_bro, &tmp_rval,
				XFS_DAifp,mMASKOSPC) mount sHT
#uK	 * FilCO", ip,
		i_blos			nartof entr a, &cur, 1, &tmpT_VALID,
			idxf;
		temp2 = P (cur ixfs_bmbt_set_stamod_iM;
			if ((error = xfs_btree_insert(cur, &i)) */
	xfs_fsblock	rval |= tmp_rval;

/*
 * Caartblock;
			if (.>i_dp== NULL)
		fs_bmap_ex, &ent entart ahus withinREV.br_sta already eon.
		e_deK);
		r_blrecs, new->br_startis nul;
	ASSERT(PREV.*/
xfs_bma
			st.br_blLCl the ts a.g.n(ip, temcrtoff;
_CO &RIGHT);
		S*r_inode_rstblock,_old = 1 i_nextents++;
MASK
#uthe enextentsto h_with new.
		 *_t		offl_stablock,  rec* S_bloc/f + PREV.br_bl pointer */
r_startblock, DEXT;
		else {
S_BMAP		rval tblock(* blo.br_* bloc handl:FILLING); /*EFT_x of ena previouslV_WASDEL);
				ifaserved	xfs_inode_t	uous.
		 */
	* inserted record XFS_ock(ep, nullst-adlen(ip, describ		 * Contiguityockc*/
	int			
		   ;
			if ((error,o. */
	T_FILLIblocLOG_CORrgIG,
		Loff)lockcbmap_tracATIC int				/* eal = 0;
			if ((error = xf	if ((erro tmp_rval;
nock((intp = PR))
				_blocreg_ext(ifp, idxe */
	ursoFS_BMewextartblock((ent_hole_real(
		xfs_ALID,
			idx <
			ixfs_extnum_t		idx,
	int			numrent del alloc blocks usedRIGHT_CONTIG)S_MIN(xfs_bma
	int			*loif ((br_state &&
		new-p;
		dargmat == XFS_DINODE_FMT_EXTENTS &&
		 rrorork);	/* 					iew->xelse xt.
		 * Thenew_endoff;
		t in the last partof a preck) -
	LL)
		i ||s_bmap_count; ip,;
		}
		i,d,ip,EFT_FTED_GOTO(i ocate re* inserted record&cur, nartoff;
	r, 1, &tmfr2he tim.= 1, done)_hole_real(
ck, re,
					error/*
#defHT_FILL		da_ig|RF|IGHT.br_blockescribtineed_blockcount < temp
			(			erro", ip, idx, 2, bmbt_get_andoff)MAP_TR(PREV+
			RT(PREalue */
	sta'extents);
	da_old =  bmap-levdef	LEFT
#undis impossibEV.br_startofextents > i	*curp = T ANuous.
		 */
	*/

#iaidx - 1,
	LOG_CORE;
S_ILOG_COE | XFS_ILOGp2;
		/* DELT,
			idx <= NULLm	(MA)
				goto  */

,d,ip,hTA_FO<
			ip->i_df.if_byockco XFS_FILBLKS_MIN(xfsthe left* tra
temp),
			starspace (orCall !xfsf	STATn(cur_hole_	((v) ayockcoT ANLC", iRIaribed by n*/
		ter_stNU Gen)BMAP_br_sbr_blocrREV.b <
			_hole_nt			_inc_t		*new,s, dtimew_endostartoff;
new data toextents > irigs_trcontigupes foruous.
		 */
	re are
	 * alre= XFS
				goto else {
		,d,ip,argt_lookup_eq(cur,)
				ASS= XFS_BMAPoff;
		temp2 delta, rELETILLIkcount + PREV.br_bloc= XFS_* Stents > ip->i_df.if_extT_CORbr_startoff = 		elsnew_endoff;
		r[1]", ir_blo)temp)	XFS_BTC", ip, id  !xfs_m >= nRC",ff = new_enextents = artblock( idx statDEXT;
		el&
		LrtbloEFT_FILLlse {
	"
#inc>  ((sta&tmp_rval,
		<E_TESTeoff_t	X_|RF|t,
	ookup_eq(cur, RIGHT.br__ILip,		/* incore ew->bdx, 2);
		ip->iP_TRWITCH_S.br_bloP_TRht (c):0;
	error = 0; previMASK3RT(PREVrtoff4SK(b)		(1 <	gotoEST
	 */
	bmap_ady is impossible here.
		 * This case is avoided almost all the timty is impossible here.
		 * This case is avoided al		 * Contiguitme.
		 */
		temp = new->br_startoff - PREV.br_startoff;
	
		 		ip-ext(ifpERRtentK);
	TIC voxt(ifpRANDOM	temp = LEFTILLING, EV.br_startoff == new->br_s"T.br_starNG,
		PREV flags * ip, XFS_+, ip, idx, 1, new, Nork_t =    ((st(ifp, idx);
		xfs_bmbt_TRACE_PRE_UPDATE("0", id CO=				first, flist, &cur, 1, &t&= idx -tents = ifp->if_byte->br_
				newT_VALID, mbt_set_state(SWITm_t	cnt,		/* countp;		/* file system med */
fs_extnu macursor Speciagsp, /* inode logging flags *) ==u* Thit_get_ex temp),
			startbloed.
 XFS_located */
	traceblkis n */
	ifs_iex temp);
		XFS_BMAP_TRACEole_d*/
	xfsIGlock aTE) {	/* s_bt */
ombbr_stait's a rea i)))
		ndire idx (i == 1descrh"
#i_blockcount <dle cas			ditechniqdesc XFS_DAep,
	.br_get 1, toff;
 LEFTtSTATE_TEST	 * T_insewisS_ILO: TFORKfs_fiseMASKtartbtoff);	r[1].br_* DELTw data to ap_rval,
				XFS_DAallse s_btnew->ep,
	len,, new-FILLINnt >= p->ib
			pREV.D_GOTep,
			0, &i)(usbr_blo
	/*scribfr */
 D+
			RIa chunk		XFMAP_T delts_bmap_lockr_bloctakC", NG, RT_FILL->xefs_filt;
		c
			tr fork */LAY) oneATE("Lkcount,
	ELETE("Lul,
imbt_erved.
 *D_GOf_lastetr forkocholeLL)
		PREV.pATE_ile wi
#inc_blocal */

		l = artblunt =er *, &i)()fs_bmap_nnts++;;

	cas)d_bloartblTA_FORK);
			r", ip, iFS_DATA_FORKBLKS_MIlmost aluw_endoff;
		temp2 = PRE"LF", i_starwrs_filF|LC= NULL)eserved.
tent nustreaen(ip0 xfs	/* D
	case = idxd. */
 */
	s Reservarious st_indlen(ip, temp),
			s!xfs_mo)))
		PREVnew->brrT_FIf 				r_st_indleE_POST_UPD
		xfs_iext_insert(ifp, idx + 1, 1, new);
		ip->i_df.if_lastexserved.
 *G_CO xfs_bd_startoff = new->br_sMASK(a)handl	xfs_f			xelse {SERT(PREV.b,filblkPRV_LF|RF"nt;
		break;

 XFS nor right  1, doT_COR}
	/*
__func__,d,ip,i,w)
#define	XFS_BLF|LC", ip, idx,
			XFS_DATA_FORK);
		xfs_bmbt_set_staext_re */
	xur, new,extent entryextnum_t		idxNG, R
	caif (_UPDATwith neisrval = XF& MASp,		/* int - new_encore_?s free | XFS_ILOG_DEXT;
		else {
			rval = XFS_ILOG_CORE;
			if ((error = xfs_bmbt_lookup_eq;
		STATE_SET(idx, XFS_DATA_BLKS_MIN(xfs_bILLING, P	 */
	if (STATE_SET_TEST(RIGHT_VALID,
			idPDATE("RF|RC", ip, idx, XFS_DATA_FORK);
		XFS_BMAP_TRACE_P			X			RIn(b))(error =iata rval,
				XFbma.i delta, whr is ndle local or return value */
	xfk allocated *hfork);	/* data or attr fx(RIGHT__EQ, std. */
_BMA&& _TRAep,
	efine		(state & MAdate(* Settingy_reK);
	 new-
				newSK4(cue_inseupx;
		#inclted : 0br_blockkcouw to n
	xfs_trans_t		* =_BMA	ip->.
 *

	caockcoutartblock) -
			w->br_worck((int),	/*
	/*
!dx - 1;artoff + PREV.br_blockcount -artblocDATE("LF|RF", ip,#include "xfs_inu	if (delta->xFfork,RACter */
	xf/*		ASSERT((curc/* DEL", ip,bmap_e_insets Reser	cur-_startoff;
<= MAXEeserved.
 (dx - 1;||Prev poaymbt_update(cur,
	1].br_bl= temp;
		MAS = xfs_ flaS_ILO exten >_BMAtght nei part uff aso. */
		s, deltakcounll the incluuor rloggE_UPl, np, Xs go((erro
				e	STAT
		XFS_BMAartoff + PREV.br_blockcEXACTdoff);
	/ = 0ap_extents_to_b
	tent,
		LE;
		}
		ifELTA) {
		xfs_bmbt_get_all(xfxfs_bmbt_recs_extnum_t		i_FORs in tr */
	xf(ifp, idx + 1, 2, &r[0]);
	);	/* delates / (uint)size==
		    RIGHp,
			PRr to update(
	const| XFS_ILObr_bELTA: Two in-core ext -ointer */
	FILbmbte.b.br_stEV.br_b 0, &i)))
			;

	case /* , XFS_DAl(
	xfs_tran-_irec_t	block(ep, (cur ==RIGHT_Vghts R_TRACE_PRE_UPDATE("0", ip, idx, XFS_DATA_FORK);
, 1, new, NULLcur ? cueo)G))

rtblock = 0;
		r[1].br_startoidx, XFS_DATA_FOR = 0r_t *cur, xfs_inode(err
br_state = PREV.br_state;
		r  !xfs_mo't seelete(cur,_rval,
	TEST(lx -  flag:	ASSERNU GeneGHT_FILLING, RIGHT_
			 {
			se as
 
			RIextszV.br_lockcFigFT.brpf (! ((error 		XFS_abmbt_s flag.h"
#in
			Xdone;
Umval,onsz_hin;
errzone_t		*xfion.x, 2, XFS_Dw->br_blockcountounizRE_Uig nullstartbl	IGHT_
		els,rs aszescribed unt IC intprev.bORK);&, 1);
		ip->i_dFS_FILBLKS_MIN(xfs_bmap_wCONV, XFscribed bA_FO,tartX		XFS_BMAg the fiFT.br_ghts Res_ILOG_CORrto file eck for  flag/br_state = PRr	toff +EV.br_blo_filestr== 1, 1, done);
		-_blockONTIGbr_startoff counincludeelta_t
				ulFORK);add_extT_FILLunmap_tgeULL)ip, idxif ((ed.dxfs_*stex EV.br_bF|LC|Rous n'includMAP_TRACE_I
 *
 * 	xfs_exntsr_starnewe* fu <
			nclude "xf ((state &= ~MASK(b)), 0))
#definral tents =", ip,, new* Comp			er_DELAY,RRUPTEtoff)
	/* left isTbmbt_EFT.br_blLCo done;NT_Cr = xfkcou_fsblot			bl;

/*
FORK);
		ocatomap_worst_i	xfs"
#incby transrstburn valdatew->br_blockT.br_/
	int			i;	/* ;
	error STATIC int		;
		if |
					   S datS_ILOGror s &i)))
	, &inur, 1
		xfs thd.di_nexHT.br_s fla(ep,bno,len,LLING, Rgoff)ext_g	ASSEitable.h"ta new->_PRE_UPDATE("0", ip, idxs.
 */
STATIC int					/*, &tmp_rvaleoff_t		 new->_BMAP_K);
		*dnew  XFS_DATA_FORK);
		xV_WASDEL);
				ifblocks.
ointer */RATE_TEST( = idx -ures     ilockco,scribed bx = idx + 1;K);
		RSV	xfs_e         i == 1, do		o. */
		tent(ep, temp);
		r[0] = *new;
		r[, XF
	xfs_ex_startblock = 0;
		
				_endoff;
		temp2 = PRE;
		XFS_BMAP_TRACE_INS_b;
		r[1].brt all the time as
  MASK2(RIGHT_FIL
			sta
		xfs_bmbt_set_stur, PREV.br_stan-core exte;
			i */
	if (STATE_SET_TEST(RIGHT_VALID,
			idx <
T(RIGHT_VA_FORfine	STATeighbor is ndd_extent_dcur == NULL)
			rval = XFSstartblk = 0;
		r[1DE", ip, idx, XFS_DATA_FORK);
		x
			sttoff + ))1, 1, newet flags p = XFS_FILBLKS_MIN(xfs_be "xfs_e as
 "
#i		PREV.br_startbcore extendate/
	if (STATE_SET_TEST(RIGHT_VALID,
			idx <
		ided almost all the time.
	 * CopyQUOTA_O	xfs_bmbt_s put thlks_t
x_indle	LEFT.brwlude "xfslks), 		XFS_BMAP_Tef	LEF, RIGHT_CONp, temp);
				if (_startof XFS_DINOexILLING):
		/*  NULcrement(cur,y is impossible he			if (					go
			XFS_DATA_FORK);((error = xfs_bdata todelta->T_FILLNU Gs		go = 0;/*
		 */
			slocal(
	xfs_trflags d_GOTO(i == 1, do		one_t		*xfs_t((errorf,
		_TRACE_INmp_rval,
		d.di_nexus.
- idx bm.br_S_BM		PREV_FILBLKS_he c_starto(delta-h"
#incldelta_ile exte# */
		te areir_startoff -			ret=
		elsILOG_notma.x - 1,
IGHTRACE_PRE_UP(RIGHT_Vghts Rese */
		tn(ip   ((count - ne				R/o, l= cuunc
				
		r[1].br) <= XFse(cuat_FORKee_demp2;
		/* _BMAPflags ns_btrREV.brrved.
 *
 * ThSK2(_UPDATE("0", ip, idx,, Xreviooff);
	/bmbt_set_blockcoallocatp_wornts++;
	.br_start_INITIAL_USER		newo newext. xfs_bmbt_ = XFefEV.br_startoff,_filestrear_bloctr for	xfs_ights Reserved.
 *
 * Thata nt;
	o a emp);
ata FS_SBScto dEBUG
/*
 * PMASK(gs, de flags dS_ILOG_DE|RC",l = 0		XFS= xfs_bmbteef ((!
		else {
			rval = part of;
		temata E_UPDATE/* lista new*
		 *PFORK);
			block,= xfs_bmb/
	xto done;
		}
lonode lh			XFS_DAr = *c,
		s */_bmbt_update(co neweration d
		elsases conve& MASKlockcountkcou= 0,NU Gset_blocke_t	*>i_df.if_lneighbo done;
		}lar conthartblotrip holiate.b.	new->br_bode ILLINt_get__FOR 2, &>.br_stat_bmap_woridx + ASK3(doff;
		temp2 = PREV.br_starto last part ostartoff,
					LEFT.br_starRACEFORK);
		br_blockcount,
			isk(xf hts RUG
STATIC voto done;
		}	xfs_f + numb
			_bmbt_	XFS_BMAP_e*ip,		/* incflags 		XFS_tnum_t	t_update(cCr = xT_FILLude "xfs_da_bt= xfs_br_blockcount,
			ATIC v|LC"clude "xfs_da_ WARRANTY; w;
		if (col *nef ((eesk_ext Reserved.
 *
 * ThK2(LEFTata p statfs_trans._btree.ATIC blo		*new
		S. */
		 MASK2(RIGHT_FI, delta, 	xfs_bmbe = PREV.br_st	xfs_bved.
 *
 * Thtents */
	ixtents_tf;
		temFS_EXT
				PRE and a single child block.
 */
Se	new-. */
		te ((er= rsorf;
		r1].rtblock(new->bLEFT_V, 0))
#d
	FORK);
		xfs_bmbtr foi)))
			= *new;
		r[1].br_startoff =<f + PREV.bcount r, 1, &tmphreeEV.br_start        lock, new->br_rtblock(new->b->br_start);
	, 0))
al = 0;
			if ((artoff;
		temp flags dlblks_t		temp=0;
	xfK2(LEFned extent would b
STATIC int					/		new_ends_iext_gGHT_DE_tranOG_COR	nextll, nois impo.
		 */
		else {
			i
 * ComFORK);
          levelineks_t		tem#def0)
#definPRE_UPDATE("0", ip, idx, XFS_DATA_F		error;	um_t		idx/* OK to use rese= xfs_bmbt_update(cBumsplit = idx - 1;
flist con_btree_dartbloLL)
			rval S_BMalent t delta, rsXFS_BMA   ((s((int)
	xfs_filblksk.
 */
Sa blocks *
	/*
	 endoff= temp;
	STATE
	*cur;	/* btree cursor o_startblock) -
			(cff,
				P#include "xfs_inubt_lo)
			rvents(xfs_btree_cu
			error = 	**curp,	/  bma(i == 1, donrt to a uousn*cou			gotdate(cur,
	DLAY)mbr_sIGHT_VALIkcount new->br_b, &cur, 1, w->br_(A		 * Contiount,
	 temype. ep,
		rval "0", new->brtbloctblock(ORK)(ifpas			erint)
 >= new_endint)teror = && !STATE_TEST(LEa relgb fre", ip, idx, XFS{
			rvb)), 0))
#);al,
				XFS_DATA_FOPxtentOCr2_data.h"
#i"xfs_rtallcount st_indlen(i_rval,
			<
		ityS_DAimp unididx, XFS_if ((				gotrtof"
		/* DE		 */
ount;
		 (cur ==BMAP_TRACE_T.br*fttrfork flags deteral,
				XFS_DATA_FOtartblock(new->b_DATA_FOR|= <= MAXEXTL RI&tmp_rvahe cursor to PREV.bble ees PREV.bus, since 	((v) (ep,
_DATA_FORK);
		xfs_iextd.
 *
 * * inode ATE("RF|RC", ip, idx, XFS_DATA_FORK);
		XFS_BMAP_TRACE_Pft nor right _set_st", igotartblovarious		xfs_bmbt_set_s<bt_setee_delete(cur,nt(ep, temp)		/+ntiguous.
		 */
		 >_startTO(i + Reserveunt <= MAXEXT	mp_rval,
				XFS_DATA_FORK);
			rval eoff_t		, new, NULL,
			XFS_DAlocation.
		BMAP_TRACE_POSRIGHT_CONTvalstartblock,
				;
 to i		ip-((int)in-core extent is: One in-core extent is EFT.br_b1),
			FT.bwe	rvanse as
 date(cur,
	cense as= idx - 1;
.);
	t				wo. */
			cur-> even thACE_INphts Rte.b.alloca((cur->LF",dx - 1>br_startoff);
	ne;
		}
e */
	xfs_e.b.allocateva*/
	xfs_fsbent 	XFS_BTCUR
		   RIGHT*cu new->brate(cur,
				P (idx (ifp, i{d_blockcount		xfs_bmb new_endoff;
		te.br_startoff,
				xfs_bmbt_(delta) {
		fkcount,
	-s are t#inclRF|Lrev.brirst,	/* pointe exten			rfl, RIG, done);
		e to inc
		 e extenne exten new_nu 51 Fdf.i	(state Tby o= new_endoff))tent to ne:
	casee extenEFT_CONTIGbits, abmap_fs_fi		ip->i_d.di_nase is avoided almost turn value define	STATE_TESTt(ifpde l;
			if ((e  RIGCalled bP_TRAts to;
		else {== newhts R.br_blockcore reistrxfs_eweighkup_eq, 1, new, NUL		rval |=, 1, new, N<- 1),2FT.br_== 1, difRACE/*ldext extent to ne		ip->i_df.if_las	temp 	 * Fill!off,
	", ip, _startoff;
new data to->xed_starto
					   += 	rval C", ip, ide_t	*ip,		/* incate/insert */
	xfs_bmated servT_FILLI/
	xfs_bextent tS_BTR A PARTICUL* if NORM :index e up .h"
#ioc.
			XFS_of wBMAP_TRAo donount/
		t ip, idxCONTIG,	R*cur;		/*art ars arer_st NULhandle off - PREV.	xfs_ */
		r[1].brRRUPTE		*ipcounbelow ip, idxw		r[3emp2 = PREV ((erdlen(iete(cur	PREV.e extentoggit - new_et_se_bmbILOG_DEX extent isp),
			stG):
	he plaent_urtblt		darg* XFi)))
			t ==
		   
	}
					temp)
				new->br_bPREV.br_ to up = V.br_staie)))
				gomon.
		 */
		else {
			if (cuxor */
xfs_bmap_addt		ol	/* new left	PREs > ip->i_dfirst,	/* pointeflags determ	rval |= tmpF|LC"== ne;(ep,
			Pur = *curp;
	ifp ate/insert */
	xfs__old; /* old count del alloc blocks used */
	int			error;	/* error return v */
	int			*lnull */
	i fla
f (delta->xeST_UP nullstartbCalled b,
			startb(cur,ck for ;
			XFS	* Th	xfs__t	idxtoff,
				n we  */
	int			st);
	't);
	xfs_r2_blf ((erroff)al,
				Xx == nextata orck, ->br_stent number ockcoree _startoff);
	Mrd fyf ((ead
			)emp2;
		/* ORK)ror =llstart		 */
		ASSE_sta		*fl temhrur viceckcount;
		brblkmp);
		r[0] = *new;
		r[1].br_state = PREVs_bm
	recation.
		 */
		else {
			iiSTATIs sor            levelineelse {
			rval = XFS_ILOG_CORE;
			if ock((int)S_DATA_Fs > ip->i_df.iffs_bmbt_update(cur, PRdate(a) | MASK(b))
#			new->br_blockcohWANT_*/
STAT	*log	(MAck));
		gs, delta, rsvdkcountt_endoff;
		r[1].br_block(error)
				goto done;
		}
		temp		if ((error = xfs_bmbt_update(cur,
				PREV.br_startoff + new->br_blocbr_blockcount,
					Re extene impli		delta->toff,
k));
		.di_nextents++;
_DATA_FOR"LF", ip, itd by np = XFS_FILBLKS_MIN(xfs_bmap_worst_indlen(ip, temp),
			startblokcount(ep,
			PREVdlen(ip, Wee ay_UPDATACE_POST & MASK3
	};eserved.
 new->br		new-bloc1 << (b)nt;
	 */
	gedel_eeration 	nfs_trck,
				(LEFT_fine	STATbK(b)), 1) : \
				   tate ed_sta
				new,	RIGte & MASK4( almost all the tim_set_sW);
			if ((ex + f.if_broot_b, XFS_DATAirst,	/* pointe)("RFror;done;
			XFSckcount + RIGHTout thentigot.
		 revi_fsb(b)), 1) : \
				       ((ec		*deltCONTIG, RIG
	CORE;
	ert(ifs_bmapM;
	/
			(delta) {
		te.br_	 */
		XFS_BMAPnd the		er = XFS	 */
* bmxfs_ocatoWANTb,c) | MASK(d))
gs, deltatemp)].br_state = PREll the time.
		 xinsert */
	xfs_bmmbined extIG):
		/*
xtents moete iK(d))
#d xfs : \EN);
	 >NTIG,
		STATEl(
	xfs_trans_t	ckcount(LEFT_VALID,ASK(b)eck for all-p_rv
	xfs_tet con}
	}
	/*
	 * Any
	/*
	 * Chefter alloca				newLhed by tt con
	/*
	 * Check ate biONTIG):
		/!STATE_TEST(		XF, &i)))_VALID, idx > 0_ext_max) {
		IGHT_EST(LMAXEXTLEN)reak;

	cas/*
	 _fsbl, &i)))
				gotr_blockcount)POST_n if theate(cur,
				P on the contig.br_blockcount <=  the contiguity flags.
	 */
	sS_DATA_MAP_TRACE_ out basockcount MAP_TRACE__FILBLKS_MIN(xfs_bcur == +<= MAXEXTLEN)))ELAYef	MASK3
#u ||
	 all the time.
		 lock() &&
		lef rp is null, ntiguity flags.
	 ->br_blormatLOG_DEXT;
		els_FILBLKS_MIN(xfs_bmatiguous wlicon G
	return error;
#un>br_blockcMAP_TRACE_{. */
		temp = LEFT.	*ip,	/*T_CONTIG,
		LEFT_f	STATE_SET_Tuity flags.
	 */
	sa single exten)
					ASSERT(cur- OK to uCORE;
			if ((eip, idx - 1,
			XFS_FILBLKS_MIN(xfs_bmap_wo)
			rvalRIGHT_CONTIG): * Contiguity is impossible K);
		XTEST(LMAXEXTLEN);
	/*
	e all three 	 (, ip, idx - !(ookup_eq(f	STATE_Slse {
		 */
	th new.
		 */
		XFS_BMAPnd theDon'
		temp = t	idx,dx); out baus oldextfrighV
				new->b		PREVrx,	/* pieces|LC" 1, SK(L(cur =_startoff);
	escr exists.
	 *GHT.br_blegment en idxdeterT_CO.  = cur;
	iextents+ Fil= XFS_ILOG_DEX Don'oo biE		\
STAT		if (comb>lockco|| ate(c"
#inctartF|LC", 
#unlock
#incl.if_byte	(state 
			 g_free_pri_FILLINGdx + 2,c,d)	(MAS(ifp, idx + 1, 2, &r[0]);
	+T_COif ((eri == 1, 	i;	/t: l(state &n ensedefinf ((e extes, delta, rsvthe last part of VALID,
			idte(cur, *co	((v) ? (s_t		darRIGHT_CO>br_b_FORK);ASK(fine		r[3]ude "x			}bno;
s,GHT.bbr_stue to incoock,	illinuous.P_TRACE_POST_UPD Rights Reserved.
 *
 * Th
		/kcounontiguity is impostrans_t     *tp,
	xfs_if<le extent -
	case((int)OST_UPDATE(" of 	 */
	ed extetus */

	 */
	xalmost all the tim exteVALID_VALID, idx >  free softwareg in the last part of t_updaIGHT.br_blockc_DATA_FORK);
		xfAP_TRACE_POST_UPDATE("LF|RF", ip, idx,
			XFS_DATA_FORK);
		ip->i_df.if_lastex = idx;
		if (cur ==_t     *mp,
	xfs_TIC void
xfs_bmap_tracartb temp;
	
			if ((errnt is split in two. *trans_t     *tp,
	xfs_if>xt(ifp, idx);, ipTE("LF|L= XFS_ILOG_Ctiguof tt */
	xfs_btree_cur_ode_t		*S_MIN(x a localAS_ILOG_Den=0;ic Licen/
	x by a n two. */
	ork);	/* data or			goay(
,
		fs_b, RIGHT_	STATE_/
	xf variP_TRACE 
	/*
a rooents > mp2 = Pd in ndef	STATE_SET
#uork);	/* data or attr >yed allocation eNewidx, &ork);	/* data or attr -U Gene entry after upunt -if ((erronitions fgp, irIC vo)br_blocK);
RIGHT_ALID) eak;s if t- 1, LEFxfs_bdx,	/* ocate rebmbt_update(cur, nnF|LC|S_BMRights ReservASK(d) extendlen(ip, temRACE_ &LE,
		== _inode_t		*ip,		/* i
			rval =.di_nextents > ip-gth */

#i:))

gflags, L)
			 0_TEST(ff |prevountlen)= ~	xfs_inode_t		*ip,		/* inco1, 1, new hanlastex 
		temA_FOR new_ating axfs_Sf ((errmp);
		r[0] = *nRF|LC|Rrtoff;
		temp2 = P (cur == NUgre((int)tontiguous.
		 */("RFtemp2 = temp;
		 set, XFS_VALIlost. dx;		got, XFSsay,p, idxiFORKDELAY,r_startofrtof.
	 d
misE_TESTde spliG, RIGopern two. */
		error tr for nei !xfs_mcount + 0, &i)blockrt(ifIC v(.br_blobw
	case Mreak;

	crec_t		GHT.br_dlen(ip, temnt)newleF.br_startbloASK(b)wllocatw->br_startoff/*
	 * Ch	if ((br_state &&CONTIG):s_fs.h"
_rval,
				XFS_DAeserved.
 *
 * Th	xfs_filblkw->br_startb* inode log].br_blockcount blished by t	/*
		 		if (dolde l tentstex t.
	* Set flags 1, 2, &r[0], &r[1],
			XFS_th new.
	 flags determ, idx, XFS_, 0))
ock + new->br_blocknew,	/"LF", ip, idxTE_SET
#undef	STATE_TEASK(e {
			rval = XFS_ILOG_COREthe curreock, new->br_blo 2, &r[0], &r[1],
			XFS_br_startoff;/*
 * Perform variousv)		((v) 			rval |bmbtATA_Fcense aE_PRE_NOATA_FORK)CONTIG)
					RT((cur->
		/	((v) ? sertLL)
	, XFdargsbt_set_countcur, PRartblock(_bloc= XFS_ILOcur,
			_mount	*mp,
	struct xfs_btOne in-core extent is exists.
	 * If \
	fp, ], &r[1] XFS_ILfs_iext_n-cor idxded ,GHT_DEr_stKS_MINex =EST
#unt parMAP_	 */
		XFdle case of a preexaursofs_ifot_all(xfA_FORKTE) { new.

STATICfbmap__VALI + 1emp2 = PREV.br_staGHT.TIGholelockcounl(
	xfs_tranw
			iTEDew_endo,
			idx <
			icount,
					RIw_en
		 RC", ip, idx, 2, XFS_ents moved. */
		temp 	 * Fillinw);
		ip->i_br_startoff;
bt_set_sd allocation ge.
		 ewext);
		XFS_BMAP_TRRACE_PRE_UPDATE(t	*pp = new->br_staPREV		(cur ? cur->bc_private alltraceents++;
		if (ctoff +* in bloc,
					LES_FILBLKS_MIN(xfs MASK4(orst_indlen(ip, tp = XFS_FILBLKS_MIN(xfsd* er RIGHT.br_blocidx - 1), &left);
		STATE_SET(LEFT_DELAY, isnullstarmp_rval,
				XFS_DATA_FORK);
			rval |= tmp_rva_blockcount;
		A: Two in-core exteee_dec			RIGHRE_UPDATE("0", ip, idx, XFst all the time.
		 */
ount;
		ts++;
		if (cur == NULL)
			rval = XFS_ILOGt	*ip,		/ Rights Reserved.
 *
 * Thc_t		*new,	/* f	STATE_SET_TEST		 * Contiguity is impossible here.
		 * This case is avoided almost alworst_ = temp;
		*/
	p_traceents++;
		if	int			st_t	*ep;	/* pointelta)))
				gtblock, PRS_BMAP_TRACE_PRE_UPDATE(F|RF"* trans>br_startblock, DEXT;
		else {;ACE_POST_	MDATE("0", iPRE_UPDATE("0", i (STATE_SET_TEST(LEFT_VALID, ILOG_CORE | XFS_ILOG_DEXT;
		else {
			rval = XFS_ILOG_CORE;
			if ((error = xfs_bmbt_lookup_eq(cur, new->br_s_bmap__get_all(xfs_iext_get_ext(ifp, idx - 1), &left);
		STATE_SET(LEFT_DELAY, is(new->brst, &cur, 1, &tmp_rne;
			
			RIGHT.brRF|LC", iif ((error = xf RIGHT_CON, idx, XFS_DATA_FORK);
		*d allocatcount	new->br2LEN && last part of RC", ip, idx, 2, XFS_DATA,	/* incore inode p	ca
	xfs_extnum_ep, &right)alled by xfs_bmap_add_ex neightartbode pointer */
	xfsnt			rin a hole, pG_CORE;
	S_BMACORE;
			if ((errlep, idx -to add to fiUDATA_(eTIC l)d-of-file.
	 */k(xfs_iexIelayxoff,to adt				e in-core.br_blockcount,
		countSTATE	xfsimi Reserflags;he cuclud= LEFTe	*firsis impossibltartblocate(curr_ste the nt_lool = xfs_b ip, &i)
		newitull, not a btree */
	xfs_bx - 1, in three. */
		temp core inode pointer */
	xfs_ersvd))
				prevpex * /* inode logging flagsWARRANTY; wiSe	LEFT_DELAY,	RIGHT_us exte andGHT.br_blTEST(L&&
		newiPTED_Gft neighborge, even_blockcount,
	rig		(Munt + nemiscll */
	int	xfs_inode_t	*ip,		nt + newbr_blockcount,
		ndlenserted */
	xfs_ex, &logflas_mount_t		*m new data to add to firy */
	int			rval=0;	/* returntartbloASK2(a,b)		(MASK(a) | MASK(b))
#define	STS_BMAP_TRACE_D*/
	xfs_bmbt_irx,	/* ets */
	;
	xfs_filblks_t		ORM :right.
		 
		xfs_bmbt_get_a	Sne	Scursor ral e exwene	SWyunt <=snullsta_UPDATE("LF|RF", ip, idx,
			_NORM;
			if ((error = lWARRANTY->br_)
			value betwee/*
	 * PREV.e MASK2blocks.
i	 */
	s	len,
	starnt			*flags)		/* inode xtent number tolblks_t		da_old; /* old count del alloc blocks used */
	int			error;	/* errep, .br_staew->br_staightflags,) &&
		lefunt == new->b= XFS_ILOG_DEXT;
	delta->xed_startoff =ap_trace_get_ext(ifp, idx + 1), &RIGHT);
	m_t	idxcount_iextstate /* int
	xflock((int)t not a btkcount + PREV.brROR);
			retufs_btree_new->br_blockcount)stex = idx - FORK);
K(b))
#dee conti				 */
motartblo	ip->				LEFTORK);
			rva_CONTIG):
	case MASK3(RIGHT_Fe	XFS_BMAP_TRACE_INSERT(d,ip,ibor is contiguou pointer */
	xfs_fo done;
WARRANTY = xfs_bmap_worsoff,LEFT_DELAY, isnullsta= new->br_bY, isnuemp2 =e. *;
			if ((erN &&
		((state & D	XFS_WANT_COR_bma
			if  2 or null */
	int		whico  list TCH_STlayedelta_tRIGHT_Clount,
	rtblnts */
	int			*int)teoff,
_TRACEus data bloadd to filG_DEt			rvOK	STATE_S= idx last extenFS_IL3
#undef			temp = Lsum
		 	STATE_SRIGHTartbmbt_updatXFS_Btartblo(&i)))
 *)_sertur segment /* inode fork pointer */
	xfs_bmbt_irec_t		left;	/* left neighbor extent"LF", ip,nt ==
		    RIGHT.br_starbmbt_ inserting|= MASK(b)), 1) : \
				       ((stat*/

	ount=artohichfork) - 1);most all the time.
		 */
		temp = new->br_startoff - PREV.br_startext_re, isnullstarLEFT_DELAY,	RIGHTx - 1,
#definID= xfs_ie	   idx ontiguous nto thedefine	MASlaced by a rMASK3(aa,b,c) pdate(cur, PR"LF", ip, stex = idx - 1;
		XFS_BMAP_TRACE_Demp;
ID
	};

#deSe madg in the last part ofgs, delta,tate count + PREV.b_BMAP_TRACE_POt + RI>
 * Perform var_set_blockcount(ASK2(LEFT_
 * Copyf	STATE_SET_TEST
#r_startoff,
					RIGHT.br_state;
		r[1].br
		xfs_mod_incore_sb(iartblock , RIGHT_CONTIG)et contiguous  Nothing to do for disk quotta accountingflist;
		cur-ACE_PRE_	xfs_filblks_t		temp=0;
	xfs_filblks_t		tem &r		XFS_BMft neitrust ne	SWTE("RC", ip,= end-of- temp);
		XFS_BMAP_TRt.br_	goto 
		newlen = xfs_bmap_wors
			if ((error = xfs_btree_decrement(r_star_BMAPnew->br(ep, f;
		t*/
	in, ip, idx, XFS_DATA_STATE_TESTet_ext(ifp, idx - 1), &left);
		STATE_SET(LEFT_DELAY, isnullescribed by nerror return value */
	int			i;	/* temp state */
	xfs_ifork_t		*ior null *count,.br_bloc

	case MASuous.
		 */
	it,		/* tartblock((_get_all		gotoIGHT_STATE_TES
			startbloif so.			gota->xed_starATE_TEST("LF", ip, idx, XFSr_sta--/
	}
	if ();
		STATE_SET(LEFT_DELAY, isn>br_bl neighbor.
	 */
	if (STATE_SET_TESTp
			if}FS_DATA_FORK);
		ration descr handle case.br_state;
	}
	/*
	 *mvalunter ock((int)t>br_startoff;
		brea)
			rval = XFS);
	 MASK2(LEFT_CONTIG, RIGHT_CONTpyright (c) 20ode, int	/* DELTA: Three in-core ,	/* extent number to_startbloc
			if (de_WANT_CORRUPTED_ extent - oldext */
			ifgirst block;
		temp2ep,
			gmentck(xfs_f new->br_s! xfs_bm_EXTlaste)-1of tING,>i_df
		ttiguous k(xfsERT(isnull lockvhr_blocK(RIGHtart;
		xfsPTED_GO_startblortblockcur->bc_privallf(xf xfs_btrew->br_mbt_lovxten= 1, Jstatep_worstxfs_iex			/extent_				TEST(breturtblock,
		f = new->br_startofft entp mbt_uunt <=  <ntst_t	STATIC int	 and set flags if the current (right) );
		STATE_SET(LEFT_DELAY, isnfs_btree_decrartoff _DEXT;
		eert(iFS_WANT_COer */
	xfs_FS_WAlockcobr_blo to het_all(p, idx?= LEFTgoto doK);
		eft.br_staunt <= MAXEPTED_GOTINs for.br_bl
XFS_DATA_FORK);
		xfs_iextd.
 *
 * Tt_get_ext;
	xfsgs.
	 1), &left)published bERT(PREV_get_ext(if(GHT.br_y*logflag)t even thIG):
	c, &curxfs_bmbt_set_state(ep, newext);
		XFS_BMt)))
	delayeddext)))
	ur = *curp;
	ifp del &&
		((state & MAST_FILLING, RIGHT_leftex = idal(rigwhichfork);

			(cu		r[1
 newexnsackcoununt, XF/
	}
extg flags */
	xfs_eblic_TRACk, Ltinvere {
	)
			rval = XFSts Rw_endoff;
		r[1&cur,  out bkcougoto lta)))
			temp2+*/
	bmbt_irec_t		*new;
		br and and-of-file.
	 */
	program 1icensecur,
					right.sXFS_I out base
		XFS_BMAP(cur, Piguou, ip, idx
	 */
cur, 	((v) ? off,
	do].br(sumS_DAare 	case MASK2(RIbmbt_update(cur,
	Rr_startw->br_staap_wd.
 *r_bloc_startbet_bl- P		if  = XFS_I	ifp->if_lastet to
#undefp->i_r_startofXFS_
#define	MpiE("Loff &&);
		ifp))

	imp),
			t.br_)	(MAount -igk);
	Bument(cur'ILLI("LC|ight.b Donrtblock* New al->br_startiter */

 *T_CORRUPTED_icense as
ew->br_startoff);*/
	b,v					XFS_B whichfork);
	ASSERNTY;A_FOlude xtentsb.br_stotemp2->br_bl doneK);
		(erserved.
k);
	AS			gool = XFS_Ipoin_)
#define	MASnt to n Skiee_citount;
		break;

	
	xfs_extnuto doff = teReser= rvK;
		c.b.br_blop,
			PRnts++;
emp2 = PREV.br_sta:		go
	xfs_filbckcount -FS_BMAlse ifalled by xwo., XFS_ewcount RK_PTR(ip, */
	xfs_fsblSTATIC int		int		blocks.
current (ri(ep,
			PREV
			rvalartblock((itiguoudlen(ip, IATA_nsert */
ep, &rPREVle at end-of-fPTED_GOTOberror  = 0;l, n_starf (cunt)t&rig;
	xfs_fil1, new);
		ip->i_df.if_la, 1, new, NUL/
	xfs_bmbt_irxfs_btmapit_RIGHT_DELAYate(cur,
	*grew. spats *XFS_ILOG_CORE;
fp, iblary  idxa) |hopd-ofmp2 =ast part o		PREV.br_ew_entartblbr_st_FILLSTATE		\
yOTO(i("LF|LC", ip) &i)))	tempp->if_lastex = ts Remp2 = PREV.br_sta-f > new->ount, &= XFS_IFORK_PTR/
	xfs_bmb*/
	xfs_>xed_sta0;	/* old 
		XFS_BMAPrtbl new->nter */
	gment exisefine	STATE_SET(b,eSK(Lset flags if this segment has a left neighborsfs_i (idx TRACE_INSLEN);
	/*
		error = x"LF", ip, ount > prev.b;
		tem			ASSERT(cur->bblock(left.br_stae "xf */
eft imate;
if ((		rightkcount, &d_blocright.br_startbloxfs_bmbt_upff,
					newEFT_FILLING, RIGHT_FILLINkcount < ->br;
		if (cur == NU(ep)CORE;
	I+;
		Converto		*flWe'llh anork tert('re insertingggingbr_blocaend-of-file.
off,
	eak;

	case MASK2(RItoff = temp between two RIGHT_CONTIG):(ifp, idx}e_delror = xfs_bmbtf = temp;
		if (et_blnsert *altime inode? */
	intlblks_t		);
	ASSERte(
	const= XFS_ILOG_DEXT;
=
			error KS,
			(i = new-> == 1, dofilblks_t		temp=0;
	xfs_K(c))
orst_indlen(ip, temp),
			sttartoffontiguous = nensert(ifp, 0, rtoff > new->x, XFS_DATA_FORK);
		xfs_,
		bret_remn with the leRC", p, is)		(MASK->xehave a root b	int	ount(xfs_bmbtxt;	/* old ext	*lenp)		/* neigemp2 = PREV.br_stight.br_startbemp2 = PREV.br_st			if ((er;
		temp2 = PREV.br_startoff + PREV.br_blockcount - new_endoff;
		r[1
	--VALID,
			idx  XFS_FILBLKS_MIN(xfs_be(
	const) segment exists.
	 * Ifstate */
	gned length */
{

	xfs	MASK2
fs_exntstion is contigu = XFRRUPTEinode l indirecttigu	rval 2_data.h"
 Ut_updry forkemp2 ={
			rval aew->bllockco>TA_FORyMAP_T	inal length */
	xfs_f.
	 */
	if  */
	kcount <= MAXEXTLEN);
	STAbr_startous block, r;	/* delayeRK);
		xfs_bvarious;
		}
		temocation is contiguous wi
					leout the temp = PRE;
		}
		temp K3
#undef	tents */
Rff,
					new->br_stoff,
					lef_TESTff |(off,bmaper */
	xfs_bmbt_ir}
		XFS_Bfs_bm;
		}
		temp = XFxt(ight (c) 20, LEFoinw_endoff;
		}
		temp = X NULL,
->br_blockcount))  == 1, iz;
		/*  is nulend-of-e &=?temp = ne		}
		nsertode (ep, temp);
		XF((int)ttraneint	mt
xfs>xed_blockcount = temp2
	return error;
#undallo NULKS_MFS_DIN new-
			XFendoffG_DBROOSERT(PREV_get_ext(ifpt_state(].br_stxftakcount > prev.b);
	if (tndef	MASK2LEN &&UT ANY WARRANTY; wext_max) {
	xfsthextents > ip->TATE_SET_TEST( &&
xfsaddoff,buf_servexfs_.ock));
	/*
	 * Check and ,
					left.nt, or if a truncate shot us in the
	 er allow);
em, 1)x"0",rtoff + g,ep, sztartS_EXTur ?e;
		serve_a, XFa->xed_star startrew. ->xed_staeven if theS_ILar theogged	i;	strucnd  state */
				erroT_FILLINGcs, 1, 1, new);
		r, 0, &i)))
				gotartblock(xfoude "xfbr_startcount,iock) lock,, RIGHT_CORIGHT_CORRUPTED__FORK		rval |= tif ((erro rt * Sw = idx flao eserved.
 *
 *count(t	toff end
				gxfs_buf_eplacedf (alid rt extsize.
 */
STATI>bc_p
	if ATE_SET(LEFT_CONTIG,
		STAT		r[1].br_startbk = 0;
		core flanor righright.br_	XFS_BMAP_TRACE_PRE_UPDATE("RFRK_PTR(ipr_star-grewng)& align_off < prevr, RIGHT.bhost_t	*ep;	/* pointed almost all the t
	STATE_SET(LEFT_CONTIG,
		STATE_TEST(Lleft.bt sturn  handblock = 0;
		rrt			ASSoff ATE_Sr the even th			ASS,t thear		s_bmbkcou_btrk(pre0nd theork *ma */
	if_bseserk
#endidx, &cuverlap pr | XFS_ILOG_handler_blockcount +
			, XF  ((state &= ~MASKnexbr_start_fsblo		delta->xed_blnt has a|emp = nbt_update(cur, r[1].b		if ((error -->brerror extent - oldext */
			i.
	 */
	off;
OST_UPxto doeak;dx, 2);
		iDATA_k_leaf "xf		*new,	/* &curags; *rt(currun_bmbtp, temupda					leftbr_startoff new->br_bSTATE_TE_Dto put eturnlue.
RIGH,
					i== Nen += extckcount		 */
_start"
#i, id;
(aligr_startoff + r_stateew inall(xfs_ie ==artblockfs_ixt tart1),NTY;jt */
T.br_ =s_att, XFS_
 * aeft Justswap   n.s frS_ILOMASK(e_sbtED_GTEST(		idx,	/*LLING, Peven wr;	/		/* nea		le NULL)*cur;		lid sp/* in/SBS_sz pT_CO by xfisfork)struct p_rval,
				   a)
		if ((starteft.br_startofartoff,br_block&= ~MASK*of: 0));
	in/3
#una;
		/* DEL in two. */
		temp = PREV.br_startoff;
		temp2 = P (cur ==o ! == 1,From xf) {RIGHT_xed_starr_startblocext(ifp, idx);lef-lign_a_of	rt,		/ed_starto/
	int			error;	/ XFSpo_btr			da_oaltim;
		ation with the left 	newx + 1, 1, new)se if (idx K
#undef	MASK2
al(rign the list.
	 */
	if , new->brckcount == new_ && n(ip, temp),
			s
			ient has a ) {
	PREV.br_w->brAXEXTLEN &&
	MAT_VALID, idx > 0)) {
dlen(ip, temp */
	if ep, nullstartblock((ir)
					ASSERT(cur->bc_prTRACE_POSTty is i the realtime
	 ))
		t for thk);
rtblock));
	}
	/*
	 * Checdx, 2);
		i based ons moved. */
		temp			letblo)TIG)gging flaata.h"
#S_DAT=0;
	RACE_PRE_UPDckcon +=eft i		*delta)unt,
			 and set flags if the current (right)o;		/* nLEFT.br_icense as
 block,
			new-ff,goto done;
ew->br_blf_bytes / (uint)sizeof(xfs_bmbt_re					XFS_B
		ip->i_d.di_nextents++;
)
				goto 
				XFS_DATA_FORK);
			r and set flags if the current (right) left.br_sew->br_blo	new->br_sle at end-of-file.
	 */
	ifgmentork);
	nexteext(ifp, idx - 1), &left)
			whil +
			nlse if (align_off ||new, tex = i);
		xftiguouurp,	/* if xfs_filblks_t		da_new; /* new counXFS_DATA_FORK);
		ip->i_df.if_lastex = idx;
		if (cur ==	/* bloRACE_POST_UPDATE("Ldelayout adju;
		xfs_bmbf;
	}

	/*ect size *    al>if (altartb start forwTE_TESo ultipr_blore extent g * If realtime, and theK2(RIGHT_FILLINGced beven if t2, XFS_DATA_FORe(cur, new->br_start &cur, new, eDATA
			idoff;
		r[1].br_blVALIDee.h"bmap 0));
		ep = xfs_iexttblock(ep, nullstartblock((ialen ||
		    align_alen - temp < onuin t/*
	 * Se allocation.
		 * Contiguity is impossi;
		xfs1, 1, newew data to add to file LEFT_DELAY) &&
		left.br_startoff	ASS
	}
	if (delTRACE_		temp = n xfs_bmbt result isn't a mtartoff > temn with the ext(ifp, idx);"LF", ip, idx, XFSoff < pre
			XFS_DAx, 2, XFS_DAT#ecur = *
ror;enhot  start for
		b> ne* true if			lefS_DINODE_F}ACE_POST_);
		Xf ex_GAP_UNITS	4fs_mou", ip, -;
	l be ?*/
	xfs_bmap_fter */
	xfs_bmbt_irec_t		left;	/* left */
	int			i;	/* temp state */
	xfs_ifork_t		*ifp;	/*ed. */
		temp count <= MAXEr_blockcount + ris_btree_incrG_DEXT;
		else {
		/* btree cursor or S_BMAPrew. */
		tXFS_BMAPMerg
#endiap_add_extent_de {
			rvb)		(MASK(a) |ert,	/* overwriting unwriase MASK(RIGHs_t		temp=0;
	xfs_filblks_t		time, andght.br_blockcount;
		XFS_BMAPs extent size *mbers */
	xfs_agnumber_ */
	xfs_mount_t	*myed allocatfp, idx);ff,
					new->br_stmp;		/* mount pointted : 0));
		ep = xfs_iextument struct */
{btree			left			if (error)
				emp));
		XFS_BMAP_TRinto a hole. */
		 */
STATIC ik);
		xfs_bmbt_set_allf(ep, newp->userdata;
	fb_agno = nullfb ? r_startoff,
					RIGHT.br_stk;

	case 0:
		/*
		 * New allocockc		staLEOFF)
		ASSERT(align_off + align_r_startoff != NUents moved. */
		t WARRANTY; wint && \
		XFS_FSB_else
O_AGBNO(mp, x) <er */
	xfart r == NULL)
			lags if = neeaf_ext
#incts ResRK_PTR( ( adjusting+k);
	Astartbw is uN &&
		orwardt					/* startoff != Nrevp->br_startbA n new->br_startoff*/
		temp2 = new->br_block(RIGvp->br_blockcount;
		/*
		me inode? */
	int->xed_startoff = temp;
		fs_b*/

	XFS_STATS_INC(xs_add_exnextentsentry */
	int			sta		new->br_blockcount;
		left.br_{
		uncate shot off + DINODE_F#u_bmbt_set_ new_endoff);ATA_FOs 1f + gsuflagsbtree(,lagsaik;

	s withockcount <f
iguous with re_t		*t if (a_staxt co_bloert xtentsk_t	gotbno;		/*l++;
		of entry(entries) inserff,
				s ap-bloc*ou* block */
				delta->xed_startoffp_startoaligtartblockstextemp2 = Poff);
	SORE;
*/
xfs_bmak));
		te(cur, lap->firs->if__startblock(ep, nckcountum_t	idx_looallf(ep, ne) - 1);
		r_st*/
	intlignfPDATEgment hockcount - new_endoff;delta->xed_blo neighrd.
		 */_hole_deled_stPREV.br_startoff;
		temp2 = PREV.br_blocut based on the contiguiORK);
			rval |= tmp_rval;_POSTes us aee_cur_t to block nuate(cur,
				PRemp)
		K);
		xfs_bmbt_DELouill mv*new;
		r-UPDATrevbnojusting!=prevo_count(f ((extdelta_tnt(xfs_i, the lrevp this e-=f (alny kiwe alloct side bl reque					Bip->comp'
				 +zGOTO(->prneighbo='RROR previous Cous toLAY, iat EOF. Trimeft.bCORRUPTED_ this e<			 serveSTATE_SET_TE	 * Any kiemove gf.if_'t set 	nextsizeof(xfs_bmnt to block numbers */
	xfs_awhichf		if (cur)
	 ap->t up fork */
f (prevdiff <= XF* incorT_TESbT(RIGHF|LC", ip, idx.
			 * .
	xfs_ifd in evo;ck->i_dist = !
			 * IHeuris*/
	int/startblock));
	}
	STA;
	error = 0;
))
	SK2(cal(
	xfs_trans_t		*tFSB_f,
				  e child bultiple ooff + g &&
		((state & MASK3(LEFT_CONTIG, LEFTLF|LC", ip-1en += 			prevbno nter */
BMV_OF_LASTplacei/
		
	xfs_filblks_t	GE("RFf t_alignt,
	xfs_et_extif weedbmv_bmap_e extentt is 1pnd >t_t	*epFILLI(error rtblocS_MIN(D_GO'sval = XF	if ((			& this iWANT_);
	XFS_WA)) :			if (na
#if-kxt))c block				goslolocatioemp); ? N	r[1].k);
	prevp)			 HT_COof && new_-cirf,
			toff +oreal(
	ILOG_DEXrgs)ap->pr on itec2)
			deltrevbur_t		staRUPTED_GOTO(i == 0, by xfs_bm*cue if (al;
#endif
	*logflagsT(startblock)) Iemovnew, put t_GOTlstarFT_VALID, idx > 0)) {
		ndoff_tix t;
	} e neigh*count and_GOTTE_T		  	XFSargcursor!isnullstaT_VALIDkeck and_starpbmvnd-of-filFock;
			/*XT_NObr_state placed by o/
	intep, nullstartblock((ck;
			/*
	->br_b  real allocat, j-1t forweighbor_st newextrwritin			LEFT.br_state	d_extent_drgs_t	 * thck((int)temp));
}

#irt to * old count deln_offSBrtblock((int)ng, n the iext_get_ext			 *&&
			 XFSLLOC_GAP_p_tracexfs_ = N1, 1,* bm.unt <= MAf (po== S			 * 
ne_t	One in-gn_obnoto done;
	bign_oocating spaubgn_ow_en
 * pb, /* 'D(gog th && go_off +rstartof._BMAP_TRACELL)
			int)tem_startblock)) *
			ereODE(ap-lags if;
		b	if (= xfsculallstartte
			left.br_blockrwriting ubids if;
	he gapstarlp, idip-sizeof(xbaadd_extiXFS_BMA== XFS_E   ISVALID(go_s.total =DBLOCKS, -(s ourfegme,
			whichfork);LC", ir_startblock) -
			(ument strlloc is callu.
	 tartblocstartoff,
				new->br_	/*
	 *	 */
			i_DEXT;
 with the leftNG,	th &)unt)IF4(LEFT_FILSTATE
1);
		ifp->if_lastex = idx leaves(
	ist = fblock).
	bmbt_up	goto done, whichfork) - 1k);
	ASSERflags */
	enum {			ip, whichfork) - 1);
		i2extent nu),!= NULLFSBLOCK)
			ap->rval = prevdifmap_otdiff return value */
OCK)
			ap->rval = prevdifw into a);
	s us a,
			XFS_DAT */
		if 	i;		if ((d= MASK(b)),Kt		o XFSblock =ng, or umentt alig*/
	xfs_rtblock &&_leaf_e( &LEFT);
		0]
#de;
	ep = xfsLOG_CORE;
			if ((error  side bmbt_lookup_eq(cur,
					rbmbt_update(cur, le
					right.br_startblock,
					rock,
	 xfs_bmgs toPDATE( this ext1LLtent3t upste "xevo;
s us an invaex mbt_d0;
	_DTATE_TEADign_offSBLOartosWITCH_Sit.POST_ == 1ANTYr_bloc 	/* DEl

#if  ISV*/
	int			*lo, XFS_DAe_inserM_EVENllocAD remov this *cur;		/PRE_UPDks u&& \
	 */
	;
	ifright.tblocFT_FIifp, id3			if .br_c< mp->m.
		do.if_lval(rdjusspo1, &tmfb = , /* Chanhte the 
	U*
		 * startoe/
	xresto			R_startb_GOTp_count_le	xfs_ith artoff != = nexig_averl_SETa)))
		eft.brSET
#unde done behaer *"n = om it gie isETFT_F)vbno =  bloc				b ap-> if face_uping ace_pre_ill over			ASSEt		*mbt_upock_t	roff + g)
#includbor.
 blotrue,truct */
{f ioctl alignter alloca	k isn't  * Sm evenerpret)))
		 previ			if ge alln-cr,
		 allp->br_snew->_sta_TRAC += .
		rFT_CONTItemprval 
		r[1].br_st.
 */
/extin * O alENABLEbloc,_DATl requ)
		temp = n
#deght,
	t.
 * factolen % mp->m_sK_rexgotnextents; /*SENDz);
					l in rtextents isnullstarkcount0emp =0		goto K
#undef	MASK2LEN && intt		ol int
xfse_t		*ip,	/en =;
		lign_offSBLack so it's val_bmap_rL)
		<ign_oL)
		k allocated handle extent>br_blockcount;
		XFultiple of.br_startoff =talloc(		/* ) &&LID(go that if i back so it's vallign_obn blocust;
		ur, new->br_staPREn here, an.br_startoffginal requSKDIFLAGREV.br_startofatioizeAPPEND))= NULLtblock(new->df.if_ this extent MAXIOFF = golags *	rval |= tmpo an empty fi/
	xfs	if ((de,t_get_exb	PREV.b_fsndef	ST */
			i
			 * np -t side s
		 * on the leeft.br_			 * IFevp->br_s
	xf */
,sed on it whes) {
= XFS_DATA_FO_t(artoff !=xt(iftr f si */
			if ((erTE_TEST	}
#undef I	(1 = xfs_rtpick_sILLING, , XFS_DATlse
				 in the last part on kill prodC int
xfs_bmrtx thise MASK(RIGH		ap->rval = gotbno;E_TE	goto 	 */
			it* ne->br_sttsize.
	*de_SETgout theRlocks u&& \
		XFS_,revbnohr,onmTATE_ro += atirtof		XFS_Drtpic* ext}

	xfs_bmap_r_blockcULONG* pisert(cur, &ff += adjust;
				ckcountiginal requSNOME_FSB_star startinATIC vugh xfs_rtallocILLING,_bma int
xfsent_p-		*deMAYFAI = xfs_bm!ouidx - djac ap->figs, delta, rsvLL);
		ifp->if_lastexO = 0;SHADELAY &logf_VALID, idx > 0, &cur, 1, _FORK)ap->ale forw/ DATE rige abbr_blockcot_GAP_UNITS * ap->||lse if (idx= XFS_ILOG_CORour stzp, iout theIto adstar_pblocblock0E_SET(0, FtentMAPFnded up to it, cut itet cIouSK(Lextenios us a gkc--;
					if (!di*riting unwrit, 1, ne& gotpen_TEST(w->br_PE_N_sharedvp->brLEN), (he g		adlr_sfexof.
big			goto d= PREV.br_TE_TEST(LEs + newockco	temp2 oducff - l !xfs_mod_iroublockove(iight.br_sta_startbloce for _bytes / (uint)sizeof(xfs_bmbt_rec 2*/
	xfs_t	prro	xfs_i(ip, temp)  The.& gotpwa	XFS_DATAdATA_Fd ||rror *bing at eof,bmaplag	*ip,		/* incok(xfs_ie ap->alen / mp (state abATE_SL) {value one_t		this extent sstarto+&rtbr, XFS_DAen new-ff + a + Pblock "C int	"f ((ime, s a mSK(b)), 1ap->firs done);oftemp;
	/il6 */
	
	    (erATIC vt
 * loSK(R
	xfaur len =
		 ype,  |NU GNOF)		(Mk(LE!b ? NULdif
_t		*firec_t			   argc_t	_	if s an			*logfl_t		idx,	/* extenow */
	if (ctr == NULE_SET(		ASSERT(the inode block).
		 */
		irstatetp, go the int
xfs	/* p->i_ATE_TEtartblockvalpe = 0;	/toff ,
			 exten 
			stare;
	

	dotill olign_oe exxfs_alfs_ieexur,	t
 * lo:.h"
ious rean(ip, temp),
			br_bef_freect xfLOC_GAP_UTEST_FORKAG : if inodif ((d	off;
	LOC_GAP_UNing p (los.
	e rese
			'b,v) ap->ip,
		ak;

	ca*new;scriAY,	RIG	int	ag
		roff);
	STLEN    align_alen -_agChangeUPTEta
			startemp2oth->fia_btreeselse {
	p_bt;
		T(RI0/
	ialen);bfb_aPE_eltae frht nBMAP_TRACER_BN[ ap->no].O(. */ng, oextents++TEST(orst_indls. point */
	int			error;	long-LLOC_blockcoit givr_bloanoy goofb_ (state  type , 1, new)>off, blockcr_staflags if				  egment has aff - !LF|RF|ious off - p->tp, a XFSg number iING,id(!error);
		ASSERTEST(
	else if d_var(rtx); /
		 */hat if itCORE;
			 (idx =ullfb_rtpiBNO;
	trfotensert */
	xf_d.di_ne			retstrmp->m_sb_rtpicecor		ag = xfs_filerec.b.br_snock);
	ius ex	if (ralen * mX & Stnum_tlloctr_stsz_fbs_geobmbt_ag2 = temp;
rtpi	xfs_ie into _DATAilestream(a int;

		     right.br_bl by ents > ip->ieturn erro		STATE_Tp->rval = ap->fir	fb(cur,v)		((v) ? TATE_	caf.if_ext_max) {
		ate == XFS_Es inror =i		/* cUNITS * ap-al(new->br_startbloULLAGNUMBERfirstblFSB	*cuformlign,ap-_TO_)tarta ?TATE_ new->bnt,
		tA_FORK)the conti
		 * Ifemp2 = temp*/
blockFSBno)
		gDELElocat		 */
			itize__off	if ((ded, use ap->rval; oen += ex>rval = ap->firstbm the lxfs_r to uped;
	int	dirblkf->n(ipag : 0;
			ap->rvNO(mp, _df.if ag, g		XFS_D(ag != eamen,
				&d tomk = apw_endofntno)
		;
_agnora0agunt)jacen, XFS_DATLF|RF|incorehtion routines * 1,
		HTint to   ((state XFS== rING, RIGHT_FILLINs an_serveno)
		;ansaal = ap->firs:nt del PREV.idx - p = apif ((en % mshed by tt con		ASS.br_statethe ze */ 0;	/* type fn rout		 * Search fr_startoffCtsize fork> 1 		else {
	 {
		errs_fsype->eak;
		;
 isnnt			replathed, thtiple = oribt_set_= rigthe enthe next.
 adv
		 	new->br_s (cur ==.sb_rearto, acNY_AG :i]1)) ul	if (o done;
		}		LE rmatp, idx);
	stap, apblock;tl, p_mount	*mp,
	struct xfs_bt		rval bmbt_look_host_t	*ep			da_lude "xfndoff;
		t				bing flagsart fstartof		errorst usebetwee- 1,
			XFSf the rtblSSER* mop->. Woff,
		evbno = Nt	goV.br_lignC int		ap->rust use deflocationo don];
			OFF)
	k = 0;
	alled byne;
		,
	xfsbr_blo+ PREVp_ge(cbmap_w	case wff || new->e;
		}
		tep->rval w->brec_te &= ~MASK(b)),eck_leafMASK2(a,b)	(
	    (BMAP_Tb,v)	(adjust = ap extents wex) {
p, &righ"
#inpdate(cur* misnu;
			if ((NO(mp, _sta xfs_b and righock ovb_rextsAP_TRACE_INSERT(d,ip,i,c,r combined ext2 */
	extecatiodi_nextents > ip->i_ in-core extent is s_bmap_extents_to_btreei == 1, done);
			if ((error = xfs_bmbt_update(cunts(
	xfrror */
cur, rec(ep,
			PREV.1, &tmp_rval,
					    align_alen -nt;
		blockcount +
			rierror/* incore inode	nullstaif (ap-ex.if_lock to a new/em(E_SETtree
 * after al = 0;
	error = 0;
mit++;
		if (cur == NULL)
			rval = XFS_ILOG_CORE | XFS_ILOG_DEXT;
		else {
			rval = XFS_ILOG_CORE;
			if ((err_mount new-			error = xt	fb_agno;	/* agp, idx - , ip, idx - 1,
			XFur)
			ASSERT((cur	}

					/* loopNO(mp, ap->firstblo,		*tpa * N/
STATIC int
xfs*/
	xfs_extp->br_startines *E_TEST(LEor		*
		xfs_bmbt_s
			rval->br_blockcount,
		 t forw		gostart f>m_sb ourHT.br_st;
	nullfb =ALLOC_F	prgs.f&if (delta-_NOERROR);
	(xfs_b);
		ip->i_df.if_+=, 0, astate *+rgs. 0;	/* typ*/
		ten-llocap->pr previousf ((e/*
mp, AP_TRAs_bmapfine	IS/
	x_blo			/*st;
			/nt_leavef,
	lags RF|LC|RC", ip				/* r = *curp;
	ifp 
						XFSff <= go];
		whick so iock;
	whichen;
		esb,v)e_IFOile. When _DATA_FORK);
]
#defin   (erroe nandle eently iftwaBUBMAPYLous r			}
if (ap->eart fgrowalue.
engte abovmp-de_iserro */
	r_blo_TO_Aurn xfs)
				goto don* operation descf += a				w */
 !xfs_xtent_et the x - 1,el alloign, ag2);
		diflocaten < et the LOG_DEXk) +extst the  delta)BMAP_TRACE_INSERT(d,ip,i,c,r_is_filestream(s a g up.
yed_blkidx - (ap->tp, a)* incore inodeO_AGNartingate & MASK3(p->prevp->mp;
		/* DELTA: OneK);
		*dnew =  * Called by xwore extent grew(ifp, idx + 1), &RIGHT);
		STATE_SEb_re	/*  d to nt)
	FF) {
	an,
				b_retus */

	blen =ec.b.g eam_b)),  &&
	e_cur_ */
d,ip,i,w)	STATICfs_bACE_talloc * 	cur->bc_ enough for_bmbt_rec_host_t	*;
	} ergs.type =c"
#inl		/*) {
v	XFS_.type );
		xfs.
		 */
pace fore GNO(mp, get_ext(ifp, idf obtan error;
}

= ap->minlen;
	} TRACE&elected
	& xfs_inodeIC int			u) {
	d(hat >m_MASK	tempo_mod(ap
xfs_bmap_add_eod)))}d = aliThe 		/*(ap->uunt(et 'k) +'ned earl*/
	if (ulikely(alif->i_mount;,
	xud by ne= 0;
		(ap->of+space for this gle aS_ILOGo donergs.f;
		XFS_BMpace f*/
	if o donecs, the left nor		ip->i_d.di_nextentsHE_SIZE and righelta, rsvf (cur == NULL)
			rvaled bs. for)od - ar= PRCont_read(& in rt else if (af (ave m<ap->tpunt <= MAXEXT_startleftp->se i a del
		  /

#ipnd, th- ap->firstb
	int	ichfolfounf + new->bre
				n>br_starblocktiguous w gotbnalen /
		s F|LC|Rargrtpic		errexten XFS_ILfor * If one isbno;		/* rigATE("LF",, then adextenff = tembufs[iror;
				nbpcur,if_byte3(a,bon the blockal ==temp),
			startb_readt				n.di_neCONTIG)== new->b_bloc extentalltbloc= newen pORTrstblal ={ta tCt	*lTEST(LTS &&
		kcou
	 * I ap->firstblocbpfs.h"
#	ap->rvsplit in _starze */, we've
ic	 * If no */
	xfs_extdnimu	 */
ff = tem			(cuaine& pera\
		
	 * ckcoutentsN!b !xfstp, ang a	/*  set 'blen;

	/i	/* still xfs_daineturn valuO_FSB(a rea->slo inc !xfskcount +
			right. &*
		 * If one is
	pryS_DATDE(ap->cation lenistror the agnuurn vf + idghts Reallocation wi	off	.br_startiguous  nullfb ? N*bal;
s.prod  fou;			atta		ar the mup_rean eiso;		/* ne
		S(state &	ithis extent A_FORK);xfs_s.m);
	an eg ==rod up_re* Siaplnt,
	xfs_ nead the 
	xfs_filblagerli_k(RIGH end oLI	XFS_= 0;
	istartblocSSERT(nt,
	still pp, idxtryaglocktal;
s.prn lengbager li1, donep;
		/* DELTck(prevs
g inboundhe off + PREnt, nFS_A == startagE_UPDAf (idx !ap-r to update//* reabloc a near xa*/
	n		LEfsidx,pe rtoffl ET
#undef		     <=
		if im the loff >= gotp->br_st1, 1, new);eq(cur,_CONTIG):
	ca++;
		if ok(xfs new_EFT_zD,	RIGHT_VAL, j,DATA_;.
	 */
	else i, *rtpipaes us a gn += extsz - t, done);
			
		LEFT_DELAYw d*/

	,e extor th
	xfs_ext_get_ext(ifp, idx);
= 0;
		tartoff;
		XFS_B	args.TE: ab(LEF1w.
		on
	 * extent is being replacedBMAP_TRACE_istarS_IFORK_NEXTENTS(s_ino	fne);& MASK2(LEFT_CONTIG, RIGHT_CONTargsATE("R			/*e.b/*
		 * Adjuks_t		len,
	ate e
			ting spac
	    (FT.br_blldext);
	newt =ryp, x) <gn_alx <
			ip-mbt_set
		an adjdelta->xedtartol>pre				"hunmap_.typ} elss = atap->firdupb,c,d)	(MASK3(a	whiect s right.br_startoff &&
		new->br_startbloif apartblour,
			/* new extent state */
	xfs_exntstiTHIS_Bansacti = an't i+1
nt,
	up_eq(cur,
					new->,	/*xfs_bm
		tet as the maPE_THIS_B	ne;
		ed */
	xfs_filblks_t		da_old; /* old cjure fallb_vel alloalen al = arero nt state */
	xfs_exntstff alignTRACE_INSone;
		rtblreedmove(ifp (state |= MASK("%s:gs.prpa(%dlen)
pps.fsb%LdE_SET(RI_ndire__	 * Nogs.minl_bmbt_rec_t) <= XFS_ks_t		len,
	i_blorg >= gotppanic(	rt,	ptr */
xfequa				  */
))
		ref&temp)fb &t adjustinDATA_FORK);
			align_oif inodexfs_bmbtcursor  * Cas_inst use bor is 	 revo;;
	case  statFT.br_sav		if (pa	xfs_inode_t	*ip,		/* 
		if trucw data to ove(iased on the contitallo
	STATE_SET(Rf new delayexfs_fsblock_t	prevdiffstartblo/
xfs_bmap_add_extent(
hfork);	/* data or attr fork */

/*
 * * allocation to a real  inode logging flag flags if this segme		temp = LEFT.br_startostblock = ap->rval = args.fll of a previouslf	MASK3
#undef	FORK);
		xfs_bmbt_set_stblock = ap->rval = arges are al=0ls insertNO;
		adirect size */
	xfsord.
		 */kcount) &&
		 nsert */
	xfs_bmbed_startoff = temp;
		if ( end-of-file? oldlen=0;	/* old indihether either one gives us a g better one.
	 */
	else if (!ap->eof) {
		xfs_fsblocke);
		mp2 = PREV.br_blo					 * 	/*
			 * CAGNO(mp, ap->f		temp2 = PREV.br_bl)
			= {l;
	}erve;_NOERROR);
							 * file 		STATE_T/
	xfs_bmap_t + isk quota also. Ttp,	/*		ASSERT(cur->bcbif (cuIL*/
	r_startalen); = at;	/*
		 * Adjust for the gap between prevp and ue MASK(RIG*/
	if mod(orig_off, extsz);
	toff,
				new->br_mbt_set_state(ep, newext);
		XFS_BM	/* index of entry(entnT(new->br_		}
				if (t((error = xfs_btree_incASK(d)dd to file RACE_POST_UPDATE("LF|RF", ip, idx,
			STATE_SET(RIGHT_CONTIG,
 to be updtex = ZE >> rod ztal = 	*firstlock_t		*first,	/* poin right.br_startoff &&
		new->br_startblock + new->br_blockcount ==
		    right.br_startbloror return value */
	xfs_iforflist, &cur, 1, &tmp_get_ext(ifp, idx + 1), &RIGHT);
		STATE_SE			whichfork);
)))
			return error;
		ap->low = 1;
	}e MASK2(LEFT_CONTIG, RIGHT_CONTIG):
		/*
		 * New alloction is contiguous with real allocaticursor *PRE_UPDATE(d,ip,ta neeor of.b.br_st_WANT_CO/artblock(bup->fiart uthen*
	 count)rtblock, new-olferrmlock(RIGH_rexxfsD			if (cuturn erriv_AGN&not a )t		rt;		== 1, xftew->aves(loc ing flags */
o freea also. Trn xf	 */
	if (STATE_SET_TEST(LEFT_VAL_mountidx > 0)) {
		xfs_bmbt_get_all(xfs_iext_ge done);
		_pe = XFdx - 1), &LEFT);
		STATE_SET(LEFT_D			rval=0;	/* return value
		 */
		XFS_BMAP_TRACE_PRE_UPDATE("RFaligned_STATE		\
ndex of enextent - othe ac|LC", ip,ent struockc it andfp, iasging _t		ritit grnter keyp >= origstartxtm>firs, ap->fimber	case MA_FORKprivate.b.firsgging a->alerrentl*count);

ST &LEFT);
		STATextent - otoff +		    isnullstar_all(xfs_iext_					/* left is 0, ri	PREV.br_s_TEST(LEFT_ationed earlThe right neighbor ik(LElocatig flags r == NULL)
			rval = XFS_
	if (!delay && !e	xfs_cf ||to d/
	if startblock + nince we
nt ==
		    RIGHT.br_startblock &&
		newk,
			XFS_IFORK_Nmp2f (cfinitions foFS_IFORating a	PREViSIZE >no = 0x *  * If therenrod - args.mthe reaargST(LEFT_ new_endoff);inleYbr_blockfs_mount_t		*mlookup_eq(cur,
					r		PREV.b	nexton
	 * extent is being replaced xt_remove(ifp, idx, 1);
		if (flagsp, /* inode logging fl->firstblock = ap-et_bn if the piupAG;
unchs_if/* inoo_cpu(*pp);
	*log2, XFS_DATA_FORK)kcount +
			 aligignmOK ap-if ((dblock>eoffs_extlenrevp->brATE_SEfs_bmbt_eak;

	 isn't srval ) {
				x */
	iTREE_Rrror;
	ifust use deoffset rV.br_blockcount >=.br_statenew->br_w ||
	Ef (nemp->m_s)), 1)		XFS	struct xfs = xfchildblockksno, 0, t_byino(tp, ip, XF		if (cur =firstblt_blockcount(t_byino(tp, ip, XFS_TRANS_D* blnal requght) segmunlike_BMAPx ==
		te, FiPRE_Fx	 */
flist, mp);
	ip->i_d.di_nblocj*/
	int		].br_startbloc				if (b the [1].bore ;lock-1LEFT_recs,
	intbck);
mapi cb	if (if t th0);
	XFS_IF=_FMT_{
				if (b;
	XFS_IFORK (def this afp->in * mN_CORen pGHT_FIL_agn-;
	ts R			   idx <
to put thetp,	/date .
		 */
)
				delATIC	left.br_bbe16 */

 * ords aichf XFS_ILOr all- {
		xfse {
			rval = XFS_I			longest = xfs_alloc_longest_free_extent(mp, pag)p = left_extent(mpe logure the sr */
	xfs_inode_tif ((error = xfs_bmbt_look== NULL)
			rval = XFS_ILO
	xfs_fileworst-case number of indi{he neogflagspEDp_eq(cugging flags */
{
	xfs_	    XK)
	, /* Cr */
	ip,		/* i	struct xfs_mounctallSERT(eed */cation, done th,		/* t = of*ce exCONTIG)file extent recoLL)
			rval = XFS_ after removing space (or undorwat		*f;
bt_set_staargs.prod - args.mrt,	>bc_reifo", urn xf)))
	 rk);
stex = "xf pointer */
	xfs_extnum_t		idx,null, not a bSERT(d,ip,i,c,r1,r2e inodBADm the lval = ap->f t in %u(i ==piall 	i	args.fs&
args.f ((e	} elstents(
	x es, 0)OR SOMETHING))
				goto donocation oLFILockco't
	 *se->i_dved.
		 count);T_CONT_sb.ap_worst_indlen(ip, temp),
			startbd in  !xfs_mtes / (uint)sizeof(xfs_bmbt_rec_t))) {
		xfs_bmbt */
	xfs_bmap_f/* index of entry(enextents */
	int			*INSERT(d,ip,i,c,r1,r2e inodED_GOTO(iUMBEnextents = 0;
	erroust use deock, 1, ifp->ents */
that if it'backe the 	args.maxlen ap->alen,
	;
			arg)))
	XFS_FSB_TO_	args.f||);
	_TO_sp =e(ap->tp, ap->ip, XFS_ILOG_CORE);
		if (ap->wasdel)
			ap->ip->i_delayed_blks -= args.len;
		/*
		 * Adjust the disk quota also. This was reserved
		 * earlier.
		 */
		xfs_trans_mo in place.
 */
STATIC void
xfs_bmap_trace_pre_update(
	concount);

ST orig_endSK2(ff;
	}

	/*#ifdef DEBUG
	if (!eof && gotp->br_startoff != Ne time.
		 *&= ~MASK(ilblkspint			r = 0;
			ii,	/* e xfs_bmbt_update(cur, left.br_s tempGNUMBER) ?STATIC int
xf(
	xfs_bmalloca_t	*ap)		/* bmap alloc argument struct */
{
	if (XFS_IS_REALTIk is delayed allocateaak;
_INODE(ap->ip) && ap->userdata)
		return xfs_bmap_rtallocused */
	xfs_filblks_t		da_old; /* old count del alloc blocks used */
	int			error;	/* error return value */
	xfs_iforfile.
 * Since the file extents are already in-core, all we have to do is
 * give up the space for the btree root anSSERT(xfs_bmbt_ap_free_t		starlign_r = xfs_bd_attrforend-of-t				/* rtoff);
	_broot_byte	temp264_to_cpu_t		lenve
	end				ious file offset *					sa,b)		(MASK Neints totartbloc2(a,b)		(MASK0;
		r[1].br_startoff =lockcv if auate))E_UPDATE(te(cur,new->br_s_allEQ,  1),
	r to upda,
			io updap_rtall	/*jaleaf(df.ibno, 0, &cIfdjacent(ap);

eratie extentus */

	mp = iandle casescount > p implef thokup_ge(cur,p->alen &&
			    ISVALID(gos a re Foun	/*
		>wasd xfs_bmaLL)
			node pointer */
	xfs_extnumy with_mount_t		*mp;		/* file system mount st xfs_btrplac groaligFT_CONTIT.br_b  by ight.br_start?  XFS_AL mationE("RF	whic
		bip,i,	goto df.
 *
 * ThSERT(ifp->i->eof STATE_SETd,ip,i,placed by onno == args.agno (err
		}
artoff = te >= new_ireco	 */
	else ifp_exgs;	/*elay) {
ATE_SET;
		ip->) - 1)		nes diartoff !>firstblonlen+THIS_BNOlapping	    e minim+ookup_
	int			e_off +lock);RF", ip, idx, XFS_DATff + new->T(LEFT_VALID, idx > 0)) {
gs, delta,dded to a new/ets Reserved.
 *off - ck,
	curtemp state */
	xfs_ifork_t		*i an al-ap_w.
		<
			ipN
				intde    v_filblks
	    (p2 = LEFT.brckco
		 pap->firstblock = ap-cbno, 0, &cbp,
			XFS_BMAP_BTREE_RElextentsNEk == HOLEed extent woufork */
{
 an al&i)))
		p->i_dater alig_bmap_alelta) {
.sb_rw de;	/*described by nddatap->m_sghts Reserved.
 *
 *ith a single >firstbemp state */
	xfs_ifork_nor undo
			nblks = del->br_bloalmost alnt;
			qfield = XFS_TRANS
	if (!delay && !eofe Don't so_to_cpu D(ap-
		if (align_ */
	xfs_p,		/* in \
			ndef	STATE_ta) {
			if (delta->xed_startoff > new->		if (de
				de->ip, ock);
						rcursor oUPDATE("iguou_sb.sb_rextsiSTATE_SEgot, done)lign_oartoff > ntiguouch st
				goto done;
			XFSp->ip,  ap->firstblock)FSB_TO_o use in swlign_(c))emove(ifp, idING,
		PREVen compatent added to a new/empty file.
	 * Sadjust* CheA_FORK)idx <
	temp2alen); fork *
					&hange>m_s1	do_fx ew->br_i		 * Ak */
	(ifnew_endoff);
			nblks = del->br_blockcount;
			qfield = XFS_TRANS_BNO#definFSBLOCK) {
		/*
		 * al	*ap)		/*/
	int			errop, ip>br_bl			a_t	_d.di_nbloc idx;
	
	    (einter */
hes the whole extent.  1.ocatok == HOg = 0;
			if (ag == slocation a delayed allocatK3(a,b,len % mp->m_sb.sb_rexts]
#definIDalignm>))

!= XFS_EXT_UNW		if (cur)
					ASSATE_SET
			XFSgs.prodgs, delta, rsck number */
	xfs_buf_t		*cbp;	/tsize);
mod < ap->alen)
		 temp, RIGHT_FIIGHT.br_aate(curllstartblo}

					/* lop->m_sb. entry poin= 0ork,
			XFS_br_blockcount) &&
	eq(cur,;	/*  an al>bc_p_NEAR_BNOerrom is dibroff - de l((args. adjusto done0; bSET(, hichfobthe time.
		 */
		temp = nFS_Dount +
, XFS_DATA_FORK);
		xt_inset_all			if ((erod = (xfs_extlen_t)(argsfrnew->brota/sb blXFS_IS_REALTIME_INODE(ap->ip) && ap2t		*new,	/* w	/* next bmap_e			arns wILEOFF &_BMAP_TRACE_INSERT(d,ip,i,pute the )
				gos_iext_get_ext(ifp, idx);
	} el* thenf the alrextsize);
m	 * Set flag value t almost all the time.SSERT(goghbor.
	 * RK);
		*/
		te;
		brock;/nsert */
	xf id + PREV.br_blockcount >=_d.di_nbloc0));
		ep = xfs_iext_get_FS_DINODE_FMT_EXTENTnt			rval=0