/*
 *  fs/nfs/nfs4proc.c
 *
 *  Client-side procedure declarations for NFSv4.
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

#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/sunrpc/clnt.h>
#include <linux/nfs.h>
#include <linux/nfs4.h>
#include <linux/nfs_fs.h>
#include <linux/nfs_page.h>
#include <linux/namei.h>
#include <linux/mount.h>
#include <linux/module.h>
#include <linux/sunrpc/bc_xprt.h>

#include "nfs4_fs.h"
#include "delegation.h"
#include "internal.h"
#include "iostat.h"
#include "callback.h"

#define NFSDBG_FACILITY		NFSDBG_PROC

#define NFS4_POLL_RETRY_MIN	(HZ/10)
#define NFS4_POLL_RETRY_MAX	(15*HZ)

#define NFS4_MAX_LOOP_ON_RECOVER (10)

struct nfs4_opendata;
static int _nfs4_proc_open(struct nfs4_opendata *data);
static int nfs4_do_fsinfo(struct nfs_server *, struct nfs_fh *, struct nfs_fsinfo *);
static int nfs4_async_handle_error(struct rpc_task *, const struct nfs_server *, struct nfs4_state *);
static int _nfs4_proc_lookup(struct inode *dir, const struct qstr *name, struct nfs_fh *fhandle, struct nfs_fattr *fattr);
static int _nfs4_proc_getattr(struct nfs_server *server, struct nfs_fh *fhandle, struct nfs_fattr *fattr);

/* Prevent leaks of NFSv4 errors into userland */
static int nfs4_map_errors(int err)
{
	if (err >= -1000)
		return err;
	switch (err) {
	case -NFS4ERR_RESOURCE:
		return -EREMOTEIO;
	default:
		dprintk("%s could not handle NFSv4 error %d\n",
				__func__, -err);
		break;
	}
	return -EIO;
}

/*
 * This is our standard bitmap for GETATTR requests.
 */
const u32 nfs4_fattr_bitmap[2] = {
	FATTR4_WORD0_TYPE
	| FATTR4_WORD0_CHANGE
	| FATTR4_WORD0_SIZE
	| FATTR4_WORD0_FSID
	| FATTR4_WORD0_FILEID,
	FATTR4_WORD1_MODE
	| FATTR4_WORD1_NUMLINKS
	| FATTR4_WORD1_OWNER
	| FATTR4_WORD1_OWNER_GROUP
	| FATTR4_WORD1_RAWDEV
	| FATTR4_WORD1_SPACE_USED
	| FATTR4_WORD1_TIME_ACCESS
	| FATTR4_WORD1_TIME_METADATA
	| FATTR4_WORD1_TIME_MODIFY
};

const u32 nfs4_statfs_bitmap[2] = {
	FATTR4_WORD0_FILES_AVAIL
	| FATTR4_WORD0_FILES_FREE
	| FATTR4_WORD0_FILES_TOTAL,
	FATTR4_WORD1_SPACE_AVAIL
	| FATTR4_WORD1_SPACE_FREE
	| FATTR4_WORD1_SPACE_TOTAL
};

const u32 nfs4_pathconf_bitmap[2] = {
	FATTR4_WORD0_MAXLINK
	| FATTR4_WORD0_MAXNAME,
	0
};

const u32 nfs4_fsinfo_bitmap[2] = { FATTR4_WORD0_MAXFILESIZE
			| FATTR4_WORD0_MAXREAD
			| FATTR4_WORD0_MAXWRITE
			| FATTR4_WORD0_LEASE_TIME,
			0
};

const u32 nfs4_fs_locations_bitmap[2] = {
	FATTR4_WORD0_TYPE
	| FATTR4_WORD0_CHANGE
	| FATTR4_WORD0_SIZE
	| FATTR4_WORD0_FSID
	| FATTR4_WORD0_FILEID
	| FATTR4_WORD0_FS_LOCATIONS,
	FATTR4_WORD1_MODE
	| FATTR4_WORD1_NUMLINKS
	| FATTR4_WORD1_OWNER
	| FATTR4_WORD1_OWNER_GROUP
	| FATTR4_WORD1_RAWDEV
	| FATTR4_WORD1_SPACE_USED
	| FATTR4_WORD1_TIME_ACCESS
	| FATTR4_WORD1_TIME_METADATA
	| FATTR4_WORD1_TIME_MODIFY
	| FATTR4_WORD1_MOUNTED_ON_FILEID
};

static void nfs4_setup_readdir(u64 cookie, __be32 *verifier, struct dentry *dentry,
		struct nfs4_readdir_arg *readdir)
{
	__be32 *start, *p;

	BUG_ON(readdir->count < 80);
	if (cookie > 2) {
		readdir->cookie = cookie;
		memcpy(&readdir->verifier, verifier, sizeof(readdir->verifier));
		return;
	}

	readdir->cookie = 0;
	memset(&readdir->verifier, 0, sizeof(readdir->verifier));
	if (cookie == 2)
		return;
	
	/*
	 * NFSv4 servers do not return entries for '.' and '..'
	 * Therefore, we fake these entries here.  We let '.'
	 * have cookie 0 and '..' have cookie 1.  Note that
	 * when talking to the server, we always send cookie 0
	 * instead of 1 or 2.
	 */
	start = p = kmap_atomic(*readdir->pages, KM_USER0);
	
	if (cookie == 0) {
		*p++ = xdr_one;                                  /* next */
		*p++ = xdr_zero;                   /* cookie, first word */
		*p++ = xdr_one;                   /* cookie, second word */
		*p++ = xdr_one;                             /* entry len */
		memcpy(p, ".\0\0\0", 4);                        /* entry */
		p++;
		*p++ = xdr_one;                         /* bitmap length */
		*p++ = htonl(FATTR4_WORD0_FILEID);             /* bitmap */
		*p++ = htonl(8);              /* attribute buffer length */
		p = xdr_encode_hyper(p, NFS_FILEID(dentry->d_inode));
	}
	
	*p++ = xdr_one;                                  /* next */
	*p++ = xdr_zero;                   /* cookie, first word */
	*p++ = xdr_two;                   /* cookie, second word */
	*p++ = xdr_two;                             /* entry len */
	memcpy(p, "..\0\0", 4);                         /* entry */
	p++;
	*p++ = xdr_one;                         /* bitmap length */
	*p++ = htonl(FATTR4_WORD0_FILEID);             /* bitmap */
	*p++ = htonl(8);              /* attribute buffer length */
	p = xdr_encode_hyper(p, NFS_FILEID(dentry->d_parent->d_inode));

	readdir->pgbase = (char *)p - (char *)start;
	readdir->count -= readdir->pgbase;
	kunmap_atomic(start, KM_USER0);
}

static int nfs4_wait_clnt_recover(struct nfs_client *clp)
{
	int res;

	might_sleep();

	res = wait_on_bit(&clp->cl_state, NFS4CLNT_MANAGER_RUNNING,
			nfs_wait_bit_killable, TASK_KILLABLE);
	return res;
}

static int nfs4_delay(struct rpc_clnt *clnt, long *timeout)
{
	int res = 0;

	might_sleep();

	if (*timeout <= 0)
		*timeout = NFS4_POLL_RETRY_MIN;
	if (*timeout > NFS4_POLL_RETRY_MAX)
		*timeout = NFS4_POLL_RETRY_MAX;
	schedule_timeout_killable(*timeout);
	if (fatal_signal_pending(current))
		res = -ERESTARTSYS;
	*timeout <<= 1;
	return res;
}

/* This is the error handling routine for processes that are allowed
 * to sleep.
 */
static int nfs4_handle_exception(const struct nfs_server *server, int errorcode, struct nfs4_exception *exception)
{
	struct nfs_client *clp = server->nfs_client;
	struct nfs4_state *state = exception->state;
	int ret = errorcode;

	exception->retry = 0;
	switch(errorcode) {
		case 0:
			return 0;
		case -NFS4ERR_ADMIN_REVOKED:
		case -NFS4ERR_BAD_STATEID:
		case -NFS4ERR_OPENMODE:
			if (state == NULL)
				break;
			nfs4_state_mark_reclaim_nograce(clp, state);
		case -NFS4ERR_STALE_CLIENTID:
		case -NFS4ERR_STALE_STATEID:
		case -NFS4ERR_EXPIRED:
			nfs4_schedule_state_recovery(clp);
			ret = nfs4_wait_clnt_recover(clp);
			if (ret == 0)
				exception->retry = 1;
#if !defined(CONFIG_NFS_V4_1)
			break;
#else /* !defined(CONFIG_NFS_V4_1) */
			if (!nfs4_has_session(server->nfs_client))
				break;
			/* FALLTHROUGH */
		case -NFS4ERR_BADSESSION:
		case -NFS4ERR_BADSLOT:
		case -NFS4ERR_BAD_HIGH_SLOT:
		case -NFS4ERR_CONN_NOT_BOUND_TO_SESSION:
		case -NFS4ERR_DEADSESSION:
		case -NFS4ERR_SEQ_FALSE_RETRY:
		case -NFS4ERR_SEQ_MISORDERED:
			dprintk("%s ERROR: %d Reset session\n", __func__,
				errorcode);
			set_bit(NFS4CLNT_SESSION_SETUP, &clp->cl_state);
			exception->retry = 1;
			/* FALLTHROUGH */
#endif /* !defined(CONFIG_NFS_V4_1) */
		case -NFS4ERR_FILE_OPEN:
		case -NFS4ERR_GRACE:
		case -NFS4ERR_DELAY:
			ret = nfs4_delay(server->client, &exception->timeout);
			if (ret != 0)
				break;
		case -NFS4ERR_OLD_STATEID:
			exception->retry = 1;
	}
	/* We failed to handle the error */
	return nfs4_map_errors(ret);
}


static void renew_lease(const struct nfs_server *server, unsigned long timestamp)
{
	struct nfs_client *clp = server->nfs_client;
	spin_lock(&clp->cl_lock);
	if (time_before(clp->cl_last_renewal,timestamp))
		clp->cl_last_renewal = timestamp;
	spin_unlock(&clp->cl_lock);
}

#if defined(CONFIG_NFS_V4_1)

/*
 * nfs4_free_slot - free a slot and efficiently update slot table.
 *
 * freeing a slot is trivially done by clearing its respective bit
 * in the bitmap.
 * If the freed slotid equals highest_used_slotid we want to update it
 * so that the server would be able to size down the slot table if needed,
 * otherwise we know that the highest_used_slotid is still in use.
 * When updating highest_used_slotid there may be "holes" in the bitmap
 * so we need to scan down from highest_used_slotid to 0 looking for the now
 * highest slotid in use.
 * If none found, highest_used_slotid is set to -1.
 */
static void
nfs4_free_slot(struct nfs4_slot_table *tbl, u8 free_slotid)
{
	int slotid = free_slotid;

	spin_lock(&tbl->slot_tbl_lock);
	/* clear used bit in bitmap */
	__clear_bit(slotid, tbl->used_slots);

	/* update highest_used_slotid when it is freed */
	if (slotid == tbl->highest_used_slotid) {
		slotid = find_last_bit(tbl->used_slots, tbl->max_slots);
		if (slotid >= 0 && slotid < tbl->max_slots)
			tbl->highest_used_slotid = slotid;
		else
			tbl->highest_used_slotid = -1;
	}
	rpc_wake_up_next(&tbl->slot_tbl_waitq);
	spin_unlock(&tbl->slot_tbl_lock);
	dprintk("%s: free_slotid %u highest_used_slotid %d\n", __func__,
		free_slotid, tbl->highest_used_slotid);
}

void nfs41_sequence_free_slot(const struct nfs_client *clp,
			      struct nfs4_sequence_res *res)
{
	struct nfs4_slot_table *tbl;

	if (!nfs4_has_session(clp)) {
		dprintk("%s: No session\n", __func__);
		return;
	}
	tbl = &clp->cl_session->fc_slot_table;
	if (res->sr_slotid == NFS4_MAX_SLOT_TABLE) {
		dprintk("%s: No slot\n", __func__);
		/* just wake up the next guy waiting since
		 * we may have not consumed a slot after all */
		rpc_wake_up_next(&tbl->slot_tbl_waitq);
		return;
	}
	nfs4_free_slot(tbl, res->sr_slotid);
	res->sr_slotid = NFS4_MAX_SLOT_TABLE;
}

static void nfs41_sequence_done(struct nfs_client *clp,
				struct nfs4_sequence_res *res,
				int rpc_status)
{
	unsigned long timestamp;
	struct nfs4_slot_table *tbl;
	struct nfs4_slot *slot;

	/*
	 * sr_status remains 1 if an RPC level error occurred. The server
	 * may or may not have processed the sequence operation..
	 * Proceed as if the server received and processed the sequence
	 * operation.
	 */
	if (res->sr_status == 1)
		res->sr_status = NFS_OK;

	/* -ERESTARTSYS can result in skipping nfs41_sequence_setup */
	if (res->sr_slotid == NFS4_MAX_SLOT_TABLE)
		goto out;

	tbl = &clp->cl_session->fc_slot_table;
	slot = tbl->slots + res->sr_slotid;

	if (res->sr_status == 0) {
		/* Update the slot's sequence and clientid lease timer */
		++slot->seq_nr;
		timestamp = res->sr_renewal_time;
		spin_lock(&clp->cl_lock);
		if (time_before(clp->cl_last_renewal, timestamp))
			clp->cl_last_renewal = timestamp;
		spin_unlock(&clp->cl_lock);
		return;
	}
out:
	/* The session may be reset by one of the error handlers. */
	dprintk("%s: Error %d free the slot \n", __func__, res->sr_status);
	nfs41_sequence_free_slot(clp, res);
}

/*
 * nfs4_find_slot - efficiently look for a free slot
 *
 * nfs4_find_slot looks for an unset bit in the used_slots bitmap.
 * If found, we mark the slot as used, update the highest_used_slotid,
 * and respectively set up the sequence operation args.
 * The slot number is returned if found, or NFS4_MAX_SLOT_TABLE otherwise.
 *
 * Note: must be called with under the slot_tbl_lock.
 */
static u8
nfs4_find_slot(struct nfs4_slot_table *tbl, struct rpc_task *task)
{
	int slotid;
	u8 ret_id = NFS4_MAX_SLOT_TABLE;
	BUILD_BUG_ON((u8)NFS4_MAX_SLOT_TABLE != (int)NFS4_MAX_SLOT_TABLE);

	dprintk("--> %s used_slots=%04lx highest_used=%d max_slots=%d\n",
		__func__, tbl->used_slots[0], tbl->highest_used_slotid,
		tbl->max_slots);
	slotid = find_first_zero_bit(tbl->used_slots, tbl->max_slots);
	if (slotid >= tbl->max_slots)
		goto out;
	__set_bit(slotid, tbl->used_slots);
	if (slotid > tbl->highest_used_slotid)
		tbl->highest_used_slotid = slotid;
	ret_id = slotid;
out:
	dprintk("<-- %s used_slots=%04lx highest_used=%d slotid=%d \n",
		__func__, tbl->used_slots[0], tbl->highest_used_slotid, ret_id);
	return ret_id;
}

static int nfs4_recover_session(struct nfs4_session *session)
{
	struct nfs_client *clp = session->clp;
	unsigned int loop;
	int ret;

	for (loop = NFS4_MAX_LOOP_ON_RECOVER; loop != 0; loop--) {
		ret = nfs4_wait_clnt_recover(clp);
		if (ret != 0)
			break;
		if (!test_bit(NFS4CLNT_SESSION_SETUP, &clp->cl_state))
			break;
		nfs4_schedule_state_manager(clp);
		ret = -EIO;
	}
	return ret;
}

static int nfs41_setup_sequence(struct nfs4_session *session,
				struct nfs4_sequence_args *args,
				struct nfs4_sequence_res *res,
				int cache_reply,
				struct rpc_task *task)
{
	struct nfs4_slot *slot;
	struct nfs4_slot_table *tbl;
	int status = 0;
	u8 slotid;

	dprintk("--> %s\n", __func__);
	/* slot already allocated? */
	if (res->sr_slotid != NFS4_MAX_SLOT_TABLE)
		return 0;

	memset(res, 0, sizeof(*res));
	res->sr_slotid = NFS4_MAX_SLOT_TABLE;
	tbl = &session->fc_slot_table;

	spin_lock(&tbl->slot_tbl_lock);
	if (test_bit(NFS4CLNT_SESSION_SETUP, &session->clp->cl_state)) {
		if (tbl->highest_used_slotid != -1) {
			rpc_sleep_on(&tbl->slot_tbl_waitq, task, NULL);
			spin_unlock(&tbl->slot_tbl_lock);
			dprintk("<-- %s: Session reset: draining\n", __func__);
			return -EAGAIN;
		}

		/* The slot table is empty; start the reset thread */
		dprintk("%s Session Reset\n", __func__);
		spin_unlock(&tbl->slot_tbl_lock);
		status = nfs4_recover_session(session);
		if (status)
			return status;
		spin_lock(&tbl->slot_tbl_lock);
	}

	slotid = nfs4_find_slot(tbl, task);
	if (slotid == NFS4_MAX_SLOT_TABLE) {
		rpc_sleep_on(&tbl->slot_tbl_waitq, task, NULL);
		spin_unlock(&tbl->slot_tbl_lock);
		dprintk("<-- %s: no free slots\n", __func__);
		return -EAGAIN;
	}
	spin_unlock(&tbl->slot_tbl_lock);

	slot = tbl->slots + slotid;
	args->sa_session = session;
	args->sa_slotid = slotid;
	args->sa_cache_this = cache_reply;

	dprintk("<-- %s slotid=%d seqid=%d\n", __func__, slotid, slot->seq_nr);

	res->sr_session = session;
	res->sr_slotid = slotid;
	res->sr_renewal_time = jiffies;
	/*
	 * sr_status is only set in decode_sequence, and so will remain
	 * set to 1 if an rpc level failure occurs.
	 */
	res->sr_status = 1;
	return 0;
}

int nfs4_setup_sequence(struct nfs_client *clp,
			struct nfs4_sequence_args *args,
			struct nfs4_sequence_res *res,
			int cache_reply,
			struct rpc_task *task)
{
	int ret = 0;

	dprintk("--> %s clp %p session %p sr_slotid %d\n",
		__func__, clp, clp->cl_session, res->sr_slotid);

	if (!nfs4_has_session(clp))
		goto out;
	ret = nfs41_setup_sequence(clp->cl_session, args, res, cache_reply,
				   task);
	if (ret != -EAGAIN) {
		/* terminate rpc task */
		task->tk_status = ret;
		task->tk_action = NULL;
	}
out:
	dprintk("<-- %s status=%d\n", __func__, ret);
	return ret;
}

struct nfs41_call_sync_data {
	struct nfs_client *clp;
	struct nfs4_sequence_args *seq_args;
	struct nfs4_sequence_res *seq_res;
	int cache_reply;
};

static void nfs41_call_sync_prepare(struct rpc_task *task, void *calldata)
{
	struct nfs41_call_sync_data *data = calldata;

	dprintk("--> %s data->clp->cl_session %p\n", __func__,
		data->clp->cl_session);
	if (nfs4_setup_sequence(data->clp, data->seq_args,
				data->seq_res, data->cache_reply, task))
		return;
	rpc_call_start(task);
}

static void nfs41_call_sync_done(struct rpc_task *task, void *calldata)
{
	struct nfs41_call_sync_data *data = calldata;

	nfs41_sequence_done(data->clp, data->seq_res, task->tk_status);
	nfs41_sequence_free_slot(data->clp, data->seq_res);
}

struct rpc_call_ops nfs41_call_sync_ops = {
	.rpc_call_prepare = nfs41_call_sync_prepare,
	.rpc_call_done = nfs41_call_sync_done,
};

static int nfs4_call_sync_sequence(struct nfs_client *clp,
				   struct rpc_clnt *clnt,
				   struct rpc_message *msg,
				   struct nfs4_sequence_args *args,
				   struct nfs4_sequence_res *res,
				   int cache_reply)
{
	int ret;
	struct rpc_task *task;
	struct nfs41_call_sync_data data = {
		.clp = clp,
		.seq_args = args,
		.seq_res = res,
		.cache_reply = cache_reply,
	};
	struct rpc_task_setup task_setup = {
		.rpc_client = clnt,
		.rpc_message = msg,
		.callback_ops = &nfs41_call_sync_ops,
		.callback_data = &data
	};

	res->sr_slotid = NFS4_MAX_SLOT_TABLE;
	task = rpc_run_task(&task_setup);
	if (IS_ERR(task))
		ret = PTR_ERR(task);
	else {
		ret = task->tk_status;
		rpc_put_task(task);
	}
	return ret;
}

int _nfs4_call_sync_session(struct nfs_server *server,
			    struct rpc_message *msg,
			    struct nfs4_sequence_args *args,
			    struct nfs4_sequence_res *res,
			    int cache_reply)
{
	return nfs4_call_sync_sequence(server->nfs_client, server->client,
				       msg, args, res, cache_reply);
}

#endif /* CONFIG_NFS_V4_1 */

int _nfs4_call_sync(struct nfs_server *server,
		    struct rpc_message *msg,
		    struct nfs4_sequence_args *args,
		    struct nfs4_sequence_res *res,
		    int cache_reply)
{
	args->sa_session = res->sr_session = NULL;
	return rpc_call_sync(server->client, msg, 0);
}

#define nfs4_call_sync(server, msg, args, res, cache_reply) \
	(server)->nfs_client->cl_call_sync((server), (msg), &(args)->seq_args, \
			&(res)->seq_res, (cache_reply))

static void nfs4_sequence_done(const struct nfs_server *server,
			       struct nfs4_sequence_res *res, int rpc_status)
{
#ifdef CONFIG_NFS_V4_1
	if (nfs4_has_session(server->nfs_client))
		nfs41_sequence_done(server->nfs_client, res, rpc_status);
#endif /* CONFIG_NFS_V4_1 */
}

/* no restart, therefore free slot here */
static void nfs4_sequence_done_free_slot(const struct nfs_server *server,
					 struct nfs4_sequence_res *res,
					 int rpc_status)
{
	nfs4_sequence_done(server, res, rpc_status);
	nfs4_sequence_free_slot(server->nfs_client, res);
}

static void update_changeattr(struct inode *dir, struct nfs4_change_info *cinfo)
{
	struct nfs_inode *nfsi = NFS_I(dir);

	spin_lock(&dir->i_lock);
	nfsi->cache_validity |= NFS_INO_INVALID_ATTR|NFS_INO_REVAL_PAGECACHE|NFS_INO_INVALID_DATA;
	if (!cinfo->atomic || cinfo->before != nfsi->change_attr)
		nfs_force_lookup_revalidate(dir);
	nfsi->change_attr = cinfo->after;
	spin_unlock(&dir->i_lock);
}

struct nfs4_opendata {
	struct kref kref;
	struct nfs_openargs o_arg;
	struct nfs_openres o_res;
	struct nfs_open_confirmargs c_arg;
	struct nfs_open_confirmres c_res;
	struct nfs_fattr f_attr;
	struct nfs_fattr dir_attr;
	struct path path;
	struct dentry *dir;
	struct nfs4_state_owner *owner;
	struct nfs4_state *state;
	struct iattr attrs;
	unsigned long timestamp;
	unsigned int rpc_done : 1;
	int rpc_status;
	int cancelled;
};


static void nfs4_init_opendata_res(struct nfs4_opendata *p)
{
	p->o_res.f_attr = &p->f_attr;
	p->o_res.dir_attr = &p->dir_attr;
	p->o_res.seqid = p->o_arg.seqid;
	p->c_res.seqid = p->c_arg.seqid;
	p->o_res.server = p->o_arg.server;
	nfs_fattr_init(&p->f_attr);
	nfs_fattr_init(&p->dir_attr);
	p->o_res.seq_res.sr_slotid = NFS4_MAX_SLOT_TABLE;
}

static struct nfs4_opendata *nfs4_opendata_alloc(struct path *path,
		struct nfs4_state_owner *sp, fmode_t fmode, int flags,
		const struct iattr *attrs)
{
	struct dentry *parent = dget_parent(path->dentry);
	struct inode *dir = parent->d_inode;
	struct nfs_server *server = NFS_SERVER(dir);
	struct nfs4_opendata *p;

	p = kzalloc(sizeof(*p), GFP_KERNEL);
	if (p == NULL)
		goto err;
	p->o_arg.seqid = nfs_alloc_seqid(&sp->so_seqid);
	if (p->o_arg.seqid == NULL)
		goto err_free;
	p->path.mnt = mntget(path->mnt);
	p->path.dentry = dget(path->dentry);
	p->dir = parent;
	p->owner = sp;
	atomic_inc(&sp->so_count);
	p->o_arg.fh = NFS_FH(dir);
	p->o_arg.open_flags = flags;
	p->o_arg.fmode = fmode & (FMODE_READ|FMODE_WRITE);
	p->o_arg.clientid = server->nfs_client->cl_clientid;
	p->o_arg.id = sp->so_owner_id.id;
	p->o_arg.name = &p->path.dentry->d_name;
	p->o_arg.server = server;
	p->o_arg.bitmask = server->attr_bitmask;
	p->o_arg.claim = NFS4_OPEN_CLAIM_NULL;
	if (flags & O_EXCL) {
		u32 *s = (u32 *) p->o_arg.u.verifier.data;
		s[0] = jiffies;
		s[1] = current->pid;
	} else if (flags & O_CREAT) {
		p->o_arg.u.attrs = &p->attrs;
		memcpy(&p->attrs, attrs, sizeof(p->attrs));
	}
	p->c_arg.fh = &p->o_res.fh;
	p->c_arg.stateid = &p->o_res.stateid;
	p->c_arg.seqid = p->o_arg.seqid;
	nfs4_init_opendata_res(p);
	kref_init(&p->kref);
	return p;
err_free:
	kfree(p);
err:
	dput(parent);
	return NULL;
}

static void nfs4_opendata_free(struct kref *kref)
{
	struct nfs4_opendata *p = container_of(kref,
			struct nfs4_opendata, kref);

	nfs_free_seqid(p->o_arg.seqid);
	if (p->state != NULL)
		nfs4_put_open_state(p->state);
	nfs4_put_state_owner(p->owner);
	dput(p->dir);
	path_put(&p->path);
	kfree(p);
}

static void nfs4_opendata_put(struct nfs4_opendata *p)
{
	if (p != NULL)
		kref_put(&p->kref, nfs4_opendata_free);
}

static int nfs4_wait_for_completion_rpc_task(struct rpc_task *task)
{
	int ret;

	ret = rpc_wait_for_completion_task(task);
	return ret;
}

static int can_open_cached(struct nfs4_state *state, fmode_t mode, int open_mode)
{
	int ret = 0;

	if (open_mode & O_EXCL)
		goto out;
	switch (mode & (FMODE_READ|FMODE_WRITE)) {
		case FMODE_READ:
			ret |= test_bit(NFS_O_RDONLY_STATE, &state->flags) != 0;
			break;
		case FMODE_WRITE:
			ret |= test_bit(NFS_O_WRONLY_STATE, &state->flags) != 0;
			break;
		case FMODE_READ|FMODE_WRITE:
			ret |= test_bit(NFS_O_RDWR_STATE, &state->flags) != 0;
	}
out:
	return ret;
}

static int can_open_delegated(struct nfs_delegation *delegation, fmode_t fmode)
{
	if ((delegation->type & fmode) != fmode)
		return 0;
	if (test_bit(NFS_DELEGATION_NEED_RECLAIM, &delegation->flags))
		return 0;
	nfs_mark_delegation_referenced(delegation);
	return 1;
}

static void update_open_stateflags(struct nfs4_state *state, fmode_t fmode)
{
	switch (fmode) {
		case FMODE_WRITE:
			state->n_wronly++;
			break;
		case FMODE_READ:
			state->n_rdonly++;
			break;
		case FMODE_READ|FMODE_WRITE:
			state->n_rdwr++;
	}
	nfs4_state_set_mode_locked(state, state->state | fmode);
}

static void nfs_set_open_stateid_locked(struct nfs4_state *state, nfs4_stateid *stateid, fmode_t fmode)
{
	if (test_bit(NFS_DELEGATED_STATE, &state->flags) == 0)
		memcpy(state->stateid.data, stateid->data, sizeof(state->stateid.data));
	memcpy(state->open_stateid.data, stateid->data, sizeof(state->open_stateid.data));
	switch (fmode) {
		case FMODE_READ:
			set_bit(NFS_O_RDONLY_STATE, &state->flags);
			break;
		case FMODE_WRITE:
			set_bit(NFS_O_WRONLY_STATE, &state->flags);
			break;
		case FMODE_READ|FMODE_WRITE:
			set_bit(NFS_O_RDWR_STATE, &state->flags);
	}
}

static void nfs_set_open_stateid(struct nfs4_state *state, nfs4_stateid *stateid, fmode_t fmode)
{
	write_seqlock(&state->seqlock);
	nfs_set_open_stateid_locked(state, stateid, fmode);
	write_sequnlock(&state->seqlock);
}

static void __update_open_stateid(struct nfs4_state *state, nfs4_stateid *open_stateid, const nfs4_stateid *deleg_stateid, fmode_t fmode)
{
	/*
	 * Protect the call to nfs4_state_set_mode_locked and
	 * serialise the stateid update
	 */
	write_seqlock(&state->seqlock);
	if (deleg_stateid != NULL) {
		memcpy(state->stateid.data, deleg_stateid->data, sizeof(state->stateid.data));
		set_bit(NFS_DELEGATED_STATE, &state->flags);
	}
	if (open_stateid != NULL)
		nfs_set_open_stateid_locked(state, open_stateid, fmode);
	write_sequnlock(&state->seqlock);
	spin_lock(&state->owner->so_lock);
	update_open_stateflags(state, fmode);
	spin_unlock(&state->owner->so_lock);
}

static int update_open_stateid(struct nfs4_state *state, nfs4_stateid *open_stateid, nfs4_stateid *delegation, fmode_t fmode)
{
	struct nfs_inode *nfsi = NFS_I(state->inode);
	struct nfs_delegation *deleg_cur;
	int ret = 0;

	fmode &= (FMODE_READ|FMODE_WRITE);

	rcu_read_lock();
	deleg_cur = rcu_dereference(nfsi->delegation);
	if (deleg_cur == NULL)
		goto no_delegation;

	spin_lock(&deleg_cur->lock);
	if (nfsi->delegation != deleg_cur ||
	    (deleg_cur->type & fmode) != fmode)
		goto no_delegation_unlock;

	if (delegation == NULL)
		delegation = &deleg_cur->stateid;
	else if (memcmp(deleg_cur->stateid.data, delegation->data, NFS4_STATEID_SIZE) != 0)
		goto no_delegation_unlock;

	nfs_mark_delegation_referenced(deleg_cur);
	__update_open_stateid(state, open_stateid, &deleg_cur->stateid, fmode);
	ret = 1;
no_delegation_unlock:
	spin_unlock(&deleg_cur->lock);
no_delegation:
	rcu_read_unlock();

	if (!ret && open_stateid != NULL) {
		__update_open_stateid(state, open_stateid, NULL, fmode);
		ret = 1;
	}

	return ret;
}


static void nfs4_return_incompatible_delegation(struct inode *inode, fmode_t fmode)
{
	struct nfs_delegation *delegation;

	rcu_read_lock();
	delegation = rcu_dereference(NFS_I(inode)->delegation);
	if (delegation == NULL || (delegation->type & fmode) == fmode) {
		rcu_read_unlock();
		return;
	}
	rcu_read_unlock();
	nfs_inode_return_delegation(inode);
}

static struct nfs4_state *nfs4_try_open_cached(struct nfs4_opendata *opendata)
{
	struct nfs4_state *state = opendata->state;
	struct nfs_inode *nfsi = NFS_I(state->inode);
	struct nfs_delegation *delegation;
	int open_mode = opendata->o_arg.open_flags & O_EXCL;
	fmode_t fmode = opendata->o_arg.fmode;
	nfs4_stateid stateid;
	int ret = -EAGAIN;

	for (;;) {
		if (can_open_cached(state, fmode, open_mode)) {
			spin_lock(&state->owner->so_lock);
			if (can_open_cached(state, fmode, open_mode)) {
				update_open_stateflags(state, fmode);
				spin_unlock(&state->owner->so_lock);
				goto out_return_state;
			}
			spin_unlock(&state->owner->so_lock);
		}
		rcu_read_lock();
		delegation = rcu_dereference(nfsi->delegation);
		if (delegation == NULL ||
		    !can_open_delegated(delegation, fmode)) {
			rcu_read_unlock();
			break;
		}
		/* Save the delegation */
		memcpy(stateid.data, delegation->stateid.data, sizeof(stateid.data));
		rcu_read_unlock();
		ret = nfs_may_open(state->inode, state->owner->so_cred, open_mode);
		if (ret != 0)
			goto out;
		ret = -EAGAIN;

		/* Try to update the stateid using the delegation */
		if (update_open_stateid(state, NULL, &stateid, fmode))
			goto out_return_state;
	}
out:
	return ERR_PTR(ret);
out_return_state:
	atomic_inc(&state->count);
	return state;
}

static struct nfs4_state *nfs4_opendata_to_nfs4_state(struct nfs4_opendata *data)
{
	struct inode *inode;
	struct nfs4_state *state = NULL;
	struct nfs_delegation *delegation;
	int ret;

	if (!data->rpc_done) {
		state = nfs4_try_open_cached(data);
		goto out;
	}

	ret = -EAGAIN;
	if (!(data->f_attr.valid & NFS_ATTR_FATTR))
		goto err;
	inode = nfs_fhget(data->dir->d_sb, &data->o_res.fh, &data->f_attr);
	ret = PTR_ERR(inode);
	if (IS_ERR(inode))
		goto err;
	ret = -ENOMEM;
	state = nfs4_get_open_state(inode, data->owner);
	if (state == NULL)
		goto err_put_inode;
	if (data->o_res.delegation_type != 0) {
		int delegation_flags = 0;

		rcu_read_lock();
		delegation = rcu_dereference(NFS_I(inode)->delegation);
		if (delegation)
			delegation_flags = delegation->flags;
		rcu_read_unlock();
		if ((delegation_flags & 1UL<<NFS_DELEGATION_NEED_RECLAIM) == 0)
			nfs_inode_set_delegation(state->inode,
					data->owner->so_cred,
					&data->o_res);
		else
			nfs_inode_reclaim_delegation(state->inode,
					data->owner->so_cred,
					&data->o_res);
	}

	update_open_stateid(state, &data->o_res.stateid, NULL,
			data->o_arg.fmode);
	iput(inode);
out:
	return state;
err_put_inode:
	iput(inode);
err:
	return ERR_PTR(ret);
}

static struct nfs_open_context *nfs4_state_find_open_context(struct nfs4_state *state)
{
	struct nfs_inode *nfsi = NFS_I(state->inode);
	struct nfs_open_context *ctx;

	spin_lock(&state->inode->i_lock);
	list_for_each_entry(ctx, &nfsi->open_files, list) {
		if (ctx->state != state)
			continue;
		get_nfs_open_context(ctx);
		spin_unlock(&state->inode->i_lock);
		return ctx;
	}
	spin_unlock(&state->inode->i_lock);
	return ERR_PTR(-ENOENT);
}

static struct nfs4_opendata *nfs4_open_recoverdata_alloc(struct nfs_open_context *ctx, struct nfs4_state *state)
{
	struct nfs4_opendata *opendata;

	opendata = nfs4_opendata_alloc(&ctx->path, state->owner, 0, 0, NULL);
	if (opendata == NULL)
		return ERR_PTR(-ENOMEM);
	opendata->state = state;
	atomic_inc(&state->count);
	return opendata;
}

static int nfs4_open_recover_helper(struct nfs4_opendata *opendata, fmode_t fmode, struct nfs4_state **res)
{
	struct nfs4_state *newstate;
	int ret;

	opendata->o_arg.open_flags = 0;
	opendata->o_arg.fmode = fmode;
	memset(&opendata->o_res, 0, sizeof(opendata->o_res));
	memset(&opendata->c_res, 0, sizeof(opendata->c_res));
	nfs4_init_opendata_res(opendata);
	ret = _nfs4_proc_open(opendata);
	if (ret != 0)
		return ret; 
	newstate = nfs4_opendata_to_nfs4_state(opendata);
	if (IS_ERR(newstate))
		return PTR_ERR(newstate);
	nfs4_close_state(&opendata->path, newstate, fmode);
	*res = newstate;
	return 0;
}

static int nfs4_open_recover(struct nfs4_opendata *opendata, struct nfs4_state *state)
{
	struct nfs4_state *newstate;
	int ret;

	/* memory barrier prior to reading state->n_* */
	clear_bit(NFS_DELEGATED_STATE, &state->flags);
	smp_rmb();
	if (state->n_rdwr != 0) {
		ret = nfs4_open_recover_helper(opendata, FMODE_READ|FMODE_WRITE, &newstate);
		if (ret != 0)
			return ret;
		if (newstate != state)
			return -ESTALE;
	}
	if (state->n_wronly != 0) {
		ret = nfs4_open_recover_helper(opendata, FMODE_WRITE, &newstate);
		if (ret != 0)
			return ret;
		if (newstate != state)
			return -ESTALE;
	}
	if (state->n_rdonly != 0) {
		ret = nfs4_open_recover_helper(opendata, FMODE_READ, &newstate);
		if (ret != 0)
			return ret;
		if (newstate != state)
			return -ESTALE;
	}
	/*
	 * We may have performed cached opens for all three recoveries.
	 * Check if we need to update the current stateid.
	 */
	if (test_bit(NFS_DELEGATED_STATE, &state->flags) == 0 &&
	    memcmp(state->stateid.data, state->open_stateid.data, sizeof(state->stateid.data)) != 0) {
		write_seqlock(&state->seqlock);
		if (test_bit(NFS_DELEGATED_STATE, &state->flags) == 0)
			memcpy(state->stateid.data, state->open_stateid.data, sizeof(state->stateid.data));
		write_sequnlock(&state->seqlock);
	}
	return 0;
}

/*
 * OPEN_RECLAIM:
 * 	reclaim state on the server after a reboot.
 */
static int _nfs4_do_open_reclaim(struct nfs_open_context *ctx, struct nfs4_state *state)
{
	struct nfs_delegation *delegation;
	struct nfs4_opendata *opendata;
	fmode_t delegation_type = 0;
	int status;

	opendata = nfs4_open_recoverdata_alloc(ctx, state);
	if (IS_ERR(opendata))
		return PTR_ERR(opendata);
	opendata->o_arg.claim = NFS4_OPEN_CLAIM_PREVIOUS;
	opendata->o_arg.fh = NFS_FH(state->inode);
	rcu_read_lock();
	delegation = rcu_dereference(NFS_I(state->inode)->delegation);
	if (delegation != NULL && test_bit(NFS_DELEGATION_NEED_RECLAIM, &delegation->flags) != 0)
		delegation_type = delegation->type;
	rcu_read_unlock();
	opendata->o_arg.u.delegation_type = delegation_type;
	status = nfs4_open_recover(opendata, state);
	nfs4_opendata_put(opendata);
	return status;
}

static int nfs4_do_open_reclaim(struct nfs_open_context *ctx, struct nfs4_state *state)
{
	struct nfs_server *server = NFS_SERVER(state->inode);
	struct nfs4_exception exception = { };
	int err;
	do {
		err = _nfs4_do_open_reclaim(ctx, state);
		if (err != -NFS4ERR_DELAY)
			break;
		nfs4_handle_exception(server, err, &exception);
	} while (exception.retry);
	return err;
}

static int nfs4_open_reclaim(struct nfs4_state_owner *sp, struct nfs4_state *state)
{
	struct nfs_open_context *ctx;
	int ret;

	ctx = nfs4_state_find_open_context(state);
	if (IS_ERR(ctx))
		return PTR_ERR(ctx);
	ret = nfs4_do_open_reclaim(ctx, state);
	put_nfs_open_context(ctx);
	return ret;
}

static int _nfs4_open_delegation_recall(struct nfs_open_context *ctx, struct nfs4_state *state, const nfs4_stateid *stateid)
{
	struct nfs4_opendata *opendata;
	int ret;

	opendata = nfs4_open_recoverdata_alloc(ctx, state);
	if (IS_ERR(opendata))
		return PTR_ERR(opendata);
	opendata->o_arg.claim = NFS4_OPEN_CLAIM_DELEGATE_CUR;
	memcpy(opendata->o_arg.u.delegation.data, stateid->data,
			sizeof(opendata->o_arg.u.delegation.data));
	ret = nfs4_open_recover(opendata, state);
	nfs4_opendata_put(opendata);
	return ret;
}

int nfs4_open_delegation_recall(struct nfs_open_context *ctx, struct nfs4_state *state, const nfs4_stateid *stateid)
{
	struct nfs4_exception exception = { };
	struct nfs_server *server = NFS_SERVER(state->inode);
	int err;
	do {
		err = _nfs4_open_delegation_recall(ctx, state, stateid);
		switch (err) {
			case 0:
			case -ENOENT:
			case -ESTALE:
				goto out;
			case -NFS4ERR_STALE_CLIENTID:
			case -NFS4ERR_STALE_STATEID:
			case -NFS4ERR_EXPIRED:
				/* Don't recall a delegation if it was lost */
				nfs4_schedule_state_recovery(server->nfs_client);
				goto out;
			case -ERESTARTSYS:
				/*
				 * The show must go on: exit, but mark the
				 * stateid as needing recovery.
				 */
			case -NFS4ERR_ADMIN_REVOKED:
			case -NFS4ERR_BAD_STATEID:
				nfs4_state_mark_reclaim_nograce(server->nfs_client, state);
			case -ENOMEM:
				err = 0;
				goto out;
		}
		err = nfs4_handle_exception(server, err, &exception);
	} while (exception.retry);
out:
	return err;
}

static void nfs4_open_confirm_done(struct rpc_task *task, void *calldata)
{
	struct nfs4_opendata *data = calldata;

	data->rpc_status = task->tk_status;
	if (RPC_ASSASSINATED(task))
		return;
	if (data->rpc_status == 0) {
		memcpy(data->o_res.stateid.data, data->c_res.stateid.data,
				sizeof(data->o_res.stateid.data));
		nfs_confirm_seqid(&data->owner->so_seqid, 0);
		renew_lease(data->o_res.server, data->timestamp);
		data->rpc_done = 1;
	}
}

static void nfs4_open_confirm_release(void *calldata)
{
	struct nfs4_opendata *data = calldata;
	struct nfs4_state *state = NULL;

	/* If this request hasn't been cancelled, do nothing */
	if (data->cancelled == 0)
		goto out_free;
	/* In case of error, no cleanup! */
	if (!data->rpc_done)
		goto out_free;
	state = nfs4_opendata_to_nfs4_state(data);
	if (!IS_ERR(state))
		nfs4_close_state(&data->path, state, data->o_arg.fmode);
out_free:
	nfs4_opendata_put(data);
}

static const struct rpc_call_ops nfs4_open_confirm_ops = {
	.rpc_call_done = nfs4_open_confirm_done,
	.rpc_release = nfs4_open_confirm_release,
};

/*
 * Note: On error, nfs4_proc_open_confirm will free the struct nfs4_opendata
 */
static int _nfs4_proc_open_confirm(struct nfs4_opendata *data)
{
	struct nfs_server *server = NFS_SERVER(data->dir->d_inode);
	struct rpc_task *task;
	struct  rpc_message msg = {
		.rpc_proc = &nfs4_procedures[NFSPROC4_CLNT_OPEN_CONFIRM],
		.rpc_argp = &data->c_arg,
		.rpc_resp = &data->c_res,
		.rpc_cred = data->owner->so_cred,
	};
	struct rpc_task_setup task_setup_data = {
		.rpc_client = server->client,
		.rpc_message = &msg,
		.callback_ops = &nfs4_open_confirm_ops,
		.callback_data = data,
		.workqueue = nfsiod_workqueue,
		.flags = RPC_TASK_ASYNC,
	};
	int status;

	kref_get(&data->kref);
	data->rpc_done = 0;
	data->rpc_status = 0;
	data->timestamp = jiffies;
	task = rpc_run_task(&task_setup_data);
	if (IS_ERR(task))
		return PTR_ERR(task);
	status = nfs4_wait_for_completion_rpc_task(task);
	if (status != 0) {
		data->cancelled = 1;
		smp_wmb();
	} else
		status = data->rpc_status;
	rpc_put_task(task);
	return status;
}

static void nfs4_open_prepare(struct rpc_task *task, void *calldata)
{
	struct nfs4_opendata *data = calldata;
	struct nfs4_state_owner *sp = data->owner;

	if (nfs_wait_on_sequence(data->o_arg.seqid, task) != 0)
		return;
	/*
	 * Check if we still need to send an OPEN call, or if we can use
	 * a delegation instead.
	 */
	if (data->state != NULL) {
		struct nfs_delegation *delegation;

		if (can_open_cached(data->state, data->o_arg.fmode, data->o_arg.open_flags))
			goto out_no_action;
		rcu_read_lock();
		delegation = rcu_dereference(NFS_I(data->state->inode)->delegation);
		if (delegation != NULL &&
		    test_bit(NFS_DELEGATION_NEED_RECLAIM, &delegation->flags) == 0) {
			rcu_read_unlock();
			goto out_no_action;
		}
		rcu_read_unlock();
	}
	/* Update sequence id. */
	data->o_arg.id = sp->so_owner_id.id;
	data->o_arg.clientid = sp->so_client->cl_clientid;
	if (data->o_arg.claim == NFS4_OPEN_CLAIM_PREVIOUS) {
		task->tk_msg.rpc_proc = &nfs4_procedures[NFSPROC4_CLNT_OPEN_NOATTR];
		nfs_copy_fh(&data->o_res.fh, data->o_arg.fh);
	}
	data->timestamp = jiffies;
	if (nfs4_setup_sequence(data->o_arg.server->nfs_client,
				&data->o_arg.seq_args,
				&data->o_res.seq_res, 1, task))
		return;
	rpc_call_start(task);
	return;
out_no_action:
	task->tk_action = NULL;

}

static void nfs4_open_done(struct rpc_task *task, void *calldata)
{
	struct nfs4_opendata *data = calldata;

	data->rpc_status = task->tk_status;

	nfs4_sequence_done_free_slot(data->o_arg.server, &data->o_res.seq_res,
				     task->tk_status);

	if (RPC_ASSASSINATED(task))
		return;
	if (task->tk_status == 0) {
		switch (data->o_res.f_attr->mode & S_IFMT) {
			case S_IFREG:
				break;
			case S_IFLNK:
				data->rpc_status = -ELOOP;
				break;
			case S_IFDIR:
				data->rpc_status = -EISDIR;
				break;
			default:
				data->rpc_status = -ENOTDIR;
		}
		renew_lease(data->o_res.server, data->timestamp);
		if (!(data->o_res.rflags & NFS4_OPEN_RESULT_CONFIRM))
			nfs_confirm_seqid(&data->owner->so_seqid, 0);
	}
	data->rpc_done = 1;
}

static void nfs4_open_release(void *calldata)
{
	struct nfs4_opendata *data = calldata;
	struct nfs4_state *state = NULL;

	/* If this request hasn't been cancelled, do nothing */
	if (data->cancelled == 0)
		goto out_free;
	/* In case of error, no cleanup! */
	if (data->rpc_status != 0 || !data->rpc_done)
		goto out_free;
	/* In case we need an open_confirm, no cleanup! */
	if (data->o_res.rflags & NFS4_OPEN_RESULT_CONFIRM)
		goto out_free;
	state = nfs4_opendata_to_nfs4_state(data);
	if (!IS_ERR(state))
		nfs4_close_state(&data->path, state, data->o_arg.fmode);
out_free:
	nfs4_opendata_put(data);
}

static const struct rpc_call_ops nfs4_open_ops = {
	.rpc_call_prepare = nfs4_open_prepare,
	.rpc_call_done = nfs4_open_done,
	.rpc_release = nfs4_open_release,
};

/*
 * Note: On error, nfs4_proc_open will free the struct nfs4_opendata
 */
static int _nfs4_proc_open(struct nfs4_opendata *data)
{
	struct inode *dir = data->dir->d_inode;
	struct nfs_server *server = NFS_SERVER(dir);
	struct nfs_openargs *o_arg = &data->o_arg;
	struct nfs_openres *o_res = &data->o_res;
	struct rpc_task *task;
	struct rpc_message msg = {
		.rpc_proc = &nfs4_procedures[NFSPROC4_CLNT_OPEN],
		.rpc_argp = o_arg,
		.rpc_resp = o_res,
		.rpc_cred = data->owner->so_cred,
	};
	struct rpc_task_setup task_setup_data = {
		.rpc_client = server->client,
		.rpc_message = &msg,
		.callback_ops = &nfs4_open_ops,
		.callback_data = data,
		.workqueue = nfsiod_workqueue,
		.flags = RPC_TASK_ASYNC,
	};
	int status;

	kref_get(&data->kref);
	data->rpc_done = 0;
	data->rpc_status = 0;
	data->cancelled = 0;
	task = rpc_run_task(&task_setup_data);
	if (IS_ERR(task))
		return PTR_ERR(task);
	status = nfs4_wait_for_completion_rpc_task(task);
	if (status != 0) {
		data->cancelled = 1;
		smp_wmb();
	} else
		status = data->rpc_status;
	rpc_put_task(task);
	if (status != 0 || !data->rpc_done)
		return status;

	if (o_res->fh.size == 0)
		_nfs4_proc_lookup(dir, o_arg->name, &o_res->fh, o_res->f_attr);

	if (o_arg->open_flags & O_CREAT) {
		update_changeattr(dir, &o_res->cinfo);
		nfs_post_op_update_inode(dir, o_res->dir_attr);
	} else
		nfs_refresh_inode(dir, o_res->dir_attr);
	if(o_res->rflags & NFS4_OPEN_RESULT_CONFIRM) {
		status = _nfs4_proc_open_confirm(data);
		if (status != 0)
			return status;
	}
	if (!(o_res->f_attr->valid & NFS_ATTR_FATTR))
		_nfs4_proc_getattr(server, &o_res->fh, o_res->f_attr);
	return 0;
}

static int nfs4_recover_expired_lease(struct nfs_server *server)
{
	struct nfs_client *clp = server->nfs_client;
	unsigned int loop;
	int ret;

	for (loop = NFS4_MAX_LOOP_ON_RECOVER; loop != 0; loop--) {
		ret = nfs4_wait_clnt_recover(clp);
		if (ret != 0)
			break;
		if (!test_bit(NFS4CLNT_LEASE_EXPIRED, &clp->cl_state) &&
		    !test_bit(NFS4CLNT_CHECK_LEASE,&clp->cl_state))
			break;
		nfs4_schedule_state_recovery(clp);
		ret = -EIO;
	}
	return ret;
}

/*
 * OPEN_EXPIRED:
 * 	reclaim state on the server after a network partition.
 * 	Assumes caller holds the appropriate lock
 */
static int _nfs4_open_expired(struct nfs_open_context *ctx, struct nfs4_state *state)
{
	struct nfs4_opendata *opendata;
	int ret;

	opendata = nfs4_open_recoverdata_alloc(ctx, state);
	if (IS_ERR(opendata))
		return PTR_ERR(opendata);
	ret = nfs4_open_recover(opendata, state);
	if (ret == -ESTALE)
		d_drop(ctx->path.dentry);
	nfs4_opendata_put(opendata);
	return ret;
}

static inline int nfs4_do_open_expired(struct nfs_open_context *ctx, struct nfs4_state *state)
{
	struct nfs_server *server = NFS_SERVER(state->inode);
	struct nfs4_exception exception = { };
	int err;

	do {
		err = _nfs4_open_expired(ctx, state);
		if (err != -NFS4ERR_DELAY)
			break;
		nfs4_handle_exception(server, err, &exception);
	} while (exception.retry);
	return err;
}

static int nfs4_open_expired(struct nfs4_state_owner *sp, struct nfs4_state *state)
{
	struct nfs_open_context *ctx;
	int ret;

	ctx = nfs4_state_find_open_context(state);
	if (IS_ERR(ctx))
		return PTR_ERR(ctx);
	ret = nfs4_do_open_expired(ctx, state);
	put_nfs_open_context(ctx);
	return ret;
}

/*
 * on an EXCLUSIVE create, the server should send back a bitmask with FATTR4-*
 * fields corresponding to attributes that were used to store the verifier.
 * Make sure we clobber those fields in the later setattr call
 */
static inline void nfs4_exclusive_attrset(struct nfs4_opendata *opendata, struct iattr *sattr)
{
	if ((opendata->o_res.attrset[1] & FATTR4_WORD1_TIME_ACCESS) &&
	    !(sattr->ia_valid & ATTR_ATIME_SET))
		sattr->ia_valid |= ATTR_ATIME;

	if ((opendata->o_res.attrset[1] & FATTR4_WORD1_TIME_MODIFY) &&
	    !(sattr->ia_valid & ATTR_MTIME_SET))
		sattr->ia_valid |= ATTR_MTIME;
}

/*
 * Returns a referenced nfs4_state
 */
static int _nfs4_do_open(struct inode *dir, struct path *path, fmode_t fmode, int flags, struct iattr *sattr, struct rpc_cred *cred, struct nfs4_state **res)
{
	struct nfs4_state_owner  *sp;
	struct nfs4_state     *state = NULL;
	struct nfs_server       *server = NFS_SERVER(dir);
	struct nfs4_opendata *opendata;
	int status;

	/* Protect against reboot recovery conflicts */
	status = -ENOMEM;
	if (!(sp = nfs4_get_state_owner(server, cred))) {
		dprintk("nfs4_do_open: nfs4_get_state_owner failed!\n");
		goto out_err;
	}
	status = nfs4_recover_expired_lease(server);
	if (status != 0)
		goto err_put_state_owner;
	if (path->dentry->d_inode != NULL)
		nfs4_return_incompatible_delegation(path->dentry->d_inode, fmode);
	status = -ENOMEM;
	opendata = nfs4_opendata_alloc(path, sp, fmode, flags, sattr);
	if (opendata == NULL)
		goto err_put_state_owner;

	if (path->dentry->d_inode != NULL)
		opendata->state = nfs4_get_open_state(path->dentry->d_inode, sp);

	status = _nfs4_proc_open(opendata);
	if (status != 0)
		goto err_opendata_put;

	if (opendata->o_arg.open_flags & O_EXCL)
		nfs4_exclusive_attrset(opendata, sattr);

	state = nfs4_opendata_to_nfs4_state(opendata);
	status = PTR_ERR(state);
	if (IS_ERR(state))
		goto err_opendata_put;
	nfs4_opendata_put(opendata);
	nfs4_put_state_owner(sp);
	*res = state;
	return 0;
err_opendata_put:
	nfs4_opendata_put(opendata);
err_put_state_owner:
	nfs4_put_state_owner(sp);
out_err:
	*res = NULL;
	return status;
}


static struct nfs4_state *nfs4_do_open(struct inode *dir, struct path *path, fmode_t fmode, int flags, struct iattr *sattr, struct rpc_cred *cred)
{
	struct nfs4_exception exception = { };
	struct nfs4_state *res;
	int status;

	do {
		status = _nfs4_do_open(dir, path, fmode, flags, sattr, cred, &res);
		if (status == 0)
			break;
		/* NOTE: BAD_SEQID means the server and client disagree about the
		 * book-keeping w.r.t. state-changing operations
		 * (OPEN/CLOSE/LOCK/LOCKU...)
		 * It is actually a sign of a bug on the client or on the server.
		 *
		 * If we receive a BAD_SEQID error in the particular case of
		 * doing an OPEN, we assume that nfs_increment_open_seqid() will
		 * have unhashed the old state_owner for us, and that we can
		 * therefore safely retry using a new one. We should still warn
		 * the user though...
		 */
		if (status == -NFS4ERR_BAD_SEQID) {
			printk(KERN_WARNING "NFS: v4 server %s "
					" returned a bad sequence-id error!\n",
					NFS_SERVER(dir)->nfs_client->cl_hostname);
			exception.retry = 1;
			continue;
		}
		/*
		 * BAD_STATEID on OPEN means that the server cancelled our
		 * state before it received the OPEN_CONFIRM.
		 * Recover by retrying the request as per the discussion
		 * on Page 181 of RFC3530.
		 */
		if (status == -NFS4ERR_BAD_STATEID) {
			exception.retry = 1;
			continue;
		}
		if (status == -EAGAIN) {
			/* We must have found a delegation */
			exception.retry = 1;
			continue;
		}
		res = ERR_PTR(nfs4_handle_exception(NFS_SERVER(dir),
					status, &exception));
	} while (exception.retry);
	return res;
}

static int _nfs4_do_setattr(struct inode *inode, struct rpc_cred *cred,
			    struct nfs_fattr *fattr, struct iattr *sattr,
			    struct nfs4_state *state)
{
	struct nfs_server *server = NFS_SERVER(inode);
        struct nfs_setattrargs  arg = {
                .fh             = NFS_FH(inode),
                .iap            = sattr,
		.server		= server,
		.bitmask = server->attr_bitmask,
        };
        struct nfs_setattrres  res = {
		.fattr		= fattr,
		.server		= server,
        };
        struct rpc_message msg = {
		.rpc_proc	= &nfs4_procedures[NFSPROC4_CLNT_SETATTR],
		.rpc_argp	= &arg,
		.rpc_resp	= &res,
		.rpc_cred	= cred,
        };
	unsigned long timestamp = jiffies;
	int status;

	nfs_fattr_init(fattr);

	if (nfs4_copy_delegation_stateid(&arg.stateid, inode)) {
		/* Use that stateid */
	} else if (state != NULL) {
		nfs4_copy_stateid(&arg.stateid, state, current->files);
	} else
		memcpy(&arg.stateid, &zero_stateid, sizeof(arg.stateid));

	status = nfs4_call_sync(server, &msg, &arg, &res, 1);
	if (status == 0 && state != NULL)
		renew_lease(server, timestamp);
	return status;
}

static int nfs4_do_setattr(struct inode *inode, struct rpc_cred *cred,
			   struct nfs_fattr *fattr, struct iattr *sattr,
			   struct nfs4_state *state)
{
	struct nfs_server *server = NFS_SERVER(inode);
	struct nfs4_exception exception = { };
	int err;
	do {
		err = nfs4_handle_exception(server,
				_nfs4_do_setattr(inode, cred, fattr, sattr, state),
				&exception);
	} while (exception.retry);
	return err;
}

struct nfs4_closedata {
	struct path path;
	struct inode *inode;
	struct nfs4_state *state;
	struct nfs_closeargs arg;
	struct nfs_closeres res;
	struct nfs_fattr fattr;
	unsigned long timestamp;
};

static void nfs4_free_closedata(void *data)
{
	struct nfs4_closedata *calldata = data;
	struct nfs4_state_owner *sp = calldata->state->owner;

	nfs4_put_open_state(calldata->state);
	nfs_free_seqid(calldata->arg.seqid);
	nfs4_put_state_owner(sp);
	path_put(&calldata->path);
	kfree(calldata);
}

static void nfs4_close_done(struct rpc_task *task, void *data)
{
	struct nfs4_closedata *calldata = data;
	struct nfs4_state *state = calldata->state;
	struct nfs_server *server = NFS_SERVER(calldata->inode);

	nfs4_sequence_done(server, &calldata->res.seq_res, task->tk_status);
	if (RPC_ASSASSINATED(task))
		return;
        /* hmm. we are done with the inode, and in the process of freeing
	 * the state_owner. we keep this around to process errors
	 */
	switch (task->tk_status) {
		case 0:
			nfs_set_open_stateid(state, &calldata->res.stateid, 0);
			renew_lease(server, calldata->timestamp);
			break;
		case -NFS4ERR_STALE_STATEID:
		case -NFS4ERR_OLD_STATEID:
		case -NFS4ERR_BAD_STATEID:
		case -NFS4ERR_EXPIRED:
			if (calldata->arg.fmode == 0)
				break;
		default:
			if (nfs4_async_handle_error(task, server, state) == -EAGAIN) {
				nfs4_restart_rpc(task, server->nfs_client);
				return;
			}
	}
	nfs4_sequence_free_slot(server->nfs_client, &calldata->res.seq_res);
	nfs_refresh_inode(calldata->inode, calldata->res.fattr);
}

static void nfs4_close_prepare(struct rpc_task *task, void *data)
{
	struct nfs4_closedata *calldata = data;
	struct nfs4_state *state = calldata->state;
	int clear_rd, clear_wr, clear_rdwr;

	if (nfs_wait_on_sequence(calldata->arg.seqid, task) != 0)
		return;

	clear_rd = clear_wr = clear_rdwr = 0;
	spin_lock(&state->owner->so_lock);
	/* Calculate the change in open mode */
	if (state->n_rdwr == 0) {
		if (state->n_rdonly == 0) {
			clear_rd |= test_and_clear_bit(NFS_O_RDONLY_STATE, &state->flags);
			clear_rdwr |= test_and_clear_bit(NFS_O_RDWR_STATE, &state->flags);
		}
		if (state->n_wronly == 0) {
			clear_wr |= test_and_clear_bit(NFS_O_WRONLY_STATE, &state->flags);
			clear_rdwr |= test_and_clear_bit(NFS_O_RDWR_STATE, &state->flags);
		}
	}
	spin_unlock(&state->owner->so_lock);
	if (!clear_rd && !clear_wr && !clear_rdwr) {
		/* Note: exit _without_ calling nfs4_close_done */
		task->tk_action = NULL;
		return;
	}
	nfs_fattr_init(calldata->res.fattr);
	if (test_bit(NFS_O_RDONLY_STATE, &state->flags) != 0) {
		task->tk_msg.rpc_proc = &nfs4_procedures[NFSPROC4_CLNT_OPEN_DOWNGRADE];
		calldata->arg.fmode = FMODE_READ;
	} else if (test_bit(NFS_O_WRONLY_STATE, &state->flags) != 0) {
		task->tk_msg.rpc_proc = &nfs4_procedures[NFSPROC4_CLNT_OPEN_DOWNGRADE];
		calldata->arg.fmode = FMODE_WRITE;
	}
	calldata->timestamp = jiffies;
	if (nfs4_setup_sequence((NFS_SERVER(calldata->inode))->nfs_client,
				&calldata->arg.seq_args, &calldata->res.seq_res,
				1, task))
		return;
	rpc_call_start(task);
}

static const struct rpc_call_ops nfs4_close_ops = {
	.rpc_call_prepare = nfs4_close_prepare,
	.rpc_call_done = nfs4_close_done,
	.rpc_release = nfs4_free_closedata,
};

/* 
 * It is possible for data to be read/written from a mem-mapped file 
 * after the sys_close call (which hits the vfs layer as a flush).
 * This means that we can't safely call nfsv4 close on a file until 
 * the inode is cleared. This in turn means that we are not good
 * NFSv4 citizens - we do not indicate to the server to update the file's 
 * share state even when we are done with one of the three share 
 * stateid's in the inode.
 *
 * NOTE: Caller must be holding the sp->so_owner semaphore!
 */
int nfs4_do_close(struct path *path, struct nfs4_state *state, int wait)
{
	struct nfs_server *server = NFS_SERVER(state->inode);
	struct nfs4_closedata *calldata;
	struct nfs4_state_owner *sp = state->owner;
	struct rpc_task *task;
	struct rpc_message msg = {
		.rpc_proc = &nfs4_procedures[NFSPROC4_CLNT_CLOSE],
		.rpc_cred = state->owner->so_cred,
	};
	struct rpc_task_setup task_setup_data = {
		.rpc_client = server->client,
		.rpc_message = &msg,
		.callback_ops = &nfs4_close_ops,
		.workqueue = nfsiod_workqueue,
		.flags = RPC_TASK_ASYNC,
	};
	int status = -ENOMEM;

	calldata = kzalloc(sizeof(*calldata), GFP_KERNEL);
	if (calldata == NULL)
		goto out;
	calldata->inode = state->inode;
	calldata->state = state;
	calldata->arg.fh = NFS_FH(state->inode);
	calldata->arg.stateid = &state->open_stateid;
	if (nfs4_has_session(server->nfs_client))
		memset(calldata->arg.stateid->data, 0, 4);    /* clear seqid */
	/* Serialization for the sequence id */
	calldata->arg.seqid = nfs_alloc_seqid(&state->owner->so_seqid);
	if (calldata->arg.seqid == NULL)
		goto out_free_calldata;
	calldata->arg.fmode = 0;
	calldata->arg.bitmask = server->cache_consistency_bitmask;
	calldata->res.fattr = &calldata->fattr;
	calldata->res.seqid = calldata->arg.seqid;
	calldata->res.server = server;
	calldata->res.seq_res.sr_slotid = NFS4_MAX_SLOT_TABLE;
	calldata->path.mnt = mntget(path->mnt);
	calldata->path.dentry = dget(path->dentry);

	msg.rpc_argp = &calldata->arg,
	msg.rpc_resp = &calldata->res,
	task_setup_data.callback_data = calldata;
	task = rpc_run_task(&task_setup_data);
	if (IS_ERR(task))
		return PTR_ERR(task);
	status = 0;
	if (wait)
		status = rpc_wait_for_completion_task(task);
	rpc_put_task(task);
	return status;
out_free_calldata:
	kfree(calldata);
out:
	nfs4_put_open_state(state);
	nfs4_put_state_owner(sp);
	return status;
}

static int nfs4_intent_set_file(struct nameidata *nd, struct path *path, struct nfs4_state *state, fmode_t fmode)
{
	struct file *filp;
	int ret;

	/* If the open_intent is for execute, we have an extra check to make */
	if (fmode & FMODE_EXEC) {
		ret = nfs_may_open(state->inode,
				state->owner->so_cred,
				nd->intent.open.flags);
		if (ret < 0)
			goto out_close;
	}
	filp = lookup_instantiate_filp(nd, path->dentry, NULL);
	if (!IS_ERR(filp)) {
		struct nfs_open_context *ctx;
		ctx = nfs_file_open_context(filp);
		ctx->state = state;
		return 0;
	}
	ret = PTR_ERR(filp);
out_close:
	nfs4_close_sync(path, state, fmode & (FMODE_READ|FMODE_WRITE));
	return ret;
}

struct dentry *
nfs4_atomic_open(struct inode *dir, struct dentry *dentry, struct nameidata *nd)
{
	struct path path = {
		.mnt = nd->path.mnt,
		.dentry = dentry,
	};
	struct dentry *parent;
	struct iattr attr;
	struct rpc_cred *cred;
	struct nfs4_state *state;
	struct dentry *res;
	fmode_t fmode = nd->intent.open.flags & (FMODE_READ | FMODE_WRITE | FMODE_EXEC);

	if (nd->flags & LOOKUP_CREATE) {
		attr.ia_mode = nd->intent.open.create_mode;
		attr.ia_valid = ATTR_MODE;
		if (!IS_POSIXACL(dir))
			attr.ia_mode &= ~current_umask();
	} else {
		attr.ia_valid = 0;
		BUG_ON(nd->intent.open.flags & O_CREAT);
	}

	cred = rpc_lookup_cred();
	if (IS_ERR(cred))
		return (struct dentry *)cred;
	parent = dentry->d_parent;
	/* Protect against concurrent sillydeletes */
	nfs_block_sillyrename(parent);
	state = nfs4_do_open(dir, &path, fmode, nd->intent.open.flags, &attr, cred);
	put_rpccred(cred);
	if (IS_ERR(state)) {
		if (PTR_ERR(state) == -ENOENT) {
			d_add(dentry, NULL);
			nfs_set_verifier(dentry, nfs_save_change_attribute(dir));
		}
		nfs_unblock_sillyrename(parent);
		return (struct dentry *)state;
	}
	res = d_add_unique(dentry, igrab(state->inode));
	if (res != NULL)
		path.dentry = res;
	nfs_set_verifier(path.dentry, nfs_save_change_attribute(dir));
	nfs_unblock_sillyrename(parent);
	nfs4_intent_set_file(nd, &path, state, fmode);
	return res;
}

int
nfs4_open_revalidate(struct inode *dir, struct dentry *dentry, int openflags, struct nameidata *nd)
{
	struct path path = {
		.mnt = nd->path.mnt,
		.dentry = dentry,
	};
	struct rpc_cred *cred;
	struct nfs4_state *state;
	fmode_t fmode = openflags & (FMODE_READ | FMODE_WRITE);

	cred = rpc_lookup_cred();
	if (IS_ERR(cred))
		return PTR_ERR(cred);
	state = nfs4_do_open(dir, &path, fmode, openflags, NULL, cred);
	put_rpccred(cred);
	if (IS_ERR(state)) {
		switch (PTR_ERR(state)) {
			case -EPERM:
			case -EACCES:
			case -EDQUOT:
			case -ENOSPC:
			case -EROFS:
				lookup_instantiate_filp(nd, (struct dentry *)state, NULL);
				return 1;
			default:
				goto out_drop;
		}
	}
	if (state->inode == dentry->d_inode) {
		nfs_set_verifier(dentry, nfs_save_change_attribute(dir));
		nfs4_intent_set_file(nd, &path, state, fmode);
		return 1;
	}
	nfs4_close_sync(&path, state, fmode);
out_drop:
	d_drop(dentry);
	return 0;
}

void nfs4_close_context(struct nfs_open_context *ctx, int is_sync)
{
	if (ctx->state == NULL)
		return;
	if (is_sync)
		nfs4_close_sync(&ctx->path, ctx->state, ctx->mode);
	else
		nfs4_close_state(&ctx->path, ctx->state, ctx->mode);
}

static int _nfs4_server_capabilities(struct nfs_server *server, struct nfs_fh *fhandle)
{
	struct nfs4_server_caps_arg args = {
		.fhandle = fhandle,
	};
	struct nfs4_server_caps_res res = {};
	struct rpc_message msg = {
		.rpc_proc = &nfs4_procedures[NFSPROC4_CLNT_SERVER_CAPS],
		.rpc_argp = &args,
		.rpc_resp = &res,
	};
	int status;

	status = nfs4_call_sync(server, &msg, &args, &res, 0);
	if (status == 0) {
		memcpy(server->attr_bitmask, res.attr_bitmask, sizeof(server->attr_bitmask));
		server->caps &= ~(NFS_CAP_ACLS|NFS_CAP_HARDLINKS|
				NFS_CAP_SYMLINKS|NFS_CAP_FILEID|
				NFS_CAP_MODE|NFS_CAP_NLINK|NFS_CAP_OWNER|
				NFS_CAP_OWNER_GROUP|NFS_CAP_ATIME|
				NFS_CAP_CTIME|NFS_CAP_MTIME);
		if (res.attr_bitmask[0] & FATTR4_WORD0_ACL)
			server->caps |= NFS_CAP_ACLS;
		if (res.has_links != 0)
			server->caps |= NFS_CAP_HARDLINKS;
		if (res.has_symlinks != 0)
			server->caps |= NFS_CAP_SYMLINKS;
		if (res.attr_bitmask[0] & FATTR4_WORD0_FILEID)
			server->caps |= NFS_CAP_FILEID;
		if (res.attr_bitmask[1] & FATTR4_WORD1_MODE)
			server->caps |= NFS_CAP_MODE;
		if (res.attr_bitmask[1] & FATTR4_WORD1_NUMLINKS)
			server->caps |= NFS_CAP_NLINK;
		if (res.attr_bitmask[1] & FATTR4_WORD1_OWNER)
			server->caps |= NFS_CAP_OWNER;
		if (res.attr_bitmask[1] & FATTR4_WORD1_OWNER_GROUP)
			server->caps |= NFS_CAP_OWNER_GROUP;
		if (res.attr_bitmask[1] & FATTR4_WORD1_TIME_ACCESS)
			server->caps |= NFS_CAP_ATIME;
		if (res.attr_bitmask[1] & FATTR4_WORD1_TIME_METADATA)
			server->caps |= NFS_CAP_CTIME;
		if (res.attr_bitmask[1] & FATTR4_WORD1_TIME_MODIFY)
			server->caps |= NFS_CAP_MTIME;

		memcpy(server->cache_consistency_bitmask, res.attr_bitmask, sizeof(server->cache_consistency_bitmask));
		server->cache_consistency_bitmask[0] &= FATTR4_WORD0_CHANGE|FATTR4_WORD0_SIZE;
		server->cache_consistency_bitmask[1] &= FATTR4_WORD1_TIME_METADATA|FATTR4_WORD1_TIME_MODIFY;
		server->acl_bitmask = res.acl_bitmask;
	}

	return status;
}

int nfs4_server_capabilities(struct nfs_server *server, struct nfs_fh *fhandle)
{
	struct nfs4_exception exception = { };
	int err;
	do {
		err = nfs4_handle_exception(server,
				_nfs4_server_capabilities(server, fhandle),
				&exception);
	} while (exception.retry);
	return err;
}

static int _nfs4_lookup_root(struct nfs_server *server, struct nfs_fh *fhandle,
		struct nfs_fsinfo *info)
{
	struct nfs4_lookup_root_arg args = {
		.bitmask = nfs4_fattr_bitmap,
	};
	struct nfs4_lookup_res res = {
		.server = server,
		.fattr = info->fattr,
		.fh = fhandle,
	};
	struct rpc_message msg = {
		.rpc_proc = &nfs4_procedures[NFSPROC4_CLNT_LOOKUP_ROOT],
		.rpc_argp = &args,
		.rpc_resp = &res,
	};

	nfs_fattr_init(info->fattr);
	return nfs4_call_sync(server, &msg, &args, &res, 0);
}

static int nfs4_lookup_root(struct nfs_server *server, struct nfs_fh *fhandle,
		struct nfs_fsinfo *info)
{
	struct nfs4_exception exception = { };
	int err;
	do {
		err = nfs4_handle_exception(server,
				_nfs4_lookup_root(server, fhandle, info),
				&exception);
	} while (exception.retry);
	return err;
}

/*
 * get the file handle for the "/" directory on the server
 */
static int nfs4_proc_get_root(struct nfs_server *server, struct nfs_fh *fhandle,
			      struct nfs_fsinfo *info)
{
	int status;

	status = nfs4_lookup_root(server, fhandle, info);
	if (status == 0)
		status = nfs4_server_capabilities(server, fhandle);
	if (status == 0)
		status = nfs4_do_fsinfo(server, fhandle, info);
	return nfs4_map_errors(status);
}

/*
 * Get locations and (maybe) other attributes of a referral.
 * Note that we'll actually follow the referral later when
 * we detect fsid mismatch in inode revalidation
 */
static int nfs4_get_referral(struct inode *dir, const struct qstr *name, struct nfs_fattr *fattr, struct nfs_fh *fhandle)
{
	int status = -ENOMEM;
	struct page *page = NULL;
	struct nfs4_fs_locations *locations = NULL;

	page = alloc_page(GFP_KERNEL);
	if (page == NULL)
		goto out;
	locations = kmalloc(sizeof(struct nfs4_fs_locations), GFP_KERNEL);
	if (locations == NULL)
		goto out;

	status = nfs4_proc_fs_locations(dir, name, locations, page);
	if (status != 0)
		goto out;
	/* Make sure server returned a different fsid for the referral */
	if (nfs_fsid_equal(&NFS_SERVER(dir)->fsid, &locations->fattr.fsid)) {
		dprintk("%s: server did not return a different fsid for a referral at %s\n", __func__, name->name);
		status = -EIO;
		goto out;
	}

	memcpy(fattr, &locations->fattr, sizeof(struct nfs_fattr));
	fattr->valid |= NFS_ATTR_FATTR_V4_REFERRAL;
	if (!fattr->mode)
		fattr->mode = S_IFDIR;
	memset(fhandle, 0, sizeof(struct nfs_fh));
out:
	if (page)
		__free_page(page);
	if (locations)
		kfree(locations);
	return status;
}

static int _nfs4_proc_getattr(struct nfs_server *server, struct nfs_fh *fhandle, struct nfs_fattr *fattr)
{
	struct nfs4_getattr_arg args = {
		.fh = fhandle,
		.bitmask = server->attr_bitmask,
	};
	struct nfs4_getattr_res res = {
		.fattr = fattr,
		.server = server,
	};
	struct rpc_message msg = {
		.rpc_proc = &nfs4_procedures[NFSPROC4_CLNT_GETATTR],
		.rpc_argp = &args,
		.rpc_resp = &res,
	};
	
	nfs_fattr_init(fattr);
	return nfs4_call_sync(server, &msg, &args, &res, 0);
}

static int nfs4_proc_getattr(struct nfs_server *server, struct nfs_fh *fhandle, struct nfs_fattr *fattr)
{
	struct nfs4_exception exception = { };
	int err;
	do {
		err = nfs4_handle_exception(server,
				_nfs4_proc_getattr(server, fhandle, fattr),
				&exception);
	} while (exception.retry);
	return err;
}

/* 
 * The file is not closed if it is opened due to the a request to change
 * the size of the file. The open call will not be needed once the
 * VFS layer lookup-intents are implemented.
 *
 * Close is called when the inode is destroyed.
 * If we haven't opened the file for O_WRONLY, we
 * need to in the size_change case to obtain a stateid.
 *
 * Got race?
 * Because OPEN is always done by name in nfsv4, it is
 * possible that we opened a different file by the same
 * name.  We can recognize this race condition, but we
 * can't do anything about it besides returning an error.
 *
 * This will be fixed with VFS changes (lookup-intent).
 */
static int
nfs4_proc_setattr(struct dentry *dentry, struct nfs_fattr *fattr,
		  struct iattr *sattr)
{
	struct inode *inode = dentry->d_inode;
	struct rpc_cred *cred = NULL;
	struct nfs4_state *state = NULL;
	int status;

	nfs_fattr_init(fattr);
	
	/* Search for an existing open(O_WRITE) file */
	if (sattr->ia_valid & ATTR_FILE) {
		struct nfs_open_context *ctx;

		ctx = nfs_file_open_context(sattr->ia_file);
		if (ctx) {
			cred = ctx->cred;
			state = ctx->state;
		}
	}

	status = nfs4_do_setattr(inode, cred, fattr, sattr, state);
	if (status == 0)
		nfs_setattr_update_inode(inode, sattr);
	return status;
}

static int _nfs4_proc_lookupfh(struct nfs_server *server, const struct nfs_fh *dirfh,
		const struct qstr *name, struct nfs_fh *fhandle,
		struct nfs_fattr *fattr)
{
	int		       status;
	struct nfs4_lookup_arg args = {
		.bitmask = server->attr_bitmask,
		.dir_fh = dirfh,
		.name = name,
	};
	struct nfs4_lookup_res res = {
		.server = server,
		.fattr = fattr,
		.fh = fhandle,
	};
	struct rpc_message msg = {
		.rpc_proc = &nfs4_procedures[NFSPROC4_CLNT_LOOKUP],
		.rpc_argp = &args,
		.rpc_resp = &res,
	};

	nfs_fattr_init(fattr);

	dprintk("NFS call  lookupfh %s\n", name->name);
	status = nfs4_call_sync(server, &msg, &args, &res, 0);
	dprintk("NFS reply lookupfh: %d\n", status);
	return status;
}

static int nfs4_proc_lookupfh(struct nfs_server *server, struct nfs_fh *dirfh,
			      struct qstr *name, struct nfs_fh *fhandle,
			      struct nfs_fattr *fattr)
{
	struct nfs4_exception exception = { };
	int err;
	do {
		err = _nfs4_proc_lookupfh(server, dirfh, name, fhandle, fattr);
		/* FIXME: !!!! */
		if (err == -NFS4ERR_MOVED) {
			err = -EREMOTE;
			break;
		}
		err = nfs4_handle_exception(server, err, &exception);
	} while (exception.retry);
	return err;
}

static int _nfs4_proc_lookup(struct inode *dir, const struct qstr *name,
		struct nfs_fh *fhandle, struct nfs_fattr *fattr)
{
	int status;
	
	dprintk("NFS call  lookup %s\n", name->name);
	status = _nfs4_proc_lookupfh(NFS_SERVER(dir), NFS_FH(dir), name, fhandle, fattr);
	if (status == -NFS4ERR_MOVED)
		status = nfs4_get_referral(dir, name, fattr, fhandle);
	dprintk("NFS reply lookup: %d\n", status);
	return status;
}

static int nfs4_proc_lookup(struct inode *dir, struct qstr *name, struct nfs_fh *fhandle, struct nfs_fattr *fattr)
{
	struct nfs4_exception exception = { };
	int err;
	do {
		err = nfs4_handle_exception(NFS_SERVER(dir),
				_nfs4_proc_lookup(dir, name, fhandle, fattr),
				&exception);
	} while (exception.retry);
	return err;
}

static int _nfs4_proc_access(struct inode *inode, struct nfs_access_entry *entry)
{
	struct nfs_server *server = NFS_SERVER(inode);
	struct nfs_fattr fattr;
	struct nfs4_accessargs args = {
		.fh = NFS_FH(inode),
		.bitmask = server->attr_bitmask,
	};
	struct nfs4_accessres res = {
		.server = server,
		.fattr = &fattr,
	};
	struct rpc_message msg = {
		.rpc_proc = &nfs4_procedures[NFSPROC4_CLNT_ACCESS],
		.rpc_argp = &args,
		.rpc_resp = &res,
		.rpc_cred = entry->cred,
	};
	int mode = entry->mask;
	int status;

	/*
	 * Determine which access bits we want to ask for...
	 */
	if (mode & MAY_READ)
		args.access |= NFS4_ACCESS_READ;
	if (S_ISDIR(inode->i_mode)) {
		if (mode & MAY_WRITE)
			args.access |= NFS4_ACCESS_MODIFY | NFS4_ACCESS_EXTEND | NFS4_ACCESS_DELETE;
		if (mode & MAY_EXEC)
			args.access |= NFS4_ACCESS_LOOKUP;
	} else {
		if (mode & MAY_WRITE)
			args.access |= NFS4_ACCESS_MODIFY | NFS4_ACCESS_EXTEND;
		if (mode & MAY_EXEC)
			args.access |= NFS4_ACCESS_EXECUTE;
	}
	nfs_fattr_init(&fattr);
	status = nfs4_call_sync(server, &msg, &args, &res, 0);
	if (!status) {
		entry->mask = 0;
		if (res.access & NFS4_ACCESS_READ)
			entry->mask |= MAY_READ;
		if (res.access & (NFS4_ACCESS_MODIFY | NFS4_ACCESS_EXTEND | NFS4_ACCESS_DELETE))
			entry->mask |= MAY_WRITE;
		if (res.access & (NFS4_ACCESS_LOOKUP|NFS4_ACCESS_EXECUTE))
			entry->mask |= MAY_EXEC;
		nfs_refresh_inode(inode, &fattr);
	}
	return status;
}

static int nfs4_proc_access(struct inode *inode, struct nfs_access_entry *entry)
{
	struct nfs4_exception exception = { };
	int err;
	do {
		err = nfs4_handle_exception(NFS_SERVER(inode),
				_nfs4_proc_access(inode, entry),
				&exception);
	} while (exception.retry);
	return err;
}

/*
 * TODO: For the time being, we don't try to get any attributes
 * along with any of the zero-copy operations READ, READDIR,
 * READLINK, WRITE.
 *
 * In the case of the first three, we want to put the GETATTR
 * after the read-type operation -- this is because it is hard
 * to predict the length of a GETATTR response in v4, and thus
 * align the READ data correctly.  This means that the GETATTR
 * may end up partially falling into the page cache, and we should
 * shift it into the 'tail' of the xdr_buf before processing.
 * To do this efficiently, we need to know the total length
 * of data received, which doesn't seem to be available outside
 * of the RPC layer.
 *
 * In the case of WRITE, we also want to put the GETATTR after
 * the operation -- in this case because we want to make sure
 * we get the post-operation mtime and size.  This means that
 * we can't use xdr_encode_pages() as written: we need a variant
 * of it which would leave room in the 'tail' iovec.
 *
 * Both of these changes to the XDR layer would in fact be quite
 * minor, but I decided to leave them for a subsequent patch.
 */
static int _nfs4_proc_readlink(struct inode *inode, struct page *page,
		unsigned int pgbase, unsigned int pglen)
{
	struct nfs4_readlink args = {
		.fh       = NFS_FH(inode),
		.pgbase	  = pgbase,
		.pglen    = pglen,
		.pages    = &page,
	};
	struct nfs4_readlink_res res;
	struct rpc_message msg = {
		.rpc_proc = &nfs4_procedures[NFSPROC4_CLNT_READLINK],
		.rpc_argp = &args,
		.rpc_resp = &res,
	};

	return nfs4_call_sync(NFS_SERVER(inode), &msg, &args, &res, 0);
}

static int nfs4_proc_readlink(struct inode *inode, struct page *page,
		unsigned int pgbase, unsigned int pglen)
{
	struct nfs4_exception exception = { };
	int err;
	do {
		err = nfs4_handle_exception(NFS_SERVER(inode),
				_nfs4_proc_readlink(inode, page, pgbase, pglen),
				&exception);
	} while (exception.retry);
	return err;
}

/*
 * Got race?
 * We will need to arrange for the VFS layer to provide an atomic open.
 * Until then, this create/open method is prone to inefficiency and race
 * conditions due to the lookup, create, and open VFS calls from sys_open()
 * placed on the wire.
 *
 * Given the above sorry state of affairs, I'm simply sending an OPEN.
 * The file will be opened again in the subsequent VFS open call
 * (nfs4_proc_file_open).
 *
 * The open for read will just hang around to be used by any process that
 * opens the file O_RDONLY. This will all be resolved with the VFS changes.
 */

static int
nfs4_proc_create(struct inode *dir, struct dentry *dentry, struct iattr *sattr,
                 int flags, struct nameidata *nd)
{
	struct path path = {
		.mnt = nd->path.mnt,
		.dentry = dentry,
	};
	struct nfs4_state *state;
	struct rpc_cred *cred;
	fmode_t fmode = flags & (FMODE_READ | FMODE_WRITE);
	int status = 0;

	cred = rpc_lookup_cred();
	if (IS_ERR(cred)) {
		status = PTR_ERR(cred);
		goto out;
	}
	state = nfs4_do_open(dir, &path, fmode, flags, sattr, cred);
	d_drop(dentry);
	if (IS_ERR(state)) {
		status = PTR_ERR(state);
		goto out_putcred;
	}
	d_add(dentry, igrab(state->inode));
	nfs_set_verifier(dentry, nfs_save_change_attribute(dir));
	if (flags & O_EXCL) {
		struct nfs_fattr fattr;
		status = nfs4_do_setattr(state->inode, cred, &fattr, sattr, state);
		if (status == 0)
			nfs_setattr_update_inode(state->inode, sattr);
		nfs_post_op_update_inode(state->inode, &fattr);
	}
	if (status == 0 && (nd->flags & LOOKUP_OPEN) != 0)
		status = nfs4_intent_set_file(nd, &path, state, fmode);
	else
		nfs4_close_sync(&path, state, fmode);
out_putcred:
	put_rpccred(cred);
out:
	return status;
}

static int _nfs4_proc_remove(struct inode *dir, struct qstr *name)
{
	struct nfs_server *server = NFS_SERVER(dir);
	struct nfs_removeargs args = {
		.fh = NFS_FH(dir),
		.name.len = name->len,
		.name.name = name->name,
		.bitmask = server->attr_bitmask,
	};
	struct nfs_removeres res = {
		.server = server,
	};
	struct rpc_message msg = {
		.rpc_proc = &nfs4_procedures[NFSPROC4_CLNT_REMOVE],
		.rpc_argp = &args,
		.rpc_resp = &res,
	};
	int			status;

	nfs_fattr_init(&res.dir_attr);
	status = nfs4_call_sync(server, &msg, &args, &res, 1);
	if (status == 0) {
		update_changeattr(dir, &res.cinfo);
		nfs_post_op_update_inode(dir, &res.dir_attr);
	}
	return status;
}

static int nfs4_proc_remove(struct inode *dir, struct qstr *name)
{
	struct nfs4_exception exception = { };
	int err;
	do {
		err = nfs4_handle_exception(NFS_SERVER(dir),
				_nfs4_proc_remove(dir, name),
				&exception);
	} while (exception.retry);
	return err;
}

static void nfs4_proc_unlink_setup(struct rpc_message *msg, struct inode *dir)
{
	struct nfs_server *server = NFS_SERVER(dir);
	struct nfs_removeargs *args = msg->rpc_argp;
	struct nfs_removeres *res = msg->rpc_resp;

	args->bitmask = server->cache_consistency_bitmask;
	res->server = server;
	msg->rpc_proc = &nfs4_procedures[NFSPROC4_CLNT_REMOVE];
}

static int nfs4_proc_unlink_done(struct rpc_task *task, struct inode *dir)
{
	struct nfs_removeres *res = task->tk_msg.rpc_resp;

	nfs4_sequence_done(res->server, &res->seq_res, task->tk_status);
	if (nfs4_async_handle_error(task, res->server, NULL) == -EAGAIN)
		return 0;
	nfs4_sequence_free_slot(res->server->nfs_client, &res->seq_res);
	update_changeattr(dir, &res->cinfo);
	nfs_post_op_update_inode(dir, &res->dir_attr);
	return 1;
}

static int _nfs4_proc_rename(struct inode *old_dir, struct qstr *old_name,
		struct inode *new_dir, struct qstr *new_name)
{
	struct nfs_server *server = NFS_SERVER(old_dir);
	struct nfs4_rename_arg arg = {
		.old_dir = NFS_FH(old_dir),
		.new_dir = NFS_FH(new_dir),
		.old_name = old_name,
		.new_name = new_name,
		.bitmask = server->attr_bitmask,
	};
	struct nfs_fattr old_fattr, new_fattr;
	struct nfs4_rename_res res = {
		.server = server,
		.old_fattr = &old_fattr,
		.new_fattr = &new_fattr,
	};
	struct rpc_message msg = {
		.rpc_proc = &nfs4_procedures[NFSPROC4_CLNT_RENAME],
		.rpc_argp = &arg,
		.rpc_resp = &res,
	};
	int			status;
	
	nfs_fattr_init(res.old_fattr);
	nfs_fattr_init(res.new_fattr);
	status = nfs4_call_sync(server, &msg, &arg, &res, 1);

	if (!status) {
		update_changeattr(old_dir, &res.old_cinfo);
		nfs_post_op_update_inode(old_dir, res.old_fattr);
		update_changeattr(new_dir, &res.new_cinfo);
		nfs_post_op_update_inode(new_dir, res.new_fattr);
	}
	return status;
}

static int nfs4_proc_rename(struct inode *old_dir, struct qstr *old_name,
		struct inode *new_dir, struct qstr *new_name)
{
	struct nfs4_exception exception = { };
	int err;
	do {
		err = nfs4_handle_exception(NFS_SERVER(old_dir),
				_nfs4_proc_rename(old_dir, old_name,
					new_dir, new_name),
				&exception);
	} while (exception.retry);
	return err;
}

static int _nfs4_proc_link(struct inode *inode, struct inode *dir, struct qstr *name)
{
	struct nfs_server *server = NFS_SERVER(inode);
	struct nfs4_link_arg arg = {
		.fh     = NFS_FH(inode),
		.dir_fh = NFS_FH(dir),
		.name   = name,
		.bitmask = server->attr_bitmask,
	};
	struct nfs_fattr fattr, dir_attr;
	struct nfs4_link_res res = {
		.server = server,
		.fattr = &fattr,
		.dir_attr = &dir_attr,
	};
	struct rpc_message msg = {
		.rpc_proc = &nfs4_procedures[NFSPROC4_CLNT_LINK],
		.rpc_argp = &arg,
		.rpc_resp = &res,
	};
	int			status;

	nfs_fattr_init(res.fattr);
	nfs_fattr_init(res.dir_attr);
	status = nfs4_call_sync(server, &msg, &arg, &res, 1);
	if (!status) {
		update_changeattr(dir, &res.cinfo);
		nfs_post_op_update_inode(dir, res.dir_attr);
		nfs_post_op_update_inode(inode, res.fattr);
	}

	return status;
}

static int nfs4_proc_link(struct inode *inode, struct inode *dir, struct qstr *name)
{
	struct nfs4_exception exception = { };
	int err;
	do {
		err = nfs4_handle_exception(NFS_SERVER(inode),
				_nfs4_proc_link(inode, dir, name),
				&exception);
	} while (exception.retry);
	return err;
}

struct nfs4_createdata {
	struct rpc_message msg;
	struct nfs4_create_arg arg;
	struct nfs4_create_res res;
	struct nfs_fh fh;
	struct nfs_fattr fattr;
	struct nfs_fattr dir_fattr;
};

static struct nfs4_createdata *nfs4_alloc_createdata(struct inode *dir,
		struct qstr *name, struct iattr *sattr, u32 ftype)
{
	struct nfs4_createdata *data;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (data != NULL) {
		struct nfs_server *server = NFS_SERVER(dir);

		data->msg.rpc_proc = &nfs4_procedures[NFSPROC4_CLNT_CREATE];
		data->msg.rpc_argp = &data->arg;
		data->msg.rpc_resp = &data->res;
		data->arg.dir_fh = NFS_FH(dir);
		data->arg.server = server;
		data->arg.name = name;
		data->arg.attrs = sattr;
		data->arg.ftype = ftype;
		data->arg.bitmask = server->attr_bitmask;
		data->res.server = server;
		data->res.fh = &data->fh;
		data->res.fattr = &data->fattr;
		data->res.dir_fattr = &data->dir_fattr;
		nfs_fattr_init(data->res.fattr);
		nfs_fattr_init(data->res.dir_fattr);
	}
	return data;
}

static int nfs4_do_create(struct inode *dir, struct dentry *dentry, struct nfs4_createdata *data)
{
	int status = nfs4_call_sync(NFS_SERVER(dir), &data->msg,
				    &data->arg, &data->res, 1);
	if (status == 0) {
		update_changeattr(dir, &data->res.dir_cinfo);
		nfs_post_op_update_inode(dir, data->res.dir_fattr);
		status = nfs_instantiate(dentry, data->res.fh, data->res.fattr);
	}
	return status;
}

static void nfs4_free_createdata(struct nfs4_createdata *data)
{
	kfree(data);
}

static int _nfs4_proc_symlink(struct inode *dir, struct dentry *dentry,
		struct page *page, unsigned int len, struct iattr *sattr)
{
	struct nfs4_createdata *data;
	int status = -ENAMETOOLONG;

	if (len > NFS4_MAXPATHLEN)
		goto out;

	status = -ENOMEM;
	data = nfs4_alloc_createdata(dir, &dentry->d_name, sattr, NF4LNK);
	if (data == NULL)
		goto out;

	data->msg.rpc_proc = &nfs4_procedures[NFSPROC4_CLNT_SYMLINK];
	data->arg.u.symlink.pages = &page;
	data->arg.u.symlink.len = len;
	
	status = nfs4_do_create(dir, dentry, data);

	nfs4_free_createdata(data);
out:
	return status;
}

static int nfs4_proc_symlink(struct inode *dir, struct dentry *dentry,
		struct page *page, unsigned int len, struct iattr *sattr)
{
	struct nfs4_exception exception = { };
	int err;
	do {
		err = nfs4_handle_exception(NFS_SERVER(dir),
				_nfs4_proc_symlink(dir, dentry, page,
							len, sattr),
				&exception);
	} while (exception.retry);
	return err;
}

static int _nfs4_proc_mkdir(struct inode *dir, struct dentry *dentry,
		struct iattr *sattr)
{
	struct nfs4_createdata *data;
	int status = -ENOMEM;

	data = nfs4_alloc_createdata(dir, &dentry->d_name, sattr, NF4DIR);
	if (data == NULL)
		goto out;

	status = nfs4_do_create(dir, dentry, data);

	nfs4_free_createdata(data);
out:
	return status;
}

static int nfs4_proc_mkdir(struct inode *dir, struct dentry *dentry,
		struct iattr *sattr)
{
	struct nfs4_exception exception = { };
	int err;
	do {
		err = nfs4_handle_exception(NFS_SERVER(dir),
				_nfs4_proc_mkdir(dir, dentry, sattr),
				&exception);
	} while (exception.retry);
	return err;
}

static int _nfs4_proc_readdir(struct dentry *dentry, struct rpc_cred *cred,
                  u64 cookie, struct page *page, unsigned int count, int plus)
{
	struct inode		*dir = dentry->d_inode;
	struct nfs4_readdir_arg args = {
		.fh = NFS_FH(dir),
		.pages = &page,
		.pgbase = 0,
		.count = count,
		.bitmask = NFS_SERVER(dentry->d_inode)->attr_bitmask,
	};
	struct nfs4_readdir_res res;
	struct rpc_message msg = {
		.rpc_proc = &nfs4_procedures[NFSPROC4_CLNT_READDIR],
		.rpc_argp = &args,
		.rpc_resp = &res,
		.rpc_cred = cred,
	};
	int			status;

	dprintk("%s: dentry = %s/%s, cookie = %Lu\n", __func__,
			dentry->d_parent->d_name.name,
			dentry->d_name.name,
			(unsigned long long)cookie);
	nfs4_setup_readdir(cookie, NFS_COOKIEVERF(dir), dentry, &args);
	res.pgbase = args.pgbase;
	status = nfs4_call_sync(NFS_SERVER(dir), &msg, &args, &res, 0);
	if (status == 0)
		memcpy(NFS_COOKIEVERF(dir), res.verifier.data, NFS4_VERIFIER_SIZE);

	nfs_invalidate_atime(dir);

	dprintk("%s: returns %d\n", __func__, status);
	return status;
}

static int nfs4_proc_readdir(struct dentry *dentry, struct rpc_cred *cred,
                  u64 cookie, struct page *page, unsigned int count, int plus)
{
	struct nfs4_exception exception = { };
	int err;
	do {
		err = nfs4_handle_exception(NFS_SERVER(dentry->d_inode),
				_nfs4_proc_readdir(dentry, cred, cookie,
					page, count, plus),
				&exception);
	} while (exception.retry);
	return err;
}

static int _nfs4_proc_mknod(struct inode *dir, struct dentry *dentry,
		struct iattr *sattr, dev_t rdev)
{
	struct nfs4_createdata *data;
	int mode = sattr->ia_mode;
	int status = -ENOMEM;

	BUG_ON(!(sattr->ia_valid & ATTR_MODE));
	BUG_ON(!S_ISFIFO(mode) && !S_ISBLK(mode) && !S_ISCHR(mode) && !S_ISSOCK(mode));

	data = nfs4_alloc_createdata(dir, &dentry->d_name, sattr, NF4SOCK);
	if (data == NULL)
		goto out;

	if (S_ISFIFO(mode))
		data->arg.ftype = NF4FIFO;
	else if (S_ISBLK(mode)) {
		data->arg.ftype = NF4BLK;
		data->arg.u.device.specdata1 = MAJOR(rdev);
		data->arg.u.device.specdata2 = MINOR(rdev);
	}
	else if (S_ISCHR(mode)) {
		data->arg.ftype = NF4CHR;
		data->arg.u.device.specdata1 = MAJOR(rdev);
		data->arg.u.device.specdata2 = MINOR(rdev);
	}
	
	status = nfs4_do_create(dir, dentry, data);

	nfs4_free_createdata(data);
out:
	return status;
}

static int nfs4_proc_mknod(struct inode *dir, struct dentry *dentry,
		struct iattr *sattr, dev_t rdev)
{
	struct nfs4_exception exception = { };
	int err;
	do {
		err = nfs4_handle_exception(NFS_SERVER(dir),
				_nfs4_proc_mknod(dir, dentry, sattr, rdev),
				&exception);
	} while (exception.retry);
	return err;
}

static int _nfs4_proc_statfs(struct nfs_server *server, struct nfs_fh *fhandle,
		 struct nfs_fsstat *fsstat)
{
	struct nfs4_statfs_arg args = {
		.fh = fhandle,
		.bitmask = server->attr_bitmask,
	};
	struct nfs4_statfs_res res = {
		.fsstat = fsstat,
	};
	struct rpc_message msg = {
		.rpc_proc = &nfs4_procedures[NFSPROC4_CLNT_STATFS],
		.rpc_argp = &args,
		.rpc_resp = &res,
	};

	nfs_fattr_init(fsstat->fattr);
	return  nfs4_call_sync(server, &msg, &args, &res, 0);
}

static int nfs4_proc_statfs(struct nfs_server *server, struct nfs_fh *fhandle, struct nfs_fsstat *fsstat)
{
	struct nfs4_exception exception = { };
	int err;
	do {
		err = nfs4_handle_exception(server,
				_nfs4_proc_statfs(server, fhandle, fsstat),
				&exception);
	} while (exception.retry);
	return err;
}

static int _nfs4_do_fsinfo(struct nfs_server *server, struct nfs_fh *fhandle,
		struct nfs_fsinfo *fsinfo)
{
	struct nfs4_fsinfo_arg args = {
		.fh = fhandle,
		.bitmask = server->attr_bitmask,
	};
	struct nfs4_fsinfo_res res = {
		.fsinfo = fsinfo,
	};
	struct rpc_message msg = {
		.rpc_proc = &nfs4_procedures[NFSPROC4_CLNT_FSINFO],
		.rpc_argp = &args,
		.rpc_resp = &res,
	};

	return nfs4_call_sync(server, &msg, &args, &res, 0);
}

static int nfs4_do_fsinfo(struct nfs_server *server, struct nfs_fh *fhandle, struct nfs_fsinfo *fsinfo)
{
	struct nfs4_exception exception = { };
	int err;

	do {
		err = nfs4_handle_exception(server,
				_nfs4_do_fsinfo(server, fhandle, fsinfo),
				&exception);
	} while (exception.retry);
	return err;
}

static int nfs4_proc_fsinfo(struct nfs_server *server, struct nfs_fh *fhandle, struct nfs_fsinfo *fsinfo)
{
	nfs_fattr_init(fsinfo->fattr);
	return nfs4_do_fsinfo(server, fhandle, fsinfo);
}

static int _nfs4_proc_pathconf(struct nfs_server *server, struct nfs_fh *fhandle,
		struct nfs_pathconf *pathconf)
{
	struct nfs4_pathconf_arg args = {
		.fh = fhandle,
		.bitmask = server->attr_bitmask,
	};
	struct nfs4_pathconf_res res = {
		.pathconf = pathconf,
	};
	struct rpc_message msg = {
		.rpc_proc = &nfs4_procedures[NFSPROC4_CLNT_PATHCONF],
		.rpc_argp = &args,
		.rpc_resp = &res,
	};

	/* None of the pathconf attributes are mandatory to implement */
	if ((args.bitmask[0] & nfs4_pathconf_bitmap[0]) == 0) {
		memset(pathconf, 0, sizeof(*pathconf));
		return 0;
	}

	nfs_fattr_init(pathconf->fattr);
	return nfs4_call_sync(server, &msg, &args, &res, 0);
}

static int nfs4_proc_pathconf(struct nfs_server *server, struct nfs_fh *fhandle,
		struct nfs_pathconf *pathconf)
{
	struct nfs4_exception exception = { };
	int err;

	do {
		err = nfs4_handle_exception(server,
				_nfs4_proc_pathconf(server, fhandle, pathconf),
				&exception);
	} while (exception.retry);
	return err;
}

static int nfs4_read_done(struct rpc_task *task, struct nfs_read_data *data)
{
	struct nfs_server *server = NFS_SERVER(data->inode);

	dprintk("--> %s\n", __func__);

	/* nfs4_sequence_free_slot called in the read rpc_call_done */
	nfs4_sequence_done(server, &data->res.seq_res, task->tk_status);

	if (nfs4_async_handle_error(task, server, data->args.context->state) == -EAGAIN) {
		nfs4_restart_rpc(task, server->nfs_client);
		return -EAGAIN;
	}

	nfs_invalidate_atime(data->inode);
	if (task->tk_status > 0)
		renew_lease(server, data->timestamp);
	return 0;
}

static void nfs4_proc_read_setup(struct nfs_read_data *data, struct rpc_message *msg)
{
	data->timestamp   = jiffies;
	msg->rpc_proc = &nfs4_procedures[NFSPROC4_CLNT_READ];
}

static int nfs4_write_done(struct rpc_task *task, struct nfs_write_data *data)
{
	struct inode *inode = data->inode;
	
	/* slot is freed in nfs_writeback_done */
	nfs4_sequence_done(NFS_SERVER(inode), &data->res.seq_res,
			   task->tk_status);

	if (nfs4_async_handle_error(task, NFS_SERVER(inode), data->args.context->state) == -EAGAIN) {
		nfs4_restart_rpc(task, NFS_SERVER(inode)->nfs_client);
		return -EAGAIN;
	}
	if (task->tk_status >= 0) {
		renew_lease(NFS_SERVER(inode), data->timestamp);
		nfs_post_op_update_inode_force_wcc(inode, data->res.fattr);
	}
	return 0;
}

static void nfs4_proc_write_setup(struct nfs_write_data *data, struct rpc_message *msg)
{
	struct nfs_server *server = NFS_SERVER(data->inode);

	data->args.bitmask = server->cache_consistency_bitmask;
	data->res.server = server;
	data->timestamp   = jiffies;

	msg->rpc_proc = &nfs4_procedures[NFSPROC4_CLNT_WRITE];
}

static int nfs4_commit_done(struct rpc_task *task, struct nfs_write_data *data)
{
	struct inode *inode = data->inode;
	
	nfs4_sequence_done(NFS_SERVER(inode), &data->res.seq_res,
			   task->tk_status);
	if (nfs4_async_handle_error(task, NFS_SERVER(inode), NULL) == -EAGAIN) {
		nfs4_restart_rpc(task, NFS_SERVER(inode)->nfs_client);
		return -EAGAIN;
	}
	nfs4_sequence_free_slot(NFS_SERVER(inode)->nfs_client,
				&data->res.seq_res);
	nfs_refresh_inode(inode, data->res.fattr);
	return 0;
}

static void nfs4_proc_commit_setup(struct nfs_write_data *data, struct rpc_message *msg)
{
	struct nfs_server *server = NFS_SERVER(data->inode);
	
	data->args.bitmask = server->cache_consistency_bitmask;
	data->res.server = server;
	msg->rpc_proc = &nfs4_procedures[NFSPROC4_CLNT_COMMIT];
}

/*
 * nfs4_proc_async_renew(): This is not one of the nfs_rpc_ops; it is a special
 * standalone procedure for queueing an asynchronous RENEW.
 */
static void nfs4_renew_done(struct rpc_task *task, void *data)
{
	struct nfs_client *clp = (struct nfs_client *)task->tk_msg.rpc_argp;
	unsigned long timestamp = (unsigned long)data;

	if (task->tk_status < 0) {
		/* Unless we're shutting down, schedule state recovery! */
		if (test_bit(NFS_CS_RENEWD, &clp->cl_res_state) != 0)
			nfs4_schedule_state_recovery(clp);
		return;
	}
	spin_lock(&clp->cl_lock);
	if (time_before(clp->cl_last_renewal,timestamp))
		clp->cl_last_renewal = timestamp;
	spin_unlock(&clp->cl_lock);
}

static const struct rpc_call_ops nfs4_renew_ops = {
	.rpc_call_done = nfs4_renew_done,
};

int nfs4_proc_async_renew(struct nfs_client *clp, struct rpc_cred *cred)
{
	struct rpc_message msg = {
		.rpc_proc	= &nfs4_procedures[NFSPROC4_CLNT_RENEW],
		.rpc_argp	= clp,
		.rpc_cred	= cred,
	};

	return rpc_call_async(clp->cl_rpcclient, &msg, RPC_TASK_SOFT,
			&nfs4_renew_ops, (void *)jiffies);
}

int nfs4_proc_renew(struct nfs_client *clp, struct rpc_cred *cred)
{
	struct rpc_message msg = {
		.rpc_proc	= &nfs4_procedures[NFSPROC4_CLNT_RENEW],
		.rpc_argp	= clp,
		.rpc_cred	= cred,
	};
	unsigned long now = jiffies;
	int status;

	status = rpc_call_sync(clp->cl_rpcclient, &msg, 0);
	if (status < 0)
		return status;
	spin_lock(&clp->cl_lock);
	if (time_before(clp->cl_last_renewal,now))
		clp->cl_last_renewal = now;
	spin_unlock(&clp->cl_lock);
	return 0;
}

static inline int nfs4_server_supports_acls(struct nfs_server *server)
{
	return (server->caps & NFS_CAP_ACLS)
		&& (server->acl_bitmask & ACL4_SUPPORT_ALLOW_ACL)
		&& (server->acl_bitmask & ACL4_SUPPORT_DENY_ACL);
}

/* Assuming that XATTR_SIZE_MAX is a multiple of PAGE_CACHE_SIZE, and that
 * it's OK to put sizeof(void) * (XATTR_SIZE_MAX/PAGE_CACHE_SIZE) bytes on
 * the stack.
 */
#define NFS4ACL_MAXPAGES (XATTR_SIZE_MAX >> PAGE_CACHE_SHIFT)

static void buf_to_pages(const void *buf, size_t buflen,
		struct page **pages, unsigned int *pgbase)
{
	const void *p = buf;

	*pgbase = offset_in_page(buf);
	p -= *pgbase;
	while (p < buf + buflen) {
		*(pages++) = virt_to_page(p);
		p += PAGE_CACHE_SIZE;
	}
}

struct nfs4_cached_acl {
	int cached;
	size_t len;
	char data[0];
};

static void nfs4_set_cached_acl(struct inode *inode, struct nfs4_cached_acl *acl)
{
	struct nfs_inode *nfsi = NFS_I(inode);

	spin_lock(&inode->i_lock);
	kfree(nfsi->nfs4_acl);
	nfsi->nfs4_acl = acl;
	spin_unlock(&inode->i_lock);
}

static void nfs4_zap_acl_attr(struct inode *inode)
{
	nfs4_set_cached_acl(inode, NULL);
}

static inline ssize_t nfs4_read_cached_acl(struct inode *inode, char *buf, size_t buflen)
{
	struct nfs_inode *nfsi = NFS_I(inode);
	struct nfs4_cached_acl *acl;
	int ret = -ENOENT;

	spin_lock(&inode->i_lock);
	acl = nfsi->nfs4_acl;
	if (acl == NULL)
		goto out;
	if (buf == NULL) /* user is just asking for length */
		goto out_len;
	if (acl->cached == 0)
		goto out;
	ret = -ERANGE; /* see getxattr(2) man page */
	if (acl->len > buflen)
		goto out;
	memcpy(buf, acl->data, acl->len);
out_len:
	ret = acl->len;
out:
	spin_unlock(&inode->i_lock);
	return ret;
}

static void nfs4_write_cached_acl(struct inode *inode, const char *buf, size_t acl_len)
{
	struct nfs4_cached_acl *acl;

	if (buf && acl_len <= PAGE_SIZE) {
		acl = kmalloc(sizeof(*acl) + acl_len, GFP_KERNEL);
		if (acl == NULL)
			goto out;
		acl->cached = 1;
		memcpy(acl->data, buf, acl_len);
	} else {
		acl = kmalloc(sizeof(*acl), GFP_KERNEL);
		if (acl == NULL)
			goto out;
		acl->cached = 0;
	}
	acl->len = acl_len;
out:
	nfs4_set_cached_acl(inode, acl);
}

static ssize_t __nfs4_get_acl_uncached(struct inode *inode, void *buf, size_t buflen)
{
	struct page *pages[NFS4ACL_MAXPAGES];
	struct nfs_getaclargs args = {
		.fh = NFS_FH(inode),
		.acl_pages = pages,
		.acl_len = buflen,
	};
	struct nfs_getaclres res = {
		.acl_len = buflen,
	};
	void *resp_buf;
	struct rpc_message msg = {
		.rpc_proc = &nfs4_procedures[NFSPROC4_CLNT_GETACL],
		.rpc_argp = &args,
		.rpc_resp = &res,
	};
	struct page *localpage = NULL;
	int ret;

	if (buflen < PAGE_SIZE) {
		/* As long as we're doing a round trip to the server anyway,
		 * let's be prepared for a page of acl data. */
		localpage = alloc_page(GFP_KERNEL);
		resp_buf = page_address(localpage);
		if (localpage == NULL)
			return -ENOMEM;
		args.acl_pages[0] = localpage;
		args.acl_pgbase = 0;
		args.acl_len = PAGE_SIZE;
	} else {
		resp_buf = buf;
		buf_to_pages(buf, buflen, args.acl_pages, &args.acl_pgbase);
	}
	ret = nfs4_call_sync(NFS_SERVER(inode), &msg, &args, &res, 0);
	if (ret)
		goto out_free;
	if (res.acl_len > args.acl_len)
		nfs4_write_cached_acl(inode, NULL, res.acl_len);
	else
		nfs4_write_cached_acl(inode, resp_buf, res.acl_len);
	if (buf) {
		ret = -ERANGE;
		if (res.acl_len > buflen)
			goto out_free;
		if (localpage)
			memcpy(buf, resp_buf, res.acl_len);
	}
	ret = res.acl_len;
out_free:
	if (localpage)
		__free_page(localpage);
	return ret;
}

static ssize_t nfs4_get_acl_uncached(struct inode *inode, void *buf, size_t buflen)
{
	struct nfs4_exception exception = { };
	ssize_t ret;
	do {
		ret = __nfs4_get_acl_uncached(inode, buf, buflen);
		if (ret >= 0)
			break;
		ret = nfs4_handle_exception(NFS_SERVER(inode), ret, &exception);
	} while (exception.retry);
	return ret;
}

static ssize_t nfs4_proc_get_acl(struct inode *inode, void *buf, size_t buflen)
{
	struct nfs_server *server = NFS_SERVER(inode);
	int ret;

	if (!nfs4_server_supports_acls(server))
		return -EOPNOTSUPP;
	ret = nfs_revalidate_inode(server, inode);
	if (ret < 0)
		return ret;
	ret = nfs4_read_cached_acl(inode, buf, buflen);
	if (ret != -ENOENT)
		return ret;
	return nfs4_get_acl_uncached(inode, buf, buflen);
}

static int __nfs4_proc_set_acl(struct inode *inode, const void *buf, size_t buflen)
{
	struct nfs_server *server = NFS_SERVER(inode);
	struct page *pages[NFS4ACL_MAXPAGES];
	struct nfs_setaclargs arg = {
		.fh		= NFS_FH(inode),
		.acl_pages	= pages,
		.acl_len	= buflen,
	};
	struct nfs_setaclres res;
	struct rpc_message msg = {
		.rpc_proc	= &nfs4_procedures[NFSPROC4_CLNT_SETACL],
		.rpc_argp	= &arg,
		.rpc_resp	= &res,
	};
	int ret;

	if (!nfs4_server_supports_acls(server))
		return -EOPNOTSUPP;
	nfs_inode_return_delegation(inode);
	buf_to_pages(buf, buflen, arg.acl_pages, &arg.acl_pgbase);
	ret = nfs4_call_sync(server, &msg, &arg, &res, 1);
	nfs_access_zap_cache(inode);
	nfs_zap_acl_cache(inode);
	return ret;
}

static int nfs4_proc_set_acl(struct inode *inode, const void *buf, size_t buflen)
{
	struct nfs4_exception exception = { };
	int err;
	do {
		err = nfs4_handle_exception(NFS_SERVER(inode),
				__nfs4_proc_set_acl(inode, buf, buflen),
				&exception);
	} while (exception.retry);
	return err;
}

static int
_nfs4_async_handle_error(struct rpc_task *task, const struct nfs_server *server, struct nfs_client *clp, struct nfs4_state *state)
{
	if (!clp || task->tk_status >= 0)
		return 0;
	switch(task->tk_status) {
		case -NFS4ERR_ADMIN_REVOKED:
		case -NFS4ERR_BAD_STATEID:
		case -NFS4ERR_OPENMODE:
			if (state == NULL)
				break;
			nfs4_state_mark_reclaim_nograce(clp, state);
		case -NFS4ERR_STALE_CLIENTID:
		case -NFS4ERR_STALE_STATEID:
		case -NFS4ERR_EXPIRED:
			rpc_sleep_on(&clp->cl_rpcwaitq, task, NULL);
			nfs4_schedule_state_recovery(clp);
			if (test_bit(NFS4CLNT_MANAGER_RUNNING, &clp->cl_state) == 0)
				rpc_wake_up_queued_task(&clp->cl_rpcwaitq, task);
			task->tk_status = 0;
			return -EAGAIN;
#if defined(CONFIG_NFS_V4_1)
		case -NFS4ERR_BADSESSION:
		case -NFS4ERR_BADSLOT:
		case -NFS4ERR_BAD_HIGH_SLOT:
		case -NFS4ERR_DEADSESSION:
		case -NFS4ERR_CONN_NOT_BOUND_TO_SESSION:
		case -NFS4ERR_SEQ_FALSE_RETRY:
		case -NFS4ERR_SEQ_MISORDERED:
			dprintk("%s ERROR %d, Reset session\n", __func__,
				task->tk_status);
			set_bit(NFS4CLNT_SESSION_SETUP, &clp->cl_state);
			task->tk_status = 0;
			return -EAGAIN;
#endif /* CONFIG_NFS_V4_1 */
		case -NFS4ERR_DELAY:
			if (server)
				nfs_inc_server_stats(server, NFSIOS_DELAY);
		case -NFS4ERR_GRACE:
			rpc_delay(task, NFS4_POLL_RETRY_MAX);
			task->tk_status = 0;
			return -EAGAIN;
		case -NFS4ERR_OLD_STATEID:
			task->tk_status = 0;
			return -EAGAIN;
	}
	task->tk_status = nfs4_map_errors(task->tk_status);
	return 0;
}

static int
nfs4_async_handle_error(struct rpc_task *task, const struct nfs_server *server, struct nfs4_state *state)
{
	return _nfs4_async_handle_error(task, server, server->nfs_client, state);
}

int nfs4_proc_setclientid(struct nfs_client *clp, u32 program, unsigned short port, struct rpc_cred *cred)
{
	nfs4_verifier sc_verifier;
	struct nfs4_setclientid setclientid = {
		.sc_verifier = &sc_verifier,
		.sc_prog = program,
	};
	struct rpc_message msg = {
		.rpc_proc = &nfs4_procedures[NFSPROC4_CLNT_SETCLIENTID],
		.rpc_argp = &setclientid,
		.rpc_resp = clp,
		.rpc_cred = cred,
	};
	__be32 *p;
	int loop = 0;
	int status;

	p = (__be32*)sc_verifier.data;
	*p++ = htonl((u32)clp->cl_boot_time.tv_sec);
	*p = htonl((u32)clp->cl_boot_time.tv_nsec);

	for(;;) {
		setclientid.sc_name_len = scnprintf(setclientid.sc_name,
				sizeof(setclientid.sc_name), "%s/%s %s %s %u",
				clp->cl_ipaddr,
				rpc_peeraddr2str(clp->cl_rpcclient,
							RPC_DISPLAY_ADDR),
				rpc_peeraddr2str(clp->cl_rpcclient,
							RPC_DISPLAY_PROTO),
				clp->cl_rpcclient->cl_auth->au_ops->au_name,
				clp->cl_id_uniquifier);
		setclientid.sc_netid_len = scnprintf(setclientid.sc_netid,
				sizeof(setclientid.sc_netid),
				rpc_peeraddr2str(clp->cl_rpcclient,
							RPC_DISPLAY_NETID));
		setclientid.sc_uaddr_len = scnprintf(setclientid.sc_uaddr,
				sizeof(setclientid.sc_uaddr), "%s.%u.%u",
				clp->cl_ipaddr, port >> 8, port & 255);

		status = rpc_call_sync(clp->cl_rpcclient, &msg, 0);
		if (status != -NFS4ERR_CLID_INUSE)
			break;
		if (signalled())
			break;
		if (loop++ & 1)
			ssleep(clp->cl_lease_time + 1);
		else
			if (++clp->cl_id_uniquifier == 0)
				break;
	}
	return status;
}

static int _nfs4_proc_setclientid_confirm(struct nfs_client *clp, struct rpc_cred *cred)
{
	struct nfs_fsinfo fsinfo;
	struct rpc_message msg = {
		.rpc_proc = &nfs4_procedures[NFSPROC4_CLNT_SETCLIENTID_CONFIRM],
		.rpc_argp = clp,
		.rpc_resp = &fsinfo,
		.rpc_cred = cred,
	};
	unsigned long now;
	int status;

	now = jiffies;
	status = rpc_call_sync(clp->cl_rpcclient, &msg, 0);
	if (status == 0) {
		spin_lock(&clp->cl_lock);
		clp->cl_lease_time = fsinfo.lease_time * HZ;
		clp->cl_last_renewal = now;
		spin_unlock(&clp->cl_lock);
	}
	return status;
}

int nfs4_proc_setclientid_confirm(struct nfs_client *clp, struct rpc_cred *cred)
{
	long timeout = 0;
	int err;
	do {
		err = _nfs4_proc_setclientid_confirm(clp, cred);
		switch (err) {
			case 0:
				return err;
			case -NFS4ERR_RESOURCE:
				/* The IBM lawyers misread another document! */
			case -NFS4ERR_DELAY:
				err = nfs4_delay(clp->cl_rpcclient, &timeout);
		}
	} while (err == 0);
	return err;
}

struct nfs4_delegreturndata {
	struct nfs4_delegreturnargs args;
	struct nfs4_delegreturnres res;
	struct nfs_fh fh;
	nfs4_stateid stateid;
	unsigned long timestamp;
	struct nfs_fattr fattr;
	int rpc_status;
};

static void nfs4_delegreturn_done(struct rpc_task *task, void *calldata)
{
	struct nfs4_delegreturndata *data = calldata;

	nfs4_sequence_done_free_slot(data->res.server, &data->res.seq_res,
				     task->tk_status);

	data->rpc_status = task->tk_status;
	if (data->rpc_status == 0)
		renew_lease(data->res.server, data->timestamp);
}

static void nfs4_delegreturn_release(void *calldata)
{
	kfree(calldata);
}

#if defined(CONFIG_NFS_V4_1)
static void nfs4_delegreturn_prepare(struct rpc_task *task, void *data)
{
	struct nfs4_delegreturndata *d_data;

	d_data = (struct nfs4_delegreturndata *)data;

	if (nfs4_setup_sequence(d_data->res.server->nfs_client,
				&d_data->args.seq_args,
				&d_data->res.seq_res, 1, task))
		return;
	rpc_call_start(task);
}
#endif /* CONFIG_NFS_V4_1 */

static const struct rpc_call_ops nfs4_delegreturn_ops = {
#if defined(CONFIG_NFS_V4_1)
	.rpc_call_prepare = nfs4_delegreturn_prepare,
#endif /* CONFIG_NFS_V4_1 */
	.rpc_call_done = nfs4_delegreturn_done,
	.rpc_release = nfs4_delegreturn_release,
};

static int _nfs4_proc_delegreturn(struct inode *inode, struct rpc_cred *cred, const nfs4_stateid *stateid, int issync)
{
	struct nfs4_delegreturndata *data;
	struct nfs_server *server = NFS_SERVER(inode);
	struct rpc_task *task;
	struct rpc_message msg = {
		.rpc_proc = &nfs4_procedures[NFSPROC4_CLNT_DELEGRETURN],
		.rpc_cred = cred,
	};
	struct rpc_task_setup task_setup_data = {
		.rpc_client = server->client,
		.rpc_message = &msg,
		.callback_ops = &nfs4_delegreturn_ops,
		.flags = RPC_TASK_ASYNC,
	};
	int status = 0;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (data == NULL)
		return -ENOMEM;
	data->args.fhandle = &data->fh;
	data->args.stateid = &data->stateid;
	data->args.bitmask = server->attr_bitmask;
	nfs_copy_fh(&data->fh, NFS_FH(inode));
	memcpy(&data->stateid, stateid, sizeof(data->stateid));
	data->res.fattr = &data->fattr;
	data->res.server = server;
	data->res.seq_res.sr_slotid = NFS4_MAX_SLOT_TABLE;
	nfs_fattr_init(data->res.fattr);
	data->timestamp = jiffies;
	data->rpc_status = 0;

	task_setup_data.callback_data = data;
	msg.rpc_argp = &data->args,
	msg.rpc_resp = &data->res,
	task = rpc_run_task(&task_setup_data);
	if (IS_ERR(task))
		return PTR_ERR(task);
	if (!issync)
		goto out;
	status = nfs4_wait_for_completion_rpc_task(task);
	if (status != 0)
		goto out;
	status = data->rpc_status;
	if (status != 0)
		goto out;
	nfs_refresh_inode(inode, &data->fattr);
out:
	rpc_put_task(task);
	return status;
}

int nfs4_proc_delegreturn(struct inode *inode, struct rpc_cred *cred, const nfs4_stateid *stateid, int issync)
{
	struct nfs_server *server = NFS_SERVER(inode);
	struct nfs4_exception exception = { };
	int err;
	do {
		err = _nfs4_proc_delegreturn(inode, cred, stateid, issync);
		switch (err) {
			case -NFS4ERR_STALE_STATEID:
			case -NFS4ERR_EXPIRED:
			case 0:
				return 0;
		}
		err = nfs4_handle_exception(server, err, &exception);
	} while (exception.retry);
	return err;
}

#define NFS4_LOCK_MINTIMEOUT (1 * HZ)
#define NFS4_LOCK_MAXTIMEOUT (30 * HZ)

/* 
 * sleep, with exponential backoff, and retry the LOCK operation. 
 */
static unsigned long
nfs4_set_lock_task_retry(unsigned long timeout)
{
	schedule_timeout_killable(timeout);
	timeout <<= 1;
	if (timeout > NFS4_LOCK_MAXTIMEOUT)
		return NFS4_LOCK_MAXTIMEOUT;
	return timeout;
}

static int _nfs4_proc_getlk(struct nfs4_state *state, int cmd, struct file_lock *request)
{
	struct inode *inode = state->inode;
	struct nfs_server *server = NFS_SERVER(inode);
	struct nfs_client *clp = server->nfs_client;
	struct nfs_lockt_args arg = {
		.fh = NFS_FH(inode),
		.fl = request,
	};
	struct nfs_lockt_res res = {
		.denied = request,
	};
	struct rpc_message msg = {
		.rpc_proc	= &nfs4_procedures[NFSPROC4_CLNT_LOCKT],
		.rpc_argp       = &arg,
		.rpc_resp       = &res,
		.rpc_cred	= state->owner->so_cred,
	};
	struct nfs4_lock_state *lsp;
	int status;

	arg.lock_owner.clientid = clp->cl_clientid;
	status = nfs4_set_lock_state(state, request);
	if (status != 0)
		goto out;
	lsp = request->fl_u.nfs4_fl.owner;
	arg.lock_owner.id = lsp->ls_id.id;
	status = nfs4_call_sync(server, &msg, &arg, &res, 1);
	switch (status) {
		case 0:
			request->fl_type = F_UNLCK;
			break;
		case -NFS4ERR_DENIED:
			status = 0;
	}
	request->fl_ops->fl_release_private(request);
out:
	return status;
}

static int nfs4_proc_getlk(struct nfs4_state *state, int cmd, struct file_lock *request)
{
	struct nfs4_exception exception = { };
	int err;

	do {
		err = nfs4_handle_exception(NFS_SERVER(state->inode),
				_nfs4_proc_getlk(state, cmd, request),
				&exception);
	} while (exception.retry);
	return err;
}

static int do_vfs_lock(struct file *file, struct file_lock *fl)
{
	int res = 0;
	switch (fl->fl_flags & (FL_POSIX|FL_FLOCK)) {
		case FL_POSIX:
			res = posix_lock_file_wait(file, fl);
			break;
		case FL_FLOCK:
			res = flock_lock_file_wait(file, fl);
			break;
		default:
			BUG();
	}
	return res;
}

struct nfs4_unlockdata {
	struct nfs_locku_args arg;
	struct nfs_locku_res res;
	struct nfs4_lock_state *lsp;
	struct nfs_open_context *ctx;
	struct file_lock fl;
	const struct nfs_server *server;
	unsigned long timestamp;
};

static struct nfs4_unlockdata *nfs4_alloc_unlockdata(struct file_lock *fl,
		struct nfs_open_context *ctx,
		struct nfs4_lock_state *lsp,
		struct nfs_seqid *seqid)
{
	struct nfs4_unlockdata *p;
	struct inode *inode = lsp->ls_state->inode;

	p = kzalloc(sizeof(*p), GFP_KERNEL);
	if (p == NULL)
		return NULL;
	p->arg.fh = NFS_FH(inode);
	p->arg.fl = &p->fl;
	p->arg.seqid = seqid;
	p->res.seqid = seqid;
	p->res.seq_res.sr_slotid = NFS4_MAX_SLOT_TABLE;
	p->arg.stateid = &lsp->ls_stateid;
	p->lsp = lsp;
	atomic_inc(&lsp->ls_count);
	/* Ensure we don't close file until we're done freeing locks! */
	p->ctx = get_nfs_open_context(ctx);
	memcpy(&p->fl, fl, sizeof(p->fl));
	p->server = NFS_SERVER(inode);
	return p;
}

static void nfs4_locku_release_calldata(void **
 *)
{
	struct
 *  funs/nft-si *.c
 ns fo=Sv4.
;
	nfs_free_seqid( NFSv4.
->arg.) 200)Copyri4_putfatio_state2 The Regenlsp Unity oyrigopen_context.
 *  All rctxts rkht (.
 *  All);
}

higaic  Clie declatios/done(ocedurerpc_task *edis,son   nt-side pru>
 *
  declarations for NFSv4.FSv4.
  Coniversisequenceich.ed
 *  All rserver, &
 *  All rresof t_res,
			   ibut->ticy Adusts rif (RPC_ASSASSINATED(ibut))
		return;
	switch of soare met:FSv4.{
		case 0:itiomemcpy wit  All righ conmet:
nid.t-sidotic list the followi andsclaimer.
 *ngsizeofs listn thconllowins  2. Redist)ts rerm mew_s4propermitted souvided  disclaimetimestamhts r		break. Re*    -NFS4ERR_BAD_STATEIDnotigist of conr iOLthe list   documenty for anSTALEr other materials providedEXPIRE matedistributidefault  doc 1. modifasync_handle_error the inary form m, this lNULL) == -EAGAINrce 	 modifr reprt_rpc  rm mibutiocumentation  this->ved.clientts r}  modificy for, @umic)lodrick Smith ftwabovwithout speher mathaisclaimer.
 *ngrm mist ndy Adamon and<andros@umipreparedu>without
Ributtriburovidedd use in sou proTHE binarys, wms,TWARE orTWARE IS listy nor t_wait_on written .
 *  All rtsn ththe, list) != 0 procode musty nobinary form must reflags &ith _LOCK_INITIALIZEDo end0ight
 /* Note: exit _WARE IS_ runnNY E WARRANTIESare  */
		claiaboveacrovid=sed t. ReHALL THE }
cumentationust repro = jiffiesHE REGErior wrtupLAR PURPOSE ARE listIS SOFTWAREDATAPROVIDED ``AS IS'' ADISCLAI_argmust r BUSINESS INND ANY EXPR, 1D. IN r proHALL THE T NOist ohigrt the uSS OR IMPLIEconst ING, BUT NOHER Iops, OR
 *  CONING
=RECT.T (INCLUD, ISE)  =  WARRANTIESRISING ,OTHERWISE) ASEQUENON ANWAY OUTSEQUEUSE OF
 *  pro, OR PRO, EVENOF THE c NFSv4.
,
};OR IMPLIEING, BUT NOT LIMImodifdonaryckDING, BUfileros@u *flher urce and b without
Kend *ctxx/errno.h>
#indros@uns and *lspg.h>
#include cf the *LAIME n source and binary forms, wiut
 *pING, BUT NOmessage msg OR OTSE OF H DA = &modiftource ans[NFSPROC4_CLNTludeFU]her E OF Tred = ctx->nt.h,
	} <linuxs@um_fsibutR SERVBUT NOe <liIor withmodule.h>IS PRODINGFS_CLIENT( must reprodu->inode)modulemounclude <= &msgmodulist backENCE ORlude <EGLIGt.h"modulworkqueu POSSIBiod_
#define ncludeBE LI=NOT LTASK_ASYNCmodule
RISIEns andthis iprod ary fo - when cancelARY,a atio,AIME
	 *Y_MAX	(ed)
ILITY		pasNG, in,NTIESit won't beneith 4_PO.ith /
	fl->fl_typal.hF_UNLCKTRY_or withrsclailloinary forms,(fl,de <, clnt fs.h>
Uniy norr witING, BIRECT,ved.ermiss the2o(struct NOT LIM n anPTR(-ENOMEMcr wr 
	msgE OF arglude&E
 *  DIS, of its
 *resocedur *  Rm mustux/sunrpc/bc_xpde "iohigaor without
 *po *);
sITY,suninclu(& struct nfs4_st, STRIC *);ABinand binludeduledelayncludrior wuct nfocedu,r *nacmd,/modulemtructdex/morrittstude <int _nfsnu_n.h"
ttr(si*  ffs.I(legy fors_se#x/module.h(struct le.h>.h>*f of it, str/moduleruct nfclnude dulenclude <luMITED ct nnY, Oopyr = 0;
	unsigned char flSPROC

#d4_H DA_s	if PROC
 nfshigaic inDSe <lSEof M met:
n..h>
#at	if (erg;Y_MINU
statL_before_ we doY		NITY	nary int _	 1. err >= -1000 |= FL_EXISTS;
	down_read(ludei->rwsem nfs.h>
so_vfse "ca(intk("%s coulilsclaimer iR, INorNOENTIRECT,up %d\n"ditio	__func__, 	goto outITED TarES Otmaps, wiGETATTR rREGENeturn TY OFT Squests wit//* Is0)ILITa detructOOP_ON_?		dpry notest_bit(ver,DELEGreprr othe, &truct nPROC
*  LIRD0_TYPE
	|lt nfsintk("%s couu.
/* Pfl.ownerx/mot nfserr;
**
 *);struct& must reprstruct return er, -er4_2 nf[2]  F_bitm*;

/*EID,
	WDEV
4_WT LIMR4_WO	smoduledel	if (er;

/*u intolude <linux/r	}
 code m -EIO;)o_fsinfo(uct nfsreturn erPTR_ERRTRACT, SORD0_ISLITY, 32 nSPACEORD1_SPACEETADATA
	| FATPARTIfor_compleSE) derict nfsmap[2fs4e.h>
utS_FREE
	|AWDEoumodifntk("%s could not=(	| Ferr)TTR4f (erl = {
	FA OR Itr);
WORD1reveservetattrD1_TIME_nt nfsON)  arg *fattr);

/*ut nfsres rITUTETOTAATTR4_WORD1_nt leakISCLAth or| FAetattr(struct x nfsi2] = {
	F0_tattr(strufl
 *  fmap_erloncumet reproTHE */
ILESTR4_W_FR			| _MAX_Ller *f_WORD0_MAX_ftwares@umthis;
getattr(struct x/m = {
	FATTR4_Wint _nD1_OWNGROUP
	| h *fhan4_WORD0_LEASE_Th>
#include <0
};

const u32 ngLEASE_TIMEditio0
nt leaks of_bitmap[2] = {
	Fy forms, wORD0_MAXNAMerr)erv_FILEI=TR4_e "_CHANGE nfs_fhD1_SPACEsinfoMAXWRITEitio| Fserver, <liER(h>
#h *f NFS  kzD1_OW(nsLIEDb*p), GFP_KERNELATTR4stapVS_TOTALSPAC_proc_l, stUT
	pINTERRfhserver,FHLINKSS_TO| FATTR4_lcedupTR4_	| FATTR4_FSv4.| FATTR4_{
	FATOWNER
	| FATTR4_WORruct nME_US->soME_METADAT_GROUSED
	|WORD1_TIME_MOUNTED_ON_FILEI_SP     OUNTED_ON_ATTR4__WORD1_TIME_METADATA
	| FATTR4_WO| FATTR4_ruct dMOUNdir(u64 cookD
};

 = {
	F ClieATTR4uct nfsetup %d\n	struc  2. Rcedu must reproducee32 *start, *p;ME_MO.IS PROG_ON(IS SOFTWARE IS PRO-> struc "n < 80);
intk(nfs4ie > 2readds4_fsinfid.(&readdir.
 	struct nfs4intryditi
};

conft, *->veriNY EXPR.srssionreaddf coORD1_SLOT_TABLEOUNTEDdentry_WORD0p DATA, Ort, *->co;
	atomico_biNTED_ON_FIcoupeasynph <km = geoviddclude <linux/rikmsmiteefinisNGE
	_,  * TWNER
	|'
	 *AGES _proc_lpAL,
	fh *, struc:niverfh *, structntry,
	TED FATFILTes he     :ith CONSEnd truct dRAWDEV
 OR IMPLIED
 *  WARRANTIT OF THEfs4_WDEV
 fo *);
 */
coO, THE re pING, BOUNTED_ON_FILE0_FSIDtart or withost of c	FATTR4_WORD1LI
	readdirs_fithout
rm must reproducOF THETR4_WORD: begin!itma  GETATTRWNER_GROR A IL
	|CUICES; LOSSE
 *  DISC	struct nfY OF
 *OATTR4_WOHALL or 2/* DoEMOTneG, BoTEIOan FSv4.toRD1_R=ME_MO	FATTR4_WO!er ma*/
		*p++ = x/* r));itten kmap_atIsize
 SEQID_CONFIRMED)IRECT,tENTIAL*p++ = xdr_zero;*/
		*p++ = WORD1_TIME_ookie, first wo * insteads4_op*/
		*p++ = 
	BUreaddruct dTIcount(&reaemcpy(p, ".'
	 *dr_o		ret = 1dr_ou32 nf	memWORD1_TIME_MEmcpy(p, ".\0one;(cookie} elsenl(FATTR4 length
		memcpy(p, hddirROCUREMR4_WOF listSUBSTATTR4 GOOr;
	switRVICES; LOSSUSE,r));
	ifRS; OFITS; OR nfs_sertasRUPTI2 nfHOWEVERSE ON D OOFTWARTHEORYer lengtqstrILITY, WHESE OOF
 CON u32 nfs*/
		*p++ = xdare !claiFATT%d		*p++ = xdr_,                 ESS OR IMPLIED
 *  WARRANTI are pING, BUT NOT LIMITED TO, THE . NFS/procart = p = kTR4_2)
		r(*tart, *->pages, KM_d */
		*p++ = xdd */
		*p++ = xdr_one prior written pabov     t(&re.notice, this ltIS'' AND ANY EXPRAXFILons
s listabove copyrig*MITED e            =th */
	*p++ = htTTR41.NOT LIMITED TO,ISCLA WARRANRD0_TYPE
	|-err)ervnl(8);              /*Y OFlenENTIAL= p = kmap_atILEID eurn; *ARE ICULArmDATA
	| ht
 cpy(p, ".\0      0itmae);

	recati_TYPE
	|
conHALL_hyper(p,BLE
 FS_FIL(nclu
 and '.y form must reproduce the ly(p, ".\s list2map */
	*p++ = htoR
	|*  MER THE Rreproduc10)
AMAGES  Rdr_zerIBUTORt err)
|erver,ludeORON ANDIREC4_WORnfs_fh   /* *dentrNUML *  TH<km	memth.dID(de->d* attr)_USER0);ust reproduce}s heFAT*/
		*p++ = xdr_oifier, fir
		*p++ = xdr_py(p, ".\0tw0", 4);       ++ = xdr_orifier, seOF THE son    andleep();

	if (*timeou/*EID(denxdr_encoe, thir->c"..\0\0", 4)t *clnt, long *timeoFS4_POe == h	| FATT nfsong *timeout)u32 nf__, -err)erv->EAD
	| FA
	    ".\0e_MAXNAMor_clnto IMPrlandE_bitic(*reORD1_TIME_Mh"
#);  /*TLNT_MANAGs ndorREfsinttr = htonl(8);	struct nfe = (ond wtfs
	| WORD1_R= hyper(p, NFS_FS_TOTAL	ddirETADATAreshedule_15*H                     xe));

	re_RE nfsMAXrce *diti IS =  that ar  And/
#inclyn th(err) {
	t of condnd terovidreturn entries = -ERESTAR*  T CONSILITY,snt, long *timeout)
                truct qstrnext */OR TORERWISE) ANCE TTR4N "cat.h"
#i USE OF THIS
RISINGOF
 TWARE, EVfer  or 2DVISED THISlengthOSSIBILITY N IF ADVI    	switchNFSDBGnext  eRTSY0;
/TTR4_WORD0_L *)ss4_eATTR4setlKS
	| hconf_bitmapokie == TOTAFATTR4_WOR_GROUP
	| F* neres | FATbinaim                /* entry len */RD0_MAXNAMmeout);
	if (fataodule.h>
#ions_bitmap[2] = {
	Fe ==emcp	case -NFS4ERR_STALE_ameions_bitmap[2LEASE_TI    nclud       ME_MODIFY
D0_LEASE_TISv4 errors into};

const uuct nfs4_stre_rec_bitmap[2]"ATTR4
		c"_bitmaruct nfs_fh eption->rinternopenNFIG_NFS_V4_1);
steak;
#else /* calostaeak;ILITY		NFNFSDBG_FACnext 		4_has_smei.		if (!nfs4_h
		reL_hand	| FATet;
			/* SOURCdir->veion(const>		break;
		*p+en*
 * RD1_OWNGROUP
	| FATTE1_MOUNTED_ON_FILEI_TIATTR4prOUNTED hancase -N_TIME_MEFS4CINKRETRY_MAX;
	| FA;

/*1_TIME_MERAD_HIGH_SLOT:
ItimeTLKW(cmdor ha(FATTR4_bub FATR htonlREGENR_GROtending(cu
		case -NDERED:
	_SEQ_Mf its
 *  D1_TIME_*  Redistr,rm mlienOPENMODE:
 -NFS4ERR_DEAENMODE:ERED:
eproc_lNULL)
				brea4_signalookupD1_TIME_n.h"
follrrcode);
	ERED:
is the error ha_proc_l
starbitmap[2fs4tatic S_AVAILtart = p = kmap_atine fFILES_TOTALTIs4_sEt-= len */
	tatic EN:
		case -NFS4           /* aonlhev4 e_timnt, &edlNY Erout		NF, wipr == es that ar  And_GROUP
	| FATTR4s_fh D1_Tre_RESO_WORD1_oROUGSSION_qsif (PIRE, sIN_REVOendif ase -NFS4ERR_OPENMODE:
			>cl_state);
			excep4ERR_Re_TOTAL
};

conntry,DRACE:
		c1_TIME_MEFS4COPENMODE:
	ase -NFS4ERR_OP4_excepSE) Ampde proce= {t <=FALLTh>
#l(FAoRE2 nfINCach10)
e| FATREf possible...ENNY Dap_aSIZt struct nfs_ser_atomic(*reEN:
		case -NF <= 0)
		NTIALp+ TOTA	er /*  the
 *   :t
 *  :
			FSEQ_FAclaimer i, 1serv* Thiditi!=ntation anDELAYr_CON' AND ANYtionsn thits
ct nfs_c(, this let.h>tslotNTIESturn ewhile (ate slot .retryepLOCA->retrovid1;
ME_M/* We failne;   hexpiredh>
#ase ast_rETADATA{ FATTR4_WOse -(reap_e}
ddir_arg *readreyrigs4pro(ode);
			set_bit(NFS4CLNTrovided s4_map_erRD0_Mditions ct nfs_clENMODE:
ut spec*cl    rovide-dition| FATretain("%s)ght
*    
}

/*
 * ESOed(  /* G_Nst word */
ly drivin the dititurn -E(clpe;
	_lasg conewal,d be able rce st_used_slotid thereRY:
 be able FATpin_uns/nf(&st_used_socktid eq#i	bre= (d_slotid isFS_V4_1)efinate;etry =t (c)te s- id i a ate slot effic spely upd&clp use.talockFSv4.
 id iNY Enone fois:
		vialse.
 *et_recleaWORD  the
 ze don_unlockthe highestOPENonst_unlintk(, &clp==sed top for fol
static void renew_leasate it
 * so that t (cookie == ase - If the freed L
	| FATe prntk("%s could no)
			| _state
		casep->cl_last_CHANGES_TFSv4 nfs_sORD0_FILES_AVAwise we know that the highest_userror ha{ORD1_SPACElast_TYPES_TTR4_WORD1_SPACE_AV_CONdlACCE_MAXN_state);
ddire foll by clETADATA
	| F_servghesth>
#find_sloti<t(tbl->used_slots in t bitmap for GETATTR r-err_before(clp->cl_last_renewal,timestamp))
		cLfs_clienYe/
	*pcan      !
	if (/* ...but aon   races ECIAit is frnd cand is_slo_slot] = {
	FATTR4_WERR_FILE_OPEN & ~FL_SLEEPo we= 0 &&id isid < tbl->maxn usesspin_}

vohighIZE
usarg *readdary fo */
co_state);
to scan down from high			ifslotid)to 0 l    /* last_bit(tbl->used_sldown the sientIDENTwe always want", 4sleep here_tbl_l->highe       /*ditiid in use|	if e_free_s= -1otid < tbl->max_slots)
			tbl->highestT_slotid)id == uct dWARNptio_TABLVFSTY		 IS of PIRE in usock(&manage0_FIut spe FATze dobl;
sloti:V4_1) */
		cas
			if the WORD1_SPACE", __func__,
		free_slo %d\n", __func__FILES_TO	/* We failedze do, u8*/
stn useidused_snt_slotid)=l_waitq);
		 up n dowfrom h}

vo use: No sed_sloable to size down the slot table if needeWhen updatid to sca;
	res->sr_wa4c) 2tion, con *r
{
	retugt !=his  now
 * hiNIED* nfs need orse oro we need se.
 * If none found, te it
 * so that the serher maused_slotid is set to -1.
 */
static void
nfs4_free_slot(struct nfs4_slo
neD1_TIME_tbl->TR4_WOR
	|  *filp		if (state == NULL)
				brea
static void renew_leas0readdnext guy waiting4_WOR	ir->verifier,==  {,timestamp))
	MAXFI    cookie ;

	rslotIMEOUT&clpree_slot(cedn usie =fy			| >sr_yd, tb_modi dmer in thHIGHer, 0_unloiltruct ssed0) NAGER/* bi_TABase -Ntk("%s couxdr_z>sr_ ||onst struct nendtid  useSSION_unlINVAEP
	|imer iSGl_loSE_hanened_sloLIMIuct!er iDEADSEquests.
urred. Thegwn from highes_sessetid in use(comap
 * so wes o ThisoR_>cl_loion->tid;t's sequLion->f
	equests.
 up tbo wehigheslotidresDIFYc -NFenD1_TIMEfc)
		g_set t FATTost std);
	ress +ct n-BLE)
 nfs_fh ot tablap forize down ht
 /* U_slotient  0 lnfs4lot = tbl-oto out;
caENMODs41_sequORD0_FILES_AVAe down the slot tabl>cl_last_renewa
#enuct nfs4_sltus)
{
NTIESut speidion->fRY:lotid in u *   _LOCAtbl->highest_uvent _lock(dprintk sour_WORDPtart RESTARTSYMAXNast_reigna| FA(
		if ers.ast_to -1.
}
	nfs4_fslo after all */
		rpc_wing its respectid)%utus =andallt
 * in the bitmap.
 * If the freed slotid eqfl == 1)
		res->tid)wep)) {res)d_slotii>cl_sso ``AS IS'' -NFS4EwSPACEbAMAGleres)ns i rror)
			c is set t if _oneedD. INotherwise* Ifknow ``AS IS'' l1)
		resp.
 *is stiltid 	*p+equent *clp,
		ch.edstn down from highest_used_es-> 0 loo retaiFILESwal_timtruct 
#inclnfs4_=		breaMer i sizeunfree adse.
 l->h.timeout =frATEID:
			als reshOVIDED ersitcument_staLE* and  - (char *)TED TO,s list3. Neisequ)
			rialsce, this lNTABIak;
#er mateeBLE;
	BUILD_BUG_ON((h un LIMITE use.
 scxceptie_setu *
  NFS_ highesTWARE IS PROasynrn;
	}
	nfs nfs8ust _ir	retuus);
r, 0, /*tonl(8ned(e show THE Rgo onNe co , dprimark(!nfs4	res->", 4);  as /* b15*H> %s IMP.oid nfsNY Dsize;    ILD_BUADM to hanKr, 0, sized_slots, tb
 * aother mateerials providedd	int sreturn;ved and prnc__) of it t_nogs: fitmap.s=%04lxtus ==   /* ase -__,
		staso weed=%d id nfs41_= bitma sessispiner */
		++	__nditio	_,&clp-> ki
	excochat the ++ = SIGLOSTif det(clp->e_free_slot(cbitmap.
 *nfs4_ax_sl now
 * higher, 0, ' AND ANY}he server would be able* just whighest_used_slotid is set to -1.
 */
static void
nfs4WORD1_SPee_slot(strucLITY		NFXot a_NAMEthe V4_ACL "system have acl"t_use nfs fsetxTOTAverFATTRooki NFSNFS4_M,ate *streed *keock(&cCOV	mightbuf	FATTize_t buflentid nfse -NFn source ant -= reACE:
		caFS4_Mnt_rs PAR>seq_nr;strcmp(op !=table if t(&ree;
	p;, first word */
 -EOPNOTSUPPver-cl_lock);
		if (tretaaclE_USEDsed_fA
	| 	if abservbl->[0g

	, win\n" pIG_NsuggestABLELL T15*Hx_sl));
t != unratis4_fr TED  bit *ble;
	hat's w`AS we'll    t(&re.g.	if (ten cl_lastnt *c havee -NFS = Net.ENMOBuargs *armer.
  endr/ext3'sFATTd bytion,D1_TIME_
		if4_scUP, &c supportedENMODnt *clp,
	in kernel-\n", _de;
		s *tb("%s:sp: f| FAifRTSYt *sbl->useturn e(loo    _lock.AX_L
	| FAT0; loER;int _SESop--ow th_slo %s\n", PARTIrest_otid 	timestO EVEspin_< tbl->mretur!SIZE
	| F	bre
#in_ewal = _SETUP,eq_nr;sed_higanoles"t(res, 0, t *slotxceptiohigan_, __fun(o n == != NFS4			tblME_METADATAr_firion\n", _lisatic i/* slot already allocated?f (r		tbllready ar, 0, size)%s\n",leut sstrlen(otid = NFS4_MAX_SLOT_T+t, & * and ructt;
this_The sesstid)shigan>counes->eak;
		if (!tte nee the b	;
	/itmabuf &&X_SLOT_ <res-enewal = timeRANGeof("<-- %stbl- and '.		tblSION_SETUP, &clp->cl,set: s triviallleLARY,tot ad, updat, w_fixnst uferral
	u8 slotiligned long _ync_in*ATTR)TABLEhe slo(		spi->valid       able FlastatS_FIL) &&id <);
	res->sr_slotid us = nu> %s\S_rec>sr_s->srion(TUP, &c)
		retur		__funV4_REFERRALn);
		if	dpr
	re);
	res->sr_sprint()if (status)d_sl |l, edisd_slotidsit(s |
		= NFS4_MAX_SLOTNLINt_id);
	resml,timeS_IFDIR |= NFRUGOed to;XUGO;
	res->srnlin->cl2hest_usent retle;
	WORD1_slot
esetion\nit(NFS4dibinae *state = eqs_);
 up gons_bitmap[2] =    no * If  *->slot_tbl_l the freed41_s*ts +ghest_used_slotid we want to update it
 * sod  <atbmap[the esk[2]
				ex[0->sstatustbl->usspin_ mus>
#iach				ply    A xdrSle;
[1_)
#deintk("<--try,
	_atoONe sessrfs4_wait_clnt_otid);
	res->sr_sap[2ence = erro	.dir_ have not cod_slmodul up WORD} res->.ts + =_lock)SUBST_sloar =ax_sloar_nRD0_M	;
		if (t;
	struSETUP, &last_bi
				exc;
	res->sr_sl= 4_MAin_lock(&s4_wait_clnt_reco	case -NFS4ERR_STALE_STATEID:
		case -NFS4ERR_EXPIRED:
			nfss

/*
 *d_slhedule_sta, __funcdr_one; de);
			set_b* bitm *clp = 
		__func_leep();

	if xdr_zBADSESSION:
		cas_handset _init(&->slot_tbl_l->	res->;
	ristrNFS4_e prddir->verifier,sed 0leveotidR4_Wntk(velyan rp;
	/ORD0_FILES_AVA++ = xync -NFS4ERRNFIG_Nhe ses				gsCIDENT			| otid TR4_WORD1S;
	struRe_do0;

	dprintk(n;
	= NFS4ence(clp->cltruct ed -NFS4ERR_irs nfsr_has_s*p++ = htonlstate, N */
		rpc_w#ifdefout <=Gtus;: No1
/*;
			/4_WO);
	exchange_id()
 c NFSSstrus->srotid != ahas tive bi, t:
	TR4_Wundrst_15*Hence If ENMODssocifreeditq);->srstaltk_(INCLUDIwillNFS4truct nfsENMOation ann tewal = k("-x_sloence(cl			|rslot

ble;
e dowtWORDTA
	ENMObspinisome phNFS44_WOstat=%BLE)tk("<-/wake_up_nextotid);
	rek->tk
		__fulots\n" GETon->ret*cay nate = exceptmeout
	reTABLEbl->us,
		ierBLE)
tiUSED
	l,timestam1dir_arg *reay waitinpinFATTRseon->retryNCLU->nfs_clientclX;
	ldir_arg *rPROC
t in decode_sequennc_4ERR_BADSL l remata;

	dprk */
	--_firsdat:
	/* The sessi.h"
#inin_unlockthe highest withp;
	spin_unlockthe highestNeithen_unlion-EXCH GET_IDurn;
	}
out:ESSION_SETUP,*clp,
		ly,
				gs,
le_state_recoi0_LEASE_TI__be32,timeclp,
	est_uce(dataleep.
 */
static       clres->srATTl,timest free*) just walaime;y,
		lient, &e(h.ed)luence, boot_AXFI.tv_sec NFS/* bi
	retatic void 
		it *s1l_sync_dn_status *
 .
 *
 *)
{= & just wa n
	FSv4.
 1wal_tic.c
 id_tmap.
 ct_sleef(l_, INCLUquen, wync_pre just w	"%s/%s %uize downatus);
	ipaddrze downE_STAeeretur2_tab;

tatusrpcITS; OR
 *  nl(8OT LDISPLAY_ADDRFS4_M(CONne/

#

d_uniqule;
	*clp,TATTR rclp
	*p++ = xtatustruct nfs4_sn the	if (tl4_fi_time;
		spin1_seation anCLID_RCE:Ee_slot(clp, q_res);
}

st_status == 0)lp,s\n",1_s++t tabnt_seque		dple;
-= reaING, ret;
	reSION_SETU1_c);
			sk;
	stpy(ptk("<-- %s *task;
	sNFS4_MAX_SLOn 0;

	orseOUNTED_ON_FIck(&UCH DAAXFI= {
	FAsource and bin_args = arg}->sl*
 **{
		M_USER0);
	
	ifp task_setup = nfs4*e sequence
	 * o_names, INCLlp->cl_last_LD
 *  WARRp task_setup = s send cookie 0
	 * instead oft spec	m str_tbl, ta up tp = Hy = 	| F=_fsin_seq.*  R.h>
#inc=try len */
	ot(cata = {
alleq_args = argp = truct*)= NFS4_POLL_RETRY_MINall = &nfsession);
	= c/* j_frestructtablt = ,    s);
triggerp,
				strsirst_z
 leng	
	rewe'r = tvok_c_dat)in *  SOTIAthe highesent)readdir->count -= rstruct n,
				sf (tbl-c__,
				es->la/
	mxdr_one;   nc_data *da->lrstrue sizeo            
 *
 *claimereset by eply,
		(p, ".\0\0\0", 4);         		.clp__);
		/* just wa npret;
Ceply) fromivedNTIESpr_\n", __ th bitak;
		
				stde_hy,ent *c	*p+ -NFS4E The ndiffs4_callSSION_SETU orret;
}

st_d_sls
		casetupcac    al_1) *k);
	}= &*
 *as i slo_reply,
		uct rpc_clnt *clnt, long *timeoong *timeourned tid != NFS4data-l->slot_

const(NFS4_MAXrgs->sa_session = reatic void tbl->pcity Redis NULL;
	rax_slots)
in bitma*/
	p++;
	*pne(structnce(clp->cl_splyintk(code h */
	*p++ = htonl ust be cent *clp)
c'.'
	 * 
ed_slotou:
	did =ree_scumentation anGRACintk(he_reply)
{
Rlock: ove copyr		.seq_args = args>nfs_client->cl_calD_STATEID:		*p++tokie k;
			/* Y reson Reserver), (msg), &(slot(tn souslots FILEvedtic inS;
	Sskippnfo *);
 */
co = &n(rovided m        /* bicl_s		.seq_args = ) \
	>nfs_cl spe*task;
	sstatmsg++;
gsc_da		.seq_aate = exception->state;
		else {
		ret =et = errorcode;

	exception->retryrgs *args,
		  ret 
	rh(errorcode) {
		case 0:
(struct rpcrpc_stst		casenext(&tbl->slo	else {
		ret R_ERR(task) = &nfs41_cact nfs4_s GETA4_WORed_s>nfsTABLE)
		reteq_res, data->cacgs,
			_e32 *n = rlot(sert nfs	.servt specsk_setup);
	if (IS_ERR(tus);nfs4_setup_lr4_MAXerverivr, st in decode_sequenc	else {
		ret = task= 0)
				exc{
		.;
41_call_synfs4_s;
	}
	
	*ee_sn(c_seqp->c,urs        ;
		if (t(status)e byETADATA0bl->h	| FATTR4e32 *s_sync_doD1_TGET_0_CHAN
		i

static void nfs41_call_sync_done(struct rpcvoidt_clnt_recover(clp);
			if (ret 
				exception->retryruct nfs4_sequencelirintk(1)
			break;
#else /* !defined(CONFIG_NFartefinepare,
*/
selse /* !definor withnt))
c void nfs41_call_sysr_sta	(servermset(&len */
	msr_stiead 0,d_sloof(t, msg, 0);
}

#define nfs4_calls triviallryVALID_A timelp = HRc_staendif /* !defined(CO
			reted as if the serve,
		 down from higclient,
				  FS4_uap_eid = frn 0;

	mem}a s *argsnt, rablede proc	.seq_args = args,
		.seq__res = res,
		.cache_replyRseq_c vostrutructarguct nfs4_"<-- %s ;*gnedatus;_FS4_Mion = NULL;
	Y_MAX	(s re*tbtate_nfmaxg *rect k the old_ata con(stopendivalustatuse prps_fattr fO
or GEy,
	ce : t = task:*
 *_reqs=%u"<--  %pLD_STATEID:
			NFS4ERR_Bp)tbots[at a alreUntilence*		stdyPIRE <l_,
		(struadbl->munc__insse;
ock(upo slot twal_	p->o con.slot()
				bitmapendata *n_lo		*p+FS4ERR_Bwal_ti) \
	)e -NFS;
	in rpc_tp->f_adoes'S4ER be ol cach= PTR
 */
static inrepl++slot-> /*XXX_call_synREQ_TOO_BIGandlingp - (char *)stask->d, updtid);
Y_MAX	8 slse= NFS4SIONso t0; i <t(&pts ose; ++itbl-sp, fmodes[i]KED:
n = {it(&p-aren)
{
ree_befofuncres;
	struc, &ee;

oFS4_POL*s)
{
	str_t{
	str,
	| Fuct nfs_servtbl=%p rpc_stribuendata *=, cache_reply,
	nce_)
qstat procedg.seq;
ORD0_nit(&*ence, aes, rpc_status)atus);
	t = taskt i		if atmp)
{res c_res;
	struc nfs4_map_er->srTA
	aticnelstruc);
sdd_slotinit(&p->f_bitme byetup_->high>client nfs4id nfs4 slots\n"__fitq,				strER_RUNNes.f_TOTA
		__func_ORD0_FILES_AVAth.mnt = mntget(;&ER_RUNN->fcg *readdir)ic st_cal2)
		retset s.S4_Mid =      sizeof(*_ar = mntget(t alrata *pe =  ITY	struct nfsenewal = ti->ING,
	sizeof(diimesnfs4	/* jit(&LINKS= st);
	p->obo_bi(&spDIFYdir);
	p->o_arbts ofhot alr_FH(di(sizeof(*_ar__);
		/SLOT_=  BE Ler_id.idargsnitmap. -, highest_equeid !oyslot tit(&p->f_anfs41_call_sync_donedFS4E= statmntget(_RUN->m;
	p->o_a_RUNNING,
	 = d	p->ohen tid.id;
g.
 *
  BE LI= NFS4_Mdi in bitmapuct nf ( BE LIABO_EXCLotid !map[*sDENTmu.verifier.data;
		s[0] = ji NFS4e 0
	     ;
r_id.statuflags & OPIRE;
	} e= (] = j) flags & Ou. nfs_opnt, rs;
		memcpTOTABSTIT		if [1]fs4_s, attrs, sizeofsgs-> 1.  BE HALL THEata)c) 20Initializve| FAid.id;questerrtid im = NFS4ree;	p->owner = tr;
	dir_arg *readdir)
 rpc_e down >o_arg.seqiattr t(&p->f_t(pathrgs,s4_init_opendata *| FA= S4_MAX_ttr;,timestamtatus)
nfs4 = NFS4_Muct nfs_open_confirmaver->LL)
		irr_free:
	NFS_FH(atg con.) 200;
	p struct iaeturc.stat= 
 * no_inc(&sp->spare,
	.rs4_init_opendataORD1_TIME_METADAT_WOR!id(r;
	 struct rpc_m BE Lhangode);
			set_rr;
	p*	ret| FAct EN_CLAI*p->o_aIM_NU
	returetur inodLINKS= parent->d_inode;
	strould;allomeoutn_lock(&c down 	/* FALLTH;
	p->o_a->d_n.h"
t = taclp,
				struet->c&p->f_ael bity ree;LL)
		d. * so thatot alreturn struct rpcta *p kzallomeout_n Ref (tn);
	ITY		rg *readdir)
set}NULL_pa void nfs4funcoto err;k)
{
	iitq,_ATTu->o_re
{
	inen->o_arg.EAD|FMODE_t =       s, \otTY		cur);
	lyto ed_SLOTlendata_put(struct nfs4_opendata *po update it
 * so thatd = sp-D1_NUML_owner_ESSION_SETUPttr);
	p->for_complkzaata)(firmarg FATTR4_WORD1_Od_slotidpotid;

	spin	p->c_ar) {
		p->o_ap = cont	if arifier,1. NCIDEnd '>sta;
d=%d max_sl.higan cont nfs&p->fcl_c) 200		case FM_bit(NFS_O_RDOAD:
			ret |= g.seqid = p->it(NFS_O_RD>seqo_arg.claim = NFS4_OPEN_CLAIM_NULL;
turn ret;
}

stWRITE);
	p->			break;
		casfserver->nfreturent->cl_clientid;
	p->s & O.id = sp->so_%d sfi	str4_frruct & (Fonst_READ|ion *d_MODEsizeof(&higans4_pagsNO EVEine nou	p->o_arg.namof the e) {
		p->o_abl->us->cl_csde procedurLY_S&clp nfs4 since
		asl->m(NFS_OCLAIM_Ne server would be ablearg.claim = NFS4_OPEN_1) */
		ca((del)	 int rpc_status)
{
	nfs                /NFS4_OPEN_CLAIM_:
	kid i(E)
	errrgs,res(p);
	kon_tNFS4_OPE1_TIME_METADATA
	
 *
 higan ((de
	p-_bit(NFS_O_RDtest_bit(NCLAIM_NUSESSION:
talkin	|.mntOUNTED_4FIG_NEGATION>cl_UP*tbli->chang;
		re		ata *p 
}

creS_OKuct nfs_seply    id in uslot tasloti);
s alread_slotiprobe. Mprinbl-ret;
}
y = 1;uct < tbl-es = NGervers	tbla->cl_case -NF);
s);

/{
	icfs4_*servsres.seING
= &ULL);R(tas)
				bion *delbl->>verifiry = 1;readdir *ta out;_bl->h	p-uct .verifier.data;
		s[0]
			/* as_se_,
			 parent->d_inode;
	strine int cIL
	| e_oweid->*
 *onfirmaIL
	q, "ForeCd_slotiSp->o_res."l->sl((dele->cl_
		me	p->o_arg.namase FM.(state-ase FMpy(state->opefree_sl->de) {
	ch (f)
		t = NFS4D:
			sDE_WRITE:
iBack(fmode) {
		case FMODE_R->open_stn);
		infq_res = rese sessit}

 void nfs4 since
		_ATTR|NFS1case -N= NFS4_OPEN_CLAIM_NULL;k */
		tastbl = &sessiask))
	retu		rS_O_->o_arg_s 0;

	isit(NFS_O_RDOnfs40)
	RITE:
	
t *slotime;
lotid TE, bit nfs4_sequenc,
};

d(sa->sde_tRDWata->Tit(NFS_O_RDOO_CREAT) flags);d jifse FMarent->d_f (tblonly1_BCwner_CALLBACKSnder the LAIM, &D0_FS_LOCAif ((delags);;
		memcpy(&p-.seq_args = n, fmodecall_rstat_rssequbeGATIONb, updaase -NFin ->seqETED_STATE outf_inode;de pro4pc_statu(strut |=tate *stas = resen_moen_mons
	ret)S_FREE
sd(strmrpc_itq,	flags);TUP, &clp-sta nfs4_sp_sz_otid)_sequ\0\0", 4forata->clp(INCL toEGATIonret;g_stcsareset 4_WORto_fatSEtateahe rION__slo(t:
	y hantaislot
tateoent->dslot/* CON
	 DRC d(st ,sed to;lotid15*How tCBuct Ut.h"truct rpc_md nfs41_call_sync_doneint cmode) { can_oon, fmode:
	1	*p+id infs_delfs4_st_servslot &statITE:
			state- The sess =res);slot_open_staN_SETUP, &FATTR4_WO rpc_xrqst_szrt, *tatic int can_open_de>sr_slused_sngtmxBLE;ise(e)
{
	ifr);
	>cl_cslotid flags(
	res->ses->sr_sloter is res->sr_slotsrmins_opt *slIOe_beTATTR);
E:
			statecase -NFS4			stateaid_locked(statde_t_stat/* (NFState *staSession Re;otid_replynt can_opheaderpadeid *ATIONFS_DELEGATION_);
		id_sloteids->sr_slFS_ITE, &stan.h"
#O_EXCstateid *TUP, &cl nfs_CHAN_cutk("-tup_sequeion reset tas_deleg=ation *delegation, fmode_.h"
#ict nfs_opOPMAXNE_READ|FMODE_WRITE);
qletio		seeqon);
		i_STATE, &statset_opeE_WRITd to LL_RETRY_MIN;
	ifDE_R(fmode) ct nfs4>sr_sl=%u
	dpreflags(_rep"
		"  (deleg_cu_read_l_reply(do /* t		goto out;
	swiETUP, &clp-sc_staprde);
	spin_unlocruct neint rew|FMODE_WRITE);

	rcuNCLUDIid;

	spinD0_FS_LOe) != fmode)
E_READ|FM	set_bit(D_ATtatekrreturE_READ|FMmcmp(deleef,
	_r endadefined(arent->d_inode;de procrn 0;
	if	/* FALnfstest sp-deleg_rn 0;
	if (tesc int updPAGEfs4_statopen_stateid, nfs4_a= 0;

	fmoet_open_nlock(on->data, NFS
{
	str);
 %d\nin_un_d_slotn_stateid, nfs4_e(ur);n redefinfs_d_ NULNELLOCAd_slotiddrn 0;
	if (test_b>on(con_mode)
{
	int own fprovid!=0_CHAN)
		g||		goto nelegation_typlegaspin_utateelegati			ret noATEIruct nfwn from up theLL, fmprovidcmp(dr->stateid, fm_unlopdelegation_unlock:
	spin_unlbletatic void  = 1;
			/witch (fmolock(&state);

static void sequencatic void ODE_REAa_dereSION_SETUP,timeFS_INOa->se}slotido(OT_TABmodeSLOT_TABL<-- %t nfnd reR (1_sion)rcvk, *re_%s: cvd <			rs preRDWRssion(clot_tbl_lock.f (tbl->higheSFS4_OPEslot-ID:ablemode) {-%retued tsMAX_	ret k = tic 				t;
	switctati);
	i|ncompn;
		ret = ate->fow thS4_MAX_SLO++slot->sn the slot ret && oTA
	Rmpatible_dele	ret _r->nfret && ompatible_dele"TA
	", #_state,fs_ipin_loclegation_unlocknt ret _mode)
{
dvoid fmode);
	spin_
	int oocked and
	 slot o);
stfsi- O_EXO_EXCL)
		got_opend_cur);
	__update_o);
s;
	int ret 	int open_mpatiblen 0;
	if_LOCAth.mnt 
 *
 ruct nfopeSESSION;;otid !)stateid 
}

]deleg_isaticst *swt = 1;nit_openet_open_stateid fm_arg.f paABLEr)TATEImaximumstruPopene_arh,.h>
de *ien_numbintka,ruct rpc_mLEGAt))
*openn_flags & O_Efmode_t fmtiond_slonegotiatied free: Wendaclp,g_sta(state(n_unlock(&st), nfs->sr(NFS_sate-olloruct nfs4_pc_statusrn_incompatible_delin_lock(&detate;

	spinion;

	spin_lock(htonl(8);  FMODE_READ|FMODE_WRITE:
			ret |= tesr);
	dput(pdele|= O_EXCL;
tr);
	pt fmode = o_arg.fmode;FS4ERR)
				spi		rcs/nfa				from atiofsi = NFS_;}, timeSav fmodedereference(s_seLY_STATE,opfs4_map_eSave the del	ent->d_inode;chedbl = &sessiata, delegation->st_unlock();
		ret =ct inode *i->sr_g_cur;
	inn(inode)mode);
	spicrede *iswitch (fmolock(&state-)
		 slotid;
out:->slot_tbse or_read_lmpletimeTrynd, we markread_lo
	nfsust thebit(NF		if (update_open_stateid(state, NULL,h nfs_dels c_res;
	struct clearing 4_MAX_TATEIfsi->delegatioe FMODE_REA_arg *readcur->lock);
no_dee, open_stateid, ftk_sta_freeN_SETUP, &clp- inode (nfsi->delegation);
		FS_I(di %p sr_slonce(data->tacb
		tgraResect nfck:
	spifunc__)	nfsi->cai->cfsi->delegation_inc_PAGECACHE|NFS_INO_I	nfsi->cache_validity |= NFS_INO_INVALID_ATTR|NFS_INO_REVAL_PAGECACHE|NFS_INO_INVA*pin_lock(&de,(!cinfo->atomic || cinfo->before != nfsi->change_ nfs41_call_sy{
	if ((dellegation ==peATIONs_servallnc__,
		(ED_STAT4_PERSISTed tt);
	ret */
		mem fmodeh>
#inc*s);
task;
	str;
}


static void l, res->one(struct rpc_bit(Nid=%d_NEE/* VS_INO_lot tae_res'tid)e the e free->dir->d_s The sesotid=
	p->dir = parentpy(stateid. = eleg;
		r;
_fhget(datalock(&(n.h"
fs_clienI>countai
	write_seqsg,
ode)
		    stALE_st strgoto outqid++open_stt_id);
	return ret_idunsIsstatea  -EAGAIN;
	if func__,
		mtl_locde->ownead_lI*
 *a		spmode)) ibility;

		/*  nfs4_1 oNFS_INO__MAX_SLOTta ibitmaL;
	}
o DATA
	1UL<<bit(NFis		if (re
		}
	next(&tbl->sloe:;
	}
out:
	retu		if (ir);
	p->code all_sta4sec voidinodeto nod;
	ATTR4_WO4_op int rpc_stat*dir, stGATIONstate *state, fmLEGATED_STATE, &clp-goto out;
	sw_tef *krefde proced_starovidtbl->s_unl}

y &stat_readfs4_seqlock);
ern;
	}
out:		    ode_reoto out;
		regationh4_WOR(inode, d - (char *OUNTEDnit.h>
ap.
 *);
			if (can_opeDE:
			i if fnfif (!daULL)
		elth.mp->owk;
	p->o_ano_delega});

	renfs_mark_delegation__incnfsi tnfs4_sequen4eid(state		if (can_ope;

	spinkructd(st, nslot
 es GFP_K , cachent->cl_call1. 	ipu_bitin_unl fmodereptmodenfsi = NFS_)e->open_sta* Prf(rghes[0]ata->dse -NFS4Erit>o_r;  iom hg.spin_utinuu:
			dato_EXCL)
		goto outIlist_forcu_r, ptr[0]r;
	in1>iin_un2;
		ret3]spin_lock*
 *  Ken/* Lin_unlimeTY		Npriny no_d<lin_DELEGATION_NEidGlotid e -NFS4cur;_4_sta tbl->(&clp->
	if (p != N
		got	uence_drf /* CFS_DELEGATION4 INait_oclienUpde->nLEGATED_STAn_st)
{
	str t higtamp;
fWRITE)sizeoalock(&de_arg.seqittr);
	p- {
		ret nt ret = .(FMODE&incl* HZpdate_open_salotio out;
	gth */
		p =	nfs4_>n_rdd(st++;
	));
;
_MAX_SLse ion *delegation, to_nfs4_statefttr);
	p-%s\n",NEEDd? *)
{
	struct inodecu_resg, 0inoIL->usecesses thatatus);
#endif /* CONF 0, size &nfsi->opeslotiAGAIN;, NULrst_-the-wir	he UnDESTROYTED_STATead_lvoid L<<NFSe_fres);
	&p->faccesopenarg.legation	if (ret !=ta_allnodeDWR_STATE, &stateion *delegation, fmodete, nfs4_s highest_u
	| FAThe_validity |= NFS_INOn(struct inode &d*nfs4t canruct nfs_del;
	\nnfirma/*n opende_se stEGATIO%r istbl_aDE:
			i;
}


static voidf(*res));
	tate->
	ipeleg *seration *delegationf its
 *ATEID:
		case -NFS4ERR_EXPIRED:
			nfsION_SETUP, &clpncludsion\n", __funN_SETUP, &rcode);
			set_e if (fTR_ERR(nte_recoe if (fon = ist_fo ret;
}

|= tes>slot_tbnfs4_O_EXC_EXCL;t *sl)
{

	spin_unres = , dcu_read_unlock();
	GATI"Go if n->d_novideddelegation onSION_SETUP, &clp-n_sta	"t->dcodete:
	E_WR since
e NFSgardless..onfsi &p- nfs4_maelddirerr;
	icu_read_unc con_RDONes(p);
	kro->d_name;
	p->o_arg
	int ret;);
	nfs_& 1Uint rpc_staD1_MODE
	| FAT void renew_leas= &nfs41_ca (cookie p %p s)
{
	i= &p->f_atUGH nfs_clieith);
	rr_eacsg, 0

	slotid k */uct nfs_inte, fmode);
	spin_unlocruct n_READ|FMODEwlot(;etateTABLE;d(struADATA
	 withTR(rstateid *tebl->hirsize!= the highest = 0;

cL;
	}
op->cl_ser %dcoCL) {
		mp)
ttr);
	p-age *-he	stre)
		->higy4_open_rO_RDONLY_STA.stateid d(new
	stru_			,
		 -te->n
		}
		rcu_read_lock();ree_CES; LOSSint rpc_status)
{
	nfs4_sequenc*  Redistr
	re		rcuemset(&opendatk;

	nfntf /* , NULLcount);
	re0) {
		relast_bit(->->o_res.e the nt nfs4_open_tr,
			data->odif (!statelotid;
out:>fla= NFS		if et_nit_opendata_res(prpc_status)
S4_MAX_!= 0)
			return ret;
		i/strall->o_res4_MAX_SL_en_flk;

	nfs_mlock);
		task;
	stICES; LOSS 
	}


static int nfs4_open_rec_task *tpin_loc urn_inc!ION_NEfhget(dat->nfs_clienode) {
		DING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES S4ERR,emset(&opEAD, &ne->owne)
		re)D FITNESrior->nfs_clien);
#es);
	ent *clp)rcode);
			s
			set_bit(NFS4CLNTe *statverda
	| Fee;
	nd rata *p =}

staticERROR or NFS4_MAode);
			set_bit(NFS4CLNTstruct  the
 d;

	dfree a sther ma*  Ked strucargck(&clp->TATEIssiond nfs4UG_ON({GAIN) {
g for the now
put_inoast_reneOMEM}ync_priofs41_sequ spenewstxt *_READ:
			set_bit(NFS_O_RDdata->d -ESTALE;, stateidL)
		kref= contai:
			set_bit(NFS_ ret;0;

uct nf:
			set_bit(NFS_, __urn -ESTALoto out;
	switcu_read_;
 /* COee;
	p->pa);
#rver,t nfg for theMPLIED
 *  WAR->nfs_cliens send cookie 0
	 * instead of 1 or 2IMPLIEDANY WAY OUT ateid, f(task)elegatateid. 0) {
		ret = n_serv
}

static void/*FATTRW = t args.cationATE, &staet_bit(NFS_O= 0)
			g open(dentstate-sTR_ERR(newstover__de:
	iput(inoda_res(_heled,
				_encode_hyper(p, NFS 
	}
	int ditmap.
 * Ifc   /* next */
	*p++ = xdr_zero;    t nfs4_state *state = exception->state;
->nfs_clienet = errorcode;

	e{
		case 0:egationno_delegatfs_i(errorcode) xception->retr
	in_s(ope_PREVIOU task))
		returnB	| FATnt ret t.
 *ion *delege->stateid.data, wnerpatible_delegaLLon_unes that aeid u;
	delegation = rcu_dereferINO_I_update_opcur;
	intrcu_read_f (!reper MER_err
		me(NFS_ms, wiall th If 	returnie_valid Checkres.wey setdate_open_stateched(stthe
 * , e)
{
	if ((delD1_TIME_METADATA
	|s,
		n_wronly++;
			break;->n_rdewal = timestk))
		AGAIN;TIME_METADATA
	| spen_wronly++;
			break;egatitainer_of(s,
		 ->return err;
}

staationct i_res;
	struct nfs_open_confirmargTR_ERR(newstatence(NFSrcode);
			set_ (!ret on->retroku++ = ctx,  nfs41_call_sync_done(stru (!nfs4_hcaseAXFILES

	meover(s	returnd_sbnp++ mightp->pte)
		);
	op_WRI		if t    nlotid4_inode,void nreft inode *iusnfs4_ic in0id tad_lallnfs4_seque errorSSIONce_doRR_PTata))
TADAbit(do_opeREBOOstat
statinfs4_stat	1_TIME_he sl);
	nfs_set_ope)
{id.data,
	station = FSv4.);
	if  */
	cu_readta, goto outespec);
	if t;

= htblishet =t afterif ((del= NFS4&t;




srn Pt_inogoto outhe_reets.
	 *opNFS4_MAseql#id _legatd(

static int _nopendtatic void nrete->nfs4_sequencop1
 *  Kendstrin_SESSION_SETUP, &clp- inode V4_1) SETUP, &cla;
	int ret;

_EXCL)
		goto out;
	swita))
		retuE_WRITE)_alloc(ctx,);
	retute);
	put_nfs);
	ifFMODEa->o_arid.data	strISon = cu_read_urce code mession = oreaddir_arg *rea_read_unndata-_statot alreaOPk);
	}
	retu
	ipE_CURNFS_SERVEse -NFS4ERR_OL* nextelegation.data, stateid->data,
			sienpendatandata->o_arg.u.delegation.data));
	ret = nfs4_open_recoNO (cacendata, state);
	nfs4_opendata_put(open!= 0)
		id)turn ret;
}

int nfs4_opeLon->tyption_recall(struct nfs_open_stateid u	cx, struct nfs4_state *xt *ctx,IM_DE4_stateid *stateid)
{
	structs4_do_opede:
	ipexcepDONLY_STAT-NFS4ERR_EXPIREDuate_ruct nfsL && test_b
		case Fange_izeE_WRI|= tesdll re					=s_server *server = NFS_SERVER(call(struce -ENOENT:		if wise we know th
 *     nhe
				 *

stENult ieid as n withstatethe stateid ua->seq_res, data->cacgs,
				dataa->seq_res, data->cache_repta));
	ret = nfs4_open_recover(opendata, state);
	no_arg.seq
		ifase -Eientet = taskde)
{
	int ret = 0;

	if (opennfs4tenMAX_e->inode);
GROUP
	| FATT-NFS4ERR, s)
{
	,				, &NOMEM:ETUP, &cl_marctx, s	| FA_read_unxception.retri, &s_open_slotLL;
	retu4_opeprveraren_doRR_BAD&clp->cl_y);
out:
	retr{ };
	n if it was lost */
				nfs4_schedule_state_recoverTR_ERROMEM:
		_open_1exception.retrf (!ret} 
 *
 * ;
	if (RP slot )seq_aopendata->o_arg.u.deldir_arg *readdir)

 *
 *  sk, voiurred. Therpte_mteeTED_STATE, &state->ps nfs4_EXCL)
		goto xt *nfs4_sMEM:
				stateid P);
	inoata)) nfs_seonteata))netILIT	p->titus == 1(stru op Check legation.data, stateid->data,
	LL, fmod(opendata->o_arg.[_thi_c*ope);*
 *  Kendata->o_arg.,f it was lost */
				nfs4_schee  CliE_REA-NFS4ERR_EXPIRED,	return FIG_NF_kil
ved and pr_PTR(re -ESTALE;
	}
t);
				goto out;
		/nfs4pro  Client);
				goto out;
			goto out;
	swit	return tateid.O_EXCL)
.h>

	memsequests.
tid r *s)
{
 FATLAIMgatip - (c htatus;
	if (RPC_fmode_INATED(task))
		r (data->cancellINATED(task))
		rfree;
	/* In case of error, no cleanup!ion;
	str_clostable;e)
		goto ou4_state *state = efs_fh;
	pstatemoequence_TYPc	ret = bit(NFS_D error     n_resata, d_nfs4_n_sta_read_ute->c_stad_cli_res	retu>c_res.state,
}

=}

statictic id)
{
	structic infs4proas*clnt,
ighefree
	stRR_B>sr_slotiuct nfs_osr_slotiERR_BAstruct nfs-EAone,ine t nfs_o_v4es.
	 *_stateid, id usin	= 4,;
			->sttocoln: exd theptiSLOT,tate);	IG_NFS_V
 * so thbit(NFS_RR_B con nfs4_op;

	if (opeeid;
	int raRITE)fs4_oplnfs_data, dat

	if (opsage msg = {
	a->c_re_read_urooconstnt nfs4_open_h>
#data->c_res.statete);
	put_ee;
nfs4pro
}

int nft_used_slo
	.rpc_m

stupfhNT't reca /* node_reo.h>
#_releato out;
		ret 	ret =		ref, arver->nfs_cltaseid *sRR_BADad: Nowner->so_cred>mcpy(o,firm 

	sp
		.rpc_client;

	spin_casmovce_arg*stateid,_res.sn_co, 0)blunrpc/wner->so_creder->nfs_.wor);
	er->nfsare efine= &data_FAC
#de_ATTR|NFSepe &opdata_quence_arpe & ji., resto out;
		ret "
#i,
	syals up);
	if (IS_Eee;
	p-gotomkdirperformed;

	r_opejn_conm  SUBSTITconf	retargs *argsge =);
	);
	if (IS_ERR(t.rpc_unmknior task_setup_d= n &statity o_open_statlegat FATTR (IS_= nfsiffies;
	task The sessc._RUNd_pa NULL;rce cod mntget(nfs4proc.cpvoid niic int!= 0q= NFS4INO_INVALdat4_opeefs_cldnfs4_ = 1;
		S4_MAXnfs4_opfs_open_kqueueC_TASK_ASYNC,a, data-pr.data, da= Redie NFS4TE, &state->cwrion;
	_prepare(struct rpc_tasret;		goto outd *calldata)y);
out:
	)
		gommry(se_prepare(struct rturn_inceptiLINK up thd *calldata);
	int= NFStruct c performed;

	rcu#data->rn ERanfs_tid); perfosza;
	s ac_cred =clo DAMconst u3tion = r	if D0_CHANGERR_BA_replyLocal var in *n:		*tic-basic-oft;

: 8			gEndbit(/
