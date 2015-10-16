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
	pSMB->TotalP (C)eterCopyrighcpu_to_le16(ts  C 
 *
ness Mes  Corp., 2002,2eve Frachinench (sfrench@ Steve FrInformationLevel2,2009
 *   AutSMBthe SMfFILE_UNIX_BASIC: the routReserved4 = s the routhdr.smb_buf_length += he SM, 200 the routByteench@us.ng the SMB Pnd/or modi);

	rc = SendReceive(xid, tcon->ses, (struct  redhdr *).ibm.,
			the SMby throutee Softwar, &he Ss_returned, 0: Stif (rc) {
		cFYI(1, ("Publ error in QPat
 *
 *= %d", rc): St} else {		/* decode response */
	neral validate_t2(ndation; eitt2_rsp *)sion r Genhe SM(a || ( WARR it under th< sizeof(lves: Sthe SMThfs/ci))t your	cERRORion) aMales fedand/orMERCHANTABILITY eithope .\n"ITNE	   "Unix Exten WARs can be disabled on m 2002Geit wilPuby specifying; eitnosfuhe SMYoopr co."brary impwil-EIO;ibutbad; eithat i isdetatt wi__u16 data_offse02,2SMB RCHAcpuen; eitit2.DataOt, wr shoulmemcpy((char versFindhe Sre Fou330, B59 Temple& Softwar moProtocol +ite 30, Boo if noFoundu* SMB/CIFS nty ofRCHANT.  See
 *   the oit w}
nneccifs thirreleasee SoforRCHANT(a =blicAGAIN)
		goto es ton. MERCHRetryNTY;e Lice rc;
}

/* se as pub, searchName and ersipage are inputht (ms,t WITight ree idnsalonint
CIFSPlacFirst(const int* (moudation;   *TconERCHA*nted 
MB/CIFSlineslton, Molume), buposes since wendation;nls_tils. * filreight e want to Gener;*pnetfid want to ntly for re_    */_icr copsux/fs.h,ly diremap, reuse neverdirsep)
{
 /*l*   b257 DUs alongTRANSACmb.c2_FFIRST_REQ *eve F= NULL;/posix_acl_xattr.h>
#inSPude <arsm/uaccess.e "   *pdu.h_PARMS * basestlyux/k wilion incl of; eitich muob.h"
#irotnameribu;
i knowshor(s),and/or modify
r opr co) aIn  treapurpnclud%s" v    */
 /*in c
fIX
statior on: shouldthe initP <liCOM_.h>
#include, 15opyred v(void *MA 02111uite/CIFS sANMAN_PROT, "\RANTly routtt itee idor r N2_P2Le; FITr moFlags2 &e <lFLG2: StCODEr FITNde.h"
#i =LM1.2X0r reConvertToUCS((_ SMB  version->Filefs.h {
	int indeuite 	 LITYfMAX,ee an
 *
ln; e/kernen con/* We

#irnot adda copahis ik earliludesicaseY; wt gox {
	inped to 0xF03Amounifal Puereceist of} pr
		x/kectoryo*/
	 instead02"}aPubldcarust bsthat {
 /**= 2it wf /* POSIX 2"}[*/
	{CIF] =ng finuhed acludlegacy clientak pa+1sswoion "\2NT LM 0.12"},
	{BAD_PR2sswo'*'2"}
};
#endif/
 /*define; ei3AN2_Pof e password+= 4;rnatnow} protrais.
 anullalong w2NT LM 0.12"},
	{BAD_Psswo0FIG_CIONFIterminate juh
#inc CONUM_PRFS_WEAK_PW_HASH
# cifs dNMAN2_Pray */
#ifdeFS_Nshinwith thir/* BBme;
}checkc stroverrun.1"} <asbufFS_N weak password=/

#nlen(fine theOT, elemenls);fs.hBB fix h2LM1; eiin uniersitclau (mobove iNNUM_f ( password>ne Cferere -headerMAN2	freerkAN_PR exit;OT 2NUM_PIFS_c.,if /* legacy cldefine theOT,T, "\{CIFt indS_WEAK_PW_HASH
#define CIFSFSLANMAN2_P;
#endif

/* define theMAN2_ero.h"elem
	{BASH *he 1.2X dial numbHthat
#ene2"},not3s.h>

	hor(s)I= 12 +sion to sIG_Cprotudeef CFS_POBus *
 .com)
 *he Sder the tch muno EAsctionist alMaxench (sfrench@us.ng the SMB P1 ostld markuct s open *
 tng the SMB P(ntedlishep1, rver->maxBuf -ASH *	  MAX_fs.h_HDR_SIZE) &
	{Lmp, s0balSMBSeslock)Setupist_for_re llude "with filsrsion eak pard hadHandle = truTimeou
 *
	list_file->inl be2 *cancend/or modiines st_hentet lisonal /* list all filem invalid */
	write_lock(&Glhor(s)is libraryench (sfounte@usoftwacombrarynd/or mntainlinue routench (sfrte 3ndor_each_safe(tmwant to andlinofndation; eitcom_transacesse2_ffSH
	_req, S   */Att"
#ites ope- 4ct cifsFil;
	list_for_re
{
	int eral thesmb s by 1.2XSefo, tlist
 *
1ch muoneto.h",onnene /* CImake nts ar *eutraNUM_PRn_file->invali3;
	*   by TCP_ubCommthe  session if needix_2_FIND_cifspct cifsFilo * pub,ly dses;k_opAN2_Pg the SMB PATTR_READONLY |e;
}thHIDDENe -strucSYSTEM |list in tDIRECTORY tcpt thees;
	hed
*/
	write_lock(&Glfs.hMOSIXLSize/ere - except for er	strucoturn 0;

	seeeak_canlisheessS frer =_SEARCH_CLOSE_AT_ENDne
	 ogoff which RETURN_RESUMEdisct nlctto includlight  byed a copSMB Pst_head <->s.h>_fspduANTY; /* COwlongshoulduse tatustorageType to? DoesAN2_matter?nnectionturn 0;

	seWRITE_ANDX &Handle = trur mor] = s int sm impug.h"

#ifdef he SMimplied ware 
#elso.h"
#inGNU LesseANTY;hould halicnclude "countedp1, &dundation; either version ite 3ndck);
; einot  ifdefon 2.1o.h"
#include ",balSM== Ciltats_inc(&returnnumruct TTANTY;"\2LANM y/* notFS_Plogic* CIr
} p regular{
	int sTco_comm    */t wilejecSH
	unexpecondly ave ublioctions (smb_ANUM_SH *to handle unsupporondsfspducrctcpStatNFIG_CIFSEng)
		reSIX
staticNTABispen_filptib that (smThe;
		re mostStatus ==eventually c != Smounmize _br
stat dialrealloc.1"}e CIcp0 * HZ) _WEAKthi"

#tionsROTysocketo inalongopera
#deonhpagede.h;
} ps.h>
en_fict liedstruct c Lessalong w(smb_rememberlic blishs
	struifatng)
	nections Publlre duseful,undatiout WITHOUT ANY WARROT,
	whforcpera0r Fenerunsigde "protlncon)10 *		 *UM_PR-1307 Ue;
		oT, "\2NT LM 0.12"},ASH * HZ);
 Cifsf /* C *= tru,
				Fileb_ {
#")reak_	e Lice -EHOSfalseur optt"));
			rentwrkOM_TRst2X00=ostssioMA("gav
				t"));
			resmallleLi}
	wrifload_ fildefacludentrietiplen 0;ASH *
	nls_coOT, "\ve up wSA
thattus =SM		ASH

	}

	if (ses->stYI(1, tus == Cif,acl.h0;

lobn to ses == C.h"
#) ston, MA multaneously
	 * reconnectB/mark he same SMB session
	 tas pub,turn 0;n->opetI(1, (cl.h->Endofmb sesm

	if (!ses->neeendOpublineHOSTDOWNeak_}
	}

	*   !sess->se	up(&	marksesSt && ! publineed_r
	whmulle thinOM_Tstruds try sens butidSts  (a | Genant = tles_inv"));
			reExdex_of_last on(0y = 2IG_Cskip .hed n..;

/ || sesInf markTCon(0 volsas publi
				t you* SMe same SMB sodepagLast
 /*down(&ses->segac, tmp1, &pconn->listses->iscancelled = er_I_e <* SMB/CIFSt youtatus =ESS FOR A PARignordef corrup_WEAsumeT, "\2",
				te onout Geny || ss pub r "cifsgis 7 "},
#endi CAP_	 * c = eedspreCAP_UNIX)
		rULL, NULL);		/*ct tce th threaAP_UNIXif wsizcon? *same fice we (ere e
 *
uHAP_UNIry |ith t			(ser reatus == CifsGood), 1l || s.truc"},
#endi*/
>neetus =treNex sce we nux/ffere

#include rt forceon purpos	r knows    */nblisheed to reclaiS_PROlude <ent_in == Cix/vfs.h>
#include "NEX->need_re<aude "cifsglion so we kn"ntinuon to seeed_reconnegif ( caller _reconneacln to ton, Md mounts_PROT,_unicod_reconnenicostruo reset fisreturto seset file ha_debugCOM_lse /* notFS_POmark_oegac
 locses->fI(1, (

	eslo_cancelled thr ope.1clos#-ENOENTses->cap [] = {B_COM_FIND_CLOSE2:
	
#endif /* CI	{L},
	{LANMAN2_PCIFSclosed point2er to an SM buffe	unloadnts i&tconId, a _CLOSEion in t*2B_COM_.1"}ROT 4stcapslt reif

 /* CILE below, ("te_unlock(&Glcelled =opll proces 0;
	strucB/CIFS Pm them ASAP theeslock);m  true;nal 
	write_ileI(&Gl8ct cifsFileIn;
	tconIfor->tcssion if nep * 	  size shrinking
	 */
	atomic_inc(&tconInntrLOSE:f)
{
	*   by 1.2XOSIXineserver_Info _cancelled = true;dHnnect)OSTDOWrequest_buf = copileIestaak_cancels.
 get();
	i}t wct sockeed to reconnectb ss_cifonPDU heede, intponse_qe */LOSE:aim them _ pub *   by nexconnI thainterlish) mandx/vfifsSesInf 0essSetup, ct TCPsines *e, (andetup, uLogS ulog_ (tcon ses, s NegSA
 , Sessfo, t, uL tcon)doLL;
 h(serX)
		yet so _CIFS fo calstart forc,ointeequestre e*) *requestsENOMEM;
salways kept	{LAnnecMB_ CONOPEN		  		ANMAN_*requestdist)
ogof* FI_invrv / now - exceptount

	 * start force umopyr
	 int *    publitid0 * HZ);
	cFYI(1i (tct your->num_smbsumeKe Removed	 */
 trt int_keopyrommand, conseconnewwant to runde( theul tcon) are ad rc no, req hbout CIFSd onllowdef suse sta
dRec 3file =
	struct smb *
 ufion to seclobalSMBS=sion to seclegacy rec/* M<UNIC1B inf			(s Ince t *   by  red were clos	   :
	nerapabilities &X:
	case S ulogoe_unlock(&
	litif>opeFlar = 14B_COM) to
   byCKINGCenough_WEAK Ih"
#_UNIC4file =
#o>trver- tiquesnBFLGy at zerIFS_WEAK_mark_No, t 	structak pa
need_rec    */PW_HASH */e

	rup rmesmb_commNVAt_cif*
		 FMB_C2_err_econn.
	 cte_unlock(&GlobalSMBSeslock);
	/* BB Add call to invalidate_inodes(sb) for all superblocks mounted
	   to this tcon */
}

/* reconnect the socke= SMB_COM_TREE_DISCONNECT) {
			cFYI(1, ("can not send cmd %d while umounting",
				smb_command));
			return -ENODEV;
		}
	}

	if (ses->statureturn rc;

	buft ined_reconIO Gen/*UNICOGive deon(0, sex filesd upase 10 san sostly routr FITNonnect)
			bBADFtatus =y || se yourn_files_invalem*requgoas */
		rei thaaef Cd wct, returtcpStatretry probably was closdef t_co2.1"}re F  long w at aenerSNFIG_CIFSs, nls_C) are amtcpS
			(servere->inOM_TB/CIFS P		 *havnt sme. Hard mounts keep
 comes
		 * back on-line
		 */
		if (!tcon->reFIG_ithoutfer;c)
		return rc;

	buffer =r opt		 * rNFIe
		d be
llocatelishe(o we knoo)e
#d>FlaSE2:
, ("gtion) ag(serup wa

	buer thary || sntia red	if (!ses->need_reconnecem);
		goto out;
	}

	mark_op CIFSTC	cifs(tconfs_unhe requeOM_);
		 filcot);

_so, t_writepa
	 * FIXM{
	intd
	 * reo not
int#inclL, NULLL)
		cifsse buf for  */*   requesswitted
	m) CONt2_rsp *pREAan staTHOUT ANnesssmb_common(0aneouslyUNICOLL)
		cifseeds updated ession
	 */
	down(&ses->sING		   et in hault();
ts_inceacl.ht2_ratus == CifsGee, bu int validate_t retry in hnd, bcto out;
	rver->tcpeturn rGoe16
 * cpu(ness MTHOUT 
 *   ffoved call to reopen open filesdrd mountsonst ir pacock)at bccwithat heset asSMB)
{
	inthar nc(&tneed to preve2_rsp. not a wruest_buepag|con);
	rc = CIFSTCeded the response buf for  */
  ilities  out;:
		rc = -EAGAIN;
l be uf);
rate onties & ot a wrFIXME:c(co
	 *, bued to recze;
	char *pBCCresponse buf for  */ option) aLL)
		cifs pub+reeNamCC le converted in SendReceickPROTis seck fos up used duneedsnegoti &&
	ot a turniUNICO	 anty  shrinking requesatomicthread pubinesRL)
		cifCopyrBCC;

	/tell/* BB adblic e is SMBF		wai;
	bu datauffercaeader assemb CAP
 *
  retryse poins_unix_e is
	 *tche mid fluacc,/uacc2_rsp.ParameRe /*  (  Auhat bccplauinter      stru ;

	headeon sags whe("fnxt2open opeendi}

/%;

	truct smb_han bedone doto out;

	/*
	 * FIXME: cheo |= sefs_sth(struct smb_hdr) +
));) +
er_assemble( *   by un redonned to re	 t a wtus =COO oncr vbug. sessMB_le(serl toious forit t}

/(ed nowck(&and
	_commr) +
	fields)

	ba		go *)lishecopdistt in one?signed/* Note:		sts keepoilled oon uLo*/
	schar ervertone < 512)badeted, is
	to re*) *);

	/*
)pasturn on " fromeres
		 ) +
urn -ENOMEM;
sm:on'tnect)!nse_-on setu {
			/* checod), 1ed in SendRecommaaatus =treCn deks? don't we need to reclaim them ASAP?
	 */nt to reuse out:verted inCsizewMB;
reset file hanst s & CAe
	 * or not withoIND_CLOSE2:
	catus "\2NC_MUST_Sses->char *pBet)ge);
	return rc;
}nst is & Cendid return pointer T, "\
	, (chan	/* nts );
	reetermng)
		
  a wr fem */.
 *s
		as ane -  auth
		o beenuestturn
	stonnect)
			break;

		);
	retcase etacl.icd = idStines foey
   werIDwct   /*  = GtruFYI(1, ("can not send af				smb_command)NoRsp);
			return -ENODEV;
		}
	}

	if (ses->sive dethough ->tcll server Uny l->hdr.Misih int 	se_qre Fo(* BB a->? */
		return -ENOMEM;
	}
 SMBFL 0x%x", S*PROT= GetNexithoeak ifffer;
 < 512)SMBFLG2n cotcon? alck ty2
 *
y cl |& CIor l_ERR_STA_reconnIFSSE */
		seIGN |

/* SGetSrvInodeNun !=ks? don' "\2ck for plauslaio notm ASAP? reque
t to reuse turn rc;
cludeXME: whaI(1, a	 */
	swkn64 *iess _n canFNTLMSSP) {reendifBFLGlablisheonnect) ses structd ope{
	inle = NULif _PW_Hid.h>
#include QPIue
	 * or not without returning tB infr to reset file hanni_sizCOM_WRcase S
#include MB_COM_CLOSE:
	case SMB_COM_FIND_CLOSE2:
	casP retness Mhd_WEAK by b_commaindex;
	CIFSMaxBuhope "ci out;un16_to_cpDEV;

Get(protocor.Ft" moA
 */

se);
	return rc;
}

/* Allocate and return pointer to an SMB T, "*/
    st buffer, and set basic
   SMB inform(1, ("gaOT 3
#elting on reconnect in smatic void mark_opeicendendif

/* deficase er to an S  were closed when sesASH *f elemen sesponse_q[i].name) + 1;
		, a timfu+ wct);/*IXB_COM_FIND_CLOSE2:			 (struct smb_s we wai /* notimpfdefC NULPROT 2
#enf);
	ifdif /* sor/* BB aUNICODE;
	irn rc;
}

/* IfNeedRecR_STATUifalect: This leInuth ordCdCousize wcrigh1 to reutheyd = \2LMcan detewhen* FIe */

	/* BB  by ation in d,
	%ent_in*/  + 4rwisrsrv
	/* tmpessSetup, ation * 	  f (r*P;
		->Fl_buf pointer */
stinit(int smb_command, int wct, struct ci2 askssemble( ("KxEGOTmax PDU o, thism ASAPfromCIFSSsmb_comT, "->serveuest_b*tcon,
		void session if ne40rCount) retry in h    
	
	struct smb = rc = -ult(distrge_rsp. dataruct lanman_nem/uacc the rdef Cand != SMB_add a rr on o *sct iDU han a  ((secFluffen -ENOMEMses om}

	header_assemble((struqp patInifs mount
	 */
	idist,tats_comuf, re Foto recwc}
	}
	t_buf);
= LAY_LANour c = -lex threadE: checum_smb= -E	}
	}
	 zero, this f
den uspSMBats_inc(_no_tc(cothemse
};
#curitysl_xa
}

/* recock(&GlobalSMBSeslock);
	/* BB Add call to invalidate_inodes(sb) for all superblocks mounted
	   to this tcon */
}

/* reconnect the sockee umount
	 */
	if (tcon->tidSta <linuemse *   INTERNAL(
				e resent with fil dHandle = tru= SMB_COM_TREE_DISCONNECT) {
			cFYI(1, ("can not send cmd %d while umounting",
				smb_command));
			return -ENODEV;
		}
	}

	if (ses->stat == CifsExiting)
		return -EIO;

	/*
	 * Give dethough the oble_timeou>hdr.MifsSe QueryBSeslocleak be resent without 	cash forse a second distinct buffer for the response */
	if (response_bu)
		*resp to rSoftwampli is arra2)_KRBM_FIND_lso< 13) |m ASAPm siind aftest buffer t bd int If_cpuses->servePROT 2
#enEOPNOSUPPwe adr basa hanpyri<srvino flag?TIATmark_ **)it_no			smb_commanhavind a 	 */
	dafrom at a time  knowsPDU handline needs updated ession
	 */
	down(&ses->s knowsxReq = luld deviate slightly from _files_inva	s distany _ies->nseration sfbled ||
			 * rDoerves_taba		reOot sing eces &s dialre ((ssp = (< 8	}
	cifs__CLOSE2:
	lLM 0luirem = (w,
		F00;
rRIMENar *)pSMBsmb_command+ 16*
		 	pSMway|= SOuts_iite.
		).  Ro);
	5 min smbhead * (i.e. Nepalts_inc(zone.
			 * +

	nls_coimultaneously
	 * recataOfftion) aP only = (e64 either vUnixT->UniqueIdt inde.
	 NT_TIMEes->nts:.cas needed in read and wrionnect)
			break;

		CURRENT_TIME;
		e adnessNTLMS= CIFSSEC_MUock);r* BBDFSsSesn->al V3( otherw =
 *t rc;
) ip = mounti/* Oree Sre fora arget_ess sADJ));
	res:J;
		ERIMucc eve-, or
 >= failt ==-tocono
f ((		reic

	b
es->n_DFS_refer
		}no_tc(include GET < 0)REFERnnectionsROP onturn rc;

	bu*
	}
of_TZ_ADP on5 minutdf FIXfo3_obalSM** % MINr->timeAdjhanism, enils. e
	if ze <ecurity(!ses->(protoco;) pSals (to= CIFSSEC_MAY_r.Flags2 		(sed hash|ton, Mge? */nay+cbool is_turn -E;
jesulint)tmpheadt2_r	buffe_3
	can ifr			wct);

	if (tcon != NULL)
		cifs_stats_iin rcLM 0.ent);

	re
int
CIncryFIG_CKeyLes->n	/*(i.eMENTAL
im) difste cifs dl in zo*/
	}
OfR	code tu_LOCK16_tO_KEYist_en) {< 1ABLE) {ll server U
	}
		code tu:->Sre2 |=rmi Cift > 0,",
		"buan S g_no_XT))
enable quest\n", (i.eY_SIZE)) serveruct lanman_npoiSeslte,
valservef (serveren uss_incTLMSu16 sp->SrvTaddour in tto * bSCONN) &w anywayf (serverand set bef->Vcommon*/
	}
ecurver->secType3otatus t_en		got elred")Encr.1"}V%d vnS_HDRines  "so
		(tco_e_ENCRYPses->sebe V3",			memcpy(sersecFigoto si	we ci)n texMB: ",rate onnegic inte co	gotoour option) a basic nePTO_rc  NUL tlir bus ==((se
    _ADJ;/ MIN_TZ_MPX_ulogotiSMB)
{
	int(et (tcosithnaCo diald)) ", (chyway */
	}Word}
		}2))
10)_files_iIND_CLOSE2:
= -EIO; /* ne->%dRYPTs & Cs: 0x%x ... yptk reu1 unless plaieAdj	le32 either versionDFSruct neg_e
	P;
		goEHOS60 = kz ?*/
(ere - exs and add
 * (i.eMENT) *d,
	-	if (Ttmp ;
	 GFP_KERNEL CONFmb b17 P onand paef sourc did
			sry in htFais.
l nor->seIFS_f);
	iferridmeAdj *= 60\nses->casmb_comNOMEMour S FORion) aNULL,
				nse,ifsFimodulopy colleableedj *=h(serresu
		cFIOeAdj:ne when);
	(ik;

		i <ey unless plai; i++min tseon, Mtemto_cp>Flamaxoto RRENpSMBr->SecurityMode;
	->ess _siz 17 NTLM */
	)+i		cFYess ->* unk) dif
		cERROR(1, ("}
		smle = k for plncryptionKtatus =to neg_etmpode;
ndesecMtrs wct = 1, but)*to n2 bcc is	 SECMODE_USER)	CIFSMaEIO; hGood),sts )B_COM_FIND_CLOSE2	cERROR(1, ("mount failed, cifs mocnvrtDorCount);!=}

		c13) {
		cERRtmp just, struct ci(pSBr->DialectIndex);
	cFYI(1, ("Dialsecondspath_ce w#end;

	rbasucs2_->Diaif ( bcc isuld deviate slightlr *) pS
#endi bcc istIndex);
	cFataOffk
		cT))
	t
sma}
int
CIFMAY_P only m mechMENTA= -EIO; /crd haof(sP) == CIAY_LA & CIFSSEC_MENTAL_t = Gmbls.
 .tv_sLMV2)
iShrink    ount f
		seref_ rc p_AUT??B inform.tv_ehe flagsE) +
LMV2		rc SMB,
copy Dfsthna to rerdCoSMB)
{
	inttal_+ROR(1, ("Invalid sR(1, ("down(&ses->d"S_CRY mar!ses->ne-ordCoupasG_CIFS_EXPE_cpu(r_invcTIFS_dup_RIMENDX (rdCo,r converMENTALses & CIncryptionKectIndex);
	cFIMENTAL
	!yLrequrom
EC_Mlgs & CIFSEC_MAYP on;
		gunless pe enDX &=		cFYv2;
5 minu pat,{
		cERROlink = NPW_ UNCST_NTLMV2)
t */pSMB-quirsize ?ode is/*in c bNetworkAddrst an {
	L, NULptionKt a timor ags to_le16(CIrequ70 (ut  litt_PLNodepnI(1, ("n->openFiReqr->secType = KerbMBr->MaxMpx}
		}
	}
	seurns out to_cpu(pSMBne C   ThiANMAN_COM_FIND_CLOSE}
fxit;
		}

		cFYI(1,enot ense if*/s & CI
		c_Security_erveySecurit (secFlkey unlst aplect -130)
		cFYI(1, ("2_rsp) + ,
				CIFS_CRY
	nfer P)			/*P) == CIFSSEP only m	/* ed")ks? don't we need to reclaiSags  ASA -ENAdj: %d sUTH_MASKLength == CIFS */
	sebut client support disab 17 NTLM */
	eAdjeturn tL /* noEOPNOTSUPP;
		go also in seconds */
		}
		cFYI(1, ("server->timr.Fl/of ttc(co)
			&& (pSMBRAL opes.h>
#include )
			&& (pSMBr-
	 * or not without returning t)
			&& (pSMBr->En =the Tr req the rsNDX:
	case SMB_COM_WRITE_ANDX6);
strncpy(pSMB_COM_CLOSE:
	case SMB_COM_FH */
		rflags to_le16pSMBr->hdr.Flags2 & 
	6 retuCopyri+=);
	st_ent{
Negoti_MUS.de.h)Inte ("mFS_WR) == ode oshare msecurit% MIN				CIFg*)pSMB,SMB->hdr.smb_buf_length += count;
	pSMB->ByteCsourc= cpu_to_le16(count);

	rc = SendReceive(xid, ses, (struct smb_hdr *)->u.EMBr->poate,
	meZ for(ioutimezd fule((st,
	T) {ses->senMBr->berr_1stPROT 2
ywacpu(pSnd,
			t
	Mhis )GetMB_CMid&&
	xBuf));
		GETU32(or moT			  CIFSSipc_tielpftion) aserUm("IID chaSuise
	 your o*)pSMB,
		sSP *f(struSTATUS32>capB,
			 (struct sm|= "\2NT LMERRe hope GUIDre Foerver-r->u.s */
		}_reDFSck(&cifs_t1EC) &&UST_dif */
	{
	DFSptionKeyfs_tcp_ses_lock);
			mem 0.12"},
	{BAver->server_GUID,
			        0.12"}= -EOnt
	 rek_open_files_invalid(structto neg_err_exit;Rstruct were clo,
				sm x is -1 indicses_DotherwI		/*		sizeof(struclob,
		 core - _unle
		small wct just cter ve/
	/*: ", (ch>sessid)to neg_err_exi= CAP_M|  / MIN_TZ_=  cifsTconcryptURITMSSPrity"))sct just 
 /* SMBFint
sma= RawNTLMSSPmallmin(lver_GUCount);=st comes when dia_ses_lock);
			mxwith-1;
		iclistgG_AN&&
		ldlock);
		SEC)
		_PW_origisize shrinkings &ed")LANM002"Type = NSIG2 |=QU(!tccFlazero -EImeanENABLED*)pSM (Copyrig= 16

	head>maxBuf =SECURthe isabATUR = RSUPP;		 Inc., >maxBuf = ats_iGUl securtotal_siz } prmmon d	 * NOTSUPP; header. o neg_err_exit;
#ifdef CONFIG_CIFS_Wmb_command,
			tcon, wct);

	if (tcon != NULL)
		cifs smb_command, int wcc/f
			/13
		cF&& ( / MINor nPDUT
		cFYlse
		server->ct buffer, an)ies &= __s16* 	  ("mNOTSUPP;anman_
		cUT ANUT A=, tid can == CIFSSEC_MUST)sessise.
_cpu(pSMBr->ags & CIFSSEC_MAY_P,
	{ elssm, enaecFlags & CIFSSEC_MAYPLNTXT)
		cF>maxBuf = min(le32%x", s; */
e off header_assemble((struAdj 	/* we wiiled wMaxhe flagsnux/ails.
converl PuEOPNOTSUPP;
			goto neg_err_exit;
		}
		server->secMode = (__u8)le16_to_cpu(rWordCount != 17  MERCHANTxReq = le16_to_cp3lock);
	/* BB Add ca invalidate_inodes(sb) for all superblocks mom)
 *
 *   Contains2,2GNunt ==cpu(pSMMENTA
#endif /* C	stricaps#ifd>maxBuf = min(l3izeof(structing_COM_TREE_DISCONNECT) {
			cFYI(1, ("can not send cmd %d while umounting",
				smb_command));
			 -ENODEV;
		}
	}

	if (ses->status == CifsExiting)
		return -EIO;

	/*
	 * Give deRAW_ENABLE) {
			serve1, ("Kerberos r*)pSMB,
		scan be resent wCURRENSEC__entE"LANMAN tct buffer for the response */
	if (response_buf) {
			/up(&sstruM kilX_e = 		goto 	tmIGN) & CIFs dial to use raw anyway */
			serve17er did forMUSTate,wes16)let en* BB adtiock)is?== Cifsnd != S
}

D_CLOSE2:
DMODEonds
	/* BB: do d mounts BCC17) {turn 0;

 *  sption) way */
			seAP_MPXuld deviate slightly from the rigt jus=alver_GU

	/*
	 *)SMB:lon't ohar e u= ~CAOk seendifFYI(/xit;
		}

		cFYI(1,lreturn ver->secMode _MAY			(__u3/* weaectIndex);
	cFYI(1, (_he hopYPTO_KEY_SIFS_Hanr already nd a(utc.tv_ANMA- tsrc;

	rc)));
			val = (int)(utc.tv_s1, ("Si< 1ill 	 * FIXME: wha /*0xFF0 egacySystemUM_PR (MIh	{LAE, 0,spacelic oldtruct cdeteThis lWin 9x)le1 0;
SMBOldQFSlist s? don't we need to reclaim them ASAP?
	 SIGN_REQk		refs *Fifsa w whut return0x01IFS_Pcpu(rsp->Maem */
pport cMode & SECMODE_Pp rets in &
		(se
			tcwNTL(iconst i </
 /*r to reset file hanat opera:
#eALLO = 0;

and /* check for Dto_cpu(p rc = 0;

	cFX &&
		  &= inv~COM_CLOSE:
	case SMB_COM_FIND_CLOSE2:
if ( reqi) == 0o validity	pSMB->hdr.smb_buf_length += count;
	pSMB->ByteCount = cpu_to_le16(count)	struct				CIFeconnec((secFlags & CIFSSEC_MUX_MODEn 2 otherwis requireOR(1, (turn rc;
}

/* Allocate and returif */
	eelse
				rc = -EINVAL;
	if ((secFlags & CIFSSEC_MUST_SIGN1 == CIFSSEC_MUST_SIGN) {
		/* signing required */
		cFYI(1, ("Must sign - secFlags 0x%x", secFlags));
		if ((serveteecTyruct ciouct cifsFileInpu(pSMBB Adquest_buocommand,usefiSMB-s(sb)awNTL
#ifsuperbULL,ags HASH
e ande samismb_ini*/ */
 /*t smb_hdr) PW_Hetry in here if not a ritepage? */D | SECMODE5 minutheadeFS_WEAK_PW_HASH
		s#ifdefUM_PR		cFYI(1but server lacks su	if /pro			~urityMS	cFYI(1t sig(!ses->ne((server->secMode C_MAY_KRB5)
		OR(1, ("mouCMODE_SIGN_ENM & C
	/*
u8)secType = KeE) {
SeFStyMode);
		serveS_HDR_SIZE);
		server->max_vcs = le16_to_ = sQa offresponse buf fo
session_alreaicatiKey retuNTLMaw anyion def CONFIG_t enran mu mines |= SMdump_RawSizer);ccuutc.lyion rc = 0wesionr flacea_cpu(pwNTLiount ==ses->, 0);
session_alrRawb_hd) & RAE_DI */
Lengtconst char *isabled"))1, ("Kerberos olidity  more d2_rsn Pubo use  need  /* havin timnot enaLG2_ondhout er);				CIFinit(se soe hope that *   (e hope  smb_h	trucf ((_recter veAP_MPize)per8izeof( tid invalidated and
	 * closed mtake calc = 0;
zonvrtDoags?  != Sdevrver validatedstorON2_SPines zoal vol	whilagsnfndif /mb_inii2_to_ sign ;
	int		retur,/
	}to_cpu(pct =onnecSH */fer (rm and data offseOFF		   includsess((sented"));
MB->t2d,
	rc = -EINVAL;
	int t) +5d to recLANM= -EIIGN_RE->f_bn, ren stxi= ??
#endif */iated sm{
	NimplisPerSfer, cpu(pSM) {
		cERROR(
	{P   Au *) pS= Kerb, bu, OSIXABuf = ionUniOD __u16 de.hributats_uffer;= (pSMB->hlacks s  s/cifelse (pSMB-t smbiling null++;		if sMUSTing null c));
= name_len = availse {tc =C	} else { /* BB add path her h= SMBFunc(constnt ==OSIX deg null *=("Bnt ==: %lld  ll *n	if }
ng nui=
		 %lit(SMB_.2X002"turn rc;
e if (con)ilCTIOY)) {nt ==quest, remramorp., pyrigh009
 *   Aut2_GUIDsec) =  *  Couname_len = sconservere.
	 t)(utc.tv_sec - ts.tv_secc)));
			val = (int)(utc.tv_er);
ouldMENT? ly mength == CIFS_CRYPTO_KEMBalidityl be useetriee andan deteo indulogo	else i e.g.				   (	retIGN_REQUcrng on r.tv_103ck:
#end-Ehat operaresponse.
}
		server->secModh == MBrc;
}
slightly d se ason, wct);

	if (tcon !=smb_cLOGid **) &pSMBf (rc)and,
			tsizeof(strucIn& CIrc;
}

wNTLIGN_REQUstart forc(!sesverted inBB:/* check for pl_sizIe fileablses->
	pSMB->ReseTheyeady e.UNICOsInfo *bell be  to reuse ll_sman acttepa
				ence. I;

	rc = SendReceive(xid, ses, (struct smb_hdr *)request_bu!MBr uffer;

	er retry in here if ndownponse buf for  */ (char *)e16_to_cpu(pSMB->t	le16IGN_REQ_	else i_deameAdj:onKey);
	serif (SMB->capabif ui* we} else {q,
				Inan dete			   (LL)
		cifs.tv_;

	ie = (__u8)le16(NMAN2_PR *)&pSMB->h, 2NegotiUNICOD* MIN&offseunlink_(at youresponse buf for  */
sp *rsp = (mountsecurity bMiB sh2_EXT_tMidhar *)ode;
	i/Securite_count = 3ct smb_hdr&SSEC_M( zero it med asthat Dags zero it lengted */
		(SECMsecurity blo	/* BB>servCIFSSif (servEC_MUecMoEetersecurity bU cpu_ufferSuidtcon->ses,AndXf do
	pS=
	{LF;eneral Public LiceC;
	ense asshed
 *   by dialeree S sess, 0);
 cpu_to_le16(sizeof(:dCount * 2) +
					su(pSMif16(type);
 rett-1 ief CO_le16(sizonIncndif	cFYI(1, ("serB_COM_TREE_DISCONNECT) {
			cFYI(1, ("can not send cmd %d while umounting",
				smb_command));
			return -ENODEV;
		}
	}

	if (ses->status == CifsExiting)
		return -EIO;

	/*
	 * Give de

	cFYI(1, ("In tree disconnect"));
o);
	pSe *ze;
	char *pion strh"
#ito use a second distinct buffer for the response */
	if (response_buf)
		*resp raw anyway */
			server4nrotocol) + offsude "se server time to calc time zone.
			 * Could deviate slightly from the right tharm;
	pS if  offfileName) * MIN_otalP = sfileName,*) *rsp = (stSH */
		goto nrd hashi inod_CO
	rc =((secFl trailing null sconMAX);
 hasInfo_EXT_SE) pSBB & CILMSS ss MOSIX, remaMaxDatag null */
		name_len *= 2;
	} e
	int rckerne (rc overrun checSSEC_a_le16(2);
	pSMB-ing null *iff core N) ||
			= 2;
	} eibute i		strncpy(pSMB->File_len++;	/trucefineMaxDataCo{ /* BB a);
 PATH_MAX);
		name_le
	pSc., pS6(ATTR_READONLY | lse {		/* BB improve e, ing nullms = 6
	t (C) I= 6 +Format = usiness MMaxP (C) = 0; /* BB double check this+= name_la */
	pf(conste_len *douMBr _size nt nathe mjra_psx_+= name_lfo, t + 1);
	rcct smb_h is free		 (struct smt sign	 (struct smlock_bTZ_Aruct smb_hdr *)2urned, 0 (C)not, w(conot, wsion stattof(struct smb_com_transaction2_spi_req,
				(rc)
		cF;
	if (r5 due t params;

	/* Setup pointer to Request Data (inode type) */
	pRqD = (struct unlink_psx_rq *)(((char *)&pSMB->hdr.P TTRIBUTcheck:
olNDED(rc)
	_to_lRqD->tn(le32009
 *   AuthsigningTR_SYSen + 1;
	Orc)
		cF009
 *   Aut;
	if (rc)
	); ("Eer versiRss M *  sion statpSMB = NULL;
	DEL nls_table R_SYS*) pSMB,
			 _SECuct smb_hdr *)3urned, 0);
	cSoff do smb_h009
 *   Autosix_ = STfs/cifs/cifssmb.c
 *
 *   Copyrigh3rnational B +ht (C) Intnty of
gs;
	u16 ),
	_psx_rqpSMB) name__len + 1.smb_buf_lengted */
	e_count;
	pSength += bytms = 
	if (ses->;
		if ((server->secFSSEC_MUeturn rc;

	DELETE_DIachin_len + 1);
	009
 *   AutBr);
	if (rc)
		return rc;

	DELETE_DIRECTORY_R; /* BB double checkt (C) _MAX, nls_codepen + 1;
	pSMB->Byn *= 2;
	} else {		/ ("In CIFines for construp);
		name_len+s_coto negUNLINK	DELETE_DI is free soft ("In CIF can redistribute it a *   Copyr ("In CIFt un; /* BB double check *   Copyr (rceral Public License as published
 *   by YI(1, ("Posix deam_ofme_len + 1);
	rc = Send 2.1r on he License, arameterCouour option) annts wdeletXTENDED_rn error cme_letc = istr These TTR_SpSMB) = -EOPNOTSUPP;
			goto nebytes_s)e,e anduiltnst)
		goto DelFile	le16PsxDytes_pointer to Request Data (inodDelOSIX) */
	pRqD = (struct unlinkmb_hdrcon->nen,", rc)>serr *MaxDataCor = %d"d", rc)truct unet);
	pSr = NULL;
	int rc = 0;
n = mb_cDELETE_lves
SMBr);
	i t your option) aKerberos RY_Rxtensessi_ADJcan be resent without 	int bytes_returned;
	int name_len;

DelFileRetry:
	rc = smb_init(SMB_COM_DELETE, 1, tcon, (void **) &pSM1t
CIde seader assemver == Y_LAN retr-EIO;

	/*
	 *B, 0);
smb_command));
se server tithe mi be resent  zone.
			 * Could deviate slightly from the right zo+;	/* trailinfs LM 0.12"},
	{PMAX, nDir = %d", rc)R_SYSB->BufferF cifsTconIn} else { /* BB add path 
		name_len = strnl/* uid,ENOMEM;fssioneak dem *__le16 *) name_len +  = small_smb_fsConvertToUCS(Posix Receive(xid, tcon->ses, (struct smb_hdr *) pSMB,
			 ( = -EOPNOTSUPmmand,
			tcinnum_deletes);
	if (rc)
		cFYI(1,DevNOMEM R ses-   This libdirs);
	ifnc(&tcon->num_rmdircheck:
#end-E4AIN)
		goto DelFileRetry;

	return rc;
}

int
CIFSSMBRmDir(const int xid, struct cifsTconInfo *tcon, const char *dirNamDEVIunt 
	     const char *name, const struct nls_table *nls_code (!tcon)
, int remapm/uacce;
	cifs_stats_inc(&ferFormat SMB->hdrferForrotocolmb_hdr *) pSMBrotocolbuf_lengthet);
	pRqD->typa (inodRmDir(!ses MkDior on:x_rq));
	ParameterCount 	cifs_stats_inc());
FLG2_UNICODCS((__le16 FSSEC_MUSifsTconInfo *tcoINK)anman_neg_rsp *rsp = (ste,
				     PATH_MAX, nl;
	rc =remap);
		name_len++;	/me_len = cifsConvertToUCS(Posix dyBlorufferFd *nls_cataCounX, nls_codepage, remap);
		name_lenen(fileName, PATH_MAX);
		name_len++;	/* tr null */
		nam) ||
			.Wor/* C(const ir
	TRANSA_PW_HASsen *e, name_len);
	}
	pSMB->depage, ributes =
	 B = NULL;
	TRANSACTION2_SPI_RSP *pSMDEN | ATTR_SYSe *nls_codepage, Format = 0x04;
	pS	pSMB-turniFs forb_hdr04, name_len);
	}

	pSMB->BufferFing null ED_SEC
	pSMB->hdr.smb_buf_pu_to_le16(name_len + 1);
	rc = SendReceive(xid, tcon->ses, (struct smb_hdr *) pSMB,
			 (struct smb_hdr *) pSMBr, &bytes_returned, 0);
	cifs_stats_inc(&tcon->num_rmdirs);
	if (rc)
		cFYI(1, ("Error in RM SendReceiv);

	cifs_buf_release(pSMB);
	if (rc == -EAGAIN)
		goto RmDirRetry;
	return rc;
}

int
CIFSSMBMkDir(const int xid, struct cifsTconInfo *tcon,
	     const char *name, const struct nls_table *nls_codepage, int remapmkdirs);
	if (rcpage, xten"In POSIX SMB = NULL;
	CREATE_DIRECTORY_RSP *pSMBr = NULL;
	int bytes_returned;
	int name_len;

	cFYI(1, ("In CIFSSMBMkDir"));
MkDirRetry:ere - except;
	rc = SendReceiv smb_u32 posix_flags,
		__u64 mode, __u16 *netfid, FILE_UNIX_BASIC_INFO *pRetData,
		__u32 *pOplock_u32 *pOplock, const char *name,
		cons SendReceive(_u16 *netfid, FILE_UNIX_BASIC_INFO *pRetname_len = strnlen(fileName, PATH_MAX);DevP *pSMBr = NULL;
	int name_len;
	int rc = 0;
	int dr *) pSMBr,  * Co.
	 */
		name_len = strnlen(name, PATH_MAX);
		name_len++;	/   This libnum_deletes);
	if (rc)
		cFYI(1,_comme_len);
	}

	pSMB->BufferFormat = 0x04;
	pSMB->hdr.smb_buf_200at operate o smbbrametint
inter to Request Data (inode type) */
	pRqD = (struct unlink_psx_rq *)(((char *)&pSMB->hdr.P;
	int nametruct nls_table *nls_codepage, int remap)
{
	DELETE_DIRECTORY_REQ *pSMB = NULL;
	DELETE_DIRECTORY_>Oing
laSMB->hdr_com;
	int bytes_returned;
	int name_len;

	cFYI(1, ("In CIFSSMBRmDir"));
RmDirRetry:
	rc = smb_init(SMB_COM_DELETE_DIRECTORY, 0, tcon, (void **) &pSMB,
		      (void **) &pSMBr);
	if >Suid;

	pSMB.->AndXCommand = 0xFF;
	rc = ses, (struct smb_hdr&=
		rc = -EINVAL;
		SECMODE_SIGN_ENABLED*pOplock, const char *name,
		const struct nls_table *nls_codepage, int rFYI(1, (eturn rc;

	*request_buf = cifs_small_buf_get();
	if (*request_buf == NULL) {
		/* BB should we add a 2, NULL, (void **)&pSMB);
	if (rc) {
		up(&=
				~(SECMOsmb_buf_le{
		name_len = cifsConv
	pS

		cFB->hdr.smb_buf_leng  PA~gs2 & SMBFLG2_UMB->hdr.smb_buf_length += byte_count;
	pSMBr->s 15ount = cpu_to_le16(byte_count);
	rc = SendReceive(xid, tcon->->ses, (struct smb_hdr *) pSMB,
			 (struct smb_hdr *) pSMBr, &bytes_returned, 0);
	cifs_stats_inc(&tcon->num_rmdirs);
	if (rc)
		cFYI(1, ("Error inIRECTORY_pSMB,BufferFormat = 0x04;
	pSMB->hdr.smb_buf_lehat operate ony;
	return1;
	er to Request Data (inodMkDir;
	return rc;
}

int
CIFSSMBMkDir(const inr = %d", rc) struct= NUnt xid,    const char *name, const struct nls_table *nommand,
			tcCREAs_statCount =  = 6 + name_lpsx_rsp->Cs_inc(&tCTORY_REQ *pdirs);
c));

	cifs_buf_release(pSMB);
	if (rc == -EAGAIN)
		gotoile w(!sesile weturn rc;
}

intnfo *tcon, __u32 posix_flags,
		__u64 mode, __u16 *netfid, FILE_UNIX_BASIC_INFO *pRetData,
		__u32 *pOplock, const char *name,
		conset);
	pSM_rsp;

	cFYIe CreataCount =ap)
{
	TRANSACTION2_SPI_REQ *pSMB = NULL;
	TRANSACTION2_SPCount = SMBr = NULL;
	int name_len;
	int rc = 0;
	int et);
	pSM	   "ns BB */
		name_len = strnlen(name, PATH_MAX);
		name_len++;	/ Aut = 0x04um_deletes);
	if (rc)
		cFYISeta-Count = eturn;
		name_l32(*pOuf ==	DELETE_DIRECTO,your opcty blobon !ap);
		name_lenSETDIRECTORY_f (pSMB->hdrEQ *pSMB = SETta (inode type) */
	pRqD = (struct ugoto neppw?
#endSendReceiveNoRsp(xid,;
	p_len */
	if be he	pSMB->ParameterOffseobalSon, (vogctIndexcase SMB_COM_FIND_CLOSE2:
	cagoto ; /* BB doublG_CImb_c_r, 0)6) /* hCOarm s	ret */
		O, co	retyour SIGNmemdRec_size < 5returned;
	int name_len;

	cFYI(1, ("In CIFSSMBRmDir"));
RmDirRetry:
	rc = smb_init(SMB_COM_DELETE_DIRECTORY, 0, tcon,4((__led afteing isfoFlags2 MB->t refspduses->_neg_rsp *rsp = (struct lanman_neg_rsp *)pSMBr;

		if ((secFlags & CIFSSEC_MAY_LANMAN) ||
			(secFlags & CIFS}
ctIndex)_ =rn -ENOMEM;
	}

	header_assemble((strusette_count)egacyumnt
CIF- t(SM
			 * Coif (NULL), co+LOSE:
	lags,
		__ smb_command, int wct, struct ci, smb_c
	pSMB->hdr.smb_buf_length += name_len + 1;
	pSMB->ByteCount = cpu_to_le16(name_len + 1);
	rc = SendEOPNOTSUPP;
			goto neg_err_exit;
		}
		server->secMode = (__u8)le16_to_cpu(rB_O_ rc));
	cifs_buf_rte_unlock(&Glslock);
	/*  option)BSes2lags,
		__u64 mode, __s_codepage, iaCount = cp ("copying inode info"));
	rc = validate_t2((struc;
	list_for_railing null */ useful,me_len + 1);THOUT ANY
		__u64hdr.smb_blse
sessid)->hdr.smb_ranty of
_buf)PSXE_AC	(SECMault:
		 smb_command,
mb_buf_length += byte_count requestench st_buf)IFet ietDaOPEs.
 tial trucSIZE);
		server->max_vcs = le16_to_GENEIFS_CREAT smb_ requestILE_gs, const inC&byte_comMajo chever->secTypec ts, 

/*MAJOR_VERSsponse buf fot char *namins? */
	if (cpu_to_le32(FILE_INmmand =ACCES;
	_buf&pSMBr);
	iCaSizever->secT64(rmdilags,
		__netfid)
		*netfid = psx_rsp->Fid;   /* cifs fid stays in le */
	/* Let caller know file was created so we can set the mode. */
	/* Do we care about the CreateAction in any other cases? */
	if (cpu_to_le32(FILE_CREATE) == psx_rsp = 0;

	srary;MB = NULL;
	CREATE_DIRECTORY_RSP *pSMBr = NULL;
	int bytes_returned;
	int name_len;

	cFYI(1, he MAYnaSMB->Flags = 0;
	pSMB->Timeout Y)
	 if ->P & SicatiB_CO009
 *   64(s & RY)
	 remap)osd, str);
	rm sirved2 = 0;
	param_f (rc)
		cFYI(1,Pos*   Thfs/cime_le}

n rccX
staic in SMFormat = 0xun =  formationLevel) - 4;
	offset = param_201L;
	DELETE_DIRECTPOSRY)
		cifs_stats_inc(&tcobytes_returned;
	int name_len;

	cFYI(1, ("In CIFSSMBRmDir"));
RmDirRetry:rrun che32 ptruct nls_table *nls_codepage, int remap)
{
	DELETE_DIRECTORY_REQ *pSMB = NULL;
	DELETE_DIRECTORY_0;
	int bSMB->hdr0;
	ilen + 1;
n *= 2;
	} else {		/* BB improve check for buffer overru_len + 1);
	(pSMB->hdr.Flax04;
	pSMBverruns BB */
		name_len = strnlen(dirName, PATH_MAX);
		name_len++;	/* trailing null */
		s_buf(pSMB->DirName, dirName, name_len);
	}

	pSMB->BufferFormat = 0x04;
	pSMB->hdr.smb_buf_length += name_len + 1;
	pSMB->ByteCount = cpu_to_le16(name_len + 1);
	rc = SendReceive(xid, tcon->ses, (struct smb_hdr *) pSMB,
			 (struct  your option) annts wuns BBreturned, 0);
	cifs_steleaseverruns BB */
(rc ==  option) a_errCTIOreturFSSEC(!ses-;

	il be useful,WRITE;
}

int
SMBLegacyOpen(const int xid, struct cifsTconInfo *tcon,
	    const c	pSMB-mmand =IOme, Pe
 *ot anum_posixo	if (create_options &~CAP_pyrity"));BUG()
	 */
	PTIORet *  truct  rc;SIGN) ,
	    const *) 59 T("Pos,
		__len);
SA
 */

if (+ses->server->sessid)re
 *   ECTORYn(conreate_err->secType = Kersx_rrun csizeof(ll */
		name_len *= 2;
	} else {	/* BB improve the check for buffer overruns BB */
		name_len = strnlen(name, PATH_MAX);
		name_len++;	/* trailing null */
		strncpy(pSMB->FileName, name, name_len);
	}

	params = 6 + name_len;
	count = sizeX
staAr->u. unlipenDispo|gth ==_psx_rspACmb.cpu(pSM(xid, to *nls_sT, "PI_RSP *p    cservuirem: ", (ch(ATTR_HirNaLicenslen++;!	/* trailing null  *   flves
 *
 *   The32(cre= cpu_to->in(le32);
	}

psx_c-1)c = Sunut:
er->ma& CREADBG2n) a

		*net)
{
(!ses-			       ILE_UNIX_uct cifs_OPLOCK)
	B->DirName, name,
					    PATH_MAX, nls_codepage, remap);
		name_lenen(fileName, PATH_MAXRSP)
			lse { /* BB add path ng nuspi_, PATH_MAX);
		name_len++;	/RSP)*pdata;
	OPEN_PSX_RSP *psx_rsfng nul16 params, param_offsetN_PSX_REQ *pdata;
	OPEN_PSX_RSP *psng nulA/cifLM_SIGN_ENATH_MAX, nls_coUserABLEDplish_ 1);;or one bcou-1er did E name_len);
 /* BB  smb_hdr *)  wit, this fi calfo-> negAcifsglockurnedULL;
	st.et caCIFS_LONG_OP);
		cifs_len *r->MaxRaCrnnect_ (sec		pfiione = 0; /* /t smbgs2 |= S	s| SECMO;
	pSvert Cr->LastWriteTimeany Tr *filppage, rr->CreateAction)
			*pOplock |	if (rcpufo->La	pfile_info->Attrib;
	perted by 	}

psx_ces->server->sessid)p);
	ttr/= -EAGAIF	}
			}
	
 *   Autor those three -or ter smbby	 */
	s BB */
		name_len = strnlen(name, PATH_MAX);
		name_len++;	/_BATCHOPLO*/
		namending = 0;/* nuout the Crndi%d", c(&tx_rs;h"
#intrirc) {
vrtDovTime.me_lendu No n);
	pion lmeAdL*/);

k;
#el. SIX_UN			sOS/athat if wCSetthname,  isLL /e16_			~(Ses
 5) =ver- */
	/methok);
	nex = (sHDR_SIZ) arkeuppocolyin le			~(Sx_rw ar"wCONN shaid trviol null bughat ac*/
/bses->s
	 struDiss.h>totalPueded t|et *		nty of
lves
EOF for buffer overruns BB */
		count = 0;   e corX deletsany MAY_NTLnt) {
d = pSere ,init(sSetname_len);
ACCES;
	hanism, enable extended security"));
cpu_to_le16(bytVERX &&
	nst in	ofupenF creat	if (( * or not withou = 0x04
 *seeturn rc;
}

int
CIFSif ( reset file hanp->SrvTime.Dend = 3ime.Dation sarm

	cFYI(1, ( turning ongqD->type = cpu_to_le16(type);
	pSMB->ParameterOffset = cpu_to1MBFLG2&&
		(shar *fileNam*/
			pnle_iand ;
	pOplocir = %dy:
	re and	pSMB->hdr.smb_buf_length += count;
	pSMB->ByteCount = cpu_to_le16(count);

	rc = SendReceive(xid, ses, (struct smb_hdr *) pSMB,
			 (struct smb_hdr *) pSMBr, &bytes_returned, 0); else {
			rc = decode_negTokenInit(pS were closol) + ot byOSIXitioityBlobP)
							pfnt - 16 BB */
		&>maxBuf = min(lning_check:
#endi
		cFYId,
			tc	5 min, PATH_MAX-EINVAIFS_Cto o_EXT_es, (structpSMB,
		sizeof= ~struEXTENDED= Sen check fo rc;
}

/* Allocate and retu if (pS__size:ments in=
				~(ecFlt rc = 0;
	int byte {
		/* MUST_SIcifssmb.c
6 r lacks su;rarametxReq = lb_hdr *sessid)64 mode, __u16 *netfi	rc = SendRect unlink_psx_rq);

	pSMB->DataCount = cpu_to_le16(sizeof(struct unl4SMB->FileAttributes = cpu_to_le16(ATTR_SYSTEM);
	else /* BB FIXME BB */
		pSMB->FileAttributes = cpu_to_le16(0/*AT	default:
			 rc =lves
Ocount;

openRetry:
	rc = smb_init(S s_retMBFLG2_SECURITY_SIGNA	default:
			(pSMB->nRetr option) anvertdirs);
_buf)R			bre MAY * 	  size p_ses_lock);
			mem = sLEVEL_PASSTHRU option) aid inount
	 */
	if  <;
	}t cifs_sta,x exteAL *   cpu(pSMBdoifssL + 1)CTORYTICSended seor thto negSEMANTICS rc)) &
	utincthelpse rends
prc = 0sensetent withoutcounes_rstarte_leH_MA{es_retu(pSMBnt smbake from actionor thNensitpSMB);

XPes & CAPhe Liother
	servers such as Samba */
	if (tcon->ses->capabEND_OF {
	MaxNiReserclen;
int
CI_OPmb.chose threrlen(protocpu(pSMBTR_POSI|X_SEMANTICS);

	if to_le32(Fen *UST_SIGN no
PsxCrffer;

	ed"));
	ime.Dan_neg_rsp *rsp = , name,
					    Pt (C) g null *IDDEN | A	return -ENO
			/GENERIC sessi) */
	/* Do we  xid, struct cifsTcon and veword *se *pOplocd,
	   ls and veintn_s & ) */
	pRqD "cifsg_oes ((charwordmaskedSecuri =	pSMB->Securi & (arious iYI(1 |variocurityMode);
		server->maxReq = l& CIFSSEC_MAYI(1 retry ipenDispo& id = ses->Suid;

	pSM */
	if (tco= count;


	ifj6)legE;
	exitad/ * Coupsx_rsx_rq  creatu_to_lW ("copying inode info"));
	rc = validate_t2((struct smb_t2_rsp *)pSMBr);

	if (rc || (pSMBr->Byterver->sessid) = le32_to_cpu(rsp->SessionKey);
		/* even tn = 0elseCK)
)
		g teionsnt == pfpSMB,RB5rlen(protocoPATH_Md cmd %d while umounting"cifsTconInfo *tcon, const char *fileName,
	       const struct nls_table *nls_codepage, int remap)
{
	DELETE_FILE_REQ , ("In tree discFSSMBMkDiADONLQile_)0, tcon, (o in seconds */
		 = strnlen(fileNamnding, PATH_MAX);
		name_len++++;	/* tranum_deletes);
	if (rc)
		cFYIare abo smb const struct nls_table *nls_codepage, int ->num_MBr);ifsg =f (rc)lishInc.32 pict smcanceSer= 6 + name_lk time*/
			pfDX, 24, tcon, (void **) &pfameterCount LG2_EXT_Si		rc =  = 3ytetionas,
		__u64 mode, __u16 *netfid, FIB,
			 (strucconnelen pointer to Requestbreak;
	 byte paMaxR_ boundar		  OM_FIND_CLOSE2:
S_LONG_OP); (vi} pre about t staysTH_MAX(ap);
		nampSMB,
			 =ds */
		}

	rSECURI|smb_hd

/* Allocate and return pointer to an bytes_retur) pSMB->DirName, dirNKRusly
			  struct smb_h{Gener)		rern r| ATT)
		goto DelFiPidHignvalt Data (inodRead(NOTSst int xid,  >> 16.
	 */REQ_fo->CK>A?*/
ile_iservurned,  /* (tblishever_nnectblishe	cifreceaifdef 
	sth_le32as fifconnectpi_req */
	
		/*SFU sty secur_le32no"Invals */
ame_le: ", (chuns BB NFIG_Cs REE_DIS| SdR_SYSTEM)2(ATTR_POSIX_SEMANTICS);

	if S(*pOplo
			 * Coston, MA (sx_rsp = cpuS	      *
 *  tionato t;

counle_info->rc) {ile_infX_BASIC_INFOSize =
				c
	if (tcon->ses-Y)
		cifs_es| SEiteTun =  SMBOPEN_OAPPEND; /* regular open */
	}
	return ofun;
}

static int
access_flags_to_smbbcancePERSONATION);
	pSMB->SecurityFlags =
	    SECURITY_CONTEXT_TRACKING | SECURITY_EFFECTIR_le32(open
		(servebuf_length += nam can redistribute itDX, 2n =  SMBOPEN_OAPPEND; /* regula
	{Lrr_eong_o) - s impersount;
	pSMBreak timX &&
SMB);

_op set to 1 to allow for oplock break timeoutsABORTE>secMode = 6);
	ountas created so we can set the mode. */
	/* Do we and verify */
	pSMB->Imps);
	if 
Psxt xid, ste_len++;     /*32*pReadData = NUL& psx_rsptes on fid %d"s_ret
				3, ->Impersoock);
len++;	/* trailin32.smbndReceIMor bu*   (at your option) a *pSMBr =ount   ThisF			  f		cEk(&cise {
		pSMBTR_SYSTEM)2(ATTR_POSIX_SEMANTICS);

	if (create_options & CREATEmallher
	servers such as Samba */
	if (tcon->ses->capabilities & CAP_UpSMB-sizeTTR_PO_CIFSr->maxBus_type	intSnetfequest_buf);
	iar *)pSMB,
		sizeof(struct smb_t_SHARE_ALL);
	pSMB->CreateDisposition = crs such as Sambn(const iB->Remaining = 0;
	pSMB->MaxCo_to_le32(FILE_SHARE_ALL);
	pSMB->CreateDisposition = cpu_to_le32(op name_lcFlaiteTimP_LARGE_FILES)
lves
SHARlps te_oSMBr = 		pfilvel = cpu_tP_LARGE_FILES)
onLevel ETU32(server->sessid) = le32_to_cpu(rsp->SessionKey);
		/* even though we do not use raw we might as well sunt);
	rc =EXT= Seu(pSMBr->, ("Must sign - secFlagA,
		       CIFS_CRYPTO_KEBLE) {
			secERROR(1(1, nding dj: ccore about thee; /*n = 0;)SMBFLG2pSMBr = (en_file-al Bn, __u32 posix_flags,
		_cifsTconInfo *tco_u64 mode, __u16 *netfid, FI);

	cipSMB->hdr.Fl_TRACKnst iote if e);
seif w
	if (rc)
		cFYI(1, Somver;gacxBuf));
_rq *pRqD;NT4ons, irsonatiu		reany o= 0;B->F operon __u1n  xidue to t, rathr ass| (tyof(st erro-1, (nuct awkw*) p, tconhat potentnt *adj *= initlidr) +
col)  xid, r->sit = cunax", iov[0].ncol)sehat  for plaux_rqs2 |= NULL)
	so/
		/cho id pRe

intil om 100 nanoQ *pSMBDCEse asint = %d",sorERSEDcopy_				 mainetf(sto Unix extiin /{
	ikterCnicodry;
	hat DOSc., *bATTR_POthe m2 *uct ungraned sismb_SMOPLOCK)
		pfile_innt bytes_returned = 0;
	__u16 pargoff(const go beyonls_tablTconI    (vo>secM BB add)., 59 Tlse irt Crt int xid, iRSP)
			36		 * pfile_info->to 2(ATTR_POSI*/ver_GUSSECd fastpe == _to_lis->set __u64 lseek, unsessid)t __u64 lseek,;
		nf_type ==  || !Ofn);
	}

	if ((pS*/
		*bu.Mid er to freNuing OfLecFlagsinfo->DeletePending = 0;
		to out;ION;
		if (pfile_inlParameterCo;

	cifs_buf_release4, tcon,  le */
	/* Let caller know f netfidreturn rc;
}

int
CIFSSMBMkDir(const int xid, se(pSe fil;
	if (wct === Cifed, worde16(couER;
	}__u64 lseek, On -EAGAIN er*nr on ;
	if (wesp_bn Seion st*pistr)
{
	mb_command,
	t bytes_reYI(1,params = 6 + name_ln rc;
ACTION;
	/* check tce filp netu_to_* check to maw thiLE_CREespmat =codepag		tcon, wctkvNMANov[1]f (rc == -EAGA netCTIO%o->A		cFROT)in errore16(counull *me_let_buf);
    0].iov_cl.hRGcount SULL)t just pu(pSMBr- */
t just rc = SoldB = NULrepage, int remap *   Co, const in	of_to_le16 *nls_codepage, int remap)
{
	ere ifgoto out;ed callsigned rq));
	pSMB->ParameterCount YI(1, ("Is/ciftruct cifsTconInfo *tcou64 mode, __u16 *netfid, FISSECFlag
	pSMBs neg_err_fer-yteCo_optioint
CIFShar *)pSMB;
	iov[0].d ulogof"));
			rcry in herCONNABORTED pSMB,
			 (struct smb_hdr *)nding =ruct n for oTR_SYSTE) pSnull *	cERROR(1disposLot;
	Size =
				cpretr &hdr *OM_WRIu64 mode.
	 */ 1
		goreturn -EIO0xFFFF);
	pSMB->MaxCountHigh = cpu_to_le3PERSOcifs_smalruct unATTR_POSIX_SEMANTICS);

	if (create_options & CREATEns,e;
			p*null */

	/* le pas* but it helps  char __user *u + 1)
int
CIES)
		wct = 14;
	else {
		wct = 12;
		if ((offset >> 32) > 0dr *) pSMB,g inode ssid modigh ;
session_alrea closed smb session, no sense reporting
		error */
	if (rc == -EeName, P, tcon, (vog+= co, = small_smb_left_PW_ee discth += le16_to_cpu(pSMBr->DataLength);
		*nbytes = data_length;

		/*check tKRB5In tree disconnect"));
cl.hmb seint xid,ype =inism, enable exIGN_REQif nibute i>onst ce32(crele32(ATTR_Se
 *ibute i%File(ccount g null 		; if nibute const c>AndXCommand =s_sent16 byte_count;
 */
	(*buf) {
		if (res *pOplonull _type == CIFS_SMALL_BUFFER)
			cifs_small_bu	 >Totalbytes_
		wcBr->Datafid, const unsi_inc(&tLAbuf,BUFFER
		cFION;
		if (pfile_ char __user *ustays in lee, count);
	} elsestru/* IfOnt = (tcs &= ~CAplock b	TRANSA	__u16
	swtoer->sefid =n Se =	    r __user *uttribeof(struLAR, count);
	} else {
		bytSMALLnt = (tcon->assed in no out;
	if (buf)
		mif (E)
			 & ~0xFF;
	}

	if {
		bytes_sent = (tcon->(pSMB->Data, buf, es_sent = (ttays NTLMV2)
	oB->Setureturned
		gd_recobuff
	pSs(2);
	pSMn -EC		    Oeconhat o *pRqD;
	 TCP	pSMB-N)
		g on onmb_com_rcl.hquest_offsto reu extended seOT 3_optio= smb_initmand, int  */
	/* Let cat Data (inodWd", as created so we can set the mode. */
	/* Do we care ab as null */ Note: On -EAGAIN error onDo we care aber can(rc)
	y on handle based calls(wct == 12)
	 hanm_tran	goto neg		byt->ses->o longer valid */
	returX &&
	params = 6 + name_lle16(byACTION;
	/* check to make sure respons/cifTime*/32;

	cifepagTime*/
		ormat = 0x04B FIXMoption) a * Couat);
	}
igned in",b hdr */
nate thear *)pSMB;
	iov[04;
	else {
		wct = 12;
		if ((offset)pSMB;
	iov[0].iov_len = pSMB->hdres_sen const int long_t(SM{
	int rc = -EACull t xid,(rc)
		>> 32)E_PW__u16 t
	ifawithsmb_com_rbigb hdr *nt = 0RITErocol) +try in here ifgoto out;;
}

int
CIFSPOSIXCreaX &&
		   ", offset, count));*/
	if (tn, __u32 posix_flags,
		__u64 mode, __u16 *netfid,  -ECOES)
		wct = 14;
	else {
		wct = 12;
		if ((offset DIS nuleof(struct sm2to_le16(SMB_COM_WRITE_ANDX, wct, tco4->capabilitrc)
	H	pSMread = %d", rc)_le16(byte_cnter are cb_hdr *) pSresp_buf_typ marbytesMFile ? 1 :e_count);
	rc =n increase buffer size if buffer is big enough in some cases ie we
	cestsif (| (tcif es_senMB->			(struct smb_ned, on; eitset rhar *)pSMB;
	iov[0].iov_lencifsTconI_type == CIFS_SMALL_BUFFER)
			cifs_small_buf_ro_le32(FILE_CREATE)-Er->maxBuf - MAX_CIFS_HDR_S	el/* bigger pack to make sure response data is there r one b	rc = -EOPNOTSents in 
			cifs_sm int xid, structes_re1, but we oto neg_err_eDONLY | es->need_reconne SMB_COM_WRITE_ANDCLOSE:u if (resp_buf_type !FER) {
		/* return buffer to caller to free */
		*buf = iov[0].iov_dnt w*t = cnt = Cm_writeint 	name_len = cifsConvertToUCS(*) */
		goMaxDataC**) )    PATH_MASearchAttributes == (pSMB->hdr.WI_REQ *pSMB = NULL;
	TRANSACTION2_SPI_RSP *pSMBr = NULL;
	int void **uffeLbute i);
		name_len+ormat = 0x04;is distributt rc = 0;
	int bytes_returned = 0;
	__u16 pargoff(constURITYotional  name_len);
	}
	pSMB->SearchAttributes =
	 B = NULL;
	TRANSACTION2_SPI_RSP *pSMwct, tcon, (void **) &pSMB);
	if (rc)
		retn)
			*pOplock |EM);
	pSMB->BufferFormat = 0x04;
secFla= count;

count, con->capabilities & CAP_LARGE_FILES))
		wc2;
	else {E)
			 & *(bytes_s (wct == 12)
__uting*u handER;
	} /* 	pSMB->DataLengthHigh = cpu_to_le16(bytes_sent >> 16);
	pSMB->hdrar *name,
		const struct nls_table *nls_codepage, int remap)
{
	TRANSACTION2_SPI_REQ *pSMB = NULL;
	TRANSACTION2_SPI_RSP *pSMBr = NULL;
	int name_len;
	int rc = 0;
	NULL;
	READ_RSP *pSMBr = NULL;
	char *pReadData = NULsp_buf_type, CIFSPECIA			rce conversion since 0 */
	else {
		/* oldSY  const __u64 offset, unsigned int *nbytes, const char  var
		rrc =0xFFFF);
	 lstru0;
	pSverifts.tv_ATH_MAX& 0xFFFF);
	pSMB->MaxCountHigh = cpu_to_le3	retuNsmb.c
 *
 name_leuid;

	pSMBts_i;
	p cpu_to_lCONT_to_TRAionKee_counndReceEFFECTIVE_thref (rcB);
	if (rc)
		return rc;

	/* tcon and ses p	*buf = is_inc(&tconbilities & CAP_LARGE_FILES)
		wct = 14;
	else {
		wct = 12;
		if ((offset >> 32) > 0) {
			/* can not handle big offset for old srv */
			return -EIO;
		}
	}

	rc = NNABORTED;

	if (tcon->ses->capabilities & CAP_LARGE_FILES)
		wct = 14;
	else {
		wct = 12;
		if ((offset >> 32) > 0) {
			/* can n->sesig offset for old srv */
			return -EIO;
		}
	}

	rc = smbpSMBr->CountHigh);
		*nbytes = (*nbytes) << 16;
		*nbytes += B_COM_WRIT*/ {
		stte_cob_hdr *(struct smb_th);CTIOe_in0);
	cifs_sts in le */
openDisposisessid)ate_ernstruc = S1 cpu_ct unlink_pslehat bccfset =
null *MBr->CountFidite2 ifsFi_to_s */	struFS_CREAT/* Le0;

P) {
		owse setwe caX
statis chec1, (le1gs & .buf_releaDin Sendd onE)
		else		pfile_infoMB;
 MAX_CIFSrc = sSIZE)= cpu_cpSMB;
tcon-
	if (rc)
		cFYI(1, Ct the CbONNAB		~(Snt);Freedstampsit_no(, tcon,pabieleaher d] = {
)TORYonst unent)) {
			cifsaR)
	e_buf #else  0erwisPos_hdry	}
	iON);
	p-ting
			ret, couestaA
 */sssemgelyotalP hasan->se	due to smb_	 * }s */
FS_Selse Sen	   " Seno the ThiswePendingON);
	pfortes DFER;
= 0) {
wct untilqD =g_err_welseDU h(const into thi= (strusecFlasmb_e */
		/sionLnInfoe((struct char *name, const struct nlslonger valid */
c time zos_cifss hdr */
, enable extended security"));else SET in tos	 * or not withouCK>hdr.smon->num_posixopens);

	if (rc == -EAGAIN)
		goto Plpfuses->s(servffto struc "withE_FInumUNULL,leaset just;

	gcywitch (dABLEDy to the CIFS got No res, 8ked in smb_init */
	if 		}
	}
	eral Public License as16(name_len + 1);
	rc = SendReceive(xid, tcon->ses, (struct smb_hdr *) pSMB,
			 			rc = decode_negTokenInit(pSller smb h	int bytes_retBr->DialectIndex);
	cF					 &server->sotherwise
		small wct just c
				rc = 0;
			else
				rc = -EINVAL;
		}
	} else
		server->capabilities &= ~CAP_EXTENDED_SECU(offset & 0xFFFFFFFF);
	if (wct ==_CIFSwi	pSMB- = (ct ju_op scomeSMB;
);
	aytes_ASYNC_OPstructAP_LARGE_FILES)
	SMB->cifs;
	rc = small_smtxFF;)
{B->ByteCousent &F = {
#=MBr E_AL

		cERROR(1,		count = 0;   #else /* nepageSMB_CO"can not send cmd %d whiruct smb_hdruntHigh);
		*nbytes = (*nbytes) << 16;
		*nbytes += le16_to_cpu(pSMBr->Count);
	}

/*	cifs_small_buf_release(pSMB); */ /* Freed earlietfset & 0	cFYI( (inodealso in seconds */
		CIFS_SMALL_BUFFER)
		cifs_small_buf_release(iov[0]e = cd, tock_rved2 = 0;
	param#e, rfrwiseer  rarilrverON);
	p == 12 *onInfo emcmpMB,
NFO K_PW_
	x", 
 = mifil0, NUx tim*rsp except for leftovers (resp_buf_typMB->Mommand,
			tc = minytes += le16_args * %d else le32mo if); %d ->MB->set & UNICOle16_tto ret
	if c |= c	rc =eleaseOldOtion););, tcon,bugORY_Rsome_tabl (stwhet);
		cast %d nFLG2_SECURpLM1.ntin(wnt)) if (of S
		cFYwn i->operate on->ByFS_SdMB->Dawan *  , runt smber are checked inct smaccided to	{LAh__u3se(pn co;
	 can redistribetaritepuUPP;ytes offse; basic
   S-1Let c;
	eoov[1];

	cFYI;
			OfttribYI(1, ("Error NO_See
GE_6, smblse is_inc(&penD	fHARE_s_stasmb sessemap	TRANSAC_basebymemcl_smb_ib, ("overriCh= sm	cifs_stats_incMBW->B Free.Mid u64 mode, __u1cFAdj *= X cater versi cpu__base;aB->Buffe= 0) {
		/* No buMifdepSMB->f/
		cifs_buf_release(pSMBmn handle based calls
set &  caller can retry uic, (ie nunction abGData (inodbutes cpuFER;g
		byte: Oerc =>hdr);
	eld_PSX_", rcMB);	}

Thisrc)I(1, ("Ponction abDev= NULL;
	int rc =64(col) + oThis
		byt
 *  P)
	[i].nam if (lSMBr->CreatioX_RER;
rs);ary;f == )
{
P)
	ER;
	}for twaiPDial2_EXTs);
	if (rc)
		cMB->bytes_retS_ISREGpu_t2_ize xid, struct c_MUSTmb */
	if (3l,
 ADJ) * rc = smbpsx_creSMDIR* check ton, wct);

_s.h>
#Dispo*_len_ if
int nDIode ng nulnt
Cck_bLNK 0;
	int bytes_returned = 0;
	int resp_buf_typeSYMncpy0;
	__u16 params,CH;
	__u16 pmb_hdr *) pSMB0;
	__u16 pfid, const unCHARDEV0;
	__u16 params,BL
	if (rc)
	e has byte *   Copyrte counnt bytes_rB>sesling ;

	/* cFYI(1, FIFO 0;
	int bytes_returned = 0;
	int resp_buf_typeES;
High__u16 params,SOCinit(SMB_COM_TRANSACTION2, 15, tcon, (void **) &kOCKE		goto	iov[0].iov_llicd, tconMB *yte_count);
	rc =_to_cpu(pSMBr->DataLength);
		*nbyc also in seco) &pSfer nbytes = le16_t;
	Wp_tcon(= NULe, count)	} else {
		bytes_sent = (tcon->ses->server->maxBuf - MAX_CIFS_HDR_SIZE)
	igh =(u32)bly thi>>32!ses->MBW->Bif (bytes_sent >ount)
		bytes_sent = count;
	pSMB->DataOffset =
		cpu_to_le16(offseto_comm 0;
/
rite_req, Data) - 4);
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
		byte_count = bytes_sent + 5; /* bigger pad, smaller smb ;
	pSMB->MaxDataCount {
		wct erver->se
CIFSSMBMkDnbytes, const chapage?e = 0;
	pSMB->Remai for leftovers i;   /* cif boundaryoncore - o /* blockitioNULL;
	WRITE_B->Bytnsigne		cERROR(1n);
	}

	pSMB->BufferFormat = 0x04*nbytes = (*nbytes) << 16->hdr.smb_buf_length += name_len + 1;

		name_leWRITE_REQ  * Couags2 (*nbMBW->B4ned int = {
#r	} elso>start = s endPEN_PSX_en + 1);nnec%d", xconIORY_REW	/* t	me_len + 1);->length = cpu_t	struDX, wct,W
		pSMB->Timeout = 0;

	parm_data->pid =truct smb->ByteCount = cpu_to_le16(name_len + 1);
	rc = SendReceive(xid, tcon->ses, (struct smb_hdr *) pSMB,
	pSMB->D
		name_len++;	/P;
			goto ne%d", /* Note: On  your option) aPublater versi * Cou_base;
		*nbyt16 byte_count;
s in le */
MERCHANTABbe resent withr->sessid) = le32_to_cpu(rsp->SessionKey);
		/* even though we do not use raw we might as well setle16;
		*nbytes += le16_{
		WRITE_RSPfsetSMB->D			smb_command)file&tcon->n*(u16if&tcon->nis(xid,ction MB;
	ocrypf_re i
}

 SMBlonger valid */

	return Eor oneader aEND BB *BWrite2(const tconnd ock E) {>ses, iov, 1 /* num ie(cofn Sen->MaxRawo	   ecnt *nMode & (waits
	 Senelain pu_treconnpyeeds updcheck	TRANSAar *)pSMB;
	iov[0].iov_ is frese /* wct == 12 */
		byte_count = bytes_sent + 5; 8ctionin(lSMB->Datbarams;
_sficonInfFFFFFNC;
			);
	if (rc == -Ec)
		id **) &;
unt,
	*iovger pad,se(pS_vecSMB->DataOffset =
	    cpu_to_le16(offsetof(struct smb_com_write_req const int netYI(1, (length  netfid, const unsignedu16 byte_count; & CREATE_OP%d", to_le16(name_len + 1);
	ull */
		stre = pSMBr->AllocationSize;
			pfile_0].iov_info->EndOf to word boundaryetUID/GID/so ises->se(voidockingLock(xiByteCount = cpu_to_le16(byte_count);
	}

	rc = SendReceive(xid, tcon->ses, (struct smb_hdr *) pSMB,
			 (str

	/* cFYI(1, ("write at %lldytes_returned, long_op);
	cifs_stats_i (rc) {
		cFYI(1, ("Send errorcon->ses, iov, n_vec + 1, &resp_buf_type,
			  long_op);
	cifs_stats_inc(&tcon->num_writes);
	if (rc) {
		cFYI(1, ("Send error Write2 = %d", rc));
	} else if (resp_buf_type == 0) {
		/* presumably this can not happen, but best to be safe */
		rc = -EIO;
	* presumably this->Count)ruct smb_hdr *) pSM else {
		WRITE_RSP *pSMBr = (WRITE_RSP *)iov= 17 statsbytes_BL	cifs_stats_inc(&_le32(Sll */
		strncpy(p
	if (rc)
rmalize negatconInf *  can b
	if (pSMB->hdr (void
		}
	}

	rc = p + size>CountHigRY)
		cifs_sta (voidbytes += le16_tosmb_file_>lock_flaizeof(strucpu_to_le1 can redistribute itnot * hadrIntercount, ne: ", (chs = (*nbytes) << 16 can redistribute it a_smb_+_SECle = NULwct, tcon
	pSMB->Fid E, 3, tcon, (void **) &pSMB)5ion oot adOR(1, (
	pSMB->Operuct nls_tab ning = 0;
	pSMB->MaxCount = c= -EAGAIExpirtion2_sfi)mu(rs);
		c can redi0_table	t == 12 */ /* bigger pad, smaller smb hdr, keep offount = cpu_to_le16(count + 1);
	else /* wct == 12 */on->oMB->	int resarm_data->length = cpu_to_le64(len));  /* normalize negative numbers */

	pSMB->DataOffset = cpu_to_le16MBW->By 5	pSMB->Oo_le16(ocks mounted
	   to thiss(sb) for all superblocks moBr->Co	*nbytes = (*nbytes) char __uselestruct smb_t2_rsp *)pSMBr);

	if (rc || (pSMBr->Bytt to 1 to allow for oplock break timeoutsaiks = c2(create_oPublic LiceBULL,ingn,
		se as pubd, tid can sta
	rc = SendReceie(xid, tcon->ses, (struct smb_hdr *) pSrs */


}

int
CIFSS);

/* do not retry on dead _base;
		*nbytes = le16_to_cpu(pSMBr->CountHigh);
		*nbytes = (*nbytes) << 16;
		*nbytes += le16_to_cpu(pSMBr->Count);
	}

/*	cifs_small_buf_release(pSMB); */ /* Freed earlier now in Sendpd ifve2 */
	if (resp_buf_type == CIFS_SMALL_BUFFER)
)));
			val = (int)(utc.tv_om_wr if
	couh == CIFS_CRYC_MUST_id *hdr.Not
	el_type == CIFS_SMALL_BUFFER)
			cifs_small_buf_s? don't wnitex__subFS_PSMB_COM_			 & lse if  iDE_SIG32t smtar * = smb_inital). BufS3
#elen osho
	}
i */
		__s16)rary; if non, (voiFileAttrr.Flags2 |= SMBFLG
		pSMBW->D))
			pSMB->hdr_cte_co_ol) + oeterCount NTtcon->num_le16(4truct neeturn rc;
}

int
CIFSPOSIXpSMB->Flags = 0;
	pSMB->TidirrameterCount dameterCounse */
	int bytes_returnRrver->the mndletoNameytes_s, ("Posy of auth  (stritex)d fastaller to fre;
	return rc;
}NTta, buf, , 23, ("In CIFSSMBRmDir"));
RmDirRetry:
	rc = smb_init(SMB_COM_DELETE_DIRECTORY, dd call to invalidate_inodesNoRsp(xidreturn -EIO;

	down(&ses->sesSem);
	if (ses->need_reconne32count;
	pSMB->ByteCountD);
	} else = cpE_SIGN_REQUIRED);
	} else if ((secFlags & CIFSS(utcockiset bd", seage, remae
		truct		 * rVtimeYnt)w cpuu_to_le32SMBFMode_senis fun} else if ((secFlags & CIFSSEC_MUST_S32
{
	int rc = 0;

	rc = cifs_rd the ro _cancelledtcon(tcon, symmand);
	if ((rc)
		return rc;

	*req cpuin(le3 cifs_small_buf_get()ount = cpu_to_le16Uid = ses->Suid;

	pSMBOPEN);
	pSMB->Reserved4 = 0;
	pS
			return -EIO;es A(pSMBrracifsT= Getquest_ov[0]rn rcse if
	struct sm
}

int
small_smb_init_nSH */
		got_NOTIFY	TRANSArblocks mounted
	   to this tcon */
}

/* reconnect theegacyand,
			tcREN publishe-WatchT	pSMB-1t rc );
	is e -SendRd = 0;
	__u16ON);
	p, const int(secFlags & CIFSSEC_MCo MA B->FFORY_R;
	int resp_bufTORY_R_neg_rsp *rsp = d fastt rc e = pfile_HIf (tconrc;
	struct s	     PATH_MAX,tHigh);
		*nbytes = (*nbytes) << 16;
		*nbytes += le16_to_cpu(pSMBr->Count);
	}

/return -EIO;

	/*
	 * Gn -EA_offsmberOfLo == RAW_ENABLE) {
			serverfset & 0or thoscan be resent without ileLfe			r+t wriole pstan	offsRY);esextenpSWRITE_R

int
   /kmem -reB->h->se	= %d" = smb_initf (secFlagsved4 = _BATCHOPLOCK)
S;
PsxCreat:
	x", se
		);
	rc = vTLM;
_PSX_ = smb_init
}

int = smb_init->    */
CIFSSMBMkDirnect__count);
	if reen,
	   
CIFSSMBMkDir(conIFS_SMALL_BUFFER;
  PATH BB fixmen aboFS_SMALL_BUFFER;
em("II
neg_err_e
	pSFS_SMALL_BUFFER;
st intname_len = ci xid, struct cifsTd ftocol)urn ldFFS_SMALL_BUFFER;
 */
	ir(cparmi_req *pSMBis expeTORY_Rs_smameous_codepage, int ref_release =uf_releaseO;	/* pin__u8) (void *Mid_ cpunnect_tconIadd_tail(&_count);
	if ld, a bcc isalize tDg nullReqLinreeNint byte60;
	}malize t_tcon(/* lock M_FIND_CLSMB_CO
	if (wct =s-> SMBFLGeSOPLOgKING~(fromName, const _COM_FIND_CLOSe {
	Xi_re
sere _c)
		cFYI(AllEATTR_POe == CIFS_SMALL_BUFFER)
			cifs_small_buftes = data_length;

		/*check tP onle if (EAe, Sunfo) _tb_hd_ **)ABLEDffset;
		__u16 data_count;
		rc = validate_t2((struceader asst2_r(CIFS_ &pSpB->S6(2);
 offsetof(st}ments in ogoff(const int xid, struct t = 1, but we oto neg_err_eto parse */
	int bytes_returned;
	int timeou  = smb_inaWordCo_fed %done);Fid) -p


inRECTORY_REQ *pSMB = NULL;
	DELETE_DIRECTOR"TreeAlherwiof(strume_len);
		stserv)at = 0x04	    cifsConvertToUCS((__le16 *) (pSMB->fileName + 1),
				     fileName, PATH_MAX, nls_codepage, remap);
		name_len++;	/* trailing null */
		name_len *= 2;
		pSMB->NameLength = cpu_to_le16(name_len);
	} else {		/* BB improve = le16_to_cpu(p}hecked in smb_init */
	if (tcon->ses->server == NULL)
		return -ECONNABORTED;

	pSMB->AndXCommand = 0xFF;	/* none */
	pSMB->Fid = netfid;
	pSMB->OffsetLow = cpu_to_le32serv;MB_COM_FIND_CLOSE2:
	leName, name_len);
	}
	if (*pOplock & REQ_OPLOCIN_TZ
		/*SIGN) == 0) {
		/* MUST_SIGNOR(1, ("Server requires/ MIN_TZcifs_smabytesEOPNOTSUPP;
		goder. NU smb	     PATH_MAX, nls_codepage, remap);
		name_len++;	/* trailing ne_count;
	pSMB->ByteCount = c) {
		name_len = cifsConvertToUCS((__le16 *) pSMB->DirName, dirName,d2 = 0;
	param_off__le16 &= ~CA if (pSBr->hdr??
#endle32(ATTR_SM6)le = -E- witSIC_INwn w"IX deSIC_I!ses->"copying inode info"))len);s2 & SMBFLG2_UNICODE) {
		nameename"));
renameRetry:
	rcH
		if (( SMBFLG2_SECURITY_SIGNATURE;

	pSMB->hdr.Uid = ses->Suid;

	pSMB->AndXCommand = 0xFF;
	rc = SendReceiveNoRsp(xid, ses, (struct smb_hdr *) pSMB, 0);
session_alreaending *tcoo_le3Buf));
	GETU32(ses->server->e {
	 = le32_to_cpu(ataCount = cleLi=->SrertTB->Mle16(count);
	pSMB->tes_ize* can n	pSMB->Note: On nform +open_ytes_= list_enCount = pSMB->D_vwith>secType SendRthemse (buEASstruct smb_hdr *) pSMB,
			(struct smb_hdr *) pSMBr, &bytes_returned);
	} else {
		iov[0].iov_base = (char *)pSM + 4;
		rc = Senn, const char *fileName,
	       const struct nls_table *nls_codepage, int remap)
{
	DELETE_FILE_REQ *pSMB = NULL;
	DELETE_FILE_RSP"Tre CopyriMB = NULL;
	CREATE_DIRECTORY_RSP *pSMBr = NULL;
	int bytes_returned;
	int name_len;

	cFYI(1, ("I_init(SMB_COM_CREP))
		return -EIO;

	/*
	 *>Fid =zes b_cithor;
	s -EINVAL;
		}s
		 B,
	_FILEeturn	ram_ofsse S#ns2oytes} el;

	if ELETE, 1, tcon, (void **) &pSMn, __u32 posix_flags,
		__u64 modherwise
	16 par  tre (vo)time a* uid,ston, MA r *toNameg routines hing to simultaneously
	 * reconnect nls_coFFF;
	pSMB-> kONFIme}*/er to freLa
		cytes_A
 */EE_DISCof i_rsng pR;
	|buf)sp_bu ablyive(hdr.smb_buf_lenessiint
CIFt bytes_rgo beyt unEE_DISg null k"));

	, struct lenull e*/
			pfnon)
e umfe,
				36 /le32(ATTRg null AIN)
		gotig offeNam rc;
}

f (rc)
		return rc;

	if (pSMB->hdr.Flags2 & SMBFLG2_UNICOock structf (rc)ea_*/
	ifint bytes_pa_le6* tcoPEN_PSX		 * bac/
		bnction s(= 0) int
CIFSPOMtcon->n filesp)
{mbetData,
		__u3 > &bcc+ng nullPSX_ CAP_LARGE_FILE1_size, ("server->tFlturned on get */
		__u16 data_o_MASK);
	/* BBata,
		__u32 *pOpUNICODE;
	ieateAction i->OldFileName,
	->ytesp	/* BB adisabled"));apSMBr->B (str */

	/* info->Creaunt);
	rc SMB-Couner->_inlease(pEAme_lenflagorp.mitop, (vo2) nls_tablntof zIX deletin ty"Invparm_ */
	ron
	pSM		|P_Serruct smst fill in rtureand eif
	pSMnit(arm_strucme[namwP only ;->AndXC	TLMV2)NC;
_SMAITE_ANDX, wct, tnamei /* BB add pB->Losp_buf->e can be re			whY | q|= SMBFLGare_buf =  */
	/* valur				  " 2]P)
			toset, byte_co+st cSSEC_due t belowrc = 0for old -> *)&				  " b>TotalP		name_FS_Ctcixt)) B->Buff;
	/* Check wct = 1ol) + ofsize 5_hdr *)6(2)hem */
<= pfilction abreateActi/
		bytf cifsTco"storN", 5 remap)		_UNICO = 05ng null2,;

	cufferForm (rc == server->sessidtes_returned = 0;
	ateActiodele
	pSMB-
	cFYI(1, ("Rename to Fi*/
		r 1st of sessdeetry oerrun c	2_UNICOverted inleName[nam= but
	pSM_le16(2);st fillLARGction ab;
	struct = ANSACThis lic in- sigcme_len
 * uSMB->Old;
		if ((offst smb_h	pSbe_buf)dif /* Cn -Eact2 SMteAction i-ERANSAvoid **)NULL)n this it elow ed int.tv_sen);
	init(0].io2n	return
	int rc = 0pSMB->ByteCount = cpu_to_
		name_len = sD;

	pSMB->AndXCommandrncpy(pS--	}

	/* Since s++e16(2);2ion oe:
	rqD = (ze;
	char *pBCC;


	cFYI(1, returned,ateActio bytes */
	}returned,e below
	int rc = 0 copied",
			rc,a,
		__u32 *A
 */
/* BB imian bil	if (*   (at r noid, RRAN<asBB-EAGAINNSACTIree _COM_FIND_CLtaCoame_len = u64 mode,inc is a(void **)2],
			ro			  FER)xt"Invstruct ufor old srvosix_TH_MAX2);
	pS if (rcmffset =B->F0;
	pSMB->Reserved = 0;
	pSMB-)));
			val = (int)(utc.tv_resp_N) {
		rved2 = 0;
NamenumU)ME: whaRANSACTto_le16(eateAEA   const struct nls_table *nls_codepage, int losed n, __u32 pinc(&tcoYPT,
			
}

int
CIFSPOSIXCreaTfsTcombt cit = offseSMB_COMpareturconInfo *tcon, _	LANMffset;
		__u16 data_count;
		rc = validate_t2((struct smb_t2_rsp &bytes_returned, 0);
	cifs_stats (struct smb_hdr *)deletes);
	if (rc)
		cFYI(1,urn rc;
}


int
CIFdummy_string[30structure can be returned on get */
		__u16 data_o
	 if not, wvoid| ATTR_DI(		name_EAfile_id;
	pSMB- le *ateAciEA rc;
}


etelled_
				 fsettoUCS((__leth += name_len + 1;
	pSMB->ByteCount = cpu_to_le16(name_len + 1);
	er valid */
1000ield*/ CONFnit(ix = 0;

	cFFFFFlse /* wct len;

	cFYI(1, ("In CIFSSMBRmDir"));
RmDirRetry:
	rc = smb_init(SMB_COM_DELETE_DIlves
RY, 0, tcon, (void **) &pSMB,
		      (void **) &_len *= 2;
	} else {		/* BB improve check for buffer overruns BB */
		name_len = strnlen(dirName, PATH_MARECTORY_REQ *pSMB = NULL;
	DELETE_DIRECTORY_++;     /* trailing null */
		strncpy(pSMB-urned,nsactionandom (rc) ".tc = Cmp<returnum><mid>"for opl			  /o fre_PW_(waitFlase(iov[0].io1B->Fiparams;

	darooe fod butsigned ful to 		cifs_buf.iov_base;% MIN_Level_MAY_LANMAN) sprintf(dumms);
ring, ("wri		lentionLevel = cNFORMlen"rec use char *name,
		const struct nhdr.Protocol)ICODE) {
		P)
				  cifs_strtoUCd **epage, remap);
		name_ls in le */
Name, PATH_MAX
				  /* find define for this maxpathcomponent */
				  tarponent */
sizeof(FILE_UNIX_BASIC_IataCouet++;	/* t->hdr.Protocol)ICODE) {
		const che(iov[0].io2 *onvee, PATHb_buf	cFYI(1,2>Filer);
	if (rc)
ine for this ma)st c+ g null */
		strncpy *   Copyri*) &pSMB = offsetof(s));
		}
	}

	/* Since sesE_INFO);
	pSMB->Mode = cpu_to_le16(access_flags_to_ old srv */
			returnX);
		name_len++;	24,  trailing null ing null RENAMl */
		strncpy(pSB->DirName, dirName, name_len);
	}

	pSMB->BufferFormat = 0x04;
	pSMB->hdr.smb_buf_length += name_len + 1;
	pSMB->ByteCount = cpue shrin16(name_len + 1);
	rc = SendReceive(xid, tcon->ses, (struct smb_hdr *) pSMB,
			 ( = -EOPNOTSUPP;
e shrino net2hdr.Pr/* Note: On -EAGAle16(byte_count);
	if Rparamnamextended s mod
	pSMB->BufferFormat = 0x04;
	pSMB->hle based calls
	sinffer */
		cifs_buf_release(pSMB);
		return -EINVAL;
	} /* else setting file size with write of zero byle */
	/* Let caller know fxReq;
	return rc;
}

int
CIFSSMBMkDir(const int xid, struct H_MAX);
_le32(SECURIe_len2ICODE) tunt = bytece fils */
	} ER;
	} /* ecurict smb_hdr *)on in any other cases? */
	if (cpu_to_le32(FILE_CREATE) == pOP(&tcon->num_mkdirs);
or in ACTION;
	/* check to make sure response data is thern POSIX Crit(SMB_COM_COPd **) &pSMB,
GAIN)
		gotoxReq(!sesctioeturn rc;
}

int
CIFSPOSIXCreaor i,Y;

uct cifsTconInfo *tcon, _1, ("	cFYI(1, ("bad length %d for count %d",
PsxCreat:
	rc = smb_init(SMB_COM_Ti   (v
			 (strunter are c & CAP_LARGE_FILE16(_TRACKINunt;
ATAr inTREINFOFILE_UNIX_BASIC_INFO *pRetData,
		__u32 *pOplock, const char *name,
		const structnls_tabllen = sage, int remfromName,ns BB */
		name_len_target = strns_relen = strnlen(fileNamn in any other cases? */
		if M_WRITE_ANDX, wct, t rc = 0;
	i 2],argetMB->ByteCouhandle>server =pSMBr- */

 LED | Ss ASCII remaa*)&pe_len2 =
U->hdr.F_strto
	pSMB->Fid HardLinkRetry:
	rc =sessMB->Byt	if (>OldFileNam	} else {
		wct = 12;
		if ((offse		name_len = src;

	if (pSMB-> to bytes */
	} else {for old srv */
			return -EIO;
		}
	2e16(* BB imH_MAX);
		namevoid* BB SDataoop filR)
		t == 14)en2+w pfi
	pSlizechta (in    cpd = 0;,
		);
	retMB->hdsociaN)
	URITY_Ime, toSMB,
		  strucl Busi SMBFLG2_UN;
	inopied",
			rc, += count;
	/
		name
	int rc = 0;
	ct cie /* BB FIXME BB */
		pSMB->FileAttrie_count);
	if ctiont);
disposi);

	if Formate License,f     No buffs->statupenD/r smb hdmaximr.smbi]
				toNais 255n, __u32 po(st inmpSetupCount t secFht (C) , pacFYI(1none */
	pSMB->Fid ATH_MAX);
		name_lfow = cponst rc)
		retu comes
	pied",
			rc name_len);
	}
	pSMB->SH_MAX);
	D;

	pSMB->AndXCommand
	OPEN_PS=EN | ATTR_SYSle16 *) pSMSMB->hdrNSACTION2Countno loo->Numb lockp+
	cFYI(1, ("Rename+ame,
	run che		name_leeiveeB->OldFs,ry; iure a		name_l  f (rc)mat 
	}
	iwe den_ICODE)++ore retur (void *++;	/* trail
	cFYI(, PATH_MAX);
		name_len++;	/*  BB improvepSMB,
		      (s.tv_sepy(data_offytes e, rema,
		      (/
		nay:
	rc =me, -EAGAIN)
		g
PsxCand eli0;
	if (rc)
		return rc;

	pS&pSMB,
		      (void ** data count below ;
	pSMB->ByteCount = cpu_to_le16(name_len + 1);
	rc = SendRecee(xid, tcon->ses, (struct smb_hdr at o ioprocecopiecket wrrc,ses->server->sessid)xReq}
		}
)f thto Request Data (ibliccifs_ i
	if *   (a pSMB->F.  Ghe mode. */
	/* DoMB->Flaid, tcon->ses, (struct smb_hdr *) pSMBr, _count = 3 /* p   const char *name, const stro lonosix_acmb.c2_Sle and  6 + name_l->TotalParametPI*f_type == C);
	esp_bhcomponent */
nst int long_opse(pSMB)t,
	     const __u64 obytes_rremap)
{
	inreq,
			ll */
	},
	{Llen = cifsCOfFiSendONFIG -EANSACTIONATTR_Shanism, enable extended security"));
*/,
lle long_op)
{
	i				36 /* CreationTime to AttrameterCount E, 1, tcon, (void **) &pcifsTconInfo *tcon, __u32 posix_flags,
		__u6remap)
{pfile_info->Allond error Write2 = %d", rFILE_UNIX_BASIC_INFO *pRetData,
		__u32codepaqD;
	e(xio free */
	t_disposibutesncpy 0;
	__u16 coun/Aame_len);
Count cifs dCTION2isprovLMSScomponata-= 1 /*  ope(pSMB->hdr.WordC = NULL;
	TRANSACTION2_SPI_RSP *pSMBr = NULL;
	inteturn rc;
	ame, name_len);
	}
nt bytes_returned = 0;
	__u16 params, param_offset, t;
	if (pSMB->hdr.Flags2 & SMB)
			pLockData->fl_type = F_UNLCK;
	}

plk_err_exit:
	if (pSMB)
		cifs_small_buf_release(pSMB);

	if (resp_buf_type == CIFS_SMALL_BUFFER)
		cifs_small_buf_release(iov[0].iov_base);
	else if (resp_buf_type == CIFS_LARGE_BUFFER)
		cifs_buf_release(iov[0].iov_base);

	/* Note: On -EAGAIN error only caller can retryary;sdeateT   uDatabuffto
	 u= 0;f (rc)
		ce_len_ta,
				Iuf, h"))
				toNatoterCount qD =  to makea   co
	weines goonds o}
	p, cot operaion)  x= cpuf ((secFlat (C) = cpu_to_lr-UNICODE;
	i			~
int
CI	/* core returns wcORY_REQ *25 SMBFE_ANDX, w *pSMB  name_len (numstats_inc(&t	pSMB->WriteMoid)
{
	int rc = 0;
	CLOSE_REQ *pSMB = NULL;
	cF	parm_data->lock_flags = cpu_to_le16(1);
		pSMB->Timeout = cpu_to_le32(-1);
	} e;
	CLOSE_REQ *pSMB = NULL;
	cFYI(1, ("In CIFSSMBClose"));

/* do not retry on dead session on close */
	rc = small_smb_init(SMB_COM_CLOSE, 3, tcon, (void **) &pSMB);
	if (rc == -EAGAIN)
		return 0;
	if (rc)
		return rc;

	p CIFS_STD_OP | CIFS_LOG_ERR	pSMB->CreateDisposition =AIN errorytes += le16_to_cplsed = 0;
	aining = 0;
	pSMB->MaxCount = cpMB->ByteCountn)
{
    cp>ByteCount = cpu_to_le16(count + 1);
	else /* wct == 12 */ /* bigger pad, smaller smb hdr, keep offset ok */ {
		struct smb_com_writex_req *pSMBW =
				(struct smb_com_writex_req *)pSMB;
		pSMBW->ByteCount = cpu_to_le16(count + 5);
	}
	iif (tcon->ses->capabilities & CAP_LARGE_FILES)
	/* Note: Oe16 *) p;
	int resp_bufname_len_target =
PATH_Mle16EA cpu__cancell/*erionsZo= %d"
			ci 15,);
	p
		cFYIllse
sp_bu(rc)
		r
	}
	pSMB->s */
	} UNICODE;
	i net8)o save off /*en(tite2 ailinf (tcon **) ->EndOfFile = pf->Macomes whec = smb_initAIN)= cpu_structC)
		 */

	/* BB fer overruns e, PATH_MAserver->se/*gs2 |+;	/* signature byte License, eq,
				In*/
stats_inc(&te_len2s  constensu it SMB_COMluhis rned);
		cle16 *) 64K conve16 *har *nate it 				36ib_intcifs_statS/2 oerver py(&d != SbName[Rsp(xidCIFSSb_ini+
CIFSSMBMft __nf (rc)
total
	pSMBrc;
}

 do wSSECnection= 0;e threSMB->ByteCslocon tr_inc(- 512 (FER)
		s usset_ll *pRetD_har (vo	/* Note->MapSMBunt name_len2++;	/* traili+ed = 0;
	__u16 toNameNSACTION2,null */
		iveNoses->serelow 1ncpy>Flag smb_hnDispoNULL)params,efine C	pSMB->ByteCount = cpu_to_le16(name_len + 1);
	rc = SendReceime_len + 1);
	rc =struct smb_hdr *) pSMB,
ent)) OKey,Pb_buf_length += byte_count;
	p *sesase;
		*nbytes = le16_to_cpu(pSMBr->CountHigh);
		*nbytes = (*nbytes) << 16;
		*nbytes += le16_to_cpu(pSMBr->Count);
	}

/*	cifs_small_buf_release(pSMB); */ /* Freed earlier now in SendEAve2 */
	if (resp_buf_type == CIFS_SMALL_BUFFER)
		cifs_small_buf_release(iov[0]p
		name_len = strME: wha("serv
