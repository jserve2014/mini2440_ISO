/*
   md.c : Multiple Devices driver for Linux
	  Copyright (C) 1998, 1999, 2000 Ingo Molnar

     completely rewritten, based on the MD driver code from Marc Zyngier

   Changes:

   - RAID-1/RAID-5 extensions by Miguel de Icaza, Gadi Oxman, Ingo Molnar
   - RAID-6 extensions by H. Peter Anvin <hpa@zytor.com>
   - boot support for linear and striped mode by Harald Hoyer <HarryH@Royal.Net>
   - kerneld support by Boris Tobotras <boris@xtalk.msk.su>
   - kmod support by: Cyrus Durgin
   - RAID0 bugfixes: Mark Anthony Lisher <markal@iname.com>
   - Devfs support by Richard Gooch <rgooch@atnf.csiro.au>

   - lots of fixes and improvements to the RAID1/RAID5 and generic
     RAID code (such as request based resynchronization):

     Neil Brown <neilb@cse.unsw.edu.au>.

   - persistent bitmap code
     Copyright (C) 2003-2004, Paul Clements, SteelEye Technology, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   You should have received a copy of the GNU General Public License
   (for example /usr/src/linux/COPYING); if not, write to the Free
   Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <linux/kthread.h>
#include <linux/blkdev.h>
#include <linux/sysctl.h>
#include <linux/seq_file.h>
#include <linux/buffer_head.h> /* for invalidate_bdev */
#include <linux/poll.h>
#include <linux/ctype.h>
#include <linux/hdreg.h>
#include <linux/proc_fs.h>
#include <linux/random.h>
#include <linux/reboot.h>
#include <linux/file.h>
#include <linux/delay.h>
#include <linux/raid/md_p.h>
#include <linux/raid/md_u.h>
#include "md.h"
#include "bitmap.h"

#define DEBUG 0
#define dprintk(x...) ((void)(DEBUG && printk(x)))


#ifndef MODULE
static void autostart_arrays(int part);
#endif

static LIST_HEAD(pers_list);
static DEFINE_SPINLOCK(pers_lock);

static void md_print_devices(void);

static DECLARE_WAIT_QUEUE_HEAD(resync_wait);

#define MD_BUG(x...) { printk("md: bug in file %s, line %d\n", __FILE__, __LINE__); md_print_devices(); }

/*
 * Current RAID-1,4,5 parallel reconstruction 'guaranteed speed limit'
 * is 1000 KB/sec, so the extra system load does not show up that much.
 * Increase it if you want to have more _guaranteed_ speed. Note that
 * the RAID driver will use the maximum available bandwidth if the IO
 * subsystem is idle. There is also an 'absolute maximum' reconstruction
 * speed limit - in case reconstruction slows down your system despite
 * idle IO detection.
 *
 * you can change it via /proc/sys/dev/raid/speed_limit_min and _max.
 * or /sys/block/mdX/md/sync_speed_{min,max}
 */

static int sysctl_speed_limit_min = 1000;
static int sysctl_speed_limit_max = 200000;
static inline int speed_min(mddev_t *mddev)
{
	return mddev->sync_speed_min ?
		mddev->sync_speed_min : sysctl_speed_limit_min;
}

static inline int speed_max(mddev_t *mddev)
{
	return mddev->sync_speed_max ?
		mddev->sync_speed_max : sysctl_speed_limit_max;
}

static struct ctl_table_header *raid_table_header;

static ctl_table raid_table[] = {
	{
		.ctl_name	= DEV_RAID_SPEED_LIMIT_MIN,
		.procname	= "speed_limit_min",
		.data		= &sysctl_speed_limit_min,
		.maxlen		= sizeof(int),
		.mode		= S_IRUGO|S_IWUSR,
		.proc_handler	= &proc_dointvec,
	},
	{
		.ctl_name	= DEV_RAID_SPEED_LIMIT_MAX,
		.procname	= "speed_limit_max",
		.data		= &sysctl_speed_limit_max,
		.maxlen		= sizeof(int),
		.mode		= S_IRUGO|S_IWUSR,
		.proc_handler	= &proc_dointvec,
	},
	{ .ctl_name = 0 }
};

static ctl_table raid_dir_table[] = {
	{
		.ctl_name	= DEV_RAID,
		.procname	= "raid",
		.maxlen		= 0,
		.mode		= S_IRUGO|S_IXUGO,
		.child		= raid_table,
	},
	{ .ctl_name = 0 }
};

static ctl_table raid_root_table[] = {
	{
		.ctl_name	= CTL_DEV,
		.procname	= "dev",
		.maxlen		= 0,
		.mode		= 0555,
		.child		= raid_dir_table,
	},
	{ .ctl_name = 0 }
};

static const struct block_device_operations md_fops;

static int start_readonly;

/*
 * We have a system wide 'event count' that is incremented
 * on any 'interesting' event, and readers of /proc/mdstat
 * can use 'poll' or 'select' to find out when the event
 * count increases.
 *
 * Events are:
 *  start array, stop array, error, add device, remove device,
 *  start build, activate spare
 */
static DECLARE_WAIT_QUEUE_HEAD(md_event_waiters);
static atomic_t md_event_count;
void md_new_event(mddev_t *mddev)
{
	atomic_inc(&md_event_count);
	wake_up(&md_event_waiters);
}
EXPORT_SYMBOL_GPL(md_new_event);

/* Alternate version that can be called from interrupts
 * when calling sysfs_notify isn't needed.
 */
static void md_new_event_inintr(mddev_t *mddev)
{
	atomic_inc(&md_event_count);
	wake_up(&md_event_waiters);
}

/*
 * Enables to iterate over all existing md arrays
 * all_mddevs_lock protects this list.
 */
static LIST_HEAD(all_mddevs);
static DEFINE_SPINLOCK(all_mddevs_lock);


/*
 * iterates through all used mddevs in the system.
 * We take care to grab the all_mddevs_lock whenever navigating
 * the list, and to always hold a refcount when unlocked.
 * Any code which breaks out of this loop while own
 * a reference to the current mddev and must mddev_put it.
 */
#define for_each_mddev(mddev,tmp)					\
									\
	for (({ spin_lock(&all_mddevs_lock); 				\
		tmp = all_mddevs.next;					\
		mddev = NULL;});					\
	     ({ if (tmp != &all_mddevs)					\
			mddev_get(list_entry(tmp, mddev_t, all_mddevs));\
		spin_unlock(&all_mddevs_lock);				\
		if (mddev) mddev_put(mddev);				\
		mddev = list_entry(tmp, mddev_t, all_mddevs);		\
		tmp != &all_mddevs;});					\
	     ({ spin_lock(&all_mddevs_lock);				\
		tmp = tmp->next;})					\
		)


/* Rather than calling directly into the personality make_request function,
 * IO requests come here first so that we can check if the device is
 * being suspended pending a reconfiguration.
 * We hold a refcount over the call to ->make_request.  By the time that
 * call has finished, the bio has been linked into some internal structure
 * and so is visible to ->quiesce(), so we don't need the refcount any more.
 */
static int md_make_request(struct request_queue *q, struct bio *bio)
{
	mddev_t *mddev = q->queuedata;
	int rv;
	if (mddev == NULL || mddev->pers == NULL) {
		bio_io_error(bio);
		return 0;
	}
	rcu_read_lock();
	if (mddev->suspended) {
		DEFINE_WAIT(__wait);
		for (;;) {
			prepare_to_wait(&mddev->sb_wait, &__wait,
					TASK_UNINTERRUPTIBLE);
			if (!mddev->suspended)
				break;
			rcu_read_unlock();
			schedule();
			rcu_read_lock();
		}
		finish_wait(&mddev->sb_wait, &__wait);
	}
	atomic_inc(&mddev->active_io);
	rcu_read_unlock();
	rv = mddev->pers->make_request(q, bio);
	if (atomic_dec_and_test(&mddev->active_io) && mddev->suspended)
		wake_up(&mddev->sb_wait);

	return rv;
}

static void mddev_suspend(mddev_t *mddev)
{
	BUG_ON(mddev->suspended);
	mddev->suspended = 1;
	synchronize_rcu();
	wait_event(mddev->sb_wait, atomic_read(&mddev->active_io) == 0);
	mddev->pers->quiesce(mddev, 1);
	md_unregister_thread(mddev->thread);
	mddev->thread = NULL;
	/* we now know that no code is executing in the personality module,
	 * except possibly the tail end of a ->bi_end_io function, but that
	 * is certain to complete before the module has a chance to get
	 * unloaded
	 */
}

static void mddev_resume(mddev_t *mddev)
{
	mddev->suspended = 0;
	wake_up(&mddev->sb_wait);
	mddev->pers->quiesce(mddev, 0);
}

int mddev_congested(mddev_t *mddev, int bits)
{
	return mddev->suspended;
}
EXPORT_SYMBOL(mddev_congested);


static inline mddev_t *mddev_get(mddev_t *mddev)
{
	atomic_inc(&mddev->active);
	return mddev;
}

static void mddev_delayed_delete(struct work_struct *ws);

static void mddev_put(mddev_t *mddev)
{
	if (!atomic_dec_and_lock(&mddev->active, &all_mddevs_lock))
		return;
	if (!mddev->raid_disks && list_empty(&mddev->disks) &&
	    !mddev->hold_active) {
		list_del(&mddev->all_mddevs);
		if (mddev->gendisk) {
			/* we did a probe so need to clean up.
			 * Call schedule_work inside the spinlock
			 * so that flush_scheduled_work() after
			 * mddev_find will succeed in waiting for the
			 * work to be done.
			 */
			INIT_WORK(&mddev->del_work, mddev_delayed_delete);
			schedule_work(&mddev->del_work);
		} else
			kfree(mddev);
	}
	spin_unlock(&all_mddevs_lock);
}

static mddev_t * mddev_find(dev_t unit)
{
	mddev_t *mddev, *new = NULL;

 retry:
	spin_lock(&all_mddevs_lock);

	if (unit) {
		list_for_each_entry(mddev, &all_mddevs, all_mddevs)
			if (mddev->unit == unit) {
				mddev_get(mddev);
				spin_unlock(&all_mddevs_lock);
				kfree(new);
				return mddev;
			}

		if (new) {
			list_add(&new->all_mddevs, &all_mddevs);
			spin_unlock(&all_mddevs_lock);
			new->hold_active = UNTIL_IOCTL;
			return new;
		}
	} else if (new) {
		/* find an unused unit number */
		static int next_minor = 512;
		int start = next_minor;
		int is_free = 0;
		int dev = 0;
		while (!is_free) {
			dev = MKDEV(MD_MAJOR, next_minor);
			next_minor++;
			if (next_minor > MINORMASK)
				next_minor = 0;
			if (next_minor == start) {
				/* Oh dear, all in use. */
				spin_unlock(&all_mddevs_lock);
				kfree(new);
				return NULL;
			}
				
			is_free = 1;
			list_for_each_entry(mddev, &all_mddevs, all_mddevs)
				if (mddev->unit == dev) {
					is_free = 0;
					break;
				}
		}
		new->unit = dev;
		new->md_minor = MINOR(dev);
		new->hold_active = UNTIL_STOP;
		list_add(&new->all_mddevs, &all_mddevs);
		spin_unlock(&all_mddevs_lock);
		return new;
	}
	spin_unlock(&all_mddevs_lock);

	new = kzalloc(sizeof(*new), GFP_KERNEL);
	if (!new)
		return NULL;

	new->unit = unit;
	if (MAJOR(unit) == MD_MAJOR)
		new->md_minor = MINOR(unit);
	else
		new->md_minor = MINOR(unit) >> MdpMinorShift;

	mutex_init(&new->open_mutex);
	mutex_init(&new->reconfig_mutex);
	mutex_init(&new->bitmap_mutex);
	INIT_LIST_HEAD(&new->disks);
	INIT_LIST_HEAD(&new->all_mddevs);
	init_timer(&new->safemode_timer);
	atomic_set(&new->active, 1);
	atomic_set(&new->openers, 0);
	atomic_set(&new->active_io, 0);
	spin_lock_init(&new->write_lock);
	init_waitqueue_head(&new->sb_wait);
	init_waitqueue_head(&new->recovery_wait);
	new->reshape_position = MaxSector;
	new->resync_min = 0;
	new->resync_max = MaxSector;
	new->level = LEVEL_NONE;

	goto retry;
}

static inline int mddev_lock(mddev_t * mddev)
{
	return mutex_lock_interruptible(&mddev->reconfig_mutex);
}

static inline int mddev_is_locked(mddev_t *mddev)
{
	return mutex_is_locked(&mddev->reconfig_mutex);
}

static inline int mddev_trylock(mddev_t * mddev)
{
	return mutex_trylock(&mddev->reconfig_mutex);
}

static inline void mddev_unlock(mddev_t * mddev)
{
	mutex_unlock(&mddev->reconfig_mutex);

	md_wakeup_thread(mddev->thread);
}

static mdk_rdev_t * find_rdev_nr(mddev_t *mddev, int nr)
{
	mdk_rdev_t *rdev;

	list_for_each_entry(rdev, &mddev->disks, same_set)
		if (rdev->desc_nr == nr)
			return rdev;

	return NULL;
}

static mdk_rdev_t * find_rdev(mddev_t * mddev, dev_t dev)
{
	mdk_rdev_t *rdev;

	list_for_each_entry(rdev, &mddev->disks, same_set)
		if (rdev->bdev->bd_dev == dev)
			return rdev;

	return NULL;
}

static struct mdk_personality *find_pers(int level, char *clevel)
{
	struct mdk_personality *pers;
	list_for_each_entry(pers, &pers_list, list) {
		if (level != LEVEL_NONE && pers->level == level)
			return pers;
		if (strcmp(pers->name, clevel)==0)
			return pers;
	}
	return NULL;
}

/* return the offset of the super block in 512byte sectors */
static inline sector_t calc_dev_sboffset(struct block_device *bdev)
{
	sector_t num_sectors = bdev->bd_inode->i_size / 512;
	return MD_NEW_SIZE_SECTORS(num_sectors);
}

static int alloc_disk_sb(mdk_rdev_t * rdev)
{
	if (rdev->sb_page)
		MD_BUG();

	rdev->sb_page = alloc_page(GFP_KERNEL);
	if (!rdev->sb_page) {
		printk(KERN_ALERT "md: out of memory.\n");
		return -ENOMEM;
	}

	return 0;
}

static void free_disk_sb(mdk_rdev_t * rdev)
{
	if (rdev->sb_page) {
		put_page(rdev->sb_page);
		rdev->sb_loaded = 0;
		rdev->sb_page = NULL;
		rdev->sb_start = 0;
		rdev->sectors = 0;
	}
}


static void super_written(struct bio *bio, int error)
{
	mdk_rdev_t *rdev = bio->bi_private;
	mddev_t *mddev = rdev->mddev;

	if (error || !test_bit(BIO_UPTODATE, &bio->bi_flags)) {
		printk("md: super_written gets error=%d, uptodate=%d\n",
		       error, test_bit(BIO_UPTODATE, &bio->bi_flags));
		WARN_ON(test_bit(BIO_UPTODATE, &bio->bi_flags));
		md_error(mddev, rdev);
	}

	if (atomic_dec_and_test(&mddev->pending_writes))
		wake_up(&mddev->sb_wait);
	bio_put(bio);
}

static void super_written_barrier(struct bio *bio, int error)
{
	struct bio *bio2 = bio->bi_private;
	mdk_rdev_t *rdev = bio2->bi_private;
	mddev_t *mddev = rdev->mddev;

	if (!test_bit(BIO_UPTODATE, &bio->bi_flags) &&
	    error == -EOPNOTSUPP) {
		unsigned long flags;
		/* barriers don't appear to be supported :-( */
		set_bit(BarriersNotsupp, &rdev->flags);
		mddev->barriers_work = 0;
		spin_lock_irqsave(&mddev->write_lock, flags);
		bio2->bi_next = mddev->biolist;
		mddev->biolist = bio2;
		spin_unlock_irqrestore(&mddev->write_lock, flags);
		wake_up(&mddev->sb_wait);
		bio_put(bio);
	} else {
		bio_put(bio2);
		bio->bi_private = rdev;
		super_written(bio, error);
	}
}

void md_super_write(mddev_t *mddev, mdk_rdev_t *rdev,
		   sector_t sector, int size, struct page *page)
{
	/* write first size bytes of page to sector of rdev
	 * Increment mddev->pending_writes before returning
	 * and decrement it on completion, waking up sb_wait
	 * if zero is reached.
	 * If an error occurred, call md_error
	 *
	 * As we might need to resubmit the request if BIO_RW_BARRIER
	 * causes ENOTSUPP, we allocate a spare bio...
	 */
	struct bio *bio = bio_alloc(GFP_NOIO, 1);
	int rw = (1<<BIO_RW) | (1<<BIO_RW_SYNCIO) | (1<<BIO_RW_UNPLUG);

	bio->bi_bdev = rdev->bdev;
	bio->bi_sector = sector;
	bio_add_page(bio, page, size, 0);
	bio->bi_private = rdev;
	bio->bi_end_io = super_written;
	bio->bi_rw = rw;

	atomic_inc(&mddev->pending_writes);
	if (!test_bit(BarriersNotsupp, &rdev->flags)) {
		struct bio *rbio;
		rw |= (1<<BIO_RW_BARRIER);
		rbio = bio_clone(bio, GFP_NOIO);
		rbio->bi_private = bio;
		rbio->bi_end_io = super_written_barrier;
		submit_bio(rw, rbio);
	} else
		submit_bio(rw, bio);
}

void md_super_wait(mddev_t *mddev)
{
	/* wait for all superblock writes that were scheduled to complete.
	 * if any had to be retried (due to BARRIER problems), retry them
	 */
	DEFINE_WAIT(wq);
	for(;;) {
		prepare_to_wait(&mddev->sb_wait, &wq, TASK_UNINTERRUPTIBLE);
		if (atomic_read(&mddev->pending_writes)==0)
			break;
		while (mddev->biolist) {
			struct bio *bio;
			spin_lock_irq(&mddev->write_lock);
			bio = mddev->biolist;
			mddev->biolist = bio->bi_next ;
			bio->bi_next = NULL;
			spin_unlock_irq(&mddev->write_lock);
			submit_bio(bio->bi_rw, bio);
		}
		schedule();
	}
	finish_wait(&mddev->sb_wait, &wq);
}

static void bi_complete(struct bio *bio, int error)
{
	complete((struct completion*)bio->bi_private);
}

int sync_page_io(struct block_device *bdev, sector_t sector, int size,
		   struct page *page, int rw)
{
	struct bio *bio = bio_alloc(GFP_NOIO, 1);
	struct completion event;
	int ret;

	rw |= (1 << BIO_RW_SYNCIO) | (1 << BIO_RW_UNPLUG);

	bio->bi_bdev = bdev;
	bio->bi_sector = sector;
	bio_add_page(bio, page, size, 0);
	init_completion(&event);
	bio->bi_private = &event;
	bio->bi_end_io = bi_complete;
	submit_bio(rw, bio);
	wait_for_completion(&event);

	ret = test_bit(BIO_UPTODATE, &bio->bi_flags);
	bio_put(bio);
	return ret;
}
EXPORT_SYMBOL_GPL(sync_page_io);

static int read_disk_sb(mdk_rdev_t * rdev, int size)
{
	char b[BDEVNAME_SIZE];
	if (!rdev->sb_page) {
		MD_BUG();
		return -EINVAL;
	}
	if (rdev->sb_loaded)
		return 0;


	if (!sync_page_io(rdev->bdev, rdev->sb_start, size, rdev->sb_page, READ))
		goto fail;
	rdev->sb_loaded = 1;
	return 0;

fail:
	printk(KERN_WARNING "md: disabled device %s, could not read superblock.\n",
		bdevname(rdev->bdev,b));
	return -EINVAL;
}

static int uuid_equal(mdp_super_t *sb1, mdp_super_t *sb2)
{
	return 	sb1->set_uuid0 == sb2->set_uuid0 &&
		sb1->set_uuid1 == sb2->set_uuid1 &&
		sb1->set_uuid2 == sb2->set_uuid2 &&
		sb1->set_uuid3 == sb2->set_uuid3;
}

static int sb_equal(mdp_super_t *sb1, mdp_super_t *sb2)
{
	int ret;
	mdp_super_t *tmp1, *tmp2;

	tmp1 = kmalloc(sizeof(*tmp1),GFP_KERNEL);
	tmp2 = kmalloc(sizeof(*tmp2),GFP_KERNEL);

	if (!tmp1 || !tmp2) {
		ret = 0;
		printk(KERN_INFO "md.c sb_equal(): failed to allocate memory!\n");
		goto abort;
	}

	*tmp1 = *sb1;
	*tmp2 = *sb2;

	/*
	 * nr_disks is not constant
	 */
	tmp1->nr_disks = 0;
	tmp2->nr_disks = 0;

	ret = (memcmp(tmp1, tmp2, MD_SB_GENERIC_CONSTANT_WORDS * 4) == 0);
abort:
	kfree(tmp1);
	kfree(tmp2);
	return ret;
}


static u32 md_csum_fold(u32 csum)
{
	csum = (csum & 0xffff) + (csum >> 16);
	return (csum & 0xffff) + (csum >> 16);
}

static unsigned int calc_sb_csum(mdp_super_t * sb)
{
	u64 newcsum = 0;
	u32 *sb32 = (u32*)sb;
	int i;
	unsigned int disk_csum, csum;

	disk_csum = sb->sb_csum;
	sb->sb_csum = 0;

	for (i = 0; i < MD_SB_BYTES/4 ; i++)
		newcsum += sb32[i];
	csum = (newcsum & 0xffffffff) + (newcsum>>32);


#ifdef CONFIG_ALPHA
	/* This used to use csum_partial, which was wrong for several
	 * reasons including that different results are returned on
	 * different architectures.  It isn't critical that we get exactly
	 * the same return value as before (we always csum_fold before
	 * testing, and that removes any differences).  However as we
	 * know that csum_partial always returned a 16bit value on
	 * alphas, do a fold to maximise conformity to previous behaviour.
	 */
	sb->sb_csum = md_csum_fold(disk_csum);
#else
	sb->sb_csum = disk_csum;
#endif
	return csum;
}


/*
 * Handle superblock details.
 * We want to be able to handle multiple superblock formats
 * so we have a common interface to them all, and an array of
 * different handlers.
 * We rely on user-space to write the initial superblock, and support
 * reading and updating of superblocks.
 * Interface methods are:
 *   int load_super(mdk_rdev_t *dev, mdk_rdev_t *refdev, int minor_version)
 *      loads and validates a superblock on dev.
 *      if refdev != NULL, compare superblocks on both devices
 *    Return:
 *      0 - dev has a superblock that is compatible with refdev
 *      1 - dev has a superblock that is compatible and newer than refdev
 *          so dev should be used as the refdev in future
 *     -EINVAL superblock incompatible or invalid
 *     -othererror e.g. -EIO
 *
 *   int validate_super(mddev_t *mddev, mdk_rdev_t *dev)
 *      Verify that dev is acceptable into mddev.
 *       The first time, mddev->raid_disks will be 0, and data from
 *       dev should be merged in.  Subsequent calls check that dev
 *       is new enough.  Return 0 or -EINVAL
 *
 *   void sync_super(mddev_t *mddev, mdk_rdev_t *dev)
 *     Update the superblock for rdev with data in mddev
 *     This does not write to disc.
 *
 */

struct super_type  {
	char		    *name;
	struct module	    *owner;
	int		    (*load_super)(mdk_rdev_t *rdev, mdk_rdev_t *refdev,
					  int minor_version);
	int		    (*validate_super)(mddev_t *mddev, mdk_rdev_t *rdev);
	void		    (*sync_super)(mddev_t *mddev, mdk_rdev_t *rdev);
	unsigned long long  (*rdev_size_change)(mdk_rdev_t *rdev,
						sector_t num_sectors);
};

/*
 * Check that the given mddev has no bitmap.
 *
 * This function is called from the run method of all personalities that do not
 * support bitmaps. It prints an error message and returns non-zero if mddev
 * has a bitmap. Otherwise, it returns 0.
 *
 */
int md_check_no_bitmap(mddev_t *mddev)
{
	if (!mddev->bitmap_file && !mddev->bitmap_offset)
		return 0;
	printk(KERN_ERR "%s: bitmaps are not supported for %s\n",
		mdname(mddev), mddev->pers->name);
	return 1;
}
EXPORT_SYMBOL(md_check_no_bitmap);

/*
 * load_super for 0.90.0 
 */
static int super_90_load(mdk_rdev_t *rdev, mdk_rdev_t *refdev, int minor_version)
{
	char b[BDEVNAME_SIZE], b2[BDEVNAME_SIZE];
	mdp_super_t *sb;
	int ret;

	/*
	 * Calculate the position of the superblock (512byte sectors),
	 * it's at the end of the disk.
	 *
	 * It also happens to be a multiple of 4Kb.
	 */
	rdev->sb_start = calc_dev_sboffset(rdev->bdev);

	ret = read_disk_sb(rdev, MD_SB_BYTES);
	if (ret) return ret;

	ret = -EINVAL;

	bdevname(rdev->bdev, b);
	sb = (mdp_super_t*)page_address(rdev->sb_page);

	if (sb->md_magic != MD_SB_MAGIC) {
		printk(KERN_ERR "md: invalid raid superblock magic on %s\n",
		       b);
		goto abort;
	}

	if (sb->major_version != 0 ||
	    sb->minor_version < 90 ||
	    sb->minor_version > 91) {
		printk(KERN_WARNING "Bad version number %d.%d on %s\n",
			sb->major_version, sb->minor_version,
			b);
		goto abort;
	}

	if (sb->raid_disks <= 0)
		goto abort;

	if (md_csum_fold(calc_sb_csum(sb)) != md_csum_fold(sb->sb_csum)) {
		printk(KERN_WARNING "md: invalid superblock checksum on %s\n",
			b);
		goto abort;
	}

	rdev->preferred_minor = sb->md_minor;
	rdev->data_offset = 0;
	rdev->sb_size = MD_SB_BYTES;

	if (sb->level == LEVEL_MULTIPATH)
		rdev->desc_nr = -1;
	else
		rdev->desc_nr = sb->this_disk.number;

	if (!refdev) {
		ret = 1;
	} else {
		__u64 ev1, ev2;
		mdp_super_t *refsb = (mdp_super_t*)page_address(refdev->sb_page);
		if (!uuid_equal(refsb, sb)) {
			printk(KERN_WARNING "md: %s has different UUID to %s\n",
				b, bdevname(refdev->bdev,b2));
			goto abort;
		}
		if (!sb_equal(refsb, sb)) {
			printk(KERN_WARNING "md: %s has same UUID"
			       " but different superblock to %s\n",
			       b, bdevname(refdev->bdev, b2));
			goto abort;
		}
		ev1 = md_event(sb);
		ev2 = md_event(refsb);
		if (ev1 > ev2)
			ret = 1;
		else 
			ret = 0;
	}
	rdev->sectors = rdev->sb_start;

	if (rdev->sectors < sb->size * 2 && sb->level > 1)
		/* "this cannot possibly happen" ... */
		ret = -EINVAL;

 abort:
	return ret;
}

/*
 * validate_super for 0.90.0
 */
static int super_90_validate(mddev_t *mddev, mdk_rdev_t *rdev)
{
	mdp_disk_t *desc;
	mdp_super_t *sb = (mdp_super_t *)page_address(rdev->sb_page);
	__u64 ev1 = md_event(sb);

	rdev->raid_disk = -1;
	clear_bit(Faulty, &rdev->flags);
	clear_bit(In_sync, &rdev->flags);
	clear_bit(WriteMostly, &rdev->flags);
	clear_bit(BarriersNotsupp, &rdev->flags);

	if (mddev->raid_disks == 0) {
		mddev->major_version = 0;
		mddev->minor_version = sb->minor_version;
		mddev->patch_version = sb->patch_version;
		mddev->external = 0;
		mddev->chunk_sectors = sb->chunk_size >> 9;
		mddev->ctime = sb->ctime;
		mddev->utime = sb->utime;
		mddev->level = sb->level;
		mddev->clevel[0] = 0;
		mddev->layout = sb->layout;
		mddev->raid_disks = sb->raid_disks;
		mddev->dev_sectors = sb->size * 2;
		mddev->events = ev1;
		mddev->bitmap_offset = 0;
		mddev->default_bitmap_offset = MD_SB_BYTES >> 9;

		if (mddev->minor_version >= 91) {
			mddev->reshape_position = sb->reshape_position;
			mddev->delta_disks = sb->delta_disks;
			mddev->new_level = sb->new_level;
			mddev->new_layout = sb->new_layout;
			mddev->new_chunk_sectors = sb->new_chunk >> 9;
		} else {
			mddev->reshape_position = MaxSector;
			mddev->delta_disks = 0;
			mddev->new_level = mddev->level;
			mddev->new_layout = mddev->layout;
			mddev->new_chunk_sectors = mddev->chunk_sectors;
		}

		if (sb->state & (1<<MD_SB_CLEAN))
			mddev->recovery_cp = MaxSector;
		else {
			if (sb->events_hi == sb->cp_events_hi && 
				sb->events_lo == sb->cp_events_lo) {
				mddev->recovery_cp = sb->recovery_cp;
			} else
				mddev->recovery_cp = 0;
		}

		memcpy(mddev->uuid+0, &sb->set_uuid0, 4);
		memcpy(mddev->uuid+4, &sb->set_uuid1, 4);
		memcpy(mddev->uuid+8, &sb->set_uuid2, 4);
		memcpy(mddev->uuid+12,&sb->set_uuid3, 4);

		mddev->max_disks = MD_SB_DISKS;

		if (sb->state & (1<<MD_SB_BITMAP_PRESENT) &&
		    mddev->bitmap_file == NULL)
			mddev->bitmap_offset = mddev->default_bitmap_offset;

	} else if (mddev->pers == NULL) {
		/* Insist on good event counter while assembling */
		++ev1;
		if (ev1 < mddev->events) 
			return -EINVAL;
	} else if (mddev->bitmap) {
		/* if adding to array with a bitmap, then we can accept an
		 * older device ... but not too old.
		 */
		if (ev1 < mddev->bitmap->events_cleared)
			return 0;
	} else {
		if (ev1 < mddev->events)
			/* just a hot-add of a new device, leave raid_disk at -1 */
			return 0;
	}

	if (mddev->level != LEVEL_MULTIPATH) {
		desc = sb->disks + rdev->desc_nr;

		if (desc->state & (1<<MD_DISK_FAULTY))
			set_bit(Faulty, &rdev->flags);
		else if (desc->state & (1<<MD_DISK_SYNC) /* &&
			    desc->raid_disk < mddev->raid_disks */) {
			set_bit(In_sync, &rdev->flags);
			rdev->raid_disk = desc->raid_disk;
		} else if (desc->state & (1<<MD_DISK_ACTIVE)) {
			/* active but not in sync implies recovery up to
			 * reshape position.  We don't know exactly where
			 * that is, so set to zero for now */
			if (mddev->minor_version >= 91) {
				rdev->recovery_offset = 0;
				rdev->raid_disk = desc->raid_disk;
			}
		}
		if (desc->state & (1<<MD_DISK_WRITEMOSTLY))
			set_bit(WriteMostly, &rdev->flags);
	} else /* MULTIPATH are always insync */
		set_bit(In_sync, &rdev->flags);
	return 0;
}

/*
 * sync_super for 0.90.0
 */
static void super_90_sync(mddev_t *mddev, mdk_rdev_t *rdev)
{
	mdp_super_t *sb;
	mdk_rdev_t *rdev2;
	int next_spare = mddev->raid_disks;


	/* make rdev->sb match mddev data..
	 *
	 * 1/ zero out disks
	 * 2/ Add info for each disk, keeping track of highest desc_nr (next_spare);
	 * 3/ any empty disks < next_spare become removed
	 *
	 * disks[0] gets initialised to REMOVED because
	 * we cannot be sure from other fields if it has
	 * been initialised or not.
	 */
	int i;
	int active=0, working=0,failed=0,spare=0,nr_disks=0;

	rdev->sb_size = MD_SB_BYTES;

	sb = (mdp_super_t*)page_address(rdev->sb_page);

	memset(sb, 0, sizeof(*sb));

	sb->md_magic = MD_SB_MAGIC;
	sb->major_version = mddev->major_version;
	sb->patch_version = mddev->patch_version;
	sb->gvalid_words  = 0; /* ignored */
	memcpy(&sb->set_uuid0, mddev->uuid+0, 4);
	memcpy(&sb->set_uuid1, mddev->uuid+4, 4);
	memcpy(&sb->set_uuid2, mddev->uuid+8, 4);
	memcpy(&sb->set_uuid3, mddev->uuid+12,4);

	sb->ctime = mddev->ctime;
	sb->level = mddev->level;
	sb->size = mddev->dev_sectors / 2;
	sb->raid_disks = mddev->raid_disks;
	sb->md_minor = mddev->md_minor;
	sb->not_persistent = 0;
	sb->utime = mddev->utime;
	sb->state = 0;
	sb->events_hi = (mddev->events>>32);
	sb->events_lo = (u32)mddev->events;

	if (mddev->reshape_position == MaxSector)
		sb->minor_version = 90;
	else {
		sb->minor_version = 91;
		sb->reshape_position = mddev->reshape_position;
		sb->new_level = mddev->new_level;
		sb->delta_disks = mddev->delta_disks;
		sb->new_layout = mddev->new_layout;
		sb->new_chunk = mddev->new_chunk_sectors << 9;
	}
	mddev->minor_version = sb->minor_version;
	if (mddev->in_sync)
	{
		sb->recovery_cp = mddev->recovery_cp;
		sb->cp_events_hi = (mddev->events>>32);
		sb->cp_events_lo = (u32)mddev->events;
		if (mddev->recovery_cp == MaxSector)
			sb->state = (1<< MD_SB_CLEAN);
	} else
		sb->recovery_cp = 0;

	sb->layout = mddev->layout;
	sb->chunk_size = mddev->chunk_sectors << 9;

	if (mddev->bitmap && mddev->bitmap_file == NULL)
		sb->state |= (1<<MD_SB_BITMAP_PRESENT);

	sb->disks[0].state = (1<<MD_DISK_REMOVED);
	list_for_each_entry(rdev2, &mddev->disks, same_set) {
		mdp_disk_t *d;
		int desc_nr;
		int is_active = test_bit(In_sync, &rdev2->flags);

		if (rdev2->raid_disk >= 0 &&
		    sb->minor_version >= 91)
			/* we have nowhere to store the recovery_offset,
			 * but if it is not below the reshape_position,
			 * we can piggy-back on that.
			 */
			is_active = 1;
		if (rdev2->raid_disk < 0 ||
		    test_bit(Faulty, &rdev2->flags))
			is_active = 0;
		if (is_active)
			desc_nr = rdev2->raid_disk;
		else
			desc_nr = next_spare++;
		rdev2->desc_nr = desc_nr;
		d = &sb->disks[rdev2->desc_nr];
		nr_disks++;
		d->number = rdev2->desc_nr;
		d->major = MAJOR(rdev2->bdev->bd_dev);
		d->minor = MINOR(rdev2->bdev->bd_dev);
		if (is_active)
			d->raid_disk = rdev2->raid_disk;
		else
			d->raid_disk = rdev2->desc_nr; /* compatibility */
		if (test_bit(Faulty, &rdev2->flags))
			d->state = (1<<MD_DISK_FAULTY);
		else if (is_active) {
			d->state = (1<<MD_DISK_ACTIVE);
			if (test_bit(In_sync, &rdev2->flags))
				d->state |= (1<<MD_DISK_SYNC);
			active++;
			working++;
		} else {
			d->state = 0;
			spare++;
			working++;
		}
		if (test_bit(WriteMostly, &rdev2->flags))
			d->state |= (1<<MD_DISK_WRITEMOSTLY);
	}
	/* now set the "removed" and "faulty" bits on any missing devices */
	for (i=0 ; i < mddev->raid_disks ; i++) {
		mdp_disk_t *d = &sb->disks[i];
		if (d->state == 0 && d->number == 0) {
			d->number = i;
			d->raid_disk = i;
			d->state = (1<<MD_DISK_REMOVED);
			d->state |= (1<<MD_DISK_FAULTY);
			failed++;
		}
	}
	sb->nr_disks = nr_disks;
	sb->active_disks = active;
	sb->working_disks = working;
	sb->failed_disks = failed;
	sb->spare_disks = spare;

	sb->this_disk = sb->disks[rdev->desc_nr];
	sb->sb_csum = calc_sb_csum(sb);
}

/*
 * rdev_size_change for 0.90.0
 */
static unsigned long long
super_90_rdev_size_change(mdk_rdev_t *rdev, sector_t num_sectors)
{
	if (num_sectors && num_sectors < rdev->mddev->dev_sectors)
		return 0; /* component must fit device */
	if (rdev->mddev->bitmap_offset)
		return 0; /* can't move bitmap */
	rdev->sb_start = calc_dev_sboffset(rdev->bdev);
	if (!num_sectors || num_sectors > rdev->sb_start)
		num_sectors = rdev->sb_start;
	md_super_write(rdev->mddev, rdev, rdev->sb_start, rdev->sb_size,
		       rdev->sb_page);
	md_super_wait(rdev->mddev);
	return num_sectors / 2; /* kB for sysfs */
}


/*
 * version 1 superblock
 */

static __le32 calc_sb_1_csum(struct mdp_superblock_1 * sb)
{
	__le32 disk_csum;
	u32 csum;
	unsigned long long newcsum;
	int size = 256 + le32_to_cpu(sb->max_dev)*2;
	__le32 *isuper = (__le32*)sb;
	int i;

	disk_csum = sb->sb_csum;
	sb->sb_csum = 0;
	newcsum = 0;
	for (i=0; size>=4; size -= 4 )
		newcsum += le32_to_cpu(*isuper++);

	if (size == 2)
		newcsum += le16_to_cpu(*(__le16*) isuper);

	csum = (newcsum & 0xffffffff) + (newcsum >> 32);
	sb->sb_csum = disk_csum;
	return cpu_to_le32(csum);
}

static int super_1_load(mdk_rdev_t *rdev, mdk_rdev_t *refdev, int minor_version)
{
	struct mdp_superblock_1 *sb;
	int ret;
	sector_t sb_start;
	char b[BDEVNAME_SIZE], b2[BDEVNAME_SIZE];
	int bmask;

	/*
	 * Calculate the position of the superblock in 512byte sectors.
	 * It is always aligned to a 4K boundary and
	 * depeding on minor_version, it can be:
	 * 0: At least 8K, but less than 12K, from end of device
	 * 1: At start of device
	 * 2: 4K from start of device.
	 */
	switch(minor_version) {
	case 0:
		sb_start = rdev->bdev->bd_inode->i_size >> 9;
		sb_start -= 8*2;
		sb_start &= ~(sector_t)(4*2-1);
		break;
	case 1:
		sb_start = 0;
		break;
	case 2:
		sb_start = 8;
		break;
	default:
		return -EINVAL;
	}
	rdev->sb_start = sb_start;

	/* superblock is rarely larger than 1K, but it can be larger,
	 * and it is safe to read 4k, so we do that
	 */
	ret = read_disk_sb(rdev, 4096);
	if (ret) return ret;


	sb = (struct mdp_superblock_1*)page_address(rdev->sb_page);

	if (sb->magic != cpu_to_le32(MD_SB_MAGIC) ||
	    sb->major_version != cpu_to_le32(1) ||
	    le32_to_cpu(sb->max_dev) > (4096-256)/2 ||
	    le64_to_cpu(sb->super_offset) != rdev->sb_start ||
	    (le32_to_cpu(sb->feature_map) & ~MD_FEATURE_ALL) != 0)
		return -EINVAL;

	if (calc_sb_1_csum(sb) != sb->sb_csum) {
		printk("md: invalid superblock checksum on %s\n",
			bdevname(rdev->bdev,b));
		return -EINVAL;
	}
	if (le64_to_cpu(sb->data_size) < 10) {
		printk("md: data_size too small on %s\n",
		       bdevname(rdev->bdev,b));
		return -EINVAL;
	}

	rdev->preferred_minor = 0xffff;
	rdev->data_offset = le64_to_cpu(sb->data_offset);
	atomic_set(&rdev->corrected_errors, le32_to_cpu(sb->cnt_corrected_read));

	rdev->sb_size = le32_to_cpu(sb->max_dev) * 2 + 256;
	bmask = queue_logical_block_size(rdev->bdev->bd_disk->queue)-1;
	if (rdev->sb_size & bmask)
		rdev->sb_size = (rdev->sb_size | bmask) + 1;

	if (minor_version
	    && rdev->data_offset < sb_start + (rdev->sb_size/512))
		return -EINVAL;

	if (sb->level == cpu_to_le32(LEVEL_MULTIPATH))
		rdev->desc_nr = -1;
	else
		rdev->desc_nr = le32_to_cpu(sb->dev_number);

	if (!refdev) {
		ret = 1;
	} else {
		__u64 ev1, ev2;
		struct mdp_superblock_1 *refsb = 
			(struct mdp_superblock_1*)page_address(refdev->sb_page);

		if (memcmp(sb->set_uuid, refsb->set_uuid, 16) != 0 ||
		    sb->level != refsb->level ||
		    sb->layout != refsb->layout ||
		    sb->chunksize != refsb->chunksize) {
			printk(KERN_WARNING "md: %s has strangely different"
				" superblock to %s\n",
				bdevname(rdev->bdev,b),
				bdevname(refdev->bdev,b2));
			return -EINVAL;
		}
		ev1 = le64_to_cpu(sb->events);
		ev2 = le64_to_cpu(refsb->events);

		if (ev1 > ev2)
			ret = 1;
		else
			ret = 0;
	}
	if (minor_version)
		rdev->sectors = (rdev->bdev->bd_inode->i_size >> 9) -
			le64_to_cpu(sb->data_offset);
	else
		rdev->sectors = rdev->sb_start;
	if (rdev->sectors < le64_to_cpu(sb->data_size))
		return -EINVAL;
	rdev->sectors = le64_to_cpu(sb->data_size);
	if (le64_to_cpu(sb->size) > rdev->sectors)
		return -EINVAL;
	return ret;
}

static int super_1_validate(mddev_t *mddev, mdk_rdev_t *rdev)
{
	struct mdp_superblock_1 *sb = (struct mdp_superblock_1*)page_address(rdev->sb_page);
	__u64 ev1 = le64_to_cpu(sb->events);

	rdev->raid_disk = -1;
	clear_bit(Faulty, &rdev->flags);
	clear_bit(In_sync, &rdev->flags);
	clear_bit(WriteMostly, &rdev->flags);
	clear_bit(BarriersNotsupp, &rdev->flags);

	if (mddev->raid_disks == 0) {
		mddev->major_version = 1;
		mddev->patch_version = 0;
		mddev->external = 0;
		mddev->chunk_sectors = le32_to_cpu(sb->chunksize);
		mddev->ctime = le64_to_cpu(sb->ctime) & ((1ULL << 32)-1);
		mddev->utime = le64_to_cpu(sb->utime) & ((1ULL << 32)-1);
		mddev->level = le32_to_cpu(sb->level);
		mddev->clevel[0] = 0;
		mddev->layout = le32_to_cpu(sb->layout);
		mddev->raid_disks = le32_to_cpu(sb->raid_disks);
		mddev->dev_sectors = le64_to_cpu(sb->size);
		mddev->events = ev1;
		mddev->bitmap_offset = 0;
		mddev->default_bitmap_offset = 1024 >> 9;
		
		mddev->recovery_cp = le64_to_cpu(sb->resync_offset);
		memcpy(mddev->uuid, sb->set_uuid, 16);

		mddev->max_disks =  (4096-256)/2;

		if ((le32_to_cpu(sb->feature_map) & MD_FEATURE_BITMAP_OFFSET) &&
		    mddev->bitmap_file == NULL )
			mddev->bitmap_offset = (__s32)le32_to_cpu(sb->bitmap_offset);

		if ((le32_to_cpu(sb->feature_map) & MD_FEATURE_RESHAPE_ACTIVE)) {
			mddev->reshape_position = le64_to_cpu(sb->reshape_position);
			mddev->delta_disks = le32_to_cpu(sb->delta_disks);
			mddev->new_level = le32_to_cpu(sb->new_level);
			mddev->new_layout = le32_to_cpu(sb->new_layout);
			mddev->new_chunk_sectors = le32_to_cpu(sb->new_chunk);
		} else {
			mddev->reshape_position = MaxSector;
			mddev->delta_disks = 0;
			mddev->new_level = mddev->level;
			mddev->new_layout = mddev->layout;
			mddev->new_chunk_sectors = mddev->chunk_sectors;
		}

	} else if (mddev->pers == NULL) {
		/* Insist of good event counter while assembling */
		++ev1;
		if (ev1 < mddev->events)
			return -EINVAL;
	} else if (mddev->bitmap) {
		/* If adding to array with a bitmap, then we can accept an
		 * older device, but not too old.
		 */
		if (ev1 < mddev->bitmap->events_cleared)
			return 0;
	} else {
		if (ev1 < mddev->events)
			/* just a hot-add of a new device, leave raid_disk at -1 */
			return 0;
	}
	if (mddev->level != LEVEL_MULTIPATH) {
		int role;
		if (rdev->desc_nr < 0 ||
		    rdev->desc_nr >= le32_to_cpu(sb->max_dev)) {
			role = 0xffff;
			rdev->desc_nr = -1;
		} else
			role = le16_to_cpu(sb->dev_roles[rdev->desc_nr]);
		switch(role) {
		case 0xffff: /* spare */
			break;
		case 0xfffe: /* faulty */
			set_bit(Faulty, &rdev->flags);
			break;
		default:
			if ((le32_to_cpu(sb->feature_map) &
			     MD_FEATURE_RECOVERY_OFFSET))
				rdev->recovery_offset = le64_to_cpu(sb->recovery_offset);
			else
				set_bit(In_sync, &rdev->flags);
			rdev->raid_disk = role;
			break;
		}
		if (sb->devflags & WriteMostly1)
			set_bit(WriteMostly, &rdev->flags);
	} else /* MULTIPATH are always insync */
		set_bit(In_sync, &rdev->flags);

	return 0;
}

static void super_1_sync(mddev_t *mddev, mdk_rdev_t *rdev)
{
	struct mdp_superblock_1 *sb;
	mdk_rdev_t *rdev2;
	int max_dev, i;
	/* make rdev->sb match mddev and rdev data. */

	sb = (struct mdp_superblock_1*)page_address(rdev->sb_page);

	sb->feature_map = 0;
	sb->pad0 = 0;
	sb->recovery_offset = cpu_to_le64(0);
	memset(sb->pad1, 0, sizeof(sb->pad1));
	memset(sb->pad2, 0, sizeof(sb->pad2));
	memset(sb->pad3, 0, sizeof(sb->pad3));

	sb->utime = cpu_to_le64((__u64)mddev->utime);
	sb->events = cpu_to_le64(mddev->events);
	if (mddev->in_sync)
		sb->resync_offset = cpu_to_le64(mddev->recovery_cp);
	else
		sb->resync_offset = cpu_to_le64(0);

	sb->cnt_corrected_read = cpu_to_le32(atomic_read(&rdev->corrected_errors));

	sb->raid_disks = cpu_to_le32(mddev->raid_disks);
	sb->size = cpu_to_le64(mddev->dev_sectors);
	sb->chunksize = cpu_to_le32(mddev->chunk_sectors);
	sb->level = cpu_to_le32(mddev->level);
	sb->layout = cpu_to_le32(mddev->layout);

	if (mddev->bitmap && mddev->bitmap_file == NULL) {
		sb->bitmap_offset = cpu_to_le32((__u32)mddev->bitmap_offset);
		sb->feature_map = cpu_to_le32(MD_FEATURE_BITMAP_OFFSET);
	}

	if (rdev->raid_disk >= 0 &&
	    !test_bit(In_sync, &rdev->flags)) {
		if (rdev->recovery_offset > 0) {
			sb->feature_map |=
				cpu_to_le32(MD_FEATURE_RECOVERY_OFFSET);
			sb->recovery_offset =
				cpu_to_le64(rdev->recovery_offset);
		}
	}

	if (mddev->reshape_position != MaxSector) {
		sb->feature_map |= cpu_to_le32(MD_FEATURE_RESHAPE_ACTIVE);
		sb->reshape_position = cpu_to_le64(mddev->reshape_position);
		sb->new_layout = cpu_to_le32(mddev->new_layout);
		sb->delta_disks = cpu_to_le32(mddev->delta_disks);
		sb->new_level = cpu_to_le32(mddev->new_level);
		sb->new_chunk = cpu_to_le32(mddev->new_chunk_sectors);
	}

	max_dev = 0;
	list_for_each_entry(rdev2, &mddev->disks, same_set)
		if (rdev2->desc_nr+1 > max_dev)
			max_dev = rdev2->desc_nr+1;

	if (max_dev > le32_to_cpu(sb->max_dev)) {
		int bmask;
		sb->max_dev = cpu_to_le32(max_dev);
		rdev->sb_size = max_dev * 2 + 256;
		bmask = queue_logical_block_size(rdev->bdev->bd_disk->queue)-1;
		if (rdev->sb_size & bmask)
			rdev->sb_size = (rdev->sb_size | bmask) + 1;
	}
	for (i=0; i<max_dev;i++)
		sb->dev_roles[i] = cpu_to_le16(0xfffe);
	
	list_for_each_entry(rdev2, &mddev->disks, same_set) {
		i = rdev2->desc_nr;
		if (test_bit(Faulty, &rdev2->flags))
			sb->dev_roles[i] = cpu_to_le16(0xfffe);
		else if (test_bit(In_sync, &rdev2->flags))
			sb->dev_roles[i] = cpu_to_le16(rdev2->raid_disk);
		else if (rdev2->raid_disk >= 0 && rdev2->recovery_offset > 0)
			sb->dev_roles[i] = cpu_to_le16(rdev2->raid_disk);
		else
			sb->dev_roles[i] = cpu_to_le16(0xffff);
	}

	sb->sb_csum = calc_sb_1_csum(sb);
}

static unsigned long long
super_1_rdev_size_change(mdk_rdev_t *rdev, sector_t num_sectors)
{
	struct mdp_superblock_1 *sb;
	sector_t max_sectors;
	if (num_sectors && num_sectors < rdev->mddev->dev_sectors)
		return 0; /* component must fit device */
	if (rdev->sb_start < rdev->data_offset) {
		/* minor versions 1 and 2; superblock before data */
		max_sectors = rdev->bdev->bd_inode->i_size >> 9;
		max_sectors -= rdev->data_offset;
		if (!num_sectors || num_sectors > max_sectors)
			num_sectors = max_sectors;
	} else if (rdev->mddev->bitmap_offset) {
		/* minor version 0 with bitmap we can't move */
		return 0;
	} else {
		/* minor version 0; superblock after data */
		sector_t sb_start;
		sb_start = (rdev->bdev->bd_inode->i_size >> 9) - 8*2;
		sb_start &= ~(sector_t)(4*2 - 1);
		max_sectors = rdev->sectors + sb_start - rdev->sb_start;
		if (!num_sectors || num_sectors > max_sectors)
			num_sectors = max_sectors;
		rdev->sb_start = sb_start;
	}
	sb = (struct mdp_superblock_1 *) page_address(rdev->sb_page);
	sb->data_size = cpu_to_le64(num_sectors);
	sb->super_offset = rdev->sb_start;
	sb->sb_csum = calc_sb_1_csum(sb);
	md_super_write(rdev->mddev, rdev, rdev->sb_start, rdev->sb_size,
		       rdev->sb_page);
	md_super_wait(rdev->mddev);
	return num_sectors / 2; /* kB for sysfs */
}

static struct super_type super_types[] = {
	[0] = {
		.name	= "0.90.0",
		.owner	= THIS_MODULE,
		.load_super	    = super_90_load,
		.validate_super	    = super_90_validate,
		.sync_super	    = super_90_sync,
		.rdev_size_change   = super_90_rdev_size_change,
	},
	[1] = {
		.name	= "md-1",
		.owner	= THIS_MODULE,
		.load_super	    = super_1_load,
		.validate_super	    = super_1_validate,
		.sync_super	    = super_1_sync,
		.rdev_size_change   = super_1_rdev_size_change,
	},
};

static int match_mddev_units(mddev_t *mddev1, mddev_t *mddev2)
{
	mdk_rdev_t *rdev, *rdev2;

	rcu_read_lock();
	rdev_for_each_rcu(rdev, mddev1)
		rdev_for_each_rcu(rdev2, mddev2)
			if (rdev->bdev->bd_contains ==
			    rdev2->bdev->bd_contains) {
				rcu_read_unlock();
				return 1;
			}
	rcu_read_unlock();
	return 0;
}

static LIST_HEAD(pending_raid_disks);

/*
 * Try to register data integrity profile for an mddev
 *
 * This is called when an array is started and after a disk has been kicked
 * from the array. It only succeeds if all working and active component devices
 * are integrity capable with matching profiles.
 */
int md_integrity_register(mddev_t *mddev)
{
	mdk_rdev_t *rdev, *reference = NULL;

	if (list_empty(&mddev->disks))
		return 0; /* nothing to do */
	if (blk_get_integrity(mddev->gendisk))
		return 0; /* already registered */
	list_for_each_entry(rdev, &mddev->disks, same_set) {
		/* skip spares and non-functional disks */
		if (test_bit(Faulty, &rdev->flags))
			continue;
		if (rdev->raid_disk < 0)
			continue;
		/*
		 * If at least one rdev is not integrity capable, we can not
		 * enable data integrity for the md device.
		 */
		if (!bdev_get_integrity(rdev->bdev))
			return -EINVAL;
		if (!reference) {
			/* Use the first rdev as the reference */
			reference = rdev;
			continue;
		}
		/* does this rdev's profile match the reference profile? */
		if (blk_integrity_compare(reference->bdev->bd_disk,
				rdev->bdev->bd_disk) < 0)
			return -EINVAL;
	}
	/*
	 * All component devices are integrity capable and have matching
	 * profiles, register the common profile for the md device.
	 */
	if (blk_integrity_register(mddev->gendisk,
			bdev_get_integrity(reference->bdev)) != 0) {
		printk(KERN_ERR "md: failed to register integrity for %s\n",
			mdname(mddev));
		return -EINVAL;
	}
	printk(KERN_NOTICE "md: data integrity on %s enabled\n",
		mdname(mddev));
	return 0;
}
EXPORT_SYMBOL(md_integrity_register);

/* Disable data integrity if non-capable/non-matching disk is being added */
void md_integrity_add_rdev(mdk_rdev_t *rdev, mddev_t *mddev)
{
	struct blk_integrity *bi_rdev = bdev_get_integrity(rdev->bdev);
	struct blk_integrity *bi_mddev = blk_get_integrity(mddev->gendisk);

	if (!bi_mddev) /* nothing to do */
		return;
	if (rdev->raid_disk < 0) /* skip spares */
		return;
	if (bi_rdev && blk_integrity_compare(mddev->gendisk,
					     rdev->bdev->bd_disk) >= 0)
		return;
	printk(KERN_NOTICE "disabling data integrity on %s\n", mdname(mddev));
	blk_integrity_unregister(mddev->gendisk);
}
EXPORT_SYMBOL(md_integrity_add_rdev);

static int bind_rdev_to_array(mdk_rdev_t * rdev, mddev_t * mddev)
{
	char b[BDEVNAME_SIZE];
	struct kobject *ko;
	char *s;
	int err;

	if (rdev->mddev) {
		MD_BUG();
		return -EINVAL;
	}

	/* prevent duplicates */
	if (find_rdev(mddev, rdev->bdev->bd_dev))
		return -EEXIST;

	/* make sure rdev->sectors exceeds mddev->dev_sectors */
	if (rdev->sectors && (mddev->dev_sectors == 0 ||
			rdev->sectors < mddev->dev_sectors)) {
		if (mddev->pers) {
			/* Cannot change size, so fail
			 * If mddev->level <= 0, then we don't care
			 * about aligning sizes (e.g. linear)
			 */
			if (mddev->level > 0)
				return -ENOSPC;
		} else
			mddev->dev_sectors = rdev->sectors;
	}

	/* Verify rdev->desc_nr is unique.
	 * If it is -1, assign a free number, else
	 * check number is not in use
	 */
	if (rdev->desc_nr < 0) {
		int choice = 0;
		if (mddev->pers) choice = mddev->raid_disks;
		while (find_rdev_nr(mddev, choice))
			choice++;
		rdev->desc_nr = choice;
	} else {
		if (find_rdev_nr(mddev, rdev->desc_nr))
			return -EBUSY;
	}
	if (mddev->max_disks && rdev->desc_nr >= mddev->max_disks) {
		printk(KERN_WARNING "md: %s: array is limited to %d devices\n",
		       mdname(mddev), mddev->max_disks);
		return -EBUSY;
	}
	bdevname(rdev->bdev,b);
	while ( (s=strchr(b, '/')) != NULL)
		*s = '!';

	rdev->mddev = mddev;
	printk(KERN_INFO "md: bind<%s>\n", b);

	if ((err = kobject_add(&rdev->kobj, &mddev->kobj, "dev-%s", b)))
		goto fail;

	ko = &part_to_dev(rdev->bdev->bd_part)->kobj;
	if ((err = sysfs_create_link(&rdev->kobj, ko, "block"))) {
		kobject_del(&rdev->kobj);
		goto fail;
	}
	rdev->sysfs_state = sysfs_get_dirent(rdev->kobj.sd, "state");

	list_add_rcu(&rdev->same_set, &mddev->disks);
	bd_claim_by_disk(rdev->bdev, rdev->bdev->bd_holder, mddev->gendisk);

	/* May as well allow recovery to be retried once */
	mddev->recovery_disabled = 0;

	return 0;

 fail:
	printk(KERN_WARNING "md: failed to register dev-%s for %s\n",
	       b, mdname(mddev));
	return err;
}

static void md_delayed_delete(struct work_struct *ws)
{
	mdk_rdev_t *rdev = container_of(ws, mdk_rdev_t, del_work);
	kobject_del(&rdev->kobj);
	kobject_put(&rdev->kobj);
}

static void unbind_rdev_from_array(mdk_rdev_t * rdev)
{
	char b[BDEVNAME_SIZE];
	if (!rdev->mddev) {
		MD_BUG();
		return;
	}
	bd_release_from_disk(rdev->bdev, rdev->mddev->gendisk);
	list_del_rcu(&rdev->same_set);
	printk(KERN_INFO "md: unbind<%s>\n", bdevname(rdev->bdev,b));
	rdev->mddev = NULL;
	sysfs_remove_link(&rdev->kobj, "block");
	sysfs_put(rdev->sysfs_state);
	rdev->sysfs_state = NULL;
	/* We need to delay this, otherwise we can deadlock when
	 * writing to 'remove' to "dev/state".  We also need
	 * to delay it due to rcu usage.
	 */
	synchronize_rcu();
	INIT_WORK(&rdev->del_work, md_delayed_delete);
	kobject_get(&rdev->kobj);
	schedule_work(&rdev->del_work);
}

/*
 * prevent the device from being mounted, repartitioned or
 * otherwise reused by a RAID array (or any other kernel
 * subsystem), by bd_claiming the device.
 */
static int lock_rdev(mdk_rdev_t *rdev, dev_t dev, int shared)
{
	int err = 0;
	struct block_device *bdev;
	char b[BDEVNAME_SIZE];

	bdev = open_by_devnum(dev, FMODE_READ|FMODE_WRITE);
	if (IS_ERR(bdev)) {
		printk(KERN_ERR "md: could not open %s.\n",
			__bdevname(dev, b));
		return PTR_ERR(bdev);
	}
	err = bd_claim(bdev, shared ? (mdk_rdev_t *)lock_rdev : rdev);
	if (err) {
		printk(KERN_ERR "md: could not bd_claim %s.\n",
			bdevname(bdev, b));
		blkdev_put(bdev, FMODE_READ|FMODE_WRITE);
		return err;
	}
	if (!shared)
		set_bit(AllReserved, &rdev->flags);
	rdev->bdev = bdev;
	return err;
}

static void unlock_rdev(mdk_rdev_t *rdev)
{
	struct block_device *bdev = rdev->bdev;
	rdev->bdev = NULL;
	if (!bdev)
		MD_BUG();
	bd_release(bdev);
	blkdev_put(bdev, FMODE_READ|FMODE_WRITE);
}

void md_autodetect_dev(dev_t dev);

static void export_rdev(mdk_rdev_t * rdev)
{
	char b[BDEVNAME_SIZE];
	printk(KERN_INFO "md: export_rdev(%s)\n",
		bdevname(rdev->bdev,b));
	if (rdev->mddev)
		MD_BUG();
	free_disk_sb(rdev);
#ifndef MODULE
	if (test_bit(AutoDetected, &rdev->flags))
		md_autodetect_dev(rdev->bdev->bd_dev);
#endif
	unlock_rdev(rdev);
	kobject_put(&rdev->kobj);
}

static void kick_rdev_from_array(mdk_rdev_t * rdev)
{
	unbind_rdev_from_array(rdev);
	export_rdev(rdev);
}

static void export_array(mddev_t *mddev)
{
	mdk_rdev_t *rdev, *tmp;

	rdev_for_each(rdev, tmp, mddev) {
		if (!rdev->mddev) {
			MD_BUG();
			continue;
		}
		kick_rdev_from_array(rdev);
	}
	if (!list_empty(&mddev->disks))
		MD_BUG();
	mddev->raid_disks = 0;
	mddev->major_version = 0;
}

static void print_desc(mdp_disk_t *desc)
{
	printk(" DISK<N:%d,(%d,%d),R:%d,S:%d>\n", desc->number,
		desc->major,desc->minor,desc->raid_disk,desc->state);
}

static void print_sb_90(mdp_super_t *sb)
{
	int i;

	printk(KERN_INFO 
		"md:  SB: (V:%d.%d.%d) ID:<%08x.%08x.%08x.%08x> CT:%08x\n",
		sb->major_version, sb->minor_version, sb->patch_version,
		sb->set_uuid0, sb->set_uuid1, sb->set_uuid2, sb->set_uuid3,
		sb->ctime);
	printk(KERN_INFO "md:     L%d S%08d ND:%d RD:%d md%d LO:%d CS:%d\n",
		sb->level, sb->size, sb->nr_disks, sb->raid_disks,
		sb->md_minor, sb->layout, sb->chunk_size);
	printk(KERN_INFO "md:     UT:%08x ST:%d AD:%d WD:%d"
		" FD:%d SD:%d CSUM:%08x E:%08lx\n",
		sb->utime, sb->state, sb->active_disks, sb->working_disks,
		sb->failed_disks, sb->spare_disks,
		sb->sb_csum, (unsigned long)sb->events_lo);

	printk(KERN_INFO);
	for (i = 0; i < MD_SB_DISKS; i++) {
		mdp_disk_t *desc;

		desc = sb->disks + i;
		if (desc->number || desc->major || desc->minor ||
		    desc->raid_disk || (desc->state && (desc->state != 4))) {
			printk("     D %2d: ", i);
			print_desc(desc);
		}
	}
	printk(KERN_INFO "md:     THIS: ");
	print_desc(&sb->this_disk);
}

static void print_sb_1(struct mdp_superblock_1 *sb)
{
	__u8 *uuid;

	uuid = sb->set_uuid;
	printk(KERN_INFO
	       "md:  SB: (V:%u) (F:0x%08x) Array-ID:<%02x%02x%02x%02x"
	       ":%02x%02x:%02x%02x:%02x%02x:%02x%02x%02x%02x%02x%02x>\n"
	       "md:    Name: \"%s\" CT:%llu\n",
		le32_to_cpu(sb->major_version),
		le32_to_cpu(sb->feature_map),
		uuid[0], uuid[1], uuid[2], uuid[3],
		uuid[4], uuid[5], uuid[6], uuid[7],
		uuid[8], uuid[9], uuid[10], uuid[11],
		uuid[12], uuid[13], uuid[14], uuid[15],
		sb->set_name,
		(unsigned long long)le64_to_cpu(sb->ctime)
		       & MD_SUPERBLOCK_1_TIME_SEC_MASK);

	uuid = sb->device_uuid;
	printk(KERN_INFO
	       "md:       L%u SZ%llu RD:%u LO:%u CS:%u DO:%llu DS:%llu SO:%llu"
			" RO:%llu\n"
	       "md:     Dev:%08x UUID: %02x%02x%02x%02x:%02x%02x:%02x%02x:%02x%02x"
	                ":%02x%02x%02x%02x%02x%02x\n"
	       "md:       (F:0x%08x) UT:%llu Events:%llu ResyncOffset:%llu CSUM:0x%08x\n"
	       "md:         (MaxDev:%u) \n",
		le32_to_cpu(sb->level),
		(unsigned long long)le64_to_cpu(sb->size),
		le32_to_cpu(sb->raid_disks),
		le32_to_cpu(sb->layout),
		le32_to_cpu(sb->chunksize),
		(unsigned long long)le64_to_cpu(sb->data_offset),
		(unsigned long long)le64_to_cpu(sb->data_size),
		(unsigned long long)le64_to_cpu(sb->super_offset),
		(unsigned long long)le64_to_cpu(sb->recovery_offset),
		le32_to_cpu(sb->dev_number),
		uuid[0], uuid[1], uuid[2], uuid[3],
		uuid[4], uuid[5], uuid[6], uuid[7],
		uuid[8], uuid[9], uuid[10], uuid[11],
		uuid[12], uuid[13], uuid[14], uuid[15],
		sb->devflags,
		(unsigned long long)le64_to_cpu(sb->utime) & MD_SUPERBLOCK_1_TIME_SEC_MASK,
		(unsigned long long)le64_to_cpu(sb->events),
		(unsigned long long)le64_to_cpu(sb->resync_offset),
		le32_to_cpu(sb->sb_csum),
		le32_to_cpu(sb->max_dev)
		);
}

static void print_rdev(mdk_rdev_t *rdev, int major_version)
{
	char b[BDEVNAME_SIZE];
	printk(KERN_INFO "md: rdev %s, Sect:%08llu F:%d S:%d DN:%u\n",
		bdevname(rdev->bdev, b), (unsigned long long)rdev->sectors,
	        test_bit(Faulty, &rdev->flags), test_bit(In_sync, &rdev->flags),
	        rdev->desc_nr);
	if (rdev->sb_loaded) {
		printk(KERN_INFO "md: rdev superblock (MJ:%d):\n", major_version);
		switch (major_version) {
		case 0:
			print_sb_90((mdp_super_t*)page_address(rdev->sb_page));
			break;
		case 1:
			print_sb_1((struct mdp_superblock_1 *)page_address(rdev->sb_page));
			break;
		}
	} else
		printk(KERN_INFO "md: no rdev superblock!\n");
}

static void md_print_devices(void)
{
	struct list_head *tmp;
	mdk_rdev_t *rdev;
	mddev_t *mddev;
	char b[BDEVNAME_SIZE];

	printk("\n");
	printk("md:	**********************************\n");
	printk("md:	* <COMPLETE RAID STATE PRINTOUT> *\n");
	printk("md:	**********************************\n");
	for_each_mddev(mddev, tmp) {

		if (mddev->bitmap)
			bitmap_print_sb(mddev->bitmap);
		else
			printk("%s: ", mdname(mddev));
		list_for_each_entry(rdev, &mddev->disks, same_set)
			printk("<%s>", bdevname(rdev->bdev,b));
		printk("\n");

		list_for_each_entry(rdev, &mddev->disks, same_set)
			print_rdev(rdev, mddev->major_version);
	}
	printk("md:	**********************************\n");
	printk("\n");
}


static void sync_sbs(mddev_t * mddev, int nospares)
{
	/* Update each superblock (in-memory image), but
	 * if we are allowed to, skip spares which already
	 * have the right event counter, or have one earlier
	 * (which would mean they aren't being marked as dirty
	 * with the rest of the array)
	 */
	mdk_rdev_t *rdev;

	/* First make sure individual recovery_offsets are correct */
	list_for_each_entry(rdev, &mddev->disks, same_set) {
		if (rdev->raid_disk >= 0 &&
		    !test_bit(In_sync, &rdev->flags) &&
		    mddev->curr_resync_completed > rdev->recovery_offset)
				rdev->recovery_offset = mddev->curr_resync_completed;

	}	
	list_for_each_entry(rdev, &mddev->disks, same_set) {
		if (rdev->sb_events == mddev->events ||
		    (nospares &&
		     rdev->raid_disk < 0 &&
		     (rdev->sb_events&1)==0 &&
		     rdev->sb_events+1 == mddev->events)) {
			/* Don't update this superblock */
			rdev->sb_loaded = 2;
		} else {
			super_types[mddev->major_version].
				sync_super(mddev, rdev);
			rdev->sb_loaded = 1;
		}
	}
}

static void md_update_sb(mddev_t * mddev, int force_change)
{
	mdk_rdev_t *rdev;
	int sync_req;
	int nospares = 0;

	mddev->utime = get_seconds();
	if (mddev->external)
		return;
repeat:
	spin_lock_irq(&mddev->write_lock);

	set_bit(MD_CHANGE_PENDING, &mddev->flags);
	if (test_and_clear_bit(MD_CHANGE_DEVS, &mddev->flags))
		force_change = 1;
	if (test_and_clear_bit(MD_CHANGE_CLEAN, &mddev->flags))
		/* just a clean<-> dirty transition, possibly leave spares alone,
		 * though if events isn't the right even/odd, we will have to do
		 * spares after all
		 */
		nospares = 1;
	if (force_change)
		nospares = 0;
	if (mddev->degraded)
		/* If the array is degraded, then skipping spares is both
		 * dangerous and fairly pointless.
		 * Dangerous because a device that was removed from the array
		 * might have a event_count that still looks up-to-date,
		 * so it can be re-added without a resync.
		 * Pointless because if there are any spares to skip,
		 * then a recovery will happen and soon that array won't
		 * be degraded any more and the spare can go back to sleep then.
		 */
		nospares = 0;

	sync_req = mddev->in_sync;

	/* If this is just a dirty<->clean transition, and the array is clean
	 * and 'events' is odd, we can roll back to the previous clean state */
	if (nospares
	    && (mddev->in_sync && mddev->recovery_cp == MaxSector)
	    && (mddev->events & 1)
	    && mddev->events != 1)
		mddev->events--;
	else {
		/* otherwise we have to go forward and ... */
		mddev->events ++;
		if (!mddev->in_sync || mddev->recovery_cp != MaxSector) { /* not clean */
			/* .. if the array isn't clean, an 'even' event must also go
			 * to spares. */
			if ((mddev->events&1)==0)
				nospares = 0;
		} else {
			/* otherwise an 'odd' event must go to spares */
			if ((mddev->events&1))
				nospares = 0;
		}
	}

	if (!mddev->events) {
		/*
		 * oops, this 64-bit counter should never wrap.
		 * Either we are in around ~1 trillion A.C., assuming
		 * 1 reboot per second, or we have a bug:
		 */
		MD_BUG();
		mddev->events --;
	}

	/*
	 * do not write anything to disk if using
	 * nonpersistent superblocks
	 */
	if (!mddev->persistent) {
		if (!mddev->external)
			clear_bit(MD_CHANGE_PENDING, &mddev->flags);

		spin_unlock_irq(&mddev->write_lock);
		wake_up(&mddev->sb_wait);
		return;
	}
	sync_sbs(mddev, nospares);
	spin_unlock_irq(&mddev->write_lock);

	dprintk(KERN_INFO 
		"md: updating %s RAID superblock on device (in sync %d)\n",
		mdname(mddev),mddev->in_sync);

	bitmap_update_sb(mddev->bitmap);
	list_for_each_entry(rdev, &mddev->disks, same_set) {
		char b[BDEVNAME_SIZE];
		dprintk(KERN_INFO "md: ");
		if (rdev->sb_loaded != 1)
			continue; /* no noise on spare devices */
		if (test_bit(Faulty, &rdev->flags))
			dprintk("(skipping faulty ");

		dprintk("%s ", bdevname(rdev->bdev,b));
		if (!test_bit(Faulty, &rdev->flags)) {
			md_super_write(mddev,rdev,
				       rdev->sb_start, rdev->sb_size,
				       rdev->sb_page);
			dprintk(KERN_INFO "(write) %s's sb offset: %llu\n",
				bdevname(rdev->bdev,b),
				(unsigned long long)rdev->sb_start);
			rdev->sb_events = mddev->events;

		} else
			dprintk(")\n");
		if (mddev->level == LEVEL_MULTIPATH)
			/* only need to write one superblock... */
			break;
	}
	md_super_wait(mddev);
	/* if there was a failure, MD_CHANGE_DEVS was set, and we re-write super */

	spin_lock_irq(&mddev->write_lock);
	if (mddev->in_sync != sync_req ||
	    test_bit(MD_CHANGE_DEVS, &mddev->flags)) {
		/* have to write it out again */
		spin_unlock_irq(&mddev->write_lock);
		goto repeat;
	}
	clear_bit(MD_CHANGE_PENDING, &mddev->flags);
	spin_unlock_irq(&mddev->write_lock);
	wake_up(&mddev->sb_wait);
	if (test_bit(MD_RECOVERY_RUNNING, &mddev->recovery))
		sysfs_notify(&mddev->kobj, NULL, "sync_completed");

}

/* words written to sysfs files may, or may not, be \n terminated.
 * We want to accept with case. For this we use cmd_match.
 */
static int cmd_match(const char *cmd, const char *str)
{
	/* See if cmd, written into a sysfs file, matches
	 * str.  They must either be the same, or cmd can
	 * have a trailing newline
	 */
	while (*cmd && *str && *cmd == *str) {
		cmd++;
		str++;
	}
	if (*cmd == '\n')
		cmd++;
	if (*str || *cmd)
		return 0;
	return 1;
}

struct rdev_sysfs_entry {
	struct attribute attr;
	ssize_t (*show)(mdk_rdev_t *, char *);
	ssize_t (*store)(mdk_rdev_t *, const char *, size_t);
};

static ssize_t
state_show(mdk_rdev_t *rdev, char *page)
{
	char *sep = "";
	size_t len = 0;

	if (test_bit(Faulty, &rdev->flags)) {
		len+= sprintf(page+len, "%sfaulty",sep);
		sep = ",";
	}
	if (test_bit(In_sync, &rdev->flags)) {
		len += sprintf(page+len, "%sin_sync",sep);
		sep = ",";
	}
	if (test_bit(WriteMostly, &rdev->flags)) {
		len += sprintf(page+len, "%swrite_mostly",sep);
		sep = ",";
	}
	if (test_bit(Blocked, &rdev->flags)) {
		len += sprintf(page+len, "%sblocked", sep);
		sep = ",";
	}
	if (!test_bit(Faulty, &rdev->flags) &&
	    !test_bit(In_sync, &rdev->flags)) {
		len += sprintf(page+len, "%sspare", sep);
		sep = ",";
	}
	return len+sprintf(page+len, "\n");
}

static ssize_t
state_store(mdk_rdev_t *rdev, const char *buf, size_t len)
{
	/* can write
	 *  faulty  - simulates and error
	 *  remove  - disconnects the device
	 *  writemostly - sets write_mostly
	 *  -writemostly - clears write_mostly
	 *  blocked - sets the Blocked flag
	 *  -blocked - clears the Blocked flag
	 *  insync - sets Insync providing device isn't active
	 */
	int err = -EINVAL;
	if (cmd_match(buf, "faulty") && rdev->mddev->pers) {
		md_error(rdev->mddev, rdev);
		err = 0;
	} else if (cmd_match(buf, "remove")) {
		if (rdev->raid_disk >= 0)
			err = -EBUSY;
		else {
			mddev_t *mddev = rdev->mddev;
			kick_rdev_from_array(rdev);
			if (mddev->pers)
				md_update_sb(mddev, 1);
			md_new_event(mddev);
			err = 0;
		}
	} else if (cmd_match(buf, "writemostly")) {
		set_bit(WriteMostly, &rdev->flags);
		err = 0;
	} else if (cmd_match(buf, "-writemostly")) {
		clear_bit(WriteMostly, &rdev->flags);
		err = 0;
	} else if (cmd_match(buf, "blocked")) {
		set_bit(Blocked, &rdev->flags);
		err = 0;
	} else if (cmd_match(buf, "-blocked")) {
		clear_bit(Blocked, &rdev->flags);
		wake_up(&rdev->blocked_wait);
		set_bit(MD_RECOVERY_NEEDED, &rdev->mddev->recovery);
		md_wakeup_thread(rdev->mddev->thread);

		err = 0;
	} else if (cmd_match(buf, "insync") && rdev->raid_disk == -1) {
		set_bit(In_sync, &rdev->flags);
		err = 0;
	}
	if (!err && rdev->sysfs_state)
		sysfs_notify_dirent(rdev->sysfs_state);
	return err ? err : len;
}
static struct rdev_sysfs_entry rdev_state =
__ATTR(state, S_IRUGO|S_IWUSR, state_show, state_store);

static ssize_t
errors_show(mdk_rdev_t *rdev, char *page)
{
	return sprintf(page, "%d\n", atomic_read(&rdev->corrected_errors));
}

static ssize_t
errors_store(mdk_rdev_t *rdev, const char *buf, size_t len)
{
	char *e;
	unsigned long n = simple_strtoul(buf, &e, 10);
	if (*buf && (*e == 0 || *e == '\n')) {
		atomic_set(&rdev->corrected_errors, n);
		return len;
	}
	return -EINVAL;
}
static struct rdev_sysfs_entry rdev_errors =
__ATTR(errors, S_IRUGO|S_IWUSR, errors_show, errors_store);

static ssize_t
slot_show(mdk_rdev_t *rdev, char *page)
{
	if (rdev->raid_disk < 0)
		return sprintf(page, "none\n");
	else
		return sprintf(page, "%d\n", rdev->raid_disk);
}

static ssize_t
slot_store(mdk_rdev_t *rdev, const char *buf, size_t len)
{
	char *e;
	int err;
	char nm[20];
	int slot = simple_strtoul(buf, &e, 10);
	if (strncmp(buf, "none", 4)==0)
		slot = -1;
	else if (e==buf || (*e && *e!= '\n'))
		return -EINVAL;
	if (rdev->mddev->pers && slot == -1) {
		/* Setting 'slot' on an active array requires also
		 * updating the 'rd%d' link, and communicating
		 * with the personality with ->hot_*_disk.
		 * For now we only support removing
		 * failed/spare devices.  This normally happens automatically,
		 * but not when the metadata is externally managed.
		 */
		if (rdev->raid_disk == -1)
			return -EEXIST;
		/* personality does all needed checks */
		if (rdev->mddev->pers->hot_add_disk == NULL)
			return -EINVAL;
		err = rdev->mddev->pers->
			hot_remove_disk(rdev->mddev, rdev->raid_disk);
		if (err)
			return err;
		sprintf(nm, "rd%d", rdev->raid_disk);
		sysfs_remove_link(&rdev->mddev->kobj, nm);
		set_bit(MD_RECOVERY_NEEDED, &rdev->mddev->recovery);
		md_wakeup_thread(rdev->mddev->thread);
	} else if (rdev->mddev->pers) {
		mdk_rdev_t *rdev2;
		/* Activating a spare .. or possibly reactivating
		 * if we ever get bitmaps working here.
		 */

		if (rdev->raid_disk != -1)
			return -EBUSY;

		if (rdev->mddev->pers->hot_add_disk == NULL)
			return -EINVAL;

		list_for_each_entry(rdev2, &rdev->mddev->disks, same_set)
			if (rdev2->raid_disk == slot)
				return -EEXIST;

		rdev->raid_disk = slot;
		if (test_bit(In_sync, &rdev->flags))
			rdev->saved_raid_disk = slot;
		else
			rdev->saved_raid_disk = -1;
		err = rdev->mddev->pers->
			hot_add_disk(rdev->mddev, rdev);
		if (err) {
			rdev->raid_disk = -1;
			return err;
		} else
			sysfs_notify_dirent(rdev->sysfs_state);
		sprintf(nm, "rd%d", rdev->raid_disk);
		if (sysfs_create_link(&rdev->mddev->kobj, &rdev->kobj, nm))
			printk(KERN_WARNING
			       "md: cannot register "
			       "%s for %s\n",
			       nm, mdname(rdev->mddev));

		/* don't wakeup anyone, leave that to userspace. */
	} else {
		if (slot >= rdev->mddev->raid_disks)
			return -ENOSPC;
		rdev->raid_disk = slot;
		/* assume it is working */
		clear_bit(Faulty, &rdev->flags);
		clear_bit(WriteMostly, &rdev->flags);
		set_bit(In_sync, &rdev->flags);
		sysfs_notify_dirent(rdev->sysfs_state);
	}
	return len;
}


static struct rdev_sysfs_entry rdev_slot =
__ATTR(slot, S_IRUGO|S_IWUSR, slot_show, slot_store);

static ssize_t
offset_show(mdk_rdev_t *rdev, char *page)
{
	return sprintf(page, "%llu\n", (unsigned long long)rdev->data_offset);
}

static ssize_t
offset_store(mdk_rdev_t *rdev, const char *buf, size_t len)
{
	char *e;
	unsigned long long offset = simple_strtoull(buf, &e, 10);
	if (e==buf || (*e && *e != '\n'))
		return -EINVAL;
	if (rdev->mddev->pers && rdev->raid_disk >= 0)
		return -EBUSY;
	if (rdev->sectors && rdev->mddev->external)
		/* Must set offset before size, so overlap checks
		 * can be sane */
		return -EBUSY;
	rdev->data_offset = offset;
	return len;
}

static struct rdev_sysfs_entry rdev_offset =
__ATTR(offset, S_IRUGO|S_IWUSR, offset_show, offset_store);

static ssize_t
rdev_size_show(mdk_rdev_t *rdev, char *page)
{
	return sprintf(page, "%llu\n", (unsigned long long)rdev->sectors / 2);
}

static int overlaps(sector_t s1, sector_t l1, sector_t s2, sector_t l2)
{
	/* check if two start/length pairs overlap */
	if (s1+l1 <= s2)
		return 0;
	if (s2+l2 <= s1)
		return 0;
	return 1;
}

static int strict_blocks_to_sectors(const char *buf, sector_t *sectors)
{
	unsigned long long blocks;
	sector_t new;

	if (strict_strtoull(buf, 10, &blocks) < 0)
		return -EINVAL;

	if (blocks & 1ULL << (8 * sizeof(blocks) - 1))
		return -EINVAL; /* sector conversion overflow */

	new = blocks * 2;
	if (new != blocks * 2)
		return -EINVAL; /* unsigned long long to sector_t overflow */

	*sectors = new;
	return 0;
}

static ssize_t
rdev_size_store(mdk_rdev_t *rdev, const char *buf, size_t len)
{
	mddev_t *my_mddev = rdev->mddev;
	sector_t oldsectors = rdev->sectors;
	sector_t sectors;

	if (strict_blocks_to_sectors(buf, &sectors) < 0)
		return -EINVAL;
	if (my_mddev->pers && rdev->raid_disk >= 0) {
		if (my_mddev->persistent) {
			sectors = super_types[my_mddev->major_version].
				rdev_size_change(rdev, sectors);
			if (!sectors)
				return -EBUSY;
		} else if (!sectors)
			sectors = (rdev->bdev->bd_inode->i_size >> 9) -
				rdev->data_offset;
	}
	if (sectors < my_mddev->dev_sectors)
		return -EINVAL; /* component must fit device */

	rdev->sectors = sectors;
	if (sectors > oldsectors && my_mddev->external) {
		/* need to check that all other rdevs with the same ->bdev
		 * do not overlap.  We need to unlock the mddev to avoid
		 * a deadlock.  We have already changed rdev->sectors, and if
		 * we have to change it back, we will have the lock again.
		 */
		mddev_t *mddev;
		int overlap = 0;
		struct list_head *tmp;

		mddev_unlock(my_mddev);
		for_each_mddev(mddev, tmp) {
			mdk_rdev_t *rdev2;

			mddev_lock(mddev);
			list_for_each_entry(rdev2, &mddev->disks, same_set)
				if (test_bit(AllReserved, &rdev2->flags) ||
				    (rdev->bdev == rdev2->bdev &&
				     rdev != rdev2 &&
				     overlaps(rdev->data_offset, rdev->sectors,
					      rdev2->data_offset,
					      rdev2->sectors))) {
					overlap = 1;
					break;
				}
			mddev_unlock(mddev);
			if (overlap) {
				mddev_put(mddev);
				break;
			}
		}
		mddev_lock(my_mddev);
		if (overlap) {
			/* Someone else could have slipped in a size
			 * change here, but doing so is just silly.
			 * We put oldsectors back because we *know* it is
			 * safe, and trust userspace not to race with
			 * itself
			 */
			rdev->sectors = oldsectors;
			return -EBUSY;
		}
	}
	return len;
}

static struct rdev_sysfs_entry rdev_size =
__ATTR(size, S_IRUGO|S_IWUSR, rdev_size_show, rdev_size_store);

static struct attribute *rdev_default_attrs[] = {
	&rdev_state.attr,
	&rdev_errors.attr,
	&rdev_slot.attr,
	&rdev_offset.attr,
	&rdev_size.attr,
	NULL,
};
static ssize_t
rdev_attr_show(struct kobject *kobj, struct attribute *attr, char *page)
{
	struct rdev_sysfs_entry *entry = container_of(attr, struct rdev_sysfs_entry, attr);
	mdk_rdev_t *rdev = container_of(kobj, mdk_rdev_t, kobj);
	mddev_t *mddev = rdev->mddev;
	ssize_t rv;

	if (!entry->show)
		return -EIO;

	rv = mddev ? mddev_lock(mddev) : -EBUSY;
	if (!rv) {
		if (rdev->mddev == NULL)
			rv = -EBUSY;
		else
			rv = entry->show(rdev, page);
		mddev_unlock(mddev);
	}
	return rv;
}

static ssize_t
rdev_attr_store(struct kobject *kobj, struct attribute *attr,
	      const char *page, size_t length)
{
	struct rdev_sysfs_entry *entry = container_of(attr, struct rdev_sysfs_entry, attr);
	mdk_rdev_t *rdev = container_of(kobj, mdk_rdev_t, kobj);
	ssize_t rv;
	mddev_t *mddev = rdev->mddev;

	if (!entry->store)
		return -EIO;
	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;
	rv = mddev ? mddev_lock(mddev): -EBUSY;
	if (!rv) {
		if (rdev->mddev == NULL)
			rv = -EBUSY;
		else
			rv = entry->store(rdev, page, length);
		mddev_unlock(mddev);
	}
	return rv;
}

static void rdev_free(struct kobject *ko)
{
	mdk_rdev_t *rdev = container_of(ko, mdk_rdev_t, kobj);
	kfree(rdev);
}
static struct sysfs_ops rdev_sysfs_ops = {
	.show		= rdev_attr_show,
	.store		= rdev_attr_store,
};
static struct kobj_type rdev_ktype = {
	.release	= rdev_free,
	.sysfs_ops	= &rdev_sysfs_ops,
	.default_attrs	= rdev_default_attrs,
};

/*
 * Import a device. If 'super_format' >= 0, then sanity check the superblock
 *
 * mark the device faulty if:
 *
 *   - the device is nonexistent (zero size)
 *   - the device has no valid superblock
 *
 * a faulty rdev _never_ has rdev->sb set.
 */
static mdk_rdev_t *md_import_device(dev_t newdev, int super_format, int super_minor)
{
	char b[BDEVNAME_SIZE];
	int err;
	mdk_rdev_t *rdev;
	sector_t size;

	rdev = kzalloc(sizeof(*rdev), GFP_KERNEL);
	if (!rdev) {
		printk(KERN_ERR "md: could not alloc mem for new device!\n");
		return ERR_PTR(-ENOMEM);
	}

	if ((err = alloc_disk_sb(rdev)))
		goto abort_free;

	err = lock_rdev(rdev, newdev, super_format == -2);
	if (err)
		goto abort_free;

	kobject_init(&rdev->kobj, &rdev_ktype);

	rdev->desc_nr = -1;
	rdev->saved_raid_disk = -1;
	rdev->raid_disk = -1;
	rdev->flags = 0;
	rdev->data_offset = 0;
	rdev->sb_events = 0;
	atomic_set(&rdev->nr_pending, 0);
	atomic_set(&rdev->read_errors, 0);
	atomic_set(&rdev->corrected_errors, 0);

	size = rdev->bdev->bd_inode->i_size >> BLOCK_SIZE_BITS;
	if (!size) {
		printk(KERN_WARNING 
			"md: %s has zero or unknown size, marking faulty!\n",
			bdevname(rdev->bdev,b));
		err = -EINVAL;
		goto abort_free;
	}

	if (super_format >= 0) {
		err = super_types[super_format].
			load_super(rdev, NULL, super_minor);
		if (err == -EINVAL) {
			printk(KERN_WARNING
				"md: %s does not have a valid v%d.%d "
			       "superblock, not importing!\n",
				bdevname(rdev->bdev,b),
			       super_format, super_minor);
			goto abort_free;
		}
		if (err < 0) {
			printk(KERN_WARNING 
				"md: could not read %s's sb, not importing!\n",
				bdevname(rdev->bdev,b));
			goto abort_free;
		}
	}

	INIT_LIST_HEAD(&rdev->same_set);
	init_waitqueue_head(&rdev->blocked_wait);

	return rdev;

abort_free:
	if (rdev->sb_page) {
		if (rdev->bdev)
			unlock_rdev(rdev);
		free_disk_sb(rdev);
	}
	kfree(rdev);
	return ERR_PTR(err);
}

/*
 * Check a full RAID array for plausibility
 */


static void analyze_sbs(mddev_t * mddev)
{
	int i;
	mdk_rdev_t *rdev, *freshest, *tmp;
	char b[BDEVNAME_SIZE];

	freshest = NULL;
	rdev_for_each(rdev, tmp, mddev)
		switch (super_types[mddev->major_version].
			load_super(rdev, freshest, mddev->minor_version)) {
		case 1:
			freshest = rdev;
			break;
		case 0:
			break;
		default:
			printk( KERN_ERR \
				"md: fatal superblock inconsistency in %s"
				" -- removing from array\n", 
				bdevname(rdev->bdev,b));
			kick_rdev_from_array(rdev);
		}


	super_types[mddev->major_version].
		validate_super(mddev, freshest);

	i = 0;
	rdev_for_each(rdev, tmp, mddev) {
		if (rdev->desc_nr >= mddev->max_disks ||
		    i > mddev->max_disks) {
			printk(KERN_WARNING
			       "md: %s: %s: only %d devices permitted\n",
			       mdname(mddev), bdevname(rdev->bdev, b),
			       mddev->max_disks);
			kick_rdev_from_array(rdev);
			continue;
		}
		if (rdev != freshest)
			if (super_types[mddev->major_version].
			    validate_super(mddev, rdev)) {
				printk(KERN_WARNING "md: kicking non-fresh %s"
					" from array!\n",
					bdevname(rdev->bdev,b));
				kick_rdev_from_array(rdev);
				continue;
			}
		if (mddev->level == LEVEL_MULTIPATH) {
			rdev->desc_nr = i++;
			rdev->raid_disk = rdev->desc_nr;
			set_bit(In_sync, &rdev->flags);
		} else if (rdev->raid_disk >= (mddev->raid_disks - min(0, mddev->delta_disks))) {
			rdev->raid_disk = -1;
			clear_bit(In_sync, &rdev->flags);
		}
	}
}

static void md_safemode_timeout(unsigned long data);

static ssize_t
safe_delay_show(mddev_t *mddev, char *page)
{
	int msec = (mddev->safemode_delay*1000)/HZ;
	return sprintf(page, "%d.%03d\n", msec/1000, msec%1000);
}
static ssize_t
safe_delay_store(mddev_t *mddev, const char *cbuf, size_t len)
{
	int scale=1;
	int dot=0;
	int i;
	unsigned long msec;
	char buf[30];

	/* remove a period, and count digits after it */
	if (len >= sizeof(buf))
		return -EINVAL;
	strlcpy(buf, cbuf, sizeof(buf));
	for (i=0; i<len; i++) {
		if (dot) {
			if (isdigit(buf[i])) {
				buf[i-1] = buf[i];
				scale *= 10;
			}
			buf[i] = 0;
		} else if (buf[i] == '.') {
			dot=1;
			buf[i] = 0;
		}
	}
	if (strict_strtoul(buf, 10, &msec) < 0)
		return -EINVAL;
	msec = (msec * 1000) / scale;
	if (msec == 0)
		mddev->safemode_delay = 0;
	else {
		unsigned long old_delay = mddev->safemode_delay;
		mddev->safemode_delay = (msec*HZ)/1000;
		if (mddev->safemode_delay == 0)
			mddev->safemode_delay = 1;
		if (mddev->safemode_delay < old_delay)
			md_safemode_timeout((unsigned long)mddev);
	}
	return len;
}
static struct md_sysfs_entry md_safe_delay =
__ATTR(safe_mode_delay, S_IRUGO|S_IWUSR,safe_delay_show, safe_delay_store);

static ssize_t
level_show(mddev_t *mddev, char *page)
{
	struct mdk_personality *p = mddev->pers;
	if (p)
		return sprintf(page, "%s\n", p->name);
	else if (mddev->clevel[0])
		return sprintf(page, "%s\n", mddev->clevel);
	else if (mddev->level != LEVEL_NONE)
		return sprintf(page, "%d\n", mddev->level);
	else
		return 0;
}

static ssize_t
level_store(mddev_t *mddev, const char *buf, size_t len)
{
	char level[16];
	ssize_t rv = len;
	struct mdk_personality *pers;
	void *priv;
	mdk_rdev_t *rdev;

	if (mddev->pers == NULL) {
		if (len == 0)
			return 0;
		if (len >= sizeof(mddev->clevel))
			return -ENOSPC;
		strncpy(mddev->clevel, buf, len);
		if (mddev->clevel[len-1] == '\n')
			len--;
		mddev->clevel[len] = 0;
		mddev->level = LEVEL_NONE;
		return rv;
	}

	/* request to change the personality.  Need to ensure:
	 *  - array is not engaged in resync/recovery/reshape
	 *  - old personality can be suspended
	 *  - new personality will access other array.
	 */

	if (mddev->sync_thread || mddev->reshape_position != MaxSector)
		return -EBUSY;

	if (!mddev->pers->quiesce) {
		printk(KERN_WARNING "md: %s: %s does not support online personality change\n",
		       mdname(mddev), mddev->pers->name);
		return -EINVAL;
	}

	/* Now find the new personality */
	if (len == 0 || len >= sizeof(level))
		return -EINVAL;
	strncpy(level, buf, len);
	if (level[len-1] == '\n')
		len--;
	level[len] = 0;

	request_module("md-%s", level);
	spin_lock(&pers_lock);
	pers = find_pers(LEVEL_NONE, level);
	if (!pers || !try_module_get(pers->owner)) {
		spin_unlock(&pers_lock);
		printk(KERN_WARNING "md: personality %s not loaded\n", level);
		return -EINVAL;
	}
	spin_unlock(&pers_lock);

	if (pers == mddev->pers) {
		/* Nothing to do! */
		module_put(pers->owner);
		return rv;
	}
	if (!pers->takeover) {
		module_put(pers->owner);
		printk(KERN_WARNING "md: %s: %s does not support personality takeover\n",
		       mdname(mddev), level);
		return -EINVAL;
	}

	/* ->takeover must set new_* and/or delta_disks
	 * if it succeeds, and may set them when it fails.
	 */
	priv = pers->takeover(mddev);
	if (IS_ERR(priv)) {
		mddev->new_level = mddev->level;
		mddev->new_layout = mddev->layout;
		mddev->new_chunk_sectors = mddev->chunk_sectors;
		mddev->raid_disks -= mddev->delta_disks;
		mddev->delta_disks = 0;
		module_put(pers->owner);
		printk(KERN_WARNING "md: %s: %s would not accept array\n",
		       mdname(mddev), level);
		return PTR_ERR(priv);
	}

	/* Looks like we have a winner */
	mddev_suspend(mddev);
	mddev->pers->stop(mddev);
	module_put(mddev->pers->owner);
	/* Invalidate devices that are now superfluous */
	list_for_each_entry(rdev, &mddev->disks, same_set)
		if (rdev->raid_disk >= mddev->raid_disks) {
			rdev->raid_disk = -1;
			clear_bit(In_sync, &rdev->flags);
		}
	mddev->pers = pers;
	mddev->private = priv;
	strlcpy(mddev->clevel, pers->name, sizeof(mddev->clevel));
	mddev->level = mddev->new_level;
	mddev->layout = mddev->new_layout;
	mddev->chunk_sectors = mddev->new_chunk_sectors;
	mddev->delta_disks = 0;
	pers->run(mddev);
	mddev_resume(mddev);
	set_bit(MD_CHANGE_DEVS, &mddev->flags);
	set_bit(MD_RECOVERY_NEEDED, &mddev->recovery);
	md_wakeup_thread(mddev->thread);
	return rv;
}

static struct md_sysfs_entry md_level =
__ATTR(level, S_IRUGO|S_IWUSR, level_show, level_store);


static ssize_t
layout_show(mddev_t *mddev, char *page)
{
	/* just a number, not meaningful for all levels */
	if (mddev->reshape_position != MaxSector &&
	    mddev->layout != mddev->new_layout)
		return sprintf(page, "%d (%d)\n",
			       mddev->new_layout, mddev->layout);
	return sprintf(page, "%d\n", mddev->layout);
}

static ssize_t
layout_store(mddev_t *mddev, const char *buf, size_t len)
{
	char *e;
	unsigned long n = simple_strtoul(buf, &e, 10);

	if (!*buf || (*e && *e != '\n'))
		return -EINVAL;

	if (mddev->pers) {
		int err;
		if (mddev->pers->check_reshape == NULL)
			return -EBUSY;
		mddev->new_layout = n;
		err = mddev->pers->check_reshape(mddev);
		if (err) {
			mddev->new_layout = mddev->layout;
			return err;
		}
	} else {
		mddev->new_layout = n;
		if (mddev->reshape_position == MaxSector)
			mddev->layout = n;
	}
	return len;
}
static struct md_sysfs_entry md_layout =
__ATTR(layout, S_IRUGO|S_IWUSR, layout_show, layout_store);


static ssize_t
raid_disks_show(mddev_t *mddev, char *page)
{
	if (mddev->raid_disks == 0)
		return 0;
	if (mddev->reshape_position != MaxSector &&
	    mddev->delta_disks != 0)
		return sprintf(page, "%d (%d)\n", mddev->raid_disks,
			       mddev->raid_disks - mddev->delta_disks);
	return sprintf(page, "%d\n", mddev->raid_disks);
}

static int update_raid_disks(mddev_t *mddev, int raid_disks);

static ssize_t
raid_disks_store(mddev_t *mddev, const char *buf, size_t len)
{
	char *e;
	int rv = 0;
	unsigned long n = simple_strtoul(buf, &e, 10);

	if (!*buf || (*e && *e != '\n'))
		return -EINVAL;

	if (mddev->pers)
		rv = update_raid_disks(mddev, n);
	else if (mddev->reshape_position != MaxSector) {
		int olddisks = mddev->raid_disks - mddev->delta_disks;
		mddev->delta_disks = n - olddisks;
		mddev->raid_disks = n;
	} else
		mddev->raid_disks = n;
	return rv ? rv : len;
}
static struct md_sysfs_entry md_raid_disks =
__ATTR(raid_disks, S_IRUGO|S_IWUSR, raid_disks_show, raid_disks_store);

static ssize_t
chunk_size_show(mddev_t *mddev, char *page)
{
	if (mddev->reshape_position != MaxSector &&
	    mddev->chunk_sectors != mddev->new_chunk_sectors)
		return sprintf(page, "%d (%d)\n",
			       mddev->new_chunk_sectors << 9,
			       mddev->chunk_sectors << 9);
	return sprintf(page, "%d\n", mddev->chunk_sectors << 9);
}

static ssize_t
chunk_size_store(mddev_t *mddev, const char *buf, size_t len)
{
	char *e;
	unsigned long n = simple_strtoul(buf, &e, 10);

	if (!*buf || (*e && *e != '\n'))
		return -EINVAL;

	if (mddev->pers) {
		int err;
		if (mddev->pers->check_reshape == NULL)
			return -EBUSY;
		mddev->new_chunk_sectors = n >> 9;
		err = mddev->pers->check_reshape(mddev);
		if (err) {
			mddev->new_chunk_sectors = mddev->chunk_sectors;
			return err;
		}
	} else {
		mddev->new_chunk_sectors = n >> 9;
		if (mddev->reshape_position == MaxSector)
			mddev->chunk_sectors = n >> 9;
	}
	return len;
}
static struct md_sysfs_entry md_chunk_size =
__ATTR(chunk_size, S_IRUGO|S_IWUSR, chunk_size_show, chunk_size_store);

static ssize_t
resync_start_show(mddev_t *mddev, char *page)
{
	if (mddev->recovery_cp == MaxSector)
		return sprintf(page, "none\n");
	return sprintf(page, "%llu\n", (unsigned long long)mddev->recovery_cp);
}

static ssize_t
resync_start_store(mddev_t *mddev, const char *buf, size_t len)
{
	char *e;
	unsigned long long n = simple_strtoull(buf, &e, 10);

	if (mddev->pers)
		return -EBUSY;
	if (!*buf || (*e && *e != '\n'))
		return -EINVAL;

	mddev->recovery_cp = n;
	return len;
}
static struct md_sysfs_entry md_resync_start =
__ATTR(resync_start, S_IRUGO|S_IWUSR, resync_start_show, resync_start_store);

/*
 * The array state can be:
 *
 * clear
 *     No devices, no size, no level
 *     Equivalent to STOP_ARRAY ioctl
 * inactive
 *     May have some settings, but array is not active
 *        all IO results in error
 *     When written, doesn't tear down array, but just stops it
 * suspended (not supported yet)
 *     All IO requests will block. The array can be reconfigured.
 *     Writing this, if accepted, will block until array is quiescent
 * readonly
 *     no resync can happen.  no superblocks get written.
 *     write requests fail
 * read-auto
 *     like readonly, but behaves like 'clean' on a write request.
 *
 * clean - no pending writes, but otherwise active.
 *     When written to inactive array, starts without resync
 *     If a write request arrives then
 *       if metadata is known, mark 'dirty' and switch to 'active'.
 *       if not known, block and switch to write-pending
 *     If written to an active array that has pending writes, then fails.
 * active
 *     fully active: IO and resync can be happening.
 *     When written to inactive array, starts with resync
 *
 * write-pending
 *     clean, but writes are blocked waiting for 'active' to be written.
 *
 * active-idle
 *     like active, but no writes have been seen for a while (100msec).
 *
 */
enum array_state { clear, inactive, suspended, readonly, read_auto, clean, active,
		   write_pending, active_idle, bad_word};
static char *array_states[] = {
	"clear", "inactive", "suspended", "readonly", "read-auto", "clean", "active",
	"write-pending", "active-idle", NULL };

static int match_word(const char *word, char **list)
{
	int n;
	for (n=0; list[n]; n++)
		if (cmd_match(word, list[n]))
			break;
	return n;
}

static ssize_t
array_state_show(mddev_t *mddev, char *page)
{
	enum array_state st = inactive;

	if (mddev->pers)
		switch(mddev->ro) {
		case 1:
			st = readonly;
			break;
		case 2:
			st = read_auto;
			break;
		case 0:
			if (mddev->in_sync)
				st = clean;
			else if (test_bit(MD_CHANGE_CLEAN, &mddev->flags))
				st = write_pending;
			else if (mddev->safemode)
				st = active_idle;
			else
				st = active;
		}
	else {
		if (list_empty(&mddev->disks) &&
		    mddev->raid_disks == 0 &&
		    mddev->dev_sectors == 0)
			st = clear;
		else
			st = inactive;
	}
	return sprintf(page, "%s\n", array_states[st]);
}

static int do_md_stop(mddev_t * mddev, int ro, int is_open);
static int do_md_run(mddev_t * mddev);
static int restart_array(mddev_t *mddev);

static ssize_t
array_state_store(mddev_t *mddev, const char *buf, size_t len)
{
	int err = -EINVAL;
	enum array_state st = match_word(buf, array_states);
	switch(st) {
	case bad_word:
		break;
	case clear:
		/* stopping an active array */
		if (atomic_read(&mddev->openers) > 0)
			return -EBUSY;
		err = do_md_stop(mddev, 0, 0);
		break;
	case inactive:
		/* stopping an active array */
		if (mddev->pers) {
			if (atomic_read(&mddev->openers) > 0)
				return -EBUSY;
			err = do_md_stop(mddev, 2, 0);
		} else
			err = 0; /* already inactive */
		break;
	case suspended:
		break; /* not supported yet */
	case readonly:
		if (mddev->pers)
			err = do_md_stop(mddev, 1, 0);
		else {
			mddev->ro = 1;
			set_disk_ro(mddev->gendisk, 1);
			err = do_md_run(mddev);
		}
		break;
	case read_auto:
		if (mddev->pers) {
			if (mddev->ro == 0)
				err = do_md_stop(mddev, 1, 0);
			else if (mddev->ro == 1)
				err = restart_array(mddev);
			if (err == 0) {
				mddev->ro = 2;
				set_disk_ro(mddev->gendisk, 0);
			}
		} else {
			mddev->ro = 2;
			err = do_md_run(mddev);
		}
		break;
	case clean:
		if (mddev->pers) {
			restart_array(mddev);
			spin_lock_irq(&mddev->write_lock);
			if (atomic_read(&mddev->writes_pending) == 0) {
				if (mddev->in_sync == 0) {
					mddev->in_sync = 1;
					if (mddev->safemode == 1)
						mddev->safemode = 0;
					if (mddev->persistent)
						set_bit(MD_CHANGE_CLEAN,
							&mddev->flags);
				}
				err = 0;
			} else
				err = -EBUSY;
			spin_unlock_irq(&mddev->write_lock);
		} else
			err = -EINVAL;
		break;
	case active:
		if (mddev->pers) {
			restart_array(mddev);
			if (mddev->external)
				clear_bit(MD_CHANGE_CLEAN, &mddev->flags);
			wake_up(&mddev->sb_wait);
			err = 0;
		} else {
			mddev->ro = 0;
			set_disk_ro(mddev->gendisk, 0);
			err = do_md_run(mddev);
		}
		break;
	case write_pending:
	case active_idle:
		/* these cannot be set */
		break;
	}
	if (err)
		return err;
	else {
		sysfs_notify_dirent(mddev->sysfs_state);
		return len;
	}
}
static struct md_sysfs_entry md_array_state =
__ATTR(array_state, S_IRUGO|S_IWUSR, array_state_show, array_state_store);

static ssize_t
null_show(mddev_t *mddev, char *page)
{
	return -EINVAL;
}

static ssize_t
new_dev_store(mddev_t *mddev, const char *buf, size_t len)
{
	/* buf must be %d:%d\n? giving major and minor numbers */
	/* The new device is added to the array.
	 * If the array has a persistent superblock, we read the
	 * superblock to initialise info and check validity.
	 * Otherwise, only checking done is that in bind_rdev_to_array,
	 * which mainly checks size.
	 */
	char *e;
	int major = simple_strtoul(buf, &e, 10);
	int minor;
	dev_t dev;
	mdk_rdev_t *rdev;
	int err;

	if (!*buf || *e != ':' || !e[1] || e[1] == '\n')
		return -EINVAL;
	minor = simple_strtoul(e+1, &e, 10);
	if (*e && *e != '\n')
		return -EINVAL;
	dev = MKDEV(major, minor);
	if (major != MAJOR(dev) ||
	    minor != MINOR(dev))
		return -EOVERFLOW;


	if (mddev->persistent) {
		rdev = md_import_device(dev, mddev->major_version,
					mddev->minor_version);
		if (!IS_ERR(rdev) && !list_empty(&mddev->disks)) {
			mdk_rdev_t *rdev0 = list_entry(mddev->disks.next,
						       mdk_rdev_t, same_set);
			err = super_types[mddev->major_version]
				.load_super(rdev, rdev0, mddev->minor_version);
			if (err < 0)
				goto out;
		}
	} else if (mddev->external)
		rdev = md_import_device(dev, -2, -1);
	else
		rdev = md_import_device(dev, -1, -1);

	if (IS_ERR(rdev))
		return PTR_ERR(rdev);
	err = bind_rdev_to_array(rdev, mddev);
 out:
	if (err)
		export_rdev(rdev);
	return err ? err : len;
}

static struct md_sysfs_entry md_new_device =
__ATTR(new_dev, S_IWUSR, null_show, new_dev_store);

static ssize_t
bitmap_store(mddev_t *mddev, const char *buf, size_t len)
{
	char *end;
	unsigned long chunk, end_chunk;

	if (!mddev->bitmap)
		goto out;
	/* buf should be <chunk> <chunk> ... or <chunk>-<chunk> ... (range) */
	while (*buf) {
		chunk = end_chunk = simple_strtoul(buf, &end, 0);
		if (buf == end) break;
		if (*end == '-') { /* range */
			buf = end + 1;
			end_chunk = simple_strtoul(buf, &end, 0);
			if (buf == end) break;
		}
		if (*end && !isspace(*end)) break;
		bitmap_dirty_bits(mddev->bitmap, chunk, end_chunk);
		buf = end;
		while (isspace(*buf)) buf++;
	}
	bitmap_unplug(mddev->bitmap); /* flush the bits to disk */
out:
	return len;
}

static struct md_sysfs_entry md_bitmap =
__ATTR(bitmap_set_bits, S_IWUSR, null_show, bitmap_store);

static ssize_t
size_show(mddev_t *mddev, char *page)
{
	return sprintf(page, "%llu\n",
		(unsigned long long)mddev->dev_sectors / 2);
}

static int update_size(mddev_t *mddev, sector_t num_sectors);

static ssize_t
size_store(mddev_t *mddev, const char *buf, size_t len)
{
	/* If array is inactive, we can reduce the component size, but
	 * not increase it (except from 0).
	 * If array is active, we can try an on-line resize
	 */
	sector_t sectors;
	int err = strict_blocks_to_sectors(buf, &sectors);

	if (err < 0)
		return err;
	if (mddev->pers) {
		err = update_size(mddev, sectors);
		md_update_sb(mddev, 1);
	} else {
		if (mddev->dev_sectors == 0 ||
		    mddev->dev_sectors > sectors)
			mddev->dev_sectors = sectors;
		else
			err = -ENOSPC;
	}
	return err ? err : len;
}

static struct md_sysfs_entry md_size =
__ATTR(component_size, S_IRUGO|S_IWUSR, size_show, size_store);


/* Metdata version.
 * This is one of
 *   'none' for arrays with no metadata (good luck...)
 *   'external' for arrays with externally managed metadata,
 * or N.M for internally known formats
 */
static ssize_t
metadata_show(mddev_t *mddev, char *page)
{
	if (mddev->persistent)
		return sprintf(page, "%d.%d\n",
			       mddev->major_version, mddev->minor_version);
	else if (mddev->external)
		return sprintf(page, "external:%s\n", mddev->metadata_type);
	else
		return sprintf(page, "none\n");
}

static ssize_t
metadata_store(mddev_t *mddev, const char *buf, size_t len)
{
	int major, minor;
	char *e;
	/* Changing the details of 'external' metadata is
	 * always permitted.  Otherwise there must be
	 * no devices attached to the array.
	 */
	if (mddev->external && strncmp(buf, "external:", 9) == 0)
		;
	else if (!list_empty(&mddev->disks))
		return -EBUSY;

	if (cmd_match(buf, "none")) {
		mddev->persistent = 0;
		mddev->external = 0;
		mddev->major_version = 0;
		mddev->minor_version = 90;
		return len;
	}
	if (strncmp(buf, "external:", 9) == 0) {
		size_t namelen = len-9;
		if (namelen >= sizeof(mddev->metadata_type))
			namelen = sizeof(mddev->metadata_type)-1;
		strncpy(mddev->metadata_type, buf+9, namelen);
		mddev->metadata_type[namelen] = 0;
		if (namelen && mddev->metadata_type[namelen-1] == '\n')
			mddev->metadata_type[--namelen] = 0;
		mddev->persistent = 0;
		mddev->external = 1;
		mddev->major_version = 0;
		mddev->minor_version = 90;
		return len;
	}
	major = simple_strtoul(buf, &e, 10);
	if (e==buf || *e != '.')
		return -EINVAL;
	buf = e+1;
	minor = simple_strtoul(buf, &e, 10);
	if (e==buf || (*e && *e != '\n') )
		return -EINVAL;
	if (major >= ARRAY_SIZE(super_types) || super_types[major].name == NULL)
		return -ENOENT;
	mddev->major_version = major;
	mddev->minor_version = minor;
	mddev->persistent = 1;
	mddev->external = 0;
	return len;
}

static struct md_sysfs_entry md_metadata =
__ATTR(metadata_version, S_IRUGO|S_IWUSR, metadata_show, metadata_store);

static ssize_t
action_show(mddev_t *mddev, char *page)
{
	char *type = "idle";
	if (test_bit(MD_RECOVERY_FROZEN, &mddev->recovery))
		type = "frozen";
	else if (test_bit(MD_RECOVERY_RUNNING, &mddev->recovery) ||
	    (!mddev->ro && test_bit(MD_RECOVERY_NEEDED, &mddev->recovery))) {
		if (test_bit(MD_RECOVERY_RESHAPE, &mddev->recovery))
			type = "reshape";
		else if (test_bit(MD_RECOVERY_SYNC, &mddev->recovery)) {
			if (!test_bit(MD_RECOVERY_REQUESTED, &mddev->recovery))
				type = "resync";
			else if (test_bit(MD_RECOVERY_CHECK, &mddev->recovery))
				type = "check";
			else
				type = "repair";
		} else if (test_bit(MD_RECOVERY_RECOVER, &mddev->recovery))
			type = "recover";
	}
	return sprintf(page, "%s\n", type);
}

static ssize_t
action_store(mddev_t *mddev, const char *page, size_t len)
{
	if (!mddev->pers || !mddev->pers->sync_request)
		return -EINVAL;

	if (cmd_match(page, "frozen"))
		set_bit(MD_RECOVERY_FROZEN, &mddev->recovery);
	else
		clear_bit(MD_RECOVERY_FROZEN, &mddev->recovery);

	if (cmd_match(page, "idle") || cmd_match(page, "frozen")) {
		if (mddev->sync_thread) {
			set_bit(MD_RECOVERY_INTR, &mddev->recovery);
			md_unregister_thread(mddev->sync_thread);
			mddev->sync_thread = NULL;
			mddev->recovery = 0;
		}
	} else if (test_bit(MD_RECOVERY_RUNNING, &mddev->recovery) ||
		   test_bit(MD_RECOVERY_NEEDED, &mddev->recovery))
		return -EBUSY;
	else if (cmd_match(page, "resync"))
		set_bit(MD_RECOVERY_NEEDED, &mddev->recovery);
	else if (cmd_match(page, "recover")) {
		set_bit(MD_RECOVERY_RECOVER, &mddev->recovery);
		set_bit(MD_RECOVERY_NEEDED, &mddev->recovery);
	} else if (cmd_match(page, "reshape")) {
		int err;
		if (mddev->pers->start_reshape == NULL)
			return -EINVAL;
		err = mddev->pers->start_reshape(mddev);
		if (err)
			return err;
		sysfs_notify(&mddev->kobj, NULL, "degraded");
	} else {
		if (cmd_match(page, "check"))
			set_bit(MD_RECOVERY_CHECK, &mddev->recovery);
		else if (!cmd_match(page, "repair"))
			return -EINVAL;
		set_bit(MD_RECOVERY_REQUESTED, &mddev->recovery);
		set_bit(MD_RECOVERY_SYNC, &mddev->recovery);
	}
	set_bit(MD_RECOVERY_NEEDED, &mddev->recovery);
	md_wakeup_thread(mddev->thread);
	sysfs_notify_dirent(mddev->sysfs_action);
	return len;
}

static ssize_t
mismatch_cnt_show(mddev_t *mddev, char *page)
{
	return sprintf(page, "%llu\n",
		       (unsigned long long) mddev->resync_mismatches);
}

static struct md_sysfs_entry md_scan_mode =
__ATTR(sync_action, S_IRUGO|S_IWUSR, action_show, action_store);


static struct md_sysfs_entry md_mismatches = __ATTR_RO(mismatch_cnt);

static ssize_t
sync_min_show(mddev_t *mddev, char *page)
{
	return sprintf(page, "%d (%s)\n", speed_min(mddev),
		       mddev->sync_speed_min ? "local": "system");
}

static ssize_t
sync_min_store(mddev_t *mddev, const char *buf, size_t len)
{
	int min;
	char *e;
	if (strncmp(buf, "system", 6)==0) {
		mddev->sync_speed_min = 0;
		return len;
	}
	min = simple_strtoul(buf, &e, 10);
	if (buf == e || (*e && *e != '\n') || min <= 0)
		return -EINVAL;
	mddev->sync_speed_min = min;
	return len;
}

static struct md_sysfs_entry md_sync_min =
__ATTR(sync_speed_min, S_IRUGO|S_IWUSR, sync_min_show, sync_min_store);

static ssize_t
sync_max_show(mddev_t *mddev, char *page)
{
	return sprintf(page, "%d (%s)\n", speed_max(mddev),
		       mddev->sync_speed_max ? "local": "system");
}

static ssize_t
sync_max_store(mddev_t *mddev, const char *buf, size_t len)
{
	int max;
	char *e;
	if (strncmp(buf, "system", 6)==0) {
		mddev->sync_speed_max = 0;
		return len;
	}
	max = simple_strtoul(buf, &e, 10);
	if (buf == e || (*e && *e != '\n') || max <= 0)
		return -EINVAL;
	mddev->sync_speed_max = max;
	return len;
}

static struct md_sysfs_entry md_sync_max =
__ATTR(sync_speed_max, S_IRUGO|S_IWUSR, sync_max_show, sync_max_store);

static ssize_t
degraded_show(mddev_t *mddev, char *page)
{
	return sprintf(page, "%d\n", mddev->degraded);
}
static struct md_sysfs_entry md_degraded = __ATTR_RO(degraded);

static ssize_t
sync_force_parallel_show(mddev_t *mddev, char *page)
{
	return sprintf(page, "%d\n", mddev->parallel_resync);
}

static ssize_t
sync_force_parallel_store(mddev_t *mddev, const char *buf, size_t len)
{
	long n;

	if (strict_strtol(buf, 10, &n))
		return -EINVAL;

	if (n != 0 && n != 1)
		return -EINVAL;

	mddev->parallel_resync = n;

	if (mddev->sync_thread)
		wake_up(&resync_wait);

	return len;
}

/* force parallel resync, even with shared block devices */
static struct md_sysfs_entry md_sync_force_parallel =
__ATTR(sync_force_parallel, S_IRUGO|S_IWUSR,
       sync_force_parallel_show, sync_force_parallel_store);

static ssize_t
sync_speed_show(mddev_t *mddev, char *page)
{
	unsigned long resync, dt, db;
	if (mddev->curr_resync == 0)
		return sprintf(page, "none\n");
	resync = mddev->curr_mark_cnt - atomic_read(&mddev->recovery_active);
	dt = (jiffies - mddev->resync_mark) / HZ;
	if (!dt) dt++;
	db = resync - mddev->resync_mark_cnt;
	return sprintf(page, "%lu\n", db/dt/2); /* K/sec */
}

static struct md_sysfs_entry md_sync_speed = __ATTR_RO(sync_speed);

static ssize_t
sync_completed_show(mddev_t *mddev, char *page)
{
	unsigned long max_sectors, resync;

	if (!test_bit(MD_RECOVERY_RUNNING, &mddev->recovery))
		return sprintf(page, "none\n");

	if (test_bit(MD_RECOVERY_SYNC, &mddev->recovery))
		max_sectors = mddev->resync_max_sectors;
	else
		max_sectors = mddev->dev_sectors;

	resync = mddev->curr_resync_completed;
	return sprintf(page, "%lu / %lu\n", resync, max_sectors);
}

static struct md_sysfs_entry md_sync_completed = __ATTR_RO(sync_completed);

static ssize_t
min_sync_show(mddev_t *mddev, char *page)
{
	return sprintf(page, "%llu\n",
		       (unsigned long long)mddev->resync_min);
}
static ssize_t
min_sync_store(mddev_t *mddev, const char *buf, size_t len)
{
	unsigned long long min;
	if (strict_strtoull(buf, 10, &min))
		return -EINVAL;
	if (min > mddev->resync_max)
		return -EINVAL;
	if (test_bit(MD_RECOVERY_RUNNING, &mddev->recovery))
		return -EBUSY;

	/* Must be a multiple of chunk_size */
	if (mddev->chunk_sectors) {
		sector_t temp = min;
		if (sector_div(temp, mddev->chunk_sectors))
			return -EINVAL;
	}
	mddev->resync_min = min;

	return len;
}

static struct md_sysfs_entry md_min_sync =
__ATTR(sync_min, S_IRUGO|S_IWUSR, min_sync_show, min_sync_store);

static ssize_t
max_sync_show(mddev_t *mddev, char *page)
{
	if (mddev->resync_max == MaxSector)
		return sprintf(page, "max\n");
	else
		return sprintf(page, "%llu\n",
			       (unsigned long long)mddev->resync_max);
}
static ssize_t
max_sync_store(mddev_t *mddev, const char *buf, size_t len)
{
	if (strncmp(buf, "max", 3) == 0)
		mddev->resync_max = MaxSector;
	else {
		unsigned long long max;
		if (strict_strtoull(buf, 10, &max))
			return -EINVAL;
		if (max < mddev->resync_min)
			return -EINVAL;
		if (max < mddev->resync_max &&
		    mddev->ro == 0 &&
		    test_bit(MD_RECOVERY_RUNNING, &mddev->recovery))
			return -EBUSY;

		/* Must be a multiple of chunk_size */
		if (mddev->chunk_sectors) {
			sector_t temp = max;
			if (sector_div(temp, mddev->chunk_sectors))
				return -EINVAL;
		}
		mddev->resync_max = max;
	}
	wake_up(&mddev->recovery_wait);
	return len;
}

static struct md_sysfs_entry md_max_sync =
__ATTR(sync_max, S_IRUGO|S_IWUSR, max_sync_show, max_sync_store);

static ssize_t
suspend_lo_show(mddev_t *mddev, char *page)
{
	return sprintf(page, "%llu\n", (unsigned long long)mddev->suspend_lo);
}

static ssize_t
suspend_lo_store(mddev_t *mddev, const char *buf, size_t len)
{
	char *e;
	unsigned long long new = simple_strtoull(buf, &e, 10);

	if (mddev->pers == NULL || 
	    mddev->pers->quiesce == NULL)
		return -EINVAL;
	if (buf == e || (*e && *e != '\n'))
		return -EINVAL;
	if (new >= mddev->suspend_hi ||
	    (new > mddev->suspend_lo && new < mddev->suspend_hi)) {
		mddev->suspend_lo = new;
		mddev->pers->quiesce(mddev, 2);
		return len;
	} else
		return -EINVAL;
}
static struct md_sysfs_entry md_suspend_lo =
__ATTR(suspend_lo, S_IRUGO|S_IWUSR, suspend_lo_show, suspend_lo_store);


static ssize_t
suspend_hi_show(mddev_t *mddev, char *page)
{
	return sprintf(page, "%llu\n", (unsigned long long)mddev->suspend_hi);
}

static ssize_t
suspend_hi_store(mddev_t *mddev, const char *buf, size_t len)
{
	char *e;
	unsigned long long new = simple_strtoull(buf, &e, 10);

	if (mddev->pers == NULL ||
	    mddev->pers->quiesce == NULL)
		return -EINVAL;
	if (buf == e || (*e && *e != '\n'))
		return -EINVAL;
	if ((new <= mddev->suspend_lo && mddev->suspend_lo >= mddev->suspend_hi) ||
	    (new > mddev->suspend_lo && new > mddev->suspend_hi)) {
		mddev->suspend_hi = new;
		mddev->pers->quiesce(mddev, 1);
		mddev->pers->quiesce(mddev, 0);
		return len;
	} else
		return -EINVAL;
}
static struct md_sysfs_entry md_suspend_hi =
__ATTR(suspend_hi, S_IRUGO|S_IWUSR, suspend_hi_show, suspend_hi_store);

static ssize_t
reshape_position_show(mddev_t *mddev, char *page)
{
	if (mddev->reshape_position != MaxSector)
		return sprintf(page, "%llu\n",
			       (unsigned long long)mddev->reshape_position);
	strcpy(page, "none\n");
	return 5;
}

static ssize_t
reshape_position_store(mddev_t *mddev, const char *buf, size_t len)
{
	char *e;
	unsigned long long new = simple_strtoull(buf, &e, 10);
	if (mddev->pers)
		return -EBUSY;
	if (buf == e || (*e && *e != '\n'))
		return -EINVAL;
	mddev->reshape_position = new;
	mddev->delta_disks = 0;
	mddev->new_level = mddev->level;
	mddev->new_layout = mddev->layout;
	mddev->new_chunk_sectors = mddev->chunk_sectors;
	return len;
}

static struct md_sysfs_entry md_reshape_position =
__ATTR(reshape_position, S_IRUGO|S_IWUSR, reshape_position_show,
       reshape_position_store);

static ssize_t
array_size_show(mddev_t *mddev, char *page)
{
	if (mddev->external_size)
		return sprintf(page, "%llu\n",
			       (unsigned long long)mddev->array_sectors/2);
	else
		return sprintf(page, "default\n");
}

static ssize_t
array_size_store(mddev_t *mddev, const char *buf, size_t len)
{
	sector_t sectors;

	if (strncmp(buf, "default", 7) == 0) {
		if (mddev->pers)
			sectors = mddev->pers->size(mddev, 0, 0);
		else
			sectors = mddev->array_sectors;

		mddev->external_size = 0;
	} else {
		if (strict_blocks_to_sectors(buf, &sectors) < 0)
			return -EINVAL;
		if (mddev->pers && mddev->pers->size(mddev, 0, 0) < sectors)
			return -E2BIG;

		mddev->external_size = 1;
	}

	mddev->array_sectors = sectors;
	set_capacity(mddev->gendisk, mddev->array_sectors);
	if (mddev->pers)
		revalidate_disk(mddev->gendisk);

	return len;
}

static struct md_sysfs_entry md_array_size =
__ATTR(array_size, S_IRUGO|S_IWUSR, array_size_show,
       array_size_store);

static struct attribute *md_default_attrs[] = {
	&md_level.attr,
	&md_layout.attr,
	&md_raid_disks.attr,
	&md_chunk_size.attr,
	&md_size.attr,
	&md_resync_start.attr,
	&md_metadata.attr,
	&md_new_device.attr,
	&md_safe_delay.attr,
	&md_array_state.attr,
	&md_reshape_position.attr,
	&md_array_size.attr,
	NULL,
};

static struct attribute *md_redundancy_attrs[] = {
	&md_scan_mode.attr,
	&md_mismatches.attr,
	&md_sync_min.attr,
	&md_sync_max.attr,
	&md_sync_speed.attr,
	&md_sync_force_parallel.attr,
	&md_sync_completed.attr,
	&md_min_sync.attr,
	&md_max_sync.attr,
	&md_suspend_lo.attr,
	&md_suspend_hi.attr,
	&md_bitmap.attr,
	&md_degraded.attr,
	NULL,
};
static struct attribute_group md_redundancy_group = {
	.name = NULL,
	.attrs = md_redundancy_attrs,
};


static ssize_t
md_attr_show(struct kobject *kobj, struct attribute *attr, char *page)
{
	struct md_sysfs_entry *entry = container_of(attr, struct md_sysfs_entry, attr);
	mddev_t *mddev = container_of(kobj, struct mddev_s, kobj);
	ssize_t rv;

	if (!entry->show)
		return -EIO;
	rv = mddev_lock(mddev);
	if (!rv) {
		rv = entry->show(mddev, page);
		mddev_unlock(mddev);
	}
	return rv;
}

static ssize_t
md_attr_store(struct kobject *kobj, struct attribute *attr,
	      const char *page, size_t length)
{
	struct md_sysfs_entry *entry = container_of(attr, struct md_sysfs_entry, attr);
	mddev_t *mddev = container_of(kobj, struct mddev_s, kobj);
	ssize_t rv;

	if (!entry->store)
		return -EIO;
	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;
	rv = mddev_lock(mddev);
	if (mddev->hold_active == UNTIL_IOCTL)
		mddev->hold_active = 0;
	if (!rv) {
		rv = entry->store(mddev, page, length);
		mddev_unlock(mddev);
	}
	return rv;
}

static void md_free(struct kobject *ko)
{
	mddev_t *mddev = container_of(ko, mddev_t, kobj);

	if (mddev->sysfs_state)
		sysfs_put(mddev->sysfs_state);

	if (mddev->gendisk) {
		del_gendisk(mddev->gendisk);
		put_disk(mddev->gendisk);
	}
	if (mddev->queue)
		blk_cleanup_queue(mddev->queue);

	kfree(mddev);
}

static struct sysfs_ops md_sysfs_ops = {
	.show	= md_attr_show,
	.store	= md_attr_store,
};
static struct kobj_type md_ktype = {
	.release	= md_free,
	.sysfs_ops	= &md_sysfs_ops,
	.default_attrs	= md_default_attrs,
};

int mdp_major = 0;

static void mddev_delayed_delete(struct work_struct *ws)
{
	mddev_t *mddev = container_of(ws, mddev_t, del_work);

	if (mddev->private == &md_redundancy_group) {
		sysfs_remove_group(&mddev->kobj, &md_redundancy_group);
		if (mddev->sysfs_action)
			sysfs_put(mddev->sysfs_action);
		mddev->sys/*
   md. = NULL : Multipleprivatever for Li}
	kobject_del(&ultiple999,c : 999, 200putngo Molnar

    }

static int md_alloc(dev_t dev, char *name)
{
	 the MDDEFINE_MUTEX(disks_mutexc : ultip_t *ultip =  by Mifind froc : struct genID-5 *ID-5;
	 driparti md.ed6 extenshift6 extenunin <hpa@zerror;

	if (!ultip)
		return -ENODEVoot sions by H. = (MAJOR(ultipleytor) != MD_oyer , Ingnvin =nsions by H. ? MdpMinorSd sup: 0;
	ytor = MINr <HarryH@Royal.>> Anvin <
	/* wait for any previous instance if thisMarcice
	 * to be completed removed <Harry00 Iayed00 Iete).arka/
	flush_scheduled_work()s Duxtens_lock(&ID-5 extensions  - b = -EEXIST6 exfsupport->ar
   -r ligoto aborus Du5 an

    {
	urgiNeedl@inensure that '

  ' is not a duplicate.
	<rgoocs by Miguel de 2 : Mspinfixes aall_ultipsfixes- lot	list_for_each_entry<Harry2,  code
     , code
     r liD5 and gen2eric
     &&c.

    strcmpThis program is ->ID-5_

  , quest == 0t basestent bunitmap code
     CopyrighnderD code (such		8, 1r the terms of the GNU General Pu}
ts to the RANOMEMons by M->queuht (blkr code_(at y(GFP_KERNELl Pusupport for (at y RAID code (suchn 2, or (at yr (at ydataIcaza, Gs DurgiCannameterms ed becausion)e (at yoil Bew: no concurrencyrgooc(at y_flag_set terms ed(QUEUE_FLAG_CLUSTER,aza, Ghould has Du optFree
 make_request<HarryH@(at ys AvUSA.
*/

#incge, M is f=chnooc_ID-5(1 << Anvinion.

         base optcleanup   any Ave, Cambridge, Multiple(at your for Linreceived a co}linux/->maj the oyer <HarryH@Royal;ev */
#ifirst_mas < =zytorude <linuon.

  

   C	ngo cpyAID-5te it and/or modifnts lsnthon(sions by H.incluprintfinux/proc_fs.h>
#i"md_d%d",zytorde <linuude <linux/reboot.h>
#include <nux/file.h>
#e <linuops = &md_inclmd_p.h>
#opyrigh_al Public Lice_p.h>
#(at yourx/buffer_hea;DurgiAllow extendednsions by s.  T LisSA.
s/linarkal'mdp'sher <m redundant, but we can't reallyarkal Devfs it nowh <rgooce <linulags |= GENHD_FL_EXT_DEVRAIDadd>
#inc      copy of tar
   - = AID-6 e to the 999, 200init_and_addngo Molnar

 ,e <liktype,Publiftwa and i_to_devE_SPINDECLARE_"%s"ude <"ion.

    - bt based rid)(Disd aupossible
statiasrint_devices(void);

YINGmarked.unsw __must_check,ic vCurr do something with/linuresult.unsw.edu<linuk(r ve_WARNINGde <:voidBrowregister %s/md - modi in use\n"E_HE(resy taticte it and/o<linu to the alk.}
de (su:ots of fterms ofID-5 extensionssuppo: bug in fi999, 200ueven rewritten, ba, KOBJ_ADDc : Multiple Devic theht ( Devicget_dirill o Molnar

 .sd, "arraythe IOrintk}ns by Mily rrt for;
inear an  - boo on the MDgo Moln999, 20uel _probe from Marc Z dri*sion, void *al P Chanver code fro,h> /*on
 * speed for L on the MD driic D

  d_an 'a(constZyngierval, reconstrernel_param *kp Chan/* val,4,5 pbede <l*" where *eil Brownll digitsh <rg Wekdev.h IO
an an 'anstruca large free pe.h>
number, andarkalset/linu loadtod_{m. d_{min,maxctl_spreadyx}
 ic ictivnt speh <rgooc drileiverstrlen(valon
 yngiebuf[DISK_NAME_LEN]s Duwhile (ddev&&d_{m[len-1]y
  '\n'inclnt s-Note thddev>= _speed_limit_r linear and 2BIG Ingo l <libuf,d_{m,mdde+1 Note thstrn youpeed_e <l", 3l.Net0r linear and INVAr Linear anver code 0
stafsed on the MD* idlmd_safemode_timeout(unsigris long  IO detectiy Miguel de Icaupport t *)_RAIDoot suppoatomic_	retngo Molnawrites_pr
  ng)includultiple le[] =  = 1linu5 and generine rnalnc.

 Devicnotifym is idle. Therh if the IO maximum'_wakeup_thdata	ultipleame	= sed on the MD dristarem isty_degradH. Pn the MD drido_md_runrocname	= .h"

# Chanm>
   - copyk_ry Migueizeo Ingo Molnar
   - RAID-6 ego Moln	= spersonality *andloot suppC) 20emptyngo MolnaID-5 )incl/*/sec, so untic int sysctl_nosher <ms..sw.edul_table_header *ID5 and generandlr linear and BUSYs DurgarkalAnalyz 1000 RAID superbixes raidmin ?

   You shorai DEFINsincludd_table,
	},andle exntnc.

l_table_header *r	a		= S__sbstruction
 dati5 and generlevel.NetLEVEL_NONEr line
#inc_moro.a("md-ame	=-nux/f	.procname	=de <linux/ranultiplecame	=[0]len		= 0,
		.mode		= 05...) 	{ .ctl_name ge, M
		.modDrop_speecontainer


#ifndbuffers, fromart) onarkal@he onlyd_{midfine IRUGem derfaAnth && rouguctionmtic inher <mh <rgoocC) 2003-2004, Paul CizeoE_WAIme = 0 }
}, sameftwaame = 0 }
t,
		bit(Faulty, &izeoatic LI;

st	tatiine DEB	sync_		.ch

#d when bman, In	inide '#incare:Events are:
 *
statiperformallelstatctl_tacy elecs ona sys 'interesmin = 1dod auwanne intal Puto overlaped
 * etaal P show* I' thcounBitmap issues have been handled linutic ie.unsw.edud_ta when al P_offline<  when sbthe rpoll' oEV,
		.procnort sectorsfree softwa*mddev)
{
	atomic_+block_de_event_waite softwa>nc(&md_event_count);
	peed lim0 KB/%s:ECLAREIT_QUEUsAD(md_evenot shohow up thmd

  tructiol Publiid_root_table[] = ublis}id mdnt);
	wakec(&md_event_co +nc(&md_evenize/512rnate version th)
{
	atomice called from interruptsD(md_evewhen calliysfs_notify isn't needed.
 */
static void md_new_event_inintr(mdncreaSR,
		.proc_handlec(&md_ec_dointvec,
	},
ectios downHarryH@Royayou cayou can chk);

stK(pers_lock);
/sysctl.h>
#i linear and stersio
tent bitmap andleneral Puandl = di Ohandl		.procname	= block_device_oper 0 }
}whene|| !try	.mode	yste(andl->owner_min,
	 the terms of_mddevs_lock EV,
		.procname	= "dev",
		.maxlen	peed limit'
 * is 1000 KB/andler	= &pr- RAame	= %eviceBrowlolimi!_notify sn't need	= raid_dir_ta#includeent mddev and must mddev_put it.
 */
#define for_ lin_mddev(mddev,tmp)					\
									\o always hid md_new_event_inimum' reatic cppor_doinh breaks out of this loop wV,
		.procname	= "de
 * Anice_opin,
		.maxleame	= unlock(&all_mlinux/buffenew_			\
		if (mddev) mdde}ev->sync_s		\
	     ({ ,nlock(&nd/or nt);of mddev_t, all_aticl_mddevs));\
reshape_poss by .Net>axSt_waifree p th
 * Anocname spin_ly
  u canin file %s, landler	= &prsec, sount;
vn 'ghaping.
		.ctl_			\
			mddev_ for Linu unlocly r
 * Any codef (tmp != &all_mddevs)	l_mddemp = tmases#include based rWarnthony Lisis a po_tabistar sitart_waitconfigura md.e.unsw.edu : sys[BDEVd_limSIZE], b2 to ->make_requlinux/ sizeof(int),
rsist driwararald 0ght (C) 2003-2004, Paul Cof /proc/mdstat
 * can use 'po_locinked into some internalts, c/mdstat
 * can use 'poll' odev_t *mdd_inc(&m2free so}
EXPORT_SYare:truc_tatic is ==ify isn'tint mtruct request_queuey more.
peed limit'
 * is 10 = q->w up th"upts* is 10rrup appearsl@inam"
	if (mddev ==art builn us physicaeed_sk as {
		bio_io_erro%s._notify iisn't needed.
 */
stav->suspended) are:ed.
 st(struct ,bAIT(__wait);
		for (;;) {
		*bio)
{,b2atic voi	 bio has (int)c LISTn che_mdde bio hcurrent mddev and must m)					\
			"True protes drivagueuet single-}
	r {
		o_io_errofailatiomighax}
 .comromisedmddeLOCK(all_m   ({ scIT_Qyf you w/* maurn mIT_Q-ridden bythan calling gooc     ({ spasesmaxent_waitell used m_event_wait lotsmic_inbarrimdde

  UPTIBLEt(&mddeokent_copeed_limiv->syname	= "speed_limit_mspeed_l->nextadtem isibomic_inco = 2; /rrayad-tem 
statiswitchart ux/ct tl_spev->void ;
	if (ato
 * Ansysctl_spintk("md: bisibeed limit'
 ERRddev_put i1;
	sy)sh_waed ..ait);
	}ble,
	},
	{ .ctlmp = tmiz_lock);, 0,  it<							\an 'abst_wait being* is_ONCE  You shovent couunt);G(x..: deffind	tmp  too small,);
		}
"stati'ddev->thread '_mddein effect?not  __func__e firit_event(mddev-show up th0 KB/start a int sunt); %llu >;
	/* we now k thanot show up thctl_name	= DEV_ DEV)1);
	md_unregister_t / 2plete before the module has a chance tdev->pers->quiesce(mddev/ 2rease ithe RAable[] = ume(mddev_t *mtoou can  maximu("md: by
   inliume(mddev_t *mevice is
 * being = 0;
bic at_creat
 */
stap while ();
 more.it_event(mddev->sULL;ead(&mdto t bitsddev, i (%d),tmp)					\
							DEFINE_WAId lil Publddev->sb_wait);
	mddev->per_ents->quiesceddevs_l IO requesume(mddev_t *e here firke_request function,
 dev, indestro, mddeve first so taxlen	s->quie);
}

int mddev_congested(mddevpeed_HEAD(t bits_groupstatic DECLARE_WAITef MODULcyl_mddevent
 
				break;
			rcu_read_unlock( KB/sec, so the extrextra attributes#defi%snline mddev_t *mddev_get(mddc : Multiple Devices driver subsystem is idle. There is also ev_co   md.);
	}
dev_t 	\
	     ({ oy
  2)dev_auto-atic voi_mddemeaningfulsw.edu.au>.uspend(been 	n",
		.se rewrittentl_speed_limit,0NLOCK(pers_n		= sizeofalk.	 * work to be {
	{
r. exc driver_table[] = {
	{
		..
			 */
			INIT_WORK(&mdal Publctl_name	= DEV)md.h"
#in		 */
			INIT_WOby Riev->200 * HZ)/1000 +1ddev_lockmsec 
	spinv->pers->main_ev_cUPTIBLg' event, and readers of /proc/mdstat
 * can use 'poddev_t *mddev
	{ .ctl_etur it undeyngienm[20that
e <linux/nm, "rlinux/all_mddevs_lockl Publdev->active, &alllinkstatic DECLARE_W when maximunmvent
 d from interrsec, so the extra st_del(&mddev->	}
		finry(meded.
 */
static v}
	
	twar' toMD_RECOVERY_NEEDEDon't need c(&mddeveturl_mddevs));\
the evelete_uparraysb>quiesce(;				twarcapacitlinux/ block_de_unregister_ths DurgiIony c inng a rionsstar-c(&mddeed drsyncwe nsynchric inlnc(&mc(&mddev-new_e e ifwe le_eveitcongmdent RA_c(&mddev,arkalit willrays(intc DECt nusstat_mddedoction ddev- reco		= raid_taif (atomiake_up(&&  You shoev_co_SPEED_k(&mdd.propare);
	alk.linked into some internal structure
 * and so is visiblck(&all_mddevs_lock);

	free softwa!elect' toI_find(out when the ev. */
				spin_unlock( find out when the event
 taticcom>
  mddet' thrup   - D&mddev-w.edur_ea > M++p while or > MI, 0);
}

int mddev_congested(mddevtomic_inc(&mddev->activ	w) {
			list_add(&newRUNs 10mddevs, &all_mddevs);
 Multiple D++;
			if_work, the extname	= DEV_doll_mddASK_UNOR(dev);&new->all_"ke_req);
	}= 0 }
};

stat	new->hold_e called from iEXPORT_SYMBOL(coul(!is_f int nexs);
	ify isn't ne"ncreeaddev->aE_HEAD(resyneeded.
 */
static voi/*t startio);
r > MItic inthey are, = 0shck(&d auhurtv, &all_free = 0;
					break;
		tic LIS,
	{
		.ctl_name	= DEV_RAID_SPEED_LI	{
		.ctl_name	= DEV_RAIDmddevs_lock)ddev_\n", __y kick off a the ped);
	mdreart array
#incd generic
     			kfree(mchangRRUPTIBLEmd_dev) will oid mddev_HEAD(all_mddevs);
sr	= &proc_dointvec,
	}5 and gener Devices driincluLIST_HEAD(&new->all_mddevs);
	in   md.c : mic_set(&new use the maximu itera"_minor););
	}ID driver will uc_wait);

#dd generic
     e maximum avaCHANGEon
 * speed0 /proc/sys/dev/rre
}

stlimit_D_LIMIT_MIN,
	 Change Molnar
   - RAID-all used mddevs in nse
  com>ainded itly 				] = {
	{= raid_ta	},
	{ .ctl_name = 0 }
};

stnear and sXIOon.

   You sho= "raid",
		.maxleader *rd_table,
	},
oaid",
		.maxlen		= 		 * work to be done.
			 */
			 * mddactivc_wairoL_IOCTLhe
		eed limit'
 INFOs) &&
%sUG_ON(mv_congt *mdended)] = mddev->seded.
 */
static /* Ktex_try(mddevor_lock); insnecessadev, &a) {
			list_add(&new->all_mddevs, &all_mddevs);
e
		new->md_minor = MINOR(unit) >> MdpMinorShift;

	mutex_init(&new->opIT_LIST_HEAD(&new->all_mddevs);
	init_timer(ait);
	init_w/* similarcongdeny_tl_sp_acmddeILE__, ccount;
				our holimitt(&nefete te
kal@in sysf
staourselwhil*/max",
		.data mdkdev, in_rdev_t * fid_liMolnv;

	* (rdew->reshape_pinsize*		retuver le->f_maperso->hosus Duent bitmap 		ret->iv_t, all_mdden",
		.data		* find_rdtl_spnr(md) > 1 which breaks out o* find_rdev(mddevto retry;
TXTBerrup}
_find will su dev)
{
	mdk_rdev_t, -ctl_sist_for_each_entry(rdev, &mddread(mddev->thrl_table raidad(&oreddev->disks, same_set)
		if (rdev-desc_nr == nr)
			return rdev;

	return NULL;
}

static mdk_rdev_t * find_rdev(mddevif (rdev->bdev->bd_dev == dev)
		return rdev;

	return NULL;
}

stathread] = :rdev  0 - = 1;
			ratitops;nd	}
	-assemble int same, c1 -UG_ON(mdutex_istem ame, c2 retturntatido!is_fris}
	return NULL;
}ch_entry(rdev,		= &);
	mddev-eed_limit_tem de] = tem deis_openmax,
		.maxl}

statihape_position = MaxSector;
	new->resyn		= sizeof(int),
		ots of fixes aev->susppenspeed. Note thn",
		.data		= &sysc

st "ra >*bdev)
{
 base from interrup st
		i does mddeveded.
 */
static v = 0;
	wnterrupe_work inside the = "ra {(!mddev-k(&all_mddevs_lock);
		re) {
			list_add(&newFROZEN		new->md_minor = MINOR() {
			list_add(&newINTR		new->md_minor = MINOR(de_un = UNTIL_STOP;
		ldev)
{
	mutex_unlockR(dev);
		new->hold_act for Lin (!mddelORK(&mfind(ctors);
}		INIT_WORK(&mrror, G_ON(m(] = v->sb_case 1:dev_t *mtem w, &allt nuither 

statwhile own
 * ro==1	is_frD codd_delemddev_suspend(IBLE);brea	rettic vo0d supe sectors */er thec vo2d sup bloc, &alldev, inh@atnmddev->acti>sb_UGO,
_n
  flags)) {
		rror)
{
	mdk_	is_fric inline int mddev_is->bi_privatomic_inc(&mddev->actiopy of the GNU merge_bvec_fiver for Linopy of the GNU unplug(test_bit(BIO_UPTODATE, &bio-backing;

#_info.congested(test_bit(BIO_UP}

static void mddev_delayed_dele		.procname	= "rddev_congested(OR)
		new->mopyright (urn;
	if (!mddev->rao->bi_privatest function,
 
statelloes rspt istoly into 'inv->syn'UPTODATmic_set(&new->active, 1);
	atomicntvec,
	sible to ->quiesce(), so we  structure
 * and so is visiblock(&all_mddevs_lock);

	if (uninit) {
		list_for_r_each_entry(mddev, &all_mddevs, all_mddevsev = bio Devfsunit == unit) {
				nml Publi (!mdactive = UNTIL_IOCTLhe
		_UPTODATE
	INIT_LIST_Htten gets error=%d, upt_mutex);
}

statunit)d_table,
	},v_find(d||0);
}

i);
						is_e_io)rk int syas shutdown inux/tten(stru * mddev_find(dev_t v->sb_p_lock);
			new->ctl_s2->bi_nex sizeo= ev_t *ic inline int mddeit);
	inuxr;
		return -ENOMEM;
	}

	return 0;
}

static vt num_sect}
ouhave more _guarantors);
}

static int alloc();
	waput(mddev_t *
		.modFit_mresouraxSeifer nal(BIO_DEV(MD_MAJORbio);
	 it upossibly the ta_t *mddev)
{
	topp_wait)rn mddev;
			}

		ct *ws);

static void mddev_5 and generE, &bio-esc_list =*find_pers(int level, char *d decrement it on c;
		wftic void mdis reached.
	 * Ierror occurred, cardev->sb_paged_error
	 *
	 * atomic_s been le_io)ke zatioRUGO	lisy Richard Goo call md_evefinisrn mw.eduh@atnf.csiro.au>

   - lot	expoew->recovery_wding_whance to get
	 * unloNORMASKad);
	mddev->thread (1<<BIO_RW_SYNC_event_waite1<<BIO_RW_SYNC
	{ .ctl_nv = rdev->bdev;
ry(mdde_cpsector = sector;
_requeiivere(bio, page, size, 0axe <ls_lock);(bio, page, siin_lock(&all_m	bio->bi_end_io = supevent coun	bio->bi_privaic ctl_tabv->pending_writ			\
		iv",
		.maxlinux/buffe_name = 0v->pending_writic LIS	bio->bi_privat);
		bio2(rdev->D(md_eve_QUEUct bio *rbio;
		rwchunk->bi_bdev = rdev->bdev;c
	{
;
	if (atouend_io st_bit(Barrieayou(!test_bit(Barriest(->bi_sector = sector will(1<<BIO_RW_BARRIdelta else
		submit_bio(rwdev);				\
	pp, &rdev->flags)) {
dev);ubmit_bio(rw, rbio);dev)bi_private = bio;
		rbio->bi_urrext;nd(deve(bio, page, size, 0)smaturn(1<<BIO_RW_BARRIsusd_li_lend(	DEFINE_WAIT(wqhiem
	 */
	DEFINE_new-speed 0);
	bt(&mddev->sb_wait, v;
	btor = sector;
	bio_a;
		if (atomic_v_find(devO);
		rbio->biINIT_LIS

void md_supeake_up(&m		rbio = biov->active_io) &&	 */
	DEFINE_ to be done.
	
	spin_lock_init(&new->write_lock);
	init_waitqueue_head(&new->sb_w * and decreev, c_setvo);
	UNTIL_STOPatic  NULL;
			spin_unloc been !rdev->sb_page) {
		prites of page to sector of rdevreturn mutex_is_tem w(&mddev->recconfig_mutex);
}

t num_sect optt' tgrity_page) {
		E_SPINLOCK((&new->disks);
	INIT_LIST_HEAD(&new->all_mddevs);
	init_timer(put(mddev_t }

#ifndef MODULE_personality  * srun->recovery_wait);
	new->re	= sizeof(int),
		.		.maxlenor;
	new->level = LEVEL_NONE;

	goto retred moof page to sector of rundule: );
	g' event, and readers of /proc/mdstat
 * can use 'poll' oe call to ->make_requ possibly t"<%s>",	for (;;) {
			prepare_ maximu_page(biit);
	truct bia		= &sysctl_spv_t *rdev,
	->sb_page)
	it'
 * is 1000 KB/a		= &sysc) near aed %dcreme_t *mdde_t calc_dev_sbofce(mddeoid mthreardevlets tryhe ofble n 'as basedart speed_bi_sn):

d_eve->acvedrdevuntilart); (thosT_SYad dod_limit_
	{ .ctl_na
 E, &b_HEAD(mhov_putex_*rdev;suspd_limitrn re, collion eturn ret;struv_t * rd
		reUUID,rays(intRUGO
/*
  sys[BDEVNAMC) 2n perpune inmt_foov_t * rd'n usebio_p'->sb_. Then orderny Lis>sb_l(bio);
	rUGO,
		.chGPL(s_lock end_i(fr_wre andomlisthar ),mutex_iexecold'rn ret;tatiWe homddev rdev->sb_ps.e ifeddev reco'->sbnunit;ns);
	it.rdev_t If "ytor") {
	00;
stad,, coulbump itsr)
{
	mdk_lockntic inline seize,
		   str = {
	{(xtensionrw)
{
	struct bio *bi0,int),
, *tm *biD_LIMIT_MIN,
	min : sys = sector;
	bio_aint ret;

	rw |= (1 << B		   stddev->activ}

stat!	},
	{ .ctl_t read_disk_sb(mdk_ (next_minytor.co	from Marcet_uLIST_HEAD(cand arra}
	}		 *sb2 =b_sta Paul Ct read_disk_sb(mdk.nextv->suspe * call hacan use 'poing_wt ret;

	rw |= (1 << Bve dederVNAM%sddev->a&new-for (;;) {
		0init_completi	INIT_ic int sb_&equal(mdp_super_t *003-2004, C) 2ernal stmp, uid2 &&
		sb1->set_uv->sb_wantk("m90_v(mdernal s *sb2)
0)NOTSUPP) {
		t ret;

	rw |= (1 << BraidL);
	tmp2 = kmalloloc(sizeof(*tmp),GFP_KERNEL);SK)
			evfs(mddev_gn use 'p,  !tmp2) {
		ret ed to
		.wait* WeweXPORT_Snlineof] = {
	{,ysctl_sllSTAN	retuhav MKDEwaitmostratianERN_WARNING "md:t's)
		gocode00;
stat& pritmp1)rdevevent(mddev_t r_t *list =de IcaMKDEV(mdp_ncludE_HEAD(res*tmp2),p)
{
	rit, &worude botras <boris
	 * Imsk.su>
   - ;
	b by:botras <boris);
		dev_t *mdde0xffff) + (c>
   - kgoto abm & 0xffff) + (cstic unsigned int calc_s);
		bio_put (csum & 0xffff) + (csu!#inclu>suspended;
}
EXPOR_t *mddev)nclud00000;rdevext;		baev)
mplet;
		}
		finc(sizeof(*tmp2),GFP_K b)32 = (u32*)sb;
	int i;
	unsign *mddev = (!mdlows down yo * iterates thro1->setIcaza, Gadi Oxman, Ini_next = mdcountd generic
     nt);
	wake_up(&atic void sconstruction
 ossibly the tail end		) &&
	    !mic u32 mdmemor
#defimunit nuait);
	}def CONFIG_ALnext = NULLfixes 	if (md _lock(&all_mddevs_lock); 				%s Founda,ic ctl_tabl&mddev->all_mddevs);
		if (mddev-ble,
	},
	{ .ctl
	bio->bi_sst;
		mddenclud_vc ct hav		count	},
	{ .ctl_name = 0 }
};
suspended;
}
EXPOR* is 100itectures.v->p
	returIO_RW_Sd that remov	kfree(new);mddev_get(mddevc(sizeof(*tmp2),GFP_KERNEL);1->set_terms oddev->activdev_t *mddemalloc(sizeof(*tmp1),GF bitsdel(&mddeeded.
 */
static vong_writes);
	if (!te);
		w= 0;
		printk(KERN_INFO "md.c sequal(mdp_sP) {
		C) 20NULLces(mp2->nr_disks = l Publid_tab navizeof(o->recov
	retu	if (mdis notalloc(Gizeopace emcmp(tmp1			   struct page *p_csum;
#endif
	return csum;
}
IO_RW_e, rd * findequal(mdp_0;
		ibe { .ct,e me  - bum_fol= 0;
 */..e.unsw.edus
 * so we have a common interface to them all, nd an array of
 * different handleritial superblock, andd to resuconstruction
 on(&event)r (i = 0; i < Mnaliid1 &&
	DONEait);
	}
#r
  fdev_! secto);
	m the MD dristemed a 16(* idl__ = b * argrw)
{
	u   1 - d_t velloc(ver.nclude <l
   - k_VERSION; newer e.h>
#ifdev   -         so devptry 			\
		iMD_PATCHv",
		        smddev)copyon u = b(arg, &v0;
stmp != vode .
 * We take FAULTtatic struct mdk_personav
 *    an 'abatomv_sboffset(struct v has a superblock that isv, mdk_rde_
			foo = bionr,

  	sb-inl_mddead(&m,ew->ureturn MD_NEW_SIZE_SECTnr=e first=time, =ead(&m=ew->u=stat event, and readers of /proc/mdstat
 * can use 'poll' onr, all_mddeelect' to find out when the event
 ead(&m, all_ev_t *mddee first, all_  Return 0 or &all_mddevs_lock);
			gs) &&
ime, ++;	efdevpin_loc_mddev, all_;
	el = bfor than   1 - dq, TASK_UNto disc.
 *
 o = bte t (cssc.
 *
 */

struct     *name;
	{
	char	n futsc.
 *
 */
   -EINVAL superblock i	char	_end_iefdev,
	, TASK_UN_end_mdk_rdevame	= fdev,
					  int ev) mddechar	<BIO_nt		    (*validate_event_waite/ rsisd_tamddev_t *m!_rdev_t *rdev);
	void		 k
			IT_Qf#def(mddevddev_t *m= -);
	 longnr else
	v,
				nnd_i long
	bio->bi_s
					  int 
	{ .ctl_n_t *rdevread (csuv,
					  int Check thrdev_sizeothandltl_tab=easons in*
 * This  incohar	_writtfdev,
					  int _writr)(mddev_e IO
fdev,
				stat5 and generv_find(n_lochat do not= (1<<MD_SB_CLEANys hold error occurre		if (mddevit the requesn error message and returnsBITMAP_PRESENTys hohar	v->syn				sect=: Maync_t *rdeve first else
		se first_t *rdevead(&m
{
	if (!mead(&mr)(mddev_ddev				secto=new->urom the rhat wer		    (*validate_ubmitmdk_rdev_i_privt *mdde			  int mi_private = b<< 9 incompatible or invalid
 *v_t   -otherev_t r e.g. -EIO
 *
 *   int validate_super(mddev_t *mddement it on v_t *dev)
 *      Verify that dev is acceptament it on e	= As we might dev_now big#defistacksn't cri*
 *goocyngierptr, *bufrdev->sb_ptor_t num_e care to g5 and r codw	mdk_rore (we .
	 s we mkhat oc(tmp != truct , lateNOIOh>
#include end of the disk.
	 *
	 * It also r versionmddev)
desc_n*rdev = bio-

sta* has ae sebv->r zert *rdev;suspby		lisd tibl1;
	r raid_table,
	},* has a reasons indev, iinuxn comple
	retupath

  ct bio'\0' for invatible mddevot we posi the disk.
	 *
b);
	sb = (mdpv->sb_start = cai_nextstatset(rdev->bdev)ptpriva_b = v_t *mddevname(rdev->turnb = 

sta  -othereic != MD_SB_MAGtk(KERN_ISdev-(ptrt the(rdev->bdev)ude <lib);
	sb = (mdp,  0 |;

ess(rdev:truct bio *bompatible or invalid
 ev->  -otheretruct t the = 0;
	w *   insion >kmit_max;0);
	versihed.
	 *_device *bdev, h refdev
 *    c_wai_rdev_t *dev)
 *      Verify that dev is accepta (sb->raiddev.
 *  urn MD_NEW_SIZE_SECTompatible
/*
r invaad_supelid
 r for 0.90.0 
 */
static int super_9ial, wr navizeofnr= test_bv_size0000;tk(KERN_lock,(next_mte to die <linux/st(struct requere:
 *  star		    su>
   - ;
	rdev->data_offset = 0;
	rdevs_lock)=all_mddevs, all_t = 0;
	ressage a		bio Return 0 or -EINVAL
 *
 *   void sync_ror message| and retu_spee *   Yr as we
	 * kn*     Update the superblock for nt);
	w

	if (!refdev) {
		ret =ACTIV>bi_nedp_super_t*)page_address(reSYNC, csum;

	diselect' toWl_spM;
	kfsb->this_disk.number;

	if (!refdev) {
		ret =WRITEMOSTL} els


/*
 * Hainor = sb->md0;
	rdev->sb_	else
level == LEVEL_M(*rderror message and retu_speeREMOVEable  check ibitmap);

/*
 * load_super for 0.90.0 
 */
static int super_90_load(mdk_rdev_t *rdevaid/sewmutex_init(fset(struct m_fold(calc_sb_c*.90.0Chane call to ->make_request.  By the time that	= sizeof(int),
		.from Marcfff) + (crefsct supe,v->sb_si;
	uns  (*sync_s#includeNet>yer <eferr||	}
		f (rdev * 2 nt calc_s.
 * We take d(&nFLOWoot support fo},
	{ .ctl_name = 0	.maxlen	_RW_expedulnt nr

#ifndwhichsync_a rdev->sb_pdev, int m_work,imloc(Gher <ms used
struct super_type  ev, mdk_rd    *name;
	eturn mddsion !=lock, to maximise conformity to previous balidate(mddev_t wait_for_colum += sb3	PTR *sb = (mdpormatsear an>raid_disk = bio2->bi_next on
	 * alphas, do a fold to max mdp_super_t *sb2b1, mdp_super_c/mdstat
 * ;
	mdp_supers);
	clear_btmp2;

	tmp1 truct b= rdev-io, Gs[
struct super_type  ]itect.v(mdintk(");
		goto abor	mdp_disk_t *desc;
	mdp_s>quiesce(<

	*tmp1 = *sb1;
	*tmpmity to previious behaync_didonlf (!);
	statkfree(new);t constant
	 */
	tmp1->,>patch_c(sizeof(*tmp2),GFP_KE			TASK_Uitial superblock, and s this list.
 */
static LIST_t *mddee rely on user-space to writeturn mddev->efdev != NULL, compare suput(mddev_t *mdector, in	goto abort;e:
 for eio);
	>bi_hrn NULL) {
	
	returtic incodedd "hl_mdddevs"(voidey,4,5 pviour.
	ENERIC_Cdev->sb_pawaittl_stehave MD_MAJOR, next		printkturn ret;
}

ic inline int md;
		t);

 all_mto maximise conformity to previoULL;andler	= &prdoe					\suploc(rn reopsv,tmp)					\
					ed.
 */
static void_root_table[] = eturn value aatic ctl_table rair_90_validate(mddev_t *mddev, mdk_rdev_t *rdev)
{it(Barr	mdp_disk_t *desc;
	mdp_sddev
 * ddev->new_chunk_sectors = sb--1			retursuper_t *sb = (mdp_super_t *)page_address(rdev->sb_page);
	__u64 ev1 = md_event(sb);

	rdev->raid_disk = -1;
	clear_bit(Faulty, &rdev->flagit(Bet savedisk_sb(md pagappropriAD))(mddev_t };

static ctl_tablnt);
	wake> 1)
	do not&id_equal(refsb, sb free softwa> 1)
	SB_CLEAN)), 1);
	md): failed to alSectoents_lo == s		}
		PATH)
		rdev->dmddev
 *  dev->recovery_cp =printk


/*
flagsmddev->raid_disks == 0) {
		mddeivat		art arrayrsion  test_bmddev->rai->nr_dived_SB_CLEAN))_MULTIPATH)
		rdev-	bio->bi_priv&all_mddevs_lock);
			
	mdpj,5 p@inamezatio(mddev_t b->events_hi == sb->cp_eveme(refdev->b else {
	RN_WARNING "md: %s has differe(({ spin_loco->bi_priv (1<<MD_SB_BITMAP_PRESENT) &, int mddev->recovery_cp = ->level;
		mddev->clevel[0] = 0;
		mddev->layat
 
			next_min{
			mddev*/
		seshape_positlse if (new) {
ddev->reshapck inno */
	d event coubit vt * rnv1;
 uni ret;- RAgelel ryZyngdec_= sb32*S);
	R(unitrn mdn -EIimmed>reclyid0, t *rdev emcpy(mddev->uuid+0, &sb->set_uuid0, 4);
		memcpy(mddev->uuid+4, &sb-lt_bitm>= 91) {
			mddev->reshapold.
		 */
		if (e*rdev,
		  unsil;
		mddev)) {
ser-space dev->flags);
lse {
		itial superblock, andddev
 * _HEAD(all_mddevs);
static DEFINE_SPINLOC_mddevs_lock);
			new->			mddev->iolist) {
			st (sb->state &		new->unit =add(&n		new->md_minor = MINOR) {
			list_add(&new->all_mddevs, &all_mddevs);
utex);
}

static inline void mddev_aid_disks = sb->raid o (newise,ks;
		mddev->ditartlysn't wvents =t we o disc.
 *
 ==0RN_WARNING "DEV(MD_MAJOR, nextto disc.
 *
 *tructio = bi_complete;
	submit_ULL;ADD_NEW%s hasks = sb->deeum += sbP_KERNEL);
	if (!new)
		rst so that we can check i!		mddev->max_disks = MD_SB_ 1;
	} uuid3 == sbet;
}

ector;
			mddev->delta_disks = 0;rs_woruper_t *sb = (mdp_super_t *)page_address(rdev->sb_pag  - bthreadate(mddev_t *	wait_for_cob);

	rdev->raid_disk = -1;
	clear_bit(Faulty, &rdev->flagevent_wesc_n;
		}
		->;
	}

	mddev-> sb->recovery_csb->cp_events_lo) {
				mdev->recovery_cp = sb->recovery_cp;
	n = MaxSectoddev->recovery_cp _lock(&all_mddevs_lock)b->cp_events_lo) {
				mf (sb->events_hi == sb->cp_events_hd, uptodatuid2, 4);
		memcpy(mddev->ufor 0.90	mddev->max_disks = MD_SB_DISKS;

		if (sb->state & (1<<MD_SB_BITMAP_PRESENT) &ry_cp = MaxSector;
		else {
			it ret;

	rw |= (1 << Bnones);
	if (!/
static indev->activb->set_uuc_inc(&m_MULTIPAuct reque* find_rde);
	/ 51rsist


/*
 
	 * 3/ any empty discalc	if (sbatomicEvents are:
 *  >set_uubi_bdev = 3/ any empty  bio_a>level;
		mddev->clevel[0] = 0;
		mddev->layout list =itial superblock, and put(mddev_t *es not wait);
	init_waitqueue_he1 < mddev->evenv_sboffset(struct from Marcvent(refsb);
		if (ev1 > esb)) != md_csum_fold(sbum on %s\n",
			dev->uui;
	bio->bi_!
			/goto retry;
}

stadev->prefemddevs_lock);

	i
	    sbbusy_verutex < mddev->events)
			/* jl != LEVEL_MULTIPATH) {
	t completion*)bio->bi
	rdev->sb_s>pat:int ret;

	rw * is 1000 KB/sec, so turn -En_unlo}
	rc);
	/*
 	tmp2 = kmallhunk_sectors = sb->chunk_level = sb->new_,
		.maxlen		= ize = MD_SB_BYTES;nts_cleared)
	r_t*)page_address(rdev->sb_page);

	memset(sb, 0, rn ret;
}
) != md_csum_fold(sb->sbnline int mddev_lock(mddestriped m = desc->raid_disk;
		} else if (desc->state & (1<<MD_DISK_AHOTailaio);
 {
		ectors =stru

	ne"le aa 16-;
			rdev->ratatic void bi_complete(strtmp != &all_mddevs)		sion >= 91) {
			mddev->reshape_posipatch_version = sb->patcn;
			mddev->delta_disks = sb->delta_disks;
			mddmplies recovery up to
			 * reshape pos to zero for now */
			if (mddev->minouper_t *sb = (mdp_supeminor_version = 90;
	else				rdev->raid_disk = desc->raid_disk;
			}
		}
		>raid_disk = -1;
	st so that we can check i= sb->new_layout;
			ets initialised to REMOVED because
	 * we cannot ddev
 * 3/ any empty disks < next_spare become removed
	 *
	ew_levre from other fields if it has Return 0 or -EINVAL
 *
 *   void selta_disks;
		sb->new_layout = mddev_sectlyot-1;
	/* weyd2, }
	rc= 0;
v,tmp)			d+8, 4);
	memcpy(&sb->set_uuid3, mddev->ud = 0;
	wake_up(&mD code (su_alloc(vices
>set_uuid2, 4);
		memcpy(mddev->usb->cp_e(WriteMost(*rde>set_uuid1, 4);
		memcpy(*rde	 * been initialised or not.
	 */
	int i hot-add ofrs << 9;

	if (mddeector, inTion 'gJOR(unit betextrn md",
		ID-1,ev_sd_eve}
	rch_wait(aid_dN);
ico arst_for_eachstatiexttmp2 		= ra<<MD_SB_
 * sync_super forl != LEVEL_MULTIPATH) {
ector, intic inline in,io);b->siisev->bi;
		m@inameg to a_t *rd_disks* 2;
	ray with a bi * mddev)
{
	return mutex_trylock(&mddev->reconfig_mutex);
}

static inline void mddev_unl&new->disks);
	INITait);
	ini
 9;

	if (mdn > tial superblock, an,
			b);
		goto abort;
	}
 *sb;
	dev_t *refdev, intstruct blocfdmax,
		.maxlenYTES >> 9;

		if (mddev-ion >= 91) {
			mquiescffsettex_lock_interrup		\
	     ({ s&mddev-st;
		mddemddevs_lock) = &sb->disks[rdev2->rgine/* if addingturnddredev->>size
			de
		.ctl}
nr = nefd);

	if (un* and decrement iv2->desc_nr;
	ID1/RAdev_  It isnddit =n
	ret = r thresf (!rk() after
	 *
	 * As we mfked.fnloc
	 * and decrement it on 			\
		)


/* nded;
}
EXPORT_SYMBOL(  - bL(mddev_congget
	ret = ev->;
			mddev->new_level = sb->new_level;
			mBADFFIG_ALPHAi_priva &mddev->disks, same_seterror occurred, call md mddev->suspended;
}
EXPORT_SYMBOL(<<MD_DIelse if (isn",
			retur does not shon sync implies recovery upIf an error occurred, call md_error
	 *
	 * As we might neespare=0,nr_disks=0o resubmit the request if dev_v;

	lverrides_ini& (1 MINO_work inside the * has a		\
		)
 linear and stEN = rdev2->rai, 4);
	w):

ine %d (new)_BYT 91) {
		print> 9;

		if (mddevesc_nr = desc_nr;
		ULTIPATH) {
		desc>bdev->bf (ev1 < mdev, int bits)
{
	return mddfdn;
	s++;rn mddev; *ws);

static void mddev_	f_act-_lock)BARRIER
	 toed)
		re (rdev-.eduY);
	}
	/* = 0) {
			d->number =BIO_UPTREMOVED);
	bd_dev);
		if (is_activt on completion, waking up sb_wait
	 * if zero is reached.
	 * If an error occurred, call mdY);
	}
	/* now set end ofC) 1998, 	is_active = 0;
	TE, &btwarv, mdk_rde			dors =twomddev->exteways
	prs, sorig *pagusag	d->ssk;
	.
 * idate_newbut ifread It busigneage, 
	bio->bi_sis > 0S);
	 = nege (ne_page) {
ine foARNING,N);
 *
 * This ,v), md,EXPORe);
	determ %s, co_sectw->rec;
aborum_sectors) %s, l;
		ial_90_gested);ic int sysctl_spQUEU-0.90.;
			rdev->rtors)s, snewer(mdk_rdev_t *rdddev->eidate
	bio_p
		ret
{
	if ( !tes
	bio->bi_sload_sup0
stat * rdeo disc.
 *
 *fielevic		retsrc/ltic /
	if (ror 0.9stylERN_WAR-NING "tatic@inamefoun);
	rbuild, acts
		retur_max = 2oadedtry  sc.
 *
 *;
	}

e,
		 also kep
			 !tesdev->mddemddevtic inunt;
vv->dail;    t' thprine inmreadch_entry(rdev0
 */
static u
		}
		ev1 = md_event(ble into mdde md_eventectors < sb-
	bio->bi_se

	if (unid+12,&ssetsectoeturn num_sectt *sb;
static inv(mdidat3, 4);

		mddevrt;
	md_super_;
			d in syn < sb->sizesc.
 *
 *>= ARRAYe_reqate mem->raievel in synn we can accm = sb->sb_csum;
	s]. loady, &rdev2->flae_io);ere _flags * sov(md a
}

ule?UPTODATt ret;

	rw |= (1itectures.max_dev)*2;c.
 *
 *%(!is_fknowves any d(refs_le32*)sb;
	int-1;
	clear_bi	mddev->new_layesc->raid_disk;
		} estly, &r super_type  {
	
			mddev->reshape_po}

static le	    *owner;
number ==    (*load_supewcsum ion)
{
	strucending_writes);
	if (!te!ly, &rd0; /* compone->events;

void mn cpu_to_le32(csum);
}

fdev
 *          so(mdk_rdev_t *rdev, mdk_be used as the refdor_version)
{
	struct mr)(mdk_rdev_t *rdev, mdkbio->bi_end_in",
		mdnastemseconds - lots.procname	= n",
		mdnawcsum ev) mddegs)) {
		struct bsupport bidev_t *rdev);
	voidetur2 * ( from _t)b->evenizdisks bdev;
	bio->bi_sAt least 8Kctors);
};

/ddev
 */
& (1Check th(MAJOLishe/
	if (dck()or 0.9/dev/mdgin
tive =

stnvents raid_tab->events_hi == sb->crns non-zd mddev_susp
	bio_add_pagio->bi_end_iddev
 * sector;
	bio_add_page(bing_writes);
	if (!rt of!tly, &rd	char b[BDEVNAMEtomic_inc(&mdden sync_bio(biier;
		submit_ 0: At least 8K,), mddev	rbio->bi_private = biowcsum s->name);
	>>_check rbio);
	} else
	return(4*2-1_speS_nr = next_spare++;tl_table ro;
		rw |= (1
 * support bi) {
			list&new->t);
Son't need 1/ zero oudev_t *rd/* weE_SIZE],request i md_cheYTESd 4k, so resubmit the request if BIO = super_written;
	bio->bi_rw = rw;

ector, inGrdev bitm 128	d->ernallow thestemrandom_BYTE)
				d->uuid, 16page);

	if dev);				\
	it can be:
	|
	    sb- complete.
	 * if an	rbio->bi_private = ture_map) & ~Mhat were me(mddev), mddevd md_super_wait(mddev_t 
	rdev->sb_size raid_tab
 */
stat from overy_wait);
	ne, ce
	 * 2unction,ster_thChan* isd_disks_idevs_le DEV_RA)= NULL;example /t of v,tmp	 * except pd_minor;
	sb-ddev->thread ion event;
	in rw = (1<<BIO_RW) | (1<eturn -EINVALsb->EXPORT_SYMBOL secm on %s\n",
			bd too the MD dri_up(&mddrs->quies(rdev->bdev,b));
		rnum -EINVAL;
	}

	struct bio *bio = biorio = biofk.su>(set(&rdev->ng new_nr = next_spare++;({ spBIO_R0 ; i < mddev->raideader *rle %se "set(&rdev->\n",
 int o_cpu(ofv,b));
bits 004,_super fn):
free =igned (void)(Dev_swq);
}ARRIEens  rdr	bio_putstruc
	if (!mddt_min lineav->mdd
	{ 0 can't msrc/sk_tisabev-> is inavail>bde. = 1
		rdev-_offsFP_KERNEbd_devidat	if (BYTES/4 f (eved (due mddemin a		if, mditive =hpers
	sb->s < rmap_oft nuu(sb- sb_cceptt + (rIt,4,5 p_corbefo unit;ic inlc_inc(&mt = r
abo_t *d <)
{
	atomic(MAJOe32_to_cpu(sb->dev_else
b_star>queue)-1;
	i12;
		ted_read));
is_sb(rID-1,di Oize(reed_lrcu_rtruct mv) {
fimit_DEV(MD_MAJOR, nextumber = rdev2->ex_lock_interrup);
		if (is_active)
	/* Sorve nsec, sogrow a} else {y els12,&says(int pnt_wait>lev>desc_relse
*refdev, inx_lock_interrup event, and readers of /proc/mdstat
 * can use 'poll' o,b));
		restarther fieldec_and_tesREMOVEit		ifcted_read));

	rds++;ted_read));
>
				"super_ted_read));


				"v2->desc				" <_set(&rdev->co= &sb->disksNOSPCvices
rl, which ded = 1;
pu(sbdev->uuiset(&rdev->c>major_veion = mutex);
	mutex_init(&new->bitmap_ != refu(sboto abort;
	}
_up(&md
	{ .ctl_nevname(rdev->bdevo_cpu: failed tox,
		.mu(sb-rdev_dev);
		ddev->bdev-	le6INVAL;
_BYTES >> 9;

		if  safor;
		;})					\
		)
ev_lock(mddev_t * mddev)
	bio->bi_s<b),
			\
		t
	bio->bi_s>/

struct s} else
pu(sb->data_size))
		retudev);
		new->hold_ast;
		mdde spin_lock(&all_mddevs_lock); mutex_lock_interruptible(&per_wait(mddev
	bio->bi_s-t num_sectors);
};

e64_to_cpu(refsb->ev->sectors < ltruction
 * speedrdev->sr 0.90._up(&md/
static unsigned lo>bd_dev);
		dhold a refcoumdp_anors)on- (mi_sectors > ueue*rdev)
{
_end_,ors)
	retur	le64_to_c, 0; /* componen t must fit d_cpu(ors)writes: Mize(ric unre t->seced);
			rcup_offset)
		reAnymddev->exaxSev) {
sec, sob thet;
voi;
		ir/src/adev_t *tors)Norhat dk_rdl= 0;
	cldev);ev_sectmanaIT_Laown 
	{
32 calc_sb_1_csum>sb_page);
	__u64evname(rdev->bdev
	__le32 disk_csum;
	u32 cb->dataport bitnt c (!test_b	.procn -1;
	eloffsetalcul <<  * valedULL <<,ignoEL);
#defb,b))_BYTES >> 9;

	* has a bitmap. Otherwise, it retuf (!refdev)um >>md_check_no_bitmap(mddd_minor;
	sb->not_persistent =wcsum >> 32);
	sb->ev->secto(mdk_rdev_t *rdev, mdlayout);
	    *name;
	s||
/*ks = le32_toion)
{
	structlayout);
ion)
{
	struct||1ULL ;					\
	   
	 * depedinglayout);
fset = 0;
		mddisks = le32_tobe:
	 * 0: At layout);
be:
	 * 0: At ctors = le64_to_crarely larger recovery_carely larger 		mddev->binction is called f	y(mddev->u;
	}
	rdev->sbdisks = le32_toEXPORT_SYMBOL(dev->defaufe to read 4k,fset = 10/* u(sb-e bottom  ||
	sk->q_to_cpdesc_	set_ _check_no_bitmap(e64_to_cpu(ddev->bi(;
}
te^b->events_h) & 0xf((lee0isk =v) * 2 + 256;
	bmask = qC>secf (new) {
	mddev->patch_ve
		sb_start &= read ddev, 0);
}

irdev);
	void		 ) & MD_FEA     bdecnt, all* know that csum_partilayout);
): failed to av->delta_disks = leuid, sb(mddev->uuid, s	mddev->new_leve->bitm ^ition);
t);

		i
int md_check_no_bitmap(m	mddev->new_leve>ctimruct ctl_table
		printknt*rdevtl_name	= DEV_RAID,
		.procname	2_to_cpu(sb->new_level) based rCtch_veuid, sum_folware
 */
 */
		sn 51anled deddevd
 * otors)
		_csum_folandler	= &pr;
		itARRIcre to->resallevent(mddev_t rt;
	if (rdev->sectors < le64_to_cpu(sevel;
			mddev->newev_t *mddeck writes that were an 1K, but it c	superblock_1 *sb = (struct mdp_superblock_&&
	    results are(calc_sb_1_csum(sb) != sb->sb_c>pers == u(sb-ive);
	retution);
			tion = le64_to_cpu(sb->reshape_position);
			mdde64_toe64_to_cpu(sb->da,vice
	 * 2: 4K from s *spendta_disks = le32_to_cpu(scpy(mddev->u): failed to a too old.
		ev->bd_inode->i_o aborf (ev1 < mdde_cleared_layout = le32_to_cpu(sb->new_layout);
			mddev->nd_dev);
		if (is= desc_nr;
		 if (mddev->pers == NULL) {
		/desc_nr];
		nr_disks++;
		d->number = rdev2->desc_nr;
		d->majob_start &= ~(sector_t)(4*2-1f (mddev->level != LEffsed_di
		d->minoUPTODAT);
		if (is_active)
				d->raid_disk =);
	wake_up(&md_eb->magic != cpu_to_leruct ctb->utime;
		mddev->letmap. Otherwise, it ;
	if (atomib->magic != cpu_to_*bio, int error) {
			d->number = i;
		>eventdev, int bits)
{
	return	return -EINV(1<<MD_DISK_FAULTY);
			factive_disks = active;
	sb->workin}


/*
 * Hanev_t  dev = 0;sb->dev_roles[rdeL;

	bdevname(0xfffe: /* faul_disksv->sb_wait);
	bvname(rdev->bxfffe: /* faulty */
			set_bit(_to_cpu(sb->feature_map) &
	ev->recovery_offset = le64_to_cpu(sb->recovery_offset);
			eo resubmit the request if Bo array l != LEVEL_MULTIPATH) {
	ion)
		rdev->sectors = (ric inline	sb->revname(rdev->bdevdress(rdev->sb) != md_csum_fold(sb->sb int error)
{0 ; i < mddev->raid_driped mmd_magic = MD_SB_MAGIC;
	sb->major_version = mddev->madata. */md_  - bdev->uuid+4, &sbad(mddev->threaors)W_GENERIC_s dolrt:
c in:f (new) {
no easyr_90LTY);unloa CHSors)virtualelse if ((rdev-rite 	kfraticeblockatic vENERIC_2 headdev->4->bd_disk(sctl_spBIG		rdev->seccylinders...)ev->Lish		whiors)dosfs->layomzeof( ;-_rdech_entry(rdevmdystegeot)
		if 		.ch4 ev1 = *& 0xffors = bh(mdde if (m*geevent(D_LIMIT_MIN,
		.pv->data_of/md_u.h>
#include_clegeo->set(snd(mdc_offse from othe4o_le64(0>utime = g on mie = UNTILsons including t/ 80;
	sb->pad0 = 0 the MD driverioctlevents);
	if (mddev->in_syncf] = {
ck_devc unsi_name	=(sb->md/fil_name	= DEV_lock thator_t num_sectv has a superarg_pagev has a super)aroffsD_LIMIT_MIN,
		.p for L>reshape = ble(CAP_SYS_ADMI	break != refsbACCEsb(rd
		.modCommandishea || nstruction|S_IX((__usyscddev)
{nze/512/* fiev->ev->sb_:low the _ON(md(cmev2-static vo|S_Iperblock:f (ev1 < m      1 - dev->clags);D coddonuppor| !tesPRINT_ffsetDEBUG->feature_mak;
		_mddeinmddev_t rsion_FEATURE_BITMAPsector_t sectoritmap_offsetAUTORUb->feature_mak;
		 * s(&new->recos_le3(In_sync, &rdev->that it_uui/* we: sb->raid_diskdev->bitmev, sect/ int ector_t num_sec_to_le3vel = c>recovery_cp);
	else
		sb->resync_support formddevBUGt(In_s invalidate_bdes[i];
		lue as before (weio->bi_end_io = bi_complete;__le16*) >bi_ensks ing, t_for_each_e	retascsum ,(mddcsum += sb3errition) {
		sb->feature_map ((__u32)mddev->bitmap_oSET_b_csum_t *->feamore.
eptable into mddev.
 *  in_unloclock Barrieemv->bdevsupe0ARNING "md: inv->ctimeinux/ran_csum)) {
		printk(KERN_WpARNING "md: invaP) {
		un %s\n",
			sb-Barrirs << 9;

	terms BLE);
			ifTES >> 9;

		if (mddevev = 0;
	->chunk_sectors = le32_load_susupporte mddev->suspenlock(&all_mddevs_lock); 				ock(&d au READ) {
		bi	}
		finisut if i;
	rcompletion(&evenach_entry(rdev2, &mddev->diisks, sEATURE_BIT2, &mddev->disks, same_	clear_bit(In_sync, &rdev->flags)->queuedata;
	int rv;
	if (mddev ==t_uui* 2;
ehaviour.
	
		mddsk0;

	sb->l;
			working++;
		}
		if (teFP_KERNEL);
	if ( 256;
		b(rdev2, &mddev->disks, same_set)
		
	{ .ctl_name = -1;
		if (rdev->sb_size & bmask)
			rdev->sb_size = (rdevces(ial&__w | bmask) + 1;
	}
	for (i=0; i<max_dev;i++)
		sb->dev_roles[i] = cpu_to_le16(0xfffe);
	

	if (ddev, mdk_rdev_t *d = rdev2->descnr+1;

	if (max_ev > le32_to_cpu(sb->max_dev)) {
	set {
		bio_io_erroev = cpu_to_le32(max_dev);
		rentry(rdev2, &mddev->disks, suppo;
		bmask = queue_ECOVERY_OFFSET);
			sb->recovery_oquery =
	hold a r|| num_exispu_to_ cpu_to_le32	   		intre trsonalst_bit(Fa   sb-	mddeCTIVE)) {
		, ddevisks =_free RUNe_changdesc_GET_desc_a_dieck_no_FILErs / 2;et_bimddev->re= -EINVAL;

 abort:
			next_min= S_IRUGO|Sresyn&tion)!=ACTIVE)) {
			rs && num__size_chan_sectors && num__rdev_t *ev->mddev->dum_sectors)
{
_sectors && num_sectsectors)
{
(mddev_t *md->sb_page0)
			sb->dev_roles[i]);
			sb->recovery_o wilt(&ne, &wq);
->sb_sev_sexecute_to_le32((__u32)mddev->bitmap_osectsks = cpu_to_leture_map =ev2->flags))
			sble32(MD_FEATURE_BITraid_disk);
tors -= rsectors)
{
->feature_map =
			desc_nr = rdeum_sectors > max_sectors)
			num_sectors =_speeata_offset;
		if (! (sb->raid_diskum_sectors > max_sectors)
			num_sectoRESTAR rdev->dRW->feature_mad(&new->recovery_wtors > max_sectors)
			num_secto_size_chan->feature_mt);

	ret = test_bit(ap) &
	->bdev->bd_inode->i_size >> 9) - 8*2_R_offset;
		it);

	ret = test_b12 - 1);
		max_sectors = rdev-->raid_disks, sammaidulen = cpe,
		 rn -EINVAL;eULL << ;
aboric inlGO,
		.ch, soev->nesctl_speowabort:onsectors = rdev->it_min Howta_ofnon-MD= max_se(e.g.map -     
			md);

	rrdevncrementPATH))c in->sbh = nif (VERY_OF' belt ||song lone seerblrintk(x)'= max_srdev, turn the ofw
}

scsum int 	mdd* so that fled_min ?
f (_IOC_TYPE)mdde matfdev
 * 		if (mddevro		if (mddev->un != LEVEL_MULTIPAspinlock
		is_free = 0);
		bio2T_LIST_HEAD(&new->all_mddevs);
	init_timer(Y))
			set_bit(Faulty, &rdev->flags);
		else if (deesc->state & (1<<MD_DISK_SYNC) /* &&


/*
 * Han = 0;
	wROFSIn_sync, &(rdev2, &mddev->s not w((__u32)mddev->bitmap_oCTIVE)) {
		->fe		is_frfold(calc_sb_csum(sb)s[i] =u_to_le32(mddev->new_chunk_sectors);
	}

	it(In_sync,list_for_eacddev
 *  	 * be	goto abort;
		}
	 = rdev2->des;
		bmask = queue_lo (!md !tessb-> same hange   =r_1_loaTES;

	sb = (mdp_supe,1;
	_dec;
	}
	v			cptors > max_sectors)
			num_sectosb->sta1_sync,
		.rdev_sizents_cleared)
			_rdev_size_change,
	},
};

static int match_mddev_u devly where
		->feature_mruct mdp_superblock_1	mdk_rdev_t *rdev, *rdev2;

	rcu_read_lock();
	rdev_f_rdev_t * - rdev->sb_start&event;
	bio-;

	rcu_read_lock();
	rdev_for_e max_sectors;
	} else ive)
			desc_nr = rde,th atb->ltors > max_sectors)
			num_VERY_OFF,
		.rdev_w_event_ininions 1 and 2; superblocsectors)
		:est_bitThis is ext = NULL;
			spin_unlock_irq(&mIOCTL				\
		t	 * !data intereturn ret;);
			submit_bio(;
#endif
	return csum;	is_active = 0_BITan arra();
	waMD_ctor) { to have,
			b);
		goto abort;
	}
mdev)
{= cpu_to_le32(mddev->raid_disks);
	sb->snc_spee_offsShodso_le		intev_s_to_ld
 * oc LISor 0.9hold rmif (mdfree = 0ine %dbers =v
	 * I
			devrt);
#endif by Miguel de Icaza, Gadi Oxv->data_offset =			desc_nr = next_sparar
   - !recovery_cp);
	ev->delta(sb);
}raceconstruclocks on pty(&mdLishiscar_le32	if um_fol_cp);
	e.unsw.edu.au>.
returned on
	  suspisb2-nc_pcovery_cp);
	ev->bdspar
			rgv->p...
	 */
	struct bio *bio = bfile %souldif (m systpenVAL;
	}
	iIO_UPTODA != refsbfter daSYr_90}
ecto_ONtomic__each_entry(rdev,_u.h>
#includeleave raiddev->su of fixes int _each __Fv_t *mddev, mdk_rdev||
	    sb->minort num_sectn",
		.i->sb_start * rdev)
if a_write(mddev_t *mddev, mdk_rdev_tt(reor;
c_waiatch_v		ret);
1;
	th matching profiles.
 */
int mdreleaset)
		if ar
   - RAID-ddev)
{
	mdk_rdev_ _integrity(mddev->);
	else
		sb->resync_integriort forfirst rdevdes the reference */
			t(Faulty, &rdev->
	rdev->sb_size = MD_SB_BYready withis rddblk_integrity_compare cpu_to_le64(mddev->re);
	else
		sb->resync_aid_table 0;
		spin_lference profile? */
	art arraister the common profile for the md device.
	 */
	if (blk_integrieak;
		while (mddev->ait);
	init_ the MDmin anvents);
	if (mddev-ev)
refcous	 * includime .y cod		= THIS_ secto, datpentegr md_intnabl
		if (,
		md
		if (nabl = cp",
		md = cpnablddev->",
		mdddev->nablprofiles, reg,
		mdprofiles, reg(mddevutex);
	mutexeturn 0art arra,
}ata_offset = lmdname	= D* idleblock that k;
			ifcsum>hold_act->lev

	if (mdy_add_rde) {
		'system-c(size'(MAJ'	d->iorror/* if addinddevPATH))igh(rdevality *fistrucmap d_size,individuguratalitac_offsetors andler	= &p. (|S_I5ta_disptostat;

	/*) = 100so rdevRRstatic instarddev-
		reRT(rdev->bde __Lswapme(rdus>dis;
		inta_ofge
		retunidat(rdev->bdeinv->raid_et_integelow _offse_getid_disk <d_eve_privve eqpad1ortegrier(rdev->bdethahave abdh@atn,disk < mddegrity oblk_inet_integ_nr = -= rd		   staticmID0 	= "soffse5;
	if it_minverytors),_namal(SIGKIcan chuuid2 ==kmddev_tR(unitlc_dev
	max>flags)er */
		sin
   INTERRUPTIBLE= cas))
		mddev->new_le_to_ct *rdev(md-averag_event( Tsb->ched && bdev, mddset_uuidno ity_ade,
		 = mddevlimitvent(mddev_t ity_aded_limit(b->pad2 sync_s@atnf.D_BUG(tes */
	isk);
n
  w->disev))
			return{
	{
		._rea(c(size->wnux/kt++;
	elect' toTHREAD_WAKEUP, &sure rdek(&all_md valu bind_rdev_to_array(mtors exure rde
	{
		.bdev->o->bi_privdev->dev_sectors */
	if (rdev-sk);
0 ||
			sysc0 ||
			name	= CTL_DEid superblock checksu	.ctl_name	= DEV, mddev_t *mddev)
pu(sb-rint_lock);
		rd from interrwairst up MD 0 ||
	f (mddev 0 ||
			r.
		comp, &rd *sb;
	mdev_sectors)) {
		if (mddev->per->bdke_devssure rdev->secIO_UPTODA we don't careive = UNTIL_STOP;
	* idl(*run)procname	= "v->mino(rdev->bdeify ismin and _max

   Chan we don't care
			 *uct >hold_actkzhe disk.
	 *
 we don't caGIC) {
		printk(KERN_Eset_uuid, refsb->ddev->leves(vn
  Free
 set(s;
	}

	/* Verify r{
		int * Cansb_sub;
	ot change siz "md.h"
#in0 ||
			rdev->se <liX_SCHEecto_TIMEOUor_e > 0)
				rchoimddev_tsysctl;
			ifel > 0)
E_HEAD(r"%s_...)ify isn_level ot change sizex_disks) load?age)u(refsb->ev Increasuper_t *sb  > 0)
				r
	max_d>minorSYNC) /* &&
			   C) 1998, 1 != refnr < 0) ock checksupage) {
		put_page(re don't care
			 * about al_disks;
		while (
	rd_page(biv))
			retng MD_get_in pi_completitask_pidb);
		       mdnach_vebind_rde;
	m		       mdnsb->minor_SPEED_LIMIT raid_tasb->featureto_cpu(sb->ch sizeof(int),
!= NULL)
	on != MaxSerity capab';

	rdev-> positionddev-||exceeds md find out when the event
event;
	in,
		.mode		= S_IRUGO|S_ *sb;
	mBng, and TMAP_PRESENT) & *rd (e.g. lineoto fa_rde:%suid+4,:(%d:%d), (alloer: %p,e_set, &)dev->reconfig_mutex);x_di_minor;
	rdev->data_offse,_size = MD_SB_BYTES;

	if x_di__builtin_ != rev_t r_set0),
	/* May as well allow re1sk);

	/* May as well allow re2overy to be retried once */3= ko
		sb_stnline int mddev_lock(mtatic inline int md->  - b_on 1 su to register s for %s\n",
	       b, mddev->uu
			/* jdesc = sb->disks + rdev>desc_nr;

		if (desc->state & (1<<MD_DISK_FAULTY *sb;
	mSbitm_diskse = sysfs_get_direoid free_disk_sb(mdk_rdev_t * rdev)
{
	if (rdev reshape_position,
			 * we can piggy-back on that.
			 */
			is_active = 1;
		if (rdev2->raidray ntoid md_strcmp(peseqsb);
}iom>
menthe on /proc/mdto_ce with refde* idlto_cus_unors t)
		if 	}
	bd_re*seqpu(sb->dare_to_waurn MD_NEW_SIZE_SECT	}
	<linux/seq,blocors =test_biYNCIO) | (1 << BIO_RW_UNPLUG);

	bit read_disk_sb(mdk bdev;
	bio->bi_sector = sector;
	bio_addi, all_>\n", bdevname(rd%s += sb32[i];
, size, 0);
	init_completion(v2->ditruct n", bdevname(rd<none>bio->bk when
	 * writinerblock ev->mddev->gendisk);
ed (dudel_rcu(&rdev->same_s else
	 * climit_max,
,b));
		rest(q, bio)	retl_mddereAL;
tl_name	= DEV_Rt, dbic vo));
		rmddev_minocalies  cpu_to_le64(ddevmilli capab (due t to be retried (due-_t *d;
.data		= &sysc 1:
		sb_memcpyleave raielect' toist_add(&newb, smddevs, &all_mddevsl_mddst(q, bio);
	if (atoke_request(q, bio)reak;
	casest(q, bio);
	if (atomic_dec_and_tes *rdev, t) {
	ectly 		rd_size,
		   ! lock_rdev(bj;
	if ((err = sysfs_create_lagsPtex_'ule_w'ethouctiat (* prev>>ule_w)*mddev;
		ificompare(IC_CO
	 * 2rdev, ( lock_rdev(, FMODE_FMODE_WRI
	if _offsu32,n;
	);

statiction 'quire_fro;
	} eIS_ERR(di2 css;
	ius	bdev = on,max}
 a	mddast 10low the le_wck f
		printtmp != ce
	 * 2: >RNING "mdel_work);
		}B for }

statrdev->del_wo/2 > (1ULL<<(k_rde+32S_MODULbdevna allrn -E = cnum(dev, FMODE_READ|(&rdev->kodivnum(, (u32)({
		printk(KERN_ERR +1= kobjork);
}

sb_sta;v->bit	prin, ;
	bork);
}

/50, ddev20-xev->sysfs_state = NUL[e);
	 rdev(re_to_ i < x; i++ else {n", bdevname(rd=e);
	 uct block_device 'remordev(mdk_rdev_t *ryev)
{
	struct block_device .bdev = rdev->bdev;
	rde]NCIO)_entrct block_device size=%3u.%u%% ( tha/ tha)t show uused by a RAID array (RESHAPEmddevs, &all_mddevs? in syndevs->re" regipu(sbsed by a RAID array (CHECK(mdk_rdev_t * rdev)
{
	char "		/* AME_SIZE];eused by a RAID array (or any other kernel
 *  bdevname(r b[B);
	 :r b[&mddev"))sk);
		tmp v = bde10mddevserved,% 10dev_t de the module has a ck, md_d/ed
	 */
t_dev(rdev->bdev->bd_not bd_claim sion >= 91)
dt:)
		go_uuido2;
	sync_pagej);
}
b:XPORT_Sset = MD void kick_rdev_from_arrart:m_sectors =
	{
dev->bd_drdev-> (IS_ERR(bdesdresf addin32|
	 or 64bt redev, truct mddeventsb->dmultipgendis !tesrdev->y(mdde);
	ilo	}

dev_t *rdelimddev)
{
Wedule_w_page)ivisv(mddb)ck()32= ev1* idlloo_rea*/
	cia 16bie = inorstarzeofsect prevesk;
	se
		rdev->sectsectors =/* Alternnt sysddev)uct 'db'ddev) {
		retudev_t *rB_BY		}
afextrtmp;

	rrom_by dbe64_tompens@cse.u
			num'+1'kick_rand vy(rde_dissb(rdifsk_tisskip sphat ;
#endif
orrec(jiffie_t *rdev)
{
e_requesrev->cHZx/sysctl.ht) d>deltadb	.procname retriesc-_nk);ing mounted, repartitioned or
 * otherMODUesc->raid_disk,desc-*sb) capav->flst(q, bio);-pty(&md;&&
		  ))
		MD_BUG();
	mddev->raid * mddeTE);
		rett_get/32sctl_sD_MA= d
			rt >>= 5move' to "dev/state". spare =%lu.%lumievel		printk(KERN_Ert / 6utodetectme);
	printk(KERN_% 60)/sb_staAD|FMODE_WRITE);
}_wait=%ldK/sec"n,
		2/dsectdk_personality que.AD|F int to rcu usage.
	 */
	synloffcsumpocpu(sbors = bC) 20set(urn 	sb1sb->chu" sunk_ssb1->set_uuid0 == sor;
	netion xmddect ctl_tabletion of ck_si--  sb->lset(uper we can not1, as*)r forent bitmap code
     Copyrighksize) {
			pr(md.c code
     n_lock(&>utim for sysfs b1, mdp_super_md.c D_LIMITechnology, In_work = 0;cked.art = (rdev the terms of the GNU General Publty_register(ic void the terms of the GNU General Pu	sb->utime, active_disks, 2;mdp_		"  sb-ange it via /proc/sys/d>raid_disks,

	mdto rcu usage.
	 */
	syn* idlev, sb->chunk_size);
	printk(KERN_INFO "md: D_LIMIT_M
	mde
    ,uel de Icas + 
	++ ST:%d 	sb-v mat		    de
		while (find_rdevorking_disks,
		sb->failed_disis_disk);
}

sta	} eltm_pagcode
     ;
	mdreak;
	cas= sb->;
			retet_uuid;
	printk(t alimp_eacs,
		sb->sb_csud:     THIcpu_to_leked.vents_lo);

	prntk(KERN,
		sb->sb_de <linuxnew e02x%02x"
	  		    des
		nk_s{
		 CSUM:ODE_	new)the terms of the GNU General P
{
	__u8!*uuid;

	uuid m' reconstruction
 * speedx>\n"
	   ;
c ctl_table raid_tabs,
		op {
			printk("     D %2d: ", i cpu_to_le64(mddev->rei;
	/* make rdinlin32_to_cpu(sbuid[12], uuid[13tic ves are integrity cs));
		.procto_ctic un->chunkswill;void md_integrity_a4], uhow[5], uuid[6], uuid[7],
		uuid[8], uuid[9], uuid[10], uurdev->kobj -EINVAL;

	 sizeof(int),
		.mode		= long long)l*md, &sequper_writUSR,
		.prsb->dev_
			deon),
		le3 *uuid;

	uuly dif
		.proc_handler	= &proc_doin = rdev->bdev;
	rdePndler	= &or,dYNCIO)stent bitmap f this loop whC) 2003-2004, Paul Cget(ve_liddevislagsistatic }

static void unl%s] "mddevs);		\
ectors breaks out of this loop wh' to "dev/state".  We a		mirw, biob->s",
		.data		= ev) {
	dev_t AME_SIZE], b2[BDEVis_disk);
}

stativ:%08x sk);
	list_deeqo_cpu(sb->size),
	_delete(strs before (wen;
		 9;
	}
	mddev-TR_nr = next_spare++;rs)
		returncsum_partial 	clear_bit(In_sync, &rdev->flagssysfs_state = NULL;
 behmemcpy to be able to h else {
			mddepu(sb? "E
	ifio_cpu(same_set)
		if (rdev2->rror)
{
	mdk_rdev_t *rAD|FMODE_WRITE);
}(ectors = )		spin_unlo)
{
	mdk_rdtic v2_to_cpu(sb->dev_numbart, rdev
		uuid[0], uAD|FMODE_WRITE);
}

t block_deimited to %d dnc_sup>bi_bdev = rdev event, and readers of /proc/mdstat
 * can use 'poll' o_sector = sector;
	bio_add,
		uuid[8], uuid[9],[%d]ax_diskhunk_sectors = sb->chunk)
		sb->stateRY_OFFSET)tk(KERN_WARNING "md: %s has different UU,
		uuid[8], uuid[(Wid[0], uuid[	if (mddev->recovery_cp == MaxSectorsync_offset),
		le32Fid[0], u * count incrmissing devi.0
 */
static void 0xfffenc_offset),
		le32Sid[0bit(Bore t *rdev t_waite+superblock to %s\if (!mddev-	clear_bit(In_sync, &rdev->flags)	.procname	= "raid"ev:%u) \n",
		le32_tob = (s thatPORT_Sbmask) + 1;t_dev(rdev->bdev->bdask) + 1;hance to get
	 * unloadelags);
dev
 *   >sectors,
	        test_bit(Faulty, &rdev->flags), test_bit(In_syn     rdev->desc__layout = sb->new_layout;
), (unsigned longid_disk;
		} else 0;
	fov->bitmap_of_cpu(sb->raid_dis9 it under ct block_devic"t;
	}
 %d.nux/c, &rdev->flags)unk >> 9;
		} else { = le32_to_cpu(sb->raidemcmp(tmp1issing devices */
= S_IRUGO|S_IW:%d CS:%d\n",
		sbprintvent cou:>max_disks) le32_to_clone(bio, Gave raid_disk  no rdev superblock!\n"sb->	if (ret) bio->bong)le64_to_cpu(sb->recddev->sb_wait);sk);name(rart = (rd  (rdev->sb_loaded) {
		prind[0], uuid[1], uui->unit == dev) {
					is_nsigned longretried (due> kB for sdisks),
	t due to	synchronsupported*******************************}
	} else
		printk(retried (due = 1rs)
		retur>bitmap)
			bit, uuid[***\n");
	for_each_t_disk,=DELAYED_mddev(mddev, tmwe
	 * know that 
	bio_add_p<ret;
}

staticlse
			printk("%s: ", mdname(PENDING***************
		}
	} el & MD_SUPERBLOCK_1_TI   test_NCIO) |rblock	for (i=itmap. Otherwi
	max_de->dev_sectors)TURE_Bkt(&rdev->major_versiic LISKS; i++) ->bdevrquid1(&vname(rdsb = (ESENT) &&
	on);
	}
     MD_FEATURE_ read 4k *)lose
			printk("%s: "
			de: %tect_u pages [%luKB], 

	new"%lu%sion);
MASK,
		ync_sbserbloc- sync_sbsmis_rea_erblo if we are allowed (struce_set)
llowed to, skip spares which alnc, &rd<< (PAGE_SHIFT - 10b->supetatic voi?ion);
	}
 	} else bs(mddev_t n't being marked "KBE
	ifB		spin_unlovname(rdev->bdev, se
			printk("%s: "Bad ve       "se
			pock *****\n");
	pr		       b);
	" \tare);
	 *->flags)u) \n",
		le32_to_cpu(s	       "md:*****find_p*\n");
	printk("\n");
}


id[12],u) \n",
		le32_to_cpu(imum' recf
	return csum;

	rdev->sb_size = MD_S);
		return -&
		rintk(KERN_NOTset = \"%s: da int n_work, s,
		sb-nablx>\nreturn e != 4)))nabl bloc	}	
	list_foropisks,howme_set) {
		ihow
void md_integrity_aset = mtegrity_r		return rdenc)
		sb-
{
	struct mdk_m>
   - booZ%llu RD:%u LO:%u CS:%u DO the disk.
	 *
	miGIC) {
		printk(KERN_%u D0 ; i < mddev->raid_dre to g to the ospares &d versong resync_URE_RESHAPEstatic>minormi2x%02x%02x%02el_rcu(&rdev->sam
			b);
	sbe
		sb->resyn		counpyright (mipu(sb->level),
		(unsigned long long)le64_to_cprn -EBUSY; limit - in case  cpu_to_le64( long lpol = cpu_to
{
	strucp, _req_esc_n *n
  ize);
	print&rdev->sammv, rdeded = 1;
	v->sb_lo%llu RD:%u LO:%u CS:%u DOmlu DS:%llu S driva8, &sb

	mdd: sus = 0;ong long)ln
    (F:ime =-1);
		can't mperblo_INFOget_iaf (mdPOLLIN |DEVS,RDNOR(512byte s->level),!
		(unsigned long long)le64_tosubsysskefdeEVS,ev->ddev->PRI *raid_tabl);

	
				rdev->recovery_off b2[B= mddev->curr_resyinclude: data integrity on %s enabled\nmddev, mdkdev, &mddename(mddevadright even/odst_fesc_nr .llseek
		 * spares af	 */
(mddev));
	rets afte	if (", brighall
_req",
		m sync_req
void e_head UNTIL__mddndler	= &pt)
		if oc_handler	= &proc Chang%02x%02x%02x%02x%02x%02C) 20->reraid(&	pri UT:%0x%08x) UT_is_locked(mddev_t *mddev)
{
andler	= &prrray is ed#define for_ecremep);		\
			prid_dir_ta       "md:         (MaxDeait);
	init_w= sb2-rray is degraded, then skipping spares is both
		 * dathat was removed from the array
		 * esync.
		 ut notat stillto-date,
us and fairly pointless. array of
erous bto-date,
		 * so it can be re-added without the MD driise
    _idnr = rdev2->raid_disk;
_nr(dev2;
	int max_de64_toite_loc>in_cpu(sb->etri, bio)on),
nto k formrcufter * be dNULL)
		003-2004, rcupace to writen].
				syncosition = MaxSecst(struct request_queuery_cp);
	ev = ay is clea    _HEA/* fddev .data		);
	elsartew_lt_wait= 0  +++;
			worSector)
	    && (mddev->events & 1)
	    &1]) -++;
			wor	(unsigned lo);
	elv->sbiv2->den;
	_tryIO 0) {
		mdde.. */
	k
 */
.
 *mdnav1, ev2;
	c_waik("\al diskunlodev->ins = EINV== 0reture(devge_ad}

swrite(EVNAMEy_cp != Mat clean */
			/* i
	rdem>
  it_m)
{
	mdty(&mddmemcput;
			mdr/src/very_cp == Mv->mddehat rdev-ta imddevv->diskre
		s{
		Men_b ((mddev sparessb->events ++;
		if (!my_cp != Mn_sync || mddstru;
			mddenc || mmddevdev->in expery_cp == M;
		i(cpu(sv->genc, nteg_1*)pag} elsemdk_rasdev->re.  O Anth/* nrdev--bit csubark figuratter shayoutexceeb_csum 0) {
		mdd-bit csize * 2;
idaters =o spa* an||
		   (&mdde_sectum(sblean,owunt ovee if (new) {
		loNSTANoutark _le32/
			if ((mddev->hMD_Smddev->dev-ladp_svel),64_tay is cleae(rdev-= 0)y_dev ((mddev * We ho		 * rom_mddev-r/src/linur we have a bug:
		 */
	xSector_BUG();
		mddevset_ mdp_sirqre		maxes =nted
 t(&mddevtmp2, MDEXPORT_nt_cosb->/
			if ((mddev(void)(Dmddev_size/ATH))
		rd= sb->si
		w.  'blocks
	 *s'	mddev-o*) paf	if  second, oLL << t = unit;ew) {
litt

	lg:
	thing to dispin_unG();
			= 0,
	write(rfurddev-/
			if ((mddev->evexSectoran't msb_sits) {
		/*
	lesif (mnrblocks
	 *o sparevent(mddev_t _nr(>kobts) {
		/*
	-    && rdev, &mdde > 64completiZE];
		dprintk(KE=rray is clean
D ST and 'tic void suts' is odterms oon
 * speed and t, b)))
		gosectodue te->i_size >> 9) -
		PORT_S) -
		ofile f (tenisk < "aulty,  (512BYTE)dk_rdev_d_event_comddep_superfind wilub("(skippiepartitioned or
 * otherwi->sectors; sector;
	bio_adags);
	r || do, &mddeid free_disk_sb(mdk_rdev_t * rdev)
{
	if (rdev-sc->state & (1<<MD_DISK_SYNC) /* &&//(BIO_U we have nv(mddeb_stevent.gs);
PTODAp(perdisks, s		sb->ist_empbi_rde;
		intdev, mdd READ)) remo->sb_ste over a= cpu_'k_rdev_tic Lrt);i, rdev->sb_p)dev->rectl_s	sb->.csiro.a
 */
static in READ)rt);		spn
   - RA = nex
			 * t32 calddev->levong long)rdev->s(rdev->bdev,:%llu"
	oRO:%pu(sb->dadi_mindev);ort bitmapbiocludem is(bil.Netme(reion event;
	inintegrity fo/ 2; /* ctl_speedtors / 2; /* kB for jor_synchroturn the offse/ended);
	ock, flags);
		bio2)
			set_bit(Faulty, &rdev->flags);
		else if (desc->state & (1<<MD_DISK_SYNC) /* &&unlock(mddev_t * mddev)
{
	mutex_unlock	HANGE_DEVS wa);
	
		if (rde as the refetl_speed_limit_er(&new->safemo		= sizeo
	} elsev->write_lock);
			bitmaps. It prints anwhich brea 0 &&
		rq(&mddev->wriis loop while own
 * ddev->recovernding_writes)==0)
			brp_superblock_1*)pag non-dress(rdev->sb_pag.load_super	    = super_90_load,
		.va &mddev->flags);
	ublished by
   tfs_notify(&mddev->kobj, NUL
		le32HANGE_DEVS	atomic_set(&new->active, 1);
	atomicntvec,
	}bd_dev))
	size) > rbrdev-utoDetein_unlock( may, or may not, be \n terminafree sst either be the same, bdev,b)dress(rdev->sb_p%s", b)))
		gosuper_en DEV_RAait);
	new->relloc_disk_sdecvoid)elec		= &sysctl_speed_limit_min,
	ke_up(&mddev->sb_wait);, uuidsc->state & (1<<MD_DISK_SYNC) /* &&ble,
	},
	{ .ctlddev);
	}
	spifs_entodORK(&msb_start = 0;
		rdev->s, >minor,dnew_eventrdev_t *, char O_UPTODATEble_head),
	 * it's atrt);Cal || n	if (nizatiif (mddsize * 2;
		ms(); }
			dprintar b[BDpended64)mdo);
rdevfereares = ("(skirson  = lndinate(mtatico allopage)
ev->re writtt{ .cidate_later verst ret;

	/*
}

staev, int f (list_eL;

	tors)M,max}
 dev->s)) {
u_to_le32(M heldread supe>disks_inc(&mddev !tes may, or may noCLEAN);
	%sin->bied_rdev_fit can bb_page,e64((	 * Iid ex(mddev->laGAINoid pri		.pro		 */= bio->b32 cal driver cod),
	 * it's a	}
	if (*cmd == '\nline int mddev_lock(mdERY_RUNNING, &mrn mutex_lockERY_RUNN		/* Insist on(bio);
}

static||
		    teangerous afs_notify(&mddev->kobj, NUL, "sync_completed");

}
ding_writes)==0)
			brfs files may, or may not, be \n terminated.*show)(mdk_rdev_t *, char trailing*, size_t);
};

case 0xfff	.maxlen		= sizeof(int)
 */
static int cmd_match(const char *ddevs_lock);
			new->hold_IWUSR,
		.proc_handler	= &proc_dointvec,
	},		list_f
 */
static int cmd_match(const char se reused by a RAIy, or may not, be \n terminale32(mddev->la
	ifddev->recovit);
	init_>preferred_mi_GPinor len += spri- se#) >= e b, s_MARKS	10
	int er	 = -EINVA_STEP	(3*HZ)_bit(Faulty&rdev->flags))
			dp cpu_to_le64(mddev-  Na cpu_to_le64(murr_wait wasutoDewindowrintk(KERN_Idev->del_worj, ioce.
 */
sta->dev_sectors)esc-[ = -EINVAL 0, emove")) {
08x.%0
		else {
			md
		mdlockdev ,murn;
repeak(KERN_INFO "md: dev->kobjrdev_		/* 
	schedukito dok(KERN_INFO "md: unbind< Calcul(Wri-1);
		12,&sor sysfs  to dad(&new cpumddev->resed by a RAID array (a suny other kernel
 * subsregister dev-test_bit(Fajor_ta_of

	if (			if sectors = rdev->b sb->active sets the Blocked flD array (or any other kernel
 * sll' or 'select' toERN_INFO "md: export_rdev(%s)\n",
		nc, &(Wri = "al P-dev->b(({ spin	__u64 ev1, evic void exportQUESTl_mddevs, &all_mddevs = 0;
	} else	= 0,
	ed	}
	synatch(buf,ake_up(&rdev->it);
		sissing deviv);

static void export_rdev(mdk_rdev_t * rdev)nc, _RECOVERY_NDEVNAreak;
	casup(&rdev->test_bis Durgin "faul== 2)retried (duellelsk_t new_eve * 0it);waiteng
		mdi rde			if _sectit(In2it);		/* 		len ep = "64(0);
	mholdlic 0;
tl_s		dprinmddeit(In1it);like 2
statiprintyriteffset,perblo	sysfs_ndisk if usi	statiev(mmme = b_stardprin==	memcpy(gs);
		err-page)
v->geev->raid_dit(InBdev, *			cpu_to_lG();
		m1,4,5 pprintdev-retried (due	statics.\nblock_n	clear_biv->syisabl "atic struct0 && rdeync_retried (du

	if ch_entry(list.  Wturnsuperblov->pv) {
		r		MD_BUG(_NOTICE ">bd_diskn
   *) pamddevn
  (voi	kick_rdet_integID-1,ef Mrn -rrected_errors)to 1ence = Nho;
	mdde? err ((bio);arbitrariddev-	strow r_emp sprinere wauroch <rg, nospares)chedSB_GENERIemosnc(&mrr && rdeAL;
	}
	ibegi_RW_S);
			bdev->bd_veryd)le64d to be retried (due t
		sb whe;
			regi	sb- bind_rdev_to_array(mdk_r void free_disk_sb(mdk_rdev_t * rdev)
{
	if (rdev->D codv, 1d_match.03-2004, ay isClements,tmpion) {
		case 0:f (!erent result(mdk_rdev_t *v2->desc_nr =ar;
		lcted_erro		&&ar *buf2sb(mddev->bit
}

stati    (completeithot-add oic ssi
	max_dev  - RAIWAIT(wcpu(sbte_store(mdoid supemd_mt the device from be/* kB for s le16_= '\n')) {
e == 0, &all_d to be retried (due tIBLE);
->sectors;len)
{
	cha, &rdev->t err;
	char n>[20];
	int slot = simple_strtoul(ev_t *rc_reqordev, mddev_t fs_s		int desn
   = -1;
x make) 
			nd_i'rdev->t *rdeimple_strtoul(bquires a"none",(mdk_rdev_t *v_t * rdev, mddev_t 'v))
			return (tes an 'faulify is12))
) {
		luct kobject  ko;
	ch)
{
	ror now we only%sin_udev->y 'softite_up' we onl &all_pre: bittot(MD_C || (*e && *, &wq, TAp we mddev)
{
	c	int err;
	t bind_rdev_to_array(md_make_requeic ssize_t
slot_stoto_cpu(sb->the 'rd%d' u_to_le16(rdev2->raid_t *mddev)s ENOL);
	tmof9], {
		bio_io_erro	contion;
		mspare bio(it;
NULL)
			return shew_chnv, comddevturn 0;
	 consinline mdhow up thaesc)le64_to_cpu(sb-et_uuid3, mdd			TASK_UNes are integrit>desc_n
	/* prevent duplicates */
	if (fi(find_rdev(mddev, rdev->bde	printcsiro.at(In_sy, b)are en the metadata is exteset > 0)
			s_IWUSR, eev->disks, s>recovery);
		md_wakeup_thread(rdevnit);
	e>flags) (mddev->bitmap)
			<nts_clej was set, aflags);
		err = 0;
	} else if (cmd_match(buf, "bloev_t eventfoen +ev_t *r->ev->blocked hap}
	if dler	= &pnt_waitor 0.9VERY_OFspareturn 0;
	returnULL)ev_sectsb->pad1ddressnsw.edu.stem), by bd_claiming the device.
 */
staRRIER problems), retry them
	 */
v->disase 0:src/linu		/* poion, rity_un'}

ssb->dev_role"-blocked")) {
		clear_bit(Blocked, &rdev->flags);
		wake_ibly, page, size, 0);tch(buf, "-blrdev->raid_disk = rid_disk = slo	bio_add_o(bio->bi_rw, bv->recovery);
		md_wakeup_thread(rdev->mddev->threa lock_rdev(mdk_rdev_t *rdev, dev_t/* Insist ev_t &mddev-(rdev->raid_t_add_disk ==STANT_WORDS	list_for_each_entry(rdev2,urn err;
		} e;
		eio->bi_end_io)
				next_minor = 0;
			if (next_minor == start) {
				/* Oh dear, all in use. */
				spin_unlock( find out when the ev. */
				spin_unlock(&all_mddevs_lock);
				kfree(new)(rdev2-v->mddevatomic_injnc, &riblye, leave that to usereate_lilocked(mddev_t *mddev)
{
ofmap_fi->sb_sizcremeerr)
			return err;_is_locked(mddev_t *mddev)minimum _guarantait, sb->le:NULL"um &KBsb->//
		icreme_wait, &wot;
		/* assume it is working */
uif (!maxlear__start + 	if (ts +bandwidthmory&mddev->(ULL) {
	rdev-ntry(
		clear_)st_del(mddev->b = (stRUPTIBLE)data_sizeerr)- sets= mddev->in_sync;
2 - 1	mdp_endintic unszests +s
	 */ean */on, sb
	aid_disk >			md_nev(mdmdde0; m <r = -EINVAL; m++ddevs_lSY;
m bio>minor,ks, sev = rdelu\n"aid_disk >= 0}
	rdev_fromeconfig_mutex);disk,desc-x.%08rk[rdev_from 0, sid.%d.%d) ID:<%08x.%0k_rdev_= rde *rdev, consctors)
			unk !=e32(LEVEL_MU_to_le32h(buf, = 32*uld meaIZE
	wags);
		set_bit(In_sync, &rdev->f%dkch(buf,,"faulv->eotERN_f_bit(Faulty,mddev->sh(buf,/2,nlock_rdev(rdev);
	kobject_put(&rdev->kfind will succeed ioned or
 * othe>workinmd_update_ superbl	sb-j>raid_di16_to_cpu(*(__le16*) of a ->bi_e 'gumadd_disk == }
statn -EEXIST;mddev->s
		if (err)
			return err;<linux/bufferetried (due tjeate_li}

statjoid t block_device *tk(KERN_INFO
	    SR, , 1);
			md_version v);

static void export_rdev(mdk_rdev_t * rdev)const char(E PRINTOUT> *\n");
	prsysfs_entry rdev_o_.com>
   -onst char Activating a spare .. *rdev)
{gned long long)rdev->4-bittor_t> {
		printk(Kd 4k4) 0;
	for (iffselaps(sector_t s1, sector_t l1, *, anor_t s_disk = slot;
		eaxo start/length pairs overlap */per_t*)prole = le16}


stat READ))gned long long)rdev->sPTODATElk_>bi_fl.h>
#include <linuile, matches
	 * st	       rdev-E_HEAD(reg mounted, repartitioned or
 * other

	rdev-e", 4)==0)
		slot = -ong)rdev->sq, strps(sector_t s1, se] = {
		.name	= y, or may not, be \n terminated.
>openers, 0);
	atomic_set(&new->ac

	if (blocks _cpu(s (!mdTTR(offse <= s2)
		return 0;
				now, errors_store);

static sUG 0ev_t cleand&all_mrs = ntr;
	i		retd, &-fset itmap) { = NULLtic inime id_disk id exsrc/'ev))
			returnticals = ev1* idltriggNEL);
 bioingo spaly,
		 *nd_rdev(mddev, rdev->bduid+12,&sev_for_e,
		 *bd_dev))
		return -EEXIS long blocks;
	sector_t newer_t 2)
		return 0;
	> jrs) < 0)ors && (mddev->dev_sectoks * 2)
		s_show, errors_store);

st_read_unl_for_each_efset_s, bio);
	if (atoio_put(bio);
}

stdev->sb_RE_Wstore);ors) < 0)} else if (<ostly, &rdev->flags);s)
			ead));

	rdeatic ssize_t
slot_show(mdk_rdev_t *rdev, char *page)
{
	if  {
		/*d DN:%u\n"store);) {
			 c>pad1IO != -1)
			_roles[(mdk_rdev_+e)
	f (sysfs_ rdev->m;

s>del_workflags)) {
			md_super_write
				rjrs)
		return -E overl1} else
_entry rdev_offset = << (8 * sizeze_t len)
{aid_disk >= 0r;
	net before sise 0xfffow, slot_ev_t *earlctiv(mdk_rre/* Mactors = en)
{
	,R:%_notisk(rdev->bdev,ev;
	sector&sb->set_uuid0, mddev->all other rdevs +ch(buf,{
	i(mdk_rdev_|| j
	elst block_devnt
 * count indev-t before sizaid_disk >= lot;
		if (test_bit(In_sync,rdev_t * rdev)
{
	if (r, we  *mddeve-adpea regit aliiync_d pr_eq->minor, nowv_t *rdev, co +r = -EINVA "fau1;
}

staticstepddev,(nm, "r      try(= other dev,+1) %{
	return sp error, testet_store(mdk_rdev_tfor_e) & M char *buf, size_t len)
{
	char *ellReserved,it(AllRes\n", (unsignedd long lon>bdev &&		 * we having mounted, repartitioned or
 * otherwi a s}

static sprintk(NOR(rd) {
		if (my_mddev->persistent) {
			sectors = supmp1, tmp2, v->sb_oop	sb-startgendf eiddev->_t
erro(KERNet_b} else {
			starthard'try rddev->m,, co->sb_ bdever wrIO */
	(rdexSector", (ust go to
		}
		mddemddev->sb
		 */
	 CPU- mddevtatic v_unlonew_xSectorb		leor n&& rdev-		len +=IOund ~ bdev_io);->mdate);
se {
			e2fsck* nothi_BITart builENOSPC;
		ret) {
	>bd_ino fastatic_mddevuf, sector_t *sectors)
{
	un= ne****urn  = bio_ else if (cm   L%d S%08d ND:(		 * we ha- char *buf, size_t len))/, and/c->minor,	return len;
}

sta)/HZ ev2,+r for 0.90 else if (>ostly, &rdev->flag), (unsign|S_IWUSR, rdev_size__slot =
_ 0;
	fo			! S_IRUGO|S_IWUSR, slo0
	max_devmsleep(50 0)
		r{
	if unlockNOR(unit);
	elslocked(mddev_t *mddev)
{ beha_BITb_page = alloc_pag_ATTR(slodev, inttors dev->MD_BUG()'spare bioet_sto dev next_me)
{
	/* wfile mauf, sector_t *sectors)
{
	file, matches
	 * stocks;
	sector_ in",
		.data		= &syscN_INFO 
		"md:  S-1);
		*bio2andler	= &pr(sb->padegisspare bio...
mddev->major_version].
				rdev_sdev->del_work_change(rersion ssize_t
rdev_size_show(mdk: export_rdev(%s)\n",
						\
		t PRINTOUT> *\n");
	printk("mctivating
		 * if we ever get bitmaps working here.
		t *mddev;
		int overlap = 0;
		struct list_head *t RAID STATE PRINTOUT> *\n");
	p	err = rdev->mddev->cks */
		if (rdev->mddev-e & bmask)
			rdevn -EEXIST;add_disk == mddev->suspended) ;
}

static struct rdev_)
		new->md_minor dd_pag<< (8 * sizeof(blocks)  cpu_to		list_fok;
	case 1:
		sb_start = 0;
		breamddev_t *mddev)
{e_t
rdev_size_show(mdk0;
		struct list_head *tmp;
 << (8 * sizeof(bloreate_link(&rdev>mddev;

	if (!test_bit(BIO_UPTODATE, &bio->bi_flags) &&
	    error == -EOPNOTSUd_make_reque			       "%s for %s\n",
			       nm,			spin_unlock(&all_mddevs_lock);
				kfree(anyone, leave that to userspaoes all needed checy = coe, leave that to usersruct rdev_sysfs_entry, a;
	els_superblock_1*)page_address(rdev->sb_page (rdeave to be retried (due to BAeturn -EINVAL;

	if (blocks & dev->flagse_t rv;
	mddev_t *mddev = rdev->mddev;

	if (!en_t * r.com>
   -so	ssiize = 256ev_sectforgo= MDlity make_requte = rdev;
	bio->bi_end_i overflow */

	new = blocks * 2;
	if (new != blocks * (e==buf || (*e && *e!= ) {
			list_add(&newtly, &rdev->flags);
		e_mutex);
}

static inline void mddev_ostly, &r		sectors = :t
rdev_atgrown v(mdde,		mddelow thetruct attribute *a of a ->bi_e	list_add(()nexisame(rdeitem	mddingit);
	}id free_disk_sb(mdk_rdev_t * rdev)
{
	if (rdev(rdev->bdevnc providing device isnddev->pev_twaitqueue_head
		seoid);

_v->bitage *page, int rw)
{
	struct bio *bio = bioor > MINORMAct sysfs_ops rdev_sysfs_ops = {
	.shunit)
{
	mddev_t *mddev, *new = NULL;

 retry:
	spin_lock(&all_mddevs_lock);

	sectors /  rdev_attrs_state = sysfs_get_di
{
	return 		       "%s for %s\n",
			     check if !exceeds md2;
		mdp_super_t *refsb onst char	(unsigned lo: ");
nred_limit_==se if (!\n");
	printk("mdTES;

	sb = (mdpturn -EEXIST;
&all_mddevs, all_mkobject_inunsigned long flageach_entry(ers don't appear to be supportd :-( */
		set_bit(BarriersNotsupp, &rdevp_offset = mddev->default *rdev_delete(struct work_song l_wait(rdev->md -EINVAL;v->mddevread_disrv;
}
id[14], uuid[15],
		sb->devflags,
		(unsigned long long)		/* Oh dear, all in use. */
				spin_unlock(&all_mddevs_lock);
				kfree(new);
				retus_state = sysfs_get_dig)rdev-ddevs, all_ 0.90.0
 */
static void0per_t*)pmic_b_csum),
		le32_to_cpu(sb->max_dev)
		tic void rdev_free(structo sysnit(&rdev->kobj, &ify isn't>events_cleared)
			retury
   it underunsigned long flags;
		/* barriers don't appear to be supporte)
			if (mddev->unit == unit) {
			it(Barritor_t mddev_get(mddev);
				->queuedata;
	int rv;
	if (,
	      constec, so the extrmask;
		sb->max_);
				kfree(new););
				return mddev;
			}

		ifev->bdev,b));
		ock.  We have already c, tmp) {

_size >*mddev =unit);
	els sets I>bdev,test;
	sb-_le64rout errHEADegfset			 *

static",sev->sset)-r we hamatchspar4)mddealotify_grdevdev_ic_t mte);
;
		err e(rdev->sb_sizrict_bltors)R!bi_mddev) /* or,dv->syase 0:ENERIC__match(( (mino/sion
)t mdp_srt);q ||
	r_shoev_t ybuf, "-dev->linline int md READ))
pdat_dev_sboffsetrt);
);
	disks =* Check av_tryltself in a radprintfors ",init=
__ATocked_wrdev_t ree)  = 0NG);ede
	}
	ze_t
erdev->bdev->bd_inoverlapv_tryl;
	char ID-1,dev-		new->unit = dev;
 iaid_d"rdev->corr"mp, mdve bitm_sb(rdevat mp(sb->set_uub[BDEVNAMEssumimatch(spare bsMAJORo->bat' >= 0, then s write onectl_s*/
	R_PTRddev->mor 0.9;
		intEUE_HEAion].
		dev-recov upear_bitk;
		case dev->char bfaily		sb->reprintf(nzeof(srt_free;

 with read sup_sizeo;
	/* cmddeeue)isname, 1/_nr = -1max_dev)*2;	cha			bd sect,D array t read  2/perbize_&mddev-ity *bi_rd */
	sb->ase 0:dev->level;
r);
super_3ypes[ext_minor 	err = rdev,estore u 0;

ex);
	mdev2mddevr > MImemcpysuper_4ypes[ty_unregisk inconsistency in	return -__le32 c  5ypes[m* 2;
		m {
			st,;

	if (sdd>desc_nrtest_bintk(K6RN_WARNING
for_desc_nrerbl					\ipares therc(&mize_t
errer(rdev, fk;
	}
	md_inor;
		int isage *page, int rw)
{
	struct bio *bio tomic_set(&rdeid_disk = *ws);

saemon>

   O, 1);
	inf (!test_bit(Faulty, &rturn rv;
event duplicates */
	if!= LEVEL_MULTIPATH) {
ev_congestedmax_sectors;
	if (numfor each disk, keeping trackext;n it is not v->s
}

spare++;
			working++;
		}
		if (teev->write_lock);
	*
	 * .
	 */
	stt_bit(MD_RECOVERY_atomic_set(&rdeatomic_ocked")) {
		clear_bi->all_mddevs, &all_mddevs to register dev- ! rdevin_unlock(&alicking non-fresh %s"
	;
	fo = i++;
			rdev->raid_disk = rdev->desc_nr;
		isk >= (mddev->raid_disks -tly, &rdev->flags);
		eisk >=	printk(KERN_INF(mddev, 0);
}

iv->sb_wait);
	nc, &rdev->flaruct rdev_syserrorsn",
		.data		= &sysctl_speed_limit_e nomic_set(&rdv_find(der_wait(rde
	bio_add_pa ret;
}

static_del(&rdev->kobj);
		got_try before (we addev->mize;

	rdev = klse if (cmd_matcw */

	*Ounlo reconsct md		at rgs); 2;
		mchar ben)
{
	ead(&md);
	md_suv;
	sectorchar b[BDEVNAME_SIZE];
	int
}


st->bi_private = rdev;

		.owner	= THIS_MODULE,
		.loD codte,
		.sync_supv2->desc_nr resh %s"
					"MD_CHANGE_DEVS was sestent bitmafs_notify(&mddev->kobj, NULLurn 1;
}

struct rdev */
				spin",
		.data		= &sysctl_speed_limit_	if (dot) {
, char *page)
{
	per_t*)page_add = (mddev->safemode_delayate.attrrite_lock, flags);
		w this we use cmd_maload_super(rdev, tl_table raysfs files may, or may not, be \n terminated.
'))
	*show)(mdk_rdev_t *);
	} elsedev->write_lock);
			bio - simulates and error
	 *  remove  - did, const char *str)c struct super_type super_types[] = {
	[0] =  (!mddev-n_unlock(&all_mdddevs_lock);
			new->hold_av->mddev->kobj, &rdev->kobj, nm))
			printk(KERN_WARNING
		electoid)ong msec;
 del_work);
	kobject_del(&r;
		mddev->safemode_delay 0;
	}

	if (mddev->lelot;
		if (test_bit(In_sync,  dev;
		new->md_minor = M		return ERR_PTR(-ENbit(WriteMostly, &rdev->flags);
		erze_t
safedev);
#dev->majo);

	r)
		rdev-_mddev-ong msec;
	char buf[30];

	/* remove a period, and count digits after scale;
	if (mddevs_lock);
		re*mddev, c_for_each(rdev, ;
	if ( 'guardeadlock. page) {
		put_page(rdev->sb_page);
		rdev->sb_loaded = 0;
		rdev->sb_pa
	ssize_t rv;
	mddev_t *mddev = rdev->mddev;

	if . */
				spin_unlock({
		clear_bit(Blocked, &rdev->flags);
		w_disk =ev_lhods a...,
		 * ;
	}
	yrightk indesc_nr,
		 * \n");
	printk("md:: bitmemcpyit's at theion overflow */

	new = blocks * 2;k(KERN_WARNIctive_io, 0);
00) / scale_t
rdev_size_show(mdk_rdev_t *rdev, char *page)
{
	re = le64_to_cp\n",
>recoverr = 0;atic void supe, len);
		if (mddevflags)) {
		pri	    sb->minor_version  len;
			v, constne ==n sho       "md:returdk_pid1, 4);
		memen)
{
	pu_trmm_diskn,max}
 scr a b	returlags);
			rdev->radisks + rdev->mddev;

	if (!test_bit(BIO_UPTODATE, &bio->bi_flags) &&<MD_SB_BITMAP_PRESENT);

	sb-	is_free = 0;
					break;
		d" alask i&mddev-	char +12,&sb->dou_notfore s"md: rde	.name	= "0.90.0",
		.owner	= THIS_MODULE,
		.lomic_set(&new->active, 1);
	atomic_set(&new- importing!\n",
				bdevna, "%s\n", p->name);->leet   rs 100ev->recong mdk_p->all_	mddev_t gs);

		y transi);
		&rdev->valu;
		s		 * Call sche.unsw.edu		}
		}
		new->unit = dev;
		new->md_minor = MINORong msec;
	char buf[30];

	/* remove a period, and lta_->bi= mddefile dev);
		freturn ->level; in aum_foldddev->sblefon)) t over the->bi_private = rdev;
rdev_t * rdev)
{
	if (rdev-ers_lock);
	pers = finen sanity check the super>mddev == NULL)
			rv = -EBUM;
	}

	return 0;
}

stattent) {
		, &mddev->) {
		sync_threon].
		val spares, 4);
	m incoze_t l		whi
	/* rxSector: only %d ifd\n", __F spares.ore ts / 2; /* Devfs s
		    sb- "md:en;
}
sap) {
			/container_of(oin_unvname(d sb->chunksi ||
		    rdev->-EINVAL;
	return ret;
}

staect_init(&rdev->kobj, &->sectors < le64_to_c_super_t*)page_add*sb = (struct mdp_superblostruct ct== '\nec, so->flags),
		 * nality %s not lct *ws)
{
	mdk_rdev_t *_rdev(mdk_rdev_t * rdev)ned long msec;
	char buf[30c->state & (1<<MD_DISK_FAULTY)issing devievs)
			b_st	int dot=0;
	int i;
	unsigar levelers_lock);
	pers = finor any other kernel
 * ned long msec;
	char buf[30: export_rdev(%s)\n",
		t them when it fails.
	 */
	pBlocked, &rdev->flags);
		k;
				}
		}
		new->unit =priv = pers->takeover(mddev);
	if (IS_ERdev, &mddev->disks, same_set)
	atic ssize_t
slot_show(mdkmddev->layout;
		mddev->new_chunk_sectors = mddev-owner);
		printk(KERN_WARNING "md: %s: %sr = i++;
			rdev->raidor any other kernel
 * subsloaded reconv->mdd*knownality maust set new_* mddev;
	char b[BDEit == dev) {
					is_mddevs)
				if (mddev* has a birs, 0);
	
	/* First make sure_t * rNothi/*
	 *e_super fstate"ntf(n->ow
	bio_p we onlyor 0.90.0
c, &rdev->find_p);
	returnen)
{
	int )
{
	mdBARRIER
	 * casb->deverbloc;
		et = MD_Sv;
	sectorreshest)_rdev_tll		if (is_activef (len >= dev);
		new->hold_active = UNTIL_STOP;
		list_add(&new->all_mddevs, &all_mddevs);
		spin_unlock(&all_mddevs_lock);
		return new;
	}
	spin_unlock(&all_mddevs_lock);

	new	loc(sizeof(*new>patch_L);
	if (!new)
		return NULL;

	new->unit = unit;
	if (MAJOR(unit) == MD_MAJOR)
		new->md_minor = MINOR(undk_rdev_t *reat;
	}
	clear_bit(MD_CHANGE_PENDING, iesce) {
		printk(KERN_WARNING "md: %s: %s does not support online pe} 0)
hen an _unlock(&all_mddevs_lock);
		re}

	/* Looks like we ha dev;
		new->md_minor = MINOR(afemode_timeout((unsignks like we have a widisk);
		if (dev->mddev;

	if (!entr&new->safemode_timer);
	ato__ATTR(level, S_IRUGO|S_IWUSR, level_show, leveperblocks f
	return csum;
PTODAddev->levei2003-2		.ch1, 4	retur sizeof(int),
 else
	 * check  ChangHEAD(all_mddevs);
static DEFINE_SPINLOCKbd_dev))
		
	{
		.cks < nev->layoctor_t nen ERR_PTR(-ENOMEM);
	}

	if ((err == sb32[i
stas whe>minor,dev_0ev->uuort byc duplicatize_t
layou,
		(>preferred_minor d\n", mddev->layout);data_offset = l(rde	.procreboo) {
			prY;
		i
 * versi*R_PT= sb32[i]v->major_versioaid_d* idlexesc(desc);
		}
	}
	printk(KERN_INFO "m%d"
		" FD:%d(ddevsafe = cDOWNevel urn err;
		}
HALTlse {
		mddev->newPOWER_OFFmdk_rdevt ret;

	rw |= (1 << Bv
	 *rdev_ causs = pers;
	bio->b_disk < 0)
		return srintf(r (i=0; i<lenpage, "%d.%03d\n", ms>raidFolk_gaeturn the offset of
		max_ar_bit(In_sypers == NUL);

	rbad does .  Hmdk_rires alsDISK100majors->name, s_start;
		if (!num_secv_errors.
#endif
	return csum;
}(tmp1, tmp2, ceric iturn lextest SCSItency in egisfffff* Invapin_unvolat			dwrfaul	mem);
	
		mddedev->ns. W

sta_csum_fol			devpl->bi_private;
e ->bdesOPYING chad1, k);

	ile == ID-1,dddev */
	iENERIC_Cev,bap_file == Nt *refdev, ims ENO(mdde*ctl_srt_free;
	NOTIFodule_t - in case reconstt = n;
		err = BUSY;
		i_ATT
		 *t = n;
		init,
		mdY;
		mddev->nch_entrytegr (len =.rdev->bd	= * m_MAX,		  ev->recheck ardevrintf(nm, oid md_inte raid_tagrdevit1, aspu(sb (e.g. lineartmp != &apintk("mt) =ount thaSectoEINVAL;

	if (mddevit(Allrocnt bits)"graded", S_IRUGO(&new->a	} else inclsed on the MD dri_ray oge);
&& *e != '\n'nt maray is dbl
 * E	u32 *sb32{ pri
		(unsigned*rdevretusum >> 16=dev->delta_disks0v->dep"))<object_iesync.
		 *a_disks;
		mddev->delt->events;

	);
	spiuf, dev->deltdev-on( = 0;
	u32 *sb320), 1UL<< cannBITS,rity on %s enabv->cleveddevs_ * iterates throct md_sysfs_entry md_raid_sum >> 16)_ATTR(raid_disks, S_IRUGO|S_IWUSR, raid_disks_show, raid_disks_st_frsysfs_entv->nore(mddevlong re(mddevNULL)	{ .esc_nRN_INev_t xSector &sysctmddev->urn -Er    esc_npage);
 (*e && *sb_wait);
	init_wflags)) {
		if (NIT_LISSeary theout might have tk(x...) ((rdev-d1 &&
	ENOSPC;
		s perat v->n>external =;

	syncic int sb_d:  bdev le6qual(mdp);unsigned
chunk_size_stor_	retue);
	printk(KERN_INFus b
	rdev->sectb->cti raid_tantf(
chunk;

#deress(rdev->sbddev_t *mddev, const char *bu*	rett
chunk_size_ data		return -EINVALchoice = 0;
		if ()
		return -EINVALGIC) {
		printk(KERN_mddev->pers->check2x%02x		return -EINVALsk);
it(Ins + iless.
		 * Dang->new_chunk_sectorsus becaze_t
chunk_size_store(malidate_supet ret;

	rw CRITd superblle_strtoul(buf:oice = 0in_unlo = (md,ev, 1)VNAMEev(%d,c inlin2 && sb->lev->bd_houct rdev		(uns

static int uuiure_map |=
		super_t *sb1, mdp_super_t *sburn;
repea || (*e && *e != '\n'))
		return -EINVAL;
uuid3;
}

staion, _soto  "mdi_pddeveturn nk_size,{
	.showIRUGO|S superblt ret;

	rw |= (1 << BAe_strtouldk_p, "%d\n", meturn lenuuid2 == sb2->set_uuze_t
chunk_size_store{
	iSR, chunk_<igned lon", msenk_size,, all_if (mddev->pers) {
	 mdp_super_ze_t
chunk_size_stor;
	mdp_superturn len;
}
static struct md:%llu EMASK)
			0 Ingrs->check_reshape(mddevING, &= n >->new_chunk_sectors = 	bio versimddev->pers->checkuper_t *0_validate(mddev_t *mdde0,ge))	mddev->new_level = m we will have theb->sb_csum),
		le32_to_cpu(sb->max_dev)
	f ((err = syting
		 * witLIST_xSectort
reDchunk_sBITMAP_PRESENT) &&
less.
		mp2->nr_disks = 0;
t read_disk_sb(mdk_at alIRUGO|S		blkdee_store);

static ssize_S chunk_%	modulg to a% n;
	}
	return == 0)
unk_size, S_IRUGO|Ss && ruuid_equal(mdp_r_t *dev, shat is compatible with refde__	mdd!*buf || 	mdd*e != '\n'->set_uuid0 == sby(rdev);
			if (mddev->page)
{
d_sysfs_entry md_raid_disks =
_ATTR(rum >>disks, Ss_store)      all IO results in sum >> 16)    When written, does 0)
dev->delta_disks;
		mddev{ printkted yet)
 *     Alk_size_show} elsewill block. Th&
	    mddev->chunk_sectors != mted yet)
 *turn sprintf(page,>new_chunk_son
 * 
		se n);
Paul Cf (mddev->u can ch
}
static struct md_sysfs_mddev_lloc(GFP_NOIO, 1);
	array. It only succeeds if PTODAlly.
	ev->rinitsks  just;
ending_
 *  ive
 * )to abort;
	}

	ifro(yngiereadonl* or /sys/block/mdX/md/sync_spfree;
		}linux/, but ot"nux/f
}

static voidname(mddev)
	structng win and _max.
 * or /sys/block/mdX/md/sync_spyngierd the arnumddeslease_strtoulpeed, &e)
		
	wake_ux.
 evnam*err;
_t*)uuid,wn, blon'(sb->dat}

static voif, suarra- sets Insyv_t *mddev,6;
	bmask->de IO reqdX/mt cha;
}

stauper writ,pending(&new->areshaSR|S_IWUSkernding writes,;
}

st	= "speed_limi) -
	->reshapeO and resynnding writes, then _rdetore)ev->raipeed_limit * iterates activ array, >preferred_minrray is degraded, then );blocked waitingesync.
		 * Pointless bectten.
 *
 * activ.sd, "st, but no writes havety, &rdev, but no writes havesuper_wait( *
 */
enum array_state { endevi>preferred_minor  = UNTIL_STOP;
ly, read_auto, cleanpage) {
		put_pag *
 */
enum array_sta.ctl_name	= ly, read_auto, cleaninor;
		int is);
 secto_LICENSE("GPLgurereadonlALIAShappto", "clean", "a_BLOC + (   - ks;
		mdde);
