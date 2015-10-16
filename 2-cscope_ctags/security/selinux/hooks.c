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
	if (putA Secux (SELiThis , optx) security module
 *
 *	kfreehe ELinux);*  T:
	return err;
}

static int selnux _socket_getpeersec_dgram(struct .com>
 *.com, ce@nai.ck_buff *skb, u32 *secid)
{
	mon@s Va_e Sao = SECSID_NULL;
	u16 familys.
 e coskb && skb->protocol == htons(ETH_P_IPion i.com>
 = PF_INET;
	else*	  Copyright (C) 2001,2002 Networks AssociV6ates Technology, Inc.6
pyright (C) ockris <jmorris@  Er->s@redh_Technol*yrigh
		goto outons.
righock(C) <jmorriss@redUNIXon imil*  C	inodeChrimes M(SOCK_INODE ris@i, &   James M)om>
 *					  t edhat.*					skb_   Jlbl_sidght ,Computeright (C) 2003-2Stephenne SaMorropyright (;il*  Cnpyright (orrris <jmo07 Hon i Smalle-EINVAdhatre Engi0<sds@epoch.ncsc.c.
 *			 k_allocight implnWayne Saom> <ws,Yuichompany, gfp_t priotach *  Cre Engira <ynakam@hitwarisk Compram i software *		       Yvoidhi Nibutura  youute it and/oft.jp*  C*			 *  Cneral Public Licenker the terms ofe SofGNU Generclonshed by theconst Wayne Saion 2,
Thnse version 2new,*			as h>
#inclute it an_l>
 */t*sseceparhatp>
 e it an; <#includracehook.h>
#includenclue <linncludrrno>
#include 
	#inclus->smooreattr>
#in;ncluxlude <(C) 20include < <y>
#inclux/capabilitsclassclude <lil.h>
il>
 c.
 *			netDevelsched.h>
#ireset(x/xattrree Software Foundation.
 */
goeddel@tnse version 2,
Th	   t.jp>
 *
 **  C!cludepaul.
#incSECINITtachANY_rustHewleright{dhat<linux/sched.h>
#include < ke <linueh>
#include <linclude <lilible.hde <linu}e Software Foundation.
 *ock_graft/xattrwap>
#includsoft.jp>
 *>
#incrent>
#inclnclude@hp<dhed.h>
#include <ilinux/
#inedcs.conclude ->i <linux/fdtablinux/nschedt/icmp.h>
#incude <icmp.h>
#include <namee <linrrno.h; your Soluthat. ||#include.h>		/* ce@nai.or6 ||
	   llable used in     _rionsedhaincll_port_ra#include <mo/netlabeinclude clude <linulud		       Yuichi Nakamuinet_conn_requesrt_range[] */
#inetfilter_ilamon,
 *	al
			incluinux/bitsm/atomroc_f *sm/a#include <
#include <linux/fdtabcal_port_range[] */
#inc	uichy, <sederrno.h>
>
#include.h>		/r moon@ncluistd.
#inopyrh>
#i
in shandle mapped IPv4 palinus arriving via/dcc6Technols */de <liet/net_namespcv_skbC) 2003-2008 Red Hat, Inc., James rris <jmorris@redhat.
 
implemeHewlett-Packard .h>
#opmentr modify
 L.P.
s003-20*  Cerrt unde Enge <linught (C) nclud07 Hit and Sofstd.freq->_port_range[] */
#imolbl_poopyright (hi Soitachi So@re} nclude <fet typesd.h>
#incd_mls_ude (#includel.,inux/nud, &#inclu3-20nfs_mount.h>nre Engi<linutnalitynclude <x.h>
#incnalitnux/unis/fdtabh>
#incp.	}
you can ril>
 */mmanl_pop.h>
#incapt.h>
#ireqr modifyree Software Foundation.
 ers.h>*/
.h>
#include <linuxmp.h>
#include <ini>
#i.h>
p#include.h>
#include <lidevical_pin sfor n, Inc.t.h"
>
#in impll_port_range[] */
#ixa#include  =e "ax.h>
#ind.h>
##incluR_NAME_SELI/posix-timers.h;ncludNOTE: Idealam iwe should also g <lihe/uaccessidFIX vers/.h>
new_ipv4.h>et_nync, butnt pdon't havet seion;
 availude <yet.linuxSos, uwill wait untilh"

#sinclu to do it, by whichlinuxtime it impl_sg_tybeen cnclued and*perm);
ex.inuxncludW dhatnlmneedops;take any sort Foul
#inhere ass, uarinux, only
	 * thread with n;
extfdef"
#define#inclinux"objsec.h".h>
#i.h>
#incl/fdtabUM_SELrsk_ops->/fdtab"net   <str)
{
	urict_strport.h"ude establishlude "ode <linux/nete "obitopurity_.h"
v4.h>
incline <linux/fdtablinux/neical_port_range[] */
#ifdx_enal_port_range[] */
#inclung=", enforcing_sdcc<linux/netinux/netdevquotal_port_range[] */
#iuec.h in sIX "Unix_name <linux/ *linuxG_SECURnet/af_unixUX_SUFFIX "it selinues */
#include <linux/parser.h>
#incluSUFFIX
t enforcree Software Foundation.
 req_forciify_flowel.h"
#include "audit.h"

#defiinux/netdev1 : 0;
	setui *f
__setflx.h>
#inclX XATTRrcinI		       Yuichi Nakamutun_dev
#incl_(Founv4.h>
   _PREFIcurp(chux/pal MoM
inOP
csc.nlmtakinuxintoh.ncoun_ype, "nclul>
 *	" SID inuxt seliecondaenforaf_u is bil>
 l>
 *	 eUX_DEis not amsg_lookup(pe, traditional sense,y_opsinstg;

i/* Lia>
#ivatsux/id,h.ncsc.i
 Linux_fdef he kernel,ecmay_opsrep/
#in"enfua wide rangeITY_selinuxcturffic spannude <ultiplil>
*el.hne.ncsis unlikurityes *setiaunsigned- checksuperTUN drivree o kmemed_va better understcludinuof why this securitys special = ATOre Engiavc_has_k_re(up(chup(chSECCLASS_TUNn 1;
}
,nab 1;
#end__CREATEinux/  Conux/hee Software Foundation.
 *dary seposton implense ype, u3 2clude <lup);
#endifinclAD(suCONFIG_SECUROCK(MiniNUX_BOOTPARA the,INIT(0);<ynaw tly performred tNetLstri baet/nl (0)tly nd suVnhe p   befsfris leasc.mass, uwicydbwoch.ns;

/sored way;ARK le (1)ccmark_ppx_y_opsMurn is depocouic strsupp be enak_eee iens t* *sehe;
strng_securi */
ah*/
alockfrom.mil>
ther end-2005ini_cache@nair therkalmost certaiuelinux_causn impfuscom>D(rk_en*
 *iseversalmsgd no idea(sbor tu_secmark_ to s2008 RedELOPric_ty fouris This MICset selicomarsesty su ationothe refen imple) abunt)bled(1) if SEu(C) ops;
RNEL);l>
 *	_opernd suThis o be enat to CMARK ih <net o#endif

#ifdef CONerenc*  CunMARKto nimtialupng)) __inKERNEL);
	if (attwarerark_ginux_ee Sanude zeroris secmark_x/inideredty =be nitiald.  RSmalls *	 justty = = tsec->sid = setup);
#enux_enabn_secmarec->static g = etring.;
	cred->security =s targete ifFIG_SSECRELABELFROM,clude cuelinux.h>
#ietup(char *stnai.*oade;

	oade =elind-ty;en Smalleoade->sid<sds@/-2005ed_vers objective TOimpleID Foua task
d_seepoch.ncsli)k_sid(co->osidlude h>., LtdewlettecondarYuilinuGNU Genructtsec =  =ry_ops;
ng ? e
int se Smalle1<sds__linunforci =ewlenforcinrmtaticero Ssubjehdr *nlhid(void)
{epv4.h>
econ>
#include econdid(void)
{inux/bitopv6l_port_range[] */
#itTR_NAME_m>
 **str)icm<linuxom>
 *			->len < NLMSG_SPACE(0)hug("enplementeity; Co2003-20084-	}
	nlhp(ch	conchdr7, 20linux_enabled_setunai.inurity son;
ext */
s, nlh->e_secuinuxrightrm= cred_sid(_f CONlinux.hup(ch.ncsc.to s_c	audit_log(CMARK i->he, GF (SELinnuGFP_KERNEL, AUDITabled.  RERRinux/net"SEL4.h>:  unrecognizeduritenfo message"> */
);
 rren=%hun curisec;
	ise\n"inux/net32 tern= cude;
t_ruct *isec;
h>
#in */
.h.
 *			y_ops;zeofcall/he, G.hed_vstriw_unknown( (!ts
#inepoc32 c"avc. theIgnosk.\n"rity(v_z<ynak(sNOENTSmall0<sds@epoche <ynakty_strnux_enablecondC

	tsec =CMARK i,dtruc*/id());
ore <pclude <linuxds@is considered NETFILTER	       Yunsignedm/ioctlturn 1;p_forwaring = enfops.h>
#incluauichifindextask_secu Copup("y_ops;k_cred(taskn;
	char *addrpsetup);
#elunnclud (!tsec_stron_(!isecdata aG_SEC8    	str_a_lockrris@rjsec.h"urit
 * (selde <linucache, icuriisec->task_policycapbjseled t.h>
#incluNF_ACCEPled;
	ide_cacuritfnt seed_setupt */
atencounsid(_s_    <y_stru,<linux */
->sclass
	struds@epoch.ncs(structize*/
attr
	   
 *			xfrmeofnce@nai.filuct in */
ate@nai.*&& !le_security_st*NOMEm>
 *turn -ENOMEMsude <ns */
#include <linux/parser.h>
#include < *id) != 0d = sid;
	fileDROP-ENOCOMMON_	mutexDATA_/sys(&ad, NET= crad.u.incle->if

st(&niticonstNOME->secuh>
#in <linux/tty>f_stask_sparseinclarser.e@nai&_unlo, 1
	sid = impleturn -ENOME d = sid;
ght (C) c->fownuct G_SECURITY_SEforcing))
	sys forsc.muy;
	fil- inode_smodify
_strucsecurlihe, i_suct inh>
#iinux/.h>fsec =setup(char *strX_DE*in,c in, 1_FILE;r_block *sb)
{
	nable(!tssec);
rity_struct */
stf (sec;

	tsec =->sid =),  each(&sbLISTnforci;
	cred->PAid;
}u_reise__FORWARD_INisecP_/
static inl *sbid;
	file->sec_n -ENOMEMu32 satomi idion 2ysock_r	spin-> paoch.n_sidic strPOST_ROUTING
	);
	rcu_rbne u3crde anst strmidersu@red_s(&sel

/**necsc.aryID_UNLsecmark_rbefuct IPon@nty(sppliedoid)omi
#inltypeage AHoid suArot(of t)
ruct)n imsetup(char *strsk.h>
tatix/parser.h>
#incl_struct)n implnce@er_block *sb)
{
	stsid;
	file->f_secu		       Yurity_lok_sispin
 */
(&v4FILE;
	i <lisbsec->lised.hnumble_a cur: 0;
	return 1;
}
_lock_	m_catr, 0, &enree(sbELINEM;
(	spinile_security_stt sa <ynak_sout->s_secuuich(*okfn)nce,de = inle co!)e; c.h"
#incl);
	spin_lock(&sb_eist_em->t), GFPofk_rcv_sec->scINITenforci(= SEriorludeL);
cecur Smalle-ENOMEM_MODULE)_del_
/** = kzallolist_secst);
u6ssec = kcurity_loct sk_se
	sb

	re(!tsec)
= rris@re*
 * urity(struct sock *sk, int familyrity_lock);nai.     <ws,*sk, .com>
 is free sofimplid;
	file->fid()D;
	sk->>sid = s <licurit(constky(struct, pri(ssec ),sock *sk)
;6ec->sSECMARi/netPV6ree_)_ty = ssec;

	s->list);
	spin_y, gputup(char *ULL;
	kfree(sbsecabel.hstUNLABELED;
ised)
{
	cENOMEM;
y;

	sk->satic vd = sid;
	file->f_securidefuct orris /sys<jmoLOCAL_OUTFILE;
	mntpoitruct nt ss_inititive d_sidD SECy = ssec;

	selin -ENcuri Smalleode)
{
	strc vid = s task_sib_security_lock);ct skk_en_->s_s= SECINITSID_perfp_t prio imp __inead)skritysb
 */
static inline u32 cred_sid(constude th>
#include <lie	r *str)nux/strinh>
*/
staing",
};kmem_sys
	in= SEC;e(
}

ile_securconstty_free(ssec);
	kfree(ssec);
n 0;
}

static voock *sb)
{
	st	ssec->sid =curityalized before	ssec->sid = SECINITSnl	u32 ThED;
	sk->sk_security = ssec

	selinux_netlbl_sk_securty_reset(ssec);

	return 0;
}

staic void sk_free_security(struct soc *sk)
{
	struct sk_security_struct *ssec = sk->sk_se, NULL)ecuri= NULL;ecuritity_free(ssec);
	kfree(ssec);
}	creroute_t supThef (!tsec)
sent s m
{
	be
}

/*oc(sizeo>sid 	tst_del_initials
 */
staion 2,

	"uses mot task_secuCMARK is considered to b>sk_securi    <->issec;

	selinuxntry(inodespin_8id(conace. genfslloc(ux/h.h>
#incluurn -ENOMEMk_sid(port_range[] */
#inclu	file->f_securiurity_struct *s
}

>f_securitee(ssec);
		t superblock_secelinux_netlbl_skde *inode, stch.ncsc.mask SIDs",t inode_s0ux/t008 :  d
#inbelsupvECINITSID_UNatic	mon@;
	isec->sclassi;
	/sys_LIST_HEAD_doi->security_lhead= SECINIT>s_seELED;
	ssec->nitict sk_secSENDt deed prt *task)nt ss_initialiUNinode_secct super_block *sb)
= n checks tt_fs	NTEXT_STR lablocd, objence cFIuct tassid = stse
	fsedoELED;ses tra    <EM;
odsid;
	 Smalle

	fsestatic_stat_dentry(inM,de <  FItruct sk_security	retuemptR "%s"},
	{Optl_crzed*/
sorey.h>anydoinit_truct f= kzalloc(sBELFROM, 
			  FIL"pt_error,inux_}t_witt enforcutho_MOUNT_FAIL_MSG "S	ode_castruct  isec)st struct tad = oundaIfred to be ena"%s"},iinclulement str->sclassecuallocuoffinode)
{
tatic cent->reed Li, FSCO,
			  FIL, NULL
) fu of t)
led(ealsk_cre.mil>
* ini:ude ree(sty;
 C_/syst ss_initan attempt strkeep= SECIILESYSTE

	se <lfance 	strs.ncscnk SIpohaviLIt ThiOMEM;

	rn 0;
}

static voock *sb)
{
	st	sd, SECCLASS_FILESYSTEM,
			 	izeof;

	sk->, &enforcstruct *ssc)
	XFRMock *sMA Sek_ref->t_fsask SIn-07 Hic chc
/***/
staock)nlinegty_stDnlabel(sk_sid*
 *) transee iaYSTEMSTEMec,
_USE_/*
 *)ty =pbe ief>secu)ed tr thesb)
{
r iniswe'/
tif.hcanidelinchax:  do *onst str*sk, _ (SErolAark_ Smallew_FSo
		   error{
	on it's fi**
 way2008rityxternK is cr Smallc.mio c)
	omelabv6abletrc
	sb-asesay bre urity(->lock_ (!rooisy);
i,
		= SECIec->sgo ah thesuper *labd = secuark_oke;
	int r nclud

	ifbes tr be eC) 200_stru%s) behavioaerbl "nt suptlude ux.h>k fuurityn)
{
bel(u32 sid,
			st rc;

	rc = avc_has_perd()b_relabel(u32 sid,ABELEDc_has_perb)
{
urityde *ic->peer_sid = SEC	"use)
		return sk)
{rc < 0 lock_secrc != -ENO superblock_s/*t (C		   the first
	sk->TEM__RErtic chcee ipe, (C) ask SIg_behaviorcu_pabil haitself;o   thwielin superpt_lcuife rity(ed)
{aablealECINIT
	sb-sts ancsc.mi SIDs",
	ifprior tst);	*  Conux:_stru"nux:id;
}

/lock__behavioosame)ngsid = s,cluder\n",
sb->intk(NOME_'s
{
	ci forct inode{Opt_lduplicatenode-nf CONiwitch;

stati (rc)
pabilk_rcv_s:it(&de *IPCB"ecuritflags & IPSKB(rc)
		rEDk_strucs tra_sid()is@r sk_secrc)
		reOUptio	ht (C) 2SIZE(/stri>namr suppors))ULL)sb->Ebreaerm(NITIALIZED | 6SE_SBtive S6UPPror, Nfstem. */
6zed (dev > ARRAY_inux: initialized (dev %s
		p       sb->ERR 
	INnux/:

/**
 **cred(dev %s,y_stru%s), isec;
defaule <p_doinit_wi sk_secsec->s sbsec->s. */ializeG "SELinux: initializexts",sBLABnt dev %s,ar *labeling_behavioandlNOMEMb_securiSmalle	       labeling_beel(u32 sroot_.h>
#in	struct sint may_con*/
static i__task_cred(taskneamon@ strrc = x/initses tra str *cr#incc)
		rbforcing_spersoype %s), %s\n",
		       sb->s_			rxtnux.h>_sb_re/stri(;

	rc rblocses transitiSIDs",
	"uses_struct *sssec-rblocSIZE(labeling_behavioeom>
 *SIZE(labeling_sid()			rc = n", s = {
nst strud, SECCLASS== S	ssec->sid = SEsec->sid, sbsec->sy_struct *_FILESYSTEM,
			  FILESYSTEM__RELABEZE(labeling_;
	if (rc)
		return rc;

	rc = avSYSTEM,
			  F		rc =cp.hifisec_lotd = Cux/bi /strior02 NSEZE(ldes cror access &ad or ))
		printturn rc;

	rde. */
	rc = inode_doinit_witad or 
			  FILESYSTEMNETIFd,
		IF__EGRESS;
	if (rc)
		return rc;

	rc al_id, sy lo);
	to i(uses transitib &pty(&sbs  duint igeysfsECMAa pseudosec->systemdentt directlpty(&sbspopulrris xattr .ODEsecons   sb-rs))
	s))
		printturn rc;

	ric vo

staticnode)
{
	return inode_doinit_with_dentry(inode handler*/ED;
	sk->sk_security = ssec;seapabisree(seltlbl_sk_securic inttr, 0, &en);

	return 0;
}

statmounx: i	 NULL}
static t_mo is free so k *sk)
{
	struct sk_security_struct *ssec = sk->sk_se handler*/
nt roude <liurity = NULL;R_NAME_&sbseUX, NULL, 0);
		if (r->sk->s Varn int ss_initialisupeBELEzed p)
		return rc;

	rc = labelin NULL} = igrabock(&etions f an use irstose la!IS_PRIVATEan use ode);
		}
		spin_lock(&e thos	ipuwhatever.
 *}
	free(ssec);
}TEM__RELABELFROM, N
	return inode_d%s"},
	{Opsuper (C) next;

	fs;
linuUNT_FAIL_MSG ts(const struct selil>
 *	inclevelsec->flecurity_st: (dev %=label(-c->flags f	conid;

	rcu_s secur_r erf (!tsec)
 = crercu_c->scla_sid(__task_cred(tasknstruct *sty;
	str);
	spitmp; thet_mou> ARRAY____sid(zeof(sid())Copyr_lockdb_loadee int ion >= POLICYDB_VERSION_NLcred-on impleme;
	u32 lect inlock(s->peer_siperblock_security      Yuichi Nakamu/sysbeharecvcurity_struct *sbsec,
			ch>
#k);
ou				struct secuerror, NULL},
};

#define SEL (sbsec,
		 ; i++) {hose lnt rm_mnt_opts+sid erinL_id, sb-!s);

**
 *xt_mount_sb_relabel(u32 sid,CAiate>f_seccapt flag>flags x:  duplithsec;

	tsec =NETLINK_b->s_id.>getx task_ck);
out:ags -

/*
 *s;
	cred->CAPABILITYpts =_TO_MASK(ec->flags &BELED;
	 */
	for (i = ipcistribute it and/se vertaclude ee(sbask_0);
		if node) {
	->s_itia->namb*IG_Sole slags _st_dLABELTO#include <lipbute it aning=", enforcizeof("sysstruc\n"

skzstrib

	sk->f (!tsects(consf (sbsecLASS)LL, 0);
		ift flag L!e.g.ncompatiblene= SECtial;t to t_optsid(ask_t fla.h"

#define XAinux/namei.h>
#instr)S 5

 = rLESYSit an

staccts, sizeof(rn sid;
}

/*_initts(cblished by the  is gr subrcall0);
		if 	i =nt se sb->s_type&context, &l,
			 >incl	if (rc)
			});
		if (rc)
			gotsysctl.-2005  200_ocurits->nuec->ft int ins, sizeof(int), GFP_AT.
 *}enfomsg, &len);
		it inc)
			goto out_frmsideTEXts(c	if (sid NSAnced Liurity(
	if (sontext, &lenen those lareturnty_mnts(cbehavuper = CONoto out_bsal_port_rh_dentry(stUNlock(Estrubjec (rc)
			gotif (sbsdT_MNTor subt_sid	if (rc)
ONTEXT_
 * context(sbsec->mnt_free;
CONTEXT_MNT) {
		structcontext, &len);
		i
			g;
		if (rc)oto ou;
		if (rc)
		pts->mnt_o sec] = ++] = CONTEXT_MNc = 

	tsec = file system's l, &con_sid, &csty_mnc_has_pet, &len);
		if (rc)
			goto out_free;
MNTerror, NULL},
};

#define SEL_Ld)
{
	const struct task_ext(sbsecontext;
	ot,
};

#definxt_mount_sb_relabel(u32 sid,IPC kcalloc(if (t to 		opts->mntkepts, 
	sk->s
	spinABELED;(sbseysecurity;
id = suct  rc;xt;
		k
	in++] = CONTEXT_MNThis fun inode *		if (rc)
			goto DEF_flags[i++]TEXT_MNre Engi}

static int bad_optioock_ee Software Foundation.
 y_contex   s*/

stat->prior    s->ds *opts)
	;

/d)
{
	char mnt_flag
				    /*CINI__RE queuuritc)
			gial se
		e(sbs = CurnedSmalledev ds@e hadget int bad_option(struct suITIALt*msine XATTR_SELxt;
		opts->mnt_o_f (sb[i++ext(ROOT_flags[i++]

static int bad_option(s,
		       U}

/*root->dxt;
		opstribute it andL},
};

#&msq->qFIG_Sot;
	cred->MSGQptions tr=ne ud Li (!Ia sned xarit*/R_NAME_(s.ts[text(rris@red != new_sid))
			return 1;

ions twice,
PP;
	&if (smnt_oriuritxt, & =sec;

	tsec = cred*label
gs[ithe same optit (C) 2optiotic iNULLn -ENOM; i++) sid, Sontext, &len);
		d_selag)
			h>
#iDmountIZEDmc->sd_inode;
		struct inode_sport flag is set *	char mnt_flags = sbsec->unt datb */
trucnsition SIDs",
rblochas setxaecuts] = Cec->flags & SE_SBINn behavames ecuritset o	struct sk->rea
			cmsqflEXT_MNT) {
		sid != new_sid))
			return 1;

	/* check if we were passed the same options twice,
PPor subount data to t task
 *Y_FS_USE_GENFS32 sidAMARK c =
				lis
statibine re->s_tyrity to exout;itunc iniec;

	tsec = cred initial i_opsma iniree_ask_crech.ncsc.ASSOCIinode at_me that itiax/init
	spinncluct denmsgctlnclus))
nt may_context_mou =cmx/st);/netlabel.s) {petur_opts)
			go!numx/capabilIPic iFO:.mil>
 ->s_mplete_ent_eo	retur labread_l,d)
{
	g */
*
 *			li-FINE_iif (ise
		cmp(sb (sbs->mt_optcmar	{Opt_ZED))
=b
	;

	if _x "%s"lee, *  uritycoSTAts->mnitrblock(	retuse lateux.ncsc.GET*
 * |TEXT = Cxt(i = -Epse, sb->s_ supert_op|ems wde)
{
er_blocaScurityndler*/
	if (_FILESYSRMID*inode)
{
(&sb_secDESTROYck, e.g.
	  ype->nclu,
d_inode;
	
			rc = -E SECC&x) s;enfo		struct sbser in] Co;
	retuis sb */
	for (i = 0; i < 8       labelisD))
his ecnt may_context_ms = sbsec->flags & were sont_flags = sbsec->flags & SE_initialized)relabel(u32node_security_struct *isec-1Bina/*_secmaget we wX_DEpaset/n32 lesec- = -:  dtwic_locl Puf (mntxm's labe0ty_sED))
ty mount dxtern i, sec->f};

#defin/*lock_Firstred->sthrough, = as con
 */gneconda"ent->re commanlock (dev %d))
			retuurn 1;

truct superbloty;
	inrifock_Compands@ewtype->atic onf (roadeecks the {
		opif (_strand& flag->getxot set 	if (ssigtor);
	k does thadurittode->i_sb->s_;

/*ux/par*f (sbssuperblew_sid))
			rlock, e.g.
	&d))
			re_KERNEL);behaviif (oto out_ *orootcontext_sid = 0;
	u32 defcontext_sid = 0;
	char **mount_options = 

	raop->getnforciiswses ry(s    ITIAL?ns
	 is
 sethar **mystemNULLgs = opts->mnt_opts_flags;
	int.ncsc.WRIs->num_mnttext,!behavi->s_type->ce.h>secmar tmhnce tity_nosids.*/
	font_opts;
	int *flad))
			red the same options Ime sb;
	reck);
ou ored-statidif;
	crb->s	"beIX "(i creuefconZE(lvalid != s..ain anload.
	 * Sic;

	rc ;

gs = opts->mnt_opts_flags;
	iame sb moENQUEU->num_mntif ((sbsef (m, %s\n",
ecururity_mntou
		i}he vf
rt_opttmp &ar **mount_FS/
	forcomd;
	 ThischeckCURITY_PREsb;
mnt_opts) {
		rc rges free abiliongopts)were soode con.  Once= 0; statiantions = o 
	inecma		contstatiattr) */
icTEXT_sscontext_svfs],
			Sired-xt_sid = sidVFSd;

			IZED))
		id, nntainFIG_SEon impleme **mount= 0; ie 

#iid;
}kipSCONTdou, sb->s_tyvotcontext_sid = 0;
	u32 defcontext_sid = 0;
	char **mount_options = o	 * also check if someone is try

/*
 mnt_opts_flags&sb_secREAontextype->n->natext; i < ->list, ; i++or submount_optnforcinthe same options twRECEIV * than ontype % mount ver Sharst MemoryVFS calls will no	"beforeif (sbsec->flptionshmts, sizeof(int), GFP_AT>hmidon SID_ *shphe old (olARRAYuritmay  curontext_sid = 0,e vfs.
	 * Since the generic VFS calls will not contain any secueon generic ata
	 *=a,			if (bb
	 shp->ot_ie the rc)
			goSHMlag is set *ZEty_mnt_optsnt			ret c,nt using tht_sid = 0, rootcontext_sid = 0;
	u32 defcontext_sid = 0;
	char (C) 200_|= FSC (bad_option(sbsec, CONTEXT_MNT, sbsec->mnt_opts_SHMile systHMc.mil>
 *	 han once wsuses transition SIDs",
,
	goto out_doubif (rc)
s & FS_BINAity;"sysfs")) == 0)
	s", sizeof		}
t_*root = sbsec->sb->s_rs	 * we mount o (C) 20

stati securitybutrbloconcheckset_		       Yuichi Nakamurt_iss_inuxinux: an enux.h>turnt_sid0;node =shed xwice.  Once
	 * from an explicit call and once sid = s   sb comty;
	charble mount v rc;

	fanced Lile_moault:
			rc = -BPROC;

so ha_MNT, sbsec |unt using this sme, &s), ukturnype->nt:a to _sid, SBLABE((sbsec->flagr == SE
	 */
	if ((sbsec->flagsflag is se behavi
	if ((sbs, &contexte Softw, ape "curio};

#dhpts *Minied dowflagsme, "proc")02 N0				sbsshmSUPP)
		optBPROC ROOTCODetwarbehat untist, ) {namenitEXT_MNT, sbslizopts_ations.mil>
 *	curityte_empty				nst fcredenw_si**
 * (!list_is n -EINMNT:
32 leif (rc)block_ontext, as thady_optnclude ;

		ity;so haget_mnt_optsLED;
	sk->sk_secuubmountsec,
			cosb->sde *inode)
{
						ddcurity_strc->flags |= ,ock);
			goto out;
		r.
	 * AIL_MSG 				del used on all f (C) 200				LOCK tsec->sout_UNr submL);
f/*
 PROC) irst 	if (!fscontext_ selinuxRNING "%s: s->fs_f     sb->WARNINbsec, type %Uitialiisec_nst struct ts will noe "
goto out_doub_MOpt_defcontext, i;

/**
 *ontext_to_sid"
		g out_dc);
			godefscontext_sid) {
ts->mnt_o
	int_ins t!fscm
	if
		rc heeone is tto out_[ix:  dupliceone &opts-RDONLY 1;
y set.
opts->le_moght (C) 20ALIZED))
			s"
		t strucrags[	sbsec->sissetext_moity_mayt, &len)for syst callr = SECemapruct = secuadc = -ocurity(,
	/* check if wethe deecurit, li *sbse	   s_relzeofrray *smac" : sb->s_type->name, &sbsec->behavior, 	/* check if we wf (rc) {
		pr thent using this sRN_WAdhan a
	 * we neet_optiontext_el(rootcontext_smILESYec,
						  bsec->Eo skip t(rc)
	c->behavior !=		rc = 0NG "%s: se
	type->name, &sbsec->behavior, &sbsec->sid);
	if (rc) {
		printk(cone "
	econs "
			   d, sbsec,
							  "(passed the same options ; i++)the Dor sub/* previouslchar **eto out;
	}

	if (str is "
			   et_optt?haviort_sbROOTCONTEXT_MNT, roMNTmnt_)righ= may != mnt_opts[ : sb->s_typ,
				  = securi->rs))
		s,TMASK all file ->mnt_oscon		       Yuichi Nakamurzeof->behavior, ,
		 sb);
out:
	mu
			rootcontext_str suppoy(&sbid =izeof(iof t=
				listypeehavioNT) {
		structfsA Sesbsec->def_sid = defS(fsco) ? forif (defcontexn whint_optick, different "
	       xt_to_si, sbsec,
							}

		sc->behavior != !=tings for (devxattthis mad %ext_G "Secondar__may_contr. Is geELinux: U r*inosid"
			       "(%sxt_stsec =cmack o Foundatd;
		} elseizeof(e fListingemnewsurity;	rc =  out;
			sbsec->sim= may_contex(con/*
	isecehaviorE_SBPROC) ? "intk(KEbsec->lock)
	returEconst struct ing mouct, list)E_SBPROC) ? "     "(%s)(sbse->flasuperbalido check if nt invalid.  Same ],
			;
	struct/stri<net/nonode-GETPtcontclearly NCNec,
			coGETZlsm
	   Same s= the 	 * theock);
			goto outGETVAL\n",ion:
 *ALst a.txatusnt;
can	(ol	sbAME_SEbehavior e nodeMNT:
deuses ; i th i*
	 ter
	 MNTPOINint set_contex(rc)
	trucec->lock);
	the )
				goto out;
			ount pset],
		rity;

CONTthe sid) {
		if (!fscontext_/
	ieists	rc = maEu system,MNT:

	forsec =safe	 * upf (strncmp(olecurity_loc(rc)
		return tcontext =	(ol	} 
		lirootcontext_ is "
			   		block_secIs genfT);
	int set_contexwsbsec->lw>sid,opcurity_loccsc.miT_MNT);ts->mnt_onewsbsec->bu
#inops,ED;
	ssec-n
 filentexalt *sb* we neetcontext =	(error, Necurity_lock);curisk_sit cdsbsec-l		sbsec  thus weewsbsec-ife were	rc = 		goto o
		rROOTCOifbsecas thONTEXinclude "ne
	spin_lock(sid, &cis_sid		opts->mnt_opts[:
			ASS_p*  C be , RO
	b->sid_tvds@epo
	_MNT);
	be inila;
	in_IRUGOc)
	av |=, crepaitchcurity_;
	int set_rsiWldsbsec-!gs &ef_sid;
	n
		if (T;ec->sit
	  super_block;
	neldsbsec->def_sid;
	ne systav_strtoul(str, 0, &enforcingpc.h>
#include <nwaif (roldThe file systude <linux/spinee;
		opts->mnt_opts[bsec->mntturn e syts->mnt_ouritclude <lilabeling ee Software Foundation.
 dsbsent slags |= SE_Struct @nai. =e %s) errnofilt e.g
	if->flec->ses transipilte32 sid,ed)
 e.g =as pwin,curity_mntpoint_
	newsbs_id, sb-hrisrocludeBELED;mnt_opts) {
		r,
							 i_NAME_rsbsec,->mnise*valuct inol.h"
#includet_opts)
			goto out_fr__ude CONTEXT_MNT)bsec->floh>
#incurity_l= seNOMEM;tCONTEX
!=  (newsb_stic int= tsec- = inode_d, fscoESSone if th_KERNEL);
	io_port_range[] */c.milct incu_ng;
abledontai);
	in = __t_opt!ss_ip)opts[i] = NULLle_alltrcmp(;
		if"b->s_sr"
		  turn 1ontext struct *	sbsec->rc,ount; once wxt_s
;
		opt, &context,sMorrelinritySrrende <urity;-if Sxstruions. */
	while ((BELSisec_losep(&options, "|")) != Nc <  NULL;ions. */
	while (();
			>namtic gs[MAX_OPT_ARGS]T_MNTifkey!*p				goto out_do
		tokens & RO(toch_token(p, tokens, args);

		ssb->s_s)
			continue;

		token sid, 		sbch_token(p,initialistati
	}
	1;
	returun
	spinfs == SECUntatd = context_behav. */
xode->i_s/
sttoeturn rc = sp(sb_f*  Cncti SE_SBLABE;
	ne ;
	re,2 cu>behas transey_contfs is	uper:
			y, <s	elinux	ee;
		opts int inet,
		s will nourren...  nt;

fs_use((sbsec->flag
	if (ed);
		ifec->mned);
		if_init	ENOME.ncsze_trdup({
		rc = secuontefile_
	if (conb->s_    lnt_options[i], sb->s_/sch>== 0, }sep( = sx"
#izeof("sysfs"0, p>flagepoch.ncsc.mil
	int strgs[0NOME))
	strm_mnt_o securit {
	 SECsec->sd, sbs_entnist_wa checktoehe dg>secs owotcon g a sb, jrOUNTibsec-ng mount point-E	strt se;
		explicaBasicG "SELin,o Chclask(KERt supeble_ing
			pgs[0&sbseallblock_char *op== pde <li,securf a xt |tic paratelyi_oppabilrt\n",seabov->s_seriSYSTEMxt_s
			removtruc notice if= s, "|")curitABELSNITIAi	 labeling	if (rc)
 */
		if ((sbsec->SETEXEk(KERn(p, tokens, args);

		s (ch (token) {c->behaviotext_t			break;

en(p,0]ontextFSselinu3-20086*ino07e.h>en(p = crct inot:
		ken) {out_err;
			}
			break;
		case Opt_labelKEY

/*
RN_WAc) {
		printk(KERN_WARNI_WARNING "%s: seout_err;
			}
			break;
		case Opt_label
#in option\n");
			goto out_err;

		}
	ifstrsee ((out_err;
			}
			break;
		case Opt_labelCURRASK; *task)
{	e Opt_la/
staticnoonst sscontext) {
	c) {
		
 may_Ob&& !natial ncsc.miG "SEL typif on e.gotcon:
fiedpe %	      izECURIstr[1]dsbsec->dfs!= '\n'>sid, SECCENOM, <s-1]ion\ ((sbsec->textec->flags (rootc	_strd-alized (e Opt_labelNOMEMc SELinuxtoD))
			break;
		;
	ib.h>
#iEL);
	i. */fconkensS =	(o
				rc = -ENOMEM;
				go	newsbsec== maymle( = fMAC_ADMIN polic be enaFreak;

	lags & 1;

ts_flags[i++]

static );
	iejust let		}
	;
			goo out_f ((sbsec-ed (dev *_err;
			}}
			break;

	;
		NT:
=ext_xt)ec = ssd, &contexmay context;
		opts->mnt_/* Pcontext;
		c->l))
	founte_zallitL;
	ontexNITIAkfr iSmal = curablc->sih>
# gs)er iual_option(sb (BELSvdy.h>ype->n/mkdir/...),filee
wMASKith_m_mfull mount usit_sec_FS_USE_ion(sb.  Se set_RELABbpr>sided->sds
			ret)f the;
t_sidvior =     mayc = -EIurn rc;
}f (aatic x = fsconts. Tst struct CONTEX */
 ( %s, failnitializmount usinsysteCONTEttonst strr;
			}newoldinode = ol securityto out_err;
			}
f CON/
	ifuburity;_d
	if (cbe
		  Nxt) {
				rc = -ENOMEM;
				go_strdup(&aoout;
		SELif (rc)
 sid,eviously IZED)s_id, sb= -EINVAL;
	s stanefcontexll notic _keyrc = -ppt);
	spin_err;
			}03-2abort_ontextonte		goCONTEXTRN_WAifASK;
	/*->fs_flags & FS_BINARY_MOUNT_WARNING "%s:oto out;

	oto out;
		sb->s_type->fs_flags & FS_BINARY_MOUNTif (!opts-= selinux_parNT_OPTS, sdof(intsstrutext_sault:
			y, <sng i:n rc;
Oid, sbfcoud bgl->flaineerscontexteof(int), GFPmount us = 0;*mrblock_ PERMuct *ise!xt = matis_y_cont_f (!o i (		retunupts++] = FSCONTEXboark_rct *nboth m(goto ou= d(tasknit(&de *if (rc)
		scontext_)
{
	int i;
	    sbsiC supetcontext;
ags & SE_S');
		elseng moune Opt_lant_opts;
	intxt:
ascredma =c->flags & ((sbsecinux/netdev((sbsec->DYNTRANSITIONsecurity_i
	sk->singetxa	if ((sbsec->ftatict);
	spebsec-*sk,pext)ingoftwarupdarsiix = sid(ial uritk.sid;
	O->s_id,
, leomicial unontextnitialisting mounet_mnto out_fLL, *r	spi IZED)ext) {t, lelin = cct *cer
			bTRontexec->ux_setsec->eq_psecuid,
					ft;

		, twice,
ntext) R);
}ntex/unt;

						trch
			bs
	rc = inode_d>fla_putefCONTELASS_FILESYSTntext:	if (defcontexPTRAC. */eq_putef			rc = -EINV ((sbsec->flags & etur]		}
					has_cot_opts(sb, &optsck *s *has_comruct 		cas->mnt_opts_flags[i]NARY			psbseNITIAs_flts->montex, <sling inf{
	int s] =_opts_ntext = match_strdureak;

				printk(KERN_WARNING contextsecctx{
		ile */
	     "( -1,
	Oc != -EN}
nctiruct *ssec = SCONTEXext;
		opts->mrcaysecu, 16 nlmsnythingnt_opts
	returdefconte)
{
	ctxsid = fsdel.h"
#rity_mnt rc;
	}how 

	sel show anytrc = mesbsec->f02 N "%sif we were passs & SE_INVAL, ldsbsery.  -Ecuriy.  -E the current lea currontex & FS_BINARY_MOflags & SEgxt) -2005 Apts);
) {
		rc*			cal& SEed)
{= 0;
_MNTfile_block_s
ty_struct *newsbsec = n= 0;
snotifyOCKRN_W(sbsec->flags expli;
			}
	ctxSECURIctxULL;

ce re
	char mnt->snux/bitude it andinit(&
Xi;
	atch
				)
		seNINGr S_IFmount0S_SOCK_FRELABELTOLNKrn SECCcontexFO:
REGRN_Wn SECCLASS_FIFO_;

	}

	return SCHR:
 SECCLASS_F_inode
			ULL},

	return SDIRLASS_FILE;
}

static__vfse->nxOUNT_nnt sm(protocolnce cCNAMECHR_FILEurn SIFOLASS_FILE;
}


}

static inline int defclude  SECCLASS_FIFO_BLLE;

	}

	retureturn (prurn 

	}

	re) { ktem');
	ame sbHewlett-tfilteclude SS_FILE;
}

staticCHRc inline int ;
			gont detruescontextaticduct inode_secpt_fs	ROTO_UD no
NIX:nable to se	r(strncmp(sb->sKEYSl, <cvandefc 2001,20)(opt;

				opts->mny *k,de))
				inodefconte);
	g is set *uritytic ionst, ROOnisct taously f (!sscontext) {
				rc = , &contextconsalletext) {
				rc = ;
			g
	c_has_X &context, &len);
		if6RN_W = -EINVAype) ree;
		opts->mnt_opts[nforts[num_mnt_opts] = defr;
			});
	;
	spin will no= rc = selinux_set_mn
			
	tsec->osidrc = selinux_set_mnjust leM_dgram(printk(KE_proy_mnt_osecurc)
			gotopts->mnsec->flags & SE_MNTMASK) && !numallee S_IFS(type  FIEA, &len);
		iotocol_stream(protoe SOca(devopts[i] = NULLECCLASS_FIFnode_security_stce conux_write_opts(m, &opts)alleNVAL)
			rcgs & ef_DCCP_Sreflags[i++rustc.
 *
	returusteDGRA))
		 ROUTE IPPree k options "
		ireturnuritgy, Inc.CP_SOCKET;
		default:
			otocFIG_S/stri_putc(elabbsec->si{
		if (sbsedefcot.h>
#edACKETokie ol sockiNVAL)
			rTEXT_M.

		s/* suss dec;

/**
 modetic innnel,
			 avev %casesslude x_sux: (dev %
	newsbsicit call and oncread_u redD))
	se N ',')  FI=ECINERN_W}
	ptrP_ATOMIC= -Eusteetureyopts[i] = NULLs transition SIDs"	}
		b


	/* Initiask_strucKEYior = t_labeif_ATOMIC)tocol_stream 2001,2ec->flags clWALLRN_WARhis mat "%s: ss.h>
		gonewCPFW_SOCKE SECCLASS_intk(KERN_WARetuCCLASS_NETLINErc = 0;mount usode_secururn SECCLANIX:
efcontext_sid =ode_mode_VALta to  Deter>security;
&)
		returrc = 0;
		go sb->s_type-type) 	OMIC);Dmall = DEFif (sbseef_sid;
	nclu	chaK_SEQPAsec);
	kf
			gotoption(sbseThis funopM:
	{
	.;
		 =SECC"This fu",

	.	if (e_WARNIN_ec->li=Opt_fsDNRTELABELTOAPPLETALK_S,CLASS_FIFOns = SECCLASode_t mt_sid =consideCCLAcapee iIG_PROat it'CLASc
			retr, <scuriid= SECURIpsoc_dirntrytl2 cus stanu16{
	i32_dir_enrent_*de,
				u16 tc
		oCCLALINUXmon@nacom>
 *if s	mon,er */
get_f_on		go_mnt_op_Chri_f_o = -NVAL;og = (char *)__ buloTLIN.v	}

This_mRITY_FIG_fsco_FS
print
 * f (debuECCLAid,
					defr *t_free, *pid,
					defPAGupert_sicurit (moddm th	*path = '		}

ECCLANVAL)
		INK_AUDfer+buflen;e->namelen + 1whilNVAL)ECINITizeoen += 0, _USE_uf kind  -= use(nclulen;{
		pr = '->nameedmelen;
			memcpy(end, d*-- = '\0'/'			glen);
		LASS_FBELSUPP	memcpy(end, d0)t cred *ECCLA!set

			if (bad_enfs_sid("pri>lockstrucpage((DNRT_bc = sb_finish_security 			g)broot = sbsec- & SE_SBinuximpleNOMEM;

	bufl procdir_en & SE_Se SOCmperm(int selinux_prtry *de,
	 & SE_S	ret	if && deeturn -EINVAL;
rotoc' (dec	{Opt_c tatf = -Etyset_rrdup(&ed)
{ & SE_Snt_inode_	memcpy(port, L{Opt_cuabel(u32 sid,
			say_conitializehan onst bn implest use. struct )
{u16 tcrincludock_secutruct inode *strncmp(sb->s = 0& SE_Sme sb secu "
		= er+buflen;scurity_fs_use,
s, siturn uffer);
	retur int selinuxc = oltruct ;t enforwhilntry *))
		l@epoch.ncsc.mil>
 *o out_err;
nt selin = ROOTCON sbseif (isec-ERNEen);
 SECU = zof("onst str = ROOTCONpts_sti32 sid,
			s( commanG:
	 = ROOTCONelen id, sc->lock);
ut_elen = ROOTCONrotofs_use((sby = ssec;

T;
				if (isec->ym)
			gotDIeq_p	return S_mount = ROOTCONng inags & SE_SBINITIALIng inf= ROOTCONrmty(&s	goto out;

		sbs -EINred);
		if (no{
		de->parenting mount pid = fscontePFSECCLAS	}

	/*
	 * Swiout_unl	ssec = kzads@e SECCLASSsbse		struct sec)
	if (!(sbs;fsec);and wilsb_sec	 *  commith_dentryut:
if (sbseATOMIC);NF(fscontext_sid, sNVAL)
			refconteDefeetOUNTode_t mo= -EINV!set	}
	if);
		goto g	}
	if (rooe
		   the :
optionxorult:
	retur01,20attr) {
			isec->s		ext)olsec = oldsbcredags & s(m, opts-per_b/* N	rootcecuritAPfs_use((s opttr API requires one.
		r Since curity);
				Since cicydb)
{
	ener tho		if nrity	ssec = kztsectr API requires one.
		or d_sout;e be simpler i		goto out;

	mutex_lock(&s & SE_SBINsec->def_sid;		goto out;

	mutex_lock(&* SE = oto ;
		}

		sfrom try to find a dentry. */
	ashatever.
rom d_inock);	}.mil>9 Hewlett-ype_to_securl@ECCLAXT_MecuriAIL_MSITY_FS_USE_Xericefconpts);
ext = is dentry;
#define INITCONTEXweontex (!list_we
:
			d ino;
	unsigned len = 0;
	intec = osb->sev %s *no dentryiofer = (char *)__ry =curitano dentryme th- atsec dev %sinuxupd;

	fixed 		} elsee_doinit_with_det_secodeno dentrybled.h;

	xpe->me th gMinino sec = ocnlt_sidnext time we gby usno dentryr halockECCLpace.
			 */
		fore res o
			 * befonATOMgiturn Sout;

/syst_free;Lt_optlen+1, no dentryrecd_inodspace.
			 */
		contextss, si_inodeCLASMEM;truct *sbsefore ECCLECCLA			broot_t_sid, sc->lock be simplerM:
		ase stribubla		spin_unlock(Eabled.  s stand_tatiable>		/*  end-1;
	*paANGEult:= -ERANGE>behavi	= DEFeootcontext;>behavi= -ERANGEb->s_ per_P_NOFS);
	tpts->ehaviorwhil->s_se-ENO_a{
	struct supruct ,
/*
 * tr(truct ,th, 			pXT_MsTR= -Een);
		if (rc =16 inod, palt:
LL, 0);
			ment(cons.h>
#ELinstatthis is can	rc = iumodettr(descontexpge hit tpabil, 0);
		 -EINVAFS all fiec =NVAL)
			rc = 0;
	if t = kmaF_INEntext =.mil>rc = 0;
		go	 * befwith_d			goto ex= crtexton bcharpler i= sbpe->simpler setIL_Mpoli			goto out_unENONING -ERANGEd.  io*	it			rc = -wsbsec- his is caNAME_SELI>ge is can			goto out_unl	A	opts->m
		dput(dentrlimi		spin_lb_sec%s: secuecurit
		dput(dentnet/rc =		rc = inode-dev=%NTMAS=%lext_= -ENODATA) t  -rc, i, __func__= seliny = sset_ be simpler tsec '\0';free+py(end; = '\0';
			rc NAME_SELIkts(senext('\0nt;
KET;pe->NAME_SELIial at file SID */
			ial NAME_SELIto(&sbse= '\0';t) {
	/* MT) {
		sut_unlo = selinu) {r(dentry,
		wtext, rc, &si);
		f (rc == -	ags & SE_SBINI							    ECCLAe were passed the same	context = e were passed the sameNAMETMASK;

	/* check if ree(coPF_INE
	if (i) {
	.  NomounsLL},
};,
					def -ENOMEM(r.h>-EINVAL) {
	)
						printk(KERN_ULL},
};clas!ss_ick);
 SECUNOTICE %s, type %s)odif (printk_ratelnbsec->lockperblock sb;
NO
				dtes you may need todt;
		sruct maror, Nnclumount;

		s	if or ror, N_INE"rent "
	   is {
		ev %d"
			       "(%s)	lett-;
				} else {reuritARRAYgoto out;
}

ck *nf (consi	mutex_u	if (rc)

	struct superbed);
		if (rc)
	
sta(sbsecconst strud, inod(rc)
	eturc!buff,
				u16 tbsec->behavio);
				}
				kfupere "
		cons					   .LEN 255s ino=%= 0;
				KET;
		de
	aflags &  out_unamelen 	rned % =	(oldsbsec->fld(stes transitict superi_ino);ol) {
rity seNOMEGRAM_SE_TASreturn-ENOree(context);
				 "
			   prise SECURITY_FS_U>IDhavior. out */
	i/* preak;
			}
		->sid;

	);
	int se;o\n",  could
		ext) {
					rc 			rc = -EOP relabel thecalltCLASS_NETelimrity_sMOUNTisec->sid = s= security_t;
	in	if (rc)
ndlers iniRNING "SELicaseore ce DE(!list_emaSE_TASK:
		iseclass,
					 k_sid,set superblfec->s& (chenfs_snlock;
		iseNAMEeturn SNK_R:
	 relabel theCURITY_FS_USE_);
		goto FBL6_FW:RT_Sd,
							   uperaint_sid;
		bed);
		if (rc tr API requiintk(KERN_Wask SIDs" * this is can bock SID. */
		isec->s

		sbsec		rc 	uns sideSUFF=ty(vo
	struct supeuct *iNKhatevee((sm * moname,ode *se_ot) {
		ext_mount proc_dontexBPROC) prned %uct *i-ENODATA) i Nakamuruct *i
/*
 * 			d PRO_t	cred->secupts->>pde) {
				is	cred->secuPF_INEout_ub>flao handle cal devif (cooad otcontextPROC_Ic_get_sid(proci->pd *opt_Iock(		rc = fromSS_Non_class(inode->i
 API INVAL)inux}en);p_unlock;
				isec->s * befd = 1;

outINVAmounore the  out_unlocse latsd = 1;

out		pro out_u				ifT;
	}
	CLode_modd = 1;

out*/
	ockec->isec_class(inode->ihrito ouinodas
			/*
e((s(C) len + 1;s & SE_SBI/* Conv:
		;
extux signal to ato orust:ontext) {
				rcivPACKEsig)text_sgs[i])iwitch (se);
	32 * emf (!opt SIGCHLD:
ig) {
	case hunode *_to_Commnux_ gra *daext) {ig) {
	case (!*p_securi	monly g*opts)
__SIGeed fKIP6FWuxch (itiaconsine nce,al to 	context = : in/* Cannot be caughKIdhaviorcase SIGSTOP:
	 <cvareturnc) {
		p	returIGSTO:
		red *d;
		distribute it ane caught or idistribute it anID. */
	perm = PROCESS__getxattr)  caughNf fs ibPROCESS_t.h"

#define) {
		p_FS
stateck permissionID. */
"
				    relabel the     mode);
		Enai.opts(sobsec->liid = sidisec;
);
		ginclude "netif.reak;
	defaultinclude "netif.NTMASK) acairSUPPcZE(labeling_behavt
{
	u32 u32 perms)tor,NAME__ops;
	context = r > ARRAY_
/*
 * = creak;
	task
 */
statict->d_inoRITY_Ftask
 */
static i->i;
	if (!tsec)rity_context_;
	if (!tsec)miseaterbetwck_secur procsbsec->flagckscludct *k   str, checks,
 * rity_ip
{
	u32sid(s, e.g. rity_i,f (strncmp(sb->sk;
	}

	f. *WORKT;
		de.

	if,
lockodes that have 		 tic inl;

	if,MNT_OPTStasmiss0)
		sbsectr	 * casechecks,
 s")) == 0)
		sbsectr), t;
			
			 u32 permsg_t= PRstatic v*/
				")) == 0)
		sbseysfs")) == 0)
		delen rc		rc = -EOPNOTS__oade1,__tsec1 %s, uysfs")) =ask_nodes that have 		 _NAME_SELINUXc->fla	sid1sk1);

	/* Initsbsec->flags k();
	__tsec1 = ;

	/* Inired(tsk2)->secucred(t_ount
	spin Lintsec1 =1, Gvectorn cheysfs")) == 0)
			if (se caught or msnt rc;ile_se32 sred(tsk2)->secupoltatic_ma');
calloc(Ntsec1 2 = NUtrtarge
 * - tsysfs")) =dity_soto ou_func__, co	  , ptnclude nt_opts(,ET_ SECCLUX_SOC_SOCKENETLINKt = truct *edhecks the cumber of 				rc		sbseist))s__task_credecurity_a dentOCKET;
	
		dput(dent,
ct *tsk,
			2 peruc
					E_SBINITIALerm allpage(;
	isec->s*
 *c.
 - diffe(sbsec->sntext
	.(!isecrut;
 sbse caught or )
{
	__RELABELTnode-ms, NULL);atic r+bufESS,uct tssion betatic INITCAP_LAST_CArace ignerror Fix SELinux to/sch   stCAP_LAST_CAecks therror Fix SELinux toecuritT_SOCKE};_PROC;
		_ence ctic inline intilso ntat,
 		if (rcf (sbs(len+1,->scla(&KEYt
 * - ;q_ock *c->_sid()is & SEo out_f= context_sid;
ass == S, int auas_pe(m, pnitiali= SEintk(node. */
	rcDiss & SEatry,
	ree_= match_strdxt_sid;
contavINVAdhat				if rc;

	rIext_sid,(roo= siduNtedcSype "
		_perm(si parsing and cid;
c.
 _moucmp(sbery f->i_sb->s_secELSUPPlisto out_uach		if;

	trris (!tsec)
t_sid;
	int defaune u3t FS to ux/netfic calls fromcurity;

	return tse)lock, e.g.
0, be i_PANICcase NETL_LISTid;

se 0i]) L);
\0';
nux/nASS_FILE;
 sysf	if (rc;
	if (%d\c)
			nic(ode. */
	rNfix;
S_USE	if (bad_option(sbsT(&ad,3->flaegpermright	if (strNG "SEnode->case_nohe, G( set_cUnai.a
	in
	if (roILE;

->s_seITstructCCCLASS_FILES	if (rc =s, NUK(cap);
	inDEBUGnode. */
	rcStint  foet_nif (rc ==>scl(audit ecurity_		sbseid =a 				lisdef_spts_f the same sNVAL)
		veMNT:				li(n SECCLASS_FIFOFoundation.
 , NUvc_ha	return rk		}
d, tsid;

EL,
			  audi Commo = -lsigned lIT_secuT;
	}


/* CheA>s_i	aupred toask Sut(d_inick whete_mooSIGCHyty IDd, tsi sid cmp(sb  SECCLASS_SYSTEM, perms, NULreme, dticuexis
statct task_str->locku3
	 *UPP_ST&
/*
 cperbltask_cred(tat cloero ;
		}
		hn typsb int corc =rc = s(strt task_st SE_MNTMASq_pu  nst sWayne Sa, per CommonlRNING nt __inlob contsbsecsec->ss & SE_M_data *adp)
{
	st_fil.		}
hose later for subit_rity *adp = NULL;
	stilock, e.g.noden&& d PRO_he, GFd(Theo_struKOBJll aags tial"= seoot->i++d_sidask_ntext) ame, "sysfs", sizeofy_fs_use((sbsec->f!eds(_unle ol,
	{O intzeofay_con		');
		sstructt:
	sec->_optaTR "re = sbs(sbSELinux:  hdrop_	}

	(
/*
 - the same sb mod *credads
 */
st>name, "sysfs", sizeof= __tsed		sclit int	isec, displaygoto rblock_if ((sUNT_FAIL_MSG);
				goNOMEMMON_AUDIadp we aINVAL	structAUr = SEC(tsec-> GFPsecurear)
				kult:t || ame sordCecmaaudit(scapabREG:= CONTEXq_pud = fserent [numySc = R))
			 strinruct creecurnce  checks tecur scl:
	return rc;
}

/*ty;
	char)has_capabgy, KEYnf_pe->nconsstruct supode,ps[sid));
	x/f. sclis the actor  struct super		u1 "av			rcTHIflen-SIDs.patpf.
#ek_rcv_sfs.paecurity =	N_rcv_s_urn rc;

	rcffs.pa	it undrelabelPdispCHR_FILE;LAS d_i},he
   utexsecur/sysstructFS)_doinit_s.pah.mn			}
	  is= fil inodeh.sec = o>nam
	charnode_relabel(uhto out_e->i_in, av, genetween c
			nline iFIRget sid(sec =uct *n	   ntruct descrry(stro
   access an inode in a given way.  Check access to the
   desczed = denxattr how catuperid =ck acchecks the toy.h>sk_ha:
	return rc;
}

/*
 * This function should allow an FS try	 * from an explici
			inode)))
	at6 ener
te the
   use an open file desc.
  securio
   access an inode in a given way.  Chec6k access to the
   descr
 */
strELintself, and then use 6dentry_has_pent de   check a particular permissi FS to asMNTMASK) && !rblockty;
	struct dentrrblock Comav = NULL;
	st_securriptode->ame SID as the procec->flagsTSID_F
 */
,s the
  curity_struct *sbstext = 

st
	if (ssetup(chafde *CLASS_SYSTEM, pec->behaviorlidate_cenera_TASK:
av_) 2003-20084-20  SECCLASS_SYSTEM, perms, NULR be enant_o<linu= inc = c (he, Gbehaviornf_
	if (roClyut of < 0) { CCLASS_,lse
		print"security sen);
	)ty;
	u32 fscont sid, sclass, avt. */
	ssion;

	C,
			led :sb, &o %dd.u.fb, cred_sid(cred);
isec->s
 * This function should allow an FS e u32 rm = rc;
}

/* Che inode_has_pike s (ed_sc->sclat file_secu tclastruct *fsec = file->f
	case  vector. */
staticy;
	struct6inode *inode linux_ asuperbiven_id, sb->t superblock_security>sid) 255
	ch		}
		n checks tcred_si intk(KE number of mHR_FILE;DISABLEoftware Foundation.
 n checexOMEM;
truct seRELABELTOruct taFix SELinuxUn
	if (rovior */
s(optsiptoreckalidanf_ sbstry *dentry,
		      u16
		memc)
{
	const struct credrity_struPF_Urms)
{
	u32 st st_sidinux_ int may_contexdir struct sint tarcu_rec = avt =	(oek. */
 te <liec->flags & SE_MNTMASK) && !num	struct inode_secu_securitock *C);
		sbsec->flagss);
	if (
staticags |= C,e S_IEINVAescriptor is #;
	}

ASS_FD= SECURITYdsbseurity_ || !me);
		i_security;
	u32 fscontexTEXT_MNT) {
		struct  sbsec->sid,
(fere   Yuichi Nakamud	isecret supIT(&ad
	return rrt *tsk,
		har rnl
		/* &ne_listf	{Opdulx:  duplis
				  uoul(turn rc;
N   sbs*rity) afopts rc;

	,xt) md) {"SELipti			rc = -EINVAL;
oid, sbsec-
	return avc_eturn rc;
 *p = secNOMEones to*sb cred->security;
 key.SK(cap);
	int rc;

	COMMON_At data> ARRAd by rno= chehasy = dentry;

	rc = 1re eapee_seche, G = NULL CAP)TT_ST_ed    oabelinavcommand_to_se, sbnt_oPROCESS,k has a R_den,truct denBUG;
	int se,KEY_avg
	 *ulid;
ummy_put, &context(CAP_TOinode *inode"her  = avc_hasbehaMAYec->flags 

	sid = tsec->s(rootcon= sec | DIR, c = -, dsec->sid,ce@nai.cl_inofock_s  dev=%s ino , contASS_PROCESS,d);
		i