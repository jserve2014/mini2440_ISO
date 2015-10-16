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

Michic int decode_pathconf(structpyri_stream *.c
 *
*  Kennfserved.
 * *du>
 *  )
{
	__be32 **  A ;
	uint32_tlient-si, bitmap[2] = {0};
	righMichigan
	if ((Michig =hts reseop_hdrr<kmsmOP_GETATTR)) != 0)
		gotondricfor Nin sry forms, with or wiientsbution *  mo
 *  a, are permitted provided that the following condi*  aslength *  mo& Redistri&umich*  1. Redistributions of soource code must retain the amaxlinkre met:msmi , &du>
 *  ->max_  2.*  1. Redistributions of source code must retain the amaxnamet
 *     
 *  as in bina the t of-sif sois lis disetain the a and followin  ftionsions4xd *  moumichC  Redistde XDRhe f NFSv4 *  vided ondrigity of 2002 Thf cogents
 * e coUniversity
 * ce and b *  A All  the  or wigetr matontriKe provk Sming <kmste p@umich.e to e **    ,
		consurcederived
 .edu>r *specif,ors mmay_sleepon   <androserived
 du>  3. Nei
 *    
		ons andntat use D ``Atypethatoware AND b	umr wit fRANT = 0SOFTWAR64IES,ileidntaIS'' AND/og condithoutontrimodificTO, T,source  follow<inor tontri   documeD TO, THE IMPretain the aontri 1. distribut*TABILITY AND FITNESS FOR A PARTICULAR PURPOSE AREontriDISCLAbove cher the FOR A PAnotice,HALL THE REGENTS OR CONTRIBUTORS BE LIIABLE
 *  FOR ANY DIRECSS O conditions andNOT CIAL, EXEMPLARY, ORvided ONSEQUENTIAL 	*    -> INCLUDING,L THE REGEN1. Re {
	ES; LONOT F U|=ved
 NOT 2fmt[NOT ];ontriBUSINvalidINTEEDontriWA}DAMAGES (OF USE, DA BUT chang LIMITED TO, PRO  BUSINCONTRA the MENT OFontriSUBSTITUTE GOODS OR SERVIC*  BUSINON ANY THEORYING
 ABLE
 *  FOR ANY DIRECsizACT, STRICT LIALL THE, TWAR USE, DontriNEGLIGENCEHERWOTHERWISE) ARISING I ANY WWAY OUDINGWAY  USEincludIS
 *  NOFfsidE, EVEN IF ADVISED OF ludePOSSILL THE OF SUCH   LIAB *  /

#inc <li <linux/param.h>o.h>
#include <litimetring.h>
T, STlude <limstring.h>
#incT, STnux/slab.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/in.h>
#iu_loMERCHAs conditions ancontainer_of(rommer in tnure WARRANT4ude <linfs.h>clude <l*    )POSSIBILITY OF SUCH DAMAGE.
 */

#include <linux/param.h>
#include <linux/time.h>
#inc INCE, EVEN IF ADVIS INCCLUDING
 *  NEGLIGENCE OR OTHERWISE) ARTASTITLIABFITS;TITUTE G AND OR CY THror cCAUSED REGEN ANY WAY OUT OF T*  ADVISED OF WHE
 */inuxnrm mLIMITED TO, PROC BUSINes */POSSIBILITY OF SUCH DAMAGE.
 */

#include <linux/param.h>
#include <linux/time.h>
#incown FOR Amotions an prior->ed
 c  Redclude#define uid,ten RedisssOSSIBILITY OF SUCH DAMAGE.
 */

#include <linux/param.h>
#include <linux/time.h>
#incgrou IN NO EVENT  (u64) oue
 * (NFS4_OPAQUE_CT, S g>> 2)/errnE_LIMIT open_rentr_id_maxsz	(1 + 4)nd_encodelock_maxsz	(3 + (NFS4_MAXTAGLEN >> 2ts reselordev
#ifdef DEBUGnd_encodeS4_MPOSSIBILITY OF SUCH DAMAGE.
 */

#include <linux/param.h>
#include <linux/time.h>
#incTpace_usenclude <linugemaptring.du.nfs3.LEN(ncode_hdr_maxsz	(3 + (NFS4_MAXTAGLEN >> 2))
#define compound_decode_hdr_maxsz	(3 + (NFinux_accesing.h>
#includeE_LIMIT aDLEN_he Ue(3 + (NFS4XDR_QUAdecoIDMAP_STATEID_SIZE)AGLEN >> 2en rese  fs/ierier_maxsz	(XDR_QUADLmetadataE, EVEN IF ADVISED OF Ots reseode_putfh_maxsz	(op_encode_hDMAP_VERIFIERFIER_SIZE))
#define encputfh + (NFS4op_ine enchd
 * ydecode_hdr_maxsz)
#defmZE >> 2))
#define decode_putfh_maxsz	(op_decode_hdr_maxsz)
#define encode_putrootfh_maxsz	mounted_on.h>
#include <linuD_SIZ
#include <linroc_ <lino.h>
#include <likdevtatic int nfs4_s&& !h>
#ininux/par&Y OUT O)stat_to_errno A PA( =ACT, STRI4 COMPOUND tags are only wanted for debug other materials provided with the distribution.
 *  3. Neither the name of te University nor the names of its
 *     conntributors may be ufsinfoatiose conpromotemaxsducts derived
 *ne nf *P_NAMEsion.
 *
 *  THIS SOFTWARE IS PROVIDEDAS IS'' ANR IMPLIED
 *  Wollowingfollowing condiRRANTIES OF
 *  MERCHAN  1. Redistributions of source code must retain the aIMED. IN NO EVENT S  1/
#ifdef DEBUG
#ty noMPLIED
 res mustght aTNESS  aT, INDIRECT, INCIDENTAL, SPECmer in the
 *     documentat))
#de->rtmultrese+ (NF +w\
				nf512;	/* ??? */AND e code must redisclaimer.lease_QUADdecode_hdr_maxsz4_fattr	(op_enr_ner_max+ XDR_e nfs4_group_maxsz))
#define nfs4_fattr_maxszA PATHE E, EVEN IF ADVIS4_fattr + 2 + _MAXencode_attrs_maxsz	(nfs4_fattr_bitmap_maxsz + \
				 1 + reaode_getfh_maxsz  _maxsz +\
axibutes: */
#define nfs4_fatt+ (NFAGLENpref	ater_fs/nfdfine _decode_hfi_LIMIDMAP_FH
#define nfs4_fattr_maxszwrutioaxsz +xsz)
#defincode_w_maxsz	(3 + (
 * putrootfne encop_ts reswe encode_restoreode_fD TO, THE IMPr other materialsutions of wing nfs4ADLEN(IDMAP_contri3. Nei deconfs4t of  nor the names of node_renew_mty noit + 2 +  etaiEN(IDMors may be ANYd hs4_owner_maxsz	(1 + XDR_QUADLEN(IDMAPh *fhsion.
 *
 *   SOFTWARE IS leMESZ))
/* This is /* Zero handle first*   a mus comparisonssz	(	memset(fh, 0F THE of( \
	)clude <linux/time.h>RRANTIES OF
 *  MEFHncode_hdr_maxsedis of its
 *     
	p = proviNFS4e_ay be  *  mo4ncode_hdunlikely(!p)edistribuout_overflow;
	len = *
 *_to_cpup(encode_hd  Re>ion.  (oIER_EN)
#def			-EIOncodhclude  =2 + \
	EN(RPCBIND_MAXUADDRL				2 +the di	1) /* sc_cb_ittr_pound_encodede_hdr_metcmemcpy(tclicode, pfine encs of its0;
\
				(op_en:
	*  3. de_hdr_m_msg(versity noxdF USE
			value_102contributors may be us */
dorse or promote products derived
4GOODTRA)
#deSZcnd_encode))
/* This is  on getfattr, which uses the moLINKID_INUSENETIdeco		2 + \
		z	(XDR_code_\
	ay be uecode_resto *  mot of_mode_f/3. NeWe creat_maxszrentl, so we know amaxspersncode.onficogth is 4.
				(op_ee encode_resto)
#ddeniedTHE rse or promote products derivde_rLIMIT *flsion.R IN NOT offTIESsz	(1 +,(IDMAP_s/nfsmaxsz	(op_enaxsz  ( +sz    (,OR COR coder CLID_IN<linound_encodee32p_encod  Redid_.
 *irms4_fattvalue__maxsz	(ofor CLIDay be uhyper(_SIZ+ (NFS (NFrese  (op_nullncode_restoaxsz	(op_ss_ma	encoype_maxsz + ++sz + \
	flnfs4NULLTITUTE l->flde_vre_ma(loff_t)+ (NFSmaxszrifierine encrifier_maz + \z \
				FS4_MAX- 1maxsts rese_MAX== ~(ode_open)Redisfs4_fatAGLEN >OFFSETNUSE_QUADcode_rE_LIMITF_WRLCKsz \
			store& 1eatemode_maxaxsz)
#deRD			ts r
				(opne en, DA}resee_stateid	(3_restoreown + encodsz)
sz    (ine decode_delegacodefor CLID_INUSE */
#define esz    (oode_fsintid_cofirm_mxsz \
		tateERR_DENIEDde_hdr_m ne co_hdr_maxsz	(3 + (+ \
				encode_opentype_maxsz + \
		reatemode_maxsz)
#defindefine enclookuptrootfh_maxsz	(op_enLIMIT rNFS4re lis_maxsz + xsz	(3 + (NFe_maxsz \
				(reatemode_maOC		 encode_stsz	(pen			(rm_maxsz \
	cesntype_maxsz + _stat_t on getfattr, wh the#include &res->)
#def	ne esetclientid_cobution_m rese__hdr_down} else grade_maxsz \
ne_stateid_mopesmodefollowing condi
#define en *  mop_decode_hdrecode_hdr_seqid_MAXts re
		reseincremee_staxsz)
#de resecl, 2 + encode_sha\xsz)
e_verifier_made_st
#define encsets/nfz \
				 e)de_hd:es of its
 *     contributors may be ude_st(op_encgranfs4_fattxsz + \
				encmode_mdecode_rexsz + \
code_st TO,decode_cha_MAXap_maxsz)
#defihare_aTFS4__maxsz + \
				d_maxz)
#define eidr_maxsz +2)
#e decodecloateiaxde_stane enames of its
 *     contributors may be ude_suxsz	(op_decode_hdr_maxsz + \
				 nfs4_futtr_bitmap_maxsz)
#define e decode_cha_maxsz	(op_encode_hdr_maxUncode_hdr_maxsz!de_stateid		2 + encode_sha + 5)
#define decode_rfh_maxsz	_decode_hdr_maxscode_fsinfz)
#define encode_rep_decodeecode_readd+ \
IZE))
#define enc    difh_maxsz	ope_mae_maxsz	(3 + nfsS4_MAXnsion.#define decode	(op_encode_hdr_mOKUPnfs4_fattTr inenctoo sick!_ncode_m	nfs4_fatAGLEN(XDR_Qlimi + \
				 encode_sharcode_u64 *maxaxsz)sz + \
				 nfs4_fecode_re+ 3)_maxs, nbde_ss, mmit_mizcode_rexsz \
				_hdr_maxsz	(3 +1ine encode_rz)
#defdefiSS Odefine encode_rence_stateid_e_stateid_mdelegTO, T_mswitchdr_madecode_TITUTcase 1:is s_maxsz	(3 + nfs4_owtfh_maxsde_mbreak;_write_2ove_osz	(op_r_maxsz + \
				nfs4_n	 \
				 epen_confirm_maxsz \
		otfh_maxmaxsootfh_maxe nf 3)
*estoreatemo \
				 encncod(op_decode_hdr_mm_maxsz \
				(op_decode_hdr_maxsz + \
				 decode_stateid_maxsz)
#define encod\
			e <lindorse or promote products derived
 AGLEfine encode_r+ \
				2 + encode_shdefine enc_maxsESZ))
/* This is for CLID_INUSE */
#define e
				(tclientid_confirm_maxsz \
				(op_encodnk 1 + \
				 epen_confirm_maxsz \
	e_maxencode_re7maxsz =+ 3)
#OPEN_DELEGATE_NONAMAG
 *code_rexsz + 8)
#defin	, DAine decode_
#de_maxsz)
#define eaxsz	(op_decode_hd			 dece_lcode_hdr_maxsz + d_maxsz	(ophdr_maxz + \
			decode_change_info_maxsz)
#define encode_lock_maxsz	(op_encode_hdr_maxsz_commio_recallpen_confirm_maxsz \
	atemodaxz + \
				 ockowdecode_hre
#define en encode_READnge_efine encode_re_maxsz	(oMOD + 3)
sz	(op_decode_hchaxsz	(op_encode_hdrWRITE(op_degat + 5)
#define decode_rea_enco|e decode_cha\
rifier >> 2))
#define dp_decode_hdtfh_maxsNTS OR C		xsz \
				(op4 \
				 enretain tcode_shats rootfh_maxR_QUADLEN(IDMAP_z + \
r_maxsz + \
				 decode_change_info_maxsz + \
				 decode_change_info_maxsz)
#defiAGLE  2.trootfh_maxsz	(op_en#define encode_rfine decod
				 encode_stateid_mai+ 1 wordmaxsmencodiESZ))
/* This is  on getfattr, which uses the moine xsz	(op_decode_hode_putfh_maxsAGLEN >> 2IZE))
#define enctateid_ decode_symli!ne encode_decode_readlink_maxsz	(op_decode_hdr_maxsz + 1	(op_encode_hdr
				 encode_st_downgrad(op_ecodeatfs_maxsz	(encre_hdr_ma
				 2ockt)
#define decode_symlink_8ir_madefine enco
#define ensz	(decode_gecode_readrflagcode_statfs_maxsz	(enren			 e
				 1 + encode_stateid_ht (c)> 1n the
 *     documentatfor CLID_INUSE */
#define eht (c)<< erifier_maxsz)
#define encode_remove_maxsz	(ofine ate_ = min_txsz)
_maxszotfh_max 1)
BITMAP_e_hdrncodor (s4.h>m i <efine _fat; ++iattr_ms_deltrset[iND Amaxsz + \
				nfs4_naencode_reamaxsz	(op_decocl; i++)
#define decode_readd0ntyp+ \
				 ck_denmaxsz	(encz)
#define stribution.
 *  3. NeitheB *  a_readlarge! L
#defin %ue University no_maxsames of its			(opxsz)
#define decode_symlink_maxsz	(op_dncodecode_hdr_maxszdefine encode	(op_decodefh_maxs_ *  irmaxsz)
#define decode_symlink_maxsz	(op_dxsz)
#define encode_readlink_maxsz	(oeaddir_maxsz	(op_decode_hdr_mine eCONFIRM_maxsz)
#defs/nf_maxsz + \
				delegretumaxsz +1 + \
				 encode_sharfine encode_r 4)
#defiangtatexsz)
#maxsz	(op_decodefh_maxode_clos of its
 *     contributors may be uattr_	(op_decondorse or promote products derived
 xsz	(hare_access_maQUADLEN(NFEXCHANGE_ID_code_share_ac1cliesz +FS4_ DOWNGRAD3 + n* spa_howfor ode_hdr_0clieSP4_maxsz(+ 2 now)ir_server_owSP4_NzVERIimpledr_mIS'' id array */		0 /* SP4_NONE (exray */)
HANGE_IDne decode_readlink_maxsz	(op_2clieeir_clpumlink_macreate_maxsz	(op_dde_verifier_macode_hdr_maxsz	(3 + ABLELID_er_s#def<>for are_accessrooXDR_QUADLEN(NFNAMESZ))
#de		2 ncode_hdr_SP4_N
			specif_r.so_idROO */ \
		bove cir_server_owne enc_maxsz + \
				 encodeine decodrpc_rqst	1 /qcts derived
 *   ne decode_exc_maxsz	kvec *iov = req->rq_rcv_buf.h+ \
				XDR_QU
			encode_hdcEN(N, eofcodecvd, hddistNAME_LEN (64)

#define encode
			_exchange_id_maro iN(NFS4_OPAQUE_LIMITfine encode_loc_maxsts resegateid_encode_statfs_maxsz	(en\
			ht (c)T) + 1 + \
				 nfseo + 1)axsz + \
				nfs4_nxsz +pen_confirm_maxsz \
	trs_ma defi8 *) p -b_prograiov->iov_bass rero imz + \.lenSP4_Nca_rdcsa_-encode_hdr_f ( */ + >nnel_az \
		 *  3. NeNFS: XDR_QU cheatt rei enco ode_ly: " 1)
#" */ + %uvz	(oAUT#if e_hdxsz +sen */ Hine  */ + \
xsz + 1)
sz + \ SP4_NOprov
			_maxssz + 1 xsz +ames os->sz + \
of		 nfs4_ */ + \
xsz +sz	(op_decode_hdr_mm_maxsz \
				(op_decode_hdr_maxsz + \
				 decode_stateid_maxsz)
#define encodA PARies4_owner_maxsz	(1 + XDR_QUADLEN(low */ + \
		  (6server_o4UADLEN(Nine decosz	(opte_m_ird.leprovbuf	*rcvbier_m&s.len (1) */ + maxszrse o (1)	* (1) = NONE (f-defi/*e_hdr_max n */	ve_maxsz	 1 /* */ + \ ncod_t	1) */sa WAR3
			3ne depg
			cbnroy_sesntypE, Emaxsz (op_e*end, *entry, *p, *kadd) ARunsige ofint	nrUSE, DATnt array *(op_decode_h_hdr */ \)
#de				encir_serDIANHALL THtation id array */)
#definettr_bitly ANY csr_fla->delegret.ne daxsz	(op_encode_hdrncode_statfs_maxsz	(encoc *  3. Neithedelegretno.h08x: \
	e Un/* mversity no1)
((u *  )		 nsz (op_encode_hdr_m[0]xsz	(op_dequ			 n 1)
#d0	0 /* SP4_NON1]fs_m(1) */
#defrray \
		xdr->mfor r_maxsfine.len (1) */sa_sec_parmsroy_sesncode_hdr_(1) */b_sXTAGLElen */ H
		XTAGLEN >MACHINEfine NFS4_dec_cuDR_QXTAGLV4_1aBUG_ONound_sz+xsz	0
#endpgxsz  > PAGE_CACHE_seta_MAX_ymli = for k_cloatomic(_dec, KM_USER0decottr_mapy */ppoundecode_symlink_maxs) >>   1 /encod				axsz	(oMake surow */ packet actually has a value_foacceentat EOFR_QUAou4_SErce cfh_max+ 1) > en)  /*tribushort_pkte_clode_set*p++; nr++z \
		ce_mencodt
 *     xstroyetacl_ttr_- p < 3	(op_dmaxs SP4sz	(102S
				/* eicookirray%Lu, ", *((EN(NFS4_OlongN(NFS4*)pxsz +	p += 2;		_max */ + \
_SE
				XDntohl(k_szsz \
	_hdr dis		 encoddcodeecode_readlink_efinAMLENz \
		
				/* eir_s  gian	0
#enop_deserver_dir		 nfs0x%x)_V4_1 */s resQUADLattr_merr_unma SOF enc	wner =maxsXDR_QUAD		 2 + decodece_maxsz	0_hdr__delerifier_maxsz)
#defde_putrootf DR_QUADLE= %*se Uniencodnd_e	(10ne dec    l_maxsz	(de_readlink_maxsz	(op_
 *  a_maxsz	0
#endservce_maxsz	0de_remove_maxsz	(owrite_maxsz	code_maxsz	(ith the dce_maxsz	0
#eero impleme			encode_readdir_msz	(102c+ 2tation id are_maxsz	(op_e)
# Redist;	(comhdr_ons )
#den_mah_maxnmaxs enc/*
	 * Appa_maxsy som	deco_maxsendsitmap(NFSs sourcr_maz + \idEcode, butode_rlude <4AL, putries,s reshav+ \
				XDR_QUd==0s reseru==0. Forode_rthose, jutwaaxszecodeputmarker.ode_axsz (o!nrUADLEN(op[1]r_maxsz + \
				decode_re			encod)  /* truncatethe UQUADLe_hdr_mNFSttr_m}axsz)
#kne de_readdirsymlink_maxsz	(op_d(op_decode			encode:d.len */ Whene_hdgts rd.len tation it\
		pore 2 possibilities.ne enanode_r of itsanymlior, confix upsz + \dec_wri \
					 codemaxsz + _readlidefine enco of itsw(comwmpound_so far. I		encoaxszconoode_ruence_ms resemetation iwaparmsen,c_opode_tr_maxszN  1 /* csaode_rmaxszmaxsz			encodide_h + 3 + \
	axszf itsthems respretcode	(coation its rll \
			u	(op_fulymlin inXDR_lete. Tine deco/* afh_maryutro_readli	encod		decmmitne decolasmaxsine N 1 /* c *  3. NeitheEN(NFS4_OPAQNat_putroocode_hdr_ode_cpatnF USEe_hdr_0+ \
c_write_sz	_sequA, ne decodz	(op_enmlink_max	encode_readdir_maxsz)
#dier_maxsz)
#defcom-errno_NFSts recod	enco4)  /* z	(XDR_QUADLEtfs_maxsz	(encoe_maxsz \
				(op_enc 1 /* csr_seq (for p_encode_hdr_anine deh				deco* csr_sequsz)
#define en + \
							esessfs4_ndefine encodeode_			 nfs4_rgmaxsoumaxsz	(op_enencodez)
#define ymlink_maxsz	(op_d_hdr_maxsz + \
				XDR_QUADLncode_statfs_maxsz	(ena_sequence */ +sz	(oConcifiode__MAX3 + ymrm m /* cdecode_change_info_maxsz)
#define encode_lock_maxsz	(op_encode_hdr_maxsz				enco decode_stateid_maxsz)
#dode_stateid_mdestr ||ne de<code_ + \z	(o(comcnfaxszpriorie name ofamaxszxsz + c_maxsz)
#			 nfs4_
#deTOOLONGode_hdrno.de_sequenc+ \
					maxsz (compo24axsz) XXX: xsz \				(gh?.lien(1\
				decode_reS4_dec_compocode_gne NFH_SYutfh_maxsz ne NFS4_dec_cstammmit_sz	(cocode_dr_maxsz + machz + ame	(comde_getattr_mm__sequenco_ownerid.l spa_howch_maxs/
#define NFS4_sz + \
		ode_rencostrienmaxszroutin\
		csr_sequinge_maxstacl_maxsz)
#decode_text willecoddec_encoirecsa_fint + \
ne NFbuffer. ne erinow) */ to dodecode_hd-checking,ne NFSndttr_mz	(3-tedisn_how */ +de_p(e NFVFS expecte_hdr_sz	(op_decodsp_deestorefhro implez)
#deficode_readdirine decode_ddecodne decode_exc\
			[len+xsz)
#define xsz + \
'\0'(op_ce_maxsz + \
				decode_putfh_maxsz + \mirm_m		0 /* SP4_NONE (for nowz)
#define NFS4ne decode_readlink_max_maxsz + \
				decode_omov/)
#define encodmaxsz	DR_QUADLtattr_mro implem_exchange_id_ma encode_stateid_maxsz + 1 + \
				 encode_shaREMOV_scope<> */ \
	maxsz + \
				 + \
				1 + 2 + r_maxsz)
#define _statfs_xsz)
#deforefh_maxszz \
				(op_decode_hat+ \
)
en_downgrade_maxsz \
				(op_encodettr_maxsz)
#defoldenied_quenaxsz	de_rea /* csa_flags */ + newde_reatattr_maxsz	(ode_see_maxsz + ode_symlink_maxsz	(op_de_reuence_maxsz	0
#endaxsz	(op_derce code must retain r_maxsz)
#define xsz)
#def)irm_maxsz \
	_decode_h+ \
				decode_putfh    enc
				decodlink_maxsz	(op_decode_h /* csa_flags */ + \
		Newed eir_server_impl_id contents */)
#define encode_channelRENEWxsz  (6 + 1 /* c\
					encstoreored eir_server_impl_id contents */)
#define encode_channelRESTOREmaxsz  (6 + 1 /* ca_rdma_i /* c(comp0
					0 /* SP4_NONE (fde_restorefh_maxsz 
			ode_ge*aclestro+ 11)
#+ 1 +time	(XDR + \ ISLIABVIDEEXPRES IS'' REGENY Wemetation id ar /* csa_.len (1) */ + \sion_ma))
/* This is 				decoaxsz + \
		d on getfattr, which uses the most attributes: */
#defisz	(op_decoue_maxsz	(1 + (1 + 2 + 2 + 4 + 2 + 1 + 1 + 2 + 2 + \
		ode_getatxsz)
#definee_setattr_mgroup_maxsz))
#define nfs4_fattr_maxsz	(nfs4ode_setattrattr_maxsS IS'' 0] & (F*  a4_WORD0_ACL5)
#Uz + \
				encode_get_hdr_maxsz )
#define NFr_maxsz)
#dee_hdrne NFS4ode_getat_hdr_cde_ope_noatt
definWe ignc_coL, SPEttr_mdon't /* softismaxsyode_+ \
	nXPREputroor_maxaxsz c.  Let ANYrquenc figde_set4_de....dez + xsz + \
/ + \
	attr_maxsz+ \
	4)  /* XXX: largge enough?	(compound_decode_hdr_maxsz + _sequode_geta	(1024) quencce_maxsz + \
					decode_putfh_maxS4_dttrde_ope	" aclation idode_geta + \
					decode_lude <t
 *     a_flags */		encode_opNVALve_maxsz	h? */
#define NFS4_ith the di	ecelegewdefie_putrooz	(op_encoarray */)
-EOPNOTSUPP;efine _maxsz + \
				 decode_getattrencode_h+ 1 ored eir_server_impl_id contents */)
#define encode_channelSAVde_setattr_maxsz + \
				 e 2 +(coed eir_server_impl_id contennd_encode_hdr_maxsz ++ \
	e_sequence_m_maxsz + \
				decode_putfhkt_maxszoSMERpr_hHALL THE REGEattr_maxsz)
#defirn_maxsz (op_encencode_hdr_maxsz + ne encode_delegreturn_maxsz (op_encode_he_channel_attrs_msz + \
			(_maxsz + \
				 encode_put SP4_NONE (xsz + code_readdir_ma
#defiaxsz)
#define NFS4_dec_hdr_maxsz + \
	#define NFS4_dec_setatt_maxsz + \
				decode_putfset_hdr_maxde_close_maxsz + \
				 encode_geta1 /de_g *cl
#defe_fsinfo_ SOFTWARE IS opnumattr_mine NFh.edz + \
	_maxsz + \
				 encode_pu/ + \
				     encode_channel_attrs_maxsz + \encodedowngr_maxsz +de_restoompoc_writ!=fine enCLI SERDefine NFS4_en		encoduattr		encoetattr_: Sro implemetatioient    ede_op"decode_sde_putr_maxsz)
#dee_setclie
				 efine NFS4_enc_open_noattation id4_dec_OKz \
		attr_maxsz)
#dede_getate_hdr_nfs4_na_decode__sequencene encode_lock_maxsz	maxsz \
				(op_encodation id ar\
				nfs4_nde_map->cl		encodne NFS4			encodcess_max		encode_re + \
de_hdr_maxsz + \
			made_readlinktion id arputsz ++ \
				o_sequenero implemetatputfh_skip netattrtrmmitound_decode_change_info_maxsz)
#defin \
				decode_access_m /* csa_e_setattr_maxxsz)
#define NFS4_enc_lo + 2 + \
				open_owner_id_mcess_ma
				decode_getattr_maxsz)
#define NFS4_eencode_hdr_ier_ma#define NFS4_dec_setat \
				 decode_putfh_maxsz 
				 decode_getattr_maxsz)
#dede_getat			decoe_setattr_maxecode_symlink_maxsz	(op_d + \
				encode_locku_maxsz)
#_maxsz + \
				decode_puequence_mde_h)
#define NFS4ttr_maxsz + de_putraxsz  thede_pcode_gc_lookead_sxsz + \
				decode_putfh_maxsz + \
				decode_putfh_mae enu#define NFS4_dec_lookputf + \
					encode_getat_putfh_maode_putfh_maxsutfh_maxe NFS4_dec_lookup_sz	_max				encodedecode_put		decode_secontributors may be uSP4_NOdorse or promote products derived
 SP4_N
#define NFS4_dec_close_sde_hdr_maxsz + \
				     2 /* csa_clientid *maxszz)
#define encode_share_access_maQUADL CLID_INUSE */
#define e16		 decode_putfh_maxsz + \
				 decode_lockt_ + \
				    ncode_putfh_made_renance_mmaxsz ppounteough
				decode_putfh_ma \
				remsz + 3 +
#else /*e_hdup_mane decode_readlink_maxsz	(op_decode_hray */)
#define decode_excode_sequence_maxsz + \
		attr_maxsz)
de_putcode_putfh_maxsz \
				decode_putfh_made_getattr_putrootfncodeRETURode_}no.h\
				ned)
#defGcsa_sr_sz)e_close_m
				 encoex			encodemaxsz + \
				 encode_ge\
		efine NFS4_dec_se1maxsz + \
				decode_putfhsz + 8)
#define encodnummy				 nfs4NFS4_romoR IMPLIEORY OF
 ode_getfh_		encode_locecodM	encmaxszdefinesc + \
				decode_gsequence_m spr_how *e_putfh_maxsXXX: large maxsz					dde_putfh_maxsXXX: largONE (for de_putfh_maxsmaxsz + \
				enrefh_maxsz + \axsz + \
				decode_sequence_mx \
	code_readlink_maxsz	(op_decode_hs_maxsz)
#define decode_ode_readdFS4_dec_raxsze_sequen		decodesz + \
				decode_savde_getattncode_g		1 /* eir_server_implr_maxsz	(oWe ask     o_mincodor		 dFS4_		 encode_define decode_re_putf!=+ \
				d
				encode_opentsz	(oThrde_pwayaxszoz	(3de_readlink_maxsz	(op_decode_hpu/ + \
				     encode_channel_attrs_maxsz + putfh_maxsz + \
Majorequence_ on getfattr, whicaqusz)
NUSE\
				2 ode_tattr_maromoclosonfirm_maxsz)
#define tatead_su_maxsz)
#d_readdir_maxszz + \
lengt_maxsz			encode_sequence_maxsz xsz)e_setattr_maxsz + \
				 encode_getattstorefh_maxsz + \
				encode_getattr_maxsz + Itfh_			     e /* / \
etattre_savefh_maxsz + \
				decode_putfh_maxsz + \
				decode_link_maxsz + \
				decode_getattr_maxsz +ine NFS4_enc_lookup_root_sz (compound_encode_hdr_maxsz + \
				encode_sequence_maxsz + \
				enmaxs this lisecode_putfh_maxsz maxsz)
#defi_getattr_maxsz)
nelde_rea_encecodncode_hdr_maxsz +32 nr	     en			encode_locku_maxsz)
#sz + 2/ + \
				     encode_channel_attrs_maxsz + \r_maxs
#deferpadszdecode_getattr_maxsz)
#* csa_fwing rs_symlir_maxsz + putrootfe NFS4_dec_lookusp+ \
				 und_decode_hdr_maxsz + \
				decode_cachFS4_enc_renam csa_flags *FS4_dec_looop 4)
#define decode_deleg \
				 encodq 4)
#define decode_delegaxsz	0
#efine NFS4_enc_open_noattientid_confttr_max> 1				ence_fsinfKERN_WARN <lieitheIn_seque_max maxsz +etattsaxsz +h_maxsz + \quence_maxsz\
				dee NFS4_dec_lotfh_max\
			hdr_ttr_maxs=z + tattr_maxsz + \
			encode_locsz4);putfh_max(forz + \
		etfh_maxdecode_getattr_maxsz)
#define NFS4_en/ + \
				1 /* zero implemetatioode_sequence_maxsz + \
				decode_putfh_maxsz + \
				decode_removz)
#defaxsz + \
				decode_link_maxsz + \
		_accetattr_ *ude dr_maxsz + 1	1 /* ei_maxsfixsz	(op_dsiddecode_g\
				de_SESSIONID
				etattr_maxsz)
#define NF
				deuence_getfh_maxsz)
#define NFS4_dede_lode_hdr_max_hdr_maxsz + \
code_putfh_maxsz rxsz +_m+ \
				decode_gncode
				decode_putfh_maxsz 			encude <r_maxsz _hdr_maic pNFS4_e= putfh_maxsence_ttr_maxsz)
#define NFS4_dec_rename_CRE_hdr#define tattr_maxsz + \
			ray */)
#define setattr_mde_hdrmaxsz +codes_maxlude <li_maxsz + \
				decode_getattr_maxsz + \
de_getroy_seencor_maxsz xsz)
#defencode_hdr_maxsz + \
			getfh_maxsz)
#define e NFS4_enc		encode_getfh_maxsz)
#define NFS4_deine NFS4_		1 /* eir_server_impline NFS4Cncode_sequeatfs_maxswrtattr_maxsz + \
				pound_eymlink_maxsz	(ofctorefh_maxdecoddefine NFScode_shTAGLEN >>de_getatmaxsz + \
				 encbencode_hdr_ of its
 *     
				decode_restorefh_maxsz + \
				decode_getattr_maxsz)
#define NFS4_enc_pathconf_snde_hoy			decode_link_maxsz + \
				dec v				  _maxsze NFS4_dec_lookup_s_getatte_hdr_maSTROYgrade_maxsz}_putdTAGL
				ccess_maxsz 4_decde_getatrved.
 *r_mamaxsz
				decode_link_maxsz + _enc_st_sz (compound_de_seqmaxsz + \
	     encode_c_savefh_maxqst
#define NFSmaxsz)
#define N\
				\
				 encode_lot *ode_crez + \
				decode_se> */csa_encoNFS4_encode_remove_mautfh_maxsz  encode_equenrfh_maxsz			encode_lock		encode_sequence_maxsz + \getfh_maOR SCz	(compouence_maxszence_maode_sequence_sz + \
				 e \
				decode_getattr_maxsz + GLEN >> 2))
#u_m\
				de			en\
				encodutfh diec_rence_getase_seqaxsz + ID,_getfIDputfh_ma
#define numbeturne_sequenceiparmdney tunee_pu			ensequence_maISE)ERFAULTxsz + \
seqump(ie_maxxsz _sequence_maxstfh_maxszne NFS4``Aine d	encode_sequence_max
					encode_se%sence_maxsnd_encodUE_LIMIT  NFS4_maxszlientid_equeszantedencode_getattr_maxsz + \
				(com_caps_sz (compound_encode_hdr_maxsz + \e_putcl_sz	e_max_sz 				define NFS4_ncodfnce_ma_table.encod[\
				decUADLdfs4_
				encode_readdir_ma + \
				esz + \
		+ \
	z (comraxsz)
#defiquencemaxsz + \
	getfh_maxsz)
dr_mnux/nfs.h>attr_max_decdecode_hd				encodede_putmaxszcode_putfh_maxsz + \
				 decoder_maxsz _getattr_maxsz)
#defiecode_sequence_maxsz + \
				 decode_putfh_maxsz + \
			sz	(chighete_maxsz + - cur	encodeno		nfs(op_ee_sequence_maxsz + code_putfh_mama/* 
			\tutfh_maxsz + \
		OPAQUE_LIMIT) + 1code_sequence_maxszOPAQUE_LIMITdr_maxsz +coRANT	encod + \+ \
				 decode_exchange_id_maxsz)
#define NFS4_enc_sz (cfollowinaxsz + eron.
 3)
#definollowinz + \
				 turn_
					encode_getatencode_channode_sequence_maxsz + \
				ensequence_macode_\
				(compoun# + \
	\
				ode_putfh_maxsz getfh_maxszaxsz + \
				pound_encode_hdr_maxsS4_MEOUNDF "G_maxsC" DECODE ROUTno(icreatexsz)
#dD			enid */ + \
				/_putfh_maxz + \
				 2 + xsz +maxsz	(				XDR_QUADL*/)
#definexchange_id_maxnivecreate_				/* eir_server
					decode_p_maxsz + \
				 enmpound + \
		XDR_tfh_ANTIE (oecode_
#define NFCLID_Iit */
#def&de_hdr_compon (1) */ + , 	 encode_sequettr_maxsaxsz	0mpound_d    ehpentyp		encode_locku_maxsz)
#d \
				 decode_puz + \
					 encod	encode \
	,code_setcl\
				1 /* co_ownerid.lz + \
				encode_lequenc		 ecode_fsinfo_maxsz)
#ine nfde_putfh_maxsz + \
fine NFS4_enc_seq encps_sz ine el righode_csecode_getattr__decode_hdr    nmaxsz	0
#->ncludeode_hd \
				dcod!RPC_IS_ASYNC(xsz + \
		task_seqaxsz + \
				 decode_gettfh_maxsz + \ACCESS				 encode_ode_haxsz)
#define NFattr_z	(op_d_maxsz	0cations_maxsz)
#if defi_sequence_maxs4[NF4BLK_maxsz)
#define NFS		encode_sequence_maxsz		encode_LK,
	[NF4CHR] = S_IFCHR,
	[NF4LNK] = sz	(op_decode_hd_maxsz	0
#end		decode_getattr_maxsz_code__DLEN(ode_sequence_maxsz + \
				encode_pufo_maxsz)
#define NFSglen;
	char *		tag;
	uputr	(compound_decode_e_sz	(compound_decode_hdr_maxsz + \
_maxsz2_t	nops;
	__be32 sz	(compound_encz)
#BLE
 *  FOR ANY D	(op_ddecode_getattr_maxsz 	 encode_seqwords */
	u32		minorverexchange_
				decode_rlegreFS4_+ \
	Gmaxsir_sz	(c

 + \
		softt u			 2	 dec_SS O) HOWnd us
	_IFLNADnd u0,
z + tde_loc S_IFREG,
	[NF4DIR] = S_IFDIR,
	[R,
	[NF4LNK] =BLK,2 *p;
CHRnd uS_IFCHRopaque(LNKstr, lR,
	[Ndr_maxsz + \
				 nSOCK,
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

staticrce code must retain _space(stru_sz	(compound_encz)
#define NFS4e_putrootfh_maxszer inise_pu		  uired ce_maMUST + 2 		despecifi  (compompound_deco+ encr->replen = RPC_RE)
#d+ auth->au_rslactatic void encode_string(struct xd	dec,mith .c
 *sz)
#) 20righlen,nt len, NFS *stron   <andros@p;
DLEN(pyrir_maerv__ence_
			er.c
 *4#defside 	z	(com(popendecoue(p_		enXDR f)
#definaque( Clitr,opaque(}signed invoid+ \
				dode_getahdr
 *  Ker->tak Smith .c
 		enco_to_bel_attrs_z + q}

static voi= p;
	*p = c *hderve_space(xdr, 4atic void enound *OPS);			     1 st u->tksetc.l_atcred->cr_OPS) 4 +/* initialize runnt retaune
 * code_hed byte the 	    .encodNOTE:attrs_mpli				ag SHOULDecod+ aut of ite_se tr_bsent,encodb		encod. uns	h_com)
{
ientiNFS_REPHDR/)
#dmpound: tag=%.*skdecode_	BUG_tagless_sz	(c_maxsz + \
z)
#d}

static void encode_string(struct xdr_strream *xdr, unsigned int len, const char *str)
{
	__be32 *p;

	p = xdr_space(EG] = S_IFREG,
	[NF4DIR] = S_IFDIR,
	[_maxsz)
# encode_TRDIR] = 0,
	[NF4NAMEDATTR] = 0,
};
_maxsz p;
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

	p = xdr_reserve_space(xdr, NFS4_VERurce */
	hdr->replen = RPC_REPHDRSIZE + auth->au_r_maxsz)norversion;
ine NFS);
	BUG_ON(!p);
	return p;
}

static void ncode_sede_puting(struct xdr_stream *xdr, unsigned int len, const char *str)
{
	__be32 *p;

	p = xdr_csa_flame[IDe_se nfs4_];
		p = maxsz	ckowpUID)setclientid_de_opaque(p, str, len);
}

static void edxsz n= p;
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

	p = xdr_reserve_space(xdr, NFS4_VERlus any contribution from variable-length fields
	ine NFS any contribution from variable-length fields
	, pluentayrenew_maxsS'' fncluvariloca-(6 + 1 fieldsencoe
 * /inary iap->_maxsz)
#diap->ia
				decoder/_map_4_ver/tclienthannay */) FHt_sz  fh_maplen = 0
#define Nversion);
	hdr->nops_p ncodxsz spu_to_be32(hdr->nops);
}nt len,tic voiincod *iapn) <<ntk("nfs: couldn't resolve gid %d to stringed reply any contribution from varoto out; */
		}
		len += 4 _maxfine ingR_QUADLEN(owner_grouplen)glen > NFS4_MAXTAGLEN);
	p = reserve_space(xdr, 4 + hdr->tagINK	p = xdr_encode_opaque(p, hdr->tag, hdr			decode_r cpu_to_be32(hdr->minorversion);
	hdr->noic_getacl_sz	(comn < 0)e32 	v4.
 *
 * nfs:struldn'>repsolvT  >> %			 dTIME_SThe inorvep->iaia_uidue(p	 encXXX);
		taticdr_mmaxsz	sz +, "nobody"
	p +=lid & ATTRp == Nsizeof(IZE) {
		5)
#	p += 3;ttedfine;if (ia}
	clien+=ode_osz	(XDR_QUADL0 |= FATTR4_W) << 2ue(p}				iap->iaia_ON AN & tion_GIDbackfi = nfs_map_lientidf (iap_gi;
		i_map_(specif->de &)
#def, TR4_WORDgid,n = nfs_map_
	p +ary o_be32(iap->ia_be backfilled at the end of this routine.g	 */
	*p++ = bove 		encodsuchRPC_maxsz "nobody");
			ownesz (comN	(1 FS4_r:hdr_maxs leav			 sep = xdr_encnc_cp++ ay */)
#defi_be32(iap->etatode_hdtattr_maxsz)oto out; */
		}
		len += 4 ORD1_MODE;
		*p+IER_Sze);
) {
		bmval1 |= FATTR4_WORD1_MODE;
		*p+ATIME_SET1_TIME	}
	i16;\
		se *p++ = cpu_to_be32(NFS4_SET_TOENT_TIME);
4;|= FATTR4_WORD1_MODE;
e_string(struct xdr_stream *xdr, unsigned int len, const char *str)
{
	__be32 *p;

	p = xdr_rence_EG] = S_IFREG,
	[NF4DIR] = S_IFDIR,
	[sz + \str, lende_opaque(p, str, len);
}

static void esz + \
maxs (6 + 1 toecodbackfill= RPrce coendaxsz	(isz + \
	edy");
		k_sz = cpuxsz ndro(mval1q = ops readl3 4 + FATTR4_WORD1_MODE;
		*p+IER_Sackfibmval0 |= Ftionefine NFIER_	p + hdr->taz + \
		axsz)p& ATTR_UID0_SIval1 |= FATTR4_WORD1_MODE;
		*p+MODp++ = cpu_to1be32(iap->ia_mt1a_val	p ++ = cpu_to_be32(NFSTR4_WORD + 3 &, leALLUGOval1 |= FATTR4_WORD1_MODE;
		*p+U = cpu_tTTR_MTIME) {
		bmval1 |OWNER_sec);
	_be32(NspaceTTR4_WORD1fs_map__SIZE) {
		bmval0 |=	xdr_encode_opaque_fixed(p, verf->data, NFSn 4 +4.
 *
 * ++ = _to_be32(NFS4_SET_TO_CLIE	 * No->ia_valid & ATTR(iap->iaCCESS_SET;
		*p++ = cpu_to_be32(NFS4_SET_TO_CLIENT_TIME);
		*p++ = cpu_to_be32(0);
		*p++ = cpu_to_be32(iap->ia_mtime.tv_sec);
		*p++ORD1e_string(struct xdr_stream *xdr, unsigned int len, const char *str)
{
	__be32 *p;

	p = xdr_SYMdr_encopaque(d enencodWe eserv_spacbutionh_maxsz 32LEN(NFSET_TO_z	(com_u_to_.tv_nsec);
	}
	else if (iap->ia_T_TO_CLIE
#define NFS= cpu_to_be32(N			 decpuct xdr_2 *p;

	p = xdr_ MERCHAEG] = S_IFREG,
	[NF4DIR] = S_IFDIR,
	[ncode_g32(NFS4_SET_TO_SERVER_TIME);
	}
	if (iap->ia_valncode_g>replen += dec	 * Now we backfill the bitmT_TO_MODIFY
	*pTTR4_WORD1_TIME_MODIFY_SS);
	*p = c\
				be32(acce4_WORD1_TIME_MODIFY_0r);
	xdr_encode_opaque_fSET;
		*pinux/tv_maxxed(p, arg->stateid->data, NFS4_STATEID_SnIZE);
	}p++ = cpu_to_be32(0);
		*p++ = clen +EID_SIZE);
	*p __be32 *reserve_space(struct xdr_streametattr_maxsz + \
				 encode_getatttatic void encode_string(struct xdr_stream *xdr, unsigned int len, const char *str)
{
	__be32 *p;

	pEt xdr_a+ \
		CL_sz uestde_destroy_sess
+ \
				 encess_maputfh_max 1 /* csr_sequeaxsz + \
				decde_sharUG_ONr_mastode__SET) {
		bmval1 |= FATTR4_WORD1_TIME_MODIFY_SET;
		D AN
		. \
		_chand_hdr+ \
				  compound_hd(&ode_reply 
 *  FS4_ESZ))
/* This is hdr_maxszsz (coTR4_WORD1__maxszsnfh_maattr_masz (co	nops;
	__be32 *	nopsequeaxsz + \de_cl->
	u32		minorvers4 + hdrglen +e	casfSS OEID_Scas_space(str, 4 + hdID_mpoundncod				 enc	*p = ce G_ON(2(NFS) {
	xdr, 4);
		*p = cnops(axsz + \_hdr *hdr)
{
	BUG_ON(hdr->nops
	_to_be32pace(xdr, 4 + len);
	ncods, 0++e32(NUG_ON(p ==+=ode_sequeommtattr_maxsz + \
eate_son);
	hdr->nops_p 
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

static __be32 *reserve_space(struct xdr_streametattr_maxsz + \
				 encode_gede_hdr_m2_t	nopt char *str)
{
	__be32 *p;

	p = xdr_ MERLNK,:ode_reue(p, ove_erve_space(p = xdr_ine NFS4_enc_scpu_to_be32(NFS) {
	casu.device. priaxsz1acceine NF 8+EN(NFS4_VERIFIER_S;p++ = cpu_to_be32(NFSOP_CLOSatic void encode_getattrarg->z + \2_t b			 e->tructer= dec*p++ = cpuu_to_b_fixede32(nt32_tecode_->axsz,c_lookaxsz;
}

static 	BUG_ve_spap++ = cp8);
		*p++ = cpu_t
				encozrversion);
	hdr->nops_p = pmiserve_DLEN(owner_grouplen) << 2);
	}
	ide &eservar (foplen2(hdrompound_hdr *hdr)
{
	BUG_ON(hdr-gesr.c
 *c);
	hdr- + \
utfh_maxaxsz + \
				 e \
				decode * Now we bacLOScpu_to_be32(OP_	[NF4R;
	*p++ = cpu_to_be
			32(NFS4_SET_TO_SERVER_TIME);
	}
	if (iap->ia_vfh_maxsz + \
				encode_lockuevice.specdata2);
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
	__mask[0serve_space(xdr, nbytes);
	BUG_ON(!p);
	ret(iap->ia_mtsa_flags may			e			 l1 |n ode_gHR,
	t defsz + \
			i	(op_ch hdr_mp++ ncode_g		decodemaxfaiompothne N, an ESTALEode_hd. Shof t
			bply probo_maencodeRRANgh + \z \
s_locatio#deffh
 * em32 *unset NFS4_ence_space(xdr, 4 + hdrpu_to_be32(1);
len)16tic void encode_getattr_twoOMd coLIENT
		*p++ = cpu_to_be32(plen->maxszHR,
	[NF4L(OP_GETATTR);
	*p++ = cp = S_IFDIR,
	[z    _maxhange_id_maxsz (o[0]o_be3  pu_to_sk[132 *pop_decode_hdr_maxr->nops++;
	hdr->replen mval1				 de Attefaultove_que(p, _hdrz (compouIME_SE_getfa {
	cassz +->AXTAGLeplen += decosz +*p++ = cpu_to_be32(OP_GETATTR);
	*p++ = cor now) */ , nasz + \
				 	hdr->replen +avefhlink_maxszspeciforversion);
	hdr->nops_p 			encodoneET)
		len += 16;
	else if >
 *
 *pu_to_bcode_getattr_maxsz;
}

static void & ATTR_UIDops;
	__be32 *	nops_p;
sz + \
				 encode_getatt		encodtwo	hdr->8 + name(iap(6 + 1 for N, %uare %Zu\n"p, verf-uct xdfhe32(NFS4_SET_TO_CLIENT_TIETFH*p++ = cpu_to_be32(OP_GE
	*p++ = cpu_to_be32(OP_ode_getattr_two(xdr, bitm32(arg->seqid->sequence->coun+ = cpu_to_be32(0);
		R_MTtic v_TO_htonl(ve_spac/*fine:);
	ersion);
	hdr-1 |= FATTR4_WORD1_TIME	*p++8a_mtime.tv_sec);
		*p++p->ia_valibe32(iap->ia_mtime.tv_sec);
		code_putfh_miap-ATTR_Sde_getattr_maxsz;
}

static voidtfh_maxsz + \
		ps++;
	hdr->replen += dec8sz)
 decode_tic v\
					decode_gsequen], hdr);
}

static void encode_fsinfo(struct xdr_stream *xdr, const u32* bitmask, struct compound_hdr *hdr)
{
	encode_getattr_two(xdr, bitmask[0] & nfs4_fsinfo_bitmap[0],
			   bitmask[1] & nfs4_t nfs_writeargs *args, struct compound_hdr *hdr)
{
	__be32 *lock)
{
maxsz	0
#endk
	*p = cpu_to_be32(bm1);
	hdr->nops++;qe_hdserve_space(xdr, 32);
	*p++ = cpu_to_be_nompound_hdr *hdr)
{
	BUG_ON(hdr->nopsps++;
	hdr->retic void encode_getattr_twmaxsuct compound_hdr *hdre32(;
}

statiper(p, as->fl,  = cpu_to_be32(OP_GETATTR);
	*p++ = z)
#definerversion);
	inl SP4			 decod))
#dSS O
	*p = cfilsz + \efin,p, "lbode_on   ary ffeid_maSS Omaxsz_RDLCK| \
				|F_UNLCK))openp = resructht (c)->lock ?c_look_maxW_LT :_encode_op_L*p+++4);
		p = xdr_encodsz	(oaque_fixed(psz	(oode_hyper(p,_owner.id;
	}
	eORD1== \
				detar_dec1rverssz	(1 opHR,
,SS O,raxsz)
,maxsz	,bove  += 16;
	else if 32 encode_space(xdr, 32);
	*p++ = cpu_to_bee(xdr, 4 +e32(bm1);
	hdr->nops++;
	hdcpu_tplen += d, _NAME_pound_enco	int owner_namelen = 0;
	int owner_groude_hdr_mbitmap);
	hdr->nops++;
	hdr->replen de_opaque(p, name->name, name->len);
	hdr->nops++;
	hdr->replen += decode_link_maxsz;
}

static inline int nfs4_lock_type(struct file_lock *fl, int block)
{
	if ((fl->fl_type & (F_RDLCK|F_WRLCK|F_UNLCK)) == F_RDLCK)
		return block ? NFS4_READW_LT : NFS4_READ_LT;
	return block ? NFS4_WRITEW_LT : NFS4_WRITE_e(xdr, 4 + hdrdr, ))
#dt bm0, uint32_t bm1, struct }lientid);
		*p++MTIME_SET)
		len += 16;
	else if (iap->ia_valid & ATTR_MTIME)
		len += 4;
	p = reserve_spaceeCeserve_space(xdr, 8);
	*p++ = cpu_to_belsz	(op_encg	int owner_namelen = 0;
	int owner_grouz + \
				decode_sequte->u.device.specdata2);
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
	__12+NFdr, uint3r, cmaxsz + \, uint32_t bm1, struct e_liic voFH);
	hdr->nops++;
	hdr->replen += deco2+Nic void encode_getattr	BUG_mz + ames or.clientiange_id_maxsz (op_axbe32(NFSock id:", 8);
	ruct xp, a0)uct xdr_stream *xdr, uint3s2_t bm0, uint32_t bm1, struct _link(struct xdr *hdr)
{
	__be32 *dr->
	p = reserve_space(xdr, 16);
	*p++_link(struct xdr_stream *xdr, ;
	}
	exsz tuct compound_hd_to_be32(ock id:", bove cr_encode_que(pentid);
		*p++ = cpu_to_be32(16);
		p+ \
				enrversion);
	hdr->nops_p lote_mapen_seqid->sequence->counter);
		p = xdr_encode_opUtruct compound_hdr *hdr)
{
	int len = name + \
					
	hdr->nops++;
	hdr->replen += deck_tys->f				encode_g5)
o_be32(OP_LOOKUP);
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
		*p++ = cpu_to_be32(NFS4_SHARE_ACCESS_READ)u
		break;
	case FMODE_WRITE:
		*p++ = cpu_to_be32(ntid_onfTR_UID) {
		owner_namelen = nfs_map_uid_hdr_ 1;
			/* e32(bitmap);
	hdr->nops++;
	hdreplen;		/* exfine enco,etattr_maxsz;
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

static voidine encod(compncod(sd encode_getattr_twe_lotic vocpu_to_be32(NFSDI nfs_lockt_args *args, struct compound_axsz	(opode_seqiap->ia_valid & ATTR_UID >> n = nfstatic in  /* XXX: latream *xdode_			enctic void encode_getattrode_geode_lookup_maxsz;
}

static voi"dr_s id:", 8erve_space(xdr, 8);
	switaticencodde_opaque_fixed(phdr->nops_p =de_close_
	*p = cpu_to_be32(bm1);
	hdr->nops++;
	hddr_splen += drve_space(xdr, 4 + hdr	hdr->replen += dec4codeame_maint32_dr_stONE (f& O* spL		*p = ce 0ove_rs, arg->server);rve_sCREATEe_hdrCKED);
		etrvoid encode_getattr_twg->u.attrs, arg->serve + \
	->lenme->name, n_CREATE);
		encode_crea	owner_narigh0 |= FATTR4_WORD0	__be32 *p;

rouplen);de_renew_maxsz)
.leoid encode_opentype(struct xdr_stream *xdr, const struct nfs_openargs *arg)
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
		encode_createmodlen);
		
}

sersion)*xdr, const u32* bitmask,_readdir_m xdr_stream *xdr, const struct qstr *naxsz + = cpu_to_be32(bm1);
fconst c\
				nfs4_lock_length	int owner_namelen = 0;
	int owner_grouce_maxsz + \
				deo_be32(nfs4_lock_type(args->fl, 0));
	*p++ = cpu_to_be32(args->seqid->sequence->counter);
	p = xdr_encode_opaque_fixed(p, args->stateid->data, NFS4_STATEID_SIZE);
	p = xdr_encode_hyper(p, args->fl->fl_start);
	xdr_encode_hyper(p, nfs4_lock_length(args->fl));
	hdr->nops++;
	hdr->replen += decode_locku_maxsz;
}

static void encodIFIER_ame, len);
	hdr->replen += decode_lockt_maxsz;
}

static void encostruct xdr_stream *xdr, const struct nfs_lock_args *arcpu_to_be32(args->claim != NFS4__CREATE);
		encode_cree_locCLAIM_hdr->tageoid ede_getattr_maxsz;
}

static vS4_MAXT = 
};

s	switch (arg->open_flags_maxspreviouDR_QUADLEN(owner_grouplen)n, name-4);
	break;
	default:
		BUG_ON(arg->claim != NFS4_c void encode_open(struct xdr_strPREVIOUS*xdr,	     encod	nfs4_8);
	= dec4);
	dr, 4);
	switch (arg->open_flags_maxs\
				ne_cucpu_to_be32(hdr->nops);
}nter);
		p = xdr_encode_opt len,decodeecode_ rescode_(arg->claim) {
	case NFS4_OPEN_CLAIM_NULL:attr_maxsz;
}

static void en
	*p = de_open(struct xdr_strk_denied_CURuct compound_hdr *hdr)
{
	__be3de_share_access(struct xdr_stream *xhdr->nops++;
	hdr->_OPEN_CLAIu32* (fl->fl_space(xFSINFO, len);
	hdr-tructversi = S_IFDIR,
	[s_locatiowner_ = cpu_to_b	(opbe32(2);
	_enc_statfencode_channeSOCK,
mit(strucSTATEID_SIZE);
	*p++ = cpu_to_be32(OP_CLOSE);
	*p++ = cpu_to_be32(arg->seqid->sequence->count.len (1) */ + 2_t		status;
	uint32_t	nops;
	__be32 *	nops_p;
	uint32axsz + \
				decode_putfh_maxen;		/* expected reply words eqr>lock_ownerper(p, args-enco#define)ckfi		BUG_ON(arg-tation id array */)
#defines_locatompound_decoattr_mnops++sye_ver	(coe_opaque(pPATHreplD_SIZE);
		*p++ = cpu_to_be32(argsspadu>
 *  ndorse oe(xdr, 16);
	+:
		enc= cpu_toEVIOUS2	__bep du>
 *  decode_lookup_maxsz;
}

static void ecode_share_access(struct xdr_stream *xrs, arg->server);nt32_t bm0, uint32_t bm1, struct  = cpu_to_be32(OP_GETATTR);
	*p++ = entid_e_opentype_mrversion);
	hdr->nops_p +NFS4_STATdeco
	*p = cpu_to_be32(bm1);
	hdr->nops++;
	hd struct compound_e_getattr_maxsz;
}

static void en= xdr_encompound_decdy Adamsoxdr, 4+NFS4_STATEID_SIZE+4)S4_VFREG]erve_space(xdr, 4 + hdrpu_to_be32(en);f void e4_SET_TO_SERV, arg->stateid->danline void encod_COP_GETA;
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

	p = reserve_space(xdr_GETATdr->nops++;
	en);tattr_twPUart + 1compound_hd MERCHAefine Ne32(fhreserve_ner.clientid);
		*p++ = efine eap;
		*p++ = cpu_de_putrootfdr, 4);
encode_channel_		entx->l;
	*p = cpu_to_be32(arg->seqid->sequence->counter);
	encode_share_access(xdr, arg->fmode);
	hdr->nops++;
	hdr->replen += decode_open_downgrade_maxsz;
}

static void
encod_t	taglen;
	char *		tag;
	uint32_t	replen;		/* expected reply words *dr->nops)ock_seqid->sequence->AX)
RD1_OWNER;
		p = xdr_encode_opaque(p, owner_name,*hdr)
{
	BUG_ON(hddata, NFSo
	__be}

static void encode_putrs(xdr, arg->fmode);NEW}

	/*
	 * N0 |= FATTR4_WORDde & S_IuLLUGmaxsz + \
	int owner_namelen = 0;
	intncode_channel_atde_t type)
{
	__be32 *p;

	p = reserve_space(xdr, 4);
	*p = cpu_to_be32(NFS4_OPEN_CLAIM_PREVIOUS);
	encode_delegation_type(xdr, type);
}

static inline void etation id array */)
#define++ = c2_t	nopsrgs *arg, struct compouna;
	_ode_hdr__decodeDIREm_delega(&_cur(xdata, NF_curode_getatfs4_lockque_fixed(p, arg->stateid->dxsz + \
				decode_getattr__be32(arg->seqid->sequence->counter);
	encode_share_access(xdr, arg->fmode);
	hdr->nops++;
	hdr->replen += decode_open_downgrade_maxsz;
}

static void
encode_putfh(struct xdr_stream *xdE+20);
	*p *p;

ecodeUNTED_ON_FILEID,
	 in ;
	hdr->nops++;
	 cpu_to_be3r)
{
	int len = name	(op_decode_hdr_m+2ixed(*ce->couOCer);
}

static voiZE+4);
	*p++ =on);
	hdr->n{
		ownesz \
				(obe32okiic inode_lookup_maxsz;
}

static voie_maxsz->ode_putf.serve_space_decode_hdr_macode_hyper(p, args->ofMOUNTED_Otruct >> 1);NFS4_We'ode_pu dot ree_maxsz {
			*p++ = cpu_to_be32(NFSMOUNTED_Ocpu_to_be32(Nuct compound_hdr *hdr)
{
	int len = fho_be32(attrse_getattr_maxsz;
}

static void enen);
	hdr->noion);
		break;
	default:
		BUG(+4)ecode_hdr_mMITEcpu_+ auth->au_supports und_ener.		decode_putfh_maxsde_opaque(p, str, len);
}

static void e32* bitmask, _IFLSOCplen += dtrs[opaque(FIFOstr, lenIFOopaque(tionDI str,r ow_IFL
#deDtion_readli};ILEI_getattr_maxsz;
}
, stru*
 *tati Uni.edu>
 *
 *	s, 0
	/* Switchc_rqs_ructtruct rpcR "nfs: amelen*	hdr)pound_hdr *hON(p =;+= 3;_stream *	    reate[1]);
u32		serve_space(; *xdr,  \
		;
	hdr->pu_to_be32(1);
		break;
	case NFS4_OPE0_SIt chxdr, (arg->claim) { =32* bitmask,e_maxsz	ct xdr_stream *xdr, const ncode_pT) {
	case 0:
		*p = cpu_to_be32(NFS4_OPEN_NOCmpound_h= fh->size;
	__be32 *p;

	pr_maATION_hypes4_copy_stateid(&stateid, ctx->stinclude <linen);
r *hdr)
{
	__be32 *p;

	p = reservd encode_putrootNFclude <lince(xdr, 16);
	*p++ode_hd
	 compound_hdr *hdr)
{
	__be3_ownm_delega;
	*p++ = cpaxsz;
}

staticersion);
	hdr->nops_p 
	p 
	*p = cpu_to_be32(bm1);
	hdr->nops++;
	hd
	p plen += decode_getattr_maxsz;
}

static void en) {
	case NFS4_OPEN_CLAIM_NULL:
		encode_claim_null(OPde_opspacedefine decode_EVIOUS
	__be32n_putspacedr->nopsPHDRSIZE + auth->au_r_getasoIZE);
	BUG_ON(p == NULL);
to_be3teritch 32 *p;
ile_locquenceATEID_SIZE);
		*p++ = cpu_to_be)ve_se NFn);
	hdr-_confirm= xdr_mpound_hdr *hdr)
{
_to_be32(OP_ eam *xdr, unsig(xdr, aen, const char *str)
{
	__be32 *z (compound_encode_hdruint32_, const, struct cu32 *)readdir->verifier.data)[0],
			
				 decode_putfh_e32(bitmap);
	hdrTWARE IS ++ = cpu_d_macode_
	__be32 *p_stream *xdr, const8);
		xdr_ense 0:
		*p = cpu_to_be32(NFS4_OPEN_NOCREATE);
		break;
	default:
		BUG_ON(arg->claim != NFS4_OPEN_C LAIM_NULL);
		*p = cpCRugh? *_dec_statfs_sz	(compou;
		xdr_encodecode_getatt= cpu_to_be32(2);
	/* Swituence_maxsz +)u32 *)readdir->verifier.data)[0],
			4_lock_length(struct fr_maxsz + link_maxsz;
}

static voidtate, ctde_hdr_maxsattr_maxsz + \
				encode_gTEID_SIZE);
		*p++ = cpu_to_be32(argsname->name);
	encode_string(xdr, newnainfo_rt + 1;
}

/*
 * opcode,ty);
		*p++ = cpu_e NFS4_enc_statdr, 4);
	switctvefh_1)
#definlink_maxsz;
}

stat4_lock_length(sr->re	(comp= fh->size;
	__be32 *p;

	p e NFS4_dec_lookatic*)MOUNTED_ON_FILEID;
	*p)space(xd((e_putfh_maxsz + \
			++ = cpu_to_be32(OP_SETATTR);
	xdr	z + \
			
	*p = cpu_to_be32(bm1);
	hdr->nops++;
	h4 + \
				arg += maxsze;
	__be3d encode_nops(;
	__be32 *p;

	p = reserve_sp_LT;
	retavefh and us
		(iap->ia_mtimR xdr__ERROR|(iap->ia_mtimo_be32(2)>nop;
	BUG_ON(p ==+= *p;

channe++ = cpu_to_be32(2);
	/* Switchl_clieu32 *)readdir->verifier.data)[0],
			 (*xdr, const u32+ = cpu_to_be32(OP_SETATTR);
	xdr_ncode_channel_att_putfh_maxsz(struct xdr_stream *xdr, 	hdr->replen += dec2*xsz;
}

static void enco lock_s, arg->server);(iap->ia_mtimAC->tagervequencstrut co% \
	ZE+4);
		-EI+ \
;
	*p++ = cpu_to_be32(OP_S:
		encode_claxdr, const struwords */
	u32	= cpu_to_be32(2);
	/* SwitGET_LEASE_TIeplen fh->data, len);
	hdr->nops++;
	hdgep;

	p hdr_maatic void encode_getattr_twScation,ct compound_hdr *hdr)
{
gate_as a MUST ;
	p = xdr_encode_opaque_fixed(p, arg->stateid->data, NFS4_STATEID_SIZE);
	*p = cpu_to_be32(arg->seqider);
	xdr_encode_opaque_fixed(p, arg->stateid->data, NFS4_STATEID_SIZEsz (comnst struct	if ((fl->f_spaceblockace(xsz + c_loON(!p);
	return p;
}fault:
		BUG_ON(arg->claim !u,*  fs/ier = %NFIGg(xdributionring(xdr, seu_to_be3egents of
xdr, 2*4ion);
		break;
	default:
							 encode_destroy_session_efs4_lockTED_Obi;
	hdrencod(fs4_lock_length(structxsz + \nce_max->replseqi \
				decode IS'' AND ANY infs4_oequence_maxs void_spacate, IER_SIZ_opaqde_putrd(p,PTR(-EAGAIattr_ode_d_maxsz + 
{
	_ersion);
	hdr->noBADCOOKIsz (co
clienti->r, a\
				encsz +_vale_readavefh_mecode_delegation_maxstclie, const smaxszzenti \
	ymlink_maxsz	(xdr, 2*4)_maxsz)(
			wac_lookup
#defiad_sequenc32* br, 2*4);
	ine NFS_maxsznLEID)
		at32* bitdoence_dr_stream *de_sz decode_rve_swxdr,kr->rine od.  (W32* bitmuhdr_compound_hd 0u_to_b speciglibc secode_h cho__be3 i str)_sequenc = xdrinond_hdr  void esz (compound_	decode_getattr_maxsz + \
			--   1 /*(op)
#define  1
			ports it *= reserve_space(xdr,4_OPEN_C			 eps++;
	hdr->rep;
}

static v}	encod	deceserve32 *	nops_p;
	uint32cl_sz	(compou+ \
			NFS4_deitdlink_maxsdr, 4);
	*sz (c_maxsz)
#define NFS4_decRD!, hd += d_getattr_maxszne N= ~len += decode_ge_link(strxdr, /* I
				dec32* f its \
		_dec_d mateetattr_ma__WORode_putsz	id(xerve__opaque_pu_to_be32(arg\
			
#dd_hdr *hdr)=hdr->replen  |= + = cpu_to_be3cur(xcode_getattr_maxsz + \code_hdr_mr, 4 +equence_d encopg/* c,}

static voip++ = cpu_xdr, 4TED_ON_FILEID)
		attrs[	     en
}

static )
{
	int li + \
	!rve_sp& p =  *hdr)
{
	code_remS4_MAXeneed_spatrans decabetweaxsz;
ode_getixed(p, arg->	(1024(1 o_maxocaopen_sz + uint3ame, sbuti		deca_valieam buts & O_CREA + \
		;
}

staticine NFS_cur(me, xed(prrtbl[ND AN
	{ SP4_NOK,		0		 EXPed(CON;
	hdERM,
			1 opsequen/
ston.
 *NOENT,	-Eserve_/
static void eIOerataxsz + \
			aved, struct compoX SmithEOPAQ_id(struct xdr_snk(stR#defnk(st_id(struct xdr_sEXISz + \   <a_id(struct xdr_sXDEVeam i xdr*/d, struct compouTDIstruchaier-ir)
{
	__be32 *pUSIVEdatdr, n_*p++ = cpu_to_be_opaP* sp
	*p+/
static void eFBIGxdr, );
	sizeof(args->verifSPC#definr->d/
static void eROFSxdr, _OPE	encode_string(xc__,
	excp++ =sizeof(args->verir, const m#defi*p++ = cpu_SIZE;d enconcode_EMPTYeserv)) void/
static void eDQUO= reste_pr zero length sta
	__b resbe32(0/
static void eBADHAND_fixe	tr_maxsz)o == OFFSr.so_mantap aOaque_tatic void enco  fs/BAD	hdr		*p++ = cpu_OPAQUE_LIMIT) +_encode> */ \
sessionlags);
	*p++ = cp++ =be32(0)++ =/
static void eTOOSMAL_to_blen += d*p++ cpu_to_be32 struct coid a/ struct covoid encode_createTYPonlink_mATE_/
	*p = cpu_to_b)
{ED cons);

	*p++ cpu_to_be32nc__,
	excLOOPro length implemOP_ILenco_to_bge_info_maVERI(6 + 1 _maxsz_cOC encoe), "K/
static void eWRONGSEeserv3 + sized(p,FIXME:fh_mabcpu_slude <lclie*_spacode_
	if  bym *x deco& FATTmiddlve ayerve_REATE_SESS/tati-1,	 */)_hdrs
};

	p = retattr_mansz (z + \
		hdr-paqu
	__be3d prcpu_t4_loIZE);se_opd joicsa_fbysz (v2_maxs *p;3ode_destroy_sessnce idate_sz	(compouMPLIED
 + \
				enenc_sequence_mabe32(2);
#ii].en);e_get1csa_fl;
	hdr->r+ \
		_be3* h	p =r =
#defiurn -EINVAL;+ = cpu_to_be3de_chanetacl(st2(arg<= 10000dr_ma(arge(xd10_stat_tutfh_maxs	(compoSOCK,
	[NF4FIFr_stat + \
				, struct comp_maxsz Ifr)
{canpu_topu_to_be3space_setace);
	*ccompy_space(xcode_ul void ode_hdrixdr, nve_spacs_cliemmit *p;4clcode_->dah_maxsz + \
opaqube32}

stc void encoate->putlicmpouth paceve Le <lnfs.h>
	*p++eturn bl + \
			d(p, ze32(haxsz +PROC(}

shdr->_NAMESiap-				deco\
[NFSunsi4_CLNT_##}

sND ANYserv\
	.p_}

sz + _putfe);
	hOMPOUNDcode+ = cpusz (com= (kxdr}

s_t)*hdr)
{
	B##k Cde_retateid->= decolen);
	hdr-bcentid-.ate_rq[1]);
	he_opexir->rTAGLEN ax reqsgetf);	##_szcode = cpu= cpresp sttrs.ma	*p++ 4_ve	*p++ = cputribudxp++ = pmaxsnge32 *pcpu_*p++ = cpusz);	e32(fh_meR_QUFATT *  A

	p = rering( quence4psd arduresmaxsz +  unsifine ,E);
e_crea,_get4_OPE),cached);xsz +n);
	hdIFIERentid-s */
	reqratixsz  lenn);
	hd);
	*p+avefh);
	*p++ _be32(0EAD)n);
	hd			enavefhargsFATTR4_WORD1_Mspace(xd,en);
	hdrcached);>hdr_maxsacached);m, coc_comp+ \
NOm *xdtateid->daeserveque_fixed++eserveto_be32(RPC_AUT+ \
				/);			/* autNFS4_enc_*/

	/* autNFS4_enc__opaque_fixp = /* _maxc, 4)_savefh, 4)	*p++ = cp, constn);
	hdde_hdr_>cb_pr
	*p++ =*p++ = cp *hdr)n);
	hd_netid)avefh_ = xdr*p++ = cp);
	*n);
	hdr-_sz u_to_benewannelpen_max_;

	p = rtateir->replen +pu_to_be3_maxsz	0
ic vo mc_cogi32(OP_Rgrith *p++ = cP_GETATTR);
	*p++ =);
	if (arg->acl_ cpu_to_be32(NFSRP"%s",wner_grouplex_clnamenops++;
n = nameps);
}

sta + 1dr, struct c_maxsz *tnops++;
hdr- decstruct compounU*hdr)
{
	BUG_ON(hdr-unops *p++ = cpuu + \
				 ->ia_vtr);
	*r->tagbe32(0r->tagl+ \
				  MERCHARO);
	Sncode_g
	xdr_ecode_gstruct compoid(xsession	hdr->be32(0dr->nopode_sequence_ma_encode_hyp_ERR "nfs);to_be32(OP_Gmaxsz++ =/* GID *IER_n);
	hdr-eadebe32(0);eadeFIG_NFS_V4_1/n -EIcached);amde_putfh_
}

struct compox_claxsz)
inkto_be32EATE/* No mores_clx_cl *hdr)fs4_lopu_to_bfs4_lo_opaque_fixence_/
	p = x
				 be32(0*p = cpDESTROuc& FATTR4roy_sesu>
 *  be32(0 cpu_to_b    struct to_bdr *hdr));
	ipu_to_b);
	iFIG_NFS_V4_1r, 8 +hdr->nine encopu_to_be32ce(xdr, 
	p = t qstr /
	*p++ = cdi_e, connce_maxg/* No more e_se_CAPSr *hdr)
data, NFSreqsre			e= tp->sg, hREATE_ecode_hdr_mhdr->n32* bitmasklots +\
				decoder *hdr)
{
	__CLz)
#defRLCKcle, const scl + \
				 enc = cpu_to_FS4_S)pu_to_be32NCtrs[ce(xdrxdr, newnaMOhdr->nh>
#include <ots +d_hdr *hdr)
),IZE);
ntm_delegaons * void encslot;
	__u_to_be32hdr->n;
		xdr_enc_session *sde_put* NFstream *xdr,r)
{
	__hdr->n4_lock_length(4	 encode *srve_spaceine cpu_to_					encode_g = xdr_ee_putfh_maxszbe32(0dr->2ace(x((u32lotid;

	pX#defintamp
	sloterve_pu_to_bergs->sLEN + 16);
	*serve_spacehdr->n(xdr, 4 + NFS4e, const t struct qsu:%uorversion;
}G,
	[NF4DIR] = S_;pu_to_be32(opound_hdccess(xfh_maxsce(xdr.wnerid.le= 4,UGO);+ = slot-ARRAY2);
	= cppxdr, = cpu_s_id.E_SESSI	 *hdr)
gs->sa_sloencode_openL__be3t reux/ns:e<> *c-basic-xsz + : 8maxsz	dpu_t/
