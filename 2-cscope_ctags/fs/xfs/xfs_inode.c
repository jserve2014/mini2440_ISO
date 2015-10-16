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
#include <linux/log2.h>

#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_types.h"
#include "xfs_bit.h"
#include "xfs_log.h"
#include "xfs_inum.h"
#include "xfs_trans.h"
#include "xfs_trans_priv.h"
#include "xfs_sb.h"
#include "xfs_ag.h"
#include "xfs_dir2.h"
#include "xfs_dmapi.h"
#include "xfs_mount.h"
#include "xfs_bmap_btree.h"
#include "xfs_alloc_btree.h"
#include "xfs_ialloc_btree.h"
#include "xfs_dir2_sf.h"
#include "xfs_attr_sf.h"
#include "xfs_dinode.h"
#include "xfs_inode.h"
#include "xfs_buf_item.h"
#include "xfs_inode_item.h"
#include "xfs_btree.h"
#include "xfs_btree_trace.h"
#include "xfs_alloc.h"
#include "xfs_ialloc.h"
#include "xfs_bmap.h"
#include "xfs_rw.h"
#include "xfs_error.h"
#include "xfs_utils.h"
#include "xfs_dir2_trace.h"
#include "xfs_quota.h"
#include "xfs_filestream.h"
#include "xfs_vnodeops.h"

kmem_zone_t *xfs_ifork_zone;
kmem_zone_t *xfs_inode_zone;

/*
 * Used in xfs_itruncate().  This is the maximum number of extents
 * freed from a file in a single transaction.
 */
#define	XFS_ITRUNC_MAX_EXTENTS	2

STATIC int xfs_iflush_int(xfs_inode_t *, xfs_buf_t *);
STATIC int xfs_iformat_local(xfs_inode_t *, xfs_dinode_t *, int, int);
STATIC int xfs_iformat_extents(xfs_inode_t *, xfs_dinode_t *, int);
STATIC int xfs_iformat_btree(xfs_inode_t *, xfs_dinode_t *, int);

#ifdef DEBUG
/*
 * Make sure that the extents in the given memory buffer
 * are valid.
 */
STATIC void
xfs_validate_extents(
	xfs_ifork_t		*ifp,
	int			nrecs,
	xfs_exntfmt_t		fmt)
{
	xfs_bmbt_irec_t		irec;
	xfs_bmbt_rec_host_t	rec;
	int			i;

	for (i = 0; i < nrecs; i++) {
		xfs_bmbt_rec_host_t *ep = xfs_iext_get_ext(ifp, i);
		rec.l0 = get_unaligned(&ep->l0);
		rec.l1 = get_unaligned(&ep->l1);
		xfs_bmbt_get_all(&rec, &irec);
		if (fmt == XFS_EXTFMT_NOSTATE)
			ASSERT(irec.br_state == XFS_EXT_NORM);
	}
}
#else /* DEBUG */
#define xfs_validate_extents(ifp, nrecs, fmt)
#endif /* DEBUG */

/*
 * Check that none of the inode's in the buffer have a next
 * unlinked field of 0.
 */
#if defined(DEBUG)
void
xfs_inobp_check(
	xfs_mount_t	*mp,
	xfs_buf_t	*bp)
{
	int		i;
	int		j;
	xfs_dinode_t	*dip;

	j = mp->m_inode_cluster_size >> mp->m_sb.sb_inodelog;

	for (i = 0; i < j; i++) {
		dip = (xfs_dinode_t *)xfs_buf_offset(bp,
					i * mp->m_sb.sb_inodesize);
		if (!dip->di_next_unlinked)  {
			xfs_fs_cmn_err(CE_ALERT, mp,
				"Detected a bogus zero next_unlinked field in incore inode buffer 0x%p.  About to pop an ASSERT.",
				bp);
			ASSERT(dip->di_next_unlinked);
		}
	}
}
#endif

/*
 * Find the buffer associated with the given inode map
 * We do basic validation checks on the buffer once it has been
 * retrieved from disk.
 */
STATIC int
xfs_imap_to_bp(
	xfs_mount_t	*mp,
	xfs_trans_t	*tp,
	struct xfs_imap	*imap,
	xfs_buf_t	**bpp,
	uint		buf_flags,
	uint		iget_flags)
{
	int		error;
	int		i;
	int		ni;
	xfs_buf_t	*bp;

	error = xfs_trans_read_buf(mp, tp, mp->m_ddev_targp, imap->im_blkno,
				   (int)imap->im_len, buf_flags, &bp);
	if (error) {
		if (error != EAGAIN) {
			cmn_err(CE_WARN,
				"xfs_imap_to_bp: xfs_trans_read_buf()returned "
				"an error %d on %s.  Returning error.",
				error, mp->m_fsname);
		} else {
			ASSERT(buf_flags & XFS_BUF_TRYLOCK);
		}
		return error;
	}

	/*
	 * Validate the magic number and version of every inode in the buffer
	 * (if DEBUG kernel) or the first inode in the buffer, otherwise.
	 */
#ifdef DEBUG
	ni = BBTOB(imap->im_len) >> mp->m_sb.sb_inodelog;
#else	/* usual case */
	ni = 1;
#endif

	for (i = 0; i < ni; i++) {
		int		di_ok;
		xfs_dinode_t	*dip;

		dip = (xfs_dinode_t *)xfs_buf_offset(bp,
					(i << mp->m_sb.sb_inodelog));
		di_ok = be16_to_cpu(dip->di_magic) == XFS_DINODE_MAGIC &&
			    XFS_DINODE_GOOD_VERSION(dip->di_version);
		if (unlikely(XFS_TEST_ERROR(!di_ok, mp,
						XFS_ERRTAG_ITOBP_INOTOBP,
						XFS_RANDOM_ITOBP_INOTOBP))) {
			if (iget_flags & XFS_IGET_BULKSTAT) {
				xfs_trans_brelse(tp, bp);
				return XFS_ERROR(EINVAL);
			}
			XFS_CORRUPTION_ERROR("xfs_imap_to_bp",
						XFS_ERRLEVEL_HIGH, mp, dip);
#ifdef DEBUG
			cmn_err(CE_PANIC,
					"Device %s - bad inode magic/vsn "
					"daddr %lld #%d (magic=%x)",
				XFS_BUFTARG_NAME(mp->m_ddev_targp),
				(unsigned long long)imap->im_blkno, i,
				be16_to_cpu(dip->di_magic));
#endif
			xfs_trans_brelse(tp, bp);
			return XFS_ERROR(EFSCORRUPTED);
		}
	}

	xfs_inobp_check(mp, bp);

	/*
	 * Mark the buffer as an inode buffer now that it looks good
	 */
	XFS_BUF_SET_VTYPE(bp, B_FS_INO);

	*bpp = bp;
	return 0;
}

/*
 * This routine is called to map an inode number within a file
 * system to the buffer containing the on-disk version of the
 * inode.  It returns a pointer to the buffer containing the
 * on-disk inode in the bpp parameter, and in the dip parameter
 * it returns a pointer to the on-disk inode within that buffer.
 *
 * If a non-zero error is returned, then the contents of bpp and
 * dipp are undefined.
 *
 * Use xfs_imap() to determine the size and location of the
 * buffer to read from disk.
 */
int
xfs_inotobp(
	xfs_mount_t	*mp,
	xfs_trans_t	*tp,
	xfs_ino_t	ino,
	xfs_dinode_t	**dipp,
	xfs_buf_t	**bpp,
	int		*offset,
	uint		imap_flags)
{
	struct xfs_imap	imap;
	xfs_buf_t	*bp;
	int		error;

	imap.im_blkno = 0;
	error = xfs_imap(mp, tp, ino, &imap, imap_flags);
	if (error)
		return error;

	error = xfs_imap_to_bp(mp, tp, &imap, &bp, XFS_BUF_LOCK, imap_flags);
	if (error)
		return error;

	*dipp = (xfs_dinode_t *)xfs_buf_offset(bp, imap.im_boffset);
	*bpp = bp;
	*offset = imap.im_boffset;
	return 0;
}


/*
 * This routine is called to map an inode to the buffer containing
 * the on-disk version of the inode.  It returns a pointer to the
 * buffer containing the on-disk inode in the bpp parameter, and in
 * the dip parameter it returns a pointer to the on-disk inode within
 * that buffer.
 *
 * If a non-zero error is returned, then the contents of bpp and
 * dipp are undefined.
 *
 * The inode is expected to already been mapped to its buffer and read
 * in once, thus we can use the mapping information stored in the inode
 * rather than calling xfs_imap().  This allows us to avoid the overhead
 * of looking at the inode btree for small block file systems
 * (see xfs_imap()).
 */
int
xfs_itobp(
	xfs_mount_t	*mp,
	xfs_trans_t	*tp,
	xfs_inode_t	*ip,
	xfs_dinode_t	**dipp,
	xfs_buf_t	**bpp,
	uint		buf_flags)
{
	xfs_buf_t	*bp;
	int		error;

	ASSERT(ip->i_imap.im_blkno != 0);

	error = xfs_imap_to_bp(mp, tp, &ip->i_imap, &bp, buf_flags, 0);
	if (error)
		return error;

	if (!bp) {
		ASSERT(buf_flags & XFS_BUF_TRYLOCK);
		ASSERT(tp == NULL);
		*bpp = NULL;
		return EAGAIN;
	}

	*dipp = (xfs_dinode_t *)xfs_buf_offset(bp, ip->i_imap.im_boffset);
	*bpp = bp;
	return 0;
}

/*
 * Move inode type and inode format specific information from the
 * on-disk inode to the in-core inode.  For fifos, devs, and sockets
 * this means set if_rdev to the proper value.  For files, directories,
 * and symlinks this means to bring in the in-line data or extent
 * pointers.  For a file in B-tree format, only the root is immediately
 * brought in-core.  The rest will be in-lined in if_extents when it
 * is first referenced (see xfs_iread_extents()).
 */
STATIC int
xfs_iformat(
	xfs_inode_t		*ip,
	xfs_dinode_t		*dip)
{
	xfs_attr_shortform_t	*atp;
	int			size;
	int			error;
	xfs_fsize_t             di_size;
	ip->i_df.if_ext_max =
		XFS_IFORK_DSIZE(ip) / (uint)sizeof(xfs_bmbt_rec_t);
	error = 0;

	if (unlikely(be32_to_cpu(dip->di_nextents) +
		     be16_to_cpu(dip->di_anextents) >
		     be64_to_cpu(dip->di_nblocks))) {
		xfs_fs_repair_cmn_err(CE_WARN, ip->i_mount,
			"corrupt dinode %Lu, extent total = %d, nblocks = %Lu.",
			(unsigned long long)ip->i_ino,
			(int)(be32_to_cpu(dip->di_nextents) +
			      be16_to_cpu(dip->di_anextents)),
			(unsigned long long)
				be64_to_cpu(dip->di_nblocks));
		XFS_CORRUPTION_ERROR("xfs_iformat(1)", XFS_ERRLEVEL_LOW,
				     ip->i_mount, dip);
		return XFS_ERROR(EFSCORRUPTED);
	}

	if (unlikely(dip->di_forkoff > ip->i_mount->m_sb.sb_inodesize)) {
		xfs_fs_repair_cmn_err(CE_WARN, ip->i_mount,
			"corrupt dinode %Lu, forkoff = 0x%x.",
			(unsigned long long)ip->i_ino,
			dip->di_forkoff);
		XFS_CORRUPTION_ERROR("xfs_iformat(2)", XFS_ERRLEVEL_LOW,
				     ip->i_mount, dip);
		return XFS_ERROR(EFSCORRUPTED);
	}

	if (unlikely((ip->i_d.di_flags & XFS_DIFLAG_REALTIME) &&
		     !ip->i_mount->m_rtdev_targp)) {
		xfs_fs_repair_cmn_err(CE_WARN, ip->i_mount,
			"corrupt dinode %Lu, has realtime flag set.",
			ip->i_ino);
		XFS_CORRUPTION_ERROR("xfs_iformat(realtime)",
				     XFS_ERRLEVEL_LOW, ip->i_mount, dip);
		return XFS_ERROR(EFSCORRUPTED);
	}

	switch (ip->i_d.di_mode & S_IFMT) {
	case S_IFIFO:
	case S_IFCHR:
	case S_IFBLK:
	case S_IFSOCK:
		if (unlikely(dip->di_format != XFS_DINODE_FMT_DEV)) {
			XFS_CORRUPTION_ERROR("xfs_iformat(3)", XFS_ERRLEVEL_LOW,
					      ip->i_mount, dip);
			return XFS_ERROR(EFSCORRUPTED);
		}
		ip->i_d.di_size = 0;
		ip->i_size = 0;
		ip->i_df.if_u2.if_rdev = xfs_dinode_get_rdev(dip);
		break;

	case S_IFREG:
	case S_IFLNK:
	case S_IFDIR:
		switch (dip->di_format) {
		case XFS_DINODE_FMT_LOCAL:
			/*
			 * no local regular files yet
			 */
			if (unlikely((be16_to_cpu(dip->di_mode) & S_IFMT) == S_IFREG)) {
				xfs_fs_repair_cmn_err(CE_WARN, ip->i_mount,
					"corrupt inode %Lu "
					"(local format for regular file).",
					(unsigned long long) ip->i_ino);
				XFS_CORRUPTION_ERROR("xfs_iformat(4)",
						     XFS_ERRLEVEL_LOW,
						     ip->i_mount, dip);
				return XFS_ERROR(EFSCORRUPTED);
			}

			di_size = be64_to_cpu(dip->di_size);
			if (unlikely(di_size > XFS_DFORK_DSIZE(dip, ip->i_mount))) {
				xfs_fs_repair_cmn_err(CE_WARN, ip->i_mount,
					"corrupt inode %Lu "
					"(bad size %Ld for local inode).",
					(unsigned long long) ip->i_ino,
					(long long) di_size);
				XFS_CORRUPTION_ERROR("xfs_iformat(5)",
						     XFS_ERRLEVEL_LOW,
						     ip->i_mount, dip);
				return XFS_ERROR(EFSCORRUPTED);
			}

			size = (int)di_size;
			error = xfs_iformat_local(ip, dip, XFS_DATA_FORK, size);
			break;
		case XFS_DINODE_FMT_EXTENTS:
			error = xfs_iformat_extents(ip, dip, XFS_DATA_FORK);
			break;
		case XFS_DINODE_FMT_BTREE:
			error = xfs_iformat_btree(ip, dip, XFS_DATA_FORK);
			break;
		default:
			XFS_ERROR_REPORT("xfs_iformat(6)", XFS_ERRLEVEL_LOW,
					 ip->i_mount);
			return XFS_ERROR(EFSCORRUPTED);
		}
		break;

	default:
		XFS_ERROR_REPORT("xfs_iformat(7)", XFS_ERRLEVEL_LOW, ip->i_mount);
		return XFS_ERROR(EFSCORRUPTED);
	}
	if (error) {
		return error;
	}
	if (!XFS_DFORK_Q(dip))
		return 0;
	ASSERT(ip->i_afp == NULL);
	ip->i_afp = kmem_zone_zalloc(xfs_ifork_zone, KM_SLEEP);
	ip->i_afp->if_ext_max =
		XFS_IFORK_ASIZE(ip) / (uint)sizeof(xfs_bmbt_rec_t);
	switch (dip->di_aformat) {
	case XFS_DINODE_FMT_LOCAL:
		atp = (xfs_attr_shortform_t *)XFS_DFORK_APTR(dip);
		size = be16_to_cpu(atp->hdr.totsize);

		if (unlikely(size < sizeof(struct xfs_attr_sf_hdr))) {
			xfs_fs_repair_cmn_err(CE_WARN, ip->i_mount,
				"corrupt inode %Lu "
				"(bad attr fork size %Ld).",
				(unsigned long long) ip->i_ino,
				(long long) size);
			XFS_CORRUPTION_ERROR("xfs_iformat(8)",
					     XFS_ERRLEVEL_LOW,
					     ip->i_mount, dip);
			return XFS_ERROR(EFSCORRUPTED);
		}

		error = xfs_iformat_local(ip, dip, XFS_ATTR_FORK, size);
		break;
	case XFS_DINODE_FMT_EXTENTS:
		error = xfs_iformat_extents(ip, dip, XFS_ATTR_FORK);
		break;
	case XFS_DINODE_FMT_BTREE:
		error = xfs_iformat_btree(ip, dip, XFS_ATTR_FORK);
		break;
	default:
		error = XFS_ERROR(EFSCORRUPTED);
		break;
	}
	if (error) {
		kmem_zone_free(xfs_ifork_zone, ip->i_afp);
		ip->i_afp = NULL;
		xfs_idestroy_fork(ip, XFS_DATA_FORK);
	}
	return error;
}

/*
 * The file is in-lined in the on-disk inode.
 * If it fits into if_inline_data, then copy
 * it there, otherwise allocate a buffer for it
 * and copy the data there.  Either way, set
 * if_data to point at the data.
 * If we allocate a buffer for the data, make
 * sure that its size is a multiple of 4 and
 * record the real size in i_real_bytes.
 */
STATIC int
xfs_iformat_local(
	xfs_inode_t	*ip,
	xfs_dinode_t	*dip,
	int		whichfork,
	int		size)
{
	xfs_ifork_t	*ifp;
	int		real_size;

	/*
	 * If the size is unreasonable, then something
	 * is wrong and we just bail out rather than crash in
	 * kmem_alloc() or memcpy() below.
	 */
	if (unlikely(size > XFS_DFORK_SIZE(dip, ip->i_mount, whichfork))) {
		xfs_fs_repair_cmn_err(CE_WARN, ip->i_mount,
			"corrupt inode %Lu "
			"(bad size %d for local fork, size = %d).",
			(unsigned long long) ip->i_ino, size,
			XFS_DFORK_SIZE(dip, ip->i_mount, whichfork));
		XFS_CORRUPTION_ERROR("xfs_iformat_local", XFS_ERRLEVEL_LOW,
				     ip->i_mount, dip);
		return XFS_ERROR(EFSCORRUPTED);
	}
	ifp = XFS_IFORK_PTR(ip, whichfork);
	real_size = 0;
	if (size == 0)
		ifp->if_u1.if_data = NULL;
	else if (size <= sizeof(ifp->if_u2.if_inline_data))
		ifp->if_u1.if_data = ifp->if_u2.if_inline_data;
	else {
		real_size = roundup(size, 4);
		ifp->if_u1.if_data = kmem_alloc(real_size, KM_SLEEP);
	}
	ifp->if_bytes = size;
	ifp->if_real_bytes = real_size;
	if (size)
		memcpy(ifp->if_u1.if_data, XFS_DFORK_PTR(dip, whichfork), size);
	ifp->if_flags &= ~XFS_IFEXTENTS;
	ifp->if_flags |= XFS_IFINLINE;
	return 0;
}

/*
 * The file consists of a set of extents all
 * of which fit into the on-disk inode.
 * If there are few enough extents to fit into
 * the if_inline_ext, then copy them there.
 * Otherwise allocate a buffer for them and copy
 * them into it.  Either way, set if_extents
 * to point at the extents.
 */
STATIC int
xfs_iformat_extents(
	xfs_inode_t	*ip,
	xfs_dinode_t	*dip,
	int		whichfork)
{
	xfs_bmbt_rec_t	*dp;
	xfs_ifork_t	*ifp;
	int		nex;
	int		size;
	int		i;

	ifp = XFS_IFORK_PTR(ip, whichfork);
	nex = XFS_DFORK_NEXTENTS(dip, whichfork);
	size = nex * (uint)sizeof(xfs_bmbt_rec_t);

	/*
	 * If the number of extents is unreasonable, then something
	 * is wrong and we just bail out rather than crash in
	 * kmem_alloc() or memcpy() below.
	 */
	if (unlikely(size < 0 || size > XFS_DFORK_SIZE(dip, ip->i_mount, whichfork))) {
		xfs_fs_repair_cmn_err(CE_WARN, ip->i_mount,
			"corrupt inode %Lu ((a)extents = %d).",
			(unsigned long long) ip->i_ino, nex);
		XFS_CORRUPTION_ERROR("xfs_iformat_extents(1)", XFS_ERRLEVEL_LOW,
				     ip->i_mount, dip);
		return XFS_ERROR(EFSCORRUPTED);
	}

	ifp->if_real_bytes = 0;
	if (nex == 0)
		ifp->if_u1.if_extents = NULL;
	else if (nex <= XFS_INLINE_EXTS)
		ifp->if_u1.if_extents = ifp->if_u2.if_inline_ext;
	else
		xfs_iext_add(ifp, 0, nex);

	ifp->if_bytes = size;
	if (size) {
		dp = (xfs_bmbt_rec_t *) XFS_DFORK_PTR(dip, whichfork);
		xfs_validate_extents(ifp, nex, XFS_EXTFMT_INODE(ip));
		for (i = 0; i < nex; i++, dp++) {
			xfs_bmbt_rec_host_t *ep = xfs_iext_get_ext(ifp, i);
			ep->l0 = get_unaligned_be64(&dp->l0);
			ep->l1 = get_unaligned_be64(&dp->l1);
		}
		XFS_BMAP_TRACE_EXLIST(ip, nex, whichfork);
		if (whichfork != XFS_DATA_FORK ||
			XFS_EXTFMT_INODE(ip) == XFS_EXTFMT_NOSTATE)
				if (unlikely(xfs_check_nostate_extents(
				    ifp, 0, nex))) {
					XFS_ERROR_REPORT("xfs_iformat_extents(2)",
							 XFS_ERRLEVEL_LOW,
							 ip->i_mount);
					return XFS_ERROR(EFSCORRUPTED);
				}
	}
	ifp->if_flags |= XFS_IFEXTENTS;
	return 0;
}

/*
 * The file has too many extents to fit into
 * the inode, so they are in B-tree format.
 * Allocate a buffer for the root of the B-tree
 * and copy the root into it.  The i_extents
 * field will remain NULL until all of the
 * extents are read in (when they are needed).
 */
STATIC int
xfs_iformat_btree(
	xfs_inode_t		*ip,
	xfs_dinode_t		*dip,
	int			whichfork)
{
	xfs_bmdr_block_t	*dfp;
	xfs_ifork_t		*ifp;
	/* REFERENCED */
	int			nrecs;
	int			size;

	ifp = XFS_IFORK_PTR(ip, whichfork);
	dfp = (xfs_bmdr_block_t *)XFS_DFORK_PTR(dip, whichfork);
	size = XFS_BMAP_BROOT_SPACE(dfp);
	nrecs = be16_to_cpu(dfp->bb_numrecs);

	/*
	 * blow out if -- fork has less extents than can fit in
	 * fork (fork shouldn't be a btree format), root btree
	 * block has more records than can fit into the fork,
	 * or the number of extents is greater than the number of
	 * blocks.
	 */
	if (unlikely(XFS_IFORK_NEXTENTS(ip, whichfork) <= ifp->if_ext_max
	    || XFS_BMDR_SPACE_CALC(nrecs) >
			XFS_DFORK_SIZE(dip, ip->i_mount, whichfork)
	    || XFS_IFORK_NEXTENTS(ip, whichfork) > ip->i_d.di_nblocks)) {
		xfs_fs_repair_cmn_err(CE_WARN, ip->i_mount,
			"corrupt inode %Lu (btree).",
			(unsigned long long) ip->i_ino);
		XFS_ERROR_REPORT("xfs_iformat_btree", XFS_ERRLEVEL_LOW,
				 ip->i_mount);
		return XFS_ERROR(EFSCORRUPTED);
	}

	ifp->if_broot_bytes = size;
	ifp->if_broot = kmem_alloc(size, KM_SLEEP);
	ASSERT(ifp->if_broot != NULL);
	/*
	 * Copy and convert from the on-disk structure
	 * to the in-memory structure.
	 */
	xfs_bmdr_to_bmbt(ip->i_mount, dfp,
			 XFS_DFORK_SIZE(dip, ip->i_mount, whichfork),
			 ifp->if_broot, size);
	ifp->if_flags &= ~XFS_IFEXTENTS;
	ifp->if_flags |= XFS_IFBROOT;

	return 0;
}

STATIC void
xfs_dinode_from_disk(
	xfs_icdinode_t		*to,
	xfs_dinode_t		*from)
{
	to->di_magic = be16_to_cpu(from->di_magic);
	to->di_mode = be16_to_cpu(from->di_mode);
	to->di_version = from ->di_version;
	to->di_format = from->di_format;
	to->di_onlink = be16_to_cpu(from->di_onlink);
	to->di_uid = be32_to_cpu(from->di_uid);
	to->di_gid = be32_to_cpu(from->di_gid);
	to->di_nlink = be32_to_cpu(from->di_nlink);
	to->di_projid = be16_to_cpu(from->di_projid);
	memcpy(to->di_pad, from->di_pad, sizeof(to->di_pad));
	to->di_flushiter = be16_to_cpu(from->di_flushiter);
	to->di_atime.t_sec = be32_to_cpu(from->di_atime.t_sec);
	to->di_atime.t_nsec = be32_to_cpu(from->di_atime.t_nsec);
	to->di_mtime.t_sec = be32_to_cpu(from->di_mtime.t_sec);
	to->di_mtime.t_nsec = be32_to_cpu(from->di_mtime.t_nsec);
	to->di_ctime.t_sec = be32_to_cpu(from->di_ctime.t_sec);
	to->di_ctime.t_nsec = be32_to_cpu(from->di_ctime.t_nsec);
	to->di_size = be64_to_cpu(from->di_size);
	to->di_nblocks = be64_to_cpu(from->di_nblocks);
	to->di_extsize = be32_to_cpu(from->di_extsize);
	to->di_nextents = be32_to_cpu(from->di_nextents);
	to->di_anextents = be16_to_cpu(from->di_anextents);
	to->di_forkoff = from->di_forkoff;
	to->di_aformat	= from->di_aformat;
	to->di_dmevmask	= be32_to_cpu(from->di_dmevmask);
	to->di_dmstate	= be16_to_cpu(from->di_dmstate);
	to->di_flags	= be16_to_cpu(from->di_flags);
	to->di_gen	= be32_to_cpu(from->di_gen);
}

void
xfs_dinode_to_disk(
	xfs_dinode_t		*to,
	xfs_icdinode_t		*from)
{
	to->di_magic = cpu_to_be16(from->di_magic);
	to->di_mode = cpu_to_be16(from->di_mode);
	to->di_version = from ->di_version;
	to->di_format = from->di_format;
	to->di_onlink = cpu_to_be16(from->di_onlink);
	to->di_uid = cpu_to_be32(from->di_uid);
	to->di_gid = cpu_to_be32(from->di_gid);
	to->di_nlink = cpu_to_be32(from->di_nlink);
	to->di_projid = cpu_to_be16(from->di_projid);
	memcpy(to->di_pad, from->di_pad, sizeof(to->di_pad));
	to->di_flushiter = cpu_to_be16(from->di_flushiter);
	to->di_atime.t_sec = cpu_to_be32(from->di_atime.t_sec);
	to->di_atime.t_nsec = cpu_to_be32(from->di_atime.t_nsec);
	to->di_mtime.t_sec = cpu_to_be32(from->di_mtime.t_sec);
	to->di_mtime.t_nsec = cpu_to_be32(from->di_mtime.t_nsec);
	to->di_ctime.t_sec = cpu_to_be32(from->di_ctime.t_sec);
	to->di_ctime.t_nsec = cpu_to_be32(from->di_ctime.t_nsec);
	to->di_size = cpu_to_be64(from->di_size);
	to->di_nblocks = cpu_to_be64(from->di_nblocks);
	to->di_extsize = cpu_to_be32(from->di_extsize);
	to->di_nextents = cpu_to_be32(from->di_nextents);
	to->di_anextents = cpu_to_be16(from->di_anextents);
	to->di_forkoff = from->di_forkoff;
	to->di_aformat = from->di_aformat;
	to->di_dmevmask = cpu_to_be32(from->di_dmevmask);
	to->di_dmstate = cpu_to_be16(from->di_dmstate);
	to->di_flags = cpu_to_be16(from->di_flags);
	to->di_gen = cpu_to_be32(from->di_gen);
}

STATIC uint
_xfs_dic2xflags(
	__uint16_t		di_flags)
{
	uint			flags = 0;

	if (di_flags & XFS_DIFLAG_ANY) {
		if (di_flags & XFS_DIFLAG_REALTIME)
			flags |= XFS_XFLAG_REALTIME;
		if (di_flags & XFS_DIFLAG_PREALLOC)
			flags |= XFS_XFLAG_PREALLOC;
		if (di_flags & XFS_DIFLAG_IMMUTABLE)
			flags |= XFS_XFLAG_IMMUTABLE;
		if (di_flags & XFS_DIFLAG_APPEND)
			flags |= XFS_XFLAG_APPEND;
		if (di_flags & XFS_DIFLAG_SYNC)
			flags |= XFS_XFLAG_SYNC;
		if (di_flags & XFS_DIFLAG_NOATIME)
			flags |= XFS_XFLAG_NOATIME;
		if (di_flags & XFS_DIFLAG_NODUMP)
			flags |= XFS_XFLAG_NODUMP;
		if (di_flags & XFS_DIFLAG_RTINHERIT)
			flags |= XFS_XFLAG_RTINHERIT;
		if (di_flags & XFS_DIFLAG_PROJINHERIT)
			flags |= XFS_XFLAG_PROJINHERIT;
		if (di_flags & XFS_DIFLAG_NOSYMLINKS)
			flags |= XFS_XFLAG_NOSYMLINKS;
		if (di_flags & XFS_DIFLAG_EXTSIZE)
			flags |= XFS_XFLAG_EXTSIZE;
		if (di_flags & XFS_DIFLAG_EXTSZINHERIT)
			flags |= XFS_XFLAG_EXTSZINHERIT;
		if (di_flags & XFS_DIFLAG_NODEFRAG)
			flags |= XFS_XFLAG_NODEFRAG;
		if (di_flags & XFS_DIFLAG_FILESTREAM)
			flags |= XFS_XFLAG_FILESTREAM;
	}

	return flags;
}

uint
xfs_ip2xflags(
	xfs_inode_t		*ip)
{
	xfs_icdinode_t		*dic = &ip->i_d;

	return _xfs_dic2xflags(dic->di_flags) |
				(XFS_IFORK_Q(ip) ? XFS_XFLAG_HASATTR : 0);
}

uint
xfs_dic2xflags(
	xfs_dinode_t		*dip)
{
	return _xfs_dic2xflags(be16_to_cpu(dip->di_flags)) |
				(XFS_DFORK_Q(dip) ? XFS_XFLAG_HASATTR : 0);
}

/*
 * Read the disk inode attributes into the in-core inode structure.
 */
int
xfs_iread(
	xfs_mount_t	*mp,
	xfs_trans_t	*tp,
	xfs_inode_t	*ip,
	xfs_daddr_t	bno,
	uint		iget_flags)
{
	xfs_buf_t	*bp;
	xfs_dinode_t	*dip;
	int		error;

	/*
	 * Fill in the location information in the in-core inode.
	 */
	ip->i_imap.im_blkno = bno;
	error = xfs_imap(mp, tp, ip->i_ino, &ip->i_imap, iget_flags);
	if (error)
		return error;
	ASSERT(bno == 0 || bno == ip->i_imap.im_blkno);

	/*
	 * Get pointers to the on-disk inode and the buffer containing it.
	 */
	error = xfs_imap_to_bp(mp, tp, &ip->i_imap, &bp,
			       XFS_BUF_LOCK, iget_flags);
	if (error)
		return error;
	dip = (xfs_dinode_t *)xfs_buf_offset(bp, ip->i_imap.im_boffset);

	/*
	 * If we got something that isn't an inode it means someone
	 * (nfs or dmi) has a stale handle.
	 */
	if (be16_to_cpu(dip->di_magic) != XFS_DINODE_MAGIC) {
#ifdef DEBUG
		xfs_fs_cmn_err(CE_ALERT, mp, "xfs_iread: "
				"dip->di_magic (0x%x) != "
				"XFS_DINODE_MAGIC (0x%x)",
				be16_to_cpu(dip->di_magic),
				XFS_DINODE_MAGIC);
#endif /* DEBUG */
		error = XFS_ERROR(EINVAL);
		goto out_brelse;
	}

	/*
	 * If the on-disk inode is already linked to a directory
	 * entry, copy all of the inode into the in-core inode.
	 * xfs_iformat() handles copying in the inode format
	 * specific information.
	 * Otherwise, just get the truly permanent information.
	 */
	if (dip->di_mode) {
		xfs_dinode_from_disk(&ip->i_d, dip);
		error = xfs_iformat(ip, dip);
		if (error)  {
#ifdef DEBUG
			xfs_fs_cmn_err(CE_ALERT, mp, "xfs_iread: "
					"xfs_iformat() returned error %d",
					error);
#endif /* DEBUG */
			goto out_brelse;
		}
	} else {
		ip->i_d.di_magic = be16_to_cpu(dip->di_magic);
		ip->i_d.di_version = dip->di_version;
		ip->i_d.di_gen = be32_to_cpu(dip->di_gen);
		ip->i_d.di_flushiter = be16_to_cpu(dip->di_flushiter);
		/*
		 * Make sure to pull in the mode here as well in
		 * case the inode is released without being used.
		 * This ensures that xfs_inactive() will see that
		 * the inode is already free and not try to mess
		 * with the uninitialized part of it.
		 */
		ip->i_d.di_mode = 0;
		/*
		 * Initialize the per-fork minima and maxima for a new
		 * inode here.  xfs_iformat will do it for old inodes.
		 */
		ip->i_df.if_ext_max =
			XFS_IFORK_DSIZE(ip) / (uint)sizeof(xfs_bmbt_rec_t);
	}

	/*
	 * The inode format changed when we moved the link count and
	 * made it 32 bits long.  If this is an old format inode,
	 * convert it in memory to look like a new one.  If it gets
	 * flushed to disk we will convert back before flushing or
	 * logging it.  We zero out the new projid field and the old link
	 * count field.  We'll handle clearing the pad field (the remains
	 * of the old uuid field) when we actually convert the inode to
	 * the new format. We don't change the version number so that we
	 * can distinguish this from a real new format inode.
	 */
	if (ip->i_d.di_version == 1) {
		ip->i_d.di_nlink = ip->i_d.di_onlink;
		ip->i_d.di_onlink = 0;
		ip->i_d.di_projid = 0;
	}

	ip->i_delayed_blks = 0;
	ip->i_size = ip->i_d.di_size;

	/*
	 * Mark the buffer containing the inode as something to keep
	 * around for a while.  This helps to keep recently accessed
	 * meta-data in-core longer.
	 */
	XFS_BUF_SET_REF(bp, XFS_INO_REF);

	/*
	 * Use xfs_trans_brelse() to release the buffer containing the
	 * on-disk inode, because it was acquired with xfs_trans_read_buf()
	 * in xfs_itobp() above.  If tp is NULL, this is just a normal
	 * brelse().  If we're within a transaction, then xfs_trans_brelse()
	 * will only release the buffer if it is not dirty within the
	 * transaction.  It will be OK to release the buffer in this case,
	 * because inodes on disk are never destroyed and we will be
	 * locking the new in-core inode before putting it in the hash
	 * table where other processes can find it.  Thus we don't have
	 * to worry about the inode being changed just because we released
	 * the buffer.
	 */
 out_brelse:
	xfs_trans_brelse(tp, bp);
	return error;
}

/*
 * Read in extents from a btree-format inode.
 * Allocate and fill in if_extents.  Real work is done in xfs_bmap.c.
 */
int
xfs_iread_extents(
	xfs_trans_t	*tp,
	xfs_inode_t	*ip,
	int		whichfork)
{
	int		error;
	xfs_ifork_t	*ifp;
	xfs_extnum_t	nextents;
	size_t		size;

	if (unlikely(XFS_IFORK_FORMAT(ip, whichfork) != XFS_DINODE_FMT_BTREE)) {
		XFS_ERROR_REPORT("xfs_iread_extents", XFS_ERRLEVEL_LOW,
				 ip->i_mount);
		return XFS_ERROR(EFSCORRUPTED);
	}
	nextents = XFS_IFORK_NEXTENTS(ip, whichfork);
	size = nextents * sizeof(xfs_bmbt_rec_t);
	ifp = XFS_IFORK_PTR(ip, whichfork);

	/*
	 * We know that the size is valid (it's checked in iformat_btree)
	 */
	ifp->if_lastex = NULLEXTNUM;
	ifp->if_bytes = ifp->if_real_bytes = 0;
	ifp->if_flags |= XFS_IFEXTENTS;
	xfs_iext_add(ifp, 0, nextents);
	error = xfs_bmap_read_extents(tp, ip, whichfork);
	if (error) {
		xfs_iext_destroy(ifp);
		ifp->if_flags &= ~XFS_IFEXTENTS;
		return error;
	}
	xfs_validate_extents(ifp, nextents, XFS_EXTFMT_INODE(ip));
	return 0;
}

/*
 * Allocate an inode on disk and return a copy of its in-core version.
 * The in-core inode is locked exclusively.  Set mode, nlink, and rdev
 * appropriately within the inode.  The uid and gid for the inode are
 * set according to the contents of the given cred structure.
 *
 * Use xfs_dialloc() to allocate the on-disk inode. If xfs_dialloc()
 * has a free inode available, call xfs_iget()
 * to obtain the in-core version of the allocated inode.  Finally,
 * fill in the inode and log its initial contents.  In this case,
 * ialloc_context would be set to NULL and call_again set to false.
 *
 * If xfs_dialloc() does not have an available inode,
 * it will replenish its supply by doing an allocation. Since we can
 * only do one allocation within a transaction without deadlocks, we
 * must commit the current transaction before returning the inode itself.
 * In this case, therefore, we will set call_again to true and return.
 * The caller should then commit the current transaction, start a new
 * transaction, and call xfs_ialloc() again to actually get the inode.
 *
 * To ensure that some other process does not grab the inode that
 * was allocated during the first call to xfs_ialloc(), this routine
 * also returns the [locked] bp pointing to the head of the freelist
 * as ialloc_context.  The caller should hold this buffer across
 * the commit and pass it back into this routine on the second call.
 *
 * If we are allocating quota inodes, we do not have a parent inode
 * to attach to or associate with (i.e. pip == NULL) because they
 * are not linked into the directory structure - they are attached
 * directly to the superblock - and so have no parent.
 */
int
xfs_ialloc(
	xfs_trans_t	*tp,
	xfs_inode_t	*pip,
	mode_t		mode,
	xfs_nlink_t	nlink,
	xfs_dev_t	rdev,
	cred_t		*cr,
	xfs_prid_t	prid,
	int		okalloc,
	xfs_buf_t	**ialloc_context,
	boolean_t	*call_again,
	xfs_inode_t	**ipp)
{
	xfs_ino_t	ino;
	xfs_inode_t	*ip;
	uint		flags;
	int		error;
	timespec_t	tv;
	int		filestreams = 0;

	/*
	 * Call the space management code to pick
	 * the on-disk inode to be allocated.
	 */
	error = xfs_dialloc(tp, pip ? pip->i_ino : 0, mode, okalloc,
			    ialloc_context, call_again, &ino);
	if (error)
		return error;
	if (*call_again || ino == NULLFSINO) {
		*ipp = NULL;
		return 0;
	}
	ASSERT(*ialloc_context == NULL);

	/*
	 * Get the in-core inode with the lock held exclusively.
	 * This is because we're setting fields here we need
	 * to prevent others from looking at until we're done.
	 */
	error = xfs_trans_iget(tp->t_mountp, tp, ino,
				XFS_IGET_CREATE, XFS_ILOCK_EXCL, &ip);
	if (error)
		return error;
	ASSERT(ip != NULL);

	ip->i_d.di_mode = (__uint16_t)mode;
	ip->i_d.di_onlink = 0;
	ip->i_d.di_nlink = nlink;
	ASSERT(ip->i_d.di_nlink == nlink);
	ip->i_d.di_uid = current_fsuid();
	ip->i_d.di_gid = current_fsgid();
	ip->i_d.di_projid = prid;
	memset(&(ip->i_d.di_pad[0]), 0, sizeof(ip->i_d.di_pad));

	/*
	 * If the superblock version is up to where we support new format
	 * inodes and this is currently an old format inode, then change
	 * the inode version number now.  This way we only do the conversion
	 * here rather than here and in the flush/logging code.
	 */
	if (xfs_sb_version_hasnlink(&tp->t_mountp->m_sb) &&
	    ip->i_d.di_version == 1) {
		ip->i_d.di_version = 2;
		/*
		 * We've already zeroed the old link count, the projid field,
		 * and the pad field.
		 */
	}

	/*
	 * Project ids won't be stored on disk if we are using a version 1 inode.
	 */
	if ((prid != 0) && (ip->i_d.di_version == 1))
		xfs_bump_ino_vers2(tp, ip);

	if (pip && XFS_INHERIT_GID(pip)) {
		ip->i_d.di_gid = pip->i_d.di_gid;
		if ((pip->i_d.di_mode & S_ISGID) && (mode & S_IFMT) == S_IFDIR) {
			ip->i_d.di_mode |= S_ISGID;
		}
	}

	/*
	 * If the group ID of the new file does not match the effective group
	 * ID or one of the supplementary group IDs, the S_ISGID bit is cleared
	 * (and only if the irix_sgid_inherit compatibility variable is set).
	 */
	if ((irix_sgid_inherit) &&
	    (ip->i_d.di_mode & S_ISGID) &&
	    (!in_group_p((gid_t)ip->i_d.di_gid))) {
		ip->i_d.di_mode &= ~S_ISGID;
	}

	ip->i_d.di_size = 0;
	ip->i_size = 0;
	ip->i_d.di_nextents = 0;
	ASSERT(ip->i_d.di_nblocks == 0);

	nanotime(&tv);
	ip->i_d.di_mtime.t_sec = (__int32_t)tv.tv_sec;
	ip->i_d.di_mtime.t_nsec = (__int32_t)tv.tv_nsec;
	ip->i_d.di_atime = ip->i_d.di_mtime;
	ip->i_d.di_ctime = ip->i_d.di_mtime;

	/*
	 * di_gen will have been taken care of in xfs_iread.
	 */
	ip->i_d.di_extsize = 0;
	ip->i_d.di_dmevmask = 0;
	ip->i_d.di_dmstate = 0;
	ip->i_d.di_flags = 0;
	flags = XFS_ILOG_CORE;
	switch (mode & S_IFMT) {
	case S_IFIFO:
	case S_IFCHR:
	case S_IFBLK:
	case S_IFSOCK:
		ip->i_d.di_format = XFS_DINODE_FMT_DEV;
		ip->i_df.if_u2.if_rdev = rdev;
		ip->i_df.if_flags = 0;
		flags |= XFS_ILOG_DEV;
		break;
	case S_IFREG:
		/*
		 * we can't set up filestreams until after the VFS inode
		 * is set up properly.
		 */
		if (pip && xfs_inode_is_filestream(pip))
			filestreams = 1;
		/* fall through */
	case S_IFDIR:
		if (pip && (pip->i_d.di_flags & XFS_DIFLAG_ANY)) {
			uint	di_flags = 0;

			if ((mode & S_IFMT) == S_IFDIR) {
				if (pip->i_d.di_flags & XFS_DIFLAG_RTINHERIT)
					di_flags |= XFS_DIFLAG_RTINHERIT;
				if (pip->i_d.di_flags & XFS_DIFLAG_EXTSZINHERIT) {
					di_flags |= XFS_DIFLAG_EXTSZINHERIT;
					ip->i_d.di_extsize = pip->i_d.di_extsize;
				}
			} else if ((mode & S_IFMT) == S_IFREG) {
				if (pip->i_d.di_flags & XFS_DIFLAG_RTINHERIT)
					di_flags |= XFS_DIFLAG_REALTIME;
				if (pip->i_d.di_flags & XFS_DIFLAG_EXTSZINHERIT) {
					di_flags |= XFS_DIFLAG_EXTSIZE;
					ip->i_d.di_extsize = pip->i_d.di_extsize;
				}
			}
			if ((pip->i_d.di_flags & XFS_DIFLAG_NOATIME) &&
			    xfs_inherit_noatime)
				di_flags |= XFS_DIFLAG_NOATIME;
			if ((pip->i_d.di_flags & XFS_DIFLAG_NODUMP) &&
			    xfs_inherit_nodump)
				di_flags |= XFS_DIFLAG_NODUMP;
			if ((pip->i_d.di_flags & XFS_DIFLAG_SYNC) &&
			    xfs_inherit_sync)
				di_flags |= XFS_DIFLAG_SYNC;
			if ((pip->i_d.di_flags & XFS_DIFLAG_NOSYMLINKS) &&
			    xfs_inherit_nosymlinks)
				di_flags |= XFS_DIFLAG_NOSYMLINKS;
			if (pip->i_d.di_flags & XFS_DIFLAG_PROJINHERIT)
				di_flags |= XFS_DIFLAG_PROJINHERIT;
			if ((pip->i_d.di_flags & XFS_DIFLAG_NODEFRAG) &&
			    xfs_inherit_nodefrag)
				di_flags |= XFS_DIFLAG_NODEFRAG;
			if (pip->i_d.di_flags & XFS_DIFLAG_FILESTREAM)
				di_flags |= XFS_DIFLAG_FILESTREAM;
			ip->i_d.di_flags |= di_flags;
		}
		/* FALLTHROUGH */
	case S_IFLNK:
		ip->i_d.di_format = XFS_DINODE_FMT_EXTENTS;
		ip->i_df.if_flags = XFS_IFEXTENTS;
		ip->i_df.if_bytes = ip->i_df.if_real_bytes = 0;
		ip->i_df.if_u1.if_extents = NULL;
		break;
	default:
		ASSERT(0);
	}
	/*
	 * Attribute fork settings for new inode.
	 */
	ip->i_d.di_aformat = XFS_DINODE_FMT_EXTENTS;
	ip->i_d.di_anextents = 0;

	/*
	 * Log the new values stuffed into the inode.
	 */
	xfs_trans_log_inode(tp, ip, flags);

	/* now that we have an i_mode we can setup inode ops and unlock */
	xfs_setup_inode(ip);

	/* now we have set up the vfs inode we can associate the filestream */
	if (filestreams) {
		error = xfs_filestream_associate(pip, ip);
		if (error < 0)
			return -error;
		if (!error)
			xfs_iflags_set(ip, XFS_IFILESTREAM);
	}

	*ipp = ip;
	return 0;
}

/*
 * Check to make sure that there are no blocks allocated to the
 * file beyond the size of the file.  We don't check this for
 * files with fixed size extents or real time extents, but we
 * at least do it for regular files.
 */
#ifdef DEBUG
void
xfs_isize_check(
	xfs_mount_t	*mp,
	xfs_inode_t	*ip,
	xfs_fsize_t	isize)
{
	xfs_fileoff_t	map_first;
	int		nimaps;
	xfs_bmbt_irec_t	imaps[2];

	if ((ip->i_d.di_mode & S_IFMT) != S_IFREG)
		return;

	if (XFS_IS_REALTIME_INODE(ip))
		return;

	if (ip->i_d.di_flags & XFS_DIFLAG_EXTSIZE)
		return;

	nimaps = 2;
	map_first = XFS_B_TO_FSB(mp, (xfs_ufsize_t)isize);
	/*
	 * The filesystem could be shutting down, so bmapi may return
	 * an error.
	 */
	if (xfs_bmapi(NULL, ip, map_first,
			 (XFS_B_TO_FSB(mp,
				       (xfs_ufsize_t)XFS_MAXIOFFSET(mp)) -
			  map_first),
			 XFS_BMAPI_ENTIRE, NULL, 0, imaps, &nimaps,
			 NULL, NULL))
	    return;
	ASSERT(nimaps == 1);
	ASSERT(imaps[0].br_startblock == HOLESTARTBLOCK);
}
#endif	/* DEBUG */

/*
 * Calculate the last possible buffered byte in a file.  This must
 * include data that was buffered beyond the EOF by the write code.
 * This also needs to deal with overflowing the xfs_fsize_t type
 * which can happen for sizes near the limit.
 *
 * We also need to take into account any blocks beyond the EOF.  It
 * may be the case that they were buffered by a write which failed.
 * In that case the pages will still be in memory, but the inode size
 * will never have been updated.
 */
STATIC xfs_fsize_t
xfs_file_last_byte(
	xfs_inode_t	*ip)
{
	xfs_mount_t	*mp;
	xfs_fsize_t	last_byte;
	xfs_fileoff_t	last_block;
	xfs_fileoff_t	size_last_block;
	int		error;

	ASSERT(xfs_isilocked(ip, XFS_IOLOCK_EXCL|XFS_IOLOCK_SHARED));

	mp = ip->i_mount;
	/*
	 * Only check for blocks beyond the EOF if the extents have
	 * been read in.  This eliminates the need for the inode lock,
	 * and it also saves us from looking when it really isn't
	 * necessary.
	 */
	if (ip->i_df.if_flags & XFS_IFEXTENTS) {
		xfs_ilock(ip, XFS_ILOCK_SHARED);
		error = xfs_bmap_last_offset(NULL, ip, &last_block,
			XFS_DATA_FORK);
		xfs_iunlock(ip, XFS_ILOCK_SHARED);
		if (error) {
			last_block = 0;
		}
	} else {
		last_block = 0;
	}
	size_last_block = XFS_B_TO_FSB(mp, (xfs_ufsize_t)ip->i_size);
	last_block = XFS_FILEOFF_MAX(last_block, size_last_block);

	last_byte = XFS_FSB_TO_B(mp, last_block);
	if (last_byte < 0) {
		return XFS_MAXIOFFSET(mp);
	}
	last_byte += (1 << mp->m_writeio_log);
	if (last_byte < 0) {
		return XFS_MAXIOFFSET(mp);
	}
	return last_byte;
}

#if defined(XFS_RW_TRACE)
STATIC void
xfs_itrunc_trace(
	int		tag,
	xfs_inode_t	*ip,
	int		flag,
	xfs_fsize_t	new_size,
	xfs_off_t	toss_start,
	xfs_off_t	toss_finish)
{
	if (ip->i_rwtrace == NULL) {
		return;
	}

	ktrace_enter(ip->i_rwtrace,
		     (void*)((long)tag),
		     (void*)ip,
		     (void*)(unsigned long)((ip->i_d.di_size >> 32) & 0xffffffff),
		     (void*)(unsigned long)(ip->i_d.di_size & 0xffffffff),
		     (void*)((long)flag),
		     (void*)(unsigned long)((new_size >> 32) & 0xffffffff),
		     (void*)(unsigned long)(new_size & 0xffffffff),
		     (void*)(unsigned long)((toss_start >> 32) & 0xffffffff),
		     (void*)(unsigned long)(toss_start & 0xffffffff),
		     (void*)(unsigned long)((toss_finish >> 32) & 0xffffffff),
		     (void*)(unsigned long)(toss_finish & 0xffffffff),
		     (void*)(unsigned long)current_cpu(),
		     (void*)(unsigned long)current_pid(),
		     (void*)NULL,
		     (void*)NULL,
		     (void*)NULL);
}
#else
#define	xfs_itrunc_trace(tag, ip, flag, new_size, toss_start, toss_finish)
#endif

/*
 * Start the truncation of the file to new_size.  The new size
 * must be smaller than the current size.  This routine will
 * clear the buffer and page caches of file data in the removed
 * range, and xfs_itruncate_finish() will remove the underlying
 * disk blocks.
 *
 * The inode must have its I/O lock locked EXCLUSIVELY, and it
 * must NOT have the inode lock held at all.  This is because we're
 * calling into the buffer/page cache code and we can't hold the
 * inode lock when we do so.
 *
 * We need to wait for any direct I/Os in flight to complete before we
 * proceed with the truncate. This is needed to prevent the extents
 * being read or written by the direct I/Os from being removed while the
 * I/O is in flight as there is no other method of synchronising
 * direct I/O with the truncate operation.  Also, because we hold
 * the IOLOCK in exclusive mode, we prevent new direct I/Os from being
 * started until the truncate completes and drops the lock. Essentially,
 * the xfs_ioend_wait() call forms an I/O barrier that provides strict
 * ordering between direct I/Os and the truncate operation.
 *
 * The flags parameter can have either the value XFS_ITRUNC_DEFINITE
 * or XFS_ITRUNC_MAYBE.  The XFS_ITRUNC_MAYBE value should be used
 * in the case that the caller is locking things out of order and
 * may not be able to call xfs_itruncate_finish() with the inode lock
 * held without dropping the I/O lock.  If the caller must drop the
 * I/O lock before calling xfs_itruncate_finish(), then xfs_itruncate_start()
 * must be called again with all the same restrictions as the initial
 * call.
 */
int
xfs_itruncate_start(
	xfs_inode_t	*ip,
	uint		flags,
	xfs_fsize_t	new_size)
{
	xfs_fsize_t	last_byte;
	xfs_off_t	toss_start;
	xfs_mount_t	*mp;
	int		error = 0;

	ASSERT(xfs_isilocked(ip, XFS_IOLOCK_EXCL));
	ASSERT((new_size == 0) || (new_size <= ip->i_size));
	ASSERT((flags == XFS_ITRUNC_DEFINITE) ||
	       (flags == XFS_ITRUNC_MAYBE));

	mp = ip->i_mount;

	/* wait for the completion of any pending DIOs */
	if (new_size == 0 || new_size < ip->i_size)
		xfs_ioend_wait(ip);

	/*
	 * Call toss_pages or flushinval_pages to get rid of pages
	 * overlapping the region being removed.  We have to use
	 * the less efficient flushinval_pages in the case that the
	 * caller may not be able to finish the truncate without
	 * dropping the inode's I/O lock.  Make sure
	 * to catch any pages brought in by buffers overlapping
	 * the EOF by searching out beyond the isize by our
	 * block size. We round new_size up to a block boundary
	 * so that we don't toss things on the same block as
	 * new_size but before it.
	 *
	 * Before calling toss_page or flushinval_pages, make sure to
	 * call remapf() over the same region if the file is mapped.
	 * This frees up mapped file references to the pages in the
	 * given range and for the flushinval_pages case it ensures
	 * that we get the latest mapped changes flushed out.
	 */
	toss_start = XFS_B_TO_FSB(mp, (xfs_ufsize_t)new_size);
	toss_start = XFS_FSB_TO_B(mp, toss_start);
	if (toss_start < 0) {
		/*
		 * The place to start tossing is beyond our maximum
		 * file size, so there is no way that the data extended
		 * out there.
		 */
		return 0;
	}
	last_byte = xfs_file_last_byte(ip);
	xfs_itrunc_trace(XFS_ITRUNC_START, ip, flags, new_size, toss_start,
			 last_byte);
	if (last_byte > toss_start) {
		if (flags & XFS_ITRUNC_DEFINITE) {
			xfs_tosspages(ip, toss_start,
					-1, FI_REMAPF_LOCKED);
		} else {
			error = xfs_flushinval_pages(ip, toss_start,
					-1, FI_REMAPF_LOCKED);
		}
	}

#ifdef DEBUG
	if (new_size == 0) {
		ASSERT(VN_CACHED(VFS_I(ip)) == 0);
	}
#endif
	return error;
}

/*
 * Shrink the file to the given new_size.  The new size must be smaller than
 * the current size.  This will free up the underlying blocks in the removed
 * range after a call to xfs_itruncate_start() or xfs_atruncate_start().
 *
 * The transaction passed to this routine must have made a permanent log
 * reservation of at least XFS_ITRUNCATE_LOG_RES.  This routine may commit the
 * given transaction and start new ones, so make sure everything involved in
 * the transaction is tidy before calling here.  Some transaction will be
 * returned to the caller to be committed.  The incoming transaction must
 * already include the inode, and both inode locks must be held exclusively.
 * The inode must also be "held" within the transaction.  On return the inode
 * will be "held" within the returned transaction.  This routine does NOT
 * require any disk space to be reserved for it within the transaction.
 *
 * The fork parameter must be either xfs_attr_fork or xfs_data_fork, and it
 * indicates the fork which is to be truncated.  For the attribute fork we only
 * support truncation to size 0.
 *
 * We use the sync parameter to indicate whether or not the first transaction
 * we perform might have to be synchronous.  For the attr fork, it needs to be
 * so if the unlink of the inode is not yet known to be permanent in the log.
 * This keeps us from freeing and reusing the blocks of the attribute fork
 * before the unlink of the inode becomes permanent.
 *
 * For the data fork, we normally have to run synchronously if we're being
 * called out of the inactive path or we're being called out of the create path
 * where we're truncating an existing file.  Either way, the truncate needs to
 * be sync so blocks don't reappear in the file with altered data in case of a
 * crash.  wsync filesystems can run the first case async because anything that
 * shrinks the inode has to run sync so by the time we're called here from
 * inactive, the inode size is permanently set to 0.
 *
 * Calls from the truncate path always need to be sync unless we're in a wsync
 * filesystem and the file has already been unlinked.
 *
 * The caller is responsible for correctly setting the sync parameter.  It gets
 * too hard for us to guess here which path we're being called out of just
 * based on inode state.
 *
 * If we get an error, we must return with the inode locked and linked into the
 * current transaction. This keeps things simple for the higher level code,
 * because it always knows that the inode is locked and held in the transaction
 * that returns to it whether errors occur or not.  We don't mark the inode
 * dirty on error so that transactions can be easily aborted if possible.
 */
int
xfs_itruncate_finish(
	xfs_trans_t	**tp,
	xfs_inode_t	*ip,
	xfs_fsize_t	new_size,
	int		fork,
	int		sync)
{
	xfs_fsblock_t	first_block;
	xfs_fileoff_t	first_unmap_block;
	xfs_fileoff_t	last_block;
	xfs_filblks_t	unmap_len=0;
	xfs_mount_t	*mp;
	xfs_trans_t	*ntp;
	int		done;
	int		committed;
	xfs_bmap_free_t	free_list;
	int		error;

	ASSERT(xfs_isilocked(ip, XFS_ILOCK_EXCL|XFS_IOLOCK_EXCL));
	ASSERT((new_size == 0) || (new_size <= ip->i_size));
	ASSERT(*tp != NULL);
	ASSERT((*tp)->t_flags & XFS_TRANS_PERM_LOG_RES);
	ASSERT(ip->i_transp == *tp);
	ASSERT(ip->i_itemp != NULL);
	ASSERT(ip->i_itemp->ili_flags & XFS_ILI_HOLD);


	ntp = *tp;
	mp = (ntp)->t_mountp;
	ASSERT(! XFS_NOT_DQATTACHED(mp, ip));

	/*
	 * We only support truncating the entire attribute fork.
	 */
	if (fork == XFS_ATTR_FORK) {
		new_size = 0LL;
	}
	first_unmap_block = XFS_B_TO_FSB(mp, (xfs_ufsize_t)new_size);
	xfs_itrunc_trace(XFS_ITRUNC_FINISH1, ip, 0, new_size, 0, 0);
	/*
	 * The first thing we do is set the size to new_size permanently
	 * on disk.  This way we don't have to worry about anyone ever
	 * being able to look at the data being freed even in the face
	 * of a crash.  What we're getting around here is the case where
	 * we free a block, it is allocated to another file, it is written
	 * to, and then we crash.  If the new data gets written to the
	 * file but the log buffers containing the free and reallocation
	 * don't, then we'd end up with garbage in the blocks being freed.
	 * As long as we make the new_size permanent before actually
	 * freeing any blocks it doesn't matter if they get writtten to.
	 *
	 * The callers must signal into us whether or not the size
	 * setting here must be synchronous.  There are a few cases
	 * where it doesn't have to be synchronous.  Those cases
	 * occur if the file is unlinked and we know the unlink is
	 * permanent or if the blocks being truncated are guaranteed
	 * to be beyond the inode eof (regardless of the link count)
	 * and the eof value is permanent.  Both of these cases occur
	 * only on wsync-mounted filesystems.  In those cases, we're
	 * guaranteed that no user will ever see the data in the blocks
	 * that are being truncated so the truncate can run async.
	 * In the free beyond eof case, the file may wind up with
	 * more blocks allocated to it than it needs if we crash
	 * and that won't get fixed until the next time the file
	 * is re-opened and closed but that's ok as that shouldn't
	 * be too many blocks.
	 *
	 * However, we can't just make all wsync xactions run async
	 * because there's one call out of the create path that needs
	 * to run sync where it's truncating an existing file to size
	 * 0 whose size is > 0.
	 *
	 * It's probably possible to come up with a test in this
	 * routine that would correctly distinguish all the above
	 * cases from the values of the function parameters and the
	 * inode state but for sanity's sake, I've decided to let the
	 * layers above just tell us.  It's simpler to correctly figure
	 * out in the layer above exactly under what conditions we
	 * can run async and I think it's easier for others read and
	 * follow the logic in case something has to be changed.
	 * cscope is your friend -- rcc.
	 *
	 * The attribute fork is much simpler.
	 *
	 * For the attribute fork we allow the caller to tell us whether
	 * the unlink of the inode that led to this call is yet permanent
	 * in the on disk log.  If it is not and we will be freeing extents
	 * in this inode then we make the first transaction synchronous
	 * to make sure that the unlink is permanent by the time we free
	 * the blocks.
	 */
	if (fork == XFS_DATA_FORK) {
		if (ip->i_d.di_nextents > 0) {
			/*
			 * If we are not changing the file size then do
			 * not update the on-disk file size - we may be
			 * called from xfs_inactive_free_eofblocks().  If we
			 * update the on-disk file size and then the system
			 * crashes before the contents of the file are
			 * flushed to disk then the files may be full of
			 * holes (ie NULL files bug).
			 */
			if (ip->i_size != new_size) {
				ip->i_d.di_size = new_size;
				ip->i_size = new_size;
				xfs_trans_log_inode(ntp, ip, XFS_ILOG_CORE);
			}
		}
	} else if (sync) {
		ASSERT(!(mp->m_flags & XFS_MOUNT_WSYNC));
		if (ip->i_d.di_anextents > 0)
			xfs_trans_set_sync(ntp);
	}
	ASSERT(fork == XFS_DATA_FORK ||
		(fork == XFS_ATTR_FORK &&
			((sync && !(mp->m_flags & XFS_MOUNT_WSYNC)) ||
			 (sync == 0 && (mp->m_flags & XFS_MOUNT_WSYNC)))));

	/*
	 * Since it is possible for space to become allocated beyond
	 * the end of the file (in a crash where the space is allocated
	 * but the inode size is not yet updated), simply remove any
	 * blocks which show up between the new EOF and the maximum
	 * possible file size.  If the first block to be removed is
	 * beyond the maximum file size (ie it is the same as last_block),
	 * then there is nothing to do.
	 */
	last_block = XFS_B_TO_FSB(mp, (xfs_ufsize_t)XFS_MAXIOFFSET(mp));
	ASSERT(first_unmap_block <= last_block);
	done = 0;
	if (last_block == first_unmap_block) {
		done = 1;
	} else {
		unmap_len = last_block - first_unmap_block + 1;
	}
	while (!done) {
		/*
		 * Free up up to XFS_ITRUNC_MAX_EXTENTS.  xfs_bunmapi()
		 * will tell us whether it freed the entire range or
		 * not.  If this is a synchronous mount (wsync),
		 * then we can tell bunmapi to keep all the
		 * transactions asynchronous since the unlink
		 * transaction that made this inode inactive has
		 * already hit the disk.  There's no danger of
		 * the freed blocks being reused, there being a
		 * crash, and the reused blocks suddenly reappearing
		 * in this file with garbage in them once recovery
		 * runs.
		 */
		xfs_bmap_init(&free_list, &first_block);
		error = xfs_bunmapi(ntp, ip,
				    first_unmap_block, unmap_len,
				    xfs_bmapi_aflag(fork) |
				      (sync ? 0 : XFS_BMAPI_ASYNC),
				    XFS_ITRUNC_MAX_EXTENTS,
				    &first_block, &free_list,
				    NULL, &done);
		if (error) {
			/*
			 * If the bunmapi call encounters an error,
			 * return to the caller where the transaction
			 * can be properly aborted.  We just need to
			 * make sure we're not holding any resources
			 * that we were not when we came in.
			 */
			xfs_bmap_cancel(&free_list);
			return error;
		}

		/*
		 * Duplicate the transaction that has the permanent
		 * reservation and commit the old transaction.
		 */
		error = xfs_bmap_finish(tp, &free_list, &committed);
		ntp = *tp;
		if (committed) {
			/* link the inode into the next xact in the chain */
			xfs_trans_ijoin(ntp, ip,
					XFS_ILOCK_EXCL | XFS_IOLOCK_EXCL);
			xfs_trans_ihold(ntp, ip);
		}

		if (error) {
			/*
			 * If the bmap finish call encounters an error, return
			 * to the caller where the transaction can be properly
			 * aborted.  We just need to make sure we're not
			 * holding any resources that we were not when we came
			 * in.
			 *
			 * Aborting from this point might lose some blocks in
			 * the file system, but oh well.
			 */
			xfs_bmap_cancel(&free_list);
			return error;
		}

		if (committed) {
			/*
			 * Mark the inode dirty so it will be logged and
			 * moved forward in the log as part of every commit.
			 */
			xfs_trans_log_inode(ntp, ip, XFS_ILOG_CORE);
		}

		ntp = xfs_trans_dup(ntp);
		error = xfs_trans_commit(*tp, 0);
		*tp = ntp;

		/* link the inode into the next transaction in the chain */
		xfs_trans_ijoin(ntp, ip, XFS_ILOCK_EXCL | XFS_IOLOCK_EXCL);
		xfs_trans_ihold(ntp, ip);

		if (error)
			return error;
		/*
		 * transaction commit worked ok so we can drop the extra ticket
		 * reference that we gained in xfs_trans_dup()
		 */
		xfs_log_ticket_put(ntp->t_ticket);
		error = xfs_trans_reserve(ntp, 0,
					XFS_ITRUNCATE_LOG_RES(mp), 0,
					XFS_TRANS_PERM_LOG_RES,
					XFS_ITRUNCATE_LOG_COUNT);
		if (error)
			return error;
	}
	/*
	 * Only update the size in the case of the data fork, but
	 * always re-log the inode so that our permanent transaction
	 * can keep on rolling it forward in the log.
	 */
	if (fork == XFS_DATA_FORK) {
		xfs_isize_check(mp, ip, new_size);
		/*
		 * If we are not changing the file size then do
		 * not update the on-disk file size - we may be
		 * called from xfs_inactive_free_eofblocks().  If we
		 * update the on-disk file size and then the system
		 * crashes before the contents of the file are
		 * flushed to disk then the files may be full of
		 * holes (ie NULL files bug).
		 */
		if (ip->i_size != new_size) {
			ip->i_d.di_size = new_size;
			ip->i_size = new_size;
		}
	}
	xfs_trans_log_inode(ntp, ip, XFS_ILOG_CORE);
	ASSERT((new_size != 0) ||
	       (fork == XFS_ATTR_FORK) ||
	       (ip->i_delayed_blks == 0));
	ASSERT((new_size != 0) ||
	       (fork == XFS_ATTR_FORK) ||
	       (ip->i_d.di_nextents == 0));
	xfs_itrunc_trace(XFS_ITRUNC_FINISH2, ip, 0, new_size, 0, 0);
	return 0;
}

/*
 * This is called when the inode's link count goes to 0.
 * We place the on-disk inode on a list in the AGI.  It
 * will be pulled from this list when the inode is freed.
 */
int
xfs_iunlink(
	xfs_trans_t	*tp,
	xfs_inode_t	*ip)
{
	xfs_mount_t	*mp;
	xfs_agi_t	*agi;
	xfs_dinode_t	*dip;
	xfs_buf_t	*agibp;
	xfs_buf_t	*ibp;
	xfs_agino_t	agino;
	short		bucket_index;
	int		offset;
	int		error;

	ASSERT(ip->i_d.di_nlink == 0);
	ASSERT(ip->i_d.di_mode != 0);
	ASSERT(ip->i_transp == tp);

	mp = tp->t_mountp;

	/*
	 * Get the agi buffer first.  It ensures lock ordering
	 * on the list.
	 */
	error = xfs_read_agi(mp, tp, XFS_INO_TO_AGNO(mp, ip->i_ino), &agibp);
	if (error)
		return error;
	agi = XFS_BUF_TO_AGI(agibp);

	/*
	 * Get the index into the agi hash table for the
	 * list this inode will go on.
	 */
	agino = XFS_INO_TO_AGINO(mp, ip->i_ino);
	ASSERT(agino != 0);
	bucket_index = agino % XFS_AGI_UNLINKED_BUCKETS;
	ASSERT(agi->agi_unlinked[bucket_index]);
	ASSERT(be32_to_cpu(agi->agi_unlinked[bucket_index]) != agino);

	if (be32_to_cpu(agi->agi_unlinked[bucket_index]) != NULLAGINO) {
		/*
		 * There is already another inode in the bucket we need
		 * to add ourselves to.  Add us at the front of the list.
		 * Here we put the head pointer into our next pointer,
		 * and then we fall through to point the head at us.
		 */
		error = xfs_itobp(mp, tp, ip, &dip, &ibp, XFS_BUF_LOCK);
		if (error)
			return error;

		ASSERT(be32_to_cpu(dip->di_next_unlinked) == NULLAGINO);
		/* both on-disk, don't endian flip twice */
		dip->di_next_unlinked = agi->agi_unlinked[bucket_index];
		offset = ip->i_imap.im_boffset +
			offsetof(xfs_dinode_t, di_next_unlinked);
		xfs_trans_inode_buf(tp, ibp);
		xfs_trans_log_buf(tp, ibp, offset,
				  (offset + sizeof(xfs_agino_t) - 1));
		xfs_inobp_check(mp, ibp);
	}

	/*
	 * Point the bucket head pointer at the inode being inserted.
	 */
	ASSERT(agino != 0);
	agi->agi_unlinked[bucket_index] = cpu_to_be32(agino);
	offset = offsetof(xfs_agi_t, agi_unlinked) +
		(sizeof(xfs_agino_t) * bucket_index);
	xfs_trans_log_buf(tp, agibp, offset,
			  (offset + sizeof(xfs_agino_t) - 1));
	return 0;
}

/*
 * Pull the on-disk inode from the AGI unlinked list.
 */
STATIC int
xfs_iunlink_remove(
	xfs_trans_t	*tp,
	xfs_inode_t	*ip)
{
	xfs_ino_t	next_ino;
	xfs_mount_t	*mp;
	xfs_agi_t	*agi;
	xfs_dinode_t	*dip;
	xfs_buf_t	*agibp;
	xfs_buf_t	*ibp;
	xfs_agnumber_t	agno;
	xfs_agino_t	agino;
	xfs_agino_t	next_agino;
	xfs_buf_t	*last_ibp;
	xfs_dinode_t	*last_dip = NULL;
	short		bucket_index;
	int		offset, last_offset = 0;
	int		error;

	mp = tp->t_mountp;
	agno = XFS_INO_TO_AGNO(mp, ip->i_ino);

	/*
	 * Get the agi buffer first.  It ensures lock ordering
	 * on the list.
	 */
	error = xfs_read_agi(mp, tp, agno, &agibp);
	if (error)
		return error;

	agi = XFS_BUF_TO_AGI(agibp);

	/*
	 * Get the index into the agi hash table for the
	 * list this inode will go on.
	 */
	agino = XFS_INO_TO_AGINO(mp, ip->i_ino);
	ASSERT(agino != 0);
	bucket_index = agino % XFS_AGI_UNLINKED_BUCKETS;
	ASSERT(be32_to_cpu(agi->agi_unlinked[bucket_index]) != NULLAGINO);
	ASSERT(agi->agi_unlinked[bucket_index]);

	if (be32_to_cpu(agi->agi_unlinked[bucket_index]) == agino) {
		/*
		 * We're at the head of the list.  Get the inode's
		 * on-disk buffer to see if there is anyone after us
		 * on the list.  Only modify our next pointer if it
		 * is not already NULLAGINO.  This saves us the overhead
		 * of dealing with the buffer when there is no need to
		 * change it.
		 */
		error = xfs_itobp(mp, tp, ip, &dip, &ibp, XFS_BUF_LOCK);
		if (error) {
			cmn_err(CE_WARN,
				"xfs_iunlink_remove: xfs_itobp()  returned an error %d on %s.  Returning error.",
				error, mp->m_fsname);
			return error;
		}
		next_agino = be32_to_cpu(dip->di_next_unlinked);
		ASSERT(next_agino != 0);
		if (next_agino != NULLAGINO) {
			dip->di_next_unlinked = cpu_to_be32(NULLAGINO);
			offset = ip->i_imap.im_boffset +
				offsetof(xfs_dinode_t, di_next_unlinked);
			xfs_trans_inode_buf(tp, ibp);
			xfs_trans_log_buf(tp, ibp, offset,
					  (offset + sizeof(xfs_agino_t) - 1));
			xfs_inobp_check(mp, ibp);
		} else {
			xfs_trans_brelse(tp, ibp);
		}
		/*
		 * Point the bucket head pointer at the next inode.
		 */
		ASSERT(next_agino != 0);
		ASSERT(next_agino != agino);
		agi->agi_unlinked[bucket_index] = cpu_to_be32(next_agino);
		offset = offsetof(xfs_agi_t, agi_unlinked) +
			(sizeof(xfs_agino_t) * bucket_index);
		xfs_trans_log_buf(tp, agibp, offset,
				  (offset + sizeof(xfs_agino_t) - 1));
	} else {
		/*
		 * We need to search the list for the inode being freed.
		 */
		next_agino = be32_to_cpu(agi->agi_unlinked[bucket_index]);
		last_ibp = NULL;
		while (next_agino != agino) {
			/*
			 * If the last inode wasn't the one pointing to
			 * us, then release its buffer since we're not
			 * going to do anything with it.
			 */
			if (last_ibp != NULL) {
				xfs_trans_brelse(tp, last_ibp);
			}
			next_ino = XFS_AGINO_TO_INO(mp, agno, next_agino);
			error = xfs_inotobp(mp, tp, next_ino, &last_dip,
					    &last_ibp, &last_offset, 0);
			if (error) {
				cmn_err(CE_WARN,
			"xfs_iunlink_remove: xfs_inotobp()  returned an error %d on %s.  Returning error.",
					error, mp->m_fsname);
				return error;
			}
			next_agino = be32_to_cpu(last_dip->di_next_unlinked);
			ASSERT(next_agino != NULLAGINO);
			ASSERT(next_agino != 0);
		}
		/*
		 * Now last_ibp points to the buffer previous to us on
		 * the unlinked list.  Pull us from the list.
		 */
		error = xfs_itobp(mp, tp, ip, &dip, &ibp, XFS_BUF_LOCK);
		if (error) {
			cmn_err(CE_WARN,
				"xfs_iunlink_remove: xfs_itobp()  returned an error %d on %s.  Returning error.",
				error, mp->m_fsname);
			return error;
		}
		next_agino = be32_to_cpu(dip->di_next_unlinked);
		ASSERT(next_agino != 0);
		ASSERT(next_agino != agino);
		if (next_agino != NULLAGINO) {
			dip->di_next_unlinked = cpu_to_be32(NULLAGINO);
			offset = ip->i_imap.im_boffset +
				offsetof(xfs_dinode_t, di_next_unlinked);
			xfs_trans_inode_buf(tp, ibp);
			xfs_trans_log_buf(tp, ibp, offset,
					  (offset + sizeof(xfs_agino_t) - 1));
			xfs_inobp_check(mp, ibp);
		} else {
			xfs_trans_brelse(tp, ibp);
		}
		/*
		 * Point the previous inode on the list to the next inode.
		 */
		last_dip->di_next_unlinked = cpu_to_be32(next_agino);
		ASSERT(next_agino != 0);
		offset = last_offset + offsetof(xfs_dinode_t, di_next_unlinked);
		xfs_trans_inode_buf(tp, last_ibp);
		xfs_trans_log_buf(tp, last_ibp, offset,
				  (offset + sizeof(xfs_agino_t) - 1));
		xfs_inobp_check(mp, last_ibp);
	}
	return 0;
}

STATIC void
xfs_ifree_cluster(
	xfs_inode_t	*free_ip,
	xfs_trans_t	*tp,
	xfs_ino_t	inum)
{
	xfs_mount_t		*mp = free_ip->i_mount;
	int			blks_per_cluster;
	int			nbufs;
	int			ninodes;
	int			i, j, found, pre_flushed;
	xfs_daddr_t		blkno;
	xfs_buf_t		*bp;
	xfs_inode_t		*ip, **ip_found;
	xfs_inode_log_item_t	*iip;
	xfs_log_item_t		*lip;
	xfs_perag_t		*pag = xfs_get_perag(mp, inum);

	if (mp->m_sb.sb_blocksize >= XFS_INODE_CLUSTER_SIZE(mp)) {
		blks_per_cluster = 1;
		ninodes = mp->m_sb.sb_inopblock;
		nbufs = XFS_IALLOC_BLOCKS(mp);
	} else {
		blks_per_cluster = XFS_INODE_CLUSTER_SIZE(mp) /
					mp->m_sb.sb_blocksize;
		ninodes = blks_per_cluster * mp->m_sb.sb_inopblock;
		nbufs = XFS_IALLOC_BLOCKS(mp) / blks_per_cluster;
	}

	ip_found = kmem_alloc(ninodes * sizeof(xfs_inode_t *), KM_NOFS);

	for (j = 0; j < nbufs; j++, inum += ninodes) {
		blkno = XFS_AGB_TO_DADDR(mp, XFS_INO_TO_AGNO(mp, inum),
					 XFS_INO_TO_AGBNO(mp, inum));


		/*
		 * Look for each inode in memory and attempt to lock it,
		 * we can be racing with flush and tail pushing here.
		 * any inode we get the locks on, add to an array of
		 * inode items to process later.
		 *
		 * The get the buffer lock, we could beat a flush
		 * or tail pushing thread to the lock here, in which
		 * case they will go looking for the inode buffer
		 * and fail, we need some other form of interlock
		 * here.
		 */
		found = 0;
		for (i = 0; i < ninodes; i++) {
			read_lock(&pag->pag_ici_lock);
			ip = radix_tree_lookup(&pag->pag_ici_root,
					XFS_INO_TO_AGINO(mp, (inum + i)));

			/* Inode not in memory or we found it already,
			 * nothing to do
			 */
			if (!ip || xfs_iflags_test(ip, XFS_ISTALE)) {
				read_unlock(&pag->pag_ici_lock);
				continue;
			}

			if (xfs_inode_clean(ip)) {
				read_unlock(&pag->pag_ici_lock);
				continue;
			}

			/* If we can get the locks then add it to the
			 * list, otherwise by the time we get the bp lock
			 * below it will already be attached to the
			 * inode buffer.
			 */

			/* This inode will already be locked - by us, lets
			 * keep it that way.
			 */

			if (ip == free_ip) {
				if (xfs_iflock_nowait(ip)) {
					xfs_iflags_set(ip, XFS_ISTALE);
					if (xfs_inode_clean(ip)) {
						xfs_ifunlock(ip);
					} else {
						ip_found[found++] = ip;
					}
				}
				read_unlock(&pag->pag_ici_lock);
				continue;
			}

			if (xfs_ilock_nowait(ip, XFS_ILOCK_EXCL)) {
				if (xfs_iflock_nowait(ip)) {
					xfs_iflags_set(ip, XFS_ISTALE);

					if (xfs_inode_clean(ip)) {
						xfs_ifunlock(ip);
						xfs_iunlock(ip, XFS_ILOCK_EXCL);
					} else {
						ip_found[found++] = ip;
					}
				} else {
					xfs_iunlock(ip, XFS_ILOCK_EXCL);
				}
			}
			read_unlock(&pag->pag_ici_lock);
		}

		bp = xfs_trans_get_buf(tp, mp->m_ddev_targp, blkno, 
					mp->m_bsize * blks_per_cluster,
					XFS_BUF_LOCK);

		pre_flushed = 0;
		lip = XFS_BUF_FSPRIVATE(bp, xfs_log_item_t *);
		while (lip) {
			if (lip->li_type == XFS_LI_INODE) {
				iip = (xfs_inode_log_item_t *)lip;
				ASSERT(iip->ili_logged == 1);
				lip->li_cb = (void(*)(xfs_buf_t*,xfs_log_item_t*)) xfs_istale_done;
				xfs_trans_ail_copy_lsn(mp->m_ail,
							&iip->ili_flush_lsn,
							&iip->ili_item.li_lsn);
				xfs_iflags_set(iip->ili_inode, XFS_ISTALE);
				pre_flushed++;
			}
			lip = lip->li_bio_list;
		}

		for (i = 0; i < found; i++) {
			ip = ip_found[i];
			iip = ip->i_itemp;

			if (!iip) {
				ip->i_update_core = 0;
				xfs_ifunlock(ip);
				xfs_iunlock(ip, XFS_ILOCK_EXCL);
				continue;
			}

			iip->ili_last_fields = iip->ili_format.ilf_fields;
			iip->ili_format.ilf_fields = 0;
			iip->ili_logged = 1;
			xfs_trans_ail_copy_lsn(mp->m_ail, &iip->ili_flush_lsn,
						&iip->ili_item.li_lsn);

			xfs_buf_attach_iodone(bp,
				(void(*)(xfs_buf_t*,xfs_log_item_t*))
				xfs_istale_done, (xfs_log_item_t *)iip);
			if (ip != free_ip) {
				xfs_iunlock(ip, XFS_ILOCK_EXCL);
			}
		}

		if (found || pre_flushed)
			xfs_trans_stale_inode_buf(tp, bp);
		xfs_trans_binval(tp, bp);
	}

	kmem_free(ip_found);
	xfs_put_perag(mp, pag);
}

/*
 * This is called to return an inode to the inode free list.
 * The inode should already be truncated to 0 length and have
 * no pages associated with it.  This routine also assumes that
 * the inode is already a part of the transaction.
 *
 * The on-disk copy of the inode will have been added to the list
 * of unlinked inodes in the AGI. We need to remove the inode from
 * that list atomically with respect to freeing it here.
 */
int
xfs_ifree(
	xfs_trans_t	*tp,
	xfs_inode_t	*ip,
	xfs_bmap_free_t	*flist)
{
	int			error;
	int			delete;
	xfs_ino_t		first_ino;
	xfs_dinode_t    	*dip;
	xfs_buf_t       	*ibp;

	ASSERT(xfs_isilocked(ip, XFS_ILOCK_EXCL));
	ASSERT(ip->i_transp == tp);
	ASSERT(ip->i_d.di_nlink == 0);
	ASSERT(ip->i_d.di_nextents == 0);
	ASSERT(ip->i_d.di_anextents == 0);
	ASSERT((ip->i_d.di_size == 0 && ip->i_size == 0) ||
	       ((ip->i_d.di_mode & S_IFMT) != S_IFREG));
	ASSERT(ip->i_d.di_nblocks == 0);

	/*
	 * Pull the on-disk inode from the AGI unlinked list.
	 */
	error = xfs_iunlink_remove(tp, ip);
	if (error != 0) {
		return error;
	}

	error = xfs_difree(tp, ip->i_ino, flist, &delete, &first_ino);
	if (error != 0) {
		return error;
	}
	ip->i_d.di_mode = 0;		/* mark incore inode as free */
	ip->i_d.di_flags = 0;
	ip->i_d.di_dmevmask = 0;
	ip->i_d.di_forkoff = 0;		/* mark the attr fork not in use */
	ip->i_df.if_ext_max =
		XFS_IFORK_DSIZE(ip) / (uint)sizeof(xfs_bmbt_rec_t);
	ip->i_d.di_format = XFS_DINODE_FMT_EXTENTS;
	ip->i_d.di_aformat = XFS_DINODE_FMT_EXTENTS;
	/*
	 * Bump the generation count so no one will be confused
	 * by reincarnations of this inode.
	 */
	ip->i_d.di_gen++;

	xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);

	error = xfs_itobp(ip->i_mount, tp, ip, &dip, &ibp, XFS_BUF_LOCK);
	if (error)
		return error;

        /*
	* Clear the on-disk di_mode. This is to prevent xfs_bulkstat
	* from picking up this inode when it is reclaimed (its incore state
	* initialzed but not flushed to disk yet). The in-core di_mode is
	* already cleared  and a corresponding transaction logged.
	* The hack here just synchronizes the in-core to on-disk
	* di_mode value in advance before the actual inode sync to disk.
	* This is OK because the inode is already unlinked and would never
	* change its di_mode again for this inode generation.
	* This is a temporary hack that would require a proper fix
	* in the future.
	*/
	dip->di_mode = 0;

	if (delete) {
		xfs_ifree_cluster(ip, tp, first_ino);
	}

	return 0;
}

/*
 * Reallocate the space for if_broot based on the number of records
 * being added or deleted as indicated in rec_diff.  Move the records
 * and pointers in if_broot to fit the new size.  When shrinking this
 * will eliminate holes between the records and pointers created by
 * the caller.  When growing this will create holes to be filled in
 * by the caller.
 *
 * The caller must not request to add more records than would fit in
 * the on-disk inode root.  If the if_broot is currently NULL, then
 * if we adding records one will be allocated.  The caller must also
 * not request that the number of records go below zero, although
 * it can go to zero.
 *
 * ip -- the inode whose if_broot area is changing
 * ext_diff -- the change in the number of records, positive or negative,
 *	 requested for the if_broot array.
 */
void
xfs_iroot_realloc(
	xfs_inode_t		*ip,
	int			rec_diff,
	int			whichfork)
{
	struct xfs_mount	*mp = ip->i_mount;
	int			cur_max;
	xfs_ifork_t		*ifp;
	struct xfs_btree_block	*new_broot;
	int			new_max;
	size_t			new_size;
	char			*np;
	char			*op;

	/*
	 * Handle the degenerate case quietly.
	 */
	if (rec_diff == 0) {
		return;
	}

	ifp = XFS_IFORK_PTR(ip, whichfork);
	if (rec_diff > 0) {
		/*
		 * If there wasn't any memory allocated before, just
		 * allocate it now and get out.
		 */
		if (ifp->if_broot_bytes == 0) {
			new_size = (size_t)XFS_BMAP_BROOT_SPACE_CALC(rec_diff);
			ifp->if_broot = kmem_alloc(new_size, KM_SLEEP);
			ifp->if_broot_bytes = (int)new_size;
			return;
		}

		/*
		 * If there is already an existing if_broot, then we need
		 * to realloc() it and shift the pointers to their new
		 * location.  The records don't change location because
		 * they are kept butted up against the btree block header.
		 */
		cur_max = xfs_bmbt_maxrecs(mp, ifp->if_broot_bytes, 0);
		new_max = cur_max + rec_diff;
		new_size = (size_t)XFS_BMAP_BROOT_SPACE_CALC(new_max);
		ifp->if_broot = kmem_realloc(ifp->if_broot, new_size,
				(size_t)XFS_BMAP_BROOT_SPACE_CALC(cur_max), /* old size */
				KM_SLEEP);
		op = (char *)XFS_BMAP_BROOT_PTR_ADDR(mp, ifp->if_broot, 1,
						     ifp->if_broot_bytes);
		np = (char *)XFS_BMAP_BROOT_PTR_ADDR(mp, ifp->if_broot, 1,
						     (int)new_size);
		ifp->if_broot_bytes = (int)new_size;
		ASSERT(ifp->if_broot_bytes <=
			XFS_IFORK_SIZE(ip, whichfork) + XFS_BROOT_SIZE_ADJ);
		memmove(np, op, cur_max * (uint)sizeof(xfs_dfsbno_t));
		return;
	}

	/*
	 * rec_diff is less than 0.  In this case, we are shrinking the
	 * if_broot buffer.  It must already exist.  If we go to zero
	 * records, just get rid of the root and clear the status bit.
	 */
	ASSERT((ifp->if_broot != NULL) && (ifp->if_broot_bytes > 0));
	cur_max = xfs_bmbt_maxrecs(mp, ifp->if_broot_bytes, 0);
	new_max = cur_max + rec_diff;
	ASSERT(new_max >= 0);
	if (new_max > 0)
		new_size = (size_t)XFS_BMAP_BROOT_SPACE_CALC(new_max);
	else
		new_size = 0;
	if (new_size > 0) {
		new_broot = kmem_alloc(new_size, KM_SLEEP);
		/*
		 * First copy over the btree block header.
		 */
		memcpy(new_broot, ifp->if_broot, XFS_BTREE_LBLOCK_LEN);
	} else {
		new_broot = NULL;
		ifp->if_flags &= ~XFS_IFBROOT;
	}

	/*
	 * Only copy the records and pointers if there are any.
	 */
	if (new_max > 0) {
		/*
		 * First copy the records.
		 */
		op = (char *)XFS_BMBT_REC_ADDR(mp, ifp->if_broot, 1);
		np = (char *)XFS_BMBT_REC_ADDR(mp, new_broot, 1);
		memcpy(np, op, new_max * (uint)sizeof(xfs_bmbt_rec_t));

		/*
		 * Then copy the pointers.
		 */
		op = (char *)XFS_BMAP_BROOT_PTR_ADDR(mp, ifp->if_broot, 1,
						     ifp->if_broot_bytes);
		np = (char *)XFS_BMAP_BROOT_PTR_ADDR(mp, new_broot, 1,
						     (int)new_size);
		memcpy(np, op, new_max * (uint)sizeof(xfs_dfsbno_t));
	}
	kmem_free(ifp->if_broot);
	ifp->if_broot = new_broot;
	ifp->if_broot_bytes = (int)new_size;
	ASSERT(ifp->if_broot_bytes <=
		XFS_IFORK_SIZE(ip, whichfork) + XFS_BROOT_SIZE_ADJ);
	return;
}


/*
 * This is called when the amount of space needed for if_data
 * is increased or decreased.  The change in size is indicated by
 * the number of bytes that need to be added or deleted in the
 * byte_diff parameter.
 *
 * If the amount of space needed has decreased below the size of the
 * inline buffer, then switch to using the inline buffer.  Otherwise,
 * use kmem_realloc() or kmem_alloc() to adjust the size of the buffer
 * to what is needed.
 *
 * ip -- the inode whose if_data area is changing
 * byte_diff -- the change in the number of bytes, positive or negative,
 *	 requested for the if_data array.
 */
void
xfs_idata_realloc(
	xfs_inode_t	*ip,
	int		byte_diff,
	int		whichfork)
{
	xfs_ifork_t	*ifp;
	int		new_size;
	int		real_size;

	if (byte_diff == 0) {
		return;
	}

	ifp = XFS_IFORK_PTR(ip, whichfork);
	new_size = (int)ifp->if_bytes + byte_diff;
	ASSERT(new_size >= 0);

	if (new_size == 0) {
		if (ifp->if_u1.if_data != ifp->if_u2.if_inline_data) {
			kmem_free(ifp->if_u1.if_data);
		}
		ifp->if_u1.if_data = NULL;
		real_size = 0;
	} else if (new_size <= sizeof(ifp->if_u2.if_inline_data)) {
		/*
		 * If the valid extents/data can fit in if_inline_ext/data,
		 * copy them from the malloc'd vector and free it.
		 */
		if (ifp->if_u1.if_data == NULL) {
			ifp->if_u1.if_data = ifp->if_u2.if_inline_data;
		} else if (ifp->if_u1.if_data != ifp->if_u2.if_inline_data) {
			ASSERT(ifp->if_real_bytes != 0);
			memcpy(ifp->if_u2.if_inline_data, ifp->if_u1.if_data,
			      new_size);
			kmem_free(ifp->if_u1.if_data);
			ifp->if_u1.if_data = ifp->if_u2.if_inline_data;
		}
		real_size = 0;
	} else {
		/*
		 * Stuck with malloc/realloc.
		 * For inline data, the underlying buffer must be
		 * a multiple of 4 bytes in size so that it can be
		 * logged and stay on word boundaries.  We enforce
		 * that here.
		 */
		real_size = roundup(new_size, 4);
		if (ifp->if_u1.if_data == NULL) {
			ASSERT(ifp->if_real_bytes == 0);
			ifp->if_u1.if_data = kmem_alloc(real_size, KM_SLEEP);
		} else if (ifp->if_u1.if_data != ifp->if_u2.if_inline_data) {
			/*
			 * Only do the realloc if the underlying size
			 * is really changing.
			 */
			if (ifp->if_real_bytes != real_size) {
				ifp->if_u1.if_data =
					kmem_realloc(ifp->if_u1.if_data,
							real_size,
							ifp->if_real_bytes,
							KM_SLEEP);
			}
		} else {
			ASSERT(ifp->if_real_bytes == 0);
			ifp->if_u1.if_data = kmem_alloc(real_size, KM_SLEEP);
			memcpy(ifp->if_u1.if_data, ifp->if_u2.if_inline_data,
				ifp->if_bytes);
		}
	}
	ifp->if_real_bytes = real_size;
	ifp->if_bytes = new_size;
	ASSERT(ifp->if_bytes <= XFS_IFORK_SIZE(ip, whichfork));
}

void
xfs_idestroy_fork(
	xfs_inode_t	*ip,
	int		whichfork)
{
	xfs_ifork_t	*ifp;

	ifp = XFS_IFORK_PTR(ip, whichfork);
	if (ifp->if_broot != NULL) {
		kmem_free(ifp->if_broot);
		ifp->if_broot = NULL;
	}

	/*
	 * If the format is local, then we can't have an extents
	 * array so just look for an inline data array.  If we're
	 * not local then we may or may not have an extents list,
	 * so check and free it up if we do.
	 */
	if (XFS_IFORK_FORMAT(ip, whichfork) == XFS_DINODE_FMT_LOCAL) {
		if ((ifp->if_u1.if_data != ifp->if_u2.if_inline_data) &&
		    (ifp->if_u1.if_data != NULL)) {
			ASSERT(ifp->if_real_bytes != 0);
			kmem_free(ifp->if_u1.if_data);
			ifp->if_u1.if_data = NULL;
			ifp->if_real_bytes = 0;
		}
	} else if ((ifp->if_flags & XFS_IFEXTENTS) &&
		   ((ifp->if_flags & XFS_IFEXTIREC) ||
		    ((ifp->if_u1.if_extents != NULL) &&
		     (ifp->if_u1.if_extents != ifp->if_u2.if_inline_ext)))) {
		ASSERT(ifp->if_real_bytes != 0);
		xfs_iext_destroy(ifp);
	}
	ASSERT(ifp->if_u1.if_extents == NULL ||
	       ifp->if_u1.if_extents == ifp->if_u2.if_inline_ext);
	ASSERT(ifp->if_real_bytes == 0);
	if (whichfork == XFS_ATTR_FORK) {
		kmem_zone_free(xfs_ifork_zone, ip->i_afp);
		ip->i_afp = NULL;
	}
}

/*
 * Increment the pin count of the given buffer.
 * This value is protected by ipinlock spinlock in the mount structure.
 */
void
xfs_ipin(
	xfs_inode_t	*ip)
{
	ASSERT(xfs_isilocked(ip, XFS_ILOCK_EXCL));

	atomic_inc(&ip->i_pincount);
}

/*
 * Decrement the pin count of the given inode, and wake up
 * anyone in xfs_iwait_unpin() if the count goes to 0.  The
 * inode must have been previously pinned with a call to xfs_ipin().
 */
void
xfs_iunpin(
	xfs_inode_t	*ip)
{
	ASSERT(atomic_read(&ip->i_pincount) > 0);

	if (atomic_dec_and_test(&ip->i_pincount))
		wake_up(&ip->i_ipin_wait);
}

/*
 * This is called to unpin an inode. It can be directed to wait or to return
 * immediately without waiting for the inode to be unpinned.  The caller must
 * have the inode locked in at least shared mode so that the buffer cannot be
 * subsequently pinned once someone is waiting for it to be unpinned.
 */
STATIC void
__xfs_iunpin_wait(
	xfs_inode_t	*ip,
	int		wait)
{
	xfs_inode_log_item_t	*iip = ip->i_itemp;

	ASSERT(xfs_isilocked(ip, XFS_ILOCK_EXCL|XFS_ILOCK_SHARED));
	if (atomic_read(&ip->i_pincount) == 0)
		return;

	/* Give the log a push to start the unpinning I/O */
	xfs_log_force(ip->i_mount, (iip && iip->ili_last_lsn) ?
				iip->ili_last_lsn : 0, XFS_LOG_FORCE);
	if (wait)
		wait_event(ip->i_ipin_wait, (atomic_read(&ip->i_pincount) == 0));
}

static inline void
xfs_iunpin_wait(
	xfs_inode_t	*ip)
{
	__xfs_iunpin_wait(ip, 1);
}

static inline void
xfs_iunpin_nowait(
	xfs_inode_t	*ip)
{
	__xfs_iunpin_wait(ip, 0);
}


/*
 * xfs_iextents_copy()
 *
 * This is called to copy the REAL extents (as opposed to the delayed
 * allocation extents) from the inode into the given buffer.  It
 * returns the number of bytes copied into the buffer.
 *
 * If there are no delayed allocation extents, then we can just
 * memcpy() the extents into the buffer.  Otherwise, we need to
 * examine each extent in turn and skip those which are delayed.
 */
int
xfs_iextents_copy(
	xfs_inode_t		*ip,
	xfs_bmbt_rec_t		*dp,
	int			whichfork)
{
	int			copied;
	int			i;
	xfs_ifork_t		*ifp;
	int			nrecs;
	xfs_fsblock_t		start_block;

	ifp = XFS_IFORK_PTR(ip, whichfork);
	ASSERT(xfs_isilocked(ip, XFS_ILOCK_EXCL|XFS_ILOCK_SHARED));
	ASSERT(ifp->if_bytes > 0);

	nrecs = ifp->if_bytes / (uint)sizeof(xfs_bmbt_rec_t);
	XFS_BMAP_TRACE_EXLIST(ip, nrecs, whichfork);
	ASSERT(nrecs > 0);

	/*
	 * There are some delayed allocation extents in the
	 * inode, so copy the extents one at a time and skip
	 * the delayed ones.  There must be at least one
	 * non-delayed extent.
	 */
	copied = 0;
	for (i = 0; i < nrecs; i++) {
		xfs_bmbt_rec_host_t *ep = xfs_iext_get_ext(ifp, i);
		start_block = xfs_bmbt_get_startblock(ep);
		if (isnullstartblock(start_block)) {
			/*
			 * It's a delayed allocation extent, so skip it.
			 */
			continue;
		}

		/* Translate to on disk format */
		put_unaligned(cpu_to_be64(ep->l0), &dp->l0);
		put_unaligned(cpu_to_be64(ep->l1), &dp->l1);
		dp++;
		copied++;
	}
	ASSERT(copied != 0);
	xfs_validate_extents(ifp, copied, XFS_EXTFMT_INODE(ip));

	return (copied * (uint)sizeof(xfs_bmbt_rec_t));
}

/*
 * Each of the following cases stores data into the same region
 * of the on-disk inode, so only one of them can be valid at
 * any given time. While it is possible to have conflicting formats
 * and log flags, e.g. having XFS_ILOG_?DATA set when the fork is
 * in EXTENTS format, this can only happen when the fork has
 * changed formats after being modified but before being flushed.
 * In these cases, the format always takes precedence, because the
 * format indicates the current state of the fork.
 */
/*ARGSUSED*/
STATIC void
xfs_iflush_fork(
	xfs_inode_t		*ip,
	xfs_dinode_t		*dip,
	xfs_inode_log_item_t	*iip,
	int			whichfork,
	xfs_buf_t		*bp)
{
	char			*cp;
	xfs_ifork_t		*ifp;
	xfs_mount_t		*mp;
#ifdef XFS_TRANS_DEBUG
	int			first;
#endif
	static const short	brootflag[2] =
		{ XFS_ILOG_DBROOT, XFS_ILOG_ABROOT };
	static const short	dataflag[2] =
		{ XFS_ILOG_DDATA, XFS_ILOG_ADATA };
	static const short	extflag[2] =
		{ XFS_ILOG_DEXT, XFS_ILOG_AEXT };

	if (!iip)
		return;
	ifp = XFS_IFORK_PTR(ip, whichfork);
	/*
	 * This can happen if we gave up in iformat in an error path,
	 * for the attribute fork.
	 */
	if (!ifp) {
		ASSERT(whichfork == XFS_ATTR_FORK);
		return;
	}
	cp = XFS_DFORK_PTR(dip, whichfork);
	mp = ip->i_mount;
	switch (XFS_IFORK_FORMAT(ip, whichfork)) {
	case XFS_DINODE_FMT_LOCAL:
		if ((iip->ili_format.ilf_fields & dataflag[whichfork]) &&
		    (ifp->if_bytes > 0)) {
			ASSERT(ifp->if_u1.if_data != NULL);
			ASSERT(ifp->if_bytes <= XFS_IFORK_SIZE(ip, whichfork));
			memcpy(cp, ifp->if_u1.if_data, ifp->if_bytes);
		}
		break;

	case XFS_DINODE_FMT_EXTENTS:
		ASSERT((ifp->if_flags & XFS_IFEXTENTS) ||
		       !(iip->ili_format.ilf_fields & extflag[whichfork]));
		ASSERT((xfs_iext_get_ext(ifp, 0) != NULL) ||
			(ifp->if_bytes == 0));
		ASSERT((xfs_iext_get_ext(ifp, 0) == NULL) ||
			(ifp->if_bytes > 0));
		if ((iip->ili_format.ilf_fields & extflag[whichfork]) &&
		    (ifp->if_bytes > 0)) {
			ASSERT(XFS_IFORK_NEXTENTS(ip, whichfork) > 0);
			(void)xfs_iextents_copy(ip, (xfs_bmbt_rec_t *)cp,
				whichfork);
		}
		break;

	case XFS_DINODE_FMT_BTREE:
		if ((iip->ili_format.ilf_fields & brootflag[whichfork]) &&
		    (ifp->if_broot_bytes > 0)) {
			ASSERT(ifp->if_broot != NULL);
			ASSERT(ifp->if_broot_bytes <=
			       (XFS_IFORK_SIZE(ip, whichfork) +
				XFS_BROOT_SIZE_ADJ));
			xfs_bmbt_to_bmdr(mp, ifp->if_broot, ifp->if_broot_bytes,
				(xfs_bmdr_block_t *)cp,
				XFS_DFORK_SIZE(dip, mp, whichfork));
		}
		break;

	case XFS_DINODE_FMT_DEV:
		if (iip->ili_format.ilf_fields & XFS_ILOG_DEV) {
			ASSERT(whichfork == XFS_DATA_FORK);
			xfs_dinode_put_rdev(dip, ip->i_df.if_u2.if_rdev);
		}
		break;

	case XFS_DINODE_FMT_UUID:
		if (iip->ili_format.ilf_fields & XFS_ILOG_UUID) {
			ASSERT(whichfork == XFS_DATA_FORK);
			memcpy(XFS_DFORK_DPTR(dip),
			       &ip->i_df.if_u2.if_uuid,
			       sizeof(uuid_t));
		}
		break;

	default:
		ASSERT(0);
		break;
	}
}

STATIC int
xfs_iflush_cluster(
	xfs_inode_t	*ip,
	xfs_buf_t	*bp)
{
	xfs_mount_t		*mp = ip->i_mount;
	xfs_perag_t		*pag = xfs_get_perag(mp, ip->i_ino);
	unsigned long		first_index, mask;
	unsigned long		inodes_per_cluster;
	int			ilist_size;
	xfs_inode_t		**ilist;
	xfs_inode_t		*iq;
	int			nr_found;
	int			clcount = 0;
	int			bufwasdelwri;
	int			i;

	ASSERT(pag->pagi_inodeok);
	ASSERT(pag->pag_ici_init);

	inodes_per_cluster = XFS_INODE_CLUSTER_SIZE(mp) >> mp->m_sb.sb_inodelog;
	ilist_size = inodes_per_cluster * sizeof(xfs_inode_t *);
	ilist = kmem_alloc(ilist_size, KM_MAYFAIL|KM_NOFS);
	if (!ilist)
		return 0;

	mask = ~(((XFS_INODE_CLUSTER_SIZE(mp) >> mp->m_sb.sb_inodelog)) - 1);
	first_index = XFS_INO_TO_AGINO(mp, ip->i_ino) & mask;
	read_lock(&pag->pag_ici_lock);
	/* really need a gang lookup range call here */
	nr_found = radix_tree_gang_lookup(&pag->pag_ici_root, (void**)ilist,
					first_index, inodes_per_cluster);
	if (nr_found == 0)
		goto out_free;

	for (i = 0; i < nr_found; i++) {
		iq = ilist[i];
		if (iq == ip)
			continue;
		/* if the inode lies outside this cluster, we're done. */
		if ((XFS_INO_TO_AGINO(mp, iq->i_ino) & mask) != first_index)
			break;
		/*
		 * Do an un-protected check to see if the inode is dirty and
		 * is a candidate for flushing.  These checks will be repeated
		 * later after the appropriate locks are acquired.
		 */
		if (xfs_inode_clean(iq) && xfs_ipincount(iq) == 0)
			continue;

		/*
		 * Try to get locks.  If any are unavailable or it is pinned,
		 * then this inode cannot be flushed and is skipped.
		 */

		if (!xfs_ilock_nowait(iq, XFS_ILOCK_SHARED))
			continue;
		if (!xfs_iflock_nowait(iq)) {
			xfs_iunlock(iq, XFS_ILOCK_SHARED);
			continue;
		}
		if (xfs_ipincount(iq)) {
			xfs_ifunlock(iq);
			xfs_iunlock(iq, XFS_ILOCK_SHARED);
			continue;
		}

		/*
		 * arriving here means that this inode can be flushed.  First
		 * re-check that it's dirty before flushing.
		 */
		if (!xfs_inode_clean(iq)) {
			int	error;
			error = xfs_iflush_int(iq, bp);
			if (error) {
				xfs_iunlock(iq, XFS_ILOCK_SHARED);
				goto cluster_corrupt_out;
			}
			clcount++;
		} else {
			xfs_ifunlock(iq);
		}
		xfs_iunlock(iq, XFS_ILOCK_SHARED);
	}

	if (clcount) {
		XFS_STATS_INC(xs_icluster_flushcnt);
		XFS_STATS_ADD(xs_icluster_flushinode, clcount);
	}

out_free:
	read_unlock(&pag->pag_ici_lock);
	kmem_free(ilist);
	return 0;


cluster_corrupt_out:
	/*
	 * Corruption detected in the clustering loop.  Invalidate the
	 * inode buffer and shut down the filesystem.
	 */
	read_unlock(&pag->pag_ici_lock);
	/*
	 * Clean up the buffer.  If it was B_DELWRI, just release it --
	 * brelse can handle it with no problems.  If not, shut down the
	 * filesystem before releasing the buffer.
	 */
	bufwasdelwri = XFS_BUF_ISDELAYWRITE(bp);
	if (bufwasdelwri)
		xfs_buf_relse(bp);

	xfs_force_shutdown(mp, SHUTDOWN_CORRUPT_INCORE);

	if (!bufwasdelwri) {
		/*
		 * Just like incore_relse: if we have b_iodone functions,
		 * mark the buffer as an error and call them.  Otherwise
		 * mark it as stale and brelse.
		 */
		if (XFS_BUF_IODONE_FUNC(bp)) {
			XFS_BUF_CLR_BDSTRAT_FUNC(bp);
			XFS_BUF_UNDONE(bp);
			XFS_BUF_STALE(bp);
			XFS_BUF_ERROR(bp,EIO);
			xfs_biodone(bp);
		} else {
			XFS_BUF_STALE(bp);
			xfs_buf_relse(bp);
		}
	}

	/*
	 * Unlocks the flush lock
	 */
	xfs_iflush_abort(iq);
	kmem_free(ilist);
	return XFS_ERROR(EFSCORRUPTED);
}

/*
 * xfs_iflush() will write a modified inode's changes out to the
 * inode's on disk home.  The caller must have the inode lock held
 * in at least shared mode and the inode flush completion must be
 * active as well.  The inode lock will still be held upon return from
 * the call and the caller is free to unlock it.
 * The inode flush will be completed when the inode reaches the disk.
 * The flags indicate how the inode's buffer should be written out.
 */
int
xfs_iflush(
	xfs_inode_t		*ip,
	uint			flags)
{
	xfs_inode_log_item_t	*iip;
	xfs_buf_t		*bp;
	xfs_dinode_t		*dip;
	xfs_mount_t		*mp;
	int			error;
	int			noblock = (flags == XFS_IFLUSH_ASYNC_NOBLOCK);
	enum { INT_DELWRI = (1 << 0), INT_ASYNC = (1 << 1) };

	XFS_STATS_INC(xs_iflush_count);

	ASSERT(xfs_isilocked(ip, XFS_ILOCK_EXCL|XFS_ILOCK_SHARED));
	ASSERT(!completion_done(&ip->i_flush));
	ASSERT(ip->i_d.di_format != XFS_DINODE_FMT_BTREE ||
	       ip->i_d.di_nextents > ip->i_df.if_ext_max);

	iip = ip->i_itemp;
	mp = ip->i_mount;

	/*
	 * If the inode isn't dirty, then just release the inode
	 * flush lock and do nothing.
	 */
	if (xfs_inode_clean(ip)) {
		xfs_ifunlock(ip);
		return 0;
	}

	/*
	 * We can't flush the inode until it is unpinned, so wait for it if we
	 * are allowed to block.  We know noone new can pin it, because we are
	 * holding the inode lock shared and you need to hold it exclusively to
	 * pin the inode.
	 *
	 * If we are not allowed to block, force the log out asynchronously so
	 * that when we come back the inode will be unpinned. If other inodes
	 * in the same cluster are dirty, they will probably write the inode
	 * out for us if they occur after the log force completes.
	 */
	if (noblock && xfs_ipincount(ip)) {
		xfs_iunpin_nowait(ip);
		xfs_ifunlock(ip);
		return EAGAIN;
	}
	xfs_iunpin_wait(ip);

	/*
	 * This may have been unpinned because the filesystem is shutting
	 * down forcibly. If that's the case we must not write this inode
	 * to disk, because the log record didn't make it to disk!
	 */
	if (XFS_FORCED_SHUTDOWN(mp)) {
		ip->i_update_core = 0;
		if (iip)
			iip->ili_format.ilf_fields = 0;
		xfs_ifunlock(ip);
		return XFS_ERROR(EIO);
	}

	/*
	 * Decide how buffer will be flushed out.  This is done before
	 * the call to xfs_iflush_int because this field is zeroed by it.
	 */
	if (iip != NULL && iip->ili_format.ilf_fields != 0) {
		/*
		 * Flush out the inode buffer according to the directions
		 * of the caller.  In the cases where the caller has given
		 * us a choice choose the non-delwri case.  This is because
		 * the inode is in the AIL and we need to get it out soon.
		 */
		switch (flags) {
		case XFS_IFLUSH_SYNC:
		case XFS_IFLUSH_DELWRI_ELSE_SYNC:
			flags = 0;
			break;
		case XFS_IFLUSH_ASYNC_NOBLOCK:
		case XFS_IFLUSH_ASYNC:
		case XFS_IFLUSH_DELWRI_ELSE_ASYNC:
			flags = INT_ASYNC;
			break;
		case XFS_IFLUSH_DELWRI:
			flags = INT_DELWRI;
			break;
		default:
			ASSERT(0);
			flags = 0;
			break;
		}
	} else {
		switch (flags) {
		case XFS_IFLUSH_DELWRI_ELSE_SYNC:
		case XFS_IFLUSH_DELWRI_ELSE_ASYNC:
		case XFS_IFLUSH_DELWRI:
			flags = INT_DELWRI;
			break;
		case XFS_IFLUSH_ASYNC_NOBLOCK:
		case XFS_IFLUSH_ASYNC:
			flags = INT_ASYNC;
			break;
		case XFS_IFLUSH_SYNC:
			flags = 0;
			break;
		default:
			ASSERT(0);
			flags = 0;
			break;
		}
	}

	/*
	 * Get the buffer containing the on-disk inode.
	 */
	error = xfs_itobp(mp, NULL, ip, &dip, &bp,
				noblock ? XFS_BUF_TRYLOCK : XFS_BUF_LOCK);
	if (error || !bp) {
		xfs_ifunlock(ip);
		return error;
	}

	/*
	 * First flush out the inode that xfs_iflush was called with.
	 */
	error = xfs_iflush_int(ip, bp);
	if (error)
		goto corrupt_out;

	/*
	 * If the buffer is pinned then push on the log now so we won't
	 * get stuck waiting in the write for too long.
	 */
	if (XFS_BUF_ISPINNED(bp))
		xfs_log_force(mp, (xfs_lsn_t)0, XFS_LOG_FORCE);

	/*
	 * inode clustering:
	 * see if other inodes can be gathered into this write
	 */
	error = xfs_iflush_cluster(ip, bp);
	if (error)
		goto cluster_corrupt_out;

	if (flags & INT_DELWRI) {
		xfs_bdwrite(mp, bp);
	} else if (flags & INT_ASYNC) {
		error = xfs_bawrite(mp, bp);
	} else {
		error = xfs_bwrite(mp, bp);
	}
	return error;

corrupt_out:
	xfs_buf_relse(bp);
	xfs_force_shutdown(mp, SHUTDOWN_CORRUPT_INCORE);
cluster_corrupt_out:
	/*
	 * Unlocks the flush lock
	 */
	xfs_iflush_abort(ip);
	return XFS_ERROR(EFSCORRUPTED);
}


STATIC int
xfs_iflush_int(
	xfs_inode_t		*ip,
	xfs_buf_t		*bp)
{
	xfs_inode_log_item_t	*iip;
	xfs_dinode_t		*dip;
	xfs_mount_t		*mp;
#ifdef XFS_TRANS_DEBUG
	int			first;
#endif

	ASSERT(xfs_isilocked(ip, XFS_ILOCK_EXCL|XFS_ILOCK_SHARED));
	ASSERT(!completion_done(&ip->i_flush));
	ASSERT(ip->i_d.di_format != XFS_DINODE_FMT_BTREE ||
	       ip->i_d.di_nextents > ip->i_df.if_ext_max);

	iip = ip->i_itemp;
	mp = ip->i_mount;


	/*
	 * If the inode isn't dirty, then just release the inode
	 * flush lock and do nothing.
	 */
	if (xfs_inode_clean(ip)) {
		xfs_ifunlock(ip);
		return 0;
	}

	/* set *dip = inode's place in the buffer */
	dip = (xfs_dinode_t *)xfs_buf_offset(bp, ip->i_imap.im_boffset);

	/*
	 * Clear i_update_core before copying out the data.
	 * This is for coordination with our timestamp updates
	 * that don't hold the inode lock. They will always
	 * update the timestamps BEFORE setting i_update_core,
	 * so if we clear i_update_core after they set it we
	 * are guaranteed to see their updates to the timestamps.
	 * I believe that this depends on strongly ordered memory
	 * semantics, but we have that.  We use the SYNCHRONIZE
	 * macro to make sure that the compiler does not reorder
	 * the i_update_core access below the data copy below.
	 */
	ip->i_update_core = 0;
	SYNCHRONIZE();

	/*
	 * Make sure to get the latest timestamps from the Linux inode.
	 */
	xfs_synchronize_times(ip);

	if (XFS_TEST_ERROR(be16_to_cpu(dip->di_magic) != XFS_DINODE_MAGIC,
			       mp, XFS_ERRTAG_IFLUSH_1, XFS_RANDOM_IFLUSH_1)) {
		xfs_cmn_err(XFS_PTAG_IFLUSH, CE_ALERT, mp,
		    "xfs_iflush: Bad inode %Lu magic number 0x%x, ptr 0x%p",
			ip->i_ino, be16_to_cpu(dip->di_magic), dip);
		goto corrupt_out;
	}
	if (XFS_TEST_ERROR(ip->i_d.di_magic != XFS_DINODE_MAGIC,
				mp, XFS_ERRTAG_IFLUSH_2, XFS_RANDOM_IFLUSH_2)) {
		xfs_cmn_err(XFS_PTAG_IFLUSH, CE_ALERT, mp,
			"xfs_iflush: Bad inode %Lu, ptr 0x%p, magic number 0x%x",
			ip->i_ino, ip, ip->i_d.di_magic);
		goto corrupt_out;
	}
	if ((ip->i_d.di_mode & S_IFMT) == S_IFREG) {
		if (XFS_TEST_ERROR(
		    (ip->i_d.di_format != XFS_DINODE_FMT_EXTENTS) &&
		    (ip->i_d.di_format != XFS_DINODE_FMT_BTREE),
		    mp, XFS_ERRTAG_IFLUSH_3, XFS_RANDOM_IFLUSH_3)) {
			xfs_cmn_err(XFS_PTAG_IFLUSH, CE_ALERT, mp,
				"xfs_iflush: Bad regular inode %Lu, ptr 0x%p",
				ip->i_ino, ip);
			goto corrupt_out;
		}
	} else if ((ip->i_d.di_mode & S_IFMT) == S_IFDIR) {
		if (XFS_TEST_ERROR(
		    (ip->i_d.di_format != XFS_DINODE_FMT_EXTENTS) &&
		    (ip->i_d.di_format != XFS_DINODE_FMT_BTREE) &&
		    (ip->i_d.di_format != XFS_DINODE_FMT_LOCAL),
		    mp, XFS_ERRTAG_IFLUSH_4, XFS_RANDOM_IFLUSH_4)) {
			xfs_cmn_err(XFS_PTAG_IFLUSH, CE_ALERT, mp,
				"xfs_iflush: Bad directory inode %Lu, ptr 0x%p",
				ip->i_ino, ip);
			goto corrupt_out;
		}
	}
	if (XFS_TEST_ERROR(ip->i_d.di_nextents + ip->i_d.di_anextents >
				ip->i_d.di_nblocks, mp, XFS_ERRTAG_IFLUSH_5,
				XFS_RANDOM_IFLUSH_5)) {
		xfs_cmn_err(XFS_PTAG_IFLUSH, CE_ALERT, mp,
			"xfs_iflush: detected corrupt incore inode %Lu, total extents = %d, nblocks = %Ld, ptr 0x%p",
			ip->i_ino,
			ip->i_d.di_nextents + ip->i_d.di_anextents,
			ip->i_d.di_nblocks,
			ip);
		goto corrupt_out;
	}
	if (XFS_TEST_ERROR(ip->i_d.di_forkoff > mp->m_sb.sb_inodesize,
				mp, XFS_ERRTAG_IFLUSH_6, XFS_RANDOM_IFLUSH_6)) {
		xfs_cmn_err(XFS_PTAG_IFLUSH, CE_ALERT, mp,
			"xfs_iflush: bad inode %Lu, forkoff 0x%x, ptr 0x%p",
			ip->i_ino, ip->i_d.di_forkoff, ip);
		goto corrupt_out;
	}
	/*
	 * bump the flush iteration count, used to detect flushes which
	 * postdate a log record during recovery.
	 */

	ip->i_d.di_flushiter++;

	/*
	 * Copy the dirty parts of the inode into the on-disk
	 * inode.  We always copy out the core of the inode,
	 * because if the inode is dirty at all the core must
	 * be.
	 */
	xfs_dinode_to_disk(dip, &ip->i_d);

	/* Wrap, we never let the log put out DI_MAX_FLUSH */
	if (ip->i_d.di_flushiter == DI_MAX_FLUSH)
		ip->i_d.di_flushiter = 0;

	/*
	 * If this is really an old format inode and the superblock version
	 * has not been updated to support only new format inodes, then
	 * convert back to the old inode format.  If the superblock version
	 * has been updated, then make the conversion permanent.
	 */
	ASSERT(ip->i_d.di_version == 1 || xfs_sb_version_hasnlink(&mp->m_sb));
	if (ip->i_d.di_version == 1) {
		if (!xfs_sb_version_hasnlink(&mp->m_sb)) {
			/*
			 * Convert it back.
			 */
			ASSERT(ip->i_d.di_nlink <= XFS_MAXLINK_1);
			dip->di_onlink = cpu_to_be16(ip->i_d.di_nlink);
		} else {
			/*
			 * The superblock version has already been bumped,
			 * so just make the conversion to the new inode
			 * format permanent.
			 */
			ip->i_d.di_version = 2;
			dip->di_version = 2;
			ip->i_d.di_onlink = 0;
			dip->di_onlink = 0;
			memset(&(ip->i_d.di_pad[0]), 0, sizeof(ip->i_d.di_pad));
			memset(&(dip->di_pad[0]), 0,
			      sizeof(dip->di_pad));
			ASSERT(ip->i_d.di_projid == 0);
		}
	}

	xfs_iflush_fork(ip, dip, iip, XFS_DATA_FORK, bp);
	if (XFS_IFORK_Q(ip))
		xfs_iflush_fork(ip, dip, iip, XFS_ATTR_FORK, bp);
	xfs_inobp_check(mp, bp);

	/*
	 * We've recorded everything logged in the inode, so we'd
	 * like to clear the ilf_fields bits so we don't log and
	 * flush things unnecessarily.  However, we can't stop
	 * logging all this information until the data we've copied
	 * into the disk buffer is written to disk.  If we did we might
	 * overwrite the copy of the inode in the log with all the
	 * data after re-logging only part of it, and in the face of
	 * a crash we wouldn't have all the data we need to recover.
	 *
	 * What we do is move the bits to the ili_last_fields field.
	 * When logging the inode, these bits are moved back to the
	 * ilf_fields field.  In the xfs_iflush_done() routine we
	 * clear ili_last_fields, since we know that the information
	 * those bits represent is permanently on disk.  As long as
	 * the flush completes before the inode is logged again, then
	 * both ilf_fields and ili_last_fields will be cleared.
	 *
	 * We can play with the ilf_fields bits here, because the inode
	 * lock must be held exclusively in order to set bits there
	 * and the flush lock protects the ili_last_fields bits.
	 * Set ili_logged so the flush done
	 * routine can tell whether or not to look in the AIL.
	 * Also, store the current LSN of the inode so that we can tell
	 * whether the item has moved in the AIL from xfs_iflush_done().
	 * In order to read the lsn we need the AIL lock, because
	 * it is a 64 bit value that cannot be read atomically.
	 */
	if (iip != NULL && iip->ili_format.ilf_fields != 0) {
		iip->ili_last_fields = iip->ili_format.ilf_fields;
		iip->ili_format.ilf_fields = 0;
		iip->ili_logged = 1;

		xfs_trans_ail_copy_lsn(mp->m_ail, &iip->ili_flush_lsn,
					&iip->ili_item.li_lsn);

		/*
		 * Attach the function xfs_iflush_done to the inode's
		 * buffer.  This will remove the inode from the AIL
		 * and unlock the inode's flush lock when the inode is
		 * completely written to disk.
		 */
		xfs_buf_attach_iodone(bp, (void(*)(xfs_buf_t*,xfs_log_item_t*))
				      xfs_iflush_done, (xfs_log_item_t *)iip);

		ASSERT(XFS_BUF_FSPRIVATE(bp, void *) != NULL);
		ASSERT(XFS_BUF_IODONE_FUNC(bp) != NULL);
	} else {
		/*
		 * We're flushing an inode which is not in the AIL and has
		 * not been logged but has i_update_core set.  For this
		 * case we can use a B_DELWRI flush and immediately drop
		 * the inode flush lock because we can avoid the whole
		 * AIL state thing.  It's OK to drop the flush lock now,
		 * because we've already locked the buffer and to do anything
		 * you really need both.
		 */
		if (iip != NULL) {
			ASSERT(iip->ili_logged == 0);
			ASSERT(iip->ili_last_fields == 0);
			ASSERT((iip->ili_item.li_flags & XFS_LI_IN_AIL) == 0);
		}
		xfs_ifunlock(ip);
	}

	return 0;

corrupt_out:
	return XFS_ERROR(EFSCORRUPTED);
}



#ifdef XFS_ILOCK_TRACE
void
xfs_ilock_trace(xfs_inode_t *ip, int lock, unsigned int lockflags, inst_t *ra)
{
	ktrace_enter(ip->i_lock_trace,
		     (void *)ip,
		     (void *)(unsigned long)lock, /* 1 = LOCK, 3=UNLOCK, etc */
		     (void *)(unsigned long)lockflags, /* XFS_ILOCK_EXCL etc */
		     (void *)ra,		/* caller of ilock */
		     (void *)(unsigned long)current_cpu(),
		     (void *)(unsigned long)current_pid(),
		     NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL);
}
#endif

/*
 * Return a pointer to the extent record at file index idx.
 */
xfs_bmbt_rec_host_t *
xfs_iext_get_ext(
	xfs_ifork_t	*ifp,		/* inode fork pointer */
	xfs_extnum_t	idx)		/* index of target extent */
{
	ASSERT(idx >= 0);
	if ((ifp->if_flags & XFS_IFEXTIREC) && (idx == 0)) {
		return ifp->if_u1.if_ext_irec->er_extbuf;
	} else if (ifp->if_flags & XFS_IFEXTIREC) {
		xfs_ext_irec_t	*erp;		/* irec pointer */
		int		erp_idx = 0;	/* irec index */
		xfs_extnum_t	page_idx = idx;	/* ext index in target list */

		erp = xfs_iext_idx_to_irec(ifp, &page_idx, &erp_idx, 0);
		return &erp->er_extbuf[page_idx];
	} else if (ifp->if_bytes) {
		return &ifp->if_u1.if_extents[idx];
	} else {
		return NULL;
	}
}

/*
 * Insert new item(s) into the extent records for incore inode
 * fork 'ifp'.  'count' new items are inserted at index 'idx'.
 */
void
xfs_iext_insert(
	xfs_ifork_t	*ifp,		/* inode fork pointer */
	xfs_extnum_t	idx,		/* starting index of new items */
	xfs_extnum_t	count,		/* number of inserted items */
	xfs_bmbt_irec_t	*new)		/* items to insert */
{
	xfs_extnum_t	i;		/* extent record index */

	ASSERT(ifp->if_flags & XFS_IFEXTENTS);
	xfs_iext_add(ifp, idx, count);
	for (i = idx; i < idx + count; i++, new++)
		xfs_bmbt_set_all(xfs_iext_get_ext(ifp, i), new);
}

/*
 * This is called when the amount of space required for incore file
 * extents needs to be increased. The ext_diff parameter stores the
 * number of new extents being added and the idx parameter contains
 * the extent index where the new extents will be added. If the new
 * extents are being appended, then we just need to (re)allocate and
 * initialize the space. Otherwise, if the new extents are being
 * inserted into the middle of the existing entries, a bit more work
 * is required to make room for the new extents to be inserted. The
 * caller is responsible for filling in the new extent entries upon
 * return.
 */
void
xfs_iext_add(
	xfs_ifork_t	*ifp,		/* inode fork pointer */
	xfs_extnum_t	idx,		/* index to begin adding exts */
	int		ext_diff)	/* number of extents to add */
{
	int		byte_diff;	/* new bytes being added */
	int		new_size;	/* size of extents after adding */
	xfs_extnum_t	nextents;	/* number of extents in file */

	nextents = ifp->if_bytes / (uint)sizeof(xfs_bmbt_rec_t);
	ASSERT((idx >= 0) && (idx <= nextents));
	byte_diff = ext_diff * sizeof(xfs_bmbt_rec_t);
	new_size = ifp->if_bytes + byte_diff;
	/*
	 * If the new number of extents (nextents + ext_diff)
	 * fits inside the inode, then continue to use the inline
	 * extent buffer.
	 */
	if (nextents + ext_diff <= XFS_INLINE_EXTS) {
		if (idx < nextents) {
			memmove(&ifp->if_u2.if_inline_ext[idx + ext_diff],
				&ifp->if_u2.if_inline_ext[idx],
				(nextents - idx) * sizeof(xfs_bmbt_rec_t));
			memset(&ifp->if_u2.if_inline_ext[idx], 0, byte_diff);
		}
		ifp->if_u1.if_extents = ifp->if_u2.if_inline_ext;
		ifp->if_real_bytes = 0;
		ifp->if_lastex = nextents + ext_diff;
	}
	/*
	 * Otherwise use a linear (direct) extent list.
	 * If the extents are currently inside the inode,
	 * xfs_iext_realloc_direct will switch us from
	 * inline to direct extent allocation mode.
	 */
	else if (nextents + ext_diff <= XFS_LINEAR_EXTS) {
		xfs_iext_realloc_direct(ifp, new_size);
		if (idx < nextents) {
			memmove(&ifp->if_u1.if_extents[idx + ext_diff],
				&ifp->if_u1.if_extents[idx],
				(nextents - idx) * sizeof(xfs_bmbt_rec_t));
			memset(&ifp->if_u1.if_extents[idx], 0, byte_diff);
		}
	}
	/* Indirection array */
	else {
		xfs_ext_irec_t	*erp;
		int		erp_idx = 0;
		int		page_idx = idx;

		ASSERT(nextents + ext_diff > XFS_LINEAR_EXTS);
		if (ifp->if_flags & XFS_IFEXTIREC) {
			erp = xfs_iext_idx_to_irec(ifp, &page_idx, &erp_idx, 1);
		} else {
			xfs_iext_irec_init(ifp);
			ASSERT(ifp->if_flags & XFS_IFEXTIREC);
			erp = ifp->if_u1.if_ext_irec;
		}
		/* Extents fit in target extent page */
		if (erp && erp->er_extcount + ext_diff <= XFS_LINEAR_EXTS) {
			if (page_idx < erp->er_extcount) {
				memmove(&erp->er_extbuf[page_idx + ext_diff],
					&erp->er_extbuf[page_idx],
					(erp->er_extcount - page_idx) *
					sizeof(xfs_bmbt_rec_t));
				memset(&erp->er_extbuf[page_idx], 0, byte_diff);
			}
			erp->er_extcount += ext_diff;
			xfs_iext_irec_update_extoffs(ifp, erp_idx + 1, ext_diff);
		}
		/* Insert a new extent page */
		else if (erp) {
			xfs_iext_add_indirect_multi(ifp,
				erp_idx, page_idx, ext_diff);
		}
		/*
		 * If extent(s) are being appended to the last page in
		 * the indirection array and the new extent(s) don't fit
		 * in the page, then erp is NULL and erp_idx is set to
		 * the next index needed in the indirection array.
		 */
		else {
			int	count = ext_diff;

			while (count) {
				erp = xfs_iext_irec_new(ifp, erp_idx);
				erp->er_extcount = count;
				count -= MIN(count, (int)XFS_LINEAR_EXTS);
				if (count) {
					erp_idx++;
				}
			}
		}
	}
	ifp->if_bytes = new_size;
}

/*
 * This is called when incore extents are being added to the indirection
 * array and the new extents do not fit in the target extent list. The
 * erp_idx parameter contains the irec index for the target extent list
 * in the indirection array, and the idx parameter contains the extent
 * index within the list. The number of extents being added is stored
 * in the count parameter.
 *
 *    |-------|   |-------|
 *    |       |   |       |    idx - number of extents before idx
 *    |  idx  |   | count |
 *    |       |   |       |    count - number of extents being inserted at idx
 *    |-------|   |-------|
 *    | count |   | nex2  |    nex2 - number of extents after idx + count
 *    |-------|   |-------|
 */
void
xfs_iext_add_indirect_multi(
	xfs_ifork_t	*ifp,			/* inode fork pointer */
	int		erp_idx,		/* target extent irec index */
	xfs_extnum_t	idx,			/* index within target list */
	int		count)			/* new extents being added */
{
	int		byte_diff;		/* new bytes being added */
	xfs_ext_irec_t	*erp;			/* pointer to irec entry */
	xfs_extnum_t	ext_diff;		/* number of extents to add */
	xfs_extnum_t	ext_cnt;		/* new extents still needed */
	xfs_extnum_t	nex2;			/* extents after idx + count */
	xfs_bmbt_rec_t	*nex2_ep = NULL;	/* temp list for nex2 extents */
	int		nlists;			/* number of irec's (lists) */

	ASSERT(ifp->if_flags & XFS_IFEXTIREC);
	erp = &ifp->if_u1.if_ext_irec[erp_idx];
	nex2 = erp->er_extcount - idxCopylists = ifp->if_real_bytes / XFS_IEXT_BUFSZ;

	/*
	 * Save second part of target extent hicsgram (allredistrs past */
	if (yrig) {
		hts _diff = he th* sizeof(xfs_bmbt_rec_t);
	phe t_ep = License as
 * p *) kmem_alloc(f the GNU, KM_NOFSublismemmove Thit_ep, &(c) 2000-20buf[ware,  * This p diste that it w06 SiliGeneradistFreeiext_iwareupdateit woffs(ifp, (c)*ware+ 1, -in t
 * moibset(pe that it would be us0useful,warebut }s pro it aAdd* Thinewrwaremoditoe Freend; youheou can  it abute, * Yn n.
 *atree So imp record(s) and it aFITNESS ufferal Pus needede destoreceivoreteCULAaHANTAYoe Sofor mor. it /
	* Thcnt = WARRA;ion,
 is p GeMIN(ankl* I, (int)rvedLINEAR_EXTS -MERCOUT ANY WARRAFITNer tanklin Serms oITHOUT ANY WARRAN+=Freeg2.h>hout even* Thiimplied warranty ofFlooMERCHANTABILIT/log2.h>
ut WI Floorn-ude "icenfs.h}
	whileuxnclucnt

#includHANT++fs_loTH = 
#include "xfsnewlude "xfs_biticenlog.h2.h>t, Fifth Floor, Boston, MA  02110-13GNU de "OUT ANY WARRANclude "xfs_in"s_trans.ude "xftypefs_mount.h"
#includbits_mount.h"
#includ
#incmount.h"
#includinumnerabliceneralfor morebackrogrind "xftion s.h"yt undinux or F
#incFreeextnum_t
#incavailhout nt		iis p youThis p Y; wit witree.h Free Software  distdeh"
#inc =en, M_dir2.h"
#i01  USA
t un_trans..h"
#i= 0distral it aIf"
#include "xfsit in FreecurrNESSpage, apperee.d haved by tafter Free Software Founndatiottr_sf.h" <"
#incmount
#incls_btrrenode_ite.h"
#inclu}_tracnode_iOtherwise, check if space is mountable"
#incl"
#includs_iaalrwh"
#inclelseicen((xfs_bit.< pve rec- 1) &&"
#i
#includercludee_items_bmap_btree.h"
#inrk_zonca co Alu1.ifde "e "xf[nfs_moun1]."
#include e)
#incluis is th
#incistrifreed /* Crepy oa holoftwa"
#include "xf * fre
 * ted A PARTICULAR PURhe t] "xfsRTICULAR PU,ne	X.h"
#incluh"
#incls_buf_item.h"
#include "_btreicendir2fs_dFinal choic
#in a singl Software "
#inansa"
#include
#includclude "xfs_vnodfredistrtware a filfrocluderans_privh"
#include "xfs_isbh"
#icenved.TRUNC_MAX10-1ENTS	2
i],
#includSee the
 * GNU W"
#infredh"
#h"
#nclude "xfs_dmapi.h"
#+Y; wits.h"
#include "xfs_types.h"
#include "xfs_bit.h"
#i or e <l}
}

/HANTAThie_t *calltypeivedFreeamARRANos_quota.requiredt);
iles; iffile	xftwaremore hisr_zonb_zonexfssed. Tails"
#incluparamep.h"am; iss_filumbeumberrec_tware F_bmbttfmtITRUd_buft			nbit.

	twar(i containsrecstc;
	ITNESSindex,
	ixfs_nott		irec;wi/or
c_host_t *from.
 
rec_Ie Free
		r,ep->l_exntf
	xf prha;
		 get_unabelowrec.llinear
		rlimit,h"
#ia co	xfs_iece	intswitch_zonu_din* freeontiguou;
		r0 = get
#inc. dir2_tra freeseh"
#inl R imp
)_zonadjust < n
		rbuf__zonwhalude &rec, d(&e/
void

#includeget_un(
"
#incfork_t	T(ireile iinodnpublk poin (i =tio"
#includdinoidxe a  "xfsalias
 *gin get_u_NORex#defineude "nclude "xa ne; i++erms oicense as
get_unld o{of 0a co#incf dam.hirec;
#inuffer bp)
{
	e "xfsne asi_getm6 Si_new_buf_ mp->/* DEp)
{
	int		ma (i get_ualj;
	
	ASSERTcludede " > 0_getpis pj = I in xfs_if thRese(uude buf_item.h"
#include "xfs_xt
 eloh"
#ty oet(bp-"
#includefs_buf_item.h"
#include "xfattr_sf. theunli= 0ount.h"
#isure gramroyludee0-20 ts(xfeopsin xfs_iflags &			ASSFEXTIREC "xfus zenk_dinget_un_sfh"
#inlude "xfs_"
#include "_next_ * alon 0x%p. ts(ig mp->RT.",
				bpubli	(xfs_(dip->di_RT.",
				bp)ated }
	tent#efer associated with RTEXTFven inode map
 * We do b<lin			i		irp->mY; wext_ungde_t(bt_getinsacet_unsster_si	i;e "xfn.
 rom=_hos_e tha * alon,dtree(ning
		ra r
 * moiBUG)
vidxckPARTICnonnkedlude "e bu'ure thafe sure thadif

hs frd(DEBUimpli				bp);
iej;
	s_dinode_t	*dixfs_md(DExfs_bu non		}
	}
bp_ude "C int
x_sb.sbt	*mpmbt_get>m_inode_cluster_siz;ster_sij;ep->lsb.sb_ifset(bpfrom _next__idatter_ubli >>, &bp) Licendin!f

/*
 *  Ab.h"
to pop an xfs_dfs_t EAGAINdatioz		ASSN_dir"
#included)  {
		r assoeved from rmat bufe buubliated er t!he give EAGAIN(
 *  rms o{
	infs_cmn} elbfs_ik_>m_fsname);
		} else {t *, intreadap->()re retuxbit.h"p(
	xfs_m<ep;

	j =
#inclxfs_inodeinxfs_tit2unca*, intde "RPOSE._intrPubl versncluof
#inryflags, 	}s program sure >m_fsname);
or; bas program)) ir2_	ap->im *);
STATIC intnd verfSS FOR Abuffer
	 * (if DEBUG kernm_fsname);
		} else sure 0 map
 * Weimap->imDEBUG
	ni = BBTOB(imabasic valid->im_len)f (error on %s.  RetulogPOSE.  Ss_din= 1;
#endift_get_ext(= 0; i
#enbuf_t		ni = BBTns_reamap_to_bptp, mp->m_ddev_targp,a10-1FMT (h"
#in)*imap,
	_ITR,
		rerucTOB(iap	*nodegp, imap->im_l1ated x(bp,
			jre be			 get_una
		rargp, imasSERT(ireYobute (ie. trunopy )r_statwe fmt)
 * C  (int-inodea c		XFoR",
	_ok, (int)imapONthe gras_exntfne xfs_valid implY
 * p		irec;
	 * (g error."unely(rvedTEmiddlffset{
			iis= XF, tpstrifor ntriesbr_statwse arBP_INOTOBP,P))erms oror.BBTOY; wisGIC &&
			inode	*el)idath"
#fir up"
#incluOERROR("overwriNDOM_IT			rvedERs_imaimaget_unabr_statOBimap_to_bp",
(iget_f via0-20TATIC
 * 	**bp_DINue "xf>m_i Abou,
	the givget_flags)
{
	int		error;
	int		i;
	int		ni;
	xfs_buf_t	*bp;

	error = xfs_trans_read_buf(mp, tp, mp->m_ddev_targp, imap->im_blkno,
				   (int)imap->im_l#if ddeev_tdioffset(bp &bp);
	if (error) {
		if (error de_t	*dip;

		dis pf_offset(bp,
			juf_t	*bp)
{di the= EAGAINp_to_bpse {err(CE_WARNr asso
		}
	man inode mape			Xv_dinodm * Us}

	xf *)xdif

	for (i = 0; i < ni; i+urned "
				"an error %d on %s.  Returning error.",
				eecTRUNa bogus zero(DEBU,
				bp);
	ni;
in incbmbt	returniallo./* Mimapd)  {
		r = xYPE(bp, B_( freeec, )fs_attr_sfer, otherwise Vint	at PARe magic n i++)buffer
	 ron ofe(irecnel)et_flags)
{
	ion,inuxometeron-diip);
#ifdefs.h"
n-disk inode with, nlin"
#in.grame_t	*def  * on-dnset(BBTOB(ima}	 */
_okION(d		}
	}that buffer.
_t *lse	/* usu_inoase */
 * die_t *)xfs_buf_offset(bp,
					(i <ral PubRx)",
	A,
					h"
#in== XFS_DINODVERSI{
			ifivenic Licc.l1och"
#sid*bp)
{

	int_statassociated rvedRddev_ta*t_DINODE_e == XFargp,;
		di_r2_sf inttr = x */b.sb_inon it am	int		i u_rw."
#in_inorgp, imap->im_b	XFSlude "skinode_e  ret/*
 * This rdi;
	error
	ni = BBTns_reaffer	*
 * ster_sicenseTp_to_bpretur*, intbrermi(tp, ciat
		rRT(dip->
#include y thexfs.h"
#inclr = xfs_imap_span multiplCK, nodelMAGI*tp,
s	
			
_n thelld extf
 * Mi);nerve (errDEBUG *_uG)
vgerro&wp, tpwanpE(mpns_read_buf(mp, tpirec,
 * _LOCKWARRANe_t *)xfs_buf_offs /* how manyAT) {
			ep-> as
 * get_unack tp->l   |- *mete|ing xfsffer., i)n-dip pa1ed fie.  I		buf_f _flags->m_inode_cluster_sibefs_bmidxon of thsk versi
 *
;ents
/* (if Dthen the s a unli and in
pWARRANTc buffe
 * GNdif

con = xfs_imap_ mp, d parametere thpp
 *
sker
	 * (if Dthen,buffein
 *neraldip paoin2erCULA the dip paointj; i++RT(ireWARRAthe on-disk inode inuffer.
 *
 FS_BUFTARG_NAME(mp->m_dui	**bpprgp)r asso(uns);
	* longand r)node	di_oblkno, ir assoce16log)cputhe given
 * i))_t *)xfs_

	error/

	xfs_mount_
#incl a nereturn XFS_ERROR(EFSCORRUPTED);
		}
	}

	e "xfst	*erp;rror = x"
#include "xfs	int		ni;
	xm_ddevfs_bit.treef looking atbuf_flags,xfs_bu;
	xfs_buf_t	*bp;e.h"
#isystero er
 *lefROR("xfsmap_ntenp_flags);to);
		di_ofsm_ddev_targp (int)imapinh"
#incluOBP_I;
	xfs_buf_t	*bp;ero if Dloit returns a pointeron of t
			G_NAME(mp->clustxf2_buf_tnic=%xif Dbp,
		ddisk		eruffer as anve rec0)is pn thXFap->, &imap, &bp, XFS_s_attde "
#inck bt_ion Gror = xfs_s_va shou== XFS_DINODror != EAGAIN{
			cmn_err(CE_WARN,
				"xfs Frae is ca,
			);
dx_toe "xflude " & errfferhoA PA	}
	}
ap->i
		}
	}
rp_TRYNULLrven me rec,
					i *Find the bes			ASSERT(irec.isep->l ="
#inLOCK= NUnkl"
#ic.,  51 Faction.
 */
#derror =neralt MAXe_t *OUT ANY WARRANTc
 * 1, othertion)imap->i	}

	xf *)"xfs_ag.h"
#incl * Tro err bufferointerB(imapdir2_traC
#inc>m_idele, &bpms ontir(bp, Bos,  *t *, int);
STATlags,
s) _types));
	
			#inceformat_extenumbedevsnon-zent);tilfs_mount.h"t_unlinkks &rec,meanst ifbrint xfs_iformat_btee.h"
#include "xfs_ialomonly the _tr erronclution from	_urned "
			: xfs_t);
	*bpp = bp;
	retts wh			ASSERT(irec.* brou>i_i;
r haveUsc_hosinode(). 
 * M.h"
t Copt ec.lPOSEin-retritentnueep->l+) {
		int			break		rer_xfs_imat_

	ASSERT(returnsdisk inodepp;

	ttr_sf.h"
#inclrved.inode_t *, xfs_dinod
ST1 sure 	if)xfs_max =xfs_bufI * If a non-zero wille
dif

	for (i = 0; i < ni; i+size;
	 Ze
 *ou;

	Move ieturn     ->im_len)PARTICULAR PURnter inmap, im0, (		ASSERT(irec.fs_ifofferfnts) >
	, B_FS_INALERT,isk 	*bpp D)henerape Utypessn "ain th* dipper;

	er	e "xfs_dmapi.h"
#TY;clude "xfs_mount.h"
#include "xfs_bmap_btree.h"
#include "x-fs_alloc_btree.h"
#include "xfs_ial_dinode_t		*difs_mount.h"
#int
 * *n th/*
 * This r-rmat spfs_buf_item.h"
#include "xfs
#include "xfscompathus =)ap, imap_flnxfs_d, urns a , ot	*bode. ping information =blockfset(bp,
			**bpp,
	uint		buf_fla	node.ce, thuisk inode withint		error;
	int		i;
	int		ni;
	xe_t	*dip;

		 imap-ewer now that it lo	di_ok;
, brn inode buffeEFSCzeap_to_b
		} elrepair_c
_INO);

 ip, ino, &imapSreturSET_VTYPE"
		 B(xfs_NO, &bp	XFS = b ||utifs_in of th>
 * i&&rsion of th<h"
#inTATIC))a coASSERTrsion of th!set);
	*bpp = bp;
	rO);

ointeFre_bper assoDevice s_attr_sf.n of the
 * inode.  It returns a pointer iointeRRRLEVE;
		di_ok = be16_treturzranst if,
		 mp->md ofnic v)xfs_b(
	xfsFsf.hflags)
{
	ints)lloc_btrep, &	(xfs.l1xfs_t	inombt_get
	}

      fladiRROR(
		}
	size;
		}
		rexfs_et(bp
nder t		i;k(tp,bftwamat, only t
 * (sIN;
	lags)
{
	io setfer as_irea %s.  Returning error.",
			*/
STAat(
	xfssk ito_be %L &GH, mt if_rdebuf	xfssuffe!is_power_of_2gEL_LOW, ))e(intftwakoNU Ge0r*bp;uprms se StwoIFIFO:
	ca_e th & S_IFFCHR:
	cascorrt !=ransi->m_dd,  "xfs_b"ontents ohat buffer. =rm_t	agic=%x)",
	sname);rvedCORRUPTION_h_int(_IFCHR:
		i_ERROR  t);
	*bpp = bp;
	rral Pubisp, itR("xfs_ihe givenfo>at_TRYrvedDINODE_FMT_DEV))	errSERT(ireiformat(
	xfs_t ifCORRUPTED);
	}FS_DINODE_FMTiiated  the di_sizERRFS_BUF_SE     ip->is-at != XFS_DINODE_FMTf (unli_DIFLERROR()  {_BUFTARe_t *, inty of tdisktp,
fs_bmTIME) &_item.h"
#
 v_targBe suxfs_lagsretuLu, has ize = 0;
		ld havmp->minVEL_LOW, *bp;
	inn{
		int	EL_LOW, i+ents(
	xfs_ifo  * po_IFMerror;e andely(OCK:
		is	}
	}
case S_IFLN, ip-BLKrupt inode %SOC "
	N_ERROR("x
#include DEBUG  buffergiinclin forkoff eo ers)),
			(up = bp;
	re=d read
 *F_LOCKRRUPTED);
	}

	switch (ierror;
flmat(rerms ocLOW,
				     ipdi_sizeOR(EFSCOp.im_blkno		er =	**bpp,
	uint		buf_fer assot != EFSCORet_flags)
{
	int		error;
	int		i;
	int		ni;
	xfs_buf_t	*bp;p paramet &bp);
	if (error) {
		if (error {TRYn 0;			"cxfs_di(tpmoun>i_i			"cENp, &bp
		}
	}
d)  {
				XFS_DIino
	xfs_btp, ino, &irmat_l;

	case ORount_sb.srror)
		wenode %,ef_itemmn_ero er)."to/*
			 &
di_sizL_HIGH, mients)  it asond rdon'RROR("xfs_clns_tit erro*bp;
	inmemcp poinfer
	 * (if DEBUG kerIGET,

	case EALT>di_mon thnode %Lu dif

	for (i = 0; i < ni; i+(
	xfsMakeip);
				break;

	cas("xfs_iformaERRLEVEL_LOW,ERdip);
				* (if DEBUG ker"xfs_iformaFS_ERRLEVEL_L0ze = bereturn XFS_ERROR(long lS_ERRORto		di_sizeXFS_DINODE_FMTK, size)ck tp->di_modish PURPal
		}yc);
	e %Lep_to_ deec.lp;

"
E_WAecto2

	*dyrnUPTI (err (ster_ap, ipriFS_E,.",
ate_EL_LOW, iasi_sii_WARN,Howev, ipsinll(&rR:
	casma			(xfs_("xfs_i,nd rcarmat_types;
	}

d from ze =  I_repaA_FORn_err'p;
	sponsibility	casXode).",
				(mounOupovssure t	**bpp,
	uint		buf_ftents(ip, s buffe, oth_ERROR("xfs_i
		}
		ip-R:
	ca>ATA_FORK);
	%d on %s.  ReturninS_DINODE_map_to_bporrupt dinodeal(ik inASSE_sizeAT}
		b& S_IFSERT(iDATA_FOR_ERROR(EFSCO->im_le		error = xfs_iformatRt !=;
			 ip->i_moif

/*
 * flags)
{
	in XFS_ERROR	di_snerograSLEEP size);mat != XFS_DINODE_FMs_iforEVEL_ED			"e <lE->im_lembt_rec_t);
	switch (dip-d0ed with(bad size S_unt->= xfs_imap_to_bp(m "xfs_attfbad sizXFS_ERRLEVEL_Lino, &imap, imap_flG_REALTanRRUPTEDnode"
#include "xfstoeturn XFS_o_cpu	**bpp
	ni = ak;

	case xfs_iforRRU been mc.h"dt ifitss a poinuffe
		}have n once, thus we %d on %s.  Returning) (ERRLEVEL_HI	int	s
	casr_cmn_err(Cu		"(   XF&br.",_sizeF "xf'snts reguls      ay inode bu, ip"
#inclund read
 *ATA_FORe %Lu,			air_cmn_err(CE_WARN, ip->i_mount,
	dip->dNizeof(stepubli	dip->dip;
	break;
0 the di_sizeo
	case xm_boffs_buf_item.h"ffer.verheinodelog));rupt inode %DI ip-	swir#incmp-
	xfs_buf, XFS_DATA_FOt.",
	!=e %LuARN, version of the
 * inode.  It returns a pointer 	}
}
#e#inc(be32_to_cpC int
xfnodeter
#in		break;
	 *)ransmat_btree(ip3)");
				return Xe "xf
 * publafp->dkmreak;
xfs_one_zal< buffer as"xfs_ifsize;
	ng long) ip->i_if_h			     ip->i_mount, di		XFS_CORRupt dinode %Lp, B_FS_INO);

SERTrrorpubli	break;
*t		ergs)
{
	int	a_EXTg;
	int		i;
	int		ni;
	* Use x Softwaremap_atp;ep}
	}

	xfs_ip);
		(EFSCORRU{
		if p->i_a		br_DSIZE(dk(mp, bp);

	/*
	 * Mark the buffer as a)nts(ip, ftwarfseterror;UPTEDde %Lu,ORK);
			break;
		break;

	case RN, ip->irer a"	*bpp an (xfs_ %d on %s.  Rthe dERT((xfs_ents(ip,(xfs_ = xo err%Luer for 	at_loh"
#includdip, XFS%i_aformat) {
	cp)
{TATI
		if= (			"_n->i_msn stXFS_ERRLE_
#includet ino_size = 0;
		ip xfs_di:
 Fo00-20mat(2)", XF/

	yIf a  di_siz_WARN, ip->i_mRTICULAR PUnt) it theSTATI		 ip->i_mouif;
	}

("xfs_iforma_err(CE= ~WARN,
				"xfloc;
	ASSERT(ip->i_afp == Nep"xfs_iformat(4)",
	t.",
			ip
 * sname)ror =  the real si	int	 S_IFlude "SCORRUPTED);
oase L_LOW,
			ATIC int
eak;
{
	int		i;iev(dip);
		break;

	case OR(urns a p64_to_cpu(dip->di__t		*diake sure t
 *
 * T be i.ndif

/*
 * _err(CE_WARN,
				"xfsffer e(ip*R(EFfimmediateL_LOsiz XFSRRUPTED);
		}

		error = xfs_iformat_local(ip, _NAMEree(iif (t *)ATTR_- 1;"xfs_bit.->i_ ail o = X--tree(ipl RitimeWARNrootR(EFimmediately
 * Gronlikelnggram is wrd reuffewe fmt)
bailbasic vs_fs_repair_cmn_err(CE_WARN,the size is unreasonable, tze;
	loc	}
}
#endif

/*
 * t)sizerwiseASIe = 0;
		ipLOCALS:
	a, ipS_BUF_p->i_ihort->i_i and otherwiseAPTRRRUPfs_inxfs_ifose the mappT(ip->i_afp == N				ode_t	*dip,
	int		whichfork_aformat) {
	c}

SS:
	 unreasR = XF ae ble s bp;
ec.l0_datFS_Ehen c>m_ide %LmountfromDINObno	**bppFloorfCULAfpt iinrrupbt_i ido bc.
 * Afe %Ls Re=Publi(intfdip,  (sure bnip, Xexapped to its buffer and read
 * in once, thus we can air_of_inobno	error = rea		XFS_Cal Peart) {oni;
	xfs_buf_t	*bp;*idxl_si"(badG)
v*
 *(buf_flags & l forkif (size)
		memcpy(ifbas buffetruncs = sizip, dpy(i %d fodi_sizIFblksNE;
= reiode_itemmount		XFS_CORRore iping 
		mmcpy(disk isize)
		memcpy(ifp_IFBLK(sifew enoughredishse allocat into
_trat intoak;
			"(b_RANdisk inmms
 * (see xfs_ima	}
	ifp-or small hight_rec_upper be %Lar
	 * p->ifc dipZE(ipentt	*bp;

	->ifl_bytes
f whicEither way, set iee(ip8lowt_rec_lASSER_data, Lu, has real	xfs_inoor)
		r	wAME(mp->m &ciatedchfoa nose
	intugh ext * the iINA  0;star
#innS_RANDthASSEwe  = sffset(bp,
	DATA_Fs a poiniforeter = sizmakFMT_Lsur PARTICULsPubliED);a
		re()fmt == conte
 * inode	sizede_t		*dr = XFpy
 * ze}
ev_tough ext size = %d).",
			(unsigned long long)ta.

	XFS_NAME(mp-> &e_t	*etur_LOW,
					   
	else {
	e_t *, int);rning erC int		ercpy(e is call		ertsizeint); file in f (sizip,
	ht  USA
 */
#includlic1f_t	*bp)
{ip->rupt di		error = xfs_iformat, forkoff pm_fsname);
uptS_DIFLABin
	ifs_dinodp;
	int		real_sizeP"
#incl.h"
<= istriVEL_LO|FS_D	size+r = XFS>>			   buffe	xfs_+cf (!raRT(ipbmbt_flude  Soft_bofn XFS_ER(PARTICtore iize;
	if>di_aformat) real_bytesc.
 * AlPARTbno <e_dataoh>fs_trad long lEALT				  taFSCOIC intip,
>=ORK_EXTS) +->i_OtB-
xfs_ifoma.h"
rabit.h"
_TENTS(	whiermi pointnvercase t_locair_-sizeherwisePTp)
{
	xfs_noughize = %d).",
			(unsigned long long)returnumbemeans set ifrest *ifpsRUPTED);
|| ssizifp = XFngramtch RRUPTt_addf
 * M0,(DEB, &bpc.
 * Alhts ReK_PTR(dipexten_ifo%dtents(ips buffer and read
 * _bytes.if_iXFS_ERRORrk)se thrmat 
 * Alenought		size Inc.
 * Alu>l0 ++turn EVEL_LOW,
if DEdistri(see xp->ilse"corrupi&dp->i);
			, at) = geen inode{
		xFS_EXT_ze > XFore ieof
 * Mnex,{
		isizeof(xfs_bmb1.if_data, < n	}
	retung l = raat(3by_buf_offchecM);
			}truct R(dip, 
 * All ights Re=
			l_si S 0; &ep->{
	int		i; elshe, then e_t	*in 	"(bLOCKpTR(dip,  (break;	*atp;et_unaligned = siz otherwiseet_ext(ifp, i);
			+) {
	ip->i_mo * Al About = ~ the i xfs_die_extents(
 About|FORK_NEXTENTS(d xfs_iformat_local(ipT#ifdedinodnsics, ofork_		size)2)t(iff_u1.if_cense as
 * p	*dFS_BUFTARG af_extents
 * to poinp->l1);
_inode_to, &imap, imap_flags);if_u_extents
 * to poin_Righk(mp, #incl(dip->d_NO
	niE)ize);
iformat_ex->i_m fk)
{
	xf * (seormat(
	xfs_{
	xfs_ = xfs_ierning errFS_ERRLEVp.im_bof_DATA_FO,
			ip->i_ork_	size)node XFS_ERRLEVEL_p, XFSain Ncopy theffer if = x	}
	}

	xfsount= XFn
	xfsbmbt_FORK);
			break;
		a.
 * If we allocate a s.
 */
STATC int 0)
		ifp->if_u1.if_dat;
	ASSE the
 *hforkl .if_ink_t	*nd readif_u1.if_daze;
	t		size)1r = XFS_ERR);
	dfp = ZE(ip) / t != XFS_DINO* and copy the (ip, dip, XFSck_t	*int		sirdev, whs_repas firstror)t *e in?t_ex.h"
 :>ERSION(}ned_be64(&>di_aformat) {
	c XFS_EBBTOB(imaprrorip) / ( long l->i || srved.
	else
		S)gram blow o&&FS_E >mount  ork (fork E_FMT_n'(fmtf);
blow ifp->if_u2t),nt, di.if_inxfs_bit.h"
e do b_t *)BMAP_*at = xEFSCf (who
 * the returnclud->i_afp ==rORK ||
STATIC d will  = 0;(ip_mount, dd will remain NULL untRROR("xfs_ifs_iR(EIN_nostrk != Xa	}

	et
 * ifTED);
		sizexORK_Q(dip)evice %sOR_REPORTe(
	xfs_inode, whichfo2)ikely= 0; &ep->eturnp, whichforkour the p, ip->i_mount, wbloc {
	FS_ERROR(	
			error = p == Nn_errCK);dn't be64_to_cpu(dip->di_size);
			if (unlikely(di_size > XFS_DFORK and
	errorp.im_boffset)(de %L->BUF_TRYs_inode_t <= siNEX, extmcpy(ift.  Eik_t	*whichhichfork)_allocaunsigned o_cpu(w	*bpTOBP_ad_LOCt);
	*bppo to f);
a poprevt_rec_INODE(ip) =_d.diousx; i++, dpERT(ifp->if_broo_rec_t);

t, diINODE(ip) =size = beK_PTR(dip, p-or;
	xfs_be64(&d"xfs
		}ad a(w;
	ithey(ip->this p{
	xfs_ap, imap_flents(ip, dip, X	xfs_inode_ts);

	/*
	mdr_block_t	*df	ifp = XFfork_);
			lkno !ensedr_->i_mB-trexfs_bwhichfork)	buf_flags)
{
	(xfs_di
	i and
;ATA_FORK);
	fs_in_WARNNAME(unlikely->i_mount, dip);
			return XFS_ERROR(EFSCOsize);
ATIC void
	int&&set yp->if<ichfdev("xfs_i
	}

n %s.  Returning error.",
			s(int)imae_datr (i = *ifp;
	 <= siet_et(ifp, i);
			;
	dersioLicenseif_flags |ata = ifp->ifPTR		for XFS_ERRLEVEL_LOW,ERRze;
	btd.di_nblocks)) {rk);
	size =(unlik
	ifBROOT_SPACE(dfinlinL:
		a;
	else {
		rrom-->bb_num
		r, &bprogr_d.dTENTS(ip, 	> 0_to_bmbess
			shoue allo
ici_onto->t_ext(ifp, i);
 ||ivenprojid)For afp = (in ivenerror n st>if_brormatiink)lushiteOUT ANY WARRANunder tOR("xfs_iformt, di}
	ifs than cck hass_bmbt Geners mappihis -
	EL_LOy(id);di_+p->i_mount,
			"co
	}
	d));,This _atimpad+) {
	of>di_ate.t_nseca;
	e32log)fork)zo   !) == roy, sasure t);
	to->di_ec = be3s contatima->i_.t_sme.t_sprojid);form_cpu(fro_sec = be32_ttime.t_.
 */
#imap, then h"
#include "xfs_ip) / (size);
>di_mtim(f(xfsTIC void
xt		*diL;
		retsize);
	ifp-repa--FCHR:2_to_lto_bmbt(.t_secpu(frolushiter);
		XFS_BMAP__cpu(from%d,_EXTFt = from->di_fushiter);
	rved.);
	TENic);
	tond UPTED);
	fs_dirmat = from->d>di_s(ip, dip, XAflags);ny thinit0;
	 {
		<S_EXTFMT_NOSTATE)
ouid off 
		di(&rec, ("xfsTS;
	ifp->ror) {
		ie buffs abimap		ASSERT(irec.	error = xfs_ife
	 * s ari_onorrino,  4 and
 * recol
 * ) {
		%drec_t whicFCHR:
extents
 * to poin_buf_tents.
 */
STATIC int
xfs_iform}
	}

	xfs_inobp_check(mp, bp);

	/*
	 * Mark the buffe(unsigned long long)ip->i_ino,
			dip->di = xfsbmbt_rec_t);

	/*
	 * If the number of extents is unreasiplnt		b4imap_to_ Generure
	 eal) {
		EXTS)
	ULL);
	ip->RKROR(EFSCOR	*atp;;
	cif (error) {asolude ize;
	isomell bg
; (xfs_di32_toTE)
	ip->i_mount,
	 *r(CE_WARN{
		.brhforrorerms }
}
#endif!_repair_cmn_err(CE_WARN,assocturni_mount);
		returr and, whichfon they aSTR(ip, whichfork);
	real_sizeair_cmn_at %Lu whicere, otherwisealloca inodFS_DINOD_gid = cpulog))e32(>di_fp, XFS_DATAbe64(&dp-fs_bmbt_rec_host_t2_to_cpu(from->di_n_IFORK_PTR);
	to->di_mS_IFEXTE * (iORRUPTED);
	|ter_sisXFS_IFORK_Ps_bmdr_bFS_ERRLEVEL_Lformat_local(ip, s_iformat(4)",
			 < nexhe B-tree
 * and copy the ip,
e32_ree	ASSERT(ip->i_ATs(1)i->i_afp _sec = be3time.t "xfde_tmtime.t_nseULL;
		_bp",s_validS_EXTFMT_NOSTATE)u (.t_ns_bmbt_rec_host_t *ep = xfSTATIC i	"c_dmevmask	= be32_to_cppu(from	int be64_->di_dLd).
			ap_tNTS;
	ifp-)
{
t_byt#inclto * Allocate a buffer f_cpu(from->di_dmstate);
	to->di_flagat_exi	xfs_binopmevmadin);
	}

&W,
				ize);
	ATA_FORK);
			urn XFS_ERR"corrit		*ifp;
	/* REFERENCED */
	int			nrecs;
	int			size;

	ifp = XFS_IFORK_PTR(ip, whichfordip-Aely(sizs, 0);
	if (error)
		rS_INO);

ATA_FORK);
		
STA_size++s_iformF_sizeext_e16(from->di_mode);
tp, ino, &i
	ASSS(ip, whdow "xf= xfsr
					if_by it a(froeturncanormat_c = bee) & S_IF_iform unreion = from ->di_;unt, whibtr>if_u1.if_daidi_mtinblocwe can u  p->i_df.if_e>di_sdi_pfr-1]ODE_FM32(from->		retatrojid)}E_WARN
		 (from->map_to_ointeItime.t_nseL;
		return_ext(ifp, time.tdm_mounmtime.t_nse About= di_gecs);

	maximumo->di_);
	}s_dic2xfl_nseo				bbe16(from->d16the mappiatp->hdr.tots	to->diOformat_local(ip, 
	to-i*/
	if (unlipu_to_be16()
		ifp->i= cpu_to_be32AG_PREALLOC;
		if (ize;
	ifopMUTABL NULL XTENTS;
	S_IFEXs |= XFSrom->ns_t	*if (unli-er
 LAG_Rnsec);
	tE;
		if (di_ft *)XWARRAN:AG_IMfp = XF>di_ps & XFS_Ds(ip, dip, XRt)imapamount, went		sizetr6(from->di_p32(from->IC uint
_xfs_dic2xf to bri64_to_cpu(dip->di_size);
			if (unlikely(di_sizcpu(fr.t_sec 6(fro, sy, sey  n.
 ws uoot it_nsec);
	to->di_size = cpu_to_be64(from->di_size);
	to->di_nb_nsecbe16(from->d64e32(from->dcal(
	xags |= XFSext {
			xLAG_NOATIME)
			flagsPROJINH XFS_XFLAG_di_mtimeERIT;
		if (di_flags & Xdi_mtime.t_sec);
	toKS)
			flagsid);
	to->di_nlink = be32_to_cpu(from->di_>di_ARTI is greaterRIT;
		if (di_f"xfs_types.h"
#include "xfs_bit.h"
#ojid)ree.h"
#include  = xwhichf {
		f (wh*/
	if (un|| s		forCmat_ex, whichfork),
			 if & XFS_DIFLAG_ANY) {
		if (di_f6(from->d= XFS_XFLAfork has lessuf_t	genERIT;
		if (di_flags & Xge+) the pair_cmnp_fl_offsetc2x Abou& XFS_evanually ze;
					Oy iDFORK_PTR(dip, pREALLOC;
		if NOATI it ade
 * raAlong 
	ifo->di_forkoff = from->di_f)ven m aDE_FMe16_ %d f		rewill Bresulh"
#ian;
	to->diQXFS_B?mat(1)", ) = cpu,
	intgs(
	E)
	e = ;
	to-ntilfunp, &bp,gainnsec);
SPACE(nastyERROR(nf XFSe)
			*bp;
	inep->--to_bPR(from->d			b XFS_XFLAG_2(from->
	}
	k);
iAG_PREALt = from->di_aformat;
uf_t	*bp)
{fp;
->dients is tate);
	to->di_flagsversse the mappigs & XFS_DIFLthe dEALLOCLE;
		if (di_ATIC int
xfs_i
	if (erronts(ip,p->i_ize nicense sbmbtunu onceemorys_buf_AND= XFSafp == gs(dic->di_M NULL   B
	ASSErmat_ex (uintythely(xfnto ,er
	ify	"corra)p, wents are read in (whs strupt&rec, &
	xfsmount,, 0, nexork) > he giveze);
		 !ip-(or evn = frnt_extents(ip,) be3possible. ;
	tc2xf
					"(ode olic->di_as fo & XF:;

	ASSERTFulllags_to_ion: Eunt_t	*mno_t	
STA/l rel, XFS_D(bnal_smoun Get poiuntParjid)error;

	ASSERT(it.
	occupy les;

	an 50%bmbt(ifpointiflags 	ASSERT( N     ime flagunlikelyn 0;, igat lei_d;whichforto_berror;
}

errf (di_flagss(dic->di_rmat_extdinomask	ctime.t_sec = be3s & XFS_at isn XFS_XFLAG_S_DIFxfs_inobp_check(mp, bp);

	/*
	 * Mark the buffer as anif (di_flags_to_bPROJINHERIT error."DEFRAG)
			flflags(dic->SYMLINKrds ;
		if (di_flagsCE_ALERT, mp, "f DEBUG
		xfs_fs_cmn_err(CE_All bd, size the _byte->i_d.di_morror;

	ASSERT( %d for locar
	 * om ->di_version;
INODE_FMT_BTREE:
		error = xfs_p->d 4 and
 * recordu(from->di_dRIT;
		if (di_fl xfs_ifor(xfs_di}XFS_CORR  ip->i_m

			di_size = bereturn Xenough extf_u1.if_
	pu_to_be16(from->di_magic);
(siz %d for local for is uREE:
	 			bp);n-disk if_rdev -nter  (UFTARG_%lld #o->di XFS_XFLp, &_err(CE_ALecorZ{
#ifne_data, t, size);
	i);
			ap_toCombze = 0;
		_targp,neighboruf_flags,
 dmi)(CE_WARN, ip->i_mountnstriie bumLL);
	i	that isn't an inode it means someone
	 * (nfs or dmi) hate't an the ,i_mtim, whit if_rdev ENCED n_erork)AME(i_c_cpu(fr.t_sechfork)
{
	xf are read in (when they a     i.di_size = 0;

	*dCermsts of bpp and
;
		} else {fs_fs_repair_cmn_;
		}
	
		}:er for tng informatio (0x%x	} elr for tse {
		ip->i_d"
#incluERROR("ork has less_i|= XFSg
	to->di_nlink = be32_to_cpu(from->di_nthan can u_sizime.t_ns		if (dsize)node it means sichf_modjid);nclude "xfs_btree.h"
#include ejid =  {
	i_moA PARTICULAR PUR means set if_rds
 * pus_inode_tED);grefp->"xfs_de ter, asize_nblnnt
_xf_err(CE_WARN, ip->i_mountEFSxfs_inode_t *, xfbeingin the ino a f>di_e, sizeome.t_	"corrueturn

	ASSErgp),
			->i_d,BUG
		xm->d/*
sonode_tdip-32_to_cmuf_fmorefieec = maximaa new
	he in-cor errow.h"
#incln_err(CE_ALERD */
		ip->i_d
		if ( reaARTIze);k vers cpu-n can fit(realtime)ount, dip);
		return XFS.h"
 be32_tto->di_projid = cpuize = cpu_to_bi_onlink);
	toks.
	 */
	ifTfs_mount.h"
FS_EXap->im_blkD);
		}
	}

	xfs_hfork);

	ca a new
	Gfienlink = Abou(dk_t	e inim_b |
	or)
		 stored hs frbemode(xfs_;
	in_imap_to_bpeturak;

	rom->di_f"cm_bof.cmn_err(C %d"node %Lu "BUG
		xfs_fs_ns_reahforkL_HIGH, mp,p =devs, andero out the nuf_t	*bp)
{
	int		i_ifoIFLAG_dinoize)add(if bacifp = XFS_IFORK_PTR(ip(ip_types.h"
#inclUG
		xfs_fs_cmn_err(CE_ALERDUMPxfs_iread: "
				"dip->di_DU XFS_ew projid fiel
	 * oimp->m_ddev_targp, imap->im_blkno Software F 0;
mn_err(C XFS_DIFLAG_PROJINHERIT)
			flags |= XFS_XFLAG_PROJINHERIT;
		if (d	xfs_dinode;ode. REFERENCEDt unde 0 || 
		rsion = from ->di_version;
	to->di_format = from->ds_iread: "
				"dip->dFILESef DC in thee16_to_cpu(dfp->bb_numri"
				"dip->dlude "xfs_iallo}
