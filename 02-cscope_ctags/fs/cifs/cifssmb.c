/*
 *   fs/cifs/cifssmb.c
 *
 *   Copyright (C) International Business Machines  Corp., 2002,2009
 *   Author(s): Steve French (sfrench@us.ibm.com)
 *
 *   Contains the routines for constructing the SMB PDUs themselves
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published
 *   by the Free Software Foundation; either version 2.1 of the License, or
 *   (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this library; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

 /* SMB/CIFS PDU handling routines here - except for leftovers in connect.c   */
 /* These are mostly routines that operate on a pathname, or on a tree id     */
 /* (mounted volume), but there are eight handle based routines which must be */
 /* treated slightly differently for reconnection purposes since we never     */
 /* want to reuse a stale file handle and only the caller knows the file info */

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/vfs.h>
#include <linux/posix_acl_xattr.h>
#include <asm/uaccess.h>
#include "cifspdu.h"
#include "cifsglob.h"
#include "cifsacl.h"
#include "cifsproto.h"
#include "cifs_unicode.h"
#include "cifs_debug.h"

#ifdef CONFIG_CIFS_POSIX
static struct {
	int index;
	char *name;
} protocols[] = {
#ifdef CONFIG_CIFS_WEAK_PW_HASH
	{LANMAN_PROT, "\2LM1.2X002"},
	{LANMAN2_PROT, "\2LANMAN2.1"},
#endif /* weak password hashing for legacy clients */
	{CIFS_PROT, "\2NT LM 0.12"},
	{POSIX_PROT, "\2POSIX 2"},
	{BAD_PROT, "\2"}
};
#else
static struct {
	int index;
	char *name;
} protocols[] = {
#ifdef CONFIG_CIFS_WEAK_PW_HASH
	{LANMAN_PROT, "\2LM1.2X002"},
	{LANMAN2_PROT, "\2LANMAN2.1"},
#endif /* weak password hashing for legacy clients */
	{CIFS_PROT, "\2NT LM 0.12"},
	{BAD_PROT, "\2"}
};
#endif

/* define the number of elements in the cifs dialect array */
#ifdef CONFIG_CIFS_POSIX
#ifdef CONFIG_CIFS_WEAK_PW_HASH
#define CIFS_NUM_PROT 4
#else
#define CIFS_NUM_PROT 2
#endif /* CIFS_WEAK_PW_HASH */
#else /* not posix */
#ifdef CONFIG_CIFS_WEAK_PW_HASH
#define CIFS_NUM_PROT 3
#else
#define CIFS_NUM_PROT 1
#endif /* CONFIG_CIFS_WEAK_PW_HASH */
#endif /* CIFS_POSIX */

/* Mark as invalid, all open files on tree connections since they
   were closed when session to server was lost */
static void mark_open_files_invalid(struct cifsTconInfo *pTcon)
{
	struct cifsFileInfo *open_file = NULL;
	struct list_head *tmp;
	struct list_head *tmp1;

/* list all files open on tree connection and mark them invalid */
	write_lock(&GlobalSMBSeslock);
	list_for_each_safe(tmp, tmp1, &pTcon->openFileList) {
		open_file = list_entry(tmp, struct cifsFileInfo, tlist);
		open_file->invalidHandle = true;
		open_file->oplock_break_cancelled = true;
	}
	write_unlock(&GlobalSMBSeslock);
	/* BB Add call to invalidate_inodes(sb) for all superblocks mounted
	   to this tcon */
}

/* reconnect the socket, tcon, and smb session if needed */
static int
cifs_reconnect_tcon(struct cifsTconInfo *tcon, int smb_command)
{
	int rc = 0;
	struct cifsSesInfo *ses;
	struct TCP_Server_Info *server;
	struct nls_table *nls_codepage;

	/*
	 * SMBs NegProt, SessSetup, uLogoff do not have tcon yet so check for
	 * tcp and smb session status done differently for those three - in the
	 * calling routine
	 */
	if (!tcon)
		return 0;

	ses = tcon->ses;
	server = ses->server;

	/*
	 * only tree disconnect, open, and write, (and ulogoff which does not
	 * have tcon) are allowed as we start force umount
	 */
	if (tcon->tidStatus == CifsExiting) {
		if (smb_command != SMB_COM_WRITE_ANDX &&
		    smb_command != SMB_COM_OPEN_ANDX &&
		    smb_command != SMB_COM_TREE_DISCONNECT) {
			cFYI(1, ("can not send cmd %d while umounting",
				smb_command));
			return -ENODEV;
		}
	}

	if (ses->status == CifsExiting)
		return -EIO;

	/*
	 * Give demultiplex thread up to 10 seconds to reconnect, should be
	 * greater than cifs socket timeout which is 7 seconds
	 */
	while (server->tcpStatus == CifsNeedReconnect) {
		wait_event_interruptible_timeout(server->response_q,
			(server->tcpStatus == CifsGood), 10 * HZ);

		/* is TCP session is reestablished now ?*/
		if (server->tcpStatus != CifsNeedReconnect)
			break;

		/*
		 * on "soft" mounts we wait once. Hard mounts keep
		 * retrying until process is killed or server comes
		 * back on-line
		 */
		if (!tcon->retry || ses->status == CifsExiting) {
			cFYI(1, ("gave up waiting on reconnect in smb_init"));
			return -EHOSTDOWN;
		}
	}

	if (!ses->need_reconnect && !tcon->need_reconnect)
		return 0;

	nls_codepage = load_nls_default();

	/*
	 * need to prevent multiple threads trying to simultaneously
	 * reconnect the same SMB session
	 */
	down(&ses->sesSem);
	if (ses->need_reconnect)
		rc = cifs_setup_session(0, ses, nls_codepage);

	/* do we need to reconnect tcon? */
	if (rc || !tcon->need_reconnect) {
		up(&ses->sesSem);
		goto out;
	}

	mark_open_files_invalid(tcon);
	rc = CIFSTCon(0, ses, tcon->treeName, tcon, nls_codepage);
	up(&ses->sesSem);
	cFYI(1, ("reconnect tcon rc = %d", rc));

	if (rc)
		goto out;

	/*
	 * FIXME: check if wsize needs updated due to negotiated smb buffer
	 * 	  size shrinking
	 */
	atomic_inc(&tconInfoReconnectCount);

	/* tell server Unix caps we support */
	if (ses->capabilities & CAP_UNIX)
		reset_cifs_unix_caps(0, tcon, NULL, NULL);

	/*
	 * Removed call to reopen open files here. It is safer (and faster) to
	 * reopen files one at a time as needed in read and write.
	 *
	 * FIXME: what about file locks? don't we need to reclaim them ASAP?
	 */

out:
	/*
	 * Check if handle based operation so we know whether we can continue
	 * or not without returning to caller to reset file handle
	 */
	switch (smb_command) {
	case SMB_COM_READ_ANDX:
	case SMB_COM_WRITE_ANDX:
	case SMB_COM_CLOSE:
	case SMB_COM_FIND_CLOSE2:
	case SMB_COM_LOCKING_ANDX:
		rc = -EAGAIN;
	}

	unload_nls(nls_codepage);
	return rc;
}

/* Allocate and return pointer to an SMB request buffer, and set basic
   SMB information in the SMB header.  If the return code is zero, this
   function must have filled in request_buf pointer */
static int
small_smb_init(int smb_command, int wct, struct cifsTconInfo *tcon,
		void **request_buf)
{
	int rc = 0;

	rc = cifs_reconnect_tcon(tcon, smb_command);
	if (rc)
		return rc;

	*request_buf = cifs_small_buf_get();
	if (*request_buf == NULL) {
		/* BB should we add a retry in here if not a writepage? */
		return -ENOMEM;
	}

	header_assemble((struct smb_hdr *) *request_buf, smb_command,
			tcon, wct);

	if (tcon != NULL)
		cifs_stats_inc(&tcon->num_smbs_sent);

	return rc;
}

int
small_smb_init_no_tc(const int smb_command, const int wct,
		     struct cifsSesInfo *ses, void **request_buf)
{
	int rc;
	struct smb_hdr *buffer;

	rc = small_smb_init(smb_command, wct, NULL, request_buf);
	if (rc)
		return rc;

	buffer = (struct smb_hdr *)*request_buf;
	buffer->Mid = GetNextMid(ses->server);
	if (ses->capabilities & CAP_UNICODE)
		buffer->Flags2 |= SMBFLG2_UNICODE;
	if (ses->capabilities & CAP_STATUS32)
		buffer->Flags2 |= SMBFLG2_ERR_STATUS;

	/* uid, tid can stay at zero as set in header assemble */

	/* BB add support for turning on the signing when
	this function is used after 1st of session setup requests */

	return rc;
}

/* If the return code is zero, this function must fill in request_buf pointer */
static int
smb_init(int smb_command, int wct, struct cifsTconInfo *tcon,
	 void **request_buf /* returned */ ,
	 void **response_buf /* returned */ )
{
	int rc = 0;

	rc = cifs_reconnect_tcon(tcon, smb_command);
	if (rc)
		return rc;

	*request_buf = cifs_buf_get();
	if (*request_buf == NULL) {
		/* BB should we add a retry in here if not a writepage? */
		return -ENOMEM;
	}
    /* Although the original thought was we needed the response buf for  */
    /* potential retries of smb operations it turns out we can determine */
    /* from the mid flags when the request buffer can be resent without  */
    /* having to use a second distinct buffer for the response */
	if (response_buf)
		*response_buf = *request_buf;

	header_assemble((struct smb_hdr *) *request_buf, smb_command, tcon,
			wct);

	if (tcon != NULL)
		cifs_stats_inc(&tcon->num_smbs_sent);

	return rc;
}

static int validate_t2(struct smb_t2_rsp *pSMB)
{
	int rc = -EINVAL;
	int total_size;
	char *pBCC;

	/* check for plausible wct, bcc and t2 data and parm sizes */
	/* check for parm and data offset going beyond end of smb */
	if (pSMB->hdr.WordCount >= 10) {
		if ((le16_to_cpu(pSMB->t2_rsp.ParameterOffset) <= 1024) &&
		   (le16_to_cpu(pSMB->t2_rsp.DataOffset) <= 1024)) {
			/* check that bcc is at least as big as parms + data */
			/* check that bcc is less than negotiated smb buffer */
			total_size = le16_to_cpu(pSMB->t2_rsp.ParameterCount);
			if (total_size < 512) {
				total_size +=
					le16_to_cpu(pSMB->t2_rsp.DataCount);
				/* BCC le converted in SendReceive */
				pBCC = (pSMB->hdr.WordCount * 2) +
					sizeof(struct smb_hdr) +
					(char *)pSMB;
				if ((total_size <= (*(u16 *)pBCC)) &&
				   (total_size <
					CIFSMaxBufSize+MAX_CIFS_HDR_SIZE)) {
					return 0;
				}
			}
		}
	}
	cifs_dump_mem("Invalid transact2 SMB: ", (char *)pSMB,
		sizeof(struct smb_t2_rsp) + 16);
	return rc;
}
int
CIFSSMBNegotiate(unsigned int xid, struct cifsSesInfo *ses)
{
	NEGOTIATE_REQ *pSMB;
	NEGOTIATE_RSP *pSMBr;
	int rc = 0;
	int bytes_returned;
	int i;
	struct TCP_Server_Info *server;
	u16 count;
	unsigned int secFlags;
	u16 dialect;

	if (ses->server)
		server = ses->server;
	else {
		rc = -EIO;
		return rc;
	}
	rc = smb_init(SMB_COM_NEGOTIATE, 0, NULL /* no tcon yet */ ,
		      (void **) &pSMB, (void **) &pSMBr);
	if (rc)
		return rc;

	/* if any of auth flags (ie not sign or seal) are overriden use them */
	if (ses->overrideSecFlg & (~(CIFSSEC_MUST_SIGN | CIFSSEC_MUST_SEAL)))
		secFlags = ses->overrideSecFlg;  /* BB FIXME fix sign flags? */
	else /* if override flags set only sign/seal OR them with global auth */
		secFlags = extended_security | ses->overrideSecFlg;

	cFYI(1, ("secFlags 0x%x", secFlags));

	pSMB->hdr.Mid = GetNextMid(server);
	pSMB->hdr.Flags2 |= (SMBFLG2_UNICODE | SMBFLG2_ERR_STATUS);

	if ((secFlags & CIFSSEC_MUST_KRB5) == CIFSSEC_MUST_KRB5)
		pSMB->hdr.Flags2 |= SMBFLG2_EXT_SEC;
	else if ((secFlags & CIFSSEC_AUTH_MASK) == CIFSSEC_MAY_KRB5) {
		cFYI(1, ("Kerberos only mechanism, enable extended security"));
		pSMB->hdr.Flags2 |= SMBFLG2_EXT_SEC;
	}
#ifdef CONFIG_CIFS_EXPERIMENTAL
	else if ((secFlags & CIFSSEC_MUST_NTLMSSP) == CIFSSEC_MUST_NTLMSSP)
		pSMB->hdr.Flags2 |= SMBFLG2_EXT_SEC;
	else if ((secFlags & CIFSSEC_AUTH_MASK) == CIFSSEC_MAY_NTLMSSP) {
		cFYI(1, ("NTLMSSP only mechanism, enable extended security"));
		pSMB->hdr.Flags2 |= SMBFLG2_EXT_SEC;
	}
#endif

	count = 0;
	for (i = 0; i < CIFS_NUM_PROT; i++) {
		strncpy(pSMB->DialectsArray+count, protocols[i].name, 16);
		count += strlen(protocols[i].name) + 1;
		/* null at end of source and target buffers anyway */
	}
	pSMB->hdr.smb_buf_length += count;
	pSMB->ByteCount = cpu_to_le16(count);

	rc = SendReceive(xid, ses, (struct smb_hdr *) pSMB,
			 (struct smb_hdr *) pSMBr, &bytes_returned, 0);
	if (rc != 0)
		goto neg_err_exit;

	dialect = le16_to_cpu(pSMBr->DialectIndex);
	cFYI(1, ("Dialect: %d", dialect));
	/* Check wct = 1 error case */
	if ((pSMBr->hdr.WordCount < 13) || (dialect == BAD_PROT)) {
		/* core returns wct = 1, but we do not ask for core - otherwise
		small wct just comes when dialect index is -1 indicating we
		could not negotiate a common dialect */
		rc = -EOPNOTSUPP;
		goto neg_err_exit;
#ifdef CONFIG_CIFS_WEAK_PW_HASH
	} else if ((pSMBr->hdr.WordCount == 13)
			&& ((dialect == LANMAN_PROT)
				|| (dialect == LANMAN2_PROT))) {
		__s16 tmp;
		struct lanman_neg_rsp *rsp = (struct lanman_neg_rsp *)pSMBr;

		if ((secFlags & CIFSSEC_MAY_LANMAN) ||
			(secFlags & CIFSSEC_MAY_PLNTXT))
			server->secType = LANMAN;
		else {
			cERROR(1, ("mount failed weak security disabled"
				   " in /proc/fs/cifs/SecurityFlags"));
			rc = -EOPNOTSUPP;
			goto neg_err_exit;
		}
		server->secMode = (__u8)le16_to_cpu(rsp->SecurityMode);
		server->maxReq = le16_to_cpu(rsp->MaxMpxCount);
		server->maxBuf = min((__u32)le16_to_cpu(rsp->MaxBufSize),
				(__u32)CIFSMaxBufSize + MAX_CIFS_HDR_SIZE);
		server->max_vcs = le16_to_cpu(rsp->MaxNumberVcs);
		GETU32(server->sessid) = le32_to_cpu(rsp->SessionKey);
		/* even though we do not use raw we might as well set this
		accurately, in case we ever find a need for it */
		if ((le16_to_cpu(rsp->RawMode) & RAW_ENABLE) == RAW_ENABLE) {
			server->max_rw = 0xFF00;
			server->capabilities = CAP_MPX_MODE | CAP_RAW_MODE;
		} else {
			server->max_rw = 0;/* do not need to use raw anyway */
			server->capabilities = CAP_MPX_MODE;
		}
		tmp = (__s16)le16_to_cpu(rsp->ServerTimeZone);
		if (tmp == -1) {
			/* OS/2 often does not set timezone therefore
			 * we must use server time to calc time zone.
			 * Could deviate slightly from the right zone.
			 * Smallest defined timezone difference is 15 minutes
			 * (i.e. Nepal).  Rounding up/down is done to match
			 * this requirement.
			 */
			int val, seconds, remain, result;
			struct timespec ts, utc;
			utc = CURRENT_TIME;
			ts = cnvrtDosUnixTm(rsp->SrvTime.Date,
					    rsp->SrvTime.Time, 0);
			cFYI(1, ("SrvTime %d sec since 1970 (utc: %d) diff: %d",
				(int)ts.tv_sec, (int)utc.tv_sec,
				(int)(utc.tv_sec - ts.tv_sec)));
			val = (int)(utc.tv_sec - ts.tv_sec);
			seconds = abs(val);
			result = (seconds / MIN_TZ_ADJ) * MIN_TZ_ADJ;
			remain = seconds % MIN_TZ_ADJ;
			if (remain >= (MIN_TZ_ADJ / 2))
				result += MIN_TZ_ADJ;
			if (val < 0)
				result = -result;
			server->timeAdj = result;
		} else {
			server->timeAdj = (int)tmp;
			server->timeAdj *= 60; /* also in seconds */
		}
		cFYI(1, ("server->timeAdj: %d seconds", server->timeAdj));


		/* BB get server time for time conversions and add
		code to use it and timezone since this is not UTC */

		if (rsp->EncryptionKeyLength ==
				cpu_to_le16(CIFS_CRYPTO_KEY_SIZE)) {
			memcpy(server->cryptKey, rsp->EncryptionKey,
				CIFS_CRYPTO_KEY_SIZE);
		} else if (server->secMode & SECMODE_PW_ENCRYPT) {
			rc = -EIO; /* need cryptkey unless plain text */
			goto neg_err_exit;
		}

		cFYI(1, ("LANMAN negotiated"));
		/* we will not end up setting signing flags - as no signing
		was in LANMAN and server did not return the flags on */
		goto signing_check;
#else /* weak security disabled */
	} else if (pSMBr->hdr.WordCount == 13) {
		cERROR(1, ("mount failed, cifs module not built "
			  "with CIFS_WEAK_PW_HASH support"));
			rc = -EOPNOTSUPP;
#endif /* WEAK_PW_HASH */
		goto neg_err_exit;
	} else if (pSMBr->hdr.WordCount != 17) {
		/* unknown wct */
		rc = -EOPNOTSUPP;
		goto neg_err_exit;
	}
	/* else wct == 17 NTLM */
	server->secMode = pSMBr->SecurityMode;
	if ((server->secMode & SECMODE_USER) == 0)
		cFYI(1, ("share mode security"));

	if ((server->secMode & SECMODE_PW_ENCRYPT) == 0)
#ifdef CONFIG_CIFS_WEAK_PW_HASH
		if ((secFlags & CIFSSEC_MAY_PLNTXT) == 0)
#endif /* CIFS_WEAK_PW_HASH */
			cERROR(1, ("Server requests plain text password"
				  " but client support disabled"));

	if ((secFlags & CIFSSEC_MUST_NTLMV2) == CIFSSEC_MUST_NTLMV2)
		server->secType = NTLMv2;
	else if (secFlags & CIFSSEC_MAY_NTLM)
		server->secType = NTLM;
	else if (secFlags & CIFSSEC_MAY_NTLMV2)
		server->secType = NTLMv2;
	else if (secFlags & CIFSSEC_MAY_KRB5)
		server->secType = Kerberos;
	else if (secFlags & CIFSSEC_MAY_NTLMSSP)
		server->secType = RawNTLMSSP;
	else if (secFlags & CIFSSEC_MAY_LANMAN)
		server->secType = LANMAN;
/* #ifdef CONFIG_CIFS_EXPERIMENTAL
	else if (secFlags & CIFSSEC_MAY_PLNTXT)
		server->secType = ??
#endif */
	else {
		rc = -EOPNOTSUPP;
		cERROR(1, ("Invalid security type"));
		goto neg_err_exit;
	}
	/* else ... any others ...? */

	/* one byte, so no need to convert this or EncryptionKeyLen from
	   little endian */
	server->maxReq = le16_to_cpu(pSMBr->MaxMpxCount);
	/* probably no need to store and check maxvcs */
	server->maxBuf = min(le32_to_cpu(pSMBr->MaxBufferSize),
			(__u32) CIFSMaxBufSize + MAX_CIFS_HDR_SIZE);
	server->max_rw = le32_to_cpu(pSMBr->MaxRawSize);
	cFYI(DBG2, ("Max buf = %d", ses->server->maxBuf));
	GETU32(ses->server->sessid) = le32_to_cpu(pSMBr->SessionKey);
	server->capabilities = le32_to_cpu(pSMBr->Capabilities);
	server->timeAdj = (int)(__s16)le16_to_cpu(pSMBr->ServerTimeZone);
	server->timeAdj *= 60;
	if (pSMBr->EncryptionKeyLength == CIFS_CRYPTO_KEY_SIZE) {
		memcpy(server->cryptKey, pSMBr->u.EncryptionKey,
		       CIFS_CRYPTO_KEY_SIZE);
	} else if ((pSMBr->hdr.Flags2 & SMBFLG2_EXT_SEC)
			&& (pSMBr->EncryptionKeyLength == 0)) {
		/* decode security blob */
	} else if (server->secMode & SECMODE_PW_ENCRYPT) {
		rc = -EIO; /* no crypt key only if plain text pwd */
		goto neg_err_exit;
	}

	/* BB might be helpful to save off the domain of server here */

	if ((pSMBr->hdr.Flags2 & SMBFLG2_EXT_SEC) &&
		(server->capabilities & CAP_EXTENDED_SECURITY)) {
		count = pSMBr->ByteCount;
		if (count < 16) {
			rc = -EIO;
			goto neg_err_exit;
		}
		read_lock(&cifs_tcp_ses_lock);
		if (server->srv_count > 1) {
			read_unlock(&cifs_tcp_ses_lock);
			if (memcmp(server->server_GUID,
				   pSMBr->u.extended_response.
				   GUID, 16) != 0) {
				cFYI(1, ("server UID changed"));
				memcpy(server->server_GUID,
					pSMBr->u.extended_response.GUID,
					16);
			}
		} else {
			read_unlock(&cifs_tcp_ses_lock);
			memcpy(server->server_GUID,
			       pSMBr->u.extended_response.GUID, 16);
		}

		if (count == 16) {
			server->secType = RawNTLMSSP;
		} else {
			rc = decode_negTokenInit(pSMBr->u.extended_response.
						 SecurityBlob,
						 count - 16,
						 &server->secType);
			if (rc == 1)
				rc = 0;
			else
				rc = -EINVAL;
		}
	} else
		server->capabilities &= ~CAP_EXTENDED_SECURITY;

#ifdef CONFIG_CIFS_WEAK_PW_HASH
signing_check:
#endif
	if ((secFlags & CIFSSEC_MAY_SIGN) == 0) {
		/* MUST_SIGN already includes the MAY_SIGN FLAG
		   so if this is zero it means that signing is disabled */
		cFYI(1, ("Signing disabled"));
		if (server->secMode & SECMODE_SIGN_REQUIRED) {
			cERROR(1, ("Server requires "
				   "packet signing to be enabled in "
				   "/proc/fs/cifs/SecurityFlags."));
			rc = -EOPNOTSUPP;
		}
		server->secMode &=
			~(SECMODE_SIGN_ENABLED | SECMODE_SIGN_REQUIRED);
	} else if ((secFlags & CIFSSEC_MUST_SIGN) == CIFSSEC_MUST_SIGN) {
		/* signing required */
		cFYI(1, ("Must sign - secFlags 0x%x", secFlags));
		if ((server->secMode &
			(SECMODE_SIGN_ENABLED | SECMODE_SIGN_REQUIRED)) == 0) {
			cERROR(1,
				("signing required but server lacks support"));
			rc = -EOPNOTSUPP;
		} else
			server->secMode |= SECMODE_SIGN_REQUIRED;
	} else {
		/* signing optional ie CIFSSEC_MAY_SIGN */
		if ((server->secMode & SECMODE_SIGN_REQUIRED) == 0)
			server->secMode &=
				~(SECMODE_SIGN_ENABLED | SECMODE_SIGN_REQUIRED);
	}

neg_err_exit:
	cifs_buf_release(pSMB);

	cFYI(1, ("negprot rc %d", rc));
	return rc;
}

int
CIFSSMBTDis(const int xid, struct cifsTconInfo *tcon)
{
	struct smb_hdr *smb_buffer;
	int rc = 0;

	cFYI(1, ("In tree disconnect"));

	/* BB: do we need to check this? These should never be NULL. */
	if ((tcon->ses == NULL) || (tcon->ses->server == NULL))
		return -EIO;

	/*
	 * No need to return error on this operation if tid invalidated and
	 * closed on server already e.g. due to tcp session crashing. Also,
	 * the tcon is no longer on the list, so no need to take lock before
	 * checking this.
	 */
	if ((tcon->need_reconnect) || (tcon->ses->need_reconnect))
		return 0;

	rc = small_smb_init(SMB_COM_TREE_DISCONNECT, 0, tcon,
			    (void **)&smb_buffer);
	if (rc)
		return rc;

	rc = SendReceiveNoRsp(xid, tcon->ses, smb_buffer, 0);
	if (rc)
		cFYI(1, ("Tree disconnect failed %d", rc));

	/* No need to return error on this operation if tid invalidated and
	   closed on server already e.g. due to tcp session crashing */
	if (rc == -EAGAIN)
		rc = 0;

	return rc;
}

int
CIFSSMBLogoff(const int xid, struct cifsSesInfo *ses)
{
	LOGOFF_ANDX_REQ *pSMB;
	int rc = 0;

	cFYI(1, ("In SMBLogoff for session disconnect"));

	/*
	 * BB: do we need to check validity of ses and server? They should
	 * always be valid since we have an active reference. If not, that
	 * should probably be a BUG()
	 */
	if (!ses || !ses->server)
		return -EIO;

	down(&ses->sesSem);
	if (ses->need_reconnect)
		goto session_already_dead; /* no need to send SMBlogoff if uid
					      already closed due to reconnect */
	rc = small_smb_init(SMB_COM_LOGOFF_ANDX, 2, NULL, (void **)&pSMB);
	if (rc) {
		up(&ses->sesSem);
		return rc;
	}

	pSMB->hdr.Mid = GetNextMid(ses->server);

	if (ses->server->secMode &
		   (SECMODE_SIGN_REQUIRED | SECMODE_SIGN_ENABLED))
			pSMB->hdr.Flags2 |= SMBFLG2_SECURITY_SIGNATURE;

	pSMB->hdr.Uid = ses->Suid;

	pSMB->AndXCommand = 0xFF;
	rc = SendReceiveNoRsp(xid, ses, (struct smb_hdr *) pSMB, 0);
session_already_dead:
	up(&ses->sesSem);

	/* if session dead then we do not need to do ulogoff,
		since server closed smb session, no sense reporting
		error */
	if (rc == -EAGAIN)
		rc = 0;
	return rc;
}

int
CIFSPOSIXDelFile(const int xid, struct cifsTconInfo *tcon, const char *fileName,
		 __u16 type, const struct nls_table *nls_codepage, int remap)
{
	TRANSACTION2_SPI_REQ *pSMB = NULL;
	TRANSACTION2_SPI_RSP *pSMBr = NULL;
	struct unlink_psx_rq *pRqD;
	int name_len;
	int rc = 0;
	int bytes_returned = 0;
	__u16 params, param_offset, offset, byte_count;

	cFYI(1, ("In POSIX delete"));
PsxDelete:
	rc = smb_init(SMB_COM_TRANSACTION2, 15, tcon, (void **) &pSMB,
		      (void **) &pSMBr);
	if (rc)
		return rc;

	if (pSMB->hdr.Flags2 & SMBFLG2_UNICODE) {
		name_len =
		    cifsConvertToUCS((__le16 *) pSMB->FileName, fileName,
				     PATH_MAX, nls_codepage, remap);
		name_len++;	/* trailing null */
		name_len *= 2;
	} else { /* BB add path length overrun check */
		name_len = strnlen(fileName, PATH_MAX);
		name_len++;	/* trailing null */
		strncpy(pSMB->FileName, fileName, name_len);
	}

	params = 6 + name_len;
	pSMB->MaxParameterCount = cpu_to_le16(2);
	pSMB->MaxDataCount = 0; /* BB double check this with jra */
	pSMB->MaxSetupCount = 0;
	pSMB->Reserved = 0;
	pSMB->Flags = 0;
	pSMB->Timeout = 0;
	pSMB->Reserved2 = 0;
	param_offset = offsetof(struct smb_com_transaction2_spi_req,
				InformationLevel) - 4;
	offset = param_offset + params;

	/* Setup pointer to Request Data (inode type) */
	pRqD = (struct unlink_psx_rq *)(((char *)&pSMB->hdr.Protocol) + offset);
	pRqD->type = cpu_to_le16(type);
	pSMB->ParameterOffset = cpu_to_le16(param_offset);
	pSMB->DataOffset = cpu_to_le16(offset);
	pSMB->SetupCount = 1;
	pSMB->Reserved3 = 0;
	pSMB->SubCommand = cpu_to_le16(TRANS2_SET_PATH_INFORMATION);
	byte_count = 3 /* pad */  + params + sizeof(struct unlink_psx_rq);

	pSMB->DataCount = cpu_to_le16(sizeof(struct unlink_psx_rq));
	pSMB->TotalDataCount = cpu_to_le16(sizeof(struct unlink_psx_rq));
	pSMB->ParameterCount = cpu_to_le16(params);
	pSMB->TotalParameterCount = pSMB->ParameterCount;
	pSMB->InformationLevel = cpu_to_le16(SMB_POSIX_UNLINK);
	pSMB->Reserved4 = 0;
	pSMB->hdr.smb_buf_length += byte_count;
	pSMB->ByteCount = cpu_to_le16(byte_count);
	rc = SendReceive(xid, tcon->ses, (struct smb_hdr *) pSMB,
			 (struct smb_hdr *) pSMBr, &bytes_returned, 0);
	if (rc)
		cFYI(1, ("Posix delete returned %d", rc));
	cifs_buf_release(pSMB);

	cifs_stats_inc(&tcon->num_deletes);

	if (rc == -EAGAIN)
		goto PsxDelete;

	return rc;
}

int
CIFSSMBDelFile(const int xid, struct cifsTconInfo *tcon, const char *fileName,
	       const struct nls_table *nls_codepage, int remap)
{
	DELETE_FILE_REQ *pSMB = NULL;
	DELETE_FILE_RSP *pSMBr = NULL;
	int rc = 0;
	int bytes_returned;
	int name_len;

DelFileRetry:
	rc = smb_init(SMB_COM_DELETE, 1, tcon, (void **) &pSMB,
		      (void **) &pSMBr);
	if (rc)
		return rc;

	if (pSMB->hdr.Flags2 & SMBFLG2_UNICODE) {
		name_len =
		    cifsConvertToUCS((__le16 *) pSMB->fileName, fileName,
				     PATH_MAX, nls_codepage, remap);
		name_len++;	/* trailing null */
		name_len *= 2;
	} else {		/* BB improve check for buffer overruns BB */
		name_len = strnlen(fileName, PATH_MAX);
		name_len++;	/* trailing null */
		strncpy(pSMB->fileName, fileName, name_len);
	}
	pSMB->SearchAttributes =
	    cpu_to_le16(ATTR_READONLY | ATTR_HIDDEN | ATTR_SYSTEM);
	pSMB->BufferFormat = 0x04;
	pSMB->hdr.smb_buf_length += name_len + 1;
	pSMB->ByteCount = cpu_to_le16(name_len + 1);
	rc = SendReceive(xid, tcon->ses, (struct smb_hdr *) pSMB,
			 (struct smb_hdr *) pSMBr, &bytes_returned, 0);
	cifs_stats_inc(&tcon->num_deletes);
	if (rc)
		cFYI(1, ("Error in RMFile = %d", rc));

	cifs_buf_release(pSMB);
	if (rc == -EAGAIN)
		goto DelFileRetry;

	return rc;
}

int
CIFSSMBRmDir(const int xid, struct cifsTconInfo *tcon, const char *dirName,
	     const struct nls_table *nls_codepage, int remap)
{
	DELETE_DIRECTORY_REQ *pSMB = NULL;
	DELETE_DIRECTORY_RSP *pSMBr = NULL;
	int rc = 0;
	int bytes_returned;
	int name_len;

	cFYI(1, ("In CIFSSMBRmDir"));
RmDirRetry:
	rc = smb_init(SMB_COM_DELETE_DIRECTORY, 0, tcon, (void **) &pSMB,
		      (void **) &pSMBr);
	if (rc)
		return rc;

	if (pSMB->hdr.Flags2 & SMBFLG2_UNICODE) {
		name_len = cifsConvertToUCS((__le16 *) pSMB->DirName, dirName,
					 PATH_MAX, nls_codepage, remap);
		name_len++;	/* trailing null */
		name_len *= 2;
	} else {		/* BB improve check for buffer overruns BB */
		name_len = strnlen(dirName, PATH_MAX);
		name_len++;	/* trailing null */
		strncpy(pSMB->DirName, dirName, name_len);
	}

	pSMB->BufferFormat = 0x04;
	pSMB->hdr.smb_buf_length += name_len + 1;
	pSMB->ByteCount = cpu_to_le16(name_len + 1);
	rc = SendReceive(xid, tcon->ses, (struct smb_hdr *) pSMB,
			 (struct smb_hdr *) pSMBr, &bytes_returned, 0);
	cifs_stats_inc(&tcon->num_rmdirs);
	if (rc)
		cFYI(1, ("Error in RMDir = %d", rc));

	cifs_buf_release(pSMB);
	if (rc == -EAGAIN)
		goto RmDirRetry;
	return rc;
}

int
CIFSSMBMkDir(const int xid, struct cifsTconInfo *tcon,
	     const char *name, const struct nls_table *nls_codepage, int remap)
{
	int rc = 0;
	CREATE_DIRECTORY_REQ *pSMB = NULL;
	CREATE_DIRECTORY_RSP *pSMBr = NULL;
	int bytes_returned;
	int name_len;

	cFYI(1, ("In CIFSSMBMkDir"));
MkDirRetry:
	rc = smb_init(SMB_COM_CREATE_DIRECTORY, 0, tcon, (void **) &pSMB,
		      (void **) &pSMBr);
	if (rc)
		return rc;

	if (pSMB->hdr.Flags2 & SMBFLG2_UNICODE) {
		name_len = cifsConvertToUCS((__le16 *) pSMB->DirName, name,
					    PATH_MAX, nls_codepage, remap);
		name_len++;	/* trailing null */
		name_len *= 2;
	} else {		/* BB improve check for buffer overruns BB */
		name_len = strnlen(name, PATH_MAX);
		name_len++;	/* trailing null */
		strncpy(pSMB->DirName, name, name_len);
	}

	pSMB->BufferFormat = 0x04;
	pSMB->hdr.smb_buf_length += name_len + 1;
	pSMB->ByteCount = cpu_to_le16(name_len + 1);
	rc = SendReceive(xid, tcon->ses, (struct smb_hdr *) pSMB,
			 (struct smb_hdr *) pSMBr, &bytes_returned, 0);
	cifs_stats_inc(&tcon->num_mkdirs);
	if (rc)
		cFYI(1, ("Error in Mkdir = %d", rc));

	cifs_buf_release(pSMB);
	if (rc == -EAGAIN)
		goto MkDirRetry;
	return rc;
}

int
CIFSPOSIXCreate(const int xid, struct cifsTconInfo *tcon, __u32 posix_flags,
		__u64 mode, __u16 *netfid, FILE_UNIX_BASIC_INFO *pRetData,
		__u32 *pOplock, const char *name,
		const struct nls_table *nls_codepage, int remap)
{
	TRANSACTION2_SPI_REQ *pSMB = NULL;
	TRANSACTION2_SPI_RSP *pSMBr = NULL;
	int name_len;
	int rc = 0;
	int bytes_returned = 0;
	__u16 params, param_offset, offset, byte_count, count;
	OPEN_PSX_REQ *pdata;
	OPEN_PSX_RSP *psx_rsp;

	cFYI(1, ("In POSIX Create"));
PsxCreat:
	rc = smb_init(SMB_COM_TRANSACTION2, 15, tcon, (void **) &pSMB,
		      (void **) &pSMBr);
	if (rc)
		return rc;

	if (pSMB->hdr.Flags2 & SMBFLG2_UNICODE) {
		name_len =
		    cifsConvertToUCS((__le16 *) pSMB->FileName, name,
				     PATH_MAX, nls_codepage, remap);
		name_len++;	/* trailing null */
		name_len *= 2;
	} else {	/* BB improve the check for buffer overruns BB */
		name_len = strnlen(name, PATH_MAX);
		name_len++;	/* trailing null */
		strncpy(pSMB->FileName, name, name_len);
	}

	params = 6 + name_len;
	count = sizeof(OPEN_PSX_REQ);
	pSMB->MaxParameterCount = cpu_to_le16(2);
	pSMB->MaxDataCount = cpu_to_le16(1000);	/* large enough */
	pSMB->MaxSetupCount = 0;
	pSMB->Reserved = 0;
	pSMB->Flags = 0;
	pSMB->Timeout = 0;
	pSMB->Reserved2 = 0;
	param_offset = offsetof(struct smb_com_transaction2_spi_req,
				InformationLevel) - 4;
	offset = param_offset + params;
	pdata = (OPEN_PSX_REQ *)(((char *)&pSMB->hdr.Protocol) + offset);
	pdata->Level = cpu_to_le16(SMB_QUERY_FILE_UNIX_BASIC);
	pdata->Permissions = cpu_to_le64(mode);
	pdata->PosixOpenFlags = cpu_to_le32(posix_flags);
	pdata->OpenFlags =  cpu_to_le32(*pOplock);
	pSMB->ParameterOffset = cpu_to_le16(param_offset);
	pSMB->DataOffset = cpu_to_le16(offset);
	pSMB->SetupCount = 1;
	pSMB->Reserved3 = 0;
	pSMB->SubCommand = cpu_to_le16(TRANS2_SET_PATH_INFORMATION);
	byte_count = 3 /* pad */  + params + count;

	pSMB->DataCount = cpu_to_le16(count);
	pSMB->ParameterCount = cpu_to_le16(params);
	pSMB->TotalDataCount = pSMB->DataCount;
	pSMB->TotalParameterCount = pSMB->ParameterCount;
	pSMB->InformationLevel = cpu_to_le16(SMB_POSIX_OPEN);
	pSMB->Reserved4 = 0;
	pSMB->hdr.smb_buf_length += byte_count;
	pSMB->ByteCount = cpu_to_le16(byte_count);
	rc = SendReceive(xid, tcon->ses, (struct smb_hdr *) pSMB,
			 (struct smb_hdr *) pSMBr, &bytes_returned, 0);
	if (rc) {
		cFYI(1, ("Posix create returned %d", rc));
		goto psx_create_err;
	}

	cFYI(1, ("copying inode info"));
	rc = validate_t2((struct smb_t2_rsp *)pSMBr);

	if (rc || (pSMBr->ByteCount < sizeof(OPEN_PSX_RSP))) {
		rc = -EIO;	/* bad smb */
		goto psx_create_err;
	}

	/* copy return information to pRetData */
	psx_rsp = (OPEN_PSX_RSP *)((char *) &pSMBr->hdr.Protocol
			+ le16_to_cpu(pSMBr->t2.DataOffset));

	*pOplock = le16_to_cpu(psx_rsp->OplockFlags);
	if (netfid)
		*netfid = psx_rsp->Fid;   /* cifs fid stays in le */
	/* Let caller know file was created so we can set the mode. */
	/* Do we care about the CreateAction in any other cases? */
	if (cpu_to_le32(FILE_CREATE) == psx_rsp->CreateAction)
		*pOplock |= CIFS_CREATE_ACTION;
	/* check to make sure response data is there */
	if (psx_rsp->ReturnedLevel != cpu_to_le16(SMB_QUERY_FILE_UNIX_BASIC)) {
		pRetData->Type = cpu_to_le32(-1); /* unknown */
		cFYI(DBG2, ("unknown type"));
	} else {
		if (pSMBr->ByteCount < sizeof(OPEN_PSX_RSP)
					+ sizeof(FILE_UNIX_BASIC_INFO)) {
			cERROR(1, ("Open response data too small"));
			pRetData->Type = cpu_to_le32(-1);
			goto psx_create_err;
		}
		memcpy((char *) pRetData,
			(char *)psx_rsp + sizeof(OPEN_PSX_RSP),
			sizeof(FILE_UNIX_BASIC_INFO));
	}

psx_create_err:
	cifs_buf_release(pSMB);

	if (posix_flags & SMB_O_DIRECTORY)
		cifs_stats_inc(&tcon->num_posixmkdirs);
	else
		cifs_stats_inc(&tcon->num_posixopens);

	if (rc == -EAGAIN)
		goto PsxCreat;

	return rc;
}

static __u16 convert_disposition(int disposition)
{
	__u16 ofun = 0;

	switch (disposition) {
		case FILE_SUPERSEDE:
			ofun = SMBOPEN_OCREATE | SMBOPEN_OTRUNC;
			break;
		case FILE_OPEN:
			ofun = SMBOPEN_OAPPEND;
			break;
		case FILE_CREATE:
			ofun = SMBOPEN_OCREATE;
			break;
		case FILE_OPEN_IF:
			ofun = SMBOPEN_OCREATE | SMBOPEN_OAPPEND;
			break;
		case FILE_OVERWRITE:
			ofun = SMBOPEN_OTRUNC;
			break;
		case FILE_OVERWRITE_IF:
			ofun = SMBOPEN_OCREATE | SMBOPEN_OTRUNC;
			break;
		default:
			cFYI(1, ("unknown disposition %d", disposition));
			ofun =  SMBOPEN_OAPPEND; /* regular open */
	}
	return ofun;
}

static int
access_flags_to_smbopen_mode(const int access_flags)
{
	int masked_flags = access_flags & (GENERIC_READ | GENERIC_WRITE);

	if (masked_flags == GENERIC_READ)
		return SMBOPEN_READ;
	else if (masked_flags == GENERIC_WRITE)
		return SMBOPEN_WRITE;

	/* just go for read/write */
	return SMBOPEN_READWRITE;
}

int
SMBLegacyOpen(const int xid, struct cifsTconInfo *tcon,
	    const char *fileName, const int openDisposition,
	    const int access_flags, const int create_options, __u16 *netfid,
	    int *pOplock, FILE_ALL_INFO *pfile_info,
	    const struct nls_table *nls_codepage, int remap)
{
	int rc = -EACCES;
	OPENX_REQ *pSMB = NULL;
	OPENX_RSP *pSMBr = NULL;
	int bytes_returned;
	int name_len;
	__u16 count;

OldOpenRetry:
	rc = smb_init(SMB_COM_OPEN_ANDX, 15, tcon, (void **) &pSMB,
		      (void **) &pSMBr);
	if (rc)
		return rc;

	pSMB->AndXCommand = 0xFF;       /* none */

	if (pSMB->hdr.Flags2 & SMBFLG2_UNICODE) {
		count = 1;      /* account for one byte pad to word boundary */
		name_len =
		   cifsConvertToUCS((__le16 *) (pSMB->fileName + 1),
				    fileName, PATH_MAX, nls_codepage, remap);
		name_len++;     /* trailing null */
		name_len *= 2;
	} else {                /* BB improve check for buffer overruns BB */
		count = 0;      /* no pad */
		name_len = strnlen(fileName, PATH_MAX);
		name_len++;     /* trailing null */
		strncpy(pSMB->fileName, fileName, name_len);
	}
	if (*pOplock & REQ_OPLOCK)
		pSMB->OpenFlags = cpu_to_le16(REQ_OPLOCK);
	else if (*pOplock & REQ_BATCHOPLOCK)
		pSMB->OpenFlags = cpu_to_le16(REQ_BATCHOPLOCK);

	pSMB->OpenFlags |= cpu_to_le16(REQ_MORE_INFO);
	pSMB->Mode = cpu_to_le16(access_flags_to_smbopen_mode(access_flags));
	pSMB->Mode |= cpu_to_le16(0x40); /* deny none */
	/* set file as system file if special file such
	   as fifo and server expecting SFU style and
	   no Unix extensions */

	if (create_options & CREATE_OPTION_SPECIAL)
		pSMB->FileAttributes = cpu_to_le16(ATTR_SYSTEM);
	else /* BB FIXME BB */
		pSMB->FileAttributes = cpu_to_le16(0/*ATTR_NORMAL*/);

	if (create_options & CREATE_OPTION_READONLY)
		pSMB->FileAttributes |= cpu_to_le16(ATTR_READONLY);

	/* BB FIXME BB */
/*	pSMB->CreateOptions = cpu_to_le32(create_options &
						 CREATE_OPTIONS_MASK); */
	/* BB FIXME END BB */

	pSMB->Sattr = cpu_to_le16(ATTR_HIDDEN | ATTR_SYSTEM | ATTR_DIRECTORY);
	pSMB->OpenFunction = cpu_to_le16(convert_disposition(openDisposition));
	count += name_len;
	pSMB->hdr.smb_buf_length += count;

	pSMB->ByteCount = cpu_to_le16(count);
	/* long_op set to 1 to allow for oplock break timeouts */
	rc = SendReceive(xid, tcon->ses, (struct smb_hdr *) pSMB,
			(struct smb_hdr *)pSMBr, &bytes_returned, CIFS_LONG_OP);
	cifs_stats_inc(&tcon->num_opens);
	if (rc) {
		cFYI(1, ("Error in Open = %d", rc));
	} else {
	/* BB verify if wct == 15 */

/*		*pOplock = pSMBr->OplockLevel; */ /* BB take from action field*/

		*netfid = pSMBr->Fid;   /* cifs fid stays in le */
		/* Let caller know file was created so we can set the mode. */
		/* Do we care about the CreateAction in any other cases? */
	/* BB FIXME BB */
/*		if (cpu_to_le32(FILE_CREATE) == pSMBr->CreateAction)
			*pOplock |= CIFS_CREATE_ACTION; */
	/* BB FIXME END */

		if (pfile_info) {
			pfile_info->CreationTime = 0; /* BB convert CreateTime*/
			pfile_info->LastAccessTime = 0; /* BB fixme */
			pfile_info->LastWriteTime = 0; /* BB fixme */
			pfile_info->ChangeTime = 0;  /* BB fixme */
			pfile_info->Attributes =
				cpu_to_le32(le16_to_cpu(pSMBr->FileAttributes));
			/* the file_info buf is endian converted by caller */
			pfile_info->AllocationSize =
				cpu_to_le64(le32_to_cpu(pSMBr->EndOfFile));
			pfile_info->EndOfFile = pfile_info->AllocationSize;
			pfile_info->NumberOfLinks = cpu_to_le32(1);
			pfile_info->DeletePending = 0;
		}
	}

	cifs_buf_release(pSMB);
	if (rc == -EAGAIN)
		goto OldOpenRetry;
	return rc;
}

int
CIFSSMBOpen(const int xid, struct cifsTconInfo *tcon,
	    const char *fileName, const int openDisposition,
	    const int access_flags, const int create_options, __u16 *netfid,
	    int *pOplock, FILE_ALL_INFO *pfile_info,
	    const struct nls_table *nls_codepage, int remap)
{
	int rc = -EACCES;
	OPEN_REQ *pSMB = NULL;
	OPEN_RSP *pSMBr = NULL;
	int bytes_returned;
	int name_len;
	__u16 count;

openRetry:
	rc = smb_init(SMB_COM_NT_CREATE_ANDX, 24, tcon, (void **) &pSMB,
		      (void **) &pSMBr);
	if (rc)
		return rc;

	pSMB->AndXCommand = 0xFF;	/* none */

	if (pSMB->hdr.Flags2 & SMBFLG2_UNICODE) {
		count = 1;	/* account for one byte pad to word boundary */
		name_len =
		    cifsConvertToUCS((__le16 *) (pSMB->fileName + 1),
				     fileName, PATH_MAX, nls_codepage, remap);
		name_len++;	/* trailing null */
		name_len *= 2;
		pSMB->NameLength = cpu_to_le16(name_len);
	} else {		/* BB improve check for buffer overruns BB */
		count = 0;	/* no pad */
		name_len = strnlen(fileName, PATH_MAX);
		name_len++;	/* trailing null */
		pSMB->NameLength = cpu_to_le16(name_len);
		strncpy(pSMB->fileName, fileName, name_len);
	}
	if (*pOplock & REQ_OPLOCK)
		pSMB->OpenFlags = cpu_to_le32(REQ_OPLOCK);
	else if (*pOplock & REQ_BATCHOPLOCK)
		pSMB->OpenFlags = cpu_to_le32(REQ_BATCHOPLOCK);
	pSMB->DesiredAccess = cpu_to_le32(access_flags);
	pSMB->AllocationSize = 0;
	/* set file as system file if special file such
	   as fifo and server expecting SFU style and
	   no Unix extensions */
	if (create_options & CREATE_OPTION_SPECIAL)
		pSMB->FileAttributes = cpu_to_le32(ATTR_SYSTEM);
	else
		pSMB->FileAttributes = cpu_to_le32(ATTR_NORMAL);

	/* XP does not handle ATTR_POSIX_SEMANTICS */
	/* but it helps speed up case sensitive checks for other
	servers such as Samba */
	if (tcon->ses->capabilities & CAP_UNIX)
		pSMB->FileAttributes |= cpu_to_le32(ATTR_POSIX_SEMANTICS);

	if (create_options & CREATE_OPTION_READONLY)
		pSMB->FileAttributes |= cpu_to_le32(ATTR_READONLY);

	pSMB->ShareAccess = cpu_to_le32(FILE_SHARE_ALL);
	pSMB->CreateDisposition = cpu_to_le32(openDisposition);
	pSMB->CreateOptions = cpu_to_le32(create_options & CREATE_OPTIONS_MASK);
	/* BB Expirement with various impersonation levels and verify */
	pSMB->ImpersonationLevel = cpu_to_le32(SECURITY_IMPERSONATION);
	pSMB->SecurityFlags =
	    SECURITY_CONTEXT_TRACKING | SECURITY_EFFECTIVE_ONLY;

	count += name_len;
	pSMB->hdr.smb_buf_length += count;

	pSMB->ByteCount = cpu_to_le16(count);
	/* long_op set to 1 to allow for oplock break timeouts */
	rc = SendReceive(xid, tcon->ses, (struct smb_hdr *) pSMB,
			(struct smb_hdr *)pSMBr, &bytes_returned, CIFS_LONG_OP);
	cifs_stats_inc(&tcon->num_opens);
	if (rc) {
		cFYI(1, ("Error in Open = %d", rc));
	} else {
		*pOplock = pSMBr->OplockLevel; /* 1 byte no need to le_to_cpu */
		*netfid = pSMBr->Fid;	/* cifs fid stays in le */
		/* Let caller know file was created so we can set the mode. */
		/* Do we care about the CreateAction in any other cases? */
		if (cpu_to_le32(FILE_CREATE) == pSMBr->CreateAction)
			*pOplock |= CIFS_CREATE_ACTION;
		if (pfile_info) {
			memcpy((char *)pfile_info, (char *)&pSMBr->CreationTime,
				36 /* CreationTime to Attributes */);
			/* the file_info buf is endian converted by caller */
			pfile_info->AllocationSize = pSMBr->AllocationSize;
			pfile_info->EndOfFile = pSMBr->EndOfFile;
			pfile_info->NumberOfLinks = cpu_to_le32(1);
			pfile_info->DeletePending = 0;
		}
	}

	cifs_buf_release(pSMB);
	if (rc == -EAGAIN)
		goto openRetry;
	return rc;
}

int
CIFSSMBRead(const int xid, struct cifsTconInfo *tcon, const int netfid,
	    const unsigned int count, const __u64 lseek, unsigned int *nbytes,
	    char **buf, int *pbuf_type)
{
	int rc = -EACCES;
	READ_REQ *pSMB = NULL;
	READ_RSP *pSMBr = NULL;
	char *pReadData = NULL;
	int wct;
	int resp_buf_type = 0;
	struct kvec iov[1];

	cFYI(1, ("Reading %d bytes on fid %d", count, netfid));
	if (tcon->ses->capabilities & CAP_LARGE_FILES)
		wct = 12;
	else {
		wct = 10; /* old style read */
		if ((lseek >> 32) > 0)  {
			/* can not handle this big offset for old */
			return -EIO;
		}
	}

	*nbytes = 0;
	rc = small_smb_init(SMB_COM_READ_ANDX, wct, tcon, (void **) &pSMB);
	if (rc)
		return rc;

	/* tcon and ses pointer are checked in smb_init */
	if (tcon->ses->server == NULL)
		return -ECONNABORTED;

	pSMB->AndXCommand = 0xFF;       /* none */
	pSMB->Fid = netfid;
	pSMB->OffsetLow = cpu_to_le32(lseek & 0xFFFFFFFF);
	if (wct == 12)
		pSMB->OffsetHigh = cpu_to_le32(lseek >> 32);

	pSMB->Remaining = 0;
	pSMB->MaxCount = cpu_to_le16(count & 0xFFFF);
	pSMB->MaxCountHigh = cpu_to_le32(count >> 16);
	if (wct == 12)
		pSMB->ByteCount = 0;  /* no need to do le conversion since 0 */
	else {
		/* old style read */
		struct smb_com_readx_req *pSMBW =
			(struct smb_com_readx_req *)pSMB;
		pSMBW->ByteCount = 0;
	}

	iov[0].iov_base = (char *)pSMB;
	iov[0].iov_len = pSMB->hdr.smb_buf_length + 4;
	rc = SendReceive2(xid, tcon->ses, iov, 1 /* num iovecs */,
			 &resp_buf_type, CIFS_STD_OP | CIFS_LOG_ERROR);
	cifs_stats_inc(&tcon->num_reads);
	pSMBr = (READ_RSP *)iov[0].iov_base;
	if (rc) {
		cERROR(1, ("Send error in read = %d", rc));
	} else {
		int data_length = le16_to_cpu(pSMBr->DataLengthHigh);
		data_length = data_length << 16;
		data_length += le16_to_cpu(pSMBr->DataLength);
		*nbytes = data_length;

		/*check that DataLength would not go beyond end of SMB */
		if ((data_length > CIFSMaxBufSize)
				|| (data_length > count)) {
			cFYI(1, ("bad length %d for count %d",
				 data_length, count));
			rc = -EIO;
			*nbytes = 0;
		} else {
			pReadData = (char *) (&pSMBr->hdr.Protocol) +
					le16_to_cpu(pSMBr->DataOffset);
/*			if (rc = copy_to_user(buf, pReadData, data_length)) {
				cERROR(1,("Faulting on read rc = %d",rc));
				rc = -EFAULT;
			}*/ /* can not use copy_to_user when using page cache*/
			if (*buf)
				memcpy(*buf, pReadData, data_length);
		}
	}

/*	cifs_small_buf_release(pSMB); */ /* Freed earlier now in SendReceive2 */
	if (*buf) {
		if (resp_buf_type == CIFS_SMALL_BUFFER)
			cifs_small_buf_release(iov[0].iov_base);
		else if (resp_buf_type == CIFS_LARGE_BUFFER)
			cifs_buf_release(iov[0].iov_base);
	} else if (resp_buf_type != CIFS_NO_BUFFER) {
		/* return buffer to caller to free */
		*buf = iov[0].iov_base;
		if (resp_buf_type == CIFS_SMALL_BUFFER)
			*pbuf_type = CIFS_SMALL_BUFFER;
		else if (resp_buf_type == CIFS_LARGE_BUFFER)
			*pbuf_type = CIFS_LARGE_BUFFER;
	} /* else no valid buffer on return - leave as null */

	/* Note: On -EAGAIN error only caller can retry on handle based calls
		since file handle passed in no longer valid */
	return rc;
}


int
CIFSSMBWrite(const int xid, struct cifsTconInfo *tcon,
	     const int netfid, const unsigned int count,
	     const __u64 offset, unsigned int *nbytes, const char *buf,
	     const char __user *ubuf, const int long_op)
{
	int rc = -EACCES;
	WRITE_REQ *pSMB = NULL;
	WRITE_RSP *pSMBr = NULL;
	int bytes_returned, wct;
	__u32 bytes_sent;
	__u16 byte_count;

	/* cFYI(1, ("write at %lld %d bytes", offset, count));*/
	if (tcon->ses == NULL)
		return -ECONNABORTED;

	if (tcon->ses->capabilities & CAP_LARGE_FILES)
		wct = 14;
	else {
		wct = 12;
		if ((offset >> 32) > 0) {
			/* can not handle big offset for old srv */
			return -EIO;
		}
	}

	rc = smb_init(SMB_COM_WRITE_ANDX, wct, tcon, (void **) &pSMB,
		      (void **) &pSMBr);
	if (rc)
		return rc;
	/* tcon and ses pointer are checked in smb_init */
	if (tcon->ses->server == NULL)
		return -ECONNABORTED;

	pSMB->AndXCommand = 0xFF;	/* none */
	pSMB->Fid = netfid;
	pSMB->OffsetLow = cpu_to_le32(offset & 0xFFFFFFFF);
	if (wct == 14)
		pSMB->OffsetHigh = cpu_to_le32(offset >> 32);

	pSMB->Reserved = 0xFFFFFFFF;
	pSMB->WriteMode = 0;
	pSMB->Remaining = 0;

	/* Can increase buffer size if buffer is big enough in some cases ie we
	can send more if LARGE_WRITE_X capability returned by the server and if
	our buffer is big enough or if we convert to iovecs on socket writes
	and eliminate the copy to the CIFS buffer */
	if (tcon->ses->capabilities & CAP_LARGE_WRITE_X) {
		bytes_sent = min_t(const unsigned int, CIFSMaxBufSize, count);
	} else {
		bytes_sent = (tcon->ses->server->maxBuf - MAX_CIFS_HDR_SIZE)
			 & ~0xFF;
	}

	if (bytes_sent > count)
		bytes_sent = count;
	pSMB->DataOffset =
		cpu_to_le16(offsetof(struct smb_com_write_req, Data) - 4);
	if (buf)
		memcpy(pSMB->Data, buf, bytes_sent);
	else if (ubuf) {
		if (copy_from_user(pSMB->Data, ubuf, bytes_sent)) {
			cifs_buf_release(pSMB);
			return -EFAULT;
		}
	} else if (count != 0) {
		/* No buffer */
		cifs_buf_release(pSMB);
		return -EINVAL;
	} /* else setting file size with write of zero bytes */
	if (wct == 14)
		byte_count = bytes_sent + 1; /* pad */
	else /* wct == 12 */
		byte_count = bytes_sent + 5; /* bigger pad, smaller smb hdr */

	pSMB->DataLengthLow = cpu_to_le16(bytes_sent & 0xFFFF);
	pSMB->DataLengthHigh = cpu_to_le16(bytes_sent >> 16);
	pSMB->hdr.smb_buf_length += byte_count;

	if (wct == 14)
		pSMB->ByteCount = cpu_to_le16(byte_count);
	else { /* old style write has byte count 4 bytes earlier
		  so 4 bytes pad  */
		struct smb_com_writex_req *pSMBW =
			(struct smb_com_writex_req *)pSMB;
		pSMBW->ByteCount = cpu_to_le16(byte_count);
	}

	rc = SendReceive(xid, tcon->ses, (struct smb_hdr *) pSMB,
			 (struct smb_hdr *) pSMBr, &bytes_returned, long_op);
	cifs_stats_inc(&tcon->num_writes);
	if (rc) {
		cFYI(1, ("Send error in write = %d", rc));
		*nbytes = 0;
	} else {
		*nbytes = le16_to_cpu(pSMBr->CountHigh);
		*nbytes = (*nbytes) << 16;
		*nbytes += le16_to_cpu(pSMBr->Count);
	}

	cifs_buf_release(pSMB);

	/* Note: On -EAGAIN error only caller can retry on handle based calls
		since file handle passed in no longer valid */

	return rc;
}

int
CIFSSMBWrite2(const int xid, struct cifsTconInfo *tcon,
	     const int netfid, const unsigned int count,
	     const __u64 offset, unsigned int *nbytes, struct kvec *iov,
	     int n_vec, const int long_op)
{
	int rc = -EACCES;
	WRITE_REQ *pSMB = NULL;
	int wct;
	int smb_hdr_len;
	int resp_buf_type = 0;

	*nbytes = 0;

	cFYI(1, ("write2 at %lld %d bytes", (long long)offset, count));

	if (tcon->ses->capabilities & CAP_LARGE_FILES) {
		wct = 14;
	} else {
		wct = 12;
		if ((offset >> 32) > 0) {
			/* can not handle big offset for old srv */
			return -EIO;
		}
	}
	rc = small_smb_init(SMB_COM_WRITE_ANDX, wct, tcon, (void **) &pSMB);
	if (rc)
		return rc;
	/* tcon and ses pointer are checked in smb_init */
	if (tcon->ses->server == NULL)
		return -ECONNABORTED;

	pSMB->AndXCommand = 0xFF;	/* none */
	pSMB->Fid = netfid;
	pSMB->OffsetLow = cpu_to_le32(offset & 0xFFFFFFFF);
	if (wct == 14)
		pSMB->OffsetHigh = cpu_to_le32(offset >> 32);
	pSMB->Reserved = 0xFFFFFFFF;
	pSMB->WriteMode = 0;
	pSMB->Remaining = 0;

	pSMB->DataOffset =
	    cpu_to_le16(offsetof(struct smb_com_write_req, Data) - 4);

	pSMB->DataLengthLow = cpu_to_le16(count & 0xFFFF);
	pSMB->DataLengthHigh = cpu_to_le16(count >> 16);
	smb_hdr_len = pSMB->hdr.smb_buf_length + 1; /* hdr + 1 byte pad */
	if (wct == 14)
		pSMB->hdr.smb_buf_length += count+1;
	else /* wct == 12 */
		pSMB->hdr.smb_buf_length += count+5; /* smb data starts later */
	if (wct == 14)
		pSMB->ByteCount = cpu_to_le16(count + 1);
	else /* wct == 12 */ /* bigger pad, smaller smb hdr, keep offset ok */ {
		struct smb_com_writex_req *pSMBW =
				(struct smb_com_writex_req *)pSMB;
		pSMBW->ByteCount = cpu_to_le16(count + 5);
	}
	iov[0].iov_base = pSMB;
	if (wct == 14)
		iov[0].iov_len = smb_hdr_len + 4;
	else /* wct == 12 pad bigger by four bytes */
		iov[0].iov_len = smb_hdr_len + 8;


	rc = SendReceive2(xid, tcon->ses, iov, n_vec + 1, &resp_buf_type,
			  long_op);
	cifs_stats_inc(&tcon->num_writes);
	if (rc) {
		cFYI(1, ("Send error Write2 = %d", rc));
	} else if (resp_buf_type == 0) {
		/* presumably this can not happen, but best to be safe */
		rc = -EIO;
	} else {
		WRITE_RSP *pSMBr = (WRITE_RSP *)iov[0].iov_base;
		*nbytes = le16_to_cpu(pSMBr->CountHigh);
		*nbytes = (*nbytes) << 16;
		*nbytes += le16_to_cpu(pSMBr->Count);
	}

/*	cifs_small_buf_release(pSMB); */ /* Freed earlier now in SendReceive2 */
	if (resp_buf_type == CIFS_SMALL_BUFFER)
		cifs_small_buf_release(iov[0].iov_base);
	else if (resp_buf_type == CIFS_LARGE_BUFFER)
		cifs_buf_release(iov[0].iov_base);

	/* Note: On -EAGAIN error only caller can retry on handle based calls
		since file handle passed in no longer valid */

	return rc;
}


int
CIFSSMBLock(const int xid, struct cifsTconInfo *tcon,
	    const __u16 smb_file_id, const __u64 len,
	    const __u64 offset, const __u32 numUnlock,
	    const __u32 numLock, const __u8 lockType, const bool waitFlag)
{
	int rc = 0;
	LOCK_REQ *pSMB = NULL;
/*	LOCK_RSP *pSMBr = NULL; */ /* No response data other than rc to parse */
	int bytes_returned;
	int timeout = 0;
	__u16 count;

	cFYI(1, ("CIFSSMBLock timeout %d numLock %d", (int)waitFlag, numLock));
	rc = small_smb_init(SMB_COM_LOCKING_ANDX, 8, tcon, (void **) &pSMB);

	if (rc)
		return rc;

	if (lockType == LOCKING_ANDX_OPLOCK_RELEASE) {
		timeout = CIFS_ASYNC_OP; /* no response expected */
		pSMB->Timeout = 0;
	} else if (waitFlag) {
		timeout = CIFS_BLOCKING_OP; /* blocking operation, no timeout */
		pSMB->Timeout = cpu_to_le32(-1);/* blocking - do not time out */
	} else {
		pSMB->Timeout = 0;
	}

	pSMB->NumberOfLocks = cpu_to_le16(numLock);
	pSMB->NumberOfUnlocks = cpu_to_le16(numUnlock);
	pSMB->LockType = lockType;
	pSMB->AndXCommand = 0xFF;	/* none */
	pSMB->Fid = smb_file_id; /* netfid stays le */

	if ((numLock != 0) || (numUnlock != 0)) {
		pSMB->Locks[0].Pid = cpu_to_le16(current->tgid);
		/* BB where to store pid high? */
		pSMB->Locks[0].LengthLow = cpu_to_le32((u32)len);
		pSMB->Locks[0].LengthHigh = cpu_to_le32((u32)(len>>32));
		pSMB->Locks[0].OffsetLow = cpu_to_le32((u32)offset);
		pSMB->Locks[0].OffsetHigh = cpu_to_le32((u32)(offset>>32));
		count = sizeof(LOCKING_ANDX_RANGE);
	} else {
		/* oplock break */
		count = 0;
	}
	pSMB->hdr.smb_buf_length += count;
	pSMB->ByteCount = cpu_to_le16(count);

	if (waitFlag) {
		rc = SendReceiveBlockingLock(xid, tcon, (struct smb_hdr *) pSMB,
			(struct smb_hdr *) pSMB, &bytes_returned);
		cifs_small_buf_release(pSMB);
	} else {
		rc = SendReceiveNoRsp(xid, tcon->ses, (struct smb_hdr *)pSMB,
				      timeout);
		/* SMB buffer freed by function above */
	}
	cifs_stats_inc(&tcon->num_locks);
	if (rc)
		cFYI(1, ("Send error in Lock = %d", rc));

	/* Note: On -EAGAIN error only caller can retry on handle based calls
	since file handle passed in no longer valid */
	return rc;
}

int
CIFSSMBPosixLock(const int xid, struct cifsTconInfo *tcon,
		const __u16 smb_file_id, const int get_flag, const __u64 len,
		struct file_lock *pLockData, const __u16 lock_type,
		const bool waitFlag)
{
	struct smb_com_transaction2_sfi_req *pSMB  = NULL;
	struct smb_com_transaction2_sfi_rsp *pSMBr = NULL;
	struct cifs_posix_lock *parm_data;
	int rc = 0;
	int timeout = 0;
	int bytes_returned = 0;
	int resp_buf_type = 0;
	__u16 params, param_offset, offset, byte_count, count;
	struct kvec iov[1];

	cFYI(1, ("Posix Lock"));

	if (pLockData == NULL)
		return -EINVAL;

	rc = small_smb_init(SMB_COM_TRANSACTION2, 15, tcon, (void **) &pSMB);

	if (rc)
		return rc;

	pSMBr = (struct smb_com_transaction2_sfi_rsp *)pSMB;

	params = 6;
	pSMB->MaxSetupCount = 0;
	pSMB->Reserved = 0;
	pSMB->Flags = 0;
	pSMB->Reserved2 = 0;
	param_offset = offsetof(struct smb_com_transaction2_sfi_req, Fid) - 4;
	offset = param_offset + params;

	count = sizeof(struct cifs_posix_lock);
	pSMB->MaxParameterCount = cpu_to_le16(2);
	pSMB->MaxDataCount = cpu_to_le16(1000); /* BB find max SMB from sess */
	pSMB->SetupCount = 1;
	pSMB->Reserved3 = 0;
	if (get_flag)
		pSMB->SubCommand = cpu_to_le16(TRANS2_QUERY_FILE_INFORMATION);
	else
		pSMB->SubCommand = cpu_to_le16(TRANS2_SET_FILE_INFORMATION);
	byte_count = 3 /* pad */  + params + count;
	pSMB->DataCount = cpu_to_le16(count);
	pSMB->ParameterCount = cpu_to_le16(params);
	pSMB->TotalDataCount = pSMB->DataCount;
	pSMB->TotalParameterCount = pSMB->ParameterCount;
	pSMB->ParameterOffset = cpu_to_le16(param_offset);
	parm_data = (struct cifs_posix_lock *)
			(((char *) &pSMB->hdr.Protocol) + offset);

	parm_data->lock_type = cpu_to_le16(lock_type);
	if (waitFlag) {
		timeout = CIFS_BLOCKING_OP; /* blocking operation, no timeout */
		parm_data->lock_flags = cpu_to_le16(1);
		pSMB->Timeout = cpu_to_le32(-1);
	} else
		pSMB->Timeout = 0;

	parm_data->pid = cpu_to_le32(current->tgid);
	parm_data->start = cpu_to_le64(pLockData->fl_start);
	parm_data->length = cpu_to_le64(len);  /* normalize negative numbers */

	pSMB->DataOffset = cpu_to_le16(offset);
	pSMB->Fid = smb_file_id;
	pSMB->InformationLevel = cpu_to_le16(SMB_SET_POSIX_LOCK);
	pSMB->Reserved4 = 0;
	pSMB->hdr.smb_buf_length += byte_count;
	pSMB->ByteCount = cpu_to_le16(byte_count);
	if (waitFlag) {
		rc = SendReceiveBlockingLock(xid, tcon, (struct smb_hdr *) pSMB,
			(struct smb_hdr *) pSMBr, &bytes_returned);
	} else {
		iov[0].iov_base = (char *)pSMB;
		iov[0].iov_len = pSMB->hdr.smb_buf_length + 4;
		rc = SendReceive2(xid, tcon->ses, iov, 1 /* num iovecs */,
				&resp_buf_type, timeout);
		pSMB = NULL; /* request buf already freed by SendReceive2. Do
				not try to free it twice below on exit */
		pSMBr = (struct smb_com_transaction2_sfi_rsp *)iov[0].iov_base;
	}

	if (rc) {
		cFYI(1, ("Send error in Posix Lock = %d", rc));
	} else if (get_flag) {
		/* lock structure can be returned on get */
		__u16 data_offset;
		__u16 data_count;
		rc = validate_t2((struct smb_t2_rsp *)pSMBr);

		if (rc || (pSMBr->ByteCount < sizeof(struct cifs_posix_lock))) {
			rc = -EIO;      /* bad smb */
			goto plk_err_exit;
		}
		data_offset = le16_to_cpu(pSMBr->t2.DataOffset);
		data_count  = le16_to_cpu(pSMBr->t2.DataCount);
		if (data_count < sizeof(struct cifs_posix_lock)) {
			rc = -EIO;
			goto plk_err_exit;
		}
		parm_data = (struct cifs_posix_lock *)
			((char *)&pSMBr->hdr.Protocol + data_offset);
		if (parm_data->lock_type == cpu_to_le16(CIFS_UNLCK))
			pLockData->fl_type = F_UNLCK;
	}

plk_err_exit:
	if (pSMB)
		cifs_small_buf_release(pSMB);

	if (resp_buf_type == CIFS_SMALL_BUFFER)
		cifs_small_buf_release(iov[0].iov_base);
	else if (resp_buf_type == CIFS_LARGE_BUFFER)
		cifs_buf_release(iov[0].iov_base);

	/* Note: On -EAGAIN error only caller can retry on handle based calls
	   since file handle passed in no longer valid */

	return rc;
}


int
CIFSSMBClose(const int xid, struct cifsTconInfo *tcon, int smb_file_id)
{
	int rc = 0;
	CLOSE_REQ *pSMB = NULL;
	cFYI(1, ("In CIFSSMBClose"));

/* do not retry on dead session on close */
	rc = small_smb_init(SMB_COM_CLOSE, 3, tcon, (void **) &pSMB);
	if (rc == -EAGAIN)
		return 0;
	if (rc)
		return rc;

	pSMB->FileID = (__u16) smb_file_id;
	pSMB->LastWriteTime = 0xFFFFFFFF;
	pSMB->ByteCount = 0;
	rc = SendReceiveNoRsp(xid, tcon->ses, (struct smb_hdr *) pSMB, 0);
	cifs_stats_inc(&tcon->num_closes);
	if (rc) {
		if (rc != -EINTR) {
			/* EINTR is expected when user ctl-c to kill app */
			cERROR(1, ("Send error in Close = %d", rc));
		}
	}

	/* Since session is dead, file will be closed on server already */
	if (rc == -EAGAIN)
		rc = 0;

	return rc;
}

int
CIFSSMBFlush(const int xid, struct cifsTconInfo *tcon, int smb_file_id)
{
	int rc = 0;
	FLUSH_REQ *pSMB = NULL;
	cFYI(1, ("In CIFSSMBFlush"));

	rc = small_smb_init(SMB_COM_FLUSH, 1, tcon, (void **) &pSMB);
	if (rc)
		return rc;

	pSMB->FileID = (__u16) smb_file_id;
	pSMB->ByteCount = 0;
	rc = SendReceiveNoRsp(xid, tcon->ses, (struct smb_hdr *) pSMB, 0);
	cifs_stats_inc(&tcon->num_flushes);
	if (rc)
		cERROR(1, ("Send error in Flush = %d", rc));

	return rc;
}

int
CIFSSMBRename(const int xid, struct cifsTconInfo *tcon,
	      const char *fromName, const char *toName,
	      const struct nls_table *nls_codepage, int remap)
{
	int rc = 0;
	RENAME_REQ *pSMB = NULL;
	RENAME_RSP *pSMBr = NULL;
	int bytes_returned;
	int name_len, name_len2;
	__u16 count;

	cFYI(1, ("In CIFSSMBRename"));
renameRetry:
	rc = smb_init(SMB_COM_RENAME, 1, tcon, (void **) &pSMB,
		      (void **) &pSMBr);
	if (rc)
		return rc;

	pSMB->BufferFormat = 0x04;
	pSMB->SearchAttributes =
	    cpu_to_le16(ATTR_READONLY | ATTR_HIDDEN | ATTR_SYSTEM |
			ATTR_DIRECTORY);

	if (pSMB->hdr.Flags2 & SMBFLG2_UNICODE) {
		name_len =
		    cifsConvertToUCS((__le16 *) pSMB->OldFileName, fromName,
				     PATH_MAX, nls_codepage, remap);
		name_len++;	/* trailing null */
		name_len *= 2;
		pSMB->OldFileName[name_len] = 0x04;	/* pad */
	/* protocol requires ASCII signature byte on Unicode string */
		pSMB->OldFileName[name_len + 1] = 0x00;
		name_len2 =
		    cifsConvertToUCS((__le16 *)&pSMB->OldFileName[name_len + 2],
				     toName, PATH_MAX, nls_codepage, remap);
		name_len2 += 1 /* trailing null */  + 1 /* Signature word */ ;
		name_len2 *= 2;	/* convert to bytes */
	} else {	/* BB improve the check for buffer overruns BB */
		name_len = strnlen(fromName, PATH_MAX);
		name_len++;	/* trailing null */
		strncpy(pSMB->OldFileName, fromName, name_len);
		name_len2 = strnlen(toName, PATH_MAX);
		name_len2++;	/* trailing null */
		pSMB->OldFileName[name_len] = 0x04;  /* 2nd buffer format */
		strncpy(&pSMB->OldFileName[name_len + 1], toName, name_len2);
		name_len2++;	/* trailing null */
		name_len2++;	/* signature byte */
	}

	count = 1 /* 1st signature byte */  + name_len + name_len2;
	pSMB->hdr.smb_buf_length += count;
	pSMB->ByteCount = cpu_to_le16(count);

	rc = SendReceive(xid, tcon->ses, (struct smb_hdr *) pSMB,
			 (struct smb_hdr *) pSMBr, &bytes_returned, 0);
	cifs_stats_inc(&tcon->num_renames);
	if (rc)
		cFYI(1, ("Send error in rename = %d", rc));

	cifs_buf_release(pSMB);

	if (rc == -EAGAIN)
		goto renameRetry;

	return rc;
}

int CIFSSMBRenameOpenFile(const int xid, struct cifsTconInfo *pTcon,
		int netfid, const char *target_name,
		const struct nls_table *nls_codepage, int remap)
{
	struct smb_com_transaction2_sfi_req *pSMB  = NULL;
	struct smb_com_transaction2_sfi_rsp *pSMBr = NULL;
	struct set_file_rename *rename_info;
	char *data_offset;
	char dummy_string[30];
	int rc = 0;
	int bytes_returned = 0;
	int len_of_str;
	__u16 params, param_offset, offset, count, byte_count;

	cFYI(1, ("Rename to File by handle"));
	rc = smb_init(SMB_COM_TRANSACTION2, 15, pTcon, (void **) &pSMB,
			(void **) &pSMBr);
	if (rc)
		return rc;

	params = 6;
	pSMB->MaxSetupCount = 0;
	pSMB->Reserved = 0;
	pSMB->Flags = 0;
	pSMB->Timeout = 0;
	pSMB->Reserved2 = 0;
	param_offset = offsetof(struct smb_com_transaction2_sfi_req, Fid) - 4;
	offset = param_offset + params;

	data_offset = (char *) (&pSMB->hdr.Protocol) + offset;
	rename_info = (struct set_file_rename *) data_offset;
	pSMB->MaxParameterCount = cpu_to_le16(2);
	pSMB->MaxDataCount = cpu_to_le16(1000); /* BB find max SMB from sess */
	pSMB->SetupCount = 1;
	pSMB->Reserved3 = 0;
	pSMB->SubCommand = cpu_to_le16(TRANS2_SET_FILE_INFORMATION);
	byte_count = 3 /* pad */  + params;
	pSMB->ParameterCount = cpu_to_le16(params);
	pSMB->TotalParameterCount = pSMB->ParameterCount;
	pSMB->ParameterOffset = cpu_to_le16(param_offset);
	pSMB->DataOffset = cpu_to_le16(offset);
	/* construct random name ".cifs_tmp<inodenum><mid>" */
	rename_info->overwrite = cpu_to_le32(1);
	rename_info->root_fid  = 0;
	/* unicode only call */
	if (target_name == NULL) {
		sprintf(dummy_string, "cifs%x", pSMB->hdr.Mid);
		len_of_str = cifsConvertToUCS((__le16 *)rename_info->target_name,
					dummy_string, 24, nls_codepage, remap);
	} else {
		len_of_str = cifsConvertToUCS((__le16 *)rename_info->target_name,
					target_name, PATH_MAX, nls_codepage,
					remap);
	}
	rename_info->target_name_len = cpu_to_le32(2 * len_of_str);
	count = 12 /* sizeof(struct set_file_rename) */ + (2 * len_of_str);
	byte_count += count;
	pSMB->DataCount = cpu_to_le16(count);
	pSMB->TotalDataCount = pSMB->DataCount;
	pSMB->Fid = netfid;
	pSMB->InformationLevel =
		cpu_to_le16(SMB_SET_FILE_RENAME_INFORMATION);
	pSMB->Reserved4 = 0;
	pSMB->hdr.smb_buf_length += byte_count;
	pSMB->ByteCount = cpu_to_le16(byte_count);
	rc = SendReceive(xid, pTcon->ses, (struct smb_hdr *) pSMB,
			 (struct smb_hdr *) pSMBr, &bytes_returned, 0);
	cifs_stats_inc(&pTcon->num_t2renames);
	if (rc)
		cFYI(1, ("Send error in Rename (by file handle) = %d", rc));

	cifs_buf_release(pSMB);

	/* Note: On -EAGAIN error only caller can retry on handle based calls
		since file handle passed in no longer valid */

	return rc;
}

int
CIFSSMBCopy(const int xid, struct cifsTconInfo *tcon, const char *fromName,
	    const __u16 target_tid, const char *toName, const int flags,
	    const struct nls_table *nls_codepage, int remap)
{
	int rc = 0;
	COPY_REQ *pSMB = NULL;
	COPY_RSP *pSMBr = NULL;
	int bytes_returned;
	int name_len, name_len2;
	__u16 count;

	cFYI(1, ("In CIFSSMBCopy"));
copyRetry:
	rc = smb_init(SMB_COM_COPY, 1, tcon, (void **) &pSMB,
			(void **) &pSMBr);
	if (rc)
		return rc;

	pSMB->BufferFormat = 0x04;
	pSMB->Tid2 = target_tid;

	pSMB->Flags = cpu_to_le16(flags & COPY_TREE);

	if (pSMB->hdr.Flags2 & SMBFLG2_UNICODE) {
		name_len = cifsConvertToUCS((__le16 *) pSMB->OldFileName,
					    fromName, PATH_MAX, nls_codepage,
					    remap);
		name_len++;     /* trailing null */
		name_len *= 2;
		pSMB->OldFileName[name_len] = 0x04;     /* pad */
		/* protocol requires ASCII signature byte on Unicode string */
		pSMB->OldFileName[name_len + 1] = 0x00;
		name_len2 =
		    cifsConvertToUCS((__le16 *)&pSMB->OldFileName[name_len + 2],
				toName, PATH_MAX, nls_codepage, remap);
		name_len2 += 1 /* trailing null */  + 1 /* Signature word */ ;
		name_len2 *= 2; /* convert to bytes */
	} else { 	/* BB improve the check for buffer overruns BB */
		name_len = strnlen(fromName, PATH_MAX);
		name_len++;     /* trailing null */
		strncpy(pSMB->OldFileName, fromName, name_len);
		name_len2 = strnlen(toName, PATH_MAX);
		name_len2++;    /* trailing null */
		pSMB->OldFileName[name_len] = 0x04;  /* 2nd buffer format */
		strncpy(&pSMB->OldFileName[name_len + 1], toName, name_len2);
		name_len2++;    /* trailing null */
		name_len2++;    /* signature byte */
	}

	count = 1 /* 1st signature byte */  + name_len + name_len2;
	pSMB->hdr.smb_buf_length += count;
	pSMB->ByteCount = cpu_to_le16(count);

	rc = SendReceive(xid, tcon->ses, (struct smb_hdr *) pSMB,
		(struct smb_hdr *) pSMBr, &bytes_returned, 0);
	if (rc) {
		cFYI(1, ("Send error in copy = %d with %d files copied",
			rc, le16_to_cpu(pSMBr->CopyCount)));
	}
	cifs_buf_release(pSMB);

	if (rc == -EAGAIN)
		goto copyRetry;

	return rc;
}

int
CIFSUnixCreateSymLink(const int xid, struct cifsTconInfo *tcon,
		      const char *fromName, const char *toName,
		      const struct nls_table *nls_codepage)
{
	TRANSACTION2_SPI_REQ *pSMB = NULL;
	TRANSACTION2_SPI_RSP *pSMBr = NULL;
	char *data_offset;
	int name_len;
	int name_len_target;
	int rc = 0;
	int bytes_returned = 0;
	__u16 params, param_offset, offset, byte_count;

	cFYI(1, ("In Symlink Unix style"));
createSymLinkRetry:
	rc = smb_init(SMB_COM_TRANSACTION2, 15, tcon, (void **) &pSMB,
		      (void **) &pSMBr);
	if (rc)
		return rc;

	if (pSMB->hdr.Flags2 & SMBFLG2_UNICODE) {
		name_len =
		    cifs_strtoUCS((__le16 *) pSMB->FileName, fromName, PATH_MAX
				  /* find define for this maxpathcomponent */
				  , nls_codepage);
		name_len++;	/* trailing null */
		name_len *= 2;

	} else {	/* BB improve the check for buffer overruns BB */
		name_len = strnlen(fromName, PATH_MAX);
		name_len++;	/* trailing null */
		strncpy(pSMB->FileName, fromName, name_len);
	}
	params = 6 + name_len;
	pSMB->MaxSetupCount = 0;
	pSMB->Reserved = 0;
	pSMB->Flags = 0;
	pSMB->Timeout = 0;
	pSMB->Reserved2 = 0;
	param_offset = offsetof(struct smb_com_transaction2_spi_req,
				InformationLevel) - 4;
	offset = param_offset + params;

	data_offset = (char *) (&pSMB->hdr.Protocol) + offset;
	if (pSMB->hdr.Flags2 & SMBFLG2_UNICODE) {
		name_len_target =
		    cifs_strtoUCS((__le16 *) data_offset, toName, PATH_MAX
				  /* find define for this maxpathcomponent */
				  , nls_codepage);
		name_len_target++;	/* trailing null */
		name_len_target *= 2;
	} else {	/* BB improve the check for buffer overruns BB */
		name_len_target = strnlen(toName, PATH_MAX);
		name_len_target++;	/* trailing null */
		strncpy(data_offset, toName, name_len_target);
	}

	pSMB->MaxParameterCount = cpu_to_le16(2);
	/* BB find exact max on data count below from sess */
	pSMB->MaxDataCount = cpu_to_le16(1000);
	pSMB->SetupCount = 1;
	pSMB->Reserved3 = 0;
	pSMB->SubCommand = cpu_to_le16(TRANS2_SET_PATH_INFORMATION);
	byte_count = 3 /* pad */  + params + name_len_target;
	pSMB->DataCount = cpu_to_le16(name_len_target);
	pSMB->ParameterCount = cpu_to_le16(params);
	pSMB->TotalDataCount = pSMB->DataCount;
	pSMB->TotalParameterCount = pSMB->ParameterCount;
	pSMB->ParameterOffset = cpu_to_le16(param_offset);
	pSMB->DataOffset = cpu_to_le16(offset);
	pSMB->InformationLevel = cpu_to_le16(SMB_SET_FILE_UNIX_LINK);
	pSMB->Reserved4 = 0;
	pSMB->hdr.smb_buf_length += byte_count;
	pSMB->ByteCount = cpu_to_le16(byte_count);
	rc = SendReceive(xid, tcon->ses, (struct smb_hdr *) pSMB,
			 (struct smb_hdr *) pSMBr, &bytes_returned, 0);
	cifs_stats_inc(&tcon->num_symlinks);
	if (rc)
		cFYI(1, ("Send error in SetPathInfo create symlink = %d", rc));

	cifs_buf_release(pSMB);

	if (rc == -EAGAIN)
		goto createSymLinkRetry;

	return rc;
}

int
CIFSUnixCreateHardLink(const int xid, struct cifsTconInfo *tcon,
		       const char *fromName, const char *toName,
		       const struct nls_table *nls_codepage, int remap)
{
	TRANSACTION2_SPI_REQ *pSMB = NULL;
	TRANSACTION2_SPI_RSP *pSMBr = NULL;
	char *data_offset;
	int name_len;
	int name_len_target;
	int rc = 0;
	int bytes_returned = 0;
	__u16 params, param_offset, offset, byte_count;

	cFYI(1, ("In Create Hard link Unix style"));
createHardLinkRetry:
	rc = smb_init(SMB_COM_TRANSACTION2, 15, tcon, (void **) &pSMB,
		      (void **) &pSMBr);
	if (rc)
		return rc;

	if (pSMB->hdr.Flags2 & SMBFLG2_UNICODE) {
		name_len = cifsConvertToUCS((__le16 *) pSMB->FileName, toName,
					    PATH_MAX, nls_codepage, remap);
		name_len++;	/* trailing null */
		name_len *= 2;

	} else {	/* BB improve the check for buffer overruns BB */
		name_len = strnlen(toName, PATH_MAX);
		name_len++;	/* trailing null */
		strncpy(pSMB->FileName, toName, name_len);
	}
	params = 6 + name_len;
	pSMB->MaxSetupCount = 0;
	pSMB->Reserved = 0;
	pSMB->Flags = 0;
	pSMB->Timeout = 0;
	pSMB->Reserved2 = 0;
	param_offset = offsetof(struct smb_com_transaction2_spi_req,
				InformationLevel) - 4;
	offset = param_offset + params;

	data_offset = (char *) (&pSMB->hdr.Protocol) + offset;
	if (pSMB->hdr.Flags2 & SMBFLG2_UNICODE) {
		name_len_target =
		    cifsConvertToUCS((__le16 *) data_offset, fromName, PATH_MAX,
				     nls_codepage, remap);
		name_len_target++;	/* trailing null */
		name_len_target *= 2;
	} else {	/* BB improve the check for buffer overruns BB */
		name_len_target = strnlen(fromName, PATH_MAX);
		name_len_target++;	/* trailing null */
		strncpy(data_offset, fromName, name_len_target);
	}

	pSMB->MaxParameterCount = cpu_to_le16(2);
	/* BB find exact max on data count below from sess*/
	pSMB->MaxDataCount = cpu_to_le16(1000);
	pSMB->SetupCount = 1;
	pSMB->Reserved3 = 0;
	pSMB->SubCommand = cpu_to_le16(TRANS2_SET_PATH_INFORMATION);
	byte_count = 3 /* pad */  + params + name_len_target;
	pSMB->ParameterCount = cpu_to_le16(params);
	pSMB->TotalParameterCount = pSMB->ParameterCount;
	pSMB->DataCount = cpu_to_le16(name_len_target);
	pSMB->TotalDataCount = pSMB->DataCount;
	pSMB->ParameterOffset = cpu_to_le16(param_offset);
	pSMB->DataOffset = cpu_to_le16(offset);
	pSMB->InformationLevel = cpu_to_le16(SMB_SET_FILE_UNIX_HLINK);
	pSMB->Reserved4 = 0;
	pSMB->hdr.smb_buf_length += byte_count;
	pSMB->ByteCount = cpu_to_le16(byte_count);
	rc = SendReceive(xid, tcon->ses, (struct smb_hdr *) pSMB,
			 (struct smb_hdr *) pSMBr, &bytes_returned, 0);
	cifs_stats_inc(&tcon->num_hardlinks);
	if (rc)
		cFYI(1, ("Send error in SetPathInfo (hard link) = %d", rc));

	cifs_buf_release(pSMB);
	if (rc == -EAGAIN)
		goto createHardLinkRetry;

	return rc;
}

int
CIFSCreateHardLink(const int xid, struct cifsTconInfo *tcon,
		   const char *fromName, const char *toName,
		   const struct nls_table *nls_codepage, int remap)
{
	int rc = 0;
	NT_RENAME_REQ *pSMB = NULL;
	RENAME_RSP *pSMBr = NULL;
	int bytes_returned;
	int name_len, name_len2;
	__u16 count;

	cFYI(1, ("In CIFSCreateHardLink"));
winCreateHardLinkRetry:

	rc = smb_init(SMB_COM_NT_RENAME, 4, tcon, (void **) &pSMB,
		      (void **) &pSMBr);
	if (rc)
		return rc;

	pSMB->SearchAttributes =
	    cpu_to_le16(ATTR_READONLY | ATTR_HIDDEN | ATTR_SYSTEM |
			ATTR_DIRECTORY);
	pSMB->Flags = cpu_to_le16(CREATE_HARD_LINK);
	pSMB->ClusterCount = 0;

	pSMB->BufferFormat = 0x04;

	if (pSMB->hdr.Flags2 & SMBFLG2_UNICODE) {
		name_len =
		    cifsConvertToUCS((__le16 *) pSMB->OldFileName, fromName,
				     PATH_MAX, nls_codepage, remap);
		name_len++;	/* trailing null */
		name_len *= 2;

		/* protocol specifies ASCII buffer format (0x04) for unicode */
		pSMB->OldFileName[name_len] = 0x04;
		pSMB->OldFileName[name_len + 1] = 0x00; /* pad */
		name_len2 =
		    cifsConvertToUCS((__le16 *)&pSMB->OldFileName[name_len + 2],
				     toName, PATH_MAX, nls_codepage, remap);
		name_len2 += 1 /* trailing null */  + 1 /* Signature word */ ;
		name_len2 *= 2;	/* convert to bytes */
	} else {	/* BB improve the check for buffer overruns BB */
		name_len = strnlen(fromName, PATH_MAX);
		name_len++;	/* trailing null */
		strncpy(pSMB->OldFileName, fromName, name_len);
		name_len2 = strnlen(toName, PATH_MAX);
		name_len2++;	/* trailing null */
		pSMB->OldFileName[name_len] = 0x04;	/* 2nd buffer format */
		strncpy(&pSMB->OldFileName[name_len + 1], toName, name_len2);
		name_len2++;	/* trailing null */
		name_len2++;	/* signature byte */
	}

	count = 1 /* string type byte */  + name_len + name_len2;
	pSMB->hdr.smb_buf_length += count;
	pSMB->ByteCount = cpu_to_le16(count);

	rc = SendReceive(xid, tcon->ses, (struct smb_hdr *) pSMB,
			 (struct smb_hdr *) pSMBr, &bytes_returned, 0);
	cifs_stats_inc(&tcon->num_hardlinks);
	if (rc)
		cFYI(1, ("Send error in hard link (NT rename) = %d", rc));

	cifs_buf_release(pSMB);
	if (rc == -EAGAIN)
		goto winCreateHardLinkRetry;

	return rc;
}

int
CIFSSMBUnixQuerySymLink(const int xid, struct cifsTconInfo *tcon,
			const unsigned char *searchName, char **symlinkinfo,
			const struct nls_table *nls_codepage)
{
/* SMB_QUERY_FILE_UNIX_LINK */
	TRANSACTION2_QPI_REQ *pSMB = NULL;
	TRANSACTION2_QPI_RSP *pSMBr = NULL;
	int rc = 0;
	int bytes_returned;
	int name_len;
	__u16 params, byte_count;
	char *data_start;

	cFYI(1, ("In QPathSymLinkInfo (Unix) for path %s", searchName));

querySymLinkRetry:
	rc = smb_init(SMB_COM_TRANSACTION2, 15, tcon, (void **) &pSMB,
		      (void **) &pSMBr);
	if (rc)
		return rc;

	if (pSMB->hdr.Flags2 & SMBFLG2_UNICODE) {
		name_len =
		    cifs_strtoUCS((__le16 *) pSMB->FileName, searchName,
				  PATH_MAX, nls_codepage);
		name_len++;	/* trailing null */
		name_len *= 2;
	} else {	/* BB improve the check for buffer overruns BB */
		name_len = strnlen(searchName, PATH_MAX);
		name_len++;	/* trailing null */
		strncpy(pSMB->FileName, searchName, name_len);
	}

	params = 2 /* level */  + 4 /* rsrvd */  + name_len /* incl null */ ;
	pSMB->TotalDataCount = 0;
	pSMB->MaxParameterCount = cpu_to_le16(2);
	pSMB->MaxDataCount = cpu_to_le16(CIFSMaxBufSize);
	pSMB->MaxSetupCount = 0;
	pSMB->Reserved = 0;
	pSMB->Flags = 0;
	pSMB->Timeout = 0;
	pSMB->Reserved2 = 0;
	pSMB->ParameterOffset = cpu_to_le16(offsetof(
	struct smb_com_transaction2_qpi_req, InformationLevel) - 4);
	pSMB->DataCount = 0;
	pSMB->DataOffset = 0;
	pSMB->SetupCount = 1;
	pSMB->Reserved3 = 0;
	pSMB->SubCommand = cpu_to_le16(TRANS2_QUERY_PATH_INFORMATION);
	byte_count = params + 1 /* pad */ ;
	pSMB->TotalParameterCount = cpu_to_le16(params);
	pSMB->ParameterCount = pSMB->TotalParameterCount;
	pSMB->InformationLevel = cpu_to_le16(SMB_QUERY_FILE_UNIX_LINK);
	pSMB->Reserved4 = 0;
	pSMB->hdr.smb_buf_length += byte_count;
	pSMB->ByteCount = cpu_to_le16(byte_count);

	rc = SendReceive(xid, tcon->ses, (struct smb_hdr *) pSMB,
			 (struct smb_hdr *) pSMBr, &bytes_returned, 0);
	if (rc) {
		cFYI(1, ("Send error in QuerySymLinkInfo = %d", rc));
	} else {
		/* decode response */

		rc = validate_t2((struct smb_t2_rsp *)pSMBr);
		/* BB also check enough total bytes returned */
		if (rc || (pSMBr->ByteCount < 2))
			rc = -EIO;
		else {
			bool is_unicode;
			u16 count = le16_to_cpu(pSMBr->t2.DataCount);

			data_start = ((char *) &pSMBr->hdr.Protocol) +
					   le16_to_cpu(pSMBr->t2.DataOffset);

			if (pSMBr->hdr.Flags2 & SMBFLG2_UNICODE)
				is_unicode = true;
			else
				is_unicode = false;

			/* BB FIXME investigate remapping reserved chars here */
			*symlinkinfo = cifs_strndup_from_ucs(data_start, count,
						    is_unicode, nls_codepage);
			if (!*symlinkinfo)
				rc = -ENOMEM;
		}
	}
	cifs_buf_release(pSMB);
	if (rc == -EAGAIN)
		goto querySymLinkRetry;
	return rc;
}

#ifdef CONFIG_CIFS_EXPERIMENTAL
/* Initialize NT TRANSACT SMB into small smb request buffer.
   This assumes that all NT TRANSACTS that we init here have
   total parm and data under about 400 bytes (to fit in small cifs
   buffer size), which is the case so far, it easily fits. NB:
	Setup words themselves and ByteCount
	MaxSetupCount (size of returned setup area) and
	MaxParameterCount (returned parms size) must be set by caller */
static int
smb_init_nttransact(const __u16 sub_command, const int setup_count,
		   const int parm_len, struct cifsTconInfo *tcon,
		   void **ret_buf)
{
	int rc;
	__u32 temp_offset;
	struct smb_com_ntransact_req *pSMB;

	rc = small_smb_init(SMB_COM_NT_TRANSACT, 19 + setup_count, tcon,
				(void **)&pSMB);
	if (rc)
		return rc;
	*ret_buf = (void *)pSMB;
	pSMB->Reserved = 0;
	pSMB->TotalParameterCount = cpu_to_le32(parm_len);
	pSMB->TotalDataCount  = 0;
	pSMB->MaxDataCount = cpu_to_le32((tcon->ses->server->maxBuf -
					  MAX_CIFS_HDR_SIZE) & 0xFFFFFF00);
	pSMB->ParameterCount = pSMB->TotalParameterCount;
	pSMB->DataCount  = pSMB->TotalDataCount;
	temp_offset = offsetof(struct smb_com_ntransact_req, Parms) +
			(setup_count * 2) - 4 /* for rfc1001 length itself */;
	pSMB->ParameterOffset = cpu_to_le32(temp_offset);
	pSMB->DataOffset = cpu_to_le32(temp_offset + parm_len);
	pSMB->SetupCount = setup_count; /* no need to le convert byte fields */
	pSMB->SubCommand = cpu_to_le16(sub_command);
	return 0;
}

static int
validate_ntransact(char *buf, char **ppparm, char **ppdata,
		   __u32 *pparmlen, __u32 *pdatalen)
{
	char *end_of_smb;
	__u32 data_count, data_offset, parm_count, parm_offset;
	struct smb_com_ntransact_rsp *pSMBr;

	*pdatalen = 0;
	*pparmlen = 0;

	if (buf == NULL)
		return -EINVAL;

	pSMBr = (struct smb_com_ntransact_rsp *)buf;

	/* ByteCount was converted from little endian in SendReceive */
	end_of_smb = 2 /* sizeof byte count */ + pSMBr->ByteCount +
			(char *)&pSMBr->ByteCount;

	data_offset = le32_to_cpu(pSMBr->DataOffset);
	data_count = le32_to_cpu(pSMBr->DataCount);
	parm_offset = le32_to_cpu(pSMBr->ParameterOffset);
	parm_count = le32_to_cpu(pSMBr->ParameterCount);

	*ppparm = (char *)&pSMBr->hdr.Protocol + parm_offset;
	*ppdata = (char *)&pSMBr->hdr.Protocol + data_offset;

	/* should we also check that parm and data areas do not overlap? */
	if (*ppparm > end_of_smb) {
		cFYI(1, ("parms start after end of smb"));
		return -EINVAL;
	} else if (parm_count + *ppparm > end_of_smb) {
		cFYI(1, ("parm end after end of smb"));
		return -EINVAL;
	} else if (*ppdata > end_of_smb) {
		cFYI(1, ("data starts after end of smb"));
		return -EINVAL;
	} else if (data_count + *ppdata > end_of_smb) {
		cFYI(1, ("data %p + count %d (%p) ends after end of smb %p start %p",
			*ppdata, data_count, (data_count + *ppdata),
			end_of_smb, pSMBr));
		return -EINVAL;
	} else if (parm_count + data_count > pSMBr->ByteCount) {
		cFYI(1, ("parm count and data count larger than SMB"));
		return -EINVAL;
	}
	*pdatalen = data_count;
	*pparmlen = parm_count;
	return 0;
}

int
CIFSSMBQueryReparseLinkInfo(const int xid, struct cifsTconInfo *tcon,
			const unsigned char *searchName,
			char *symlinkinfo, const int buflen, __u16 fid,
			const struct nls_table *nls_codepage)
{
	int rc = 0;
	int bytes_returned;
	struct smb_com_transaction_ioctl_req *pSMB;
	struct smb_com_transaction_ioctl_rsp *pSMBr;

	cFYI(1, ("In Windows reparse style QueryLink for path %s", searchName));
	rc = smb_init(SMB_COM_NT_TRANSACT, 23, tcon, (void **) &pSMB,
		      (void **) &pSMBr);
	if (rc)
		return rc;

	pSMB->TotalParameterCount = 0 ;
	pSMB->TotalDataCount = 0;
	pSMB->MaxParameterCount = cpu_to_le32(2);
	/* BB find exact data count max from sess structure BB */
	pSMB->MaxDataCount = cpu_to_le32((tcon->ses->server->maxBuf -
					  MAX_CIFS_HDR_SIZE) & 0xFFFFFF00);
	pSMB->MaxSetupCount = 4;
	pSMB->Reserved = 0;
	pSMB->ParameterOffset = 0;
	pSMB->DataCount = 0;
	pSMB->DataOffset = 0;
	pSMB->SetupCount = 4;
	pSMB->SubCommand = cpu_to_le16(NT_TRANSACT_IOCTL);
	pSMB->ParameterCount = pSMB->TotalParameterCount;
	pSMB->FunctionCode = cpu_to_le32(FSCTL_GET_REPARSE_POINT);
	pSMB->IsFsctl = 1; /* FSCTL */
	pSMB->IsRootFlag = 0;
	pSMB->Fid = fid; /* file handle always le */
	pSMB->ByteCount = 0;

	rc = SendReceive(xid, tcon->ses, (struct smb_hdr *) pSMB,
			 (struct smb_hdr *) pSMBr, &bytes_returned, 0);
	if (rc) {
		cFYI(1, ("Send error in QueryReparseLinkInfo = %d", rc));
	} else {		/* decode response */
		__u32 data_offset = le32_to_cpu(pSMBr->DataOffset);
		__u32 data_count = le32_to_cpu(pSMBr->DataCount);
		if ((pSMBr->ByteCount < 2) || (data_offset > 512)) {
		/* BB also check enough total bytes returned */
			rc = -EIO;	/* bad smb */
			goto qreparse_out;
		}
		if (data_count && (data_count < 2048)) {
			char *end_of_smb = 2 /* sizeof byte count */ +
				pSMBr->ByteCount + (char *)&pSMBr->ByteCount;

			struct reparse_data *reparse_buf =
						(struct reparse_data *)
						((char *)&pSMBr->hdr.Protocol
								 + data_offset);
			if ((char *)reparse_buf >= end_of_smb) {
				rc = -EIO;
				goto qreparse_out;
			}
			if ((reparse_buf->LinkNamesBuf +
				reparse_buf->TargetNameOffset +
				reparse_buf->TargetNameLen) > end_of_smb) {
				cFYI(1, ("reparse buf beyond SMB"));
				rc = -EIO;
				goto qreparse_out;
			}

			if (pSMBr->hdr.Flags2 & SMBFLG2_UNICODE) {
				cifs_from_ucs2(symlinkinfo, (__le16 *)
						(reparse_buf->LinkNamesBuf +
						reparse_buf->TargetNameOffset),
						buflen,
						reparse_buf->TargetNameLen,
						nls_codepage, 0);
			} else { /* ASCII names */
				strncpy(symlinkinfo,
					reparse_buf->LinkNamesBuf +
					reparse_buf->TargetNameOffset,
					min_t(const int, buflen,
					   reparse_buf->TargetNameLen));
			}
		} else {
			rc = -EIO;
			cFYI(1, ("Invalid return data count on "
				 "get reparse info ioctl"));
		}
		symlinkinfo[buflen] = 0; /* just in case so the caller
					does not go off the end of the buffer */
		cFYI(1, ("readlink result - %s", symlinkinfo));
	}

qreparse_out:
	cifs_buf_release(pSMB);

	/* Note: On -EAGAIN error only caller can retry on handle based calls
		since file handle passed in no longer valid */

	return rc;
}
#endif /* CIFS_EXPERIMENTAL */

#ifdef CONFIG_CIFS_POSIX

/*Convert an Access Control Entry from wire format to local POSIX xattr format*/
static void cifs_convert_ace(posix_acl_xattr_entry *ace,
			     struct cifs_posix_ace *cifs_ace)
{
	/* u8 cifs fields do not need le conversion */
	ace->e_perm = cpu_to_le16(cifs_ace->cifs_e_perm);
	ace->e_tag  = cpu_to_le16(cifs_ace->cifs_e_tag);
	ace->e_id   = cpu_to_le32(le64_to_cpu(cifs_ace->cifs_uid));
	/* cFYI(1,("perm %d tag %d id %d",ace->e_perm,ace->e_tag,ace->e_id)); */

	return;
}

/* Convert ACL from CIFS POSIX wire format to local Linux POSIX ACL xattr */
static int cifs_copy_posix_acl(char *trgt, char *src, const int buflen,
			       const int acl_type, const int size_of_data_area)
{
	int size =  0;
	int i;
	__u16 count;
	struct cifs_posix_ace *pACE;
	struct cifs_posix_acl *cifs_acl = (struct cifs_posix_acl *)src;
	posix_acl_xattr_header *local_acl = (posix_acl_xattr_header *)trgt;

	if (le16_to_cpu(cifs_acl->version) != CIFS_ACL_VERSION)
		return -EOPNOTSUPP;

	if (acl_type & ACL_TYPE_ACCESS) {
		count = le16_to_cpu(cifs_acl->access_entry_count);
		pACE = &cifs_acl->ace_array[0];
		size = sizeof(struct cifs_posix_acl);
		size += sizeof(struct cifs_posix_ace) * count;
		/* check if we would go beyond end of SMB */
		if (size_of_data_area < size) {
			cFYI(1, ("bad CIFS POSIX ACL size %d vs. %d",
				size_of_data_area, size));
			return -EINVAL;
		}
	} else if (acl_type & ACL_TYPE_DEFAULT) {
		count = le16_to_cpu(cifs_acl->access_entry_count);
		size = sizeof(struct cifs_posix_acl);
		size += sizeof(struct cifs_posix_ace) * count;
/* skip past access ACEs to get to default ACEs */
		pACE = &cifs_acl->ace_array[count];
		count = le16_to_cpu(cifs_acl->default_entry_count);
		size += sizeof(struct cifs_posix_ace) * count;
		/* check if we would go beyond end of SMB */
		if (size_of_data_area < size)
			return -EINVAL;
	} else {
		/* illegal type */
		return -EINVAL;
	}

	size = posix_acl_xattr_size(count);
	if ((buflen == 0) || (local_acl == NULL)) {
		/* used to query ACL EA size */
	} else if (size > buflen) {
		return -ERANGE;
	} else /* buffer big enough */ {
		local_acl->a_version = cpu_to_le32(POSIX_ACL_XATTR_VERSION);
		for (i = 0; i < count ; i++) {
			cifs_convert_ace(&local_acl->a_entries[i], pACE);
			pACE++;
		}
	}
	return size;
}

static __u16 convert_ace_to_cifs_ace(struct cifs_posix_ace *cifs_ace,
				     const posix_acl_xattr_entry *local_ace)
{
	__u16 rc = 0; /* 0 = ACL converted ok */

	cifs_ace->cifs_e_perm = le16_to_cpu(local_ace->e_perm);
	cifs_ace->cifs_e_tag =  le16_to_cpu(local_ace->e_tag);
	/* BB is there a better way to handle the large uid? */
	if (local_ace->e_id == cpu_to_le32(-1)) {
	/* Probably no need to le convert -1 on any arch but can not hurt */
		cifs_ace->cifs_uid = cpu_to_le64(-1);
	} else
		cifs_ace->cifs_uid = cpu_to_le64(le32_to_cpu(local_ace->e_id));
	/*cFYI(1,("perm %d tag %d id %d",ace->e_perm,ace->e_tag,ace->e_id));*/
	return rc;
}

/* Convert ACL from local Linux POSIX xattr to CIFS POSIX ACL wire format */
static __u16 ACL_to_cifs_posix(char *parm_data, const char *pACL,
			       const int buflen, const int acl_type)
{
	__u16 rc = 0;
	struct cifs_posix_acl *cifs_acl = (struct cifs_posix_acl *)parm_data;
	posix_acl_xattr_header *local_acl = (posix_acl_xattr_header *)pACL;
	int count;
	int i;

	if ((buflen == 0) || (pACL == NULL) || (cifs_acl == NULL))
		return 0;

	count = posix_acl_xattr_count((size_t)buflen);
	cFYI(1, ("setting acl with %d entries from buf of length %d and "
		"version of %d",
		count, buflen, le32_to_cpu(local_acl->a_version)));
	if (le32_to_cpu(local_acl->a_version) != 2) {
		cFYI(1, ("unknown POSIX ACL version %d",
		     le32_to_cpu(local_acl->a_version)));
		return 0;
	}
	cifs_acl->version = cpu_to_le16(1);
	if (acl_type == ACL_TYPE_ACCESS)
		cifs_acl->access_entry_count = cpu_to_le16(count);
	else if (acl_type == ACL_TYPE_DEFAULT)
		cifs_acl->default_entry_count = cpu_to_le16(count);
	else {
		cFYI(1, ("unknown ACL type %d", acl_type));
		return 0;
	}
	for (i = 0; i < count; i++) {
		rc = convert_ace_to_cifs_ace(&cifs_acl->ace_array[i],
					&local_acl->a_entries[i]);
		if (rc != 0) {
			/* ACE not converted */
			break;
		}
	}
	if (rc == 0) {
		rc = (__u16)(count * sizeof(struct cifs_posix_ace));
		rc += sizeof(struct cifs_posix_acl);
		/* BB add check to make sure ACL does not overflow SMB */
	}
	return rc;
}

int
CIFSSMBGetPosixACL(const int xid, struct cifsTconInfo *tcon,
		   const unsigned char *searchName,
		   char *acl_inf, const int buflen, const int acl_type,
		   const struct nls_table *nls_codepage, int remap)
{
/* SMB_QUERY_POSIX_ACL */
	TRANSACTION2_QPI_REQ *pSMB = NULL;
	TRANSACTION2_QPI_RSP *pSMBr = NULL;
	int rc = 0;
	int bytes_returned;
	int name_len;
	__u16 params, byte_count;

	cFYI(1, ("In GetPosixACL (Unix) for path %s", searchName));

queryAclRetry:
	rc = smb_init(SMB_COM_TRANSACTION2, 15, tcon, (void **) &pSMB,
		(void **) &pSMBr);
	if (rc)
		return rc;

	if (pSMB->hdr.Flags2 & SMBFLG2_UNICODE) {
		name_len =
			cifsConvertToUCS((__le16 *) pSMB->FileName, searchName,
					 PATH_MAX, nls_codepage, remap);
		name_len++;     /* trailing null */
		name_len *= 2;
		pSMB->FileName[name_len] = 0;
		pSMB->FileName[name_len+1] = 0;
	} else {	/* BB improve the check for buffer overruns BB */
		name_len = strnlen(searchName, PATH_MAX);
		name_len++;     /* trailing null */
		strncpy(pSMB->FileName, searchName, name_len);
	}

	params = 2 /* level */  + 4 /* rsrvd */  + name_len /* incl null */ ;
	pSMB->TotalDataCount = 0;
	pSMB->MaxParameterCount = cpu_to_le16(2);
	/* BB find exact max data count below from sess structure BB */
	pSMB->MaxDataCount = cpu_to_le16(4000);
	pSMB->MaxSetupCount = 0;
	pSMB->Reserved = 0;
	pSMB->Flags = 0;
	pSMB->Timeout = 0;
	pSMB->Reserved2 = 0;
	pSMB->ParameterOffset = cpu_to_le16(
		offsetof(struct smb_com_transaction2_qpi_req,
			 InformationLevel) - 4);
	pSMB->DataCount = 0;
	pSMB->DataOffset = 0;
	pSMB->SetupCount = 1;
	pSMB->Reserved3 = 0;
	pSMB->SubCommand = cpu_to_le16(TRANS2_QUERY_PATH_INFORMATION);
	byte_count = params + 1 /* pad */ ;
	pSMB->TotalParameterCount = cpu_to_le16(params);
	pSMB->ParameterCount = pSMB->TotalParameterCount;
	pSMB->InformationLevel = cpu_to_le16(SMB_QUERY_POSIX_ACL);
	pSMB->Reserved4 = 0;
	pSMB->hdr.smb_buf_length += byte_count;
	pSMB->ByteCount = cpu_to_le16(byte_count);

	rc = SendReceive(xid, tcon->ses, (struct smb_hdr *) pSMB,
		(struct smb_hdr *) pSMBr, &bytes_returned, 0);
	cifs_stats_inc(&tcon->num_acl_get);
	if (rc) {
		cFYI(1, ("Send error in Query POSIX ACL = %d", rc));
	} else {
		/* decode response */

		rc = validate_t2((struct smb_t2_rsp *)pSMBr);
		if (rc || (pSMBr->ByteCount < 2))
		/* BB also check enough total bytes returned */
			rc = -EIO;      /* bad smb */
		else {
			__u16 data_offset = le16_to_cpu(pSMBr->t2.DataOffset);
			__u16 count = le16_to_cpu(pSMBr->t2.DataCount);
			rc = cifs_copy_posix_acl(acl_inf,
				(char *)&pSMBr->hdr.Protocol+data_offset,
				buflen, acl_type, count);
		}
	}
	cifs_buf_release(pSMB);
	if (rc == -EAGAIN)
		goto queryAclRetry;
	return rc;
}

int
CIFSSMBSetPosixACL(const int xid, struct cifsTconInfo *tcon,
		   const unsigned char *fileName,
		   const char *local_acl, const int buflen,
		   const int acl_type,
		   const struct nls_table *nls_codepage, int remap)
{
	struct smb_com_transaction2_spi_req *pSMB = NULL;
	struct smb_com_transaction2_spi_rsp *pSMBr = NULL;
	char *parm_data;
	int name_len;
	int rc = 0;
	int bytes_returned = 0;
	__u16 params, byte_count, data_count, param_offset, offset;

	cFYI(1, ("In SetPosixACL (Unix) for path %s", fileName));
setAclRetry:
	rc = smb_init(SMB_COM_TRANSACTION2, 15, tcon, (void **) &pSMB,
		      (void **) &pSMBr);
	if (rc)
		return rc;
	if (pSMB->hdr.Flags2 & SMBFLG2_UNICODE) {
		name_len =
			cifsConvertToUCS((__le16 *) pSMB->FileName, fileName,
				      PATH_MAX, nls_codepage, remap);
		name_len++;     /* trailing null */
		name_len *= 2;
	} else {	/* BB improve the check for buffer overruns BB */
		name_len = strnlen(fileName, PATH_MAX);
		name_len++;     /* trailing null */
		strncpy(pSMB->FileName, fileName, name_len);
	}
	params = 6 + name_len;
	pSMB->MaxParameterCount = cpu_to_le16(2);
	/* BB find max SMB size from sess */
	pSMB->MaxDataCount = cpu_to_le16(1000);
	pSMB->MaxSetupCount = 0;
	pSMB->Reserved = 0;
	pSMB->Flags = 0;
	pSMB->Timeout = 0;
	pSMB->Reserved2 = 0;
	param_offset = offsetof(struct smb_com_transaction2_spi_req,
				InformationLevel) - 4;
	offset = param_offset + params;
	parm_data = ((char *) &pSMB->hdr.Protocol) + offset;
	pSMB->ParameterOffset = cpu_to_le16(param_offset);

	/* convert to on the wire format for POSIX ACL */
	data_count = ACL_to_cifs_posix(parm_data, local_acl, buflen, acl_type);

	if (data_count == 0) {
		rc = -EOPNOTSUPP;
		goto setACLerrorExit;
	}
	pSMB->DataOffset = cpu_to_le16(offset);
	pSMB->SetupCount = 1;
	pSMB->Reserved3 = 0;
	pSMB->SubCommand = cpu_to_le16(TRANS2_SET_PATH_INFORMATION);
	pSMB->InformationLevel = cpu_to_le16(SMB_SET_POSIX_ACL);
	byte_count = 3 /* pad */  + params + data_count;
	pSMB->DataCount = cpu_to_le16(data_count);
	pSMB->TotalDataCount = pSMB->DataCount;
	pSMB->ParameterCount = cpu_to_le16(params);
	pSMB->TotalParameterCount = pSMB->ParameterCount;
	pSMB->Reserved4 = 0;
	pSMB->hdr.smb_buf_length += byte_count;
	pSMB->ByteCount = cpu_to_le16(byte_count);
	rc = SendReceive(xid, tcon->ses, (struct smb_hdr *) pSMB,
			 (struct smb_hdr *) pSMBr, &bytes_returned, 0);
	if (rc)
		cFYI(1, ("Set POSIX ACL returned %d", rc));

setACLerrorExit:
	cifs_buf_release(pSMB);
	if (rc == -EAGAIN)
		goto setAclRetry;
	return rc;
}

/* BB fix tabs in this function FIXME BB */
int
CIFSGetExtAttr(const int xid, struct cifsTconInfo *tcon,
	       const int netfid, __u64 *pExtAttrBits, __u64 *pMask)
{
	int rc = 0;
	struct smb_t2_qfi_req *pSMB = NULL;
	struct smb_t2_qfi_rsp *pSMBr = NULL;
	int bytes_returned;
	__u16 params, byte_count;

	cFYI(1, ("In GetExtAttr"));
	if (tcon == NULL)
		return -ENODEV;

GetExtAttrRetry:
	rc = smb_init(SMB_COM_TRANSACTION2, 15, tcon, (void **) &pSMB,
			(void **) &pSMBr);
	if (rc)
		return rc;

	params = 2 /* level */ + 2 /* fid */;
	pSMB->t2.TotalDataCount = 0;
	pSMB->t2.MaxParameterCount = cpu_to_le16(4);
	/* BB find exact max data count below from sess structure BB */
	pSMB->t2.MaxDataCount = cpu_to_le16(4000);
	pSMB->t2.MaxSetupCount = 0;
	pSMB->t2.Reserved = 0;
	pSMB->t2.Flags = 0;
	pSMB->t2.Timeout = 0;
	pSMB->t2.Reserved2 = 0;
	pSMB->t2.ParameterOffset = cpu_to_le16(offsetof(struct smb_t2_qfi_req,
					       Fid) - 4);
	pSMB->t2.DataCount = 0;
	pSMB->t2.DataOffset = 0;
	pSMB->t2.SetupCount = 1;
	pSMB->t2.Reserved3 = 0;
	pSMB->t2.SubCommand = cpu_to_le16(TRANS2_QUERY_FILE_INFORMATION);
	byte_count = params + 1 /* pad */ ;
	pSMB->t2.TotalParameterCount = cpu_to_le16(params);
	pSMB->t2.ParameterCount = pSMB->t2.TotalParameterCount;
	pSMB->InformationLevel = cpu_to_le16(SMB_QUERY_ATTR_FLAGS);
	pSMB->Pad = 0;
	pSMB->Fid = netfid;
	pSMB->hdr.smb_buf_length += byte_count;
	pSMB->t2.ByteCount = cpu_to_le16(byte_count);

	rc = SendReceive(xid, tcon->ses, (struct smb_hdr *) pSMB,
			 (struct smb_hdr *) pSMBr, &bytes_returned, 0);
	if (rc) {
		cFYI(1, ("error %d in GetExtAttr", rc));
	} else {
		/* decode response */
		rc = validate_t2((struct smb_t2_rsp *)pSMBr);
		if (rc || (pSMBr->ByteCount < 2))
		/* BB also check enough total bytes returned */
			/* If rc should we check for EOPNOSUPP and
			   disable the srvino flag? or in caller? */
			rc = -EIO;      /* bad smb */
		else {
			__u16 data_offset = le16_to_cpu(pSMBr->t2.DataOffset);
			__u16 count = le16_to_cpu(pSMBr->t2.DataCount);
			struct file_chattr_info *pfinfo;
			/* BB Do we need a cast or hash here ? */
			if (count != 16) {
				cFYI(1, ("Illegal size ret in GetExtAttr"));
				rc = -EIO;
				goto GetExtAttrOut;
			}
			pfinfo = (struct file_chattr_info *)
				 (data_offset + (char *) &pSMBr->hdr.Protocol);
			*pExtAttrBits = le64_to_cpu(pfinfo->mode);
			*pMask = le64_to_cpu(pfinfo->mask);
		}
	}
GetExtAttrOut:
	cifs_buf_release(pSMB);
	if (rc == -EAGAIN)
		goto GetExtAttrRetry;
	return rc;
}

#endif /* CONFIG_POSIX */

#ifdef CONFIG_CIFS_EXPERIMENTAL
/* Get Security Descriptor (by handle) from remote server for a file or dir */
int
CIFSSMBGetCIFSACL(const int xid, struct cifsTconInfo *tcon, __u16 fid,
		  struct cifs_ntsd **acl_inf, __u32 *pbuflen)
{
	int rc = 0;
	int buf_type = 0;
	QUERY_SEC_DESC_REQ *pSMB;
	struct kvec iov[1];

	cFYI(1, ("GetCifsACL"));

	*pbuflen = 0;
	*acl_inf = NULL;

	rc = smb_init_nttransact(NT_TRANSACT_QUERY_SECURITY_DESC, 0,
			8 /* parm len */, tcon, (void **) &pSMB);
	if (rc)
		return rc;

	pSMB->MaxParameterCount = cpu_to_le32(4);
	/* BB TEST with big acls that might need to be e.g. larger than 16K */
	pSMB->MaxSetupCount = 0;
	pSMB->Fid = fid; /* file handle always le */
	pSMB->AclFlags = cpu_to_le32(CIFS_ACL_OWNER | CIFS_ACL_GROUP |
				     CIFS_ACL_DACL);
	pSMB->ByteCount = cpu_to_le16(11); /* 3 bytes pad + 8 bytes parm */
	pSMB->hdr.smb_buf_length += 11;
	iov[0].iov_base = (char *)pSMB;
	iov[0].iov_len = pSMB->hdr.smb_buf_length + 4;

	rc = SendReceive2(xid, tcon->ses, iov, 1 /* num iovec */, &buf_type,
			 CIFS_STD_OP);
	cifs_stats_inc(&tcon->num_acl_get);
	if (rc) {
		cFYI(1, ("Send error in QuerySecDesc = %d", rc));
	} else {                /* decode response */
		__le32 *parm;
		__u32 parm_len;
		__u32 acl_len;
		struct smb_com_ntransact_rsp *pSMBr;
		char *pdata;

/* validate_nttransact */
		rc = validate_ntransact(iov[0].iov_base, (char **)&parm,
					&pdata, &parm_len, pbuflen);
		if (rc)
			goto qsec_out;
		pSMBr = (struct smb_com_ntransact_rsp *)iov[0].iov_base;

		cFYI(1, ("smb %p parm %p data %p", pSMBr, parm, *acl_inf));

		if (le32_to_cpu(pSMBr->ParameterCount) != 4) {
			rc = -EIO;      /* bad smb */
			*pbuflen = 0;
			goto qsec_out;
		}

/* BB check that data area is minimum length and as big as acl_len */

		acl_len = le32_to_cpu(*parm);
		if (acl_len != *pbuflen) {
			cERROR(1, ("acl length %d does not match %d",
				   acl_len, *pbuflen));
			if (*pbuflen > acl_len)
				*pbuflen = acl_len;
		}

		/* check if buffer is big enough for the acl
		   header followed by the smallest SID */
		if ((*pbuflen < sizeof(struct cifs_ntsd) + 8) ||
		    (*pbuflen >= 64 * 1024)) {
			cERROR(1, ("bad acl length %d", *pbuflen));
			rc = -EINVAL;
			*pbuflen = 0;
		} else {
			*acl_inf = kmalloc(*pbuflen, GFP_KERNEL);
			if (*acl_inf == NULL) {
				*pbuflen = 0;
				rc = -ENOMEM;
			}
			memcpy(*acl_inf, pdata, *pbuflen);
		}
	}
qsec_out:
	if (buf_type == CIFS_SMALL_BUFFER)
		cifs_small_buf_release(iov[0].iov_base);
	else if (buf_type == CIFS_LARGE_BUFFER)
		cifs_buf_release(iov[0].iov_base);
/*	cifs_small_buf_release(pSMB); */ /* Freed earlier now in SendReceive2 */
	return rc;
}

int
CIFSSMBSetCIFSACL(const int xid, struct cifsTconInfo *tcon, __u16 fid,
			struct cifs_ntsd *pntsd, __u32 acllen)
{
	__u16 byte_count, param_count, data_count, param_offset, data_offset;
	int rc = 0;
	int bytes_returned = 0;
	SET_SEC_DESC_REQ *pSMB = NULL;
	NTRANSACT_RSP *pSMBr = NULL;

setCifsAclRetry:
	rc = smb_init(SMB_COM_NT_TRANSACT, 19, tcon, (void **) &pSMB,
			(void **) &pSMBr);
	if (rc)
			return (rc);

	pSMB->MaxSetupCount = 0;
	pSMB->Reserved = 0;

	param_count = 8;
	param_offset = offsetof(struct smb_com_transaction_ssec_req, Fid) - 4;
	data_count = acllen;
	data_offset = param_offset + param_count;
	byte_count = 3 /* pad */  + param_count;

	pSMB->DataCount = cpu_to_le32(data_count);
	pSMB->TotalDataCount = pSMB->DataCount;
	pSMB->MaxParameterCount = cpu_to_le32(4);
	pSMB->MaxDataCount = cpu_to_le32(16384);
	pSMB->ParameterCount = cpu_to_le32(param_count);
	pSMB->ParameterOffset = cpu_to_le32(param_offset);
	pSMB->TotalParameterCount = pSMB->ParameterCount;
	pSMB->DataOffset = cpu_to_le32(data_offset);
	pSMB->SetupCount = 0;
	pSMB->SubCommand = cpu_to_le16(NT_TRANSACT_SET_SECURITY_DESC);
	pSMB->ByteCount = cpu_to_le16(byte_count+data_count);

	pSMB->Fid = fid; /* file handle always le */
	pSMB->Reserved2 = 0;
	pSMB->AclFlags = cpu_to_le32(CIFS_ACL_DACL);

	if (pntsd && acllen) {
		memcpy((char *) &pSMBr->hdr.Protocol + data_offset,
			(char *) pntsd,
			acllen);
		pSMB->hdr.smb_buf_length += (byte_count + data_count);

	} else
		pSMB->hdr.smb_buf_length += byte_count;

	rc = SendReceive(xid, tcon->ses, (struct smb_hdr *) pSMB,
		(struct smb_hdr *) pSMBr, &bytes_returned, 0);

	cFYI(1, ("SetCIFSACL bytes_returned: %d, rc: %d", bytes_returned, rc));
	if (rc)
		cFYI(1, ("Set CIFS ACL returned %d", rc));
	cifs_buf_release(pSMB);

	if (rc == -EAGAIN)
		goto setCifsAclRetry;

	return (rc);
}

#endif /* CONFIG_CIFS_EXPERIMENTAL */

/* Legacy Query Path Information call for lookup to old servers such
   as Win9x/WinME */
int SMBQueryInformation(const int xid, struct cifsTconInfo *tcon,
			const unsigned char *searchName,
			FILE_ALL_INFO *pFinfo,
			const struct nls_table *nls_codepage, int remap)
{
	QUERY_INFORMATION_REQ *pSMB;
	QUERY_INFORMATION_RSP *pSMBr;
	int rc = 0;
	int bytes_returned;
	int name_len;

	cFYI(1, ("In SMBQPath path %s", searchName));
QInfRetry:
	rc = smb_init(SMB_COM_QUERY_INFORMATION, 0, tcon, (void **) &pSMB,
		      (void **) &pSMBr);
	if (rc)
		return rc;

	if (pSMB->hdr.Flags2 & SMBFLG2_UNICODE) {
		name_len =
			cifsConvertToUCS((__le16 *) pSMB->FileName, searchName,
					PATH_MAX, nls_codepage, remap);
		name_len++;     /* trailing null */
		name_len *= 2;
	} else {
		name_len = strnlen(searchName, PATH_MAX);
		name_len++;     /* trailing null */
		strncpy(pSMB->FileName, searchName, name_len);
	}
	pSMB->BufferFormat = 0x04;
	name_len++; /* account for buffer type byte */
	pSMB->hdr.smb_buf_length += (__u16) name_len;
	pSMB->ByteCount = cpu_to_le16(name_len);

	rc = SendReceive(xid, tcon->ses, (struct smb_hdr *) pSMB,
			 (struct smb_hdr *) pSMBr, &bytes_returned, 0);
	if (rc) {
		cFYI(1, ("Send error in QueryInfo = %d", rc));
	} else if (pFinfo) {
		struct timespec ts;
		__u32 time = le32_to_cpu(pSMBr->last_write_time);

		/* decode response */
		/* BB FIXME - add time zone adjustment BB */
		memset(pFinfo, 0, sizeof(FILE_ALL_INFO));
		ts.tv_nsec = 0;
		ts.tv_sec = time;
		/* decode time fields */
		pFinfo->ChangeTime = cpu_to_le64(cifs_UnixTimeToNT(ts));
		pFinfo->LastWriteTime = pFinfo->ChangeTime;
		pFinfo->LastAccessTime = 0;
		pFinfo->AllocationSize =
			cpu_to_le64(le32_to_cpu(pSMBr->size));
		pFinfo->EndOfFile = pFinfo->AllocationSize;
		pFinfo->Attributes =
			cpu_to_le32(le16_to_cpu(pSMBr->attr));
	} else
		rc = -EIO; /* bad buffer passed in */

	cifs_buf_release(pSMB);

	if (rc == -EAGAIN)
		goto QInfRetry;

	return rc;
}




int
CIFSSMBQPathInfo(const int xid, struct cifsTconInfo *tcon,
		 const unsigned char *searchName,
		 FILE_ALL_INFO *pFindData,
		 int legacy /* old style infolevel */,
		 const struct nls_table *nls_codepage, int remap)
{
/* level 263 SMB_QUERY_FILE_ALL_INFO */
	TRANSACTION2_QPI_REQ *pSMB = NULL;
	TRANSACTION2_QPI_RSP *pSMBr = NULL;
	int rc = 0;
	int bytes_returned;
	int name_len;
	__u16 params, byte_count;

/* cFYI(1, ("In QPathInfo path %s", searchName)); */
QPathInfoRetry:
	rc = smb_init(SMB_COM_TRANSACTION2, 15, tcon, (void **) &pSMB,
		      (void **) &pSMBr);
	if (rc)
		return rc;

	if (pSMB->hdr.Flags2 & SMBFLG2_UNICODE) {
		name_len =
		    cifsConvertToUCS((__le16 *) pSMB->FileName, searchName,
				     PATH_MAX, nls_codepage, remap);
		name_len++;	/* trailing null */
		name_len *= 2;
	} else {	/* BB improve the check for buffer overruns BB */
		name_len = strnlen(searchName, PATH_MAX);
		name_len++;	/* trailing null */
		strncpy(pSMB->FileName, searchName, name_len);
	}

	params = 2 /* level */ + 4 /* reserved */ + name_len /* includes NUL */;
	pSMB->TotalDataCount = 0;
	pSMB->MaxParameterCount = cpu_to_le16(2);
	/* BB find exact max SMB PDU from sess structure BB */
	pSMB->MaxDataCount = cpu_to_le16(4000);
	pSMB->MaxSetupCount = 0;
	pSMB->Reserved = 0;
	pSMB->Flags = 0;
	pSMB->Timeout = 0;
	pSMB->Reserved2 = 0;
	pSMB->ParameterOffset = cpu_to_le16(offsetof(
	struct smb_com_transaction2_qpi_req, InformationLevel) - 4);
	pSMB->DataCount = 0;
	pSMB->DataOffset = 0;
	pSMB->SetupCount = 1;
	pSMB->Reserved3 = 0;
	pSMB->SubCommand = cpu_to_le16(TRANS2_QUERY_PATH_INFORMATION);
	byte_count = params + 1 /* pad */ ;
	pSMB->TotalParameterCount = cpu_to_le16(params);
	pSMB->ParameterCount = pSMB->TotalParameterCount;
	if (legacy)
		pSMB->InformationLevel = cpu_to_le16(SMB_INFO_STANDARD);
	else
		pSMB->InformationLevel = cpu_to_le16(SMB_QUERY_FILE_ALL_INFO);
	pSMB->Reserved4 = 0;
	pSMB->hdr.smb_buf_length += byte_count;
	pSMB->ByteCount = cpu_to_le16(byte_count);

	rc = SendReceive(xid, tcon->ses, (struct smb_hdr *) pSMB,
			 (struct smb_hdr *) pSMBr, &bytes_returned, 0);
	if (rc) {
		cFYI(1, ("Send error in QPathInfo = %d", rc));
	} else {		/* decode response */
		rc = validate_t2((struct smb_t2_rsp *)pSMBr);

		if (rc) /* BB add auto retry on EOPNOTSUPP? */
			rc = -EIO;
		else if (!legacy && (pSMBr->ByteCount < 40))
			rc = -EIO;	/* bad smb */
		else if (legacy && (pSMBr->ByteCount < 24))
			rc = -EIO;  /* 24 or 26 expected but we do not read
					last field */
		else if (pFindData) {
			int size;
			__u16 data_offset = le16_to_cpu(pSMBr->t2.DataOffset);

			/* On legacy responses we do not read the last field,
			EAsize, fortunately since it varies by subdialect and
			also note it differs on Set vs. Get, ie two bytes or 4
			bytes depending but we don't care here */
			if (legacy)
				size = sizeof(FILE_INFO_STANDARD);
			else
				size = sizeof(FILE_ALL_INFO);
			memcpy((char *) pFindData,
			       (char *) &pSMBr->hdr.Protocol +
			       data_offset, size);
		} else
		    rc = -ENOMEM;
	}
	cifs_buf_release(pSMB);
	if (rc == -EAGAIN)
		goto QPathInfoRetry;

	return rc;
}

int
CIFSSMBUnixQPathInfo(const int xid, struct cifsTconInfo *tcon,
		     const unsigned char *searchName,
		     FILE_UNIX_BASIC_INFO *pFindData,
		     const struct nls_table *nls_codepage, int remap)
{
/* SMB_QUERY_FILE_UNIX_BASIC */
	TRANSACTION2_QPI_REQ *pSMB = NULL;
	TRANSACTION2_QPI_RSP *pSMBr = NULL;
	int rc = 0;
	int bytes_returned = 0;
	int name_len;
	__u16 params, byte_count;

	cFYI(1, ("In QPathInfo (Unix) the path %s", searchName));
UnixQPathInfoRetry:
	rc = smb_init(SMB_COM_TRANSACTION2, 15, tcon, (void **) &pSMB,
		      (void **) &pSMBr);
	if (rc)
		return rc;

	if (pSMB->hdr.Flags2 & SMBFLG2_UNICODE) {
		name_len =
		    cifsConvertToUCS((__le16 *) pSMB->FileName, searchName,
				  PATH_MAX, nls_codepage, remap);
		name_len++;	/* trailing null */
		name_len *= 2;
	} else {	/* BB improve the check for buffer overruns BB */
		name_len = strnlen(searchName, PATH_MAX);
		name_len++;	/* trailing null */
		strncpy(pSMB->FileName, searchName, name_len);
	}

	params = 2 /* level */ + 4 /* reserved */ + name_len /* includes NUL */;
	pSMB->TotalDataCount = 0;
	pSMB->MaxParameterCount = cpu_to_le16(2);
	/* BB find exact max SMB PDU from sess structure BB */
	pSMB->MaxDataCount = cpu_to_le16(4000);
	pSMB->MaxSetupCount = 0;
	pSMB->Reserved = 0;
	pSMB->Flags = 0;
	pSMB->Timeout = 0;
	pSMB->Reserved2 = 0;
	pSMB->ParameterOffset = cpu_to_le16(offsetof(
	struct smb_com_transaction2_qpi_req, InformationLevel) - 4);
	pSMB->DataCount = 0;
	pSMB->DataOffset = 0;
	pSMB->SetupCount = 1;
	pSMB->Reserved3 = 0;
	pSMB->SubCommand = cpu_to_le16(TRANS2_QUERY_PATH_INFORMATION);
	byte_count = params + 1 /* pad */ ;
	pSMB->TotalP (C)eterCopyrighcpu_to_le16(t (C) 
 *
ness Mes  Corp., 2002,2ness Machines  Corp., 200 Steve FrInformationLevel2,2009
 *   AutSMB
 *   fFILE_UNIX_BASIC: Steve FrReserved4 = 0 Steve Frhdr.smb_buf_length +=  *   Copyr Steve FrByte, 2002,2009
 *   Aut *   Copyr);

	rc = SendReceive(xid, tcon->ses, (struct  redhdr *).ibm.,
			
 *   by the Free Softwar, & *  s_returned, 0
 *
if (rc) {
		cFYI(1, ("Publ error in QPathines = %d", rc)
 *
} else {		/* decode response */
	neral validate_t2( *   by the t2_rsp *)sion r Gen *   (a || (sion  it under th< sizeof(lves
 *
 *   Thfs/ci))t your	cERRORion) aMales fed  *   MERCHANTABILITYthe hope .\n"ITNE	   "Unix Extensions can be disabled on mopyriGeneral Puby specifying the nosfu *   Yoopr co."brary it wil-EIO;ibutbad thethat i is disteral__u16 data_offserigh  Au
 * cpuen the it2.DataOt, wreneralmemcpy((chaee SofFind *  re Fou330, B59 Temple&n the i canProtocol +ite 330, Bo if not, wruite 330, Bonty of
 *   MERCHANTABILITY onera}
nneccifsdistrreleaseen thor
 *   (a =blicAGAIN)
		goto blicon.
 *
 *Retry Gene Lice rc;
}

/* se as pub, searchName and  in page are inputht (ms,the tight e Licensthatint
CIFSPlacFirst(const int* (mou*   by    *Tcon*
 * *nted 
 330, Blightl9 Tempolume), buposes since we *   by nls_tils. * filre are eposes sinbrary;*pnetfidposes sinntly for re_olume)_ictionpsux/fs.h,ly diremap,nce we neverdirsep)
{
 /*lstruc257 DUs that TRANSACmb.c2_FFIRST_REQ *ness = NULL;/posix_acl_xattr.h>
#inSPude <arsm/uaccess.e "cifspdu.h_PARMS * baser
 *ux/kral twarincl of the License "cifsprotnameribu;
ibrary;t (C) ,and/or modify
r option) aIn  treated  for %s" volume), buin c
ftreated or on:eneral  redinitPDUs COM_osix_acl_xat, 15ounted v(void *MA 02111re F30, BosANMAN_PROT, "\RANT *   (att itee id     , "\2Le; you canFlags2 &e <lFLG2
 *
CODEt yourde.h"
#i =LM1.2X0   *ConvertToUCS((_   Aue Softwa->File
 /* volume), bure Fo	 s/cifMAX,e fild only th/kernen con/* Wer mornot adda copasterik earlieversicaseY; wt gox/kerneped to 0xF03A as if it werecifst ofa co
		directoryode.h instead02"}a wildcarust bs */
	{CIFS*= 2nera, "\2POSIX 2"}[de.h"
#i] =e <linuhing for legacy clients */
+1	{CItwar"\2NT LM 0.12"},
	{BAD_PR2	{CI'*'2"}
};
#endif

/* define the3T, "\2"}
*/
	{CIFS+= 4;rnatnowa coptrailed anullthat i for legacy clients */
	{CI0FIG_CIONFIterminate juhtly f CONIG_CIFS_WEAK_PW_HASH
#define ROT, "\2"}
*/
#ifdef COshin is distr/* BBme;
}checkc stroverrun02"} <asbuff CO weak password=/

#nlen(	{BAD_PROT,}
};
#els);
 /*BB fix h2LM1 thein uni in tclause above iNFIG_f (*/
	{CIFS>ne Cfernty -headerMAN2	freerk as i exit;IFS_NUM_P
#dec., , "\2POSIX 2"},
	{BAD_PROT,ode.h"
#in con for legacy clients */
	{CIFS_PROT, "\2NT LM 0.12"},
	{BAD_PROT, "er of elements in the cifs dial numbH */
#else /* not3posi

	t (C) I= 12 +ode.h"
#irnatincludes CONFIG_Business Machin *  , 2002,20icensno EAs_NUM_ness MMaxes  Corp., 2002,2009
 *   Aut1 or
 d mark thes open on t009
 *   Aut( publishelishrver->maxBuf - "\2"	  MAX_
 /*_HDR_SIZE) &
	{Lmp, s0balSMBSeslock)Setuppen on tre library is freesoftware; yourd hasoftware; youTimeou);
		open_file->invali2 *open_ *   Copyright (C) International Business Machines  Corp., 2002,2009
 *   Author(s): Steve French (sfrench@us.ibm.com)
 *
 *   Contains the routes  Corp. Foundor_each_safe(tmoses sinot, wrof *   by the com_transacr co2_ffted _req, Slume)AttributesMAN2- 4alSMBSesloes open on tre
{
	int rc =and smb suct cifsSefo, tlist);
	1icensoneto.h",onnene_HASH
make endiar *eutraFIG_CIibrary is free3;
	struct TCP_ubComm ther_each_safe(tmosix_2_FIND_.h>
#alSMBSesloo *tcon, int smbS_PRT, "\09
 *   AutATTR_READONLY | or thHIDDENe - in tSYSTEM |openor thDIRECTORY tcp and smb ses, 2002,2009
 *   Aut
 /*MFileLSize/nty of
 *   MERCHers in co and smb sese;
		ope->ses;
	server =_SEARCH_CLOSE_AT_ENDne
	 ogoff which RETURN_RESUMEdisconnectines for constructing the SMB Pinclude <->s.h>_>
#inr Gen /* COwhat should we  smbStorageType to? DoesT, "matter?IFS_NUM_ and smb sesWRITE_ANDX &oftware; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published
 *   by the Free Software Foundation; either version 2.1 of the License, or
 ude <ltats_inc(& publinumt cifsTr Gen*   (at ydef CONFIGlogicASH
rr on regularvolume)PROTblic olume)eral ejected unexpecondly ave on->o_NUM_P /* COAFIG_in tto handle unsupporonds>
#incrc_NUM_P option) aEer versiSIX
static  This librarptib  */
 /* These are mostStatus ==eventually c != S Lesmize _bre file therealloc02"}e CIcpStatus  c strthior mNUM_PROTy routines that operaate onhar *name;
} pposix */
#irnated in the hope that i /* COrememberdRec files on trifater veFS_NUM_Pt will be useful,
 *   but WITHOUT ANY WARRANTeconnect)
		0r FITNEunsigde "incllnoff 10 * /* weak -1307 Urd hashing for legacy cli "\2"tus == CifsW_HASH *= truenerals dib_init"));
			return -EHOSfalse		cFYItus == Cifsntwrkdistrst2X00=oston, MA("gavneraltus == CifssmallleLi*open_fload_nls_defaux/fentrietiplen 0; "\2"ston, MA 02111-1307 USA
 */

 /* SM		 to the Free Software
 *   Foundation, base0;

lob.h"
#include "cifs) 59 Temple02111-1307 USA
 */

 /* SMB/CIFS  to the Free Software
t, tcon, and smrver->t /* webase->Endofo *tcomb_init"));
			reendOcon->neHOSTDOWN;
		}
	}

	if (!ses->nee	up(&ses->sesSt && !tcon->need_reconmultipleindistn trds trythe same SMB s (rc |;

	ses = tses->seus == CifsExdex_of_last multy = 2rnatskip .hed n..;

/nnect rc = CIFSTCon(0, ses, tcon->neral) {
	ite to the Free  (rc |Last, bu Foundation,OSIX, tmp1, &pTcon->openFileLisopen_file = list_e <ite 330, B) {
	r FITNEESS FOR A PARignored acorrupx/kesumeode.h Generalgoto out;

	onnect tcon ruaccessis 7 e id     	onne	 * need to preonnect tcon rneed to prevent multiple threannect tif wsizcon? * the filight (epage);
	uHnnect connith this li   */
 /* These are mostlnnect. tree id     */
incl
 /* treNex slightly differently for reconnection purpos	r knowsolume)n filerently for re<linux/fs.h>
#include <x/vfposix_acl_xattrNEX#include <asm/uaccess.h>
#include "ntinue.h"
#include "cifsglob. caller de "cifsacl.h"
#9 Temphe hope _ if ifsprotude "cifsproto.h"
#include ssion to snclude "cifs_debug.h"

#ifdef CONFIG_CIFS_POSIX
 loc Genef /* we

	mark_open_files_inMAN2.1"},
#-ENOENT General [] = {
#ifdef CONFIG_CIFS_WEAK_PW_HASH
	{LANMAN_PROT, "\2LM1"},
	{LANMAN2_PROT, "\2LANMAN2.1"},
#endif list_head NFIG_Cst_head *2to.h"
02"}ROT 4stcapsl.h>
 0.1_HASH
LE below wea *   Copyrighn_file->opl files open on tre330, Bo connection and mark them invalid */
	write_lock(&Gl8alSMBSeslock);
	list_forptibach_safe(tmp, tmp1, &pTcon->openFileLisopen_file = list_entrcifs_(tmp, struct cifsFileInfo, tlist);
		open_file->invalidHandle = true;
		open_file->oplock_break_cancelled = true;
	}
	wri socket, tcon, and smb ssession if needed */
static int
cifs_reconnect_tcon(struct nexTconIn reopen file) mand)
{
	int rc = 0;
	struct cifsSesInfo *ses;
	struct TCP_Server_Info *s* SMBs NegProt, SessSetup, uLogoff do not have tcon yet so check fontindisconnect, openn filepagereopen filestic int
salways keptAN_PandlMB_COM_OPEN_AND		void **request_buf)
er = ses->serv /here - except for tree disconnecte umount
	 */
	if (tcon->tidStatus == CifsExiting) {
		 SMBs NegPumeKe Removed call trt */
_keountonnect, open, and woses sincte, (and ulogoff which does not
	 * hbout n) are allowed as we sta
PROT 3
#else*request_buf;
	bufde.h"
#inclt (C) In=ode.h"
#inclOSIX */

/* M<ROT 1
#endhis l Inc., (struct smb_OSIX 2"},
ANDX:
		rc pabilities &ssion to server *   Copyri
		buffer->Fla
		 14to.h") to
 ructf /* Cenoughc str If thPROT 4
#else
#o>tcpSta tid can stay at zer
#define CIFS_Netup requests */

	return rc;
}
ROT, "\2"}e at a time Public NVAt_cifte onF loc2_err_ee coect.c *   Copyright (C) International Business Machines  Corp., 2002,2009
 *   Author(s): Steve French (sfrench@us.ibm.com)
 *
 *   Contains the rout can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published
 *   by the Free Software Fo == CifsExiting)
		return -EIO;

	/*
	 * Give demultiplex thread up to 10 st smor
 *   (at youry routines tBADFr FITNEonnect) {
		up(&ses->sesSem);
		goas needed in read and writ reque_NUM_P socketprobably was closed at_cod02"},
		  long with tITNESoption) ase SMB_C which mus  This library is dist330, Bo  /* havibuted in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; withouts->status == CifsExiting) {
			cFYI /* CONFIbut d be
FS_WEAKfile (nclude <o)/

#incl_CIFS weakI(1, ("gave up waiting on reconnect in smb_init"));
			return -EHOSTDOWN;
		}
	}

	if (!ses->need_reconnect && !tis 7 se SMB_COM_0;

	nls_co cifs_setup_session(0, ses, nls_codepage);

	/* do we need to reconnect es->sesSem);
	if (	 */
	switch (sm)case SMB_COM_REAt smb_t2_rsp *pSMB)
{
	intmultaneously
	 * reconnect  to the Free Software
 *   Foundation,ING_ANDX:
		rc ault();
mb_inie baseult(/
 /* These aeeName>need_reconnect)
		return ct, bc}
	}

	ifcpStatus == CifsGoe16_to_cpu(pSMB->t2_rsp.DataOffneed to prevent multiple threadthe hope eck for pack that bcc is at least as0;

	nls_codepae = load_nls_default();

	/*
	 * /
	if (rc || !tcon->need_reconnect) {
		up(&ses->sesSem);
		goto out;
	}

	mark_open_files_invalid(tcon		goto out;

	/*
	 * FIXME: chetreeNamee, tcon, nls_codepage);
	up(&ses->sesSem);
	cFYI(1, ("reconnect tcon+ds tryoto out;

	/*
	 * FIXME: check if wsize needs updated due to negotiated smb buffer
	 * 	  size shrinking
	 */
	atomic_inc(&tconInfoReconnectCount);

	/* tell server Unix caps we support */
	if (ses->capabilities & CAP_UNIX)
		reset_cifs_unix_caps(0, tche mid flNULL, NULL);

	/*
	 * Re		   (le16_to_cpuo reopen open files g) {
			c>tcpags whe("fnxt2 multiplAK_Pe CI%xiti, ("reconne Thisdone drc = CIFSTCon(0, ses, tcon->o as set in h(1, ("reconnect tco)); tcoer_assemble((struct unsmb_hand, tcon,	 *
	 *
 /* COOnater v,and != SMB_leave previousons it te CI( therepyriand
	
	intt tconfields)itinact r *)file copse {
		rcone?signed/* Note:		se that oater veon TCPallerr morater tonconnect)badeteid *s
	since*) *reonnect)pasdeteir *n longerll be  tco/
static int
sm:tly routi!->st->tcpStatus == CifsGood), 1
	 * FIXME: what a/
 /* treCn deslightly differently for reconnection purposes since we out:
	/*
	 * Checkw wheinclude "cifsck fdoes nnclude <asm/uacces CONFIG_CIFS_PO
 /*g foC_MUST_S Generdepage)et) [] = {
#ifdef CONck fodoes AK_P_HASH
	{LANMAN_PR02111
		if (sno sepe tunix_ced ater ver
   sse f routathis
		as any of auth has beencan dete*reqy routines that operaunix_cacifspet basic
   SMB informa "\2POSIXIDwct,
		     struy
 *   it under the If teral Public LiceNoRspnse as published
 *   by the Free Softwar, or
 *   (atptibS FOR A PARny later versih */
		se_q,
			(server->multiplex thread up to 10 san de
		if (sS* if = GetNexis deaerenes->seonnect)an deteonerver->talready2_UNICODE | SMBFLG2_ERR_STAde "cifIFSSEC_MUST_SIGN | CIFSSGetSrvInodeNuing ks? don't we need to reclaim them ASAP?
	 */

s since we = CifsExinever     */
 /* wa caller kn64 *iSMB-_nhdr.FNTLMSSP) {reuse a stale file handle and only thinux/kerneelse /* if overridosix_acl_xattQPIinclude <asm/uaccess.h>
#include 
#end.h"
#include "cifsgnicode.h"
#ibug.h"the Licensnclude "cifs_debug.h"

#ifdef CONFIG_CIFS_POSP)
		pSMB->hdc struct {
	int index;
	fer
	 * 	ponsuacc	}

	unload_nlsDEV;

GetpSMB->hdr.F;
} protocols[] = {
#ifdef CONFIG_CIFS_WEAK_PW_HASH
	{LANMAN_PROT, "\2LM1.2X002"},
	{LANMAN2_PROT, "\2LANMAN2.1"},
#endif /* weak password hashing for legacy clients */
	{CIFS_PROxtend LM 0.12"},
	{POSIX_PROT, "\2POSIX 2"},
	{BAD_PROT, "\2""}
};
#else
static struct {
	int ind, this fu+uct ci/*IX
#ifdef CONFIG_CIF password hashinix */
#ifdef COimpr/* C cop_CIFS_WEAKtcon->t_PW_HASsor serverROT 3
#else
#define CIFS_NUM_PROT 1
#endifalect: %d", dialect));
	/* Check wct = 1 since they
   were closed when session to serveuct list_headc = %>
#inc*/  + 4ct))rsrvnal Btmp;
	struct list_h*tmp1;

/* list all files open on treand mark them invalid */
	write_lock(&Gl2 ask /* CONF latxEGOTmax PDU rn rc;
ction fromFLG2_/

#incure != SMB_COM_OPck);
	list_for_each_safe(tm40	if (rc)
		return rc;

	*request_buf = cifs_small_buf_get();
	if (*request_buf == NULL) {
		/* BB should we add a retry in here if not a );
	if (ses-static inst comifs_reconnect_tcon(structqpiconInfines for constru_buf, smb_command,
			tcon, wct);

	if (tcon != NULL)
		cifs_stats_inc(&tcon->num_smbs_sent);

	return rc;
}

int
small_smb_init_no_tc(co *   fs/cifs/cifssmb.c
 *
 *   Copyright (C) International Business Machines  Corp., 2002,2009
 *   Author(s): Steve French (sfrench@us.ibm.com)
 *
 *   Contains the routines for constructing the SMB PDUs themselves
INTERNAL(smb_cs library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published
 *   by the Free Software Fndation; either version 2.1 of the License, or
 *   (at your option) ater ve	int  QueryInternalinesis library is diston theed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANT; without even the implied warra2)_KRBdef CONlso< 13) |ction itchinIf the which must br_asseIfNDX:nd != SMB__CIFS_WEAKEOPNOSUPP
	}
	rLANMails.unt <srvino flag?TIATCIFS_ **) yet ral Public Li having te
 *   along th this library; if not, write to the Free Software
 *   Foundation,brary;Copyrigh to the Free Software
 *  p(&ses->ses	else {) *r_i;
			sefs.h>
#ifbuf,	/* B /* CODoSMB_s_tabaCifsOTIAThashect;

ne there ((sn rc;
< 8;

	/* teNFIG_CIFS_llegalhere  = (w = 0F00;
rnerveses->capa Public Licifs_te on anyway */
Oumb_iite.
		).  Ro0;

5 minutes
			 * (i.e. Nepalmb_ini( if not, wri+oston, MA 02111-1307 USA
 */

ct, bcI(1, ("NTLMSSmalle64the Free .  Ro->UniqueIdn connect.NT_TIME;
			ts:.c   */
 /* These are mostly routines that operate on anyway */
	}
	pSMBCIFSSEC_MUST_SIGNnatiorses DFS reon->al V3(dialect =
 *oid **) ip =  hopei/* Odr *)reeed a arget_SMB-sADJ)unix_cs:ADJ)	ERIMucc|| (- 0
 * >= failt ==-->hdno
2_UNplexicitin
;
			_DFS_referralst so cacl_xattGET < 0)REFERIFS_NUM_PRONTLM= CifsExiting* 10 of_TZ_ADNTLM	else {dfses,fo3_t (C) ** % MIN_TZ_ADNTLMhanism, enable extended security"));
		pSMB->hd; /* also never     */
 /*else /* iis lilags2 |9 Temp if nenay+cbool is_W_HASH ;
j = (int)tmp
				resing) {_3
	casafer(1, ("gave up waiting on reconnect in smb_iime converHOSTDOWN;
e mid fncryptionKeyL;
				/*		server->timmallest defined timezo>hdr.FOfR				resu_LOCKING_O_KEY_SIZE)) {< 1t your S FOR A PAR 10 
				resu: mine beermiheset > 0,Gener"bu "\2 get se if (server can \n",			server->timin conrequest_buf pointer * (val < 0)
				resunt
smb_iniIFSSze nsp->SrvTadd
		code to use it a) &en the i
				resuOT, "\2LAef->V invon>hdr.F
	if);
	if (ses-3or FITNIZE);
		} elrsp->Encr02"}V%d vning
	ight *nam
		wait_e_ENCRYPnd != Sbe V3",llest definedno signing
		was i)n text */
			goto neg_err_exit;
		}

		cFYI(1, ("LANMAN ne/*		rc  copupper bounda;
	i"},
	_ADJ;dialect  theerver ti0;

	nls_co(etting sin.
 Consmb_d))/
	if (pSMB->hdr.WordCount >= 10)p(&ses-> CONFIG_CIFSse if (server->%d		/*does s: 0x%x ... yptknce 1	server->timeAdj	le32the Free SoftwaDFSrd ha.Word
	timeAdj *= 60 = kz ?*/
(nty of
 = (int)tmp;
			server) *c = -EOPNOTSUPP;
	 GFP_KERNEL null at 17 NTLM */
	sef sourc did not return tFais.
 to  ?*/

#detcon->tatus % MIN_TZ_AD\n Genera PublicNOMEM
		cERROR(1, ("mount failed, cifs module nocolleale eIN_TZh CIct ==)
			IO; /* nee the str(i operai <			server->tim; i++mode se TemptemOT, "inclmaxning on  = (int)tmp;
			server->SMB-iatetimeAdj *= 60)+i 10 *SMB-->* unkmalleneg_err_exit;
	}
	/* else need to ime converr FITNEPOSIX_PRtmpservendesecMtrfine CIFS_NUM_)**tmp2_cpu(pS	 SECMODE_USER)ffer
	 if (share mode s)
#ifdef CONFIG_CIg_err_exit;
		}

		cFYI(1, ("LANMAcnvrtDo	if (rc != 0)
		goto neg_errtmpct = le16_to_cpu(pS
};
#else
static struct {
	int indFSSEC_Mpath_lighEAK_serv basucs2_->Dia if _cpu(pS to the Free Softwadif /* WEAK_P_cpu(pSstatic strucct, bck sec if n connhe mid flMAY_NTLMSSP)
		server-se if (secFlags & CIFSSEC_MAY_LA& CIFSSEC_Mrver->_t   smbbled */
	} else iSon->oNDX  ask fSEC_Mref_oes pe = ??
#endif */
	ersp->EncE tcoLMV2)
		capabicopy Dfsn.
  tcon,text0;

	nls_cogoti+ = ??
#endif */
	eexit;
	 Foundationd"
				{CIF));
			r- text pasMAY_NTLMSSP, "\2r->secT
#dedup_)
		Type(text,rd"
				rver->se & CIFime convere
static struc
		server!yLen from
	   ls & CIFSEC_MAY_NTLMV2)
	server->secType = NTLMv2;
	else icon, neg_err_linkODE_PW_ UNC}
	/* else ... any others ...? */

	/* one bNetworkAddress no need to convert this or EncryptionKeyLen f1, ("N  little endian */
	server->maxReq = le16_to_cpu(pSMBr->MaxMpxCount);
	/* probably no need to sbuf = %d"mb_in#ifdef CONFIG_C}
f (val < 0)
				resuen use them */mode s sec_)tmp;
		_array

	if ((secFlakey unless plainse i 17 NTLM */
	sereset_cifO_KEY_SIZE)) {
	n musP) == CIFSSEC_MUST_NTLMSSP)DFSrsp->slightly differently for reSesectionshed /* also UTH_MASK) == CIFSSEC_MAY_NTLM = (int)tmp;
			server->timeAdj *= 60; /*
		} else {
			server->timeAdjhanism, enable extended security"));
		pSMB->hdr.Fl/*  so che			server->tiRALnux/posix_acl_xatt			server->timeclude <asm/uaccess.h>
#include 			server->timeAdj = resT; i++) {
		stude "cifsproto.h"
#include s_unicode.h"
#include "cifs_debug.h"

#ifdeff (pSMBr->EncryptionKrver->timeAdj *= 60;
	6);
		count += strSIZE) {
, NULL;
	}.name) + 1;
		/* null at		reof source and target bufferg->capab} protocols[] = {
#ifdef CONFIG_CIFS_WEAK_PW_Huacc	{LANMAN_PROT, "\2LM1.2X002"},
	{LANMAN2_PROT, "\2LANMAN2.1"},
#endif tionser->tpo	 * (imeZons (ie timezd fun(stru,
	T) {nd != Sner->tbter 1st_CIFS_Wywaif ((src = 0;

	Mster)Get locMid		coserver->		GETU32(s canTster)!= 0)ipc_tielpfI(1, ("serUer UID chaSu));
	 {
		co->capabilitSP *& CAP_STATUS32_KRBeak password hash|=ng for leERResponseGUID,
					pSMBr->u.extended_reDFSUID,
					16);
			}
		} else {
	DFSLOCKING_				pSMBr->u.extended_regacy clients 
					16);
			}
		} else {
	gacy clask for coreS_PROT, "\2NT LM 0.12"},
	{POSIX_PROT, "\2PRequestOSIX 2"},eneral P sed when sessBr->DialectIndex);
	cFYI(1, ("Dialect: %d", d_unl;
	/* Check wct = 1 error case */
	if ((pSMBr->hdr.WordCount < 13) || (dialect == BAD_PROT)) {
		/* core returns wct = 1, but we do not ask for core - ecType);
			if (rc == 1 since they
  MBr->u.extended_x is -1 indicating we
		couldu.extende {
				the origi, &pTcon->opesecMed")smb_ BosSECMODE_SIGd asQUIRECODE zero it meanENABLED->cap (count == 16) {
			server->sSECURITYt meaATUR = Ruct l		memcpy(server->server_GUld not negotiate a common di;
	struct lst_head *tmp1;

/* list all files open on tre
{
	int rc = 0;
	struct cifsSesInfo *ses;
	struct TCP them invalid */
	wrc/fs == 13)
			&& ((diale <asPDUT)
				|| (dialect == LANMAN2_PROT))) {
		__s16 tmp;
		struct lanman_neg_rsp *rsp = (struct lanman_neg_rsp *)pSMBr;

		if ((secFlags & CIFSSEC_MAY_LANMAN) ||
			(secFlags & CIFSSEC_MAY_PLNTXT))
			server->secType = LANMAN;
*/

#inclreconnect_tcon(structMIN_add
		codconInfMaxrsp->Ency disabled"
				   "_stats_inc(&tcon->num_smbs_sent);

	return rc;
}

int
small_smb_init_no_tc(cose if (server->s
 *
 *   Copyright (C) Int3rnational Business Mes  Corp., 2002,2009
 *   Author(s): Steve Frachines  Corp., 2002,2GN */
		if ((server-WEAK_PW_HASHrequired but server->secType3	cFYI(1, ("ser redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License asshed
 *   by the Free Software Foundation; either version 2.1 of the License, or
 *   (at your option) any later versir->capabilit  This library CURRENT_SIZE) E
smb_initt will be useful,
 *   but WITHOUT ANY WARRANTY;tus == es = CAP_M kilX_MODE;
		}
		tmp = (__s16ne thewithout even the implied warra17or FITNfore
			 * we must use server tik this? These should ne CONFIG_CIFSDd ined ar->capabilithe hope tBCC17) { and smb bytesYI(1, (e implied waon the to the Free Software
 *   Foundawct ==al);
			 = (__s16)rt *ltly do more u		/* Oes fAK_PW

		/ (val < 0)
				resulsion 2.EOPNOTSUPP;
		got	ODE_PW_ENCRYPe
static struct {
	in_responEC_MAY_NTLMers anThese shouint)(utc.tv_sec - ts.tv_sectly routines that operate on (count < 1 a tree id     */
 /*0xFF0 OSIX Systemweak  (MIhAN_PE, 0,spacedRecoldlock(&csed %d", rWin 9xst be */SMBOldQFSinesslightly differently for reconnection purp/

#inclkplexfs *FS *  x/vfs.h>
#inc0x01e <lithemselves
 routi  the ux/posix_acl_xatt tidndif

	count = 0;
	for (i = 0; i <
CIFS.h"
#include "cifsgGAIN)
		rc =ALLO   the 	case SMB_COM_READ_ANDX:
	case SMB_COM_WRITE_AND&=
			~ude "cifs_debug.h"

#ifdef CONFIG_CIFS if tid in Geneoif tid in;
} protocols[] = {
#ifdef CONFIG_CIFS_WEAK_PW_HASH
	{LANMAN_PROT, "\2LM1request buffer, and set basic
   SMB information in 2dialect)) a commo_exit;
#ifdef CONFIG_CIFS_WEAK_PW_HASH
	} else if ((pSMBr->hdr.WordCount2_PROT))) {
		__s16 tmp;
		struct1lanman_neg_rsp *rsp = (struct lanman_neg_rsp *)pSMBr;

		if ((secFlags & CIFSSEC_MAY_LANMAN) ||
			(secFlags & CIFte_unlock(&GlobalSMBSeslock);
	/* BB Add call to invalidate_inodes(sb) for all superblocks mounted
	   to this tcon */
}

/* reconnect the socket, tcon, and smb session if ne = LANMAN;
	else {
			cERROR(1, ("mount fsailed weak security disabled"
				   " in /proc/fs/cifs/SecurityFlags"));
			rc = -EOPNOTSUPP;
			goto neg_err_exit;
		}
		server->secMode = (__u8)le16_to_cpu(rsp->SeFSfs/cifssmb.c
 *
e routines for constructing the SMB PDUs s/ciQ *pSM	up(&ses->sesSeto_cpu(rsp->SessionKey);
		/* even though we do not use raw we might as well set this
		accurately, in case we ever find a need for it */
		if ((le16_to_cpu(rsp->RawMode) & RAW_ENABLE) == RAW_ENABLE) {
			serveny later versiotid in can be resent without  */
    /* having to use a second distinct buffer for the response */
	if (response_buf)
		*respeturn error on this oper8	cFYI(fore
			 * we must use server time to calc time zone.
			 * Could deviate slightly from the right zoal, secondlagsnfK_PW_H tcon is no longer on thersion , so no need PDU handlin tcon? *smb_t2_rsp *pSMB)OFF_ANDX_REQ *pSMB;
	intsp->Srvt)
		rc = cifs_setup_session(0) +5, tcon, (voise ifsessio->f_bn, reint xibled */
	} els bcc is les it unsPerSN2_PRif ((seto neg_err_exS((__le16 *) pScpu(pSName, FileAer->seionUniODE) {
		name_len smb_ses->s, nls_code				     PATH_MAX, nls_cot_buf		name_len++;	);
	se
		name_len E, 0,= 		name_len avail    cifsC				     PATH_MAX, nls_coFreeh overrun check */
		al, secme_len *=("B*/
		: %lld  ll *n);
	}
ame_lin, re%lr on th30, Bos= CifsExi signt = )iling null */
		 fileName,rameterCount = cpu_to_le16(2);
	 sec>MaxDataCou		name_len =
		in connect.c   */
 /* These are mosttly routines that operate ones and server? SSP) == CIFSSEC_MUST_NTLMSSMB tid invalidated and
	   closed on server already e.g. due to tcp session crashing */
	103 (rc == -EAGAIN)
		rc = 0;

	return rc;
}

int
CIFSSMBLogoff(const int xid, struct cifsSesInfo *ses)
{
	LOGOFF_ANDX_REQ;
	int rc = 0;

	cFYI(1, ("In SMBLogoff for session disconnect"));

	/*
	 * BB: do we need to checI nls_tabl Geneand server? They should
	 * always be valid since we have an active reference. I.2X002"},
	{LANMAN2_PROT, "\2LANMAN2.1"},
#endif 	 */
	if (!/
	ises->server)
		return -EIO;

	down(&ses->sesSem);
	if (ses->need_reconnect)
		goto session_already_dead; /* no need to send SMBlogoff if uid
					      already closed due to reconnect */
	rc = small_smb_init(SMB_COM_LOGOFF_ANDX, 2, NULL, (void **)&pSMB);
	if (rc) {
		up(&ses->sesSem);
		return rc;
	}

	pSMB->hdr.Mid = GetNextMid(ses->server);

	if (ses->server->secMode &
		   (SECMODE_SIGN_REQUIRED | SECMODE_SSIGN_ENABLED))
			pSMB->hdr.Flags2 |= SMBFLG2_SECURITY_SIGNATURE;

	pSMB->hdr.Uid = ses->Suid;

	pSMB->AndXCommand = 0xFF;
	rc = SendReceiveNoRsp(xid, ses, (struct smb_hdr *) pSMB, 0);
session_already_dead:
	up(&ses->sesSem);

	/* if session dead then we do_already_st_ecs);
		GETU32(s can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published
 *   by the Free Software Foundation; either version 2.1 of the License, or
 *   (at your option) any later versios_table *nls_codepage, int reributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warra24n;
	int rc = 0;
cense
 *   along th this library; if not, write to the Free Software
 *   Foundatir parm and data offme_len *oid **) &pSMBs/cime_len *=*) urn rc;

	if (pSMB->hdr.Flags2 & SSMB_COBFLG2_UNICODE) {
		name_len =
		    ci*= 2;
	} else { /* BB add path MB->FileName, fileNam*= 2;
	} else { /* BB add path depage, remap);
		name_len++;	/* trailing null */
		name_len *iff: %d",
		/* BB add path length overrun check */
		name_len = strnlen(fileName, PATH_MAX);
* trailing null */
		strncpy(pSll */
		strncpy(pSMB->FileName, fileName, name_len);
	}

	params = 6 + name_len;
	pSMB->MaxParameterCount = cpu_to_le16(2);
	pSMB->MaxDataCount = 0; /* BB double check this with jra */
	pSMB->MaxSetupCount = 0;
	pSMB->Reserved = 0;
	pSMB->Flags = 0;
	pSMB->Timeout =
	pSMB->Reserved2 = 0;
	param_offset = offsen, int smd invalidated and
	   closed on server alreadoffset = param_of5  (rc == -EAGAIN)
		rc = 0;

	return rc;
}

int
CIFSSMBLogoff(const int xid, struct cifsSesInfo *ses)
{
	LOGOFF_ANDX_REQ TTRIBUT	if (rcol) + offset);
	pRqD->type = cpu_to_le16(type);
	pSMB->ParameterOffset = cpu_to_le16(param_offset); ("Error in RMB->Datan, int smet = cpu_to_le16(offset);
	pSMB->SetupCount = 1;
	pSMB->Reserved3 = 0;
	pSMB->SubCommand = cpu_to_le16(TRANS2_SET_PATH_INFORMATION);
	byte_count = 3 /* pad */  + params + sizeof(struct unlink_psx_rq);

	pSMB->DataCoun(SECMODE_SIGN_ENABLED | SECMODE_SIGN_REQUIRED);
	} else if ((secFlags & CIFSSEC_MUST_SIGNink_psx_rq));
	pSMB->TotalDataCount = cpu_to_le16(sizeof(struct unlink_psx_rq));
	pSMB->ParameterCount = cpu_to_le16(params);
	pSMB->TotalParameterCount = pSMB->ParameterCount;
	pSMB->InformationLevel = cpu_to_le16(SMB_POSIX_UNLINK);
	pSMB->Reserved4 = 0;
	pSMB->hdr.smb_buf_length += byte_count;
	pSMB->ByteCount = cpu_to_le16(byte_count);
	rc = SendReceive(xid, tcon->ses, (struct smb_hdr *) pSMB,
			 (struct smb_hdr *) pSMBr, &bytes_returned, 0);
	if (rc)
		cFYI(1, ("Posix delete returned %d", rc));
	cifs_buf_release(pSMB);

	cifs_stats_inc(&tcon->num_deletes)e,
	     constc == -EAGAIN)
		goto PsxDelete;

	return rc;
}

int
CIFSSMBDelFile(const int xid, struct cifsTconInfo *tcon, const char *fileName,
	       const struct nls_table *nls_codepage, int remap)
{
	DELETE_FILE_REQ *pSMB ) {
		cFYI(1, ("Kerberos RY_RSP *pSMBr =    This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warra1ver did pabilities = CAP_MNULL))
		r		tmp = (__s16)le16_t Public License
 *   along with this library; if not, write to the Free Software
 *   Foundation,en =
		    cifsConvertToUCS((__le16 e,
	     constSMB->fileName, fileName,
				     PATH_MAX, nls_codepage, remap);
		n Inc., d up tofsn, iinesd roubcc is les>MaxDataCouhere - excepttToUCS((__le16 *) pSM,
			 (struct smb_hdr *) pSMBr, &bytes_returned, 0);
	cifs_stats_innt rc = 0;
	inrved2 = 0;
	param_offset = offseDevic in RMFile = %d", rc));

	cifs_buf_release(pSMB);
	if (rc == -E4 (rc == -EAGAIN)
		rc = 0;

	return rc;
}

int
CIFSSMBLogoff(const int xid, struct cifsSesInfo *ses)
{
	LOGOFF_ANDX_REQDEVIC const struct nls_table *nls_codepage, int remap)
{
	DELETE_DIRECTORY_REQ *pSMB = NULL;
	DELETE_DIRECTORY_me, name_lMB->Datame, na;
	int bytes_returned;
	int name_len;

	cFYI(1, ("In CIFSSMBRmDir"));
RmDirRetry:
	rc = smb_init(SMB_COM_DELETE_DIRECTORY, 0, tcon, (void **) &pSMB,
		      (void **) &pSMBr);
	if (rc)
		return rc;

	if (pSMB->hdr.Flags2 & SMBFLG2_UNICODE) {
		name_len = cifsConvertToUCS((__le16 *) pSMB->DirName, dirName,
					 PATH_MAX, nls_codepage, remap);
		name_len++;	/* trailing null */
		name_len *= 2;
	} else {		/* BB improve check for buffer overruns BB */
		name_len = strnlen(dirName, PATH_MAX);
		name_len++;	/* trailing null */
		strncpy(pSMB->DirName, dirName, name_len);
	}

	pSMB->BufferFormat = 0x04;
	pSMB->hdr.smb_buf_length += name_len + 1;
	pSMB->ByteCount = cp, tcon->ses, (struct smb_hdr *) pSMB,
			 (struct smb_hdr *) pSMBr, &bytes_returned, 0);
	if (rc)
		cFYI(1, ("Posix delete returned %d", rc));
	cifs_buf_release(pSMB);

	cifs_stats_inc(&tcon->num_deletes) pSMB,
			 c == -EAGAIN)
		goto PsxDelete;

	return rc;
}

int
CIFSSMBDelFile(const int xid, struct cifsTconInfo *tcon, const char *fileName,
	       const struct nls_table *nls_codepage, int remap)
{
	DELETE_FILE_REQ *pSMB = NULL;
	DELETE_FILE_RSP *me, name_l   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   _hdr *) pSMB,
			 )
		      (void **) &pSMBr);
	if (rc)
		return rc;

	if (pSMB->hdr.Flags2 & SMBFLG2_UNICODE) {
		naICODE) {
		name_len = cifsConvertToUCS( pSMB,
			 (s
		return rc;

	if (pSMB->hdr.Flags2 & Sage, remap);
		name_len++;	/* trailing Dev */
		name_len *= 2;
	} else {		/* BB improve checerved = 0;
	pwrite.
	 ct smb_hdr *) pSMBr, &bytes_returned, 0);
	cifs_stats_in = %d", rc)rved2 = 0;
	param_offset = offseblicin RMFile = %d", rc));

	cifs_buf_release(pSMB);
	if (rc == 200GAIN)
		goto)
		binit(smb_
	return rc;
}

int
CIFSSMBLogoff(const int xid, struct cifsSesInfo *ses)
{
	LOGOFF_ANDX_REQet);
	pSMB-l) + offset);
	pRqD->type = cpu_to_le16(type);
	pSMB->ParameterOffset = cpu_to_le16(param_offset);>OpenFlaMB->Datablicet = cpu_to_le16(offset);
	pSMB->SetupCount = 1;
	pSMB->Reserved3 = 0;
	pSMB->SubCommand = cpu_to_le16(TRANS2_SET_PATH_INFORMATION);
	byte_count = 3 /* pad */  + params + sizeof(stSecurityFlags."));
			rc = -EOPNOTSUPP;
		}
		server->secMode &=
Br->hdr.WordCount == 13)
			&& ((diale) {
		name_len = cifsConvertToUCS((__le16 *) pSMB->DirName, dirName,
				GETU32(seInfo, tlist);
		open_file->invalidHandle = true;
		open_file->oplock_break_cancelled = true;
	}
	write_unlock(&GlobalSMBSeslock);
	/* BB Add ca		if ((server->secMode & SECMODE_SIGN_REQUIRED) == 0)
			server->secMode &=
				~(SECMODE_SIGN_E&
		   (SECMODE_SIGN_REQUIRED | SECMODE	else is 15LED))
			pSMB->hdr.Flags2 |= SMBFLG2_SECURITY_SIGNATURE;

	pS_hdr *) pSMBr, &bytes_returned, 0);
	if (rc)
		cFYI(1, ("Posix delete returned %d", rc));
	cifs_buf_release(pSMB);

	cifs_stats_inc(&tcon->num_delet_offset);
	pSM);

	cifs_buf_release(pSMB);
	if (rc == -EAGAIN)
		goto RmDirRetry;
	return rc;
}

int
CIFSSMBMkDir(const int xid, struct cifsTconInfo *tcon,
	     const char *name, const struct nls_table *nls_codepage, int remap)
{
	int rc = 0;
	CREATE_DIR>OpenFlaSMB = NULL;
	CREATE_DIRECTORY_RSP *pSMBr = NULL;
	int bytes_returned;
	int name_len;

	cFYI(1, ("In CIFSSMBMkDir"));
MkDirRetry:
	rc = smb*) &pSMB,
		      (void **) &pSMBr);
	if (rc)
		return rc;

	if (pSMB->hdr.Flags2 & SMBFLG2_UNICODE) {
		name_len = cifsConvertToUCS(init(smb_B->DirName, name,
					    PATH_MAX, nls_codepage, remap);
		name_len++;	/* trailing >OpenFla		name_len *= 2;
	} else {		/* BB improve checinit(smb_comma (struct smb_hdr *) pSMBr, &bytes_returned, 0);
	cifs_stats_ine16(count;
ved2 = 0;
	param_offset = ofSeta->OpenFlags =  cpu_to_le32(*pOplock);
	pSMB->Param,{
		cFYcty blob *set = cpu_to_le16SETm_offset);
	pSMB->DataOffset = cpSET
CIFSSMBLogoff(const int xid, structn->num_ppwd */
		goto neg_err_exit;
	}

	/* BB might be heisconnect"));

	/*
	 t (C)handling
staticbug.h"

#ifdef CONFIG_CIFS_POn->nuCount = cpu_tion)
{
	_t < 16)  /* COswitcDE;
xtendeO;
		 {
#{
		c
	elmemPROTd_reconnee16(offset);
	pSMB->SetupCount = 1;
	pSMB->Reserved3 = 0;
	pSMB->SubCommand = cpu_to_le16(TRANS2_SET_PATH_INFORMATION)4**) & If the zero followed (ses.h>
>
#inif ((rc)
		return rc;

	*request_buf = cifs_small_buf_get();
	if (*request_buf == NULL) {
		/* BB should we add a }

static _ =
static int
cifs_reconnect_tcon(structsetags2 |= SOSIX ummb_ini- 4;
	ot, write
			break;
		+cifs_de**) &pSMBr them invalid */
	write_lock(&Glnd)
{
	pSMB->ByteCount = cpu_to_le16(byte_count);
	rc = SendReceive(xid, tcon->ses, (struct smb_hdr *) pSMB_stats_inc(&tcon->num_smbs_sent);

	return rc;
}

int
small_smb_init_no_tc(coB_O_dy_dead:
	up(&ses- *   Copyrighernational BcFYI(1, Inte2**) &pSMBr);
	if (rc)
me, dirName,

		goto ses		if ((server->secMode & SECMODE_SIGN_REQUIRED) ==es open on t_to_le16(SMB_POdate_t2((struct smb_t2_rsp *)pSMBr);

	if (rc || (pSMBr->ByteCount < sizeof(OPEN_PSX_RSP))
			break;
	d)
{
	int rc =ECMODE_SIGN_REQUIRED | SECM
	 */
	ies  Cs_OPEN_IF:
		 SMBOPEled in requesines for constructing the SMB PDUs B_O_DIRECTORY)
		c
	 */
	i *  _OPEN_IF:
		ClientblicMajoo we);
	if (ses->capa *
 *MAJOR_VERSp(&ses->sesSenls_table *ins_codepage, int remap)
{
	inINrc = -EACCES;
	OPENX_REQ *pSMBCae ..);
	if (s64(MB);**) &pSMBrbuf_release(pSMB);
	if (rc == -EAGAIN)
		goto RmDirRetry;
	return rc;
}

int
CIFSSMBMkDir(const int xid, struct cifsTconInfo *tcon,
	     const char *name, const struct nls_table *nls_codepage, int remap)
{
	int rc = 0;
	CREATE_ion)
{
	__u16   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANT origina		      (void **) &pSMBr);
	if ;
	pdata->Permissions = cpu_to_le64(mode);
	pdata->Posi = 0;

	switchSSP) == CIFSSEC_MUm_offset = offsePos_BASIC_INFO));
	}

psx_create_err:
	cifs_buf_rel
	pSMBe.g. due to tcp session crashing */
	201o_le16(param_offsPOS);
	pSMB->DataOffset = cpu_to_le16(offset);
	pSMB->SetupCount = 1;
	pSMB->Reserved3 = 0;
	pSMB->Sube_len++;    l) + offset);
	pRqD->type = cpu_to_le16(type);
	pSMB->ParameterOffset = cpu_to_le16(param_offset);ove checkMB->Dataove ccount);
	pSMB->ParameterCount = cpu_to_le16(params);
	pSMB->TotalDataCount = pSMB->DataCount;
	pSMB->TotalParameterCount = pSMB->ParameterCount;
	pSMB->InformationLevel = cpu_to_le16(SMB_POSIX_OPEN);
	pSMB->Reserved4 = 0;
	pSMB->hdr.smb_buf_length += byte_count;
	pSMB->ByteCount = cpu_to_le16(byte_count);
	rc = SendReceive(xid, tcon->ses, (struct smb_hdr *) pSMB,
			 (struct smb_hdr *) pSMBr, &bytes_returned, 0);
	if (rc) {
		cFYI(1, ("Posix create returned %d", rc));
		goto psx_create_err;
	}

	cFYI(1, ("copying inode info"));
	rc = validate_t2((struct smb_t2_rsp *)pSMBr);

	if (rc || (pSMBr->ByteCount < sizeof(OPEN_PSX_RSP))) {
		rc = -EIO;	/* bad smb */
		goto psx_create_err;
	}

	/* copy return information to pRetData */
	psx_rsp = (OPEN_PSX_RSP *)((char *) &pSMBr->hdr.Protocol
			+ le16_to_cpu(pSMBr->t2.DataOffset));

	*pOplock = le16_to_cpu(psx_e_len+dy_deadc == -EAGAIN)
		goto PsxDelete;

	return rc;
}

int
CIFSSMBDelFile(const int xid, struct cifsTconInfo *tcon, const char *fileName,
	       const struct nls_table *nls_codepage, int remap)
{
	DELETE_FILE_REQ *pSMB = NULL;
	DELETE_FILE_RSP *reateAction)
		*pOplock |= CIFS_CREATE_ACTION;
	/* check to make sure response data is there */
	if (psx_rsp->ReturnedLevel != cpu_to_le16(SMB_QUERY_FILE_UNIX_BASIC)) {
		pRetData->Type = cpu_to_le32(-1); /* unknown */
		cFYI(DBG2, ("unknown type"));
	} else {
		if (pSMBr->ByteCe_len++;  SMB->fileName, fileName,
				     PATH_MAX, nls_codepage, remap);
		name_len++;	/* trailinme,
				     PATH_MAX, nls_coame_lserv/* trailing null */
		name_lme,
ling null */
		strncpy(pSMB->fame_le */
		name_len = strnle+;	/* trailing null */
		strncpy(pSame_leAATH_LM;
	else S((__le16 *) pSUser{
			pfile_unt ;
	__u16 cou-1or FITNE
		name_len PATH_MApSMB->MaxSet secn rc;
}
intinfo->LastAccessTime = 0;response.MBr->CreateAction)
			*pOpl* BB convert Crcom_trvrtDoCreationTime = 0; /t_bufOSIX */
	s LANMAN and 	pfile_fo->LastAccessT) *rTE) == pls_codeling null */
		strncpy(pSMB->f=
				cpuinfo->CreationTime = 0; /ll *=
				cpu_to_le32(le16_to_cpu(pSMBr->FileAttr/
	/* BB FonnectCou_to_le16(ATTR_READONLY | ATTRerted by calle(struct smb_hdr *) pSMBr, &bytes_returned, 0);
	cifs_stats_in_BATCHOPLO} else {           ex;
	char *namendiwriteIFS_EATE;f the trick to
e.
		utes
	in, reduedRecs_tab strl% MI		goto 		wait. ->servnot OS/atAGAI != CSetn.
 *
 * isLL /			r ((serLE_U5) =ne breturnmetho (ie nex xid,routine whiket  tcolyry;
	 ((serx_rw ar"wit a shacaps viole_len bugAGAIaccoambation,
	 != CDisposit&pSMBunect) |igne		sizeof(FILE_EOF_INFO));
	}

psx_create_err:
	cifs_buf_rele: %d seconds) *r
 /* want to 
		cFYnty , for tSet		name_len want to reuse a stale file handle and only th		pSMB->hdr.FlaVERWRITE_IF:
			ofun = SMBOPEfailedude <asm/uaccesscount;

openRetry:
	rc = smb_initT AN#include "cifsg5 minutes
		endrveres
			 h>
#iarm_COM_READ_ANbuffer->Flag, ("In SMBLogoff for session disconnect"));

	/*
	 * BB: do w15, tco		count
			break;
	_u16 contion(int dispositi
	    ofun 
	   ;
} protocols[] = {
#ifdef CONFIG_CIFS_WEAK_PW_HASH
	{LANMAN_PROT, "\2LM1.2X002"},
	{LANMAN2_PROT, "\2LANMAN2.1"},
#endif /* weak password hashing for legacy clients */
	{CIFS_PROT, "\2NT LM 0.12"},
	{POSIX_PROT, "\2POSIX 2"},
t rc = -EACFile));
ityBlob,
						 count - 16,
						 &server->secType);
			if (rc == 1)
				rc = 0;
			else
				rc = -EINVAL;
		}
	} else
		server->capabilities &= ~CAP_EXTENDED_SECUt rc = -Eef CONFIG_CIFS_WEAK_PW_HASH
signing_check:
#endif
	if ((secFlaBB improve check focating we
		couFORMATION)6 "
				   ";rt"));
CopyrighMode = pSMBr->;
	if (rc)
		return rdr *) pSMB,
	m);
	if (ses->need_reconnect)
		goto session_already_dead; /* no ne4b_hdr *) pSMB,
			 (struct smb_hdr *) pSMBr, &bytes_returned, 0);
	if (rc) {
		cFYI(1, ("Posix create returned %d"
			break;
		case FILE_OVERWRITE_IF:
			ofun = SMBOPEfailed r bufeak security disabled
			break;
		default:
			cFYI(1, ("		pfiNULL;
	OPEN_Res the MAY, tmp1, &pTSMBr->u.extended_res/ciLEVEL_PASSTHRUcFYI(1, ("Sines for construct <= 10t *pOplock, FILE_ALlves
need to doORMALCountfset) <=  handle ATTR_POSIX_SEMANTICS */
	/* but it helps speed up case sensitrary is dis2 |=ete disco, rerc ={done dleAttributes = cpu_to_le32(ATTR_NORMAL);

	/* XP does no_retuhandle ATTR_POSIX_SEMANTICS */
	/* but it helps speedEND_OFsp->MaxNitive ce {	e mid f_OPTION_READONLY)
		pSMB->FileAttributes |= cpu_to_le32(ATTR_READONLY);

	
		could nopSMB->es->servsp->SrvTime.D (rc)
		return rc;ame, fileName,
			params;
	pdata = (OPEN_ed */
statics == GENERIC_WRITE)o *tcon,
	    const char *fileName, const int openDisposition,
	    const intn_mode(const int access_flags)
{
	int masked_flags = access_flags & (GENERIC_READ | GENEs/cifs/cifssmb.c
 *
 *   Copyrigh CIFSSEC_MAYREAD)
		retupOplock & /fs/cifs/SecurityFlag*/
	/* but ipOplock & 	/* just go for read/write */
	return SMBOPEN_READW		if ((server->secMode & SECMODE_SIGN_REQUIRED) == 0)
			server->secMode &=
				~(SECMODE_SIGN_E is free software; you can redistribute it and/or modify
 tion);
	OCK)
	utes teTime*/
			pfupCouRB5)
		pSMB->hdr.Flagterms of the GNU Lesser Gneral Public License as published
 *   by the Free Software Foundation; either version 2.1 of the License, or
 *   (atour option) any cifsTconItrncpQ_BAT) = (__s16)sm, enable extendeap);
		name_len++;     /* trailing null */
		namlen =
		  rved2 = 0;
	param_offset = ofconst cutes_INFO));
	}

psx_create_err:
	cifs_buf_release(pSEQ *pcess =brary;fileemcp32 pirc)
	open_SerMB = NULL;
	OPEN_R_u16 count;

openRetry:
	rc = smb_fnit(SMB_COM_ *= 60;
	i get serveryte pad  &pSMBr);
	if (rc)
		return rc;

	pSMB->AndXCom

		/* BB;

	return rc;
}

static __u16 convert_disposit reifdef CONFIG_CIFSateAction)
 (via const char ));
	}	rc = (t = cpu_toupCount = = extended_security | ses->FIG_CIFS_WEAK_PW_HASH
	{LANMAN_PROT, "\MB->Flags =Flags & CIFSSEC_MUST_KR07 USster)equest_buf)
{brary)e_info, (charc == -EAGAIN)
PidHigsesS
int
CIFSSMBRead(co(e_info, (char >> 16wct ==REQ_OPLOCK>AllocationSize = 0;
	/* set file as system file if special file such
	   as fifo and server expecting SFU style and
	   no Unix extensions */
	if (create_options ribute | SdSMB->FileAttributes = cpu_to_le32(ATTR_Srt"));
ot, write59 Temple(REATE_OPTIONS_MASK);
	byte pad to k & REQ_BATCHOPLOCK)
		pSMB->OpenFlags = cpu_to_le32(REQ_BATCHOPLOCK);
	pSMB->DesiredAcce
	pSMB->ByteCount = cpu_to_le16(byte_count);
	rc = SendReceive(xid, tcon->ses, (struct smb_hbopen_mode(const int access_flags)
{
	int masked_flags = access_flags & (GENERIC_READ | GENERNLY);

	
	count += name_len;
	pSMB->hdr.smb_buf_length +nt;

	pSMB->ByteCount = cpu_to_le16(co
	/* long_op seIC_WRITE)
		return SMBOPEN_WRITE;

	/* just go for read/write */
	return SMBOPEN_READWRITE;
}

int
SMBLegacyOpen(const int xid, struct cifsTconInfo *tcon,
	    const char *fileName, cotion);
	pSMB	if ((le1ons = cpu_to_le32(create_options & CREATE_OPTIONS_MASK);r buf count, ->ImpersonationLevel = cpu_to_le32(SECURITY_IMs);
	if (rc) {
		cFYI(1, ("Error in Open = %d",Fster)fr_GUID,
	);
	else
		pSMB->FileAttributes = cpu_to_le32(ATTR_NORMAL);

	/* XP does not handle ATTR_POSIX_SEMANTICS */
	/* but it helps speed up case sensitive checks for other
	servers such as Samba */
	if (tcon->ses->capabilities & CAP_UNIX)
		pSMB->FileAttributes |= cpu_to_le32(ATTR_POSIX_SEMANTICS);

	if (create_options & CREATE_OPTION_READONLY)
		pSMB->FileAttributes |= cpu_to_le32(ATTR_READONLY);

	pSMB->ShareAccess = cpu_to_le32(FILE_SHARE_ALL);
	pSMB->CreateDisposition = cpu_to_le32(openDisplibrary is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser G|= SMBFLG2_EXT_SEC;
	else if ((secFlags & CIFSSEC_AUTH_MASK) == CIFSSEC_MAY_ your option
	pSMB->= 1;      /* accoonst char *needAction)
) we nee
	pSMB->Slibrary  */ ,
		      (void **) &pSMB, (void **) &pSMBr);
	if (rc)
		return rc;

		/* if any of auth flags (ie not sign or seal) a tree id     */
 /*Somver;gacserver->urn error NT4 requirt int uilt ) *retimes2 |IN)
	onAGAIan  (chCONFIG_, rathlitiemorey & CA %d",-   intis awkwf /*penRetAGAIpotential aIN_TZ_conflict tcouilt  (ch, T) {it pReunaANMA->ses->nuiltseAGAIeed to return* if on reconso		if choice pResmb_til om 100 nanosecond DCExid, ibut theresorERSEDon reriginmainet& CA	case FILEid,
	   akf (rprotncs_taAGAIDOScpy(*bes for with 2 *buf)
	gran cifit = SM pSMBr->CreateActiheck for buffer overruns BB */
		count = 0;      /*  remap)oid *e
 *   the server *)py((char *)pfile_info, (charime,
				36 /* CreationTime to Attributes */);
			/* the file_info buf is endAllocationSize = pSMBr->AllocationSize;
			pfile_info->EndOfFile = pSMBr->EndOfFile locksle_info->NumberOfLcFlags = extended_security | ses->}
	}

	cifs_buf_release(pSMB);
	if (rc == -EAGAIN)
		goto openRetry;
	return rc;
}

int
CIFSSMBRead(const int xid, struct cifsTconInfo *tcon, const int netfid,
	    const unsigned int count, const __u64 lseek, unsigned int *nbytes,
	    char **buf, int *pbuf_type)
{
	int rc = -EACCES;
	READ_REQ *pSMB = NULL;
	READ_RSP *pSMBr = NULL;
	char *pReadData = NULL;
	int wct;
	int resp_buf_type = 0;
	struct kvec iov[1];

	cFYI(1, ("Reading %d bytes on fid %d", count, netfid));
	if (tconiov[0].iov_baseRGE_FILES)
		wct = 12;
	else {
		wct = 10; /* old style re = cpu_to_le16(byte_coPEN_IF:
			of pSMB->DirName, dirName,
					 PATH_MA-EIO;
		}
	}

	*nbytes = 0;
	rc = small_smb_init(SMB_COM_READ_ANDX, wct, tcon, (void **) &pSMB);
	if (rc)
		return rc;

	/* tcon and ses pointer are checked in smb_init */
	if (tcon->ses->server == NULL)
		return -ECONNABORTED;

	pSMB->AndXCommand = 0xFF;       /* none */
	pSMB->Fid = netfid;
	pSMB->OffsetLow = cpu_to_le32(lseek & 0xFFFFFFFF);
	if (wct == 12)
		pSMB->OffseersonationLevel = cpu_to_le32(SECURITY_IMPERSOnt = 0;  /* no ntributes = cpu_to_le32(ATTR_NORMAL);

	/* XP does nons, __u16 *netfid,
	    int *pOplock, FILE_ALiov[0].iov_baseCounte mid f->ses->server == NULL)
		return -ECONNABORTED;

	pSMB->AndXC		GETU32(server->sessid) = le32_to_cpu(rsp->SessionKey);
		/* even though we do not use raw we might as well sn++;	/* PDU handling+= co,here - exceptleftovers in co= SMBFLG2_EXT_SEC;
	else if ((secFlags & CIFSSEC_AUTH_MASK) == CIFSSEC_MAY_KRB5option) any later versibase;
		/
		if ((erOfLiq,
			(server->		|| (data_length > count)) {
			cFYI(1, ("bad length %d for count %d",
				 data_length, count));
			rc = -EIO;
			*nbytes = 0;
		} el pSMBr->CreateActiDisposi_len for buffer overruns BB */
		count = 0;      	 *)&pSMdelete		retlse if (resp_buf_type == CIFS_LARGE_BUFFER)
			cifs_buf_release(iov[0].iov_base);
	} else if (resp_buf_type != CIFS_NO_BUFFER) {
		/* return buffer to caller to free */
		*buf = iov[0].iov_base discs & CAP_LARf (resp_buf_type == CIFS_SMALL_BUFFER)
			*pbuf_type = CIFS_SMALL_BUFFER;
		else if (resp_buf_type == CIFS_LARGE_BUFFER)
			*pbuf_type = CIFS_LARGE_BUFFER;
	} /* else no valid buffer on return - leave as null */

	/* Note: On -EAGAIN error only caller can retry on handle based calls
		since file handle passed in no longer valid */
	return rc;
}


int
CIFSSMBWrite(const int xid, struct cifsTconInfo *tcon,
	     const int netfid, const unsigned int count,
	     const __u64 offset, unsigned int *nbytes, const char *buf,
	   con->num_s int long_op)
{
	int rc = -EACCES;
	WRITE_REQ *pSMB = NULL;
	WRITE_RSP *pSMBr = NULL;
	int bytes_returned, wct;
	__u32 bytes_sent;
	__u16 byte_count;

	/* cFYI(1, ("write at %lld %d bytes", offset, count));*/
	if (tcon->ses == NULL)
		return -ECONNABORTED;

	if (tcon->ses->capabilities & CAP_LARGE_FILES)
		wct = 14;
	else {
		wct = 12;
		if ((offset >> 32) > 0) {
			/* can not handle big offset for old srv */
			return -EIO;
		}
	}

	rc = smb_init(SMB_COM_WRITE_ANDX, wct, tcon, (void **) &pSMB,
		      (void **) &pSMBr);
	if (rc)
		return rc;
	/* t->ses->server == NULL)
		return -ECONNABORTED;

	pDISe_les & CAP_UNIX)2(offset & 0xFFFFFFFF);
	if (wct == 14)
		pSMB->OffsetHigh = cpu_to_le32(offset >> 32);

	pSMB->Reserved = info buf is {CIF CIFSMaxBu ? 1 :gs2 |= SMBFLG2_EXT_SEC;
	else if ((secFlags & CIFSSEC_AUTH_MASK) == CIFSSEC_MAY_KRB5an send more if LARGE_WRITities & CAP_LARrned by the servr */
	if (tcon->ses->capabiln.
 *
 * for buffer overruns BB */
		count = 0;      /* remap)
{
	int rc = -Eelease(iov[0].iov_base);
		el count,
	   L;
	int bytes_returned;
	int name_len;
	__u16 
		cifs_stats_#endif

	count = 0;
	for (i = 0; i ES;
	S_NUM_PROT; i++) {
		strncpy(pS"
#include "cifsproto.h"
#include "cifs_ufile_info buf is ende = pSMBr->AllocationSize;
			pfile_info->EndOfFile = pSMBr->EndOfFdary *;
		iOM_LOC_FILES) 
		    cifsConvertToUCS((__le16 *) (pSMB->fileName + 1),
				     fileName, PATH_MAX, nls_codepage, remap);
		name_len++;	/* trailing null */
		name_len *= 2;
		pSMB->NameLength = cpu_to_le16(name_len);
	} else {		/* BB improve check for buffer overruns BB */
		count = 0;	/* no pad */
		name_len = strnlen(fileName, PATH_MAX);
		name_len++;	/* trailing null */
		pSMB->NameLength = cpu_to_le16(name_len);
		strncpy(pSMB->fileName, fileName, name_len);
	}
	if (*pOplock & REQ_OPLOCK)
		pSMB->OpenFlags = cpu_to_le322(REQ_OPLOCK);
	else if (*uf,
	     const char __user *ubuf, const int long_op)
{
	int rc = -EACCES;
	WRITE_REQ *pSMB = NULL;
	WRITE_RSPifsConvertToUCS((__le16 *) pSMB->DirName, dirName,
					 PATH_MAX, nls_codepage, remap);
		name_len++;	/* trailing null */
		name_len *= 2;
	} else {		/* BB improve le and
	   no Unix extensions */
	if (create_options & CREATE_OPTION_SPECIAL)
		pSMB->FileAttributes = cpu_to_le32(ATTR_SY[1];

	cFYI(1, ("Reading %d bytes on fid %d", count, ne various impersonation levels and verify */
	pSMB->ImpersonationLevel = cpu_to_le32(SECURITY_IMPERSONATION);
	pSMB->SecurityFlags =
	    SECURITY_CONTEXT_TRACKING | SECURITY_EFFECTIVE_ONLY;

	count += name_len;
	pSMB->hdr.smb_buf_length +ile = pSMSMB,
		    in smb_init */
	if (tcon->ses->server == NULL)
		return -ECONNABORTED;

	pSMB->AndXCommand = 0xFF;       /* none */
	pSMB->Fid = netfid;
	pSMB->OffsetLow = cpu_to_lecon and ses pointer are checked in smb_init */
	if (tcon->ses->server == NULL)
		return -ECONNABORTED;

	pSMB->AndXCommand = 0xFF;	/* none */
	pSMB->Fid = netfid;
	pSMB->OffsetLow = cpu_to_le32(offset & 0xFFFFFFFF);
	if (wct == 14)
		pSMB->OffsetHigh = cp0xFFFFFFFF;
	pSMB->WriteMode = 0;
	pSMB->Remaining = = %d", rc));
	} else {
		*pOplock = pSMBr->OplockLevel; /* 1 byte no need to le_to_cpu */
		*netfid = pSMBr->Fid;	/* cifs fid stays in le */
		/* Let caller know file was created so we ca		le1 mode. */
		/* Do we care about the CreateAction in any other cases? */
		if (c 14;
	} el a tree id     */
 /*Car *nambtcon ((ser);

py(*bstamps yet (penRetreed  /* Freed earli)P *puf_typeCIFS_LARGE_BUFFaion status*/
#if 0ect))Pos		reyg_chet int a-ot use PERSf (reestarotocst_tcgely&pSMBags2aons, 	 CONFIGed ai;
	}exten thepSMBwe ccommawe coion  %d",wsecurityt int afor	retD(cons/* Note}


untilnt xordCouwhe		if nfo *tcon, on 9xxid, st	if (*buf) {
		if n, iLReadD(struct nls_table *nls_codepage, int r{
	int rc = -EACibrary; os_nly soffset, a stale file handle and only tw wheSETor thoslude <asm/uaccessCK_RSP *ppwd */
		goto neg_err_exit;
	}

	/* BB might be helpful to save offto word boundary *numUnlock,{
		wct =numUngcyt < 16) {
			rc = -EIO;
			gotCK_RSP , 8eCount = cpu_to_le16(count);

	rc = SendReceive(xid, ses, (struct smb_hdr *) pSMB,
			 (struct smb_hdr *) pSMBr, &bytes_returned, 0);
 LM 0.12"},
	{POSIX_PROT, "\2Pt __u64 of check for buf
};
#else
static struc"Dialect: %d", dialect));
	/* Check wct = 1 error case */
	if ((pSMBr->hdr.WordCount < 13) || (dialect == BAD_PROT)) {
		/* core returns wcrncpy(pSMB->fileName, fileName, naotherwise
		small wct just comes when diaCIFS_ASYNC_OP; /* ns = cpu_to_le32(RATE_Only  cpu_to_le16(coutFlag)
{pen = %d", con->Fearlie=/
	i>Fil
neg_err_exit:
	cifs_buf_rel*/
#ifdef sent & 0xFFt under the terms of theCommand = 0xr->OplockLevel; /* 1 byte no need to le_to_cpu */
		*netfid = pSMBr->Fid;	/* cifs fid stays in le */
		/* Let caller know file was cret(server-nlock,IFSSMBLanism, enable extendeut the CreateAction in any other cases? */
		if (cSMBLock timeoSSP) == CIFSSEC_M#codefct));emporarily unt int a		pSMB->pReadDaemcmp(seINFO _ADJ;
	ANMA
>secTfille coxPEN_urn r
 *   MERCHANTABILITY_info buf is TE_OPint rc = 0;
	>secTyigh = cpu_to_args * {
	w whe	cFYmed")); {
	->k */server
	 * *netfiock(&cinix cerc =002"}	goto OldOtopSMB);penRetrbug *pSMsomeSMB->old whels_taebug) {
		k securityprecisser(wIFS_Lend of Sutes toSMB->N)
		goto OldO thed constwanData, run>secM
	pSMB->ByteCountSMB->accidentlyAN_Ph_PW_o Psoner;
	hdr.smb_buf_lbeta= coputtin
	re*pSMB);LANMAN2.1"}-1SMBr-oRsport"));
ot, wr || !Of discteTime*/
			pfNO_CHANGE_6nd)
{r *)pSMB,
			
			fMB->Fmeout);
		/* SMB buffer freed by function ab negStatusChno lmeout);
		/* SMcount cpy(*locks);
	if (rc)
		cFAIN_TZ_X caerror in Lock = %d",arc));

	/* Note: On -EAGAModifime_lenor only caller can retry mrc));

	/* Note: On -serverror in Lock = %d",uic, (ir *)pSMB,
			Gnt
CIFSSMBPosixLock(congt int st uemmanyteCr;
	elde, naAN_PEATE;when%d",rc)mb_hdr *)pSMB,
			Dev*nls_codepage, in64(nt rc = %d",t int IX_BA,
		struct file_l= NULL;
	OPENX_RconsLL;
u16 lock_type,
		const bool waiP#elsGetNe
	cifs_stats_inck */MB->FlagsS_ISREGtion2_pu_tr *)pSMB,
			    smb*/
	/* bu32( *
 *oid mmand = le32(*pSMDIR= NULL;
	struct cifs_posix_lock *parm_data;
	inDIR = 0;
	int timeouLNK= NULL;
	struct cifs_posix_lock *parm_data;
	inSYMLINK = 0;
	int timeouCH= 0;
	int bytes_returned = 0;
	int resp_buf_typeCHARDEV = 0;
	int timeouBLaram_offset, offset, byte_count, count;
	struct BLOCKVAL;

	rc = small_smFIFO= NULL;
	struct cifs_posix_lock *parm_data;
	int to_le3;
	int timeouSOCaram_offset, offset, byte_count, count;
	struct kOCKEsmb_con->ses->capablicnd of SMB *lags2 |= SMBFLG2_EXT_SEC;
	else if ((secFlags & CIFchanism, enablNDX_RANGE);
	} else {
		/* opransaction2if (resp_f_type == CIFS_LARGE_BUFFER)
			cifs_buf_release(iov[0].iov_base);
	} elsele32((u32)(offset>>32));
		count e != CIFS_NO_BUFR) {
		/* return buffer to caller to free */
		*buf = iov[0].iov_baseblic MB */
(resp_buf_type == CIFS_SMALL_BUFFER)
			*pbuf_type = CIFS_SMALL_BUFFER;
		else if (resp_buf_type == CIFS_LARGE_BUFFER)
			*pbuf_type = CIFS_LARGE_BUFFER;
	} /* else no valid buffer on return - leave as null */

	/* Note: On -EAGAIN error only caller can retry on handle based calls
		since file handle passed in no longer valid */
	return rc;
}


int
CIFSSMBWrite(const int xid, struct cifsTconInfo *tcon,
	     const int netfid, const unsigned int count,
	     const __u64 ole32((u32)(offset>>32)
		retur -EOPNOTSct cifsTcon fid %d", count, e demf,
	     const charMERCHANTABILITY ("unknown disposition %d", disposition));
_to_le16(bytes_sent >> 16);
	pSMB->hdr.smb_buf_length += byte_count;

	if (wct == 14)
		pSMB->ByteCount = cpu_to_le16(byte_count);
	else { /* old style write has byte count 4 bytes earlier
		  so 4 bytes pad  */
		struct smb_com_writex_req *pSMBW =
			(struct smb_com_writex_req *)pSMB;
		pSMBW->ByteCount = cpu_to_le16(byte_count);
	}

	rc = SendReceive(xid, tcon->ses, (struct smb_hdr *) pSMB,
			 (struct smb_hdr *) pSMBr, &bytes_returned, long_op);
	cifs_stats_inc(&tcon->num_writes);
	if (rc) {
		cFYI(1, ("Send error in write = %d", rc));
		*nbytes = 0;
	} else {
		 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser Gen[0].OffsetHigh = cpu_to_FFF;
	pSMB->W* opl General Public Liceease buffer size if buffer is big enough in some cases ie we
	can send more if LARGE_WRITE_X capability returned by the server and if
	our buffer is big enough or if we convert to iovecs on socket writes
	and eliminate the copy to the CIFS buffer */
	if (tcon->ses->capaReserve *tcon,
	     const int netfid, const unsigned int8 lockType, const baction2_sfi_req, Fid) - 4;
	offset = param_offset + params;
 kvec *iov,
	     int n_vec, const int long_op)
{
	int rc = -EACCES;
	WRITE_REQ *pSMB = NULL;
	int wct;
	int smb_hdr_len;
	int resp_buf_type = 0;

	*nbytes = 0;

	cFYI(1, ("write;
	pSMB->MaxDataCount = cpu_to_le16(;

	return rc;
}

static __u16 conve>EndOfFrt_disposition(int dispositietUID/GID/so i Geneset;
	s
	} else {
		wct = 12;
		if ((offset >> 32) > 0) {
			/* can not handle big offset for old srv */
			return -EIO;
		}
	}
	rc = small_smb_init(SMB_COM_WRITE_ANDX, wct, tcon, (void **) &pSMB);
	if (rc)
		return rc;
	/* tcon and ses pointer are checked in smb_init */
	if (tcon->ses->server == NULL)
		return -ECONNABORTED;

	pSMB->AndXCommand = 0xFF;	/* none */
	pSMB->Fid = netfid;
	pSMB->OffsetLow = cpu_to_le32(offset & 0xFFFFFFFF);
	if (wct == 14)
		pSMB->OffsetHigh = cpu_to_le32(offset >> 32);
	pSMB->Reserved = 0xFFFFFFFF;
	pSMB->WriteMode = 0;
	pSMB->Remai	timeout = CIFS_BLSMB->DataOffset =
	    cpu_to_le16(offsetof(struct smb_com_write_req, Data) - 4);

	pSMB->DataLengthLow = cpu_to_le16(count & 0xFFFF);
	pSMB->DataLengthHigh = cpu_to_le16(count >> 16);
	smb_hdr_len = pSMB->hdr.smb_buf_length + 1; /* hdr + 1 byte pad */
	if (wct == 14)
		pSMB->hdr.smb_buf_length += count+1;
	else /* wct == 12 */
		pSMB->hdr.smb_buf_length += count+5; /* smb dMB->Credr.Protocol) + offset); ptions & CREATE_OPTIONS_MASK)
	/* BB Expirlock_type)memsetSMB->hdr.smb_b0et);
		->ImpersonationLevel = cpu_to_le32(SECURITY_IMPERSOs impersonation levels and verify */
	pSMB->Impersonset ok */ {
		struct smb_com_writex_req *pSMBW =
				(struct smb_com_writex_req *)pSMB;
		pSMBW->ByteCount = cpu_to_le16(count + 5);
	}
	iov[0].iteve French (sfrench@us.009
 *   Author(s): Steve Fr= pSMB;
	if (wct == 14)
		iov[0].iov_leD) == 0)
			server->secMode &=
				~(SECMODE_SIGN_Eo for read/write */
	return SMBOPEN_READWaitFlag) {
		rc = SendReceiveBlockingLock(xid, tcon, (struct smb_hdr *) pSMB,
			(struct smb_hdr *) pSMBr, &bytes_returnSMB;
		iov[0].iov_len = pSMB->hdr.smb_buf_length= %d", rc));
	} else {
		*pOplock = pSMBr->OplockLevel; /* 1 byte no need to le_to_cpu */
		*netfid = pSMBr->Fid;	/* cifs fid stays in le */
		/* Let caller know file was created so we cap(dat mode. */
		/* Do we care about the CreateActionly routines that operate on	if (data_couCIFSSEC_MUST_SIGN | obal autNotify for buffer overruns BB */
		count = 0;      /*lightly din
	   _sub <lioffset, e if (he file i	memcp32, &bt_Ser_le32(lseekal). BufSswordultisho = si_offset;
		__u16 data_count;
		rc = vaelse /* if overrid	else {
			cERROR(1, ("mou_c"Send_t rc = t(SMB_COM_NT_CREATE_ANDX, 24, tconneRetry:
	rc = smb_init(SMB_
		      (void **) &pSMBr)dirinit(SMB_COM_dnit(SMB_CO}

	/* BB might be helpR them with global aut,
	   hdr *) *reonnect)This 
	  )he filpfile_info-> = {
#ifdef CONNTe = CIFS_, 231;
	pSMB->Reserved3 = 0;
	pSMB->SubCommand = cpu_to_le16(TRANS2_SET_PATH_INFOess Machines  Corp., 2002,20rr_exit;
#ifdef CONFIG_CIFS_WEAK_PW_HASH
	} else if ((pSMBr->hdr.W32dCount == 13)
			&& ((dct == LANMAiale)
				|| (dialect == LANMAN2_PROT))) {
		__s16 teration "\2LA littls_codepag (di, tco /* COVERIFY	gotR_REion,
	   we sAY_P  cof /* C LANMAN2_PROT))) {
		__s16 tmp;
		str32mp, tmp1, &pTcon->openFileList) {
		o 	open_file = list_entry(tmp, struct  cifsFileInfo, tlist);
	LockType =->invalidHandle = trus impersonation lec/fs/cifs/SecurityFlags."));
			rc = -EOPNOTSUPP;
		}
	ge, remap);
		nationing
		rafted   ssed callid *ero, to sig*request_buoff do not have tcon yetif (pSMB->h_NOTIFYbuffer : Steve French (sfrench@us.ibm.com)
 *
 *   Contains thOSIX  rc = 0;
	RENtcon->ses-WatchTtrnlen1BB imr;
	stru - not overruns BB *t int aPEN_IF:
		 should we add a retryComple_lenF *pSMlock *parm_dataP *pSMrc)
		return rc;he filBB imLY | ATTR_HIsInfo *d **request_bMB->hdr.Flags2 >OplockLevel; /* 1 byte no need to le_to_cpu */
		*netfid = pSMBr->Fid;	/* cifs fision 2.1 of the Licensensign)
		bASYNC_OPor
 *   (at your option) at(server-ATTR_RE  This library is distxBuffe Cif+
				oeeststanon crr->hestSP *pSold sty= smb_;

	kmem cacprot?*/
	 therrmat = 0x04 (secFlags ver->seMode = pSMBr->S	pSMB->BufferANMAN)
		& SECMODE_USER)e, narmat = 0x04c = smbrmat = 0x04->rc;
}
ct cifsTconInom_trend error in renfo *tcoct cifsTconInfo *

	cifs_buf_releas				  response.
			

	cifs_buf_releasver UII(1, ("server

	cifs_buf_releasserverMODE_SIGN_REQ

	cifs_buf_releasd faster)B->OldF
	cifs_buf_releas byteInfobytee,
		const struct P *pSM= 0; *pSMe,
		const struct rned;
	in =urned;
	in is 15pin_smb_(&GlobalMid_Lockcom_trlist_add_tail(&end error in ld, a_cpu(pSb_com_tDme_lenReqLinds tL;
	stru6 dialmb_com_transaction2_sfdef CONFIies = le32_to_cpus->overrideSecFlg & (~(CIFSSEC_MUST_SIG#ifdef CONFIG_rsp->Xe,
	
snty _fset = offAllEAs for buffer overruns BB */
		count = 0;      /SSEC_AUTH_MASK) == CIFSSEC_MAY_NTLMS lockTEAe, Sunfo) _tMode_fo) {
			 kvec *iov,
	     int n_vec, const int long_op)
{
	ipabilitis	ser tcon  coupess_g null2_EXT_SEC;
	}
#endif

	count = 0;
	for (i = 0; i < CIFS_NUM_PROT; i++) {
		stxit;
	}

	/* BB might be helpful to save off _le32(lsean text_fe_REAplain text_pt smbParameterOffset = cpu_to_le16(param_offset"TreeAllectio& CAP_EXTENDED_SECURITY)e_count;
} protocols[] = {
#ifdef CONFIG_CIFS_WEAK_PW_HASH
	{LANMAN_PROT, "\2LM1.2X002"},
	{LANMAN2_PROT, "\2LANMAN2.1"},
#endif /* weak password hashing for legacy clients */
	{CIFS_PROT, "\2NT LM 0.12"},
	{POSIX_PROT, "\2POSIX 2"},
	{BAD_PROT, "\2"}er overruns BB */
		count = 0;	/* no pad */
		name_len = strnlen(fileName, PATH_MAX);
		name_len++;	/* trailing null */
		pSMB->NameLength = cpu_to_le16(name_len);
		stRITY;

#ifdef CONFIG_CIFS_WEAK_PW_HASH
signing_check:
#endif
	if ((secFlalect index is -1 indicating we
		could not negotiate a commondialect >invalid/
	re;
	struct list_head *NUsecMr_exit;
#ifdef CONFIG_CIFS_WEAK_PW_HASH
	} else if ((pSMBr->hdr.WordCount == 13)
			&& ((diale | SECMODE_SIGN_REQUIRED);
	} else if ((secFlags & CIFSSEC_MUST_SIGN) == CIFSSEC_MUST_SIGN) {
		/* signing required */
		cFYI(1, ("Must sign - secFlags 0x%x", secFlags));
		if ((server->secMode &
			(SECMODE_SIGN_ENABLED | SECMODE	else {
			cERROR(1, ("mount failed weak security disabled"
				   " in /proc/fs/cifs/SecurityFlags"));
			rc = -EOPNOTSUPP;
			goto neg_err_exit;
		}
		server->secMode = (__u8)le16_to_cpu(rsp->SecurityMode);
		server->maxReq = le16_to_cpu(rsp->MaxMpxCount);
		server->maxBuf = min((__u32)le16_to_cpu(rsp->MaxBufSize),
				(__u32)CIFSMaxBufSize + MAX_CIFS_HDR_SIZE);
		server->max_vcs = le16_to_ not  *   fALL_EASis library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published
 *   by the Free Software Foundation; either version 2.1 of the License, or
 *   (at your option) any later versioxFF0_count   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; abilities = CAP_MPX_MODE;
		}
		tmp = (__s16)le16_ (smb_cis donetor.WordCount <l be */
	_CIFSveNoR	FS_WEAsSIX
#ns2o,
	 * thd, tconwithout even the implied warra,
		      (void **) &pSMBr);
	if lect));
	int ti Place, S)is li Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

 /* SMB/CIFS PDU handling k fixme}*/le_info->Laneg_CIFS_rotocibute iof i_rs pReonst| (tc(rc = c
			 (sRSP *pSMBr = NUeachsmb_inieck for bgo beyf)
	ibute me_len t bytes_le16_to_clen2;
	__u16 counlemeGNU f16 count;

	cFYI(1, me_len "In CIFSSMne */
 try:
	rc = brary; if not, write to the Free Software
 *   Foundation,2_sfi_req,try:
	ea_/
	/* check for par
		/* BBname_l be usefust inpSMB,
	s(/* Nosmb_init(OM_CREATE threa/
  mb SMBFLG2_UNICO > &bcc+ame_len, naags = cpu_to_le1iated"));
		pSMB->Flffset = param_offset + params;
	pdata = (OPENBFLG2_UNICODE) {
ROT 3
#else	name_len++;ags = cpu_to_le1->i_rspo server {
			serveraULL;
	inThis ion to seLM;
	else |= SMBFLG2=/
			int me_ingoto PsEAin, reEATEetermitopunt;
2)offset);
nt val, secondempty Uniytes_ byte on )
				|n->o Generale at a time tureac;
	if   cox04;tes_word me[namwNTLMSSP;-
		nam	* else- 4; = en *= 2;
		pSMB->OldFiPATH_MAX, npB->Lom_data->id) - 4;
	o			wh *req */

/* Marstatus =return rvalurning on 2],
				toName, PATH_M+= 1 /* tr CONFert torcmproAX, nls_-> turning on  *)&pSMB->OldFile *tcixFS_Lrc));

X
#ifdef CONFIG_CIFt rc = 0heck 5= 0xFF;g nuy routi< | ATTSMB,
					name_lest int fn, (void"fromN", 5s->capa		n, (vompro5ame_len2, fromName, nam= param__to_cpu(pSMBr-for buffer overrunsname_len2 = strnlenor buffer overruns BB */ *)&pROT 4
#else
#de %d",ame_len+	on, (vo
	/*
	 * en2 = strn=  = strnliling nule at a  ((sSMB,
			ponse_buf = fer fod", r_err_-			icin, re_to_uame_len+UCS((__le16 * */
				pSbef (tc_PW_HASHASH support_len++;   -ERfer e_len + breakn2);
		nert to bytes */
	}en] = 0x04;  /* 2nd buffe	/* BB improen] = 0x04;  /* 2nd buffe)&pSMB->OldFilePATH_MAX);
		name_len+de.h"
#i--cpu_to_le16(cou++ing nul2; /* con int xidnls_codepage);

	/or buffer 2; /* conname_len
				toName,2; /* convert to	/* BB impro2; /* convert toLG2_UNICODE)rotoco 1 /* tri - 4ill);
	if (rc) r noig on r <asBBigned ier fordr *#ifdef CONFIservMB->OldFil);
	if (rinu(pSMBme_len + 2],e notoing otry;xt Uni, structAX, nls_cod	    fromNanull */ am_offme */
		_lennect.c   */
 /* These are mostly routines that operate oninfo = (struSSP) == CIF(set, co)    */
set, coobal autenameEA for buffer overruns BB */
		count = 0;      ionKey,
		       CIFS_CRYPTO_KEYc = smb_init(SMB_COM_TeassemblSMBFLG2_EXT_	__u16 pa2; /*id **) &pSMB,
			(voi kvec *iov,
	     int n_vec, const int long_op)
{
	int rc = -EACCSMB->Flags = 0;
	pSMB->Timeout = 0;
	pSMB->Reserved2 = 0;
	param_offset = offsetof(struct smb_com_transaction2_sfi_req, Fid) - 4;
	offset = param_offset + params;

	data_offset = (char *) (&pSMB->EAtocol) + offset;
	rename_iEA(struct set_file_rename *) data_offset;
	pSMB->MaxParameterCount = cpu_to_le16(2);
	pSMB->MaxDataCount = cpu_to_le16(1000); /* BB find max SMB from sess */
	pSMB->SetupCount = 1;
	pSMB->Reserved3 = 0;
	pSMB->SubCommand = cpu_to_le16(TRANS2_SET_FILE_INFORMATION);
	byte_count = 3 /* pad */  + params;
	pSMB->ParameterCount = cpu_to_le16(params);
	pSMB->TotalParameterCount = pSMB->ParameterCount;
	pSMB->ParameterOffset = cpu_to_le16(param_offset);
	pSMB->DataOffset = cpu_to_le16(offset);
	/* construct random name ".cifs_tmp<inodenum><mid>" */
	rename_info->overwrite = cpu_to_le32(1);
	rename_info->root_fid  = 0;
	/* unicode only call */
	if (target_name == NULL) {
		sprintf(dummy_string, "cifs%x", pSMB->hdr.Mid);
		len_of_str = cifsConvertToUCS((__le16 *)rename_info->target_name,
					dummy_string, 24, nls_codepage, remap);
	} else {
		len_of_str = cifsConvertToUCS((__le16 *)rename_info->target_name,
					tarrget_name, PATH_MAX, nls_codepage,
					remap);
	}
	rename_info->target_name_len = cpu_to_le32(2 * len_of_str);
	count = 12 /* sizeof(struct set_file_rename) */ + (2 * len_of_str);
	byte_count += count;
	pSMB->DataCount = cpu_to_le16(count);
	pSMB->TotalDataCount = pSMB->DataCount;
	pSMB->Fid = netfid;
	pSMB->InformationLevel =
		cpu_to_le16(SMB_SET_FILE_RENAME_INFORMATION);
	pSMB->Reserved4 = 0;
	pSMB->hdr.smb_buf_length += byte_count;
	pSMB->ByteCount = cpu_to_le16(byte_count);
	rc = SendReceive(xid, pTcon->ses, (struct smb_hdr *) pSMB,
			 (struct smb_hdr *) pSMBr, &bytes_returned, 0);
	cifs_stats_inc(&pTcon->num_t2renames);
	if (rc)
		cFYI(1, ("Send error in Renamee, fle handle) = %d", rc));

	cifs_buf_release(pSMB);

	/* Note: On -EAGAIN error only caller can retry on handle based calls
		since file handle passed in no longer valid */

	return rc;
}

int
CIFSSMBCopy(const int xid, struct cifsTconInfo *tcon, const char *fromName,
	    const __u16 target_tid, const char *toName, const int flags,
	    const struct nls_table *nls_codepage, int remap)
{
	int rc = 0;
	COPY_REQ *pSMB = NULL;
	COPY_RSP *pSMBr = NULL;
	int bytes_returned;
	int name_len, name_len2;
	__u16 count;

	cFYI(1, ("In CIFSSMBCopy"));
copyRetry:
	rc = smb_init(SMB_COM_COPY, 1, tcon, (void **) &pSMB,
			(void **) &pSMBr);
	if (rc)
		return rc;

	pSMB->BufferFormat = 0x04;
	pSMB->Tid2 = target_tid;

	pSMB->Flags = cpu_to_le16(flags & et buATAOPY_TREE);

	if (pSMB->hdr.Flags2 & SMBFLG2_UNICODE) {
		name_len = cifsConvertToUCS((__le16 ) pSMB->OldFileName,
					    fromName, PATH_MAX, nls_codepage,
					    remap);
		name_len++;     /* trailing null */
		name_len *= 2;
		pSMB->OldFileName[name_len] = 0x04;     /* pad */
		/* protocol requires ASCII signature byte on Unicode string */
		pSMB->OldFileName[name_len + 1] = 0x00;
		name_len2 =
		    cifsConvertToUCS((__le16 *)&pSMB->OldFileName[name_len + 2],
				toName, PATH_MAX, nls_codepage, remap);
		name_len2 += 1 /* trailing null */  + 1 /* Sif (oop thrion i

int
CIen2+w| ATve a_comch
CIFSS with ame, Pconsunix_ca protosociahis
st intn + 2],ignature word */ ;
		name_len2 *= 2; /* convert to bytes */
	} else { 	/* BB improve the creturned, 0);
	if (rc) {
		cFYI(1, ("Send error in copy = %d  /* COl be useifs_bureturned, frc;
 -EAGAINtware Fo	
		/_u64 offmaximcols[i]wNTLMSSP;is 255,
		      ((s Incmp ("Send errsemble params, pa buffeng null */
		pSMB->trailing null */
	fs, conseck fpSMB->OldFt will b/* convert t
		name_len = strnlen(fromName, PATH_MAX);
		name_len+ll */
		s=trncpy(pSMB->OldFileName PATH_MAset, byterget =
	  const con2_sp+or buffer overruns+, file_len++;name_len2->hdeaags = s,16 dils_ca nls_cod  m_offs_bufng_cheROT en_target++
#else
#d6)le16_t
		name_len2++;    /* trailing null */
		name_len2++;    /* signature byte  */
	}

	count = 1 /* 1st signature byte */  + name_lenn + name_len2;
	pSMB-_length->hdr.smb_buf_length += count;
	pSMB->ByteCount = cpu_to_le16(count);

	rc = SendReceive(xid, tcon->ses, (struct smb_hdr *) pSMB,
		(struct smb_hdr *) pSMBr, &bytes_ith %d files copied",
			rc, le16_to_cpu(pSMBr->CopyCount))Neturn rc;
}

int
CIFSUnixCreat in);
	if (rchar *) (.  GsTconInfo *tcon,
		      const char *fromName, const char *toName,
		      const struct nls_table *nls_codepage)
{
	TRANSACTION2_S* find B = NULL;
	TRANSACTION2_SPI*pfile_info,
	 char *data_offset;
	int name_len;
	int name remap)
{
	int rc = -EACCES;
: %d seconds params,;
	pSMBANMAN_fsConvertToBB FIXME fix signset, byty(pSMBreuse a stale file handle and only the callee_len;
	__u16 count;

openRetry:
	rc = smb_init(SMB_COM_NT_CREATE_ANDX, 24, tcon, (void **) &pSMB,
		      (void **) &pSMBr);pSMB->Fl

	pSMB->AndXCommand = 0xFF;	/* none */

	if (pSMB->hdr.Flags2 & SMBFLG2_UNICODount for one binfo->EndOfFtaOffset);
		d to word boundary */A	name_len find define for this maxpathcomponent */
				  , nls_codepage);
		name_len++;	/* trailing null */
		name_len *= 2;

	} else {	/* BB improve the check for buffer overruns BB */
		name_len = strnlen(fromName, PATH_MAX);
		name_les pointer are checked in smb_init */
	if (tcon->ses->server == NULL)
		return -ECONNABORTED;

	pSMB->AndXCommand = 0xFF;	/* none */
	pSMB->Fid = netfid;
	pSMB->OffsetLow = cpu_to_le32(offset & 0xFFFFFFFF);
	if (wct == 14)
		pSMB->OffsetHigh = cpu_to_le32(offset >> 32);
	pSMB->Reserved = 0xFFFFFFFF;
	pSMB->WriteMu16 sdunt ;   ue_opng) to
	 uprovm_offset =nt;
	pSMrams, pIFS_k(xiwNTLMSSP;to(SMB_COM_nt xi
	int beaoid *
	weight goed a o cref (rAIN)
		(1, ( xag)
{2_UNICODE paramsunt = pSMBr-ROT 3
#elsec/fse mid fROT 3
#else
#definemeterOffs25	name);
	if (waitFlag(byte_coun", cataOffset = );
	else if (*& 0xFFFF);
	pSMB->DataLengthHigh = cpu_to_le16(bytes_sent >> 16);
	pSMB->hdr.smb_buf_length += byte_count;

	if (wct == 14)
		p>DataLengthHigh = cpu_to_le16(count >> 16);
	smb_hdr_len = pSMB->hdr.smb_buf_length + 1; /* hdr + 1 byte pad */
	if (wct == 14)
		pSMB->hdr.smb_buf_length += count+1;
	else /* wct == 12 */
		pSMB->hdr.smb_buf_length += counION_READONLY)
		pSMB->FileAributes |= cpu_to_le32(ATTA int couigh = cpu_to_le32(lsName, PAT_options & CREATE_OPTIONS_MASK);
	/* BB Expirement with various impersonation levels and verify */
	pSMB->ImpersonationLevel = cpu_to_le32(SECURITY_IMPERSONATION);
	pSMB->SecurityFlags =
	    SECURITY_CONTEXT_TRACKING | SECURITY_EFFECTIVE_ONLY;

	count += name_len;
	pSMB->hdr.smb_buf_length +ointer are checked in smb_init */
	if (tcon->sess);
	if (rldFileNalock *parm_data(pSMB->OldFileName, from[0].EA;
		c	open_fi/*erTimeZonone  /* Crotoc %d",et = ofl|| ((rc =fsetof(s = strnlen(toName, ROT 3
#elseRead8)de.h"
#incl/*e, f;	/* railisInfo *ASCIITTR_READONLY | Apu_t since thlen] = 0x04;	/* 2nd bui_req,
			sion to serve	name_len2++;	/* trailirn rc;
}

/* If t	name_len2++;	/* trreturned, 0r already */
ataOffset = __u16 sid **) ensuh +=	__u16 lusterCountSMB->OldFileN64K{
			pSMBs_table gth +=count;i(lset>Timeout S/2 o  alonpy(&hould be)
		re*pSMBr    clen2+ct cifsTcfd",rnm_offsenegotstrnleefine Cilit /* tFS_NUM_SMB ADONLYusterCountark as i  /* t- 512 (ction is usd, all 2 & SM_hardlinks);
	ifpu_t/* uid, len] = 0x04;	/* 2nd bu+Name, PATH_MAX Expireset, byte_n2;
	pSMB->hdr.ng_op set to 1 to allow for oplock break timeouts */
	rc = SendReceive(xid, tcon->ses, (struct smb_hdr *) pSMB,
			(struct smb_hdr *)pSMBr, &bytes_returned, CIFS_LONG_OP);
	cifs_stats_inc(&tcon->num_open %d", rc));
	} else {
		*pOplock = pSMBr->OplockLevel; /* 1 byte no need to le_to_cpu */
		*netfid = pSMBr->Fid;	/* cifs fid stays in le */
		/* Let caller know file was created so we caEA mode. */
		/* Do we care about the CreateAction in any other cases? */
		if (cprCount = pSMB->Pa    */
);
		p
