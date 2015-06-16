/*
 *  NSA Security-Enhanced Linux (SELinux) security module
 *
 *  This file contains the SELinux hook function implementations.
 *
 *  Authors:  Stephen Smalley, <sds@epoch.ncsc.mil>
 *	      Chris Vance, <cvance@nai.com>
 *	      Wayne Salamon, <wsalamon@nai.com>
 *	      James Morris <jmorris@redhat.com>
 *
 *  Copyright (C) 2001,2002 Networks Associates Technology, Inc.
 *  Copyright (C) 2003-2008 Red Hat, Inc., James Morris <jmorris@redhat.com>
 *					   Eric Paris <eparis@redhat.com>
 *  Copyright (C) 2004-2005 Trusted Computer Solutions, Inc.
 *			    <dgoeddel@trustedcs.com>
 *  Copyright (C) 2006, 2007, 2009 Hewlett-Packard Development Company, L.P.
 *	Paul Moore <paul.moore@hp.com>
 *  Copyright (C) 2007 Hitachi Software Engineering Co., Ltd.
 *		       Yuichi Nakamura <ynakam@hitachisoft.jp>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License version 2,
 *	as published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/tracehook.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/security.h>
#include <linux/xattr.h>
#include <linux/capability.h>
#include <linux/unistd.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/slab.h>
#include <linux/pagemap.h>
#include <linux/swap.h>
#include <linux/spinlock.h>
#include <linux/syscalls.h>
#include <linux/file.h>
#include <linux/fdtable.h>
#include <linux/namei.h>
#include <linux/mount.h>
#include <linux/proc_fs.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_ipv6.h>
#include <linux/tty.h>
#include <net/icmp.h>
#include <net/ip.h>		/* for local_port_range[] */
#include <net/tcp.h>		/* struct or_callable used in sock_rcv_skb */
#include <net/net_namespace.h>
#include <net/netlabel.h>
#include <linux/uaccess.h>
#include <asm/ioctls.h>
#include <asm/atomic.h>
#include <linux/bitops.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>	/* for network interface checks */
#include <linux/netlink.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/dccp.h>
#include <linux/quota.h>
#include <linux/un.h>		/* for Unix socket types */
#include <net/af_unix.h>	/* for Unix socket types */
#include <linux/parser.h>
#include <linux/nfs_mount.h>
#include <net/ipv6.h>
#include <linux/hugetlb.h>
#include <linux/personality.h>
#include <linux/sysctl.h>
#include <linux/audit.h>
#include <linux/string.h>
#include <linux/selinux.h>
#include <linux/mutex.h>
#include <linux/posix-timers.h>

#include "avc.h"
#include "objsec.h"
#include "netif.h"
#include "netnode.h"
#include "netport.h"
#include "xfrm.h"
#include "netlabel.h"
#include "audit.h"

#define XATTR_SELINUX_SUFFIX "selinux"
#define XATTR_NAME_SELINUX XATTR_SECURITY_PREFIX XATTR_SELINUX_SUFFIX

#define NUM_SEL_MNT_OPTS 5

extern unsigned int policydb_loaded_version;
extern int selinux_nlmsg_lookup(u16 sclass, u16 nlmsg_type, u32 *perm);
extern struct security_operations *security_ops;

/* SECMARK reference count */
atomic_t selinux_secmark_refcount = ATOMIC_INIT(0);

#ifdef CONFIG_SECURITY_SELINUX_DEVELOP
int selinux_enforcing;

static int __init enforcing_setup(char *str)
{
	unsigned long enforcing;
	if (!strict_strtoul(str, 0, &enforcing))
		selinux_enforcing = enforcing ? 1 : 0;
	return 1;
}
__setup("enforcing=", enforcing_setup);
#endif

#ifdef CONFIG_SECURITY_SELINUX_BOOTPARAM
int selinux_enabled = CONFIG_SECURITY_SELINUX_BOOTPARAM_VALUE;

static int __init selinux_enabled_setup(char *str)
{
	unsigned long enabled;
	if (!strict_strtoul(str, 0, &enabled))
		selinux_enabled = enabled ? 1 : 0;
	return 1;
}
__setup("selinux=", selinux_enabled_setup);
#else
int selinux_enabled = 1;
#endif


/*
 * Minimal support for a secondary security module,
 * just to allow the use of the capability module.
 */
static struct security_operations *secondary_ops;

/* Lists of inode and superblock security structures initialized
   before the policy was loaded. */
static LIST_HEAD(superblock_security_head);
static DEFINE_SPINLOCK(sb_security_lock);

static struct kmem_cache *sel_inode_cache;

/**
 * selinux_secmark_enabled - Check to see if SECMARK is currently enabled
 *
 * Description:
 * This function checks the SECMARK reference counter to see if any SECMARK
 * targets are currently configured, if the reference counter is greater than
 * zero SECMARK is considered to be enabled.  Returns true (1) if SECMARK is
 * enabled, false (0) if SECMARK is disabled.
 *
 */
static int selinux_secmark_enabled(void)
{
	return (atomic_read(&selinux_secmark_refcount) > 0);
}

/*
 * initialise the security for the init task
 */
static void cred_init_security(void)
{
	struct cred *cred = (struct cred *) current->real_cred;
	struct task_security_struct *tsec;

	tsec = kzalloc(sizeof(struct task_security_struct), GFP_KERNEL);
	if (!tsec)
		panic("SELinux:  Failed to initialize initial task.\n");

	tsec->osid = tsec->sid = SECINITSID_KERNEL;
	cred->security = tsec;
}

/*
 * get the security ID of a set of credentials
 */
static inline u32 cred_sid(const struct cred *cred)
{
	const struct task_security_struct *tsec;

	tsec = cred->security;
	return tsec->sid;
}

/*
 * get the objective security ID of a task
 */
static inline u32 task_sid(const struct task_struct *task)
{
	u32 sid;

	rcu_read_lock();
	sid = cred_sid(__task_cred(task));
	rcu_read_unlock();
	return sid;
}

/*
 * get the subjective security ID of the current task
 */
static inline u32 current_sid(void)
{
	const struct task_security_struct *tsec = current_cred()->security;

	return tsec->sid;
}

/* Allocate and free functions for each kind of security blob. */

static int inode_alloc_security(struct inode *inode)
{
	struct inode_security_struct *isec;
	u32 sid = current_sid();

	isec = kmem_cache_zalloc(sel_inode_cache, GFP_NOFS);
	if (!isec)
		return -ENOMEM;

	mutex_init(&isec->lock);
	INIT_LIST_HEAD(&isec->list);
	isec->inode = inode;
	isec->sid = SECINITSID_UNLABELED;
	isec->sclass = SECCLASS_FILE;
	isec->task_sid = sid;
	inode->i_security = isec;

	return 0;
}

static void inode_free_security(struct inode *inode)
{
	struct inode_security_struct *isec = inode->i_security;
	struct superblock_security_struct *sbsec = inode->i_sb->s_security;

	spin_lock(&sbsec->isec_lock);
	if (!list_empty(&isec->list))
		list_del_init(&isec->list);
	spin_unlock(&sbsec->isec_lock);

	inode->i_security = NULL;
	kmem_cache_free(sel_inode_cache, isec);
}

static int file_alloc_security(struct file *file)
{
	struct file_security_struct *fsec;
	u32 sid = current_sid();

	fsec = kzalloc(sizeof(struct file_security_struct), GFP_KERNEL);
	if (!fsec)
		return -ENOMEM;

	fsec->sid = sid;
	fsec->fown_sid = sid;
	file->f_security = fsec;

	return 0;
}

static void file_free_security(struct file *file)
{
	struct file_security_struct *fsec = file->f_security;
	file->f_security = NULL;
	kfree(fsec);
}

static int superblock_alloc_security(struct super_block *sb)
{
	struct superblock_security_struct *sbsec;

	sbsec = kzalloc(sizeof(struct superblock_security_struct), GFP_KERNEL);
	if (!sbsec)
		return -ENOMEM;

	mutex_init(&sbsec->lock);
	INIT_LIST_HEAD(&sbsec->list);
	INIT_LIST_HEAD(&sbsec->isec_head);
	spin_lock_init(&sbsec->isec_lock);
	sbsec->sb = sb;
	sbsec->sid = SECINITSID_UNLABELED;
	sbsec->def_sid = SECINITSID_FILE;
	sbsec->mntpoint_sid = SECINITSID_UNLABELED;
	sb->s_security = sbsec;

	return 0;
}

static void superblock_free_security(struct super_block *sb)
{
	struct superblock_security_struct *sbsec = sb->s_security;

	spin_lock(&sb_security_lock);
	if (!list_empty(&sbsec->list))
		list_del_init(&sbsec->list);
	spin_unlock(&sb_security_lock);

	sb->s_security = NULL;
	kfree(sbsec);
}

static int sk_alloc_security(struct sock *sk, int family, gfp_t priority)
{
	struct sk_security_struct *ssec;

	ssec = kzalloc(sizeof(*ssec), priority);
	if (!ssec)
		return -ENOMEM;

	ssec->peer_sid = SECINITSID_UNLABELED;
	ssec->sid = SECINITSID_UNLABELED;
	sk->sk_security = ssec;

	selinux_netlbl_sk_security_reset(ssec);

	return 0;
}

static void sk_free_security(struct sock *sk)
{
	struct sk_security_struct *ssec = sk->sk_security;

	sk->sk_security = NULL;
	selinux_netlbl_sk_security_free(ssec);
	kfree(ssec);
}

/* The security server must be initialized before
   any labeling or access decisions can be provided. */
extern int ss_initialized;

/* The file system's label must be initialized prior to use. */

static char *labeling_behaviors[6] = {
	"uses xattr",
	"uses transition SIDs",
	"uses task SIDs",
	"uses genfs_contexts",
	"not configured for labeling",
	"uses mountpoint labeling",
};

static int inode_doinit_with_dentry(struct inode *inode, struct dentry *opt_dentry);

static inline int inode_doinit(struct inode *inode)
{
	return inode_doinit_with_dentry(inode, NULL);
}

enum {
	Opt_error = -1,
	Opt_context = 1,
	Opt_fscontext = 2,
	Opt_defcontext = 3,
	Opt_rootcontext = 4,
	Opt_labelsupport = 5,
};

static const match_table_t tokens = {
	{Opt_context, CONTEXT_STR "%s"},
	{Opt_fscontext, FSCONTEXT_STR "%s"},
	{Opt_defcontext, DEFCONTEXT_STR "%s"},
	{Opt_rootcontext, ROOTCONTEXT_STR "%s"},
	{Opt_labelsupport, LABELSUPP_STR},
	{Opt_error, NULL},
};

#define SEL_MOUNT_FAIL_MSG "SELinux:  duplicate or incompatible mount options\n"

static int may_context_mount_sb_relabel(u32 sid,
			struct superblock_security_struct *sbsec,
			const struct cred *cred)
{
	const struct task_security_struct *tsec = cred->security;
	int rc;

	rc = avc_has_perm(tsec->sid, sbsec->sid, SECCLASS_FILESYSTEM,
			  FILESYSTEM__RELABELFROM, NULL);
	if (rc)
		return rc;

	rc = avc_has_perm(tsec->sid, sid, SECCLASS_FILESYSTEM,
			  FILESYSTEM__RELABELTO, NULL);
	return rc;
}

static int may_context_mount_inode_relabel(u32 sid,
			struct superblock_security_struct *sbsec,
			const struct cred *cred)
{
	const struct task_security_struct *tsec = cred->security;
	int rc;
	rc = avc_has_perm(tsec->sid, sbsec->sid, SECCLASS_FILESYSTEM,
			  FILESYSTEM__RELABELFROM, NULL);
	if (rc)
		return rc;

	rc = avc_has_perm(sid, sbsec->sid, SECCLASS_FILESYSTEM,
			  FILESYSTEM__ASSOCIATE, NULL);
	return rc;
}

static int sb_finish_set_opts(struct super_block *sb)
{
	struct superblock_security_struct *sbsec = sb->s_security;
	struct dentry *root = sb->s_root;
	struct inode *root_inode = root->d_inode;
	int rc = 0;

	if (sbsec->behavior == SECURITY_FS_USE_XATTR) {
		/* Make sure that the xattr handler exists and that no
		   error other than -ENODATA is returned by getxattr on
		   the root directory.  -ENODATA is ok, as this may be
		   the first boot of the SELinux kernel before we have
		   assigned xattr values to the filesystem. */
		if (!root_inode->i_op->getxattr) {
			printk(KERN_WARNING "SELinux: (dev %s, type %s) has no "
			       "xattr support\n", sb->s_id, sb->s_type->name);
			rc = -EOPNOTSUPP;
			goto out;
		}
		rc = root_inode->i_op->getxattr(root, XATTR_NAME_SELINUX, NULL, 0);
		if (rc < 0 && rc != -ENODATA) {
			if (rc == -EOPNOTSUPP)
				printk(KERN_WARNING "SELinux: (dev %s, type "
				       "%s) has no security xattr handler\n",
				       sb->s_id, sb->s_type->name);
			else
				printk(KERN_WARNING "SELinux: (dev %s, type "
				       "%s) getxattr errno %d\n", sb->s_id,
				       sb->s_type->name, -rc);
			goto out;
		}
	}

	sbsec->flags |= (SE_SBINITIALIZED | SE_SBLABELSUPP);

	if (sbsec->behavior > ARRAY_SIZE(labeling_behaviors))
		printk(KERN_ERR "SELinux: initialized (dev %s, type %s), unknown behavior\n",
		       sb->s_id, sb->s_type->name);
	else
		printk(KERN_DEBUG "SELinux: initialized (dev %s, type %s), %s\n",
		       sb->s_id, sb->s_type->name,
		       labeling_behaviors[sbsec->behavior-1]);

	if (sbsec->behavior == SECURITY_FS_USE_GENFS ||
	    sbsec->behavior == SECURITY_FS_USE_MNTPOINT ||
	    sbsec->behavior == SECURITY_FS_USE_NONE ||
	    sbsec->behavior > ARRAY_SIZE(labeling_behaviors))
		sbsec->flags &= ~SE_SBLABELSUPP;

	/* Special handling for sysfs. Is genfs but also has setxattr handler*/
	if (strncmp(sb->s_type->name, "sysfs", sizeof("sysfs")) == 0)
		sbsec->flags |= SE_SBLABELSUPP;

	/* Initialize the root inode. */
	rc = inode_doinit_with_dentry(root_inode, root);

	/* Initialize any other inodes associated with the superblock, e.g.
	   inodes created prior to initial policy load or inodes created
	   during get_sb by a pseudo filesystem that directly
	   populates itself. */
	spin_lock(&sbsec->isec_lock);
next_inode:
	if (!list_empty(&sbsec->isec_head)) {
		struct inode_security_struct *isec =
				list_entry(sbsec->isec_head.next,
					   struct inode_security_struct, list);
		struct inode *inode = isec->inode;
		spin_unlock(&sbsec->isec_lock);
		inode = igrab(inode);
		if (inode) {
			if (!IS_PRIVATE(inode))
				inode_doinit(inode);
			iput(inode);
		}
		spin_lock(&sbsec->isec_lock);
		list_del_init(&isec->list);
		goto next_inode;
	}
	spin_unlock(&sbsec->isec_lock);
out:
	return rc;
}

/*
 * This function should allow an FS to ask what it's mount security
 * options were so it can use those later for submounts, displaying
 * mount options, or whatever.
 */
static int selinux_get_mnt_opts(const struct super_block *sb,
				struct security_mnt_opts *opts)
{
	int rc = 0, i;
	struct superblock_security_struct *sbsec = sb->s_security;
	char *context = NULL;
	u32 len;
	char tmp;

	security_init_mnt_opts(opts);

	if (!(sbsec->flags & SE_SBINITIALIZED))
		return -EINVAL;

	if (!ss_initialized)
		return -EINVAL;

	tmp = sbsec->flags & SE_MNTMASK;
	/* count the number of mount options for this sb */
	for (i = 0; i < 8; i++) {
		if (tmp & 0x01)
			opts->num_mnt_opts++;
		tmp >>= 1;
	}
	/* Check if the Label support flag is set */
	if (sbsec->flags & SE_SBLABELSUPP)
		opts->num_mnt_opts++;

	opts->mnt_opts = kcalloc(opts->num_mnt_opts, sizeof(char *), GFP_ATOMIC);
	if (!opts->mnt_opts) {
		rc = -ENOMEM;
		goto out_free;
	}

	opts->mnt_opts_flags = kcalloc(opts->num_mnt_opts, sizeof(int), GFP_ATOMIC);
	if (!opts->mnt_opts_flags) {
		rc = -ENOMEM;
		goto out_free;
	}

	i = 0;
	if (sbsec->flags & FSCONTEXT_MNT) {
		rc = security_sid_to_context(sbsec->sid, &context, &len);
		if (rc)
			goto out_free;
		opts->mnt_opts[i] = context;
		opts->mnt_opts_flags[i++] = FSCONTEXT_MNT;
	}
	if (sbsec->flags & CONTEXT_MNT) {
		rc = security_sid_to_context(sbsec->mntpoint_sid, &context, &len);
		if (rc)
			goto out_free;
		opts->mnt_opts[i] = context;
		opts->mnt_opts_flags[i++] = CONTEXT_MNT;
	}
	if (sbsec->flags & DEFCONTEXT_MNT) {
		rc = security_sid_to_context(sbsec->def_sid, &context, &len);
		if (rc)
			goto out_free;
		opts->mnt_opts[i] = context;
		opts->mnt_opts_flags[i++] = DEFCONTEXT_MNT;
	}
	if (sbsec->flags & ROOTCONTEXT_MNT) {
		struct inode *root = sbsec->sb->s_root->d_inode;
		struct inode_security_struct *isec = root->i_security;

		rc = security_sid_to_context(isec->sid, &context, &len);
		if (rc)
			goto out_free;
		opts->mnt_opts[i] = context;
		opts->mnt_opts_flags[i++] = ROOTCONTEXT_MNT;
	}
	if (sbsec->flags & SE_SBLABELSUPP) {
		opts->mnt_opts[i] = NULL;
		opts->mnt_opts_flags[i++] = SE_SBLABELSUPP;
	}

	BUG_ON(i != opts->num_mnt_opts);

	return 0;

out_free:
	security_free_mnt_opts(opts);
	return rc;
}

static int bad_option(struct superblock_security_struct *sbsec, char flag,
		      u32 old_sid, u32 new_sid)
{
	char mnt_flags = sbsec->flags & SE_MNTMASK;

	/* check if the old mount command had the same options */
	if (sbsec->flags & SE_SBINITIALIZED)
		if (!(sbsec->flags & flag) ||
		    (old_sid != new_sid))
			return 1;

	/* check if we were passed the same options twice,
	 * aka someone passed context=a,context=b
	 */
	if (!(sbsec->flags & SE_SBINITIALIZED))
		if (mnt_flags & flag)
			return 1;
	return 0;
}

/*
 * Allow filesystems with binary mount data to explicitly set mount point
 * labeling information.
 */
static int selinux_set_mnt_opts(struct super_block *sb,
				struct security_mnt_opts *opts)
{
	const struct cred *cred = current_cred();
	int rc = 0, i;
	struct superblock_security_struct *sbsec = sb->s_security;
	const char *name = sb->s_type->name;
	struct inode *inode = sbsec->sb->s_root->d_inode;
	struct inode_security_struct *root_isec = inode->i_security;
	u32 fscontext_sid = 0, context_sid = 0, rootcontext_sid = 0;
	u32 defcontext_sid = 0;
	char **mount_options = opts->mnt_opts;
	int *flags = opts->mnt_opts_flags;
	int num_opts = opts->num_mnt_opts;

	mutex_lock(&sbsec->lock);

	if (!ss_initialized) {
		if (!num_opts) {
			/* Defer initialization until selinux_complete_init,
			   after the initial policy is loaded and the security
			   server is ready to handle calls. */
			spin_lock(&sb_security_lock);
			if (list_empty(&sbsec->list))
				list_add(&sbsec->list, &superblock_security_head);
			spin_unlock(&sb_security_lock);
			goto out;
		}
		rc = -EINVAL;
		printk(KERN_WARNING "SELinux: Unable to set superblock options "
			"before the security server is initialized\n");
		goto out;
	}

	/*
	 * Binary mount data FS will come through this function twice.  Once
	 * from an explicit call and once from the generic calls from the vfs.
	 * Since the generic VFS calls will not contain any security mount data
	 * we need to skip the double mount verification.
	 *
	 * This does open a hole in which we will not notice if the first
	 * mount using this sb set explict options and a second mount using
	 * this sb does not set any security options.  (The first options
	 * will be used for both mounts)
	 */
	if ((sbsec->flags & SE_SBINITIALIZED) && (sb->s_type->fs_flags & FS_BINARY_MOUNTDATA)
	    && (num_opts == 0))
		goto out;

	/*
	 * parse the mount options, check if they are valid sids.
	 * also check if someone is trying to mount the same sb more
	 * than once with different security options.
	 */
	for (i = 0; i < num_opts; i++) {
		u32 sid;

		if (flags[i] == SE_SBLABELSUPP)
			continue;
		rc = security_context_to_sid(mount_options[i],
					     strlen(mount_options[i]), &sid);
		if (rc) {
			printk(KERN_WARNING "SELinux: security_context_to_sid"
			       "(%s) failed for (dev %s, type %s) errno=%d\n",
			       mount_options[i], sb->s_id, name, rc);
			goto out;
		}
		switch (flags[i]) {
		case FSCONTEXT_MNT:
			fscontext_sid = sid;

			if (bad_option(sbsec, FSCONTEXT_MNT, sbsec->sid,
					fscontext_sid))
				goto out_double_mount;

			sbsec->flags |= FSCONTEXT_MNT;
			break;
		case CONTEXT_MNT:
			context_sid = sid;

			if (bad_option(sbsec, CONTEXT_MNT, sbsec->mntpoint_sid,
					context_sid))
				goto out_double_mount;

			sbsec->flags |= CONTEXT_MNT;
			break;
		case ROOTCONTEXT_MNT:
			rootcontext_sid = sid;

			if (bad_option(sbsec, ROOTCONTEXT_MNT, root_isec->sid,
					rootcontext_sid))
				goto out_double_mount;

			sbsec->flags |= ROOTCONTEXT_MNT;

			break;
		case DEFCONTEXT_MNT:
			defcontext_sid = sid;

			if (bad_option(sbsec, DEFCONTEXT_MNT, sbsec->def_sid,
					defcontext_sid))
				goto out_double_mount;

			sbsec->flags |= DEFCONTEXT_MNT;

			break;
		default:
			rc = -EINVAL;
			goto out;
		}
	}

	if (sbsec->flags & SE_SBINITIALIZED) {
		/* previously mounted with options, but not on this attempt? */
		if ((sbsec->flags & SE_MNTMASK) && !num_opts)
			goto out_double_mount;
		rc = 0;
		goto out;
	}

	if (strcmp(sb->s_type->name, "proc") == 0)
		sbsec->flags |= SE_SBPROC;

	/* Determine the labeling behavior to use for this filesystem type. */
	rc = security_fs_use((sbsec->flags & SE_SBPROC) ? "proc" : sb->s_type->name, &sbsec->behavior, &sbsec->sid);
	if (rc) {
		printk(KERN_WARNING "%s: security_fs_use(%s) returned %d\n",
		       __func__, sb->s_type->name, rc);
		goto out;
	}

	/* sets the context of the superblock for the fs being mounted. */
	if (fscontext_sid) {
		rc = may_context_mount_sb_relabel(fscontext_sid, sbsec, cred);
		if (rc)
			goto out;

		sbsec->sid = fscontext_sid;
	}

	/*
	 * Switch to using mount point labeling behavior.
	 * sets the label used on all file below the mountpoint, and will set
	 * the superblock context if not already set.
	 */
	if (context_sid) {
		if (!fscontext_sid) {
			rc = may_context_mount_sb_relabel(context_sid, sbsec,
							  cred);
			if (rc)
				goto out;
			sbsec->sid = context_sid;
		} else {
			rc = may_context_mount_inode_relabel(context_sid, sbsec,
							     cred);
			if (rc)
				goto out;
		}
		if (!rootcontext_sid)
			rootcontext_sid = context_sid;

		sbsec->mntpoint_sid = context_sid;
		sbsec->behavior = SECURITY_FS_USE_MNTPOINT;
	}

	if (rootcontext_sid) {
		rc = may_context_mount_inode_relabel(rootcontext_sid, sbsec,
						     cred);
		if (rc)
			goto out;

		root_isec->sid = rootcontext_sid;
		root_isec->initialized = 1;
	}

	if (defcontext_sid) {
		if (sbsec->behavior != SECURITY_FS_USE_XATTR) {
			rc = -EINVAL;
			printk(KERN_WARNING "SELinux: defcontext option is "
			       "invalid for this filesystem type\n");
			goto out;
		}

		if (defcontext_sid != sbsec->def_sid) {
			rc = may_context_mount_inode_relabel(defcontext_sid,
							     sbsec, cred);
			if (rc)
				goto out;
		}

		sbsec->def_sid = defcontext_sid;
	}

	rc = sb_finish_set_opts(sb);
out:
	mutex_unlock(&sbsec->lock);
	return rc;
out_double_mount:
	rc = -EINVAL;
	printk(KERN_WARNING "SELinux: mount invalid.  Same superblock, different "
	       "security settings for (dev %s, type %s)\n", sb->s_id, name);
	goto out;
}

static void selinux_sb_clone_mnt_opts(const struct super_block *oldsb,
					struct super_block *newsb)
{
	const struct superblock_security_struct *oldsbsec = oldsb->s_security;
	struct superblock_security_struct *newsbsec = newsb->s_security;

	int set_fscontext =	(oldsbsec->flags & FSCONTEXT_MNT);
	int set_context =	(oldsbsec->flags & CONTEXT_MNT);
	int set_rootcontext =	(oldsbsec->flags & ROOTCONTEXT_MNT);

	/*
	 * if the parent was able to be mounted it clearly had no special lsm
	 * mount options.  thus we can safely put this sb on the list and deal
	 * with it later
	 */
	if (!ss_initialized) {
		spin_lock(&sb_security_lock);
		if (list_empty(&newsbsec->list))
			list_add(&newsbsec->list, &superblock_security_head);
		spin_unlock(&sb_security_lock);
		return;
	}

	/* how can we clone if the old one wasn't set up?? */
	BUG_ON(!(oldsbsec->flags & SE_SBINITIALIZED));

	/* if fs is reusing a sb, just let its options stand... */
	if (newsbsec->flags & SE_SBINITIALIZED)
		return;

	mutex_lock(&newsbsec->lock);

	newsbsec->flags = oldsbsec->flags;

	newsbsec->sid = oldsbsec->sid;
	newsbsec->def_sid = oldsbsec->def_sid;
	newsbsec->behavior = oldsbsec->behavior;

	if (set_context) {
		u32 sid = oldsbsec->mntpoint_sid;

		if (!set_fscontext)
			newsbsec->sid = sid;
		if (!set_rootcontext) {
			struct inode *newinode = newsb->s_root->d_inode;
			struct inode_security_struct *newisec = newinode->i_security;
			newisec->sid = sid;
		}
		newsbsec->mntpoint_sid = sid;
	}
	if (set_rootcontext) {
		const struct inode *oldinode = oldsb->s_root->d_inode;
		const struct inode_security_struct *oldisec = oldinode->i_security;
		struct inode *newinode = newsb->s_root->d_inode;
		struct inode_security_struct *newisec = newinode->i_security;

		newisec->sid = oldisec->sid;
	}

	sb_finish_set_opts(newsb);
	mutex_unlock(&newsbsec->lock);
}

static int selinux_parse_opts_str(char *options,
				  struct security_mnt_opts *opts)
{
	char *p;
	char *context = NULL, *defcontext = NULL;
	char *fscontext = NULL, *rootcontext = NULL;
	int rc, num_mnt_opts = 0;

	opts->num_mnt_opts = 0;

	/* Standard string-based options. */
	while ((p = strsep(&options, "|")) != NULL) {
		int token;
		substring_t args[MAX_OPT_ARGS];

		if (!*p)
			continue;

		token = match_token(p, tokens, args);

		switch (token) {
		case Opt_context:
			if (context || defcontext) {
				rc = -EINVAL;
				printk(KERN_WARNING SEL_MOUNT_FAIL_MSG);
				goto out_err;
			}
			context = match_strdup(&args[0]);
			if (!context) {
				rc = -ENOMEM;
				goto out_err;
			}
			break;

		case Opt_fscontext:
			if (fscontext) {
				rc = -EINVAL;
				printk(KERN_WARNING SEL_MOUNT_FAIL_MSG);
				goto out_err;
			}
			fscontext = match_strdup(&args[0]);
			if (!fscontext) {
				rc = -ENOMEM;
				goto out_err;
			}
			break;

		case Opt_rootcontext:
			if (rootcontext) {
				rc = -EINVAL;
				printk(KERN_WARNING SEL_MOUNT_FAIL_MSG);
				goto out_err;
			}
			rootcontext = match_strdup(&args[0]);
			if (!rootcontext) {
				rc = -ENOMEM;
				goto out_err;
			}
			break;

		case Opt_defcontext:
			if (context || defcontext) {
				rc = -EINVAL;
				printk(KERN_WARNING SEL_MOUNT_FAIL_MSG);
				goto out_err;
			}
			defcontext = match_strdup(&args[0]);
			if (!defcontext) {
				rc = -ENOMEM;
				goto out_err;
			}
			break;
		case Opt_labelsupport:
			break;
		default:
			rc = -EINVAL;
			printk(KERN_WARNING "SELinux:  unknown mount option\n");
			goto out_err;

		}
	}

	rc = -ENOMEM;
	opts->mnt_opts = kcalloc(NUM_SEL_MNT_OPTS, sizeof(char *), GFP_ATOMIC);
	if (!opts->mnt_opts)
		goto out_err;

	opts->mnt_opts_flags = kcalloc(NUM_SEL_MNT_OPTS, sizeof(int), GFP_ATOMIC);
	if (!opts->mnt_opts_flags) {
		kfree(opts->mnt_opts);
		goto out_err;
	}

	if (fscontext) {
		opts->mnt_opts[num_mnt_opts] = fscontext;
		opts->mnt_opts_flags[num_mnt_opts++] = FSCONTEXT_MNT;
	}
	if (context) {
		opts->mnt_opts[num_mnt_opts] = context;
		opts->mnt_opts_flags[num_mnt_opts++] = CONTEXT_MNT;
	}
	if (rootcontext) {
		opts->mnt_opts[num_mnt_opts] = rootcontext;
		opts->mnt_opts_flags[num_mnt_opts++] = ROOTCONTEXT_MNT;
	}
	if (defcontext) {
		opts->mnt_opts[num_mnt_opts] = defcontext;
		opts->mnt_opts_flags[num_mnt_opts++] = DEFCONTEXT_MNT;
	}

	opts->num_mnt_opts = num_mnt_opts;
	return 0;

out_err:
	kfree(context);
	kfree(defcontext);
	kfree(fscontext);
	kfree(rootcontext);
	return rc;
}
/*
 * string mount options parsing and call set the sbsec
 */
static int superblock_doinit(struct super_block *sb, void *data)
{
	int rc = 0;
	char *options = data;
	struct security_mnt_opts opts;

	security_init_mnt_opts(&opts);

	if (!data)
		goto out;

	BUG_ON(sb->s_type->fs_flags & FS_BINARY_MOUNTDATA);

	rc = selinux_parse_opts_str(options, &opts);
	if (rc)
		goto out_err;

out:
	rc = selinux_set_mnt_opts(sb, &opts);

out_err:
	security_free_mnt_opts(&opts);
	return rc;
}

static void selinux_write_opts(struct seq_file *m,
			       struct security_mnt_opts *opts)
{
	int i;
	char *prefix;

	for (i = 0; i < opts->num_mnt_opts; i++) {
		char *has_comma;

		if (opts->mnt_opts[i])
			has_comma = strchr(opts->mnt_opts[i], ',');
		else
			has_comma = NULL;

		switch (opts->mnt_opts_flags[i]) {
		case CONTEXT_MNT:
			prefix = CONTEXT_STR;
			break;
		case FSCONTEXT_MNT:
			prefix = FSCONTEXT_STR;
			break;
		case ROOTCONTEXT_MNT:
			prefix = ROOTCONTEXT_STR;
			break;
		case DEFCONTEXT_MNT:
			prefix = DEFCONTEXT_STR;
			break;
		case SE_SBLABELSUPP:
			seq_putc(m, ',');
			seq_puts(m, LABELSUPP_STR);
			continue;
		default:
			BUG();
		};
		/* we need a comma before each option */
		seq_putc(m, ',');
		seq_puts(m, prefix);
		if (has_comma)
			seq_putc(m, '\"');
		seq_puts(m, opts->mnt_opts[i]);
		if (has_comma)
			seq_putc(m, '\"');
	}
}

static int selinux_sb_show_options(struct seq_file *m, struct super_block *sb)
{
	struct security_mnt_opts opts;
	int rc;

	rc = selinux_get_mnt_opts(sb, &opts);
	if (rc) {
		/* before policy load we may get EINVAL, don't show anything */
		if (rc == -EINVAL)
			rc = 0;
		return rc;
	}

	selinux_write_opts(m, &opts);

	security_free_mnt_opts(&opts);

	return rc;
}

static inline u16 inode_mode_to_security_class(umode_t mode)
{
	switch (mode & S_IFMT) {
	case S_IFSOCK:
		return SECCLASS_SOCK_FILE;
	case S_IFLNK:
		return SECCLASS_LNK_FILE;
	case S_IFREG:
		return SECCLASS_FILE;
	case S_IFBLK:
		return SECCLASS_BLK_FILE;
	case S_IFDIR:
		return SECCLASS_DIR;
	case S_IFCHR:
		return SECCLASS_CHR_FILE;
	case S_IFIFO:
		return SECCLASS_FIFO_FILE;

	}

	return SECCLASS_FILE;
}

static inline int default_protocol_stream(int protocol)
{
	return (protocol == IPPROTO_IP || protocol == IPPROTO_TCP);
}

static inline int default_protocol_dgram(int protocol)
{
	return (protocol == IPPROTO_IP || protocol == IPPROTO_UDP);
}

static inline u16 socket_type_to_security_class(int family, int type, int protocol)
{
	switch (family) {
	case PF_UNIX:
		switch (type) {
		case SOCK_STREAM:
		case SOCK_SEQPACKET:
			return SECCLASS_UNIX_STREAM_SOCKET;
		case SOCK_DGRAM:
			return SECCLASS_UNIX_DGRAM_SOCKET;
		}
		break;
	case PF_INET:
	case PF_INET6:
		switch (type) {
		case SOCK_STREAM:
			if (default_protocol_stream(protocol))
				return SECCLASS_TCP_SOCKET;
			else
				return SECCLASS_RAWIP_SOCKET;
		case SOCK_DGRAM:
			if (default_protocol_dgram(protocol))
				return SECCLASS_UDP_SOCKET;
			else
				return SECCLASS_RAWIP_SOCKET;
		case SOCK_DCCP:
			return SECCLASS_DCCP_SOCKET;
		default:
			return SECCLASS_RAWIP_SOCKET;
		}
		break;
	case PF_NETLINK:
		switch (protocol) {
		case NETLINK_ROUTE:
			return SECCLASS_NETLINK_ROUTE_SOCKET;
		case NETLINK_FIREWALL:
			return SECCLASS_NETLINK_FIREWALL_SOCKET;
		case NETLINK_INET_DIAG:
			return SECCLASS_NETLINK_TCPDIAG_SOCKET;
		case NETLINK_NFLOG:
			return SECCLASS_NETLINK_NFLOG_SOCKET;
		case NETLINK_XFRM:
			return SECCLASS_NETLINK_XFRM_SOCKET;
		case NETLINK_SELINUX:
			return SECCLASS_NETLINK_SELINUX_SOCKET;
		case NETLINK_AUDIT:
			return SECCLASS_NETLINK_AUDIT_SOCKET;
		case NETLINK_IP6_FW:
			return SECCLASS_NETLINK_IP6FW_SOCKET;
		case NETLINK_DNRTMSG:
			return SECCLASS_NETLINK_DNRT_SOCKET;
		case NETLINK_KOBJECT_UEVENT:
			return SECCLASS_NETLINK_KOBJECT_UEVENT_SOCKET;
		default:
			return SECCLASS_NETLINK_SOCKET;
		}
	case PF_PACKET:
		return SECCLASS_PACKET_SOCKET;
	case PF_KEY:
		return SECCLASS_KEY_SOCKET;
	case PF_APPLETALK:
		return SECCLASS_APPLETALK_SOCKET;
	}

	return SECCLASS_SOCKET;
}

#ifdef CONFIG_PROC_FS
static int selinux_proc_get_sid(struct proc_dir_entry *de,
				u16 tclass,
				u32 *sid)
{
	int buflen, rc;
	char *buffer, *path, *end;

	buffer = (char *)__get_free_page(GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	buflen = PAGE_SIZE;
	end = buffer+buflen;
	*--end = '\0';
	buflen--;
	path = end-1;
	*path = '/';
	while (de && de != de->parent) {
		buflen -= de->namelen + 1;
		if (buflen < 0)
			break;
		end -= de->namelen;
		memcpy(end, de->name, de->namelen);
		*--end = '/';
		path = end;
		de = de->parent;
	}
	rc = security_genfs_sid("proc", path, tclass, sid);
	free_page((unsigned long)buffer);
	return rc;
}
#else
static int selinux_proc_get_sid(struct proc_dir_entry *de,
				u16 tclass,
				u32 *sid)
{
	return -EINVAL;
}
#endif

/* The inode's security attributes must be initialized before first use. */
static int inode_doinit_with_dentry(struct inode *inode, struct dentry *opt_dentry)
{
	struct superblock_security_struct *sbsec = NULL;
	struct inode_security_struct *isec = inode->i_security;
	u32 sid;
	struct dentry *dentry;
#define INITCONTEXTLEN 255
	char *context = NULL;
	unsigned len = 0;
	int rc = 0;

	if (isec->initialized)
		goto out;

	mutex_lock(&isec->lock);
	if (isec->initialized)
		goto out_unlock;

	sbsec = inode->i_sb->s_security;
	if (!(sbsec->flags & SE_SBINITIALIZED)) {
		/* Defer initialization until selinux_complete_init,
		   after the initial policy is loaded and the security
		   server is ready to handle calls. */
		spin_lock(&sbsec->isec_lock);
		if (list_empty(&isec->list))
			list_add(&isec->list, &sbsec->isec_head);
		spin_unlock(&sbsec->isec_lock);
		goto out_unlock;
	}

	switch (sbsec->behavior) {
	case SECURITY_FS_USE_XATTR:
		if (!inode->i_op->getxattr) {
			isec->sid = sbsec->def_sid;
			break;
		}

		/* Need a dentry, since the xattr API requires one.
		   Life would be simpler if we could just pass the inode. */
		if (opt_dentry) {
			/* Called from d_instantiate or d_splice_alias. */
			dentry = dget(opt_dentry);
		} else {
			/* Called from selinux_complete_init, try to find a dentry. */
			dentry = d_find_alias(inode);
		}
		if (!dentry) {
			/*
			 * this is can be hit on boot when a file is accessed
			 * before the policy is loaded.  When we load policy we
			 * may find inodes that have no dentry on the
			 * sbsec->isec_head list.  No reason to complain as these
			 * will get fixed up the next time we go through
			 * inode_doinit with a dentry, before these inodes could
			 * be used again by userspace.
			 */
			goto out_unlock;
		}

		len = INITCONTEXTLEN;
		context = kmalloc(len+1, GFP_NOFS);
		if (!context) {
			rc = -ENOMEM;
			dput(dentry);
			goto out_unlock;
		}
		context[len] = '\0';
		rc = inode->i_op->getxattr(dentry, XATTR_NAME_SELINUX,
					   context, len);
		if (rc == -ERANGE) {
			kfree(context);

			/* Need a larger buffer.  Query for the right size. */
			rc = inode->i_op->getxattr(dentry, XATTR_NAME_SELINUX,
						   NULL, 0);
			if (rc < 0) {
				dput(dentry);
				goto out_unlock;
			}
			len = rc;
			context = kmalloc(len+1, GFP_NOFS);
			if (!context) {
				rc = -ENOMEM;
				dput(dentry);
				goto out_unlock;
			}
			context[len] = '\0';
			rc = inode->i_op->getxattr(dentry,
						   XATTR_NAME_SELINUX,
						   context, len);
		}
		dput(dentry);
		if (rc < 0) {
			if (rc != -ENODATA) {
				printk(KERN_WARNING "SELinux: %s:  getxattr returned "
				       "%d for dev=%s ino=%ld\n", __func__,
				       -rc, inode->i_sb->s_id, inode->i_ino);
				kfree(context);
				goto out_unlock;
			}
			/* Map ENODATA to the default file SID */
			sid = sbsec->def_sid;
			rc = 0;
		} else {
			rc = security_context_to_sid_default(context, rc, &sid,
							     sbsec->def_sid,
							     GFP_NOFS);
			if (rc) {
				char *dev = inode->i_sb->s_id;
				unsigned long ino = inode->i_ino;

				if (rc == -EINVAL) {
					if (printk_ratelimit())
						printk(KERN_NOTICE "SELinux: inode=%lu on dev=%s was found to have an invalid "
							"context=%s.  This indicates you may need to relabel the inode or the "
							"filesystem in question.\n", ino, dev, context);
				} else {
					printk(KERN_WARNING "SELinux: %s:  context_to_sid(%s) "
					       "returned %d for dev=%s ino=%ld\n",
					       __func__, context, -rc, dev, ino);
				}
				kfree(context);
				/* Leave with the unlabeled SID */
				rc = 0;
				break;
			}
		}
		kfree(context);
		isec->sid = sid;
		break;
	case SECURITY_FS_USE_TASK:
		isec->sid = isec->task_sid;
		break;
	case SECURITY_FS_USE_TRANS:
		/* Default to the fs SID. */
		isec->sid = sbsec->sid;

		/* Try to obtain a transition SID. */
		isec->sclass = inode_mode_to_security_class(inode->i_mode);
		rc = security_transition_sid(isec->task_sid,
					     sbsec->sid,
					     isec->sclass,
					     &sid);
		if (rc)
			goto out_unlock;
		isec->sid = sid;
		break;
	case SECURITY_FS_USE_MNTPOINT:
		isec->sid = sbsec->mntpoint_sid;
		break;
	default:
		/* Default to the fs superblock SID. */
		isec->sid = sbsec->sid;

		if ((sbsec->flags & SE_SBPROC) && !S_ISLNK(inode->i_mode)) {
			struct proc_inode *proci = PROC_I(inode);
			if (proci->pde) {
				isec->sclass = inode_mode_to_security_class(inode->i_mode);
				rc = selinux_proc_get_sid(proci->pde,
							  isec->sclass,
							  &sid);
				if (rc)
					goto out_unlock;
				isec->sid = sid;
			}
		}
		break;
	}

	isec->initialized = 1;

out_unlock:
	mutex_unlock(&isec->lock);
out:
	if (isec->sclass == SECCLASS_FILE)
		isec->sclass = inode_mode_to_security_class(inode->i_mode);
	return rc;
}

/* Convert a Linux signal to an access vector. */
static inline u32 signal_to_av(int sig)
{
	u32 perm = 0;

	switch (sig) {
	case SIGCHLD:
		/* Commonly granted from child to parent. */
		perm = PROCESS__SIGCHLD;
		break;
	case SIGKILL:
		/* Cannot be caught or ignored */
		perm = PROCESS__SIGKILL;
		break;
	case SIGSTOP:
		/* Cannot be caught or ignored */
		perm = PROCESS__SIGSTOP;
		break;
	default:
		/* All other signals. */
		perm = PROCESS__SIGNAL;
		break;
	}

	return perm;
}

/*
 * Check permission between a pair of credentials
 * fork check, ptrace check, etc.
 */
static int cred_has_perm(const struct cred *actor,
			 const struct cred *target,
			 u32 perms)
{
	u32 asid = cred_sid(actor), tsid = cred_sid(target);

	return avc_has_perm(asid, tsid, SECCLASS_PROCESS, perms, NULL);
}

/*
 * Check permission between a pair of tasks, e.g. signal checks,
 * fork check, ptrace check, etc.
 * tsk1 is the actor and tsk2 is the target
 * - this uses the default subjective creds of tsk1
 */
static int task_has_perm(const struct task_struct *tsk1,
			 const struct task_struct *tsk2,
			 u32 perms)
{
	const struct task_security_struct *__tsec1, *__tsec2;
	u32 sid1, sid2;

	rcu_read_lock();
	__tsec1 = __task_cred(tsk1)->security;	sid1 = __tsec1->sid;
	__tsec2 = __task_cred(tsk2)->security;	sid2 = __tsec2->sid;
	rcu_read_unlock();
	return avc_has_perm(sid1, sid2, SECCLASS_PROCESS, perms, NULL);
}

/*
 * Check permission between current and another task, e.g. signal checks,
 * fork check, ptrace check, etc.
 * current is the actor and tsk2 is the target
 * - this uses current's subjective creds
 */
static int current_has_perm(const struct task_struct *tsk,
			    u32 perms)
{
	u32 sid, tsid;

	sid = current_sid();
	tsid = task_sid(tsk);
	return avc_has_perm(sid, tsid, SECCLASS_PROCESS, perms, NULL);
}

#if CAP_LAST_CAP > 63
#error Fix SELinux to handle capabilities > 63.
#endif

/* Check whether a task is allowed to use a capability. */
static int task_has_capability(struct task_struct *tsk,
			       const struct cred *cred,
			       int cap, int audit)
{
	struct common_audit_data ad;
	struct av_decision avd;
	u16 sclass;
	u32 sid = cred_sid(cred);
	u32 av = CAP_TO_MASK(cap);
	int rc;

	COMMON_AUDIT_DATA_INIT(&ad, CAP);
	ad.tsk = tsk;
	ad.u.cap = cap;

	switch (CAP_TO_INDEX(cap)) {
	case 0:
		sclass = SECCLASS_CAPABILITY;
		break;
	case 1:
		sclass = SECCLASS_CAPABILITY2;
		break;
	default:
		printk(KERN_ERR
		       "SELinux:  out of range capability %d\n", cap);
		BUG();
	}

	rc = avc_has_perm_noaudit(sid, sid, sclass, av, 0, &avd);
	if (audit == SECURITY_CAP_AUDIT)
		avc_audit(sid, sid, sclass, av, &avd, rc, &ad);
	return rc;
}

/* Check whether a task is allowed to use a system operation. */
static int task_has_system(struct task_struct *tsk,
			   u32 perms)
{
	u32 sid = task_sid(tsk);

	return avc_has_perm(sid, SECINITSID_KERNEL,
			    SECCLASS_SYSTEM, perms, NULL);
}

/* Check whether a task has a particular permission to an inode.
   The 'adp' parameter is optional and allows other audit
   data to be passed (e.g. the dentry). */
static int inode_has_perm(const struct cred *cred,
			  struct inode *inode,
			  u32 perms,
			  struct common_audit_data *adp)
{
	struct inode_security_struct *isec;
	struct common_audit_data ad;
	u32 sid;

	validate_creds(cred);

	if (unlikely(IS_PRIVATE(inode)))
		return 0;

	sid = cred_sid(cred);
	isec = inode->i_security;

	if (!adp) {
		adp = &ad;
		COMMON_AUDIT_DATA_INIT(&ad, FS);
		ad.u.fs.inode = inode;
	}

	return avc_has_perm(sid, isec->sid, isec->sclass, perms, adp);
}

/* Same as inode_has_perm, but pass explicit audit data containing
   the dentry to help the auditing code to more easily generate the
   pathname if needed. */
static inline int dentry_has_perm(const struct cred *cred,
				  struct vfsmount *mnt,
				  struct dentry *dentry,
				  u32 av)
{
	struct inode *inode = dentry->d_inode;
	struct common_audit_data ad;

	COMMON_AUDIT_DATA_INIT(&ad, FS);
	ad.u.fs.path.mnt = mnt;
	ad.u.fs.path.dentry = dentry;
	return inode_has_perm(cred, inode, av, &ad);
}

/* Check whether a task can use an open file descriptor to
   access an inode in a given way.  Check access to the
   descriptor itself, and then use dentry_has_perm to
   check a particular permission to the file.
   Access to the descriptor is implicitly granted if it
   has the same SID as the process.  If av is zero, then
   access to the file is not checked, e.g. for cases
   where only the descriptor is affected like seek. */
static int file_has_perm(const struct cred *cred,
			 struct file *file,
			 u32 av)
{
	struct file_security_struct *fsec = file->f_security;
	struct inode *inode = file->f_path.dentry->d_inode;
	struct common_audit_data ad;
	u32 sid = cred_sid(cred);
	int rc;

	COMMON_AUDIT_DATA_INIT(&ad, FS);
	ad.u.fs.path = file->f_path;

	if (sid != fsec->sid) {
		rc = avc_has_perm(sid, fsec->sid,
				  SECCLASS_FD,
				  FD__USE,
				  &ad);
		if (rc)
			goto out;
	}

	/* av is zero if only checking access to the descriptor. */
	rc = 0;
	if (av)
		rc = inode_has_perm(cred, inode, av, &ad);

out:
	return rc;
}

/* Check whether a task can create a file. */
static int may_create(struct inode *dir,
		      struct dentry *dentry,
		      u16 tclass)
{
	const struct cred *cred = current_cred();
	const struct task_security_struct *tsec = cred->security;
	struct inode_security_struct *dsec;
	struct superblock_security_struct *sbsec;
	u32 sid, newsid;
	struct common_audit_data ad;
	int rc;

	dsec = dir->i_security;
	sbsec = dir->i_sb->s_security;

	sid = tsec->sid;
	newsid = tsec->create_sid;

	COMMON_AUDIT_DATA_INIT(&ad, FS);
	ad.u.fs.path.dentry = dentry;

	rc = avc_has_perm(sid, dsec->sid, SECCLASS_DIR,
			  DIR__ADD_NAME | DIR__SEARCH,
			  &ad);
	if (rc)
		return rc;

	if (!newsid || !(sbsec->flags & SE_SBLABELSUPP)) {
		rc = security_transition_sid(sid, dsec->sid, tclass, &newsid);
		if (rc)
			return rc;
	}

	rc = avc_has_perm(sid, newsid, tclass, FILE__CREATE, &ad);
	if (rc)
		return rc;

	return avc_has_perm(newsid, sbsec->sid,
			    SECCLASS_FILESYSTEM,
			    FILESYSTEM__ASSOCIATE, &ad);
}

/* Check whether a task can create a key. */
static int may_create_key(u32 ksid,
			  struct task_struct *ctx)
{
	u32 sid = task_sid(ctx);

	return avc_has_perm(sid, ksid, SECCLASS_KEY, KEY__CREATE, NULL);
}

#define MAY_LINK	0
#define MAY_UNLINK	1
#define MAY_RMDIR	2

/* Check whether a task can link, unlink, or rmdir a file/directory. */
static int may_link(struct inode *dir,
		    struct dentry *dentry,
		    int kind)

{
	struct inode_security_struct *dsec, *isec;
	struct common_audit_data ad;
	u32 sid = current_sid();
	u32 av;
	int rc;

	dsec = dir->i_security;
	isec = dentry->d_inode->i_security;

	COMMON_AUDIT_DATA_INIT(&ad, FS);
	ad.u.fs.path.dentry = dentry;

	av = DIR__SEARCH;
	av |= (kind ? DIR__REMOVE_NAME : DIR__ADD_NAME);
	rc = avc_has_perm(sid, dsec->sid, SECCLASS_DIR, av, &ad);
	if (rc)
		return rc;

	switch (kind) {
	case MAY_LINK:
		av = FILE__LINK;
		break;
	case MAY_UNLINK:
		av = FILE__UNLINK;
		break;
	case MAY_RMDIR:
		av = DIR__RMDIR;
		break;
	default:
		printk(KERN_WARNING "SELinux: %s:  unrecognized kind %d\n",
			__func__, kind);
		return 0;
	}

	rc = avc_has_perm(sid, isec->sid, isec->sclass, av, &ad);
	return rc;
}

static inline int may_rename(struct inode *old_dir,
			     struct dentry *old_dentry,
			     struct inode *new_dir,
			     struct dentry *new_dentry)
{
	struct inode_security_struct *old_dsec, *new_dsec, *old_isec, *new_isec;
	struct common_audit_data ad;
	u32 sid = current_sid();
	u32 av;
	int old_is_dir, new_is_dir;
	int rc;

	old_dsec = old_dir->i_security;
	old_isec = old_dentry->d_inode->i_security;
	old_is_dir = S_ISDIR(old_dentry->d_inode->i_mode);
	new_dsec = new_dir->i_security;

	COMMON_AUDIT_DATA_INIT(&ad, FS);

	ad.u.fs.path.dentry = old_dentry;
	rc = avc_has_perm(sid, old_dsec->sid, SECCLASS_DIR,
			  DIR__REMOVE_NAME | DIR__SEARCH, &ad);
	if (rc)
		return rc;
	rc = avc_has_perm(sid, old_isec->sid,
			  old_isec->sclass, FILE__RENAME, &ad);
	if (rc)
		return rc;
	if (old_is_dir && new_dir != old_dir) {
		rc = avc_has_perm(sid, old_isec->sid,
				  old_isec->sclass, DIR__REPARENT, &ad);
		if (rc)
			return rc;
	}

	ad.u.fs.path.dentry = new_dentry;
	av = DIR__ADD_NAME | DIR__SEARCH;
	if (new_dentry->d_inode)
		av |= DIR__REMOVE_NAME;
	rc = avc_has_perm(sid, new_dsec->sid, SECCLASS_DIR, av, &ad);
	if (rc)
		return rc;
	if (new_dentry->d_inode) {
		new_isec = new_dentry->d_inode->i_security;
		new_is_dir = S_ISDIR(new_dentry->d_inode->i_mode);
		rc = avc_has_perm(sid, new_isec->sid,
				  new_isec->sclass,
				  (new_is_dir ? DIR__RMDIR : FILE__UNLINK), &ad);
		if (rc)
			return rc;
	}

	return 0;
}

/* Check whether a task can perform a filesystem operation. */
static int superblock_has_perm(const struct cred *cred,
			       struct super_block *sb,
			       u32 perms,
			       struct common_audit_data *ad)
{
	struct superblock_security_struct *sbsec;
	u32 sid = cred_sid(cred);

	sbsec = sb->s_security;
	return avc_has_perm(sid, sbsec->sid, SECCLASS_FILESYSTEM, perms, ad);
}

/* Convert a Linux mode and permission mask to an access vector. */
static inline u32 file_mask_to_av(int mode, int mask)
{
	u32 av = 0;

	if ((mode & S_IFMT) != S_IFDIR) {
		if (mask & MAY_EXEC)
			av |= FILE__EXECUTE;
		if (mask & MAY_READ)
			av |= FILE__READ;

		if (mask & MAY_APPEND)
			av |= FILE__APPEND;
		else if (mask & MAY_WRITE)
			av |= FILE__WRITE;

	} else {
		if (mask & MAY_EXEC)
			av |= DIR__SEARCH;
		if (mask & MAY_WRITE)
			av |= DIR__WRITE;
		if (mask & MAY_READ)
			av |= DIR__READ;
	}

	return av;
}

/* Convert a Linux file to an access vector. */
static inline u32 file_to_av(struct file *file)
{
	u32 av = 0;

	if (file->f_mode & FMODE_READ)
		av |= FILE__READ;
	if (file->f_mode & FMODE_WRITE) {
		if (file->f_flags & O_APPEND)
			av |= FILE__APPEND;
		else
			av |= FILE__WRITE;
	}
	if (!av) {
		/*
		 * Special file opened with flags 3 for ioctl-only use.
		 */
		av = FILE__IOCTL;
	}

	return av;
}

/*
 * Convert a file to an access vector and include the correct open
 * open permission.
 */
static inline u32 open_file_to_av(struct file *file)
{
	u32 av = file_to_av(file);

	if (selinux_policycap_openperm) {
		mode_t mode = file->f_path.dentry->d_inode->i_mode;
		/*
		 * lnk files and socks do not really have an 'open'
		 */
		if (S_ISREG(mode))
			av |= FILE__OPEN;
		else if (S_ISCHR(mode))
			av |= CHR_FILE__OPEN;
		else if (S_ISBLK(mode))
			av |= BLK_FILE__OPEN;
		else if (S_ISFIFO(mode))
			av |= FIFO_FILE__OPEN;
		else if (S_ISDIR(mode))
			av |= DIR__OPEN;
		else if (S_ISSOCK(mode))
			av |= SOCK_FILE__OPEN;
		else
			printk(KERN_ERR "SELinux: WARNING: inside %s with "
				"unknown mode:%o\n", __func__, mode);
	}
	return av;
}

/* Hook functions begin here. */

static int selinux_ptrace_access_check(struct task_struct *child,
				     unsigned int mode)
{
	int rc;

	rc = cap_ptrace_access_check(child, mode);
	if (rc)
		return rc;

	if (mode == PTRACE_MODE_READ) {
		u32 sid = current_sid();
		u32 csid = task_sid(child);
		return avc_has_perm(sid, csid, SECCLASS_FILE, FILE__READ, NULL);
	}

	return current_has_perm(child, PROCESS__PTRACE);
}

static int selinux_ptrace_traceme(struct task_struct *parent)
{
	int rc;

	rc = cap_ptrace_traceme(parent);
	if (rc)
		return rc;

	return task_has_perm(parent, current, PROCESS__PTRACE);
}

static int selinux_capget(struct task_struct *target, kernel_cap_t *effective,
			  kernel_cap_t *inheritable, kernel_cap_t *permitted)
{
	int error;

	error = current_has_perm(target, PROCESS__GETCAP);
	if (error)
		return error;

	return cap_capget(target, effective, inheritable, permitted);
}

static int selinux_capset(struct cred *new, const struct cred *old,
			  const kernel_cap_t *effective,
			  const kernel_cap_t *inheritable,
			  const kernel_cap_t *permitted)
{
	int error;

	error = cap_capset(new, old,
				      effective, inheritable, permitted);
	if (error)
		return error;

	return cred_has_perm(old, new, PROCESS__SETCAP);
}

/*
 * (This comment used to live with the selinux_task_setuid hook,
 * which was removed).
 *
 * Since setuid only affects the current process, and since the SELinux
 * controls are not based on the Linux identity attributes, SELinux does not
 * need to control this operation.  However, SELinux does control the use of
 * the CAP_SETUID and CAP_SETGID capabilities using the capable hook.
 */

static int selinux_capable(struct task_struct *tsk, const struct cred *cred,
			   int cap, int audit)
{
	int rc;

	rc = cap_capable(tsk, cred, cap, audit);
	if (rc)
		return rc;

	return task_has_capability(tsk, cred, cap, audit);
}

static int selinux_sysctl_get_sid(ctl_table *table, u16 tclass, u32 *sid)
{
	int buflen, rc;
	char *buffer, *path, *end;

	rc = -ENOMEM;
	buffer = (char *)__get_free_page(GFP_KERNEL);
	if (!buffer)
		goto out;

	buflen = PAGE_SIZE;
	end = buffer+buflen;
	*--end = '\0';
	buflen--;
	path = end-1;
	*path = '/';
	while (table) {
		const char *name = table->procname;
		size_t namelen = strlen(name);
		buflen -= namelen + 1;
		if (buflen < 0)
			goto out_free;
		end -= namelen;
		memcpy(end, name, namelen);
		*--end = '/';
		path = end;
		table = table->parent;
	}
	buflen -= 4;
	if (buflen < 0)
		goto out_free;
	end -= 4;
	memcpy(end, "/sys", 4);
	path = end;
	rc = security_genfs_sid("proc", path, tclass, sid);
out_free:
	free_page((unsigned long)buffer);
out:
	return rc;
}

static int selinux_sysctl(ctl_table *table, int op)
{
	int error = 0;
	u32 av;
	u32 tsid, sid;
	int rc;

	sid = current_sid();

	rc = selinux_sysctl_get_sid(table, (op == 0001) ?
				    SECCLASS_DIR : SECCLASS_FILE, &tsid);
	if (rc) {
		/* Default to the well-defined sysctl SID. */
		tsid = SECINITSID_SYSCTL;
	}

	/* The op values are "defined" in sysctl.c, thereby creating
	 * a bad coupling between this module and sysctl.c */
	if (op == 001) {
		error = avc_has_perm(sid, tsid,
				     SECCLASS_DIR, DIR__SEARCH, NULL);
	} else {
		av = 0;
		if (op & 004)
			av |= FILE__READ;
		if (op & 002)
			av |= FILE__WRITE;
		if (av)
			error = avc_has_perm(sid, tsid,
					     SECCLASS_FILE, av, NULL);
	}

	return error;
}

static int selinux_quotactl(int cmds, int type, int id, struct super_block *sb)
{
	const struct cred *cred = current_cred();
	int rc = 0;

	if (!sb)
		return 0;

	switch (cmds) {
	case Q_SYNC:
	case Q_QUOTAON:
	case Q_QUOTAOFF:
	case Q_SETINFO:
	case Q_SETQUOTA:
		rc = superblock_has_perm(cred, sb, FILESYSTEM__QUOTAMOD, NULL);
		break;
	case Q_GETFMT:
	case Q_GETINFO:
	case Q_GETQUOTA:
		rc = superblock_has_perm(cred, sb, FILESYSTEM__QUOTAGET, NULL);
		break;
	default:
		rc = 0;  /* let the kernel handle invalid cmds */
		break;
	}
	return rc;
}

static int selinux_quota_on(struct dentry *dentry)
{
	const struct cred *cred = current_cred();

	return dentry_has_perm(cred, NULL, dentry, FILE__QUOTAON);
}

static int selinux_syslog(int type)
{
	int rc;

	rc = cap_syslog(type);
	if (rc)
		return rc;

	switch (type) {
	case 3:		/* Read last kernel messages */
	case 10:	/* Return size of the log buffer */
		rc = task_has_system(current, SYSTEM__SYSLOG_READ);
		break;
	case 6:		/* Disable logging to console */
	case 7:		/* Enable logging to console */
	case 8:		/* Set level of messages printed to console */
		rc = task_has_system(current, SYSTEM__SYSLOG_CONSOLE);
		break;
	case 0:		/* Close log */
	case 1:		/* Open log */
	case 2:		/* Read from log */
	case 4:		/* Read/clear last kernel messages */
	case 5:		/* Clear ring buffer */
	default:
		rc = task_has_system(current, SYSTEM__SYSLOG_MOD);
		break;
	}
	return rc;
}

/*
 * Check that a process has enough memory to allocate a new virtual
 * mapping. 0 means there is enough memory for the allocation to
 * succeed and -ENOMEM implies there is not.
 *
 * Do not audit the selinux permission check, as this is applied to all
 * processes that allocate mappings.
 */
static int selinux_vm_enough_memory(struct mm_struct *mm, long pages)
{
	int rc, cap_sys_admin = 0;

	rc = selinux_capable(current, current_cred(), CAP_SYS_ADMIN,
			     SECURITY_CAP_NOAUDIT);
	if (rc == 0)
		cap_sys_admin = 1;

	return __vm_enough_memory(mm, pages, cap_sys_admin);
}

/* binprm security operations */

static int selinux_bprm_set_creds(struct linux_binprm *bprm)
{
	const struct task_security_struct *old_tsec;
	struct task_security_struct *new_tsec;
	struct inode_security_struct *isec;
	struct common_audit_data ad;
	struct inode *inode = bprm->file->f_path.dentry->d_inode;
	int rc;

	rc = cap_bprm_set_creds(bprm);
	if (rc)
		return rc;

	/* SELinux context only depends on initial program or script and not
	 * the script interpreter */
	if (bprm->cred_prepared)
		return 0;

	old_tsec = current_security();
	new_tsec = bprm->cred->security;
	isec = inode->i_security;

	/* Default to the current task SID. */
	new_tsec->sid = old_tsec->sid;
	new_tsec->osid = old_tsec->sid;

	/* Reset fs, key, and sock SIDs on execve. */
	new_tsec->create_sid = 0;
	new_tsec->keycreate_sid = 0;
	new_tsec->sockcreate_sid = 0;

	if (old_tsec->exec_sid) {
		new_tsec->sid = old_tsec->exec_sid;
		/* Reset exec SID on execve. */
		new_tsec->exec_sid = 0;
	} else {
		/* Check for a default transition on this program. */
		rc = security_transition_sid(old_tsec->sid, isec->sid,
					     SECCLASS_PROCESS, &new_tsec->sid);
		if (rc)
			return rc;
	}

	COMMON_AUDIT_DATA_INIT(&ad, FS);
	ad.u.fs.path = bprm->file->f_path;

	if (bprm->file->f_path.mnt->mnt_flags & MNT_NOSUID)
		new_tsec->sid = old_tsec->sid;

	if (new_tsec->sid == old_tsec->sid) {
		rc = avc_has_perm(old_tsec->sid, isec->sid,
				  SECCLASS_FILE, FILE__EXECUTE_NO_TRANS, &ad);
		if (rc)
			return rc;
	} else {
		/* Check permissions for the transition. */
		rc = avc_has_perm(old_tsec->sid, new_tsec->sid,
				  SECCLASS_PROCESS, PROCESS__TRANSITION, &ad);
		if (rc)
			return rc;

		rc = avc_has_perm(new_tsec->sid, isec->sid,
				  SECCLASS_FILE, FILE__ENTRYPOINT, &ad);
		if (rc)
			return rc;

		/* Check for shared state */
		if (bprm->unsafe & LSM_UNSAFE_SHARE) {
			rc = avc_has_perm(old_tsec->sid, new_tsec->sid,
					  SECCLASS_PROCESS, PROCESS__SHARE,
					  NULL);
			if (rc)
				return -EPERM;
		}

		/* Make sure that anyone attempting to ptrace over a task that
		 * changes its SID has the appropriate permit */
		if (bprm->unsafe &
		    (LSM_UNSAFE_PTRACE | LSM_UNSAFE_PTRACE_CAP)) {
			struct task_struct *tracer;
			struct task_security_struct *sec;
			u32 ptsid = 0;

			rcu_read_lock();
			tracer = tracehook_tracer_task(current);
			if (likely(tracer != NULL)) {
				sec = __task_cred(tracer)->security;
				ptsid = sec->sid;
			}
			rcu_read_unlock();

			if (ptsid != 0) {
				rc = avc_has_perm(ptsid, new_tsec->sid,
						  SECCLASS_PROCESS,
						  PROCESS__PTRACE, NULL);
				if (rc)
					return -EPERM;
			}
		}

		/* Clear any possibly unsafe personality bits on exec: */
		bprm->per_clear |= PER_CLEAR_ON_SETID;
	}

	return 0;
}

static int selinux_bprm_secureexec(struct linux_binprm *bprm)
{
	const struct cred *cred = current_cred();
	const struct task_security_struct *tsec = cred->security;
	u32 sid, osid;
	int atsecure = 0;

	sid = tsec->sid;
	osid = tsec->osid;

	if (osid != sid) {
		/* Enable secure mode for SIDs transitions unless
		   the noatsecure permission is granted between
		   the two SIDs, i.e. ahp returns 0. */
		atsecure = avc_has_perm(osid, sid,
					SECCLASS_PROCESS,
					PROCESS__NOATSECURE, NULL);
	}

	return (atsecure || cap_bprm_secureexec(bprm));
}

extern struct vfsmount *selinuxfs_mount;
extern struct dentry *selinux_null;

/* Derived from fs/exec.c:flush_old_files. */
static inline void flush_unauthorized_files(const struct cred *cred,
					    struct files_struct *files)
{
	struct common_audit_data ad;
	struct file *file, *devnull = NULL;
	struct tty_struct *tty;
	struct fdtable *fdt;
	long j = -1;
	int drop_tty = 0;

	tty = get_current_tty();
	if (tty) {
		file_list_lock();
		if (!list_empty(&tty->tty_files)) {
			struct inode *inode;

			/* Revalidate access to controlling tty.
			   Use inode_has_perm on the tty inode directly rather
			   than using file_has_perm, as this particular open
			   file may belong to another process and we are only
			   interested in the inode-based check here. */
			file = list_first_entry(&tty->tty_files, struct file, f_u.fu_list);
			inode = file->f_path.dentry->d_inode;
			if (inode_has_perm(cred, inode,
					   FILE__READ | FILE__WRITE, NULL)) {
				drop_tty = 1;
			}
		}
		file_list_unlock();
		tty_kref_put(tty);
	}
	/* Reset controlling tty. */
	if (drop_tty)
		no_tty();

	/* Revalidate access to inherited open files. */

	COMMON_AUDIT_DATA_INIT(&ad, FS);

	spin_lock(&files->file_lock);
	for (;;) {
		unsigned long set, i;
		int fd;

		j++;
		i = j * __NFDBITS;
		fdt = files_fdtable(files);
		if (i >= fdt->max_fds)
			break;
		set = fdt->open_fds->fds_bits[j];
		if (!set)
			continue;
		spin_unlock(&files->file_lock);
		for ( ; set ; i++, set >>= 1) {
			if (set & 1) {
				file = fget(i);
				if (!file)
					continue;
				if (file_has_perm(cred,
						  file,
						  file_to_av(file))) {
					sys_close(i);
					fd = get_unused_fd();
					if (fd != i) {
						if (fd >= 0)
							put_unused_fd(fd);
						fput(file);
						continue;
					}
					if (devnull) {
						get_file(devnull);
					} else {
						devnull = dentry_open(
							dget(selinux_null),
							mntget(selinuxfs_mount),
							O_RDWR, cred);
						if (IS_ERR(devnull)) {
							devnull = NULL;
							put_unused_fd(fd);
							fput(file);
							continue;
						}
					}
					fd_install(fd, devnull);
				}
				fput(file);
			}
		}
		spin_lock(&files->file_lock);

	}
	spin_unlock(&files->file_lock);
}

/*
 * Prepare a process for imminent new credential changes due to exec
 */
static void selinux_bprm_committing_creds(struct linux_binprm *bprm)
{
	struct task_security_struct *new_tsec;
	struct rlimit *rlim, *initrlim;
	int rc, i;

	new_tsec = bprm->cred->security;
	if (new_tsec->sid == new_tsec->osid)
		return;

	/* Close files for which the new task SID is not authorized. */
	flush_unauthorized_files(bprm->cred, current->files);

	/* Always clear parent death signal on SID transitions. */
	current->pdeath_signal = 0;

	/* Check whether the new SID can inherit resource limits from the old
	 * SID.  If not, reset all soft limits to the lower of the current
	 * task's hard limit and the init task's soft limit.
	 *
	 * Note that the setting of hard limits (even to lower them) can be
	 * controlled by the setrlimit check.  The inclusion of the init task's
	 * soft limit into the computation is to avoid resetting soft limits
	 * higher than the default soft limit for cases where the default is
	 * lower than the hard limit, e.g. RLIMIT_CORE or RLIMIT_STACK.
	 */
	rc = avc_has_perm(new_tsec->osid, new_tsec->sid, SECCLASS_PROCESS,
			  PROCESS__RLIMITINH, NULL);
	if (rc) {
		for (i = 0; i < RLIM_NLIMITS; i++) {
			rlim = current->signal->rlim + i;
			initrlim = init_task.signal->rlim + i;
			rlim->rlim_cur = min(rlim->rlim_max, initrlim->rlim_cur);
		}
		update_rlimit_cpu(rlim->rlim_cur);
	}
}

/*
 * Clean up the process immediately after the installation of new credentials
 * due to exec
 */
static void selinux_bprm_committed_creds(struct linux_binprm *bprm)
{
	const struct task_security_struct *tsec = current_security();
	struct itimerval itimer;
	u32 osid, sid;
	int rc, i;

	osid = tsec->osid;
	sid = tsec->sid;

	if (sid == osid)
		return;

	/* Check whether the new SID can inherit signal state from the old SID.
	 * If not, clear itimers to avoid subsequent signal generation and
	 * flush and unblock signals.
	 *
	 * This must occur _after_ the task SID has been updated so that any
	 * kill done after the flush will be checked against the new SID.
	 */
	rc = avc_has_perm(osid, sid, SECCLASS_PROCESS, PROCESS__SIGINH, NULL);
	if (rc) {
		memset(&itimer, 0, sizeof itimer);
		for (i = 0; i < 3; i++)
			do_setitimer(i, &itimer, NULL);
		spin_lock_irq(&current->sighand->siglock);
		if (!(current->signal->flags & SIGNAL_GROUP_EXIT)) {
			__flush_signals(current);
			flush_signal_handlers(current, 1);
			sigemptyset(&current->blocked);
		}
		spin_unlock_irq(&current->sighand->siglock);
	}

	/* Wake up the parent if it is waiting so that it can recheck
	 * wait permission to the new task SID. */
	read_lock(&tasklist_lock);
	__wake_up_parent(current, current->real_parent);
	read_unlock(&tasklist_lock);
}

/* superblock security operations */

static int selinux_sb_alloc_security(struct super_block *sb)
{
	return superblock_alloc_security(sb);
}

static void selinux_sb_free_security(struct super_block *sb)
{
	superblock_free_security(sb);
}

static inline int match_prefix(char *prefix, int plen, char *option, int olen)
{
	if (plen > olen)
		return 0;

	return !memcmp(prefix, option, plen);
}

static inline int selinux_option(char *option, int len)
{
	return (match_prefix(CONTEXT_STR, sizeof(CONTEXT_STR)-1, option, len) ||
		match_prefix(FSCONTEXT_STR, sizeof(FSCONTEXT_STR)-1, option, len) ||
		match_prefix(DEFCONTEXT_STR, sizeof(DEFCONTEXT_STR)-1, option, len) ||
		match_prefix(ROOTCONTEXT_STR, sizeof(ROOTCONTEXT_STR)-1, option, len) ||
		match_prefix(LABELSUPP_STR, sizeof(LABELSUPP_STR)-1, option, len));
}

static inline void take_option(char **to, char *from, int *first, int len)
{
	if (!*first) {
		**to = ',';
		*to += 1;
	} else
		*first = 0;
	memcpy(*to, from, len);
	*to += len;
}

static inline void take_selinux_option(char **to, char *from, int *first,
				       int len)
{
	int current_size = 0;

	if (!*first) {
		**to = '|';
		*to += 1;
	} else
		*first = 0;

	while (current_size < len) {
		if (*from != '"') {
			**to = *from;
			*to += 1;
		}
		from += 1;
		current_size += 1;
	}
}

static int selinux_sb_copy_data(char *orig, char *copy)
{
	int fnosec, fsec, rc = 0;
	char *in_save, *in_curr, *in_end;
	char *sec_curr, *nosec_save, *nosec;
	int open_quote = 0;

	in_curr = orig;
	sec_curr = copy;

	nosec = (char *)get_zeroed_page(GFP_KERNEL);
	if (!nosec) {
		rc = -ENOMEM;
		goto out;
	}

	nosec_save = nosec;
	fnosec = fsec = 1;
	in_save = in_end = orig;

	do {
		if (*in_end == '"')
			open_quote = !open_quote;
		if ((*in_end == ',' && open_quote == 0) ||
				*in_end == '\0') {
			int len = in_end - in_curr;

			if (selinux_option(in_curr, len))
				take_selinux_option(&sec_curr, in_curr, &fsec, len);
			else
				take_option(&nosec, in_curr, &fnosec, len);

			in_curr = in_end + 1;
		}
	} while (*in_end++);

	strcpy(in_save, nosec_save);
	free_page((unsigned long)nosec_save);
out:
	return rc;
}

static int selinux_sb_kern_mount(struct super_block *sb, int flags, void *data)
{
	const struct cred *cred = current_cred();
	struct common_audit_data ad;
	int rc;

	rc = superblock_doinit(sb, data);
	if (rc)
		return rc;

	/* Allow all mounts performed by the kernel */
	if (flags & MS_KERNMOUNT)
		return 0;

	COMMON_AUDIT_DATA_INIT(&ad, FS);
	ad.u.fs.path.dentry = sb->s_root;
	return superblock_has_perm(cred, sb, FILESYSTEM__MOUNT, &ad);
}

static int selinux_sb_statfs(struct dentry *dentry)
{
	const struct cred *cred = current_cred();
	struct common_audit_data ad;

	COMMON_AUDIT_DATA_INIT(&ad, FS);
	ad.u.fs.path.dentry = dentry->d_sb->s_root;
	return superblock_has_perm(cred, dentry->d_sb, FILESYSTEM__GETATTR, &ad);
}

static int selinux_mount(char *dev_name,
			 struct path *path,
			 char *type,
			 unsigned long flags,
			 void *data)
{
	const struct cred *cred = current_cred();

	if (flags & MS_REMOUNT)
		return superblock_has_perm(cred, path->mnt->mnt_sb,
					   FILESYSTEM__REMOUNT, NULL);
	else
		return dentry_has_perm(cred, path->mnt, path->dentry,
				       FILE__MOUNTON);
}

static int selinux_umount(struct vfsmount *mnt, int flags)
{
	const struct cred *cred = current_cred();

	return superblock_has_perm(cred, mnt->mnt_sb,
				   FILESYSTEM__UNMOUNT, NULL);
}

/* inode security operations */

static int selinux_inode_alloc_security(struct inode *inode)
{
	return inode_alloc_security(inode);
}

static void selinux_inode_free_security(struct inode *inode)
{
	inode_free_security(inode);
}

static int selinux_inode_init_security(struct inode *inode, struct inode *dir,
				       char **name, void **value,
				       size_t *len)
{
	const struct cred *cred = current_cred();
	const struct task_security_struct *tsec = cred->security;
	struct inode_security_struct *dsec;
	struct superblock_security_struct *sbsec;
	u32 sid, newsid, clen;
	int rc;
	char *namep = NULL, *context;

	dsec = dir->i_security;
	sbsec = dir->i_sb->s_security;

	sid = tsec->sid;
	newsid = tsec->create_sid;

	if (!newsid || !(sbsec->flags & SE_SBLABELSUPP)) {
		rc = security_transition_sid(sid, dsec->sid,
					     inode_mode_to_security_class(inode->i_mode),
					     &newsid);
		if (rc) {
			printk(KERN_WARNING "%s:  "
			       "security_transition_sid failed, rc=%d (dev=%s "
			       "ino=%ld)\n",
			       __func__,
			       -rc, inode->i_sb->s_id, inode->i_ino);
			return rc;
		}
	}

	/* Possibly defer initialization to selinux_complete_init. */
	if (sbsec->flags & SE_SBINITIALIZED) {
		struct inode_security_struct *isec = inode->i_security;
		isec->sclass = inode_mode_to_security_class(inode->i_mode);
		isec->sid = newsid;
		isec->initialized = 1;
	}

	if (!ss_initialized || !(sbsec->flags & SE_SBLABELSUPP))
		return -EOPNOTSUPP;

	if (name) {
		namep = kstrdup(XATTR_SELINUX_SUFFIX, GFP_NOFS);
		if (!namep)
			return -ENOMEM;
		*name = namep;
	}

	if (value && len) {
		rc = security_sid_to_context_force(newsid, &context, &clen);
		if (rc) {
			kfree(namep);
			return rc;
		}
		*value = context;
		*len = clen;
	}

	return 0;
}

static int selinux_inode_create(struct inode *dir, struct dentry *dentry, int mask)
{
	return may_create(dir, dentry, SECCLASS_FILE);
}

static int selinux_inode_link(struct dentry *old_dentry, struct inode *dir, struct dentry *new_dentry)
{
	return may_link(dir, old_dentry, MAY_LINK);
}

static int selinux_inode_unlink(struct inode *dir, struct dentry *dentry)
{
	return may_link(dir, dentry, MAY_UNLINK);
}

static int selinux_inode_symlink(struct inode *dir, struct dentry *dentry, const char *name)
{
	return may_create(dir, dentry, SECCLASS_LNK_FILE);
}

static int selinux_inode_mkdir(struct inode *dir, struct dentry *dentry, int mask)
{
	return may_create(dir, dentry, SECCLASS_DIR);
}

static int selinux_inode_rmdir(struct inode *dir, struct dentry *dentry)
{
	return may_link(dir, dentry, MAY_RMDIR);
}

static int selinux_inode_mknod(struct inode *dir, struct dentry *dentry, int mode, dev_t dev)
{
	return may_create(dir, dentry, inode_mode_to_security_class(mode));
}

static int selinux_inode_rename(struct inode *old_inode, struct dentry *old_dentry,
				struct inode *new_inode, struct dentry *new_dentry)
{
	return may_rename(old_inode, old_dentry, new_inode, new_dentry);
}

static int selinux_inode_readlink(struct dentry *dentry)
{
	const struct cred *cred = current_cred();

	return dentry_has_perm(cred, NULL, dentry, FILE__READ);
}

static int selinux_inode_follow_link(struct dentry *dentry, struct nameidata *nameidata)
{
	const struct cred *cred = current_cred();

	return dentry_has_perm(cred, NULL, dentry, FILE__READ);
}

static int selinux_inode_permission(struct inode *inode, int mask)
{
	const struct cred *cred = current_cred();

	if (!mask) {
		/* No permission to check.  Existence test. */
		return 0;
	}

	return inode_has_perm(cred, inode,
			      file_mask_to_av(inode->i_mode, mask), NULL);
}

static int selinux_inode_setattr(struct dentry *dentry, struct iattr *iattr)
{
	const struct cred *cred = current_cred();
	unsigned int ia_valid = iattr->ia_valid;

	/* ATTR_FORCE is just used for ATTR_KILL_S[UG]ID. */
	if (ia_valid & ATTR_FORCE) {
		ia_valid &= ~(ATTR_KILL_SUID | ATTR_KILL_SGID | ATTR_MODE |
			      ATTR_FORCE);
		if (!ia_valid)
			return 0;
	}

	if (ia_valid & (ATTR_MODE | ATTR_UID | ATTR_GID |
			ATTR_ATIME_SET | ATTR_MTIME_SET | ATTR_TIMES_SET))
		return dentry_has_perm(cred, NULL, dentry, FILE__SETATTR);

	return dentry_has_perm(cred, NULL, dentry, FILE__WRITE);
}

static int selinux_inode_getattr(struct vfsmount *mnt, struct dentry *dentry)
{
	const struct cred *cred = current_cred();

	return dentry_has_perm(cred, mnt, dentry, FILE__GETATTR);
}

static int selinux_inode_setotherxattr(struct dentry *dentry, const char *name)
{
	const struct cred *cred = current_cred();

	if (!strncmp(name, XATTR_SECURITY_PREFIX,
		     sizeof XATTR_SECURITY_PREFIX - 1)) {
		if (!strcmp(name, XATTR_NAME_CAPS)) {
			if (!capable(CAP_SETFCAP))
				return -EPERM;
		} else if (!capable(CAP_SYS_ADMIN)) {
			/* A different attribute in the security namespace.
			   Restrict to administrator. */
			return -EPERM;
		}
	}

	/* Not an attribute we recognize, so just check the
	   ordinary setattr permission. */
	return dentry_has_perm(cred, NULL, dentry, FILE__SETATTR);
}

static int selinux_inode_setxattr(struct dentry *dentry, const char *name,
				  const void *value, size_t size, int flags)
{
	struct inode *inode = dentry->d_inode;
	struct inode_security_struct *isec = inode->i_security;
	struct superblock_security_struct *sbsec;
	struct common_audit_data ad;
	u32 newsid, sid = current_sid();
	int rc = 0;

	if (strcmp(name, XATTR_NAME_SELINUX))
		return selinux_inode_setotherxattr(dentry, name);

	sbsec = inode->i_sb->s_security;
	if (!(sbsec->flags & SE_SBLABELSUPP))
		return -EOPNOTSUPP;

	if (!is_owner_or_cap(inode))
		return -EPERM;

	COMMON_AUDIT_DATA_INIT(&ad, FS);
	ad.u.fs.path.dentry = dentry;

	rc = avc_has_perm(sid, isec->sid, isec->sclass,
			  FILE__RELABELFROM, &ad);
	if (rc)
		return rc;

	rc = security_context_to_sid(value, size, &newsid);
	if (rc == -EINVAL) {
		if (!capable(CAP_MAC_ADMIN))
			return rc;
		rc = security_context_to_sid_force(value, size, &newsid);
	}
	if (rc)
		return rc;

	rc = avc_has_perm(sid, newsid, isec->sclass,
			  FILE__RELABELTO, &ad);
	if (rc)
		return rc;

	rc = security_validate_transition(isec->sid, newsid, sid,
					  isec->sclass);
	if (rc)
		return rc;

	return avc_has_perm(newsid,
			    sbsec->sid,
			    SECCLASS_FILESYSTEM,
			    FILESYSTEM__ASSOCIATE,
			    &ad);
}

static void selinux_inode_post_setxattr(struct dentry *dentry, const char *name,
					const void *value, size_t size,
					int flags)
{
	struct inode *inode = dentry->d_inode;
	struct inode_security_struct *isec = inode->i_security;
	u32 newsid;
	int rc;

	if (strcmp(name, XATTR_NAME_SELINUX)) {
		/* Not an attribute we recognize, so nothing to do. */
		return;
	}

	rc = security_context_to_sid_force(value, size, &newsid);
	if (rc) {
		printk(KERN_ERR "SELinux:  unable to map context to SID"
		       "for (%s, %lu), rc=%d\n",
		       inode->i_sb->s_id, inode->i_ino, -rc);
		return;
	}

	isec->sid = newsid;
	return;
}

static int selinux_inode_getxattr(struct dentry *dentry, const char *name)
{
	const struct cred *cred = current_cred();

	return dentry_has_perm(cred, NULL, dentry, FILE__GETATTR);
}

static int selinux_inode_listxattr(struct dentry *dentry)
{
	const struct cred *cred = current_cred();

	return dentry_has_perm(cred, NULL, dentry, FILE__GETATTR);
}

static int selinux_inode_removexattr(struct dentry *dentry, const char *name)
{
	if (strcmp(name, XATTR_NAME_SELINUX))
		return selinux_inode_setotherxattr(dentry, name);

	/* No one is allowed to remove a SELinux security label.
	   You can change the label, but all data must be labeled. */
	return -EACCES;
}

/*
 * Copy the inode security context value to the user.
 *
 * Permission check is handled by selinux_inode_getxattr hook.
 */
static int selinux_inode_getsecurity(const struct inode *inode, const char *name, void **buffer, bool alloc)
{
	u32 size;
	int error;
	char *context = NULL;
	struct inode_security_struct *isec = inode->i_security;

	if (strcmp(name, XATTR_SELINUX_SUFFIX))
		return -EOPNOTSUPP;

	/*
	 * If the caller has CAP_MAC_ADMIN, then get the raw context
	 * value even if it is not defined by current policy; otherwise,
	 * use the in-core value under current policy.
	 * Use the non-auditing forms of the permission checks since
	 * getxattr may be called by unprivileged processes commonly
	 * and lack of permission just means that we fall back to the
	 * in-core context value, not a denial.
	 */
	error = selinux_capable(current, current_cred(), CAP_MAC_ADMIN,
				SECURITY_CAP_NOAUDIT);
	if (!error)
		error = security_sid_to_context_force(isec->sid, &context,
						      &size);
	else
		error = security_sid_to_context(isec->sid, &context, &size);
	if (error)
		return error;
	error = size;
	if (alloc) {
		*buffer = context;
		goto out_nofree;
	}
	kfree(context);
out_nofree:
	return error;
}

static int selinux_inode_setsecurity(struct inode *inode, const char *name,
				     const void *value, size_t size, int flags)
{
	struct inode_security_struct *isec = inode->i_security;
	u32 newsid;
	int rc;

	if (strcmp(name, XATTR_SELINUX_SUFFIX))
		return -EOPNOTSUPP;

	if (!value || !size)
		return -EACCES;

	rc = security_context_to_sid((void *)value, size, &newsid);
	if (rc)
		return rc;

	isec->sid = newsid;
	isec->initialized = 1;
	return 0;
}

static int selinux_inode_listsecurity(struct inode *inode, char *buffer, size_t buffer_size)
{
	const int len = sizeof(XATTR_NAME_SELINUX);
	if (buffer && len <= buffer_size)
		memcpy(buffer, XATTR_NAME_SELINUX, len);
	return len;
}

static void selinux_inode_getsecid(const struct inode *inode, u32 *secid)
{
	struct inode_security_struct *isec = inode->i_security;
	*secid = isec->sid;
}

/* file security operations */

static int selinux_revalidate_file_permission(struct file *file, int mask)
{
	const struct cred *cred = current_cred();
	struct inode *inode = file->f_path.dentry->d_inode;

	/* file_mask_to_av won't add FILE__WRITE if MAY_APPEND is set */
	if ((file->f_flags & O_APPEND) && (mask & MAY_WRITE))
		mask |= MAY_APPEND;

	return file_has_perm(cred, file,
			     file_mask_to_av(inode->i_mode, mask));
}

static int selinux_file_permission(struct file *file, int mask)
{
	struct inode *inode = file->f_path.dentry->d_inode;
	struct file_security_struct *fsec = file->f_security;
	struct inode_security_struct *isec = inode->i_security;
	u32 sid = current_sid();

	if (!mask)
		/* No permission to check.  Existence test. */
		return 0;

	if (sid == fsec->sid && fsec->isid == isec->sid &&
	    fsec->pseqno == avc_policy_seqno())
		/* No change since dentry_open check. */
		return 0;

	return selinux_revalidate_file_permission(file, mask);
}

static int selinux_file_alloc_security(struct file *file)
{
	return file_alloc_security(file);
}

static void selinux_file_free_security(struct file *file)
{
	file_free_security(file);
}

static int selinux_file_ioctl(struct file *file, unsigned int cmd,
			      unsigned long arg)
{
	const struct cred *cred = current_cred();
	u32 av = 0;

	if (_IOC_DIR(cmd) & _IOC_WRITE)
		av |= FILE__WRITE;
	if (_IOC_DIR(cmd) & _IOC_READ)
		av |= FILE__READ;
	if (!av)
		av = FILE__IOCTL;

	return file_has_perm(cred, file, av);
}

static int file_map_prot_check(struct file *file, unsigned long prot, int shared)
{
	const struct cred *cred = current_cred();
	int rc = 0;

#ifndef CONFIG_PPC32
	if ((prot & PROT_EXEC) && (!file || (!shared && (prot & PROT_WRITE)))) {
		/*
		 * We are making executable an anonymous mapping or a
		 * private file mapping that will also be writable.
		 * This has an additional check.
		 */
		rc = cred_has_perm(cred, cred, PROCESS__EXECMEM);
		if (rc)
			goto error;
	}
#endif

	if (file) {
		/* read access is always possible with a mapping */
		u32 av = FILE__READ;

		/* write access only matters if the mapping is shared */
		if (shared && (prot & PROT_WRITE))
			av |= FILE__WRITE;

		if (prot & PROT_EXEC)
			av |= FILE__EXECUTE;

		return file_has_perm(cred, file, av);
	}

error:
	return rc;
}

static int selinux_file_mmap(struct file *file, unsigned long reqprot,
			     unsigned long prot, unsigned long flags,
			     unsigned long addr, unsigned long addr_only)
{
	int rc = 0;
	u32 sid = current_sid();

	/*
	 * notice that we are intentionally putting the SELinux check before
	 * the secondary cap_file_mmap check.  This is such a likely attempt
	 * at bad behaviour/exploit that we always want to get the AVC, even
	 * if DAC would have also denied the operation.
	 */
	if (addr < CONFIG_LSM_MMAP_MIN_ADDR) {
		rc = avc_has_perm(sid, sid, SECCLASS_MEMPROTECT,
				  MEMPROTECT__MMAP_ZERO, NULL);
		if (rc)
			return rc;
	}

	/* do DAC check on address space usage */
	rc = cap_file_mmap(file, reqprot, prot, flags, addr, addr_only);
	if (rc || addr_only)
		return rc;

	if (selinux_checkreqprot)
		prot = reqprot;

	return file_map_prot_check(file, prot,
				   (flags & MAP_TYPE) == MAP_SHARED);
}

static int selinux_file_mprotect(struct vm_area_struct *vma,
				 unsigned long reqprot,
				 unsigned long prot)
{
	const struct cred *cred = current_cred();

	if (selinux_checkreqprot)
		prot = reqprot;

#ifndef CONFIG_PPC32
	if ((prot & PROT_EXEC) && !(vma->vm_flags & VM_EXEC)) {
		int rc = 0;
		if (vma->vm_start >= vma->vm_mm->start_brk &&
		    vma->vm_end <= vma->vm_mm->brk) {
			rc = cred_has_perm(cred, cred, PROCESS__EXECHEAP);
		} else if (!vma->vm_file &&
			   vma->vm_start <= vma->vm_mm->start_stack &&
			   vma->vm_end >= vma->vm_mm->start_stack) {
			rc = current_has_perm(current, PROCESS__EXECSTACK);
		} else if (vma->vm_file && vma->anon_vma) {
			/*
			 * We are making executable a file mapping that has
			 * had some COW done. Since pages might have been
			 * written, check ability to execute the possibly
			 * modified content.  This typically should only
			 * occur for text relocations.
			 */
			rc = file_has_perm(cred, vma->vm_file, FILE__EXECMOD);
		}
		if (rc)
			return rc;
	}
#endif

	return file_map_prot_check(vma->vm_file, prot, vma->vm_flags&VM_SHARED);
}

static int selinux_file_lock(struct file *file, unsigned int cmd)
{
	const struct cred *cred = current_cred();

	return file_has_perm(cred, file, FILE__LOCK);
}

static int selinux_file_fcntl(struct file *file, unsigned int cmd,
			      unsigned long arg)
{
	const struct cred *cred = current_cred();
	int err = 0;

	switch (cmd) {
	case F_SETFL:
		if (!file->f_path.dentry || !file->f_path.dentry->d_inode) {
			err = -EINVAL;
			break;
		}

		if ((file->f_flags & O_APPEND) && !(arg & O_APPEND)) {
			err = file_has_perm(cred, file, FILE__WRITE);
			break;
		}
		/* fall through */
	case F_SETOWN:
	case F_SETSIG:
	case F_GETFL:
	case F_GETOWN:
	case F_GETSIG:
		/* Just check FD__USE permission */
		err = file_has_perm(cred, file, 0);
		break;
	case F_GETLK:
	case F_SETLK:
	case F_SETLKW:
#if BITS_PER_LONG == 32
	case F_GETLK64:
	case F_SETLK64:
	case F_SETLKW64:
#endif
		if (!file->f_path.dentry || !file->f_path.dentry->d_inode) {
			err = -EINVAL;
			break;
		}
		err = file_has_perm(cred, file, FILE__LOCK);
		break;
	}

	return err;
}

static int selinux_file_set_fowner(struct file *file)
{
	struct file_security_struct *fsec;

	fsec = file->f_security;
	fsec->fown_sid = current_sid();

	return 0;
}

static int selinux_file_send_sigiotask(struct task_struct *tsk,
				       struct fown_struct *fown, int signum)
{
	struct file *file;
	u32 sid = task_sid(tsk);
	u32 perm;
	struct file_security_struct *fsec;

	/* struct fown_struct is never outside the context of a struct file */
	file = container_of(fown, struct file, f_owner);

	fsec = file->f_security;

	if (!signum)
		perm = signal_to_av(SIGIO); /* as per send_sigio_to_task */
	else
		perm = signal_to_av(signum);

	return avc_has_perm(fsec->fown_sid, sid,
			    SECCLASS_PROCESS, perm, NULL);
}

static int selinux_file_receive(struct file *file)
{
	const struct cred *cred = current_cred();

	return file_has_perm(cred, file, file_to_av(file));
}

static int selinux_dentry_open(struct file *file, const struct cred *cred)
{
	struct file_security_struct *fsec;
	struct inode *inode;
	struct inode_security_struct *isec;

	inode = file->f_path.dentry->d_inode;
	fsec = file->f_security;
	isec = inode->i_security;
	/*
	 * Save inode label and policy sequence number
	 * at open-time so that selinux_file_permission
	 * can determine whether revalidation is necessary.
	 * Task label is already saved in the file security
	 * struct as its SID.
	 */
	fsec->isid = isec->sid;
	fsec->pseqno = avc_policy_seqno();
	/*
	 * Since the inode label or policy seqno may have changed
	 * between the selinux_inode_permission check and the saving
	 * of state above, recheck that access is still permitted.
	 * Otherwise, access might never be revalidated against the
	 * new inode label or new policy.
	 * This check is not redundant - do not remove.
	 */
	return inode_has_perm(cred, inode, open_file_to_av(file), NULL);
}

/* task security operations */

static int selinux_task_create(unsigned long clone_flags)
{
	return current_has_perm(current, PROCESS__FORK);
}

/*
 * allocate the SELinux part of blank credentials
 */
static int selinux_cred_alloc_blank(struct cred *cred, gfp_t gfp)
{
	struct task_security_struct *tsec;

	tsec = kzalloc(sizeof(struct task_security_struct), gfp);
	if (!tsec)
		return -ENOMEM;

	cred->security = tsec;
	return 0;
}

/*
 * detach and free the LSM part of a set of credentials
 */
static void selinux_cred_free(struct cred *cred)
{
	struct task_security_struct *tsec = cred->security;

	BUG_ON((unsigned long) cred->security < PAGE_SIZE);
	cred->security = (void *) 0x7UL;
	kfree(tsec);
}

/*
 * prepare a new set of credentials for modification
 */
static int selinux_cred_prepare(struct cred *new, const struct cred *old,
				gfp_t gfp)
{
	const struct task_security_struct *old_tsec;
	struct task_security_struct *tsec;

	old_tsec = old->security;

	tsec = kmemdup(old_tsec, sizeof(struct task_security_struct), gfp);
	if (!tsec)
		return -ENOMEM;

	new->security = tsec;
	return 0;
}

/*
 * transfer the SELinux data to a blank set of creds
 */
static void selinux_cred_transfer(struct cred *new, const struct cred *old)
{
	const struct task_security_struct *old_tsec = old->security;
	struct task_security_struct *tsec = new->security;

	*tsec = *old_tsec;
}

/*
 * set the security data for a kernel service
 * - all the creation contexts are set to unlabelled
 */
static int selinux_kernel_act_as(struct cred *new, u32 secid)
{
	struct task_security_struct *tsec = new->security;
	u32 sid = current_sid();
	int ret;

	ret = avc_has_perm(sid, secid,
			   SECCLASS_KERNEL_SERVICE,
			   KERNEL_SERVICE__USE_AS_OVERRIDE,
			   NULL);
	if (ret == 0) {
		tsec->sid = secid;
		tsec->create_sid = 0;
		tsec->keycreate_sid = 0;
		tsec->sockcreate_sid = 0;
	}
	return ret;
}

/*
 * set the file creation context in a security record to the same as the
 * objective context of the specified inode
 */
static int selinux_kernel_create_files_as(struct cred *new, struct inode *inode)
{
	struct inode_security_struct *isec = inode->i_security;
	struct task_security_struct *tsec = new->security;
	u32 sid = current_sid();
	int ret;

	ret = avc_has_perm(sid, isec->sid,
			   SECCLASS_KERNEL_SERVICE,
			   KERNEL_SERVICE__CREATE_FILES_AS,
			   NULL);

	if (ret == 0)
		tsec->create_sid = isec->sid;
	return 0;
}

static int selinux_kernel_module_request(void)
{
	return task_has_system(current, SYSTEM__MODULE_REQUEST);
}

static int selinux_task_setpgid(struct task_struct *p, pid_t pgid)
{
	return current_has_perm(p, PROCESS__SETPGID);
}

static int selinux_task_getpgid(struct task_struct *p)
{
	return current_has_perm(p, PROCESS__GETPGID);
}

static int selinux_task_getsid(struct task_struct *p)
{
	return current_has_perm(p, PROCESS__GETSESSION);
}

static void selinux_task_getsecid(struct task_struct *p, u32 *secid)
{
	*secid = task_sid(p);
}

static int selinux_task_setnice(struct task_struct *p, int nice)
{
	int rc;

	rc = cap_task_setnice(p, nice);
	if (rc)
		return rc;

	return current_has_perm(p, PROCESS__SETSCHED);
}

static int selinux_task_setioprio(struct task_struct *p, int ioprio)
{
	int rc;

	rc = cap_task_setioprio(p, ioprio);
	if (rc)
		return rc;

	return current_has_perm(p, PROCESS__SETSCHED);
}

static int selinux_task_getioprio(struct task_struct *p)
{
	return current_has_perm(p, PROCESS__GETSCHED);
}

static int selinux_task_setrlimit(unsigned int resource, struct rlimit *new_rlim)
{
	struct rlimit *old_rlim = current->signal->rlim + resource;

	/* Control the ability to change the hard limit (whether
	   lowering or raising it), so that the hard limit can
	   later be used as a safe reset point for the soft limit
	   upon context transitions.  See selinux_bprm_committing_creds. */
	if (old_rlim->rlim_max != new_rlim->rlim_max)
		return current_has_perm(current, PROCESS__SETRLIMIT);

	return 0;
}

static int selinux_task_setscheduler(struct task_struct *p, int policy, struct sched_param *lp)
{
	int rc;

	rc = cap_task_setscheduler(p, policy, lp);
	if (rc)
		return rc;

	return current_has_perm(p, PROCESS__SETSCHED);
}

static int selinux_task_getscheduler(struct task_struct *p)
{
	return current_has_perm(p, PROCESS__GETSCHED);
}

static int selinux_task_movememory(struct task_struct *p)
{
	return current_has_perm(p, PROCESS__SETSCHED);
}

static int selinux_task_kill(struct task_struct *p, struct siginfo *info,
				int sig, u32 secid)
{
	u32 perm;
	int rc;

	if (!sig)
		perm = PROCESS__SIGNULL; /* null signal; existence test */
	else
		perm = signal_to_av(sig);
	if (secid)
		rc = avc_has_perm(secid, task_sid(p),
				  SECCLASS_PROCESS, perm, NULL);
	else
		rc = current_has_perm(p, perm);
	return rc;
}

static int selinux_task_wait(struct task_struct *p)
{
	return task_has_perm(p, current, PROCESS__SIGCHLD);
}

static void selinux_task_to_inode(struct task_struct *p,
				  struct inode *inode)
{
	struct inode_security_struct *isec = inode->i_security;
	u32 sid = task_sid(p);

	isec->sid = sid;
	isec->initialized = 1;
}

/* Returns error only if unable to parse addresses */
static int selinux_parse_skb_ipv4(struct sk_buff *skb,
			struct common_audit_data *ad, u8 *proto)
{
	int offset, ihlen, ret = -EINVAL;
	struct iphdr _iph, *ih;

	offset = skb_network_offset(skb);
	ih = skb_header_pointer(skb, offset, sizeof(_iph), &_iph);
	if (ih == NULL)
		goto out;

	ihlen = ih->ihl * 4;
	if (ihlen < sizeof(_iph))
		goto out;

	ad->u.net.v4info.saddr = ih->saddr;
	ad->u.net.v4info.daddr = ih->daddr;
	ret = 0;

	if (proto)
		*proto = ih->protocol;

	switch (ih->protocol) {
	case IPPROTO_TCP: {
		struct tcphdr _tcph, *th;

		if (ntohs(ih->frag_off) & IP_OFFSET)
			break;

		offset += ihlen;
		th = skb_header_pointer(skb, offset, sizeof(_tcph), &_tcph);
		if (th == NULL)
			break;

		ad->u.net.sport = th->source;
		ad->u.net.dport = th->dest;
		break;
	}

	case IPPROTO_UDP: {
		struct udphdr _udph, *uh;

		if (ntohs(ih->frag_off) & IP_OFFSET)
			break;

		offset += ihlen;
		uh = skb_header_pointer(skb, offset, sizeof(_udph), &_udph);
		if (uh == NULL)
			break;

		ad->u.net.sport = uh->source;
		ad->u.net.dport = uh->dest;
		break;
	}

	case IPPROTO_DCCP: {
		struct dccp_hdr _dccph, *dh;

		if (ntohs(ih->frag_off) & IP_OFFSET)
			break;

		offset += ihlen;
		dh = skb_header_pointer(skb, offset, sizeof(_dccph), &_dccph);
		if (dh == NULL)
			break;

		ad->u.net.sport = dh->dccph_sport;
		ad->u.net.dport = dh->dccph_dport;
		break;
	}

	default:
		break;
	}
out:
	return ret;
}

#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)

/* Returns error only if unable to parse addresses */
static int selinux_parse_skb_ipv6(struct sk_buff *skb,
			struct common_audit_data *ad, u8 *proto)
{
	u8 nexthdr;
	int ret = -EINVAL, offset;
	struct ipv6hdr _ipv6h, *ip6;

	offset = skb_network_offset(skb);
	ip6 = skb_header_pointer(skb, offset, sizeof(_ipv6h), &_ipv6h);
	if (ip6 == NULL)
		goto out;

	ipv6_addr_copy(&ad->u.net.v6info.saddr, &ip6->saddr);
	ipv6_addr_copy(&ad->u.net.v6info.daddr, &ip6->daddr);
	ret = 0;

	nexthdr = ip6->nexthdr;
	offset += sizeof(_ipv6h);
	offset = ipv6_skip_exthdr(skb, offset, &nexthdr);
	if (offset < 0)
		goto out;

	if (proto)
		*proto = nexthdr;

	switch (nexthdr) {
	case IPPROTO_TCP: {
		struct tcphdr _tcph, *th;

		th = skb_header_pointer(skb, offset, sizeof(_tcph), &_tcph);
		if (th == NULL)
			break;

		ad->u.net.sport = th->source;
		ad->u.net.dport = th->dest;
		break;
	}

	case IPPROTO_UDP: {
		struct udphdr _udph, *uh;

		uh = skb_header_pointer(skb, offset, sizeof(_udph), &_udph);
		if (uh == NULL)
			break;

		ad->u.net.sport = uh->source;
		ad->u.net.dport = uh->dest;
		break;
	}

	case IPPROTO_DCCP: {
		struct dccp_hdr _dccph, *dh;

		dh = skb_header_pointer(skb, offset, sizeof(_dccph), &_dccph);
		if (dh == NULL)
			break;

		ad->u.net.sport = dh->dccph_sport;
		ad->u.net.dport = dh->dccph_dport;
		break;
	}

	/* includes fragments */
	default:
		break;
	}
out:
	return ret;
}

#endif /* IPV6 */

static int selinux_parse_skb(struct sk_buff *skb, struct common_audit_data *ad,
			     char **_addrp, int src, u8 *proto)
{
	char *addrp;
	int ret;

	switch (ad->u.net.family) {
	case PF_INET:
		ret = selinux_parse_skb_ipv4(skb, ad, proto);
		if (ret)
			goto parse_error;
		addrp = (char *)(src ? &ad->u.net.v4info.saddr :
				       &ad->u.net.v4info.daddr);
		goto okay;

#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
	case PF_INET6:
		ret = selinux_parse_skb_ipv6(skb, ad, proto);
		if (ret)
			goto parse_error;
		addrp = (char *)(src ? &ad->u.net.v6info.saddr :
				       &ad->u.net.v6info.daddr);
		goto okay;
#endif	/* IPV6 */
	default:
		addrp = NULL;
		goto okay;
	}

parse_error:
	printk(KERN_WARNING
	       "SELinux: failure in selinux_parse_skb(),"
	       " unable to parse packet\n");
	return ret;

okay:
	if (_addrp)
		*_addrp = addrp;
	return 0;
}

/**
 * selinux_skb_peerlbl_sid - Determine the peer label of a packet
 * @skb: the packet
 * @family: protocol family
 * @sid: the packet's peer label SID
 *
 * Description:
 * Check the various different forms of network peer labeling and determine
 * the peer label/SID for the packet; most of the magic actually occurs in
 * the security server function security_net_peersid_cmp().  The function
 * returns zero if the value in @sid is valid (although it may be SECSID_NULL)
 * or -EACCES if @sid is invalid due to inconsistencies with the different
 * peer labels.
 *
 */
static int selinux_skb_peerlbl_sid(struct sk_buff *skb, u16 family, u32 *sid)
{
	int err;
	u32 xfrm_sid;
	u32 nlbl_sid;
	u32 nlbl_type;

	selinux_skb_xfrm_sid(skb, &xfrm_sid);
	selinux_netlbl_skbuff_getsid(skb, family, &nlbl_type, &nlbl_sid);

	err = security_net_peersid_resolve(nlbl_sid, nlbl_type, xfrm_sid, sid);
	if (unlikely(err)) {
		printk(KERN_WARNING
		       "SELinux: failure in selinux_skb_peerlbl_sid(),"
		       " unable to determine packet's peer label\n");
		return -EACCES;
	}

	return 0;
}

/* socket security operations */
static int socket_has_perm(struct task_struct *task, struct socket *sock,
			   u32 perms)
{
	struct inode_security_struct *isec;
	struct common_audit_data ad;
	u32 sid;
	int err = 0;

	isec = SOCK_INODE(sock)->i_security;

	if (isec->sid == SECINITSID_KERNEL)
		goto out;
	sid = task_sid(task);

	COMMON_AUDIT_DATA_INIT(&ad, NET);
	ad.u.net.sk = sock->sk;
	err = avc_has_perm(sid, isec->sid, isec->sclass, perms, &ad);

out:
	return err;
}

static int selinux_socket_create(int family, int type,
				 int protocol, int kern)
{
	const struct cred *cred = current_cred();
	const struct task_security_struct *tsec = cred->security;
	u32 sid, newsid;
	u16 secclass;
	int err = 0;

	if (kern)
		goto out;

	sid = tsec->sid;
	newsid = tsec->sockcreate_sid ?: sid;

	secclass = socket_type_to_security_class(family, type, protocol);
	err = avc_has_perm(sid, newsid, secclass, SOCKET__CREATE, NULL);

out:
	return err;
}

static int selinux_socket_post_create(struct socket *sock, int family,
				      int type, int protocol, int kern)
{
	const struct cred *cred = current_cred();
	const struct task_security_struct *tsec = cred->security;
	struct inode_security_struct *isec;
	struct sk_security_struct *sksec;
	u32 sid, newsid;
	int err = 0;

	sid = tsec->sid;
	newsid = tsec->sockcreate_sid;

	isec = SOCK_INODE(sock)->i_security;

	if (kern)
		isec->sid = SECINITSID_KERNEL;
	else if (newsid)
		isec->sid = newsid;
	else
		isec->sid = sid;

	isec->sclass = socket_type_to_security_class(family, type, protocol);
	isec->initialized = 1;

	if (sock->sk) {
		sksec = sock->sk->sk_security;
		sksec->sid = isec->sid;
		sksec->sclass = isec->sclass;
		err = selinux_netlbl_socket_post_create(sock->sk, family);
	}

	return err;
}

/* Range of port numbers used to automatically bind.
   Need to determine whether we should perform a name_bind
   permission check between the socket and the port number. */

static int selinux_socket_bind(struct socket *sock, struct sockaddr *address, int addrlen)
{
	u16 family;
	int err;

	err = socket_has_perm(current, sock, SOCKET__BIND);
	if (err)
		goto out;

	/*
	 * If PF_INET or PF_INET6, check name_bind permission for the port.
	 * Multiple address binding for SCTP is not supported yet: we just
	 * check the first address now.
	 */
	family = sock->sk->sk_family;
	if (family == PF_INET || family == PF_INET6) {
		char *addrp;
		struct inode_security_struct *isec;
		struct common_audit_data ad;
		struct sockaddr_in *addr4 = NULL;
		struct sockaddr_in6 *addr6 = NULL;
		unsigned short snum;
		struct sock *sk = sock->sk;
		u32 sid, node_perm;

		isec = SOCK_INODE(sock)->i_security;

		if (family == PF_INET) {
			addr4 = (struct sockaddr_in *)address;
			snum = ntohs(addr4->sin_port);
			addrp = (char *)&addr4->sin_addr.s_addr;
		} else {
			addr6 = (struct sockaddr_in6 *)address;
			snum = ntohs(addr6->sin6_port);
			addrp = (char *)&addr6->sin6_addr.s6_addr;
		}

		if (snum) {
			int low, high;

			inet_get_local_port_range(&low, &high);

			if (snum < max(PROT_SOCK, low) || snum > high) {
				err = sel_netport_sid(sk->sk_protocol,
						      snum, &sid);
				if (err)
					goto out;
				COMMON_AUDIT_DATA_INIT(&ad, NET);
				ad.u.net.sport = htons(snum);
				ad.u.net.family = family;
				err = avc_has_perm(isec->sid, sid,
						   isec->sclass,
						   SOCKET__NAME_BIND, &ad);
				if (err)
					goto out;
			}
		}

		switch (isec->sclass) {
		case SECCLASS_TCP_SOCKET:
			node_perm = TCP_SOCKET__NODE_BIND;
			break;

		case SECCLASS_UDP_SOCKET:
			node_perm = UDP_SOCKET__NODE_BIND;
			break;

		case SECCLASS_DCCP_SOCKET:
			node_perm = DCCP_SOCKET__NODE_BIND;
			break;

		default:
			node_perm = RAWIP_SOCKET__NODE_BIND;
			break;
		}

		err = sel_netnode_sid(addrp, family, &sid);
		if (err)
			goto out;

		COMMON_AUDIT_DATA_INIT(&ad, NET);
		ad.u.net.sport = htons(snum);
		ad.u.net.family = family;

		if (family == PF_INET)
			ad.u.net.v4info.saddr = addr4->sin_addr.s_addr;
		else
			ipv6_addr_copy(&ad.u.net.v6info.saddr, &addr6->sin6_addr);

		err = avc_has_perm(isec->sid, sid,
				   isec->sclass, node_perm, &ad);
		if (err)
			goto out;
	}
out:
	return err;
}

static int selinux_socket_connect(struct socket *sock, struct sockaddr *address, int addrlen)
{
	struct sock *sk = sock->sk;
	struct inode_security_struct *isec;
	int err;

	err = socket_has_perm(current, sock, SOCKET__CONNECT);
	if (err)
		return err;

	/*
	 * If a TCP or DCCP socket, check name_connect permission for the port.
	 */
	isec = SOCK_INODE(sock)->i_security;
	if (isec->sclass == SECCLASS_TCP_SOCKET ||
	    isec->sclass == SECCLASS_DCCP_SOCKET) {
		struct common_audit_data ad;
		struct sockaddr_in *addr4 = NULL;
		struct sockaddr_in6 *addr6 = NULL;
		unsigned short snum;
		u32 sid, perm;

		if (sk->sk_family == PF_INET) {
			addr4 = (struct sockaddr_in *)address;
			if (addrlen < sizeof(struct sockaddr_in))
				return -EINVAL;
			snum = ntohs(addr4->sin_port);
		} else {
			addr6 = (struct sockaddr_in6 *)address;
			if (addrlen < SIN6_LEN_RFC2133)
				return -EINVAL;
			snum = ntohs(addr6->sin6_port);
		}

		err = sel_netport_sid(sk->sk_protocol, snum, &sid);
		if (err)
			goto out;

		perm = (isec->sclass == SECCLASS_TCP_SOCKET) ?
		       TCP_SOCKET__NAME_CONNECT : DCCP_SOCKET__NAME_CONNECT;

		COMMON_AUDIT_DATA_INIT(&ad, NET);
		ad.u.net.dport = htons(snum);
		ad.u.net.family = sk->sk_family;
		err = avc_has_perm(isec->sid, sid, isec->sclass, perm, &ad);
		if (err)
			goto out;
	}

	err = selinux_netlbl_socket_connect(sk, address);

out:
	return err;
}

static int selinux_socket_listen(struct socket *sock, int backlog)
{
	return socket_has_perm(current, sock, SOCKET__LISTEN);
}

static int selinux_socket_accept(struct socket *sock, struct socket *newsock)
{
	int err;
	struct inode_security_struct *isec;
	struct inode_security_struct *newisec;

	err = socket_has_perm(current, sock, SOCKET__ACCEPT);
	if (err)
		return err;

	newisec = SOCK_INODE(newsock)->i_security;

	isec = SOCK_INODE(sock)->i_security;
	newisec->sclass = isec->sclass;
	newisec->sid = isec->sid;
	newisec->initialized = 1;

	return 0;
}

static int selinux_socket_sendmsg(struct socket *sock, struct msghdr *msg,
				  int size)
{
	return socket_has_perm(current, sock, SOCKET__WRITE);
}

static int selinux_socket_recvmsg(struct socket *sock, struct msghdr *msg,
				  int size, int flags)
{
	return socket_has_perm(current, sock, SOCKET__READ);
}

static int selinux_socket_getsockname(struct socket *sock)
{
	return socket_has_perm(current, sock, SOCKET__GETATTR);
}

static int selinux_socket_getpeername(struct socket *sock)
{
	return socket_has_perm(current, sock, SOCKET__GETATTR);
}

static int selinux_socket_setsockopt(struct socket *sock, int level, int optname)
{
	int err;

	err = socket_has_perm(current, sock, SOCKET__SETOPT);
	if (err)
		return err;

	return selinux_netlbl_socket_setsockopt(sock, level, optname);
}

static int selinux_socket_getsockopt(struct socket *sock, int level,
				     int optname)
{
	return socket_has_perm(current, sock, SOCKET__GETOPT);
}

static int selinux_socket_shutdown(struct socket *sock, int how)
{
	return socket_has_perm(current, sock, SOCKET__SHUTDOWN);
}

static int selinux_socket_unix_stream_connect(struct socket *sock,
					      struct socket *other,
					      struct sock *newsk)
{
	struct sk_security_struct *ssec;
	struct inode_security_struct *isec;
	struct inode_security_struct *other_isec;
	struct common_audit_data ad;
	int err;

	isec = SOCK_INODE(sock)->i_security;
	other_isec = SOCK_INODE(other)->i_security;

	COMMON_AUDIT_DATA_INIT(&ad, NET);
	ad.u.net.sk = other->sk;

	err = avc_has_perm(isec->sid, other_isec->sid,
			   isec->sclass,
			   UNIX_STREAM_SOCKET__CONNECTTO, &ad);
	if (err)
		return err;

	/* connecting socket */
	ssec = sock->sk->sk_security;
	ssec->peer_sid = other_isec->sid;

	/* server child socket */
	ssec = newsk->sk_security;
	ssec->peer_sid = isec->sid;
	err = security_sid_mls_copy(other_isec->sid, ssec->peer_sid, &ssec->sid);

	return err;
}

static int selinux_socket_unix_may_send(struct socket *sock,
					struct socket *other)
{
	struct inode_security_struct *isec;
	struct inode_security_struct *other_isec;
	struct common_audit_data ad;
	int err;

	isec = SOCK_INODE(sock)->i_security;
	other_isec = SOCK_INODE(other)->i_security;

	COMMON_AUDIT_DATA_INIT(&ad, NET);
	ad.u.net.sk = other->sk;

	err = avc_has_perm(isec->sid, other_isec->sid,
			   isec->sclass, SOCKET__SENDTO, &ad);
	if (err)
		return err;

	return 0;
}

static int selinux_inet_sys_rcv_skb(int ifindex, char *addrp, u16 family,
				    u32 peer_sid,
				    struct common_audit_data *ad)
{
	int err;
	u32 if_sid;
	u32 node_sid;

	err = sel_netif_sid(ifindex, &if_sid);
	if (err)
		return err;
	err = avc_has_perm(peer_sid, if_sid,
			   SECCLASS_NETIF, NETIF__INGRESS, ad);
	if (err)
		return err;

	err = sel_netnode_sid(addrp, family, &node_sid);
	if (err)
		return err;
	return avc_has_perm(peer_sid, node_sid,
			    SECCLASS_NODE, NODE__RECVFROM, ad);
}

static int selinux_sock_rcv_skb_compat(struct sock *sk, struct sk_buff *skb,
				       u16 family)
{
	int err = 0;
	struct sk_security_struct *sksec = sk->sk_security;
	u32 peer_sid;
	u32 sk_sid = sksec->sid;
	struct common_audit_data ad;
	char *addrp;

	COMMON_AUDIT_DATA_INIT(&ad, NET);
	ad.u.net.netif = skb->iif;
	ad.u.net.family = family;
	err = selinux_parse_skb(skb, &ad, &addrp, 1, NULL);
	if (err)
		return err;

	if (selinux_secmark_enabled()) {
		err = avc_has_perm(sk_sid, skb->secmark, SECCLASS_PACKET,
				   PACKET__RECV, &ad);
		if (err)
			return err;
	}

	if (selinux_policycap_netpeer) {
		err = selinux_skb_peerlbl_sid(skb, family, &peer_sid);
		if (err)
			return err;
		err = avc_has_perm(sk_sid, peer_sid,
				   SECCLASS_PEER, PEER__RECV, &ad);
		if (err)
			selinux_netlbl_err(skb, err, 0);
	} else {
		err = selinux_netlbl_sock_rcv_skb(sksec, skb, family, &ad);
		if (err)
			return err;
		err = selinux_xfrm_sock_rcv_skb(sksec->sid, skb, &ad);
	}

	return err;
}

static int selinux_socket_sock_rcv_skb(struct sock *sk, struct sk_buff *skb)
{
	int err;
	struct sk_security_struct *sksec = sk->sk_security;
	u16 family = sk->sk_family;
	u32 sk_sid = sksec->sid;
	struct common_audit_data ad;
	char *addrp;
	u8 secmark_active;
	u8 peerlbl_active;

	if (family != PF_INET && family != PF_INET6)
		return 0;

	/* Handle mapped IPv4 packets arriving via IPv6 sockets */
	if (family == PF_INET6 && skb->protocol == htons(ETH_P_IP))
		family = PF_INET;

	/* If any sort of compatibility mode is enabled then handoff processing
	 * to the selinux_sock_rcv_skb_compat() function to deal with the
	 * special handling.  We do this in an attempt to keep this function
	 * as fast and as clean as possible. */
	if (!selinux_policycap_netpeer)
		return selinux_sock_rcv_skb_compat(sk, skb, family);

	secmark_active = selinux_secmark_enabled();
	peerlbl_active = netlbl_enabled() || selinux_xfrm_enabled();
	if (!secmark_active && !peerlbl_active)
		return 0;

	COMMON_AUDIT_DATA_INIT(&ad, NET);
	ad.u.net.netif = skb->iif;
	ad.u.net.family = family;
	err = selinux_parse_skb(skb, &ad, &addrp, 1, NULL);
	if (err)
		return err;

	if (peerlbl_active) {
		u32 peer_sid;

		err = selinux_skb_peerlbl_sid(skb, family, &peer_sid);
		if (err)
			return err;
		err = selinux_inet_sys_rcv_skb(skb->iif, addrp, family,
					       peer_sid, &ad);
		if (err) {
			selinux_netlbl_err(skb, err, 0);
			return err;
		}
		err = avc_has_perm(sk_sid, peer_sid, SECCLASS_PEER,
				   PEER__RECV, &ad);
		if (err)
			selinux_netlbl_err(skb, err, 0);
	}

	if (secmark_active) {
		err = avc_has_perm(sk_sid, skb->secmark, SECCLASS_PACKET,
				   PACKET__RECV, &ad);
		if (err)
			return err;
	}

	return err;
}

static int selinux_socket_getpeersec_stream(struct socket *sock, char __user *optval,
					    int __user *optlen, unsigned len)
{
	int err = 0;
	char *scontext;
	u32 scontext_len;
	struct sk_security_struct *ssec;
	struct inode_security_struct *isec;
	u32 peer_sid = SECSID_NULL;

	isec = SOCK_INODE(sock)->i_security;

	if (isec->sclass == SECCLASS_UNIX_STREAM_SOCKET ||
	    isec->sclass == SECCLASS_TCP_SOCKET) {
		ssec = sock->sk->sk_security;
		peer_sid = ssec->peer_sid;
	}
	if (peer_sid == SECSID_NULL) {
		err = -ENOPROTOOPT;
		goto out;
	}

	err = security_sid_to_context(peer_sid, &scontext, &scontext_len);

	if (err)
		goto out;

	if (scontext_len > len) {
		err = -ERANGE;
		goto out_len;
	}

	if (copy_to_user(optval, scontextnhanced Li_len))
		err = -EFAULT;

ounux) :
	if (putA Secux (SELinux) , optx) security module
 *
 *	kfreehe SELinu);*  T:
	return err;
}

static int selinux_socket_getpeersec_dgram(struct        *    , ce@nai.ck_buff *skb, u32 *secid)
{
	mon@s Va_ai.co = SECSID_NULL;
	u16 familys.
 e coskb && skb->protocol == htons(ETH_P_IPsecur.com>
 = PF_INET;
	else *  Copyright (C) 2001,2002 Networks AssociV6ates Technology, Inc.6
 *  Copyrighocktes Technolog  Er->s@redh_.com>
 **  Co
		goto out *
 *  CoockrighTechnoloogy, UNIXecurmil>
 *	inodeChriai.co(SOCK_INODE   Eri, &   James M)
 *  Copyright , Inc.
 *			skb_s Valbl_sidCopy,Compute  Copyright (C) Stephennai.co =    James M;ile con  James Morrris <jmorrisecur Smalle-EINVAs@re Smalle0<sds@epoch.ncsc.mil>
 *	 k_allocamesuritynce@nai.com> <ws,ncsc.ompany, gfp_t priotachm>
 * Smallera <ynakam@hitachisk Company,  software<sds@epoch.nvoidhi Nakamura 
 * am@hitachisoft.jp>
 *
 *	m>
 *neral Public Licenker the terms of the GNU GenerclonPublic Liceconst Wayne Sa
 *
 *	Thsoft.jp>
 *
 new,
 *	as ayne Salam@hitach_linux/t*sseceparhat.com@hitach; <linux/tracehook.h>
#include#inc<linux#inclrrno.h>
#includ
	linux/s->smooreattr.h>
#;nux/xattr.hright 
#include <y.h>
#inux/capabilitsclassinclude <le <lilinumil>
 *	netDevelacehook.h>
reset(linux/ser the terms of the GNU Genergoeddel@tsoft.jp>
 *
 *	Thmon@nai.com>
 *e co!,
 *	paul.mooreSECINIT<jmoANY_rustc.
 *  Cop{ Incinux/tracehook.h>
#include k<linux/errno.h>
#includ#include <lible.h <linux/} the terms of the GNU Genock_graftinux/swap.h>
#inclce@nai.com>
 *	parentude <linux/t    <dehook.h>
#includei<linuxrustedcs.co#includ->i.h>
#include <linux/sched.h>
#include <ble.h>
#include <linux/name*  Copat.com>
 * r SolutInc. ||x/errno..h>		/* struct or6 ||
	   llable used in sock_rions, Ine <l.h>
#inclde <linux/mo/netlabee <linuxinclude#includds@epoch.ncsc.mil>
 *	inet_conn_reques#include <linux/netfilter_ilamon, <wsal
			
#inctfilter_sm/atomroc_f *requde <linux/tracehook.h>
#include le.h>
#include <linux/na	csc.y, <sedhat.com>
nux/errno.m>
 *  Comon@#incistd.	      Jinclu
	/* handle mapped IPv4 pa    s arriving via/dcc6.com>
 s */lude <sed in sock_rcv_skbight (C) 2001,2002 Networks Associates Technology, Inc.
 
rity moHewlett-Packard Development Company, L.P.
s (C) 2e coerrftware Engi<linuxCopyrigh#incl07 Hitachi Sofnux/freq->h>
#include <linux/molb.h>
   James Morris <jmorris@re}  <linux/fet typesook.h>
#id_mls_
 * (/netlabel.,linux/ud, &>
#incC) 2nfs_mount.h>n Smalley, <stlb.h>
#include>
#includnality.h>
#include inux/udp.	}
you can relinux/mman.h>
include <asm/atomireq Companyer the terms of the GNU Geinclud*/

#incinux/swap.h>
##include <linux/init.h>
#inpt.h>
#include <linux/netdevice.h>	/* for network#inclx/security.h>
#include <linux/xade <linux =e "a>
#inclux/capade <liy.h>
#incluality.h>
#inclu;h>
#iNOTE: Idealny, we should also get the/uaccessid forvers/
#innew.com>
 *in sync, butnt pdon't haveversion;
 available yet./
#inSont pwill wait untilncluds.h>
# to do it, by which/
#intime iturity_sg_tybeen created and*perm);
ex.ude h>
#iW u16 nlmneedops;take any sort of lcludhere asnt partype, only
	 * thread with accessops;"
#definede <lude "objsec.h"
#incl
#includeclude UM_SELrsk_ops->clude "netnode.h"
#include "netport.h"e <aestablishelinux/swap.h>
#inclnux/bitops.h>
#inclm>
 *	netlink.h>
#include <linux/tile.h>
#include <linux/fdtable.h>
#include <linux/name
#include <linux/dccp.h>
#include <linux/quota.h>
#include <linux/un.h>		/* for Unix socket types */
#include <net/af_unix.h>	/* for Unix sockHewlett-Packard Development Company, LSUFFIX

#defineer the terms of the GNU Gereq_e <liify_flowux/init.h>
#inpt.h>
#include <lude <linux/1 : 0;
	setui *flm>
 *fl>
#includeX XATTR_SELIds@epoch.ncsc.mil>
 *	tun_dev_linux_(of tm>
 *	   _PREFIcurncluelopml MoM
inOP
int nlmtaklinuintoic ioun_versi"oc_flinux_" SID sinctype,       enforciat is belinulinux_seUX_DEis not amsg_lookup(ersitraditional sense,enforinstg;

i/* Lia *	ivatsecond,ic int i
extnux_fdef he kernel,ecmaenforrepnux/ntlinua wide rangeITY_networkcturffic spannlinumultiplelin*el.hnec inis unlikuctures initial
#inclu- checksuperTUN driver to kmemed_va better understandlinuof why thismsg_lookus special = ATO Smalleavc_has_perm(incluincluSECCLASS_TUN>
#incl,nabl>
#incl__CREATEude <elsei Sofr the terms of the GNU Gendary sepostecurity nse version 2,
 *	as up);
#endif

#ifdef CONFIG_SECURITY_SELINUX_BOOTPARAM
in, u16 nlmallow tly performFIG_SNetLabel based l (0)linuUX_DEVnhe p   befsf SECleant sass, uwicydbwatic s;

/soFIG_way;ARK le (1)cicydb_ppx_enforMARK is dstatou_versisuppCURITY_k_enable Sec* inires strng_secmaerity_hity_lockfrom selinther end*
 * ini_cache *sesecmarkalmost certaiux_enforcauscuritfusid)
D(supertialise thealmsgd no idea(sb_securMARK is  kmem 2001,20ELOPric_ty fou SEC= ATOMICsetype, commentskup(u a secondary security ) abunt)bled, u16 nlmurighforcisecondlinux__operUX_DE= ATOECURITY_PREFIallow the use o
#include <linux/ference counter to nimal support for a secondary seattacher is greater than
 * zero SECMARK is considered to be enabled.  Returns  * just to allow the use olude <linucket typn checks the SECMARnux/string.ference counter to s targete if any SECRELABELFROM,s are cunfs_mount.h>
#include <neuct *tsec;

	tsec = cred-ty;
	return tsec->sid;
}

/*
 * get the objective TOrity ID of a task
 */
static inli);

	tsec->osid/udp.h>., Ltd.
 *		       Yuichi Nakamunlmsgs the S = enforcing ? 1 : 0;
	return 1;
}
__setude <li =.
 *de <linrm_setup);
#subjehdr *nlh_setup);
#eom>
 *	    nux/errno.h     _setup);
#tfilter_ipv6.h>
#include <linux/tty.h>
#i  Eri<net/icmp.h>
#
 *  Copy->len < NLMSG_SPACE(0)hugetlity moduering Cot (C) 2004-	}
	nlhncluubjechdr7, 20socket types */
#iuct inlookup(uaccess.h>
#, nlh->uct intype  CoprmD of a task
nux/ffs_mouninclic int kmem_c	audit_log(allow t->he, GFnced LinuGFP_KERNEL, AUDIT_SELINUX_ERRude <lin"SEL>
 *:  unrecognized(sb_link message">lock);
 rren=%hun ints.h>
#	ise\n"ude <lin32 sid = current_uaccess.h>
#ux/senlock.hil>
 *	enforcy focall/audit.hget <ynaw_unknown(secur*/

stat32 c"avc.M
inIgnosk.\n"_cache_zalloc(sNOENTeturn0;
}

statie_alloc_securcket type     Cecks the allow t,ded. */id();

tephen Smalley, <sds@#ifdef CONFIG_NETFILTERs@epoch.nunsignedm/ioctls.h>
#ip_forwarlinux/swaplamon, <wsalacsc.ifindexetup);
#else up("enforcstatic inlin;
	char *addrplude <linuxunistd.securit_stron_he, GFdata aclude8inodmark_activeNULL;
man.h>
che_free(selard Deveche_freeinlock.hil>
 *	policycapmmanrighftware EngiNF_ACCEPled;
	imem_cache_frtypes */
#inurity_sen;
extask_s_inode_cache,ncludn.h>
urrent_sid();
}

static inalloc(sizeity_str
	inol>
 *	xfrmeof(struct file_allority_struct *&& !;
}

static in*file)
{
	struct file_se*  Coewlett-Packard Development Company, L.P.
 *id) != 0*file)
{
	struDROPt fiCOMMON_	mutexDATA_/sys(&ad, NETD ofad.u.netle->ifux/u(&isecec = file->flink.h>
#m>
 *  Coty = fsec;

parse-Pacment Ctruct&_unlo, 1rity IDurity(struct file *file)
{
Copyrighc->fown_sidclude <linux/ "netport.hsys_rcvnt suy(&isec- loc_secompany,turn t))
		li_free_se_alloux/selinux.h>de_caclude "objsec.h"ere *in,ude , 1_FILE;uct file *file)
tic securty = fmem_cache_frh>
#if (n checks the _struct),  each(&sbsecude <liference cPA to sec->ise__FORWARD_IN, GFP_>
#include < *sb)
{
	structt), GFP_KERNE
#inue (1) id
 *
 y strucsbsec-> paaticn tas_versiPOST_ROUTING
	;

	tsec-beuct crx_seenabledmCONFsur->s_s(&sel initneint aryID_UNLMARK is dbefe_frIP32 *licyppliedoid)omican leverage AHID_UNLArot(void)
ee_securilude "objsec.h"skmon,_setlopment Company, _free_security(strruct file *file)
{
e)
{
	struct file_ds@epoch.nsecurity;

	spin_lock(&v4sbsec->issecurity;

	shooknumblock_senux/bitops.h>
#include <l	el.h"
#includencludevice *in(sbsec);
}

static int sk_alloc_soutblock_secsc.(*okfn)sec_lock);
	if (!)e; you can rein_lock(&sbsec->iseist_em->oc(sizeofruct or curre#if defined( *sbsecs MorL);
c)
		return -ENOMEM_MODULE)_del_init(&sbsec->list);
	spin_u6lock(&sb_security_lock);

	sb->s_security = NULL;
	kfree(sbsec);
}

static int sk_alloc_security(struct sock *sk, int family, gfp_t priority)
{
	struct sk_security_struct *ssec;

	ssec = kzalloc(sizeof(*ssec), priority);6 curr#endifid inPV6.\n")_sb->s_security;

	spin_lock(&soutpu#include <ps.h>
#include <linux/ist_del_init(&is* just t file_aloc(sizeof(struct*file)
{
	struct file_sedef_sid = SECINITSID_LOCAL_OUTsbsec->mntpoint_sid = SECINITSLABELd to D;
	sb->s_security = sbsec;

	return 0;
}

static vruct *tsec;

	free_security(struct super_block *sb)
{
	struperblock_securit for eachskf (!sb zero SECMARK is considered to be ena_contrrno.h>
#include	e <net/netlabel.h>
h>
#ining",
};
nux/syscallNOMEM;e(fsec);
}

staec = sb->s_security;

	spin_lock(&_security(struct file *file)
{
sbsec->list))
		list_del_init(&sbsec->list);
	spin_unl

/* Thsecurity_lock);

	sb->s_secrity = NULL;
	kfree(sbsec)
}

static int sk_alloc_security(sruct sock *sk, int family, gfp_t prority)
{
	struct sk_security_struct *ssec;

	ssec = 

/* The;

	ority);
	if (_sb->s_security;

	spin_lock(&srencroute_compaThe security server must besuppory(&isec->list	tup("enforcan
 * zero SE
 *
 *	
	"uses mo_setup);
#endif

#ifdef CONFIG_SECU_lock);

	inode->i_security = NUL
	spin_unlock(&8	tsec ions for eincli Software Engitruct file_;

	ts>
#include <linux/name	struct file_security_struct *fsec = file->f_security;
	file->f_security = NULL;
	kfree(fsec);
}

static int superblock_alloc_sec0  Co2001ons can be provb)
{
	structec;
	u32 sid = current_si;
	INIT_LIST_HEAD(&sb->security;
head);
	spin_lock_init(&sbsec->isec_lock);
	SEND= sb;
	sbsec->sid = SECINITSID_UNloc_security(struct file *fil= avc_has_peec)
	NTEXT_STR laomicd, SECCLASS_FIblock_atruct *tsenode_doinit(struct inode *inode)
{
	return inode_doinit_with_dentry(inM,
			  FIsec_lock);
	if (!list_empty(&isec->list	ialized before
   any&sbsec-nt_sid(&sbsec->isec_lock);

	XT_STR "pt_error, NULL},
};

#define SEL_MOUNT_FAIL_MSG "S	kmem_cache_free(sel superblock_se of theIfFIG_SECURITY_"%s"},ibility modsuperurrent_ ininincluoff
	retaticsec;

	D(superntext, FSCONTEXT_STR "%s"},
) funvoid)
led(eal
stati selin*tion:
 * ncluding. C_INIT= SECINITan attempabledkeepd
 *
 ILESYSTEL);
	as fastecmarksc intnerblpotic LIt = Aile_alloc_security(struct file *file)
{
	sntext, FSCONTEXT_STR "%s"},
	ist_ec(sizeofclude "ne_struct *sbsecXFRMESYSTEM_uses dst->ec)
uperbln-rris;

	rcinith>
#iny(st is cgotic Dnecuritc;

	tATTR) transnablaoid)
id)
ty =_USE_XATTR) to p<linefcount)IG_SsecmasL);
	rtionswe'/
atomicanid crechaions o * enabled int _ncedrolA is returnew_FS_USE_XATTR) {
	on it's fiitiaway 200 strxtern und crreturnt seo bsecomsecuv6ic strcuct casesay bre (sbsec->ernel (!roois or it_emd
 *
  the go ahg;

cmarkurn 0-ENODATA is okct *sbsec PackdYSTEMbtructCURITight (type %s) behavioas no "ncompatible mount optinux_nesecurity_struct *fsec;
	u32 sid = current_sid()le_security_struct), GFPurrent_siL);
	if (!fsec)
		return -ENOMEM;

	fsec->sid = sid;
	fsec->fown_sid = sid;
	file->f_securi/*pyriUSE_XATTR) {
	sizeofbsec->irc;

	rced_versirighuperbl
static vthe xattr haitself;oid crwi creecmarko_secuife policstatia_SELalb)
{
	uct csts anint serblock_seifsb->s_tn;
			else
			type "
				       ernelstatic vosux_nngtruct *,andler\n",
t crintk(KERN_'sust ting_s"%s"},
	{Opt_lduplicate or innux/fiwitchnux/un.h
	sbsexattrruct or:ILE;
	isIPCB"xattr flags & IPSKB	sbsec->EDeturn truct task_slogyock);
	sbsec->sOUptio	opyrightSIZE(labeling_behaviors))ULL)ERN_EbreauritNITIALIZED | 6SE_SBLABELS6UPP);

	if (sbsec->6behavior > ARRAY_SIZE(labeling_behaviors))
		printk(KERN_ERR "SELinux: initialized (dev %s, type %s), unknowdefaulphen(&sbsec->lock);
	INIT_L_HEAD(&sbsec-ing_beehaviors))
		printk(Kf (!sbs;
	int rc;

	return 0;
}

static void file_free_seceturn sbsec->lock);
	INITode_doinroot_
#incluh_dentry(struct inodh>
#include
 */
static inline u32 cred_sid(const struct cred *cr   sbsec->be <linux/personux: initialized (dev %s, type ecurxt_mount_sb_relabel(u32 sid,
			struct superblock_security_struct *sbsec,
			const struct cred *cred)
{
	const struct task_security_n", s*tsec = cred->security;
	intsbsec->list);
	INIT_LIST_HEAD(&sbsec->isec_head);
	spin_lock_init(&sbsec->isec_truct task_s= sb;
	sbsec->sid = SECINITSID_UN superblock_securitymon@ifunistd.tcp.h>ilter labelor == SEtrucdes crloc(sizeof&des cr
	sbsec->sid = SECINIT	INIT_LIST_HEAD(&sbsec->isec_des cr_lock_init(&sbseNETIFct *fIF__EGRESS= sb;
	sbsec->sid = SECINITSIal policy loior to i((struct superb &ior to i  during get_sb by a pseudo filesystem that directlior to ipopulates itself.ODE,    s%s, ty);
	sb;
	sbsec->sid = SECINIT"avc.h"
#inclist))
		list_del_init(&sbsec->list);
	spin_unlrblock_secsecurity_lock);

	sb->s_secuse
int seNULL;
	kfree(sbsec);bel.h"
#includet sk_alloc_security(stode))
				inode_doinit(inod, gfp_t pri iority)
{
	struct sk_security_struct *ssec;

	ssec = rblock_secu, NUout*ssec), priority);
	if (!ssec)
		return -ENOMEM;

	ssec->peer_sid = SECINITSID_UNLABELED;
	ssec->sid = SECINITSIDlock);
		inode = igrab(inode);
		if (inode) {
			if (!IS_PRIVATE(inode))
				inode_doinit(inode);
			iput(inode);
		}
		spin_lock(&sbsec->isec_lock);
		list_del_init(&isec->list);
		goto next_inode;
	}
	spin_unlock(&sbsec->isec_lock
	selinux_netlbl_sk_securnux_netlbl *sbsec = inode-security_fre
 * get the sist);
_r er security ID of the current task
 */
static inlin_struct *ttruct f
	char tmp;? 1 inodered_sid(__task_cred(task))e conity(sdb_loaded_v.h>
on >= POLICYDB_VERSION_NLence ecurity moget the subjective s		return en Smalley, <sds@epoch.ncsc.mil>
 *	INITIALIrecvsec_lock);
	if (!list_emptcapa;
	if t(&isec->list);ck);

	inode->i_security = NUlags & SE_SBINITIALI
		if , NUm_mnt_opts+n -EINVAL;

	if (!ss_initial	struct file_security_structCAPsec = filcap& SE_Snt_optsions for thn checks the NETLINK_UPP);

.ct), ATOMIC);
	if (!opts- targetsference cCAPABILITYpts =_TO_MASK(m_mnt_opts+, GFP_KEds@epoch.ncsc.ipc <ynakam@hitachisoft.jpta#incincludeOMIC-ENOMEM;
se
int sebloc__opting_b*any o>mnt_opts_up("CCLASS_e <linux/netpkam@hitach>
#include <l*cred)
{
	t fil\n"

skz<ynak(sizeof securitsbsec->flags & FSCO)urn -ENOMEM;& SE_SBL!secuftware EngineNOMEMsk));PREFIOMIC);id(OMIC& SE_include <linux/#includ#include <net/S 5

id()d);
	itachux/uaccions for th
 *		       Yof thsbseal Public License ver{
		rc = -ENOMEM;
	i = 0;
	if (sbsec->flags & FSCONTEXT_>

#i (sbsec->fla}
	if (sbsec->flags rris@re*
 *   out_oc(opts->num_mntbjecbjec<ynakam@hitachisoft.jpT;
	}
	 *msgi = 0;
	if (bjecc->flags & FSCONTm CONTEXsbserity_sid_to_context(sbsecurity_sid_to_contexen);
		if (rc)
			gotosbse_free;
		opts->mnt_optsbse.h>
#incnux/syscallUNtive Eype msgsbsec->flags sbsec->dT_MNT) {
		rc = security_T;
	}
	ial Public License verONTEXT_MNT) {
		rc = security_sid_to_context(sbse>flag	if (sbsec-mnt_op	if (sbsec->fl->mnt_opts_fli] = oc(opts->num_mnt_optecks the Sc->mntpoint_sid, &conc = -ENOs	gotourrent_s	i = 0;
	if (sbsec->flags & FSCONTEXT_MNTck);

	inode->i_security = NULL just to allow the use of securitree;
		opot->i_security	struct file_security_structIPCsec = filsbsePREFIpts->mnt_opkepts, sizeof(char *), GFP_rectlyux/string.ruct *isec;
	u
		optkcalloc(opts->num_mnt;

	ssecT;
	}
	if (sbsec->flags & DEFCONTEXT_MNT) {
		r SmalleT;
	}
	if (sbsec->flagsT) {r the terms of the GNU Ge inode *root = sbsec->sb->s_root->d_inode;
		s inode *root = sbsec->
		      /*	isec->i queuttr c->flagopersts alude opts);
	return rc;
}

s had if (sbsec->flags & DEFCONTE had t*mslinux/netdevi
		opts->mnt_opts_flags[i++] = ROOTCONTEXT_MNT;
	}
	if (sbsec->flags & SE_SBLABELSUupporrONTEXT
		opts-<ynakam@hitachide->i_sec&msq->q any otference cMSGQ& SE_SBLr= context;
	ka somsecurit*/
	if (!(s.ts[i] = NULL;
		opts->mnt_opts_flags[i++] = SE_SBLABELSUPP;
	& flag)
			ri != optec =n checks the SECMArn 0;

out_ec->flags & S	goto o & SECMARK
 * GFP_KERINITIALmem_caid_to_context(sbs */
	if (!(sux/seD))
		if (mIT_LT_MNT) {
		rc = security_gs & SE_SBINITIALIroot = sbsec->sb->s_root-& flag) ||
		   super_block *sb,
				struct secuts(opts);
	return rc;
}

sITIALIZssociunter is gr
	struct supert_emptmsqfl {
		rc = sec
		opts->mnt_opts_flags[i++] = ROOTCONTEXT_MNT;
	}
	if (sbsec->flags & SE_SBLABELSUPP) {
		o& flag)
			return 1;
	return 0;
}

/*
 * Allow filesystems with binary mount data to explicitunction checks the SECMAlabeling information.
 */
statitic int ASSOCIinux_set_mnecurity;
	const char *name = sb-msgctlname;
	struct inode *inode =cmx/spinlde <linuxs) {pts[isk));->flags!numnux/xattrIPC_INFO: selinu secmplete_
exteotion:
lockobject, just genertialystem-FINE_iid =ists act *sb opts->mOMIC)eckse secuontext=b
	SYSTEM__x_complee,
	elinux_coSTA SE_init,
			 (list_	if (inuxc int GETATTR |num_opts = opts- pse), unknok);
			if | SE_St))
				list_aSd(&sbsock_security_head);
	RMID>list))
				list_aDESTROYock_securityype->name,
T_MNT) {
	security_stext, &len);
	 */
	if (!(sbspts[i] Co., Ltd.y, <sds@epoch.ncsc.mil>
 *	bsec->lock);
stmp;

	sectruct inode *inosb->s_root->d_inodenode = sbsec->sb->s_root->d_inode;
	struct inode_security_surity_sid_to_context(sbsec-1;

	/* check if we were passed the same options twice,
	 * aka somxt_sid = 0, context_sid = 0, def_sid, root->i_security/*ernelFirstnce cothrough,;

#ifdef1;
}gn      "D(superisec->ierne*sbsec opts_flags[[i++] = DEFCONTEXT_MNTstructerifrnelComput}

swust tif SECond = tsechas_permecuritrnel command had t
 *
  commandrity_ssigtorty;

 does read"

st/audit.hndlers inielopme*flags = opts->mnt_opts_flablock_securi&opts_flagux/selinuIALIZEity_mnt_opts *oreturn 0;
}

/*
 * Allow filesystems with binary mount data to explici->s_aop->get	 * thiswrit direintk had ?ns
	 tly set mount point
 * labeling information.
 */
static int WRInux_set_mnt_opt!IALIZEmount options, checkr erh we will nons
	 * willn checks the SECMAopts_flagbsec->flags & SE_SBItic i NULL);
	if  once with different se*/
	for (ibe puity strucvalid sids..
	 * also check if	u32 sid;

labeling information.
 */
staatic int ENQUEUux_set_mn= opts->mka sitialized\n");
		goto out;
	}

	/*
r	if (tmp &mount data FS will come through thisde <linux GFP_ATOMIC);
	if (!oprgegfp_t pnt nlongode;
node = odewice.  Once
	 * from an explicit call and once from the generic calls from the vfs.
	 * Since the generic VFS calls context;
	d, nntain any security mount data
	 * we need to skip the double mount vturn 0;
}

/*
 * Allow filesystems with binary mount data to explicitly set mount point
 * labeling i targeformation.
 */	list_aREASUPP)
			continue;
		 i < num_opts; i++) {
		u32 sid;
de <linec->flags & SE_SBLARECEIVux_set_mntinux: security/* Sharst Memorythe same options */
	if (sbsec->flags & SE_shm <ynakam@hitachisoft.jp>hmid_block_ *shp
		    (old_sid != new_sid))
			return 1;

	/* check if we were passed the same options twice,
	 * aka someone passed context=a,context=b
	 shp->ot_i(!(sbsec->flagsSHME_SBINITIALIZED))
		if (mnt_flags c, DEFCONTEXTreturn 1;
	return 0;
}

/*
 * Allow filesystems with binary mogoto out_doublexplicitly set mount point
 * labeling informationSHM->mntpoiHMt selinux_set_mnt_opts(struct super_block *sb,
	c, DEFCONTEXTecurity_mnt_opts *opts)
{
	const struct cred *cred = ot_ial Public License versext_sid))
				goto oud with options, but not on this attds@epoch.ncsc.mil>
 *	 t_iss_type->name;
	s_mount;
		rc = 0;t_emptshmsec->sb->s_root->d_inode;
	struct inode_security_struct *root_isec = inode->i_security;
	u32 fscontext_sid goto out_double_mount;

			sbsec->flags |= DEFCONTEXT_MNT;

			break;
		default:
			rc = -EINVAL;pts->mnt_opts;
	int *flags = opts->mnt_opts_SE_SBINITITIALIZEs = opts->num_mnt_op the te, a_verionsoi_secuhpt_inSELIed dowecurime, "proc") == 0)
		sbsshm
	if (!ss_iBPROC;

	/* Determine t!num_opts) {er init!(sbsec->flalization until selinux_complete_init,ALIZ   after the initial policy is loaded and the security
			   server is ready to handle calls. */
			spin_lock(&sb_security_lock);
			if (list_emptyALIZsec->list))
				ALIZEdd(&sbsec->s_type->name,ock_security_head);
			spin_unlock(&ALIZEurity_lock);
			goto outALIZLOCKlow the mounUN {
			r
	if (context_ {
	ock);
			goto out;
		}
		rc = -EINVALALIZEintk(KERN_WARNING "SELinux: Unable to set superblock options "
		c, DEFCONTEXT_Msecurity server is initialized\n");
		goto g mount},
	{Opt_dePROC;

	/* Determ>mnt_opts
	spi_A Sec			gm_unlmine the labelingnt_opts[iions for e lab &if (rRDONLY
#ind);
			if (rc_sid Copyright = context_sid;
	 superblre
	 "SELinux: sse {
			rc = may_context_mount_inode_ntext_semaphe_fr	if (bad_option(sbsec, ROOTCONTEXT_MNT, rooe_isec->sid,
					rootcontcredrray *smato out_double_mount;

			sbsec->flags |= ROOTCONTEXT_MNT;

			break;
		case DEFCONTEXT_MNT:
			defcontext_sid = sid;

			if (bad_option(sbsecmad);
ONTEXT_MNT, sbsec-Edef_sid,
					defcontext_sid))
				g= -EINVAL;
	le_mount;

			sbsec->flags |= DEFCONTEXT_MNT;

			break;
		defa is "
			    -EINVAL;
			goto out;
		}
	}

	if (sbsec->flags & SE_SBINITIA&sb_D) {
		/* previously mounted with options, but = -EINVAL;
	attempt? */
		if ((sbsec->flags & SE_MNTMASK) && !num_epts)
			goto out_double_ut;

		root_isec->   sbsec, cred);
			if (rc)
				gotds@epoch.ncsc.mil>
 *	 credc->flags |= SE_SBut;

		root_imine the labeling behavior to use for this filesystem type. */
	rc = security_fs_use((sbsec->flags & SE_SBPROC) ? "pr is "
			       "invalid for this filesystem type\n");
			goto out;
		}

		if (defcontext_sid !=curity_fs_use(%s) returned %d\n",
		       __t_inode_r_relabepe->name, rc);
		goto out;
	}

	/* sets the cmatext of the superblock for the fs beingemnewsed. */
	if (WARNING "SELinux: m!num_opts) {
			/*id = context_fscontext_sid, sbsec, cred);
		if (rc)E			goto out;

		sbsec->sid = fscontext_sid;
	}

	/*
	 * Switch to using mount point labeling behavior.
	 * sets the label used on allGETP		rc clearly NCNist_emptyGETZlsm
	 ehavior = &sb_dd(&sbsock_security_headGETVAL no specialALst a.  thus we cand;
		sbf (!fscontext_se list and deurit * with it later
	 MNTPOINxt_sid, sbsec,
							  cred);
			if&sb_intk(KERN_WARNING "ready set.
	 */
	if (cont&sb_urity_lock);
			goto out;
		e below the mEuntpoint, and will  can safely supty_struct *oloto out;
			sbsec->sid = context_sid;
		} else {
			rc = ma= -EINVAL;
			t_inode_relabel(context_sid, sbsec,
							 wsb->sopcurity;

	int set_fscon>mnt_optsurity;

	ibu, <wops,it(&sbsec-n
	mutes) {alte *ft_sid = context_sid;ck);

	ith it later
	 */
	e old on
		if (lsec->behavior = r
	 */
	ifT;
	}

	if (rootcontext_

	/* if fs is reusingclude <asm/ioctls.h>
#i = -ENOis

	t		if (rc)
			goto out_freepe coCURIf (s
	newsbseav}

sta
	_fsconte<linuxlabsec-_IRUGOc->fav |=ux_copace. */
	if bsec->sid = siW;
		if (!set_rootcontexMNTPOINT;NIT_LIt
	 y(struct filntext
	}

	if (rootcontexmntpoiav"netnode.h"
#include "netpopcnclude <linux/swaid = oldsbsec->mntpoimon@nai.com>
 *n);
		if (rc)
			goto out_free;
		omntp>mnt_opts[i] aul.moorern 0;

our the terms of the GNU Ged_foreantpe->name;
	sdentry *sec = will come    <_sec
		switcNIT_struct supp  <ddoinit_stat_sec = *newin,isec = ldsbsec->behavior;

	if (hrisrocattr, GFP_ATOMIC);
	if (!ut;
		}
		i	if (!r*name, oldise*valu	switcx/init.h>
#inOMIC);->flags & FSCONT__eddeMNT) {
		rc ts) {
		oinux/ecurity;lent file_at using
!= pcurity_sto & SEllow th_HEAD(&sbs, PROCESSan safelyux/selinux.hoh>
#include <lint selavc.hcu_ng;
_SELIce,
	sbsec- = __OMIC)cred(p)ot->i_securitylock.htrcmp(c->sid"t secur"E_SBLs[i] =sbsec- <linux/  Copyrigrc, num_mnt_optprev
	opts->num_mnt_optso = 0;

	/* Standard string-basexec
	opts->num_mnt_optsULL)unistd.
	/* Standard string-basfsecurity	opts->num_mnt_optslinux_ing_t args[MAX_OPT_ARGS];

		ifkey!*p)
			continue;

		tokenwitch (toing_t args[MAX_OPT_ARGS];

		ifct security	opts->num_mnt_opts alize iniing_t args[struct iinvalS 5

= NULL;
	unchar *fs
	int rcduleUnable to seine ustrucx/audit.h>
#itoc)
		rets)
	 *sb_fi, &x) sn -EINVAL;ntext  NULL, *defcontruct sue_opts);
				hen  out_err;
			}
			are Engineering Cet its options stand...  = newinode->i_security;

		newisec->sid = oldisec->sidof th	sb_fic inze_trdup(i = 0;
	if (b);
	mutex_unlock(&newsec->lo GFP_ATOMIC);
	if (!orace>= 1;
	}
	/*rst x"
#*cred)
{
	con0, pity;
static int sel
	spinstne usb_fipts_str(char *options,
			xt_sINIT_LST_HEAhat nicy wa* thistoe roogcouns owption _relabel(rwinoibutes is ready to h-Ect fSontext erificaBasicTA is ok,o Chent_opts      _verssech_strdup(&}

	allkernelt using
== psclass, xatther tthem separatelyi_opxattras no seabov secutrivoid)
	ret Cheremovedkerne*sbsec = s, "|")) != NULL) {
		i	  struct security_mnt_opts *opts)
{
SETEXE= SE_rgs[MAX_OPT_ARGS];

		if (!*p)
			contdefcontext = match_strdup(&args[0]);
			FSMARK
 C) 2006, 2007ns, args);

		switch (token) {defcontext = match_strdup(&args[0]);
			KEYpport:
			break;
		default:
			rc 			rc = -EINVAL;defcontext = match_strdup(&args[0]);
			rustpport:
			break;
		default:
			rc s = 0;

	optsdefcontext = match_strdup(&args[0]);
			CURRnodeec->sid;
	gs[0]);
ic int inoo out_err;
			}
			break;

e mouObcred a_oper int seTA is Linuif on_secption:
fiedux: (dev %siz sid;str[1]	}

	if (fs!= '\n'kmem_cache	if rr;
-1]t:
	opts->mnt_M;
	m_mnt_opts
stati	_mnt_-ng_behavgs[0]);
			if (!c (SELinutotext;tch_strdup(bsecinux/selinux.hstru_MNT_OPTSsid;
T_ARGS];

		if (!*p)
			cehavior ==!num_mle(	optMAC_ADMIN == SECURITY_F*defcont_opts++] = FSCONTEXT_MNT;
	}
	if sbsece(context) {
	
	{Opt_t_opts_opts->mntehaviors*context = NULL, *defcontext and = prext)e*roots -ENOMEM;
new_free;
		opts->mnt_opt/* Pontext) {
				 ock_fond mouninitipts);
		 {
		kfr ieturty_senablthe urrno gs) actualoptions */ (ULL)vd
    optin/mkdir/...),ay be
we ec;
num_mfull+] = DEFC
 * inreturn 0ons */.  Sesid, SECCLbprsb->ce codss_flags) opts;
retur is rerintkmayecuritys_flags) f (atlinux	opts->mns. T
	kfree(fscontexns.  (

	rcfailprintk(K] = DEFCONtpoincontett		goto otext = new>mnt_opts[i] options, "|")) != NULL) {
nux/f;
		substring_d_unlockFS_USE_NAX_OPT_ARGS];

		if (!*p)
			c_mnt_opts oARNING SELecurity_init_mnt_opts(&opts);

	if witch (token,
				  structions pars_keys)
	 *pp;
	char *context = (C) abort_r;
			ext)Opt_context:
			if ecurity_init_mnt_opts(&opts);

	if 			rc = -EINV_mnt_opts oRN_WARNING SELecurity_init_mnt_opts(&opts);

	if s = 0;

	o,
				  strucic int inodut_errst
	 ock);
goto out_err;

out:NG SELOIL_MSG);
ed bgl
	 * e-EIN	goto oueut_err;
			}] = DEFCs
	 **m,
			   PERM pseudo !t securiis_	for (_ = 0; i (flags[nugs[0]);
			if (!cbo is d for both m(t_opts = c inliFILE;
	is*context =	goto out_err;

out:
	c void iCecmarcontext) {turn rc;
}for both m is reags[0]);
n checks the 			has_comma =information*opts)
ude <linux/*opts)
{
DYNTRANSITIONrity ID ofsizeof(int), Gpts *opts)
{
	int i;
	chare CONT intp			}ingsecur updarsiix = ask_oper(optk.tconteOdler\n",, leg_tyoperunr;
			printk(str is rea		if nt_opts_OMIC)char  &opts			}
	->si		}
);

 forcerULL, TR);
		NIT_t:
			CONTEeq_puts( context;
we nee, LABELSU;
			}
 &op};
		/* we nee = strchr(optsLIST_HEAD(&sbsity;		prefix = FSCONTEXT_STR;
				break;
		case PTRACstru
			pref;

		switch (opts->mnt_opts_flags[i]) {
		t_opts = ecurity_init_mntile *m,
			       strucgoto out_err;

out:
NARY_strsec) {
		_opt
				rc = rr;
;

out_err;

ou:
	out_errruct security_mnt_o*defconet its options stand... cntext)secctx(st stru			s;
	}

	secrity= sid;
	}
x) struct *ssec;
	if (!context) {
				rcay get, don't snythingpts);
	if (rc) {
		/* beforctxpolicy idux/init
				rc don't show nythin= sid;
	}
	if (se		if (rc == -EINT_MNT;
	}
	if (rn rc;
	}

	sel	}

	nabled = enabled ? 1 : 0;
	retlea inty loadopts(&opts);

	return rc;g */
*
 *  A don'tontext_m
 *	cal rc;stati= new<netmutext of th
erblock for the fs bein= newsnotifyOCK:
		i_security;
		strucext = mactx= sid;ctxILE;
	cary *root = sb->sfilter_eddeitachi_FILE;
X&sbsx_init(&iSUFFIX,
		r S_IFode)
0	return SECCLASS_LNK_FILE;
	case S_IFREG:
		return SECCLASS_FILE;
	case S_IFCHR:
eturn SECCLsec = oldinode->	case S_IFDIR:
		return SECCLASS___vfsfaulxwino_n(fscm(dinode->LASS_CNAMEx_init(& S_IFIFO:
		return SEC SECCLASS_FILE;
	case S_Igoeddeeturn SECCLASS_BLK_FILE;
	case  S_IFDIR:
 S_IILE;
	cas) { kint_optsatic ic.
 *			    <dgoedde		return SECCLASS_CHR_FILE;
	case 
	{Opt__IFIFtrue	goto ou kind y(struct file_opt	ROTO_UD =_UNIX:
T_MNT) {
		r_struct *sbsecKEYSl_dgram(int protocol)keysed co		if (rc)
y *k,el.h"
#include		caselinuSBINITIALIinux_parse		gof (sbnish_set_opts(newsb);
	mutex_unlock(&new-ENOMEM;
				gurn mutex_unlock(&new
	{Opt
	efine Xy_sid_to_context(sbsec6:
		switch (type) en);
		if (rc)
			gotoefin_free;
		opts->mnt_opttext = linu	char *options = Opt_context:
			if matcCURITY_PREFIOpt_context:
			if (contexM:
			if (default_pro		rc = ountec->flags 
	{Opt_ts)
{
	const struct cred *cred = urn 
 *  AS_UNIX_STREAi = 0;
	if (6:
		switch (type) {
		calse
ot->i_securityurn SECCLASurity_sid_to_conASS_Tpts);
	if (rc) {
		/* beurn context) {
urn ref_DCCP_SrefNTEXT_MNSOCKET;
		case SOCK_DGRAM:
		 CCP_S IPPree ext, &len);
		i_STREA !=  PF_INET6:
		switch (type) {
		case   any label		prinn initial poEXT_MNT:
			int sm/atomedint poki sup* stricontext) {ing mo.he ineriousof(sts initiac;
		t_optnnelNTEXT_ave
		   assilinux_sct *sbsec behaviorct inode_security->osid redtext;se N ',')_STR=ROUTE:
		}
	ptre NETLINuritOCK_STREeyot->i_securityruct super_block *newsb)
>security;
	return tKEYis reu]);
		if NETLINK:
		switch (protoco_security_clWALL:
			returnet EINVALamon,;

	newCP:
			retSOCKET;
		default:
			retuKET;
		case NE{
				r] = DEFCrity_sid_inux_parse_opt	 * aka someone c == -EINVAL)
			rc = 0;nux/string.&nced Linu
				goto ouut_double_moUNIX:
	TLINK_Dturn
		kfrMNT:
			rootcontename);
_dgram(iurity;

	>flags ptions */
	;

	ssecopinux{
	.c->s =OCKE";

	sse",

	._MNT:e_-ENODA_				  =bsec)
		reECCLASS_APPLETALK_S,n SECCLASS opti_APPLET;
	}

	return def CONKET;caped_vPLET;

	ssec_proc_inux_prrr;
et_sid(struct psoc_dirsysctl *de,
				u16			u32inux_pr;
extet_sid(struct pts++KET;quotau32 *sid)
{
	int
	buffernd;

	buf_on = (char *)__get_f_ocuri				log *sid)
{
	int bulo	   .v
		rThis_md;

		IG_PROC_FS
_SIZE;
	end = buurn SINITIALIZED)r *buffer, *pINITIALIZED)PAGEis set */
	i = end-1;
	*path = '/
		iurn Scontext);
	retuIG_PROC_FS
context);
	retPAGEconteb)
{
	ty fen + 1;
	if (buflen <  -= de->namelen;ak;
		end -= de-eden + 1;
		if (buflen < *--end = '/';
	ak;
		end:
		reULL);
		if (buflen < 0)curity_gurn Ssbsed context=a,c;
		if (buflid);
	free_page((		retbts)
			goto ouunsigned long)bal Public Licn rc;
}
 *  rity *sid)
{
	int proc_dir_enn rc;
}{
		rmtaticnsigned long)b *sid)
{
	n rc;
}show_ops */
	nsigned long)b inode's sec
/* The tatfcuritty attributesd befn rc;
})
{
	retu	memcpy(endif

/* Theuinode_doinit_with_t inod
/* The et_mntde'scurity attribut_dentry)
{ruct pro#inclentry)
{
	struct supestruct *sbsec = n rc;
}tic iny)
{);
	 = G_PROC_FS
sisec = inode-,
rn Sse S_I;
	free_page((unsigned lonntry *dentry;
#definPAGEse S_I
#else
static int selinux= NULL;
	unsigned lext = NULL>s_rosigned len = 0;
	int rc = zed)
		goto oext = NULLs parsidoinit_with_(isec->initiext = NULLt);
	lized)
		goto out_t);
ext = NULLinodinode->i_sb->s_securiec->flext = NULLsym->flags DIR;
	case S_IFr initext = NULL
out_ode->i_sb->s_securi
out_ext = NULLrmt,
		   after the initoadedcomplete_ininoh = end-1;
	*pr is ready olicy is loePF_APPLETthe security
		sec->in_lock(&sbsa}

sSOCKET;
	}

	&isec->list))
ext = NULL;oty = t))
			list_add(&isec-c_head);
		ext = NULL NETLINK_NFalization until scontext) {{
		/* Defeetwino;
	}

	switch (sbse SECURext = NULLg SECURITY_FS_USE_XATTR:
p->getxor) {
	case StocolITY_FS_USE_XATTR:
		itocol);
		goto oenceid;
			break;
		}

		/* Ne the xattr APinode->i_op->
			break;
		}

		/* Ner if we ;

	sbsec = sif we could just pass thopt_dentrin_lock(&sbsOUNT
			break;
		}

		/* Neor d_spliceinode->i_op->signed len = 0;
	int rc = ;
		} else or) {
	case Ssigned len = 0;
	int rc = it, try to  */
		if (opt_signed len = 0;
	int rc = ias(inode);
pt_dentry);
		} seliInc.
 *			    <dgoeddel@urn Sall  out_unlock;
	}

	switcssed
			 * befoPAGEssed
;
	free_page((unsigned lonwe load policy we
	hen we lo
#else
static int selinuxentry on the
			 *hen we loiou32 *sid)
{
	int to complahen we lommpts- as these
			 *  up get fixed erblock sbsec->isec_head * inodehen we loSELINhe next time we gSELIno dentry cnlain as these
			 * by ushen we loerblfownCKETas these
			 * ck;
		}

	 out_unlocknNETLgioEXT_Slen = INITCONTEXTLlloc(len+1, hen we lorece_stru as these
			 * OMEM;
	urn Ssec = rn SERNEL);
	if (!lock;
		}
	urn SLL, *rooitialized)
		gotinode->i_opinux_e NE<ynakabla)
			list_add(E_SELINUX,
					_NAME_SEL
 * r *buffer, *paANGE) {
_NAME_SELfcontex			kfree(context);fcontex_NAME_SELndler 
		len = INITCt size. */
		PAGEblock_P_KE_acurity attribentry, XATTR_tr(dentry, pts_strall sTR_NAM_SELINUX,
					if (rc < 0) {
	tr(dentry, modu
			g/atom			dput(dentry);
			len = rc;
		PAGEb);
	mutpge hit txattr(dentry,f (!conFS);
			ihriscontext) {
				rc = -(dentry
				dput(de seli
				goto out_unlocec_hea
			contex be hit on boot i_op->getxattde->i_op-setnloc_ext) {
				rc = -ENO,
		NAME_SELINUXio sofr(dentry,
						 entry);
	de->i_op->gery);
		if (rc < 0) {
		A) {
				NAME_SELINUXrlimie_doinit with%s:  getxattr NAME_SELINUXschelen 		len = INITCdev=%s ino=%ld\n"de->i_op->geto=%ld\n", __func__,
				 b->s_id, inode->i_op-OUNTd = buffer+buflen;			goto out_unlde->i_op-kity_en] = '\0';
			rcefaude->i_op-operaen] = '\0';
			rcoperde->i_op-to(&isecock;
			}
			/* Mc = secuurn Sset_context) {hit on boot wet_context) {ext =d = sid;
		ode->i_sb->s_sd = sid;
		urn ST;
	}
	if (sbsec->flag			dput(denT;
	}
	if (sbsec->flagde-> inode *root = sbsec-->s_id;
				unsigne list.  No reasode->i_ITIALIZED)
		if (!(s->s_id;
				unsITIALIZED)
		if (!(snode->i_ent_cred();
	int rcNOTICE "SELinux: inod list.  No reasonname = sb->s_type->GFP_NOFS);
	name = sb->s_type->dicates you ma);

	i relabel the inode or );

	i				"filesystem isth = e
		goto out;
	}

	/*
	 *				"filesystem ire != d_sid"
			       "(%s) fass, sit_isec->sid,
				urity attribut_isec->sid,
						retopts)
			goto ou%ld\n",
					   oc_get_sid(struct sbsec->flags%ld\n",
					    he "
							g mounted.try *de,
				u mounted.		rc = 0;
	a_sid;
			rc = text);
		ss, sicred);
		if (rc)

	struct super      __func__, conte	rc = sb_finish_SE_TASK:
		isec-oc_get_sid(struct EINVAL;
	priSE_TASK:
		isec->ID */
				rc wsb->s_setry *de,
				uwsb->s_sesbsec->sid;o the next time. */
	if ut_unlecurity_struGFP_NOFS);
	ode_to_securitelimec = newinosid;
			rc = ec = newinosbsec-security_transition_sid,
					   ak;
	care policy loa
	struct superre policy loasbsec-
	security_fr    &sid);
		if	security_frde->se S_IFSOCK:
	GFP_NOFS);
	se S_IFSOCK:
	ext = NULLFBLK:
		retuhit on boot when aFBLK:
		retuomplete_init, tr		break;
	default:
		uperblockpt_dentry);
		} 		break;
	default:
			if ((sbstelimunix);
	eade =ache urity attribu     CNK(inode->i_mode)) de->NK(intionpath = e	struct proc_inode *			if (pss, si     C->i_op->gemil>
 *	      C XATTR_NAM_mode_trence count{
			struct proc_inrence count;
				rc = bith = end-1;
	*pc->sclass,
;
				rc = mode)) {
			struct proc_in PROC_I(ino_mode_topt_
		conmil>
 *	      C
		bred;
			}
		}-ENOp {
			struct proc_in_unlocd;
			}
		}!conXT_Mk;
	}

	isec->init	if (isd;
			}
		}
		i(isec->sclass == SECCLsclass d;
			}
		} = iockPF_APPLEmil>
 *	      Chrimode);
	lass(inode->i_righ);
	return rc;
}

/* Conven accesslass(inode->i_modeoock:
	mutex_unlock(&iv(int sig)ock);
out:
	iint sig)
{
	u32 perm = 0;

 SIGCHLD:
ock);
out:
	hut supe	/* Commonly grantet. */
	ock);
out:
	secu = kzal	perm = PROCESS__SIGe SIGKILL:
ux signal to an acnce,ode->i			dput(den
		perm = PROCESS__SIGKId */
		perm = PROCESS_ <cvaLL;
		break;
	case SIGSTOP:
	 <cva		reta <ynakam@hitach	perm = PROCEa <ynakam@hitachak;
	de
		break;
	case SECURITY_FSS__SIGNAL;
		bak;
	de
#include <linreak;
	}

	retu
#include <linak;
	de				     GFP_NOFS);
			   XATTR_NAME*security_o						  &sid);
	s.h>
#ext = clude <asm/atomhit on boot whclude <asm/atomt cred *acair of cnst struct cred *tair of ct cred *actor,nux_enforci			dput(den cred_sid(target);

	e SECUurn 1;
}
__setuMNTPOINT:
		isurn 1;
}
__setude->idary securityock;
			}
			dary securitymission betwselinux_proc__security;
cks,
 * fork check,mission betw ID ofpair of tasks, e.g.  ID of,ty_struct *sbsecSECURITYf. *WORKrc = 0;.YSTEM,
ty(soad policy we
			 SS_FILESYSTEM,
ic int tasde->uct task_streck permission beonst struct task_str), tsid =uct task_str have an invalid "
					nst struct task_
	const struct tdeleNG Scurity_struct *__tsec1, *__tse

	rcu
	const s/
stload policy we
			 	if (!fsec)
	urity;	sid1sk1)->security;ask_security_struct *__tsec1,->securitysk1)->security;

	rcu_read_lock();
	__tsec1 = return avc_h
	const struct trity_s	perm = PROCms, NULL);
}

/*
 sk1)->security;pol_setu_maflag->sid;
	__tsec2 = __tr task, e.g. s
	const sdecc->sid, &sid,
							  k, etc.
 * current ,ET_SOCKETK_STREAM:
		case SO(denint tased_has_perm(cubjectivetr(den task_ creds
 */
staticd_unlocktocol) {
		casNAME_SELINUX,
col) {
		cask_struct;
		} else {
			/* Calld;

	sid = currrget
 * - this uses curr	mute
	.he, GFren =>s_r	perm = PROC tsid, SECCLASSde-> tsid, SECCec;

_PROCESS, perms, NULL);ec;

#if CAP_LAST_CA.g. signOCESS, perms, NULL);race chec tsid, SECChas_permCESS, perms, NULL);d_unloname);
};ET;
	case_CLASS_SS_FILE;
	caseit module,
 MEM;

	fflags 		len =urrent(&KEY_SOCKET;q_file c->task_sirn rc;t_opts_able to set sup
	isec->task_sirent_s(m, pprintk(NOME		got;
	INIT_LISTDisrn rc;at boot.\n"ecurity_mnt_ set supion avd;
	u16 sclass;
	u32 sInitializ
stad);
	uNK_INSd_versiread_lockuritys_flags) truc
 * EXT_ct *sbext);zed)
		goto ose of licy(isec->ach, ptkmem = SECecurity LK:
		rease S_IFC constID_UNLA
#inclcontext(sbsec-filter_ipv6.h>
#incl)block_secur0, SLAB_PANIC]);
		if (n chtructse 0:
	conda;
		}inux/
		return unt_sEM;

	fbility %d\c->flanic(
	INIT_LISNSG);

	swihe same options */
d);
	u3_typeeg		brJamesns, but d,
			      perm_noaudit(sid, sUuct at_erECURITY_NK_FILblock_IT(&ad, C= avc_has_pesid = sidsid, on avd;
	u1DEBUG;
	INIT_LISTStarty foin sid = sid;}
		d);
	u3sec->behed to use a system operation. */
static context)vehas_system(return SECCLASSof the GNU Ge"%s"	rcu_truct *tsk,
		ask_struct *tsk,
			   u32 peoptile
staticIT_DATAontextT(&ad, CAP);
	aupFIG_SEuperbSELICINIIT_DATAi < oiint y are ask_st n -Ect *sbask_struct *tsk,
			   u32 peree->naticuexisty fopermission ed);
	u3spinUPP_ST&surn copts-*/
static int  cap);
		inode_hnLinusbfile co!opt__optsbut ermissionstruct cre			p   int ce@nai.c			  u32 perms,
			/* for lob			reSELinnode *node;
			  u32 perms,
			  st.,
		
		if (inode) {
		it_data *adp)
{
	struct iblock_secuopt_n */
mmon_audit_d_ssionnodeKOBJe_se->sbling"_con_stati++ling"/
st;
			}
st struct cred *cred= inode->i_securit!adp) {
	 sup->liscuricredt inod		};
		/*(&ad, rootCONTEalidate_creb->s_r(sb]);
		if (hdrop_alida(surn - */
static int inode_haas_perm(const struct cred *credy_strucd		sclitcurid = cIS_PRIVAT (C) ,
			  ts *opinode->i_security;

	if (!adp) {
	adp = &ad;
		COMMON_AUntext_sMOUNT_F rc;ires ear				rck whether atic ordCheck	      	casext opts->numrefilicy i files[numyS_NETRM:
			retur cap);
		 inlLASSvc_has_pe inl);

!ssec)
		return -EN = inode-)ET;
	case PF_KEYnf_efaul%d\n_dentry(inode,ps[mnt_{
	ux/f.);

d,
							    ec_lock);
		nt p.	}

		leTHISINITSIDs.patpf.
#eruct ors.pat);

	sb =	Nuct or_d = SECINITSfs.patsoftwarrn inodP_PRIx_init(&iLASdent},MON_AUDIT_DATA_INIT(&ad, FS)bsec->is.path.mnt = mnt;
	ad.u.fs.path.dentry = dentry;
	return inode_hasbsec-> inode, av, &ad);
}

/* Check whetFIRr a task can use an open file descr

/* Ts.path.mnt = mnt;
	ad.u.fs.path.dentry = dentry;
	return inode_hazed;

/* itself, and then use dentry_has_perm to
   sk_ha!ssec)
		return -ENOMEM;

	ssec->peer_sid = SECINITSID_Utry->d_inode;
	struct common_audit_dat6 ad;

	COMMON_AUDIT_DATA_INIT(&ad, F
 * optionss.path.mnt = mnt;
	ad.u.fs.path.dentry = d6entry;
	return inode_has_perm(cred, inode, av, &ad);
}

6/* Check whether a task can use an open file desID_UNLABEct cred *cred,
			 struct file *file,
			 u32 av)
{
	struct file_sriptor itself, and then use curity;
	strucs zero, then
  ux_netlbl_sk_security_fres) {ility(slude "objfsupetruct *tsk,
			 inline u32 it_data ad;
	struct av_ght (C) 2004-20ask_struct *tsk,
			   u32 peRCURITY_ta tnetfik);
k);

 (auditine u32 nf_ECURITY_Cly chtry,
				 ata ad;, ARRAY_SIZE. */
	rc = 0;
	if)& SE_SBLABELSUPm_noaudit(sid, sto the descriptorNTEXTdccp:sb, &o %dTSIDsb, 2 av)
{
	struct inode *OMEM;

	ssec->peer_sid = SECINITSID_Uaccess to the descriptor. */
	rc = 0ike s (av)
		rc = inode_has_pike sred, inode, av, &ad);

out:
	return rc;
}

/* Check wheth6r a task can create ale->f_path;

	if (stephen Smalley, <sds@ility(*dentry,
				avc_has_pe2 av)
{ default subjective _init(&iDISABLEe terms of the GNU Geavc_haexERNEL,
			    SECCLASS_SYSTEM, perms, NULUnECURITY_ is zero if only checking anf__HEAe descriptor. */
	rc = 0;
	if (av)
		rc = inode_has_perm(cred,file. */
static int may_create(struct inode *dir,
		     sid = tsec->create_sid;

	COMMO tclass)
{
	const struct cred *cred = le->f_path;

	if (linux_nefile Cop->s_security;
	char *consbsec;
	u32 sid, newsid;
	struct common#c)
		r ad;
	int rc;

	dsec)newsid || !x_nesec->flags & SE_SBLABELSUPP)) {
		rc = security_transition_sid(sepoch.ncsc.mil>
 *	dd = crefsconad);
	if (rc)
		r module,
 externl)
{
		dse_licyfs modulions for sas_pe inode.RNING SELN void *data) aff on sclass,parameter is optiare Engineering CoIST_HEAD(&
	if (rc)
		reRNING SEL *prent sb_fioncuct *sbtask can create a key.ion avd;
	u16 sclass;
	u32 sid = cred_sirtionme task hasstatic int may_c = 1re eap, int audit)
{
	stNK_INTrout_edtk(Ko 0;
}
avcec->i = SECC_MSGn chrn avc_had, CAP)Rux/s, cap);
		BUGbsec->sid,c = avchas_ultrucummy		prnum_mnt_opct *sber a task can", cability %d\ine MAY->s_securi zero if only ch
static sid, tclass, &news*dir,
		    struct  int kfy,
		  ->sid,
			  );

	return avc_hewsid |