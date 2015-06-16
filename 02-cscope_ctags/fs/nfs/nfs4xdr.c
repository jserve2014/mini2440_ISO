/*
 *  fs/nfs/nfs4xdr.c
 *
 *  Client-side XDR for NFSv4.
 *
 *  Copyright (c) 2002 The Regents of the University of Michigan.
 *  All rights reserved.
 *
 *  Kendrick Smith <kmsmith@umich.edu>
 *  Andy Adamson   <andros@umich.edu>
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the University nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 *  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 *  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 *  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 *  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <linux/param.h>
#include <linux/time.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/in.h>
#include <linux/pagemap.h>
#include <linux/proc_fs.h>
#include <linux/kdev_t.h>
#include <linux/sunrpc/clnt.h>
#include <linux/nfs.h>
#include <linux/nfs4.h>
#include <linux/nfs_fs.h>
#include <linux/nfs_idmap.h>
#include "nfs4_fs.h"

#define NFSDBG_FACILITY		NFSDBG_XDR

/* Mapping from NFS error code to "errno" error code. */
#define errno_NFSERR_IO		EIO

static int nfs4_stat_to_errno(int);

/* NFSv4 COMPOUND tags are only wanted for debugging purposes */
#ifdef DEBUG
#define NFS4_MAXTAGLEN		20
#else
#define NFS4_MAXTAGLEN		0
#endif

/* lock,open owner id:
 * we currently use size 2 (u64) out of (NFS4_OPAQUE_LIMIT  >> 2)
 */
#define open_owner_id_maxsz	(1 + 4)
#define lock_owner_id_maxsz	(1 + 4)
#define decode_lockowner_maxsz	(1 + XDR_QUADLEN(IDMAP_NAMESZ))
#define compound_encode_hdr_maxsz	(3 + (NFS4_MAXTAGLEN >> 2))
#define compound_decode_hdr_maxsz	(3 + (NFS4_MAXTAGLEN >> 2))
#define op_encode_hdr_maxsz	(1)
#define op_decode_hdr_maxsz	(2)
#define encode_stateid_maxsz	(XDR_QUADLEN(NFS4_STATEID_SIZE))
#define decode_stateid_maxsz	(XDR_QUADLEN(NFS4_STATEID_SIZE))
#define encode_verifier_maxsz	(XDR_QUADLEN(NFS4_VERIFIER_SIZE))
#define decode_verifier_maxsz	(XDR_QUADLEN(NFS4_VERIFIER_SIZE))
#define encode_putfh_maxsz	(op_encode_hdr_maxsz + 1 + \
				(NFS4_FHSIZE >> 2))
#define decode_putfh_maxsz	(op_decode_hdr_maxsz)
#define encode_putrootfh_maxsz	(op_encode_hdr_maxsz)
#define decode_putrootfh_maxsz	(op_decode_hdr_maxsz)
#define encode_getfh_maxsz      (op_encode_hdr_maxsz)
#define decode_getfh_maxsz      (op_decode_hdr_maxsz + 1 + \
				((3+NFS4_FHSIZE) >> 2))
#define nfs4_fattr_bitmap_maxsz 3
#define encode_getattr_maxsz    (op_encode_hdr_maxsz + nfs4_fattr_bitmap_maxsz)
#define nfs4_name_maxsz		(1 + ((3 + NFS4_MAXNAMLEN) >> 2))
#define nfs4_path_maxsz		(1 + ((3 + NFS4_MAXPATHLEN) >> 2))
#define nfs4_owner_maxsz	(1 + XDR_QUADLEN(IDMAP_NAMESZ))
#define nfs4_group_maxsz	(1 + XDR_QUADLEN(IDMAP_NAMESZ))
/* This is based on getfattr, which uses the most attributes: */
#define nfs4_fattr_value_maxsz	(1 + (1 + 2 + 2 + 4 + 2 + 1 + 1 + 2 + 2 + \
				3 + 3 + 3 + nfs4_owner_maxsz + nfs4_group_maxsz))
#define nfs4_fattr_maxsz	(nfs4_fattr_bitmap_maxsz + \
				nfs4_fattr_value_maxsz)
#define decode_getattr_maxsz    (op_decode_hdr_maxsz + nfs4_fattr_maxsz)
#define encode_attrs_maxsz	(nfs4_fattr_bitmap_maxsz + \
				 1 + 2 + 1 + \
				nfs4_owner_maxsz + \
				nfs4_group_maxsz + \
				4 + 4)
#define encode_savefh_maxsz     (op_encode_hdr_maxsz)
#define decode_savefh_maxsz     (op_decode_hdr_maxsz)
#define encode_restorefh_maxsz  (op_encode_hdr_maxsz)
#define decode_restorefh_maxsz  (op_decode_hdr_maxsz)
#define encode_fsinfo_maxsz	(encode_getattr_maxsz)
#define decode_fsinfo_maxsz	(op_decode_hdr_maxsz + 11)
#define encode_renew_maxsz	(op_encode_hdr_maxsz + 3)
#define decode_renew_maxsz	(op_decode_hdr_maxsz)
#define encode_setclientid_maxsz \
				(op_encode_hdr_maxsz + \
				XDR_QUADLEN(NFS4_VERIFIER_SIZE) + \
				XDR_QUADLEN(NFS4_SETCLIENTID_NAMELEN) + \
				1 /* sc_prog */ + \
				XDR_QUADLEN(RPCBIND_MAXNETIDLEN) + \
				XDR_QUADLEN(RPCBIND_MAXUADDRLEN) + \
				1) /* sc_cb_ident */
#define decode_setclientid_maxsz \
				(op_decode_hdr_maxsz + \
				2 + \
				1024) /* large value for CLID_INUSE */
#define encode_setclientid_confirm_maxsz \
				(op_encode_hdr_maxsz + \
				3 + (NFS4_VERIFIER_SIZE >> 2))
#define decode_setclientid_confirm_maxsz \
				(op_decode_hdr_maxsz)
#define encode_lookup_maxsz	(op_encode_hdr_maxsz + nfs4_name_maxsz)
#define decode_lookup_maxsz	(op_decode_hdr_maxsz)
#define encode_share_access_maxsz \
				(2)
#define encode_createmode_maxsz	(1 + encode_attrs_maxsz)
#define encode_opentype_maxsz	(1 + encode_createmode_maxsz)
#define encode_claim_null_maxsz	(1 + nfs4_name_maxsz)
#define encode_open_maxsz	(op_encode_hdr_maxsz + \
				2 + encode_share_access_maxsz + 2 + \
				open_owner_id_maxsz + \
				encode_opentype_maxsz + \
				encode_claim_null_maxsz)
#define decode_ace_maxsz	(3 + nfs4_owner_maxsz)
#define decode_delegation_maxsz	(1 + decode_stateid_maxsz + 1 + \
				decode_ace_maxsz)
#define decode_change_info_maxsz	(5)
#define decode_open_maxsz	(op_decode_hdr_maxsz + \
				decode_stateid_maxsz + \
				decode_change_info_maxsz + 1 + \
				nfs4_fattr_bitmap_maxsz + \
				decode_delegation_maxsz)
#define encode_open_confirm_maxsz \
				(op_encode_hdr_maxsz + \
				 encode_stateid_maxsz + 1)
#define decode_open_confirm_maxsz \
				(op_decode_hdr_maxsz + \
				 decode_stateid_maxsz)
#define encode_open_downgrade_maxsz \
				(op_encode_hdr_maxsz + \
				 encode_stateid_maxsz + 1 + \
				 encode_share_access_maxsz)
#define decode_open_downgrade_maxsz \
				(op_decode_hdr_maxsz + \
				 decode_stateid_maxsz)
#define encode_close_maxsz	(op_encode_hdr_maxsz + \
				 1 + encode_stateid_maxsz)
#define decode_close_maxsz	(op_decode_hdr_maxsz + \
				 decode_stateid_maxsz)
#define encode_setattr_maxsz	(op_encode_hdr_maxsz + \
				 encode_stateid_maxsz + \
				 encode_attrs_maxsz)
#define decode_setattr_maxsz	(op_decode_hdr_maxsz + \
				 nfs4_fattr_bitmap_maxsz)
#define encode_read_maxsz	(op_encode_hdr_maxsz + \
				 encode_stateid_maxsz + 3)
#define decode_read_maxsz	(op_decode_hdr_maxsz + 2)
#define encode_readdir_maxsz	(op_encode_hdr_maxsz + \
				 2 + encode_verifier_maxsz + 5)
#define decode_readdir_maxsz	(op_decode_hdr_maxsz + \
				 decode_verifier_maxsz)
#define encode_readlink_maxsz	(op_encode_hdr_maxsz)
#define decode_readlink_maxsz	(op_decode_hdr_maxsz + 1)
#define encode_write_maxsz	(op_encode_hdr_maxsz + \
				 encode_stateid_maxsz + 4)
#define decode_write_maxsz	(op_decode_hdr_maxsz + \
				 2 + decode_verifier_maxsz)
#define encode_commit_maxsz	(op_encode_hdr_maxsz + 3)
#define decode_commit_maxsz	(op_decode_hdr_maxsz + \
				 decode_verifier_maxsz)
#define encode_remove_maxsz	(op_encode_hdr_maxsz + \
				nfs4_name_maxsz)
#define decode_remove_maxsz	(op_decode_hdr_maxsz + \
				 decode_change_info_maxsz)
#define encode_rename_maxsz	(op_encode_hdr_maxsz + \
				2 * nfs4_name_maxsz)
#define decode_rename_maxsz	(op_decode_hdr_maxsz + \
				 decode_change_info_maxsz + \
				 decode_change_info_maxsz)
#define encode_link_maxsz	(op_encode_hdr_maxsz + \
				nfs4_name_maxsz)
#define decode_link_maxsz	(op_decode_hdr_maxsz + decode_change_info_maxsz)
#define encode_lock_maxsz	(op_encode_hdr_maxsz + \
				 7 + \
				 1 + encode_stateid_maxsz + 8)
#define decode_lock_denied_maxsz \
				(8 + decode_lockowner_maxsz)
#define decode_lock_maxsz	(op_decode_hdr_maxsz + \
				 decode_lock_denied_maxsz)
#define encode_lockt_maxsz	(op_encode_hdr_maxsz + 12)
#define decode_lockt_maxsz	(op_decode_hdr_maxsz + \
				 decode_lock_denied_maxsz)
#define encode_locku_maxsz	(op_encode_hdr_maxsz + 3 + \
				 encode_stateid_maxsz + \
				 4)
#define decode_locku_maxsz	(op_decode_hdr_maxsz + \
				 decode_stateid_maxsz)
#define encode_access_maxsz	(op_encode_hdr_maxsz + 1)
#define decode_access_maxsz	(op_decode_hdr_maxsz + 2)
#define encode_symlink_maxsz	(op_encode_hdr_maxsz + \
				1 + nfs4_name_maxsz + \
				1 + \
				nfs4_fattr_maxsz)
#define decode_symlink_maxsz	(op_decode_hdr_maxsz + 8)
#define encode_create_maxsz	(op_encode_hdr_maxsz + \
				1 + 2 + nfs4_name_maxsz + \
				encode_attrs_maxsz)
#define decode_create_maxsz	(op_decode_hdr_maxsz + \
				decode_change_info_maxsz + \
				nfs4_fattr_bitmap_maxsz)
#define encode_statfs_maxsz	(encode_getattr_maxsz)
#define decode_statfs_maxsz	(decode_getattr_maxsz)
#define encode_delegreturn_maxsz (op_encode_hdr_maxsz + 4)
#define decode_delegreturn_maxsz (op_decode_hdr_maxsz)
#define encode_getacl_maxsz	(encode_getattr_maxsz)
#define decode_getacl_maxsz	(op_decode_hdr_maxsz + \
				 nfs4_fattr_bitmap_maxsz + 1)
#define encode_setacl_maxsz	(op_encode_hdr_maxsz + \
				 encode_stateid_maxsz + 3)
#define decode_setacl_maxsz	(decode_setattr_maxsz)
#define encode_fs_locations_maxsz \
				(encode_getattr_maxsz)
#define decode_fs_locations_maxsz \
				(0)

#if defined(CONFIG_NFS_V4_1)
#define NFS4_MAX_MACHINE_NAME_LEN (64)

#define encode_exchange_id_maxsz (op_encode_hdr_maxsz + \
				encode_verifier_maxsz + \
				1 /* co_ownerid.len */ + \
				XDR_QUADLEN(NFS4_EXCHANGE_ID_LEN) + \
				1 /* flags */ + \
				1 /* spa_how */ + \
				0 /* SP4_NONE (for now) */ + \
				1 /* zero implemetation id array */)
#define decode_exchange_id_maxsz (op_decode_hdr_maxsz + \
				2 /* eir_clientid */ + \
				1 /* eir_sequenceid */ + \
				1 /* eir_flags */ + \
				1 /* spr_how */ + \
				0 /* SP4_NONE (for now) */ + \
				2 /* eir_server_owner.so_minor_id */ + \
				/* eir_server_owner.so_major_id<> */ \
				XDR_QUADLEN(NFS4_OPAQUE_LIMIT) + 1 + \
				/* eir_server_scope<> */ \
				XDR_QUADLEN(NFS4_OPAQUE_LIMIT) + 1 + \
				1 /* eir_server_impl_id array length */ + \
				0 /* ignored eir_server_impl_id contents */)
#define encode_channel_attrs_maxsz  (6 + 1 /* ca_rdma_ird.len (0) */)
#define decode_channel_attrs_maxsz  (6 + \
				     1 /* ca_rdma_ird.len */ + \
				     1 /* ca_rdma_ird */)
#define encode_create_session_maxsz  (op_encode_hdr_maxsz + \
				     2 /* csa_clientid */ + \
				     1 /* csa_sequence */ + \
				     1 /* csa_flags */ + \
				     encode_channel_attrs_maxsz + \
				     encode_channel_attrs_maxsz + \
				     1 /* csa_cb_program */ + \
				     1 /* csa_sec_parms.len (1) */ + \
				     1 /* cb_secflavor (AUTH_SYS) */ + \
				     1 /* stamp */ + \
				     1 /* machinename.len */ + \
				     XDR_QUADLEN(NFS4_MAX_MACHINE_NAME_LEN) + \
				     1 /* uid */ + \
				     1 /* gid */ + \
				     1 /* gids.len (0) */)
#define decode_create_session_maxsz  (op_decode_hdr_maxsz +	\
				     XDR_QUADLEN(NFS4_MAX_SESSIONID_LEN) + \
				     1 /* csr_sequence */ + \
				     1 /* csr_flags */ + \
				     decode_channel_attrs_maxsz + \
				     decode_channel_attrs_maxsz)
#define encode_destroy_session_maxsz    (op_encode_hdr_maxsz + 4)
#define decode_destroy_session_maxsz    (op_decode_hdr_maxsz)
#define encode_sequence_maxsz	(op_encode_hdr_maxsz + \
				XDR_QUADLEN(NFS4_MAX_SESSIONID_LEN) + 4)
#define decode_sequence_maxsz	(op_decode_hdr_maxsz + \
				XDR_QUADLEN(NFS4_MAX_SESSIONID_LEN) + 5)
#else /* CONFIG_NFS_V4_1 */
#define encode_sequence_maxsz	0
#define decode_sequence_maxsz	0
#endif /* CONFIG_NFS_V4_1 */

#define NFS4_enc_compound_sz	(1024)  /* XXX: large enough? */
#define NFS4_dec_compound_sz	(1024)  /* XXX: large enough? */
#define NFS4_enc_read_sz	(compound_encode_hdr_maxsz + \
				encode_sequence_maxsz + \
				encode_putfh_maxsz + \
				encode_read_maxsz)
#define NFS4_dec_read_sz	(compound_decode_hdr_maxsz + \
				decode_sequence_maxsz + \
				decode_putfh_maxsz + \
				decode_read_maxsz)
#define NFS4_enc_readlink_sz	(compound_encode_hdr_maxsz + \
				encode_sequence_maxsz + \
				encode_putfh_maxsz + \
				encode_readlink_maxsz)
#define NFS4_dec_readlink_sz	(compound_decode_hdr_maxsz + \
				decode_sequence_maxsz + \
				decode_putfh_maxsz + \
				decode_readlink_maxsz)
#define NFS4_enc_readdir_sz	(compound_encode_hdr_maxsz + \
				encode_sequence_maxsz + \
				encode_putfh_maxsz + \
				encode_readdir_maxsz)
#define NFS4_dec_readdir_sz	(compound_decode_hdr_maxsz + \
				decode_sequence_maxsz + \
				decode_putfh_maxsz + \
				decode_readdir_maxsz)
#define NFS4_enc_write_sz	(compound_encode_hdr_maxsz + \
				encode_sequence_maxsz + \
				encode_putfh_maxsz + \
				encode_write_maxsz + \
				encode_getattr_maxsz)
#define NFS4_dec_write_sz	(compound_decode_hdr_maxsz + \
				decode_sequence_maxsz + \
				decode_putfh_maxsz + \
				decode_write_maxsz + \
				decode_getattr_maxsz)
#define NFS4_enc_commit_sz	(compound_encode_hdr_maxsz + \
				encode_sequence_maxsz + \
				encode_putfh_maxsz + \
				encode_commit_maxsz + \
				encode_getattr_maxsz)
#define NFS4_dec_commit_sz	(compound_decode_hdr_maxsz + \
				decode_sequence_maxsz + \
				decode_putfh_maxsz + \
				decode_commit_maxsz + \
				decode_getattr_maxsz)
#define NFS4_enc_open_sz        (compound_encode_hdr_maxsz + \
				encode_sequence_maxsz + \
				encode_putfh_maxsz + \
				encode_savefh_maxsz + \
				encode_open_maxsz + \
				encode_getfh_maxsz + \
				encode_getattr_maxsz + \
				encode_restorefh_maxsz + \
				encode_getattr_maxsz)
#define NFS4_dec_open_sz        (compound_decode_hdr_maxsz + \
				decode_sequence_maxsz + \
				decode_putfh_maxsz + \
				decode_savefh_maxsz + \
				decode_open_maxsz + \
				decode_getfh_maxsz + \
				decode_getattr_maxsz + \
				decode_restorefh_maxsz + \
				decode_getattr_maxsz)
#define NFS4_enc_open_confirm_sz \
				(compound_encode_hdr_maxsz + \
				 encode_putfh_maxsz + \
				 encode_open_confirm_maxsz)
#define NFS4_dec_open_confirm_sz \
				(compound_decode_hdr_maxsz + \
				 decode_putfh_maxsz + \
				 decode_open_confirm_maxsz)
#define NFS4_enc_open_noattr_sz	(compound_encode_hdr_maxsz + \
					encode_sequence_maxsz + \
					encode_putfh_maxsz + \
					encode_open_maxsz + \
					encode_getattr_maxsz)
#define NFS4_dec_open_noattr_sz	(compound_decode_hdr_maxsz + \
					decode_sequence_maxsz + \
					decode_putfh_maxsz + \
					decode_open_maxsz + \
					decode_getattr_maxsz)
#define NFS4_enc_open_downgrade_sz \
				(compound_encode_hdr_maxsz + \
				 encode_sequence_maxsz + \
				 encode_putfh_maxsz + \
				 encode_open_downgrade_maxsz + \
				 encode_getattr_maxsz)
#define NFS4_dec_open_downgrade_sz \
				(compound_decode_hdr_maxsz + \
				 decode_sequence_maxsz + \
				 decode_putfh_maxsz + \
				 decode_open_downgrade_maxsz + \
				 decode_getattr_maxsz)
#define NFS4_enc_close_sz	(compound_encode_hdr_maxsz + \
				 encode_sequence_maxsz + \
				 encode_putfh_maxsz + \
				 encode_close_maxsz + \
				 encode_getattr_maxsz)
#define NFS4_dec_close_sz	(compound_decode_hdr_maxsz + \
				 decode_sequence_maxsz + \
				 decode_putfh_maxsz + \
				 decode_close_maxsz + \
				 decode_getattr_maxsz)
#define NFS4_enc_setattr_sz	(compound_encode_hdr_maxsz + \
				 encode_sequence_maxsz + \
				 encode_putfh_maxsz + \
				 encode_setattr_maxsz + \
				 encode_getattr_maxsz)
#define NFS4_dec_setattr_sz	(compound_decode_hdr_maxsz + \
				 decode_sequence_maxsz + \
				 decode_putfh_maxsz + \
				 decode_setattr_maxsz + \
				 decode_getattr_maxsz)
#define NFS4_enc_fsinfo_sz	(compound_encode_hdr_maxsz + \
				encode_sequence_maxsz + \
				encode_putfh_maxsz + \
				encode_fsinfo_maxsz)
#define NFS4_dec_fsinfo_sz	(compound_decode_hdr_maxsz + \
				decode_sequence_maxsz + \
				decode_putfh_maxsz + \
				decode_fsinfo_maxsz)
#define NFS4_enc_renew_sz	(compound_encode_hdr_maxsz + \
				encode_renew_maxsz)
#define NFS4_dec_renew_sz	(compound_decode_hdr_maxsz + \
				decode_renew_maxsz)
#define NFS4_enc_setclientid_sz	(compound_encode_hdr_maxsz + \
				encode_setclientid_maxsz)
#define NFS4_dec_setclientid_sz	(compound_decode_hdr_maxsz + \
				decode_setclientid_maxsz)
#define NFS4_enc_setclientid_confirm_sz \
				(compound_encode_hdr_maxsz + \
				encode_setclientid_confirm_maxsz + \
				encode_putrootfh_maxsz + \
				encode_fsinfo_maxsz)
#define NFS4_dec_setclientid_confirm_sz \
				(compound_decode_hdr_maxsz + \
				decode_setclientid_confirm_maxsz + \
				decode_putrootfh_maxsz + \
				decode_fsinfo_maxsz)
#define NFS4_enc_lock_sz        (compound_encode_hdr_maxsz + \
				encode_sequence_maxsz + \
				encode_putfh_maxsz + \
				encode_lock_maxsz)
#define NFS4_dec_lock_sz        (compound_decode_hdr_maxsz + \
				decode_sequence_maxsz + \
				decode_putfh_maxsz + \
				decode_lock_maxsz)
#define NFS4_enc_lockt_sz       (compound_encode_hdr_maxsz + \
				encode_sequence_maxsz + \
				encode_putfh_maxsz + \
				encode_lockt_maxsz)
#define NFS4_dec_lockt_sz       (compound_decode_hdr_maxsz + \
				 decode_sequence_maxsz + \
				 decode_putfh_maxsz + \
				 decode_lockt_maxsz)
#define NFS4_enc_locku_sz       (compound_encode_hdr_maxsz + \
				encode_sequence_maxsz + \
				encode_putfh_maxsz + \
				encode_locku_maxsz)
#define NFS4_dec_locku_sz       (compound_decode_hdr_maxsz + \
				decode_sequence_maxsz + \
				decode_putfh_maxsz + \
				decode_locku_maxsz)
#define NFS4_enc_access_sz	(compound_encode_hdr_maxsz + \
				encode_sequence_maxsz + \
				encode_putfh_maxsz + \
				encode_access_maxsz + \
				encode_getattr_maxsz)
#define NFS4_dec_access_sz	(compound_decode_hdr_maxsz + \
				decode_sequence_maxsz + \
				decode_putfh_maxsz + \
				decode_access_maxsz + \
				decode_getattr_maxsz)
#define NFS4_enc_getattr_sz	(compound_encode_hdr_maxsz + \
				encode_sequence_maxsz + \
				encode_putfh_maxsz + \
				encode_getattr_maxsz)
#define NFS4_dec_getattr_sz	(compound_decode_hdr_maxsz + \
				decode_sequence_maxsz + \
				decode_putfh_maxsz + \
				decode_getattr_maxsz)
#define NFS4_enc_lookup_sz	(compound_encode_hdr_maxsz + \
				encode_sequence_maxsz + \
				encode_putfh_maxsz + \
				encode_lookup_maxsz + \
				encode_getattr_maxsz + \
				encode_getfh_maxsz)
#define NFS4_dec_lookup_sz	(compound_decode_hdr_maxsz + \
				decode_sequence_maxsz + \
				decode_putfh_maxsz + \
				decode_lookup_maxsz + \
				decode_getattr_maxsz + \
				decode_getfh_maxsz)
#define NFS4_enc_lookup_root_sz (compound_encode_hdr_maxsz + \
				encode_sequence_maxsz + \
				encode_putrootfh_maxsz + \
				encode_getattr_maxsz + \
				encode_getfh_maxsz)
#define NFS4_dec_lookup_root_sz (compound_decode_hdr_maxsz + \
				decode_sequence_maxsz + \
				decode_putrootfh_maxsz + \
				decode_getattr_maxsz + \
				decode_getfh_maxsz)
#define NFS4_enc_remove_sz	(compound_encode_hdr_maxsz + \
				encode_sequence_maxsz + \
				encode_putfh_maxsz + \
				encode_remove_maxsz + \
				encode_getattr_maxsz)
#define NFS4_dec_remove_sz	(compound_decode_hdr_maxsz + \
				decode_sequence_maxsz + \
				decode_putfh_maxsz + \
				decode_remove_maxsz + \
				decode_getattr_maxsz)
#define NFS4_enc_rename_sz	(compound_encode_hdr_maxsz + \
				encode_sequence_maxsz + \
				encode_putfh_maxsz + \
				encode_savefh_maxsz + \
				encode_putfh_maxsz + \
				encode_rename_maxsz + \
				encode_getattr_maxsz + \
				encode_restorefh_maxsz + \
				encode_getattr_maxsz)
#define NFS4_dec_rename_sz	(compound_decode_hdr_maxsz + \
				decode_sequence_maxsz + \
				decode_putfh_maxsz + \
				decode_savefh_maxsz + \
				decode_putfh_maxsz + \
				decode_rename_maxsz + \
				decode_getattr_maxsz + \
				decode_restorefh_maxsz + \
				decode_getattr_maxsz)
#define NFS4_enc_link_sz	(compound_encode_hdr_maxsz + \
				encode_sequence_maxsz + \
				encode_putfh_maxsz + \
				encode_savefh_maxsz + \
				encode_putfh_maxsz + \
				encode_link_maxsz + \
				decode_getattr_maxsz + \
				encode_restorefh_maxsz + \
				decode_getattr_maxsz)
#define NFS4_dec_link_sz	(compound_decode_hdr_maxsz + \
				decode_sequence_maxsz + \
				decode_putfh_maxsz + \
				decode_savefh_maxsz + \
				decode_putfh_maxsz + \
				decode_link_maxsz + \
				decode_getattr_maxsz + \
				decode_restorefh_maxsz + \
				decode_getattr_maxsz)
#define NFS4_enc_symlink_sz	(compound_encode_hdr_maxsz + \
				encode_sequence_maxsz + \
				encode_putfh_maxsz + \
				encode_symlink_maxsz + \
				encode_getattr_maxsz + \
				encode_getfh_maxsz)
#define NFS4_dec_symlink_sz	(compound_decode_hdr_maxsz + \
				decode_sequence_maxsz + \
				decode_putfh_maxsz + \
				decode_symlink_maxsz + \
				decode_getattr_maxsz + \
				decode_getfh_maxsz)
#define NFS4_enc_create_sz	(compound_encode_hdr_maxsz + \
				encode_sequence_maxsz + \
				encode_putfh_maxsz + \
				encode_savefh_maxsz + \
				encode_create_maxsz + \
				encode_getfh_maxsz + \
				encode_getattr_maxsz + \
				encode_restorefh_maxsz + \
				encode_getattr_maxsz)
#define NFS4_dec_create_sz	(compound_decode_hdr_maxsz + \
				decode_sequence_maxsz + \
				decode_putfh_maxsz + \
				decode_savefh_maxsz + \
				decode_create_maxsz + \
				decode_getfh_maxsz + \
				decode_getattr_maxsz + \
				decode_restorefh_maxsz + \
				decode_getattr_maxsz)
#define NFS4_enc_pathconf_sz	(compound_encode_hdr_maxsz + \
				encode_sequence_maxsz + \
				encode_putfh_maxsz + \
				encode_getattr_maxsz)
#define NFS4_dec_pathconf_sz	(compound_decode_hdr_maxsz + \
				decode_sequence_maxsz + \
				decode_putfh_maxsz + \
				decode_getattr_maxsz)
#define NFS4_enc_statfs_sz	(compound_encode_hdr_maxsz + \
				encode_sequence_maxsz + \
				encode_putfh_maxsz + \
				encode_statfs_maxsz)
#define NFS4_dec_statfs_sz	(compound_decode_hdr_maxsz + \
				decode_sequence_maxsz + \
				decode_putfh_maxsz + \
				decode_statfs_maxsz)
#define NFS4_enc_server_caps_sz (compound_encode_hdr_maxsz + \
				encode_sequence_maxsz + \
				encode_putfh_maxsz + \
				encode_getattr_maxsz)
#define NFS4_dec_server_caps_sz (compound_decode_hdr_maxsz + \
				decode_sequence_maxsz + \
				decode_putfh_maxsz + \
				decode_getattr_maxsz)
#define NFS4_enc_delegreturn_sz	(compound_encode_hdr_maxsz + \
				encode_sequence_maxsz + \
				encode_putfh_maxsz + \
				encode_delegreturn_maxsz + \
				encode_getattr_maxsz)
#define NFS4_dec_delegreturn_sz (compound_decode_hdr_maxsz + \
				decode_sequence_maxsz + \
				decode_delegreturn_maxsz + \
				decode_getattr_maxsz)
#define NFS4_enc_getacl_sz	(compound_encode_hdr_maxsz + \
				encode_sequence_maxsz + \
				encode_putfh_maxsz + \
				encode_getacl_maxsz)
#define NFS4_dec_getacl_sz	(compound_decode_hdr_maxsz + \
				decode_sequence_maxsz + \
				decode_putfh_maxsz + \
				decode_getacl_maxsz)
#define NFS4_enc_setacl_sz	(compound_encode_hdr_maxsz + \
				encode_sequence_maxsz + \
				encode_putfh_maxsz + \
				encode_setacl_maxsz)
#define NFS4_dec_setacl_sz	(compound_decode_hdr_maxsz + \
				decode_sequence_maxsz + \
				decode_putfh_maxsz + \
				decode_setacl_maxsz)
#define NFS4_enc_fs_locations_sz \
				(compound_encode_hdr_maxsz + \
				 encode_sequence_maxsz + \
				 encode_putfh_maxsz + \
				 encode_lookup_maxsz + \
				 encode_fs_locations_maxsz)
#define NFS4_dec_fs_locations_sz \
				(compound_decode_hdr_maxsz + \
				 decode_sequence_maxsz + \
				 decode_putfh_maxsz + \
				 decode_lookup_maxsz + \
				 decode_fs_locations_maxsz)
#if defined(CONFIG_NFS_V4_1)
#define NFS4_enc_exchange_id_sz \
				(compound_encode_hdr_maxsz + \
				 encode_exchange_id_maxsz)
#define NFS4_dec_exchange_id_sz \
				(compound_decode_hdr_maxsz + \
				 decode_exchange_id_maxsz)
#define NFS4_enc_create_session_sz \
				(compound_encode_hdr_maxsz + \
				 encode_create_session_maxsz)
#define NFS4_dec_create_session_sz \
				(compound_decode_hdr_maxsz + \
				 decode_create_session_maxsz)
#define NFS4_enc_destroy_session_sz	(compound_encode_hdr_maxsz + \
					 encode_destroy_session_maxsz)
#define NFS4_dec_destroy_session_sz	(compound_decode_hdr_maxsz + \
					 decode_destroy_session_maxsz)
#define NFS4_enc_sequence_sz \
				(compound_decode_hdr_maxsz + \
				 encode_sequence_maxsz)
#define NFS4_dec_sequence_sz \
				(compound_decode_hdr_maxsz + \
				 decode_sequence_maxsz)
#define NFS4_enc_get_lease_time_sz	(compound_encode_hdr_maxsz + \
					 encode_sequence_maxsz + \
					 encode_putrootfh_maxsz + \
					 encode_fsinfo_maxsz)
#define NFS4_dec_get_lease_time_sz	(compound_decode_hdr_maxsz + \
					 decode_sequence_maxsz + \
					 decode_putrootfh_maxsz + \
					 decode_fsinfo_maxsz)
#endif /* CONFIG_NFS_V4_1 */

static const umode_t nfs_type2fmt[] = {
	[NF4BAD] = 0,
	[NF4REG] = S_IFREG,
	[NF4DIR] = S_IFDIR,
	[NF4BLK] = S_IFBLK,
	[NF4CHR] = S_IFCHR,
	[NF4LNK] = S_IFLNK,
	[NF4SOCK] = S_IFSOCK,
	[NF4FIFO] = S_IFIFO,
	[NF4ATTRDIR] = 0,
	[NF4NAMEDATTR] = 0,
};

struct compound_hdr {
	int32_t		status;
	uint32_t	nops;
	__be32 *	nops_p;
	uint32_t	taglen;
	char *		tag;
	uint32_t	replen;		/* expected reply words */
	u32		minorversion;
};

static __be32 *reserve_space(struct xdr_stream *xdr, size_t nbytes)
{
	__be32 *p = xdr_reserve_space(xdr, nbytes);
	BUG_ON(!p);
	return p;
}

static void encode_string(struct xdr_stream *xdr, unsigned int len, const char *str)
{
	__be32 *p;

	p = xdr_reserve_space(xdr, 4 + len);
	BUG_ON(p == NULL);
	xdr_encode_opaque(p, str, len);
}

static void encode_compound_hdr(struct xdr_stream *xdr,
				struct rpc_rqst *req,
				struct compound_hdr *hdr)
{
	__be32 *p;
	struct rpc_auth *auth = req->rq_task->tk_msg.rpc_cred->cr_auth;

	/* initialize running count of expected bytes in reply.
	 * NOTE: the replied tag SHOULD be the same is the one sent,
	 * but this is not required as a MUST for the server to do so. */
	hdr->replen = RPC_REPHDRSIZE + auth->au_rslack + 3 + hdr->taglen;

	dprintk("encode_compound: tag=%.*s\n", (int)hdr->taglen, hdr->tag);
	BUG_ON(hdr->taglen > NFS4_MAXTAGLEN);
	p = reserve_space(xdr, 4 + hdr->taglen + 8);
	p = xdr_encode_opaque(p, hdr->tag, hdr->taglen);
	*p++ = cpu_to_be32(hdr->minorversion);
	hdr->nops_p = p;
	*p = cpu_to_be32(hdr->nops);
}

static void encode_nops(struct compound_hdr *hdr)
{
	BUG_ON(hdr->nops > NFS4_MAX_OPS);
	*hdr->nops_p = htonl(hdr->nops);
}

static void encode_nfs4_verifier(struct xdr_stream *xdr, const nfs4_verifier *verf)
{
	__be32 *p;

	p = xdr_reserve_space(xdr, NFS4_VERIFIER_SIZE);
	BUG_ON(p == NULL);
	xdr_encode_opaque_fixed(p, verf->data, NFS4_VERIFIER_SIZE);
}

static void encode_attrs(struct xdr_stream *xdr, const struct iattr *iap, const struct nfs_server *server)
{
	char owner_name[IDMAP_NAMESZ];
	char owner_group[IDMAP_NAMESZ];
	int owner_namelen = 0;
	int owner_grouplen = 0;
	__be32 *p;
	__be32 *q;
	int len;
	uint32_t bmval0 = 0;
	uint32_t bmval1 = 0;

	/*
	 * We reserve enough space to write the entire attribute buffer at once.
	 * In the worst-case, this would be
	 *   12(bitmap) + 4(attrlen) + 8(size) + 4(mode) + 4(atime) + 4(mtime)
	 *          = 36 bytes, plus any contribution from variable-length fields
	 *            such as owner/group.
	 */
	len = 16;

	/* Sigh */
	if (iap->ia_valid & ATTR_SIZE)
		len += 8;
	if (iap->ia_valid & ATTR_MODE)
		len += 4;
	if (iap->ia_valid & ATTR_UID) {
		owner_namelen = nfs_map_uid_to_name(server->nfs_client, iap->ia_uid, owner_name);
		if (owner_namelen < 0) {
			dprintk("nfs: couldn't resolve uid %d to string\n",
					iap->ia_uid);
			/* XXX */
			strcpy(owner_name, "nobody");
			owner_namelen = sizeof("nobody") - 1;
			/* goto out; */
		}
		len += 4 + (XDR_QUADLEN(owner_namelen) << 2);
	}
	if (iap->ia_valid & ATTR_GID) {
		owner_grouplen = nfs_map_gid_to_group(server->nfs_client, iap->ia_gid, owner_group);
		if (owner_grouplen < 0) {
			dprintk("nfs: couldn't resolve gid %d to string\n",
					iap->ia_gid);
			strcpy(owner_group, "nobody");
			owner_grouplen = sizeof("nobody") - 1;
			/* goto out; */
		}
		len += 4 + (XDR_QUADLEN(owner_grouplen) << 2);
	}
	if (iap->ia_valid & ATTR_ATIME_SET)
		len += 16;
	else if (iap->ia_valid & ATTR_ATIME)
		len += 4;
	if (iap->ia_valid & ATTR_MTIME_SET)
		len += 16;
	else if (iap->ia_valid & ATTR_MTIME)
		len += 4;
	p = reserve_space(xdr, len);

	/*
	 * We write the bitmap length now, but leave the bitmap and the attribute
	 * buffer length to be backfilled at the end of this routine.
	 */
	*p++ = cpu_to_be32(2);
	q = p;
	p += 3;

	if (iap->ia_valid & ATTR_SIZE) {
		bmval0 |= FATTR4_WORD0_SIZE;
		p = xdr_encode_hyper(p, iap->ia_size);
	}
	if (iap->ia_valid & ATTR_MODE) {
		bmval1 |= FATTR4_WORD1_MODE;
		*p++ = cpu_to_be32(iap->ia_mode & S_IALLUGO);
	}
	if (iap->ia_valid & ATTR_UID) {
		bmval1 |= FATTR4_WORD1_OWNER;
		p = xdr_encode_opaque(p, owner_name, owner_namelen);
	}
	if (iap->ia_valid & ATTR_GID) {
		bmval1 |= FATTR4_WORD1_OWNER_GROUP;
		p = xdr_encode_opaque(p, owner_group, owner_grouplen);
	}
	if (iap->ia_valid & ATTR_ATIME_SET) {
		bmval1 |= FATTR4_WORD1_TIME_ACCESS_SET;
		*p++ = cpu_to_be32(NFS4_SET_TO_CLIENT_TIME);
		*p++ = cpu_to_be32(0);
		*p++ = cpu_to_be32(iap->ia_mtime.tv_sec);
		*p++ = cpu_to_be32(iap->ia_mtime.tv_nsec);
	}
	else if (iap->ia_valid & ATTR_ATIME) {
		bmval1 |= FATTR4_WORD1_TIME_ACCESS_SET;
		*p++ = cpu_to_be32(NFS4_SET_TO_SERVER_TIME);
	}
	if (iap->ia_valid & ATTR_MTIME_SET) {
		bmval1 |= FATTR4_WORD1_TIME_MODIFY_SET;
		*p++ = cpu_to_be32(NFS4_SET_TO_CLIENT_TIME);
		*p++ = cpu_to_be32(0);
		*p++ = cpu_to_be32(iap->ia_mtime.tv_sec);
		*p++ = cpu_to_be32(iap->ia_mtime.tv_nsec);
	}
	else if (iap->ia_valid & ATTR_MTIME) {
		bmval1 |= FATTR4_WORD1_TIME_MODIFY_SET;
		*p++ = cpu_to_be32(NFS4_SET_TO_SERVER_TIME);
	}

	/*
	 * Now we backfill the bitmap and the attribute buffer length.
	 */
	if (len != ((char *)p - (char *)q) + 4) {
		printk(KERN_ERR "nfs: Attr length error, %u != %Zu\n",
				len, ((char *)p - (char *)q) + 4);
		BUG();
	}
	len = (char *)p - (char *)q - 12;
	*q++ = htonl(bmval0);
	*q++ = htonl(bmval1);
	*q = htonl(len);

/* out: */
}

static void encode_access(struct xdr_stream *xdr, u32 access, struct compound_hdr *hdr)
{
	__be32 *p;

	p = reserve_space(xdr, 8);
	*p++ = cpu_to_be32(OP_ACCESS);
	*p = cpu_to_be32(access);
	hdr->nops++;
	hdr->replen += decode_access_maxsz;
}

static void encode_close(struct xdr_stream *xdr, const struct nfs_closeargs *arg, struct compound_hdr *hdr)
{
	__be32 *p;

	p = reserve_space(xdr, 8+NFS4_STATEID_SIZE);
	*p++ = cpu_to_be32(OP_CLOSE);
	*p++ = cpu_to_be32(arg->seqid->sequence->counter);
	xdr_encode_opaque_fixed(p, arg->stateid->data, NFS4_STATEID_SIZE);
	hdr->nops++;
	hdr->replen += decode_close_maxsz;
}

static void encode_commit(struct xdr_stream *xdr, const struct nfs_writeargs *args, struct compound_hdr *hdr)
{
	__be32 *p;

	p = reserve_space(xdr, 16);
	*p++ = cpu_to_be32(OP_COMMIT);
	p = xdr_encode_hyper(p, args->offset);
	*p = cpu_to_be32(args->count);
	hdr->nops++;
	hdr->replen += decode_commit_maxsz;
}

static void encode_create(struct xdr_stream *xdr, const struct nfs4_create_arg *create, struct compound_hdr *hdr)
{
	__be32 *p;

	p = reserve_space(xdr, 8);
	*p++ = cpu_to_be32(OP_CREATE);
	*p = cpu_to_be32(create->ftype);

	switch (create->ftype) {
	case NF4LNK:
		p = reserve_space(xdr, 4);
		*p = cpu_to_be32(create->u.symlink.len);
		xdr_write_pages(xdr, create->u.symlink.pages, 0, create->u.symlink.len);
		break;

	case NF4BLK: case NF4CHR:
		p = reserve_space(xdr, 8);
		*p++ = cpu_to_be32(create->u.device.specdata1);
		*p = cpu_to_be32(create->u.device.specdata2);
		break;

	default:
		break;
	}

	encode_string(xdr, create->name->len, create->name->name);
	hdr->nops++;
	hdr->replen += decode_create_maxsz;

	encode_attrs(xdr, create->attrs, create->server);
}

static void encode_getattr_one(struct xdr_stream *xdr, uint32_t bitmap, struct compound_hdr *hdr)
{
	__be32 *p;

	p = reserve_space(xdr, 12);
	*p++ = cpu_to_be32(OP_GETATTR);
	*p++ = cpu_to_be32(1);
	*p = cpu_to_be32(bitmap);
	hdr->nops++;
	hdr->replen += decode_getattr_maxsz;
}

static void encode_getattr_two(struct xdr_stream *xdr, uint32_t bm0, uint32_t bm1, struct compound_hdr *hdr)
{
	__be32 *p;

	p = reserve_space(xdr, 16);
	*p++ = cpu_to_be32(OP_GETATTR);
	*p++ = cpu_to_be32(2);
	*p++ = cpu_to_be32(bm0);
	*p = cpu_to_be32(bm1);
	hdr->nops++;
	hdr->replen += decode_getattr_maxsz;
}

static void encode_getfattr(struct xdr_stream *xdr, const u32* bitmask, struct compound_hdr *hdr)
{
	encode_getattr_two(xdr, bitmask[0] & nfs4_fattr_bitmap[0],
			   bitmask[1] & nfs4_fattr_bitmap[1], hdr);
}

static void encode_fsinfo(struct xdr_stream *xdr, const u32* bitmask, struct compound_hdr *hdr)
{
	encode_getattr_two(xdr, bitmask[0] & nfs4_fsinfo_bitmap[0],
			   bitmask[1] & nfs4_fsinfo_bitmap[1], hdr);
}

static void encode_fs_locations(struct xdr_stream *xdr, const u32* bitmask, struct compound_hdr *hdr)
{
	encode_getattr_two(xdr, bitmask[0] & nfs4_fs_locations_bitmap[0],
			   bitmask[1] & nfs4_fs_locations_bitmap[1], hdr);
}

static void encode_getfh(struct xdr_stream *xdr, struct compound_hdr *hdr)
{
	__be32 *p;

	p = reserve_space(xdr, 4);
	*p = cpu_to_be32(OP_GETFH);
	hdr->nops++;
	hdr->replen += decode_getfh_maxsz;
}

static void encode_link(struct xdr_stream *xdr, const struct qstr *name, struct compound_hdr *hdr)
{
	__be32 *p;

	p = reserve_space(xdr, 8 + name->len);
	*p++ = cpu_to_be32(OP_LINK);
	xdr_encode_opaque(p, name->name, name->len);
	hdr->nops++;
	hdr->replen += decode_link_maxsz;
}

static inline int nfs4_lock_type(struct file_lock *fl, int block)
{
	if ((fl->fl_type & (F_RDLCK|F_WRLCK|F_UNLCK)) == F_RDLCK)
		return block ? NFS4_READW_LT : NFS4_READ_LT;
	return block ? NFS4_WRITEW_LT : NFS4_WRITE_LT;
}

static inline uint64_t nfs4_lock_length(struct file_lock *fl)
{
	if (fl->fl_end == OFFSET_MAX)
		return ~(uint64_t)0;
	return fl->fl_end - fl->fl_start + 1;
}

/*
 * opcode,type,reclaim,offset,length,new_lock_owner = 32
 * open_seqid,open_stateid,lock_seqid,lock_owner.clientid, lock_owner.id = 40
 */
static void encode_lock(struct xdr_stream *xdr, const struct nfs_lock_args *args, struct compound_hdr *hdr)
{
	__be32 *p;

	p = reserve_space(xdr, 32);
	*p++ = cpu_to_be32(OP_LOCK);
	*p++ = cpu_to_be32(nfs4_lock_type(args->fl, args->block));
	*p++ = cpu_to_be32(args->reclaim);
	p = xdr_encode_hyper(p, args->fl->fl_start);
	p = xdr_encode_hyper(p, nfs4_lock_length(args->fl));
	*p = cpu_to_be32(args->new_lock_owner);
	if (args->new_lock_owner){
		p = reserve_space(xdr, 4+NFS4_STATEID_SIZE+32);
		*p++ = cpu_to_be32(args->open_seqid->sequence->counter);
		p = xdr_encode_opaque_fixed(p, args->open_stateid->data, NFS4_STATEID_SIZE);
		*p++ = cpu_to_be32(args->lock_seqid->sequence->counter);
		p = xdr_encode_hyper(p, args->lock_owner.clientid);
		*p++ = cpu_to_be32(16);
		p = xdr_encode_opaque_fixed(p, "lock id:", 8);
		xdr_encode_hyper(p, args->lock_owner.id);
	}
	else {
		p = reserve_space(xdr, NFS4_STATEID_SIZE+4);
		p = xdr_encode_opaque_fixed(p, args->lock_stateid->data, NFS4_STATEID_SIZE);
		*p = cpu_to_be32(args->lock_seqid->sequence->counter);
	}
	hdr->nops++;
	hdr->replen += decode_lock_maxsz;
}

static void encode_lockt(struct xdr_stream *xdr, const struct nfs_lockt_args *args, struct compound_hdr *hdr)
{
	__be32 *p;

	p = reserve_space(xdr, 52);
	*p++ = cpu_to_be32(OP_LOCKT);
	*p++ = cpu_to_be32(nfs4_lock_type(args->fl, 0));
	p = xdr_encode_hyper(p, args->fl->fl_start);
	p = xdr_encode_hyper(p, nfs4_lock_length(args->fl));
	p = xdr_encode_hyper(p, args->lock_owner.clientid);
	*p++ = cpu_to_be32(16);
	p = xdr_encode_opaque_fixed(p, "lock id:", 8);
	xdr_encode_hyper(p, args->lock_owner.id);
	hdr->nops++;
	hdr->replen += decode_lockt_maxsz;
}

static void encode_locku(struct xdr_stream *xdr, const struct nfs_locku_args *args, struct compound_hdr *hdr)
{
	__be32 *p;

	p = reserve_space(xdr, 12+NFS4_STATEID_SIZE+16);
	*p++ = cpu_to_be32(OP_LOCKU);
	*p++ = cpu_to_be32(nfs4_lock_type(args->fl, 0));
	*p++ = cpu_to_be32(args->seqid->sequence->counter);
	p = xdr_encode_opaque_fixed(p, args->stateid->data, NFS4_STATEID_SIZE);
	p = xdr_encode_hyper(p, args->fl->fl_start);
	xdr_encode_hyper(p, nfs4_lock_length(args->fl));
	hdr->nops++;
	hdr->replen += decode_locku_maxsz;
}

static void encode_lookup(struct xdr_stream *xdr, const struct qstr *name, struct compound_hdr *hdr)
{
	int len = name->len;
	__be32 *p;

	p = reserve_space(xdr, 8 + len);
	*p++ = cpu_to_be32(OP_LOOKUP);
	xdr_encode_opaque(p, name->name, len);
	hdr->nops++;
	hdr->replen += decode_lookup_maxsz;
}

static void encode_share_access(struct xdr_stream *xdr, fmode_t fmode)
{
	__be32 *p;

	p = reserve_space(xdr, 8);
	switch (fmode & (FMODE_READ|FMODE_WRITE)) {
	case FMODE_READ:
		*p++ = cpu_to_be32(NFS4_SHARE_ACCESS_READ);
		break;
	case FMODE_WRITE:
		*p++ = cpu_to_be32(NFS4_SHARE_ACCESS_WRITE);
		break;
	case FMODE_READ|FMODE_WRITE:
		*p++ = cpu_to_be32(NFS4_SHARE_ACCESS_BOTH);
		break;
	default:
		*p++ = cpu_to_be32(0);
	}
	*p = cpu_to_be32(0);		/* for linux, share_deny = 0 always */
}

static inline void encode_openhdr(struct xdr_stream *xdr, const struct nfs_openargs *arg)
{
	__be32 *p;
 /*
 * opcode 4, seqid 4, share_access 4, share_deny 4, clientid 8, ownerlen 4,
 * owner 4 = 32
 */
	p = reserve_space(xdr, 8);
	*p++ = cpu_to_be32(OP_OPEN);
	*p = cpu_to_be32(arg->seqid->sequence->counter);
	encode_share_access(xdr, arg->fmode);
	p = reserve_space(xdr, 28);
	p = xdr_encode_hyper(p, arg->clientid);
	*p++ = cpu_to_be32(16);
	p = xdr_encode_opaque_fixed(p, "open id:", 8);
	xdr_encode_hyper(p, arg->id);
}

static inline void encode_createmode(struct xdr_stream *xdr, const struct nfs_openargs *arg)
{
	__be32 *p;

	p = reserve_space(xdr, 4);
	switch(arg->open_flags & O_EXCL) {
	case 0:
		*p = cpu_to_be32(NFS4_CREATE_UNCHECKED);
		encode_attrs(xdr, arg->u.attrs, arg->server);
		break;
	default:
		*p = cpu_to_be32(NFS4_CREATE_EXCLUSIVE);
		encode_nfs4_verifier(xdr, &arg->u.verifier);
	}
}

static void encode_opentype(struct xdr_stream *xdr, const struct nfs_openargs *arg)
{
	__be32 *p;

	p = reserve_space(xdr, 4);
	switch (arg->open_flags & O_CREAT) {
	case 0:
		*p = cpu_to_be32(NFS4_OPEN_NOCREATE);
		break;
	default:
		BUG_ON(arg->claim != NFS4_OPEN_CLAIM_NULL);
		*p = cpu_to_be32(NFS4_OPEN_CREATE);
		encode_createmode(xdr, arg);
	}
}

static inline void encode_delegation_type(struct xdr_stream *xdr, fmode_t delegation_type)
{
	__be32 *p;

	p = reserve_space(xdr, 4);
	switch (delegation_type) {
	case 0:
		*p = cpu_to_be32(NFS4_OPEN_DELEGATE_NONE);
		break;
	case FMODE_READ:
		*p = cpu_to_be32(NFS4_OPEN_DELEGATE_READ);
		break;
	case FMODE_WRITE|FMODE_READ:
		*p = cpu_to_be32(NFS4_OPEN_DELEGATE_WRITE);
		break;
	default:
		BUG();
	}
}

static inline void encode_claim_null(struct xdr_stream *xdr, const struct qstr *name)
{
	__be32 *p;

	p = reserve_space(xdr, 4);
	*p = cpu_to_be32(NFS4_OPEN_CLAIM_NULL);
	encode_string(xdr, name->len, name->name);
}

static inline void encode_claim_previous(struct xdr_stream *xdr, fmode_t type)
{
	__be32 *p;

	p = reserve_space(xdr, 4);
	*p = cpu_to_be32(NFS4_OPEN_CLAIM_PREVIOUS);
	encode_delegation_type(xdr, type);
}

static inline void encode_claim_delegate_cur(struct xdr_stream *xdr, const struct qstr *name, const nfs4_stateid *stateid)
{
	__be32 *p;

	p = reserve_space(xdr, 4+NFS4_STATEID_SIZE);
	*p++ = cpu_to_be32(NFS4_OPEN_CLAIM_DELEGATE_CUR);
	xdr_encode_opaque_fixed(p, stateid->data, NFS4_STATEID_SIZE);
	encode_string(xdr, name->len, name->name);
}

static void encode_open(struct xdr_stream *xdr, const struct nfs_openargs *arg, struct compound_hdr *hdr)
{
	encode_openhdr(xdr, arg);
	encode_opentype(xdr, arg);
	switch (arg->claim) {
	case NFS4_OPEN_CLAIM_NULL:
		encode_claim_null(xdr, arg->name);
		break;
	case NFS4_OPEN_CLAIM_PREVIOUS:
		encode_claim_previous(xdr, arg->u.delegation_type);
		break;
	case NFS4_OPEN_CLAIM_DELEGATE_CUR:
		encode_claim_delegate_cur(xdr, arg->name, &arg->u.delegation);
		break;
	default:
		BUG();
	}
	hdr->nops++;
	hdr->replen += decode_open_maxsz;
}

static void encode_open_confirm(struct xdr_stream *xdr, const struct nfs_open_confirmargs *arg, struct compound_hdr *hdr)
{
	__be32 *p;

	p = reserve_space(xdr, 4+NFS4_STATEID_SIZE+4);
	*p++ = cpu_to_be32(OP_OPEN_CONFIRM);
	p = xdr_encode_opaque_fixed(p, arg->stateid->data, NFS4_STATEID_SIZE);
	*p = cpu_to_be32(arg->seqid->sequence->counter);
	hdr->nops++;
	hdr->replen += decode_open_confirm_maxsz;
}

static void encode_open_downgrade(struct xdr_stream *xdr, const struct nfs_closeargs *arg, struct compound_hdr *hdr)
{
	__be32 *p;

	p = reserve_space(xdr, 4+NFS4_STATEID_SIZE+4);
	*p++ = cpu_to_be32(OP_OPEN_DOWNGRADE);
	p = xdr_encode_opaque_fixed(p, arg->stateid->data, NFS4_STATEID_SIZE);
	*p = cpu_to_be32(arg->seqid->sequence->counter);
	encode_share_access(xdr, arg->fmode);
	hdr->nops++;
	hdr->replen += decode_open_downgrade_maxsz;
}

static void
encode_putfh(struct xdr_stream *xdr, const struct nfs_fh *fh, struct compound_hdr *hdr)
{
	int len = fh->size;
	__be32 *p;

	p = reserve_space(xdr, 8 + len);
	*p++ = cpu_to_be32(OP_PUTFH);
	xdr_encode_opaque(p, fh->data, len);
	hdr->nops++;
	hdr->replen += decode_putfh_maxsz;
}

static void encode_putrootfh(struct xdr_stream *xdr, struct compound_hdr *hdr)
{
	__be32 *p;

	p = reserve_space(xdr, 4);
	*p = cpu_to_be32(OP_PUTROOTFH);
	hdr->nops++;
	hdr->replen += decode_putrootfh_maxsz;
}

static void encode_stateid(struct xdr_stream *xdr, const struct nfs_open_context *ctx)
{
	nfs4_stateid stateid;
	__be32 *p;

	p = reserve_space(xdr, NFS4_STATEID_SIZE);
	if (ctx->state != NULL) {
		nfs4_copy_stateid(&stateid, ctx->state, ctx->lockowner);
		xdr_encode_opaque_fixed(p, stateid.data, NFS4_STATEID_SIZE);
	} else
		xdr_encode_opaque_fixed(p, zero_stateid.data, NFS4_STATEID_SIZE);
}

static void encode_read(struct xdr_stream *xdr, const struct nfs_readargs *args, struct compound_hdr *hdr)
{
	__be32 *p;

	p = reserve_space(xdr, 4);
	*p = cpu_to_be32(OP_READ);

	encode_stateid(xdr, args->context);

	p = reserve_space(xdr, 12);
	p = xdr_encode_hyper(p, args->offset);
	*p = cpu_to_be32(args->count);
	hdr->nops++;
	hdr->replen += decode_read_maxsz;
}

static void encode_readdir(struct xdr_stream *xdr, const struct nfs4_readdir_arg *readdir, struct rpc_rqst *req, struct compound_hdr *hdr)
{
	uint32_t attrs[2] = {
		FATTR4_WORD0_RDATTR_ERROR|FATTR4_WORD0_FILEID,
		FATTR4_WORD1_MOUNTED_ON_FILEID,
	};
	__be32 *p;

	p = reserve_space(xdr, 12+NFS4_VERIFIER_SIZE+20);
	*p++ = cpu_to_be32(OP_READDIR);
	p = xdr_encode_hyper(p, readdir->cookie);
	p = xdr_encode_opaque_fixed(p, readdir->verifier.data, NFS4_VERIFIER_SIZE);
	*p++ = cpu_to_be32(readdir->count >> 1);  /* We're not doing readdirplus */
	*p++ = cpu_to_be32(readdir->count);
	*p++ = cpu_to_be32(2);
	/* Switch to mounted_on_fileid if the server supports it */
	if (readdir->bitmask[1] & FATTR4_WORD1_MOUNTED_ON_FILEID)
		attrs[0] &= ~FATTR4_WORD0_FILEID;
	else
		attrs[1] &= ~FATTR4_WORD1_MOUNTED_ON_FILEID;
	*p++ = cpu_to_be32(attrs[0] & readdir->bitmask[0]);
	*p = cpu_to_be32(attrs[1] & readdir->bitmask[1]);
	hdr->nops++;
	hdr->replen += decode_readdir_maxsz;
	dprintk("%s: cookie = %Lu, verifier = %08x:%08x, bitmap = %08x:%08x\n",
			__func__,
			(unsigned long long)readdir->cookie,
			((u32 *)readdir->verifier.data)[0],
			((u32 *)readdir->verifier.data)[1],
			attrs[0] & readdir->bitmask[0],
			attrs[1] & readdir->bitmask[1]);
}

static void encode_readlink(struct xdr_stream *xdr, const struct nfs4_readlink *readlink, struct rpc_rqst *req, struct compound_hdr *hdr)
{
	__be32 *p;

	p = reserve_space(xdr, 4);
	*p = cpu_to_be32(OP_READLINK);
	hdr->nops++;
	hdr->replen += decode_readlink_maxsz;
}

static void encode_remove(struct xdr_stream *xdr, const struct qstr *name, struct compound_hdr *hdr)
{
	__be32 *p;

	p = reserve_space(xdr, 8 + name->len);
	*p++ = cpu_to_be32(OP_REMOVE);
	xdr_encode_opaque(p, name->name, name->len);
	hdr->nops++;
	hdr->replen += decode_remove_maxsz;
}

static void encode_rename(struct xdr_stream *xdr, const struct qstr *oldname, const struct qstr *newname, struct compound_hdr *hdr)
{
	__be32 *p;

	p = reserve_space(xdr, 4);
	*p = cpu_to_be32(OP_RENAME);
	encode_string(xdr, oldname->len, oldname->name);
	encode_string(xdr, newname->len, newname->name);
	hdr->nops++;
	hdr->replen += decode_rename_maxsz;
}

static void encode_renew(struct xdr_stream *xdr, const struct nfs_client *client_stateid, struct compound_hdr *hdr)
{
	__be32 *p;

	p = reserve_space(xdr, 12);
	*p++ = cpu_to_be32(OP_RENEW);
	xdr_encode_hyper(p, client_stateid->cl_clientid);
	hdr->nops++;
	hdr->replen += decode_renew_maxsz;
}

static void
encode_restorefh(struct xdr_stream *xdr, struct compound_hdr *hdr)
{
	__be32 *p;

	p = reserve_space(xdr, 4);
	*p = cpu_to_be32(OP_RESTOREFH);
	hdr->nops++;
	hdr->replen += decode_restorefh_maxsz;
}

static int
encode_setacl(struct xdr_stream *xdr, struct nfs_setaclargs *arg, struct compound_hdr *hdr)
{
	__be32 *p;

	p = reserve_space(xdr, 4+NFS4_STATEID_SIZE);
	*p++ = cpu_to_be32(OP_SETATTR);
	xdr_encode_opaque_fixed(p, zero_stateid.data, NFS4_STATEID_SIZE);
	p = reserve_space(xdr, 2*4);
	*p++ = cpu_to_be32(1);
	*p = cpu_to_be32(FATTR4_WORD0_ACL);
	if (arg->acl_len % 4)
		return -EINVAL;
	p = reserve_space(xdr, 4);
	*p = cpu_to_be32(arg->acl_len);
	xdr_write_pages(xdr, arg->acl_pages, arg->acl_pgbase, arg->acl_len);
	hdr->nops++;
	hdr->replen += decode_setacl_maxsz;
	return 0;
}

static void
encode_savefh(struct xdr_stream *xdr, struct compound_hdr *hdr)
{
	__be32 *p;

	p = reserve_space(xdr, 4);
	*p = cpu_to_be32(OP_SAVEFH);
	hdr->nops++;
	hdr->replen += decode_savefh_maxsz;
}

static void encode_setattr(struct xdr_stream *xdr, const struct nfs_setattrargs *arg, const struct nfs_server *server, struct compound_hdr *hdr)
{
	__be32 *p;

	p = reserve_space(xdr, 4+NFS4_STATEID_SIZE);
	*p++ = cpu_to_be32(OP_SETATTR);
	xdr_encode_opaque_fixed(p, arg->stateid.data, NFS4_STATEID_SIZE);
	hdr->nops++;
	hdr->replen += decode_setattr_maxsz;
	encode_attrs(xdr, arg->iap, server);
}

static void encode_setclientid(struct xdr_stream *xdr, const struct nfs4_setclientid *setclientid, struct compound_hdr *hdr)
{
	__be32 *p;

	p = reserve_space(xdr, 4 + NFS4_VERIFIER_SIZE);
	*p++ = cpu_to_be32(OP_SETCLIENTID);
	xdr_encode_opaque_fixed(p, setclientid->sc_verifier->data, NFS4_VERIFIER_SIZE);

	encode_string(xdr, setclientid->sc_name_len, setclientid->sc_name);
	p = reserve_space(xdr, 4);
	*p = cpu_to_be32(setclientid->sc_prog);
	encode_string(xdr, setclientid->sc_netid_len, setclientid->sc_netid);
	encode_string(xdr, setclientid->sc_uaddr_len, setclientid->sc_uaddr);
	p = reserve_space(xdr, 4);
	*p = cpu_to_be32(setclientid->sc_cb_ident);
	hdr->nops++;
	hdr->replen += decode_setclientid_maxsz;
}

static void encode_setclientid_confirm(struct xdr_stream *xdr, const struct nfs_client *client_state, struct compound_hdr *hdr)
{
	__be32 *p;

	p = reserve_space(xdr, 12 + NFS4_VERIFIER_SIZE);
	*p++ = cpu_to_be32(OP_SETCLIENTID_CONFIRM);
	p = xdr_encode_hyper(p, client_state->cl_clientid);
	xdr_encode_opaque_fixed(p, client_state->cl_confirm.data, NFS4_VERIFIER_SIZE);
	hdr->nops++;
	hdr->replen += decode_setclientid_confirm_maxsz;
}

static void encode_write(struct xdr_stream *xdr, const struct nfs_writeargs *args, struct compound_hdr *hdr)
{
	__be32 *p;

	p = reserve_space(xdr, 4);
	*p = cpu_to_be32(OP_WRITE);

	encode_stateid(xdr, args->context);

	p = reserve_space(xdr, 16);
	p = xdr_encode_hyper(p, args->offset);
	*p++ = cpu_to_be32(args->stable);
	*p = cpu_to_be32(args->count);

	xdr_write_pages(xdr, args->pages, args->pgbase, args->count);
	hdr->nops++;
	hdr->replen += decode_write_maxsz;
}

static void encode_delegreturn(struct xdr_stream *xdr, const nfs4_stateid *stateid, struct compound_hdr *hdr)
{
	__be32 *p;

	p = reserve_space(xdr, 4+NFS4_STATEID_SIZE);

	*p++ = cpu_to_be32(OP_DELEGRETURN);
	xdr_encode_opaque_fixed(p, stateid->data, NFS4_STATEID_SIZE);
	hdr->nops++;
	hdr->replen += decode_delegreturn_maxsz;
}

#if defined(CONFIG_NFS_V4_1)
/* NFSv4.1 operations */
static void encode_exchange_id(struct xdr_stream *xdr,
			       struct nfs41_exchange_id_args *args,
			       struct compound_hdr *hdr)
{
	__be32 *p;

	p = reserve_space(xdr, 4 + sizeof(args->verifier->data));
	*p++ = cpu_to_be32(OP_EXCHANGE_ID);
	xdr_encode_opaque_fixed(p, args->verifier->data, sizeof(args->verifier->data));

	encode_string(xdr, args->id_len, args->id);

	p = reserve_space(xdr, 12);
	*p++ = cpu_to_be32(args->flags);
	*p++ = cpu_to_be32(0);	/* zero length state_protect4_a */
	*p = cpu_to_be32(0);	/* zero length implementation id array */
	hdr->nops++;
	hdr->replen += decode_exchange_id_maxsz;
}

static void encode_create_session(struct xdr_stream *xdr,
				  struct nfs41_create_session_args *args,
				  struct compound_hdr *hdr)
{
	__be32 *p;
	char machine_name[NFS4_MAX_MACHINE_NAME_LEN];
	uint32_t len;
	struct nfs_client *clp = args->client;

	len = scnprintf(machine_name, sizeof(machine_name), "%s",
			clp->cl_ipaddr);

	p = reserve_space(xdr, 20 + 2*28 + 20 + len + 12);
	*p++ = cpu_to_be32(OP_CREATE_SESSION);
	p = xdr_encode_hyper(p, clp->cl_ex_clid);
	*p++ = cpu_to_be32(clp->cl_seqid);			/*Sequence id */
	*p++ = cpu_to_be32(args->flags);			/*flags */

	/* Fore Channel */
	*p++ = cpu_to_be32(args->fc_attrs.headerpadsz);	/* header padding size */
	*p++ = cpu_to_be32(args->fc_attrs.max_rqst_sz);	/* max req size */
	*p++ = cpu_to_be32(args->fc_attrs.max_resp_sz);	/* max resp size */
	*p++ = cpu_to_be32(args->fc_attrs.max_resp_sz_cached);	/* Max resp sz cached */
	*p++ = cpu_to_be32(args->fc_attrs.max_ops);	/* max operations */
	*p++ = cpu_to_be32(args->fc_attrs.max_reqs);	/* max requests */
	*p++ = cpu_to_be32(0);				/* rdmachannel_attrs */

	/* Back Channel */
	*p++ = cpu_to_be32(args->fc_attrs.headerpadsz);	/* header padding size */
	*p++ = cpu_to_be32(args->bc_attrs.max_rqst_sz);	/* max req size */
	*p++ = cpu_to_be32(args->bc_attrs.max_resp_sz);	/* max resp size */
	*p++ = cpu_to_be32(args->bc_attrs.max_resp_sz_cached);	/* Max resp sz cached */
	*p++ = cpu_to_be32(args->bc_attrs.max_ops);	/* max operations */
	*p++ = cpu_to_be32(args->bc_attrs.max_reqs);	/* max requests */
	*p++ = cpu_to_be32(0);				/* rdmachannel_attrs */

	*p++ = cpu_to_be32(args->cb_program);		/* cb_program */
	*p++ = cpu_to_be32(1);
	*p++ = cpu_to_be32(RPC_AUTH_UNIX);			/* auth_sys */

	/* authsys_parms rfc1831 */
	*p++ = cpu_to_be32((u32)clp->cl_boot_time.tv_nsec);	/* stamp */
	p = xdr_encode_opaque(p, machine_name, len);
	*p++ = cpu_to_be32(0);				/* UID */
	*p++ = cpu_to_be32(0);				/* GID */
	*p = cpu_to_be32(0);				/* No more gids */
	hdr->nops++;
	hdr->replen += decode_create_session_maxsz;
}

static void encode_destroy_session(struct xdr_stream *xdr,
				   struct nfs4_session *session,
				   struct compound_hdr *hdr)
{
	__be32 *p;
	p = reserve_space(xdr, 4 + NFS4_MAX_SESSIONID_LEN);
	*p++ = cpu_to_be32(OP_DESTROY_SESSION);
	xdr_encode_opaque_fixed(p, session->sess_id.data, NFS4_MAX_SESSIONID_LEN);
	hdr->nops++;
	hdr->replen += decode_destroy_session_maxsz;
}
#endif /* CONFIG_NFS_V4_1 */

static void encode_sequence(struct xdr_stream *xdr,
			    const struct nfs4_sequence_args *args,
			    struct compound_hdr *hdr)
{
#if defined(CONFIG_NFS_V4_1)
	struct nfs4_session *session = args->sa_session;
	struct nfs4_slot_table *tp;
	struct nfs4_slot *slot;
	__be32 *p;

	if (!session)
		return;

	tp = &session->fc_slot_table;

	WARN_ON(args->sa_slotid == NFS4_MAX_SLOT_TABLE);
	slot = tp->slots + args->sa_slotid;

	p = reserve_space(xdr, 4 + NFS4_MAX_SESSIONID_LEN + 16);
	*p++ = cpu_to_be32(OP_SEQUENCE);

	/*
	 * Sessionid + seqid + slotid + max slotid + cache_this
	 */
	dprintk("%s: sessionid=%u:%u:%u:%u seqid=%d slotid=%d "
		"max_slotid=%d cache_this=%d\n",
		__func__,
		((u32 *)session->sess_id.data)[0],
		((u32 *)session->sess_id.data)[1],
		((u32 *)session->sess_id.data)[2],
		((u32 *)session->sess_id.data)[3],
		slot->seq_nr, args->sa_slotid,
		tp->highest_used_slotid, args->sa_cache_this);
	p = xdr_encode_opaque_fixed(p, session->sess_id.data, NFS4_MAX_SESSIONID_LEN);
	*p++ = cpu_to_be32(slot->seq_nr);
	*p++ = cpu_to_be32(args->sa_slotid);
	*p++ = cpu_to_be32(tp->highest_used_slotid);
	*p = cpu_to_be32(args->sa_cache_this);
	hdr->nops++;
	hdr->replen += decode_sequence_maxsz;
#endif /* CONFIG_NFS_V4_1 */
}

/*
 * END OF "GENERIC" ENCODE ROUTINES.
 */

static u32 nfs4_xdr_minorversion(const struct nfs4_sequence_args *args)
{
#if defined(CONFIG_NFS_V4_1)
	if (args->sa_session)
		return args->sa_session->clp->cl_minorversion;
#endif /* CONFIG_NFS_V4_1 */
	return 0;
}

/*
 * Encode an ACCESS request
 */
static int nfs4_xdr_enc_access(struct rpc_rqst *req, __be32 *p, const struct nfs4_accessargs *args)
{
	struct xdr_stream xdr;
	struct compound_hdr hdr = {
		.minorversion = nfs4_xdr_minorversion(&args->seq_args),
	};

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_compound_hdr(&xdr, req, &hdr);
	encode_sequence(&xdr, &args->seq_args, &hdr);
	encode_putfh(&xdr, args->fh, &hdr);
	encode_access(&xdr, args->access, &hdr);
	encode_getfattr(&xdr, args->bitmask, &hdr);
	encode_nops(&hdr);
	return 0;
}

/*
 * Encode LOOKUP request
 */
static int nfs4_xdr_enc_lookup(struct rpc_rqst *req, __be32 *p, const struct nfs4_lookup_arg *args)
{
	struct xdr_stream xdr;
	struct compound_hdr hdr = {
		.minorversion = nfs4_xdr_minorversion(&args->seq_args),
	};

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_compound_hdr(&xdr, req, &hdr);
	encode_sequence(&xdr, &args->seq_args, &hdr);
	encode_putfh(&xdr, args->dir_fh, &hdr);
	encode_lookup(&xdr, args->name, &hdr);
	encode_getfh(&xdr, &hdr);
	encode_getfattr(&xdr, args->bitmask, &hdr);
	encode_nops(&hdr);
	return 0;
}

/*
 * Encode LOOKUP_ROOT request
 */
static int nfs4_xdr_enc_lookup_root(struct rpc_rqst *req, __be32 *p, const struct nfs4_lookup_root_arg *args)
{
	struct xdr_stream xdr;
	struct compound_hdr hdr = {
		.minorversion = nfs4_xdr_minorversion(&args->seq_args),
	};

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_compound_hdr(&xdr, req, &hdr);
	encode_sequence(&xdr, &args->seq_args, &hdr);
	encode_putrootfh(&xdr, &hdr);
	encode_getfh(&xdr, &hdr);
	encode_getfattr(&xdr, args->bitmask, &hdr);
	encode_nops(&hdr);
	return 0;
}

/*
 * Encode REMOVE request
 */
static int nfs4_xdr_enc_remove(struct rpc_rqst *req, __be32 *p, const struct nfs_removeargs *args)
{
	struct xdr_stream xdr;
	struct compound_hdr hdr = {
		.minorversion = nfs4_xdr_minorversion(&args->seq_args),
	};

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_compound_hdr(&xdr, req, &hdr);
	encode_sequence(&xdr, &args->seq_args, &hdr);
	encode_putfh(&xdr, args->fh, &hdr);
	encode_remove(&xdr, &args->name, &hdr);
	encode_getfattr(&xdr, args->bitmask, &hdr);
	encode_nops(&hdr);
	return 0;
}

/*
 * Encode RENAME request
 */
static int nfs4_xdr_enc_rename(struct rpc_rqst *req, __be32 *p, const struct nfs4_rename_arg *args)
{
	struct xdr_stream xdr;
	struct compound_hdr hdr = {
		.minorversion = nfs4_xdr_minorversion(&args->seq_args),
	};

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_compound_hdr(&xdr, req, &hdr);
	encode_sequence(&xdr, &args->seq_args, &hdr);
	encode_putfh(&xdr, args->old_dir, &hdr);
	encode_savefh(&xdr, &hdr);
	encode_putfh(&xdr, args->new_dir, &hdr);
	encode_rename(&xdr, args->old_name, args->new_name, &hdr);
	encode_getfattr(&xdr, args->bitmask, &hdr);
	encode_restorefh(&xdr, &hdr);
	encode_getfattr(&xdr, args->bitmask, &hdr);
	encode_nops(&hdr);
	return 0;
}

/*
 * Encode LINK request
 */
static int nfs4_xdr_enc_link(struct rpc_rqst *req, __be32 *p, const struct nfs4_link_arg *args)
{
	struct xdr_stream xdr;
	struct compound_hdr hdr = {
		.minorversion = nfs4_xdr_minorversion(&args->seq_args),
	};

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_compound_hdr(&xdr, req, &hdr);
	encode_sequence(&xdr, &args->seq_args, &hdr);
	encode_putfh(&xdr, args->fh, &hdr);
	encode_savefh(&xdr, &hdr);
	encode_putfh(&xdr, args->dir_fh, &hdr);
	encode_link(&xdr, args->name, &hdr);
	encode_getfattr(&xdr, args->bitmask, &hdr);
	encode_restorefh(&xdr, &hdr);
	encode_getfattr(&xdr, args->bitmask, &hdr);
	encode_nops(&hdr);
	return 0;
}

/*
 * Encode CREATE request
 */
static int nfs4_xdr_enc_create(struct rpc_rqst *req, __be32 *p, const struct nfs4_create_arg *args)
{
	struct xdr_stream xdr;
	struct compound_hdr hdr = {
		.minorversion = nfs4_xdr_minorversion(&args->seq_args),
	};

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_compound_hdr(&xdr, req, &hdr);
	encode_sequence(&xdr, &args->seq_args, &hdr);
	encode_putfh(&xdr, args->dir_fh, &hdr);
	encode_savefh(&xdr, &hdr);
	encode_create(&xdr, args, &hdr);
	encode_getfh(&xdr, &hdr);
	encode_getfattr(&xdr, args->bitmask, &hdr);
	encode_restorefh(&xdr, &hdr);
	encode_getfattr(&xdr, args->bitmask, &hdr);
	encode_nops(&hdr);
	return 0;
}

/*
 * Encode SYMLINK request
 */
static int nfs4_xdr_enc_symlink(struct rpc_rqst *req, __be32 *p, const struct nfs4_create_arg *args)
{
	return nfs4_xdr_enc_create(req, p, args);
}

/*
 * Encode GETATTR request
 */
static int nfs4_xdr_enc_getattr(struct rpc_rqst *req, __be32 *p, const struct nfs4_getattr_arg *args)
{
	struct xdr_stream xdr;
	struct compound_hdr hdr = {
		.minorversion = nfs4_xdr_minorversion(&args->seq_args),
	};

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_compound_hdr(&xdr, req, &hdr);
	encode_sequence(&xdr, &args->seq_args, &hdr);
	encode_putfh(&xdr, args->fh, &hdr);
	encode_getfattr(&xdr, args->bitmask, &hdr);
	encode_nops(&hdr);
	return 0;
}

/*
 * Encode a CLOSE request
 */
static int nfs4_xdr_enc_close(struct rpc_rqst *req, __be32 *p, struct nfs_closeargs *args)
{
	struct xdr_stream xdr;
	struct compound_hdr hdr = {
		.minorversion = nfs4_xdr_minorversion(&args->seq_args),
	};

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_compound_hdr(&xdr, req, &hdr);
	encode_sequence(&xdr, &args->seq_args, &hdr);
	encode_putfh(&xdr, args->fh, &hdr);
	encode_close(&xdr, args, &hdr);
	encode_getfattr(&xdr, args->bitmask, &hdr);
	encode_nops(&hdr);
	return 0;
}

/*
 * Encode an OPEN request
 */
static int nfs4_xdr_enc_open(struct rpc_rqst *req, __be32 *p, struct nfs_openargs *args)
{
	struct xdr_stream xdr;
	struct compound_hdr hdr = {
		.minorversion = nfs4_xdr_minorversion(&args->seq_args),
	};

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_compound_hdr(&xdr, req, &hdr);
	encode_sequence(&xdr, &args->seq_args, &hdr);
	encode_putfh(&xdr, args->fh, &hdr);
	encode_savefh(&xdr, &hdr);
	encode_open(&xdr, args, &hdr);
	encode_getfh(&xdr, &hdr);
	encode_getfattr(&xdr, args->bitmask, &hdr);
	encode_restorefh(&xdr, &hdr);
	encode_getfattr(&xdr, args->bitmask, &hdr);
	encode_nops(&hdr);
	return 0;
}

/*
 * Encode an OPEN_CONFIRM request
 */
static int nfs4_xdr_enc_open_confirm(struct rpc_rqst *req, __be32 *p, struct nfs_open_confirmargs *args)
{
	struct xdr_stream xdr;
	struct compound_hdr hdr = {
		.nops   = 0,
	};

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_compound_hdr(&xdr, req, &hdr);
	encode_putfh(&xdr, args->fh, &hdr);
	encode_open_confirm(&xdr, args, &hdr);
	encode_nops(&hdr);
	return 0;
}

/*
 * Encode an OPEN request with no attributes.
 */
static int nfs4_xdr_enc_open_noattr(struct rpc_rqst *req, __be32 *p, struct nfs_openargs *args)
{
	struct xdr_stream xdr;
	struct compound_hdr hdr = {
		.minorversion = nfs4_xdr_minorversion(&args->seq_args),
	};

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_compound_hdr(&xdr, req, &hdr);
	encode_sequence(&xdr, &args->seq_args, &hdr);
	encode_putfh(&xdr, args->fh, &hdr);
	encode_open(&xdr, args, &hdr);
	encode_getfattr(&xdr, args->bitmask, &hdr);
	encode_nops(&hdr);
	return 0;
}

/*
 * Encode an OPEN_DOWNGRADE request
 */
static int nfs4_xdr_enc_open_downgrade(struct rpc_rqst *req, __be32 *p, struct nfs_closeargs *args)
{
	struct xdr_stream xdr;
	struct compound_hdr hdr = {
		.minorversion = nfs4_xdr_minorversion(&args->seq_args),
	};

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_compound_hdr(&xdr, req, &hdr);
	encode_sequence(&xdr, &args->seq_args, &hdr);
	encode_putfh(&xdr, args->fh, &hdr);
	encode_open_downgrade(&xdr, args, &hdr);
	encode_getfattr(&xdr, args->bitmask, &hdr);
	encode_nops(&hdr);
	return 0;
}

/*
 * Encode a LOCK request
 */
static int nfs4_xdr_enc_lock(struct rpc_rqst *req, __be32 *p, struct nfs_lock_args *args)
{
	struct xdr_stream xdr;
	struct compound_hdr hdr = {
		.minorversion = nfs4_xdr_minorversion(&args->seq_args),
	};

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_compound_hdr(&xdr, req, &hdr);
	encode_sequence(&xdr, &args->seq_args, &hdr);
	encode_putfh(&xdr, args->fh, &hdr);
	encode_lock(&xdr, args, &hdr);
	encode_nops(&hdr);
	return 0;
}

/*
 * Encode a LOCKT request
 */
static int nfs4_xdr_enc_lockt(struct rpc_rqst *req, __be32 *p, struct nfs_lockt_args *args)
{
	struct xdr_stream xdr;
	struct compound_hdr hdr = {
		.minorversion = nfs4_xdr_minorversion(&args->seq_args),
	};

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_compound_hdr(&xdr, req, &hdr);
	encode_sequence(&xdr, &args->seq_args, &hdr);
	encode_putfh(&xdr, args->fh, &hdr);
	encode_lockt(&xdr, args, &hdr);
	encode_nops(&hdr);
	return 0;
}

/*
 * Encode a LOCKU request
 */
static int nfs4_xdr_enc_locku(struct rpc_rqst *req, __be32 *p, struct nfs_locku_args *args)
{
	struct xdr_stream xdr;
	struct compound_hdr hdr = {
		.minorversion = nfs4_xdr_minorversion(&args->seq_args),
	};

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_compound_hdr(&xdr, req, &hdr);
	encode_sequence(&xdr, &args->seq_args, &hdr);
	encode_putfh(&xdr, args->fh, &hdr);
	encode_locku(&xdr, args, &hdr);
	encode_nops(&hdr);
	return 0;
}

/*
 * Encode a READLINK request
 */
static int nfs4_xdr_enc_readlink(struct rpc_rqst *req, __be32 *p, const struct nfs4_readlink *args)
{
	struct xdr_stream xdr;
	struct compound_hdr hdr = {
		.minorversion = nfs4_xdr_minorversion(&args->seq_args),
	};

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_compound_hdr(&xdr, req, &hdr);
	encode_sequence(&xdr, &args->seq_args, &hdr);
	encode_putfh(&xdr, args->fh, &hdr);
	encode_readlink(&xdr, args, req, &hdr);

	xdr_inline_pages(&req->rq_rcv_buf, hdr.replen << 2, args->pages,
			args->pgbase, args->pglen);
	encode_nops(&hdr);
	return 0;
}

/*
 * Encode a READDIR request
 */
static int nfs4_xdr_enc_readdir(struct rpc_rqst *req, __be32 *p, const struct nfs4_readdir_arg *args)
{
	struct xdr_stream xdr;
	struct compound_hdr hdr = {
		.minorversion = nfs4_xdr_minorversion(&args->seq_args),
	};

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_compound_hdr(&xdr, req, &hdr);
	encode_sequence(&xdr, &args->seq_args, &hdr);
	encode_putfh(&xdr, args->fh, &hdr);
	encode_readdir(&xdr, args, req, &hdr);

	xdr_inline_pages(&req->rq_rcv_buf, hdr.replen << 2, args->pages,
			 args->pgbase, args->count);
	dprintk("%s: inlined page args = (%u, %p, %u, %u)\n",
			__func__, hdr.replen << 2, args->pages,
			args->pgbase, args->count);
	encode_nops(&hdr);
	return 0;
}

/*
 * Encode a READ request
 */
static int nfs4_xdr_enc_read(struct rpc_rqst *req, __be32 *p, struct nfs_readargs *args)
{
	struct xdr_stream xdr;
	struct compound_hdr hdr = {
		.minorversion = nfs4_xdr_minorversion(&args->seq_args),
	};

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_compound_hdr(&xdr, req, &hdr);
	encode_sequence(&xdr, &args->seq_args, &hdr);
	encode_putfh(&xdr, args->fh, &hdr);
	encode_read(&xdr, args, &hdr);

	xdr_inline_pages(&req->rq_rcv_buf, hdr.replen << 2,
			 args->pages, args->pgbase, args->count);
	req->rq_rcv_buf.flags |= XDRBUF_READ;
	encode_nops(&hdr);
	return 0;
}

/*
 * Encode an SETATTR request
 */
static int nfs4_xdr_enc_setattr(struct rpc_rqst *req, __be32 *p, struct nfs_setattrargs *args)
{
	struct xdr_stream xdr;
	struct compound_hdr hdr = {
		.minorversion = nfs4_xdr_minorversion(&args->seq_args),
	};

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_compound_hdr(&xdr, req, &hdr);
	encode_sequence(&xdr, &args->seq_args, &hdr);
	encode_putfh(&xdr, args->fh, &hdr);
	encode_setattr(&xdr, args, args->server, &hdr);
	encode_getfattr(&xdr, args->bitmask, &hdr);
	encode_nops(&hdr);
	return 0;
}

/*
 * Encode a GETACL request
 */
static int
nfs4_xdr_enc_getacl(struct rpc_rqst *req, __be32 *p,
		struct nfs_getaclargs *args)
{
	struct xdr_stream xdr;
	struct compound_hdr hdr = {
		.minorversion = nfs4_xdr_minorversion(&args->seq_args),
	};
	uint32_t replen;

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_compound_hdr(&xdr, req, &hdr);
	encode_sequence(&xdr, &args->seq_args, &hdr);
	encode_putfh(&xdr, args->fh, &hdr);
	replen = hdr.replen + nfs4_fattr_bitmap_maxsz + 1;
	encode_getattr_two(&xdr, FATTR4_WORD0_ACL, 0, &hdr);

	xdr_inline_pages(&req->rq_rcv_buf, replen << 2,
		args->acl_pages, args->acl_pgbase, args->acl_len);
	encode_nops(&hdr);
	return 0;
}

/*
 * Encode a WRITE request
 */
static int nfs4_xdr_enc_write(struct rpc_rqst *req, __be32 *p, struct nfs_writeargs *args)
{
	struct xdr_stream xdr;
	struct compound_hdr hdr = {
		.minorversion = nfs4_xdr_minorversion(&args->seq_args),
	};

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_compound_hdr(&xdr, req, &hdr);
	encode_sequence(&xdr, &args->seq_args, &hdr);
	encode_putfh(&xdr, args->fh, &hdr);
	encode_write(&xdr, args, &hdr);
	req->rq_snd_buf.flags |= XDRBUF_WRITE;
	encode_getfattr(&xdr, args->bitmask, &hdr);
	encode_nops(&hdr);
	return 0;
}

/*
 *  a COMMIT request
 */
static int nfs4_xdr_enc_commit(struct rpc_rqst *req, __be32 *p, struct nfs_writeargs *args)
{
	struct xdr_stream xdr;
	struct compound_hdr hdr = {
		.minorversion = nfs4_xdr_minorversion(&args->seq_args),
	};

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_compound_hdr(&xdr, req, &hdr);
	encode_sequence(&xdr, &args->seq_args, &hdr);
	encode_putfh(&xdr, args->fh, &hdr);
	encode_commit(&xdr, args, &hdr);
	encode_getfattr(&xdr, args->bitmask, &hdr);
	encode_nops(&hdr);
	return 0;
}

/*
 * FSINFO request
 */
static int nfs4_xdr_enc_fsinfo(struct rpc_rqst *req, __be32 *p, struct nfs4_fsinfo_arg *args)
{
	struct xdr_stream xdr;
	struct compound_hdr hdr = {
		.minorversion = nfs4_xdr_minorversion(&args->seq_args),
	};

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_compound_hdr(&xdr, req, &hdr);
	encode_sequence(&xdr, &args->seq_args, &hdr);
	encode_putfh(&xdr, args->fh, &hdr);
	encode_fsinfo(&xdr, args->bitmask, &hdr);
	encode_nops(&hdr);
	return 0;
}

/*
 * a PATHCONF request
 */
static int nfs4_xdr_enc_pathconf(struct rpc_rqst *req, __be32 *p, const struct nfs4_pathconf_arg *args)
{
	struct xdr_stream xdr;
	struct compound_hdr hdr = {
		.minorversion = nfs4_xdr_minorversion(&args->seq_args),
	};

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_compound_hdr(&xdr, req, &hdr);
	encode_sequence(&xdr, &args->seq_args, &hdr);
	encode_putfh(&xdr, args->fh, &hdr);
	encode_getattr_one(&xdr, args->bitmask[0] & nfs4_pathconf_bitmap[0],
			   &hdr);
	encode_nops(&hdr);
	return 0;
}

/*
 * a STATFS request
 */
static int nfs4_xdr_enc_statfs(struct rpc_rqst *req, __be32 *p, const struct nfs4_statfs_arg *args)
{
	struct xdr_stream xdr;
	struct compound_hdr hdr = {
		.minorversion = nfs4_xdr_minorversion(&args->seq_args),
	};

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_compound_hdr(&xdr, req, &hdr);
	encode_sequence(&xdr, &args->seq_args, &hdr);
	encode_putfh(&xdr, args->fh, &hdr);
	encode_getattr_two(&xdr, args->bitmask[0] & nfs4_statfs_bitmap[0],
			   args->bitmask[1] & nfs4_statfs_bitmap[1], &hdr);
	encode_nops(&hdr);
	return 0;
}

/*
 * GETATTR_BITMAP request
 */
static int nfs4_xdr_enc_server_caps(struct rpc_rqst *req, __be32 *p,
				    struct nfs4_server_caps_arg *args)
{
	struct xdr_stream xdr;
	struct compound_hdr hdr = {
		.minorversion = nfs4_xdr_minorversion(&args->seq_args),
	};

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_compound_hdr(&xdr, req, &hdr);
	encode_sequence(&xdr, &args->seq_args, &hdr);
	encode_putfh(&xdr, args->fhandle, &hdr);
	encode_getattr_one(&xdr, FATTR4_WORD0_SUPPORTED_ATTRS|
			   FATTR4_WORD0_LINK_SUPPORT|
			   FATTR4_WORD0_SYMLINK_SUPPORT|
			   FATTR4_WORD0_ACLSUPPORT, &hdr);
	encode_nops(&hdr);
	return 0;
}

/*
 * a RENEW request
 */
static int nfs4_xdr_enc_renew(struct rpc_rqst *req, __be32 *p, struct nfs_client *clp)
{
	struct xdr_stream xdr;
	struct compound_hdr hdr = {
		.nops	= 0,
	};

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_compound_hdr(&xdr, req, &hdr);
	encode_renew(&xdr, clp, &hdr);
	encode_nops(&hdr);
	return 0;
}

/*
 * a SETCLIENTID request
 */
static int nfs4_xdr_enc_setclientid(struct rpc_rqst *req, __be32 *p, struct nfs4_setclientid *sc)
{
	struct xdr_stream xdr;
	struct compound_hdr hdr = {
		.nops	= 0,
	};

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_compound_hdr(&xdr, req, &hdr);
	encode_setclientid(&xdr, sc, &hdr);
	encode_nops(&hdr);
	return 0;
}

/*
 * a SETCLIENTID_CONFIRM request
 */
static int nfs4_xdr_enc_setclientid_confirm(struct rpc_rqst *req, __be32 *p, struct nfs_client *clp)
{
	struct xdr_stream xdr;
	struct compound_hdr hdr = {
		.nops	= 0,
	};
	const u32 lease_bitmap[2] = { FATTR4_WORD0_LEASE_TIME, 0 };

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_compound_hdr(&xdr, req, &hdr);
	encode_setclientid_confirm(&xdr, clp, &hdr);
	encode_putrootfh(&xdr, &hdr);
	encode_fsinfo(&xdr, lease_bitmap, &hdr);
	encode_nops(&hdr);
	return 0;
}

/*
 * DELEGRETURN request
 */
static int nfs4_xdr_enc_delegreturn(struct rpc_rqst *req, __be32 *p, const struct nfs4_delegreturnargs *args)
{
	struct xdr_stream xdr;
	struct compound_hdr hdr = {
		.minorversion = nfs4_xdr_minorversion(&args->seq_args),
	};

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_compound_hdr(&xdr, req, &hdr);
	encode_sequence(&xdr, &args->seq_args, &hdr);
	encode_putfh(&xdr, args->fhandle, &hdr);
	encode_delegreturn(&xdr, args->stateid, &hdr);
	encode_getfattr(&xdr, args->bitmask, &hdr);
	encode_nops(&hdr);
	return 0;
}

/*
 * Encode FS_LOCATIONS request
 */
static int nfs4_xdr_enc_fs_locations(struct rpc_rqst *req, __be32 *p, struct nfs4_fs_locations_arg *args)
{
	struct xdr_stream xdr;
	struct compound_hdr hdr = {
		.minorversion = nfs4_xdr_minorversion(&args->seq_args),
	};
	uint32_t replen;

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_compound_hdr(&xdr, req, &hdr);
	encode_sequence(&xdr, &args->seq_args, &hdr);
	encode_putfh(&xdr, args->dir_fh, &hdr);
	encode_lookup(&xdr, args->name, &hdr);
	replen = hdr.replen;	/* get the attribute into args->page */
	encode_fs_locations(&xdr, args->bitmask, &hdr);

	xdr_inline_pages(&req->rq_rcv_buf, replen << 2, &args->page,
			0, PAGE_SIZE);
	encode_nops(&hdr);
	return 0;
}

#if defined(CONFIG_NFS_V4_1)
/*
 * EXCHANGE_ID request
 */
static int nfs4_xdr_enc_exchange_id(struct rpc_rqst *req, uint32_t *p,
				    struct nfs41_exchange_id_args *args)
{
	struct xdr_stream xdr;
	struct compound_hdr hdr = {
		.minorversion = args->client->cl_minorversion,
	};

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_compound_hdr(&xdr, req, &hdr);
	encode_exchange_id(&xdr, args, &hdr);
	encode_nops(&hdr);
	return 0;
}

/*
 * a CREATE_SESSION request
 */
static int nfs4_xdr_enc_create_session(struct rpc_rqst *req, uint32_t *p,
				       struct nfs41_create_session_args *args)
{
	struct xdr_stream xdr;
	struct compound_hdr hdr = {
		.minorversion = args->client->cl_minorversion,
	};

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_compound_hdr(&xdr, req, &hdr);
	encode_create_session(&xdr, args, &hdr);
	encode_nops(&hdr);
	return 0;
}

/*
 * a DESTROY_SESSION request
 */
static int nfs4_xdr_enc_destroy_session(struct rpc_rqst *req, uint32_t *p,
					struct nfs4_session *session)
{
	struct xdr_stream xdr;
	struct compound_hdr hdr = {
		.minorversion = session->clp->cl_minorversion,
	};

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_compound_hdr(&xdr, req, &hdr);
	encode_destroy_session(&xdr, session, &hdr);
	encode_nops(&hdr);
	return 0;
}

/*
 * a SEQUENCE request
 */
static int nfs4_xdr_enc_sequence(struct rpc_rqst *req, uint32_t *p,
				 struct nfs4_sequence_args *args)
{
	struct xdr_stream xdr;
	struct compound_hdr hdr = {
		.minorversion = nfs4_xdr_minorversion(args),
	};

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_compound_hdr(&xdr, req, &hdr);
	encode_sequence(&xdr, args, &hdr);
	encode_nops(&hdr);
	return 0;
}

/*
 * a GET_LEASE_TIME request
 */
static int nfs4_xdr_enc_get_lease_time(struct rpc_rqst *req, uint32_t *p,
				       struct nfs4_get_lease_time_args *args)
{
	struct xdr_stream xdr;
	struct compound_hdr hdr = {
		.minorversion = nfs4_xdr_minorversion(&args->la_seq_args),
	};
	const u32 lease_bitmap[2] = { FATTR4_WORD0_LEASE_TIME, 0 };

	xdr_init_encode(&xdr, &req->rq_snd_buf, p);
	encode_compound_hdr(&xdr, req, &hdr);
	encode_sequence(&xdr, &args->la_seq_args, &hdr);
	encode_putrootfh(&xdr, &hdr);
	encode_fsinfo(&xdr, lease_bitmap, &hdr);
	encode_nops(&hdr);
	return 0;
}
#endif /* CONFIG_NFS_V4_1 */

static void print_overflow_msg(const char *func, const struct xdr_stream *xdr)
{
	dprintk("nfs: %s: prematurely hit end of receive buffer. "
		"Remaining buffer length is %tu words.\n",
		func, xdr->end - xdr->p);
}

static int decode_opaque_inline(struct xdr_stream *xdr, unsigned int *len, char **string)
{
	__be32 *p;

	p = xdr_inline_decode(xdr, 4);
	if (unlikely(!p))
		goto out_overflow;
	*len = be32_to_cpup(p);
	p = xdr_inline_decode(xdr, *len);
	if (unlikely(!p))
		goto out_overflow;
	*string = (char *)p;
	return 0;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static int decode_compound_hdr(struct xdr_stream *xdr, struct compound_hdr *hdr)
{
	__be32 *p;

	p = xdr_inline_decode(xdr, 8);
	if (unlikely(!p))
		goto out_overflow;
	hdr->status = be32_to_cpup(p++);
	hdr->taglen = be32_to_cpup(p);

	p = xdr_inline_decode(xdr, hdr->taglen + 4);
	if (unlikely(!p))
		goto out_overflow;
	hdr->tag = (char *)p;
	p += XDR_QUADLEN(hdr->taglen);
	hdr->nops = be32_to_cpup(p);
	if (unlikely(hdr->nops < 1))
		return nfs4_stat_to_errno(hdr->status);
	return 0;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static int decode_op_hdr(struct xdr_stream *xdr, enum nfs_opnum4 expected)
{
	__be32 *p;
	uint32_t opnum;
	int32_t nfserr;

	p = xdr_inline_decode(xdr, 8);
	if (unlikely(!p))
		goto out_overflow;
	opnum = be32_to_cpup(p++);
	if (opnum != expected) {
		dprintk("nfs: Server returned operation"
			" %d but we issued a request for %d\n",
				opnum, expected);
		return -EIO;
	}
	nfserr = be32_to_cpup(p);
	if (nfserr != NFS_OK)
		return nfs4_stat_to_errno(nfserr);
	return 0;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

/* Dummy routine */
static int decode_ace(struct xdr_stream *xdr, void *ace, struct nfs_client *clp)
{
	__be32 *p;
	unsigned int strlen;
	char *str;

	p = xdr_inline_decode(xdr, 12);
	if (likely(p))
		return decode_opaque_inline(xdr, &strlen, &str);
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static int decode_attr_bitmap(struct xdr_stream *xdr, uint32_t *bitmap)
{
	uint32_t bmlen;
	__be32 *p;

	p = xdr_inline_decode(xdr, 4);
	if (unlikely(!p))
		goto out_overflow;
	bmlen = be32_to_cpup(p);

	bitmap[0] = bitmap[1] = 0;
	p = xdr_inline_decode(xdr, (bmlen << 2));
	if (unlikely(!p))
		goto out_overflow;
	if (bmlen > 0) {
		bitmap[0] = be32_to_cpup(p++);
		if (bmlen > 1)
			bitmap[1] = be32_to_cpup(p);
	}
	return 0;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static inline int decode_attr_length(struct xdr_stream *xdr, uint32_t *attrlen, __be32 **savep)
{
	__be32 *p;

	p = xdr_inline_decode(xdr, 4);
	if (unlikely(!p))
		goto out_overflow;
	*attrlen = be32_to_cpup(p);
	*savep = xdr->p;
	return 0;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static int decode_attr_supported(struct xdr_stream *xdr, uint32_t *bitmap, uint32_t *bitmask)
{
	if (likely(bitmap[0] & FATTR4_WORD0_SUPPORTED_ATTRS)) {
		decode_attr_bitmap(xdr, bitmask);
		bitmap[0] &= ~FATTR4_WORD0_SUPPORTED_ATTRS;
	} else
		bitmask[0] = bitmask[1] = 0;
	dprintk("%s: bitmask=%08x:%08x\n", __func__, bitmask[0], bitmask[1]);
	return 0;
}

static int decode_attr_type(struct xdr_stream *xdr, uint32_t *bitmap, uint32_t *type)
{
	__be32 *p;
	int ret = 0;

	*type = 0;
	if (unlikely(bitmap[0] & (FATTR4_WORD0_TYPE - 1U)))
		return -EIO;
	if (likely(bitmap[0] & FATTR4_WORD0_TYPE)) {
		p = xdr_inline_decode(xdr, 4);
		if (unlikely(!p))
			goto out_overflow;
		*type = be32_to_cpup(p);
		if (*type < NF4REG || *type > NF4NAMEDATTR) {
			dprintk("%s: bad type %d\n", __func__, *type);
			return -EIO;
		}
		bitmap[0] &= ~FATTR4_WORD0_TYPE;
		ret = NFS_ATTR_FATTR_TYPE;
	}
	dprintk("%s: type=0%o\n", __func__, nfs_type2fmt[*type]);
	return ret;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static int decode_attr_change(struct xdr_stream *xdr, uint32_t *bitmap, uint64_t *change)
{
	__be32 *p;
	int ret = 0;

	*change = 0;
	if (unlikely(bitmap[0] & (FATTR4_WORD0_CHANGE - 1U)))
		return -EIO;
	if (likely(bitmap[0] & FATTR4_WORD0_CHANGE)) {
		p = xdr_inline_decode(xdr, 8);
		if (unlikely(!p))
			goto out_overflow;
		xdr_decode_hyper(p, change);
		bitmap[0] &= ~FATTR4_WORD0_CHANGE;
		ret = NFS_ATTR_FATTR_CHANGE;
	}
	dprintk("%s: change attribute=%Lu\n", __func__,
			(unsigned long long)*change);
	return ret;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static int decode_attr_size(struct xdr_stream *xdr, uint32_t *bitmap, uint64_t *size)
{
	__be32 *p;
	int ret = 0;

	*size = 0;
	if (unlikely(bitmap[0] & (FATTR4_WORD0_SIZE - 1U)))
		return -EIO;
	if (likely(bitmap[0] & FATTR4_WORD0_SIZE)) {
		p = xdr_inline_decode(xdr, 8);
		if (unlikely(!p))
			goto out_overflow;
		xdr_decode_hyper(p, size);
		bitmap[0] &= ~FATTR4_WORD0_SIZE;
		ret = NFS_ATTR_FATTR_SIZE;
	}
	dprintk("%s: file size=%Lu\n", __func__, (unsigned long long)*size);
	return ret;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static int decode_attr_link_support(struct xdr_stream *xdr, uint32_t *bitmap, uint32_t *res)
{
	__be32 *p;

	*res = 0;
	if (unlikely(bitmap[0] & (FATTR4_WORD0_LINK_SUPPORT - 1U)))
		return -EIO;
	if (likely(bitmap[0] & FATTR4_WORD0_LINK_SUPPORT)) {
		p = xdr_inline_decode(xdr, 4);
		if (unlikely(!p))
			goto out_overflow;
		*res = be32_to_cpup(p);
		bitmap[0] &= ~FATTR4_WORD0_LINK_SUPPORT;
	}
	dprintk("%s: link support=%s\n", __func__, *res == 0 ? "false" : "true");
	return 0;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static int decode_attr_symlink_support(struct xdr_stream *xdr, uint32_t *bitmap, uint32_t *res)
{
	__be32 *p;

	*res = 0;
	if (unlikely(bitmap[0] & (FATTR4_WORD0_SYMLINK_SUPPORT - 1U)))
		return -EIO;
	if (likely(bitmap[0] & FATTR4_WORD0_SYMLINK_SUPPORT)) {
		p = xdr_inline_decode(xdr, 4);
		if (unlikely(!p))
			goto out_overflow;
		*res = be32_to_cpup(p);
		bitmap[0] &= ~FATTR4_WORD0_SYMLINK_SUPPORT;
	}
	dprintk("%s: symlink support=%s\n", __func__, *res == 0 ? "false" : "true");
	return 0;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static int decode_attr_fsid(struct xdr_stream *xdr, uint32_t *bitmap, struct nfs_fsid *fsid)
{
	__be32 *p;
	int ret = 0;

	fsid->major = 0;
	fsid->minor = 0;
	if (unlikely(bitmap[0] & (FATTR4_WORD0_FSID - 1U)))
		return -EIO;
	if (likely(bitmap[0] & FATTR4_WORD0_FSID)) {
		p = xdr_inline_decode(xdr, 16);
		if (unlikely(!p))
			goto out_overflow;
		p = xdr_decode_hyper(p, &fsid->major);
		xdr_decode_hyper(p, &fsid->minor);
		bitmap[0] &= ~FATTR4_WORD0_FSID;
		ret = NFS_ATTR_FATTR_FSID;
	}
	dprintk("%s: fsid=(0x%Lx/0x%Lx)\n", __func__,
			(unsigned long long)fsid->major,
			(unsigned long long)fsid->minor);
	return ret;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static int decode_attr_lease_time(struct xdr_stream *xdr, uint32_t *bitmap, uint32_t *res)
{
	__be32 *p;

	*res = 60;
	if (unlikely(bitmap[0] & (FATTR4_WORD0_LEASE_TIME - 1U)))
		return -EIO;
	if (likely(bitmap[0] & FATTR4_WORD0_LEASE_TIME)) {
		p = xdr_inline_decode(xdr, 4);
		if (unlikely(!p))
			goto out_overflow;
		*res = be32_to_cpup(p);
		bitmap[0] &= ~FATTR4_WORD0_LEASE_TIME;
	}
	dprintk("%s: file size=%u\n", __func__, (unsigned int)*res);
	return 0;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static int decode_attr_aclsupport(struct xdr_stream *xdr, uint32_t *bitmap, uint32_t *res)
{
	__be32 *p;

	*res = ACL4_SUPPORT_ALLOW_ACL|ACL4_SUPPORT_DENY_ACL;
	if (unlikely(bitmap[0] & (FATTR4_WORD0_ACLSUPPORT - 1U)))
		return -EIO;
	if (likely(bitmap[0] & FATTR4_WORD0_ACLSUPPORT)) {
		p = xdr_inline_decode(xdr, 4);
		if (unlikely(!p))
			goto out_overflow;
		*res = be32_to_cpup(p);
		bitmap[0] &= ~FATTR4_WORD0_ACLSUPPORT;
	}
	dprintk("%s: ACLs supported=%u\n", __func__, (unsigned int)*res);
	return 0;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static int decode_attr_fileid(struct xdr_stream *xdr, uint32_t *bitmap, uint64_t *fileid)
{
	__be32 *p;
	int ret = 0;

	*fileid = 0;
	if (unlikely(bitmap[0] & (FATTR4_WORD0_FILEID - 1U)))
		return -EIO;
	if (likely(bitmap[0] & FATTR4_WORD0_FILEID)) {
		p = xdr_inline_decode(xdr, 8);
		if (unlikely(!p))
			goto out_overflow;
		xdr_decode_hyper(p, fileid);
		bitmap[0] &= ~FATTR4_WORD0_FILEID;
		ret = NFS_ATTR_FATTR_FILEID;
	}
	dprintk("%s: fileid=%Lu\n", __func__, (unsigned long long)*fileid);
	return ret;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static int decode_attr_mounted_on_fileid(struct xdr_stream *xdr, uint32_t *bitmap, uint64_t *fileid)
{
	__be32 *p;
	int ret = 0;

	*fileid = 0;
	if (unlikely(bitmap[1] & (FATTR4_WORD1_MOUNTED_ON_FILEID - 1U)))
		return -EIO;
	if (likely(bitmap[1] & FATTR4_WORD1_MOUNTED_ON_FILEID)) {
		p = xdr_inline_decode(xdr, 8);
		if (unlikely(!p))
			goto out_overflow;
		xdr_decode_hyper(p, fileid);
		bitmap[1] &= ~FATTR4_WORD1_MOUNTED_ON_FILEID;
		ret = NFS_ATTR_FATTR_FILEID;
	}
	dprintk("%s: fileid=%Lu\n", __func__, (unsigned long long)*fileid);
	return ret;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static int decode_attr_files_avail(struct xdr_stream *xdr, uint32_t *bitmap, uint64_t *res)
{
	__be32 *p;
	int status = 0;

	*res = 0;
	if (unlikely(bitmap[0] & (FATTR4_WORD0_FILES_AVAIL - 1U)))
		return -EIO;
	if (likely(bitmap[0] & FATTR4_WORD0_FILES_AVAIL)) {
		p = xdr_inline_decode(xdr, 8);
		if (unlikely(!p))
			goto out_overflow;
		xdr_decode_hyper(p, res);
		bitmap[0] &= ~FATTR4_WORD0_FILES_AVAIL;
	}
	dprintk("%s: files avail=%Lu\n", __func__, (unsigned long long)*res);
	return status;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static int decode_attr_files_free(struct xdr_stream *xdr, uint32_t *bitmap, uint64_t *res)
{
	__be32 *p;
	int status = 0;

	*res = 0;
	if (unlikely(bitmap[0] & (FATTR4_WORD0_FILES_FREE - 1U)))
		return -EIO;
	if (likely(bitmap[0] & FATTR4_WORD0_FILES_FREE)) {
		p = xdr_inline_decode(xdr, 8);
		if (unlikely(!p))
			goto out_overflow;
		xdr_decode_hyper(p, res);
		bitmap[0] &= ~FATTR4_WORD0_FILES_FREE;
	}
	dprintk("%s: files free=%Lu\n", __func__, (unsigned long long)*res);
	return status;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static int decode_attr_files_total(struct xdr_stream *xdr, uint32_t *bitmap, uint64_t *res)
{
	__be32 *p;
	int status = 0;

	*res = 0;
	if (unlikely(bitmap[0] & (FATTR4_WORD0_FILES_TOTAL - 1U)))
		return -EIO;
	if (likely(bitmap[0] & FATTR4_WORD0_FILES_TOTAL)) {
		p = xdr_inline_decode(xdr, 8);
		if (unlikely(!p))
			goto out_overflow;
		xdr_decode_hyper(p, res);
		bitmap[0] &= ~FATTR4_WORD0_FILES_TOTAL;
	}
	dprintk("%s: files total=%Lu\n", __func__, (unsigned long long)*res);
	return status;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static int decode_pathname(struct xdr_stream *xdr, struct nfs4_pathname *path)
{
	u32 n;
	__be32 *p;
	int status = 0;

	p = xdr_inline_decode(xdr, 4);
	if (unlikely(!p))
		goto out_overflow;
	n = be32_to_cpup(p);
	if (n == 0)
		goto root_path;
	dprintk("path ");
	path->ncomponents = 0;
	while (path->ncomponents < n) {
		struct nfs4_string *component = &path->components[path->ncomponents];
		status = decode_opaque_inline(xdr, &component->len, &component->data);
		if (unlikely(status != 0))
			goto out_eio;
		if (path->ncomponents != n)
			dprintk("/");
		dprintk("%s", component->data);
		if (path->ncomponents < NFS4_PATHNAME_MAXCOMPONENTS)
			path->ncomponents++;
		else {
			dprintk("cannot parse %d components in path\n", n);
			goto out_eio;
		}
	}
out:
	dprintk("\n");
	return status;
root_path:
/* a root pathname is sent as a zero component4 */
	path->ncomponents = 1;
	path->components[0].len=0;
	path->components[0].data=NULL;
	dprintk("path /\n");
	goto out;
out_eio:
	dprintk(" status %d", status);
	status = -EIO;
	goto out;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static int decode_attr_fs_locations(struct xdr_stream *xdr, uint32_t *bitmap, struct nfs4_fs_locations *res)
{
	int n;
	__be32 *p;
	int status = -EIO;

	if (unlikely(bitmap[0] & (FATTR4_WORD0_FS_LOCATIONS -1U)))
		goto out;
	status = 0;
	if (unlikely(!(bitmap[0] & FATTR4_WORD0_FS_LOCATIONS)))
		goto out;
	dprintk("%s: fsroot ", __func__);
	status = decode_pathname(xdr, &res->fs_path);
	if (unlikely(status != 0))
		goto out;
	p = xdr_inline_decode(xdr, 4);
	if (unlikely(!p))
		goto out_overflow;
	n = be32_to_cpup(p);
	if (n <= 0)
		goto out_eio;
	res->nlocations = 0;
	while (res->nlocations < n) {
		u32 m;
		struct nfs4_fs_location *loc = &res->locations[res->nlocations];

		p = xdr_inline_decode(xdr, 4);
		if (unlikely(!p))
			goto out_overflow;
		m = be32_to_cpup(p);

		loc->nservers = 0;
		dprintk("%s: servers ", __func__);
		while (loc->nservers < m) {
			struct nfs4_string *server = &loc->servers[loc->nservers];
			status = decode_opaque_inline(xdr, &server->len, &server->data);
			if (unlikely(status != 0))
				goto out_eio;
			dprintk("%s ", server->data);
			if (loc->nservers < NFS4_FS_LOCATION_MAXSERVERS)
				loc->nservers++;
			else {
				unsigned int i;
				dprintk("%s: using first %u of %u servers "
					"returned for location %u\n",
						__func__,
						NFS4_FS_LOCATION_MAXSERVERS,
						m, res->nlocations);
				for (i = loc->nservers; i < m; i++) {
					unsigned int len;
					char *data;
					status = decode_opaque_inline(xdr, &len, &data);
					if (unlikely(status != 0))
						goto out_eio;
				}
			}
		}
		status = decode_pathname(xdr, &loc->rootpath);
		if (unlikely(status != 0))
			goto out_eio;
		if (res->nlocations < NFS4_FS_LOCATIONS_MAXENTRIES)
			res->nlocations++;
	}
	if (res->nlocations != 0)
		status = NFS_ATTR_FATTR_V4_REFERRAL;
out:
	dprintk("%s: fs_locations done, error = %d\n", __func__, status);
	return status;
out_overflow:
	print_overflow_msg(__func__, xdr);
out_eio:
	status = -EIO;
	goto out;
}

static int decode_attr_maxfilesize(struct xdr_stream *xdr, uint32_t *bitmap, uint64_t *res)
{
	__be32 *p;
	int status = 0;

	*res = 0;
	if (unlikely(bitmap[0] & (FATTR4_WORD0_MAXFILESIZE - 1U)))
		return -EIO;
	if (likely(bitmap[0] & FATTR4_WORD0_MAXFILESIZE)) {
		p = xdr_inline_decode(xdr, 8);
		if (unlikely(!p))
			goto out_overflow;
		xdr_decode_hyper(p, res);
		bitmap[0] &= ~FATTR4_WORD0_MAXFILESIZE;
	}
	dprintk("%s: maxfilesize=%Lu\n", __func__, (unsigned long long)*res);
	return status;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static int decode_attr_maxlink(struct xdr_stream *xdr, uint32_t *bitmap, uint32_t *maxlink)
{
	__be32 *p;
	int status = 0;

	*maxlink = 1;
	if (unlikely(bitmap[0] & (FATTR4_WORD0_MAXLINK - 1U)))
		return -EIO;
	if (likely(bitmap[0] & FATTR4_WORD0_MAXLINK)) {
		p = xdr_inline_decode(xdr, 4);
		if (unlikely(!p))
			goto out_overflow;
		*maxlink = be32_to_cpup(p);
		bitmap[0] &= ~FATTR4_WORD0_MAXLINK;
	}
	dprintk("%s: maxlink=%u\n", __func__, *maxlink);
	return status;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static int decode_attr_maxname(struct xdr_stream *xdr, uint32_t *bitmap, uint32_t *maxname)
{
	__be32 *p;
	int status = 0;

	*maxname = 1024;
	if (unlikely(bitmap[0] & (FATTR4_WORD0_MAXNAME - 1U)))
		return -EIO;
	if (likely(bitmap[0] & FATTR4_WORD0_MAXNAME)) {
		p = xdr_inline_decode(xdr, 4);
		if (unlikely(!p))
			goto out_overflow;
		*maxname = be32_to_cpup(p);
		bitmap[0] &= ~FATTR4_WORD0_MAXNAME;
	}
	dprintk("%s: maxname=%u\n", __func__, *maxname);
	return status;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static int decode_attr_maxread(struct xdr_stream *xdr, uint32_t *bitmap, uint32_t *res)
{
	__be32 *p;
	int status = 0;

	*res = 1024;
	if (unlikely(bitmap[0] & (FATTR4_WORD0_MAXREAD - 1U)))
		return -EIO;
	if (likely(bitmap[0] & FATTR4_WORD0_MAXREAD)) {
		uint64_t maxread;
		p = xdr_inline_decode(xdr, 8);
		if (unlikely(!p))
			goto out_overflow;
		xdr_decode_hyper(p, &maxread);
		if (maxread > 0x7FFFFFFF)
			maxread = 0x7FFFFFFF;
		*res = (uint32_t)maxread;
		bitmap[0] &= ~FATTR4_WORD0_MAXREAD;
	}
	dprintk("%s: maxread=%lu\n", __func__, (unsigned long)*res);
	return status;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static int decode_attr_maxwrite(struct xdr_stream *xdr, uint32_t *bitmap, uint32_t *res)
{
	__be32 *p;
	int status = 0;

	*res = 1024;
	if (unlikely(bitmap[0] & (FATTR4_WORD0_MAXWRITE - 1U)))
		return -EIO;
	if (likely(bitmap[0] & FATTR4_WORD0_MAXWRITE)) {
		uint64_t maxwrite;
		p = xdr_inline_decode(xdr, 8);
		if (unlikely(!p))
			goto out_overflow;
		xdr_decode_hyper(p, &maxwrite);
		if (maxwrite > 0x7FFFFFFF)
			maxwrite = 0x7FFFFFFF;
		*res = (uint32_t)maxwrite;
		bitmap[0] &= ~FATTR4_WORD0_MAXWRITE;
	}
	dprintk("%s: maxwrite=%lu\n", __func__, (unsigned long)*res);
	return status;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static int decode_attr_mode(struct xdr_stream *xdr, uint32_t *bitmap, umode_t *mode)
{
	uint32_t tmp;
	__be32 *p;
	int ret = 0;

	*mode = 0;
	if (unlikely(bitmap[1] & (FATTR4_WORD1_MODE - 1U)))
		return -EIO;
	if (likely(bitmap[1] & FATTR4_WORD1_MODE)) {
		p = xdr_inline_decode(xdr, 4);
		if (unlikely(!p))
			goto out_overflow;
		tmp = be32_to_cpup(p);
		*mode = tmp & ~S_IFMT;
		bitmap[1] &= ~FATTR4_WORD1_MODE;
		ret = NFS_ATTR_FATTR_MODE;
	}
	dprintk("%s: file mode=0%o\n", __func__, (unsigned int)*mode);
	return ret;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static int decode_attr_nlink(struct xdr_stream *xdr, uint32_t *bitmap, uint32_t *nlink)
{
	__be32 *p;
	int ret = 0;

	*nlink = 1;
	if (unlikely(bitmap[1] & (FATTR4_WORD1_NUMLINKS - 1U)))
		return -EIO;
	if (likely(bitmap[1] & FATTR4_WORD1_NUMLINKS)) {
		p = xdr_inline_decode(xdr, 4);
		if (unlikely(!p))
			goto out_overflow;
		*nlink = be32_to_cpup(p);
		bitmap[1] &= ~FATTR4_WORD1_NUMLINKS;
		ret = NFS_ATTR_FATTR_NLINK;
	}
	dprintk("%s: nlink=%u\n", __func__, (unsigned int)*nlink);
	return ret;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static int decode_attr_owner(struct xdr_stream *xdr, uint32_t *bitmap,
		struct nfs_client *clp, uint32_t *uid, int may_sleep)
{
	uint32_t len;
	__be32 *p;
	int ret = 0;

	*uid = -2;
	if (unlikely(bitmap[1] & (FATTR4_WORD1_OWNER - 1U)))
		return -EIO;
	if (likely(bitmap[1] & FATTR4_WORD1_OWNER)) {
		p = xdr_inline_decode(xdr, 4);
		if (unlikely(!p))
			goto out_overflow;
		len = be32_to_cpup(p);
		p = xdr_inline_decode(xdr, len);
		if (unlikely(!p))
			goto out_overflow;
		if (!may_sleep) {
			/* do nothing */
		} else if (len < XDR_MAX_NETOBJ) {
			if (nfs_map_name_to_uid(clp, (char *)p, len, uid) == 0)
				ret = NFS_ATTR_FATTR_OWNER;
			else
				dprintk("%s: nfs_map_name_to_uid failed!\n",
						__func__);
		} else
			dprintk("%s: name too long (%u)!\n",
					__func__, len);
		bitmap[1] &= ~FATTR4_WORD1_OWNER;
	}
	dprintk("%s: uid=%d\n", __func__, (int)*uid);
	return ret;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static int decode_attr_group(struct xdr_stream *xdr, uint32_t *bitmap,
		struct nfs_client *clp, uint32_t *gid, int may_sleep)
{
	uint32_t len;
	__be32 *p;
	int ret = 0;

	*gid = -2;
	if (unlikely(bitmap[1] & (FATTR4_WORD1_OWNER_GROUP - 1U)))
		return -EIO;
	if (likely(bitmap[1] & FATTR4_WORD1_OWNER_GROUP)) {
		p = xdr_inline_decode(xdr, 4);
		if (unlikely(!p))
			goto out_overflow;
		len = be32_to_cpup(p);
		p = xdr_inline_decode(xdr, len);
		if (unlikely(!p))
			goto out_overflow;
		if (!may_sleep) {
			/* do nothing */
		} else if (len < XDR_MAX_NETOBJ) {
			if (nfs_map_group_to_gid(clp, (char *)p, len, gid) == 0)
				ret = NFS_ATTR_FATTR_GROUP;
			else
				dprintk("%s: nfs_map_group_to_gid failed!\n",
						__func__);
		} else
			dprintk("%s: name too long (%u)!\n",
					__func__, len);
		bitmap[1] &= ~FATTR4_WORD1_OWNER_GROUP;
	}
	dprintk("%s: gid=%d\n", __func__, (int)*gid);
	return ret;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static int decode_attr_rdev(struct xdr_stream *xdr, uint32_t *bitmap, dev_t *rdev)
{
	uint32_t major = 0, minor = 0;
	__be32 *p;
	int ret = 0;

	*rdev = MKDEV(0,0);
	if (unlikely(bitmap[1] & (FATTR4_WORD1_RAWDEV - 1U)))
		return -EIO;
	if (likely(bitmap[1] & FATTR4_WORD1_RAWDEV)) {
		dev_t tmp;

		p = xdr_inline_decode(xdr, 8);
		if (unlikely(!p))
			goto out_overflow;
		major = be32_to_cpup(p++);
		minor = be32_to_cpup(p);
		tmp = MKDEV(major, minor);
		if (MAJOR(tmp) == major && MINOR(tmp) == minor)
			*rdev = tmp;
		bitmap[1] &= ~ FATTR4_WORD1_RAWDEV;
		ret = NFS_ATTR_FATTR_RDEV;
	}
	dprintk("%s: rdev=(0x%x:0x%x)\n", __func__, major, minor);
	return ret;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static int decode_attr_space_avail(struct xdr_stream *xdr, uint32_t *bitmap, uint64_t *res)
{
	__be32 *p;
	int status = 0;

	*res = 0;
	if (unlikely(bitmap[1] & (FATTR4_WORD1_SPACE_AVAIL - 1U)))
		return -EIO;
	if (likely(bitmap[1] & FATTR4_WORD1_SPACE_AVAIL)) {
		p = xdr_inline_decode(xdr, 8);
		if (unlikely(!p))
			goto out_overflow;
		xdr_decode_hyper(p, res);
		bitmap[1] &= ~FATTR4_WORD1_SPACE_AVAIL;
	}
	dprintk("%s: space avail=%Lu\n", __func__, (unsigned long long)*res);
	return status;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static int decode_attr_space_free(struct xdr_stream *xdr, uint32_t *bitmap, uint64_t *res)
{
	__be32 *p;
	int status = 0;

	*res = 0;
	if (unlikely(bitmap[1] & (FATTR4_WORD1_SPACE_FREE - 1U)))
		return -EIO;
	if (likely(bitmap[1] & FATTR4_WORD1_SPACE_FREE)) {
		p = xdr_inline_decode(xdr, 8);
		if (unlikely(!p))
			goto out_overflow;
		xdr_decode_hyper(p, res);
		bitmap[1] &= ~FATTR4_WORD1_SPACE_FREE;
	}
	dprintk("%s: space free=%Lu\n", __func__, (unsigned long long)*res);
	return status;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static int decode_attr_space_total(struct xdr_stream *xdr, uint32_t *bitmap, uint64_t *res)
{
	__be32 *p;
	int status = 0;

	*res = 0;
	if (unlikely(bitmap[1] & (FATTR4_WORD1_SPACE_TOTAL - 1U)))
		return -EIO;
	if (likely(bitmap[1] & FATTR4_WORD1_SPACE_TOTAL)) {
		p = xdr_inline_decode(xdr, 8);
		if (unlikely(!p))
			goto out_overflow;
		xdr_decode_hyper(p, res);
		bitmap[1] &= ~FATTR4_WORD1_SPACE_TOTAL;
	}
	dprintk("%s: space total=%Lu\n", __func__, (unsigned long long)*res);
	return status;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static int decode_attr_space_used(struct xdr_stream *xdr, uint32_t *bitmap, uint64_t *used)
{
	__be32 *p;
	int ret = 0;

	*used = 0;
	if (unlikely(bitmap[1] & (FATTR4_WORD1_SPACE_USED - 1U)))
		return -EIO;
	if (likely(bitmap[1] & FATTR4_WORD1_SPACE_USED)) {
		p = xdr_inline_decode(xdr, 8);
		if (unlikely(!p))
			goto out_overflow;
		xdr_decode_hyper(p, used);
		bitmap[1] &= ~FATTR4_WORD1_SPACE_USED;
		ret = NFS_ATTR_FATTR_SPACE_USED;
	}
	dprintk("%s: space used=%Lu\n", __func__,
			(unsigned long long)*used);
	return ret;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static int decode_attr_time(struct xdr_stream *xdr, struct timespec *time)
{
	__be32 *p;
	uint64_t sec;
	uint32_t nsec;

	p = xdr_inline_decode(xdr, 12);
	if (unlikely(!p))
		goto out_overflow;
	p = xdr_decode_hyper(p, &sec);
	nsec = be32_to_cpup(p);
	time->tv_sec = (time_t)sec;
	time->tv_nsec = (long)nsec;
	return 0;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static int decode_attr_time_access(struct xdr_stream *xdr, uint32_t *bitmap, struct timespec *time)
{
	int status = 0;

	time->tv_sec = 0;
	time->tv_nsec = 0;
	if (unlikely(bitmap[1] & (FATTR4_WORD1_TIME_ACCESS - 1U)))
		return -EIO;
	if (likely(bitmap[1] & FATTR4_WORD1_TIME_ACCESS)) {
		status = decode_attr_time(xdr, time);
		if (status == 0)
			status = NFS_ATTR_FATTR_ATIME;
		bitmap[1] &= ~FATTR4_WORD1_TIME_ACCESS;
	}
	dprintk("%s: atime=%ld\n", __func__, (long)time->tv_sec);
	return status;
}

static int decode_attr_time_metadata(struct xdr_stream *xdr, uint32_t *bitmap, struct timespec *time)
{
	int status = 0;

	time->tv_sec = 0;
	time->tv_nsec = 0;
	if (unlikely(bitmap[1] & (FATTR4_WORD1_TIME_METADATA - 1U)))
		return -EIO;
	if (likely(bitmap[1] & FATTR4_WORD1_TIME_METADATA)) {
		status = decode_attr_time(xdr, time);
		if (status == 0)
			status = NFS_ATTR_FATTR_CTIME;
		bitmap[1] &= ~FATTR4_WORD1_TIME_METADATA;
	}
	dprintk("%s: ctime=%ld\n", __func__, (long)time->tv_sec);
	return status;
}

static int decode_attr_time_modify(struct xdr_stream *xdr, uint32_t *bitmap, struct timespec *time)
{
	int status = 0;

	time->tv_sec = 0;
	time->tv_nsec = 0;
	if (unlikely(bitmap[1] & (FATTR4_WORD1_TIME_MODIFY - 1U)))
		return -EIO;
	if (likely(bitmap[1] & FATTR4_WORD1_TIME_MODIFY)) {
		status = decode_attr_time(xdr, time);
		if (status == 0)
			status = NFS_ATTR_FATTR_MTIME;
		bitmap[1] &= ~FATTR4_WORD1_TIME_MODIFY;
	}
	dprintk("%s: mtime=%ld\n", __func__, (long)time->tv_sec);
	return status;
}

static int verify_attr_len(struct xdr_stream *xdr, __be32 *savep, uint32_t attrlen)
{
	unsigned int attrwords = XDR_QUADLEN(attrlen);
	unsigned int nwords = xdr->p - savep;

	if (unlikely(attrwords != nwords)) {
		dprintk("%s: server returned incorrect attribute length: "
			"%u %c %u\n",
				__func__,
				attrwords << 2,
				(attrwords < nwords) ? '<' : '>',
				nwords << 2);
		return -EIO;
	}
	return 0;
}

static int decode_change_info(struct xdr_stream *xdr, struct nfs4_change_info *cinfo)
{
	__be32 *p;

	p = xdr_inline_decode(xdr, 20);
	if (unlikely(!p))
		goto out_overflow;
	cinfo->atomic = be32_to_cpup(p++);
	p = xdr_decode_hyper(p, &cinfo->before);
	xdr_decode_hyper(p, &cinfo->after);
	return 0;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static int decode_access(struct xdr_stream *xdr, struct nfs4_accessres *access)
{
	__be32 *p;
	uint32_t supp, acc;
	int status;

	status = decode_op_hdr(xdr, OP_ACCESS);
	if (status)
		return status;
	p = xdr_inline_decode(xdr, 8);
	if (unlikely(!p))
		goto out_overflow;
	supp = be32_to_cpup(p++);
	acc = be32_to_cpup(p);
	access->supported = supp;
	access->access = acc;
	return 0;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static int decode_opaque_fixed(struct xdr_stream *xdr, void *buf, size_t len)
{
	__be32 *p;

	p = xdr_inline_decode(xdr, len);
	if (likely(p)) {
		memcpy(buf, p, len);
		return 0;
	}
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static int decode_stateid(struct xdr_stream *xdr, nfs4_stateid *stateid)
{
	return decode_opaque_fixed(xdr, stateid->data, NFS4_STATEID_SIZE);
}

static int decode_close(struct xdr_stream *xdr, struct nfs_closeres *res)
{
	int status;

	status = decode_op_hdr(xdr, OP_CLOSE);
	if (status != -EIO)
		nfs_increment_open_seqid(status, res->seqid);
	if (!status)
		status = decode_stateid(xdr, &res->stateid);
	return status;
}

static int decode_verifier(struct xdr_stream *xdr, void *verifier)
{
	return decode_opaque_fixed(xdr, verifier, 8);
}

static int decode_commit(struct xdr_stream *xdr, struct nfs_writeres *res)
{
	int status;

	status = decode_op_hdr(xdr, OP_COMMIT);
	if (!status)
		status = decode_verifier(xdr, res->verf->verifier);
	return status;
}

static int decode_create(struct xdr_stream *xdr, struct nfs4_change_info *cinfo)
{
	__be32 *p;
	uint32_t bmlen;
	int status;

	status = decode_op_hdr(xdr, OP_CREATE);
	if (status)
		return status;
	if ((status = decode_change_info(xdr, cinfo)))
		return status;
	p = xdr_inline_decode(xdr, 4);
	if (unlikely(!p))
		goto out_overflow;
	bmlen = be32_to_cpup(p);
	p = xdr_inline_decode(xdr, bmlen << 2);
	if (likely(p))
		return 0;
out_overflow:
	print_overflow_msg(__func__, xdr);
	return -EIO;
}

static int decode_server_caps(struct xdr_stream *xdr, struct nfs4_server_caps_res *res)
{
	__be32 *savep;
	uint32_t attrlen, bitmap[2] = {0};
	int status;

	if ((status = decode_op_hdr(xdr, OP_GETATTR)) != 0)
		goto xdr_error;
	if ((status = decode_attr_bitmap(xdr, bitmap)) != 0)
		goto xdr_error;
	if ((status = decode_attr_length(xdr, &attrlen, &savep)) != 0)
		goto xdr_error;
	if ((status = decode_attr_supported(xdr, bitmap, res->attr_bitmask)) != 0)
		goto xdr_error;
	if ((status = decode_attr_link_support(xdr, bitmap, &res->has_links)) != 0)
		goto xdr_error;
	if ((status = decode_attr_symlink_support(xdr, bitmap, &res->has_symlinks)) != 0)
		goto xdr_error;
	if ((status = decode_attr_aclsupport(xdr, bitmap, &res->acl_bitmask)) != 0)
		goto xdr_error;
	status = verify_attr_len(xdr, savep, attrlen);
xdr_error:
	dprintk("%s: xdr returned %d!\n", __func__, -status);
	return status;
}

static int decode_statfs(struct xdr_stream *xdr, struct nfs_fsstat *fsstat)
{
	__be32 *savep;
	uint32_t attrlen, bitmap[2] = {0};
	int status;

	if ((status = decode_op_hdr(xdr, OP_GETATTR)) != 0)
		goto xdr_error;
	if ((status = decode_attr_bitmap(xdr, bitmap)) != 0)
		goto xdr_error;
	if ((status = decode_attr_length(xdr, &attrlen, &savep)) != 0)
		goto xdr_error;

	if ((status = decode_attr_files_avail(xdr, bitmap, &fsstat->afiles)) != 0)
		goto xdr_error;
	if ((status = decode_attr_files_free(xdr, bitmap, &fsstat->ffiles)) != 0)
		goto xdr_error;
	if ((status = decode_attr_files_total(xdr, bitmap, &fsstat->tfiles)) != 0)
		goto xdr_error;
	if ((status = decode_attr_space_avail(xdr, bitmap, &fsstat->abytes)) != 0)
		goto xdr_error;
	if ((status = decode_attr_space_free(xdr, bitmap, &fsstat->fbytes)) != 0)
		goto xdr_error;
	if ((status = decode_attr_space_total(xdr, bitmap, &fsstat->tbytes)) != 0)
		goto xdr_error;

	status = verify_attr_len(xdr, savep, s/nflen);
xdr_error:
	dprintk("%s: xdr returned %d!\n", __func__, -status);
	ht (c) he Uni;
}

he Uic int decode_pathconf(structpyri_stream *.c
 *
*  Kennfserved.
 * *rved.
 *)
{
	__be32 *
 *  ;
	uint32_tlient-si, bitmap[2] = {0};
	righMichigan
	if ((he Uni =hts reseop_hdrr.c
 *OP_GETATTR)) != 0)
		gotondricfor Nin sry forms, with or wis/nfsbutionr.c
 *bution, are permitted provided that the following conditionslengthr.c
 *& Redistri&
 *  , are permitted provided thhat the following conditionsmaxlinkre met:
 *
 , &rved.
 *->max_  2., are permitted provided that the following conditionsmaxname Redistributions in binary fot of-sid this list of conditions andorms, wit  fs/nfs/nfs4xdr.c
 *
 *  Client-side XDR for NFSv4.
 *
 *  Copyright (c) 2002 The Regents of the University of Michigan.
 *  All rights resegetfs/nf
 *  Kendrick Smith <kmsmith@umich.e to e * to e,
		consurceh@umich.eserver *specif, righmay_sleepon   <andros@umich.edu>
 *
 *  Redistr
		ibution and use D ``Atypein source and b	umreset fRANT = 0.edu>
 64IES,ileidntation and/oh or without
 *  modification,that thorms, w<in the
 *     documentation and/og conditions
 *  are met:
 *
 *TABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAbove copyright
 *     notice,TABILITY AND FITNESS FOR A PARTICULAR  PURPOSE ARE
 *  DISCLASS O RedistributionSS OCIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL 	 to e-> INCLUDING,ILITY AND Fre pe {
	ES; LOSS OF U|=ich.eSS O2fmt[SS O];
 *  BUSINvalidINTEED
 *  WA}DAMAGES (INCLUDING, BUT chang LIMITED TO, PROS; LOSSCONTRAitionMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSSON ANY THEORY OF
 PURPOSE ARE
 *  DISCLAsizACT, STRICT LIABILITY, TWARCLUDING
 *  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFfsidCT, STRICT LIABILITY, ludePOSSIBILITY OF SUCH DAMAGE.
 */

#include <linux/param.h>
#include <linux/time.h>
#inclIMITE <linux/mm.h>
#include IMITEPOSSIBILITY OF SUCH DAMAGE.
 */

#include <linux/param.h>
#include <linux/time.h>
#inclu_locations Redistributiocontainer_of(rom this linure without4<linux/nfs.h>e <linux to e)CLUDING
 *  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFRANTCT, STRICT LIABIRANTMENT OF
 *  SUBSTITUTE GOODS OR SERVICETA, OR PROFITS; OR
 *  BUSINESS INTEror cCAUSED AND ON ANY THEORY OF
 *  LIABILITY, WHETHER IN n  2. RedistributionS; LOSSes */CLUDING
 *  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFowne
 *  motributiospecif->ch.ecliente <li#define uid,ten permissLUDING
 *  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFgrouare met:
 *
  (u64) out of (NFS4_OPAQUE_LIMIT g>> 2)
 */
#define open_owner_id_maxsz	(1 + 4)
#define lock_owner_id_maxsz	(1 + 4)
#define decode_lordev
#ifdef DEBUG
#define S4_MCLUDING
 *  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTpace_usede <linux/pagemap.h>
#idu.nfs3.LEN(fine open_owner_id_maxsz	(1 + 4)
#define lock_owner_id_maxsz	(1 + 4)
#define decode_lotime_acces>
#include <lin#define aDLEN_stateid_maxsz	(XDR_QUADLEN(NFS4_STATEID_SIZE))
#define encode_verifier_maxsz	(XDR_QUADLEN(metadataCT, STRICT LIABILITY, Odecode_verifier_maxsz	(XDR_QUADLEN(NFS4_VERIFIER_SIZE))
#define encode_putfh_maxsz	(op_encode_hdodifyERIFIER_SIZE))
#definemdecode_verifier_maxsz	(XDR_QUADLEN(NFS4_VERIFIER_SIZE))
#define encode_putfh_maxsz	(op_encmounted_on#include <linux/pagemapclude <linux/proc_fs.h>
#include <linux/kdevTA, OR PROFITS; && !includ IN ANY &HEORY O) OR
 *  BUSIN     ( = LIMITED USED AND ON ANY THEORY OF
 *  LIABILITY,  fs/nfs/nfs4xdr.c
 *
 *  Client-side XDR for NFSv4.
 *
 *  Copyright (c) 2002The Regents of the University of Michigan.
  *  All rights resefsinfondorse or promote products derived
 *ne nf *P_NAMEon   <andros@umich.edu>
 *
 *  Redistribution anin source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions andP_NAME->rtmultode_maxsz +w\
				nf512;	/* ??? */and the following disclaimer.leaseADLENERIFIER_SIZE))
#maxsz +code_hdr_must reproduce the above copyright
 *     notice, this lis    TWARE, EVEN IF ADVISmaxsz + + 2 + 1 + ust reproduce the above copyright
 *     notice, this lisreade <linux/pagemapmaxsz + \
ax, are permitted provided thamaxsz)
#depref	nfs4_fattrdhdr_maxsz)
#defidefinNFS4_FHt
 *     notice, this liswritmaxsz + nfs4_fattr_maxszwefine decode_savefh_maxsz     (op_decodewhdr_maxsz)
#defiop_dentation and/or other materials provided with the distribution.
 *  3. Neither the name of the University nor the names of its
 *     contributors may be used hndorse or promote products derived
 *h *fhon   <andros@h.edu>
 *
 * lenin source and bin/* Zero handle first to allow comparisonse de	memset(fh, 0, TWARof( \
	)F THE USE OF THIS
 *thout
 *  modificaFHfine open_owneermiity of Michigan
	p =ndricies *e_ts resr.c
 *4fine opeunlikely(!p)ermitted out_overflow;
	len = ndro_to_cpup(efine opelien> NFS4_FHSIZEEN) + \
			-EIO (ophF THE  =+ \
			EN(RPCBIND_MAXUADDRLEN) + \-side 	1) /* sc_cb_ident */
#define decode_setcmemcpy() /*axsz, p encode_sity of 0;
ine decode_s:
	.
 *
 decode_s_msg(egents of xdNCLUD2 + \
				102.
 *  All rights rese  2.  *  Kendrick Smith <kmsmith@umich.4 CONTRA_NAMESZc
#define source and binorms, with or without
 *  modifLINKBIND_MAXNETIDLEN) + \
				XDR_QUAaxsz \
	ts resemaxsz + nfsr.c
 *name_mop_de/*
 * We create the rentl, so we know a propers_maxs.idencogth is 4.
e decode_hdr_maxsz)
#deock_deniedITY   Kendrick Smith <kmsmith@umi    efine *flon    BUT NOT offset encogth, (NFS4_attr_op_encode_hdr_maxsz + discla,ESS OR e for CLID_INUSE */
#define e32de_setclientid_confirm_maxsz \
				(op_encodEN(RPCBIts resehyper(E))
maxsz	de_sode_claim_null_maxsz)
#dep_encode_sSS Ontid_maxsz \
				++de_setclflFITSNULL OR
 * l->fl_star		nf(loff_t)maxsz	tr_maeid_maencode_eid_maxsz + + + \
				z	(1 + - 1tr_mdecode_1 + == ~( BUT NOT)permie_maxsz)
#defiOFFSET_MAX_ace_maxsz)#defineF_WRLCKdefine d#defi& 1ecode_hdr_mamaxsz + \RD			decone decopncodeING,}ode_ace_maxsz	(3 + nfs4_own_hdr_maxde_s disclantid_maxsz \
				(op_EN(RPCBIND_MAXUADDRLEN) + \ disclaiop_decodc_cb_ient */2 + \
		_maxERR_DENIEDIER_SIZE >> 2))
#define decode_setclientid_confirm_maxsz \
				(op_decode_hdr_maxsz)
#deocne encode_lookup_maxsz	(op_encode_hdefine res *resmaxsz)
#define decode_lookup_maxsz	(op_decode_hdr_mOCsz)
#define encopen			1t */
#definecess_maxsz)
#defi; OR
 *orms, with or wihe Ulude <lin&res->z + \
	ncod	1) /* sc_cb_ibitmap_mcodee_open_down} else s_maxsz)
#definine decode_opese_morms, with or wifine encoder.c
 *decode_setclode_sopen_seqid1 + decod
		ode_incremeine maxsz + \code_cl, e_hdr_maxsz + \ne enstateid_maxszfine fine encode_setattr+ \
				 e)IER_S:rsity of Michigan.
 *  All rights resee_opten_downgrade_maxsz \
				(op_encode_hdr_tmaxsz + \
				 encode_statid_maxsz + 1 + \
				 encode_share_aTccess_maxsz)
#defin 1 + encode_stateisz \
				(2)
# decode_close_maxode_sencodeversity of Michigan.
 *  All rights resee_opuen_downgrade_maxsz \
				(op_encode_hdr_umaxsz + \
				 encode_stateid_maxsz + 1 + \
				 encode_share_aUfine open_owner!fine decodode_hdr_maxsz + \
				 encode_stateid_z	(op_encgrade_maxsz \
		(op_decode_hdr_maxsz + \
				 decode_stateid_maxs + 2)
#define encode_readdir_maxsz	(opokupclaim_null_maxsz	(1 + non   sz \
				(2)
#				 encode_shareOKUPde_maxsz This enctoo sick!_createmode_maxsz)
#deR_QUADlimiaxsz	(op_decode_hdr_maxsz u64 *max	nfs4			(op_encode_hdr_maxsz + r_ma1 + \, nbe_ops, mmit_mizaxsz + 2 + \
				open_owner_id_m1xsz + \
				encode_opentype_maxsz + \
				encine decodeine decode_delegation_mswitchstate decode OR
 case 1:
		claim_null_maxsz)
#dsz	(op_eaxszbreak;code_re2ove_ommit_mine decode_delegation_m	sz	(op_dentid_maxsz \
				(op_	xsz	(op_ 1 +axsz	(op_info_max* nfs4_name_sz	(op_deco \
	S4_VERIFIER_SIZE >> 2))
#define decode_setclientid_confirm_maxsz \
				(op_decode_hdr_maxsz)
#ddelegnfs.h
 *  Kendrick Smith <kmsmith@umich.e)
#daxsz + \
				op_encode_hdr_maxsz +ne encode_1 + \in source and binEN(RPCBIND_MAXUADDRLEN) + \
				1) /* sc_cb_ident */
#define decode_setcnk_maxsz	(op_dentid_maxsz \
				(op_deco + \
				 7 + \
	=r_maxsOPEN_DELEGATE_NONE OR
 *ecode_h \
				 7 + \
		ING,	(op_decode \
	_decode_hdr_maxsz + \
				 decode_s+ decode_lde_setclientid_coode_close_mshare_access_maxEN(RPCBIND_MAXUADDRLEN) + \
				1) /* sc_cb_ident */
#define decode_setcmaxsz o_recallntid_maxsz \
				(op_name_maxs+ decode_lockow decode_redecode_lock_denied_READove_maxsz + \
				 1 + \
			MODdr_max \
				 decode_chdecode_lock_denied_WRITEsz + 3 + \
				 encode_stateid_masz	(o|eid_maxsz + \
eid_macode_verifier_max		 decode_ssz	(op_eFITNESS 		2 + \
				1024sz	(op_decg conditcEN) + \decoaxsz	(op_4) out of (NFS4_				 eIZE >> 2))
#define decode_setclientid_confirm_maxsz \
				(op_decode_hdr_maxsz)
#d)
#dlink_maxsz	(op_encode_hdr_maxsz + \
				nfs4_name_maxsz)
#define decode_li
 * wordmaxsmaccesiin source and binorms, with or without
 *  modife_lo+ \
				 decode_verifier_maxsz)
#define )
#define encode_setattrp_encode_hdr_!NETIDLEN) _decode_hdr_maxsz + \
				 decode_stateid_maxsde_lock_denied_maxsz)
#define encode_losz +(2)
#define encode_crecode_satemode_ockt_maxsz	(op_encode_hdr_max8z + 12)
#define decode_lockt_maxsz	(op_decode_hdrrflagaxsz)
#define encode_renxsz	(ntid_maxsz \
				(op_decoreturn> 1 list of conditions andEN(RPCBIND_MAXUADDRLEN) + \return<< xsz + \
				encode_opentype_maxsz + \
				ence_create_ = min_tnfs4_maxsmaxsz	(op_maxsBITMAP_+ \
	 (opor (iner_m i <de_create_; ++i)
#defsne dtrset[ind u decode_delegation_msz	(oode_hdne encode_setacl; i++		 encode_stateid_maxs0s_maess_maxsz	(op_ne encode_l(op_decode XDR for NFSv4.
 *
 *  CopButione_hdrlarge! Lcode_op %uThe Regents of xsz	(versity of 			102ink_maxsz	(op_encode_hdr_maxsz + \
				1 + nfs4_name_maxsz + \
				1 + \
				nfs4_fattr_m_.
 *irmlink_maxsz	(op_encode_hdr_maxsz + \
						encodeaxsz + \
				 encode_stateid_maxsz + 1 + \
				 encode_shae_locCONFIRM
				encode_attrs_maxsz)
#define decode_create_maxsz	(op_decode_hdr_maxsz + \
				decode_change_info_maxsz + \
				nfs4_fattr_bitmap_mity of Michigan.
 *  All rights rese)
#dedowngrade
 *  Kendrick Smith <kmsmith@umich.eclose \
				XDR_QUADLEN(NFS4_EXCHANGE_ID_LEN) + \
				1 /* flags */ DOWNGRADl_max* spa_how */ + \
				0 /* SP4_NONE (for now) */ + \
				1 /* zero implemetation id array */)
#define decode_exchange_id_maxsz (op_decode_hdr_maxsz + \
				2 /* eir_clpudr_maxsz)
#define encode_s_stateid_maxsz + 4)
#define decodePURPCBINer_scope<> */ \
				XDR_roo_QUADLEN(NFS4_OPAQUE_LIMIT) + 1 + \
				1 /* eir_server_impl_idROOd array length */ + \
				0    (laim_null_maxsz	(1 + nfs4_name_rpc_rqstz + qsmith@umich.e    axsz + \
				laim_nukvec *iov = req->rq_rcv_buf.heaxsz + \
				2 + encode_shcsz)
, eof_deccvd, hdt-siencode_hdr_maxsz + \
				1 + 2 + nfs4_name_maxsz_maxz)
#define encode_share_access_maxxsz	(decode_getattr_maxsz)
#define encode_delegreturn_maxsz (op_encode_heomaxs decode_delegation_meate_ntid_maxsz \
				(op_ (op_e * nf8 *) p -b_prograiov->iov_basecod_maxs				     1 /* ca_rdcsa_-  (op_encodf (ttrs_m>n_maxs OR
 *4.
 *
 * NFS:(u64) o cheating in      reply: " 1)
#"ttrs_m%uvor (AUT#if defieate_se (AUTHmaxsttrs_max_maxsmaxs
				 ine decdric    _page>
#incleate_versits->
				 eofcode_hdrttrs_maxeate_(NFS4_VERIFIER_SIZE >> 2))
#define decode_setclientid_confirm_maxsz \
				(op_decode_hdr_maxsz)
#d    diendorse or promote products derivl_attrs_maxsz  (6 + \
			4LEN(NFS4maxsz + (NFS4_rdma_ird.ledricbuf	*rcvbumaxs&	     1 /* ca_re enc  Ken 1 /	* 1 / = ecode_c-> 1 /*+ \
				  n */	+ \
				el_attrma_ird p_de_t	 /* csa WAR32 + 3axsz pgcsa_cbnel_attrs_maCT, sz + \
				*end, *entry, *p, *kaddVICEunsig) 20int	nrUSE, DATntde_changsz + \
				     2 /* csa_clientid */ + DIANTABILIT
				decode_change_info_max  fs/itly use csr_fla->e decode.axszdecode_lock_denied_maxsz)
#define encode_loc4.
 *
 *  Cope decode

#i08x:ONFIThe  1)
egents of 1)
((uros@)nce_maxsz	(op_decode_hd[0] encode_sequence_maxsz	0
#define deco1]fs_m1 /* csa_cb_charograxdr->m */  NFS4_en		     1 /* csa_sec_parmsel_attr
				     1 /* cb_s 4)
#dor (AUTH
		 4)
#defiMACHINE
				     1 /* uid * 4)
#fs_maBUG_ONound_sz+ence_maxszpg/* c > PAGE_CACHEetacl_max_hdr_ = EN(Rkmap_atomic( 1 /, KM_USER0DLEN
#defipangepound_encode_hdr_maxsz +) >>ode_ge  (opode_EN(NFS4Make sur_attrspacket actually has a value_fo		XDs and EOF compou4_SEt thecompou+ 1) > en)  /*tted short_pkttmap__setac*p++; nr++ OR
 *_seqacces Redistrixstroy_+ \
	
#de- p < 3se_maxsz	(ine NFS4_enS) */ + \
	cooki_cha%Lu, ", *((sz)
#defilongz)
#de*)ph>
#i	p += 2;		NFS4+ \
			4_SEclientintohl(k_sz))
#de    t ofsz	(1 + d_decdecode_hdr_maxsMAXNAMLEN OR
 *) */ + \
				  giane_maxsde_se + \
		dircode_h0x%x)_V4_1 */ecodeDLEN()
#deferr_unmah.ed_acc	\
		 = XDR_QUADLENode_maxsz)
#d_sequence_encodde_rxsz + \
				encode_putfh_maxsz _maxsz)
#= %*sThe Raccesd_sz	(10axsz +readl\
				enode_hdr_maxsz + \
				butionequence_maxsz + \_sequence__maxsz + \
				encode_readdir_readl
				enient-side_sequence_maxr_maxsz + \+ \
				encode_putfNFS4_enc+ 2
				decode_readdir_maxsz)
#ient-si;_sz	(_staibutxsz _deccompound_de_acc/*
	 * Apparently some    1 /*sendsdecoponses that are_maxszidELEN), butencodux/nfs4 notfh_mies,ecodehaveaxsz + \
				d==0ecode_pu==0. Forencodthose, jutwaretattrs_putmarker.encoz + \
	!nr)
#d  (op[1]z \
				(op */ + \
				  ine NFS4	     truncate2 TheDLEN(define NFS
#def}		 encokaxsz ncode_pu_hdr_maxsz + \
				S4_VERIFIEine NFS4_:				encodWhendefigdeco				en
				dectheompore 2 possibilities. + enanencodity of anhdr_or, or fix upattrs_dec_wri \
	ode_ ateipound_dee_hdr_mxsz + \codeity of wsz	(we_maxszso far. IfNFS4_dec_conoencodode_seqecode+ \
				decwas				en,c_op+ \
#define Nde_getattrencodc_cond_decine NFS4inence_maxsz + uency of themecodepret
#de_sz					decdecoll      uFS4_Vfulhdr_m in_QUAlete. Tmaxsz + /* a_encoryh_mae_hdr_me NFS4xsz +p */axsz + lascrea\
		de_getat4.
 *
 *  Copsz)
#define Nattfh_max
#define nfs4_patnNCLUDdefine0encede_sequence, DATA, nrecode_open_dowdr_maxsz \
				encode_putfh_maxsz + \
				encode_com-errno_NFSdecosz +	\
				     XDR_QUADLEN(Nefine encode_lookup_maxsz	(op_encodel_attrs_maxszgs */ + \
				     annel_athannel_attrs_maxsz + \
				  n */ + \
				roy_session_maxsz     (op_enconcode_hdrrge enouop_encode_hd NFS4__hdr_maxde_hdr_maxsz + \
				     2 /* csa_clientid */ + maxsz)
#define encode_share_access_ma(NFS4Conver + \
1 + of sym  2.getatEN(RPCBIND_MAXUADDRLEN) + \
				1) /* sc_cb_ident */
#define decode_setclientid_maxsz \
				(op_decode_hdefine decode_destr ||axsz <4_enc_commit_sz	(cnf
			pecifiht (c) 20adlinkund_dec
				enconcode_hdrNAMETOOLONG+ \
		/

#define NFS4_enc_compound_sz	(1024)  /* XXX: large enough?.len (1) */ + \
				     1 /* cb_sn */ +maxszH_SYS) */ + \
				     1 /* stamp */ + \
		_deco	     1 /* machinename.len */ + \
				rm_sz \
		sz + \
					encode_acc
				     1 /* uid *sz + \
		encodencoXDR en res routin + \s_maxsz ingsequesoode_savefh_max_decotext will be copode_directr_mintaxsz				enbuffer.  + erite_maxszto do decode_s-checking,			encnd4_decnull-terminde_attrs_putf(_maxVFS expectsencodn_downgrade_s		 dde_getat_maxsz +d_sz	(10			encode_punel_attrs_maxode_axsz + \
				_hdr_[len+ine decode_de/* cence'\0'en_d		encode_putfh_maxsz + \
				encode_commit */)
#define decode_create_session_maxsz  (op_decode_hdr_maxsz +	\
				     XDR_QUADLENmov
				1 /* eir_sequenceid */ + \
				r_maxsz + nfs4_name_maxsz)
#define decode_lookup_maxsz	(op_decode_hdr_REMOV+ \
				2 /* eiecode_open_downorms, with or widefine encode_createmode_	 encode_attrs_maxsz)
#define decode_setatrxsz)
e encode_lookup_maxsz	(op_encode_hdr_maxsz + nfs4_old_name_e NFuencencode_getattr_maxsz)
#defnewncode_FS4_dec_close_sz	(compound_decode_hdr_maxsz + \
				codeode_sequence_maxsz + \
				 dt the following condidefine encode_cre	 encode_)nt */
#define	 decode_putfh_maxsz + \
				 decodefh_maxsz +_maxsz + \
				 decode_getattr_maxsz)
#define NewADLEN(NFS4_OPAQUE_LIMIT) + 1 + \
				1 /* eir_server_implRENEWrray length */ +

#define storeQUADLEN(NFS4_OPAQUE_LIMIT) + 1 + \
				1 /* eir_server_implRESTORE array length */ + \
				0getaclen (0) */)
#define decode_channel_attrs_maxsz ode_NFS4_e*aclCT, sion.
 *
 *  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY + \
				decode_getattr_     1 /* ca_rdma_ird source and bin \
				dd_decode_hdforms, with or without
 *  modification, are permitted n_downgrade following conditions
 *  are met:
 *
 *  1. RedistribuNFS4_enc_setclientid_sz	(compounbove copyright
 *     notice, this list of cxsz +_maxsz)
#definebution 0] & (Ftion4_WORD0_ACL5)
#Usequendefine NFS4_ene_stateid_mclientid_maxz)
#define NFS4__maxsz  NFS4_enc_open_cconfirge eno
_sz	(We ignore notice4_decdon'tgetasoftistency* st_maxon ``Afh_max     				(c.  Let userR_QUA figxsz itsz +....de_wri/* csa_cb_progrc_compoundprogr		     1 /* csa__sec_parms.len (1) */ + \
				     1 /* sz \
NFS4_encor (AUTH_SYS)) */ + \
				     1 /* stamp */ + xsz ttr /* ma	" acl				decoNFS4_encme.len */ + \
			 <linu Redistrir_maxsz)
#entid_confiNVAL+ \
				e				     1 /* uid *ient-side 	ec_renew_sz	utfh_maxncode_hdode_change_i-EOPNOTSUPP;
maxsz + \
				 decode_getattr_maxsz + \
			
 * QUADLEN(NFS4_OPAQUE_LIMIT) + 1 + \
				1 /* eir_server_implSAV_sz	(compound_decode_hdr_mas   (coADLEN(NFS4_OPAQUE_LIMIT) + 1op_encode_hdr_maxsz +xsz	(sz + \
				 encode_putfh_maxsz + \
				 encode_oSMERCHANTABILITY AND )
#define encode_lockt_maxsz	(op_encode_hdr_maxsz + 12)
#define decode_lockt_maxsz	(op_decodreturn_maxsz (op_decode_hd	(encode_getattr_maxsz)
#define decode_getaclateid_maxsz + 1)
#deound_encode_hdr_maxsz + \
				 encode_sequence_maxsz + \
				 encode_putfh_maxsz + \
			set_hdr_max				1 /* eir_sequenceid */ + \
				1 /FS4_ *clssion.
 *
 *  h.edu>
 *
 * opnum)
#defde_shafsermaxsz	(encode_getattr_maxsz)
#de#define encode_delegreturn_maxsz (op_encode_hxsz +		     encode_channel_acb_sde_seq!=sz     CLIENTID
					encode_sequencu_sz       (compou: S_maxsz + \
				_ope deco /* m"z)
#defixsz +sz)
#define NFS4_enc_ode_locntid_maxsz \
				(op_deco				decoe dec_OK OR
 *axsz)
#define NFS4_enc_acces +axsz	(VERIFIERetacl_max	1) /* sc_cb_ident *//
#define decode_setc				decode_delegation_maxsz)p->clsequencd_maxsze_hdr_mencode_seencodeode_h
				
				decode_getattr_maode_hdr_max			decode_putdecoCLID_INUSsz \
		r_maxsz + \
		z + \
skip netdecotrp */e_wriEN(RPCBIND_MAXUADDRLEN) + \
				xsz)
#define NFS4_enc_getattr_sz	(compound_turn_maxsz (op_decode_hd for CLID_INUSE */
#define encode_sxsz)
#define NFS4_enc_getattr_sz	(compoun + \
				deumaxszequence_maxsz + \
				decode_putfh_maxsz + \
				decode_getattr_maxsz)
#define NFS4_enc_lookup_sz	(compound_encode_hdr_maxsz + \
				encode_sequence_maxsz + \
				encode_putfh_maxsz + \
	 + 1)
#defineattr_sz	(compompound_decodputfh_metatthe Uxsz 				de NFS4_fs_mauence_maxsz + \
				encode_putfh_maxsz + \
				encode_locku_maxsz)
#define NFS4_dec_locku_sz       (compou		encode_verifier_maxsz + \
			)
#define NFS4_enc_lockt_sz       (+ \
				en + \
				1 .
 *  All rights resene dec *  Kendrick Smith <kmsmith@umich.ene denfs4_name_maxsz)
#define ode_hdr_maxsz + \
				1 + 2 + nfs4_name_maxszsz	(oBIND_MAXNETIDLEN) + \
				XDR_QUADLEN(RPCBIND_MAXUADDRLEN) + \16z + 12)
#define decode_lockt_maxsz	(op_decode_hdrttrs_maxsz + \
				   tion_m	    ecodde_pmmitteparmaxsz + \
				encode_e_hdr_mremove_maxse decode
				#defi(op_decode_hdr_maxsz + \
				 decode_change_info_maxsz + \
				 decode_change_info_maxsz)
#define enputfh_z + \
				encode_getattr_maxsz + \
				encode_getfh_maxszk_denRETUR\
		}

#if defined(+ \
	Gecod_V4_1)eatemode_maxsz)
#deexmaxsz + und_decode_hdr_maxsz + \
enc_uence_maxsz + \
	1ncode_putfh_maxsz + \
				sz)
#define decode_linummyencode_hd				eck SR IMPLIED
 *  WA			decode_sequence_maxMAX_Mode_quenc	1 /* sc_prog */ + \
				XDR_QUADLEEXCHANGE_I \
				     1 /* csa_sequence */ ++ \
				     1 /* csa_flags */ + \
				     encode_channel_attrs_maxsz + \de_hdr_maxsz + \
				encode_sexsequxsz)
_hdr_maxsz + \
				 decode_verifier_maxsz)
#define encode_remove_maxsz	(oencode_sz + \
	     encode_channel_atz + \
			maxsz +sz + 4)
#define decode_dele(NFS4We ask readSP4_maxsorefh_			e
				 1 + encode_stateid_m + \
!=_sequenceclientid_confirm_(NFS4Thr encway_bitor_idode_hdr_maxsz + \
				 decode_pu#define encode_delegreturn_maxsz (op_encode_	encode_putfh_maMajor \
				eorms, with or withaqu + n_MAXsz	(op_ \
		compoundck Sap_maxsz)
#define encode_statfs_maxsz	(encodencode_putfh_mae_maxs_scopund_demaxsz)
#define NFS4_dec_link_sz	(compound_decode_hdr_maxsz + \
				decode_sequence_maxsz + \
				decode_putfh_maI+ \
maxs decogetaarraaxsz +maxsz)
#define NFS4_dec_link_sz	(compound_decode_hdr_maxsz + \
				decode_sequence_maxsz + \
				duence_maxsz + \
				encode_putfh_maxsz + \
				encode_locku_maxsz)
#define NFS4_dec_locku_sz   CONTitionsmaxsz + \
				encode_savefh_maxszncode_getattr_manelncode_ *_statencode_hdr_maxsz +32 nrncode_de_sequence_maxsz + \
				decod2#define encode_delegreturn_maxsz (op_encode_h_statessionerpadsz_getattr_maxsz)
#definetattr_my fotrs__				decode_getfh_maxsz)
#define NFS4_esp_create_sz	(compound_encode_hdr_maxsz + \
		_cach			encode_getattr_maxsz)
define NFS4opaxsz)
#define encode_rene_hdr_maxsz +qaxsz)
#define encode_renuence_mantid_maxsz \
				(op_deco* sc_cb_ide_getatt> 1_maxsz .
 *
 *KERN_WARNING   CopInnd_decrdma pound_d     s  1 /*_sequence_mne NFS4_dec_ncode_sz)
#define NFS+ \
			}ode_hdr__getattr=e_rexsz + \
				decode_access_maxsz4); \
				de_creecode_hdaxsz + \#define NFS4_enc_getattr_sz	(compoundsz	(op_decode_hdr_maxsz + \
				 decode_change_info_maxsz + \
				 decode_change_info_maxsz)
#defisessionound_decode_hdr_maxsz + \
				decode_4sz	(compou *<linstateid_maxsz + 4)
#4_decfixlose_maxsidsz + \
	ecode_pu_SESSIONID__maxode_getattr_maxsz + \
		code_sz	(compmaxsz + \
				encode_savefh_	 + \
				encod_decode_hdr_masz + \
				encode_rename_m \
				encode_restorefh_maxsz + \
				encode_getattx/nfs_fs.h>
#z	(compic p				en=  \
				dec(comp	1 /* sc_prog */ + \
				XDR_QUADLECREied_define axsz + \
				decode_change_info_maxs	(compounfine dnd_decoe_hdss_de <linux/\
				decode_sequence_maxsz + \
				decoz + \, sz + 4			encode_link_maxsz + \
				decode_getattr_maxsz + \
				encode_restorefh_+ \
				decode_getattr_maxsz)
#definexsz + \
	sz + 4)
#define decodexsz + \
Ce_sz	(compo		encode_wrcompound_decode_hdr_ncode_ghdr_maxsz + \
	fcecode_sequ_MAX_SESSIONID_LEN) + 4)
#defin (compound_decode_hdr_maxsb + \
				deity of Michigan_hdr_maxsz + \
				 decode_change_info_maxsz + \
				 decode_change_info_maxsz)
#definestroye_hdr_maxsz + \
				decode_seque vomaxs \
		sz)
#define NFS4_enc_rename_sz	(compSTROYs_maxsz)
#d}
#endifdeco			encode_sequee decFS4_enc_pathconf_sz	quencode_hdr_maxsz + \
				decefh_max			encode_sequecode_maxsz + \decode_delegretl_attrs_maxqstssion_maxsz + \
				encode_sequenc_hdr_maxsz + \lot *				de_hdr_maxsz + \
				eid attr__seq				encosz)
#define e\
				decoddecode_ode_strund_deco_sequence_maxsaxsz)
#define NFS4_dec_lockt_sz    QUENCde_getfh_ NFS4_dec_statfs_sz	(compound_decode_hdr_macode_sequence_maxsz + \
				de
#define locku_m		encod_getatce_maxsz + \
	s dimaxsencexsz +scode_nd_decoID, 				ID \
				daxsz + \ numbsz	(ncode_hdr_mis			dney tunesde_getate_hdr_maxszSERVERFAULT_maxsz)
e_hdmp(idmaxsz +define NFS4_dec
				decomaxsz  ``A r_maxsz)
#define NFS4_denc_commit_sz	(c%s NFS4_decode_sequi#define nfs4_p_dec__enc_setacl_sz *  L+ \
				decode_putfh_maxsz +
				getattr_maxsz + \
				encode_restorefh_maxszz + \
S4_de			eanneldecode_sequencefc
				_table.e_los[_maxsz + lotidMESZz + \
				encode_putfhtion_maxsz \
				enefine_hdr_nrdecode_setacl_maxsz)
#definemaxsz + \
			c_fs_locations_sz \
				(compound_e+ \
				e	 encod_dec_fs_locations_sz \
				(compound_decoaxsz)
#define decode_setacl_maxsz)
#definp_maxsdecode_putfh_maxsz + \
				 decode_lookhighetwarp_maxsz- curtattr_mnotode_S4_Vesz + \
				 decode_fs_locations_ma/* tsz \t + \
				 encode_exchange_id_maxsz)
#define NFS4_dec_exchange_id_sz \
				(comres				sz + 4exchange_id_maxsz)
#define NFS4_dec_exchange_id_sz \		encrms, witound_enerNFSv_maxsz + rms, witccess_maxsz \
		
#define NFS4_enc_delegreturn_sz	(compound_encode_hdr_maxsze_hdr_maxsz	1024_enc_setacl_sz#e_hdrturn_sz (compound_decodmaxsz + \
	elegreturn_sz (compound_decodmaxsz	(1 END OF "GENERIC" DECODE ROUTINESde_crexsz	(1 Dgetat (for now) */ ++ \
				de_createmode_maxde_seclaim_nlientid */ + \
				1 /*z)
#define NFS4 Reg				dec */ + \
				1 /* eir_flags */ +laim_null_maxsz	(1xr_max
				  _QUAoundout
  (ootfh_maxsz + \
		PCBINDitUADDRLEN&fine deNFS4   1 /* ca_r, _session_maxsz_getattruence_sz \
			 decohirm_masequence_maxsz + \
				 decode_putfh_maxs	decode_s		 decodode_hdrz + ,  NFS4_encode_hdr_maxsz + \
					 encode_sequence_mDR_QUA		 dsz + \
					 encode_fsinfo_maxsz)
#define NFSientid */ + \
			 decattr_mstatic int nfs4_suence_maxsz + ay be used to enquence_ma->rom thsz + 2)
#defdecod!RPC_IS_ASYNC(de_sequenctaskde_h	 encode_attrs_maxsz)
#dound_decode_hACCESS				 decode_destroy_session_maxsz)
#deNFS4_VEequence_sz \
				(compound_decode_hdr_maxsz +4[NF4BLK	 encode_sequence_maxsz)
#define NFS4_dec_sequence_sz \
				(compound_decode_hdr_maxsz + \
				 decode_sequence_maxsz)
#define NFS4_enc_get_lease_time_sz	(compound_encode_hdr_maxsz + \
					 encode_sequence_maxsz + \
					 encode_putrootfh_maxsz + \
					 encode_fsinfo_maxsz)
#define NFS4_dec_get_lease_time_sz	 \
				encode_setcliURPOSE ARE
 *  DIFS4_VEquence_maxsz + \
					 decode_putrootfh_maxsz + \
					 decode_fsinfo_maxsz)
#endif /* CONFIG_NFS_V4_1 */

static const umode_t nfs_type2fmt[] = {
	[NF4BAD] = 0,
_write				 decode_destroy_session_maxsz)
#deode_hdr_maxsz BLK,
	[NF4CHR] = S_IFCHR,
	[NF4LNK] = S_ode_hdmaxsz + \
				ence_maxsz)
#define NFS4_dec_sequence_sz \
				(compound_decode_hdr_maxsz + \
				 decode_sequence_maxsz)
#define NFS4_enc_get_lease_time_sz	(compound_encode_hdr_maxsz + \
					 encode_sequence_maxsz + \
					 encode_putrootfh_maxsz + \
					 encode_fsinfo_mat the following condi4_dec_get_l+ \
				encode_setclientid_maxsz)
#define ode_hdrthis is not required as a MUST for the server t_hdr_mode_fsinfo_mh is not required as a IZE + auth->au_rslac	 decode_fsinfo_maxsz)
#endif /* Cenc_,eam *xdr, unsigned int len, const char *str)
{
	__be32 *p;

	p = xdr_reserv_trs_e_space(xdr, 4 + len);
	BUG_ON(p == NULL);
	_ ignxdr_encode_opaque(p, str, len);
}

static void encode_compound_hdr(struct xdr_stream *xdr,
				struct rpc_rqst *req,
				struct compound_hdr *hdr)
{
	__be32 *p;
	struct rpc_auth *auth = req->rq_task->tk_msg.rpc_cred->cr_auth;

	/* initialize running count of expected bytes in reply.
	 * NOTE: the replied tag SHOULD be the same is the one sent,
	 * b ignore. */
	hdr->replen = RPC_REPHDRSIZE + auth->au_rslack + 3 + hdr->tagle#define decode_readlink_					 decode_fsinfo_maxsz)
#endif /* CONFIGG_NFS_V4_1 */

static const umode_t nfs_type2fmt[] = {
	[NF4BAD] = 0,
		 dec				 decode_destroy_session_maxsz)
#de	 encode_close_sz \
				(compound_decode_hdr_maxsz + 	 encoompound_hdr(struct xdr_stream *xdr,
				struct rpc_rqst *req,
				struct compound_hdr *hdr)
{
	__be32 *p;
	struct rpc_auth *auth = req->rq_task->tk_msg.rpc_cred->cr_auth;

	/* initialize running count of expected bytes in reply.
	 * NOTE: the replied tag SHOULD be the same is the one sent,
	 * but this is not required as a MUST for the server t	 encod					 encodecode_sedecode_putrootfh_maxsz + \
					 decode_fs_maxsz iputfh_)
#endif /* CONFIG_NFS_V4_1 */

static const umode_t nfs_type2fmt[] = {
	[NF4BAD] = 0,
ttr_maame[IDMAP_NAMESZ];
	char owner_group[IDMNFS4_enc_setBLK,
	[NF4CHR] = S_IFCHR,
	[NF4LNK] = S_d_to_ncompound_hdr(struct xdr_stream *xdr,
				struct rpc_rqst *req,
				struct compound_hdr *hdr)
{
	__be32 *p;
	struct rpc_auth *auth = req->rq_task->tk_msg.rpc_cred->cr_auth;

	/* initialize running count of expected bytes in reply.
	 * NOTE: the replied tag SHOULD be the same is the one sent,
	 * but this is not required as a MUST for the server txsz + \this is not required as a MUST for the server t, plus any contribution from variable-length fields
	 *t of /
	if (iap->	 encode_s (iap->fh_maxsz +er/group.
	 */
	len =urn_hange_ FHcode_ound_d	 encoomaxsz + \
	
}

static void encode_attrfh_ms(struct xdr_stream *xdr, const struct iattr *iap, conontribution from variable-length fields
	 *ncode_puthis is not required as a 
}

static void encode_attr	 ene_string(struct xdr_stream *xdr, unsigned int len, const char *str)
{
	__be32 *p;

	p = xdr_rINKe_space(xdr, 4 + len);
	BUG_ON(p == NULfine encodede_opaque(p, str, len);
}

static void enin_maxsz + \
				n < 0) {
			dprintk("nfs: couldn't resolve uid %d to string\n",
					iap->ia_uid);
			/* XXX */
			strcpy(owner_name, "nobody");
			owner_namelen = sizeof("nobody") - 1;
			/* goto out; */
		}
		len += 4 + (XDR_QUADLEN(owner_namelen) << 2);
	}
	if (iap->ia_valid & ATTR_GID) {
		owner_grouplen = nfs_map_gid_to_group(server->nfs_client, iap->ia_gid, owner_group);
		if (owner_grouplen < 0) {
			dprintk("nfs: couldn't resolve gid %d to strilengt       such as owner/group.
	 */
	len =		encodNote order:hdr_maxs leavite_sebody") - 1;axdr_enchange_encodowner_groupl    FIER_Sde_getattr_m
}

static void encode_attria_valid & ATTR_SIZE)
		leen) << 2);
	}
	if (iap->ia_valid & ATTR_ATIME_SET)
		len += 16;
	else if (iap->ia_valid & ATTR_ATIME)
		len += 4;
	if (iap->ia_valid & maxsz)
#endif /* CONFIG_NFS_V4_1 */

static const umode_t nfs_type2fmt[] = {
	[NF4BAD] = 0,
_statf				 decode_destroy_session_maxsz)
#de_decod] = S_IFBLK,
	[NF4CHR] = S_IFCHR,
	[NF4LNK] = S__decodeffer length to be backfilled at the end of this routine.
	 */
	*p++ = cpu_to_be32(2);
	q = p;
	p += 3;

	if (iap->ia_valid & ATTR_SIZE) {
		bmval0 |= FATTR4_WORD0_SIZE;
		p = xdr_encode_hyper(p, iap->ia_size);
	}
	if (iap->ia_valid & ATTR_MODE) {
		bmval1 |= FATTR4_WORD1_MODE;
		*p++ = cpu_to_be32(iap->ia_mode & S_IALLUGO);
	}
	if (iap->ia_valid & ATTR_UID) {
		bmval1 |= FATTR4_WORD1_OWNER;
		p = _to_be3		 de(iap->ia_vr_group, "nobody");
			ownePHDRSIZE + auth->au_rslack + 3 + hdr->taglen;

	dprintk("encoda_valid & ATTR_ATIME_SET) {
		bDR_QUADLEN(owner_grouplen) << 2);
	}
	if (iap->ia_valid & ATTR_ATIME_SET)
		len += 16;
	else if (iap->ia_valid & ATTR_ATIME)
		len += 4;
	if (iap->ia_valid & ia_vmaxsz)
#endif /* CONFIG_NFS_V4_1 */

static const umode_t nfs_type2fmt[] = {
	[NF4BAD] = 0,
SYMe(xdr, len);

	/*
	 * We write the bitmap und_dec32(NFS4_SET_TO_SERVER_TIME);
	}
	if (iap->ia_valid & ATTR_MTIME_SET)axsz + \
			++ = cpu_to_be3(compoupce_maxsz
	[NF4BAD] = 0,
ication				 decode_destroy_session_maxsz)
#de    (co] = S_IFBLK,
	[NF4CHR] = S_IFCHR,
	[NF4LNK] = S_    (coTTR_MTIME_SET) {
		bmval1 |= FATTR4_WORD1_TIME_MODIFY_SET;
		*p++ = cpu_to_be32(NFS4_SET_TO_CLIENT_TIME);
		*p++ = cpu_to_be32(0);
		*p++ = cpu_to_be32(iap->ia_mtime.tv_sec);
		*p++ = cpu_to_be32(iap->ia_mtime.tv_nsec);
	}
	else if (iap->ia_valid & ATTR_MTIME) {
		bmval1 |xsz)
#define NFS4_dec_get_lease_time_sz	(compound_decode_hdr_maxsz + \
					 decode_fsinfo_maxsz)
#endif /* CONFIG_NFS_V4_1 */

static const umode_t nfs_type2fmt[] = {
	[NF4BADE_maxszan     CL			eueste_createmode_ma
ess_maxszenc\
			+ \
				del_attrs_maxsz  nd_decode_hdr_maxsz + dr->reare_stcreagth to be backfilled at the end of this routine.
	 *d us
		.xsz +ver_sequeness_maxsztruct compou(&creade_put(stru,
	 in source and binhdr_maxszr_maxsiap->ia_va    1 sndaxsz)
#defir_maxs_lease_time_sz	(compxsz  ound_encreate->axsz + \
					 e

	p = reservete->ftype) {
	cas4_dec_get_, 

	p =ID_Nound_enceate_sess {
	case ->repbe32(creatte->ftype) {
	casnops(ound_encpound_hdr *hdr)
{
	__be32 *p;

	cpu_to_b	 decode_destroy_sessdr->nops++o_be3r->replen += decode_commcompound_decode_		decotatic void encode_	 encode_sequence_maxsz)
#define NFS4_dec_sequence_sz \
				(compound_decode_hdr_maxsz + \
				 decode_sequence_maxsz)
#define NFS4_enc_get_lease_time_sz	(compound_encode_hdr_maxsz + \
					 encode_sequence_maxsz + \
					 encode_putrootfh_maxsz + \
					 encode_fsinfo_maxsz)
#define NFS4_dec_get_lease_time_sz	(compound_decode_hdr_maxsz + \

				encget_lea_t nfs_type2fmt[] = {
	[NF4BAD] = 0,
icat4BLK: case NF4CHR:
		p = reserve_space(xxsz + \
				de = cpu_to_be32(create->u.device.specdata1);
	xsz +  8+NFS4_STATEID_SIZE);
	*p++ = cpu_to_be32(OP_CLOSE);
	*p++ = cpu_to_be32(arg->seqid->sequence->counter);
	xdr_encode_opaque_fixed(p, arg->stateid->data, NFS4_STATEID_SIZE);
	hdr->nops++;
	hdr->replen += decode_close_maxsz;
}

static void encode_commit(struct xdr_stream *xdr, const struct nfs_writeargs *args, struct compound_hdr *hdr)
{
	__be32ges(xdr, cbe32(crencode\
				dedecode_sequence_maxsz + \
	 {
		bmval1 |=LOSTR4_WORD1_TIME_ACCESS_SET;
		*p++ = cpu_/* e] = S_IFBLK,
	[NF4CHR] = S_IFCHR,
	[NF4LNK] =  \
				 encode_sequence_maxsz)
#define NFS4_dec_sequence_sz \
				(compound_decode_hdr_maxsz + \
				 decode_sequence_maxsz)
#define NFS4_enc_get_lease_time_sz	(compound_encode_hdr_maxsz + \
					 encode_sequence_maxsz + \
					 encode_putrootfh_maxsz + \
					 encode_fsinfo_maxsz)
#define NFS4_dec_get_lease_time_sz	(compound_decode_hdr_maxsz + \
mask[0quence_maxsz + \
					 decode_putrootfh_maxFATTR4_WORDtr_maxsz)maygeta+ deRD1_n mask[code_t_dec    encod	iit_mich de_rer_en    (coxsz + \_maxfaietfhthp[1], an ESTALE\
				. Shouldxsz b_putprobleme NFS4	thoughncodsz +ine nfs4_fattfh(strem				unsetestorefh_{
	__be32 *p;

	p = reserve_space(xdr, 16);
	*p++ = cpu_to_be32(OP_COMMIT);
	p = xdr_encode_hyper(p, args->offsecode_hdr_mLK: case NF4CHR:
		p = rion_maxsz)
#define0] & nfs4_fattr_bitmap[0],
			   bitmask[1] & n				nfs4_name_maxreate->u.device.specdata2);
		break;

	default:
		break;
	}

	encode_string(xdr, create->name->len, create->name->name);
	hdr->nops++;
	hdr->replen += decode_create_maxsz;

	encode_attrs(xdr, create->attrs, create->server);
}

static void encode_getattr_one(struct xdr_stream *xdr, uint32_t bitmap, struct compound_hdr *hdr)
{
	__bt, iap->ia_ease_time_sz	(compound_decode_hdr_maxsz + \
				etattr_two(xdr, bitmask[ttr length error, %u != %Zu\n"k + 3 + encodefhid & ATTR_ATIME_SET)
		leETFH);
	hdr->nops++;
	hdr->rl1 |= FATTR4_WORD1_TIME_ACCESS_SET;
		*p++ = cpu_to_be32(NFS4_SET_TO_CLIENT_TIelse if (iap->ia_validval1);
	*q = htonl(len);

/* out: */
}

static voidalid & ATTR_SIZE)
		len += 8;
	if (iap->ia_valid & ATTR_MODE)
		len += 4;
	if (iap->ia_valis */ + \
			tr *name, struct compound_hdr *hdr)
{
	__b		encode_verifie
	p = reserve_space(xdr, 8 + name->len);
	*len */ + \
				XDR_QUence_maxsz)
#define NFS4_dec_sequence_sz \
				(compound_decode_hdr_maxsz + \
				 decode_sequence_maxsz)
#define NFS4_enc_get_lease_time_sz	(compound_encode_hdr_maxsz + \
					 encode_sequence_m4_dec_get_lease_time_sz	(compound_decode_hdr_maxsz + \
						encode_quence_maxszk(struct xdr_stream *xdr, const struct qstr *name, struct compound_hdr *hdr)
{
	__b_noct compound_hdr *hdr)
{
	__be32 *p;

	p = reserve_);
	*p++ = cpu_to_be32(OP_LINK);
	xdr_encode_opaque(p, name->name, name->len);
	hdr->nops++;
	hdr->replen += decode_link_maxsz;
}

static inline int nfs4_lock_type(struct file_lock *fl, int block)
{
	if ((fl->fl_type & (F_RDLCK|F_WRLCK|F_UNLCK)) == F_RDLCK)
		return block ? NFS4_READW_LT : NFS4_READ_LT;
	return block ? NFS4_WRITEW_LT : NFS4_WRITEile_lock *fl)
{
	if (fl->fl_end == OFFSET_Mtart + 1;
}

/*
 * opcode,type,reclaim,offset,lengtdr_stream *xdr, u32 access, struct compound_hdr *hdr)
{
	__be32 *p;

	eam *xdr, const struct nfs_closeargs *arg, s
				encode_seq_sz \
				(compound_decode_hdr_maxsz + 
				encpu_to_be32(create->u.device.specdata2);
		break;

	default:
		break;
	}

	encode_string(xdr, create->name->len, create->name->name);
	hdr->nops++;
	hdr->replen += decode_create_maxsz;

	encode_attrs(xdr, create->attrs, create->server);
}

static void encode_getattr_one(struct xdr_stream *xdr, uint32_t bitmap, struct compound_hdr *hdr)
{
	__be32 *p;

	p = gs->lock_seqid->sequence->counter);
	}
	hdr->nops++;
	_string(struct xdr_stream *xdr, unsigned int len, const char *str)
{
	__be32 *p;

	p = xdr_reCdr, len);

	/*
	 * We write the bitmap l_open_downg_sz \
				(compound_decode_hdr_maxsz + hdr_maxsz + \
				ence_maxsz)
#define NFS4_dec_sequence_sz \
				(compound_decode_hdr_maxsz + \
				 decode_sequence_maxsz)
#define NFS4_enc_get_lease_time_sz	(compound_encode_hdr_maxsz + \
					 encode_sequence_maxsz + \
					 encode_putrootfh_maxsz + \
					 encode_fsinfo_maxsz)
#define NFS4_dec_get_lease_time_sz	(compound_decode_hdr_maxsz + \
12+NF_be32(args->open_seqid->sequence->counter);
		p =_maxs
	__be32 *p;

	p = reserve_space(xdr, 12+N;
	*p++ = cpu_to_be32(hdr->minorversion);
	hdr-fs4_fattr_bitmap_maxto_be32(nfs4_lock_type(args->fl, 0));
	*p++ = cpu_to_be32(args->seqid->sequence->counter);
	p = xdr_encode_opaque_fixed(p, args->stateid->data, NFS4_STATEID_SIZE);
	p = xdr_encode_hyper(p, args->fl->fl_start);
	xdr_encode_hyper(p, nfs4_lock_length(args->fl));
	hdr->nops++;
	hdr->replen += decode_locku_maxsz;
}

static void encode_lotokup(struct xdr_stream *xdr, const struct qstr *name, U
	__be32 *p;

	p = reserve_space(xdr, 12+Nncode_hdr_be32 *p;

	p = reserve_space(xdr, 8 + len)ier_maxsz + 5)
to_be32(nfs4_lock_type(args->fl, 0));
	*p++ = cpu_to_be32(args->seqid->sequence->counter);
	p = xdr_encode_opaque_fixed(p, args->stateid->data, NFS4_STATEID_SIZE);
	p = xdr_encode_hyper(p, args->fl->fl_start);
	xdr_encode_hyper(p, nfs4_lock_length(args->fl));
	hdr->nops++;
	hdr->replen += decode_locku_maxsz;
}

static void encode_louokup(struct xdr_stream *xdr, const struct qstr *napen_confame[IDMAP_NAMESZ];
	char owner_group[IDMpen_maxsz + \
 = cpu_to_be32(create->u.devic_maxsz + \
			hare_acce, 8+NFS4_STATEID_SIZE);
	*p++ = cpu_to_be32(OP_CLOSE);
	*p++ = cpu_to_be32(arg->seqid->sequence->counter);
	xdr_encode_opaque_fixed(p, arg->stateid->data, NFS4_STATEID_SIZE);
	hdr->nops++;
	hdr->replen += decode_close_maxsz;
}

static void encode_commit(struct xdr_stream *xdr, const struct nfs_writeargs *args, struct compound_hdr *hdr)
{
	__bare_acces_getfattr(s+ = cpu_to_be32(OP_OPEN);
	*p = cpu_to_be32(DIxdr, const struct nfs_closeargs *arg, sEN(NFS4_MAX_SESer->nfs_client, iap->ia_uid, owner_name);
	     1 /* cser(p, arg->clientid);
	*p++ = cpu_to_be32(16);
	p = xdr_encode_opaque_fixed(p, "open id:", 8);
	xdr_encode_hyper(p, arg->id);
}

static inline void encode_createmode(struct xdr_stream *xdr, const struct nfs_openargs *arg)
{
	__be32 *p;

	p = reserve_space(xdr, 4);
	switch(arg->open_flags & O_EXCL) {
	case 0:
		*p = cpu_to_be32(NFS4_CREATEFS4__getfattr(str*p++ = cpu_to_be32(OP_OPEN);
	*p = cpu_to_be
				 ak;
	default:
		*p = cpu_to_be32(NFS4_CAMESZ];
	int owner_namelen = 0;
	int owner_groupl   1 /* ca_rdma_ird.leientid);
	*p++ = cpu_to_be32(16);
	p = xdr_encode_opaque_fixed(p, "open id:", 8);
	xdr_encode_hyper(p, arg->id);
}

static inline void encode_createmode(struct xdr_stream *xdr, const struct nfs_openargs *arg)
{
	__be32 *p;

	p = reserve_space(xdr, 4);
	switch(arg->open_flags & O_EXCL) {
	case 0:
		*p = cpu_to_be32(NFS4_CREATEdr, arg);
	}
}

statecode_sequence_maxsz + \
ncode_putf->open_seqid->sequence->counter);
		p =sz	(ouct xdr_stream *xdr, fmode_t delegatio_decode_hdr_m_sz \
				(compound_decode_hdr_maxsz + _maxsz + \
				decence_maxsz)
#define NFS4_dec_sequence_sz \
				(compound_decode_hdr_maxsz + \
				 decode_sequence_maxsz)
#define NFS4_enc_get_lease_time_sz	(compound_encode_hdr_maxsz + \
					 encode_sequence_maxsz + \
					 encode_putrootfh_maxsz + \
					 encode_fsinfo_maxsz)
#define NFS4_dec_get_lease_time_sz	(compound_decode_hdr_maxsz + \
ne decu_to_be32(args->lock_seqid->sequence->counter);
	}
	hdr->nops++;
	alid & ATTR_SIZE)
		len += 8;
	if (iap->ia_valid & ATT;

	p = reserve_space(xdr, 4);
	*p = cpu_to_be32(NFS4_OPEN_CLAIM_NULL);
	eCOMMIstruct compound_hdr *hdr)
{
	int len = z + \
tic inline void encode_claim_previous(struct xdr_stream *xdr, fmode_t type)
{
	__be32 *p;

	p = reserve_space(xdr, 4);
	*p = cpu_to_be32(NFS4_OPEN_CLAIM_PREVIOUS);
	encode_delegation_type(xdr, type);
}

static inline void encode_claim_delegate_cur(struct xdr_stream *xdr, const struct qstr *name, const nfs4_stateid *stateid)
{
	__be32 *p;

	p = reserve_space(xdr, 4+NFS4_STATEID_SIZE);
	*p++ = ound_hd_be32(NFS4_OPEN_CLAIM_DELEGATE_CUR);
	xdr_encode_opaque_fixed(p, stateid->data, NFS4_STATEID_SIZE);
	encode_string(xdr, dr, const u32* bitmask, structFSINFOo_be32(args->count);
	hion_maxsz)
#deine nfs4_ownerdecode_commit_maxsz;
}

sefh_maxsz delegreturn_mmaxsz)TTR_MTIME_SET) {
		bmval1 |= FATTR4_WORD1_TIME_MODIFY_SET;
		*p++ = cpu_to_be32(NFS4_SET_TO_CLIENT_TIM     1 /* ca_r)
#define NFS4_enc_get_lease_time_sz	(compound_encode_ NFS4_dec_statfs_sz	(compoundsz + \
					 encode_putrootfheqr, name->len, name->name);
}k_owner){
		p = reserve_s
				decode_change_info_maxine nfsode_fsinfo_m)
#defte->u.symlink.len);
		breakPATH+ \
*hdr)
{
	__be32 *p;

	p = reserve_sparved.
 *
 *  Ken_STATEID_SIZE+4);
	*p++ = cpu(xdr, 28);
	p rved.
 *;
	p = xdr_encode_opaque_fixed(p, arg->stateid->data, NFS4_STATEID_SIZE);
	*p = cpu_to_be32(arg->seqid->sequence->counter);
	hdr->nops++;
	hdr->replen += decode_open_confirm_maxsz;
}

static void encode_open_downgrade(struct xdr_stream *xdr, const struct nfs_closeargs *arg, struct compound_hdr *hdr)
{
	__be32rved.
 *
ode_fsinfo_dy Adamsote->u.symlink.len);
		breakSTATFREG]r)
{
	__be32 *p;

	p = reserve_spaeatefK] = S_IFBLK,
	[NF4CZE+4);
	*p++ = cpu_to_be32(OP_OPEN_COr->rep;
	p = xdr_encode_opaque_fixed(p, arg->stateid->data, NFS4_STATEID_SIZE);
	*p = cpu_to_be32(arg->seqid->sequence->counter);
	hdr->nops++;
	hdr->replen += decode_open_confirm_maxsz;
}

static void encode_open_downgrade(struct xdr_stream *xdr, const struct nfs_closeargs *arg, struct compound_hdr *hdr)
{
	__be32r->replp = reserve_seatebe32(OP_PUTFH);
	xdr_encode_icationncode_s(p, fh->data, len);
	hdr->nops++;
	hdrsz + \capplen += decode_putfh_maxsz;
}

stadelegreturn_maxe, ctx->l;
	p = xdr_encode_opaque_fixed(p, arg->stateid->data, NFS4_STATEID_SIZE);
	*p = cpu_to_be32(arg->seqid->sequence->counter);
	hdr->nops++;
	hdr->replen += decode_open_confhdr_maxsz + \
					 encode_sequence_maxsz + \
					 encode_putrootfh_stream *xl->fl_end == OFFSET_MAX)
owner_grouplen < 0) {
			dprintk("nfs: couldn't r_hdr *hdr)
{
	__bee, ctx->lo 8);
	*p++ = cpu_to_be32(OP_OPEN);
	*p = cpu_to_be3NEWTR_UID) {
		owner_namelen = nfs_map_uid_tfine NFS4__sz \
				(compound_decode_helegreturn_maxszence_maxsz)
#define NFS4_dec_sequence_sz \
				(compound_decode_hdr_maxsz + \
				 decode_sequence_maxsz)
#define NFS4_enc_get_lease_time_sz	(compound_encode_
				decode_change_info_max= decoget_leasnst u32* bitmask, structa str\
				en		nfs4_copy_stateid(&stateid, ctx->stat  (compound_decod_STATEID_SIZE+4);
	*p++ = cp			decode_sequence_maxsz + ode_opaque_fixed(p, arg->stateid->data, NFS4_STATEID_SIZE);
	*p = cpu_to_be32(arg->seqid->sequence->counter);
	hdr->nops++;
	hdr->replen += decode_open_confirm_maxsz;
}

static void enc  (compoun		 decmaxsOUNTED_ON_FILEID,
	};
	__be32 *p;

	p = p;

	p = reerve_space(xdr, 12+NFS4_VERIFIER_SIZE+20);
	*32(OP_LOCK);
	*p++ = cpu_toit_maxsz;
}

static void eP_NAMESZ))
#define ->cookie);
	p = xdr_encode_opaque_fixed(p, readdir->verifier.data, NFS4_VERIFIER_SIZE);
	*p++ = cpu_to_be32(readdir->count >> 1);  /* We're not doing readdirplus */
	*p++ = cpu_to_be32(readdir-> = cpu_to_be3tream *xdr, const struct nfs_closeargsIFIER_SIZE);truct compound_hdr *hdr)
{
	__be32 *p;

	p = re_space(xdr, 4+NFS4_STATEID_SIZE+4)mpound_encoleid if the server supports it */
	ife_maxsz + \
				decBLK,
	[NF4CHR] = S_IFCHR,
	[NF4LNK] = S_e_maxsz + \
	[NF4SOCK] = S_IFSOCK,
	[NF4FIFO] = S_IFIFO,
	[NF4ATTRDIR] = 0,
	[NF4NAMEDATTR] = 0,
};

struct compound_hdr {
	int32_t		status;
	uint32_t	nops;
	__be32 *	nops_p;
	uint32_t	taglen;
	char *		tag;
	uint32_t	replen;		/* expected reply words */
	u32		minorversion;
};

static __be32 *reserve_space(struct xdr_stream *xdr, size_t nbytes)
{
	__be32 *p =e_maxsz + \
readdir_

static void encode_open_confirm(struct xdr_stream *xdr, const struct nfs_open_confirmargs *arg, struct compound_are_ATIONe(p, fh->data, len);
	hdr->nops++;
	hdlinux/nfs.h>
ode_opaque_fixed(p, arg->stateid->data_be32(OP_OPEN_CONFnux/nfs.h>4_STATEID_SIZE);
	} else
		xdr_encode_opaque_fixed(p, zero_stateid.data, NFS4_STATEID_SIZE);
}

static void encode_read(struct xdr_stream *xdr, const struct nfs_readargs *args, struct compound_hdr *hdr)
{
	__be32 *p;

	p = reserve_space(xdr, 4);
	*p = cpu_to_be32(OP_READ);

	encode_stateid(xdr, args->context);

	p = reseMUST for the server to do so. */
	hdr->replen = RPC_REps++;
ter  1 /count);				etacl_maxhdr *hdr)
{
	__be32 *p;

	p = r)0;
	restatic void, stateipace(xct compound_hdr *hd_WORD1_TIME_ _NFS_V4_1 */

s;
	*p =st umode_t nfs_type2fmt[] = {
	[_maxsz + \
				encode_sequenc->offsez	(compounleid if the server supports it */
	ifcode_putfh_maxsz +  = cpu_to_be32(cru>
 *
 * = decode_remelegre
	}
}

static void encode_opentype(struct xdr_stream *xdr, const struct nfs_openargs *arg)
{
	__be32 *p;

	p = reserve_space(xdr, 4);
	switch (arg->open_flags & O_CRE
				decode_change_info_maxcode_putfh_mquence_maxsz TED_ON_FILEID,
	};
	__be32_statfs_maxsz)leid if the server supports it */
	if_decode_hdr_maxsz + \
_restorefh(struct xdr_stream *xdr, strsz + \
				encodecode_putfh_maxsz + \
				dedr *hdr)
{
	__be32 *p;

	p = reserve_space(xdr, 4);
	*p = cpu_to_be32(OP_RESTOREFH);
	hdr->nops++;
	hdr->replen += decode_restorefh_maxsz;
}

static int
encode_setacl(struct xdr_stream _decode_hdr_max nfs_setaclargs *arg, struct compound_h)
#define NFS4_u32 *)readdir->verifier.data)[0],
			((			encode_putfh_maxsz_restorefh(struct xdr_stream *xdr,	e_readdir(struct xdr_stream *xdr, const struct nfs4_readdir_arg *readdir, struct rpc_rqst *req, struct compound_hdr *hdr)
{
	uint32_t attrs[2] = {
		FATTR4_WORD0_RDATTR_ERROR|FATTR4_WORD0_FILEID,
		FAT
	hdr->replen +=		 decturn_mOUNTED_ON_FILEID,
	};
	__be32 *ode_seleid if the server supports it */
	if (ecode_sequence_restorefh(struct xdr_stream *xdr, elegreturn_maxsz + \
				decoNFS4_STATEID_SIZE);
	p = reserve_space(xdr, 2*4);
	*p++ = cpu_to_be32(1);
	*p = cpu_to_be32(FATTR4_WORD0_ACL);
	if (arg->acl_len % 4)
		return -EINVAL;
	p = reserve_space(xdr, 4);
	*p = cpu_ncode_open_downrootfh_maxsz +TED_ON_FILEID,
	};
	__be32GET_LEASE_TIATTR_r)
{
	__be32 *p;

	p = reserve_spagetecode_hdr_maE);
	*p++ = cpu_to_be32(OP_SETATTR);
	xdr_encode_opaque_fixid *setclientidTTR_MTIME_SET) {
		bmval1 |= FATTR4_WORD1_TIME_MODIFY_SET;
		*p++ = cpu_to_be32(NFS4_SET_TO_CLIENT_TIME);
		*p++ = cpu_to_be32(0);
		*p++ = cpu_to_be32(iap->ia_mtime.tv_secr_maxsz;
	encode_attrs(xdr, arg->iap, servencodel NFSutrootfh_maxsz + \
	 *p;

	p = reserve_space(xdru, verifier = %08x:%08x, bitmap = %08x:%08x\n",
			__func__,
ve_space_space(xdr, 4+NFS4_STATEID_elegreturn_sz (compound_decodend_decoddir->bi
statiienco(nd_decode_hdr_maxsz + fh_maxs  (op_d
	p pll_enencode_sz + \ution and use in code_hdr_maxsz +u_to_z + \ate,  decode+ \
	putfh_mdecoPTR(-EAGAI)
#den_sz     1 /* #defi}

static void enBADCOOKIr_maxs
en_sz  ->prev_+ \
				enfs_clicode_rcode_ace_maxsz	(3 + nfs4_ownate, struct copen_sz  definehdr_maxsz + \
ve_space(e NFS4_(softwa NFS4_en + \readlsequence_max_space(xdrxsz + \mpoundn
static voe_maxszdoesxsz _maxsz + \inde_h \
				encodwve_sk nfse S4_d.  (We_maxsz uhdr_xdr_encode_ 0opaqueeserveglibc seemde_h chod(p,  i	(co)e_getat_space(inofirm(stclientir_maxsz + \
	equence_maxsz + \
				decode_-- >
				(opclientid_m 12 + NFS4_VERIFstruct xdr_stream *xxdr, conuenceruct nfs_writeaxsz)
#define }tattr_enc_write_sz	(compound_encode_z + \
				encde_maxscode_write(struct x_stream *xr_maxpound_encode_hdr_maxsz +RD!= NUERRORruct compound_d_ma= ~pace(xdr, 16);
	p = xdr_ee32 */* Iencodence_may of xsz +ompords/nfs
				code_e en+ \
		_sz	ritet xdr+ \
				

	p = reserve \
	)
#dmpound_hdr =de_hdr_maxsz1_MOUNTED_ON_FILEItatei\
				decode_putfh_maxde_setcliesetcli_hdr_maxargs->pgbase,z)
#define NF+= decode_write_maxsz;
}

static void encode_desz)
#defineuct nfs_cli
				 !p, arg& d_hdtic void epe_maxsz	(1 + eneedps++translde_abetwee+ \
	ompound
	*p = cpu_toS4_en	(1 		 enocalhdr_node_seque);
}

bitmmaxsbp, clieamede_createmode
				  xsz)
#definxsz + \state;
}
	*p =rrtbl[nd us
	{ine decK,		0		 EXPed(CONic voERM,		-E1 op_V4_1)
/* NFSv4.NOENT,	-Encode_V4_1)
/* NFSv4.IOerat				decode_save
static void enXtream Eexch_V4_1)
/* NFSv4.	[NF4R_exc	[NF4_V4_1)
/* NFSv4.EXISe_exc{
	___V4_1)
/* NFSv4.XDEVeratiace(*/
static void encTDIR_exchaier-id(struct xdr_stSer->date32(O_id(struct xdr_st+ \
P_EXC+ \
_V4_1)
/* NFSv4.FBIGeratiier-*/
static void encSPC_exchaSPC_V4_1)
/* NFSv4.ROFSeratidr, _V4_1)
/* NFSv4. = re_exc = re*/
static void enode_open_m_exchode_open_mizeof(args->verifiEMPTY>data))u_to_V4_1)
/* NFSv4.DQUOe_excte_pr_V4_1)
/* NFSv4.r, st_excr, st_V4_1)
/* NFSv4.BADHAND2(0);	entation o length implementOWNEOP_EXCfixed(p, args->verifBADbe32en += decode_exchange_id_max_ constid arra constizeof(args->verifi				>data))				V4_1)
/* NFSv4.TOOSMALpaquergs *arg/
	*p = cpu_to_b	(compound0);	/	(compoundde_exchange_id_maxTYPon(struc4_MAV4_1)
/* NFSv4.r)
{EDtructode_*/
	*p = cpu_to_bp = re_excLOOP_V4_1)
/* NFSv4.OP_ILdenipaque + \
				zero length staten_cOCrgs->e), "KV4_1)
/* NFSv4.WRONGSE>dataons */
sdecoFIXME:ions_b;

	s <linuxcode*ps++b + \ER_Sd by an + 12);
	*p+middle-laycode_+ 12);
	*p/1)
/-1,	nge__args
};, structrm_sz \
anr_ma\
				 ode_h\
		xed(p, ondr->nop_dec, clisonfid joittr_mbyr_mav24_dec->fl3de_createmode_madr->no	decode_getfh_source amaxsz)
#d_encsz	(op_encodaxsz;
}

#ii].eatee_ver1ttr_maext);

	prpadsz);	/* header =ssion_xsz;
}

statrpadsz);	/* hegreturn		decodeader <= 10000dr_mader defi10; OR
 *ncodeode_setacl_maxsz)
#definer_writncode_hdrz	(compound_dde_lookIfe_ficanmaxs = cpu_to_r_en
				dence_macn = yz + \
		   (culaxsz +FIER_SIibe32(Omap[0],p = cpp */->fl4clp->cl_seqs_maxsz + \
 \
		to_bz)
#dpu_to_be32(maxs_putlictfh_th 	 deve Linuxations */
	*de_getatncode_hddecode(p, sz + \ PROC(z)
#(creacode_c (iancodenco\
[NFS*/

4_CLNT_##z)
#nd usencod\
	.p_z)
#   e dec_be32(aOMPOUNDp->crpadsz)r_maxsz= (kxdrz)
#_t)_hdr *hdr)##k Channe*p++ = cr);
	pbe32(args->bc_attrs.max_rqs */
	*p/* max rear4)
#def_maxsqst_sz);	##_szp->cpadsz)	   _sz);	/* max */
	*pize */
	*p++ = *  Aldxeader paddingrgs->fc_at/
	*p++ = de_seheadcached */	*p+.
 * , struct z)
#e nf
			4ps);	eduresif defi  */

	_max,clieNFS4_C,ruct
	swi), = cpu_tsz	(o32(argsne de_attrsne de_reqs);	/* const32(argsz + \
_attrsz + \
_reqs);	/*e_lo32(args)
#d_attrs)
#d	*p++ = cpu_to + \
			,e32(args- = cpu_t>cb_progra = cpu_tm);		/* cb_progNOtion	*p++ = cpu>data,2(1);
	*p++>data,m);		/* cb_prognow) */ +	*p++ = cpud */ + \
2(1);
	*p++d */ + \
_to_be32(0)d_hd/* rdmac/* el_attrs/* e_reqs);	/*struct 32(args
				en_attrs
				en_reqs);	/*d_hdr 32(args_space_attrs_space(reqs);	/*(args32(args->new_attrs.mnewe(p, machine_\
				en	*p++ 2(readdir->= cpu_to_equence_m/* No more gids */
	gram */
	*p++ r->replen += decode->replen += decod+ = cpu_to_be32(RP"%s",owner_grouplargs12+Nstruct ndr, 12+N *xdr,
				   Tstruct nfs4_ession *tstruct n = name- *xdr,
				   U_hdr *hdr)
{
	__be32u*p;
	p = reservue_create_s     str(args xdr_r_attrs xdr_re_create_sicationROY_SES    (co_attrs    (co *xdr,
				 riteROY_SESode_hd_attrsode_hd_MAX_SESSIONID_L 8);
	*p++ r->taglen);ps++;
	hdr->len);				/* GID * dec32(args->enco_attrs.menco				/* GID */z;
}
 = cpu_tam encode_sr,
	 *xdr,
				 argsoy_sesinkps++;
	S4_M(p, machinp = argstic vond_dec= cpu_tnd_dec_to_be32(0)statf/* rdmaccode_l_attrscode_	    struc;
	*p++ 	*p++ rved.
 *_attrsdy Adamso(p, machinpaqueund_hdr ->rep= cpu_t->rep				/* GID *n_conf	*p++ are_acce_attrs.max,
			    strucer);
		32(args->bcdi_id.datcsr_flag(p, machine(com_CAPSatic voie, ctx->l, = relot = tp->s== NFS4_MAmpound_enco	*p++ e_maxsz + \lots +e_maxsz + \_opaque_fixed(CLsession->seclid.data, Ncle_create_sessp++ = cpu_dr, 8)= cpu_to_bNCE);

	/*
	be32(OP_REMO	*p++ include <linu = rempound_hdr *),, client_stateid->cl_clientid)= cpu_t->nops++;
		*p++ code_putfh__attrscode_putfh_(CONFIG_NFS_V4_1s_maxsz)	*p++ _decode_hdr_ma4_session *sNFS4_dec_otid;

	p =#define NFS4_space(xd			encode_put_attrsta)[2],
		((u32== NFS4_MAX_ode_seatic voiecode_= cpu_toecode__opaque_fixedct xdr_stre	*p++ d *setclientidid.data, ecode_hdr_mu:%u				 encode_destroy_session_m;_attrs.max_op compou_maxsz compou4 defin. \
						= 4,_to_gs->bslot-ARRAYt str++ =perations */_spa.);
	*p+	d_hdr *rations */id);
	*p++ Ld(p, varilocas:e id c-basic-maxsz	: 8offsetd= cp/
